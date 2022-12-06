// SPDX-License-Identifier: GPL-2.0-only
/*
 * An I2C and SPI driver for the NXP PCF2131 RTC
 *
 * Copyright 2013 Til-Technologies
 * Author: Renaud Cerrato <r.cerrato@til-technologies.fr>
 * Copyright 2022 NXP
 *
 * based on the other drivers in this same directory.
 *
 */

#include <linux/bcd.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/watchdog.h>

/* Control register 1 */
#define PCF2131_REG_CTRL1              0x00
#define PCF2131_BIT_CTRL1_SI           BIT(0)
#define PCF2131_BIT_CTRL1_MI           BIT(1)
#define PCF2131_BIT_CTRL1_12_24        BIT(2)
#define PCF2131_BIT_CTRL1_POR_OVRD     BIT(3)
#define PCF2131_BIT_CTRL1_100TH_S_DIS  BIT(4)
#define PCF2131_BIT_CTRL1_STOP         BIT(5)
#define PCF2131_BIT_CTRL1_TC_DIS       BIT(6)

/* Control register 2 */
#define PCF2131_REG_CTRL2              0x01
#define PCF2131_BIT_CTRL2_AIE          BIT(1)
#define PCF2131_BIT_CTRL2_AF           BIT(4)
#define PCF2131_BIT_CTRL2_WDTF         BIT(6)

/* Control register 3 */
#define PCF2131_REG_CTRL3              0x02
#define PCF2131_BIT_CTRL3_BLIE         BIT(0)
#define PCF2131_BIT_CTRL3_BIE          BIT(1)
#define PCF2131_BIT_CTRL3_BLF          BIT(2)
#define PCF2131_BIT_CTRL3_BF           BIT(3)
#define PCF2131_BIT_CTRL3_BTSE         BIT(4)

/* Control register 4 */
#define PCF2131_REG_CTRL4              0x03
#define PCF2131_BIT_CTRL4_TSF1         BIT(7)
#define PCF2131_BIT_CTRL4_TSF2         BIT(6)
#define PCF2131_BIT_CTRL4_TSF3         BIT(5)
#define PCF2131_BIT_CTRL4_TSF4         BIT(4)

/* Control register 5 */
#define PCF2131_REG_CTRL5              0x04
#define PCF2131_BIT_CTRL5_TSIE1        BIT(7)
#define PCF2131_BIT_CTRL5_TSIE2        BIT(6)
#define PCF2131_BIT_CTRL5_TSIE3        BIT(5)
#define PCF2131_BIT_CTRL5_TSIE4        BIT(4)

/* Software reset register */
#define PCF2131_REG_SR_RESET           0x05
#define PCF2131_SR_VAL_SoftReset       0x2c
#define PCF2131_SR_VAL_Clr_Pres        0xa4
#define PCF2131_SR_VAL_Clr_TS          0x25

/* Time and date registers */
#define PCF2131_REG_100th_SC           0x06
#define PCF2131_REG_SC                 0x07
#define PCF2131_BIT_SC_OSF             BIT(7)
#define PCF2131_REG_MN                 0x08
#define PCF2131_REG_HR                 0x09
#define PCF2131_REG_DM                 0x0a
#define PCF2131_REG_DW                 0x0b
#define PCF2131_REG_MO                 0x0c
#define PCF2131_REG_YR                 0x0d

/* Alarm registers */
#define PCF2131_REG_ALARM_SC           0x0e
#define PCF2131_REG_ALARM_MN           0x0f
#define PCF2131_REG_ALARM_HR           0x10
#define PCF2131_REG_ALARM_DM           0x11
#define PCF2131_REG_ALARM_DW           0x12
#define PCF2131_BIT_ALARM_AE           BIT(7)

#define PCF2131_REG_CLKOUT_CTL         0x13

/* Watchdog registers */
#define PCF2131_REG_WD_CTL             0x35
#define PCF2131_BIT_WD_CTL_TF0         BIT(0)
#define PCF2131_BIT_WD_CTL_TF1         BIT(1)
#define PCF2131_BIT_WD_CTL_TI_TP       BIT(5)
#define PCF2131_BIT_WD_CTL_CD          BIT(7)
#define PCF2131_REG_WD_VAL             0x36

