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

/* end of Y420 modes*/

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

