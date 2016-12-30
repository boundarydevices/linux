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
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/vdac_dev.h>
#include <linux/io.h>
#include <linux/mutex.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif

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
static struct mutex vdac_mutex;

#define HHI_VDAC_CNTL0 0x10bd
#define HHI_VDAC_CNTL1 0x10be

#define VDAC_MODULE_ATV_DEMOD 0x1
#define VDAC_MODULE_DTV_DEMOD 0x2
#define VDAC_MODULE_TVAFE     0x4
#define VDAC_MODULE_CVBS_OUT  0x8
#define VDAC_MODULE_AUDIO_OUT  0x10

void __iomem *vdac_hiu_reg_base;/* = *ioremap(0xc883c000, 0x2000); */

static bool vdac_init_succ_flag;

/* priority for module,
 * module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; cvbsout:0x8
 */
static unsigned int pri_flag;

#ifdef CONFIG_AMLOGIC_VPU
static unsigned int vpu_gate;
#endif

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
	uint32_t ret;

	if (vdac_init_succ_flag)
		ret = readl(vdac_hiu_reg_base+((reg - 0x1000)<<2));
	else {
		pr_err("%s: vdac rd error !!\n", __func__);
		ret = 0;
	}
	return ret;
}

static inline uint32_t vdac_hiu_reg_write(uint32_t reg, uint32_t val)
{
	if (vdac_init_succ_flag)
		writel(val, (vdac_hiu_reg_base+((reg - 0x1000)<<2)));
	else
		pr_err("%s: vdac wr error !!\n", __func__);

	return 0;
}

static inline void vdac_hiu_reg_setb(uint32_t reg,
	    const uint32_t value,
	    const uint32_t start,
	    const uint32_t len)
{
	vdac_hiu_reg_write(reg, ((vdac_hiu_reg_read(reg) &
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
#if 0
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL) {
			if (on)
				vdac_cntl0_bit9 |= VDAC_MODULE_AUDIO_OUT;
			else
				vdac_cntl0_bit9 &= ~VDAC_MODULE_AUDIO_OUT;
		}
#endif
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
#if 0
	if (is_meson_txl_cpu() || (is_meson_gxtvbb_cpu() &&
		(get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR) != 0xa)))
		vdac_hiu_reg_setb(HHI_VDAC_CNTL0, enable, 9, 1);
	else
#endif
	vdac_hiu_reg_setb(HHI_VDAC_CNTL0, ~enable, 9, 1);
#if 0
	pr_info("\nvdac_cntl0_bit9 reg:0x%x\n",
			vdac_hiu_reg_getb(HHI_VDAC_CNTL0, 9, 1));
#endif
}
EXPORT_SYMBOL(ana_ref_cntl0_bit9);

/*avin cvbsout signal,
 * module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; dac:0x8
 */
void vdac_out_cntl0_bit10(bool on, unsigned int module_sel)
{
	bool enable = 0;

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
#if 0
	pr_info("\vdac_cntl0_bit0 reg:0x%x\n",
			vdac_hiu_reg_getb(HHI_VDAC_CNTL0, 10, 1));
#endif
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

	vdac_hiu_reg_setb(HHI_VDAC_CNTL0, enable, 0, 1);
#if 0
	pr_info("\vdac_cntl0_bit0 reg:0x%x\n",
			vdac_hiu_reg_getb(HHI_VDAC_CNTL0, 0, 1));
#endif
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
#if 0
	if (is_meson_txl_cpu() || (is_meson_gxtvbb_cpu() &&
		(get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR) != 0xa)))
		vdac_hiu_reg_setb(HHI_VDAC_CNTL1, enable, 3, 1);
	else
#endif
	vdac_hiu_reg_setb(HHI_VDAC_CNTL1, ~enable, 3, 1);
#if 0
	pr_info("\nvdac_cntl1_bit3 reg:0x%x\n",
		vdac_hiu_reg_getb(HHI_VDAC_CNTL1, 3, 1));
#endif
}
EXPORT_SYMBOL(vdac_out_cntl1_bit3);

