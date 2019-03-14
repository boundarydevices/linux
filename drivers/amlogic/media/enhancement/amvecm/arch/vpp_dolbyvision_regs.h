/*
 * drivers/amlogic/media/enhancement/amvecm/arch/vpp_dolbyvision_regs.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#ifndef VPP_DOLBYVISION_REGS_H
#define VPP_DOLBYVISION_REGS_H

#define CORE1_OFFSET             (0x1UL << 24)
#define CORE1_1_OFFSET           (0x1UL << 25)
#define CORE2A_OFFSET            (0x1UL << 26)
#define CORE3_OFFSET             (0x1UL << 27)
#define CORETV_OFFSET            (0x1UL << 28)

#define DOLBY_CORE1_REG_START		(0x00 + CORE1_OFFSET)
#define DOLBY_CORE1_CLKGATE_CTRL	(0xf2 + CORE1_OFFSET)
#define DOLBY_CORE1_SWAP_CTRL0		(0xf3 + CORE1_OFFSET)
#define DOLBY_CORE1_SWAP_CTRL1		(0xf4 + CORE1_OFFSET)
#define DOLBY_CORE1_SWAP_CTRL2		(0xf5 + CORE1_OFFSET)
#define DOLBY_CORE1_SWAP_CTRL3		(0xf6 + CORE1_OFFSET)
#define DOLBY_CORE1_SWAP_CTRL4		(0xf7 + CORE1_OFFSET)
#define DOLBY_CORE1_SWAP_CTRL5		(0xf8 + CORE1_OFFSET)
#define DOLBY_CORE1_DMA_CTRL		(0xf9 + CORE1_OFFSET)
#define DOLBY_CORE1_DMA_STATUS		(0xfa + CORE1_OFFSET)
#define DOLBY_CORE1_STATUS0		(0xfb + CORE1_OFFSET)
#define DOLBY_CORE1_STATUS1		(0xfc + CORE1_OFFSET)
#define DOLBY_CORE1_STATUS2		(0xfd + CORE1_OFFSET)
#define DOLBY_CORE1_STATUS3		(0xfe + CORE1_OFFSET)
#define DOLBY_CORE1_DMA_PORT		(0xff + CORE1_OFFSET)

#define DOLBY_CORE1_1_REG_START		(0x00 + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_CLKGATE_CTRL	(0xf2 + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_SWAP_CTRL0	(0xf3 + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_SWAP_CTRL1	(0xf4 + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_SWAP_CTRL2	(0xf5 + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_SWAP_CTRL3	(0xf6 + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_SWAP_CTRL4	(0xf7 + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_SWAP_CTRL5	(0xf8 + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_DMA_CTRL		(0xf9 + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_DMA_STATUS	(0xfa + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_STATUS0		(0xfb + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_STATUS1		(0xfc + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_STATUS2		(0xfd + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_STATUS3		(0xfe + CORE1_1_OFFSET)
#define DOLBY_CORE1_1_DMA_PORT		(0xff + CORE1_1_OFFSET)

#define DOLBY_CORE2A_REG_START		(0x00 + CORE2A_OFFSET)
#define DOLBY_CORE2A_CTRL		(0x01 + CORE2A_OFFSET)
#define DOLBY_CORE2A_CLKGATE_CTRL	(0x32 + CORE2A_OFFSET)
#define DOLBY_CORE2A_SWAP_CTRL0		(0x33 + CORE2A_OFFSET)
#define DOLBY_CORE2A_SWAP_CTRL1		(0x34 + CORE2A_OFFSET)
#define DOLBY_CORE2A_SWAP_CTRL2		(0x35 + CORE2A_OFFSET)
#define DOLBY_CORE2A_SWAP_CTRL3		(0x36 + CORE2A_OFFSET)
#define DOLBY_CORE2A_SWAP_CTRL4		(0x37 + CORE2A_OFFSET)
#define DOLBY_CORE2A_SWAP_CTRL5		(0x38 + CORE2A_OFFSET)
#define DOLBY_CORE2A_DMA_CTRL		(0x39 + CORE2A_OFFSET)
#define DOLBY_CORE2A_DMA_STATUS		(0x3a + CORE2A_OFFSET)
#define DOLBY_CORE2A_STATUS0		(0x3b + CORE2A_OFFSET)
#define DOLBY_CORE2A_STATUS1		(0x3c + CORE2A_OFFSET)
#define DOLBY_CORE2A_STATUS2		(0x3d + CORE2A_OFFSET)
#define DOLBY_CORE2A_STATUS3		(0x3e + CORE2A_OFFSET)
#define DOLBY_CORE2A_DMA_PORT		(0x3f + CORE2A_OFFSET)

#define DOLBY_CORE3_REG_START		(0x00 + CORE3_OFFSET)
#define DOLBY_CORE3_CLKGATE_CTRL	(0xf0 + CORE3_OFFSET)
#define DOLBY_CORE3_SWAP_CTRL0		(0xf1 + CORE3_OFFSET)
#define DOLBY_CORE3_SWAP_CTRL1		(0xf2 + CORE3_OFFSET)
#define DOLBY_CORE3_SWAP_CTRL2		(0xf3 + CORE3_OFFSET)
#define DOLBY_CORE3_SWAP_CTRL3		(0xf4 + CORE3_OFFSET)
#define DOLBY_CORE3_SWAP_CTRL4		(0xf5 + CORE3_OFFSET)
#define DOLBY_CORE3_SWAP_CTRL5		(0xf6 + CORE3_OFFSET)
#define DOLBY_CORE3_SWAP_CTRL6		(0xf7 + CORE3_OFFSET)
#define DOLBY_CORE3_DIAG_CTRL		(0xf8 + CORE3_OFFSET)
#define DOLBY_CORE3_CRC_CTRL		(0xfb + CORE3_OFFSET)
#define DOLBY_CORE3_INPUT_CSC_CRC	(0xfc + CORE3_OFFSET)
#define DOLBY_CORE3_OUTPUT_CSC_CRC	(0xfd + CORE3_OFFSET)

#define DOLBY_TV_REG_START		(0x00 + CORETV_OFFSET)
#define DOLBY_TV_CLKGATE_CTRL		(0xf1 + CORETV_OFFSET)
#define DOLBY_TV_SWAP_CTRL0		(0xf2 + CORETV_OFFSET)
#define DOLBY_TV_SWAP_CTRL1		(0xf3 + CORETV_OFFSET)
#define DOLBY_TV_SWAP_CTRL2		(0xf4 + CORETV_OFFSET)
#define DOLBY_TV_SWAP_CTRL3		(0xf5 + CORETV_OFFSET)
#define DOLBY_TV_SWAP_CTRL4		(0xf6 + CORETV_OFFSET)
#define DOLBY_TV_SWAP_CTRL5		(0xf7 + CORETV_OFFSET)
#define DOLBY_TV_SWAP_CTRL6		(0xf8 + CORETV_OFFSET)
#define DOLBY_TV_SWAP_CTRL7		(0xf9 + CORETV_OFFSET)
#define DOLBY_TV_AXI2DMA_CTRL0		(0xfa + CORETV_OFFSET)
#define DOLBY_TV_AXI2DMA_CTRL1		(0xfb + CORETV_OFFSET)
#define DOLBY_TV_AXI2DMA_CTRL2		(0xfc + CORETV_OFFSET)
#define DOLBY_TV_AXI2DMA_CTRL3		(0xfd + CORETV_OFFSET)
#define DOLBY_TV_STATUS0		(0xfe + CORETV_OFFSET)
#define DOLBY_TV_STATUS1		(0xff + CORETV_OFFSET)


#define VPP_WRAP_OSD1_MATRIX_EN_CTRL    0x3d6d
#define VPP_WRAP_OSD2_MATRIX_EN_CTRL    0x3d7d
#define VPP_WRAP_OSD3_MATRIX_EN_CTRL    0x3dbd
#define DOLBY_PATH_CTRL			0x1a0c
#define VIU_MISC_CTRL1			0x1a07
#define VPP_DOLBY_CTRL			0x1d93
#define VIU_SW_RESET			0x1a01
#define VPU_HDMI_FMT_CTRL		0x2743
#if 0
/* core 1 display manager 24 registers */
struct dm_register_ipcore_1_s {
	uint32_t SRange;
	uint32_t Srange_Inverse;
	uint32_t Frame_Format_1;
	uint32_t Frame_Format_2;
	uint32_t Frame_Pixel_Def;
	uint32_t Y2RGB_Coefficient_1;
	uint32_t Y2RGB_Coefficient_2;
	uint32_t Y2RGB_Coefficient_3;
	uint32_t Y2RGB_Coefficient_4;
	uint32_t Y2RGB_Coefficient_5;
	uint32_t Y2RGB_Offset_1;
	uint32_t Y2RGB_Offset_2;
	uint32_t Y2RGB_Offset_3;
	uint32_t EOTF;
	uint32_t A2B_Coefficient_1;
	uint32_t A2B_Coefficient_2;
	uint32_t A2B_Coefficient_3;
	uint32_t A2B_Coefficient_4;
	uint32_t A2B_Coefficient_5;
	uint32_t C2D_Coefficient_1;
	uint32_t C2D_Coefficient_2;
	uint32_t C2D_Coefficient_3;
	uint32_t C2D_Coefficient_4;
	uint32_t C2D_Coefficient_5;
};

