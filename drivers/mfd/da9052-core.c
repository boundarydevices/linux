/*
 * da9052-core.c  --  Device access for Dialog DA9052
 *
 * Copyright(c) 2009 Dialog Semiconductor Ltd.
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 * Author: Dialog Semiconductor Ltd <dchen@diasemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/list.h>
#include <linux/mfd/core.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/semaphore.h>

#include <linux/mfd/da9052/da9052.h>
#include <linux/mfd/da9052/adc.h>
#include <linux/mfd/da9052/tsi.h>

#define SUCCESS		0
#define FAILURE		1

struct da9052_eh_nb eve_nb_array[EVE_CNT];
static struct da9052_ssc_ops ssc_ops;
struct mutex manconv_lock;
static struct semaphore eve_nb_array_lock;
static struct da9052 *da9052_data;

void da9052_lock(struct da9052 *da9052)
{
	mutex_lock(&da9052->ssc_lock);
}
EXPORT_SYMBOL(da9052_lock);

void da9052_unlock(struct da9052 *da9052)
{
	mutex_unlock(&da9052->ssc_lock);
}
EXPORT_SYMBOL(da9052_unlock);

int da9052_ssc_write(struct da9052 *da9052, struct da9052_ssc_msg *sscmsg)
{
	int ret = 0;

	/* Reg address should be a valid address on PAGE0 or PAGE1 */
	if ((sscmsg->addr < DA9052_PAGE0_REG_START) ||
	(sscmsg->addr > DA9052_PAGE1_REG_END) ||
	((sscmsg->addr > DA9052_PAGE0_REG_END) &&
	(sscmsg->addr < DA9052_PAGE1_REG_START)))
		return INVALID_REGISTER;

	ret = ssc_ops.write(da9052, sscmsg);

	/* Update local cache if required */
	if (!ret) {
		/* Check if this register is Non-volatile*/
		if (da9052->ssc_cache[sscmsg->addr].type != VOLATILE) {
			/* Update value */
			da9052->ssc_cache[sscmsg->addr].val = sscmsg->data;
			/* Make this cache entry valid */
			da9052->ssc_cache[sscmsg->addr].status = VALID;
		}
	}

	return ret;
}

int da9052_ssc_read(struct da9052 *da9052, struct da9052_ssc_msg *sscmsg)
{
	int ret = 0;

	/* Reg addr should be a valid address on PAGE0 or PAGE1 */
	if ((sscmsg->addr < DA9052_PAGE0_REG_START) ||
	(sscmsg->addr > DA9052_PAGE1_REG_END) ||
	((sscmsg->addr > DA9052_PAGE0_REG_END) &&
	(sscmsg->addr < DA9052_PAGE1_REG_START)))
		return INVALID_REGISTER;

	/*
	 * Check if this is a Non-volatile register, if yes then return value -
	 * from cache instead of actual reading from hardware. Before reading -
	 * cache entry, make sure that the entry is valid
	 */
	/* The read request is for Non-volatile register */
	/* Check if we have valid cached value for this */
	if (da9052->ssc_cache[sscmsg->addr].status == VALID) {
		/* We have valid cached value, copy this value */
		sscmsg->data = da9052->ssc_cache[sscmsg->addr].val;
		return 0;
	}

	ret = ssc_ops.read(da9052, sscmsg);
	/* Update local cache if required */
	if (!ret) {
		/* Check if this register is Non-volatile*/
		if (da9052->ssc_cache[sscmsg->addr].type != VOLATILE) {
			/* Update value */
			da9052->ssc_cache[sscmsg->addr].val = sscmsg->data;
			/* Make this cache entry valid */
			da9052->ssc_cache[sscmsg->addr].status = VALID;
		}
	}

	return ret;
}

