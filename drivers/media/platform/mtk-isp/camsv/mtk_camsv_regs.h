/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_CAMSV_REGS_H__
#define __MTK_CAMSV_REGS_H__

#define CAMSV_MODULE_EN				0x0510
#define CAMSV_CLK_EN				0x0530
#define CAMSV_INT_EN				0x0518
#define CAMSV_INT_STATUS			0x051c
#define CAMSV_FMT_SEL				0x0514
#define CAMSV_SW_CTL				0x0520
#define CAMSV_PAK				0x054c

#define CAMSV_TG_SEN_MODE			0x0230
#define CAMSV_TG_VF_CON				0x0234
#define CAMSV_TG_SEN_GRAB_PXL			0x0238
#define CAMSV_TG_SEN_GRAB_LIN			0x023c
#define CAMSV_TG_PATH_CFG			0x0240
#define CAMSV_TG_TIME_STAMP			0x02a0
#define CAMSV_TG_TIME_STAMP_CTL			0x02b0

#define CAMSV_FBC_IMGO_CTL1			0x0110
#define CAMSV_FBC_IMGO_CTL2			0x0114
#define CAMSV_FBC_IMGO_ENQ_ADDR			0x0118
#define CAMSV_IMGO_FBC				0x052c

#define CAMSV_IMGO_CON				0x003c
#define CAMSV_IMGO_CON2				0x0040
#define CAMSV_IMGO_CON3				0x0044
#define CAMSV_IMGO_XSIZE			0x0030
#define CAMSV_IMGO_YSIZE			0x0034
#define CAMSV_IMGO_STRIDE			0x0038
#define CAMSV_IMGO_FH_BASE_ADDR			0x0404
#define CAMSV_DMA_FH_EN				0x0400
#define CAMSV_DMA_RSV1				0x03b0
#define CAMSV_DMA_RSV6				0x03c4
#define CAMSV_SPECIAL_FUN_EN			0x0018

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
#define CAMSV_IRQ_SW_PASS1_DON			BIT(20)
#define CAMSV_IRQ_TG_SOF_WAIT			BIT(21)

#define INT_ST_MASK_CAMSV                                                      \
	(CAMSV_IRQ_VS1 | CAMSV_IRQ_TG1 | CAMSV_IRQ_TG2 | CAMSV_IRQ_EXPDON1 |   \
	 CAMSV_IRQ_SOF1 | CAMSV_IRQ_PASS1_DON | CAMSV_IRQ_SW_PASS1_DON)

#define INT_ST_MASK_CAMSV_ERR                                                  \
	(CAMSV_IRQ_TG_ERR | CAMSV_IRQ_TG_GBERR | CAMSV_IRQ_IMGO_ERR)

#endif /* __MTK_CAMSV_REGS_H__ */
