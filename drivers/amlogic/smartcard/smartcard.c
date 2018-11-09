/*
 * drivers/amlogic/smartcard/smartcard.c
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/fcntl.h>
#include <asm/irq.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/gpio/consumer.h>
#ifndef MESON_CPU_TYPE
#define MESON_CPU_TYPE 0x50
#endif
#ifndef MESON_CPU_TYPE_MESON6
#define MESON_CPU_TYPE_MESON6		0x60
#endif
#ifdef CONFIG_ARCH_ARC700
#include <asm/arch/am_regs.h>
#else
#endif
#include <linux/poll.h>
#include <linux/delay.h>
/*#include < mach/gpio.h > */
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#define OWNER_NAME "smc"
#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6
#include <mach/mod_gate.h>
#include <mach/power_gate.h>
#endif

#include <linux/version.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/amsmc.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/amlogic/media/utils/vdec_reg.h>

#include "smartcard.h"
#include "c_stb_regs_define.h"
#include "smc_reg.h"

#define DRIVER_NAME "amsmc"
#define MODULE_NAME "amsmc"
#define DEVICE_NAME "amsmc"
#define CLASS_NAME "amsmc-class"

#define INPUT 0
#define OUTPUT 1
#define OUTLEVEL_LOW 0
#define OUTLEVEL_HIGH 1
#define PULLLOW 1
#define PULLHIGH 0

/*#define FILE_DEBUG*/
#define MEM_DEBUG
#define SMC_GPIO_NUM_PROP(node, prop_name, str, gpio_pin) { \
	if (!of_property_read_string(node, prop_name, &str)) { \
		gpio_pin = \
		desc_to_gpio(of_get_named_gpiod_flags(node, \
					prop_name, 0, NULL)); \
	} \
}

