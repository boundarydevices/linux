/*
 * include/linux/amlogic/media/vpu/vpu.h
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

#ifndef __VPU_H__
#define __VPU_H__

enum vpu_mod_e {
	VPU_VIU_OSD1 = 0,     /* reg0[1:0] //common */
	VPU_VIU_OSD2,         /* reg0[3:2] //common */
	VPU_VIU_VD1,          /* reg0[5:4] //common */
	VPU_VIU_VD2,          /* reg0[7:6] //common */
	VPU_VIU_CHROMA,       /* reg0[9:8] //common */
	VPU_VIU_OFIFO,        /* reg0[11:10] //common */
	VPU_VIU_SCALE,        /* reg0[13:12] //common */
	VPU_VIU_OSD_SCALE,    /* reg0[15:14] //common */
	VPU_VIU_VDIN0,        /* reg0[17:16] //common */
	VPU_VIU_VDIN1,        /* reg0[19:18] //common */
	VPU_PIC_ROT1,         /* reg0[21:20] */
	VPU_PIC_ROT2,         /* reg0[23:22] */
	VPU_PIC_ROT3,         /* reg0[25:24] */
	VPU_VIU_SRSCL,        /* reg0[21:20]  //G9TV, GXBB, GXTVBB */
	VPU_VIU_OSDSR,        /* reg0[23:22]  //G9TV, GXBB */
	VPU_AFBC_DEC1,        /* reg0[23:22]  //GXTVBB */
	VPU_DI_PRE,           /* reg0[27:26] //common */
	VPU_DI_POST,          /* reg0[29:28] //common */
	VPU_SHARP,            /* reg0[31:30]  //common */

	VPU_VIU2_OSD1,        /* reg1[1:0] */
	VPU_VIU2_OSD2,        /* reg1[3:2] */
	VPU_D2D3,             /* reg1[3:0]    //G9TV */
	VPU_VIU2_VD1,         /* reg1[5:4] */
	VPU_VIU2_CHROMA,      /* reg1[7:6] */
	VPU_VIU2_OFIFO,       /* reg1[9:8] */
	VPU_VIU2_SCALE,       /* reg1[11:10] */
	VPU_VIU2_OSD_SCALE,   /* reg1[13:12] */
	VPU_VDIN_AM_ASYNC,    /* reg1[15:14]  //G9TV */
	VPU_VPU_ARB,          /* reg1[15:14]  //GXBB, GXTVBB, GXL */
	VPU_VDISP_AM_ASYNC,   /* reg1[17:16]  //G9TV */
	VPU_OSD1_AFBCD,       /* reg1[17:16]  //GXTVBB */
	VPU_AFBC_DEC0,        /* reg1[17:16]  //GXTVBB */
	VPU_AFBC_DEC,         /* reg1[17:16]  //GXBB */
	VPU_VPUARB2_AM_ASYNC, /* reg1[19:18]  //G9TV */
	VPU_VENCP,            /* reg1[21:20] //common */
	VPU_VENCL,            /* reg1[23:22] //common */
	VPU_VENCI,            /* reg1[25:24] //common */
	VPU_ISP,              /* reg1[27:26] */
	VPU_CVD2,             /* reg1[29:28]  //G9TV, G9BB */
	VPU_ATV_DMD,          /* reg1[31:30]  //G9TV, G9BB */
	VPU_LDIM_STTS,        /* reg1[29:28]  //GXTVBB, GXL */
	VPU_XVYCC_LUT,        /* reg1[31:30]  //GXTVBB, GXL */

	VPU_VIU1_WM,          /* reg2[1:0]  //GXL, TXL */

	/* for clk_gate */
	VPU_VPU_TOP,
	VPU_VPU_CLKB,
	VPU_RDMA,
	VPU_MISC,      /* hs,vs,interrupt */
	VPU_VENC_DAC,
	VPU_VLOCK,
	VPU_DI,
	VPU_VPP,

	VPU_MAX,
};

/* VPU memory power down */
#define VPU_MEM_POWER_ON		0
#define VPU_MEM_POWER_DOWN		1

#define VPU_CLK_GATE_ON			1
#define VPU_CLK_GATE_OFF		0

extern unsigned int get_vpu_clk(void);
extern unsigned int get_vpu_clk_vmod(unsigned int vmod);
extern int request_vpu_clk_vmod(unsigned int vclk, unsigned int vmod);
extern int release_vpu_clk_vmod(unsigned int vmod);

extern void switch_vpu_mem_pd_vmod(unsigned int vmod, int flag);
extern int get_vpu_mem_pd_vmod(unsigned int vmod);

extern void switch_vpu_clk_gate_vmod(unsigned int vmod, int flag);
#endif
