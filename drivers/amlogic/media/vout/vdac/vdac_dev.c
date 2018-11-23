/*
 * drivers/amlogic/media/vout/vdac/vdac_dev.c
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

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/vdac_dev.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include <linux/mutex.h>

#define AMVDAC_NAME               "amvdac"
#define AMVDAC_DRIVER_NAME        "amvdac"
#define AMVDAC_MODULE_NAME        "amvdac"
#define AMVDAC_DEVICE_NAME        "amvdac"
#define AMVDAC_CLASS_NAME         "amvdac"

struct amvdac_dev_s {
	dev_t                       devt;
	struct cdev                 cdev;
	dev_t                       devno;
	struct device               *dev;
	struct class                *clsp;
};
static struct amvdac_dev_s amvdac_dev;
static struct meson_vdac_data *s_vdac_data;
static struct mutex vdac_mutex;

#define HHI_VDAC_CNTL0 0xbd
#define HHI_VDAC_CNTL1 0xbe
#define HHI_VDAC_CNTL0_G12A 0xbb
#define HHI_VDAC_CNTL1_G12A 0xbc

#define VDAC_MODULE_ATV_DEMOD 0x1
#define VDAC_MODULE_DTV_DEMOD 0x2
#define VDAC_MODULE_TVAFE     0x4
#define VDAC_MODULE_CVBS_OUT  0x8
#define VDAC_MODULE_AUDIO_OUT  0x10

static bool vdac_init_succ_flag;

/* priority for module,
 * module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; cvbsout:0x8
 */
static unsigned int pri_flag;

static unsigned int vdac_cntl0_bit9;
module_param(vdac_cntl0_bit9, uint, 0644);
MODULE_PARM_DESC(vdac_cntl0_bit9, "vdac_cntl0_bit9");

static unsigned int vdac_cntl1_bit3;
module_param(vdac_cntl1_bit3, uint, 0644);
MODULE_PARM_DESC(vdac_cntl1_bit3, "vdac_cntl1_bit3");

static unsigned int vdac_cntl0_bit0;
module_param(vdac_cntl0_bit0, uint, 0644);
MODULE_PARM_DESC(vdac_cntl0_bit0, "vdac_cntl0_bit0");

static unsigned int vdac_cntl0_bit10;
module_param(vdac_cntl0_bit10, uint, 0644);
MODULE_PARM_DESC(vdac_cntl0_bit10, "vdac_cntl0_bit10");

static inline uint32_t vdac_hiu_reg_read(uint32_t reg)
{
	return aml_read_hiubus(reg);
}

static inline void vdac_hiu_reg_write(uint32_t reg, uint32_t val)
{
	return aml_write_hiubus(reg, val);
}

