/*
 * drivers/amlogic/media/vin/tvin/bt656/bt656_601_in.c
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
#include <linux/major.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>

/* #include <mach/am_regs.h> */
/* #include <mach/mod_gate.h> */

#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "../tvin_frontend.h"
#include "bt656_601_in.h"

#define BT656_DEV_NAME  "amvdec_656in"
#define BT656_DRV_NAME  "amvdec_656in"
#define BT656_CLS_NAME  "amvdec_656in"
#define BT656_MOD_NAME  "amvdec_656in"

/* #define HANDLE_BT656IN_IRQ */

#define BT656_MAX_DEVS             2
/* #define BT656IN_ANCI_DATA_SIZE        0x4000 */

#define BT656_VER "2017/08/23"

/* Per-device (per-bank) structure */

/* static struct struct am656in_dev_s am656in_dev_; */
static dev_t am656in_devno;
static struct class *am656in_clsp;
static unsigned char hw_cnt;
static struct am656in_dev_s *am656in_devp[BT656_MAX_DEVS];

#ifdef HANDLE_BT656IN_IRQ
static const char bt656in_dec_id[] = "bt656in-dev";
#endif

static struct mutex bt656_mutex;

/* bt656 debug function */
static ssize_t reg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t count)
{
	unsigned int argn = 0, addr = 0, value = 0, end = 0;
	char *p, *para, *buf_work, cmd = 0;
	char *argv[3];
	long val;
	struct am656in_dev_s *devp = dev_get_drvdata(dev);

	if ((devp->dec_status & TVIN_AM656_RUNNING) == 0) {
		BT656ERR("bt656[%d] is not running.\n", devp->index);
		return count;
	}

	buf_work = kstrdup(buff, GFP_KERNEL);
	p = buf_work;

	for (argn = 0; argn < 3; argn++) {
		para = strsep(&p, " ");
		if (para == NULL)
			break;
		argv[argn] = para;
	}

	if (argn < 1 || argn > 3)
		goto reg_store_exit;

	cmd = argv[0][0];
	switch (cmd) {
	case 'r':
	case 'R':
		if (argn < 2) {
			pr_err("syntax error.\n");
		} else{
			/* addr = simple_strtol(argv[1], NULL, 16); */
			if (kstrtol(argv[1], 16, &val) < 0)
				break;
			addr = val;
			value = bt656_rd(devp->index, addr);
			pr_info("reg[%d:0x%2x]=0x%08x\n",
					devp->index, addr, value);
		}
		break;
	case 'w':
	case 'W':
		if (argn < 3) {
			pr_err("syntax error.\n");
		} else{
			/* value = simple_strtol(argv[1], NULL, 16); */
			if (kstrtol(argv[1], 16, &val) < 0)
				break;
			value = val;
			/* addr = simple_strtol(argv[2], NULL, 16); */
			if (kstrtol(argv[2], 16, &val) < 0)
				break;
			addr = val;
			bt656_wr(devp->index, addr, value);
			pr_info("Write reg[%d:0x%2x]=0x%08x\n",
				devp->index, addr,
				bt656_rd(devp->index, addr));
		}
		break;
	case 'd':
	case 'D':
		/* if (argn < 3) { */
		/* pr_err("syntax error.\n"); */
		/* } else { */
			/* addr = simple_strtol(argv[1], NULL, 16); */
			/* if (kstrtol(argv[1], 16, &val) < 0) */
			/* break; */
			/* addr = val; */
			/* end = simple_strtol(argv[2], NULL, 16); */
			/* if (kstrtol(argv[2], 16, &val) < 0) */
			/* break; */
			/* end = val; */
			addr = 0;
			end = 0x1f;
			for (; addr <= end; addr++)
				pr_info("reg[%d:0x%2x]=0x%08x\n",
					devp->index, addr,
					bt656_rd(devp->index, addr));
		/* } */
		break;
	default:
		pr_err("not support.\n");
		break;
	}

reg_store_exit:
	kfree(buf_work);
	return count;
}

static ssize_t reg_show(struct device *dev,
		struct device_attribute *attr, char *buff)
{
	ssize_t len = 0;

	len += sprintf(buff+len, "Usage:\n");
	len += sprintf(buff+len,
		"\techo [read|write <data>] addr > reg;\n");
	len += sprintf(buff+len,
		"\techo dump > reg; Dump regs.\n");
	len += sprintf(buff+len, "Address format:\n");
	len += sprintf(buff+len, "\taddr : 0xXXXX, 8 bits register address\n");

	return len;
}
static DEVICE_ATTR(reg, 0644, reg_show, reg_store);

static void init_656in_dec_parameter(struct am656in_dev_s *devp)
{
	enum tvin_sig_fmt_e fmt;
	const struct tvin_format_s *fmt_info_p;

	fmt = devp->para.fmt;
	fmt_info_p = tvin_get_fmt_info(fmt);

	if (!fmt_info_p) {
		BT656ERR("bt656[%d] %s:invaild fmt %d.\n",
				devp->index, __func__, fmt);
		return;
	}

	if (fmt < TVIN_SIG_FMT_MAX) {
		devp->para.v_active    = fmt_info_p->v_active;
		devp->para.h_active    = fmt_info_p->h_active;
		devp->para.hsync_phase = 0;
		devp->para.vsync_phase = 0;
		devp->para.hs_bp       = 0;
		devp->para.vs_bp       = 0;
	}
}

static void init_656in_dec_hdmi_parameter(struct am656in_dev_s *devp)
{
	enum tvin_sig_fmt_e fmt;
	const struct tvin_format_s *fmt_info_p;

	fmt = devp->para.fmt;
	fmt_info_p = tvin_get_fmt_info(fmt);

	if (!fmt_info_p) {
		BT656ERR("bt656[%d] %s:invaild fmt %d.\n",
				devp->index, __func__, fmt);
		return;
	}

	if (fmt < TVIN_SIG_FMT_MAX) {
		devp->para.v_active    = fmt_info_p->v_active;
		devp->para.h_active    = fmt_info_p->h_active;
		/*devp->para.hsync_phase = fmt_info_p->v_active;
		 *devp->para.vsync_phase = fmt_info_p->v_active;
		 */
		devp->para.hs_bp       = fmt_info_p->hs_bp;
		devp->para.vs_bp       = fmt_info_p->vs_bp;
	}
}

static void reset_bt656in_module(struct am656in_dev_s *devp)
{
	int temp_data = 0;
	unsigned int offset = devp->index;

	temp_data = bt656_rd(offset, BT_CTRL);
	temp_data &= ~(1 << BT_EN_BIT);
	bt656_wr(offset, BT_CTRL, temp_data); /* disable BT656 input */

	/* reset BT656in module. */
	temp_data = bt656_rd(offset, BT_CTRL);
	temp_data |= (1 << BT_SOFT_RESET);
	bt656_wr(offset, BT_CTRL, temp_data);

	temp_data = bt656_rd(offset, BT_CTRL);
	temp_data &= ~(1 << BT_SOFT_RESET);
	bt656_wr(offset, BT_CTRL, temp_data);
}

