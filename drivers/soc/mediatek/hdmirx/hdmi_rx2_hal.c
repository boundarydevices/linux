// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>

#include "mtk_hdmirx.h"
#include "hdmi_rx2_hw.h"
#include "vga_table.h"
#include "hdmi_rx2_hal.h"
#include "hdmi_rx2_hdcp.h"
#include "hdmi_rx2_ctrl.h"
#include "hdmi_rx2_dvi.h"
#include "hdmi_rx2_aud_task.h"
#include "hdmi_rx21_phy.h"

u32 hdmi_fmeter(u8 sel)
{
	u32 temp = 0xffffffff;

	return temp;
}

void delay_ms(u32 delay)
{
	usleep_range(1000 * delay, 1000 * delay + 50);
}

u32
hdmi2com_value_diff(u32 a, u32 b)
{
	if (a >= b)
		return a - b;
	return b - a;
}

void
hdmi2com_term_r(struct MTK_HDMI *myhdmi, u8 term_r)
{
	u32 temp;

	if (term_r > 0)
		temp = term_r;
	else
		temp = 8;

	w_fld(HDMI_CAL, temp,
		REG_CALIB_DATA);
	w_fld(HDMI_CAL, 1,
		REG_CALIB_SEL);
}

void
hdmi2com_sel_aacr_ctrl(struct MTK_HDMI *myhdmi, bool bEn)
{
	w_fld(HDMI_AUD_PLL_SEL, (bEn ? 1 : 0), REG_AUD_PLL_SEL);
}

bool
hdmi2com_get_aud_pll_ctrl(struct MTK_HDMI *myhdmi)
{
	return (bool)r_fld(HDMI_AUD_PLL_SEL, REG_AUD_PLL_SEL);
}

bool
hdmi2com_is_hdcp1x_decrypt_on(struct MTK_HDMI *myhdmi)
{
	return (bool)r_fld(TOP_MISC, HDCP1X_DECRYPT_EN);
}

bool
hdmi2com_is_hdcp2x_decrypt_on(struct MTK_HDMI *myhdmi)
{
	return (bool)r_fld(TOP_MISC, HDCP2X_DECRYPT_EN);
}

u32
hdmi2com_v_res_count(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = r_fld(VID_RES, VRESOLUTION_15_0_);
	return temp;
}

u32
hdmi2com_h_res_count(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = r_fld(VID_RES, HRESOLUTION_15_0_);
	return temp;
}

u32
hdmi2com_h_total(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = ((u32)r_fld(RG_FDET_HSYNC_HIGH_COUNT,
		FDET_HSYNC_LOW_COUNT));
	tmp += ((u32)r_fld(RG_FDET_HSYNC_HIGH_COUNT,
		FDET_HSYNC_HIGH_COUNT));

	return tmp;
}

u32
hdmi2com_v_total_even(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = ((u32)r_fld(RG_FDET_VSYNC_HIGH_COUNT_EVEN,
		FDET_VSYNC_LOW_COUNT_EVEN));
	tmp += ((u32)r_fld(RG_FDET_VSYNC_HIGH_COUNT_EVEN,
		FDET_VSYNC_HIGH_COUNT_EVEN));

	return tmp;
}

u32
hdmi2com_v_total_odd(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = ((u32)r_fld(RG_FDET_VSYNC_HIGH_COUNT_ODD,
		FDET_VSYNC_LOW_COUNT_ODD));
	tmp += ((u32)r_fld(RG_FDET_VSYNC_HIGH_COUNT_ODD,
		FDET_VSYNC_HIGH_COUNT_ODD));

	return tmp;
}

u32
hdmi2com_v_total(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = hdmi2com_v_total_even(myhdmi);

	if (hdmi2com_is_interlace(myhdmi))
		tmp += hdmi2com_v_total_odd(myhdmi);

	return tmp;
}

u32
hdmi2com_h_active(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = ((u32)r_fld(RG_FDET_LINE_COUNT, FDET_PIXEL_COUNT));

	return tmp;
}

u32
hdmi2com_v_active(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = (u32)r_fld(RG_FDET_LINE_COUNT, FDET_LINE_COUNT);

	return tmp;
}

u32
hdmi2com_v_front_porch_odd(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = r_fld(RG_FDET_VBACK_COUNT_ODD, FDET_VFRONT_COUNT_ODD);
	return tmp;
}

u32
hdmi2com_v_front_porch_even(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = r_fld(RG_FDET_VBACK_COUNT_EVEN, FDET_VFRONT_COUNT_EVEN);
	return tmp;
}

u32
hdmi2com_v_back_porch_odd(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = r_fld(RG_FDET_VBACK_COUNT_ODD, FDET_VBACK_COUNT_ODD);
	return tmp;
}

u32
hdmi2com_v_back_porch_even(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = r_fld(RG_FDET_VBACK_COUNT_EVEN, FDET_VBACK_COUNT_EVEN);
	return tmp;
}

u32
hdmi2com_v_sync_high_even(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = r_fld(RG_FDET_VSYNC_HIGH_COUNT_EVEN,
		FDET_VSYNC_HIGH_COUNT_EVEN);
	return tmp;
}

u32
hdmi2com_v_sync_low_even(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = r_fld(RG_FDET_VSYNC_HIGH_COUNT_EVEN,
		FDET_VSYNC_LOW_COUNT_EVEN);
	return tmp;
}

u32
hdmi2com_v_sync_high_odd(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = r_fld(RG_FDET_VSYNC_HIGH_COUNT_ODD,
		FDET_VSYNC_HIGH_COUNT_ODD);
	return tmp;
}

u32
hdmi2com_v_sync_low_odd(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = r_fld(RG_FDET_VSYNC_HIGH_COUNT_ODD,
		FDET_VSYNC_LOW_COUNT_ODD);
	return tmp;
}

u32
hdmi2com_h_front_porch(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = r_fld(RG_FDET_HBACK_COUNT,	FDET_HFRONT_COUNT);
	return tmp;
}

u32
hdmi2com_h_back_porch(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = r_fld(RG_FDET_HBACK_COUNT, FDET_HBACK_COUNT);
	return tmp;
}

u32
hdmi2com_h_sync_high(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = r_fld(RG_FDET_HSYNC_HIGH_COUNT, FDET_HSYNC_HIGH_COUNT);
	return tmp;
}

u32
hdmi2com_h_sync_low(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = r_fld(RG_FDET_HSYNC_HIGH_COUNT, FDET_HSYNC_LOW_COUNT);
	return tmp;
}

void
hdmi2com_timing_changed_irq_clr(struct MTK_HDMI *myhdmi)
{
	w_reg(O_FDET_IRQ_STATUS, 0xffffffff);
	w_reg(O_FDET_IRQ_STATUS, 0);
}

u32
hdmi2com_h_de_changed(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = r_reg(I_FDET_IRQ_STATUS);

	return temp & 0x600;
}

u32
hdmi2com_h_total_changed(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = r_reg(I_FDET_IRQ_STATUS);

	return temp & 0x180;
}

u32
hdmi2com_bus_clk_freq(struct MTK_HDMI *myhdmi)
{
	return 192000038;
}

u32
hdmi2com_frame_rate(struct MTK_HDMI *myhdmi)
{
	u32 tmp, u4CpuBusClock;
	u32 u4FrameRate = HDMI2_COM_INVALID_FRAME_RATE;

	tmp = hdmi2_vrr_frame_rate(myhdmi);
	if (tmp != 0) {
		RX_DEF_LOG("[RX]VRR en, base frame rate: %d\n", tmp);
		return tmp;
	}

	tmp = (u32)r_fld(RG_FDET_FRAME_RATE, FDET_FRAME_RATE);
	if (tmp == 0) {
		RX_DEF_LOG("[RX]invalid frame rate\n");
		return tmp;
	}
	u4CpuBusClock = hdmi2com_bus_clk_freq(myhdmi);

	tmp = (u32)u4CpuBusClock * 10 / tmp;
	tmp = tmp * 10;

	if ((tmp >= 11900) && (tmp <= 12100))
		u4FrameRate = 120;
	else if ((tmp >= 9900) && (tmp <= 10100))
		u4FrameRate = 100;
	else if ((tmp <= 6800) && (tmp >= 6500))
		u4FrameRate = 67;
	else if ((tmp <= 7100) && (tmp >= 6900))
		u4FrameRate = 70;
	else if ((tmp <= 7300) && (tmp >= 7100))
		u4FrameRate = 72;
	else if ((tmp <= 7600) && (tmp >= 7400))
		u4FrameRate = 75;
	else if ((tmp <= 8600) && (tmp >= 8400))
		u4FrameRate = 85;
	else if ((tmp >= 5950) && (tmp <= 6050))
		u4FrameRate = 60;
	else if ((tmp <= 5700) && (tmp >= 5500))
		u4FrameRate = 56;
	else if ((tmp >= 4950) && (tmp <= 5050))
		u4FrameRate = 50;
	else if ((tmp >= 2350) && (tmp <= 2450))
		u4FrameRate = 24;
	else if ((tmp >= 2450) && (tmp <= 2550))
		u4FrameRate = 25;
	else if ((tmp >= 2950) && (tmp <= 3050))
		u4FrameRate = 30;

	return u4FrameRate;
}

u32
hdmi2com_xclk_of_pclk(struct MTK_HDMI *myhdmi)
{
	u32 tmp;

	tmp = r_fld(RG_RX_APLL_POLE, AAC_XCLK_IN_PCLK23_0);

	return tmp;
}

u32
hdmi2com_pixel_clk(struct MTK_HDMI *myhdmi)
{
	u32 tmp, u4CpuBusClock;

	tmp = hdmi2com_xclk_of_pclk(myhdmi);

	if (tmp == 0) {
		RX_DEF_LOG("[RX]invalid pixel clock\n");
		return tmp;
	}

	u4CpuBusClock = hdmi2com_bus_clk_freq(myhdmi);
	tmp = 2048 * (u4CpuBusClock / 1000) / tmp;
	tmp = tmp * 1000;

	return tmp;
}

u32
hdmi2com_h_clk(struct MTK_HDMI *myhdmi)
{
	u32 ret;
	u32 wDiv;

	wDiv = hdmi2com_h_total(myhdmi);

	if (wDiv == 0)
		return 1;

	ret = (hdmi2com_pixel_clk(myhdmi) / (wDiv));
	ret /= 100;

	return ret;
}

bool
hdmi2com_is_h_res_stb(struct MTK_HDMI *myhdmi)
{
	return (bool)r_fld(TOP_MISC, HRES_STB);
}

bool
hdmi2com_is_v_res_stb(struct MTK_HDMI *myhdmi)
{
	return (bool)r_fld(TOP_MISC, VRES_STB);
}

bool
hdmi2com_ana_crc(struct MTK_HDMI *myhdmi, u16 u2Times)
{
	/* to do */
	u32 u4CrcResult[2];
	u32 u4CrcTmpData;
	u8 index = 0;

	u4CrcResult[0] = 0;
	u4CrcResult[1] = 0;

	/* to do */
	while (u2Times) {
		u2Times--;
		msleep(20);
		if (index > 2) {
			RX_DEF_LOG("[RX]CRC fail\n");
			return 0;
		}
		w_fld(DIG_PHY_CONTROL_1, 0, C_CRC_START);
		w_fld(DIG_PHY_CONTROL_1, 1, C_CRC_CLR);
		usleep_range(1000, 1050);
		w_fld(DIG_PHY_STATUS_0, 1, HDMI20_DIG_PHY_CRC_FAIL);
		w_fld(DIG_PHY_STATUS_0, 1, HDMI20_DIG_PHY_CRC_RDY);

		while (r_fld(DIG_PHY_STATUS_0, HDMI20_DIG_PHY_CRC_RDY) != 0) {
			usleep_range(1000, 1050);
			if (r_fld(TOP_MISC,
				    SCDT) == 0)
				break;
		}
		while (r_fld(DIG_PHY_STATUS_0, HDMI20_DIG_PHY_CRC_OUT) != 0) {
			usleep_range(1000, 1050);
			if (r_fld(TOP_MISC, SCDT) == 0)
				break;
		}
		w_fld(DIG_PHY_CONTROL_1, 0, C_CRC_CLR);
		if (r_fld(DIG_PHY_STATUS_0, HDMI20_DIG_PHY_CRC_RDY) == 0) {

			w_fld(DIG_PHY_CONTROL_1, 1, C_CRC_START);
			while (r_fld(DIG_PHY_STATUS_0,
				HDMI20_DIG_PHY_CRC_RDY) != 1) {
				msleep(100);
				if (r_fld(TOP_MISC, SCDT) == 0)
					break;
			}

			if (r_fld(DIG_PHY_STATUS_0,
				    HDMI20_DIG_PHY_CRC_RDY) == 1) {
				u4CrcTmpData =
					r_fld(DIG_PHY_STATUS_0,
						HDMI20_DIG_PHY_CRC_OUT);

				if ((u4CrcTmpData == u4CrcResult[0]) ||
					(u4CrcTmpData == u4CrcResult[1]))
					continue;

				if (index < 2) {
					u4CrcResult[index] = u4CrcTmpData;
					index++;
				}
			} else {
				RX_DEF_LOG("[RX]CRC not ready\n");
				return 0;
			}
		}
	}

	if (hdmi2com_is_interlace(myhdmi))
		RX_DEF_LOG("[RX]interlaced signal\n");
	else
		RX_DEF_LOG("[RX]progressive signal\n");

	if (index == 1) {
		RX_DEF_LOG("[RX]assume progressive signal\n");
		RX_DEF_LOG("[RX]CRC result:\n");
		RX_DEF_LOG("[RX]0x%x\n", u4CrcResult[0]);
	} else if (index == 2) {
		RX_DEF_LOG("[RX]assume interlaced signal\n");
		RX_DEF_LOG("[RX]CRC result:\n");
		RX_DEF_LOG("[RX]0x%x\n", u4CrcResult[0]);
		RX_DEF_LOG("[RX]0x%x\n", u4CrcResult[1]);
	}
	return 0;
}

u32 hdmi2com_ana_get_crcvalue(struct MTK_HDMI *myhdmi)
{
	return r_fld(DIG_PHY_STATUS_0, HDMI20_DIG_PHY_CRC_OUT);
}

bool hdmi2com_ana_get_crcfail(struct MTK_HDMI *myhdmi)
{
	return r_fld(DIG_PHY_STATUS_0, HDMI20_DIG_PHY_CRC_FAIL);
}

bool hdmi2com_ana_get_crcready(struct MTK_HDMI *myhdmi)
{
	return r_fld(DIG_PHY_STATUS_0, HDMI20_DIG_PHY_CRC_RDY);
}

bool
hdmi2com_crc(struct MTK_HDMI *myhdmi, u16 u2Times)
{
	u32 u4CrcResult[2];
	u32 u4CrcTmpData;
	u8 index = 0;

	u4CrcResult[0] = 0;
	u4CrcResult[1] = 0;

	while (u2Times) {
		u2Times--;
		msleep(20);
		if (index > 2) {
			RX_DEF_LOG("[RX]CRC fail\n");
			return 0;
		}

		w_fld(RG_HDMI_CRC, 1, HDMI20_CRC_CLR);
		w_fld(RG_HDMI_CRC, 0, HDMI20_CRC_START);
		w_fld(RG_HDMI_CRC, 3, HDMI20_CRC_MODE);
		usleep_range(1000, 1050);
		w_fld(RG_HDMI_CRC, 0, HDMI20_VIDEO_CRC_RDY);
		w_fld(RG_HDMI_CRC, 1, HDMI20_VIDEO_CRC_FAIL);

		while (r_fld(RG_HDMI_CRC, HDMI20_VIDEO_CRC_RDY) != 0) {
			usleep_range(10000, 10050);
			if (r_fld(TOP_MISC, SCDT) == 0)
				break;
		}

		while (r_fld(RG_HDMI_CRC, HDMI20_VIDEO_CRC_OUT) != 0) {
			usleep_range(10000, 10050);
			if (r_fld(TOP_MISC, SCDT) == 0)
				break;
		}

		w_fld(RG_HDMI_CRC, 0, HDMI20_CRC_CLR);

		if (r_fld(RG_HDMI_CRC, HDMI20_VIDEO_CRC_RDY) == 0) {
			w_fld(RG_HDMI_CRC, 1, HDMI20_CRC_START);
			while (r_fld(RG_HDMI_CRC, HDMI20_VIDEO_CRC_RDY) != 1) {
				msleep(100);
				if (r_fld(TOP_MISC, SCDT) == 0)
					break;
			}

			if (r_fld(RG_HDMI_CRC, HDMI20_VIDEO_CRC_RDY) == 1) {
				u4CrcTmpData = r_fld(RG_HDMI_CRC,
					HDMI20_VIDEO_CRC_OUT);

				if ((u4CrcTmpData == u4CrcResult[0]) ||
					(u4CrcTmpData == u4CrcResult[1]))
					continue;

				if (index < 2)
					u4CrcResult[index] = u4CrcTmpData;
				index++;
			} else {
				RX_DEF_LOG("[RX]CRC not ready\n");
				return 0;
			}
		}
	}

	if (hdmi2com_is_interlace(myhdmi))
		RX_DEF_LOG("[RX]interlaced signal\n");
	else
		RX_DEF_LOG("[RX]progressive signal\n");

	if (index == 1) {
		RX_DEF_LOG("[RX]assume progressive signal\n");
		RX_DEF_LOG("[RX]CRC result:\n");
		RX_DEF_LOG("[RX]0x%x\n", u4CrcResult[0]);
		myhdmi->crc0 = u4CrcResult[0];
	} else if (index == 2) {
		RX_DEF_LOG("[RX]assume interlaced signal\n");
		RX_DEF_LOG("[RX]CRC result:\n");
		RX_DEF_LOG("[RX]0x%x\n", u4CrcResult[0]);
		RX_DEF_LOG("[RX]0x%x\n", u4CrcResult[1]);
		myhdmi->crc0 = u4CrcResult[0];
		myhdmi->crc1 = u4CrcResult[1];
	}
	return 1;
}

u32 hdmi2com_dig_get_crcvalue(struct MTK_HDMI *myhdmi)
{
	return r_fld(RG_HDMI_CRC, HDMI20_VIDEO_CRC_OUT);
}

bool hdmi2com_dig_get_crcfail(struct MTK_HDMI *myhdmi)
{
	return r_fld(RG_HDMI_CRC, HDMI20_VIDEO_CRC_FAIL);
}

bool hdmi2com_dig_get_crcready(struct MTK_HDMI *myhdmi)
{
	return r_fld(RG_HDMI_CRC, HDMI20_VIDEO_CRC_RDY);
}

bool
hdmi2com_is_interlace(struct MTK_HDMI *myhdmi)
{
	return (bool)r_fld(FDET_STATUS, INTERLACED_DETECT);
}

bool
hdmi2com_is_av_mute(struct MTK_HDMI *myhdmi)
{
	return (bool)r_fld(RX_AUDP_STAT, HDMI_MUTE);
}

void
hdmi2com_av_mute_clr(struct MTK_HDMI *myhdmi, u8 u1En)
{
	w_fld(RX_AUDP_STAT, u1En, REG_CLEAR_AV_MUTE);
}

void
hdmi2com_aud_mute_mode(struct MTK_HDMI *myhdmi, bool dsd)
{
	if (dsd)
		w_fld(RG_RX_AUDRX_CTRL, 1, REG_HW_MUTE_EN);
	else
		w_fld(RG_RX_AUDRX_CTRL, 0, REG_HW_MUTE_EN);
}

void
hdmi2com_aud_mute_set(struct MTK_HDMI *myhdmi, u8 u1En)
{
	w_fld(RG_RX_TDM_CTRL1, u1En, REG_AUDIO_MUTE);
}

void
hdmi2com_vid_mute_set(struct MTK_HDMI *myhdmi, u8 u1En)
{
	w_fld(RX_AUDP_STAT, u1En, REG_VIDEO_MUTE);
}

void
hdmi2com_vid_mute_set_force(struct MTK_HDMI *myhdmi, bool en)
{
	if (en)
		w_fld(RG_OUTPUT_BLANK_Y, 1, ENABLE_ACTIVE_OVERRIDE);
	else
		w_fld(RG_OUTPUT_BLANK_Y, 0, ENABLE_ACTIVE_OVERRIDE);
}

bool hdmi2com_is_video_mute(struct MTK_HDMI *myhdmi)
{
	if (r_fld(RG_OUTPUT_BLANK_Y, ENABLE_ACTIVE_OVERRIDE))
		return TRUE;
	return FALSE;
}

void
hdmi2com_vid_black_set(struct MTK_HDMI *myhdmi, u8 u1IsRgb)
{
	if (u1IsRgb) {
		w_fld(RG_OUTPUT_ACTIVE_CB, 0, OUTPUT_ACTIVE_Y);
		w_fld(RG_OUTPUT_ACTIVE_CB, 0, OUTPUT_ACTIVE_CB);
		w_fld(RG_OUTPUT_ACTIVE_CR, 0, OUTPUT_ACTIVE_CR);
	} else {
		w_fld(RG_OUTPUT_ACTIVE_CB, 0x100, OUTPUT_ACTIVE_Y);
		w_fld(RG_OUTPUT_ACTIVE_CB, 0x800, OUTPUT_ACTIVE_CB);
		w_fld(RG_OUTPUT_ACTIVE_CR, 0x800, OUTPUT_ACTIVE_CR);
	}
}

void
hdmi2com_aac_en(struct MTK_HDMI *myhdmi, u8 u1En)
{
	/* to do */
	w_fld(RG_RX_AEC_EN1, u1En, REG_AAC_EN);
	w_fld(RG_RX_AEC_EN1, u1En, REG_AAC_ALL);
	w_fld(RG_RX_AEC_EN1, u1En, REG_AAC_OUT_OFF_EN);
	w_fld(RG_RX_AEC_EN1, u1En, REG_CTRL_ACR_EN);
}

bool
hdmi2com_is_hdmi_mode(struct MTK_HDMI *myhdmi)
{
	return (bool)r_fld(RX_AUDP_STAT, HDMI_MODE_DET);
}

bool
hdmi2com_is_aud_mute(struct MTK_HDMI *myhdmi)
{
	return (bool)r_fld(RG_RX_TDM_CTRL1, AAC_AUDIO_MUTE);
}

bool
hdmi2com_is_scrambing_by_scdc(struct MTK_HDMI *myhdmi)
{
	return (bool)r_fld(TOP_MISC, SCRAMBLING_EN);
}

bool
hdmi2com_is_hw_scrambing_en(struct MTK_HDMI *myhdmi)
{
	return (bool)r_fld(DIG_PHY_CONTROL_2, HW_SCRAMBLING_ON_EN);
}

bool
hdmi2com_is_tmds_clk_radio_40x(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->force_40x == 1)
		return TRUE;
	return (bool)r_fld(TOP_MISC, TMDS_CLK_RATIO);
}

bool
hdmi2com_is_ckdt_1(struct MTK_HDMI *myhdmi)
{
	//return (bool)r_fld(TOP_MISC, CKDT);
	return _KHal_HDMIRx_GetClockStableFlag(myhdmi);
}

bool
hdmi2com_is_scdt_1(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->vrr_en)
		return myhdmi->scdt;
	return (bool)r_fld(TOP_MISC, SCDT);
}

bool
hdmi2com_is_vsync_detected(struct MTK_HDMI *myhdmi)
{
	/* 2e018[14] */
	if (r_fld(RX_INTR4, REG_INTR2_STAT6)) {
		w_reg(RX_INTR4,	Fld2Msk32(REG_INTR2_STAT6));
		return TRUE;
	}

	return FALSE;
}

bool
hdmi2com_is_vid_clk_changed(struct MTK_HDMI *myhdmi)
{
	/* 2e018[8] */
	if (r_fld(RX_INTR4, REG_INTR2_STAT0)) {
		w_reg(RX_INTR4,	Fld2Msk32(REG_INTR2_STAT0));
		return TRUE;
	}

	return FALSE;
}

void
hdmi2com_vid_clk_changed_clr(struct MTK_HDMI *myhdmi)
{
	w_reg(RX_INTR4,	Fld2Msk32(REG_INTR2_STAT0));
}

bool
hdmi2com_is_pdt_clk_changed(struct MTK_HDMI *myhdmi)
{
	if (r_fld(INTR_STATUS_RESVD, INTR_VID_CLK_CHG)) {
		w_reg(INTR_STATUS_RESVD, Fld2Msk32(INTR_VID_CLK_CHG));
		return TRUE;
	}

	return FALSE;
}

bool
hdmi2com_is_hdcp2x_ecc_out_of_sync(struct MTK_HDMI *myhdmi)
{
	if (r_fld(RX_HDCP2X_INTR_MASK, REG_ECC_OUT_OF_SYNC_INTR)) {
		w_reg(RX_HDCP2X_INTR_MASK, Fld2Msk32(REG_ECC_OUT_OF_SYNC_INTR));
		return TRUE;
	}
	return FALSE;
}

void
hdmi2com_clear_hdcp2x_accm(struct MTK_HDMI *myhdmi)
{
	w_fld(RX_HDCP2X_ECC_CTRL, 1, RI_ACCM_ERR_MANU_CLR);
	w_fld(RX_HDCP2X_ECC_CTRL, 0, RI_ACCM_ERR_MANU_CLR);
}

bool
hdmi2com_is_h_unstable(struct MTK_HDMI *myhdmi)
{
	if (r_fld(INTR_STATUS_RESVD, INTR_HDERES_UNSTABLE)) {
		w_reg(INTR_STATUS_RESVD, Fld2Msk32(INTR_HDERES_UNSTABLE));
		return TRUE;
	}

	return FALSE;
}

bool
hdmi2com_is_v_unstable(struct MTK_HDMI *myhdmi)
{
	if (r_fld(INTR_STATUS_RESVD, INTR_VDERES_UNSTABLE)) {
		w_reg(INTR_STATUS_RESVD, Fld2Msk32(INTR_VDERES_UNSTABLE));
		return TRUE;
	}

	return FALSE;
}

