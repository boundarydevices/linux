/*
 * drivers/amlogic/media/video_processor/ppmgr/ppmgr_vpp.c
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
/*#include <mach/am_regs.h>*/
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
/*#include <linux/amlogic/amports/vfp.h>*/
#include "vfp.h"
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/cpu_version.h>
/*#include <linux/amlogic/ge2d/ge2d_main.h>*/
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/sched/rt.h>
#include "ppmgr_log.h"
#include "ppmgr_pri.h"
#include "ppmgr_dev.h"
#include <linux/mm.h>
#include <linux/amlogic/media/ppmgr/ppmgr.h>
#include <linux/amlogic/media/ppmgr/ppmgr_status.h>
/*#include "../amports/video.h"*/
#include <linux/amlogic/media/video_sink/video.h>
/*#include "../amports/vdec_reg.h"*/
#include <linux/amlogic/media/utils/vdec_reg.h>
/*#include "../display/osd/osd_reg.h"*/
#include "../../osd/osd_reg.h"
#include "ppmgr_vpp.h"
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/media/ppmgr/tbff.h>

/*#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6*/
/*#include <mach/mod_gate.h>*/
/*#endif*/


#define PPMGRVPP_INFO(fmt, args...) pr_info("PPMGRVPP: info: "fmt"", ## args)
#define PPMGRVPP_DBG(fmt, args...) pr_debug("PPMGRVPP: dbg: "fmt"", ## args)
#define PPMGRVPP_WARN(fmt, args...) pr_warn("PPMGRVPP: warn: "fmt"", ## args)
#define PPMGRVPP_ERR(fmt, args...) pr_err("PPMGRVPP: err: "fmt"", ## args)

#define VF_POOL_SIZE 7
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
#define ASS_POOL_SIZE 2
#else
#define ASS_POOL_SIZE 1
#endif

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS
#define MASK_POOL_SIZE 1
#else
#define MASK_POOL_SIZE 0
#endif

#define PPMGR_TB_DETECT

#define RECEIVER_NAME "ppmgr"
#define PROVIDER_NAME   "ppmgr"

#define MM_ALLOC_SIZE SZ_16M
#define MAX_WIDTH  960
#define MAX_HEIGHT 736
#define THREAD_INTERRUPT 0
#define THREAD_RUNNING 1
#define INTERLACE_DROP_MODE 1
#define EnableVideoLayer()  \
SET_MPEG_REG_MASK(VPP_MISC, \
(VPP_VD1_PREBLEND) | (VPP_PREBLEND_EN) | (VPP_VD1_POSTBLEND))

#define DisableVideoLayer() \
CLEAR_MPEG_REG_MASK(VPP_MISC, \
(VPP_VD1_PREBLEND)|(VPP_VD1_POSTBLEND))

#define DisableVideoLayer_PREBELEND() \
CLEAR_MPEG_REG_MASK(VPP_MISC, \
VPP_VD1_PREBLEND)

static int ass_index;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
static int backup_index = -1;
static int backup_content_w = 0, backup_content_h;
static int scaler_x, scaler_y, scaler_w, scaler_h;
static int scale_clear_count;
static int scaler_pos_changed;
/* extern bool get_scaler_pos_reset(void); */
/* extern void set_scaler_pos_reset(bool flag); */
/* extern u32 amvideo_get_scaler_mode(void); */
/* extern u32 amvideo_get_scaler_para(
 * int *x, int *y, int *w, int *h, u32 *ratio);
 */

#endif

static DEFINE_SPINLOCK(lock);
static bool ppmgr_blocking;
static bool ppmgr_inited;
static int ppmgr_reset_type;
static int ppmgr_buffer_status;
static struct ppframe_s vfp_pool[VF_POOL_SIZE];
static struct vframe_s *vfp_pool_free[VF_POOL_SIZE + 1];
static struct vframe_s *vfp_pool_ready[VF_POOL_SIZE + 1];
struct buf_status_s {
	int index;
	int dirty;
};
static struct buf_status_s buf_status[VF_POOL_SIZE];

struct vfq_s q_ready, q_free;
static int display_mode_change = VF_POOL_SIZE;
static struct semaphore thread_sem;
static DEFINE_MUTEX(ppmgr_mutex);
static bool ppmgr_quit_flag;

#ifdef PPMGR_TB_DETECT
#define TB_DETECT_BUFFER_MAX_SIZE 16
#define TB_DETECT_W 128
#define TB_DETECT_H 96

struct tb_buf_s {
	ulong vaddr;
	ulong paddr;
};

enum tb_status {
	tb_idle,
	tb_running,
	tb_done,
};

static DEFINE_MUTEX(tb_mutex);

static struct tb_buf_s detect_buf[TB_DETECT_BUFFER_MAX_SIZE];
static struct task_struct *tb_detect_task;
static int tb_task_running;
static struct semaphore tb_sem;
static atomic_t detect_status;
static atomic_t tb_detect_flag;
static u8 tb_detect_last_flag;
static u32 tb_buff_wptr;
static u32 tb_buff_rptr;
static s32 tb_canvas = -1;
static u32 tb_src_canvas;
static s8 tb_buffer_status;
static u32 tb_buffer_start;
static u32 tb_buffer_size;
static u8 tb_first_frame_type;
static u32 tb_buffer_len = TB_DETECT_BUFFER_MAX_SIZE;
static atomic_t tb_reset_flag;
static u32 tb_init_mute;
static atomic_t tb_skip_flag;
static atomic_t tb_run_flag;
static bool tb_quit_flag;
static struct TB_DetectFuncPtr *gfunc;
static int tb_buffer_init(void);
static int tb_buffer_uninit(void);
#endif

const struct vframe_receiver_op_s *vf_ppmgr_reg_provider(void);

/* void vf_ppmgr_unreg_provider(void);
 * void vf_ppmgr_reset(int type);
 * void ppmgr_vf_put_dec(struct vframe_s *vf);
 */

/* extern u32 timestamp_pcrscr_enable_state(void); */

#define is_valid_ppframe(pp_vf) \
((pp_vf >= &vfp_pool[0]) && (pp_vf <= &vfp_pool[VF_POOL_SIZE-1]))

/* ***********************************************
 *
 *   Canvas helpers.
 *
 * ************************************************
 */
static u32 ppmgr_canvas_tab[8] = {
PPMGR_CANVAS_INDEX + 0,
PPMGR_CANVAS_INDEX + 1,
PPMGR_CANVAS_INDEX + 2,
PPMGR_CANVAS_INDEX + 3,
PPMGR_CANVAS_INDEX + 4,
PPMGR_CANVAS_INDEX + 5,
PPMGR_CANVAS_INDEX + 6,
PPMGR_CANVAS_INDEX + 7};
u32 index2canvas(u32 index)
{
	return ppmgr_canvas_tab[index];
}

/* ***********************************************
 *
 *   ppmgr as a frame provider
 *
 * ***********************************************
 */
static int task_running;
static int still_picture_notify;
/*static int q_free_set = 0 ;*/
static struct vframe_s *ppmgr_vf_peek(void *op_arg)
{
	struct vframe_s *vf;

	if (ppmgr_blocking)
		return NULL;
	vf = vfq_peek(&q_ready);
	return vf;
}

static struct vframe_s *ppmgr_vf_get(void *op_arg)
{
	if (ppmgr_blocking)
		return NULL;
	return vfq_pop(&q_ready);
}

/*recycle vframe belongs to amvideo*/
static void ppmgr_vf_video_put(struct ppframe_s *pp)
{
	struct ppframe_s *pp_vf = pp;
	struct ppframe_s *vf_local;

	int i = vfq_level(&q_free);
	int index;

	while (i > 0) {
		index = (q_free.rp + i - 1) % (q_free.size);
		vf_local = to_ppframe(q_free.pool[index]);
		if (vf_local->index == pp_vf->index)
			return;

		i--;
	}
	i = vfq_level(&q_ready);
	while (i > 0) {
		index = (q_ready.rp + i - 1) % (q_ready.size);
		vf_local = to_ppframe(q_ready.pool[index]);
		if (vf_local->index == pp_vf->index)
			return;

		i--;
	}
	/* the frame is in bypass mode, put the decoder frame */
	if (pp_vf->dec_frame) {
		ppmgr_vf_put_dec(pp_vf->dec_frame);
		pp_vf->dec_frame = NULL;
	}

}
static void ppmgr_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct ppframe_s *vf_local;
	int i;
	int index;
	struct ppframe_s *pp_vf = to_ppframe(vf);

	if (ppmgr_blocking)
		return;

	i = vfq_level(&q_free);

	while (i > 0) {
		index = (q_free.rp + i - 1) % (q_free.size);
		vf_local = to_ppframe(q_free.pool[index]);
		if (vf_local->index == pp_vf->index)
			return;

		i--;
	}
	i = vfq_level(&q_ready);
	while (i > 0) {
		index = (q_ready.rp + i - 1) % (q_ready.size);
		vf_local = to_ppframe(q_ready.pool[index]);
		if (vf_local->index == pp_vf->index)
			return;

		i--;
	}

	/* the frame is in bypass mode, put the decoder frame */
	if (pp_vf->dec_frame) {
		ppmgr_vf_put_dec(pp_vf->dec_frame);
		pp_vf->dec_frame = NULL;
	}
	vfq_push(&q_free, vf);
}

int vf_ppmgr_get_states(struct vframe_states *states)
{
	int ret = -1;
	unsigned long flags;
	struct vframe_provider_s *vfp;

	vfp = vf_get_provider(RECEIVER_NAME);
	spin_lock_irqsave(&lock, flags);
	if (vfp && vfp->ops && vfp->ops->vf_states)
		ret = vfp->ops->vf_states(states, vfp->op_arg);

	spin_unlock_irqrestore(&lock, flags);
	return ret;
}

static int get_source_type(struct vframe_s *vf)
{
	enum ppmgr_source_type ret;
	int interlace_mode;

	interlace_mode = vf->type & VIDTYPE_TYPEMASK;
	if ((vf->source_type == VFRAME_SOURCE_TYPE_HDMI)
		|| (vf->source_type == VFRAME_SOURCE_TYPE_CVBS)) {
		if ((vf->bitdepth & BITDEPTH_Y10)
			&& (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL))
			ret = VDIN_10BIT_NORMAL;
		else
			ret = VDIN_8BIT_NORMAL;
	} else {
		if ((vf->bitdepth & BITDEPTH_Y10)
			&& (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL)) {
			if (interlace_mode == VIDTYPE_INTERLACE_TOP)
				ret = DECODER_10BIT_TOP;
			else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
				ret = DECODER_10BIT_BOTTOM;
			else
				ret = DECODER_10BIT_NORMAL;
		} else {
			if (interlace_mode == VIDTYPE_INTERLACE_TOP)
				ret = DECODER_8BIT_TOP;
			else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
				ret = DECODER_8BIT_BOTTOM;
			else
				ret = DECODER_8BIT_NORMAL;
		}
	}
	return ret;
}

static int get_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_YUV420;
	enum ppmgr_source_type soure_type;

	soure_type = get_source_type(vf);
	switch (soure_type) {
	case DECODER_8BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422;
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21;
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444;
		else
			format = GE2D_FORMAT_M24_YUV420;
		break;
	case DECODER_8BIT_BOTTOM:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422B & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21B & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444B & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420B & (3 << 3));
		break;
	case DECODER_8BIT_TOP:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444T & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420T & (3 << 3));
		break;
	case DECODER_10BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		}
		break;
	case DECODER_10BIT_BOTTOM:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422
					| (GE2D_FORMAT_S16_10BIT_YUV422B
					& (3 << 3));
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422
					| (GE2D_FORMAT_S16_12BIT_YUV422B
					& (3 << 3));
		}
		break;
	case DECODER_10BIT_TOP:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422
					| (GE2D_FORMAT_S16_10BIT_YUV422T
					& (3 << 3));
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422
					| (GE2D_FORMAT_S16_12BIT_YUV422T
					& (3 << 3));
		}
		break;
	case VDIN_8BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422;
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21;
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444;
		else
			format = GE2D_FORMAT_M24_YUV420;
		break;
	case VDIN_10BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		}
		break;
	default:
		format = GE2D_FORMAT_M24_YUV420;
	}
	return format;
}


static void dma_flush(u32 buf_start, u32 buf_size)
{
	return;
	/*
	dma_sync_single_for_device(
		&ppmgr_device.pdev->dev, buf_start,
		buf_size, DMA_TO_DEVICE);
	*/
}

/* extern int get_property_change(void); */
/* extern void set_property_change(int flag); */
/* extern int get_buff_change(void); */
/* extern void set_buff_change(int flag); */

static int ppmgr_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_PUT) {
#ifdef DDD
		PPMGRVPP_WARN("video put, avail=%d, free=%d\n",
			vfq_level(&q_ready), vfq_level(&q_free));
#endif
		up(&thread_sem);
	}
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
	if (type & VFRAME_EVENT_RECEIVER_POS_CHANGED) {
		if (task_running) {
			scaler_pos_changed = 1;
			/*printk("--ppmgr: get pos changed msg.\n");*/
			up(&thread_sem);
		}
	}
#endif
	if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT) {
		if (task_running && !ppmgr_device.use_prot) {

			if (timestamp_pcrscr_enable_state())
				return 0;

			if (get_property_change()) {
				/*printk("--ppmgr: get angle changed msg.\n");*/
				set_property_change(0);
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
				if (!amvideo_get_scaler_mode()) {
					still_picture_notify = 1;
					up(&thread_sem);
				}
#else
				still_picture_notify = 1;
				up(&thread_sem);
#endif
			} else {
				up(&thread_sem);
			}
		}
	}
#ifdef CONFIG_V4L_AMLOGIC_VIDEO
	if ((type & VFRAME_EVENT_RECEIVER_PARAM_SET)) {
		unsigned int *eventparam = (unsigned int *)data;

		ppmgr_device.canvas_width = eventparam[0];
		ppmgr_device.canvas_height = eventparam[1];
		ppmgr_device.receiver_format = eventparam[2];
		/* ppmgr_buffer_init(0); */
	}
#endif
	return 0;
}

