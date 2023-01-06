/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MDP3_PLAT_MT8188_H__
#define __MDP3_PLAT_MT8188_H__

static const struct mdp_platform_config mt8188_plat_cfg = {
	.rdma_support_10bit             = true,
	.rdma_rsz1_sram_sharing         = false,
	.rdma_upsample_repeat_only      = false,
	.rdma_esl_setting		= true,
	.rdma_event_num			= 4,
	.rsz_disable_dcm_small_sample   = false,
	.rsz_etc_control		= true,
	.wrot_filter_constraint		= false,
	.wrot_event_num			= 4,
	.tdshp_hist_num			= 17,
	.tdshp_constrain		= true,
	.tdshp_contour			= true,
};

static const struct of_device_id mt8188_mdp_probe_infra[MDP_INFRA_MAX] = {
	[MDP_INFRA_MMSYS] = { .compatible = "mediatek,mt8188-vppsys0" },
	[MDP_INFRA_MMSYS2] = { .compatible = "mediatek,mt8188-vppsys1" },
	[MDP_INFRA_MUTEX] = { .compatible = "mediatek,mt8188-vpp-mutex" },
	[MDP_INFRA_MUTEX2] = { .compatible = "mediatek,mt8188-vpp-mutex" },
	[MDP_INFRA_SCP] = { .compatible = "mediatek,mt8188-scp" }
};

static const struct mdp_pipe_info mt8188_pipe_info[] = {
	{MDP_PIPE_WPEI, 0, 0},
	{MDP_PIPE_RDMA0, 0, 2},
	{MDP_PIPE_VPP1_SOUT, 0, 3},
	{MDP_PIPE_RDMA1, 1, 2},
	{MDP_PIPE_RDMA2, 1, 2},
	{MDP_PIPE_RDMA3, 1, 3},
	{MDP_PIPE_VPP0_SOUT, 1, 4},
};

static const u32 mt8188_mutex_idx[MDP_MAX_COMP_COUNT] = {
	[MDP_COMP_RDMA0] = MUTEX_MOD_IDX_MDP_RDMA0,
	[MDP_COMP_RDMA2] = MUTEX_MOD_IDX_MDP_RDMA2,
	[MDP_COMP_RDMA3] = MUTEX_MOD_IDX_MDP_RDMA3,
	[MDP_COMP_FG0] = MUTEX_MOD_IDX_MDP_FG0,
	[MDP_COMP_FG2] = MUTEX_MOD_IDX_MDP_FG2,
	[MDP_COMP_FG3] = MUTEX_MOD_IDX_MDP_FG3,
	[MDP_COMP_HDR0] = MUTEX_MOD_IDX_MDP_HDR0,
	[MDP_COMP_HDR2] = MUTEX_MOD_IDX_MDP_HDR2,
	[MDP_COMP_HDR3] = MUTEX_MOD_IDX_MDP_HDR3,
	[MDP_COMP_AAL0] = MUTEX_MOD_IDX_MDP_AAL0,
	[MDP_COMP_AAL2] = MUTEX_MOD_IDX_MDP_AAL2,
	[MDP_COMP_AAL3] = MUTEX_MOD_IDX_MDP_AAL3,
	[MDP_COMP_RSZ0] = MUTEX_MOD_IDX_MDP_RSZ0,
	[MDP_COMP_RSZ2] = MUTEX_MOD_IDX_MDP_RSZ2,
	[MDP_COMP_RSZ3] = MUTEX_MOD_IDX_MDP_RSZ3,
	[MDP_COMP_MERGE2] = MUTEX_MOD_IDX_MDP_MERGE2,
	[MDP_COMP_MERGE3] = MUTEX_MOD_IDX_MDP_MERGE3,
	[MDP_COMP_TDSHP0] = MUTEX_MOD_IDX_MDP_TDSHP0,
	[MDP_COMP_TDSHP2] = MUTEX_MOD_IDX_MDP_TDSHP2,
	[MDP_COMP_TDSHP3] = MUTEX_MOD_IDX_MDP_TDSHP3,
	[MDP_COMP_COLOR0] = MUTEX_MOD_IDX_MDP_COLOR0,
	[MDP_COMP_COLOR2] = MUTEX_MOD_IDX_MDP_COLOR2,
	[MDP_COMP_COLOR3] = MUTEX_MOD_IDX_MDP_COLOR3,
	[MDP_COMP_OVL0] = MUTEX_MOD_IDX_MDP_OVL0,
	[MDP_COMP_PAD0] = MUTEX_MOD_IDX_MDP_PAD0,
	[MDP_COMP_PAD2] = MUTEX_MOD_IDX_MDP_PAD2,
	[MDP_COMP_PAD3] = MUTEX_MOD_IDX_MDP_PAD3,
	[MDP_COMP_TCC0] = MUTEX_MOD_IDX_MDP_TCC0,
	[MDP_COMP_WROT0] = MUTEX_MOD_IDX_MDP_WROT0,
	[MDP_COMP_WROT2] = MUTEX_MOD_IDX_MDP_WROT2,
	[MDP_COMP_WROT3] = MUTEX_MOD_IDX_MDP_WROT3,
};