static inline void vdac_hiu_reg_setb(uint32_t reg,
	    const uint32_t value,
	    const uint32_t start,
	    const uint32_t len)
{
	aml_write_hiubus(reg, ((aml_read_hiubus(reg) &
			~(((1L << (len)) - 1) << (start))) |
			(((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t vdac_hiu_reg_getb(uint32_t reg,
	    const uint32_t start,
	    const uint32_t len)
{
	uint32_t val;

	val = ((vdac_hiu_reg_read(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

/* adc/dac ref signal,
 * module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; dac:0x8
 */
void ana_ref_cntl0_bit9(bool on, unsigned int module_sel)
{
	bool enable = 0;

	/*tl1:bandgap en, bc[7] default:0 opened*/
	if (s_vdac_data->cpu_id == VDAC_CPU_TL1)
		return;

	switch (module_sel & 0x1f) {
	case VDAC_MODULE_ATV_DEMOD: /* dtv demod */
		if (on)
			vdac_cntl0_bit9 |= VDAC_MODULE_ATV_DEMOD;
		else
			vdac_cntl0_bit9 &= ~VDAC_MODULE_ATV_DEMOD;
		break;
	case VDAC_MODULE_DTV_DEMOD: /* atv demod */
		if (on)
			vdac_cntl0_bit9 |= VDAC_MODULE_DTV_DEMOD;
		else
			vdac_cntl0_bit9 &= ~VDAC_MODULE_DTV_DEMOD;
		break;
	case VDAC_MODULE_TVAFE: /* cvbs in demod */
		if (on)
			vdac_cntl0_bit9 |= VDAC_MODULE_TVAFE;
		else
			vdac_cntl0_bit9 &= ~VDAC_MODULE_TVAFE;
		break;
	case VDAC_MODULE_CVBS_OUT: /* cvbs in demod */
		if (on)
			vdac_cntl0_bit9 |= VDAC_MODULE_CVBS_OUT;
		else
			vdac_cntl0_bit9 &= ~VDAC_MODULE_CVBS_OUT;
		break;
	case VDAC_MODULE_AUDIO_OUT: /* audio out ctrl*/
		if (s_vdac_data->cpu_id == VDAC_CPU_TXL ||
			s_vdac_data->cpu_id == VDAC_CPU_TXLX) {
			if (on)
				vdac_cntl0_bit9 |= VDAC_MODULE_AUDIO_OUT;
			else
				vdac_cntl0_bit9 &= ~VDAC_MODULE_AUDIO_OUT;
		}
		break;
	default:
		pr_err("module_sel: 0x%x wrong module index !! ", module_sel);
		break;
	}
	/* pr_info("\nvdac_cntl0_bit9: 0x%x\n", vdac_cntl0_bit9); */

	if ((vdac_cntl0_bit9 & 0x1f) == 0)
		enable = 0;
	else
		enable = 1;

	if (s_vdac_data->cpu_id == VDAC_CPU_TXL ||
			s_vdac_data->cpu_id == VDAC_CPU_TXLX)
		vdac_hiu_reg_setb(HHI_VDAC_CNTL0, enable, 9, 1);
	else if (s_vdac_data->cpu_id >= VDAC_CPU_G12AB)
		vdac_hiu_reg_setb(HHI_VDAC_CNTL0_G12A, ~enable, 9, 1);
	else
		vdac_hiu_reg_setb(HHI_VDAC_CNTL0, ~enable, 9, 1);
}
EXPORT_SYMBOL(ana_ref_cntl0_bit9);

/*avin cvbsout signal,
 * module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; dac:0x8
 */
void vdac_out_cntl0_bit10(bool on, unsigned int module_sel)
{
	bool enable = 0;

	/*bit10 is for bandgap startup setting in g12a*/
	if (s_vdac_data->cpu_id >= VDAC_CPU_G12AB)
		return;

	switch (module_sel & 0xf) {
	case VDAC_MODULE_ATV_DEMOD: /* dtv demod */
		if (on)
			vdac_cntl0_bit10 |= VDAC_MODULE_ATV_DEMOD;
		else
			vdac_cntl0_bit10 &= ~VDAC_MODULE_ATV_DEMOD;
		break;
	case VDAC_MODULE_DTV_DEMOD: /* atv demod */
		if (on)
			vdac_cntl0_bit10 |= VDAC_MODULE_DTV_DEMOD;
		else
			vdac_cntl0_bit10 &= ~VDAC_MODULE_DTV_DEMOD;
		break;
	case VDAC_MODULE_TVAFE: /* av in demod */
		if (on)
			vdac_cntl0_bit10 |= VDAC_MODULE_TVAFE;
		else
			vdac_cntl0_bit10 &= ~VDAC_MODULE_TVAFE;
		break;
	case VDAC_MODULE_CVBS_OUT: /* cvbs out demod */
		if (on)
			vdac_cntl0_bit10 |= VDAC_MODULE_CVBS_OUT;
		else
			vdac_cntl0_bit10 &= ~VDAC_MODULE_CVBS_OUT;
		break;
	default:
		pr_err("module_sel: 0x%x wrong module index !! ", module_sel);
		break;
	}
	/* pr_info("\vdac_cntl0_bit0: 0x%x\n", vdac_cntl1_bit3); */

	if ((vdac_cntl0_bit10 & 0xf) == 0)
		enable = 0;
	else
		enable = 1;

	vdac_hiu_reg_setb(HHI_VDAC_CNTL0, enable, 10, 1);
}
EXPORT_SYMBOL(vdac_out_cntl0_bit10);

/*atv cvbsout signal,
 * module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; dac:0x8
 */
void vdac_out_cntl0_bit0(bool on, unsigned int module_sel)
{
	bool enable = 0;

	switch (module_sel & 0xf) {
	case VDAC_MODULE_ATV_DEMOD: /* dtv demod */
		if (on)
			vdac_cntl0_bit0 |= VDAC_MODULE_ATV_DEMOD;
		else
			vdac_cntl0_bit0 &= ~VDAC_MODULE_ATV_DEMOD;
		break;
	case VDAC_MODULE_DTV_DEMOD: /* atv demod */
		if (on)
			vdac_cntl0_bit0 |= VDAC_MODULE_DTV_DEMOD;
		else
			vdac_cntl0_bit0 &= ~VDAC_MODULE_DTV_DEMOD;
		break;
	case VDAC_MODULE_TVAFE: /* av in demod */
		if (on)
			vdac_cntl0_bit0 |= VDAC_MODULE_TVAFE;
		else
			vdac_cntl0_bit0 &= ~VDAC_MODULE_TVAFE;
		break;
	case VDAC_MODULE_CVBS_OUT: /* cvbs in demod */
		if (on)
			vdac_cntl0_bit0 |= VDAC_MODULE_CVBS_OUT;
		else
			vdac_cntl0_bit0 &= ~VDAC_MODULE_CVBS_OUT;
		break;
	default:
		pr_err("module_sel: 0x%x wrong module index !! ", module_sel);
		break;
	}
	/* pr_info("\vdac_cntl0_bit0: 0x%x\n", vdac_cntl1_bit3); */

	if ((vdac_cntl0_bit0 & 0xf) == 0)
		enable = 0;
	else
		enable = 1;

	if (s_vdac_data->cpu_id >= VDAC_CPU_G12AB)
		vdac_hiu_reg_setb(HHI_VDAC_CNTL0_G12A, enable, 0, 1);
	else
		vdac_hiu_reg_setb(HHI_VDAC_CNTL0, enable, 0, 1);
}
EXPORT_SYMBOL(vdac_out_cntl0_bit0);

/* dac out pwd ctl,
 * module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; dac:0x8
 */
void vdac_out_cntl1_bit3(bool on, unsigned int module_sel)
{
	bool enable = 0;

	switch (module_sel & 0xf) {
	case VDAC_MODULE_ATV_DEMOD: /* atv demod */
		if (on)
			vdac_cntl1_bit3 |= VDAC_MODULE_ATV_DEMOD;
		else
			vdac_cntl1_bit3 &= ~VDAC_MODULE_ATV_DEMOD;
		break;
	case VDAC_MODULE_DTV_DEMOD: /* dtv demod */
		if (on)
			vdac_cntl1_bit3 |= VDAC_MODULE_DTV_DEMOD;
		else
			vdac_cntl1_bit3 &= ~VDAC_MODULE_DTV_DEMOD;
		break;
	case VDAC_MODULE_TVAFE: /* av in demod */
		if (on)
			vdac_cntl1_bit3 |= VDAC_MODULE_TVAFE;
		else
			vdac_cntl1_bit3 &= ~VDAC_MODULE_TVAFE;
		break;
	case VDAC_MODULE_CVBS_OUT: /* cvbs in demod */
		if (on)
			vdac_cntl1_bit3 |= VDAC_MODULE_CVBS_OUT;
		else
			vdac_cntl1_bit3 &= ~VDAC_MODULE_CVBS_OUT;
		break;
	default:
		pr_err("module_sel: 0x%x wrong module index !! ", module_sel);
		break;
	}
	/* pr_info("\nvdac_cntl1_bit3: 0x%x\n", vdac_cntl1_bit3); */

	if ((vdac_cntl1_bit3 & 0xf) == 0)
		enable = 0;
	else
		enable = 1;

	if (s_vdac_data->cpu_id == VDAC_CPU_TXL ||
			s_vdac_data->cpu_id == VDAC_CPU_TXLX)
		vdac_hiu_reg_setb(HHI_VDAC_CNTL1, enable, 3, 1);
	else if (s_vdac_data->cpu_id >= VDAC_CPU_G12AB)
		vdac_hiu_reg_setb(HHI_VDAC_CNTL1_G12A, ~enable, 3, 1);
	else
		vdac_hiu_reg_setb(HHI_VDAC_CNTL1, ~enable, 3, 1);
}
EXPORT_SYMBOL(vdac_out_cntl1_bit3);

void vdac_set_ctrl0_ctrl1(unsigned int ctrl0, unsigned int ctrl1)
{
	if (!s_vdac_data) {
		pr_err("\n%s: s_vdac_data NULL\n", __func__);
		return;
	}
	if (s_vdac_data->cpu_id >= VDAC_CPU_G12AB) {
		vdac_hiu_reg_write(HHI_VDAC_CNTL0_G12A, ctrl0);
		vdac_hiu_reg_write(HHI_VDAC_CNTL1_G12A, ctrl1);
	} else {
		vdac_hiu_reg_write(HHI_VDAC_CNTL0, ctrl0);
		vdac_hiu_reg_write(HHI_VDAC_CNTL1, ctrl1);
	}
}
EXPORT_SYMBOL(vdac_set_ctrl0_ctrl1);

/* dac ctl,
 * module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; dac:0x8
 */
void vdac_enable(bool on, unsigned int module_sel)
{
	if (!s_vdac_data) {
		pr_err("\n%s: s_vdac_data NULL\n", __func__);
		return;
	}

	pr_info("\n%s: on:%d,module_sel:%x\n", __func__, on, module_sel);

	mutex_lock(&vdac_mutex);
	switch (module_sel) {
	case VDAC_MODULE_ATV_DEMOD: /* atv demod */
		if ((on && (pri_flag & VDAC_MODULE_ATV_DEMOD))
			|| (!on && !(pri_flag & VDAC_MODULE_ATV_DEMOD))) {
			pr_info("%s: ATV DEMOD had done!:%d.\n", __func__, on);
			break;
		}

		if (on) {
			ana_ref_cntl0_bit9(1, VDAC_MODULE_ATV_DEMOD);
			/*after txlx need reset bandgap after bit9 enabled*/
			/*bit10 reset bandgap in g12a*/
			if (s_vdac_data->cpu_id == VDAC_CPU_TXLX) {
				vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 1, 13, 1);
				udelay(5);
				vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 0, 13, 1);
			}
			pri_flag &= ~VDAC_MODULE_TVAFE;
			pri_flag |= VDAC_MODULE_ATV_DEMOD;
			if (pri_flag & VDAC_MODULE_CVBS_OUT)
				break;
			/*Cdac pwd*/
			vdac_out_cntl1_bit3(1, VDAC_MODULE_ATV_DEMOD);
			/* enable AFE output buffer */
			if (s_vdac_data->cpu_id < VDAC_CPU_G12AB)
				vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 0, 10, 1);
			vdac_out_cntl0_bit0(1, VDAC_MODULE_ATV_DEMOD);
		} else {
			ana_ref_cntl0_bit9(0, VDAC_MODULE_ATV_DEMOD);
			pri_flag &= ~VDAC_MODULE_ATV_DEMOD;
			if (pri_flag & VDAC_MODULE_CVBS_OUT)
				break;
			vdac_out_cntl0_bit0(0, VDAC_MODULE_ATV_DEMOD);
			/* Disable AFE output buffer */
			if (s_vdac_data->cpu_id < VDAC_CPU_G12AB)
				vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 0, 10, 1);
			/* enable dac output */
			vdac_out_cntl1_bit3(0, VDAC_MODULE_ATV_DEMOD);
		}
		break;
	case VDAC_MODULE_DTV_DEMOD: /* dtv demod */
		if (on) {
			if (s_vdac_data->cpu_id == VDAC_CPU_GXLX)
				vdac_out_cntl1_bit3(1, VDAC_MODULE_DTV_DEMOD);
			ana_ref_cntl0_bit9(1, VDAC_MODULE_DTV_DEMOD);
			if (s_vdac_data->cpu_id == VDAC_CPU_TXLX) {
				vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 1, 13, 1);
				udelay(5);
				vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 0, 13, 1);
			}
			pri_flag |= VDAC_MODULE_DTV_DEMOD;
		} else {
			ana_ref_cntl0_bit9(0, VDAC_MODULE_DTV_DEMOD);
			if (s_vdac_data->cpu_id == VDAC_CPU_GXLX)
				vdac_out_cntl1_bit3(0, VDAC_MODULE_DTV_DEMOD);
			pri_flag &= ~VDAC_MODULE_DTV_DEMOD;
		}
		break;
	case VDAC_MODULE_TVAFE: /* av in demod */
		if (on) {
			ana_ref_cntl0_bit9(1, VDAC_MODULE_TVAFE);
			/*after txlx need reset bandgap after bit9 enabled*/
			if (s_vdac_data->cpu_id == VDAC_CPU_TXLX) {
				vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 1, 13, 1);
				udelay(5);
				vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 0, 13, 1);
			}
			pri_flag &= ~VDAC_MODULE_ATV_DEMOD;
			pri_flag |= VDAC_MODULE_TVAFE;
			if (pri_flag & VDAC_MODULE_CVBS_OUT)
				break;
			vdac_out_cntl1_bit3(0, VDAC_MODULE_TVAFE);
			vdac_out_cntl0_bit10(1, VDAC_MODULE_TVAFE);
			if (s_vdac_data->cpu_id == VDAC_CPU_TL1) {
				/*[6][8]bypass buffer enable*/
				vdac_hiu_reg_setb(HHI_VDAC_CNTL1_G12A, 1, 6, 1);
				vdac_hiu_reg_setb(HHI_VDAC_CNTL1_G12A, 1, 8, 1);
			}
		} else {
			ana_ref_cntl0_bit9(0, VDAC_MODULE_TVAFE);
			if (s_vdac_data->cpu_id == VDAC_CPU_TL1) {
				/*[6][8]bypass buffer disable*/
				vdac_hiu_reg_setb(HHI_VDAC_CNTL1_G12A, 0, 6, 1);
				vdac_hiu_reg_setb(HHI_VDAC_CNTL1_G12A, 0, 8, 1);
			}
			pri_flag &= ~VDAC_MODULE_TVAFE;
			if (pri_flag & VDAC_MODULE_CVBS_OUT)
				break;
			/* Disable AFE output buffer */
			vdac_out_cntl0_bit10(0, VDAC_MODULE_TVAFE);
			/* enable dac output */
			vdac_out_cntl1_bit3(0, VDAC_MODULE_TVAFE);
		}
		break;
	case VDAC_MODULE_CVBS_OUT: /* cvbs in demod */
		if (on) {
			vdac_out_cntl1_bit3(1, VDAC_MODULE_CVBS_OUT);
			vdac_out_cntl0_bit0(1, VDAC_MODULE_CVBS_OUT);
			ana_ref_cntl0_bit9(1, VDAC_MODULE_CVBS_OUT);
			if (s_vdac_data->cpu_id == VDAC_CPU_TXLX) {
				vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 1, 13, 1);
				udelay(5);
				vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 0, 13, 1);
			}
			vdac_out_cntl0_bit10(0, VDAC_MODULE_CVBS_OUT);
			pri_flag |= VDAC_MODULE_CVBS_OUT;
		} else {
			/*vdac_out_cntl0_bit10(0, VDAC_MODULE_CVBS_OUT);*/
			ana_ref_cntl0_bit9(0, VDAC_MODULE_CVBS_OUT);
			vdac_out_cntl0_bit0(0, VDAC_MODULE_CVBS_OUT);
			vdac_out_cntl1_bit3(0, VDAC_MODULE_CVBS_OUT);
			pri_flag &= ~VDAC_MODULE_CVBS_OUT;
			if (pri_flag & VDAC_MODULE_ATV_DEMOD) {
				vdac_out_cntl1_bit3(1, VDAC_MODULE_ATV_DEMOD);
				if (s_vdac_data->cpu_id < VDAC_CPU_G12AB)
					vdac_hiu_reg_setb(HHI_VDAC_CNTL0,
						0, 10, 1);
				vdac_out_cntl0_bit0(1, VDAC_MODULE_ATV_DEMOD);
			} else if (pri_flag & VDAC_MODULE_TVAFE) {
				vdac_out_cntl1_bit3(0, VDAC_MODULE_TVAFE);
				vdac_out_cntl0_bit10(1, VDAC_MODULE_TVAFE);
			} else if (pri_flag & VDAC_MODULE_DTV_DEMOD) {
				if (s_vdac_data->cpu_id == VDAC_CPU_GXLX)
					vdac_out_cntl1_bit3(1,
							VDAC_MODULE_DTV_DEMOD);
				ana_ref_cntl0_bit9(1, VDAC_MODULE_DTV_DEMOD);
			}
		}
		break;
	case VDAC_MODULE_AUDIO_OUT: /* audio demod */
		/*Bandgap optimization*/
		if (s_vdac_data->cpu_id == VDAC_CPU_TXHD ||
			s_vdac_data->cpu_id == VDAC_CPU_TXLX)
			vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 0xe, 3, 5);

		if (s_vdac_data->cpu_id == VDAC_CPU_TXL ||
			s_vdac_data->cpu_id == VDAC_CPU_TXLX) {
			if (on)
				ana_ref_cntl0_bit9(1, VDAC_MODULE_AUDIO_OUT);
			else
				ana_ref_cntl0_bit9(0, VDAC_MODULE_AUDIO_OUT);
		}
		break;
	default:
		pr_err("%s:module_sel: 0x%x wrong module index !! "
					, __func__, module_sel);
		break;
	}

	mutex_unlock(&vdac_mutex);
}
EXPORT_SYMBOL(vdac_enable);