/* Timestamp 1 registers */
#define PCF2131_REG_TS1_CTRL           0x14
#define PCF2131_BIT_TS1_CTRL_TSOFF     BIT(6)
#define PCF2131_BIT_TS1_CTRL_TSM       BIT(7)
#define PCF2131_REG_TS1_SC             0x15
#define PCF2131_REG_TS1_MN             0x16
#define PCF2131_REG_TS1_HR             0x17
#define PCF2131_REG_TS1_DM             0x18
#define PCF2131_REG_TS1_MO             0x19
#define PCF2131_REG_TS1_YR             0x1a

/* Timestamp 2 registers */
#define PCF2131_REG_TS2_CTRL           0x1b
#define PCF2131_BIT_TS2_CTRL_TSOFF     BIT(6)
#define PCF2131_BIT_TS2_CTRL_TSM       BIT(7)
#define PCF2131_REG_TS2_SC             0x1c
#define PCF2131_REG_TS2_MN             0x1d
#define PCF2131_REG_TS2_HR             0x1e
#define PCF2131_REG_TS2_DM             0x1f
#define PCF2131_REG_TS2_MO             0x20
#define PCF2131_REG_TS2_YR             0x21

/* Timestamp 3 registers */
#define PCF2131_REG_TS3_CTRL           0x22
#define PCF2131_BIT_TS3_CTRL_TSOFF     BIT(6)
#define PCF2131_BIT_TS3_CTRL_TSM       BIT(7)
#define PCF2131_REG_TS3_SC             0x23
#define PCF2131_REG_TS3_MN             0x24
#define PCF2131_REG_TS3_HR             0x25
#define PCF2131_REG_TS3_DM             0x26
#define PCF2131_REG_TS3_MO             0x27
#define PCF2131_REG_TS3_YR             0x28

/* Timestamp 4 registers */
#define PCF2131_REG_TS4_CTRL           0x29
#define PCF2131_BIT_TS4_CTRL_TSOFF     BIT(6)
#define PCF2131_BIT_TS4_CTRL_TSM       BIT(7)
#define PCF2131_REG_TS4_SC             0x2a
#define PCF2131_REG_TS4_MN             0x2b
#define PCF2131_REG_TS4_HR             0x2c
#define PCF2131_REG_TS4_DM             0x2d
#define PCF2131_REG_TS4_MO             0x2e
#define PCF2131_REG_TS4_YR             0x2f

/* Interrupt mask*/
#define PCF2131_REG_INTA_MASK1         0x31
#define PCF2131_BIT_INTA_MASK1_AIEA    BIT(2)

/* Watchdog registers */
#define PCF2131_REG_WD_CTL             0x35
#define PCF2131_BIT_WD_CTL_TF0         BIT(0)
#define PCF2131_BIT_WD_CTL_TF1         BIT(1)
#define PCF2131_BIT_WD_CTL_TI_TP       BIT(5)
#define PCF2131_BIT_WD_CTL_CD          BIT(7)
#define PCF2131_REG_WD_VAL             0x36

/* Watchdog timer value constants */
#define PCF2131_WD_VAL_STOP            0
#define PCF2131_WD_VAL_MIN             2
#define PCF2131_WD_VAL_MAX             255
#define PCF2131_WD_VAL_DEFAULT         60

/* Mask for currently enabled interrupts */
#define PCF2131_CTRL2_IRQ_MASK (PCF2131_BIT_CTRL2_AF | \
		PCF2131_BIT_CTRL2_WDTF)

struct pcf2131 {
	struct rtc_device *rtc;
	struct watchdog_device wdd;
	struct regmap *regmap;
};

/*
 * In the routines that deal directly with the pcf2131 hardware, we use
 * rtc_time -- month 0-11, hour 0-23, yr = calendar year-epoch.
 */
