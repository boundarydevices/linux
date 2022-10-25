// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Copyright (c) 2021 BayLibre, SAS
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/pm_wakeup.h>
#include <linux/timer.h>

#include <drm/drm_displayid.h>
#include <drm/drm_edid.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_scdc_helper.h>

#include "mtk_drm_crtc.h"
#include "mtk_mt8195_hdmi.h"
#include "mtk_hdmi_common.h"
#include "mtk_mt8195_hdmi_ddc.h"
#include "mtk_mt8195_hdmi_regs.h"

#define RGB444_8bit BIT(0)
#define RGB444_10bit BIT(1)
#define RGB444_12bit BIT(2)
#define RGB444_16bit BIT(3)

#define YCBCR444_8bit BIT(4)
#define YCBCR444_10bit BIT(5)
#define YCBCR444_12bit BIT(6)
#define YCBCR444_16bit BIT(7)

#define YCBCR422_8bit_NO_SUPPORT BIT(8)
#define YCBCR422_10bit_NO_SUPPORT BIT(9)
#define YCBCR422_12bit BIT(10)
#define YCBCR422_16bit_NO_SUPPORT BIT(11)

#define YCBCR420_8bit BIT(12)
#define YCBCR420_10bit BIT(13)
#define YCBCR420_12bit BIT(14)
#define YCBCR420_16bit BIT(15)

#define BYTES_TO_UINT32(msb, b1, b2, lsb)                                      \
	(((msb & 0xff) << 24) + ((b1 & 0xff) << 16) + ((b2 & 0xff) << 8) +     \
	 ((lsb & 0xff)))

// Define MTK hardware-specific buffer size for HW alignment required.
// It greater than or equal to the size defined in HDMI specification.
#define MT8195_HDMI_SPD_BUFFER_SIZE (31)
#define MT8195_HDMI_AVI_BUFFER_SIZE (17)

static inline struct mtk_hdmi *hdmi_ctx_from_conn(struct drm_connector *c)
{
	return container_of(c, struct mtk_hdmi, conn);
}

static inline void mtk_hdmi_clr_all_int_status(struct mtk_hdmi *hdmi)
{
	/*clear all tx irq*/
	mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0xffffffff);
	mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0x00000000);
	mtk_hdmi_write(hdmi, TOP_INT_CLR01, 0xffffffff);
	mtk_hdmi_write(hdmi, TOP_INT_CLR01, 0x00000000);
}

static inline void mtk_hdmi_disable_all_int(struct mtk_hdmi *hdmi)
{
	/*disable all tx irq*/
	mtk_hdmi_write(hdmi, TOP_INT_MASK00, 0x00000000);
	mtk_hdmi_write(hdmi, TOP_INT_MASK01, 0x00000000);
}

static inline void mtk_hdmi_en_hdcp_reauth_int(struct mtk_hdmi *hdmi,
					       bool enable)
{
	if (enable)
		mtk_hdmi_mask(hdmi, TOP_INT_MASK00,
			      HDCP2X_RX_REAUTH_REQ_DDCM_INT_UNMASK,
			      HDCP2X_RX_REAUTH_REQ_DDCM_INT_UNMASK);
	else
		mtk_hdmi_mask(hdmi, TOP_INT_MASK00,
			      HDCP2X_RX_REAUTH_REQ_DDCM_INT_MASK,
			      HDCP2X_RX_REAUTH_REQ_DDCM_INT_UNMASK);
}

static inline void mtk_hdmi_enable_hpd_pord_irq(struct mtk_hdmi *hdmi,
						bool enable)
{
	if (enable)
		mtk_hdmi_mask(hdmi, TOP_INT_MASK00, 0x0000000f, 0x0000000f);
	else
		mtk_hdmi_mask(hdmi, TOP_INT_MASK00, 0x00000000, 0x0000000f);
}

static inline void mtk_hdmi_clr_htplg_pord_irq(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_mask(hdmi, TOP_INT_CLR00, 0x0000000f, 0x0000000f);
	mtk_hdmi_mask(hdmi, TOP_INT_CLR00, 0x00000000, 0x0000000f);
}

static inline void mtk_hdmi_set_sw_hpd(struct mtk_hdmi *hdmi, bool high)
{
	if (high)
		mtk_hdmi_mask(hdmi, hdmi->conf->reg_hdmitx_config_ofs, 0x1 << HDMITX_SW_HPD_SHIFT,
			      HDMITX_SW_HPD);
	else
		mtk_hdmi_mask(hdmi, hdmi->conf->reg_hdmitx_config_ofs, 0x0 << HDMITX_SW_HPD_SHIFT,
			      HDMITX_SW_HPD);
}

static inline void mtk_hdmi_force_hdcp_hpd(struct mtk_hdmi *hdmi)
{
	/* force HDCP HPD to 1*/
	mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, HDCP2X_HPD_OVR, HDCP2X_HPD_OVR);
	mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, HDCP2X_HPD_SW, HDCP2X_HPD_SW);
}

static void mtk_hdmi_disable_hdcp_encrypt(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, 0x0 << HDCP2X_ENCRYPT_EN_SHIFT,
		      HDCP2X_ENCRYPT_EN);
	mtk_hdmi_mask(hdmi, HDCP1X_CTRL, 0x0 << HDCP1X_ENC_EN_SHIFT,
		      HDCP1X_ENC_EN);
}

static void mtk_hdmi_yuv420_downsample(struct mtk_hdmi *hdmi, bool enable)
{
	if (enable) {
		mtk_hdmi_mask(hdmi, hdmi->conf->reg_hdmitx_config_ofs,
			      HDMI_YUV420_MODE | HDMITX_SW_HPD,
			      HDMI_YUV420_MODE | HDMITX_SW_HPD);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG,
			      C444_C422_CONFIG_ENABLE, C444_C422_CONFIG_ENABLE);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG,
			      C422_C420_CONFIG_ENABLE, C422_C420_CONFIG_ENABLE);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG, 0,
			      C422_C420_CONFIG_BYPASS);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG,
			      C422_C420_CONFIG_OUT_CB_OR_CR,
			      C422_C420_CONFIG_OUT_CB_OR_CR);
		mtk_hdmi_mask(hdmi, VID_OUT_FORMAT,
			      OUTPUT_FORMAT_DEMUX_420_ENABLE,
			      OUTPUT_FORMAT_DEMUX_420_ENABLE);
	} else {
		mtk_hdmi_mask(hdmi, hdmi->conf->reg_hdmitx_config_ofs, 0 | HDMITX_SW_HPD,
			      HDMI_YUV420_MODE | HDMITX_SW_HPD);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG, 0,
			      C444_C422_CONFIG_ENABLE);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG, 0,
			      C422_C420_CONFIG_ENABLE);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG,
			      C422_C420_CONFIG_BYPASS, C422_C420_CONFIG_BYPASS);
		mtk_hdmi_mask(hdmi, VID_DOWNSAMPLE_CONFIG, 0,
			      C422_C420_CONFIG_OUT_CB_OR_CR);
		mtk_hdmi_mask(hdmi, VID_OUT_FORMAT, 0,
			      OUTPUT_FORMAT_DEMUX_420_ENABLE);
	}
}

static bool mtk_hdmi_tmds_over_340M(struct mtk_hdmi *hdmi)
{
	unsigned long pixel_clk, tmds_clk;

	pixel_clk = hdmi->mode.clock * 1000;

	/* TMDS clk frequency */
	if (hdmi->color_depth == HDMI_8_BIT)
		tmds_clk = pixel_clk;
	else if (hdmi->color_depth == HDMI_10_BIT)
		tmds_clk = pixel_clk * 5 / 4;
	else if (hdmi->color_depth == HDMI_12_BIT)
		tmds_clk = pixel_clk * 3 / 2;
	else if (hdmi->color_depth == HDMI_16_BIT)
		tmds_clk = pixel_clk * 2;
	else
		return -EINVAL;

	if (tmds_clk >= 340000000 && hdmi->csp != HDMI_COLORSPACE_YUV420)
		return true;

	return false;
}

static inline void mtk_hdmi_enable_scrambling(struct mtk_hdmi *hdmi,
					      bool enable)
{
	usleep_range(100, 150);

	if (enable)
		mtk_hdmi_mask(hdmi, TOP_CFG00, SCR_ON | HDMI2_ON,
			      SCR_ON | HDMI2_ON);
	else
		mtk_hdmi_mask(hdmi, TOP_CFG00, SCR_OFF | HDMI2_OFF,
			      SCR_ON | HDMI2_ON);
}

static void mtk_hdmi_hw_vid_black(struct mtk_hdmi *hdmi, bool black)
{
	if (black)
		mtk_hdmi_mask(hdmi, TOP_VMUTE_CFG1, REG_VMUTE_EN, REG_VMUTE_EN);
	else
		mtk_hdmi_mask(hdmi, TOP_VMUTE_CFG1, 0, REG_VMUTE_EN);
}

