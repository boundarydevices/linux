/*
 * drivers/amlogic/media/enhancement/amvecm/arch/vpp_hdr_regs.h
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

#ifndef VPP_HDR_REGS_H
#define VPP_HDR_REGS_H

#define XVYCC_LUT_R_ADDR_PORT	0x315e
#define XVYCC_LUT_R_DATA_PORT	0x315f
#define XVYCC_LUT_G_ADDR_PORT	0x3160
#define XVYCC_LUT_G_DATA_PORT	0x3161
#define XVYCC_LUT_B_ADDR_PORT	0x3162
#define XVYCC_LUT_B_DATA_PORT	0x3163
#define XVYCC_INV_LUT_CTL		0x3164
#define XVYCC_LUT_CTL			0x3165

#define XVYCC_INV_LUT_Y_ADDR_PORT     0x3158
#define XVYCC_INV_LUT_Y_DATA_PORT     0x3159

extern struct am_regs_s r_lut_hdr_hdr;
extern struct am_regs_s r_lut_sdr_sdr;
extern struct am_regs_s r_lut_hdr_sdr_level1;
extern struct am_regs_s r_lut_hdr_sdr_level2;
extern struct am_regs_s r_lut_hdr_sdr_level3;

#define VD1_HDR2_CTRL                      0x3800
#define VD1_HDR2_CLK_GATE                  0x3801
#define VD1_HDR2_MATRIXI_COEF00_01         0x3802
#define VD1_HDR2_MATRIXI_COEF02_10         0x3803
#define VD1_HDR2_MATRIXI_COEF11_12         0x3804
#define VD1_HDR2_MATRIXI_COEF20_21         0x3805
#define VD1_HDR2_MATRIXI_COEF22            0x3806
#define VD1_HDR2_MATRIXI_COEF30_31         0x3807
#define VD1_HDR2_MATRIXI_COEF32_40         0x3808
#define VD1_HDR2_MATRIXI_COEF41_42         0x3809
#define VD1_HDR2_MATRIXI_OFFSET0_1         0x380a
#define VD1_HDR2_MATRIXI_OFFSET2           0x380b
#define VD1_HDR2_MATRIXI_PRE_OFFSET0_1     0x380c
#define VD1_HDR2_MATRIXI_PRE_OFFSET2       0x380d
#define VD1_HDR2_MATRIXO_COEF00_01         0x380e
#define VD1_HDR2_MATRIXO_COEF02_10         0x380f
#define VD1_HDR2_MATRIXO_COEF11_12         0x3810
#define VD1_HDR2_MATRIXO_COEF20_21         0x3811
#define VD1_HDR2_MATRIXO_COEF22            0x3812
#define VD1_HDR2_MATRIXO_COEF30_31         0x3813
#define VD1_HDR2_MATRIXO_COEF32_40         0x3814
#define VD1_HDR2_MATRIXO_COEF41_42         0x3815
#define VD1_HDR2_MATRIXO_OFFSET0_1         0x3816
#define VD1_HDR2_MATRIXO_OFFSET2           0x3817
#define VD1_HDR2_MATRIXO_PRE_OFFSET0_1     0x3818
#define VD1_HDR2_MATRIXO_PRE_OFFSET2       0x3819
#define VD1_HDR2_MATRIXI_CLIP              0x381a
#define VD1_HDR2_MATRIXO_CLIP              0x381b
#define VD1_HDR2_CGAIN_OFFT                0x381c
#define VD1_EOTF_LUT_ADDR_PORT             0x381e
#define VD1_EOTF_LUT_DATA_PORT             0x381f
#define VD1_OETF_LUT_ADDR_PORT             0x3820
#define VD1_OETF_LUT_DATA_PORT             0x3821
#define VD1_CGAIN_LUT_ADDR_PORT            0x3822
#define VD1_CGAIN_LUT_DATA_PORT            0x3823
#define VD1_HDR2_CGAIN_COEF0               0x3824
#define VD1_HDR2_CGAIN_COEF1               0x3825
#define VD1_OGAIN_LUT_ADDR_PORT            0x3826
#define VD1_OGAIN_LUT_DATA_PORT            0x3827
#define VD1_HDR2_ADPS_CTRL                 0x3828
#define VD1_HDR2_ADPS_ALPHA0               0x3829
#define VD1_HDR2_ADPS_ALPHA1               0x382a
#define VD1_HDR2_ADPS_BETA0                0x382b
#define VD1_HDR2_ADPS_BETA1                0x382c
#define VD1_HDR2_ADPS_BETA2                0x382d
#define VD1_HDR2_ADPS_COEF0                0x382e
#define VD1_HDR2_ADPS_COEF1                0x382f
#define VD1_HDR2_GMUT_CTRL                 0x3830
#define VD1_HDR2_GMUT_COEF0                0x3831
#define VD1_HDR2_GMUT_COEF1                0x3832
#define VD1_HDR2_GMUT_COEF2                0x3833
#define VD1_HDR2_GMUT_COEF3                0x3834
#define VD1_HDR2_GMUT_COEF4                0x3835
#define VD1_HDR2_PIPE_CTRL1                0x3836
#define VD1_HDR2_PIPE_CTRL2                0x3837
#define VD1_HDR2_PIPE_CTRL3                0x3838
#define VD1_HDR2_PROC_WIN1                 0x3839
#define VD1_HDR2_PROC_WIN2                 0x383a
#define VD1_HDR2_MATRIXI_EN_CTRL           0x383b
#define VD1_HDR2_MATRIXO_EN_CTRL           0x383c

#define OSD1_HDR2_CTRL                      0x38a0
#define OSD1_HDR2_CLK_GATE                  0x38a1
#define OSD1_HDR2_MATRIXI_COEF00_01         0x38a2
#define OSD1_HDR2_MATRIXI_COEF02_10         0x38a3
#define OSD1_HDR2_MATRIXI_COEF11_12         0x38a4
#define OSD1_HDR2_MATRIXI_COEF20_21         0x38a5
#define OSD1_HDR2_MATRIXI_COEF22            0x38a6
#define OSD1_HDR2_MATRIXI_COEF30_31         0x38a7
#define OSD1_HDR2_MATRIXI_COEF32_40         0x38a8
#define OSD1_HDR2_MATRIXI_COEF41_42         0x38a9
#define OSD1_HDR2_MATRIXI_OFFSET0_1         0x38aa
#define OSD1_HDR2_MATRIXI_OFFSET2           0x38ab
#define OSD1_HDR2_MATRIXI_PRE_OFFSET0_1     0x38ac
#define OSD1_HDR2_MATRIXI_PRE_OFFSET2       0x38ad
#define OSD1_HDR2_MATRIXO_COEF00_01         0x38ae
#define OSD1_HDR2_MATRIXO_COEF02_10         0x38af
#define OSD1_HDR2_MATRIXO_COEF11_12         0x38b0
#define OSD1_HDR2_MATRIXO_COEF20_21         0x38b1
#define OSD1_HDR2_MATRIXO_COEF22            0x38b2
#define OSD1_HDR2_MATRIXO_COEF30_31         0x38b3
#define OSD1_HDR2_MATRIXO_COEF32_40         0x38b4
#define OSD1_HDR2_MATRIXO_COEF41_42         0x38b5
#define OSD1_HDR2_MATRIXO_OFFSET0_1         0x38b6
#define OSD1_HDR2_MATRIXO_OFFSET2           0x38b7
#define OSD1_HDR2_MATRIXO_PRE_OFFSET0_1     0x38b8
#define OSD1_HDR2_MATRIXO_PRE_OFFSET2       0x38b9
#define OSD1_HDR2_MATRIXI_CLIP              0x38ba
#define OSD1_HDR2_MATRIXO_CLIP              0x38bb
#define OSD1_HDR2_CGAIN_OFFT                0x38bc
#define OSD1_EOTF_LUT_ADDR_PORT             0x38be
#define OSD1_EOTF_LUT_DATA_PORT             0x38bf
#define OSD1_OETF_LUT_ADDR_PORT             0x38c0
#define OSD1_OETF_LUT_DATA_PORT             0x38c1
#define OSD1_CGAIN_LUT_ADDR_PORT            0x38c2
#define OSD1_CGAIN_LUT_DATA_PORT            0x38c3
#define OSD1_HDR2_CGAIN_COEF0               0x38c4
#define OSD1_HDR2_CGAIN_COEF1               0x38c5
#define OSD1_OGAIN_LUT_ADDR_PORT            0x38c6
#define OSD1_OGAIN_LUT_DATA_PORT            0x38c7
#define OSD1_HDR2_ADPS_CTRL                 0x38c8
#define OSD1_HDR2_ADPS_ALPHA0               0x38c9
#define OSD1_HDR2_ADPS_ALPHA1               0x38ca
#define OSD1_HDR2_ADPS_BETA0                0x38cb
#define OSD1_HDR2_ADPS_BETA1                0x38cc
#define OSD1_HDR2_ADPS_BETA2                0x38cd
#define OSD1_HDR2_ADPS_COEF0                0x38ce
#define OSD1_HDR2_ADPS_COEF1                0x38cf
#define OSD1_HDR2_GMUT_CTRL                 0x38d0
#define OSD1_HDR2_GMUT_COEF0                0x38d1
#define OSD1_HDR2_GMUT_COEF1                0x38d2
#define OSD1_HDR2_GMUT_COEF2                0x38d3
#define OSD1_HDR2_GMUT_COEF3                0x38d4
#define OSD1_HDR2_GMUT_COEF4                0x38d5
#define OSD1_HDR2_PIPE_CTRL1                0x38d6
#define OSD1_HDR2_PIPE_CTRL2                0x38d7
#define OSD1_HDR2_PIPE_CTRL3                0x38d8
#define OSD1_HDR2_PROC_WIN1                 0x38d9
#define OSD1_HDR2_PROC_WIN2                 0x38da
#define OSD1_HDR2_MATRIXI_EN_CTRL           0x38db
#define OSD1_HDR2_MATRIXO_EN_CTRL           0x38dc
#endif
