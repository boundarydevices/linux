/*
 * drivers/amlogic/media/deinterlace/deinterlace.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
#include <linux/of_fdt.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vpu/vpu.h>
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#endif
#include <linux/amlogic/media/video_sink/video.h>
#include "register.h"
#include "deinterlace.h"
#include "deinterlace_dbg.h"
#include "nr_downscale.h"
#include "di_pps.h"
#define CREATE_TRACE_POINTS
#include "deinterlace_trace.h"

/*2018-07-18 add debugfs*/
#include <linux/seq_file.h>
#include <linux/debugfs.h>
/*2018-07-18 -----------*/

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE deinterlace_trace
#include <trace/define_trace.h>

#ifdef DET3D
#include "detect3d.h"
#endif
#define ENABLE_SPIN_LOCK_ALWAYS

#define DEVICE_NAME		"deinterlace"
#define CLASS_NAME		"deinterlace"

static struct di_pre_stru_s di_pre_stru;

static struct di_post_stru_s di_post_stru;

static DEFINE_SPINLOCK(di_lock2);

#define di_lock_irqfiq_save(irq_flag) \
	spin_lock_irqsave(&di_lock2, irq_flag)

#define di_unlock_irqfiq_restore(irq_flag) \
	spin_unlock_irqrestore(&di_lock2, irq_flag)

#ifdef SUPPORT_MPEG_TO_VDIN
static int mpeg2vdin_flag;
static int mpeg2vdin_en;
#endif

static int queue_print_flag = -1;
static int di_reg_unreg_cnt = 100;
static bool overturn;
static bool check_start_drop_prog;
static bool mcpre_en = true;

static bool mc_mem_alloc;

static unsigned int di_pre_rdma_enable;


static unsigned int di_force_bit_mode = 10;
module_param(di_force_bit_mode, uint, 0664);
MODULE_PARM_DESC(di_force_bit_mode, "force DI bit mode to 8 or 10 bit");

static bool full_422_pack;
/* destroy unnecessary frames before display */
static unsigned int hold_video;

static DEFINE_SPINLOCK(plist_lock);

static di_dev_t *de_devp;
static dev_t di_devno;
static struct class *di_clsp;

static const char version_s[] = "2019-03-18a";

static int bypass_state = 1;
static int bypass_all;
/*1:enable bypass pre,ei only;
 * 2:debug force bypass pre,ei for post
 */
static int bypass_pre;
static int bypass_trick_mode = 1;
static int bypass_3d = 1;
static int invert_top_bot;
static int skip_top_bot;/*1or2: may affect atv when bypass di*/

static int bypass_post;
static bool post_wr_en;
static unsigned int post_wr_support;
static int bypass_post_state;
/* 0, use di_wr_buf still;
 * 1, call pre_de_done_buf_clear to clear di_wr_buf;
 * 2, do nothing
 */

static int force_width;
static int force_height;
/* add avoid vframe put/get error */
static int di_blocking;
/*
 * bit[2]: enable bypass all when skip
 * bit[1:0]: enable bypass post when skip
 */
static int di_vscale_skip_enable;

/* 0: not support nr10bit, 1: support nr10bit */
static unsigned int nr10bit_support;

#ifdef RUN_DI_PROCESS_IN_IRQ
/*
 * di_process() run in irq,
 * di_reg_process(), di_unreg_process() run in kernel thread
 * di_reg_process_irq(), di_unreg_process_irq() run in irq
 * di_vf_put(), di_vf_peek(), di_vf_get() run in irq
 * di_receiver_event_fun() run in task or irq
 */
/*
 * important:
 * to set input2pre, VFRAME_EVENT_PROVIDER_VFRAME_READY of
 * vdin should be sent in irq
 */

static int input2pre;
/*false:process progress by field;
 * true: process progress by frame with 2 interlace buffer
 */
static int input2pre_buf_miss_count;
static int input2pre_proc_miss_count;
static int input2pre_throw_count;
static int input2pre_miss_policy;
/* 0, do not force pre_de_busy to 0, use di_wr_buf after de_irq happen;
 * 1, force pre_de_busy to 0 and call pre_de_done_buf_clear to clear di_wr_buf
 */
#endif
/*false:process progress by field;
 * bit0: process progress by frame with 2 interlace buffer
 * bit1: temp add debug for 3d process FA,1:bit0 force to 1;
 */
static int use_2_interlace_buff;
/* prog_proc_config,
 * bit[2:1]: when two field buffers are used,
 * 0 use vpp for blending ,
 * 1 use post_di module for blending
 * 2 debug mode, bob with top field
 * 3 debug mode, bot with bot field
 * bit[0]:
 * 0 "prog vdin" use two field buffers,
 * 1 "prog vdin" use single frame buffer
 * bit[4]:
 * 0 "prog frame from decoder/vdin" use two field buffers,
 * 1 use single frame buffer
 * bit[5]:
 * when two field buffers are used for decoder (bit[4] is 0):
 * 1,handle prog frame as two interlace frames
 * bit[6]:(bit[4] is 0,bit[5] is 0,use_2_interlace_buff is 0): 0,
 * process progress frame as field,blend by post;
 * 1, process progress frame as field,process by normal di
 */
static int prog_proc_config = (1 << 5) | (1 << 1) | 1;
/*
 * for source include both progressive and interlace pictures,
 * always use post_di module for blending
 */
#define is_handle_prog_frame_as_interlace(vframe) \
	(((prog_proc_config & 0x30) == 0x20) && \
	 ((vframe->type & VIDTYPE_VIU_422) == 0))

static int frame_count;
static int disp_frame_count;
static int start_frame_drop_count = 2;
/* static int start_frame_hold_count = 0; */

static int video_peek_cnt;
static unsigned long reg_unreg_timeout_cnt;
static int di_vscale_skip_count;
static int di_vscale_skip_count_real;
static int vpp_3d_mode;
static bool det3d_en;
#ifdef DET3D
static unsigned int det3d_mode;
static void set3d_view(enum tvin_trans_fmt trans_fmt, struct vframe_s *vf);
#endif
int get_current_vscale_skip_count(struct vframe_s *vf);

static void di_pq_parm_destroy(struct di_pq_parm_s *pq_ptr);
static struct di_pq_parm_s *di_pq_parm_create(struct am_pq_parm_s *);

static uint init_flag;/*flag for di buferr*/
static unsigned int reg_flag;/*flag for vframe reg/unreg*/
static unsigned int unreg_cnt;/*cnt for vframe unreg*/
static unsigned int reg_cnt;/*cnt for vframe reg*/
static unsigned char active_flag;
static unsigned char recovery_flag;
static unsigned int force_recovery = 1;
static unsigned int force_recovery_count;
static unsigned int recovery_log_reason;
static unsigned int recovery_log_queue_idx;
static struct di_buf_s *recovery_log_di_buf;


static long same_field_top_count;
static long same_field_bot_count;
/* bit 0:
 * 0, keep 3 buffers in pre_ready_list for checking;
 * 1, keep 4 buffers in pre_ready_list for checking;
 */

static int post_hold_line = 8; /*2019-01-10: from VLSI feijun from 17 to 8*/
static int post_urgent = 1;

/*pre process speed debug */
static int pre_process_time;
static int di_receiver_event_fun(int type, void *data, void *arg);
static void di_uninit_buf(unsigned int disable_mirror);
static void log_buffer_state(unsigned char *tag);
/* static void put_get_disp_buf(void); */

static const
struct vframe_receiver_op_s di_vf_receiver = {
	.event_cb	= di_receiver_event_fun
};

static struct vframe_receiver_s di_vf_recv;

static vframe_t *di_vf_peek(void *arg);
static vframe_t *di_vf_get(void *arg);
static void di_vf_put(vframe_t *vf, void *arg);
static int di_event_cb(int type, void *data, void *private_data);
static int di_vf_states(struct vframe_states *states, void *arg);
static void di_process(void);
static void di_reg_process(void);
static void di_reg_process_irq(void);
static void di_unreg_process(void);
static void di_unreg_process_irq(void);
static struct queue_s *get_queue_by_idx(int idx);
static void dump_state(void);
static void recycle_keep_buffer(void);

static const
struct vframe_operations_s deinterlace_vf_provider = {
	.peek		= di_vf_peek,
	.get		= di_vf_get,
	.put		= di_vf_put,
	.event_cb	= di_event_cb,
	.vf_states	= di_vf_states,
};

static struct vframe_provider_s di_vf_prov;

static int di_sema_init_flag;
static struct semaphore di_sema;
static struct tasklet_struct di_pre_tasklet;
void trigger_pre_di_process(unsigned char idx)
{
	if (di_sema_init_flag == 0)
		return;

	log_buffer_state((idx == 'i') ? "irq" : ((idx == 'p') ?
		"put" : ((idx == 'r') ? "rdy" : "oth")));

	/* tasklet_hi_schedule(&di_pre_tasklet); */
	tasklet_schedule(&di_pre_tasklet);
	de_devp->jiffy = jiffies_64;
	/* trigger di_reg_process and di_unreg_process */
	if ((idx != 'p') && (idx != 'i'))
		up(&di_sema);
}

static unsigned int di_printk_flag;
#define DI_PRE_INTERVAL         (HZ / 100)

/*
 * progressive frame process type config:
 * 0, process by field;
 * 1, process by frame (only valid for vdin source whose
 * width/height does not change)
 */
static vframe_t *vframe_in[MAX_IN_BUF_NUM];
static vframe_t vframe_in_dup[MAX_IN_BUF_NUM];
static vframe_t vframe_local[MAX_LOCAL_BUF_NUM * 2];
static vframe_t vframe_post[MAX_POST_BUF_NUM];
static struct di_buf_s *cur_post_ready_di_buf;

static struct di_buf_s di_buf_local[MAX_LOCAL_BUF_NUM * 2];
static struct di_buf_s di_buf_in[MAX_IN_BUF_NUM];
static struct di_buf_s di_buf_post[MAX_POST_BUF_NUM];


/************For Write register**********************/
static unsigned int di_stop_reg_flag;
static unsigned int num_di_stop_reg_addr = 4;
static unsigned int di_stop_reg_addr[4] = {0};
static unsigned int di_dbg_mask;

unsigned int is_need_stop_reg(unsigned int addr)
{
	int idx = 0;

	if (di_stop_reg_flag) {
		for (idx = 0; idx < num_di_stop_reg_addr; idx++) {
			if (addr == di_stop_reg_addr[idx]) {
				pr_dbg("stop write addr: %x\n", addr);
				return 1;
			}
		}
	}

	return 0;
}

void DI_Wr(unsigned int addr, unsigned int val)
{
	if (is_need_stop_reg(addr))
		return;

	Wr(addr, val);
}

void DI_Wr_reg_bits(unsigned int adr, unsigned int val,
		unsigned int start, unsigned int len)
{
	if (is_need_stop_reg(adr))
		return;

	Wr_reg_bits(adr, val, start, len);
}

void DI_VSYNC_WR_MPEG_REG(unsigned int addr, unsigned int val)
{
	if (is_need_stop_reg(addr))
		return;
	if (post_wr_en && post_wr_support)
		DI_Wr(addr, val);
	else
		VSYNC_WR_MPEG_REG(addr, val);
}

void DI_VSYNC_WR_MPEG_REG_BITS(unsigned int addr, unsigned int val,
	unsigned int start, unsigned int len)
{
	if (is_need_stop_reg(addr))
		return;
	if (post_wr_en && post_wr_support)
		DI_Wr_reg_bits(addr, val, start, len);
	else
		VSYNC_WR_MPEG_REG_BITS(addr, val, start, len);
}

unsigned int DI_POST_REG_RD(unsigned int addr)
{
	if (IS_ERR_OR_NULL(de_devp))
		return 0;
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		pr_err("[DI] REG 0x%x access prohibited.\n", addr);
		return 0;
	}
	return VSYNC_RD_MPEG_REG(addr);
}
EXPORT_SYMBOL(DI_POST_REG_RD);

int DI_POST_WR_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	if (IS_ERR_OR_NULL(de_devp))
		return 0;
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		pr_err("[DI] REG 0x%x access prohibited.\n", adr);
		return -1;
	}
	return VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
}
EXPORT_SYMBOL(DI_POST_WR_REG_BITS);
/**********************************/

/*****************************
 *	 di attr management :
 *	 enable
 *	 mode
 *	 reg
 ******************************/
/*config attr*/
static ssize_t
show_config(struct device *dev,
	    struct device_attribute *attr, char *buf)
{
	int pos = 0;

	return pos;
}

static ssize_t
store_config(struct device *dev, struct device_attribute *attr, const char *buf,
	     size_t count);

static int run_flag = DI_RUN_FLAG_RUN;
static int pre_run_flag = DI_RUN_FLAG_RUN;
static int dump_state_flag;

/*2018-08-17 add debugfs*/
/*add debugfs: get dump_state parameter*/
struct di_pre_stru_s *get_di_pre_stru(void)
{
	return &di_pre_stru;
}

struct di_post_stru_s *get_di_post_stru(void)
{
	return &di_post_stru;
}

struct di_dev_s *get_di_de_devp(void)
{
	return de_devp;
}

const char *get_di_version_s(void)
{
	return version_s;
}

int get_di_di_blocking(void)
{
	return di_blocking;
}

int get_di_video_peek_cnt(void)
{
	return video_peek_cnt;
}

unsigned long get_di_reg_unreg_timeout_cnt(void)
{
	return reg_unreg_timeout_cnt;
}

uint get_di_init_flag(void)
{
	return init_flag;
}

unsigned char get_di_recovery_flag(void)
{
	return recovery_flag;
}

unsigned int get_di_recovery_log_reason(void)
{
	return recovery_log_reason;
}

unsigned int get_di_recovery_log_queue_idx(void)
{
	return recovery_log_queue_idx;
}

struct di_buf_s *get_di_recovery_log_di_buf(void)
{
	return recovery_log_di_buf;
}

struct vframe_s **get_di_vframe_in(void)
{
	return &vframe_in[0];
}

int get_di_dump_state_flag(void)
{
	return dump_state_flag;
}
/*--------------------------*/

static ssize_t
store_dbg(struct device *dev,
	  struct device_attribute *attr,
	  const char *buf, size_t count)
{
	if (strncmp(buf, "buf", 3) == 0) {
		struct di_buf_s *di_buf_tmp = 0;

		if (kstrtoul(buf + 3, 16, (unsigned long *)&di_buf_tmp))
			return count;
		dump_di_buf(di_buf_tmp);
	} else if (strncmp(buf, "vframe", 6) == 0) {
		vframe_t *vf = 0;

		if (kstrtoul(buf + 6, 16, (unsigned long *)&vf))
			return count;
		dump_vframe(vf);
	} else if (strncmp(buf, "pool", 4) == 0) {
		unsigned long idx = 0;
		if (kstrtoul(buf + 4, 10, &idx))
			return count;
		dump_pool(get_queue_by_idx(idx));
	} else if (strncmp(buf, "state", 4) == 0) {
		dump_state();
		pr_info("add new debugfs: cat /sys/kernel/debug/di/state\n");
	} else if (strncmp(buf, "prog_proc_config", 16) == 0) {
		if (buf[16] == '1')
			prog_proc_config = 1;
		else
			prog_proc_config = 0;
	} else if (strncmp(buf, "init_flag", 9) == 0) {
		if (buf[9] == '1')
			init_flag = 1;
		else
			init_flag = 0;
	} else if (strncmp(buf, "run", 3) == 0) {
		/* timestamp_pcrscr_enable(1); */
		run_flag = DI_RUN_FLAG_RUN;
	} else if (strncmp(buf, "pause", 5) == 0) {
		/* timestamp_pcrscr_enable(0); */
		run_flag = DI_RUN_FLAG_PAUSE;
	} else if (strncmp(buf, "step", 4) == 0) {
		run_flag = DI_RUN_FLAG_STEP;
	} else if (strncmp(buf, "prun", 4) == 0) {
		pre_run_flag = DI_RUN_FLAG_RUN;
	} else if (strncmp(buf, "ppause", 6) == 0) {
		pre_run_flag = DI_RUN_FLAG_PAUSE;
	} else if (strncmp(buf, "pstep", 5) == 0) {
		pre_run_flag = DI_RUN_FLAG_STEP;
	} else if (strncmp(buf, "dumpreg", 7) == 0) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
			dump_di_reg_g12();
		else
			pr_info("add new debugfs: cat /sys/kernel/debug/di/dumpreg\n");
	} else if (strncmp(buf, "dumpmif", 7) == 0) {
		dump_mif_size_state(&di_pre_stru, &di_post_stru);
	} else if (strncmp(buf, "recycle_buf", 11) == 0) {
		recycle_keep_buffer();
	} else if (strncmp(buf, "recycle_post", 12) == 0) {
		if (di_vf_peek(NULL))
			di_vf_put(di_vf_get(NULL), NULL);
	} else if (strncmp(buf, "mem_map", 7) == 0) {
		dump_buf_addr(di_buf_local, MAX_LOCAL_BUF_NUM * 2);
	} else {
		pr_info("DI no support cmd %s!!!\n", buf);
	}

	return count;
}
static int __init di_read_canvas_reverse(char *str)
{
	unsigned char *ptr = str;

	pr_dbg("%s: bootargs is %s.\n", __func__, str);
	if (strstr(ptr, "1")) {
		invert_top_bot |= 0x1;
		overturn = true;
	} else {
		invert_top_bot &= (~0x1);
		overturn = false;
	}

	return 0;
}
__setup("video_reverse=", di_read_canvas_reverse);
static unsigned int di_debug_flag;/* enable rdma even di bypassed */
static unsigned char *di_log_buf;
static unsigned int di_log_wr_pos;
static unsigned int di_log_rd_pos;
static unsigned int di_log_buf_size;
static unsigned int di_printk_flag;
static unsigned int di_log_flag;
static unsigned int buf_state_log_threshold = 16;
static unsigned int buf_state_log_start;
/*  set to 1 by condition of "post_ready count < buf_state_log_threshold",
 * reset to 0 by set buf_state_log_threshold as 0
 */

static DEFINE_SPINLOCK(di_print_lock);

#define PRINT_TEMP_BUF_SIZE 128

int di_print_buf(char *buf, int len)
{
	unsigned long flags;
	int pos;
	int di_log_rd_pos_;

	if (di_log_buf_size == 0)
		return 0;

	spin_lock_irqsave(&di_print_lock, flags);
	di_log_rd_pos_ = di_log_rd_pos;
	if (di_log_wr_pos >= di_log_rd_pos)
		di_log_rd_pos_ += di_log_buf_size;

	for (pos = 0; pos < len && di_log_wr_pos < (di_log_rd_pos_ - 1);
	     pos++, di_log_wr_pos++) {
		if (di_log_wr_pos >= di_log_buf_size)
			di_log_buf[di_log_wr_pos - di_log_buf_size] = buf[pos];
		else
			di_log_buf[di_log_wr_pos] = buf[pos];
	}
	if (di_log_wr_pos >= di_log_buf_size)
		di_log_wr_pos -= di_log_buf_size;
	spin_unlock_irqrestore(&di_print_lock, flags);
	return pos;
}

static u64 cur_to_msecs(void)
{
	u64 cur = sched_clock();

	do_div(cur, NSEC_PER_MSEC);
	return cur;
}

/* static int log_seq = 0; */
int di_print(const char *fmt, ...)
{
	va_list args;
	int avail = PRINT_TEMP_BUF_SIZE;
	char buf[PRINT_TEMP_BUF_SIZE];
	int pos, len = 0;

	if (di_printk_flag & 1) {
		if (di_log_flag & DI_LOG_PRECISE_TIMESTAMP)
			pr_dbg("%llums:", cur_to_msecs());
		va_start(args, fmt);
		vprintk(fmt, args);
		va_end(args);
		return 0;
	}

	if (di_log_buf_size == 0)
		return 0;

/* len += snprintf(buf+len, avail-len, "%d:",log_seq++); */
	if (di_log_flag & DI_LOG_TIMESTAMP)
		len += snprintf(buf + len, avail - len, "%u:",
			jiffies_to_msecs(jiffies_64));

	va_start(args, fmt);
	len += vsnprintf(buf + len, avail - len, fmt, args);
	va_end(args);

	if ((avail - len) <= 0)
		buf[PRINT_TEMP_BUF_SIZE - 1] = '\0';

	pos = di_print_buf(buf, len);
/* pr_dbg("di_print:%d %d\n", di_log_wr_pos, di_log_rd_pos); */
	return pos;
}

static ssize_t read_log(char *buf)
{
	unsigned long flags;
	ssize_t read_size = 0;

	if (di_log_buf_size == 0)
		return 0;
/* pr_dbg("show_log:%d %d\n", di_log_wr_pos, di_log_rd_pos); */
	spin_lock_irqsave(&di_print_lock, flags);
	if (di_log_rd_pos < di_log_wr_pos)
		read_size = di_log_wr_pos - di_log_rd_pos;

	else if (di_log_rd_pos > di_log_wr_pos)
		read_size = di_log_buf_size - di_log_rd_pos;

	if (read_size > PAGE_SIZE)
		read_size = PAGE_SIZE;
	if (read_size > 0)
		memcpy(buf, di_log_buf + di_log_rd_pos, read_size);

	di_log_rd_pos += read_size;
	if (di_log_rd_pos >= di_log_buf_size)
		di_log_rd_pos = 0;
	spin_unlock_irqrestore(&di_print_lock, flags);
	return read_size;
}

static ssize_t
show_log(struct device *dev, struct device_attribute *attr, char *buf)
{
	return read_log(buf);
}

static ssize_t
store_log(struct device *dev,
	  struct device_attribute *attr,
	  const char *buf, size_t count)
{
	unsigned long flags, tmp;

	if (strncmp(buf, "bufsize", 7) == 0) {
		if (kstrtoul(buf + 7, 10, &tmp))
			return count;
		spin_lock_irqsave(&di_print_lock, flags);
		kfree(di_log_buf);
		di_log_buf = NULL;
		di_log_buf_size = 0;
		di_log_rd_pos = 0;
		di_log_wr_pos = 0;
		if (tmp >= 1024) {
			di_log_buf_size = 0;
			di_log_rd_pos = 0;
			di_log_wr_pos = 0;
			di_log_buf = kmalloc(tmp, GFP_KERNEL);
			if (di_log_buf)
				di_log_buf_size = tmp;
		}
		spin_unlock_irqrestore(&di_print_lock, flags);
		pr_dbg("di_store:set bufsize tmp %lu %u\n",
			tmp, di_log_buf_size);
	} else if (strncmp(buf, "printk", 6) == 0) {
		if (kstrtoul(buf + 6, 10, &tmp))
			return count;
		di_printk_flag = tmp;
	} else {
		di_print("%s", buf);
	}
	return 16;
}

static ssize_t
show_vframe_status(struct device *dev,
		   struct device_attribute *attr,
		   char *buf)
{
	int ret = 0;
	struct vframe_states states;

	ret = vf_get_states_by_name(VFM_NAME, &states);

	if (ret == 0) {
		ret += sprintf(buf + ret, "vframe_pool_size=%d\n",
			states.vf_pool_size);
		ret += sprintf(buf + ret, "vframe buf_free_num=%d\n",
			states.buf_free_num);
		ret += sprintf(buf + ret, "vframe buf_recycle_num=%d\n",
			states.buf_recycle_num);
		ret += sprintf(buf + ret, "vframe buf_avail_num=%d\n",
			states.buf_avail_num);
	} else {
		ret = 0;
		ret += sprintf(buf + ret, "vframe no states\n");
	}

	return ret;
}

static ssize_t show_tvp_region(struct device *dev,
			     struct device_attribute *attr, char *buff)
{
	ssize_t len = 0;

	len = sprintf(buff, "segment DI:%lx - %lx (size:0x%x)\n",
			de_devp->mem_start,
			de_devp->mem_start + de_devp->mem_size - 1,
			de_devp->mem_size);
	return len;
}



static ssize_t store_dump_mem(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t len);
static DEVICE_ATTR(config, 0640, show_config, store_config);
static DEVICE_ATTR(debug, 0200, NULL, store_dbg);
static DEVICE_ATTR(dump_pic, 0200, NULL, store_dump_mem);
static DEVICE_ATTR(log, 0640, show_log, store_log);
static DEVICE_ATTR(provider_vframe_status, 0444, show_vframe_status, NULL);
static DEVICE_ATTR(tvp_region, 0444, show_tvp_region, NULL);
/***************************
 * di buffer management
 ***************************/

static char *vframe_type_name[] = {
	"", "di_buf_in", "di_buf_loc", "di_buf_post"
};

static unsigned int default_width = 1920;
static unsigned int default_height = 1080;

/*
 * all buffers are in
 * 1) list of local_free_list,in_free_list,pre_ready_list,recycle_list
 * 2) di_pre_stru.di_inp_buf
 * 3) di_pre_stru.di_wr_buf
 * 4) cur_post_ready_di_buf
 * 5) (struct di_buf_s*)(vframe->private_data)->di_buf[]
 *
 * 6) post_free_list_head
 * 8) (struct di_buf_s*)(vframe->private_data)
 */

#define queue_t struct queue_s
static queue_t queue[QUEUE_NUM];
static struct di_buf_pool_s di_buf_pool[VFRAME_TYPE_NUM];
static struct queue_s *get_queue_by_idx(int idx)
{
	if (idx < QUEUE_NUM)
		return &(queue[idx]);
	else
		return NULL;
}

static void queue_init(int local_buffer_num)
{
	int i, j;

	for (i = 0; i < QUEUE_NUM; i++) {
		queue_t *q = &queue[i];

		for (j = 0; j < MAX_QUEUE_POOL_SIZE; j++)
			q->pool[j] = 0;

		q->in_idx = 0;
		q->out_idx = 0;
		q->num = 0;
		q->type = 0;
		if ((i == QUEUE_RECYCLE) ||
			(i == QUEUE_DISPLAY) ||
			(i == QUEUE_TMP)     ||
			(i == QUEUE_POST_DOING))
			q->type = 1;

		if ((i == QUEUE_LOCAL_FREE) && use_2_interlace_buff)
			q->type = 2;
	}
	if (local_buffer_num > 0) {
		di_buf_pool[VFRAME_TYPE_IN - 1].di_buf_ptr = &di_buf_in[0];
		di_buf_pool[VFRAME_TYPE_IN - 1].size = MAX_IN_BUF_NUM;

		di_buf_pool[VFRAME_TYPE_LOCAL-1].di_buf_ptr = &di_buf_local[0];
		di_buf_pool[VFRAME_TYPE_LOCAL - 1].size = local_buffer_num;

		di_buf_pool[VFRAME_TYPE_POST - 1].di_buf_ptr = &di_buf_post[0];
		di_buf_pool[VFRAME_TYPE_POST - 1].size = MAX_POST_BUF_NUM;
	}
}

struct di_buf_s *get_di_buf(int queue_idx, int *start_pos)
{
	queue_t *q = &(queue[queue_idx]);
	int idx = 0;
	unsigned int pool_idx, di_buf_idx;
	struct di_buf_s *di_buf = NULL;
	int start_pos_init = *start_pos;

	if (di_log_flag & DI_LOG_QUEUE)
		di_print("%s:<%d:%d,%d,%d> %d\n", __func__, queue_idx,
			q->num, q->in_idx, q->out_idx, *start_pos);

	if (q->type == 0) {
		if ((*start_pos) < q->num) {
			idx = q->out_idx + (*start_pos);
			if (idx >= MAX_QUEUE_POOL_SIZE)
				idx -= MAX_QUEUE_POOL_SIZE;

			(*start_pos)++;
		} else {
			idx = MAX_QUEUE_POOL_SIZE;
		}
	} else if ((q->type == 1) || (q->type == 2)) {
		for (idx = (*start_pos); idx < MAX_QUEUE_POOL_SIZE; idx++) {
			if (q->pool[idx] != 0) {
				*start_pos = idx + 1;
				break;
			}
		}
	}
	if (idx < MAX_QUEUE_POOL_SIZE) {
		pool_idx = ((q->pool[idx] >> 8) & 0xff) - 1;
		di_buf_idx = q->pool[idx] & 0xff;
		if (pool_idx < VFRAME_TYPE_NUM) {
			if (di_buf_idx < di_buf_pool[pool_idx].size)
				di_buf =
					&(di_buf_pool[pool_idx].di_buf_ptr[
						  di_buf_idx]);
		}
	}

	if ((di_buf) && ((((pool_idx + 1) << 8) | di_buf_idx)
			 != ((di_buf->type << 8) | (di_buf->index)))) {
		pr_dbg("%s: Error (%x,%x)\n", __func__,
			(((pool_idx + 1) << 8) | di_buf_idx),
			((di_buf->type << 8) | (di_buf->index)));
		if (recovery_flag == 0) {
			recovery_log_reason = 1;
			recovery_log_queue_idx = (start_pos_init<<8)|queue_idx;
			recovery_log_di_buf = di_buf;
		}
		recovery_flag++;
		di_buf = NULL;
	}

	if (di_log_flag & DI_LOG_QUEUE) {
		if (di_buf)
			di_print("%s: %x(%d,%d)\n", __func__, di_buf,
				pool_idx, di_buf_idx);
		else
			di_print("%s: %x\n", __func__, di_buf);
	}

	return di_buf;
}


static struct di_buf_s *get_di_buf_head(int queue_idx)
{
	queue_t *q = &(queue[queue_idx]);
	int idx;
	unsigned int pool_idx, di_buf_idx;
	struct di_buf_s *di_buf = NULL;

	if (di_log_flag & DI_LOG_QUEUE)
		di_print("%s:<%d:%d,%d,%d>\n", __func__, queue_idx,
			q->num, q->in_idx, q->out_idx);

	if (q->num > 0) {
		if (q->type == 0) {
			idx = q->out_idx;
		} else {
			for (idx = 0; idx < MAX_QUEUE_POOL_SIZE; idx++)
				if (q->pool[idx] != 0)
					break;
		}
		if (idx < MAX_QUEUE_POOL_SIZE) {
			pool_idx = ((q->pool[idx] >> 8) & 0xff) - 1;
			di_buf_idx = q->pool[idx] & 0xff;
			if (pool_idx < VFRAME_TYPE_NUM) {
				if (di_buf_idx < di_buf_pool[pool_idx].size)
					di_buf = &(di_buf_pool[pool_idx].
						di_buf_ptr[di_buf_idx]);
			}
		}
	}

	if ((di_buf) && ((((pool_idx + 1) << 8) | di_buf_idx) !=
			 ((di_buf->type << 8) | (di_buf->index)))) {

		pr_dbg("%s: Error (%x,%x)\n", __func__,
			(((pool_idx + 1) << 8) | di_buf_idx),
			((di_buf->type << 8) | (di_buf->index)));

		if (recovery_flag == 0) {
			recovery_log_reason = 2;
			recovery_log_queue_idx = queue_idx;
			recovery_log_di_buf = di_buf;
		}
		recovery_flag++;
		di_buf = NULL;
	}

	if (di_log_flag & DI_LOG_QUEUE) {
		if (di_buf)
			di_print("%s: %x(%d,%d)\n", __func__, di_buf,
				pool_idx, di_buf_idx);
		else
			di_print("%s: %x\n", __func__, di_buf);
	}

	return di_buf;
}

static void queue_out(struct di_buf_s *di_buf)
{
	int i;
	queue_t *q;

	if (di_buf == NULL) {

		pr_dbg("%s:Error\n", __func__);

		if (recovery_flag == 0)
			recovery_log_reason = 3;

		recovery_flag++;
		return;
	}
	if (di_buf->queue_index >= 0 && di_buf->queue_index < QUEUE_NUM) {
		q = &(queue[di_buf->queue_index]);

		if (di_log_flag & DI_LOG_QUEUE)
			di_print("%s:<%d:%d,%d,%d> %x\n", __func__,
				di_buf->queue_index, q->num, q->in_idx,
				q->out_idx, di_buf);

		if (q->num > 0) {
			if (q->type == 0) {
				if (q->pool[q->out_idx] ==
				    ((di_buf->type << 8) | (di_buf->index))) {
					q->num--;
					q->pool[q->out_idx] = 0;
					q->out_idx++;
					if (q->out_idx >= MAX_QUEUE_POOL_SIZE)
						q->out_idx = 0;
					di_buf->queue_index = -1;
				} else {

					pr_dbg(
						"%s: Error (%d, %x,%x)\n",
						__func__,
						di_buf->queue_index,
						q->pool[q->out_idx],
						((di_buf->type << 8) |
						(di_buf->index)));

					if (recovery_flag == 0) {
						recovery_log_reason = 4;
						recovery_log_queue_idx =
							di_buf->queue_index;
						recovery_log_di_buf = di_buf;
					}
					recovery_flag++;
				}
			} else if (q->type == 1) {
				int pool_val =
					(di_buf->type << 8) | (di_buf->index);
				for (i = 0; i < MAX_QUEUE_POOL_SIZE; i++) {
					if (q->pool[i] == pool_val) {
						q->num--;
						q->pool[i] = 0;
						di_buf->queue_index = -1;
						break;
					}
				}
				if (i == MAX_QUEUE_POOL_SIZE) {

					pr_dbg("%s: Error\n", __func__);

					if (recovery_flag == 0) {
						recovery_log_reason = 5;
						recovery_log_queue_idx =
							di_buf->queue_index;
						recovery_log_di_buf = di_buf;
					}
					recovery_flag++;
				}
			} else if (q->type == 2) {
				int pool_val =
					(di_buf->type << 8) | (di_buf->index);
				if ((di_buf->index < MAX_QUEUE_POOL_SIZE) &&
				    (q->pool[di_buf->index] == pool_val)) {
					q->num--;
					q->pool[di_buf->index] = 0;
					di_buf->queue_index = -1;
				} else {

					pr_dbg("%s: Error\n", __func__);

					if (recovery_flag == 0) {
						recovery_log_reason = 5;
						recovery_log_queue_idx =
							di_buf->queue_index;
						recovery_log_di_buf = di_buf;
					}
					recovery_flag++;
				}
			}
		}
	} else {

		pr_dbg("%s: Error, queue_index %d is not right\n",
			__func__, di_buf->queue_index);

		if (recovery_flag == 0) {
			recovery_log_reason = 6;
			recovery_log_queue_idx = 0;
			recovery_log_di_buf = di_buf;
		}
		recovery_flag++;
	}

	if (di_log_flag & DI_LOG_QUEUE)
		di_print("%s done\n", __func__);

}

static void queue_in(struct di_buf_s *di_buf, int queue_idx)
{
	queue_t *q = NULL;

	if (di_buf == NULL) {
		pr_dbg("%s:Error\n", __func__);
		if (recovery_flag == 0) {
			recovery_log_reason = 7;
			recovery_log_queue_idx = queue_idx;
			recovery_log_di_buf = di_buf;
		}
		recovery_flag++;
		return;
	}
	if (di_buf->queue_index != -1) {
		pr_dbg("%s:%s[%d] Error, queue_index(%d) is not -1\n",
			__func__, vframe_type_name[di_buf->type],
			di_buf->index, di_buf->queue_index);
		if (recovery_flag == 0) {
			recovery_log_reason = 8;
			recovery_log_queue_idx = queue_idx;
			recovery_log_di_buf = di_buf;
		}
		recovery_flag++;
		return;
	}
	q = &(queue[queue_idx]);
	if (di_log_flag & DI_LOG_QUEUE)
		di_print("%s:<%d:%d,%d,%d> %x\n", __func__, queue_idx,
			q->num, q->in_idx, q->out_idx, di_buf);

	if (q->type == 0) {
		q->pool[q->in_idx] = (di_buf->type << 8) | (di_buf->index);
		di_buf->queue_index = queue_idx;
		q->in_idx++;
		if (q->in_idx >= MAX_QUEUE_POOL_SIZE)
			q->in_idx = 0;

		q->num++;
	} else if (q->type == 1) {
		int i;

		for (i = 0; i < MAX_QUEUE_POOL_SIZE; i++) {
			if (q->pool[i] == 0) {
				q->pool[i] = (di_buf->type<<8)|(di_buf->index);
				di_buf->queue_index = queue_idx;
				q->num++;
				break;
			}
		}
		if (i == MAX_QUEUE_POOL_SIZE) {
			pr_dbg("%s: Error\n", __func__);
			if (recovery_flag == 0) {
				recovery_log_reason = 9;
				recovery_log_queue_idx = queue_idx;
			}
			recovery_flag++;
		}
	} else if (q->type == 2) {
		if ((di_buf->index < MAX_QUEUE_POOL_SIZE) &&
		    (q->pool[di_buf->index] == 0)) {
			q->pool[di_buf->index] =
				(di_buf->type << 8) | (di_buf->index);
			di_buf->queue_index = queue_idx;
			q->num++;
		} else {
			pr_dbg("%s: Error\n", __func__);
			if (recovery_flag == 0) {
				recovery_log_reason = 9;
				recovery_log_queue_idx = queue_idx;
			}
			recovery_flag++;
		}
	}

	if (di_log_flag & DI_LOG_QUEUE)
		di_print("%s done\n", __func__);
}