static void mtk_hdmi_hw_aud_mute(struct mtk_hdmi *hdmi)
{
	if (mtk_hdmi_read(hdmi, AIP_CTRL) & DSD_EN)
		mtk_hdmi_mask(hdmi, AIP_TXCTRL,
			      DSD_MUTE_DATA | AUD_MUTE_FIFO_EN,
			      DSD_MUTE_DATA | AUD_MUTE_FIFO_EN);
	else
		mtk_hdmi_mask(hdmi, AIP_TXCTRL, AUD_MUTE_FIFO_EN,
			      AUD_MUTE_FIFO_EN);
}

static void mtk_hdmi_hw_aud_unmute(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_mask(hdmi, AIP_TXCTRL, AUD_MUTE_DIS, AUD_MUTE_FIFO_EN);
}

static void mtk_hdmi_hw_reset(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_mask(hdmi, hdmi->conf->reg_hdmitx_config_ofs, 0x0 << HDMITX_SW_RSTB_SHIFT,
		      HDMITX_SW_RSTB);
	usleep_range(1, 5);
	mtk_hdmi_mask(hdmi, hdmi->conf->reg_hdmitx_config_ofs, 0x1 << HDMITX_SW_RSTB_SHIFT,
		      HDMITX_SW_RSTB);
}

static void mtk_hdmi_enable_hdmi_mode(struct mtk_hdmi *hdmi, bool enable)
{
	if (enable)
		mtk_hdmi_mask(hdmi, TOP_CFG00, HDMI_MODE_HDMI, HDMI_MODE_HDMI);
	else
		mtk_hdmi_mask(hdmi, TOP_CFG00, HDMI_MODE_DVI, HDMI_MODE_HDMI);
}

static bool mtk_hdmi_sink_is_hdmi_device(struct mtk_hdmi *hdmi)
{
	if (hdmi->dvi_mode)
		return false;
	else
		return true;
}

static void mtk_hdmi_set_deep_color(struct mtk_hdmi *hdmi, bool is_hdmi_sink)
{
	unsigned int deep_color = 0;

	/* ycbcr422 12bit no deep color */
	if (hdmi->csp == HDMI_COLORSPACE_YUV422) {
		deep_color = DEEPCOLOR_MODE_8BIT;
	} else {
		switch (hdmi->color_depth) {
		case HDMI_8_BIT:
			deep_color = DEEPCOLOR_MODE_8BIT;
			break;
		case HDMI_10_BIT:
			deep_color = DEEPCOLOR_MODE_10BIT;
			break;
		case HDMI_12_BIT:
			deep_color = DEEPCOLOR_MODE_12BIT;
			break;
		case HDMI_16_BIT:
			deep_color = DEEPCOLOR_MODE_16BIT;
			break;
		default:
			WARN(1, "Unssupported color depth %d\n",
			     hdmi->color_depth);
		}
	}

	mtk_hdmi_mask(hdmi, TOP_CFG00, deep_color, DEEPCOLOR_MODE_MASKBIT);

	/* GCP */
	mtk_hdmi_mask(hdmi, TOP_CFG00, 0, DEEPCOLOR_PAT_EN);
	if (is_hdmi_sink && deep_color != DEEPCOLOR_MODE_8BIT)
		mtk_hdmi_mask(hdmi, TOP_MISC_CTLR, DEEP_COLOR_ADD,
			      DEEP_COLOR_ADD);
	else
		mtk_hdmi_mask(hdmi, TOP_MISC_CTLR, 0, DEEP_COLOR_ADD);
}

static void mtk_hdmi_hw_audio_infoframe(struct mtk_hdmi *hdmi, u8 *buffer,
					u8 len)
{
	enum hdmi_infoframe_type frame_type;
	u8 frame_ver;
	u8 frame_len;
	u8 checksum;

	frame_type = buffer[0];
	frame_ver = buffer[1];
	frame_len = buffer[2];
	checksum = buffer[3];

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, AUD_DIS_WR | AUD_DIS,
		      AUD_EN_WR | AUD_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, AUD_RPT_DIS, AUD_RPT_EN);

	mtk_hdmi_write(hdmi, TOP_AIF_HEADER,
		       BYTES_TO_UINT32(0, frame_len, frame_ver, frame_type));
	mtk_hdmi_write(hdmi, TOP_AIF_PKT00,
		       BYTES_TO_UINT32(buffer[6], buffer[5], buffer[4],
				       buffer[3]));
	mtk_hdmi_write(hdmi, TOP_AIF_PKT01,
		       BYTES_TO_UINT32(0, 0, buffer[8], buffer[7]));
	mtk_hdmi_write(hdmi, TOP_AIF_PKT02, 0);
	mtk_hdmi_write(hdmi, TOP_AIF_PKT03, 0);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, AUD_RPT_EN, AUD_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, AUD_EN_WR | AUD_EN,
		      AUD_EN_WR | AUD_EN);
}

static void mtk_hdmi_hw_avi_infoframe(struct mtk_hdmi *hdmi, u8 *buffer, u8 len)
{
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, AVI_DIS_WR | AVI_DIS,
		      AVI_EN_WR | AVI_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, AVI_RPT_DIS, AVI_RPT_EN);

	mtk_hdmi_write(hdmi, TOP_AVI_HEADER,
		       BYTES_TO_UINT32(0, buffer[2], buffer[1], buffer[0]));

	mtk_hdmi_write(hdmi, TOP_AVI_PKT00,
		       BYTES_TO_UINT32(buffer[6], buffer[5], buffer[4],
				       buffer[3]));

	mtk_hdmi_write(hdmi, TOP_AVI_PKT01,
		       BYTES_TO_UINT32(0, buffer[9], buffer[8], buffer[7]));

	mtk_hdmi_write(hdmi, TOP_AVI_PKT02,
		       BYTES_TO_UINT32(buffer[13], buffer[12], buffer[11],
				       buffer[10]));

	mtk_hdmi_write(hdmi, TOP_AVI_PKT03,
		       BYTES_TO_UINT32(0, buffer[16], buffer[15], buffer[14]));

	mtk_hdmi_write(hdmi, TOP_AVI_PKT04, 0);
	mtk_hdmi_write(hdmi, TOP_AVI_PKT05, 0);

	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, AVI_RPT_EN, AVI_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, AVI_EN_WR | AVI_EN,
		      AVI_EN_WR | AVI_EN);
}

static void mtk_hdmi_hw_spd_infoframe(struct mtk_hdmi *hdmi, u8 *buffer, u8 len)
{
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, SPD_DIS_WR | SPD_DIS,
		      SPD_EN_WR | SPD_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, SPD_RPT_DIS, SPD_RPT_EN);

	mtk_hdmi_write(hdmi, TOP_SPDIF_HEADER,
		       BYTES_TO_UINT32(0, buffer[2], buffer[1], buffer[0]));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT00,
		       BYTES_TO_UINT32(buffer[6], buffer[5], buffer[4],
				       buffer[3]));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT01,
		       BYTES_TO_UINT32(0, buffer[9], buffer[8], buffer[7]));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT02,
		       BYTES_TO_UINT32(buffer[13], buffer[12], buffer[11],
				       buffer[10]));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT03,
		       BYTES_TO_UINT32(0, buffer[16], buffer[15], buffer[14]));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT04,
		       BYTES_TO_UINT32(buffer[20], buffer[19], buffer[18],
				       buffer[17]));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT05,
		       BYTES_TO_UINT32(0, buffer[23], buffer[22], buffer[21]));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT06,
		       BYTES_TO_UINT32(buffer[27], buffer[26], buffer[25],
				       buffer[24]));

	mtk_hdmi_write(hdmi, TOP_SPDIF_PKT07,
		       BYTES_TO_UINT32(0, buffer[30], buffer[29], buffer[28]));

	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, SPD_RPT_EN, SPD_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, SPD_EN_WR | SPD_EN,
		      SPD_EN_WR | SPD_EN);
}

static int mtk_hdmi_setup_audio_infoframe(struct mtk_hdmi *hdmi)
{
	struct hdmi_codec_params *params = &hdmi->aud_param.codec_params;
	struct hdmi_audio_infoframe frame;
	u8 buffer[14];
	ssize_t err;

	memcpy(&frame, &params->cea, sizeof(struct hdmi_audio_infoframe));

	err = hdmi_audio_infoframe_pack(&frame, buffer, sizeof(buffer));
	if (err < 0)
		return err;

	mtk_hdmi_hw_audio_infoframe(hdmi, buffer, sizeof(buffer));
	return 0;
}

static void mtk_hdmi_hw_send_aud_packet(struct mtk_hdmi *hdmi, bool enable)
{
	if (enable)
		mtk_hdmi_mask(hdmi, AIP_TXCTRL, 0, AUD_PACKET_DROP);
	else
		mtk_hdmi_mask(hdmi, AIP_TXCTRL, AUD_PACKET_DROP,
			      AUD_PACKET_DROP);
}

static inline void mtk_hdmi_hw_send_av_mute(struct mtk_hdmi *hdmi)
{
	/*GCP packet */
	mtk_hdmi_mask(hdmi, TOP_CFG01, 0, CP_CLR_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_CFG01, 0, CP_SET_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, CP_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, CP_EN | CP_EN_WR);

	mtk_hdmi_mask(hdmi, TOP_CFG01, 0, CP_CLR_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_CFG01, CP_SET_MUTE_EN, CP_SET_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, CP_RPT_EN, CP_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, CP_EN | CP_EN_WR, CP_EN | CP_EN_WR);
}

