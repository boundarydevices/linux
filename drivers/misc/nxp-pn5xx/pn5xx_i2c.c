/*
 * Copyright (C) 2010 Trusted Logic S.A.
 * modifications copyright (C) 2015 NXP B.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include "pn5xx_i2c.h"
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>

#define MAX_BUFFER_SIZE	512

#define MODE_OFF    0
#define MODE_RUN    1
#define MODE_FW     2

/* Only pn548, pn547 and pn544 are supported */
#define CHIP "pn544"
#define DRIVER_CARD "PN54x NFC"
#define DRIVER_DESC "NFC driver for PN54x Family"

#ifndef CONFIG_OF
#define CONFIG_OF
#endif

struct pn54x_dev	{
	wait_queue_head_t read_wq;
	struct mutex read_mutex;
	struct i2c_client *client;
	struct miscdevice pn54x_device;
	int ven_gpio;
	int firm_gpio;
	int irq_gpio;
	int clkreq_gpio;
	struct regulator *pvdd_reg;
	struct regulator *vbat_reg;
	struct regulator *pmuvcc_reg;
	struct regulator *sevdd_reg;
	bool irq_enabled;
	spinlock_t irq_enabled_lock;
};

/**********************************************************
 * Interrupt control and handler
 **********************************************************/
static void pn54x_disable_irq(struct pn54x_dev *pn54x_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn54x_dev->irq_enabled_lock, flags);
	if (pn54x_dev->irq_enabled) {
		disable_irq_nosync(pn54x_dev->client->irq);
		pn54x_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&pn54x_dev->irq_enabled_lock, flags);
}

static irqreturn_t pn54x_dev_irq_handler(int irq, void *dev_id)
{
	struct pn54x_dev *pn54x_dev = dev_id;

	pn54x_disable_irq(pn54x_dev);

	/* Wake up waiting readers */
	wake_up(&pn54x_dev->read_wq);

	return IRQ_HANDLED;
}

/**********************************************************
 * private functions
 **********************************************************/
static int pn544_enable(struct pn54x_dev *dev, int mode)
{
	int r;

	/* turn on the regulators */
	/* -- if the regulators were specified, they're required */
	if(dev->pvdd_reg != NULL)
	{
		r = regulator_enable(dev->pvdd_reg);
		if (r < 0){
			pr_err("%s: not able to enable pvdd\n", __func__);
			return r;
		}
	}
	if(dev->vbat_reg != NULL)
	{
		r = regulator_enable(dev->vbat_reg);
		if (r < 0){
			pr_err("%s: not able to enable vbat\n", __func__);
			goto enable_exit0;
		}
	}
	if(dev->pmuvcc_reg != NULL)
	{
		r = regulator_enable(dev->pmuvcc_reg);
		if (r < 0){
			pr_err("%s: not able to enable pmuvcc\n", __func__);
			goto enable_exit1;
		}
	}
	if(dev->sevdd_reg != NULL)
	{
		r = regulator_enable(dev->sevdd_reg);
		if (r < 0){
			pr_err("%s: not able to enable sevdd\n", __func__);
			goto enable_exit2;
		}
	}

	if (MODE_RUN == mode) {
		pr_info("%s power on\n", __func__);
		if (gpio_is_valid(dev->firm_gpio))
			gpio_set_value_cansleep(dev->firm_gpio, 0);
		gpio_set_value_cansleep(dev->ven_gpio, 1);
		msleep(100);
	}
	else if (MODE_FW == mode) {
		/* power on with firmware download (requires hw reset)
		 */
		pr_info("%s power on with firmware\n", __func__);
		gpio_set_value(dev->ven_gpio, 1);
		msleep(20);
		if (gpio_is_valid(dev->firm_gpio)) {
			gpio_set_value(dev->firm_gpio, 1);
		}
		else {
			pr_err("%s Unused Firm GPIO %d\n", __func__, mode);
			return GPIO_UNUSED;
		}
		msleep(20);
		gpio_set_value(dev->ven_gpio, 0);
		msleep(100);
		gpio_set_value(dev->ven_gpio, 1);
		msleep(20);
	}
	else {
		pr_err("%s bad arg %d\n", __func__, mode);
		return -EINVAL;
	}

	return 0;

enable_exit2:
	if(dev->pmuvcc_reg) regulator_disable(dev->pmuvcc_reg);
enable_exit1:
	if(dev->vbat_reg) regulator_disable(dev->vbat_reg);
enable_exit0:
	if(dev->pvdd_reg) regulator_disable(dev->pvdd_reg);

	return r;
}