void vdac_set_ctrl0_ctrl1(unsigned int ctrl0, unsigned int ctrl1)
{
	vdac_hiu_reg_write(HHI_VDAC_CNTL0, ctrl0);
	vdac_hiu_reg_write(HHI_VDAC_CNTL1, ctrl1);
}
EXPORT_SYMBOL(vdac_set_ctrl0_ctrl1);

#ifdef CONFIG_AMLOGIC_VPU
void vpu_clk_gate_set(bool on, unsigned int module_sel)
{
	bool enable = 0;

	switch (module_sel & 0x1f) {
	case VDAC_MODULE_ATV_DEMOD: /* dtv demod */
		if (on)
			vpu_gate |= VDAC_MODULE_ATV_DEMOD;
		else
			vpu_gate &= ~VDAC_MODULE_ATV_DEMOD;
		break;
	case VDAC_MODULE_DTV_DEMOD: /* atv demod */
		if (on)
			vpu_gate |= VDAC_MODULE_DTV_DEMOD;
		else
			vpu_gate &= ~VDAC_MODULE_DTV_DEMOD;
		break;
	case VDAC_MODULE_TVAFE: /* cvbs in demod */
		if (on)
			vpu_gate |= VDAC_MODULE_TVAFE;
		else
			vpu_gate &= ~VDAC_MODULE_TVAFE;
		break;
	case VDAC_MODULE_CVBS_OUT: /* cvbs in demod */
		if (on)
			vpu_gate |= VDAC_MODULE_CVBS_OUT;
		else
			vpu_gate &= ~VDAC_MODULE_CVBS_OUT;
		break;
	case VDAC_MODULE_AUDIO_OUT: /* audio out ctrl*/
		if (on)
			vpu_gate |= VDAC_MODULE_AUDIO_OUT;
		else
			vpu_gate &= ~VDAC_MODULE_AUDIO_OUT;
		break;
	default:
		pr_err("module_sel: 0x%x wrong module index !! ", module_sel);
		break;
	}
	if ((vpu_gate & 0x1f) == 0)
		enable = 0;
	else
		enable = 1;
	if (enable)
		switch_vpu_clk_gate_vmod(VPU_VENC_DAC, VPU_CLK_GATE_ON);
	else
		switch_vpu_clk_gate_vmod(VPU_VENC_DAC, VPU_CLK_GATE_OFF);
}
#endif

/* dac ctl,
 * module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; dac:0x8
 */
