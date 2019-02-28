/*
 * drivers/amlogic/media/deinterlace/deinterlace_hw.h
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

#ifndef _DI_HW_H
#define _DI_HW_H
#include <linux/amlogic/media/amvecm/amvecm.h>
#include "pulldown_drv.h"
#include "nr_drv.h"

/* if post size < 80, filter of ei can't work */
#define MIN_POST_WIDTH  80
#define MIN_BLEND_WIDTH  27

#define	SKIP_CTRE_NUM	13

enum eAFBC_REG {
	eAFBC_ENABLE,			/*0x1ae0*/
	eAFBC_MODE,				/*0x1ae1*/
	eAFBC_SIZE_IN,			/*0x1ae2*/
	eAFBC_DEC_DEF_COLOR,	/*0x1ae3*/
	eAFBC_CONV_CTRL,		/*0x1ae4*/
	eAFBC_LBUF_DEPTH,		/*0x1ae5*/
	eAFBC_HEAD_BADDR,		/*0x1ae6*/
	eAFBC_BODY_BADDR,		/*0x1ae7*/
	eAFBC_SIZE_OUT,			/*0x1ae8*/
	eAFBC_OUT_YSCOPE,		/*0x1ae9*/
	eAFBC_STAT,				/*0x1aea*/
	eAFBC_VD_CFMT_CTRL,		/*0x1aeb*/
	eAFBC_VD_CFMT_W,		/*0x1aec*/
	eAFBC_MIF_HOR_SCOPE,	/*0x1aed*/
	eAFBC_MIF_VER_SCOPE,	/*0x1aee*/
	eAFBC_PIXEL_HOR_SCOPE,	/*0x1aef*/
	eAFBC_PIXEL_VER_SCOPE,	/*0x1af0*/
	eAFBC_VD_CFMT_H,		/*0x1af1*/
};

enum eAFBC_DEC {
	eAFBC_DEC0,
	eAFBC_DEC1,
};
#define AFBC_REG_INDEX_NUB	(18)
#define AFBC_DEC_NUB		(2)

struct DI_MIF_s {
	unsigned short	luma_x_start0;
	unsigned short	luma_x_end0;
	unsigned short	luma_y_start0;
	unsigned short	luma_y_end0;
	unsigned short	chroma_x_start0;
	unsigned short	chroma_x_end0;
	unsigned short	chroma_y_start0;
	unsigned short	chroma_y_end0;
	unsigned int	nocompress;
	unsigned		set_separate_en:2;
	unsigned		src_field_mode:1;
	unsigned		src_prog:1;
	unsigned		video_mode:1;
	unsigned		output_field_num:1;
	unsigned		bit_mode:2;
	/*
	 * unsigned		burst_size_y:2; set 3 as default
	 * unsigned		burst_size_cb:2;set 1 as default
	 * unsigned		burst_size_cr:2;set 1 as default
	 */
	unsigned		canvas0_addr0:8;
	unsigned		canvas0_addr1:8;
	unsigned		canvas0_addr2:8;
};

struct DI_SIM_MIF_s {
	unsigned short	start_x;
	unsigned short	end_x;
	unsigned short	start_y;
	unsigned short	end_y;
	unsigned short	canvas_num;
	unsigned short	bit_mode;
};

struct DI_MC_MIF_s {
	unsigned short start_x;
	unsigned short start_y;
	unsigned short end_y;
	unsigned short size_x;
	unsigned short size_y;
	unsigned short canvas_num;
	unsigned short blend_en;
	unsigned short vecrd_offset;
};

enum gate_mode_e {
	GATE_AUTO,
	GATE_ON,
	GATE_OFF,
};

struct mcinfo_lmv_s {
	unsigned char lock_flag;
	char			lmv;
	unsigned short lock_cnt;
};

struct di_pq_parm_s {
	struct am_pq_parm_s pq_parm;
	struct am_reg_s *regs;
	struct list_head list;
};

extern u32 afbc_disable_flag;
void read_pulldown_info(unsigned int *glb_frm_mot_num,
	unsigned int *glb_fid_mot_num);
void read_new_pulldown_info(struct FlmModReg_t *pFMRegp);
void pulldown_info_clear_g12a(void);
void combing_pd22_window_config(unsigned int width, unsigned int height);
void di_hw_init(bool pulldown_en, bool mc_enable);
void di_hw_uninit(void);
void enable_di_pre_aml(
	struct DI_MIF_s		   *di_inp_mif,
	struct DI_MIF_s		   *di_mem_mif,
	struct DI_MIF_s		   *di_chan2_mif,
	struct DI_SIM_MIF_s    *di_nrwr_mif,
	struct DI_SIM_MIF_s    *di_mtnwr_mif,
	struct DI_SIM_MIF_s    *di_contp2rd_mif,
	struct DI_SIM_MIF_s    *di_contprd_mif,
	struct DI_SIM_MIF_s    *di_contwr_mif,
	unsigned char madi_en, unsigned char pre_field_num,
	unsigned char pre_vdin_link);