u32
hdmi2com_cts_of_ncts(struct MTK_HDMI *myhdmi)
{
	return (r_fld(RG_RX_UPLL_SVAL, CTS_VAL_HW_19_16_) << 16) |
	       (r_fld(RG_RX_UPLL_SVAL, CTS_VAL_HW_15_8_) << 8) |
	       (r_fld(RG_RX_UPLL_SVAL, CTS_VAL_HW_7_0_));
}

u32
hdmi2com_n_of_ncts(struct MTK_HDMI *myhdmi)
{
	return (r_fld(RG_RX_CTS_SVAL3, N_VAL_HW_19_16_) << 16) |
	       (r_fld(RG_RX_N_HVAL2, N_VAL_HW_15_8_) << 8) |
	       (r_fld(RG_RX_N_HVAL2, N_VAL_HW_7_0_));
}

u8
hdmi2com_aud_fs(struct MTK_HDMI *myhdmi)
{
	u32 temp, temp1;

	temp = r_fld(RG_RX_TCLK_FS, RHDMI_AUD_SAMPLE_F);

	if ((temp == HDMI2_AUD_FS_44K) || (temp == HDMI2_AUD_FS_88K) ||
		(temp == HDMI2_AUD_FS_176K) || (temp == HDMI2_AUD_FS_48K) ||
		(temp == HDMI2_AUD_FS_96K) || (temp == HDMI2_AUD_FS_192K) ||
		(temp == HDMI2_AUD_FS_32K) || (temp == HDMI2_AUD_FS_768K) ||
		(temp == HDMI2_AUD_FS_UNKNOWN))
		return temp;

	temp1 = r_fld(RG_RX_TCLK_FS, RHDMI_AUD_SAMPLE_F_EXTN);
	temp = temp + (temp1 << 4);

	if ((temp == HDMI2_AUD_FS_384K) || (temp == HDMI2_AUD_FS_1536K) ||
		(temp == HDMI2_AUD_FS_1024K) || (temp == HDMI2_AUD_FS_3528K) ||
		(temp == HDMI2_AUD_FS_7056K) || (temp == HDMI2_AUD_FS_14112K) ||
		(temp == HDMI2_AUD_FS_64K) || (temp == HDMI2_AUD_FS_128K) ||
		(temp == HDMI2_AUD_FS_256K) || (temp == HDMI2_AUD_FS_512K))
		return temp;

	return HDMI2_AUD_FS_UNKNOWN;
}

u32
hdmi2com_tmds_clk_freq(struct MTK_HDMI *myhdmi)
{
	u32 u4TMDSClkFreq, u4PixelClkFreq, u410bitClkFreq;

	u4PixelClkFreq = hdmi2com_pixel_clk(myhdmi);
	/* The real pixel clk should be divided by 2 for YC420 */
	if (hdmi2com_is_420_mode(myhdmi))
		u4PixelClkFreq = u4PixelClkFreq / 2;

	if (hdmi2com_get_deep_status(myhdmi) == HDMI_RX_BIT_DEPTH_10_BIT)
		u410bitClkFreq = (u4PixelClkFreq / 8) * 10;
	else if (hdmi2com_get_deep_status(myhdmi) == HDMI_RX_BIT_DEPTH_12_BIT)
		u410bitClkFreq = (u4PixelClkFreq / 8) * 12;
	else if (hdmi2com_get_deep_status(myhdmi) == HDMI_RX_BIT_DEPTH_16_BIT)
		u410bitClkFreq = (u4PixelClkFreq / 8) * 16;
	else
		u410bitClkFreq = u4PixelClkFreq;

	if (hdmi2com_is_tmds_clk_radio_40x(myhdmi))
		u4TMDSClkFreq = u410bitClkFreq / 4;
	else
		u4TMDSClkFreq = u410bitClkFreq;

	return u4TMDSClkFreq;
}

u8
hdmi2com_aud_fs_cal(struct MTK_HDMI *myhdmi)
{
	u32 wCTS_HW, wN_HW;
	u32 wAudSampleRate, wTMDSClkFreq;
	u8 btmp;
	static u8 Ori_audio_FS;

	btmp = 20;

	wCTS_HW = hdmi2com_cts_of_ncts(myhdmi);
	wN_HW = hdmi2com_n_of_ncts(myhdmi);
	wTMDSClkFreq = hdmi2com_tmds_clk_freq(myhdmi); /* pixclk */

	if (wCTS_HW == 0) {
		RX_DEF_LOG("[RX]invalid C value\n");
		return HDMI2_AUD_FS_UNKNOWN;
	}

	if (hdmi2com_is_tmds_clk_radio_40x(myhdmi))
		wAudSampleRate = (((4 * wTMDSClkFreq) / wCTS_HW) *
			((wN_HW * 100) / 128)) / 10000;
	else
		wAudSampleRate = ((wTMDSClkFreq / wCTS_HW) *
			((wN_HW * 100) / 128)) / 10000;

	if (wAudSampleRate > (320 - btmp) && wAudSampleRate < (320 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_32K)
			Ori_audio_FS = HDMI2_AUD_FS_32K;
		return HDMI2_AUD_FS_32K;
	} else if (wAudSampleRate > (441 - btmp) &&
		   wAudSampleRate < (441 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_44K)
			Ori_audio_FS = HDMI2_AUD_FS_44K;
		return HDMI2_AUD_FS_44K;
	} else if (wAudSampleRate > (480 - btmp) &&
		   wAudSampleRate < (480 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_48K)
			Ori_audio_FS = HDMI2_AUD_FS_48K;
		return HDMI2_AUD_FS_48K;
	} else if (wAudSampleRate > (880 - btmp) &&
		   wAudSampleRate < (880 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_88K)
			Ori_audio_FS = HDMI2_AUD_FS_88K;
		return HDMI2_AUD_FS_88K;
	} else if (wAudSampleRate > (960 - btmp) &&
		   wAudSampleRate < (960 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_96K)
			Ori_audio_FS = HDMI2_AUD_FS_96K;
		return HDMI2_AUD_FS_96K;
	} else if (wAudSampleRate > (1760 - btmp) &&
		   wAudSampleRate < (1760 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_176K)
			Ori_audio_FS = HDMI2_AUD_FS_176K;
		return HDMI2_AUD_FS_176K;
	} else if (wAudSampleRate > (1920 - btmp) &&
		   wAudSampleRate < (1920 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_192K)
			Ori_audio_FS = HDMI2_AUD_FS_192K;
		return HDMI2_AUD_FS_192K;
	} else if (wAudSampleRate > (640 - btmp) &&
		   wAudSampleRate < (640 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_64K)
			Ori_audio_FS = HDMI2_AUD_FS_64K;
		return HDMI2_AUD_FS_64K;
	} else if (wAudSampleRate > (1280 - btmp) &&
		   wAudSampleRate < (1280 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_128K)
			Ori_audio_FS = HDMI2_AUD_FS_128K;
		return HDMI2_AUD_FS_128K;
	} else if (wAudSampleRate > (2560 - btmp) &&
		   wAudSampleRate < (2560 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_256K)
			Ori_audio_FS = HDMI2_AUD_FS_256K;
		return HDMI2_AUD_FS_256K;
	} else if (wAudSampleRate > (5120 - btmp) &&
		   wAudSampleRate < (5120 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_512K)
			Ori_audio_FS = HDMI2_AUD_FS_512K;
		return HDMI2_AUD_FS_512K;
	} else if (wAudSampleRate > (3840 - btmp) &&
		   wAudSampleRate < (3840 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_384K)
			Ori_audio_FS = HDMI2_AUD_FS_384K;
		return HDMI2_AUD_FS_384K;
	} else if (wAudSampleRate > (3528 - btmp) &&
		   wAudSampleRate < (3528 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_3528K)
			Ori_audio_FS = HDMI2_AUD_FS_3528K;
		return HDMI2_AUD_FS_3528K;
	} else if (wAudSampleRate > (7056 - btmp) &&
		   wAudSampleRate < (7056 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_7056K)
			Ori_audio_FS = HDMI2_AUD_FS_7056K;
		return HDMI2_AUD_FS_7056K;
	} else if (wAudSampleRate > (14112 - btmp) &&
		   wAudSampleRate < (14112 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_14112K)
			Ori_audio_FS = HDMI2_AUD_FS_14112K;
		return HDMI2_AUD_FS_14112K;
	} else if (wAudSampleRate > (10240 - btmp) &&
		   wAudSampleRate < (10240 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_1024K)
			Ori_audio_FS = HDMI2_AUD_FS_1024K;
		return HDMI2_AUD_FS_1024K;
	} else if (wAudSampleRate > (15360 - btmp) &&
		   wAudSampleRate < (15360 + btmp)) {
		if (Ori_audio_FS != HDMI2_AUD_FS_1536K)
			Ori_audio_FS = HDMI2_AUD_FS_1536K;
		return HDMI2_AUD_FS_1536K;
	} else {
		return HDMI2_AUD_FS_UNKNOWN;
	}
}

