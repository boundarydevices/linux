/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmirx_drv.c
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
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/poll.h>
#include <linux/io.h>
#include <linux/suspend.h>
/* #include <linux/earlysuspend.h> */
#include <linux/delay.h>
#include <linux/of_device.h>

/* Amlogic headers */
/*#include <linux/amlogic/amports/vframe_provider.h>*/
/*#include <linux/amlogic/amports/vframe_receiver.h>*/
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
/*#include <linux/amlogic/amports/vframe.h>*/
#include <linux/of_gpio.h>

/* Local include */
#include "hdmi_rx_drv.h"
#include "hdmi_rx_wrapper.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_pktinfo.h"
#include "hdmi_rx_edid.h"
#include "hdmi_rx_eq.h"
#include "hdmi_rx_repeater.h"
/*------------------------extern function------------------------------*/
static int aml_hdcp22_pm_notify(struct notifier_block *nb,
	unsigned long event, void *dummy);
/*------------------------extern function end------------------------------*/

/*------------------------macro define------------------------------*/
#define TVHDMI_NAME			"hdmirx"
#define TVHDMI_DRIVER_NAME		"hdmirx"
#define TVHDMI_MODULE_NAME		"hdmirx"
#define TVHDMI_DEVICE_NAME		"hdmirx"
#define TVHDMI_CLASS_NAME		"hdmirx"
#define INIT_FLAG_NOT_LOAD		0x80

#define HDMI_DE_REPEAT_DONE_FLAG	0xF0
#define FORCE_YUV			1
#define FORCE_RGB			2
#define DEF_LOG_BUF_SIZE		0 /* (1024*128) */
#define PRINT_TEMP_BUF_SIZE		64 /* 128 */

/*------------------------macro define end------------------------------*/

/*------------------------variable define------------------------------*/
static unsigned char init_flag;
static dev_t	hdmirx_devno;
static struct class	*hdmirx_clsp;
/* static int open_flage; */
struct device *hdmirx_dev;
struct delayed_work     eq_dwork;
struct workqueue_struct *eq_wq;
struct delayed_work	esm_dwork;
struct workqueue_struct	*esm_wq;
struct delayed_work	repeater_dwork;
struct workqueue_struct	*repeater_wq;
unsigned int hdmirx_addr_port;
unsigned int hdmirx_data_port;
unsigned int hdmirx_ctrl_port;
/* attr */
static unsigned char *hdmirx_log_buf;
static unsigned int  hdmirx_log_wr_pos;
static unsigned int  hdmirx_log_rd_pos;
static unsigned int  hdmirx_log_buf_size;
unsigned int pwr_sts;
unsigned char *pEdid_buffer;
struct tasklet_struct rx_tasklet;
uint32_t *pd_fifo_buf;
static DEFINE_SPINLOCK(rx_pr_lock);
DECLARE_WAIT_QUEUE_HEAD(query_wait);

int hdmi_yuv444_enable;
module_param(hdmi_yuv444_enable, int, 0664);
MODULE_PARM_DESC(hdmi_yuv444_enable, "hdmi_yuv444_enable");

static int force_color_range;
MODULE_PARM_DESC(force_color_range, "\n force_color_range\n");
module_param(force_color_range, int, 0664);

int pc_mode_en;
MODULE_PARM_DESC(pc_mode_en, "\n pc_mode_en\n");
module_param(pc_mode_en, int, 0664);

bool en_4096_2_3840;
int en_4k_2_2k;
int en_4k_timing = 1;
bool hdmi_cec_en;
int skip_frame_cnt = 1;
/* suspend_pddq_sel:
 * 0: keep phy on when suspend(don't need phy init when
 *   resume), it doesn't work now because phy VDDIO_3.3V
 *   will power off when suspend, and tmds clk will be low;
 * 1&2: when CEC off there's no SDA low issue for MTK box,
 *   these workaround are not needed
 * 1: disable phy when suspend, set rxsense 1 and 0 when resume to
 *   release DDC from hdcp2.2 for MTK box, as LG 49UB8800-CE does
 * 2: disable phy when suspend, set rxsense 1 and 0 when suspend
 *   to release DDC from hdcp2.2 for MTK xiaomi box
 * other value: keep previous logic
 */
int suspend_pddq_sel = 1;

struct reg_map reg_maps[MAP_ADDR_MODULE_NUM];

static struct notifier_block aml_hdcp22_pm_notifier = {
	.notifier_call = aml_hdcp22_pm_notify,
};

static struct meson_hdmirx_data rx_txhd_data = {
	.chip_id = CHIP_ID_TXHD,
};

static struct meson_hdmirx_data rx_txlx_data = {
	.chip_id = CHIP_ID_TXLX,
};

static struct meson_hdmirx_data rx_txl_data = {
	.chip_id = CHIP_ID_TXL,
};

static struct meson_hdmirx_data rx_gxtvbb_data = {
	.chip_id = CHIP_ID_GXTVBB,
};

static const struct of_device_id hdmirx_dt_match[] = {
	{
		.compatible     = "amlogic, hdmirx_txhd",
		.data           = &rx_txhd_data
	},
	{
		.compatible     = "amlogic, hdmirx_txlx",
		.data           = &rx_txlx_data
	},
	{
		.compatible     = "amlogic, hdmirx_txl",
		.data           = &rx_txl_data
	},
	{
		.compatible     = "amlogic, hdmirx_gxtvbb",
		.data           = &rx_gxtvbb_data
	},
	{},
};

/*
 * rx_init_reg_map - physical addr map
 *
 * map physical address of I/O memory resources
 * into the core virtual address space
 */
int rx_init_reg_map(struct platform_device *pdev)
{
	int i;
	struct resource *res = 0;
	int size = 0;
	int ret = 0;

	for (i = 0; i < MAP_ADDR_MODULE_NUM; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			rx_pr("missing memory resource\n");
			ret = -ENOMEM;
			break;
		}
		size = resource_size(res);
		reg_maps[i].phy_addr = res->start;
		reg_maps[i].p = devm_ioremap_nocache(&pdev->dev,
			res->start, size);
		/* rx_pr("phy_addr = 0x%x, size = 0x%x, maped:%p\n", */
			/* reg_maps[i].phy_addr, size, reg_maps[i].p); */
	}
	return ret;
}


/*
 * first_bit_set - get position of the first set bit
 */
static unsigned int first_bit_set(uint32_t data)
{
	unsigned int n = 32;

	if (data != 0) {
		for (n = 0; (data & 1) == 0; n++)
			data >>= 1;
	}
	return n;
}

/*
 * get - get masked bits of data
 */
unsigned int rx_get_bits(unsigned int data, unsigned int mask)
{
	return (data & mask) >> first_bit_set(mask);
}

unsigned int rx_set_bits(unsigned int data,
	unsigned int mask, unsigned int value)
{
	return ((value << first_bit_set(mask)) & mask) | (data & ~mask);
}

/*
 * hdmirx_dec_support - check if given port is supported
 * @fe: frontend device of tvin interface
 * @port: port of specified frontend
 *
 * return 0 if specified port of frontend is supported,
 * otherwise return a negative value.
 */
int hdmirx_dec_support(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	if ((port >= TVIN_PORT_HDMI0) && (port <= TVIN_PORT_HDMI7)) {
		rx_pr("hdmirx support\n");
		return 0;
	} else
		return -1;
}

/*
 * hdmirx_dec_open - open frontend
 * @fe: frontend device of tvin interface
 * @port: port index of specified frontend
 */
int hdmirx_dec_open(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct hdmirx_dev_s *devp;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	devp->param.port = port;

	/* should enable the adc ref signal for audio pll */
	vdac_enable(1, 0x10);

	hdmirx_open_port(port);
	rx.open_fg = 1;
	rx_pr("%s port:%x ok nosignal:%d\n", __func__, port, rx.no_signal);
	return 0;
}

/*
 * hdmirx_dec_start - after signal stable, vdin
 * start process video in detected format
 * @fe: frontend device of tvin interface
 * @fmt: format in which vdin process
 */
void hdmirx_dec_start(struct tvin_frontend_s *fe, enum tvin_sig_fmt_e fmt)
{
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;
	parm->info.fmt = fmt;
	parm->info.status = TVIN_SIG_STATUS_STABLE;
	rx_pr("%s fmt:%d ok\n", __func__, fmt);
}

