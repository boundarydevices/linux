/*
 *  anatop_thermal.c - anatop Thermal Zone Driver ($Revision: 41 $)
 *
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *  Copyright (C) 2011-2013 Freescale Semiconductor, Inc.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/kmod.h>
#include <linux/reboot.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include "anatop_driver.h"
#include <asm/system_misc.h>
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <mach/hardware.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <asm/irq.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/stat.h>
#include <linux/mfd/syscon.h>

/* register define of anatop */
#define HW_ANADIG_ANA_MISC0		(0x00000150)
#define HW_ANADIG_ANA_MISC0_SET		(0x00000154)
#define HW_ANADIG_ANA_MISC0_CLR		(0x00000158)
#define HW_ANADIG_ANA_MISC0_TOG		(0x0000015c)
#define HW_ANADIG_ANA_MISC1		(0x00000160)
#define HW_ANADIG_ANA_MISC1_SET		(0x00000164)
#define HW_ANADIG_ANA_MISC1_CLR		(0x00000168)
#define HW_ANADIG_ANA_MISC1_TOG		(0x0000016c)

#define HW_ANADIG_TEMPSENSE0		(0x00000180)
#define HW_ANADIG_TEMPSENSE0_SET	(0x00000184)
#define HW_ANADIG_TEMPSENSE0_CLR	(0x00000188)
#define HW_ANADIG_TEMPSENSE0_TOG	(0x0000018c)

#define HW_ANADIG_TEMPSENSE1		(0x00000190)
#define HW_ANADIG_TEMPSENSE1_SET	(0x00000194)
#define HW_ANADIG_TEMPSENSE1_CLR	(0x00000198)

#define BM_ANADIG_ANA_MISC0_REFTOP_SELBIASOFF		0x00000008
#define BM_ANADIG_ANA_MISC1_IRQ_TEMPSENSE		0x20000000

#define BP_ANADIG_TEMPSENSE0_ALARM_VALUE		20
#define BM_ANADIG_TEMPSENSE0_ALARM_VALUE		0xFFF00000
#define BF_ANADIG_TEMPSENSE0_ALARM_VALUE(v)  \
	(((v) << 20) & BM_ANADIG_TEMPSENSE0_ALARM_VALUE)
#define BP_ANADIG_TEMPSENSE0_TEMP_VALUE			8
#define BM_ANADIG_TEMPSENSE0_TEMP_VALUE			0x000FFF00
#define BF_ANADIG_TEMPSENSE0_TEMP_VALUE(v)  \
	(((v) << 8) & BM_ANADIG_TEMPSENSE0_TEMP_VALUE)
#define BM_ANADIG_TEMPSENSE0_RSVD0			0x00000080
#define BM_ANADIG_TEMPSENSE0_TEST			0x00000040
#define BP_ANADIG_TEMPSENSE0_VBGADJ			3
#define BM_ANADIG_TEMPSENSE0_VBGADJ			0x00000038
#define BF_ANADIG_TEMPSENSE0_VBGADJ(v)  \
	(((v) << 3) & BM_ANADIG_TEMPSENSE0_VBGADJ)
#define BM_ANADIG_TEMPSENSE0_FINISHED			0x00000004
#define BM_ANADIG_TEMPSENSE0_MEASURE_TEMP		0x00000002
#define BM_ANADIG_TEMPSENSE0_POWER_DOWN			0x00000001

#define BP_ANADIG_TEMPSENSE1_RSVD0			16
#define BM_ANADIG_TEMPSENSE1_RSVD0			0xFFFF0000
#define BF_ANADIG_TEMPSENSE1_RSVD0(v) \
	(((v) << 16) & BM_ANADIG_TEMPSENSE1_RSVD0)
#define BP_ANADIG_TEMPSENSE1_MEASURE_FREQ		0
#define BM_ANADIG_TEMPSENSE1_MEASURE_FREQ		0x0000FFFF
#define BF_ANADIG_TEMPSENSE1_MEASURE_FREQ(v)  \
	(((v) << 0) & BM_ANADIG_TEMPSENSE1_MEASURE_FREQ)

/* define */
#define PREFIX					"ANATOP: "
#define ANATOP_THERMAL_NOTIFY_CRITICAL		0xF0
#define ANATOP_THERMAL_NOTIFY_HOT		0xF1
#define ANATOP_TRIPS_CRITICAL			0x01
#define ANATOP_TRIPS_HOT			0x02
#define ANATOP_TRIPS_ACTIVE			0x08

#define ANATOP_TRIPS_POINT_CRITICAL		0x0
#define ANATOP_TRIPS_POINT_HOT			0x1
#define ANATOP_TRIPS_POINT_ACTIVE		0x2

#define ANATOP_TRIPS_INIT      (ANATOP_TRIPS_CRITICAL | \
		ANATOP_TRIPS_HOT | ANATOP_TRIPS_ACTIVE)

#define KELVIN_OFFSET				273
#define TEMP_CRITICAL				373	/* 100 C*/
#define TEMP_HOT				363	/* 90 C*/
#define TEMP_ACTIVE				353	/* 80 C*/
/* 3276 RTC clocks delay, 100ms */
#define MEASURE_FREQ				3276
#define KELVIN_TO_CEL(t, off) (((t) - (off)))
#define CEL_TO_KELVIN(t, off) (((t) + (off)))
#define DEFAULT_RATIO				145
#define DEFAULT_N40C				1563
#define REG_VALUE_TO_CEL(ratio, raw)	((raw_n40c - raw) * 100 / ratio - 40)
#define ANATOP_DEBUG				false
#define THERMAL_FUSE_NAME			"/sys/fsl_otp/HW_OCOTP_ANA1"
#define THERMAL_UPDATE_INTERVAL			2000
#define CPUx					"/sys/devices/system/cpu/cpu"
#define MAX_GOVERNOR_NAME_LEN			12
#define MAX_CPU_FREQ_LEN			7
#define MAX_CPU_ONLINE_LEN			1

