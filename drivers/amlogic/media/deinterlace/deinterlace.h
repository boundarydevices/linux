/*
 * drivers/amlogic/media/deinterlace/deinterlace.h
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

#ifndef _DI_H
#define _DI_H
#include <linux/cdev.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/atomic.h>
#include "film_vof_soft.h"
/* di hardware version m8m2*/
#define NEW_DI_V1 0x00000002 /* from m6tvc */
#define NEW_DI_V2 0x00000004 /* from m6tvd */
#define NEW_DI_V3 0x00000008 /* from gx */
#define NEW_DI_V4 0x00000010 /* dnr added */

/*trigger_pre_di_process param*/
#define TRIGGER_PRE_BY_PUT		'p'
#define TRIGGER_PRE_BY_DE_IRQ		'i'
#define TRIGGER_PRE_BY_UNREG		'u'
/*di_timer_handle*/
#define TRIGGER_PRE_BY_TIMER		't'
#define TRIGGER_PRE_BY_FORCE_UNREG	'f'
#define TRIGGER_PRE_BY_VFRAME_READY	'r'
#define TRIGGER_PRE_BY_PROVERDER_UNREG	'n'
#define TRIGGER_PRE_BY_DEBUG_DISABLE	'd'
#define TRIGGER_PRE_BY_TIMERC		'T'
#define TRIGGER_PRE_BY_PROVERDER_REG	'R'

#define BYPASS_GET_MAX_BUF_NUM	4
/*vframe define*/
#define vframe_t struct vframe_s

/* canvas defination */
#define DI_USE_FIXED_CANVAS_IDX

#undef USE_LIST
/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
#define NEW_KEEP_LAST_FRAME
/* #endif */
#define	DET3D
#undef SUPPORT_MPEG_TO_VDIN /* for all ic after m6c@20140731 */

/************************************
 *	 di hardware level interface
 *************************************/
#define MAX_WIN_NUM			5
/* if post size < 80, filter of ei can't work */
#define MIN_POST_WIDTH  80
#define MIN_BLEND_WIDTH  27
struct pulldown_detect_info_s {
	unsigned int field_diff;
/* total pixels difference between current field and previous field */
	unsigned int field_diff_num;
/* the number of pixels with big difference between
 * current field and previous field
 */
	unsigned int frame_diff;
/*total pixels difference between current field and previouse-previouse field*/
	unsigned int frame_diff_num;
/* the number of pixels with big difference between current
 * field and previouse-previous field
 */
	unsigned int frame_diff_skew;
/* the difference between current frame_diff and previous frame_diff */
	unsigned int frame_diff_num_skew;
/* the difference between current frame_diff_num and previous frame_diff_num*/
/* parameters for detection */
	unsigned int field_diff_by_pre;
	unsigned int field_diff_by_next;
	unsigned int field_diff_num_by_pre;
	unsigned int field_diff_num_by_next;
	unsigned int frame_diff_by_pre;
	unsigned int frame_diff_num_by_pre;
	unsigned int frame_diff_skew_ratio;
	unsigned int frame_diff_num_skew_ratio;
	/* matching pattern */
	unsigned int field_diff_pattern;
	unsigned int field_diff_num_pattern;
	unsigned int frame_diff_pattern;
	unsigned int frame_diff_num_pattern;
};
#define pulldown_detect_info_t struct pulldown_detect_info_s

struct pd_win_prop_s {
	unsigned int pixels_num;
};
#define pd_win_prop_t struct pd_win_prop_s
enum process_fun_index_e {
	PROCESS_FUN_NULL = 0,
	PROCESS_FUN_DI,
	PROCESS_FUN_PD,
	PROCESS_FUN_PROG,
	PROCESS_FUN_BOB
};
#define process_fun_index_t enum process_fun_index_e
enum pulldown_mode_e {
	PULL_DOWN_BLEND_0 = 0,/* buf1=dup[0] */
	PULL_DOWN_BLEND_2 = 1,/* buf1=dup[2] */
	PULL_DOWN_MTN	  = 2,/* mtn only */
	PULL_DOWN_BUF1	  = 3,/* do wave with dup[0] */
	PULL_DOWN_EI	  = 4,/* ei only */
	PULL_DOWN_NORMAL  = 5,/* normal di */
};

