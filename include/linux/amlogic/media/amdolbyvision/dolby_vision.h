/*
 * include/linux/amlogic/media/amdolbyvision/dolby_vision.h
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

#ifndef _DV_H_
#define _DV_H_

#define V1_5
#define V2_4

#include <linux/types.h>

extern void enable_dolby_vision(int enable);
extern bool is_dolby_vision_enable(void);
extern bool is_dolby_vision_on(void);
extern bool for_dolby_vision_certification(void);
extern void set_dolby_vision_mode(int mode);
extern int get_dolby_vision_mode(void);
extern void dolby_vision_set_toggle_flag(int flag);
extern int dolby_vision_wait_metadata(struct vframe_s *vf);
extern int dolby_vision_pop_metadata(void);
extern int dolby_vision_update_metadata(struct vframe_s *vf);
extern int dolby_vision_process(struct vframe_s *vf, u32 display_size,
	u8 pps_state);
extern void dolby_vision_init_receiver(void *pdev);
extern void dolby_vision_vf_put(struct vframe_s *vf);
extern struct vframe_s *dolby_vision_vf_peek_el(struct vframe_s *vf);
extern void dolby_vision_dump_setting(int debug_flag);
extern void dolby_vision_dump_struct(void);
extern void enable_osd_path(int on, int shadow_mode);
extern void tv_dolby_vision_config(int config);
extern void dolby_vision_update_pq_config(
	char *pq_config_buf);
extern int dolby_vision_update_setting(void);
extern bool is_dolby_vision_stb_mode(void);
extern bool is_meson_g12(void);
extern bool is_meson_gxm(void);
extern bool is_meson_box(void);
extern bool is_meson_txlx(void);
extern bool is_meson_txlx_tvmode(void);
extern bool is_meson_txlx_stbmode(void);
extern bool is_meson_tm2(void);
extern bool is_meson_tm2_tvmode(void);
extern bool is_meson_tm2_stbmode(void);
extern bool is_meson_tvmode(void);
extern void tv_dolby_vision_crc_clear(int flag);
extern char *tv_dolby_vision_get_crc(u32 *len);
extern void tv_dolby_vision_insert_crc(bool print);
extern int dolby_vision_check_hdr10(struct vframe_s *vf);
extern void tv_dolby_vision_dma_table_modify(
	u32 tbl_id, uint64_t value);
extern void tv_dolby_vision_efuse_info(void);
extern int dolby_vision_parse_metadata(
	struct vframe_s *vf, u8 toggle_mode, bool bypass_release);
extern void dolby_vision_update_vsvdb_config(
	char *vsvdb_buf, u32 tbl_size);
extern void tv_dolby_vision_el_info(void);

extern int enable_rgb_to_yuv_matrix_for_dvll(
	int32_t on, uint32_t *coeff_orig, uint32_t bits);

extern bool is_dovi_frame(struct vframe_s *vf);
extern void update_graphic_width_height(unsigned int width,
	unsigned int height);

#endif
