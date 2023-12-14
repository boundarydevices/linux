// SPDX-License-Identifier: GPL-2.0+
/*
 * NEOISP context registers/memory setting helpers
 *
 * Copyright 2023 NXP
 * Author: Aymen Sghaier (aymen.sghaier@nxp.com)
 *
 */

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>
#include <uapi/linux/nxp_neoisp.h>

#include "neoisp.h"
#include "neoisp_regs.h"
#include "neoisp_ctx.h"

/*
 * This is the initial set of parameters setup by driver upon a streamon ioctl for DCG node.
 * It could be updated later by the driver depending on input/output formats setup by userspace
 * and also if fine tuned parameters are provided by a third party (IPA).
 */
struct neoisp_meta_params_s neoisp_default_params = {
	.features_cfg = {
		.hdr_decompress_dcg_cfg = 1,
		.hdr_decompress_vs_cfg = 1,
		.obwb0_cfg = 1,
		.obwb1_cfg = 1,
		.obwb2_cfg = 1,
		.hdr_merge_cfg = 1,
		.rgbir_cfg = 1,
		.stat_cfg = 1,
		.ir_compress_cfg = 1,
		.bnr_cfg = 1,
		.vignetting_ctrl_cfg = 1,
		.ctemp_cfg = 1,
		.demosaic_cfg = 1,
		.rgb2yuv_cfg = 1,
		.dr_comp_cfg = 1,
		.nr_cfg = 1,
		.af_cfg = 1,
		.ee_cfg = 1,
		.df_cfg = 1,
		.convmed_cfg = 1,
		.cas_cfg = 1,
		.gcm_cfg = 1,
		.vignetting_table_cfg = 1,
		.drc_global_tonemap_cfg = 1,
		.drc_local_tonemap_cfg = 1,
	},
	.regs = {
	.decompress_dcg = { .ctrl_enable = 1,
		.knee_point1 = (1 << 16) - 1, /* default ibpp is 16 */
		.knee_ratio0 = 1 << 4,
	},
	.decompress_vs = { .ctrl_enable = 0 },
	.obwb[0] = {
		.ctrl_obpp = 3,
		.r_ctrl_gain = 1 << 8,
		.r_ctrl_offset = 0,
		.gr_ctrl_gain = 1 << 8,
		.gr_ctrl_offset = 0,
		.gb_ctrl_gain = 1 << 8,
		.gb_ctrl_offset = 0,
		.b_ctrl_gain = 1 << 8,
		.b_ctrl_offset = 0,
		},
	.obwb[1] = {
		.ctrl_obpp = 2,
		.r_ctrl_gain = 1 << 8,
		.r_ctrl_offset = 0,
		.gr_ctrl_gain = 1 << 8,
		.gr_ctrl_offset = 0,
		.gb_ctrl_gain = 1 << 8,
		.gb_ctrl_offset = 0,
		.b_ctrl_gain = 1 << 8,
		.b_ctrl_offset = 0,
		},
	.obwb[2] = {
		.ctrl_obpp = 3,
		.r_ctrl_gain = 1 << 8,
		.r_ctrl_offset = 0,
		.gr_ctrl_gain = 1 << 8,
		.gr_ctrl_offset = 0,
		.gb_ctrl_gain = 1 << 8,
		.gb_ctrl_offset = 0,
		.b_ctrl_gain = 1 << 8,
		.b_ctrl_offset = 0,
		},
	.hdr_merge = { .ctrl_enable = 0, },
	.rgbir = { .ctrl_enable = 0, },
	.stat = {},
	.ir_compress = { .ctrl_enable = 0, },
	.bnr = {
		.ctrl_enable = 1,
		.ctrl_debug = 0,
		.ctrl_obpp = 3,
		.ctrl_nhood = 0,
		.ypeak_peak_outsel = 0,
		.ypeak_peak_sel = 0,
		.ypeak_peak_low = 1 << 7,
		.ypeak_peak_high = 1 << 8,
		.yedge_th0_edge_th0 = 20,
		.yedge_scale_scale = 1 << 10,
		.yedge_scale_shift = 10,
		.yedges_th0_edge_th0 = 20,
		.yedges_scale_scale = 1 << 10,
		.yedges_scale_shift = 10,
		.yedgea_th0_edge_th0 = 20,
		.yedgea_scale_scale = 1 << 10,
		.yedgea_scale_shift = 10,
		.yluma_x_th0_th = 20,
		.yluma_y_th_luma_y_th0 = 10,
		.yluma_y_th_luma_y_th1 = 1 << 8,
		.yluma_scale_scale = 1 << 10,
		.yluma_scale_shift = 10,
		.yalpha_gain_gain = 1 << 8,
		.yalpha_gain_offset = 0,
		.cpeak_peak_outsel = 0,
		.cpeak_peak_sel = 0,
		.cpeak_peak_low = 1 << 7,
		.cpeak_peak_high = 1 << 8,
		.cedge_th0_edge_th0 = 10,
		.cedge_scale_scale = 1 << 10,
		.cedge_scale_shift = 10,
		.cedges_th0_edge_th0 = 20,
		.cedges_scale_scale = 1 << 10,
		.cedges_scale_shift = 10,
		.cedgea_th0_edge_th0 = 20,
		.cedgea_scale_scale = 1 << 10,
		.cedgea_scale_shift = 10,
		.cluma_x_th0_th = 20,
		.cluma_y_th_luma_y_th0 = 10,
		.cluma_y_th_luma_y_th1 = 0,
		.cluma_scale_scale = 1 << 10,
		.cluma_scale_shift = 10,
		.calpha_gain_gain = 1 << 8,
		.calpha_gain_offset = 0,
		.stretch_gain = 1 << 8,
	},
	.vignetting_ctrl = { .ctrl_enable = 0, },
	.ctemp  = { .ctrl_enable = 0, },
	.demosaic = {
		.ctrl_fmt = 0,
		.activity_ctl_alpha = 1 << 8,
		.activity_ctl_act_ratio = 1 << 8,
		.dynamics_ctl0_strengthg = 1 << 8,
		.dynamics_ctl0_strengthc = 1 << 8,
		.dynamics_ctl2_max_impact = 1 << 7,
	},
	.rgb2yuv = {
		.gain_ctrl_rgain = 1 << 8,
		.gain_ctrl_bgain = 1 << 8,
		.mat_rxcy = {
			{76, 148,  29},
			{-36, -73, 111},
			{157, -130, -26},
			},
		.csc_offsets = {0, 0, 0},
		},
	.drc = {
		.gbl_gain_gain = 1 << 8,
		.lcl_stretch_stretch = 1 << 8,
		.alpha_alpha = 1 << 8,
		},
	.nrc = { .ctrl_enable = 0, },
	.afc = {{}}, /* bypass */
	.eec = { .ctrl_enable = 0, },
	.dfc = { .ctrl_enable = 0, },
	.convf = { .ctrl_flt = 0, }, /* bypassed */
	.cas = {
		.gain_shift = 0,
		.gain_scale = 1,
		.corr_corr = 0,
		.offset_offset = 0,
		},
	.gcm = {
		.imat_rxcy = {
			{256, 0, 292},
			{256, -101, -149},
			{256, 520, 0},
			},
		.ioffsets = {0, 0, 0},
		.omat_rxcy = {
			{256, 0, 0},
			{0, 256, 0},
			{0, 0, 256},
			},
		.ooffsets = {0, 0, 0},
		.gamma0_gamma0 = 1 << 8,
		.gamma0_offset0 = 0,
		.gamma1_gamma1 = 1 << 8,
		.gamma1_offset1 = 0,
		.gamma2_gamma2 = 1 << 8,
		.gamma2_offset2 = 0,
		.blklvl0_ctrl_gain0 = 1 << 8,
		.blklvl0_ctrl_offset0 = 0,
		.blklvl1_ctrl_gain1 = 1 << 8,
		.blklvl1_ctrl_offset1 = 0,
		.blklvl2_ctrl_gain2 = 1 << 8,
		.blklvl2_ctrl_offset2 = 0,
		.lowth_ctrl01_threshold0 = 0,
		.lowth_ctrl01_threshold1 = 0,
		.lowth_ctrl2_threshold2 = 0,
		.mat_confg_sign_confg = 1,
		},
	},
	.mems = {
		.gtm = {
			.drc_global_tonemap = {
				0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x016f, 0x0ea9,
				0x1a73, 0x2734, 0x3c0b, 0x5229, 0x5c92, 0x6188, 0x64b4, 0x68a0,
				0x72cf, 0x7de6, 0x87c2, 0x9094, 0x9884, 0x9fb3, 0xa63a, 0xac30,
				0xb1a7, 0xb6ae, 0xbb52, 0xbf9e, 0xc39c, 0xc753, 0xcaca, 0xce09,
				0xd113, 0xd3ee, 0xd69f, 0xd928, 0xdb8c, 0xddd0, 0xdff5, 0xe1fe,
				0xe3ed, 0xe5c4, 0xe785, 0xe930, 0xeac8, 0xec4e, 0xedc4, 0xef29,
				0xf07f, 0xf1c7, 0xf302, 0xf431, 0xf555, 0xf66d, 0xf77b, 0xf87f,
				0xf979, 0xfa6b, 0xfb55, 0xfc36, 0xfd11, 0xfde4, 0xfeb0, 0xff75,
				0xffff, 0xfd81, 0xfaf5, 0xf88f, 0xf64b, 0xf426, 0xf21f, 0xf031,
				0xee5d, 0xec9f, 0xeaf7, 0xe962, 0xe7e0, 0xe66f, 0xe50e, 0xe3bc,
				0xe278, 0xe141, 0xe017, 0xdef8, 0xdde4, 0xdcdb, 0xdbdc, 0xdae6,
				0xd9f8, 0xd913, 0xd836, 0xd761, 0xd692, 0xd5cb, 0xd509, 0xd44e,
				0xd399, 0xd02e, 0xccf7, 0xc9ee, 0xc711, 0xc45b, 0xc1ca, 0xbf5b,
				0xbd0a, 0xbad7, 0xb8be, 0xb6bf, 0xb4d7, 0xb304, 0xb146, 0xaf9a,
				0xae01, 0xac78, 0xaaff, 0xa994, 0xa838, 0xa6e9, 0xa5a6, 0xa46f,
				0xa343, 0xa221, 0xa10a, 0x9ffc, 0x9ef5, 0x9deb, 0x9ce6, 0x9bdf,
				0x9afa, 0x983e, 0x95ab, 0x933e, 0x90f3, 0x8ec8, 0x8cba, 0x8ac7,
				0x88ee, 0x872b, 0x857d, 0x83e4, 0x825d, 0x80e8, 0x7f83, 0x7e2d,
				0x7ce5, 0x7bab, 0x7a7c, 0x7955, 0x783d, 0x7727, 0x761d, 0x751b,
				0x7421, 0x7330, 0x724a, 0x7167, 0x7089, 0x6fae, 0x6ed8, 0x6e0c,
				0x6d57, 0x6b78, 0x69b6, 0x680e, 0x667d, 0x6502, 0x639a, 0x6246,
				0x6102, 0x5fce, 0x5ea9, 0x5d91, 0x5c86, 0x5b87, 0x5a93, 0x59a9,
				0x58c9, 0x57f3, 0x5724, 0x565e, 0x55a0, 0x54e9, 0x5438, 0x538e,
				0x52ea, 0x524c, 0x51b3, 0x511f, 0x5091, 0x5007, 0x4f81, 0x4f00,
				0x4e83, 0x4d51, 0x4c32, 0x4b23, 0x4a23, 0x4911, 0x480d, 0x4716,
				0x462c, 0x454c, 0x4473, 0x439d, 0x42d0, 0x420d, 0x4152, 0x40a0,
				0x3ff5, 0x3f54, 0x3eba, 0x3e26, 0x3d94, 0x3d06, 0x3be7, 0x3ad1,
				0x39c6, 0x38c2, 0x37c9, 0x36d7, 0x35ee, 0x350d, 0x3432, 0x335e,
				0x3291, 0x3109, 0x2f97, 0x2e3b, 0x2cf2, 0x2bbb, 0x2a95, 0x297d,
				0x2874, 0x2777, 0x2687, 0x25a1, 0x24c6, 0x23f5, 0x232d, 0x226d,
				0x21b6, 0x2106, 0x205d, 0x1fba, 0x1f1e, 0x1e88, 0x1df7, 0x1d6b,
				0x1ce5, 0x1c63, 0x1be6, 0x1b6d, 0x1af8, 0x1a87, 0x1a19, 0x19af,
				0x1948, 0x1884, 0x17cb, 0x171d, 0x1679, 0x15dd, 0x154a, 0x14be,
				0x143a, 0x13bb, 0x1343, 0x12d0, 0x1263, 0x11fa, 0x1196, 0x1136,
				0x10db, 0x1083, 0x102e, 0x0fdd, 0x0f8f, 0x0f44, 0x0efb, 0x0eb5,
				0x0e72, 0x0e31, 0x0df3, 0x0db6, 0x0d7c, 0x0d43, 0x0d0c, 0x0cd7,
				0x0ca4, 0x0c42, 0x0be5, 0x0b8e, 0x0b3c, 0x0aee, 0x0aa5, 0x0a5f,
				0x0a1d, 0x09dd, 0x09a1, 0x0968, 0x0931, 0x08fd, 0x08cb, 0x089b,
				0x086d, 0x0841, 0x0817, 0x07ee, 0x07c7, 0x07a2, 0x077d, 0x075a,
				0x0739, 0x0718, 0x06f9, 0x06db, 0x06be, 0x06a1, 0x0686, 0x066b,
				0x0652, 0x0621, 0x05f2, 0x05c7, 0x059e, 0x0577, 0x0552, 0x052f,
				0x050e, 0x04ee, 0x04d0, 0x04b4, 0x0498, 0x047e, 0x0465, 0x044d,
				0x0436, 0x0420, 0x040b, 0x03f7, 0x03e3, 0x03d1, 0x03be, 0x03ad,
				0x039c, 0x038c, 0x037c, 0x036d, 0x035f, 0x0350, 0x0343, 0x0335,
				0x0329, 0x0310, 0x02f9, 0x02e3, 0x02cf, 0x02bb, 0x02a9, 0x0297,
				0x0287, 0x0277, 0x0268, 0x025a, 0x024c, 0x023f, 0x0232, 0x0226,
				0x021b, 0x0210, 0x0205, 0x01fb, 0x01f1, 0x01e8, 0x01df, 0x01d6,
				0x01ce, 0x01c6, 0x01be, 0x01b6, 0x01af, 0x01a8, 0x01a1, 0x019a,
				0x0194, 0x0188, 0x017c, 0x0171, 0x0167, 0x015d, 0x0154, 0x014b,
				0x0143, 0x013b, 0x0134, 0x012d, 0x0126, 0x011f, 0x0119, 0x0113,
				0x010d, 0x0108, 0x0102, 0x00fd, 0x00f8, 0x00f4, 0x00ef, 0x00eb,
				0x00e7, 0x00e3, 0x00df, 0x00db, 0x00d7, 0x00d4, 0x00d0, 0x00cd,
			},
		},
	},
};