static inline void mtk_hdmi_hw_send_av_unmute(struct mtk_hdmi *hdmi)
{
	/*GCP packet */
	mtk_hdmi_mask(hdmi, TOP_CFG01, 0, CP_CLR_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_CFG01, 0, CP_SET_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, CP_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, CP_EN | CP_EN_WR);

	mtk_hdmi_mask(hdmi, TOP_CFG01, CP_CLR_MUTE_EN, CP_CLR_MUTE_EN);
	mtk_hdmi_mask(hdmi, TOP_CFG01, 0, CP_SET_MUTE_DIS);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, CP_RPT_EN, CP_RPT_EN);
	mtk_hdmi_mask(hdmi, TOP_INFO_EN, CP_EN | CP_EN_WR, CP_EN | CP_EN_WR);
}

static void mtk_hdmi_hw_ncts_enable(struct mtk_hdmi *hdmi, bool enable)
{
	unsigned int data;

	data = mtk_hdmi_read(hdmi, AIP_CTRL);

	if (enable)
		data |= CTS_SW_SEL;
	else
		data &= ~CTS_SW_SEL;

	mtk_hdmi_write(hdmi, AIP_CTRL, data);
}

static void mtk_hdmi_hw_aud_set_channel_status(struct mtk_hdmi *hdmi,
					       u8 *channel_status)
{
	/* actually, only the first 5 or 7 bytes of Channel Status
	 * contain useful information
	 */
	mtk_hdmi_write(hdmi, AIP_I2S_CHST0,
		       BYTES_TO_UINT32(channel_status[3], channel_status[2],
				       channel_status[1], channel_status[0]));
	mtk_hdmi_write(hdmi, AIP_I2S_CHST1,
		       BYTES_TO_UINT32(0, channel_status[6], channel_status[5],
				       channel_status[4]));
}

struct hdmi_acr_n {
	unsigned int clock;
	unsigned int n[3];
};

/* Recommended N values from HDMI specification, tables 7-1 to 7-3 */
static const struct hdmi_acr_n hdmi_rec_n_table[] = {
	/* Clock, N: 32kHz 44.1kHz 48kHz */
	{ 25175, { 4576, 7007, 6864 } },
	{ 74176, { 11648, 17836, 11648 } },
	{ 148352, { 11648, 8918, 5824 } },
	{ 296703, { 5824, 4459, 5824 } },
	{ 297000, { 3072, 4704, 5120 } },
	{ 0, { 4096, 6272, 6144 } }, /* all other TMDS clocks */
};

/**
 * hdmi_recommended_n() - Return N value recommended by HDMI specification
 * @freq: audio sample rate in Hz
 * @clock: rounded TMDS clock in kHz
 */
static int hdmi_recommended_n(unsigned int freq, unsigned int clock)
{
	const struct hdmi_acr_n *recommended;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(hdmi_rec_n_table) - 1; i++) {
		if (clock == hdmi_rec_n_table[i].clock)
			break;
	}

	if (i == ARRAY_SIZE(hdmi_rec_n_table))
		return -EINVAL;

	recommended = hdmi_rec_n_table + i;

	switch (freq) {
	case 32000:
		return recommended->n[0];
	case 44100:
		return recommended->n[1];
	case 48000:
		return recommended->n[2];
	case 88200:
		return recommended->n[1] * 2;
	case 96000:
		return recommended->n[2] * 2;
	case 176400:
		return recommended->n[1] * 4;
	case 192000:
		return recommended->n[2] * 4;
	default:
		return (128 * freq) / 1000;
	}
}

static unsigned int hdmi_mode_clock_to_hz(unsigned int clock)
{
	switch (clock) {
	case 25175:
		return 25174825; /* 25.2/1.001 MHz */
	case 74176:
		return 74175824; /* 74.25/1.001 MHz */
	case 148352:
		return 148351648; /* 148.5/1.001 MHz */
	case 296703:
		return 296703297; /* 297/1.001 MHz */
	default:
		return clock * 1000;
	}
}

static unsigned int hdmi_expected_cts(unsigned int audio_sample_rate,
				      unsigned int tmds_clock, unsigned int n)
{
	return DIV_ROUND_CLOSEST_ULL((u64)hdmi_mode_clock_to_hz(tmds_clock) * n,
				     128 * audio_sample_rate);
}

static void mtk_hdmi_hw_aud_set_ncts(struct mtk_hdmi *hdmi,
				     unsigned int sample_rate,
				     unsigned int clock)
{
	unsigned int ncts;
	int n;

	n = hdmi_recommended_n(sample_rate, clock);

	if (n == -EINVAL) {
		DRM_ERROR("Invalid sample rate: %u\n", sample_rate);
		return;
	}

	ncts = hdmi_expected_cts(sample_rate, clock, n);
	mtk_hdmi_write(hdmi, AIP_N_VAL, n);
	mtk_hdmi_write(hdmi, AIP_CTS_SVAL, ncts);
}

static void mtk_hdmi_aud_enable_packet(struct mtk_hdmi *hdmi, bool enable)
{
	mtk_hdmi_hw_send_aud_packet(hdmi, enable);
}

static void mtk_hdmi_aud_on_off_hw_ncts(struct mtk_hdmi *hdmi, bool on)
{
	mtk_hdmi_hw_ncts_enable(hdmi, on);
}

static void mtk_hdmi_audio_dsd_config(struct mtk_hdmi *hdmi,
				      unsigned char chnum, bool dsd_bypass)
{
	mtk_hdmi_mask(hdmi, AIP_CTRL, DSD_EN, SPDIF_EN | DSD_EN | HBRA_ON);
	mtk_hdmi_mask(hdmi, AIP_TXCTRL, DSD_MUTE_DATA, DSD_MUTE_DATA);
	if (dsd_bypass)
		mtk_hdmi_write(hdmi, TOP_AUD_MAP, 0x75316420);
	else
		mtk_hdmi_write(hdmi, TOP_AUD_MAP, 0x04230150);

	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, 0, I2S2DSD_EN);
}

static inline void mtk_hdmi_hw_i2s_fifo_map(struct mtk_hdmi *hdmi,
					    unsigned int fifo_mapping)
{
	mtk_hdmi_mask(hdmi, AIP_I2S_CTRL, fifo_mapping,
		      FIFO3_MAP | FIFO2_MAP | FIFO1_MAP | FIFO0_MAP);
}

static inline void mtk_hdmi_hw_i2s_ch_number(struct mtk_hdmi *hdmi,
					     unsigned int chnum)
{
	mtk_hdmi_mask(hdmi, AIP_CTRL, chnum << I2S_EN_SHIFT, I2S_EN);
}

static void mtk_hdmi_hw_i2s_ch_mapping(struct mtk_hdmi *hdmi,
				       unsigned char chnum,
				       unsigned char mapping)
{
	unsigned int bdata;

	switch (chnum) {
	case 2:
		bdata = 0x1;
		break;
	case 3:
		bdata = 0x3;
		break;
	case 6:
		if (mapping == 0x0E) {
			bdata = 0xf;
			break;
		}
		fallthrough;
	case 5:
		bdata = 0x7;
		break;
	case 7:
	case 8:
		bdata = 0xf;
		break;
	default:
		bdata = 0x1;
	}

	mtk_hdmi_hw_i2s_fifo_map(hdmi, (MAP_SD3 << 6) | (MAP_SD2 << 4) |
					       (MAP_SD1 << 2) | (MAP_SD0 << 0));
	mtk_hdmi_hw_i2s_ch_number(hdmi, bdata);

	if (chnum == 2)
		mtk_hdmi_mask(hdmi, AIP_TXCTRL, LAYOUT0, LAYOUT1);
	else
		mtk_hdmi_mask(hdmi, AIP_TXCTRL, LAYOUT1, LAYOUT1);
}

static void mtk_hdmi_i2s_data_fmt(struct mtk_hdmi *hdmi, unsigned char fmt)
{
	unsigned int u4Data;

	u4Data = mtk_hdmi_read(hdmi, AIP_I2S_CTRL);
	u4Data &= ~(WS_HIGH | I2S_1ST_BIT_NOSHIFT | JUSTIFY_RIGHT);

	switch (fmt) {
	case HDMI_I2S_MODE_RJT_24BIT:
	case HDMI_I2S_MODE_RJT_16BIT:
		u4Data |= (WS_HIGH | I2S_1ST_BIT_NOSHIFT | JUSTIFY_RIGHT);
		u4Data |= (WS_HIGH | I2S_1ST_BIT_NOSHIFT | JUSTIFY_RIGHT);
		break;

	case HDMI_I2S_MODE_LJT_24BIT:
	case HDMI_I2S_MODE_LJT_16BIT:
		u4Data |= (WS_HIGH | I2S_1ST_BIT_NOSHIFT);
		u4Data |= (WS_HIGH | I2S_1ST_BIT_NOSHIFT);
		break;

	case HDMI_I2S_MODE_I2S_24BIT:
	case HDMI_I2S_MODE_I2S_16BIT:
	default:
		break;
	}
	mtk_hdmi_write(hdmi, AIP_I2S_CTRL, u4Data);
}