void hdmi2com_fs_sw_mode(struct MTK_HDMI *myhdmi,
	unsigned int mode, unsigned int fs)
{
	if (mode == 0) {
		w_fld(ACR_CONTROL_1, 0, FS_HW_SW_SEL); //Fs HW mode
		return;
	}

	if (fs == HDMI2_AUD_FS_32K) {
		w_fld(ACR_CONTROL_0, 0x03, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 0, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_44K) {
		w_fld(ACR_CONTROL_0, 0x0, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 0, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_48K) {
		w_fld(ACR_CONTROL_0, 0x02, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 0, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_64K) {
		w_fld(ACR_CONTROL_0, 0x0b, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 0, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_88K) {
		w_fld(ACR_CONTROL_0, 0x08, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 0, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_96K) {
		w_fld(ACR_CONTROL_0, 0x0a, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 0, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_128K) {
		w_fld(ACR_CONTROL_0, 0x2b, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 2, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_176K) {
		w_fld(ACR_CONTROL_0, 0x0c, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 0, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_192K) {
		w_fld(ACR_CONTROL_0, 0x0e, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 0, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_256K) {
		w_fld(ACR_CONTROL_0, 0x1b, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 1, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_3528K) {
		w_fld(ACR_CONTROL_0, 0x0d, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 0, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_384K) {
		w_fld(ACR_CONTROL_0, 0x05, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 0, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_512K) {
		w_fld(ACR_CONTROL_0, 0x3b, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 3, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_768K) {
		w_fld(ACR_CONTROL_0, 0x09, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 0, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_1024K) {
		w_fld(ACR_CONTROL_0, 0x35, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 3, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_1536K) {
		w_fld(ACR_CONTROL_0, 0x15, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 1, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_7056K) {
		w_fld(ACR_CONTROL_0, 0x2d, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 2, FS_VAL_SW_EXTN);
	} else if (fs == HDMI2_AUD_FS_14112K) {
		w_fld(ACR_CONTROL_0, 0x1d, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 1, FS_VAL_SW_EXTN);
	} else {
		w_fld(ACR_CONTROL_0, 0x02, FS_VAL_SW);
		w_fld(HDMI_AUD_PLL_SEL, 0, FS_VAL_SW_EXTN);
	}

	w_fld(ACR_CONTROL_1, 1, FS_HW_SW_SEL);
}

void hdmi2com_hbr_sw_mode(struct MTK_HDMI *myhdmi,
	unsigned int mode)
{
	if (mode == 0) {
		RX_AUDIO_LOG("[RX]not hbr\n");
		w_fld(ACR_CONTROL_1, 0, FS_HW_SW_SEL);
	} else if (mode == HDMI2_AUD_FS_48K) {
		RX_AUDIO_LOG("[RX]hbr 192k\n");
		w_fld(ACR_CONTROL_1, 1, FS_HW_SW_SEL);
		w_fld(ACR_CONTROL_0, 0x02, FS_VAL_SW);
	} else if (mode == HDMI2_AUD_FS_64K) {
		RX_AUDIO_LOG("[RX]hbr 256k\n");
		w_fld(ACR_CONTROL_1, 1, FS_HW_SW_SEL);
		w_fld(ACR_CONTROL_0, 0x0b, FS_VAL_SW);
	} else if (mode == HDMI2_AUD_FS_88K) {
		RX_AUDIO_LOG("[RX]hbr 352.8k\n");
		w_fld(ACR_CONTROL_1, 1, FS_HW_SW_SEL);
		w_fld(ACR_CONTROL_0, 0x08, FS_VAL_SW);
	} else if (mode == HDMI2_AUD_FS_96K) {
		RX_AUDIO_LOG("[RX]hbr 384k\n");
		w_fld(ACR_CONTROL_1, 1, FS_HW_SW_SEL);
		w_fld(ACR_CONTROL_0, 0x0a, FS_VAL_SW);
	} else if (mode == HDMI2_AUD_FS_128K) {
		RX_AUDIO_LOG("[RX]hbr 512k\n");
		w_fld(ACR_CONTROL_1, 1, FS_HW_SW_SEL);
		w_fld(ACR_CONTROL_0, 0x2b, FS_VAL_SW);
	} else if (mode == HDMI2_AUD_FS_176K) {
		RX_AUDIO_LOG("[RX]hbr 705k\n");
		w_fld(ACR_CONTROL_1, 1, FS_HW_SW_SEL);
		w_fld(ACR_CONTROL_0, 0x0c, FS_VAL_SW);
	} else if (mode == HDMI2_AUD_FS_192K) {
		RX_AUDIO_LOG("[RX]hbr 768k\n");
		w_fld(ACR_CONTROL_1, 1, FS_HW_SW_SEL);
		w_fld(ACR_CONTROL_0, 0x0e, FS_VAL_SW);
	} else if (mode == HDMI2_AUD_FS_256K) {
		RX_AUDIO_LOG("[RX]hbr 1024k\n");
		w_fld(ACR_CONTROL_1, 1, FS_HW_SW_SEL);
		w_fld(ACR_CONTROL_0, 0x1b, FS_VAL_SW);
	} else if (mode == HDMI2_AUD_FS_3528K) {
		RX_AUDIO_LOG("[RX]hbr 1411k\n");
		w_fld(ACR_CONTROL_1, 1, FS_HW_SW_SEL);
		w_fld(ACR_CONTROL_0, 0x0d, FS_VAL_SW);
	} else if (mode == HDMI2_AUD_FS_384K) {
		RX_AUDIO_LOG("[RX]hbr 1536k\n");
		w_fld(ACR_CONTROL_1, 1, FS_HW_SW_SEL);
		w_fld(ACR_CONTROL_0, 0x05, FS_VAL_SW);
	} else {
		RX_AUDIO_LOG("[RX]hbr 768k\n");
		w_fld(ACR_CONTROL_1, 1, FS_HW_SW_SEL);
		w_fld(ACR_CONTROL_0, 0x0e, FS_VAL_SW);
	}
}

void
hdmi2com_aud_n_set(struct MTK_HDMI *myhdmi, u32 u4N)
{
	w_fld(RG_RX_N_SVAL1, 1, REG_N_HW_SW_SEL);

	w_fld(RG_RX_N_SVAL1, u4N & 0xff, REG_N_VAL_SW_7_0_);

	w_fld(RG_RX_N_HVAL2, (u4N >> 8), REG_N_VAL_SW_15_8_);
	w_fld(RG_RX_N_HVAL2, (u4N >> 16), REG_N_VAL_SW_19_16_);
}

void
hdmi2com_aud_cts_set(struct MTK_HDMI *myhdmi, u32 u4CTS)
{
	w_fld(RG_RX_N_SVAL1, 1, REG_CTS_HW_SW_SEL);
	w_fld(RG_RX_CTS_SVAL3, u4CTS & 0xff, REG_CTS_VAL_SW_7_0_);
	w_fld(RG_RX_CTS_SVAL3, (u4CTS >> 8), REG_CTS_VAL_SW_15_8_);
	w_fld(RG_RX_CTS_SVAL3, (u4CTS >> 16), REG_CTS_VAL_SW_19_16_);
}

void
hdmi2com_aud_fs_set(struct MTK_HDMI *myhdmi, u8 u4Fs)
{
	w_fld(RG_RX_N_SVAL1, 1, REG_FS_HW_SW_SEL);
	w_fld(RG_RX_N_SVAL1, u4Fs & 0x3f, REG_FS_VAL_SW);
}

bool
hdmi2com_aud_channel_status_changed(struct MTK_HDMI *myhdmi)
{
	bool flag;

	flag = r_fld(RX_INTR8, REG_INTR8_STAT7) ? TRUE : FALSE;
	if (flag) {
		/* write "1" to clean */
		w_reg(RX_INTR8,	Fld2Msk32(REG_INTR8_STAT7));
	}
	return flag;
}

bool
hdmi2com_is_hd_audio(struct MTK_HDMI *myhdmi)
{
	bool bRxHBR;

	bRxHBR = r_fld(RX_AUDP_STAT, HDMI_HBRA_ON) ? TRUE : FALSE;
	return bRxHBR;
}

bool
hdmi2com_is_dsd_audio(struct MTK_HDMI *myhdmi)
{
	bool bRxDSD;

	bRxDSD = r_fld(RX_AUDP_STAT, HDMI_AUD_DSD_ON) ? TRUE : FALSE;
	return bRxDSD;
}

bool
hdmi2com_is_audio_packet(struct MTK_HDMI *myhdmi)
{
	bool bRxAudPkt;

	bRxAudPkt = r_fld(RX_INTR4, REG_INTR2_STAT1) ? TRUE : FALSE;
	if (bRxAudPkt) {
		/* write "1" to clean if audio bit is set */
		w_reg(RX_INTR4,	Fld2Msk32(REG_INTR2_STAT1));
	}
	return bRxAudPkt;
}

bool
hdmi2com_is_multi_pcm_aud(struct MTK_HDMI *myhdmi)
{
	u8 bRxMultiCh;

	bRxMultiCh = r_fld(RX_AUDP_STAT, HDMI_LAYOUT);
	if (bRxMultiCh == 0)
		return FALSE;
	return TRUE;
}

u8
hdmi2com_aud_channel_num(struct MTK_HDMI *myhdmi)
{
	u8 temp;

	temp = r_fld(RG_RX_MUTE_DIV, AUD_CH_NUM1);
	return temp;
}

u8
hdmi2com_get_aud_fs(struct MTK_HDMI *myhdmi)
{
	u8 temp;

	temp = r_fld(RG_RX_CHST5, AUD_SAMPLE_F);
	return temp;
}

u8
hdmi2com_get_aud_valid_channel(struct MTK_HDMI *myhdmi)
{
	u8 temp;

	temp = r_fld(HDMI_STATUS_RO_RESVD, HEAD_SP);
	return temp & 0x0F;
}

void
hdmi2com_set_aud_valid_channel(struct MTK_HDMI *myhdmi, u8 u1ValidCh)
{
	u8 temp;

	temp = u1ValidCh >> 4;
	if (temp & 0x01)
		w_fld(RG_RX_AUDRX_CTRL, 1, REG_SD0_EN);
	else
		w_fld(RG_RX_AUDRX_CTRL, 0, REG_SD0_EN);

	if (temp & 0x02)
		w_fld(RG_RX_AUDRX_CTRL, 1, REG_SD1_EN);
	else
		w_fld(RG_RX_AUDRX_CTRL, 0, REG_SD1_EN);

	if (temp & 0x04)
		w_fld(RG_RX_AUDRX_CTRL, 1, REG_SD2_EN);
	else
		w_fld(RG_RX_AUDRX_CTRL, 0, REG_SD2_EN);

	if (temp & 0x08)
		w_fld(RG_RX_AUDRX_CTRL, 1, REG_SD3_EN);
	else
		w_fld(RG_RX_AUDRX_CTRL, 0, REG_SD3_EN);
}

void
hdmi2com_set_aud_mute_channel(struct MTK_HDMI *myhdmi, u8 u1MuteCh)
{
	if (u1MuteCh & 0x01) /* ch0 */
		w_fld(RG_RX_TDM_CTRL1, 1, REG_CH0_MUTE);
	else
		w_fld(RG_RX_TDM_CTRL1, 0, REG_CH0_MUTE);

	if (u1MuteCh & 0x02) /* ch1 */
		w_fld(RG_RX_TDM_CTRL1, 1, REG_CH1_MUTE);
	else
		w_fld(RG_RX_TDM_CTRL1, 0, REG_CH1_MUTE);

	if (u1MuteCh & 0x04) /* ch2 */
		w_fld(RG_RX_TDM_CTRL1, 1, REG_CH2_MUTE);
	else
		w_fld(RG_RX_TDM_CTRL1, 0, REG_CH2_MUTE);

	if (u1MuteCh & 0x08) /* ch3 */
		w_fld(RG_RX_TDM_CTRL1, 1, REG_CH3_MUTE);
	else
		w_fld(RG_RX_TDM_CTRL1, 0, REG_CH3_MUTE);
}

void
hdmi2com_set_aud_i2s_format(
	struct MTK_HDMI *myhdmi, u8 u1fmt, u8 u1Cycle)
{
	if (u1fmt == FORMAT_RJ) {
		w_fld(RG_RX_AUDRX_CTRL, 1, REG_WS);
		w_fld(RG_RX_AUDRX_CTRL, 1, REG_JUSTIFY);
		w_fld(RG_RX_AUDRX_CTRL, 1, REG_1ST_BIT);
	} else if (u1fmt == FORMAT_LJ) {
		w_fld(RG_RX_AUDRX_CTRL, 1, REG_WS);
		w_fld(RG_RX_AUDRX_CTRL, 0, REG_JUSTIFY);
		w_fld(RG_RX_AUDRX_CTRL, 1, REG_1ST_BIT);
	} else if (u1fmt == FORMAT_I2S) {
		w_fld(RG_RX_AUDRX_CTRL, 0, REG_WS);
		w_fld(RG_RX_AUDRX_CTRL, 0, REG_JUSTIFY);
		w_fld(RG_RX_AUDRX_CTRL, 0, REG_1ST_BIT);
	}

	if (u1Cycle == LRCK_CYC_16)
		w_fld(RG_RX_AUDRX_CTRL, 1, REG_SIZE);
	else if (u1Cycle == LRCK_CYC_32)
		w_fld(RG_RX_AUDRX_CTRL, 0, REG_SIZE);
}

void
hdmi2com_set_aud_i2s_lr_inv(struct MTK_HDMI *myhdmi, u8 u1LRInv)
{
	if (u1LRInv)
		w_fld(RG_RX_AUDRX_CTRL, 1, REG_WS);
	else
		w_fld(RG_RX_AUDRX_CTRL, 0, REG_WS);
}

void
hdmi2com_set_aud_i2s_lr_ck_edge(
	struct MTK_HDMI *myhdmi, u8 u1EdgeFmt)
{
	if (u1EdgeFmt == 0)
		w_fld(RG_RX_AUDRX_CTRL, 0, REG_CLK_EDGE);
	else
		w_fld(RG_RX_AUDRX_CTRL, 1, REG_CLK_EDGE);
}

void
hdmi2com_set_aud_i2s_mclk(struct MTK_HDMI *myhdmi, u8 u1MclkType)
{
	if (u1MclkType == MCLK_128FS) {
		w_fld(ACR_CONTROL_0, RX_SW_MCLK_128FS, FM_VAL_SW);
		w_fld(ACR_CONTROL_1, RX_SW_MCLK_128FS, FM_IN_VAL_SW);
	} else if (u1MclkType == MCLK_256FS) {
		w_fld(ACR_CONTROL_0, RX_SW_MCLK_256FS, FM_VAL_SW);
		w_fld(ACR_CONTROL_1, RX_SW_MCLK_256FS, FM_IN_VAL_SW);
	} else if (u1MclkType == MCLK_384FS) {
		w_fld(ACR_CONTROL_0, RX_SW_MCLK_384FS, FM_VAL_SW);
		w_fld(ACR_CONTROL_1, RX_SW_MCLK_384FS, FM_IN_VAL_SW);
	} else if (u1MclkType == MCLK_512FS) {
		w_fld(ACR_CONTROL_0, RX_SW_MCLK_512FS, FM_VAL_SW);
		w_fld(ACR_CONTROL_1, RX_SW_MCLK_512FS, FM_IN_VAL_SW);
	} else {
		RX_DEF_LOG("[RX]Set Audio Output MCLK Err\n");
	}
}

void
hdmi2com_set_aud_fs(struct MTK_HDMI *myhdmi, enum HDMI_RX_AUDIO_FS eFS)
{
	switch (eFS) {
	case SW_44p1K:
		w_fld(RG_RX_N_SVAL1, 0,	REG_FS_VAL_SW);
		w_fld(RG_RX_N_SVAL1, 1,	REG_FS_HW_SW_SEL);
		break;

	case SW_88p2K:
		w_fld(RG_RX_N_SVAL1, 8,	REG_FS_VAL_SW);
		w_fld(RG_RX_N_SVAL1, 1, REG_FS_HW_SW_SEL);
		break;

	case SW_176p4K:
		w_fld(RG_RX_N_SVAL1, 0x0C, REG_FS_VAL_SW);
		w_fld(RG_RX_N_SVAL1, 1, REG_FS_HW_SW_SEL);
		break;

	case SW_48K:
		w_fld(RG_RX_N_SVAL1, 0x02, REG_FS_VAL_SW);
		w_fld(RG_RX_N_SVAL1, 1,	REG_FS_HW_SW_SEL);
		break;

	case SW_96K:
		w_fld(RG_RX_N_SVAL1, 0x0a, REG_FS_VAL_SW);
		w_fld(RG_RX_N_SVAL1, 1,	REG_FS_HW_SW_SEL);
		break;

	case SW_192K:
		w_fld(RG_RX_N_SVAL1, 0x0e, REG_FS_VAL_SW);
		w_fld(RG_RX_N_SVAL1, 1, REG_FS_HW_SW_SEL);
		break;

	case SW_32K:
		w_fld(RG_RX_N_SVAL1, 0x03, REG_FS_VAL_SW);
		w_fld(RG_RX_N_SVAL1, 1,	REG_FS_HW_SW_SEL);
		break;

	case HW_FS:
		w_fld(RG_RX_N_SVAL1, 0,	REG_FS_HW_SW_SEL);
		break;

	default:
		break;
	}
}

u8
hdmi2com_get_aud_i2s_mclk(struct MTK_HDMI *myhdmi)
{
	u8 u1MclkType = MCLK_128FS;
	u32 temp = r_fld(ACR_CONTROL_0,
		FM_VAL_SW);

	switch (temp) {
	case RX_SW_MCLK_128FS:
		u1MclkType = MCLK_128FS;
		break;
	case RX_SW_MCLK_256FS:
		u1MclkType = MCLK_256FS;
		break;
	case RX_SW_MCLK_384FS:
		u1MclkType = MCLK_384FS;
		break;
	case RX_SW_MCLK_512FS:
		u1MclkType = MCLK_512FS;
		break;
	default:
		break;
	}

	return u1MclkType;
}

u8
hdmi2com_get_aud_ch0(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = r_fld(RG_RX_MUTE_DIV, CHNL_STATUS_BIT7_0);
	return temp;
}

u8
hdmi2com_get_aud_ch1(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = r_fld(RG_RX_MUTE_DIV, CHNL_STATUS_BIT15_8);
	return temp;
}

u8
hdmi2com_get_aud_ch2(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = r_fld(RG_RX_MUTE_DIV, CHNL_STATUS_BIT23_16);
	return temp;
}

u8
hdmi2com_get_aud_ch3(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = r_fld(RG_RX_CHST5, REG_CS_BIT31_24);
	return temp;
}

u8
hdmi2com_get_aud_ch4(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = r_fld(RG_RX_CHST5, REG_CS_BIT40_32);
	return temp;
}

u32
hdmi2com_get_aud_err(struct MTK_HDMI *myhdmi)
{
	u32 error = 0;

	if (r_fld(RX_INTR4, REG_INTR4_STAT0)) {
		error |= HDMI_AUDIO_FIFO_UNDERRUN;
		w_reg(RX_INTR4, Fld2Msk32(REG_INTR4_STAT0));
	}

	if (r_fld(RX_INTR4, REG_INTR4_STAT1)) {
		error |= HDMI_AUDIO_FIFO_OVERRUN;
		w_reg(RX_INTR4, Fld2Msk32(REG_INTR4_STAT1));
	}

	if (r_fld(RX_INTR8, REG_INTR5_STAT0)) {
		error |= HDMI_AUDIO_FS_CHG;
		w_reg(RX_INTR8,	Fld2Msk32(REG_INTR5_STAT0));
	}

	return error;
}

bool
hdmi2com_get_aud_layout(struct MTK_HDMI *myhdmi)
{
	return (bool)r_fld(RX_AUDP_STAT, HDMI_LAYOUT);
}

u8
hdmi2com_get_aud_fifo_status(struct MTK_HDMI *myhdmi)
{
	u8 u1Fifo = 0;

	if (r_fld(RX_INTR4, REG_INTR4_STAT0)) {
		u1Fifo |= HDMI_AUDIO_FIFO_UNDERRUN;
		w_reg(RX_INTR4,	Fld2Msk32(REG_INTR4_STAT0));
	}

	if (r_fld(RX_INTR4, REG_INTR4_STAT1)) {
		u1Fifo |= HDMI_AUDIO_FIFO_OVERRUN;
		w_reg(RX_INTR4, Fld2Msk32(REG_INTR4_STAT1));
	}

	if (u1Fifo == 0)
		u1Fifo = HDMI_AUDIO_FIFO_NORMAL;

	return u1Fifo;
}

void
hdmi2com_aud_apll_sel(struct MTK_HDMI *myhdmi, u8 u1On)
{
	w_fld(ACR_CONTROL_11, u1On, REG_APLL_STATUS_SEL);
}

void
hdmi2com_audio_fifo_reset(struct MTK_HDMI *myhdmi)
{
	w_fld(RX_PWD_CTRL, 1, REG_FIFO_RST);
	usleep_range(1000, 1050);
	w_fld(RX_PWD_CTRL, 0, REG_FIFO_RST);

	usleep_range(1000, 1050);
	/* Reset  Interrupt status : under-run / over-run / terc4  / hdcp */
	w_reg(RX_INTR4,	Fld2Msk32(REG_INTR4_STAT0));
	w_reg(RX_INTR4,	Fld2Msk32(REG_INTR4_STAT1));
	w_reg(RX_INTR8,	Fld2Msk32(REG_INTR5_STAT0));
}

bool
hdmi2com_get_aud_infoframe(
	struct MTK_HDMI *myhdmi, union Audio_InfoFrame *pAudioInfoFrame)
{
	u8 checksum;
	int i;

	if (pAudioInfoFrame == NULL)
		return ER_FAIL;

	pAudioInfoFrame->pktbyte.AUD_HB[0] =
		r_fld(AUDRX_CHSUM, CEA_AUD_HEADER_7_0_);
	pAudioInfoFrame->pktbyte.AUD_HB[1] =
		r_fld(AUDRX_CHSUM, CEA_AUD_HEADER_15_8_);
	pAudioInfoFrame->pktbyte.AUD_HB[2] =
		r_fld(AUDRX_CHSUM, CEA_AUD_LENGTH);
	checksum =
		r_fld(AUDRX_CHSUM, CEA_AUD_CHECKSUM);

	/* HDMIRX_AUD_INFOFRAME_DB1~BD10 */
	pAudioInfoFrame->pktbyte.AUD_DB[0] =
		r_fld(AUDRX_DBYTE4,	CEA_AUD_DBYTE1);
	pAudioInfoFrame->pktbyte.AUD_DB[1] =
		r_fld(AUDRX_DBYTE4,	CEA_AUD_DBYTE2);
	pAudioInfoFrame->pktbyte.AUD_DB[2] =
		r_fld(AUDRX_DBYTE4,	CEA_AUD_DBYTE3);
	pAudioInfoFrame->pktbyte.AUD_DB[3] =
		r_fld(AUDRX_DBYTE4,	CEA_AUD_DBYTE4);
	pAudioInfoFrame->pktbyte.AUD_DB[4] =
		r_fld(AUDRX_DBYTE8,	CEA_AUD_DBYTE5);
	pAudioInfoFrame->pktbyte.AUD_DB[5] =
		r_fld(AUDRX_DBYTE8,	CEA_AUD_DBYTE6);
	pAudioInfoFrame->pktbyte.AUD_DB[6] =
		r_fld(AUDRX_DBYTE8,	CEA_AUD_DBYTE7);
	pAudioInfoFrame->pktbyte.AUD_DB[7] =
		r_fld(AUDRX_DBYTE8,	CEA_AUD_DBYTE8);
	pAudioInfoFrame->pktbyte.AUD_DB[8] =
		r_fld(AUDRX_DBYTE12, CEA_AUD_DBYTE9);
	pAudioInfoFrame->pktbyte.AUD_DB[9] =
		r_fld(AUDRX_DBYTE12, CEA_AUD_DBYTE10);

	/* config info value */
	pAudioInfoFrame->info.Type = pAudioInfoFrame->pktbyte.AUD_HB[0];
	pAudioInfoFrame->info.Ver = pAudioInfoFrame->pktbyte.AUD_HB[1];
	pAudioInfoFrame->info.Len = pAudioInfoFrame->pktbyte.AUD_HB[2];
	pAudioInfoFrame->info.AudioChannelCount =
		pAudioInfoFrame->pktbyte.AUD_DB[0] & 0x07;

	pAudioInfoFrame->info.RSVD1 = 0;
	pAudioInfoFrame->info.AudioCodingType =
		(pAudioInfoFrame->pktbyte.AUD_DB[0] >> 4) & 0x0f;
	pAudioInfoFrame->info.SampleSize =
		pAudioInfoFrame->pktbyte.AUD_DB[1] & 0x03;
	pAudioInfoFrame->info.SampleFreq =
		(pAudioInfoFrame->pktbyte.AUD_DB[1] >> 2) & 0x07;
	pAudioInfoFrame->info.Rsvd2 = 0;
	pAudioInfoFrame->info.FmtCoding =
		(pAudioInfoFrame->pktbyte.AUD_DB[0] >> 4) & 0x0f;
	pAudioInfoFrame->info.SpeakerPlacement =
		pAudioInfoFrame->pktbyte.AUD_DB[3];
	pAudioInfoFrame->info.Rsvd3 = 0;
	pAudioInfoFrame->info.LevelShiftValue =
		(pAudioInfoFrame->pktbyte.AUD_DB[4] >> 3) & 0x0f;
	pAudioInfoFrame->info.DM_INH =
		(pAudioInfoFrame->pktbyte.AUD_DB[4] >> 7) & 0x01;

	for (i = 0; i < 3; i++)
		checksum += pAudioInfoFrame->pktbyte.AUD_HB[i];
	for (i = 0; i < 5; i++) /* only 5 bytes is valued */
		checksum += pAudioInfoFrame->pktbyte.AUD_DB[i];

	if (checksum == 0)
		return 0;
	else
		return 1;
}

enum HdmiRxDepth
hdmi2com_get_deep_status(struct MTK_HDMI *myhdmi)
{
	return (enum HdmiRxDepth)
		(r_fld(RX_AUDP_FIFO, RO_COLOR_DEPTH) & 0x3);
}

u32
hdmi2com_xclk_of_pdt_clk(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = r_fld(RG_RX_DEBUG_CLK_XPCNT, AAC_XCLK_IN_DEBUG_CLK_23_0_);
	return temp;
}

u32
hdmi2com_get_ref_clk(struct MTK_HDMI *myhdmi)
{
	u32 u4Val = 0;
	u32 u4CpuBusClock;

	u4Val = hdmi2com_xclk_of_pdt_clk(myhdmi);
	if (u4Val == 0) {
		RX_DEF_LOG("[RX]invalid clock\n");
		return u4Val;
	}

	u4CpuBusClock = hdmi2com_bus_clk_freq(myhdmi);
	u4CpuBusClock = u4CpuBusClock / 1000;
	u4Val = (2048 * (u4CpuBusClock)) / u4Val;
	return u4Val;
}

bool
hdmi2com_is_valid_packet(struct MTK_HDMI *myhdmi, u8 *pduf1,
	u8 *pduf2, u8 len)
{
	u8 i;

	for (i = 0; i < len; i++) {
		if (pduf1[i] != pduf2[i])
			return FALSE;
	}

	return TRUE;
}

bool
hdmi2com_save_packet(struct MTK_HDMI *myhdmi, u8 *p_to,
	u8 *p_from, u8 len)
{
	u8 i;
	bool is_not_same = FALSE;

	for (i = 0; i < len; i++) {
		if (p_to[i] != p_from[i])
			is_not_same = TRUE;
		p_to[i] = p_from[i];
	}

	return is_not_same;
}

void
hdmi2com_get_packet(struct MTK_HDMI *myhdmi, u8 Type,
	u8 *pduf, u8 len)
{
	void __iomem *u1StartAddr = 0;
	u8 i = 0, m = 0, n = 0, k = 0, u1Len = 0;

	switch (Type) {
	case HDMI_INFO_FRAME_ID_AVI:
		u1StartAddr = AVIRX_VERS + 2;
		for (i = 0; i < len; i++)
			pduf[i] = r_reg_1b(u1StartAddr + i);
		break;
	case HDMI_INFO_FRAME_ID_SPD:
		u1StartAddr = SPDRX_CHSUM;
		for (i = 0; i < len; i++)
			pduf[i] = r_reg_1b(u1StartAddr + i);
		break;
	case HDMI_INFO_FRAME_ID_AUD:
		u1StartAddr = AUDRX_CHSUM;
		for (i = 0; i < len; i++)
			pduf[i] = r_reg_1b(u1StartAddr + i);
		break;
	case HDMI_INFO_FRAME_ID_MPEG:
		u1StartAddr = MPEGRX_CHSUM;
		for (i = 0; i < len; i++)
			pduf[i] = r_reg_1b(u1StartAddr + i);
		break;
	case HDMI_INFO_FRAME_ID_UNREC:
		u1StartAddr = RX_UNREC_BYTE4;
		for (i = 0; i < len; i++)
			pduf[i] = r_reg_1b(u1StartAddr + i);
		break;
	case HDMI_INFO_FRAME_ID_ACP:
		u1StartAddr = RX_ACP_BYTE4;
		for (i = 0; i < len; i++)
			pduf[i] = r_reg_1b(u1StartAddr + i);
		break;
	case HDMI_INFO_FRAME_ID_VSI:
		u1StartAddr = VSIRX_LENGTH + 1;
		for (i = 0; i < len; i++)
			pduf[i] = r_reg_1b(u1StartAddr + i);
		break;
	case HDMI_INFO_FRAME_ID_DVVSI:
		//read dolby vsi
		for (m = 0; m < 3; m++)
			pduf[m] = r_reg_1b(HEADER_VER2_HEADER_BYTE2 + 1 + m);
		u1Len = (pduf[2] & 0x1f) + 1;
		for (m = 0; m < u1Len; m++) {
			n = m / 4;
			k = m % 4;
			pduf[m+3] = r_reg_1b(HEADER_VER2_DBYTE3 + k - n * 4);
		}
		break;
	case HDMI_INFO_FRAME_ID_METADATA:
		u1StartAddr = METADATA_HEADER_BYTE2;
		for (i = 0; i < len; i++)
			pduf[i] = r_reg_1b(u1StartAddr + i);
		break;
	case HDMI_INFO_FRAME_ID_ISRC0:
		u1StartAddr = RX_ISRC1_LENGTH;
		for (i = 0; i < len; i++) {
			pduf[i] = r_reg_1b(u1StartAddr + i);
			if (i == 2)
				u1StartAddr = RX_ISRC1_LENGTH + 1;
		}
		break;
	case HDMI_INFO_FRAME_ID_ISRC1:
		u1StartAddr = RX_ISRC2_LENGTH;
		for (i = 0; i < len; i++) {
			pduf[i] = r_reg_1b(u1StartAddr + i);
			if (i == 2)
				u1StartAddr = RX_ISRC2_LENGTH + 1;
		}
		break;
	case HDMI_INFO_FRAME_ID_GCP:
		u1StartAddr = RX_GCP_LENGTH;
		for (i = 0; i < len; i++) {
			pduf[i] = r_reg_1b(u1StartAddr + i);
			if (i == 2)
				u1StartAddr = RX_GCP_LENGTH + 1;
		}
		break;
	case HDMI_INFO_FRAME_ID_HFVSI:
		u1StartAddr = HF_VSIRX_LENGTH;
		for (i = 0; i < len; i++) {
			pduf[i] = r_reg_1b(u1StartAddr + i);
			if (i == 2)
				u1StartAddr = HF_VSIRX_LENGTH + 1;
		}
		break;
	case HDMI_INFO_FRAME_ID_86:
		u1StartAddr = HEADER_86_HEADER_BYTE2;
		for (i = 0; i < len; i++) {
			pduf[i] = r_reg_1b(u1StartAddr + i);
			if (i == 2)
				u1StartAddr = HEADER_86_HEADER_BYTE2 + 1;
		}
		break;
	case HDMI_INFO_FRAME_ID_87:
		u1StartAddr = HEADER_87_HEADER_BYTE2;
		for (i = 0; i < len; i++) {
			pduf[i] = r_reg_1b(u1StartAddr + i);
			if (i == 2)
				u1StartAddr = HEADER_87_HEADER_BYTE2 + 1;
		}
		break;
	default:
		break;
	}
}

void
hdmi2com_info(struct MTK_HDMI *myhdmi, u8 Type)
{
	struct HdmiInfo *hdmi_rx_packet;
	u8 bBuffer1[31] = {0};
	u8 bBuffer2[31] = {0};

	switch (Type) {
	case HDMI_INFO_FRAME_ID_AVI:
		hdmi_rx_packet = &(myhdmi->AviInfo);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_AVI;
		hdmi_rx_packet->bLength = 19;
		break;
	case HDMI_INFO_FRAME_ID_SPD:
		hdmi_rx_packet = &(myhdmi->SpdInfo);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_SPD;
		hdmi_rx_packet->bLength = 31;
		break;
	case HDMI_INFO_FRAME_ID_AUD:
		hdmi_rx_packet = &(myhdmi->AudInfo);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_AUD;
		hdmi_rx_packet->bLength = 31;
		break;
	case HDMI_INFO_FRAME_ID_MPEG:
		hdmi_rx_packet = &(myhdmi->MpegInfo);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_MPEG;
		hdmi_rx_packet->bLength = 31;
		break;
	case HDMI_INFO_FRAME_ID_UNREC:
		hdmi_rx_packet = &(myhdmi->UnrecInfo);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_UNREC;
		hdmi_rx_packet->bLength = 31;
		break;
	case HDMI_INFO_FRAME_ID_ACP:
		hdmi_rx_packet = &(myhdmi->AcpInfo);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_ACP;
		hdmi_rx_packet->bLength = 31;
		break;
	case HDMI_INFO_FRAME_ID_VSI:
		hdmi_rx_packet = &(myhdmi->VsiInfo);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_VSI;
		hdmi_rx_packet->bLength = 31;
		break;
	case HDMI_INFO_FRAME_ID_DVVSI:
		hdmi_rx_packet = &(myhdmi->DVVSIInfo);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_DVVSI;
		hdmi_rx_packet->bLength = 31;
		break;
	case HDMI_INFO_FRAME_ID_METADATA:
		hdmi_rx_packet = &(myhdmi->MetaInfo);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_METADATA;
		hdmi_rx_packet->bLength = 31;
		break;
	case HDMI_INFO_FRAME_ID_ISRC0:
		hdmi_rx_packet = &(myhdmi->Isrc0Info);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_ISRC0;
		hdmi_rx_packet->bLength = 19;
		break;
	case HDMI_INFO_FRAME_ID_ISRC1:
		hdmi_rx_packet = &(myhdmi->Isrc1Info);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_ISRC1;
		hdmi_rx_packet->bLength = 19;
		break;
	case HDMI_INFO_FRAME_ID_GCP:
		hdmi_rx_packet = &(myhdmi->GcpInfo);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_GCP;
		hdmi_rx_packet->bLength = 10;
		break;
	case HDMI_INFO_FRAME_ID_HFVSI:
		hdmi_rx_packet = &(myhdmi->HfVsiInfo);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_HFVSI;
		hdmi_rx_packet->bLength = 31;
		break;
	case HDMI_INFO_FRAME_ID_86:
		hdmi_rx_packet = &(myhdmi->Info86);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_86;
		hdmi_rx_packet->bLength = 31;
		break;
	case HDMI_INFO_FRAME_ID_87:
		hdmi_rx_packet = &(myhdmi->Info87);
		hdmi_rx_packet->u1InfoID = HDMI_INFO_FRAME_ID_87;
		hdmi_rx_packet->bLength = 31;
		break;

	default:
		RX_DEF_LOG("[RX]invalid packet type\n");
		return;
	}

	hdmi2com_get_packet(myhdmi, hdmi_rx_packet->u1InfoID, bBuffer1,
		hdmi_rx_packet->bLength);
	hdmi2com_get_packet(myhdmi, hdmi_rx_packet->u1InfoID, bBuffer2,
		hdmi_rx_packet->bLength);

	if (hdmi2com_is_valid_packet(myhdmi, bBuffer1, bBuffer2,
		    hdmi_rx_packet->bLength) == FALSE) {
		RX_DEF_LOG("[RX]ignore invalid %d pkt\n",
			hdmi_rx_packet->u1InfoID);
		return;
	}

	hdmi_rx_packet->fgValid = TRUE;

	if (hdmi2com_save_packet(myhdmi,
		&(hdmi_rx_packet->u1InfoData[0]),
		bBuffer1, hdmi_rx_packet->bLength)) {
		hdmi_rx_packet->fgChanged = TRUE;
		RX_INFO_LOG("[RX]%x ptk changed\n",
			hdmi_rx_packet->u1InfoData[0]);
	}
}

void
hdmi2com_clear_info_header(struct MTK_HDMI *myhdmi, u8 Type)
{
	struct HdmiInfo *hdmi_rx_packet = NULL;

	switch (Type) {
	case HDMI_INFO_FRAME_ID_AVI:
		hdmi_rx_packet = &(myhdmi->AviInfo);
		break;
	case HDMI_INFO_FRAME_ID_SPD:
		hdmi_rx_packet = &(myhdmi->SpdInfo);
		break;
	case HDMI_INFO_FRAME_ID_AUD:
		hdmi_rx_packet = &(myhdmi->AudInfo);
		break;
	case HDMI_INFO_FRAME_ID_MPEG:
		hdmi_rx_packet = &(myhdmi->MpegInfo);
		break;
	case HDMI_INFO_FRAME_ID_UNREC:
		hdmi_rx_packet = &(myhdmi->UnrecInfo);
		break;
	case HDMI_INFO_FRAME_ID_ACP:
		hdmi_rx_packet = &(myhdmi->AcpInfo);
		break;
	case HDMI_INFO_FRAME_ID_VSI:
		hdmi_rx_packet = &(myhdmi->VsiInfo);
		break;
	case HDMI_INFO_FRAME_ID_DVVSI:
		hdmi_rx_packet = &(myhdmi->DVVSIInfo);
		break;
	case HDMI_INFO_FRAME_ID_METADATA:
		hdmi_rx_packet = &(myhdmi->MetaInfo);
		break;
	case HDMI_INFO_FRAME_ID_ISRC0:
		hdmi_rx_packet = &(myhdmi->Isrc0Info);
		break;
	case HDMI_INFO_FRAME_ID_ISRC1:
		hdmi_rx_packet = &(myhdmi->Isrc1Info);
		break;
	case HDMI_INFO_FRAME_ID_GCP:
		hdmi_rx_packet = &(myhdmi->GcpInfo);
		break;
	case HDMI_INFO_FRAME_ID_HFVSI:
		hdmi_rx_packet = &(myhdmi->HfVsiInfo);
		break;
	case HDMI_INFO_FRAME_ID_86:
		hdmi_rx_packet = &(myhdmi->Info86);
		break;
	case HDMI_INFO_FRAME_ID_87:
		hdmi_rx_packet = &(myhdmi->Info87);
		break;

	default:
		RX_DEF_LOG("[RX]invalid packet type\n");
		return;
	}

	if (hdmi_rx_packet->u1InfoData[0]) {
		RX_DEF_LOG("[RX]clear ptk: 0x%2x\n",
			hdmi_rx_packet->u1InfoData[0]);
		hdmi_rx_packet->u1InfoData[0] = 0;
	}
}

u8
hdmi2com_info_header(struct MTK_HDMI *myhdmi, u8 Type)
{
	void __iomem *u1StartAddr;
	u8 bBuffer = 0;

	switch (Type) {
	case HDMI_INFO_FRAME_ID_AVI:
		u1StartAddr = AVIRX_VERS + 2;
		break;
	case HDMI_INFO_FRAME_ID_SPD:
		u1StartAddr = SPDRX_CHSUM;
		break;
	case HDMI_INFO_FRAME_ID_AUD:
		u1StartAddr = AUDRX_CHSUM;
		break;
	case HDMI_INFO_FRAME_ID_MPEG:
		u1StartAddr = MPEGRX_CHSUM;
		break;
	case HDMI_INFO_FRAME_ID_UNREC:
		u1StartAddr = RX_UNREC_BYTE4;
		break;
	case HDMI_INFO_FRAME_ID_ACP:
		u1StartAddr = RX_ACP_BYTE4;
		break;
	case HDMI_INFO_FRAME_ID_VSI:
		u1StartAddr = VSIRX_LENGTH + 1;
		break;
	case HDMI_INFO_FRAME_ID_DVVSI:
		u1StartAddr = HEADER_VER2_HEADER_BYTE2 + 1;
		break;
	case HDMI_INFO_FRAME_ID_METADATA:
		u1StartAddr = METADATA_HEADER_BYTE2;
		break;
	case HDMI_INFO_FRAME_ID_ISRC0:
		u1StartAddr = RX_ISRC1_LENGTH;
		break;
	case HDMI_INFO_FRAME_ID_ISRC1:
		u1StartAddr = RX_ISRC2_LENGTH;
		break;
	case HDMI_INFO_FRAME_ID_GCP:
		u1StartAddr = RX_GCP_LENGTH;
		break;
	case HDMI_INFO_FRAME_ID_HFVSI:
		u1StartAddr = HF_VSIRX_LENGTH;
		break;
	case HDMI_INFO_FRAME_ID_86:
		u1StartAddr = HEADER_86_HEADER_BYTE2;
		break;
	case HDMI_INFO_FRAME_ID_87:
		u1StartAddr = HEADER_87_HEADER_BYTE2;
		break;
	default:
		return 0xff;
	}

	bBuffer = r_reg_1b(u1StartAddr);

	return bBuffer;
}

struct HdmiVideoInfo_t
hdmi2com_avi_infoframe_parser(struct MTK_HDMI *myhdmi)
{
	u8 u1Colorimetry;
	u8 u1ExtColorrimetry;
	u8 u1ColorInfo;
	u8 u1Quant;
	u8 u1Yquant;

	struct HdmiVideoInfo_t vInfo;

	memset(&vInfo, 0, sizeof(vInfo));

	u1Colorimetry = (r_reg_1b(AVIRX_DBYTE2 + 3) & 0xC0) >> 6;
	u1ExtColorrimetry = (r_reg_1b(AVIRX_DBYTE6) & 0x70) >> 4;
	u1ColorInfo = (r_reg_1b(AVIRX_DBYTE2 + 2) & 0x60) |
		(u1Colorimetry << 3) |	u1ExtColorrimetry;
	u1Quant = (r_reg_1b(AVIRX_DBYTE6) & 0x06) >> 2;
	u1Yquant = (r_reg_1b(AVIRX_DBYTE6 + 2) & 0xc0) >> 6;

	switch (u1ColorInfo) {
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		vInfo.clrSpc = HDMI_RX_CLRSPC_RGB;
		break;
	case 0x1C:
		vInfo.clrSpc = HDMI_RX_CLRSPC_Adobe_RGB;
		break;
	case 0x1E:
		vInfo.clrSpc = HDMI_RX_CLRSPC_BT_2020_RGB_non_const_luminous;
		break;
	case 0x20:
	case 0x21:
	case 0x22:
	case 0x23:
	case 0x24:
	case 0x25:
	case 0x26:
	case 0x27:
	case 0x28:
	case 0x29:
	case 0x2a:
	case 0x2b:
	case 0x2c:
	case 0x2d:
	case 0x2e:
	case 0x2f:
		vInfo.clrSpc = HDMI_RX_CLRSPC_YC422_601;
		break;
	case 0x30:
	case 0x31:
	case 0x32:
	case 0x33:
	case 0x34:
	case 0x35:
	case 0x36:
	case 0x37:
		vInfo.clrSpc = HDMI_RX_CLRSPC_YC422_709;
		break;
	case 0x38:
		vInfo.clrSpc = HDMI_RX_CLRSPC_XVYC422_601;
		break;
	case 0x39:
		vInfo.clrSpc = HDMI_RX_CLRSPC_XVYC422_709;
		break;
	case 0x3A:
		vInfo.clrSpc = HDMI_RX_CLRSPC_sYCC422_601;
		break;
	case 0x3B:
		vInfo.clrSpc = HDMI_RX_CLRSPC_Adobe_YCC422_601;
		break;
	case 0x3D:
		vInfo.clrSpc = HDMI_RX_CLRSPC_BT_2020_YCC422_const_luminous;
		break;
	case 0x3E:
		vInfo.clrSpc = HDMI_RX_CLRSPC_BT_2020_YCC422_non_const_luminous;
		break;
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x44:
	case 0x45:
	case 0x46:
	case 0x47:
	case 0x48:
	case 0x49:
	case 0x4a:
	case 0x4b:
	case 0x4c:
	case 0x4d:
	case 0x4e:
	case 0x4f:
		vInfo.clrSpc = HDMI_RX_CLRSPC_YC444_601;
		break;
	case 0x50:
	case 0x51:
	case 0x52:
	case 0x53:
	case 0x54:
	case 0x55:
	case 0x56:
	case 0x57:
		vInfo.clrSpc = HDMI_RX_CLRSPC_YC444_709;
		break;
	case 0x58:
		vInfo.clrSpc = HDMI_RX_CLRSPC_XVYC444_601;
		break;
	case 0x59:
		vInfo.clrSpc = HDMI_RX_CLRSPC_XVYC444_709;
		break;
	case 0x5A:
		vInfo.clrSpc = HDMI_RX_CLRSPC_sYCC444_601;
		break;
	case 0x5B:
		vInfo.clrSpc = HDMI_RX_CLRSPC_Adobe_YCC444_601;
		break;
	case 0x5D:
		vInfo.clrSpc = HDMI_RX_CLRSPC_BT_2020_YCC444_const_luminous;
		break;
	case 0x5E:
		vInfo.clrSpc = HDMI_RX_CLRSPC_BT_2020_YCC444_non_const_luminous;
		break;
	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x64:
	case 0x65:
	case 0x66:
	case 0x67:
	case 0x68:
	case 0x69:
	case 0x6a:
	case 0x6b:
	case 0x6c:
	case 0x6d:
	case 0x6e:
	case 0x6f:
		vInfo.clrSpc = HDMI_RX_CLRSPC_YC420_601;
		break;
	case 0x70:
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x77:
		vInfo.clrSpc = HDMI_RX_CLRSPC_YC420_709;
		break;
	case 0x78:
		vInfo.clrSpc = HDMI_RX_CLRSPC_XVYC420_601;
		break;
	case 0x79:
		vInfo.clrSpc = HDMI_RX_CLRSPC_XVYC420_709;
		break;
	case 0x7A:
		vInfo.clrSpc = HDMI_RX_CLRSPC_sYCC420_601;
		break;
	case 0x7B:
		vInfo.clrSpc = HDMI_RX_CLRSPC_Adobe_YCC420_601;
		break;
	case 0x7D:
		vInfo.clrSpc = HDMI_RX_CLRSPC_BT_2020_YCC420_const_luminous;
		break;
	case 0x7E:
		vInfo.clrSpc = HDMI_RX_CLRSPC_BT_2020_YCC420_non_const_luminous;
		break;
	default:
		vInfo.clrSpc = HDMI_RX_CLRSPC_RGB;
		break;
	}

	if (u1Quant == 1)
		vInfo.range = HDMI_RX_RGB_LIMT;
	else if (u1Quant == 2)
		vInfo.range = HDMI_RX_RGB_FULL;

	if (u1Yquant == 1)
		vInfo.range = HDMI_RX_YCC_LIMT;
	else if (u1Yquant == 2)
		vInfo.range = HDMI_RX_YCC_FULL;

	return vInfo;
}

void
hdmi2com_hf_infoframe_parser(struct MTK_HDMI *myhdmi)
{
}

void
hdmi2com_vsi_infoframe_parser(struct MTK_HDMI *myhdmi)
{
}

u8
bHDMI2ComGetBitsFromByte(u8 u1Byte, u8 u1Bits, u8 u1BitIndex)
{
	u8 Mask = (0xff >> (8 - u1Bits)) << (u1BitIndex - u1Bits);

	return (u1Byte & Mask) >> (u1BitIndex - u1Bits);
}

u32
dwHDMI2ComGetBits(struct HDMI_HDR_EMP_MD_T *prHdmi_DyMdPKT, u8 u1Bits)
{
	u8 Bytes = 0;
	u8 u1temp;
	u8 i;
	u32 u4Result;
	u8 ByteIndex = 0;
	u8 u1TmpBits = 0;

	if (prHdmi_DyMdPKT == NULL)
		return 0xffffffff;

	ByteIndex = prHdmi_DyMdPKT->u1ByteIndex;

	if (prHdmi_DyMdPKT->u1BitIndex >= u1Bits) {
		Bytes = 1;
		u4Result = bHDMI2ComGetBitsFromByte(
			prHdmi_DyMdPKT->au1DyMdPKT[ByteIndex],
			u1Bits,
			prHdmi_DyMdPKT->u1BitIndex);
		if (prHdmi_DyMdPKT->u1BitIndex == u1Bits) {
			prHdmi_DyMdPKT->u1BitIndex = 8;
			prHdmi_DyMdPKT->u1ByteIndex =
				prHdmi_DyMdPKT->u1ByteIndex + 1;
		} else {
			prHdmi_DyMdPKT->u1BitIndex =
				prHdmi_DyMdPKT->u1BitIndex - u1Bits;
		}
	} else {
		u1temp = (u1Bits-prHdmi_DyMdPKT->u1BitIndex) / 8;
		Bytes = ((u1Bits - prHdmi_DyMdPKT->u1BitIndex) % 8) ?
			u1temp + 2 :
			u1temp + 1;
		u4Result = bHDMI2ComGetBitsFromByte(
			prHdmi_DyMdPKT->au1DyMdPKT[ByteIndex],
			prHdmi_DyMdPKT->u1BitIndex,
			prHdmi_DyMdPKT->u1BitIndex);
		u1TmpBits = u1TmpBits+prHdmi_DyMdPKT->u1BitIndex;
		prHdmi_DyMdPKT->u1BitIndex = 8;
		ByteIndex = ByteIndex + 1;
		for (i = 1; i < Bytes - 1; i++) {
			u4Result = (u4Result<<8) |
				bHDMI2ComGetBitsFromByte(
					prHdmi_DyMdPKT->au1DyMdPKT[ByteIndex],
					8,
					8);
			u1TmpBits += 8;
			ByteIndex++;
		}
		u4Result = (u4Result << (u1Bits-u1TmpBits)) |
			bHDMI2ComGetBitsFromByte(
				prHdmi_DyMdPKT->au1DyMdPKT[ByteIndex],
				u1Bits - u1TmpBits,
				8);
		if ((u1Bits - u1TmpBits) == 8) {
			prHdmi_DyMdPKT->u1BitIndex = 8;
			ByteIndex = ByteIndex + 1;
		} else {
			prHdmi_DyMdPKT->u1BitIndex = 8 - (u1Bits - u1TmpBits);
		}
		prHdmi_DyMdPKT->u1ByteIndex = ByteIndex;
	}

	return u4Result;
}

void
vHDMI2ComHDR10PlusVsiEn(struct MTK_HDMI *myhdmi)
{
	w_fld(VER1_VSI_CTRL, 0x1, VER1_PACKET_FOR_NORMAL_USE);
	w_fld(VER1_VSI_CTRL, 0x1, NEW_VER1_VSI_ONLY);
	w_fld(VER1_VSI_CTRL, 0x1, VER1_VSI_IEEE_ID_CHK_EN);
	w_fld(VER1_VSI_CTRL, 0x90848B, VER1_PB_IEEE_ID);
	w_fld(INTR_MASK_RESVD, 0x1, INTR_CEA_VER1_VSI_NEW_MASK);
}

void
vHDMI2ComGetHDR10PlusVsiMetadata(struct MTK_HDMI *myhdmi,
	struct HdmiInfo *prVsiMetadata)
{
	u8 i;
	u8 u1Len;
	void __iomem *u4Address;
	u8 *pu1InfoData = prVsiMetadata->u1InfoData;

	if (!prVsiMetadata)
		return;

	for (i = 0; i < 3; i++) {
		u4Address = HEADER_VER1_HEADER_BYTE2 + 1;
		*pu1InfoData++ = r_reg_1b(u4Address + i);
	}
	u1Len = (prVsiMetadata->u1InfoData[2] & 0x1f) + 1;
	u1Len = u1Len < 29 ? u1Len : 29; //buffer len => 32, minus 3 => 29
	for (i = 0; i < u1Len; i++) {
		u4Address = HEADER_VER1_DBYTE3;
		*pu1InfoData++ = r_reg_1b(u4Address - (i & 0xfc) + (i & 0x3));
	}
}

void
vHDMI2ComDynamicMetaDataEn(struct MTK_HDMI *myhdmi, u8 u1En)
{
	w_reg(INTR_STATUS_RESVD, Fld2Msk32(INTR_MTW_PULSE));
	w_fld(INTR_MASK_RESVD, u1En, INTR_MTW_PULSE_MASK);
}

void
vHDMI2ComDynamicMetaDataClrInt(struct MTK_HDMI *myhdmi, u8 u1Fifo)
{
	if (u1Fifo)
		w_reg(INTR_STATUS_RESVD, Fld2Msk32(INTR_FIFO1_READY));
	else
		w_reg(INTR_STATUS_RESVD, Fld2Msk32(INTR_FIFO0_READY));
}

void
vHDMI2ComGetDynamicMetadata(struct MTK_HDMI *myhdmi,
	u8 u1Len,
	struct HDMI_HDR_EMP_MD_T *prDynamicMetadata)
{
	u16 i, j;
	u8 u1LenMSB, u1LenLSB;
	u8 *pu1DyMdPkt = prDynamicMetadata->au1DyMdPKT;

	if (prDynamicMetadata == NULL)
		return;

	for (i = 0; i < u1Len; i++) {
		r_reg(DYNAMIC_HDR_CTRL1);
		for (j = 0; j < DYNAMIC_METADATA_PKT_LEN; j++)
			*pu1DyMdPkt++ = r_reg_1b(DYNAMIC_HDR_FIFO_DATA0 + j);
	}
	// calc data set length
	u1LenMSB = prDynamicMetadata->au1DyMdPKT[5];
	u1LenLSB = prDynamicMetadata->au1DyMdPKT[6];
	prDynamicMetadata->u2Length = (u1LenMSB << 8) | u1LenLSB;
}

void
hdmi2com_set_scdc_en(struct MTK_HDMI *myhdmi, u8 u1En)
{
	w_fld(SCDC_REG00, u1En, RGS_DDC_EN);
}

void hdmi2com_vrr_emp(struct MTK_HDMI *myhdmi)
{
	myhdmi->vrr_emp.EmpData[0] = r_reg(EMP_VRR_PACKET1_D0);
	myhdmi->vrr_emp.EmpData[1] = r_reg(EMP_VRR_PACKET1_D1);
	myhdmi->vrr_emp.EmpData[2] = r_reg(EMP_VRR_PACKET1_D2);
	myhdmi->vrr_emp.EmpData[3] = r_reg(EMP_VRR_PACKET1_D3);
	myhdmi->vrr_emp.EmpData[4] = r_reg(EMP_VRR_PACKET1_D4);
	myhdmi->vrr_emp.EmpData[5] = r_reg(EMP_VRR_PACKET1_D5);
	myhdmi->vrr_emp.EmpData[6] = r_reg(EMP_VRR_PACKET1_D6);
	myhdmi->vrr_emp.EmpData[7] = r_reg(EMP_VRR_PACKET1_D7);
}

void
hdmi2com_scdc_status_clr(struct MTK_HDMI *myhdmi)
{
	w_fld(SCDC_REG10, 0, RGS_PWR5V);
	w_fld(SCDC_REG10, 0, RGS_HPD_IN);

	hal_delay_2us(1);

	w_fld(SCDC_REG10, 1, RGS_PWR5V);
	w_fld(SCDC_REG10, 1, RGS_HPD_IN);
}

void
hdmi2com_force_scrambing_on_by_sw(struct MTK_HDMI *myhdmi, u8 u1En)
{
	w_fld(DIG_PHY_CONTROL_2, 0, HW_SCRAMBLING_ON_EN);
	w_fld(DIG_PHY_CONTROL_2, u1En, REG_SCRAMBLING_ON);
}

void
hdmi2com_set_420_output(struct MTK_HDMI *myhdmi, u8 u1En)
{
	if (u1En) {
		w_fld(INPUT_MAPPING, 1, MUX_420_ENABLE);
		w_fld(INPUT_MAPPING, 0, SELECT_Y);
		w_fld(INPUT_MAPPING, 1, SELECT_CB);
		w_fld(INPUT_MAPPING, 2, SELECT_CR);
	} else {
		w_fld(INPUT_MAPPING, 0, MUX_420_ENABLE);
		w_fld(INPUT_MAPPING, 0, SELECT_Y);
		w_fld(INPUT_MAPPING, 1, SELECT_CB);
		w_fld(INPUT_MAPPING, 2, SELECT_CR);
	}
}

void
hdmi2com_set_420_to_422(struct MTK_HDMI *myhdmi, u8 u1En)
{
	w_fld(C420_C422_CONFIG, u1En, C420_C422_ENABLE);
}

void
hdmi2com_sherman_reset(struct MTK_HDMI *myhdmi)
{
	w_fld(SHERMAN_RST, 0, REG_SHERMAN_RSTB);
	hal_delay_2us(1);
	w_fld(SHERMAN_RST, 1, REG_SHERMAN_RSTB);
}

void hdmi2com_clr_int(struct MTK_HDMI *myhdmi)
{
	w_fld(SHERMAN_RST, 1, REG_SHERMAN_INT_CLR);
	w_fld(SHERMAN_RST, 0, REG_SHERMAN_INT_CLR);
}

void
hdmi2com_sw_reset(struct MTK_HDMI *myhdmi)
{
	w_fld(RX_PWD_CTRL, 1, REG_HD20_SW_RST);
	hal_delay_2us(1);
	w_fld(RX_PWD_CTRL, 0, REG_HD20_SW_RST);
}

void
hdmi2com_set_422_to_444(struct MTK_HDMI *myhdmi, u8 u1En)
{
	if (u1En == 1) {
		w_fld(C422_C444_CONFIG, 1, USE_CB_OR_CR);
		w_fld(C422_C444_CONFIG, 1, C422_C444_ENABLE);
	} else {
		w_fld(C422_C444_CONFIG, 0, USE_CB_OR_CR);
		w_fld(C422_C444_CONFIG, 0, C422_C444_ENABLE);
	}
}

void
hdmi2com_set_hv_sync_pol(struct MTK_HDMI *myhdmi, bool is_hd)
{
	if (is_hd) {
		w_fld(OUTPUT_SYNC_CONFIG, 0, VSYNC_POLARITY_OUTPUT_SYNC);
		w_fld(OUTPUT_SYNC_CONFIG, 0, HSYNC_POLARITY_OUTPUT_SYNC);
	} else {
		w_fld(OUTPUT_SYNC_CONFIG, 1, VSYNC_POLARITY_OUTPUT_SYNC);
		w_fld(OUTPUT_SYNC_CONFIG, 1, HSYNC_POLARITY_OUTPUT_SYNC);
	}
}

void
hdmi2com_video_init(struct MTK_HDMI *myhdmi)
{
	/* 0x3fc[0] reset MAC register include 0x178  */
	hdmi2com_sherman_reset(myhdmi);

	w_fld(DIG_PHY_CONTROL_2, 1,	HW_SCRAMBLING_ON_EN);

	w_fld(VID_THR, 2, VRES_THR);
	w_fld(VID_THR, 2, HRES_THR);
	w_fld(VID_THR, 2, VSTB_CNT_THR);
	w_fld(VID_THR, 15, HSTB_CNT_THR);

	w_fld(RG_VID_XPCLK_EN, 8, REG_VRES_XCLK_DIFF);

	w_fld(RESIGET_RW_ENABLE_0, 0, FDET_FRAME_RATE_WR_EN);
	w_fld(RESIGET_RW_ENABLE_0, 7, FDET_FRAME_RATE_RD_EN);
	w_fld(RESIGET_RW_ENABLE_0, 0, FDET_PIXEL_COUNT_WR_EN);
	w_fld(RESIGET_RW_ENABLE_0, 3, FDET_PIXEL_COUNT_RD_EN);
	w_fld(RESIGET_RW_ENABLE_0, 0, FDET_LINE_COUNT_WR_EN);
	w_fld(RESIGET_RW_ENABLE_0, 3, FDET_LINE_COUNT_RD_EN);
	w_fld(RESIGET_RW_ENABLE_0, 0, FDET_HSYNC_LOW_COUNT_WR_EN);
	w_fld(RESIGET_RW_ENABLE_0, 3, FDET_HSYNC_LOW_COUNT_RD_EN);
	w_fld(RESIGET_RW_ENABLE_0, 0, FDET_HSYNC_HIGH_COUNT_WR_EN);
	w_fld(RESIGET_RW_ENABLE_0, 3, FDET_HSYNC_HIGH_COUNT_RD_EN);
	w_fld(RESIGET_RW_ENABLE_0, 0, FDET_HFRONT_COUNT_WR_EN);
	w_fld(RESIGET_RW_ENABLE_0, 3, FDET_HFRONT_COUNT_RD_EN);
	w_fld(RESIGET_RW_ENABLE_0, 0, FDET_HBACK_COUNT_WR_EN);
	w_fld(RESIGET_RW_ENABLE_0, 3, FDET_HBACK_COUNT_RD_EN);
	w_fld(RESIGET_RW_ENABLE_0, 0, FDET_VSYNC_LOW_COUNT_EVEN_WR_EN);
	w_fld(RESIGET_RW_ENABLE_1, 3, FDET_VSYNC_LOW_COUNT_EVEN_RD_EN);
	w_fld(RESIGET_RW_ENABLE_1, 0, FDET_VSYNC_HIGH_COUNT_EVEN_WR_EN);
	w_fld(RESIGET_RW_ENABLE_1, 3, FDET_VSYNC_HIGH_COUNT_EVEN_RD_EN);
	w_fld(RESIGET_RW_ENABLE_1, 0, FDET_VFRONT_COUNT_EVEN_WR_EN);
	w_fld(RESIGET_RW_ENABLE_1, 3, FDET_VFRONT_COUNT_EVEN_RD_EN);
	w_fld(RESIGET_RW_ENABLE_1, 0, FDET_VBACK_COUNT_EVEN_WR_EN);
	w_fld(RESIGET_RW_ENABLE_1, 3, FDET_VBACK_COUNT_EVEN_RD_EN);
	w_fld(RESIGET_RW_ENABLE_1, 0, FDET_VSYNC_LOW_COUNT_ODD_WR_EN);
	w_fld(RESIGET_RW_ENABLE_1, 3, FDET_VSYNC_LOW_COUNT_ODD_RD_EN);
	w_fld(RESIGET_RW_ENABLE_1, 0, FDET_VSYNC_HIGH_COUNT_ODD_WR_EN);
	w_fld(RESIGET_RW_ENABLE_1, 3, FDET_VSYNC_HIGH_COUNT_ODD_RD_EN);
	w_fld(RESIGET_RW_ENABLE_1, 0, FDET_VBACK_COUNT_EVEN_WR_EN);
	w_fld(RESIGET_RW_ENABLE_1, 3, FDET_VBACK_COUNT_EVEN_RD_EN);
	w_fld(RESIGET_RW_ENABLE_1, 0, FDET_VSYNC_HIGH_COUNT_ODD_WR_EN);
	w_fld(RESIGET_RW_ENABLE_1, 3, FDET_VSYNC_HIGH_COUNT_ODD_RD_EN);
	w_fld(RESIGET_RW_ENABLE_1, 0, FDET_VFRONT_COUNT_ODD_WR_EN);
	w_fld(RESIGET_RW_ENABLE_1, 3, FDET_VFRONT_COUNT_ODD_RD_EN);
	w_fld(RESIGET_RW_ENABLE_1, 0, FDET_VBACK_COUNT_ODD_WR_EN);
	w_fld(RESIGET_RW_ENABLE_1, 3, FDET_VBACK_COUNT_ODD_RD_EN);
	w_fld(RESIGET_RW_ENABLE_1, 0, FDET_FRAME_COUNT_WR_EN);
	w_fld(RESIGET_RW_ENABLE_2, 3, FDET_FRAME_COUNT_RD_EN);

	w_fld(O_FDET_IRQ_STATUS, 0xf, STATUS_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	FRAME_RATE_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	PIXEL_COUNT_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	LINE_COUNT_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	HSYNC_LOW_COUNT_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	HSYNC_HIGH_COUNT_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	HFRONT_COUNT_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	HBACK_COUNT_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	VSYNC_LOW_COUNT_EVEN_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	VSYNC_HIGH_COUNT_EVEN_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	VFRONT_COUNT_EVEN_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	VBACK_COUNT_EVEN_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	VSYNC_LOW_COUNT_ODD_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	VSYNC_HIGH_COUNT_ODD_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	VFRONT_COUNT_ODD_CHANGED_CLEAR);
	w_fld(O_FDET_IRQ_STATUS, 1,	VBACK_COUNT_ODD_CHANGED_CLEAR);

	w_fld(RESIGET_RW_ENABLE_2, 0, STATUS_CHANGED_MASK);
	w_fld(RESIGET_RW_ENABLE_2, 0, FRAME_RATE_CHANGED_MASK);
	w_fld(RESIGET_RW_ENABLE_2, 0, HFRONT_COUNT_CHANGED_MASK);
	w_fld(RESIGET_RW_ENABLE_2, 0, HBACK_COUNT_CHANGED_MASK);
	w_fld(RESIGET_RW_ENABLE_2, 0, VFRONT_COUNT_EVEN_CHANGED_MASK);
	w_fld(RESIGET_RW_ENABLE_2, 0, VBACK_COUNT_EVEN_CHANGED_MASK);
	w_fld(RESIGET_RW_ENABLE_2, 0, VFRONT_COUNT_ODD_CHANGED_MASK);
	w_fld(RESIGET_RW_ENABLE_2, 0, VBACK_COUNT_ODD_CHANGED_MASK);

	/* video mute */
	w_fld(RG_OUTPUT_BLANK_Y, 0, ENABLE_ACTIVE_OVERRIDE);
	w_fld(RX_AUDP_STAT, 0, REG_CLEAR_AV_MUTE);
	w_fld(RG_OUTPUT_ACTIVE_CB, 0x000, OUTPUT_ACTIVE_Y);
	w_fld(RG_OUTPUT_ACTIVE_CB, 0x800, OUTPUT_ACTIVE_CB);
	w_fld(RG_OUTPUT_ACTIVE_CR, 0x800, OUTPUT_ACTIVE_CR);

	/* fmeter 297m, not 27m */
	w_fld(HDMI_AUD_PLL_SEL, 0, DEBUG_FMETTER_REF_CLK_SEL);

	/* audio Fs auto */
	w_fld(ACR_CONTROL_1, 0,	FS_HW_SW_SEL);

	/* disable data alignment */
	w_fld(RX_AUDP_FIFO, 1, REG_BYP_DALIGN);
	/* hdcp2 rst */
	w_fld(RX_PWD_CTRL, 0, REG_HDCP2X_CRST_N);
	//w_fld(DIG_PHY_CONTROL_2, 0, REG_PRAM_8051_CNTL_SEL);
	/* hdcp2x setting */
	w_fld(RX_PKT_CNT2, 50, REG_HDCP_PKT_THRESH15_0);
	w_fld(RX_HDCP2X_ECC_CNT2CHK_1, 3, RI_CNT2CHK_ECC_7_0_);
	w_fld(RX_HDCP2X_ECC_CTRL, 1, RI_ECC_CHK_EN);

	w_reg(RX_INTR4 + 0x10, 0);
	w_reg(RX_INTR8 + 0x10, 0);
	w_reg(RX_INTR12 + 0x10, 0);
	w_reg(RX_INTR13 + 0x10, 0);
	w_reg(RX_INTR4, 0xFFFFFFFF);
	w_reg(RX_INTR8, 0xFFFFFFFF);
	w_reg(RX_INTR12, 0xFFFFFFFF);
	w_reg(RX_INTR13, 0xFFFFFFFF);
	w_reg(INTR_MASK_RESVD, 0);
	w_reg(INTR_STATUS_RESVD, 0xFFFFFFFF);
	w_reg(RXHDCP2X_CMD_INTR1_MASK, 0x00FF00FF);
	w_reg(REG_INTR_RESERVED2_MASK, 0);
	w_reg(REG_INTR_RESERVED2, 0xFFFFFFFF);
	//w_fld(HDMIRX_TOP_CONFIG, 1, RG_INT_INVERT);

	/* deglitch for SDA and SCL delay */
	w_fld(SCDC_REG10, 6, RGS_SDA_DE_CNT);
	w_fld(SCDC_REG10, 3, RGS_SCL_DE_CNT);

	//clear VRR mode output
	w_fld(VDIEO_CTRL, 1, REG_VRR_EN_CLR);
	w_fld(EMP_VRR_PACKET_CNTRL, 1, REG_VRR_EMP_PKT1_NEW_ONLY);
}

/** @ingroup type_group_hdmirx_hal_API
 * @par Description\n
 *	   audio config.
 * @param[in]
 *	   myhdmi: struct MTK_HDMI *myhdmi.
 * @return
 *	   none.
 * @par Boundary case and Limitation
 *	   none.
 * @par Error case and Error handling
 *	   none.
 * @par Call graph and Caller graph (refer to the graph below)
 * @par Refer to the source code
 */
void
hdmi2com_audio_config(struct MTK_HDMI *myhdmi)
{
	w_fld(RG_RX_AUDRX_CTRL, 0, REG_1ST_BIT);
	w_fld(RG_RX_AUDRX_CTRL, 0, REG_DATA_DIR);
	w_fld(RG_RX_AUDRX_CTRL, 0, REG_JUSTIFY);
	w_fld(RG_RX_AUDRX_CTRL, 0, REG_WS);
	w_fld(RG_RX_AUDRX_CTRL, 0, REG_MSB);
	w_fld(RG_RX_AUDRX_CTRL, 0, REG_SIZE);
	w_fld(RG_RX_AUDRX_CTRL, 0, REG_CLK_EDGE);
	w_fld(RG_RX_AUDRX_CTRL, 0, REG_INVALID_EN);
	w_fld(RG_RX_AUDRX_CTRL, 0, REG_PCM);
	w_fld(RG_RX_AUDRX_CTRL, 0, REG_VUCP);
	w_fld(RG_RX_AUDRX_CTRL, 1, REG_MUTE_FLAT);
	w_fld(RG_RX_AUDRX_CTRL, 1, REG_MCLK_EN);
	w_fld(RG_RX_AUDRX_CTRL, 1, REG_SD0_EN);
	w_fld(RG_RX_AUDRX_CTRL, 1, REG_SD1_EN);
	w_fld(RG_RX_AUDRX_CTRL, 1, REG_SD2_EN);
	w_fld(RG_RX_AUDRX_CTRL, 1, REG_SD3_EN);
	w_fld(RG_RX_AUDRX_CTRL, 0, REG_SD0_MAP);
	w_fld(RG_RX_AUDRX_CTRL, 1, REG_SD1_MAP);
	w_fld(RG_RX_AUDRX_CTRL, 2, REG_SD2_MAP);
	w_fld(RG_RX_AUDRX_CTRL, 3, REG_SD3_MAP);
	w_fld(RG_RX_AUDRX_CTRL, 1, REG_SPDIF_EN);
	w_fld(RG_RX_AUDRX_CTRL, 1, REG_SPDIF_MODE);
	w_fld(RG_RX_AUDRX_CTRL, 1, REG_I2S_MODE);
	w_fld(RG_RX_AUDRX_CTRL, 0, REG_PASS_AUD_ERR);
	w_fld(RG_RX_AUDRX_CTRL, 1, REG_PASS_SPDIF_ERR);
}

void
hdmi2com_aud_acr_setting(struct MTK_HDMI *myhdmi)
{
	w_fld(ACR_CONTROL_4, 3, SDREF);

	/* The following is for software mode DDS Setting,just for pll default
	 */
	if ((hdmi2com_aud_fs_cal(myhdmi) == HDMI2_AUD_FS_44K) ||
		(hdmi2com_aud_fs_cal(myhdmi) == HDMI2_AUD_FS_88K) ||
		(hdmi2com_aud_fs_cal(myhdmi) == HDMI2_AUD_FS_176K))
		/* 44.1k */
		w_fld(ACR_CONTROL_10, 0x131205bc, REG_HADDS2_PCW_SW);
	else
		/* 48KHz */
		/* for DDS mode HDMI setting */
		w_fld(ACR_CONTROL_10, 0x14d867c4, REG_HADDS2_PCW_SW);

	w_fld(ACR_CONTROL_11, 0, HD20_PCW_DATA_SEL_FOR_40NM);
	w_fld(ACR_CONTROL_11, 2, HD20_PCW_DATA_DIV);
	w_fld(ACR_CONTROL_8, 0, XTAL_24M_CLK_SEL);
}

void
hdmi2com_audio_init(struct MTK_HDMI *myhdmi)
{
	/* need to confirm sequence */
	w_fld(RG_RX_N_SVAL1, 0, REG_ACR_INIT);
	w_fld(RG_RX_N_SVAL1, 0,	REG_FS_HW_SW_SEL);
	w_fld(RG_RX_N_SVAL1, 0,	REG_CTS_REUSED_AUTO_EN);
	w_fld(RG_RX_N_SVAL1, 0,	REG_N_HW_SW_SEL);
	w_fld(RG_RX_N_SVAL1, 0,	REG_CTS_HW_SW_SEL);
	w_fld(RG_RX_N_SVAL1, 0,	REG_UPLL_HW_SW_SEL);
	w_fld(RG_RX_N_SVAL1, 0,	REG_CTS_DROPPED_AUTO_EN);
	w_fld(RG_RX_N_SVAL1, 3,	REG_MCLK4DSD);
	w_fld(RG_RX_N_SVAL1, 0,	REG_MCLK4HBRA);
	w_fld(RG_RX_N_SVAL1, 0,	REG_MCLK4DAC);

	w_fld(TOP_MISC, 1, REG_HW_DSD_FEATURE);
	w_fld(TOP_MISC, 1, REG_HW_HBR_FEATURE);
	w_fld(TOP_MISC, 1, REG_HW_8CH_FEATURE);

	w_fld(ACR_CONTROL_0, 0,	POST_DIV2_ENA);
	w_fld(ACR_CONTROL_0, 0,	FM_VAL_SW);
	w_fld(RG_RX_N_SVAL1, 0,	REG_FM_VAL_SW);

	w_fld(RG_RX_N_SVAL1, 1,	REG_CTS_REUSED_AUTO_EN);
	w_fld(RG_RX_N_SVAL1, 1,	REG_CTS_DROPPED_AUTO_EN);

	hdmi2com_audio_config(myhdmi);
	hdmi2com_aud_acr_setting(myhdmi);
}

void
hdmi2com_audio_pll_sel(struct MTK_HDMI *myhdmi, u8 u1Pll)
{
	/* AACR */
	if (u1Pll >= 1) {
		hdmi2com_sel_aacr_ctrl(myhdmi, TRUE);
		hdmi2com_aud_acr_setting(myhdmi);
		w_fld(HDMIRX_CLK_CONFIG_0, 0, RG_DACR_CLK_SEL);
	} else {
		/* DACR */
		hdmi2com_sel_aacr_ctrl(myhdmi, FALSE);
		hdmi2com_aud_acr_rst(myhdmi);
		hdmi2com_aud_dacr_rst(myhdmi);
		w_fld(HDMIRX_CLK_CONFIG_0, 1, RG_DACR_CLK_SEL);
	}
}

void
hdmi2com_audio_pll_config(struct MTK_HDMI *myhdmi, bool enable)
{
	/* disable DIV */
	w_fld(HDMIRX_CLK_CONFIG_0, 1, RG_APLL_DIV_RESET);
	w_fld(HDMIRX_CLK_CONFIG_0, 0, RG_DACR_CLK_SEL);

	/* set ACR SW PCW value */
	w_fld(ACR_CONTROL_11, 0, PCW_CHG_SW);
	/* pcw=(294*8)/26, 90.46153846 */
	w_fld(ACR_CONTROL_10, 0x5ABDFDCB, REG_HADDS2_PCW_SW);
	w_fld(ACR_CONTROL_11, 1, PCW_CHG_SW);
	/* select SW PCW value to AUDPLL */
	w_fld(ACR_CONTROL_11, 1, HD20_PCW_DATA_SEL);
	w_fld(ACR_CONTROL_11, 0, PCW_CHG_SW);

	/* select hdmirx_apll pcw from ACR */
	w_fld(EARC_RX_CFG_2, 1, RG_HDMIRX_PCW_SEL);

	/* pll power off */
	w_fld(HDMIRX_APLL_CON0, 0, RG_HDMIRX_APLL_EN);
	HAL_Delay_us(1);
	w_fld(HDMIRX_APLL_CON5, 1, RG_HDMIRX_APLL_SDM_ISO_EN);
	HAL_Delay_us(1);
	w_fld(HDMIRX_APLL_CON5, 0, RG_HDMIRX_APLL_SDM_PWR_ON);
	/* pll power on */
	if (enable == TRUE) {
		w_fld(HDMIRX_APLL_CON5, 1, RG_HDMIRX_APLL_SDM_PWR_ON);
		HAL_Delay_us(1);
		w_fld(HDMIRX_APLL_CON5, 0, RG_HDMIRX_APLL_SDM_ISO_EN);
		w_fld(HDMIRX_APLL_CON0, 1, RG_HDMIRX_APLL_SDM_FRA_EN);
		HAL_Delay_us(20);
		w_fld(HDMIRX_APLL_CON2, 1, RG_HDMIRX_APLL_SDM_PCW_CHG);
		/* pcw from HDMIRX ACR */
		w_fld(HDMIRX_APLL_CON2, 3, RG_HDMIRX_APLL_POSDIV);
		HAL_Delay_us(1);
		w_fld(HDMIRX_APLL_CON0, 1, RG_HDMIRX_APLL_EN);
		HAL_Delay_us(20);
	}
}

void
hdmi2com_ddc_path_config(struct MTK_HDMI *myhdmi, bool enable)
{
	/* edid deglich */
	w_fld(EDID_DEV0, 0x3, FLD_EDID2_SCL_DE_CNT0);
	w_fld(EDID_DEV0, 0x4, FLD_EDID2_SDA_DE_CNT0);

	/* deglitch SDA */
	w_fld(HDMIRX_CLK_CONFIG_0, 3, RG_SDA_DEGLITCH_CNT);
	w_fld(HDMIRX_CLK_CONFIG_1, 1, RG_DDC_SDA_DEGLITCH_EN);
	/* deglitch SCL */
	w_fld(HDMIRX_CLK_CONFIG_1, 3, RG_SCL_DEGLITCH_CNT);
	w_fld(HDMIRX_CLK_CONFIG_1, 1, RG_DDC_SCL_DEGLITCH_EN);

	w_fld(HDMIRX_CLK_CONFIG_1, 1, RG_DDC_EN);
	w_fld(HDMIRX_CLK_CONFIG_1, 0, RG_DDC_RST);
	w_fld(HDMIRX_CLK_CONFIG_1, 1, RG_DDC_RST);
}

void
hdmi2com_pad_init(struct MTK_HDMI *myhdmi)
{
	/* 5v */
	w_fld(REG_PWR_5V_CON, 3, REG_PWR_5V_DEGLITCH_CNT);
	w_fld(REG_PWR_5V_CON, 1, REG_PWR_5V_DEGLITCH_EN);

	/* sda/scl/hpd5v, config in dts, refer to pinmux, select mode 1 */
}

void
hdmi2com_420_2x_clk_config(struct MTK_HDMI *myhdmi, u8 flag)
{
	if (flag == 1) {
		vHDMI21Phy2xCLK(myhdmi, TRUE);
		w_fld(HDMIRX_CLK_CONFIG_1, 1, RG_DP_CLR_CLK_SEL);
	} else {
		vHDMI21Phy2xCLK(myhdmi, FALSE);
		w_fld(HDMIRX_CLK_CONFIG_1, 0, RG_DP_CLR_CLK_SEL);
	}

	/* reset div */
	//w_fld(HDMIRX_CLK_CONFIG_1, 0, RG_DIV_CLK_ENGINE_RST);
	//w_fld(HDMIRX_CLK_CONFIG_1, 1, RG_DIV_CLK_ENGINE_RST);
}

void hdmi2com_rst_div(struct MTK_HDMI *myhdmi)
{
	/* reset div */
	w_fld(HDMIRX_CLK_CONFIG_1, 0, RG_DIV_CLK_ENGINE_RST);
	w_fld(HDMIRX_CLK_CONFIG_1, 1, RG_DIV_CLK_ENGINE_RST);
}

u8
hdmi2com_is_420_clk_config(struct MTK_HDMI *myhdmi)
{
	if (r_fld(HDMIRX_CLK_CONFIG_1, RG_DP_CLR_CLK_SEL) == 1)
		return 1;
	return 0;
}

void
hdmi2com_path_config(struct MTK_HDMI *myhdmi)
{
	vHDMI21Phy2xCLK(myhdmi, FALSE);

	/* P0_CLK0_PXIEL is 2 * pixel_clk */

	/* disable CG */
	w_fld(HDMIRX_TOP_CONFIG, 0, RG_HDMIRX_CLK_CG);

	/* AD_HDMI20_PLL340M_CK -> tmds_clk */

	/* AD_HDMI20_DEEP340M_CK -> dp_clr_clk */
	w_fld(HDMIRX_CLK_CONFIG_1, 0, RG_DP_CLR_CLK_SEL);
	/* 680ck/2=deep340 */
	w_fld(HDMIRX_CLK_CONFIG_1, 1, RG_DPCLR_DIV);

	/* AD_HDMI20_DEEP340M_CK -> dp_clr_clk_2x */
	w_fld(HDMIRX_CLK_CONFIG_1, 0, RG_DP_CLR_2X_CLK_SEL);
	w_fld(HDMIRX_CLK_CONFIG_1, 1, RG_DPCLR2X_DIV);

	/* reset div */
	w_fld(HDMIRX_CLK_CONFIG_1, 0, RG_DIV_CLK_ENGINE_RST);
	w_fld(HDMIRX_CLK_CONFIG_1, 1, RG_DIV_CLK_ENGINE_RST);

	/* enable CG */
	w_fld(HDMIRX_TOP_CONFIG, 0xfff, RG_HDMIRX_CLK_CG);
}

bool
hdmi2com_upstream_is_hdcp2x_device(struct MTK_HDMI *myhdmi)
{
	if (r_fld(RX_TMDS_CCTRL2, HDCP2X_MODE_SEL))
		return TRUE;
	return FALSE;
}

u32
hdmi2com_hdcp1x_status(struct MTK_HDMI *myhdmi)
{
	enum HdmiRxHdcp1Status hdcpSta = HDMI_RX_HDCP1_STATUS_OFF;

	if (r_fld(RX_SHA_LENGTH1, HDCP_AUTHENTICATED))
		hdcpSta |= HDMI_RX_HDCP1_STATUS_AUTH_DONE;
	else
		hdcpSta |= HDMI_RX_HDCP1_STATUS_AUTH_FAIL;

	if (r_fld(RX_SHA_LENGTH1, HDCP_DECRYPTING))
		hdcpSta |= HDMI_RX_HDCP1_STATUS_DECRYPT_SUCCESS;
	else
		hdcpSta |= HDMI_RX_HDCP1_STATUS_DECRYPT_FAIL;

	return hdcpSta;
}

u32
hdmi2com_hdcp2x_status(struct MTK_HDMI *myhdmi)
{
	enum HdmiRxHdcp2Status hdcpSta = HDMI_RX_HDCP2_STATUS_OFF;

	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR0_SRC0))
		hdcpSta |= HDMI_RX_HDCP2_STATUS_AUTH_DONE;

	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR0_SRC1))
		hdcpSta |= HDMI_RX_HDCP2_STATUS_AUTH_FAIL;

	if (r_fld(TOP_MISC, HDCP2X_DECRYPT_EN))
		hdcpSta |= HDMI_RX_HDCP2_STATUS_DECRYPT_SUCCESS;
	else
		hdcpSta |= HDMI_RX_HDCP2_STATUS_DECRYPT_FAIL;

	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR0_SRC4))
		hdcpSta |= HDMI_RX_HDCP2_STATUS_CCHK_DONE;

	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR0_SRC5))
		hdcpSta |= HDMI_RX_HDCP2_STATUS_CCHK_FAIL;

	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR1_SRC2))
		hdcpSta |= HDMI_RX_HDCP2_STATUS_AKE_SENT_RCVD;

	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR1_SRC2))
		hdcpSta |= HDMI_RX_HDCP2_STATUS_SKE_SENT_RCVD;

	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR1_SRC6))
		hdcpSta |= HDMI_RX_HDCP2_STATUS_CERT_SENT_RCVD;

	return hdcpSta;
}

void
hdmi2com_get_aksv(struct MTK_HDMI *myhdmi, u8 *pbuf)
{
	pbuf[0] = r_fld(RX_SHD_AKSV1, HDCP_AKSV1);
	pbuf[1] = r_fld(RX_SHD_AKSV5, HDCP_AKSV2);
	pbuf[2] = r_fld(RX_SHD_AKSV5, HDCP_AKSV3);
	pbuf[3] = r_fld(RX_SHD_AKSV5, HDCP_AKSV4);
	pbuf[4] = r_fld(RX_SHD_AKSV5, HDCP_AKSV5);
}

void
hdmi2com_get_bksv(struct MTK_HDMI *myhdmi, u8 *pbuf)
{
	pbuf[0] = r_fld(RX_SHD_BKSV_4, HDCP_BKSV1);
	pbuf[1] = r_fld(RX_SHD_BKSV_4, HDCP_BKSV2);
	pbuf[2] = r_fld(RX_SHD_BKSV_4, HDCP_BKSV3);
	pbuf[3] = r_fld(RX_SHD_BKSV_4, HDCP_BKSV4);
	pbuf[4] = r_fld(RX_SHD_AKSV1, HDCP_BKSV5);
}

void
hdmi2com_get_an(struct MTK_HDMI *myhdmi, u8 *pbuf)
{
	pbuf[0] = r_fld(RX_SHD_AN4, HDCP_AN1);
	pbuf[1] = r_fld(RX_SHD_AN4, HDCP_AN2);
	pbuf[2] = r_fld(RX_SHD_AN4, HDCP_AN3);
	pbuf[3] = r_fld(RX_SHD_AN4, HDCP_AN4);
	pbuf[4] = r_fld(RX_SHD_AN8, HDCP_AN5);
	pbuf[5] = r_fld(RX_SHD_AN8, HDCP_AN6);
	pbuf[6] = r_fld(RX_SHD_AN8, HDCP_AN7);
	pbuf[7] = r_fld(RX_SHD_AN8, HDCP_AN8);
}

u16
hdmi2com_get_ri(struct MTK_HDMI *myhdmi)
{
	u8 Ri_0, Ri_1;

	Ri_0 = r_fld(RX_SHD_AKSV1, RI0);
	Ri_1 = r_fld(RX_SHD_AKSV1, RI1);

	return (Ri_1 << 8) | Ri_0;
}

bool
hdmi2com_is_hdcp1x_auth_start(struct MTK_HDMI *myhdmi)
{
	if (r_fld(RX_INTR4, REG_INTR1_STAT1)) {
		w_reg(RX_INTR4,	Fld2Msk32(REG_INTR1_STAT1));
		return TRUE;
	}
	return FALSE;
}

bool
hdmi2com_is_hdcp1x_auth_done(struct MTK_HDMI *myhdmi)
{
	if (r_fld(RX_INTR4, REG_INTR1_STAT0)) {
		w_reg(RX_INTR4,	Fld2Msk32(REG_INTR1_STAT0));
		return TRUE;
	}
	return FALSE;
}

void
hdmi2com_set_hdcp1x_rpt_mode(struct MTK_HDMI *myhdmi, bool bEn)
{
	/* enable hdcp1.x repeater mode */
	w_fld(TOP_MISC, (bEn ? 1 : 0), REG_HW_RPT_FEATURE);
	/* set bcaps is repeater */
	w_fld(RX_HDCP_DEBUG, (bEn ? 1 : 0), REG_REPEATER);
}

bool
hdmi2com_get_hdcp1x_rpt_mode(struct MTK_HDMI *myhdmi, bool log)
{
	u8 temp;

	temp = r_fld(TOP_MISC, REG_HW_RPT_FEATURE);
	if (log)
		RX_DEF_LOG("[RX]REG_HW_RPT_FEATURE=%d\n", temp);
	temp = r_fld(RX_HDCP_DEBUG, REG_REPEATER);
	if (log)
		RX_DEF_LOG("[RX]REG_REPEATER=%d\n", temp);

	if (temp)
		return TRUE;
	return FALSE;
}

bool hdmi2com_is_hdcp1_vready(struct MTK_HDMI *myhdmi)
{
	if (r_fld(RX_KSV_FIFO, RPT_V_RDY))
		return TRUE;
	return FALSE;
}

void hdmi2com_set_hdcp1_rpt(struct MTK_HDMI *myhdmi,
	struct RxHDCPRptData *pdata)
{
	u32 i = 0;
	u8 pbuf[5];
	u32 temp, temp1;

	w_fld(RX_HDCP_DEBUG, pdata->count, REG_DEV_CNT);
	w_fld(RX_HDCP_DEBUG, pdata->depth, REG_DEPTH);

	if (pdata->is_casc_exc)
		w_fld(RX_HDCP_DEBUG, 1, RPT_CASC_EXCEED);
	else
		w_fld(RX_HDCP_DEBUG, 0, RPT_CASC_EXCEED);
	if (pdata->is_devs_exc)
		w_fld(RX_HDCP_DEBUG, 1, REG_DEV_EXCEED);
	else
		w_fld(RX_HDCP_DEBUG, 0, REG_DEV_EXCEED);

	/* ksv list length */
	w_fld(RX_SHA_LENGTH1, ((pdata->count * 5) & 0xFF),
		REG_SHA_LENGTH_7_0_);
	w_fld(RX_KSV_FIFO, (((pdata->count * 5)>>8) & 0x03),
		REG_SHA_LENGTH_9_8_);

	/* write ksv list */
	/* set start address to 0 */
	w_reg(RX_ADR + 0xE00, 0);
	w_reg(RX_ADR + 0xE00, 0x08);
	/* write ksv list */
	for (i = 0; i < ((pdata->count * 5) & 0x3FF); i++) {
		w_reg(RX_ADR + 0xE7C, pdata->list[i]);
		HAL_Delay_us(2);
	}

	if (myhdmi->my_debug == 0xa5) {
		temp = r_fld(RX_HDCP_DEBUG,	REG_DEV_CNT);
		temp1 = r_fld(RX_HDCP_DEBUG, REG_DEPTH);
		RX_DEF_LOG("[RX]hw dev count=%d,depth=%d\n", temp, temp1);

		/* set start address to 0 */
		w_reg(RX_ADR + 0xE00, 0);
		w_reg(RX_ADR + 0xE00, 0x08);
		pbuf[0] = r_reg(0xE7C);
		for (i = 0; i < pdata->count; i++) {
			HAL_Delay_us(2);
			pbuf[0] = r_reg(RX_ADR + 0xE7C);
			HAL_Delay_us(2);
			pbuf[1] = r_reg(RX_ADR + 0xE7C);
			HAL_Delay_us(2);
			pbuf[2] = r_reg(RX_ADR + 0xE7C);
			HAL_Delay_us(2);
			pbuf[3] = r_reg(RX_ADR + 0xE7C);
			HAL_Delay_us(2);
			pbuf[4] = r_reg(RX_ADR + 0xE7C);
			RX_DEF_LOG("[RX]hw ksv_list[%d/%d]:%x %x %x %x %x\n",
				i, pdata->count,
				pbuf[0], pbuf[1], pbuf[2], pbuf[3], pbuf[4]);
		}
	}

	/* set start address to 0 */
	w_reg(RX_ADR + 0xE00, 0);
	w_reg(RX_ADR + 0xE00, 0x08);

	/* go */
	/* set start address to 0 */
	w_reg(RX_ADR + 0xE00, 0);
	w_reg(RX_ADR + 0xE00, 0x10);

	if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP)) {
		RX_DEF_LOG("[RX]repeater para\n");
		RX_DEF_LOG("[RX]is_hdcp2=%d\n",
			pdata->is_hdcp2);
		RX_DEF_LOG("[RX]is_casc_exc=%d\n",
			pdata->is_casc_exc);
		RX_DEF_LOG("[RX]is_devs_exc=%d\n",
			pdata->is_devs_exc);
		RX_DEF_LOG("[RX]is_hdcp1_present=%d\n",
			pdata->is_hdcp1_present);
		RX_DEF_LOG("[RX]is_hdcp2_present=%d\n",
			pdata->is_hdcp2_present);
		RX_DEF_LOG("[RX]count=%d\n",
			pdata->count);
		RX_DEF_LOG("[RX]depth=%d\n",
			pdata->depth);
		for (i = 0; i < pdata->count; i++) {
			RX_DEF_LOG("[RX]ksvlist[%d]: %x %x %x %x %x\n", i,
				pdata->list[i*5+0],
				pdata->list[i*5+1],
				pdata->list[i*5+2],
				pdata->list[i*5+3],
				pdata->list[i*5+4]);
		}
	}
}