/*
 * NTSC or PAL input(interlace mode): CLOCK + D0~D7(with SAV + EAV )
 */
static void reinit_bt656in_dec(struct am656in_dev_s *devp)
{
	unsigned int offset = devp->index;

	reset_bt656in_module(devp);

	 /* field 0/1 start lcnt: default value */
	bt656_wr(offset, BT_FIELDSADR, (4 << 16) | 4);
	/* configuration the BT PORT control */
	/* For standaREAD_CBUS_REG bt656 in stream,
	 * there's no HSYNC VSYNC pins.
	 */
	/* So we don't need to configure the port. */
	/* data itself is 8 bits. */
	bt656_wr(offset, BT_PORT_CTRL, 1 << BT_D8B);


	bt656_wr(offset, BT_SWAP_CTRL, (4 << 0) |  /* POS_Y1_IN */
			(5 << 4) |        /* POS_Cr0_IN */
			(6 << 8) |        /* POS_Y0_IN */
			(7 << 12));       /* POS_CB0_IN */

	bt656_wr(offset, BT_LINECTRL, 0);
	/* there is no use anci in m2 */
	/* ANCI is the field blanking data, like close caption.
	 * If it connected to digital camara interface,
	 * the jpeg bitstream also use this ANCI FIFO.
	 */
	/* bt656_wr(BT_ANCISADR, devp->mem_start); */
	/* bt656_wr(BT_ANCIEADR, 0); //devp->mem_start + devp->mem_size); */

	bt656_wr(offset, BT_AFIFO_CTRL,   (1 << 31) |
			(1 << 6) |     /* fill _en; */
			(1 << 3));     /* urgent */

	bt656_wr(offset, BT_INT_CTRL,   /* (1 << 5) |    //ancififo done int. */
			/* (1 << 4) |    //SOF interrupt enable. */
			/* (1 << 3) |      //EOF interrupt enable. */
			(1 << 1)); /* | //input overflow interrupt enable. */
	/* (1 << 0)); //bt656 controller error interrupt enable. */

	bt656_wr(offset, BT_ERR_CNT, (626 << 16) | (1760));

	if (devp->para.fmt == TVIN_SIG_FMT_BT656IN_576I_50HZ) {
		/* field 0/1 VBI last line number */
		bt656_wr(offset, BT_VBIEND,   22 | (22 << 16));
		 /* Line number of the first video start line in field 0/1. */
		bt656_wr(offset, BT_VIDEOSTART,   23 | (23 << 16));
		/* Line number of the last video line in field 1.
		 * added video end for avoid overflow.
		 */
		bt656_wr(offset, BT_VIDEOEND,    312 |
				(312 << 16));

		/* Update bt656 status register when end of frame. */
		bt656_wr(offset, BT_CTRL,    (0 << BT_UPDATE_ST_SEL) |
				/* Repeated the color data when
				 * do 4:2:2 -> 4:4:4 data transfer.
				 */
				(1 << BT_COLOR_REPEAT) |
				/* use haREAD_CBUS_REGware to check
				 * the PAL/NTSC format
				 * input format if it's
				 * standaREAD_CBUS_REG BT656 input format.
				 */
				(1 << BT_AUTO_FMT) |
				/* BT656 standaREAD_CBUS_REG interface. */
				(1 << BT_MODE_BIT) |
				(1 << BT_EN_BIT) | /* enable BT moduale. */
				/* timing reference is from bit stream. */
				(1 << BT_REF_MODE_BIT) |
				/* use external xclk27. */
				(1 << BT_CLK27_SEL_BIT) |
				(1 << BT_XCLK27_EN_BIT)); /* xclk27 is input. */
		/* wr(VDIN_WR_V_START_END, 287 |   */
		/* (0 << 16));   */
	} else {
		/* if(am656in_dec_info.para.fmt  == TVIN_SIG_FMT_BT656IN_480I)
		 * //input is NTSC
		 */

		/* field 0/1 VBI last line number */
		bt656_wr(offset, BT_VBIEND,   21 | (21 << 16));
		/* Line number of the first video start line in field 0/1. */
		bt656_wr(offset, BT_VIDEOSTART,   18 | (18 << 16));
		/* Line number of the last video line in field 1.
		 * added video end for avoid overflow.
		 */
		bt656_wr(offset, BT_VIDEOEND,    257 |
				(257 << 16));

		/* Update bt656 status register when end of frame. */
		bt656_wr(offset, BT_CTRL,    (0 << BT_UPDATE_ST_SEL) |
				/* Repeated the color data when
				 * do 4:2:2 -> 4:4:4 data transfer.
				 */
			(1 << BT_COLOR_REPEAT) |
			/* use haREAD_CBUS_REGware to check the
			 * PAL/NTSC format input format
			 * if it's standaREAD_CBUS_REG
			 *  BT656 input format.
			 */
			(1 << BT_AUTO_FMT) |
				/* BT656 standaREAD_CBUS_REG interface. */
			(1 << BT_MODE_BIT) |
			(1 << BT_EN_BIT) |    /* enable BT moduale. */
				/* timing reference is from bit stream. */
			(1 << BT_REF_MODE_BIT) |
			(1 << BT_CLK27_SEL_BIT) |    /* use external xclk27. */
			(1 << BT_XCLK27_EN_BIT) |       /* xclk27 is input. */
			(1 << BT_FMT_MODE_BIT));   /* input format is NTSC */
		/* wr(VDIN_WR_V_START_END, 239 | */
		/* (0 << 16));     */

	}
}