static int ppmgr_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_recycle_num = 0;
	states->buf_free_num = vfq_level(&q_free);
	states->buf_avail_num = vfq_level(&q_ready);

	spin_unlock_irqrestore(&lock, flags);

	return 0;
}

static const struct vframe_operations_s ppmgr_vf_provider = {
		.peek = ppmgr_vf_peek,
		.get = ppmgr_vf_get,
		.put = ppmgr_vf_put,
		.event_cb = ppmgr_event_cb,
		.vf_states = ppmgr_vf_states, };
static struct vframe_provider_s ppmgr_vf_prov;

/************************************************
 *
 *   ppmgr as a frame receiver
 *
 *************************************************/

static int ppmgr_receiver_event_fun(int type, void *data, void*);

static const struct vframe_receiver_op_s ppmgr_vf_receiver = {.event_cb =
		ppmgr_receiver_event_fun};
static struct vframe_receiver_s ppmgr_vf_recv;

static int ppmgr_receiver_event_fun(int type, void *data, void *private_data)
{
	struct vframe_states states;

	switch (type) {
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
#ifdef DDD
		PPMGRVPP_WARN("dec put, avail=%d, free=%d\n",
			vfq_level(&q_ready), vfq_level(&q_free));
#endif
		up(&thread_sem);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		ppmgr_vf_states(&states, NULL);
		if (states.buf_avail_num > 0)
			return RECEIVER_ACTIVE;
		if (vf_notify_receiver(
			PROVIDER_NAME,
			VFRAME_EVENT_PROVIDER_QUREY_STATE,
			NULL)
		== RECEIVER_ACTIVE)
			return RECEIVER_ACTIVE;
		return RECEIVER_INACTIVE;
	case VFRAME_EVENT_PROVIDER_START:
#ifdef DDD
		PPMGRVPP_WARN("register now\n");
#endif
		vf_ppmgr_reg_provider();
		vf_notify_receiver(
				PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_START,
				NULL);
		break;
	case VFRAME_EVENT_PROVIDER_UNREG:
#ifdef DDD
		PPMGRVPP_WARN("unregister now\n");
#endif
		vf_ppmgr_unreg_provider();
		break;
	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG:
		break;
	case VFRAME_EVENT_PROVIDER_RESET:
		vf_ppmgr_reset(0);
		vf_notify_receiver(
			PROVIDER_NAME,
			VFRAME_EVENT_PROVIDER_RESET,
			NULL);
		break;
	case VFRAME_EVENT_PROVIDER_FR_HINT:
	case VFRAME_EVENT_PROVIDER_FR_END_HINT:
		vf_notify_receiver(PROVIDER_NAME, type, data);
		break;
	default:
		break;
	}
	return 0;
}

void vf_local_init(void)
{
	int i;

	set_property_change(0);
	still_picture_notify = 0;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
	scaler_pos_changed = 0;
	scaler_x = scaler_y = scaler_w = scaler_h = 0;
	backup_content_w = backup_content_h = 0;
#endif
	vfq_init(&q_free, VF_POOL_SIZE + 1, &vfp_pool_free[0]);
	vfq_init(&q_ready, VF_POOL_SIZE + 1, &vfp_pool_ready[0]);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		vfp_pool[i].index = i;
		vfp_pool[i].dec_frame = NULL;
		vfq_push(&q_free, &vfp_pool[i].frame);
	}

	for (i = 0; i < VF_POOL_SIZE; i++) {
		buf_status[i].index = ppmgr_canvas_tab[i];
		buf_status[i].dirty = 1;
	}
	sema_init(&thread_sem, 1);
}

static const struct vframe_provider_s *dec_vfp;

const struct vframe_receiver_op_s *vf_ppmgr_reg_provider(void)
{
	const struct vframe_receiver_op_s *r = NULL;

	mutex_lock(&ppmgr_mutex);

	vf_local_init();
	vf_reg_provider(&ppmgr_vf_prov);
	if (start_ppmgr_task() == 0)
		r = &ppmgr_vf_receiver;

	ppmgr_device.started = 1;

	mutex_unlock(&ppmgr_mutex);

	return r;
}

void vf_ppmgr_unreg_provider(void)
{
	mutex_lock(&ppmgr_mutex);

	stop_ppmgr_task();

	vf_unreg_provider(&ppmgr_vf_prov);

	dec_vfp = NULL;

	ppmgr_device.started = 0;
	/*ppmgr_device.use_prot = 0;*/

	mutex_unlock(&ppmgr_mutex);
}

/*skip the second reset request*/
static unsigned long last_reset_time;
static unsigned long current_reset_time;
#define PPMGR_RESET_INTERVAL  (HZ/5)
void vf_ppmgr_reset(int type)
{
	if (ppmgr_inited) {
		current_reset_time = jiffies;
		if (abs(current_reset_time-last_reset_time)
			< PPMGR_RESET_INTERVAL)
			return;

		ppmgr_blocking = true;
		ppmgr_reset_type = type;
		up(&thread_sem);

		last_reset_time = current_reset_time;
	}
}

void vf_ppmgr_init_receiver(void)
{
	vf_receiver_init(
		&ppmgr_vf_recv,
		RECEIVER_NAME,
		&ppmgr_vf_receiver,
		NULL);
}

void vf_ppmgr_reg_receiver(void)
{
	vf_reg_receiver(&ppmgr_vf_recv);
}
void vf_ppmgr_init_provider(void)
{
	vf_provider_init(
		&ppmgr_vf_prov,
		PROVIDER_NAME,
		&ppmgr_vf_provider,
		NULL);
}

static inline struct vframe_s *ppmgr_vf_peek_dec(void)
{
#if 0
	struct vframe_provider_s *vfp;
	struct vframe_s *vf;

	vfp = vf_get_provider(RECEIVER_NAME);
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return NULL;

	vf = vfp->ops->peek(vfp->op_arg);
	return vf;
#else
	return vf_peek(RECEIVER_NAME);
#endif
}

static inline struct vframe_s *ppmgr_vf_get_dec(void)
{
#if 0

	struct vframe_provider_s *vfp;
	struct vframe_s *vf;

	vfp = vf_get_provider(RECEIVER_NAME);
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return NULL;
	vf = vfp->ops->get(vfp->op_arg);
	return vf;
#else
	return vf_get(RECEIVER_NAME);
#endif

}

void ppmgr_vf_put_dec(struct vframe_s *vf)
{
#if 0

	struct vframe_provider_s *vfp;

	vfp = vf_get_provider(RECEIVER_NAME);
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return;
	vfp->ops->put(vf, vfp->op_arg);
#else
	vf_put(vf, RECEIVER_NAME);
#endif
}

/************************************************
 *
 *   main task functions.
 *
 *************************************************/
static void vf_rotate_adjust(struct vframe_s *vf, struct vframe_s *new_vf,
		int angle)
{
	int w = 0, h = 0, disp_w = 0, disp_h = 0;
	int scale_down_value = 1;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS
	scale_down_value = ppmgr_device.scale_down + 1;
#endif
	disp_w = ppmgr_device.disp_width / scale_down_value;
	disp_h = ppmgr_device.disp_height / scale_down_value;

	if (angle & 1) {
		int ar = (vf->ratio_control
			>> DISP_RATIO_ASPECT_RATIO_BIT) & 0x3ff;

		/* h = min((int)vf->width, disp_h); */
		h = min_t(int, vf->width, disp_h);

		if (ar == 0)
			w = vf->height * h / vf->width;
		else
			w = (ar * h) >> 8;

		if (w > disp_w) {
			h = (h * disp_w) / w;
			w = disp_w;
		}

		new_vf->ratio_control = DISP_RATIO_PORTRAIT_MODE;
		new_vf->ratio_control |=
				(h * 0x100 / w) << DISP_RATIO_ASPECT_RATIO_BIT;
		/*set video aspect ratio*/
	} else {
		if ((vf->width < disp_w) && (vf->height < disp_h)) {
			w = vf->width;
			h = vf->height;
		} else {
			if ((vf->width * disp_h) > (disp_w * vf->height)) {
				w = disp_w;
				h = disp_w * vf->height / vf->width;
			} else {
				h = disp_h;
				w = disp_h * vf->width / vf->height;
			}
		}

		new_vf->ratio_control = vf->ratio_control;
	}

	if (h > 1080) {
		w = w * 1080 / h;
		h = 1080;
	}

	new_vf->width = w;
	new_vf->height = h;
}
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
static void display_mode_adjust(struct ge2d_context_s *context,
		struct vframe_s *new_vf)
{
	int canvas_width = ppmgr_device.canvas_width;
	int canvas_height = ppmgr_device.canvas_height;
	int vf_width = new_vf->width;
	int vf_height = new_vf->height;
	static int current_display_mode;

	if (ppmgr_device.display_mode != current_display_mode) {
		current_display_mode = ppmgr_device.display_mode;
		display_mode_change = VF_POOL_SIZE;
	}
	if (display_mode_change > 0) {
		display_mode_change--;
		fillrect(context, 0, 0,
				canvas_width, canvas_height, 0x008080ff);
	}
	if (ppmgr_device.display_mode == 0) {/*stretch full*/
		stretchblt_noalpha(context, 0, 0, vf_width,
				vf_height,
				0, 0, canvas_width,
				canvas_height);
	} else if (ppmgr_device.display_mode == 1) {/*keep size*/
		stretchblt_noalpha(context, 0, 0, vf_width,
				vf_height,
				(canvas_width - vf_width) / 2,
				(canvas_height - vf_height) / 2,
				vf_width, vf_height);
	} else if (ppmgr_device.display_mode == 2) {/*keep ration black*/
		int dw = 0, dh = 0;
		if (vf_width / vf_height >= canvas_width / canvas_height) {
			dw = canvas_width;
			dh = dw * vf_height / vf_width;
			stretchblt_noalpha(context, 0, 0, vf_width,
				vf_height,
				(canvas_width - dw) / 2,
				(canvas_height - dh) / 2, dw, dh);
		} else {
			dh = canvas_height;
			dw = dh * vf_width / vf_height;
			stretchblt_noalpha(context, 0, 0, vf_width,
				vf_height,
				(canvas_width - dw) / 2,
				(canvas_height - dh) / 2, dw, dh);
		}
	} else if (ppmgr_device.display_mode == 3) {
		int dw = 0, dh = 0;
		if (vf_width / vf_height
				>= canvas_width / canvas_height) {
			dh = canvas_height;
			dw = dh * vf_width / vf_height;
			stretchblt_noalpha(
				context, 0, 0, vf_width,
				vf_height,
				(canvas_width - dw) / 2,
				(canvas_height - dh) / 2, dw, dh);
		} else {
			dw = canvas_width;
			dh = dw * vf_height / vf_width;
			stretchblt_noalpha(context, 0, 0, vf_width,
				vf_height,
				(canvas_width - dw) / 2,
				(canvas_height - dh) / 2, dw, dh);
		}
	}
}
#endif

#ifdef PPMGR_TB_DETECT
static int process_vf_tb_detect(struct vframe_s *vf,
		struct ge2d_context_s *context,
		struct config_para_ex_s *ge2d_config)
{
	struct canvas_s cs0, cs1, cs2, cd;
	int interlace_mode;
	struct vframe_s src_vf;
	u32 format = GE2D_FORMAT_M24_YUV420;
	u32 h_scale_coef_type =
		context->config.h_scale_coef_type;
	u32 v_scale_coef_type =
		context->config.v_scale_coef_type;

	if (unlikely(!vf))
		return -1;

	interlace_mode = vf->type & VIDTYPE_TYPEMASK;
	if (!interlace_mode)
		return 0;

	if (vf->type & VIDTYPE_VIU_422)
		format = GE2D_FORMAT_S16_YUV422;
	else if (vf->type & VIDTYPE_VIU_NV21)
		format = GE2D_FORMAT_M24_NV21;
	else
		format = GE2D_FORMAT_M24_YUV420;
	if (tb_buff_wptr & 1) {
		format = format
			| (GE2D_FORMAT_M24_NV21B & (3 << 3));
		context->config.h_scale_coef_type =
			FILTER_TYPE_GAU0;
		context->config.v_scale_coef_type =
			FILTER_TYPE_GAU0_BOT;
	} else {
		format = format
			| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		context->config.h_scale_coef_type =
			FILTER_TYPE_GAU0;
		context->config.v_scale_coef_type =
			FILTER_TYPE_GAU0;
	}
	canvas_config(tb_canvas,
		detect_buf[tb_buff_wptr].paddr,
		TB_DETECT_W, TB_DETECT_H,
		CANVAS_ADDR_NOWRAP,
		CANVAS_BLKMODE_LINEAR);
	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));

	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	src_vf = *vf;
	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(
			tb_src_canvas & 0xff,
			&src_vf.canvas0_config[0]);
		canvas_config_config(
			(tb_src_canvas >> 8) & 0xff,
			&src_vf.canvas0_config[1]);
		if (src_vf.plane_num == 2) {
			src_vf.canvas0Addr =
				tb_src_canvas & 0xffff;
		} else if (src_vf.plane_num == 3) {
			canvas_config_config(
				(tb_src_canvas >> 16) & 0xff,
				&src_vf.canvas0_config[2]);
			src_vf.canvas0Addr =
				tb_src_canvas & 0xffffff;
		}
		canvas_read(
			src_vf.canvas0Addr & 0xff, &cs0);
		canvas_read(
			(src_vf.canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read(
			(src_vf.canvas0Addr >> 16) & 0xff, &cs2);
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
	}
	ge2d_config->src_planes[0].addr = cs0.addr;
	ge2d_config->src_planes[0].w = cs0.width;
	ge2d_config->src_planes[0].h = cs0.height;
	ge2d_config->src_planes[1].addr = cs1.addr;
	ge2d_config->src_planes[1].w = cs1.width;
	ge2d_config->src_planes[1].h = cs1.height;
	ge2d_config->src_planes[2].addr = cs2.addr;
	ge2d_config->src_planes[2].w = cs2.width;
	ge2d_config->src_planes[2].h = cs2.height;
	canvas_read(tb_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = src_vf.canvas0Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = format;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = tb_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format =
		GE2D_FORMAT_S8_Y | GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = TB_DETECT_W;
	ge2d_config->dst_para.height = TB_DETECT_H;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		context->config.h_scale_coef_type =
			h_scale_coef_type;
		context->config.v_scale_coef_type =
			v_scale_coef_type;
		return -1;
	}

	stretchblt_noalpha(
		context, 0, 0, vf->width,
		vf->height / 2,
		0, 0, TB_DETECT_W,
		TB_DETECT_H);
	codec_mm_dma_flush(
		(void *)detect_buf[tb_buff_wptr].vaddr,
		TB_DETECT_W * TB_DETECT_H,
		DMA_FROM_DEVICE);
	context->config.h_scale_coef_type =
		h_scale_coef_type;
	context->config.v_scale_coef_type =
		v_scale_coef_type;
	return 1;
}
#endif

