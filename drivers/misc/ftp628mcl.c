/*
 * Copyright (C) 2017 Boundary Devices, Inc.
 *
 * Driver for the Fujitsu FTP-628 MCL printer
 *
 * Inspired from TI PRU sample code (TIDEP0056)
 *
 * Licensed under the GPL-2 or later.
 */
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>

#define DEV_NAME "ftp628"

#define DOTS_PER_LINE	384
#define BYTES_PER_LINE	(DOTS_PER_LINE / 8)

#define MOTOR_A1	(1 << 0)
#define MOTOR_An1	(3 << 0)
#define MOTOR_B1	(1 << 2)
#define MOTOR_Bn1	(3 << 2)

#define MOTOR_DELAY_DEFAULT	800
#define STROBE_DELAY_DEFAULT	400

const uint32_t mt_phases[] = {
	MOTOR_Bn1,
	MOTOR_Bn1 | MOTOR_An1,
	MOTOR_An1,
	MOTOR_An1 | MOTOR_B1,
	MOTOR_B1,
	MOTOR_B1 | MOTOR_A1,
	MOTOR_A1,
	MOTOR_A1 | MOTOR_Bn1
};

static struct ftp628_data {
	struct spi_device	*spi;
	struct gpio_desc	*latch_gpio;
	struct gpio_desc	*mt_ab_gpios[4];
	struct gpio_desc	*mt_ate_gpio;
	struct gpio_desc	*mt_dcay_gpios[2];
	struct gpio_desc	*mt_fault_gpio;
	struct gpio_desc	*mt_sleep_gpio;
	struct gpio_desc	*mt_toff_gpio;
	struct gpio_desc	*mt_trq_gpios[2];
	struct gpio_desc	*paper_out_gpio;
	struct gpio_desc	*strobe_gpios[6];
	int mt_phase;
	int motor_delay_us;
	int strobe_delay_us;
} ftp628_data;

static void ftp628_suspend_motors(struct ftp628_data *pdata, int suspend)
{
	gpiod_direction_output(pdata->mt_sleep_gpio, !!suspend);
	mdelay(1);
}

static void ftp628_motor_next_phase(struct ftp628_data *pdata)
{
	struct device *dev = &pdata->spi->dev;
	int i;

	dev_dbg(dev, "%s (%d/%d): %x\n", __func__, pdata->mt_phase + 1,
		ARRAY_SIZE(mt_phases), mt_phases[pdata->mt_phase]);

	udelay(pdata->motor_delay_us);

	for (i = 0; i < ARRAY_SIZE(pdata->mt_ab_gpios); i++) {
		if (mt_phases[pdata->mt_phase] & (1 << i))
			gpiod_set_value(pdata->mt_ab_gpios[i], 1);
		else
			gpiod_set_value(pdata->mt_ab_gpios[i], 0);
	}

	pdata->mt_phase = (pdata->mt_phase + 1) %
		ARRAY_SIZE(mt_phases);
}

static void ftp628_toggle_gpio(struct gpio_desc *gpio, int state, int delay_us)
{
	udelay(1);
	gpiod_set_value(gpio, state);
	udelay(delay_us);
	gpiod_set_value(gpio, !state);
	udelay(1);
}