/*
 * hdmirx_dec_stop - after signal unstable, vdin
 * stop decoder
 * @fe: frontend device of tvin interface
 * @port: port index of specified frontend
 */
void hdmirx_dec_stop(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;
	/* parm->info.fmt = TVIN_SIG_FMT_NULL; */
	/* parm->info.status = TVIN_SIG_STATUS_NULL; */
	rx_pr("%s ok\n", __func__);
}

/*
 * hdmirx_dec_open - close frontend
 * @fe: frontend device of tvin interface
 */
void hdmirx_dec_close(struct tvin_frontend_s *fe)
{
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;

	/* should disable the adc ref signal for audio pll */
	vdac_enable(0, 0x10);

	/* open_flage = 0; */
	rx.open_fg = 0;
	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;
	/*del_timer_sync(&devp->timer);*/
	hdmirx_close_port();
	parm->info.fmt = TVIN_SIG_FMT_NULL;
	parm->info.status = TVIN_SIG_STATUS_NULL;
	rx_pr("%s ok\n", __func__);
}

/*
 * hdmirx_dec_isr - interrupt handler
 * @fe: frontend device of tvin interface
 */
int hdmirx_dec_isr(struct tvin_frontend_s *fe, unsigned int hcnt64)
{
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;
	uint32_t avmuteflag;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;

	/*prevent spurious pops or noise when pw down*/
	if (rx.state == FSM_SIG_READY) {
		avmuteflag =
			hdmirx_rd_dwc(DWC_PDEC_GCP_AVMUTE) & 0x03;
		if (avmuteflag == 0x02) {
			rx.avmute_skip += 1;
			return TVIN_BUF_SKIP;
		}
		rx.avmute_skip = 0;
	}

	/* if there is any error or overflow, do some reset, then rerurn -1;*/
	if ((parm->info.status != TVIN_SIG_STATUS_STABLE) ||
	    (parm->info.fmt == TVIN_SIG_FMT_NULL))
		return -1;
	else if (rx.skip > 0)
		return TVIN_BUF_SKIP;
	return 0;
}

/*
 * hdmi_dec_callmaster
 * @port: port index of specified frontend
 * @fe: frontend device of tvin interface
 *
 * return power_5V status of specified port
 */
static int hdmi_dec_callmaster(enum tvin_port_e port,
	struct tvin_frontend_s *fe)
{
	int status = rx_get_hdmi5v_sts();

	switch (port) {
	case TVIN_PORT_HDMI0:
		status = (status >> 0) & 0x1;
		break;
	case TVIN_PORT_HDMI1:
		status = (status >> 1) & 0x1;
		break;
	case TVIN_PORT_HDMI2:
		status = (status >> 2) & 0x1;
		break;
	case TVIN_PORT_HDMI3:
	    status = (status >> 3) & 0x1;
		break;
	default:
		status = 1;
		break;
	}
	return status;

}

void hdmirx_extcon_register(struct platform_device *pdev, struct device *dev)
{
	struct extcon_dev *edev;
	int ret;

	/*hdmirx extcon start rx22*/
	edev = extcon_dev_allocate(rx22_ext);
	if (IS_ERR(edev)) {
		rx_pr("failed to allocate rx_excton_rx22\n");
		return;
	}
	edev->dev.parent = dev;
	edev->name = "rx_excton_rx22";
	dev_set_name(&edev->dev, "rx22");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		rx_pr("failed to register rx_excton_rx22\n");
		return;
	}
	rx.rx_excton_rx22 = edev;

	/*hdmirx extcon rp auth*/
	edev = extcon_dev_allocate(rx22_ext);
	if (IS_ERR(edev)) {
		rx_pr("failed to allocate rx_excton_auth\n");
		return;
	}
	edev->dev.parent = dev;
	edev->name = "rx_excton_auth";
	dev_set_name(&edev->dev, "rp_auth");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		rx_pr("failed to register rx_excton_auth\n");
		return;
	}
	rx.hdcp.rx_excton_auth = edev;
	/* rx_pr("hdmirx_extcon_register done\n"); */
}
static struct tvin_decoder_ops_s hdmirx_dec_ops = {
	.support    = hdmirx_dec_support,
	.open       = hdmirx_dec_open,
	.start      = hdmirx_dec_start,
	.stop       = hdmirx_dec_stop,
	.close      = hdmirx_dec_close,
	.decode_isr = hdmirx_dec_isr,
	.callmaster_det = hdmi_dec_callmaster,
};

/*
 * hdmirx_is_nosig
 * @fe: frontend device of tvin interface
 *
 * return true if no signal is detected,
 * otherwise return false.
 */
bool hdmirx_is_nosig(struct tvin_frontend_s *fe)
{
	bool ret = 0;

	ret = rx_is_nosig();
	return ret;
}

/***************************************************
 *func: hdmirx_fmt_chg
 *	@fe: frontend device of tvin interface
 *
 *	return true if video format changed, otherwise
 *	return false.
 ***************************************************/
bool hdmirx_fmt_chg(struct tvin_frontend_s *fe)
{
	bool ret = false;
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;
	if (rx_is_sig_ready() == false)
		ret = true;
	else {
		fmt = hdmirx_hw_get_fmt();
		if (fmt != parm->info.fmt) {
			rx_pr("hdmirx fmt: %d --> %d\n",
				parm->info.fmt, fmt);
			parm->info.fmt = fmt;
			ret = true;
		} else
		    ret = false;
	}
	return ret;
}

/*
 * hdmirx_fmt_chg - get current video format
 * @fe: frontend device of tvin interface
 */
enum tvin_sig_fmt_e hdmirx_get_fmt(struct tvin_frontend_s *fe)
{
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;

	fmt = hdmirx_hw_get_fmt();
	return fmt;
}

bool hdmirx_hw_check_frame_skip(void)
{
	if ((rx.state != FSM_SIG_READY) || (rx.skip > 0))
		return true;

	return false;
}
/*
 * hdmirx_get_dvi_info - get dvi state
 */
void hdmirx_get_dvi_info(struct tvin_sig_property_s *prop)
{
	prop->dvi_info = rx.pre.sw_dvi;
}

/*
 * hdmirx_get_fps_info - get video frame rate info
 */
void hdmirx_get_fps_info(struct tvin_sig_property_s *prop)
{
	uint32_t rate = rx.pre.frame_rate;

	rate = rate/100 + (((rate%100)/10 >= 5) ? 1 : 0);
	prop->fps = rate;
}

/*
 * hdmirx_set_timing_info
 * adjust timing info for video process
 */
void hdmirx_set_timing_info(struct tvin_sig_property_s *prop)
{
	enum tvin_sig_fmt_e sig_fmt;

	sig_fmt = hdmirx_hw_get_fmt();
	/* in some PC case, 4096X2160 show in 3840X2160 monitor will */
	/* result in blurred, so adjust hactive to 3840 to show dot by dot */
	if (en_4k_2_2k) {
		if ((sig_fmt == TVIN_SIG_FMT_HDMI_4096_2160_00HZ) ||
			(sig_fmt == TVIN_SIG_FMT_HDMI_3840_2160_00HZ)) {
			prop->scaling4w = 1920;
			prop->scaling4h = 1080;
		}
	} else if (en_4096_2_3840) {
		if ((sig_fmt == TVIN_SIG_FMT_HDMI_4096_2160_00HZ) &&
			(prop->color_format == TVIN_RGB444)) {
			prop->hs = 128;
			prop->he = 128;
		}
	}
	/* under 4k2k50/60hz 10/12bit mode, */
	/* hdmi out clk will overstep the max sample rate of vdin */
	/* so need discard the last line data to avoid display err */
	/* 420 : hdmiout clk = pixel clk * 2 */
	/* 422 : hdmiout clk = pixel clk * colordepth / 8 */
	/* 444 : hdmiout clk = pixel clk */
	if ((rx.pre.colordepth > E_COLORDEPTH_8) &&
		(prop->fps > 49) &&
		((sig_fmt == TVIN_SIG_FMT_HDMI_4096_2160_00HZ) ||
		(sig_fmt == TVIN_SIG_FMT_HDMI_3840_2160_00HZ)))
		prop->ve = 1;
}

/*
 * hdmirx_get_color_fmt - get video color format
 */
