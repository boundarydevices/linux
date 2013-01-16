/*
 * Copyright (C) 2011-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*!
 * @file bus_freq.c
 *
 * @brief A common API for the Freescale Semiconductor i.MXC CPUfreq module
 * and DVFS CORE module.
 *
 * The APIs are for setting bus frequency to low or high.
 *
 * @ingroup PM
 */
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/mutex.h>
#include <mach/hardware.h>
#include <mach/clock.h>
#include <mach/busfreq.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <asm/tlb.h>
#include <linux/suspend.h>

#define LPAPM_CLK		24000000
#define DDR_AUDIO_CLK		50000000
#define DDR_MED_CLK		400000000
#define DDR3_NORMAL_CLK		528000000

int high_bus_freq_mode;
int med_bus_freq_mode;
int audio_bus_freq_mode;
int low_bus_freq_mode;

static int bus_freq_scaling_initialized;
static struct device *busfreq_dev;
static int busfreq_suspended;

static int bus_freq_scaling_is_active;
static int high_bus_count, med_bus_count, audio_bus_count;
static unsigned int ddr_low_rate;
unsigned int ddr_med_rate;
unsigned int ddr_normal_rate;

extern int init_mmdc_settings(void);
extern int update_ddr_freq(int ddr_rate);

DEFINE_MUTEX(bus_freq_mutex);

static struct clk *pll2_400;
static struct clk *periph_clk;
static struct clk *periph_pre_clk;
static struct clk *periph_clk2_sel;
static struct clk *periph_clk2;
static struct clk *osc_clk;
static struct clk *cpu_clk;
static struct clk *pll3;
static struct clk *pll2;
static struct clk *pll2_200;

static struct delayed_work low_bus_freq_handler;
static struct delayed_work bus_freq_daemon;

int low_bus_freq;

int reduce_bus_freq(void)
{
	int ret = 0;
	clk_enable(pll3);
	if (low_bus_freq) {
		/* Need to ensure that PLL2_PFD_400M is kept ON. */
		clk_enable(pll2_400);
		update_ddr_freq(DDR_AUDIO_CLK);
		/* Make sure periph clk's parent also got updated */
		ret = clk_set_parent(periph_clk2_sel, pll3);
		if (ret)
			printk(KERN_WARNING "%s: %d: clk set parent fail!\n",
				__func__, __LINE__);
		ret = clk_set_parent(periph_pre_clk, pll2_200);
		if (ret)
			printk(KERN_WARNING "%s: %d: clk set parent fail!\n",
				__func__, __LINE__);
		ret = clk_set_parent(periph_clk, periph_pre_clk);
		if (ret)
			printk(KERN_WARNING "%s: %d: clk set parent fail!\n",
				__func__, __LINE__);
		audio_bus_freq_mode = 1;
		low_bus_freq_mode = 0;
	} else {
		update_ddr_freq(LPAPM_CLK);
		/* Make sure periph clk's parent also got updated */
		ret = clk_set_parent(periph_clk2_sel, osc_clk);
		if (ret)
			printk(KERN_WARNING "%s: %d: clk set parent fail!\n",
				__func__, __LINE__);
		ret = clk_set_parent(periph_clk, periph_clk2);
		if (ret)
			printk(KERN_WARNING "%s: %d: clk set parent fail!\n",
				__func__, __LINE__);
		if (audio_bus_freq_mode)
			clk_disable(pll2_400);
		low_bus_freq_mode = 1;
		audio_bus_freq_mode = 0;
	}
	if (med_bus_freq_mode)
		clk_disable(pll2_400);

	clk_disable(pll3);
	med_bus_freq_mode = 0;
	high_bus_freq_mode = 0;

	if (audio_bus_freq_mode)
		printk(KERN_DEBUG "Bus freq set to audio mode. Count:\
			high %d, med %d, audio %d\n",
			high_bus_count, med_bus_count, audio_bus_count);
	if (low_bus_freq_mode)
		printk(KERN_DEBUG "Bus freq set to low mode. Count:\
			high %d, med %d, audio %d\n",
			high_bus_count, med_bus_count, audio_bus_count);

	return ret;
}

static void reduce_bus_freq_handler(struct work_struct *work)
{
	mutex_lock(&bus_freq_mutex);

	reduce_bus_freq();

	mutex_unlock(&bus_freq_mutex);
}

/* Set the DDR, AHB to 24MHz.
  * This mode will be activated only when none of the modules that
  * need a higher DDR or AHB frequency are active.
  */