int vdac_enable_check_dtv(void)
{
	return (pri_flag & VDAC_MODULE_DTV_DEMOD) ? 1:0;
}

static int amvdac_open(struct inode *inode, struct file *file)
{
	struct amvdac_dev_s *devp;
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct amvdac_dev_s, cdev);
	file->private_data = devp;
	return 0;
}

static int amvdac_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static const struct file_operations amvdac_fops = {
	.owner   = THIS_MODULE,
	.open    = amvdac_open,
	.release = amvdac_release,
};

struct meson_vdac_data meson_gxtvbb_vdac_data = {
	.cpu_id = VDAC_CPU_GXTVBB,
	.name = "meson-gxtvbb-vdac",
};

struct meson_vdac_data meson_gx_l_m_vdac_data = {
	.cpu_id = VDAC_CPU_GX_L_M,
	.name = "meson-gx_l_m-vdac",
};

struct meson_vdac_data meson_txl_vdac_data = {
	.cpu_id = VDAC_CPU_TXL,
	.name = "meson-txl-vdac",
};

struct meson_vdac_data meson_txlx_vdac_data = {
	.cpu_id = VDAC_CPU_TXLX,
	.name = "meson-txlx-vdac",
};

struct meson_vdac_data meson_gxlx_vdac_data = {
	.cpu_id = VDAC_CPU_GXLX,
	.name = "meson-gxlx-vdac",
};