#ifdef FILE_DEBUG
#define DBUF_SIZE (512*2)
#define pr_dbg(fmt, args...) \
	do { if (smc_debug > 0) { \
			dcnt = sprintf(dbuf, fmt, ## args); \
			debug_write(dbuf, dcnt); \
		} \
	} while (0)
#define pr_dbg printk
#define Fpr(a...) \
	do { \
		if (smc_debug > 1) { \
			dcnt = sprintf(dbuf, a); \
			debug_write(dbuf, dcnt); \
		} \
	} while (0)
#define Ipr		 Fpr

#elif defined(MEM_DEBUG)
#define DBUF_SIZE (1024*1024*1)
#define pr_dbg(fmt, args...) \
	do {\
		if (smc_debug > 0) { \
			if (dwrite > (DBUF_SIZE - 512)) \
				sprintf(dbuf+dwrite, "lost\n"); \
			else { \
				dcnt = sprintf(dbuf+dwrite, fmt, ## args); \
				dwrite += dcnt; \
			} \
		} \
	} while (0)
#define Fpr(_a...) \
	do { \
		if (smc_debug > 1) { \
			if (dwrite > (DBUF_SIZE - 512)) \
				sprintf(dbuf+dwrite, "lost\n"); \
			else { \
				dcnt = print_time(local_clock(), dbuf+dwrite); \
				dwrite += dcnt; \
				dcnt = sprintf(dbuf+dwrite, _a); \
				dwrite += dcnt; \
			} \
		} \
	} while (0)
#define Ipr		 Fpr

#else
#if 1
#define pr_dbg(fmt, args...) \
do {\
	if (smc_debug > 0) \
		pr_err("Smartcard: " fmt, ## args); \
} \
while (0)
#define Fpr(a...)	do { if (smc_debug > 1) printk(a); } while (0)
#define Ipr		 Fpr
#else
#define pr_dbg(fmt, args...)
#define Fpr(a...)
#endif
#endif

#define pr_error(fmt, args...) pr_err("Smartcard: " fmt, ## args)
#define pr_inf(fmt, args...)  pr_err(fmt, ## args)
#define DEBUG_FILE_NAME	 "/storage/external_storage/debug.smc"
static struct file *debug_filp;
static loff_t debug_file_pos;
static int smc_debug;
#ifdef MEM_DEBUG
static char *dbuf;
static int dread, dwrite;
static int dcnt;
#endif
/*no used reset ctl,need use clk in 4.9 kernel*/
static struct clk *aml_smartcard_clk;

#define REG_READ 0
#define REG_WRITE 1
void operate_reg(unsigned int reg, int read_write, unsigned int *value)
{
	void __iomem *vaddr;

	reg = round_down(reg, 0x3);
	vaddr = ioremap(reg, 0x4);
	pr_dbg("smc ioremap %x %p\n", reg, vaddr);
	if (read_write == REG_READ)
		*value = readl(vaddr);
	else if (read_write == REG_WRITE)
		writel(*value, vaddr);
	iounmap(vaddr);
}

void debug_write(const char __user *buf, size_t count)
{
	mm_segment_t old_fs;

	if (!debug_filp)
		return;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (count != vfs_write(debug_filp, buf, count, &debug_file_pos))
		pr_dbg("Failed to write debug file\n");

	set_fs(old_fs);
}

#ifdef MEM_DEBUG
static void open_debug(void)
{
	debug_filp = filp_open(DEBUG_FILE_NAME, O_WRONLY, 0);
	if (IS_ERR(debug_filp)) {
		pr_dbg("smartcard: open debug file failed\n");
		debug_filp = NULL;
	} else {
		pr_dbg("smc: debug file[%s] open.\n", DEBUG_FILE_NAME);
	}
}
static void close_debug(void)
{
	if (debug_filp) {
		filp_close(debug_filp, current->files);
		debug_filp = NULL;
		debug_file_pos = 0;
	}
}

static size_t print_time(u64 ts, char *buf)
{
	unsigned long rem_nsec;

	rem_nsec = do_div(ts, 1000000000);

	if (!buf)
		return snprintf(NULL, 0, "[%5lu.000000] ", (unsigned long)ts);

	return sprintf(buf, "[%5lu.%06lu] ",
			(unsigned long)ts, rem_nsec / 1000);
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id smc_dt_match[] = {
	{	.compatible = "amlogic,smartcard",
	},
	{},
};
#else
#define smc_dt_match NULL
#endif


MODULE_PARM_DESC(smc0_irq, "\n\t\t Irq number of smartcard0");
static int smc0_irq = -1;
module_param(smc0_irq, int, 0644);

MODULE_PARM_DESC(smc0_reset, "\n\t\t Reset GPIO pin of smartcard0");
static int smc0_reset = -1;
module_param(smc0_reset, int, 0644);

MODULE_PARM_DESC(atr_delay, "\n\t\t atr delay");
static int atr_delay;
module_param(atr_delay, int, 0644);

MODULE_PARM_DESC(atr_holdoff, "\n\t\t atr_holdoff");
static int atr_holdoff = 1;
module_param(atr_holdoff, int, 0644);

MODULE_PARM_DESC(cwt_det_en, "\n\t\t cwt_det_en");
static int cwt_det_en;
module_param(cwt_det_en, int, 0644);
MODULE_PARM_DESC(btw_det_en, "\n\t\t btw_det_en");
static int btw_det_en;
module_param(btw_det_en, int, 0644);
MODULE_PARM_DESC(etu_msr_en, "\n\t\t etu_msr_en");
static int etu_msr_en;
module_param(etu_msr_en, int, 0644);
MODULE_PARM_DESC(clock_source, "\n\t\t clock_source");
static int clock_source;
module_param(clock_source, int, 0644);

#define NO_HOT_RESET
/*#define DISABLE_RECV_INT*/
/*#define ATR_FROM_INT*/
#define SW_INVERT
/*#define SMC_FIQ*/
/*#define ATR_OUT_READ*/
#define KEEP_PARAM_AFTER_CLOSE

#define CONFIG_AML_SMARTCARD_GPIO_FOR_DET
#define CONFIG_AM_SMARTCARD_GPIO_FOR_RST

#ifdef CONFIG_AML_SMARTCARD_GPIO_FOR_DET
#define DET_FROM_PIO
#endif
#ifdef CONFIG_AM_SMARTCARD_GPIO_FOR_RST
#define RST_FROM_PIO
#endif

#ifndef CONFIG_MESON_ARM_GIC_FIQ
#undef SMC_FIQ
#endif

#ifdef SMC_FIQ
#include < plat/fiq_bridge.h >
#ifndef ATR_FROM_INT
#define ATR_FROM_INT
#endif
#endif

#define RECV_BUF_SIZE	 1024
#define SEND_BUF_SIZE	 1024

#define RESET_ENABLE	  (smc->reset_level) /*reset*/
#define RESET_DISABLE	 (!smc->reset_level) /*dis-reset*/

enum sc_type {
	SC_DIRECT,
	SC_INVERSE,
};

struct smc_dev {
	int		id;
	struct device	 *dev;
	struct platform_device *pdev;
	int		init;
	int		used;
	int		cardin;
	int		active;
	struct mutex	  lock;
	spinlock_t		 slock;
	wait_queue_head_t rd_wq;
	wait_queue_head_t wr_wq;
	int		recv_start;
	int		recv_count;
	int		send_start;
	int		send_count;
	char	  recv_buf[RECV_BUF_SIZE];
	char	  send_buf[SEND_BUF_SIZE];
	struct am_smc_param param;
	struct am_smc_atr  atr;

	struct gpio_desc *enable_pin;
#define SMC_ENABLE_PIN_NAME "smc:ENABLE"
	int	  enable_level;

	struct gpio_desc *enable_5v3v_pin;
#define SMC_ENABLE_5V3V_PIN_NAME "smc:5V3V"
	int	  enable_5v3v_level;
	int (*reset)(void*, int);
	int		irq_num;
	int		reset_level;

	u32	 pin_clk_pinmux_reg;
	u32	 pin_clk_pinmux_bit;
	struct gpio_desc *pin_clk_pin;
#define SMC_CLK_PIN_NAME "smc:PINCLK"
	u32	 pin_clk_oen_reg;
	u32	 pin_clk_out_reg;
	u32	 pin_clk_oebit;
	u32	 pin_clk_oubit;
	u32	 use_enable_pin;

#ifdef SW_INVERT
	int		atr_mode;
	enum sc_type		 sc_type;
#endif

	int	  recv_end;
	int	  send_end;
#ifdef SMC_FIQ
	bridge_item_t	 smc_fiq_bridge;
#endif

#ifdef DET_FROM_PIO
	struct gpio_desc *detect_pin;
#define SMC_DETECT_PIN_NAME "smc:DETECT"
#endif
#ifdef RST_FROM_PIO
	struct gpio_desc *reset_pin;
#define SMC_RESET_PIN_NAME "smc:RESET"
#endif

	int		detect_invert;

	struct pinctrl	  *pinctrl;

	struct tasklet_struct	 tasklet;
};

#define SMC_DEV_NAME	 "smc"
#define SMC_CLASS_NAME  "smc-class"
#define SMC_DEV_COUNT	1

#define WRITE_CBUS_REG(_r, _v)   aml_write_cbus(_r, _v)
#define READ_CBUS_REG(_r)        aml_read_cbus(_r)

#define SMC_READ_REG(a)          READ_MPEG_REG(SMARTCARD_##a)
#define SMC_WRITE_REG(a, b)      WRITE_MPEG_REG(SMARTCARD_##a, b)

static struct mutex smc_lock;
static int		 smc_major;
static struct smc_dev	smc_dev[SMC_DEV_COUNT];
static int ENA_GPIO_PULL = 1;
static int DIV_SMC = 3;

#ifdef SW_INVERT
static const unsigned char inv_table[256] = {
	0xFF, 0x7F, 0xBF, 0x3F, 0xDF, 0x5F, 0x9F, 0x1F,
	0xEF, 0x6F, 0xAF, 0x2F, 0xCF, 0x4F, 0x8F, 0x0F,
	0xF7, 0x77, 0xB7, 0x37, 0xD7, 0x57, 0x97, 0x17,
	0xE7, 0x67, 0xA7, 0x27, 0xC7, 0x47, 0x87, 0x07,
	0xFB, 0x7B, 0xBB, 0x3B, 0xDB, 0x5B, 0x9B, 0x1B,
	0xEB, 0x6B, 0xAB, 0x2B, 0xCB, 0x4B, 0x8B, 0x0B,
	0xF3, 0x73, 0xB3, 0x33, 0xD3, 0x53, 0x93, 0x13,
	0xE3, 0x63, 0xA3, 0x23, 0xC3, 0x43, 0x83, 0x03,
	0xFD, 0x7D, 0xBD, 0x3D, 0xDD, 0x5D, 0x9D, 0x1D,
	0xED, 0x6D, 0xAD, 0x2D, 0xCD, 0x4D, 0x8D, 0x0D,
	0xF5, 0x75, 0xB5, 0x35, 0xD5, 0x55, 0x95, 0x15,
	0xE5, 0x65, 0xA5, 0x25, 0xC5, 0x45, 0x85, 0x05,
	0xF9, 0x79, 0xB9, 0x39, 0xD9, 0x59, 0x99, 0x19,
	0xE9, 0x69, 0xA9, 0x29, 0xC9, 0x49, 0x89, 0x09,
	0xF1, 0x71, 0xB1, 0x31, 0xD1, 0x51, 0x91, 0x11,
	0xE1, 0x61, 0xA1, 0x21, 0xC1, 0x41, 0x81, 0x01,
	0xFE, 0x7E, 0xBE, 0x3E, 0xDE, 0x5E, 0x9E, 0x1E,
	0xEE, 0x6E, 0xAE, 0x2E, 0xCE, 0x4E, 0x8E, 0x0E,
	0xF6, 0x76, 0xB6, 0x36, 0xD6, 0x56, 0x96, 0x16,
	0xE6, 0x66, 0xA6, 0x26, 0xC6, 0x46, 0x86, 0x06,
	0xFA, 0x7A, 0xBA, 0x3A, 0xDA, 0x5A, 0x9A, 0x1A,
	0xEA, 0x6A, 0xAA, 0x2A, 0xCA, 0x4A, 0x8A, 0x0A,
	0xF2, 0x72, 0xB2, 0x32, 0xD2, 0x52, 0x92, 0x12,
	0xE2, 0x62, 0xA2, 0x22, 0xC2, 0x42, 0x82, 0x02,
	0xFC, 0x7C, 0xBC, 0x3C, 0xDC, 0x5C, 0x9C, 0x1C,
	0xEC, 0x6C, 0xAC, 0x2C, 0xCC, 0x4C, 0x8C, 0x0C,
	0xF4, 0x74, 0xB4, 0x34, 0xD4, 0x54, 0x94, 0x14,
	0xE4, 0x64, 0xA4, 0x24, 0xC4, 0x44, 0x84, 0x04,
	0xF8, 0x78, 0xB8, 0x38, 0xD8, 0x58, 0x98, 0x18,
	0xE8, 0x68, 0xA8, 0x28, 0xC8, 0x48, 0x88, 0x08,
	0xF0, 0x70, 0xB0, 0x30, 0xD0, 0x50, 0x90, 0x10,
	0xE0, 0x60, 0xA0, 0x20, 0xC0, 0x40, 0x80, 0x00
};
#endif /*SW_INVERT*/

#define dump(b, l) do { \
	int i; \
	pr_dbg("dump: "); \
	for (i = 0; i < (l); i++) \
		pr_dbg("%02x ", *(((unsigned char *)(b))+i)); \
		pr_dbg("\n"); \
} while (0)

static int _gpio_out(struct gpio_desc *gpio, int val, const char *owner);
static int smc_default_init(struct smc_dev *smc);
static int smc_hw_set_param(struct smc_dev *smc);
static int smc_hw_reset(struct smc_dev *smc);
static int smc_hw_active(struct smc_dev *smc);
static int smc_hw_deactive(struct smc_dev *smc);
static int smc_hw_get_status(struct smc_dev *smc, int *sret);

static ssize_t show_gpio_pull(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	if (ENA_GPIO_PULL > 0)
		return sprintf(buf, "%s\n", "enable GPIO pull low");
	else
		return sprintf(buf, "%s\n", "disable GPIO pull low");
}

static ssize_t set_gpio_pull(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int dbg;

	if (kstrtoint(buf, 0, &dbg))
		return -EINVAL;

	ENA_GPIO_PULL = dbg;
	return count;
}

static ssize_t show_5v3v(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct smc_dev *smc = NULL;
	int enable_5v3v = 0;

	mutex_lock(&smc_lock);
	smc = &smc_dev[0];
	enable_5v3v = smc->enable_5v3v_level;
	mutex_unlock(&smc_lock);

	return sprintf(buf, "5v3v_pin level = %d\n", enable_5v3v);
}

static ssize_t store_5v3v(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	unsigned int enable_5v3v = 0;
	struct smc_dev *smc = NULL;

	if (kstrtouint(buf, 0, &enable_5v3v))
		return -EINVAL;

	mutex_lock(&smc_lock);
	smc = &smc_dev[0];
	smc->enable_5v3v_level = enable_5v3v;

	if (smc->enable_5v3v_pin != NULL) {
		_gpio_out(smc->enable_5v3v_pin,
			smc->enable_5v3v_level,
			SMC_ENABLE_5V3V_PIN_NAME);
		pr_error("enable_pin: -->(%d)\n",
			(smc->enable_5v3v_level) ? 1 : 0);
	}
	mutex_unlock(&smc_lock);

	return count;
}

static ssize_t show_freq(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	return sprintf(buf, "%dKHz\n", smc_dev[0].param.freq);
}

static ssize_t store_freq(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int freq = 0;

	if (kstrtoint(buf, 0, &freq))
		return -EINVAL;

	if (freq)
		smc_dev[0].param.freq = freq;

	pr_dbg("freq -> %dKHz\n", smc_dev[0].param.freq);
	return count;
}

static ssize_t show_div_smc(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	return sprintf(buf, "div -> %d\n", DIV_SMC);
}

static ssize_t store_div_smc(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int div = 0;

	if (kstrtoint(buf, 0, &div))
		return -EINVAL;

	if (div)
		DIV_SMC = div;

	pr_dbg("div -> %d\n", DIV_SMC);
	return count;
}

#ifdef MEM_DEBUG
static ssize_t show_debug(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	pr_inf("Usage:\n");
	pr_inf("\techo [ 1 | 2 | 0 | dump | reset ] >");
	pr_inf("debug : enable(1/2)|disable|dump|reset\n");
	pr_inf("\t dump file: "DEBUG_FILE_NAME"\n");
	return 0;
}

static ssize_t store_debug(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int smc_debug_level = 0;

	switch (buf[0]) {
	case '2':
		smc_debug_level++;
	case '1': {
		void *p = krealloc((const void *)dbuf, DBUF_SIZE, GFP_KERNEL);

		smc_debug_level++;
		if (p) {
			dbuf = (char *)p;
			smc_debug = smc_debug_level;
		} else {
			pr_error("krealloc(dbuf:%d) failed\n", DBUF_SIZE);
		}
		break;
	}
	case '0':
		smc_debug = 0;
		kfree(dbuf);
		dbuf = NULL;
		break;
	case 'r':
	case 'R':
		if (smc_debug) {
			memset(dbuf, 0, DBUF_SIZE);
			dwrite = 0;
			dread = 0;
			pr_inf("dbuf cleanup\n");
		}
		break;
	case 'd':
	case 'D':
		if (smc_debug) {
			open_debug();
			debug_write(dbuf, dwrite+5);
			close_debug();
			pr_inf("dbuf dump ok\n");
		}
		break;
	default:
		break;
	}
	return count;
}
#endif

static struct class_attribute smc_class_attrs[] = {
	__ATTR(smc_gpio_pull, 0644, show_gpio_pull, set_gpio_pull),
	__ATTR(ctrl_5v3v, 0644, show_5v3v, store_5v3v),
	__ATTR(freq, 0644, show_freq, store_freq),
	__ATTR(div_smc, 0644, show_div_smc, store_div_smc),
#ifdef MEM_DEBUG
	__ATTR(debug, 0644, show_debug, store_debug),
#endif
	__ATTR_NULL
};

static struct class smc_class = {
	.name = SMC_CLASS_NAME,
	.class_attrs = smc_class_attrs,
};



long smc_get_reg_base(void)
{
	int newbase = 0;

	if (get_cpu_type() > MESON_CPU_MAJOR_ID_TXL &&
		get_cpu_type() != MESON_CPU_MAJOR_ID_GXLX) {
		newbase = 1;
	}
	return (newbase) ? 0x9400 : 0x2110;
}

#ifndef CONFIG_OF
static int _gpio_request(unsigned int gpio, const char *owner)
{
	gpio_request(gpio, owner);
	return 0;
}
#endif

static int _gpio_out(struct gpio_desc *gpio, int val, const char *owner)
{
	int ret = 0;

	if (val < 0) {
		pr_dbg("gpio out val = -1.\n");
		return -1;
	}
	if (val != 0)
		val = 1;
	ret = gpiod_direction_output(gpio, val);
	pr_dbg("smc gpio out ret %d\n", ret);
	return ret;
}

#ifdef DET_FROM_PIO
static int _gpio_in(struct gpio_desc *gpio, const char *owner)
{
	int ret = 0;

	ret = gpiod_get_value(gpio);
	return ret;
}
#endif
static int _gpio_free(struct gpio_desc *gpio, const char *owner)
{

	gpiod_put(gpio);
	return 0;
}

static inline int smc_write_end(struct smc_dev *smc)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&smc->slock, flags);
	ret = (!smc->cardin || !smc->send_count);
	spin_unlock_irqrestore(&smc->slock, flags);

	return ret;
}


static inline int smc_can_read(struct smc_dev *smc)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&smc->slock, flags);
	ret = (!smc->cardin || smc->recv_count);
	spin_unlock_irqrestore(&smc->slock, flags);

	return ret;
}