#define        FACTOR1         15976
#define        FACTOR2         4297157

/* variables */
static unsigned int anatop_base, ratio, raw_25c, raw_hot;
static unsigned int hot_temp, raw_n40c, raw_125c, raw_critical, thermal_irq;
static struct clk *pll3_clk;
static bool full_run = true, suspend_flag, cooling_cpuhotplug;
static bool calibration_valid;
static bool cooling_device_disable;
static const struct anatop_device_id thermal_device_ids[] = {
	{ANATOP_THERMAL_HID},
	{""},
};

int thermal_hot;
EXPORT_SYMBOL(thermal_hot);
atomic_t thermal_on = ATOMIC_INIT(1);

extern struct cpu_op *(*get_cpu_op)(int *op);
/* Save the string we will read/write to the sys file */
static unsigned char cpu_sys_file[80];
static unsigned int cpu_mask;
static unsigned int anatop_thermal_cpufreq_is_init;
/* Save cpu operation table and current work point */
static struct cpu_op *cpu_op_tbl;
static int cpu_op_nr;

/* Save the cpu freq necessray setting before thermal driver change it */
static char saved_max_cpufreq_str[10];

/* New cpufreq to be set */
static unsigned int cpufreq_new;
static char cpufreq_new_str[10];

/* This variable record the cpufreq change, when we lower the
cpufreq, it minor 1, and when we promote cpufreq, it add 1, so
if it is 0, mean we didn't change the cpufreq */
static int cpufreq_change_count;

enum {
	DEBUG_USER_STATE = 1U << 0,
	DEBUG_VERBOSE = 1U << 1,
};
static int debug_mask = DEBUG_USER_STATE;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

/* functions */
static int anatop_thermal_remove(struct platform_device *pdev);
static int anatop_thermal_suspend(struct platform_device *pdev,
		pm_message_t state);
static int anatop_thermal_resume(struct platform_device *pdev);
static int anatop_thermal_counting_ratio(unsigned int fuse_data);

/* struct */
struct anatop_thermal_state {
	u8 critical:1;
	u8 hot:1;
	u8 passive:1;
	u8 active:1;
	u8 reserved:4;
	int active_index;
};

struct anatop_thermal_state_flags {
	u8 valid:1;
	u8 enabled:1;
	u8 reserved:6;
};

struct anatop_thermal_critical {
	struct anatop_thermal_state_flags flags;
	unsigned long temperature;
};

struct anatop_thermal_hot {
	struct anatop_thermal_state_flags flags;
	unsigned long temperature;
};

struct anatop_thermal_passive {
	struct anatop_thermal_state_flags flags;
	unsigned long temperature;
	unsigned long tc1;
	unsigned long tc2;
	unsigned long tsp;
};

struct anatop_thermal_active {
	struct anatop_thermal_state_flags flags;
	unsigned long temperature;
};

struct anatop_thermal_trips {
	struct anatop_thermal_critical critical;
	struct anatop_thermal_hot hot;
	struct anatop_thermal_passive passive;
	struct anatop_thermal_active active;
};

struct anatop_thermal_flags {
	u8 cooling_mode:1;
	u8 devices:1;
	u8 reserved:6;
};

struct anatop_thermal {
	struct device device;
	anatop_handle handle;		/* no handle for fixed hardware */
	char name[40];
	int id;
	void *driver_data;
	long temperature;
	unsigned long last_temperature;
	unsigned long polling_frequency;
	unsigned char zombie;
	struct delayed_work thermal_work;
	struct anatop_thermal_flags flags;
	struct anatop_thermal_state state;
	struct anatop_thermal_trips trips;
	struct anatop_handle_list devices;
	struct thermal_zone_device *thermal_zone;
	int tz_enabled;
	int kelvin_offset;
	struct mutex lock;
	int passive_delay;
	int polling_delay;
	unsigned int trips_count;
	char type[THERMAL_NAME_LENGTH];
};

/***************************************************************
			Function
****************************************************************/
int anatop_thermal_cpufreq_up(void)
{
	int ret = -EINVAL;
#ifdef CONFIG_CPU_FREQ
	int fd;
	if (cpufreq_change_count > 0)
		return 0;

	strcpy(cpu_sys_file, CPUx);
	strcat(cpu_sys_file, "0/cpufreq/scaling_max_freq");

	fd = sys_open((const char __user __force *)cpu_sys_file,
		O_RDWR, 0700);
	if (fd >= 0) {
		sys_write(fd, saved_max_cpufreq_str,
			strlen(saved_max_cpufreq_str));
		cpufreq_update_policy(0);
		sys_close(fd);
		ret = 0;
	}
	cpufreq_change_count++;
#endif
	return ret;
}
int anatop_thermal_cpufreq_down(void)
{
	int ret = -EINVAL;
#ifdef CONFIG_CPU_FREQ
	int fd;

	if (cpufreq_change_count < 0)
		return 0;

	cpufreq_new = cpu_op_tbl[0].cpu_rate;
	cpufreq_new /= 1000;

	strcpy(cpu_sys_file, CPUx);
	sprintf(cpufreq_new_str, "%d", cpufreq_new);
	strcat(cpu_sys_file, "0/cpufreq/scaling_max_freq");

	fd = sys_open((const char __user __force *)cpu_sys_file,
	  O_RDWR, 0700);
	if (fd >= 0) {

		sys_read(fd, saved_max_cpufreq_str, MAX_CPU_FREQ_LEN);
		sys_write(fd, cpufreq_new_str, strlen(cpufreq_new_str));
		sys_close(fd);
		ret = 0;
	}

	cpufreq_change_count--;
	cpufreq_update_policy(0);
#endif
	return ret;
}