static ssize_t ftp628_write(struct file *file, const char __user *data,
			    size_t count, loff_t *off)
{
	uint8_t buffer[BYTES_PER_LINE];
	struct ftp628_data *pdata = &ftp628_data;
	struct device *dev = &pdata->spi->dev;
	int ret;
	int i;
	loff_t file_off = 0;

	size_t size = simple_write_to_buffer(&buffer, sizeof(buffer),
					     &file_off, data, count);

	dev_dbg(dev, "%s count %d (size %d)\n", __func__, count, size);

	/* Only accept one line of data or advance paper only */
	if ((count != BYTES_PER_LINE) && (count != 1))
		return -EINVAL;

	/* Handle carriage return command */
	if (size == 1) {
		if (buffer[0] == '\r') {
			if (gpiod_get_value(pdata->mt_fault_gpio))
				return -EFAULT;
			if (gpiod_get_value(pdata->paper_out_gpio))
				return -ENOSPC;
			ftp628_motor_next_phase(pdata);
		} else if (buffer[0] == 'B') {
			ftp628_suspend_motors(pdata, 0);
		} else {
			ftp628_suspend_motors(pdata, 1); /* 'E' */
		}
		return size;
	}

	/* Send dots to shift registers */
	ret = spi_write(pdata->spi, buffer, sizeof(buffer));
	if (ret < 0)
		return -EIO;

	/* Toggle the latch signal (hold 1us) */
	ftp628_toggle_gpio(pdata->latch_gpio, 0, 1);

	/* Toggle all the strobe signals */
	for (i = 0; i < ARRAY_SIZE(pdata->strobe_gpios); i++)
		ftp628_toggle_gpio(pdata->strobe_gpios[i], 1,
				   pdata->strobe_delay_us);

	return size;
}

static const struct file_operations ftp628_fops = {
	.owner = THIS_MODULE,
	.write = ftp628_write,
};

static struct miscdevice ftp628_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEV_NAME,
	.fops = &ftp628_fops,
};

static ssize_t ftp628_motor_delay_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct ftp628_data *pdata = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", pdata->motor_delay_us);
}

static ssize_t ftp628_motor_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct ftp628_data *pdata = dev_get_drvdata(dev);

	sscanf(buf, "%d", &pdata->motor_delay_us);

	return count;
}

static ssize_t ftp628_strobe_delay_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct ftp628_data *pdata = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", pdata->strobe_delay_us);
}

static ssize_t ftp628_strobe_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct ftp628_data *pdata = dev_get_drvdata(dev);

	sscanf(buf, "%d", &pdata->strobe_delay_us);

	return count;
}

static DEVICE_ATTR(motor_delay_us, S_IRUGO | S_IWUSR, ftp628_motor_delay_show,
		   ftp628_motor_delay_store);
static DEVICE_ATTR(strobe_delay_us, S_IRUGO | S_IWUSR, ftp628_strobe_delay_show,
		   ftp628_strobe_delay_store);

static struct attribute *ftp628_attrs[] = {
	&dev_attr_motor_delay_us.attr,
	&dev_attr_strobe_delay_us.attr,
	NULL
};

static const struct attribute_group ftp628_attr_group = {
	.attrs = ftp628_attrs,
};

#ifdef CONFIG_OF
static const struct of_device_id ftp628_dt_ids[] = {
	{ .compatible = "fujitsu,ftp628" },
	{ }
};
MODULE_DEVICE_TABLE(of, ftp628_dt_ids);