static int process_vf_deinterlace_nv21(struct vframe_s *vf,
		struct ge2d_context_s *context,
		struct config_para_ex_s *ge2d_config)
{
	struct canvas_s cs0, cs1, cs2, cd;

	if (!vf)
		return -1;

	if (vf->type & VIDTYPE_MVC)
		return 0;

	if (vf->type & VIDTYPE_COMPRESS)
		return 0;
	if ((vf->canvas0Addr == vf->canvas1Addr) || (ppmgr_device.angle == 0)) {
		/*printk("++ppmgr interlace skip.\n");*/
		return 0;
	}
	/* operation top line*/
	/* Y data*/
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;
	ge2d_config->dst_xy_swap = 0;

	canvas_read(vf->canvas0Addr & 0xff, &cs0);
	canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
	canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
	ge2d_config->src_planes[0].addr = cs0.addr;
	ge2d_config->src_planes[0].w = cs0.width;
	ge2d_config->src_planes[0].h = cs0.height;
	ge2d_config->src_planes[1].addr = cs1.addr;
	ge2d_config->src_planes[1].w = cs1.width;
	ge2d_config->src_planes[1].h = cs1.height;
	ge2d_config->src_planes[2].addr = cs2.addr;
	ge2d_config->src_planes[2].w = cs2.width;
	ge2d_config->src_planes[2].h = cs2.height;

	canvas_read(PPMGR_DEINTERLACE_BUF_NV21_CANVAS, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->dst_planes[1].addr = 0;
	ge2d_config->dst_planes[1].w = 0;
	ge2d_config->dst_planes[1].h = 0;
	ge2d_config->dst_planes[2].addr = 0;
	ge2d_config->dst_planes[2].w = 0;
	ge2d_config->dst_planes[2].h = 0;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_key.key_color = 0;

	ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FORMAT_M24_NV21;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;

	ge2d_config->dst_para.canvas_index = PPMGR_DEINTERLACE_BUF_NV21_CANVAS;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = (GE2D_FORMAT_S24_YUV444T & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = vf->width;
	ge2d_config->dst_para.height = vf->height / 2;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, 0, 0, vf->width, vf->height / 2, 0, 0,
			vf->width, vf->height / 2);

	/* operation bottom line*/
	/*Y data*/
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;
	ge2d_config->dst_xy_swap = 0;

	canvas_read(vf->canvas1Addr & 0xff, &cs0);
	canvas_read((vf->canvas1Addr >> 8) & 0xff, &cs1);
	canvas_read((vf->canvas1Addr >> 16) & 0xff, &cs2);
	ge2d_config->src_planes[0].addr = cs0.addr;
	ge2d_config->src_planes[0].w = cs0.width;
	ge2d_config->src_planes[0].h = cs0.height;
	ge2d_config->src_planes[1].addr = cs1.addr;
	ge2d_config->src_planes[1].w = cs1.width;
	ge2d_config->src_planes[1].h = cs1.height;
	ge2d_config->src_planes[2].addr = cs2.addr;
	ge2d_config->src_planes[2].w = cs2.width;
	ge2d_config->src_planes[2].h = cs2.height;

	canvas_read(PPMGR_DEINTERLACE_BUF_NV21_CANVAS, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->dst_planes[1].addr = 0;
	ge2d_config->dst_planes[1].w = 0;
	ge2d_config->dst_planes[1].h = 0;
	ge2d_config->dst_planes[2].addr = 0;
	ge2d_config->dst_planes[2].w = 0;
	ge2d_config->dst_planes[2].h = 0;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_key.key_color = 0;

	ge2d_config->src_para.canvas_index = vf->canvas1Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FORMAT_M24_NV21;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;

	ge2d_config->dst_para.canvas_index = PPMGR_DEINTERLACE_BUF_NV21_CANVAS;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = (GE2D_FORMAT_S24_YUV444B & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = vf->width;
	ge2d_config->dst_para.height = vf->height / 2;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, 0, 0, vf->width, vf->height / 2, 0, 0,
			vf->width, vf->height / 2);

	return 2;
}

static int process_vf_deinterlace(struct vframe_s *vf,
		struct ge2d_context_s *context,
		struct config_para_ex_s *ge2d_config)
{
	struct canvas_s cs, cd;
	int ret = 0;

	if (!vf)
		return -1;

	if (vf->type & VIDTYPE_MVC)
		return 0;

	if (vf->type & VIDTYPE_COMPRESS)
		return 0;
	if ((vf->canvas0Addr == vf->canvas1Addr) || (ppmgr_device.bypass)
			|| (ppmgr_device.angle == 0)) {
		/*printk("++ppmgr interlace skip.\n");*/
		return 0;
	}

	if (vf->type & VIDTYPE_VIU_NV21) {
		ret = process_vf_deinterlace_nv21(vf, context, ge2d_config);
		return ret;
	}
	/* operation top line*/
	/* Y data*/
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;
	ge2d_config->dst_xy_swap = 0;

	canvas_read(vf->canvas0Addr & 0xff, &cs);
	ge2d_config->src_planes[0].addr = cs.addr;
	ge2d_config->src_planes[0].w = cs.width;
	ge2d_config->src_planes[0].h = cs.height;
	ge2d_config->src_planes[1].addr = 0;
	ge2d_config->src_planes[1].w = 0;
	ge2d_config->src_planes[1].h = 0;
	ge2d_config->src_planes[2].addr = 0;
	ge2d_config->src_planes[2].w = 0;
	ge2d_config->src_planes[2].h = 0;

	canvas_read(PPMGR_DEINTERLACE_BUF_CANVAS, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->dst_planes[1].addr = 0;
	ge2d_config->dst_planes[1].w = 0;
	ge2d_config->dst_planes[1].h = 0;
	ge2d_config->dst_planes[2].addr = 0;
	ge2d_config->dst_planes[2].w = 0;
	ge2d_config->dst_planes[2].h = 0;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_key.key_color = 0;

	ge2d_config->src_para.canvas_index = vf->canvas0Addr & 0xff;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FMT_S8_Y;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;

	ge2d_config->dst_para.canvas_index = PPMGR_DEINTERLACE_BUF_CANVAS;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FMT_S8_Y
			| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = vf->width;
	ge2d_config->dst_para.height = vf->height / 2;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, 0, 0, vf->width, vf->height / 2, 0, 0,
			vf->width, vf->height / 2);

	/*U data*/
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;
	ge2d_config->dst_xy_swap = 0;

	canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs);
	ge2d_config->src_planes[0].addr = cs.addr;
	ge2d_config->src_planes[0].w = cs.width;
	ge2d_config->src_planes[0].h = cs.height;
	ge2d_config->src_planes[1].addr = 0;
	ge2d_config->src_planes[1].w = 0;
	ge2d_config->src_planes[1].h = 0;
	ge2d_config->src_planes[2].addr = 0;
	ge2d_config->src_planes[2].w = 0;
	ge2d_config->src_planes[2].h = 0;

	canvas_read(PPMGR_DEINTERLACE_BUF_CANVAS + 1, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->dst_planes[1].addr = 0;
	ge2d_config->dst_planes[1].w = 0;
	ge2d_config->dst_planes[1].h = 0;
	ge2d_config->dst_planes[2].addr = 0;
	ge2d_config->dst_planes[2].w = 0;
	ge2d_config->dst_planes[2].h = 0;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_key.key_color = 0;

	ge2d_config->src_para.canvas_index = (vf->canvas0Addr >> 8) & 0xff;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FMT_S8_CB;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width / 2;
	ge2d_config->src_para.height = vf->height / 4;

	ge2d_config->dst_para.canvas_index = PPMGR_DEINTERLACE_BUF_CANVAS + 1;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FMT_S8_CB
			| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = vf->width / 2;
	ge2d_config->dst_para.height = vf->height / 4;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, 0, 0, vf->width / 2, vf->height / 4, 0, 0,
			vf->width / 2, vf->height / 4);

	/*V data*/
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;
	ge2d_config->dst_xy_swap = 0;

	canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs);
	ge2d_config->src_planes[0].addr = cs.addr;
	ge2d_config->src_planes[0].w = cs.width;
	ge2d_config->src_planes[0].h = cs.height;
	ge2d_config->src_planes[1].addr = 0;
	ge2d_config->src_planes[1].w = 0;
	ge2d_config->src_planes[1].h = 0;
	ge2d_config->src_planes[2].addr = 0;
	ge2d_config->src_planes[2].w = 0;
	ge2d_config->src_planes[2].h = 0;

	canvas_read(PPMGR_DEINTERLACE_BUF_CANVAS + 2, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->dst_planes[1].addr = 0;
	ge2d_config->dst_planes[1].w = 0;
	ge2d_config->dst_planes[1].h = 0;
	ge2d_config->dst_planes[2].addr = 0;
	ge2d_config->dst_planes[2].w = 0;
	ge2d_config->dst_planes[2].h = 0;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_key.key_color = 0;

	ge2d_config->src_para.canvas_index = (vf->canvas0Addr >> 16) & 0xff;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FMT_S8_CR;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width / 2;
	ge2d_config->src_para.height = vf->height / 4;

	ge2d_config->dst_para.canvas_index = PPMGR_DEINTERLACE_BUF_CANVAS + 2;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FMT_S8_CR
			| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = vf->width / 2;
	ge2d_config->dst_para.height = vf->height / 4;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, 0, 0, vf->width / 2, vf->height / 4, 0, 0,
			vf->width / 2, vf->height / 4);

	/* operation bottom line*/
	/*Y data*/
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;
	ge2d_config->dst_xy_swap = 0;

	canvas_read(vf->canvas1Addr & 0xff, &cs);
	ge2d_config->src_planes[0].addr = cs.addr;
	ge2d_config->src_planes[0].w = cs.width;
	ge2d_config->src_planes[0].h = cs.height;
	ge2d_config->src_planes[1].addr = 0;
	ge2d_config->src_planes[1].w = 0;
	ge2d_config->src_planes[1].h = 0;
	ge2d_config->src_planes[2].addr = 0;
	ge2d_config->src_planes[2].w = 0;
	ge2d_config->src_planes[2].h = 0;

