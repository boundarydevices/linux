/*
 * drivers/amlogic/media/enhancement/amvecm/amvecm_drm.c
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

#include <drm/drmP.h>
#include <uapi/drm/drm_mode.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include "arch/ve_regs.h"
#include "arch/vpp_regs.h"
#include "amve.h"

void amvecm_drm_init(u32 index)
{
	amvecm_gamma_init(1);
}
EXPORT_SYMBOL(amvecm_drm_init);

/*gamam size*/
int amvecm_drm_get_gamma_size(u32 index)
{
	return GAMMA_SIZE;
}
EXPORT_SYMBOL(amvecm_drm_get_gamma_size);

/*get gamma table*/
int amvecm_drm_gamma_get(u32 index, u16 *red, u16 *green, u16 *blue)
{
	int i = 0;

	for (i = 0; i < GAMMA_SIZE; i++) {
		red[i] = video_gamma_table_r.data[i] << 6;
		green[i] = video_gamma_table_g.data[i] << 6;
		blue[i] = video_gamma_table_b.data[i] << 6;
	}
	return 0;
}
EXPORT_SYMBOL(amvecm_drm_gamma_get);

/*set gamma table*/
int amvecm_drm_gamma_set(u32 index, struct drm_color_lut *lut, int lut_size)
{

	int i = 0;

	if (lut_size != GAMMA_SIZE) {
		pr_info("AMGAMMA_DRM: %s: lutsize is unsuitable\n", __func__);
		return -1;
	}

	for (i = 0; i < GAMMA_SIZE; i++) {
		video_gamma_table_r.data[i] = ((lut[i].red >> 6) & 0x3ff);
		video_gamma_table_g.data[i] = ((lut[i].green >> 6) & 0x3ff);
		video_gamma_table_b.data[i] = ((lut[i].blue >> 6) & 0x3ff);
	}

	vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
	vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
	vecm_latch_flag |= FLAG_GAMMA_TABLE_B;

	return 0;
}
EXPORT_SYMBOL(amvecm_drm_gamma_set);

/*gamma enable*/
int amvecm_drm_gamma_enable(u32 index)
{
	vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
	return 0;
}
EXPORT_SYMBOL(amvecm_drm_gamma_enable);

/*gamma disable*/
int amvecm_drm_gamma_disable(u32 index)
{
	vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
	return 0;
}
EXPORT_SYMBOL(amvecm_drm_gamma_disable);

int am_meson_ctm_set(u32 index, struct drm_color_ctm *ctm)
{
	int64_t m[9];
	int i = 0;

	for (i = 0; i < 9; i++) {
		m[i] = ctm->matrix[i];
		// DRM uses signed 32.32 fixed point while Meson expects signed
		// 3.10 fixed point. The following operations are performed to
		// transform the numbers:
		// - shift the sign bit from bit 63 to bit 12,
		// - shift the 2 integer bits from from starting bit 21 to 10,
		// - shift the fractional part and take the 10 significant bits.
		m[i] = ((m[i] >> 51) & 0x1000) | ((m[i] >> 22) & 0xfff);
	}
	VSYNC_WR_MPEG_REG_BITS(VPP_POST_MATRIX_EN_CTRL, 1, 0, 1);

	VSYNC_WR_MPEG_REG(VPP_POST_MATRIX_COEF00_01, ((m[0] & 0xfff) << 16)
		| (m[1] & 0xfff));
	VSYNC_WR_MPEG_REG(VPP_POST_MATRIX_COEF02_10, ((m[2] & 0xfff) << 16)
		| (m[3] & 0xfff));
	VSYNC_WR_MPEG_REG(VPP_POST_MATRIX_COEF11_12, ((m[4] & 0xfff) << 16)
		| (m[5] & 0xfff));
	VSYNC_WR_MPEG_REG(VPP_POST_MATRIX_COEF20_21, ((m[6] & 0xfff) << 16)
		| (m[7] & 0xfff));
	VSYNC_WR_MPEG_REG(VPP_POST_MATRIX_COEF22, (m[8] & 0xfff));
	VSYNC_WR_MPEG_REG(VPP_POST_MATRIX_OFFSET0_1, 0x0);
	VSYNC_WR_MPEG_REG(VPP_POST_MATRIX_OFFSET2, 0x0);
	VSYNC_WR_MPEG_REG(VPP_POST_MATRIX_PRE_OFFSET0_1, 0x0);
	VSYNC_WR_MPEG_REG(VPP_POST_MATRIX_PRE_OFFSET2, 0x0);
	return 0;
}
EXPORT_SYMBOL(am_meson_ctm_set);

int am_meson_ctm_disable(void)
{
	VSYNC_WR_MPEG_REG_BITS(VPP_POST_MATRIX_EN_CTRL, 0, 0, 1);
	return 0;
}
EXPORT_SYMBOL(am_meson_ctm_disable);