void hdmi2com_set_hdcp1_vready(struct MTK_HDMI *myhdmi, bool is_ready)
{
	if (is_ready) {
		/* set fifo ready */
		w_fld(RX_HDCP_DEBUG, 1, REG_FIFO_RDY);
	} else {
		/* clear fifo ready */
		w_fld(RX_HDCP_DEBUG, 0, REG_FIFO_RDY);
	}

	w_fld(RX_HDCP1X_STATUS_WP_EN, 1, FIFO_READY_WP_EN);
}

bool
hdmi2com_is_hdcp2x_auth_start(struct MTK_HDMI *myhdmi)
{
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR1_SRC2)) {
		w_reg(RXHDCP2X_CMD_INTR1_MASK, Fld2Msk32(REG_INTR0_MASK2));
		w_reg(RXHDCP2X_CMD_INTR1_MASK, 0);
		return TRUE;
	}
	return FALSE;
}

void hdmi2com_hdcp2x_req(struct MTK_HDMI *myhdmi, u8 en)
{
	if (en)
		w_fld(TOP_MISC, 1, REG_RE_AUTH_REQ);
	else
		w_fld(TOP_MISC, 0, REG_RE_AUTH_REQ);
}

void hdmi2com_clear_receiver_id_setting(struct MTK_HDMI *myhdmi)
{
	w_fld(RX_HDCP2X_RPT_SMNG_K_EX, 0, REG_HDCP2X_RPT_DEVCNT);
	w_fld(RX_HDCP2X_RPT_SMNG_K_EX, 0, REG_HDCP2X_RPT_DEPTH);
}

