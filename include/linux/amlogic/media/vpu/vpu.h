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

/* ************************************************ */
/* VPU module define */
/* ************************************************ */
enum vpu_mod_e {
	VPU_VIU_OSD1 = 0,     /* reg0[1:0], common */
	VPU_VIU_OSD2,         /* reg0[3:2], common */
	VPU_VIU_VD1,          /* reg0[5:4], common */
	VPU_VIU_VD2,          /* reg0[7:6], common */
	VPU_VIU_CHROMA,       /* reg0[9:8], common */
	VPU_VIU_OFIFO,        /* reg0[11:10], common */
	VPU_VIU_SCALE,        /* reg0[13:12], common */
	VPU_VIU_OSD_SCALE,    /* reg0[15:14], common */
	VPU_VIU_VDIN0,        /* reg0[17:16], common */
	VPU_VIU_VDIN1,        /* reg0[19:18], common */
	VPU_VIU_SRSCL,        /* reg0[21:20], GXBB, GXTVBB, TXLX */
	VPU_VIU_OSDSR,        /* reg0[23:22], GXBB */
	VPU_AFBC_DEC1,        /* reg0[23:22], GXTVBB, TXLX */
	VPU_VIU_DI_SCALE,     /* reg0[25:24], G12A */
	VPU_DI_PRE,           /* reg0[27:26], common */
	VPU_DI_POST,          /* reg0[29:28], common */
	VPU_SHARP,            /* reg0[31:30], common */

	VPU_VIU2,             /* reg1[3:0], reg2[23:22], SM1 */
	VPU_VIU2_OSD1,        /* reg1[1:0] */
	VPU_VIU2_OSD2,        /* reg1[3:2] */
	VPU_VIU2_VD1,         /* reg1[5:4] */
	VPU_VIU2_CHROMA,      /* reg1[7:6] */
	VPU_VIU2_OFIFO,       /* reg1[9:8] */
	VPU_VIU2_SCALE,       /* reg1[11:10] */
	VPU_VIU2_OSD_SCALE,   /* reg1[13:12] */
	VPU_VKSTONE,          /* reg1[5:4], TXLX */
	VPU_DOLBY_CORE3,      /* reg1[7:6], TXLX */
	VPU_DOLBY0,           /* reg1[9:8], TXLX */
	VPU_DOLBY1A,          /* reg1[11:10], TXLX */
	VPU_DOLBY1B,          /* reg1[13:12], TXLX */
	VPU_VPU_ARB,          /* reg1[15:14], GXBB, GXTVBB, GXL, TXLX */
	VPU_AFBC_DEC,         /* reg1[17:16], GXBB, GXTVBB, TXL, TXLX */
	VPU_OSD_AFBCD,        /* reg1[19:18], TXLX */
	VPU_VD2_SCALE,        /* reg1[19:18], G12A */
	VPU_VENCP,            /* reg1[21:20], common */
	VPU_VENCL,            /* reg1[23:22], common */
	VPU_VENCI,            /* reg1[25:24], common */
	VPU_LS_STTS,          /* reg1[27:26], tl1 */
	VPU_LDIM_STTS,        /* reg1[29:28], GXTVBB, GXL, TXL, TXLX */
	VPU_TV_DEC_CVD2,      /* reg1[29:28] */
	VPU_XVYCC_LUT,        /* reg1[31:30], GXTVBB, GXL, TXL, TXLX */
	VPU_VD2_OSD2_SCALE,   /* reg1[31:30], G12A */

	VPU_VIU_WM,           /* reg2[1:0], GXL, TXL, TXLX */
	VPU_TCON,             /* reg2[3:2], TXHD, TL1 */
	VPU_VIU_OSD3,         /* reg2[5:4], G12A */
	VPU_VIU_OSD4,         /* reg2[7:6], G12A */
	VPU_MAIL_AFBCD,       /* reg2[9:8], G12A */
	VPU_VD1_SCALE,        /* reg2[11:10], G12A */
	VPU_OSD_BLD34,        /* reg2[13:12], G12A */
	VPU_PRIME_DOLBY_RAM,  /* reg2[15:14], G12A */
	VPU_VD2_OFIFO,        /* reg2[17:16], G12A */
	VPU_DS,               /* reg2[19:18], TL1 */
	VPU_LUT3D,            /* reg2[21:20], G12B */
	VPU_VIU2_OSD_ROT,     /* reg2[23:22], G12B */
	VPU_RDMA,             /* reg2[31:30], G12A */

	VPU_AXI_WR1,          /* reg4[1:0], TL1 */
	VPU_AXI_WR0,          /* reg4[3:2], TL1 */
	VPU_AFBCE,            /* reg4[5:4], TL1 */

	VPU_MOD_MAX,

	/* for clk_gate */
	VPU_VPU_TOP,
	VPU_VPU_CLKB,
	VPU_CLK_VIB,
	VPU_CLK_B_REG_LATCH,
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