int da9052_ssc_write_many(struct da9052 *da9052, struct da9052_ssc_msg *sscmsg,
				int msg_no)
{
	int ret = 0;
	int cnt = 0;

	/* Check request size */
	if (msg_no > MAX_READ_WRITE_CNT)
		return -EIO;

	ret = ssc_ops.write_many(da9052, sscmsg, msg_no);
	/* Update local cache, if required */
	for (cnt = 0; cnt < msg_no; cnt++) {
		/* Check if this register is Non-volatile*/
		if (da9052->ssc_cache[sscmsg[cnt].addr].type != VOLATILE) {
			/* Update value */
			da9052->ssc_cache[sscmsg[cnt].addr].val =
			sscmsg[cnt].data;
			/* Make this cache entry valid */
			da9052->ssc_cache[sscmsg[cnt].addr].status = VALID;
		}
	}
	return ret;
}

int da9052_ssc_read_many(struct da9052 *da9052, struct da9052_ssc_msg *sscmsg,
			int msg_no)
{
	int ret = 0;
	int cnt = 0;

	/* Check request size */
	if (msg_no > MAX_READ_WRITE_CNT)
		return -EIO;

	ret = ssc_ops.read_many(da9052, sscmsg, msg_no);
	/* Update local cache, if required */
	for (cnt = 0; cnt < msg_no; cnt++) {
		/* Check if this register is Non-volatile*/
		if (da9052->ssc_cache[sscmsg[cnt].addr].type
			!= VOLATILE) {
			/* Update value */
			da9052->ssc_cache[sscmsg[cnt].addr].val =
			sscmsg[cnt].data;
			/* Make this cache entry valid */
			da9052->ssc_cache[sscmsg[cnt].addr].status = VALID;
		}
	}
	return ret;
}

int da9052_ssc_modify_many(struct da9052 *da9052,
	struct da9052_ssc_msg *sscmsg,
	struct da9052_modify_msg *modmsg, int cnt)
{
	int i;
	int ret;
	da9052_lock(da9052);
	ret = da9052->read_many(da9052, sscmsg, cnt);
	if (ret) {
		DA9052_DEBUG("%s: read_many failed\n", __FUNCTION__);
		goto exit1;
	}
	for (i = 0; i < cnt; i++) {
		sscmsg[i].data &= ~modmsg[i].clear_mask;
		sscmsg[i].data |= modmsg[i].set_mask;
	}
	ret = da9052->write_many(da9052, sscmsg, cnt);
	if (ret)
		DA9052_DEBUG("%s: write_many failed\n", __FUNCTION__);
exit1:
	da9052_unlock(da9052);
	return ret;
}

static irqreturn_t da9052_eh_isr(int irq, void *dev_id)
{
	struct da9052 *da9052 = dev_id;
	/* Schedule work to be done */
	schedule_work(&da9052->eh_isr_work);
	/* Disable IRQ */
	disable_irq_nosync(da9052->irq);
	return IRQ_HANDLED;
}

int eh_register_nb(struct da9052 *da9052, struct da9052_eh_nb *nb)
{

	if (nb == NULL) {
		printk(KERN_INFO "EH REGISTER FUNCTION FAILED\n");
		return -EINVAL;
	}

	if (nb->eve_type >= EVE_CNT) {
		printk(KERN_INFO "Invalid DA9052 Event Type\n");
		return -EINVAL;
	}

	/* Initialize list head inside notifier block */
	INIT_LIST_HEAD(&nb->nb_list);

	/* Acquire NB array lock */
	if (down_interruptible(&eve_nb_array_lock))
		return -EAGAIN;

	/* Add passed NB to corresponding EVENT list */
	list_add_tail(&nb->nb_list, &(eve_nb_array[nb->eve_type].nb_list));

	/* Release NB array lock */
	up(&eve_nb_array_lock);

	return 0;
}

int eh_unregister_nb(struct da9052 *da9052, struct da9052_eh_nb *nb)
{

	if (nb == NULL)
		return -EINVAL;

	/* Acquire nb array lock */
	if (down_interruptible(&eve_nb_array_lock))
		return -EAGAIN;

	/* Remove passed NB from list */
	list_del_init(&(nb->nb_list));

	/* Release NB array lock */
	up(&eve_nb_array_lock);

	return 0;
}