static inline void mtk_hdmi_i2s_sck_edge(struct mtk_hdmi *hdmi,
					 unsigned int edge)
{
	mtk_hdmi_mask(hdmi, AIP_I2S_CTRL, edge, SCK_EDGE_RISE);
}

static inline void mtk_hdmi_i2s_cbit_order(struct mtk_hdmi *hdmi,
					   unsigned int cbit)
{
	mtk_hdmi_mask(hdmi, AIP_I2S_CTRL, cbit, CBIT_ORDER_SAME);
}

static inline void mtk_hdmi_i2s_vbit(struct mtk_hdmi *hdmi, unsigned int vbit)
{
	mtk_hdmi_mask(hdmi, AIP_I2S_CTRL, vbit, VBIT_COM);
}

static inline void mtk_hdmi_i2s_data_direction(struct mtk_hdmi *hdmi,
					       unsigned int data_dir)
{
	mtk_hdmi_mask(hdmi, AIP_I2S_CTRL, data_dir, DATA_DIR_LSB);
}

static inline void mtk_hdmi_hw_audio_type(struct mtk_hdmi *hdmi,
					  unsigned int spdif_i2s)
{
	mtk_hdmi_mask(hdmi, AIP_CTRL, spdif_i2s << SPDIF_EN_SHIFT, SPDIF_EN);
}

static unsigned char mtk_hdmi_get_i2s_ch_mapping(struct mtk_hdmi *hdmi,
						 unsigned char channel_type)
{
	unsigned char LFE, RRC, RLC;
	unsigned char FC = 0, RR = 0, RL = 0, RC = 0;
	unsigned char ch_number = 0;
	unsigned char channelmap = 0x00;

	switch (channel_type) {
	case HDMI_AUD_CHAN_TYPE_1_0:
	case HDMI_AUD_CHAN_TYPE_2_0:
		LFE = 0;
		ch_number = 2;
		break;

	case HDMI_AUD_CHAN_TYPE_1_1:
	case HDMI_AUD_CHAN_TYPE_2_1:
		LFE = 1;
		ch_number = 3;
		break;

	case HDMI_AUD_CHAN_TYPE_3_0:
		FC = 1;
		LFE = 0;
		ch_number = 3;
		break;

	case HDMI_AUD_CHAN_TYPE_3_0_LRS:
		RR = 1;
		RL = 1;
		LFE = 0;
		ch_number = 4;
		break;

	case HDMI_AUD_CHAN_TYPE_3_1_LRS:
		FC = 0;
		LFE = 1;
		RR = 1;
		RL = 1;
		ch_number = 5;
		break;

	case HDMI_AUD_CHAN_TYPE_4_0_CLRS:
		FC = 1;
		LFE = 0;
		RR = 1;
		RL = 1;
		ch_number = 5;
		break;

	case HDMI_AUD_CHAN_TYPE_4_1_CLRS:
		FC = 1;
		LFE = 1;
		RR = 1;
		RL = 1;
		ch_number = 6;
		break;

	case HDMI_AUD_CHAN_TYPE_3_1:
		FC = 1;
		LFE = 1;
		ch_number = 4;
		break;

	case HDMI_AUD_CHAN_TYPE_4_0:
		RR = 1;
		RL = 1;
		LFE = 0;
		ch_number = 4;
		break;

	case HDMI_AUD_CHAN_TYPE_4_1:
		RR = 1;
		RL = 1;
		LFE = 1;
		ch_number = 5;
		break;

	case HDMI_AUD_CHAN_TYPE_5_0:
		FC = 1;
		LFE = 0;
		RR = 1;
		RL = 1;
		ch_number = 5;
		break;

	case HDMI_AUD_CHAN_TYPE_5_1:
		FC = 1;
		LFE = 1;
		RR = 1;
		RL = 1;
		ch_number = 6;
		break;

	case HDMI_AUD_CHAN_TYPE_6_0:
	case HDMI_AUD_CHAN_TYPE_6_0_CS:
	case HDMI_AUD_CHAN_TYPE_6_0_CH:
	case HDMI_AUD_CHAN_TYPE_6_0_OH:
	case HDMI_AUD_CHAN_TYPE_6_0_CHR:
		FC = 1;
		LFE = 0;
		RR = 1;
		RL = 1;
		RC = 1;
		ch_number = 6;
		break;

	case HDMI_AUD_CHAN_TYPE_6_1:
	case HDMI_AUD_CHAN_TYPE_6_1_CS:
	case HDMI_AUD_CHAN_TYPE_6_1_CH:
	case HDMI_AUD_CHAN_TYPE_6_1_OH:
	case HDMI_AUD_CHAN_TYPE_6_1_CHR:
		FC = 1;
		LFE = 1;
		RR = 1;
		RL = 1;
		RC = 1;
		ch_number = 7;
		break;

	case HDMI_AUD_CHAN_TYPE_7_0:
	case HDMI_AUD_CHAN_TYPE_7_0_LH_RH:
	case HDMI_AUD_CHAN_TYPE_7_0_LSR_RSR:
	case HDMI_AUD_CHAN_TYPE_7_0_LC_RC:
	case HDMI_AUD_CHAN_TYPE_7_0_LW_RW:
	case HDMI_AUD_CHAN_TYPE_7_0_LSD_RSD:
	case HDMI_AUD_CHAN_TYPE_7_0_LSS_RSS:
	case HDMI_AUD_CHAN_TYPE_7_0_LHS_RHS:
	case HDMI_AUD_CHAN_TYPE_7_0_CS_CH:
	case HDMI_AUD_CHAN_TYPE_7_0_CS_OH:
	case HDMI_AUD_CHAN_TYPE_7_0_CS_CHR:
	case HDMI_AUD_CHAN_TYPE_7_0_CH_OH:
	case HDMI_AUD_CHAN_TYPE_7_0_CH_CHR:
	case HDMI_AUD_CHAN_TYPE_7_0_OH_CHR:
	case HDMI_AUD_CHAN_TYPE_7_0_LSS_RSS_LSR_RSR:
	case HDMI_AUD_CHAN_TYPE_8_0_LH_RH_CS:
		FC = 1;
		LFE = 0;
		RR = 1;
		RL = 1;
		RRC = 1;
		RLC = 1;
		ch_number = 7;
		break;

	case HDMI_AUD_CHAN_TYPE_7_1:
	case HDMI_AUD_CHAN_TYPE_7_1_LH_RH:
	case HDMI_AUD_CHAN_TYPE_7_1_LSR_RSR:
	case HDMI_AUD_CHAN_TYPE_7_1_LC_RC:
	case HDMI_AUD_CHAN_TYPE_7_1_LW_RW:
	case HDMI_AUD_CHAN_TYPE_7_1_LSD_RSD:
	case HDMI_AUD_CHAN_TYPE_7_1_LSS_RSS:
	case HDMI_AUD_CHAN_TYPE_7_1_LHS_RHS:
	case HDMI_AUD_CHAN_TYPE_7_1_CS_CH:
	case HDMI_AUD_CHAN_TYPE_7_1_CS_OH:
	case HDMI_AUD_CHAN_TYPE_7_1_CS_CHR:
	case HDMI_AUD_CHAN_TYPE_7_1_CH_OH:
	case HDMI_AUD_CHAN_TYPE_7_1_CH_CHR:
	case HDMI_AUD_CHAN_TYPE_7_1_OH_CHR:
	case HDMI_AUD_CHAN_TYPE_7_1_LSS_RSS_LSR_RSR:
		FC = 1;
		LFE = 1;
		RR = 1;
		RL = 1;
		RRC = 1;
		RLC = 1;
		ch_number = 8;
		break;

	default:
		ch_number = 2;
		break;
	}

	switch (ch_number) {
	case 8:
		break;

	case 7:
		break;

	case 6:
		if (FC == 1 && RR == 1 && RL == 1 && RC == 1 && LFE == 0) {
			/* 6.0 */
			channelmap = 0x0E;
		} else if (FC == 1 && RR == 1 && RL == 1 && RC == 0 &&
			   LFE == 1) {
			/* 5.1 */
			channelmap = 0x0B;
		}
		break;

	case 5:
		break;

	case 4:
		if (RR == 1 && RL == 1 && LFE == 0)
			channelmap = 0x08;
		else if (FC == 1 && LFE == 1)
			channelmap = 0x03;
		break;

	case 3:
		if (FC == 1)
			channelmap = 0x02;
		else if (LFE == 1)
			channelmap = 0x01;
		break;

	case 2:
		channelmap = 0x00;
		break;

	default:
		break;
	}

	return channelmap;
}

static inline void mtk_hdmi_hw_i2s_ch_swap(struct mtk_hdmi *hdmi,
					   unsigned char swapbit)
{
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, swapbit << 20, 0x0F << 20);
}