/* NTSC or PAL input(interlace mode): CLOCK + D0~D7 + HSYNC + VSYNC + FID */
static void reinit_bt601in_dec(struct am656in_dev_s *devp)
{
	unsigned int offset = devp->index;

	reset_bt656in_module(devp);

	 /* use external idq pin. */
	bt656_wr(offset, BT_PORT_CTRL,    (0 << BT_IDQ_EN)   |
			(1 << BT_IDQ_PHASE)   |
			/* FID came from HS VS. */
			(1 << BT_FID_HSVS) |
			(1 << BT_HSYNC_PHASE) |
			(1 << BT_D8B)     |
			(4 << BT_FID_DELAY) |
			(5 << BT_VSYNC_DELAY) |
			(5 << BT_HSYNC_DELAY));

	/* FID field check done point. */
	bt656_wr(offset, BT_601_CTRL2, (10 << 16));

	/* suppose the input bitstream format is Cb0 Y0 Cr0 Y1. */
	bt656_wr(offset, BT_SWAP_CTRL,    (4 << 0) |
			(5 << 4) |
			(6 << 8) |
			(7 << 13));

	bt656_wr(offset, BT_LINECTRL, (1 << 31) | /*software line ctrl enable*/
			(1644 << 16) |    /* 1440 + 204 */
			220);

	/* ANCI is the field blanking data, like close caption.
	 *  If it connected to digital camara interface,
	 *   the jpeg bitstream also use this ANCI FIFO.
	 */
	/* wr(BT_ANCISADR, devp->mem_start); */
	/* wr(BT_ANCIEADR, 0);//devp->mem_start + devp->mem_size); */

	bt656_wr(offset, BT_AFIFO_CTRL,   (1 << 31) |
			(1 << 6) |     /* fill _en; */
			(1 << 3));     /* urgent */

	bt656_wr(offset, BT_INT_CTRL,   /* (1 << 5) |    //ancififo done int. */
			/* (1 << 4) |  //SOF interrupt enable. */
			/* (1 << 3) | //EOF interrupt enable. */
			(1 << 1)); /* | //input overflow interrupt enable. */
	/* (1 << 0)); //bt656 controller error interrupt enable. */
	bt656_wr(offset, BT_ERR_CNT, (626 << 16) | (2000));
	/* otherwise there is always error flag, */
	/* because the camera input use HREF ont HSYNC, */
	/* there are some lines without HREF sometime */
	bt656_wr(offset, BT_FIELDSADR, (1 << 16) | 1);/* field 0/1 start lcnt */

	if (devp->para.fmt == TVIN_SIG_FMT_BT601IN_576I_50HZ) {
		/* input is PAL */
		/* field 0/1 VBI last line number */
		bt656_wr(offset, BT_VBIEND, 22 | (22 << 16));
		/* Line number of the first video start line in field 0/1. */
		bt656_wr(offset, BT_VIDEOSTART, 23 | (23 << 16));
		/* Line number of the last video line in field 1.
		 * added video end for avoid overflow.
		 */
		bt656_wr(offset, BT_VIDEOEND, 312 |
				(312 << 16));
		/* BT656 standaREAD_CBUS_REG interface. */
		bt656_wr(offset, BT_CTRL,    (0 << BT_MODE_BIT) |
				(1 << BT_AUTO_FMT)     |
				 /* enable BT moduale. */
				(1 << BT_EN_BIT) |
				/* timing reference is from bit stream. */
				(0 << BT_REF_MODE_BIT) |
				(0 << BT_FMT_MODE_BIT) |     /* PAL */
				(1 << BT_SLICE_MODE_BIT) |
				/* use external fid pin. */
				(0 << BT_FID_EN_BIT)   |
				/* use external xclk27. */
				(1 << BT_CLK27_SEL_BIT) |
				 /* xclk27 is input. */
				(1 << BT_XCLK27_EN_BIT));
		/* wr(VDIN_WR_V_START_END, 287 |  */
		/* (0 << 16));    */
	} else {
	/* if(am656in_dec_info.para.fmt == TVIN_SIG_FMT_BT601IN_480I)
	 * input is NTSC
	 */
		 /* field 0/1 VBI last line number */
		bt656_wr(offset, BT_VBIEND, 21 | (21 << 16));
		/* Line number of the first video start line in field 0/1. */
		bt656_wr(offset, BT_VIDEOSTART, 18 | (18 << 16));
		/* Line number of the last video line in field 1.
		 * added video end for avoid overflow.
		 */
		bt656_wr(offset, BT_VIDEOEND, 257 |
				(257 << 16));
		bt656_wr(offset, BT_CTRL, (0 << BT_MODE_BIT) |
				(1 << BT_AUTO_FMT)     |
				 /* enablem656in_star BT moduale. */
				(1 << BT_EN_BIT) |
				/* timing reference is from bit stream. */
				(0 << BT_REF_MODE_BIT) |
				(1 << BT_FMT_MODE_BIT) |     /* NTSC */
				(1 << BT_SLICE_MODE_BIT) |
				/* use external fid pin. */
				(0 << BT_FID_EN_BIT)   |
				 /* use external xclk27. */
				(1 << BT_CLK27_SEL_BIT) |
				 /* xclk27 is input. */
				(1 << BT_XCLK27_EN_BIT));
		/* bt656_wr(VDIN_WR_V_START_END, 239 |   */
		/* (0 << 16));  */

	}
}

