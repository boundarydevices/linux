/*
 * drivers/amlogic/media/enhancement/amvecm/amcm.h
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

#ifndef __AM_CM_H
#define __AM_CM_H


#include <linux/amlogic/media/vfm/vframe.h>
#include "linux/amlogic/media/amvecm/cm.h"

struct cm_regs_s {
	unsigned int val:32;
	unsigned int reg:14;
	unsigned int port:2;
	/* 0    NA  NA direct access */
		/* 1    VPP_CHROMA_ADDR_PORT */
				/* VPP_CHROMA_DATA_PORT CM port registers */
		/* 2    NA NA reserved */
    /* 3    NA NA reserved */
	unsigned int bit:5;
	unsigned int wid:5;
	unsigned int mode:1;
	unsigned int rsv:5;
};

struct sr1_regs_s {
	unsigned int addr;
	unsigned int mask;
	unsigned int  val;
};

extern unsigned int vecm_latch_flag;
extern unsigned int cm_size;
extern unsigned int cm2_patch_flag;
extern int cm_en; /* 0:disabel;1:enable */
extern int dnlp_en;/*0:disabel;1:enable */

extern unsigned int sr1_reg_val[101];

/* *********************************************************************** */
/* *** IOCTL-oriented functions ****************************************** */
/* *********************************************************************** */
void am_set_regmap(struct am_regs_s *p);
extern void amcm_disable(void);
extern void amcm_enable(void);
extern void amcm_level_sel(unsigned int cm_level);
extern void cm2_frame_size_patch(unsigned int width, unsigned int height);
extern void cm2_frame_switch_patch(void);
extern void cm_latch_process(void);
extern int cm_load_reg(struct am_regs_s *arg);

/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8) */
/* #define WRITE_VPP_REG(x,val) */
/* WRITE_VCBUS_REG(x,val) */
/* #define WRITE_VPP_REG_BITS(x,val,start,length) */
/* WRITE_VCBUS_REG_BITS(x,val,start,length) */
/* #define READ_VPP_REG(x) */
/* READ_VCBUS_REG(x) */
/* #define READ_VPP_REG_BITS(x,start,length) */
/* READ_VCBUS_REG_BITS(x,start,length) */
/* #else */
/* #define WRITE_VPP_REG(x,val) */
/* WRITE_CBUS_REG(x,val) */
/* #define WRITE_VPP_REG_BITS(x,val,start,length) */
/* WRITE_CBUS_REG_BITS(x,val,start,length) */
/* #define READ_VPP_REG(x) */
/* READ_CBUS_REG(x) */
/* #define READ_VPP_REG_BITS(x,start,length) */
/* READ_CBUS_REG_BITS(x,start,length) */
/* #endif */


#endif

