/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/hdmi_tx_hw.c
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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
/* #include <linux/amports/canvas.h> */
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/hdmi_tx/enc_clk_config.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_ddc.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/reset.h>
#include <linux/compiler.h>
#include "mach_reg.h"
#include "hdmi_tx_reg.h"

#if 0   /* todo */
#include "../hdmi_tx_hdcp.h"
#include "../hdmi_tx_compliance.h"
#endif
#include "tvenc_conf.h"
#include "common.h"
#include "hdcpVerify.h"
#include "hw_clk.h"

#define EDID_RAM_ADDR_SIZE	 (8)

static void hdmi_audio_init(unsigned int spdif_flag);
static void hdmitx_dump_tvenc_reg(int cur_VIC, int pr_info_flag);

static void mode420_half_horizontal_para(void);
static void hdmi_phy_suspend(void);
static void hdmi_phy_wakeup(struct hdmitx_dev *hdev);
static void hdmitx_set_phy(struct hdmitx_dev *hdev);
static void hdmitx_set_hw(struct hdmitx_dev *hdev);
static void set_hdmi_audio_source(unsigned int src);
static void hdmitx_csc_config(unsigned char input_color_format,
	unsigned char output_color_format, unsigned char color_depth);
static int hdmitx_hdmi_dvi_config(struct hdmitx_dev *hdev,
	unsigned int dvi_mode);
static void hdmitx_set_avi_colorimetry(struct hdmi_format_para *para);

unsigned char hdmi_pll_mode; /* 1, use external clk as hdmi pll source */

/* HSYNC polarity: active high */
#define HSYNC_POLARITY	 1
/* VSYNC polarity: active high */
#define VSYNC_POLARITY	 1
/* Pixel format: 0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422. */
#define TX_INPUT_COLOR_FORMAT   COLORSPACE_YUV444
/* Pixel range: 0=16-235/240; 1=16-240; 2=1-254; 3=0-255. */
#define TX_INPUT_COLOR_RANGE	0
/* Pixel bit width: 4=24-bit; 5=30-bit; 6=36-bit; 7=48-bit. */
#define TX_COLOR_DEPTH		 COLORDEPTH_24B
/* Pixel format: 0=RGB444; 1=YCbCr422; 2=YCbCr444; 3=YCbCr420. */
#define TX_OUTPUT_COLOR_FORMAT  COLORSPACE_YUV444
#define TX_OUTPUT_COLOR_RANGE 0

#if 1
/* 0=I2S 2-channel; 1=I2S 4 x 2-channel. */
#define TX_I2S_8_CHANNEL	0
#endif

static unsigned int tx_aud_src; /* 0: SPDIF  1: I2S */

/* store downstream ksv lists */
static char *rptx_ksvs;
static char rptx_ksv_prbuf[1271]; /* 127 * 5 * 2 + 1 */
MODULE_PARM_DESC(rptx_ksvs, "\n downstream ksvs\n");
module_param(rptx_ksvs, charp, 0444);
static int rptx_ksv_no;
static int rptx_ksvlist_retry;
static char rptx_ksv_buf[635];

/* static struct tasklet_struct EDID_tasklet; */
static unsigned int delay_flag;
static unsigned long serial_reg_val = 0x1;
static unsigned char i2s_to_spdif_flag = 1;
static unsigned long color_depth_f;
static unsigned long COLORSPACE_f;
static unsigned char new_reset_sequence_flag = 1;
static unsigned char power_mode = 1;
static unsigned char power_off_vdac_flag;
/* 0, do not use fixed tvenc val for all mode;
 * 1, use fixed tvenc val mode for 480i;
 * 2, use fixed tvenc val mode for all modes
 */
static unsigned char use_tvenc_conf_flag = 1;

static void hdmitx_set_packet(int type, unsigned char *DB, unsigned char *HB);
static void hdmitx_setaudioinfoframe(unsigned char *AUD_DB,
	unsigned char *CHAN_STAT_BUF);
static int hdmitx_set_dispmode(struct hdmitx_dev *hdev);
static int hdmitx_set_audmode(struct hdmitx_dev *hdev,
	struct hdmitx_audpara *audio_param);
static void hdmitx_setupirq(struct hdmitx_dev *hdev);
static void hdmitx_debug(struct hdmitx_dev *hdev, const char *buf);
static void hdmitx_uninit(struct hdmitx_dev *hdev);
static int hdmitx_cntl(struct hdmitx_dev *hdev, unsigned int cmd,
	unsigned int argv);
static int hdmitx_cntl_ddc(struct hdmitx_dev *hdev, unsigned int cmd,
	unsigned long argv);
static int hdmitx_get_state(struct hdmitx_dev *hdev, unsigned int cmd,
	unsigned int argv);
static int hdmitx_cntl_config(struct hdmitx_dev *hdev, unsigned int cmd,
	unsigned int argv);
static int hdmitx_cntl_misc(struct hdmitx_dev *hdev, unsigned int cmd,
	unsigned int  argv);
static void digital_clk_on(unsigned char flag);
static void digital_clk_off(unsigned char flag);

static int hdmitx_hpd_hw_op(enum hpd_op cmd)
{
	switch (get_cpu_type()) {
	case MESON_CPU_MAJOR_ID_GXBB:
		return hdmitx_hpd_hw_op_gxbb(cmd);
	case MESON_CPU_MAJOR_ID_GXTVBB:
		return hdmitx_hpd_hw_op_gxtvbb(cmd);
	case MESON_CPU_MAJOR_ID_GXL:
	case MESON_CPU_MAJOR_ID_GXM:
		return hdmitx_hpd_hw_op_gxl(cmd);
	case MESON_CPU_MAJOR_ID_TXLX:
		return hdmitx_hpd_hw_op_txlx(cmd);
	default:
		break;
	}
	return 0;
}

int read_hpd_gpio(void)
{
	switch (get_cpu_type()) {
	case MESON_CPU_MAJOR_ID_GXBB:
		return read_hpd_gpio_gxbb();
	case MESON_CPU_MAJOR_ID_GXTVBB:
		return read_hpd_gpio_gxtvbb();
	case MESON_CPU_MAJOR_ID_GXL:
	case MESON_CPU_MAJOR_ID_GXM:
		return read_hpd_gpio_gxl();
	case MESON_CPU_MAJOR_ID_TXLX:
		return read_hpd_gpio_txlx();
	default:
		break;
	}
	return 0;
}
EXPORT_SYMBOL(read_hpd_gpio);

int hdmitx_ddc_hw_op(enum ddc_op cmd)
{
	switch (get_cpu_type()) {
	case MESON_CPU_MAJOR_ID_GXBB:
		return hdmitx_ddc_hw_op_gxbb(cmd);
	case MESON_CPU_MAJOR_ID_GXTVBB:
		return hdmitx_ddc_hw_op_gxtvbb(cmd);
	case MESON_CPU_MAJOR_ID_GXL:
	case MESON_CPU_MAJOR_ID_GXM:
		return hdmitx_ddc_hw_op_gxl(cmd);
	case MESON_CPU_MAJOR_ID_TXLX:
		return hdmitx_ddc_hw_op_txlx(cmd);
	default:
		break;
	}
	return 0;
}

int hdmitx_hdcp_opr(unsigned int val)
{
	if (val == 1) { /* HDCP14_ENABLE */
		register long x0 asm("x0") = 0x82000010;
		asm volatile(
			__asmeq("%0", "x0")
			"smc #0\n"
			: : "r"(x0)
		);
	}
	if (val == 2) { /* HDCP14_RESULT */
		register long x0 asm("x0") = 0x82000011;
		asm volatile(
			__asmeq("%0", "x0")
			"smc #0\n"
			: "+r"(x0)
		);
		return (unsigned int)(x0&0xffffffff);
	}
	if (val == 0) { /* HDCP14_INIT */
		register long x0 asm("x0") = 0x82000012;
		asm volatile(
			__asmeq("%0", "x0")
			"smc #0\n"
			: : "r"(x0)
		);
	}
	if (val == 3) { /* HDCP14_EN_ENCRYPT */
		register long x0 asm("x0") = 0x82000013;
		asm volatile(
			__asmeq("%0", "x0")
			"smc #0\n"
			: : "r"(x0)
		);
	}
	if (val == 4) { /* HDCP14_OFF */
		register long x0 asm("x0") = 0x82000014;
		asm volatile(
			__asmeq("%0", "x0")
			"smc #0\n"
			: : "r"(x0)
		);
	}
	if (val == 5) {	/* HDCP_MUX_22 */
		register long x0 asm("x0") = 0x82000015;
		asm volatile(
			__asmeq("%0", "x0")
			"smc #0\n"
			: : "r"(x0)
		);
	}
	if (val == 6) {	/* HDCP_MUX_14 */
		register long x0 asm("x0") = 0x82000016;
		asm volatile(
			__asmeq("%0", "x0")
			"smc #0\n"
			: : "r"(x0)
		);
	}
	if (val == 7) { /* HDCP22_RESULT */
		register long x0 asm("x0") = 0x82000017;
		asm volatile(
			__asmeq("%0", "x0")
			"smc #0\n"
			: "+r"(x0)
		);
		return (unsigned int)(x0&0xffffffff);
	}
	if (val == 0xa) { /* HDCP14_KEY_LSTORE */
		register long x0 asm("x0") = 0x8200001a;
		asm volatile(
			__asmeq("%0", "x0")
			"smc #0\n"
			: "+r"(x0)
		);
		return (unsigned int)(x0&0xffffffff);
	}
	if (val == 0xb) { /* HDCP22_KEY_LSTORE */
		register long x0 asm("x0") = 0x8200001b;
		asm volatile(
			__asmeq("%0", "x0")
			"smc #0\n"
			: "+r"(x0)
		);
		return (unsigned int)(x0&0xffffffff);
	}
	if (val == 0xc) { /* HDCP22_KEY_SET_DUK */
		register long x0 asm("x0") = 0x8200001c;
		asm volatile(
			__asmeq("%0", "x0")
			"smc #0\n"
			: "+r"(x0)
		);
		return (unsigned int)(x0&0xffffffff);
	}
	return -1;
}

static void config_avmute(unsigned int val)
{
	pr_info("avmute set to %d\n", val);
	switch (val) {
	case SET_AVMUTE:
		hdmitx_set_reg_bits(HDMITX_DWC_FC_GCP, 1, 1, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_GCP, 0, 0, 1);
		break;
	case CLR_AVMUTE:
		hdmitx_set_reg_bits(HDMITX_DWC_FC_GCP, 0, 1, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_GCP, 1, 0, 1);
		break;
	case OFF_AVMUTE:
	default:
		hdmitx_set_reg_bits(HDMITX_DWC_FC_GCP, 0, 1, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_GCP, 0, 0, 1);
		break;
	}
}

static void config_video_mapping(enum hdmi_color_space cs,
	enum hdmi_color_depth cd)
{
	unsigned int val = 0;

	pr_info("hdmitx: config: cs = %d cd = %d\n", cs, cd);
	switch (cs) {
	case COLORSPACE_RGB444:
		switch (cd) {
		case COLORDEPTH_24B:
			val = 0x1;
			break;
		case COLORDEPTH_30B:
			val = 0x3;
			break;
		case COLORDEPTH_36B:
			val = 0x5;
			break;
		case COLORDEPTH_48B:
			val = 0x7;
			break;
		default:
			break;
		}
		break;
	case COLORSPACE_YUV444:
	case COLORSPACE_YUV420:
		switch (cd) {
		case COLORDEPTH_24B:
			val = 0x9;
			break;
		case COLORDEPTH_30B:
			val = 0xb;
			break;
		case COLORDEPTH_36B:
			val = 0xd;
			break;
		case COLORDEPTH_48B:
			val = 0xf;
			break;
		default:
			break;
		}
		break;
	case COLORSPACE_YUV422:
		switch (cd) {
		case COLORDEPTH_24B:
			val = 0x16;
			break;
		case COLORDEPTH_30B:
			val = 0x14;
			break;
		case COLORDEPTH_36B:
			val = 0x12;
			break;
		case COLORDEPTH_48B:
			pr_info("hdmitx: y422 no 48bits mode\n");
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	hdmitx_set_reg_bits(HDMITX_DWC_TX_INVID0, val, 0, 4);

	switch (cd) {
	case COLORDEPTH_24B:
		val = 0x4;
		break;
	case COLORDEPTH_30B:
		val = 0x5;
		break;
	case COLORDEPTH_36B:
		val = 0x6;
		break;
	case COLORDEPTH_48B:
		val = 0x7;
		break;
	default:
		break;
	}
	hdmitx_set_reg_bits(HDMITX_DWC_VP_PR_CD, val, 4, 4);

	switch (cd) {
	case COLORDEPTH_30B:
	case COLORDEPTH_36B:
	case COLORDEPTH_48B:
		hdmitx_set_reg_bits(HDMITX_DWC_VP_CONF, 0, 6, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_VP_CONF, 1, 5, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_VP_CONF, 1, 2, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_VP_CONF, 0, 0, 2);
		break;
	case COLORDEPTH_24B:
		break;
	default:
		break;
	}
	hdmitx_set_reg_bits(HDMITX_DWC_VP_PR_CD, val, 4, 4);
}

/* record HDMITX current format, matched with uboot */
/* ISA_DEBUG_REG0 0x2600
 * bit[11]: Y420
 * bit[10:8]: HDMI VIC
 * bit[7:0]: CEA VIC
 */
static unsigned int get_hdmitx_format(void)
{
	return hd_read_reg(P_ISA_DEBUG_REG0);
}

static int hdmitx_uboot_already_display(void)
{
	if ((hd_read_reg(P_HHI_HDMI_CLK_CNTL) & (1 << 8))
		&& (hd_read_reg(P_HHI_HDMI_PLL_CNTL) & (1 << 31))
		&& (get_hdmitx_format())) {
		pr_info("hdmitx: alread display in uboot 0x%x\n",
			get_hdmitx_format());
		return 1;
	} else
		return 0;
}

/* for 30bits colordepth */
static void set_vmode_clk(struct hdmitx_dev *hdev)
{
	hdmitx_set_clk(hdev);
}

static void hdmi_hwp_init(struct hdmitx_dev *hdev)
{
	/* Enable clocks and bring out of reset */

	/* Enable hdmitx_sys_clk */
	/* .clk0 ( cts_oscin_clk ), */
	/* .clk1 ( fclk_div4 ), */
	/* .clk2 ( fclk_div3 ), */
	/* .clk3 ( fclk_div5 ), */
/* [10: 9] clk_sel. select cts_oscin_clk=24MHz */
/* [	8] clk_en. Enable gated clock */
/* [ 6: 0] clk_div. Divide by 1. = 24/1 = 24 MHz */
	hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 0x100, 0, 16);

	hd_write_reg(P_HHI_HDCP22_CLK_CNTL, 0x01000100);
	hd_set_reg_bits(P_HHI_GCLK_MPEG2, 1, 3, 1);

/* Enable clk81_hdmitx_pclk */
	hd_set_reg_bits(P_HHI_GCLK_MPEG2, 1, 4, 1);
	/* wire	wr_enable = control[3]; */
	/* wire	fifo_enable = control[2]; */
	/* assign phy_clk_en = control[1]; */
/* Bring HDMITX MEM output of power down */
	hd_set_reg_bits(P_HHI_MEM_PD_REG0, 0, 8, 8);
	if (hdmitx_uboot_already_display()) {
		/* Get uboot output color space from AVI */
		switch (hdmitx_rd_reg(HDMITX_DWC_FC_AVICONF0) & 0x3) {
		case 0:
			hdev->para->cs = COLORSPACE_RGB444;
			break;
		case 1:
			hdev->para->cs = COLORSPACE_YUV422;
			break;
		case 2:
			hdev->para->cs = COLORSPACE_YUV444;
			break;
		case 3:
			hdev->para->cs = COLORSPACE_YUV420;
			break;
		default:
			break;
		}
		/* If color space is not 422, then get depth from VP_PR_CD */
		if (hdev->para->cs != COLORSPACE_YUV422) {
			switch ((hdmitx_rd_reg(HDMITX_DWC_VP_PR_CD) >> 4) &
				0xf) {
			case 5:
				hdev->para->cd = COLORDEPTH_30B;
				break;
			case 6:
				hdev->para->cd = COLORDEPTH_36B;
				break;
			case 7:
				hdev->para->cd = COLORDEPTH_48B;
				break;
			case 0:
			case 4:
			default:
				hdev->para->cd = COLORDEPTH_24B;
				break;
			}
		} else {
			/* If colorspace is 422, then get depth from VP_REMAP */
			switch (hdmitx_rd_reg(HDMITX_DWC_VP_REMAP) & 0x3) {
			case 1:
				hdev->para->cd = COLORDEPTH_30B;
				break;
			case 2:
				hdev->para->cd = COLORDEPTH_36B;
				break;
			case 0:
			default:
				hdev->para->cd = COLORDEPTH_24B;
				break;
			}
		}
		return;
	}

	hdev->para->cd = COLORDEPTH_RESERVED;
	hdev->para->cs = COLORSPACE_RESERVED;
	/* reset HDMITX APB & TX & PHY */
	hd_set_reg_bits(P_RESET0_REGISTER, 1, 19, 1);
	hd_set_reg_bits(P_RESET2_REGISTER, 1, 15, 1);
	hd_set_reg_bits(P_RESET2_REGISTER, 1,  2, 1);
	/* Enable APB3 fail on error */
	hd_set_reg_bits(P_HDMITX_CTRL_PORT, 1, 15, 1);
	hd_set_reg_bits((P_HDMITX_CTRL_PORT + 0x10), 1, 15, 1);
	/* Bring out of reset */
	hdmitx_wr_reg(HDMITX_TOP_SW_RESET,  0);
	udelay(200);
	hdmitx_set_reg_bits(HDMITX_TOP_CLK_CNTL, 3, 0, 2);
	hdmitx_set_reg_bits(HDMITX_TOP_CLK_CNTL, 3, 4, 2);
	hdmitx_wr_reg(HDMITX_DWC_MC_LOCKONCLOCK, 0xff);
	hdmitx_wr_reg(HDMITX_TOP_INTR_MASKN, 0x1f);
}

static void hdmi_hwi_init(struct hdmitx_dev *hdev)
{
	unsigned int data32 = 0;

	hdmitx_set_reg_bits(HDMITX_DWC_FC_INVIDCONF, 1, 7, 1);
	hdmitx_wr_reg(HDMITX_DWC_A_HDCPCFG1, 0x7);
	hdmitx_wr_reg(HDMITX_DWC_A_HDCPCFG0, 0x53);
	/* Enable skpclk to HDCP2.2 IP */
	hdmitx_set_reg_bits(HDMITX_TOP_CLK_CNTL, 1, 7, 1);
	/* Enable esmclk to HDCP2.2 IP */
	hdmitx_set_reg_bits(HDMITX_TOP_CLK_CNTL, 1, 6, 1);
	/* Enable tmds_clk to HDCP2.2 IP */
	hdmitx_set_reg_bits(HDMITX_TOP_CLK_CNTL, 1, 5, 1);

	hdmitx_hpd_hw_op(HPD_INIT_DISABLE_PULLUP);
	hdmitx_hpd_hw_op(HPD_INIT_SET_FILTER);
	hdmitx_ddc_hw_op(DDC_INIT_DISABLE_PULL_UP_DN);
	hdmitx_ddc_hw_op(DDC_MUX_DDC);

/* Configure E-DDC interface */
	data32  = 0;
	data32 |= (1    << 24); /* [26:24] infilter_ddc_intern_clk_divide */
	data32 |= (0    << 16); /* [23:16] infilter_ddc_sample_clk_divide */
	data32 |= (0    << 8);  /* [10: 8] infilter_cec_intern_clk_divide */
	data32 |= (1    << 0);  /* [ 7: 0] infilter_cec_sample_clk_divide */
	hdmitx_wr_reg(HDMITX_TOP_INFILTER, data32);

	data32 = 0;
	data32 |= (0 << 6);  /* [  6] read_req_mask */
	data32 |= (0 << 2);  /* [  2] done_mask */
	hdmitx_wr_reg(HDMITX_DWC_I2CM_INT, data32);

	data32 = 0;
	data32 |= (0 << 6);  /* [  6] nack_mask */
	data32 |= (0 << 2);  /* [  2] arbitration_error_mask */
	hdmitx_wr_reg(HDMITX_DWC_I2CM_CTLINT, data32);

/* [  3] i2c_fast_mode: 0=standard mode; 1=fast mode. */
	data32 = 0;
	data32 |= (0 << 3);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_DIV, data32);

	hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_HCNT_1, 0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_HCNT_0, 0x67);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_LCNT_1, 0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_LCNT_0, 0x78);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_FS_SCL_HCNT_1, 0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_FS_SCL_HCNT_0, 0x0f);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_FS_SCL_LCNT_1, 0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_FS_SCL_LCNT_0, 0x20);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SDA_HOLD,	0x08);

	data32 = 0;
	data32 |= (0 << 5);  /* [  5] updt_rd_vsyncpoll_en */
	data32 |= (0 << 4);  /* [  4] read_request_en  // scdc */
	data32 |= (0 << 0);  /* [  0] read_update */
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SCDC_UPDATE,  data32);
}

void HDMITX_Meson_Init(struct hdmitx_dev *hdev)
{
	hdev->HWOp.SetPacket = hdmitx_set_packet;
	hdev->HWOp.SetAudioInfoFrame = hdmitx_setaudioinfoframe;
	hdev->HWOp.SetDispMode = hdmitx_set_dispmode;
	hdev->HWOp.SetAudMode = hdmitx_set_audmode;
	hdev->HWOp.SetupIRQ = hdmitx_setupirq;
	hdev->HWOp.DebugFun = hdmitx_debug;
	hdev->HWOp.UnInit = hdmitx_uninit;
	hdev->HWOp.Cntl = hdmitx_cntl;	/* todo */
	hdev->HWOp.CntlDDC = hdmitx_cntl_ddc;
	hdev->HWOp.GetState = hdmitx_get_state;
	hdev->HWOp.CntlPacket = hdmitx_cntl;
	hdev->HWOp.CntlConfig = hdmitx_cntl_config;
	hdev->HWOp.CntlMisc = hdmitx_cntl_misc;
	init_reg_map(hdev->chip_type);
	digital_clk_on(0xff);
	hdmi_hwp_init(hdev);
	hdmi_hwi_init(hdev);
	config_avmute(CLR_AVMUTE);
	hdmitx_set_audmode(NULL, NULL);
	rptx_ksvs = &rptx_ksv_prbuf[0];
}

static irqreturn_t intr_handler(int irq, void *dev)
{
	unsigned int data32 = 0;
	struct hdmitx_dev *hdev = (struct hdmitx_dev *)dev;
	/* get interrupt status */
	data32 = hdmitx_rd_reg(HDMITX_TOP_INTR_STAT);
	hdmi_print(IMP, SYS "irq %x\n", data32);
	if (hdev->hpd_lock == 1) {
		hdmitx_wr_reg(HDMITX_TOP_INTR_STAT_CLR, 0xf);
		hdmi_print(IMP, HPD "HDMI hpd locked\n");
		goto next;
	}
	/* check HPD status */
	if ((data32 & (1 << 1)) && (data32 & (1 << 2))) {
		if (hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO))
			data32 &= ~(1 << 2);
		else
			data32 &= ~(1 << 1);
	}
	/* HPD rising */
	if (data32 & (1 << 1)) {
		hdev->hdmitx_event |= HDMI_TX_HPD_PLUGIN;
		hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGOUT;
		queue_delayed_work(hdev->hdmi_wq,
			&hdev->work_hpd_plugin, HZ / 2);
	}
	/* HPD falling */
	if (data32 & (1 << 2)) {
		hdev->hdmitx_event |= HDMI_TX_HPD_PLUGOUT;
		hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGIN;
		queue_delayed_work(hdev->hdmi_wq,
			&hdev->work_hpd_plugout, 0);
	}
