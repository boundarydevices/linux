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
	DDP_COMPONENT_DP_INTF0,
	DDP_COMPONENT_DPI0,
	DDP_COMPONENT_DPI1,
	DDP_COMPONENT_DSC0,
	DDP_COMPONENT_DSC1,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_DSI1,
	DDP_COMPONENT_DSI2,
	DDP_COMPONENT_DSI3,
	DDP_COMPONENT_GAMMA,
	DDP_COMPONENT_MERGE0,
	DDP_COMPONENT_MERGE1,
	DDP_COMPONENT_MERGE2,
	DDP_COMPONENT_MERGE3,
	DDP_COMPONENT_MERGE4,
	DDP_COMPONENT_MERGE5,
	DDP_COMPONENT_OD0,
	DDP_COMPONENT_OD1,
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL_2L0,
	DDP_COMPONENT_OVL_2L1,
	DDP_COMPONENT_OVL_2L2,
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_PWM0,
	DDP_COMPONENT_PWM1,
	DDP_COMPONENT_PWM2,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_RDMA4,
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

enum mtk_mdp_pipe_id {
	MDP_PIPE_RDMA0,
	MDP_PIPE_IMGI,
	MDP_PIPE_WPEI,
	MDP_PIPE_WPEI2,
	MDP_PIPE_MAX
};

enum mtk_isp_ctrl {
	ISP_CTRL_MMSYS_SW0_RST_B,
	ISP_CTRL_MMSYS_SW1_RST_B,
	ISP_CTRL_MDP_ASYNC_CFG_WD,
	ISP_CTRL_MDP_ASYNC_IPU_CFG_WD,
	ISP_CTRL_ISP_RELAY_CFG_WD,
	ISP_CTRL_IPU_RELAY_CFG_WD,
	ISP_CTRL_MAX
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

void mtk_mmsys_mdp_isp_ctrl(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			    enum mtk_mdp_comp_id id);

void mtk_mmsys_mdp_camin_ctrl(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			      enum mtk_mdp_comp_id id,
			      u32 camin_w, u32 camin_h);

#endif /* __MTK_MMSYS_H */