struct meson_vdac_data meson_txhd_vdac_data = {
	.cpu_id = VDAC_CPU_TXHD,
	.name = "meson-txhd-vdac",
};

struct meson_vdac_data meson_g12ab_vdac_data = {
	.cpu_id = VDAC_CPU_G12AB,
	.name = "meson-g12ab-vdac",
};

struct meson_vdac_data meson_tl1_vdac_data = {
	.cpu_id = VDAC_CPU_TL1,
	.name = "meson-tl1-vdac",
};

static const struct of_device_id meson_vdac_dt_match[] = {
	{
		.compatible = "amlogic, vdac-gxtvbb",
		.data		= &meson_gxtvbb_vdac_data,
	}, {
		.compatible = "amlogic, vdac-gxl",
		.data		= &meson_gx_l_m_vdac_data,
	}, {
		.compatible = "amlogic, vdac-gxm",
		.data		= &meson_gx_l_m_vdac_data,
	}, {
		.compatible = "amlogic, vdac-txl",
		.data		= &meson_txl_vdac_data,
	}, {
		.compatible = "amlogic, vdac-txlx",
		.data		= &meson_txlx_vdac_data,
	}, {
		.compatible = "amlogic, vdac-gxlx",
		.data		= &meson_gxlx_vdac_data,
	}, {
		.compatible = "amlogic, vdac-txhd",
		.data		= &meson_txhd_vdac_data,
	}, {
		.compatible = "amlogic, vdac-g12a",
		.data		= &meson_g12ab_vdac_data,
	}, {
		.compatible = "amlogic, vdac-g12b",
		.data		= &meson_g12ab_vdac_data,
	}, {
		.compatible = "amlogic, vdac-tl1",
		.data		= &meson_tl1_vdac_data,
	},
	{},
};