int set_low_bus_freq(int low_bus_mode)
{
	if (busfreq_suspended)
		return 0;

	if (!bus_freq_scaling_initialized || !bus_freq_scaling_is_active)
		return 0;

	/* Don't lower the frequency immediately. Instead
	  * scheduled a delayed work and drop the freq if
	  * the conditions still remain the same.
	  */
	low_bus_freq = low_bus_mode;
	schedule_delayed_work(&low_bus_freq_handler,
				usecs_to_jiffies(3000000));
	return 0;
}

/* Set the DDR to either 528MHz or 400MHz for MX6q
 * or 400MHz for MX6DL.
 */
int set_high_bus_freq(int high_bus_freq)
{
	int ret = 0;

	if (bus_freq_scaling_initialized && bus_freq_scaling_is_active)
		cancel_delayed_work_sync(&low_bus_freq_handler);

	if (busfreq_suspended)
		return 0;
	high_bus_freq = 1;

	if (!bus_freq_scaling_initialized || !bus_freq_scaling_is_active)
		return 0;

	if (high_bus_freq_mode && high_bus_freq)
		return 0;

	/* medium bus freq is only supported for MX6DQ */
	if (med_bus_freq_mode && !high_bus_freq)
		return 0;

	clk_enable(pll3);
	if (high_bus_freq) {
		update_ddr_freq(ddr_normal_rate);
		/* Make sure periph clk's parent also got updated */
		ret = clk_set_parent(periph_clk2_sel, pll3);
		if (ret)
			printk(KERN_WARNING "%s: %d: clk set parent fail!\n",
				__func__, __LINE__);
		ret = clk_set_parent(periph_pre_clk, pll2);
		if (ret)
			printk(KERN_WARNING "%s: %d: clk set parent fail!\n",
				__func__, __LINE__);
		ret = clk_set_parent(periph_clk, periph_pre_clk);
		if (ret)
			printk(KERN_WARNING "%s: %d: clk set parent fail!\n",
				__func__, __LINE__);
		if (med_bus_freq_mode)
			clk_disable(pll2_400);
		high_bus_freq_mode = 1;
		med_bus_freq_mode = 0;
	} else {
		clk_enable(pll2_400);
		update_ddr_freq(ddr_med_rate);
		/* Make sure periph clk's parent also got updated */
		ret = clk_set_parent(periph_clk2_sel, pll3);
		if (ret)
			printk(KERN_WARNING "%s: %d: clk set parent fail!\n",
				__func__, __LINE__);
		ret = clk_set_parent(periph_pre_clk, pll2_400);
		if (ret)
			printk(KERN_WARNING "%s: %d: clk set parent fail!\n",
				__func__, __LINE__);
		ret = clk_set_parent(periph_clk, periph_pre_clk);
		if (ret)
			printk(KERN_WARNING "%s: %d: clk set parent fail!\n",
				__func__, __LINE__);
		high_bus_freq_mode = 0;
		med_bus_freq_mode = 1;
	}
	if (audio_bus_freq_mode)
		clk_disable(pll2_400);

	low_bus_freq_mode = 0;
	audio_bus_freq_mode = 0;

	clk_disable(pll3);

	if (high_bus_freq_mode)
		printk(KERN_DEBUG "Bus freq set to high mode. Count:\
			high %d, med %d, audio %d\n",
			high_bus_count, med_bus_count, audio_bus_count);
	if (med_bus_freq_mode)
		printk(KERN_DEBUG "Bus freq set to med mode. Count:\
			high %d, med %d, audio %d\n",
			high_bus_count, med_bus_count, audio_bus_count);

	return 0;
}
void request_bus_freq(enum bus_freq_mode mode)
{
	mutex_lock(&bus_freq_mutex);
	if (mode == BUS_FREQ_HIGH)
		high_bus_count++;
	else if (mode == BUS_FREQ_MED)
		med_bus_count++;
	else if (mode == BUS_FREQ_AUDIO)
		audio_bus_count++;

	if (busfreq_suspended || !bus_freq_scaling_initialized ||
		!bus_freq_scaling_is_active) {
		mutex_unlock(&bus_freq_mutex);
		return;
	}

	cancel_delayed_work_sync(&low_bus_freq_handler);
	if ((mode == BUS_FREQ_HIGH) && (!high_bus_freq_mode)) {
		set_high_bus_freq(1);
		mutex_unlock(&bus_freq_mutex);
		return;
	}
	if ((mode == BUS_FREQ_MED) && (!high_bus_freq_mode) &&
		(!med_bus_freq_mode)) {
		set_high_bus_freq(0);
		mutex_unlock(&bus_freq_mutex);
		return;
	}
	if ((mode == BUS_FREQ_AUDIO) && (!high_bus_freq_mode) &&
		(!med_bus_freq_mode) && (!audio_bus_freq_mode)) {
		set_low_bus_freq(1);
		mutex_unlock(&bus_freq_mutex);
		return;
	}
	mutex_unlock(&bus_freq_mutex);
	return;
}
EXPORT_SYMBOL(request_bus_freq);