static inline int smc_can_write(struct smc_dev *smc)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&smc->slock, flags);
	ret = (!smc->cardin || (smc->send_count != SEND_BUF_SIZE));
	spin_unlock_irqrestore(&smc->slock, flags);
	return ret;
}

static int smc_hw_set_param(struct smc_dev *smc)
{
	unsigned long v = 0;
	struct SMCCARD_HW_Reg0 *reg0;
	struct SMCCARD_HW_Reg6 *reg6;
	struct SMCCARD_HW_Reg2 *reg2;
	struct SMCCARD_HW_Reg5 *reg5;
	/*
	 * SMC_ANSWER_TO_RST *reg1;
	 * SMC_INTERRUPT_Reg *reg_int;
	 */
	unsigned long freq_cpu = clk_get_rate(aml_smartcard_clk)/1000*DIV_SMC;

	pr_error("hw set param\n");

	v = SMC_READ_REG(REG0);
	reg0 = (struct SMCCARD_HW_Reg0 *)&v;
	reg0->etu_divider = ETU_DIVIDER_CLOCK_HZ * smc->param.f /
	    (smc->param.d * smc->param.freq) - 1;
	SMC_WRITE_REG(REG0, v);
	pr_error("REG0: 0x%08lx\n", v);
	pr_error("f	  :%d\n", smc->param.f);
	pr_error("d	  :%d\n", smc->param.d);
	pr_error("freq	:%d\n", smc->param.freq);

	v = SMC_READ_REG(REG1);
	pr_error("REG1: 0x%08lx\n", v);

	v = SMC_READ_REG(REG2);
	reg2 = (struct SMCCARD_HW_Reg2 *)&v;
	reg2->recv_invert = smc->param.recv_invert;
	reg2->recv_parity = smc->param.recv_parity;
	reg2->recv_lsb_msb = smc->param.recv_lsb_msb;
	reg2->xmit_invert = smc->param.xmit_invert;
	reg2->xmit_parity = smc->param.xmit_parity;
	reg2->xmit_lsb_msb = smc->param.xmit_lsb_msb;
	reg2->xmit_retries = smc->param.xmit_retries;
	reg2->xmit_repeat_dis = smc->param.xmit_repeat_dis;
	reg2->recv_no_parity = smc->param.recv_no_parity;
	reg2->clk_tcnt = freq_cpu/smc->param.freq - 1;
	reg2->det_filter_sel = DET_FILTER_SEL_DEFAULT;
	reg2->io_filter_sel = IO_FILTER_SEL_DEFAULT;
	reg2->clk_sel = clock_source;
	/*reg2->pulse_irq = 0;*/
	SMC_WRITE_REG(REG2, v);
	pr_error("REG2: 0x%08lx\n", v);
	pr_error("recv_inv:%d\n", smc->param.recv_invert);
	pr_error("recv_lsb:%d\n", smc->param.recv_lsb_msb);
	pr_error("recv_par:%d\n", smc->param.recv_parity);
	pr_error("recv_npa:%d\n", smc->param.recv_no_parity);
	pr_error("xmit_inv:%d\n", smc->param.xmit_invert);
	pr_error("xmit_lsb:%d\n", smc->param.xmit_lsb_msb);
	pr_error("xmit_par:%d\n", smc->param.xmit_parity);
	pr_error("xmit_rep:%d\n", smc->param.xmit_repeat_dis);
	pr_error("xmit_try:%d\n", smc->param.xmit_retries);
	pr_error("clk_tcnt:%d freq_cpu:%ld\n", reg2->clk_tcnt, freq_cpu);

	v = SMC_READ_REG(REG5);
	reg5 = (struct SMCCARD_HW_Reg5 *)&v;
	reg5->cwt_detect_en = cwt_det_en;
	reg5->bwt_base_time_gnt = BWT_BASE_DEFAULT;
	SMC_WRITE_REG(REG5, v);
	pr_error("REG5: 0x%08lx\n", v);

	v = SMC_READ_REG(REG6);
	reg6 = (struct SMCCARD_HW_Reg6 *)&v;
	reg6->cwi_value = smc->param.cwi;
	reg6->bwi = smc->param.bwi;
	reg6->bgt = smc->param.bgt-2;
	reg6->N_parameter = smc->param.n;
	SMC_WRITE_REG(REG6, v);
	pr_error("REG6: 0x%08lx\n", v);
	pr_error("N	  :%d\n", smc->param.n);
	pr_error("cwi	 :%d\n", smc->param.cwi);
	pr_error("bgt	 :%d\n", smc->param.bgt);
	pr_error("bwi	 :%d\n", smc->param.bwi);

	return 0;
}

static int smc_default_init(struct smc_dev *smc)
{
	smc->param.f = F_DEFAULT;
	smc->param.d = D_DEFAULT;
	smc->param.freq = FREQ_DEFAULT;
	smc->param.recv_invert = 0;
	smc->param.recv_parity = 0;
	smc->param.recv_lsb_msb = 0;
	smc->param.recv_no_parity = 1;
	smc->param.xmit_invert = 0;
	smc->param.xmit_lsb_msb = 0;
	smc->param.xmit_retries = 1;
	smc->param.xmit_repeat_dis = 1;
	smc->param.xmit_parity = 0;

	/*set reg6 param value */
	smc->param.n = N_DEFAULT;
	smc->param.bwi = BWI_DEFAULT;
	smc->param.cwi = CWI_DEFAULT;
	smc->param.bgt = BGT_DEFAULT;
	return 0;
}

static int smc_hw_setup(struct smc_dev *smc)
{
	unsigned long v = 0;
	struct SMCCARD_HW_Reg0 *reg0;
	struct SMC_ANSWER_TO_RST *reg1;
	struct SMCCARD_HW_Reg2 *reg2;
	struct SMC_INTERRUPT_Reg *reg_int;
	struct SMCCARD_HW_Reg5 *reg5;
	struct SMCCARD_HW_Reg6 *reg6;

	unsigned long freq_cpu = clk_get_rate(aml_smartcard_clk)/1000*DIV_SMC;

	pr_error("SMC CLK SOURCE - %luKHz\n", freq_cpu);

#ifdef RST_FROM_PIO
	_gpio_out(smc->reset_pin, RESET_ENABLE, SMC_RESET_PIN_NAME);
#endif

	v = SMC_READ_REG(REG0);
	reg0 = (struct SMCCARD_HW_Reg0 *)&v;
	reg0->enable = 1;
	reg0->clk_en = 0;
	reg0->clk_oen = 0;
	reg0->card_detect = 0;
	reg0->start_atr = 0;
	reg0->start_atr_en = 0;
	reg0->rst_level = RESET_ENABLE;
	reg0->io_level = 0;
	reg0->recv_fifo_threshold = FIFO_THRESHOLD_DEFAULT;
	reg0->etu_divider = ETU_DIVIDER_CLOCK_HZ * smc->param.f
	    / (smc->param.d*smc->param.freq) - 1;
	reg0->first_etu_offset = 5;
	SMC_WRITE_REG(REG0, v);
	pr_error("REG0: 0x%08lx\n", v);
	pr_error("f	  :%d\n", smc->param.f);
	pr_error("d	  :%d\n", smc->param.d);
	pr_error("freq	:%d\n", smc->param.freq);

	v = SMC_READ_REG(REG1);
	reg1 = (struct SMC_ANSWER_TO_RST *)&v;
	reg1->atr_final_tcnt = ATR_FINAL_TCNT_DEFAULT;
	reg1->atr_holdoff_tcnt = ATR_HOLDOFF_TCNT_DEFAULT;
	reg1->atr_clk_mux = ATR_CLK_MUX_DEFAULT;
	reg1->atr_holdoff_en = atr_holdoff;/*ATR_HOLDOFF_EN;*/
	reg1->etu_clk_sel = ETU_CLK_SEL;
	SMC_WRITE_REG(REG1, v);
	pr_error("REG1: 0x%08lx\n", v);

	v = SMC_READ_REG(REG2);
	reg2 = (struct SMCCARD_HW_Reg2 *)&v;
	reg2->recv_invert = smc->param.recv_invert;
	reg2->recv_parity = smc->param.recv_parity;
	reg2->recv_lsb_msb = smc->param.recv_lsb_msb;
	reg2->xmit_invert = smc->param.xmit_invert;
	reg2->xmit_parity = smc->param.xmit_parity;
	reg2->xmit_lsb_msb = smc->param.xmit_lsb_msb;
	reg2->xmit_retries = smc->param.xmit_retries;
	reg2->xmit_repeat_dis = smc->param.xmit_repeat_dis;
	reg2->recv_no_parity = smc->param.recv_no_parity;
	reg2->clk_tcnt = freq_cpu/smc->param.freq - 1;
	reg2->det_filter_sel = DET_FILTER_SEL_DEFAULT;
	reg2->io_filter_sel = IO_FILTER_SEL_DEFAULT;
	reg2->clk_sel = clock_source;
	/*reg2->pulse_irq = 0;*/
	SMC_WRITE_REG(REG2, v);
	pr_error("REG2: 0x%08lx\n", v);
	pr_error("recv_inv:%d\n", smc->param.recv_invert);
	pr_error("recv_lsb:%d\n", smc->param.recv_lsb_msb);
	pr_error("recv_par:%d\n", smc->param.recv_parity);
	pr_error("recv_npa:%d\n", smc->param.recv_no_parity);
	pr_error("xmit_inv:%d\n", smc->param.xmit_invert);
	pr_error("xmit_lsb:%d\n", smc->param.xmit_lsb_msb);
	pr_error("xmit_par:%d\n", smc->param.xmit_parity);
	pr_error("xmit_rep:%d\n", smc->param.xmit_repeat_dis);
	pr_error("xmit_try:%d\n", smc->param.xmit_retries);
	pr_error("clk_tcnt:%d freq_cpu:%ld\n", reg2->clk_tcnt, freq_cpu);

	v = SMC_READ_REG(INTR);
	reg_int = (struct SMC_INTERRUPT_Reg *)&v;
	reg_int->recv_fifo_bytes_threshold_int_mask = 0;
	reg_int->send_fifo_last_byte_int_mask = 1;
	reg_int->cwt_expeired_int_mask = 1;
	reg_int->bwt_expeired_int_mask = 1;
	reg_int->write_full_fifo_int_mask = 1;
	reg_int->send_and_recv_confilt_int_mask = 1;
	reg_int->recv_error_int_mask = 1;
	reg_int->send_error_int_mask = 1;
	reg_int->rst_expired_int_mask = 1;
	reg_int->card_detect_int_mask = 0;
	SMC_WRITE_REG(INTR, v | 0x03FF);
	pr_error("INTR: 0x%08lx\n", v);

	v = SMC_READ_REG(REG5);
	reg5 = (struct SMCCARD_HW_Reg5 *)&v;
	reg5->cwt_detect_en = cwt_det_en;
	reg5->btw_detect_en = btw_det_en;
	reg5->etu_msr_en = etu_msr_en;
	reg5->bwt_base_time_gnt = BWT_BASE_DEFAULT;
	SMC_WRITE_REG(REG5, v);
	pr_error("REG5: 0x%08lx\n", v);


	v = SMC_READ_REG(REG6);
	reg6 = (struct SMCCARD_HW_Reg6 *)&v;
	reg6->N_parameter = smc->param.n;
	reg6->cwi_value = smc->param.cwi;
	reg6->bgt = smc->param.bgt-2;
	reg6->bwi = smc->param.bwi;
	SMC_WRITE_REG(REG6, v);
	pr_error("REG6: 0x%08lx\n", v);
	pr_error("N	  :%d\n", smc->param.n);
	pr_error("cwi	 :%d\n", smc->param.cwi);
	pr_error("bgt	 :%d\n", smc->param.bgt);
	pr_error("bwi	 :%d\n", smc->param.bwi);
	return 0;
}

