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
#include <sound/soc.h>
#include "audio_io.h"

enum ddr_num {
	DDR_A,
	DDR_B,
	DDR_C,
	DDR_D,
};

enum ddr_types {
	LJ_8BITS,
	LJ_16BITS,
	RJ_16BITS,
	LJ_32BITS,
	RJ_32BITS,
};

/*
 * from tl1, add new source FRATV, FRHDMIRX, LOOPBACK_B, SPDIFIN_LB, VAD
 */
enum toddr_src {
	TDMIN_A,
	TDMIN_B,
	TDMIN_C,
	SPDIFIN,
	PDMIN,
	FRATV, /* NONE for axg, g12a, g12b */
	TDMIN_LB,
	LOOPBACK_A,
	FRHDMIRX, /* from tl1 chipset*/
	LOOPBACK_B,
	SPDIFIN_LB,
	VAD,
};

enum resample_src {
	ASRC_TODDR_A,
	ASRC_TODDR_B,
	ASRC_TODDR_C,
	ASRC_TODDR_D,      /* from tl1 chipset */
	ASRC_LOOPBACK_A,
	ASRC_LOOPBACK_B,
};

enum frddr_dest {
	TDMOUT_A,
	TDMOUT_B,
	TDMOUT_C,
	SPDIFOUT_A,
	SPDIFOUT_B,
};

struct toddr_fmt {
	unsigned int type;
	unsigned int msb;
	unsigned int lsb;
	unsigned int endian;
	unsigned int bit_depth;
	unsigned int ch_num;
	unsigned int rate;
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
void aml_toddr_set_format(struct toddr *to, struct toddr_fmt *fmt);
void aml_toddr_insert_chanum(struct toddr *to);
unsigned int aml_toddr_read(struct toddr *to);
void aml_toddr_write(struct toddr *to, unsigned int val);

/* resample */
void aml_set_resample(int id, bool enable, int resample_module);
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
void aml_frddr_set_format(struct frddr *fr,
	unsigned int msb, unsigned int frddr_type);
/* audio eq drc */
void aml_set_aed(bool enable, int aed_module);

void frddr_init_without_mngr(unsigned int frddr_index, unsigned int src0_sel);
void frddr_deinit_without_mngr(unsigned int frddr_index);

int toddr_src_get(void);
const char *toddr_src_get_str(int idx);
int frddr_src_get(void);
const char *frddr_src_get_str(int idx);

int card_add_ddr_kcontrols(struct snd_soc_card *card);

#endif