static inline void ctx_blk_write(uint32_t field, __u32 *ptr, __u32 *dest)
{
	__u32 woffset, wcount;

	woffset = ISP_GET_OFF(field);
	wcount = ISP_GET_WSZ(field);
	if (IS_ERR_OR_NULL(ptr) || IS_ERR_OR_NULL(dest)) {
		pr_err("Invalid pointer for memcpy block !");
		return;
	}
	memcpy(&dest[woffset], ptr, wcount * sizeof(__u32));
}

static void neoisp_set_hdr_decompress0(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_CTRL_CAM0_IDX],
			NEO_CTRL_CAM0_ENABLE_SET(p->decompress_dcg.ctrl_enable));
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_POINT1_CAM0_IDX],
			p->decompress_dcg.knee_point1);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_POINT2_CAM0_IDX],
			p->decompress_dcg.knee_point2);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_POINT3_CAM0_IDX],
			p->decompress_dcg.knee_point3);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_POINT4_CAM0_IDX],
			p->decompress_dcg.knee_point4);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_OFFSET0_CAM0_IDX],
			p->decompress_dcg.knee_offset0);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_OFFSET1_CAM0_IDX],
			p->decompress_dcg.knee_offset1);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_OFFSET2_CAM0_IDX],
			p->decompress_dcg.knee_offset2);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_OFFSET3_CAM0_IDX],
			p->decompress_dcg.knee_offset3);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_OFFSET4_CAM0_IDX],
			p->decompress_dcg.knee_offset4);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_RATIO01_CAM0_IDX],
			NEO_HDR_DECOMPRESS0_KNEE_RATIO01_CAM0_RATIO0_SET(p->decompress_dcg.knee_ratio0)
			| NEO_HDR_DECOMPRESS0_KNEE_RATIO01_CAM0_RATIO1_SET(p->decompress_dcg.knee_ratio1));
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_RATIO23_CAM0_IDX],
			NEO_HDR_DECOMPRESS0_KNEE_RATIO23_CAM0_RATIO2_SET(p->decompress_dcg.knee_ratio2)
			| NEO_HDR_DECOMPRESS0_KNEE_RATIO23_CAM0_RATIO3_SET(p->decompress_dcg.knee_ratio3));
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_RATIO4_CAM0_IDX],
			p->decompress_dcg.knee_ratio4);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_NPOINT0_CAM0_IDX],
			p->decompress_dcg.knee_npoint0);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_NPOINT1_CAM0_IDX],
			p->decompress_dcg.knee_npoint1);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_NPOINT2_CAM0_IDX],
			p->decompress_dcg.knee_npoint2);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_NPOINT3_CAM0_IDX],
			p->decompress_dcg.knee_npoint3);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS0_KNEE_NPOINT4_CAM0_IDX],
			p->decompress_dcg.knee_npoint4);
}

static void neoisp_set_hdr_decompress1(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_CTRL_CAM0_IDX],
			NEO_CTRL_CAM0_ENABLE_SET(p->decompress_vs.ctrl_enable));
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_POINT1_CAM0_IDX],
			p->decompress_vs.knee_point1);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_POINT2_CAM0_IDX],
			p->decompress_vs.knee_point2);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_POINT3_CAM0_IDX],
			p->decompress_vs.knee_point3);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_POINT4_CAM0_IDX],
			p->decompress_vs.knee_point4);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_OFFSET0_CAM0_IDX],
			p->decompress_vs.knee_offset0);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_OFFSET1_CAM0_IDX],
			p->decompress_vs.knee_offset1);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_OFFSET2_CAM0_IDX],
			p->decompress_vs.knee_offset2);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_OFFSET3_CAM0_IDX],
			p->decompress_vs.knee_offset3);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_OFFSET4_CAM0_IDX],
			p->decompress_vs.knee_offset4);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_RATIO01_CAM0_IDX],
			NEO_HDR_DECOMPRESS1_KNEE_RATIO01_CAM0_RATIO0_SET(p->decompress_vs.knee_ratio0)
			| NEO_HDR_DECOMPRESS1_KNEE_RATIO01_CAM0_RATIO1_SET(p->decompress_vs.knee_ratio1));
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_RATIO23_CAM0_IDX],
			NEO_HDR_DECOMPRESS1_KNEE_RATIO23_CAM0_RATIO2_SET(p->decompress_vs.knee_ratio2)
			| NEO_HDR_DECOMPRESS1_KNEE_RATIO23_CAM0_RATIO3_SET(p->decompress_vs.knee_ratio3));
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_RATIO4_CAM0_IDX],
			p->decompress_vs.knee_ratio4);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_NPOINT0_CAM0_IDX],
			p->decompress_vs.knee_npoint0);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_NPOINT1_CAM0_IDX],
			p->decompress_vs.knee_npoint1);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_NPOINT2_CAM0_IDX],
			p->decompress_vs.knee_npoint2);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_NPOINT3_CAM0_IDX],
			p->decompress_vs.knee_npoint3);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_DECOMPRESS1_KNEE_NPOINT4_CAM0_IDX],
			p->decompress_vs.knee_npoint4);
}

static void neoisp_set_ob_wb0(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB0_CTRL_CAM0_IDX],
			NEO_OB_WB0_CTRL_CAM0_OBPP_SET(p->obwb[0].ctrl_obpp));
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB0_R_CTRL_CAM0_IDX],
			NEO_OB_WB0_R_CTRL_CAM0_OFFSET_SET(p->obwb[0].r_ctrl_offset)
			| NEO_OB_WB0_R_CTRL_CAM0_GAIN_SET(p->obwb[0].r_ctrl_gain));
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB0_GR_CTRL_CAM0_IDX],
			NEO_OB_WB0_GR_CTRL_CAM0_OFFSET_SET(p->obwb[0].gr_ctrl_offset)
			| NEO_OB_WB0_GR_CTRL_CAM0_GAIN_SET(p->obwb[0].gr_ctrl_gain));
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB0_GB_CTRL_CAM0_IDX],
			NEO_OB_WB0_GB_CTRL_CAM0_OFFSET_SET(p->obwb[0].gb_ctrl_offset)
			| NEO_OB_WB0_GB_CTRL_CAM0_GAIN_SET(p->obwb[0].gb_ctrl_gain));
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB0_B_CTRL_CAM0_IDX],
			NEO_OB_WB0_B_CTRL_CAM0_OFFSET_SET(p->obwb[0].b_ctrl_offset)
			| NEO_OB_WB0_B_CTRL_CAM0_GAIN_SET(p->obwb[0].b_ctrl_gain));
}

static void neoisp_set_ob_wb1(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB1_CTRL_CAM0_IDX],
			NEO_OB_WB1_CTRL_CAM0_OBPP_SET(p->obwb[1].ctrl_obpp));
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB1_R_CTRL_CAM0_IDX],
			NEO_OB_WB1_R_CTRL_CAM0_OFFSET_SET(p->obwb[1].r_ctrl_offset)
			| NEO_OB_WB1_R_CTRL_CAM0_GAIN_SET(p->obwb[1].r_ctrl_gain));
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB1_GR_CTRL_CAM0_IDX],
			NEO_OB_WB1_GR_CTRL_CAM0_OFFSET_SET(p->obwb[1].gr_ctrl_offset)
			| NEO_OB_WB1_GR_CTRL_CAM0_GAIN_SET(p->obwb[1].gr_ctrl_gain));
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB1_GB_CTRL_CAM0_IDX],
			NEO_OB_WB1_GB_CTRL_CAM0_OFFSET_SET(p->obwb[1].gb_ctrl_offset)
			| NEO_OB_WB1_GB_CTRL_CAM0_GAIN_SET(p->obwb[1].gb_ctrl_gain));
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB1_B_CTRL_CAM0_IDX],
			NEO_OB_WB1_B_CTRL_CAM0_OFFSET_SET(p->obwb[1].b_ctrl_offset)
			| NEO_OB_WB1_B_CTRL_CAM0_GAIN_SET(p->obwb[1].b_ctrl_gain));
}