static int pcf2131_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct pcf2131 *pcf2131 = dev_get_drvdata(dev);
	unsigned char buf[55];
	int ret;

	ret = regmap_bulk_read(pcf2131->regmap, PCF2131_REG_CTRL1,
			       (buf + PCF2131_REG_CTRL1),
			       ARRAY_SIZE(buf) - PCF2131_REG_CTRL1);
	if (ret) {
		dev_err(dev, "%s: read error\n", __func__);
		return ret;
	}

	if (buf[PCF2131_REG_CTRL3] & PCF2131_BIT_CTRL3_BLF)
		dev_info(dev,
			"low voltage detected, check/replace RTC battery.\n");

	/* Clock integrity is not guaranteed when OSF flag is set. */
	if (buf[PCF2131_REG_SC] & PCF2131_BIT_SC_OSF) {
		/*
		 * no need clear the flag here,
		 * it will be cleared once the new date is saved
		 */
		dev_warn(dev,
			 "oscillator stop detected, date/time is not reliable\n");
		return -EINVAL;
	}

	dev_dbg(dev,
		"%s: raw data is cr3=%02x, sec=%02x, min=%02x, hr=%02x, mday=%02x, wday=%02x, mon=%02x, year=%02x\n",
		__func__, buf[PCF2131_REG_CTRL3], buf[PCF2131_REG_SC],
		buf[PCF2131_REG_MN], buf[PCF2131_REG_HR],
		buf[PCF2131_REG_DM], buf[PCF2131_REG_DW],
		buf[PCF2131_REG_MO], buf[PCF2131_REG_YR]);

	tm->tm_sec = bcd2bin(buf[PCF2131_REG_SC] & 0x7F);
	tm->tm_min = bcd2bin(buf[PCF2131_REG_MN] & 0x7F);
	tm->tm_hour = bcd2bin(buf[PCF2131_REG_HR] & 0x3F); /* rtc hr 0-23 */
	tm->tm_mday = bcd2bin(buf[PCF2131_REG_DM] & 0x3F);
	tm->tm_wday = buf[PCF2131_REG_DW] & 0x07;
	tm->tm_mon = bcd2bin(buf[PCF2131_REG_MO] & 0x1F) - 1; /* rtc mn 1-12 */
	tm->tm_year = bcd2bin(buf[PCF2131_REG_YR]);
	tm->tm_year += 100;

	dev_dbg(dev, "%s: tm is secs=%d, mins=%d, hours=%d, mday=%d, mon=%d, year=%d, wday=%d\n",
		__func__,
		tm->tm_sec, tm->tm_min, tm->tm_hour,
		tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_wday);
	return 0;
}

static int pcf2131_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct pcf2131 *pcf2131 = dev_get_drvdata(dev);
	unsigned char buf[16];
	int err;
	int ret;

	/*
	 * Need the following information:
	 * Register 0x00 Bit 5 - STOP	Set to 1 to stop, set to 0 to start again
	 * Register 0x05       - write 0xa4 to clear prescaler
	 */

	ret = regmap_bulk_read(pcf2131->regmap, PCF2131_REG_CTRL1,
		(buf + PCF2131_REG_CTRL1),
		ARRAY_SIZE(buf) - PCF2131_REG_CTRL1);

	if (ret) {
		dev_err(dev, "%s: read error\n", __func__);
		return ret;
	}

	buf[PCF2131_REG_CTRL1]		|= PCF2131_BIT_CTRL1_STOP | PCF2131_BIT_CTRL1_100TH_S_DIS;
	buf[PCF2131_REG_CTRL1]		&= ~PCF2131_BIT_CTRL1_12_24;	/* 24 hour format */
	buf[PCF2131_REG_CTRL3]		&= 0x1f;	/* PWRMNG = 0 */
	buf[PCF2131_REG_SR_RESET]	= PCF2131_SR_VAL_Clr_Pres;

	dev_dbg(dev, "%s: secs=%d, mins=%d, hours=%d, mday=%d, mon=%d, year=%d, wday=%d\n",
		__func__,
		tm->tm_sec, tm->tm_min, tm->tm_hour,
		tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_wday);

	/* hours, minutes and seconds */
	buf[PCF2131_REG_100th_SC] = 0x00;	/* 100th Second is not used */
	buf[PCF2131_REG_SC] = bin2bcd(tm->tm_sec);	/* this will also clear OSF flag */
	buf[PCF2131_REG_MN] = bin2bcd(tm->tm_min);
	buf[PCF2131_REG_HR] = bin2bcd(tm->tm_hour);
	buf[PCF2131_REG_DM] = bin2bcd(tm->tm_mday);
	buf[PCF2131_REG_DW] = tm->tm_wday & 0x07;

	/* month, 1 - 12 */
	buf[PCF2131_REG_MO] = bin2bcd(tm->tm_mon + 1);

	/* year */
	buf[PCF2131_REG_YR] = bin2bcd(tm->tm_year - 100);

	/* write register's data */
	err = regmap_bulk_write(pcf2131->regmap, PCF2131_REG_CTRL1,
				buf + PCF2131_REG_CTRL1, 1);
	err += regmap_bulk_write(pcf2131->regmap, PCF2131_REG_CTRL3,
				 buf + PCF2131_REG_CTRL3, 1);
	err += regmap_bulk_write(pcf2131->regmap, PCF2131_REG_SR_RESET,
				 buf + PCF2131_REG_SR_RESET, 1);
	err += regmap_bulk_write(pcf2131->regmap, PCF2131_REG_100th_SC,
				 buf + PCF2131_REG_100th_SC, 8);
	err += regmap_update_bits(pcf2131->regmap, PCF2131_REG_CTRL1,
				  PCF2131_BIT_CTRL1_STOP, 0);

	if (err) {
		dev_err(dev,
			"%s: err=%d", __func__, err);
		return err;
	}

	return 0;
}