static void enable_smc_clk(struct smc_dev *smc)
{
	unsigned int _value;

	if ((smc->pin_clk_pinmux_reg == -1)
			|| (smc->pin_clk_pinmux_bit == -1))
		return;
	_value = READ_CBUS_REG(smc->pin_clk_pinmux_reg);
	_value |= smc->pin_clk_pinmux_bit;
	WRITE_CBUS_REG(smc->pin_clk_pinmux_reg, _value);
}

static void disable_smc_clk(struct smc_dev *smc)
{
	unsigned int _value;

	if ((smc->pin_clk_pinmux_reg == -1)
			|| (smc->pin_clk_pinmux_bit == -1))
		return;

	_value = READ_CBUS_REG(smc->pin_clk_pinmux_reg);
	_value &= ~smc->pin_clk_pinmux_bit;
	WRITE_CBUS_REG(smc->pin_clk_pinmux_reg, _value);
	_value = READ_CBUS_REG(smc->pin_clk_pinmux_reg);

	/*pr_dbg("disable smc_clk: mux[%x]\n", _value);*/

	if ((smc->pin_clk_oen_reg != -1)
			&& (smc->pin_clk_out_reg != -1)
			&& (smc->pin_clk_oebit != -1)
			&& (smc->pin_clk_oubit != -1)) {

		/*force the clk pin to low.*/
		_value = READ_CBUS_REG(smc->pin_clk_oen_reg);
		_value &= ~smc->pin_clk_oebit;
		WRITE_CBUS_REG(smc->pin_clk_oen_reg, _value);
		_value = READ_CBUS_REG(smc->pin_clk_out_reg);
		_value &= ~smc->pin_clk_oubit;
		WRITE_CBUS_REG(smc->pin_clk_out_reg, _value);
		pr_dbg("disable smc_clk: pin[%x](reg)\n", _value);
	} else if (smc->pin_clk_pin != NULL) {

		udelay(20);
		/*	_gpio_out(smc->pin_clk_pin,
		 *	0,
		 *	SMC_CLK_PIN_NAME);
		 */
		udelay(1000);

		/*pr_dbg("disable smc_clk: pin[%x](pin)\n", smc->pin_clk_pin);*/
	} else {

		pr_error("no reg/bit or pin");
		pr_error("defined for clk-pin contrl.\n");
	}
}

static int smc_hw_active(struct smc_dev *smc)
{
	if (ENA_GPIO_PULL > 0) {
		enable_smc_clk(smc);
		udelay(200);
	}
	if (!smc->active) {

		if (smc->reset) {
			smc->reset(NULL, 0);
			pr_dbg("call reset(0) in bsp.\n");
		} else {
			if (smc->use_enable_pin) {
				_gpio_out(smc->enable_pin,
					smc->enable_level,
					SMC_ENABLE_PIN_NAME);
			}
		}

		udelay(200);
		smc_hw_setup(smc);

		smc->active = 1;
	}

	return 0;
}

static int smc_hw_deactive(struct smc_dev *smc)
{
	if (smc->active) {
		unsigned long sc_reg0 = SMC_READ_REG(REG0);
		struct SMCCARD_HW_Reg0 *sc_reg0_reg = (void *)&sc_reg0;

		sc_reg0_reg->rst_level = RESET_ENABLE;
		sc_reg0_reg->enable = 1;
		sc_reg0_reg->start_atr = 0;
		sc_reg0_reg->start_atr_en = 0;
		sc_reg0_reg->clk_en = 0;
		SMC_WRITE_REG(REG0, sc_reg0);
#ifdef RST_FROM_PIO
		/*_gpio_out(smc->reset_pin, RESET_ENABLE, SMC_RESET_PIN_NAME);*/
		_gpio_out(smc->reset_pin, RESET_DISABLE, SMC_RESET_PIN_NAME);
#endif
		udelay(200);

		if (smc->reset) {
			smc->reset(NULL, 1);
			pr_dbg("call reset(1) in bsp.\n");
		} else {
			if (smc->use_enable_pin)
				_gpio_out(smc->enable_pin,
					smc->enable_level,
					SMC_ENABLE_PIN_NAME);
		}
		if (ENA_GPIO_PULL > 0) {
			disable_smc_clk(smc);
			/*smc_pull_down_data();*/
		}

		smc->active = 0;
	}

	return 0;
}

#define INV(a) ((smc->sc_type == SC_INVERSE) ? inv_table[(int)(a)] : (a))

#ifndef ATR_FROM_INT
static int smc_hw_get(struct smc_dev *smc, int cnt, int timeout)
{
	unsigned long sc_status;
	int times = timeout*100;
	struct SMC_STATUS_Reg *sc_status_reg =
	    (struct SMC_STATUS_Reg *)&sc_status;

	while ((times > 0) && (cnt > 0)) {
		sc_status = SMC_READ_REG(STATUS);

		/*pr_dbg("read atr status %08x\n", sc_status);*/

		if (sc_status_reg->rst_expired_status)
			pr_error("atr timeout\n");

		if (sc_status_reg->cwt_expeired_status) {
			pr_error("cwt timeout when get atr,");
			pr_error("but maybe it is natural!\n");
		}

		if (sc_status_reg->recv_fifo_empty_status) {
			udelay(10);
			times--;
		} else {
			while (sc_status_reg->recv_fifo_bytes_number > 0) {
				u8 byte = (SMC_READ_REG(FIFO))&0xff;

#ifdef SW_INVERT
				if (smc->sc_type == SC_INVERSE)
					byte = inv_table[byte];
#endif

				smc->atr.atr[smc->atr.atr_len++] = byte;
				sc_status_reg->recv_fifo_bytes_number--;
				cnt--;
				if (cnt == 0) {
					pr_dbg("read atr bytes ok\n");
					return 0;
				}
			}
		}
	}

	pr_error("read atr failed\n");
	return -1;
}

#else

static int smc_fiq_get(struct smc_dev *smc, int size, int timeout)
{
	int ret = 0;
	int times = timeout/10;
	int start, end;

	if (!times)
		times = 1;

	while ((times > 0) && (size > 0)) {

		start = smc->recv_start;
		end = smc->recv_end;/*momentary value*/

		if (!smc->cardin) {
			ret = -ENODEV;
		} else if (start == end) {
			ret = -EAGAIN;
		} else {
			int i;
			/*ATR only, no loop*/
			ret = end - start;
			if (ret > size)
				ret = size;
			memcpy(&smc->atr.atr[smc->atr.atr_len],
				&smc->recv_buf[start], ret);
			for (i = smc->atr.atr_len;
				i < smc->atr.atr_len+ret;
				i++)
				smc->atr.atr[i] = INV((int)smc->atr.atr[i]);
			smc->atr.atr_len += ret;

			smc->recv_start += ret;
			size - = ret;
		}

		if (ret < 0) {
			msleep(20);
			times--;
		}
	}

	if (size > 0)
		ret = -ETIMEDOUT;

	return ret;
}
#endif /*ifndef ATR_FROM_INT*/

static int smc_hw_read_atr(struct smc_dev *smc)
{
	char *ptr = smc->atr.atr;
	int his_len, t, tnext = 0, only_t0 = 1, loop_cnt = 0;
	int i;

	pr_dbg("read atr\n");

#ifdef ATR_FROM_INT
#define smc_hw_get smc_fiq_get
#endif

	smc->atr.atr_len = 0;
	if (smc_hw_get(smc, 2, 2000) < 0)
		goto end;

#ifdef SW_INVERT
	if (ptr[0] == 0x03) {
		smc->sc_type = SC_INVERSE;
		ptr[0] = inv_table[(int)ptr[0]];
		if (smc->atr.atr_len > 1)
			ptr[1] = inv_table[(int)ptr[1]];
	} else if (ptr[0] == 0x3F) {
		smc->sc_type = SC_INVERSE;
		if (smc->atr.atr_len > 1)
			ptr[1] = inv_table[(int)ptr[1]];
	} else if (ptr[0] == 0x3B) {
		smc->sc_type = SC_DIRECT;
	} else if (ptr[0] == 0x23) {
		smc->sc_type = SC_DIRECT;
		ptr[0] = inv_table[(int)ptr[0]];
		if (smc->atr.atr_len > 1)
			ptr[1] = inv_table[(int)ptr[1]];
	}
#endif /*SW_INVERT*/

	ptr++;
	his_len = ptr[0]&0x0F;

	do {
		tnext = 0;
		loop_cnt++;
		if (ptr[0]&0x10) {
			if (smc_hw_get(smc, 1, 1000) < 0)
				goto end;
		}
		if (ptr[0]&0x20) {
			if (smc_hw_get(smc, 1, 1000) < 0)
				goto end;
		}
		if (ptr[0]&0x40) {
			if (smc_hw_get(smc, 1, 1000) < 0)
				goto end;
		}
		if (ptr[0]&0x80) {
			if (smc_hw_get(smc, 1, 1000) < 0)
				goto end;

			ptr = &smc->atr.atr[smc->atr.atr_len-1];
			t = ptr[0]&0x0F;
			if (t)
				only_t0 = 0;
			if (ptr[0]&0xF0)
				tnext = 1;
		}
	} while (tnext && loop_cnt < 4);

	if (!only_t0)
		his_len++;
	smc_hw_get(smc, his_len, 2000);

	pr_dbg("get atr len:%d data: ", smc->atr.atr_len);
	for (i = 0; i < smc->atr.atr_len; i++)
		pr_dbg("%02x ", smc->atr.atr[i]);
	pr_dbg("\n");

#ifdef ATR_OUT_READ
	if (smc->atr.atr_len) {
		pr_dbg("reset recv_start %d->0\n", smc->recv_start);
		smc->recv_start = 0;
		if (smc->sc_type == SC_INVERSE) {
			int i;

			for (i = 0; i < smc->atr.atr_len; i++)
				smc->recv_buf[smc->recv_start+i] =
				    smc->atr.atr[i];
		}
	}
#endif
	return 0;

end:
	pr_error("read atr failed\n");
	return -EIO;
#ifdef ATR_FROM_INT
#undef smc_hw_get
#endif
}