static void neoisp_set_ob_wb2(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB2_CTRL_CAM0_IDX],
			NEO_OB_WB2_CTRL_CAM0_OBPP_SET(p->obwb[2].ctrl_obpp));
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB2_R_CTRL_CAM0_IDX],
			NEO_OB_WB2_R_CTRL_CAM0_OFFSET_SET(p->obwb[2].r_ctrl_offset)
			| NEO_OB_WB2_R_CTRL_CAM0_GAIN_SET(p->obwb[2].r_ctrl_gain));
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB2_GR_CTRL_CAM0_IDX],
			NEO_OB_WB2_GR_CTRL_CAM0_OFFSET_SET(p->obwb[2].gr_ctrl_offset)
			| NEO_OB_WB2_GR_CTRL_CAM0_GAIN_SET(p->obwb[2].gr_ctrl_gain));
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB2_GB_CTRL_CAM0_IDX],
			NEO_OB_WB2_GB_CTRL_CAM0_OFFSET_SET(p->obwb[2].gb_ctrl_offset)
			| NEO_OB_WB2_GB_CTRL_CAM0_GAIN_SET(p->obwb[2].gb_ctrl_gain));
	regmap_field_write(neoispd->regs.fields[NEO_OB_WB2_B_CTRL_CAM0_IDX],
			NEO_OB_WB2_B_CTRL_CAM0_OFFSET_SET(p->obwb[2].b_ctrl_offset)
			| NEO_OB_WB2_B_CTRL_CAM0_GAIN_SET(p->obwb[2].b_ctrl_gain));
}

static void neoisp_set_hdr_merge(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_HDR_MERGE_CTRL_CAM0_IDX],
			NEO_HDR_MERGE_CTRL_CAM0_ENABLE_SET(p->hdr_merge.ctrl_enable)
			| NEO_HDR_MERGE_CTRL_CAM0_MOTION_FIX_EN_SET(p->hdr_merge.ctrl_motion_fix_en)
			| NEO_HDR_MERGE_CTRL_CAM0_BLEND_3X3_SET(p->hdr_merge.ctrl_blend_3x3)
			| NEO_HDR_MERGE_CTRL_CAM0_GAIN1BPP_SET(p->hdr_merge.ctrl_gain1bpp)
			| NEO_HDR_MERGE_CTRL_CAM0_GAIN0BPP_SET(p->hdr_merge.ctrl_gain0bpp)
			| NEO_HDR_MERGE_CTRL_CAM0_OBPP_SET(p->hdr_merge.ctrl_obpp));
	regmap_field_write(neoispd->regs.fields[NEO_HDR_MERGE_GAIN_OFFSET_CAM0_IDX],
			NEO_HDR_MERGE_GAIN_OFFSET_CAM0_OFFSET1_SET(p->hdr_merge.gain_offset_offset1)
			| NEO_HDR_MERGE_GAIN_OFFSET_CAM0_OFFSET0_SET(p->hdr_merge.gain_offset_offset0));
	regmap_field_write(neoispd->regs.fields[NEO_HDR_MERGE_GAIN_SCALE_CAM0_IDX],
			NEO_HDR_MERGE_GAIN_SCALE_CAM0_SCALE1_SET(p->hdr_merge.gain_scale_scale1)
			| NEO_HDR_MERGE_GAIN_SCALE_CAM0_SCALE0_SET(p->hdr_merge.gain_scale_scale0));
	regmap_field_write(neoispd->regs.fields[NEO_HDR_MERGE_GAIN_SHIFT_CAM0_IDX],
			NEO_HDR_MERGE_GAIN_SHIFT_CAM0_SHIFT1_SET(p->hdr_merge.gain_shift_shift1)
			| NEO_HDR_MERGE_GAIN_SHIFT_CAM0_SHIFT0_SET(p->hdr_merge.gain_shift_shift0));
	regmap_field_write(neoispd->regs.fields[NEO_HDR_MERGE_LUMA_TH_CAM0_IDX], p->hdr_merge.luma_th_th0);
	regmap_field_write(neoispd->regs.fields[NEO_HDR_MERGE_LUMA_SCALE_CAM0_IDX],
			NEO_HDR_MERGE_LUMA_SCALE_CAM0_SCALE_SET(p->hdr_merge.luma_scale_scale)
			| NEO_HDR_MERGE_LUMA_SCALE_CAM0_SHIFT_SET(p->hdr_merge.luma_scale_shift)
			| NEO_HDR_MERGE_LUMA_SCALE_CAM0_THSHIFT_SET(p->hdr_merge.luma_scale_thshift));
	regmap_field_write(neoispd->regs.fields[NEO_HDR_MERGE_DOWNSCALE_CAM0_IDX],
			NEO_HDR_MERGE_DOWNSCALE_CAM0_IMGSCALE1_SET(p->hdr_merge.downscale_imgscale1)
			| NEO_HDR_MERGE_DOWNSCALE_CAM0_IMGSCALE0_SET(p->hdr_merge.downscale_imgscale0));
	regmap_field_write(neoispd->regs.fields[NEO_HDR_MERGE_UPSCALE_CAM0_IDX],
			NEO_HDR_MERGE_UPSCALE_CAM0_IMGSCALE1_SET(p->hdr_merge.upscale_imgscale1)
			| NEO_HDR_MERGE_UPSCALE_CAM0_IMGSCALE0_SET(p->hdr_merge.upscale_imgscale0));
	regmap_field_write(neoispd->regs.fields[NEO_HDR_MERGE_POST_SCALE_CAM0_IDX], p->hdr_merge.post_scale_scale);
}

static void neoisp_set_rgbir(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_CTRL_CAM0_IDX],
			NEO_RGBIR_CTRL_CAM0_ENABLE_SET(p->rgbir.ctrl_enable));
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_CCM0_CAM0_IDX], p->rgbir.ccm0_ccm);
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_CCM1_CAM0_IDX], p->rgbir.ccm1_ccm);
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_CCM2_CAM0_IDX], p->rgbir.ccm2_ccm);
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_CCM0_TH_CAM0_IDX],
			p->rgbir.ccm0_th_threshold);
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_CCM1_TH_CAM0_IDX],
			p->rgbir.ccm1_th_threshold);
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_CCM2_TH_CAM0_IDX],
			p->rgbir.ccm2_th_threshold);
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_ROI0_POS_CAM0_IDX],
			NEO_RGBIR_ROI0_POS_CAM0_XPOS_SET(p->rgbir.roi[0].xpos)
			| NEO_RGBIR_ROI0_POS_CAM0_YPOS_SET(p->rgbir.roi[0].ypos));
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_ROI0_SIZE_CAM0_IDX],
			NEO_RGBIR_ROI0_SIZE_CAM0_WIDTH_SET(p->rgbir.roi[0].width)
			| NEO_RGBIR_ROI0_SIZE_CAM0_HEIGHT_SET(p->rgbir.roi[0].height));
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_ROI1_POS_CAM0_IDX],
			NEO_RGBIR_ROI1_POS_CAM0_XPOS_SET(p->rgbir.roi[1].xpos)
			| NEO_RGBIR_ROI1_POS_CAM0_YPOS_SET(p->rgbir.roi[1].ypos));
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_ROI1_SIZE_CAM0_IDX],
			NEO_RGBIR_ROI1_SIZE_CAM0_WIDTH_SET(p->rgbir.roi[1].width)
			| NEO_RGBIR_ROI1_SIZE_CAM0_HEIGHT_SET(p->rgbir.roi[1].height));
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_HIST0_CTRL_CAM0_IDX],
			NEO_HIST_CTRL_CAM0_OFFSET_SET(p->rgbir.hists[0].hist_ctrl_offset)
			| NEO_HIST_CTRL_CAM0_CHANNEL_SET(p->rgbir.hists[0].hist_ctrl_channel)
			| NEO_HIST_CTRL_CAM0_PATTERN_SET(p->rgbir.hists[0].hist_ctrl_pattern)
			| NEO_HIST_CTRL_CAM0_DIR_VS_DIF_SET(p->rgbir.hists[0].hist_ctrl_dir_vs_dif)
			| NEO_HIST_CTRL_CAM0_LIN_VS_LOG_SET(p->rgbir.hists[0].hist_ctrl_lin_vs_log));
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_HIST0_SCALE_CAM0_IDX],
			p->rgbir.hists[0].hist_scale_scale);
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_HIST1_CTRL_CAM0_IDX],
			NEO_HIST_CTRL_CAM0_OFFSET_SET(p->rgbir.hists[1].hist_ctrl_offset)
			| NEO_HIST_CTRL_CAM0_CHANNEL_SET(p->rgbir.hists[1].hist_ctrl_channel)
			| NEO_HIST_CTRL_CAM0_PATTERN_SET(p->rgbir.hists[1].hist_ctrl_pattern)
			| NEO_HIST_CTRL_CAM0_DIR_VS_DIF_SET(p->rgbir.hists[1].hist_ctrl_dir_vs_dif)
			| NEO_HIST_CTRL_CAM0_LIN_VS_LOG_SET(p->rgbir.hists[1].hist_ctrl_lin_vs_log));
	regmap_field_write(neoispd->regs.fields[NEO_RGBIR_HIST1_SCALE_CAM0_IDX],
			p->rgbir.hists[1].hist_scale_scale);
}

