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
#include <linux/types.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>

#include <linux/clk.h>
#include <linux/atomic.h>
#include "deinterlace_hw.h"
#include "pulldown_drv.h"
#include "nr_drv.h"

/*trigger_pre_di_process param*/
#define TRIGGER_PRE_BY_PUT			'p'
#define TRIGGER_PRE_BY_DE_IRQ		'i'
#define TRIGGER_PRE_BY_UNREG		'u'
/*di_timer_handle*/
#define TRIGGER_PRE_BY_TIMER			't'
#define TRIGGER_PRE_BY_FORCE_UNREG		'f'
#define TRIGGER_PRE_BY_VFRAME_READY		'r'
#define TRIGGER_PRE_BY_PROVERDER_UNREG	'n'
#define TRIGGER_PRE_BY_DEBUG_DISABLE	'd'
#define TRIGGER_PRE_BY_PROVERDER_REG	'R'

#define DI_RUN_FLAG_RUN			0
#define DI_RUN_FLAG_PAUSE		1
#define DI_RUN_FLAG_STEP		2
#define DI_RUN_FLAG_STEP_DONE	3

#define USED_LOCAL_BUF_MAX		3
#define BYPASS_GET_MAX_BUF_NUM	4

/* buffer management related */
#define MAX_IN_BUF_NUM				20
#define MAX_LOCAL_BUF_NUM			10
#define MAX_POST_BUF_NUM			16

#define VFRAME_TYPE_IN				1
#define VFRAME_TYPE_LOCAL			2
#define VFRAME_TYPE_POST			3
#define VFRAME_TYPE_NUM				3

/*vframe define*/
#define vframe_t struct vframe_s

/* canvas defination */
#define DI_USE_FIXED_CANVAS_IDX
//#define	DET3D
#undef SUPPORT_MPEG_TO_VDIN
#define CLK_TREE_SUPPORT
#ifndef CONFIG_AMLOGIC_MEDIA_RDMA
#ifndef VSYNC_WR_MPEG_REG
#define VSYNC_WR_MPEG_REG(adr, val) aml_write_vcbus(adr, val)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len)  \
		aml_vcbus_update_bits(adr, ((1<<len)-1)<<start, val<<start)
#define VSYNC_RD_MPEG_REG(adr) aml_read_vcbus(adr)
#endif
#endif

#define IS_VDIN_SRC(src) ( \
	(src == VFRAME_SOURCE_TYPE_TUNER) || \
	(src == VFRAME_SOURCE_TYPE_CVBS) || \
	(src == VFRAME_SOURCE_TYPE_COMP) || \
	(src == VFRAME_SOURCE_TYPE_HDMI))

#define IS_I_SRC(vftype) (vftype & VIDTYPE_INTERLACE_BOTTOM)

#define IS_COMP_MODE(vftype) (vftype & VIDTYPE_COMPRESS)

enum process_fun_index_e {
	PROCESS_FUN_NULL = 0,
	PROCESS_FUN_DI,
	PROCESS_FUN_PD,
	PROCESS_FUN_PROG,
	PROCESS_FUN_BOB
};
#define process_fun_index_t enum process_fun_index_e

enum canvas_idx_e {
	NR_CANVAS,
	MTN_CANVAS,
	MV_CANVAS,
};
#define pulldown_mode_t enum pulldown_mode_e
struct di_buf_s {
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
	unsigned long cnt_adr;
	int cnt_canvas_idx;
	unsigned long mcinfo_adr;
	int mcinfo_canvas_idx;
	unsigned short *mcinfo_vaddr;
	bool bflg_vmap;
	unsigned long mcvec_adr;
	int mcvec_canvas_idx;
	struct mcinfo_pre_s {
	unsigned int highvertfrqflg;
	unsigned int motionparadoxflg;
	unsigned int regs[26];/* reg 0x2fb0~0x2fc9 */
	} curr_field_mcinfo;
	/* blend window */
	struct pulldown_detected_s
	pd_config;
	/* tff bff check result bit[1:0]*/
	unsigned int privated;
	unsigned int canvas_config_flag;
	/* 0,configed; 1,config type 1 (prog);
	 * 2, config type 2 (interlace)
	 */
	unsigned int canvas_height;
	unsigned int canvas_width[3];/* nr/mtn/mv */
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
	struct page	*pages;
};
#define RDMA_DET3D_IRQ				0x20
/* vdin0 rdma irq */
#define RDMA_DEINT_IRQ				0x2
#define RDMA_TABLE_SIZE             ((PAGE_SIZE)<<1)