next:
	/* internal interrupt */
	if (data32 & (1 << 0)) {
		hdev->hdmitx_event |= HDMI_TX_INTERNAL_INTR;
		queue_work(hdev->hdmi_wq, &hdev->work_internal_intr);
	}
	if (data32 & (1 << 3)) {
		unsigned int rd_nonce_mode =
			hdmitx_rd_reg(HDMITX_TOP_SKP_CNTL_STAT) & 0x1;
		pr_info("hdcp22: Nonce %s  Vld: %d\n",
			rd_nonce_mode ? "HW" : "SW",
			((hdmitx_rd_reg(HDMITX_TOP_SKP_CNTL_STAT) >> 31) & 1));
		if (rd_nonce_mode)
			hdmitx_wr_reg(HDMITX_TOP_INTR_STAT_CLR, (1 << 3));
		else {
			hdmitx_wr_reg(HDMITX_TOP_NONCE_0,  0x32107654);
			hdmitx_wr_reg(HDMITX_TOP_NONCE_1,  0xba98fedc);
			hdmitx_wr_reg(HDMITX_TOP_NONCE_2,  0xcdef89ab);
			hdmitx_wr_reg(HDMITX_TOP_NONCE_3,  0x45670123);
			hdmitx_wr_reg(HDMITX_TOP_NONCE_0,  0x76543210);
			hdmitx_wr_reg(HDMITX_TOP_NONCE_1,  0xfedcba98);
			hdmitx_wr_reg(HDMITX_TOP_NONCE_2,  0x89abcdef);
			hdmitx_wr_reg(HDMITX_TOP_NONCE_3,  0x01234567);
		}
	}
	if (data32 & (1 << 30)) {
		pr_info("hdcp22: reg stat: 0x%x\n",
			hdmitx_rd_reg(HDMITX_DWC_HDCP22REG_STAT));
		hdmitx_wr_reg(HDMITX_DWC_HDCP22REG_STAT, 0xff);
	}
	/* ack INTERNAL_INTR or else we stuck with no interrupts at all */
	hdmitx_wr_reg(HDMITX_TOP_INTR_STAT_CLR, data32 | 0x7);
	return IRQ_HANDLED;
}

static unsigned long modulo(unsigned long a, unsigned long b)
{
	if (a >= b)
		return a - b;
	else
		return a;
}

static signed int to_signed(unsigned int a)
{
	if (a <= 7)
		return a;
	else
		return a - 16;
}

static void delay_us(int us)
{
	/* udelay(us); */
	if (delay_flag&0x1)
		mdelay((us+999)/1000);
} /* delay_us */

/*
 * mode: 1 means Progressive;  0 means interlaced
 */
static void enc_vpu_bridge_reset(int mode)
{
	unsigned int wr_clk = 0;

	wr_clk = (hd_read_reg(P_VPU_HDMI_SETTING) & 0xf00) >> 8;
	if (mode) {
		hd_write_reg(P_ENCP_VIDEO_EN, 0);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 0, 0, 2);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 0, 8, 4);
		mdelay(1);
		hd_write_reg(P_ENCP_VIDEO_EN, 1);
		mdelay(1);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, wr_clk, 8, 4);
		mdelay(1);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 2, 0, 2);
	} else {
		hd_write_reg(P_ENCI_VIDEO_EN, 0);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 0, 0, 2);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 0, 8, 4);
		mdelay(1);
		hd_write_reg(P_ENCI_VIDEO_EN, 1);
		mdelay(1);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, wr_clk, 8, 4);
		mdelay(1);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 0, 2);
	}
}

static void hdmi_tvenc1080i_set(struct hdmitx_vidpara *param)
{
	unsigned long VFIFO2VD_TO_HDMI_LATENCY = 2;
	unsigned long TOTAL_PIXELS = 0, PIXEL_REPEAT_HDMI = 0,
		PIXEL_REPEAT_VENC = 0, ACTIVE_PIXELS = 0;
	unsigned int FRONT_PORCH = 88, HSYNC_PIXELS = 0, ACTIVE_LINES = 0,
		INTERLACE_MODE = 0, TOTAL_LINES = 0, SOF_LINES = 0,
		VSYNC_LINES = 0;
	unsigned int LINES_F0 = 0, LINES_F1 = 563, BACK_PORCH = 0,
		EOF_LINES = 2, TOTAL_FRAMES = 0;

	unsigned long total_pixels_venc = 0;
	unsigned long active_pixels_venc = 0;
	unsigned long front_porch_venc = 0;
	unsigned long hsync_pixels_venc = 0;

	unsigned long de_h_begin = 0, de_h_end = 0;
	unsigned long de_v_begin_even = 0, de_v_end_even = 0,
		de_v_begin_odd = 0, de_v_end_odd = 0;
	unsigned long hs_begin = 0, hs_end = 0;
	unsigned long vs_adjust = 0;
	unsigned long vs_bline_evn = 0, vs_eline_evn = 0,
		vs_bline_odd = 0, vs_eline_odd = 0;
	unsigned long vso_begin_evn = 0, vso_begin_odd = 0;

	if (param->VIC == HDMI_1080i60) {
		INTERLACE_MODE = 1;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (1920*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (1080/(1+INTERLACE_MODE));
		LINES_F0 = 562;
		LINES_F1 = 563;
		FRONT_PORCH = 88;
		HSYNC_PIXELS = 44;
		BACK_PORCH = 148;
		EOF_LINES = 2;
		VSYNC_LINES = 5;
		SOF_LINES = 15;
		TOTAL_FRAMES = 4;
	} else if (param->VIC == HDMI_1080i50) {
		INTERLACE_MODE = 1;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (1920*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (1080/(1+INTERLACE_MODE));
		LINES_F0 = 562;
		LINES_F1 = 563;
		FRONT_PORCH = 528;
		HSYNC_PIXELS = 44;
		BACK_PORCH = 148;
		EOF_LINES = 2;
		VSYNC_LINES = 5;
		SOF_LINES = 15;
		TOTAL_FRAMES = 4;
	}
	TOTAL_PIXELS = (FRONT_PORCH+HSYNC_PIXELS+BACK_PORCH+ACTIVE_PIXELS);
	TOTAL_LINES = (LINES_F0+(LINES_F1*INTERLACE_MODE));

	total_pixels_venc = (TOTAL_PIXELS / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	active_pixels_venc = (ACTIVE_PIXELS / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	front_porch_venc = (FRONT_PORCH / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	hsync_pixels_venc =
		(HSYNC_PIXELS / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC);

	hd_write_reg(P_ENCP_VIDEO_MODE, hd_read_reg(P_ENCP_VIDEO_MODE)|(1<<14));

	/* Program DE timing */
	de_h_begin = modulo(hd_read_reg(P_ENCP_VIDEO_HAVON_BEGIN) +
		VFIFO2VD_TO_HDMI_LATENCY, total_pixels_venc);
	de_h_end  = modulo(de_h_begin + active_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DE_H_BEGIN, de_h_begin);
	hd_write_reg(P_ENCP_DE_H_END, de_h_end);
	/* Program DE timing for even field */
	de_v_begin_even = hd_read_reg(P_ENCP_VIDEO_VAVON_BLINE);
	de_v_end_even  = de_v_begin_even + ACTIVE_LINES;
	hd_write_reg(P_ENCP_DE_V_BEGIN_EVEN, de_v_begin_even);
	hd_write_reg(P_ENCP_DE_V_END_EVEN,  de_v_end_even);
	/* Program DE timing for odd field if needed */
	if (INTERLACE_MODE) {
		de_v_begin_odd = to_signed((
			hd_read_reg(P_ENCP_VIDEO_OFLD_VOAV_OFST) & 0xf0)>>4)
			+ de_v_begin_even + (TOTAL_LINES-1)/2;
		de_v_end_odd = de_v_begin_odd + ACTIVE_LINES;
		hd_write_reg(P_ENCP_DE_V_BEGIN_ODD, de_v_begin_odd);/* 583 */
		hd_write_reg(P_ENCP_DE_V_END_ODD, de_v_end_odd);  /* 1123 */
	}

	/* Program Hsync timing */
	if (de_h_end + front_porch_venc >= total_pixels_venc) {
		hs_begin = de_h_end + front_porch_venc - total_pixels_venc;
		vs_adjust  = 1;
	} else {
		hs_begin = de_h_end + front_porch_venc;
		vs_adjust  = 0;
	}
	hs_end = modulo(hs_begin + hsync_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DVI_HSO_BEGIN,  hs_begin);
	hd_write_reg(P_ENCP_DVI_HSO_END, hs_end);

	/* Program Vsync timing for even field */
	if (de_v_begin_even >= SOF_LINES + VSYNC_LINES + (1-vs_adjust))
		vs_bline_evn = de_v_begin_even - SOF_LINES - VSYNC_LINES
			- (1-vs_adjust);
	else
		vs_bline_evn = TOTAL_LINES + de_v_begin_even - SOF_LINES
			- VSYNC_LINES - (1-vs_adjust);

	vs_eline_evn = modulo(vs_bline_evn + VSYNC_LINES, TOTAL_LINES);
	hd_write_reg(P_ENCP_DVI_VSO_BLINE_EVN, vs_bline_evn);   /* 0 */
	hd_write_reg(P_ENCP_DVI_VSO_ELINE_EVN, vs_eline_evn);   /* 5 */
	vso_begin_evn = hs_begin; /* 2 */
	hd_write_reg(P_ENCP_DVI_VSO_BEGIN_EVN, vso_begin_evn);  /* 2 */
	hd_write_reg(P_ENCP_DVI_VSO_END_EVN, vso_begin_evn);  /* 2 */
	/* Program Vsync timing for odd field if needed */
	if (INTERLACE_MODE) {
		vs_bline_odd = de_v_begin_odd-1 - SOF_LINES - VSYNC_LINES;
		vs_eline_odd = de_v_begin_odd-1 - SOF_LINES;
		vso_begin_odd  = modulo(hs_begin + (total_pixels_venc>>1),
			total_pixels_venc);
		hd_write_reg(P_ENCP_DVI_VSO_BLINE_ODD, vs_bline_odd);
		hd_write_reg(P_ENCP_DVI_VSO_ELINE_ODD, vs_eline_odd);
		hd_write_reg(P_ENCP_DVI_VSO_BEGIN_ODD, vso_begin_odd);
		hd_write_reg(P_ENCP_DVI_VSO_END_ODD, vso_begin_odd);
	}

	hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
		(0 << 1) |
		(HSYNC_POLARITY << 2) |
		(VSYNC_POLARITY << 3) |
		(0 << 4) |
		(4 << 5) |
		(1 << 8) |
		(0 << 12)
	);
	hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
}

static void hdmi_tvenc4k2k_set(struct hdmitx_vidpara *param)
{
	unsigned long VFIFO2VD_TO_HDMI_LATENCY = 2;
	unsigned long TOTAL_PIXELS = 4400, PIXEL_REPEAT_HDMI = 0,
		PIXEL_REPEAT_VENC = 0, ACTIVE_PIXELS = 3840;
	unsigned int FRONT_PORCH = 1020, HSYNC_PIXELS = 0, ACTIVE_LINES = 2160,
		INTERLACE_MODE = 0U, TOTAL_LINES = 0, SOF_LINES = 0,
		VSYNC_LINES = 0;
	unsigned int LINES_F0 = 2250, LINES_F1 = 2250, BACK_PORCH = 0,
		EOF_LINES = 8, TOTAL_FRAMES = 0;

	unsigned long total_pixels_venc = 0;
	unsigned long active_pixels_venc = 0;
	unsigned long front_porch_venc = 0;
	unsigned long hsync_pixels_venc = 0;

	unsigned long de_h_begin = 0, de_h_end = 0;
	unsigned long de_v_begin_even = 0, de_v_end_even = 0;
	unsigned long hs_begin = 0, hs_end = 0;
	unsigned long vs_adjust = 0;
	unsigned long vs_bline_evn = 0, vs_eline_evn  = 0;
	unsigned long vso_begin_evn = 0;

	switch (param->VIC) {
	case HDMI_4k2k_30:
	case HDMI_3840x2160p60_16x9:
	case HDMI_3840x2160p60_16x9_Y420:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (3840*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (2160/(1+INTERLACE_MODE));
		LINES_F0 = 2250;
		LINES_F1 = 2250;
		FRONT_PORCH = 176;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 8 + 1;
		VSYNC_LINES = 10;
		SOF_LINES = 72 + 1;
		TOTAL_FRAMES = 3;
		break;
	case HDMI_4k2k_25:
	case HDMI_3840x2160p50_16x9:
	case HDMI_3840x2160p50_16x9_Y420:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (3840*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (2160/(1+INTERLACE_MODE));
		LINES_F0 = 2250;
		LINES_F1 = 2250;
		FRONT_PORCH = 1056;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 8 + 1;
		VSYNC_LINES = 10;
		SOF_LINES = 72 + 1;
		TOTAL_FRAMES = 3;
		break;
	case HDMI_4k2k_24:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (3840*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (2160/(1+INTERLACE_MODE));
		LINES_F0 = 2250;
		LINES_F1 = 2250;
		FRONT_PORCH = 1276;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 8 + 1;
		VSYNC_LINES = 10;
		SOF_LINES = 72 + 1;
		TOTAL_FRAMES = 3;
		break;
	case HDMI_4k2k_smpte_24:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (4096*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (2160/(1+INTERLACE_MODE));
		LINES_F0 = 2250;
		LINES_F1 = 2250;
		FRONT_PORCH = 1020;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 8 + 1;
		VSYNC_LINES = 10;
		SOF_LINES = 72 + 1;
		TOTAL_FRAMES = 3;
		break;
	case HDMI_4096x2160p25_256x135:
	case HDMI_4096x2160p50_256x135:
	case HDMI_4096x2160p50_256x135_Y420:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (4096*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (2160/(1+INTERLACE_MODE));
		LINES_F0 = 2250;
		LINES_F1 = 2250;
		FRONT_PORCH = 968;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 128;
		EOF_LINES = 8;
		VSYNC_LINES = 10;
		SOF_LINES = 72;
		TOTAL_FRAMES = 3;
		break;
	case HDMI_4096x2160p30_256x135:
	case HDMI_4096x2160p60_256x135:
	case HDMI_4096x2160p60_256x135_Y420:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (4096*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (2160/(1+INTERLACE_MODE));
		LINES_F0 = 2250;
		LINES_F1 = 2250;
		FRONT_PORCH = 88;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 128;
		EOF_LINES = 8;
		VSYNC_LINES = 10;
		SOF_LINES = 72;
		TOTAL_FRAMES = 3;
		break;
	default:
		pr_info("hdmitx20: no setting for VIC = %d\n", param->VIC);
		break;
	}

	TOTAL_PIXELS = (FRONT_PORCH+HSYNC_PIXELS+BACK_PORCH+ACTIVE_PIXELS);
	TOTAL_LINES = (LINES_F0+(LINES_F1*INTERLACE_MODE));

	total_pixels_venc = (TOTAL_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	active_pixels_venc = (ACTIVE_PIXELS / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	front_porch_venc = (FRONT_PORCH   / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	hsync_pixels_venc = (HSYNC_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);

	de_h_begin = modulo(hd_read_reg(P_ENCP_VIDEO_HAVON_BEGIN) +
		VFIFO2VD_TO_HDMI_LATENCY, total_pixels_venc);
	de_h_end  = modulo(de_h_begin + active_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DE_H_BEGIN, de_h_begin);
	hd_write_reg(P_ENCP_DE_H_END, de_h_end);
	/* Program DE timing for even field */
	de_v_begin_even = hd_read_reg(P_ENCP_VIDEO_VAVON_BLINE);
	de_v_end_even  = modulo(de_v_begin_even + ACTIVE_LINES, TOTAL_LINES);
	hd_write_reg(P_ENCP_DE_V_BEGIN_EVEN, de_v_begin_even);
	hd_write_reg(P_ENCP_DE_V_END_EVEN,  de_v_end_even);

	/* Program Hsync timing */
	if (de_h_end + front_porch_venc >= total_pixels_venc) {
		hs_begin = de_h_end + front_porch_venc - total_pixels_venc;
		vs_adjust  = 1;
	} else {
		hs_begin = de_h_end + front_porch_venc;
		vs_adjust  = 1;
	}
	hs_end = modulo(hs_begin + hsync_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DVI_HSO_BEGIN,  hs_begin);
	hd_write_reg(P_ENCP_DVI_HSO_END, hs_end);

	/* Program Vsync timing for even field */
	if (de_v_begin_even >= SOF_LINES + VSYNC_LINES + (1-vs_adjust))
		vs_bline_evn = de_v_begin_even - SOF_LINES - VSYNC_LINES
			- (1-vs_adjust);
	else
		vs_bline_evn = TOTAL_LINES + de_v_begin_even - SOF_LINES
			- VSYNC_LINES - (1-vs_adjust);
	vs_eline_evn = modulo(vs_bline_evn + VSYNC_LINES, TOTAL_LINES);
	hd_write_reg(P_ENCP_DVI_VSO_BLINE_EVN, vs_bline_evn);
	hd_write_reg(P_ENCP_DVI_VSO_ELINE_EVN, vs_eline_evn);
	vso_begin_evn = hs_begin;
	hd_write_reg(P_ENCP_DVI_VSO_BEGIN_EVN, vso_begin_evn);
	hd_write_reg(P_ENCP_DVI_VSO_END_EVN, vso_begin_evn);
	hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
			(0 << 1) |
			(HSYNC_POLARITY << 2) |
			(VSYNC_POLARITY << 3) |
			(0 << 4) |
			(4 << 5) |
			(0 << 8) |
			(0 << 12)
	);
	hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
	hd_write_reg(P_ENCP_VIDEO_EN, 1);
}

static void hdmi_tvenc480i_set(struct hdmitx_vidpara *param)
{
	unsigned long VFIFO2VD_TO_HDMI_LATENCY = 1;
	unsigned long TOTAL_PIXELS = 0, PIXEL_REPEAT_HDMI = 0,
		PIXEL_REPEAT_VENC = 0, ACTIVE_PIXELS = 0;
	unsigned int FRONT_PORCH = 38, HSYNC_PIXELS = 124, ACTIVE_LINES = 0,
		INTERLACE_MODE = 0, TOTAL_LINES = 0, SOF_LINES = 0,
		VSYNC_LINES = 0;
	unsigned int LINES_F0 = 262, LINES_F1 = 263, BACK_PORCH = 114,
		EOF_LINES = 2, TOTAL_FRAMES = 0;

	unsigned long total_pixels_venc = 0;
	unsigned long active_pixels_venc = 0;
	unsigned long front_porch_venc = 0;
	unsigned long hsync_pixels_venc = 0;

	unsigned long de_h_begin = 0, de_h_end = 0;
	unsigned long de_v_begin_even = 0, de_v_end_even = 0,
		de_v_begin_odd = 0, de_v_end_odd = 0;
	unsigned long hs_begin = 0, hs_end = 0;
	unsigned long vs_adjust = 0;
	unsigned long vs_bline_evn = 0, vs_eline_evn = 0,
		vs_bline_odd = 0, vs_eline_odd = 0;
	unsigned long vso_begin_evn = 0, vso_begin_odd = 0;

	hd_set_reg_bits(P_HHI_GCLK_OTHER, 1, 8, 1);
	switch (param->VIC) {
	case HDMI_480i60:
	case HDMI_480i60_16x9:
	case HDMI_480i60_16x9_rpt:
		INTERLACE_MODE = 1;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 1;
		ACTIVE_PIXELS	= (720*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (480/(1+INTERLACE_MODE));
		LINES_F0 = 262;
		LINES_F1 = 263;
		FRONT_PORCH = 38;
		HSYNC_PIXELS = 124;
		BACK_PORCH = 114;
		EOF_LINES = 4;
		VSYNC_LINES = 3;
		SOF_LINES = 15;
		TOTAL_FRAMES = 4;
	break;
	case HDMI_576i50:
	case HDMI_576i50_16x9:
	case HDMI_576i50_16x9_rpt:
		INTERLACE_MODE = 1;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 1;
		ACTIVE_PIXELS	= (720*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (576/(1+INTERLACE_MODE));
		LINES_F0 = 312;
		LINES_F1 = 313;
		FRONT_PORCH = 24;
		HSYNC_PIXELS = 126;
		BACK_PORCH = 138;
		EOF_LINES = 2;
		VSYNC_LINES = 3;
		SOF_LINES = 19;
		TOTAL_FRAMES = 4;
		break;
	default:
		break;
	}

	TOTAL_PIXELS = (FRONT_PORCH+HSYNC_PIXELS+BACK_PORCH+ACTIVE_PIXELS);
	TOTAL_LINES = (LINES_F0+(LINES_F1*INTERLACE_MODE));

	total_pixels_venc = (TOTAL_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC); /* 1716 / 2 * 2 = 1716 */
	active_pixels_venc = (ACTIVE_PIXELS / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	front_porch_venc = (FRONT_PORCH   / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC); /* 38   / 2 * 2 = 38 */
	hsync_pixels_venc = (HSYNC_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC); /* 124  / 2 * 2 = 124 */

	de_h_begin = modulo(hd_read_reg(P_ENCI_VFIFO2VD_PIXEL_START) +
		VFIFO2VD_TO_HDMI_LATENCY, total_pixels_venc);
	de_h_end  = modulo(de_h_begin + active_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCI_DE_H_BEGIN, de_h_begin);	/* 235 */
	hd_write_reg(P_ENCI_DE_H_END, de_h_end);	 /* 1675 */

	de_v_begin_even = hd_read_reg(P_ENCI_VFIFO2VD_LINE_TOP_START);
	de_v_end_even  = de_v_begin_even + ACTIVE_LINES;
	de_v_begin_odd = hd_read_reg(P_ENCI_VFIFO2VD_LINE_BOT_START);
	de_v_end_odd = de_v_begin_odd + ACTIVE_LINES;
	hd_write_reg(P_ENCI_DE_V_BEGIN_EVEN, de_v_begin_even);
	hd_write_reg(P_ENCI_DE_V_END_EVEN,  de_v_end_even);
	hd_write_reg(P_ENCI_DE_V_BEGIN_ODD, de_v_begin_odd);
	hd_write_reg(P_ENCI_DE_V_END_ODD, de_v_end_odd);

	/* Program Hsync timing */
	if (de_h_end + front_porch_venc >= total_pixels_venc) {
		hs_begin = de_h_end + front_porch_venc - total_pixels_venc;
		vs_adjust  = 1;
	} else {
		hs_begin = de_h_end + front_porch_venc;
		vs_adjust  = 0;
	}
	hs_end = modulo(hs_begin + hsync_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCI_DVI_HSO_BEGIN,  hs_begin);  /* 1713 */
	hd_write_reg(P_ENCI_DVI_HSO_END, hs_end);	/* 121 */

	/* Program Vsync timing for even field */
	if (de_v_end_odd-1 + EOF_LINES + vs_adjust >= LINES_F1) {
		vs_bline_evn = de_v_end_odd-1 + EOF_LINES + vs_adjust
			- LINES_F1;
		vs_eline_evn = vs_bline_evn + VSYNC_LINES;
		hd_write_reg(P_ENCI_DVI_VSO_BLINE_EVN, vs_bline_evn);
		/* vso_bline_evn_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_ELINE_EVN, vs_eline_evn);
		/* vso_eline_evn_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_BEGIN_EVN, hs_begin);
		hd_write_reg(P_ENCI_DVI_VSO_END_EVN, hs_begin);
	} else {
		vs_bline_odd = de_v_end_odd-1 + EOF_LINES + vs_adjust;
		hd_write_reg(P_ENCI_DVI_VSO_BLINE_ODD, vs_bline_odd);
		/* vso_bline_odd_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_BEGIN_ODD, hs_begin);
	if (vs_bline_odd + VSYNC_LINES >= LINES_F1) {
		vs_eline_evn = vs_bline_odd + VSYNC_LINES - LINES_F1;
		hd_write_reg(P_ENCI_DVI_VSO_ELINE_EVN, vs_eline_evn);
		/* vso_eline_evn_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_END_EVN, hs_begin);
	} else {
		vs_eline_odd = vs_bline_odd + VSYNC_LINES;
		hd_write_reg(P_ENCI_DVI_VSO_ELINE_ODD, vs_eline_odd);
		/* vso_eline_odd_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_END_ODD, hs_begin);
	}
	}
	/* Program Vsync timing for odd field */
	if (de_v_end_even-1 + EOF_LINES + 1 >= LINES_F0) {
		vs_bline_odd = de_v_end_even-1 + EOF_LINES + 1 - LINES_F0;
		vs_eline_odd = vs_bline_odd + VSYNC_LINES;
		hd_write_reg(P_ENCI_DVI_VSO_BLINE_ODD, vs_bline_odd);
		/* vso_bline_odd_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_ELINE_ODD, vs_eline_odd);
		/* vso_eline_odd_reg_wr_cnt ++; */
		vso_begin_odd  = modulo(hs_begin + (total_pixels_venc>>1),
			total_pixels_venc);
		hd_write_reg(P_ENCI_DVI_VSO_BEGIN_ODD, vso_begin_odd);
		hd_write_reg(P_ENCI_DVI_VSO_END_ODD, vso_begin_odd);
	} else {
		vs_bline_evn = de_v_end_even-1 + EOF_LINES + 1;
		hd_write_reg(P_ENCI_DVI_VSO_BLINE_EVN, vs_bline_evn); /* 261 */
		/* vso_bline_evn_reg_wr_cnt ++; */
		vso_begin_evn  = modulo(hs_begin + (total_pixels_venc>>1),
			total_pixels_venc);
		hd_write_reg(P_ENCI_DVI_VSO_BEGIN_EVN, vso_begin_evn);
	if (vs_bline_evn + VSYNC_LINES >= LINES_F0) {
		vs_eline_odd = vs_bline_evn + VSYNC_LINES - LINES_F0;
		hd_write_reg(P_ENCI_DVI_VSO_ELINE_ODD, vs_eline_odd);
		/* vso_eline_odd_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_END_ODD, vso_begin_evn);
	} else {
		vs_eline_evn = vs_bline_evn + VSYNC_LINES;
		hd_write_reg(P_ENCI_DVI_VSO_ELINE_EVN, vs_eline_evn);
		/* vso_eline_evn_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_END_EVN, vso_begin_evn);
	}
	}

	hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
			(0 << 1) |
			(0 << 2) |
			(0 << 3) |
			(0 << 4) |
			(4 << 5) |
			(1 << 8) |
			(1 << 12)
	);
	if ((param->VIC == HDMI_480i60_16x9_rpt) ||
		(param->VIC == HDMI_576i50_16x9_rpt))
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 3, 12, 4);
	hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 0, 1);
}

static void hdmi_tvenc_set(struct hdmitx_vidpara *param)
{
	unsigned long VFIFO2VD_TO_HDMI_LATENCY = 2;
	unsigned long TOTAL_PIXELS = 0, PIXEL_REPEAT_HDMI = 0,
		PIXEL_REPEAT_VENC = 0, ACTIVE_PIXELS = 0;
	unsigned int FRONT_PORCH = 0, HSYNC_PIXELS = 0, ACTIVE_LINES = 0,
		INTERLACE_MODE = 0U, TOTAL_LINES = 0, SOF_LINES = 0,
		VSYNC_LINES = 0;
	unsigned int LINES_F0 = 0, LINES_F1 = 0, BACK_PORCH = 0,
		EOF_LINES = 0, TOTAL_FRAMES = 0;

	unsigned long total_pixels_venc = 0;
	unsigned long active_pixels_venc = 0;
	unsigned long front_porch_venc = 0;
	unsigned long hsync_pixels_venc = 0;

	unsigned long de_h_begin = 0, de_h_end = 0;
	unsigned long de_v_begin_even = 0, de_v_end_even = 0;
	unsigned long hs_begin = 0, hs_end = 0;
	unsigned long vs_adjust = 0;
	unsigned long vs_bline_evn = 0, vs_eline_evn = 0;
	unsigned long vso_begin_evn = 0;

	switch (param->VIC) {
	case HDMI_3840x1080p120hz:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= 3840;
		ACTIVE_LINES = 1080;
		LINES_F0 = 1125;
		LINES_F1 = 1125;
		FRONT_PORCH = 176;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 4;
		VSYNC_LINES = 5;
		SOF_LINES = 36;
		TOTAL_FRAMES = 0;
		break;
	case HDMI_3840x1080p100hz:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= 3840;
		ACTIVE_LINES = 1080;
		LINES_F0 = 1125;
		LINES_F1 = 1125;
		FRONT_PORCH = 1056;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 4;
		VSYNC_LINES = 5;
		SOF_LINES = 36;
		TOTAL_FRAMES = 0;
		break;
	case HDMI_3840x540p240hz:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= 3840;
		ACTIVE_LINES = 1080;
		LINES_F0 = 562;
		LINES_F1 = 562;
		FRONT_PORCH = 176;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 2;
		VSYNC_LINES = 2;
		SOF_LINES = 18;
		TOTAL_FRAMES = 0;
		break;
	case HDMI_3840x540p200hz:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= 3840;
		ACTIVE_LINES = 1080;
		LINES_F0 = 562;
		LINES_F1 = 562;
		FRONT_PORCH = 1056;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 2;
		VSYNC_LINES = 2;
		SOF_LINES = 18;
		TOTAL_FRAMES = 0;
		break;
	case HDMI_480p60:
	case HDMI_480p60_16x9:
	case HDMI_480p60_16x9_rpt:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= (720*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (480/(1+INTERLACE_MODE));
		LINES_F0 = 525;
		LINES_F1 = 525;
		FRONT_PORCH = 16;
		HSYNC_PIXELS = 62;
		BACK_PORCH = 60;
		EOF_LINES = 9;
		VSYNC_LINES = 6;
		SOF_LINES = 30;
		TOTAL_FRAMES = 4;
		break;
	case HDMI_576p50:
	case HDMI_576p50_16x9:
	case HDMI_576p50_16x9_rpt:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= (720*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (576/(1+INTERLACE_MODE));
		LINES_F0 = 625;
		LINES_F1 = 625;
		FRONT_PORCH = 12;
		HSYNC_PIXELS = 64;
		BACK_PORCH = 68;
		EOF_LINES = 5;
		VSYNC_LINES = 5;
		SOF_LINES = 39;
		TOTAL_FRAMES = 4;
		break;
	case HDMI_720p60:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= (1280*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (720/(1+INTERLACE_MODE));
		LINES_F0 = 750;
		LINES_F1 = 750;
		FRONT_PORCH = 110;
		HSYNC_PIXELS = 40;
		BACK_PORCH = 220;
		EOF_LINES = 5;
		VSYNC_LINES = 5;
		SOF_LINES = 20;
		TOTAL_FRAMES = 4;
		break;
	case HDMI_720p50:
		INTERLACE_MODE = 0U;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= (1280*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (720/(1+INTERLACE_MODE));
		LINES_F0 = 750;
		LINES_F1 = 750;
		FRONT_PORCH = 440;
		HSYNC_PIXELS = 40;
		BACK_PORCH = 220;
		EOF_LINES = 5;
		VSYNC_LINES = 5;
		SOF_LINES = 20;
		TOTAL_FRAMES = 4;
		break;
	case HDMI_1080p50:
	case HDMI_1080p25:
		INTERLACE_MODE	= 0U;
		PIXEL_REPEAT_VENC  = 0;
		PIXEL_REPEAT_HDMI  = 0;
		ACTIVE_PIXELS = (1920*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (1080/(1+INTERLACE_MODE));
		LINES_F0 = 1125;
		LINES_F1 = 1125;
		FRONT_PORCH = 528;
		HSYNC_PIXELS = 44;
		BACK_PORCH = 148;
		EOF_LINES = 4;
		VSYNC_LINES = 5;
		SOF_LINES = 36;
		TOTAL_FRAMES = 4;
		break;
	case HDMI_1080p24:
		INTERLACE_MODE	= 0U;
		PIXEL_REPEAT_VENC  = 0;
		PIXEL_REPEAT_HDMI  = 0;
		ACTIVE_PIXELS = (1920*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (1080/(1+INTERLACE_MODE));
		LINES_F0 = 1125;
		LINES_F1 = 1125;
		FRONT_PORCH = 638;
		HSYNC_PIXELS = 44;
		BACK_PORCH = 148;
		EOF_LINES = 4;
		VSYNC_LINES = 5;
		SOF_LINES = 36;
		TOTAL_FRAMES = 4;
		break;
	case HDMI_1080p60:
	case HDMI_1080p30:
		INTERLACE_MODE	= 0U;
		PIXEL_REPEAT_VENC  = 0;
		PIXEL_REPEAT_HDMI  = 0;
		ACTIVE_PIXELS = (1920*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (1080/(1+INTERLACE_MODE));
		LINES_F0 = 1125;
		LINES_F1 = 1125;
		FRONT_PORCH = 88;
		HSYNC_PIXELS = 44;
		BACK_PORCH = 148;
		EOF_LINES = 4;
		VSYNC_LINES = 5;
		SOF_LINES = 36;
		TOTAL_FRAMES = 4;
		break;
	default:
		break;
	}

	TOTAL_PIXELS = (FRONT_PORCH+HSYNC_PIXELS+BACK_PORCH+ACTIVE_PIXELS);
	TOTAL_LINES = (LINES_F0+(LINES_F1*INTERLACE_MODE));

	total_pixels_venc = (TOTAL_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	active_pixels_venc = (ACTIVE_PIXELS / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	front_porch_venc = (FRONT_PORCH   / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	hsync_pixels_venc = (HSYNC_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);

	hd_write_reg(P_ENCP_VIDEO_MODE, hd_read_reg(P_ENCP_VIDEO_MODE)|(1<<14));
	/* Program DE timing */
	de_h_begin = modulo(hd_read_reg(P_ENCP_VIDEO_HAVON_BEGIN) +
		VFIFO2VD_TO_HDMI_LATENCY,  total_pixels_venc);
	de_h_end  = modulo(de_h_begin + active_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DE_H_BEGIN, de_h_begin);	/* 220 */
	hd_write_reg(P_ENCP_DE_H_END, de_h_end);	 /* 1660 */
	/* Program DE timing for even field */
	de_v_begin_even = hd_read_reg(P_ENCP_VIDEO_VAVON_BLINE);
	de_v_end_even  = de_v_begin_even + ACTIVE_LINES;
	hd_write_reg(P_ENCP_DE_V_BEGIN_EVEN, de_v_begin_even);
	hd_write_reg(P_ENCP_DE_V_END_EVEN,  de_v_end_even);	/* 522 */

	/* Program Hsync timing */
	if (de_h_end + front_porch_venc >= total_pixels_venc) {
		hs_begin = de_h_end + front_porch_venc - total_pixels_venc;
		vs_adjust  = 1;
	} else {
		hs_begin = de_h_end + front_porch_venc;
		vs_adjust  = 0;
	}
	hs_end = modulo(hs_begin + hsync_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DVI_HSO_BEGIN,  hs_begin);
	hd_write_reg(P_ENCP_DVI_HSO_END, hs_end);

	/* Program Vsync timing for even field */
	if (de_v_begin_even >= SOF_LINES + VSYNC_LINES + (1-vs_adjust))
		vs_bline_evn = de_v_begin_even - SOF_LINES - VSYNC_LINES -
			(1-vs_adjust);
	else
		vs_bline_evn = TOTAL_LINES + de_v_begin_even - SOF_LINES -
			VSYNC_LINES - (1-vs_adjust);
	vs_eline_evn = modulo(vs_bline_evn + VSYNC_LINES, TOTAL_LINES);
	hd_write_reg(P_ENCP_DVI_VSO_BLINE_EVN, vs_bline_evn);   /* 5 */
	hd_write_reg(P_ENCP_DVI_VSO_ELINE_EVN, vs_eline_evn);   /* 11 */
	vso_begin_evn = hs_begin; /* 1692 */
	hd_write_reg(P_ENCP_DVI_VSO_BEGIN_EVN, vso_begin_evn);  /* 1692 */
	hd_write_reg(P_ENCP_DVI_VSO_END_EVN, vso_begin_evn);  /* 1692 */

	if ((param->VIC == HDMI_3840x540p240hz) ||
		(param->VIC == HDMI_3840x540p200hz))
		hd_write_reg(P_ENCP_DE_V_END_EVEN, 0x230);
	switch (param->VIC) {
	case HDMI_3840x1080p120hz:
	case HDMI_3840x1080p100hz:
	case HDMI_3840x540p240hz:
	case HDMI_3840x540p200hz:
		hd_write_reg(P_VPU_HDMI_SETTING, 0x8e);
		break;
	case HDMI_480i60:
	case HDMI_480i60_16x9:
	case HDMI_576i50:
	case HDMI_576i50_16x9:
	case HDMI_480i60_16x9_rpt:
	case HDMI_576i50_16x9_rpt:
		hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
				(0 << 1) |
				(0 << 2) |
				(0 << 3) |
				(0 << 4) |
				(4 << 5) |
				(1 << 8) |
				(1 << 12)
		);
		if ((param->VIC == HDMI_480i60_16x9_rpt) ||
			(param->VIC == HDMI_576i50_16x9_rpt))
			hd_set_reg_bits(P_VPU_HDMI_SETTING, 3, 12, 4);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 0, 1);
		break;
	case HDMI_1080i60:
	case HDMI_1080i50:
		hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
				(0 << 1) |
				(HSYNC_POLARITY << 2) |
				(VSYNC_POLARITY << 3) |
				(0 << 4) |
				(0 << 5) |
				(1 << 8) |
				(0 << 12)
		);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
		break;
	case HDMI_4k2k_30:
	case HDMI_4k2k_25:
	case HDMI_4k2k_24:
	case HDMI_4k2k_smpte_24:
	case HDMI_4096x2160p25_256x135:
	case HDMI_4096x2160p30_256x135:
	case HDMI_4096x2160p50_256x135:
	case HDMI_4096x2160p60_256x135:
	case HDMI_4096x2160p50_256x135_Y420:
	case HDMI_4096x2160p60_256x135_Y420:
	case HDMI_3840x2160p50_16x9:
	case HDMI_3840x2160p60_16x9:
	case HDMI_3840x2160p50_16x9_Y420:
	case HDMI_3840x2160p60_16x9_Y420:
		hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
			(0 << 1) |
			(HSYNC_POLARITY << 2) |
			(VSYNC_POLARITY << 3) |
			(0 << 4) |
			(4 << 5) |
			(0 << 8) |
			(0 << 12)
		);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
		hd_write_reg(P_ENCP_VIDEO_EN, 1); /* Enable VENC */
		break;
	case HDMI_480p60_16x9_rpt:
	case HDMI_576p50_16x9_rpt:
	case HDMI_480p60:
	case HDMI_480p60_16x9:
	case HDMI_576p50:
	case HDMI_576p50_16x9:
		hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
				(0 << 1) |
				(0 << 2) |
				(0 << 3) |
				(0 << 4) |
				(4 << 5) |
				(1 << 8) |
				(0 << 12)
		);
		if ((param->VIC == HDMI_480p60_16x9_rpt) ||
			(param->VIC == HDMI_576p50_16x9_rpt))
			hd_set_reg_bits(P_VPU_HDMI_SETTING, 3, 12, 4);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
		break;
	case HDMI_720p60:
	case HDMI_720p50:
		hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
				(0 << 1) |
				(HSYNC_POLARITY << 2) |
				(VSYNC_POLARITY << 3) |
				(0 << 4) |
				(4 << 5) |
				(1 << 8) |
				(0 << 12)
		);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
		break;
	default:
		hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
				(0 << 1) | /* [	1] src_sel_encp */
				(HSYNC_POLARITY << 2) |
				(VSYNC_POLARITY << 3) |
				(0 << 4) |
				(4 << 5) |
				(0 << 8) |
				(0 << 12)
		);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
	}
	if ((param->VIC == HDMI_480p60_16x9_rpt) ||
		(param->VIC == HDMI_576p50_16x9_rpt))
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 3, 12, 4);
	hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
}

static void digital_clk_off(unsigned char flag)
{
	/* TODO */
}

static void digital_clk_on(unsigned char flag)
{
/* clk81_set(); */
	if (flag&4) {
		hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 0, 0, 7);
		hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 0, 9, 3);
		hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 1, 8, 1);
	}
	if (flag&2) {
		/* on hdmi pixel clock */
		hd_write_reg(P_HHI_GCLK_MPEG2,
			hd_read_reg(P_HHI_GCLK_MPEG2) | (1<<4));
		hd_write_reg(P_HHI_GCLK_OTHER,
			hd_read_reg(P_HHI_GCLK_OTHER)|(1<<17));
	}
}

void phy_pll_off(void)
{
	hdmi_phy_suspend();
}

static void hdmi_audio_init(unsigned int spdif_flag)
{
	return;
	/* TODO */
}

/************************************
 *	hdmitx hardware level interface
 *************************************/

static void hdmitx_dump_tvenc_reg(int cur_VIC, int pr_info_flag)
{
}

static void hdmitx_config_tvenc_reg(int vic, unsigned int reg,
	unsigned int val)
{
}

static void hdmitx_set_pll(struct hdmitx_dev *hdev)
{
	hdmi_print(IMP, SYS "set pll\n");
	hdmi_print(IMP, SYS "param->VIC:%d\n", hdev->cur_video_param->VIC);

	set_vmode_clk(hdev);
}

static void set_phy_by_mode(unsigned int mode)
{
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
		switch (mode) {
		case 1: /* 5.94Gbps, 3.7125Gbsp */
			hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x333d3282);
			hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x2136315b);
			break;
		case 2: /* 2.97Gbps */
			hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33303382);
			hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x2036315b);
			break;
		case 3: /* 1.485Gbps */
			hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33303042);
			hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x2016315b);
			break;
		default: /* 742.5Mbps, and below */
			hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33604132);
			hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x0016315b);
			break;
		}
		return;
	}

	/* other than GXL */
	switch (mode) {
	case 1: /* 5.94Gbps, 3.7125Gbsp */
		hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33353245);
		hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x2100115b);
		break;
	case 2: /* 2.97Gbps */
		hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33634283);
		hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0xb000115b);
		break;
	case 3: /* 1.485Gbps, and below */
	default:
		hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33632122);
		hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x2000115b);
		break;
	}
}