int anatop_thermal_cpu_hotplug(bool cpu_on)
{
	int ret = -EINVAL;
#ifdef CONFIG_HOTPLUG
	unsigned int cpu;
	char online;
	char cpu_number[9];
	int fd;

	if (cpu_on) {
		for (cpu = 1; cpu < num_possible_cpus(); cpu++) {
			strcpy(cpu_sys_file, CPUx);
			sprintf(cpu_number, "%d%s", cpu, "/online");
			strcat(cpu_sys_file, cpu_number);
			fd = sys_open((const char __user __force *)cpu_sys_file,
			  O_RDWR, 0700);
			if (fd >= 0) {
				sys_read(fd, &online, MAX_CPU_ONLINE_LEN);
				if ((online == '0') && ((cpu_mask &
					(0x1 << cpu)) != 0)) {
					sys_write(fd, (char *)"1",
						MAX_CPU_ONLINE_LEN);
					cpu_mask &= ~(0x1 << cpu);
					ret = 0;
					sys_close(fd);
				}
				sys_close(fd);
			}
		}
	} else {
		if (num_online_cpus() < 2)
			return ret;

		for (cpu = num_possible_cpus() - 1; cpu > 0; cpu--) {
			strcpy(cpu_sys_file, CPUx);
			sprintf(cpu_number, "%d%s", cpu, "/online");
			strcat(cpu_sys_file, cpu_number);
			fd = sys_open((const char __user __force *)cpu_sys_file,
					O_RDWR, 0700);
			if (fd >= 0) {
				sys_read(fd, &online, 1);
				if (online == '1') {
					sys_write(fd, (char *)"0",
						MAX_CPU_ONLINE_LEN);
					cpu_mask |= 0x1 << cpu;
					ret = 0;
					sys_close(fd);
				}
				sys_close(fd);
			}
		}
	}
#endif
	return ret;
}

static BLOCKING_NOTIFIER_HEAD(thermal_chain_head);

int register_thermal_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&thermal_chain_head, nb);
}
EXPORT_SYMBOL_GPL(register_thermal_notifier);

int unregister_thermal_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&thermal_chain_head, nb);
}
EXPORT_SYMBOL_GPL(unregister_thermal_notifier);

int thermal_notifier_call_chain(unsigned long val)
{
	return (blocking_notifier_call_chain(&thermal_chain_head, val,
		NULL) == NOTIFY_BAD) ? -EINVAL : 0;
}

static int
imx_processor_set_cur_state(struct anatop_thermal *thermal,
			unsigned long state)
{
	int result = 0;

	/* state =0 means we are at a low temp, we should try to attach the
	secondary CPUs that detached by thermal driver */
	if (cooling_cpuhotplug) {
		if (!state) {
			thermal_hot = 0;
			if (atomic_read(&thermal_on))
				thermal_notifier_call_chain(0);
			anatop_thermal_cpu_hotplug(true);
		}
	} else {
		if (!state) {
			thermal_hot = 0;
			if (atomic_read(&thermal_on))
				thermal_notifier_call_chain(0);
			if (cpufreq_change_count < 0)
				anatop_thermal_cpufreq_up();
			else if (cpufreq_change_count > 0)
				anatop_thermal_cpufreq_down();
		}
	}

	return result;
}

void anatop_thermal_cpufreq_init(void)
{
	anatop_thermal_cpufreq_is_init = 1;
}

void anatop_thermal_cpufreq_exit(void)
{
	anatop_thermal_cpufreq_is_init = 0;
}
static int anatop_dump_temperature_register(void)
{
	if (!anatop_base) {
		pr_info("anatop_base is not initialized!!!\n");
		return -EINVAL;
	}
	pr_info("HW_ANADIG_TEMPSENSE0 = 0x%x\n",
			__raw_readl(anatop_base + HW_ANADIG_TEMPSENSE0));
	pr_info("HW_ANADIG_TEMPSENSE1 = 0x%x\n",
			__raw_readl(anatop_base + HW_ANADIG_TEMPSENSE1));
	pr_info("HW_ANADIG_MISC1 = 0x%x\n",
			__raw_readl(anatop_base + HW_ANADIG_ANA_MISC1));
	return 0;
}
static void anatop_update_alarm(unsigned int alarm_value)
{
	if (cooling_device_disable || suspend_flag)
		return;
	/* set alarm value */
	__raw_writel(BM_ANADIG_TEMPSENSE0_ALARM_VALUE,
		anatop_base + HW_ANADIG_TEMPSENSE0_CLR);
	__raw_writel(BF_ANADIG_TEMPSENSE0_ALARM_VALUE(alarm_value),
		anatop_base + HW_ANADIG_TEMPSENSE0_SET);

	return;
}
static int anatop_thermal_get_temp(struct anatop_thermal *thermal,
			    long *temp)
{
	struct anatop_thermal *tz = thermal;
	unsigned int tmp;
	unsigned int reg;

	if (!tz)
		return -EINVAL;

	if (!ratio || suspend_flag) {
		*temp = KELVIN_TO_CEL(TEMP_ACTIVE, KELVIN_OFFSET);
		return 0;
	}

	tz->last_temperature = tz->temperature;

	if ((__raw_readl(anatop_base + HW_ANADIG_TEMPSENSE0) &
		BM_ANADIG_TEMPSENSE0_POWER_DOWN) != 0) {
		/* need to keep sensor power up as we enable alarm
		function */
		__raw_writel(BM_ANADIG_TEMPSENSE0_POWER_DOWN,
			anatop_base + HW_ANADIG_TEMPSENSE0_CLR);
		__raw_writel(BM_ANADIG_ANA_MISC0_REFTOP_SELBIASOFF,
			anatop_base + HW_ANADIG_ANA_MISC0_SET);

		/* write measure freq */
		reg = __raw_readl(anatop_base + HW_ANADIG_TEMPSENSE1);
		reg &= ~BM_ANADIG_TEMPSENSE1_MEASURE_FREQ;
		reg |= MEASURE_FREQ;
		__raw_writel(reg, anatop_base + HW_ANADIG_TEMPSENSE1);
	}
	__raw_writel(BM_ANADIG_TEMPSENSE0_MEASURE_TEMP,
		anatop_base + HW_ANADIG_TEMPSENSE0_CLR);
	__raw_writel(BM_ANADIG_TEMPSENSE0_FINISHED,
		anatop_base + HW_ANADIG_TEMPSENSE0_CLR);
	__raw_writel(BM_ANADIG_TEMPSENSE0_MEASURE_TEMP,
		anatop_base + HW_ANADIG_TEMPSENSE0_SET);

	tmp = 0;
	/* read temperature values */
	while ((__raw_readl(anatop_base + HW_ANADIG_TEMPSENSE0)
		& BM_ANADIG_TEMPSENSE0_FINISHED) == 0)
		msleep(10);

	reg = __raw_readl(anatop_base + HW_ANADIG_TEMPSENSE0);
	tmp = (reg & BM_ANADIG_TEMPSENSE0_TEMP_VALUE)
		>> BP_ANADIG_TEMPSENSE0_TEMP_VALUE;
	__raw_writel(BM_ANADIG_TEMPSENSE0_FINISHED,
		anatop_base + HW_ANADIG_TEMPSENSE0_CLR);

	if (ANATOP_DEBUG)
		anatop_dump_temperature_register();
	/* only the temp between -40C and 125C is valid, this
	is for save */
	if (tmp <= raw_n40c && tmp >= raw_125c)
		tz->temperature = REG_VALUE_TO_CEL(ratio, tmp);
	else {
		printk(KERN_WARNING "Invalid temperature, force it to 25C\n");
		tz->temperature = 25;
	}

	if (debug_mask & DEBUG_VERBOSE)
		pr_info("Cooling device Temperature is %lu C\n",
			tz->temperature);

	*temp = (cooling_device_disable && tz->temperature >=
		KELVIN_TO_CEL(TEMP_CRITICAL, KELVIN_OFFSET)) ?
		KELVIN_TO_CEL(TEMP_CRITICAL - 1, KELVIN_OFFSET) :
			tz->temperature;

	/* Set alarm threshold if necessary */
	if ((__raw_readl(anatop_base + HW_ANADIG_TEMPSENSE0) &
		BM_ANADIG_TEMPSENSE0_ALARM_VALUE) == 0)
		anatop_update_alarm(raw_critical);

	return 0;
}