static int list_count(int queue_idx)
{
	return queue[queue_idx].num;
}

static bool queue_empty(int queue_idx)
{
	bool ret = (queue[queue_idx].num == 0);

	return ret;
}

static bool is_in_queue(struct di_buf_s *di_buf, int queue_idx)
{
	bool ret = 0;
	struct di_buf_s *p = NULL;
	int itmp;
	unsigned int overflow_cnt;

	overflow_cnt = 0;
	if ((di_buf == NULL) || (queue_idx < 0) || (queue_idx >= QUEUE_NUM)) {
		ret = 0;
		di_print("%s: not in queue:%d!!!\n", __func__, queue_idx);
		return ret;
	}
	queue_for_each_entry(p, ptmp, queue_idx, list) {
		if (p == di_buf) {
			ret = 1;
			break;
		}
		if (overflow_cnt++ > MAX_QUEUE_POOL_SIZE) {
			ret = 0;
			di_print("%s: overflow_cnt!!!\n", __func__);
			break;
		}
	}
	return ret;
}
//---------------------------
u8 *di_vmap(ulong addr, u32 size, bool *bflg)
{
	u8 *vaddr = NULL;
	ulong phys = addr;
	u32 offset = phys & ~PAGE_MASK;
	u32 npages = PAGE_ALIGN(size) / PAGE_SIZE;
	struct page **pages = NULL;
	pgprot_t pgprot;
	int i;

	if (!PageHighMem(phys_to_page(phys)))
		return phys_to_virt(phys);

	if (offset)
		npages++;

	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return NULL;

	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}

	/*nocache*/
	pgprot = pgprot_writecombine(PAGE_KERNEL);

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_err("the phy(%lx) vmaped fail, size: %d\n",
			addr - offset, npages << PAGE_SHIFT);
		vfree(pages);
		return NULL;
	}

	vfree(pages);

//	if (debug_mode & 0x20) {
//		di_print("[HIGH-MEM-MAP] %s, pa(%lx) to va(%p), size: %d\n",
//			__func__, addr, vaddr + offset, npages << PAGE_SHIFT);
//	}
	*bflg = true;

	return vaddr + offset;
}

void di_unmap_phyaddr(u8 *vaddr)
{
	void *addr = (void *)(PAGE_MASK & (ulong)vaddr);

	vunmap(addr);
}
//-------------------------
static ssize_t
store_dump_mem(struct device *dev, struct device_attribute *attr,
	       const char *buf, size_t len)
{
	unsigned int n = 0, canvas_w = 0, canvas_h = 0;
	unsigned long nr_size = 0, dump_adr = 0;
	struct di_buf_s *di_buf = NULL;
	struct vframe_s *post_vf = NULL;
	char *buf_orig, *ps, *token;
	char *parm[3] = { NULL };
	char delim1[3] = " ";
	char delim2[2] = "\n";
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buff = NULL;
	mm_segment_t old_fs;
	bool bflg_vmap = false;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (strcmp(parm[0], "capture") == 0)
		di_buf = di_pre_stru.di_mem_buf_dup_p;
	else if (strcmp(parm[0], "capture_post") == 0) {
		if (di_vf_peek(NULL)) {
			post_vf = di_vf_get(NULL);
			if (!IS_ERR_OR_NULL(post_vf)) {
				di_buf = post_vf->private_data;
				di_vf_put(post_vf, NULL);
			}
			post_vf = NULL;
		}
	} else if (strcmp(parm[0], "capture_nrds") == 0)
		get_nr_ds_buf(&dump_adr, &nr_size);
	else {
		pr_err("wrong dump cmd\n");
		return len;
	}
	if (nr_size == 0) {
		if (unlikely(di_buf == NULL))
			return len;
		canvas_w = di_buf->canvas_width[NR_CANVAS];
		canvas_h = di_buf->canvas_height;
		nr_size = canvas_w * canvas_h * 2;
		dump_adr = di_buf->nr_adr;
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
/* pr_dbg("dump path =%s\n",dump_path); */
		filp = filp_open(parm[1], O_RDWR | O_CREAT, 0666);
		if (IS_ERR(filp)) {
			pr_err("create %s error.\n", parm[1]);
			return len;
		}
		dump_state_flag = 1;
		if (de_devp->flags & DI_MAP_FLAG) {
			//buff = (void *)phys_to_virt(dump_adr);
			buff = di_vmap(dump_adr, nr_size, &bflg_vmap);
			if (buff == NULL) {
				pr_info("di_vap err\n");
				filp_close(filp, NULL);
				return len;

			}
		} else {
			buff = ioremap(dump_adr, nr_size);
		}
		if (IS_ERR_OR_NULL(buff))
			pr_err("%s: ioremap error.\n", __func__);
		vfs_write(filp, buff, nr_size, &pos);
/*	pr_dbg("di_chan2_buf_dup_p:\n	nr:%u,mtn:%u,cnt:%u\n",
 * di_pre_stru.di_chan2_buf_dup_p->nr_adr,
 * di_pre_stru.di_chan2_buf_dup_p->mtn_adr,
 * di_pre_stru.di_chan2_buf_dup_p->cnt_adr);
 * pr_dbg("di_inp_buf:\n	nr:%u,mtn:%u,cnt:%u\n",
 * di_pre_stru.di_inp_buf->nr_adr,
 * di_pre_stru.di_inp_buf->mtn_adr,
 * di_pre_stru.di_inp_buf->cnt_adr);
 * pr_dbg("di_wr_buf:\n	nr:%u,mtn:%u,cnt:%u\n",
 * di_pre_stru.di_wr_buf->nr_adr,
 * di_pre_stru.di_wr_buf->mtn_adr,
 * di_pre_stru.di_wr_buf->cnt_adr);
 * pr_dbg("di_mem_buf_dup_p:\n  nr:%u,mtn:%u,cnt:%u\n",
 * di_pre_stru.di_mem_buf_dup_p->nr_adr,
 * di_pre_stru.di_mem_buf_dup_p->mtn_adr,
 * di_pre_stru.di_mem_buf_dup_p->cnt_adr);
 * pr_dbg("di_mem_start=%u\n",di_mem_start);
 */
		vfs_fsync(filp, 0);
		pr_info("write buffer 0x%lx  to %s.\n", dump_adr, parm[1]);
		if (bflg_vmap)
			di_unmap_phyaddr(buff);


		if (!(de_devp->flags & DI_MAP_FLAG))
			iounmap(buff);
		dump_state_flag = 0;
		filp_close(filp, NULL);
		set_fs(old_fs);
	return len;
}

#define is_from_vdin(vframe) (vframe->type & VIDTYPE_VIU_422)
static void recycle_vframe_type_pre(struct di_buf_s *di_buf);
static void recycle_vframe_type_post(struct di_buf_s *di_buf);
static void add_dummy_vframe_type_pre(struct di_buf_s *src_buf);
#ifdef DI_BUFFER_DEBUG
static void
recycle_vframe_type_post_print(struct di_buf_s *di_buf,
				const char *func,
				const int line);
#endif

static void dis2_di(void)
{
	ulong flags = 0, irq_flag2 = 0;

	init_flag = 0;
	di_lock_irqfiq_save(irq_flag2);
/* vf_unreg_provider(&di_vf_prov); */
	vf_light_unreg_provider(&di_vf_prov);
	di_unlock_irqfiq_restore(irq_flag2);
	reg_flag = 0;
	spin_lock_irqsave(&plist_lock, flags);
	di_lock_irqfiq_save(irq_flag2);
	if (di_pre_stru.di_inp_buf) {
		if (vframe_in[di_pre_stru.di_inp_buf->index]) {
			vf_put(
				vframe_in[di_pre_stru.di_inp_buf->index],
				VFM_NAME);
			vframe_in[di_pre_stru.di_inp_buf->index] = NULL;
			vf_notify_provider(
				VFM_NAME, VFRAME_EVENT_RECEIVER_PUT, NULL);
		}
		di_pre_stru.di_inp_buf->invert_top_bot_flag = 0;
		queue_in(di_pre_stru.di_inp_buf, QUEUE_IN_FREE);
		di_pre_stru.di_inp_buf = NULL;
	}
	di_uninit_buf(0);
	if (get_blackout_policy()) {
		DI_Wr(DI_CLKG_CTRL, 0x2);
		if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
			enable_di_post_mif(GATE_OFF);
			di_post_gate_control(false);
			di_top_gate_control(false, false);
		}
	}

	if (post_wr_en && post_wr_support)
		diwr_set_power_control(0);

	di_unlock_irqfiq_restore(irq_flag2);
	spin_unlock_irqrestore(&plist_lock, flags);
}

static ssize_t
store_config(struct device *dev,
	     struct device_attribute *attr,
	     const char *buf, size_t count)
{
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_cmd_params(buf_orig, (char **)(&parm));

	if (strncmp(buf, "disable", 7) == 0) {

		di_print("%s: disable\n", __func__);

		if (init_flag) {
			di_pre_stru.disable_req_flag = 1;

			trigger_pre_di_process(TRIGGER_PRE_BY_DEBUG_DISABLE);
			while (di_pre_stru.disable_req_flag)
				usleep_range(1000, 1001);
		}
	} else if (strncmp(buf, "dis2", 4) == 0) {
		dis2_di();
	} else if (strcmp(parm[0], "hold_video") == 0) {
		pr_info("%s(%s %s)\n", __func__, parm[0], parm[1]);
		rc = kstrtouint(parm[1], 10, &hold_video);
	}
	kfree(buf_orig);
	return count;
}

static unsigned char is_progressive(vframe_t *vframe)
{
	unsigned char ret = 0;

	ret = ((vframe->type & VIDTYPE_TYPEMASK) == VIDTYPE_PROGRESSIVE);
	return ret;
}

static unsigned char is_source_change(vframe_t *vframe)
{
#define VFRAME_FORMAT_MASK      \
	(VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_444 | \
	 VIDTYPE_MVC)
	if ((di_pre_stru.cur_width != vframe->width) ||
	(di_pre_stru.cur_height != vframe->height) ||
	(((di_pre_stru.cur_inp_type & VFRAME_FORMAT_MASK) !=
	(vframe->type & VFRAME_FORMAT_MASK)) &&
	(!is_handle_prog_frame_as_interlace(vframe))) ||
	(di_pre_stru.cur_source_type != vframe->source_type)) {
		/* video format changed */
		return 1;
	} else if (
	((di_pre_stru.cur_prog_flag != is_progressive(vframe)) &&
	(!is_handle_prog_frame_as_interlace(vframe))) ||
	((di_pre_stru.cur_inp_type & VIDTYPE_VIU_FIELD) !=
	(vframe->type & VIDTYPE_VIU_FIELD))
	) {
		/* just scan mode changed */
		if (!di_pre_stru.force_interlace)
			pr_dbg("DI I<->P.\n");
		return 2;
	}
	return 0;
}
/*
 * static unsigned char is_vframe_type_change(vframe_t* vframe)
 * {
 * if(
 * (di_pre_stru.cur_prog_flag!=is_progressive(vframe))||
 * ((di_pre_stru.cur_inp_type&VFRAME_FORMAT_MASK)!=
 * (vframe->type&VFRAME_FORMAT_MASK))
 * )
 * return 1;
 *
 * return 0;
 * }
 */
static int trick_mode;

unsigned char is_bypass(vframe_t *vf_in)
{
	unsigned int vtype = 0;
	int ret = 0;
	static vframe_t vf_tmp;

	if (di_debug_flag & 0x10000) /* for debugging */
		return (di_debug_flag >> 17) & 0x1;

	if (bypass_all)
		return 1;
	if (di_pre_stru.cur_prog_flag &&
	    (
	     (di_pre_stru.cur_width > 1920) || (di_pre_stru.cur_height > 1080)
	     || (di_pre_stru.cur_inp_type & VIDTYPE_VIU_444))
	    )
		return 1;

	if ((di_pre_stru.cur_width < 16) || (di_pre_stru.cur_height < 16))
		return 1;

	if (di_pre_stru.cur_inp_type & VIDTYPE_MVC)
		return 1;

	if (di_pre_stru.cur_source_type == VFRAME_SOURCE_TYPE_PPMGR)
		return 1;

	if (bypass_trick_mode) {
		int trick_mode_fffb = 0;
		int trick_mode_i = 0;

		if (bypass_trick_mode&0x1)
			query_video_status(0, &trick_mode_fffb);
		if (bypass_trick_mode&0x2)
			query_video_status(1, &trick_mode_i);
		trick_mode = trick_mode_fffb | (trick_mode_i << 1);
		if (trick_mode)
			return 1;
	}

	if (bypass_3d &&
	    (di_pre_stru.source_trans_fmt != 0))
		return 1;

/*prot is conflict with di post*/
	if (vf_in && vf_in->video_angle)
		return 1;
	if (vf_in && (vf_in->type & VIDTYPE_PIC))
		return 1;
#if 0
	if (vf_in && (vf_in->type & VIDTYPE_COMPRESS))
		return 1;
#endif
	if ((di_vscale_skip_enable & 0x4) &&
		vf_in && !post_wr_en) {
		//--------------------------------------
		if (vf_in->type & VIDTYPE_COMPRESS) {
			vf_tmp.width = vf_in->compWidth;
			vf_tmp.height = vf_in->compHeight;
			if (vf_tmp.width > 1920 || vf_tmp.height > 1088)
				return 1;

		}
		//--------------------------------------
		/*backup vtype,set type as progressive*/
		vtype = vf_in->type;
		vf_in->type &= (~VIDTYPE_TYPEMASK);
		vf_in->type &= (~VIDTYPE_VIU_NV21);
		vf_in->type |= VIDTYPE_VIU_SINGLE_PLANE;
		vf_in->type |= VIDTYPE_VIU_FIELD;
		vf_in->type |= VIDTYPE_PRE_INTERLACE;
		vf_in->type |= VIDTYPE_VIU_422;
		ret = get_current_vscale_skip_count(vf_in);
		di_vscale_skip_count = (ret&0xff);
		vpp_3d_mode = ((ret>>8)&0xff);
		vf_in->type = vtype;
		if (di_vscale_skip_count > 0 ||
			(vpp_3d_mode
			#ifdef DET3D
			&& (!det3d_en)
			#endif
			)
			)
			return 1;
	}

	return 0;
}

static unsigned char is_bypass_post(void)
{
	if (di_debug_flag & 0x40000) /* for debugging */
		return (di_debug_flag >> 19) & 0x1;

	/*prot is conflict with di post*/
	if (di_pre_stru.orientation)
		return 1;
	if (bypass_post)
		return 1;


#ifdef DET3D
	if (di_pre_stru.vframe_interleave_flag != 0)
		return 1;

#endif
	return 0;
}

#ifdef RUN_DI_PROCESS_IN_IRQ
static unsigned char is_input2pre(void)
{
	if (input2pre
	    && di_pre_stru.cur_prog_flag
	    && di_pre_stru.vdin_source
	    && (bypass_state == 0))
		return 1;

	return 0;
}
#endif

#ifdef DI_USE_FIXED_CANVAS_IDX
static int di_post_idx[2][6];
static int di_pre_idx[2][10];
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static unsigned int di_inp_idx[3];
#else
static int di_wr_idx;
#endif
static int di_get_canvas(void)
{
	unsigned int pre_num = 7, post_num = 6, i = 0;

	if (mcpre_en) {
		/* mem/chan2/nr/mtn/contrd/contrd2/
		 * contw/mcinfrd/mcinfow/mcvecw
		 */
		pre_num = 10;
		/* buf0/buf1/buf2/mtnp/mcvec */
		post_num = 6;
	}
	if (canvas_pool_alloc_canvas_table("di_pre",
	&di_pre_idx[0][0], pre_num, CANVAS_MAP_TYPE_1)) {
		pr_err("%s allocate di pre canvas error.\n", __func__);
		return 1;
	}
	if (di_pre_rdma_enable) {
		if (canvas_pool_alloc_canvas_table("di_pre",
		&di_pre_idx[1][0], pre_num, CANVAS_MAP_TYPE_1)) {
			pr_err("%s allocate di pre canvas error.\n", __func__);
			return 1;
		}
	} else {
		for (i = 0; i < pre_num; i++)
			di_pre_idx[1][i] = di_pre_idx[0][i];
	}
	if (canvas_pool_alloc_canvas_table("di_post",
		    &di_post_idx[0][0], post_num, CANVAS_MAP_TYPE_1)) {
		pr_err("%s allocate di post canvas error.\n", __func__);
		return 1;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	if (canvas_pool_alloc_canvas_table("di_post",
		    &di_post_idx[1][0], post_num, CANVAS_MAP_TYPE_1)) {
		pr_err("%s allocate di post canvas error.\n", __func__);
		return 1;
	}
#else
	for (i = 0; i < post_num; i++)
		di_post_idx[1][i] = di_post_idx[0][i];
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (canvas_pool_alloc_canvas_table("di_inp", &di_inp_idx[0], 3,
			CANVAS_MAP_TYPE_1)) {
		pr_err("%s allocat di inp canvas error.\n", __func__);
		return 1;
	}
	pr_info("DI: support multi decoding %u~%u~%u.\n",
		di_inp_idx[0], di_inp_idx[1], di_inp_idx[2]);
#endif
	if (de_devp->post_wr_support == 0)
		return 0;

#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (canvas_pool_alloc_canvas_table("di_wr",
			&di_wr_idx, 1, CANVAS_MAP_TYPE_1)) {
		pr_err("%s allocat di write back canvas error.\n",
			__func__);
			return 1;
	}
	pr_info("DI: support post write back %u.\n", di_wr_idx);
#endif
	return 0;
}

static void config_canvas_idx(struct di_buf_s *di_buf, int nr_canvas_idx,
	int mtn_canvas_idx)
{
	unsigned int height = 0;

	if (!di_buf)
		return;
	if (di_buf->canvas_config_flag == 1) {
		if (nr_canvas_idx >= 0) {
			/* linked two interlace buffer should double height*/
			if (di_buf->di_wr_linked_buf)
				height = (di_buf->canvas_height << 1);
			else
				height =  di_buf->canvas_height;
			di_buf->nr_canvas_idx = nr_canvas_idx;
			canvas_config(nr_canvas_idx, di_buf->nr_adr,
				di_buf->canvas_width[NR_CANVAS], height, 0, 0);
		}
	} else if (di_buf->canvas_config_flag == 2) {
		if (nr_canvas_idx >= 0) {
			di_buf->nr_canvas_idx = nr_canvas_idx;
			canvas_config(nr_canvas_idx, di_buf->nr_adr,
				di_buf->canvas_width[NR_CANVAS],
				di_buf->canvas_height, 0, 0);
		}
		if (mtn_canvas_idx >= 0) {
			di_buf->mtn_canvas_idx = mtn_canvas_idx;
			canvas_config(mtn_canvas_idx, di_buf->mtn_adr,
				di_buf->canvas_width[MTN_CANVAS],
				di_buf->canvas_height, 0, 0);
		}
	}
	if (nr_canvas_idx >= 0) {
		di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
		di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
	}
}

static void config_cnt_canvas_idx(struct di_buf_s *di_buf,
	unsigned int cnt_canvas_idx)
{

	if (!di_buf)
		return;

	di_buf->cnt_canvas_idx = cnt_canvas_idx;
	canvas_config(
		cnt_canvas_idx, di_buf->cnt_adr,
		di_buf->canvas_width[MTN_CANVAS],
		di_buf->canvas_height, 0, 0);
}

static void config_mcinfo_canvas_idx(struct di_buf_s *di_buf,
	int mcinfo_canvas_idx)
{

	if (!di_buf)
		return;

	di_buf->mcinfo_canvas_idx = mcinfo_canvas_idx;
	canvas_config(
		mcinfo_canvas_idx, di_buf->mcinfo_adr,
		di_buf->canvas_height, 2, 0, 0);
}
static void config_mcvec_canvas_idx(struct di_buf_s *di_buf,
	int mcvec_canvas_idx)
{

	if (!di_buf)
		return;

	di_buf->mcvec_canvas_idx = mcvec_canvas_idx;
	canvas_config(
		mcvec_canvas_idx, di_buf->mcvec_adr,
		di_buf->canvas_width[MV_CANVAS],
		di_buf->canvas_height, 0, 0);
}


#else

static void config_canvas(struct di_buf_s *di_buf)
{
	unsigned int height = 0;

	if (!di_buf)
		return;

	if (di_buf->canvas_config_flag == 1) {
		/* linked two interlace buffer should double height*/
		if (di_buf->di_wr_linked_buf)
			height = (di_buf->canvas_height << 1);
		else
			height =  di_buf->canvas_height;
		canvas_config(di_buf->nr_canvas_idx, di_buf->nr_adr,
			di_buf->canvas_width[NR_CANVAS], height, 0, 0);
		di_buf->canvas_config_flag = 0;
	} else if (di_buf->canvas_config_flag == 2) {
		canvas_config(di_buf->nr_canvas_idx, di_buf->nr_adr,
			di_buf->canvas_width[MV_CANVAS],
			di_buf->canvas_height, 0, 0);
		canvas_config(di_buf->mtn_canvas_idx, di_buf->mtn_adr,
			di_buf->canvas_width[MTN_CANVAS],
			di_buf->canvas_height, 0, 0);
		di_buf->canvas_config_flag = 0;
	}
}

#endif
#ifdef CONFIG_CMA
static unsigned int di_cma_alloc_total(struct di_dev_s *de_devp)
{
	de_devp->total_pages =
		dma_alloc_from_contiguous(&(de_devp->pdev->dev),
		de_devp->mem_size >> PAGE_SHIFT, 0);
	if (de_devp->total_pages) {
		de_devp->mem_start =
			page_to_phys(de_devp->total_pages);
		pr_dbg("%s:DI CMA allocate addr:0x%lx ok.\n",
			__func__, de_devp->mem_start);
		return 1;
	}
	pr_err("%s:xxxxxx DI CMA allocate fail.\n", __func__);
	if (de_devp->flag_cma != 0 && de_devp->nrds_enable) {
		nr_ds_buf_init(de_devp->flag_cma, 0,
			&(de_devp->pdev->dev));
	}

	return 0;


}

static bool cma_print;
static unsigned int di_cma_alloc(struct di_dev_s *devp)
{
	unsigned int start_time, end_time, delta_time;
	struct di_buf_s *buf_p = NULL;
	int itmp, alloc_cnt = 0;

	start_time = jiffies_to_msecs(jiffies);
	queue_for_each_entry(buf_p, ptmp, QUEUE_LOCAL_FREE, list) {

		if (buf_p->pages == NULL) {
			buf_p->pages =
			dma_alloc_from_contiguous(&(devp->pdev->dev),
				devp->buffer_size >> PAGE_SHIFT, 0);
			if (IS_ERR_OR_NULL(buf_p->pages)) {
				buf_p->pages = NULL;
				pr_err("xxxxxxxxx DI CMA  allocate %d fail.\n",
					buf_p->index);
				return 0;
			} else {
				alloc_cnt++;
				if (cma_print)
					pr_info("DI CMA  allocate buf[%d]page:0x%p\n",
						buf_p->index, buf_p->pages);
			}
		} else {
			if (cma_print) {
				pr_err("DI buf[%d] page:0x%p cma alloced skip\n",
					buf_p->index, buf_p->pages);
			}
		}
		buf_p->nr_adr = page_to_phys(buf_p->pages);
		if (cma_print)
			pr_info(" addr 0x%lx ok.\n", buf_p->nr_adr);
		if (di_pre_stru.buf_alloc_mode == 0) {
			buf_p->mtn_adr = buf_p->nr_adr +
				di_pre_stru.nr_size;
			buf_p->cnt_adr = buf_p->nr_adr +
				di_pre_stru.nr_size +
				di_pre_stru.mtn_size;
			if (mc_mem_alloc) {
				buf_p->mcvec_adr = buf_p->nr_adr +
					di_pre_stru.nr_size +
					di_pre_stru.mtn_size +
					di_pre_stru.count_size;
				buf_p->mcinfo_adr =
					buf_p->nr_adr +
					di_pre_stru.nr_size +
					di_pre_stru.mtn_size +
					di_pre_stru.count_size +
					di_pre_stru.mv_size;
			}
		}
	}
	if (post_wr_en && post_wr_support) {
		queue_for_each_entry(buf_p, ptmp, QUEUE_POST_FREE, list) {
			if (buf_p->pages == NULL) {
				buf_p->pages =
					dma_alloc_from_contiguous(
					&(devp->pdev->dev),
					devp->post_buffer_size>>PAGE_SHIFT,
					0);
				if (IS_ERR_OR_NULL(buf_p->pages)) {
					buf_p->pages = NULL;
					pr_err("xxxxxxxxx DI CMA allocate %d fail.\n",
						buf_p->index);
					return 0;
				}
				alloc_cnt++;
				if (cma_print)
					pr_info("DI CMA allocate buf[%d]page:0x%p\n",
						buf_p->index,
						buf_p->pages);
			} else {
				pr_err("DI buf[%d] page:0x%p cma alloced skip\n",
					buf_p->index, buf_p->pages);
			}
			buf_p->nr_adr = page_to_phys(buf_p->pages);
			if (cma_print)
				pr_info(" addr 0x%lx ok.\n", buf_p->nr_adr);
		}
	}
	if (de_devp->flag_cma != 0 && de_devp->nrds_enable) {
		nr_ds_buf_init(de_devp->flag_cma, 0,
			&(de_devp->pdev->dev));
	}

	end_time = jiffies_to_msecs(jiffies);
	delta_time = end_time - start_time;
	pr_info("%s:alloc %u buffer use %u ms(%u~%u)\n",
			__func__, alloc_cnt, delta_time, start_time, end_time);
	return 1;
}

static void di_cma_release(struct di_dev_s *devp)
{
	unsigned int i, ii, rels_cnt = 0, start_time, end_time, delta_time;
	struct di_buf_s *buf_p, *keep_buf;

	keep_buf = di_post_stru.keep_buf;
	start_time = jiffies_to_msecs(jiffies);
	for (i = 0; (i < devp->buf_num_avail); i++) {
		buf_p = &(di_buf_local[i]);
		ii = USED_LOCAL_BUF_MAX;
		if (!IS_ERR_OR_NULL(keep_buf)) {
			for (ii = 0; ii < USED_LOCAL_BUF_MAX; ii++) {
				if (buf_p == keep_buf->di_buf_dup_p[ii]) {
					pr_dbg("%s skip buf[%d].\n\n",
						__func__, i);
					break;
				}
			}
		}
		if ((ii >= USED_LOCAL_BUF_MAX) &&
			(buf_p->pages != NULL)) {
			if (dma_release_from_contiguous(&(devp->pdev->dev),
					buf_p->pages,
					devp->buffer_size >> PAGE_SHIFT)) {
				buf_p->pages = NULL;
				rels_cnt++;
				if (cma_print)
					pr_info(
					"DI CMA  release buf[%d] ok.\n", i);
			} else {
				pr_err("DI CMA  release buf[%d] fail.\n", i);
			}
		} else {
			if (!IS_ERR_OR_NULL(buf_p->pages) && cma_print) {
				pr_info("DI buf[%d] page:0x%p no release.\n",
					buf_p->index, buf_p->pages);
			}
		}
	}
	if (post_wr_en && post_wr_support) {
		for (i = 0; i < di_post_stru.di_post_num; i++) {
			buf_p = &(di_buf_post[i]);
			if (buf_p->pages != NULL) {
				if (dma_release_from_contiguous(
						&(devp->pdev->dev),
						buf_p->pages,
						devp->post_buffer_size>>
						PAGE_SHIFT)) {
					buf_p->pages = NULL;
					rels_cnt++;
					if (cma_print)
						pr_info(
					"DI CMA release post buf[%d] ok.\n", i);
				} else {
					pr_err("DI CMA release post buf[%d] fail.\n",
						i);
				}
			}
		}
	}
	if (de_devp->nrds_enable) {
		nr_ds_buf_uninit(de_devp->flag_cma,
			&(de_devp->pdev->dev));
	}

	end_time = jiffies_to_msecs(jiffies);
	delta_time = end_time - start_time;
	pr_info("%s:release %u buffer use %u ms(%u~%u)\n",
			__func__, rels_cnt, delta_time, start_time, end_time);
}
#endif
static int di_init_buf(int width, int height, unsigned char prog_flag)
{
	int i;
	int canvas_height = height + 8;
	struct page *tmp_page = NULL;
	unsigned int di_buf_size = 0, di_post_buf_size = 0, mtn_size = 0;
	unsigned int nr_size = 0, count_size = 0, mv_size = 0, mc_size = 0;
	unsigned int nr_width = width, mtn_width = width, mv_width = width;
	unsigned int nr_canvas_width = width, mtn_canvas_width = width;
	unsigned int mv_canvas_width = width, canvas_align_width = 32;
	unsigned long di_post_mem = 0, nrds_mem = 0;
	struct di_buf_s *keep_buf = di_post_stru.keep_buf;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	frame_count = 0;
	disp_frame_count = 0;
	cur_post_ready_di_buf = NULL;
	/* decoder'buffer had been releae no need put */
	for (i = 0; i < MAX_IN_BUF_NUM; i++)
		vframe_in[i] = NULL;
	memset(&di_pre_stru, 0, sizeof(di_pre_stru));
	if (nr10bit_support) {
		if (full_422_pack)
			nr_width = (width * 5) / 4;
		else
			nr_width = (width * 3) / 2;
	} else {
		nr_width = width;
	}
	/* make sure canvas width must be divided by 256bit|32byte align */
	nr_canvas_width = nr_width << 1;
	mtn_canvas_width = mtn_width >> 1;
	mv_canvas_width = (mv_width << 1) / 5;
	nr_canvas_width = roundup(nr_canvas_width, canvas_align_width);
	mtn_canvas_width = roundup(mtn_canvas_width, canvas_align_width);
	mv_canvas_width = roundup(mv_canvas_width, canvas_align_width);
	nr_width = nr_canvas_width >> 1;
	mtn_width = mtn_canvas_width << 1;
	mv_width = (mv_canvas_width * 5) >> 1;

	if (prog_flag) {
		di_pre_stru.prog_proc_type = 1;
		di_pre_stru.buf_alloc_mode = 1;
		di_buf_size = nr_width * canvas_height * 2;
		di_buf_size = roundup(di_buf_size, PAGE_SIZE);
		de_devp->buf_num_avail = de_devp->mem_size / di_buf_size;
		if (de_devp->buf_num_avail > (2 * MAX_LOCAL_BUF_NUM))
			de_devp->buf_num_avail = 2 * MAX_LOCAL_BUF_NUM;

	} else {
		di_pre_stru.prog_proc_type = 0;
		di_pre_stru.buf_alloc_mode = 0;
		/*nr_size(bits)=w*active_h*8*2(yuv422)
		 * mtn(bits)=w*active_h*4
		 * cont(bits)=w*active_h*4 mv(bits)=w*active_h/5*16
		 * mcinfo(bits)=active_h*16
		 */
		nr_size = (nr_width * canvas_height)*8*2/16;
		mtn_size = (mtn_width * canvas_height)*4/16;
		count_size = (mtn_width * canvas_height)*4/16;
		mv_size = (mv_width * canvas_height)/5;
		mc_size = roundup(canvas_height >> 1, canvas_align_width) << 1;

		if (mc_mem_alloc) {
			di_buf_size = nr_size + mtn_size + count_size +
				mv_size + mc_size;
		} else {
			di_buf_size = nr_size + mtn_size + count_size;
		}
		di_buf_size = roundup(di_buf_size, PAGE_SIZE);
		de_devp->buf_num_avail = de_devp->mem_size / di_buf_size;

		if (post_wr_en && post_wr_support) {
			de_devp->buf_num_avail = (de_devp->mem_size +
				(nr_width*canvas_height<<2)) /
				(di_buf_size + (nr_width*canvas_height<<1));
		}

		if (de_devp->buf_num_avail > MAX_LOCAL_BUF_NUM)
			de_devp->buf_num_avail = MAX_LOCAL_BUF_NUM;

	}
	de_devp->buffer_size = di_buf_size;
	di_pre_stru.nr_size = nr_size;
	di_pre_stru.count_size = count_size;
	di_pre_stru.mtn_size = mtn_size;
	di_pre_stru.mv_size = mv_size;
	di_pre_stru.mcinfo_size = mc_size;
	same_field_top_count = 0;
	same_field_bot_count = 0;

	queue_init(de_devp->buf_num_avail);
	for (i = 0; i < de_devp->buf_num_avail; i++) {
		struct di_buf_s *di_buf = &(di_buf_local[i]);
		int ii = USED_LOCAL_BUF_MAX;
		if (!IS_ERR_OR_NULL(keep_buf)) {
			for (ii = 0; ii < USED_LOCAL_BUF_MAX; ii++) {
				if (di_buf == keep_buf->di_buf_dup_p[ii]) {
					di_print("%s skip %d\n", __func__, i);
					break;
				}
			}
		}

		if (ii >= USED_LOCAL_BUF_MAX) {
			/* backup cma pages */
			tmp_page = di_buf->pages;
			memset(di_buf, 0, sizeof(struct di_buf_s));
			di_buf->pages = tmp_page;
			di_buf->type = VFRAME_TYPE_LOCAL;
			di_buf->pre_ref_count = 0;
			di_buf->post_ref_count = 0;
			di_buf->canvas_width[NR_CANVAS] = nr_canvas_width;
			di_buf->canvas_width[MTN_CANVAS] = mtn_canvas_width;
			di_buf->canvas_width[MV_CANVAS] = mv_canvas_width;

			if (prog_flag) {
				di_buf->canvas_height = canvas_height;
				di_buf->nr_adr = de_devp->mem_start +
					di_buf_size * i;
				di_buf->canvas_config_flag = 1;
			} else {
				di_buf->canvas_height = (canvas_height>>1);
				di_buf->canvas_height =
					roundup(di_buf->canvas_height,
					canvas_align_width);
				di_buf->nr_adr = de_devp->mem_start +
					di_buf_size * i;
				di_buf->mtn_adr = de_devp->mem_start +
					di_buf_size * i +
					nr_size;
				di_buf->cnt_adr = de_devp->mem_start +
					di_buf_size * i +
					nr_size + mtn_size;

				if (mc_mem_alloc) {
					di_buf->mcvec_adr = de_devp->mem_start +
						di_buf_size * i +
						nr_size + mtn_size + count_size;
					di_buf->mcinfo_adr =
						de_devp->mem_start +
						di_buf_size * i + nr_size +
						mtn_size + count_size + mv_size;
				}
				di_buf->canvas_config_flag = 2;
			}
			di_buf->index = i;
			di_buf->vframe = &(vframe_local[i]);
			di_buf->vframe->private_data = di_buf;
			di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
			di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
			di_buf->queue_index = -1;
			di_buf->invert_top_bot_flag = 0;
			queue_in(di_buf, QUEUE_LOCAL_FREE);
		}
	}
#ifdef CONFIG_CMA
	if (de_devp->flag_cma == 1) {
		pr_dbg("%s:cma alloc req time: %u ms\n",
			__func__, jiffies_to_msecs(jiffies));
		atomic_set(&de_devp->mem_flag, 0);
		di_pre_stru.cma_alloc_req = 1;
		up(&di_sema);
	}
#endif
	di_post_mem = de_devp->mem_start +
		di_buf_size*de_devp->buf_num_avail;
	if (post_wr_en && post_wr_support) {
		di_post_buf_size = nr_width * canvas_height * 2;
		/* pre buffer must 2 more than post buffer */
		if ((de_devp->buf_num_avail - 2) > MAX_POST_BUF_NUM)
			di_post_stru.di_post_num = MAX_POST_BUF_NUM;
		else
			di_post_stru.di_post_num = (de_devp->buf_num_avail - 2);
		pr_info("DI: di post buffer size %u byte.\n", di_post_buf_size);
	} else {
		di_post_stru.di_post_num = MAX_POST_BUF_NUM;
		di_post_buf_size = 0;
	}
	de_devp->post_buffer_size = di_post_buf_size;
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		struct di_buf_s *di_buf = &(di_buf_in[i]);

		if (di_buf) {
			memset(di_buf, 0, sizeof(struct di_buf_s));
			di_buf->type = VFRAME_TYPE_IN;
			di_buf->pre_ref_count = 0;
			di_buf->post_ref_count = 0;
			di_buf->vframe = &(vframe_in_dup[i]);
			di_buf->vframe->private_data = di_buf;
			di_buf->index = i;
			di_buf->queue_index = -1;
			di_buf->invert_top_bot_flag = 0;
			queue_in(di_buf, QUEUE_IN_FREE);
		}
	}

	for (i = 0; i < di_post_stru.di_post_num; i++) {
		struct di_buf_s *di_buf = &(di_buf_post[i]);

		if (di_buf) {
			if (di_buf != keep_buf) {
				memset(di_buf, 0, sizeof(struct di_buf_s));
				di_buf->type = VFRAME_TYPE_POST;
				di_buf->index = i;
				di_buf->vframe = &(vframe_post[i]);
				di_buf->vframe->private_data = di_buf;
				di_buf->queue_index = -1;
				di_buf->invert_top_bot_flag = 0;
				if (post_wr_en && post_wr_support) {
					di_buf->canvas_width[NR_CANVAS] =
						(nr_width << 1);
					di_buf->canvas_height = canvas_height;
					di_buf->canvas_config_flag = 1;
					di_buf->nr_adr = di_post_mem +
						di_post_buf_size*i;
				}
				queue_in(di_buf, QUEUE_POST_FREE);
			}
		}
	}
	if (de_devp->flag_cma == 0 && de_devp->nrds_enable) {
		nrds_mem = di_post_mem +
			di_post_stru.di_post_num * di_post_buf_size;
		nr_ds_buf_init(de_devp->flag_cma, nrds_mem,
			&(de_devp->pdev->dev));
	}
	return 0;
}