static void neoisp_set_stat_hists(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_STAT_ROI0_POS_CAM0_IDX],
			NEO_STAT_ROI0_POS_CAM0_XPOS_SET(p->stat.roi0.xpos)
			| NEO_STAT_ROI0_POS_CAM0_YPOS_SET(p->stat.roi0.ypos));
	regmap_field_write(neoispd->regs.fields[NEO_STAT_ROI0_SIZE_CAM0_IDX],
			NEO_STAT_ROI0_SIZE_CAM0_WIDTH_SET(p->stat.roi0.width)
			| NEO_STAT_ROI0_SIZE_CAM0_HEIGHT_SET(p->stat.roi0.height));
	regmap_field_write(neoispd->regs.fields[NEO_STAT_ROI1_POS_CAM0_IDX],
			NEO_STAT_ROI0_POS_CAM0_XPOS_SET(p->stat.roi1.xpos)
			| NEO_STAT_ROI0_POS_CAM0_YPOS_SET(p->stat.roi1.ypos));
	regmap_field_write(neoispd->regs.fields[NEO_STAT_ROI1_SIZE_CAM0_IDX],
			NEO_STAT_ROI0_SIZE_CAM0_WIDTH_SET(p->stat.roi1.width)
			| NEO_STAT_ROI0_SIZE_CAM0_HEIGHT_SET(p->stat.roi1.height));

	regmap_field_write(neoispd->regs.fields[NEO_STAT_HIST0_CTRL_CAM0_IDX],
			NEO_HIST_CTRL_CAM0_OFFSET_SET(p->stat.hists[0].hist_ctrl_offset)
			| NEO_HIST_CTRL_CAM0_CHANNEL_SET(p->stat.hists[0].hist_ctrl_channel)
			| NEO_HIST_CTRL_CAM0_PATTERN_SET(p->stat.hists[0].hist_ctrl_pattern)
			| NEO_HIST_CTRL_CAM0_DIR_VS_DIF_SET(p->stat.hists[0].hist_ctrl_dir_vs_dif)
			| NEO_HIST_CTRL_CAM0_LIN_VS_LOG_SET(p->stat.hists[0].hist_ctrl_lin_vs_log));
	regmap_field_write(neoispd->regs.fields[NEO_STAT_HIST0_SCALE_CAM0_IDX],
			p->stat.hists[0].hist_scale_scale);
	regmap_field_write(neoispd->regs.fields[NEO_STAT_HIST1_CTRL_CAM0_IDX],
			NEO_HIST_CTRL_CAM0_OFFSET_SET(p->stat.hists[1].hist_ctrl_offset)
			| NEO_HIST_CTRL_CAM0_CHANNEL_SET(p->stat.hists[1].hist_ctrl_channel)
			| NEO_HIST_CTRL_CAM0_PATTERN_SET(p->stat.hists[1].hist_ctrl_pattern)
			| NEO_HIST_CTRL_CAM0_DIR_VS_DIF_SET(p->stat.hists[1].hist_ctrl_dir_vs_dif)
			| NEO_HIST_CTRL_CAM0_LIN_VS_LOG_SET(p->stat.hists[1].hist_ctrl_lin_vs_log));
	regmap_field_write(neoispd->regs.fields[NEO_STAT_HIST1_SCALE_CAM0_IDX],
			p->stat.hists[1].hist_scale_scale);
	regmap_field_write(neoispd->regs.fields[NEO_STAT_HIST2_CTRL_CAM0_IDX],
			NEO_HIST_CTRL_CAM0_OFFSET_SET(p->stat.hists[2].hist_ctrl_offset)
			| NEO_HIST_CTRL_CAM0_CHANNEL_SET(p->stat.hists[2].hist_ctrl_channel)
			| NEO_HIST_CTRL_CAM0_PATTERN_SET(p->stat.hists[2].hist_ctrl_pattern)
			| NEO_HIST_CTRL_CAM0_DIR_VS_DIF_SET(p->stat.hists[2].hist_ctrl_dir_vs_dif)
			| NEO_HIST_CTRL_CAM0_LIN_VS_LOG_SET(p->stat.hists[2].hist_ctrl_lin_vs_log));
	regmap_field_write(neoispd->regs.fields[NEO_STAT_HIST2_SCALE_CAM0_IDX],
			p->stat.hists[2].hist_scale_scale);
	regmap_field_write(neoispd->regs.fields[NEO_STAT_HIST3_CTRL_CAM0_IDX],
			NEO_HIST_CTRL_CAM0_OFFSET_SET(p->stat.hists[3].hist_ctrl_offset)
			| NEO_HIST_CTRL_CAM0_CHANNEL_SET(p->stat.hists[3].hist_ctrl_channel)
			| NEO_HIST_CTRL_CAM0_PATTERN_SET(p->stat.hists[3].hist_ctrl_pattern)
			| NEO_HIST_CTRL_CAM0_DIR_VS_DIF_SET(p->stat.hists[3].hist_ctrl_dir_vs_dif)
			| NEO_HIST_CTRL_CAM0_LIN_VS_LOG_SET(p->stat.hists[3].hist_ctrl_lin_vs_log));
	regmap_field_write(neoispd->regs.fields[NEO_STAT_HIST3_SCALE_CAM0_IDX],
			p->stat.hists[3].hist_scale_scale);
}

static void neoisp_set_ir_compress(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_CTRL_CAM0_IDX],
			NEO_IR_COMPRESS_CTRL_CAM0_ENABLE_SET(p->ir_compress.ctrl_enable)
			| NEO_IR_COMPRESS_CTRL_CAM0_OBPP_SET(p->ir_compress.ctrl_obpp));
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_POINT1_CAM0_IDX],
			p->ir_compress.knee_point1_kneepoint);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_POINT2_CAM0_IDX],
			p->ir_compress.knee_point2_kneepoint);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_POINT3_CAM0_IDX],
			p->ir_compress.knee_point3_kneepoint);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_POINT4_CAM0_IDX],
			p->ir_compress.knee_point4_kneepoint);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_OFFSET0_CAM0_IDX],
			p->ir_compress.knee_offset0_offset);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_OFFSET1_CAM0_IDX],
			p->ir_compress.knee_offset1_offset);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_OFFSET2_CAM0_IDX],
			p->ir_compress.knee_offset2_offset);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_OFFSET3_CAM0_IDX],
			p->ir_compress.knee_offset3_offset);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_OFFSET4_CAM0_IDX],
			p->ir_compress.knee_offset4_offset);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_RATIO01_CAM0_IDX],
			NEO_IR_COMPRESS_KNEE_RATIO01_CAM0_RATIO0_SET(p->ir_compress.knee_ratio01_ratio0)
			| NEO_IR_COMPRESS_KNEE_RATIO01_CAM0_RATIO1_SET(p->ir_compress.knee_ratio01_ratio1));
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_RATIO23_CAM0_IDX],
			NEO_IR_COMPRESS_KNEE_RATIO23_CAM0_RATIO2_SET(p->ir_compress.knee_ratio23_ratio2)
			| NEO_IR_COMPRESS_KNEE_RATIO23_CAM0_RATIO3_SET(p->ir_compress.knee_ratio23_ratio3));
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_RATIO4_CAM0_IDX],
			p->ir_compress.knee_ratio4_ratio4);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_NPOINT0_CAM0_IDX],
			p->ir_compress.knee_npoint0_kneepoint);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_NPOINT1_CAM0_IDX],
			p->ir_compress.knee_npoint1_kneepoint);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_NPOINT2_CAM0_IDX],
			p->ir_compress.knee_npoint2_kneepoint);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_NPOINT3_CAM0_IDX],
			p->ir_compress.knee_npoint3_kneepoint);
	regmap_field_write(neoispd->regs.fields[NEO_IR_COMPRESS_KNEE_NPOINT4_CAM0_IDX],
			p->ir_compress.knee_npoint4_kneepoint);
}