/* CAMERA input(progressive mode): CLOCK + D0~D7 + HREF + VSYNC */
static void reinit_camera_dec(struct am656in_dev_s *devp)
{
	unsigned int offset = devp->index;

	/* reset_bt656in_module(); */
	unsigned int temp_data;
	unsigned char hsync_enable = devp->para.hsync_phase;
	unsigned char vsync_enable = devp->para.vsync_phase;
	unsigned short hs_bp       = devp->para.hs_bp;
	unsigned short vs_bp       = devp->para.vs_bp;
	unsigned int h_active      = devp->para.h_active;
	unsigned int v_active      = devp->para.v_active;

	if (is_meson_m8b_cpu()) {
		/* top reset for bt656 */
		/* WRITE_CBUS_REG_BITS(RESET1_REGISTER, 1, 5, 1); */
		/* WRITE_CBUS_REG_BITS(RESET1_REGISTER, 0, 5, 1); */
		aml_cbus_update_bits(RESET1_REGISTER, 0x1<<5, 1);
		aml_cbus_update_bits(RESET1_REGISTER, 0x1<<5, 0);
	}
	/* disable 656,reset */
	bt656_wr(offset, BT_CTRL, 1<<31);

	/*wr(BT_VIDEOSTART, 1 | (1 << 16));
	 *  //Line number of the first video start line in field 0/1.
	 *    there is a blank
	 * wr(BT_VIDEOEND , (am656in_dec_info.active_line ));
	 *  //Line number of the last video line in field 1.
	 *    added video end for avoid overflow.
	 * ((am656in_dec_info.active_line ) << 16));
	 */
	/* Line number of the last video line in field 0 */

	/* use external idq pin. */
	bt656_wr(offset, BT_PORT_CTRL, (0 << BT_IDQ_EN)   |
			(0 << BT_IDQ_PHASE)   |
			 /* FID came from HS VS. */
			(0 << BT_FID_HSVS)    |
			(1 << BT_ACTIVE_HMODE) |
			(vsync_enable << BT_VSYNC_PHASE) |
			(hsync_enable << BT_HSYNC_PHASE) |
			(0 << BT_D8B)         |
			(4 << BT_FID_DELAY)   | /* 4 */
			(0 << BT_VSYNC_DELAY)  |
			(2 << BT_HSYNC_DELAY)); /* 2 */

	/* WRITE_CBUS_REG(BT_PORT_CTRL,0x421001); */
	/* FID field check done point. */
	bt656_wr(offset, BT_601_CTRL2, (10 << 16));

	bt656_wr(offset, BT_SWAP_CTRL,
			(5 << 0) |        /* POS_Cb0_IN */
			(4 << 4) |        /* POS_Y0_IN */
			(7 << 8) |        /* POS_Cr0_IN */
			(6 << 12));       /* POS_Y1_IN */

	/* horizontal active data start offset, *2 for 422 sampling */
	bt656_wr(offset, BT_LINECTRL, (1 << 31) |
			(((h_active + hs_bp) << 1) << 16) |
			(hs_bp << 1));/* horizontal active data start offset */

	/* ANCI is the field blanking data, like close caption.
	 * If it connected to digital camara interface,
	 *  the jpeg bitstream also use this ANCI FIFO.
	 */
	/* bt656_wr(BT_ANCISADR, devp->mem_start); */
	/* bt656_wr(BT_ANCIEADR, 0);//devp->mem_start + devp->mem_size); */

	bt656_wr(offset, BT_AFIFO_CTRL,   (1 << 31) |
			(1 << 6) |     /* fill _en; */
			(1 << 3));     /* urgent */

	bt656_wr(offset, BT_INT_CTRL,   /* (1 << 5) |    //ancififo done int. */
			/* (1 << 4) |    //SOF interrupt enable. */
			/* (1 << 3) |      //EOF interrupt enable. */
			(1 << 1));      /* input overflow interrupt enable. */
	/* (1 << 0));      //bt656 controller error interrupt enable. */

	 /* total lines per frame and total pixel per line */
	bt656_wr(offset, BT_ERR_CNT, ((2000) << 16) | (2000 * 10));
	/* otherwise there is always error flag, */
	/* because the camera input use HREF ont HSYNC, */
	/* there are some lines without HREF sometime */

	/* field 0/1 start lcnt */
	bt656_wr(offset, BT_FIELDSADR, (1 << 16) | 1);

	/* field 0/1 VBI last line number */
	bt656_wr(offset, BT_VBISTART, 1 | (1 << 16));
	/* field 0/1 VBI last line number */
	bt656_wr(offset, BT_VBIEND,   1 | (1 << 16));

	/* Line number of the first video start
	 * line in field 0/1.there is a blank
	 */
	bt656_wr(offset, BT_VIDEOSTART, vs_bp | (vs_bp << 16));
	/* Line number of the last video line in field 1.
	 *  added video end for avoid overflow.
	 */
	/* Line number of the last video line in field 0 */
	bt656_wr(offset, BT_VIDEOEND,
		((v_active + vs_bp - 1) |
		((v_active + vs_bp - 1) << 16)));

	bt656_wr(offset, BT_DELAY_CTRL, 0x22222222);

	/* enable BTR656 interface */
	if (devp->para.isp_fe_port == TVIN_PORT_CAMERA) {
		temp_data = (1 << BT_EN_BIT) /* enable BT moduale. */
			/* timing reference is from bit stream. */
			|(0 << BT_REF_MODE_BIT)
			|(0 << BT_FMT_MODE_BIT)      /* PAL */
			|(0 << BT_SLICE_MODE_BIT)   /* no ancillay flag. */
			/* BT656 standard interface. */
			|(0 << BT_MODE_BIT)
			|(1 << BT_CLOCK_ENABLE)	/* enable 656 clock. */
			|(0 << BT_FID_EN_BIT)	/* use external fid pin. */
			/* xclk27 is input. change to Raw_mode setting from M8*/
			/* |(0 << BT_XCLK27_EN_BIT)      */
			|(0 << BT_PROG_MODE)
			|(0 << BT_AUTO_FMT)
			|(0 << BT_CAMERA_MODE)     /* enable camera mode */
			|(1 << BT_656CLOCK_RESET)
			|(1 << BT_SYSCLOCK_RESET)
			|(1 << BT_RAW_MODE) /* enable raw data output */
			|(1 << BT_RAW_ISP) /* enable raw data to isp */
			|(0 << 28) /* enable csi2 pin */
		    ;
	} else {
		temp_data =
			/* BT656 standard interface. */
			(0 << BT_MODE_BIT)
			/* camera mode */
			|(1 << BT_CAMERA_MODE)
			/* enable BT data input */
			|(1 << BT_EN_BIT)
			/* timing reference is from bit stream. */
			|(0 << BT_REF_MODE_BIT)
			|(0 << BT_FMT_MODE_BIT)     /* NTSC */
			|(0 << BT_SLICE_MODE_BIT)
			/* use external fid pin. */
			|(0 << BT_FID_EN_BIT)
			/* enable bt656 clock input */
			|(1 << 7)
			|(0 << 18)              /* eol_delay //4 */
			|(1 << 16) /* update status for debug */
			/* enable sys clock input */
			|(1 << BT_XCLK27_EN_BIT)
			|(0x60000000);   /* out of reset for BT_CTRL. */
	}
	if ((devp->para.bt_path == BT_PATH_GPIO) ||
	    (devp->para.bt_path == BT_PATH_GPIO_B)) {
		temp_data &= (~(1<<28));
		bt656_wr(offset, BT_CTRL, temp_data);
	} else if (devp->para.bt_path == BT_PATH_CSI2) {
#ifdef CONFIG_TVIN_ISP
		temp_data |= (1<<28);
		bt656_wr(offset, BT_CTRL, temp_data);
		/* power on mipi csi phy */
		aml_write_cbus(HHI_CSI_PHY_CNTL0, 0xfdc1ff81);
		aml_write_cbus(HHI_CSI_PHY_CNTL1, 0x3fffff);
		temp_data = aml_read_cbus(HHI_CSI_PHY_CNTL2);
		temp_data &= 0x7ff00000;
		temp_data |= 0x80000fc0;
		aml_write_cbus(HHI_CSI_PHY_CNTL2, temp_data);
#endif
	}
}