static void hdmitx_set_phy(struct hdmitx_dev *hdev)
{
	if (!hdev)
		return;
	hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x0);
#if 1
/* P_HHI_HDMI_PHY_CNTL1	bit[1]: enable clock	bit[0]: soft reset */
#define RESET_HDMI_PHY() \
do { \
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0xf, 0, 4); \
	mdelay(2); \
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0xe, 0, 4); \
	mdelay(2); \
} while (0)

	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0x0390, 16, 16);
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0x1, 17, 1);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)
		hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0x0, 17, 1);
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0x0, 0, 4);
	msleep(100);
	RESET_HDMI_PHY();
	RESET_HDMI_PHY();
	RESET_HDMI_PHY();
#undef RESET_HDMI_PHY
#endif
	switch (hdev->cur_VIC) {
	case HDMI_4k2k_24:
	case HDMI_4k2k_25:
	case HDMI_4k2k_30:
	case HDMI_4k2k_smpte_24:
	case HDMI_4096x2160p25_256x135:
	case HDMI_4096x2160p30_256x135:
		if ((hdev->para->cs == COLORSPACE_YUV422)
			|| (hdev->para->cd == COLORDEPTH_24B))
			set_phy_by_mode(2);
		else
			set_phy_by_mode(1);
		break;
	case HDMI_3840x2160p50_16x9:
	case HDMI_3840x2160p60_16x9:
	case HDMI_4096x2160p50_256x135:
	case HDMI_4096x2160p60_256x135:
		if (hdev->para->cs == COLORSPACE_YUV420)
			set_phy_by_mode(2);
		else
			set_phy_by_mode(1);
		break;
	case HDMI_3840x2160p50_16x9_Y420:
	case HDMI_3840x2160p60_16x9_Y420:
	case HDMI_4096x2160p50_256x135_Y420:
	case HDMI_4096x2160p60_256x135_Y420:
		if (hdev->para->cd == COLORDEPTH_24B)
			set_phy_by_mode(2);
		else
			set_phy_by_mode(1);
		break;
	case HDMI_1080p60:
	case HDMI_1080p50:
		if (hdev->flag_3dfp)
			set_phy_by_mode(2);
		else
			set_phy_by_mode(3);
		break;
	default:
		if (hdev->flag_3dfp)
			set_phy_by_mode(3);
		else
			set_phy_by_mode(4);
		break;
	}
}

static void set_tmds_clk_div40(unsigned int div40)
{
	pr_info("hdmitx div40: %d\n", div40);
	if (div40) {
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_01, 0);
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_23, 0x03ff03ff);
	} else {
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_01, 0x001f001f);
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_23, 0x001f001f);
	}
	hdmitx_set_reg_bits(HDMITX_DWC_FC_SCRAMBLER_CTRL, !!div40, 0, 1);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, 0x1);
	msleep(20);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, 0x2);
}

static void hdmitx_set_scdc(struct hdmitx_dev *hdev)
{
	switch (hdev->cur_video_param->VIC) {
	case HDMI_3840x2160p50_16x9:
	case HDMI_3840x2160p60_16x9:
	case HDMI_4096x2160p50_256x135:
	case HDMI_4096x2160p60_256x135:
		if ((hdev->para->cs == COLORSPACE_YUV420)
			&& (hdev->para->cd == COLORDEPTH_24B))
			hdev->para->tmds_clk_div40 = 0;
		else
			hdev->para->tmds_clk_div40 = 1;
		break;
	case HDMI_3840x2160p50_16x9_Y420:
	case HDMI_3840x2160p60_16x9_Y420:
	case HDMI_4096x2160p50_256x135_Y420:
	case HDMI_4096x2160p60_256x135_Y420:
	case HDMI_3840x2160p50_64x27_Y420:
	case HDMI_3840x2160p60_64x27_Y420:
		if (hdev->para->cd == COLORDEPTH_24B)
			hdev->para->tmds_clk_div40 = 0;
		else
			hdev->para->tmds_clk_div40 = 1;
		break;
	case HDMI_3840x2160p24_16x9:
	case HDMI_3840x2160p24_64x27:
	case HDMI_4096x2160p24_256x135:
	case HDMI_3840x2160p25_16x9:
	case HDMI_3840x2160p25_64x27:
	case HDMI_4096x2160p25_256x135:
	case HDMI_3840x2160p30_16x9:
	case HDMI_3840x2160p30_64x27:
	case HDMI_4096x2160p30_256x135:
		if ((hdev->para->cs == COLORSPACE_YUV422)
			|| (hdev->para->cd == COLORDEPTH_24B))
			hdev->para->tmds_clk_div40 = 0;
		else
			hdev->para->tmds_clk_div40 = 1;
		break;
	default:
		hdev->para->tmds_clk_div40 = 0;
		break;
	}
	set_tmds_clk_div40(hdev->para->tmds_clk_div40);
	if (hdev->RXCap.scdc_present)
		scdc_config(hdev);
}

