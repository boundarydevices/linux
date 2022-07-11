/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Copyright (c) 2021 BayLibre, SAS
 */

#ifndef _MTK_HDMI_MT8195_CTRL_H
#define _MTK_HDMI_MT8195_CTRL_H

#include <linux/hdmi.h>
#include <drm/drm_bridge.h>

extern struct platform_driver mtk_hdmi_mt8195_ddc_driver;

/* struct mtk_hdmi_info is used to propagate blob property to userspace */
struct mtk_hdmi_info {
	unsigned short edid_sink_colorimetry;
	unsigned char edid_sink_rgb_color_bit;
	unsigned char edid_sink_ycbcr_color_bit;
	unsigned char ui1_sink_dc420_color_bit;
	unsigned short edid_sink_max_tmds_clock;
	unsigned short edid_sink_max_tmds_character_rate;
	unsigned char edid_sink_support_dynamic_hdr;
};

#endif /* _MTK_HDMI_MT8195_CTRL_H */