/* bt601_hdmi input(progressive or interlace): CLOCK + D0~D7 + HSYNC + VSYNC */
static void reinit_bt601_hdmi_dec(struct am656in_dev_s *devp)
{
	unsigned int offset = devp->index;

	/* reset_bt656in_module(); */
	unsigned int temp_data;
	unsigned char hsync_enable = devp->para.hsync_phase;
	unsigned char vsync_enable = devp->para.vsync_phase;
	unsigned short hs_bp       = devp->para.hs_bp;
	unsigned short vs_bp       = devp->para.vs_bp;
	unsigned int h_active      = devp->para.h_active;
	unsigned int v_active      = devp->para.v_active;
	unsigned int fid_check_cnt = devp->para.fid_check_cnt;

#if 0
	pr_info("para hsync_phase:   %d\n"
		"para vsync_phase:   %d\n"
		"para hs_bp:         %d\n"
		"para vs_bp:         %d\n"
		"para h_active:      %d\n"
		"para v_active:      %d\n",
		hsync_enable, vsync_enable,
		hs_bp, vs_bp, h_active, v_active);
#endif

	/* disable 656,reset */
	bt656_wr(offset, BT_CTRL, 1<<31);

	bt656_wr(offset, BT_PORT_CTRL, (0 << BT_IDQ_EN)   |
			(0 << BT_IDQ_PHASE)     |
			(1 << BT_ACTIVE_HMODE)  |
			(vsync_enable << BT_VSYNC_PHASE) |
			(hsync_enable << BT_HSYNC_PHASE) |
			(0 << BT_D8B)           |
			(0 << BT_FID_DELAY)     |
			(0 << BT_FID_HSVS_PCNT) | /* use hs/vs */
			(1 << BT_FID_HSVS)      | /* generate fid */
			(0 << BT_VSYNC_DELAY)   |
			(0 << BT_HSYNC_DELAY));

	/* FID field check done point. */
	bt656_wr(offset, BT_601_CTRL2, (fid_check_cnt << 16));

	bt656_wr(offset, BT_SWAP_CTRL,
			(4 << 0) |        /* POS_Cb0_IN */
			(5 << 4) |        /* POS_Y0_IN */
			(6 << 8) |        /* POS_Cr0_IN */
			(7 << 12));       /* POS_Y1_IN */

	/* horizontal active data start offset, *2 for 422 sampling */
	bt656_wr(offset, BT_LINECTRL, (1 << 31) |
			(((h_active + hs_bp) << 1) << 16) |
			(hs_bp << 1));

	bt656_wr(offset, BT_AFIFO_CTRL,   (1 << 31) |
			(1 << 6) |     /* fill _en; */
			(1 << 3));     /* urgent */

	bt656_wr(offset, BT_INT_CTRL,    /* (1 << 5) |   //ancififo done int. */
			/* (1 << 4) |    //SOF interrupt enable. */
			/* (1 << 3) |      //EOF interrupt enable. */
			(1 << 1));      /* input overflow interrupt enable. */
		/* (1 << 0));      //bt656 controller error interrupt enable. */

	 /* total lines per frame and total pixel per line */
	bt656_wr(offset, BT_ERR_CNT, ((2000) << 16) | (2000 * 10));
	/* otherwise there is always error flag, */
	/* because the camera input use HREF ont HSYNC, */
	/* there are some lines without HREF sometime */

	/* field 0/1 start lcnt */
	bt656_wr(offset, BT_FIELDSADR, (0 << 16) | 0);

	/* field 0/1 VBI last line number */
	bt656_wr(offset, BT_VBISTART, 0 | (0 << 16));
	/* field 0/1 VBI last line number */
	bt656_wr(offset, BT_VBIEND,   0 | (0 << 16));

	/* Line number of the first video start
	 * line in field 0/1.there is a blank
	 */
	bt656_wr(offset, BT_VIDEOSTART, vs_bp | (vs_bp << 16));
	/* Line number of the last video line in field 1.
	 *  added video end for avoid overflow.
	 */
	/* Line number of the last video line in field 0 */
	bt656_wr(offset, BT_VIDEOEND,
		((v_active + vs_bp - 1) |
		((v_active + vs_bp - 1) << 16)));

	bt656_wr(offset, BT_DELAY_CTRL, 0x22222222);

	/* enable BTR656 interface */
	temp_data =
		/* BT656 standard interface. */
		(0 << BT_MODE_BIT)
		/* camera mode */
		|(1 << BT_CAMERA_MODE)
		/* enable BT data input */
		|(1 << BT_EN_BIT)
		/* timing reference is from bit stream. */
		|(0 << BT_REF_MODE_BIT)
		|(0 << BT_FMT_MODE_BIT)
		|(0 << BT_SLICE_MODE_BIT)
		/* use external fid pin. */
		|(0 << BT_FID_EN_BIT)
		/* enable bt656 clock input */
		|(1 << 7)
		|(0 << 18)              /* eol_delay */
		|(1 << 16) /* update status for debug */
		/* enable sys clock input */
		|(1 << BT_XCLK27_EN_BIT)
		|(0x60000000);   /* out of reset for BT_CTRL. */

	temp_data &= (~(1<<28));
	bt656_wr(offset, BT_CTRL, temp_data);
}

static void start_amvdec_656_601_camera_in(struct am656in_dev_s *devp)
{
	enum tvin_port_e port =  devp->para.port;

	if (devp->dec_status & TVIN_AM656_RUNNING) {
		BT656PR("%s: bt656 have started alreadly.\n", __func__);
		return;
	}
	devp->dec_status = TVIN_AM656_RUNNING;
	/* NTSC or PAL input(interlace mode): D0~D7(with SAV + EAV ) */
	if (port == TVIN_PORT_BT656) {
		devp->para.fmt = TVIN_SIG_FMT_BT656IN_576I_50HZ;
		init_656in_dec_parameter(devp);
		reinit_bt656in_dec(devp);
		/* reset_656in_dec_parameter(); */
		devp->dec_status = TVIN_AM656_RUNNING;
	} else if (port == TVIN_PORT_BT601) {
		devp->para.fmt = TVIN_SIG_FMT_BT601IN_576I_50HZ;
		init_656in_dec_parameter(devp);
		reinit_bt601in_dec(devp);
		devp->dec_status = TVIN_AM656_RUNNING;

	} else if (port == TVIN_PORT_CAMERA) {
		init_656in_dec_parameter(devp);
		reinit_camera_dec(devp);
		devp->dec_status = TVIN_AM656_RUNNING;
	} else if (port == TVIN_PORT_BT656_HDMI) {
		init_656in_dec_hdmi_parameter(devp);
		reinit_bt601_hdmi_dec(devp);
		devp->dec_status = TVIN_AM656_RUNNING;
	} else if (port == TVIN_PORT_BT601_HDMI) {
		init_656in_dec_hdmi_parameter(devp);
		reinit_bt601_hdmi_dec(devp);
		devp->dec_status = TVIN_AM656_RUNNING;
	} else {
		devp->para.fmt  = TVIN_SIG_FMT_NULL;
		devp->para.port = TVIN_PORT_NULL;
		BT656ERR("%s: input is not selected, please try again.\n",
			__func__);
		return;
	}
	BT656PR("bt656[%d](%s): %s input port: %s fmt: %s.\n",
		devp->index, BT656_VER, __func__,
		tvin_port_str(devp->para.port),
		tvin_sig_fmt_str(devp->para.fmt));
}