int reg_modify(struct da9052 *da9052, unsigned reg,
		unsigned clear_mask, unsigned set_mask)
{
	int ret;
	struct da9052_ssc_msg sscmsg;
	sscmsg.addr = reg;
	sscmsg.data = 0;
	da9052_lock(da9052);
	ret = da9052_ssc_read(da9052, &sscmsg);
	if (ret >= 0) {
		sscmsg.data &= ~clear_mask;
		sscmsg.data |= set_mask;
		ret = da9052_ssc_write(da9052, &sscmsg);
	}
	da9052_unlock(da9052);
	return ret;
}

int event_modify(struct da9052 *da9052, unsigned char eve_type,
		unsigned enable)
{
	int ret;
	struct da9052_ssc_msg sscmsg;
	if (eve_type >= 32)
		return -EINVAL;
	sscmsg.addr = DA9052_IRQMASKA_REG + (eve_type >> 3);
	da9052_lock(da9052);
	if (enable)
		__clear_bit(eve_type, &da9052->irq_mask);
	else
		__set_bit(eve_type, &da9052->irq_mask);
	sscmsg.data = da9052->irq_mask >> (eve_type & 0x18);

	ret = da9052_ssc_write(da9052, &sscmsg);
	da9052_unlock(da9052);
	return ret;
}

int eh_enable(struct da9052 *da9052, unsigned char eve_type)
{
	pr_debug("da9052 enable 0x%x\n", eve_type);
	return event_modify(da9052, eve_type, 1);
}

int eh_disable(struct da9052 *da9052, unsigned char eve_type)
{
	pr_debug("da9052 disable 0x%x\n", eve_type);
	return event_modify(da9052, eve_type, 0);
}

static int process_events(struct da9052 *da9052, int events_sts)
{

	int cnt = 0;
	int tmp_events_sts = 0;
	unsigned char event = 0;

	struct list_head *ptr;
	struct da9052_eh_nb *nb_ptr;

	/* Now we have retrieved all events, process them one by one */
	//for (cnt = 0; cnt < PRIO_CNT; cnt++) {
	for (cnt = 0; cnt < EVE_CNT; cnt++) {
		/*
		 * Starting with highest priority event,
		 * traverse through all event
		 */
		tmp_events_sts = events_sts;

		/* Find the event associated with higher priority */

		/* KPIT NOTE: This is commented as we are not using prioritization */
		//	event = eve_prio_map[cnt];
		event = cnt;

		/* Check if interrupt is received for this event */
		/* KPIT : event is commneted to not use prioritization */
		//if (!((tmp_events_sts >> event) & 0x1)) {
		if (!((tmp_events_sts >> cnt) & 0x1))
			/* Event bit is not set for this event */
			/* Move to next priority event */
			continue;

		if (list_empty(&(eve_nb_array[event].nb_list)))
			continue;

		/* Event bit is set, execute all registered call backs */
		if (down_interruptible(&eve_nb_array_lock)){
			printk(KERN_CRIT "Can't acquire eve_nb_array_lock \n");
			return -EIO;
		}

		list_for_each(ptr, &(eve_nb_array[event].nb_list)) {
			/*
			 * nb_ptr will point to the structure in which
			 * nb_list is embedded
			 */
			nb_ptr = list_entry(ptr, struct da9052_eh_nb, nb_list);
			nb_ptr->call_back(nb_ptr, events_sts);
		}
		up(&eve_nb_array_lock);
	}
	return 0;
}