void hdmirx_get_color_fmt(struct tvin_sig_property_s *prop)
{
	int format = rx.pre.colorspace;

	if (rx.pre.sw_dvi == 1)
		format = E_COLOR_RGB;
	prop->dest_cfmt = TVIN_YUV422;
	switch (format) {
	case E_COLOR_YUV422:
	case E_COLOR_YUV420:
		prop->color_format = TVIN_YUV422;
		break;
	case E_COLOR_YUV444:
		prop->color_format = TVIN_YUV444;
		if (hdmi_yuv444_enable)
			prop->dest_cfmt = TVIN_YUV444;
		break;
	case E_COLOR_RGB:
	default:
		prop->color_format = TVIN_RGB444;
		if (it_content || pc_mode_en)
			prop->dest_cfmt = TVIN_YUV444;
		break;
	}
	if (rx.pre.interlaced == 1)
		prop->dest_cfmt = TVIN_YUV422;

	switch (prop->color_format) {
	case TVIN_YUV444:
	case TVIN_YUV422:
		if (yuv_quant_range == E_RANGE_LIMIT)
			prop->color_fmt_range = TVIN_YUV_LIMIT;
		else if (yuv_quant_range == E_RANGE_FULL)
			prop->color_fmt_range = TVIN_YUV_FULL;
		else
			prop->color_fmt_range = TVIN_FMT_RANGE_NULL;
		break;
	case TVIN_RGB444:
		if ((rgb_quant_range == E_RANGE_FULL) || (rx.pre.sw_dvi == 1))
			prop->color_fmt_range = TVIN_RGB_FULL;
		else if (rgb_quant_range == E_RANGE_LIMIT)
			prop->color_fmt_range = TVIN_RGB_LIMIT;
		else
			prop->color_fmt_range = TVIN_FMT_RANGE_NULL;
		break;
	default:
		prop->color_fmt_range = TVIN_FMT_RANGE_NULL;
		break;
	}
}

/*
 * hdmirx_get_colordepth - get video color depth
 */
void hdmirx_get_colordepth(struct tvin_sig_property_s *prop)
{
	prop->colordepth = rx.pre.colordepth;
}

int hdmirx_hw_get_3d_structure(void)
{
	uint8_t ret = 0;

	if (rx.vs_info_details.vd_fmt == VSI_FORMAT_3D_FORMAT)
		ret = 1;
	return ret;
}

/*
 * hdmirx_get_vsi_info - get vsi info
 * this func is used to get 3D format and dobly
 * vision state of current video
 */
void hdmirx_get_vsi_info(struct tvin_sig_property_s *prop)
{
	rx_get_vsi_info();

	prop->trans_fmt = TVIN_TFMT_2D;
	prop->dolby_vision = false;
	if (hdmirx_hw_get_3d_structure() == 1) {
		if (rx.vs_info_details._3d_structure == 0x1) {
			/* field alternative */
			prop->trans_fmt = TVIN_TFMT_3D_FA;
		} else if (rx.vs_info_details._3d_structure == 0x2) {
			/* line alternative */
			prop->trans_fmt = TVIN_TFMT_3D_LA;
		} else if (rx.vs_info_details._3d_structure == 0x3) {
			/* side-by-side full */
			prop->trans_fmt = TVIN_TFMT_3D_LRF;
		} else if (rx.vs_info_details._3d_structure == 0x4) {
			/* L + depth */
			prop->trans_fmt = TVIN_TFMT_3D_LD;
		} else if (rx.vs_info_details._3d_structure == 0x5) {
			/* L + depth + graphics + graphics-depth */
			prop->trans_fmt = TVIN_TFMT_3D_LDGD;
		} else if (rx.vs_info_details._3d_structure == 0x6) {
			/* top-and-bot */
			prop->trans_fmt = TVIN_TFMT_3D_TB;
		} else if (rx.vs_info_details._3d_structure == 0x8) {
			/* Side-by-Side half */
			switch (rx.vs_info_details._3d_ext_data) {
			case 0x5:
				/*Odd/Left picture, Even/Right picture*/
				prop->trans_fmt = TVIN_TFMT_3D_LRH_OLER;
				break;
			case 0x6:
				/*Even/Left picture, Odd/Right picture*/
				prop->trans_fmt = TVIN_TFMT_3D_LRH_ELOR;
				break;
			case 0x7:
				/*Even/Left picture, Even/Right picture*/
				prop->trans_fmt = TVIN_TFMT_3D_LRH_ELER;
				break;
			case 0x4:
				/*Odd/Left picture, Odd/Right picture*/
			default:
				prop->trans_fmt = TVIN_TFMT_3D_LRH_OLOR;
				break;
			}
		}
	} else {
		prop->dolby_vision = rx.vs_info_details.dolby_vision;
		prop->low_latency = rx.vs_info_details.low_latency;
		if ((rx.vs_info_details.dolby_vision == true) &&
			(rx.vs_info_details.dolby_timeout <=
				dv_nopacket_timeout) &&
			(rx.vs_info_details.dolby_timeout != 0))
			rx.vs_info_details.dolby_timeout--;
		if (rx.vs_info_details.dolby_timeout == 0) {
			rx.vs_info_details.dolby_vision = false;
			rx_pr("dv timeout\n");
		}
		if (log_level & VSI_LOG) {
			rx_pr("prop->dolby_vision:%d\n", prop->dolby_vision);
			rx_pr("prop->low_latency:%d\n", prop->low_latency);
		}
	}
}
/*
 * hdmirx_get_repetition_info - get repetition info
 */
void hdmirx_get_repetition_info(struct tvin_sig_property_s *prop)
{
	prop->decimation_ratio = rx.pre.repeat |
			HDMI_DE_REPEAT_DONE_FLAG;
}

/*
 * hdmirx_get_hdr_info - get hdr info
 */
void hdmirx_get_hdr_info(struct tvin_sig_property_s *prop)
{
	struct drm_infoframe_st *drmpkt;

	/*check drm packet is attach every VS*/
	uint32_t drm_attach = rx_pkt_chk_attach_drm();

	drmpkt = (struct drm_infoframe_st *)&(rx_pkt.drm_info);

	if (drm_attach) {
		rx.hdr_info.hdr_state = HDR_STATE_SET;
		rx_pkt_clr_attach_drm();
	}

	/* hdr data processing */
	switch (rx.hdr_info.hdr_state) {
	case HDR_STATE_NULL:
		/* filter for state, 10*10ms */
		if (rx.hdr_info.hdr_check_cnt++ > 10) {
			prop->hdr_info.hdr_state = HDR_STATE_NULL;
			rx.hdr_info.hdr_check_cnt = 0;
		}
		break;
	case HDR_STATE_GET:
		rx.hdr_info.hdr_check_cnt = 0;
		break;
	case HDR_STATE_SET:
		rx.hdr_info.hdr_check_cnt = 0;
		if (prop->hdr_info.hdr_state != HDR_STATE_GET) {
			#if 0
			/*prop->hdr_info.hdr_data = rx.hdr_info.hdr_data;*/
			#else
			if (rx_pkt_chk_busy_drm())
				break;

			prop->hdr_info.hdr_data.length = drmpkt->length;
			prop->hdr_info.hdr_data.eotf = drmpkt->des_u.tp1.eotf;
			prop->hdr_info.hdr_data.metadata_id =
				drmpkt->des_u.tp1.meta_des_id;
			prop->hdr_info.hdr_data.primaries[0].x =
				drmpkt->des_u.tp1.dis_pri_x0;
			prop->hdr_info.hdr_data.primaries[0].y =
				drmpkt->des_u.tp1.dis_pri_y0;
			prop->hdr_info.hdr_data.primaries[1].x =
				drmpkt->des_u.tp1.dis_pri_x1;
			prop->hdr_info.hdr_data.primaries[1].y =
				drmpkt->des_u.tp1.dis_pri_y1;
			prop->hdr_info.hdr_data.primaries[2].x =
				drmpkt->des_u.tp1.dis_pri_x2;
			prop->hdr_info.hdr_data.primaries[2].y =
				drmpkt->des_u.tp1.dis_pri_y2;
			prop->hdr_info.hdr_data.white_points.x =
				drmpkt->des_u.tp1.white_points_x;
			prop->hdr_info.hdr_data.white_points.y =
				drmpkt->des_u.tp1.white_points_y;
			prop->hdr_info.hdr_data.master_lum.x =
				drmpkt->des_u.tp1.max_dislum;
			prop->hdr_info.hdr_data.master_lum.y =
				drmpkt->des_u.tp1.min_dislum;
			prop->hdr_info.hdr_data.mcll =
				drmpkt->des_u.tp1.max_light_lvl;
			prop->hdr_info.hdr_data.mfall =
				drmpkt->des_u.tp1.max_fa_light_lvl;
			#endif

			/* vdin can read current hdr data */
			prop->hdr_info.hdr_state = HDR_STATE_GET;

			/* Rx can get new hdr data */
			rx.hdr_info.hdr_state = HDR_STATE_NULL;
		}
		break;
	default:
		break;
	}
}