static void stop_amvdec_bt656(struct am656in_dev_s *devp)
{
	if (devp->dec_status & TVIN_AM656_RUNNING) {
		reset_bt656in_module(devp);
		devp->dec_status = TVIN_AM656_STOP;
	} else {
		BT656PR("bt656[%d]:%s device is not started yet.\n",
			devp->index, __func__);
	}
}

static void am656_clktree_control(struct am656in_dev_s *devp, int flag)
{
	BT656PR("bt656[%d]:%s: flag=%d\n", devp->index, __func__, flag);
	if (flag) { /* clk enable */
		if (IS_ERR(devp->gate_bt656)) {
			BT656ERR("bt656[%d] %s: gate_bt656\n",
				devp->index, __func__);
		} else {
			clk_prepare_enable(devp->gate_bt656);
		}
		if (IS_ERR(devp->gate_bt656_pclk)) {
			BT656ERR("bt656[%d] %s: gate_bt656_pclk\n",
				devp->index, __func__);
		} else {
			clk_prepare_enable(devp->gate_bt656_pclk);
		}
		if (IS_ERR(devp->bt656_clk)) {
			BT656ERR("bt656[%d] %s: bt656_clk\n",
				devp->index, __func__);
		} else {
			clk_prepare_enable(devp->bt656_clk);
		}
		udelay(100);
	} else { /* clk disable */
		if (IS_ERR(devp->bt656_clk)) {
			BT656ERR("bt656[%d] %s: bt656_clk\n",
				devp->index, __func__);
		} else {
			clk_disable_unprepare(devp->bt656_clk);
		}
		if (IS_ERR(devp->gate_bt656_pclk)) {
			BT656ERR("bt656[%d] %s: gate_bt656_pclk\n",
				devp->index, __func__);
		} else {
			clk_disable_unprepare(devp->gate_bt656_pclk);
		}
		if (IS_ERR(devp->gate_bt656)) {
			BT656ERR("bt656[%d] %s: gate_bt656\n",
				devp->index, __func__);
		} else {
			clk_disable_unprepare(devp->gate_bt656);
		}
	}
}

/*
 * return true when need skip frame otherwise return false
 */
static bool am656_check_skip_frame(struct tvin_frontend_s *fe)
{
	struct am656in_dev_s *devp;

	devp =  container_of(fe, struct am656in_dev_s, frontend);
	if (devp->skip_vdin_frame_count > 0) {
		devp->skip_vdin_frame_count--;
		return true;
	} else
		return false;
}
int am656in_support(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	if ((port < TVIN_PORT_BT656) || (port > TVIN_PORT_BT601_HDMI))
		return -1;
	else
		return 0;
}

static int am656in_open(struct inode *node, struct file *file)
{
	struct am656in_dev_s *devp;

	/* Get the per-device structure that contains this cdev */
	devp = container_of(node->i_cdev, struct am656in_dev_s, cdev);

	if (devp->index >= hw_cnt)
		return -ENXIO;

	file->private_data = devp;

	BT656PR("open device %s ok\n", dev_name(devp->dev));

	return 0;
}
static int am656in_release(struct inode *node, struct file *file)
{
	/* struct vdin_dev_s *devp = file->private_data; */

	file->private_data = NULL;

	/* BT656PR("close device %s ok\n", dev_name(devp->dev)); */

	return 0;
}

static const struct file_operations am656in_fops = {
	.owner    = THIS_MODULE,
	.open     = am656in_open,
	.release  = am656in_release,
};
/*called by vdin && sever for v4l2 framework*/

void am656in_start(struct tvin_frontend_s *fe, enum tvin_sig_fmt_e fmt)
{
	struct am656in_dev_s *devp;

	devp = container_of(fe, struct am656in_dev_s, frontend);

	mutex_lock(&bt656_mutex);
	start_amvdec_656_601_camera_in(devp);
	mutex_unlock(&bt656_mutex);

}
static void am656in_stop(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct am656in_dev_s *devp;

	devp = container_of(fe, struct am656in_dev_s, frontend);
	if ((port < TVIN_PORT_BT656) || (port > TVIN_PORT_BT601_HDMI)) {
		BT656ERR("%s: invaild port %d.\n", __func__, port);
		return;
	}

	mutex_lock(&bt656_mutex);
	stop_amvdec_bt656(devp);
	BT656PR("bt656[%d] %s stop device stop ok.\n", devp->index, __func__);
	mutex_unlock(&bt656_mutex);
}
static void am656in_get_sig_property(struct tvin_frontend_s *fe,
		struct tvin_sig_property_s *prop)
{
	struct am656in_dev_s *devp;

	devp = container_of(fe, struct am656in_dev_s, frontend);
	prop->color_format = devp->para.cfmt;
	prop->dest_cfmt    = devp->para.dfmt;
	prop->decimation_ratio = 0;
}

/*as use the spin_lock,
 *1--there is no sleep,
 *2--it is better to shorter the time,
 */
int am656in_isr(struct tvin_frontend_s *fe, unsigned int hcnt)
{
	unsigned int ccir656_status = 0;
	struct am656in_dev_s *devp;
	unsigned int offset;

	devp = container_of(fe, struct am656in_dev_s, frontend);
	offset = devp->index;
	ccir656_status = bt656_rd(offset, BT_STATUS);
	if (ccir656_status & 0xf0)   /* AFIFO OVERFLOW */
		devp->overflow_cnt++;
	if (devp->overflow_cnt > 5) {
		devp->overflow_cnt = 0;
		 /* NTSC or PAL input(interlace mode): D0~D7(with SAV + EAV ) */
		if (devp->para.port == TVIN_PORT_BT656)
			reinit_bt656in_dec(devp);
		else if (devp->para.port == TVIN_PORT_BT601)
			reinit_bt601in_dec(devp);
		else if (devp->para.port == TVIN_PORT_BT656_HDMI)
			reinit_bt601_hdmi_dec(devp);
		else if (devp->para.port == TVIN_PORT_BT601_HDMI)
			reinit_bt601_hdmi_dec(devp);
		else /* if(am656in_dec_info.para.port == TVIN_PORT_CAMERA) */
			reinit_camera_dec(devp);
		 /* WRITE_CBUS_REGite 1 to clean the SOF interrupt bit */
		bt656_wr(offset, BT_STATUS, ccir656_status | (1 << 9));
		BT656PR("bt656[%d] %s bt656in fifo overflow.\n",
			devp->index, __func__);
	}
	return 0;
}

/*
 *power on 656 module&init the parameters,such as
 *power color fmt...,will be used by vdin
 */