static int anatop_thermal_trips_update(struct anatop_thermal *tz, int flag)
{
	/* Critical, Shutdown */
	if (flag & ANATOP_TRIPS_CRITICAL) {
		tz->trips.critical.temperature = TEMP_CRITICAL - 273;
		tz->trips.critical.flags.valid = 1;
	}

	/* Hot, take actions to lower temperature */
	if (flag & ANATOP_TRIPS_HOT) {
			tz->trips.hot.temperature = TEMP_HOT - 273;
			tz->trips.hot.flags.valid = 1;
	}

	/* Active, safe state */
	if (flag & ANATOP_TRIPS_ACTIVE) {
			tz->trips.active.temperature = TEMP_ACTIVE - 273;
			tz->trips.active.flags.valid = 1;
	}

	return 0;
}

static int anatop_thermal_get_trip_points(struct anatop_thermal *tz)
{
	int valid, ret;

	ret = anatop_thermal_trips_update(tz, ANATOP_TRIPS_INIT);

	valid = tz->trips.critical.flags.valid |
		tz->trips.hot.flags.valid |
		tz->trips.active.flags.valid;

	if (!valid) {
		printk(KERN_WARNING "No valid trip found\n");
		ret = -ENODEV;
	}
	return ret;
}

static int anatop_thermal_get_trip_type(struct anatop_thermal *thermal,
				 int trip, enum thermal_trip_type *type)
{
	struct anatop_thermal *tz = thermal;

	if (!tz || trip < 0)
		return -EINVAL;

	if (tz->trips.critical.flags.valid) {
		if (!trip) {
			*type = THERMAL_TRIP_CRITICAL;
			return 0;
		}
		trip--;
	}

	if (tz->trips.hot.flags.valid) {
		if (!trip) {
			*type = THERMAL_TRIP_HOT;
			return 0;
		}
		trip--;
	}

	if (tz->trips.passive.flags.valid) {
		if (!trip) {
			*type = THERMAL_TRIP_PASSIVE;
			return 0;
		}
		trip--;
	}

	if (tz->trips.active.flags.valid) {
		if (!trip) {
			*type = THERMAL_TRIP_ACTIVE;
			return 0;
		}
		trip--;
	}

	return -EINVAL;
}

static int anatop_thermal_set_trip_temp(struct anatop_thermal *thermal,
				 int trip, unsigned long *temp)
{
	struct anatop_thermal *tz = thermal;

	if (!tz || trip < 0)
		return -EINVAL;

	switch (trip) {
	case ANATOP_TRIPS_POINT_CRITICAL:
		/* Critical Shutdown */
		if (tz->trips.critical.flags.valid) {
			tz->trips.critical.temperature = CEL_TO_KELVIN(
				*temp, tz->kelvin_offset);
			raw_critical = raw_25c - ratio * (*temp - 25) / 100;
			anatop_update_alarm(raw_critical);
		}
		break;
	case ANATOP_TRIPS_POINT_HOT:
		/* Hot */
		if (tz->trips.hot.flags.valid)
				tz->trips.hot.temperature = CEL_TO_KELVIN(
				*temp, tz->kelvin_offset);
		break;
	case ANATOP_TRIPS_POINT_ACTIVE:
		/* Active */
		if (tz->trips.active.flags.valid)
				tz->trips.active.temperature = CEL_TO_KELVIN(
				*temp, tz->kelvin_offset);
		break;
	default:
		break;
	}

	return 0;
}

