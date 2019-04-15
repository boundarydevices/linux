/*
 * drivers/amlogic/media/vin/tvin/csi/csi.c
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/major.h>

#include <linux/amlogic/media/mipi/am_mipi_csi2.h>

#include "../tvin_global.h"
#include "../vdin/vdin_regs.h"
#include "../vdin/vdin_drv.h"
#include "../vdin/vdin_ctl.h"
#include "../tvin_format_table.h"
#include "../tvin_frontend.h"
#include "csi.h"

#define DEV_NAME  "amvdec_csi"
#define DRV_NAME  "amvdec_csi"
#define CLS_NAME  "amvdec_csi"
#define MOD_NAME  "amvdec_csi"

#define CSI_MAX_DEVS             1
#define WDG_STEP_JIFFIES        10

static dev_t amcsi_devno;
static struct class *amcsi_clsp;

static void init_csi_dec_parameter(struct amcsi_dev_s *devp)
{
	enum tvin_sig_fmt_e fmt;
	const struct tvin_format_s *fmt_info_p;

	pr_info("init_csi_dec_parameter.\n");
	fmt = devp->para.fmt;
	fmt_info_p =
	(struct tvin_format_s *)tvin_get_fmt_info(fmt);
	devp->para.v_active    = 1080;
	devp->para.h_active    = 1920;
	devp->para.hsync_phase = 0;
	devp->para.vsync_phase = 0;
	devp->para.hs_bp       = 0;
	devp->para.vs_bp       = 0;
	devp->para.csi_hw_info.lanes = 2;
}

static void reset_btcsi_module(void)
{
	DPRINT("%s, %d\n", __func__, __LINE__);
}

static void reinit_csi_dec(struct amcsi_dev_s *devp)
{
	DPRINT("%s, %d\n", __func__, __LINE__);
}

static void start_amvdec_csi(struct amcsi_dev_s *devp)
{
	enum tvin_port_e port =  devp->para.port;

	pr_info("start_amvdec_csi.\n");

	if (devp->dec_status & TVIN_AMCSI_RUNNING) {
		pr_info("%s csi have started alreadly.\n",
			__func__);
		return;
	}
	devp->dec_status = TVIN_AMCSI_RUNNING;
	pr_info("start_amvdec_csi port = %x\n", port);
	if (port == TVIN_PORT_MIPI) {
		init_csi_dec_parameter(devp);
		reinit_csi_dec(devp);
	} else {
		devp->para.fmt  = TVIN_SIG_FMT_NULL;
		devp->para.port = TVIN_PORT_NULL;
		DPRINT("%s: input is not selected, please try again\n",
			__func__);
		return;
	}
	devp->dec_status = TVIN_AMCSI_RUNNING;
}

static void stop_amvdec_csi(struct amcsi_dev_s *devp)
{
	if (devp->dec_status & TVIN_AMCSI_RUNNING) {
		reset_btcsi_module();
		devp->dec_status = TVIN_AMCSI_STOP;
	} else
		DPRINT("%s device is not started yet\n", __func__);
}

static bool amcsi_check_skip_frame(struct tvin_frontend_s *fe)
{
	struct amcsi_dev_s *devp =
		container_of(fe, struct amcsi_dev_s, frontend);

	if (devp->csi_parm.skip_frames > 0) {
		devp->csi_parm.skip_frames--;
		return true;
	} else
		return false;
}

int amcsi_support(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	if (port != TVIN_PORT_MIPI) {
		DPRINT("this is not MIPI port\n");
		return -1;
	} else {
		return 0;
	}
}

static int amcsi_open(struct inode *node, struct file *file)
{
	struct amcsi_dev_s *csi_devp;

	csi_devp = container_of(node->i_cdev, struct amcsi_dev_s, cdev);
	file->private_data = csi_devp;

	return 0;
}

static int amcsi_release(struct inode *node, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static const struct file_operations amcsi_fops = {
	.owner    = THIS_MODULE,
	.open     = amcsi_open,
	.release  = amcsi_release,
};

void amcsi_start(struct tvin_frontend_s *fe, enum tvin_sig_fmt_e fmt)
{
	struct amcsi_dev_s *csi_devp;

	csi_devp = container_of(fe, struct amcsi_dev_s, frontend);
	start_amvdec_csi(csi_devp);
}

static void amcsi_stop(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct amcsi_dev_s *devp =
		container_of(fe, struct amcsi_dev_s, frontend);

	if (port != TVIN_PORT_MIPI) {
		DPRINT("%s:invaild port %d.\n", __func__, port);
		return;
	}
	stop_amvdec_csi(devp);
}

void amcsi_get_sig_property(struct tvin_frontend_s *fe,
		struct tvin_sig_property_s *prop)
{
	struct amcsi_dev_s *devp =
		container_of(fe, struct amcsi_dev_s, frontend);

	prop->color_format = devp->para.cfmt;
	prop->dest_cfmt = devp->para.dfmt;
	pr_info("devp->para.cfmt=%d, devp->para.dfmt=%d\n",
		devp->para.cfmt, devp->para.dfmt);
	prop->decimation_ratio = 0;
}

int amcsi_isr(struct tvin_frontend_s *fe, unsigned int hcnt)
{
	struct amcsi_dev_s *devp =
		container_of(fe, struct amcsi_dev_s, frontend);
	unsigned int data1 = 0;
	struct am_csi2_frame_s frame;

	frame.w = READ_CSI_ADPT_REG_BIT(CSI2_PIC_SIZE_STAT, 0, 16);
	frame.h = READ_CSI_ADPT_REG_BIT(CSI2_PIC_SIZE_STAT, 16, 16);
	frame.err = READ_CSI_ADPT_REG(CSI2_ERR_STAT0);
	data1 = READ_CSI_ADPT_REG(CSI2_GEN_STAT0);

	if (frame.err) {
		DPRINT("%s,error---pixel cnt:%d, line cnt:%d\n",
			__func__, frame.w, frame.h);
		DPRINT("error state:0x%x.,status:0x%x\n",
			frame.err, data1);
		devp->overflow_cnt++;
		WRITE_CSI_ADPT_REG(CSI2_ERR_STAT0, 0);
	}
	if (devp->overflow_cnt > 4) {
		DPRINT("should reset mipi\n");
		devp->overflow_cnt = 0;
		return 0;
	}

	devp->reset = 0;

	return 0;
}

static ssize_t csi_attr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct amcsi_dev_s *csi_devp;
	int i;

	csi_devp = dev_get_drvdata(dev);
	if (csi_devp->dec_status != TVIN_AMCSI_RUNNING) {
		len += sprintf(buf+len, "csi does not start\n");
		return len;
	}

	len += sprintf(buf+len, "csi parameters below\n");
	len +=
	sprintf(buf+len, "\tlanes=%d, channel=%d\n"
		"\tclk_channel=%d\n"
		"\tmode=%d, clock_lane_mode=%d, active_pixel=%d\n"
		"\tactive_line=%d, frame_size=%d, ui_val=%dns\n"
		"\ths_freq=%dhz, urgent=%d\n",
		csi_devp->csi_parm.lanes,
		csi_devp->csi_parm.channel,
		csi_devp->csi_parm.clk_channel,
		csi_devp->csi_parm.mode,
		csi_devp->csi_parm.clock_lane_mode,
		csi_devp->csi_parm.active_pixel,
		csi_devp->csi_parm.active_line,
		csi_devp->csi_parm.frame_size,
		csi_devp->csi_parm.ui_val,
		csi_devp->csi_parm.hs_freq,
		csi_devp->csi_parm.urgent);

	len += sprintf(buf+len, "csi adapter register below\n");
	for (i = CSI_ADPT_START_REG; i <= CSI_ADPT_END_REG; i++) {
		len += sprintf(buf+len, "\t[0x%04x]=0x%08x\n",
		i - CSI_ADPT_START_REG, READ_CSI_ADPT_REG(i));
	}

	len += sprintf(buf+len, "csi phy register below\n");
	for (i = CSI_PHY_START_REG; i <= CSI_PHY_END_REG; i++) {
		len += sprintf(buf+len, "\t[0x%04x]=0x%08x\n",
		i, READ_CSI_PHY_REG(i));
	}

	len += sprintf(buf+len, "csi host register below\n");
	for (i = CSI_HST_START_REG; i <= CSI_HST_END_REG; i++) {
		len += sprintf(buf+len, "\t[0x%04x]=0x%08x\n",
		i << 2, READ_CSI_HST_REG(i));
	}

	return len;
}

static ssize_t csi_attr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	struct amcsi_dev_s *csi_devp;

	unsigned int n = 0;

	char *buf_orig, *ps, *token;
	char *parm[6] = {NULL};

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	csi_devp = dev_get_drvdata(dev);

	ps = buf_orig;
	while (1) {
		if (n >= ARRAY_SIZE(parm)) {
			pr_info("parm array overflow, n=%d\n", n);
			kfree(buf_orig);
			return len;
		}
		token = strsep(&ps, "\n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (strcmp(parm[0], "reset") == 0) {
		pr_info("reset\n");
		am_mipi_csi2_init(&csi_devp->csi_parm);
	} else if (strcmp(parm[0], "init") == 0) {
		pr_info("init mipi measure clock\n");
		init_am_mipi_csi2_clock();
	} else if (strcmp(parm[0], "min") == 0) {
		csi_devp->min_frmrate =
			kstrtol(parm[1], 16, NULL);
		if ((csi_devp->min_frmrate * WDG_STEP_JIFFIES) < HZ)
			csi_devp->min_frmrate = HZ/WDG_STEP_JIFFIES;
		pr_info("min_frmrate=%d\n", csi_devp->min_frmrate);
	}

	kfree(buf_orig);
	return len;
}

static DEVICE_ATTR(hw_info, 0664, csi_attr_show, csi_attr_store);

static int amcsi_feopen(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct amcsi_dev_s *csi_devp =
		container_of(fe, struct amcsi_dev_s, frontend);
	struct vdin_parm_s *parm = fe->private_data;

	if (port != TVIN_PORT_MIPI) {
		DPRINT("[mipi..]%s:invaild port %d.\n", __func__, port);
		return -1;
	}

	if (!memcpy(&csi_devp->para, parm,
		sizeof(struct vdin_parm_s))) {
		DPRINT("[mipi..]%s memcpy error.\n", __func__);
		return -1;
	}

	init_am_mipi_csi2_clock();

	csi_devp->para.port = port;

	memcpy(&csi_devp->csi_parm,
		&parm->csi_hw_info, sizeof(struct csi_parm_s));
	csi_devp->csi_parm.skip_frames = parm->skip_count;

	csi_devp->reset = 0;
	csi_devp->reset_count = 0;

	cal_csi_para(&csi_devp->csi_parm);
	am_mipi_csi2_init(&csi_devp->csi_parm);

	return 0;
}

static void amcsi_feclose(struct tvin_frontend_s *fe)
{
	struct amcsi_dev_s *devp =
		container_of(fe, struct amcsi_dev_s, frontend);
	enum tvin_port_e port = devp->para.port;

	if (port != TVIN_PORT_MIPI) {
		DPRINT("[mipi..]%s:invaild port %d.\n", __func__, port);
		return;
	}

	devp->reset = 0;
	devp->reset_count = 0;

	am_mipi_csi2_uninit();

	memset(&devp->para, 0, sizeof(struct vdin_parm_s));
}

static struct tvin_state_machine_ops_s amcsi_machine_ops = {
	.nosig               = NULL,
	.fmt_changed         = NULL,
	.get_fmt             = NULL,
	.fmt_config          = NULL,
	.adc_cal             = NULL,
	.pll_lock            = NULL,
	.get_sig_property    = amcsi_get_sig_property,
	.vga_set_param       = NULL,
	.vga_get_param       = NULL,
	.check_frame_skip    = amcsi_check_skip_frame,
};

static struct tvin_decoder_ops_s amcsi_decoder_ops_s = {
	.support                = amcsi_support,
	.open                   = amcsi_feopen,
	.start                  = amcsi_start,
	.stop                   = amcsi_stop,
	.close                  = amcsi_feclose,
	.decode_isr             = amcsi_isr,
};

static int csi_add_cdev(struct cdev *cdevp,
		const struct file_operations *fops, int minor)
{
	int ret;
	dev_t devno = MKDEV(MAJOR(amcsi_devno), minor);

	cdev_init(cdevp, fops);
	cdevp->owner = THIS_MODULE;
	ret = cdev_add(cdevp, devno, 1);
	return ret;
}

static void csi_delete_device(int minor)
{
	dev_t devno = MKDEV(MAJOR(amcsi_devno), minor);

	device_destroy(amcsi_clsp, devno);
}

static int amvdec_csi_probe(struct platform_device *pdev)
{
	int ret = 0;
	int id = 0;
	struct amcsi_dev_s *devp = NULL;

	devp = kmalloc(sizeof(struct amcsi_dev_s), GFP_KERNEL);
	if (!devp) {
		ret = -1;
		goto fail_kmalloc_dev;
	}
	memset(devp, 0, sizeof(struct amcsi_dev_s));

	ret = csi_add_cdev(&devp->cdev, &amcsi_fops, 0);
	if (ret != 0) {
		pr_err("%s: failed to add cdev\n", __func__);
		goto fail_add_cdev;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "csi_id", &id);
	if (ret != 0) {
		pr_err("%s: don't find  csi_id.\n", __func__);
		goto fail_add_cdev;
	}
	pdev->id = id;

	sprintf(devp->frontend.name, "%s", DEV_NAME);
	tvin_frontend_init(&devp->frontend, &amcsi_decoder_ops_s,
		&amcsi_machine_ops, pdev->id);
	tvin_reg_frontend(&devp->frontend);
	devp->pdev = pdev;

	platform_set_drvdata(pdev, devp);

	am_mipi_csi2_para_init(pdev);

	pr_info("amvdec_csi probe ok.\n");
	return ret;

fail_add_cdev:
	kfree(devp);
fail_kmalloc_dev:
	return ret;
}

static int amvdec_csi_remove(struct platform_device *pdev)
{
	struct amcsi_dev_s *devp;

	devp = (struct amcsi_dev_s *)platform_get_drvdata(pdev);

	tvin_unreg_frontend(&devp->frontend);
	device_remove_file(devp->dev, &dev_attr_hw_info);
	deinit_am_mipi_csi2_clock();
	csi_delete_device(pdev->id);
	cdev_del(&devp->cdev);
	dev_set_drvdata(devp->dev, NULL);
	platform_set_drvdata(pdev, NULL);
	kfree(devp);
	return 0;
}

static const struct of_device_id csi_dt_match[] = {
	{
		.compatible = "amlogic, amvdec_csi",
	},
	{},
};

static struct platform_driver amvdec_csi_driver = {
	.probe      = amvdec_csi_probe,
	.remove     = amvdec_csi_remove,
	.driver     = {
			.name   = DRV_NAME,
			.owner  = THIS_MODULE,
			.of_match_table = csi_dt_match,
	}
};

static int __init amvdec_csi_init_module(void)
{
	int ret = 0;

	pr_info("amvdec_csi module: init.\n");
	ret = alloc_chrdev_region(&amcsi_devno, 0,
		CSI_MAX_DEVS, DEV_NAME);
	if (ret != 0) {
		pr_err("%s:failed to alloc major number\n",
			__func__);
		goto fail_alloc_cdev_region;
	}

	pr_info("%s:major %d\n", __func__, MAJOR(amcsi_devno));

	amcsi_clsp = class_create(THIS_MODULE, CLS_NAME);
	if (IS_ERR(amcsi_clsp)) {
		ret = PTR_ERR(amcsi_clsp);
		pr_err("%s:failed to create class\n", __func__);
		goto fail_class_create;
	}

	ret = platform_driver_register(&amvdec_csi_driver);
	if (ret) {
		pr_err("failed to register amvdec_csi driver\n");
		goto fail_pdrv_register;
	}

	pr_info("amvdec_csi module: init. ok\n");
	return 0;

fail_pdrv_register:
	class_destroy(amcsi_clsp);
fail_class_create:
	unregister_chrdev_region(amcsi_devno, CSI_MAX_DEVS);
fail_alloc_cdev_region:
	pr_err("amvdec_csi module: init failed, ret=%d\n", ret);

	return ret;
}

static void __exit amvdec_csi_exit_module(void)
{
	pr_info("amvdec_csi module remove.\n");
	DPRINT("%s, %d\n", __func__, __LINE__);
	class_destroy(amcsi_clsp);
	unregister_chrdev_region(amcsi_devno, CSI_MAX_DEVS);
	platform_driver_unregister(&amvdec_csi_driver);
}

module_init(amvdec_csi_init_module);
module_exit(amvdec_csi_exit_module);

MODULE_DESCRIPTION("AMLOGIC CSI input driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
