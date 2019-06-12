/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_common/hdmi_parameters.c
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
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_common.h>
#include <linux/string.h>

static struct hdmi_format_para fmt_para_1920x1080p60_16x9 = {
	.vic = HDMI_1920x1080p60_16x9,
	.name = "1920x1080p60hz",
	.sname = "1080p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 148500,
	.timing = {
		.pixel_freq = 148500,
		.frac_freq = 148352,
		.h_freq = 67500,
		.v_freq = 60000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1920,
		.h_total = 2200,
		.h_blank = 280,
		.h_front = 88,
		.h_sync = 44,
		.h_back = 148,
		.v_active = 1080,
		.v_total = 1125,
		.v_blank = 45,
		.v_front = 4,
		.v_sync = 5,
		.v_back = 36,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1080p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 148500000,
		.htotal            = 2200,
		.vtotal            = 1125,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_1920x1080p30_16x9 = {
	.vic = HDMI_1920x1080p30_16x9,
	.name = "1920x1080p30hz",
	.sname = "1080p30hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 74250,
	.timing = {
		.pixel_freq = 74250,
		.frac_freq = 74176,
		.h_freq = 67500,
		.v_freq = 30000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1920,
		.h_total = 2200,
		.h_blank = 280,
		.h_front = 88,
		.h_sync = 44,
		.h_back = 148,
		.v_active = 1080,
		.v_total = 1125,
		.v_blank = 45,
		.v_front = 4,
		.v_sync = 5,
		.v_back = 36,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1080p30hz",
		.mode              = VMODE_HDMI,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 30,
		.sync_duration_den = 1,
		.video_clk         = 74250000,
		.htotal            = 2200,
		.vtotal            = 1125,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_1920x1080p50_16x9 = {
	.vic = HDMI_1920x1080p50_16x9,
	.name = "1920x1080p50hz",
	.sname = "1080p50hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 148500,
	.timing = {
		.pixel_freq = 148500,
		.h_freq = 56250,
		.v_freq = 50000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1920,
		.h_total = 2640,
		.h_blank = 720,
		.h_front = 528,
		.h_sync = 44,
		.h_back = 148,
		.v_active = 1080,
		.v_total = 1125,
		.v_blank = 45,
		.v_front = 4,
		.v_sync = 5,
		.v_back = 36,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1080p50hz",
		.mode              = VMODE_HDMI,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 148500000,
		.htotal            = 2640,
		.vtotal            = 1125,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_1920x1080p25_16x9 = {
	.vic = HDMI_1920x1080p25_16x9,
	.name = "1920x1080p25hz",
	.sname = "1080p25hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 74250,
	.timing = {
		.pixel_freq = 74250,
		.h_freq = 56250,
		.v_freq = 50000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1920,
		.h_total = 2640,
		.h_blank = 720,
		.h_front = 528,
		.h_sync = 44,
		.h_back = 148,
		.v_active = 1080,
		.v_total = 1125,
		.v_blank = 45,
		.v_front = 4,
		.v_sync = 5,
		.v_back = 36,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1080p25hz",
		.mode              = VMODE_HDMI,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 25,
		.sync_duration_den = 1,
		.video_clk         = 74250000,
		.htotal            = 2640,
		.vtotal            = 1125,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_1920x1080p24_16x9 = {
	.vic = HDMI_1920x1080p24_16x9,
	.name = "1920x1080p24hz",
	.sname = "1080p24hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 74250,
	.timing = {
		.pixel_freq = 74250,
		.frac_freq = 74176,
		.h_freq = 27000,
		.v_freq = 24000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1920,
		.h_total = 2750,
		.h_blank = 830,
		.h_front = 638,
		.h_sync = 44,
		.h_back = 148,
		.v_active = 1080,
		.v_total = 1125,
		.v_blank = 45,
		.v_front = 4,
		.v_sync = 5,
		.v_back = 36,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1080p24hz",
		.mode              = VMODE_HDMI,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 24,
		.sync_duration_den = 1,
		.video_clk         = 74250000,
		.htotal            = 2750,
		.vtotal            = 1125,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_3840x2160p30_16x9 = {
	.vic = HDMI_3840x2160p30_16x9,
	.name = "3840x2160p30hz",
	.sname = "2160p30hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 297000,
	.timing = {
		.pixel_freq = 297000,
		.frac_freq = 296703,
		.h_freq = 67500,
		.v_freq = 30000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 3840,
		.h_total = 4400,
		.h_blank = 560,
		.h_front = 176,
		.h_sync = 88,
		.h_back = 296,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "2160p30hz",
		.mode              = VMODE_HDMI,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 30,
		.sync_duration_den = 1,
		.video_clk         = 297000000,
		.htotal            = 4400,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_3840x2160p60_16x9 = {
	.vic = HDMI_3840x2160p60_16x9,
	.name = "3840x2160p60hz",
	.sname = "2160p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 1,
	.tmds_clk_div40 = 1,
	.tmds_clk = 594000,
	.timing = {
		.pixel_freq = 594000,
		.frac_freq = 593407,
		.h_freq = 135000,
		.v_freq = 60000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 3840,
		.h_total = 4400,
		.h_blank = 560,
		.h_front = 176,
		.h_sync = 88,
		.h_back = 296,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "2160p60hz",
		.mode              = VMODE_HDMI,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
		.htotal            = 4400,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_3840x2160p50_16x9 = {
	.vic = HDMI_3840x2160p50_16x9,
	.name = "3840x2160p50hz",
	.sname = "2160p50hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 1,
	.tmds_clk_div40 = 1,
	.tmds_clk = 594000,
	.timing = {
		.pixel_freq = 594000,
		.h_freq = 112500,
		.v_freq = 50000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 3840,
		.h_total = 5280,
		.h_blank = 1440,
		.h_front = 1056,
		.h_sync = 88,
		.h_back = 296,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "2160p50hz",
		.mode              = VMODE_HDMI,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
		.htotal            = 5280,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_3840x2160p25_16x9 = {
	.vic = HDMI_3840x2160p25_16x9,
	.name = "3840x2160p25hz",
	.sname = "2160p25hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 297000,
	.timing = {
		.pixel_freq = 297000,
		.h_freq = 56250,
		.v_freq = 25000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 3840,
		.h_total = 5280,
		.h_blank = 1440,
		.h_front = 1056,
		.h_sync = 88,
		.h_back = 296,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "2160p25hz",
		.mode              = VMODE_HDMI,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 25,
		.sync_duration_den = 1,
		.video_clk         = 297000000,
		.htotal            = 5280,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_3840x2160p24_16x9 = {
	.vic = HDMI_3840x2160p24_16x9,
	.name = "3840x2160p24hz",
	.sname = "2160p24hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 297000,
	.timing = {
		.pixel_freq = 297000,
		.frac_freq = 296703,
		.h_freq = 54000,
		.v_freq = 24000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 3840,
		.h_total = 5500,
		.h_blank = 1660,
		.h_front = 1276,
		.h_sync = 88,
		.h_back = 296,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "2160p24hz",
		.mode              = VMODE_HDMI,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 24,
		.sync_duration_den = 1,
		.video_clk         = 297000000,
		.htotal            = 5500,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_4096x2160p24_256x135 = {
	.vic = HDMI_4096x2160p24_256x135,
	.name = "4096x2160p24hz",
	.sname = "smpte24hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 297000,
	.timing = {
		.pixel_freq = 297000,
		.frac_freq = 296703,
		.h_freq = 54000,
		.v_freq = 24000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 4096,
		.h_total = 5500,
		.h_blank = 1404,
		.h_front = 1020,
		.h_sync = 88,
		.h_back = 296,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "smpte24hz",
		.mode              = VMODE_HDMI,
		.width             = 4096,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 24,
		.sync_duration_den = 1,
		.video_clk         = 297000000,
		.htotal            = 5500,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_4096x2160p25_256x135 = {
	.vic = HDMI_4096x2160p25_256x135,
	.name = "4096x2160p25hz",
	.sname = "smpte25hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 297000,
	.timing = {
		.pixel_freq = 297000,
		.h_freq = 56250,
		.v_freq = 25000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 4096,
		.h_total = 5280,
		.h_blank = 1184,
		.h_front = 968,
		.h_sync = 88,
		.h_back = 128,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "smpte25hz",
		.mode              = VMODE_HDMI,
		.width             = 4096,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 25,
		.sync_duration_den = 1,
		.video_clk         = 297000000,
		.htotal            = 5280,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_4096x2160p30_256x135 = {
	.vic = HDMI_4096x2160p30_256x135,
	.name = "4096x2160p30hz",
	.sname = "smpte30hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 297000,
	.timing = {
		.pixel_freq = 297000,
		.frac_freq = 296703,
		.h_freq = 67500,
		.v_freq = 30000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 4096,
		.h_total = 4400,
		.h_blank = 304,
		.h_front = 88,
		.h_sync = 88,
		.h_back = 128,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "smpte30hz",
		.mode              = VMODE_HDMI,
		.width             = 4096,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 30,
		.sync_duration_den = 1,
		.video_clk         = 297000000,
		.htotal            = 4400,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_4096x2160p50_256x135 = {
	.vic = HDMI_4096x2160p50_256x135,
	.name = "4096x2160p50hz",
	.sname = "smpte50hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 1,
	.tmds_clk_div40 = 1,
	.tmds_clk = 594000,
	.timing = {
		.pixel_freq = 594000,
		.h_freq = 112500,
		.v_freq = 50000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 4096,
		.h_total = 5280,
		.h_blank = 1184,
		.h_front = 968,
		.h_sync = 88,
		.h_back = 128,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "smpte50hz",
		.mode              = VMODE_HDMI,
		.width             = 4096,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
		.htotal            = 5280,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_4096x2160p60_256x135 = {
	.vic = HDMI_4096x2160p60_256x135,
	.name = "4096x2160p60hz",
	.sname = "smpte60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 1,
	.tmds_clk_div40 = 1,
	.tmds_clk = 594000,
	.timing = {
		.pixel_freq = 594000,
		.frac_freq = 593407,
		.h_freq = 135000,
		.v_freq = 60000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 4096,
		.h_total = 4400,
		.h_blank = 304,
		.h_front = 88,
		.h_sync = 88,
		.h_back = 128,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "smpte60hz",
		.mode              = VMODE_HDMI,
		.width             = 4096,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
		.htotal            = 4400,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_1920x1080i60_16x9 = {
	.vic = HDMI_1920x1080i60_16x9,
	.name = "1920x1080i60hz",
	.sname = "1080i60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 0,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 74250,
	.timing = {
		.pixel_freq = 74250,
		.frac_freq = 74176,
		.h_freq = 33750,
		.v_freq = 60000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1920,
		.h_total = 2200,
		.h_blank = 280,
		.h_front = 88,
		.h_sync = 44,
		.h_back = 148,
		.v_active = 1080/2,
		.v_total = 1125,
		.v_blank = 45/2,
		.v_front = 2,
		.v_sync = 5,
		.v_back = 15,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1080i60hz",
		.mode              = VMODE_HDMI,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 540,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 148500000,
		.htotal            = 2200,
		.vtotal            = 1125,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_1920x1080i50_16x9 = {
	.vic = HDMI_1920x1080i50_16x9,
	.name = "1920x1080i50hz",
	.sname = "1080i50hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 0,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 74250,
	.timing = {
		.pixel_freq = 74250,
		.h_freq = 28125,
		.v_freq = 50000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1920,
		.h_total = 2640,
		.h_blank = 720,
		.h_front = 528,
		.h_sync = 44,
		.h_back = 148,
		.v_active = 1080/2,
		.v_total = 1125,
		.v_blank = 45/2,
		.v_front = 2,
		.v_sync = 5,
		.v_back = 15,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1080i50hz",
		.mode              = VMODE_HDMI,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 540,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 148500000,
		.htotal            = 2640,
		.vtotal            = 1125,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_1280x720p60_16x9 = {
	.vic = HDMI_1280x720p60_16x9,
	.name = "1280x720p60hz",
	.sname = "720p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 74250,
	.timing = {
		.pixel_freq = 74250,
		.frac_freq = 74176,
		.h_freq = 45000,
		.v_freq = 60000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1280,
		.h_total = 1650,
		.h_blank = 370,
		.h_front = 110,
		.h_sync = 40,
		.h_back = 220,
		.v_active = 720,
		.v_total = 750,
		.v_blank = 30,
		.v_front = 5,
		.v_sync = 5,
		.v_back = 20,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "720p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1280,
		.height            = 720,
		.field_height      = 720,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 74250000,
		.htotal            = 1650,
		.vtotal            = 750,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_1280x720p50_16x9 = {
	.vic = HDMI_1280x720p50_16x9,
	.name = "1280x720p50hz",
	.sname = "720p50hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 74250,
	.timing = {
		.pixel_freq = 74250,
		.h_freq = 37500,
		.v_freq = 50000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1280,
		.h_total = 1980,
		.h_blank = 700,
		.h_front = 440,
		.h_sync = 40,
		.h_back = 220,
		.v_active = 720,
		.v_total = 750,
		.v_blank = 30,
		.v_front = 5,
		.v_sync = 5,
		.v_back = 20,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "720p50hz",
		.mode              = VMODE_HDMI,
		.width             = 1280,
		.height            = 720,
		.field_height      = 720,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 74250000,
		.htotal            = 1980,
		.vtotal            = 750,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_720x480p60_16x9 = {
	.vic = HDMI_720x480p60_16x9,
	.name = "720x480p60hz",
	.sname = "480p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 27027,
	.timing = {
		.pixel_freq = 27027,
		.frac_freq = 27000,
		.h_freq = 31469,
		.v_freq = 59940,
		.vsync_polarity = 0,
		.hsync_polarity = 0,
		.h_active = 720,
		.h_total = 858,
		.h_blank = 138,
		.h_front = 16,
		.h_sync = 62,
		.h_back = 60,
		.v_active = 480,
		.v_total = 525,
		.v_blank = 45,
		.v_front = 9,
		.v_sync = 6,
		.v_back = 30,
		.v_sync_ln = 7,
	},
	.hdmitx_vinfo = {
		.name              = "480p60hz",
		.mode              = VMODE_HDMI,
		.width             = 720,
		.height            = 480,
		.field_height      = 480,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
		.htotal            = 858,
		.vtotal            = 525,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_720x480i60_16x9 = {
	.vic = HDMI_720x480i60_16x9,
	.name = "720x480i60hz",
	.sname = "480i60hz",
	.pixel_repetition_factor = 1,
	.progress_mode = 0,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 27027,
	.timing = {
		.pixel_freq = 27027,
		.frac_freq = 27000,
		.h_freq = 15734,
		.v_freq = 59940,
		.vsync_polarity = 0,
		.hsync_polarity = 0,
		.h_active = 1440,
		.h_total = 1716,
		.h_blank = 276,
		.h_front = 38,
		.h_sync = 124,
		.h_back = 114,
		.v_active = 480/2,
		.v_total = 525,
		.v_blank = 45/2,
		.v_front = 4,
		.v_sync = 3,
		.v_back = 15,
		.v_sync_ln = 4,
	},
	.hdmitx_vinfo = {
		.name              = "480i60hz",
		.mode              = VMODE_HDMI,
		.width             = 720,
		.height            = 480,
		.field_height      = 240,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
		.htotal            = 1716,
		.vtotal            = 525,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCI,
	},
};

static struct hdmi_format_para fmt_para_720x576p50_16x9 = {
	.vic = HDMI_720x576p50_16x9,
	.name = "720x576p50hz",
	.sname = "576p50hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 27000,
	.timing = {
		.pixel_freq = 27000,
		.h_freq = 31250,
		.v_freq = 50000,
		.vsync_polarity = 0,
		.hsync_polarity = 0,
		.h_active = 720,
		.h_total = 864,
		.h_blank = 144,
		.h_front = 12,
		.h_sync = 64,
		.h_back = 68,
		.v_active = 576,
		.v_total = 625,
		.v_blank = 49,
		.v_front = 5,
		.v_sync = 5,
		.v_back = 39,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "576p50hz",
		.mode              = VMODE_HDMI,
		.width             = 720,
		.height            = 576,
		.field_height      = 576,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
		.htotal            = 864,
		.vtotal            = 625,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_720x576i50_16x9 = {
	.vic = HDMI_720x576i50_16x9,
	.name = "720x576i50hz",
	.sname = "576i50hz",
	.pixel_repetition_factor = 1,
	.progress_mode = 0,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 27000,
	.timing = {
		.pixel_freq = 27000,
		.h_freq = 15625,
		.v_freq = 50000,
		.vsync_polarity = 0,
		.hsync_polarity = 0,
		.h_active = 1440,
		.h_total = 1728,
		.h_blank = 288,
		.h_front = 24,
		.h_sync = 126,
		.h_back = 138,
		.v_active = 576/2,
		.v_total = 625,
		.v_blank = 49/2,
		.v_front = 2,
		.v_sync = 3,
		.v_back = 19,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "576i50hz",
		.mode              = VMODE_HDMI,
		.width             = 720,
		.height            = 576,
		.field_height      = 288,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
		.htotal            = 1728,
		.vtotal            = 625,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCI,
	},
};

/* the following are for Y420 mode*/

static struct hdmi_format_para fmt_para_3840x2160p50_16x9_y420 = {
	.vic = HDMI_3840x2160p50_16x9_Y420,
	.name = "3840x2160p50hz420",
	.sname = "2160p50hz420",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 594000,
	.timing = {
		.pixel_freq = 594000,
		.h_freq = 112500,
		.v_freq = 50000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 3840,
		.h_total = 5280,
		.h_blank = 1440,
		.h_front = 1056,
		.h_sync = 88,
		.h_back = 296,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "2160p50hz420",
		.mode              = VMODE_HDMI,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
		.htotal            = 5280,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_4096x2160p50_256x135_y420 = {
	.vic = HDMI_4096x2160p50_256x135_Y420,
	.name = "4096x2160p50hz420",
	.sname = "smpte50hz420",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 594000,
	.timing = {
		.pixel_freq = 594000,
		.h_freq = 112500,
		.v_freq = 50000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 4096,
		.h_total = 5280,
		.h_blank = 1184,
		.h_front = 968,
		.h_sync = 88,
		.h_back = 296,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "smpte50hz420",
		.mode              = VMODE_HDMI,
		.width             = 4096,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
		.htotal            = 5280,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_3840x2160p60_16x9_y420 = {
	.vic = HDMI_3840x2160p60_16x9_Y420,
	.name = "3840x2160p60hz420",
	.sname = "2160p60hz420",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 594000,
	.timing = {
		.pixel_freq = 594000,
		.frac_freq = 593407,
		.h_freq = 135000,
		.v_freq = 60000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 3840,
		.h_total = 4400,
		.h_blank = 560,
		.h_front = 176,
		.h_sync = 88,
		.h_back = 296,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "2160p60hz420",
		.mode              = VMODE_HDMI,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
		.htotal            = 4400,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};


static struct hdmi_format_para fmt_para_4096x2160p60_256x135_y420 = {
	.vic = HDMI_4096x2160p60_256x135_Y420,
	.name = "4096x2160p60hz420",
	.sname = "smpte60hz420",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 594000,
	.timing = {
		.pixel_freq = 594000,
		.frac_freq = 593407,
		.h_freq = 135000,
		.v_freq = 60000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 4096,
		.h_total = 4400,
		.h_blank = 304,
		.h_front = 88,
		.h_sync = 88,
		.h_back = 128,
		.v_active = 2160,
		.v_total = 2250,
		.v_blank = 90,
		.v_front = 8,
		.v_sync = 10,
		.v_back = 72,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "smpte60hz420",
		.mode              = VMODE_HDMI,
		.width             = 4096,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
		.htotal            = 4400,
		.vtotal            = 2250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_non_hdmi_fmt = {
	.vic = HDMI_Unknown,
	.name = "invalid",
	.sname = "invalid",
	.hdmitx_vinfo = {
		.name              = "invalid",
		.mode              = VMODE_MAX,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 148500000,
		.htotal            = 2200,
		.vtotal            = 1125,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

/* null mode is used for HDMI, such as current mode is 1080p24hz
 * but want to switch to 1080p 23.976hz
 * so 'echo null > /sys/class/display/mode' firstly, then set
 * 'echo 1 > /sys/class/amhdmitx/amhdmitx0/frac_rate_policy'
 * and 'echo 1080p24hz > /sys/class/display/mode'
 */
static struct hdmi_format_para fmt_para_null_hdmi_fmt = {
	.vic = HDMI_Unknown,
	.name = "null",
	.sname = "null",
	.hdmitx_vinfo = {
		.name              = "null",
		.mode              = VMODE_HDMI,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 148500000,
		.htotal            = 2200,
		.vtotal            = 1125,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_2560x1080p50_64x27 = {
	.vic = HDMI_2560x1080p50_64x27,
	.name = "2560x1080p50hz",
	.sname = "2560x1080p50hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 185625,
	.timing = {
		.pixel_freq = 185625,
		.h_freq = 56250,
		.v_freq = 50000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 2560,
		.h_total = 3300,
		.h_blank = 740,
		.h_front = 548,
		.h_sync = 44,
		.h_back = 148,
		.v_active = 1080,
		.v_total = 1125,
		.v_blank = 45,
		.v_front = 4,
		.v_sync = 5,
		.v_back = 36,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "2560x1080p50hz",
		.mode              = VMODE_HDMI,
		.width             = 2560,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 64,
		.aspect_ratio_den  = 27,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 185625000,
		.htotal            = 3300,
		.vtotal            = 1125,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_2560x1080p60_64x27 = {
	.vic = HDMI_2560x1080p60_64x27,
	.name = "2560x1080p60hz",
	.sname = "2560x1080p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 198000,
	.timing = {
		.pixel_freq = 198000,
		.h_freq = 66000,
		.v_freq = 60000,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 2560,
		.h_total = 3000,
		.h_blank = 440,
		.h_front = 248,
		.h_sync = 44,
		.h_back = 148,
		.v_active = 1080,
		.v_total = 1100,
		.v_blank = 20,
		.v_front = 4,
		.v_sync = 5,
		.v_back = 11,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "2560x1080p60hz",
		.mode              = VMODE_HDMI,
		.width             = 2560,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 64,
		.aspect_ratio_den  = 27,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 198000000,
		.htotal            = 3000,
		.vtotal            = 1100,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

/*
 * VESA timing describe
 */
static struct hdmi_format_para fmt_para_vesa_640x480p60_4x3 = {
	.vic = HDMIV_640x480p60hz,
	.name = "640x480p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 25175,
	.timing = {
		.pixel_freq = 25175,
		.h_freq = 26218,
		.v_freq = 59940,
		.vsync = 60,
		.vsync_polarity = 0,
		.hsync_polarity = 0,
		.h_active = 640,
		.h_total = 800,
		.h_blank = 160,
		.h_front = 16,
		.h_sync = 96,
		.h_back = 48,
		.v_active = 480,
		.v_total = 525,
		.v_blank = 45,
		.v_front = 10,
		.v_sync = 2,
		.v_back = 33,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "640x480p60hz",
		.mode              = VMODE_HDMI,
		.width             = 640,
		.height            = 480,
		.field_height      = 480,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 25175000,
		.htotal            = 800,
		.vtotal            = 525,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_800x480p60_4x3 = {
	.vic = HDMIV_800x480p60hz,
	.name = "800x480p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 29760,
	.timing = {
		.pixel_freq = 29760,
		.h_freq = 30000,
		.v_freq = 60000,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 800,
		.h_total = 992,
		.h_blank = 192,
		.h_front = 24,
		.h_sync = 72,
		.h_back = 96,
		.v_active = 480,
		.v_total = 500,
		.v_blank = 20,
		.v_front = 3,
		.v_sync = 7,
		.v_back = 10,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "800x480p60hz",
		.mode              = VMODE_HDMI,
		.width             = 800,
		.height            = 480,
		.field_height      = 480,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 29760000,
		.htotal            = 992,
		.vtotal            = 500,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_800x600p60_4x3 = {
	.vic = HDMIV_800x600p60hz,
	.name = "800x600p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 40000,
	.timing = {
		.pixel_freq = 66666,
		.h_freq = 37879,
		.v_freq = 60317,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 800,
		.h_total = 1056,
		.h_blank = 256,
		.h_front = 40,
		.h_sync = 128,
		.h_back = 88,
		.v_active = 600,
		.v_total = 628,
		.v_blank = 28,
		.v_front = 1,
		.v_sync = 4,
		.v_back = 23,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "800x600p60hz",
		.mode              = VMODE_HDMI,
		.width             = 800,
		.height            = 600,
		.field_height      = 600,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 66666000,
		.htotal            = 1056,
		.vtotal            = 628,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_852x480p60_213x120 = {
	.vic = HDMIV_852x480p60hz,
	.name = "852x480p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 30240,
	.timing = {
		.pixel_freq = 30240,
		.h_freq = 31900,
		.v_freq = 59960,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 852,
		.h_total = 948,
		.h_blank = 96,
		.h_front = 40,
		.h_sync = 16,
		.h_back = 40,
		.v_active = 480,
		.v_total = 532,
		.v_blank = 52,
		.v_front = 10,
		.v_sync = 2,
		.v_back = 40,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "852x480p60hz",
		.mode              = VMODE_HDMI,
		.width             = 852,
		.height            = 480,
		.field_height      = 480,
		.aspect_ratio_num  = 213,
		.aspect_ratio_den  = 120,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 30240000,
		.htotal            = 948,
		.vtotal            = 532,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_854x480p60_427x240 = {
	.vic = HDMIV_854x480p60hz,
	.name = "854x480p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 30240,
	.timing = {
		.pixel_freq = 30240,
		.h_freq = 31830,
		.v_freq = 59950,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 854,
		.h_total = 950,
		.h_blank = 96,
		.h_front = 40,
		.h_sync = 16,
		.h_back = 40,
		.v_active = 480,
		.v_total = 531,
		.v_blank = 51,
		.v_front = 10,
		.v_sync = 2,
		.v_back = 39,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "854x480p60hz",
		.mode              = VMODE_HDMI,
		.width             = 854,
		.height            = 480,
		.field_height      = 480,
		.aspect_ratio_num  = 427,
		.aspect_ratio_den  = 240,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 30240000,
		.htotal            = 950,
		.vtotal            = 531,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1024x600p60_17x10 = {
	.vic = HDMIV_1024x600p60hz,
	.name = "1024x600p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 50400,
	.timing = {
		.pixel_freq = 50400,
		.h_freq = 38280,
		.v_freq = 60000,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1024,
		.h_total = 1344,
		.h_blank = 320,
		.h_front = 24,
		.h_sync = 136,
		.h_back = 160,
		.v_active = 600,
		.v_total = 638,
		.v_blank = 38,
		.v_front = 3,
		.v_sync = 6,
		.v_back = 29,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1024x600p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1024,
		.height            = 600,
		.field_height      = 600,
		.aspect_ratio_num  = 17,
		.aspect_ratio_den  = 10,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 50400000,
		.htotal            = 1344,
		.vtotal            = 638,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1024x768p60_4x3 = {
	.vic = HDMIV_1024x768p60hz,
	.name = "1024x768p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 79500,
	.timing = {
		.pixel_freq = 79500,
		.h_freq = 48360,
		.v_freq = 60004,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1024,
		.h_total = 1344,
		.h_blank = 320,
		.h_front = 24,
		.h_sync = 136,
		.h_back = 160,
		.v_active = 768,
		.v_total = 806,
		.v_blank = 38,
		.v_front = 3,
		.v_sync = 6,
		.v_back = 29,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1024x768p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1024,
		.height            = 768,
		.field_height      = 768,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 79500000,
		.htotal            = 1344,
		.vtotal            = 806,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1152x864p75_4x3 = {
	.vic = HDMIV_1152x864p75hz,
	.name = "1152x864p75hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 108000,
	.timing = {
		.pixel_freq = 108000,
		.h_freq = 67500,
		.v_freq = 75000,
		.vsync = 75,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1152,
		.h_total = 1600,
		.h_blank = 448,
		.h_front = 64,
		.h_sync = 128,
		.h_back = 256,
		.v_active = 864,
		.v_total = 900,
		.v_blank = 36,
		.v_front = 1,
		.v_sync = 3,
		.v_back = 32,
	},
	.hdmitx_vinfo = {
		.name              = "1152x864p75hz",
		.mode              = VMODE_HDMI,
		.width             = 1152,
		.height            = 864,
		.field_height      = 864,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 75,
		.sync_duration_den = 1,
		.video_clk         = 108000000,
		.htotal            = 1600,
		.vtotal            = 900,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1280x768p60_5x3 = {
	.vic = HDMIV_1280x768p60hz,
	.name = "1280x768p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 79500,
	.timing = {
		.pixel_freq = 79500,
		.h_freq = 47776,
		.v_freq = 59870,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1280,
		.h_total = 1664,
		.h_blank = 384,
		.h_front = 64,
		.h_sync = 128,
		.h_back = 192,
		.v_active = 768,
		.v_total = 798,
		.v_blank = 30,
		.v_front = 3,
		.v_sync = 7,
		.v_back = 20,
	},
	.hdmitx_vinfo = {
		.name              = "1280x768p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1280,
		.height            = 768,
		.field_height      = 768,
		.aspect_ratio_num  = 5,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 79500000,
		.htotal            = 1664,
		.vtotal            = 798,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1280x800p60_8x5 = {
	.vic = HDMIV_1280x800p60hz,
	.name = "1280x800p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 83500,
	.timing = {
		.pixel_freq = 83500,
		.h_freq = 49380,
		.v_freq = 59910,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1280,
		.h_total = 1440,
		.h_blank = 160,
		.h_front = 48,
		.h_sync = 32,
		.h_back = 80,
		.v_active = 800,
		.v_total = 823,
		.v_blank = 23,
		.v_front = 3,
		.v_sync = 6,
		.v_back = 14,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1280x800p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1280,
		.height            = 800,
		.field_height      = 800,
		.aspect_ratio_num  = 8,
		.aspect_ratio_den  = 5,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 83500000,
		.htotal            = 1440,
		.vtotal            = 823,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1280x960p60_4x3 = {
	.vic = HDMIV_1280x960p60hz,
	.name = "1280x960p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 108000,
	.timing = {
		.pixel_freq = 108000,
		.h_freq = 60000,
		.v_freq = 60000,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1280,
		.h_total = 1800,
		.h_blank = 520,
		.h_front = 96,
		.h_sync = 112,
		.h_back = 312,
		.v_active = 960,
		.v_total = 1000,
		.v_blank = 40,
		.v_front = 1,
		.v_sync = 3,
		.v_back = 36,
	},
	.hdmitx_vinfo = {
		.name              = "1280x960p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1280,
		.height            = 960,
		.field_height      = 960,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 108000000,
		.htotal            = 1800,
		.vtotal            = 1000,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1280x1024p60_5x4 = {
	.vic = HDMIV_1280x1024p60hz,
	.name = "1280x1024p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 108000,
	.timing = {
		.pixel_freq = 108000,
		.h_freq = 64080,
		.v_freq = 60020,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1280,
		.h_total = 1688,
		.h_blank = 408,
		.h_front = 48,
		.h_sync = 112,
		.h_back = 248,
		.v_active = 1024,
		.v_total = 1066,
		.v_blank = 42,
		.v_front = 1,
		.v_sync = 3,
		.v_back = 38,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1280x1024p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1280,
		.height            = 1024,
		.field_height      = 1024,
		.aspect_ratio_num  = 5,
		.aspect_ratio_den  = 4,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 108000000,
		.htotal            = 1688,
		.vtotal            = 1066,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1360x768p60_16x9 = {
	.vic = HDMIV_1360x768p60hz,
	.name = "1360x768p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 855000,
	.timing = {
		.pixel_freq = 855000,
		.h_freq = 47700,
		.v_freq = 60015,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1360,
		.h_total = 1792,
		.h_blank = 432,
		.h_front = 64,
		.h_sync = 112,
		.h_back = 256,
		.v_active = 768,
		.v_total = 795,
		.v_blank = 27,
		.v_front = 3,
		.v_sync = 6,
		.v_back = 18,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1360x768p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1360,
		.height            = 768,
		.field_height      = 768,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 85500000,
		.htotal            = 1792,
		.vtotal            = 795,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1366x768p60_16x9 = {
	.vic = HDMIV_1366x768p60hz,
	.name = "1366x768p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 85500,
	.timing = {
		.pixel_freq = 85500,
		.h_freq = 47880,
		.v_freq = 59790,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1366,
		.h_total = 1792,
		.h_blank = 426,
		.h_front = 70,
		.h_sync = 143,
		.h_back = 213,
		.v_active = 768,
		.v_total = 798,
		.v_blank = 30,
		.v_front = 3,
		.v_sync = 3,
		.v_back = 24,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1366x768p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1366,
		.height            = 768,
		.field_height      = 768,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 85500000,
		.htotal            = 1792,
		.vtotal            = 798,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1400x1050p60_4x3 = {
	.vic = HDMIV_1400x1050p60hz,
	.name = "1400x1050p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 121750,
	.timing = {
		.pixel_freq = 121750,
		.h_freq = 65317,
		.v_freq = 59978,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1400,
		.h_total = 1864,
		.h_blank = 464,
		.h_front = 88,
		.h_sync = 144,
		.h_back = 232,
		.v_active = 1050,
		.v_total = 1089,
		.v_blank = 39,
		.v_front = 3,
		.v_sync = 4,
		.v_back = 32,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1400x1050p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1400,
		.height            = 1050,
		.field_height      = 1050,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 121750000,
		.htotal            = 1864,
		.vtotal            = 1089,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1440x900p60_8x5 = {
	.vic = HDMIV_1440x900p60hz,
	.name = "1440x900p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 106500,
	.timing = {
		.pixel_freq = 106500,
		.h_freq = 56040,
		.v_freq = 59887,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1440,
		.h_total = 1904,
		.h_blank = 464,
		.h_front = 80,
		.h_sync = 152,
		.h_back = 232,
		.v_active = 900,
		.v_total = 934,
		.v_blank = 34,
		.v_front = 3,
		.v_sync = 6,
		.v_back = 25,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1440x900p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1440,
		.height            = 900,
		.field_height      = 900,
		.aspect_ratio_num  = 8,
		.aspect_ratio_den  = 5,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 106500000,
		.htotal            = 1904,
		.vtotal            = 934,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1440x2560p60_9x16 = {
	.vic = HDMIV_1440x2560p60hz,
	.name = "1440x2560p60hz",
	.sname = "1440x2560p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 244850,
	.timing = {
		.pixel_freq = 244850,
		.h_freq = 155760,
		.v_freq = 59999,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1440,
		.h_total = 1572,
		.h_blank = 132,
		.h_front = 64,
		.h_sync = 4,
		.h_back = 64,
		.v_active = 2560,
		.v_total = 2596,
		.v_blank = 36,
		.v_front = 16,
		.v_sync = 4,
		.v_back = 16,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1440x2560p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1440,
		.height            = 2560,
		.field_height      = 2560,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 244850000,
		.htotal            = 1572,
		.vtotal            = 2596,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1600x900p60_16x9 = {
	.vic = HDMIV_1600x900p60hz,
	.name = "1600x900p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 108000,
	.timing = {
		.pixel_freq = 108000,
		.h_freq = 60000,
		.v_freq = 60000,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1600,
		.h_total = 1800,
		.h_blank = 200,
		.h_front = 24,
		.h_sync = 80,
		.h_back = 96,
		.v_active = 900,
		.v_total = 1000,
		.v_blank = 100,
		.v_front = 1,
		.v_sync = 3,
		.v_back = 96,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1600x900p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1600,
		.height            = 900,
		.field_height      = 900,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 108000000,
		.htotal            = 1800,
		.vtotal            = 1000,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1600x1200p60_4x3 = {
	.vic = HDMIV_1600x1200p60hz,
	.name = "1600x1200p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 162000,
	.timing = {
		.pixel_freq = 162000,
		.h_freq = 75000,
		.v_freq = 60000,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1600,
		.h_total = 2160,
		.h_blank = 560,
		.h_front = 64,
		.h_sync = 192,
		.h_back = 304,
		.v_active = 1200,
		.v_total = 1250,
		.v_blank = 50,
		.v_front = 1,
		.v_sync = 3,
		.v_back = 46,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1600x1200p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1600,
		.height            = 1200,
		.field_height      = 1200,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 162000000,
		.htotal            = 2160,
		.vtotal            = 1250,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1680x1050p60_8x5 = {
	.vic = HDMIV_1680x1050p60hz,
	.name = "1680x1050p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 146250,
	.timing = {
		.pixel_freq = 146250,
		.h_freq = 65340,
		.v_freq = 59954,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1680,
		.h_total = 2240,
		.h_blank = 560,
		.h_front = 104,
		.h_sync = 176,
		.h_back = 280,
		.v_active = 1050,
		.v_total = 1089,
		.v_blank = 39,
		.v_front = 3,
		.v_sync = 6,
		.v_back = 30,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1680x1050p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1680,
		.height            = 1050,
		.field_height      = 1050,
		.aspect_ratio_num  = 8,
		.aspect_ratio_den  = 5,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 146250000,
		.htotal            = 2240,
		.vtotal            = 1089,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_1920x1200p60_8x5 = {
	.vic = HDMIV_1920x1200p60hz,
	.name = "1920x1200p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 193250,
	.timing = {
		.pixel_freq = 193250,
		.h_freq = 74700,
		.v_freq = 59885,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 1920,
		.h_total = 2592,
		.h_blank = 672,
		.h_front = 136,
		.h_sync = 200,
		.h_back = 336,
		.v_active = 1200,
		.v_total = 1245,
		.v_blank = 45,
		.v_front = 3,
		.v_sync = 6,
		.v_back = 36,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "1920x1200p60hz",
		.mode              = VMODE_HDMI,
		.width             = 1920,
		.height            = 1200,
		.field_height      = 1200,
		.aspect_ratio_num  = 8,
		.aspect_ratio_den  = 5,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 193250000,
		.htotal            = 2592,
		.vtotal            = 1245,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_2160x1200p90_9x5 = {
	.vic = HDMIV_2160x1200p90hz,
	.name = "2160x1200p90hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 268550,
	.timing = {
		.pixel_freq = 268550,
		.h_freq = 109080,
		.v_freq = 90000,
		.vsync = 90,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 2160,
		.h_total = 2462,
		.h_blank = 302,
		.h_front = 190,
		.h_sync = 32,
		.h_back = 80,
		.v_active = 1200,
		.v_total = 1212,
		.v_blank = 12,
		.v_front = 6,
		.v_sync = 3,
		.v_back = 3,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "2160x1200p90hz",
		.mode              = VMODE_HDMI,
		.width             = 2160,
		.height            = 1200,
		.field_height      = 1200,
		.aspect_ratio_num  = 9,
		.aspect_ratio_den  = 5,
		.sync_duration_num = 90,
		.sync_duration_den = 1,
		.video_clk         = 268550000,
		.htotal            = 2462,
		.vtotal            = 1212,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para fmt_para_vesa_2560x1600p60_8x5 = {
	.vic = HDMIV_2560x1600p60hz,
	.name = "2560x1600p60hz",
	.pixel_repetition_factor = 0,
	.progress_mode = 1,
	.scrambler_en = 0,
	.tmds_clk_div40 = 0,
	.tmds_clk = 348500,
	.timing = {
		.pixel_freq = 348500,
		.h_freq = 99458,
		.v_freq = 59987,
		.vsync = 60,
		.vsync_polarity = 1,
		.hsync_polarity = 1,
		.h_active = 2560,
		.h_total = 3504,
		.h_blank = 944,
		.h_front = 192,
		.h_sync = 280,
		.h_back = 472,
		.v_active = 1600,
		.v_total = 1658,
		.v_blank = 58,
		.v_front = 3,
		.v_sync = 6,
		.v_back = 49,
		.v_sync_ln = 1,
	},
	.hdmitx_vinfo = {
		.name              = "2560x1600p60hz",
		.mode              = VMODE_HDMI,
		.width             = 2560,
		.height            = 1600,
		.field_height      = 1600,
		.aspect_ratio_num  = 8,
		.aspect_ratio_den  = 5,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 348500000,
		.htotal            = 3504,
		.vtotal            = 1658,
		.fr_adj_type       = VOUT_FR_ADJ_HDMI,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
	},
};

static struct hdmi_format_para *all_fmt_paras[] = {
	&fmt_para_3840x2160p60_16x9,
	&fmt_para_3840x2160p50_16x9,
	&fmt_para_3840x2160p30_16x9,
	&fmt_para_3840x2160p25_16x9,
	&fmt_para_3840x2160p24_16x9,
	&fmt_para_4096x2160p24_256x135,
	&fmt_para_4096x2160p25_256x135,
	&fmt_para_4096x2160p30_256x135,
	&fmt_para_4096x2160p50_256x135,
	&fmt_para_4096x2160p60_256x135,
	&fmt_para_1920x1080p25_16x9,
	&fmt_para_1920x1080p30_16x9,
	&fmt_para_1920x1080p50_16x9,
	&fmt_para_1920x1080p60_16x9,
	&fmt_para_1920x1080p24_16x9,
	&fmt_para_1920x1080i60_16x9,
	&fmt_para_1920x1080i50_16x9,
	&fmt_para_1280x720p60_16x9,
	&fmt_para_1280x720p50_16x9,
	&fmt_para_720x480p60_16x9,
	&fmt_para_720x480i60_16x9,
	&fmt_para_720x576p50_16x9,
	&fmt_para_720x576i50_16x9,
	&fmt_para_3840x2160p60_16x9_y420,
	&fmt_para_4096x2160p60_256x135_y420,
	&fmt_para_3840x2160p50_16x9_y420,
	&fmt_para_4096x2160p50_256x135_y420,
	&fmt_para_2560x1080p50_64x27,
	&fmt_para_2560x1080p60_64x27,
	&fmt_para_vesa_640x480p60_4x3,
	&fmt_para_vesa_800x480p60_4x3,
	&fmt_para_vesa_800x600p60_4x3,
	&fmt_para_vesa_852x480p60_213x120,
	&fmt_para_vesa_854x480p60_427x240,
	&fmt_para_vesa_1024x600p60_17x10,
	&fmt_para_vesa_1024x768p60_4x3,
	&fmt_para_vesa_1152x864p75_4x3,
	&fmt_para_vesa_1280x768p60_5x3,
	&fmt_para_vesa_1280x800p60_8x5,
	&fmt_para_vesa_1280x960p60_4x3,
	&fmt_para_vesa_1280x1024p60_5x4,
	&fmt_para_vesa_1360x768p60_16x9,
	&fmt_para_vesa_1366x768p60_16x9,
	&fmt_para_vesa_1400x1050p60_4x3,
	&fmt_para_vesa_1440x900p60_8x5,
	&fmt_para_vesa_1440x2560p60_9x16,
	&fmt_para_vesa_1600x900p60_16x9,
	&fmt_para_vesa_1600x1200p60_4x3,
	&fmt_para_vesa_1680x1050p60_8x5,
	&fmt_para_vesa_1920x1200p60_8x5,
	&fmt_para_vesa_2160x1200p90_9x5,
	&fmt_para_vesa_2560x1600p60_8x5,
	&fmt_para_null_hdmi_fmt,
	&fmt_para_non_hdmi_fmt,
	NULL,
};

struct hdmi_format_para *hdmi_get_fmt_paras(enum hdmi_vic vic)
{
	int i;

	for (i = 0; all_fmt_paras[i] != NULL; i++) {
		if (vic == all_fmt_paras[i]->vic)
			return all_fmt_paras[i];
	}
	return &fmt_para_non_hdmi_fmt;
}

struct hdmi_format_para *hdmi_match_dtd_paras(struct dtd *t)
{
	int i;

	if (!t)
		return NULL;
	for (i = 0; all_fmt_paras[i]; i++) {
		if ((abs(all_fmt_paras[i]->timing.pixel_freq / 10
		    - t->pixel_clock) <= (t->pixel_clock + 1000) / 1000) &&
		    (t->h_active == all_fmt_paras[i]->timing.h_active) &&
		    (t->h_blank == all_fmt_paras[i]->timing.h_blank) &&
		    (t->v_active == all_fmt_paras[i]->timing.v_active) &&
		    (t->v_blank == all_fmt_paras[i]->timing.v_blank) &&
		    (t->h_sync_offset == all_fmt_paras[i]->timing.h_front) &&
		    (t->h_sync == all_fmt_paras[i]->timing.h_sync) &&
		    (t->v_sync_offset == all_fmt_paras[i]->timing.v_front) &&
		    (t->v_sync == all_fmt_paras[i]->timing.v_sync)
		    )
			return all_fmt_paras[i];
	}

	return NULL;
}

struct hdmi_format_para *hdmi_get_vesa_paras(struct vesa_standard_timing *t)
{
	int i;

	if (!t)
		return NULL;
	for (i = 0; all_fmt_paras[i] != NULL; i++) {
		if ((t->hactive == all_fmt_paras[i]->timing.h_active) &&
			(t->vactive == all_fmt_paras[i]->timing.v_active)) {
			if (t->hsync &&
				(t->hsync == all_fmt_paras[i]->timing.vsync))
				return all_fmt_paras[i];
			if ((t->hblank && (t->hblank ==
				all_fmt_paras[i]->timing.h_blank))
				 && (t->vblank && (t->vblank ==
				all_fmt_paras[i]->timing.v_blank))
				 && (t->tmds_clk && (t->tmds_clk ==
				all_fmt_paras[i]->tmds_clk / 10)))
				return all_fmt_paras[i];
		}
	}
	return NULL;
}

static struct parse_cd parse_cd_[] = {
	{COLORDEPTH_24B, "8bit",},
	{COLORDEPTH_30B, "10bit"},
	{COLORDEPTH_36B, "12bit"},
	{COLORDEPTH_48B, "16bit"},
};

static struct parse_cs parse_cs_[] = {
	{COLORSPACE_RGB444, "rgb",},
	{COLORSPACE_YUV422, "422",},
	{COLORSPACE_YUV444, "444",},
	{COLORSPACE_YUV420, "420",},
};

static struct parse_cr parse_cr_[] = {
	{COLORRANGE_LIM, "limit",},
	{COLORRANGE_FUL, "full",},
};

const char *hdmi_get_str_cd(struct hdmi_format_para *para)
{
	int i;

	for (i = 0; i < sizeof(parse_cd_) / sizeof(struct parse_cd); i++) {
		if (para->cd == parse_cd_[i].cd)
			return parse_cd_[i].name;
	}
	return NULL;
}

const char *hdmi_get_str_cs(struct hdmi_format_para *para)
{
	int i;

	for (i = 0; i < sizeof(parse_cs_) / sizeof(struct parse_cs); i++) {
		if (para->cs == parse_cs_[i].cs)
			return parse_cs_[i].name;
	}
	return NULL;
}

const char *hdmi_get_str_cr(struct hdmi_format_para *para)
{
	int i;

	for (i = 0; i < sizeof(parse_cr_) / sizeof(struct parse_cr); i++) {
		if (para->cr == parse_cr_[i].cr)
			return parse_cr_[i].name;
	}
	return NULL;
}

/* parse the string from "dhmitx output FORMAT" */
static void hdmi_parse_attr(struct hdmi_format_para *para, char const *name)
{
	int i;

	/* parse color depth */
	for (i = 0; i < sizeof(parse_cd_) / sizeof(struct parse_cd); i++) {
		if (strstr(name, parse_cd_[i].name)) {
			para->cd = parse_cd_[i].cd;
			break;
		}
	}
	/* set default value */
	if (i == sizeof(parse_cd_) / sizeof(struct parse_cd))
		para->cd = COLORDEPTH_24B;

	/* parse color space */
	for (i = 0; i < sizeof(parse_cs_) / sizeof(struct parse_cs); i++) {
		if (strstr(name, parse_cs_[i].name)) {
			para->cs = parse_cs_[i].cs;
			break;
		}
	}
	/* set default value */
	if (i == sizeof(parse_cs_) / sizeof(struct parse_cs))
		para->cs = COLORSPACE_YUV444;

	/* parse color range */
	for (i = 0; i < sizeof(parse_cr_) / sizeof(struct parse_cr); i++) {
		if (strstr(name, parse_cr_[i].name)) {
			para->cr = parse_cr_[i].cr;
			break;
		}
	}
	/* set default value */
	if (i == sizeof(parse_cr_) / sizeof(struct parse_cr))
		para->cr = COLORRANGE_FUL;
}

/*
 * Parameter 'name' can be 1080p60hz, or 1920x1080p60hz
 * or 3840x2160p60hz, 2160p60hz
 * or 3840x2160p60hz420, 2160p60hz420 (Y420 mode)
 */
struct hdmi_format_para *hdmi_get_fmt_name(char const *name, char const *attr)
{
	int i;
	char *lname;
	enum hdmi_vic vic = HDMI_Unknown;
	struct hdmi_format_para *para = &fmt_para_non_hdmi_fmt;

	if (!name)
		return para;

	for (i = 0; all_fmt_paras[i]; i++) {
		lname = all_fmt_paras[i]->name;
		if (lname && (strncmp(name, lname, strlen(lname)) == 0)) {
			vic = all_fmt_paras[i]->vic;
			break;
		}
		lname = all_fmt_paras[i]->sname;
		if (lname && (strncmp(name, lname, strlen(lname)) == 0)) {
			vic = all_fmt_paras[i]->vic;
			break;
		}
	}
	if ((vic != HDMI_Unknown) && (i != sizeof(all_fmt_paras) /
		sizeof(struct hdmi_format_para *))) {
		para = all_fmt_paras[i];
		memset(&para->ext_name[0], 0, sizeof(para->ext_name));
		memcpy(&para->ext_name[0], name, sizeof(para->ext_name));
		hdmi_parse_attr(para, name);
		hdmi_parse_attr(para, attr);
	} else {
		para = &fmt_para_non_hdmi_fmt;
		hdmi_parse_attr(para, name);
		hdmi_parse_attr(para, attr);
	}
	if (strstr(name, "420"))
		para->cs = COLORSPACE_YUV420;

	/* only 2160p60/50hz smpte60/50hz have Y420 mode */
	if (para->cs == COLORSPACE_YUV420) {
		switch ((para->vic) & 0xff) {
		case HDMI_3840x2160p50_16x9:
		case HDMI_3840x2160p60_16x9:
		case HDMI_4096x2160p50_256x135:
		case HDMI_4096x2160p60_256x135:
		case HDMI_3840x2160p50_64x27:
		case HDMI_3840x2160p60_64x27:
			break;
		default:
			para = &fmt_para_non_hdmi_fmt;
			break;
		}
	}

	return para;
}

static struct hdmi_format_para tst_para;
static inline void copy_para(struct hdmi_format_para *des,
	struct hdmi_format_para *src)
{
	if (!des || !src)
		return;
	memcpy(des, src, sizeof(struct hdmi_format_para));
}

struct hdmi_format_para *hdmi_tst_fmt_name(char const *name, char const *attr)
{
	int i;
	char *lname;
	enum hdmi_vic vic = HDMI_Unknown;

	copy_para(&tst_para, &fmt_para_non_hdmi_fmt);
	if (!name)
		return &tst_para;

	for (i = 0; all_fmt_paras[i]; i++) {
		lname = all_fmt_paras[i]->name;
		if (lname && (strncmp(name, lname, strlen(lname)) == 0)) {
			vic = all_fmt_paras[i]->vic;
			break;
		}
		lname = all_fmt_paras[i]->sname;
		if (lname && (strncmp(name, lname, strlen(lname)) == 0)) {
			vic = all_fmt_paras[i]->vic;
			break;
		}
	}
	if ((vic != HDMI_Unknown) && (i != sizeof(all_fmt_paras) /
		sizeof(struct hdmi_format_para *))) {
		copy_para(&tst_para, all_fmt_paras[i]);
		memset(&tst_para.ext_name[0], 0, sizeof(tst_para.ext_name));
		memcpy(&tst_para.ext_name[0], name, sizeof(tst_para.ext_name));
		hdmi_parse_attr(&tst_para, name);
		hdmi_parse_attr(&tst_para, attr);
	} else {
		copy_para(&tst_para, &fmt_para_non_hdmi_fmt);
		hdmi_parse_attr(&tst_para, name);
		hdmi_parse_attr(&tst_para, attr);
	}
	if (strstr(name, "420"))
		tst_para.cs = COLORSPACE_YUV420;

	/* only 2160p60/50hz smpte60/50hz have Y420 mode */
	if (tst_para.cs == COLORSPACE_YUV420) {
		switch ((tst_para.vic) & 0xff) {
		case HDMI_3840x2160p50_16x9:
		case HDMI_3840x2160p60_16x9:
		case HDMI_4096x2160p50_256x135:
		case HDMI_4096x2160p60_256x135:
		case HDMI_3840x2160p50_64x27:
		case HDMI_3840x2160p60_64x27:
			break;
		default:
			copy_para(&tst_para, &fmt_para_non_hdmi_fmt);
			break;
		}
	}

	return &tst_para;
}

struct vinfo_s *hdmi_get_valid_vinfo(char *mode)
{
	int i;
	struct vinfo_s *info = NULL;
	char mode_[32];

	if (strlen(mode)) {
		/* the string of mode contains char NF */
		memset(mode_, 0, sizeof(mode_));
		strncpy(mode_, mode, sizeof(mode_));
		for (i = 0; i < sizeof(mode_); i++)
			if (mode_[i] == 10)
				mode_[i] = 0;

		for (i = 0; all_fmt_paras[i]; i++) {
			if (all_fmt_paras[i]->hdmitx_vinfo.mode == VMODE_MAX)
				continue;
			if (strncmp(all_fmt_paras[i]->hdmitx_vinfo.name, mode_,
				strlen(mode_)) == 0) {
				info = &all_fmt_paras[i]->hdmitx_vinfo;
				break;
			}
		}
	}
	return info;
}

/* For check all format parameters only */
void check_detail_fmt(void)
{
	int i;
	struct hdmi_format_para *p;
	struct hdmi_cea_timing *t;

	pr_warn("VIC Hactive Vactive I/P Htotal Hblank Vtotal Vblank Hfreq Vfreq Pfreq\n");
	for (i = 0; all_fmt_paras[i] != NULL; i++) {
		p = all_fmt_paras[i];
		t = &p->timing;
		pr_warn("%s[%d] %d %d %c %d %d %d %d %d %d %d\n",
			all_fmt_paras[i]->name, all_fmt_paras[i]->vic,
			t->h_active, t->v_active,
			(p->progress_mode) ? 'P' : 'I',
			t->h_total, t->h_blank, t->v_total, t->v_blank,
			t->h_freq, t->v_freq, t->pixel_freq);
	}

	pr_warn("\nVIC Hfront Hsync Hback Hpol Vfront Vsync Vback Vpol Ln\n");
	for (i = 0; all_fmt_paras[i] != NULL; i++) {
		p = all_fmt_paras[i];
		t = &p->timing;
	pr_warn("%s[%d] %d %d %d %c %d %d %d %c %d\n",
		all_fmt_paras[i]->name, all_fmt_paras[i]->vic,
		t->h_front, t->h_sync, t->h_back,
		(t->hsync_polarity) ? 'P' : 'N',
		t->v_front, t->v_sync, t->v_back,
		(t->vsync_polarity) ? 'P' : 'N',
		t->v_sync_ln);
	}

	pr_warn("\nCheck Horizon parameter\n");
	for (i = 0; all_fmt_paras[i] != NULL; i++) {
		p = all_fmt_paras[i];
		t = &p->timing;
	if (t->h_total != (t->h_active + t->h_blank))
		pr_warn("VIC[%d] Ht[%d] != (Ha[%d] + Hb[%d])\n",
		all_fmt_paras[i]->vic, t->h_total, t->h_active,
		t->h_blank);
	if (t->h_blank != (t->h_front + t->h_sync + t->h_back))
		pr_warn("VIC[%d] Hb[%d] != (Hf[%d] + Hs[%d] + Hb[%d])\n",
			all_fmt_paras[i]->vic, t->h_blank,
			t->h_front, t->h_sync, t->h_back);
	}

	pr_warn("\nCheck Vertical parameter\n");
	for (i = 0; all_fmt_paras[i] != NULL; i++) {
		p = all_fmt_paras[i];
		t = &p->timing;
		if (t->v_total != (t->v_active + t->v_blank))
			pr_warn("VIC[%d] Vt[%d] != (Va[%d] + Vb[%d]\n",
				all_fmt_paras[i]->vic, t->v_total, t->v_active,
				t->v_blank);
	if ((t->v_blank != (t->v_front + t->v_sync + t->v_back))
		& (p->progress_mode == 1))
		pr_warn("VIC[%d] Vb[%d] != (Vf[%d] + Vs[%d] + Vb[%d])\n",
			all_fmt_paras[i]->vic, t->v_blank,
			t->v_front, t->v_sync, t->v_back);
	if ((t->v_blank/2 != (t->v_front + t->v_sync + t->v_back))
		& (p->progress_mode == 0))
		pr_warn("VIC[%d] Vb[%d] != (Vf[%d] + Vs[%d] + Vb[%d])\n",
			all_fmt_paras[i]->vic, t->v_blank, t->v_front,
			t->v_sync, t->v_back);
	}
}

/* Recommended N and Expected CTS for 32kHz */
struct hdmi_audio_fs_ncts aud_32k_para = {
	.array[0] = {
		.tmds_clk = 25175,
		.n = 4576,
		.cts = 28125,
		.n_36bit = 9152,
		.cts_36bit = 84375,
		.n_48bit = 4576,
		.cts_48bit = 56250,
	},
	.array[1] = {
		.tmds_clk = 25200,
		.n = 4096,
		.cts = 25200,
		.n_36bit = 4096,
		.cts_36bit = 37800,
		.n_48bit = 4096,
		.cts_48bit = 50400,
	},
	.array[2] = {
		.tmds_clk = 27000,
		.n = 4096,
		.cts = 27000,
		.n_36bit = 4096,
		.cts_36bit = 40500,
		.n_48bit = 4096,
		.cts_48bit = 54000,
	},
	.array[3] = {
		.tmds_clk = 27027,
		.n = 4096,
		.cts = 27027,
		.n_36bit = 8192,
		.cts_36bit = 81081,
		.n_48bit = 4096,
		.cts_48bit = 54054,
	},
	.array[4] = {
		.tmds_clk = 54000,
		.n = 4096,
		.cts = 54000,
		.n_36bit = 4096,
		.cts_36bit = 81000,
		.n_48bit = 4096,
		.cts_48bit = 108000,
	},
	.array[5] = {
		.tmds_clk = 54054,
		.n = 4096,
		.cts = 54054,
		.n_36bit = 4096,
		.cts_36bit = 81081,
		.n_48bit = 4096,
		.cts_48bit = 108108,
	},
	.array[6] = {
		.tmds_clk = 74176,
		.n = 11648,
		.cts = 210937,
		.n_36bit = 11648,
		.cts_36bit = 316406,
		.n_48bit = 11648,
		.cts_48bit = 421875,
	},
	.array[7] = {
		.tmds_clk = 74250,
		.n = 4096,
		.cts = 74250,
		.n_36bit = 4096,
		.cts_36bit = 111375,
		.n_48bit = 4096,
		.cts_48bit = 148500,
	},
	.array[8] = {
		.tmds_clk = 148352,
		.n = 11648,
		.cts = 421875,
		.n_36bit = 11648,
		.cts_36bit = 632812,
		.n_48bit = 11648,
		.cts_48bit = 843750,
	},
	.array[9] = {
		.tmds_clk = 148500,
		.n = 4096,
		.cts = 148500,
		.n_36bit = 4096,
		.cts_36bit = 222750,
		.n_48bit = 4096,
		.cts_48bit = 297000,
	},
	.array[10] = {
		.tmds_clk = 296703,
		.n = 5824,
		.cts = 421875,
	},
	.array[11] = {
		.tmds_clk = 297000,
		.n = 3072,
		.cts = 222750,
	},
	.array[12] = {
		.tmds_clk = 593407,
		.n = 5824,
		.cts = 843750,
	},
	.array[13] = {
		.tmds_clk = 594000,
		.n = 3072,
		.cts = 445500,
	},
	.def_n = 4096,
};

/* Recommended N and Expected CTS for 44.1kHz and Multiples */
struct hdmi_audio_fs_ncts aud_44k1_para = {
	.array[0] = {
		.tmds_clk = 25175,
		.n = 7007,
		.cts = 31250,
		.n_36bit = 7007,
		.cts_36bit = 46875,
		.n_48bit = 7007,
		.cts_48bit = 62500,
	},
	.array[1] = {
		.tmds_clk = 25200,
		.n = 6272,
		.cts = 28000,
		.n_36bit = 6272,
		.cts_36bit = 42000,
		.n_48bit = 6272,
		.cts_48bit = 56000,
	},
	.array[2] = {
		.tmds_clk = 27000,
		.n = 6272,
		.cts = 30000,
		.n_36bit = 6272,
		.cts_36bit = 45000,
		.n_48bit = 6272,
		.cts_48bit = 60000,
	},
	.array[3] = {
		.tmds_clk = 27027,
		.n = 6272,
		.cts = 30030,
		.n_36bit = 6272,
		.cts_36bit = 45045,
		.n_48bit = 6272,
		.cts_48bit = 60060,
	},
	.array[4] = {
		.tmds_clk = 54000,
		.n = 6272,
		.cts = 60000,
		.n_36bit = 6272,
		.cts_36bit = 90000,
		.n_48bit = 6272,
		.cts_48bit = 120000,
	},
	.array[5] = {
		.tmds_clk = 54054,
		.n = 6272,
		.cts = 60060,
		.n_36bit = 6272,
		.cts_36bit = 90090,
		.n_48bit = 6272,
		.cts_48bit = 120120,
	},
	.array[6] = {
		.tmds_clk = 74176,
		.n = 17836,
		.cts = 234375,
		.n_36bit = 17836,
		.cts_36bit = 351562,
		.n_48bit = 17836,
		.cts_48bit = 468750,
	},
	.array[7] = {
		.tmds_clk = 74250,
		.n = 6272,
		.cts = 82500,
		.n_36bit = 6272,
		.cts_36bit = 123750,
		.n_48bit = 6272,
		.cts_48bit = 165000,
	},
	.array[8] = {
		.tmds_clk = 148352,
		.n = 8918,
		.cts = 234375,
		.n_36bit = 17836,
		.cts_36bit = 703125,
		.n_48bit = 8918,
		.cts_48bit = 468750,
	},
	.array[9] = {
		.tmds_clk = 148500,
		.n = 6272,
		.cts = 165000,
		.n_36bit = 6272,
		.cts_36bit = 247500,
		.n_48bit = 6272,
		.cts_48bit = 330000,
	},
	.array[10] = {
		.tmds_clk = 296703,
		.n = 4459,
		.cts = 234375,
	},
	.array[11] = {
		.tmds_clk = 297000,
		.n = 4707,
		.cts = 247500,
	},
	.array[12] = {
		.tmds_clk = 593407,
		.n = 8918,
		.cts = 937500,
	},
	.array[13] = {
		.tmds_clk = 594000,
		.n = 9408,
		.cts = 990000,
	},
	.def_n = 6272,
};

/* Recommended N and Expected CTS for 48kHz and Multiples */
struct hdmi_audio_fs_ncts aud_48k_para = {
	.array[0] = {
		.tmds_clk = 25175,
		.n = 6864,
		.cts = 28125,
		.n_36bit = 9152,
		.cts_36bit = 58250,
		.n_48bit = 6864,
		.cts_48bit = 56250,
	},
	.array[1] = {
		.tmds_clk = 25200,
		.n = 6144,
		.cts = 25200,
		.n_36bit = 6144,
		.cts_36bit = 37800,
		.n_48bit = 6144,
		.cts_48bit = 50400,
	},
	.array[2] = {
		.tmds_clk = 27000,
		.n = 6144,
		.cts = 27000,
		.n_36bit = 6144,
		.cts_36bit = 40500,
		.n_48bit = 6144,
		.cts_48bit = 54000,
	},
	.array[3] = {
		.tmds_clk = 27027,
		.n = 6144,
		.cts = 27027,
		.n_36bit = 8192,
		.cts_36bit = 54054,
		.n_48bit = 6144,
		.cts_48bit = 54054,
	},
	.array[4] = {
		.tmds_clk = 54000,
		.n = 6144,
		.cts = 54000,
		.n_36bit = 6144,
		.cts_36bit = 81000,
		.n_48bit = 6144,
		.cts_48bit = 108000,
	},
	.array[5] = {
		.tmds_clk = 54054,
		.n = 6144,
		.cts = 54054,
		.n_36bit = 6144,
		.cts_36bit = 81081,
		.n_48bit = 6144,
		.cts_48bit = 108108,
	},
	.array[6] = {
		.tmds_clk = 74176,
		.n = 11648,
		.cts = 140625,
		.n_36bit = 11648,
		.cts_36bit = 210937,
		.n_48bit = 11648,
		.cts_48bit = 281250,
	},
	.array[7] = {
		.tmds_clk = 74250,
		.n = 6144,
		.cts = 74250,
		.n_36bit = 6144,
		.cts_36bit = 111375,
		.n_48bit = 6144,
		.cts_48bit = 148500,
	},
	.array[8] = {
		.tmds_clk = 148352,
		.n = 5824,
		.cts = 140625,
		.n_36bit = 11648,
		.cts_36bit = 421875,
		.n_48bit = 5824,
		.cts_48bit = 281250,
	},
	.array[9] = {
		.tmds_clk = 148500,
		.n = 6144,
		.cts = 148500,
		.n_36bit = 6144,
		.cts_36bit = 222750,
		.n_48bit = 6144,
		.cts_48bit = 297000,
	},
	.array[10] = {
		.tmds_clk = 296703,
		.n = 5824,
		.cts = 281250,
	},
	.array[11] = {
		.tmds_clk = 297000,
		.n = 5120,
		.cts = 247500,
	},
	.array[12] = {
		.tmds_clk = 593407,
		.n = 5824,
		.cts = 562500,
	},
	.array[13] = {
		.tmds_clk = 594000,
		.n = 6144,
		.cts = 594000,
	},
	.def_n = 6144,
};

static struct hdmi_audio_fs_ncts *all_aud_paras[] = {
	&aud_32k_para,
	&aud_44k1_para,
	&aud_48k_para,
	NULL,
};

unsigned int hdmi_get_aud_n_paras(enum hdmi_audio_fs fs,
	enum hdmi_color_depth cd, unsigned int tmds_clk)
{
	struct hdmi_audio_fs_ncts *p = NULL;
	unsigned int i, n;
	unsigned int N_multiples = 1;

	pr_info("hdmitx: fs = %d, cd = %d, tmds_clk = %d\n", fs, cd, tmds_clk);
	switch (fs) {
	case FS_32K:
		p = all_aud_paras[0];
		N_multiples = 1;
		break;
	case FS_44K1:
		p = all_aud_paras[1];
		N_multiples = 1;
		break;
	case FS_88K2:
		p = all_aud_paras[1];
		N_multiples = 2;
		break;
	case FS_176K4:
		p = all_aud_paras[1];
		N_multiples = 4;
		break;
	case FS_48K:
		p = all_aud_paras[2];
		N_multiples = 1;
		break;
	case FS_96K:
		p = all_aud_paras[2];
		N_multiples = 2;
		break;
	case FS_192K:
		p = all_aud_paras[2];
		N_multiples = 4;
		break;
	default: /* Default as FS_48K */
		p = all_aud_paras[2];
		N_multiples = 1;
		break;
	}
	for (i = 0; i < AUDIO_PARA_MAX_NUM; i++) {
		if (tmds_clk == p->array[i].tmds_clk)
			break;
	}

	if (i < AUDIO_PARA_MAX_NUM)
		if ((cd == COLORDEPTH_24B) || (cd == COLORDEPTH_30B))
			n = p->array[i].n ? p->array[i].n : p->def_n;
		else if (cd == COLORDEPTH_36B)
			n = p->array[i].n_36bit ?
				p->array[i].n_36bit : p->def_n;
		else if (cd == COLORDEPTH_48B)
			n = p->array[i].n_48bit ?
				p->array[i].n_48bit : p->def_n;
		else
			n = p->array[i].n ? p->array[i].n : p->def_n;
	else
		n = p->def_n;
	return n * N_multiples;
}
/*--------------------------------------------------------------*/
/* for csc coef */

static unsigned char coef_yc444_rgb_24bit_601[] = {
	0x20, 0x00, 0x69, 0x26, 0x74, 0xfd, 0x01, 0x0e,
	0x20, 0x00, 0x2c, 0xdd, 0x00, 0x00, 0x7e, 0x9a,
	0x20, 0x00, 0x00, 0x00, 0x38, 0xb4, 0x7e, 0x3b
};

static unsigned char coef_yc444_rgb_24bit_709[] = {
	0x20, 0x00, 0x71, 0x06, 0x7a, 0x02, 0x00, 0xa7,
	0x20, 0x00, 0x32, 0x64, 0x00, 0x00, 0x7e, 0x6d,
	0x20, 0x00, 0x00, 0x00, 0x3b, 0x61, 0x7e, 0x25
};


static struct hdmi_csc_coef_table hdmi_csc_coef[] = {
	{COLORSPACE_YUV444, COLORSPACE_RGB444, COLORDEPTH_24B, 0,
		sizeof(coef_yc444_rgb_24bit_601), coef_yc444_rgb_24bit_601},
	{COLORSPACE_YUV444, COLORSPACE_RGB444, COLORDEPTH_24B, 1,
		sizeof(coef_yc444_rgb_24bit_709), coef_yc444_rgb_24bit_709},
};

unsigned int hdmi_get_csc_coef(
	unsigned int input_format, unsigned int output_format,
	unsigned int color_depth, unsigned int color_format,
	unsigned char **coef_array, unsigned int *coef_length)
{
	unsigned int i = 0, max = 0;

	max = sizeof(hdmi_csc_coef)/sizeof(struct hdmi_csc_coef_table);

	for (i = 0; i < max; i++) {
		if ((input_format == hdmi_csc_coef[i].input_format) &&
			(output_format == hdmi_csc_coef[i].output_format) &&
			(color_depth == hdmi_csc_coef[i].color_depth) &&
			(color_format == hdmi_csc_coef[i].color_format)) {
			*coef_array = hdmi_csc_coef[i].coef;
			*coef_length = hdmi_csc_coef[i].coef_length;
			return 0;
		}
	}

	coef_array = NULL;
	*coef_length = 0;

	return 1;
}

bool is_hdmi14_4k(enum hdmi_vic vic)
{
	bool ret = 0;

	switch (vic) {
	case HDMI_3840x2160p24_16x9:
	case HDMI_3840x2160p25_16x9:
	case HDMI_3840x2160p30_16x9:
	case HDMI_4096x2160p24_256x135:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