/***************************************************
 *func: hdmirx_get_sig_property - get signal property
 **************************************************/
void hdmirx_get_sig_property(struct tvin_frontend_s *fe,
	struct tvin_sig_property_s *prop)
{
	hdmirx_get_dvi_info(prop);
	hdmirx_get_colordepth(prop);
	hdmirx_get_fps_info(prop);
	hdmirx_get_color_fmt(prop);
	hdmirx_get_repetition_info(prop);
	hdmirx_set_timing_info(prop);
	hdmirx_get_hdr_info(prop);
	hdmirx_get_vsi_info(prop);
	prop->skip_vf_num = skip_frame_cnt;
}

/*
 * hdmirx_check_frame_skip - check frame skip
 *
 * return true if video frame skip is needed,
 * return false otherwise.
 */
bool hdmirx_check_frame_skip(struct tvin_frontend_s *fe)
{
	return hdmirx_hw_check_frame_skip();
}

static struct tvin_state_machine_ops_s hdmirx_sm_ops = {
	.nosig            = hdmirx_is_nosig,
	.fmt_changed      = hdmirx_fmt_chg,
	.get_fmt          = hdmirx_get_fmt,
	.fmt_config       = NULL,
	.adc_cal          = NULL,
	.pll_lock         = NULL,
	.get_sig_property  = hdmirx_get_sig_property,
	.vga_set_param    = NULL,
	.vga_get_param    = NULL,
	.check_frame_skip = hdmirx_check_frame_skip,
};

/*
 * hdmirx_open - file operation interface
 */
static int hdmirx_open(struct inode *inode, struct file *file)
{
	struct hdmirx_dev_s *devp;

	devp = container_of(inode->i_cdev, struct hdmirx_dev_s, cdev);
	file->private_data = devp;
	return 0;
}

/*
 * hdmirx_release - file operation interface
 */
static int hdmirx_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*
 * hdmirx_ioctl - file operation interface
 */
static long hdmirx_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	long ret = 0;
	/* unsigned int delay_cnt = 0; */
	struct hdmirx_dev_s *devp = NULL;
	void __user *argp = (void __user *)arg;
	uint32_t param = 0, temp_val, temp;
	unsigned int size = sizeof(struct pd_infoframe_s);
	struct pd_infoframe_s pkt_info;
	void *srcbuff;

	if (_IOC_TYPE(cmd) != HDMI_IOC_MAGIC) {
		pr_err("%s invalid command: %u\n", __func__, cmd);
		return -EINVAL;
	}
	srcbuff = &pkt_info;
	devp = file->private_data;
	switch (cmd) {
	case HDMI_IOC_HDCP_GET_KSV:{
		struct _hdcp_ksv ksv;

		if (argp == NULL)
			return -EINVAL;
		mutex_lock(&devp->rx_lock);
		ksv.bksv0 = rx.hdcp.bksv[0];
		ksv.bksv1 = rx.hdcp.bksv[1];
		if (copy_to_user(argp, &ksv,
			sizeof(struct _hdcp_ksv))) {
			ret = -EFAULT;
			mutex_unlock(&devp->rx_lock);
			break;
		}
		mutex_unlock(&devp->rx_lock);
		break;
	}
	case HDMI_IOC_HDCP_ON:
		hdcp_enable = 1;
		rx_set_hpd(0);
		fsm_restart();
		break;
	case HDMI_IOC_HDCP_OFF:
		hdcp_enable = 0;
		rx_set_hpd(0);
		hdmirx_hw_config();
		fsm_restart();
		break;
	case HDMI_IOC_EDID_UPDATE:
		if (rx.open_fg) {
			rx_set_hpd(0);
			edid_update_flag = 1;
		}
		#if 0
		else {
			if (hdmi_cec_en) {
				if (is_meson_gxtvbb_cpu())
					rx_force_hpd_cfg(0);
				else
					rx_force_hpd_cfg(1);
				hdmi_rx_top_edid_update();
			} else
				rx_pr("cec_off,ignore edid update\n");
		}
		#endif
		hdmi_rx_top_edid_update();
		fsm_restart();
		rx_pr("*update edid*\n");
		break;
	case HDMI_IOC_PC_MODE_ON:
		pc_mode_en = 1;
		hotplug_wait_query();
		rx_pr("pc mode on\n");
		break;
	case HDMI_IOC_PC_MODE_OFF:
		pc_mode_en = 0;
		hotplug_wait_query();
		rx_pr("pc mode off\n");
		break;
	case HDMI_IOC_HDCP22_AUTO:
		break;
	case HDMI_IOC_HDCP22_FORCE14:
		break;
	case HDMI_IOC_PD_FIFO_PKTTYPE_EN:
		/*rx_pr("IOC_PD_FIFO_PKTTYPE_EN\n");*/
		/*get input param*/
		if (copy_from_user(&param, argp, sizeof(uint32_t))) {
			pr_err("get pd fifo param err\n");
			ret = -EFAULT;
			break;
		}
		rx_pr("en cmd:0x%x\n", param);
		rx_pkt_buffclear(param);
		temp = rx_pkt_type_mapping(param);
		packet_fifo_cfg |= temp;
		/*enable pkt pd fifo*/
		temp_val = hdmirx_rd_dwc(DWC_PDEC_CTRL);
		temp_val |= temp;
		hdmirx_wr_dwc(DWC_PDEC_CTRL, temp_val);
		/*enable int*/
		pdec_ists_en |= PD_FIFO_START_PASS|PD_FIFO_OVERFL;
		/*open pd fifo interrupt source if signal stable*/
		temp_val = hdmirx_rd_dwc(DWC_PDEC_IEN);
		if (((temp_val&PD_FIFO_START_PASS) == 0) &&
			(rx.state >= FSM_SIG_UNSTABLE)) {
			temp_val |= pdec_ists_en;
			hdmirx_wr_dwc(DWC_PDEC_IEN_SET, temp_val);
			rx_pr("open pd fifo int:0x%x\n", pdec_ists_en);
		}
		break;
	case HDMI_IOC_PD_FIFO_PKTTYPE_DIS:
		/*rx_pr("IOC_PD_FIFO_PKTTYPE_DIS\n");*/
		/*get input param*/
		if (copy_from_user(&param, argp, sizeof(uint32_t))) {
			pr_err("get pd fifo param err\n");
			ret = -EFAULT;
			break;
		}
		rx_pr("dis cmd:0x%x\n", param);
		temp = rx_pkt_type_mapping(param);
		packet_fifo_cfg &= ~temp;
		/*disable pkt pd fifo*/
		temp_val = hdmirx_rd_dwc(DWC_PDEC_CTRL);
		temp_val &= ~temp;
		hdmirx_wr_dwc(DWC_PDEC_CTRL, temp_val);
		break;

	case HDMI_IOC_GET_PD_FIFO_PARAM:
		/*mutex_lock(&pktbuff_lock);*/
		/*rx_pr("IOC_GET_PD_FIFO_PARAM\n");*/
		/*get input param*/
		if (copy_from_user(&param, argp, sizeof(uint32_t))) {
			pr_err("get pd fifo param err\n");
			ret = -EFAULT;
			/*mutex_unlock(&pktbuff_lock);*/
			break;
		}
		memset(&pkt_info, 0, sizeof(pkt_info));
		srcbuff = &pkt_info;
		size = sizeof(struct pd_infoframe_s);
		rx_get_pd_fifo_param(param, &pkt_info, size);

		/*return pkt info*/
		if ((size > 0) && (srcbuff != NULL) && (argp != NULL)) {
			if (copy_to_user(argp, srcbuff, size)) {
				pr_err("get pd fifo param err\n");
				ret = -EFAULT;
			}
		}
		/*mutex_unlock(&pktbuff_lock);*/
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
/*
 * hdmirx_compat_ioctl - file operation interface
 */
static long hdmirx_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = hdmirx_ioctl(file, cmd, arg);
	return ret;
}
#endif