bool
hdmi2com_is_hdcp2x_auth_done(struct MTK_HDMI *myhdmi)
{
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR0_SRC0)) {
		w_reg(RXHDCP2X_CMD_INTR1_MASK, Fld2Msk32(REG_INTR0_SRC0));
		return TRUE;
	}
	return FALSE;
}

u8
hdmi2com_check_hdcp1x_decrypt_on(struct MTK_HDMI *myhdmi)
{
	if (r_fld(RX_SHA_LENGTH1, HDCP_DECRYPTING))
		return 1;
	return 0;
}

u8
hdmi2com_check_hdcp2x_decrypt_on(struct MTK_HDMI *myhdmi)
{
	if (r_fld(RGS_DUMMY0, HDCP_DECODE_STATUS)) {
		w_reg(REG_DUMMY1, Fld2Msk32(REG_HDCP_DECODE_STATUS_CLR));
		return 1;
	}
	return 0;
}

void
hdmi2com_disable_hdcp2x(struct MTK_HDMI *myhdmi, bool disable)
{
	if (disable)
		w_fld(RX_TMDS_CCTRL2, 0, RI_HDCP2X_EN);
	else
		w_fld(RX_TMDS_CCTRL2, 1, RI_HDCP2X_EN);
}

void
hdmi2com_set_hdcp2x_rpt_mode(struct MTK_HDMI *myhdmi, bool bEn)
{
	w_fld(RXHDCP2X_CMD_CTRL_2, (bEn ? 1 : 0), REG_HDCP2X_RI_REPEATER);
}