	canvas_read(PPMGR_DEINTERLACE_BUF_CANVAS, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->dst_planes[1].addr = 0;
	ge2d_config->dst_planes[1].w = 0;
	ge2d_config->dst_planes[1].h = 0;
	ge2d_config->dst_planes[2].addr = 0;
	ge2d_config->dst_planes[2].w = 0;
	ge2d_config->dst_planes[2].h = 0;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_key.key_color = 0;

	ge2d_config->src_para.canvas_index = vf->canvas1Addr & 0xff;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FMT_S8_Y;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;

	ge2d_config->dst_para.canvas_index = PPMGR_DEINTERLACE_BUF_CANVAS;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FMT_S8_Y
			| (GE2D_FORMAT_M24_YUV420B & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = vf->width;
	ge2d_config->dst_para.height = vf->height / 2;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, 0, 0, vf->width, vf->height / 2, 0, 0,
			vf->width, vf->height / 2);

	/*U data*/
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;
	ge2d_config->dst_xy_swap = 0;

	canvas_read((vf->canvas1Addr >> 8) & 0xff, &cs);
	ge2d_config->src_planes[0].addr = cs.addr;
	ge2d_config->src_planes[0].w = cs.width;
	ge2d_config->src_planes[0].h = cs.height;
	ge2d_config->src_planes[1].addr = 0;
	ge2d_config->src_planes[1].w = 0;
	ge2d_config->src_planes[1].h = 0;
	ge2d_config->src_planes[2].addr = 0;
	ge2d_config->src_planes[2].w = 0;
	ge2d_config->src_planes[2].h = 0;

	canvas_read(PPMGR_DEINTERLACE_BUF_CANVAS + 1, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->dst_planes[1].addr = 0;
	ge2d_config->dst_planes[1].w = 0;
	ge2d_config->dst_planes[1].h = 0;
	ge2d_config->dst_planes[2].addr = 0;
	ge2d_config->dst_planes[2].w = 0;
	ge2d_config->dst_planes[2].h = 0;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_key.key_color = 0;

	ge2d_config->src_para.canvas_index = (vf->canvas1Addr >> 8) & 0xff;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FMT_S8_CB;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width / 2;
	ge2d_config->src_para.height = vf->height / 4;

	ge2d_config->dst_para.canvas_index = PPMGR_DEINTERLACE_BUF_CANVAS + 1;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FMT_S8_CB
			| (GE2D_FORMAT_M24_YUV420B & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = vf->width / 2;
	ge2d_config->dst_para.height = vf->height / 4;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, 0, 0, vf->width / 2, vf->height / 4, 0, 0,
			vf->width / 2, vf->height / 4);

	/*V data*/
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;
	ge2d_config->dst_xy_swap = 0;

	canvas_read((vf->canvas1Addr >> 16) & 0xff, &cs);
	ge2d_config->src_planes[0].addr = cs.addr;
	ge2d_config->src_planes[0].w = cs.width;
	ge2d_config->src_planes[0].h = cs.height;
	ge2d_config->src_planes[1].addr = 0;
	ge2d_config->src_planes[1].w = 0;
	ge2d_config->src_planes[1].h = 0;
	ge2d_config->src_planes[2].addr = 0;
	ge2d_config->src_planes[2].w = 0;
	ge2d_config->src_planes[2].h = 0;

	canvas_read(PPMGR_DEINTERLACE_BUF_CANVAS + 2, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->dst_planes[1].addr = 0;
	ge2d_config->dst_planes[1].w = 0;
	ge2d_config->dst_planes[1].h = 0;
	ge2d_config->dst_planes[2].addr = 0;
	ge2d_config->dst_planes[2].w = 0;
	ge2d_config->dst_planes[2].h = 0;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_key.key_color = 0;

	ge2d_config->src_para.canvas_index = (vf->canvas1Addr >> 16) & 0xff;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FMT_S8_CR;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width / 2;
	ge2d_config->src_para.height = vf->height / 4;

	ge2d_config->dst_para.canvas_index = PPMGR_DEINTERLACE_BUF_CANVAS + 2;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FMT_S8_CR
			| (GE2D_FORMAT_M24_YUV420B & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = vf->width / 2;
	ge2d_config->dst_para.height = vf->height / 4;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, 0, 0, vf->width / 2, vf->height / 4, 0, 0,
			vf->width / 2, vf->height / 4);
	/*printk("++ppmgr interlace success.\n");*/
	return 1;
}

static void process_vf_rotate(struct vframe_s *vf,
		struct ge2d_context_s *context,
		struct config_para_ex_s *ge2d_config,
		int deinterlace)
{
	struct vframe_s *new_vf;
	struct ppframe_s *pp_vf;
	struct canvas_s cs0, cs1, cs2, cd;
	int ret = 0;
	unsigned int cur_angle = 0;
	int interlace_mode;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS
	enum platform_type_t platform_type;
#endif
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
	int i;
	u32 mode = 0;
	int rect_x = 0, rect_y = 0, rect_w = 0, rect_h = 0;
	u32 ratio = 100;

	mode = amvideo_get_scaler_para(
		&rect_x,
		&rect_y,
		&rect_w,
		&rect_h,
		&ratio);

	if ((rect_w == 0 || rect_h == 0) && mode) {
		rect_w = ppmgr_device.disp_width;
		rect_h = ppmgr_device.disp_height;
	}
	if (ppmgr_device.receiver != 0 && mode) {
		rect_w = ppmgr_device.canvas_width;
		rect_h = ppmgr_device.canvas_height;
		mode = 0;
	}

	rect_w = max(rect_w, 64);
	rect_h = max(rect_h, 64);
#endif

	new_vf = vfq_pop(&q_free);

	if (unlikely((!new_vf) || (!vf)))
		return;

	interlace_mode = vf->type & VIDTYPE_TYPEMASK;

	pp_vf = to_ppframe(new_vf);
	pp_vf->angle = 0;
	cur_angle = (ppmgr_device.videoangle + vf->orientation) % 4;
#ifdef CONFIG_V4L_AMLOGIC_VIDEO
	if (ppmgr_device.receiver == 0)
		pp_vf->dec_frame =
			(ppmgr_device.bypass || (cur_angle == 0
					&& ppmgr_device.mirror_flag == 0)
			|| ppmgr_device.use_prot) ? vf : NULL;
	else
		pp_vf->dec_frame = NULL;

#else
	pp_vf->dec_frame =
		(ppmgr_device.bypass
		|| (cur_angle == 0
		&& ppmgr_device.mirror_flag == 0)
		|| ppmgr_device.use_prot) ? vf : NULL;
#endif

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
	if (mode)
		pp_vf->dec_frame = NULL;
#endif

	if (vf->type & VIDTYPE_MVC)
		pp_vf->dec_frame = vf;

	if (vf->type & VIDTYPE_COMPRESS)
		pp_vf->dec_frame = vf;

	if (pp_vf->dec_frame) {
		/* bypass mode */
		*new_vf = *vf;
		vfq_push(&q_ready, new_vf);
		return;
	}
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS
	platform_type = get_platform_type();
	if (platform_type == PLATFORM_TV)
		ret = ppmgr_buffer_init(1);
	else
		ret = ppmgr_buffer_init(0);
#else
	ret = ppmgr_buffer_init(0);
#endif
	if (ret < 0) {
		pp_vf->dec_frame = vf;
		*new_vf = *vf;
		vfq_push(&q_ready, new_vf);
		return;
	}
#ifdef INTERLACE_DROP_MODE
	if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM) {
		ppmgr_vf_put_dec(vf);
		vfq_push(&q_free, new_vf);
		return;
	}
	pp_vf->angle = cur_angle;
	if (interlace_mode)
		new_vf->duration = vf->duration * 2;
	else
		new_vf->duration = vf->duration;
#else
	pp_vf->angle = cur_angle;
	new_vf->duration = vf->duration;
#endif

	new_vf->duration_pulldown = vf->duration_pulldown;
	new_vf->pts = vf->pts;
	new_vf->pts_us64 = vf->pts_us64;
	new_vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
	new_vf->signal_type = vf->signal_type;
	new_vf->omx_index = vf->omx_index;
	new_vf->type = VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE
			| VIDTYPE_VIU_FIELD;
	new_vf->canvas0Addr = new_vf->canvas1Addr = index2canvas(pp_vf->index);
	new_vf->orientation = vf->orientation;
	new_vf->flag = vf->flag;

	if ((vf->source_type == VFRAME_SOURCE_TYPE_HDMI)
		|| (vf->source_type == VFRAME_SOURCE_TYPE_CVBS)) {
		if ((interlace_mode == VIDTYPE_INTERLACE_TOP)
			|| (interlace_mode == VIDTYPE_INTERLACE_BOTTOM))
			vf->height >>= 1;
	}

#ifndef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
	vf_rotate_adjust(vf, new_vf, cur_angle);
#else
	if (!mode) {
		vf_rotate_adjust(vf, new_vf, cur_angle);
		scale_clear_count = 0;
	} else {
		pp_vf->angle = vf->orientation % 4;
		cur_angle = (ppmgr_device.videoangle + vf->orientation) % 4;
		new_vf->width = ppmgr_device.disp_width;
		new_vf->height = ppmgr_device.disp_height;
		new_vf->ratio_control =
		DISP_RATIO_FORCECONFIG | DISP_RATIO_NO_KEEPRATIO;
		if ((rect_x != scaler_x)
				|| (rect_w != scaler_w)
				|| (rect_y != scaler_y)
				|| (rect_h != scaler_h)) {
			scale_clear_count = VF_POOL_SIZE;
			scaler_x = rect_x;
			scaler_y = rect_y;
			scaler_w = rect_w;
			scaler_h = rect_h;
			for (i = 0; i < VF_POOL_SIZE; i++) {
				buf_status[i].index = ppmgr_canvas_tab[i];
				buf_status[i].dirty = 1;
			}
			/* printk("--ppmgr new rect x:%d, y:%d,
			 * w:%d, h:%d.\n", rect_x, rect_y, rect_w, rect_h);
			 */
		}
		if ((!rect_w) || (!rect_h)) {
			/*printk("++ppmgr scale out of range 1.\n");*/
			ppmgr_vf_put_dec(vf);
			vfq_push(&q_free, new_vf);
			return;
		}
		if (((rect_x + rect_w) < 0) || (rect_x >= (int)new_vf->width)
				|| ((rect_y + rect_h) < 0)
				|| (rect_y >= (int)new_vf->height)) {
			/*printk("++ppmgr scale out of range 2.\n");*/
			ppmgr_vf_put_dec(vf);
			vfq_push(&q_free, new_vf);
			return;
		}
	}

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));

	for (i = 0; i < VF_POOL_SIZE; i++) {
		if (buf_status[i].index == new_vf->canvas0Addr)
			break;
	}

	if (buf_status[i].dirty == 1) {
		buf_status[i].dirty = 0;

		ge2d_config->alu_const_color = 0;/*0x000000ff;*/
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/*0xff;*/
		ge2d_config->dst_xy_swap = 0;

		canvas_read(new_vf->canvas0Addr & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = new_vf->canvas0Addr;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = new_vf->width;
		ge2d_config->src_para.height = new_vf->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = new_vf->canvas0Addr;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = new_vf->width;
		ge2d_config->dst_para.height = new_vf->height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			PPMGRVPP_ERR("++ge2d configing error.\n");
			ppmgr_vf_put_dec(vf);
			vfq_push(&q_free, new_vf);
			return;
		}
		fillrect(context, 0, 0,
				new_vf->width, new_vf->height, 0x008080ff);
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}

	if ((backup_index > 0) && (mode)) {
		unsigned int dst_w = vf->width, dst_h = vf->height;

		if ((dst_w > ppmgr_device.disp_width)
				|| (dst_h > ppmgr_device.disp_height)) {
			if ((dst_w * ppmgr_device.disp_height)
					> (dst_h * ppmgr_device.disp_width)) {
				dst_h = (dst_w * ppmgr_device.disp_height)
						/ ppmgr_device.disp_width;
				dst_w = ppmgr_device.disp_width;
			} else {
				dst_w = (dst_h * ppmgr_device.disp_width)
						/ ppmgr_device.disp_height;
				dst_h = ppmgr_device.disp_height;
			}
			dst_w = dst_w & (0xfffffffe);
			dst_h = dst_h & (0xfffffffe);
		}
		ge2d_config->alu_const_color = 0;/*0x000000ff;*/
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/*0xff;*/
		ge2d_config->dst_xy_swap = 0;

		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;

		canvas_read(new_vf->canvas0Addr & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->src_para.format = get_input_format(vf);
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0xffffffff;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = vf->width;
		ge2d_config->src_para.height = vf->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = backup_index;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_xy_swap = 0;

		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = new_vf->width;
		ge2d_config->dst_para.height = new_vf->height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			PPMGRVPP_ERR("++ge2d configing error.\n");
			ppmgr_vf_put_dec(vf);
			vfq_push(&q_free, new_vf);
			return;
		}
		stretchblt_noalpha(context, 0, 0, vf->width,
				vf->height, 0, 0, dst_w,
				dst_h);

		backup_content_w = dst_w;
		backup_content_h = dst_h;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
		/* printk("--ppmgr: backup data size: content:%d*%d\n",
		 * backup_content_w,backup_content_h);
		 */
	}
#endif

	/* data operating. */
	ge2d_config->alu_const_color = 0;/*0x000000ff;*/
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/*0xff;*/
	ge2d_config->dst_xy_swap = 0;

	canvas_read(new_vf->canvas0Addr & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;

	if (deinterlace) {
		switch (deinterlace) {
		case 1:
			canvas_read(PPMGR_DEINTERLACE_BUF_CANVAS, &cs0);
			canvas_read(PPMGR_DEINTERLACE_BUF_CANVAS + 1, &cs1);
			canvas_read(PPMGR_DEINTERLACE_BUF_CANVAS + 2, &cs2);
			ge2d_config->src_planes[0].addr = cs0.addr;
			ge2d_config->src_planes[0].w = cs0.width;
			ge2d_config->src_planes[0].h = cs0.height;
			ge2d_config->src_planes[1].addr = cs1.addr;
			ge2d_config->src_planes[1].w = cs1.width;
			ge2d_config->src_planes[1].h = cs1.height;
			ge2d_config->src_planes[2].addr = cs2.addr;
			ge2d_config->src_planes[2].w = cs2.width;
			ge2d_config->src_planes[2].h = cs2.height;
			ge2d_config->src_para.canvas_index =
				(PPMGR_DEINTERLACE_BUF_CANVAS)
				| ((PPMGR_DEINTERLACE_BUF_CANVAS + 1) << 8)
				| ((PPMGR_DEINTERLACE_BUF_CANVAS + 2) << 16);

			ge2d_config->src_para.format = GE2D_FORMAT_M24_YUV420;
			break;
		case 2:
			canvas_read(PPMGR_DEINTERLACE_BUF_NV21_CANVAS, &cs0);
			ge2d_config->src_planes[0].addr = cs0.addr;
			ge2d_config->src_planes[0].w = cs0.width;
			ge2d_config->src_planes[0].h = cs0.height;
			ge2d_config->src_para.canvas_index =
			PPMGR_DEINTERLACE_BUF_NV21_CANVAS;
			ge2d_config->src_para.format = GE2D_FORMAT_M24_YUV444;
			break;
		default:
			canvas_read(PPMGR_DEINTERLACE_BUF_CANVAS, &cs0);
			canvas_read(PPMGR_DEINTERLACE_BUF_CANVAS + 1, &cs1);
			canvas_read(PPMGR_DEINTERLACE_BUF_CANVAS + 2, &cs2);
			ge2d_config->src_planes[0].addr = cs0.addr;
			ge2d_config->src_planes[0].w = cs0.width;
			ge2d_config->src_planes[0].h = cs0.height;
			ge2d_config->src_planes[1].addr = cs1.addr;
			ge2d_config->src_planes[1].w = cs1.width;
			ge2d_config->src_planes[1].h = cs1.height;
			ge2d_config->src_planes[2].addr = cs2.addr;
			ge2d_config->src_planes[2].w = cs2.width;
			ge2d_config->src_planes[2].h = cs2.height;
			ge2d_config->src_para.canvas_index =
				(PPMGR_DEINTERLACE_BUF_CANVAS)
				| ((PPMGR_DEINTERLACE_BUF_CANVAS + 1) << 8)
				| ((PPMGR_DEINTERLACE_BUF_CANVAS + 2) << 16);
			ge2d_config->src_para.format = GE2D_FORMAT_M24_YUV420;
			break;
		}

	} else {

		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
		ge2d_config->src_para.format = get_input_format(vf);
	}
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height;

	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->dst_para.canvas_index = new_vf->canvas0Addr;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	/*ge2d_config->dst_para.mem_type = CANVAS_OSD0;*/
	/*ge2d_config->dst_para.format = GE2D_FORMAT_M24_YUV420;*/
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;

	if (cur_angle == 1) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev = 1;
	} else if (cur_angle == 2) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 1;
	} else if (cur_angle == 3) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev = 1;
	}
	if (ppmgr_device.mirror_flag != 0) {
		if (ppmgr_device.mirror_flag == 1) {
			if (cur_angle == 2 || cur_angle == 3)
				ge2d_config->dst_para.y_rev = 0;
			else
				ge2d_config->dst_para.y_rev = 1;
		} else if (ppmgr_device.mirror_flag == 2) {
			if (cur_angle == 1 || cur_angle == 2)
				ge2d_config->dst_para.x_rev = 0;
			else
				ge2d_config->dst_para.x_rev = 1;
		}
	}
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	if (ppmgr_device.receiver == 0) {
		ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
		ge2d_config->dst_para.width = new_vf->width;
		ge2d_config->dst_para.height = new_vf->height;
	} else {
		ge2d_config->dst_para.format = ppmgr_device.receiver_format;
		ge2d_config->dst_para.width = ppmgr_device.canvas_width;
		ge2d_config->dst_para.height = ppmgr_device.canvas_height;
		new_vf->width = ppmgr_device.canvas_width;
		new_vf->height = ppmgr_device.canvas_height;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_xy_swap = 0;
	}
	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		vfq_push(&q_free, new_vf);
		return;
	}

	pp_vf->angle = cur_angle;

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
	if (mode) {
		int sx, sy, sw, sh, dx, dy, dw, dh;
		unsigned int ratio_x = (vf->width << 8) / rect_w;
		unsigned int ratio_y = (vf->height << 8) / rect_h;

		if (rect_x < 0) {
			sx = ((0 - rect_x) * ratio_x) >> 8;
			sx = sx & (0xfffffffe);
			dx = 0;
		} else {
			sx = 0;
			dx = rect_x;
		}
		if ((rect_x + rect_w) > new_vf->width) {
			sw = ((rect_x + rect_w - new_vf->width)) * ratio_x >> 8;
			sw = vf->width - sx - sw;
			sw = sw & (0xfffffffe);
			if (rect_x < 0)
				dw = new_vf->width;
			else
				dw = new_vf->width - dx;
		} else {
			sw = vf->width - sx;
			sw = sw & (0xfffffffe);
			if (rect_x < 0)
				dw = rect_w + rect_x;
			else
				dw = rect_w;
		}
		if (rect_y < 0) {
			sy = ((0 - rect_y) * ratio_y) >> 8;
			sy = sy & (0xfffffffe);
			dy = 0;
		} else {
			sy = 0;
			dy = rect_y;
		}
		if ((rect_y + rect_h) > new_vf->height) {
			sh = ((rect_y + rect_h - new_vf->height))
					* ratio_y >> 8;

			sh = vf->height - sy - sh;
			sh = sh & (0xfffffffe);
			if (rect_y < 0)
				dh = new_vf->height;
			else
				dh = new_vf->height - dy;
		} else {
			sh = vf->height - sy;
			sh = sh & (0xfffffffe);
			if (rect_y < 0)
				dh = rect_h + rect_y;
			else
				dh = rect_h;
		}

		stretchblt_noalpha(context, sx, sy, sw,
				sh, dx, dy, dw, dh);
	} else {
		if (ppmgr_device.receiver == 0)
			stretchblt_noalpha(context, 0, 0, vf->width,
					vf->height, 0, 0,
					new_vf->width, new_vf->height);
		else
			display_mode_adjust(context, vf);
	}