static const struct of_device_id mt8188_sub_comp_dt_ids[] = {
	{}
};

enum mt8188_mdp_comp_id {
	/* MT8188 Comp id */
	/* ISP */
	MT8188_MDP_COMP_WPEI = 0,
	MT8188_MDP_COMP_WPEO,           /* 1 */

	/* MDP */
	MT8188_MDP_COMP_CAMIN,          /* 2 */
	MT8188_MDP_COMP_RDMA0,          /* 3 */
	MT8188_MDP_COMP_RDMA2,          /* 4 */
	MT8188_MDP_COMP_RDMA3,          /* 5 */
	MT8188_MDP_COMP_FG0,            /* 6 */
	MT8188_MDP_COMP_FG2,            /* 7 */
	MT8188_MDP_COMP_FG3,            /* 8 */
	MT8188_MDP_COMP_TO_SVPP2MOUT,   /* 9 */
	MT8188_MDP_COMP_TO_SVPP3MOUT,   /* 10 */
	MT8188_MDP_COMP_TO_WARP0MOUT,   /* 11 */
	MT8188_MDP_COMP_VPP0_SOUT,      /* 12 */
	MT8188_MDP_COMP_VPP1_SOUT,      /* 13 */
	MT8188_MDP_COMP_PQ0_SOUT,       /* 14 */
	MT8188_MDP_COMP_HDR0,           /* 15 */
	MT8188_MDP_COMP_HDR2,           /* 16 */
	MT8188_MDP_COMP_HDR3,           /* 17 */
	MT8188_MDP_COMP_AAL0,           /* 18 */
	MT8188_MDP_COMP_AAL2,           /* 19 */
	MT8188_MDP_COMP_AAL3,           /* 20 */
	MT8188_MDP_COMP_RSZ0,           /* 21 */
	MT8188_MDP_COMP_RSZ2,           /* 22 */
	MT8188_MDP_COMP_RSZ3,           /* 23 */
	MT8188_MDP_COMP_TDSHP0,         /* 24 */
	MT8188_MDP_COMP_TDSHP2,         /* 25 */
	MT8188_MDP_COMP_TDSHP3,         /* 26 */
	MT8188_MDP_COMP_COLOR0,         /* 27 */
	MT8188_MDP_COMP_COLOR2,         /* 28 */
	MT8188_MDP_COMP_COLOR3,         /* 29 */
	MT8188_MDP_COMP_OVL0,           /* 30 */
	MT8188_MDP_COMP_PAD0,           /* 31 */
	MT8188_MDP_COMP_PAD2,           /* 32 */
	MT8188_MDP_COMP_PAD3,           /* 33 */
	MT8188_MDP_COMP_TCC0,           /* 34 */
	MT8188_MDP_COMP_WROT0,          /* 35 */
	MT8188_MDP_COMP_WROT2,          /* 36 */
	MT8188_MDP_COMP_WROT3,          /* 37 */
	MT8188_MDP_COMP_MERGE2,         /* 38 */
	MT8188_MDP_COMP_MERGE3,         /* 39 */
};