/*
 * hotplug_wait_query - wake up poll process
 */
void hotplug_wait_query(void)
{
	wake_up(&query_wait);
}

/*
 * hdmirx_sts_read - read interface for tvserver
 */
static ssize_t hdmirx_sts_read(struct file *file,
	    char __user *buf, size_t count, loff_t *pos)
{
	int ret = 0;
	int rx_sts;

	/*
	 * sysctl set the pc_mode on/off via UI,tvserver need to know
	 * the status,but it cant get the status from sysctl because of the
	 * limit of the communication from sysctl to tvserver,so it needs
	 * hdmi driver to feedback the pc_mode status to tvserver
	 */

	rx_sts = pwr_sts | (pc_mode_en << 4);

	if (copy_to_user(buf, &rx_sts, sizeof(unsigned int)))
		return -EFAULT;

	return ret;
}

/*
 * hdmirx_sts_poll - poll interface for tvserver
 */
static unsigned int hdmirx_sts_poll(struct file *filp,
		poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &query_wait, wait);
	mask |= POLLIN|POLLRDNORM;

	return mask;
}

static const struct file_operations hdmirx_fops = {
	.owner		= THIS_MODULE,
	.open		= hdmirx_open,
	.release	= hdmirx_release,
	.read       = hdmirx_sts_read,
	.poll       = hdmirx_sts_poll,
	.unlocked_ioctl	= hdmirx_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hdmirx_compat_ioctl,
#endif
};

int rx_pr_buf(char *buf, int len)
{
	unsigned long flags;
	int pos;
	int hdmirx_log_rd_pos_;

	if (hdmirx_log_buf_size == 0)
		return 0;
	spin_lock_irqsave(&rx_pr_lock, flags);
	hdmirx_log_rd_pos_ = hdmirx_log_rd_pos;
	if (hdmirx_log_wr_pos >= hdmirx_log_rd_pos)
		hdmirx_log_rd_pos_ += hdmirx_log_buf_size;
	for (pos = 0;
		pos < len && hdmirx_log_wr_pos < (hdmirx_log_rd_pos_ - 1);
		pos++, hdmirx_log_wr_pos++) {
		if (hdmirx_log_wr_pos >= hdmirx_log_buf_size)
			hdmirx_log_buf[hdmirx_log_wr_pos - hdmirx_log_buf_size]
				= buf[pos];
		else
			hdmirx_log_buf[hdmirx_log_wr_pos] = buf[pos];
	}
	if (hdmirx_log_wr_pos >= hdmirx_log_buf_size)
		hdmirx_log_wr_pos -= hdmirx_log_buf_size;
	spin_unlock_irqrestore(&rx_pr_lock, flags);
	return pos;
}

int rx_pr(const char *fmt, ...)
{
	va_list args;
	int avail = PRINT_TEMP_BUF_SIZE;
	char buf[PRINT_TEMP_BUF_SIZE];
	int pos = 0;
	int len = 0;
	static bool last_break = 1;

	if ((last_break == 1) &&
		(strlen(fmt) > 1)) {
		strcpy(buf, "[RX]-");
		for (len = 0; len < strlen(fmt); len++)
			if (fmt[len] == '\n')
				pos++;
			else
				break;

		strcpy(buf + 5, fmt + pos);
	} else
		strcpy(buf, fmt);
	/* if (fmt[strlen(fmt) - 1] == '\n') */
		/* last_break = 1; */
	/* else */
		/* last_break = 0; */
	if (log_level & LOG_EN) {
		va_start(args, fmt);
		vprintk(buf, args);
		va_end(args);
		return 0;
	}
	if (hdmirx_log_buf_size == 0)
		return 0;

	/* len += snprintf(buf+len, avail-len, "%d:",log_seq++); */
	len += snprintf(buf + len, avail - len, "[%u] ", (unsigned int)jiffies);
	va_start(args, fmt);
	len += vsnprintf(buf + len, avail - len, fmt, args);
	va_end(args);
	if ((avail-len) <= 0)
		buf[PRINT_TEMP_BUF_SIZE - 1] = '\0';

	pos = rx_pr_buf(buf, len);
	return pos;
}

static int log_init(int bufsize)
{
	if (bufsize == 0) {
		if (hdmirx_log_buf) {
			/* kfree(hdmirx_log_buf); */
			hdmirx_log_buf = NULL;
			hdmirx_log_buf_size = 0;
			hdmirx_log_rd_pos = 0;
			hdmirx_log_wr_pos = 0;
		}
	}
	if ((bufsize >= 1024) && (hdmirx_log_buf == NULL)) {
		hdmirx_log_buf_size = 0;
		hdmirx_log_rd_pos = 0;
		hdmirx_log_wr_pos = 0;
		hdmirx_log_buf = kmalloc(bufsize, GFP_KERNEL);
		if (hdmirx_log_buf)
			hdmirx_log_buf_size = bufsize;
	}
	return 0;
}

static ssize_t show_log(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned long flags;
	ssize_t read_size = 0;

	if (hdmirx_log_buf_size == 0)
		return 0;
	spin_lock_irqsave(&rx_pr_lock, flags);
	if (hdmirx_log_rd_pos < hdmirx_log_wr_pos)
		read_size = hdmirx_log_wr_pos-hdmirx_log_rd_pos;
	else if (hdmirx_log_rd_pos > hdmirx_log_wr_pos)
		read_size = hdmirx_log_buf_size-hdmirx_log_rd_pos;

	if (read_size > PAGE_SIZE)
		read_size = PAGE_SIZE;
	if (read_size > 0)
		memcpy(buf, hdmirx_log_buf+hdmirx_log_rd_pos, read_size);
	hdmirx_log_rd_pos += read_size;
	if (hdmirx_log_rd_pos >= hdmirx_log_buf_size)
		hdmirx_log_rd_pos = 0;
	spin_unlock_irqrestore(&rx_pr_lock, flags);
	return read_size;
}

static ssize_t store_log(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	long tmp;
	unsigned long flags;

	if (strncmp(buf, "bufsize", 7) == 0) {
		if (kstrtoul(buf + 7, 10, &tmp) < 0)
			return -EINVAL;
		spin_lock_irqsave(&rx_pr_lock, flags);
		log_init(tmp);
		spin_unlock_irqrestore(&rx_pr_lock, flags);
		rx_pr("hdmirx_store:set bufsize tmp %ld %d\n",
			tmp, hdmirx_log_buf_size);
	} else {
		rx_pr(0, "%s", buf);
	}
	return 16;
}


static ssize_t hdmirx_debug_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return 0;
}

static ssize_t hdmirx_debug_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	hdmirx_debug(buf, count);
	return count;
}

static ssize_t hdmirx_edid_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return hdmirx_read_edid_buf(buf, PAGE_SIZE);
}

static ssize_t hdmirx_edid_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	hdmirx_fill_edid_buf(buf, count);
	return count;
}

static ssize_t hdmirx_key_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return 0;/*hdmirx_read_key_buf(buf, PAGE_SIZE);*/
}

static ssize_t hdmirx_key_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	hdmirx_fill_key_buf(buf, count);
	return count;
}

static ssize_t show_reg(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return hdmirx_hw_dump_reg(buf, PAGE_SIZE);
}

static ssize_t cec_get_state(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return 0;
}

/*
 *  val == 0 : cec disable
 *  val == 1 : cec on
 *  val == 2 : cec on && system startup
 */
static ssize_t cec_set_state(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	int cnt, val;

	/* cnt = sscanf(buf, "%x", &val); */
	cnt = kstrtoint(buf, 0, &val);
	if (cnt < 0 || val > 0xff)
		return -EINVAL;
	if (val == 0)
		hdmi_cec_en = 0;
	else if (val == 1)
		hdmi_cec_en = 1;
	else if (val == 2) {
		hdmi_cec_en = 1;
		rx_force_hpd_cfg(1);
	}
	rx_pr("cec sts = %d\n", val);
	return count;
}

static ssize_t param_get_value(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	rx_get_global_variable(buf);
	return 0;
}

static ssize_t param_set_value(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	rx_set_global_variable(buf, count);
	return count;
}