void eh_workqueue_isr(struct work_struct *work)
{
	struct da9052 *da9052 =
		container_of(work, struct da9052, eh_isr_work);

	struct da9052_ssc_msg eve_data[4];
	int events_sts, ret;
	unsigned i, j;

	/* nIRQ is asserted, read event registeres to know what happened */
	events_sts = 0;

	/* Prepare ssc message to read all four event registers */
	for (i = 0; i < 4; i++) {
		eve_data[i].addr = (DA9052_EVENTA_REG + i);
		eve_data[i].data = 0;
	}

	/* Now read all event registers */
	da9052_lock(da9052);

	ret = da9052_ssc_read_many(da9052, eve_data, 4);
	if (ret) {
		enable_irq(da9052->irq);
		da9052_unlock(da9052);
		return;
	}

	/* Collect all events */
	for (i = 0; i < 4; i++)
		events_sts |= (eve_data[i].data << (8 * i));

	pr_debug("events_sts=%08x irq_mask=%08lx\n", events_sts, da9052->irq_mask);
	events_sts &= ~da9052->irq_mask;

	/* Now clear EVENT registers */
	j = 0;
	for (i = 0; i < 4; i++) {
		if (eve_data[i].data) {
			eve_data[j].addr = (DA9052_EVENTA_REG + i);
			eve_data[j++].data = eve_data[i].data;
		}
	}
	if (events_sts & (1 << TSI_READY_EVE)) {
		da9052_unlock(da9052);
		process_events(da9052, (1 << TSI_READY_EVE));
		events_sts &= ~(1 << TSI_READY_EVE);
		/*
		 * Clearing events will allow regs 107,108,109,110 to be
		 * updated again. Therefore, we most process this event
		 * before clearing.
		 */
		da9052_lock(da9052);
	}
	if (j) {
		da9052_ssc_write_many(da9052, eve_data, j);
		da9052->spurious_irq_count = 0;
	} else {
		struct da9052_ssc_msg boost;
		boost.addr = DA9052_BOOST_REG;
		boost.data = 0;
		da9052_ssc_read(da9052, &boost);
		if (boost.data & 0x80) {
			/* clear boost over voltage/current condition */
			da9052_ssc_write(da9052, &boost);
			pr_err("%s: boost over voltage/current\n", __func__);
		} else {
			pr_err("%s: spurious irq(%d)\n", __func__, da9052->irq);
		}
		da9052->spurious_irq_count++;
	}

	da9052_unlock(da9052);

	pr_debug("da9052 events_sts=%x\n", events_sts);
	/* Process all events occurred */
	if (events_sts)
		process_events(da9052, events_sts);
	/* Enable HOST interrupt */
	if (da9052->spurious_irq_count < 5) {
		enable_irq(da9052->irq);
	} else {
		pr_err("%s: leaving irq(%d) disabled\n", __func__, da9052->irq);
	}
}

static void da9052_eh_restore_irq(struct da9052 *da9052)
{
	/* Put your platform and board specific code here */
	free_irq(da9052->irq, NULL);
}

static int da9052_add_subdevice_pdata(struct da9052 *da9052,
		const char *name, void *pdata, size_t pdata_size)
{
	struct mfd_cell cell = {
		.name = name,
		.platform_data = pdata,
		.data_size = pdata_size,
	};
	return mfd_add_devices(da9052->dev, -1, &cell, 1, NULL, 0);
}

static int da9052_add_subdevice(struct da9052 *da9052, const char *name)
{
	return da9052_add_subdevice_pdata(da9052, name, NULL, 0);
}

static struct da9052 *__da9052_global = 0 ;

struct da9052 *get_da9052(void) {
	return __da9052_global ;
}

EXPORT_SYMBOL(get_da9052);