static const struct mdp_comp_data mt8188_mdp_comp_data[MDP_MAX_COMP_COUNT] = {
	[MDP_COMP_WPEI] = {
		{MDP_COMP_TYPE_WPEI, 0, MT8188_MDP_COMP_WPEI, 0},
		{0, 0, 0}
	},
	[MDP_COMP_WPEO] = {
		{MDP_COMP_TYPE_EXTO, 0, MT8188_MDP_COMP_WPEO, 0},
		{0, 0, 0}
	},
	[MDP_COMP_CAMIN] = {
		{MDP_COMP_TYPE_DL_PATH, 0, MT8188_MDP_COMP_CAMIN, 0},
		{3, 3, 0}
	},
	[MDP_COMP_RDMA0] = {
		{MDP_COMP_TYPE_RDMA, 0, MT8188_MDP_COMP_RDMA0, 0},
		{3, 0, 0}
	},
	[MDP_COMP_RDMA2] = {
		{MDP_COMP_TYPE_RDMA, 1, MT8188_MDP_COMP_RDMA2, 1},
		{3, 0, 0}
	},
	[MDP_COMP_RDMA3] = {
		{MDP_COMP_TYPE_RDMA, 2, MT8188_MDP_COMP_RDMA3, 1},
		{3, 0, 0}
	},
	[MDP_COMP_FG0] = {
		{MDP_COMP_TYPE_FG, 0, 0, MT8188_MDP_COMP_FG0, 0},
		{1, 0, 0}
	},
	[MDP_COMP_FG2] = {
		{MDP_COMP_TYPE_FG, 1, MT8188_MDP_COMP_FG2, 1},
		{1, 0, 0}
	},
	[MDP_COMP_FG3] = {
		{MDP_COMP_TYPE_FG, 2, MT8188_MDP_COMP_FG3, 1},
		{1, 0, 0}
	},
	[MDP_COMP_HDR0] = {
		{MDP_COMP_TYPE_HDR, 0, MT8188_MDP_COMP_HDR0, 0},
		{1, 0, 0}
	},
	[MDP_COMP_HDR2] = {
		{MDP_COMP_TYPE_HDR, 1, MT8188_MDP_COMP_HDR2, 1},
		{1, 0, 0}
	},
	[MDP_COMP_HDR3] = {
		{MDP_COMP_TYPE_HDR, 2, MT8188_MDP_COMP_HDR3, 1},
		{1, 0, 0}
	},
	[MDP_COMP_AAL0] = {
		{MDP_COMP_TYPE_AAL, 0, MT8188_MDP_COMP_AAL0, 0},
		{1, 0, 0}
	},
	[MDP_COMP_AAL2] = {
		{MDP_COMP_TYPE_AAL, 1, MT8188_MDP_COMP_AAL2, 1},
		{1, 0, 0}
	},
	[MDP_COMP_AAL3] = {
		{MDP_COMP_TYPE_AAL, 2, MT8188_MDP_COMP_AAL3, 1},
		{1, 0, 0}
	},
	[MDP_COMP_RSZ0] = {
		{MDP_COMP_TYPE_RSZ, 0, MT8188_MDP_COMP_RSZ0, 0},
		{1, 0, 0}
	},
	[MDP_COMP_RSZ2] = {
		{MDP_COMP_TYPE_RSZ, 1, MT8188_MDP_COMP_RSZ2, 1},
		{2, 0, 0}
	},
	[MDP_COMP_RSZ3] = {
		{MDP_COMP_TYPE_RSZ, 2, MT8188_MDP_COMP_RSZ3, 1},
		{2, 0, 0}
	},
	[MDP_COMP_TDSHP0] = {
		{MDP_COMP_TYPE_TDSHP, 0, MT8188_MDP_COMP_TDSHP0, 0},
		{1, 0, 0}
	},
	[MDP_COMP_TDSHP2] = {
		{MDP_COMP_TYPE_TDSHP, 1, MT8188_MDP_COMP_TDSHP2, 1},
		{1, 0, 0}
	},
	[MDP_COMP_TDSHP3] = {
		{MDP_COMP_TYPE_TDSHP, 2, MT8188_MDP_COMP_TDSHP3, 1},
		{1, 0, 0}
	},
	[MDP_COMP_COLOR0] = {
		{MDP_COMP_TYPE_COLOR, 0, MT8188_MDP_COMP_COLOR0, 0},
		{1, 0, 0}
	},
	[MDP_COMP_COLOR2] = {
		{MDP_COMP_TYPE_COLOR, 1, MT8188_MDP_COMP_COLOR2, 1},
		{1, 0, 0}
	},
	[MDP_COMP_COLOR3] = {
		{MDP_COMP_TYPE_COLOR, 2, MT8188_MDP_COMP_COLOR3, 1},
		{1, 0, 0}
	},
	[MDP_COMP_OVL0] = {
		{MDP_COMP_TYPE_OVL, 0, MT8188_MDP_COMP_OVL0, 0},
		{1, 0, 0}
	},
	[MDP_COMP_PAD0] = {
		{MDP_COMP_TYPE_PAD, 0, MT8188_MDP_COMP_PAD0, 0},
		{1, 0, 0}
	},
	[MDP_COMP_PAD2] = {
		{MDP_COMP_TYPE_PAD, 1, MT8188_MDP_COMP_PAD2, 1},
		{1, 0, 0}
	},
	[MDP_COMP_PAD3] = {
		{MDP_COMP_TYPE_PAD, 2, MT8188_MDP_COMP_PAD3, 1},
		{1, 0, 0}
	},
	[MDP_COMP_TCC0] = {
		{MDP_COMP_TYPE_TCC, 0, MT8188_MDP_COMP_TCC0, 0},
		{1, 0, 0}
	},
	[MDP_COMP_WROT0] = {
		{MDP_COMP_TYPE_WROT, 0, MT8188_MDP_COMP_WROT0, 0},
		{1, 0, 0}
	},
	[MDP_COMP_WROT2] = {
		{MDP_COMP_TYPE_WROT, 1, MT8188_MDP_COMP_WROT2, 1},
		{1, 0, 0}
	},
	[MDP_COMP_WROT3] = {
		{MDP_COMP_TYPE_WROT, 2, MT8188_MDP_COMP_WROT3, 1},
		{1, 0, 0}
	},
	[MDP_COMP_MERGE2] = {
		{MDP_COMP_TYPE_MERGE, 0, MT8188_MDP_COMP_MERGE2, 1},
		{1, 0, 0}
	},
	[MDP_COMP_MERGE3] = {
		{MDP_COMP_TYPE_MERGE, 1, MT8188_MDP_COMP_MERGE3, 1},
		{1, 0, 0}
	},
	[MDP_COMP_PQ0_SOUT] = {
		{MDP_COMP_TYPE_DUMMY, 0, MT8188_MDP_COMP_PQ0_SOUT, 0},
		{0, 0, 0}
	},
	[MDP_COMP_TO_WARP0MOUT] = {
		{MDP_COMP_TYPE_DUMMY, 1, MT8188_MDP_COMP_TO_WARP0MOUT, 0},
		{0, 0, 0}
	},
	[MDP_COMP_TO_SVPP2MOUT] = {
		{MDP_COMP_TYPE_DUMMY, 2, MT8188_MDP_COMP_TO_SVPP2MOUT, 1},
		{0, 0, 0}
	},
	[MDP_COMP_TO_SVPP3MOUT] = {
		{MDP_COMP_TYPE_DUMMY, 3, MT8188_MDP_COMP_TO_SVPP3MOUT, 1},
		{0, 0, 0}
	},
	[MDP_COMP_VPP0_SOUT] = {
		{MDP_COMP_TYPE_PATH, 0, MT8188_MDP_COMP_VPP0_SOUT, 1},
		{2, 6, 0}
	},
	[MDP_COMP_VPP1_SOUT] = {
		{MDP_COMP_TYPE_PATH, 1, MT8188_MDP_COMP_VPP1_SOUT, 0},
		{2, 8, 0}
	},
};