static void mtk_hdmi_hbr_config(struct mtk_hdmi *hdmi, bool dsd_bypass)
{
	if (dsd_bypass) {
		mtk_hdmi_mask(hdmi, AIP_CTRL, HBRA_ON,
			      SPDIF_EN | DSD_EN | HBRA_ON);
		mtk_hdmi_mask(hdmi, AIP_CTRL, I2S_EN, I2S_EN);
	} else {
		mtk_hdmi_mask(hdmi, AIP_CTRL, SPDIF_EN,
			      SPDIF_EN | DSD_EN | HBRA_ON);
		mtk_hdmi_mask(hdmi, AIP_CTRL, SPDIF_INTERNAL_MODULE,
			      SPDIF_INTERNAL_MODULE);
		mtk_hdmi_mask(hdmi, AIP_CTRL, HBR_FROM_SPDIF, HBR_FROM_SPDIF);
		mtk_hdmi_mask(hdmi, AIP_CTRL, CTS_CAL_N4, CTS_CAL_N4);
	}
}

static inline void mtk_hdmi_hw_spdif_config(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, WR_1UI_UNLOCK, WR_1UI_LOCK);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, FS_UNOVERRIDE, FS_OVERRIDE_WRITE);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, WR_2UI_UNLOCK, WR_2UI_LOCK);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, 0x4 << MAX_1UI_WRITE_SHIFT,
		      MAX_1UI_WRITE);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, 0x9 << MAX_2UI_WRITE_SHIFT,
		      MAX_2UI_WRITE);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, 0x4 << AUD_ERR_THRESH_SHIFT,
		      AUD_ERR_THRESH);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, I2S2DSD_EN, I2S2DSD_EN);
}

static void mtk_hdmi_aud_set_input(struct mtk_hdmi *hdmi)
{
	unsigned char ChMapping;

	mtk_hdmi_write(hdmi, TOP_AUD_MAP,
		       C_SD7 + C_SD6 + C_SD5 + C_SD4 + C_SD3 + C_SD2 + C_SD1 +
			       C_SD0);
	mtk_hdmi_mask(hdmi, AIP_SPDIF_CTRL, 0, 0x0F << 20);
	mtk_hdmi_mask(hdmi, AIP_CTRL, 0,
		      SPDIF_EN | DSD_EN | HBRA_ON | CTS_CAL_N4 |
			      HBR_FROM_SPDIF | SPDIF_INTERNAL_MODULE);
	mtk_hdmi_mask(hdmi, AIP_TXCTRL, 0, DSD_MUTE_DATA | LAYOUT1);

	if (hdmi->aud_param.aud_input_type == HDMI_AUD_INPUT_I2S) {
		if (hdmi->aud_param.aud_codec == HDMI_AUDIO_CODING_TYPE_DSD) {
			mtk_hdmi_audio_dsd_config(
				hdmi, hdmi->aud_param.codec_params.channels,
				0);
			mtk_hdmi_hw_i2s_ch_mapping(
				hdmi, hdmi->aud_param.codec_params.channels,
				1);
		} else {
			mtk_hdmi_i2s_data_fmt(hdmi,
					      hdmi->aud_param.aud_i2s_fmt);
			mtk_hdmi_i2s_sck_edge(hdmi, SCK_EDGE_RISE);
			mtk_hdmi_i2s_cbit_order(hdmi, CBIT_ORDER_SAME);
			mtk_hdmi_i2s_vbit(hdmi, VBIT_PCM);
			mtk_hdmi_i2s_data_direction(hdmi, DATA_DIR_MSB);
			mtk_hdmi_hw_audio_type(hdmi, HDMI_AUD_INPUT_I2S);
			ChMapping = mtk_hdmi_get_i2s_ch_mapping(
				hdmi, hdmi->aud_param.aud_input_chan_type);
			mtk_hdmi_hw_i2s_ch_mapping(
				hdmi, hdmi->aud_param.codec_params.channels,
				ChMapping);
			mtk_hdmi_hw_i2s_ch_swap(hdmi, LFE_CC_SWAP);
		}
	} else {
		if (hdmi->aud_param.aud_input_type == HDMI_AUD_INPUT_SPDIF &&
		    (hdmi->aud_param.aud_codec ==
		      HDMI_AUDIO_CODING_TYPE_DTS_HD ||
		     hdmi->aud_param.aud_codec ==
		      HDMI_AUDIO_CODING_TYPE_MLP) &&
		    hdmi->aud_param.codec_params.sample_rate == 768000) {
			mtk_hdmi_hbr_config(hdmi, false);
		} else {
			mtk_hdmi_hw_spdif_config(hdmi);
			mtk_hdmi_hw_i2s_ch_mapping(hdmi, 2, 0);
		}
	}
}

static void mtk_hdmi_aud_set_sw_ncts(struct mtk_hdmi *hdmi,
				     struct drm_display_mode *display_mode)
{
	unsigned int sample_rate = hdmi->aud_param.codec_params.sample_rate;

	mtk_hdmi_aud_on_off_hw_ncts(hdmi, false);

	mtk_hdmi_hw_aud_set_ncts(hdmi, sample_rate, display_mode->clock);
}

static inline void mtk_hdmi_hw_audio_input_enable(struct mtk_hdmi *hdmi,
						  unsigned int enable)
{
	if (enable)
		mtk_hdmi_mask(hdmi, AIP_CTRL, AUD_IN_EN, AUD_IN_EN);
	else
		mtk_hdmi_mask(hdmi, AIP_CTRL, 0x0 << AUD_IN_EN_SHIFT,
			      AUD_IN_EN);
}

static void mtk_hdmi_aip_ctrl_init(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_mask(hdmi, AIP_CTRL,
		      AUD_SEL_OWRT | NO_MCLK_CTSGEN_SEL | CTS_REQ_EN,
		      AUD_SEL_OWRT | NO_MCLK_CTSGEN_SEL | MCLK_EN | CTS_REQ_EN);
	mtk_hdmi_mask(hdmi, AIP_TPI_CTRL, TPI_AUDIO_LOOKUP_DIS,
		      TPI_AUDIO_LOOKUP_EN);
}

static void mtk_hdmi_audio_reset(struct mtk_hdmi *hdmi, bool rst)
{
	if (rst)
		mtk_hdmi_mask(hdmi, AIP_TXCTRL,
			      RST4AUDIO | RST4AUDIO_FIFO | RST4AUDIO_ACR,
			      RST4AUDIO | RST4AUDIO_FIFO | RST4AUDIO_ACR);
	else
		mtk_hdmi_mask(hdmi, AIP_TXCTRL, 0,
			      RST4AUDIO | RST4AUDIO_FIFO | RST4AUDIO_ACR);
}

static void mtk_hdmi_aud_output_config(struct mtk_hdmi *hdmi,
				       struct drm_display_mode *display_mode)
{
	mtk_hdmi_hw_aud_mute(hdmi);
	mtk_hdmi_aud_enable_packet(hdmi, false);
	mtk_hdmi_audio_reset(hdmi, true);
	mtk_hdmi_aip_ctrl_init(hdmi);

	mtk_hdmi_aud_set_input(hdmi);

	mtk_hdmi_hw_aud_set_channel_status(
		hdmi, hdmi->aud_param.codec_params.iec.status);

	mtk_hdmi_setup_audio_infoframe(hdmi);

	mtk_hdmi_hw_audio_input_enable(hdmi, true);

	mtk_hdmi_audio_reset(hdmi, false);

	mtk_hdmi_aud_set_sw_ncts(hdmi, display_mode);

	usleep_range(25, 50);
	mtk_hdmi_aud_on_off_hw_ncts(hdmi, true);

	mtk_hdmi_aud_enable_packet(hdmi, true);
	mtk_hdmi_hw_aud_unmute(hdmi);
}

void mtk_hdmi_output_init_mt8195(struct mtk_hdmi *hdmi)
{
	struct hdmi_audio_param *aud_param = &hdmi->aud_param;

	aud_param->aud_codec = HDMI_AUDIO_CODING_TYPE_PCM;
	aud_param->aud_sampe_size = HDMI_AUDIO_SAMPLE_SIZE_16;
	aud_param->aud_input_type = HDMI_AUD_INPUT_I2S;
	aud_param->aud_i2s_fmt = HDMI_I2S_MODE_I2S_24BIT;
	aud_param->aud_mclk = HDMI_AUD_MCLK_128FS;
	aud_param->aud_input_chan_type = HDMI_AUD_CHAN_TYPE_2_0;

	hdmi->hpd = HDMI_PLUG_OUT;
	hdmi->set_csp_depth = RGB444_8bit;
	hdmi->csp = HDMI_COLORSPACE_RGB;
	hdmi->color_depth = HDMI_8_BIT;
	hdmi->colorimtery = HDMI_COLORIMETRY_NONE;
	hdmi->extended_colorimetry = HDMI_EXTENDED_COLORIMETRY_RESERVED;
	hdmi->quantization_range = HDMI_QUANTIZATION_RANGE_DEFAULT;
	hdmi->ycc_quantization_range = HDMI_YCC_QUANTIZATION_RANGE_LIMITED;
}

static void mtk_hdmi_reset_colorspace_setting(struct mtk_hdmi *hdmi)
{
	hdmi->set_csp_depth = RGB444_8bit;
	hdmi->csp = HDMI_COLORSPACE_RGB;
	hdmi->color_depth = HDMI_8_BIT;
	hdmi->colorimtery = HDMI_COLORIMETRY_NONE;
	hdmi->extended_colorimetry = HDMI_EXTENDED_COLORIMETRY_RESERVED;
	hdmi->quantization_range = HDMI_QUANTIZATION_RANGE_DEFAULT;
	hdmi->ycc_quantization_range = HDMI_YCC_QUANTIZATION_RANGE_LIMITED;
}

