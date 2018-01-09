/*
 * drivers/amlogic/vrtc/aml_vrtc.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/pm_wakeup.h>
#include <linux/amlogic/scpi_protocol.h>

static void __iomem *alarm_reg_vaddr;
static void __iomem *timere_low_vaddr, *timere_high_vaddr;
static unsigned int vrtc_init_date;

#define TIME_LEN 10
static int parse_init_date(const char *date)
{
	char local_str[TIME_LEN + 1];
	char *year_s, *month_s, *day_s, *str;
	unsigned int year_d, month_d, day_d;
	int ret;

	if (strlen(date) != 10)
		return -1;
	memset(local_str, 0, TIME_LEN + 1);
	strncpy(local_str, date, TIME_LEN);
	str = local_str;
	year_s = strsep(&str, "/");
	if (!year_s)
		return -1;
	month_s = strsep(&str, "/");
	if (!month_s)
		return -1;
	day_s = str;
	pr_debug("year: %s\nmonth: %s\nday: %s\n", year_s, month_s, day_s);
	ret = kstrtou32(year_s, 10, &year_d);
	if (ret < 0 || year_d > 2100 || year_d < 1900)
		return -1;
	ret = kstrtou32(month_s, 10, &month_d);
	if (ret < 0 || month_d > 12)
		return -1;
	ret = kstrtou32(day_s, 10, &day_d);
	if (ret < 0 || day_d > 31)
		return -1;
	vrtc_init_date = mktime(year_d, month_d, day_d, 0, 0, 0);
	return 0;
}

static u32 timere_read(void)
{
	u32 time = 0;
	unsigned long te = 0, temp = 0;

	/*timeE high+low, first read low, second read high*/
	te = readl(timere_low_vaddr);
	temp =  readl(timere_high_vaddr);
	te += (temp << 32);
	time = (u32)(te / 1000000);
	pr_debug("----------time_e: %us\n", time);
	return time;
}

static u32 read_te(void)
{
	u32 time = 0;

	if (timere_low_vaddr && timere_high_vaddr) {
		time = timere_read();
		time += vrtc_init_date;
	}
	return time;
}

static int aml_vrtc_read_time(struct device *dev, struct rtc_time *tm)
{
	u32 time_t = read_te();

	rtc_time_to_tm(time_t, tm);

	return 0;
}

static int aml_rtc_write_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long time_t;
	unsigned int time = 0;

	rtc_tm_to_time(tm, &time_t);
	pr_debug("aml_rtc : write the rtc time, time is %ld\n", time_t);

	if (timere_low_vaddr && timere_high_vaddr)
		time = timere_read();
	vrtc_init_date = (unsigned int)time_t - time;

	return 0;
}

static int set_wakeup_time(unsigned long time)
{
	int ret = -1;

	if (alarm_reg_vaddr) {
		writel(time, alarm_reg_vaddr);
		ret = 0;
		pr_debug("set_wakeup_time: %lu\n", time);
	}
	return ret;
}

static int aml_vrtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	unsigned long alarm_secs, cur_secs;
	int ret;
	struct	rtc_device *vrtc;

	struct rtc_time cur_rtc_time;

	vrtc = dev_get_drvdata(dev);

	if (alarm->enabled) {
		ret = rtc_tm_to_time(&alarm->time, &alarm_secs);
		if (ret)
			return ret;
		aml_vrtc_read_time(dev, &cur_rtc_time);
		ret = rtc_tm_to_time(&cur_rtc_time, &cur_secs);
		if (alarm_secs >= cur_secs) {
			alarm_secs = alarm_secs - cur_secs;
			ret = set_wakeup_time(alarm_secs);
			if (ret < 0)
				return ret;
			pr_debug("system will wakeup %lus later\n", alarm_secs);
		}
	}
	return 0;
}