static void pn544_disable(struct pn54x_dev *dev)
{
	/* power off */
	pr_info("%s power off\n", __func__);
	if (gpio_is_valid(dev->firm_gpio))
		gpio_set_value_cansleep(dev->firm_gpio, 0);
	gpio_set_value_cansleep(dev->ven_gpio, 0);
	msleep(100);

	if(dev->sevdd_reg) regulator_disable(dev->sevdd_reg);
	if(dev->pmuvcc_reg) regulator_disable(dev->pmuvcc_reg);
	if(dev->vbat_reg) regulator_disable(dev->vbat_reg);
	if(dev->pvdd_reg) regulator_disable(dev->pvdd_reg);

}

/**********************************************************
 * driver functions
 **********************************************************/
static ssize_t pn54x_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn54x_dev *pn54x_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	pr_debug("%s : reading %zu bytes.\n", __func__, count);

	mutex_lock(&pn54x_dev->read_mutex);

	if (!gpio_get_value(pn54x_dev->irq_gpio)) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto fail;
		}

		while (1) {
			pn54x_dev->irq_enabled = true;
			enable_irq(pn54x_dev->client->irq);
			ret = wait_event_interruptible(
					pn54x_dev->read_wq,
					!pn54x_dev->irq_enabled);

			pn54x_disable_irq(pn54x_dev);

			if (ret)
				goto fail;

			if (gpio_get_value(pn54x_dev->irq_gpio))
				break;

			pr_warn("%s: spurious interrupt detected\n", __func__);
		}
	}

	/* Read data */
	ret = i2c_master_recv(pn54x_dev->client, tmp, count);

	mutex_unlock(&pn54x_dev->read_mutex);

	/* pn54x seems to be slow in handling I2C read requests
	 * so add 1ms delay after recv operation */
	udelay(1000);

	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		return -EIO;
	}
	if (copy_to_user(buf, tmp, ret)) {
		pr_warn("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}
	return ret;

fail:
	mutex_unlock(&pn54x_dev->read_mutex);
	return ret;
}

static ssize_t pn54x_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn54x_dev  *pn54x_dev;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	pn54x_dev = filp->private_data;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	pr_debug("%s : writing %zu bytes.\n", __func__, count);
	/* Write data */
	ret = i2c_master_send(pn54x_dev->client, tmp, count);
	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}

	/* pn54x seems to be slow in handling I2C write requests
	 * so add 1ms delay after I2C send oparation */
	udelay(1000);

	return ret;
}