void release_bus_freq(enum bus_freq_mode mode)
{
	mutex_lock(&bus_freq_mutex);
	if (mode == BUS_FREQ_HIGH) {
		if (high_bus_count == 0) {
			printk(KERN_ERR "high bus count mismatch!\n");
			dump_stack();
			mutex_unlock(&bus_freq_mutex);
			return;
		}
		high_bus_count--;
	} else if (mode == BUS_FREQ_MED) {
		if (med_bus_count == 0) {
			printk(KERN_ERR "med bus count mismatch!\n");
			dump_stack();
			mutex_unlock(&bus_freq_mutex);
			return;
		}
		med_bus_count--;
	} else if (mode == BUS_FREQ_AUDIO) {
		if (audio_bus_count == 0) {
			printk(KERN_ERR "audio bus count mismatch!\n");
			dump_stack();
			mutex_unlock(&bus_freq_mutex);
			return;
		}
		audio_bus_count--;
	}
	if (busfreq_suspended || !bus_freq_scaling_initialized ||
		!bus_freq_scaling_is_active) {
		mutex_unlock(&bus_freq_mutex);
		return;
	}

	if ((!audio_bus_freq_mode) && (high_bus_count == 0) &&
		(med_bus_count == 0) && (audio_bus_count != 0)) {
		set_low_bus_freq(1);
		mutex_unlock(&bus_freq_mutex);
		return;
	}
	if ((!low_bus_freq_mode) && (high_bus_count == 0) &&
		(med_bus_count == 0) && (audio_bus_count == 0))
		set_low_bus_freq(0);

	mutex_unlock(&bus_freq_mutex);
	return;
}
EXPORT_SYMBOL(release_bus_freq);

static void bus_freq_daemon_handler(struct work_struct *work)
{
	mutex_lock(&bus_freq_mutex);
	high_bus_count--;
	if ((!low_bus_freq_mode) && (high_bus_count == 0) &&
		(med_bus_count == 0) && (audio_bus_count == 0))
		set_low_bus_freq(0);
	mutex_unlock(&bus_freq_mutex);
}
static ssize_t bus_freq_scaling_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (bus_freq_scaling_is_active)
		return sprintf(buf, "Bus frequency scaling is enabled\n");
	else
		return sprintf(buf, "Bus frequency scaling is disabled\n");
}

static ssize_t bus_freq_scaling_enable_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	if (strncmp(buf, "1", 1) == 0) {
		bus_freq_scaling_is_active = 1;
		set_high_bus_freq(1);
		high_bus_count++;
		/* We set bus freq to highest at the beginning,
		 * so we use this daemon thread to make sure system
		 * can enter low bus mode if
		 * there is no high bus request pending */
		schedule_delayed_work(&bus_freq_daemon,
			usecs_to_jiffies(5000000));
	} else if (strncmp(buf, "0", 1) == 0) {
		if (bus_freq_scaling_is_active)
			set_high_bus_freq(1);
		bus_freq_scaling_is_active = 0;
	}
	return size;
}

static int bus_freq_pm_notify(struct notifier_block *nb, unsigned long event,
	void *dummy)
{
	mutex_lock(&bus_freq_mutex);

	if (event == PM_SUSPEND_PREPARE) {
		high_bus_count++;
		set_high_bus_freq(1);
		busfreq_suspended = 1;
	} else if (event == PM_POST_SUSPEND) {
		busfreq_suspended = 0;
		schedule_delayed_work(&bus_freq_daemon,
			usecs_to_jiffies(5000000));
	}

	mutex_unlock(&bus_freq_mutex);

	return NOTIFY_OK;
}
static struct notifier_block imx_bus_freq_pm_notifier = {
	.notifier_call = bus_freq_pm_notify,
};

static DEVICE_ATTR(enable, 0644, bus_freq_scaling_enable_show,
			bus_freq_scaling_enable_store);

/*!
 * This is the probe routine for the bus frequency driver.
 *
 * @param   pdev   The platform device structure
 *
 * @return         The function returns 0 on success
 *
 */