static int hdmitx_set_dispmode(struct hdmitx_dev *hdev)
{
	struct hdmi_format_para *para = NULL;

	if (hdev->cur_video_param == NULL) /* disable HDMI */
		return 0;
	if (!hdmitx_edid_VIC_support(hdev->cur_video_param->VIC))
		return -1;
	hdev->cur_VIC = hdev->cur_video_param->VIC;

	hdmitx_set_scdc(hdev);

	if (color_depth_f == 24)
		hdev->cur_video_param->color_depth = COLORDEPTH_24B;
	else if (color_depth_f == 30)
		hdev->cur_video_param->color_depth = COLORDEPTH_30B;
	else if (color_depth_f == 36)
		hdev->cur_video_param->color_depth = COLORDEPTH_36B;
	else if (color_depth_f == 48)
		hdev->cur_video_param->color_depth = COLORDEPTH_48B;
	hdmi_print(INF, SYS "set mode VIC %d (cd%d,cs%d,pm%d,vd%d,%x)\n",
		hdev->cur_video_param->VIC, color_depth_f, COLORSPACE_f,
		power_mode, power_off_vdac_flag, serial_reg_val);
	if (COLORSPACE_f != 0)
		hdev->cur_video_param->color = COLORSPACE_f;
	hdmitx_set_pll(hdev);
	/*hdmitx_set_phy(hdev);*/
	if (hdev->flag_3dfp)
		set_vmode_3dfp_enc_hw(hdev->cur_video_param->VIC);
	else
		set_vmode_enc_hw(hdev->cur_video_param->VIC);
	para = hdmi_get_fmt_paras(hdev->cur_video_param->VIC);
	if (para == NULL) {
		pr_info("error at %s[%d] vic = %d\n", __func__, __LINE__,
			hdev->cur_video_param->VIC);
	} else {
		hd_write_reg(P_VPP_POSTBLEND_H_SIZE, para->hdmitx_vinfo.width);
	}

	if (hdev->flag_3dfp) {
		hd_write_reg(P_VPU_HDMI_SETTING, 0x8e);
		goto next;
	}
	switch (hdev->cur_video_param->VIC) {
	case HDMI_480i60:
	case HDMI_480i60_16x9:
	case HDMI_576i50:
	case HDMI_576i50_16x9:
	case HDMI_480i60_16x9_rpt:
	case HDMI_576i50_16x9_rpt:
		hdmi_tvenc480i_set(hdev->cur_video_param);
		break;
	case HDMI_1080i60:
	case HDMI_1080i50:
		hdmi_tvenc1080i_set(hdev->cur_video_param);
		break;
	case HDMI_4k2k_30:
	case HDMI_4k2k_25:
	case HDMI_4k2k_24:
	case HDMI_4k2k_smpte_24:
	case HDMI_4096x2160p25_256x135:
	case HDMI_4096x2160p30_256x135:
	case HDMI_4096x2160p50_256x135:
	case HDMI_4096x2160p60_256x135:
	case HDMI_3840x2160p50_16x9:
	case HDMI_3840x2160p60_16x9:
	case HDMI_3840x2160p50_16x9_Y420:
	case HDMI_3840x2160p60_16x9_Y420:
	case HDMI_4096x2160p50_256x135_Y420:
	case HDMI_4096x2160p60_256x135_Y420:
		hdmi_tvenc4k2k_set(hdev->cur_video_param);
		break;
	default:
		hdmi_tvenc_set(hdev->cur_video_param);
	}
next:
/* [ 3: 2] chroma_dnsmp. 0=use pixel 0; 1=use pixel 1; 2=use average. */
/* [	5] hdmi_dith_md: random noise selector. */
	hd_write_reg(P_VPU_HDMI_FMT_CTRL, (((TX_INPUT_COLOR_FORMAT ==
		COLORSPACE_YUV420) ? 2 : 0)  << 0) | (2 << 2) |
			(0 << 4) | /* [4]dith_en: disable dithering */
			(0  << 5) |
			(0 << 6)); /* [ 9: 6] hdmi_dith10_cntl. */
	if (hdev->para->cs == COLORSPACE_YUV420) {
		hd_set_reg_bits(P_VPU_HDMI_FMT_CTRL, 2, 0, 2);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 0, 4, 4);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 8, 1);
	}
	switch (hdev->para->cd) {
	case COLORDEPTH_30B:
	case COLORDEPTH_36B:
	case COLORDEPTH_48B:
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXM) {
			unsigned int hs_flag = 0;
			/* 12-10 dithering on */
			hd_set_reg_bits(P_VPU_HDMI_FMT_CTRL, 1, 4, 1);
			/* hsync/vsync not invert */
			hs_flag = (hd_read_reg(P_VPU_HDMI_SETTING) >> 2) & 0x3;
			hd_set_reg_bits(P_VPU_HDMI_SETTING, 0, 2, 2);
			/* 12-10 rounding off */
			hd_set_reg_bits(P_VPU_HDMI_FMT_CTRL, 0, 10, 1);
			/* 10-8 dithering off (2x2 old dither) */
			hd_set_reg_bits(P_VPU_HDMI_DITH_CNTL, 0, 4, 1);
			/* set hsync/vsync */
			hd_set_reg_bits(P_VPU_HDMI_DITH_CNTL, hs_flag, 2, 2);
		} else {
			hd_set_reg_bits(P_VPU_HDMI_FMT_CTRL, 0, 4, 1);
			hd_set_reg_bits(P_VPU_HDMI_FMT_CTRL, 0, 10, 1);
		}
		break;
	default:
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXM) {
			/* 12-10 dithering off */
			hd_set_reg_bits(P_VPU_HDMI_FMT_CTRL, 0, 4, 1);
			/* 12-10 rounding on */
			hd_set_reg_bits(P_VPU_HDMI_FMT_CTRL, 1, 10, 1);
			/* 10-8 dithering on (2x2 old dither) */
			hd_set_reg_bits(P_VPU_HDMI_DITH_CNTL, 1, 4, 1);
			/* set hsync/vsync as default 0 */
			hd_set_reg_bits(P_VPU_HDMI_DITH_CNTL, 0, 2, 2);
		} else {
			hd_set_reg_bits(P_VPU_HDMI_FMT_CTRL, 0, 4, 1);
			hd_set_reg_bits(P_VPU_HDMI_FMT_CTRL, 1, 10, 1);
		}
		break;
	}

	hdmitx_set_hw(hdev);

	/* move hdmitx_set_pll() to the end of this function. */
	/* hdmitx_set_pll(param); */
	hdev->cur_VIC = hdev->cur_video_param->VIC;
	/* For 3D, enable phy by SystemControl at last step */
	if ((!hdev->flag_3dfp) && (!hdev->flag_3dtb) && (!hdev->flag_3dss))
		hdmitx_set_phy(hdev);
	switch (hdev->cur_video_param->VIC) {
	case HDMI_480i60:
	case HDMI_480i60_16x9:
	case HDMI_576i50:
	case HDMI_576i50_16x9:
	case HDMI_480i60_16x9_rpt:
	case HDMI_576i50_16x9_rpt:
		enc_vpu_bridge_reset(0);
		break;
	default:
		enc_vpu_bridge_reset(1);
		break;
	}

	if (hdev->para->cs == COLORSPACE_YUV420) {
		/* change AVI packet */
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF0, 0x3, 0, 2);
		mode420_half_horizontal_para();
	} else {
		/* change AVI packet */
		unsigned int indicator = 0;
		unsigned int data32 = 0x0;

		switch (hdev->para->cs) {
		case COLORSPACE_RGB444:
			indicator = 0x0;
			break;
		case COLORSPACE_YUV422:
			indicator = 0x1;
			break;
		case COLORSPACE_YUV444:
		default:
			indicator = 0x2;
			break;
		case COLORSPACE_YUV420:
			indicator = 0x3;
			break;
		}
		data32 = (0x40 | ((indicator&0x4)<<5) | (indicator&0x3));
		hdmitx_wr_reg(HDMITX_DWC_FC_AVICONF0, data32);
	}

	hdmitx_set_reg_bits(HDMITX_DWC_FC_INVIDCONF, 0, 3, 1);
	mdelay(1);
	hdmitx_set_reg_bits(HDMITX_DWC_FC_INVIDCONF, 1, 3, 1);

	return 0;
}

static void hdmitx_set_packet(int type, unsigned char *DB, unsigned char *HB)
{
	int i;
	int pkt_data_len = 0;

	switch (type) {
	case HDMI_PACKET_AVI:
		break;
	case HDMI_PACKET_VEND:
		if ((!DB) || (!HB)) {
			hdmitx_set_reg_bits(HDMITX_DWC_FC_DATAUTO0, 0, 3, 1);
			return;
		}
		hdmitx_wr_reg(HDMITX_DWC_FC_VSDIEEEID0, DB[0]);
		hdmitx_wr_reg(HDMITX_DWC_FC_VSDIEEEID1, DB[1]);
		hdmitx_wr_reg(HDMITX_DWC_FC_VSDIEEEID2, DB[2]);
		hdmitx_wr_reg(HDMITX_DWC_FC_VSDPAYLOAD0, DB[3]);
		hdmitx_wr_reg(HDMITX_DWC_FC_VSDSIZE, HB[2]);
		if (DB[3] == 0x20) { /* set HDMI VIC */
			hdmitx_wr_reg(HDMITX_DWC_FC_AVIVID, 0);
			hdmitx_wr_reg(HDMITX_DWC_FC_VSDPAYLOAD1, DB[4]);
		}
		if (DB[3] == 0x40) { /* 3D VSI */
			hdmitx_wr_reg(HDMITX_DWC_FC_VSDPAYLOAD1, DB[4]);
			hdmitx_wr_reg(HDMITX_DWC_FC_VSDPAYLOAD2, DB[5]);
			if ((DB[4] >> 4) == T3D_FRAME_PACKING)
				hdmitx_wr_reg(HDMITX_DWC_FC_VSDSIZE, 5);
			else
				hdmitx_wr_reg(HDMITX_DWC_FC_VSDSIZE, 6);
		}
		/* Enable VSI packet */
		hdmitx_set_reg_bits(HDMITX_DWC_FC_DATAUTO0, 1, 3, 1);
		hdmitx_wr_reg(HDMITX_DWC_FC_DATAUTO1, 0);
		hdmitx_wr_reg(HDMITX_DWC_FC_DATAUTO2, 0x10);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_PACKET_TX_EN, 1, 4, 1);
		break;
	case HDMI_PACKET_DRM:
		pkt_data_len = 26;
		if ((!DB) || (!HB)) {
			hdmitx_set_reg_bits(HDMITX_DWC_FC_DATAUTO3, 0, 6, 1);
			hdmitx_set_reg_bits(
				HDMITX_DWC_FC_PACKET_TX_EN, 0, 7, 1);
			return;
		}
		/* Ignore HB[0] */
		hdmitx_wr_reg(HDMITX_DWC_FC_DRM_HB01, HB[1]);
		hdmitx_wr_reg(HDMITX_DWC_FC_DRM_HB02, HB[2]);
		for (i = 0; i < pkt_data_len; i++)
			hdmitx_wr_reg(HDMITX_DWC_FC_DRM_PB00 + i, DB[i]);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_DATAUTO3, 1, 6, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_PACKET_TX_EN, 1, 7, 1);
		break;
	case HDMI_AUDIO_INFO:
		pkt_data_len = 9;
		break;
	case HDMI_SOURCE_DESCRIPTION:
		pkt_data_len = 25;
		for (i = 0; i < 25; i++)
			hdmitx_wr_reg(HDMITX_DWC_FC_SPDVENDORNAME0 + i, DB[i]);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_DATAUTO0, 1, 4, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_DATAUTO2, 0x1, 4, 4);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_PACKET_TX_EN, 1, 4, 1);
		break;
	default:
		break;
	}
}


static void hdmitx_setaudioinfoframe(unsigned char *AUD_DB,
	unsigned char *CHAN_STAT_BUF)
{
}


/* set_hdmi_audio_source(unsigned int src) */
/* Description: */
/* Select HDMI audio clock source, and I2S input data source. */
/* Parameters: */
/* src -- 0=no audio clock to HDMI; 1=pcmout to HDMI; 2=Aiu I2S out to HDMI. */
static void set_hdmi_audio_source(unsigned int src)
{
	unsigned long data32;

	/* Disable HDMI audio clock input and its I2S input */
	data32 = 0;
	data32 |= (0 << 4);
	data32 |= (0 << 0);
	hd_write_reg(P_AIU_HDMI_CLK_DATA_CTRL, data32);

	/* Enable HDMI I2S input from the selected source */
	data32 = 0;
	data32 |= (src  << 4);
	data32 |= (src  << 0);
	hd_write_reg(P_AIU_HDMI_CLK_DATA_CTRL, data32);
} /* set_hdmi_audio_source */

/* 60958-3 bit 27-24 */
static unsigned char aud_csb_sampfreq[FS_MAX + 1] = {
	[FS_REFER_TO_STREAM] = 0x1, /* not indicated */
	[FS_32K] = 0x3, /* FS_32K */
	[FS_44K1] = 0x0, /* FS_44K1 */
	[FS_48K] = 0x2, /* FS_48K */
	[FS_88K2] = 0x8, /* FS_88K2 */
	[FS_96K] = 0xa, /* FS_96K */
	[FS_176K4] = 0xc, /* FS_176K4 */
	[FS_192K] = 0xe, /* FS_192K */
	[FS_768K] = 0x9, /* FS_768K */
};

/* 60958-3 bit 39:36 */
static unsigned char aud_csb_ori_sampfreq[FS_MAX + 1] = {
	[FS_REFER_TO_STREAM] = 0x0, /* not indicated */
	[FS_32K] = 0xc, /* FS_32K */
	[FS_44K1] = 0xf, /* FS_44K1 */
	[FS_48K] = 0xd, /* FS_48K */
	[FS_88K2] = 0x7, /* FS_88K2 */
	[FS_96K] = 0xa, /* FS_96K */
	[FS_176K4] = 0x3, /* FS_176K4 */
	[FS_192K] = 0x1, /* FS_192K */
};

static void set_aud_chnls(struct hdmitx_dev *hdev,
	struct hdmitx_audpara *audio_param)
{
	int i;

	pr_info("hdmitx set channel status\n");
	for (i = 0; i < 9; i++)
		/* First, set all status to 0 */
		hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS0+i, 0x00);
	/* set default 48k 2ch pcm */
	if ((audio_param->type == CT_PCM) &&
		(audio_param->channel_num == (2 - 1))) {
		hdmitx_wr_reg(HDMITX_DWC_FC_AUDSV, 0x11);
		hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS7, 0x02);
		hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS8, 0xd2);
	} else {
		hdmitx_wr_reg(HDMITX_DWC_FC_AUDSV, 0xff);
	}
	switch (audio_param->type) {
	case CT_AC_3:
	case CT_DOLBY_D:
	case CT_DST:
		hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS3, 0x01); /* CSB 20 */
		hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS5, 0x02); /* CSB 21 */
		break;
	default:
		hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS3, 0x00);
		hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS5, 0x00);
		break;
	}
	hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDSCHNLS7,
		aud_csb_sampfreq[audio_param->sample_rate], 0, 4); /*CSB 27:24*/
	hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDSCHNLS7, 0x0, 6, 2); /*CSB 31:30*/
	hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDSCHNLS7, 0x0, 4, 2); /*CSB 29:28*/
	hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDSCHNLS8, 0x2, 0, 4); /*CSB 35:32*/
	hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDSCHNLS8,  /* CSB 39:36 */
		aud_csb_ori_sampfreq[audio_param->sample_rate], 4, 4);
}

#define GET_OUTCHN_NO(a)	(((a) >> 4) & 0xf)
#define GET_OUTCHN_MSK(a)	((a) & 0xf)

static void set_aud_info_pkt(struct hdmitx_dev *hdev,
	struct hdmitx_audpara *audio_param)
{
	hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDICONF0, 0, 0, 4); /* CT */
	hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDICONF0, audio_param->channel_num,
		4, 3); /* CC */
	if (GET_OUTCHN_NO(hdev->aud_output_ch))
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDICONF0,
			GET_OUTCHN_NO(hdev->aud_output_ch) - 1, 4, 3);
	hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDICONF1, 0, 0, 3); /* SF */
	hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDICONF1, 0, 4, 2); /* SS */
	switch (audio_param->type) {
	case CT_MAT:
	case CT_DTS_HD_MA:
		/* CC: 8ch */
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDICONF0, 7, 4, 3);
		hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF2, 0x13);
		break;
	case CT_PCM:
		if (!hdev->aud_output_ch)
			hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDICONF0,
				audio_param->channel_num, 4, 3);
		if ((audio_param->channel_num == 0x7) && (!hdev->aud_output_ch))
			hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF2, 0x13);
		else
			hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF2, 0x00);
		/* Refer to CEA861-D P90 */
		switch (GET_OUTCHN_NO(hdev->aud_output_ch)) {
		case 2:
			hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF2, 0x00);
			break;
		case 4:
			hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF2, 0x03);
			break;
		case 6:
			hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF2, 0x0b);
			break;
		case 8:
			hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF2, 0x13);
			break;
		default:
			break;
		}
		break;
	case CT_DTS:
	case CT_DTS_HD:
	default:
		/* CC: 2ch */
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDICONF0, 1, 4, 3);
		hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF2, 0x0);
		break;
	}
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF3, 0);
}

static void set_aud_acr_pkt(struct hdmitx_dev *hdev,
	struct hdmitx_audpara *audio_param)
{
	unsigned int data32;
	unsigned int aud_n_para;
	unsigned int char_rate;

	/* audio packetizer config */
	hdmitx_wr_reg(HDMITX_DWC_AUD_INPUTCLKFS, tx_aud_src ? 4 : 0);

	if ((audio_param->type == CT_MAT)
	|| (audio_param->type == CT_DTS_HD_MA))
		hdmitx_wr_reg(HDMITX_DWC_AUD_INPUTCLKFS, 2);

	if ((hdev->frac_rate_policy) && (hdev->para->timing.frac_freq))
		char_rate = hdev->para->timing.frac_freq;
	else
		char_rate = hdev->para->timing.pixel_freq;
	if (hdev->para->cs == COLORSPACE_YUV422)
		aud_n_para = hdmi_get_aud_n_paras(audio_param->sample_rate,
			COLORDEPTH_24B, char_rate);
	else
		aud_n_para = hdmi_get_aud_n_paras(audio_param->sample_rate,
			hdev->para->cd, char_rate);
	/* N must mutiples 4 for DD+ */
	switch (audio_param->type) {
	case CT_DOLBY_D:
		aud_n_para *= 4;
		break;
	default:
		break;
	}
	pr_info("hdmitx aud_n_para = %d\n", aud_n_para);

	/* ACR packet configuration */
	data32 = 0;
	data32 |= (1 << 7);  /* [  7] ncts_atomic_write */
	data32 |= (0 << 0);  /* [3:0] AudN[19:16] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_N3, data32);

	data32 = 0;
	data32 |= (0 << 7);  /* [7:5] N_shift */
	data32 |= (0 << 4);  /* [  4] CTS_manual */
	data32 |= (0 << 0);  /* [3:0] manual AudCTS[19:16] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_CTS3, data32);

	hdmitx_wr_reg(HDMITX_DWC_AUD_CTS2, 0); /* manual AudCTS[15:8] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_CTS1, 0); /* manual AudCTS[7:0] */

	data32 = 0;
	data32 |= (1 << 7);  /* [  7] ncts_atomic_write */
	data32 |= (((aud_n_para>>16)&0xf) << 0);  /* [3:0] AudN[19:16] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_N3, data32);
	hdmitx_wr_reg(HDMITX_DWC_AUD_N2, (aud_n_para>>8)&0xff); /* AudN[15:8] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_N1, aud_n_para&0xff); /* AudN[7:0] */
}

static void set_aud_fifo_rst(void)
{
	/* reset audio fifo */
	hdmitx_set_reg_bits(HDMITX_DWC_AUD_CONF0, 1, 7, 1);
	hdmitx_set_reg_bits(HDMITX_DWC_AUD_CONF0, 0, 7, 1);
	hdmitx_set_reg_bits(HDMITX_DWC_AUD_SPDIF0, 1, 7, 1);
	hdmitx_set_reg_bits(HDMITX_DWC_AUD_SPDIF0, 0, 7, 1);
	hdmitx_wr_reg(HDMITX_DWC_MC_SWRSTZREQ, 0xe7);
	/* need reset again */
	hdmitx_set_reg_bits(HDMITX_DWC_AUD_SPDIF0, 1, 7, 1);
	hdmitx_set_reg_bits(HDMITX_DWC_AUD_SPDIF0, 0, 7, 1);
}

static void set_aud_samp_pkt(struct hdmitx_dev *hdev,
	struct hdmitx_audpara *audio_param)
{
	switch (audio_param->type) {
	case CT_MAT: /* HBR */
	case CT_DTS_HD_MA:
		hdmitx_set_reg_bits(HDMITX_DWC_AUD_SPDIF1, 1, 7, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_AUD_SPDIF1, 1, 6, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_AUD_SPDIF1, 24, 0, 5);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDSCONF, 1, 0, 1);
		break;
	case CT_PCM: /* AudSamp */
		hdmitx_set_reg_bits(HDMITX_DWC_AUD_SPDIF1, 0, 7, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_AUD_SPDIF1, 0, 6, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_AUD_SPDIF1, 24, 0, 5);
		if ((audio_param->channel_num == 0x7) && (!hdev->aud_output_ch))
			hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDSCONF, 1, 0, 1);
		else
			hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDSCONF, 0, 0, 1);
		switch (GET_OUTCHN_NO(hdev->aud_output_ch)) {
		case 2:
			hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDSCONF, 0, 0, 1);
			break;
		case 4:
		case 6:
		case 8:
			hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDSCONF, 1, 0, 1);
			break;
		default:
			break;
		}
		break;
	case CT_AC_3:
	case CT_DOLBY_D:
	case CT_DTS:
	case CT_DTS_HD:
	default:
		hdmitx_set_reg_bits(HDMITX_DWC_AUD_SPDIF1, 0, 7, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_AUD_SPDIF1, 0, 6, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_AUD_SPDIF1, 24, 0, 5);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AUDSCONF, 0, 0, 1);
		break;
	}
}

static void audio_mute_op(bool flag)
{
	if (flag == 0) {
		hdmitx_set_reg_bits(HDMITX_TOP_CLK_CNTL, 0, 2, 2);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_PACKET_TX_EN, 0, 0, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_PACKET_TX_EN, 0, 3, 1);
	} else {
		hdmitx_set_reg_bits(HDMITX_TOP_CLK_CNTL, 3, 2, 2);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_PACKET_TX_EN, 1, 0, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_PACKET_TX_EN, 1, 3, 1);
	}
}

static int hdmitx_set_audmode(struct hdmitx_dev *hdev,
	struct hdmitx_audpara *audio_param)
{
	unsigned int data32;

	if (!hdev)
		return 0;
	if (!audio_param)
		return 0;
	pr_info("hdmtix: set audio\n");
	audio_mute_op(hdev->tx_aud_cfg);
	/* PCM & 8 ch */
	if ((audio_param->type == CT_PCM) &&
		(audio_param->channel_num == (8 - 1)))
		tx_aud_src = 1;
	else
		tx_aud_src = 0;

	/* if hdev->aud_output_ch is true, select I2S as 8ch in, 2ch out */
	if (hdev->aud_output_ch)
		tx_aud_src = 1;

