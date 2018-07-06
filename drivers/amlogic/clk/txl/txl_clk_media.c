/*
 * drivers/amlogic/clk/txl/txl_clk_media.c
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <dt-bindings/clock/amlogic,txl-clkc.h>

#include "../clkc.h"
#include "txl.h"

/* cts_vdin_meas_clk */
PNAME(meas_parent_names) = { "xtal", "fclk_div4",
"fclk_div3", "fclk_div5", "vid_pll", "vid2_pll", "null", "null" };
static MUX(vdin_meas_mux, HHI_VDIN_MEAS_CLK_CNTL, 0x7, 9, meas_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(vdin_meas_div, HHI_VDIN_MEAS_CLK_CNTL, 0, 7, "vdin_meas_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(vdin_meas_gate, HHI_VDIN_MEAS_CLK_CNTL, 20, "vdin_meas_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* cts_vpu_p0_clk */
PNAME(vpu_parent_names) = { "fclk_div4", "fclk_div3", "fclk_div5",
"fclk_div7", "null", "null", "null",  "gp1_pll"};
static MUX(vpu_p0_mux, HHI_VPU_CLK_CNTL, 0x7, 9, vpu_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(vpu_p0_div, HHI_VPU_CLK_CNTL, 0, 7, "vpu_p0_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(vpu_p0_gate, HHI_VPU_CLK_CNTL, 8, "vpu_p0_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* cts_vpu_p1_clk */
static MUX(vpu_p1_mux, HHI_VPU_CLK_CNTL, 0x7, 25, vpu_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(vpu_p1_div, HHI_VPU_CLK_CNTL, 16, 7, "vpu_p1_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(vpu_p1_gate, HHI_VPU_CLK_CNTL, 24, "vpu_p1_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* vpu_clkb_tmp */
PNAME(vpu_clkb_parent_names) = { "vpu_mux",
				"fclk_div5", "vid_pll", "fclk_div4" };
static MUX(vpu_clkb_tmp_mux, HHI_VPU_CLKB_CNTL, 0x3, 20,
vpu_clkb_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(vpu_clkb_tmp_div, HHI_VPU_CLKB_CNTL, 16, 4, "vpu_clkb_tmp_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(vpu_clkb_tmp_gate, HHI_VPU_CLKB_CNTL, 24, "vpu_clkb_tmp_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* vpu_clkb */
PNAME(vpu_clkb_nomux_parent_names) = { "vpu_clkb_tmp_composite" };
static DIV(vpu_clkb_div, HHI_VPU_CLKB_CNTL, 0, 8, "vpu_clkb_tmp_composite",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(vpu_clkb_gate, HHI_VPU_CLKB_CNTL, 8, "vpu_clkb_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* vpu mux clk */
PNAME(vpu_mux_parent_names) = { "vpu_p0_composite", "vpu_p1_composite" };
static MESON_MUX(vpu_mux, HHI_VPU_CLK_CNTL, 0x1, 31,
		vpu_mux_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* cts_vapb p0 clk */
static MUX(vapb_p0_mux, HHI_VAPBCLK_CNTL, 0x7, 9, vpu_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(vapb_p0_div, HHI_VAPBCLK_CNTL, 0, 7, "vapb_p0_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(vapb_p0_gate, HHI_VAPBCLK_CNTL, 8, "vapb_p0_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* cts_vapb p1 clk */
static MUX(vapb_p1_mux, HHI_VAPBCLK_CNTL, 0x7, 25, vpu_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(vapb_p1_div, HHI_VAPBCLK_CNTL, 16, 7, "vapb_p1_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(vapb_p1_gate, HHI_VAPBCLK_CNTL, 24, "vapb_p1_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* cts_vapb mux clk */
PNAME(vapb_mux_parent_names) = { "vapb_p0_composite", "vapb_p1_composite" };
static MESON_MUX(vapb_mux, HHI_VAPBCLK_CNTL, 0x1, 31,
vapb_mux_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(ge2d_gate, HHI_VAPBCLK_CNTL, 30, "vapb_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/*cts vid lock clock*/
PNAME(cts_vid_lock_parent_names) = { "xtal", "fclk_div4",
"fclk_div3", "fclk_div5" };
static MUX(cts_vid_lock_mux, HHI_VID_LOCK_CLK_CNTL, 0x3, 8,
cts_vid_lock_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(cts_vid_lock_div, HHI_VID_LOCK_CLK_CNTL, 0, 7, "cts_vid_lock_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(cts_vid_lock_gate, HHI_VID_LOCK_CLK_CNTL, 7, "cts_vid_lock_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/*hdmirx cfg clock*/
PNAME(hdmirx_parent_names) = { "xtal", "fclk_div4", "fclk_div3", "fclk_div5" };
static MUX(hdmirx_cfg_mux, HHI_HDMIRX_CLK_CNTL, 0x3, 9, hdmirx_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(hdmirx_cfg_div, HHI_HDMIRX_CLK_CNTL, 0, 7, "hdmirx_cfg_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(hdmirx_cfg_gate, HHI_HDMIRX_CLK_CNTL, 8, "hdmirx_cfg_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/*hdmirx modet clock*/
static MUX(hdmirx_modet_mux, HHI_HDMIRX_CLK_CNTL,
0x3, 25, hdmirx_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(hdmirx_modet_div, HHI_HDMIRX_CLK_CNTL, 16, 7, "hdmirx_modet_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(hdmirx_modet_gate, HHI_HDMIRX_CLK_CNTL, 24, "hdmirx_modet_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/*hdmirx audmeas clock*/
PNAME(hdmirx_ref_parent_names) = { "fclk_div4",
"fclk_div3", "fclk_div5", "fclk_div7" };
static MUX(hdmirx_audmeas_mux, HHI_HDMIRX_AUD_CLK_CNTL, 0x3, 9,
hdmirx_ref_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(hdmirx_audmeas_div, HHI_HDMIRX_AUD_CLK_CNTL, 0, 7,
"hdmirx_audmeas_mux", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(hdmirx_audmeas_gate, HHI_HDMIRX_AUD_CLK_CNTL, 8,
"hdmirx_audmeas_div", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* hdmirx acr clock*/
static MUX(hdmirx_acr_mux, HHI_HDMIRX_AUD_CLK_CNTL, 0x3, 25,
hdmirx_ref_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(hdmirx_acr_div, HHI_HDMIRX_AUD_CLK_CNTL, 16, 7, "hdmirx_acr_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(hdmirx_acr_gate, HHI_HDMIRX_AUD_CLK_CNTL, 24, "hdmirx_acr_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* hdcp22 skpclk*/
PNAME(hdcp22_skp_parent_names) = { "xtal",
"fclk_div4", "fclk_div3", "fclk_div5" };
static MUX(hdcp22_skp_mux, HHI_HDCP22_CLK_CNTL,
0x3, 25, hdcp22_skp_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(hdcp22_skp_div, HHI_HDCP22_CLK_CNTL, 16, 7, "hdcp22_skp_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(hdcp22_skp_gate, HHI_HDCP22_CLK_CNTL, 24, "hdcp22_skp_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* hdcp22 esm clock */
PNAME(hdcp22_esm_parent_names) = { "fclk_div7",
"fclk_div4", "fclk_div3", "fclk_div5"  };
static MUX(hdcp22_esm_mux, HHI_HDCP22_CLK_CNTL, 0x3, 9,
hdcp22_esm_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(hdcp22_esm_div, HHI_HDCP22_CLK_CNTL, 0, 7, "hdcp22_esm_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(hdcp22_esm_gate, HHI_HDCP22_CLK_CNTL, 8, "hdcp22_esm_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* cts demod core clock */
PNAME(cts_demod_parent_names) = { "xtal",
"fclk_div4", "fclk_div3", "adc_dpll_int"};
static MUX(cts_demod_mux, HHI_DEMOD_CLK_CNTL, 0x3, 9,
cts_demod_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(cts_demod_div, HHI_DEMOD_CLK_CNTL, 0, 7, "cts_demod_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(cts_demod_gate, HHI_DEMOD_CLK_CNTL, 8, "cts_demod_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* adc extclk in clock */
PNAME(adc_extclk_in_parent_names) = { "xtal", "fclk_div4", "fclk_div3",
"fclk_div5", "fclk_div7", "mpll2", "gp0_pll", "gp1_pll" };
static MUX(adc_extclk_in_mux, HHI_DEMOD_CLK_CNTL, 0x7, 25,
adc_extclk_in_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(adc_extclk_in_div, HHI_DEMOD_CLK_CNTL, 16, 7, "adc_extclk_in_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(adc_extclk_in_gate, HHI_DEMOD_CLK_CNTL, 24, "adc_extclk_in_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* cts atv dmd clock */
PNAME(cts_atv_dmd_parent_names) = { "null", "null", "adc_extclk_composite",
"null" };
static MUX(cts_atv_dmd_mux, HHI_ATV_DMD_SYS_CLK_CNTL, 0x3, 8,
cts_atv_dmd_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(cts_atv_dmd_div, HHI_ATV_DMD_SYS_CLK_CNTL, 0, 7, "cts_atv_dmd_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(cts_atv_dmd_gate, HHI_ATV_DMD_SYS_CLK_CNTL, 7, "cts_atv_dmd_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/*vdec clock*/
/* cts_vdec_clk */
PNAME(dec_parent_names) = { "fclk_div4", "fclk_div3",
"fclk_div5", "fclk_div7", "mpll1", "mpll2", "gp0_pll", "xtal" };
static MUX(vdec_p0_mux, HHI_VDEC_CLK_CNTL, 0x7, 9, dec_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(vdec_p0_div, HHI_VDEC_CLK_CNTL, 0, 7, "vdec_p0_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(vdec_p0_gate, HHI_VDEC_CLK_CNTL, 8, "vdec_p0_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static MUX(vdec_p1_mux, HHI_VDEC3_CLK_CNTL, 0x7, 9, dec_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(vdec_p1_div, HHI_VDEC3_CLK_CNTL, 0, 7, "vdec_p1_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(vdec_p1_gate, HHI_VDEC3_CLK_CNTL, 8, "vdec_p1_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* vdec_mux clk */
PNAME(vdec_mux_parent_names) = { "vdec_p0_composite", "vdec_p1_composite" };
static MESON_MUX(vdec_mux, HHI_VDEC3_CLK_CNTL, 0x1, 15,
vdec_mux_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* hcodev clock*/
static MUX(hcodec_p0_mux, HHI_VDEC_CLK_CNTL, 0x7, 25, dec_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(hcodec_p0_div, HHI_VDEC_CLK_CNTL, 16, 7, "hcodec_p0_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(hcodec_p0_gate, HHI_VDEC_CLK_CNTL, 24, "hcodec_p0_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static MUX(hcodec_p1_mux, HHI_VDEC3_CLK_CNTL, 0x7, 25, dec_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(hcodec_p1_div, HHI_VDEC3_CLK_CNTL, 16, 7, "hcodec_p1_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(hcodec_p1_gate, HHI_VDEC3_CLK_CNTL, 24, "hcodec_p1_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* hcodec_mux clk */
PNAME(hcodec_mux_parent_names) = {
"hcodec_p0_composite", "hcodec_p1_composite" };
static MESON_MUX(hcodec_mux, HHI_VDEC3_CLK_CNTL, 0x1, 31,
hcodec_mux_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);


/* hevc clock */
static MUX(hevc_p0_mux, HHI_VDEC2_CLK_CNTL, 0x7, 25, dec_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(hevc_p0_div, HHI_VDEC2_CLK_CNTL, 16, 7, "hevc_p0_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(hevc_p0_gate, HHI_VDEC2_CLK_CNTL, 24, "hevc_p0_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static MUX(hevc_p1_mux, HHI_VDEC4_CLK_CNTL, 0x7, 25, dec_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(hevc_p1_div, HHI_VDEC4_CLK_CNTL, 16, 7, "hevc_p1_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(hevc_p1_gate, HHI_VDEC4_CLK_CNTL, 24, "hevc_p1_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* hevc_mux clk */
PNAME(hevc_mux_parent_names) = { "hevc_p0_composite", "hevc_p1_composite" };
static MESON_MUX(hevc_mux, HHI_VDEC4_CLK_CNTL, 0x1, 31,
hevc_mux_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* audio clocks */
/* cts amclk */
PNAME(cts_amclk_parent_names) = { "hdmirx_aud_pll_clk",
"mpll0", "mpll1", "mpll2", "mpll3", "gp0_pll", "gp0_pll", "null" };
static MUX(cts_amclk_mux, HHI_AUD_CLK_CNTL, 0x7, 9, cts_amclk_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(cts_amclk_div, HHI_AUD_CLK_CNTL, 0, 7, "cts_amclk_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(cts_amclk_gate, HHI_AUD_CLK_CNTL, 8, "cts_amclk_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* cts_iec958_spdif clock
 *   |\
 * 0 | \
 *	 |  |-------
 * 1 | /
 *   |/
 * offset 0x64
 * when bit[27] = 0,  cts_amclk_composite
 * when bit[27] = 1,  iec958_int_composite
 */
PNAME(cts_iec958_spdif_parent_names) = {
"cts_amclk_composite", "iec958_int_composite" };
static MUX(cts_iec958_spdif, HHI_AUD_CLK_CNTL2, 0x1, 27,
cts_iec958_spdif_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* iec958_int_composite, bit27 = 1
 *PNAME(cts_iec958_int_parent_names) = { "hdmirx_aud_pll_clk", "mpll0",
 *"mpll1", "mpll2", "mpll3", "gp0_pll", "gp1_pll", "null"};
 */
PNAME(cts_iec958_int_parent_names) = { "hdmirx_aud_pll_clk", "mpll0",
"mpll1", "mpll2" };
static MUX(cts_iec958_int_mux, HHI_AUD_CLK_CNTL2, 0x3, 25,
cts_iec958_int_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(cts_iec958_int_div, HHI_AUD_CLK_CNTL2,
16, 8, "cts_iec958_int_mux", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(cts_iec958_int_gate, HHI_AUD_CLK_CNTL2,
24, "cts_iec958_int_div", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* cts pdm*/
PNAME(cts_pdm_parent_names) = { "cts_amclk_composite", "mpll0",
"mpll1", "mpll2", "mpll3", "gp0_pll", "gp1_pll", "hdmirx_aud_pll_clk"};
static MUX(cts_pdm_mux, HHI_AUD_CLK_CNTL3, 0x7, 17, cts_pdm_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(cts_pdm_div, HHI_AUD_CLK_CNTL3, 0, 16, "cts_pdm_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(cts_pdm_gate, HHI_AUD_CLK_CNTL3, 16, "cts_pdm_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* cts audin mclk */
PNAME(cts_audin_mclk_parent_names) = { "vid2_pll", "mpll0", "mpll1",
"mpll2", "gp0_pll", "gp1_pll", "hdmirx_aud_pll_clk", "vid_pll"};
static MUX(cts_audin_mclk_mux, HHI_AUD_CLK_CNTL4, 0x7, 12,
cts_audin_mclk_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(cts_audin_mclk_div, HHI_AUD_CLK_CNTL4,
0, 10, "cts_audin_mclk_mux", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(cts_audin_mclk_gate, HHI_AUD_CLK_CNTL4,
15, "cts_audin_mclk_div", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* cts audin sclk */
PNAME(cts_audin_sclk_parent_names) = { "cts_audin_mclk_composite" };
static DIV(cts_audin_sclk_div, HHI_AUD_CLK_CNTL4,
16, 7, "cts_audin_mclk_composite", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(cts_audin_sclk_gate, HHI_AUD_CLK_CNTL4,
23, "cts_audin_sclk_div", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* cts audin lrclk */
PNAME(cts_audin_lrclk_parent_names) = { "cts_audin_sclk_composite" };
static DIV(cts_audin_lrclk_div, HHI_AUD_CLK_CNTL4,
24, 7, "cts_audin_sclk_composite", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(cts_audin_lrclk_gate, HHI_AUD_CLK_CNTL4,
31, "cts_audin_lrclk_div", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);


/* cts alocker in clk*/
PNAME(cts_alocker_in_parent_names) = { "cts_hdmirx_aud_pll_clk",
"cts_audin_mclk_composite", "audin_pcm_clk_in", "audin_hdmirx_aud_clk",
"audin_i2s_sclk_in_0", "audin_i2s_sclk_in_1", "audin_i2s_sclk_in_2",
"audin_i2s_sclk_in_3"};
static MUX(cts_alocker_in_mux, HHI_ALOCKER_CLK_CNTL, 0x7, 9,
cts_alocker_in_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(cts_alocker_in_div, HHI_ALOCKER_CLK_CNTL,
0, 8, "cts_alocker_in_mux", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(cts_alocker_in_gate, HHI_ALOCKER_CLK_CNTL,
8, "cts_alocker_in_div", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* cts alocker out clk*/
PNAME(cts_alocker_out_parent_names) = {
"cts_amclk_composite", "cts_clk_i958", "cts_aoclk_int",
"cts_pdm_composite", "cts_pcm_mclk_composite",
"cts_audin_mclk_composite", "cts_encp_clk", "cts_enci_clk" };
static MUX(cts_alocker_out_mux, HHI_ALOCKER_CLK_CNTL, 0x7, 25,
cts_alocker_out_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(cts_alocker_out_div, HHI_ALOCKER_CLK_CNTL,
16, 8, "cts_alocker_out_mux", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(cts_alocker_out_gate, HHI_ALOCKER_CLK_CNTL,
24, "cts_alocker_out_div", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* cts pcm mclk */
PNAME(cts_pcm_mclk_parent_names) = {
"mpll0", "mpll1", "mpll2", "fclk_div5" };
static MUX(cts_pcm_mclk_mux, HHI_PCM_CLK_CNTL, 0x3, 10,
cts_pcm_mclk_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(cts_pcm_mclk_div, HHI_PCM_CLK_CNTL, 0, 9, "cts_pcm_mclk_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(cts_pcm_mclk_gate, HHI_PCM_CLK_CNTL, 9, "cts_pcm_mclk_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* cts pcm sclk */
PNAME(cts_pcm_sclk_parent_names) = { "cts_pcm_mclk_composite" };
static DIV(cts_pcm_sclk_div, HHI_PCM_CLK_CNTL,
16, 6, "cts_pcm_mclk_composite", CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(cts_pcm_sclk_gate, HHI_PCM_CLK_CNTL, 22, "cts_pcm_sclk_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* alt 32k clk */
PNAME(alt_32k_parent_names) = { "xtal", "null", "fclk_div3", "fclk_div5" };
static MUX(alt_32k_mux, HHI_32K_CLK_CNTL, 0x3, 16,
alt_32k_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(alt_32k_div, HHI_32K_CLK_CNTL, 0, 14, "alt_32k_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(alt_32k_gate, HHI_32K_CLK_CNTL, 15, "alt_32k_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

#if 0
static struct clk_hw *media_vhec_mux[] = {
	[CLKID_VDEC_MUX - CLKID_VDEC_MUX] = &vdec_mux.hw,
	[CLKID_HCODEC_MUX - CLKID_VDEC_MUX] = &hcodec_mux.hw,
	[CLKID_HEVC_MUX - CLKID_VDEC_MUX] = &hevc_mux.hw,
};
#endif

/* for init mux clocks reg base*/
static struct clk_mux *txl_media_clk_muxes[] = {
	&vdin_meas_mux,
	&vpu_p0_mux,
	&vpu_p1_mux,
	&vpu_mux,
	&vapb_p0_mux,
	&vapb_p1_mux,
	&vapb_mux,
	&vpu_clkb_tmp_mux,
	&cts_vid_lock_mux,
	&hdmirx_cfg_mux,
	&hdmirx_modet_mux,
	&hdmirx_audmeas_mux,
	&hdmirx_acr_mux,
	&hdcp22_skp_mux,
	&hdcp22_esm_mux,
	&cts_demod_mux,
	&adc_extclk_in_mux,
	&cts_atv_dmd_mux,
	&vdec_p0_mux,
	&vdec_p1_mux,
	&vdec_mux,
	&hcodec_p0_mux,
	&hcodec_p1_mux,
	&hcodec_mux,
	&hevc_p0_mux,
	&hevc_p1_mux,
	&hevc_mux,
	&cts_amclk_mux,
	&cts_iec958_int_mux,
	&cts_iec958_spdif,
	&cts_pdm_mux,
	&cts_audin_mclk_mux,
	&cts_alocker_in_mux,
	&cts_alocker_out_mux,
	&cts_pcm_mclk_mux,
	&alt_32k_mux,
};

/* for init div clocks reg base*/
static struct clk_divider *txl_media_clk_divs[] = {
	&vdin_meas_div,
	&vpu_p0_div,
	&vpu_p1_div,
	&vapb_p0_div,
	&vapb_p1_div,
	&vpu_clkb_tmp_div,
	&vpu_clkb_div,
	&cts_vid_lock_div,
	&hdmirx_cfg_div,
	&hdmirx_modet_div,
	&hdmirx_audmeas_div,
	&hdmirx_acr_div,
	&hdcp22_skp_div,
	&hdcp22_esm_div,
	&cts_demod_div,
	&adc_extclk_in_div,
	&cts_atv_dmd_div,
	&vdec_p0_div,
	&vdec_p1_div,
	&hcodec_p0_div,
	&hcodec_p1_div,
	&hevc_p0_div,
	&hevc_p1_div,
	&cts_amclk_div,
	&cts_iec958_int_div,
	&cts_pdm_div,
	&cts_audin_mclk_div,
	&cts_audin_sclk_div,
	&cts_audin_lrclk_div,
	&cts_alocker_in_div,
	&cts_alocker_out_div,
	&cts_pcm_mclk_div,
	&cts_pcm_sclk_div,
	&alt_32k_div,
};

/* for init gate clocks reg base*/
static struct clk_gate *txl_media_clk_gates[] = {
	&vdin_meas_gate,
	&vpu_p0_gate,
	&vpu_p1_gate,
	&vapb_p0_gate,
	&vapb_p1_gate,
	&ge2d_gate,
	&vpu_clkb_tmp_gate,
	&vpu_clkb_gate,
	&cts_vid_lock_gate,
	&hdmirx_cfg_gate,
	&hdmirx_modet_gate,
	&hdmirx_audmeas_gate,
	&hdmirx_acr_gate,
	&hdcp22_skp_gate,
	&hdcp22_esm_gate,
	&cts_demod_gate,
	&adc_extclk_in_gate,
	&cts_atv_dmd_gate,
	&vdec_p0_gate,
	&vdec_p1_gate,
	&hcodec_p0_gate,
	&hcodec_p1_gate,
	&hevc_p0_gate,
	&hevc_p1_gate,
	&cts_amclk_gate,
	&cts_iec958_int_gate,
	&cts_pdm_gate,
	&cts_audin_mclk_gate,
	&cts_audin_sclk_gate,
	&cts_audin_lrclk_gate,
	&cts_alocker_in_gate,
	&cts_alocker_out_gate,
	&cts_pcm_mclk_gate,
	&cts_pcm_sclk_gate,
	&alt_32k_gate,
};

static struct meson_composite m_composite[] = {
	{CLKID_VPU_CLKB_TMP_COMP, "vpu_clkb_tmp_composite",
	vpu_clkb_parent_names, ARRAY_SIZE(vpu_clkb_parent_names),
	&vpu_clkb_tmp_mux.hw, &vpu_clkb_tmp_div.hw,
	&vpu_clkb_tmp_gate.hw, 0
	},/*vpu_clkb_tmp*/

	{CLKID_VPU_CLKB_COMP, "vpu_clkb_composite",
	vpu_clkb_nomux_parent_names, ARRAY_SIZE(vpu_clkb_nomux_parent_names),
	NULL, &vpu_clkb_div.hw, &vpu_clkb_gate.hw, 0
	},/*vpu_clkb*/

	{CLKID_VDIN_MEAS_COMP, "vdin_meas_composite",
	meas_parent_names, ARRAY_SIZE(meas_parent_names),
	&vdin_meas_mux.hw, &vdin_meas_div.hw,
	&vdin_meas_gate.hw, 0
	},/*vdin_meas*/

	{CLKID_VPU_P0_COMP, "vpu_p0_composite",
	vpu_parent_names, ARRAY_SIZE(vpu_parent_names),
	&vpu_p0_mux.hw, &vpu_p0_div.hw,
	&vpu_p0_gate.hw, 0
	},/* cts_vpu_clk p0*/

	{CLKID_VPU_P1_COMP, "vpu_p1_composite",
	vpu_parent_names, ARRAY_SIZE(vpu_parent_names),
	&vpu_p1_mux.hw, &vpu_p1_div.hw,
	&vpu_p1_gate.hw, 0
	},

	{CLKID_VAPB_P0_COMP, "vapb_p0_composite",
	vpu_parent_names, ARRAY_SIZE(vpu_parent_names),
	&vapb_p0_mux.hw, &vapb_p0_div.hw,
	&vapb_p0_gate.hw, 0
	},

	{CLKID_VAPB_P1_COMP, "vapb_p1_composite",
	vpu_parent_names, ARRAY_SIZE(vpu_parent_names),
	&vapb_p1_mux.hw, &vapb_p1_div.hw,
	&vapb_p1_gate.hw, 0
	},

	{CLKID_HDMIRX_CFG_COMP, "hdmirx_cfg_composite",
	hdmirx_parent_names, ARRAY_SIZE(hdmirx_parent_names),
	&hdmirx_cfg_mux.hw, &hdmirx_cfg_div.hw,
	&hdmirx_cfg_gate.hw, 0
	},

	{CLKID_HDMIRX_MODET_COMP, "hdmirx_modet_composite",
	hdmirx_parent_names, ARRAY_SIZE(hdmirx_parent_names),
	&hdmirx_modet_mux.hw, &hdmirx_modet_div.hw,
	&hdmirx_modet_gate.hw, 0
	},

	{CLKID_HDMIRX_AUDMEAS_COMP, "hdmirx_audmeas_composite",
	hdmirx_ref_parent_names, ARRAY_SIZE(hdmirx_ref_parent_names),
	&hdmirx_audmeas_mux.hw, &hdmirx_audmeas_div.hw,
	&hdmirx_audmeas_gate.hw, 0
	},

	{CLKID_HDMIRX_ACR_COMP, "hdmirx_acr_composite",
	hdmirx_ref_parent_names, ARRAY_SIZE(hdmirx_ref_parent_names),
	&hdmirx_acr_mux.hw, &hdmirx_acr_div.hw,
	&hdmirx_acr_gate.hw, 0
	},

	{CLKID_HDCP22_SKP_COMP, "hdcp22_skp_composite",
	hdcp22_skp_parent_names, ARRAY_SIZE(hdcp22_skp_parent_names),
	&hdcp22_skp_mux.hw, &hdcp22_skp_div.hw,
	&hdcp22_skp_gate.hw, 0
	},

	{CLKID_HDCP22_ESM_COMP, "hdcp22_esm_composite",
	hdcp22_esm_parent_names, ARRAY_SIZE(hdcp22_esm_parent_names),
	&hdcp22_esm_mux.hw, &hdcp22_esm_div.hw,
	&hdcp22_esm_gate.hw, 0
	},

	{CLKID_VDEC_P0_COMP, "vdec_p0_composite",
	dec_parent_names, ARRAY_SIZE(dec_parent_names),
	&vdec_p0_mux.hw, &vdec_p0_div.hw,
	&vdec_p0_gate.hw, 0
	},

	{CLKID_VDEC_P1_COMP, "vdec_p1_composite",
	dec_parent_names, ARRAY_SIZE(dec_parent_names),
	&vdec_p1_mux.hw, &vdec_p1_div.hw,
	&vdec_p1_gate.hw, 0
	},

	{CLKID_HCODEC_P0_COMP, "hcodec_p0_composite",
	dec_parent_names, ARRAY_SIZE(dec_parent_names),
	&hcodec_p0_mux.hw, &hcodec_p0_div.hw,
	&hcodec_p0_gate.hw, 0
	},

	{CLKID_HCODEC_P1_COMP, "hcodec_p1_composite",
	dec_parent_names, ARRAY_SIZE(dec_parent_names),
	&hcodec_p1_mux.hw, &hcodec_p1_div.hw,
	&hcodec_p1_gate.hw, 0
	},

	{CLKID_HEVC_P0_COMP, "hevc_p0_composite",
	dec_parent_names, ARRAY_SIZE(dec_parent_names),
	&hevc_p0_mux.hw, &hevc_p0_div.hw,
	&hevc_p0_gate.hw, 0
	},

	{CLKID_HEVC_P1_COMP, "hevc_p1_composite",
	dec_parent_names, ARRAY_SIZE(dec_parent_names),
	&hevc_p1_mux.hw, &hevc_p1_div.hw,
	&hevc_p1_gate.hw, 0
	},

	{CLKID_AMCLK_COMP, "cts_amclk_composite",
	cts_amclk_parent_names, ARRAY_SIZE(cts_amclk_parent_names),
	&cts_amclk_mux.hw, &cts_amclk_div.hw,
	&cts_amclk_gate.hw, 0
	},/* cts_amclk in clktress doc*/

	{CLKID_IEC958_INT_COMP, "iec958_int_composite",
	cts_iec958_int_parent_names, ARRAY_SIZE(cts_iec958_int_parent_names),
	&cts_iec958_int_mux.hw, &cts_iec958_int_div.hw,
	&cts_iec958_int_gate.hw, 0
	},

	{CLKID_PDM_COMP, "cts_pdm_composite",
	cts_pdm_parent_names, ARRAY_SIZE(cts_pdm_parent_names),
	&cts_pdm_mux.hw, &cts_pdm_div.hw,
	&cts_pdm_gate.hw, 0
	},/* cts_pdm in clktrees doc */

	{CLKID_AUDIN_MCLK_COMP, "cts_audin_mclk_composite",
	cts_audin_mclk_parent_names, ARRAY_SIZE(cts_audin_mclk_parent_names),
	&cts_audin_mclk_mux.hw, &cts_audin_mclk_div.hw,
	&cts_audin_mclk_gate.hw, 0
	},/* cts_audin_mclk in clktrees doc */

	{CLKID_AUDIN_SCLK_COMP, "cts_audin_sclk_composite",
	cts_audin_sclk_parent_names, ARRAY_SIZE(cts_audin_sclk_parent_names),
	NULL, &cts_audin_sclk_div.hw, &cts_audin_sclk_gate.hw, 0
	},/* cts_audin_sclk in clktrees doc */

	{CLKID_AUDIN_LRCLK_COMP, "cts_audin_lrclk_composite",
	cts_audin_lrclk_parent_names, ARRAY_SIZE(cts_audin_lrclk_parent_names),
	NULL, &cts_audin_lrclk_div.hw, &cts_audin_lrclk_gate.hw, 0
	},/* cts_audin_lrclk in clktrees doc */

	{CLKID_ALOCKER_IN_COMP, "cts_alocker_in_composite",
	cts_alocker_in_parent_names, ARRAY_SIZE(cts_alocker_in_parent_names),
	&cts_alocker_in_mux.hw, &cts_alocker_in_div.hw,
	&cts_alocker_in_gate.hw, 0
	},/* cts_alocker_in_clk in clktrees doc */

	{CLKID_ALOCKER_OUT_COMP, "cts_alocker_out_composite",
	cts_alocker_out_parent_names, ARRAY_SIZE(cts_alocker_out_parent_names),
	&cts_alocker_out_mux.hw, &cts_alocker_out_div.hw,
	&cts_alocker_out_gate.hw, 0
	},/* cts_alocker_out_clk in clktrees doc */

	{CLKID_PCM_MCLK_COMP, "cts_pcm_mclk_composite",
	cts_pcm_mclk_parent_names, ARRAY_SIZE(cts_pcm_mclk_parent_names),
	&cts_pcm_mclk_mux.hw, &cts_pcm_mclk_div.hw,
	&cts_pcm_mclk_gate.hw, 0
	},/* cts_pcm_mclk in clktrees doc */

	{CLKID_PCM_SCLK_COMP, "cts_pcm_sclk_composite",
	cts_pcm_sclk_parent_names, ARRAY_SIZE(cts_pcm_sclk_parent_names),
	NULL, &cts_pcm_sclk_div.hw, &cts_pcm_sclk_gate.hw, 0
	},/* cts_pcm_sclk in clktrees doc */

	{CLKID_CTS_DEMOD_COMP, "cts_demod_composite",
	cts_demod_parent_names, ARRAY_SIZE(cts_demod_parent_names),
	&cts_demod_mux.hw, &cts_demod_div.hw,
	&cts_demod_gate.hw, 0
	},/* cts vid lock clock in clktrees doc */

	{CLKID_ADC_EXTCLK_IN_COMP, "adc_extclk_composite",
	adc_extclk_in_parent_names, ARRAY_SIZE(adc_extclk_in_parent_names),
	&adc_extclk_in_mux.hw, &adc_extclk_in_div.hw,
	&adc_extclk_in_gate.hw, 0
	},/* cts adc extclk clock in clktrees doc */

	{CLKID_VID_LOCK_COMP, "vid_lock_composite",
	cts_vid_lock_parent_names, ARRAY_SIZE(cts_vid_lock_parent_names),
	&cts_vid_lock_mux.hw, &cts_vid_lock_div.hw,
	&cts_vid_lock_gate.hw, 0
	},/* vid lock in clktrees doc */

	{CLKID_CTS_ATV_DMD_COMP, "cts_atv_dmd_composite",
	cts_atv_dmd_parent_names, ARRAY_SIZE(cts_atv_dmd_parent_names),
	&cts_atv_dmd_mux.hw, &cts_atv_dmd_div.hw,
	&cts_atv_dmd_gate.hw, 0
	},/* vid lock in clktrees doc */

	{CLKID_ALT_32K_COMP, "alt_32k_composite",
	alt_32k_parent_names, ARRAY_SIZE(alt_32k_parent_names),
	&alt_32k_mux.hw, &alt_32k_div.hw,
	&alt_32k_gate.hw, 0
	},/* alt 32k in clktrees doc */
};

/* array for single hw*/
static struct meson_hw m_hw[] = {
	{CLKID_VDEC_MUX, &vdec_mux.hw},
	{CLKID_HCODEC_MUX, &hcodec_mux.hw},
	{CLKID_HEVC_MUX, &hevc_mux.hw},
};
void meson_txl_media_init(void)
{
	int i;
	int length, lengthhw;

	length = ARRAY_SIZE(m_composite);
	lengthhw = ARRAY_SIZE(m_hw);

	/* Populate base address for media muxes */
	for (i = 0; i < ARRAY_SIZE(txl_media_clk_muxes); i++)
		txl_media_clk_muxes[i]->reg = clk_base +
			(unsigned long)txl_media_clk_muxes[i]->reg;

	/* Populate base address for media divs */
	for (i = 0; i < ARRAY_SIZE(txl_media_clk_divs); i++)
		txl_media_clk_divs[i]->reg = clk_base +
			(unsigned long)txl_media_clk_divs[i]->reg;

	/* Populate base address for media gates */
	for (i = 0; i < ARRAY_SIZE(txl_media_clk_gates); i++)
		txl_media_clk_gates[i]->reg = clk_base +
			(unsigned long)txl_media_clk_gates[i]->reg;

	meson_clk_register_composite(clks, m_composite, length);

	clks[CLKID_VPU_MUX] = clk_register(NULL, &vpu_mux.hw);
	WARN_ON(IS_ERR(clks[CLKID_VPU_MUX]));
	clk_prepare_enable(clks[CLKID_VPU_MUX]);

	clks[CLKID_VAPB_MUX] = clk_register(NULL, &vapb_mux.hw);
	WARN_ON(IS_ERR(clks[CLKID_VAPB_MUX]));
	clk_prepare_enable(clks[CLKID_VAPB_MUX]);

	clks[CLKID_GE2D_GATE] = clk_register(NULL, &ge2d_gate.hw);
	WARN_ON(IS_ERR(clks[CLKID_GE2D_GATE]));

	/*mux*/
	clks[CLKID_VDEC_MUX] = clk_register(NULL, &vdec_mux.hw);
	WARN_ON(IS_ERR(clks[CLKID_VDEC_MUX]));

	clks[CLKID_HCODEC_MUX] = clk_register(NULL, &hcodec_mux.hw);
	WARN_ON(IS_ERR(clks[CLKID_HCODEC_MUX]));

	clks[CLKID_HEVC_MUX] = clk_register(NULL, &hevc_mux.hw);
	WARN_ON(IS_ERR(clks[CLKID_HEVC_MUX]));

	clks[CLKID_IEC958_MUX] = clk_register(NULL, &cts_iec958_spdif.hw);
	WARN_ON(IS_ERR(clks[CLKID_IEC958_MUX]));
	/* todo: set default div4 parent for tmp clkb */
	clk_set_parent(clks[CLKID_VPU_CLKB_TMP_COMP], clks[CLKID_FCLK_DIV4]);
}