static int pcf2131_rtc_ioctl(struct device *dev,
				unsigned int cmd, unsigned long arg)
{
	struct pcf2131 *pcf2131 = dev_get_drvdata(dev);
	int val, touser = 0;
	int ret;

	switch (cmd) {
	case RTC_VL_READ:
		ret = regmap_read(pcf2131->regmap, PCF2131_REG_CTRL3, &val);
		if (ret)
			return ret;

		if (val & PCF2131_BIT_CTRL3_BLF)
			touser |= RTC_VL_BACKUP_LOW;

		if (val & PCF2131_BIT_CTRL3_BF)
			touser |= RTC_VL_BACKUP_SWITCH;

		return put_user(touser, (unsigned int __user *)arg);

	case RTC_VL_CLR:
		return regmap_update_bits(pcf2131->regmap, PCF2131_REG_CTRL3,
					  PCF2131_BIT_CTRL3_BF, 0);

	default:
		return -ENOIOCTLCMD;
	}
}

/* watchdog driver */

static int pcf2131_wdt_ping(struct watchdog_device *wdd)
{
	struct pcf2131 *pcf2131 = watchdog_get_drvdata(wdd);

	return regmap_write(pcf2131->regmap, PCF2131_REG_WD_VAL, wdd->timeout);
}

/* Restart watchdog timer if feature is active. */
static int pcf2131_wdt_active_ping(struct watchdog_device *wdd)
{
	int ret = 0;

	if (watchdog_active(wdd)) {
		ret = pcf2131_wdt_ping(wdd);
		if (ret)
			dev_err(wdd->parent,
				"%s: watchdog restart failed, ret=%d\n",
				__func__, ret);
	}

	return ret;
}

static int pcf2131_wdt_start(struct watchdog_device *wdd)
{
	return pcf2131_wdt_ping(wdd);
}

static int pcf2131_wdt_stop(struct watchdog_device *wdd)
{
	struct pcf2131 *pcf2131 = watchdog_get_drvdata(wdd);

	return regmap_write(pcf2131->regmap, PCF2131_REG_WD_VAL,
			    PCF2131_WD_VAL_STOP);
}

static int pcf2131_wdt_set_timeout(struct watchdog_device *wdd,
				   unsigned int new_timeout)
{
	dev_dbg(wdd->parent, "new watchdog timeout: %is (old: %is)\n",
		new_timeout, wdd->timeout);

	wdd->timeout = new_timeout;

	return pcf2131_wdt_active_ping(wdd);
}

static const struct watchdog_info pcf2131_wdt_info = {
	.identity = "NXP PCF2131 Watchdog",
	.options = WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT,
};

static const struct watchdog_ops pcf2131_watchdog_ops = {
	.owner = THIS_MODULE,
	.start = pcf2131_wdt_start,
	.stop = pcf2131_wdt_stop,
	.ping = pcf2131_wdt_ping,
	.set_timeout = pcf2131_wdt_set_timeout,
};

/* Alarm */
static int pcf2131_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct pcf2131 *pcf2131 = dev_get_drvdata(dev);
	u8 buf[5];
	unsigned int ctrl2;
	int ret;

	ret = regmap_read(pcf2131->regmap, PCF2131_REG_CTRL2, &ctrl2);
	if (ret)
		return ret;

	ret = pcf2131_wdt_active_ping(&pcf2131->wdd);
	if (ret)
		return ret;

	ret = regmap_bulk_read(pcf2131->regmap, PCF2131_REG_ALARM_SC, buf,
			       sizeof(buf));
	if (ret)
		return ret;

	alrm->enabled = ctrl2 & PCF2131_BIT_CTRL2_AIE;
	alrm->pending = ctrl2 & PCF2131_BIT_CTRL2_AF;

	alrm->time.tm_sec = bcd2bin(buf[0] & 0x7F);
	alrm->time.tm_min = bcd2bin(buf[1] & 0x7F);
	alrm->time.tm_hour = bcd2bin(buf[2] & 0x3F);
	alrm->time.tm_mday = bcd2bin(buf[3] & 0x3F);

	return 0;
}

