/****************************************************************************
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 VeriSilicon Holdings Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************
 *
 * The GPL License (GPL)
 *
 * Copyright (c) 2020-2021 VeriSilicon Holdings Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;
 *
 *****************************************************************************
 *
 * Note: This software is released under dual MIT and GPL licenses. A
 * recipient may use this file under the terms of either the MIT license or
 * GPL License. If you wish to use only one license not the other, you can
 * indicate your decision by deleting one of the above license notices in your
 * version of this file.
 *
 *****************************************************************************/

#ifndef _VVSENSOR_PUBLIC_HEADER_H_
#define _VVSENSOR_PUBLIC_HEADER_H_

#ifndef __KERNEL__
#include <stdint.h>
#else
#include <linux/uaccess.h>
#endif

#define VVCAM_SUPPORT_MAX_MODE_COUNT                6
#define VVCAM_CAP_BUS_INFO_I2C_ADAPTER_NR_POS       8

#define SENSOR_FIX_FRACBITS 10

enum {
	VVSENSORIOC_RESET = 0x100,
	VVSENSORIOC_S_POWER,
	VVSENSORIOC_G_POWER,
	VVSENSORIOC_S_CLK,
	VVSENSORIOC_G_CLK,
	VVSENSORIOC_QUERY,
	VVSENSORIOC_S_SENSOR_MODE,
	VVSENSORIOC_G_SENSOR_MODE,
	VVSENSORIOC_READ_REG,
	VVSENSORIOC_WRITE_REG,
	VVSENSORIOC_READ_ARRAY,
	VVSENSORIOC_WRITE_ARRAY,
	VVSENSORIOC_G_NAME,
	VVSENSORIOC_G_RESERVE_ID,
	VVSENSORIOC_G_CHIP_ID,
	VVSENSORIOC_S_INIT,
	VVSENSORIOC_S_STREAM,
	VVSENSORIOC_S_LONG_EXP,
	VVSENSORIOC_S_EXP,
	VVSENSORIOC_S_VSEXP,
	VVSENSORIOC_S_LONG_GAIN,
	VVSENSORIOC_S_GAIN,
	VVSENSORIOC_S_VSGAIN,
	VVSENSORIOC_S_FPS,
	VVSENSORIOC_G_FPS,
	VVSENSORIOC_S_HDR_RADIO,
	VVSENSORIOC_S_WB,
	VVSENSORIOC_S_BLC,
	VVSENSORIOC_G_EXPAND_CURVE,
	VVSENSORIOC_S_TEST_PATTERN,
	VVSENSORIOC_MAX,
};

struct vvcam_clk_s {
	uint32_t      status;
	unsigned long sensor_mclk;
	unsigned long csi_max_pixel_clk;
};
/* W/R registers */
struct vvcam_sccb_data_s {
	uint32_t addr;
	uint32_t data;
};

/* vsi native usage */
struct vvcam_sccb_cfg_s {
	uint8_t slave_addr;
	uint8_t addr_byte;
	uint8_t data_byte;
};

struct vvcam_sccb_array_s {
	uint32_t count;
	struct vvcam_sccb_data_s *sccb_data;
};

typedef struct sensor_hdr_artio_s {
	uint32_t ratio_l_s;
	uint32_t ratio_s_vs;
	uint32_t accuracy;
} sensor_hdr_artio_t;