static int __devinit busfreq_probe(struct platform_device *pdev)
{
	u32 err;

	busfreq_dev = &pdev->dev;

	pll2_400 = clk_get(NULL, "pll2_pfd2_396m");
	if (IS_ERR(pll2_400)) {
		printk(KERN_WARNING "%s: failed to get pll2_pfd2_396m\n",
		       __func__);
		return PTR_ERR(pll2_400);
	}
	pll2_200 = devm_clk_get(&pdev->dev, "pll2_198m_clk");
	if (IS_ERR(pll2_200)) {
		printk(KERN_WARNING "%s: failed to get pll2_198m\n",
		       __func__);
		return PTR_ERR(pll2_200);
	}

	pll2 = devm_clk_get(&pdev->dev, "pll2_bus_clk");
	if (IS_ERR(pll2)) {
		printk(KERN_WARNING "%s: failed to get pll2_bus\n",
		       __func__);
		return PTR_ERR(pll2);
	}

	cpu_clk = devm_clk_get(&pdev->dev, "arm_clk");
	if (IS_ERR(cpu_clk)) {
		printk(KERN_WARNING "%s: failed to get cpu_clk\n",
		       __func__);
		return PTR_ERR(cpu_clk);
	}

	pll3 = devm_clk_get(&pdev->dev, "pll3_usb_otg_clk");
	if (IS_ERR(pll3)) {
		printk(KERN_WARNING "%s: failed to get pll3_usb_otg\n",
		       __func__);
		return PTR_ERR(pll3);
	}

	periph_clk = devm_clk_get(&pdev->dev, "periph_clk");
	if (IS_ERR(periph_clk)) {
		printk(KERN_WARNING "%s: failed to get periph\n",
		       __func__);
		return PTR_ERR(periph_clk);
	}

	periph_pre_clk = devm_clk_get(&pdev->dev, "periph_pre_clk");
	if (IS_ERR(periph_pre_clk)) {
		printk(KERN_WARNING "%s: failed to get periph_pre\n",
		       __func__);
		return PTR_ERR(periph_pre_clk);
	}

	periph_clk2 = devm_clk_get(&pdev->dev, "periph_clk2_clk");
	if (IS_ERR(periph_clk2)) {
		printk(KERN_WARNING "%s: failed to get periph_clk2\n",
		       __func__);
		return PTR_ERR(periph_clk2);
	}

	periph_clk2_sel = devm_clk_get(&pdev->dev, "periph_clk2_sel_clk");
	if (IS_ERR(periph_clk2_sel)) {
		printk(KERN_WARNING "%s: failed to get periph_clk2_sel\n",
		       __func__);
		return PTR_ERR(periph_clk2_sel);
	}

	osc_clk = devm_clk_get(&pdev->dev, "osc_clk");
	if (IS_ERR(osc_clk)) {
		printk(KERN_WARNING "%s: failed to get osc_clk\n",
		       __func__);
		return PTR_ERR(osc_clk);
	}

	err = sysfs_create_file(&busfreq_dev->kobj, &dev_attr_enable.attr);
	if (err) {
		printk(KERN_ERR
		       "Unable to register sysdev entry for BUSFREQ");
		return err;
	}

	high_bus_freq_mode = 1;
	med_bus_freq_mode = 0;
	low_bus_freq_mode = 0;

	bus_freq_scaling_is_active = 1;
	bus_freq_scaling_initialized = 1;

	ddr_low_rate = LPAPM_CLK;
	ddr_med_rate = DDR_MED_CLK;
	ddr_normal_rate = DDR3_NORMAL_CLK;

	INIT_DELAYED_WORK(&low_bus_freq_handler, reduce_bus_freq_handler);
	INIT_DELAYED_WORK(&bus_freq_daemon, bus_freq_daemon_handler);
	register_pm_notifier(&imx_bus_freq_pm_notifier);

	init_mmdc_settings();

	return 0;
}

static const struct of_device_id imx6q_busfreq_ids[] = {
	{ .compatible = "fsl,imx_busfreq", },
	{ /* sentinel */ }
};
static struct platform_driver busfreq_driver = {
	.driver = {
		   .name = "imx_busfreq",
		   .of_match_table = imx6q_busfreq_ids,
		},
	.probe = busfreq_probe,
};

/*!
 * Initialise the busfreq_driver.
 *
 * @return  The function always returns 0.
 */

static int __init busfreq_init(void)
{
	if (platform_driver_register(&busfreq_driver) != 0) {
		printk(KERN_ERR "busfreq_driver register failed\n");
		return -ENODEV;
	}

	printk(KERN_INFO "Bus freq driver module loaded\n");

	return 0;
}

static void __exit busfreq_cleanup(void)
{
	sysfs_remove_file(&busfreq_dev->kobj, &dev_attr_enable.attr);

	/* Unregister the device structure */
	platform_driver_unregister(&busfreq_driver);
	bus_freq_scaling_initialized = 0;
}

late_initcall(busfreq_init);
module_exit(busfreq_cleanup);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("BusFreq driver");
MODULE_LICENSE("GPL");