bool
hdmi2com_get_hdcp2x_rpt_mode(struct MTK_HDMI *myhdmi, bool log)
{
	u8 temp;

	temp = r_fld(RXHDCP2X_CMD_CTRL_2, REG_HDCP2X_RI_REPEATER);

	if (log)
		RX_DEF_LOG("[RX][2]REG_HDCP2X_RI_REPEATER = % d\n",
			temp);

	if (temp)
		return TRUE;
	return FALSE;
}

u32
hdmi2com_get_stream_id_type(struct MTK_HDMI *myhdmi)
{
	u8 buf[6], i;

	/* seq-num_m */
	buf[0] = r_fld(RXHDCP2X_RX_SEQ_NUM_M_2,
		REG_HDCP2X_RPT_SEQ_NUM_M_7TO0);
	buf[1] = r_fld(RXHDCP2X_RX_SEQ_NUM_M_2,
		REG_HDCP2X_RPT_SEQ_NUM_M_15TO8);
	buf[2] = r_fld(RXHDCP2X_RX_SEQ_NUM_M_2,
		REG_HDCP2X_RPT_SEQ_NUM_M_23TO16);

	/* k */
	buf[3] = r_fld(RXHDCP2X_RPT_DEPTH, REG_HDCP2X_RPT_SMNG_K_7_0_);

	/* streamid_type */
	buf[4] = 0;
	buf[5] = 0;
	if (buf[3] >= 1) {
		w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			0x0, REGRX_HDCP2X_RPT_SMNG_RD_START);
		w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			0x0, REGRX_HDCP2X_RPT_SMNG_RD);
		hal_delay_2us(1);
		w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			0x1, REGRX_HDCP2X_RPT_SMNG_RD_START);
		w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			0x0, REGRX_HDCP2X_RPT_SMNG_RD_START);
		hal_delay_2us(1);
		buf[4] = r_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			REGTX_HDCP2X_RPT_SMNG_IN);
		w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			0x1, REGRX_HDCP2X_RPT_SMNG_RD);
		w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			0x0, REGRX_HDCP2X_RPT_SMNG_RD);
		hal_delay_2us(1);
		buf[5] = r_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			REGTX_HDCP2X_RPT_SMNG_IN);
		w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			0x1, REGRX_HDCP2X_RPT_SMNG_RD);
		w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			0x0, REGRX_HDCP2X_RPT_SMNG_RD);
	}

	for (i = 0; i < 6; i++)
		RX_HDCP_LOG("[RX]stream_manage[%d]=%x\n", i, buf[i]);

	return ((buf[3] << 16) | (buf[4] << 8) | (buf[5] << 0));
}

void hdmi2com_set_hdcp2_rpt(struct MTK_HDMI *myhdmi,
	struct RxHDCPRptData *pdata)
{
	u8 i = 0;
	u8 pbuf[5];

	if (pdata->is_casc_exc)
		w_fld(RXHDCP2X_RPT_DETAIL_EX, 1,
			REG_HDCP2X_RPT_MX_CASC_EXC_WP);
	else
		w_fld(RXHDCP2X_RPT_DETAIL_EX, 0,
			REG_HDCP2X_RPT_MX_CASC_EXC_WP);

	if (pdata->is_devs_exc)
		w_fld(RXHDCP2X_RPT_DETAIL_EX, 1,
			REG_HDCP2X_RPT_MX_DEVS_EXC_WP);
	else
		w_fld(RXHDCP2X_RPT_DETAIL_EX, 0,
			REG_HDCP2X_RPT_MX_DEVS_EXC_WP);

	if (pdata->is_hdcp1_present)
		w_fld(RXHDCP2X_RPT_DETAIL_EX,
			1, REG_HDCP2X_RPT_HDCP1DEV_DSTRM_WP);
	else
		w_fld(RXHDCP2X_RPT_DETAIL_EX,
			0, REG_HDCP2X_RPT_HDCP1DEV_DSTRM_WP);

	if (pdata->is_hdcp2_present)
		w_fld(RXHDCP2X_RPT_DETAIL_EX,
			1, REG_HDCP2X_RPT_HDCP20RPT_DSTRM_WP);
	else
		w_fld(RXHDCP2X_RPT_DETAIL_EX,
			0, REG_HDCP2X_RPT_HDCP20RPT_DSTRM_WP);

	w_fld(RXHDCP2X_RX_RPT_RCVID_IN,	0x1,
		REGRX_HDCP2X_RPT_RCVID_WR_START);
	w_fld(RXHDCP2X_RX_RPT_RCVID_IN,	0x0,
		REGRX_HDCP2X_RPT_RCVID_WR_START);

	for (i = 0; i < pdata->count*5; i++) {
		w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			pdata->list[i], REGRX_HDCP2X_RPT_RCVID_IN);
		w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			0x1, REGRX_HDCP2X_RPT_RCVID_WR);
		w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			0x0, REGRX_HDCP2X_RPT_RCVID_WR);
	}
	w_fld(RXHDCP2X_RX_RPT_RCVID_IN,	0x1,
		REGRX_HDCP2X_RPT_RCVID_WR_START);
	w_fld(RXHDCP2X_RX_RPT_RCVID_IN,	0x0,
		REGRX_HDCP2X_RPT_RCVID_WR_START);

	if (myhdmi->my_debug == 0xa5) {
		for (i = 0; i < pdata->count; i++) {
			pbuf[0] = r_fld(RXHDCP2X_TX_RPT_RCVID_OUT,
				REGTX_HDCP2X_RPT_RCVID_OUT_7_0_);
			w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
				0x1, REGRX_HDCP2X_RPT_RCVID_WR);
			w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
				0x0, REGRX_HDCP2X_RPT_RCVID_WR);

			pbuf[1] = r_fld(RXHDCP2X_TX_RPT_RCVID_OUT,
				REGTX_HDCP2X_RPT_RCVID_OUT_7_0_);
			w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
				0x1, REGRX_HDCP2X_RPT_RCVID_WR);
			w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
				0x0, REGRX_HDCP2X_RPT_RCVID_WR);

			pbuf[2] = r_fld(RXHDCP2X_TX_RPT_RCVID_OUT,
				REGTX_HDCP2X_RPT_RCVID_OUT_7_0_);
			w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
				0x1, REGRX_HDCP2X_RPT_RCVID_WR);
			w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
				0x0, REGRX_HDCP2X_RPT_RCVID_WR);

			pbuf[3] = r_fld(RXHDCP2X_TX_RPT_RCVID_OUT,
				REGTX_HDCP2X_RPT_RCVID_OUT_7_0_);
			w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
				0x1, REGRX_HDCP2X_RPT_RCVID_WR);
			w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
				0x0, REGRX_HDCP2X_RPT_RCVID_WR);

			pbuf[4] = r_fld(RXHDCP2X_TX_RPT_RCVID_OUT,
				REGTX_HDCP2X_RPT_RCVID_OUT_7_0_);
			w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
				0x1, REGRX_HDCP2X_RPT_RCVID_WR);
			w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
				0x0, REGRX_HDCP2X_RPT_RCVID_WR);

			RX_DEF_LOG("[RX][2]rev_id[%d/%d]:%x %x %x %x %x\n", i,
				pdata->count, pbuf[0], pbuf[1],
				pbuf[2], pbuf[3], pbuf[4]);
		}

		w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			0x1, REGRX_HDCP2X_RPT_RCVID_WR_START);
		w_fld(RXHDCP2X_RX_RPT_RCVID_IN,
			0x0, REGRX_HDCP2X_RPT_RCVID_WR_START);
	}

	w_fld(RX_HDCP2X_RPT_SMNG_K_EX,
		pdata->count, REG_HDCP2X_RPT_DEVCNT);
	w_fld(RX_HDCP2X_RPT_SMNG_K_EX,
		pdata->depth, REG_HDCP2X_RPT_DEPTH);

	if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP)) {
		RX_DEF_LOG("[RX][2]repeater para,%lums\n",
			jiffies);
		RX_DEF_LOG("[RX][2]is_hdcp2=%d\n",
			pdata->is_hdcp2);
		RX_DEF_LOG("[RX][2]is_casc_exc=%d\n",
			pdata->is_casc_exc);
		RX_DEF_LOG("[RX][2]is_devs_exc=%d\n",
			pdata->is_devs_exc);
		RX_DEF_LOG("[RX][2]is_hdcp1_present=%d\n",
			pdata->is_hdcp1_present);
		RX_DEF_LOG("[RX][2]is_hdcp2_present=%d\n",
			pdata->is_hdcp2_present);
		RX_DEF_LOG("[RX][2]count=%d\n",
			pdata->count);
		RX_DEF_LOG("[RX][2]depth=%d\n",
			pdata->depth);
		for (i = 0; i < pdata->count; i++) {
			RX_DEF_LOG("[RX][2]rcv_id[%d]: %x %x %x %x %x\n",
				i,
				pdata->list[i*5+0],
				pdata->list[i*5+1],
				pdata->list[i*5+2],
				pdata->list[i*5+3],
				pdata->list[i*5+4]);
		}
	}
}

bool
hdmi2com_is_hdcp2x_rpt_mode(struct MTK_HDMI *myhdmi)
{
	u8 temp;

	temp = r_fld(RXHDCP2X_CMD_GEN_STATUS, REG_HDCP2X_RO_REPEATER);
	if (temp)
		return TRUE;
	return FALSE;
}

u8
hdmi2com_hdcp2x_state(struct MTK_HDMI *myhdmi)
{
	u8 temp;

	temp = r_fld(RXHDCP2X_CMD_GEN_STATUS, REG_HDCP2X_STATE_7_0_);
	return temp;
}

void
hdmi2com_check_hdcp2x_irq(struct MTK_HDMI *myhdmi)
{
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR1_SRC0))
		RX_DEF_LOG("[RX][2]ro_rpt_rcvid_changed,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR1_SRC1))
		RX_DEF_LOG("[RX][2]ro_rpt_smng_changed,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR1_SRC2))
		RX_DEF_LOG("[RX][2]ro_ake_sent_rcvd,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR1_SRC3))
		RX_DEF_LOG("[RX][2]ro_ske_sent_rcvd,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR1_SRC4))
		RX_DEF_LOG(
			"[RX][2]ro_rpt_rcvid_xfer_done,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR1_SRC5))
		RX_DEF_LOG(
			"[RX][2]ro_rpt_smng_xfer_done,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR1_SRC6))
		RX_DEF_LOG("[RX][2]ro_cert_sent_rcvd,%lums\n", jiffies);

	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR0_SRC0))
		RX_DEF_LOG("[RX][2]auth_done,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR0_SRC1))
		RX_DEF_LOG("[RX][2]auth_fail,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR0_SRC2))
		RX_DEF_LOG("[RX][2]rpt_ready,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR0_SRC3))
		RX_DEF_LOG("[RX][2]hash_fail,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR0_SRC4))
		RX_DEF_LOG("[RX][2]cchk_done,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR0_SRC5))
		RX_DEF_LOG("[RX][2]cchk_fail,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR0_SRC6))
		RX_DEF_LOG("[RX][2]reauth_req,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR0_SRC7))
		RX_DEF_LOG("[RX][2]polling_interval,%lums\n", jiffies);

	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR2_SRC0))
		RX_DEF_LOG("[RX][2]ro_km_sent_rcvd,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR2_SRC1))
		RX_DEF_LOG("[RX][2]ro_ekhkm_sent_rcvd,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR2_SRC2))
		RX_DEF_LOG("[RX][2]ro_h_sent_rcvd,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR2_SRC3))
		RX_DEF_LOG("[RX][2]ro_pair_sent_rcvd,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR2_SRC4))
		RX_DEF_LOG("[RX][2]ro_lc_sent_rcvd,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR2_SRC5))
		RX_DEF_LOG("[RX][2]ro_l_sent_rcvd,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR2_SRC6))
		RX_DEF_LOG("[RX][2]ro_vack_sent_rcvd,%lums\n", jiffies);
	if (r_fld(RXHDCP2X_CMD_INTR1_MASK, REG_INTR2_SRC7))
		RX_DEF_LOG("[RX][2]ro_mack_sent_rcvd,%lums\n", jiffies);
}

void
hdmi2com_hdcp2x_irq_clr(struct MTK_HDMI *myhdmi)
{
	w_reg(RXHDCP2X_CMD_INTR1_MASK, 0xfffffbff);
}

void
hdmi2com_hdcp_init(struct MTK_HDMI *myhdmi)
{
	//w_fld(DIG_PHY_CONTROL_2, 1, REG_PRAM_8051_CNTL_SEL);
	w_fld(RX_PWD_CTRL, 1, REG_HDCP2X_CRST_N);

	w_fld(RXHDCP2X_CMD_CTRL_2, 0, REG_HDCP2X_RI_REPEATER);

	w_fld(RXHDCP2X_GPCTL, 0x80, REG_HDCP2X_CUPD_SIZE_7_0_);
	w_fld(RXHDCP2X_GPCTL, 0x3f, REG_HDCP2X_CUPD_SIZE_15_8_);

	w_fld(RXHDCP2X_CMD_TP3, 140, REG_HDCP2X_TP1_7_0_);
	w_fld(RXHDCP2X_CMD_TP3, 0x96, REG_HDCP2X_TP3_7_0_);
	w_fld(RXHDCP2X_CMD_GP_IN3, 0, REG_HDCP2X_GP_IN1_7_0_);

	w_fld(RXHDCP2X_CMD_CTRL_2, 0, REG_HDCP2X_HDCPTX);

	w_fld(RXHDCP2X_CMD_CTRL_2, 1, REG_HDCP2X_HDMIMODE);
	w_fld(RXHDCP2X_CMD_CTRL_2, 0, REG_HDCP2X_PRECOMPUTE);
	w_fld(RXHDCP2X_CMD_CTRL_2, 0, REG_HDCP2X_STM_ID);
	w_fld(RXHDCP2X_CMD_CTRL_2, 0, REG_HDCP2X_REAUTH_SW);
	w_fld(RXHDCP2X_CMD_CTRL_2, 0, REG_HDCP2X_CTL3MSK);
	w_fld(RXHDCP2X_CMD_CTRL_2, 0xf, REG_HDCP2X_REAUTH_MSK);
	w_fld(RXHDCP2X_CMD_CTRL_2, 0, REG_HDCP2X_REAUTH_SW);

	w_fld(RXHDCP2X_CMD_CTRL_2, 0xa,	REG_HDCP2X_CPVER_3_0_);
	w_fld(RXHDCP2X_CMD_CTRL_2, 0, REG_HDCP2X_CUPD_DONE);
	w_fld(RXHDCP2X_CMD_CTRL_2, 1, REG_HDCP2X_CUPD_START);
	hal_delay_2us(1);
	w_fld(RXHDCP2X_CMD_CTRL_2, 1, REG_HDCP2X_CUPD_DONE);

	hal_delay_2us(10);

	myhdmi->u1CchkCheckCount = 0;
	while (!(hdmi2com_hdcp2x_status(myhdmi) &
		 HDMI_RX_HDCP2_STATUS_CCHK_DONE)) {
		myhdmi->u1CchkCheckCount++;
		if (myhdmi->u1CchkCheckCount >
			HDMI2_HDCP2_CCHK_DONE_CHECK_COUNT) {
			RX_DEF_LOG("[RX]HDCP2 Err:Check CCHK done fail\n");
			break;
		}
		hal_delay_2us(10);
	}
	myhdmi->u1CchkCheckCount = 0;

	w_fld(RXHDCP2X_CMD_CTRL_2, 0, REG_HDCP2X_CUPD_DONE);
	w_fld(RXHDCP2X_CMD_CTRL_2, 0, REG_HDCP2X_CUPD_START);

	/* set hdmi capbility */
	w_fld(RX_HDCP_DEBUG, 1, REG_HDMI_CAPABLE);
}

void
hdmi2com_hdcp2x_pupd(struct MTK_HDMI *myhdmi)
{
	w_fld(RXHDCP2X_CMD_CTRL_2, 1, REG_HDCP2X_CUPD_START);
	w_fld(RXHDCP2X_CMD_CTRL_2, 1, REG_HDCP2X_CUPD_DONE);
	hal_delay_2us(50);
	w_fld(RXHDCP2X_CMD_CTRL_2, 0, REG_HDCP2X_CUPD_START);
	w_fld(RXHDCP2X_CMD_CTRL_2, 0, REG_HDCP2X_CUPD_DONE);
	hal_delay_2us(10);
}

void
hdmi2com_check_hdcp1x_status(struct MTK_HDMI *myhdmi)
{
	u8 temp;

	temp = r_fld(RX_SHA_LENGTH1, HDCP_SPHST);
	RX_DEF_LOG("[RX]cipher state : %x\n", temp);

	/* bksv */
	temp = r_fld(RX_SHD_BKSV_4, HDCP_BKSV1);
	RX_DEF_LOG("[RX]bksv1 : %x\n", temp);
	temp = r_fld(RX_SHD_BKSV_4,	HDCP_BKSV2);
	RX_DEF_LOG("[RX]bksv2 : %x\n", temp);
	temp = r_fld(RX_SHD_BKSV_4,	HDCP_BKSV3);
	RX_DEF_LOG("[RX]bksv3 : %x\n", temp);
	temp = r_fld(RX_SHD_BKSV_4,	HDCP_BKSV4);
	RX_DEF_LOG("[RX]bksv4 : %x\n", temp);
	temp = r_fld(RX_SHD_AKSV1, HDCP_BKSV5);
	RX_DEF_LOG("[RX]bksv5 : %x\n", temp);
	/* ri */
	temp = r_fld(RX_SHD_AKSV1, RI0);
	RX_DEF_LOG("[RX]ri_0 : %x\n", temp);
	temp = r_fld(RX_SHD_AKSV1, RI1);
	RX_DEF_LOG("[RX]ri_1 : %x\n", temp);
	/* aksv */
	temp = r_fld(RX_SHD_AKSV1, HDCP_AKSV1);
	RX_DEF_LOG("[RX]aksv1 : %x\n", temp);
	temp = r_fld(RX_SHD_AKSV5, HDCP_AKSV2);
	RX_DEF_LOG("[RX]aksv2 : %x\n", temp);
	temp = r_fld(RX_SHD_AKSV5, HDCP_AKSV3);
	RX_DEF_LOG("[RX]aksv3 : %x\n", temp);
	temp = r_fld(RX_SHD_AKSV5, HDCP_AKSV4);
	RX_DEF_LOG("[RX]aksv4 : %x\n", temp);
	temp = r_fld(RX_SHD_AKSV5, HDCP_AKSV5);
	RX_DEF_LOG("[RX]aksv5 : %x\n", temp);
	/* an */
	temp = r_fld(RX_SHD_AN4, HDCP_AN1);
	RX_DEF_LOG("[RX]an1 : %x\n", temp);
	temp = r_fld(RX_SHD_AN4, HDCP_AN2);
	RX_DEF_LOG("[RX]an2 : %x\n", temp);
	temp = r_fld(RX_SHD_AN4, HDCP_AN3);
	RX_DEF_LOG("[RX]an3: %x\n", temp);
	temp = r_fld(RX_SHD_AN4, HDCP_AN4);
	RX_DEF_LOG("[RX]an4 : %x\n", temp);
	temp = r_fld(RX_SHD_AN8, HDCP_AN5);
	RX_DEF_LOG("[RX]an5 : %x\n", temp);
	temp = r_fld(RX_SHD_AN8, HDCP_AN6);
	RX_DEF_LOG("[RX]an6 : %x\n", temp);
	temp = r_fld(RX_SHD_AN8, HDCP_AN7);
	RX_DEF_LOG("[RX]an7 : %x\n", temp);
	temp = r_fld(RX_SHD_AN8, HDCP_AN8);
	RX_DEF_LOG("[RX]an8 : %x\n", temp);
}

void
hdmi2com_dig_phy_rst1(struct MTK_HDMI *myhdmi)
{
	w_fld(DIG_PHY_CONTROL_0, 0,	REG_DIG_PHY_RSTB);
	hal_delay_2us(1);
	w_fld(DIG_PHY_CONTROL_0, 1, REG_DIG_PHY_RSTB);
}

void
hdmi2com_dig_phy_rst(struct MTK_HDMI *myhdmi, u8 u1en)
{
	w_fld(DIG_PHY_CONTROL_0, u1en, REG_DIG_PHY_RSTB);
}

void
hdmi2com_aud_acr_rst(struct MTK_HDMI *myhdmi)
{
	w_fld(ACR_CONTROL_11, 1, PCW_CHG_SW);
	w_fld(ACR_CONTROL_11, 1, HD20_PCW_DATA_SEL);
	w_fld(ACR_CONTROL_11, 0, PCW_CHG_SW);
	hal_delay_2us(1);
	w_fld(ACR_CONTROL_11, 1, PCW_CHG_SW);

	usleep_range(1000, 1050);
	w_fld(RX_PWD_CTRL, 1, REG_ACR_RST);
	hal_delay_2us(1);
	w_fld(RX_PWD_CTRL, 0, REG_ACR_RST);
	usleep_range(1000, 1050);
	w_fld(ACR_CONTROL_11, 0, HD20_PCW_DATA_SEL);
}

void
hdmi2com_aud_dacr_rst(struct MTK_HDMI *myhdmi)
{
	w_fld(SYS_TMDS_CH_MAP, 1, REG_DACR_RST);
	hal_delay_2us(1);
	w_fld(SYS_TMDS_CH_MAP, 0, REG_DACR_RST);
}

void
hdmi2com_aud_fifo_rst(struct MTK_HDMI *myhdmi)
{
	w_fld(RX_PWD_CTRL, 1, REG_FIFO_RST);
	hal_delay_2us(1);
	w_fld(RX_PWD_CTRL, 0, REG_FIFO_RST);
	hal_delay_2us(1);
}

void
hdmi2com_deep_rst1(struct MTK_HDMI *myhdmi)
{
	w_fld(RX_PWD_CTRL, 1, REG_DC_FIFO_RST);
	hal_delay_2us(1);
	w_fld(RX_PWD_CTRL, 0, REG_DC_FIFO_RST);
}

void
hdmi2com_deep_rst(struct MTK_HDMI *myhdmi, u8 u1En)
{
	w_fld(RX_PWD_CTRL, u1En, REG_DC_FIFO_RST);
}

void hdmi2com_osc_rst(struct MTK_HDMI *myhdmi)
{
	w_fld(RX_PWD_CTRL, 1, REG_OSC_RST);
	hal_delay_2us(1);
	w_fld(RX_PWD_CTRL, 0, REG_OSC_RST);
}

void
hdmi2com_hdcp1x_rst(struct MTK_HDMI *myhdmi)
{
	w_fld(RX_PWD_CTRL, 1, REG_HDCP_RST);
	hal_delay_2us(1);
	w_fld(RX_PWD_CTRL, 0, REG_HDCP_RST);
}

void
hdmi2com_hdcp2x_rst(struct MTK_HDMI *myhdmi)
{
	w_fld(RX_PWD_CTRL, 0, REG_HDCP2X_CRST_N);
	hal_delay_2us(1);
	w_fld(RX_PWD_CTRL, 1, REG_HDCP2X_CRST_N);
}

void
hdmi2com_hdcp_reset(struct MTK_HDMI *myhdmi)
{
	hdmi2com_hdcp1x_rst(myhdmi);
	hdmi2com_set_hdcp1_vready(myhdmi, FALSE);
	hdmi2com_hdcp2x_rst(myhdmi);
	hdcp2_reset_sram_address(myhdmi);
	usleep_range(1000, 1050);
	hdmi2com_hdcp2x_pupd(myhdmi);
}

void hdmi2com_hdcp2_reset(struct MTK_HDMI *myhdmi)
{
	hdcp2_reset_sram_address(myhdmi);
	usleep_range(1000, 1050);
	hdmi2com_hdcp2x_pupd(myhdmi);
	hdmi2com_osc_rst(myhdmi);
	hdmi2com_hdcp2x_rst(myhdmi);
}

void
hdmi2com_rx_init(struct MTK_HDMI *myhdmi)
{
	hdmi2com_video_init(myhdmi);
	hdmi2com_audio_init(myhdmi);
}

void
hdmi2com_set_420_to_444(struct MTK_HDMI *myhdmi, u8 u1En)
{
	hdmi2com_set_420_output(myhdmi, u1En);
	hdmi2com_set_420_to_422(myhdmi, u1En);
	hdmi2com_set_422_to_444(myhdmi, u1En);
}

void
hdmi2com_set_rgb_to_ycbcr(struct MTK_HDMI *myhdmi, u8 enable)
{
	if (enable == 0)
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 0, ENABLE_MULTI_CSC);
	else
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 1, ENABLE_MULTI_CSC);
}

void
hdmi2com_set_input_format_level_color(struct MTK_HDMI *myhdmi,
	u8 format, u8 level, u8 color)
{
	if (format == 0) /* YUV */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 0, IN_RGB);
	else /* RGB */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 1, IN_RGB);

	if (level == 0) /* video level */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 0, IN_PC);
	else /* PC level */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 1, IN_PC);

	if (color == 0) /* BT709 */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 0, IN_STD);
	else if (color == 1) /* BT601 */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 1, IN_STD);
	else if (color == 2) /* BT202(non-constant) */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 2, IN_STD);
	else if (color == 3) /* BT202(constant) */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 3, IN_STD);
	else /* BT709 */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 0, IN_STD);
}

void
hdmi2com_set_output_format_level_color(struct MTK_HDMI *myhdmi,
	u8 format, u8 level, u8 color)
{
	if (format == 0) /* YUV */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 0, OUT_RGB);
	else /* RGB */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 1, OUT_RGB);

	if (level == 0) /* video level */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 0, OUT_PC);
	else /* PC level */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 1, OUT_PC);

	if (color == 0) /* BT709 */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 0, OUT_STD);
	else if (color == 1) /* BT601 */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 1, OUT_STD);
	else if (color == 2) /* BT202(non-constant) */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 2, OUT_STD);
	else if (color == 3) /* BT202(constant) */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 3, OUT_STD);
	else /* BT709 */
		w_fld(RG_MULTI_CSC_MULTCOEFFR1C1, 0, OUT_STD);
}