#define MAX_CANVAS_WIDTH				1920
#define MAX_CANVAS_HEIGHT				1088


/* #define DI_BUFFER_DEBUG */

#define DI_LOG_MTNINFO		0x02
#define DI_LOG_PULLDOWN		0x10
#define DI_LOG_BUFFER_STATE		0x20
#define DI_LOG_TIMESTAMP		0x100
#define DI_LOG_PRECISE_TIMESTAMP		0x200
#define DI_LOG_QUEUE		0x40
#define DI_LOG_VFRAME		0x80


#define QUEUE_LOCAL_FREE           0
#define QUEUE_IN_FREE              1
#define QUEUE_PRE_READY            2
#define QUEUE_POST_FREE            3
#define QUEUE_POST_READY           4
#define QUEUE_RECYCLE              5
#define QUEUE_DISPLAY              6
#define QUEUE_TMP                  7
#define QUEUE_POST_DOING           8
#define QUEUE_NUM                  9

#define VFM_NAME		"deinterlace"

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
#define DI_SUSPEND_FLAG 0x2
#define DI_LOAD_REG_FLAG 0x4
#define DI_VPU_CLKB_SET 0x8

#define TABLE_LEN_MAX 10000

struct di_dev_s {
	dev_t			   devt;
	struct cdev		   cdev; /* The cdev structure */
	struct device	   *dev;
	struct platform_device	*pdev;
	struct task_struct *task;
	struct clk	*vpu_clkb;
	unsigned long clkb_max_rate;
	unsigned long clkb_min_rate;
	struct list_head   pq_table_list;
	atomic_t	       pq_flag;
	unsigned char	   di_event;
	unsigned int	   pre_irq;
	unsigned int	   post_irq;
	unsigned int	   flags;
	unsigned long	   jiffy;
	unsigned long	   mem_start;
	unsigned int	   mem_size;
	unsigned int	   buffer_size;
	unsigned int	   post_buffer_size;
	unsigned int	   buf_num_avail;
	int		rdma_handle;
	/* is support nr10bit */
	unsigned int	   nr10bit_support;
	/* is DI support post wr to mem for OMX */
	unsigned int       post_wr_support;
	unsigned int nrds_enable;
	unsigned int pps_enable;
	u32 h_sc_down_en;/*sm1, tm2 ...*/
	/*struct	mutex      cma_mutex;*/
	unsigned int	   flag_cma;
	struct page			*total_pages;
	atomic_t			mem_flag;
	struct dentry *dbg_root;	/*dbg_fs*/
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
	int	pre_de_process_flag;    /* flag when pre_de_process done */
	int	pre_de_clear_flag;
	/* flag is set when VFRAME_EVENT_PROVIDER_UNREG*/
	int	unreg_req_flag;
	int	unreg_req_flag_irq;
	int	unreg_req_flag_cnt;
	int	reg_req_flag;
	int	reg_req_flag_irq;
	int	reg_req_flag_cnt;
	int	reg_irq_busy;
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
/* input size change flag, 1: need reconfig pre/nr/dnr size */
/* 0: not need config pre/nr/dnr size*/
	bool input_size_change_flag;
/* true: bypass di all logic, false: not bypass */
	bool bypass_flag;
	unsigned char prog_proc_type;
/* set by prog_proc_config when source is vdin,0:use 2 i
 * serial buffer,1:use 1 p buffer,3:use 2 i paralleling buffer
*/
	unsigned char buf_alloc_mode;
/* alloc di buf as p or i;0: alloc buf as i;
 * 1: alloc buf as p;
*/
	unsigned char madi_enable;
	unsigned char mcdi_enable;
	unsigned int  pps_dstw;
	unsigned int  pps_dsth;
	int	left_right;/*1,left eye; 0,right eye in field alternative*/
/*input2pre*/
	int	bypass_start_count;
/* need discard some vframe when input2pre => bypass */
	unsigned char vdin2nr;
	enum tvin_trans_fmt	source_trans_fmt;
	enum tvin_trans_fmt	det3d_trans_fmt;
	unsigned int det_lr;
	unsigned int det_tp;
	unsigned int det_la;
	unsigned int det_null;
	unsigned int width_bk;
#ifdef DET3D
	int	vframe_interleave_flag;
#endif
/**/
	int	pre_de_irq_timeout_count;
	int	pre_throw_flag;
	int	bad_frame_throw_count;
/*for static pic*/
	int	static_frame_count;
	bool force_interlace;
	bool bypass_pre;
	bool invert_flag;
	bool vdin_source;
	int nr_size;
	int count_size;
	int mcinfo_size;
	int mv_size;
	int mtn_size;
	int cma_alloc_req;
	int cma_alloc_done;
	int cma_release_req;
	/* for performance debug */
	unsigned long irq_time[2];
	/* combing adaptive */
	struct combing_status_s *mtn_status;
};