static int add_da9052_devices(struct da9052 *da9052)
{
	s32 ret = 0;
	struct da9052_platform_data *pdata = da9052->dev->platform_data;
	struct da9052_leds_platform_data leds_data = {
			.num_leds = pdata->led_data->num_leds,
			.led = pdata->led_data->led,
	};
	struct da9052_regulator_platform_data regulator_pdata = {
		.regulators = pdata->regulators,
	};

	struct da9052_tsi_platform_data tsi_data = *(pdata->tsi_data);
	struct da9052_bat_platform_data bat_data = *(pdata->bat_data);

	if (pdata && pdata->init) {
		ret = pdata->init(da9052);
		if (ret != 0)
			return ret;
	} else
		pr_err("No platform initialisation supplied\n");
	ret = da9052_add_subdevice(da9052, "da9052-rtc");
	if (ret)
		return ret;
	ret = da9052_add_subdevice(da9052, "da9052-onkey");
	if (ret)
		return ret;

	ret = da9052_add_subdevice(da9052, "WLED-1");
	if (ret)
		return ret;

	ret = da9052_add_subdevice(da9052, "WLED-2");
	if (ret)
		return ret;

	ret = da9052_add_subdevice(da9052, "WLED-3");
	if (ret)
		return ret;

	ret = da9052_add_subdevice(da9052, "da9052-adc");
	if (ret)
		return ret;

	ret = da9052_add_subdevice(da9052, "da9052-wdt");
	if (ret)
		return ret;

	ret = da9052_add_subdevice_pdata(da9052, "da9052-leds",
				&leds_data, sizeof(leds_data));
	if (ret)
		return ret;

	ret = da9052_add_subdevice_pdata(da9052, "da9052-regulator",
		&regulator_pdata, sizeof(regulator_pdata));
	if (ret)
		return ret;

	ret = da9052_add_subdevice_pdata(da9052, DA9052_TSI_DEVICE_NAME,
				&tsi_data, sizeof(tsi_data));
	if (ret)
		return ret;

	ret = da9052_add_subdevice_pdata(da9052, "da9052-bat",
				&bat_data, sizeof(bat_data));
	if (ret)
		return ret;

	ret = da9052_add_subdevice_pdata(da9052, "da9052-chg",
				&bat_data, sizeof(bat_data));
	if (ret)
		return ret;

        __da9052_global = da9052 ;

	return ret;
}

static ssize_t da9052_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct da9052 *da9052 = dev_get_drvdata(dev);
	struct da9052_ssc_msg msg[16];
	int i, j;
	int reg = 1;
	int ret;
	int end;
	int inc = 8;
	char *buf_start = buf;
	char *buf_end = buf + 4096 - (16*3 + 6);
	do {
		end = reg + inc;
		if (end > DA9052_CHIPID_REG)
			end = DA9052_CHIPID_REG;
		for (i = 0; reg <= end; i++, reg++) {
			msg[i].addr = reg;
		}
		da9052_lock(da9052);
		ret = da9052_ssc_read_many(da9052, msg, i);
		da9052_unlock(da9052);
		if (ret < 0) {
			dev_err(dev, "block read failed\n");
			break;
		}
		buf += sprintf(buf, "%3d: ", reg - i);
		if ((reg - i) == 1)
			buf += sprintf(buf, "   ");
		for (j = 0; j < i; j++) {
			buf += sprintf(buf, " %02x", msg[j].data);
		}
		buf += sprintf(buf, "\n");
		if (buf > buf_end)
			break;
		inc = 9;
	} while (reg <= DA9052_CHIPID_REG);
	return buf - buf_start;
}

static DEVICE_ATTR(da9052_reg, 0444, da9052_reg_show, NULL);