/*
 * All 10-bit related formats are not added in the basic format list,
 * please add the corresponding format settings before use.
 */
static const struct mdp_format mt8188_formats[] = {
	{
		.pixelformat	= V4L2_PIX_FMT_GREY,
		.mdp_color	= MDP_COLOR_GREY,
		.depth		= { 8 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB565X,
		.mdp_color	= MDP_COLOR_BGR565,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.mdp_color	= MDP_COLOR_RGB565,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB24,
		.mdp_color	= MDP_COLOR_RGB888,
		.depth		= { 24 },
		.row_depth	= { 24 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_BGR24,
		.mdp_color	= MDP_COLOR_BGR888,
		.depth		= { 24 },
		.row_depth	= { 24 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_ABGR32,
		.mdp_color	= MDP_COLOR_BGRA8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_ARGB32,
		.mdp_color	= MDP_COLOR_ARGB8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.mdp_color	= MDP_COLOR_UYVY,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_VYUY,
		.mdp_color	= MDP_COLOR_VYUY,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.mdp_color	= MDP_COLOR_YUYV,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVYU,
		.mdp_color	= MDP_COLOR_YVYU,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUV420,
		.mdp_color	= MDP_COLOR_I420,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU420,
		.mdp_color	= MDP_COLOR_YV12,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.mdp_color	= MDP_COLOR_NV12,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.mdp_color	= MDP_COLOR_NV21,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.mdp_color	= MDP_COLOR_NV16,
		.depth		= { 16 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV61,
		.mdp_color	= MDP_COLOR_NV61,
		.depth		= { 16 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV24,
		.mdp_color	= MDP_COLOR_NV24,
		.depth		= { 24 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV42,
		.mdp_color	= MDP_COLOR_NV42,
		.depth		= { 24 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.mdp_color	= MDP_COLOR_NV12,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_MM21,
		.mdp_color	= MDP_COLOR_420_BLK,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 6,
		.halign		= 6,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.mdp_color	= MDP_COLOR_NV21,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV16M,
		.mdp_color	= MDP_COLOR_NV16,
		.depth		= { 8, 8 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV61M,
		.mdp_color	= MDP_COLOR_NV61,
		.depth		= { 8, 8 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.mdp_color	= MDP_COLOR_I420,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.mdp_color	= MDP_COLOR_YV12,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUV422M,
		.mdp_color	= MDP_COLOR_I422,
		.depth		= { 8, 4, 4 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU422M,
		.mdp_color	= MDP_COLOR_YV16,
		.depth		= { 8, 4, 4 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}
};

static const struct mdp_limit mt8188_mdp_def_limit = {
	.out_limit = {
		.wmin	= 64,
		.hmin	= 64,
		.wmax	= 8192,
		.hmax	= 8192,
	},
	.cap_limit = {
		.wmin	= 64,
		.hmin	= 64,
		.wmax	= 8192,
		.hmax	= 8192,
	},
	.h_scale_up_max = 64,
	.v_scale_up_max = 64,
	.h_scale_down_max = 128,
	.v_scale_down_max = 128,
};

#endif  /* __MDP3_PLAT_MT8188_H__ */