static void neoisp_set_color_temp(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CTRL_CAM0_IDX],
			NEO_COLOR_TEMP_CTRL_CAM0_IBPP_SET(p->ctemp.ctrl_ibpp)
			| NEO_COLOR_TEMP_CTRL_CAM0_CSCON_SET(p->ctemp.ctrl_cscon)
			| NEO_COLOR_TEMP_CTRL_CAM0_ENABLE_SET(p->ctemp.ctrl_enable));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_ROI_POS_CAM0_IDX],
			NEO_COLOR_TEMP_ROI_POS_CAM0_XPOS_SET(p->ctemp.roi.xpos)
			| NEO_COLOR_TEMP_ROI_POS_CAM0_YPOS_SET(p->ctemp.roi.ypos));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_ROI_SIZE_CAM0_IDX],
			NEO_COLOR_TEMP_ROI_SIZE_CAM0_WIDTH_SET(p->ctemp.roi.width)
			| NEO_COLOR_TEMP_ROI_SIZE_CAM0_HEIGHT_SET(p->ctemp.roi.height));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_REDGAIN_CAM0_IDX],
			NEO_COLOR_TEMP_REDGAIN_CAM0_MIN_SET(p->ctemp.redgain_min)
			| NEO_COLOR_TEMP_REDGAIN_CAM0_MAX_SET(p->ctemp.redgain_max));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_BLUEGAIN_CAM0_IDX],
			NEO_COLOR_TEMP_BLUEGAIN_CAM0_MIN_SET(p->ctemp.bluegain_min)
			| NEO_COLOR_TEMP_BLUEGAIN_CAM0_MAX_SET(p->ctemp.bluegain_max));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_POINT1_CAM0_IDX],
			NEO_COLOR_TEMP_POINT1_CAM0_BLUE_SET(p->ctemp.point1_blue)
			| NEO_COLOR_TEMP_POINT1_CAM0_RED_SET(p->ctemp.point1_red));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_POINT2_CAM0_IDX],
			NEO_COLOR_TEMP_POINT2_CAM0_BLUE_SET(p->ctemp.point2_blue)
			| NEO_COLOR_TEMP_POINT2_CAM0_RED_SET(p->ctemp.point2_red));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_HOFFSET_CAM0_IDX],
			NEO_COLOR_TEMP_HOFFSET_CAM0_RIGHT_SET(p->ctemp.hoffset_right)
			| NEO_COLOR_TEMP_HOFFSET_CAM0_LEFT_SET(p->ctemp.hoffset_left));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_VOFFSET_CAM0_IDX],
			NEO_COLOR_TEMP_VOFFSET_CAM0_UP_SET(p->ctemp.voffset_up)
			| NEO_COLOR_TEMP_VOFFSET_CAM0_DOWN_SET(p->ctemp.voffset_down));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_POINT1_SLOPE_CAM0_IDX],
			NEO_COLOR_TEMP_POINT1_SLOPE_CAM0_SLOPE_L_SET(p->ctemp.point1_slope_slope_l)
			| NEO_COLOR_TEMP_POINT1_SLOPE_CAM0_SLOPE_R_SET(p->ctemp.point1_slope_slope_r));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_POINT2_SLOPE_CAM0_IDX],
			NEO_COLOR_TEMP_POINT2_SLOPE_CAM0_SLOPE_L_SET(p->ctemp.point2_slope_slope_l)
			| NEO_COLOR_TEMP_POINT2_SLOPE_CAM0_SLOPE_R_SET(p->ctemp.point2_slope_slope_r));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_LUMA_TH_CAM0_IDX],
			NEO_COLOR_TEMP_LUMA_TH_CAM0_THL_SET(p->ctemp.luma_th_thl)
			| NEO_COLOR_TEMP_LUMA_TH_CAM0_THH_SET(p->ctemp.luma_th_thh));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CSC_MAT0_CAM0_IDX],
			NEO_COLOR_TEMP_CSC_MAT0_CAM0_R0C0_SET(p->ctemp.csc_matrix[0][0])
			| NEO_COLOR_TEMP_CSC_MAT0_CAM0_R0C1_SET(p->ctemp.csc_matrix[0][1]));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CSC_MAT1_CAM0_IDX],
			NEO_COLOR_TEMP_CSC_MAT0_CAM0_R0C0_SET(p->ctemp.csc_matrix[0][2])
			| NEO_COLOR_TEMP_CSC_MAT0_CAM0_R0C1_SET(p->ctemp.csc_matrix[1][0]));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CSC_MAT2_CAM0_IDX],
			NEO_COLOR_TEMP_CSC_MAT0_CAM0_R0C0_SET(p->ctemp.csc_matrix[1][1])
			| NEO_COLOR_TEMP_CSC_MAT0_CAM0_R0C1_SET(p->ctemp.csc_matrix[1][2]));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CSC_MAT3_CAM0_IDX],
			NEO_COLOR_TEMP_CSC_MAT0_CAM0_R0C0_SET(p->ctemp.csc_matrix[2][0])
			| NEO_COLOR_TEMP_CSC_MAT0_CAM0_R0C1_SET(p->ctemp.csc_matrix[2][1]));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CSC_MAT4_CAM0_IDX], p->ctemp.csc_matrix[2][2]);
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_R_GR_OFFSET_CAM0_IDX],
			NEO_COLOR_TEMP_R_GR_OFFSET_CAM0_OFFSET0_SET(p->ctemp.offsets[0])
			| NEO_COLOR_TEMP_R_GR_OFFSET_CAM0_OFFSET1_SET(p->ctemp.offsets[1]));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_GB_B_OFFSET_CAM0_IDX],
			NEO_COLOR_TEMP_GB_B_OFFSET_CAM0_OFFSET0_SET(p->ctemp.offsets[2])
			| NEO_COLOR_TEMP_GB_B_OFFSET_CAM0_OFFSET1_SET(p->ctemp.offsets[3]));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_STAT_BLK_SIZE0_IDX],
			NEO_COLOR_TEMP_STAT_BLK_SIZE0_XSIZE_SET(p->ctemp.stat_blk_size0_xsize)
			| NEO_COLOR_TEMP_STAT_BLK_SIZE0_YSIZE_SET(p->ctemp.stat_blk_size0_ysize));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CROI0_POS_CAM0_IDX],
			NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_LOW_SET(p->ctemp.color_rois[0].pos_roverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_HIGH_SET(p->ctemp.color_rois[0].pos_roverg_high)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_LOW_SET(p->ctemp.color_rois[0].pos_boverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_HIGH_SET(p->ctemp.color_rois[0].pos_boverg_high));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CROI1_POS_CAM0_IDX],
			NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_LOW_SET(p->ctemp.color_rois[1].pos_roverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_HIGH_SET(p->ctemp.color_rois[1].pos_roverg_high)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_LOW_SET(p->ctemp.color_rois[1].pos_boverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_HIGH_SET(p->ctemp.color_rois[1].pos_boverg_high));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CROI2_POS_CAM0_IDX],
			NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_LOW_SET(p->ctemp.color_rois[2].pos_roverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_HIGH_SET(p->ctemp.color_rois[2].pos_roverg_high)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_LOW_SET(p->ctemp.color_rois[2].pos_boverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_HIGH_SET(p->ctemp.color_rois[2].pos_boverg_high));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CROI3_POS_CAM0_IDX],
			NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_LOW_SET(p->ctemp.color_rois[3].pos_roverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_HIGH_SET(p->ctemp.color_rois[3].pos_roverg_high)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_LOW_SET(p->ctemp.color_rois[3].pos_boverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_HIGH_SET(p->ctemp.color_rois[3].pos_boverg_high));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CROI4_POS_CAM0_IDX],
			NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_LOW_SET(p->ctemp.color_rois[4].pos_roverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_HIGH_SET(p->ctemp.color_rois[4].pos_roverg_high)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_LOW_SET(p->ctemp.color_rois[4].pos_boverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_HIGH_SET(p->ctemp.color_rois[4].pos_boverg_high));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CROI5_POS_CAM0_IDX],
			NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_LOW_SET(p->ctemp.color_rois[5].pos_roverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_HIGH_SET(p->ctemp.color_rois[5].pos_roverg_high)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_LOW_SET(p->ctemp.color_rois[5].pos_boverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_HIGH_SET(p->ctemp.color_rois[5].pos_boverg_high));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CROI6_POS_CAM0_IDX],
			NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_LOW_SET(p->ctemp.color_rois[6].pos_roverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_HIGH_SET(p->ctemp.color_rois[6].pos_roverg_high)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_LOW_SET(p->ctemp.color_rois[6].pos_boverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_HIGH_SET(p->ctemp.color_rois[6].pos_boverg_high));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CROI7_POS_CAM0_IDX],
			NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_LOW_SET(p->ctemp.color_rois[7].pos_roverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_HIGH_SET(p->ctemp.color_rois[7].pos_roverg_high)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_LOW_SET(p->ctemp.color_rois[7].pos_boverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_HIGH_SET(p->ctemp.color_rois[7].pos_boverg_high));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CROI8_POS_CAM0_IDX],
			NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_LOW_SET(p->ctemp.color_rois[8].pos_roverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_HIGH_SET(p->ctemp.color_rois[8].pos_roverg_high)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_LOW_SET(p->ctemp.color_rois[8].pos_boverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_HIGH_SET(p->ctemp.color_rois[8].pos_boverg_high));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_CROI9_POS_CAM0_IDX],
			NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_LOW_SET(p->ctemp.color_rois[9].pos_roverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_ROVERG_HIGH_SET(p->ctemp.color_rois[9].pos_roverg_high)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_LOW_SET(p->ctemp.color_rois[9].pos_boverg_low)
			| NEO_COLOR_TEMP_CROI0_POS_CAM0_BOVERG_HIGH_SET(p->ctemp.color_rois[9].pos_boverg_high));
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_GR_AVG_IN_CAM0_IDX], p->ctemp.gr_avg_in_gr_agv);
	regmap_field_write(neoispd->regs.fields[NEO_COLOR_TEMP_GB_AVG_IN_CAM0_IDX], p->ctemp.gb_avg_in_gb_agv);
}

static void neoisp_set_bnr(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_BNR_CTRL_CAM0_IDX],
			NEO_BNR_CTRL_CAM0_OBPP_SET(p->bnr.ctrl_obpp)
			| NEO_BNR_CTRL_CAM0_DEBUG_SET(p->bnr.ctrl_debug)
			| NEO_BNR_CTRL_CAM0_NHOOD_SET(p->bnr.ctrl_nhood)
			| NEO_BNR_CTRL_CAM0_ENABLE_SET(p->bnr.ctrl_enable));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_YPEAK_CAM0_IDX],
			NEO_BNR_YPEAK_CAM0_PEAK_LOW_SET(p->bnr.ypeak_peak_low)
			| NEO_BNR_YPEAK_CAM0_PEAK_SEL_SET(p->bnr.ypeak_peak_sel)
			| NEO_BNR_YPEAK_CAM0_PEAK_HIGH_SET(p->bnr.ypeak_peak_high)
			| NEO_BNR_YPEAK_CAM0_PEAK_OUTSEL_SET(p->bnr.ypeak_peak_outsel));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_YEDGE_TH0_CAM0_IDX], p->bnr.yedge_th0_edge_th0);
	regmap_field_write(neoispd->regs.fields[NEO_BNR_YEDGE_SCALE_CAM0_IDX],
			NEO_BNR_YEDGE_SCALE_CAM0_SCALE_SET(p->bnr.yedge_scale_scale)
			| NEO_BNR_YEDGE_SCALE_CAM0_SHIFT_SET(p->bnr.yedge_scale_shift));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_YEDGES_TH0_CAM0_IDX], p->bnr.yedges_th0_edge_th0);
	regmap_field_write(neoispd->regs.fields[NEO_BNR_YEDGES_SCALE_CAM0_IDX],
			NEO_BNR_YEDGES_SCALE_CAM0_SCALE_SET(p->bnr.yedges_scale_scale)
			| NEO_BNR_YEDGES_SCALE_CAM0_SHIFT_SET(p->bnr.yedges_scale_shift));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_YEDGEA_TH0_CAM0_IDX], p->bnr.yedgea_th0_edge_th0);
	regmap_field_write(neoispd->regs.fields[NEO_BNR_YEDGEA_SCALE_CAM0_IDX],
			NEO_BNR_YEDGEA_SCALE_CAM0_SCALE_SET(p->bnr.yedgea_scale_scale)
			| NEO_BNR_YEDGEA_SCALE_CAM0_SHIFT_SET(p->bnr.yedgea_scale_shift));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_YLUMA_X_TH0_CAM0_IDX], p->bnr.yluma_x_th0_th);
	regmap_field_write(neoispd->regs.fields[NEO_BNR_YLUMA_Y_TH_CAM0_IDX],
			NEO_BNR_YLUMA_Y_TH_CAM0_LUMA_Y_TH0_SET(p->bnr.yluma_y_th_luma_y_th0)
			| NEO_BNR_YLUMA_Y_TH_CAM0_LUMA_Y_TH1_SET(p->bnr.yluma_y_th_luma_y_th1));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_YLUMA_SCALE_CAM0_IDX],
			NEO_BNR_YLUMA_SCALE_CAM0_SCALE_SET(p->bnr.yluma_scale_scale)
			| NEO_BNR_YLUMA_SCALE_CAM0_SHIFT_SET(p->bnr.yluma_scale_shift));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_YALPHA_GAIN_CAM0_IDX],
			NEO_BNR_YALPHA_GAIN_CAM0_GAIN_SET(p->bnr.yalpha_gain_gain)
			| NEO_BNR_YALPHA_GAIN_CAM0_OFFSET_SET(p->bnr.yalpha_gain_offset));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_CPEAK_CAM0_IDX],
			NEO_BNR_CPEAK_CAM0_PEAK_LOW_SET(p->bnr.cpeak_peak_low)
			| NEO_BNR_CPEAK_CAM0_PEAK_SEL_SET(p->bnr.cpeak_peak_sel)
			| NEO_BNR_CPEAK_CAM0_PEAK_HIGH_SET(p->bnr.cpeak_peak_high)
			| NEO_BNR_CPEAK_CAM0_PEAK_OUTSEL_SET(p->bnr.cpeak_peak_outsel));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_CEDGE_TH0_CAM0_IDX], p->bnr.cedge_th0_edge_th0);
	regmap_field_write(neoispd->regs.fields[NEO_BNR_CEDGE_SCALE_CAM0_IDX],
			NEO_BNR_CEDGE_SCALE_CAM0_SCALE_SET(p->bnr.cedge_scale_scale)
			| NEO_BNR_CEDGE_SCALE_CAM0_SHIFT_SET(p->bnr.cedge_scale_shift));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_CEDGES_TH0_CAM0_IDX], p->bnr.cedges_th0_edge_th0);
	regmap_field_write(neoispd->regs.fields[NEO_BNR_CEDGES_SCALE_CAM0_IDX],
			NEO_BNR_CEDGES_SCALE_CAM0_SCALE_SET(p->bnr.cedges_scale_scale)
			| NEO_BNR_CEDGES_SCALE_CAM0_SHIFT_SET(p->bnr.cedges_scale_shift));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_CEDGEA_TH0_CAM0_IDX], p->bnr.cedgea_th0_edge_th0);
	regmap_field_write(neoispd->regs.fields[NEO_BNR_CEDGEA_SCALE_CAM0_IDX],
			NEO_BNR_CEDGEA_SCALE_CAM0_SCALE_SET(p->bnr.cedgea_scale_scale)
			| NEO_BNR_CEDGEA_SCALE_CAM0_SHIFT_SET(p->bnr.cedgea_scale_shift));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_CLUMA_X_TH0_CAM0_IDX], p->bnr.cluma_x_th0_th);
	regmap_field_write(neoispd->regs.fields[NEO_BNR_CLUMA_Y_TH_CAM0_IDX],
			NEO_BNR_CLUMA_Y_TH_CAM0_LUMA_Y_TH0_SET(p->bnr.cluma_y_th_luma_y_th0)
			| NEO_BNR_CLUMA_Y_TH_CAM0_LUMA_Y_TH1_SET(p->bnr.cluma_y_th_luma_y_th1));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_CLUMA_SCALE_CAM0_IDX],
			NEO_BNR_CLUMA_SCALE_CAM0_SCALE_SET(p->bnr.cluma_scale_scale)
			| NEO_BNR_CLUMA_SCALE_CAM0_SHIFT_SET(p->bnr.cluma_scale_shift));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_CALPHA_GAIN_CAM0_IDX],
			NEO_BNR_CALPHA_GAIN_CAM0_GAIN_SET(p->bnr.calpha_gain_gain)
			| NEO_BNR_CALPHA_GAIN_CAM0_OFFSET_SET(p->bnr.calpha_gain_offset));
	regmap_field_write(neoispd->regs.fields[NEO_BNR_STRETCH_CAM0_IDX], p->bnr.stretch_gain);
}