static int anatop_thermal_get_trip_temp(struct anatop_thermal *thermal,
				 int trip, unsigned long *temp)
{
	struct anatop_thermal *tz = thermal;

	if (!tz || trip < 0)
		return -EINVAL;
	if (tz->trips.critical.flags.valid) {
		if (!trip) {
			*temp = KELVIN_TO_CEL(
				tz->trips.critical.temperature,
				tz->kelvin_offset);
			return 0;
		}
		trip--;
	}

	if (tz->trips.hot.flags.valid) {
		if (!trip) {
			*temp = KELVIN_TO_CEL(
				tz->trips.hot.temperature,
				tz->kelvin_offset);
			return 0;
		}
		trip--;
	}

	if (tz->trips.passive.flags.valid) {
		if (!trip) {
			*temp = KELVIN_TO_CEL(
				tz->trips.passive.temperature,
				tz->kelvin_offset);
			return 0;
		}
		trip--;
	}

	if (tz->trips.active.flags.valid) {
		if (!trip) {
			*temp = KELVIN_TO_CEL(
				tz->trips.active.temperature,
				tz->kelvin_offset);
			return 0;
		}
		trip--;
	}

	return -EINVAL;
}

static int anatop_thermal_notify(struct anatop_thermal *thermal,
	int trip, enum thermal_trip_type trip_type)
{
	int ret = 0;
	u8 type = 0;
	char mode = 'r';
	const char *cmd = "reboot";

	if (cooling_device_disable)
		return ret;

	if (trip_type == THERMAL_TRIP_CRITICAL) {
		type = ANATOP_THERMAL_NOTIFY_CRITICAL;
		printk(KERN_WARNING "thermal_notify: trip_critical reached!\n");
		arm_pm_restart(mode, cmd);
	} else if (trip_type == THERMAL_TRIP_HOT) {
		if (atomic_read(&thermal_on))
			thermal_notifier_call_chain(1);
		thermal_hot = 1;
		printk(KERN_DEBUG "thermal_notify: trip_hot reached!\n");
		type = ANATOP_THERMAL_NOTIFY_HOT;
		/* if temperature increase, continue to detach secondary CPUs */
		if (cooling_cpuhotplug)
			anatop_thermal_cpu_hotplug(false);
		else
			anatop_thermal_cpufreq_down();
		if (ret)
			printk(KERN_INFO "No secondary CPUs detached!\n");
		full_run = false;
	} else {
		if (!full_run) {
			if (atomic_read(&thermal_on))
				thermal_notifier_call_chain(0);
			thermal_hot = 0;
			if (cooling_cpuhotplug)
				anatop_thermal_cpu_hotplug(true);
			else
				anatop_thermal_cpufreq_up();
			if (ret)
				printk(KERN_INFO "No secondary CPUs attached!\n");
		}
	}
	return 0;
}


static int anatop_thermal_get_info(struct anatop_thermal *tz)
{
	int result = 0;

	if (!tz)
		return -EINVAL;
	result = anatop_thermal_get_trip_points(tz);
	if (result)
		return result;

	if (tz->trips.critical.flags.valid)
		tz->trips_count++;

	if (tz->trips.hot.flags.valid)
		tz->trips_count++;

	if (tz->trips.passive.flags.valid)
		tz->trips_count++;

	if (tz->trips.active.flags.valid)
		tz->trips_count++;

	/* Get default polling frequency [_TZP] (optional) */
	tz->polling_delay = THERMAL_UPDATE_INTERVAL;
	return 0;
}

static int __init anatop_thermal_cooling_device_setup(char *str)
{
	if (!strcmp(str, "cpuhotplug")) {
		cooling_cpuhotplug = 1;
		pr_info("%s: cooling device set to hotplug!\n", __func__);
	}
	return 1;
}
__setup("cooling_device=", anatop_thermal_cooling_device_setup);

static int __init anatop_thermal_cooling_device_disable(char *str)
{
	cooling_device_disable = 1;
	pr_info("%s: cooling device is disabled!\n", __func__);
	return 1;
}
__setup("no_cooling_device", anatop_thermal_cooling_device_disable);

static int __init anatop_thermal_use_calibration(char *str)
{
	calibration_valid = true;
	pr_info("%s: use calibration data for thermal sensor!\n", __func__);

	return 1;
}
__setup("use_calibration", anatop_thermal_use_calibration);

static int anatop_thermal_counting_ratio(unsigned int fuse_data)
{
	int ret = -EINVAL;

	pr_info("Thermal calibration data is 0x%x\n", fuse_data);
	if (fuse_data == 0 || fuse_data == 0xffffffff ||
		(fuse_data & 0xfff00000) == 0) {
		pr_info(KERN_INFO
			"%s: invalid calibration data, disable cooling!!!\n",
			__func__);
		cooling_device_disable = true;
		ratio = DEFAULT_RATIO;
		disable_irq(thermal_irq);
		return ret;
	}

	ret = 0;
	/* Fuse data layout:
	 * [31:20] sensor value @ 25C
	 * [19:8] sensor value of hot
	 * [7:0] hot temperature value
	 */
	raw_25c = fuse_data >> 20;
	raw_hot = (fuse_data & 0xfff00) >> 8;
	hot_temp = fuse_data & 0xff;

	if (!calibration_valid)
		/*
		 * The universal equation for thermal sensor
		 * is slope = 0.4297157 - (0.0015976 * 25C fuse),
		 * here we convert them to integer to make them
		 * easy for counting, FACTOR1 is 15976,
		 * FACTOR2 is 4297157. Our ratio = -100 * slope.
		 */
		ratio = ((FACTOR1 * raw_25c - FACTOR2) + 50000) / 100000;
	else
		ratio = ((raw_25c - raw_hot) * 100) / (hot_temp - 25);

	pr_info("Thermal sensor with ratio = %d\n", ratio);

	raw_n40c = raw_25c + (13 * ratio) / 20;
	raw_125c = raw_25c - ratio;
	/* Init default critical temp to set alarm */
	raw_critical = raw_25c - ratio *
		(KELVIN_TO_CEL(TEMP_CRITICAL, KELVIN_OFFSET) - 25) / 100;
	clk_prepare_enable(pll3_clk);

	return ret;
}

