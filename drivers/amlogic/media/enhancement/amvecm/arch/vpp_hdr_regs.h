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

#endif
