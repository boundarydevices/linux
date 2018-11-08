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

enum status_sel {
	CURRENT_DDR_ADDR,
	NEXT_FINISH_ADDR,
	COUNT_CURRENT_DDR_ACK,
	COUNT_NEXT_FINISH_DDR_ACK,
	VAD_WAKEUP_ADDR,   /* from tl1, vad */
	VAD_FS_ADDR,
	VAD_FIFO_CNT,
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

#if 0
struct ddr_desc {
	/* start address of DDR */
	unsigned int start;
	/* finish address of DDR */
	unsigned int finish;
	/* interrupt address or counts of DDR blocks */
	unsigned int intrpt;
	/* fifo total counts */
	unsigned int fifo_depth;
	/* fifo start threshold */
	unsigned int fifo_thr;
	enum ddr_types data_type;
	unsigned int edian;
	unsigned int pp_mode;
	//unsigned int reg_base;
	struct clk *ddr;
	struct clk *ddr_arb;
};
#endif

struct ddr_chipinfo {
	/* INT and Start address is same or separated */
	bool int_start_same_addr;
	/* force finished */
	bool force_finished;
	/* same source */
	bool same_src_fn;
	/* insert channel number */
	bool insert_chnum;

	/* ddr bus in urgent */
	bool ugt;

	/* source sel switch to ctrl1
	 * for toddr, 0: source sel is controlled by ctrl0
	 *            1: source sel is controlled by ctrl1
	 * for frddr, 0: source sel is controlled by ctrl0
	 *            1: source sel is controlled by ctrl2
	 */
	bool src_sel_ctrl;

	/*
	 * resample source sel switch
	 * resample : from ctrl0 to ctrl3
	 * toddr : from ctrl0 to ctrl1
	 */
	bool asrc_src_sel_ctrl;
	/* spdif in 32bit, only support left justified */
	bool asrc_only_left_j;

	/* toddr number max
	 * 0: default, 3 toddr, axg, g12a, g12b
	 * 4: 4 toddr, tl1
	 */
	int fifo_num;

	/* power detect or VAD
	 * 0: disabled
	 * 1: power detect
	 * 2: vad
	 */
	int wakeup;
};

struct toddr {
	//struct ddr_desc dscrpt;
	struct device *dev;
	unsigned int resample: 1;
	unsigned int ext_signed: 1;
	unsigned int msb_bit;
	unsigned int lsb_bit;
	unsigned int reg_base;
	unsigned int bitdepth;
	unsigned int channels;
	unsigned int rate;

	unsigned int start_addr;
	unsigned int end_addr;

	enum toddr_src src;
	unsigned int fifo_id;

	unsigned int asrc_src_sel;

	int is_lb; /* check whether for loopback */
	int irq;
	bool in_use: 1;
	struct aml_audio_controller *actrl;
	struct ddr_chipinfo *chipinfo;
};

enum status {
	DISABLED,
	READY,    /* controls has set enable, but ddr is not in running */
	RUNNING,
};

struct toddr_attach {
	bool enable;
	int id;
	int status;
	/* which module should be attached,
	 * check which toddr in use should be attached
	 */
	enum toddr_src attach_module;
};

struct frddr_attach {
	bool enable;
	int status;
	/* which module for attach ,
	 * check which frddr in use should be added
	 */
	enum frddr_dest attach_module;
};

struct frddr {
	//struct ddr_desc dscrpt;
	struct device *dev;
	enum frddr_dest dest;
	struct aml_audio_controller *actrl;
	unsigned int reg_base;
	unsigned int fifo_id;

	unsigned int channels;
	unsigned int msb;
	unsigned int type;

	int irq;
	bool in_use;
	struct ddr_chipinfo *chipinfo;
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
int aml_toddr_set_buf_startaddr(struct toddr *to, unsigned int start);
int aml_toddr_set_buf_endaddr(struct toddr *to,	unsigned int end);

int aml_toddr_set_intrpt(struct toddr *to, unsigned int intrpt);
unsigned int aml_toddr_get_position(struct toddr *to);
unsigned int aml_toddr_get_addr(struct toddr *to, enum status_sel sel);
void aml_toddr_select_src(struct toddr *to, enum toddr_src);
void aml_toddr_enable(struct toddr *to, bool enable);
void aml_toddr_set_fifos(struct toddr *to, unsigned int thresh);
void aml_toddr_update_fifos_rd_th(struct toddr *to, int th);
void aml_toddr_force_finish(struct toddr *to);
void aml_toddr_set_format(struct toddr *to, struct toddr_fmt *fmt);
void aml_toddr_insert_chanum(struct toddr *to);
unsigned int aml_toddr_read(struct toddr *to);
void aml_toddr_write(struct toddr *to, unsigned int val);

/* resample */
void aml_set_resample(int id, bool enable, int resample_module);
/* power detect */
void aml_pwrdet_enable(bool enable, int pwrdet_module);
/* Voice Activity Detection */
void aml_set_vad(bool enable, int module);

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
	unsigned int chnum,
	unsigned int msb,
	unsigned int frddr_type);
/* audio eq drc */
void aml_set_aed(bool enable, int aed_module);

void frddr_init_without_mngr(unsigned int frddr_index, unsigned int src0_sel);
void frddr_deinit_without_mngr(unsigned int frddr_index);

int toddr_src_get(void);
const char *toddr_src_get_str(int idx);
int frddr_src_get(void);
const char *frddr_src_get_str(int idx);

int card_add_ddr_kcontrols(struct snd_soc_card *card);

void pm_audio_set_suspend(bool is_suspend);
bool pm_audio_is_suspend(void);

#endif