	pr_info("hdmitx tx_aud_src = %d\n", tx_aud_src);

	/* set_hdmi_audio_source(tx_aud_src ? 1 : 2); */
	set_hdmi_audio_source(2);

/* config IP */
/* Configure audio */
	/* I2S Sampler config */
	data32 = 0;
/* [  3] fifo_empty_mask: 0=enable int; 1=mask int. */
	data32 |= (1 << 3);
/* [  2] fifo_full_mask: 0=enable int; 1=mask int. */
	data32 |= (1 << 2);
	hdmitx_wr_reg(HDMITX_DWC_AUD_INT, data32);

	data32 = 0;
/* [  4] fifo_overrun_mask: 0=enable int; 1=mask int.
 * Enable it later when audio starts.
 */
	data32 |= (1 << 4);
	hdmitx_wr_reg(HDMITX_DWC_AUD_INT1,  data32);
/* [  5] 0=select SPDIF; 1=select I2S. */
	data32 = 0;
	data32 |= (0 << 7);  /* [  7] sw_audio_fifo_rst */
	data32 |= (tx_aud_src << 5);
	data32 |= (0 << 0);  /* [3:0] i2s_in_en: enable it later in test.c */
/* if enable it now, fifo_overrun will happen, because packet don't get sent
 * out until initial DE detected.
 */
	hdmitx_wr_reg(HDMITX_DWC_AUD_CONF0, data32);

	data32 = 0;
	data32 |= (0 << 5);  /* [7:5] i2s_mode: 0=standard I2S mode */
	data32 |= (24 << 0);  /* [4:0] i2s_width */
	hdmitx_wr_reg(HDMITX_DWC_AUD_CONF1, data32);

	data32 = 0;
	data32 |= (0 << 1);  /* [  1] NLPCM */
	data32 |= (0 << 0);  /* [  0] HBR */
	hdmitx_wr_reg(HDMITX_DWC_AUD_CONF2, data32);

	/* spdif sampler config */
/* [  2] SPDIF fifo_full_mask: 0=enable int; 1=mask int. */
/* [  3] SPDIF fifo_empty_mask: 0=enable int; 1=mask int. */
	data32 = 0;
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIFINT,  data32);
	/* [  4] SPDIF fifo_overrun_mask: 0=enable int; 1=mask int. */
	data32 = 0;
	data32 |= (0 << 4);
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIFINT1, data32);

	data32 = 0;
	data32 |= (0 << 7);  /* [  7] sw_audio_fifo_rst */
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIF0, data32);

	set_aud_info_pkt(hdev, audio_param);
	set_aud_acr_pkt(hdev, audio_param);
	set_aud_samp_pkt(hdev, audio_param);

	set_aud_chnls(hdev, audio_param);

	hdmitx_set_reg_bits(HDMITX_DWC_AUD_CONF0, tx_aud_src, 5, 1);
	if (tx_aud_src == 1) {
		if (GET_OUTCHN_MSK(hdev->aud_output_ch))
			hdmitx_set_reg_bits(HDMITX_DWC_AUD_CONF0,
				GET_OUTCHN_MSK(hdev->aud_output_ch), 0, 4);
		else
			hdmitx_set_reg_bits(HDMITX_DWC_AUD_CONF0, 0xf, 0, 4);
		/* Enable audi2s_fifo_overrun interrupt */
		hdmitx_wr_reg(HDMITX_DWC_AUD_INT1,
			hdmitx_rd_reg(HDMITX_DWC_AUD_INT1) & (~(1<<4)));
		/* Wait for 40 us for TX I2S decoder to settle */
		msleep(20);
	}
	set_aud_fifo_rst();
	udelay(10);
	hdmitx_wr_reg(HDMITX_DWC_AUD_N1, hdmitx_rd_reg(HDMITX_DWC_AUD_N1));
	hdmitx_set_reg_bits(HDMITX_DWC_FC_DATAUTO3, 1, 0, 1);

	return 1;
}

static void hdmitx_setupirq(struct hdmitx_dev *phdev)
{
	int r;

	hdmitx_wr_reg(HDMITX_TOP_INTR_STAT_CLR, 0x7);
	r = request_irq(phdev->irq_hpd, &intr_handler,
			IRQF_SHARED, "hdmitx",
			(void *)phdev);
}

static void hdmitx_uninit(struct hdmitx_dev *phdev)
{
	free_irq(phdev->irq_hpd, (void *)phdev);
	hdmi_print(1, "power off hdmi, unmux hpd\n");

	phy_pll_off();
	digital_clk_off(7); /* off sys clk */
	hdmitx_hpd_hw_op(HPD_UNMUX_HPD);
}

static void hw_reset_dbg(void)
{
	uint32_t val1 = hdmitx_rd_reg(HDMITX_DWC_MC_CLKDIS);
	uint32_t val2 = hdmitx_rd_reg(HDMITX_DWC_FC_INVIDCONF);
	uint32_t val3 = hdmitx_rd_reg(HDMITX_DWC_FC_VSYNCINWIDTH);

	hdmitx_wr_reg(HDMITX_DWC_MC_CLKDIS, 0xff);
	udelay(10);
	hdmitx_wr_reg(HDMITX_DWC_MC_CLKDIS, val1);
	udelay(10);
	hdmitx_wr_reg(HDMITX_DWC_MC_SWRSTZREQ, 0);
	udelay(10);
	hdmitx_wr_reg(HDMITX_DWC_FC_INVIDCONF, 0);
	udelay(10);
	hdmitx_wr_reg(HDMITX_DWC_FC_INVIDCONF, val2);
	udelay(10);
	hdmitx_wr_reg(HDMITX_DWC_FC_VSYNCINWIDTH, val3);
}

static int hdmitx_cntl(struct hdmitx_dev *hdev, unsigned int cmd,
	unsigned int argv)
{
	if (cmd == HDMITX_AVMUTE_CNTL) {
		return 0;
	} else if (cmd == HDMITX_EARLY_SUSPEND_RESUME_CNTL) {
		if (argv == HDMITX_EARLY_SUSPEND) {
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0, 30, 1);
			hdmi_phy_suspend();
		}
		if (argv == HDMITX_LATE_RESUME) {
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 1, 30, 1);
			hw_reset_dbg();
			pr_info("hdmitx: swrstzreq\n");
		}
		return 0;
	} else if (cmd == HDMITX_HWCMD_MUX_HPD_IF_PIN_HIGH) {
		/* turnon digital module if gpio is high */
		if (hdmitx_hpd_hw_op(HPD_IS_HPD_MUXED) == 0) {
			if (hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO)) {
				msleep(500);
				if (hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO)) {
					hdmi_print(IMP, HPD "mux hpd\n");
					digital_clk_on(4);
					delay_us(1000*100);
					hdmitx_hpd_hw_op(HPD_MUX_HPD);
				}
			}
		}
	} else if (cmd == HDMITX_HWCMD_MUX_HPD)
		hdmitx_hpd_hw_op(HPD_MUX_HPD);
/* For test only. */
	else if (cmd == HDMITX_HWCMD_TURNOFF_HDMIHW) {
		int unmux_hpd_flag = argv;

		if (unmux_hpd_flag) {
			hdmi_print(IMP, SYS "power off hdmi, unmux hpd\n");
			phy_pll_off();
			digital_clk_off(4); /* off sys clk */
			hdmitx_hpd_hw_op(HPD_UNMUX_HPD);
		} else {
			hdmi_print(IMP, SYS "power off hdmi\n");
			digital_clk_on(6);
			phy_pll_off();
			digital_clk_off(3); /* do not off sys clk */
		}
	}
	return 0;
}

static void hdmitx_print_info(struct hdmitx_dev *hdev, int pr_info_flag)
{
	hdmi_print(INF, "------------------\nHdmitx driver version: ");
	hdmi_print(INF, "%s\nSerial %x\nColor Depth %d\n", HDMITX_VER,
		serial_reg_val, color_depth_f);
	hdmi_print(INF, "reset sequence %d\n", new_reset_sequence_flag);
	hdmi_print(INF, "power mode %d\n", power_mode);
	hdmi_print(INF, "%spowerdown when unplug\n",
		hdev->unplug_powerdown?"":"do not ");
	hdmi_print(INF, "use_tvenc_conf_flag=%d\n", use_tvenc_conf_flag);
	hdmi_print(INF, "vdac %s\n", power_off_vdac_flag?"off":"on");
	hdmi_print(INF, "hdmi audio %s\n", hdmi_audio_off_flag?"off":"on");
	if (!hdmi_audio_off_flag)
		hdmi_print(INF, "audio out type %s\n",
			i2s_to_spdif_flag?"spdif":"i2s");
	hdmi_print(INF, "delay flag %d\n", delay_flag);
	hdmi_print(INF, "------------------\n");
}

struct aud_cts_log {
	unsigned int val:20;
};

static inline unsigned int get_msr_cts(void)
{
	unsigned int ret = 0;

	ret = hdmitx_rd_reg(HDMITX_DWC_AUD_CTS1);
	ret += (hdmitx_rd_reg(HDMITX_DWC_AUD_CTS2) << 8);
	ret += ((hdmitx_rd_reg(HDMITX_DWC_AUD_CTS3) & 0xf) << 16);

	return ret;
}

#define AUD_CTS_LOG_NUM	1000
struct aud_cts_log cts_buf[AUD_CTS_LOG_NUM];
static void cts_test(struct hdmitx_dev *hdev)
{
	int i;
	unsigned int min = 0, max = 0, total = 0;

	pr_info("\nhdmitx: audio: cts test\n");
	memset(cts_buf, 0, sizeof(cts_buf));
	for (i = 0; i < AUD_CTS_LOG_NUM; i++) {
		cts_buf[i].val = get_msr_cts();
		mdelay(1);
	}

	pr_info("\ncts change:\n");
	for (i = 1; i < AUD_CTS_LOG_NUM; i++) {
		if (cts_buf[i].val > cts_buf[i-1].val)
			pr_info("dis: +%d  [%d] %d  [%d] %d\n",
				cts_buf[i].val - cts_buf[i-1].val, i,
				cts_buf[i].val, i - 1, cts_buf[i - 1].val);
		if (cts_buf[i].val < cts_buf[i-1].val)
			pr_info("dis: %d  [%d] %d  [%d] %d\n",
				cts_buf[i].val - cts_buf[i-1].val, i,
				cts_buf[i].val, i - 1, cts_buf[i - 1].val);
		}

	for (i = 0, min = max = cts_buf[0].val; i < AUD_CTS_LOG_NUM; i++) {
		total += cts_buf[i].val;
		if (min > cts_buf[i].val)
			min = cts_buf[i].val;
		if (max < cts_buf[i].val)
			max = cts_buf[i].val;
	}
	pr_info("\nCTS Min: %d   Max: %d   Avg: %d/1000\n\n", min, max, total);
}

void hdmitx_dump_inter_timing(void)
{
	unsigned int tmp = 0;
#define CONNECT2REG(reg) ((hdmitx_rd_reg(reg)) + (hdmitx_rd_reg(reg + 1) << 8))
	tmp = CONNECT2REG(HDMITX_DWC_FC_INHACTV0);
	pr_info("Hactive = %d\n", tmp);

	tmp = CONNECT2REG(HDMITX_DWC_FC_INHBLANK0);
	pr_info("Hblank = %d\n", tmp);

	tmp = CONNECT2REG(HDMITX_DWC_FC_INVACTV0);
	pr_info("Vactive = %d\n", tmp);

	tmp = hdmitx_rd_reg(HDMITX_DWC_FC_INVBLANK);
	pr_info("Vblank = %d\n", tmp);

	tmp = CONNECT2REG(HDMITX_DWC_FC_HSYNCINDELAY0);
	pr_info("Hfront = %d\n", tmp);

	tmp = CONNECT2REG(HDMITX_DWC_FC_HSYNCINWIDTH0);
	pr_info("Hsync = %d\n", tmp);

	tmp = hdmitx_rd_reg(HDMITX_DWC_FC_VSYNCINDELAY);
	pr_info("Vfront = %d\n", tmp);

	tmp = hdmitx_rd_reg(HDMITX_DWC_FC_VSYNCINWIDTH);
	pr_info("Vsync = %d\n", tmp);

	/* HDMITX_DWC_FC_INFREQ0 ??? */
}

#define DUMP_CVREG_SECTION(start, end) \
do { \
	if (start > end) { \
		pr_info("Error start = 0x%x > end = 0x%x\n", \
			((start & 0xffff) >> 2), ((end & 0xffff) >> 2)); \
		break; \
	} \
	pr_info("Start = 0x%x[0x%x]   End = 0x%x[0x%x]\n", \
		start, ((start & 0xffff) >> 2), end, ((end & 0xffff) >> 2)); \
	for (addr = start; addr < end + 1; addr += 4) {	\
		val = hd_read_reg(addr); \
		if (val) \
			pr_info("0x%08x[0x%04x]: 0x%08x\n", addr, \
				((addr & 0xffff) >> 2), val); \
		} \
} while (0)

static void hdmitx_dump_all_cvregs(void)
{
#if 0
	unsigned int addr = 0, val = 0;

	DUMP_CVREG_SECTION(P_STB_TOP_CONFIG, P_CIPLUS_ENDIAN);
	DUMP_CVREG_SECTION(P_PREG_CTLREG0_ADDR, P_AHB_BRIDGE_CNTL_REG2);
	DUMP_CVREG_SECTION(P_BT_CTRL, P_BT656_ADDR_END);
	DUMP_CVREG_SECTION(P_VERSION_CTRL, P_RESET7_LEVEL);
	DUMP_CVREG_SECTION(P_SCR_HIU, P_HHI_HDMIRX_AUD_PLL_CNTL6);
	DUMP_CVREG_SECTION(P_PARSER_CONTROL, P_PARSER_AV2_WRAP_COUNT);
	DUMP_CVREG_SECTION(P_DVIN_FRONT_END_CTRL, P_DVIN_CTRL_STAT);
	DUMP_CVREG_SECTION(P_AIU_958_BPF, P_AIU_I2S_CBUS_DDR_ADDR);
	DUMP_CVREG_SECTION(P_GE2D_GEN_CTRL0, P_GE2D_GEN_CTRL4);
	DUMP_CVREG_SECTION(P_AUDIO_COP_CTL2, P_EE_ASSIST_MBOX3_FIQ_SEL);
	DUMP_CVREG_SECTION(P_AUDIN_SPDIF_MODE, P_AUDIN_ADDR_END);
	DUMP_CVREG_SECTION(P_VDIN_SCALE_COEF_IDX, P_VDIN0_SCALE_COEF_IDX);
	DUMP_CVREG_SECTION(P_VDIN0_SCALE_COEF, P_VDIN1_ASFIFO_CTRL3);
	DUMP_CVREG_SECTION(P_L_GAMMA_CNTL_PORT, P_MLVDS_RESET_CONFIG_LO);
	DUMP_CVREG_SECTION(P_VPP2_DUMMY_DATA, P_DI_CHAN2_URGENT_CTRL);
	DUMP_CVREG_SECTION(P_DI_PRE_CTRL, P_DI_CANVAS_URGENT2);
	DUMP_CVREG_SECTION(P_ENCP_VFIFO2VD_CTL, P_VIU2_VD1_FMT_W);
	DUMP_CVREG_SECTION(P_VPU_OSD1_MMC_CTRL, P_VPU_PROT3_REQ_ONOFF);
	DUMP_CVREG_SECTION(P_D2D3_GLB_CTRL, P_D2D3_RESEV_STATUS2);
	DUMP_CVREG_SECTION(P_VI_HIST_CTRL, P_DEMO_CRTL);
	DUMP_CVREG_SECTION(P_AO_RTI_STATUS_REG0, P_AO_SAR_ADC_REG12);
	DUMP_CVREG_SECTION(P_STB_VERSION, P_DEMUX_SECTION_RESET_3);
#endif
}

#define DUMP_HDMITXREG_SECTION(start, end) \
do { \
	if (start > end) { \
		pr_info("Error start = 0x%lx > end = 0x%lx\n", start, end); \
		break; \
	} \
	pr_info("Start = 0x%lx   End = 0x%lx\n", start, end); \
	for (addr = start; addr < end + 1; addr++) { \
		val = hdmitx_rd_reg(addr); \
		if (val) \
			pr_info("[0x%08x]: 0x%08x\n", addr, val); \
	} \
} while (0)

static void hdmitx_dump_intr(void)
{
	unsigned int addr = 0, val = 0;

	DUMP_HDMITXREG_SECTION(HDMITX_DWC_IH_FC_STAT0, HDMITX_DWC_IH_MUTE);
}

static void mode420_half_horizontal_para(void)
{
	unsigned int hactive = 0;
	unsigned int hblank = 0;
	unsigned int hfront = 0;
	unsigned int hsync = 0;

	hactive =  hdmitx_rd_reg(HDMITX_DWC_FC_INHACTV0);
	hactive += (hdmitx_rd_reg(HDMITX_DWC_FC_INHACTV1) & 0x3f) << 8;
	hblank =  hdmitx_rd_reg(HDMITX_DWC_FC_INHBLANK0);
	hblank += (hdmitx_rd_reg(HDMITX_DWC_FC_INHBLANK1) & 0x1f) << 8;
	hfront =  hdmitx_rd_reg(HDMITX_DWC_FC_HSYNCINDELAY0);
	hfront += (hdmitx_rd_reg(HDMITX_DWC_FC_HSYNCINDELAY1) & 0x1f) << 8;
	hsync =  hdmitx_rd_reg(HDMITX_DWC_FC_HSYNCINWIDTH0);
	hsync += (hdmitx_rd_reg(HDMITX_DWC_FC_HSYNCINWIDTH1) & 0x3) << 8;

	hactive = hactive / 2;
	hblank = hblank / 2;
	hfront = hfront / 2;
	hsync = hsync / 2;

	hdmitx_wr_reg(HDMITX_DWC_FC_INHACTV0, (hactive & 0xff));
	hdmitx_wr_reg(HDMITX_DWC_FC_INHACTV1, ((hactive >> 8) & 0x3f));
	hdmitx_wr_reg(HDMITX_DWC_FC_INHBLANK0, (hblank  & 0xff));
	hdmitx_wr_reg(HDMITX_DWC_FC_INHBLANK1, ((hblank >> 8) & 0x1f));
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINDELAY0, (hfront & 0xff));
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINDELAY1, ((hfront >> 8) & 0x1f));
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINWIDTH0, (hsync & 0xff));
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINWIDTH1, ((hsync >> 8) & 0x3));
}

static void hdmitx_set_fake_vic(struct hdmitx_dev *hdev)
{
	hdev->para->cs = COLORSPACE_YUV444;
	hdev->cur_VIC = HDMI_VIC_FAKE;
	set_vmode_clk(hdev);
}

