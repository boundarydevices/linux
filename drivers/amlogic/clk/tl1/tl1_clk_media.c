/*
 * drivers/amlogic/clk/tl1/tl1_clk_media.c
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
#include <dt-bindings/clock/amlogic,tl1-clkc.h>

#include "../clkc.h"
#include "tl1.h"

/* cts_vdin_meas_clk */
PNAME(meas_parent_names) = { "xtal", "fclk_div4",
"fclk_div3", "fclk_div5", "vid_pll", "null", "null", "null" };
static MUX(vdin_meas_mux, HHI_VDIN_MEAS_CLK_CNTL, 0x7, 9, meas_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(vdin_meas_div, HHI_VDIN_MEAS_CLK_CNTL, 0, 7, "vdin_meas_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(vdin_meas_gate, HHI_VDIN_MEAS_CLK_CNTL, 8, "vdin_meas_div",
			CLK_GET_RATE_NOCACHE);

/* cts_vpu_p0_clk */
PNAME(vpu_parent_names) = { "fclk_div3", "fclk_div4", "fclk_div5",
"fclk_div7", "mpll1", "vid_pll", "hifi_pll",  "gp0_pll"};
static MUX(vpu_p0_mux, HHI_VPU_CLK_CNTL, 0x7, 9, vpu_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(vpu_p0_div, HHI_VPU_CLK_CNTL, 0, 7, "vpu_p0_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(vpu_p0_gate, HHI_VPU_CLK_CNTL, 8, "vpu_p0_div",
			CLK_GET_RATE_NOCACHE);

/* cts_vpu_p1_clk */
static MUX(vpu_p1_mux, HHI_VPU_CLK_CNTL, 0x7, 25, vpu_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(vpu_p1_div, HHI_VPU_CLK_CNTL, 16, 7, "vpu_p1_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(vpu_p1_gate, HHI_VPU_CLK_CNTL, 24, "vpu_p1_div",
			CLK_GET_RATE_NOCACHE);
/* vpu mux clk */
PNAME(vpu_mux_parent_names) = { "vpu_p0_composite", "vpu_p1_composite" };
static MESON_MUX(vpu_mux, HHI_VPU_CLK_CNTL, 0x1, 31,
		vpu_mux_parent_names, CLK_GET_RATE_NOCACHE);

/* vpu_clkb_tmp */
PNAME(vpu_clkb_parent_names) = { "vpu_mux",
				"fclk_div4", "fclk_div5", "fclk_div7" };
static MUX(vpu_clkb_tmp_mux, HHI_VPU_CLKB_CNTL, 0x3, 20,
vpu_clkb_parent_names, CLK_GET_RATE_NOCACHE);
static DIV(vpu_clkb_tmp_div, HHI_VPU_CLKB_CNTL, 16, 4, "vpu_clkb_tmp_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(vpu_clkb_tmp_gate, HHI_VPU_CLKB_CNTL, 24, "vpu_clkb_tmp_div",
			CLK_GET_RATE_NOCACHE);

/* vpu_clkb */
PNAME(vpu_clkb_nomux_parent_names) = { "vpu_clkb_tmp_composite" };
static DIV(vpu_clkb_div, HHI_VPU_CLKB_CNTL, 0, 8, "vpu_clkb_tmp_composite",
			CLK_GET_RATE_NOCACHE);
static GATE(vpu_clkb_gate, HHI_VPU_CLKB_CNTL, 8, "vpu_clkb_div",
			CLK_GET_RATE_NOCACHE);

/* vpu_clkc*/
PNAME(vpu_clkc_parent_names) = { "fclk_div4", "fclk_div3", "fclk_div5",
"fclk_div7", "mpll1", "vid_pll", "mpll2",  "gp0_pll"};
static MUX(vpu_clkc_p0_mux, HHI_VPU_CLKC_CNTL, 0x7, 9, vpu_clkc_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(vpu_clkc_p0_div, HHI_VPU_CLKC_CNTL, 0, 7, "vpu_clkc_p0_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(vpu_clkc_p0_gate, HHI_VPU_CLKC_CNTL, 8, "vpu_clkc_p0_div",
			CLK_GET_RATE_NOCACHE);

static MUX(vpu_clkc_p1_mux, HHI_VPU_CLKC_CNTL, 0x7, 25, vpu_clkc_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(vpu_clkc_p1_div, HHI_VPU_CLKC_CNTL, 16, 7, "vpu_clkc_p1_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(vpu_clkc_p1_gate, HHI_VPU_CLKC_CNTL, 24, "vpu_clkc_p1_div",
			CLK_GET_RATE_NOCACHE);

/* vpu_clkc mux clk */
PNAME(vpu_clkc_mux_parent_names) = { "vpu_clkc_p0_composite",
				"vpu_clkc_p1_composite" };
static MESON_MUX(vpu_clkc_mux, HHI_VPU_CLKC_CNTL, 0x1, 31,
		vpu_clkc_mux_parent_names, CLK_GET_RATE_NOCACHE);

/* cts_bt656_clk0 */
PNAME(bt656_clk0_parent_names) = { "fclk_div2", "fclk_div3", "fclk_div5",
								"fclk_div7" };
static MUX(bt656_clk0_mux, HHI_BT656_CLK_CNTL, 0x3, 9, bt656_clk0_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(bt656_clk0_div, HHI_BT656_CLK_CNTL, 0, 7, "bt656_clk0_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(bt656_clk0_gate, HHI_BT656_CLK_CNTL, 7, "bt656_clk0_div",
			CLK_GET_RATE_NOCACHE);

/* cts_tcon_pll_clk */
PNAME(tcon_pll_parent_names) = { "xtal", "fclk_div5", "fclk_div4",
"fclk_div3", "mpll2", "mpll3", "vid_pll", "gp0" };
static MUX(tcon_pll_mux, HHI_TCON_CLK_CNTL, 0x3, 7, tcon_pll_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(tcon_pll_div, HHI_TCON_CLK_CNTL, 0, 6, "tcon_pll_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(tcon_pll_gate, HHI_TCON_CLK_CNTL, 6, "tcon_pll_div",
			CLK_GET_RATE_NOCACHE);

/* cts_vapb p0 clk */
PNAME(vapb_parent_names) = { "fclk_div4", "fclk_div3", "fclk_div5",
"fclk_div7", "mpll1", "vid_pll", "mpll2",  "fclk_div2p5"};
static MUX(vapb_p0_mux, HHI_VAPBCLK_CNTL, 0x7, 9, vapb_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(vapb_p0_div, HHI_VAPBCLK_CNTL, 0, 7, "vapb_p0_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(vapb_p0_gate, HHI_VAPBCLK_CNTL, 8, "vapb_p0_div",
			CLK_GET_RATE_NOCACHE);
/* cts_vapb p1 clk */
static MUX(vapb_p1_mux, HHI_VAPBCLK_CNTL, 0x7, 25, vapb_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(vapb_p1_div, HHI_VAPBCLK_CNTL, 16, 7, "vapb_p1_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(vapb_p1_gate, HHI_VAPBCLK_CNTL, 24, "vapb_p1_div",
			CLK_GET_RATE_NOCACHE);

/* cts_vapb mux clk */
PNAME(vapb_mux_parent_names) = { "vapb_p0_composite", "vapb_p1_composite" };
static MESON_MUX(vapb_mux, HHI_VAPBCLK_CNTL, 0x1, 31,
vapb_mux_parent_names, CLK_GET_RATE_NOCACHE);
static GATE(ge2d_gate, HHI_VAPBCLK_CNTL, 30, "vapb_mux",
			CLK_GET_RATE_NOCACHE);

/*hdmirx cfg clock*/
PNAME(hdmirx_parent_names) = { "xtal", "fclk_div4", "fclk_div3", "fclk_div5" };
static MUX(hdmirx_cfg_mux, HHI_HDMIRX_CLK_CNTL, 0x3, 9, hdmirx_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(hdmirx_cfg_div, HHI_HDMIRX_CLK_CNTL, 0, 7, "hdmirx_cfg_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(hdmirx_cfg_gate, HHI_HDMIRX_CLK_CNTL, 8, "hdmirx_cfg_div",
			CLK_GET_RATE_NOCACHE);
/*hdmirx modet clock*/
static MUX(hdmirx_modet_mux, HHI_HDMIRX_CLK_CNTL, 0x3, 25, hdmirx_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(hdmirx_modet_div, HHI_HDMIRX_CLK_CNTL, 16, 7, "hdmirx_modet_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(hdmirx_modet_gate, HHI_HDMIRX_CLK_CNTL, 24, "hdmirx_modet_div",
			CLK_GET_RATE_NOCACHE);

/*hdmirx audmeas clock*/
PNAME(hdmirx_ref_parent_names) = { "fclk_div4",
"fclk_div3", "fclk_div5", "fclk_div7" };
static MUX(hdmirx_audmeas_mux, HHI_HDMIRX_AUD_CLK_CNTL, 0x3, 9,
hdmirx_ref_parent_names, CLK_GET_RATE_NOCACHE);
static DIV(hdmirx_audmeas_div, HHI_HDMIRX_AUD_CLK_CNTL, 0, 7,
"hdmirx_audmeas_mux", CLK_GET_RATE_NOCACHE);
static GATE(hdmirx_audmeas_gate, HHI_HDMIRX_AUD_CLK_CNTL, 8,
"hdmirx_audmeas_div", CLK_GET_RATE_NOCACHE);
/* hdmirx acr clock*/
static MUX(hdmirx_acr_mux, HHI_HDMIRX_AUD_CLK_CNTL, 0x3, 25,
hdmirx_ref_parent_names, CLK_GET_RATE_NOCACHE);
static DIV(hdmirx_acr_div, HHI_HDMIRX_AUD_CLK_CNTL, 16, 7, "hdmirx_acr_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(hdmirx_acr_gate, HHI_HDMIRX_AUD_CLK_CNTL, 24, "hdmirx_acr_div",
			CLK_GET_RATE_NOCACHE);
/* hdcp22 skpclk*/

PNAME(hdcp22_skp_parent_names) = { "xtal",
"fclk_div4", "fclk_div3", "fclk_div5" };
static MUX(hdcp22_skp_mux, HHI_HDCP22_CLK_CNTL, 0x3, 25,
			hdcp22_skp_parent_names, CLK_GET_RATE_NOCACHE);
static DIV(hdcp22_skp_div, HHI_HDCP22_CLK_CNTL, 16, 7, "hdcp22_skp_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(hdcp22_skp_gate, HHI_HDCP22_CLK_CNTL, 24, "hdcp22_skp_div",
			CLK_GET_RATE_NOCACHE);
/* hdcp22 esm clock */
PNAME(hdcp22_esm_parent_names) = { "fclk_div7",
"fclk_div4", "fclk_div3", "fclk_div5"  };
static MUX(hdcp22_esm_mux, HHI_HDCP22_CLK_CNTL, 0x3, 9,
hdcp22_esm_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(hdcp22_esm_div, HHI_HDCP22_CLK_CNTL, 0, 7, "hdcp22_esm_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(hdcp22_esm_gate, HHI_HDCP22_CLK_CNTL, 8, "hdcp22_esm_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/*vdec clock*/
/* cts_vdec_clk */
PNAME(dec_parent_names) = { "fclk_div2p5", "fclk_div3",
"fclk_div4", "fclk_div5", "fclk_div7", "hifi", "gp0_pll", "xtal" };
static MUX(vdec_p0_mux, HHI_VDEC_CLK_CNTL, 0x7, 9, dec_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(vdec_p0_div, HHI_VDEC_CLK_CNTL, 0, 7, "vdec_p0_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(vdec_p0_gate, HHI_VDEC_CLK_CNTL, 8, "vdec_p0_div",
			CLK_GET_RATE_NOCACHE);
static MUX(vdec_p1_mux, HHI_VDEC3_CLK_CNTL, 0x7, 9, dec_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(vdec_p1_div, HHI_VDEC3_CLK_CNTL, 0, 7, "vdec_p1_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(vdec_p1_gate, HHI_VDEC3_CLK_CNTL, 8, "vdec_p1_div",
			CLK_GET_RATE_NOCACHE);

/* vdec_mux clk */
PNAME(vdec_mux_parent_names) = { "vdec_p0_composite", "vdec_p1_composite" };
static MESON_MUX(vdec_mux, HHI_VDEC3_CLK_CNTL, 0x1, 15,
vdec_mux_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* hcodev clock*/
static MUX(hcodec_p0_mux, HHI_VDEC_CLK_CNTL, 0x7, 25, dec_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(hcodec_p0_div, HHI_VDEC_CLK_CNTL, 16, 7, "hcodec_p0_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(hcodec_p0_gate, HHI_VDEC_CLK_CNTL, 24, "hcodec_p0_div",
			CLK_GET_RATE_NOCACHE);
static MUX(hcodec_p1_mux, HHI_VDEC3_CLK_CNTL, 0x7, 25, dec_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(hcodec_p1_div, HHI_VDEC3_CLK_CNTL, 16, 7, "hcodec_p1_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(hcodec_p1_gate, HHI_VDEC3_CLK_CNTL, 24, "hcodec_p1_div",
			CLK_GET_RATE_NOCACHE);

/* hcodec_mux clk */
PNAME(hcodec_mux_parent_names) = {
"hcodec_p0_composite", "hcodec_p1_composite" };
static MESON_MUX(hcodec_mux, HHI_VDEC3_CLK_CNTL, 0x1, 31,
hcodec_mux_parent_names, CLK_GET_RATE_NOCACHE);


/* hevc clock */
static MUX(hevc_p0_mux, HHI_VDEC2_CLK_CNTL, 0x7, 25, dec_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(hevc_p0_div, HHI_VDEC2_CLK_CNTL, 16, 7, "hevc_p0_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(hevc_p0_gate, HHI_VDEC2_CLK_CNTL, 24, "hevc_p0_div",
			CLK_GET_RATE_NOCACHE);
static MUX(hevc_p1_mux, HHI_VDEC4_CLK_CNTL, 0x7, 25, dec_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(hevc_p1_div, HHI_VDEC4_CLK_CNTL, 16, 7, "hevc_p1_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(hevc_p1_gate, HHI_VDEC4_CLK_CNTL, 24, "hevc_p1_div",
			CLK_GET_RATE_NOCACHE);

/* hevc_mux clk */
PNAME(hevc_mux_parent_names) = { "hevc_p0_composite", "hevc_p1_composite" };
static MESON_MUX(hevc_mux, HHI_VDEC4_CLK_CNTL, 0x1, 31,
hevc_mux_parent_names, CLK_GET_RATE_NOCACHE);

/* hevcf clock */
static MUX(hevcf_p0_mux, HHI_VDEC2_CLK_CNTL, 0x7, 9, dec_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(hevcf_p0_div, HHI_VDEC2_CLK_CNTL, 0, 7, "hevcf_p0_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(hevcf_p0_gate, HHI_VDEC2_CLK_CNTL, 8, "hevcf_p0_div",
			CLK_GET_RATE_NOCACHE);
static MUX(hevcf_p1_mux, HHI_VDEC4_CLK_CNTL, 0x7, 9, dec_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(hevcf_p1_div, HHI_VDEC4_CLK_CNTL, 0, 7, "hevcf_p1_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(hevcf_p1_gate, HHI_VDEC4_CLK_CNTL, 8, "hevcf_p1_div",
			CLK_GET_RATE_NOCACHE);

/* hevcf_mux clk */
PNAME(hevcf_mux_parent_names) = { "hevcf_p0_composite", "hevcf_p1_composite" };
static MESON_MUX(hevcf_mux, HHI_VDEC4_CLK_CNTL, 0x1, 15,
hevcf_mux_parent_names, CLK_GET_RATE_NOCACHE);

/* cts_vid_lock_clk */
PNAME(vid_lock_parent_names) = { "xtal", "cts_encl_clk",
"cts_enci_clk", "cts_encp_clk" };
static MUX(vid_lock_mux, HHI_VID_LOCK_CLK_CNTL, 0x3, 9, vid_lock_parent_names,
			CLK_GET_RATE_NOCACHE);
static DIV(vid_lock_div, HHI_VID_LOCK_CLK_CNTL, 0, 7, "vdec_p0_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(vid_lock_gate, HHI_VID_LOCK_CLK_CNTL, 7, "vdec_p0_div",
			CLK_GET_RATE_NOCACHE);

/* cts demod core clock */
PNAME(cts_demod_parent_names) = { "xtal",
"fclk_div4", "fclk_div3", "adc_dpll_int"};
static MUX(cts_demod_mux, HHI_AUDPLL_CLK_OUT_CNTL, 0x3, 9,
cts_demod_parent_names, CLK_GET_RATE_NOCACHE);
static DIV(cts_demod_div, HHI_AUDPLL_CLK_OUT_CNTL, 0, 7, "cts_demod_mux",
			CLK_GET_RATE_NOCACHE);
static GATE(cts_demod_gate, HHI_AUDPLL_CLK_OUT_CNTL, 8, "cts_demod_div",
			CLK_GET_RATE_NOCACHE);

/* adc extclk in clock */
PNAME(adc_extclk_in_parent_names) = { "xtal", "fclk_div4", "fclk_div3",
"fclk_div5", "fclk_div7", "mpll2", "gp0_pll", "gp1_pll" };
static MUX(adc_extclk_in_mux, HHI_AUDPLL_CLK_OUT_CNTL, 0x7, 25,
adc_extclk_in_parent_names, CLK_GET_RATE_NOCACHE);
static DIV(adc_extclk_in_div, HHI_AUDPLL_CLK_OUT_CNTL, 16, 7,
"adc_extclk_in_mux", CLK_GET_RATE_NOCACHE);
static GATE(adc_extclk_in_gate, HHI_AUDPLL_CLK_OUT_CNTL, 24,
"adc_extclk_in_div", CLK_GET_RATE_NOCACHE);

#if 0
static struct clk_hw *media_vhec_mux[] = {
	[CLKID_VDEC_MUX - CLKID_VDEC_MUX] = &vdec_mux.hw,
	[CLKID_HCODEC_MUX - CLKID_VDEC_MUX] = &hcodec_mux.hw,
	[CLKID_HEVC_MUX - CLKID_VDEC_MUX] = &hevc_mux.hw,
};
#endif

/* for init mux clocks reg base*/
static struct clk_mux *tl1_media_clk_muxes[] = {
	&vdin_meas_mux,
	&vpu_p0_mux,
	&vpu_p1_mux,
	&vpu_mux,
	&vapb_p0_mux,
	&vapb_p1_mux,
	&vapb_mux,
	&vpu_clkb_tmp_mux,
	&hdmirx_cfg_mux,
	&hdmirx_modet_mux,
	&hdmirx_audmeas_mux,
	&hdmirx_acr_mux,
	&hdcp22_skp_mux,
	&hdcp22_esm_mux,
	&vdec_p0_mux,
	&vdec_p1_mux,
	&vdec_mux,
	&hcodec_p0_mux,
	&hcodec_p1_mux,
	&hcodec_mux,
	&hevc_p0_mux,
	&hevc_p1_mux,
	&hevc_mux,
	&hevcf_p0_mux,
	&hevcf_p1_mux,
	&hevcf_mux,
	&vpu_clkc_p0_mux,
	&vpu_clkc_p1_mux,
	&vpu_clkc_mux,
	&vid_lock_mux,
	&bt656_clk0_mux,
	&tcon_pll_mux,
	&cts_demod_mux,
	&adc_extclk_in_mux,
};

/* for init div clocks reg base*/
static struct clk_divider *tl1_media_clk_divs[] = {
	&vdin_meas_div,
	&vpu_p0_div,
	&vpu_p1_div,
	&vapb_p0_div,
	&vapb_p1_div,
	&vpu_clkb_tmp_div,
	&vpu_clkb_div,
	&hdmirx_cfg_div,
	&hdmirx_modet_div,
	&hdmirx_audmeas_div,
	&hdmirx_acr_div,
	&hdcp22_skp_div,
	&hdcp22_esm_div,
	&vdec_p0_div,
	&vdec_p1_div,
	&hcodec_p0_div,
	&hcodec_p1_div,
	&hevc_p0_div,
	&hevc_p1_div,
	&hevcf_p0_div,
	&hevcf_p1_div,
	&vpu_clkc_p0_div,
	&vpu_clkc_p1_div,
	&vid_lock_div,
	&bt656_clk0_div,
	&tcon_pll_div,
	&cts_demod_div,
	&adc_extclk_in_div,
};

/* for init gate clocks reg base*/
static struct clk_gate *tl1_media_clk_gates[] = {
	&vdin_meas_gate,
	&vpu_p0_gate,
	&vpu_p1_gate,
	&vapb_p0_gate,
	&vapb_p1_gate,
	&ge2d_gate,
	&vpu_clkb_tmp_gate,
	&vpu_clkb_gate,
	&hdmirx_cfg_gate,
	&hdmirx_modet_gate,
	&hdmirx_audmeas_gate,
	&hdmirx_acr_gate,
	&hdcp22_skp_gate,
	&hdcp22_esm_gate,
	&vdec_p0_gate,
	&vdec_p1_gate,
	&hcodec_p0_gate,
	&hcodec_p1_gate,
	&hevc_p0_gate,
	&hevc_p1_gate,
	&hevcf_p0_gate,
	&hevcf_p1_gate,
	&vpu_clkc_p0_gate,
	&vpu_clkc_p1_gate,
	&vid_lock_gate,
	&bt656_clk0_gate,
	&tcon_pll_gate,
	&cts_demod_gate,
	&adc_extclk_in_gate,
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

	{CLKID_HEVCF_P0_COMP, "hevcf_p0_composite",
	dec_parent_names, ARRAY_SIZE(dec_parent_names),
	&hevcf_p0_mux.hw, &hevcf_p0_div.hw,
	&hevcf_p0_gate.hw, 0
	},

	{CLKID_HEVCF_P1_COMP, "hevcf_p1_composite",
	dec_parent_names, ARRAY_SIZE(dec_parent_names),
	&hevcf_p1_mux.hw, &hevcf_p1_div.hw,
	&hevcf_p1_gate.hw, 0
	},

	{CLKID_VID_LOCK_COMP, "vid_lock_composite",
	vid_lock_parent_names, ARRAY_SIZE(vid_lock_parent_names),
	&vid_lock_mux.hw, &vid_lock_div.hw,
	&vid_lock_gate.hw, 0
	},

	{CLKID_BT656_CLK0_COMP, "bt656_clk0_composite",
	bt656_clk0_parent_names, ARRAY_SIZE(bt656_clk0_parent_names),
	&bt656_clk0_mux.hw, &bt656_clk0_div.hw,
	&bt656_clk0_gate.hw, 0
	},

	{CLKID_TCON_PLL_COMP, "tcon_pll_composite",
	tcon_pll_parent_names, ARRAY_SIZE(tcon_pll_parent_names),
	&tcon_pll_mux.hw, &tcon_pll_div.hw,
	&tcon_pll_gate.hw, 0
	},

	{CLKID_DEMOD_COMP, "demod_composite",
	cts_demod_parent_names, ARRAY_SIZE(cts_demod_parent_names),
	&cts_demod_mux.hw, &cts_demod_div.hw,
	&cts_demod_gate.hw, 0
	},

	{CLKID_ADC_EXTCLK_COMP, "adc_extclk_composite",
	adc_extclk_in_parent_names, ARRAY_SIZE(adc_extclk_in_parent_names),
	&adc_extclk_in_mux.hw, &adc_extclk_in_div.hw,
	&adc_extclk_in_gate.hw, 0
	},

	{CLKID_VPU_CLKC_P0_COMP, "vpu_clkc_p0_composite",
	vpu_clkc_parent_names, ARRAY_SIZE(vpu_clkc_parent_names),
	&vpu_clkc_p0_mux.hw, &vpu_clkc_p0_div.hw,
	&vpu_clkc_p0_gate.hw, 0
	},

	{CLKID_VPU_CLKC_P1_COMP, "vpu_clkc_p1_composite",
	vpu_clkc_parent_names, ARRAY_SIZE(vpu_clkc_parent_names),
	&vpu_clkc_p1_mux.hw, &vpu_clkc_p1_div.hw,
	&vpu_clkc_p1_gate.hw, 0
	},

	{},
};

/* array for single hw*/
static struct meson_hw m_hw[] = {
	{CLKID_VDEC_MUX, &vdec_mux.hw},
	{CLKID_HCODEC_MUX, &hcodec_mux.hw},
	{CLKID_HEVC_MUX, &hevc_mux.hw},
};
void meson_tl1_media_init(void)
{
	int i;
	int length, lengthhw;

	length = ARRAY_SIZE(m_composite);
	lengthhw = ARRAY_SIZE(m_hw);

	/* Populate base address for media muxes */
	for (i = 0; i < ARRAY_SIZE(tl1_media_clk_muxes); i++)
		tl1_media_clk_muxes[i]->reg = clk_base +
			(unsigned long)tl1_media_clk_muxes[i]->reg;

	/* Populate base address for media divs */
	for (i = 0; i < ARRAY_SIZE(tl1_media_clk_divs); i++)
		tl1_media_clk_divs[i]->reg = clk_base +
			(unsigned long)tl1_media_clk_divs[i]->reg;

	/* Populate base address for media gates */
	for (i = 0; i < ARRAY_SIZE(tl1_media_clk_gates); i++)
		tl1_media_clk_gates[i]->reg = clk_base +
			(unsigned long)tl1_media_clk_gates[i]->reg;

	meson_clk_register_composite(clks, m_composite, length - 1);

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

	clks[CLKID_HEVCF_MUX] = clk_register(NULL, &hevcf_mux.hw);
	WARN_ON(IS_ERR(clks[CLKID_HEVCF_MUX]));

	clks[CLKID_VPU_CLKC_MUX] = clk_register(NULL, &vpu_clkc_mux.hw);
	WARN_ON(IS_ERR(clks[CLKID_VPU_CLKC_MUX]));

	/* todo: set default div4 parent for tmp clkb */
	clk_set_parent(clks[CLKID_VPU_CLKB_TMP_COMP], clks[CLKID_FCLK_DIV4]);
}