void smc_reset_prepare(struct smc_dev *smc)
{
	/*reset recv&send buf*/
	smc->send_start = 0;
	smc->send_count = 0;
	smc->recv_start = 0;
	smc->recv_count = 0;

	/*Read ATR*/
	smc->atr.atr_len = 0;
	smc->recv_count = 0;
	smc->send_count = 0;

	smc->recv_end = 0;
	smc->send_end = 0;

#ifdef SW_INVERT
	smc->sc_type = SC_DIRECT;
	smc->atr_mode = 1;
#endif
}

static int smc_hw_reset(struct smc_dev *smc)
{
	unsigned long flags;
	int ret;
	unsigned long sc_reg0 = SMC_READ_REG(REG0);
	struct SMCCARD_HW_Reg0 *sc_reg0_reg = (void *)&sc_reg0;
	unsigned long sc_int;
	struct SMC_INTERRUPT_Reg *sc_int_reg = (void *)&sc_int;

	pr_dbg("smc read reg0 0x%lx\n", sc_reg0);

	spin_lock_irqsave(&smc->slock, flags);
	if (smc->cardin)
		ret = 0;
	else
		ret = -ENODEV;
	spin_unlock_irqrestore(&smc->slock, flags);

	if (ret >= 0) {
		/*Reset*/
#ifdef NO_HOT_RESET
		smc->active = 0;
#endif
		if (smc->active) {

			smc_reset_prepare(smc);

			sc_reg0_reg->rst_level = RESET_ENABLE;
			sc_reg0_reg->clk_en = 1;
			sc_reg0_reg->etu_divider = ETU_DIVIDER_CLOCK_HZ *
			    smc->param.f / (smc->param.d*smc->param.freq) - 1;
			SMC_WRITE_REG(REG0, sc_reg0);
#ifdef RST_FROM_PIO
			_gpio_out(smc->reset_pin,
				RESET_ENABLE,
				SMC_RESET_PIN_NAME);
#endif

			udelay(800/smc->param.freq); /*>= 400/f ;*/

			/*disable receive interrupt*/
			sc_int = SMC_READ_REG(INTR);
			sc_int_reg->recv_fifo_bytes_threshold_int_mask = 0;
			SMC_WRITE_REG(INTR, sc_int|0x3FF);

			sc_reg0_reg->rst_level = RESET_DISABLE;
			sc_reg0_reg->start_atr = 1;
			SMC_WRITE_REG(REG0, sc_reg0);
#ifdef RST_FROM_PIO
			_gpio_out(smc->reset_pin,
				RESET_DISABLE,
				SMC_RESET_PIN_NAME);
#endif
		} else {
			smc_hw_deactive(smc);

			udelay(200);

			smc_hw_active(smc);

			smc_reset_prepare(smc);

			sc_reg0_reg->clk_en = 1;
			sc_reg0_reg->enable = 0;
			sc_reg0_reg->rst_level = RESET_ENABLE;
			SMC_WRITE_REG(REG0, sc_reg0);
#ifdef RST_FROM_PIO
			if (smc->use_enable_pin) {
				_gpio_out(smc->enable_pin,
					smc->enable_level,
					SMC_RESET_PIN_NAME);
				udelay(100);
			}
			_gpio_out(smc->reset_pin,
				RESET_ENABLE,
				SMC_RESET_PIN_NAME);
#endif
			udelay(2000); /*>= 400/f ;*/

			/*disable receive interrupt*/
			sc_int = SMC_READ_REG(INTR);
			sc_int_reg->recv_fifo_bytes_threshold_int_mask = 0;
			SMC_WRITE_REG(INTR, sc_int|0x3FF);

			sc_reg0_reg->rst_level = RESET_DISABLE;
			sc_reg0_reg->start_atr_en = 1;
			sc_reg0_reg->start_atr = 1;
			sc_reg0_reg->enable = 1;
			sc_reg0_reg->etu_divider = ETU_DIVIDER_CLOCK_HZ *
			    smc->param.f / (smc->param.d*smc->param.freq) - 1;
			SMC_WRITE_REG(REG0, sc_reg0);
#ifdef RST_FROM_PIO

			_gpio_out(smc->reset_pin,
				RESET_DISABLE, SMC_RESET_PIN_NAME);
#endif
		}

#if defined(ATR_FROM_INT)
		/*enable receive interrupt*/
		sc_int = SMC_READ_REG(INTR);
		sc_int_reg->recv_fifo_bytes_threshold_int_mask = 1;
		SMC_WRITE_REG(INTR, sc_int|0x3FF);
#endif

		/*msleep(atr_delay);*/
		ret = smc_hw_read_atr(smc);

#ifdef SW_INVERT
		smc->atr_mode = 0;
#endif

#if defined(ATR_FROM_INT)
		/*disable receive interrupt*/
		sc_int = SMC_READ_REG(INTR);
		sc_int_reg->recv_fifo_bytes_threshold_int_mask = 0;
		SMC_WRITE_REG(INTR, sc_int|0x3FF);
#endif

		/*Disable ATR*/
		sc_reg0 = SMC_READ_REG(REG0);
		sc_reg0_reg->start_atr_en = 0;
		sc_reg0_reg->start_atr = 0;
		SMC_WRITE_REG(REG0, sc_reg0);

#ifndef DISABLE_RECV_INT
		sc_int_reg->recv_fifo_bytes_threshold_int_mask = 1;
#endif
		SMC_WRITE_REG(INTR, sc_int|0x3FF);

	}

	return ret;
}

static int smc_hw_get_status(struct smc_dev *smc, int *sret)
{
	unsigned long flags;
#ifndef DET_FROM_PIO
	unsigned int reg_val;
	struct SMCCARD_HW_Reg0 *reg = (struct SMCCARD_HW_Reg0 *)&reg_val;
#endif
	spin_lock_irqsave(&smc->slock, flags);

#ifdef DET_FROM_PIO
	smc->cardin = _gpio_in(smc->detect_pin, OWNER_NAME);
	pr_dbg("get_status: card detect: %d\n", smc->cardin);
#else
	reg_val = SMC_READ_REG(REG0);
	smc->cardin = reg->card_detect;
	/* pr_error("get_status: smc reg0 %08x,
	 * card detect: %d\n", reg_val, reg->card_detect);
	 */
#endif
	/* pr_dbg("det:%d, det_invert:%d\n",
	 * smc->cardin, smc->detect_invert);
	 */
	if (smc->detect_invert)
		smc->cardin = !smc->cardin;

	*sret = smc->cardin;

	spin_unlock_irqrestore(&smc->slock, flags);

	return 0;
}


static inline void _atomic_wrap_inc(int *p, int wrap)
{
	int i = *p;

	i++;
	if (i >= wrap)
		i = 0;
	*p = i;
}

static inline void _atomic_wrap_add(int *p, int add, int wrap)
{
	int i = *p;

	i += add;
	if (i >= wrap)
		i %= wrap;
	*p = i;
}

static inline int smc_can_recv_max(struct smc_dev *smc)
{
	int start = smc->recv_start;
	int end = smc->recv_end;

	if (end >= start)
		return RECV_BUF_SIZE - end + start;
	else
		return start - end;
}

static int smc_hw_start_send(struct smc_dev *smc)
{
	unsigned int sc_status;
	struct SMC_STATUS_Reg *sc_status_reg =
		(struct SMC_STATUS_Reg *)&sc_status;
	u8 byte;

	/*trigger only*/
	sc_status = SMC_READ_REG(STATUS);
	if (smc->send_end != smc->send_start &&
		!sc_status_reg->send_fifo_full_status) {
		pr_dbg("s i f [%d:%d]\n", smc->send_start, smc->send_end);
		byte = smc->send_buf[smc->send_end];
		_atomic_wrap_inc(&smc->send_end, SEND_BUF_SIZE);
#ifdef SW_INVERT
		if (smc->sc_type == SC_INVERSE)
			byte = inv_table[byte];
#endif
		SMC_WRITE_REG(FIFO, byte);
		pr_dbg("send 1st byte to hw\n");
	}

	return 0;
}

#ifdef SMC_FIQ
static irqreturn_t smc_bridge_isr(int irq, void *dev_id)
{
	struct smc_dev *smc = (struct smc_dev *)dev_id;

#ifdef DET_FROM_PIO
	smc->cardin = _gpio_in(smc->detect_pin, SMC_DETECT_PIN_NAME);
	if (smc->detect_invert)
		smc->cardin = !smc->cardin;
#endif

	if (smc->recv_start != smc->recv_end)
		wake_up_interruptible(&smc->rd_wq);
	if (smc->send_start == smc->send_end)
		wake_up_interruptible(&smc->wr_wq);

	return IRQ_HANDLED;
}

static void smc_irq_handler(void)
{
	struct smc_dev *smc = &smc_dev[0];
	unsigned int sc_status;
	unsigned int sc_reg0;
	unsigned int sc_int;
	struct SMC_STATUS_Reg *sc_status_reg =
		(struct SMC_STATUS_Reg *)&sc_status;
	struct SMC_INTERRUPT_Reg *sc_int_reg =
		(struct SMC_INTERRUPT_Reg *)&sc_int;
	struct SMCCARD_HW_Reg0 *sc_reg0_reg =
		(void *)&sc_reg0;

	sc_int = SMC_READ_REG(INTR);
	/*Fpr("smc intr:0x%x\n", sc_int);*/

	if (sc_int_reg->recv_fifo_bytes_threshold_int) {

		int num = 0;

		sc_status = SMC_READ_REG(STATUS);
		num = sc_status_reg->recv_fifo_bytes_number;

		if (num > smc_can_recv_max(smc)) {
			pr_error("receive buffer overflow\n");
		} else {
			u8 byte;

			while (sc_status_reg->recv_fifo_bytes_number) {
				byte = SMC_READ_REG(FIFO);
#ifdef SW_INVERT
				if (!smc->atr_mode &&
					smc->sc_type == SC_INVERSE)
					byte = inv_table[byte];
#endif
				smc->recv_buf[smc->recv_end] = byte;
				_atomic_wrap_inc(&smc->recv_end, RECV_BUF_SIZE);
				num++;
				sc_status = SMC_READ_REG(STATUS);
				Fpr("F%02x ", byte);
			}
			Fpr("Fr > %d bytes\n", num);

			fiq_bridge_pulse_trigger(&smc->smc_fiq_bridge);
		}

		sc_int_reg->recv_fifo_bytes_threshold_int = 0;

	}

	if (sc_int_reg->send_fifo_last_byte_int) {
		int start = smc->send_start;
		int cnt = 0;
		u8 byte;

		while (1) {
			sc_status = SMC_READ_REG(STATUS);
			if (smc->send_end == start ||
				sc_status_reg->send_fifo_full_status)
				break;

			byte = smc->send_buf[smc->send_end];
			Fpr("Fs > %02x ", byte);
			_atomic_wrap_inc(&smc->send_end, SEND_BUF_SIZE);
#ifdef SW_INVERT
			if (smc->sc_type == SC_INVERSE)
				byte = inv_table[byte];
#endif
			SMC_WRITE_REG(FIFO, byte);
			cnt++;
		}

		Fpr("Fs > %d bytes\n", cnt);

		if (smc->send_end == start) {
			sc_int_reg->send_fifo_last_byte_int_mask = 0;
			sc_int_reg->recv_fifo_bytes_threshold_int_mask = 1;
			fiq_bridge_pulse_trigger(&smc->smc_fiq_bridge);
		}

		sc_int_reg->send_fifo_last_byte_int = 0;

	}

	SMC_WRITE_REG(INTR, sc_int|0x3FF);

#ifndef DET_FROM_PIO
	sc_reg0 = SMC_READ_REG(REG0);
	smc->cardin = sc_reg0_reg->card_detect;
	if (smc->detect_invert)
		smc->cardin = !smc->cardin;
#endif
}

