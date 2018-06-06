/*
 * sound/soc/amlogic/auge/ddr_mngr.h
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

#ifndef __AML_AUDIO_DDR_MANAGER_H_
#define __AML_AUDIO_DDR_MANAGER_H_

#include <linux/device.h>
#include <linux/interrupt.h>
#include "audio_io.h"

enum ddr_num {
	DDR_A,
	DDR_B,
	DDR_C,
};

enum ddr_types {
	LJ_8BITS,
	LJ_16BITS,
	RJ_16BITS,
	LJ_32BITS,
	RJ_32BITS,
};

enum toddr_src {
	TDMIN_A,
	TDMIN_B,
	TDMIN_C,
	SPDIFIN,
	PDMIN,
	NONE,
	TDMIN_LB,
	LOOPBACK,
};

enum frddr_dest {
	TDMOUT_A,
	TDMOUT_B,
	TDMOUT_C,
	SPDIFOUT,
	SPDIFOUT_B,
};

/* to ddrs */
int fetch_toddr_index_by_src(int toddr_src);
struct toddr *fetch_toddr_by_src(int toddr_src);
struct toddr *aml_audio_register_toddr(struct device *dev,
		struct aml_audio_controller *actrl,
		irq_handler_t handler, void *data);
int aml_audio_unregister_toddr(struct device *dev, void *data);
int aml_toddr_set_buf(struct toddr *to, unsigned int start,
			unsigned int end);
int aml_toddr_set_intrpt(struct toddr *to, unsigned int intrpt);
unsigned int aml_toddr_get_position(struct toddr *to);
void aml_toddr_select_src(struct toddr *to, enum toddr_src);
void aml_toddr_enable(struct toddr *to, bool enable);
void aml_toddr_set_fifos(struct toddr *to, unsigned int thresh);
void aml_toddr_set_format(struct toddr *to,
		unsigned int type, unsigned int msb, unsigned int lsb,
		int ch_num, int bit_depth);

/* resample */
void aml_resample_enable(bool enable, int resample_module);
/* power detect */
void aml_pwrdet_enable(bool enable, int pwrdet_module);

/* from ddrs */
int fetch_frddr_index_by_src(int frddr_src);
struct frddr *fetch_frddr_by_src(int frddr_src);

struct frddr *aml_audio_register_frddr(struct device *dev,
		struct aml_audio_controller *actrl,
		irq_handler_t handler, void *data);
int aml_audio_unregister_frddr(struct device *dev, void *data);
int aml_frddr_set_buf(struct frddr *fr, unsigned int start,
			unsigned int end);
int aml_frddr_set_intrpt(struct frddr *fr, unsigned int intrpt);
unsigned int aml_frddr_get_position(struct frddr *fr);
void aml_frddr_enable(struct frddr *fr, bool enable);
void aml_frddr_select_dst(struct frddr *fr, enum frddr_dest);
extern void aml_frddr_select_dst_ss(struct frddr *fr,
	enum frddr_dest dst, int sel, bool enable);

int aml_check_sharebuffer_valid(struct frddr *fr, int ss_sel);

void aml_frddr_set_fifos(struct frddr *fr,
		unsigned int depth, unsigned int thresh);
unsigned int aml_frddr_get_fifo_id(struct frddr *fr);
/* audio eq drc */
void aml_aed_enable(bool enable, int aed_module);

void frddr_init_default(unsigned int frddr_index, unsigned int src0_sel);
#endif