static int am656in_feopen(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct am656in_dev_s *devp;
	struct vdin_parm_s *parm = fe->private_data;

	devp = container_of(fe, struct am656in_dev_s, frontend);

	if ((port < TVIN_PORT_BT656) || (port > TVIN_PORT_BT601_HDMI)) {
		BT656ERR("bt656[%d] %s:invaild port %d.\n",
				devp->index, __func__, port);
		return -1;
	}
	if (port == TVIN_PORT_CAMERA)
		devp->skip_vdin_frame_count = parm->skip_count;

	/*copy the param from vdin to bt656*/
	if (!memcpy(&devp->para, parm, sizeof(struct vdin_parm_s))) {
		BT656ERR("bt656[%d] %s memcpy error\n", devp->index, __func__);
		return -1;
	}
	/*avoidint the param port is't equal with port*/
	devp->para.port = port;
	BT656PR("bt656[%d]:%s: color fmt:%s, hs phase:%u, vs phase:%u\n",
		devp->index, __func__,
		tvin_color_fmt_str(parm->cfmt),
		parm->hsync_phase, parm->vsync_phase);
	BT656PR("frame rate:%u, hs_bp:%u, vs_bp:%u\n",
		parm->frame_rate, parm->hs_bp, parm->vs_bp);

	/* bt656 clock enable */
	am656_clktree_control(devp, 1);

	return 0;
}
/*
 *power off the 656 module,clear the parameters
 */
static void am656in_feclose(struct tvin_frontend_s *fe)
{

	struct am656in_dev_s *devp = NULL;
	enum tvin_port_e port = 0;

	devp = container_of(fe, struct am656in_dev_s, frontend);
	port = devp->para.port;

	if ((port < TVIN_PORT_BT656) || (port > TVIN_PORT_BT601_HDMI)) {
		BT656ERR("bt656[%d] %s:invaild port %d.\n",
				devp->index, __func__, port);
		return;
	}

	memset(&devp->para, 0, sizeof(struct vdin_parm_s));

	/* bt656 clock disable */
	am656_clktree_control(devp, 0);
}
static struct tvin_state_machine_ops_s am656_machine_ops = {
	.nosig               = NULL,
	.fmt_changed         = NULL,
	.get_fmt             = NULL,
	.fmt_config          = NULL,
	.adc_cal             = NULL,
	.pll_lock            = NULL,
	.get_sig_property    = am656in_get_sig_property,
	.vga_set_param       = NULL,
	.vga_get_param       = NULL,
	.check_frame_skip    = am656_check_skip_frame,
};
static struct tvin_decoder_ops_s am656_decoder_ops_s = {
	.support                = am656in_support,
	.open                   = am656in_feopen,
	.start                  = am656in_start,
	.stop                   = am656in_stop,
	.close                  = am656in_feclose,
	.decode_isr             = am656in_isr,
};

static int bt656_add_cdev(struct cdev *cdevp,
		const struct file_operations *fops, int minor)
{
	int ret;
	dev_t devno = MKDEV(MAJOR(am656in_devno), minor);

	cdev_init(cdevp, fops);
	cdevp->owner = THIS_MODULE;
	ret = cdev_add(cdevp, devno, 1);
	return ret;
}

static struct device *bt656_create_device(struct device *parent, int minor)
{
	dev_t devno = MKDEV(MAJOR(am656in_devno), minor);

	return device_create(am656in_clsp, parent, devno, NULL, "%s%d",
			BT656_DEV_NAME, minor);
}

static void bt656_delete_device(int minor)
{
	dev_t devno = MKDEV(MAJOR(am656in_devno), minor);

	device_destroy(am656in_clsp, devno);
}

static int amvdec_656in_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct am656in_dev_s *devp = NULL;
	int bt656_rate;
	struct resource *res = 0;
	int size = 0;
	int index = 0;
	struct device_node *child;
	const char *str;
	char bt656_clk_name[20];

	BT656PR("%s: start probe .\n", __func__);

	if (is_meson_gxtvbb_cpu() || is_meson_gxl_cpu() ||
		is_meson_gxm_cpu() || is_meson_g12a_cpu() ||
		is_meson_g12b_cpu() || is_meson_tl1_cpu() ||
		is_meson_tm2_cpu()) {
		hw_cnt = 1;
	} else if (is_meson_gxbb_cpu()) {
		hw_cnt = 2;
	} else {
		hw_cnt = 0;
	}

	if (hw_cnt == 0) {
		BT656ERR("no bt656 support for current chip\n");
		return -EFAULT;
	}
	BT656PR("%s. %d hardware detected!.\n", __func__, hw_cnt);

	mutex_init(&bt656_mutex);

	ret = alloc_chrdev_region(&am656in_devno, 0, hw_cnt, BT656_DEV_NAME);
	if (ret < 0) {
		BT656ERR("%s: failed to alloc major number\n", __func__);
		goto fail_alloc_cdev_region;
	}
	BT656PR("%s: major %d\n", __func__, MAJOR(am656in_devno));

	am656in_clsp = class_create(THIS_MODULE, BT656_CLS_NAME);
	if (IS_ERR(am656in_clsp)) {
		ret = PTR_ERR(am656in_clsp);
		BT656ERR("%s: failed to create class\n", __func__);
		goto fail_class_create;
	}

	for_each_child_of_node(pdev->dev.of_node, child) {
		ret = of_property_read_u32(child, "bt656_id", &index);
		if (ret) {
			BT656ERR("%s: don't find  bt656 id.\n", __func__);
			goto fail_kmalloc_dev;
		}
		if ((index == 0) || (index > 2)) {
			BT656ERR("%s: invalid bt656 index %d.\n",
				__func__, index);
			goto fail_kmalloc_dev;
		}

		ret = of_property_read_string(child, "status", &str);
		if (ret) {
			BT656ERR("%s: bt656[%d] don't find status, disabled\n",
				__func__, index);
			continue;
		} else {
			if (strncmp(str, "okay", 2))
				continue;
		}

		/* malloc dev */
		am656in_devp[index-1] =
			kmalloc(sizeof(struct am656in_dev_s), GFP_KERNEL);
		if (!am656in_devp[index-1]) {
			BT656ERR("%s: devp malloc error\n", __func__);
			goto fail_kmalloc_dev;
		}
		memset(am656in_devp[index-1], 0, sizeof(struct am656in_dev_s));

		devp = am656in_devp[index-1];
		devp->index = index;
		BT656PR("%s: bt656 devp->index =%d\n", __func__, devp->index);

		/* create cdev and register with sysfs */
		ret = bt656_add_cdev(&devp->cdev, &am656in_fops, devp->index);
		if (ret) {
			BT656ERR("%s: failed to add cdev\n", __func__);
			goto fail_add_cdev;
		}
		devp->dev = bt656_create_device(&pdev->dev, devp->index);
		if (IS_ERR(devp->dev)) {
			BT656ERR("%s: failed to create device\n", __func__);
			ret = PTR_ERR(devp->dev);
			goto fail_create_device;
		}

		ret = device_create_file(devp->dev, &dev_attr_reg);
		if (ret < 0) {
			BT656ERR("fail to create dbg attribute file\n");
			goto fail_create_file;
		}

		/* set clktree */
		sprintf(bt656_clk_name, "cts_bt656_clk%d", devp->index);
		devp->bt656_clk = clk_get(&pdev->dev, bt656_clk_name);
		clk_set_rate(devp->bt656_clk, 333333333);
		if (!IS_ERR(devp->bt656_clk)) {
			bt656_rate = clk_get_rate(devp->bt656_clk);
			/*clk_put(devp->bt656_clk);*/
			BT656PR("%s: bt656[%d] clock is %d MHZ\n",
				__func__,
				devp->index, bt656_rate/1000000);
		} else {
			BT656ERR("%s: bt656[%d] cannot get %s !!!\n",
				__func__, devp->index, bt656_clk_name);
			ret = -ENOENT;
			goto fail_get_clktree;
		}

		devp->gate_bt656 = clk_get(&pdev->dev, "clk_gate_bt656");
		if (IS_ERR(devp->gate_bt656)) {
			BT656ERR("%s: cannot get clk_gate_bt656 !!!\n",
				__func__);
			ret = -ENOENT;
			goto fail_get_clktree;
		}

		devp->gate_bt656_pclk = clk_get(&pdev->dev,
				"clk_gate_bt656_pclk1");
		if (IS_ERR(devp->gate_bt656_pclk)) {
			BT656ERR("%s: cannot get clk_gate_bt656_pclk1 !!!\n",
				__func__);
			//ret = -ENOENT;
			//goto fail_get_clktree;
		}
		/* set regmap */
		BT656PR("%s: bt656[%d] start get ioremap .\n",
			__func__, devp->index);
		res = platform_get_resource(pdev, IORESOURCE_MEM,
				(devp->index - 1));
		if (!res) {
			BT656ERR("missing memory resource\n");
			ret = -ENOMEM;
			goto fail_get_clktree;
		}

		size = resource_size(res);
		bt656_reg_base[devp->index-1] =
			devm_ioremap_nocache(&pdev->dev, res->start, size);
		if (!bt656_reg_base[devp->index-1]) {
			BT656ERR("bt656[%d] ioremap failed\n", devp->index);
			ret = -ENOMEM;
			goto fail_get_clktree;
		}
		BT656PR("%s: bt656[%d] mapped reg_base=0x%p, size=0x%x\n",
			__func__, devp->index,
			bt656_reg_base[devp->index-1], size);

		/* set drvdata */
		dev_set_drvdata(devp->dev, devp);

		/*register frontend */
		sprintf(devp->frontend.name, "%s%d",
			BT656_DEV_NAME, devp->index);
		/* tvin_frontend_init(&devp->frontend,
		 * &am656_decoder_ops_s, &am656_machine_ops, pdev->id);
		 */
		if (!tvin_frontend_init(&devp->frontend, &am656_decoder_ops_s,
			&am656_machine_ops, devp->index-1)) {
			BT656PR("%s: tvin_frontend_init done :%d\n",
				__func__, devp->index);
			if (tvin_reg_frontend(&devp->frontend)) {
				BT656ERR("%s register frontend error\n",
					__func__);
			}
		}
	}
	BT656PR("driver probe ok.\n");

	return ret;