#else

static int transmit_chars(struct smc_dev *smc)
{
	unsigned int status;
	struct SMC_STATUS_Reg *status_r = (struct SMC_STATUS_Reg *)&status;
	int cnt = 0;
	u8 byte;
	int start = smc->send_start;

	while (1) {
		status = SMC_READ_REG(STATUS);
		if (smc->send_end == start || status_r->send_fifo_full_status)
			break;

		byte = smc->send_buf[smc->send_end];
		Ipr("s > %02x\n", byte);
		_atomic_wrap_inc(&smc->send_end, SEND_BUF_SIZE);
#ifdef SW_INVERT
		if (smc->sc_type == SC_INVERSE)
			byte = inv_table[byte];
#endif
		SMC_WRITE_REG(FIFO, byte);
		cnt++;
	}

	Ipr("s > %d bytes\n", cnt);
	return cnt;
}

static int receive_chars(struct smc_dev *smc)
{
	unsigned int status;
	unsigned int intr;
	struct SMC_STATUS_Reg *status_r = (struct SMC_STATUS_Reg *)&status;
	int cnt = 0;
	u8 byte;

	status = SMC_READ_REG(STATUS);
	if (status_r->recv_fifo_empty_status > smc_can_recv_max(smc)) {
		pr_error("receive buffer overflow\n");
		return -1;
	}

	/*clear recv_fifo_bytes_threshold_int_mask or INT lost*/
	intr = SMC_READ_REG(INTR);
	SMC_WRITE_REG(INTR, (intr & ~0x103ff));

	status = SMC_READ_REG(STATUS);
	while (!status_r->recv_fifo_empty_status) {
		byte = SMC_READ_REG(FIFO);
#ifdef SW_INVERT
		if (!smc->atr_mode && smc->sc_type == SC_INVERSE)
			byte = inv_table[byte];
#endif
		smc->recv_buf[smc->recv_end] = byte;
		_atomic_wrap_inc(&smc->recv_end, RECV_BUF_SIZE);
		cnt++;
		status = SMC_READ_REG(STATUS);
		Ipr("%02x ", byte);
	}
	Ipr("r > %d bytes\n", cnt);

	return cnt;
}

static void smc_irq_bh_handler(unsigned long arg)
{
	struct smc_dev *smc = (struct smc_dev *)arg;
#ifndef DET_FROM_PIO
	unsigned int sc_reg0;
	struct SMCCARD_HW_Reg0 *sc_reg0_reg = (void *)&sc_reg0;
#endif

	/*Read card status*/
#ifndef DET_FROM_PIO
	sc_reg0 = SMC_READ_REG(REG0);
	smc->cardin = sc_reg0_reg->card_detect;
#else
	smc->cardin = _gpio_in(smc->detect_pin, SMC_DETECT_PIN_NAME);
#endif
	if (smc->detect_invert)
		smc->cardin = !smc->cardin;

	if (smc->recv_start != smc->recv_end)
		wake_up_interruptible(&smc->rd_wq);
	if (smc->send_start == smc->send_end)
		wake_up_interruptible(&smc->wr_wq);
}


static irqreturn_t smc_irq_handler(int irq, void *data)
{
	struct smc_dev *smc = (struct smc_dev *)data;
	unsigned int sc_status;
	unsigned int sc_int;
	struct SMC_STATUS_Reg *sc_status_reg =
		(struct SMC_STATUS_Reg *)&sc_status;
	struct SMC_INTERRUPT_Reg *sc_int_reg =
		(struct SMC_INTERRUPT_Reg *)&sc_int;

	sc_int = SMC_READ_REG(INTR);
	Ipr("Int:0x%x\n", sc_int);
	sc_status = SMC_READ_REG(STATUS);
	Ipr("Sta:0x%x\n", sc_status);

	/*Receive*/
	sc_status = SMC_READ_REG(STATUS);
	if (!sc_status_reg->recv_fifo_empty_status)
		receive_chars(smc);

	/* Send */
	sc_status = SMC_READ_REG(STATUS);
	if (!sc_status_reg->send_fifo_full_status) {
		transmit_chars(smc);
		if (smc->send_end == smc->send_start) {
			sc_int_reg->send_fifo_last_byte_int_mask = 0;
			sc_int_reg->recv_fifo_bytes_threshold_int_mask = 1;
		}
	}

	SMC_WRITE_REG(INTR, sc_int|0x3FF);

	tasklet_schedule(&smc->tasklet);

	return IRQ_HANDLED;
}
#endif /*ifdef SMC_FIQ*/

static void smc_dev_deinit(struct smc_dev *smc)
{
	if (smc->irq_num != -1) {
		free_irq(smc->irq_num, &smc);
		smc->irq_num = -1;
		tasklet_kill(&smc->tasklet);
	}
	if (smc->use_enable_pin)
		_gpio_free(smc->enable_pin, SMC_ENABLE_PIN_NAME);
	clk_disable_unprepare(aml_smartcard_clk);
#if 0
	if (smc->pin_clk_pin != -1)
		_gpio_free(smc->pin_clk_pin, SMC_CLK_PIN_NAME);
#endif
#ifdef DET_FROM_PIO
	if (smc->detect_pin != NULL)
		_gpio_free(smc->detect_pin, SMC_DETECT_PIN_NAME);
#endif
#ifdef RST_FROM_PIO
	if (smc->reset_pin != NULL)
		_gpio_free(smc->reset_pin, SMC_RESET_PIN_NAME);
#endif
#ifdef CONFIG_OF
	if (smc->pinctrl)
		devm_pinctrl_put(smc->pinctrl);
#endif
	if (smc->dev)
		device_destroy(&smc_class, MKDEV(smc_major, smc->id));

	mutex_destroy(&smc->lock);

	smc->init = 0;

#if defined(MESON_CPU_TYPE_MESON8) && (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8)
	CLK_GATE_OFF(SMART_CARD_MPEG_DOMAIN);
#endif

}