enum canvas_idx_e {
	NR_CANVAS,
	MTN_CANVAS,
	MV_CANVAS,
};
#define pulldown_mode_t enum pulldown_mode_e
struct di_buf_s {
#ifdef USE_LIST
	struct list_head list;
#endif
	struct vframe_s *vframe;
	int index; /* index in vframe_in_dup[] or vframe_in[],
		    * only for type of VFRAME_TYPE_IN
		    */
	int post_proc_flag; /* 0,no post di; 1, normal post di;
			     * 2, edge only; 3, dummy
			     */
	int new_format_flag;
	int type;
	int throw_flag;
	int invert_top_bot_flag;
	int seq;
	int pre_ref_count; /* none zero, is used by mem_mif,
			    * chan2_mif, or wr_buf
			    */
	int post_ref_count; /* none zero, is used by post process */
	int queue_index;
	/*below for type of VFRAME_TYPE_LOCAL */
	unsigned long nr_adr;
	int nr_canvas_idx;
	unsigned long mtn_adr;
	int mtn_canvas_idx;
#ifdef NEW_DI_V1
	unsigned long cnt_adr;
	int cnt_canvas_idx;
#endif
#ifdef NEW_DI_V3
	unsigned long mcinfo_adr;
	int mcinfo_canvas_idx;
	unsigned long mcvec_adr;
	int mcvec_canvas_idx;
	struct mcinfo_pre_s {
	unsigned int highvertfrqflg;
	unsigned int motionparadoxflg;
	unsigned int regs[26];/* reg 0x2fb0~0x2fc9 */
	} curr_field_mcinfo;
#endif
	/* blend window */
	unsigned short reg0_s;
	unsigned short reg0_e;
	unsigned short reg0_bmode;
	unsigned short reg1_s;
	unsigned short reg1_e;
	unsigned short reg1_bmode;
	unsigned short reg2_s;
	unsigned short reg2_e;
	unsigned short reg2_bmode;
	unsigned short reg3_s;
	unsigned short reg3_e;
	unsigned short reg3_bmode;
	/* tff bff check result bit[1:0]*/
	unsigned int privated;
	unsigned int canvas_config_flag;
	/* 0,configed; 1,config type 1 (prog);
	 * 2, config type 2 (interlace)
	 */
	unsigned int canvas_height;
	unsigned int canvas_width[3];/* nr/mtn/mv */
	/*bit [31~16] width; bit [15~0] height*/
	pulldown_detect_info_t field_pd_info;
	pulldown_detect_info_t win_pd_info[MAX_WIN_NUM];
	pulldown_mode_t pulldown_mode;
	process_fun_index_t process_fun_index;
	int early_process_fun_index;
	int left_right;/*1,left eye; 0,right eye in field alternative*/
	/*below for type of VFRAME_TYPE_POST*/
	struct di_buf_s *di_buf[2];
	struct di_buf_s *di_buf_dup_p[5];
	/* 0~4: n-2, n-1, n, n+1, n+2;	n is the field to display*/
	struct di_buf_s *di_wr_linked_buf;
	/* debug for di-vf-get/put
	 * 1: after get
	 * 0: after put
	 */
	atomic_t di_cnt;
};
#ifdef DET3D
extern bool det3d_en;
#endif

extern uint mtn_ctrl1;

extern pd_win_prop_t pd_win_prop[MAX_WIN_NUM];

extern int	pd_enable;

extern void di_hw_init(void);

extern void di_hw_uninit(void);

extern void enable_di_pre_mif(int enable);

extern int di_vscale_skip_count;

extern unsigned int di_force_bit_mode;

/*
 * di hardware internal
 */
#define RDMA_DET3D_IRQ				0x20
/* vdin0 rdma irq */
#define RDMA_DEINT_IRQ				0x2
#define RDMA_TABLE_SIZE                    ((PAGE_SIZE)<<1)

#if defined(CONFIG_AM_DEINTERLACE_SD_ONLY)
#define MAX_CANVAS_WIDTH				720
#define MAX_CANVAS_HEIGHT				576
#else
#define MAX_CANVAS_WIDTH				1920
#define MAX_CANVAS_HEIGHT				1088
#endif