static void mtk_hdmi_audio_enable(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_aud_enable_packet(hdmi, true);
	hdmi->audio_enable = true;
}

static void mtk_hdmi_audio_disable(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_aud_enable_packet(hdmi, false);
	hdmi->audio_enable = false;
}

static void mtk_hdmi_audio_set_param(struct mtk_hdmi *hdmi,
				     struct hdmi_audio_param *param)
{
	if (!hdmi->audio_enable)
		return;

	memcpy(&hdmi->aud_param, param, sizeof(*param));
	mtk_hdmi_aud_output_config(hdmi, &hdmi->mode);
}

static void mtk_hdmi_change_video_resolution(struct mtk_hdmi *hdmi)
{
	bool is_over_340M = false;
	bool is_hdmi_sink = false;
	bool ret = false;

	mtk_hdmi_hw_reset(hdmi);
	mtk_hdmi_set_sw_hpd(hdmi, true);
	usleep_range(2, 5);

	mtk_hdmi_write(hdmi, HDCP_TOP_CTRL, 0x0);
	mtk_hdmi_en_hdcp_reauth_int(hdmi, true);
	mtk_hdmi_enable_hpd_pord_irq(hdmi, true);
	mtk_hdmi_force_hdcp_hpd(hdmi);

	is_hdmi_sink = mtk_hdmi_sink_is_hdmi_device(hdmi);
	mtk_hdmi_set_deep_color(hdmi, is_hdmi_sink);
	mtk_hdmi_enable_hdmi_mode(hdmi, is_hdmi_sink);

	usleep_range(5, 10);
	mtk_hdmi_hw_vid_black(hdmi, true);
	mtk_hdmi_hw_aud_mute(hdmi);
	mtk_hdmi_hw_send_av_unmute(hdmi);

	mtk_hdmi_mask(hdmi, TOP_CFG01, NULL_PKT_VSYNC_HIGH_EN,
		      NULL_PKT_VSYNC_HIGH_EN | NULL_PKT_EN);

	is_over_340M = mtk_hdmi_tmds_over_340M(hdmi);
	dev_info(hdmi->dev, "is_over_340M:%d\n", is_over_340M);

	mtk_hdmi_enable_scrambling(hdmi, is_over_340M);
	ret = drm_scdc_set_scrambling(hdmi->ddc_adpt, is_over_340M);
	dev_info(hdmi->dev, "set_scrambling:%d\n", ret);
	ret = drm_scdc_set_high_tmds_clock_ratio(hdmi->ddc_adpt, is_over_340M);
	dev_info(hdmi->dev, "set_high_tmds_clock_ratio:%d\n", ret);

	if (hdmi->csp == HDMI_COLORSPACE_YUV420)
		mtk_hdmi_yuv420_downsample(hdmi, true);
	else
		mtk_hdmi_yuv420_downsample(hdmi, false);
}

static void mtk_hdmi_output_set_display_mode(struct mtk_hdmi *hdmi,
					     struct drm_display_mode *mode)
{
	int ret;
	union phy_configure_opts opts = {
		.dp = { .link_rate = hdmi->mode.clock * 1000 }
	};

	ret = phy_configure(hdmi->phy, &opts);
	if (ret)
		dev_err(hdmi->dev, "Setting clock=%lu failed: %d", mode->clock, ret);

	mtk_hdmi_change_video_resolution(hdmi);
	mtk_hdmi_aud_output_config(hdmi, mode);
}

void mtk_hdmi_clk_enable(struct mtk_hdmi *hdmi)
{
	int i;

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d],enable clock\n", __func__, __LINE__);

	clk_set_parent(hdmi->clk[MTK_MT8195_HDIM_HDCP_SEL],
		       hdmi->clk[MTK_MT8195_HDMI_UNIVPLL_D4D8]);

	for (i = 0; i < ARRAY_SIZE(mtk_hdmi_clk_names_mt8195); i++) {
		if (i == MTK_MT8195_HDMI_UNIVPLL_D4D8 ||
		    i == MTK_MT8195_HDMI_CLK_UNIVPLL_D6D4 ||
		    i == MTK_MT8195_HDMI_CLK_MSDCPLL_D2 ||
		    i == MTK_MT8195_HDMI_CLK_HDMI_APB_SEL)
			continue;
		else
			clk_prepare_enable(hdmi->clk[i]);
	}
}

void mtk_hdmi_clk_disable_mt8195(struct mtk_hdmi *hdmi)
{
	int i;

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d],disable clock\n", __func__, __LINE__);

	for (i = 0; i < ARRAY_SIZE(mtk_hdmi_clk_names_mt8195); i++) {
		if (i == MTK_MT8195_HDMI_UNIVPLL_D4D8 ||
		    i == MTK_MT8195_HDMI_CLK_UNIVPLL_D6D4 ||
		    i == MTK_MT8195_HDMI_CLK_MSDCPLL_D2 ||
		    i == MTK_MT8195_HDMI_CLK_HDMI_APB_SEL)
			continue;
		else
			clk_disable_unprepare(hdmi->clk[i]);
	}
}

static void mtk_hdmi_hpd_event(enum hdmi_hpd_state hpd, struct device *dev)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	if (hdmi && hdmi->bridge.encoder && hdmi->bridge.encoder->dev)
		drm_helper_hpd_irq_event(hdmi->bridge.encoder->dev);
}

static enum hdmi_hpd_state mtk_hdmi_hpd_pord_status(struct mtk_hdmi *hdmi)
{
	unsigned int hpd_status;

	hpd_status = mtk_hdmi_read(hdmi, HPD_DDC_STATUS);
	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d],hpd status:0x%x\n",
			    __func__, __LINE__, hpd_status);
	if ((hpd_status & (HPD_PIN_STA | PORD_PIN_STA)) ==
	    (HPD_PIN_STA | PORD_PIN_STA))
		return HDMI_PLUG_IN_AND_SINK_POWER_ON;
	else if ((hpd_status & (HPD_PIN_STA | PORD_PIN_STA)) == HPD_PIN_STA)
		return HDMI_PLUG_IN_ONLY;
	else
		return HDMI_PLUG_OUT;
}

static irqreturn_t mtk_hdmi_isr(int irq, void *arg)
{
	struct mtk_hdmi *hdmi = arg;
	unsigned int int_status;
	int ret = IRQ_HANDLED;

	int_status = mtk_hdmi_read(hdmi, TOP_INT_STA00);
	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d],int status:0x%X\n",
			    __func__, __LINE__, int_status);

	/* handle hpd interrupt */
	if (int_status & (PORD_F_INT_STA | PORD_R_INT_STA | HTPLG_F_INT_STA |
			  HTPLG_R_INT_STA)) {
		mtk_hdmi_enable_hpd_pord_irq(hdmi, false);
		mtk_hdmi_clr_htplg_pord_irq(hdmi);
		ret = IRQ_WAKE_THREAD;
	}

	/*clear all tx irq*/
	mtk_hdmi_clr_all_int_status(hdmi);

	return ret;
}

static irqreturn_t mtk_hdmi_hpd_work_handle(int irq, void *arg)
{
	struct mtk_hdmi *hdmi = arg;
	enum hdmi_hpd_state hpd;

	/* 30ms is an empirical value that could debounce known HDMI monitor's HPD status. */
	msleep(30);

	hpd = mtk_hdmi_hpd_pord_status(hdmi);
	dev_info(hdmi->dev, "hpd:%d\n", hpd);

	if (hpd != hdmi->hpd) {
		dev_info(hdmi->dev, "Hot Plug State Change! old:%d, new:%d\n", hdmi->hpd, hpd);
		hdmi->hpd = hpd;
		mtk_hdmi_hpd_event(hpd, hdmi->dev);
	}

	mtk_hdmi_enable_hpd_pord_irq(hdmi, true);

	return IRQ_HANDLED;
}

static int mtk_hdmi_enable_disable(struct mtk_hdmi *hdmi, bool enable)
{
	int ret;

	if (enable && !hdmi->hdmi_enabled) {
		if (!hdmi->power_clk_enabled) {
			/* power domain on */
			ret = pm_runtime_get_sync(hdmi->dev);
			DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d],power domain on %d\n",
					    __func__, __LINE__, ret);

			/* clk on */
			mtk_hdmi_clk_enable(hdmi);
			hdmi->power_clk_enabled = true;
		}

		if (!hdmi->irq_registered) {
			/* disable all tx interrupts */
			mtk_hdmi_disable_all_int(hdmi);
			/* request irq */
			hdmi->hdmi_irq =
				irq_of_parse_and_map(hdmi->dev->of_node, 0);
			ret = request_threaded_irq(hdmi->hdmi_irq, mtk_hdmi_isr,
						   mtk_hdmi_hpd_work_handle,
						   IRQF_TRIGGER_HIGH, "hdmiirq",
						   hdmi);
			hdmi->irq_registered = true;
			/* enable hpd interrupt */
			mtk_hdmi_set_sw_hpd(hdmi, true);
			mtk_hdmi_enable_hpd_pord_irq(hdmi, true);
		}

	} else if (!enable && hdmi->hdmi_enabled) {
		if (hdmi->irq_registered) {
			/* free irq */
			free_irq(hdmi->hdmi_irq, NULL);
			hdmi->irq_registered = false;
		}

		if (hdmi->power_clk_enabled) {
			/* clk disable */
			mtk_hdmi_clk_disable_mt8195(hdmi);
			/* power domain off */
			ret = pm_runtime_put_sync(hdmi->dev);
			DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d],power domain off %d\n",
					    __func__, __LINE__, ret);
			hdmi->power_clk_enabled = false;
		}
	}

	hdmi->hdmi_enabled = enable;

	return 0;
}