#else
	stretchblt_noalpha(context, 0, 0,
			vf->width,
			vf->height,
			0, 0, new_vf->width, new_vf->height);

#endif
	ppmgr_vf_put_dec(vf);

	vfq_push(&q_ready, new_vf);

#ifdef DDD
	PPMGRVPP_WARN("rotate avail=%d, free=%d\n",
		vfq_level(&q_ready), vfq_level(&q_free));
#endif
	if ((!ppmgr_device.use_reserved) &&
	    (ppmgr_device.buffer_start)) {
		dma_flush(ppmgr_device.buffer_start, ppmgr_device.buffer_size);
	}
}

static void process_vf_change(struct vframe_s *vf,
		struct ge2d_context_s *context,
		struct config_para_ex_s *ge2d_config)
{
	struct vframe_s temp_vf;
	struct ppframe_s *pp_vf = to_ppframe(vf);
	struct canvas_s cs0, cs1, cs2, cd;
	int interlace_mode;
	unsigned int temp_angle = 0;
	unsigned int cur_angle = 0;
	int ret = 0;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS
	if (platform_type == PLATFORM_TV)
		ret = ppmgr_buffer_init(1);
	else
		ret = ppmgr_buffer_init(0);
#else
	ret = ppmgr_buffer_init(0);
#endif
	if (ret < 0)
		return;
	temp_vf.duration = vf->duration;
	temp_vf.duration_pulldown = vf->duration_pulldown;
	temp_vf.pts = vf->pts;
	temp_vf.pts_us64 = vf->pts_us64;
	temp_vf.flag = vf->flag;
	temp_vf.bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
	temp_vf.signal_type = vf->signal_type;
	temp_vf.omx_index = vf->omx_index;
	temp_vf.type = VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE
			| VIDTYPE_VIU_FIELD;
	temp_vf.canvas0Addr = temp_vf.canvas1Addr = ass_index;
	cur_angle = (ppmgr_device.videoangle + vf->orientation) % 4;
	temp_angle =
			(cur_angle >= pp_vf->angle) ?
			(cur_angle - pp_vf->angle) :
			(cur_angle + 4 - pp_vf->angle);

	pp_vf->angle = cur_angle;
	vf_rotate_adjust(vf, &temp_vf, temp_angle);

	interlace_mode = vf->type & VIDTYPE_TYPEMASK;

	if ((vf->source_type == VFRAME_SOURCE_TYPE_HDMI)
		|| (vf->source_type == VFRAME_SOURCE_TYPE_CVBS)) {
		if ((interlace_mode == VIDTYPE_INTERLACE_TOP)
			|| (interlace_mode == VIDTYPE_INTERLACE_BOTTOM))
			vf->height >>= 1;
	}

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	/* data operating. */
	ge2d_config->alu_const_color = 0;/*0x000000ff;*/
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/*0xff;*/
	ge2d_config->dst_xy_swap = 0;
	if (pp_vf->dec_frame) {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
		ge2d_config->src_para.format = get_input_format(vf);
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->src_para.format = get_input_format(vf);
	}

	canvas_read(temp_vf.canvas0Addr & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;

	ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height;

	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->dst_para.canvas_index = temp_vf.canvas0Addr;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;

	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = temp_vf.width;
	ge2d_config->dst_para.height = temp_vf.height;

	if (temp_angle == 1) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev = 1;
	} else if (temp_angle == 2) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 1;
	} else if (temp_angle == 3) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev = 1;
	}
	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		/*vfq_push(&q_free, new_vf);*/
		return;
	}

	stretchblt_noalpha(context, 0, 0, vf->width,
			vf->height,
			0, 0, temp_vf.width,
			temp_vf.height);

	vf->type = VIDTYPE_VIU_444
			| VIDTYPE_VIU_SINGLE_PLANE
			| VIDTYPE_VIU_FIELD;

	vf->canvas0Addr = vf->canvas1Addr = index2canvas(pp_vf->index);

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	/* data operating. */
	ge2d_config->alu_const_color = 0;/*0x000000ff;*/
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/*0xff;*/
	ge2d_config->dst_xy_swap = 0;

	canvas_read(temp_vf.canvas0Addr & 0xff, &cd);
	ge2d_config->src_planes[0].addr = cd.addr;
	ge2d_config->src_planes[0].w = cd.width;
	ge2d_config->src_planes[0].h = cd.height;
	ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444;

	canvas_read(vf->canvas0Addr & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;

	ge2d_config->src_para.canvas_index = temp_vf.canvas0Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = temp_vf.width;
	ge2d_config->src_para.height = temp_vf.height;

	vf->width = temp_vf.width;
	vf->height = temp_vf.height;
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->dst_para.canvas_index = vf->canvas0Addr;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = vf->width;
	ge2d_config->dst_para.height = vf->height;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		/*vfq_push(&q_free, new_vf);*/
		return;
	}
	stretchblt_noalpha(context, 0, 0, temp_vf.width, temp_vf.height, 0, 0,
			vf->width, vf->height);
	/*vf->duration = 0 ;*/
	if (pp_vf->dec_frame) {
		ppmgr_vf_put_dec(pp_vf->dec_frame);
		pp_vf->dec_frame = 0;
	}
	vf->ratio_control = 0;
}

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
static int process_vf_adjust(struct vframe_s *vf,
		struct ge2d_context_s *context,
		struct config_para_ex_s *ge2d_config)
{
	struct canvas_s cs, cd;
	int rect_x = 0, rect_y = 0, rect_w = 0, rect_h = 0;
	u32 ratio = 100;
	int sx, sy, sw, sh, dx, dy, dw, dh;
	unsigned int ratio_x;
	unsigned int ratio_y;
	int ret = 0;
	int i;
	struct ppframe_s *pp_vf = to_ppframe(vf);
	u32 mode = amvideo_get_scaler_para(&rect_x, &rect_y, &rect_w, &rect_h,
			&ratio);
	unsigned int cur_angle = pp_vf->angle;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS
	if (platform_type == PLATFORM_TV)
		ret = ppmgr_buffer_init(1);
	else
		ret = ppmgr_buffer_init(0);
#else
	ret = ppmgr_buffer_init(0);
#endif
	if (ret < 0)
		return -1;
	rect_w = max(rect_w, 64);
	rect_h = max(rect_h, 64);

	if (ppmgr_device.receiver != 0)
		mode = 0;


	if (!mode) {
		/*printk("--ppmgr adjust: scaler mode is disabled.\n");*/
		return -1;
	}

	if ((rect_x == scaler_x) && (rect_w == scaler_w) && (rect_y == scaler_y)
			&& (rect_h == scaler_h)) {
		/*printk("--ppmgr adjust: same pos. need not adjust.\n");*/
		return -1;
	}

	if ((!rect_w) || (!rect_h)) {
		/*printk("--ppmgr adjust: scale out of range 1.\n");*/
		return -1;
	}
	if (((rect_x + rect_w) < 0) || (rect_x >= (int)ppmgr_device.disp_width)
			|| ((rect_y + rect_h) < 0)
			|| (rect_y >= (int)ppmgr_device.disp_height)) {
		/*printk("--ppmgr adjust: scale out of range 2.\n");*/
		return -1;
	}
	if ((!backup_content_w) || (!backup_content_h)) {
		/*printk("--ppmgr adjust: scale out of range 3.\n");*/
		return -1;
	}

	scale_clear_count = VF_POOL_SIZE;
	for (i = 0; i < VF_POOL_SIZE; i++) {
		buf_status[i].index = ppmgr_canvas_tab[i];
		buf_status[i].dirty = 1;
	}
	scaler_x = rect_x;
	scaler_y = rect_y;
	scaler_w = rect_w;
	scaler_h = rect_h;

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	ge2d_config->alu_const_color = 0;/*0x000000ff;*/
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/*0xff;*/
	ge2d_config->dst_xy_swap = 0;

	canvas_read(ass_index & 0xff, &cd);
	ge2d_config->src_planes[0].addr = cd.addr;
	ge2d_config->src_planes[0].w = cd.width;
	ge2d_config->src_planes[0].h = cd.height;
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;

	ge2d_config->src_para.canvas_index = ass_index;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = ppmgr_device.disp_width;
	ge2d_config->src_para.height = ppmgr_device.disp_height;

	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->dst_para.canvas_index = ass_index;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = ppmgr_device.disp_width;
	ge2d_config->dst_para.height = ppmgr_device.disp_height;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		return -2;
	}
	fillrect(context, 0, 0,
			ppmgr_device.disp_width,
			ppmgr_device.disp_height,
			0x008080ff);

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	ge2d_config->alu_const_color = 0;/*0x000000ff;*/
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/*0xff;*/
	ge2d_config->dst_xy_swap = 0;

	canvas_read(backup_index & 0xff, &cs);
	ge2d_config->src_planes[0].addr = cs.addr;
	ge2d_config->src_planes[0].w = cs.width;
	ge2d_config->src_planes[0].h = cs.height;

	canvas_read(ass_index & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;

	ge2d_config->src_para.canvas_index = backup_index;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = ppmgr_device.disp_width;
	ge2d_config->src_para.height = ppmgr_device.disp_height;

	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->dst_para.canvas_index = ass_index;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;

	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = ppmgr_device.disp_width;
	ge2d_config->dst_para.height = ppmgr_device.disp_height;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		return -2;
	}

	ratio_x = (backup_content_w << 8) / rect_w;
	ratio_y = (backup_content_h << 8) / rect_h;

	if (rect_x < 0) {
		sx = ((0 - rect_x) * ratio_x) >> 8;
		sx = sx & (0xfffffffe);
		dx = 0;
	} else {
		sx = 0;
		dx = rect_x;
	}
	if ((rect_x + rect_w) > vf->width) {
		sw = ((rect_x + rect_w - vf->width)) * ratio_x >> 8;
		sw = backup_content_w - sx - sw;
		sw = sw & (0xfffffffe);
		if (rect_x < 0)
			dw = vf->width;
		else
			dw = vf->width - dx;
	} else {
		sw = backup_content_w - sx;
		sw = sw & (0xfffffffe);
		if (rect_x < 0)
			dw = rect_w + rect_x;
		else
			dw = rect_w;
	}

	if (cur_angle > 0) { /* for hdmi mode player*/
		sx = 0;
		dx = rect_x;
		sw = backup_content_w;
		dw = rect_w;
	}

	if (rect_y < 0) {
		sy = ((0 - rect_y) * ratio_y) >> 8;
		sy = sy & (0xfffffffe);
		dy = 0;
	} else {
		sy = 0;
		dy = rect_y;
	}
	if ((rect_y + rect_h) > vf->height) {
		sh = ((rect_y + rect_h - vf->height)) * ratio_y >> 8;
		sh = backup_content_h - sy - sh;
		sh = sh & (0xfffffffe);
		if (rect_y < 0)
			dh = vf->height;
		else
			dh = vf->height - dy;
	} else {
		sh = backup_content_h - sy;
		sh = sh & (0xfffffffe);
		if (rect_y < 0)
			dh = rect_h + rect_y;
		else
			dh = rect_h;
	}

	if (cur_angle > 0) { /* for hdmi mode player*/
		sy = 0;
		dy = rect_y;
		sh = backup_content_h;
		dh = rect_h;
	}
	/* printk("--ppmgr adjust: src x:%d, y:%d, w:%d, h:%d. dst x:%d, y:%d,
	 * w:%d, h:%d.\n", sx, sy, sw, sh,dx,dy,dw,dh);
	 */

	stretchblt_noalpha(context, sx, sy, sw, sh, dx, dy, dw, dh);

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	/* data operating. */
	ge2d_config->alu_const_color = 0;/*0x000000ff;*/
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/*0xff;*/
	ge2d_config->dst_xy_swap = 0;

	canvas_read(ass_index & 0xff, &cs);
	ge2d_config->src_planes[0].addr = cs.addr;
	ge2d_config->src_planes[0].w = cs.width;
	ge2d_config->src_planes[0].h = cs.height;

	canvas_read(vf->canvas0Addr & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;

	ge2d_config->src_para.canvas_index = ass_index;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = ppmgr_device.disp_width;
	ge2d_config->src_para.height = ppmgr_device.disp_height;

	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->dst_para.canvas_index = vf->canvas0Addr;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = vf->width;
	ge2d_config->dst_para.height = vf->height;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		return -2;
	}
	stretchblt_noalpha(context, 0, 0, ppmgr_device.disp_width,
			ppmgr_device.disp_height, 0, 0, vf->width, vf->height);
	return 0;
}
#endif