static int pcf2131_rtc_alarm_irq_enable(struct device *dev, u32 enable)
{
	struct pcf2131 *pcf2131 = dev_get_drvdata(dev);
	int ret;

	ret = regmap_update_bits(pcf2131->regmap, PCF2131_REG_CTRL2,
				 PCF2131_BIT_CTRL2_AIE,
				 enable ? PCF2131_BIT_CTRL2_AIE : 0);
	if (ret)
		return ret;

	return pcf2131_wdt_active_ping(&pcf2131->wdd);
}

static int pcf2131_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct pcf2131 *pcf2131 = dev_get_drvdata(dev);
	uint8_t buf[5];
	int ret;

	ret = regmap_update_bits(pcf2131->regmap, PCF2131_REG_CTRL2,
				 PCF2131_BIT_CTRL2_AF, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(pcf2131->regmap, PCF2131_REG_INTA_MASK1,
				 PCF2131_BIT_INTA_MASK1_AIEA, 0);
	if (ret)
		return ret;

	ret = pcf2131_wdt_active_ping(&pcf2131->wdd);
	if (ret)
		return ret;

	buf[0] = bin2bcd(alrm->time.tm_sec);
	buf[1] = bin2bcd(alrm->time.tm_min);
	buf[2] = bin2bcd(alrm->time.tm_hour);
	buf[3] = bin2bcd(alrm->time.tm_mday);
	buf[4] = PCF2131_BIT_ALARM_AE; /* Do not match on week day */

	ret = regmap_bulk_write(pcf2131->regmap, PCF2131_REG_ALARM_SC, buf,
				sizeof(buf));
	if (ret)
		return ret;
	return pcf2131_rtc_alarm_irq_enable(dev, alrm->enabled);
}

static const struct rtc_class_ops pcf2131_rtc_ops = {
	.ioctl		  = pcf2131_rtc_ioctl,
	.read_time	  = pcf2131_rtc_read_time,
	.set_time	  = pcf2131_rtc_set_time,
	.read_alarm       = pcf2131_rtc_read_alarm,
	.set_alarm        = pcf2131_rtc_set_alarm,
	.alarm_irq_enable = pcf2131_rtc_alarm_irq_enable,
};

/* sysfs interface */

static ssize_t timestamp0_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct pcf2131 *pcf2131 = dev_get_drvdata(dev->parent);
	int ret;

	ret = regmap_update_bits(pcf2131->regmap, PCF2131_REG_CTRL4,
				 PCF2131_BIT_CTRL4_TSF1, 0);
	if (ret) {
		dev_err(dev, "%s: update ctrl1 ret=%d\n", __func__, ret);
		return ret;
	}

	return count;
};

static ssize_t timestamp0_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct pcf2131 *pcf2131 = dev_get_drvdata(dev->parent);
	struct rtc_time tm;
	int ret;
	unsigned char data[30];

	ret = regmap_bulk_read(pcf2131->regmap, PCF2131_REG_CTRL1, data,
			       sizeof(data));
	if (ret) {
		dev_err(dev, "%s: read error ret=%d\n", __func__, ret);
		return ret;
	}

	dev_dbg(dev,
		"%s: raw data is cr1=%02x, cr2=%02x, cr3=%02x, ts_sc=%02x, ts_mn=%02x, ts_hr=%02x, ts_dm=%02x, ts_mo=%02x, ts_yr=%02x\n",
		__func__, data[PCF2131_REG_CTRL1], data[PCF2131_REG_CTRL2],
		data[PCF2131_REG_CTRL3], data[PCF2131_REG_TS1_SC],
		data[PCF2131_REG_TS1_MN], data[PCF2131_REG_TS1_HR],
		data[PCF2131_REG_TS1_DM], data[PCF2131_REG_TS1_MO],
		data[PCF2131_REG_TS1_YR]);

	if (!(data[PCF2131_REG_CTRL4] & PCF2131_BIT_CTRL4_TSF1))
		return 0;

	tm.tm_sec = bcd2bin(data[PCF2131_REG_TS1_SC] & 0x7F);
	tm.tm_min = bcd2bin(data[PCF2131_REG_TS1_MN] & 0x7F);
	tm.tm_hour = bcd2bin(data[PCF2131_REG_TS1_HR] & 0x3F);
	tm.tm_mday = bcd2bin(data[PCF2131_REG_TS1_DM] & 0x3F);
	/* TS_MO register (month) value range: 1-12 */
	tm.tm_mon = bcd2bin(data[PCF2131_REG_TS1_MO] & 0x1F) - 1;
	tm.tm_year = bcd2bin(data[PCF2131_REG_TS1_YR]);
	if (tm.tm_year < 70)
		tm.tm_year += 100; /* assume we are in 1970...2069 */

	ret = rtc_valid_tm(&tm);
	if (ret)
		return ret;

	return sprintf(buf, "%llu\n",
		       (unsigned long long)rtc_tm_to_time64(&tm));
};