static irqreturn_t anatop_thermal_alarm_handler(int irq, void *dev_id)
{
	char mode = 'r';
	const char *cmd = "reboot";

	if (cooling_device_disable)
		return IRQ_HANDLED;
	printk(KERN_WARNING "\nChip is too hot, reboot!!!\n");
	/* reboot */
	arm_pm_restart(mode, cmd);

	return IRQ_HANDLED;
}

static void thermal_device_set_polling(struct anatop_thermal *tz,
					    int delay)
{
	cancel_delayed_work(&(tz->thermal_work));

	if (!delay)
		return;

	if (delay > 1000)
		schedule_delayed_work(&(tz->thermal_work),
				      round_jiffies(msecs_to_jiffies(delay)));
	else
		schedule_delayed_work(&(tz->thermal_work),
				      msecs_to_jiffies(delay));
}

void thermal_device_update(struct anatop_thermal *tz)
{
	int count;
	long temp, trip_temp;
	int ret;
	enum thermal_trip_type trip_type;
	mutex_lock(&tz->lock);

	if (anatop_thermal_get_temp(tz, &temp)) {
		/* get_temp failed - retry it later */
		printk(KERN_WARNING PREFIX "failed to read out thermal zone "
		       "%d\n", tz->id);
	}

	for (count = 0; count < tz->trips_count; count++) {
		anatop_thermal_get_trip_type(tz, count, &trip_type);
		anatop_thermal_get_trip_temp(tz, count, &trip_temp);
		switch (trip_type) {
		case THERMAL_TRIP_CRITICAL:
			if (temp >= trip_temp) {
				ret = anatop_thermal_notify(tz, count,
					trip_type);
				if (!ret) {
					printk(KERN_EMERG
					       "Critical temperature reached (%ld C), shutting down.\n",
					       temp / 1000);
					orderly_poweroff(true);
				}
			}
			break;
		case THERMAL_TRIP_HOT:
			if (temp >= trip_temp) {
					anatop_thermal_notify(tz, count,
						trip_type);
				}
			break;
		case THERMAL_TRIP_ACTIVE:
			if (temp >= trip_temp) {
				imx_processor_set_cur_state(tz, 1);
			} else {
				imx_processor_set_cur_state(tz, 0);
				}
			break;
		case THERMAL_TRIP_PASSIVE:
			break;
		}
	}
	tz->last_temperature = temp;
	if (tz->polling_delay)
		thermal_device_set_polling(tz, tz->polling_delay);
	else
		thermal_device_set_polling(tz, 0);
	mutex_unlock(&tz->lock);
}

static void anatop_thermal_work(struct work_struct *work)
{
	struct anatop_thermal *data;
	data = container_of(work, struct anatop_thermal, thermal_work.work);
	thermal_device_update(data);
}

/* thermal temp sys interface */
static ssize_t
temp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	long temperature;
	int ret;
	struct anatop_thermal *tz;
	tz = dev_get_drvdata(dev);
	mutex_lock(&tz->lock);
	ret = anatop_thermal_get_temp(tz, &temperature);
	mutex_unlock(&tz->lock);
	if (ret)
		pr_info("thermal temperature read fail!");
	if (ret)
		return ret;
	return sprintf(buf, "%ld\n", temperature);
}
static DEVICE_ATTR(temp, S_IRUSR | S_IWUSR,
	temp_show, NULL);

/* thermal trips type sys interface */
static ssize_t
trip_point_type_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	enum thermal_trip_type type;
	int trip, result;
	struct anatop_thermal *tz = dev_get_drvdata(dev);
	if (!sscanf(attr->attr.name, "trip_point_%d_type", &trip))
		return -EINVAL;
	result = anatop_thermal_get_trip_type(tz, trip, &type);

	if (result)
		return result;

	switch (type) {
	case THERMAL_TRIP_CRITICAL:
		return sprintf(buf, "critical\n");
	case THERMAL_TRIP_HOT:
		return sprintf(buf, "hot\n");
	case THERMAL_TRIP_PASSIVE:
		return sprintf(buf, "passive\n");
	case THERMAL_TRIP_ACTIVE:
		return sprintf(buf, "active\n");
	default:
		return sprintf(buf, "unknown\n");
	}
}
static DEVICE_ATTR(trip_point_0_type, S_IRUSR | S_IWUSR,
		trip_point_type_show, NULL);
static DEVICE_ATTR(trip_point_1_type, S_IRUSR | S_IWUSR,
		trip_point_type_show, NULL);
static DEVICE_ATTR(trip_point_2_type, S_IRUSR | S_IWUSR,
		trip_point_type_show, NULL);

/* thermal trips temp sys interface */
static ssize_t
trip_point_temp_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct anatop_thermal *tz = dev_get_drvdata(dev);
	int trip, ret;
	long temperature;

	if (!sscanf(attr->attr.name, "trip_point_%d_temp", &trip))
		return -EINVAL;

	ret = anatop_thermal_get_trip_temp(tz, trip, &temperature);
	if (ret)
		return ret;
	return sprintf(buf, "%ld\n", temperature);
}

static ssize_t
trip_point_temp_store(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct anatop_thermal *tz = dev_get_drvdata(dev);
	int trip, ret;
	long temperature;

	if (!sscanf(attr->attr.name, "trip_point_%d_temp", &trip))
		return -EINVAL;

	ret = sscanf(buf, "%lu", &temperature);

	ret = anatop_thermal_set_trip_temp(tz, trip, &temperature);
	if (ret)
		return -EINVAL;

	return count;

}

