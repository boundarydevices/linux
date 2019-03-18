/*
 * drivers/amlogic/media/enhancement/amvecm/amve.h
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

#ifndef __AM_VE_H
#define __AM_VE_H

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/ve.h>

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


struct ve_regs_s {
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

extern struct ve_hist_s video_ve_hist;
extern void ve_hist_gamma_reset(void);
extern unsigned int ve_size;
extern struct ve_dnlp_s am_ve_dnlp;
extern struct tcon_gamma_table_s video_gamma_table_r;
extern struct tcon_gamma_table_s video_gamma_table_g;
extern struct tcon_gamma_table_s video_gamma_table_b;
extern struct tcon_gamma_table_s video_gamma_table_ioctl_set;
extern struct tcon_gamma_table_s video_gamma_table_r_adj;
extern struct tcon_gamma_table_s video_gamma_table_g_adj;
extern struct tcon_gamma_table_s video_gamma_table_b_adj;
extern struct tcon_rgb_ogo_s     video_rgb_ogo;

extern spinlock_t vpp_lcd_gamma_lock;

extern u16 gamma_data_r[256];
extern u16 gamma_data_g[256];
extern u16 gamma_data_b[256];
extern void vpp_get_lcd_gamma_table(u32 rgb_mask);

void ve_on_vs(struct vframe_s *vf);

void ve_set_bext(struct ve_bext_s *p);
void ve_set_dnlp(struct ve_dnlp_s *p);
void ve_set_dnlp_2(void);
void ve_set_hsvs(struct ve_hsvs_s *p);
void ve_set_ccor(struct ve_ccor_s *p);
void ve_set_benh(struct ve_benh_s *p);
void ve_set_demo(struct ve_demo_s *p);
void ve_set_regs(struct ve_regs_s *p);
void ve_set_regmap(struct ve_regmap_s *p);

extern void ve_enable_dnlp(void);
extern void ve_disable_dnlp(void);

extern void vpp_enable_lcd_gamma_table(void);
extern void vpp_disable_lcd_gamma_table(void);
extern void vpp_set_lcd_gamma_table(u16 *data, u32 rgb_mask);
extern void amve_write_gamma_table(u16 *data, u32 rgb_mask);
extern void vpp_set_rgb_ogo(struct tcon_rgb_ogo_s *p);
extern void vpp_phase_lock_on_vs(unsigned int cycle,
				 unsigned int stamp,
				 bool         lock50,
				 unsigned int range_fast,
				 unsigned int range_slow);
/* #if (MESON_CPU_TYPE>=MESON_CPU_TYPE_MESON6TVD) */
extern void ve_frame_size_patch(unsigned int width, unsigned int height);
/* #endif */
extern void ve_dnlp_latch_process(void);
extern void ve_lcd_gamma_process(void);
extern void lvds_freq_process(void);
extern void ve_dnlp_param_update(void);
extern void ve_new_dnlp_param_update(void);
extern void ve_ogo_param_update(void);
extern void am_set_regmap(struct am_regs_s *p);
extern void sharpness_process(struct vframe_s *vf);
extern void amvecm_bricon_process(signed int bri_val,
				signed int cont_val, struct vframe_s *vf);
extern void amvecm_color_process(signed int sat_val,
	signed int hue_val, struct vframe_s *vf);
extern void amvecm_3d_black_process(void);
extern void amvecm_3d_sync_process(void);
extern unsigned int vecm_latch_flag;
extern unsigned int cm_size;
extern unsigned int sync_3d_h_start;
extern unsigned int sync_3d_h_end;
extern unsigned int sync_3d_v_start;
extern unsigned int sync_3d_v_end;
extern unsigned int sync_3d_polarity;
extern unsigned int sync_3d_out_inv;
extern unsigned int sync_3d_black_color;
extern unsigned int sync_3d_sync_to_vbo;


#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#else
extern u32 VSYNC_RD_MPEG_REG(u32 adr);
extern int VSYNC_WR_MPEG_REG(u32 adr, u32 val);
#endif

/* #if defined(CONFIG_ARCH_MESON2) */
/* unsigned long long ve_get_vs_cnt(void); */
/* #endif */
extern int video_rgb_ogo_xvy_mtx;

#define GAMMA_SIZE 256

extern unsigned int dnlp_sel;
extern void ve_dnlp_load_reg(void);

/*gxlx sr adaptive setting*/
extern void amve_sharpness_adaptive_setting(struct vframe_s *vf,
	unsigned int sps_h_en, unsigned int sps_v_en);
extern void amve_sharpness_init(void);
extern struct am_regs_s sr1reg_sd_scale;
extern struct am_regs_s sr1reg_hd_scale;
extern struct am_regs_s sr0reg_cvbs;
extern struct am_regs_s sr1reg_cvbs;
extern struct am_regs_s sr1reg_hv_noscale;
extern void amvecm_fresh_overscan(struct vframe_s *vf);
extern void amvecm_reset_overscan(void);
extern int vpp_set_lut3d(int enable, int bLut3DLoad,
	int *pLut3D, int bLut3DCheck);
extern void vpp_lut3d_table_init(int *pLut3D, int bitdepth);
extern void dump_plut3d_table(void);
extern void dump_plut3d_reg_table(void);

extern void amvecm_gamma_init(bool en);

#endif