static void keep_mirror_buffer(void)
{
	struct di_buf_s *p = NULL;
	int i = 0, ii = 0, itmp;

	queue_for_each_entry(p, ptmp, QUEUE_DISPLAY, list) {
	if (p->di_buf[0]->type != VFRAME_TYPE_IN &&
		(p->process_fun_index != PROCESS_FUN_NULL) &&
		(ii < USED_LOCAL_BUF_MAX) &&
		(p->index == di_post_stru.cur_disp_index)) {
		di_post_stru.keep_buf = p;
		for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
			if (IS_ERR_OR_NULL(p->di_buf_dup_p[i]))
				continue;
			/* prepare for recycle
			 * the keep buffer
			 */
			p->di_buf_dup_p[i]->pre_ref_count = 0;
			p->di_buf_dup_p[i]->post_ref_count = 0;
			if ((p->di_buf_dup_p[i]->queue_index >= 0) &&
			(p->di_buf_dup_p[i]->queue_index < QUEUE_NUM)) {
				if (is_in_queue(p->di_buf_dup_p[i],
				p->di_buf_dup_p[i]->queue_index))
					queue_out(p->di_buf_dup_p[i]);
			}
			ii++;
			if (p->di_buf_dup_p[i]->di_wr_linked_buf)
				p->di_buf_dup_p[i+1] =
					p->di_buf_dup_p[i]->di_wr_linked_buf;
		}
		queue_out(p);
		break;
	}
	}

}
static void di_uninit_buf(unsigned int disable_mirror)
{
	struct di_buf_s *keep_buf = NULL;
	int i = 0;

	if (!queue_empty(QUEUE_DISPLAY) || disable_mirror)
		di_post_stru.keep_buf = NULL;

	if (disable_mirror != 1)
		keep_mirror_buffer();

	if (!IS_ERR_OR_NULL(di_post_stru.keep_buf)) {
		keep_buf = di_post_stru.keep_buf;
		pr_dbg("%s keep cur di_buf %d (",
			__func__, keep_buf->index);
		for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
			if (!IS_ERR_OR_NULL(keep_buf->di_buf_dup_p[i]))
				pr_dbg("%d\t",
					keep_buf->di_buf_dup_p[i]->index);
		}
		pr_dbg(")\n");
	}
	queue_init(0);
	/* decoder'buffer had been releae no need put */
	for (i = 0; i < MAX_IN_BUF_NUM; i++)
		vframe_in[i] = NULL;
	di_pre_stru.pre_de_process_done = 0;
	di_pre_stru.pre_de_process_flag = 0;
	if (post_wr_en && post_wr_support) {
		di_post_stru.cur_post_buf = NULL;
		di_post_stru.post_de_busy = 0;
		di_post_stru.de_post_process_done = 0;
		di_post_stru.post_wr_cnt = 0;
	}
	if (de_devp->flag_cma == 0 && de_devp->nrds_enable) {
		nr_ds_buf_uninit(de_devp->flag_cma,
			&(de_devp->pdev->dev));
	}
}

static void log_buffer_state(unsigned char *tag)
{
	if (di_log_flag & DI_LOG_BUFFER_STATE) {
		struct di_buf_s *p = NULL;/* , *ptmp; */
		int itmp;
		int in_free = 0;
		int local_free = 0;
		int pre_ready = 0;
		int post_free = 0;
		int post_ready = 0;
		int post_ready_ext = 0;
		int display = 0;
		int display_ext = 0;
		int recycle = 0;
		int di_inp = 0;
		int di_wr = 0;
		ulong irq_flag2 = 0;

		di_lock_irqfiq_save(irq_flag2);
		in_free = list_count(QUEUE_IN_FREE);
		local_free = list_count(QUEUE_LOCAL_FREE);
		pre_ready = list_count(QUEUE_PRE_READY);
		post_free = list_count(QUEUE_POST_FREE);
		post_ready = list_count(QUEUE_POST_READY);
		queue_for_each_entry(p, ptmp, QUEUE_POST_READY, list) {
			if (p->di_buf[0])
				post_ready_ext++;

			if (p->di_buf[1])
				post_ready_ext++;
		}
		queue_for_each_entry(p, ptmp, QUEUE_DISPLAY, list) {
			display++;
			if (p->di_buf[0])
				display_ext++;

			if (p->di_buf[1])
				display_ext++;
		}
		recycle = list_count(QUEUE_RECYCLE);

		if (di_pre_stru.di_inp_buf)
			di_inp++;
		if (di_pre_stru.di_wr_buf)
			di_wr++;

		if (buf_state_log_threshold == 0)
			buf_state_log_start = 0;
		else if (post_ready < buf_state_log_threshold)
			buf_state_log_start = 1;

		if (buf_state_log_start) {
			di_print(
		"[%s]i i_f %d/%d, l_f %d/%d, pre_r %d, post_f %d/%d,",
				tag,
				in_free, MAX_IN_BUF_NUM,
				local_free, de_devp->buf_num_avail,
				pre_ready,
				post_free, MAX_POST_BUF_NUM);
			di_print(
		"post_r (%d:%d), disp (%d:%d),rec %d, di_i %d, di_w %d\n",
				post_ready, post_ready_ext,
				display, display_ext,
				recycle,
				di_inp, di_wr
				);
		}
		di_unlock_irqfiq_restore(irq_flag2);
	}
}

static void dump_state(void)
{
	struct di_buf_s *p = NULL, *keep_buf;
	int itmp, i;

	dump_state_flag = 1;
	pr_info("version %s, init_flag %d, is_bypass %d\n",
			version_s, init_flag, is_bypass(NULL));
	pr_info("recovery_flag = %d, recovery_log_reason=%d, di_blocking=%d",
		recovery_flag, recovery_log_reason, di_blocking);
	pr_info("recovery_log_queue_idx=%d, recovery_log_di_buf=0x%p\n",
		recovery_log_queue_idx, recovery_log_di_buf);
	pr_info("buffer_size=%d, mem_flag=%d, cma_flag=%d\n",
		de_devp->buffer_size, atomic_read(&de_devp->mem_flag),
		de_devp->flag_cma);
	keep_buf = di_post_stru.keep_buf;
	pr_info("used_post_buf_index %d(0x%p),",
		IS_ERR_OR_NULL(keep_buf) ?
		-1 : keep_buf->index, keep_buf);
	if (!IS_ERR_OR_NULL(keep_buf)) {
		pr_info("used_local_buf_index:\n");
		for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
			p = keep_buf->di_buf_dup_p[i];
			pr_info("%d(0x%p) ",
			IS_ERR_OR_NULL(p) ? -1 : p->index, p);
		}
	}
	pr_info("\nin_free_list (max %d):\n", MAX_IN_BUF_NUM);
	queue_for_each_entry(p, ptmp, QUEUE_IN_FREE, list) {
		pr_info("index %2d, 0x%p, type %d\n",
			p->index, p, p->type);
	}
	pr_info("local_free_list (max %d):\n", de_devp->buf_num_avail);
	queue_for_each_entry(p, ptmp, QUEUE_LOCAL_FREE, list) {
		pr_info("index %2d, 0x%p, type %d\n", p->index, p, p->type);
	}

	pr_info("post_doing_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_POST_DOING, list) {
		print_di_buf(p, 2);
	}
	pr_info("pre_ready_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_PRE_READY, list) {
		print_di_buf(p, 2);
	}
	pr_info("post_free_list (max %d):\n", di_post_stru.di_post_num);
	queue_for_each_entry(p, ptmp, QUEUE_POST_FREE, list) {
		pr_info("index %2d, 0x%p, type %d, vframetype 0x%x\n",
			p->index, p, p->type, p->vframe->type);
	}
	pr_info("post_ready_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_POST_READY, list) {
		print_di_buf(p, 2);
		print_di_buf(p->di_buf[0], 1);
		print_di_buf(p->di_buf[1], 1);
	}
	pr_info("display_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_DISPLAY, list) {
		print_di_buf(p, 2);
		print_di_buf(p->di_buf[0], 1);
		print_di_buf(p->di_buf[1], 1);
	}
	pr_info("recycle_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_RECYCLE, list) {
		pr_info(
"index %d, 0x%p, type %d, vframetype 0x%x pre_ref_count %d post_ref_count %d\n",
			p->index, p, p->type,
			p->vframe->type,
			p->pre_ref_count,
			p->post_ref_count);
		if (p->di_wr_linked_buf) {
			pr_info(
	"linked index %2d, 0x%p, type %d pre_ref_count %d post_ref_count %d\n",
				p->di_wr_linked_buf->index,
				p->di_wr_linked_buf,
				p->di_wr_linked_buf->type,
				p->di_wr_linked_buf->pre_ref_count,
				p->di_wr_linked_buf->post_ref_count);
		}
	}
	if (di_pre_stru.di_inp_buf) {
		pr_info("di_inp_buf:index %d, 0x%p, type %d\n",
			di_pre_stru.di_inp_buf->index,
			di_pre_stru.di_inp_buf,
			di_pre_stru.di_inp_buf->type);
	} else {
		pr_info("di_inp_buf: NULL\n");
	}
	if (di_pre_stru.di_wr_buf) {
		pr_info("di_wr_buf:index %d, 0x%p, type %d\n",
			di_pre_stru.di_wr_buf->index,
			di_pre_stru.di_wr_buf,
			di_pre_stru.di_wr_buf->type);
	} else {
		pr_info("di_wr_buf: NULL\n");
	}
	dump_di_pre_stru(&di_pre_stru);
	dump_di_post_stru(&di_post_stru);
	pr_info("vframe_in[]:");

	for (i = 0; i < MAX_IN_BUF_NUM; i++)
		pr_info("0x%p ", vframe_in[i]);

	pr_info("\n");
	pr_info("vf_peek()=>0x%p, video_peek_cnt = %d\n",
		vf_peek(VFM_NAME), video_peek_cnt);
	pr_info("reg_unreg_timerout = %lu\n", reg_unreg_timeout_cnt);
	dump_state_flag = 0;
}

static unsigned char check_di_buf(struct di_buf_s *di_buf, int reason)
{
	int error = 0;

	if (di_buf == NULL) {
		pr_dbg("%s: Error %d, di_buf is NULL\n", __func__, reason);
		return 1;
	}

	if (di_buf->type == VFRAME_TYPE_IN) {
		if (di_buf->vframe != &vframe_in_dup[di_buf->index])
			error = 1;
	} else if (di_buf->type == VFRAME_TYPE_LOCAL) {
		if (di_buf->vframe !=
		    &vframe_local[di_buf->index])
			error = 1;
	} else if (di_buf->type == VFRAME_TYPE_POST) {
		if (di_buf->vframe != &vframe_post[di_buf->index])
			error = 1;
	} else {
		error = 1;
	}

	if (error) {
		pr_dbg(
			"%s: Error %d, di_buf wrong\n", __func__,
			reason);
		if (recovery_flag == 0)
			recovery_log_reason = reason;
		recovery_flag++;
		dump_di_buf(di_buf);
		return 1;
	}

	return 0;
}
/*
 *  di pre process
 */
static void
config_di_mcinford_mif(struct DI_MC_MIF_s *di_mcinford_mif,
		       struct di_buf_s *di_buf)
{
	if (di_buf) {
		di_mcinford_mif->size_x = (di_buf->vframe->height + 2) / 4 - 1;
		di_mcinford_mif->size_y = 1;
		di_mcinford_mif->canvas_num = di_buf->mcinfo_canvas_idx;
	}
}
static void
config_di_pre_mc_mif(struct DI_MC_MIF_s *di_mcinfo_mif,
		     struct DI_MC_MIF_s *di_mcvec_mif,
		     struct di_buf_s *di_buf)
{
	unsigned int pre_size_w = 0, pre_size_h = 0;

	if (di_buf) {
		pre_size_w = di_buf->vframe->width;
		pre_size_h = (di_buf->vframe->height + 1) / 2;
		di_mcinfo_mif->size_x = (pre_size_h + 1) / 2 - 1;
		di_mcinfo_mif->size_y = 1;
		di_mcinfo_mif->canvas_num = di_buf->mcinfo_canvas_idx;

		di_mcvec_mif->size_x = (pre_size_w + 4) / 5 - 1;
		di_mcvec_mif->size_y = pre_size_h - 1;
		di_mcvec_mif->canvas_num = di_buf->mcvec_canvas_idx;
	}
}

static void config_di_cnt_mif(struct DI_SIM_MIF_s *di_cnt_mif,
	struct di_buf_s *di_buf)
{
	if (di_buf) {
		di_cnt_mif->start_x = 0;
		di_cnt_mif->end_x = di_buf->vframe->width - 1;
		di_cnt_mif->start_y = 0;
		di_cnt_mif->end_y = di_buf->vframe->height / 2 - 1;
		di_cnt_mif->canvas_num = di_buf->cnt_canvas_idx;
	}
}

static void
config_di_wr_mif(struct DI_SIM_MIF_s *di_nrwr_mif,
		 struct DI_SIM_MIF_s *di_mtnwr_mif,
		 struct di_buf_s *di_buf)
{
	vframe_t *vf = di_buf->vframe;
	di_nrwr_mif->canvas_num = di_buf->nr_canvas_idx;
	di_nrwr_mif->start_x = 0;
	di_nrwr_mif->end_x = vf->width - 1;
	di_nrwr_mif->start_y = 0;
	if (di_buf->vframe->bitdepth & BITDEPTH_Y10)
		di_nrwr_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:1;
	else
		di_nrwr_mif->bit_mode = 0;
	if (di_pre_stru.prog_proc_type == 0)
		di_nrwr_mif->end_y = vf->height / 2 - 1;
	else
		di_nrwr_mif->end_y = vf->height - 1;
	if (di_pre_stru.prog_proc_type == 0) {
		di_mtnwr_mif->start_x = 0;
		di_mtnwr_mif->end_x = vf->width - 1;
		di_mtnwr_mif->start_y = 0;
		di_mtnwr_mif->end_y = vf->height / 2 - 1;
		di_mtnwr_mif->canvas_num = di_buf->mtn_canvas_idx;
	}
}
static bool force_prog;
module_param_named(force_prog, force_prog, bool, 0664);
static void config_di_mif(struct DI_MIF_s *di_mif, struct di_buf_s *di_buf)
{
	if (di_buf == NULL)
		return;
	di_mif->canvas0_addr0 =
		di_buf->vframe->canvas0Addr & 0xff;
	di_mif->canvas0_addr1 =
		(di_buf->vframe->canvas0Addr >> 8) & 0xff;
	di_mif->canvas0_addr2 =
		(di_buf->vframe->canvas0Addr >> 16) & 0xff;

	di_mif->nocompress = (di_buf->vframe->type & VIDTYPE_COMPRESS)?0:1;

	if (di_buf->vframe->bitdepth & BITDEPTH_Y10) {
		if (di_buf->vframe->type & VIDTYPE_VIU_444)
			di_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:2;
		else if (di_buf->vframe->type & VIDTYPE_VIU_422)
			di_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:1;
	} else {
		di_mif->bit_mode = 0;
	}
	if (di_buf->vframe->type & VIDTYPE_VIU_422) {
		/* from vdin or local vframe */
		if ((!is_progressive(di_buf->vframe))
		    || (di_pre_stru.prog_proc_type)
		    ) {
			di_mif->video_mode = 0;
			di_mif->set_separate_en = 0;
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0;
			di_mif->luma_x_start0 = 0;
			di_mif->luma_x_end0 =
				di_buf->vframe->width - 1;
			di_mif->luma_y_start0 = 0;
			if (di_pre_stru.prog_proc_type)
				di_mif->luma_y_end0 =
					di_buf->vframe->height - 1;
			else
				di_mif->luma_y_end0 =
					di_buf->vframe->height / 2 - 1;
			di_mif->chroma_x_start0 = 0;
			di_mif->chroma_x_end0 = 0;
			di_mif->chroma_y_start0 = 0;
			di_mif->chroma_y_end0 = 0;
			di_mif->canvas0_addr0 =
				di_buf->vframe->canvas0Addr & 0xff;
			di_mif->canvas0_addr1 =
				(di_buf->vframe->canvas0Addr >> 8) & 0xff;
			di_mif->canvas0_addr2 =
				(di_buf->vframe->canvas0Addr >> 16) & 0xff;
		}
	} else {
		if (di_buf->vframe->type & VIDTYPE_VIU_444)
			di_mif->video_mode = 1;
		else
			di_mif->video_mode = 0;
		if (di_buf->vframe->type & VIDTYPE_VIU_NV21)
			di_mif->set_separate_en = 2;
		else
			di_mif->set_separate_en = 1;


		if (is_progressive(di_buf->vframe) &&
		    (di_pre_stru.prog_proc_type)) {
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0; /* top */
			di_mif->luma_x_start0 = 0;
			di_mif->luma_x_end0 =
				di_buf->vframe->width - 1;
			di_mif->luma_y_start0 = 0;
			di_mif->luma_y_end0 =
				di_buf->vframe->height - 1;
			di_mif->chroma_x_start0 = 0;
			di_mif->chroma_x_end0 =
				di_buf->vframe->width / 2 - 1;
			di_mif->chroma_y_start0 = 0;
			di_mif->chroma_y_end0 =
				di_buf->vframe->height / 2 - 1;
		} else {
			di_mif->src_prog = force_prog?1:0;
			di_mif->src_field_mode = 1;
			if (
				(di_buf->vframe->type & VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_TOP) {
				di_mif->output_field_num = 0; /* top */
				di_mif->luma_x_start0 = 0;
				di_mif->luma_x_end0 =
					di_buf->vframe->width - 1;
				di_mif->luma_y_start0 = 0;
				di_mif->luma_y_end0 =
					di_buf->vframe->height - 2;
				di_mif->chroma_x_start0 = 0;
				di_mif->chroma_x_end0 =
					di_buf->vframe->width / 2 - 1;
				di_mif->chroma_y_start0 = 0;
				di_mif->chroma_y_end0 =
					di_buf->vframe->height / 2
						- (di_mif->src_prog?1:2);
			} else {
				di_mif->output_field_num = 1;
				/* bottom */
				di_mif->luma_x_start0 = 0;
				di_mif->luma_x_end0 =
					di_buf->vframe->width - 1;
				di_mif->luma_y_start0 = 1;
				di_mif->luma_y_end0 =
					di_buf->vframe->height - 1;
				di_mif->chroma_x_start0 = 0;
				di_mif->chroma_x_end0 =
					di_buf->vframe->width / 2 - 1;
				di_mif->chroma_y_start0 =
					(di_mif->src_prog?0:1);
				di_mif->chroma_y_end0 =
					di_buf->vframe->height / 2 - 1;
			}
		}
	}
}

static void di_pre_size_change(unsigned short width,
	unsigned short height, unsigned short vf_type);

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static void pre_inp_canvas_config(struct vframe_s *vf);
#endif
static void pre_de_process(void)
{
	ulong irq_flag2 = 0;
	unsigned short pre_width = 0, pre_height = 0;
	unsigned char chan2_field_num = 1;
	int canvases_idex = di_pre_stru.field_count_for_cont % 2;
	unsigned short cur_inp_field_type = VIDTYPE_TYPEMASK;
	unsigned short int_mask = 0x7f;

	di_pre_stru.pre_de_process_flag = 1;
	di_pre_stru.pre_de_busy_timer_count = 0;
	#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	pre_inp_canvas_config(di_pre_stru.di_inp_buf->vframe);
	#endif

	config_di_mif(&di_pre_stru.di_inp_mif, di_pre_stru.di_inp_buf);
	/* pr_dbg("set_separate_en=%d vframe->type %d\n",
	 * di_pre_stru.di_inp_mif.set_separate_en,
	 * di_pre_stru.di_inp_buf->vframe->type);
	 */
#ifdef DI_USE_FIXED_CANVAS_IDX
	if (di_pre_stru.di_mem_buf_dup_p != NULL &&
		di_pre_stru.di_mem_buf_dup_p != di_pre_stru.di_inp_buf) {
		config_canvas_idx(di_pre_stru.di_mem_buf_dup_p,
			di_pre_idx[canvases_idex][0], -1);
		config_cnt_canvas_idx(di_pre_stru.di_mem_buf_dup_p,
			di_pre_idx[canvases_idex][1]);
	} else {
		config_cnt_canvas_idx(di_pre_stru.di_wr_buf,
				di_pre_idx[canvases_idex][1]);
		config_di_cnt_mif(&di_pre_stru.di_contp2rd_mif,
			di_pre_stru.di_wr_buf);

	}
	if (di_pre_stru.di_chan2_buf_dup_p != NULL) {
		config_canvas_idx(di_pre_stru.di_chan2_buf_dup_p,
			di_pre_idx[canvases_idex][2], -1);
		config_cnt_canvas_idx(di_pre_stru.di_chan2_buf_dup_p,
			di_pre_idx[canvases_idex][3]);
	} else {
		config_cnt_canvas_idx(di_pre_stru.di_wr_buf,
			di_pre_idx[canvases_idex][3]);
	}
	config_canvas_idx(di_pre_stru.di_wr_buf,
		di_pre_idx[canvases_idex][4], di_pre_idx[canvases_idex][5]);
	config_cnt_canvas_idx(di_pre_stru.di_wr_buf,
		di_pre_idx[canvases_idex][6]);
	if (mcpre_en) {
		if (di_pre_stru.di_chan2_buf_dup_p != NULL)
			config_mcinfo_canvas_idx(di_pre_stru.di_chan2_buf_dup_p,
				di_pre_idx[canvases_idex][7]);
		else
			config_mcinfo_canvas_idx(di_pre_stru.di_wr_buf,
				di_pre_idx[canvases_idex][7]);

		config_mcinfo_canvas_idx(di_pre_stru.di_wr_buf,
			di_pre_idx[canvases_idex][8]);
		config_mcvec_canvas_idx(di_pre_stru.di_wr_buf,
			di_pre_idx[canvases_idex][9]);
	}
#endif
	config_di_mif(&di_pre_stru.di_mem_mif, di_pre_stru.di_mem_buf_dup_p);
	if (di_pre_stru.di_chan2_buf_dup_p == NULL) {
		config_di_mif(&di_pre_stru.di_chan2_mif,
			di_pre_stru.di_inp_buf);
	} else
		config_di_mif(&di_pre_stru.di_chan2_mif,
			di_pre_stru.di_chan2_buf_dup_p);
	config_di_wr_mif(&di_pre_stru.di_nrwr_mif, &di_pre_stru.di_mtnwr_mif,
		di_pre_stru.di_wr_buf);

	if (di_pre_stru.di_chan2_buf_dup_p)
		config_di_cnt_mif(&di_pre_stru.di_contprd_mif,
			di_pre_stru.di_chan2_buf_dup_p);
	else
		config_di_cnt_mif(&di_pre_stru.di_contprd_mif,
			di_pre_stru.di_wr_buf);

	config_di_cnt_mif(&di_pre_stru.di_contwr_mif, di_pre_stru.di_wr_buf);
	if (mcpre_en) {
		if (di_pre_stru.di_chan2_buf_dup_p)
			config_di_mcinford_mif(&di_pre_stru.di_mcinford_mif,
				di_pre_stru.di_chan2_buf_dup_p);
		else
			config_di_mcinford_mif(&di_pre_stru.di_mcinford_mif,
				di_pre_stru.di_wr_buf);

		config_di_pre_mc_mif(&di_pre_stru.di_mcinfowr_mif,
			&di_pre_stru.di_mcvecwr_mif, di_pre_stru.di_wr_buf);
	}

	if ((di_pre_stru.di_chan2_buf_dup_p) &&
	    ((di_pre_stru.di_chan2_buf_dup_p->vframe->type & VIDTYPE_TYPEMASK)
	     == VIDTYPE_INTERLACE_TOP))
		chan2_field_num = 0;
	pre_width = di_pre_stru.di_nrwr_mif.end_x + 1;
	pre_height = di_pre_stru.di_nrwr_mif.end_y + 1;
	if (di_pre_stru.input_size_change_flag) {
		cur_inp_field_type =
		(di_pre_stru.di_inp_buf->vframe->type & VIDTYPE_TYPEMASK);
		cur_inp_field_type =
	di_pre_stru.cur_prog_flag ? VIDTYPE_PROGRESSIVE : cur_inp_field_type;
		di_pre_size_change(pre_width, pre_height,
				cur_inp_field_type);
		di_pre_stru.input_size_change_flag = false;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		if (de_devp->nrds_enable) {
			nr_ds_mif_config();
			nr_ds_hw_ctrl(true);
			int_mask = 0x3f;
		} else {
			nr_ds_hw_ctrl(false);
		}
	}

	/* set interrupt mask for pre module.
	 * we need to only leave one mask open
	 * to prevent multiple entry for de_irq
	*/



	enable_di_pre_aml(&di_pre_stru.di_inp_mif,
			&di_pre_stru.di_mem_mif,
			&di_pre_stru.di_chan2_mif,
			&di_pre_stru.di_nrwr_mif,
			&di_pre_stru.di_mtnwr_mif,
			&di_pre_stru.di_contp2rd_mif,
			&di_pre_stru.di_contprd_mif,
			&di_pre_stru.di_contwr_mif,
			di_pre_stru.madi_enable,
			chan2_field_num,
			di_pre_stru.vdin2nr);

	enable_afbc_input(di_pre_stru.di_inp_buf->vframe);

	if (mcpre_en) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
			enable_mc_di_pre_g12(
				&di_pre_stru.di_mcinford_mif,
				&di_pre_stru.di_mcinfowr_mif,
				&di_pre_stru.di_mcvecwr_mif,
				di_pre_stru.mcdi_enable);
		else
			enable_mc_di_pre(
				&di_pre_stru.di_mcinford_mif,
				&di_pre_stru.di_mcinfowr_mif,
				&di_pre_stru.di_mcvecwr_mif,
				di_pre_stru.mcdi_enable);
	}

	di_pre_stru.field_count_for_cont++;
	di_txl_patch_prog(di_pre_stru.cur_prog_flag,
		di_pre_stru.field_count_for_cont, mcpre_en);

#ifdef SUPPORT_MPEG_TO_VDIN
	if (mpeg2vdin_flag) {
		struct vdin_arg_s vdin_arg;
		struct vdin_v4l2_ops_s *vdin_ops = get_vdin_v4l2_ops();

		vdin_arg.cmd = VDIN_CMD_FORCE_GO_FIELD;
		if (vdin_ops->tvin_vdin_func)
			vdin_ops->tvin_vdin_func(0, &vdin_arg);
	}
#endif
	/* must make sure follow part issue without interrupts,
	 * otherwise may cause watch dog reboot
	 */
	di_lock_irqfiq_save(irq_flag2);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		pre_frame_reset_g12(di_pre_stru.madi_enable,
			di_pre_stru.mcdi_enable);
	else
		pre_frame_reset();

	/* enable mc pre mif*/
	enable_di_pre_mif(true, mcpre_en);
	di_unlock_irqfiq_restore(irq_flag2);
	/*reinit pre busy flag*/
	di_pre_stru.pre_de_busy_timer_count = 0;
	di_pre_stru.pre_de_busy = 1;
	#ifdef SUPPORT_MPEG_TO_VDIN
	if (mpeg2vdin_flag)
		RDMA_WR_BITS(DI_PRE_CTRL, 1, 13, 1);
	#endif
	di_pre_stru.irq_time[0] = cur_to_msecs();
	di_pre_stru.irq_time[1] = cur_to_msecs();
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (di_pre_rdma_enable & 0x2)
		rdma_config(de_devp->rdma_handle, RDMA_TRIGGER_MANUAL);
	else if (di_pre_rdma_enable & 1)
		rdma_config(de_devp->rdma_handle, RDMA_DEINT_IRQ);
#endif
	di_pre_stru.pre_de_process_flag = 0;
}

static void pre_de_done_buf_clear(void)
{
	struct di_buf_s *wr_buf = NULL;

	if (di_pre_stru.di_wr_buf) {
		wr_buf = di_pre_stru.di_wr_buf;
		if ((di_pre_stru.prog_proc_type == 2) &&
		    wr_buf->di_wr_linked_buf) {
			wr_buf->di_wr_linked_buf->pre_ref_count = 0;
			wr_buf->di_wr_linked_buf->post_ref_count = 0;
			queue_in(wr_buf->di_wr_linked_buf, QUEUE_RECYCLE);
			wr_buf->di_wr_linked_buf = NULL;
		}
		wr_buf->pre_ref_count = 0;
		wr_buf->post_ref_count = 0;
		queue_in(wr_buf, QUEUE_RECYCLE);
		di_pre_stru.di_wr_buf = NULL;
	}
	if (di_pre_stru.di_inp_buf) {
		if (di_pre_stru.di_mem_buf_dup_p == di_pre_stru.di_inp_buf)
			di_pre_stru.di_mem_buf_dup_p = NULL;

		queue_in(di_pre_stru.di_inp_buf, QUEUE_RECYCLE);
		di_pre_stru.di_inp_buf = NULL;
	}
}