/* core 1 composer 173 registers */
struct composer_register_ipcore_s {
	uint32_t Composer_Mode;
	uint32_t VDR_Resolution;
	uint32_t Bit_Depth;
	uint32_t Coefficient_Log2_Denominator;
	uint32_t BL_Num_Pivots_Y;
	uint32_t BL_Pivot[5];
	uint32_t BL_Order;
	uint32_t BL_Coefficient_Y[8][3];
	uint32_t EL_NLQ_Offset_Y;
	uint32_t EL_Coefficient_Y[3];
	uint32_t Mapping_IDC_U;
	uint32_t BL_Num_Pivots_U;
	uint32_t BL_Pivot_U[3];
	uint32_t BL_Order_U;
	uint32_t BL_Coefficient_U[4][3];
	uint32_t MMR_Coefficient_U[22][2];
	uint32_t MMR_Order_U;
	uint32_t EL_NLQ_Offset_U;
	uint32_t EL_Coefficient_U[3];
	uint32_t Mapping_IDC_V;
	uint32_t BL_Num_Pivots_V;
	uint32_t BL_Pivot_V[3];
	uint32_t BL_Order_V;
	uint32_t BL_Coefficient_V[4][3];
	uint32_t MMR_Coefficient_V[22][2];
	uint32_t MMR_Order_V;
	uint32_t EL_NLQ_Offset_V;
	uint32_t EL_Coefficient_V[3];
};