static void hdmitx_debug(struct hdmitx_dev *hdev, const char *buf)
{
	char tmpbuf[128];
	int i = 0;
	int ret;
	unsigned long adr = 0;
	unsigned long value = 0;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;

	if ((strncmp(tmpbuf, "dumpreg", 7) == 0) ||
		(strncmp(tmpbuf, "dumptvencreg", 12) == 0)) {
		hdmitx_dump_tvenc_reg(hdev->cur_VIC, 1);
		return;
	} else if (strncmp(tmpbuf, "testhpll", 8) == 0) {
		ret = kstrtoul(tmpbuf + 8, 10, &value);
		hdev->cur_VIC = value;
		set_vmode_clk(hdev);
		return;
	} else if (strncmp(tmpbuf, "testpll", 7) == 0)
		return;
	else if (strncmp(tmpbuf, "testedid", 8) == 0) {
		dd();
		hdev->HWOp.CntlDDC(hdev, DDC_RESET_EDID, 0);
		hdev->HWOp.CntlDDC(hdev, DDC_EDID_READ_DATA, 0);
		return;
	} else if (strncmp(tmpbuf, "bist", 4) == 0) {
		if (strncmp(tmpbuf + 4, "off", 3) == 0) {
			hd_set_reg_bits(P_ENCP_VIDEO_MODE_ADV, 1, 3, 1);
			hd_write_reg(P_VENC_VIDEO_TST_EN, 0);
			return;
		}
		hd_set_reg_bits(P_HHI_GCLK_OTHER, 1, 3, 1);
		hd_set_reg_bits(P_ENCP_VIDEO_MODE_ADV, 0, 3, 1);
		hd_write_reg(P_VENC_VIDEO_TST_EN, 1);
		if (strncmp(tmpbuf+4, "line", 4) == 0) {
			hd_write_reg(P_VENC_VIDEO_TST_MDSEL, 2);
			return;
		}
		if (strncmp(tmpbuf+4, "dot", 3) == 0) {
			hd_write_reg(P_VENC_VIDEO_TST_MDSEL, 3);
			return;
		}
		if (strncmp(tmpbuf+4, "start", 5) == 0) {
			ret = kstrtoul(tmpbuf + 9, 10, &value);
			hd_write_reg(P_VENC_VIDEO_TST_CLRBAR_STRT, value);
			return;
		}
		if (strncmp(tmpbuf+4, "shift", 5) == 0) {
			ret = kstrtoul(tmpbuf + 9, 10, &value);
			hd_write_reg(P_VENC_VIDEO_TST_VDCNT_STSET, value);
			return;
		}
		hd_write_reg(P_VENC_VIDEO_TST_MDSEL, 1);
		value = 1920;
		ret = kstrtoul(tmpbuf + 4, 10, &value);
		hd_write_reg(P_VENC_VIDEO_TST_CLRBAR_WIDTH, value / 8);
		return;
	} else if (strncmp(tmpbuf, "dumptiming", 10) == 0) {
		hdmitx_dump_inter_timing();
		return;
	} else if (strncmp(tmpbuf, "testaudio", 9) == 0) {
		hdmitx_set_audmode(hdev, NULL);
	} else if (strncmp(tmpbuf, "dumpintr", 8) == 0) {
		hdmitx_dump_intr();
	} else if (strncmp(tmpbuf, "testhdcp", 8) == 0) {
		int i;

		i = tmpbuf[8] - '0';
		if (i == 2)
			pr_info("hdcp rslt = %d", hdmitx_hdcp_opr(2));
		if (i == 1)
			hdev->HWOp.CntlDDC(hdev, DDC_HDCP_OP, HDCP14_ON);
		return;
	} else if (strncmp(tmpbuf, "dumpallregs", 11) == 0) {
		hdmitx_dump_all_cvregs();
		return;
	} else if (strncmp(tmpbuf, "chkfmt", 6) == 0) {
		check_detail_fmt();
		return;
	} else if (strncmp(tmpbuf, "testcts", 7) == 0) {
		cts_test(hdev);
		return;
	} else if (strncmp(tmpbuf, "ss", 2) == 0) {
		pr_info("hdev->output_blank_flag: 0x%x\n",
			hdev->output_blank_flag);
		pr_info("hdev->hpd_state: 0x%x\n", hdev->hpd_state);
		pr_info("hdev->cur_VIC: 0x%x\n", hdev->cur_VIC);
	} else if (strncmp(tmpbuf, "hpd_lock", 8) == 0) {
		if (tmpbuf[8] == '1') {
			hdev->hpd_lock = 1;
			hdmi_print(INF, HPD "hdmitx: lock hpd\n");
		} else {
			hdev->hpd_lock = 0;
			hdmi_print(INF, HPD "hdmitx: unlock hpd\n");
		}
		return;
	} else if (strncmp(tmpbuf, "hpd_stick", 9) == 0) {
		if (tmpbuf[9] == '1')
			hdev->hdcp_hpd_stick = 1;
		else
			hdev->hdcp_hpd_stick = 0;
		pr_info("hdmitx: %sstick hpd\n",
			(hdev->hdcp_hpd_stick) ? "" : "un");
	} else if (strncmp(tmpbuf, "vic", 3) == 0) {
		pr_info("hdmi vic count = %d\n", hdev->vic_count);
		if ((tmpbuf[3] >= '0') && (tmpbuf[3] <= '9')) {
			hdev->vic_count = tmpbuf[3] - '0';
			hdmi_print(INF, SYS "set hdmi vic count = %d\n",
				hdev->vic_count);
		}
	} else if (strncmp(tmpbuf, "cec", 3) == 0)
		return;
	else if (strncmp(tmpbuf, "dumphdmireg", 11) == 0) {
		unsigned char reg_val = 0;
		unsigned int reg_adr = 0;

#define DUMP_HDMITX_SECTION(a, b) \
	for (reg_adr = a; reg_adr < b+1; reg_adr++) { \
		reg_val = hdmitx_rd_reg(reg_adr); \
		if (reg_val) \
			pr_info("[0x%x]: 0x%x\n", reg_adr, reg_val); \
		}

#define DUMP_HDMITX_HDCP_SECTION(a, b) \
	for (reg_adr = a; reg_adr < b+1; reg_adr++) { \
		hdmitx_wr_reg(HDMITX_DWC_A_KSVMEMCTRL, 0x1); \
		hdmitx_poll_reg(HDMITX_DWC_A_KSVMEMCTRL, (1<<1), 2 * HZ); \
		reg_val = hdmitx_rd_reg(reg_adr); \
		if (reg_val) \
			pr_info("[0x%x]: 0x%x\n", reg_adr, reg_val); \
		}

		DUMP_HDMITX_SECTION(HDMITX_TOP_SW_RESET,
			HDMITX_TOP_DONT_TOUCH1);
		DUMP_HDMITX_SECTION(HDMITX_TOP_SKP_CNTL_STAT,
			HDMITX_TOP_SEC_SCRATCH);
		DUMP_HDMITX_SECTION(HDMITX_DWC_DESIGN_ID,
			HDMITX_DWC_A_KSVMEMCTRL);
		DUMP_HDMITX_HDCP_SECTION(HDMITX_DWC_HDCP_BSTATUS_0,
			HDMITX_DWC_HDCPREG_BKSV0 - 1);
		DUMP_HDMITX_SECTION(HDMITX_DWC_HDCPREG_ANCONF,
			HDMITX_DWC_HDCP22REG_MUTE);
		DUMP_HDMITX_SECTION(HDMITX_DWC_A_HDCPCFG0,
			HDMITX_DWC_A_HDCPCFG1);
		DUMP_HDMITX_SECTION(HDMITX_DWC_HDCPREG_SEED0,
			HDMITX_DWC_HDCPREG_DPK6);
		DUMP_HDMITX_SECTION(HDMITX_DWC_HDCP22REG_CTRL,
			HDMITX_DWC_HDCP22REG_CTRL);
		return;
	} else if (strncmp(tmpbuf, "dumpcecreg", 10) == 0) {
		unsigned char cec_val = 0;
		unsigned int cec_adr = 0;
		/* HDMI CEC Regs address range:0xc000~0xc01c;0xc080~0xc094 */
		for (cec_adr = 0xc000; cec_adr < 0xc01d; cec_adr++) {
			cec_val = hdmitx_rd_reg(cec_adr);
			hdmi_print(INF, "HDMI CEC Regs[0x%x]: 0x%x\n",
				cec_adr, cec_val);
		}
		for (cec_adr = 0xc080; cec_adr < 0xc095; cec_adr++) {
			cec_val = hdmitx_rd_reg(cec_adr);
			hdmi_print(INF, "HDMI CEC Regs[0x%x]: 0x%x\n",
				cec_adr, cec_val);
		}
		return;
	} else if (strncmp(tmpbuf, "dumpcbusreg", 11) == 0) {
		unsigned int i, val;

		for (i = 0; i < 0x3000; i++) {
			val = hd_read_reg(CBUS_REG_ADDR(i));
			if (val)
				pr_info("CBUS[0x%x]: 0x%x\n", i, val);
		}
		return;
	} else if (strncmp(tmpbuf, "dumpvcbusreg", 12) == 0) {
		unsigned int i, val;

		for (i = 0; i < 0x3000; i++) {
			val = hd_read_reg(VCBUS_REG_ADDR(i));
			if (val)
				pr_info("VCBUS[0x%x]: 0x%x\n", i, val);
		}
		return;
	} else if (strncmp(tmpbuf, "log", 3) == 0) {
		if (strncmp(tmpbuf+3, "hdcp", 4) == 0) {
			static unsigned int i = 1;

			if (i & 1)
				hdev->log |= HDMI_LOG_HDCP;
			else
				hdev->log &= ~HDMI_LOG_HDCP;
			i++;
		}
		return;
	} else if (strncmp(tmpbuf, "pllcalc", 7) == 0) {
		/* TODO	clk_measure(0xff); */
		return;
	} else if (strncmp(tmpbuf, "hdmiaudio", 9) == 0) {
		ret = kstrtoul(tmpbuf+9, 16, &value);
		if (value == 1) {
			hdmi_audio_off_flag = 0;
			hdmi_audio_init(i2s_to_spdif_flag);
		} else if (value == 0)
			;
		return;
	} else if (strncmp(tmpbuf, "cfgreg", 6) == 0) {
		ret = kstrtoul(tmpbuf+6, 16, &adr);
		ret = kstrtoul(buf+i+1, 16, &value);
		hdmitx_config_tvenc_reg(hdev->cur_VIC, adr, value);
		return;
	} else if (strncmp(tmpbuf, "tvenc_flag", 10) == 0) {
		use_tvenc_conf_flag = tmpbuf[10]-'0';
		hdmi_print(INF, "set use_tvenc_conf_flag = %d\n",
			use_tvenc_conf_flag);
	} else if (strncmp(tmpbuf, "reset", 5) == 0) {
		if (tmpbuf[5] == '0')
			new_reset_sequence_flag = 0;
		else
			new_reset_sequence_flag = 1;
		return;
	} else if (strncmp(tmpbuf, "delay_flag", 10) == 0)
		delay_flag = tmpbuf[10]-'0';
	else if (tmpbuf[0] == 'v') {
		hdmitx_print_info(hdev, 1);
		return;
	} else if (tmpbuf[0] == 's') {
		ret = kstrtoul(tmpbuf+1, 16, &serial_reg_val);
		return;
	} else if (tmpbuf[0] == 'c') {
		if (tmpbuf[1] == 'd') {
			ret = kstrtoul(tmpbuf+2, 10, &color_depth_f);
			if ((color_depth_f != 24) && (color_depth_f != 30) &&
				(color_depth_f != 36)) {
				pr_info("Color depth %lu is not supported\n",
					color_depth_f);
			color_depth_f = 0;
			}
		return;
	} else if (tmpbuf[1] == 's') {
		ret = kstrtoul(tmpbuf+2, 10, &COLORSPACE_f);
		if (COLORSPACE_f > 2) {
			pr_info("Color space %lu is not supported\n",
				COLORSPACE_f);
			COLORSPACE_f = 0;
		}
	}
	} else if (strncmp(tmpbuf, "i2s", 2) == 0) {
		if (strncmp(tmpbuf+3, "off", 3) == 0)
			i2s_to_spdif_flag = 1;
		else
			i2s_to_spdif_flag = 0;
	} else if (strncmp(tmpbuf, "pattern_on", 10) == 0) {
		/* turn_on_shift_pattern(); */
		hdmi_print(INF, "Shift Pattern On\n");
		return;
	} else if (strncmp(tmpbuf, "pattern_off", 11) == 0) {
		hdmi_print(INF, "Shift Pattern Off\n");
		return;
	} else if (strncmp(tmpbuf, "prbs", 4) == 0)
		/* int prbs_mode =kstrtoul(tmpbuf+4, NULL, 10); */
		return;
	else if (tmpbuf[0] == 'w') {
		unsigned int read_back = 0;

		ret = kstrtoul(tmpbuf+2, 16, &adr);
		ret = kstrtoul(buf+i+1, 16, &value);
		if (buf[1] == 'h') {
			hdmitx_wr_reg((unsigned int)adr, (unsigned int)value);
			read_back = hdmitx_rd_reg(adr);
		}
		hdmi_print(INF, "write %x to %s reg[%x]\n", value, "HDMI", adr);
/* Add read back function in order to judge writing is OK or NG. */
		hdmi_print(INF, "Read Back %s reg[%x]=%x\n", "HDMI",
			adr, read_back);
	} else if (tmpbuf[0] == 'r') {
		ret = kstrtoul(tmpbuf+2, 16, &adr);
		if (buf[1] == 'h')
			value = hdmitx_rd_reg(adr);
		hdmi_print(INF, "%s reg[%x]=%x\n", "HDMI", adr, value);
	}
}


static void hdmitx_getediddata(unsigned char *des, unsigned char *src)
{
	int i = 0;
	unsigned int blk = src[126] + 1;

	if (blk > 4)
		blk = 4;

	for (i = 0; i < 128 * blk; i++)
		des[i] = src[i];
}

/*
 * Note: read 8 Bytes of EDID data every time
 */
static void hdmitx_read_edid(unsigned char *rx_edid)
{
	unsigned int timeout = 0;
	unsigned int i;
	unsigned int byte_num = 0;
	unsigned char   blk_no = 1;

	/* Program SLAVE/ADDR */
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, 0x50);
	/* Read complete EDID data sequentially */
	while (byte_num < 128 * blk_no) {
		hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS,  byte_num&0xff);
		if ((byte_num >= 256) && (byte_num < 512) && (blk_no > 2)) {
			/* Program SEGMENT/SEGPTR */
			hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGADDR, 0x30);
			hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGPTR, 0x1);
			hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1<<3);
		} else
			hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1<<2);
		/* Wait until I2C done */
		timeout = 0;
		while ((!(hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 1)))
			&& (timeout < 3)) {
			mdelay(2);
			timeout++;
		}
		if (timeout == 3)
			pr_info("hdmitx: ddc timeout\n");
		hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 1 << 1);
		/* Read back 8 bytes */
		for (i = 0; i < 8; i++) {
			rx_edid[byte_num] =
				hdmitx_rd_reg(HDMITX_DWC_I2CM_READ_BUFF0 + i);
			if (byte_num == 126) {
				blk_no = rx_edid[126] + 1;
				if (blk_no > 4) {
					pr_info("edid extension block number:");
					pr_info(" %d, reset to MAX 3\n",
						blk_no - 1);
					blk_no = 4; /* Max extended block */
				}
			}
			byte_num++;
		}
	}
} /* hdmi20_tx_read_edid */

static unsigned char tmp_edid_buf[128*EDID_MAX_BLOCK] = { 0 };

#define HDCP_NMOOFDEVICES 127

static int get_hdcp_depth(void)
{
	int ret;

	hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 1, 0, 1);
	hdmitx_poll_reg(HDMITX_DWC_A_KSVMEMCTRL, (1<<1), 2 * HZ);
	ret = hdmitx_rd_reg(HDMITX_DWC_HDCP_BSTATUS_1) & 0x7;
	hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 0, 0, 1);

	return ret;
}

static bool get_hdcp_max_cascade(void)
{
	bool ret;

	hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 1, 0, 1);
	hdmitx_poll_reg(HDMITX_DWC_A_KSVMEMCTRL, (1<<1), 2 * HZ);
	ret = (hdmitx_rd_reg(HDMITX_DWC_HDCP_BSTATUS_1) >> 3) & 0x1;
	hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 0, 0, 1);

	return ret;
}

static bool get_hdcp_max_devs(void)
{
	bool ret;

	hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 1, 0, 1);
	hdmitx_poll_reg(HDMITX_DWC_A_KSVMEMCTRL, (1<<1), 2 * HZ);
	ret = (hdmitx_rd_reg(HDMITX_DWC_HDCP_BSTATUS_0) >> 7) & 0x1;
	hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 0, 0, 1);

	return ret;
}

static int get_hdcp_device_count(void)
{
	int ret;

	hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 1, 0, 1);
	hdmitx_poll_reg(HDMITX_DWC_A_KSVMEMCTRL, (1<<1), 2 * HZ);
	ret = hdmitx_rd_reg(HDMITX_DWC_HDCP_BSTATUS_0) & 0x7f;
	hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 0, 0, 1);

	return ret;
}

static void get_hdcp_bstatus(void)
{
	int ret1 = 0;
	int ret2 = 0;

	hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 1, 0, 1);
	hdmitx_poll_reg(HDMITX_DWC_A_KSVMEMCTRL, (1<<1), 2 * HZ);
	ret1 = hdmitx_rd_reg(HDMITX_DWC_HDCP_BSTATUS_0);
	ret2 = hdmitx_rd_reg(HDMITX_DWC_HDCP_BSTATUS_1);
	hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 0, 0, 1);
	pr_info("BSTATUS0 = 0x%x   BSTATUS1 = 0x%x\n", ret1, ret2);
}

static void hdcp_ksv_store(unsigned char *dat, int no)
{
	int i;

	for (i = 0; i < no; i++) {
		rptx_ksv_buf[rptx_ksv_no] = dat[i];
		rptx_ksv_no++;
	}
}

static void hdcp_ksv_print(void)
{
	unsigned int i, pos;
	unsigned char *tmp_buf = NULL;

	tmp_buf = kmalloc(2000, GFP_KERNEL);
	if (!tmp_buf)
		return;

	pos = 0;
	memset(tmp_buf, 0, sizeof(2000));
	pos += sprintf(tmp_buf + pos, "Dump ksv test START\n");
	for (i = 0; (i < rptx_ksv_no) && (i < 635); i++) {
		pos += sprintf(tmp_buf + pos, "%02x", rptx_ksv_buf[i]);
		if (((i+1) % 40) == 0)    /* print 40bytes a line */
			pos += sprintf(tmp_buf + pos, "\n");
	}
	pos += sprintf(tmp_buf + pos, "\n");
	pos += sprintf(tmp_buf + pos, "Dump ksv test END\n");
	pr_info("%s\n", tmp_buf);
	kfree(tmp_buf);
}

static uint8_t *hdcp_mKsvListBuf;
static void hdcp_ksv_sha1_calc(struct hdmitx_dev *hdev)
{
	size_t list = 0;
	size_t size = 0;
	size_t i = 0;
	int valid = HDCP_IDLE;
	unsigned char ksvs[635] = {0}; /* Max 127 * 5 */
	int j = 0;

	/* 0x165e: Page 95 */
	hdcp_mKsvListBuf = kmalloc(0x1660, GFP_KERNEL);
	if (hdcp_mKsvListBuf) {
		/* KSV_LEN; */
		list = hdmitx_rd_reg(HDMITX_DWC_HDCP_BSTATUS_0) & KSV_MSK;
		if (list <= HDCP_NMOOFDEVICES) {
			size = (list * KSV_LEN) + HEADER + SHAMAX;
			for (i = 0; i < size; i++) {
				if (i < HEADER) { /* BSTATUS & M0 */
					hdcp_mKsvListBuf[(list * KSV_LEN) + i]
						= hdmitx_rd_reg(
						HDMITX_DWC_HDCP_BSTATUS_0 + i);
				} else if (i < (HEADER + (list * KSV_LEN))) {
					/* KSV list */
					hdcp_mKsvListBuf[i - HEADER] =
						hdmitx_rd_reg(
						HDMITX_DWC_HDCP_BSTATUS_0 + i);
					ksvs[j] = hdcp_mKsvListBuf[i - HEADER];
					j++;
				} else { /* SHA */
					hdcp_mKsvListBuf[i] = hdmitx_rd_reg(
						HDMITX_DWC_HDCP_BSTATUS_0 + i);
				}
			}
			valid = hdcpVerify_KSV(hdcp_mKsvListBuf, size)
				== TRUE	? HDCP_KSV_LIST_READY :
				HDCP_ERR_KSV_LIST_NOT_VALID;
		}
		hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 0, 0, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL,
			(valid == HDCP_KSV_LIST_READY) ? 0 : 1, 3, 1);
		if (valid == HDCP_KSV_LIST_READY)
			hdcp_ksv_store(ksvs, j);
		hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 1, 2, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 0, 2, 1);
		kfree(hdcp_mKsvListBuf);
	} else
		pr_info("hdcptx14: KSV List memory not valid\n");
}

static void hdcptx_events_handle(unsigned long arg)
{
	struct hdmitx_dev *hdev = (struct hdmitx_dev *)arg;
	unsigned char ksv[5] = {0};
	int pos, i;
	unsigned int bcaps_6_rp;
	static unsigned int st_flag = -1;

	bcaps_6_rp = !!(hdmitx_rd_reg(HDMITX_DWC_A_HDCPOBS3) & (1 << 6));
	if (st_flag != hdmitx_rd_reg(HDMITX_DWC_A_APIINTSTAT)) {
		st_flag = hdmitx_rd_reg(HDMITX_DWC_A_APIINTSTAT);
		pr_info("hdcp14: instat: 0x%x\n", st_flag);
	}
	if (st_flag & (1 << 7)) {
		hdmitx_wr_reg(HDMITX_DWC_A_APIINTCLR, 1 << 7);
		hdmitx_hdcp_opr(3);
		for (i = 0; i < 5; i++)
			ksv[i] = (unsigned char)
				hdmitx_rd_reg(HDMITX_DWC_HDCPREG_BKSV0 + i);
		hdcp_ksv_store(ksv, 5);
		get_hdcp_bstatus();
		if (hdev->repeater_tx) {
			rx_set_receive_hdcp(rptx_ksv_buf, (rptx_ksv_no + 1) / 5,
				(bcaps_6_rp ? get_hdcp_depth() : 0) + 1,
				bcaps_6_rp ? get_hdcp_max_cascade() : 0,
				bcaps_6_rp ? get_hdcp_max_devs() : 0);
			pr_info("%s[%d]  ksvs Num = %d  device_count = %d\n",
				__func__, __LINE__,
				(rptx_ksv_no + 1) / 5,
				bcaps_6_rp ? get_hdcp_device_count() : 0);
			memset(rptx_ksv_prbuf, 0, sizeof(rptx_ksv_prbuf));
				for (pos = 0, i = 0; i < rptx_ksv_no; i++)
					pos += sprintf(rptx_ksv_prbuf + pos,
						"%02x", rptx_ksv_buf[i]);
				rptx_ksv_prbuf[pos + 1] = '\0';
			if (1)
				hdcp_ksv_print();
		}
	}
	if (st_flag & (1 << 1)) {
		hdmitx_wr_reg(HDMITX_DWC_A_APIINTCLR, (1 << 1));
		hdmitx_wr_reg(HDMITX_DWC_A_KSVMEMCTRL, 0x1);
		hdmitx_poll_reg(HDMITX_DWC_A_KSVMEMCTRL, (1<<1), 2 * HZ);
		if (hdmitx_rd_reg(HDMITX_DWC_A_KSVMEMCTRL) & (1 << 1))
			hdcp_ksv_sha1_calc(hdev);
		else {
			pr_info("hdcptx14: KSV List memory access denied\n");
			return;
		}
		hdmitx_wr_reg(HDMITX_DWC_A_KSVMEMCTRL, 0x4);
		if (hdev->repeater_tx) {
			rptx_ksvlist_retry++;
			if (rptx_ksvlist_retry % 4 == 0) {
				for (i = 0; i < 5; i++)
					ksv[i] = (unsigned char) hdmitx_rd_reg(
						HDMITX_DWC_HDCPREG_BKSV0 + i);
				hdcp_ksv_store(ksv, 5);
				rx_set_receive_hdcp(&ksv[0], 1, 127, 1, 1);
			}
		}
	}
	if (hdev->repeater_tx && bcaps_6_rp && (get_hdcp_max_devs() ||
		get_hdcp_max_cascade())) {
		for (i = 0; i < 5; i++)
			ksv[i] = (unsigned char)
				hdmitx_rd_reg(HDMITX_DWC_HDCPREG_BKSV0 + i);
		hdcp_ksv_store(ksv, 5);
		rx_set_receive_hdcp(&ksv[0], 1, 127, 1, 1);
	}
	if (hdev->hdcp_try_times)
		mod_timer(&hdev->hdcp_timer, jiffies + HZ / 100);
	else
		return;
	hdev->hdcp_try_times--;
}

static void hdcp_start_timer(struct hdmitx_dev *hdev)
{
	static int init_flag;

	if (!init_flag) {
		init_flag = 1;
		init_timer(&hdev->hdcp_timer);
		hdev->hdcp_timer.data = (ulong)hdev;
		hdev->hdcp_timer.function = hdcptx_events_handle;
		hdev->hdcp_timer.expires = jiffies + HZ / 100;
		add_timer(&hdev->hdcp_timer);
		hdev->hdcp_try_times = 500;
		return;
	}
	hdev->hdcp_try_times = 500;
	hdev->hdcp_timer.expires = jiffies + HZ / 100;
	mod_timer(&hdev->hdcp_timer, jiffies + HZ / 100);
}

static void set_pkf_duk_nonce(void)
{
	static int nonce_mode = 1; /* 1: use HW nonce   0: use SW nonce */

	/* Configure duk/pkf */
	hdmitx_hdcp_opr(0xc);
	if (nonce_mode == 1)
		hdmitx_wr_reg(HDMITX_TOP_SKP_CNTL_STAT, 0xf);
	else {
		hdmitx_wr_reg(HDMITX_TOP_SKP_CNTL_STAT, 0xe);
/* Configure nonce[127:0].
 * MSB must be written the last to assert nonce_vld signal.
 */
		hdmitx_wr_reg(HDMITX_TOP_NONCE_0,  0x32107654);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_1,  0xba98fedc);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_2,  0xcdef89ab);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_3,  0x45670123);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_0,  0x76543210);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_1,  0xfedcba98);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_2,  0x89abcdef);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_3,  0x01234567);
	}
	udelay(10);
}

