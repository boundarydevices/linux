/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Copyright (c) 2021 BayLibre, SAS
 */

#ifndef _MTK_HDMI_COMMON_H
#define _MTK_HDMI_COMMON_H

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_print.h>

#include <linux/clk.h>
#include <linux/hdmi.h>
#include <linux/i2c.h>
#include <linux/mfd/syscon.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>

#include <sound/hdmi-codec.h>

//TODO: see what can be done here... :-/
#include "mtk_mt8195_hdmi_ddc.h"
#include "mtk_mt8195_hdmi.h"
#include "mtk_cec.h"
#include "mtk_hdmi.h"

struct mtk_hdmi_conf {
	bool tz_disabled;
	bool cea_modes_only;
	bool is_mt8195;
	bool low_power;
	unsigned long max_mode_clock;
	const u32 reg_hdmitx_config_ofs;
	void (*set_abist)(struct device *dev, enum mtk_abist_pattern mode_num);
};

enum hdmi_color_depth { HDMI_8_BIT, HDMI_10_BIT, HDMI_12_BIT, HDMI_16_BIT };

enum hdmi_aud_input_type {
	HDMI_AUD_INPUT_I2S = 0,
	HDMI_AUD_INPUT_SPDIF,
};

enum hdmi_aud_i2s_fmt {
	HDMI_I2S_MODE_RJT_24BIT = 0,
	HDMI_I2S_MODE_RJT_16BIT,
	HDMI_I2S_MODE_LJT_24BIT,
	HDMI_I2S_MODE_LJT_16BIT,
	HDMI_I2S_MODE_I2S_24BIT,
	HDMI_I2S_MODE_I2S_16BIT
};

enum hdmi_aud_mclk {
	HDMI_AUD_MCLK_128FS,
	HDMI_AUD_MCLK_192FS,
	HDMI_AUD_MCLK_256FS,
	HDMI_AUD_MCLK_384FS,
	HDMI_AUD_MCLK_512FS,
	HDMI_AUD_MCLK_768FS,
	HDMI_AUD_MCLK_1152FS,
};

enum hdmi_aud_channel_type {
	HDMI_AUD_CHAN_TYPE_1_0 = 0,
	HDMI_AUD_CHAN_TYPE_1_1,
	HDMI_AUD_CHAN_TYPE_2_0,
	HDMI_AUD_CHAN_TYPE_2_1,
	HDMI_AUD_CHAN_TYPE_3_0,
	HDMI_AUD_CHAN_TYPE_3_1,
	HDMI_AUD_CHAN_TYPE_4_0,
	HDMI_AUD_CHAN_TYPE_4_1,
	HDMI_AUD_CHAN_TYPE_5_0,
	HDMI_AUD_CHAN_TYPE_5_1,
	HDMI_AUD_CHAN_TYPE_6_0,
	HDMI_AUD_CHAN_TYPE_6_1,
	HDMI_AUD_CHAN_TYPE_7_0,
	HDMI_AUD_CHAN_TYPE_7_1,
	HDMI_AUD_CHAN_TYPE_3_0_LRS,
	HDMI_AUD_CHAN_TYPE_3_1_LRS,
	HDMI_AUD_CHAN_TYPE_4_0_CLRS,
	HDMI_AUD_CHAN_TYPE_4_1_CLRS,
	HDMI_AUD_CHAN_TYPE_6_1_CS,
	HDMI_AUD_CHAN_TYPE_6_1_CH,
	HDMI_AUD_CHAN_TYPE_6_1_OH,
	HDMI_AUD_CHAN_TYPE_6_1_CHR,
	HDMI_AUD_CHAN_TYPE_7_1_LH_RH,
	HDMI_AUD_CHAN_TYPE_7_1_LSR_RSR,
	HDMI_AUD_CHAN_TYPE_7_1_LC_RC,
	HDMI_AUD_CHAN_TYPE_7_1_LW_RW,
	HDMI_AUD_CHAN_TYPE_7_1_LSD_RSD,
	HDMI_AUD_CHAN_TYPE_7_1_LSS_RSS,
	HDMI_AUD_CHAN_TYPE_7_1_LHS_RHS,
	HDMI_AUD_CHAN_TYPE_7_1_CS_CH,
	HDMI_AUD_CHAN_TYPE_7_1_CS_OH,
	HDMI_AUD_CHAN_TYPE_7_1_CS_CHR,
	HDMI_AUD_CHAN_TYPE_7_1_CH_OH,
	HDMI_AUD_CHAN_TYPE_7_1_CH_CHR,
	HDMI_AUD_CHAN_TYPE_7_1_OH_CHR,
	HDMI_AUD_CHAN_TYPE_7_1_LSS_RSS_LSR_RSR,
	HDMI_AUD_CHAN_TYPE_6_0_CS,
	HDMI_AUD_CHAN_TYPE_6_0_CH,
	HDMI_AUD_CHAN_TYPE_6_0_OH,
	HDMI_AUD_CHAN_TYPE_6_0_CHR,
	HDMI_AUD_CHAN_TYPE_7_0_LH_RH,
	HDMI_AUD_CHAN_TYPE_7_0_LSR_RSR,
	HDMI_AUD_CHAN_TYPE_7_0_LC_RC,
	HDMI_AUD_CHAN_TYPE_7_0_LW_RW,
	HDMI_AUD_CHAN_TYPE_7_0_LSD_RSD,
	HDMI_AUD_CHAN_TYPE_7_0_LSS_RSS,
	HDMI_AUD_CHAN_TYPE_7_0_LHS_RHS,
	HDMI_AUD_CHAN_TYPE_7_0_CS_CH,
	HDMI_AUD_CHAN_TYPE_7_0_CS_OH,
	HDMI_AUD_CHAN_TYPE_7_0_CS_CHR,
	HDMI_AUD_CHAN_TYPE_7_0_CH_OH,
	HDMI_AUD_CHAN_TYPE_7_0_CH_CHR,
	HDMI_AUD_CHAN_TYPE_7_0_OH_CHR,
	HDMI_AUD_CHAN_TYPE_7_0_LSS_RSS_LSR_RSR,
	HDMI_AUD_CHAN_TYPE_8_0_LH_RH_CS,
	HDMI_AUD_CHAN_TYPE_UNKNOWN = 0xFF
};