static void top_bot_config(struct di_buf_s *di_buf)
{
	vframe_t *vframe = di_buf->vframe;

	if (((invert_top_bot & 0x1) != 0) && (!is_progressive(vframe))) {
		if (di_buf->invert_top_bot_flag == 0) {
			if (
				(vframe->type & VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_TOP) {
				vframe->type &= (~VIDTYPE_TYPEMASK);
				vframe->type |= VIDTYPE_INTERLACE_BOTTOM;
			} else {
				vframe->type &= (~VIDTYPE_TYPEMASK);
				vframe->type |= VIDTYPE_INTERLACE_TOP;
			}
			di_buf->invert_top_bot_flag = 1;
		}
	}
}

static bool pulldown_enable = true;

static bool combing_fix_en = true;
module_param_named(combing_fix_en, combing_fix_en, bool, 0664);

static int cur_lev = 2;
module_param_named(combing_cur_lev, cur_lev, int, 0444);

static void pre_de_done_buf_config(void)
{
	ulong irq_flag2 = 0;
	struct di_buf_s *post_wr_buf = NULL;
	unsigned int glb_frame_mot_num = 0;
	unsigned int glb_field_mot_num = 0;

	if (di_pre_stru.di_wr_buf) {
		if (di_pre_stru.pre_throw_flag > 0) {
			di_pre_stru.di_wr_buf->throw_flag = 1;
			di_pre_stru.pre_throw_flag--;
		} else {
			di_pre_stru.di_wr_buf->throw_flag = 0;
		}
#ifdef DET3D
		if (di_pre_stru.di_wr_buf->vframe->trans_fmt == 0 &&
				di_pre_stru.det3d_trans_fmt != 0 && det3d_en) {
			di_pre_stru.di_wr_buf->vframe->trans_fmt =
			di_pre_stru.det3d_trans_fmt;
			set3d_view(di_pre_stru.det3d_trans_fmt,
				di_pre_stru.di_wr_buf->vframe);
		}
#endif
		if (!di_pre_rdma_enable)
			di_pre_stru.di_post_wr_buf = di_pre_stru.di_wr_buf;
		post_wr_buf = di_pre_stru.di_post_wr_buf;

		if (post_wr_buf && !di_pre_stru.cur_prog_flag) {
			read_pulldown_info(&glb_frame_mot_num,
				&glb_field_mot_num);
			if (pulldown_enable)
				pulldown_detection(&post_wr_buf->pd_config,
					di_pre_stru.mtn_status, overturn);
			if (combing_fix_en)
				cur_lev = adaptive_combing_fixing(
				di_pre_stru.mtn_status,
					glb_field_mot_num,
					glb_frame_mot_num,
					di_force_bit_mode);
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
				adaptive_cue_adjust(glb_frame_mot_num,
					glb_field_mot_num);
			pulldown_info_clear_g12a();
		}

		if (di_pre_stru.cur_prog_flag) {
			if (di_pre_stru.prog_proc_type == 0) {
				/* di_mem_buf_dup->vfrme
				 * is either local vframe,
				 * or bot field of vframe from in_list
				 */
				di_pre_stru.di_mem_buf_dup_p->pre_ref_count = 0;
				di_pre_stru.di_mem_buf_dup_p
					= di_pre_stru.di_chan2_buf_dup_p;
				di_pre_stru.di_chan2_buf_dup_p
					= di_pre_stru.di_wr_buf;
#ifdef DI_BUFFER_DEBUG
				di_print("%s:set di_mem to di_chan2,",
					__func__);
				di_print("%s:set di_chan2 to di_wr_buf\n",
					__func__);
#endif
			} else {
				di_pre_stru.di_mem_buf_dup_p->pre_ref_count = 0;
				/*recycle the progress throw buffer*/
				if (di_pre_stru.di_wr_buf->throw_flag) {
					di_pre_stru.di_wr_buf->
						pre_ref_count = 0;
					di_pre_stru.di_mem_buf_dup_p = NULL;
#ifdef DI_BUFFER_DEBUG
				di_print(
				"%s set throw %s[%d] pre_ref_count to 0.\n",
				__func__,
				vframe_type_name[di_pre_stru.di_wr_buf->type],
				di_pre_stru.di_wr_buf->index);
#endif
				} else {
					di_pre_stru.di_mem_buf_dup_p
						= di_pre_stru.di_wr_buf;
				}
#ifdef DI_BUFFER_DEBUG
				di_print(
					"%s: set di_mem_buf_dup_p to di_wr_buf\n",
					__func__);
#endif
			}

			di_pre_stru.di_wr_buf->seq
				= di_pre_stru.pre_ready_seq++;
			di_pre_stru.di_wr_buf->post_ref_count = 0;
			di_pre_stru.di_wr_buf->left_right
				= di_pre_stru.left_right;
			if (di_pre_stru.source_change_flag) {
				di_pre_stru.di_wr_buf->new_format_flag = 1;
				di_pre_stru.source_change_flag = 0;
			} else {
				di_pre_stru.di_wr_buf->new_format_flag = 0;
			}
			if (bypass_state == 1) {
				di_pre_stru.di_wr_buf->new_format_flag = 1;
				bypass_state = 0;
#ifdef DI_BUFFER_DEBUG
				di_print(
					"%s:bypass_state->0, is_bypass() %d\n",
					__func__, is_bypass(NULL));
				di_print(
					"trick_mode %d bypass_all %d\n",
					trick_mode, bypass_all);
#endif
			}
			if (di_pre_stru.di_post_wr_buf)
				queue_in(di_pre_stru.di_post_wr_buf,
					QUEUE_PRE_READY);
#ifdef DI_BUFFER_DEBUG
			di_print(
				"%s: %s[%d] => pre_ready_list\n", __func__,
				vframe_type_name[di_pre_stru.di_wr_buf->type],
				di_pre_stru.di_wr_buf->index);
#endif
			if (di_pre_stru.di_wr_buf) {
				if (di_pre_rdma_enable)
					di_pre_stru.di_post_wr_buf =
				di_pre_stru.di_wr_buf;
				else
					di_pre_stru.di_post_wr_buf = NULL;
				di_pre_stru.di_wr_buf = NULL;
			}
		} else {
			di_pre_stru.di_mem_buf_dup_p->pre_ref_count = 0;
			di_pre_stru.di_mem_buf_dup_p = NULL;
			if (di_pre_stru.di_chan2_buf_dup_p) {
				di_pre_stru.di_mem_buf_dup_p =
					di_pre_stru.di_chan2_buf_dup_p;
#ifdef DI_BUFFER_DEBUG
				di_print(
				"%s: di_mem_buf_dup_p = di_chan2_buf_dup_p\n",
				__func__);
#endif
			}
			di_pre_stru.di_chan2_buf_dup_p = di_pre_stru.di_wr_buf;

			if (di_pre_stru.source_change_flag) {
				/* add dummy buf, will not be displayed */
				add_dummy_vframe_type_pre(post_wr_buf);
			}
			di_pre_stru.di_wr_buf->seq =
				di_pre_stru.pre_ready_seq++;
			di_pre_stru.di_wr_buf->left_right =
				di_pre_stru.left_right;
			di_pre_stru.di_wr_buf->post_ref_count = 0;
			if (di_pre_stru.source_change_flag) {
				di_pre_stru.di_wr_buf->new_format_flag = 1;
				di_pre_stru.source_change_flag = 0;
			} else {
				di_pre_stru.di_wr_buf->new_format_flag = 0;
			}
			if (bypass_state == 1) {
				di_pre_stru.di_wr_buf->new_format_flag = 1;
				bypass_state = 0;

#ifdef DI_BUFFER_DEBUG
				di_print(
					"%s:bypass_state->0, is_bypass() %d\n",
					__func__, is_bypass(NULL));
				di_print(
					"trick_mode %d bypass_all %d\n",
					trick_mode, bypass_all);
#endif
			}

			if (di_pre_stru.di_post_wr_buf)
				queue_in(di_pre_stru.di_post_wr_buf,
					QUEUE_PRE_READY);

			di_print("%s: %s[%d] => pre_ready_list\n", __func__,
				vframe_type_name[di_pre_stru.di_wr_buf->type],
				di_pre_stru.di_wr_buf->index);

			if (di_pre_stru.di_wr_buf) {
				if (di_pre_rdma_enable)
					di_pre_stru.di_post_wr_buf =
				di_pre_stru.di_wr_buf;
				else
					di_pre_stru.di_post_wr_buf = NULL;
				di_pre_stru.di_wr_buf = NULL;
			}
		}
	}
	if (di_pre_stru.di_post_inp_buf && di_pre_rdma_enable) {
#ifdef DI_BUFFER_DEBUG
		di_print("%s: %s[%d] => recycle_list\n", __func__,
			vframe_type_name[di_pre_stru.di_post_inp_buf->type],
			di_pre_stru.di_post_inp_buf->index);
#endif
		di_lock_irqfiq_save(irq_flag2);
		queue_in(di_pre_stru.di_post_inp_buf, QUEUE_RECYCLE);
		di_pre_stru.di_post_inp_buf = NULL;
		di_unlock_irqfiq_restore(irq_flag2);
	}
	if (di_pre_stru.di_inp_buf) {
		if (!di_pre_rdma_enable) {
#ifdef DI_BUFFER_DEBUG
			di_print("%s: %s[%d] => recycle_list\n", __func__,
			vframe_type_name[di_pre_stru.di_inp_buf->type],
			di_pre_stru.di_inp_buf->index);
#endif
			di_lock_irqfiq_save(irq_flag2);
			queue_in(di_pre_stru.di_inp_buf, QUEUE_RECYCLE);
			di_pre_stru.di_inp_buf = NULL;
			di_unlock_irqfiq_restore(irq_flag2);
		} else {
			di_pre_stru.di_post_inp_buf = di_pre_stru.di_inp_buf;
			di_pre_stru.di_inp_buf = NULL;
		}
	}
}

static void recycle_vframe_type_pre(struct di_buf_s *di_buf)
{
	ulong irq_flag2 = 0;

	di_lock_irqfiq_save(irq_flag2);

	queue_in(di_buf, QUEUE_RECYCLE);

	di_unlock_irqfiq_restore(irq_flag2);
}
/*
 * add dummy buffer to pre ready queue
 */
static void add_dummy_vframe_type_pre(struct di_buf_s *src_buf)
{
	struct di_buf_s *di_buf_tmp = NULL;

	if (!queue_empty(QUEUE_LOCAL_FREE)) {
		di_buf_tmp = get_di_buf_head(QUEUE_LOCAL_FREE);
		if (di_buf_tmp) {
			queue_out(di_buf_tmp);
			di_buf_tmp->pre_ref_count = 0;
			di_buf_tmp->post_ref_count = 0;
			di_buf_tmp->post_proc_flag = 3;
			di_buf_tmp->new_format_flag = 0;
			if (!IS_ERR_OR_NULL(src_buf))
				memcpy(di_buf_tmp->vframe, src_buf->vframe,
					sizeof(vframe_t));
			queue_in(di_buf_tmp, QUEUE_PRE_READY);
			#ifdef DI_BUFFER_DEBUG
			di_print("%s: dummy %s[%d] => pre_ready_list\n",
				__func__,
				vframe_type_name[di_buf_tmp->type],
				di_buf_tmp->index);
			#endif
		}
	}
}
/*
 * it depend on local buffer queue type is 2
 */
static int peek_free_linked_buf(void)
{
	struct di_buf_s *p = NULL;
	int itmp, p_index = -2;

	if (list_count(QUEUE_LOCAL_FREE) < 2)
		return -1;

	queue_for_each_entry(p, ptmp, QUEUE_LOCAL_FREE, list) {
		if (abs(p->index - p_index) == 1)
			return min(p->index, p_index);
		p_index = p->index;
	}
	return -1;
}
/*
 * it depend on local buffer queue type is 2
 */
static struct di_buf_s *get_free_linked_buf(int idx)
{
	struct di_buf_s *di_buf = NULL, *di_buf_linked = NULL;
	int pool_idx = 0, di_buf_idx = 0;

	queue_t *q = &(queue[QUEUE_LOCAL_FREE]);

	if (list_count(QUEUE_LOCAL_FREE) < 2)
		return NULL;
	if (q->pool[idx] != 0 && q->pool[idx + 1] != 0) {
		pool_idx = ((q->pool[idx] >> 8) & 0xff) - 1;
		di_buf_idx = q->pool[idx] & 0xff;
		if (pool_idx < VFRAME_TYPE_NUM) {
			if (di_buf_idx < di_buf_pool[pool_idx].size) {
				di_buf = &(di_buf_pool[pool_idx].
					di_buf_ptr[di_buf_idx]);
				queue_out(di_buf);
			}
		}
		pool_idx = ((q->pool[idx + 1] >> 8) & 0xff) - 1;
		di_buf_idx = q->pool[idx + 1] & 0xff;
		if (pool_idx < VFRAME_TYPE_NUM) {
			if (di_buf_idx < di_buf_pool[pool_idx].size) {
				di_buf_linked =	&(di_buf_pool[pool_idx].
					di_buf_ptr[di_buf_idx]);
				queue_out(di_buf_linked);
			}
		}
		if (IS_ERR_OR_NULL(di_buf))
			return NULL;
		di_buf->di_wr_linked_buf = di_buf_linked;
	}
	return di_buf;
}

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static void pre_inp_canvas_config(struct vframe_s *vf)
{
	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(di_inp_idx[0],
			&vf->canvas0_config[0]);
		canvas_config_config(di_inp_idx[1],
			&vf->canvas0_config[1]);
		vf->canvas0Addr = (di_inp_idx[1]<<8)|(di_inp_idx[0]);
		if (vf->plane_num == 2) {
			vf->canvas0Addr |= (di_inp_idx[1]<<16);
		} else if (vf->plane_num == 3) {
			canvas_config_config(di_inp_idx[2],
			&vf->canvas0_config[2]);
			vf->canvas0Addr |= (di_inp_idx[2]<<16);
		}
		vf->canvas1Addr = vf->canvas0Addr;
	}
}
#endif
static int pps_dstw;
module_param_named(pps_dstw, pps_dstw, int, 0644);
static int pps_dsth;
module_param_named(pps_dsth, pps_dsth, int, 0644);
static bool pps_en;
module_param_named(pps_en, pps_en, bool, 0644);
static unsigned int pps_position = 1;
module_param_named(pps_position, pps_position, uint, 0644);
static unsigned int pre_enable_mask = 3;/*bit0:ma bit1:mc*/
module_param_named(pre_enable_mask, pre_enable_mask, uint, 0644);

static unsigned char pre_de_buf_config(void)
{
	struct di_buf_s *di_buf = NULL;
	vframe_t *vframe;
	int i, di_linked_buf_idx = -1;
	unsigned char change_type = 0;
	bool bit10_pack_patch = false;
	unsigned int width_roundup = 2;

	if (di_blocking || !atomic_read(&de_devp->mem_flag))
		return 0;
	if ((list_count(QUEUE_IN_FREE) < 2 && (!di_pre_stru.di_inp_buf_next)) ||
	    (queue_empty(QUEUE_LOCAL_FREE)))
		return 0;

	if (is_bypass(NULL)) {
		/* some provider has problem if receiver
		 * get all buffers of provider
		 */
		int in_buf_num = 0;
		cur_lev = 0;
		for (i = 0; i < MAX_IN_BUF_NUM; i++)
			if (vframe_in[i] != NULL)
				in_buf_num++;
		if (in_buf_num > BYPASS_GET_MAX_BUF_NUM
#ifdef DET3D
			&& (di_pre_stru.vframe_interleave_flag == 0)
#endif
		    )
			return 0;

		di_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	} else if (di_pre_stru.prog_proc_type == 2) {
		di_linked_buf_idx = peek_free_linked_buf();
		if (di_linked_buf_idx == -1 &&
			!IS_ERR_OR_NULL(di_post_stru.keep_buf)) {
			recycle_keep_buffer();
			pr_info("%s: recycle keep buffer for peek null linked buf\n",
				__func__);
			return 0;
		}
	}
	if (di_pre_stru.di_inp_buf_next) {
		di_pre_stru.di_inp_buf = di_pre_stru.di_inp_buf_next;
		di_pre_stru.di_inp_buf_next = NULL;
#ifdef DI_BUFFER_DEBUG
		di_print("%s: di_inp_buf_next %s[%d] => di_inp_buf\n",
			__func__,
			vframe_type_name[di_pre_stru.di_inp_buf->type],
			di_pre_stru.di_inp_buf->index);
#endif
		if (di_pre_stru.di_mem_buf_dup_p == NULL) {/* use n */
			di_pre_stru.di_mem_buf_dup_p = di_pre_stru.di_inp_buf;
#ifdef DI_BUFFER_DEBUG
			di_print(
				"%s: set di_mem_buf_dup_p to be di_inp_buf\n",
				__func__);
#endif
		}
	} else {
		/* check if source change */
		vframe = vf_peek(VFM_NAME);

		if (vframe && is_from_vdin(vframe)) {
#ifdef RUN_DI_PROCESS_IN_IRQ
			di_pre_stru.vdin2nr = is_input2pre();
#endif
		}

		vframe = vf_get(VFM_NAME);

		if (vframe == NULL)
			return 0;

		if (vframe->type & VIDTYPE_COMPRESS) {
			vframe->width = vframe->compWidth;
			vframe->height = vframe->compHeight;
		}
		di_print("DI: get %dth vf[0x%p] from frontend %u ms.\n",
			di_pre_stru.in_seq, vframe,
jiffies_to_msecs(jiffies_64 - vframe->ready_jiffies64));
		vframe->prog_proc_config = (prog_proc_config&0x20) >> 5;

		if (vframe->width > 10000 || vframe->height > 10000 ||
		    hold_video || di_pre_stru.bad_frame_throw_count > 0) {
			if (vframe->width > 10000 || vframe->height > 10000)
				di_pre_stru.bad_frame_throw_count = 10;
			di_pre_stru.bad_frame_throw_count--;
			vf_put(vframe, VFM_NAME);
			vf_notify_provider(
				VFM_NAME, VFRAME_EVENT_RECEIVER_PUT, NULL);
			return 0;
		}
		bit10_pack_patch =  (is_meson_gxtvbb_cpu() ||
							is_meson_gxl_cpu() ||
							is_meson_gxm_cpu());
		width_roundup = bit10_pack_patch ? 16 : width_roundup;
		if (di_force_bit_mode == 10)
			force_width = roundup(vframe->width, width_roundup);
		else
			force_width = 0;
		di_pre_stru.source_trans_fmt = vframe->trans_fmt;
		di_pre_stru.left_right = di_pre_stru.left_right ? 0 : 1;
		di_pre_stru.invert_flag =
			(vframe->type &  TB_DETECT_MASK) ? true : false;
		vframe->type &= ~TB_DETECT_MASK;

		if ((((invert_top_bot & 0x2) != 0) ||
				di_pre_stru.invert_flag) &&
		    (!is_progressive(vframe))) {
			if (
				(vframe->type & VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_TOP) {
				vframe->type &= (~VIDTYPE_TYPEMASK);
				vframe->type |= VIDTYPE_INTERLACE_BOTTOM;
			} else {
				vframe->type &= (~VIDTYPE_TYPEMASK);
				vframe->type |= VIDTYPE_INTERLACE_TOP;
			}
		}
		di_pre_stru.width_bk = vframe->width;
		if (force_width)
			vframe->width = force_width;
		if (force_height)
			vframe->height = force_height;

		/* backup frame motion info */
		vframe->combing_cur_lev = cur_lev;

		di_print("%s: vf_get => 0x%p\n", __func__, vframe);

		di_buf = get_di_buf_head(QUEUE_IN_FREE);

		if (check_di_buf(di_buf, 10))
			return 0;

		if (di_log_flag & DI_LOG_VFRAME)
			dump_vframe(vframe);

#ifdef SUPPORT_MPEG_TO_VDIN
		if (
			(!is_from_vdin(vframe)) &&
			(vframe->sig_fmt == TVIN_SIG_FMT_NULL) &&
			mpeg2vdin_flag) {
			struct vdin_arg_s vdin_arg;
			struct vdin_v4l2_ops_s *vdin_ops = get_vdin_v4l2_ops();

			vdin_arg.cmd = VDIN_CMD_GET_HISTGRAM;
			vdin_arg.private = (unsigned int)vframe;
			if (vdin_ops->tvin_vdin_func)
				vdin_ops->tvin_vdin_func(0, &vdin_arg);
		}
#endif
		memcpy(di_buf->vframe, vframe, sizeof(vframe_t));

		di_buf->vframe->private_data = di_buf;
		vframe_in[di_buf->index] = vframe;
		di_buf->seq = di_pre_stru.in_seq;
		di_pre_stru.in_seq++;
		queue_out(di_buf);
		change_type = is_source_change(vframe);
		/* source change, when i mix p,force p as i*/
		if (change_type == 1 || (change_type == 2 &&
					 di_pre_stru.cur_prog_flag == 1)) {
			if (di_pre_stru.di_mem_buf_dup_p) {
				/*avoid only 2 i field then p field*/
				if (
					(di_pre_stru.cur_prog_flag == 0) &&
					use_2_interlace_buff)
					di_pre_stru.di_mem_buf_dup_p->
					post_proc_flag = -1;
				di_pre_stru.di_mem_buf_dup_p->pre_ref_count = 0;
				di_pre_stru.di_mem_buf_dup_p = NULL;
			}
			if (di_pre_stru.di_chan2_buf_dup_p) {
				/*avoid only 1 i field then p field*/
				if (
					(di_pre_stru.cur_prog_flag == 0) &&
					use_2_interlace_buff)
					di_pre_stru.di_chan2_buf_dup_p->
					post_proc_flag = -1;
				di_pre_stru.di_chan2_buf_dup_p->pre_ref_count =
					0;
				di_pre_stru.di_chan2_buf_dup_p = NULL;
			}
			#if 0
			/* channel change will occur between atv and dtv,
			 * that need mirror
			 */
			if (!IS_ERR_OR_NULL(di_post_stru.keep_buf)) {
				if (di_post_stru.keep_buf->vframe
					->source_type !=
					di_buf->vframe->source_type) {
					recycle_keep_buffer();
					pr_info("%s: source type changed recycle buffer!!!\n",
						__func__);
				}
			}
			#endif
			pr_info(
			"%s:%ums %dth source change: 0x%x/%d/%d/%d=>0x%x/%d/%d/%d\n",
				__func__,
				jiffies_to_msecs(jiffies_64),
				di_pre_stru.in_seq,
				di_pre_stru.cur_inp_type,
				di_pre_stru.cur_width,
				di_pre_stru.cur_height,
				di_pre_stru.cur_source_type,
				di_buf->vframe->type,
				di_buf->vframe->width,
				di_buf->vframe->height,
				di_buf->vframe->source_type);
			if (di_buf->type & VIDTYPE_COMPRESS) {
				di_pre_stru.cur_width =
					di_buf->vframe->compWidth;
				di_pre_stru.cur_height =
					di_buf->vframe->compHeight;
			} else {
				di_pre_stru.cur_width = di_buf->vframe->width;
				di_pre_stru.cur_height = di_buf->vframe->height;
			}
			di_pre_stru.cur_prog_flag =
				is_progressive(di_buf->vframe);
			if (di_pre_stru.cur_prog_flag) {
				if ((use_2_interlace_buff) &&
				    !(prog_proc_config & 0x10))
					di_pre_stru.prog_proc_type = 2;
				else
					di_pre_stru.prog_proc_type
						= prog_proc_config & 0x10;
			} else
				di_pre_stru.prog_proc_type = 0;
			di_pre_stru.cur_inp_type = di_buf->vframe->type;
			di_pre_stru.cur_source_type =
				di_buf->vframe->source_type;
			di_pre_stru.cur_sig_fmt = di_buf->vframe->sig_fmt;
			di_pre_stru.orientation = di_buf->vframe->video_angle;
			di_pre_stru.source_change_flag = 1;
			di_pre_stru.input_size_change_flag = true;
#ifdef SUPPORT_MPEG_TO_VDIN
			if ((!is_from_vdin(vframe)) &&
			    (vframe->sig_fmt == TVIN_SIG_FMT_NULL) &&
			    (mpeg2vdin_en)) {
				struct vdin_arg_s vdin_arg;
				struct vdin_v4l2_ops_s *vdin_ops =
					get_vdin_v4l2_ops();
				vdin_arg.cmd = VDIN_CMD_MPEGIN_START;
				vdin_arg.h_active = di_pre_stru.cur_width;
				vdin_arg.v_active = di_pre_stru.cur_height;
				if (vdin_ops->tvin_vdin_func)
					vdin_ops->tvin_vdin_func(0, &vdin_arg);
				mpeg2vdin_flag = 1;
			}
#endif
			di_pre_stru.field_count_for_cont = 0;
		} else if (di_pre_stru.cur_prog_flag == 0) {
			/* check if top/bot interleaved */
			if (change_type == 2)
				/* source is i interleaves p fields */
				di_pre_stru.force_interlace = true;
			if ((di_pre_stru.cur_inp_type &
			VIDTYPE_TYPEMASK) == (di_buf->vframe->type &
			VIDTYPE_TYPEMASK)) {
				if ((di_buf->vframe->type &
				VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_TOP)
					same_field_top_count++;
				else
					same_field_bot_count++;
			}
			di_pre_stru.cur_inp_type = di_buf->vframe->type;
		} else {
			di_pre_stru.cur_inp_type = di_buf->vframe->type;
		}

		if (is_bypass(di_buf->vframe)) {
			/* bypass progressive */
			di_buf->seq = di_pre_stru.pre_ready_seq++;
			di_buf->post_ref_count = 0;
			cur_lev = 0;
			if (di_pre_stru.source_change_flag) {
				di_buf->new_format_flag = 1;
				di_pre_stru.source_change_flag = 0;
			} else {
				di_buf->new_format_flag = 0;
			}

			if (bypass_state == 0) {
				if (di_pre_stru.di_mem_buf_dup_p) {
					di_pre_stru.di_mem_buf_dup_p->
					pre_ref_count = 0;
					di_pre_stru.di_mem_buf_dup_p = NULL;
				}
				if (di_pre_stru.di_chan2_buf_dup_p) {
					di_pre_stru.di_chan2_buf_dup_p->
					pre_ref_count = 0;
					di_pre_stru.di_chan2_buf_dup_p = NULL;
				}

				if (di_pre_stru.di_wr_buf) {
					di_pre_stru.di_wr_buf->pre_ref_count =
						0;
					di_pre_stru.di_wr_buf->post_ref_count =
						0;
					recycle_vframe_type_pre(
						di_pre_stru.di_wr_buf);
#ifdef DI_BUFFER_DEBUG
					di_print(
						"%s: %s[%d] => recycle_list\n",
						__func__,
						vframe_type_name[di_pre_stru.
							di_wr_buf->type],
						di_pre_stru.di_wr_buf->index);
#endif
					di_pre_stru.di_wr_buf = NULL;
				}

				di_buf->new_format_flag = 1;
				bypass_state = 1;
#ifdef DI_BUFFER_DEBUG
				di_print(
					"%s:bypass_state = 1, is_bypass() %d\n",
					__func__, is_bypass(NULL));
				di_print(
					"trick_mode %d bypass_all %d\n",
					trick_mode, bypass_all);
#endif
			}

			top_bot_config(di_buf);
			queue_in(di_buf, QUEUE_PRE_READY);
			/*if previous isn't bypass post_wr_buf not recycled */
			if (di_pre_stru.di_post_wr_buf && di_pre_rdma_enable) {
				queue_in(
					di_pre_stru.di_post_inp_buf,
					QUEUE_RECYCLE);
				di_pre_stru.di_post_inp_buf = NULL;
			}

			if (
				(bypass_pre & 0x2) &&
				!di_pre_stru.cur_prog_flag)
				di_buf->post_proc_flag = -2;
			else
				di_buf->post_proc_flag = 0;
#ifdef DI_BUFFER_DEBUG
			di_print(
				"%s: %s[%d] => pre_ready_list\n", __func__,
				vframe_type_name[di_buf->type], di_buf->index);
#endif
			return 0;
		} else if (is_progressive(di_buf->vframe)) {
			if (
				is_handle_prog_frame_as_interlace(vframe) &&
				(is_progressive(vframe))) {
				struct di_buf_s *di_buf_tmp = NULL;

				vframe_in[di_buf->index] = NULL;
				di_buf->vframe->type &=
					(~VIDTYPE_TYPEMASK);
				di_buf->vframe->type |=
					VIDTYPE_INTERLACE_TOP;
				di_buf->post_proc_flag = 0;

				di_buf_tmp =
					get_di_buf_head(QUEUE_IN_FREE);
				if (check_di_buf(di_buf_tmp, 10)) {
					recycle_vframe_type_pre(di_buf);
					pr_err("DI:no free in_buffer for progressive skip.\n");
					return 0;
				}
				queue_out(di_buf_tmp);
				di_buf_tmp->vframe->private_data
					= di_buf_tmp;
				di_buf_tmp->seq = di_pre_stru.in_seq;
				di_pre_stru.in_seq++;
				vframe_in[di_buf_tmp->index] = vframe;
				memcpy(
					di_buf_tmp->vframe, vframe,
					sizeof(vframe_t));
				di_pre_stru.di_inp_buf_next
					= di_buf_tmp;
				di_buf_tmp->vframe->type
					&= (~VIDTYPE_TYPEMASK);
				di_buf_tmp->vframe->type
					|= VIDTYPE_INTERLACE_BOTTOM;
				di_buf_tmp->post_proc_flag = 0;

				di_pre_stru.di_inp_buf = di_buf;
#ifdef DI_BUFFER_DEBUG
				di_print(
			"%s: %s[%d] => di_inp_buf; %s[%d] => di_inp_buf_next\n",
					__func__,
					vframe_type_name[di_buf->type],
					di_buf->index,
					vframe_type_name[di_buf_tmp->type],
					di_buf_tmp->index);
#endif
				if (di_pre_stru.di_mem_buf_dup_p == NULL) {
					di_pre_stru.di_mem_buf_dup_p = di_buf;
#ifdef DI_BUFFER_DEBUG
					di_print(
				"%s: set di_mem_buf_dup_p to be di_inp_buf\n",
						__func__);
#endif
				}
			} else {
				di_buf->post_proc_flag = 0;
				if ((prog_proc_config & 0x40) ||
				    di_pre_stru.force_interlace)
					di_buf->post_proc_flag = 1;
				di_pre_stru.di_inp_buf = di_buf;
#ifdef DI_BUFFER_DEBUG
				di_print(
					"%s: %s[%d] => di_inp_buf\n",
					__func__,
					vframe_type_name[di_buf->type],
					di_buf->index);
#endif
				if (
					di_pre_stru.di_mem_buf_dup_p == NULL) {
					/* use n */
					di_pre_stru.di_mem_buf_dup_p
						= di_buf;
#ifdef DI_BUFFER_DEBUG
					di_print(
				"%s: set di_mem_buf_dup_p to be di_inp_buf\n",
						__func__);
#endif
				}
			}
		} else {
			if (
				di_pre_stru.di_chan2_buf_dup_p == NULL) {
				di_pre_stru.field_count_for_cont = 0;
				/* ignore contp2rd and contprd */
			}
			di_buf->post_proc_flag = 1;
			di_pre_stru.di_inp_buf = di_buf;
			di_print("%s: %s[%d] => di_inp_buf\n", __func__,
				vframe_type_name[di_buf->type], di_buf->index);

			if (di_pre_stru.di_mem_buf_dup_p == NULL) {/* use n */
				di_pre_stru.di_mem_buf_dup_p = di_buf;
#ifdef DI_BUFFER_DEBUG
				di_print(
				"%s: set di_mem_buf_dup_p to be di_inp_buf\n",
					__func__);
#endif
			}
		}
	}

	/* di_wr_buf */
	if (di_pre_stru.prog_proc_type == 2) {
		di_linked_buf_idx = peek_free_linked_buf();
		if (di_linked_buf_idx != -1)
			di_buf = get_free_linked_buf(di_linked_buf_idx);
		else
			di_buf = NULL;
		if (di_buf == NULL) {
			/* recycle_vframe_type_pre(di_pre_stru.di_inp_buf);
			 *save for next process
			 */
			recycle_keep_buffer();
			di_pre_stru.di_inp_buf_next = di_pre_stru.di_inp_buf;
			return 0;
		}
		di_buf->post_proc_flag = 0;
		di_buf->di_wr_linked_buf->pre_ref_count = 0;
		di_buf->di_wr_linked_buf->post_ref_count = 0;
		di_buf->canvas_config_flag = 1;
	} else {
		di_buf = get_di_buf_head(QUEUE_LOCAL_FREE);
		if (check_di_buf(di_buf, 11)) {
			/* recycle_keep_buffer();
			 *  pr_dbg("%s:recycle keep buffer\n", __func__);
			 */
			recycle_vframe_type_pre(di_pre_stru.di_inp_buf);
			return 0;
		}
		queue_out(di_buf);
		if (di_pre_stru.prog_proc_type & 0x10)
			di_buf->canvas_config_flag = 1;
		else
			di_buf->canvas_config_flag = 2;
		di_buf->di_wr_linked_buf = NULL;
	}

	di_pre_stru.di_wr_buf = di_buf;
	di_pre_stru.di_wr_buf->pre_ref_count = 1;

#ifdef DI_BUFFER_DEBUG
	di_print("%s: %s[%d] => di_wr_buf\n", __func__,
		vframe_type_name[di_buf->type], di_buf->index);
	if (di_buf->di_wr_linked_buf)
		di_print("%s: linked %s[%d] => di_wr_buf\n", __func__,
			vframe_type_name[di_buf->di_wr_linked_buf->type],
			di_buf->di_wr_linked_buf->index);
#endif
	if (di_pre_stru.cur_inp_type & VIDTYPE_COMPRESS) {
		di_pre_stru.di_inp_buf->vframe->width =
			di_pre_stru.di_inp_buf->vframe->compWidth;
		di_pre_stru.di_inp_buf->vframe->height =
			di_pre_stru.di_inp_buf->vframe->compHeight;
	}

	memcpy(di_buf->vframe,
		di_pre_stru.di_inp_buf->vframe, sizeof(vframe_t));
	di_buf->vframe->private_data = di_buf;
	di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
	di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
	/* set vframe bit info */
	di_buf->vframe->bitdepth &= ~(BITDEPTH_YMASK);
	di_buf->vframe->bitdepth &= ~(FULL_PACK_422_MODE);
	if (de_devp->pps_enable && pps_position) {
		if (pps_dstw != di_buf->vframe->width) {
			di_buf->vframe->width = pps_dstw;
			di_pre_stru.width_bk = pps_dstw;
		}
		if (pps_dsth != di_buf->vframe->height)
			di_buf->vframe->height = pps_dsth;
	}
	if (di_force_bit_mode == 10) {
		di_buf->vframe->bitdepth |= (BITDEPTH_Y10);
		if (full_422_pack)
			di_buf->vframe->bitdepth |= (FULL_PACK_422_MODE);
	} else
		di_buf->vframe->bitdepth |= (BITDEPTH_Y8);

	if (di_pre_stru.prog_proc_type) {
		di_buf->vframe->type = VIDTYPE_PROGRESSIVE |
				       VIDTYPE_VIU_422 |
				       VIDTYPE_VIU_SINGLE_PLANE |
				       VIDTYPE_VIU_FIELD;
		if (di_pre_stru.cur_inp_type & VIDTYPE_PRE_INTERLACE)
			di_buf->vframe->type |= VIDTYPE_PRE_INTERLACE;
	} else {
		if (
			((di_pre_stru.di_inp_buf->vframe->type &
			  VIDTYPE_TYPEMASK) ==
			 VIDTYPE_INTERLACE_TOP))
			di_buf->vframe->type = VIDTYPE_INTERLACE_TOP |
					       VIDTYPE_VIU_422 |
					       VIDTYPE_VIU_SINGLE_PLANE |
					       VIDTYPE_VIU_FIELD;
		else
			di_buf->vframe->type = VIDTYPE_INTERLACE_BOTTOM |
					       VIDTYPE_VIU_422 |
					       VIDTYPE_VIU_SINGLE_PLANE |
					       VIDTYPE_VIU_FIELD;
		/*add for vpp skip line ref*/
		if (bypass_state == 0)
			di_buf->vframe->type |= VIDTYPE_PRE_INTERLACE;
	}

	if (is_bypass_post()) {
		if (bypass_post_state == 0)
			di_pre_stru.source_change_flag = 1;

		bypass_post_state = 1;
	} else {
		if (bypass_post_state)
			di_pre_stru.source_change_flag = 1;

		bypass_post_state = 0;
	}

	if (di_pre_stru.di_inp_buf->post_proc_flag == 0) {
		di_pre_stru.madi_enable = 0;
		di_pre_stru.mcdi_enable = 0;
		di_buf->post_proc_flag = 0;
		di_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	} else if (bypass_post_state) {
		di_pre_stru.madi_enable = 0;
		di_pre_stru.mcdi_enable = 0;
		di_buf->post_proc_flag = 0;
		di_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	} else {
		di_pre_stru.madi_enable = (pre_enable_mask&1);
		di_pre_stru.mcdi_enable = ((pre_enable_mask>>1)&1);
		di_buf->post_proc_flag = 1;
		di_patch_post_update_mc_sw(DI_MC_SW_OTHER, mcpre_en);//en
	}
	if ((di_pre_stru.di_mem_buf_dup_p == di_pre_stru.di_wr_buf) ||
	    (di_pre_stru.di_chan2_buf_dup_p == di_pre_stru.di_wr_buf)) {
		pr_dbg("+++++++++++++++++++++++\n");
		if (recovery_flag == 0)
			recovery_log_reason = 12;

		recovery_flag++;
		return 0;
	}
	return 1;
}

static int check_recycle_buf(void)
{
	struct di_buf_s *di_buf = NULL;/* , *ptmp; */
	int itmp;
	int ret = 0;

	if (di_blocking)
		return ret;
	queue_for_each_entry(di_buf, ptmp, QUEUE_RECYCLE, list) {
		if ((di_buf->pre_ref_count == 0) &&
		    (di_buf->post_ref_count == 0)) {
			if (di_buf->type == VFRAME_TYPE_IN) {
				queue_out(di_buf);
				if (vframe_in[di_buf->index]) {
					vf_put(
						vframe_in[di_buf->index],
						VFM_NAME);
					vf_notify_provider(VFM_NAME,
						VFRAME_EVENT_RECEIVER_PUT,
						NULL);
					di_print(
						"%s: vf_put(%d) %x, %u ms\n",
						__func__,
						di_pre_stru.recycle_seq,
						vframe_in[di_buf->index],
						jiffies_to_msecs(jiffies_64 -
				vframe_in[di_buf->index]->ready_jiffies64));
					vframe_in[di_buf->index] = NULL;
				}
				di_buf->invert_top_bot_flag = 0;
				queue_in(di_buf, QUEUE_IN_FREE);
				di_pre_stru.recycle_seq++;
				ret |= 1;
			} else {
				queue_out(di_buf);
				di_buf->invert_top_bot_flag = 0;
				queue_in(di_buf, QUEUE_LOCAL_FREE);
				if (di_buf->di_wr_linked_buf) {
					queue_in(
						di_buf->di_wr_linked_buf,
						QUEUE_LOCAL_FREE);
#ifdef DI_BUFFER_DEBUG
					di_print(
					"%s: linked %s[%d]=>recycle_list\n",
						__func__,
						vframe_type_name[
						di_buf->di_wr_linked_buf->type],
						di_buf->di_wr_linked_buf->index
					);
#endif
					di_buf->di_wr_linked_buf = NULL;
				}
				ret |= 2;
			}
#ifdef DI_BUFFER_DEBUG
			di_print("%s: recycle %s[%d]\n", __func__,
				vframe_type_name[di_buf->type], di_buf->index);
#endif
		}
	}
	return ret;
}

#ifdef DET3D
static void set3d_view(enum tvin_trans_fmt trans_fmt, struct vframe_s *vf)
{
	struct vframe_view_s *left_eye, *right_eye;

	left_eye = &vf->left_eye;
	right_eye = &vf->right_eye;

	switch (trans_fmt) {
	case TVIN_TFMT_3D_DET_LR:
	case TVIN_TFMT_3D_LRH_OLOR:
		left_eye->start_x = 0;
		left_eye->start_y = 0;
		left_eye->width = vf->width >> 1;
		left_eye->height = vf->height;
		right_eye->start_x = vf->width >> 1;
		right_eye->start_y = 0;
		right_eye->width = vf->width >> 1;
		right_eye->height = vf->height;
		break;
	case TVIN_TFMT_3D_DET_TB:
	case TVIN_TFMT_3D_TB:
		left_eye->start_x = 0;
		left_eye->start_y = 0;
		left_eye->width = vf->width;
		left_eye->height = vf->height >> 1;
		right_eye->start_x = 0;
		right_eye->start_y = vf->height >> 1;
		right_eye->width = vf->width;
		right_eye->height = vf->height >> 1;
		break;
	case TVIN_TFMT_3D_DET_INTERLACE:
		left_eye->start_x = 0;
		left_eye->start_y = 0;
		left_eye->width = vf->width;
		left_eye->height = vf->height >> 1;
		right_eye->start_x = 0;
		right_eye->start_y = 0;
		right_eye->width = vf->width;
		right_eye->height = vf->height >> 1;
		break;
	case TVIN_TFMT_3D_DET_CHESSBOARD:
/***
 * LRLRLR	  LRLRLR
 * LRLRLR  or RLRLRL
 * LRLRLR	  LRLRLR
 * LRLRLR	  RLRLRL
 */
		break;
	default: /* 2D */
		left_eye->start_x = 0;
		left_eye->start_y = 0;
		left_eye->width = 0;
		left_eye->height = 0;
		right_eye->start_x = 0;
		right_eye->start_y = 0;
		right_eye->width = 0;
		right_eye->height = 0;
		break;
	}
}

/*
 * static int get_3d_info(struct vframe_s *vf)
 * {
 * int ret = 0;
 *
 * vf->trans_fmt = det3d_fmt_detect();
 * pr_dbg("[det3d..]new 3d fmt: %d\n", vf->trans_fmt);
 *
 * vdin_set_view(vf->trans_fmt, vf);
 *
 * return ret;
 * }
 */
static unsigned int det3d_frame_cnt = 50;
module_param_named(det3d_frame_cnt, det3d_frame_cnt, uint, 0644);
static void det3d_irq(void)
{
	unsigned int data32 = 0, likely_val = 0;
	unsigned long frame_sum = 0;

	if (!det3d_en)
		return;

	data32 = det3d_fmt_detect();
	switch (data32) {
	case TVIN_TFMT_3D_DET_LR:
	case TVIN_TFMT_3D_LRH_OLOR:
		di_pre_stru.det_lr++;
		break;
	case TVIN_TFMT_3D_DET_TB:
	case TVIN_TFMT_3D_TB:
		di_pre_stru.det_tp++;
		break;
	case TVIN_TFMT_3D_DET_INTERLACE:
		di_pre_stru.det_la++;
		break;
	default:
		di_pre_stru.det_null++;
		break;
	}

	if (det3d_mode != data32) {
		det3d_mode = data32;
		di_print("[det3d..]new 3d fmt: %d\n", det3d_mode);
	}
	if (frame_count > 20) {
		frame_sum = di_pre_stru.det_lr + di_pre_stru.det_tp
					+ di_pre_stru.det_la
					+ di_pre_stru.det_null;
		if ((frame_count%det3d_frame_cnt) || (frame_sum > UINT_MAX))
			return;
		likely_val = max3(di_pre_stru.det_lr,
						di_pre_stru.det_tp,
						di_pre_stru.det_la);
		if (di_pre_stru.det_null >= likely_val)
			det3d_mode = 0;
		else if (likely_val == di_pre_stru.det_lr)
			det3d_mode = TVIN_TFMT_3D_LRH_OLOR;
		else if (likely_val == di_pre_stru.det_tp)
			det3d_mode = TVIN_TFMT_3D_TB;
		else
			det3d_mode = TVIN_TFMT_3D_DET_INTERLACE;
		di_pre_stru.det3d_trans_fmt = det3d_mode;
	} else {
		di_pre_stru.det3d_trans_fmt = 0;
	}
}
#endif

static bool calc_mcinfo_en = 1;
module_param(calc_mcinfo_en, bool, 0664);
MODULE_PARM_DESC(calc_mcinfo_en, "/n get mcinfo for post /n");

static unsigned int colcfd_thr = 128;
module_param(colcfd_thr, uint, 0664);
MODULE_PARM_DESC(colcfd_thr, "/n threshold for cfd/n");

unsigned int ro_mcdi_col_cfd[26];
static void get_mcinfo_from_reg_in_irq(void)
{
	unsigned int i = 0, ncolcrefsum = 0, blkcount = 0, *reg = NULL;

/*get info for current field process by post*/
	di_pre_stru.di_wr_buf->curr_field_mcinfo.highvertfrqflg =
		(Rd(MCDI_RO_HIGH_VERT_FRQ_FLG) & 0x1);
/* post:MCDI_MC_REL_GAIN_OFFST_0 */
	di_pre_stru.di_wr_buf->curr_field_mcinfo.motionparadoxflg =
		(Rd(MCDI_RO_MOTION_PARADOX_FLG) & 0x1);
/* post:MCDI_MC_REL_GAIN_OFFST_0 */
	reg = di_pre_stru.di_wr_buf->curr_field_mcinfo.regs;
	for (i = 0; i < 26; i++) {
		ro_mcdi_col_cfd[i] = Rd(0x2fb0 + i);
		di_pre_stru.di_wr_buf->curr_field_mcinfo.regs[i] = 0;
		if (!calc_mcinfo_en)
			*(reg + i) = ro_mcdi_col_cfd[i];
	}
	if (calc_mcinfo_en) {
		blkcount = (di_pre_stru.cur_width + 4) / 5;
		for (i = 0; i < blkcount; i++) {
			ncolcrefsum +=
				((ro_mcdi_col_cfd[i / 32] >> (i % 32)) & 0x1);
			if (
				((ncolcrefsum + (blkcount >> 1)) << 8) /
				blkcount > colcfd_thr)
				for (i = 0; i < blkcount; i++)
					*(reg + i / 32) += (1 << (i % 32));
		}
	}
}

static unsigned int bit_reverse(unsigned int val)
{
	unsigned int i = 0, res = 0;

	for (i = 0; i < 16; i++) {
		res |= (((val&(1<<i))>>i)<<(31-i));
		res |= (((val&(1<<(31-i)))<<i)>>(31-i));
	}
	return res;
}

static void set_post_mcinfo(struct mcinfo_pre_s *curr_field_mcinfo)
{
	unsigned int i = 0, value = 0;

	DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_REL_GAIN_OFFST_0,
		curr_field_mcinfo->highvertfrqflg, 24, 1);
	DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_REL_GAIN_OFFST_0,
		curr_field_mcinfo->motionparadoxflg, 25, 1);
	for (i = 0; i < 26; i++) {
		if (overturn)
			value = bit_reverse(curr_field_mcinfo->regs[i]);
		else
			value = curr_field_mcinfo->regs[i];
		DI_VSYNC_WR_MPEG_REG(0x2f78 + i, value);
	}
}
static unsigned char intr_mode;
static irqreturn_t de_irq(int irq, void *dev_instance)
{
#ifndef CHECK_DI_DONE
	unsigned int data32 = Rd(DI_INTR_CTRL);
	unsigned int mask32 = (data32 >> 16) & 0x3ff;
	unsigned int flag = 0;

	data32 &= 0x3fffffff;
	if ((data32 & 1) == 0 && di_dbg_mask&8)
		pr_info("irq[%d]pre|post=0 write done.\n", irq);
	if (di_pre_stru.pre_de_busy) {
		/* only one inetrrupr mask should be enable */
		if ((data32 & 2) && !(mask32 & 2)) {
			di_print("== MTNWR ==\n");
			flag = 1;
		} else if ((data32 & 1) && !(mask32 & 1)) {
			di_print("== NRWR ==\n");
			flag = 1;
		} else {
			di_print("== DI IRQ 0x%x ==\n", data32);
			flag = 0;
		}
	}

#endif

#ifdef DET3D
	if (det3d_en) {
		if ((data32 & 0x100) && !(mask32 & 0x100) && flag) {
			DI_Wr(DI_INTR_CTRL, data32);
			det3d_irq();
		} else {
			goto end;
		}
	} else {
		DI_Wr(DI_INTR_CTRL, data32);
	}
#else
	if (flag)
		DI_Wr(DI_INTR_CTRL, (data32&0xfffffffb)|(intr_mode<<30));
#endif

	if (di_pre_stru.pre_de_busy == 0) {
		di_print("%s: wrong enter %x\n", __func__, Rd(DI_INTR_CTRL));
		return IRQ_HANDLED;
	}

	if (flag) {
		di_pre_stru.irq_time[0] =
			(cur_to_msecs() - di_pre_stru.irq_time[0]);
		trace_di_pre("PRE-IRQ-0",
			di_pre_stru.field_count_for_cont,
			di_pre_stru.irq_time[0]);
		/*add from valsi wang.feng*/
		di_arb_sw(false);
		di_arb_sw(true);

		if (mcpre_en) {
			get_mcinfo_from_reg_in_irq();
			if ((is_meson_gxlx_cpu() &&
				di_pre_stru.field_count_for_cont >= 4) ||
				is_meson_txhd_cpu())
				mc_pre_mv_irq();
			calc_lmv_base_mcinfo((di_pre_stru.cur_height>>1),
				di_pre_stru.di_wr_buf->mcinfo_adr,
				di_pre_stru.mcinfo_size);
		}
		nr_process_in_irq();
		if ((data32&0x200) && de_devp->nrds_enable)
			nr_ds_irq();
		/* disable mif */
		enable_di_pre_mif(false, mcpre_en);
		di_pre_stru.pre_de_process_done = 1;
		di_pre_stru.pre_de_busy = 0;

		if (init_flag)
			/* pr_dbg("%s:up di sema\n", __func__); */
			trigger_pre_di_process(TRIGGER_PRE_BY_DE_IRQ);
	}

	return IRQ_HANDLED;
}