static int hdmitx_cntl_ddc(struct hdmitx_dev *hdev, unsigned int cmd,
	unsigned long argv)
{
	int i = 0;
	unsigned char *tmp_char = NULL;

	if (!(cmd & CMD_DDC_OFFSET))
		hdmi_print(ERR, "ddc: w: invalid cmd 0x%x\n", cmd);
	else
		hdmi_print(LOW, "ddc: cmd 0x%x\n", cmd);

	switch (cmd) {
	case DDC_RESET_EDID:
		hdmitx_wr_reg(HDMITX_DWC_I2CM_SOFTRSTZ, 0);
		memset(tmp_edid_buf, 0, ARRAY_SIZE(tmp_edid_buf));
		break;
	case DDC_IS_EDID_DATA_READY:

		break;
	case DDC_EDID_READ_DATA:
		hdmitx_read_edid(tmp_edid_buf);
		break;
	case DDC_EDID_GET_DATA:
		if (argv == 0)
			hdmitx_getediddata(&hdev->EDID_buf[0], tmp_edid_buf);
		else
			hdmitx_getediddata(&hdev->EDID_buf1[0], tmp_edid_buf);
		break;
	case DDC_PIN_MUX_OP:
		if (argv == PIN_MUX)
			hdmitx_ddc_hw_op(DDC_MUX_DDC);
		if (argv == PIN_UNMUX)
			hdmitx_ddc_hw_op(DDC_UNMUX_DDC);
		break;
	case DDC_EDID_CLEAR_RAM:
		for (i = 0; i < EDID_RAM_ADDR_SIZE; i++)
			hdmitx_wr_reg(HDMITX_DWC_I2CM_READ_BUFF0 + i, 0);
		break;
	case DDC_RESET_HDCP:

		break;
	case DDC_HDCP_MUX_INIT:
		if (argv == 2) {
			hdmitx_ddc_hw_op(DDC_MUX_DDC);
			hdmitx_set_reg_bits(HDMITX_DWC_MC_CLKDIS, 1, 6, 1);
			udelay(5);
			hdmitx_wr_reg(HDMITX_DWC_HDCP22REG_CTRL, 0x6);
			hdmitx_set_reg_bits(HDMITX_TOP_SW_RESET, 1, 5, 1);
			udelay(10);
			hdmitx_set_reg_bits(HDMITX_TOP_SW_RESET, 0, 5, 1);
			udelay(10);
			hdmitx_wr_reg(HDMITX_DWC_HDCP22REG_MASK, 0);
			hdmitx_wr_reg(HDMITX_DWC_HDCP22REG_MUTE, 0);
			set_pkf_duk_nonce();
		}
		if (argv == 1)
			hdmitx_hdcp_opr(6);
		break;
	case DDC_HDCP_OP:
		if (argv == HDCP14_ON) {
			rptx_ksvlist_retry = 0;
			rptx_ksv_no = 0;
			memset(rptx_ksv_buf, 0, sizeof(rptx_ksv_buf));
			hdmitx_ddc_hw_op(DDC_MUX_DDC);
			hdmitx_hdcp_opr(6);
			hdmitx_hdcp_opr(1);
			hdcp_start_timer(hdev);
		}
		if (argv == HDCP14_OFF) {
			rptx_ksvlist_retry = 0;
			hdmitx_hdcp_opr(4);
		}
		if (argv == HDCP22_ON) {
			hdmitx_ddc_hw_op(DDC_MUX_DDC);
			hdmitx_hdcp_opr(5);
			/* wait for start hdcp22app */
		}
		if (argv == HDCP22_OFF)
			hdmitx_hdcp_opr(6);
		break;
	case DDC_IS_HDCP_ON:
/* argv = !!((hdmitx_rd_reg(TX_HDCP_MODE)) & (1 << 7)); */
		break;
	case DDC_HDCP_GET_BKSV:
		tmp_char = (unsigned char *) argv;
		for (i = 0; i < 5; i++)
			tmp_char[i] = (unsigned char)
				hdmitx_rd_reg(HDMITX_DWC_HDCPREG_BKSV0 + 4 - i);
		break;
	case DDC_HDCP_GET_AUTH:
		if (hdev->hdcp_mode == 1)
			return hdmitx_hdcp_opr(2);
		if (hdev->hdcp_mode == 2)
			return hdmitx_hdcp_opr(7);
		else
			return 0;
		break;
	case DDC_HDCP_14_LSTORE:
		return hdmitx_hdcp_opr(0xa);
	case DDC_HDCP_22_LSTORE:
		return hdmitx_hdcp_opr(0xb);
	case DDC_SCDC_DIV40_SCRAMB:
		if (argv == 1) {
			scdc_wr_sink(TMDS_CFG, 0x3); /* TMDS 1/40 & Scramble */
			scdc_wr_sink(TMDS_CFG, 0x3); /* TMDS 1/40 & Scramble */
			hdmitx_wr_reg(HDMITX_DWC_FC_SCRAMBLER_CTRL, 1);
		} else {
			scdc_wr_sink(TMDS_CFG, 0x0); /* TMDS 1/40 & Scramble */
			scdc_wr_sink(TMDS_CFG, 0x0); /* TMDS 1/40 & Scramble */
			hdmitx_wr_reg(HDMITX_DWC_FC_SCRAMBLER_CTRL, 0);
		}
		break;
	case DDC_HDCP14_GET_BCAPS_RP:
		return !!(hdmitx_rd_reg(HDMITX_DWC_A_HDCPOBS3) & (1 << 6));
	default:
		hdmi_print(INF, "ddc: unknown cmd: 0x%x\n", cmd);
		break;
	}
	return 1;
}

static int hdmitx_hdmi_dvi_config(struct hdmitx_dev *hdev,
						unsigned int dvi_mode)
{
	if (dvi_mode == 1) {
		hdmitx_csc_config(TX_INPUT_COLOR_FORMAT,
			COLORSPACE_RGB444, TX_COLOR_DEPTH);

		/* set dvi flag */
		hdmitx_set_reg_bits(HDMITX_DWC_FC_INVIDCONF, 0, 3, 1);

	} else {
		/* set hdmi flag */
		hdmitx_set_reg_bits(HDMITX_DWC_FC_INVIDCONF, 1, 3, 1);
	}

	return 0;
}

static int hdmitx_get_hdmi_dvi_config(struct hdmitx_dev *hdev)
{
	int value = hdmitx_rd_reg(HDMITX_DWC_FC_INVIDCONF) & 0x8;

	return (value == 0)?DVI_MODE:HDMI_MODE;
}

static int hdmitx_cntl_config(struct hdmitx_dev *hdev, unsigned int cmd,
	unsigned int argv)
{
	int ret = 0;

	if (!(cmd & CMD_CONF_OFFSET))
		hdmi_print(ERR, "config: hdmitx: w: invalid cmd 0x%x\n", cmd);
	else
		hdmi_print(LOW, "config: hdmitx: conf cmd 0x%x\n", cmd);

	switch (cmd) {
	case CONF_HDMI_DVI_MODE:
		hdmitx_hdmi_dvi_config(hdev, (argv == DVI_MODE)?1:0);
		break;
	case CONF_GET_HDMI_DVI_MODE:
		ret = hdmitx_get_hdmi_dvi_config(hdev);
		break;
	case CONF_SYSTEM_ST:
		break;
	case CONF_AUDIO_MUTE_OP:
		audio_mute_op(argv == AUDIO_MUTE ? 0 : 1);
		break;
	case CONF_VIDEO_MUTE_OP:
		if (argv == VIDEO_MUTE) {
			hd_set_reg_bits(P_HHI_GCLK_OTHER, 1, 3, 1);
			hd_set_reg_bits(P_ENCP_VIDEO_MODE_ADV, 0, 3, 1);
			hd_write_reg(P_VENC_VIDEO_TST_EN, 1);
			hd_write_reg(P_VENC_VIDEO_TST_MDSEL, 0);
			/*_Y/CB/CR, 10bits Unsigned/Signed/Signed */
			hd_write_reg(P_VENC_VIDEO_TST_Y, 0x0);
			hd_write_reg(P_VENC_VIDEO_TST_CB, 0x200);
			hd_write_reg(P_VENC_VIDEO_TST_CR, 0x200);
		}
		if (argv == VIDEO_UNMUTE) {
			hd_set_reg_bits(P_ENCP_VIDEO_MODE_ADV, 1, 3, 1);
			hd_write_reg(P_VENC_VIDEO_TST_EN, 0);
		}
		break;
	case CONF_CLR_AVI_PACKET:
		hdmitx_wr_reg(HDMITX_DWC_FC_AVIVID, 0);
		if (hdmitx_rd_reg(HDMITX_DWC_FC_VSDPAYLOAD0) == 0x20)
			hdmitx_wr_reg(HDMITX_DWC_FC_VSDPAYLOAD1, 0);
		hd_write_reg(P_ISA_DEBUG_REG0, 0);
		break;
	case CONF_CLR_VSDB_PACKET:
		if (hdmitx_rd_reg(HDMITX_DWC_FC_VSDPAYLOAD0) == 0x20)
			hdmitx_wr_reg(HDMITX_DWC_FC_VSDPAYLOAD1, 0);
		break;
	case CONF_VIDEO_MAPPING:
		config_video_mapping(hdev->para->cs, hdev->para->cd);
		break;
	case CONF_CLR_AUDINFO_PACKET:
		break;
	case CONF_AVI_BT2020:
		if (argv == SET_AVI_BT2020) {
			hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF1, 3, 6, 2);
			hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF2, 6, 4, 3);
		}
		if (argv == CLR_AVI_BT2020)
			hdmitx_set_avi_colorimetry(hdev->para);
		break;
	case CONF_AVI_RGBYCC_INDIC:
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF0, argv, 0, 2);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF0, 0, 7, 1);
		break;
	case CONF_AVI_Q01:
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF2, argv, 2, 2);
		break;
	case CONF_AVI_YQ01:
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF3, argv, 2, 2);
		break;
	default:
		hdmi_print(ERR, "config: hdmitx: unknown cmd: 0x%x\n", cmd);
	}

	return ret;
}

static int hdmitx_tmds_rxsense(void)
{
	unsigned int curr0, curr3;
	int ret = 0;

	switch (get_cpu_type()) {
	case MESON_CPU_MAJOR_ID_GXBB:
		curr0 = hd_read_reg(P_HHI_HDMI_PHY_CNTL0);
		curr3 = hd_read_reg(P_HHI_HDMI_PHY_CNTL3);
		if (curr0 == 0)
			hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33632122);
		hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL3, 0x9a, 16, 8);
		ret = hd_read_reg(P_HHI_HDMI_PHY_CNTL2) & 0x1;
		hd_write_reg(P_HHI_HDMI_PHY_CNTL3, curr3);
		if (curr0 == 0)
			hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0);
		break;
	case MESON_CPU_MAJOR_ID_GXL:
	case MESON_CPU_MAJOR_ID_GXM:
	default:
		curr0 = hd_read_reg(P_HHI_HDMI_PHY_CNTL0);
		curr3 = hd_read_reg(P_HHI_HDMI_PHY_CNTL3);
		if (curr0 == 0)
			hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33604142);
		hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL3, 0x1, 4, 1);
		ret = hd_read_reg(P_HHI_HDMI_PHY_CNTL2) & 0x1;
		hd_write_reg(P_HHI_HDMI_PHY_CNTL3, curr3);
		if (curr0 == 0)
			hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0);
		break;
	}

	return ret;
}

static int hdmitx_cntl_misc(struct hdmitx_dev *hdev, unsigned int cmd,
	unsigned int argv)
{
	if (!(cmd & CMD_MISC_OFFSET))
		hdmi_print(ERR, "misc: hdmitx: w: invalid cmd 0x%x\n", cmd);
	else
		hdmi_print(LOW, "misc: hdmitx: misc cmd 0x%x\n", cmd);

	switch (cmd) {
	case MISC_HPD_MUX_OP:
		if (argv == PIN_MUX)
			argv = HPD_MUX_HPD;
		else
			argv = HPD_UNMUX_HPD;
		return hdmitx_hpd_hw_op(argv);
	case MISC_HPD_GPI_ST:
		return hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO);
	case MISC_HPLL_OP:
		if (argv == HPLL_ENABLE)
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 1, 30, 1);
		if (argv == HPLL_DISABLE)
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0, 30, 1);
		break;
	case MISC_HPLL_FAKE:
		hdmitx_set_fake_vic(hdev);
		break;
	case MISC_TMDS_PHY_OP:
		if (argv == TMDS_PHY_ENABLE)
			hdmi_phy_wakeup(hdev);  /* TODO */
		if (argv == TMDS_PHY_DISABLE)
			hdmi_phy_suspend();
		break;
	case MISC_TMDS_RXSENSE:
		return hdmitx_tmds_rxsense();
	case MISC_ESM_RESET:
		if (hdev->hdcp_hpd_stick == 1) {
			pr_info("hdcp: stick mode\n");
			break;
		}
		hdmitx_hdcp_opr(6);
		break;
	case MISC_VIID_IS_USING:
		break;
	case MISC_TMDS_CLK_DIV40:
		set_tmds_clk_div40(argv);
		break;
	case MISC_AVMUTE_OP:
		config_avmute(argv);
		break;
	case MISC_HDCP_CLKDIS:
		hdmitx_set_reg_bits(HDMITX_DWC_MC_CLKDIS, !!argv, 6, 1);
		break;
	case MISC_I2C_REACTIVE:
		hdmitx_set_reg_bits(HDMITX_DWC_A_HDCPCFG1, 0, 0, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_HDCP22REG_CTRL, 0, 2, 1);
		hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_HCNT_1, 0xff);
		hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_HCNT_0, 0xf6);
		edid_read_head_8bytes();
		hdmi_hwi_init(hdev);
		break;
	default:
		hdmi_print(ERR, "misc: hdmitx: unknown cmd: 0x%x\n", cmd);
	}
	return 1;
}

static enum hdmi_vic get_vic_from_pkt(void)
{
	enum hdmi_vic vic = HDMI_Unknown;
	unsigned int rgb_ycc = hdmitx_rd_reg(HDMITX_DWC_FC_AVICONF0);

	vic = hdmitx_rd_reg(HDMITX_DWC_FC_AVIVID);
	if (vic == HDMI_Unknown) {
		vic = (enum hdmi_vic)hdmitx_rd_reg(HDMITX_DWC_FC_VSDPAYLOAD1);
		if (vic == 1)
			vic = HDMI_3840x2160p30_16x9;
		else if (vic == 2)
			vic = HDMI_3840x2160p25_16x9;
		else if (vic == 3)
			vic = HDMI_3840x2160p24_16x9;
		else if (vic == 4)
			vic = HDMI_4096x2160p24_256x135;
		else
			vic = hd_read_reg(P_ISA_DEBUG_REG0);
	}

	rgb_ycc &= 0x3;
	switch (vic) {
	case HDMI_3840x2160p50_16x9:
		if (rgb_ycc == 0x3)
			vic = HDMI_3840x2160p50_16x9_Y420;
		break;
	case HDMI_4096x2160p50_256x135:
		if (rgb_ycc == 0x3)
			vic = HDMI_4096x2160p50_256x135_Y420;
		break;
	case HDMI_3840x2160p60_16x9:
		if (rgb_ycc == 0x3)
			vic = HDMI_3840x2160p60_16x9_Y420;
		break;
	case HDMI_4096x2160p60_256x135:
		if (rgb_ycc == 0x3)
			vic = HDMI_4096x2160p60_256x135_Y420;
		break;
		break;
	default:
		break;
	}

	return vic;
}

static int hdmitx_get_state(struct hdmitx_dev *hdev, unsigned int cmd,
	unsigned int argv)
{
	if (!(cmd & CMD_STAT_OFFSET))
		hdmi_print(ERR, "stat: hdmitx: w: invalid cmd 0x%x\n", cmd);
	else
		hdmi_print(LOW, "stat: hdmitx: misc cmd 0x%x\n", cmd);

	switch (cmd) {
	case STAT_VIDEO_VIC:
		return (int)get_vic_from_pkt();
	case STAT_VIDEO_CLK:
		break;
	default:
		break;
	}
	return 0;
}

static void hdmi_phy_suspend(void)
{
	hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x0);
}

static void hdmi_phy_wakeup(struct hdmitx_dev *hdev)
{
	hdmitx_set_phy(hdev);
	/* hdmi_print(INF, SYS "phy wakeup\n"); */
}

/* CRT_VIDEO SETTING FUNCTIONS
 * input :
 * vIdx: 0:V1; 1:V2; there have 2 parallel set clock generator: V1 and V2
 * inSel : 0:vid_pll_clk; 1:fclk_div4; 2:flck_div3; 3:fclk_div5;
 * 4:vid_pll2_clk; 5:fclk_div7; 6:vid_pll2_clk;
 * DivN : clock divider for enci_clk/encp_clk/encl_clk/vda_clk
 * /hdmi_tx_pixel_clk;
 */
void set_crt_video_enc(uint32_t vIdx, uint32_t inSel, uint32_t DivN)
{
	if (vIdx == 0) {
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 0, 19, 1);

		hd_set_reg_bits(P_HHI_VID_CLK_CNTL, inSel, 16, 3);
		hd_set_reg_bits(P_HHI_VID_CLK_DIV, (DivN-1), 0, 8);

		hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 1, 19, 1);

	} else { /* V2 */
		hd_set_reg_bits(P_HHI_VIID_CLK_CNTL, 0, 19, 1);

		hd_set_reg_bits(P_HHI_VIID_CLK_CNTL, inSel,  16, 3);
		hd_set_reg_bits(P_HHI_VIID_CLK_DIV, (DivN-1), 0, 8);

		hd_set_reg_bits(P_HHI_VIID_CLK_CNTL, 1, 19, 1);
	}
}

void hdmitx_set_avi_colorimetry(struct hdmi_format_para *para)
{
	if (!para)
		return;

	/* set Colorimetry in AVIInfo */
	switch (para->vic) {
	case HDMI_640x480p60_4x3:
	case HDMI_720x480p60_4x3:
	case HDMI_720x480p60_16x9:
	case HDMI_720x480i60_4x3:
	case HDMI_720x480i60_16x9:
	case HDMI_720x240p60_4x3:
	case HDMI_720x240p60_16x9:
	case HDMI_2880x480i60_4x3:
	case HDMI_2880x480i60_16x9:
	case HDMI_2880x240p60_4x3:
	case HDMI_2880x240p60_16x9:
	case HDMI_1440x480p60_4x3:
	case HDMI_1440x480p60_16x9:
	case HDMI_720x576p50_4x3:
	case HDMI_720x576p50_16x9:
	case HDMI_720x576i50_4x3:
	case HDMI_720x576i50_16x9:
	case HDMI_720x288p_4x3:
	case HDMI_720x288p_16x9:
	case HDMI_2880x576i50_4x3:
	case HDMI_2880x576i50_16x9:
	case HDMI_2880x288p50_4x3:
	case HDMI_2880x288p50_16x9:
	case HDMI_1440x576p_4x3:
	case HDMI_1440x576p_16x9:
	case HDMI_2880x480p60_4x3:
	case HDMI_2880x480p60_16x9:
	case HDMI_2880x576p50_4x3:
	case HDMI_2880x576p50_16x9:
	case HDMI_720x576p100_4x3:
	case HDMI_720x576p100_16x9:
	case HDMI_720x576i100_4x3:
	case HDMI_720x576i100_16x9:
	case HDMI_720x480p120_4x3:
	case HDMI_720x480p120_16x9:
	case HDMI_720x480i120_4x3:
	case HDMI_720x480i120_16x9:
	case HDMI_720x576p200_4x3:
	case HDMI_720x576p200_16x9:
	case HDMI_720x576i200_4x3:
	case HDMI_720x576i200_16x9:
	case HDMI_720x480p240_4x3:
	case HDMI_720x480p240_16x9:
	case HDMI_720x480i240_4x3:
	case HDMI_720x480i240_16x9:
		/* C1C0 601 */
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF1, 1, 6, 2);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF2, 0, 4, 3);
		break;
	default:
		/* C1C0 709 */
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF1, 2, 6, 2);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF2, 0, 4, 3);
		break;
	}
}

/*
 * color_depth: Pixel bit width: 4=24-bit; 5=30-bit; 6=36-bit; 7=48-bit.
 * input_color_format: 0=RGB444; 1=YCbCr422; 2=YCbCr444; 3=YCbCr420.
 * input_color_range: 0=limited; 1=full.
 * output_color_format: 0=RGB444; 1=YCbCr422; 2=YCbCr444; 3=YCbCr420
 */
static void config_hdmi20_tx(enum hdmi_vic vic,
	struct hdmitx_dev *hdev,
	unsigned char color_depth,
	unsigned char input_color_format,
	unsigned char output_color_format)
{
	struct hdmi_format_para *para = hdev->para;
	struct hdmi_cea_timing *t = &para->timing;
	unsigned long   data32;
	unsigned char   vid_map;
	unsigned char   csc_en;
	unsigned char   default_phase = 0;
	unsigned int tmp = 0;

#define GET_TIMING(name)      (t->name)

	/* Enable clocks and bring out of reset */

	/* Enable hdmitx_sys_clk */
	/* .clk0 ( cts_oscin_clk ), */
	/* .clk1 ( fclk_div4 ), */
	/* .clk2 ( fclk_div3 ), */
	/* .clk3 ( fclk_div5 ), */
	hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 0x0100, 0, 16);

	/* Enable clk81_hdmitx_pclk */
	hd_set_reg_bits(P_HHI_GCLK_MPEG2, 1, 4, 1);

	/* wire	wr_enable = control[3]; */
	/* wire	fifo_enable = control[2]; */
	/* assign phy_clk_en = control[1]; */
	/* Enable tmds_clk */
	/* Bring HDMITX MEM output of power down */
	hd_set_reg_bits(P_HHI_MEM_PD_REG0, 0, 8, 8);

	/* Bring out of reset */
	hdmitx_wr_reg(HDMITX_TOP_SW_RESET,  0);

	/* Enable internal pixclk, tmds_clk, spdif_clk, i2s_clk, cecclk */
	hdmitx_set_reg_bits(HDMITX_TOP_CLK_CNTL, 3, 0, 2);
	hdmitx_set_reg_bits(HDMITX_TOP_CLK_CNTL, 3, 4, 2);
	hdmitx_wr_reg(HDMITX_DWC_MC_LOCKONCLOCK, 0xff);

/* But keep spdif_clk and i2s_clk disable
 * until later enable by test.c
 */
	data32  = 0;
	hdmitx_wr_reg(HDMITX_DWC_MC_CLKDIS, data32);

	/* Enable normal output to PHY */

	data32  = 0;
	data32 |= (1 << 12);
	data32 |= (0 << 8);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_TOP_BIST_CNTL, data32);

	/* Configure video */

	vid_map = (input_color_format == COLORSPACE_RGB444) ?
		((color_depth == COLORDEPTH_24B) ? 0x01 :
		(color_depth == COLORDEPTH_30B) ? 0x03 :
		(color_depth == COLORDEPTH_36B) ? 0x05 :
		0x07) :
		((input_color_format == COLORSPACE_YUV444) ||
		(input_color_format == COLORSPACE_YUV420)) ?
		((color_depth == COLORDEPTH_24B) ? 0x09 :
		(color_depth == COLORDEPTH_30B) ? 0x0b :
		(color_depth == COLORDEPTH_36B) ? 0x0d :
		0x0f) :
		((color_depth == COLORDEPTH_24B) ? 0x16 :
		(color_depth == COLORDEPTH_30B) ? 0x14 :
		0x12);

	data32  = 0;
	data32 |= (0 << 7);
	data32 |= (vid_map << 0);
	hdmitx_wr_reg(HDMITX_DWC_TX_INVID0, data32);

	data32  = 0;
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_TX_INSTUFFING, data32);
	hdmitx_wr_reg(HDMITX_DWC_TX_GYDATA0, 0x00);
	hdmitx_wr_reg(HDMITX_DWC_TX_GYDATA1, 0x00);
	hdmitx_wr_reg(HDMITX_DWC_TX_RCRDATA0, 0x00);
	hdmitx_wr_reg(HDMITX_DWC_TX_RCRDATA1, 0x00);
	hdmitx_wr_reg(HDMITX_DWC_TX_BCBDATA0, 0x00);
	hdmitx_wr_reg(HDMITX_DWC_TX_BCBDATA1, 0x00);

	/* Configure Color Space Converter */

	csc_en  = (input_color_format != output_color_format) ? 1 : 0;

	data32  = 0;
	data32 |= (csc_en   << 0);
	hdmitx_wr_reg(HDMITX_DWC_MC_FLOWCTRL, data32);

	data32  = 0;
	data32 |= ((((input_color_format == COLORSPACE_YUV422) &&
		(output_color_format != COLORSPACE_YUV422)) ? 2 : 0) << 4);
	data32 |= ((((input_color_format != COLORSPACE_YUV422) &&
		(output_color_format == COLORSPACE_YUV422)) ? 1 : 0) << 0);
	hdmitx_wr_reg(HDMITX_DWC_CSC_CFG, data32);
	hdmitx_csc_config(input_color_format, output_color_format, color_depth);

	/* Configure video packetizer */

	/* Video Packet color depth and pixel repetition */
	data32  = 0;
	data32 |= (((output_color_format == COLORSPACE_YUV422) ?
		COLORDEPTH_24B : color_depth) << 4);
	data32 |= (0 << 0);
	if ((data32 & 0xf0) == 0x40)
		data32 &= ~(0xf << 4);
	hdmitx_wr_reg(HDMITX_DWC_VP_PR_CD,  data32);
	if (output_color_format == COLORSPACE_YUV422) {
		if (color_depth == COLORDEPTH_24B)
			hdmitx_set_reg_bits(HDMITX_DWC_VP_PR_CD, 0x4, 4, 4);
		else
			hdmitx_set_reg_bits(HDMITX_DWC_VP_PR_CD, 0, 4, 4);
	}

	/* Video Packet Stuffing */
	data32  = 0;
	data32 |= (default_phase << 5);
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_VP_STUFF,  data32);

	/* Video Packet YCC color remapping */
	data32  = 0;
	data32 |= (((color_depth == COLORDEPTH_30B) ? 1 :
		(color_depth == COLORDEPTH_36B) ? 2 : 0) << 0);
	hdmitx_wr_reg(HDMITX_DWC_VP_REMAP, data32);
	if (output_color_format == COLORSPACE_YUV422) {
		switch (color_depth) {
		case COLORDEPTH_36B:
			tmp = 2;
			break;
		case COLORDEPTH_30B:
			tmp = 1;
			break;
		case COLORDEPTH_24B:
			tmp = 0;
			break;
		}
	}
	/* [1:0] ycc422_size */
	hdmitx_set_reg_bits(HDMITX_DWC_VP_REMAP, tmp, 0, 2);

	/* Video Packet configuration */
	data32  = 0;
	data32 |= ((((output_color_format != COLORSPACE_YUV422) &&
		 (color_depth == COLORDEPTH_24B)) ? 1 : 0) << 6);
	data32 |= ((((output_color_format == COLORSPACE_YUV422) ||
		 (color_depth == COLORDEPTH_24B)) ? 0 : 1) << 5);
	data32 |= (0 << 4);
	data32 |= (((output_color_format == COLORSPACE_YUV422) ? 1 : 0)
		<< 3);
	data32 |= (1 << 2);
	data32 |= (((output_color_format == COLORSPACE_YUV422) ? 1 :
		(color_depth == COLORDEPTH_24B) ? 2 : 0) << 0);
	hdmitx_wr_reg(HDMITX_DWC_VP_CONF, data32);

	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (1 << 6);
	data32 |= (1 << 5);
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_VP_MASK, data32);

	/* Configure audio */
	/* I2S Sampler config */

	data32  = 0;
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	hdmitx_wr_reg(HDMITX_DWC_AUD_INT, data32);

	data32  = 0;
	data32 |= (1 << 4);
	hdmitx_wr_reg(HDMITX_DWC_AUD_INT1,  data32);

	hdmitx_wr_reg(HDMITX_DWC_FC_MULTISTREAM_CTRL, 0);

