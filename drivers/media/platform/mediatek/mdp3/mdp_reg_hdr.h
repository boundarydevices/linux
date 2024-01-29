/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MDP_REG_HDR_H__
#define __MDP_REG_HDR_H__

#define MDP_HDR_TOP			(0x000)
#define MDP_HDR_RELAY			(0x004)
#define MDP_HDR_SIZE_0			(0x014)
#define MDP_HDR_SIZE_1			(0x018)
#define MDP_HDR_SIZE_2			(0x01C)
#define MDP_HDR_HIST_CTRL_0		(0x020)
#define MDP_HDR_HIST_CTRL_1		(0x024)
#define MDP_HDR_3x3_COEF_00		(0x038)
#define MDP_HDR_B_CHANNEL_NR		(0x0D8)
#define MDP_HDR_A_LUMINANCE		(0x0E4)
#define MDP_HDR_LBOX_DET_1		(0x0F4)
#define MDP_HDR_CURSOR_COLOR		(0x10C)
#define MDP_HDR_Y2R_09			(0x178)
#define MDP_TONE_MAP_TOP		(0x1B0)
#define	MDP_HDR_L_MIX_0			(0x1C0)
#define MDP_HDR_HLG_SG			(0x1E0)
#define MDP_HDR_R2Y_00			(0x128)
#define MDP_HDR_R2Y_01			(0x12C)
#define MDP_HDR_R2Y_02			(0x130)
#define MDP_HDR_R2Y_03			(0x134)
#define MDP_HDR_R2Y_04			(0x138)
#define MDP_HDR_R2Y_05			(0x13C)
#define MDP_HDR_R2Y_06			(0x140)
#define MDP_HDR_R2Y_07			(0x144)
#define MDP_HDR_R2Y_08			(0x148)
#define MDP_HDR_R2Y_09			(0x14C)
#define MDP_HDR_HIST_ADDR		(0x0DC)
#define MDP_HDR_TILE_POS		(0x118)

/* MASK */
#define MDP_HDR_RELAY_MASK		(0x01)
#define MDP_HDR_TOP_MASK		(0xFF0FEB6D)
#define MDP_HDR_SIZE_0_MASK		(0x1FFF1FFF)
#define MDP_HDR_SIZE_1_MASK		(0x1FFF1FFF)
#define MDP_HDR_SIZE_2_MASK		(0x1FFF1FFF)
#define MDP_HDR_HIST_CTRL_0_MASK	(0x1FFF1FFF)
#define MDP_HDR_HIST_CTRL_1_MASK	(0x1FFF1FFF)
#define MDP_HDR_HIST_ADDR_MASK		(0xBF3F2F3F)
#define MDP_HDR_TILE_POS_MASK		(0x1FFF1FFF)
#define MDP_HDR_HLG_SG_MASK		(0x0000FFFF)
#define MDP_HDR_R2Y_00_MASK		(0xFFFFFFFF)
#define MDP_HDR_R2Y_01_MASK		(0xFFFFFFFF)
#define MDP_HDR_R2Y_02_MASK		(0xFFFFFFFF)
#define MDP_HDR_R2Y_03_MASK		(0xFFFFFFFF)
#define MDP_HDR_R2Y_04_MASK		(0xFFFF)
#define MDP_HDR_R2Y_05_MASK		(0x1FFF1FFF)
#define MDP_HDR_R2Y_06_MASK		(0xFFFF8000)
#define MDP_HDR_R2Y_07_MASK		(0x7FF07FF)
#define MDP_HDR_R2Y_08_MASK		(0xFFFFFC00)
#define MDP_HDR_R2Y_09_MASK		(0x0F)

#endif // __MDP_REG_HDR_H__