typedef struct vvcam_ae_info_s {
	uint32_t def_frm_len_lines;
	uint32_t curr_frm_len_lines;
	uint32_t one_line_exp_time_ns;
	/*
	 * normal use  max_integration_line
	 * 2Expsoure use max_integration_line and max_vsintegration_line
	 *
	 */
	uint32_t max_longintegration_line;
	uint32_t min_longintegration_line;

	uint32_t max_integration_line;
	uint32_t min_integration_line;

	uint32_t max_vsintegration_line;
	uint32_t min_vsintegration_line;

	uint32_t max_long_again;
	uint32_t min_long_again;
	uint32_t max_long_dgain;
	uint32_t min_long_dgain;

	uint32_t max_again;
	uint32_t min_again;
	uint32_t max_dgain;
	uint32_t min_dgain;

	uint32_t max_short_again;
	uint32_t min_short_again;
	uint32_t max_short_dgain;
	uint32_t min_short_dgain;

	uint32_t start_exposure;

	uint32_t gain_step;
	uint32_t cur_fps;
	uint32_t max_fps;
	uint32_t min_fps;
	uint32_t min_afps;
	uint8_t  int_update_delay_frm;
	uint8_t  gain_update_delay_frm;
	struct sensor_hdr_artio_s hdr_ratio;
} vvcam_ae_info_t;

struct sensor_mipi_info_s {
	uint32_t mipi_lane;
};

enum sensor_hdr_mode_e {
	SENSOR_MODE_LINEAR,
	SENSOR_MODE_HDR_STITCH,
	SENSOR_MODE_HDR_NATIVE,
};

enum sensor_bayer_pattern_e {
	BAYER_RGGB = 0,
	BAYER_GRBG = 1,
	BAYER_GBRG = 2,
	BAYER_BGGR = 3,
	BAYER_BUTT
};

enum sensor_stitching_mode_e {
	SENSOR_STITCHING_DUAL_DCG           = 0,    /**< dual DCG mode 3x12-bit */
	SENSOR_STITCHING_3DOL               = 1,    /**< dol3 frame 3x12-bit */
	SENSOR_STITCHING_LINEBYLINE         = 2,    /**< 3x12-bit line by line without waiting */
	SENSOR_STITCHING_16BIT_COMPRESS     = 3,    /**< 16-bit compressed data + 12-bit RAW */
	SENSOR_STITCHING_DUAL_DCG_NOWAIT    = 4,    /**< 2x12-bit dual DCG without waiting */
	SENSOR_STITCHING_2DOL               = 5,    /**< dol2 frame or 1 CG+VS sx12-bit RAW */
	SENSOR_STITCHING_L_AND_S            = 6,    /**< L+S 2x12-bit RAW */
	SENSOR_STITCHING_MAX

};

typedef struct sensor_test_pattern_s {
	uint8_t enable;
	uint32_t pattern;
}sensor_test_pattern_t;

typedef struct sensor_expand_curve_s {
	uint32_t x_bit;
	uint32_t y_bit;
	uint8_t expand_px[64];
	uint32_t expand_x_data[65];
	uint32_t expand_y_data[65];
} sensor_expand_curve_t;

typedef struct sensor_data_compress_s {
	uint32_t enable;
	uint32_t x_bit;
	uint32_t y_bit;
} sensor_data_compress_t;

typedef struct vvcam_size_s {
	uint32_t bounds_width;
	uint32_t bounds_height;
	uint32_t top;
	uint32_t left;
	uint32_t width;
	uint32_t height;
} vvcam_size_t;

typedef struct vvcam_mode_info_s {
	uint32_t index;
	vvcam_size_t size;
	uint32_t hdr_mode;
	uint32_t stitching_mode;
	uint32_t bit_width;
	sensor_data_compress_t data_compress;
	uint32_t bayer_pattern;
	vvcam_ae_info_t ae_info;
	struct sensor_mipi_info_s mipi_info;
	void *preg_data;
	uint32_t reg_data_count;
} vvcam_mode_info_t;

typedef struct sensor_blc_s {
	uint32_t red;
	uint32_t gr;
	uint32_t gb;
	uint32_t blue;
} sensor_blc_t;

typedef struct sensor_white_balance_s {
	uint32_t r_gain;
	uint32_t gr_gain;
	uint32_t gb_gain;
	uint32_t b_gain;
} sensor_white_balance_t;

typedef struct vvcam_mode_info_array_s {
	uint32_t count;
	struct vvcam_mode_info_s modes[VVCAM_SUPPORT_MAX_MODE_COUNT];
} vvcam_mode_info_array_t;

#endif
