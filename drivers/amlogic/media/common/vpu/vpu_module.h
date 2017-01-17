/*
 * drivers/amlogic/media/common/vpu/vpu_module.h
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

#ifndef __VPU_MODULE_H__
#define __VPU_MODULE_H__

/* ************************************************ */
/* VPU module name table */
/* ************************************************ */
static char *vpu_mod_table[] = {
	"viu_osd1",
	"viu_osd2",
	"viu_vd1",
	"viu_vd2",
	"viu_chroma",
	"viu_ofifo",
	"viu_scaler",
	"viu_osd_scaler",
	"viu_vdin0",
	"viu_vdin1",
	"pic_rot1",
	"pic_rot2",
	"pic_rot3",
	"viu_super_scaler",
	"viu_osd_super_scaler",
	"afbc_dec",
	"di_pre",
	"di_post",
	"viu_sharpness_line_buffer",

	"viu2_osd1",
	"viu2_osd2",
	"d2d3",
	"viu2_vd1",
	"viu2_chroma",
	"viu2_ofifo",
	"viu2_scaler",
	"viu2_osd_scaler",
	"vdin_arbitor_am_async",
	"vpu_arb",
	"display_arbitor_am_async",
	"osd1_afbcd",
	"afbc_dec0",
	"afbc_dec",
	"vpu_arbitor2_am_async",
	"vencp",
	"vencl",
	"venci",
	"isp",
	"cvd2",
	"atv_dmd",
	"ldim_stts",
	"xvycc_lut",

	"viu1_water_mark",

	/* for clk_gate */
	"vpu_top",
	"vpu_clkb",
	"vpu_rdma",
	"vpu_misc",
	"venc_dac",
	"vlock",
	"di",
	"vpp",

	"none",
};

#endif