enum hdmi_aud_channel_swap_type {
	HDMI_AUD_SWAP_LR,
	HDMI_AUD_SWAP_LFE_CC,
	HDMI_AUD_SWAP_LSRS,
	HDMI_AUD_SWAP_RLS_RRS,
	HDMI_AUD_SWAP_LR_STATUS,
};

enum mtk_hdmi_clk_id_mt8195 {
	MTK_MT8195_HDMI_CLK_UNIVPLL_D6D4,
	MTK_MT8195_HDMI_CLK_MSDCPLL_D2,
	MTK_MT8195_HDMI_CLK_HDMI_APB_SEL,
	MTK_MT8195_HDMI_UNIVPLL_D4D8,
	MTK_MT8195_HDIM_HDCP_SEL,
	MTK_MT8195_HDMI_HDCP_24M_SEL,
	MTK_MT8195_HDMI_VPP_SPLIT_HDMI,
	MTK_MT8195_HDMI_CLK_COUNT,
};

enum mtk_hdmi_clk_id_mt8183 {
	MTK_MT8183_HDMI_CLK_HDMI_PIXEL,
	MTK_MT8183_HDMI_CLK_HDMI_PLL,
	MTK_MT8183_HDMI_CLK_AUD_BCLK,
	MTK_MT8183_HDMI_CLK_AUD_SPDIF,
	MTK_MT8183_HDMI_CLK_COUNT,
};

extern const char *const mtk_hdmi_clk_names_mt8183[MTK_MT8183_HDMI_CLK_COUNT];
extern const char *const mtk_hdmi_clk_names_mt8195[MTK_MT8195_HDMI_CLK_COUNT];

extern struct drm_display_mode mode_720x480_60hz_4v3;
extern struct drm_display_mode mode_1280x720_50hz_16v9;
extern struct drm_display_mode mode_1920x1080_60hz_16v9;
extern struct drm_display_mode mode_3840x2160_30hz_16v9;

enum hdmi_hpd_state {
	HDMI_PLUG_OUT = 0,
	HDMI_PLUG_IN_AND_SINK_POWER_ON,
	HDMI_PLUG_IN_ONLY,
};

struct hdmi_audio_param {
	enum hdmi_audio_coding_type aud_codec;
	enum hdmi_audio_sample_size aud_sampe_size;
	enum hdmi_aud_input_type aud_input_type;
	enum hdmi_aud_i2s_fmt aud_i2s_fmt;
	enum hdmi_aud_mclk aud_mclk;
	enum hdmi_aud_channel_type aud_input_chan_type;
	struct hdmi_codec_params codec_params;
};