static DEVICE_ATTR_RW(timestamp0);

static struct attribute *pcf2131_attrs[] = {
	&dev_attr_timestamp0.attr,
	NULL
};

static const struct attribute_group pcf2131_attr_group = {
	.attrs	= pcf2131_attrs,
};

static irqreturn_t pcf2131_rtc_irq(int irq, void *dev)
{
	struct pcf2131 *pcf2131 = dev_get_drvdata(dev);
	unsigned int ctrl2;
	int ret = 0;

	ret = regmap_read(pcf2131->regmap, PCF2131_REG_CTRL2, &ctrl2);
	if (ret)
		return IRQ_NONE;

	if (!(ctrl2 & PCF2131_CTRL2_IRQ_MASK))
		return IRQ_NONE;

	if (ctrl2 & PCF2131_CTRL2_IRQ_MASK)
		regmap_write(pcf2131->regmap, PCF2131_REG_CTRL2,
			ctrl2 & ~PCF2131_CTRL2_IRQ_MASK);

	if (ctrl2 & PCF2131_BIT_CTRL2_AF)
		rtc_update_irq(pcf2131->rtc, 1, RTC_IRQF | RTC_AF);

	pcf2131_wdt_active_ping(&pcf2131->wdd);

	return IRQ_HANDLED;
}

static int pcf2131_probe(struct device *dev, struct regmap *regmap,
			int alarm_irq, const char *name, bool has_nvmem)
{
	struct pcf2131 *pcf2131;
	u32 wdd_timeout;
	int ret = 0;

	pcf2131 = devm_kzalloc(dev, sizeof(*pcf2131), GFP_KERNEL);
	if (!pcf2131)
		return -ENOMEM;

	pcf2131->regmap = regmap;

	dev_set_drvdata(dev, pcf2131);

	pcf2131->rtc = devm_rtc_allocate_device(dev);
	if (IS_ERR(pcf2131->rtc))
		return PTR_ERR(pcf2131->rtc);

	pcf2131->rtc->ops = &pcf2131_rtc_ops;
	pcf2131->rtc->range_min = RTC_TIMESTAMP_BEGIN_2000;
	pcf2131->rtc->range_max = RTC_TIMESTAMP_END_2099;
	pcf2131->rtc->set_start_time = true; /* Sets actual start to 1970 */
	clear_bit(RTC_FEATURE_ALARM, pcf2131->rtc->features);

	pcf2131->wdd.parent = dev;
	pcf2131->wdd.info = &pcf2131_wdt_info;
	pcf2131->wdd.ops = &pcf2131_watchdog_ops;
	pcf2131->wdd.min_timeout = PCF2131_WD_VAL_MIN;
	pcf2131->wdd.max_timeout = PCF2131_WD_VAL_MAX;
	pcf2131->wdd.timeout = PCF2131_WD_VAL_DEFAULT;
	pcf2131->wdd.min_hw_heartbeat_ms = 500;
	pcf2131->wdd.status = WATCHDOG_NOWAYOUT_INIT_STATUS;

	watchdog_set_drvdata(&pcf2131->wdd, pcf2131);
	if (alarm_irq > 0) {
		ret = devm_request_threaded_irq(dev, alarm_irq, NULL,
						pcf2131_rtc_irq,
						(IRQF_TRIGGER_FALLING | IRQF_ONESHOT),
						dev_name(dev), dev);
		if (ret) {
			dev_err(dev, "failed to request alarm irq\n");
			return ret;
		}
	}
	if (alarm_irq > 0 || device_property_read_bool(dev, "wakeup-source")) {
		device_init_wakeup(dev, true);
		set_bit(RTC_FEATURE_ALARM, pcf2131->rtc->features);
	}
	/*
	 * Watchdog timer enabled and reset pin /RST activated when timed out.
	 * Select 1Hz clock source for watchdog timer.
	 * Note: Countdown timer disabled and not available.
	 */
	ret = regmap_update_bits(pcf2131->regmap, PCF2131_REG_WD_CTL,
				 PCF2131_BIT_WD_CTL_CD |
				 PCF2131_BIT_WD_CTL_TF1 |
				 PCF2131_BIT_WD_CTL_TF0,
				 PCF2131_BIT_WD_CTL_CD |
				 PCF2131_BIT_WD_CTL_TF1);
	if (ret) {
		dev_err(dev, "%s: watchdog config (wd_ctl) failed\n", __func__);
		return ret;
	}

	/* Test if watchdog timer is started by bootloader */
	ret = regmap_read(pcf2131->regmap, PCF2131_REG_WD_VAL, &wdd_timeout);
	if (ret)
		return ret;

	if (wdd_timeout)
		set_bit(WDOG_HW_RUNNING, &pcf2131->wdd.status);

#ifdef CONFIG_WATCHDOG
	ret = devm_watchdog_register_device(dev, &pcf2131->wdd);
	if (ret)
		return ret;
#endif /* CONFIG_WATCHDOG */

	/*
	 * Disable battery low/switch-over timestamp and interrupts.
	 * Clear battery interrupt flags which can block new trigger events.
	 * Note: This is the default chip behaviour but added to ensure
	 * correct tamper timestamp and interrupt function.
	 */
	ret = regmap_update_bits(pcf2131->regmap, PCF2131_REG_CTRL3,
				 PCF2131_BIT_CTRL3_BTSE |
				 PCF2131_BIT_CTRL3_BIE |
				 PCF2131_BIT_CTRL3_BLIE, 0);
	if (ret) {
		dev_err(dev, "%s: interrupt config (ctrl3) failed\n",
			__func__);
		return ret;
	}

	/*
	 * Enable timestamp function and store timestamp of first trigger
	 * event until TSF1 interrupt flags are cleared.
	 */
	ret = regmap_update_bits(pcf2131->regmap, PCF2131_REG_TS1_CTRL,
				 PCF2131_BIT_TS1_CTRL_TSOFF |
				 PCF2131_BIT_TS1_CTRL_TSM,
				 PCF2131_BIT_TS1_CTRL_TSM);
	if (ret) {
		dev_err(dev, "%s: tamper detection config (ts1_ctrl) failed\n",
			__func__);
		return ret;
	}

	/*
	 * Enable interrupt generation when TSF1 timestamp flags
	 * are set. Interrupt signal is an open-drain output and can be
	 * left floating if unused.
	 */
	ret = regmap_update_bits(pcf2131->regmap, PCF2131_REG_CTRL5,
				 PCF2131_BIT_CTRL5_TSIE1,
				 PCF2131_BIT_CTRL5_TSIE1);
	if (ret) {
		dev_err(dev, "%s: tamper detection config (ctrl5) failed\n",
			__func__);
		return ret;
	}

	ret = rtc_add_group(pcf2131->rtc, &pcf2131_attr_group);
	if (ret) {
		dev_err(dev, "%s: tamper sysfs registering failed\n",
			__func__);
		return ret;
	}

	return devm_rtc_register_device(pcf2131->rtc);
}