struct DI_MIF_s {
	unsigned short	luma_x_start0;
	unsigned short	luma_x_end0;
	unsigned short	luma_y_start0;
	unsigned short	luma_y_end0;
	unsigned short	chroma_x_start0;
	unsigned short	chroma_x_end0;
	unsigned short	chroma_y_start0;
	unsigned short	chroma_y_end0;
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
	unsigned short size_x;
	unsigned short size_y;
	unsigned short canvas_num;
	unsigned short blend_mode;
	unsigned short vecrd_offset;
};
void disable_deinterlace(void);

void disable_pre_deinterlace(void);

void disable_post_deinterlace(void);

int get_di_pre_recycle_buf(void);


void disable_post_deinterlace_2(void);

void enable_film_mode_check(unsigned int width, unsigned int height,
		enum vframe_source_type_e);

void enable_di_pre_aml(
	struct DI_MIF_s		*di_inp_mif,
	struct DI_MIF_s		*di_mem_mif,
	struct DI_MIF_s		*di_chan2_mif,
	struct DI_SIM_MIF_s	*di_nrwr_mif,
	struct DI_SIM_MIF_s	*di_mtnwr_mif,
#ifdef NEW_DI_V1
	struct DI_SIM_MIF_s    *di_contp2rd_mif,
	struct DI_SIM_MIF_s    *di_contprd_mif,
	struct DI_SIM_MIF_s    *di_contwr_mif,
#endif
	int nr_en, int mtn_en, int pd32_check_en, int pd22_check_en,
	int hist_check_en, int pre_field_num, int pre_vdin_link,
	int hold_line, int urgent);
void enable_afbc_input(struct vframe_s *vf);
#ifdef NEW_DI_V3
void enable_mc_di_pre(struct DI_MC_MIF_s *di_mcinford_mif,
	struct DI_MC_MIF_s *di_mcinfowr_mif,
	struct DI_MC_MIF_s *di_mcvecwr_mif, int urgent);
void enable_mc_di_post(struct DI_MC_MIF_s *di_mcvecrd_mif,
	int urgent, bool reverse);
#endif

void read_new_pulldown_info(struct FlmModReg_t *pFMRegp);

void initial_di_pre_aml(int hsize_pre, int vsize_pre, int hold_line);

void initial_di_post_2(int hsize_post, int vsize_post, int hold_line);

void enable_di_post_2(
	struct DI_MIF_s		*di_buf0_mif,
	struct DI_MIF_s		*di_buf1_mif,
	struct DI_MIF_s		*di_buf2_mif,
	struct DI_SIM_MIF_s	*di_diwr_mif,
	#ifndef NEW_DI_V2
	struct DI_SIM_MIF_s	*di_mtncrd_mif,
	#endif
	struct DI_SIM_MIF_s	*di_mtnprd_mif,
	int ei_en, int blend_en, int blend_mtn_en, int blend_mode,
	int di_vpp_en, int di_ddr_en,
	int post_field_num, int hold_line, int urgent
	#ifndef NEW_DI_V1
	, unsigned long *reg_mtn_info
	#endif
);

void di_post_switch_buffer(
	struct DI_MIF_s		*di_buf0_mif,
	struct DI_MIF_s		*di_buf1_mif,
	struct DI_MIF_s		*di_buf2_mif,
	struct DI_SIM_MIF_s	*di_diwr_mif,
	#ifndef NEW_DI_V2
	struct DI_SIM_MIF_s	*di_mtncrd_mif,
	#endif
	struct DI_SIM_MIF_s	*di_mtnprd_mif,
	struct DI_MC_MIF_s		*di_mcvecrd_mif,
	int ei_en, int blend_en, int blend_mtn_en, int blend_mode,
	int di_vpp_en, int di_ddr_en,
	int post_field_num, int hold_line, int urgent
	#ifndef NEW_DI_V1
	, unsigned long *reg_mtn_info
	#endif
);

bool read_pulldown_info(pulldown_detect_info_t *field_pd_info,
		pulldown_detect_info_t *win_pd_info);

/* for video reverse */
void di_post_read_reverse(bool reverse);
void di_post_read_reverse_irq(bool reverse);
extern void recycle_keep_buffer(void);

/* #define DI_BUFFER_DEBUG */