/* #ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS */
/* extern int is_mid_local_source(struct vframe_s *vf); */
/* extern int is_mid_mvc_need_process(struct vframe_s *vf); */
/* extern int get_mid_process_type(struct vframe_s *vf); */
/* extern void ppmgr_vf_3d( */
/* struct vframe_s *vf, */
/* struct ge2d_context_s *context, */
/* struct config_para_ex_s *ge2d_config); */
/*  */
/* extern int Init3DBuff(int canvas_id); */
/* extern void Reset3Dclear(void); */
/* extern void ppmgr_vf_3d_tv( */
/* struct vframe_s *vf, */
/* struct ge2d_context_s *context, */
/* struct config_para_ex_s *ge2d_config); */
/*  */
/* extern int get_tv_process_type(struct vframe_s *vf); */
/* #endif */
static struct task_struct *task;
/* extern struct vframe_s *get_cur_dispbuf(void); */
/* extern enum platform_type_t get_platform_type(void); */

static int ppmgr_task(void *data)
{
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};
	int i;
	struct vframe_s *vf_local = NULL;
	struct ppframe_s *pp_local = NULL;
	struct ge2d_context_s *context = create_ge2d_work_queue();
	struct config_para_ex_s ge2d_config;
#ifdef PPMGR_TB_DETECT
	bool first_frame = true;
	int first_frame_type = 0;
	unsigned int skip_picture = 0;
	u8 cur_invert = 0;
	u8 last_type = 0;
	u32 last_width = 0;
	u32 last_height = 0;
	u8 reset_tb = 0;
	u32 init_mute = 0;
#endif
	memset(&ge2d_config, 0, sizeof(struct config_para_ex_s));
	sched_setscheduler(current, SCHED_FIFO, &param);
	allow_signal(SIGTERM);
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS
	Reset3Dclear();
#endif
	while (down_interruptible(&thread_sem) == 0) {
		struct vframe_s *vf = NULL;

		if (kthread_should_stop() || ppmgr_quit_flag)
			break;

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
		if (get_scaler_pos_reset()) {
			set_scaler_pos_reset(false);
			scaler_w = 0;
			scaler_h = 0;
			scaler_x = 0;
			scaler_y = 0;
		}
		if (scaler_pos_changed) {
			scaler_pos_changed = 0;
			vf = get_cur_dispbuf();
			if (!is_valid_ppframe(to_ppframe(vf)))
				continue;
			if (vf && (vf->type & VIDTYPE_COMPRESS))
				continue;

			if (vf) {
				if (process_vf_adjust(vf,
						context,
						&ge2d_config) >= 0)
					EnableVideoLayer();
			}

			vf = vfq_peek(&q_ready);
			while (vf) {
				vf = vfq_pop(&q_ready);
				ppmgr_vf_put(vf, NULL);
				vf = vfq_peek(&q_ready);
			}

			up(&thread_sem);
			continue;
		}
#endif
		if (still_picture_notify) {
			still_picture_notify = 0;
			/* DisableVideoLayer(); */

			vf = get_cur_dispbuf();
			if (!is_valid_ppframe(to_ppframe(vf)))
				continue;
			if (vf->type & VIDTYPE_COMPRESS)
				continue;

			process_vf_change(vf, context, &ge2d_config);
			vf_notify_receiver(
				PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_PROPERTY_CHANGED,
				NULL);
			vfq_lookup_start(&q_ready);
			vf = vfq_peek(&q_ready);

			while (vf) {
				vf = vfq_pop(&q_ready);
				process_vf_change(vf, context, &ge2d_config);
				vf = vfq_peek(&q_ready);
			}
			vfq_lookup_end(&q_ready);
			/* EnableVideoLayer(); */
			up(&thread_sem);
			continue;
		}

		/* process when we have both input and output space */
		while (ppmgr_vf_peek_dec()
				&& (!vfq_empty(&q_free))
				&& (!ppmgr_blocking)) {
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS
			int process_type = TYPE_NONE;
			enum platform_type_t platform_type;

			vf = ppmgr_vf_get_dec();
			if (!vf)
				break;
			if (vf && ppmgr_device.started) {
				if (!(vf->type
					& (VIDTYPE_VIU_422
						| VIDTYPE_VIU_444
						| VIDTYPE_VIU_NV21))
					|| (vf->type & VIDTYPE_INTERLACE)
					|| ppmgr_device.disable_prot
					|| (vf->type & VIDTYPE_MVC)

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
						|| amvideo_get_scaler_mode()
#endif
#ifdef CONFIG_V4L_AMLOGIC_VIDEO
						|| ppmgr_device.receiver
#endif
				) {
					ppmgr_device.use_prot = 0;
					set_video_angle(0);

					ppmgr_device.videoangle =
						(ppmgr_device.angle
						+ ppmgr_device.orientation) % 4;
					set_property_change(1);
				} else {
					ppmgr_device.use_prot = 1;
					/*ppmgr_device.angle = 0;*/
					ppmgr_device.videoangle =
						(ppmgr_device.angle
						+ ppmgr_device.orientation) % 4;

					set_property_change(1);

				}
				ppmgr_device.started = 0;
			}
			vf->video_angle =
					(ppmgr_device.angle
					+ ppmgr_device.orientation
					+ vf->orientation)%4;

			platform_type = get_platform_type();
			if (platform_type == PLATFORM_TV)
				process_type = get_tv_process_type(vf);
			else
				process_type = get_mid_process_type(vf);

			if (process_type == TYPE_NONE) {
				int ret = 0;

				if (platform_type != PLATFORM_TV)
					ret = process_vf_deinterlace(vf,
							context,
							&ge2d_config);

				process_vf_rotate(vf,
						context,
						&ge2d_config,
						(ret > 0)?ret:0);

			} else {
				if (platform_type == PLATFORM_TV)
					ppmgr_vf_3d_tv(vf,
							context, &ge2d_config);
				else
					ppmgr_vf_3d(vf, context, &ge2d_config);
			}
#else
			int ret = 0;

			vf = ppmgr_vf_get_dec();
			if (!vf)
				break;
			if (vf && ppmgr_device.started) {
				if (!(vf->type
					& (VIDTYPE_VIU_422
							| VIDTYPE_VIU_444
							| VIDTYPE_VIU_NV21))
					|| (vf->type & VIDTYPE_INTERLACE)
					|| ppmgr_device.disable_prot
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
						|| amvideo_get_scaler_mode()
#endif
#ifdef CONFIG_V4L_AMLOGIC_VIDEO
						|| ppmgr_device.receiver
#endif
						) {
					ppmgr_device.use_prot = 0;
					set_video_angle(0);

					ppmgr_device.videoangle =
						(ppmgr_device.angle
						+ ppmgr_device.orientation) % 4;

					set_property_change(1);
				} else {
					ppmgr_device.use_prot = 1;
					set_video_angle(
						ppmgr_device.global_angle);

					ppmgr_device.videoangle =
						(ppmgr_device.angle
						+ ppmgr_device.orientation) % 4;
					set_property_change(1);
				}
				ppmgr_device.started = 0;
			}
			vf->video_angle = (
					ppmgr_device.angle
					+ ppmgr_device.orientation
					+ vf->orientation) % 4;
#ifdef PPMGR_TB_DETECT
			if (vf->source_type !=
				VFRAME_SOURCE_TYPE_OTHERS)
				goto SKIP_DETECT;
			if ((vf->width * vf->height)
				>= (3840 * 2160)) {          //4k do not detect
				goto SKIP_DETECT;
			}
			if (first_frame) {
				last_type = vf->type & VIDTYPE_TYPEMASK;
				last_width = vf->width;
				last_height = vf->height;
				first_frame_type = last_type;
				tb_first_frame_type = last_type;
				first_frame = false;
				reset_tb = 0;
				skip_picture = 0;
				cur_invert = 0;
				init_mute = tb_init_mute;
				atomic_set(&tb_skip_flag, 1);
				atomic_set(&tb_reset_flag, 0);
				if (ppmgr_device.tb_detect & 0xe)
					PPMGRVPP_INFO(
					"tb first frame type: %d\n",
					last_type);
			} else if ((last_type ==
				(vf->type & VIDTYPE_TYPEMASK))
				&& last_type) {
				/* interlace seq changed */
				first_frame_type =
					vf->type & VIDTYPE_TYPEMASK;
				tb_first_frame_type =
					first_frame_type;
				reset_tb = 1;
				/* keep old invert */
				if (ppmgr_device.tb_detect & 0xe)
					PPMGRVPP_INFO(
					"tb interlace seq change, old: %d, new: %d, invert: %d\n",
					last_type,
					first_frame_type,
					cur_invert);
			} else if ((last_type == 0) &&
				((vf->type & VIDTYPE_TYPEMASK) != 0)) {
				/* prog -> interlace changed */
				first_frame_type =
					vf->type & VIDTYPE_TYPEMASK;
				tb_first_frame_type =
					first_frame_type;
				reset_tb = 1;
				if (ppmgr_device.tb_detect & 0xe)
					PPMGRVPP_INFO(
					"tb prog -> interlace, new type: %d, invert: %d\n",
					first_frame_type, cur_invert);
				/* not invert */
				cur_invert = 0;
			} else if (((last_width != vf->width)
				|| (last_height != vf->height))
				&& ((vf->type & VIDTYPE_TYPEMASK) != 0)) {
				/* size changed and next seq is interlace */
				first_frame_type =
					vf->type & VIDTYPE_TYPEMASK;
				tb_first_frame_type =
					first_frame_type;
				reset_tb = 1;
				/* keep old invert */
				if (ppmgr_device.tb_detect & 0xe)
					PPMGRVPP_INFO(
					"tb size change new type: %d, invert: %d\n",
					first_frame_type, cur_invert);
			} else if ((last_type != 0) &&
				((vf->type & VIDTYPE_TYPEMASK) == 0)) {
				/* interlace -> prog changed */
				if (ppmgr_device.tb_detect & 0xe)
					PPMGRVPP_INFO(
					"tb interlace -> prog, invert: %d\n",
					cur_invert);
				/* not invert */
				cur_invert = 0;
			}
			last_type = vf->type & VIDTYPE_TYPEMASK;
			last_width = vf->width;
			last_height = vf->height;
			if (ppmgr_device.tb_detect) {
				ret = 0;
				if (tb_buffer_status < 0)
					goto SKIP_DETECT;
				if (tb_buffer_status == 0)
					if (tb_buffer_init() <= 0)
						goto SKIP_DETECT;
				ppmgr_device.tb_detect_buf_len = tb_buffer_len;
				vf->type = (vf->type & ~TB_DETECT_MASK);
				if (init_mute > 0) {
					init_mute--;
					atomic_set(&tb_skip_flag, 1);
					goto SKIP_DETECT;
				}
				if (last_type == 0) {/* cur type is prog */
					skip_picture++;
					cur_invert = 0;
					goto SKIP_DETECT;
				}
				vf->type |=
					cur_invert <<
					TB_DETECT_MASK_BIT;
				if (reset_tb) {
					/* wait tb task done */
					while ((tb_buff_wptr >= 5)
						&& (tb_buff_rptr
						<= tb_buff_wptr - 5)
						&& (atomic_read(&tb_run_flag)
						== 1))
						usleep_range(
							4000, 5000);
					atomic_set(&detect_status,
						tb_idle);
					tb_buff_wptr = 0;
					tb_buff_rptr = 0;
					atomic_set(
						&tb_detect_flag,
						TB_DETECT_NC);
					atomic_set(&tb_reset_flag, 1);
					atomic_set(&tb_skip_flag, 1);
					skip_picture = 0;
					reset_tb  = 0;
					if (ppmgr_device.tb_detect & 0xc)
						PPMGRVPP_INFO(
						"tb detect reset once\n");
				}
				if ((atomic_read(&detect_status) == tb_done)
					&& (skip_picture >=
					ppmgr_device.tb_detect_period)
					&& (last_type == first_frame_type)) {
					int tbf_flag =
						atomic_read(&tb_detect_flag);
					u8 old_invert = cur_invert;

					atomic_set(&detect_status, tb_idle);
					tb_buff_wptr = 0;
					tb_buff_rptr = 0;
					skip_picture = 0;
					if ((tbf_flag == TB_DETECT_TBF)
						&& (first_frame_type ==
						VIDTYPE_INTERLACE_TOP)) {
						/* TBF sams as BFF */
						vf->type |=
							TB_DETECT_INVERT
							<< TB_DETECT_MASK_BIT;
						cur_invert = 1;
					} else if ((tbf_flag == TB_DETECT_BFF)
						&& (first_frame_type ==
						VIDTYPE_INTERLACE_TOP)) {
						vf->type |=
							TB_DETECT_INVERT
							<< TB_DETECT_MASK_BIT;
						cur_invert = 1;
					} else if ((tbf_flag == TB_DETECT_TFF)
						&& (first_frame_type ==
						VIDTYPE_INTERLACE_BOTTOM)) {
						vf->type |=
							TB_DETECT_INVERT
							<< TB_DETECT_MASK_BIT;
						cur_invert = 1;
					} else if (tbf_flag != TB_DETECT_NC) {
						cur_invert = 0;
					}
					vf->type = (vf->type & ~TB_DETECT_MASK);
					vf->type |=
						cur_invert <<
						TB_DETECT_MASK_BIT;
					if ((old_invert != cur_invert)
						&& (ppmgr_device.tb_detect
						& 0xe))
						PPMGRVPP_INFO(
						"tb detect flag: %d->%d, invert: %d->%d\n",
						tb_detect_last_flag,
						tbf_flag,
						old_invert,
						cur_invert);
					else if ((tb_detect_last_flag
						!= tbf_flag)
						&& (ppmgr_device.tb_detect
						& 0xc))
						PPMGRVPP_INFO(
						"tb detect flag %d->%d, invert: %d\n",
						tb_detect_last_flag,
						tbf_flag,
						cur_invert);
					tb_detect_last_flag = tbf_flag;
					atomic_set(&tb_detect_flag,
						TB_DETECT_NC);
				}
				if ((tb_buff_wptr == 0) &&
					(last_type != first_frame_type)) {
					skip_picture++;
					atomic_set(&tb_skip_flag, 1);
					if (ppmgr_device.tb_detect & 0xc)
						PPMGRVPP_INFO(
						"tb detect skip case1\n");
					goto SKIP_DETECT;
				}
				if ((tb_buff_wptr < tb_buffer_len)
					&& (atomic_read(&tb_run_flag)
						== 1)) {
					ret = process_vf_tb_detect(
						vf, context, &ge2d_config);
				} else {
					if (ppmgr_device.tb_detect & 0xc)
						PPMGRVPP_INFO(
						"tb detect skip case2\n");
					atomic_set(&tb_skip_flag, 1);
					skip_picture++;
				}
				if (ret > 0) {
					tb_buff_wptr++;
					if ((tb_buff_wptr >= 5) &&
						(atomic_read(&detect_status)
						== tb_idle))
						atomic_set(
							&detect_status,
							tb_running);
					if (tb_buff_wptr >= 5)
						up(&tb_sem);
				}
			} else {
				reset_tb = 1;
				skip_picture++;
				cur_invert = 0;
				if (init_mute > 0)
					init_mute--;
			}
SKIP_DETECT:
			if (skip_picture >
				ppmgr_device.tb_detect_period)
				skip_picture =
					ppmgr_device.tb_detect_period;
#endif
			ret = process_vf_deinterlace(vf, context, &ge2d_config);
			process_vf_rotate(
					vf, context,
					&ge2d_config,
					(ret > 0) ? ret : 0);
#endif
			vf_notify_receiver(
				PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_VFRAME_READY,
				NULL);
		}

		if (ppmgr_blocking) {
#if 0
			if (ppmgr_reset_type) {
				vf_notify_provider(
					PROVIDER_NAME,
					VFRAME_EVENT_RECEIVER_RESET,
					NULL);
				ppmgr_reset_type = 0;
			}
#endif
			/***recycle buffer to decoder***/
		if (0) {/*mark@1120a*/
			for (i = 0; i < VF_POOL_SIZE; i++)
				ppmgr_vf_video_put(&vfp_pool[i]);

			vf_local = vfq_pop(&q_ready);
			while (vf_local) {
				pp_local = to_ppframe(vf_local);
				if (pp_local->dec_frame) {
					ppmgr_vf_put_dec(pp_local->dec_frame);
					pp_local->dec_frame = NULL;
				}
				vf_local = vfq_pop(&q_ready);
			}

			for (i = 0; i < VF_POOL_SIZE; i++) {
				if (vfp_pool[i].dec_frame) {
					ppmgr_vf_put_dec(vfp_pool[i].dec_frame);
					vfp_pool[i].dec_frame = NULL;
				}
			}
		}
			/***recycle buffer to decoder***/
			vf_local_init();
			vf_light_unreg_provider(&ppmgr_vf_prov);
			msleep(30);
			vf_light_reg_provider(&ppmgr_vf_prov);
			ppmgr_blocking = false;
			up(&thread_sem);
			PPMGRVPP_WARN("ppmgr rebuild from light-unregister\n");
		}

#ifdef DDD
		PPMGRVPP_WARN("process paused, dec %p, free %d, avail %d\n",
			ppmgr_vf_peek_dec(),
			vfq_level(&q_free),
			vfq_level(&q_ready));
#endif
	}

	destroy_ge2d_work_queue(context);