static const struct drm_prop_enum_list csp_depth_props[] = {
	{ __builtin_ffs(RGB444_8bit), "RGB444_8bit" },
	{ __builtin_ffs(RGB444_10bit), "RGB444_10bit" },
	{ __builtin_ffs(RGB444_12bit), "RGB444_10bit" },
	{ __builtin_ffs(RGB444_16bit), "RGB444_16bit" },
	{ __builtin_ffs(YCBCR444_8bit), "YCBCR444_8bit" },
	{ __builtin_ffs(YCBCR444_10bit), "YCBCR444_10bit" },
	{ __builtin_ffs(YCBCR444_12bit), "YCBCR444_12bit" },
	{ __builtin_ffs(YCBCR444_16bit), "YCBCR444_16bit" },
	{ __builtin_ffs(YCBCR422_8bit_NO_SUPPORT), "YCBCR422_8bit_NO_SUPPORT" },
	{ __builtin_ffs(YCBCR422_10bit_NO_SUPPORT),
	  "YCBCR422_10bit_NO_SUPPORT" },
	{ __builtin_ffs(YCBCR422_12bit), "YCBCR422_12bit" },
	{ __builtin_ffs(YCBCR422_16bit_NO_SUPPORT),
	  "YCBCR422_16bit_NO_SUPPORT" },
	{ __builtin_ffs(YCBCR420_8bit), "YCBCR420_8bit" },
	{ __builtin_ffs(YCBCR420_10bit), "YCBCR420_10bit" },
	{ __builtin_ffs(YCBCR420_12bit), "YCBCR420_12bit" },
	{ __builtin_ffs(YCBCR420_16bit), "YCBCR420_16bit" },
};

static void mtk_hdmi_convert_colorspace_depth(struct mtk_hdmi *hdmi)
{
	switch (hdmi->set_csp_depth) {
	case RGB444_8bit:
		hdmi->csp = HDMI_COLORSPACE_RGB;
		hdmi->color_depth = HDMI_8_BIT;
		break;
	case RGB444_10bit:
		hdmi->csp = HDMI_COLORSPACE_RGB;
		hdmi->color_depth = HDMI_10_BIT;
		break;
	case RGB444_12bit:
		hdmi->csp = HDMI_COLORSPACE_RGB;
		hdmi->color_depth = HDMI_12_BIT;
		break;
	case RGB444_16bit:
		hdmi->csp = HDMI_COLORSPACE_RGB;
		hdmi->color_depth = HDMI_16_BIT;
		break;
	case YCBCR444_8bit:
		hdmi->csp = HDMI_COLORSPACE_YUV444;
		hdmi->color_depth = HDMI_8_BIT;
		break;
	case YCBCR444_10bit:
		hdmi->csp = HDMI_COLORSPACE_YUV444;
		hdmi->color_depth = HDMI_10_BIT;
		break;
	case YCBCR444_12bit:
		hdmi->csp = HDMI_COLORSPACE_YUV444;
		hdmi->color_depth = HDMI_12_BIT;
		break;
	case YCBCR444_16bit:
		hdmi->csp = HDMI_COLORSPACE_YUV444;
		hdmi->color_depth = HDMI_16_BIT;
		break;
	case YCBCR422_12bit:
		hdmi->csp = HDMI_COLORSPACE_YUV422;
		hdmi->color_depth = HDMI_12_BIT;
		break;
	case YCBCR420_8bit:
		hdmi->csp = HDMI_COLORSPACE_YUV420;
		hdmi->color_depth = HDMI_8_BIT;
		break;
	case YCBCR420_10bit:
		hdmi->csp = HDMI_COLORSPACE_YUV420;
		hdmi->color_depth = HDMI_10_BIT;
		break;
	case YCBCR420_12bit:
		hdmi->csp = HDMI_COLORSPACE_YUV420;
		hdmi->color_depth = HDMI_12_BIT;
		break;
	case YCBCR420_16bit:
		hdmi->csp = HDMI_COLORSPACE_YUV420;
		hdmi->color_depth = HDMI_16_BIT;
		break;
	default:

		hdmi->csp = HDMI_COLORSPACE_RGB;
		hdmi->color_depth = HDMI_8_BIT;
	}

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "color space:%d, color depth:%d\n",
			    hdmi->csp, hdmi->color_depth);
}

static int mtk_hdmi_conn_get_modes(struct drm_connector *conn)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);
	struct edid *edid;
	int ret;

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d]\n", __func__, __LINE__);

	if (!hdmi->ddc_adpt)
		return -ENODEV;

	edid = drm_get_edid(conn, hdmi->ddc_adpt);
	if (!edid)
		return -ENODEV;

	mtk_hdmi_show_EDID_raw_data(hdmi, edid);

	hdmi->dvi_mode = !drm_detect_hdmi_monitor(edid);

	drm_connector_update_edid_property(conn, edid);

	ret = drm_add_edid_modes(conn, edid);

	kfree(edid);

	return ret;
}

static int mtk_hdmi_conn_mode_valid(struct drm_connector *conn,
				    struct drm_display_mode *mode)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d]\n", __func__, __LINE__);

	if (mode->clock < 27000)
		return MODE_CLOCK_LOW;
	if (mode->clock > 594000)
		return MODE_CLOCK_HIGH;

	return drm_mode_validate_size(mode, 0x1fff, 0x1fff);
}

static struct drm_encoder *mtk_hdmi_conn_best_enc(struct drm_connector *conn)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);

	return hdmi->bridge.encoder;
}

static const struct drm_connector_helper_funcs mtk_hdmi_connector_helper_funcs = {
	.get_modes = mtk_hdmi_conn_get_modes,
	.mode_valid = mtk_hdmi_conn_mode_valid,
	.best_encoder = mtk_hdmi_conn_best_enc,
};

/*
 * Bridge callbacks
 */
//TODO: duplicated code?
static int mtk_hdmi_bridge_attach(struct drm_bridge *bridge,
				  enum drm_bridge_attach_flags flags)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);
	int ret;

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d]\n", __func__, __LINE__);

	if (!(flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR)) {
		DRM_ERROR("%s: The flag DRM_BRIDGE_ATTACH_NO_CONNECTOR must be supplied\n",
				__func__);
		return -EINVAL;
	}
	if (hdmi->next_bridge) {
		ret = drm_bridge_attach(bridge->encoder, hdmi->next_bridge,
				bridge, flags);
		if (ret)
			return ret;
	}

	pm_runtime_enable(hdmi->dev);
	mtk_hdmi_enable_disable(hdmi, true);

	return 0;
}

static void mtk_hdmi_bridge_disable(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d]\n", __func__, __LINE__);

	if (!hdmi->enabled)
		return;

	mtk_hdmi_hw_send_av_mute(hdmi);
	usleep_range(50000, 50050);
	mtk_hdmi_hw_vid_black(hdmi, true);
	mtk_hdmi_hw_aud_mute(hdmi);
	mtk_hdmi_disable_hdcp_encrypt(hdmi);
	usleep_range(50000, 50050);

	phy_power_off(hdmi->phy);

	hdmi->enabled = false;
}

static void mtk_hdmi_handle_plugged_change(struct mtk_hdmi *hdmi, bool plugged)
{
	if (hdmi->plugged_cb && hdmi->codec_dev)
		hdmi->plugged_cb(hdmi->codec_dev, plugged);
}

static void mtk_hdmi_bridge_post_disable(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d]\n", __func__, __LINE__);

	if (!hdmi->powered)
		return;

	phy_power_off(hdmi->phy);

	hdmi->powered = false;

	mtk_hdmi_reset_colorspace_setting(hdmi);

	/* signal the disconnect event to audio codec */
	mtk_hdmi_handle_plugged_change(hdmi, false);
}

static void mtk_hdmi_bridge_pre_enable(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);
	u8 buffer_spd[MT8195_HDMI_SPD_BUFFER_SIZE];
	u8 buffer_avi[MT8195_HDMI_AVI_BUFFER_SIZE];
	union phy_configure_opts opts = {
		.dp = { .link_rate = hdmi->mode.clock * 1000 }
	};

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d]\n", __func__, __LINE__);

	mtk_hdmi_convert_colorspace_depth(hdmi);
	mtk_hdmi_output_set_display_mode(hdmi, &hdmi->mode);
	phy_configure(hdmi->phy, &opts);
	phy_power_on(hdmi->phy);
	mtk_hdmi_send_infoframe(hdmi, buffer_spd, sizeof(buffer_spd),
				buffer_avi, sizeof(buffer_avi), &hdmi->mode);
	mtk_hdmi_hw_spd_infoframe(hdmi, buffer_spd, sizeof(buffer_avi));
	mtk_hdmi_hw_avi_infoframe(hdmi, buffer_avi, sizeof(buffer_spd));

	hdmi->powered = true;
}