static int aml_vdac_probe(struct platform_device *pdev)
{
	int ret = 0;
	const struct of_device_id *match;
	struct amvdac_dev_s *devp = &amvdac_dev;

	memset(devp, 0, (sizeof(struct amvdac_dev_s)));

	match = of_match_device(meson_vdac_dt_match, &pdev->dev);
	if (match == NULL) {
		pr_err("%s,no matched table\n", __func__);
		return -1;
	}
	s_vdac_data = (struct meson_vdac_data *)match->data;
	pr_info("%s:probe start.cpu_id:%d,name:%s\n", __func__,
		s_vdac_data->cpu_id, s_vdac_data->name);

	ret = alloc_chrdev_region(&devp->devno, 0, 1, AMVDAC_NAME);
	if (ret < 0)
		goto fail_alloc_region;

	devp->clsp = class_create(THIS_MODULE, AMVDAC_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}

	cdev_init(&devp->cdev, &amvdac_fops);
	devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&devp->cdev, devp->devno, 1);
	if (ret)
		goto fail_add_cdev;

	devp->dev = device_create(devp->clsp, NULL, devp->devno,
			NULL, AMVDAC_NAME);
	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto fail_create_device;
	}

	pr_info("%s: ok\n", __func__);

	return ret;

fail_create_device:
	pr_err("%s: device create error\n", __func__);
	cdev_del(&devp->cdev);