#ifdef CONFIG_OF
static const struct of_device_id pcf2131_of_match[] = {
	{ .compatible = "nxp,pcf2131" },
	{}
};
MODULE_DEVICE_TABLE(of, pcf2131_of_match);
#endif

#if IS_ENABLED(CONFIG_I2C)

static int pcf2131_i2c_write(void *context, const void *data, size_t count)
{
	struct device *dev = context;
	struct i2c_client *client = to_i2c_client(dev);
	int ret;

	ret = i2c_master_send(client, data, count);
	if (ret != count)
		return ret < 0 ? ret : -EIO;

	return 0;
}

static int pcf2131_i2c_gather_write(void *context,
				const void *reg, size_t reg_size,
				const void *val, size_t val_size)
{
	struct device *dev = context;
	struct i2c_client *client = to_i2c_client(dev);
	int ret;
	void *buf;

	if (WARN_ON(reg_size != 1))
		return -EINVAL;

	buf = kmalloc(val_size + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, reg, 1);
	memcpy(buf + 1, val, val_size);

	ret = i2c_master_send(client, buf, val_size + 1);

	kfree(buf);

	if (ret != val_size + 1)
		return ret < 0 ? ret : -EIO;

	return 0;
}

static int pcf2131_i2c_read(void *context, const void *reg, size_t reg_size,
				void *val, size_t val_size)
{
	struct device *dev = context;
	struct i2c_client *client = to_i2c_client(dev);
	int ret;

	if (WARN_ON(reg_size != 1))
		return -EINVAL;

	ret = i2c_master_send(client, reg, 1);
	if (ret != 1)
		return ret < 0 ? ret : -EIO;

	ret = i2c_master_recv(client, val, val_size);
	if (ret != val_size)
		return ret < 0 ? ret : -EIO;

	return 0;
}

