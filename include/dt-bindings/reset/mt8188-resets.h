/* SPDX-License-Identifier: (GPL-2.0+ OR BSD-3-Clause)*/
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Runyang Chen <runyang.chen@mediatek.com>
 */

#ifndef _DT_BINDINGS_RESET_CONTROLLER_MT8188
#define _DT_BINDINGS_RESET_CONTROLLER_MT8188

#define MT8188_TOPRGU_CONN_MCU_SW_RST          0
#define MT8188_TOPRGU_INFRA_GRST_SW_RST        1
#define MT8188_TOPRGU_IPU0_SW_RST              2
#define MT8188_TOPRGU_IPU1_SW_RST              3
#define MT8188_TOPRGU_IPU2_SW_RST              4
#define MT8188_TOPRGU_AUD_ASRC_SW_RST          5
#define MT8188_TOPRGU_INFRA_SW_RST             6
#define MT8188_TOPRGU_MMSYS_SW_RST             7
#define MT8188_TOPRGU_MFG_SW_RST               8
#define MT8188_TOPRGU_VENC_SW_RST              9
#define MT8188_TOPRGU_VDEC_SW_RST              10
#define MT8188_TOPRGU_CAM_VCORE_SW_RST         11
#define MT8188_TOPRGU_SCP_SW_RST               12
#define MT8188_TOPRGU_APMIXEDSYS_SW_RST        13
#define MT8188_TOPRGU_AUDIO_SW_RST             14
#define MT8188_TOPRGU_CAMSYS_SW_RST            15
#define MT8188_TOPRGU_MJC_SW_RST               16
#define MT8188_TOPRGU_PERI_SW_RST              17
#define MT8188_TOPRGU_PERI_AO_SW_RST           18
#define MT8188_TOPRGU_PCIE_SW_RST              19
#define MT8188_TOPRGU_ADSPSYS_SW_RST           21
#define MT8188_TOPRGU_DPTX_SW_RST              22
#define MT8188_TOPRGU_SPMI_MST_SW_RST          23

#define MT8188_TOPRGU_SW_RST_NUM               24

/* INFRA resets */
#define MT8188_INFRA_RST1_THERMAL_MCU_RST          0
#define MT8188_INFRA_RST1_THERMAL_CTRL_RST         1
#define MT8188_INFRA_RST3_PTP_CTRL_RST             2

/* VDOSYS1 */
#define MT8188_VDO1_RST_MERGE0_DL_ASYNC         9
#define MT8188_VDO1_RST_MERGE1_DL_ASYNC         10
#define MT8188_VDO1_RST_MERGE2_DL_ASYNC         11
#define MT8188_VDO1_RST_MERGE3_DL_ASYNC         32
#define MT8188_VDO1_RST_MERGE4_DL_ASYNC         33
#define MT8188_VDO1_RST_HDR_VDO_FE0_DL_ASYNC    64
#define MT8188_VDO1_RST_HDR_GFX_FE0_DL_ASYNC    65
#define MT8188_VDO1_RST_HDR_VDO_BE_DL_ASYNC     66
#define MT8188_VDO1_RST_HDR_VDO_FE1_DL_ASYNC    80
#define MT8188_VDO1_RST_HDR_GFX_FE1_DL_ASYNC    81

#endif  /* _DT_BINDINGS_RESET_CONTROLLER_MT8188 */