/* core 2 display manager 24 registers */
struct dm_register_ipcore_2_s {
	uint32_t SRange;
	uint32_t Srange_Inverse;
	uint32_t Y2RGB_Coefficient_1;
	uint32_t Y2RGB_Coefficient_2;
	uint32_t Y2RGB_Coefficient_3;
	uint32_t Y2RGB_Coefficient_4;
	uint32_t Y2RGB_Coefficient_5;
	uint32_t Y2RGB_Offset_1;
	uint32_t Y2RGB_Offset_2;
	uint32_t Y2RGB_Offset_3;
	uint32_t Frame_Format;
	uint32_t EOTF;
	uint32_t A2B_Coefficient_1;
	uint32_t A2B_Coefficient_2;
	uint32_t A2B_Coefficient_3;
	uint32_t A2B_Coefficient_4;
	uint32_t A2B_Coefficient_5;
	uint32_t C2D_Coefficient_1;
	uint32_t C2D_Coefficient_2;
	uint32_t C2D_Coefficient_3;
	uint32_t C2D_Coefficient_4;
	uint32_t C2D_Coefficient_5;
	uint32_t C2D_Offset;
	uint32_t VDR_Resolution;
};

/* core 3 display manager 26 registers */
struct dm_register_ipcore_3_s {
	uint32_t D2C_coefficient_1;
	uint32_t D2C_coefficient_2;
	uint32_t D2C_coefficient_3;
	uint32_t D2C_coefficient_4;
	uint32_t D2C_coefficient_5;
	uint32_t B2A_Coefficient_1;
	uint32_t B2A_Coefficient_2;
	uint32_t B2A_Coefficient_3;
	uint32_t B2A_Coefficient_4;
	uint32_t B2A_Coefficient_5;
	uint32_t Eotf_param_1;
	uint32_t Eotf_param_2;
	uint32_t IPT_Scale;
	uint32_t IPT_Offset_1;
	uint32_t IPT_Offset_2;
	uint32_t IPT_Offset_3;
	uint32_t Output_range_1;
	uint32_t Output_range_2;
	uint32_t RGB2YUV_coefficient_register1;
	uint32_t RGB2YUV_coefficient_register2;
	uint32_t RGB2YUV_coefficient_register3;
	uint32_t RGB2YUV_coefficient_register4;
	uint32_t RGB2YUV_coefficient_register5;
	uint32_t RGB2YUV_offset_0;
	uint32_t RGB2YUV_offset_1;
	uint32_t RGB2YUV_offset_2;
};

/* lut 5 * 256 for core 1 and core 2 */
struct dm_lut_ipcore_s {
	uint32_t TmLutI[64*4];
	uint32_t TmLutS[64*4];
	uint32_t SmLutI[64*4];
	uint32_t SmLutS[64*4];
	uint32_t G2L[256];
};

/* core 3 metadata 128 registers */
struct md_reister_ipcore_3_s {
	uint32_t raw_metadata[128];
};
#endif

#endif