static void neoisp_set_vignetting(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_VIGNETTING_CTRL_CAM0_IDX],
			NEO_VIGNETTING_CTRL_CAM0_ENABLE_SET(p->vignetting_ctrl.ctrl_enable));
	regmap_field_write(neoispd->regs.fields[NEO_VIGNETTING_BLK_CONF_CAM0_IDX],
			NEO_VIGNETTING_BLK_CONF_CAM0_COLS_SET(p->vignetting_ctrl.blk_conf_cols)
			| NEO_VIGNETTING_BLK_CONF_CAM0_ROWS_SET(p->vignetting_ctrl.blk_conf_rows));
	regmap_field_write(neoispd->regs.fields[NEO_VIGNETTING_BLK_SIZE_CAM0_IDX],
			NEO_VIGNETTING_BLK_SIZE_CAM0_XSIZE_SET(p->vignetting_ctrl.blk_size_xsize)
			| NEO_VIGNETTING_BLK_SIZE_CAM0_YSIZE_SET(p->vignetting_ctrl.blk_size_ysize));
	regmap_field_write(neoispd->regs.fields[NEO_VIGNETTING_BLK_STEPY_CAM0_IDX],
			NEO_VIGNETTING_BLK_STEPY_CAM0_STEP_SET(p->vignetting_ctrl.blk_stepy_step));
	regmap_field_write(neoispd->regs.fields[NEO_VIGNETTING_BLK_STEPX_CAM0_IDX],
			NEO_VIGNETTING_BLK_STEPX_CAM0_STEP_SET(p->vignetting_ctrl.blk_stepx_step));
}

static void neoisp_set_demosaic(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_DEMOSAIC_CTRL_CAM0_IDX],
			NEO_DEMOSAIC_CTRL_CAM0_FMT_SET(p->demosaic.ctrl_fmt));
	regmap_field_write(neoispd->regs.fields[NEO_DEMOSAIC_ACTIVITY_CTL_CAM0_IDX],
			NEO_DEMOSAIC_ACTIVITY_CTL_CAM0_ALPHA_SET(p->demosaic.activity_ctl_alpha)
			| NEO_DEMOSAIC_ACTIVITY_CTL_CAM0_ACT_RATIO_SET(p->demosaic.activity_ctl_act_ratio));
	regmap_field_write(neoispd->regs.fields[NEO_DEMOSAIC_DYNAMICS_CTL0_CAM0_IDX],
			NEO_DEMOSAIC_DYNAMICS_CTL0_CAM0_STRENGTHG_SET(p->demosaic.dynamics_ctl0_strengthg)
			| NEO_DEMOSAIC_DYNAMICS_CTL0_CAM0_STRENGTHC_SET(p->demosaic.dynamics_ctl0_strengthc));
	regmap_field_write(neoispd->regs.fields[NEO_DEMOSAIC_DYNAMICS_CTL2_CAM0_IDX],
			NEO_DEMOSAIC_DYNAMICS_CTL0_CAM0_STRENGTHC_SET(p->demosaic.dynamics_ctl2_max_impact));
}

static void neoisp_set_rgb_to_yuv(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_RGB_TO_YUV_GAIN_CTRL_CAM0_IDX],
			NEO_RGB_TO_YUV_GAIN_CTRL_CAM0_RGAIN_SET(p->rgb2yuv.gain_ctrl_rgain)
			| NEO_RGB_TO_YUV_GAIN_CTRL_CAM0_BGAIN_SET(p->rgb2yuv.gain_ctrl_bgain));
	regmap_field_write(neoispd->regs.fields[NEO_RGB_TO_YUV_MAT0_CAM0_IDX],
			NEO_RGB_TO_YUV_MAT0_CAM0_R0C0_SET(p->rgb2yuv.mat_rxcy[0][0])
			| NEO_RGB_TO_YUV_MAT0_CAM0_R0C1_SET(p->rgb2yuv.mat_rxcy[0][1]));
	regmap_field_write(neoispd->regs.fields[NEO_RGB_TO_YUV_MAT1_CAM0_IDX],
			NEO_RGB_TO_YUV_MAT1_CAM0_R0C2_SET(p->rgb2yuv.mat_rxcy[0][2]));
	regmap_field_write(neoispd->regs.fields[NEO_RGB_TO_YUV_MAT2_CAM0_IDX],
			NEO_RGB_TO_YUV_MAT2_CAM0_R1C0_SET(p->rgb2yuv.mat_rxcy[1][0])
			| NEO_RGB_TO_YUV_MAT2_CAM0_R1C1_SET(p->rgb2yuv.mat_rxcy[1][1]));
	regmap_field_write(neoispd->regs.fields[NEO_RGB_TO_YUV_MAT3_CAM0_IDX],
			NEO_RGB_TO_YUV_MAT3_CAM0_R1C2_SET(p->rgb2yuv.mat_rxcy[1][2]));
	regmap_field_write(neoispd->regs.fields[NEO_RGB_TO_YUV_MAT4_CAM0_IDX],
			NEO_RGB_TO_YUV_MAT4_CAM0_R2C0_SET(p->rgb2yuv.mat_rxcy[2][0])
			| NEO_RGB_TO_YUV_MAT4_CAM0_R2C1_SET(p->rgb2yuv.mat_rxcy[2][1]));
	regmap_field_write(neoispd->regs.fields[NEO_RGB_TO_YUV_MAT5_CAM0_IDX],
			NEO_RGB_TO_YUV_MAT5_CAM0_R2C2_SET(p->rgb2yuv.mat_rxcy[2][2]));
	regmap_field_write(neoispd->regs.fields[NEO_RGB_TO_YUV_OFFSET0_CAM0_IDX],
			NEO_RGB_TO_YUV_OFFSET0_CAM0_OFFSET_SET(p->rgb2yuv.csc_offsets[0]));
	regmap_field_write(neoispd->regs.fields[NEO_RGB_TO_YUV_OFFSET1_CAM0_IDX],
			NEO_RGB_TO_YUV_OFFSET1_CAM0_OFFSET_SET(p->rgb2yuv.csc_offsets[1]));
	regmap_field_write(neoispd->regs.fields[NEO_RGB_TO_YUV_OFFSET2_CAM0_IDX],
			NEO_RGB_TO_YUV_OFFSET2_CAM0_OFFSET_SET(p->rgb2yuv.csc_offsets[2]));
}


static void neoisp_set_drc(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_DRC_ROI0_POS_CAM0_IDX],
			NEO_DRC_ROI0_POS_CAM0_XPOS_SET(p->drc.roi0.xpos)
			| NEO_DRC_ROI0_POS_CAM0_YPOS_SET(p->drc.roi0.ypos));
	regmap_field_write(neoispd->regs.fields[NEO_DRC_ROI0_SIZE_CAM0_IDX],
			NEO_DRC_ROI0_SIZE_CAM0_WIDTH_SET(p->drc.roi0.width)
			| NEO_DRC_ROI0_SIZE_CAM0_HEIGHT_SET(p->drc.roi0.height));
	regmap_field_write(neoispd->regs.fields[NEO_DRC_ROI1_POS_CAM0_IDX],
			NEO_DRC_ROI1_POS_CAM0_XPOS_SET(p->drc.roi1.xpos)
			| NEO_DRC_ROI1_POS_CAM0_YPOS_SET(p->drc.roi1.ypos));
	regmap_field_write(neoispd->regs.fields[NEO_DRC_ROI1_SIZE_CAM0_IDX],
			NEO_DRC_ROI1_SIZE_CAM0_WIDTH_SET(p->drc.roi1.width)
			| NEO_DRC_ROI1_SIZE_CAM0_HEIGHT_SET(p->drc.roi1.height));
	regmap_field_write(neoispd->regs.fields[NEO_DRC_GROI_SUM_SHIFT_CAM0_IDX],
			NEO_DRC_GROI_SUM_SHIFT_CAM0_SHIFT0_SET(p->drc.groi_sum_shift_shift0)
			| NEO_DRC_GROI_SUM_SHIFT_CAM0_SHIFT1_SET(p->drc.groi_sum_shift_shift1));
	regmap_field_write(neoispd->regs.fields[NEO_DRC_GBL_GAIN_CAM0_IDX],
			NEO_DRC_GBL_GAIN_CAM0_GAIN_SET(p->drc.gbl_gain_gain));
	regmap_field_write(neoispd->regs.fields[NEO_DRC_LCL_BLK_SIZE_CAM0_IDX],
			NEO_DRC_LCL_BLK_SIZE_CAM0_XSIZE_SET(p->drc.lcl_blk_size_xsize)
			| NEO_DRC_LCL_BLK_SIZE_CAM0_YSIZE_SET(p->drc.lcl_blk_size_ysize));
	regmap_field_write(neoispd->regs.fields[NEO_DRC_LCL_STRETCH_CAM0_IDX],
			NEO_DRC_LCL_STRETCH_CAM0_STRETCH_SET(p->drc.lcl_stretch_stretch)
			| NEO_DRC_LCL_STRETCH_CAM0_OFFSET_SET(p->drc.lcl_stretch_offset));
	regmap_field_write(neoispd->regs.fields[NEO_DRC_LCL_BLK_STEPY_CAM0_IDX],
			NEO_DRC_LCL_BLK_STEPY_CAM0_STEP_SET(p->drc.lcl_blk_stepy_step));
	regmap_field_write(neoispd->regs.fields[NEO_DRC_LCL_BLK_STEPX_CAM0_IDX],
			NEO_DRC_LCL_BLK_STEPX_CAM0_STEP_SET(p->drc.lcl_blk_stepx_step));
	regmap_field_write(neoispd->regs.fields[NEO_DRC_LCL_SUM_SHIFT_CAM0_IDX],
			NEO_DRC_LCL_SUM_SHIFT_CAM0_SHIFT_SET(p->drc.lcl_sum_shift_shift));
	regmap_field_write(neoispd->regs.fields[NEO_DRC_ALPHA_CAM0_IDX],
			NEO_DRC_ALPHA_CAM0_ALPHA_SET(p->drc.alpha_alpha));
}

static void neoisp_set_nr(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_NR_CTRL_CAM0_IDX],
			NEO_NR_CTRL_CAM0_DEBUG_SET(p->nrc.ctrl_debug)
			| NEO_NR_CTRL_CAM0_ENABLE_SET(p->nrc.ctrl_enable));
	regmap_field_write(neoispd->regs.fields[NEO_NR_BLEND_SCALE_CAM0_IDX],
			NEO_NR_BLEND_SCALE_CAM0_SCALE_SET(p->nrc.blend_scale_scale)
			| NEO_NR_BLEND_SCALE_CAM0_SHIFT_SET(p->nrc.blend_scale_shift)
			| NEO_NR_BLEND_SCALE_CAM0_GAIN_SET(p->nrc.blend_scale_gain));
	regmap_field_write(neoispd->regs.fields[NEO_NR_BLEND_TH0_CAM0_IDX],
			NEO_NR_BLEND_TH0_CAM0_TH_SET(p->nrc.blend_th0_th));
}