bool
hdmi2com_is_420_mode(struct MTK_HDMI *myhdmi)
{
	u8 u1ColorInfo;
	bool bIs420 = FALSE;

	if (hdmi2com_is_hdmi_mode(myhdmi)) {
		if (r_fld(AVIRX_VERS, CEA_AVI_HEADER_7_0_) == 0x82) {
			u1ColorInfo = r_reg_1b(AVIRX_DBYTE2 + 2) & 0x60;
			if (u1ColorInfo == 0x60)
				bIs420 = TRUE;
		}
	}

	return bIs420;
}

void
hdmi2com_is_hpd_high(struct MTK_HDMI *myhdmi, u8 u1HDMICurrSwitch)
{

	switch (u1HDMICurrSwitch) {
	case HDMI_SWITCH_1:
		hdmi2com_set_rx_port1_hpd_level(myhdmi, TRUE);
		break;

	case HDMI_SWITCH_2:
		break;

	default:
		break;
	}
}

void
hdmi2com_set_hpd_low(struct MTK_HDMI *myhdmi, u8 u1HDMICurrSwitch)
{
	switch (u1HDMICurrSwitch) {
	case HDMI_SWITCH_1:
		hdmi2com_set_rx_port1_hpd_level(myhdmi, FALSE);
		break;

	case HDMI_SWITCH_2:
		break;

	default:
		break;
	}
}

bool
hdmi2com_get_hpd_level(struct MTK_HDMI *myhdmi, u8 u1HDMICurrSwitch)
{
	bool flag = FALSE;

	switch (u1HDMICurrSwitch) {
	case HDMI_SWITCH_1:
		flag = hdmi2com_get_rx_port1_hpd_level(myhdmi);
		break;
	default:
		break;
	}

	return flag;
}

u8
hdmi2com_get_port1_5v_level(struct MTK_HDMI *myhdmi)
{
	if (r_fld(RGS_DUMMY0, PWR_5V_ORIGINAL) == 0)
		return 0;
	return 1;
}

u8
hdmi2com_get_port2_5v_level(struct MTK_HDMI *myhdmi)
{
	return 1;
}

void
hdmi2com_switch_port_sel(struct MTK_HDMI *myhdmi, u8 bPort)
{
}