static DEVICE_ATTR(trip_point_0_temp, S_IRUGO | S_IWUSR,
	trip_point_temp_show, trip_point_temp_store);
static DEVICE_ATTR(trip_point_1_temp, S_IRUGO | S_IWUSR,
	trip_point_temp_show, trip_point_temp_store);
static DEVICE_ATTR(trip_point_2_temp, S_IRUGO | S_IWUSR,
	trip_point_temp_show, trip_point_temp_store);

/* thermal flag sys interface */
static ssize_t anatop_thermal_flag_show(struct device *dev,
			struct device_attribute *attr, char *buf) {
       return sprintf(buf, "read thermal_hot_flag:%d\n",
				atomic_read(&thermal_on));
}

static ssize_t anatop_thermal_flag_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	int ret;
	unsigned long data;

	ret = strict_strtoul(buf, 10, &data);
	if (data == 0)
		atomic_set(&thermal_on, 0);
	else
		atomic_set(&thermal_on, 1);
	return count;
}

static DEVICE_ATTR(thermal_hot_flag, S_IRUSR | S_IWUSR,
	anatop_thermal_flag_show, anatop_thermal_flag_store);

#define anatop_thermal_probe_failed(log)	\
	if (retval) {	\
		printk(KERN_ERR log);	\
		goto thermal_failed;	\
	}	\

static int anatop_thermal_probe(struct platform_device *pdev)
{
	int retval = 0;
	int irq = 0;
	int i;
	struct resource *res_calibration;
	void __iomem *base, *calibration_addr;
	struct anatop_thermal *data;
	struct device_node *np;
	struct device_node *node;

	data = devm_kzalloc(&pdev->dev, sizeof(struct anatop_thermal),
				GFP_KERNEL);
	if (!data) {
		retval = -ENOMEM;
		dev_err(&pdev->dev, "failed to devm_kzalloc!\n");
		goto device_alloc_failed;
	}
	strcpy(data->type, "i.mx_processor");
	platform_set_drvdata(pdev, data);

	retval = anatop_thermal_get_info(data);
	anatop_thermal_probe_failed("Thermal: anatop thermal get info failed\n")

	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-anatop");
	base = of_iomap(np, 0);
	anatop_base = (unsigned long)base;

	/* ioremap the calibration data address */
	res_calibration = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res_calibration == NULL) {
		dev_err(&pdev->dev,
			"No anatop calibration data address provided!\n");
		goto thermal_failed;
	}
	calibration_addr = devm_request_and_ioremap(&pdev->dev,
		res_calibration);
	if (calibration_addr == NULL) {
		dev_err(&pdev->dev,
			"failed to remap anatop calibration data address!\n");
		goto thermal_failed;
	}

	pll3_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(pll3_clk)) {
			retval = -ENOENT;
			dev_err(&pdev->dev, "can't get pll3 clock\n");
			goto thermal_failed;
	}
	raw_n40c = DEFAULT_N40C;
	/* use calibration data to get ratio */
	anatop_thermal_counting_ratio(__raw_readl(calibration_addr));

	node = of_find_node_by_name(NULL, "cpufreq-setpoint");
	if (!node) {
		printk(KERN_ERR
			"%s: failed to find device tree data!\n", __func__);
		return -EINVAL;
	}

	retval = of_property_read_u32(node, "setpoint-number", &cpu_op_nr);
	if (retval) {
		printk(KERN_ERR
			"%s: failed to get setpoint number!\n", __func__);
		return -EINVAL;
	}

	cpu_op_tbl = kzalloc(sizeof(*cpu_op_tbl) * cpu_op_nr, GFP_KERNEL);
	if (!cpu_op_tbl)
		return -ENOMEM;

	i = 0;
	for_each_compatible_node(node, NULL, "setpoint-table") {
		if (of_property_read_u32(node, "pll_rate",
			&(cpu_op_tbl[i].pll_rate)))
			continue;
		if (of_property_read_u32(node, "cpu_rate",
			&(cpu_op_tbl[i].cpu_rate)))
			continue;
		if (of_property_read_u32(node, "cpu_podf",
			&(cpu_op_tbl[i].cpu_podf)))
			continue;
		if (of_property_read_u32(node, "pu_voltage",
			&(cpu_op_tbl[i].pu_voltage)))
			continue;
		if (of_property_read_u32(node, "soc_voltage",
			&(cpu_op_tbl[i].soc_voltage)))
			continue;
		if (of_property_read_u32(node, "cpu_voltage",
			&(cpu_op_tbl[i].cpu_voltage)))
			continue;
		i++;
	}

	irq = irq_of_parse_and_map(np, 0);
	if (irq == NO_IRQ) {
		dev_err(&pdev->dev, "No anatop thermal irq provided!\n");
		goto thermal_failed;

	}

	retval = request_irq(irq, anatop_thermal_alarm_handler, 0,
		"anatop-thermal", data);
	if (retval) {
		dev_err(&pdev->dev, "can't get irq%d: %d\n", irq, retval);
		goto thermal_failed;
	}
	thermal_irq = irq;

	anatop_thermal_cpufreq_init();
	mutex_init(&data->lock);

	retval = sysfs_create_file(&pdev->dev.kobj, &dev_attr_temp.attr);
	anatop_thermal_probe_failed(
		"Thermal: sysfs_create_file temp failed\n")

	retval = sysfs_create_file(&pdev->dev.kobj,
		&dev_attr_trip_point_0_type.attr);
	anatop_thermal_probe_failed(
		"Thermal: sysfs_create_file trip_point_0_type failed\n");

	retval = sysfs_create_file(&pdev->dev.kobj,
		&dev_attr_trip_point_0_temp.attr);
	anatop_thermal_probe_failed(
		"Thermal: sysfs_create_file trip_point_0_temp failed\n");

	retval = sysfs_create_file(&pdev->dev.kobj,
		&dev_attr_trip_point_1_type.attr);
	anatop_thermal_probe_failed(
		"Thermal: sysfs_create_file trip_point_1_type failed\n");

	retval = sysfs_create_file(&pdev->dev.kobj,
		&dev_attr_trip_point_1_temp.attr);
	anatop_thermal_probe_failed(
		"Thermal: sysfs_create_file trip_point_1_temp failed\n");

	retval = sysfs_create_file(&pdev->dev.kobj,
		&dev_attr_trip_point_2_type.attr);
	anatop_thermal_probe_failed(
		"Thermal: sysfs_create_file trip_point_2_type failed\n");

	retval = sysfs_create_file(&pdev->dev.kobj,
		&dev_attr_trip_point_2_temp.attr);
	anatop_thermal_probe_failed(
		"Thermal: sysfs_create_file trip_point_2_temp failed\n");

	retval = sysfs_create_file(&pdev->dev.kobj,
		&dev_attr_thermal_hot_flag.attr);
	anatop_thermal_probe_failed(
		"Thermal: sysfs_create_file thermal_hot_flag failed\n");

	INIT_DELAYED_WORK(&data->thermal_work, anatop_thermal_work);
	thermal_device_set_polling(data, data->polling_delay);
	pr_info("%s: default cooling device is cpufreq!\n", __func__);
	goto success;