static void mtk_hdmi_bridge_enable(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d]\n", __func__, __LINE__);

	phy_power_on(hdmi->phy);
	mtk_hdmi_hw_vid_black(hdmi, false);
	mtk_hdmi_hw_aud_unmute(hdmi);

	/* signal the connect event to audio codec */
	mtk_hdmi_handle_plugged_change(hdmi, true);

	hdmi->enabled = true;
}

static enum drm_connector_status mtk_hdmi_bridge_detect(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "[%s][%d]\n", __func__, __LINE__);

	if (hdmi->hpd != HDMI_PLUG_IN_AND_SINK_POWER_ON &&
	    hdmi->hpd != HDMI_PLUG_IN_ONLY) {
		hdmi->support_csp_depth = RGB444_8bit;
		hdmi->set_csp_depth = RGB444_8bit;
		hdmi->csp = HDMI_COLORSPACE_RGB;
		hdmi->color_depth = HDMI_8_BIT;
		hdmi->colorimtery = HDMI_COLORIMETRY_NONE;
		hdmi->extended_colorimetry = HDMI_EXTENDED_COLORIMETRY_RESERVED;
		hdmi->quantization_range = HDMI_QUANTIZATION_RANGE_DEFAULT;
		hdmi->ycc_quantization_range =
			HDMI_YCC_QUANTIZATION_RANGE_LIMITED;
	}

	return (hdmi->hpd != HDMI_PLUG_OUT) ? connector_status_connected :
						    connector_status_disconnected;
}

const struct drm_bridge_funcs mtk_mt8195_hdmi_bridge_funcs = {
	.attach = mtk_hdmi_bridge_attach,
	.mode_fixup = mtk_hdmi_bridge_mode_fixup,
	.disable = mtk_hdmi_bridge_disable,
	.post_disable = mtk_hdmi_bridge_post_disable,
	.mode_set = mtk_hdmi_bridge_mode_set,
	.pre_enable = mtk_hdmi_bridge_pre_enable,
	.enable = mtk_hdmi_bridge_enable,
	.get_edid = mtk_hdmi_bridge_get_edid,
	.detect = mtk_hdmi_bridge_detect,
};

static void mtk_hdmi_set_plugged_cb(struct mtk_hdmi *hdmi,
				    hdmi_codec_plugged_cb fn,
				    struct device *codec_dev)
{
	bool plugged;

	hdmi->plugged_cb = fn;
	hdmi->codec_dev = codec_dev;
	plugged = (hdmi->hpd == HDMI_PLUG_IN_AND_SINK_POWER_ON) ? true : false;
	mtk_hdmi_handle_plugged_change(hdmi, plugged);
}

/*
 * HDMI audio codec callbacks
 */
static int mtk_hdmi_audio_hook_plugged_cb(struct device *dev, void *data,
					  hdmi_codec_plugged_cb fn,
					  struct device *codec_dev)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	if (!hdmi)
		return -ENODEV;

	mtk_hdmi_set_plugged_cb(hdmi, fn, codec_dev);
	return 0;
}

static int mtk_hdmi_audio_hw_params(struct device *dev, void *data,
				    struct hdmi_codec_daifmt *daifmt,
				    struct hdmi_codec_params *params)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);
	struct hdmi_audio_param hdmi_params;
	unsigned int chan = params->cea.channels;

	if (!hdmi->bridge.encoder)
		return -ENODEV;

	switch (chan) {
	case 2:
		hdmi_params.aud_input_chan_type = HDMI_AUD_CHAN_TYPE_2_0;
		break;
	case 4:
		hdmi_params.aud_input_chan_type = HDMI_AUD_CHAN_TYPE_4_0;
		break;
	case 6:
		hdmi_params.aud_input_chan_type = HDMI_AUD_CHAN_TYPE_5_1;
		break;
	case 8:
		hdmi_params.aud_input_chan_type = HDMI_AUD_CHAN_TYPE_7_1;
		break;
	default:
		return -EINVAL;
	}

	switch (params->sample_rate) {
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
	case 176400:
	case 192000:
		break;
	default:
		return -EINVAL;
	}

	switch (daifmt->fmt) {
	case HDMI_I2S:
		hdmi_params.aud_codec = HDMI_AUDIO_CODING_TYPE_PCM;
		hdmi_params.aud_sampe_size = HDMI_AUDIO_SAMPLE_SIZE_16;
		hdmi_params.aud_input_type = HDMI_AUD_INPUT_I2S;
		hdmi_params.aud_i2s_fmt = HDMI_I2S_MODE_I2S_24BIT;
		hdmi_params.aud_mclk = HDMI_AUD_MCLK_128FS;
		break;
	default:
		return -EINVAL;
	}

	memcpy(&hdmi_params.codec_params, params,
	       sizeof(hdmi_params.codec_params));

	mtk_hdmi_audio_set_param(hdmi, &hdmi_params);

	return 0;
}

static int mtk_hdmi_audio_startup(struct device *dev, void *data)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	mtk_hdmi_audio_enable(hdmi);

	return 0;
}

static void mtk_hdmi_audio_shutdown(struct device *dev, void *data)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	mtk_hdmi_audio_disable(hdmi);
}

static int mtk_hdmi_audio_mute(struct device *dev, void *data, bool enable,
			       int direction)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	if (direction != SNDRV_PCM_STREAM_PLAYBACK)
		return 0;

	if (enable)
		mtk_hdmi_hw_aud_mute(hdmi);
	else
		mtk_hdmi_hw_aud_unmute(hdmi);

	return 0;
}

static int mtk_hdmi_audio_get_eld(struct device *dev, void *data, uint8_t *buf,
				  size_t len)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	if (hdmi->enabled)
		memcpy(buf, hdmi->conn.eld, min(sizeof(hdmi->conn.eld), len));
	else
		memset(buf, 0, len);
	return 0;
}

static const struct hdmi_codec_ops mtk_hdmi_audio_codec_ops = {
	.hw_params = mtk_hdmi_audio_hw_params,
	.audio_startup = mtk_hdmi_audio_startup,
	.audio_shutdown = mtk_hdmi_audio_shutdown,
	.mute_stream = mtk_hdmi_audio_mute,
	.get_eld = mtk_hdmi_audio_get_eld,
	.hook_plugged_cb = mtk_hdmi_audio_hook_plugged_cb,
};

void set_hdmi_codec_pdata_mt8195(struct hdmi_codec_pdata *codec_data)
{
	codec_data->ops = &mtk_hdmi_audio_codec_ops;
	codec_data->max_i2s_channels = 2;
	codec_data->i2s = 1;
}

void set_abist_pattern(struct device *dev, enum mtk_abist_pattern mode_num)
{
	unsigned char abist_format;
	struct drm_display_mode *mode;
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	dev_err(hdmi->dev, "mode_num:%d\n", mode_num);

	switch (mode_num) {
	case MTK_ABIST_480P:
		dev_err(hdmi->dev, "0x2: 720*480P@60\n");
		mode = &mode_720x480_60hz_4v3;
		abist_format = ABIST_VIDEO_FORMAT_720x480P;
		break;
	case MTK_ABIST_720P:
		dev_err(hdmi->dev, "0x4: 1280*720P@50\n");
		mode = &mode_1280x720_50hz_16v9;
		abist_format = ABIST_VIDEO_FORMAT_720P50;
		break;
	case MTK_ABIST_1080P:
		dev_err(hdmi->dev, "0x5: 1920*1080P@60\n");
		mode = &mode_1920x1080_60hz_16v9;
		abist_format = ABIST_VIDEO_FORMAT_1080P60;
		break;
	case MTK_ABIST_2160P:
		dev_err(hdmi->dev, "0x7: 3840*2160P@30\n");
		mode = &mode_3840x2160_30hz_16v9;
		abist_format = ABIST_VIDEO_FORMAT_3840x2160P30;
		break;
	default:
		dev_err(hdmi->dev, "Wrong Mode!!\n");
		mode = &mode_720x480_60hz_4v3;
		abist_format = 0x2;
	}

	if (hdmi->enabled == true)
		drm_bridge_chain_disable(&hdmi->bridge);
	if (hdmi->powered == true)
		drm_bridge_chain_post_disable(&hdmi->bridge);

	msleep(50);

	drm_bridge_chain_mode_set(&hdmi->bridge, NULL, mode);
	drm_bridge_chain_pre_enable(&hdmi->bridge);
	drm_bridge_chain_enable(&hdmi->bridge);

	mtk_hdmi_mask(hdmi, TOP_CFG00, abist_format, ABIST_VIDEO_FORMAT_MASKBIT);
	mtk_hdmi_mask(hdmi, TOP_CFG00, (0x1 << 31), ABIST_ENABLE);
}