u32 enable_afbc_input(struct vframe_s *vf);

void mc_pre_mv_irq(void);
void enable_mc_di_pre(struct DI_MC_MIF_s *di_mcinford_mif,
	struct DI_MC_MIF_s *di_mcinfowr_mif,
	struct DI_MC_MIF_s *di_mcvecwr_mif,
	unsigned char mcdi_en);
void enable_mc_di_pre_g12(struct DI_MC_MIF_s *di_mcinford_mif,
	struct DI_MC_MIF_s *di_mcinfowr_mif,
	struct DI_MC_MIF_s *di_mcvecwr_mif,
	unsigned char mcdi_en);

void enable_mc_di_post(struct DI_MC_MIF_s *di_mcvecrd_mif,
	int urgent, bool reverse, int invert_mv);
void enable_mc_di_post_g12(struct DI_MC_MIF_s *di_mcvecrd_mif,
	int urgent, bool reverse, int invert_mv);

void disable_post_deinterlace_2(void);
void initial_di_post_2(int hsize_post, int vsize_post,
	int hold_line, bool write_en);
void enable_di_post_2(
	struct DI_MIF_s		*di_buf0_mif,
	struct DI_MIF_s		*di_buf1_mif,
	struct DI_MIF_s		*di_buf2_mif,
	struct DI_SIM_MIF_s	*di_diwr_mif,
	struct DI_SIM_MIF_s	*di_mtnprd_mif,
	int ei_en, int blend_en, int blend_mtn_en, int blend_mode,
	int di_vpp_en, int di_ddr_en,
	int post_field_num, int hold_line, int urgent,
	int invert_mv, int vskip_cnt
);
void di_post_switch_buffer(
	struct DI_MIF_s		*di_buf0_mif,
	struct DI_MIF_s		*di_buf1_mif,
	struct DI_MIF_s		*di_buf2_mif,
	struct DI_SIM_MIF_s	*di_diwr_mif,
	struct DI_SIM_MIF_s	*di_mtnprd_mif,
	struct DI_MC_MIF_s		*di_mcvecrd_mif,
	int ei_en, int blend_en, int blend_mtn_en, int blend_mode,
	int di_vpp_en, int di_ddr_en,
	int post_field_num, int hold_line, int urgent,
	int invert_mv, bool pd_en, bool mc_enable,
	int vskip_cnt
);
void di_post_read_reverse_irq(bool reverse,
	unsigned char mc_pre_flag, bool mc_enable);
void di_top_gate_control(bool top_en, bool mc_en);
void di_pre_gate_control(bool enable, bool mc_enable);
void di_post_gate_control(bool gate);
void diwr_set_power_control(unsigned char enable);
void di_hw_disable(bool mc_enable);
void enable_di_pre_mif(bool enable, bool mc_enable);
void enable_di_post_mif(enum gate_mode_e mode);
void di_hw_uninit(void);
void combing_pd22_window_config(unsigned int width, unsigned int height);
void calc_lmv_init(void);
void calc_lmv_base_mcinfo(unsigned int vf_height, unsigned short *mcinfo_vadr);
void init_field_mode(unsigned short height);
void film_mode_win_config(unsigned int width, unsigned int height);
void pulldown_vof_win_config(struct pulldown_detected_s *wins);
void di_load_regs(struct di_pq_parm_s *di_pq_ptr);
void pre_frame_reset_g12(unsigned char madi_en, unsigned char mcdi_en);
void pre_frame_reset(void);
void di_interrupt_ctrl(unsigned char ma_en,
	unsigned char det3d_en, unsigned char nrds_en,
	unsigned char post_wr, unsigned char mc_en);
void di_txl_patch_prog(int prog_flg, unsigned int cnt, bool mc_en);
bool afbc_is_supported(void);
//extern void afbc_power_sw(bool on);
extern void afbc_reg_sw(bool on);
/*extern void afbc_sw_trig(bool  on);*/
extern void afbc_sw(bool on);
extern void afbc_input_sw(bool on);
extern void dump_vd2_afbc(void);
extern int afbc_reg_unreg_flag;

extern u8 *di_vmap(ulong addr, u32 size, bool *bflg);
extern void di_unmap_phyaddr(u8 *vaddr);
extern int di_print(const char *fmt, ...);

#define DI_MC_SW_OTHER	(1<<0)
#define DI_MC_SW_REG	(1<<1)
//#define DI_MC_SW_POST	(1<<2)
#define DI_MC_SW_IC	(1<<2)

#define DI_MC_SW_ON_MASK	(DI_MC_SW_REG | DI_MC_SW_OTHER | DI_MC_SW_IC)

extern void di_patch_post_update_mc(void);
extern void di_patch_post_update_mc_sw(unsigned int cmd, bool on);

extern void di_rst_protect(bool on);
extern void di_pre_nr_wr_done_sel(bool on);
extern void di_arb_sw(bool on);

#endif