static ssize_t esm_get_base(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	int pos = 0;

	pos += snprintf(buf, PAGE_SIZE, "0x%x\n",
		reg_maps[MAP_ADDR_MODULE_HDMIRX_CAPB3].phy_addr);
	rx_pr("hdcp_rx22 get esm base:%#x\n",
		reg_maps[MAP_ADDR_MODULE_HDMIRX_CAPB3].phy_addr);

	return pos;
}

static ssize_t esm_set_base(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	return count;
}

static ssize_t show_info(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return hdmirx_show_info(buf, PAGE_SIZE);
}

static ssize_t store_info(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	return count;
}

static ssize_t set_arc_aud_type(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	rx_parse_arc_aud_type(buf);
	return count;
}

static ssize_t get_arc_aud_type(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return 0;
}

static DEVICE_ATTR(debug, 0644, hdmirx_debug_show, hdmirx_debug_store);
static DEVICE_ATTR(edid, 0644, hdmirx_edid_show, hdmirx_edid_store);
static DEVICE_ATTR(key, 0644, hdmirx_key_show, hdmirx_key_store);
static DEVICE_ATTR(log, 0644, show_log, store_log);
static DEVICE_ATTR(reg, 0644, show_reg, store_log);
static DEVICE_ATTR(cec, 0644, cec_get_state, cec_set_state);
static DEVICE_ATTR(param, 0644, param_get_value, param_set_value);
static DEVICE_ATTR(esm_base, 0644, esm_get_base, esm_set_base);
static DEVICE_ATTR(info, 0644, show_info, store_info);
static DEVICE_ATTR(arc_aud_type, 0644, get_arc_aud_type, set_arc_aud_type);


static int hdmirx_add_cdev(struct cdev *cdevp,
		const struct file_operations *fops,
		int minor)
{
	int ret;
	dev_t devno = MKDEV(MAJOR(hdmirx_devno), minor);

	cdev_init(cdevp, fops);
	cdevp->owner = THIS_MODULE;
	ret = cdev_add(cdevp, devno, 1);
	return ret;
}

static struct device *hdmirx_create_device(struct device *parent, int id)
{
	dev_t devno = MKDEV(MAJOR(hdmirx_devno),  id);

	return device_create(hdmirx_clsp, parent, devno, NULL, "%s0",
			TVHDMI_DEVICE_NAME);
	/* @to do this after Middleware API modified */
	/*return device_create(hdmirx_clsp, parent, devno, NULL, "%s",*/
	  /*TVHDMI_DEVICE_NAME); */
}

static void hdmirx_delete_device(int minor)
{
	dev_t devno = MKDEV(MAJOR(hdmirx_devno), minor);

	device_destroy(hdmirx_clsp, devno);
}

static void hdmirx_get_base_addr(struct device_node *node)
{
	int ret;

	/*get base addr from dts*/
	hdmirx_addr_port = reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
	if (node) {
		if (hdmirx_addr_port == 0) {
			ret = of_property_read_u32(node,
				"hdmirx_addr_port", &hdmirx_addr_port);
			if (ret)
				pr_err("get hdmirx_addr_port fail.\n");

			ret = of_property_read_u32(node,
					"hdmirx_data_port", &hdmirx_data_port);
			if (ret)
				pr_err("get hdmirx_data_port fail.\n");
			ret = of_property_read_u32(node,
					"hdmirx_ctrl_port", &hdmirx_ctrl_port);
			if (ret)
				pr_err("get hdmirx_ctrl_port fail.\n");
		} else {
			hdmirx_data_port = hdmirx_addr_port + 4;
			hdmirx_ctrl_port = hdmirx_addr_port + 8;
		}
		/* rx_pr("port addr:%#x ,data:%#x, ctrl:%#x\n", */
			/* hdmirx_addr_port, */
			/* hdmirx_data_port, hdmirx_ctrl_port); */
	}
}