static irqreturn_t post_irq(int irq, void *dev_instance)
{
	unsigned int data32 = Rd(DI_INTR_CTRL);

	data32 &= 0x3fffffff;
	if ((data32 & 4) == 0) {
	if (di_dbg_mask&8)
		pr_info("irq[%d]post write undone.\n", irq);
		return IRQ_HANDLED;
	}
	if ((post_wr_en && post_wr_support) && (data32&0x4)) {
		di_post_stru.de_post_process_done = 1;
		di_post_stru.post_de_busy = 0;
		di_post_stru.irq_time =
			(cur_to_msecs() - di_post_stru.irq_time);
		trace_di_post("POST-IRQ-1",
				di_post_stru.post_wr_cnt,
				di_post_stru.irq_time);
		DI_Wr(DI_INTR_CTRL, (data32&0xffff0004)|(intr_mode<<30));
		/* disable wr back avoid pps sreay in g12a */
		DI_Wr_reg_bits(DI_POST_CTRL, 0, 7, 1);
	}

	if (init_flag)
		trigger_pre_di_process(TRIGGER_PRE_BY_DE_IRQ);

	return IRQ_HANDLED;
}
/*
 * di post process
 */
static void inc_post_ref_count(struct di_buf_s *di_buf)
{
/* int post_blend_mode; */

	if (IS_ERR_OR_NULL(di_buf)) {
		pr_dbg("%s: Error\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 13;

		recovery_flag++;
		return;
	}

	if (di_buf->di_buf_dup_p[1])
		di_buf->di_buf_dup_p[1]->post_ref_count++;

	if (di_buf->di_buf_dup_p[0])
		di_buf->di_buf_dup_p[0]->post_ref_count++;

	if (di_buf->di_buf_dup_p[2])
		di_buf->di_buf_dup_p[2]->post_ref_count++;
}

static void dec_post_ref_count(struct di_buf_s *di_buf)
{
	if (IS_ERR_OR_NULL(di_buf)) {
		pr_dbg("%s: Error\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 14;

		recovery_flag++;
		return;
	}
	if (di_buf->pd_config.global_mode == PULL_DOWN_BUF1)
		return;
	if (di_buf->di_buf_dup_p[1])
		di_buf->di_buf_dup_p[1]->post_ref_count--;

	if (di_buf->di_buf_dup_p[0] &&
	    di_buf->di_buf_dup_p[0]->post_proc_flag != -2)
		di_buf->di_buf_dup_p[0]->post_ref_count--;

	if (di_buf->di_buf_dup_p[2])
		di_buf->di_buf_dup_p[2]->post_ref_count--;
}

static void vscale_skip_disable_post(struct di_buf_s *di_buf, vframe_t *disp_vf)
{
	struct di_buf_s *di_buf_i = NULL;
	int canvas_height = di_buf->di_buf[0]->canvas_height;

	if (di_vscale_skip_enable & 0x2) {/* drop the bottom field */
		if ((di_buf->di_buf_dup_p[0]) && (di_buf->di_buf_dup_p[1]))
			di_buf_i =
				(di_buf->di_buf_dup_p[1]->vframe->type &
				 VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_TOP ? di_buf->di_buf_dup_p[1]
				: di_buf->di_buf_dup_p[0];
		else
			di_buf_i = di_buf->di_buf[0];
	} else {
		if ((di_buf->di_buf[0]->post_proc_flag > 0)
		    && (di_buf->di_buf_dup_p[1]))
			di_buf_i = di_buf->di_buf_dup_p[1];
		else
			di_buf_i = di_buf->di_buf[0];
	}
	disp_vf->type = di_buf_i->vframe->type;
	/* pr_dbg("%s (%x %x) (%x %x)\n", __func__,
	 * disp_vf, disp_vf->type, di_buf_i->vframe,
	 * di_buf_i->vframe->type);
	 */
	disp_vf->width = di_buf_i->vframe->width;
	disp_vf->height = di_buf_i->vframe->height;
	disp_vf->duration = di_buf_i->vframe->duration;
	disp_vf->pts = di_buf_i->vframe->pts;
	disp_vf->flag = di_buf_i->vframe->flag;
	disp_vf->canvas0Addr = di_post_idx[di_post_stru.canvas_id][0];
	disp_vf->canvas1Addr = di_post_idx[di_post_stru.canvas_id][0];
	canvas_config(
		di_post_idx[di_post_stru.canvas_id][0],
		di_buf_i->nr_adr, di_buf_i->canvas_width[NR_CANVAS],
		canvas_height, 0, 0);
	disable_post_deinterlace_2();
	di_post_stru.vscale_skip_flag = true;
}
static void process_vscale_skip(struct di_buf_s *di_buf, vframe_t *disp_vf)
{
	int ret = 0, vpp_3d_mode = 0;

	if ((di_buf->di_buf[0] != NULL) && (di_vscale_skip_enable & 0x5) &&
	    (di_buf->process_fun_index != PROCESS_FUN_NULL)) {
		ret = get_current_vscale_skip_count(disp_vf);
		di_vscale_skip_count = (ret & 0xff);
		vpp_3d_mode = ((ret >> 8) & 0xff);
		if (((di_vscale_skip_count > 0) &&
		     (di_vscale_skip_enable & 0x5))
		    || (di_vscale_skip_enable >> 16)
		    ) {
			if ((di_vscale_skip_enable & 0x8) || vpp_3d_mode) {
				vscale_skip_disable_post(di_buf, disp_vf);
			} else {
				if (di_buf->di_buf_dup_p[1] &&
					di_buf->pd_config.global_mode !=
						PULL_DOWN_BUF1)
					di_buf->pd_config.global_mode =
						PULL_DOWN_EI;
			}
		}
	}
}
static int do_post_wr_fun(void *arg, vframe_t *disp_vf)
{
	di_post_stru.toggle_flag = true;
	return 1;
}
static int de_post_disable_fun(void *arg, vframe_t *disp_vf)
{
	struct di_buf_s *di_buf = (struct di_buf_s *)arg;

	di_post_stru.vscale_skip_flag = false;
	di_post_stru.toggle_flag = true;

	process_vscale_skip(di_buf, disp_vf);
/* for atv static image flickering */
	if (di_buf->process_fun_index == PROCESS_FUN_NULL)
		disable_post_deinterlace_2();

	return 1;
/* called for new_format_flag, make
 * video set video_property_changed
 */
}

static int do_nothing_fun(void *arg, vframe_t *disp_vf)
{
	struct di_buf_s *di_buf = (struct di_buf_s *)arg;

	di_post_stru.vscale_skip_flag = false;
	di_post_stru.toggle_flag = true;

	process_vscale_skip(di_buf, disp_vf);

	if (di_buf->process_fun_index == PROCESS_FUN_NULL) {
		if (Rd(DI_IF1_GEN_REG) & 0x1 || Rd(DI_POST_CTRL) & 0xf)
			disable_post_deinterlace_2();
	/*if(di_buf->pulldown_mode == PULL_DOWN_EI && Rd(DI_IF1_GEN_REG)&0x1)
	 * DI_VSYNC_WR_MPEG_REG(DI_IF1_GEN_REG, 0x3 << 30);
	 */
	}
	return 0;
}

static int do_pre_only_fun(void *arg, vframe_t *disp_vf)
{
	di_post_stru.vscale_skip_flag = false;
	di_post_stru.toggle_flag = true;

#ifdef DI_USE_FIXED_CANVAS_IDX
	if (arg) {
		struct di_buf_s *di_buf = (struct di_buf_s *)arg;
		vframe_t *vf = di_buf->vframe;
		int width, canvas_height;

		if ((vf == NULL) || (di_buf->di_buf[0] == NULL)) {
			di_print("error:%s,NULL point!!\n", __func__);
			return 0;
		}
		width = di_buf->di_buf[0]->canvas_width[NR_CANVAS];
		/* linked two interlace buffer should double height*/
		if (di_buf->di_buf[0]->di_wr_linked_buf)
			canvas_height =
			(di_buf->di_buf[0]->canvas_height << 1);
		else
			canvas_height =
			di_buf->di_buf[0]->canvas_height;
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		if (is_vsync_rdma_enable()) {
			di_post_stru.canvas_id = di_post_stru.next_canvas_id;
		} else {
			di_post_stru.canvas_id = 0;
			di_post_stru.next_canvas_id = 1;
			if (post_wr_en && post_wr_support)
				di_post_stru.canvas_id =
					di_post_stru.next_canvas_id;
		}
#endif

		canvas_config(
			di_post_idx[di_post_stru.canvas_id][0],
			di_buf->di_buf[0]->nr_adr,
			di_buf->di_buf[0]->canvas_width[NR_CANVAS],
			canvas_height, 0, 0);

		vf->canvas0Addr =
			di_post_idx[di_post_stru.canvas_id][0];
		vf->canvas1Addr =
			di_post_idx[di_post_stru.canvas_id][0];
#ifdef DET3D
		if (di_pre_stru.vframe_interleave_flag && di_buf->di_buf[1]) {
			canvas_config(
				di_post_idx[di_post_stru.canvas_id][1],
				di_buf->di_buf[1]->nr_adr,
				di_buf->di_buf[1]->canvas_width[NR_CANVAS],
				canvas_height, 0, 0);
			vf->canvas1Addr =
				di_post_idx[di_post_stru.canvas_id][1];
			vf->duration <<= 1;
		}
#endif
		di_post_stru.next_canvas_id = di_post_stru.canvas_id ? 0 : 1;

		if (di_buf->process_fun_index == PROCESS_FUN_NULL) {
			if (Rd(DI_IF1_GEN_REG) & 0x1 ||
				Rd(DI_POST_CTRL) & 0x10f)
				disable_post_deinterlace_2();
		}

	}
#endif

	return 0;
}

static void get_vscale_skip_count(unsigned int par)
{
	di_vscale_skip_count_real = (par >> 24) & 0xff;
}

#define get_vpp_reg_update_flag(par) ((par >> 16) & 0x1)

static unsigned int pldn_dly = 1;

static unsigned int post_blend;
module_param(post_blend, uint, 0664);
MODULE_PARM_DESC(post_blend, "/n show blend mode/n");
static unsigned int post_ei;
module_param(post_ei, uint, 0664);
MODULE_PARM_DESC(post_ei, "/n show blend mode/n");

static unsigned int post_cnt;
module_param(post_cnt, uint, 0664);
MODULE_PARM_DESC(post_cnt, "/n show blend mode/n");
static bool post_refresh;
module_param_named(post_refresh, post_refresh, bool, 0644);
static int
de_post_process(void *arg, unsigned int zoom_start_x_lines,
		unsigned int zoom_end_x_lines, unsigned int zoom_start_y_lines,
		unsigned int zoom_end_y_lines, vframe_t *disp_vf)
{
	struct di_buf_s *di_buf = (struct di_buf_s *)arg;
	struct di_buf_s *di_pldn_buf = NULL;
	unsigned int di_width, di_height, di_start_x, di_end_x, mv_offset;
	unsigned int di_start_y, di_end_y, hold_line = post_hold_line;
	unsigned int post_blend_en = 0, post_blend_mode = 0,
		     blend_mtn_en = 0, ei_en = 0, post_field_num = 0;
	int di_vpp_en, di_ddr_en;
	unsigned char mc_pre_flag = 0;
	bool invert_mv = false;
	static int post_index = -1;
	unsigned char tmp_idx = 0;

	post_cnt++;
	if (di_post_stru.vscale_skip_flag)
		return 0;

	get_vscale_skip_count(zoom_start_x_lines);

	if (IS_ERR_OR_NULL(di_buf))
		return 0;
	else if (IS_ERR_OR_NULL(di_buf->di_buf_dup_p[0]))
		return 0;
	di_pldn_buf = di_buf->di_buf_dup_p[pldn_dly];

	if (is_in_queue(di_buf, QUEUE_POST_FREE) &&
			post_index != di_buf->index) {
		post_index = di_buf->index;
		pr_info("%s post_buf[%d] is in post free list.\n",
				__func__, di_buf->index);
		return 0;
	}

	if (di_post_stru.toggle_flag && di_buf->di_buf_dup_p[1])
		top_bot_config(di_buf->di_buf_dup_p[1]);

	di_post_stru.toggle_flag = false;

	di_post_stru.cur_disp_index = di_buf->index;

	if (get_vpp_reg_update_flag(zoom_start_x_lines) || post_refresh)
		di_post_stru.update_post_reg_flag = 1;

	zoom_start_x_lines = zoom_start_x_lines & 0xffff;
	zoom_end_x_lines = zoom_end_x_lines & 0xffff;
	zoom_start_y_lines = zoom_start_y_lines & 0xffff;
	zoom_end_y_lines = zoom_end_y_lines & 0xffff;

	if (init_flag == 0 && IS_ERR_OR_NULL(di_post_stru.keep_buf))
		return 0;

	di_start_x = zoom_start_x_lines;
	di_end_x = zoom_end_x_lines;
	di_width = di_end_x - di_start_x + 1;
	di_start_y = zoom_start_y_lines;
	di_end_y = zoom_end_y_lines;
	di_height = di_end_y - di_start_y + 1;
	di_height = di_height / (di_vscale_skip_count_real + 1);
	/* make sure the height is even number */
	if (di_height%2)
		di_height++;

	if (Rd(DI_POST_SIZE) != ((di_width - 1) | ((di_height - 1) << 16)) ||
	    di_post_stru.buf_type != di_buf->di_buf_dup_p[0]->type ||
	    (di_post_stru.di_buf0_mif.luma_x_start0 != di_start_x) ||
	    (di_post_stru.di_buf0_mif.luma_y_start0 != di_start_y / 2)) {
		di_post_stru.buf_type = di_buf->di_buf_dup_p[0]->type;

		initial_di_post_2(di_width, di_height,
			hold_line,
			(post_wr_en && post_wr_support));

		if ((di_buf->di_buf_dup_p[0]->vframe == NULL) ||
			(di_buf->vframe == NULL))
			return 0;
		/* bit mode config */
		if (di_buf->vframe->bitdepth & BITDEPTH_Y10) {
			if (di_buf->vframe->type & VIDTYPE_VIU_444) {
				di_post_stru.di_buf0_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:2;
				di_post_stru.di_buf1_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:2;
				di_post_stru.di_buf2_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:2;
				di_post_stru.di_diwr_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:2;

			} else {
				di_post_stru.di_buf0_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:1;
				di_post_stru.di_buf1_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:1;
				di_post_stru.di_buf2_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:1;
				di_post_stru.di_diwr_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:1;
			}
		} else {
			di_post_stru.di_buf0_mif.bit_mode = 0;
			di_post_stru.di_buf1_mif.bit_mode = 0;
			di_post_stru.di_buf2_mif.bit_mode = 0;
			di_post_stru.di_diwr_mif.bit_mode = 0;
		}
		if (di_buf->vframe->type & VIDTYPE_VIU_444) {
			di_post_stru.di_buf0_mif.video_mode = 1;
			di_post_stru.di_buf1_mif.video_mode = 1;
			di_post_stru.di_buf2_mif.video_mode = 1;
		} else {
			di_post_stru.di_buf0_mif.video_mode = 0;
			di_post_stru.di_buf1_mif.video_mode = 0;
			di_post_stru.di_buf2_mif.video_mode = 0;
		}
		if (di_post_stru.buf_type == VFRAME_TYPE_IN &&
		    !(di_buf->di_buf_dup_p[0]->vframe->type &
		      VIDTYPE_VIU_FIELD)) {
			if (di_buf->vframe->type & VIDTYPE_VIU_NV21) {
				di_post_stru.di_buf0_mif.set_separate_en = 1;
				di_post_stru.di_buf1_mif.set_separate_en = 1;
				di_post_stru.di_buf2_mif.set_separate_en = 1;
			} else {
				di_post_stru.di_buf0_mif.set_separate_en = 0;
				di_post_stru.di_buf1_mif.set_separate_en = 0;
				di_post_stru.di_buf2_mif.set_separate_en = 0;
			}
			di_post_stru.di_buf0_mif.luma_y_start0 = di_start_y;
			di_post_stru.di_buf0_mif.luma_y_end0 = di_end_y;
		} else { /* from vdin or local vframe process by di pre */
			di_post_stru.di_buf0_mif.set_separate_en = 0;
			di_post_stru.di_buf0_mif.luma_y_start0 =
				di_start_y >> 1;
			di_post_stru.di_buf0_mif.luma_y_end0 = di_end_y >> 1;
			di_post_stru.di_buf1_mif.set_separate_en = 0;
			di_post_stru.di_buf1_mif.luma_y_start0 =
				di_start_y >> 1;
			di_post_stru.di_buf1_mif.luma_y_end0 = di_end_y >> 1;
			di_post_stru.di_buf2_mif.set_separate_en = 0;
			di_post_stru.di_buf2_mif.luma_y_end0 = di_end_y >> 1;
			di_post_stru.di_buf2_mif.luma_y_start0 =
				di_start_y >> 1;
		}
		di_post_stru.di_buf0_mif.luma_x_start0 = di_start_x;
		di_post_stru.di_buf0_mif.luma_x_end0 = di_end_x;
		di_post_stru.di_buf1_mif.luma_x_start0 = di_start_x;
		di_post_stru.di_buf1_mif.luma_x_end0 = di_end_x;
		di_post_stru.di_buf2_mif.luma_x_start0 = di_start_x;
		di_post_stru.di_buf2_mif.luma_x_end0 = di_end_x;

		if (post_wr_en && post_wr_support) {
			if (de_devp->pps_enable && pps_position == 0) {
				di_pps_config(0, di_width, di_height,
					pps_dstw, pps_dsth);
				di_post_stru.di_diwr_mif.start_x = 0;
				di_post_stru.di_diwr_mif.end_x   = pps_dstw - 1;
				di_post_stru.di_diwr_mif.start_y = 0;
				di_post_stru.di_diwr_mif.end_y   = pps_dsth - 1;
			} else {
				di_post_stru.di_diwr_mif.start_x = di_start_x;
				di_post_stru.di_diwr_mif.end_x   = di_end_x;
				di_post_stru.di_diwr_mif.start_y = di_start_y;
				di_post_stru.di_diwr_mif.end_y   = di_end_y;
			}
		}

		di_post_stru.di_mtnprd_mif.start_x = di_start_x;
		di_post_stru.di_mtnprd_mif.end_x = di_end_x;
		di_post_stru.di_mtnprd_mif.start_y = di_start_y >> 1;
		di_post_stru.di_mtnprd_mif.end_y = di_end_y >> 1;
		if (mcpre_en) {
			di_post_stru.di_mcvecrd_mif.start_x = di_start_x / 5;
			mv_offset = (di_start_x % 5) ? (5 - di_start_x % 5) : 0;
			di_post_stru.di_mcvecrd_mif.vecrd_offset =
				overturn ? (di_end_x + 1) % 5 : mv_offset;
			di_post_stru.di_mcvecrd_mif.start_y =
				(di_start_y >> 1);
			di_post_stru.di_mcvecrd_mif.size_x =
				(di_end_x + 1 + 4) / 5 - 1 - di_start_x / 5;
			di_post_stru.di_mcvecrd_mif.end_y =
				(di_end_y >> 1);
		}
		di_post_stru.update_post_reg_flag = 1;
		/* if height decrease, mtn will not enough */
		if (di_buf->pd_config.global_mode != PULL_DOWN_BUF1 &&
			!post_wr_en)
			di_buf->pd_config.global_mode = PULL_DOWN_EI;
	}

#ifdef DI_USE_FIXED_CANVAS_IDX
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	if (is_vsync_rdma_enable()) {
		di_post_stru.canvas_id = di_post_stru.next_canvas_id;
	} else {
		di_post_stru.canvas_id = 0;
		di_post_stru.next_canvas_id = 1;
		if (post_wr_en && post_wr_support)
			di_post_stru.canvas_id =
				di_post_stru.next_canvas_id;
	}
#endif
	post_blend = di_buf->pd_config.global_mode;
	switch (post_blend) {
	case PULL_DOWN_BLEND_0:
	case PULL_DOWN_NORMAL:
		config_canvas_idx(
			di_buf->di_buf_dup_p[1],
			di_post_idx[di_post_stru.canvas_id][0], -1);
		config_canvas_idx(
			di_buf->di_buf_dup_p[2], -1,
			di_post_idx[di_post_stru.canvas_id][2]);
		config_canvas_idx(
			di_buf->di_buf_dup_p[0],
			di_post_idx[di_post_stru.canvas_id][1], -1);
		config_canvas_idx(
			di_buf->di_buf_dup_p[2],
			di_post_idx[di_post_stru.canvas_id][3], -1);
		if (mcpre_en)
			config_mcvec_canvas_idx(
				di_buf->di_buf_dup_p[2],
				di_post_idx[di_post_stru.canvas_id][4]);
		break;
	case PULL_DOWN_BLEND_2:
	case PULL_DOWN_NORMAL_2:
		config_canvas_idx(
			di_buf->di_buf_dup_p[0],
			di_post_idx[di_post_stru.canvas_id][3], -1);
		config_canvas_idx(
			di_buf->di_buf_dup_p[1],
			di_post_idx[di_post_stru.canvas_id][0], -1);
		config_canvas_idx(
			di_buf->di_buf_dup_p[2], -1,
			di_post_idx[di_post_stru.canvas_id][2]);
		config_canvas_idx(
			di_buf->di_buf_dup_p[2],
			di_post_idx[di_post_stru.canvas_id][1], -1);
		if (mcpre_en)
			config_mcvec_canvas_idx(
				di_buf->di_buf_dup_p[2],
				di_post_idx[di_post_stru.canvas_id][4]);
		break;
	case PULL_DOWN_MTN:
		config_canvas_idx(
			di_buf->di_buf_dup_p[1],
			di_post_idx[di_post_stru.canvas_id][0], -1);
		config_canvas_idx(
			di_buf->di_buf_dup_p[2], -1,
			di_post_idx[di_post_stru.canvas_id][2]);
		config_canvas_idx(
			di_buf->di_buf_dup_p[0],
			di_post_idx[di_post_stru.canvas_id][1], -1);
		break;
	case PULL_DOWN_BUF1:/* wave with buf1 */
		config_canvas_idx(
			di_buf->di_buf_dup_p[1],
			di_post_idx[di_post_stru.canvas_id][0], -1);
		config_canvas_idx(
			di_buf->di_buf_dup_p[1], -1,
			di_post_idx[di_post_stru.canvas_id][2]);
		config_canvas_idx(
			di_buf->di_buf_dup_p[0],
			di_post_idx[di_post_stru.canvas_id][1], -1);
		break;
	case PULL_DOWN_EI:
		if (di_buf->di_buf_dup_p[1])
			config_canvas_idx(
				di_buf->di_buf_dup_p[1],
				di_post_idx[di_post_stru.canvas_id][0], -1);
		break;
	default:
		break;
	}
	di_post_stru.next_canvas_id = di_post_stru.canvas_id ? 0 : 1;
#endif
	if (di_buf->di_buf_dup_p[1] == NULL)
		return 0;
	if ((di_buf->di_buf_dup_p[1]->vframe == NULL) ||
		di_buf->di_buf_dup_p[0]->vframe == NULL)
		return 0;

	if (is_meson_txl_cpu() && overturn && di_buf->di_buf_dup_p[2]) {
		/*sync from kernel 3.14 txl*/
		if (post_blend == PULL_DOWN_BLEND_2)
			post_blend = PULL_DOWN_BLEND_0;
		else if (post_blend == PULL_DOWN_BLEND_0)
			post_blend = PULL_DOWN_BLEND_2;
	}

	switch (post_blend) {
	case PULL_DOWN_BLEND_0:
	case PULL_DOWN_NORMAL:
		post_field_num =
		(di_buf->di_buf_dup_p[1]->vframe->type &
		 VIDTYPE_TYPEMASK)
		== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		di_post_stru.di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		di_post_stru.di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		di_post_stru.di_buf2_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[2]->nr_canvas_idx;
		di_post_stru.di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		//mc_pre_flag = is_meson_txl_cpu()?2:(overturn?0:1);
		if (is_meson_txl_cpu() && overturn) {
			/* swap if1&if2 mean negation of mv for normal di*/
			tmp_idx = di_post_stru.di_buf1_mif.canvas0_addr0;
			di_post_stru.di_buf1_mif.canvas0_addr0 =
				di_post_stru.di_buf2_mif.canvas0_addr0;
			di_post_stru.di_buf2_mif.canvas0_addr0 = tmp_idx;
		}
		mc_pre_flag = overturn?0:1;
		if (di_buf->pd_config.global_mode == PULL_DOWN_NORMAL) {
			post_blend_mode = 3;
			/*if pulldown, mcdi_mcpreflag is 1,*/
			/*it means use previous field for MC*/
			/*else not pulldown,mcdi_mcpreflag is 2*/
			/*it means use forward & previous field for MC*/
			if (is_meson_txhd_cpu())
				mc_pre_flag = 2;
		} else {
			if (is_meson_txhd_cpu())
				mc_pre_flag = 1;
			post_blend_mode = 1;
		}
		if (is_meson_txl_cpu() && overturn)
			mc_pre_flag = 1;

		if (mcpre_en) {
			di_post_stru.di_mcvecrd_mif.canvas_num =
				di_buf->di_buf_dup_p[2]->mcvec_canvas_idx;
		}
		blend_mtn_en = 1;
		post_ei = ei_en = 1;
		post_blend_en = 1;
		break;
	case PULL_DOWN_BLEND_2:
	case PULL_DOWN_NORMAL_2:
		post_field_num =
			(di_buf->di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK)
			== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		di_post_stru.di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		di_post_stru.di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[2]->nr_canvas_idx;
		di_post_stru.di_buf2_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		di_post_stru.di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		if (is_meson_txl_cpu() && overturn) {
			di_post_stru.di_buf1_mif.canvas0_addr0 =
			di_post_stru.di_buf2_mif.canvas0_addr0;
		}
		if (mcpre_en) {
			di_post_stru.di_mcvecrd_mif.canvas_num =
				di_buf->di_buf_dup_p[2]->mcvec_canvas_idx;
			mc_pre_flag = is_meson_txl_cpu()?0:(overturn?1:0);
			if (is_meson_txlx_cpu() || is_meson_gxlx_cpu() ||
				is_meson_txhd_cpu())
				invert_mv = true;
			else if (!overturn)
				di_post_stru.di_buf2_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[2]->nr_canvas_idx;
		}
		if (di_buf->pd_config.global_mode == PULL_DOWN_NORMAL_2) {
			post_blend_mode = 3;
			/*if pulldown, mcdi_mcpreflag is 1,*/
			/*it means use previous field for MC*/
			/*else not pulldown,mcdi_mcpreflag is 2*/
			/*it means use forward & previous field for MC*/
			if (is_meson_txhd_cpu())
				mc_pre_flag = 2;
		} else {
			if (is_meson_txhd_cpu())
				mc_pre_flag = 1;
			post_blend_mode = 1;
		}
		blend_mtn_en = 1;
		post_ei = ei_en = 1;
		post_blend_en = 1;
		break;
	case PULL_DOWN_MTN:
		post_field_num =
			(di_buf->di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK)
			== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		di_post_stru.di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		di_post_stru.di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		di_post_stru.di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		post_blend_mode = 0;
		blend_mtn_en = 1;
		post_ei = ei_en = 1;
		post_blend_en = 1;
		break;
	case PULL_DOWN_BUF1:
		post_field_num =
			(di_buf->di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK)
			== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		di_post_stru.di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		di_post_stru.di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[1]->mtn_canvas_idx;
		di_post_stru.di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		post_blend_mode = 1;
		blend_mtn_en = 0;
		post_ei = ei_en = 0;
		post_blend_en = 0;
		break;
	case PULL_DOWN_EI:
		if (di_buf->di_buf_dup_p[1]) {
			di_post_stru.di_buf0_mif.canvas0_addr0 =
				di_buf->di_buf_dup_p[1]->nr_canvas_idx;
			post_field_num =
				(di_buf->di_buf_dup_p[1]->vframe->type &
				 VIDTYPE_TYPEMASK)
				== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		} else {
			post_field_num =
				(di_buf->di_buf_dup_p[0]->vframe->type &
				 VIDTYPE_TYPEMASK)
				== VIDTYPE_INTERLACE_TOP ? 0 : 1;
			di_post_stru.di_buf0_mif.src_field_mode
				= post_field_num;
		}
		post_blend_mode = 2;
		blend_mtn_en = 0;
		post_ei = ei_en = 1;
		post_blend_en = 0;
		break;
	default:
		break;
	}

	if (post_wr_en && post_wr_support) {
		config_canvas_idx(di_buf,
			di_post_idx[di_post_stru.canvas_id][5], -1);
		di_post_stru.di_diwr_mif.canvas_num = di_buf->nr_canvas_idx;
		di_vpp_en = 0;
		di_ddr_en = 1;
	} else {
		di_vpp_en = 1;
		di_ddr_en = 0;
	}

	/* if post size < MIN_POST_WIDTH, force ei */
	if ((di_width < MIN_BLEND_WIDTH) &&
		(di_buf->pd_config.global_mode == PULL_DOWN_BLEND_0 ||
		di_buf->pd_config.global_mode == PULL_DOWN_BLEND_2 ||
		di_buf->pd_config.global_mode == PULL_DOWN_NORMAL
		)) {
		post_blend_mode = 1;
		blend_mtn_en = 0;
		post_ei = ei_en = 0;
		post_blend_en = 0;
	}

	if (mcpre_en)
		di_post_stru.di_mcvecrd_mif.blend_en = post_blend_en;
	invert_mv = overturn ? (!invert_mv) : invert_mv;
	if (di_post_stru.update_post_reg_flag) {
		enable_di_post_2(
			&di_post_stru.di_buf0_mif,
			&di_post_stru.di_buf1_mif,
			&di_post_stru.di_buf2_mif,
			&di_post_stru.di_diwr_mif,
			&di_post_stru.di_mtnprd_mif,
			ei_en,                  /* ei enable */
			post_blend_en,          /* blend enable */
			blend_mtn_en,           /* blend mtn enable */
			post_blend_mode,        /* blend mode. */
			di_vpp_en,		/* di_vpp_en. */
			di_ddr_en,		/* di_ddr_en. */
			post_field_num,         /* 1 bottom generate top */
			hold_line,
			post_urgent,
			(invert_mv?1:0),
			di_vscale_skip_count_real
			);
		if (mcpre_en) {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
				enable_mc_di_post_g12(
					&di_post_stru.di_mcvecrd_mif,
					post_urgent,
					overturn, (invert_mv?1:0));
			else
				enable_mc_di_post(
					&di_post_stru.di_mcvecrd_mif,
					post_urgent,
					overturn, (invert_mv?1:0));
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
			DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, 0, 0, 2);
	} else {
		di_post_switch_buffer(
			&di_post_stru.di_buf0_mif,
			&di_post_stru.di_buf1_mif,
			&di_post_stru.di_buf2_mif,
			&di_post_stru.di_diwr_mif,
			&di_post_stru.di_mtnprd_mif,
			&di_post_stru.di_mcvecrd_mif,
			ei_en,                  /* ei enable */
			post_blend_en,          /* blend enable */
			blend_mtn_en,           /* blend mtn enable */
			post_blend_mode,        /* blend mode. */
			di_vpp_en,	/* di_vpp_en. */
			di_ddr_en,	/* di_ddr_en. */
			post_field_num,         /* 1 bottom generate top */
			hold_line,
			post_urgent,
			(invert_mv?1:0),
			pulldown_enable,
			mcpre_en,
			di_vscale_skip_count_real
			);
	}

		if (is_meson_gxtvbb_cpu()   ||
			is_meson_txl_cpu()  ||
			is_meson_txlx_cpu() ||
			is_meson_gxlx_cpu() ||
			is_meson_txhd_cpu() ||
			is_meson_g12a_cpu() ||
			is_meson_g12b_cpu() ||
			is_meson_sm1_cpu()) {
		di_post_read_reverse_irq(overturn, mc_pre_flag,
			post_blend_en ? mcpre_en : false);
		/* disable mc for first 2 fieldes mv unreliable */
		if (di_buf->seq < 2)
			DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, 0, 0, 2);
	}
	if (mcpre_en) {
		if (di_buf->di_buf_dup_p[2])
			set_post_mcinfo(&di_buf->di_buf_dup_p[2]
				->curr_field_mcinfo);
	} else if (is_meson_gxlx_cpu()
		|| is_meson_txl_cpu()
		|| is_meson_txlx_cpu())
		DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, 0, 0, 2);


/* set pull down region (f(t-1) */

	if (di_pldn_buf && pulldown_enable &&
		!di_pre_stru.cur_prog_flag) {
		unsigned short offset = (di_start_y>>1);

		if (overturn)
			offset = ((di_buf->vframe->height - di_end_y)>>1);
		else
			offset = 0;
		pulldown_vof_win_vshift(&di_pldn_buf->pd_config, offset);
		pulldown_vof_win_config(&di_pldn_buf->pd_config);
	}

	if (di_post_stru.update_post_reg_flag > 0)
		di_post_stru.update_post_reg_flag--;
	return 0;
}


static void post_de_done_buf_config(void)
{
	ulong irq_flag2 = 0;
	struct di_buf_s *di_buf = NULL;

	if (di_post_stru.cur_post_buf == NULL)
		return;

	di_lock_irqfiq_save(irq_flag2);
	queue_out(di_post_stru.cur_post_buf);
	di_buf = di_post_stru.cur_post_buf;
	if (de_devp->pps_enable && pps_position == 0) {
		di_buf->vframe->width = pps_dstw;
		di_buf->vframe->height = pps_dsth;
	}
	queue_in(di_post_stru.cur_post_buf, QUEUE_POST_READY);
	di_unlock_irqfiq_restore(irq_flag2);
	vf_notify_receiver(VFM_NAME, VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	di_post_stru.cur_post_buf = NULL;

}

static void di_post_process(void)
{
	struct di_buf_s *di_buf = NULL;
	vframe_t *vf_p = NULL;

	if (di_post_stru.post_de_busy)
		return;
	if (queue_empty(QUEUE_POST_DOING)) {
		di_post_stru.post_peek_underflow++;
		return;
	}

	di_buf = get_di_buf_head(QUEUE_POST_DOING);
	if (check_di_buf(di_buf, 20))
		return;
	vf_p = di_buf->vframe;
	if (di_post_stru.run_early_proc_fun_flag) {
		if (vf_p->early_process_fun)
			vf_p->early_process_fun = do_post_wr_fun;
	}
	if (di_buf->process_fun_index) {

		di_post_stru.post_wr_cnt++;
		de_post_process(
			di_buf, 0, vf_p->width-1,
			0, vf_p->height-1, vf_p);
		di_post_stru.post_de_busy = 1;
		di_post_stru.irq_time = cur_to_msecs();
	} else {
		di_post_stru.de_post_process_done = 1;
	}
	di_post_stru.cur_post_buf = di_buf;
}

static void recycle_vframe_type_post(struct di_buf_s *di_buf)
{
	int i;

	if (di_buf == NULL) {
		pr_dbg("%s:Error\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 15;

		recovery_flag++;
		return;
	}
	if (di_buf->process_fun_index == PROCESS_FUN_DI)
		dec_post_ref_count(di_buf);

	for (i = 0; i < 2; i++) {
		if (di_buf->di_buf[i])
			queue_in(di_buf->di_buf[i], QUEUE_RECYCLE);
	}
	queue_out(di_buf); /* remove it from display_list_head */
	di_buf->invert_top_bot_flag = 0;
	queue_in(di_buf, QUEUE_POST_FREE);
}

#ifdef DI_BUFFER_DEBUG
static void
recycle_vframe_type_post_print(struct di_buf_s *di_buf,
			       const char *func,
			       const int	line)
{
	int i;

	di_print("%s:%d ", func, line);
	for (i = 0; i < 2; i++) {
		if (di_buf->di_buf[i])
			di_print("%s[%d]<%d>=>recycle_list; ",
				vframe_type_name[di_buf->di_buf[i]->type],
				di_buf->di_buf[i]->index, i);
	}
	di_print("%s[%d] =>post_free_list\n",
		vframe_type_name[di_buf->type], di_buf->index);
}
#endif
static int debug_blend_mode = -1;
static unsigned int pldn_dly1 = 1;
static void set_pulldown_mode(struct di_buf_s *di_buf)
{
	struct di_buf_s *pre_buf_p = di_buf->di_buf_dup_p[pldn_dly1];

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXBB)) {
		if (pulldown_enable && !di_pre_stru.cur_prog_flag) {
			if (pre_buf_p) {
				di_buf->pd_config.global_mode =
					pre_buf_p->pd_config.global_mode;
			} else {
				pr_err("DI[%s]: index out of range.\n",
					__func__);
			}
		} else {
			di_buf->pd_config.global_mode
				= PULL_DOWN_NORMAL;
		}
	}
}

void drop_frame(int check_drop, int throw_flag, struct di_buf_s *di_buf)
{
	ulong irq_flag2 = 0;
	int i = 0, drop_flag = 0;

	di_lock_irqfiq_save(irq_flag2);
	if ((frame_count == 0) && check_drop)
		di_post_stru.start_pts = di_buf->vframe->pts;
	if ((check_drop && (frame_count < start_frame_drop_count))
	|| throw_flag) {
		drop_flag = 1;
	} else {
		if (check_drop && (frame_count == start_frame_drop_count)) {
			if ((di_post_stru.start_pts)
			&& (di_buf->vframe->pts == 0))
				di_buf->vframe->pts = di_post_stru.start_pts;
			di_post_stru.start_pts = 0;
		}
		for (i = 0; i < 3; i++) {
			if (di_buf->di_buf_dup_p[i]) {
				if (di_buf->di_buf_dup_p[i]->vframe->bitdepth !=
					di_buf->vframe->bitdepth) {
					pr_info("%s buf[%d] not match bit mode\n",
						__func__, i);
					drop_flag = 1;
					break;
				}
			}
		}
	}
	if (drop_flag) {
		queue_in(di_buf, QUEUE_TMP);
		recycle_vframe_type_post(di_buf);
#ifdef DI_BUFFER_DEBUG
		recycle_vframe_type_post_print(
				di_buf, __func__,
				__LINE__);
#endif
	} else {
		if (post_wr_en && post_wr_support)
			queue_in(di_buf, QUEUE_POST_DOING);
		else
			queue_in(di_buf, QUEUE_POST_READY);
		di_print("DI:%dth %s[%d] => post ready %u ms.\n",
		frame_count, vframe_type_name[di_buf->type], di_buf->index,
		jiffies_to_msecs(jiffies_64 - di_buf->vframe->ready_jiffies64));


	}
	di_unlock_irqfiq_restore(irq_flag2);
}

static int process_post_vframe(void)
{
/*
 * 1) get buf from post_free_list, config it according to buf
 * in pre_ready_list, send it to post_ready_list
 * (it will be send to post_free_list in di_vf_put())
 * 2) get buf from pre_ready_list, attach it to buf from post_free_list
 * (it will be send to recycle_list in di_vf_put() )
 */
	ulong irq_flag2 = 0;
	int i = 0;
	int ret = 0;
	int buffer_keep_count = 3;
	struct di_buf_s *di_buf = NULL;
	struct di_buf_s *ready_di_buf;
	struct di_buf_s *p = NULL;/* , *ptmp; */
	int itmp;
	int ready_count = list_count(QUEUE_PRE_READY);
	bool check_drop = false;

	if (queue_empty(QUEUE_POST_FREE))
		return 0;

	if (ready_count == 0)
		return 0;

	ready_di_buf = get_di_buf_head(QUEUE_PRE_READY);
	if ((ready_di_buf == NULL) || (ready_di_buf->vframe == NULL)) {

		pr_dbg("%s:Error1\n", __func__);

		if (recovery_flag == 0)
			recovery_log_reason = 16;

		recovery_flag++;
		return 0;
	}

	if ((ready_di_buf->post_proc_flag) &&
	    (ready_count >= buffer_keep_count)) {
		i = 0;
		queue_for_each_entry(p, ptmp, QUEUE_PRE_READY, list) {
			/* if(p->post_proc_flag == 0){ */
			if (p->type == VFRAME_TYPE_IN) {
				ready_di_buf->post_proc_flag = -1;
				ready_di_buf->new_format_flag = 1;
			}
			i++;
			if (i > 2)
				break;
		}
	}
	if (ready_di_buf->post_proc_flag > 0) {
		if (ready_count >= buffer_keep_count) {
			di_lock_irqfiq_save(irq_flag2);
			di_buf = get_di_buf_head(QUEUE_POST_FREE);
			if (check_di_buf(di_buf, 17)) {
				di_unlock_irqfiq_restore(irq_flag2);
				return 0;
			}

			queue_out(di_buf);
			di_unlock_irqfiq_restore(irq_flag2);

			i = 0;
			queue_for_each_entry(
				p, ptmp, QUEUE_PRE_READY, list) {
				di_buf->di_buf_dup_p[i++] = p;
				if (i >= buffer_keep_count)
					break;
			}
			if (i < buffer_keep_count) {

				pr_dbg("%s:Error3\n", __func__);

				if (recovery_flag == 0)
					recovery_log_reason = 18;
				recovery_flag++;
				return 0;
			}

			memcpy(di_buf->vframe,
				di_buf->di_buf_dup_p[1]->vframe,
				sizeof(vframe_t));
			di_buf->vframe->private_data = di_buf;
			if (di_buf->di_buf_dup_p[1]->post_proc_flag == 3) {
				/* dummy, not for display */
				inc_post_ref_count(di_buf);
				di_buf->di_buf[0] = di_buf->di_buf_dup_p[0];
				di_buf->di_buf[1] = NULL;
				queue_out(di_buf->di_buf[0]);
				di_lock_irqfiq_save(irq_flag2);
				queue_in(di_buf, QUEUE_TMP);
				recycle_vframe_type_post(di_buf);

				di_unlock_irqfiq_restore(irq_flag2);
#ifdef DI_BUFFER_DEBUG
				di_print("%s <dummy>: ", __func__);
#endif
			} else {
				if (di_buf->di_buf_dup_p[1]->
				post_proc_flag == 2){
					di_buf->pd_config.global_mode
						= PULL_DOWN_BLEND_2;
					/* blend with di_buf->di_buf_dup_p[2] */
				} else {
					set_pulldown_mode(di_buf);
				}
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD |
					VIDTYPE_PRE_INTERLACE;
				di_buf->vframe->width = di_pre_stru.width_bk;
				if (
					di_buf->di_buf_dup_p[1]->
					new_format_flag) {
					/* if (di_buf->di_buf_dup_p[1]
					 * ->post_proc_flag == 2) {
					 */
					di_buf->vframe->
					early_process_fun
						= de_post_disable_fun;
				} else {
					di_buf->vframe->
					early_process_fun
						= do_nothing_fun;
				}

				if (di_buf->di_buf_dup_p[1]->type
				    == VFRAME_TYPE_IN) {
					/* next will be bypass */
					di_buf->vframe->type
						= VIDTYPE_PROGRESSIVE |
						  VIDTYPE_VIU_422 |
						  VIDTYPE_VIU_SINGLE_PLANE |
							VIDTYPE_VIU_FIELD |
							VIDTYPE_PRE_INTERLACE;
					di_buf->vframe->height >>= 1;
					di_buf->vframe->canvas0Addr =
						di_buf->di_buf_dup_p[0]
						->nr_canvas_idx; /* top */
					di_buf->vframe->canvas1Addr =
						di_buf->di_buf_dup_p[0]
						->nr_canvas_idx;
					di_buf->vframe->process_fun =
						NULL;
					di_buf->process_fun_index =
						PROCESS_FUN_NULL;
				} else {
					/*for debug*/
					if (debug_blend_mode != -1)
						di_buf->pd_config.global_mode
							= debug_blend_mode;

					di_buf->vframe->process_fun =
((post_wr_en && post_wr_support) ? NULL : de_post_process);
					di_buf->process_fun_index =
						PROCESS_FUN_DI;
					inc_post_ref_count(di_buf);
				}
				di_buf->di_buf[0]
					= di_buf->di_buf_dup_p[0];
				di_buf->di_buf[1] = NULL;
				queue_out(di_buf->di_buf[0]);

				drop_frame(true,
					(di_buf->di_buf_dup_p[0]->throw_flag) ||
					(di_buf->di_buf_dup_p[1]->throw_flag) ||
					(di_buf->di_buf_dup_p[2]->throw_flag),
					di_buf);

				frame_count++;
#ifdef DI_BUFFER_DEBUG
				di_print("%s <interlace>: ", __func__);
#endif
				if (!(post_wr_en && post_wr_support))
					vf_notify_receiver(VFM_NAME,
VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
			}
			ret = 1;
		}
	} else {
		if (is_progressive(ready_di_buf->vframe) ||
		    ready_di_buf->type == VFRAME_TYPE_IN ||
		    ready_di_buf->post_proc_flag < 0 ||
		    bypass_post_state
		    ){
			int vframe_process_count = 1;
#ifdef DET3D
			int dual_vframe_flag = 0;

			if ((di_pre_stru.vframe_interleave_flag &&
			     ready_di_buf->left_right) ||
			    (bypass_post & 0x100)) {
				dual_vframe_flag = 1;
				vframe_process_count = 2;
			}
#endif
			if (skip_top_bot &&
			    (!is_progressive(ready_di_buf->vframe)))
				vframe_process_count = 2;

			if (ready_count >= vframe_process_count) {
				struct di_buf_s *di_buf_i;

				di_lock_irqfiq_save(irq_flag2);
				di_buf = get_di_buf_head(QUEUE_POST_FREE);
				if (check_di_buf(di_buf, 19)) {
					di_unlock_irqfiq_restore(irq_flag2);
					return 0;
				}

				queue_out(di_buf);
				di_unlock_irqfiq_restore(irq_flag2);

				i = 0;
				queue_for_each_entry(
					p, ptmp, QUEUE_PRE_READY, list){
					di_buf->di_buf_dup_p[i++] = p;
					if (i >= vframe_process_count) {
						di_buf->di_buf_dup_p[i] = NULL;
						di_buf->di_buf_dup_p[i+1] =
							NULL;
						break;
					}
				}
				if (i < vframe_process_count) {
					pr_dbg("%s:Error6\n", __func__);
					if (recovery_flag == 0)
						recovery_log_reason = 22;

					recovery_flag++;
					return 0;
				}

				di_buf_i = di_buf->di_buf_dup_p[0];
				if (!is_progressive(ready_di_buf->vframe)
					&& ((skip_top_bot == 1)
					|| (skip_top_bot == 2))) {
					unsigned int frame_type =
						di_buf->di_buf_dup_p[1]->
						vframe->type & VIDTYPE_TYPEMASK;
					if (skip_top_bot == 1) {
						di_buf_i = (frame_type ==
						VIDTYPE_INTERLACE_TOP)
						? di_buf->di_buf_dup_p[1]
						: di_buf->di_buf_dup_p[0];
					} else if (skip_top_bot == 2) {
						di_buf_i = (frame_type ==
						VIDTYPE_INTERLACE_BOTTOM)
						? di_buf->di_buf_dup_p[1]
						: di_buf->di_buf_dup_p[0];
					}
				}

				memcpy(di_buf->vframe,
					di_buf_i->vframe,
					sizeof(vframe_t));
				di_buf->vframe->width = di_pre_stru.width_bk;
				di_buf->vframe->private_data = di_buf;

				if (ready_di_buf->new_format_flag &&
				(ready_di_buf->type == VFRAME_TYPE_IN)) {
					pr_info("DI:%d disable post.\n",
						__LINE__);
					di_buf->vframe->early_process_fun
						= de_post_disable_fun;
				} else {
					if (ready_di_buf->type ==
						VFRAME_TYPE_IN)
						di_buf->vframe->
						early_process_fun
						 = do_nothing_fun;

					else
						di_buf->vframe->
						early_process_fun
						 = do_pre_only_fun;
				}
				if (ready_di_buf->post_proc_flag == -2) {
					di_buf->vframe->type
						|= VIDTYPE_VIU_FIELD;
					di_buf->vframe->type
						&= ~(VIDTYPE_TYPEMASK);
					di_buf->vframe->process_fun
= (post_wr_en && post_wr_support)?NULL:de_post_process;
					di_buf->process_fun_index
						= PROCESS_FUN_DI;
					di_buf->pd_config.global_mode
						= PULL_DOWN_EI;
				} else {
					di_buf->vframe->process_fun =
						NULL;
					di_buf->process_fun_index =
						PROCESS_FUN_NULL;
					di_buf->pd_config.global_mode =
						PULL_DOWN_NORMAL;
				}
				di_buf->di_buf[0] = ready_di_buf;
				di_buf->di_buf[1] = NULL;
				queue_out(ready_di_buf);

#ifdef DET3D
				if (dual_vframe_flag) {
					di_buf->di_buf[1] =
						di_buf->di_buf_dup_p[1];
					queue_out(di_buf->di_buf[1]);
				}
#endif
				drop_frame(check_drop,
					di_buf->di_buf[0]->throw_flag, di_buf);

				frame_count++;
#ifdef DI_BUFFER_DEBUG
				di_print(
					"%s <prog by frame>: ",
					__func__);
#endif
				ret = 1;
				vf_notify_receiver(VFM_NAME,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
			}
		} else if (ready_count >= 2) {
			/*for progressive input,type
			 * 1:separate tow fields,type
			 * 2:bypass post as frame
			 */
			unsigned char prog_tb_field_proc_type =
				(prog_proc_config >> 1) & 0x3;
			di_lock_irqfiq_save(irq_flag2);
			di_buf = get_di_buf_head(QUEUE_POST_FREE);

			if (check_di_buf(di_buf, 20)) {
				di_unlock_irqfiq_restore(irq_flag2);
				return 0;
			}

			queue_out(di_buf);
			di_unlock_irqfiq_restore(irq_flag2);

			i = 0;
			queue_for_each_entry(
				p, ptmp, QUEUE_PRE_READY, list) {
				di_buf->di_buf_dup_p[i++] = p;
				if (i >= 2) {
					di_buf->di_buf_dup_p[i] = NULL;
					break;
				}
			}
			if (i < 2) {
				pr_dbg("%s:Error6\n", __func__);

				if (recovery_flag == 0)
					recovery_log_reason = 21;

				recovery_flag++;
				return 0;
			}

			memcpy(di_buf->vframe,
				di_buf->di_buf_dup_p[0]->vframe,
				sizeof(vframe_t));
			di_buf->vframe->private_data = di_buf;

			/*separate one progressive frame
			 * as two interlace fields
			 */
			if (prog_tb_field_proc_type == 1) {
				/* do weave by di post */
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD |
					VIDTYPE_PRE_INTERLACE;
				if (
					di_buf->di_buf_dup_p[0]->
					new_format_flag)
					di_buf->vframe->
					early_process_fun =
						de_post_disable_fun;
				else
					di_buf->vframe->
					early_process_fun =
						do_nothing_fun;

				di_buf->pd_config.global_mode =
					PULL_DOWN_BUF1;
				di_buf->vframe->process_fun =
(post_wr_en && post_wr_support)?NULL:de_post_process;
				di_buf->process_fun_index =
					PROCESS_FUN_DI;
			} else if (prog_tb_field_proc_type == 0) {
				/* to do: need change for
				 * DI_USE_FIXED_CANVAS_IDX
				 */
				/* do weave by vpp */
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE;
				if (
					(di_buf->di_buf_dup_p[0]->
					 new_format_flag) ||
					(Rd(DI_IF1_GEN_REG) & 1))
					di_buf->vframe->
					early_process_fun =
						de_post_disable_fun;
				else
					di_buf->vframe->
					early_process_fun =
						do_nothing_fun;
				di_buf->vframe->process_fun = NULL;
				di_buf->process_fun_index =
					PROCESS_FUN_NULL;
				di_buf->vframe->canvas0Addr =
					di_buf->di_buf_dup_p[0]->
					nr_canvas_idx;
				di_buf->vframe->canvas1Addr =
					di_buf->di_buf_dup_p[1]->
					nr_canvas_idx;
			} else {
				/* to do: need change for
				 * DI_USE_FIXED_CANVAS_IDX
				 */
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD |
					VIDTYPE_PRE_INTERLACE;
				di_buf->vframe->height >>= 1;
				di_buf->vframe->width = di_pre_stru.width_bk;
				if (
					(di_buf->di_buf_dup_p[0]->
					 new_format_flag) ||
					(Rd(DI_IF1_GEN_REG) & 1))
					di_buf->vframe->
					early_process_fun =
						de_post_disable_fun;
				else
					di_buf->vframe->
					early_process_fun =
						do_nothing_fun;
				if (prog_tb_field_proc_type == 2) {
					di_buf->vframe->canvas0Addr =
						di_buf->di_buf_dup_p[0]
						->nr_canvas_idx;
/* top */
					di_buf->vframe->canvas1Addr =
						di_buf->di_buf_dup_p[0]
						->nr_canvas_idx;
				} else {
					di_buf->vframe->canvas0Addr =
						di_buf->di_buf_dup_p[1]
						->nr_canvas_idx; /* top */
					di_buf->vframe->canvas1Addr =
						di_buf->di_buf_dup_p[1]
						->nr_canvas_idx;
				}
			}

			di_buf->di_buf[0] = di_buf->di_buf_dup_p[0];
			queue_out(di_buf->di_buf[0]);
			/*check if the field is error,then drop*/
			if (
				(di_buf->di_buf_dup_p[0]->vframe->type &
				 VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_BOTTOM) {
				di_buf->di_buf[1] =
					di_buf->di_buf_dup_p[1] = NULL;
				queue_in(di_buf, QUEUE_TMP);
				recycle_vframe_type_post(di_buf);
				pr_dbg("%s drop field %d.\n", __func__,
					di_buf->di_buf_dup_p[0]->seq);
			} else {
				di_buf->di_buf[1] =
					di_buf->di_buf_dup_p[1];
				queue_out(di_buf->di_buf[1]);

				drop_frame(check_start_drop_prog,
					(di_buf->di_buf_dup_p[0]->throw_flag) ||
					(di_buf->di_buf_dup_p[1]->throw_flag),
					di_buf);
			}
			frame_count++;
#ifdef DI_BUFFER_DEBUG
			di_print("%s <prog by field>: ", __func__);
#endif
			ret = 1;
			vf_notify_receiver(VFM_NAME,
				VFRAME_EVENT_PROVIDER_VFRAME_READY,
				NULL);
		}
	}

#ifdef DI_BUFFER_DEBUG
	if (di_buf) {
		di_print("%s[%d](",
			vframe_type_name[di_buf->type], di_buf->index);
		for (i = 0; i < 2; i++) {
			if (di_buf->di_buf[i])
				di_print("%s[%d],",
				 vframe_type_name[di_buf->di_buf[i]->type],
				 di_buf->di_buf[i]->index);
		}
		di_print(")(vframe type %x dur %d)",
			di_buf->vframe->type, di_buf->vframe->duration);
		if (di_buf->di_buf_dup_p[1] &&
		    (di_buf->di_buf_dup_p[1]->post_proc_flag == 3))
			di_print("=> recycle_list\n");
		else
			di_print("=> post_ready_list\n");
	}
#endif
	return ret;
}

/*
 * di task
 */
static void di_unreg_process(void)
{
	unsigned long start_jiffes = 0;
	if (reg_flag) {
		pr_dbg("%s unreg start %d.\n", __func__, reg_flag);
		start_jiffes = jiffies_64;
		vf_unreg_provider(&di_vf_prov);
		pr_dbg("%s vf unreg cost %u ms.\n", __func__,
			jiffies_to_msecs(jiffies_64 - start_jiffes));
		unreg_cnt++;
		if (unreg_cnt > 0x3fffffff)
			unreg_cnt = 0;
		pr_dbg("%s unreg stop %d.\n", __func__, reg_flag);
		di_pre_stru.unreg_req_flag_irq = 1;
		reg_flag = 0;
		trigger_pre_di_process(TRIGGER_PRE_BY_UNREG);
	} else {
		di_pre_stru.force_unreg_req_flag = 0;
		di_pre_stru.disable_req_flag = 0;
		recovery_flag = 0;
		di_pre_stru.unreg_req_flag = 0;
		trigger_pre_di_process(TRIGGER_PRE_BY_UNREG);
	}
}

static void di_unreg_process_irq(void)
{
	ulong irq_flag2 = 0;
	unsigned int mirror_disable = 0;
#if (defined ENABLE_SPIN_LOCK_ALWAYS)
	ulong flags = 0;
	spin_lock_irqsave(&plist_lock, flags);
#endif
	init_flag = 0;
	mirror_disable = get_blackout_policy();
	di_lock_irqfiq_save(irq_flag2);
	di_print("%s: di_uninit_buf\n", __func__);
	di_uninit_buf(mirror_disable);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (di_pre_rdma_enable)
		rdma_clear(de_devp->rdma_handle);
#endif
	adpative_combing_exit();
	enable_di_pre_mif(false, mcpre_en);
	afbc_reg_sw(false);
	di_hw_uninit();
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu()
		|| is_meson_g12a_cpu() || is_meson_g12b_cpu()
		|| is_meson_tl1_cpu() || is_meson_sm1_cpu()) {
		di_pre_gate_control(false, mcpre_en);
		nr_gate_control(false);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		DI_Wr(DI_CLKG_CTRL, 0x80f60000);
		DI_Wr(DI_PRE_CTRL, 0);
	} else
		DI_Wr(DI_CLKG_CTRL, 0xf60000);
/* nr/blend0/ei0/mtn0 clock gate */
	if (mirror_disable) {
		di_hw_disable(mcpre_en);
		if (is_meson_txlx_cpu() || is_meson_txhd_cpu()
			|| is_meson_g12a_cpu() || is_meson_g12b_cpu()
			|| is_meson_tl1_cpu() || is_meson_sm1_cpu()) {
			enable_di_post_mif(GATE_OFF);
			di_post_gate_control(false);
			di_top_gate_control(false, false);
		} else {
			DI_Wr(DI_CLKG_CTRL, 0x80000000);
		}
		if (!is_meson_gxl_cpu() && !is_meson_gxm_cpu() &&
			!is_meson_gxbb_cpu() && !is_meson_txlx_cpu())
			switch_vpu_clk_gate_vmod(VPU_VPU_CLKB,
				VPU_CLK_GATE_OFF);
		pr_info("%s disable di mirror image.\n", __func__);
	}
	if (post_wr_en && post_wr_support)
		diwr_set_power_control(0);
	di_unlock_irqfiq_restore(irq_flag2);

#if (defined ENABLE_SPIN_LOCK_ALWAYS)
	spin_unlock_irqrestore(&plist_lock, flags);
#endif
	di_patch_post_update_mc_sw(DI_MC_SW_REG, false);
	di_pre_stru.force_unreg_req_flag = 0;
	di_pre_stru.disable_req_flag = 0;
	recovery_flag = 0;
	di_pre_stru.cur_prog_flag = 0;
	di_pre_stru.unreg_req_flag = 0;
	di_pre_stru.unreg_req_flag_irq = 0;
#ifdef CONFIG_CMA
	if (de_devp->flag_cma == 1) {
		pr_dbg("%s:cma release req time: %d ms\n",
			__func__, jiffies_to_msecs(jiffies));
		di_pre_stru.cma_release_req = 1;
		up(&di_sema);
	}
#endif
}

static void di_reg_process(void)
{
/*get vout information first time*/
	if (reg_flag == 1)
		return;
	vf_reg_provider(&di_vf_prov);
	vf_notify_receiver(VFM_NAME, VFRAME_EVENT_PROVIDER_START, NULL);
	reg_flag = 1;
	di_pre_stru.bypass_flag = false;
	reg_cnt++;
	if (reg_cnt > 0x3fffffff)
		reg_cnt = 0;
	di_print("########%s\n", __func__);
}
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
/* di pre rdma operation */
static void di_rdma_irq(void *arg)
{
	struct di_dev_s *di_devp = (struct di_dev_s *)arg;

	if (IS_ERR_OR_NULL(di_devp))
		return;
	if (di_devp->rdma_handle <= 0) {
		pr_err("%s rdma handle %d error.\n", __func__,
			di_devp->rdma_handle);
		return;
	}
	if (di_printk_flag)
		pr_dbg("%s...%d.\n", __func__,
			di_pre_stru.field_count_for_cont);
}

static struct rdma_op_s di_rdma_op = {
	di_rdma_irq,
	NULL
};
#endif

static void di_load_pq_table(void)
{
	struct di_pq_parm_s *pos = NULL, *tmp = NULL;

	if (atomic_read(&de_devp->pq_flag) == 0 &&
		(de_devp->flags & DI_LOAD_REG_FLAG)) {
		atomic_set(&de_devp->pq_flag, 1);
		list_for_each_entry_safe(pos, tmp,
			&de_devp->pq_table_list, list) {
			di_load_regs(pos);
			list_del(&pos->list);
			di_pq_parm_destroy(pos);
		}
		de_devp->flags &= ~DI_LOAD_REG_FLAG;
		atomic_set(&de_devp->pq_flag, 0);
	}
}

static void di_pre_size_change(unsigned short width,
	unsigned short height, unsigned short vf_type)
{
	unsigned int blkhsize = 0;
	int pps_w = 0, pps_h = 0;

	nr_all_config(width, height, vf_type);
	#ifdef DET3D
	det3d_config(det3d_en ? 1 : 0);
	#endif
	if (pulldown_enable) {
		pulldown_init(width, height);
		init_field_mode(height);

		if (is_meson_txl_cpu() ||
			is_meson_txlx_cpu() ||
			is_meson_gxlx_cpu() ||
			is_meson_txhd_cpu() ||
			is_meson_g12a_cpu() ||
			is_meson_g12b_cpu() ||
			is_meson_tl1_cpu() ||
			is_meson_sm1_cpu())
			film_mode_win_config(width, height);
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		combing_pd22_window_config(width, height);
	RDMA_WR(DI_PRE_SIZE, (width - 1) |
		((height - 1) << 16));

	if (mcpre_en) {
		blkhsize = (width + 4) / 5;
		RDMA_WR(MCDI_HV_SIZEIN, height
			| (width << 16));
		RDMA_WR(MCDI_HV_BLKSIZEIN, (overturn ? 3 : 0) << 30
			| blkhsize << 16 | height);
		RDMA_WR(MCDI_BLKTOTAL, blkhsize * height);
		if (is_meson_gxlx_cpu()) {
			RDMA_WR(MCDI_PD_22_CHK_FLG_CNT, 0);
			RDMA_WR(MCDI_FIELD_MV, 0);
		}
	}

	di_load_pq_table();

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		RDMA_WR(DI_PRE_GL_CTRL, 0x80000005);
	if (de_devp->nrds_enable)
		nr_ds_init(width, height);
	if (de_devp->pps_enable && pps_position) {
		pps_w = di_pre_stru.cur_width;
		pps_h = di_pre_stru.cur_height>>1;
		di_pps_config(1, pps_w, pps_h, pps_dstw, (pps_dsth>>1));
	}
	di_interrupt_ctrl(di_pre_stru.madi_enable,
		det3d_en?1:0,
		de_devp->nrds_enable,
		post_wr_en,
		di_pre_stru.mcdi_enable);
}

static bool need_bypass(struct vframe_s *vf)
{
	if (vf->type & VIDTYPE_MVC)
		return true;

	if (vf->source_type == VFRAME_SOURCE_TYPE_PPMGR)
		return true;

	if (vf->type & VIDTYPE_VIU_444)
		return true;

	if (vf->type & VIDTYPE_PIC)
		return true;
#if 0
	if (vf->type & VIDTYPE_COMPRESS)
		return true;
#else
	/*support G12A and TXLX platform*/
	if (vf->type & VIDTYPE_COMPRESS) {
		if (!afbc_is_supported())
			return true;
		if ((vf->compHeight > (default_height + 8))
			|| (vf->compWidth > default_width))
			return true;
	}
#endif
	if ((vf->width > default_width) ||
			(vf->height > (default_height + 8)))
		return true;

	/*true bypass for 720p above*/
	if ((vf->flag & VFRAME_FLAG_GAME_MODE) &&
		(vf->width > 720))
		return true;

	return false;
}

static bool nrds_en;
module_param_named(nrds_en, nrds_en, bool, 0644);

static void di_reg_process_irq(void)
{
	ulong irq_flag2 = 0;
	#ifndef	RUN_DI_PROCESS_IN_IRQ
	ulong flags = 0;
	#endif
	vframe_t *vframe;
	unsigned short nr_height = 0, first_field_type;

	if ((pre_run_flag != DI_RUN_FLAG_RUN) &&
	    (pre_run_flag != DI_RUN_FLAG_STEP))
		return;
	if (pre_run_flag == DI_RUN_FLAG_STEP)
		pre_run_flag = DI_RUN_FLAG_STEP_DONE;


	vframe = vf_peek(VFM_NAME);

	if (vframe) {
		if (need_bypass(vframe) || ((di_debug_flag>>20) & 0x1)) {
			if (!di_pre_stru.bypass_flag) {
				pr_info("DI bypass all %ux%u-0x%x.",
				vframe->width, vframe->height, vframe->type);
			}
			di_pre_stru.bypass_flag = true;
			di_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
			return;
		} else {
			di_pre_stru.bypass_flag = false;
		}
		/* patch for vdin progressive input */
		if ((is_from_vdin(vframe) &&
		    is_progressive(vframe))
			#ifdef DET3D
			|| det3d_en
			#endif
			|| (use_2_interlace_buff & 0x2)
			) {
			use_2_interlace_buff = 1;
			nr_height = vframe->height;
		} else {
			use_2_interlace_buff = 0;
			nr_height = (vframe->height>>1);
		}
		de_devp->nrds_enable = nrds_en;
		de_devp->pps_enable = pps_en;
		switch_vpu_clk_gate_vmod(VPU_VPU_CLKB, VPU_CLK_GATE_ON);
		if (post_wr_en && post_wr_support)
			diwr_set_power_control(1);
		/* up for vpu clkb rate change */
		up(&di_sema);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
			if (!use_2_interlace_buff) {
				di_top_gate_control(true, true);
				di_post_gate_control(true);
				/* freerun for reg configuration */
				enable_di_post_mif(GATE_AUTO);
			} else {
				di_top_gate_control(true, false);
			}
			de_devp->flags |= DI_VPU_CLKB_SET;
			enable_di_pre_mif(false, mcpre_en);
			di_pre_gate_control(true, mcpre_en);
			di_rst_protect(true);/*2019-01-22 by VLSI feng.wang*/
			di_pre_nr_wr_done_sel(true);
			nr_gate_control(true);
		} else {
			/* if mcdi enable DI_CLKG_CTRL should be 0xfef60000 */
			DI_Wr(DI_CLKG_CTRL, 0xfef60001);
			/* nr/blend0/ei0/mtn0 clock gate */
		}
		if (di_printk_flag & 2)
			di_printk_flag = 1;

		di_print("%s: vframe come => di_init_buf\n", __func__);

		if (is_progressive(vframe) && (prog_proc_config & 0x10)) {
#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
			spin_lock_irqsave(&plist_lock, flags);
#endif
			di_lock_irqfiq_save(irq_flag2);
			/*
			 * 10 bit mode need 1.5 times buffer size of
			 * 8 bit mode, init the buffer size as 10 bit
			 * mode size, to make sure can switch bit mode
			 * smoothly.
			 */
			di_init_buf(default_width, default_height, 1);

			di_unlock_irqfiq_restore(irq_flag2);

#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
			spin_unlock_irqrestore(&plist_lock, flags);
			nr_height = vframe->height;
#endif
		} else {
#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
			spin_lock_irqsave(&plist_lock, flags);
#endif
			di_lock_irqfiq_save(irq_flag2);
			/*
			 * 10 bit mode need 1.5 times buffer size of
			 * 8 bit mode, init the buffer size as 10 bit
			 * mode size, to make sure can switch bit mode
			 * smoothly.
			 */
			di_init_buf(default_width, default_height, 0);

			di_unlock_irqfiq_restore(irq_flag2);

#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
			spin_unlock_irqrestore(&plist_lock, flags);
#endif
		}

		calc_lmv_init();
		first_field_type = (vframe->type & VIDTYPE_TYPEMASK);
		di_pre_size_change(vframe->width, nr_height,
				first_field_type);

		di_pre_stru.mtn_status =
			adpative_combing_config(vframe->width,
					(vframe->height>>1),
					(vframe->source_type),
					is_progressive(vframe),
					vframe->sig_fmt);

		di_patch_post_update_mc_sw(DI_MC_SW_REG, true);
		cue_int();
		if (de_devp->flags & DI_LOAD_REG_FLAG)
			up(&di_sema);
		init_flag = 1;
		di_pre_stru.reg_req_flag_irq = 1;
	}
}

static void di_process(void)
{
	ulong irq_flag2 = 0;
	#ifndef	RUN_DI_PROCESS_IN_IRQ
	ulong flags = 0;
	#endif

	if (init_flag && (recovery_flag == 0) &&
		(dump_state_flag == 0)) {
#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
		spin_lock_irqsave(&plist_lock, flags);
#endif
		if (di_pre_stru.pre_de_busy == 0) {
			if (di_pre_stru.pre_de_process_done) {
#if 0/*def CHECK_DI_DONE*/
				/* also for NEW_DI ? 7/15/2013 */
				unsigned int data32 = Rd(DI_INTR_CTRL);
				/*DI_INTR_CTRL[bit 0], NRWR_done, set by
				 * hardware when NRWR is done,clear by write 1
				 * by code;[bit 1]
				 * MTNWR_done, set by hardware when MTNWR
				 * is done, clear by write 1 by code;these two
				 * bits have nothing to do with
				 * DI_INTR_CTRL[16](NRW irq mask, 0 to enable
				 * irq) and DI_INTR_CTRL[17]
				 * (MTN irq mask, 0 to enable irq).two
				 * interrupts are raised if both
				 * DI_INTR_CTRL[16] and DI_INTR_CTRL[17] are 0
				 */
				if (
					((data32 & 0x1) &&
					 ((di_pre_stru.enable_mtnwr == 0)
					  || (data32 &
					      0x2))) ||
					(di_pre_stru.pre_de_clear_flag == 2)) {
					RDMA_WR(DI_INTR_CTRL, data32);
#endif
				pre_process_time =
					di_pre_stru.pre_de_busy_timer_count;
				pre_de_done_buf_config();

				di_pre_stru.pre_de_process_done = 0;
				di_pre_stru.pre_de_clear_flag = 0;
#ifdef CHECK_DI_DONE
			}
#endif
			} else if (di_pre_stru.pre_de_clear_flag == 1) {
				di_lock_irqfiq_save(
					irq_flag2);
				pre_de_done_buf_clear();
				di_unlock_irqfiq_restore(
					irq_flag2);
				di_pre_stru.pre_de_process_done = 0;
				di_pre_stru.pre_de_clear_flag = 0;
			}
		}

		di_lock_irqfiq_save(irq_flag2);
		di_post_stru.check_recycle_buf_cnt = 0;
		while (check_recycle_buf() & 1) {
			if (di_post_stru.check_recycle_buf_cnt++ >
				MAX_IN_BUF_NUM) {
				di_pr_info("%s: check_recycle_buf time out!!\n",
					__func__);
				break;
			}
		}
		di_unlock_irqfiq_restore(irq_flag2);
		if ((di_pre_stru.pre_de_busy == 0) &&
			(di_pre_stru.pre_de_process_done == 0)) {
			if ((pre_run_flag == DI_RUN_FLAG_RUN) ||
				(pre_run_flag == DI_RUN_FLAG_STEP)) {
				if (pre_run_flag == DI_RUN_FLAG_STEP)
					pre_run_flag = DI_RUN_FLAG_STEP_DONE;
				if (pre_de_buf_config() &&
					(di_pre_stru.pre_de_process_flag == 0))
					pre_de_process();
			}
		}
		di_post_stru.di_post_process_cnt = 0;
		while (process_post_vframe()) {
			if (di_post_stru.di_post_process_cnt++ >
				MAX_POST_BUF_NUM) {
				di_pr_info("%s: process_post_vframe time out!!\n",
					__func__);
				break;
			}
		}
		if (post_wr_en && post_wr_support) {
			if (di_post_stru.post_de_busy == 0 &&
			di_post_stru.de_post_process_done) {
				post_de_done_buf_config();
				di_post_stru.de_post_process_done = 0;
			}
			di_post_process();
		}

#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
		spin_unlock_irqrestore(&plist_lock, flags);
#endif
	}
}
static unsigned int nr_done_check_cnt = 5;
static void di_pre_trigger_work(struct di_pre_stru_s *pre_stru_p)
{

	if (pre_stru_p->pre_de_busy && init_flag) {
		pre_stru_p->pre_de_busy_timer_count++;
		if (pre_stru_p->pre_de_busy_timer_count >= nr_done_check_cnt &&
		((cur_to_msecs() - di_pre_stru.irq_time[1]) >
		(10*nr_done_check_cnt))) {
			if (di_dbg_mask & 4) {
				dump_mif_size_state(&di_pre_stru,
					&di_post_stru);
			}
			enable_di_pre_mif(false, mcpre_en);
			if (de_devp->nrds_enable)
				nr_ds_hw_ctrl(false);
			pre_stru_p->pre_de_busy_timer_count = 0;
			pre_stru_p->pre_de_irq_timeout_count++;
			pre_stru_p->pre_de_process_done = 1;
			pre_stru_p->pre_de_busy = 0;
			pre_stru_p->pre_de_clear_flag = 2;
			if ((pre_stru_p->field_count_for_cont < 10) ||
				(di_dbg_mask&0x2)) {
				pr_info("DI*****wait %d timeout 0x%x(%d ms)*****\n",
					pre_stru_p->field_count_for_cont,
					Rd(DI_INTR_CTRL),
					(unsigned int)(cur_to_msecs() -
					di_pre_stru.irq_time[1]));
			}
		}
	} else {
		pre_stru_p->pre_de_busy_timer_count = 0;
	}

	/* if(force_trig){ */
	trigger_pre_di_process(TRIGGER_PRE_BY_TIMER);
	/* } */

	if (force_recovery) {
		if (recovery_flag || (force_recovery & 0x2)) {
			force_recovery_count++;
			if (init_flag) {
				pr_err("====== DI force recovery %u-%u =========\n",
					recovery_log_reason,
					recovery_log_queue_idx);
				print_di_buf(recovery_log_di_buf, 2);
				force_recovery &= (~0x2);
				recovery_flag = 0;
			}
		}
	}
}

static int di_task_handle(void *data)
{
	struct di_dev_s *devp;
	int ret = 0;
	devp = (struct di_dev_s *)data;
	if (!devp)
		return -1;
	while (1) {
		ret = down_interruptible(&di_sema);
		if (ret != 0)
			continue;
		if (active_flag) {
			if ((di_pre_stru.unreg_req_flag ||
				di_pre_stru.force_unreg_req_flag ||
				di_pre_stru.disable_req_flag) &&
				(di_pre_stru.pre_de_busy == 0)) {
				di_unreg_process();
				/* set min rate for power saving */
				if (de_devp->vpu_clkb) {
					clk_set_rate(de_devp->vpu_clkb,
						de_devp->clkb_min_rate);
				}
			}
			if (di_pre_stru.reg_req_flag_irq ||
				di_pre_stru.reg_req_flag) {
				di_reg_process();
				di_pre_stru.reg_req_flag = 0;
				di_pre_stru.reg_req_flag_irq = 0;
			}
			#ifdef CONFIG_CMA
			/* mutex_lock(&de_devp->cma_mutex);*/
			if (di_pre_stru.cma_release_req) {
				atomic_set(&devp->mem_flag, 0);
				di_cma_release(devp);
				di_pre_stru.cma_release_req = 0;
				di_pre_stru.cma_alloc_done = 0;
			}
			if (di_pre_stru.cma_alloc_req) {
				if (di_cma_alloc(devp))
					atomic_set(&devp->mem_flag, 1);
				else
					atomic_set(&devp->mem_flag, 0);
				di_pre_stru.cma_alloc_req = 0;
				di_pre_stru.cma_alloc_done = 1;
			}
			/* mutex_unlock(&de_devp->cma_mutex); */
			#endif
		}
		if (de_devp->flags & DI_VPU_CLKB_SET) {
			if (is_meson_txlx_cpu()) {
				if (!use_2_interlace_buff) {
					#ifdef CLK_TREE_SUPPORT
					clk_set_rate(de_devp->vpu_clkb,
						de_devp->clkb_min_rate);
					#endif
				} else {
					#ifdef CLK_TREE_SUPPORT
					clk_set_rate(de_devp->vpu_clkb,
						de_devp->clkb_max_rate);
					#endif
				}
			}
			if (is_meson_g12a_cpu() || is_meson_g12b_cpu()
				|| is_meson_tl1_cpu() ||
				is_meson_sm1_cpu()) {
				#ifdef CLK_TREE_SUPPORT
				clk_set_rate(de_devp->vpu_clkb,
						de_devp->clkb_max_rate);
				#endif
			}
			de_devp->flags &= (~DI_VPU_CLKB_SET);
		}
	}

	return 0;
}

static void di_pre_process_irq(struct di_pre_stru_s *pre_stru_p)
{
	int i;

	if (active_flag) {
		/* must wait pre de done or time out to clear the de_busy
		 * otherwise may appear watch dog reboot probablity
		 * caused by disable mif in unreg_process_irq
		 */
		if (pre_stru_p->unreg_req_flag_irq &&
			(di_pre_stru.pre_de_busy == 0))
			di_unreg_process_irq();
		if (init_flag == 0 && pre_stru_p->reg_req_flag_irq == 0)
			di_reg_process_irq();
	}

	for (i = 0; i < 2; i++) {
		if (active_flag)
			di_process();
	}
	log_buffer_state("pro");
}
static struct hrtimer di_pre_hrtimer;

static void pre_tasklet(unsigned long arg)
{
	unsigned int hrtimer_time = 0;

	hrtimer_time = jiffies_to_msecs(jiffies_64 - de_devp->jiffy);
	if (hrtimer_time > 10)
		pr_dbg("DI: tasklet schedule cost %ums.\n", hrtimer_time);
	di_pre_process_irq((struct di_pre_stru_s *)arg);
}

static enum hrtimer_restart di_pre_hrtimer_func(struct hrtimer *timer)
{
	if (!di_pre_stru.bypass_flag)
		di_pre_trigger_work(&di_pre_stru);
	hrtimer_forward_now(&di_pre_hrtimer, ms_to_ktime(10));
	di_patch_post_update_mc();
	return HRTIMER_RESTART;
}
/*
 * provider/receiver interface
 */
char *vf_get_receiver_name(const char *provider_name);
static int di_receiver_event_fun(int type, void *data, void *arg)
{
	int i;
	ulong flags;

	if (type == VFRAME_EVENT_PROVIDER_QUREY_VDIN2NR) {
		return di_pre_stru.vdin2nr;
	} else if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		pr_dbg("%s , is_bypass() %d trick_mode %d bypass_all %d\n",
			__func__, is_bypass(NULL), trick_mode, bypass_all);
		pr_info("%s: vf_notify_receiver unreg\n", __func__);

		di_pre_stru.unreg_req_flag = 1;
		di_pre_stru.vdin_source = false;
		trigger_pre_di_process(TRIGGER_PRE_BY_PROVERDER_UNREG);
		di_pre_stru.unreg_req_flag_cnt = 0;
		while (di_pre_stru.unreg_req_flag) {
			usleep_range(10000, 10001);
			if (di_pre_stru.unreg_req_flag_cnt++ >
				di_reg_unreg_cnt) {
				reg_unreg_timeout_cnt++;
				pr_err("%s:unreg_reg_flag timeout!!!\n",
					__func__);
				di_unreg_process();
				break;
			}
		}
#ifdef SUPPORT_MPEG_TO_VDIN
		if (mpeg2vdin_flag) {
			struct vdin_arg_s vdin_arg;
			struct vdin_v4l2_ops_s *vdin_ops = get_vdin_v4l2_ops();

			vdin_arg.cmd = VDIN_CMD_MPEGIN_STOP;
			if (vdin_ops->tvin_vdin_func)
				vdin_ops->tvin_vdin_func(0, &vdin_arg);

			mpeg2vdin_flag = 0;
		}
#endif
		bypass_state = 1;
#ifdef RUN_DI_PROCESS_IN_IRQ
		if (di_pre_stru.vdin_source)
			DI_Wr_reg_bits(VDIN_WR_CTRL, 0x3, 24, 3);
#endif
	} else if (type == VFRAME_EVENT_PROVIDER_RESET) {
		di_blocking = 1;

		pr_dbg("%s: VFRAME_EVENT_PROVIDER_RESET\n", __func__);
		if (is_bypass(NULL)
			|| bypass_state
			|| di_pre_stru.bypass_flag) {
			vf_notify_receiver(VFM_NAME,
				VFRAME_EVENT_PROVIDER_RESET,
				NULL);
		}

		goto light_unreg;
	} else if (type == VFRAME_EVENT_PROVIDER_LIGHT_UNREG) {
		di_blocking = 1;

		pr_dbg("%s: vf_notify_receiver ligth unreg\n", __func__);

light_unreg:
		spin_lock_irqsave(&plist_lock, flags);
		for (i = 0; i < MAX_IN_BUF_NUM; i++) {

			if (vframe_in[i])
				pr_dbg("DI:clear vframe_in[%d]\n", i);

			vframe_in[i] = NULL;
		}
		spin_unlock_irqrestore(&plist_lock, flags);
		di_blocking = 0;
	} else if (type == VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME) {
		unsigned char vf_put_flag = 0;

		pr_dbg(
			"%s:VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME\n",
			__func__);
/*
 * do not display garbage when 2d->3d or 3d->2d
 */
		spin_lock_irqsave(&plist_lock, flags);
		for (i = 0; i < MAX_IN_BUF_NUM; i++) {
			if (vframe_in[i]) {
				vf_put(vframe_in[i], VFM_NAME);
				pr_dbg("DI:clear vframe_in[%d]\n", i);
				vf_put_flag = 1;
			}
			vframe_in[i] = NULL;
		}
		if (vf_put_flag)
			vf_notify_provider(VFM_NAME,
				VFRAME_EVENT_RECEIVER_PUT, NULL);

		spin_unlock_irqrestore(&plist_lock, flags);
	} else if (type == VFRAME_EVENT_PROVIDER_VFRAME_READY) {
		if (di_pre_stru.bypass_flag)
			vf_notify_receiver(VFM_NAME,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
		trigger_pre_di_process(TRIGGER_PRE_BY_VFRAME_READY);

#ifdef RUN_DI_PROCESS_IN_IRQ
#define INPUT2PRE_2_BYPASS_SKIP_COUNT   4
		if (active_flag && di_pre_stru.vdin_source) {
			if (is_bypass(NULL)) {
				if (di_pre_stru.pre_de_busy == 0) {
					DI_Wr_reg_bits(VDIN_WR_CTRL,
						0x3, 24, 3);
					di_pre_stru.vdin2nr = 0;
				}
				if (di_pre_stru.bypass_start_count <
				    INPUT2PRE_2_BYPASS_SKIP_COUNT) {
					vframe_t *vframe_tmp = vf_get(VFM_NAME);

					if (vframe_tmp != NULL) {
						vf_put(vframe_tmp, VFM_NAME);
						vf_notify_provider(VFM_NAME,
						VFRAME_EVENT_RECEIVER_PUT,
						NULL);
					}
					di_pre_stru.bypass_start_count++;
				}
			} else if (is_input2pre()) {
				di_pre_stru.bypass_start_count = 0;
				if ((di_pre_stru.pre_de_busy != 0) &&
				    (input2pre_miss_policy == 1 &&
				     frame_count < 30)) {
					di_pre_stru.pre_de_clear_flag = 1;
					di_pre_stru.pre_de_busy = 0;
					input2pre_buf_miss_count++;
				}

				if (di_pre_stru.pre_de_busy == 0) {
					DI_Wr_reg_bits(VDIN_WR_CTRL,
						0x5, 24, 3);
					di_pre_stru.vdin2nr = 1;
					di_process();
					log_buffer_state("pr_");
					if (di_pre_stru.pre_de_busy == 0)
						input2pre_proc_miss_count++;
				} else {
					vframe_t *vframe_tmp = vf_get(VFM_NAME);

					if (vframe_tmp != NULL) {
						vf_put(vframe_tmp, VFM_NAME);
						vf_notify_provider(VFM_NAME,
						VFRAME_EVENT_RECEIVER_PUT,
						NULL);
					}
					input2pre_buf_miss_count++;
					if ((di_pre_stru.cur_width > 720 &&
					     di_pre_stru.cur_height > 576) ||
					    (input2pre_throw_count & 0x10000))
						di_pre_stru.pre_throw_flag =
							input2pre_throw_count &
							0xffff;
				}
			} else {
				if (di_pre_stru.pre_de_busy == 0) {
					DI_Wr_reg_bits(VDIN_WR_CTRL,
						0x3, 24, 3);
					di_pre_stru.vdin2nr = 0;
				}
				di_pre_stru.bypass_start_count =
					INPUT2PRE_2_BYPASS_SKIP_COUNT;
			}
		}
#endif
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		/*int in_buf_num = 0;*/
		struct vframe_states states;

		if (recovery_flag)
			return RECEIVER_INACTIVE;
#if 1/*fix for ucode reset method be break by di.20151230*/
		di_vf_states(&states, NULL);
		if (states.buf_avail_num > 0)
			return RECEIVER_ACTIVE;

		if (vf_notify_receiver(
			VFM_NAME,
			VFRAME_EVENT_PROVIDER_QUREY_STATE,
			NULL) == RECEIVER_ACTIVE)
			return RECEIVER_ACTIVE;
		return RECEIVER_INACTIVE;

#else
		for (i = 0; i < MAX_IN_BUF_NUM; i++) {
			if (vframe_in[i] != NULL)
				in_buf_num++;
		if (bypass_state == 1) {
			if (in_buf_num > 1)
				return RECEIVER_ACTIVE;
			else
				return RECEIVER_INACTIVE;
		} else {
			if (in_buf_num > 0)
				return RECEIVER_ACTIVE;
			else
				return RECEIVER_INACTIVE;
		}
#endif
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		char *provider_name = (char *)data;
		char *receiver_name = NULL;
		if (de_devp->flags & DI_SUSPEND_FLAG) {
			pr_err("[DI] reg event device hasn't resumed\n");
			return -1;
		}
		if (reg_flag) {
			pr_err("[DI] no muti instance.\n");
			return -1;
		}
		pr_info("%s: vframe provider reg %s\n", __func__,
			provider_name);

		bypass_state = 0;
		di_pre_stru.reg_req_flag = 1;

		trigger_pre_di_process(TRIGGER_PRE_BY_PROVERDER_REG);
		di_pre_stru.reg_req_flag_cnt = 0;
		while (di_pre_stru.reg_req_flag) {
			usleep_range(10000, 10001);
			if (di_pre_stru.reg_req_flag_cnt++ > di_reg_unreg_cnt) {
				reg_unreg_timeout_cnt++;
				pr_dbg("%s:reg_req_flag timeout!!!\n",
					__func__);
				break;
			}
		}

		if (strncmp(provider_name, "vdin0", 4) == 0) {
			di_pre_stru.vdin_source = true;
		} else {
			di_pre_stru.vdin_source = false;
		}
		receiver_name = vf_get_receiver_name(VFM_NAME);
		if (receiver_name) {
			if (!strcmp(receiver_name, "amvideo")) {
				di_post_stru.run_early_proc_fun_flag = 0;
				if (post_wr_en && post_wr_support)
					di_post_stru.run_early_proc_fun_flag
						= 1;
			} else {
				di_post_stru.run_early_proc_fun_flag = 1;
				pr_info("set run_early_proc_fun_flag to 1\n");
			}
		} else {
			pr_info("%s error receiver is null.\n", __func__);
		}
	}
#ifdef DET3D
	else if (type == VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE) {
		int flag = (long)data;

		di_pre_stru.vframe_interleave_flag = flag;
	}
#endif
	else if (type == VFRAME_EVENT_PROVIDER_FR_HINT) {
		vf_notify_receiver(VFM_NAME,
			VFRAME_EVENT_PROVIDER_FR_HINT, data);
	} else if (type == VFRAME_EVENT_PROVIDER_FR_END_HINT) {
		vf_notify_receiver(VFM_NAME,
			VFRAME_EVENT_PROVIDER_FR_END_HINT, data);
	}

	return 0;
}

static void fast_process(void)
{
	int i;
	ulong flags = 0, irq_flag2 = 0;

	if (active_flag && is_bypass(NULL) && (BYPASS_GET_MAX_BUF_NUM <= 1) &&
		init_flag && (recovery_flag == 0) &&
		(dump_state_flag == 0)) {
		if (vf_peek(VFM_NAME) == NULL)
			return;

		for (i = 0; i < 2; i++) {
			spin_lock_irqsave(&plist_lock, flags);
			if (di_pre_stru.pre_de_process_done) {
				pre_de_done_buf_config();
				di_pre_stru.pre_de_process_done = 0;
			}

			di_lock_irqfiq_save(irq_flag2);
			di_post_stru.check_recycle_buf_cnt = 0;
			while (check_recycle_buf() & 1) {
				if (di_post_stru.check_recycle_buf_cnt++ >
					MAX_IN_BUF_NUM) {
					di_pr_info("%s: check_recycle_buf time out!!\n",
						__func__);
					break;
				}
			}
			di_unlock_irqfiq_restore(irq_flag2);

			if ((di_pre_stru.pre_de_busy == 0) &&
			    (di_pre_stru.pre_de_process_done == 0)) {
				if ((pre_run_flag == DI_RUN_FLAG_RUN) ||
				    (pre_run_flag == DI_RUN_FLAG_STEP)) {
					if (pre_run_flag == DI_RUN_FLAG_STEP)
						pre_run_flag =
							DI_RUN_FLAG_STEP_DONE;
					if (pre_de_buf_config() &&
					(di_pre_stru.pre_de_process_flag == 0))
						pre_de_process();
				}
			}
			di_post_stru.di_post_process_cnt = 0;
			while (process_post_vframe()) {
				if (di_post_stru.di_post_process_cnt++ >
					MAX_POST_BUF_NUM) {
					pr_info("%s: process_post_vframe time out!!\n",
						__func__);
					break;
				}
			}

			spin_unlock_irqrestore(&plist_lock, flags);
		}
	}
}

static vframe_t *di_vf_peek(void *arg)
{
	vframe_t *vframe_ret = NULL;
	struct di_buf_s *di_buf = NULL;

	video_peek_cnt++;
	if (di_pre_stru.bypass_flag)
		return vf_peek(VFM_NAME);
	if ((init_flag == 0) || recovery_flag ||
		di_blocking || di_pre_stru.unreg_req_flag || dump_state_flag)
		return NULL;
	if ((run_flag == DI_RUN_FLAG_PAUSE) ||
	    (run_flag == DI_RUN_FLAG_STEP_DONE))
		return NULL;

	log_buffer_state("pek");

	fast_process();
#ifdef SUPPORT_START_FRAME_HOLD
	if ((disp_frame_count == 0) && (is_bypass(NULL) == 0)) {
		int ready_count = list_count(QUEUE_POST_READY);

		if (ready_count > start_frame_hold_count) {
			di_buf = get_di_buf_head(QUEUE_POST_READY);
			if (di_buf)
				vframe_ret = di_buf->vframe;
		}
	} else
#endif
	{
		if (!queue_empty(QUEUE_POST_READY)) {
			di_buf = get_di_buf_head(QUEUE_POST_READY);
			if (di_buf)
				vframe_ret = di_buf->vframe;
		}
	}
#ifdef DI_BUFFER_DEBUG
	if (vframe_ret)
		di_print("%s: %s[%d]:%x\n", __func__,
			vframe_type_name[di_buf->type],
			di_buf->index, vframe_ret);
#endif
	return vframe_ret;
}
/*recycle the buffer for keeping buffer*/
static void recycle_keep_buffer(void)
{
	ulong irq_flag2 = 0;
	int i = 0;
	struct di_buf_s *keep_buf;

	keep_buf = di_post_stru.keep_buf;
	if (!IS_ERR_OR_NULL(keep_buf)) {
		if (keep_buf->type == VFRAME_TYPE_POST) {
			pr_dbg("%s recycle keep cur di_buf %d (",
				__func__, keep_buf->index);
			di_lock_irqfiq_save(irq_flag2);
			for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
				if (keep_buf->di_buf_dup_p[i]) {
					queue_in(
						keep_buf->di_buf_dup_p[i],
						QUEUE_RECYCLE);
					pr_dbg(" %d ",
					keep_buf->di_buf_dup_p[i]->index);
				}
			}
			queue_in(keep_buf, QUEUE_POST_FREE);
			di_unlock_irqfiq_restore(irq_flag2);
			pr_dbg(")\n");
		}
		di_post_stru.keep_buf = NULL;
	}
}
static bool show_nrwr;
static vframe_t *di_vf_get(void *arg)
{
	vframe_t *vframe_ret = NULL;
	struct di_buf_s *di_buf = NULL, *nr_buf = NULL;
	ulong irq_flag2 = 0;

	if (di_pre_stru.bypass_flag)
		return vf_get(VFM_NAME);

	if ((init_flag == 0) || recovery_flag ||
		di_blocking || di_pre_stru.unreg_req_flag || dump_state_flag)
		return NULL;

	if ((run_flag == DI_RUN_FLAG_PAUSE) ||
	    (run_flag == DI_RUN_FLAG_STEP_DONE))
		return NULL;

#ifdef SUPPORT_START_FRAME_HOLD
	if ((disp_frame_count == 0) && (is_bypass(NULL) == 0)) {
		int ready_count = list_count(QUEUE_POST_READY);

		if (ready_count > start_frame_hold_count)
			goto get_vframe;
	} else
#endif
	if (!queue_empty(QUEUE_POST_READY)) {
#ifdef SUPPORT_START_FRAME_HOLD
get_vframe:
#endif
		log_buffer_state("ge_");
		di_lock_irqfiq_save(irq_flag2);

		di_buf = get_di_buf_head(QUEUE_POST_READY);
		if (check_di_buf(di_buf, 21)) {
			di_unlock_irqfiq_restore(irq_flag2);
			return NULL;
		}
		queue_out(di_buf);
		queue_in(di_buf, QUEUE_DISPLAY); /* add it into display_list */

		di_unlock_irqfiq_restore(irq_flag2);

		if (di_buf) {
			vframe_ret = di_buf->vframe;
			nr_buf = di_buf->di_buf_dup_p[1];
			if ((post_wr_en && post_wr_support) &&
			(di_buf->process_fun_index != PROCESS_FUN_NULL)) {
				#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
				vframe_ret->canvas0_config[0].phy_addr =
					di_buf->nr_adr;
				vframe_ret->canvas0_config[0].width =
					di_buf->canvas_width[NR_CANVAS],
				vframe_ret->canvas0_config[0].height =
					di_buf->canvas_height;
				vframe_ret->canvas0_config[0].block_mode = 0;
				vframe_ret->plane_num = 1;
				vframe_ret->canvas0Addr = -1;
				vframe_ret->canvas1Addr = -1;
				if (show_nrwr) {
					vframe_ret->canvas0_config[0].phy_addr =
						nr_buf->nr_adr;
					vframe_ret->canvas0_config[0].width =
						nr_buf->canvas_width[NR_CANVAS];
					vframe_ret->canvas0_config[0].height =
						nr_buf->canvas_height;
				}
				#else
				config_canvas_idx(di_buf, di_wr_idx, -1);
				vframe_ret->canvas0Addr = di_buf->nr_canvas_idx;
				vframe_ret->canvas1Addr = di_buf->nr_canvas_idx;
				if (show_nrwr) {
					config_canvas_idx(nr_buf,
						di_wr_idx, -1);
					vframe_ret->canvas0Addr = di_wr_idx;
					vframe_ret->canvas1Addr = di_wr_idx;
				}
				#endif
				vframe_ret->early_process_fun = do_post_wr_fun;
				vframe_ret->process_fun = NULL;
			}
			di_buf->seq = disp_frame_count;
			atomic_set(&di_buf->di_cnt, 1);
		}
		disp_frame_count++;
		if (run_flag == DI_RUN_FLAG_STEP)
			run_flag = DI_RUN_FLAG_STEP_DONE;

		log_buffer_state("get");
	}
	if (vframe_ret) {
		di_print("%s: %s[%d]:%x %u ms\n", __func__,
		vframe_type_name[di_buf->type], di_buf->index, vframe_ret,
		jiffies_to_msecs(jiffies_64 - vframe_ret->ready_jiffies64));
	}

	if (!post_wr_en && di_post_stru.run_early_proc_fun_flag && vframe_ret) {
		if (vframe_ret->early_process_fun == do_pre_only_fun)
			vframe_ret->early_process_fun(
				vframe_ret->private_data, vframe_ret);
	}
	return vframe_ret;
}

static void di_vf_put(vframe_t *vf, void *arg)
{
	struct di_buf_s *di_buf = NULL;
	ulong irq_flag2 = 0;

	if (di_pre_stru.bypass_flag) {
		vf_put(vf, VFM_NAME);
		vf_notify_provider(VFM_NAME,
			VFRAME_EVENT_RECEIVER_PUT, NULL);
		if (!IS_ERR_OR_NULL(di_post_stru.keep_buf))
			recycle_keep_buffer();
		return;
	}
/* struct di_buf_s *p = NULL; */
/* int itmp = 0; */
	if ((init_flag == 0) || recovery_flag ||
		IS_ERR_OR_NULL(vf)) {
		di_print("%s: 0x%p\n", __func__, vf);
		return;
	}
	if (di_blocking)
		return;
	log_buffer_state("pu_");
	di_buf = (struct di_buf_s *)vf->private_data;
	if (IS_ERR_OR_NULL(di_buf)) {
		pr_info("%s: get vframe %p without di buf\n",
			__func__, vf);
		return;
	}
	if (di_post_stru.keep_buf == di_buf) {
		pr_info("[DI]recycle buffer %d, get cnt %d.\n",
			di_buf->index, disp_frame_count);
		recycle_keep_buffer();
	}

	if (di_buf->type == VFRAME_TYPE_POST) {
		di_lock_irqfiq_save(irq_flag2);

		if (is_in_queue(di_buf, QUEUE_DISPLAY)) {
			if (!atomic_dec_and_test(&di_buf->di_cnt))
				di_print("%s,di_cnt > 0\n", __func__);
			recycle_vframe_type_post(di_buf);
	} else {
			di_print("%s: %s[%d] not in display list\n", __func__,
			vframe_type_name[di_buf->type], di_buf->index);
	}
		di_unlock_irqfiq_restore(irq_flag2);
#ifdef DI_BUFFER_DEBUG
		recycle_vframe_type_post_print(di_buf, __func__, __LINE__);
#endif
	} else {
		di_lock_irqfiq_save(irq_flag2);
		queue_in(di_buf, QUEUE_RECYCLE);
		di_unlock_irqfiq_restore(irq_flag2);

		di_print("%s: %s[%d] =>recycle_list\n", __func__,
			vframe_type_name[di_buf->type], di_buf->index);
	}

	trigger_pre_di_process(TRIGGER_PRE_BY_PUT);
}

static int di_event_cb(int type, void *data, void *private_data)
{
	if (type == VFRAME_EVENT_RECEIVER_FORCE_UNREG) {
		pr_info("%s: RECEIVER_FORCE_UNREG return\n",
			__func__);
		return 0;
	}
	return 0;
}

static int di_vf_states(struct vframe_states *states, void *arg)
{
	if (!states)
		return -1;
	states->vf_pool_size = de_devp->buf_num_avail;
	states->buf_free_num = list_count(QUEUE_LOCAL_FREE);
	states->buf_avail_num = list_count(QUEUE_POST_READY);
	states->buf_recycle_num = list_count(QUEUE_RECYCLE);
	if (di_dbg_mask&0x1) {
		di_pr_info("di-pre-ready-num:%d\n",
			list_count(QUEUE_PRE_READY));
		di_pr_info("di-display-num:%d\n", list_count(QUEUE_DISPLAY));
	}
	return 0;
}

/*****************************
 *	 di driver file_operations
 *
 ******************************/
static int di_open(struct inode *node, struct file *file)
{
	di_dev_t *di_in_devp;

/* Get the per-device structure that contains this cdev */
	di_in_devp = container_of(node->i_cdev, di_dev_t, cdev);
	file->private_data = di_in_devp;

	return 0;
}


static int di_release(struct inode *node, struct file *file)
{
/* di_dev_t *di_in_devp = file->private_data; */

/* Reset file pointer */

/* Release some other fields */
	file->private_data = NULL;
	return 0;
}
static struct di_pq_parm_s *di_pq_parm_create(struct am_pq_parm_s *pq_parm_p)
{
	struct di_pq_parm_s *pq_ptr = NULL;
	struct am_reg_s *am_reg_p = NULL;
	size_t mem_size = 0;

	pq_ptr = vzalloc(sizeof(struct di_pq_parm_s));
	mem_size = sizeof(struct am_pq_parm_s);
	memcpy(&pq_ptr->pq_parm, pq_parm_p, mem_size);
	mem_size = sizeof(struct am_reg_s)*pq_parm_p->table_len;
	am_reg_p = vzalloc(mem_size);
	if (!am_reg_p) {
		vfree(pq_ptr);
		pr_err("[DI] alloc pq table memory errors\n");
		return NULL;
	}
	pq_ptr->regs = am_reg_p;

	return pq_ptr;
}

static void di_pq_parm_destroy(struct di_pq_parm_s *pq_ptr)
{
	if (!pq_ptr) {
		pr_err("[DI] %s pq parm pointer null.\n", __func__);
		return;
	}
	vfree(pq_ptr->regs);
	vfree(pq_ptr);
}
static long di_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0, tab_flag = 0;
	di_dev_t *di_devp;
	void __user *argp = (void __user *)arg;
	size_t mm_size = 0;
	struct am_pq_parm_s tmp_pq_s = {0};
	struct di_pq_parm_s *di_pq_ptr = NULL;

	if (_IOC_TYPE(cmd) != _DI_) {
		pr_err("%s invalid command: %u\n", __func__, cmd);
		return -EFAULT;
	}
	di_devp = file->private_data;
	switch (cmd) {
	case AMDI_IOC_SET_PQ_PARM:
		mm_size = sizeof(struct am_pq_parm_s);
		if (copy_from_user(&tmp_pq_s, argp, mm_size)) {
			pr_err("[DI] set pq parm errors\n");
			return -EFAULT;
		}
		if (tmp_pq_s.table_len >= TABLE_LEN_MAX) {
			pr_err("[DI] load 0x%x wrong pq table_len.\n",
				tmp_pq_s.table_len);
			return -EFAULT;
		}
		tab_flag = TABLE_NAME_DI | TABLE_NAME_NR | TABLE_NAME_MCDI |
			TABLE_NAME_DEBLOCK | TABLE_NAME_DEMOSQUITO;
		if (tmp_pq_s.table_name & tab_flag) {
			pr_info("[DI] load 0x%x pq table len %u %s.\n",
				tmp_pq_s.table_name, tmp_pq_s.table_len,
				init_flag?"directly":"later");
		} else {
			pr_err("[DI] load 0x%x wrong pq table.\n",
				tmp_pq_s.table_name);
			return -EFAULT;
		}
		di_pq_ptr = di_pq_parm_create(&tmp_pq_s);
		if (!di_pq_ptr) {
			pr_err("[DI] allocat pq parm struct error.\n");
			return -EFAULT;
		}
		argp = (void __user *)tmp_pq_s.table_ptr;
		mm_size = tmp_pq_s.table_len * sizeof(struct am_reg_s);
		if (copy_from_user(di_pq_ptr->regs, argp, mm_size)) {
			pr_err("[DI] user copy pq table errors\n");
			return -EFAULT;
		}
		if (init_flag) {
			di_load_regs(di_pq_ptr);
			di_pq_parm_destroy(di_pq_ptr);
			break;
		}
		if (atomic_read(&de_devp->pq_flag) == 0) {
			atomic_set(&de_devp->pq_flag, 1);
			if (di_devp->flags & DI_LOAD_REG_FLAG) {
				struct di_pq_parm_s *pos = NULL, *tmp = NULL;
				list_for_each_entry_safe(pos, tmp,
					&di_devp->pq_table_list, list) {
					if (di_pq_ptr->pq_parm.table_name ==
						pos->pq_parm.table_name) {
						pr_info("[DI] remove 0x%x table.\n",
						pos->pq_parm.table_name);
						list_del(&pos->list);
						di_pq_parm_destroy(pos);
					}
				}
			}
			list_add_tail(&di_pq_ptr->list,
				&di_devp->pq_table_list);
			di_devp->flags |= DI_LOAD_REG_FLAG;
			atomic_set(&de_devp->pq_flag, 0);
		} else {
			pr_err("[DI] please retry table name 0x%x.\n",
			di_pq_ptr->pq_parm.table_name);
			di_pq_parm_destroy(di_pq_ptr);
			ret = -EFAULT;
		}
		break;
	default:
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long di_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = di_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations di_fops = {
	.owner		= THIS_MODULE,
	.open		= di_open,
	.release	= di_release,
	.unlocked_ioctl  = di_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = di_compat_ioctl,
#endif
};

static ssize_t
show_frame_format(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
	int ret = 0;

	if (init_flag)
		ret += sprintf(buf + ret, "%s\n",
			di_pre_stru.cur_prog_flag
			? "progressive" : "interlace");

	else
		ret += sprintf(buf + ret, "%s\n", "null");

	return ret;
}
static DEVICE_ATTR(frame_format, 0444, show_frame_format, NULL);

static int __init rmem_di_device_init(struct reserved_mem *rmem,
	struct device *dev)
{
	di_dev_t *di_devp = dev_get_drvdata(dev);

	if (di_devp) {
		di_devp->mem_start = rmem->base;
		di_devp->mem_size = rmem->size;
		if (!of_get_flat_dt_prop(rmem->fdt_node, "no-map", NULL))
			di_devp->flags |= DI_MAP_FLAG;
	pr_dbg("di reveser memory 0x%lx, size %uMB.\n",
			di_devp->mem_start, (di_devp->mem_size >> 20));
		return 0;
	}
/* pr_dbg("di reveser memory 0x%x, size %u B.\n",
 * rmem->base, rmem->size);
 */
	return 1;
}

static void rmem_di_device_release(struct reserved_mem *rmem,
				   struct device *dev)
{
	di_dev_t *di_devp = dev_get_drvdata(dev);

	if (di_devp) {
		di_devp->mem_start = 0;
		di_devp->mem_size = 0;
	}
}
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
unsigned int RDMA_RD_BITS(unsigned int adr, unsigned int start,
			  unsigned int len)
{
	if (de_devp->rdma_handle && di_pre_rdma_enable)
		return rdma_read_reg(de_devp->rdma_handle, adr) &
		       (((1 << len) - 1) << start);
	else
		return Rd_reg_bits(adr, start, len);
}

unsigned int RDMA_WR(unsigned int adr, unsigned int val)
{
	if (is_need_stop_reg(adr))
		return 0;

	if (de_devp->rdma_handle > 0 && di_pre_rdma_enable) {
		if (di_pre_stru.field_count_for_cont < 1)
			DI_Wr(adr, val);
		else
			rdma_write_reg(de_devp->rdma_handle, adr, val);
		return 0;
	}

	DI_Wr(adr, val);
	return 1;
}

unsigned int RDMA_RD(unsigned int adr)
{
	if (de_devp->rdma_handle > 0 && di_pre_rdma_enable)
		return rdma_read_reg(de_devp->rdma_handle, adr);
	else
		return Rd(adr);
}

unsigned int RDMA_WR_BITS(unsigned int adr, unsigned int val,
			  unsigned int start, unsigned int len)
{
	if (is_need_stop_reg(adr))
		return 0;

	if (de_devp->rdma_handle > 0 && di_pre_rdma_enable) {
		if (di_pre_stru.field_count_for_cont < 1)
			DI_Wr_reg_bits(adr, val, start, len);
		else
			rdma_write_reg_bits(de_devp->rdma_handle,
				adr, val, start, len);
		return 0;
	}
	DI_Wr_reg_bits(adr, val, start, len);
	return 1;
}
#else
unsigned int RDMA_RD_BITS(unsigned int adr, unsigned int start,
			  unsigned int len)
{
	return Rd_reg_bits(adr, start, len);
}
unsigned int RDMA_WR(unsigned int adr, unsigned int val)
{
	DI_Wr(adr, val);
	return 1;
}

unsigned int RDMA_RD(unsigned int adr)
{
	return Rd(adr);
}

unsigned int RDMA_WR_BITS(unsigned int adr, unsigned int val,
			  unsigned int start, unsigned int len)
{
	DI_Wr_reg_bits(adr, val, start, len);
	return 1;
}
#endif

static void set_di_flag(void)
{
	if (is_meson_txl_cpu() ||
		is_meson_txlx_cpu() ||
		is_meson_gxlx_cpu() ||
		is_meson_txhd_cpu() ||
		is_meson_g12a_cpu() ||
		is_meson_g12b_cpu() ||
		is_meson_tl1_cpu() ||
		is_meson_sm1_cpu()) {
		mcpre_en = true;
		mc_mem_alloc = true;
		pulldown_enable = false;
		di_pre_rdma_enable = false;
		/*
		 * txlx atsc 1080i ei only will cause flicker
		 * when full to small win in home screen
		 */
		di_vscale_skip_enable = (is_meson_txlx_cpu()
				|| is_meson_txhd_cpu())?12:4;
		use_2_interlace_buff = is_meson_gxlx_cpu()?0:1;
		if (is_meson_txl_cpu() ||
			is_meson_txlx_cpu() ||
			is_meson_gxlx_cpu() ||
			is_meson_txhd_cpu() ||
			is_meson_g12a_cpu() ||
			is_meson_g12b_cpu() ||
			is_meson_tl1_cpu() ||
			is_meson_sm1_cpu()) {
			full_422_pack = true;
		}

		if (nr10bit_support)
			di_force_bit_mode = 10;
		else {
			di_force_bit_mode = 8;
			full_422_pack = false;
		}
		post_hold_line =
			(is_meson_g12a_cpu() || is_meson_g12b_cpu()
				|| is_meson_tl1_cpu() ||
				is_meson_sm1_cpu())?10:17;
	} else {
		post_hold_line = 8;	/*2019-01-10: from VLSI feijun*/
		mcpre_en = false;
		pulldown_enable = false;
		di_pre_rdma_enable = false;
		di_vscale_skip_enable = 4;
		use_2_interlace_buff = 0;
		di_force_bit_mode = 8;
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		intr_mode = 3;
	if (di_pre_rdma_enable) {
		pldn_dly = 1;
		pldn_dly1 = 1;
	} else {
		pldn_dly = 2;
		pldn_dly1 = 2;
	}

}

static const struct reserved_mem_ops rmem_di_ops = {
	.device_init	= rmem_di_device_init,
	.device_release = rmem_di_device_release,
};

static int __init rmem_di_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_di_ops;
/* rmem->priv = cma; */

	di_pr_info(
	"DI reserved memory: created CMA memory pool at %pa, size %ld MiB\n",
		&rmem->base, (unsigned long)rmem->size / SZ_1M);

	return 0;
}
RESERVEDMEM_OF_DECLARE(di, "amlogic, di-mem", rmem_di_setup);

static void di_get_vpu_clkb(struct device *dev, struct di_dev_s *pdev)
{

	int ret = 0;
	unsigned int tmp_clk[2] = {0, 0};
	struct clk *vpu_clk = NULL;

	vpu_clk = clk_get(dev, "vpu_mux");
	if (IS_ERR(vpu_clk))
		pr_err("%s: get clk vpu error.\n", __func__);
	else
		clk_prepare_enable(vpu_clk);

	ret = of_property_read_u32_array(dev->of_node, "clock-range",
				tmp_clk, 2);
	if (ret) {
		pdev->clkb_min_rate = 250000000;
		pdev->clkb_max_rate = 500000000;
	} else {
		pdev->clkb_min_rate = tmp_clk[0]*1000000;
		pdev->clkb_max_rate = tmp_clk[1]*1000000;
	}
	pr_info("DI: vpu clkb <%lu, %lu>\n", pdev->clkb_min_rate,
		pdev->clkb_max_rate);
	#ifdef CLK_TREE_SUPPORT
	pdev->vpu_clkb = clk_get(dev, "vpu_clkb_composite");
	if (IS_ERR(pdev->vpu_clkb))
		pr_err("%s: get vpu clkb gate error.\n", __func__);
	clk_set_rate(pdev->vpu_clkb, pdev->clkb_min_rate);
	#endif
}

static int di_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct di_dev_s *di_devp = NULL;

	di_devp = kmalloc(sizeof(struct di_dev_s), GFP_KERNEL);
	if (!di_devp) {
		pr_err("%s fail to allocate memory.\n", __func__);
		goto fail_kmalloc_dev;
	}
	de_devp = di_devp;
	memset(di_devp, 0, sizeof(struct di_dev_s));
	di_devp->flags |= DI_SUSPEND_FLAG;
	cdev_init(&(di_devp->cdev), &di_fops);
	di_devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&(di_devp->cdev), di_devno, DI_COUNT);
	if (ret)
		goto fail_cdev_add;

	di_devp->devt = MKDEV(MAJOR(di_devno), 0);
	di_devp->dev = device_create(di_clsp, &pdev->dev,
		di_devp->devt, di_devp, "di%d", 0);

	if (di_devp->dev == NULL) {
		pr_error("device_create create error\n");
		ret = -EEXIST;
		return ret;
	}
	dev_set_drvdata(di_devp->dev, di_devp);
	platform_set_drvdata(pdev, di_devp);
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret != 0)
		pr_info("DI no reserved mem.\n");
	ret = of_property_read_u32(pdev->dev.of_node,
		"flag_cma", &(di_devp->flag_cma));
	if (ret)
		pr_err("DI-%s: get flag_cma error.\n", __func__);
	ret = of_property_read_u32(pdev->dev.of_node,
		"nrds-enable", &(di_devp->nrds_enable));
	ret = of_property_read_u32(pdev->dev.of_node,
		"pps-enable", &(di_devp->pps_enable));

	if (di_devp->flag_cma >= 1) {
#ifdef CONFIG_CMA
		di_devp->pdev = pdev;
		di_devp->flags |= DI_MAP_FLAG;
		di_devp->mem_size = dma_get_cma_size_int_byte(&pdev->dev);
		pr_info("DI: CMA size 0x%x.\n", di_devp->mem_size);
		if (di_devp->flag_cma == 2) {
			if (di_cma_alloc_total(di_devp))
				atomic_set(&di_devp->mem_flag, 1);
		} else
#endif
			atomic_set(&di_devp->mem_flag, 0);
	} else {
			atomic_set(&di_devp->mem_flag, 1);
	}
	/* mutex_init(&di_devp->cma_mutex); */
	INIT_LIST_HEAD(&di_devp->pq_table_list);

	atomic_set(&di_devp->pq_flag, 0);

	di_devp->pre_irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	pr_info("pre_irq:%d\n",
		di_devp->pre_irq);
	di_devp->post_irq = irq_of_parse_and_map(pdev->dev.of_node, 1);
	pr_info("post_irq:%d\n",
		di_devp->post_irq);

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
/* rdma handle */
	if (di_pre_rdma_enable) {
		di_rdma_op.arg = di_devp;
		di_devp->rdma_handle = rdma_register(&di_rdma_op,
			di_devp, RDMA_TABLE_SIZE);
	}
#endif
	di_pr_info("%s allocate rdma channel %d.\n", __func__,
		di_devp->rdma_handle);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		di_get_vpu_clkb(&pdev->dev, di_devp);
		#ifdef CLK_TREE_SUPPORT
		clk_prepare_enable(di_devp->vpu_clkb);
		pr_info("DI:enable vpu clkb.\n");
		#else
		aml_write_hiubus(HHI_VPU_CLKB_CNTL, 0x1000100);
		#endif
	}
	di_devp->flags &= (~DI_SUSPEND_FLAG);
	ret = of_property_read_u32(pdev->dev.of_node,
		"buffer-size", &(di_devp->buffer_size));
	if (ret)
		pr_err("DI-%s: get buffer size error.\n", __func__);

	/* set flag to indicate that post_wr is supportted */
	ret = of_property_read_u32(pdev->dev.of_node,
				"post-wr-support",
				&(di_devp->post_wr_support));
	if (ret)
		post_wr_support = 0;
	else
		post_wr_support = di_devp->post_wr_support;

	ret = of_property_read_u32(pdev->dev.of_node,
		"nr10bit-support",
		&(di_devp->nr10bit_support));
	if (ret)
		nr10bit_support = 0;
	else
		nr10bit_support = di_devp->nr10bit_support;

	memset(&di_post_stru, 0, sizeof(di_post_stru));
	di_post_stru.next_canvas_id = 1;
#ifdef DI_USE_FIXED_CANVAS_IDX
	if (di_get_canvas()) {
		pr_dbg("DI get canvas error.\n");
		ret = -EEXIST;
		return ret;
	}
#endif

	device_create_file(di_devp->dev, &dev_attr_config);
	device_create_file(di_devp->dev, &dev_attr_debug);
	device_create_file(di_devp->dev, &dev_attr_dump_pic);
	device_create_file(di_devp->dev, &dev_attr_log);
	device_create_file(di_devp->dev, &dev_attr_provider_vframe_status);
	device_create_file(di_devp->dev, &dev_attr_frame_format);
	device_create_file(di_devp->dev, &dev_attr_tvp_region);
	pd_device_files_add(di_devp->dev);
	nr_drv_init(di_devp->dev);

	init_flag = 0;
	reg_flag = 0;

	di_devp->buf_num_avail = di_devp->mem_size / di_devp->buffer_size;
	if (di_devp->buf_num_avail > MAX_LOCAL_BUF_NUM)
		di_devp->buf_num_avail = MAX_LOCAL_BUF_NUM;

	vf_receiver_init(&di_vf_recv, VFM_NAME, &di_vf_receiver, NULL);
	vf_reg_receiver(&di_vf_recv);
	vf_provider_init(&di_vf_prov, VFM_NAME, &deinterlace_vf_provider, NULL);
	active_flag = 1;
	sema_init(&di_sema, 1);
	ret = request_irq(di_devp->pre_irq, &de_irq, IRQF_SHARED,
		"pre_di", (void *)"pre_di");
	if (di_devp->post_wr_support) {
		ret = request_irq(di_devp->post_irq, &post_irq,
			IRQF_SHARED, "post_di", (void *)"post_di");
	}
	//sema_init(&di_sema, 1);
	di_sema_init_flag = 1;
	di_hw_init(pulldown_enable, mcpre_en);
	set_di_flag();

/* Disable MCDI when code does not surpport MCDI */
	if (!mcpre_en)
		DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, 0, 0, 1);
	tasklet_init(&di_pre_tasklet, pre_tasklet,
		(unsigned long)(&di_pre_stru));
	tasklet_disable(&di_pre_tasklet);
	hrtimer_init(&di_pre_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	di_pre_hrtimer.function = di_pre_hrtimer_func;
	hrtimer_start(&di_pre_hrtimer, ms_to_ktime(10), HRTIMER_MODE_REL);
	tasklet_enable(&di_pre_tasklet);
	di_pr_info("%s:Di use HRTIMER\n", __func__);
	di_devp->task = kthread_run(di_task_handle, di_devp, "kthread_di");
	if (IS_ERR(di_devp->task))
		pr_err("%s create kthread error.\n", __func__);
	di_debugfs_init();	/*2018-07-18 add debugfs*/
	di_patch_post_update_mc_sw(DI_MC_SW_IC, true);

	pr_info("%s:ok\n", __func__);
	return ret;

fail_cdev_add:
	pr_info("%s:fail_cdev_add\n", __func__);
	kfree(di_devp);

fail_kmalloc_dev:
	return ret;
}