struct di_post_stru_s {
	struct DI_MIF_s	di_buf0_mif;
	struct DI_MIF_s	di_buf1_mif;
	struct DI_MIF_s	di_buf2_mif;
	struct DI_SIM_MIF_s di_diwr_mif;
	struct DI_SIM_MIF_s	di_mtnprd_mif;
	struct DI_MC_MIF_s	di_mcvecrd_mif;
	struct di_buf_s *cur_post_buf;
	struct di_buf_s *keep_buf;
	int		update_post_reg_flag;
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
	unsigned int post_peek_underflow;
	unsigned int di_post_process_cnt;
	unsigned int check_recycle_buf_cnt;
	/* performance debug */
	unsigned int  post_wr_cnt;
	unsigned long irq_time;
};

#define MAX_QUEUE_POOL_SIZE   256
struct queue_s {
	unsigned int num;
	unsigned int in_idx;
	unsigned int out_idx;
	unsigned int type; /* 0, first in first out;
			       * 1, general;2, fix position for di buf
			       */
	unsigned int pool[MAX_QUEUE_POOL_SIZE];
};

struct di_buf_pool_s {
	struct di_buf_s *di_buf_ptr;
	unsigned int size;
};

unsigned char is_bypass(vframe_t *vf_in);

/*---get di state parameter---*/
struct di_dev_s *get_di_de_devp(void);
struct di_pre_stru_s *get_di_pre_stru(void);
struct di_post_stru_s *get_di_post_stru(void);
const char *get_di_version_s(void);
int get_di_dump_state_flag(void);
uint get_di_init_flag(void);
unsigned char get_di_recovery_flag(void);
unsigned int get_di_recovery_log_reason(void);
int get_di_di_blocking(void);
unsigned int get_di_recovery_log_queue_idx(void);
struct di_buf_s *get_di_recovery_log_di_buf(void);
int get_di_video_peek_cnt(void);
unsigned long get_di_reg_unreg_timeout_cnt(void);
struct vframe_s **get_di_vframe_in(void);


/*---------------------*/

struct di_buf_s *get_di_buf(int queue_idx, int *start_pos);

#define queue_for_each_entry(di_buf, ptm, queue_idx, list)      \
	for (itmp = 0; ((di_buf = get_di_buf(queue_idx, &itmp)) != NULL); )

#define di_dev_t struct di_dev_s

#define di_pr_info(fmt, args ...)   pr_info("DI: " fmt, ## args)

#define pr_dbg(fmt, args ...)       pr_debug("DI: " fmt, ## args)

#define pr_error(fmt, args ...)     pr_err("DI: " fmt, ## args)

#endif