/*
 * The reason we need this custom regmap_bus instead of using regmap_init_i2c()
 * is that the STOP condition is required between set register address and
 * read register data when reading from registers.
 */
static const struct regmap_bus pcf2131_i2c_regmap = {
	.write = pcf2131_i2c_write,
	.gather_write = pcf2131_i2c_gather_write,
	.read = pcf2131_i2c_read,
};

static struct i2c_driver pcf2131_i2c_driver;

static int pcf2131_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct regmap *regmap;
	static const struct regmap_config config = {
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0x36,
	};

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	regmap = devm_regmap_init(&client->dev, &pcf2131_i2c_regmap,
					&client->dev, &config);
	if (IS_ERR(regmap)) {
		dev_err(&client->dev, "%s: regmap allocation failed: %ld\n",
			__func__, PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}

	return pcf2131_probe(&client->dev, regmap, client->irq,
			     pcf2131_i2c_driver.driver.name, id->driver_data);
}

static const struct i2c_device_id pcf2131_i2c_id[] = {
	{ "pcf2131", 1 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pcf2131_i2c_id);

static struct i2c_driver pcf2131_i2c_driver = {
	.driver		= {
		.name	= "rtc-pcf2131-i2c",
		.of_match_table = of_match_ptr(pcf2131_of_match),
	},
	.probe		= pcf2131_i2c_probe,
	.id_table	= pcf2131_i2c_id,
};

static int pcf2131_i2c_register_driver(void)
{
	return i2c_add_driver(&pcf2131_i2c_driver);
}

static void pcf2131_i2c_unregister_driver(void)
{
	i2c_del_driver(&pcf2131_i2c_driver);
}

#else

static int pcf2131_i2c_register_driver(void)
{
	return 0;
}

static void pcf2131_i2c_unregister_driver(void)
{
}

#endif

#if IS_ENABLED(CONFIG_SPI_MASTER)

static struct spi_driver pcf2131_spi_driver;

static int pcf2131_spi_probe(struct spi_device *spi)
{
	static const struct regmap_config config = {
		.reg_bits = 8,
		.val_bits = 8,
		.read_flag_mask = 0xa0,
		.write_flag_mask = 0x20,
		.max_register = 0x36,
	};
	struct regmap *regmap;

	regmap = devm_regmap_init_spi(spi, &config);
	if (IS_ERR(regmap)) {
		dev_err(&spi->dev, "%s: regmap allocation failed: %ld\n",
			__func__, PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}

	return pcf2131_probe(&spi->dev, regmap, spi->irq,
			     pcf2131_spi_driver.driver.name,
			     spi_get_device_id(spi)->driver_data);
}

static const struct spi_device_id pcf2131_spi_id[] = {
	{ "pcf2131", 1 },
	{ }
};
MODULE_DEVICE_TABLE(spi, pcf2131_spi_id);

static struct spi_driver pcf2131_spi_driver = {
	.driver		= {
		.name	= "rtc-pcf2131-spi",
		.of_match_table = of_match_ptr(pcf2131_of_match),
	},
	.probe		= pcf2131_spi_probe,
	.id_table	= pcf2131_spi_id,
};

static int pcf2131_spi_register_driver(void)
{
	return spi_register_driver(&pcf2131_spi_driver);
}

static void pcf2131_spi_unregister_driver(void)
{
	spi_unregister_driver(&pcf2131_spi_driver);
}

#else

static int pcf2131_spi_register_driver(void)
{
	return 0;
}

static void pcf2131_spi_unregister_driver(void)
{
}

#endif

static int __init pcf2131_init(void)
{
	int ret;

	ret = pcf2131_i2c_register_driver();
	if (ret) {
		pr_err("Failed to register pcf2131 i2c driver: %d\n", ret);
		return ret;
	}

	ret = pcf2131_spi_register_driver();
	if (ret) {
		pr_err("Failed to register pcf2131 spi driver: %d\n", ret);
		pcf2131_i2c_unregister_driver();
	}

	return ret;
}
module_init(pcf2131_init)

static void __exit pcf2131_exit(void)
{
	pcf2131_spi_unregister_driver();
	pcf2131_i2c_unregister_driver();
}
module_exit(pcf2131_exit)

MODULE_AUTHOR("NXP");
MODULE_DESCRIPTION("NXP PCF2131 RTC driver");
MODULE_LICENSE("GPL v2");