static int di_remove(struct platform_device *pdev)
{
	struct di_dev_s *di_devp = NULL;

	di_devp = platform_get_drvdata(pdev);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
		clk_disable_unprepare(di_devp->vpu_clkb);
	di_hw_uninit();
	di_devp->di_event = 0xff;
	kthread_stop(di_devp->task);
	hrtimer_cancel(&di_pre_hrtimer);
	tasklet_kill(&di_pre_tasklet);	//ary.sui
	tasklet_disable(&di_pre_tasklet);

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
/* rdma handle */
	if (di_devp->rdma_handle > 0)
		rdma_unregister(di_devp->rdma_handle);
#endif

	vf_unreg_provider(&di_vf_prov);
	vf_unreg_receiver(&di_vf_recv);

	di_uninit_buf(1);
/* Remove the cdev */
	device_remove_file(di_devp->dev, &dev_attr_config);
	device_remove_file(di_devp->dev, &dev_attr_debug);
	device_remove_file(di_devp->dev, &dev_attr_log);
	device_remove_file(di_devp->dev, &dev_attr_dump_pic);
	device_remove_file(di_devp->dev, &dev_attr_provider_vframe_status);
	device_remove_file(di_devp->dev, &dev_attr_frame_format);
	device_remove_file(di_devp->dev, &dev_attr_tvp_region);
	pd_device_files_del(di_devp->dev);
	nr_drv_uninit(di_devp->dev);
	cdev_del(&di_devp->cdev);

	if (di_devp->flag_cma == 2) {
		if (dma_release_from_contiguous(&(pdev->dev),
			di_devp->total_pages,
			di_devp->mem_size >> PAGE_SHIFT)) {
			di_devp->total_pages = NULL;
			di_devp->mem_start = 0;
			pr_dbg("DI CMA total release ok.\n");
		} else {
			pr_dbg("DI CMA total release fail.\n");
		}
		if (de_devp->nrds_enable) {
			nr_ds_buf_uninit(de_devp->flag_cma,
				&(pdev->dev));
		}

	}
	device_destroy(di_clsp, di_devno);
	kfree(di_devp);
/* free drvdata */
	dev_set_drvdata(&pdev->dev, NULL);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void di_shutdown(struct platform_device *pdev)
{
	struct di_dev_s *di_devp = NULL;
	int ret = 0;

	di_devp = platform_get_drvdata(pdev);
	ret = hrtimer_cancel(&di_pre_hrtimer);
	pr_info("di pre hrtimer canel %d.\n", ret);
	tasklet_kill(&di_pre_tasklet);
	tasklet_disable(&di_pre_tasklet);

	init_flag = 0;
	if (is_meson_txlx_cpu())
		di_top_gate_control(true, true);
	else
		DI_Wr(DI_CLKG_CTRL, 0x2);

	if (!is_meson_txlx_cpu())
		switch_vpu_clk_gate_vmod(VPU_VPU_CLKB,
			VPU_CLK_GATE_OFF);
	kfree(di_devp);
	pr_info("[DI] shutdown done.\n");

}

#ifdef CONFIG_PM

static void di_clear_for_suspend(struct di_dev_s *di_devp)
{
	pr_info("%s\n", __func__);

	di_unreg_process();/*have flag*/
	if (di_pre_stru.unreg_req_flag_irq)
		di_unreg_process_irq();

#ifdef CONFIG_CMA
	if (di_pre_stru.cma_release_req) {
		pr_info("\tcma_release\n");
		atomic_set(&di_devp->mem_flag, 0);
		di_cma_release(di_devp);
		di_pre_stru.cma_release_req = 0;
		di_pre_stru.cma_alloc_done = 0;
	}
#endif
	hrtimer_cancel(&di_pre_hrtimer);
	tasklet_kill(&di_pre_tasklet);	//ary.sui
	tasklet_disable(&di_pre_tasklet);
	pr_info("%s end\n", __func__);
}
static int save_init_flag;
/* must called after lcd */
static int di_suspend(struct device *dev)
{

	struct di_dev_s *di_devp = NULL;

	di_devp = dev_get_drvdata(dev);
	di_devp->flags |= DI_SUSPEND_FLAG;

	di_clear_for_suspend(di_devp);//add

	/* fix suspend/resume crash problem */
	save_init_flag = init_flag;
	init_flag = 0;
#if 0	/*2019-01-18*/
	if (di_pre_stru.di_inp_buf) {
		if (vframe_in[di_pre_stru.di_inp_buf->index]) {
			vf_put(vframe_in[di_pre_stru.di_inp_buf->index],
				VFM_NAME);
			vframe_in[di_pre_stru.di_inp_buf->index] = NULL;
			vf_notify_provider(VFM_NAME,
				VFRAME_EVENT_RECEIVER_PUT, NULL);
		}
	}
#endif

	if (!is_meson_txlx_cpu())
		switch_vpu_clk_gate_vmod(VPU_VPU_CLKB,
			VPU_CLK_GATE_OFF);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
		clk_disable_unprepare(di_devp->vpu_clkb);
	pr_info("di: di_suspend\n");
	return 0;
}
/* must called before lcd */
static int di_resume(struct device *dev)
{
	struct di_dev_s *di_devp = NULL;

	di_devp = dev_get_drvdata(dev);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		clk_prepare_enable(di_devp->vpu_clkb);
	init_flag = save_init_flag;
	di_devp->flags &= ~DI_SUSPEND_FLAG;
	/*2018-01-18*/
	pr_info("%s\n", __func__);
	tasklet_init(&di_pre_tasklet, pre_tasklet,
		(unsigned long)(&di_pre_stru));
	tasklet_disable(&di_pre_tasklet);
	hrtimer_init(&di_pre_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	di_pre_hrtimer.function = di_pre_hrtimer_func;
	hrtimer_start(&di_pre_hrtimer, ms_to_ktime(10), HRTIMER_MODE_REL);
	tasklet_enable(&di_pre_tasklet);
	/************/
	pr_info("di: resume module\n");
	return 0;
}

static const struct dev_pm_ops di_pm_ops = {
	.suspend_late = di_suspend,
	.resume_early = di_resume,
};
#endif

/* #ifdef CONFIG_USE_OF */
static const struct of_device_id amlogic_deinterlace_dt_match[] = {
	{ .compatible = "amlogic, deinterlace", },
	{},
};
/* #else */
/* #define amlogic_deinterlace_dt_match NULL */
/* #endif */

static struct platform_driver di_driver = {
	.probe			= di_probe,
	.remove			= di_remove,
	.shutdown		= di_shutdown,
	.driver			= {
		.name		= DEVICE_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = amlogic_deinterlace_dt_match,
#ifdef CONFIG_PM
		.pm			= &di_pm_ops,
#endif
	}
};

static int __init di_module_init(void)
{
	int ret = 0;

	di_pr_info("%s ok.\n", __func__);

	ret = alloc_chrdev_region(&di_devno, 0, DI_COUNT, DEVICE_NAME);
	if (ret < 0) {
		pr_err("%s: failed to allocate major number\n", __func__);
		goto fail_alloc_cdev_region;
	}
	di_pr_info("%s: major %d\n", __func__, MAJOR(di_devno));
	di_clsp = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(di_clsp)) {
		ret = PTR_ERR(di_clsp);
		pr_err("%s: failed to create class\n", __func__);
		goto fail_class_create;
	}

	ret = platform_driver_register(&di_driver);
	if (ret != 0) {
		pr_err("%s: failed to register driver\n", __func__);
		goto fail_pdrv_register;
	}
	return 0;
fail_pdrv_register:
	class_destroy(di_clsp);
fail_class_create:
	unregister_chrdev_region(di_devno, DI_COUNT);
fail_alloc_cdev_region:
	return ret;
}

static void __exit di_module_exit(void)
{
	class_destroy(di_clsp);
	di_debugfs_exit();
	unregister_chrdev_region(di_devno, DI_COUNT);
	platform_driver_unregister(&di_driver);
}

module_init(di_module_init);
module_exit(di_module_exit);

module_param_named(bypass_all, bypass_all, int, 0664);
module_param_named(bypass_3d, bypass_3d, int, 0664);
module_param_named(bypass_trick_mode, bypass_trick_mode, int, 0664);
module_param_named(invert_top_bot, invert_top_bot, int, 0664);
module_param_named(skip_top_bot, skip_top_bot, int, 0664);
module_param_named(force_width, force_width, int, 0664);
module_param_named(force_height, force_height, int, 0664);
module_param_named(prog_proc_config, prog_proc_config, int, 0664);
module_param_named(start_frame_drop_count, start_frame_drop_count, int, 0664);
#ifdef SUPPORT_START_FRAME_HOLD
module_param_named(start_frame_hold_count, start_frame_hold_count, int, 0664);
#endif
module_param_named(same_field_top_count, same_field_top_count,
	long, 0664);
module_param_named(same_field_bot_count, same_field_bot_count,
	long, 0664);
MODULE_PARM_DESC(di_log_flag, "\n di log flag\n");
module_param(di_log_flag, int, 0664);

MODULE_PARM_DESC(di_debug_flag, "\n di debug flag\n");
module_param(di_debug_flag, int, 0664);

MODULE_PARM_DESC(buf_state_log_threshold, "\n buf_state_log_threshold\n");
module_param(buf_state_log_threshold, int, 0664);

MODULE_PARM_DESC(bypass_state, "\n bypass_state\n");
module_param(bypass_state, uint, 0664);

MODULE_PARM_DESC(di_vscale_skip_enable, "\n di_vscale_skip_enable\n");
module_param(di_vscale_skip_enable, uint, 0664);

MODULE_PARM_DESC(di_vscale_skip_count, "\n di_vscale_skip_count\n");
module_param(di_vscale_skip_count, int, 0664);

MODULE_PARM_DESC(di_vscale_skip_count_real, "\n di_vscale_skip_count_real\n");
module_param(di_vscale_skip_count_real, int, 0664);

module_param_named(vpp_3d_mode, vpp_3d_mode, int, 0664);
#ifdef DET3D
MODULE_PARM_DESC(det3d_en, "\n det3d_enable\n");
module_param(det3d_en, bool, 0664);
MODULE_PARM_DESC(det3d_mode, "\n det3d_mode\n");
module_param(det3d_mode, uint, 0664);
#endif

MODULE_PARM_DESC(post_hold_line, "\n post_hold_line\n");
module_param(post_hold_line, uint, 0664);

MODULE_PARM_DESC(post_urgent, "\n post_urgent\n");
module_param(post_urgent, uint, 0664);

MODULE_PARM_DESC(di_printk_flag, "\n di_printk_flag\n");
module_param(di_printk_flag, uint, 0664);

MODULE_PARM_DESC(force_recovery, "\n force_recovery\n");
module_param(force_recovery, uint, 0664);

module_param_named(force_recovery_count, force_recovery_count, uint, 0664);
module_param_named(pre_process_time, pre_process_time, uint, 0664);
module_param_named(bypass_post, bypass_post, uint, 0664);
module_param_named(post_wr_en, post_wr_en, bool, 0664);
module_param_named(post_wr_support, post_wr_support, uint, 0664);
module_param_named(bypass_post_state, bypass_post_state, uint, 0664);
/* n debug for progress interlace mixed source */
module_param_named(use_2_interlace_buff, use_2_interlace_buff, int, 0664);
module_param_named(debug_blend_mode, debug_blend_mode, int, 0664);
MODULE_PARM_DESC(debug_blend_mode, "\n force post blend mode\n");
module_param_named(nr10bit_support, nr10bit_support, uint, 0664);
module_param_named(di_stop_reg_flag, di_stop_reg_flag, uint, 0664);
module_param(di_dbg_mask, uint, 0664);
MODULE_PARM_DESC(di_dbg_mask, "\n di_dbg_mask\n");
module_param(nr_done_check_cnt, uint, 0664);
MODULE_PARM_DESC(nr_done_check_cnt, "\n nr_done_check_cnt\n");
module_param_array(di_stop_reg_addr, uint, &num_di_stop_reg_addr,
	0664);
module_param_named(mcpre_en, mcpre_en, bool, 0664);
module_param_named(check_start_drop_prog, check_start_drop_prog, bool, 0664);
module_param_named(overturn, overturn, bool, 0664);
module_param_named(queue_print_flag, queue_print_flag, int, 0664);
module_param_named(full_422_pack, full_422_pack, bool, 0644);
module_param_named(cma_print, cma_print, bool, 0644);
#ifdef DEBUG_SUPPORT
module_param_named(pulldown_enable, pulldown_enable, bool, 0644);
#ifdef RUN_DI_PROCESS_IN_IRQ
module_param_named(input2pre, input2pre, uint, 0664);
module_param_named(input2pre_buf_miss_count, input2pre_buf_miss_count,
	uint, 0664);
module_param_named(input2pre_proc_miss_count, input2pre_proc_miss_count,
	uint, 0664);
module_param_named(input2pre_miss_policy, input2pre_miss_policy, uint, 0664);
module_param_named(input2pre_throw_count, input2pre_throw_count, uint, 0664);
#endif
#ifdef SUPPORT_MPEG_TO_VDIN
module_param_named(mpeg2vdin_en, mpeg2vdin_en, int, 0664);
module_param_named(mpeg2vdin_flag, mpeg2vdin_flag, int, 0664);
#endif
module_param_named(di_pre_rdma_enable, di_pre_rdma_enable, uint, 0664);
module_param_named(pldn_dly, pldn_dly, uint, 0644);
module_param_named(pldn_dly1, pldn_dly1, uint, 0644);
module_param_named(di_reg_unreg_cnt, di_reg_unreg_cnt, int, 0664);
module_param_named(bypass_pre, bypass_pre, int, 0664);
module_param_named(frame_count, frame_count, int, 0664);
#endif
MODULE_DESCRIPTION("AMLOGIC DEINTERLACE driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("4.0.0");