static int hdmirx_switch_pinmux(struct device  dev)
{
	struct pinctrl *pin;
	const char *pin_name;
	int ret = 0;

	/* pinmux set */
	if (dev.of_node) {
		ret = of_property_read_string_index(dev.of_node,
					    "pinctrl-names",
					    0, &pin_name);
		if (!ret)
			pin = devm_pinctrl_get_select(&dev, pin_name);
			/* rx_pr("hdmirx: pinmux:%p, name:%s\n", */
			/* pin, pin_name); */
	}
	return ret;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
static void hdmirx_early_suspend(struct early_suspend *h)
{
	rx_pr("hdmirx_early_suspend ok\n");
}

static void hdmirx_late_resume(struct early_suspend *h)
{
	/* after early suspend & late resuem, also need to */
	/* do hpd reset when open port for hdcp compliance */
	pre_port = 0xff;
	rx_pr("hdmirx_late_resume ok\n");
};

static struct early_suspend hdmirx_early_suspend_handler = {
	/* .level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10, */
	.suspend = hdmirx_early_suspend,
	.resume = hdmirx_late_resume,
	/* .param = &hdmirx_device, */
};
#endif

static int hdmirx_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct hdmirx_dev_s *hdevp;
	struct resource *res;
	struct clk *xtal_clk;
	struct clk *fclk_div5_clk;
	struct clk *fclk_div7_clk;
	struct clk *tmds_clk_fs;
	int clk_rate;
	const struct of_device_id *of_id;

	log_init(DEF_LOG_BUF_SIZE);
	pEdid_buffer = (unsigned char *) pdev->dev.platform_data;
	hdmirx_dev = &pdev->dev;
	/* allocate memory for the per-device structure */
	hdevp = kmalloc(sizeof(struct hdmirx_dev_s), GFP_KERNEL);
	if (!hdevp) {
		rx_pr("hdmirx:allocate memory failed\n");
		ret = -ENOMEM;
		goto fail_kmalloc_hdev;
	}
	memset(hdevp, 0, sizeof(struct hdmirx_dev_s));
	/*get compatible matched device, to get chip related data*/
	of_id = of_match_device(hdmirx_dt_match, &pdev->dev);
	if (!of_id)
		rx_pr("unable to get matched device\n");
	hdevp->data = of_id->data;
	if (hdevp->data)
		rx.chip_id = hdevp->data->chip_id;
	else
		/*txlx chip for default*/
		rx.chip_id = CHIP_ID_TXLX;

	ret = rx_init_reg_map(pdev);
	if (ret < 0) {
		rx_pr("failed to ioremap\n");
		/*goto fail_ioremap;*/
	}
	hdmirx_get_base_addr(pdev->dev.of_node);
	/*@to get from bsp*/
	#if 0
	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"hdmirx_id", &(hdevp->index));
		if (ret) {
			pr_err("%s:don't find  hdmirx id.\n", __func__);
			goto fail_create_device;
		}
	} else {
			pr_err("%s: don't find match hdmirx node\n", __func__);
			return -1;
	}
	#endif
	hdevp->index = 0; /* pdev->id; */
	/* create cdev and reigser with sysfs */
	ret = hdmirx_add_cdev(&hdevp->cdev, &hdmirx_fops, hdevp->index);
	if (ret) {
		rx_pr("%s: failed to add cdev\n", __func__);
		goto fail_add_cdev;
	}
	/* create /dev nodes */
	hdevp->dev = hdmirx_create_device(&pdev->dev, hdevp->index);
	if (IS_ERR(hdevp->dev)) {
		rx_pr("hdmirx: failed to create device node\n");
		ret = PTR_ERR(hdevp->dev);
		goto fail_create_device;
	}
	/*create sysfs attribute files*/
	ret = device_create_file(hdevp->dev, &dev_attr_debug);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create debug attribute file\n");
		goto fail_create_debug_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_edid);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create edid attribute file\n");
		goto fail_create_edid_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_key);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create key attribute file\n");
		goto fail_create_key_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_log);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create log attribute file\n");
		goto fail_create_log_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_reg);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create reg attribute file\n");
		goto fail_create_reg_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_param);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create param attribute file\n");
		goto fail_create_param_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_info);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create info attribute file\n");
		goto fail_create_info_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_esm_base);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create esm_base attribute file\n");
		goto fail_create_esm_base_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_cec);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create cec attribute file\n");
		goto fail_create_cec_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_arc_aud_type);
		if (ret < 0) {
			rx_pr("hdmirx: fail to create arc_aud_type file\n");
			goto fail_create_arc_aud_type_file;
		}
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		rx_pr("%s: can't get irq resource\n", __func__);
		ret = -ENXIO;
		goto fail_get_resource_irq;
	}
	hdevp->irq = res->start;
	snprintf(hdevp->irq_name, sizeof(hdevp->irq_name),
			"hdmirx%d-irq", hdevp->index);
	rx_pr("hdevpd irq: %d, %d\n", hdevp->index,
			hdevp->irq);
	hdmirx_extcon_register(pdev, hdevp->dev);
	if (request_irq(hdevp->irq,
			&irq_handler,
			IRQF_SHARED,
			hdevp->irq_name,
			(void *)&rx))
		rx_pr(__func__, "RX IRQ request");
	/* frontend */
	tvin_frontend_init(&hdevp->frontend,
		&hdmirx_dec_ops,
		&hdmirx_sm_ops,
		hdevp->index);
	sprintf(hdevp->frontend.name, "%s", TVHDMI_NAME);
	if (tvin_reg_frontend(&hdevp->frontend) < 0)
		rx_pr("hdmirx: driver probe error!!!\n");

	dev_set_drvdata(hdevp->dev, hdevp);
	platform_set_drvdata(pdev, hdevp);

	xtal_clk = clk_get(&pdev->dev, "xtal");
	if (IS_ERR(xtal_clk))
		rx_pr("get xtal err\n");
	else
		clk_rate = clk_get_rate(xtal_clk);
	fclk_div5_clk = clk_get(&pdev->dev, "fclk_div5");
	if (IS_ERR(fclk_div5_clk))
		rx_pr("get fclk_div5_clk err\n");
	else
		clk_rate = clk_get_rate(fclk_div5_clk);
	fclk_div7_clk = clk_get(&pdev->dev, "fclk_div7");
	if (IS_ERR(fclk_div7_clk))
		rx_pr("get fclk_div7_clk err\n");
	else
		clk_rate = clk_get_rate(fclk_div7_clk);
	hdevp->modet_clk = clk_get(&pdev->dev, "hdmirx_modet_clk");
	if (IS_ERR(hdevp->modet_clk))
		rx_pr("get modet_clk err\n");
	else {
		clk_set_parent(hdevp->modet_clk, xtal_clk);
		clk_set_rate(hdevp->modet_clk, 24000000);
		clk_prepare_enable(hdevp->modet_clk);
		clk_rate = clk_get_rate(hdevp->modet_clk);
	}

	hdevp->cfg_clk = clk_get(&pdev->dev, "hdmirx_cfg_clk");
	if (IS_ERR(hdevp->cfg_clk))
		rx_pr("get cfg_clk err\n");
	else {
		clk_set_parent(hdevp->cfg_clk, fclk_div5_clk);
		clk_set_rate(hdevp->cfg_clk, 133333333);
		clk_prepare_enable(hdevp->cfg_clk);
		clk_rate = clk_get_rate(hdevp->cfg_clk);
	}
	hdcp22_on = rx_is_hdcp22_support();
	if (hdcp22_on) {
		hdevp->esm_clk = clk_get(&pdev->dev, "hdcp_rx22_esm");
		if (IS_ERR(hdevp->esm_clk))
			rx_pr("get esm_clk err\n");
		else {
			clk_set_parent(hdevp->esm_clk, fclk_div7_clk);
			clk_set_rate(hdevp->esm_clk, 285714285);
			clk_prepare_enable(hdevp->esm_clk);
			clk_rate = clk_get_rate(hdevp->esm_clk);
		}
		hdevp->skp_clk = clk_get(&pdev->dev, "hdcp_rx22_skp");
		if (IS_ERR(hdevp->skp_clk))
			rx_pr("get skp_clk err\n");
		else {
			clk_set_parent(hdevp->skp_clk, xtal_clk);
			clk_set_rate(hdevp->skp_clk, 24000000);
			clk_prepare_enable(hdevp->skp_clk);
			clk_rate = clk_get_rate(hdevp->skp_clk);
		}
	}
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
		tmds_clk_fs = clk_get(&pdev->dev, "hdmirx_aud_pll2fs");
		if (IS_ERR(tmds_clk_fs))
			rx_pr("get tmds_clk_fs err\n");
		hdevp->aud_out_clk = clk_get(&pdev->dev, "clk_aud_out");
		if (IS_ERR(hdevp->aud_out_clk) || IS_ERR(tmds_clk_fs))
			rx_pr("get aud_out_clk/tmds_clk_fs err\n");
		else {
			clk_set_parent(hdevp->aud_out_clk, tmds_clk_fs);
			clk_rate = clk_get_rate(hdevp->aud_out_clk);
		}
	}


	#if 0
	hdevp->acr_ref_clk = clk_get(&pdev->dev, "hdmirx_acr_ref_clk");
	if (IS_ERR(hdevp->acr_ref_clk))
		rx_pr("get acr_ref_clk err\n");
	else {
		clk_set_parent(hdevp->acr_ref_clk, fclk_div5_clk);
		clk_set_rate(hdevp->acr_ref_clk, 24000000);
		clk_rate = clk_get_rate(hdevp->acr_ref_clk);
		pr_info("%s: acr_ref_clk is %d MHZ\n", __func__,
				clk_rate/1000000);
	}
	#endif
	hdevp->audmeas_clk = clk_get(&pdev->dev, "hdmirx_audmeas_clk");
	if (IS_ERR(hdevp->audmeas_clk))
		rx_pr("get audmeas_clk err\n");
	else {
		clk_set_parent(hdevp->audmeas_clk, fclk_div5_clk);
		clk_set_rate(hdevp->audmeas_clk, 200000000);
		clk_rate = clk_get_rate(hdevp->audmeas_clk);
	}

	pd_fifo_buf = kmalloc_array(1, PFIFO_SIZE * sizeof(uint32_t),
		GFP_KERNEL);
	if (!pd_fifo_buf) {
		rx_pr("hdmirx:allocate pd fifo failed\n");
		ret = -ENOMEM;
		goto fail_kmalloc_pd_fifo;
	}

	tasklet_init(&rx_tasklet, rx_tasklet_handler, (unsigned long)&rx);
	/* create for hot plug function */
	eq_wq = create_singlethread_workqueue(hdevp->frontend.name);
	INIT_DELAYED_WORK(&eq_dwork, eq_dwork_handler);

	esm_wq = create_singlethread_workqueue(hdevp->frontend.name);
	INIT_DELAYED_WORK(&esm_dwork, rx_hpd_to_esm_handle);
	/* queue_delayed_work(eq_wq, &eq_dwork, msecs_to_jiffies(5)); */

	repeater_wq = create_singlethread_workqueue(hdevp->frontend.name);
	INIT_DELAYED_WORK(&repeater_dwork, repeater_dwork_handle);

	ret = of_property_read_u32(pdev->dev.of_node,
				"en_4k_2_2k", &en_4k_2_2k);
	if (ret) {
		rx_pr("en_4k_2_2k not found.\n");
		en_4k_2_2k = 0;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				"en_4k_timing", &en_4k_timing);
	if (ret)
		en_4k_timing = 1;

	hdmirx_hw_probe();
	hdmirx_switch_pinmux(pdev->dev);
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&hdmirx_early_suspend_handler);
#endif
	mutex_init(&hdevp->rx_lock);
	register_pm_notifier(&aml_hdcp22_pm_notifier);

	init_timer(&hdevp->timer);
	hdevp->timer.data = (ulong)hdevp;
	hdevp->timer.function = hdmirx_timer_handler;
	hdevp->timer.expires = jiffies + TIMER_STATE_CHECK;
	add_timer(&hdevp->timer);
	rx.boot_flag = true;
	rx_pr("hdmirx: driver probe ok\n");

	return 0;
fail_kmalloc_pd_fifo:
	return ret;
fail_get_resource_irq:
	return ret;
fail_create_arc_aud_type_file:
	device_remove_file(hdevp->dev, &dev_attr_arc_aud_type);
fail_create_cec_file:
	device_remove_file(hdevp->dev, &dev_attr_cec);
fail_create_esm_base_file:
	device_remove_file(hdevp->dev, &dev_attr_esm_base);
fail_create_info_file:
	device_remove_file(hdevp->dev, &dev_attr_info);
fail_create_param_file:
	device_remove_file(hdevp->dev, &dev_attr_param);
fail_create_reg_file:
	device_remove_file(hdevp->dev, &dev_attr_reg);
fail_create_log_file:
	device_remove_file(hdevp->dev, &dev_attr_log);