static void neoisp_set_df(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_DF_CTRL_CAM0_IDX],
			NEO_DF_CTRL_CAM0_DEBUG_SET(p->dfc.ctrl_debug)
			| NEO_DF_CTRL_CAM0_ENABLE_SET(p->dfc.ctrl_enable));
	regmap_field_write(neoispd->regs.fields[NEO_DF_TH_SCALE_CAM0_IDX],
			NEO_DF_TH_SCALE_CAM0_SCALE_SET(p->dfc.th_scale_scale));
	regmap_field_write(neoispd->regs.fields[NEO_DF_BLEND_SHIFT_CAM0_IDX],
			NEO_DF_BLEND_SHIFT_CAM0_SHIFT_SET(p->dfc.blend_shift_shift));
	regmap_field_write(neoispd->regs.fields[NEO_DF_BLEND_TH0_CAM0_IDX],
			NEO_DF_BLEND_TH0_CAM0_TH_SET(p->dfc.blend_th0_th));
}

static void neoisp_set_ee(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_EE_CTRL_CAM0_IDX],
			NEO_EE_CTRL_CAM0_DEBUG_SET(p->eec.ctrl_debug)
			| NEO_EE_CTRL_CAM0_ENABLE_SET(p->eec.ctrl_enable));
	regmap_field_write(neoispd->regs.fields[NEO_EE_CORING_CAM0_IDX],
			NEO_EE_CORING_CAM0_CORING_SET(p->eec.coring_coring));
	regmap_field_write(neoispd->regs.fields[NEO_EE_CLIP_CAM0_IDX],
			NEO_EE_CLIP_CAM0_CLIP_SET(p->eec.clip_clip));
	regmap_field_write(neoispd->regs.fields[NEO_EE_MASKGAIN_CAM0_IDX],
			NEO_EE_MASKGAIN_CAM0_GAIN_SET(p->eec.maskgain_gain));
}

static void neoisp_set_convmed(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_CCONVMED_CTRL_CAM0_IDX],
			NEO_CCONVMED_CTRL_CAM0_FLT_SET(p->convf.ctrl_flt));
}

static void neoisp_set_cas(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_CAS_GAIN_CAM0_IDX],
			NEO_CAS_GAIN_CAM0_SCALE_SET(p->cas.gain_scale)
			| NEO_CAS_GAIN_CAM0_SHIFT_SET(p->cas.gain_shift));
	regmap_field_write(neoispd->regs.fields[NEO_CAS_CORR_CAM0_IDX],
			NEO_CAS_CORR_CAM0_CORR_SET(p->cas.corr_corr));
	regmap_field_write(neoispd->regs.fields[NEO_CAS_OFFSET_CAM0_IDX],
			NEO_CAS_OFFSET_CAM0_OFFSET_SET(p->cas.offset_offset));
}

static void neoisp_set_gcm(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_GCM_IMAT0_CAM0_IDX],
			NEO_GCM_IMAT0_CAM0_R0C0_SET(p->gcm.imat_rxcy[0][0])
			| NEO_GCM_IMAT0_CAM0_R0C1_SET(p->gcm.imat_rxcy[0][1]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_IMAT1_CAM0_IDX],
			NEO_GCM_IMAT1_CAM0_R0C2_SET(p->gcm.imat_rxcy[0][2]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_IMAT2_CAM0_IDX],
			NEO_GCM_IMAT2_CAM0_R1C0_SET(p->gcm.imat_rxcy[1][0])
			| NEO_GCM_IMAT2_CAM0_R1C1_SET(p->gcm.imat_rxcy[1][1]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_IMAT3_CAM0_IDX],
			NEO_GCM_IMAT3_CAM0_R1C2_SET(p->gcm.imat_rxcy[1][2]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_IMAT4_CAM0_IDX],
			NEO_GCM_IMAT4_CAM0_R2C0_SET(p->gcm.imat_rxcy[2][0])
			| NEO_GCM_IMAT4_CAM0_R2C1_SET(p->gcm.imat_rxcy[2][1]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_IMAT5_CAM0_IDX],
			NEO_GCM_IMAT5_CAM0_R2C2_SET(p->gcm.imat_rxcy[2][2]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_IOFFSET0_CAM0_IDX],
			NEO_GCM_IOFFSET0_CAM0_OFFSET0_SET(p->gcm.ioffsets[0]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_IOFFSET1_CAM0_IDX],
			NEO_GCM_IOFFSET1_CAM0_OFFSET1_SET(p->gcm.ioffsets[1]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_IOFFSET2_CAM0_IDX],
			NEO_GCM_IOFFSET2_CAM0_OFFSET2_SET(p->gcm.ioffsets[2]));

	regmap_field_write(neoispd->regs.fields[NEO_GCM_OMAT0_CAM0_IDX],
			NEO_GCM_OMAT0_CAM0_R0C0_SET(p->gcm.omat_rxcy[0][0])
			| NEO_GCM_OMAT0_CAM0_R0C1_SET(p->gcm.omat_rxcy[0][1]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_OMAT1_CAM0_IDX],
			NEO_GCM_OMAT1_CAM0_R0C2_SET(p->gcm.omat_rxcy[0][2]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_OMAT2_CAM0_IDX],
			NEO_GCM_OMAT2_CAM0_R1C0_SET(p->gcm.omat_rxcy[1][0])
			| NEO_GCM_OMAT2_CAM0_R1C1_SET(p->gcm.omat_rxcy[1][1]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_OMAT3_CAM0_IDX],
			NEO_GCM_OMAT3_CAM0_R1C2_SET(p->gcm.omat_rxcy[1][2]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_OMAT4_CAM0_IDX],
			NEO_GCM_OMAT4_CAM0_R2C0_SET(p->gcm.omat_rxcy[2][0])
			| NEO_GCM_OMAT4_CAM0_R2C1_SET(p->gcm.omat_rxcy[2][1]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_OMAT5_CAM0_IDX],
			NEO_GCM_OMAT5_CAM0_R2C2_SET(p->gcm.omat_rxcy[2][2]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_OOFFSET0_CAM0_IDX],
			NEO_GCM_OOFFSET0_CAM0_OFFSET0_SET(p->gcm.ooffsets[0]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_OOFFSET1_CAM0_IDX],
			NEO_GCM_OOFFSET1_CAM0_OFFSET1_SET(p->gcm.ooffsets[1]));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_OOFFSET2_CAM0_IDX],
			NEO_GCM_OOFFSET2_CAM0_OFFSET2_SET(p->gcm.ooffsets[2]));

	regmap_field_write(neoispd->regs.fields[NEO_GCM_GAMMA0_CAM0_IDX],
			NEO_GCM_GAMMA0_CAM0_GAMMA0_SET(p->gcm.gamma0_gamma0)
			| NEO_GCM_GAMMA0_CAM0_OFFSET0_SET(p->gcm.gamma0_offset0));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_GAMMA1_CAM0_IDX],
			NEO_GCM_GAMMA1_CAM0_GAMMA1_SET(p->gcm.gamma1_gamma1)
			| NEO_GCM_GAMMA1_CAM0_OFFSET1_SET(p->gcm.gamma1_offset1));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_GAMMA2_CAM0_IDX],
			NEO_GCM_GAMMA2_CAM0_GAMMA2_SET(p->gcm.gamma2_gamma2)
			| NEO_GCM_GAMMA2_CAM0_OFFSET2_SET(p->gcm.gamma2_offset2));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_BLKLVL0_CTRL_CAM0_IDX],
			NEO_GCM_BLKLVL0_CTRL_CAM0_OFFSET0_SET(p->gcm.blklvl0_ctrl_offset0)
			| NEO_GCM_BLKLVL0_CTRL_CAM0_GAIN0_SET(p->gcm.blklvl0_ctrl_gain0));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_BLKLVL1_CTRL_CAM0_IDX],
			NEO_GCM_BLKLVL1_CTRL_CAM0_OFFSET1_SET(p->gcm.blklvl1_ctrl_offset1)
			| NEO_GCM_BLKLVL1_CTRL_CAM0_GAIN1_SET(p->gcm.blklvl1_ctrl_gain1));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_BLKLVL2_CTRL_CAM0_IDX],
			NEO_GCM_BLKLVL2_CTRL_CAM0_OFFSET2_SET(p->gcm.blklvl2_ctrl_offset2)
			| NEO_GCM_BLKLVL2_CTRL_CAM0_GAIN2_SET(p->gcm.blklvl2_ctrl_gain2));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_LOWTH_CTRL01_CAM0_IDX],
			NEO_GCM_LOWTH_CTRL01_CAM0_THRESHOLD0_SET(p->gcm.lowth_ctrl01_threshold0)
			| NEO_GCM_LOWTH_CTRL01_CAM0_THRESHOLD1_SET(p->gcm.lowth_ctrl01_threshold1));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_LOWTH_CTRL2_CAM0_IDX],
			NEO_GCM_LOWTH_CTRL2_CAM0_THRESHOLD2_SET(p->gcm.lowth_ctrl2_threshold2));
	regmap_field_write(neoispd->regs.fields[NEO_GCM_MAT_CONFG_CAM0_IDX],
			NEO_GCM_MAT_CONFG_CAM0_SIGN_CONFG_SET(p->gcm.mat_confg_sign_confg));
}

