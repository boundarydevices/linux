/*
 * drivers/amlogic/media/common/vpu/vpu_reg.h
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

#ifndef __VPU_REG_H__
#define __VPU_REG_H__
#include <linux/amlogic/iomap.h>
#include "vpu.h"

/*extern void __iomem *reg_base_aobus;*/
/*extern void __iomem *reg_base_cbus;*/

/* ********************************
 * register define
 * *********************************
 */

#define AO_RTI_GEN_PWR_SLEEP0        (0x03a << 2)
#define AO_RTI_GEN_PWR_ISO0          (0x03b << 2)

/* HHI bus */
#define HHI_GP1_PLL_CNTL             0x16
#define HHI_GP1_PLL_CNTL2            0x17
#define HHI_GP1_PLL_CNTL3            0x18
#define HHI_GP1_PLL_CNTL4            0x19

#define HHI_MEM_PD_REG0              0x40
#define HHI_VPU_MEM_PD_REG0          0x41
#define HHI_VPU_MEM_PD_REG1          0x42
#define HHI_VPU_MEM_PD_REG2          0x4d
#define HHI_VPU_MEM_PD_REG3          0x4e
#define HHI_VPU_MEM_PD_REG4          0x4c
#define HHI_VPU_MEM_PD_REG3_SM1      0x43
#define HHI_VPU_MEM_PD_REG4_SM1      0x44

#define HHI_VPU_CLKC_CNTL            0x6d
#define HHI_VPU_CLK_CNTL             0x6f
#define HHI_VPU_CLKB_CNTL            0x83
#define HHI_VAPBCLK_CNTL             0x7d

/* cbus */
#define RESET0_REGISTER              0x1101
#define RESET1_REGISTER              0x1102
#define RESET2_REGISTER              0x1103
#define RESET3_REGISTER              0x1104
#define RESET4_REGISTER              0x1105
#define RESET5_REGISTER              0x1106
#define RESET6_REGISTER              0x1107
#define RESET7_REGISTER              0x1108
#define RESET0_MASK                  0x1110
#define RESET1_MASK                  0x1111
#define RESET2_MASK                  0x1112
#define RESET3_MASK                  0x1113
#define RESET4_MASK                  0x1114
#define RESET5_MASK                  0x1115
#define RESET6_MASK                  0x1116
#define RESET7_MASK                  0x1118
#define RESET0_LEVEL                 0x1120
#define RESET1_LEVEL                 0x1121
#define RESET2_LEVEL                 0x1122
#define RESET3_LEVEL                 0x1123
#define RESET4_LEVEL                 0x1124
#define RESET5_LEVEL                 0x1125
#define RESET6_LEVEL                 0x1126
#define RESET7_LEVEL                 0x1127

#define RESET0_REGISTER_TXLX         0x0401
#define RESET1_REGISTER_TXLX         0x0402
#define RESET2_REGISTER_TXLX         0x0403
#define RESET3_REGISTER_TXLX         0x0404
#define RESET4_REGISTER_TXLX         0x0405
#define RESET5_REGISTER_TXLX         0x0406
#define RESET6_REGISTER_TXLX         0x0407
#define RESET7_REGISTER_TXLX         0x0408
#define RESET0_MASK_TXLX             0x0410
#define RESET1_MASK_TXLX             0x0411
#define RESET2_MASK_TXLX             0x0412
#define RESET3_MASK_TXLX             0x0413
#define RESET4_MASK_TXLX             0x0414
#define RESET5_MASK_TXLX             0x0415
#define RESET6_MASK_TXLX             0x0416
#define RESET7_MASK_TXLX             0x0418
#define RESET0_LEVEL_TXLX            0x0420
#define RESET1_LEVEL_TXLX            0x0421
#define RESET2_LEVEL_TXLX            0x0422
#define RESET3_LEVEL_TXLX            0x0423
#define RESET4_LEVEL_TXLX            0x0424
#define RESET5_LEVEL_TXLX            0x0425
#define RESET6_LEVEL_TXLX            0x0426
#define RESET7_LEVEL_TXLX            0x0427

/* vpu clk gate */
/* hiu_bus */
#define HHI_GCLK_OTHER               0x54
/* vcbus */
#define VPU_CLK_GATE                 0x2723

#define VDIN0_OFFSET                 0x00
#define VDIN1_OFFSET                 0x80
#define VDIN_COM_GCLK_CTRL           0x121b
#define VDIN_COM_GCLK_CTRL2          0x1270
#define VDIN0_COM_GCLK_CTRL          (VDIN0_OFFSET + VDIN_COM_GCLK_CTRL)
#define VDIN0_COM_GCLK_CTRL2         (VDIN0_OFFSET + VDIN_COM_GCLK_CTRL2)
#define VDIN1_COM_GCLK_CTRL          (VDIN1_OFFSET + VDIN_COM_GCLK_CTRL)
#define VDIN1_COM_GCLK_CTRL2         (VDIN1_OFFSET + VDIN_COM_GCLK_CTRL2)

#define DI_CLKG_CTRL                 0x1718

#define VPP_GCLK_CTRL0               0x1d72
#define VPP_GCLK_CTRL1               0x1d73
#define VPP_SC_GCLK_CTRL             0x1d74
#define VPP_SRSCL_GCLK_CTRL          0x1d77
#define VPP_OSDSR_GCLK_CTRL          0x1d78
#define VPP_XVYCC_GCLK_CTRL          0x1d79

#define DOLBY_TV_CLKGATE_CTRL        0x33f1
#define DOLBY_CORE1_CLKGATE_CTRL     0x33f2
#define DOLBY_CORE2A_CLKGATE_CTRL    0x3432
#define DOLBY_CORE3_CLKGATE_CTRL     0x36f0

#define VPU_RDARB_MODE_L1C1          0x2790
#define VPU_RDARB_MODE_L1C2          0x2799
#define VPU_RDARB_MODE_L2C1          0x279d
#define VPU_WRARB_MODE_L2C1          0x27a2


extern unsigned int vpu_hiu_read(unsigned int _reg);
extern void vpu_hiu_write(unsigned int _reg, unsigned int _value);
extern void vpu_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len);
extern unsigned int vpu_hiu_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len);
extern void vpu_hiu_set_mask(unsigned int _reg, unsigned int _mask);
extern void vpu_hiu_clr_mask(unsigned int _reg, unsigned int _mask);

extern unsigned int vpu_vcbus_read(unsigned int _reg);
extern void vpu_vcbus_write(unsigned int _reg, unsigned int _value);
extern void vpu_vcbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len);
extern unsigned int vpu_vcbus_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len);
extern void vpu_vcbus_set_mask(unsigned int _reg, unsigned int _mask);
extern void vpu_vcbus_clr_mask(unsigned int _reg, unsigned int _mask);

extern unsigned int vpu_ao_read(unsigned int _reg);
extern void vpu_ao_write(unsigned int _reg, unsigned int _value);
extern void vpu_ao_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len);

extern unsigned int vpu_cbus_read(unsigned int _reg);
extern void vpu_cbus_write(unsigned int _reg, unsigned int _value);
extern void vpu_cbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len);
extern void vpu_cbus_set_mask(unsigned int _reg, unsigned int _mask);
extern void vpu_cbus_clr_mask(unsigned int _reg, unsigned int _mask);

#endif
