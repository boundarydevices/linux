/*
 * include/linux/amlogic/media/vout/hdmi_tx/enc_clk_config.h
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

#ifndef __ENC_CLK_CONFIG_H__
#define __ENC_CLK_CONFIG_H__

enum Viu_Type {
	VIU_ENCL = 0, VIU_ENCI, VIU_ENCP, VIU_ENCT,
};

extern int set_viu_path(unsigned int viu_channel_sel,
	enum Viu_Type viu_type_sel);
extern void set_enci_clk(unsigned int clk);
extern void set_encp_clk(unsigned int clk);
/* extern void set_vmode_clk(vmode_t mode); */

struct enc_clk_val {
	enum vmode_e mode;
	unsigned int hpll_clk_out;
	unsigned int hpll_hdmi_od;
	unsigned int hpll_lvds_od;
	unsigned int viu_path;
	enum Viu_Type viu_type;
	unsigned int vid_pll_div;
	unsigned int clk_final_div;
	unsigned int hdmi_tx_pixel_div;
	unsigned int encp_div;
	unsigned int enci_div;
	unsigned int enct_div;
	unsigned int encl_div;
	unsigned int vdac0_div;
	unsigned int vdac1_div;
	unsigned int unused; /* prevent compile error */
};

#endif