void vdac_enable(bool on, unsigned int module_sel)
{
	pr_info("\n%s: on:%d,module_sel:%x\n", __func__, on, module_sel);

	mutex_lock(&vdac_mutex);
	switch (module_sel) {
	case VDAC_MODULE_ATV_DEMOD: /* atv demod */
		if (on) {
			ana_ref_cntl0_bit9(1, VDAC_MODULE_ATV_DEMOD);
			pri_flag &= ~VDAC_MODULE_TVAFE;
			pri_flag |= VDAC_MODULE_ATV_DEMOD;
			if (pri_flag & VDAC_MODULE_CVBS_OUT)
				break;
			/*Cdac pwd*/
			vdac_out_cntl1_bit3(1, VDAC_MODULE_ATV_DEMOD);
			/* enable AFE output buffer */
			vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 0, 10, 1);
			vdac_out_cntl0_bit0(1, VDAC_MODULE_ATV_DEMOD);
		} else {
			ana_ref_cntl0_bit9(0, VDAC_MODULE_ATV_DEMOD);
			pri_flag &= ~VDAC_MODULE_ATV_DEMOD;
			if (pri_flag & VDAC_MODULE_CVBS_OUT)
				break;
			vdac_out_cntl0_bit0(0, 0x4);
			/* Disable AFE output buffer */
			vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 0, 10, 1);
			/* enable dac output */
			vdac_out_cntl1_bit3(0, 0x4);
		}
		break;
	case VDAC_MODULE_DTV_DEMOD: /* dtv demod */
		if (on)
			ana_ref_cntl0_bit9(1, VDAC_MODULE_DTV_DEMOD);
		else
			ana_ref_cntl0_bit9(0, VDAC_MODULE_DTV_DEMOD);
		break;
	case VDAC_MODULE_TVAFE: /* av in demod */
		if (on) {
			ana_ref_cntl0_bit9(1, VDAC_MODULE_TVAFE);
			pri_flag &= ~VDAC_MODULE_ATV_DEMOD;
			pri_flag |= VDAC_MODULE_TVAFE;
			if (pri_flag & VDAC_MODULE_CVBS_OUT)
				break;
			vdac_out_cntl1_bit3(0, VDAC_MODULE_TVAFE);
			vdac_out_cntl0_bit10(1, VDAC_MODULE_TVAFE);
		} else {
			ana_ref_cntl0_bit9(0, VDAC_MODULE_TVAFE);
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
				vdac_hiu_reg_setb(HHI_VDAC_CNTL0, 0, 10, 1);
				vdac_out_cntl0_bit0(1, VDAC_MODULE_ATV_DEMOD);
			} else if (pri_flag & VDAC_MODULE_TVAFE) {
				vdac_out_cntl1_bit3(0, VDAC_MODULE_TVAFE);
				vdac_out_cntl0_bit10(1, VDAC_MODULE_TVAFE);
			}
		}
		break;
	case VDAC_MODULE_AUDIO_OUT: /* audio demod */
#if 0
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL) {
			if (on)
				ana_ref_cntl0_bit9(1, VDAC_MODULE_AUDIO_OUT);
			else
				ana_ref_cntl0_bit9(0, VDAC_MODULE_AUDIO_OUT);
		}
#endif
		break;
	default:
		pr_err("%s:module_sel: 0x%x wrong module index !! "
					, __func__, module_sel);
		break;
	}
	mutex_unlock(&vdac_mutex);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_clk_gate_set(on, module_sel);
#endif
}
EXPORT_SYMBOL(vdac_enable);

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

static int aml_vdac_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct amvdac_dev_s *devp = &amvdac_dev;

	memset(devp, 0, (sizeof(struct amvdac_dev_s)));

	pr_info("\n%s: probe start\n", __func__);

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
	kfree(devp);
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
	kfree(devp);
	pr_info("%s: amvdac_exit.\n", __func__);

	return 0;
}

#ifdef CONFIG_PM
static int amvdac_drv_suspend(struct platform_device *pdev,
		pm_message_t state)
{

	pr_info("%s: suspend module\n", __func__);
	return 0;
}

static int amvdac_drv_resume(struct platform_device *pdev)
{
	pr_info("%s: resume module\n", __func__);
	return 0;
}
#endif


/* static const struct of_device_id aml_vdac_dt_match[] = { */
/* { */
/* .compatible = "amlogic, vdac", */
/* }, */
/* {}, */
/* }; */

static struct platform_driver aml_vdac_driver = {
	.driver = {
		.name = "aml_vdac",
		.owner = THIS_MODULE,
		/* .of_match_table = aml_vdac_dt_match, */
	},
	.probe = aml_vdac_probe,
	.remove = __exit_p(aml_vdac_remove),
#ifdef CONFIG_PM
	.suspend    = amvdac_drv_suspend,
	.resume     = amvdac_drv_resume,
#endif
};

static int __init aml_vdac_init(void)
{
	pr_info("%s: module init\n", __func__);

	vdac_init_succ_flag = 0;

	mutex_init(&vdac_mutex);
	/* remap the hiu bus */
	vdac_hiu_reg_base = ioremap(0xc883c000, 0x2000);

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