#define DI_LOG_MTNINFO		0x02
#define DI_LOG_PULLDOWN		0x10
#define DI_LOG_BUFFER_STATE		0x20
#define DI_LOG_TIMESTAMP		0x100
#define DI_LOG_PRECISE_TIMESTAMP		0x200
#define DI_LOG_QUEUE		0x40
#define DI_LOG_VFRAME		0x80

extern unsigned int di_log_flag;
extern unsigned int di_debug_flag;
extern bool mcpre_en;
extern bool dnr_reg_update;
extern bool dnr_dm_en;
extern int mpeg2vdin_flag;
extern int di_vscale_skip_count_real;
extern unsigned int pulldown_enable;

extern bool post_wr_en;
extern unsigned int post_wr_surpport;

extern int cmb_adpset_cnt;

extern unsigned int field_diff_rate;
int di_print(const char *fmt, ...);


int get_current_vscale_skip_count(struct vframe_s *vf);

void di_set_power_control(unsigned char type, unsigned char enable);
void diwr_set_power_control(unsigned char enable);

unsigned char di_get_power_control(unsigned char type);
void config_di_bit_mode(vframe_t *vframe, unsigned int bypass_flag);
void combing_pd22_window_config(unsigned int width, unsigned int height);
int tff_bff_check(int height, int width);
void tbff_init(void);


void DI_Wr(unsigned int addr, unsigned int val);
void DI_Wr_reg_bits(unsigned int adr, unsigned int val,
		unsigned int start, unsigned int len);
void DI_VSYNC_WR_MPEG_REG(unsigned int addr, unsigned int val);
void DI_VSYNC_WR_MPEG_REG_BITS(unsigned int addr, unsigned int val,
	unsigned int start, unsigned int len);
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
extern void enable_rdma(int enable_flag);
extern int VSYNC_WR_MPEG_REG(u32 adr, u32 val);
extern int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
extern u32 VSYNC_RD_MPEG_REG(u32 adr);
extern bool is_vsync_rdma_enable(void);
#else
#ifndef VSYNC_WR_MPEG_REG
#define VSYNC_WR_MPEG_REG(adr, val) aml_write_vcbus(adr, val)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len)  \
		aml_vcbus_update_bits(adr, ((1<<len)-1)<<start, val<<start)
#define VSYNC_RD_MPEG_REG(adr) aml_read_vcbus(adr)
#endif
#endif


#define DI_COUNT   1
#define DI_MAP_FLAG	0x1
struct di_dev_s {
	dev_t			   devt;
	struct cdev		   cdev; /* The cdev structure */
	struct device	   *dev;
	struct platform_device	*pdev;
	struct task_struct *task;
	unsigned char	   di_event;
	unsigned int	   di_irq;
	unsigned int	   flags;
	unsigned int	   timerc_irq;
	unsigned long	   mem_start;
	unsigned int	   mem_size;
	unsigned int	   buffer_size;
	unsigned int	   buf_num_avail;
	unsigned int	   hw_version;
	int		rdma_handle;
	/* is surpport nr10bit */
	unsigned int	   nr10bit_surpport;
	/* is DI surpport post wr to mem for OMX */
	unsigned int       post_wr_surpport;
	unsigned int	   flag_cma;
	unsigned int	   cma_alloc[10];
	unsigned int	   buffer_addr[10];
	struct page	*pages[10];
};

struct di_pre_stru_s {
/* pre input */
	struct DI_MIF_s	di_inp_mif;
	struct DI_MIF_s	di_mem_mif;
	struct DI_MIF_s	di_chan2_mif;
	struct di_buf_s *di_inp_buf;
	struct di_buf_s *di_post_inp_buf;
	struct di_buf_s *di_inp_buf_next;
	struct di_buf_s *di_mem_buf_dup_p;
	struct di_buf_s *di_chan2_buf_dup_p;
/* pre output */
	struct DI_SIM_MIF_s	di_nrwr_mif;
	struct DI_SIM_MIF_s	di_mtnwr_mif;
	struct di_buf_s *di_wr_buf;
	struct di_buf_s *di_post_wr_buf;
#ifdef NEW_DI_V1
	struct DI_SIM_MIF_s	di_contp2rd_mif;
	struct DI_SIM_MIF_s	di_contprd_mif;
	struct DI_SIM_MIF_s	di_contwr_mif;
	int		field_count_for_cont;
/*
 * 0 (f0,null,f0)->nr0,
 * 1 (f1,nr0,f1)->nr1_cnt,
 * 2 (f2,nr1_cnt,nr0)->nr2_cnt
 * 3 (f3,nr2_cnt,nr1_cnt)->nr3_cnt
 */
#endif
	struct DI_MC_MIF_s		di_mcinford_mif;
	struct DI_MC_MIF_s		di_mcvecwr_mif;
	struct DI_MC_MIF_s		di_mcinfowr_mif;
/* pre state */
	int	in_seq;
	int	recycle_seq;
	int	pre_ready_seq;