static int ftp628_probe_dt(struct device *dev, struct ftp628_data *pdata)
{
	struct device_node *node = dev->of_node;
	const struct of_device_id *match;
	int i;

	if (!node) {
		dev_err(dev, "Device does not have associated DT data\n");
		return -EINVAL;
	}

	match = of_match_device(ftp628_dt_ids, dev);
	if (!match) {
		dev_err(dev, "Unknown device model\n");
		return -EINVAL;
	}

	pdata->latch_gpio = devm_gpiod_get(dev, "latch", GPIOD_OUT_HIGH);
	if (IS_ERR(pdata->latch_gpio)) {
		dev_err(dev, "Can't get latch gpio\n");
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(pdata->mt_ab_gpios); i++) {
		pdata->mt_ab_gpios[i] = devm_gpiod_get_index(dev, "mt-ab", i,
							     GPIOD_OUT_LOW);
		if (IS_ERR(pdata->mt_ab_gpios[i])) {
			dev_err(dev, "Can't get motor ab gpio (%d)\n", i);
			return -ENODEV;
		}
	}

	pdata->mt_ate_gpio = devm_gpiod_get(dev, "mt-ate", GPIOD_OUT_HIGH);
	if (IS_ERR(pdata->mt_ate_gpio)) {
		dev_err(dev, "Can't get motor autotune gpio\n");
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(pdata->mt_dcay_gpios); i++) {
		pdata->mt_dcay_gpios[i] = devm_gpiod_get_index(dev,
							       "mt-dcay", i,
							       GPIOD_OUT_LOW);
		if (IS_ERR(pdata->mt_dcay_gpios[i])) {
			dev_err(dev, "Can't get dcay gpio (%d)\n", i);
			return -ENODEV;
		}
	}

	pdata->mt_fault_gpio = devm_gpiod_get(dev, "mt-fault", GPIOD_IN);
	if (IS_ERR(pdata->mt_fault_gpio)) {
		dev_err(dev, "Can't get motor fault gpio\n");
		return -ENODEV;
	}

	pdata->mt_sleep_gpio = devm_gpiod_get(dev, "mt-sleep", GPIOD_OUT_HIGH);
	if (IS_ERR(pdata->mt_sleep_gpio)) {
		dev_err(dev, "Can't get motor sleep gpio\n");
		return -ENODEV;
	}

	pdata->mt_toff_gpio = devm_gpiod_get(dev, "mt-toff", GPIOD_OUT_LOW);
	if (IS_ERR(pdata->mt_toff_gpio)) {
		dev_err(dev, "Can't get motor toff gpio\n");
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(pdata->mt_trq_gpios); i++) {
		pdata->mt_trq_gpios[i] = devm_gpiod_get_index(dev,
							      "mt-trq", i,
							      GPIOD_OUT_LOW);
		if (IS_ERR(pdata->mt_trq_gpios[i])) {
			dev_err(dev, "Can't get motor trq  gpio (%d)\n", i);
			return -ENODEV;
		}
	}

	pdata->paper_out_gpio = devm_gpiod_get(dev, "paper-out", GPIOD_IN);
	if (IS_ERR(pdata->paper_out_gpio)) {
		dev_err(dev, "Can't get paper out gpio\n");
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(pdata->strobe_gpios); i++) {
		pdata->strobe_gpios[i] = devm_gpiod_get_index(dev, "strobe", i,
							      GPIOD_OUT_LOW);
		if (IS_ERR(pdata->strobe_gpios[i])) {
			dev_err(dev, "Can't get strobe gpio (%d)\n", i);
			return -ENODEV;
		}
	}

	return 0;
}
#else
#define ftp628_dt_ids NULL
static int ftp628_probe_dt(struct device *dev, struct ftp628_data *pdata)
{
	dev_err(dev, "no platform data defined\n");
	return -EINVAL;
}
#endif

static int ftp628_probe(struct spi_device *spi)
{
	struct ftp628_data *pdata = &ftp628_data;
	int ret;

	ret = ftp628_probe_dt(&spi->dev, pdata);
	if (ret != 0)
		return ret;

	/* Initialize all the values */
	pdata->motor_delay_us = MOTOR_DELAY_DEFAULT;
	pdata->strobe_delay_us = STROBE_DELAY_DEFAULT;
	pdata->mt_phase = 0;
	pdata->spi = spi;
	spi_set_drvdata(spi, pdata);

	/* Add misc char device for user-space access */
	ret = misc_register(&ftp628_device);
	if (ret) {
		dev_err(&spi->dev, "Failed to misc_register: %d\n", ret);
		return ret;
	}

	ret = sysfs_create_group(&spi->dev.kobj, &ftp628_attr_group);
	if (ret)
		dev_warn(&spi->dev, "Failed to create sysfs group: %d\n", ret);

	dev_info(&spi->dev, "probed!\n");

	return 0;
}

static int ftp628_remove(struct spi_device *spi)
{
	misc_deregister(&ftp628_device);
	sysfs_remove_group(&spi->dev.kobj, &ftp628_attr_group);

	dev_info(&spi->dev, "removed!\n");

	return 0;
}

static struct spi_driver ftp628_driver = {
	.driver = {
		.name	= DEV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(ftp628_dt_ids),
	},
	.probe		= ftp628_probe,
	.remove		= ftp628_remove,
};

module_spi_driver(ftp628_driver);

MODULE_AUTHOR("Boundary Devices <info@boundarydevices.com>");
MODULE_DESCRIPTION("Driver for Fujitsu FTP628 thermal printer");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:ftp628");