fail_add_cdev:
	pr_err("%s: add device error\n", __func__);
fail_create_class:
	pr_err("%s: class create error\n", __func__);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
fail_alloc_region:
	pr_err("%s:  alloc error\n", __func__);

	return ret;
}

static int __exit aml_vdac_remove(struct platform_device *pdev)
{
	struct amvdac_dev_s *devp = &amvdac_dev;

	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);

	pr_info("%s: amvdac_exit.\n", __func__);

	return 0;
}

#ifdef CONFIG_PM
static int amvdac_drv_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	if (s_vdac_data->cpu_id == VDAC_CPU_TXL)
		vdac_hiu_reg_write(HHI_VDAC_CNTL0, 0);
	pr_info("%s: suspend module\n", __func__);
	return 0;
}

static int amvdac_drv_resume(struct platform_device *pdev)
{
	pr_info("%s: resume module\n", __func__);
	return 0;
}
#endif

static void amvdac_drv_shutdown(struct platform_device *pdev)
{
	unsigned int cntl0, cntl1;

	pr_info("%s: shutdown module\n", __func__);
	cntl0 = 0x0;
	if (is_meson_txl_cpu() || is_meson_txlx_cpu())
		cntl1 = 0x0;
	else
		cntl1 = 0x8;
	vdac_set_ctrl0_ctrl1(cntl0, cntl1);
}

