/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_CAMSV30_REGS_H__
#define __MTK_CAMSV30_REGS_H__

/* CAMSV */
#define CAMSV_MODULE_EN				0x0000
#define CAMSV_FMT_SEL				0x0004
#define CAMSV_INT_EN				0x0008
#define CAMSV_INT_STATUS			0x000c
#define CAMSV_SW_CTL				0x0010
#define CAMSV_IMGO_FBC				0x001C
#define CAMSV_CLK_EN				0x0020
#define CAMSV_PAK				0x003c

/* CAMSV_TG */
#define CAMSV_TG_SEN_MODE			0x0010
#define CAMSV_TG_VF_CON				0x0014
#define CAMSV_TG_SEN_GRAB_PXL			0x0018
#define CAMSV_TG_SEN_GRAB_LIN			0x001c
#define CAMSV_TG_PATH_CFG			0x0020

/* CAMSV_IMG0 */
#define CAMSV_IMGO_SV_BASE_ADDR			0x0000
#define CAMSV_IMGO_SV_XSIZE			0x0008
#define CAMSV_IMGO_SV_YSIZE			0x000c
#define CAMSV_IMGO_SV_STRIDE			0x0010
#define CAMSV_IMGO_SV_CON			0x0014
#define CAMSV_IMGO_SV_CON2			0x0018

#define CAMSV_TG_SEN_MODE_CMOS_EN		BIT(0)
#define CAMSV_TG_VF_CON_VFDATA_EN		BIT(0)

/* CAMSV_CLK_EN bits */
#define CAMSV_TG_DP_CLK_EN			BIT(0)
#define CAMSV_PAK_DP_CLK_EN			BIT(2)
#define CAMSV_DMA_DP_CLK_EN			BIT(15)

/* CAMSV_SW_CTL bits */
#define CAMSV_IMGO_RST_TRIG			BIT(0)
#define CAMSV_IMGO_RST_ST			BIT(1)
#define CAMSV_SW_RST				BIT(2)

/* IRQ BITS */
#define CAMSV_IRQ_VS1				BIT(0)
#define CAMSV_IRQ_TG1				BIT(1)
#define CAMSV_IRQ_TG2				BIT(2)
#define CAMSV_IRQ_EXPDON1			BIT(3)
#define CAMSV_IRQ_TG_ERR			BIT(4)
#define CAMSV_IRQ_TG_GBERR			BIT(5)
#define CAMSV_IRQ_DROP				BIT(6)
#define CAMSV_IRQ_SOF1				BIT(7)
#define CAMSV_IRQ_PASS1_DON			BIT(10)
#define CAMSV_IRQ_IMGO_ERR			BIT(16)
#define CAMSV_IRQ_IMGO_OVERR			BIT(17)
#define CAMSV_IRQ_IMGO_DROP			BIT(19)

#define INT_ST_MASK_CAMSV                                                      \
	(CAMSV_IRQ_PASS1_DON)

#define INT_ST_MASK_CAMSV_ERR                                                  \
	(CAMSV_IRQ_TG_ERR | CAMSV_IRQ_TG_GBERR | CAMSV_IRQ_IMGO_ERR)

#endif /* __MTK_CAMSV30_REGS_H__ */