	int	pre_de_busy;            /* 1 if pre_de is not done */
	int	pre_de_busy_timer_count;
	int	pre_de_process_done;    /* flag when irq done */
	int	pre_de_clear_flag;
	/* flag is set when VFRAME_EVENT_PROVIDER_UNREG*/
	int	unreg_req_flag;
	int	unreg_req_flag_irq;
	int	reg_req_flag;
	int	reg_req_flag_irq;
	int	force_unreg_req_flag;
	int	disable_req_flag;
	/* current source info */
	int	cur_width;
	int	cur_height;
	int	cur_inp_type;
	int	cur_source_type;
	int	cur_sig_fmt;
	unsigned int orientation;
	int	cur_prog_flag; /* 1 for progressive source */
/* valid only when prog_proc_type is 0, for
 * progressive source: top field 1, bot field 0
 */
	int	source_change_flag;

	unsigned char prog_proc_type;
/* set by prog_proc_config when source is vdin,0:use 2 i
 * serial buffer,1:use 1 p buffer,3:use 2 i paralleling buffer
 */
	unsigned char buf_alloc_mode;
/* alloc di buf as p or i;0: alloc buf as i;
 * 1: alloc buf as p;
 */
	unsigned char enable_mtnwr;
	unsigned char enable_pulldown_check;

	int	same_field_source_flag;
	int	left_right;/*1,left eye; 0,right eye in field alternative*/
/*input2pre*/
	int	bypass_start_count;
/* need discard some vframe when input2pre => bypass */
	int	vdin2nr;
	enum tvin_trans_fmt	source_trans_fmt;
	enum tvin_trans_fmt	det3d_trans_fmt;
	unsigned int det_lr;
	unsigned int det_tp;
	unsigned int det_la;
	unsigned int det_null;
#ifdef DET3D
	int	vframe_interleave_flag;
#endif
	int	pre_de_irq_timeout_count;
	int	pre_throw_flag;
/*for static pic*/
	int	static_frame_count;
	bool force_interlace;
	bool bypass_pre;
	bool invert_flag;
	int nr_size;
	int count_size;
	int mcinfo_size;
	int mv_size;
	int mtn_size;
	int cma_alloc_req;
	int cma_alloc_done;
	int cma_release_req;
};

struct di_post_stru_s {
	struct DI_MIF_s	di_buf0_mif;
	struct DI_MIF_s	di_buf1_mif;
	struct DI_MIF_s	di_buf2_mif;
	struct DI_SIM_MIF_s di_diwr_mif;
	struct DI_SIM_MIF_s	di_mtnprd_mif;
	struct DI_MC_MIF_s	di_mcvecrd_mif;
	struct di_buf_s *cur_post_buf;
	int		update_post_reg_flag;
	int		post_process_fun_index;
	int		run_early_proc_fun_flag;
	int		cur_disp_index;
	int		canvas_id;
	int		next_canvas_id;
	bool		toggle_flag;
	bool		vscale_skip_flag;
	uint		start_pts;
	int		buf_type;
	int de_post_process_done;
	int post_de_busy;
	int di_post_num;
};

#define MAX_QUEUE_POOL_SIZE   256
struct queue_s {
	int		num;
	int		in_idx;
	int		out_idx;
	int		type; /* 0, first in first out;
			       * 1, general;2, fix position for di buf
			       */
	unsigned int	pool[MAX_QUEUE_POOL_SIZE];
};

#define di_dev_t struct di_dev_s

#define di_pr_info(fmt, args ...)   pr_info("DI: " fmt, ## args)

#define pr_dbg(fmt, args ...)       pr_debug("DI: " fmt, ## args)

#define pr_error(fmt, args ...)     pr_err("DI: " fmt, ## args)

#endif