int da9052_ssc_init(struct da9052 *da9052)
{
	int i;
	struct da9052_platform_data *pdata;
	struct da9052_ssc_msg ssc_msg[4];

	/* Initialize eve_nb_array */
	for (i = 0; i < EVE_CNT; i++)
		INIT_LIST_HEAD(&(eve_nb_array[i].nb_list));

	/* Initialize mutex required for ADC Manual read */
	mutex_init(&manconv_lock);

	/* Initialize NB array lock */
	init_MUTEX(&eve_nb_array_lock);

	/* Assign the read-write function pointers */
	da9052->read = da9052_ssc_read;
	da9052->write = da9052_ssc_write;
	da9052->read_many = da9052_ssc_read_many;
	da9052->write_many = da9052_ssc_write_many;
	da9052->modify_many = da9052_ssc_modify_many;
	ssc_ops.write = NULL;
#ifdef CONFIG_PMIC_DA905X_SPI
	if (SPI  == da9052->connecting_device && ssc_ops.write == NULL) {
		/* Assign the read/write pointers to SPI/read/write */
		ssc_ops.write = da9052_spi_write;
		ssc_ops.read = da9052_spi_read;
		ssc_ops.write_many = da9052_spi_write_many;
		ssc_ops.read_many = da9052_spi_read_many;
	}
#endif
#ifdef CONFIG_PMIC_DA905X_I2C
	if (I2C  == da9052->connecting_device && ssc_ops.write == NULL) {
		/* Assign the read/write pointers to SPI/read/write */
		ssc_ops.write = da9052_i2c_write;
		ssc_ops.read = da9052_i2c_read;
		ssc_ops.write_many = da9052_i2c_write_many;
		ssc_ops.read_many = da9052_i2c_read_many;
	}
#endif
	if (!ssc_ops.write)
		return -1;

	/* Assign the EH notifier block register/de-register functions */
	da9052->register_event_notifier = eh_register_nb;
	da9052->unregister_event_notifier = eh_unregister_nb;
	da9052->event_enable = eh_enable;
	da9052->event_disable = eh_disable;
	da9052->register_modify = reg_modify;

	/* Initialize ssc lock */
	mutex_init(&da9052->ssc_lock);

	pdata = da9052->dev->platform_data;
	add_da9052_devices(da9052);

	INIT_WORK(&da9052->eh_isr_work, eh_workqueue_isr);

	da9052->irq_mask = 0xffffffff;
	for (i = 0; i < 4; i++) {
		ssc_msg[i].addr = DA9052_IRQMASKA_REG + i;
		ssc_msg[i].data = 0xff;
	}
	da9052->write_many(da9052, ssc_msg, 4);

	/* read chip version */
	ssc_msg[0].addr = DA9052_CHIPID_REG;
	da9052->read(da9052, &ssc_msg[0]);
	pr_info("DA9053 chip ID reg read=0x%x ", ssc_msg[0].data);
	if ((ssc_msg[0].data & DA9052_CHIPID_MRC) == 0x80) {
		da9052->chip_version = DA9053_VERSION_AA;
		pr_info("AA version probed\n");
	} else if ((ssc_msg[0].data & DA9052_CHIPID_MRC) == 0xa0) {
		da9052->chip_version = DA9053_VERSION_BB;
		pr_info("BB version probed\n");
	} else {
		da9052->chip_version = 0;
		pr_info("unknown chip version\n");
	}

	if (request_irq(da9052->irq, da9052_eh_isr, IRQ_TYPE_LEVEL_LOW,
		DA9052_EH_DEVICE_NAME, da9052))
		return -EIO;
	enable_irq_wake(da9052->irq);
	da9052_data = da9052;

	if (device_create_file(da9052->dev, &dev_attr_da9052_reg) < 0)
		printk(KERN_WARNING "failed to add da9052 sysfs file\n");

	return 0;
}

void da9052_ssc_exit(struct da9052 *da9052)
{
	printk(KERN_INFO "DA9052: Unregistering SSC device.\n");
	mutex_destroy(&manconv_lock);
	/* Restore IRQ line */
	da9052_eh_restore_irq(da9052);
	free_irq(da9052->irq, NULL);
	mutex_destroy(&da9052->ssc_lock);
	mutex_destroy(&da9052->eve_nb_lock);
	return;
}

void da9053_power_off(void)
{
	struct da9052_ssc_msg ssc_msg;
	struct da9052_ssc_msg ssc_msg_dummy[2];
	if (!da9052_data)
		return;

	ssc_msg.addr = DA9052_CONTROLB_REG;
	da9052_data->read(da9052_data, &ssc_msg);
	ssc_msg_dummy[0].addr = DA9052_CONTROLB_REG;
	ssc_msg_dummy[0].data = ssc_msg.data | DA9052_CONTROLB_SHUTDOWN;
	ssc_msg_dummy[1].addr = DA9052_GPID9_REG;
	ssc_msg_dummy[1].data = 0;
	da9052_data->write_many(da9052_data, &ssc_msg_dummy[0], 2);
}

int da9053_get_chip_version(void)
{
	return da9052_data->chip_version;
}

MODULE_AUTHOR("Dialog Semiconductor Ltd <dchen@diasemi.com>");
MODULE_DESCRIPTION("DA9052 MFD Core");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DA9052_SSC_DEVICE_NAME);