#ifdef PPMGR_TB_DETECT
	tb_buffer_uninit();
#endif
	ppmgr_buffer_uninit();
	while (!kthread_should_stop()) {
		/* may not call stop, wait..
		 * it is killed by SIGTERM,eixt on down_interruptible
		 * if not call stop,this thread may on do_exit and
		 * kthread_stop may not work good;
		 */
		/* msleep(10); */
		usleep_range(9000, 10000);
	}
	return 0;
}

/************************************************
 *
 *   init functions.
 *
 *************************************************/
static int vout_notify_callback(
		struct notifier_block *block, unsigned long cmd,
		void *para)
{
	if (cmd == VOUT_EVENT_MODE_CHANGE)
		ppmgr_device.vinfo = get_current_vinfo();

	return 0;
}

static struct notifier_block vout_notifier = {.notifier_call =
		vout_notify_callback, };
int ppmgr_register(void)
{
	vf_ppmgr_init_provider();
	vf_ppmgr_init_receiver();
	vf_ppmgr_reg_receiver();
	vout_register_client(&vout_notifier);
	return 0;
}

int ppmgr_buffer_uninit(void)
{
	if ((!ppmgr_device.use_reserved) &&
	    (ppmgr_device.buffer_start)) {
		PPMGRVPP_INFO("cma free addr is %x , size is  %x\n",
		(unsigned int)ppmgr_device.buffer_start,
		(unsigned int)ppmgr_device.buffer_size);
		codec_mm_free_for_dma(
		"ppmgr",
		ppmgr_device.buffer_start);
		ppmgr_device.buffer_start = 0;
		ppmgr_device.buffer_size = 0;
	}
	ppmgr_buffer_status = 0;
	return 0;
}