fail_create_key_file:
	device_remove_file(hdevp->dev, &dev_attr_key);
fail_create_edid_file:
	device_remove_file(hdevp->dev, &dev_attr_edid);
fail_create_debug_file:
	device_remove_file(hdevp->dev, &dev_attr_debug);

/* fail_get_resource_irq: */
	/* hdmirx_delete_device(hdevp->index); */
fail_create_device:
	cdev_del(&hdevp->cdev);
fail_add_cdev:
/* fail_get_id: */
	kfree(hdevp);
fail_kmalloc_hdev:
	return ret;
}

static int hdmirx_remove(struct platform_device *pdev)
{
	struct hdmirx_dev_s *hdevp;

	hdevp = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&eq_dwork);
	destroy_workqueue(eq_wq);

	cancel_delayed_work_sync(&esm_dwork);
	destroy_workqueue(esm_wq);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&hdmirx_early_suspend_handler);
#endif
	mutex_destroy(&hdevp->rx_lock);
	device_remove_file(hdevp->dev, &dev_attr_debug);
	device_remove_file(hdevp->dev, &dev_attr_edid);
	device_remove_file(hdevp->dev, &dev_attr_key);
	device_remove_file(hdevp->dev, &dev_attr_log);
	device_remove_file(hdevp->dev, &dev_attr_reg);
	device_remove_file(hdevp->dev, &dev_attr_cec);
	device_remove_file(hdevp->dev, &dev_attr_esm_base);
	device_remove_file(hdevp->dev, &dev_attr_info);
	device_remove_file(hdevp->dev, &dev_attr_arc_aud_type);
	tvin_unreg_frontend(&hdevp->frontend);
	hdmirx_delete_device(hdevp->index);
	tasklet_kill(&rx_tasklet);
	kfree(pd_fifo_buf);
	cdev_del(&hdevp->cdev);
	kfree(hdevp);
	rx_pr("hdmirx: driver removed ok.\n");
	return 0;
}


static int aml_hdcp22_pm_notify(struct notifier_block *nb,
	unsigned long event, void *dummy)
{
	int delay = 0;

	if (event == PM_SUSPEND_PREPARE && hdcp22_on) {
		hdcp22_kill_esm = 1;
		/*wait time out ESM_KILL_WAIT_TIMES*20 ms*/
		while (delay++ < ESM_KILL_WAIT_TIMES) {
			if (!hdcp22_kill_esm)
				break;
			msleep(20);
		}
		if (delay < ESM_KILL_WAIT_TIMES)
			rx_pr("hdcp22 kill ok!\n");
		else
			rx_pr("hdcp22 kill timeout!\n");
	}
	return NOTIFY_OK;
}

#ifdef CONFIG_PM
static int hdmirx_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct hdmirx_dev_s *hdevp;

	hdevp = platform_get_drvdata(pdev);
	rx_pr("[hdmirx]: hdmirx_suspend\n");
	del_timer_sync(&hdevp->timer);
	/* set HPD low when cec off. */
	if (!hdmi_cec_en)
		rx_force_hpd_cfg(0);

	if (suspend_pddq_sel == 0)
		rx_pr("don't set phy pddq down\n");
	else {
		/* there's no SDA low issue on MTK box when CEC off */
		if (hdmi_cec_en != 0) {
			if (suspend_pddq_sel == 2) {
				/* set rxsense pulse */
				mdelay(10);
				hdmirx_phy_pddq(1);
				mdelay(10);
				hdmirx_phy_pddq(0);
			}
		}
		/* phy powerdown */
		hdmirx_phy_pddq(1);
	}
	if (hdcp22_on)
		hdcp22_suspend();
	rx_pr("[hdmirx]: suspend success\n");
	return 0;
}

static int hdmirx_resume(struct platform_device *pdev)
{
	struct hdmirx_dev_s *hdevp;

	hdevp = platform_get_drvdata(pdev);
	if (hdmi_cec_en != 0) {
		if (suspend_pddq_sel == 1) {
			/* set rxsense pulse, if delay time between
			 * rxsense pulse and phy_int shottern than
			 * 50ms, SDA may be pulled low 800ms on MTK box
			 */
			hdmirx_phy_pddq(0);
			mdelay(10);
			hdmirx_phy_pddq(1);
			mdelay(50);
		}
	}
	hdmirx_phy_init();
	add_timer(&hdevp->timer);
	if (hdcp22_on)
		hdcp22_resume();
	rx_pr("hdmirx: resume\n");
	pre_port = 0xff;
	rx.boot_flag = true;
	return 0;
}
#endif

static void hdmirx_shutdown(struct platform_device *pdev)
{
	struct hdmirx_dev_s *hdevp;

	hdevp = platform_get_drvdata(pdev);
	rx_pr("[hdmirx]: hdmirx_shutdown\n");
	del_timer_sync(&hdevp->timer);
	/* set HPD low when cec off. */
	if (!hdmi_cec_en)
		rx_force_hpd_cfg(0);
	/* phy powerdown */
	hdmirx_phy_pddq(1);
	if (hdcp22_on)
		hdcp22_clk_en(0);
	rx_pr("[hdmirx]: shutdown success\n");
}

#ifdef CONFIG_HIBERNATION
static int hdmirx_restore(struct device *dev)
{
	/* queue_delayed_work(eq_wq, &eq_dwork, msecs_to_jiffies(5)); */
	return 0;
}
static int hdmirx_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return hdmirx_suspend(pdev, PMSG_SUSPEND);
}

static int hdmirx_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return hdmirx_resume(pdev);
}

const struct dev_pm_ops hdmirx_pm = {
	.restore	= hdmirx_restore,
	.suspend	= hdmirx_pm_suspend,
	.resume		= hdmirx_pm_resume,
};

#endif

static struct platform_driver hdmirx_driver = {
	.probe      = hdmirx_probe,
	.remove     = hdmirx_remove,
#ifdef CONFIG_PM
	.suspend    = hdmirx_suspend,
	.resume     = hdmirx_resume,
#endif
	.shutdown	= hdmirx_shutdown,
	.driver     = {
	.name   = TVHDMI_DRIVER_NAME,
	.owner	= THIS_MODULE,
	.of_match_table = hdmirx_dt_match,
#ifdef CONFIG_HIBERNATION
	.pm     = &hdmirx_pm,
#endif
	}
};

static int __init hdmirx_init(void)
{
	int ret = 0;
	/* struct platform_device *pdev; */

	if (init_flag & INIT_FLAG_NOT_LOAD)
		return 0;

	ret = alloc_chrdev_region(&hdmirx_devno, 0, 1, TVHDMI_NAME);
	if (ret < 0) {
		rx_pr("hdmirx: failed to allocate major number\n");
		goto fail_alloc_cdev_region;
	}

	hdmirx_clsp = class_create(THIS_MODULE, TVHDMI_NAME);
	if (IS_ERR(hdmirx_clsp)) {
		rx_pr("hdmirx: can't get hdmirx_clsp\n");
		ret = PTR_ERR(hdmirx_clsp);
		goto fail_class_create;
	}

	#if 0
	pdev = platform_device_alloc(TVHDMI_NAME, 0);
	if (IS_ERR(pdev)) {
		rx_pr("%s alloc platform device error.\n",
			__func__);
		goto fail_class_create;
	}
	if (platform_device_add(pdev)) {
		rx_pr("%s failed register platform device.\n",
			__func__);
		goto fail_class_create;
	}
	#endif
	ret = platform_driver_register(&hdmirx_driver);
	if (ret != 0) {
		rx_pr("register hdmirx module failed, error %d\n",
			ret);
		ret = -ENODEV;
		goto fail_pdrv_register;
	}
	rx_pr("hdmirx: hdmirx_init.\n");

	return 0;

fail_pdrv_register:
	class_destroy(hdmirx_clsp);
fail_class_create:
	unregister_chrdev_region(hdmirx_devno, 1);
fail_alloc_cdev_region:
	return ret;

}

static void __exit hdmirx_exit(void)
{
	class_destroy(hdmirx_clsp);
	unregister_chrdev_region(hdmirx_devno, 1);
	platform_driver_unregister(&hdmirx_driver);
	rx_pr("hdmirx: hdmirx_exit.\n");
}

module_init(hdmirx_init);
module_exit(hdmirx_exit);

MODULE_DESCRIPTION("AMLOGIC HDMIRX driver");
MODULE_LICENSE("GPL");