/*
 *fail_get_resource_mem:
 *	devm_release_mem_region(&pdev->dev, res->start, size);
 */
fail_get_clktree:
	device_remove_file(devp->dev, &dev_attr_reg);
fail_create_file:
	bt656_delete_device(pdev->id);
fail_create_device:
	cdev_del(&devp->cdev);
fail_add_cdev:
	kfree(devp);
fail_kmalloc_dev:
	class_destroy(am656in_clsp);
fail_class_create:
	unregister_chrdev_region(am656in_devno, hw_cnt);
fail_alloc_cdev_region:
	return ret;
}

static int amvdec_656in_remove(struct platform_device *pdev)
{
	struct am656in_dev_s *devp;
	int index;

	for (index = 0; index < hw_cnt; index++) {
		devp = am656in_devp[index];
		if (devp == NULL)
			continue;
		device_remove_file(devp->dev, &dev_attr_reg);
		tvin_unreg_frontend(&devp->frontend);
		bt656_delete_device(pdev->id);
		cdev_del(&devp->cdev);
		kfree((const void *)devp);
	}
	class_destroy(am656in_clsp);
	unregister_chrdev_region(am656in_devno, hw_cnt);

	return 0;
}

static int amvdec_656in_resume(struct platform_device *pdev)
{
	BT656PR("%s\n", __func__);
	return 0;
}

static int amvdec_656in_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct am656in_dev_s *devp;
	int index;

	mutex_lock(&bt656_mutex);
	BT656PR("%s\n", __func__);
	for (index = 0; index < hw_cnt; index++) {
		devp = am656in_devp[index];
		if (devp == NULL)
			continue;
		if (devp->dec_status & TVIN_AM656_RUNNING) {
			reset_bt656in_module(devp);
			devp->dec_status = TVIN_AM656_STOP;
			am656_clktree_control(devp, 0);
		}
	}
	mutex_unlock(&bt656_mutex);
	return 0;
}

static void amvdec_656in_shutdown(struct platform_device *pdev)
{
	struct am656in_dev_s *devp;
	int index;

	BT656PR("%s\n", __func__);
	for (index = 0; index < hw_cnt; index++) {
		devp = am656in_devp[index];
		if (devp == NULL)
			continue;
		if (devp->dec_status & TVIN_AM656_RUNNING) {
			reset_bt656in_module(devp);
			devp->dec_status = TVIN_AM656_STOP;
			am656_clktree_control(devp, 0);
		}
	}
}


#ifdef CONFIG_OF
static const struct of_device_id bt656_dt_match[] = {
	{
		.compatible = "amlogic, amvdec_656in",
	},
	{},
};
#endif

static struct platform_driver amvdec_656in_driver = {
	.probe  = amvdec_656in_probe,
	.remove = amvdec_656in_remove,
	.suspend = amvdec_656in_suspend,
	.resume = amvdec_656in_resume,
	.shutdown = amvdec_656in_shutdown,
	.driver	= {
		.name           = BT656_DRV_NAME,
		.owner          = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = bt656_dt_match,
#endif
	}
};

static int __init amvdec_656in_init_module(void)
{
	int ret = 0;

	ret = platform_driver_register(&amvdec_656in_driver);
	if (ret != 0) {
		BT656ERR("%s: failed to register driver\n", __func__);
		return -1;
	}

	return 0;
}

static void __exit amvdec_656in_exit_module(void)
{
	platform_driver_unregister(&amvdec_656in_driver);
}

module_init(amvdec_656in_init_module);
module_exit(amvdec_656in_exit_module);

MODULE_DESCRIPTION("AMLOGIC BT656_601 input driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.0.0");