int ppmgr_buffer_init(int vout_mode)
{
	int i, j;
	u32 canvas_width, canvas_height;
	u32 decbuf_size;
	unsigned int buf_start;
	int buf_size;
	struct vinfo_s vinfo = {.width = 1280, .height = 720, };
	/* int flags = CODEC_MM_FLAGS_DMA; */
	int flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	switch (ppmgr_buffer_status) {
	case 0:/*not config*/
		break;
	case 1:/*config before , return ok*/
		return 0;
	case 2:/*config fail, won't retry , return failure*/
		return -1;
	default:
		return -1;
	}
	if (ppmgr_device.mirror_flag) {
		PPMGRVPP_INFO("CMA memory force config fail\n");
		ppmgr_buffer_status = 2;
		return -1;
	}
	if ((!ppmgr_device.use_reserved) &&
	    (ppmgr_device.buffer_start == 0)) {
		PPMGRVPP_INFO("reserved memory config fail,use CMA.\n");

		ppmgr_device.buffer_start = codec_mm_alloc_for_dma(
				"ppmgr",
				MM_ALLOC_SIZE/PAGE_SIZE, 0, flags);
		ppmgr_device.buffer_size = MM_ALLOC_SIZE;
		PPMGRVPP_INFO("cma memory is %x , size is  %x\n",
		(unsigned int)ppmgr_device.buffer_start,
		(unsigned int)ppmgr_device.buffer_size);
		if (ppmgr_device.buffer_start == 0) {
			ppmgr_buffer_status = 2;
			PPMGRVPP_ERR("cma memory config fail\n");
			return -1;
		}
	}
	ppmgr_buffer_status = 1;
	get_ppmgr_buf_info(&buf_start, &buf_size);
#ifdef CONFIG_V4L_AMLOGIC_VIDEO
	if (ppmgr_device.receiver != 0) {
		display_mode_change = VF_POOL_SIZE;
		canvas_width = ppmgr_device.canvas_width;
		canvas_height = ppmgr_device.canvas_height;
		if ((ppmgr_device.receiver_format
				== (GE2D_FORMAT_M24_NV21|GE2D_LITTLE_ENDIAN)) ||
				(ppmgr_device.receiver_format
				== (GE2D_FORMAT_M24_NV12|GE2D_LITTLE_ENDIAN))) {
			decbuf_size = (canvas_width * canvas_height * 12)>>3;
			decbuf_size = PAGE_ALIGN(decbuf_size);
			for (i = 0; i < VF_POOL_SIZE; i++) {
				canvas_config(PPMGR_CANVAS_INDEX + i,
					(ulong)(buf_start + i * decbuf_size),
					canvas_width, canvas_height,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);

				canvas_config(PPMGR_CANVAS_INDEX
					+ VF_POOL_SIZE + i,
					(ulong)(buf_start + i * decbuf_size
					+ (canvas_width * canvas_height)),
					canvas_width, canvas_height/2,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);

				ppmgr_canvas_tab[i] = (PPMGR_CANVAS_INDEX + i)
				| ((PPMGR_CANVAS_INDEX + i + VF_POOL_SIZE)<<8);

			}
		} else {
			int bpp = 4;

			if (ppmgr_device.receiver_format
				== (GE2D_FORMAT_S32_ABGR|GE2D_LITTLE_ENDIAN))
				bpp = 4;
			else if (ppmgr_device.receiver_format
				== (GE2D_FORMAT_S24_BGR|GE2D_LITTLE_ENDIAN))
				bpp = 3;
			else if (ppmgr_device.receiver_format
				== (GE2D_FORMAT_S16_RGB_565|GE2D_LITTLE_ENDIAN))
				bpp = 2;
			decbuf_size = canvas_width * canvas_height * bpp;
			/*decbuf_size = PAGE_ALIGN(decbuf_size);*/
			for (i = 0; i < VF_POOL_SIZE; i++) {
				canvas_config(PPMGR_CANVAS_INDEX + i,
					(ulong)(buf_start + i * decbuf_size),
					canvas_width * bpp, canvas_height,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);
				ppmgr_canvas_tab[i] = PPMGR_CANVAS_INDEX + i;
			}
		}
	} else
#endif
	if (vout_mode == 0) {
		ppmgr_device.vinfo = get_current_vinfo();
		if (ppmgr_device.vinfo == NULL) {
			PPMGRVPP_ERR(
			"failed to get_currnt_vinfo! Try to MAKE one!");

			ppmgr_device.vinfo = &vinfo;
		}

		if (ppmgr_device.disp_width == 0) {
			if (ppmgr_device.vinfo->width <= MAX_WIDTH)
				ppmgr_device.disp_width =
					ppmgr_device.vinfo->width;
			else
				ppmgr_device.disp_width = MAX_WIDTH;
		}

		if (ppmgr_device.disp_height == 0) {
			if (ppmgr_device.vinfo->height <= MAX_HEIGHT)
				ppmgr_device.disp_height =
						ppmgr_device.vinfo->height;
			else
				ppmgr_device.disp_height = MAX_HEIGHT;
		}
		if (get_platform_type() == PLATFORM_MID_VERTICAL) {
			int DISP_SIZE =
				ppmgr_device.disp_width >
					ppmgr_device.disp_height ?
					ppmgr_device.disp_width :
					ppmgr_device.disp_height;

			canvas_width = (DISP_SIZE + 0x1f) & ~0x1f;
			canvas_height = (DISP_SIZE + 0x1f) & ~0x1f;
		} else {
			canvas_width =
				(ppmgr_device.disp_width + 0x1f) & ~0x1f;
			canvas_height =
				(ppmgr_device.disp_height + 0x1f) & ~0x1f;
		}
		decbuf_size = canvas_width * canvas_height * 3;

		ppmgr_device.canvas_width = canvas_width;
		ppmgr_device.canvas_height = canvas_height;
		for (i = 0; i < VF_POOL_SIZE; i++) {
			canvas_config(PPMGR_CANVAS_INDEX + i,
				(ulong)(buf_start + i * decbuf_size),
				canvas_width * 3,
				canvas_height,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_32X32);
		}
		for (j = 0; j < ASS_POOL_SIZE; j++) {
			canvas_config(PPMGR_CANVAS_INDEX + VF_POOL_SIZE + j,
				(ulong)(buf_start
				+ (VF_POOL_SIZE + j) * decbuf_size),
				canvas_width * 3, canvas_height,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_32X32);
		}

		ass_index = PPMGR_CANVAS_INDEX + VF_POOL_SIZE;
		/*for rotate while pause status*/

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
		backup_index = PPMGR_CANVAS_INDEX + VF_POOL_SIZE + 1;
		/*for hdmi output*/
#endif

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS
		/* canvas mask buff*/
		canvas_config(PPMGR_CANVAS_INDEX
			+ VF_POOL_SIZE + ASS_POOL_SIZE,
			(ulong)(buf_start
			+ (VF_POOL_SIZE + ASS_POOL_SIZE) * decbuf_size),
			canvas_width, canvas_height,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);

		Init3DBuff(PPMGR_CANVAS_INDEX + VF_POOL_SIZE + ASS_POOL_SIZE);
#endif

		canvas_config(PPMGR_DEINTERLACE_BUF_CANVAS,
			(ulong)(buf_start
			+ (VF_POOL_SIZE + ASS_POOL_SIZE) * decbuf_size
			+ canvas_width * canvas_height * MASK_POOL_SIZE),
			canvas_width, canvas_height,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);

		canvas_config(PPMGR_DEINTERLACE_BUF_CANVAS + 1,
			(ulong)(buf_start
			+ (VF_POOL_SIZE + ASS_POOL_SIZE) * decbuf_size
			+ canvas_width * canvas_height * MASK_POOL_SIZE
			+ canvas_width * canvas_height), canvas_width >> 1,
			canvas_height >> 1,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);

		canvas_config(PPMGR_DEINTERLACE_BUF_CANVAS + 2,
				(ulong)(buf_start
				+ (VF_POOL_SIZE + ASS_POOL_SIZE) * decbuf_size
				+ canvas_width * canvas_height * MASK_POOL_SIZE
				+ (canvas_width * canvas_height * 5) / 4),
				canvas_width >> 1, canvas_height >> 1,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);

		canvas_config(PPMGR_DEINTERLACE_BUF_NV21_CANVAS,
			(ulong)(buf_start
			+ (VF_POOL_SIZE + ASS_POOL_SIZE) * decbuf_size
			+ canvas_width * canvas_height * MASK_POOL_SIZE),
			canvas_width * 3, canvas_height,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
	} else {

		canvas_width = 1920;
		canvas_height = 1088;
		decbuf_size = 0x600000;

		if (decbuf_size * VF_POOL_SIZE > buf_size) {
			PPMGRVPP_ERR(
					"size of ppmgr memory resource too small.\n");
			return -1;
		}

		for (i = 0; i < VF_POOL_SIZE; i++) {
			canvas_config(PPMGR_CANVAS_INDEX + i/**3*/+ 0,
					(ulong)(buf_start + i * decbuf_size),
					canvas_width * 3,
					canvas_height,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_32X32);

			canvas_config(PPMGR_DOUBLE_CANVAS_INDEX + i,
					(ulong)(buf_start +
					(2 * i) * decbuf_size / 2),
					canvas_width * 3, canvas_height / 2,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_32X32);

			canvas_config(PPMGR_DOUBLE_CANVAS_INDEX + 4 + i,
					(ulong)(buf_start +
					(2 * i + 1) *
					decbuf_size / 2),
					canvas_width * 3, canvas_height / 2,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_32X32);
		}

	}

	ppmgr_blocking = false;
	ppmgr_inited = true;
	ppmgr_reset_type = 0;
	set_buff_change(0);
	sema_init(&thread_sem, 1);
	return 0;

}

int start_ppmgr_task(void)
{
	enum platform_type_t platform_type;

	platform_type = get_platform_type();

	/*    if (get_cpu_type()>= MESON_CPU_TYPE_MESON6)*/
	/*	    switch_mod_gate_by_name("ge2d", 1);*/
	/*#endif*/
#ifdef PPMGR_TB_DETECT
	start_tb_task();
#endif
	if (!task) {
		vf_local_init();
		ppmgr_blocking = false;
		ppmgr_inited = true;
		ppmgr_reset_type = 0;
		set_buff_change(0);
		ppmgr_buffer_status = 0;
		ppmgr_quit_flag = false;
		task = kthread_run(ppmgr_task, 0, "ppmgr");
	}
	if (!IS_ERR_OR_NULL(task))
		task_running = 1;
	else
		task = NULL;
	return 0;
}

void stop_ppmgr_task(void)
{
#ifdef PPMGR_TB_DETECT
	stop_tb_task();
#endif
	if (!IS_ERR_OR_NULL(task)) {
		/* send_sig(SIGTERM, task, 1); */
		ppmgr_quit_flag = true;
		up(&thread_sem);
		kthread_stop(task);
		ppmgr_quit_flag = false;
		task = NULL;
	}
	task_running = 0;
	vf_local_init();
	/*#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6*/
	/*    switch_mod_gate_by_name("ge2d", 0);*/
	/*#endif*/
}

#ifdef PPMGR_TB_DETECT
static int tb_buffer_init(void)
{
	int i;
	//int flags = CODEC_MM_FLAGS_DMA_CPU | CODEC_MM_FLAGS_CMA_CLEAR;
	int flags = 0;

	if (tb_buffer_status)
		return tb_buffer_status;

	if (tb_src_canvas == 0) {
		if (canvas_pool_alloc_canvas_table(
			"tb_detect_src",
			&tb_src_canvas, 1,
			CANVAS_MAP_TYPE_YUV)) {
			pr_err(
				"%s alloc tb src canvas error.\n",
				__func__);
			return -1;
		}
		pr_info("alloc tb src canvas 0x%x.\n",
			tb_src_canvas);
	}

	if (tb_canvas < 0)
		tb_canvas =
			canvas_pool_map_alloc_canvas("tb_detect_dst");

	if (tb_canvas < 0)
		return -1;

	if (tb_buffer_start == 0) {
		if (!ppmgr_device.tb_detect_buf_len)
			ppmgr_device.tb_detect_buf_len = 8;
		tb_buffer_len = ppmgr_device.tb_detect_buf_len;
		tb_buffer_size = TB_DETECT_H * TB_DETECT_W
			* tb_buffer_len;
		tb_buffer_size = PAGE_ALIGN(tb_buffer_size);
		tb_buffer_start = codec_mm_alloc_for_dma(
			"tb_detect",
			tb_buffer_size/PAGE_SIZE, 0, flags);
		PPMGRVPP_INFO("tb cma memory %x, size %x, item %d\n",
			(unsigned int)tb_buffer_start,
			(unsigned int)tb_buffer_size,
			tb_buffer_len);
		if (tb_buffer_start == 0) {
			PPMGRVPP_ERR("tb cma memory config fail\n");
			tb_buffer_status = -1;
			return -1;
		}
		for (i = 0; i < tb_buffer_len; i++) {
			detect_buf[i].paddr = tb_buffer_start +
				TB_DETECT_H * TB_DETECT_W * i;
			detect_buf[i].vaddr =
				(ulong)codec_mm_vmap(detect_buf[i].paddr,
				TB_DETECT_H * TB_DETECT_W);
			if (ppmgr_device.tb_detect & 0xc) {
				PPMGRVPP_INFO(
					"detect buff(%d) paddr: %lx, vaddr: %lx\n",
					i,
					detect_buf[i].paddr,
					detect_buf[i].vaddr);
			}
		}
	}
	tb_buffer_status = 1;
	return 1;
}

static int tb_buffer_uninit(void)
{
	int i;
	if (tb_src_canvas) {
		if (tb_src_canvas & 0xff)
			canvas_pool_map_free_canvas(
				tb_src_canvas & 0xff);
		if ((tb_src_canvas >> 8) & 0xff)
			canvas_pool_map_free_canvas(
				(tb_src_canvas >> 8) & 0xff);
		if ((tb_src_canvas >> 16) & 0xff)
			canvas_pool_map_free_canvas(
				(tb_src_canvas >> 16) & 0xff);
	}
	tb_src_canvas = 0;

	if (tb_canvas >= 0)
		canvas_pool_map_free_canvas(tb_canvas);
	tb_canvas = -1;
	if (tb_buffer_start) {
		PPMGRVPP_INFO("tb cma free addr is %x, size is %x\n",
			(unsigned int)tb_buffer_start,
			(unsigned int)tb_buffer_size);
		for (i = 0; i < tb_buffer_len; i++) {
			if (detect_buf[i].vaddr) {
				codec_mm_unmap_phyaddr(
					(u8 *)detect_buf[i].vaddr);
				detect_buf[i].vaddr = 0;
			}
		}
		codec_mm_free_for_dma(
			"tb_detect",
			tb_buffer_start);
		tb_buffer_start = 0;
		tb_buffer_size = 0;
	}
	tb_buffer_status = 0;
	return 0;
}

static void tb_detect_init(void)
{
	int val = 0;

	sema_init(&tb_sem, val);
	memset(detect_buf, 0, sizeof(detect_buf));
	atomic_set(&detect_status, tb_idle);
	atomic_set(&tb_detect_flag, TB_DETECT_NC);
	atomic_set(&tb_reset_flag, 0);
	atomic_set(&tb_skip_flag, 0);
	atomic_set(&tb_run_flag, 1);
	tb_detect_last_flag = TB_DETECT_NC;
	tb_buff_wptr = 0;
	tb_buff_rptr = 0;
	tb_buffer_status = 0;
	tb_buffer_start = 0;
	tb_buffer_size = 0;
	tb_first_frame_type = 0;
	tb_quit_flag = false;
	tb_init_mute =
		ppmgr_device.tb_detect_init_mute;
}

static int tb_task(void *data)
{
	int tbff_flag;
	struct tbff_stats pReg;
	ulong y5fld[5];
	int is_top;
	int inited = 0;
	static const char * const detect_type[] = {"NC", "TFF", "BFF", "TBF"};
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};

	sched_setscheduler(current, SCHED_FIFO, &param);

	if (gfunc)
		gfunc->stats_init((&pReg), TB_DETECT_H, TB_DETECT_W);
	allow_signal(SIGTERM);
	while (down_interruptible(&tb_sem) == 0) {
		if (kthread_should_stop() || tb_quit_flag)
			break;
		if (tb_buff_rptr == 0) {
			if (atomic_read(&tb_reset_flag) != 0)
				inited = 0;
			atomic_set(&tb_reset_flag, 0);
			if (gfunc)
				gfunc->fwalg_init(inited);
		}
		inited = 1;
		is_top = (tb_buff_rptr & 1) ? 0 : 1;
		/* new -> old */
		y5fld[0] = detect_buf[tb_buff_rptr + 4].vaddr;
		y5fld[1] = detect_buf[tb_buff_rptr + 3].vaddr;
		y5fld[2] = detect_buf[tb_buff_rptr + 2].vaddr;
		y5fld[3] = detect_buf[tb_buff_rptr + 1].vaddr;
		y5fld[4] = detect_buf[tb_buff_rptr].vaddr;
		if (gfunc)
			gfunc->stats_get(y5fld, &pReg);

		is_top = is_top ^ 1;
		tbff_flag = -1;
		if (gfunc)
			tbff_flag = gfunc->fwalg_get(
				&pReg, is_top,
				(tb_first_frame_type == 3) ? 0 : 1,
				tb_buff_rptr,
				atomic_read(&tb_skip_flag),
				(ppmgr_device.tb_detect & 0x8) ? 1 : 0);

		if (tb_buff_rptr == 0)
			atomic_set(&tb_skip_flag, 0);

		if ((tbff_flag < -1) || (tbff_flag > 2)) {
			PPMGRVPP_ERR(
				"get tb detect flag error: %d\n",
				tbff_flag);
		}

		if ((tbff_flag == -1) && (gfunc))
			tbff_flag =
				gfunc->majority_get();

		if (tbff_flag == -1)
			tbff_flag =
				TB_DETECT_NC;
		else if (tbff_flag == 0)
			tbff_flag =
				TB_DETECT_TFF;
		else if (tbff_flag == 1)
			tbff_flag =
				TB_DETECT_BFF;
		else if (tbff_flag == 2)
			tbff_flag =
				TB_DETECT_TBF;
		else
			tbff_flag =
				TB_DETECT_NC;
		tb_buff_rptr++;
		if ((tb_buff_rptr > tb_buffer_len - 5)
			&& (atomic_read(&detect_status) == tb_running)) {
			atomic_set(
				&tb_detect_flag,
				tbff_flag);
			if (ppmgr_device.tb_detect & 0xc)
				PPMGRVPP_INFO("get tb detect final flag: %s\n",
					detect_type[tbff_flag]);
			atomic_set(&detect_status, tb_done);
		}
	}
	atomic_set(&tb_run_flag, 0);
	while (!kthread_should_stop())
		usleep_range(9000, 10000);
	return 0;
}

int start_tb_task(void)
{
	if (!tb_detect_task) {
		tb_detect_init();
		tb_detect_task = kthread_run(tb_task, 0, "tb_detect");
	}
	if (!IS_ERR_OR_NULL(tb_detect_task))
		tb_task_running = 1;
	else
		tb_detect_task = NULL;
	return 0;
}

void stop_tb_task(void)
{
	int val = 0;

	if (!IS_ERR_OR_NULL(tb_detect_task)) {
		tb_quit_flag = true;
		up(&tb_sem);
		/* send_sig(SIGTERM, tb_detect_task, 1); */
		kthread_stop(tb_detect_task);
		tb_quit_flag = false;
		sema_init(&tb_sem, val);
		tb_detect_task = NULL;
	}
	tb_task_running = 0;
}
#endif

void get_tb_detect_status(void)
{
#ifdef PPMGR_TB_DETECT
	static const char * const tb_type[] = {"Prog", "Top", "N/C", "Bottom"};
	static const char * const detect_type[] = {"NC", "TFF", "BFF", "TBF"};
	static const char * const status_str[] = {"Idle", "Run", "Done", "N/C"};
	u32 status = atomic_read(&detect_status);
	u32 flag = atomic_read(&tb_detect_flag);
	u32 reset_flag = atomic_read(&tb_reset_flag);

	PPMGRVPP_INFO(
		"T/B detect buffer addr: 0x%x, size: 0x%x\n",
		(unsigned int)tb_buffer_start,
		(unsigned int)tb_buffer_size);
	PPMGRVPP_INFO(
		"T/B detect canvas: %x, len: %d, buff status: %d\n",
		(unsigned int)tb_canvas,
		tb_buffer_len,
		(unsigned int)tb_buffer_status);
	PPMGRVPP_INFO(
		"T/B detect buffer wptr: %d, rptr: %d, reset: %d\n",
		tb_buff_wptr,
		tb_buff_rptr,
		reset_flag);
	PPMGRVPP_INFO(
		"T/B detect first frame type: %s, period: %d\n",
		tb_type[tb_first_frame_type],
		ppmgr_device.tb_detect_period);
	PPMGRVPP_INFO(
		"T/B detect status: %s, cur flag: %s, last flag: %s\n",
		status_str[status],
		detect_type[flag],
		detect_type[tb_detect_last_flag]);
	PPMGRVPP_INFO(
		"T/B detect init mute is %d\n",
		tb_init_mute);
	PPMGRVPP_INFO(
		"T/B detect tb_detect_task: %p, running: %d\n",
		tb_detect_task,
		tb_task_running);
	PPMGRVPP_INFO(
		"current T/B detect mode is %d\n",
		ppmgr_device.tb_detect);
#endif
}

int RegisterTB_Function(struct TB_DetectFuncPtr *func, const char *ver)
{
	int ret = -1;
#ifdef PPMGR_TB_DETECT
	mutex_lock(&tb_mutex);
	PPMGRVPP_INFO(
		"RegisterTB_Function: gfunc %p, func: %p, ver:%s\n",
		gfunc, func, ver);
	if (!gfunc && func) {
		gfunc = func;
		ret = 0;
	}
	mutex_unlock(&tb_mutex);
#endif
	return ret;
}
EXPORT_SYMBOL(RegisterTB_Function);

int UnRegisterTB_Function(struct TB_DetectFuncPtr *func)
{
	int ret = -1;
#ifdef PPMGR_TB_DETECT
	mutex_lock(&tb_mutex);
	PPMGRVPP_INFO(
		"UnRegisterTB_Function: gfunc %p, func: %p\n",
		gfunc, func);
	if (func && func == gfunc) {
		gfunc = NULL;
		ret = 0;
	}
	mutex_unlock(&tb_mutex);
#endif
	return ret;
}
EXPORT_SYMBOL(UnRegisterTB_Function);

