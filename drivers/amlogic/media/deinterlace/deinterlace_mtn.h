/*
 * drivers/amlogic/media/deinterlace/deinterlace_mtn.h
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

struct combing_param_s {
	unsigned int width;
	unsigned int height;
	enum vframe_source_type_e src_type;
	enum tvin_sig_fmt_e fmt;
	bool prog_flag;
};

struct reg_set_s {
	unsigned int adr;
	unsigned int val;
	unsigned short start;
	unsigned short len;
};
#define reg_set_t struct reg_set_s

#define REG_SET_MAX_NUM 128
#define FMT_MAX_NUM		32
struct reg_cfg_ {
	struct reg_cfg_ *next;
	unsigned int source_types_enable;
	/* each bit corresponds to one source type */
	unsigned int pre_post_type; /* pre, 0; post, 1 */
	unsigned int dtv_definition_type;
	/*high definition,0; stand definition ,1;common,2*/
	unsigned int sig_fmt_range[FMT_MAX_NUM];
	/* {bit[31:16]~bit[15:0]}, include bit[31:16] and bit[15:0]  */
	reg_set_t reg_set[REG_SET_MAX_NUM];
};
#define reg_cfg_t struct reg_cfg_
extern int last_lev;
extern int dejaggy_enable;
void adpative_combing_config(unsigned int width, unsigned int height,
		enum vframe_source_type_e src_type, bool prog,
		enum tvin_sig_fmt_e fmt);
int adaptive_combing_fixing(
	unsigned int field_diff, unsigned int frame_diff,
	int cur_lev, int bit_mode, int cmb_cnt,
	int like_pulldown22_flag, unsigned int *frame_diff_avg);
void adpative_combing_exit(void);
#ifdef CONFIG_AM_ATVDEMOD
extern int aml_atvdemod_get_snr_ex(void);
#endif
void di_apply_reg_cfg(unsigned char pre_post_type);
void di_add_reg_cfg_init(void);