static int _set_gpio(struct smc_dev *smc, struct gpio_desc **gpiod,
	char *str, int input_output, int output_level)
{
	int ret = 0;
	/*pr_dbg("smc _set_gpio %s %p\n", str, *gpiod);*/
	if (IS_ERR(*gpiod)) {
		pr_dbg("smc %s request failed\n", str);
		return -1;
	}
	if (input_output == OUTPUT) {
		*gpiod = gpiod_get(&smc->pdev->dev, str,
				output_level ? GPIOD_OUT_HIGH : GPIOD_OUT_LOW);
		ret = gpiod_direction_output(*gpiod, output_level);
	} else if (input_output == INPUT)	{
		*gpiod = gpiod_get(&smc->pdev->dev, str, GPIOD_IN);
		ret = gpiod_direction_input(*gpiod);
		ret |= gpiod_set_pull(*gpiod, GPIOD_PULL_UP);
	} else
		pr_dbg("SMC Request gpio direction invalid\n");

	return ret;
}
static int smc_dev_init(struct smc_dev *smc, int id)
{
#ifndef CONFIG_OF
	struct resource *res;
#else
	int ret;
	u32 value;
	char buf[32];
	const char *dts_str;
	struct resource *res;
#endif

#if defined(MESON_CPU_TYPE_MESON8) && (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8)
	CLK_GATE_ON(SMART_CARD_MPEG_DOMAIN);
#error
#endif
	/*of_match_node(smc_dt_match, smc->pdev->dev.of_node);*/
	smc->id = id;

#ifdef CONFIG_OF
	smc->pinctrl = devm_pinctrl_get_select_default(&smc->pdev->dev);
	if (IS_ERR(smc->pinctrl))
		return -1;

	ret = of_property_read_string(smc->pdev->dev.of_node,
		"smc_need_enable_pin", &dts_str);
	if (ret < 0) {
		pr_error("failed to get smartcard node.\n");
		return -EINVAL;
	}
	if (strcmp(dts_str, "yes") == 0)
		smc->use_enable_pin = 1;
	else
		smc->use_enable_pin = 0;
	if (smc->use_enable_pin == 1) {
		smc->enable_pin = NULL;
		if (smc->enable_pin == NULL) {
			snprintf(buf, sizeof(buf), "smc%d_enable_pin", id);
			_set_gpio(smc, &smc->enable_pin, "enable_pin",
				OUTPUT, OUTLEVEL_HIGH);
#else /*CONFIG_OF*/
			res = platform_get_resource_byname(smc->pdev,
				IORESOURCE_MEM, buf);
			if (!res)
				pr_error("cannot get resource \"%s\"\n", buf);
			else {
				smc->enable_pin = res->start;
				_gpio_request(smc->enable_pin,
					SMC_ENABLE_PIN_NAME);
			}
#endif /*CONFIG_OF*/
		}

		if (smc->use_enable_pin) {
			snprintf(buf, sizeof(buf), "smc%d_enable_level", id);
#ifdef CONFIG_OF
			ret = of_property_read_u32(smc->pdev->dev.of_node,
				buf, &value);
			if (!ret) {
				smc->enable_level = value;
				pr_error("%s: %d\n", buf, smc->enable_level);
				if (smc->enable_pin != NULL) {
					_gpio_out(smc->enable_pin,
						smc->enable_level,
						SMC_ENABLE_PIN_NAME);
					pr_error("enable_pin: -->(%d)\n",
						(!smc->enable_level)?1:0);
				}
			} else {
				pr_error("cannot find resource \"%s\"\n", buf);
			}
#else /*CONFIG_OF*/
			res = platform_get_resource_byname(smc->pdev,
				IORESOURCE_MEM, buf);
			if (!res)
				pr_error("cannot get resource \"%s\"\n", buf);
			else
				smc->enable_level = res->start;
#endif /*CONFIG_OF*/
		}
	} else
		pr_dbg("Smartcard is working with no enable pin\n");

#ifdef CONFIG_OF
	smc->reset_pin = NULL;
	ret = _set_gpio(smc, &smc->reset_pin, "reset_pin",
		OUTPUT, OUTLEVEL_HIGH);
	if (ret) {
		pr_dbg("smc reset pin request failed, we can not work now\n");
		return -1;
	}
	ret = of_property_read_u32(smc->pdev->dev.of_node,
		"reset_level", &value);
	smc->reset_level = value;
	pr_dbg("smc reset_level %d\n", value);

#else /*CONFIG_OF*/
	res = platform_get_resource_byname(smc->pdev, IORESOURCE_MEM, buf);
	if (!res)
		pr_error("cannot get resource \"%s\"\n", buf);
	else
		smc->reset_level = res->start;
#endif /*CONFIG_OF*/
	smc->irq_num = smc0_irq;
	if (smc->irq_num == -1) {
		snprintf(buf, sizeof(buf), "smc%d_irq", id);
#if 0
		ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
		if (!ret) {
			smc->irq_num = value;
			pr_error("%s: %d\n", buf, smc->irq_num);
		} else {
			pr_error("cannot find resource \"%s\"\n", buf);
			return -1;
		}
#else /*CONFIG_OF*/
		res = platform_get_resource_byname(smc->pdev,
			IORESOURCE_IRQ, buf);
		if (!res) {
			pr_error("cannot get resource \"%s\"\n", buf);
			return -1;
		}
		smc->irq_num = res->start;
#endif /*CONFIG_OF*/
	}
	smc->pin_clk_pinmux_reg = -1;
	if (smc->pin_clk_pinmux_reg == -1) {
		snprintf(buf, sizeof(buf), "smc%d_clk_pinmux_reg", id);
#ifdef CONFIG_OF
		ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
		if (!ret) {
			smc->pin_clk_pinmux_reg = value;
			pr_error("%s: 0x%x\n", buf, smc->pin_clk_pinmux_reg);
		} else
			pr_error("cannot find resource \"%s\"\n", buf);
#else /*CONFIG_OF*/
		res = platform_get_resource_byname(smc->pdev,
			IORESOURCE_MEM, buf);
		if (!res)
			pr_error("cannot get resource \"%s\"\n", buf);
		else
			smc->pin_clk_pinmux_reg = res->start;
#endif /*CONFIG_OF*/
	}
#if 1
	smc->pin_clk_pinmux_bit = -1;
	if (smc->pin_clk_pinmux_bit == -1) {
		snprintf(buf, sizeof(buf), "smc%d_clk_pinmux_bit", id);
#ifdef CONFIG_OF
		ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
		if (!ret) {
			smc->pin_clk_pinmux_bit = value;
			pr_error("%s: 0x%x\n", buf, smc->pin_clk_pinmux_bit);
		} else
			pr_error("cannot find resource \"%s\"\n", buf);
		/*TODO:*/
#else /*CONFIG_OF*/
		res = platform_get_resource_byname(smc->pdev,
			IORESOURCE_MEM, buf);
		if (!res)
			pr_error("cannot get resource \"%s\"\n", buf);
		else
			smc->pin_clk_pinmux_bit = res->start;
#endif /*CONFIG_OF*/
	}
#endif
	smc->pin_clk_oen_reg = -1;
	if (smc->pin_clk_oen_reg == -1) {
		snprintf(buf, sizeof(buf), "smc%d_clk_oen_reg", id);
#ifdef CONFIG_OF
		ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
		if (!ret) {
			smc->pin_clk_oen_reg = value;
			pr_error("%s: 0x%x\n", buf, smc->pin_clk_oen_reg);
		} else
			pr_error("cannot find resource \"%s\"\n", buf);
#else /*CONFIG_OF*/
		res = platform_get_resource_byname(smc->pdev,
			IORESOURCE_MEM, buf);
		if (!res)
			pr_error("cannot get resource \"%s\"\n", buf);
		else {
			smc->pin_clk_oen_reg = res->start;
#endif /*CONFIG_OF*/
	}

	smc->pin_clk_out_reg = -1;
	if (smc->pin_clk_out_reg == -1) {
		snprintf(buf, sizeof(buf), "smc%d_clk_out_reg", id);
#ifdef CONFIG_OF
		ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
		if (!ret) {
			smc->pin_clk_out_reg = value;
			pr_error("%s: 0x%x\n", buf, smc->pin_clk_out_reg);
		} else {
			pr_error("cannot find resource \"%s\"\n", buf);
		}
#else /*CONFIG_OF*/
		res = platform_get_resource_byname(smc->pdev,
			IORESOURCE_MEM, buf);
		if (!res)
			pr_error("cannot get resource \"%s\"\n", buf);
		else
			smc->pin_clk_out_reg = res->start;
#endif /*CONFIG_OF*/
	}

	smc->pin_clk_oebit = -1;
	if (smc->pin_clk_oebit == -1) {
		snprintf(buf, sizeof(buf), "smc%d_clk_oebit", id);
#ifdef CONFIG_OF
		ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
		if (!ret) {
			smc->pin_clk_oebit = value;
			pr_error("%s: 0x%x\n", buf, smc->pin_clk_oebit);
		} else {
			pr_error("cannot find resource \"%s\"\n", buf);
		}
#else /*CONFIG_OF*/
		res = platform_get_resource_byname(smc->pdev,
			IORESOURCE_MEM, buf);
		if (!res)
			pr_error("cannot get resource \"%s\"\n", buf);
		else
			smc->pin_clk_oebit = res->start;
#endif /*CONFIG_OF*/
	}
	smc->pin_clk_oubit = -1;
	if (smc->pin_clk_oubit == -1) {
		snprintf(buf, sizeof(buf), "smc%d_clk_oubit", id);
#ifdef CONFIG_OF
		ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
		if (!ret) {
			smc->pin_clk_oubit = value;
			pr_error("%s: 0x%x\n", buf, smc->pin_clk_oubit);
		} else {
			pr_error("cannot find resource \"%s\"\n", buf);
		}
#else /*CONFIG_OF*/
		res = platform_get_resource_byname(smc->pdev,
			IORESOURCE_MEM, buf);
		if (!res)
			pr_error("cannot get resource \"%s\"\n", buf);
		else
			smc->pin_clk_oubit = res->start;
#endif /*CONFIG_OF*/
	}

#ifdef DET_FROM_PIO
	smc->detect_pin = NULL;
	if (smc->detect_pin == NULL) {
#ifdef CONFIG_OF
		ret = _set_gpio(smc, &smc->detect_pin, "detect_pin", INPUT, 0);
		if (ret) {
			pr_dbg("smc detect_pin request failed, we can not work\n");
			return -1;
		}
#endif
#else /*CONFIG_OF*/
		ret = platform_get_resource_byname(smc->pdev,
			IORESOURCE_MEM, buf);
		if (!ret) {
			pr_error("cannot get resource \"%s\"\n", buf);
		} else {
			smc->detect_pin = res->start;
			_gpio_request(smc->detect_pin, SMC_DETECT_PIN_NAME);
		}
#endif /*CONFIG_OF*/
	}
	if (1) {
		snprintf(buf, sizeof(buf), "smc%d_clock_source", id);
#ifdef CONFIG_OF
		ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
		if (!ret) {
			clock_source = value;
			pr_error("%s: %d\n", buf, clock_source);
		} else {
			pr_error("cannot find resource \"%s\"\n", buf);
			pr_error("using clock source default: %d\n",
				clock_source);
		}
#else /*CONFIG_OF*/
		res = platform_get_resource_byname(smc->pdev,
			IORESOURCE_MEM, buf);
		if (!res) {
			pr_error("cannot get resource \"%s\"\n", buf);
			pr_error("using clock source default: %d\n",
				clock_source);
		} else {
			clock_source = res->start;
		}
#endif /*CONFIG_OF*/
	}
	smc->detect_invert = 0;
	if (1) {
		snprintf(buf, sizeof(buf), "smc%d_det_invert", id);
#ifdef CONFIG_OF
		ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
		if (!ret) {
			smc->detect_invert = value;
			pr_error("%s: %d\n", buf, smc->detect_invert);
		} else {
			pr_error("cannot find resource \"%s\"\n", buf);
		}
#else /*CONFIG_OF*/
		res = platform_get_resource_byname(smc->pdev,
			IORESOURCE_MEM, buf);
		if (!res)
			pr_error("cannot get resource \"%s\"\n", buf);
		else
			smc->detect_invert = res->start;
#endif /*CONFIG_OF*/
	}
#if 1
	smc->enable_5v3v_pin = NULL;
	if (smc->enable_5v3v_pin == NULL) {
		snprintf(buf, sizeof(buf), "smc%d_5v3v_pin", id);
#ifdef CONFIG_OF
		ret = _set_gpio(smc, &smc->enable_5v3v_pin,
			"enable_5v3v_pin", OUTPUT, OUTLEVEL_HIGH);
		if (ret == -1)
			pr_dbg("smc 5v3v_pin is not working, we might face some problems\n");
#else /*CONFIG_OF*/
		res = platform_get_resource_byname(smc->pdev,
			IORESOURCE_MEM, buf);
		if (!res) {
			pr_error("cannot get resource \"%s\"\n", buf);
		} else {
			smc->enable_5v3v_pin = res->start;
			_gpio_request(smc->enable_5v3v_pin,
				SMC_ENABLE_5V3V_PIN_NAME);
		}
#endif /*CONFIG_OF*/
	}
#endif

#if 1
	smc->enable_5v3v_level = 0;
	if (1) {
		snprintf(buf, sizeof(buf), "smc%d_5v3v_level", id);
#ifdef CONFIG_OF
		ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
		if (!ret) {
			smc->enable_5v3v_level = value;
			pr_error("%s: %d\n", buf, smc->enable_5v3v_level);
			if (smc->enable_5v3v_pin != NULL) {
				_gpio_out(smc->enable_5v3v_pin,
					smc->enable_5v3v_level,
					SMC_ENABLE_5V3V_PIN_NAME);
				pr_error("5v3v_pin: -->(%d)\n",
					(smc->enable_5v3v_level) ? 1 : 0);
			}
		} else {
			pr_error("cannot find resource \"%s\"\n", buf);
		}
#else /*CONFIG_OF*/
		res = platform_get_resource_byname(smc->pdev,
			IORESOURCE_MEM, buf);
		if (!res)
			pr_error("cannot get resource \"%s\"\n", buf);
		else
			smc->enable_5v3v_level = res->start;
#endif /*CONFIG_OF*/
	}
#endif

	init_waitqueue_head(&smc->rd_wq);
	init_waitqueue_head(&smc->wr_wq);

	spin_lock_init(&smc->slock);
	mutex_init(&smc->lock);

#ifdef SMC_FIQ
	{
		int r = -1;

		smc->smc_fiq_bridge.handle = smc_bridge_isr;
		smc->smc_fiq_bridge.key = (u32)smc;
		smc->smc_fiq_bridge.name = "smc_bridge_isr";
		r = register_fiq_bridge_handle(&smc->smc_fiq_bridge);
		if (r) {
			pr_error("smc fiq bridge register error.\n");
			return -1;
		}
	}
	request_fiq(smc->irq_num, &smc_irq_handler);
#else
	smc->irq_num = request_irq(smc->irq_num,
			(irq_handler_t)smc_irq_handler,
			IRQF_SHARED|IRQF_TRIGGER_RISING, "smc", smc);
	if (smc->irq_num < 0) {
		pr_error("request irq error!\n");
		smc_dev_deinit(smc);
		return -1;
	}

	tasklet_init(&smc->tasklet, smc_irq_bh_handler, (unsigned long)smc);
#endif
	snprintf(buf, sizeof(buf), "smc%d", smc->id);
	smc->dev = device_create(&smc_class,
		NULL, MKDEV(smc_major, smc->id), smc, buf);
	if (!smc->dev) {
		pr_error("create device error!\n");
		smc_dev_deinit(smc);
		return -1;
	}

	smc_default_init(smc);

	smc->init = 1;

	smc_hw_setup(smc);

	return 0;
}

static int smc_open(struct inode *inode, struct file *filp)
{
	int id = iminor(inode);
	struct smc_dev *smc = NULL;

	id = 0;
	smc = &smc_dev[id];
	mutex_init(&smc->lock);
	mutex_lock(&smc->lock);

#ifdef FILE_DEBUG
	open_debug();
#endif

	if (smc->used) {
		mutex_unlock(&smc->lock);
		pr_error("smartcard %d already openned!", id);
		return -EBUSY;
	}

	smc->used = 1;

#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6
	switch_mod_gate_by_name("smart_card", 1);
#endif

	mutex_unlock(&smc->lock);

	filp->private_data = smc;

	return 0;
}

static int smc_close(struct inode *inode, struct file *filp)
{
	struct smc_dev *smc = (struct smc_dev *)filp->private_data;

	mutex_lock(&smc->lock);

	smc_hw_deactive(smc);

#ifndef KEEP_PARAM_AFTER_CLOSE
	smc_default_init(smc);
#endif

	smc->used = 0;

#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6
	switch_mod_gate_by_name("smart_card", 0);
#endif

#ifdef FILE_DEBUG
	close_debug();
#endif

	mutex_unlock(&smc->lock);

	return 0;
}

static ssize_t smc_read(struct file *filp,
	char __user *buff, size_t size, loff_t *ppos)
{
	struct smc_dev *smc = (struct smc_dev *)filp->private_data;
	unsigned long flags;
	int ret;
	int start = 0, end;

	ret = mutex_lock_interruptible(&smc->lock);
	if (ret)
		return ret;

	spin_lock_irqsave(&smc->slock, flags);
	if (ret == 0) {

		start = smc->recv_start;
		end = smc->recv_end;

		if (!smc->cardin) {
			ret = -ENODEV;
		} else if (start == end) {
			ret = -EAGAIN;
		} else {
			ret = (end > start) ? (end-start) :
			    (RECV_BUF_SIZE-start+end);
			if (ret > size)
				ret = size;
		}
	}

	if (ret > 0) {
		int cnt = RECV_BUF_SIZE-start;
		long cr;

		pr_dbg("read %d bytes\n", ret);
		if (cnt >= ret) {
			cr = copy_to_user(buff, smc->recv_buf+start, ret);
		} else {
			int cnt1 = ret-cnt;

			cr = copy_to_user(buff, smc->recv_buf+start, cnt);
			cr = copy_to_user(buff+cnt, smc->recv_buf, cnt1);
		}
		_atomic_wrap_add(&smc->recv_start, ret, RECV_BUF_SIZE);
	}
	spin_unlock_irqrestore(&smc->slock, flags);

	mutex_unlock(&smc->lock);

	return ret;
}

static ssize_t smc_write(struct file *filp,
	const char __user *buff, size_t size, loff_t *offp)
{
	struct smc_dev *smc = (struct smc_dev *)filp->private_data;
	unsigned long flags;
	int ret;
	unsigned long sc_int;
	struct SMC_INTERRUPT_Reg *sc_int_reg = (void *)&sc_int;
	int start = 0, end;

	ret = mutex_lock_interruptible(&smc->lock);
	if (ret)
		return ret;

	spin_lock_irqsave(&smc->slock, flags);

	if (ret == 0) {

		start = smc->send_start;
		end = smc->send_end;

		if (!smc->cardin) {
			ret = -ENODEV;
		} else if (start != end) {
			ret = -EAGAIN;
		} else {
			ret = size;
			if (ret >= SEND_BUF_SIZE)
				ret = SEND_BUF_SIZE-1;
		}
	}

	if (ret > 0) {
		int cnt = SEND_BUF_SIZE-start;
		long cr;

		if (cnt >= ret) {
			cr = copy_from_user(smc->send_buf+start, buff, ret);
		} else {
			int cnt1 = ret-cnt;

			cr = copy_from_user(smc->send_buf+start, buff, cnt);
			cr = copy_from_user(smc->send_buf, buff+cnt, cnt1);
		}
		_atomic_wrap_add(&smc->send_start, ret, SEND_BUF_SIZE);
	}

	spin_unlock_irqrestore(&smc->slock, flags);

	if (ret > 0) {
		sc_int = SMC_READ_REG(INTR);
#ifdef DISABLE_RECV_INT
		sc_int_reg->recv_fifo_bytes_threshold_int_mask = 0;
#endif
		sc_int_reg->send_fifo_last_byte_int_mask = 1;
		SMC_WRITE_REG(INTR, sc_int|0x3FF);

		pr_dbg("write %d bytes\n", ret);

		smc_hw_start_send(smc);
	}

	mutex_unlock(&smc->lock);

	return ret;
}

static unsigned int smc_poll(struct file *filp,
	struct poll_table_struct *wait)
{
	struct smc_dev *smc = (struct smc_dev *)filp->private_data;
	unsigned int ret = 0;
	unsigned long flags;

	poll_wait(filp, &smc->rd_wq, wait);
	poll_wait(filp, &smc->wr_wq, wait);

	spin_lock_irqsave(&smc->slock, flags);

	if (smc->recv_start != smc->recv_end)
		ret |= POLLIN|POLLRDNORM;
	if (smc->send_start == smc->send_end)
		ret |= POLLOUT|POLLWRNORM;
	if (!smc->cardin)
		ret |= POLLERR;

	spin_unlock_irqrestore(&smc->slock, flags);

	return ret;
}

static long smc_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	struct smc_dev *smc = (struct smc_dev *)file->private_data;
	int ret = 0;
	long cr;

	switch (cmd) {
	case AMSMC_IOC_RESET:
	{
		ret = mutex_lock_interruptible(&smc->lock);
		if (ret)
			return ret;
		ret = smc_hw_reset(smc);
		if (ret >= 0)
			cr = copy_to_user((void *)arg, &smc->atr,
				sizeof(struct am_smc_atr));
		mutex_unlock(&smc->lock);
	}
	break;
	case AMSMC_IOC_GET_STATUS:
	{
		int status;

		smc_hw_get_status(smc, &status);
		cr = copy_to_user((void *)arg, &status, sizeof(int));
	}
	break;
	case AMSMC_IOC_ACTIVE:
	{
		ret = mutex_lock_interruptible(&smc->lock);
		if (ret)
			return ret;

		ret = smc_hw_active(smc);

		mutex_unlock(&smc->lock);
	}
	break;
	case AMSMC_IOC_DEACTIVE:
	{
		ret = mutex_lock_interruptible(&smc->lock);
		if (ret)
			return ret;

		ret = smc_hw_deactive(smc);

		mutex_unlock(&smc->lock);
	}
	break;
	case AMSMC_IOC_GET_PARAM:
	{
		ret = mutex_lock_interruptible(&smc->lock);
		if (ret)
			return ret;
		cr = copy_to_user((void *)arg, &smc->param,
			sizeof(struct am_smc_param));

		mutex_unlock(&smc->lock);
	}
	break;
	case AMSMC_IOC_SET_PARAM:
	{
		ret = mutex_lock_interruptible(&smc->lock);
		if (ret)
			return ret;

		cr = copy_from_user(&smc->param, (void *)arg,
			sizeof(struct am_smc_param));
		ret = smc_hw_set_param(smc);

		mutex_unlock(&smc->lock);
	}
	break;
	default:
		ret = -EINVAL;
	break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long smc_ioctl_compat(struct file *filp,
		unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = smc_ioctl(filp, cmd, args);
	return ret;
}
#endif


static const struct file_operations smc_fops = {
	.owner = THIS_MODULE,
	.open = smc_open,
	.write = smc_write,
	.read = smc_read,
	.release = smc_close,
	.unlocked_ioctl = smc_ioctl,
	.poll = smc_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl  = smc_ioctl_compat,
#endif
};

static int smc_probe(struct platform_device *pdev)
{
	struct smc_dev *smc = NULL;
	int i, ret;

	mutex_lock(&smc_lock);

	for (i = 0; i < SMC_DEV_COUNT; i++) {
		if (!smc_dev[i].init) {
			smc = &smc_dev[i];
			break;
		}
	}
	aml_smartcard_clk =
		devm_clk_get(&pdev->dev, "smartcard");
	if (IS_ERR_OR_NULL(aml_smartcard_clk)) {
		dev_err(&pdev->dev, "get smartcard clk fail\n");
		return -1;
	}
	clk_prepare_enable(aml_smartcard_clk);
	if (smc) {
		smc->init = 1;
		smc->pdev = pdev;
		dev_set_drvdata(&pdev->dev, smc);
		ret = smc_dev_init(smc, i);
		if (ret < 0)
			smc = NULL;
	}

	mutex_unlock(&smc_lock);

	return smc ? 0 : -1;
}

static int smc_remove(struct platform_device *pdev)
{
	struct smc_dev *smc = (struct smc_dev *)dev_get_drvdata(&pdev->dev);

	mutex_lock(&smc_lock);

	smc_dev_deinit(smc);

	mutex_unlock(&smc_lock);

	return 0;
}

static struct platform_driver smc_driver = {
	.probe		 = smc_probe,
	.remove		 = smc_remove,
	.driver		 = {
		.name	 = "amlogic-smc",
		.owner	 = THIS_MODULE,
		.of_match_table = smc_dt_match,
	},
};

static int __init smc_mod_init(void)
{
	int ret = -1;

	mutex_init(&smc_lock);
	smc_major = register_chrdev(0, SMC_DEV_NAME, &smc_fops);
	if (smc_major <= 0) {
		mutex_destroy(&smc_lock);
		pr_error("register chrdev error\n");
		goto error_register_chrdev;
	}

	if (class_register(&smc_class) < 0) {
		pr_error("register class error\n");
		goto error_class_register;
	}

	if (platform_driver_register(&smc_driver) < 0) {
		pr_error("register platform driver error\n");
		goto error_platform_drv_register;
	}
	return 0;
error_platform_drv_register:
	class_unregister(&smc_class);
error_class_register:
	unregister_chrdev(smc_major, SMC_DEV_NAME);
error_register_chrdev:
	mutex_destroy(&smc_lock);
	return ret;
}

static void __exit smc_mod_exit(void)
{
	platform_driver_unregister(&smc_driver);
	class_unregister(&smc_class);
	unregister_chrdev(smc_major, SMC_DEV_NAME);
	mutex_destroy(&smc_lock);
}

module_init(smc_mod_init);

module_exit(smc_mod_exit);

MODULE_AUTHOR("AMLOGIC");

MODULE_DESCRIPTION("AMLOGIC smart card driver");

MODULE_LICENSE("GPL");