/* if enable it now, fifo_overrun will happen, because packet don't get
 * sent out until initial DE detected.
 */
	data32  = 0;
	data32 |= (0 << 7);
	data32 |= (tx_aud_src << 5);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_AUD_CONF0, data32);

	data32  = 0;
	data32 |= (0 << 5);
	data32 |= (24   << 0);
	hdmitx_wr_reg(HDMITX_DWC_AUD_CONF1, data32);

	data32  = 0;
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_AUD_CONF2, data32);

	/* spdif sampler config */

	data32  = 0;
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIFINT,  data32);

	data32  = 0;
	data32 |= (0 << 4);
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIFINT1, data32);

	data32  = 0;
	data32 |= (0 << 7);
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIF0,	data32);

	data32  = 0;
	data32 |= (0 << 7);
	data32 |= (0 << 6);
	data32 |= (24 << 0);
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIF1,	data32);

	/* Frame Composer configuration */

	/* Video definitions, as per output video(for packet gen/schedulling) */

	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (GET_TIMING(vsync_polarity) << 6);
	data32 |= (GET_TIMING(hsync_polarity) << 5);
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (!(para->progress_mode) << 1);
	data32 |= (!(para->progress_mode) << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_INVIDCONF,  data32);

	data32  = GET_TIMING(h_active)&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_INHACTV0,   data32);
	data32  = (GET_TIMING(h_active)>>8) & 0x3f;
	hdmitx_wr_reg(HDMITX_DWC_FC_INHACTV1,   data32);

	data32  = GET_TIMING(h_blank) & 0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_INHBLANK0,  data32);
	data32  = (GET_TIMING(h_blank)>>8)&0x1f;
	hdmitx_wr_reg(HDMITX_DWC_FC_INHBLANK1,  data32);

	if (hdev->flag_3dfp) {
		data32 = (((GET_TIMING(v_active)) * 2) + (GET_TIMING(v_blank)));
		hdmitx_wr_reg(HDMITX_DWC_FC_INVACTV0, data32 & 0xff);
		hdmitx_wr_reg(HDMITX_DWC_FC_INVACTV1, (data32 >> 8) & 0x1f);
	} else {
		data32 = GET_TIMING(v_active) & 0xff;
		hdmitx_wr_reg(HDMITX_DWC_FC_INVACTV0, data32);
		data32 = (GET_TIMING(v_active) >> 8) & 0x1f;
		hdmitx_wr_reg(HDMITX_DWC_FC_INVACTV1, data32);
	}
	data32  = GET_TIMING(v_blank)&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_INVBLANK,   data32);

	data32  = GET_TIMING(h_front)&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINDELAY0,  data32);
	data32  = (GET_TIMING(h_front)>>8)&0x1f;
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINDELAY1,  data32);

	data32  = GET_TIMING(h_sync)&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINWIDTH0,  data32);
	data32  = (GET_TIMING(h_sync)>>8)&0x3;
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINWIDTH1,  data32);

	data32  = GET_TIMING(v_front)&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_VSYNCINDELAY,   data32);

	data32  = GET_TIMING(v_sync)&0x3f;
	hdmitx_wr_reg(HDMITX_DWC_FC_VSYNCINWIDTH,   data32);

	/* control period duration (typ 12 tmds periods) */
	hdmitx_wr_reg(HDMITX_DWC_FC_CTRLDUR,	12);
	/* extended control period duration (typ 32 tmds periods) */
	hdmitx_wr_reg(HDMITX_DWC_FC_EXCTRLDUR,  32);
	/* max interval betwen extended control period duration (typ 50) */
	hdmitx_wr_reg(HDMITX_DWC_FC_EXCTRLSPAC, 1);
	/* preamble filler */
	hdmitx_wr_reg(HDMITX_DWC_FC_CH0PREAM, 0x0b);
	hdmitx_wr_reg(HDMITX_DWC_FC_CH1PREAM, 0x16);
	hdmitx_wr_reg(HDMITX_DWC_FC_CH2PREAM, 0x21);

	/* write GCP packet configuration */
	data32  = 0;
	data32 |= (default_phase << 2);
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_GCP, data32);

	/* write AVI Infoframe packet configuration */

	data32  = 0;
	data32 |= (((output_color_format>>2)&0x1) << 7);
	data32 |= (1 << 6);
	data32 |= (0 << 4);
	data32 |= (0 << 2);
	data32 |= (0x2 << 0);    /* FIXED YCBCR 444 */
	hdmitx_wr_reg(HDMITX_DWC_FC_AVICONF0, data32);
	switch (output_color_format) {
	case COLORSPACE_RGB444:
		tmp = 0;
		break;
	case COLORSPACE_YUV422:
		tmp = 1;
		break;
	case COLORSPACE_YUV420:
		tmp = 3;
		break;
	case COLORSPACE_YUV444:
	default:
		tmp = 2;
		break;
	}
	hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF0, tmp, 0, 2);

	hdmitx_wr_reg(HDMITX_DWC_FC_AVICONF1, 0x8);
	hdmitx_wr_reg(HDMITX_DWC_FC_AVICONF2, 0);

	/* set Aspect Ratio in AVIInfo */
	switch (para->vic) {
	case HDMI_640x480p60_4x3:
	case HDMI_720x480p60_4x3:
	case HDMI_720x480i60_4x3:
	case HDMI_720x240p60_4x3:
	case HDMI_2880x480i60_4x3:
	case HDMI_2880x240p60_4x3:
	case HDMI_1440x480p60_4x3:
	case HDMI_720x576p50_4x3:
	case HDMI_720x576i50_4x3:
	case HDMI_720x288p_4x3:
	case HDMI_2880x576i50_4x3:
	case HDMI_2880x288p50_4x3:
	case HDMI_1440x576p_4x3:
	case HDMI_2880x480p60_4x3:
	case HDMI_2880x576p50_4x3:
	case HDMI_720x576p100_4x3:
	case HDMI_720x576i100_4x3:
	case HDMI_720x480p120_4x3:
	case HDMI_720x480i120_4x3:
	case HDMI_720x576p200_4x3:
	case HDMI_720x576i200_4x3:
	case HDMI_720x480p240_4x3:
	case HDMI_720x480i240_4x3:
		/* Picture Aspect Ratio M1/M0 4:3 */
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF1, 0x1, 4, 2);
		break;
	default:
		/* Picture Aspect Ratio M1/M0 16:9 */
		hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF1, 0x2, 4, 2);
		break;
	}
	/* Active Format Aspect Ratio R3~R0 Same as picture aspect ratio */
	hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF1, 0x8, 0, 4);

	hdmitx_set_avi_colorimetry(para);
	if (hdev->hdr_color_feature == C_BT2020)
		hdev->HWOp.CntlConfig(hdev, CONF_AVI_BT2020, SET_AVI_BT2020);

	data32  = 0;
	data32 |= (((0 == COLORRANGE_FUL) ? 1 : 0) << 2);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AVICONF3,   data32);

	hdmitx_wr_reg(HDMITX_DWC_FC_AVIVID, (para->vic & HDMITX_VIC_MASK));

	/* write Audio Infoframe packet configuration */

	data32  = 0;
	data32 |= (1 << 4);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF0,  data32);

	data32  = 0;
	data32 |= (3 << 4);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF1, data32);

	hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF2, 0x00);

	data32  = 0;
	data32 |= (1 << 5);
	data32 |= (0 << 4);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF3,  data32);

	/* write audio packet configuration */
	data32  = 0;
	data32 |= (0 << 4);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCONF, data32);

/* the audio setting bellow are only used for I2S audio IEC60958-3 frame
 * insertion
 */
	data32  = 0;
	data32 |= (0 << 7);
	data32 |= (0 << 6);
	data32 |= (0 << 5);
	data32 |= (1 << 4);
	data32 |= (0 << 3);
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSV,  data32);

	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSU,  0);

	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS0, 0x01);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS1, 0x23);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS2, 0x45);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS3, 0x67);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS4, 0x89);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS5, 0xab);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS6, 0xcd);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS7, 0x2f);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS8, 0xf0);

	/* packet queue priority (auto mode) */
	hdmitx_wr_reg(HDMITX_DWC_FC_CTRLQHIGH,  15);
	hdmitx_wr_reg(HDMITX_DWC_FC_CTRLQLOW, 3);

	/* packet scheduller configuration for SPD, VSD, ISRC1/2, ACP. */
	hdmitx_set_reg_bits(HDMITX_DWC_FC_DATAUTO0, 0, 0, 3);
	hdmitx_set_reg_bits(HDMITX_DWC_FC_DATAUTO0, 0, 4, 4);
	hdmitx_wr_reg(HDMITX_DWC_FC_DATAUTO1, 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_DATMAN, 0);

	/* packet scheduller configuration for AVI, GCP, AUDI, ACR. */
	hdmitx_set_reg_bits(HDMITX_DWC_FC_DATAUTO3, 0xe, 0, 6);
	/* If RX  support 2084 or hlg , and the hdr_src_feature is 2020
	 *  then enable HDR send out
	 */
	if ((hdev->RXCap.hdr_sup_eotf_smpte_st_2084 ||
		hdev->RXCap.hdr_sup_eotf_hlg) &&
		(hdev->hdr_color_feature == C_BT2020)) {
		hdmitx_set_reg_bits(HDMITX_DWC_FC_DATAUTO3, 1, 6, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_PACKET_TX_EN, 1, 7, 1);
	} else {
		/* If RX don't support HDR, then enable HDR send out*/
		hdmitx_set_reg_bits(HDMITX_DWC_FC_DATAUTO3, 0, 6, 1);
		hdmitx_set_reg_bits(HDMITX_DWC_FC_PACKET_TX_EN, 0, 7, 1);
	}

	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB0,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB1,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB2,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB3,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB4,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB5,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB6,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB7,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB8,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB9,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB10, 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB11, 0);

	/* Packet transmission enable */
	hdmitx_set_reg_bits(HDMITX_DWC_FC_PACKET_TX_EN, 1, 1, 1);
	hdmitx_set_reg_bits(HDMITX_DWC_FC_PACKET_TX_EN, 1, 2, 1);

	/* For 3D video */
	data32  = 0;
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_ACTSPC_HDLR_CFG, data32);

	data32  = GET_TIMING(v_active)&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_INVACT_2D_0,	data32);
	data32  = (GET_TIMING(v_active)>>8)&0xf;
	hdmitx_wr_reg(HDMITX_DWC_FC_INVACT_2D_1,	data32);

	/* Do not enable these interrupt below, we can check them at RX side. */
	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (1 << 6);
	data32 |= (1 << 5);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_MASK0,  data32);

	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (1 << 6);
	data32 |= (1 << 5);
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_MASK1,  data32);

	data32  = 0;
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_MASK2,  data32);

	/* Pixel repetition ratio the input and output video */
	data32  = 0;
	data32 |= ((para->pixel_repetition_factor+1) << 4);
	data32 |= (para->pixel_repetition_factor << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_PRCONF, data32);

	/* Configure HDCP */
	data32  = 0;
	data32 |= (0 << 7);
	data32 |= (0 << 6);
	data32 |= (0 << 4);
	data32 |= (0 << 3);
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_A_APIINTMSK, data32);

	data32  = 0;
	data32 |= (0 << 5);
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 1);
	hdmitx_wr_reg(HDMITX_DWC_A_VIDPOLCFG,   data32);

	hdmitx_wr_reg(HDMITX_DWC_A_OESSWCFG,    0x40);
	if (hdmitx_hdcp_opr(0xa))
		hdmitx_hdcp_opr(0);
	/* Interrupts */
	/* Clear interrupts */
	hdmitx_wr_reg(HDMITX_DWC_IH_FC_STAT0,  0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_FC_STAT1,  0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_FC_STAT2,  0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_AS_STAT0,  0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_PHY_STAT0, 0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0,	0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_CEC_STAT0, 0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_VP_STAT0,  0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CMPHY_STAT0, 0xff);
	hdmitx_wr_reg(HDMITX_DWC_A_APIINTCLR,  0xff);
	hdmitx_wr_reg(HDMITX_DWC_HDCP22REG_STAT, 0xff);

	hdmitx_wr_reg(HDMITX_TOP_INTR_STAT_CLR,	0x0000001f);

	/* Selectively enable/mute interrupt sources */
	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (1 << 6);
	data32 |= (1 << 5);
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_FC_STAT0,  data32);

	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (1 << 6);
	data32 |= (1 << 5);
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_FC_STAT1,  data32);

	data32  = 0;
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_FC_STAT2,  data32);

	data32  = 0;
	data32 |= (0 << 4);
	data32 |= (0 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_AS_STAT0,  data32);

	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_PHY_STAT0, 0x3f);

	data32  = 0;
	data32 |= (0 << 2);
	data32 |= (1 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_I2CM_STAT0, data32);

	data32  = 0;
	data32 |= (0 << 6);
	data32 |= (0 << 5);
	data32 |= (0 << 4);
	data32 |= (0 << 3);
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_CEC_STAT0, data32);

	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_VP_STAT0,  0xff);

	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_I2CMPHY_STAT0, 0x03);

	data32  = 0;
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE, data32);

	data32  = 0;
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_TOP_INTR_MASKN, data32);

	/* Reset pulse */
	hdmitx_rd_check_reg(HDMITX_DWC_MC_LOCKONCLOCK, 0xff, 0x9f);
	hdmitx_wr_reg(HDMITX_DWC_MC_SWRSTZREQ, 0);
	mdelay(10);

	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (1 << 6);
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	data32 |= (0 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_MC_SWRSTZREQ, data32);
	hdmitx_wr_reg(HDMITX_DWC_FC_VSYNCINWIDTH,
		hdmitx_rd_reg(HDMITX_DWC_FC_VSYNCINWIDTH));
} /* config_hdmi20_tx */

/* TODO */
static void hdmitx_csc_config(unsigned char input_color_format,
			unsigned char output_color_format,
			unsigned char color_depth)
{
	unsigned char conv_en;
	unsigned char csc_scale;
	unsigned char rgb_ycc_indicator;
	unsigned long csc_coeff_a1, csc_coeff_a2, csc_coeff_a3, csc_coeff_a4;
	unsigned long csc_coeff_b1, csc_coeff_b2, csc_coeff_b3, csc_coeff_b4;
	unsigned long csc_coeff_c1, csc_coeff_c2, csc_coeff_c3, csc_coeff_c4;
	unsigned long data32;

	conv_en = (((input_color_format  == COLORSPACE_RGB444) ||
		(output_color_format == COLORSPACE_RGB444)) &&
		(input_color_format  != output_color_format)) ? 1 : 0;

	if (conv_en) {
		if (output_color_format == COLORSPACE_RGB444) {
			csc_coeff_a1 = 0x2000;
			csc_coeff_a2 = 0x6926;
			csc_coeff_a3 = 0x74fd;
			csc_coeff_a4 = (color_depth == COLORDEPTH_24B) ?
				0x010e :
			(color_depth == COLORDEPTH_30B) ? 0x043b :
			(color_depth == COLORDEPTH_36B) ? 0x10ee :
			(color_depth == COLORDEPTH_48B) ? 0x10ee : 0x010e;
		csc_coeff_b1 = 0x2000;
		csc_coeff_b2 = 0x2cdd;
		csc_coeff_b3 = 0x0000;
		csc_coeff_b4 = (color_depth == COLORDEPTH_24B) ? 0x7e9a :
			(color_depth == COLORDEPTH_30B) ? 0x7a65 :
			(color_depth == COLORDEPTH_36B) ? 0x6992 :
			(color_depth == COLORDEPTH_48B) ? 0x6992 : 0x7e9a;
		csc_coeff_c1 = 0x2000;
		csc_coeff_c2 = 0x0000;
		csc_coeff_c3 = 0x38b4;
		csc_coeff_c4 = (color_depth == COLORDEPTH_24B) ? 0x7e3b :
			(color_depth == COLORDEPTH_30B) ? 0x78ea :
			(color_depth == COLORDEPTH_36B) ? 0x63a6 :
			(color_depth == COLORDEPTH_48B) ? 0x63a6 : 0x7e3b;
		csc_scale = 1;
	} else { /* input_color_format == COLORSPACE_RGB444 */
		csc_coeff_a1 = 0x2591;
		csc_coeff_a2 = 0x1322;
		csc_coeff_a3 = 0x074b;
		csc_coeff_a4 = 0x0000;
		csc_coeff_b1 = 0x6535;
		csc_coeff_b2 = 0x2000;
		csc_coeff_b3 = 0x7acc;
		csc_coeff_b4 = (color_depth == COLORDEPTH_24B) ? 0x0200 :
			(color_depth == COLORDEPTH_30B) ? 0x0800 :
			(color_depth == COLORDEPTH_36B) ? 0x2000 :
			(color_depth == COLORDEPTH_48B) ? 0x2000 : 0x0200;
		csc_coeff_c1 = 0x6acd;
		csc_coeff_c2 = 0x7534;
		csc_coeff_c3 = 0x2000;
		csc_coeff_c4 = (color_depth == COLORDEPTH_24B) ? 0x0200 :
			(color_depth == COLORDEPTH_30B) ? 0x0800 :
			(color_depth == COLORDEPTH_36B) ? 0x2000 :
			(color_depth == COLORDEPTH_48B) ? 0x2000 : 0x0200;
		csc_scale = 0;
	}
	} else {
		csc_coeff_a1 = 0x2000;
		csc_coeff_a2 = 0x0000;
		csc_coeff_a3 = 0x0000;
		csc_coeff_a4 = 0x0000;
		csc_coeff_b1 = 0x0000;
		csc_coeff_b2 = 0x2000;
		csc_coeff_b3 = 0x0000;
		csc_coeff_b4 = 0x0000;
		csc_coeff_c1 = 0x0000;
		csc_coeff_c2 = 0x0000;
		csc_coeff_c3 = 0x2000;
		csc_coeff_c4 = 0x0000;
		csc_scale = 1;
	}

	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A1_MSB, (csc_coeff_a1>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A1_LSB, csc_coeff_a1&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A2_MSB, (csc_coeff_a2>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A2_LSB, csc_coeff_a2&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A3_MSB, (csc_coeff_a3>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A3_LSB, csc_coeff_a3&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A4_MSB, (csc_coeff_a4>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A4_LSB, csc_coeff_a4&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B1_MSB, (csc_coeff_b1>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B1_LSB, csc_coeff_b1&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B2_MSB, (csc_coeff_b2>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B2_LSB, csc_coeff_b2&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B3_MSB, (csc_coeff_b3>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B3_LSB, csc_coeff_b3&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B4_MSB, (csc_coeff_b4>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B4_LSB, csc_coeff_b4&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C1_MSB, (csc_coeff_c1>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C1_LSB, csc_coeff_c1&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C2_MSB, (csc_coeff_c2>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C2_LSB, csc_coeff_c2&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C3_MSB, (csc_coeff_c3>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C3_LSB, csc_coeff_c3&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C4_MSB, (csc_coeff_c4>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C4_LSB, csc_coeff_c4&0xff);

	data32 = 0;
	data32 |= (color_depth  << 4);  /* [7:4] csc_color_depth */
	data32 |= (csc_scale << 0);  /* [1:0] cscscale */
	hdmitx_wr_reg(HDMITX_DWC_CSC_SCALE, data32);

	/* set csc in video path */
	hdmitx_wr_reg(HDMITX_DWC_MC_FLOWCTRL, (conv_en == 1)?0x1:0x0);

	/* set rgb_ycc indicator */
	switch (output_color_format) {
	case COLORSPACE_RGB444:
		rgb_ycc_indicator = 0x0;
		break;
	case COLORSPACE_YUV422:
		rgb_ycc_indicator = 0x1;
		break;
	case COLORSPACE_YUV420:
		rgb_ycc_indicator = 0x3;
		break;
	case COLORSPACE_YUV444:
	default:
		rgb_ycc_indicator = 0x2;
		break;
	}
	hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF0,
		((rgb_ycc_indicator & 0x4) >> 2), 7, 1);
	hdmitx_set_reg_bits(HDMITX_DWC_FC_AVICONF0,
		(rgb_ycc_indicator & 0x3), 0, 2);
}   /* hdmitx_csc_config */

static void hdmitx_set_hw(struct hdmitx_dev *hdev)
{
	enum hdmi_vic vic = HDMI_Unknown;
	struct hdmi_format_para *para = NULL;

	if (hdev->cur_video_param == NULL) {
		pr_info("error at null vidpara!\n");
		return;
	}

	vic = (enum hdmi_vic)hdev->cur_video_param->VIC;
	para = hdmi_get_fmt_paras(vic);
	if (para == NULL) {
		pr_info("error at %s[%d] vic = %d\n", __func__, __LINE__, vic);
		return;
	}

	config_hdmi20_tx(vic, hdev,
			hdev->para->cd,
			TX_INPUT_COLOR_FORMAT,
			hdev->para->cs);
}
