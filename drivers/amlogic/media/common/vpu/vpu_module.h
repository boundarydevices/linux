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
	"viu_super_scaler",
	"viu_osd_super_scaler",
	"afbc_dec1",
	"viu_di_scaler",
	"di_pre",
	"di_post",
	"viu_sharpness_line_buffer",

	"viu2",
	"viu2_osd1",
	"viu2_osd2",
	"viu2_vd1",
	"viu2_chroma",
	"viu2_ofifo",
	"viu2_scaler",
	"viu2_osd_scaler",
	"vkstone",
	"dolby_core3",
	"dolby0",
	"dolby_1a",
	"dolby_1b",
	"vpu_arb",
	"afbc_dec0",
	"osd_afbcd",
	"vd2_scaler",
	"vencp",
	"vencl",
	"venci",
	"ls_stts",
	"ldim_stts",
	"tv_decoder_cvd2",
	"xvycc_lut",
	"vd2_osd2_scaler",

	"viu_water_mark",
	"tcon",
	"viu_osd3",
	"viu_osd4",
	"mail_afbcd",
	"vd1_scaler",
	"osd_bld34",
	"prime_dolby_ram",
	"vd2_ofifo",
	"ds",
	"lut3d",
	"viu2_osd_rotation",
	"rdma",

	"axi_wr1",
	"axi_wr0",
	"afbce",

	"vpu_mod_max",

	/* for clk_gate */
	"vpu_top",
	"vpu_clkb",
	"clk_vib",
	"clk_b_reg_latch",
	"vpu_misc",
	"venc_dac",
	"vlock",
	"di",
	"vpp",

	"vpu_max",
	"none",
	"none",
};

#endif