static struct platform_driver aml_vdac_driver = {
	.driver = {
		.name = "aml_vdac",
		.owner = THIS_MODULE,
		.of_match_table = meson_vdac_dt_match,
	},
	.probe = aml_vdac_probe,
	.remove = __exit_p(aml_vdac_remove),
#ifdef CONFIG_PM
	.suspend    = amvdac_drv_suspend,
	.resume     = amvdac_drv_resume,
#endif
	.shutdown	= amvdac_drv_shutdown,
};

static int __init aml_vdac_init(void)
{
	pr_info("%s: module init\n", __func__);

	vdac_init_succ_flag = 0;
	s_vdac_data = NULL;

	mutex_init(&vdac_mutex);

	if (platform_driver_register(&aml_vdac_driver)) {
		pr_err("%s: failed to register vdac driver module\n", __func__);
		return -ENODEV;
	}

	vdac_init_succ_flag = 1;
	return 0;
}

static void __exit aml_vdac_exit(void)
{
	mutex_destroy(&vdac_mutex);
	pr_info("%s: module exit\n", __func__);
	platform_driver_unregister(&aml_vdac_driver);
}

postcore_initcall(aml_vdac_init);
module_exit(aml_vdac_exit);

MODULE_DESCRIPTION("AMLOGIC vdac driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank Zhao <frank.zhao@amlogic.com>");
