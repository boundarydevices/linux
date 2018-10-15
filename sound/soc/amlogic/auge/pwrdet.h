/*
 * sound/soc/amlogic/auge/pwrdet.c
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
#ifndef __PWRDET_H__
#define __PWRDET_H__

struct audio_buffer {
	/* ring buffer for wakeup */
	unsigned char  *buffer;
	unsigned int   wr;
	unsigned int   rd;
	unsigned int   size;
};

struct pwrdet_chipinfo {
	/* new address offset */
	int address_shift;
	/* match id function */
	bool matchid_fn;
};

struct aml_pwrdet {
	struct device *dev;

	unsigned int det_src;
	int irq;
	unsigned int hi_th;
	unsigned int lo_th;

	struct audio_buffer abuf;
	struct pwrdet_chipinfo *chipinfo;
};

#endif /*__PWRDET_H__*/