static void neoisp_set_autofocus(struct neoisp_reg_params_s *p, struct neoisp_dev_s *neoispd)
{
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI0_POS_CAM0_IDX],
			NEO_AUTOFOCUS_ROI0_POS_CAM0_XPOS_SET(p->afc.af_roi[0].xpos)
			| NEO_AUTOFOCUS_ROI0_POS_CAM0_YPOS_SET(p->afc.af_roi[0].ypos));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI0_SIZE_CAM0_IDX],
			NEO_AUTOFOCUS_ROI0_SIZE_CAM0_WIDTH_SET(p->afc.af_roi[0].width)
			| NEO_AUTOFOCUS_ROI0_SIZE_CAM0_HEIGHT_SET(p->afc.af_roi[0].height));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI1_POS_CAM0_IDX],
			NEO_AUTOFOCUS_ROI1_POS_CAM0_XPOS_SET(p->afc.af_roi[1].xpos)
			| NEO_AUTOFOCUS_ROI1_POS_CAM0_YPOS_SET(p->afc.af_roi[1].ypos));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI1_SIZE_CAM0_IDX],
			NEO_AUTOFOCUS_ROI1_SIZE_CAM0_WIDTH_SET(p->afc.af_roi[1].width)
			| NEO_AUTOFOCUS_ROI1_SIZE_CAM0_HEIGHT_SET(p->afc.af_roi[1].height));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI2_POS_CAM0_IDX],
			NEO_AUTOFOCUS_ROI2_POS_CAM0_XPOS_SET(p->afc.af_roi[2].xpos)
			| NEO_AUTOFOCUS_ROI2_POS_CAM0_YPOS_SET(p->afc.af_roi[2].ypos));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI2_SIZE_CAM0_IDX],
			NEO_AUTOFOCUS_ROI2_SIZE_CAM0_WIDTH_SET(p->afc.af_roi[2].width)
			| NEO_AUTOFOCUS_ROI2_SIZE_CAM0_HEIGHT_SET(p->afc.af_roi[2].height));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI3_POS_CAM0_IDX],
			NEO_AUTOFOCUS_ROI3_POS_CAM0_XPOS_SET(p->afc.af_roi[3].xpos)
			| NEO_AUTOFOCUS_ROI3_POS_CAM0_YPOS_SET(p->afc.af_roi[3].ypos));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI3_SIZE_CAM0_IDX],
			NEO_AUTOFOCUS_ROI3_SIZE_CAM0_WIDTH_SET(p->afc.af_roi[3].width)
			| NEO_AUTOFOCUS_ROI3_SIZE_CAM0_HEIGHT_SET(p->afc.af_roi[3].height));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI4_POS_CAM0_IDX],
			NEO_AUTOFOCUS_ROI4_POS_CAM0_XPOS_SET(p->afc.af_roi[4].xpos)
			| NEO_AUTOFOCUS_ROI4_POS_CAM0_YPOS_SET(p->afc.af_roi[4].ypos));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI4_SIZE_CAM0_IDX],
			NEO_AUTOFOCUS_ROI4_SIZE_CAM0_WIDTH_SET(p->afc.af_roi[4].width)
			| NEO_AUTOFOCUS_ROI4_SIZE_CAM0_HEIGHT_SET(p->afc.af_roi[4].height));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI5_POS_CAM0_IDX],
			NEO_AUTOFOCUS_ROI5_POS_CAM0_XPOS_SET(p->afc.af_roi[5].xpos)
			| NEO_AUTOFOCUS_ROI5_POS_CAM0_YPOS_SET(p->afc.af_roi[5].ypos));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI5_SIZE_CAM0_IDX],
			NEO_AUTOFOCUS_ROI5_SIZE_CAM0_WIDTH_SET(p->afc.af_roi[5].width)
			| NEO_AUTOFOCUS_ROI5_SIZE_CAM0_HEIGHT_SET(p->afc.af_roi[5].height));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI6_POS_CAM0_IDX],
			NEO_AUTOFOCUS_ROI6_POS_CAM0_XPOS_SET(p->afc.af_roi[6].xpos)
			| NEO_AUTOFOCUS_ROI6_POS_CAM0_YPOS_SET(p->afc.af_roi[6].ypos));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI6_SIZE_CAM0_IDX],
			NEO_AUTOFOCUS_ROI6_SIZE_CAM0_WIDTH_SET(p->afc.af_roi[6].width)
			| NEO_AUTOFOCUS_ROI6_SIZE_CAM0_HEIGHT_SET(p->afc.af_roi[6].height));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI7_POS_CAM0_IDX],
			NEO_AUTOFOCUS_ROI7_POS_CAM0_XPOS_SET(p->afc.af_roi[7].xpos)
			| NEO_AUTOFOCUS_ROI7_POS_CAM0_YPOS_SET(p->afc.af_roi[7].ypos));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI7_SIZE_CAM0_IDX],
			NEO_AUTOFOCUS_ROI7_SIZE_CAM0_WIDTH_SET(p->afc.af_roi[7].width)
			| NEO_AUTOFOCUS_ROI7_SIZE_CAM0_HEIGHT_SET(p->afc.af_roi[7].height));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI8_POS_CAM0_IDX],
			NEO_AUTOFOCUS_ROI8_POS_CAM0_XPOS_SET(p->afc.af_roi[8].xpos)
			| NEO_AUTOFOCUS_ROI8_POS_CAM0_YPOS_SET(p->afc.af_roi[8].ypos));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_ROI8_SIZE_CAM0_IDX],
			NEO_AUTOFOCUS_ROI8_SIZE_CAM0_WIDTH_SET(p->afc.af_roi[8].width)
			| NEO_AUTOFOCUS_ROI8_SIZE_CAM0_HEIGHT_SET(p->afc.af_roi[8].height));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_FIL0_COEFFS0_CAM0_IDX],
			NEO_AUTOFOCUS_FIL0_COEFFS0_CAM0_COEFF0_SET(p->afc.fil0_coeffs[0])
			| NEO_AUTOFOCUS_FIL0_COEFFS0_CAM0_COEFF1_SET(p->afc.fil0_coeffs[1])
			| NEO_AUTOFOCUS_FIL0_COEFFS0_CAM0_COEFF2_SET(p->afc.fil0_coeffs[2])
			| NEO_AUTOFOCUS_FIL0_COEFFS0_CAM0_COEFF3_SET(p->afc.fil0_coeffs[3]));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_FIL0_COEFFS1_CAM0_IDX],
			NEO_AUTOFOCUS_FIL0_COEFFS1_CAM0_COEFF4_SET(p->afc.fil0_coeffs[4])
			| NEO_AUTOFOCUS_FIL0_COEFFS1_CAM0_COEFF5_SET(p->afc.fil0_coeffs[5])
			| NEO_AUTOFOCUS_FIL0_COEFFS1_CAM0_COEFF6_SET(p->afc.fil0_coeffs[6])
			| NEO_AUTOFOCUS_FIL0_COEFFS1_CAM0_COEFF7_SET(p->afc.fil0_coeffs[7]));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_FIL0_COEFFS2_CAM0_IDX],
			NEO_AUTOFOCUS_FIL0_COEFFS2_CAM0_COEFF8_SET(p->afc.fil0_coeffs[8]));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_FIL1_COEFFS0_CAM0_IDX],
			NEO_AUTOFOCUS_FIL1_COEFFS0_CAM0_COEFF0_SET(p->afc.fil1_coeffs[0])
			| NEO_AUTOFOCUS_FIL1_COEFFS0_CAM0_COEFF1_SET(p->afc.fil1_coeffs[1])
			| NEO_AUTOFOCUS_FIL1_COEFFS0_CAM0_COEFF2_SET(p->afc.fil1_coeffs[2])
			| NEO_AUTOFOCUS_FIL1_COEFFS0_CAM0_COEFF3_SET(p->afc.fil1_coeffs[3]));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_FIL1_COEFFS1_CAM0_IDX],
			NEO_AUTOFOCUS_FIL1_COEFFS1_CAM0_COEFF4_SET(p->afc.fil1_coeffs[4])
			| NEO_AUTOFOCUS_FIL1_COEFFS1_CAM0_COEFF5_SET(p->afc.fil1_coeffs[5])
			| NEO_AUTOFOCUS_FIL1_COEFFS1_CAM0_COEFF6_SET(p->afc.fil1_coeffs[6])
			| NEO_AUTOFOCUS_FIL1_COEFFS1_CAM0_COEFF7_SET(p->afc.fil1_coeffs[7]));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_FIL1_COEFFS2_CAM0_IDX],
			NEO_AUTOFOCUS_FIL1_COEFFS2_CAM0_COEFF8_SET(p->afc.fil1_coeffs[8]));
	regmap_field_write(neoispd->regs.fields[NEO_AUTOFOCUS_FIL1_SHIFT_CAM0_IDX],
			NEO_AUTOFOCUS_FIL1_SHIFT_CAM0_SHIFT_SET(p->afc.fil1_shift_shift));
}

static void neoisp_set_mem_vignetting_table(struct neoisp_mem_params_s *p, __u32 *dest)
{
	ctx_blk_write(NEO_VIGNETTING_TABLE_MAP, (__u32 *)p->vt.vignetting_table, dest);
}

static void neoisp_set_mem_global_tonemap(struct neoisp_mem_params_s *p, __u32 *dest)
{
	ctx_blk_write(NEO_DRC_GLOBAL_TONEMAP_MAP, (__u32 *)p->gtm.drc_global_tonemap, dest);
}

static void neoisp_set_mem_local_tonemap(struct neoisp_mem_params_s *p, __u32 *dest)
{
	ctx_blk_write(NEO_DRC_LOCAL_TONEMAP_MAP, (__u32 *)p->ltm.drc_local_tonemap, dest);
}

int neoisp_set_params(struct neoisp_dev_s *neoispd, struct neoisp_meta_params_s *p)
{
	__u32 *mem = (__u32 *)neoispd->mmio_tcm;

	/* update selected blocks wrt feature config flag */
	if (p->features_cfg.hdr_decompress_dcg_cfg)
		neoisp_set_hdr_decompress0(&p->regs, neoispd);
	if (p->features_cfg.hdr_decompress_vs_cfg)
		neoisp_set_hdr_decompress1(&p->regs, neoispd);
	if (p->features_cfg.obwb0_cfg)
		neoisp_set_ob_wb0(&p->regs, neoispd);
	if (p->features_cfg.obwb1_cfg)
		neoisp_set_ob_wb1(&p->regs, neoispd);
	if (p->features_cfg.obwb2_cfg)
		neoisp_set_ob_wb2(&p->regs, neoispd);
	if (p->features_cfg.hdr_merge_cfg)
		neoisp_set_hdr_merge(&p->regs, neoispd);
	if (p->features_cfg.rgbir_cfg)
		neoisp_set_rgbir(&p->regs, neoispd);
	if (p->features_cfg.stat_cfg)
		neoisp_set_stat_hists(&p->regs, neoispd);
	if (p->features_cfg.ir_compress_cfg)
		neoisp_set_ir_compress(&p->regs, neoispd);
	if (p->features_cfg.bnr_cfg)
		neoisp_set_bnr(&p->regs, neoispd);
	if (p->features_cfg.vignetting_ctrl_cfg)
		neoisp_set_vignetting(&p->regs, neoispd);
	if (p->features_cfg.ctemp_cfg)
		neoisp_set_color_temp(&p->regs, neoispd);
	if (p->features_cfg.demosaic_cfg)
		neoisp_set_demosaic(&p->regs, neoispd);
	if (p->features_cfg.rgb2yuv_cfg)
		neoisp_set_rgb_to_yuv(&p->regs, neoispd);
	if (p->features_cfg.dr_comp_cfg)
		neoisp_set_drc(&p->regs, neoispd);
	if (p->features_cfg.nr_cfg)
		neoisp_set_nr(&p->regs, neoispd);
	if (p->features_cfg.af_cfg)
		neoisp_set_autofocus(&p->regs, neoispd);
	if (p->features_cfg.ee_cfg)
		neoisp_set_ee(&p->regs, neoispd);
	if (p->features_cfg.df_cfg)
		neoisp_set_df(&p->regs, neoispd);
	if (p->features_cfg.convmed_cfg)
		neoisp_set_convmed(&p->regs, neoispd);
	if (p->features_cfg.cas_cfg)
		neoisp_set_cas(&p->regs, neoispd);
	if (p->features_cfg.gcm_cfg)
		neoisp_set_gcm(&p->regs, neoispd);
	if (p->features_cfg.vignetting_table_cfg)
		neoisp_set_mem_vignetting_table(&p->mems, mem);
	if (p->features_cfg.drc_global_tonemap_cfg)
		neoisp_set_mem_global_tonemap(&p->mems, mem);
	if (p->features_cfg.drc_local_tonemap_cfg)
		neoisp_set_mem_local_tonemap(&p->mems, mem);

	return 0;
}