thermal_failed:
	kfree(data);
	kfree(cpu_op_tbl);
device_alloc_failed:
success:
	return retval;
}

static int anatop_thermal_remove(struct platform_device *pdev)
{
	struct anatop_thermal *tz;

	if (!pdev)
		return -EINVAL;
	tz = platform_get_drvdata(pdev);
	if (tz) {
		cancel_delayed_work_sync(&tz->thermal_work);
		sysfs_remove_file(&pdev->dev.kobj, &dev_attr_temp.attr);
		sysfs_remove_file(&pdev->dev.kobj,
			&dev_attr_trip_point_0_type.attr);
		sysfs_remove_file(&pdev->dev.kobj,
			&dev_attr_trip_point_0_temp.attr);
		sysfs_remove_file(&pdev->dev.kobj,
			&dev_attr_trip_point_1_type.attr);
		sysfs_remove_file(&pdev->dev.kobj,
			&dev_attr_trip_point_1_temp.attr);
		sysfs_remove_file(&pdev->dev.kobj,
			&dev_attr_trip_point_2_type.attr);
		sysfs_remove_file(&pdev->dev.kobj,
			&dev_attr_trip_point_2_temp.attr);
		sysfs_remove_file(&pdev->dev.kobj,
			&dev_attr_thermal_hot_flag.attr);
		mutex_destroy(&tz->lock);
		iounmap((void __iomem *)anatop_base);
		platform_set_drvdata(pdev, NULL);
		kfree(tz);
		kfree(cpu_op_tbl);
	}
	return 0;
}
static int anatop_thermal_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct anatop_thermal *tz;
	tz = platform_get_drvdata(pdev);
	if (tz)
		thermal_device_set_polling(tz, 0);

	/* turn off alarm */
	anatop_update_alarm(0);
	suspend_flag = true;
	/* power down anatop thermal sensor */
	__raw_writel(BM_ANADIG_TEMPSENSE0_MEASURE_TEMP,
		anatop_base + HW_ANADIG_TEMPSENSE0_CLR);
	__raw_writel(BM_ANADIG_TEMPSENSE0_FINISHED,
		anatop_base + HW_ANADIG_TEMPSENSE0_CLR);
	__raw_writel(BM_ANADIG_TEMPSENSE0_POWER_DOWN,
		anatop_base + HW_ANADIG_TEMPSENSE0_SET);
	__raw_writel(BM_ANADIG_ANA_MISC0_REFTOP_SELBIASOFF,
		anatop_base + HW_ANADIG_ANA_MISC0_CLR);
	return 0;
}
static int anatop_thermal_resume(struct platform_device *pdev)
{
	struct anatop_thermal *tz = platform_get_drvdata(pdev);
	/* power up anatop thermal sensor */
	__raw_writel(BM_ANADIG_TEMPSENSE0_POWER_DOWN,
		anatop_base + HW_ANADIG_TEMPSENSE0_CLR);
	__raw_writel(BM_ANADIG_ANA_MISC0_REFTOP_SELBIASOFF,
		anatop_base + HW_ANADIG_ANA_MISC0_SET);
	suspend_flag = false;
	/* turn on alarm */
	anatop_update_alarm(raw_critical);
	if (tz) {
		if (tz->polling_delay)
			thermal_device_set_polling(tz, tz->polling_delay);
		else
			thermal_device_set_polling(tz, 0);
	}
	return 0;
}

static const struct of_device_id of_anatop_thermal[] = {
	{ .compatible = "fsl,anatop-thermal",},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_anatop_thermal);

static struct platform_driver anatop_thermal_driver_ldm = {
	.driver = {
		   .name = "anatop_thermal",
		   .owner = THIS_MODULE,
		   .bus = &platform_bus_type,
		   .of_match_table = of_anatop_thermal,
		   },
	.probe = anatop_thermal_probe,
	.remove = anatop_thermal_remove,
	.suspend = anatop_thermal_suspend,
	.resume = anatop_thermal_resume,
};

static int __init anatop_thermal_init(void)
{
	pr_debug("Anatop Thermal driver loading\n");
	return platform_driver_register(&anatop_thermal_driver_ldm);
}

static void __exit anatop_thermal_exit(void)
{
	platform_driver_unregister(&anatop_thermal_driver_ldm);
	pr_debug("Anatop Thermal driver successfully unloaded\n");
}

module_init(anatop_thermal_init);
module_exit(anatop_thermal_exit);