struct mtk_hdmi {
	struct drm_bridge bridge;
	struct drm_connector conn;
	struct device *dev;
	const struct mtk_hdmi_conf *conf;
	struct phy *phy;
	struct i2c_adapter *ddc_adpt;
	//TODO: array size is max(MTK_MT8183_HDMI_CLK_COUNT, MTK_MT8195_HDMI_CLK_COUNT)... dynamically allocate instead?
	struct clk *clk[MTK_MT8195_HDMI_CLK_COUNT];
	struct drm_display_mode mode;
	bool dvi_mode;
	struct regmap *sys_regmap;
	unsigned int sys_offset;
	void __iomem *regs;
	spinlock_t property_lock;
	struct drm_property *csp_depth_prop;
	u64 support_csp_depth;
	u64 set_csp_depth;
	enum hdmi_colorspace csp;
	enum hdmi_color_depth color_depth;
	enum hdmi_colorimetry colorimtery;
	enum hdmi_extended_colorimetry extended_colorimetry;
	enum hdmi_quantization_range quantization_range;
	enum hdmi_ycc_quantization_range ycc_quantization_range;

	bool powered;
	bool enabled;
	unsigned int hdmi_irq;
	enum hdmi_hpd_state hpd;

	bool hdmi_enabled;
	bool power_clk_enabled;
	bool irq_registered;

	/* Audio */
	struct hdmi_audio_param aud_param;
	bool audio_enable;

	/* imported from mt8183 TODO?*/
	struct drm_connector *curr_conn;/* current connector (only valid when 'enabled') */
	struct mutex update_plugged_status_lock;
	struct device *cec_dev;
	struct device *codec_dev;
	hdmi_codec_plugged_cb plugged_cb;
	struct drm_bridge *next_bridge;
};

struct mtk_hdmi *hdmi_ctx_from_bridge(struct drm_bridge *b);
u32 mtk_hdmi_read(struct mtk_hdmi *hdmi, u32 offset);
void mtk_hdmi_write(struct mtk_hdmi *hdmi, u32 offset, u32 val);
inline void mtk_hdmi_clear_bits(struct mtk_hdmi *hdmi, u32 offset, u32 bits);
inline void mtk_hdmi_set_bits(struct mtk_hdmi *hdmi, u32 offset, u32 bits);
void mtk_hdmi_mask(struct mtk_hdmi *hdmi, u32 offset, u32 val, u32 mask);
int mtk_hdmi_setup_spd_infoframe(struct mtk_hdmi *hdmi, u8 *buffer, size_t bufsz,
				 const char *vendor, const char *product);
void mtk_hdmi_send_infoframe(struct mtk_hdmi *hdmi, u8 *buffer_spd, size_t bufsz_spd,
			     u8 *buffer_avi, size_t bufsz_avi, struct drm_display_mode *mode);
int mtk_hdmi_get_all_clk(struct mtk_hdmi *hdmi, struct device_node *np, const char *const *clk_names,
			 size_t num_clocks);
void mtk_hdmi_show_EDID_raw_data(struct mtk_hdmi *hdmi, struct edid *edid);
struct edid *mtk_hdmi_bridge_get_edid(struct drm_bridge *bridge,
				      struct drm_connector *connector);
bool mtk_hdmi_bridge_mode_fixup(struct drm_bridge *bridge,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode);
void mtk_hdmi_bridge_mode_set(struct drm_bridge *bridge,
			      const struct drm_display_mode *mode,
			      const struct drm_display_mode *adjusted_mode);
int mtk_hdmi_dt_parse_pdata(struct mtk_hdmi *hdmi,
			    struct platform_device *pdev, const char *const *clk_names, size_t num_clocks);
int mtk_drm_hdmi_probe(struct platform_device *pdev);
int mtk_drm_hdmi_remove(struct platform_device *pdev);

//TODO: do better than this? function pointers?
extern const struct drm_bridge_funcs mtk_mt8195_hdmi_bridge_funcs;
extern const struct drm_bridge_funcs mtk_mt8183_hdmi_bridge_funcs;
void mtk_hdmi_output_init_mt8183(struct mtk_hdmi *hdmi);
void mtk_hdmi_output_init_mt8195(struct mtk_hdmi *hdmi);
void mtk_hdmi_clk_disable_mt8195(struct mtk_hdmi *hdmi);
void mtk_hdmi_clk_disable_mt8183(struct mtk_hdmi *hdmi);
void set_hdmi_codec_pdata_mt8195(struct hdmi_codec_pdata *codec_data);
void set_hdmi_codec_pdata_mt8183(struct hdmi_codec_pdata *codec_data);

#endif //_MTK_HDMI_COMMON_H