static int pn54x_dev_open(struct inode *inode, struct file *filp)
{
	struct pn54x_dev *pn54x_dev = container_of(filp->private_data,
											   struct pn54x_dev,
											   pn54x_device);

	filp->private_data = pn54x_dev;

	pr_info("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

	// pn544_enable(pn54x_dev, MODE_RUN);

	return 0;
}

static int pn54x_dev_release(struct inode *inode, struct file *filp)
{
	// struct pn54x_dev *pn54x_dev = container_of(filp->private_data,
	//										   struct pn54x_dev,
	//										   pn54x_device);

	pr_info("%s : closing %d,%d\n", __func__, imajor(inode), iminor(inode));

	// pn544_disable(pn54x_dev);

	return 0;
}

static long  pn54x_dev_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct pn54x_dev *pn54x_dev = filp->private_data;

	pr_info("%s, cmd=%d, arg=%lu\n", __func__, cmd, arg);
	switch (cmd) {
	case PN544_SET_PWR:
		if (arg == 2) {
			/* power on w/FW */
			if (GPIO_UNUSED == pn544_enable(pn54x_dev, arg)) {
				return GPIO_UNUSED;
			}
		} else if (arg == 1) {
			/* power on */
			pn544_enable(pn54x_dev, arg);
		} else  if (arg == 0) {
			/* power off */
			pn544_disable(pn54x_dev);
		} else {
			pr_err("%s bad SET_PWR arg %lu\n", __func__, arg);
			return -EINVAL;
		}
		break;
	case PN54X_CLK_REQ:
		if(1 == arg){
			if(gpio_is_valid(pn54x_dev->clkreq_gpio)){
				gpio_set_value(pn54x_dev->clkreq_gpio, 1);
			}
			else {
				pr_err("%s Unused Clkreq GPIO %lu\n", __func__, arg);
				return GPIO_UNUSED;
			}
		}
		else if(0 == arg) {
			if(gpio_is_valid(pn54x_dev->clkreq_gpio)){
				gpio_set_value(pn54x_dev->clkreq_gpio, 0);
			}
			else {
				pr_err("%s Unused Clkreq GPIO %lu\n", __func__, arg);
				return GPIO_UNUSED;
			}
		} else {
			pr_err("%s bad CLK_REQ arg %lu\n", __func__, arg);
			return -EINVAL;
		}
		break;
	default:
		pr_err("%s bad ioctl %u\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations pn54x_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn54x_dev_read,
	.write	= pn54x_dev_write,
	.open	= pn54x_dev_open,
	.release  = pn54x_dev_release,
	.unlocked_ioctl  = pn54x_dev_ioctl,
};


/*
 * Handlers for alternative sources of platform_data
 */
#ifdef CONFIG_OF
/*
 * Translate OpenFirmware node properties into platform_data
 */
static int pn54x_get_pdata(struct device *dev,
							struct pn544_i2c_platform_data *pdata)
{
	struct device_node *node;
	u32 flags;
	int val;

	/* make sure there is actually a device tree node */
	node = dev->of_node;
	if (!node)
		return -ENODEV;

	memset(pdata, 0, sizeof(*pdata));

	/* read the dev tree data */

	/* ven pin - enable's power to the chip - REQUIRED */
	val = of_get_named_gpio_flags(node, "enable-gpios", 0, &flags);
	if (val >= 0) {
		pdata->ven_gpio = val;
	}
	else {
		dev_err(dev, "VEN GPIO error getting from OF node\n");
		return val;
	}

	/* firm pin - controls firmware download - OPTIONAL */
	val = of_get_named_gpio_flags(node, "firmware-gpios", 0, &flags);
	if (val >= 0) {
		pdata->firm_gpio = val;
	}
	else {
		pdata->firm_gpio = GPIO_UNUSED;
		dev_warn(dev, "FIRM GPIO <OPTIONAL> error getting from OF node\n");
	}

	/* irq pin - data available irq - REQUIRED */
	val = of_get_named_gpio_flags(node, "interrupt-gpios", 0, &flags);
	if (val >= 0) {
		pdata->irq_gpio = val;
	}
	else {
		dev_err(dev, "IRQ GPIO error getting from OF node\n");
		return val;
	}

	/* clkreq pin - controls the clock to the PN547 - OPTIONAL */
	val = of_get_named_gpio_flags(node, "nxp,pn54x-clkreq", 0, &flags);
	if (val >= 0) {
		pdata->clkreq_gpio = val;
	}
	else {
		pdata->clkreq_gpio = GPIO_UNUSED;
		dev_warn(dev, "CLKREQ GPIO <OPTIONAL> error getting from OF node\n");
	}

	/* handle the regulator lines - these are optional
	 * PVdd - pad Vdd (544, 547)
	 * Vbat - Battery (544, 547)
	 * PMUVcc - UICC Power (544, 547)
	 * SEVdd - SE Power (544)
	 *
	 * Will attempt to load a matching Regulator Resource for each
	 * If no resource is provided, then the input will not be controlled
	 * Example: if only PVdd is provided, it is the only one that will be
	 *  turned on/off.
	 */
	pdata->pvdd_reg = regulator_get(dev, "nxp,pn54x-pvdd");
	if(IS_ERR(pdata->pvdd_reg)) {
		pr_err("%s: could not get nxp,pn54x-pvdd, rc=%ld\n", __func__, PTR_ERR(pdata->pvdd_reg));
		pdata->pvdd_reg = NULL;
	}

	pdata->vbat_reg = regulator_get(dev, "nxp,pn54x-vbat");
	if (IS_ERR(pdata->vbat_reg)) {
		pr_err("%s: could not get nxp,pn54x-vbat, rc=%ld\n", __func__, PTR_ERR(pdata->vbat_reg));
		pdata->vbat_reg = NULL;
	}

	pdata->pmuvcc_reg = regulator_get(dev, "nxp,pn54x-pmuvcc");
	if (IS_ERR(pdata->pmuvcc_reg)) {
		pr_err("%s: could not get nxp,pn54x-pmuvcc, rc=%ld\n", __func__, PTR_ERR(pdata->pmuvcc_reg));
		pdata->pmuvcc_reg = NULL;
	}

	pdata->sevdd_reg = regulator_get(dev, "nxp,pn54x-sevdd");
	if (IS_ERR(pdata->sevdd_reg)) {
		pr_err("%s: could not get nxp,pn54x-sevdd, rc=%ld\n", __func__, PTR_ERR(pdata->sevdd_reg));
		pdata->sevdd_reg = NULL;
	}

	return 0;
}
#else
static int pn54x_get_pdata(struct device *dev,
							struct pn544_i2c_platform_data *pdata)
{
	pdata = dev->platform_data;
	return 0;
}
#endif


/*
 * pn54x_probe
 */
#ifdef KERNEL_3_4_AND_OLDER
 static int __devinit pn54x_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
#else
static int pn54x_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
#endif
{
	int ret;
	struct pn544_i2c_platform_data *pdata; // gpio values, from board file or DT
	struct pn544_i2c_platform_data tmp_pdata;
	struct pn54x_dev *pn54x_dev; // internal device specific data

	pr_info("%s\n", __func__);

	/* ---- retrieve the platform data ---- */
	/* If the dev.platform_data is NULL, then */
	/* attempt to read from the device tree */
	if(!client->dev.platform_data)
	{
		ret = pn54x_get_pdata(&(client->dev), &tmp_pdata);
		if(ret){
			return ret;
		}

		pdata = &tmp_pdata;
	}
	else
	{
		pdata = client->dev.platform_data;
	}

	if (pdata == NULL) {
		pr_err("%s : nfc probe fail\n", __func__);
		return  -ENODEV;
	}

	/* validate the the adapter has basic I2C functionality */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}

	/* reserve the GPIO pins */
	pr_info("%s: request irq_gpio %d\n", __func__, pdata->irq_gpio);
	ret = gpio_request(pdata->irq_gpio, "nfc_int");
	if (ret){
		pr_err("%s :not able to get GPIO irq_gpio\n", __func__);
		return  -ENODEV;
	}
	ret = gpio_to_irq(pdata->irq_gpio);
	if (ret < 0){
		pr_err("%s :not able to map GPIO irq_gpio to an IRQ\n", __func__);
		goto err_ven;
	}
	else{
		client->irq = ret;
	}

	pr_info("%s: request ven_gpio %d\n", __func__, pdata->ven_gpio);
	ret = gpio_request(pdata->ven_gpio, "nfc_ven");
	if (ret){
		pr_err("%s :not able to get GPIO ven_gpio\n", __func__);
		goto err_ven;
	}

	if (gpio_is_valid(pdata->firm_gpio)) {
		pr_info("%s: request firm_gpio %d\n", __func__, pdata->firm_gpio);
		ret = gpio_request(pdata->firm_gpio, "nfc_firm");
		if (ret){
			pr_err("%s :not able to get GPIO firm_gpio\n", __func__);
			goto err_firm;
		}
	}

	if (gpio_is_valid(pdata->clkreq_gpio)) {
		pr_info("%s: request clkreq_gpio %d\n", __func__, pdata->clkreq_gpio);
		ret = gpio_request(pdata->clkreq_gpio, "nfc_clkreq");
		if (ret){
			pr_err("%s :not able to get GPIO clkreq_gpio\n", __func__);
			goto err_clkreq;
		}
	}

	/* allocate the pn54x driver information structure */
	pn54x_dev = kzalloc(sizeof(*pn54x_dev), GFP_KERNEL);
	if (pn54x_dev == NULL) {
		dev_err(&client->dev, "failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	/* store the platform data in the driver info struct */
	pn54x_dev->irq_gpio = pdata->irq_gpio;
	pn54x_dev->ven_gpio = pdata->ven_gpio;
	pn54x_dev->firm_gpio = pdata->firm_gpio;
	pn54x_dev->clkreq_gpio = pdata->clkreq_gpio;
	pn54x_dev->pvdd_reg = pdata->pvdd_reg;
	pn54x_dev->vbat_reg = pdata->vbat_reg;
	pn54x_dev->pmuvcc_reg = pdata->pmuvcc_reg;
	pn54x_dev->sevdd_reg = pdata->sevdd_reg;

	pn54x_dev->client = client;

	/* finish configuring the I/O */
	ret = gpio_direction_input(pn54x_dev->irq_gpio);
	if (ret < 0) {
		pr_err("%s :not able to set irq_gpio as input\n", __func__);
		goto err_exit;
	}

	ret = gpio_direction_output(pn54x_dev->ven_gpio, 0);
	if (ret < 0) {
		pr_err("%s : not able to set ven_gpio as output\n", __func__);
		goto err_exit;
	}

	if (gpio_is_valid(pn54x_dev->firm_gpio)) {
		ret = gpio_direction_output(pn54x_dev->firm_gpio, 0);
		if (ret < 0) {
			pr_err("%s : not able to set firm_gpio as output\n",
				 __func__);
			goto err_exit;
		}
	}

	if (gpio_is_valid(pn54x_dev->clkreq_gpio)) {
		ret = gpio_direction_output(pn54x_dev->clkreq_gpio, 0);
		if (ret < 0) {
			pr_err("%s : not able to set clkreq_gpio as output\n",
				   __func__);
			goto err_exit;
		}
	}

	/* init mutex and queues */
	init_waitqueue_head(&pn54x_dev->read_wq);
	mutex_init(&pn54x_dev->read_mutex);
	spin_lock_init(&pn54x_dev->irq_enabled_lock);

	/* register as a misc device - character based with one entry point */
	pn54x_dev->pn54x_device.minor = MISC_DYNAMIC_MINOR;
	pn54x_dev->pn54x_device.name = CHIP;
	pn54x_dev->pn54x_device.fops = &pn54x_dev_fops;
	ret = misc_register(&pn54x_dev->pn54x_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	pr_info("%s : requesting IRQ %d\n", __func__, client->irq);
	pn54x_dev->irq_enabled = true;
	ret = request_irq(client->irq, pn54x_dev_irq_handler,
				IRQF_TRIGGER_HIGH, client->name, pn54x_dev);
	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}
	pn54x_disable_irq(pn54x_dev);

	i2c_set_clientdata(client, pn54x_dev);

	return 0;

err_request_irq_failed:
	misc_deregister(&pn54x_dev->pn54x_device);
err_misc_register:
err_exit:
	if (gpio_is_valid(pdata->clkreq_gpio))
		gpio_free(pdata->clkreq_gpio);
err_clkreq:
	if (gpio_is_valid(pdata->firm_gpio))
		gpio_free(pdata->firm_gpio);
err_firm:
	gpio_free(pdata->ven_gpio);
err_ven:
	gpio_free(pdata->irq_gpio);
	return ret;
}

#ifdef KERNEL_3_4_AND_OLDER
static int __devexit pn54x_remove(struct i2c_client *client)
#else
static void pn54x_remove(struct i2c_client *client)
#endif
{
	struct pn54x_dev *pn54x_dev;

	pr_info("%s\n", __func__);

	pn54x_dev = i2c_get_clientdata(client);
	free_irq(client->irq, pn54x_dev);
	misc_deregister(&pn54x_dev->pn54x_device);
	mutex_destroy(&pn54x_dev->read_mutex);
	gpio_free(pn54x_dev->irq_gpio);
	gpio_free(pn54x_dev->ven_gpio);
	if (gpio_is_valid(pn54x_dev->firm_gpio))
		gpio_free(pn54x_dev->firm_gpio);
	if (gpio_is_valid(pn54x_dev->clkreq_gpio))
		gpio_free(pn54x_dev->clkreq_gpio);
	regulator_put(pn54x_dev->pvdd_reg);
	regulator_put(pn54x_dev->vbat_reg);
	regulator_put(pn54x_dev->pmuvcc_reg);
	regulator_put(pn54x_dev->sevdd_reg);

	kfree(pn54x_dev);
}

/*
 *
 */
#ifdef CONFIG_OF
static struct of_device_id pn54x_dt_match[] = {
	{ .compatible = "nxp,pn547", },
	{ .compatible = "nxp,pn544", },
	{},
};
MODULE_DEVICE_TABLE(of, pn54x_dt_match);
#endif

static const struct i2c_device_id pn54x_id[] = {
	{ "pn547", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, pn54x_id);

static struct i2c_driver pn54x_driver = {
	.id_table	= pn54x_id,
	.probe		= pn54x_probe,
#ifdef KERNEL_3_4_AND_OLDER
	.remove		= __devexit_p(pn54x_remove),
#else
	.remove		= pn54x_remove,
#endif
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "pn544",
		.of_match_table = pn54x_dt_match,
	},
};

/*
 * module load/unload record keeping
 */

static int __init pn54x_dev_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&pn54x_driver);
}

static void __exit pn54x_dev_exit(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&pn54x_driver);
}

module_init(pn54x_dev_init);
module_exit(pn54x_dev_exit);

MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
