/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef __MTK_MMSYS_H
#define __MTK_MMSYS_H

enum mtk_ddp_comp_id;
enum mtk_mdp_comp_id;
struct device;

struct mmsys_cmdq_cmd {
	struct cmdq_pkt *pkt;
	s32 *event;
};

enum mtk_ddp_comp_id {
	DDP_COMPONENT_AAL0,
	DDP_COMPONENT_AAL1,
	DDP_COMPONENT_BLS,
	DDP_COMPONENT_CCORR,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_DITHER,
	DDP_COMPONENT_DPI0,
	DDP_COMPONENT_DPI1,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_DSI1,
	DDP_COMPONENT_DSI2,
	DDP_COMPONENT_DSI3,
	DDP_COMPONENT_GAMMA,
	DDP_COMPONENT_OD0,
	DDP_COMPONENT_OD1,
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL_2L0,
	DDP_COMPONENT_OVL_2L1,
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_PWM0,
	DDP_COMPONENT_PWM1,
	DDP_COMPONENT_PWM2,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_UFOE,
	DDP_COMPONENT_WDMA0,
	DDP_COMPONENT_WDMA1,
	DDP_COMPONENT_ID_MAX,
};

enum mtk_mdp_comp_id {
	MDP_COMP_NONE = -1,	/* Invalid engine */

	/* ISP */
	MDP_COMP_WPEI = 0,
	MDP_COMP_WPEO,		/* 1 */
	MDP_COMP_WPEI2,		/* 2 */
	MDP_COMP_WPEO2,		/* 3 */
	MDP_COMP_ISP_IMGI,	/* 4 */
	MDP_COMP_ISP_IMGO,	/* 5 */
	MDP_COMP_ISP_IMG2O,	/* 6 */

	/* IPU */
	MDP_COMP_IPUI,		/* 7 */
	MDP_COMP_IPUO,		/* 8 */

	/* MDP */
	MDP_COMP_CAMIN,		/* 9 */
	MDP_COMP_CAMIN2,	/* 10 */
	MDP_COMP_RDMA0,		/* 11 */
	MDP_COMP_AAL0,		/* 12 */
	MDP_COMP_CCORR0,	/* 13 */
	MDP_COMP_RSZ0,		/* 14 */
	MDP_COMP_RSZ1,		/* 15 */
	MDP_COMP_TDSHP0,	/* 16 */
	MDP_COMP_COLOR0,	/* 17 */
	MDP_COMP_PATH0_SOUT,	/* 18 */
	MDP_COMP_PATH1_SOUT,	/* 19 */
	MDP_COMP_WROT0,		/* 20 */
	MDP_COMP_WDMA,		/* 21 */

	/* Dummy Engine */
	MDP_COMP_RDMA1,		/* 22 */
	MDP_COMP_RSZ2,		/* 23 */
	MDP_COMP_TDSHP1,	/* 24 */
	MDP_COMP_WROT1,		/* 25 */

	MDP_MAX_COMP_COUNT	/* ALWAYS keep at the end */
};

void mtk_mmsys_ddp_connect(struct device *dev,
			   enum mtk_ddp_comp_id cur,
			   enum mtk_ddp_comp_id next);

void mtk_mmsys_ddp_disconnect(struct device *dev,
			      enum mtk_ddp_comp_id cur,
			      enum mtk_ddp_comp_id next);

void mtk_mmsys_mdp_connect(struct device *dev,
			   struct mmsys_cmdq_cmd *cmd,
			   enum mtk_mdp_comp_id cur,
			   enum mtk_mdp_comp_id next);

void mtk_mmsys_mdp_disconnect(struct device *dev,
			      struct mmsys_cmdq_cmd *cmd,
			      enum mtk_mdp_comp_id cur,
			      enum mtk_mdp_comp_id next);

#endif /* __MTK_MMSYS_H */