void
hdmi2com_get_video_info_by_avi(struct MTK_HDMI *myhdmi)
{
	struct HdmiVideoInfo_t videoInfo;

	videoInfo = hdmi2com_avi_infoframe_parser(myhdmi);

	if (videoInfo.clrSpc == HDMI_RX_CLRSPC_YC444_601)
		RX_DEF_LOG("[RX]YC444_601\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_YC422_601)
		RX_DEF_LOG("[RX]YC422_601\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_YC420_601)
		RX_DEF_LOG("[RX]YC420_601\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_YC444_709)
		RX_DEF_LOG("[RX]YC444_709\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_YC422_709)
		RX_DEF_LOG("[RX]YC422_709\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_YC420_709)
		RX_DEF_LOG("[RX]YC420_709\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_XVYC444_601)
		RX_DEF_LOG("[RX]XVYC444_601\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_XVYC422_601)
		RX_DEF_LOG("[RX]XVYC422_601\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_XVYC420_601)
		RX_DEF_LOG("[RX]XVYC420_601\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_XVYC444_709)
		RX_DEF_LOG("[RX]XVYC444_709\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_XVYC422_709)
		RX_DEF_LOG("[RX]XVYC422_709\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_XVYC420_709)
		RX_DEF_LOG("[RX]XVYC420_709\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_sYCC444_601)
		RX_DEF_LOG("[RX]sYCC444_601\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_sYCC422_601)
		RX_DEF_LOG("[RX]sYCC422_601\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_sYCC420_601)
		RX_DEF_LOG("[RX]sYCC420_601\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_Adobe_YCC444_601)
		RX_DEF_LOG("[RX]Adobe YCC444_601\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_Adobe_YCC422_601)
		RX_DEF_LOG("[RX]Adobe YCC422_601\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_Adobe_YCC420_601)
		RX_DEF_LOG("[RX]Adobe YCC420_601\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_RGB)
		RX_DEF_LOG("[RX]RGB\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_Adobe_RGB)
		RX_DEF_LOG("[RX]Adobe RGB\n");
	else if (videoInfo.clrSpc ==
		 HDMI_RX_CLRSPC_BT_2020_RGB_non_const_luminous)
		RX_DEF_LOG("[RX]BT_2020_RGB_non_const_luminous\n");
	else if (videoInfo.clrSpc == HDMI_RX_CLRSPC_BT_2020_RGB_const_luminous)
		RX_DEF_LOG("[RX]BT_2020_RGB_const_luminous\n");
	else if (videoInfo.clrSpc ==
		 HDMI_RX_CLRSPC_BT_2020_YCC444_const_luminous)
		RX_DEF_LOG("[RX]BT_2020_YCC444_const_luminous\n");
	else if (videoInfo.clrSpc ==
		 HDMI_RX_CLRSPC_BT_2020_YCC444_non_const_luminous)
		RX_DEF_LOG("[RX]BT_2020_YCC444_non_const_luminous\n");
	else if (videoInfo.clrSpc ==
		 HDMI_RX_CLRSPC_BT_2020_YCC422_non_const_luminous)
		RX_DEF_LOG("[RX]BT_2020_YCC422_non_const_luminous\n");
	else if (videoInfo.clrSpc ==
		 HDMI_RX_CLRSPC_BT_2020_YCC422_const_luminous)
		RX_DEF_LOG("[RX]BT_2020_YCC422_const_luminous\n");
	else if (videoInfo.clrSpc ==
		 HDMI_RX_CLRSPC_BT_2020_YCC420_const_luminous)
		RX_DEF_LOG("[RX]BT_2020_YCC420_const_luminous\n");
	else if (videoInfo.clrSpc ==
		 HDMI_RX_CLRSPC_BT_2020_YCC420_non_const_luminous)
		RX_DEF_LOG("[RX]BT_2020_YCC420_non_const_luminous\n");
}

void
hdmi2com_show_video_status(struct MTK_HDMI *myhdmi)
{
	if (hdmi2com_is_ckdt_1(myhdmi))
		RX_DEF_LOG("[RX]CKDT detected\n");
	else
		RX_DEF_LOG("[RX]No CKDT\n");

	if (hdmi2com_is_scdt_1(myhdmi))
		RX_DEF_LOG("[RX]SCDT detected\n");
	else
		RX_DEF_LOG("[RX]NO SCDT\n");

	RX_DEF_LOG("[RX]width: %d\n", hdmi2com_h_active(myhdmi));
	RX_DEF_LOG("[RX]height: %d\n", hdmi2com_v_active(myhdmi));
	RX_DEF_LOG("[RX]htotal: %d\n", hdmi2com_h_total(myhdmi));
	RX_DEF_LOG("[RX]vtotal: %d\n", hdmi2com_v_total(myhdmi));
	RX_DEF_LOG("[RX]pixel freq: %dHZ\n", hdmi2com_pixel_clk(myhdmi));
	RX_DEF_LOG("[RX]refresh rate: %d\n", hdmi2com_frame_rate(myhdmi));

	if (hdmi2com_is_interlace(myhdmi))
		RX_DEF_LOG("[RX]interface signal\n");
	else
		RX_DEF_LOG("[RX]progressive signal\n");

	if (hdmi2com_is_hdmi_mode(myhdmi))
		RX_DEF_LOG("[RX]HDMI mode\n");
	else
		RX_DEF_LOG("[RX]DVI mode\n");

	hdmi2com_get_video_info_by_avi(myhdmi);

	if (hdmi2com_get_deep_status(myhdmi) == HDMI_RX_BIT_DEPTH_8_BIT)
		RX_DEF_LOG("[RX]8 bit\n");
	else if (hdmi2com_get_deep_status(myhdmi) == HDMI_RX_BIT_DEPTH_10_BIT)
		RX_DEF_LOG("[RX]10 bit\n");
	else if (hdmi2com_get_deep_status(myhdmi) == HDMI_RX_BIT_DEPTH_12_BIT)
		RX_DEF_LOG("[RX]12 bit\n");
	else
		RX_DEF_LOG("[RX]16 bit\n");

	if (hdmi2com_is_scrambing_by_scdc(myhdmi))
		RX_DEF_LOG("[RX]HDMI2.0 and Tx scrambling en\n");
	else
		RX_DEF_LOG("[RX]Tx scrambling dis\n");

	if (hdmi2com_is_tmds_clk_radio_40x(myhdmi))
		RX_DEF_LOG("[RX]TMDS clock ratio-1:40\n");
	else
		RX_DEF_LOG("[RX]TMDS clock ratio-1:10\n");
}

void
hdmi2com_show_audio_status(struct MTK_HDMI *myhdmi)
{
	u8 u1FifoSta = 0;
	u8 u1Fs = 0;

	u1FifoSta = hdmi2com_get_aud_fifo_status(myhdmi);

	if (hdmi2com_is_hdmi_mode(myhdmi))
		RX_DEF_LOG("[RX]HDMI mode\n");
	else
		RX_DEF_LOG("[RX]DVI mode\n");

	if (u1FifoSta & HDMI_AUDIO_FIFO_NORMAL)
		RX_DEF_LOG("[RX]HDMI audio fifo ok\n");

	if (u1FifoSta & HDMI_AUDIO_FIFO_OVERRUN)
		RX_DEF_LOG("[RX]HDMI audio fifo overrun\n");

	if (u1FifoSta & HDMI_AUDIO_FIFO_UNDERRUN)
		RX_DEF_LOG("[RX]HDMI audio fifo underrun\n");

	if (hdmi2com_is_acr_lock(myhdmi) == TRUE)
		RX_DEF_LOG("[RX]aacr lock\n");
	else
		RX_DEF_LOG("[RX]aacr un-lock\n");

	u1Fs = hdmi2com_aud_fs(myhdmi);

	switch (u1Fs) {
	case HDMI2_AUD_FS_44K:
		RX_DEF_LOG("[RX]Audio Fs=44.1K\n");
		break;
	case HDMI2_AUD_FS_88K:
		RX_DEF_LOG("[RX]Audio Fs=88.2K\n");
		break;
	case HDMI2_AUD_FS_176K:
		RX_DEF_LOG("[RX]Audio Fs=176K\n");
		break;
	case HDMI2_AUD_FS_48K:
		RX_DEF_LOG("[RX]Audio Fs=48K\n");
		break;
	case HDMI2_AUD_FS_96K:
		RX_DEF_LOG("[RX]Audio Fs=96K\n");
		break;
	case HDMI2_AUD_FS_192K:
		RX_DEF_LOG("[RX]Audio Fs=192K\n");
		break;
	case HDMI2_AUD_FS_32K:
		RX_DEF_LOG("[RX]Audio Fs=32K\n");
		break;
	case HDMI2_AUD_FS_768K:
		RX_DEF_LOG("[RX]Audio Fs=768K\n");
		break;
	case HDMI2_AUD_FS_384K:
		RX_DEF_LOG("[RX]Audio Fs=HDMI2_AUD_FS_384K\n");
		break;
	case HDMI2_AUD_FS_1536K:
		RX_DEF_LOG("[RX]Audio Fs=HDMI2_AUD_FS_1536K\n");
		break;
	case HDMI2_AUD_FS_1024K:
		RX_DEF_LOG("[RX]Audio Fs=HDMI2_AUD_FS_1024K\n");
		break;
	case HDMI2_AUD_FS_3528K:
		RX_DEF_LOG("[RX]Audio Fs=HDMI2_AUD_FS_3528K\n");
		break;
	case HDMI2_AUD_FS_7056K:
		RX_DEF_LOG("[RX]Audio Fs=HDMI2_AUD_FS_7056K\n");
		break;
	case HDMI2_AUD_FS_14112K:
		RX_DEF_LOG("[RX]Audio Fs=HDMI2_AUD_FS_14112K\n");
		break;
	case HDMI2_AUD_FS_64K:
		RX_DEF_LOG("[RX]Audio Fs=HDMI2_AUD_FS_64K\n");
		break;
	case HDMI2_AUD_FS_128K:
		RX_DEF_LOG("[RX]Audio Fs=HDMI2_AUD_FS_128K\n");
		break;
	case HDMI2_AUD_FS_256K:
		RX_DEF_LOG("[RX]Audio Fs=HDMI2_AUD_FS_256K\n");
		break;
	case HDMI2_AUD_FS_512K:
		RX_DEF_LOG("[RX]Audio Fs=HDMI2_AUD_FS_512K\n");
		break;
	default:
		RX_DEF_LOG("[RX]%x invalid audio\n", u1Fs);
		break;
	}

	RX_DEF_LOG("[RX]Audio C = %d\n", hdmi2com_cts_of_ncts(myhdmi));
	RX_DEF_LOG("[RX]Audio  N  = %d\n", hdmi2com_n_of_ncts(myhdmi));
	RX_DEF_LOG("[RX]tmds clk = %d\n", hdmi2com_tmds_clk_freq(myhdmi));
	u1Fs = hdmi2com_aud_fs_cal(myhdmi);
	switch (u1Fs) {
	case HDMI2_AUD_FS_44K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=44.1K\n");
		break;
	case HDMI2_AUD_FS_88K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=88.2K\n");
		break;
	case HDMI2_AUD_FS_176K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=176K\n");
		break;
	case HDMI2_AUD_FS_48K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=48K\n");
		break;
	case HDMI2_AUD_FS_96K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=96K\n");
		break;
	case HDMI2_AUD_FS_192K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=192K\n");
		break;
	case HDMI2_AUD_FS_32K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=32K\n");
		break;
	case HDMI2_AUD_FS_768K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=768K\n");
		break;
	case HDMI2_AUD_FS_384K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=HDMI2_AUD_FS_384K\n");
		break;
	case HDMI2_AUD_FS_1536K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=HDMI2_AUD_FS_1536K\n");
		break;
	case HDMI2_AUD_FS_1024K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=HDMI2_AUD_FS_1024K\n");
		break;
	case HDMI2_AUD_FS_3528K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=HDMI2_AUD_FS_3528K\n");
		break;
	case HDMI2_AUD_FS_7056K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=HDMI2_AUD_FS_7056K\n");
		break;
	case HDMI2_AUD_FS_14112K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=HDMI2_AUD_FS_14112K\n");
		break;
	case HDMI2_AUD_FS_64K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=HDMI2_AUD_FS_64K\n");
		break;
	case HDMI2_AUD_FS_128K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=HDMI2_AUD_FS_128K\n");
		break;
	case HDMI2_AUD_FS_256K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=HDMI2_AUD_FS_256K\n");
		break;
	case HDMI2_AUD_FS_512K:
		RX_DEF_LOG("[RX]C/N/TMDS cal Audio Fs=HDMI2_AUD_FS_512K\n");
		break;
	default:
		RX_DEF_LOG("[RX]C/N/TMDS cal: %x invalid audio\n", u1Fs);
		break;
	}

	if (hdmi2com_get_aud_layout(myhdmi))
		RX_DEF_LOG("[RX]multi channel audio(up to 8)\n");
	else
		RX_DEF_LOG("[RX]2 channel audio\n");
}

void
hdmi2com_deep_color_auto_en(struct MTK_HDMI *myhdmi, bool en, u32 deep)
{
	w_fld(DIG_PHY_CONTROL_1, deep, REG_DEEP_STATUS);
	if (en)
		w_fld(DIG_PHY_CONTROL_1, 0, REG_DEEP_SW_SEL);
	else
		w_fld(DIG_PHY_CONTROL_1, 1, REG_DEEP_SW_SEL);
}

bool
hdmi2com_is_pixel_rpt(struct MTK_HDMI *myhdmi)
{

	bool bIsPP = FALSE;

	if (hdmi2com_is_hdmi_mode(myhdmi))
		if (r_fld(AVIRX_VERS, CEA_AVI_HEADER_7_0_) == 0x82)
			if ((r_fld(AVIRX_DBYTE6, CEA_AVI_DBYTE5) & 0xf))
				bIsPP = TRUE;

	return bIsPP;
}

u32
hdmi2com_get_pixel_rpt(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = r_fld(AVIRX_DBYTE6, CEA_AVI_DBYTE5);
	return temp;
}

u8
hdmi2com_input_color_space_type(struct MTK_HDMI *myhdmi)
{
	u8 bReadData;
	u8 ret;

	if (hdmi2com_is_hdmi_mode(myhdmi)) {
		if (r_fld(AVIRX_VERS, CEA_AVI_HEADER_7_0_) != 0x82) {
			ret = 1;
		} else {
			bReadData = r_reg_1b(AVIRX_DBYTE2 + 2) & 0x60;
			if (bReadData == 0x00)
				ret = 1;
			else
				ret = 0;
		}
	} else
		ret = 1;

	return ret;
}

u8
hdmi2com_get_clocrimetry(struct MTK_HDMI *myhdmi)
{
	u8 bReadData;

	bReadData = r_reg_1b(AVIRX_DBYTE2 + 3);

	if ((bReadData & 0xc0) == 0x40)
		return 1;
	else
		return 0;
}

u8
hdmi2com_get_range_mode(struct MTK_HDMI *myhdmi)
{
	return myhdmi->HDMIRangeMode;
}

void
hdmi2com_set_range_mode(struct MTK_HDMI *myhdmi, u8 bMode)
{
	myhdmi->HDMIRangeMode = bMode;
}

u8
hdmi2com_get_rgb_range(struct MTK_HDMI *myhdmi)
{
	u8 bMode;

	if (!hdmi2com_is_hdmi_mode(myhdmi) ||
		(r_fld(AVIRX_VERS, CEA_AVI_HEADER_7_0_) != 0x82)) {
		myhdmi->HDMIRange = SV_HDMI_RANGE_FORCE_AUTO;
		return SV_HDMI_RANGE_FORCE_AUTO;
	}

	bMode = hdmi2com_get_range_mode(myhdmi);
	myhdmi->HDMIRange = (r_reg_1b(AVIRX_DBYTE6) & 0x0c) >> 2;

	switch (bMode) {
	default:
	case SV_HDMI_RANGE_FORCE_AUTO:
		return myhdmi->HDMIRange;
	case SV_HDMI_RANGE_FORCE_LIMIT:
		return SV_HDMI_RANGE_FORCE_LIMIT;

	case SV_HDMI_RANGE_FORCE_FULL:
		return SV_HDMI_RANGE_FORCE_FULL;
	}
}

void
hdmi2com_aud_fifo_err_clr(struct MTK_HDMI *myhdmi)
{
	w_fld(RX_INTR4, 1, REG_INTR4_STAT0);
	w_fld(RX_INTR4, 1, REG_INTR4_STAT1);
}

bool
hdmi2com_is_acr_lock(struct MTK_HDMI *myhdmi)
{
	u8 temp;

	temp = r_fld(TOP_MISC, ACR_PLL_LOCK);
	if (temp == 0)
		return FALSE;
	return TRUE;
}

void
hdmi2com_set_color_ralated(struct MTK_HDMI *myhdmi)
{
	u8 u1ColorInfo;
	bool bIs422 = FALSE;
	bool bIsRGB = TRUE;

	if (hdmi2com_is_hdmi_mode(myhdmi)) {
		if (r_fld(AVIRX_VERS, CEA_AVI_HEADER_7_0_) == 0x82) {
			u1ColorInfo = r_reg_1b(AVIRX_DBYTE2 + 2) & 0x60;
			if (u1ColorInfo == 0x20) /* 422 */
				bIs422 = TRUE;
			if (u1ColorInfo != 0x00) /* RGB */
				bIsRGB = FALSE;
		}
	}

	hdmi2com_vid_black_set(myhdmi, bIsRGB);
}

void
hdmi2com_info_status_clr(struct MTK_HDMI *myhdmi)
{
	myhdmi->Hdmi3DInfo.HDMI_3D_Enable = 0;
	myhdmi->Hdmi3DInfo.HDMI_3D_Video_Format = 0;
	myhdmi->Hdmi3DInfo.HDMI_3D_Str = 0;
	myhdmi->VsiInfo.u1InfoID = HDMI_INFO_FRAME_UNKNOWN;
	myhdmi->AviInfo.u1InfoID = HDMI_INFO_FRAME_UNKNOWN;
	myhdmi->HfVsiInfo.u1InfoID = HDMI_INFO_FRAME_UNKNOWN;
	myhdmi->UnrecInfo.u1InfoID = HDMI_INFO_FRAME_UNKNOWN;

	//w_fld(RX_UNREC_DEC, 0x87, REG_UNREC_DEC);
}

u8
hdmi2com_get_scan_info(struct MTK_HDMI *myhdmi)
{
	if (!hdmi2com_is_hdmi_mode(myhdmi) ||
		(r_fld(AVIRX_VERS, CEA_AVI_HEADER_7_0_)	!= 0x82))
		return 0;
	else
		return r_reg_1b(AVIRX_DBYTE2 + 2) & 0x03;
}

u8
hdmi2com_get_aspect_ratio(struct MTK_HDMI *myhdmi)
{
	if (!hdmi2com_is_hdmi_mode(myhdmi) ||
		(r_fld(AVIRX_VERS, CEA_AVI_HEADER_7_0_) != 0x82))
		return 0;
	else
		return (r_reg_1b(AVIRX_DBYTE2 + 3) & 0x30) >> 4;
}

u8
hdmi2com_get_afd_info(struct MTK_HDMI *myhdmi)
{
	if (!hdmi2com_is_hdmi_mode(myhdmi) ||
		(r_fld(AVIRX_VERS, CEA_AVI_HEADER_7_0_) != 0x82)) {
		return 0;
	} else {
		return r_reg_1b(AVIRX_DBYTE2 + 3) & 0x0f;
	}
}

u8
hdmi2com_is_422_input(struct MTK_HDMI *myhdmi)
{
	if (!hdmi2com_is_hdmi_mode(myhdmi) ||
		(r_fld(AVIRX_VERS, CEA_AVI_HEADER_7_0_) != 0x82))
		return 0;

	if (((r_reg_1b(AVIRX_DBYTE2 + 2) & 0x60) >> 5) == 1)
		return 1;

	return 0;
}

u8
hdmi2com_get_itc_flag(struct MTK_HDMI *myhdmi)
{
	if (!hdmi2com_is_hdmi_mode(myhdmi) ||
		(r_fld(AVIRX_VERS, CEA_AVI_HEADER_7_0_) != 0x82))
		return 0;

	return (r_reg_1b(AVIRX_DBYTE6) & 0x80) >> 7;
}

u8
hdmi2com_get_itc_content(struct MTK_HDMI *myhdmi)
{
	if (!hdmi2com_is_hdmi_mode(myhdmi) ||
		(r_fld(AVIRX_VERS, CEA_AVI_HEADER_7_0_) != 0x82))
		return 0;

	return (r_reg_1b(AVIRX_DBYTE6 + 2) & 0x30) >> 4;
}

void
hdmi2com_hdcp2x_irq_handle(struct MTK_HDMI *myhdmi)
{

}

void
hdmi2com_infoframe_init(struct MTK_HDMI *myhdmi)
{
	/* aud */
	w_fld(VSI_CTRL1, 1, REG_AIF_CLR_EN);
	/* vsi */
	w_fld(VSI_CTRL1, 1, REG_VSI_CLR_EN);
	/* avi */
	w_fld(IF_CTRL2, 1, REG_AVI_CLR_EN);
	/* spd */
	w_fld(IF_CTRL2, 1, REG_SPD_CLR_EN);
	/* mpeg */
	w_fld(IF_CTRL2, 1, REG_MPEG_CLR_EN);
	/* unrec */
	w_fld(IF_CTRL2, 1, REG_UNREC_CLR_EN);
	/* acp */
	w_fld(IF_CTRL2, 1, REG_ACP_CLR_EN);
	/* isrc1 */
	w_fld(IF_CTRL2, 1, REG_ISRC1_CLR_EN);
	/* isrc2 */
	w_fld(IF_CTRL2, 1, REG_ISRC2_CLR_EN);
	/* gcp */
	w_fld(IF_CTRL2, 1, REG_GCP_CLR_EN);
	/* meta data */
	w_fld(IF_CTRL2, 1, REG_META_DATA_PKT_CLR_EN);
	/* HF-VSI */
	w_fld(HF_VSIF_CTRL, 1, REG_HF_VSI_CLR_EN);
	/* 86 */
	w_fld(HEADER_0X86_CNTRL, 0, REG_HDR_ADDR_ADDR_0X86_CLR_DIRECT);
	w_fld(HEADER_0X86_CNTRL, 1, REG_HDR_ADDR_ADDR_0X86_CLR_EN);
	/* 87 */
	w_fld(HEADER_0X87_CNTRL, 0, REG_HDR_ADDR_0X87_CLR_DIRECT);
	w_fld(HEADER_0X87_CNTRL, 1, REG_HDR_ADDR_0X87_CLR_EN);

	/* muti-vsi*/
	w_fld(VSI_CTRL2, 1, REG_VSI_IEEE_ID_CHK_EN);

#if VFY_MULTI_VSI
	w_fld(THX0_VSI_CTRL, 1, THX_VSI_CLR_EN);
	w_fld(THX1_VSI_CTRL, 1, THX1_VSI_CLR_EN);
	w_fld(VER1_VSI_CTRL, 1, VER1_VSI_CLR_EN);
	w_fld(VER2_VSI_CTRL, 1, VER2_VSI_CLR_EN);

	w_fld(THX0_VSI_CTRL, 0x445566, THX_PB_IEEE_ID);
	w_fld(THX1_VSI_CTRL, 0xabcdef, THX1_PB_IEEE_ID);

	w_fld(VER1_VSI_CTRL, 0x123456, VER1_PB_IEEE_ID);
	w_fld(VSI_CTRL_BAK1, 1, VER1_VIDEO_VALUE);

	w_fld(VER2_VSI_CTRL, 0x112233, VER2_PB_IEEE_ID);
	w_fld(VSI_CTRL_BAK1, 2, VER2_VIDEO_VALUE);

	/*THX video/audio code*/
	w_fld(VSI_CTRL_BAK1, 0x93, THX_AIDO_VALUE);
	w_fld(VSI_CTRL_BAK1, 0xbc, THX1_VIDEO_VALUE);

	/*clear*/
	w_fld(VSI_CTRL_BAK2, 4, VSYNC_CLR_CNT);
#endif
	vHDMI2ComDoviEdrVsiEn(myhdmi);
}

void vHDMI2ComDoviEdrVsiEn(struct MTK_HDMI *myhdmi)
{
	//set vsi only parse vsi packet
	w_fld(VSI_CTRL2, 0x1, REG_VSI_IEEE_ID_CHK_EN);
	//set ver2 parse dolby vis packet
	w_fld(VER2_VSI_CTRL, 0x1, VER2_PACKET_FOR_NORMAL_USE);
	//w_fld(VER2_VSI_CTRL, 0x1, NEW_VER2_VSI_ONLY);
	w_fld(VER2_VSI_CTRL, 0x1, VER2_VSI_IEEE_ID_CHK_EN);
	w_fld(VER2_VSI_CTRL, 0x1, VER2_VSI_CLR_EN);
	w_fld(VER2_VSI_CTRL, 0x00D046, VER2_PB_IEEE_ID);
	w_fld(VSI_CTRL_BAK1, 0x1, VER2_VIDEO_VALUE);
}

void
hdmi2com_force_scdc(struct MTK_HDMI *myhdmi, u32 mode)
{
	myhdmi->force_40x = 0;

	if (mode == 1) {
		RX_DEF_LOG("[RX]force 1/40 and en\n");
		myhdmi->force_40x = 1;
		hdmi2com_force_scrambing_on_by_sw(myhdmi, TRUE);
	} else if (mode == 2) {
		RX_DEF_LOG("[RX]force 1/10 and en\n");
		myhdmi->force_40x = 0;
		hdmi2com_force_scrambing_on_by_sw(myhdmi, TRUE);
	} else if (mode == 3) {
		RX_DEF_LOG("[RX]force 1/40 and auto\n");
		myhdmi->force_40x = 1;
		hdmi2com_force_scrambing_on_by_sw(myhdmi, FALSE);
	} else if (mode == 4) {
		RX_DEF_LOG("[RX]force 1/10 and auto\n");
		myhdmi->force_40x = 0;
		hdmi2com_force_scrambing_on_by_sw(myhdmi, FALSE);
	} else if (mode == 5) {
		RX_DEF_LOG("[RX]force 1/10 and dis\n");
		myhdmi->force_40x = 0;
		hdmi2com_force_scrambing_on_by_sw(myhdmi, FALSE);
	} else {
		RX_DEF_LOG("[RX]clk auto and auto\n");
		myhdmi->force_40x = 0;
		hdmi2com_force_scrambing_on_by_sw(myhdmi, FALSE);
	}
}

void
hdmi2com_set_isr_enable(
	struct MTK_HDMI *myhdmi, bool enable, u32 en_mark)
{
	if (enable)
		myhdmi->isr_mark = myhdmi->isr_mark | en_mark;
	else
		myhdmi->isr_mark = myhdmi->isr_mark & (~en_mark);

	if (enable) {
		/* 2e020[24] */
		if (en_mark & HDMI_RX_CKDT_INT)
			w_fld(RX_INTR12 + 0x10, 1, REG_INTR12_STAT0);

		/* 2e020[25] */
		if (en_mark & HDMI_RX_SCDT_INT)
			w_fld(RX_INTR12 + 0x10, 1, REG_INTR12_STAT1);

		/* 2e018[14] */
		if (en_mark & HDMI_RX_VSYNC_INT)
			w_fld(RX_INTR4 + 0x10, 1, REG_INTR2_STAT6);

		/* 2e020[26] */
		if (en_mark & HDMI_RX_CDRADIO_INT)
			w_fld(RX_INTR12 + 0x10, 1, REG_INTR12_STAT2);

		/* 2e020[27] */
		if (en_mark & HDMI_RX_SCRAMBLING_INT)
			w_fld(RX_INTR12 + 0x10, 1, REG_INTR12_STAT3);

		/* 2e01c[0] */
		if (en_mark & HDMI_RX_FSCHG_INT)
			w_fld(RX_INTR8 + 0x10, 1, REG_INTR5_STAT0);

		/* 2e01c[9] */
		if (en_mark & HDMI_RX_DSDPKT_INT)
			w_fld(RX_INTR8 + 0x10, 1, REG_INTR6_STAT1);

		/* 2e01c[12] */
		if (en_mark & HDMI_RX_HBRPKT_INT)
			w_fld(RX_INTR8 + 0x10, 1, REG_INTR6_STAT4);

		/* 2e01c[31] */
		if (en_mark & HDMI_RX_CHCHG_INT)
			w_fld(RX_INTR8 + 0x10, 1, REG_INTR8_STAT7);

		/* 2e018[16] */
		if (en_mark & HDMI_RX_AVI_INT)
			w_fld(RX_INTR4 + 0x10, 1, REG_INTR3_STAT0);

		if (en_mark & HDMI_RX_HDMI_MODE_CHG)
			w_fld(RX_INTR4 + 0x10, 1, REG_INTR2_STAT7);

		if (en_mark & HDMI_RX_HDMI_NEW_GCP)
			w_fld(RX_INTR12 + 0x10, 1, REG_INTR9_STAT4);

		if (en_mark & HDMI_RX_PDTCLK_CHG)
			w_fld(RX_INTR8 + 0x10, 1, REG_INTR5_STAT7);

		if (en_mark & HDMI_RX_H_UNSTABLE)
			w_fld(RX_INTR13 + 0x10, 1, REG_INTR13_STAT4);

		if (en_mark & HDMI_RX_V_UNSTABLE)
			w_fld(RX_INTR13 + 0x10, 1, REG_INTR13_STAT5);

		if (en_mark & HDMI_RX_SET_MUTE)
			w_fld(RX_INTR4 + 0x10, 1, REG_INTR3_STAT6);

		if (en_mark & HDMI_RX_CLEAR_MUTE)
			w_fld(INTR_MASK_RESVD, 1, INTR_CP_CLR_MUTE_CHG_MASK);

		if (en_mark & HDMI_RX_VRR_INT) {
			w_reg(INTR_STATUS_RESVD,
				Fld2Msk32(INTR_EMP_VRR_PKT1_NEW));
			w_fld(INTR_MASK_RESVD, 1, INTR_EMP_VRR_PKT1_NEW_MASK);
		}
	} else {
		/* 2e020[24] */
		if (en_mark & HDMI_RX_CKDT_INT)
			w_fld(RX_INTR12 + 0x10, 0, REG_INTR12_STAT0);

		/* 2e020[25] */
		if (en_mark & HDMI_RX_SCDT_INT)
			w_fld(RX_INTR12 + 0x10, 0, REG_INTR12_STAT1);

		/* 2e018[14] */
		if (en_mark & HDMI_RX_VSYNC_INT)
			w_fld(RX_INTR4 + 0x10, 0, REG_INTR2_STAT6);

		/* 2e020[26] */
		if (en_mark & HDMI_RX_CDRADIO_INT)
			w_fld(RX_INTR12 + 0x10, 0, REG_INTR12_STAT2);

		/* 2e020[27] */
		if (en_mark & HDMI_RX_SCRAMBLING_INT)
			w_fld(RX_INTR12 + 0x10, 0, REG_INTR12_STAT3);

		/* 2e01c[0] */
		if (en_mark & HDMI_RX_FSCHG_INT)
			w_fld(RX_INTR8 + 0x10, 0, REG_INTR5_STAT0);

		/* 2e01c[9] */
		if (en_mark & HDMI_RX_DSDPKT_INT)
			w_fld(RX_INTR8 + 0x10, 0, REG_INTR6_STAT1);

		/* 2e01c[12] */
		if (en_mark & HDMI_RX_HBRPKT_INT)
			w_fld(RX_INTR8 + 0x10, 0, REG_INTR6_STAT4);

		/* 2e01c[31] */
		if (en_mark & HDMI_RX_CHCHG_INT)
			w_fld(RX_INTR8 + 0x10, 0, REG_INTR8_STAT7);

		/* 2e018[16] */
		if (en_mark & HDMI_RX_AVI_INT)
			w_fld(RX_INTR4 + 0x10, 0, REG_INTR3_STAT0);

		if (en_mark & HDMI_RX_HDMI_MODE_CHG)
			w_fld(RX_INTR4 + 0x10, 0, REG_INTR2_STAT7);

		if (en_mark & HDMI_RX_HDMI_NEW_GCP)
			w_fld(RX_INTR12 + 0x10, 0, REG_INTR9_STAT4);

		if (en_mark & HDMI_RX_PDTCLK_CHG)
			w_fld(RX_INTR8 + 0x10, 0, REG_INTR5_STAT7);

		if (en_mark & HDMI_RX_H_UNSTABLE)
			w_fld(RX_INTR13 + 0x10, 0, REG_INTR13_STAT4);

		if (en_mark & HDMI_RX_V_UNSTABLE)
			w_fld(RX_INTR13 + 0x10, 0, REG_INTR13_STAT5);

		if (en_mark & HDMI_RX_SET_MUTE)
			w_fld(RX_INTR4 + 0x10, 0, REG_INTR3_STAT6);

		if (en_mark & HDMI_RX_CLEAR_MUTE)
			w_fld(INTR_MASK_RESVD, 0, INTR_CP_CLR_MUTE_CHG_MASK);

		if (en_mark & HDMI_RX_VRR_INT)
			w_fld(INTR_MASK_RESVD, 0, INTR_EMP_VRR_PKT1_NEW_MASK);
	}
}

u32
hdmi2com_check_isr(struct MTK_HDMI *myhdmi)
{
	u32 isr_status;
	u32 u4IntSta1, u4IntSta5, u4IntSta9, u4IntSta13, eint_mark;

	u4IntSta1 = r_reg(RX_INTR4);
	eint_mark = r_reg(RX_INTR4 + 0x10);
	w_reg(RX_INTR4, eint_mark);

	u4IntSta5 = r_reg(RX_INTR8);
	eint_mark = r_reg(RX_INTR8 + 0x10);
	w_reg(RX_INTR8, eint_mark);

	u4IntSta9 = r_reg(RX_INTR12);
	eint_mark = r_reg(RX_INTR12 + 0x10);
	w_reg(RX_INTR12, eint_mark);

	u4IntSta13 = r_reg(RX_INTR13);
	eint_mark = r_reg(RX_INTR13 + 0x10);
	w_reg(RX_INTR13, eint_mark);

	isr_status = 0;

	/* 2e020[24] */
	if (myhdmi->isr_mark & HDMI_RX_CKDT_INT)
		if (u4IntSta9 & Fld2Msk32(REG_INTR12_STAT0))
			isr_status |= HDMI_RX_CKDT_INT;

	/* 2e020[25] */
	if (myhdmi->isr_mark & HDMI_RX_SCDT_INT)
		if (u4IntSta9 & Fld2Msk32(REG_INTR12_STAT1))
			isr_status |= HDMI_RX_SCDT_INT;

	/* 2e018[14] */
	if (myhdmi->isr_mark & HDMI_RX_VSYNC_INT)
		if (u4IntSta1 & Fld2Msk32(REG_INTR2_STAT6))
			isr_status |= HDMI_RX_VSYNC_INT;

	/* 2e020[26] */
	if (myhdmi->isr_mark & HDMI_RX_CDRADIO_INT)
		if (u4IntSta9 & Fld2Msk32(REG_INTR12_STAT2))
			isr_status |= HDMI_RX_CDRADIO_INT;

	/* 2e020[27] */
	if (myhdmi->isr_mark & HDMI_RX_SCRAMBLING_INT)
		if (u4IntSta9 & Fld2Msk32(REG_INTR12_STAT3))
			isr_status |= HDMI_RX_SCRAMBLING_INT;

	/* 2e01c[0] */
	if (myhdmi->isr_mark & HDMI_RX_FSCHG_INT)
		if (u4IntSta5 & Fld2Msk32(REG_INTR5_STAT0))
			isr_status |= HDMI_RX_FSCHG_INT;

	/* 2e01c[9] */
	if (myhdmi->isr_mark & HDMI_RX_DSDPKT_INT)
		if (u4IntSta5 & Fld2Msk32(REG_INTR6_STAT1))
			isr_status |= HDMI_RX_DSDPKT_INT;

	/* 2e01c[12] */
	if (myhdmi->isr_mark & HDMI_RX_HBRPKT_INT)
		if (u4IntSta5 & Fld2Msk32(REG_INTR6_STAT4))
			isr_status |= HDMI_RX_HBRPKT_INT;

	/* 2e01c[31] */
	if (myhdmi->isr_mark & HDMI_RX_CHCHG_INT)
		if (u4IntSta5 & Fld2Msk32(REG_INTR8_STAT7))
			isr_status |= HDMI_RX_CHCHG_INT;

	/* 2e018[16] */
	if (myhdmi->isr_mark & HDMI_RX_AVI_INT)
		if (u4IntSta1 & Fld2Msk32(REG_INTR3_STAT0))
			isr_status |= HDMI_RX_AVI_INT;

	if (myhdmi->isr_mark & HDMI_RX_HDMI_MODE_CHG)
		if (u4IntSta1 & Fld2Msk32(REG_INTR2_STAT7))
			isr_status |= HDMI_RX_HDMI_MODE_CHG;

	if (myhdmi->isr_mark & HDMI_RX_HDMI_NEW_GCP)
		if (u4IntSta9 & Fld2Msk32(REG_INTR9_STAT4))
			isr_status |= HDMI_RX_HDMI_NEW_GCP;

	if (myhdmi->isr_mark & HDMI_RX_PDTCLK_CHG)
		if (u4IntSta5 & Fld2Msk32(REG_INTR5_STAT7))
			isr_status |= HDMI_RX_PDTCLK_CHG;

	if (myhdmi->isr_mark & HDMI_RX_H_UNSTABLE)
		if (u4IntSta13 & Fld2Msk32(REG_INTR13_STAT4))
			isr_status |= HDMI_RX_H_UNSTABLE;

	if (myhdmi->isr_mark & HDMI_RX_V_UNSTABLE)
		if (u4IntSta13 & Fld2Msk32(REG_INTR13_STAT5))
			isr_status |= HDMI_RX_V_UNSTABLE;

	if (myhdmi->isr_mark & HDMI_RX_SET_MUTE)
		if (u4IntSta1 & Fld2Msk32(REG_INTR3_STAT6))
			isr_status |= HDMI_RX_SET_MUTE;

	return isr_status;
}

u32
hdmi2com_check_isr130(struct MTK_HDMI *myhdmi)
{
	u32 isr_status, eint_mark, temp;

	isr_status = r_reg(INTR_STATUS_RESVD);
	eint_mark = r_reg(INTR_MASK_RESVD);
	w_reg(INTR_STATUS_RESVD, eint_mark);

	temp = 0;

	if (myhdmi->isr_mark & HDMI_RX_VSI_INT)
		if (isr_status & Fld2Msk32(INTR_CEA_VER1_VSI_NEW))
			temp = temp | HDMI_RX_VSI_INT;

	if (myhdmi->isr_mark & HDMI_RX_EMP_INT)
		if (isr_status & Fld2Msk32(INTR_MTW_PULSE))
			temp = temp | HDMI_RX_EMP_INT;

	if (myhdmi->isr_mark & HDMI_RX_VRR_INT)
		if (isr_status & Fld2Msk32(INTR_EMP_VRR_PKT1_NEW))
			temp = temp | HDMI_RX_VRR_INT;

	if (myhdmi->isr_mark & HDMI_RX_CLEAR_MUTE)
		if (isr_status & Fld2Msk32(INTR_CP_CLR_MUTE_CHG))
			temp = temp | HDMI_RX_CLEAR_MUTE;

	return temp;
}

void
hdmi2com_write_edid_to_sram(struct MTK_HDMI *myhdmi,
	u8 block_num,
	u8 *pu1Edid)
{
	u32 i = 0;

	if (pu1Edid == NULL) {
		RX_DEF_LOG(
			"[RX]HDMI20 write EDID, Fail: pu1Edid is null!\n");
		return;
	}

	w_fld(EDID_ADDR, 1, FLD_EDID2_ADDR_FIX);
	w_fld(EDID_DEV0, 1, FLD_EDID2_DEGLITCH0_EN);
	w_fld(EDID_DEV0, 1, FLD_EDID2_EDID0_DIS);

	if (block_num == 0) {
		for (i = 0; i < 128; i++) {
			w_fld(EDID_ADDR, i, FLD_EDID2_EDID_ADDR);
			w_fld(EDID_DATA, pu1Edid[i], FLD_EDID2_EDID_DATA);
		}
	} else if (block_num == 1) {
		for (i = 0; i < 128; i++) {
			w_fld(EDID_ADDR, (i + 128), FLD_EDID2_EDID_ADDR);
			w_fld(EDID_DATA, pu1Edid[i], FLD_EDID2_EDID_DATA);
		}
	} else if (block_num == 2) {
		for (i = 0; i < 128; i++) {
			w_fld(EDID_ADDR, (i + 256), FLD_EDID2_EDID_ADDR);
			w_fld(EDID_DATA, pu1Edid[i], FLD_EDID2_EDID_DATA);
		}
	} else if (block_num == 3) {
		for (i = 0; i < 128; i++) {
			w_fld(EDID_ADDR, (i + 384), FLD_EDID2_EDID_ADDR);
			w_fld(EDID_DATA, pu1Edid[i], FLD_EDID2_EDID_DATA);
		}
	}

	w_fld(EDID_DEV0, 0, FLD_EDID2_EDID0_DIS);
}

u8
hdmi2com_char_to_ascii(u8 in_d)
{
	in_d = in_d & 0xF;
	if (in_d <= 9)
		return in_d + 0x30;
	return in_d - 10 + 0x41;
}

void
hdmi2com_edid_char_to_ascii(
	u8 index, u8 *in_p, u8 *out_p)
{
	u8 i;

	out_p[0] = hdmi2com_char_to_ascii(index >> 4);
	out_p[1] = hdmi2com_char_to_ascii(index);
	out_p[2] = ':';
	for (i = 0; i < 16; i++) {
		out_p[3 + 3 * i] = hdmi2com_char_to_ascii(in_p[i] >> 4);
		out_p[3 + 3 * i + 1] = hdmi2com_char_to_ascii(in_p[i]);
		out_p[3 + 3 * i + 2] = ' ';
	}
	out_p[50] = 0;
}

void
hdmi2com_show_edid_raw_data(
	struct MTK_HDMI *myhdmi, u8 block, u8 *edid_p)
{
	u8 i;
	char pbuf[51];

	RX_DEF_LOG("[RX]EDID Block Number=#%d\n", block);

	for (i = 0; i < 8; i++) {
		hdmi2com_edid_char_to_ascii(i, &edid_p[16 * i], pbuf);
		RX_DEF_LOG("%s\n", pbuf);
	}
}

void
hdmi2com_show_sram_edid(struct MTK_HDMI *myhdmi, u8 block_num)
{
	u32 i;
	u8 edid_buf[128];

	memset(&edid_buf[0], 0, 128);

	w_fld(EDID_ADDR, 1, FLD_EDID2_ADDR_FIX);
	w_fld(EDID_DEV0, 1, FLD_EDID2_DEGLITCH0_EN);
	w_fld(EDID_DEV0, 1, FLD_EDID2_EDID0_DIS);

	if (block_num == 0) {
		for (i = 0; i < 128; i++) {
			w_fld(EDID_ADDR, i, FLD_EDID2_EDID_ADDR);
			edid_buf[i] = r_reg(EDID_DATA) & 0xff;
		}

	} else if (block_num == 1) {
		for (i = 0; i < 128; i++) {
			w_fld(EDID_ADDR, (i + 128), FLD_EDID2_EDID_ADDR);
			edid_buf[i] = r_reg(EDID_DATA) & 0xff;
		}
	} else if (block_num == 2) {
		for (i = 0; i < 128; i++) {
			w_fld(EDID_ADDR, (i + 256), FLD_EDID2_EDID_ADDR);
			edid_buf[i] = r_reg(EDID_DATA) & 0xff;
		}
	} else if (block_num == 3) {
		for (i = 0; i < 128; i++) {
			w_fld(EDID_ADDR, (i + 384), FLD_EDID2_EDID_ADDR);
			edid_buf[i] = r_reg(EDID_DATA) & 0xff;
		}
	}

	w_fld(EDID_DEV0, 0, FLD_EDID2_EDID0_DIS);

	hdmi2com_show_edid_raw_data(myhdmi, block_num, &edid_buf[0]);
}

void
hdmi2com_set_rx_port1_hpd_level(struct MTK_HDMI *myhdmi, bool fgHighLevel)
{
	if (fgHighLevel)
		w_fld(EARC_RX_CFG_2, 1, RG_SW_HPD_ON);
	else
		w_fld(EARC_RX_CFG_2, 0, RG_SW_HPD_ON);
}

bool
hdmi2com_get_rx_port1_hpd_level(struct MTK_HDMI *myhdmi)
{
	if (r_fld(EARC_RX_CFG_2, RG_SW_HPD_ON) == 0)
		return FALSE;
	return TRUE;
}

void
hdmi2com_cfg_out(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->hdmi_mode_setting == HDMI_RPT_TMDS_MODE) {
		RX_INFO_LOG("[RX]HDMI_RPT_TMDS_MODE\n");
		w_fld(HDMIRX_TOP_CONFIG, 0, G_RSTB_FIFO);
		w_fld(HDMIRX_TOP_CONFIG, 1, RG_RXTX_FIFO_SEL);
		w_fld(HDMIRX_TOP_CONFIG, 1, G_RSTB_FIFO);
	} else if (myhdmi->hdmi_mode_setting == HDMI_RPT_PIXEL_MODE) {
		RX_INFO_LOG("[RX]HDMI_RPT_PIXEL_MODE\n");
		w_fld(HDMIRX_TOP_CONFIG, 0, G_RSTB_FIFO);
		w_fld(HDMIRX_TOP_CONFIG, 0, RG_RXTX_FIFO_SEL);
		w_fld(HDMIRX_TOP_CONFIG, 1, G_RSTB_FIFO);
	} else {
		RX_INFO_LOG("[RX]can not support this output config\n");
	}
}

u32 hdmi2com_bch_err(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	w_fld(RX_PKT_THRESH2, 0x1, REG_CAPTURE_CNT);
	temp = r_reg(RX_BCH_ERR2);
	return temp;
}

void hdmi2com_clear_ced(struct MTK_HDMI *myhdmi)
{
	w_reg(RX_ADR + 0x0E8, 0x7);
	w_reg(RX_ADR + 0x0E8, 0);
}

u32 hdmi2com_get_ced(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = r_fld(SCDC_REG12, SCDCS_CH0_CED_CNT);
	temp += r_fld(SCDC_REG13, SCDCS_CH1_CED_CNT);
	temp += r_fld(SCDC_REG13, SCDCS_CH2_CED_CNT);

	return temp;
}