static const struct rtc_class_ops aml_vrtc_ops = {
	.read_time = aml_vrtc_read_time,
	.set_time = aml_rtc_write_time,
	.set_alarm = aml_vrtc_set_alarm,
};

static int aml_vrtc_probe(struct platform_device *pdev)
{
	struct	rtc_device *vrtc;
	int ret;
	u32 paddr = 0;
	const char *str;
	u32 vrtc_val;

	ret = of_property_read_u32(pdev->dev.of_node,
			"alarm_reg_addr", &paddr);
	if (!ret) {
		pr_debug("alarm_reg_paddr: 0x%x\n", paddr);
		alarm_reg_vaddr = ioremap(paddr, 0x4);
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"timer_e_addr", &paddr);
	if (!ret) {
		pr_debug("timer_e_paddr: 0x%x\n", paddr);
		timere_low_vaddr = ioremap(paddr, 0x4);
		timere_high_vaddr = ioremap(paddr + 4, 0x4);
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"timer_e_addr", &paddr);
	ret = of_property_read_string(pdev->dev.of_node, "init_date", &str);
	if (!ret) {
		pr_debug("init_date: %s\n", str);
		if (!scpi_get_vrtc(&vrtc_val)) {
			vrtc_init_date = vrtc_val;
			pr_debug("get vrtc: %us\n", vrtc_init_date);
		} else
			parse_init_date(str);
	}
	device_init_wakeup(&pdev->dev, 1);
	vrtc = rtc_device_register("aml_vrtc", &pdev->dev,
		&aml_vrtc_ops, THIS_MODULE);
	if (!vrtc)
		return -1;
	platform_set_drvdata(pdev, vrtc);

	return 0;
}

static int aml_vrtc_remove(struct platform_device *dev)
{
	struct rtc_device *vrtc = platform_get_drvdata(dev);

	rtc_device_unregister(vrtc);

	return 0;
}

static int aml_vrtc_resume(struct platform_device *pdev)
{
	set_wakeup_time(0);

	/* If timeE < 20, EE domain is thutdown,	timerE is not
	 * work during suspend. we need get vrtc value.
	 */
	if (read_te() < 20)
		if (!scpi_get_vrtc(&vrtc_init_date))
			pr_debug("get vrtc: %us\n", vrtc_init_date);

	return 0;
}

static int aml_vrtc_suspend(struct platform_device *pdev, pm_message_t state)
{
	u32 vrtc_val;

	vrtc_val = read_te();

	if (scpi_set_vrtc(vrtc_val))
		pr_debug("vrtc setting fail.\n");

	return 0;
}


static void aml_vrtc_shutdown(struct platform_device *pdev)
{
	u32 vrtc_val;
	struct timespec	new_system;

	vrtc_val = read_te();
	getnstimeofday(&new_system);

	vrtc_val = (u32)new_system.tv_sec;

	if (scpi_set_vrtc(vrtc_val))
		pr_debug("vrtc setting fail.\n");
}


static const struct of_device_id aml_vrtc_dt_match[] = {
	{ .compatible = "amlogic, aml_vrtc"},
	{},
};

struct platform_driver aml_vrtc_driver = {
	.driver = {
		.name = "aml_vrtc",
		.owner = THIS_MODULE,
		.of_match_table = aml_vrtc_dt_match,
	},
	.probe = aml_vrtc_probe,
	.remove = aml_vrtc_remove,
	.resume = aml_vrtc_resume,
	.suspend = aml_vrtc_suspend,
	.shutdown = aml_vrtc_shutdown,
};

static int  __init aml_vrtc_init(void)
{
	return platform_driver_register(&aml_vrtc_driver);
}

static void __init aml_vrtc_exit(void)
{
	return platform_driver_unregister(&aml_vrtc_driver);
}

module_init(aml_vrtc_init);
module_exit(aml_vrtc_exit);

MODULE_DESCRIPTION("Amlogic internal vrtc driver");
MODULE_LICENSE("GPL");
