/*
 * drivers/amlogic/media/video_sink/video.c
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/amlogic/major.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/sched.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include "video_priv.h"

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include <linux/amlogic/media/utils/vdec_reg.h>

#ifdef CONFIG_PM
#include <linux/delay.h>
#include <linux/pm.h>
#endif

#include <linux/amlogic/media/registers/register.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "videolog.h"
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEOCAPTURE
#include "amvideocap_priv.h"
#endif
#ifdef CONFIG_AM_VIDEO_LOG
#define AMLOG
#endif
#include <linux/amlogic/media/utils/amlog.h>
MODULE_AMLOG(LOG_LEVEL_ERROR, 0, LOG_DEFAULT_LEVEL_DESC, LOG_MASK_DESC);

#include <linux/amlogic/media/video_sink/vpp.h>
#include "linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h"
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define DISPLAY_CANVAS_BASE_INDEX2   0x10
#define DISPLAY_CANVAS_MAX_INDEX2    0x15
#include "../common/rdma/rdma.h"
#endif
#include <linux/amlogic/media/video_sink/video_prot.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/codec_mm/configs.h>

#include "../common/vfm/vfm.h"
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>

static u32 osd_vpp_misc;
static u32 osd_vpp_misc_mask;
static bool update_osd_vpp_misc;
int video_vsync = -ENXIO;
/*global video manage cmd. */

static bool legacy_vpp = true;

#define DEBUG_TMP 0

static int video_global_output = 1;
/*  video_pause_global: 0 is play, 1 is pause, 2 is invalid */
static int video_pause_global = 1;

#ifdef CONFIG_GE2D_KEEP_FRAME
/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
/* #include <mach/mod_gate.h> */
/* #endif */
/* #include "mach/meson-secure.h" */
#endif

#if 1
/*TODO for logo*/
struct platform_resource_s {
	char name[10];
	int mem_start;
	int mem_end;
};
#endif
static int debugflags;
static int output_fps;
static u32 omx_pts;
static u32 omx_pts_set_index;
static bool omx_run;
static u32 omx_version = 3;
static int omx_pts_interval_upper = 11000;
static int omx_pts_interval_lower = -5500;
static int omx_pts_set_from_hwc_count;
static bool omx_check_previous_session;
static u32 omx_cur_session = 0xffffffff;
static int drop_frame_count;
#define OMX_MAX_COUNT_RESET_SYSTEMTIME 2
static int receive_frame_count;
static int display_frame_count;
static int omx_need_drop_frame_num;
static bool omx_drop_done;
static bool video_start_post;
static bool videopeek;

/*----omx_info  bit0: keep_last_frame, bit1~31: unused----*/
static u32 omx_info = 0x1;

#define ENABLE_UPDATE_HDR_FROM_USER 0
#if ENABLE_UPDATE_HDR_FROM_USER
static struct vframe_master_display_colour_s vf_hdr;
static bool has_hdr_info;
#endif
static DEFINE_MUTEX(omx_mutex);

#define DURATION_GCD 750

static bool bypass_cm;

static bool bypass_pps = true;
/*For 3D usage ----0:  mbx   1: tv */
bool platform_type = 1;

/* for bit depth setting. */
int bit_depth_flag = 8;

bool omx_secret_mode;
#define DEBUG_FLAG_FFPLAY	(1<<0)
#define DEBUG_FLAG_CALC_PTS_INC	(1<<1)

#define RECEIVER_NAME "amvideo"

static s32 amvideo_poll_major;
/*static s8 dolby_first_delay;*/ /* for bug 145902 */

static int video_receiver_event_fun(int type, void *data, void *);

static const struct vframe_receiver_op_s video_vf_receiver = {
	.event_cb = video_receiver_event_fun
};

static struct vframe_receiver_s video_vf_recv;

#define RECEIVER4OSD_NAME "amvideo4osd"
static int video4osd_receiver_event_fun(int type, void *data, void *);

static const struct vframe_receiver_op_s video4osd_vf_receiver = {
	.event_cb = video4osd_receiver_event_fun
};

static struct vframe_receiver_s video4osd_vf_recv;

static struct vframe_provider_s *osd_prov;

static struct device *amvideo_dev;
static struct device *amvideo_poll_dev;

#define DRIVER_NAME "amvideo"
#define MODULE_NAME "amvideo"
#define DEVICE_NAME "amvideo"

#ifdef CONFIG_AML_VSYNC_FIQ_ENABLE
#define FIQ_VSYNC
#endif

/* #define SLOW_SYNC_REPEAT */
/* #define INTERLACE_FIELD_MATCH_PROCESS */
bool disable_slow_sync;

#ifdef INTERLACE_FIELD_MATCH_PROCESS
#define FIELD_MATCH_THRESHOLD  10
static int field_matching_count;
#endif

#define M_PTS_SMOOTH_MAX 45000
#define M_PTS_SMOOTH_MIN 2250
#define M_PTS_SMOOTH_ADJUST 900
static u32 underflow;
static u32 next_peek_underflow;

#define VIDEO_ENABLE_STATE_IDLE       0
#define VIDEO_ENABLE_STATE_ON_REQ     1
#define VIDEO_ENABLE_STATE_ON_PENDING 2
#define VIDEO_ENABLE_STATE_OFF_REQ    3

static DEFINE_SPINLOCK(video_onoff_lock);
static int video_onoff_state = VIDEO_ENABLE_STATE_IDLE;
static u32 video_onoff_time;
static DEFINE_SPINLOCK(video2_onoff_lock);
static int video2_onoff_state = VIDEO_ENABLE_STATE_IDLE;
static u32 hdmiin_frame_check;
static u32 hdmiin_frame_check_cnt;

#ifdef FIQ_VSYNC
#define BRIDGE_IRQ INT_TIMER_C
#define BRIDGE_IRQ_SET() WRITE_CBUS_REG(ISA_TIMERC, 1)
#endif



#if 1	/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */

#define VD1_MEM_POWER_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&delay_work_lock, flags); \
		vpu_delay_work_flag &= ~VPU_DELAYWORK_MEM_POWER_OFF_VD1; \
		spin_unlock_irqrestore(&delay_work_lock, flags); \
		switch_vpu_mem_pd_vmod(VPU_VIU_VD1, VPU_MEM_POWER_ON); \
		switch_vpu_mem_pd_vmod(VPU_AFBC_DEC, VPU_MEM_POWER_ON); \
		switch_vpu_mem_pd_vmod(VPU_DI_POST, VPU_MEM_POWER_ON); \
		if (!legacy_vpp) \
			switch_vpu_mem_pd_vmod( \
				VPU_VD1_SCALE, VPU_MEM_POWER_ON); \
	} while (0)
#define VD2_MEM_POWER_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&delay_work_lock, flags); \
		vpu_delay_work_flag &= ~VPU_DELAYWORK_MEM_POWER_OFF_VD2; \
		spin_unlock_irqrestore(&delay_work_lock, flags); \
		switch_vpu_mem_pd_vmod(VPU_VIU_VD2, VPU_MEM_POWER_ON); \
		switch_vpu_mem_pd_vmod(VPU_AFBC_DEC1, VPU_MEM_POWER_ON); \
		if (!legacy_vpp) \
			switch_vpu_mem_pd_vmod( \
				VPU_VD2_SCALE, VPU_MEM_POWER_ON); \
	} while (0)
#define VD1_MEM_POWER_OFF() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&delay_work_lock, flags); \
		vpu_delay_work_flag |= VPU_DELAYWORK_MEM_POWER_OFF_VD1; \
		vpu_mem_power_off_count = VPU_MEM_POWEROFF_DELAY; \
		spin_unlock_irqrestore(&delay_work_lock, flags); \
	} while (0)
#define VD2_MEM_POWER_OFF() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&delay_work_lock, flags); \
		vpu_delay_work_flag |= VPU_DELAYWORK_MEM_POWER_OFF_VD2; \
		vpu_mem_power_off_count = VPU_MEM_POWEROFF_DELAY; \
		spin_unlock_irqrestore(&delay_work_lock, flags); \
	} while (0)
#else
#define VD1_MEM_POWER_ON()
#define VD2_MEM_POWER_ON()
#define PROT_MEM_POWER_ON()
#define VD1_MEM_POWER_OFF()
#define VD2_MEM_POWER_OFF()
#define PROT_MEM_POWER_OFF()
#endif

#define VIDEO_LAYER_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&video_onoff_lock, flags); \
		video_onoff_state = VIDEO_ENABLE_STATE_ON_REQ; \
		video_enabled = 1;\
		video_status_saved = 1;\
		spin_unlock_irqrestore(&video_onoff_lock, flags); \
	} while (0)

#define VIDEO_LAYER_OFF() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&video_onoff_lock, flags); \
		video_onoff_state = VIDEO_ENABLE_STATE_OFF_REQ; \
		video_enabled = 0;\
		video_status_saved = 0;\
		spin_unlock_irqrestore(&video_onoff_lock, flags); \
	} while (0)

#define VIDEO_LAYER2_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&video2_onoff_lock, flags); \
		video2_onoff_state = VIDEO_ENABLE_STATE_ON_REQ; \
		spin_unlock_irqrestore(&video2_onoff_lock, flags); \
	} while (0)

#define VIDEO_LAYER2_OFF() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&video2_onoff_lock, flags); \
		video2_onoff_state = VIDEO_ENABLE_STATE_OFF_REQ; \
		spin_unlock_irqrestore(&video2_onoff_lock, flags); \
	} while (0)

#define EnableVideoLayer()  \
	do { \
		VD1_MEM_POWER_ON(); \
		VIDEO_LAYER_ON(); \
	} while (0)

#if 0  /*TV_3D_FUNCTION_OPEN*/
#define EnableVideoLayer2()  \
	do { \
		VD2_MEM_POWER_ON(); \
		SET_VCBUS_REG_MASK(VPP_MISC + cur_dev->vpp_off, \
		VPP_VD2_PREBLEND | VPP_PREBLEND_EN | \
		(0x1ff << VPP_VD2_ALPHA_BIT)); \
	} while (0)
#else
#define EnableVideoLayer2()  \
	do { \
		VD2_MEM_POWER_ON(); \
		VIDEO_LAYER2_ON(); \
	} while (0)
#endif
#define VSYNC_EnableVideoLayer2()  \
	do { \
		VD2_MEM_POWER_ON(); \
		VSYNC_WR_MPEG_REG(VPP_MISC + cur_dev->vpp_off, \
		READ_VCBUS_REG(VPP_MISC + cur_dev->vpp_off) |\
		VPP_VD2_PREBLEND | (0x1ff << VPP_VD2_ALPHA_BIT)); \
	} while (0)

#define DisableVideoLayer() \
	do { \
		CLEAR_VCBUS_REG_MASK(VPP_MISC + cur_dev->vpp_off, \
		VPP_VD1_PREBLEND | VPP_VD2_PREBLEND|\
		VPP_VD2_POSTBLEND | VPP_VD1_POSTBLEND); \
		if (!legacy_vpp) { \
			WRITE_VCBUS_REG( \
			VD1_BLEND_SRC_CTRL + cur_dev->vpp_off, 0); \
		} \
		WRITE_VCBUS_REG(AFBC_ENABLE, 0);\
		VIDEO_LAYER_OFF(); \
		VD1_MEM_POWER_OFF(); \
		video_prot.video_started = 0; \
		if (debug_flag & DEBUG_FLAG_BLACKOUT) {  \
			pr_info("DisableVideoLayer()\n"); \
		} \
	} while (0)

#if 1	/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
#define DisableVideoLayer_NoDelay() \
	do { \
		CLEAR_VCBUS_REG_MASK(VPP_MISC + cur_dev->vpp_off, \
		VPP_VD1_PREBLEND | VPP_VD2_PREBLEND|\
		VPP_VD2_POSTBLEND | VPP_VD1_POSTBLEND); \
		if (!legacy_vpp) { \
			WRITE_VCBUS_REG( \
			VD1_BLEND_SRC_CTRL + cur_dev->vpp_off, 0); \
		} \
		WRITE_VCBUS_REG(AFBC_ENABLE, 0);\
		if (debug_flag & DEBUG_FLAG_BLACKOUT) {  \
			pr_info("DisableVideoLayer_NoDelay()\n"); \
		} \
	} while (0)
#else
#define DisableVideoLayer_NoDelay() DisableVideoLayer()
#endif
#if 0  /*TV_3D_FUNCTION_OPEN */
#define DisableVideoLayer2() \
	do { \
		CLEAR_VCBUS_REG_MASK(VPP_MISC + cur_dev->vpp_off, \
		VPP_VD2_PREBLEND | VPP_PREBLEND_EN | \
		(0x1ff << VPP_VD2_ALPHA_BIT)); \
		WRITE_VCBUS_REG(VD2_AFBC_ENABLE, 0); \
		VD2_MEM_POWER_OFF(); \
	} while (0)
#else
#define DisableVideoLayer2() \
	do { \
		CLEAR_VCBUS_REG_MASK(VPP_MISC + cur_dev->vpp_off, \
		VPP_VD2_POSTBLEND | VPP_VD2_PREBLEND | \
		(0x1ff << VPP_VD2_ALPHA_BIT)); \
		VIDEO_LAYER2_OFF(); \
		VD2_MEM_POWER_OFF(); \
	} while (0)
#endif
#define DisableVideoLayer_PREBELEND() \
	do { CLEAR_VCBUS_REG_MASK(VPP_MISC + cur_dev->vpp_off, \
		VPP_VD1_PREBLEND | VPP_VD2_PREBLEND); \
		WRITE_VCBUS_REG(AFBC_ENABLE, 0);\
		if (debug_flag & DEBUG_FLAG_BLACKOUT) {  \
			pr_info("DisableVideoLayer_PREBELEND()\n"); \
		} \
	} while (0)

#ifndef CONFIG_AM_VIDEO2
#define DisableVPP2VideoLayer() \
	CLEAR_VCBUS_REG_MASK(VPP2_MISC, \
		VPP_VD1_PREBLEND | VPP_VD2_PREBLEND | \
		VPP_VD2_POSTBLEND | VPP_VD1_POSTBLEND)

#endif
/*********************************************************/
#if DEBUG_TMP
static struct switch_dev video1_state_sdev = {
/* android video layer switch device */
	.name = "video_layer1",
};
#endif



#define MAX_ZOOM_RATIO 300

#if 1	/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */

#define VPP_PREBLEND_VD_V_END_LIMIT 2304
#else
#define VPP_PREBLEND_VD_V_END_LIMIT 1080
#endif

#define DUR2PTS(x) ((x) - ((x) >> 4))
#define DUR2PTS_RM(x) ((x) & 0xf)
#define PTS2DUR(x) (((x) << 4) / 15)

#ifdef VIDEO_PTS_CHASE
static int vpts_chase;
static int av_sync_flag;
static int vpts_chase_counter;
static int vpts_chase_pts_diff;
#endif


static int step_enable;
static int step_flag;


/*seek values on.video_define.h*/
static int debug_flag;
int get_video_debug_flags(void)
{
	return debug_flag;
}

/* DEBUG_FLAG_BLACKOUT; */

static int vsync_enter_line_max;
static int vsync_exit_line_max;

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
static int vsync_rdma_line_max;
#endif
static u32 framepacking_support __nosavedata;
static unsigned int framepacking_width = 1920;
static unsigned int framepacking_height = 2205;
static unsigned int framepacking_blank = 45;
static unsigned int process_3d_type;
static unsigned int last_process_3d_type;
#ifdef TV_3D_FUNCTION_OPEN
/* toggle_3d_fa_frame is for checking the vpts_expire  in 2 vsnyc */
static int toggle_3d_fa_frame = 1;
/*the pause_one_3d_fl_frame is for close*/
/*the A/B register switch in every sync at pause mode. */
static int pause_one_3d_fl_frame;
MODULE_PARM_DESC(pause_one_3d_fl_frame, "\n pause_one_3d_fl_frame\n");
module_param(pause_one_3d_fl_frame, uint, 0664);

/*debug info control for skip & repeate vframe case*/
static unsigned int video_dbg_vf;
MODULE_PARM_DESC(video_dbg_vf, "\n video_dbg_vf\n");
module_param(video_dbg_vf, uint, 0664);

static unsigned int video_get_vf_cnt;
static unsigned int video_drop_vf_cnt;
MODULE_PARM_DESC(video_drop_vf_cnt, "\n video_drop_vf_cnt\n");
module_param(video_drop_vf_cnt, uint, 0664);

enum toggle_out_fl_frame_e {
	OUT_FA_A_FRAME,
	OUT_FA_BANK_FRAME,
	OUT_FA_B_FRAME
};

static unsigned int video_3d_format;
static unsigned int mvc_flag;
static unsigned int force_3d_scaler = 3;
static int mode_3d_changed;
static int last_mode_3d;
#endif

#ifdef TV_REVERSE
bool reverse;
#endif

const char video_dev_id[] = "amvideo-dev";

const char video_dev_id2[] = "amvideo-dev2";

int onwaitendframe;

static u32 vpp_hold_line = 8;

struct video_dev_s video_dev[2] = {
	{0x1d00 - 0x1d00, 0x1a50 - 0x1a50},
	{0x1900 - 0x1d00, 0x1e00 - 0x1a50}
};

struct video_dev_s *cur_dev = &video_dev[0];
struct video_dev_s *get_video_cur_dev(void)
{
	return cur_dev;
}
static int cur_dev_idx;

#ifdef CONFIG_PM
struct video_pm_state_s {
	int event;
	u32 vpp_misc;
	int mem_pd_vd1;
	int mem_pd_vd2;
	int mem_pd_di_post;
	int mem_pd_prot2;
	int mem_pd_prot3;
};

#endif

#define PTS_LOGGING
#define PTS_THROTTLE
/* #define PTS_TRACE_DEBUG */
/* #define PTS_TRACE_START */

#ifdef PTS_TRACE_DEBUG
static int pts_trace_his[16];
static u32 pts_his[16];
static u32 scr_his[16];
static int pts_trace_his_rd;
#endif

#if defined(PTS_LOGGING) || defined(PTS_TRACE_DEBUG)
static int pts_trace;
#endif

#if defined(PTS_LOGGING)
#define PTS_32_PATTERN_DETECT_RANGE 10
#define PTS_22_PATTERN_DETECT_RANGE 10
#define PTS_41_PATTERN_DETECT_RANGE 2

enum video_refresh_pattern {
	PTS_32_PATTERN = 0,
	PTS_22_PATTERN,
	PTS_41_PATTERN,
	PTS_MAX_NUM_PATTERNS
};

int n_patterns = PTS_MAX_NUM_PATTERNS;

static int pts_pattern[3] = {0, 0, 0};
static int pts_pattern_enter_cnt[3] = {0, 0, 0};
static int pts_pattern_exit_cnt[3] = {0, 0, 0};
static int pts_log_enable[3] = {0, 0, 0};
static int pre_pts_trace;

#define PTS_41_PATTERN_SINK_MAX 4
static int pts_41_pattern_sink[PTS_41_PATTERN_SINK_MAX];
static int pts_41_pattern_sink_index;
static int pts_pattern_detected = -1;
static bool pts_enforce_pulldown = true;
#endif

static DEFINE_MUTEX(video_module_mutex);
static DEFINE_MUTEX(video_inuse_mutex);
static DEFINE_SPINLOCK(lock);
#if ENABLE_UPDATE_HDR_FROM_USER
static DEFINE_SPINLOCK(omx_hdr_lock);
#endif
static u32 frame_par_ready_to_set, frame_par_force_to_set;
static u32 vpts_remainder;
static int video_property_changed;
static u32 video_notify_flag;
static int enable_video_discontinue_report = 1;

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
static u32 video_scaler_mode;
static int content_top = 0, content_left = 0, content_w = 0, content_h;
static int scaler_pos_changed;
#endif

#ifndef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
bool is_dolby_vision_enable(void)
{
	return 0;
}
bool is_dolby_vision_on(void)
{
	return 0;
}
bool is_dolby_vision_stb_mode(void)
{
	return 0;
}
#endif

static struct amvideocap_req *capture_frame_req;
static struct video_prot_s video_prot;
static u32 video_angle;
u32 get_video_angle(void)
{
	return video_angle;
}
EXPORT_SYMBOL(get_video_angle);

/*for video related files only.*/
void video_module_lock(void)
{
	mutex_lock(&video_module_mutex);
}
void video_module_unlock(void)
{
	mutex_unlock(&video_module_mutex);
}

int video_property_notify(int flag)
{
	video_property_changed = flag;
	return 0;
}

#if defined(PTS_LOGGING)
static ssize_t pts_pattern_enter_cnt_read_file(struct file *file,
			char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[20];
	ssize_t len;

	len = snprintf(buf, 20, "%d,%d,%d\n", pts_pattern_enter_cnt[0],
			pts_pattern_enter_cnt[1], pts_pattern_enter_cnt[2]);
	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static ssize_t pts_pattern_exit_cnt_read_file(struct file *file,
			char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[20];
	ssize_t len;

	len = snprintf(buf, 20, "%d,%d,%d\n", pts_pattern_exit_cnt[0],
			pts_pattern_exit_cnt[1], pts_pattern_exit_cnt[2]);
	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static ssize_t pts_log_enable_read_file(struct file *file,
			char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[20];
	ssize_t len;

	len = snprintf(buf, 20, "%d,%d,%d\n", pts_log_enable[0],
			pts_log_enable[1], pts_log_enable[2]);
	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static ssize_t pts_log_enable_write_file(struct file *file,
			const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[20];
	int ret;

	count = min_t(size_t, count, (sizeof(buf)-1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;
	buf[count] = 0;
	/* pts_pattern_log_enable (3:2) (2:2) (4:1) */
	ret = sscanf(buf, "%d,%d,%d", &pts_log_enable[0], &pts_log_enable[1],
			&pts_log_enable[2]);
	if (ret != 3) {
		pr_info("use echo 0/1,0/1,0/1 > /sys/kernel/debug/pts_log_enable\n");
	} else {
		pr_info("pts_log_enable: %d,%d,%d\n", pts_log_enable[0],
			pts_log_enable[1], pts_log_enable[2]);
	}
	return count;
}
static ssize_t pts_enforce_pulldown_read_file(struct file *file,
			char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[16];
	ssize_t len;

	len = snprintf(buf, 16, "%d\n", pts_enforce_pulldown);
	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static ssize_t pts_enforce_pulldown_write_file(struct file *file,
			const char __user *userbuf, size_t count, loff_t *ppos)
{
	unsigned int write_val;
	char buf[16];
	int ret;

	count = min_t(size_t, count, (sizeof(buf)-1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;
	buf[count] = 0;
	ret = kstrtoint(buf, 0, &write_val);
	if (ret != 0)
		return -EINVAL;
	pr_info("pts_enforce_pulldown: %d->%d\n",
		pts_enforce_pulldown, write_val);
	pts_enforce_pulldown = write_val;
	return count;
}

static const struct file_operations pts_pattern_enter_cnt_file_ops = {
	.open		= simple_open,
	.read		= pts_pattern_enter_cnt_read_file,
};

static const struct file_operations pts_pattern_exit_cnt_file_ops = {
	.open		= simple_open,
	.read		= pts_pattern_exit_cnt_read_file,
};

static const struct file_operations pts_log_enable_file_ops = {
	.open		= simple_open,
	.read		= pts_log_enable_read_file,
	.write		= pts_log_enable_write_file,
};

static const struct file_operations pts_enforce_pulldown_file_ops = {
	.open		= simple_open,
	.read		= pts_enforce_pulldown_read_file,
	.write		= pts_enforce_pulldown_write_file,
};
#endif

struct video_debugfs_files_s {
	const char *name;
	const umode_t mode;
	const struct file_operations *fops;
};

static struct video_debugfs_files_s video_debugfs_files[] = {
#if defined(PTS_LOGGING)
	{"pts_pattern_enter_cnt", S_IFREG | 0444,
		&pts_pattern_enter_cnt_file_ops
	},
	{"pts_pattern_exit_cnt", S_IFREG | 0444,
		&pts_pattern_exit_cnt_file_ops
	},
	{"pts_log_enable", S_IFREG | 0644,
		&pts_log_enable_file_ops
	},
	{"pts_enforce_pulldown", S_IFREG | 0644,
		&pts_enforce_pulldown_file_ops
	},
#endif
};

static struct dentry *video_debugfs_root;
static void video_debugfs_init(void)
{
	struct dentry *ent;
	int i;

	if (video_debugfs_root)
		return;
	video_debugfs_root = debugfs_create_dir("video", NULL);
	if (!video_debugfs_root)
		pr_err("can't create video debugfs dir\n");

	for (i = 0; i < ARRAY_SIZE(video_debugfs_files); i++) {
		ent = debugfs_create_file(video_debugfs_files[i].name,
			video_debugfs_files[i].mode,
			video_debugfs_root, NULL,
			video_debugfs_files[i].fops);
		if (!ent)
			pr_info("debugfs create file %s failed\n",
				video_debugfs_files[i].name);
	}
}

static void video_debugfs_exit(void)
{
	debugfs_remove(video_debugfs_root);
}

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
int video_scaler_notify(int flag)
{
	video_scaler_mode = flag;
	video_property_changed = true;
	return 0;
}

u32 amvideo_get_scaler_para(int *x, int *y, int *w, int *h, u32 *ratio)
{
	*x = content_left;
	*y = content_top;
	*w = content_w;
	*h = content_h;
	/* *ratio = 100; */
	return video_scaler_mode;
}

void amvideo_set_scaler_para(int x, int y, int w, int h, int flag)
{
	mutex_lock(&video_module_mutex);
	if (w < 2)
		w = 0;
	if (h < 2)
		h = 0;
	if (flag) {
		if ((content_left != x) || (content_top != y)
		    || (content_w != w) || (content_h != h))
			scaler_pos_changed = 1;
		content_left = x;
		content_top = y;
		content_w = w;
		content_h = h;
	} else
		vpp_set_video_layer_position(x, y, w, h);
	video_property_changed = true;
	mutex_unlock(&video_module_mutex);
}

u32 amvideo_get_scaler_mode(void)
{
	return video_scaler_mode;
}
#endif

bool to_notify_trick_wait;
/* display canvas */
#define DISPLAY_CANVAS_BASE_INDEX 0x60

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
static struct vframe_s *cur_rdma_buf;
/*
 *void vsync_rdma_config(void);
 *void vsync_rdma_config_pre(void);
 *bool is_vsync_rdma_enable(void);
 *void start_rdma(void);
 *void enable_rdma_log(int flag);
 */
static int enable_rdma_log_count;

bool rdma_enable_pre;

u32 disp_canvas_index[2][6] = {
	{
		DISPLAY_CANVAS_BASE_INDEX,
		DISPLAY_CANVAS_BASE_INDEX + 1,
		DISPLAY_CANVAS_BASE_INDEX + 2,
		DISPLAY_CANVAS_BASE_INDEX + 3,
		DISPLAY_CANVAS_BASE_INDEX + 4,
		DISPLAY_CANVAS_BASE_INDEX + 5,
	},
	{
		DISPLAY_CANVAS_BASE_INDEX2,
		DISPLAY_CANVAS_BASE_INDEX2 + 1,
		DISPLAY_CANVAS_BASE_INDEX2 + 2,
		DISPLAY_CANVAS_BASE_INDEX2 + 3,
		DISPLAY_CANVAS_BASE_INDEX2 + 4,
		DISPLAY_CANVAS_BASE_INDEX2 + 5,
	}
};

static u32 disp_canvas[2][2];
static u32 rdma_canvas_id;
static u32 next_rdma_canvas_id = 1;

#define DISPBUF_TO_PUT_MAX  8
static struct vframe_s *dispbuf_to_put[DISPBUF_TO_PUT_MAX];
static int dispbuf_to_put_num;
#else
static u32 disp_canvas_index[6] = {
	DISPLAY_CANVAS_BASE_INDEX,
	DISPLAY_CANVAS_BASE_INDEX + 1,
	DISPLAY_CANVAS_BASE_INDEX + 2,
	DISPLAY_CANVAS_BASE_INDEX + 3,
	DISPLAY_CANVAS_BASE_INDEX + 4,
	DISPLAY_CANVAS_BASE_INDEX + 5,
};

static u32 disp_canvas[2];
#endif

static u32 post_canvas;


/* zoom information */
static u32 zoom_start_x_lines;
static u32 zoom_end_x_lines;
static u32 zoom_start_y_lines;
static u32 zoom_end_y_lines;

static u32 ori_start_x_lines;
static u32 ori_end_x_lines;
static u32 ori_start_y_lines;
static u32 ori_end_y_lines;

static u32 zoom2_start_x_lines;
static u32 zoom2_end_x_lines;
static u32 zoom2_start_y_lines;
static u32 zoom2_end_y_lines;

static u32 ori2_start_x_lines;
static u32 ori2_end_x_lines;
static u32 ori2_start_y_lines;
static u32 ori2_end_y_lines;

/* wide settings */
static u32 wide_setting;

/* black out policy */
#if defined(CONFIG_JPEGLOGO)
static u32 blackout;
#else
static u32 blackout = 1;
#endif
static u32 force_blackout;

/* disable video */
static u32 disable_video = VIDEO_DISABLE_NONE;
static u32 video_enabled __nosavedata;
static u32 video_status_saved __nosavedata;
u32 get_video_enabled(void)
{
	return video_enabled;
}
/* show first frame*/
static bool show_first_frame_nosync;
bool show_first_picture;
/* static bool first_frame=false; */

/* test screen*/
static u32 test_screen;
/* rgb screen*/
static u32 rgb_screen;

/* video frame repeat count */
static u32 frame_repeat_count;

/* vout */
static const struct vinfo_s *vinfo;

/* config */
static struct vframe_s *cur_dispbuf;
static struct vframe_s *cur_dispbuf2;
static bool need_disable_vd2;
void update_cur_dispbuf(void *buf)
{
	cur_dispbuf = buf;
}
int get_video0_frame_info(struct vframe_s *vf)
{
	unsigned long flags;
	int ret = -1;

	spin_lock_irqsave(&lock, flags);
	if (is_vpp_postblend() && cur_dispbuf && vf) {
		*vf = *cur_dispbuf;
		ret = 0;
	}
	spin_unlock_irqrestore(&lock, flags);
	return ret;
}
EXPORT_SYMBOL(get_video0_frame_info);

static struct vframe_s vf_local, vf_local2;
static u32 vsync_pts_inc;
static u32 vsync_pts_inc_scale;
static u32 vsync_pts_inc_scale_base = 1;
static u32 vsync_pts_inc_upint;
static u32 vsync_pts_inc_adj;
static u32 vsync_pts_125;
static u32 vsync_pts_112;
static u32 vsync_pts_101;
static u32 vsync_pts_100;
static u32 vsync_freerun;
/* extend this value to support both slow and fast playback
 * 0,1: normal playback
 * [2,1000]: speed/vsync_slow_factor
 * >1000: speed*(vsync_slow_factor/1000000)
 */
static u32 vsync_slow_factor = 1;

/* pts alignment */
static bool vsync_pts_aligned;
static s32 vsync_pts_align;

/* frame rate calculate */
static u32 last_frame_count;
static u32 frame_count;
static u32 new_frame_count;
static u32 first_frame_toggled;
static u32 toggle_count;
static u32 last_frame_time;
static u32 timer_count;
static u32 vsync_count;
static struct vpp_frame_par_s *cur_frame_par, *next_frame_par;
static struct vpp_frame_par_s frame_parms[2];

/* vsync pass flag */
static u32 wait_sync;

/* is fffb or seeking*/
static u32 video_seek_flag;

#ifdef FIQ_VSYNC
static bridge_item_t vsync_fiq_bridge;
#endif

/* trickmode i frame*/
u32 trickmode_i;
EXPORT_SYMBOL(trickmode_i);

/* trickmode ff/fb */
u32 trickmode_fffb;
atomic_t trickmode_framedone = ATOMIC_INIT(0);
atomic_t video_sizechange = ATOMIC_INIT(0);
atomic_t video_unreg_flag = ATOMIC_INIT(0);
atomic_t video_inirq_flag = ATOMIC_INIT(0);
atomic_t video_pause_flag = ATOMIC_INIT(0);
int trickmode_duration;
int trickmode_duration_count;
u32 trickmode_vpts;
/* last_playback_filename */
char file_name[512];

/* video freerun mode */
#define FREERUN_NONE    0	/* no freerun mode */
#define FREERUN_NODUR   1	/* freerun without duration */
#define FREERUN_DUR     2	/* freerun with duration */
static u32 freerun_mode;
static u32 slowsync_repeat_enable;
static bool dmc_adjust = true;
module_param_named(dmc_adjust, dmc_adjust, bool, 0644);
static u32 dmc_config_state;
static u32 last_toggle_count;
static u32 toggle_same_count;

/* video_inuse */
static u32 video_inuse;

void set_freerun_mode(int mode)
{
	freerun_mode = mode;
}
EXPORT_SYMBOL(set_freerun_mode);

void set_pts_realign(void)
{
	vsync_pts_aligned = false;
}
EXPORT_SYMBOL(set_pts_realign);

static const enum f2v_vphase_type_e vpp_phase_table[4][3] = {
	{F2V_P2IT, F2V_P2IB, F2V_P2P},	/* VIDTYPE_PROGRESSIVE */
	{F2V_IT2IT, F2V_IT2IB, F2V_IT2P},	/* VIDTYPE_INTERLACE_TOP */
	{F2V_P2IT, F2V_P2IB, F2V_P2P},
	{F2V_IB2IT, F2V_IB2IB, F2V_IB2P}	/* VIDTYPE_INTERLACE_BOTTOM */
};

static const u8 skip_tab[6] = { 0x24, 0x04, 0x68, 0x48, 0x28, 0x08 };

/* wait queue for poll */
static wait_queue_head_t amvideo_trick_wait;

/* wait queue for poll */
static wait_queue_head_t amvideo_sizechange_wait;

#if 1				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
#define VPU_DELAYWORK_VPU_CLK            1
#define VPU_DELAYWORK_MEM_POWER_OFF_VD1  2
#define VPU_DELAYWORK_MEM_POWER_OFF_VD2  4
#define VPU_DELAYWORK_MEM_POWER_OFF_PROT 8
#define VPU_VIDEO_LAYER1_CHANGED		16
#define VPU_UPDATE_DOLBY_VISION			32

#define VPU_MEM_POWEROFF_DELAY           100
static struct work_struct vpu_delay_work;
static int vpu_clk_level;
static DEFINE_SPINLOCK(delay_work_lock);
static int vpu_delay_work_flag;
static int vpu_mem_power_off_count;
#endif

static u32 vpts_ref;
static u32 video_frame_repeat_count;
static u32 smooth_sync_enable;
static u32 hdmi_in_onvideo;
#ifdef CONFIG_AM_VIDEO2
static int video_play_clone_rate = 60;
static int android_clone_rate = 30;
static int noneseamless_play_clone_rate = 5;
#endif

#define CONFIG_AM_VOUT

void safe_disble_videolayer(void)
{
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
	if (video_scaler_mode)
		DisableVideoLayer_PREBELEND();
	else
		DisableVideoLayer();
#else
	DisableVideoLayer();
#endif
}


/*********************************************************/
static inline struct vframe_s *video_vf_peek(void)
{
	struct vframe_s *vf = vf_peek(RECEIVER_NAME);

	if (vf && vf->disp_pts && vf->disp_pts_us64) {
		vf->pts = vf->disp_pts;
		vf->pts_us64 = vf->disp_pts_us64;
		vf->disp_pts = 0;
		vf->disp_pts_us64 = 0;
	}
	return vf;
}

static inline struct vframe_s *video_vf_get(void)
{
	struct vframe_s *vf = NULL;

	vf = vf_get(RECEIVER_NAME);

	if (vf) {
		if (vf->disp_pts && vf->disp_pts_us64) {
			vf->pts = vf->disp_pts;
			vf->pts_us64 = vf->disp_pts_us64;
			vf->disp_pts = 0;
			vf->disp_pts_us64 = 0;
		}
		video_notify_flag |= VIDEO_NOTIFY_PROVIDER_GET;
		atomic_set(&vf->use_cnt, 1);
		/*always to 1,for first get from vfm provider */
		if ((vf->type & VIDTYPE_MVC) && (framepacking_support)
		&&(framepacking_width) && (framepacking_height)) {
			vf->width = framepacking_width;
			vf->height = framepacking_height;
		}
#ifdef TV_3D_FUNCTION_OPEN
		/*can be moved to h264mvc.c */
		if ((vf->type & VIDTYPE_MVC)
		    && (process_3d_type & MODE_3D_ENABLE) && vf->trans_fmt) {
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
			process_3d_type |= MODE_3D_MVC;
			mvc_flag = 1;
		} else {
			process_3d_type &= (~MODE_3D_MVC);
			mvc_flag = 0;
		}
		if (((process_3d_type & MODE_FORCE_3D_TO_2D_LR)
		|| (process_3d_type & MODE_FORCE_3D_LR)
		|| (process_3d_type & MODE_FORCE_3D_FA_LR)
		)
		&& (!(vf->type & VIDTYPE_MVC))
		&& (vf->trans_fmt != TVIN_TFMT_3D_FP)) {
			vf->trans_fmt = TVIN_TFMT_3D_DET_LR;
			vf->left_eye.start_x = 0;
			vf->left_eye.start_y = 0;
			vf->left_eye.width = vf->width / 2;
			vf->left_eye.height = vf->height;

			vf->right_eye.start_x = vf->width / 2;
			vf->right_eye.start_y = 0;
			vf->right_eye.width = vf->width / 2;
		}
		if (((process_3d_type & MODE_FORCE_3D_TO_2D_TB)
		|| (process_3d_type & MODE_FORCE_3D_TB)
		|| (process_3d_type & MODE_FORCE_3D_FA_TB)
		)
		&& (!(vf->type & VIDTYPE_MVC))
		&& (vf->trans_fmt != TVIN_TFMT_3D_FP)) {
			vf->trans_fmt = TVIN_TFMT_3D_TB;
			vf->left_eye.start_x = 0;
			vf->left_eye.start_y = 0;
			vf->left_eye.width = vf->width;
			vf->left_eye.height = vf->height/2;

			vf->right_eye.start_x = 0;
			vf->right_eye.start_y = vf->height/2;
			vf->right_eye.width = vf->width;
			vf->right_eye.height = vf->height/2;
		}
		receive_frame_count++;
#endif
	}
	return vf;

}

static int video_vf_get_states(struct vframe_states *states)
{
	int ret = -1;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
	ret = vf_get_states_by_name(RECEIVER_NAME, states);
	spin_unlock_irqrestore(&lock, flags);
	return ret;
}

static inline void video_vf_put(struct vframe_s *vf)
{
	struct vframe_provider_s *vfp = vf_get_provider(RECEIVER_NAME);

	if (vfp && vf && atomic_dec_and_test(&vf->use_cnt)) {
		vf_put(vf, RECEIVER_NAME);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (is_dolby_vision_enable())
			dolby_vision_vf_put(vf);
#endif
		video_notify_flag |= VIDEO_NOTIFY_PROVIDER_PUT;
	}
}

int ext_get_cur_video_frame(struct vframe_s **vf, int *canvas_index)
{
	if (cur_dispbuf == NULL)
		return -1;
	atomic_inc(&cur_dispbuf->use_cnt);
	*canvas_index = READ_VCBUS_REG(VD1_IF0_CANVAS0 + cur_dev->viu_off);
	*vf = cur_dispbuf;
	return 0;
}
static void dump_vframe_status(const char *name)
{
	int ret = -1;
	struct vframe_states states;
	struct vframe_provider_s *vfp;

	vfp = vf_get_provider_by_name(name);
	if (vfp && vfp->ops && vfp->ops->vf_states)
		ret = vfp->ops->vf_states(&states, vfp->op_arg);

	if (ret == 0) {
		ret += pr_info("%s_pool_size=%d\n",
			name, states.vf_pool_size);
		ret += pr_info("%s buf_free_num=%d\n",
			name, states.buf_free_num);
		ret += pr_info("%s buf_avail_num=%d\n",
			name, states.buf_avail_num);
	} else {
		ret += pr_info("%s vframe no states\n", name);
	}
}

static void dump_vdin_reg(void)
{
	unsigned int reg001, reg002;

	reg001 = READ_VCBUS_REG(0x1204);
	reg002 = READ_VCBUS_REG(0x1205);
	pr_info("VDIN_LCNT_STATUS:0x%x,VDIN_COM_STATUS0:0x%x\n",
		reg001, reg002);
}

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEOCAPTURE

int ext_put_video_frame(struct vframe_s *vf)
{
	if (vf == &vf_local)
		return 0;
	video_vf_put(vf);
	return 0;
}

static int is_need_framepacking_output(void)
{
	int ret = 0;

	if ((framepacking_support) &&
	(cur_dispbuf) && (cur_dispbuf->type & VIDTYPE_MVC)) {
		ret = 1;
	}
	return ret;
}

int ext_register_end_frame_callback(struct amvideocap_req *req)
{
	mutex_lock(&video_module_mutex);
	capture_frame_req = req;
	mutex_unlock(&video_module_mutex);
	return 0;
}
int ext_frame_capture_poll(int endflags)
{
	mutex_lock(&video_module_mutex);
	if (capture_frame_req && capture_frame_req->callback) {
		struct vframe_s *vf;
		int index;
		int ret;
		struct amvideocap_req *req = capture_frame_req;

		ret = ext_get_cur_video_frame(&vf, &index);
		if (!ret) {
			req->callback(req->data, vf, index);
			capture_frame_req = NULL;
		}
	}
	mutex_unlock(&video_module_mutex);
	return 0;
}
#endif


static void vpp_settings_h(struct vpp_frame_par_s *framePtr)
{
	struct vppfilter_mode_s *vpp_filter = &framePtr->vpp_filter;
	u32 r1, r2, r3;

	u32 x_lines;

	r1 = framePtr->VPP_hsc_linear_startp - framePtr->VPP_hsc_startp;
	r2 = framePtr->VPP_hsc_linear_endp - framePtr->VPP_hsc_startp;
	r3 = framePtr->VPP_hsc_endp - framePtr->VPP_hsc_startp;

	if ((framePtr->supscl_path == CORE0_PPS_CORE1) ||
		(framePtr->supscl_path == CORE1_AFTER_PPS))
		r3 >>= framePtr->supsc1_hori_ratio;
	if (framePtr->supscl_path == CORE0_AFTER_PPS)
		r3 >>= framePtr->supsc0_hori_ratio;

	if (platform_type == 1) {
		x_lines = zoom_end_x_lines / (framePtr->hscale_skip_count + 1);
		if (process_3d_type & MODE_3D_OUT_TB) {
			/* vd1 and vd2 do pre blend */
			VSYNC_WR_MPEG_REG(VPP_PREBLEND_VD1_H_START_END,
			((zoom_start_x_lines & VPP_VD_SIZE_MASK) <<
			VPP_VD1_START_BIT) | (((zoom_end_x_lines) &
			VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
			VSYNC_WR_MPEG_REG(VPP_BLEND_VD2_H_START_END,
			((zoom_start_x_lines & VPP_VD_SIZE_MASK) <<
			VPP_VD1_START_BIT) | (((zoom_end_x_lines) &
			VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
			VSYNC_WR_MPEG_REG(VPP_POSTBLEND_VD1_H_START_END +
			cur_dev->vpp_off,
			((framePtr->VPP_hsc_startp & VPP_VD_SIZE_MASK)
			<< VPP_VD1_START_BIT) |
			((framePtr->VPP_hsc_endp & VPP_VD_SIZE_MASK)
			<< VPP_VD1_END_BIT));
		} else if (process_3d_type & MODE_3D_OUT_LR) {
			/* vd1 and vd2 do pre blend */
			VSYNC_WR_MPEG_REG(VPP_PREBLEND_VD1_H_START_END,
			((zoom_start_x_lines & VPP_VD_SIZE_MASK) <<
			VPP_VD1_START_BIT) | (((x_lines >> 1) &
			VPP_VD_SIZE_MASK) <<
			VPP_VD1_END_BIT));
			VSYNC_WR_MPEG_REG(VPP_BLEND_VD2_H_START_END,
			((((x_lines + 1) >> 1) & VPP_VD_SIZE_MASK) <<
			VPP_VD1_START_BIT) | ((x_lines &
			VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
			VSYNC_WR_MPEG_REG(VPP_POSTBLEND_VD1_H_START_END +
			cur_dev->vpp_off,
			((framePtr->VPP_hsc_startp & VPP_VD_SIZE_MASK)
			<< VPP_VD1_START_BIT) |
			((framePtr->VPP_hsc_endp & VPP_VD_SIZE_MASK)
			<< VPP_VD1_END_BIT));
		} else{

			VSYNC_WR_MPEG_REG(VPP_POSTBLEND_VD1_H_START_END +
			cur_dev->vpp_off,
			((framePtr->VPP_hsc_startp & VPP_VD_SIZE_MASK)
			<< VPP_VD1_START_BIT) |
			((framePtr->VPP_hsc_endp & VPP_VD_SIZE_MASK)
			<< VPP_VD1_END_BIT));

			VSYNC_WR_MPEG_REG(VPP_BLEND_VD2_H_START_END +
			cur_dev->vpp_off,
			((framePtr->VPP_hd_start_lines_ &
			VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
			((framePtr->VPP_hd_end_lines_ &
			VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
		}
	} else {
			VSYNC_WR_MPEG_REG(VPP_POSTBLEND_VD1_H_START_END +
			cur_dev->vpp_off,
			((framePtr->VPP_hsc_startp & VPP_VD_SIZE_MASK)
			<< VPP_VD1_START_BIT) |
			((framePtr->VPP_hsc_endp & VPP_VD_SIZE_MASK)
			<< VPP_VD1_END_BIT));

			VSYNC_WR_MPEG_REG(VPP_BLEND_VD2_H_START_END +
			cur_dev->vpp_off,
			((framePtr->VPP_hd_start_lines_ &
			VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
			((framePtr->VPP_hd_end_lines_ &
			VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
	}
	VSYNC_WR_MPEG_REG(VPP_HSC_REGION12_STARTP +
			cur_dev->vpp_off,
			(0 << VPP_REGION1_BIT) |
			((r1 & VPP_REGION_MASK) << VPP_REGION2_BIT));

	VSYNC_WR_MPEG_REG(VPP_HSC_REGION34_STARTP + cur_dev->vpp_off,
			((r2 & VPP_REGION_MASK) << VPP_REGION3_BIT) |
			((r3 & VPP_REGION_MASK) << VPP_REGION4_BIT));
	VSYNC_WR_MPEG_REG(VPP_HSC_REGION4_ENDP + cur_dev->vpp_off, r3);

	VSYNC_WR_MPEG_REG(VPP_HSC_START_PHASE_STEP + cur_dev->vpp_off,
			vpp_filter->vpp_hf_start_phase_step);

	VSYNC_WR_MPEG_REG(VPP_HSC_REGION1_PHASE_SLOPE + cur_dev->vpp_off,
			vpp_filter->vpp_hf_start_phase_slope);

	VSYNC_WR_MPEG_REG(VPP_HSC_REGION3_PHASE_SLOPE + cur_dev->vpp_off,
			vpp_filter->vpp_hf_end_phase_slope);

	VSYNC_WR_MPEG_REG(VPP_LINE_IN_LENGTH + cur_dev->vpp_off,
			framePtr->VPP_line_in_length_);
}

static void vd2_settings_h(struct vframe_s *vf)
{
	u32 VPP_hd_start_lines_;
	u32 VPP_hd_end_lines_;

	if (vf) {
		VPP_hd_start_lines_ = 0;
		VPP_hd_end_lines_ = ((vf->type & VIDTYPE_COMPRESS) ?
		vf->compWidth : vf->width) - 1;
		VSYNC_WR_MPEG_REG(VPP_BLEND_VD2_H_START_END +
		cur_dev->vpp_off,
		((VPP_hd_start_lines_ &
		VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
		((VPP_hd_end_lines_ &
		VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
	}
}

static void vpp_settings_v(struct vpp_frame_par_s *framePtr)
{
	struct vppfilter_mode_s *vpp_filter = &framePtr->vpp_filter;
	u32 r, afbc_enble_flag;
	u32 y_lines;
	u32 v_phase;
	u32 v_skip_flag = 0;
	int x, y, w, h;

	r = framePtr->VPP_vsc_endp - framePtr->VPP_vsc_startp;
	afbc_enble_flag = 0;
	if (is_meson_gxbb_cpu())
		afbc_enble_flag = READ_VCBUS_REG(AFBC_ENABLE) & 0x100;
	if ((vpp_filter->vpp_vsc_start_phase_step > 0x1000000)
		&& afbc_enble_flag)
		VSYNC_WR_MPEG_REG(VPP_POSTBLEND_VD1_V_START_END +
			cur_dev->vpp_off, ((framePtr->VPP_vsc_startp &
			VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT)
			| (((framePtr->VPP_vsc_endp + 1) & VPP_VD_SIZE_MASK) <<
			VPP_VD1_END_BIT));
	else {
		afbc_enble_flag = READ_VCBUS_REG(AFBC_ENABLE) & 0x100;
		v_phase = vpp_filter->vpp_vsc_start_phase_step;
		vpp_get_video_layer_position(&x, &y, &w, &h);
		if (v_phase * (framePtr->vscale_skip_count + 1) > 0x1000000) {
			if ((afbc_enble_flag) && (y < 0)) {
				if ((framePtr->VPP_vsc_endp < 0x250) ||
				(framePtr->VPP_vsc_endp <
				framePtr->VPP_post_blend_vd_v_end_/2)) {
					if (framePtr->VPP_vsc_endp > 0x6)
						v_skip_flag = 1;
				}
			}
		}
		if (v_skip_flag == 1) {
			VSYNC_WR_MPEG_REG(VPP_POSTBLEND_VD1_V_START_END +
			cur_dev->vpp_off, ((framePtr->VPP_vsc_startp &
			VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT)
			| (((framePtr->VPP_vsc_endp - 6) & VPP_VD_SIZE_MASK) <<
			VPP_VD1_END_BIT));
		} else {
			VSYNC_WR_MPEG_REG(VPP_POSTBLEND_VD1_V_START_END +
			cur_dev->vpp_off, ((framePtr->VPP_vsc_startp &
			VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT)
			| ((framePtr->VPP_vsc_endp & VPP_VD_SIZE_MASK) <<
			VPP_VD1_END_BIT));
		}
	}

	if (platform_type == 1) {
		y_lines = zoom_end_y_lines / (framePtr->vscale_skip_count + 1);
		if (process_3d_type & MODE_3D_OUT_TB) {
			VSYNC_WR_MPEG_REG(VPP_PREBLEND_VD1_V_START_END,
			((zoom_start_y_lines & VPP_VD_SIZE_MASK) <<
			VPP_VD1_START_BIT) | (((y_lines >> 1) &
			VPP_VD_SIZE_MASK) <<
			VPP_VD1_END_BIT));
			VSYNC_WR_MPEG_REG(
			VPP_BLEND_VD2_V_START_END,
			((((y_lines + 1) >> 1) & VPP_VD_SIZE_MASK) <<
			VPP_VD1_START_BIT) |
			((y_lines & VPP_VD_SIZE_MASK) <<
			VPP_VD1_END_BIT));
		} else if (process_3d_type & MODE_3D_OUT_LR) {
			VSYNC_WR_MPEG_REG(VPP_PREBLEND_VD1_V_START_END,
			((zoom_start_y_lines & VPP_VD_SIZE_MASK) <<
			VPP_VD1_START_BIT) | ((zoom_end_y_lines &
			VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
			VSYNC_WR_MPEG_REG(VPP_BLEND_VD2_V_START_END,
			((zoom_start_y_lines & VPP_VD_SIZE_MASK) <<
			VPP_VD1_START_BIT) | ((zoom_end_y_lines &
			VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
		} else {
			if ((framePtr->VPP_post_blend_vd_v_end_ -
				framePtr->VPP_post_blend_vd_v_start_ + 1) >
				VPP_PREBLEND_VD_V_END_LIMIT) {
				VSYNC_WR_MPEG_REG(VPP_PREBLEND_VD1_V_START_END +
				cur_dev->vpp_off,
				((framePtr->VPP_post_blend_vd_v_start_
				& VPP_VD_SIZE_MASK) <<
				VPP_VD1_START_BIT) |
				((framePtr->VPP_post_blend_vd_v_end_ &
				VPP_VD_SIZE_MASK)
				<< VPP_VD1_END_BIT));
			} else {
				VSYNC_WR_MPEG_REG(VPP_PREBLEND_VD1_V_START_END +
				cur_dev->vpp_off,
				((0 & VPP_VD_SIZE_MASK) <<
				VPP_VD1_START_BIT) |
				(((VPP_PREBLEND_VD_V_END_LIMIT - 1) &
				VPP_VD_SIZE_MASK) <<
				VPP_VD1_END_BIT));
			}

			if (is_need_framepacking_output()) {
				VSYNC_WR_MPEG_REG(VPP_BLEND_VD2_V_START_END +
				cur_dev->vpp_off,
				(((((framePtr->VPP_vd_end_lines_ -
			framepacking_blank + 1) / 2) + framepacking_blank) &
				VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
				(((framePtr->VPP_vd_end_lines_) &
				VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
			} else {
				VSYNC_WR_MPEG_REG(VPP_BLEND_VD2_V_START_END +
				cur_dev->vpp_off,
				((((framePtr->VPP_vd_end_lines_ + 1) / 2) &
				VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
				(((framePtr->VPP_vd_end_lines_) &
				VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
			}

		}
	} else {
			if ((framePtr->VPP_post_blend_vd_v_end_ -
				framePtr->VPP_post_blend_vd_v_start_ + 1) >
				VPP_PREBLEND_VD_V_END_LIMIT) {
				VSYNC_WR_MPEG_REG(VPP_PREBLEND_VD1_V_START_END +
				cur_dev->vpp_off,
				((framePtr->VPP_post_blend_vd_v_start_
				& VPP_VD_SIZE_MASK) <<
				VPP_VD1_START_BIT) |
				((framePtr->VPP_post_blend_vd_v_end_ &
				VPP_VD_SIZE_MASK)
				<< VPP_VD1_END_BIT));
			} else {
				VSYNC_WR_MPEG_REG(VPP_PREBLEND_VD1_V_START_END +
					cur_dev->vpp_off,
					((0 & VPP_VD_SIZE_MASK) <<
					VPP_VD1_START_BIT) |
					(((VPP_PREBLEND_VD_V_END_LIMIT - 1) &
					VPP_VD_SIZE_MASK) <<
					VPP_VD1_END_BIT));
			}
			if (is_need_framepacking_output()) {
				VSYNC_WR_MPEG_REG(VPP_BLEND_VD2_V_START_END +
				cur_dev->vpp_off,
				(((((framePtr->VPP_vd_end_lines_ -
			framepacking_blank + 1) / 2) + framepacking_blank) &
				VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
				(((framePtr->VPP_vd_end_lines_) &
				VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
			} else {
				VSYNC_WR_MPEG_REG(VPP_BLEND_VD2_V_START_END +
				cur_dev->vpp_off,
				((((framePtr->VPP_vd_end_lines_ + 1) / 2) &
				VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
				(((framePtr->VPP_vd_end_lines_) &
				VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
			}

	}
	VSYNC_WR_MPEG_REG(VPP_VSC_REGION12_STARTP + cur_dev->vpp_off, 0);
	VSYNC_WR_MPEG_REG(VPP_VSC_REGION34_STARTP + cur_dev->vpp_off,
			  ((r & VPP_REGION_MASK) << VPP_REGION3_BIT) |
			  ((r & VPP_REGION_MASK) << VPP_REGION4_BIT));

	if ((framePtr->supscl_path == CORE0_PPS_CORE1) ||
		(framePtr->supscl_path == CORE1_AFTER_PPS))
		r >>= framePtr->supsc1_vert_ratio;
	if (framePtr->supscl_path == CORE0_AFTER_PPS)
		r >>= framePtr->supsc0_vert_ratio;

	VSYNC_WR_MPEG_REG(VPP_VSC_REGION4_ENDP + cur_dev->vpp_off, r);

	VSYNC_WR_MPEG_REG(VPP_VSC_START_PHASE_STEP + cur_dev->vpp_off,
			vpp_filter->vpp_vsc_start_phase_step);
}

static void vd2_settings_v(struct vframe_s *vf)
{
	u32 VPP_vd_start_lines_;
	u32 VPP_vd_end_lines_;

	if (vf) {
		VPP_vd_start_lines_ = 0;
		VPP_vd_end_lines_ = ((vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height) - 1;

		VSYNC_WR_MPEG_REG(VPP_BLEND_VD2_V_START_END +
			cur_dev->vpp_off,
			((VPP_vd_start_lines_ &
			VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
			(((VPP_vd_end_lines_) &
			VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
	}
}

#ifdef TV_3D_FUNCTION_OPEN

static void zoom_get_horz_pos(struct vframe_s *vf, u32 vpp_3d_mode, u32 *ls,
			      u32 *le, u32 *rs, u32 *re)
{
	u32 crop_sx, crop_ex, crop_sy, crop_ey;

	if (!vf)
		return;
	vpp_get_video_source_crop(&crop_sy, &crop_sx, &crop_ey, &crop_ex);

	switch (vpp_3d_mode) {
	case VPP_3D_MODE_LR:
		/*half width,double height */
		*ls = zoom_start_x_lines;
		*le = zoom_end_x_lines;
		*rs = *ls + (vf->width >> 1);
		*re = *le + (vf->width >> 1);
		if (process_3d_type & MODE_3D_OUT_LR) {
			*ls = zoom_start_x_lines;
			*le = zoom_end_x_lines >> 1;
			*rs = *ls + (vf->width >> 1);
			*re = *le + (vf->width >> 1);
		}
		break;
	case VPP_3D_MODE_TB:
	case VPP_3D_MODE_LA:
	case VPP_3D_MODE_FA:
	default:
		if (vf->trans_fmt == TVIN_TFMT_3D_FP) {
			*ls = vf->left_eye.start_x + crop_sx;
			*le = vf->left_eye.start_x + vf->left_eye.width -
				crop_ex - 1;
			*rs = vf->right_eye.start_x + crop_sx;
			*re = vf->right_eye.start_x + vf->right_eye.width -
				crop_ex - 1;
		} else if (process_3d_type & MODE_3D_OUT_LR) {
			*ls = zoom_start_x_lines;
			*le = zoom_end_x_lines >> 1;
			*rs = *ls;
			*re = *le;
			/* *rs = *ls + (vf->width); */
			/* *re = *le + (vf->width); */
		} else {
			*ls = *rs = zoom_start_x_lines;
			*le = *re = zoom_end_x_lines;
		}
		break;
	}
}

static void zoom_get_vert_pos(struct vframe_s *vf, u32 vpp_3d_mode, u32 *ls,
			u32 *le, u32 *rs, u32 *re)
{
	u32 crop_sx, crop_ex, crop_sy, crop_ey, height;

	vpp_get_video_source_crop(&crop_sy, &crop_sx, &crop_ey, &crop_ex);

	if (vf->type & VIDTYPE_INTERLACE)
		height = vf->height >> 1;
	else
		height = vf->height;

	switch (vpp_3d_mode) {
	case VPP_3D_MODE_TB:
		if (vf->trans_fmt == TVIN_TFMT_3D_FP) {
			if (vf->type & VIDTYPE_INTERLACE) {
				/*if input is interlace vertical*/
				/*crop will be reduce by half */
				*ls =
				    (vf->left_eye.start_y +
				     (crop_sy >> 1)) >> 1;
				*le =
				    ((vf->left_eye.start_y +
				      vf->left_eye.height -
				      (crop_ey >> 1)) >> 1) - 1;
				*rs =
				    (vf->right_eye.start_y +
				     (crop_sy >> 1)) >> 1;
				*re =
				    ((vf->right_eye.start_y +
				      vf->left_eye.height -
				      (crop_ey >> 1)) >> 1) - 1;
			} else {
				*ls = vf->left_eye.start_y + (crop_sy >> 1);
				*le = vf->left_eye.start_y +
					vf->left_eye.height -
					(crop_ey >> 1) - 1;
				*rs = vf->right_eye.start_y + (crop_sy >> 1);
				*re =
				    vf->right_eye.start_y +
				    vf->left_eye.height - (crop_ey >> 1) - 1;
			}
		} else {
			if ((vf->type & VIDTYPE_VIU_FIELD)
			    && (vf->type & VIDTYPE_INTERLACE)) {
				*ls = zoom_start_y_lines >> 1;
				*le = zoom_end_y_lines >> 1;
				*rs = *ls + (height >> 1);
				*re = *le + (height >> 1);

			} else if (vf->type & VIDTYPE_INTERLACE) {
				*ls = zoom_start_y_lines >> 1;
				*le = zoom_end_y_lines >> 1;
				*rs = *ls + height;
				*re = *le + height;

			} else {
				/* same width,same height */
				*ls = zoom_start_y_lines >> 1;
				*le = zoom_end_y_lines >> 1;
				*rs = *ls + (height >> 1);
				*re = *le + (height >> 1);
			}
		if ((process_3d_type & MODE_3D_TO_2D_MASK)
		    || (process_3d_type & MODE_3D_OUT_LR)) {
			/* same width,half height */
			*ls = zoom_start_y_lines;
			*le = zoom_end_y_lines;
			*rs = zoom_start_y_lines + (height >> 1);
			*re = zoom_end_y_lines + (height >> 1);
			}
		}
		break;
	case VPP_3D_MODE_LR:
		/* half width,double height */
		*ls = *rs = zoom_start_y_lines >> 1;
		*le = *re = zoom_end_y_lines >> 1;
		if ((process_3d_type & MODE_3D_TO_2D_MASK)
		    || (process_3d_type & MODE_3D_OUT_LR)) {
			/*half width ,same height */
			*ls = *rs = zoom_start_y_lines;
			*le = *re = zoom_end_y_lines;
		}
		break;
	case VPP_3D_MODE_FA:
		/*same width same heiht */
		if ((process_3d_type & MODE_3D_TO_2D_MASK)
		    || (process_3d_type & MODE_3D_OUT_LR)) {
			*ls = *rs = zoom_start_y_lines;
			*le = *re = zoom_end_y_lines;
		} else {
			*ls = *rs = (zoom_start_y_lines + crop_sy) >> 1;
			*le = *re = (zoom_end_y_lines + crop_ey) >> 1;
		}
		break;
	case VPP_3D_MODE_LA:
		*ls = *rs = zoom_start_y_lines;
		if ((process_3d_type & MODE_3D_LR_SWITCH)
		    || (process_3d_type & MODE_3D_TO_2D_R))
			*ls = *rs = zoom_start_y_lines + 1;
		if (process_3d_type & MODE_3D_TO_2D_L)
			*ls = *rs = zoom_start_y_lines;
		*le = *re = zoom_end_y_lines;
		if ((process_3d_type & MODE_3D_OUT_FA_MASK)
		    || (process_3d_type & MODE_3D_OUT_TB)
		    || (process_3d_type & MODE_3D_OUT_LR)) {
			*rs = zoom_start_y_lines + 1;
			*ls = zoom_start_y_lines;
			/* *le = zoom_end_y_lines; */
			/* *re = zoom_end_y_lines; */
		}
		break;
	default:
		*ls = *rs = zoom_start_y_lines;
		*le = *re = zoom_end_y_lines;
		break;
	}
}

#endif
static void zoom_display_horz(int hscale)
{
	u32 ls = 0, le = 0, rs = 0, re = 0;
#ifdef TV_REVERSE
	int content_w, content_l, content_r;
#endif
#ifdef TV_3D_FUNCTION_OPEN
	if (process_3d_type & MODE_3D_ENABLE) {
		zoom_get_horz_pos(cur_dispbuf, cur_frame_par->vpp_3d_mode, &ls,
				  &le, &rs, &re);
	} else {
		ls = rs = zoom_start_x_lines;
		le = re = zoom_end_x_lines;
	}
#else
	ls = rs = zoom_start_x_lines;
	le = re = zoom_end_x_lines;
#endif
	VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_X0 + cur_dev->viu_off,
			  (ls << VDIF_PIC_START_BIT) |
			  (le << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_X0 + cur_dev->viu_off,
			  (ls / 2 << VDIF_PIC_START_BIT) |
			  (le / 2 << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_X1 + cur_dev->viu_off,
			  (rs << VDIF_PIC_START_BIT) |
			  (re << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_X1 + cur_dev->viu_off,
			  (rs / 2 << VDIF_PIC_START_BIT) |
			  (re / 2 << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(VIU_VD1_FMT_W + cur_dev->viu_off,
			  (((zoom_end_x_lines - zoom_start_x_lines +
			     1) >> hscale) << VD1_FMT_LUMA_WIDTH_BIT) |
			  (((zoom_end_x_lines / 2 - zoom_start_x_lines / 2 +
			     1) >> hscale) << VD1_FMT_CHROMA_WIDTH_BIT));

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		int l_aligned;
		int r_aligned;
		int h_skip = cur_frame_par->hscale_skip_count + 1;
		if ((zoom_start_x_lines > 0) ||
		(zoom_end_x_lines < ori_end_x_lines)) {
			l_aligned = round_down(ori_start_x_lines, 32);
			r_aligned = round_up(ori_end_x_lines + 1, 32);
		} else {
			l_aligned = round_down(zoom_start_x_lines, 32);
			r_aligned = round_up(zoom_end_x_lines + 1, 32);
		}
		VSYNC_WR_MPEG_REG(AFBC_VD_CFMT_W,
			  (((r_aligned - l_aligned) / h_skip) << 16) |
			  ((r_aligned / 2 - l_aligned / 2) / h_skip));

		VSYNC_WR_MPEG_REG(AFBC_MIF_HOR_SCOPE,
			  ((l_aligned / 32) << 16) |
			  ((r_aligned / 32) - 1));

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
			VSYNC_WR_MPEG_REG(AFBC_SIZE_OUT,
				(VSYNC_RD_MPEG_REG(AFBC_SIZE_OUT)
				& 0xffff) |
				(((r_aligned - l_aligned) / h_skip) << 16));
		}
#ifdef TV_REVERSE
		if (reverse) {
			content_w = zoom_end_x_lines - zoom_start_x_lines + 1;
			content_l = (r_aligned - zoom_end_x_lines - 1) +
			(zoom_start_x_lines - l_aligned);
			content_r = content_l + content_w - 1;
			VSYNC_WR_MPEG_REG(AFBC_PIXEL_HOR_SCOPE,
				  (((content_l << 16)) | content_r) / h_skip);
		} else
#endif
		{
			VSYNC_WR_MPEG_REG(AFBC_PIXEL_HOR_SCOPE,
				  (((zoom_start_x_lines - l_aligned) << 16) |
				  (zoom_end_x_lines - l_aligned)) / h_skip);
		}
		VSYNC_WR_MPEG_REG(AFBC_SIZE_IN,
			 (VSYNC_RD_MPEG_REG(AFBC_SIZE_IN) & 0xffff) |
			 ((r_aligned - l_aligned) << 16));
	}

	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA_X0 + cur_dev->viu_off,
		(ls << VDIF_PIC_START_BIT) |
		(le << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA_X0 + cur_dev->viu_off,
		(ls / 2 << VDIF_PIC_START_BIT) |
		(le / 2 << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA_X1 + cur_dev->viu_off,
		(rs << VDIF_PIC_START_BIT) |
		(re << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA_X1 + cur_dev->viu_off,
		(rs / 2 << VDIF_PIC_START_BIT) |
		(re / 2 << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VIU_VD2_FMT_W + cur_dev->viu_off,
		(((zoom_end_x_lines - zoom_start_x_lines +
		1) >> hscale) << VD1_FMT_LUMA_WIDTH_BIT) |
		(((zoom_end_x_lines / 2 - zoom_start_x_lines / 2 +
		1) >> hscale) << VD1_FMT_CHROMA_WIDTH_BIT));
}

static void vd2_zoom_display_horz(int hscale)
{
	u32 ls, le, rs, re;
#ifdef TV_REVERSE
	int content_w, content_l, content_r;
#endif
	ls = rs = zoom2_start_x_lines;
	le = re = zoom2_end_x_lines;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		int l_aligned;
		int r_aligned;
		int h_skip = cur_frame_par->hscale_skip_count + 1;

		if ((zoom2_start_x_lines > 0) ||
		(zoom2_end_x_lines < ori2_end_x_lines)) {
			l_aligned = round_down(ori2_start_x_lines, 32);
			r_aligned = round_up(ori2_end_x_lines + 1, 32);
		} else {
			l_aligned = round_down(zoom2_start_x_lines, 32);
			r_aligned = round_up(zoom2_end_x_lines + 1, 32);
		}
		VSYNC_WR_MPEG_REG(VD2_AFBC_VD_CFMT_W,
			  (((r_aligned - l_aligned) / h_skip) << 16) |
			  ((r_aligned / 2 - l_aligned / 2) / h_skip));

		VSYNC_WR_MPEG_REG(VD2_AFBC_MIF_HOR_SCOPE,
			  ((l_aligned / 32) << 16) |
			  ((r_aligned / 32) - 1));
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
			VSYNC_WR_MPEG_REG(VD2_AFBC_SIZE_OUT,
				(VSYNC_RD_MPEG_REG(VD2_AFBC_SIZE_OUT)
				& 0xffff) |
				(((r_aligned - l_aligned) / h_skip) << 16));
		}
#ifdef TV_REVERSE
		if (reverse) {
			content_w = zoom2_end_x_lines - zoom2_start_x_lines + 1;
			content_l = (r_aligned - zoom2_end_x_lines - 1) +
			(zoom2_start_x_lines - l_aligned);
			content_r = content_l + content_w - 1;
			VSYNC_WR_MPEG_REG(VD2_AFBC_PIXEL_HOR_SCOPE,
				  (content_l << 16) | content_r);
		} else
#endif
		{
			VSYNC_WR_MPEG_REG(VD2_AFBC_PIXEL_HOR_SCOPE,
				  ((zoom2_start_x_lines - l_aligned) << 16) |
				  (zoom2_end_x_lines - l_aligned));
		}
		VSYNC_WR_MPEG_REG(VD2_AFBC_SIZE_IN,
			 (VSYNC_RD_MPEG_REG(VD2_AFBC_SIZE_IN) & 0xffff) |
			 ((r_aligned - l_aligned) << 16));
	}

	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA_X0 + cur_dev->viu_off,
		(ls << VDIF_PIC_START_BIT) |
		(le << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA_X0 + cur_dev->viu_off,
		(ls / 2 << VDIF_PIC_START_BIT) |
		(le / 2 << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA_X1 + cur_dev->viu_off,
		(rs << VDIF_PIC_START_BIT) |
		(re << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA_X1 + cur_dev->viu_off,
		(rs / 2 << VDIF_PIC_START_BIT) |
		(re / 2 << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VIU_VD2_FMT_W + cur_dev->viu_off,
		(((le - ls + 1) >> hscale)
		<< VD1_FMT_LUMA_WIDTH_BIT) |
		(((le / 2 - ls / 2 + 1) >> hscale)
		<< VD1_FMT_CHROMA_WIDTH_BIT));
}

static void zoom_display_vert(void)
{

	u32 ls, le, rs, re;

	/*if (platform_type == 1) {*/
		if (process_3d_type & MODE_3D_ENABLE) {
			zoom_get_vert_pos(cur_dispbuf,
			cur_frame_par->vpp_3d_mode, &ls,
					  &le, &rs, &re);
		} else {
			ls = rs = zoom_start_y_lines;
			le = re = zoom_end_y_lines;
		}
	/*} else {
		if (process_3d_type & MODE_3D_ENABLE) {
			zoom_get_vert_pos(cur_dispbuf,
			cur_frame_par->vpp_3d_mode, &ls,
					  &le, &rs, &re);
		} else {
			ls = rs = zoom_start_y_lines;
			le = re = zoom_end_y_lines;
		}
	}
*/

	if ((cur_dispbuf) && (cur_dispbuf->type & VIDTYPE_MVC)) {
		if (is_need_framepacking_output()) {
			VSYNC_WR_MPEG_REG(
				VD1_IF0_LUMA_Y0 + cur_dev->viu_off,
				(ls << VDIF_PIC_START_BIT) |
				(le << VDIF_PIC_END_BIT));

			VSYNC_WR_MPEG_REG(
				VD1_IF0_CHROMA_Y0 + cur_dev->viu_off,
				((ls / 2) << VDIF_PIC_START_BIT) |
				((le / 2) << VDIF_PIC_END_BIT));

			VSYNC_WR_MPEG_REG(
				VD2_IF0_LUMA_Y0 + cur_dev->viu_off,
				(rs << VDIF_PIC_START_BIT) |
				(re << VDIF_PIC_END_BIT));

			VSYNC_WR_MPEG_REG(
				VD2_IF0_CHROMA_Y0 + cur_dev->viu_off,
				((rs / 2) << VDIF_PIC_START_BIT) |
				((re / 2) << VDIF_PIC_END_BIT));
		} else {
			VSYNC_WR_MPEG_REG(
				VD1_IF0_LUMA_Y0 + cur_dev->viu_off,
				(ls * 2 << VDIF_PIC_START_BIT) |
				((le * 2 - 1) << VDIF_PIC_END_BIT));

			VSYNC_WR_MPEG_REG(
				VD1_IF0_CHROMA_Y0 + cur_dev->viu_off,
				((ls) << VDIF_PIC_START_BIT) |
				((le - 1) << VDIF_PIC_END_BIT));

			VSYNC_WR_MPEG_REG(
				VD2_IF0_LUMA_Y0 + cur_dev->viu_off,
				(ls * 2 << VDIF_PIC_START_BIT) |
				((le * 2 - 1) << VDIF_PIC_END_BIT));

			VSYNC_WR_MPEG_REG(
				VD2_IF0_CHROMA_Y0 + cur_dev->viu_off,
				((ls) << VDIF_PIC_START_BIT) |
				((le - 1) << VDIF_PIC_END_BIT));
		}
	} else {
		VSYNC_WR_MPEG_REG(
			VD1_IF0_LUMA_Y0 + cur_dev->viu_off,
			(ls << VDIF_PIC_START_BIT) |
			(le << VDIF_PIC_END_BIT));

		VSYNC_WR_MPEG_REG(
			VD1_IF0_CHROMA_Y0 + cur_dev->viu_off,
			((ls / 2) << VDIF_PIC_START_BIT) |
			((le / 2) << VDIF_PIC_END_BIT));

		VSYNC_WR_MPEG_REG(
			VD1_IF0_LUMA_Y1 + cur_dev->viu_off,
			(rs << VDIF_PIC_START_BIT) |
			(re << VDIF_PIC_END_BIT));

		VSYNC_WR_MPEG_REG(
			VD1_IF0_CHROMA_Y1 + cur_dev->viu_off,
			((rs / 2) << VDIF_PIC_START_BIT) |
			((re / 2) << VDIF_PIC_END_BIT));
		if (platform_type == 1) {
			/* vd2 */
			VSYNC_WR_MPEG_REG(
				VD2_IF0_LUMA_Y0 + cur_dev->viu_off,
				(ls << VDIF_PIC_START_BIT) |
				(le << VDIF_PIC_END_BIT));

			VSYNC_WR_MPEG_REG(
				VD2_IF0_CHROMA_Y0 + cur_dev->viu_off,
				((ls / 2) << VDIF_PIC_START_BIT) |
				((le / 2) << VDIF_PIC_END_BIT));

			VSYNC_WR_MPEG_REG(
				VD2_IF0_LUMA_Y1 + cur_dev->viu_off,
				(rs << VDIF_PIC_START_BIT) |
				(re << VDIF_PIC_END_BIT));

			VSYNC_WR_MPEG_REG(
				VD2_IF0_CHROMA_Y1 + cur_dev->viu_off,
				((rs / 2) << VDIF_PIC_START_BIT) |
				((re / 2) << VDIF_PIC_END_BIT));
		}
	}

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		int t_aligned;
		int b_aligned;
		int ori_t_aligned;
		int ori_b_aligned;
		int v_skip = cur_frame_par->vscale_skip_count + 1;
		t_aligned = round_down(zoom_start_y_lines, 4);
		b_aligned = round_up(zoom_end_y_lines + 1, 4);

		ori_t_aligned = round_down(ori_start_y_lines, 4);
		ori_b_aligned = round_up(ori_end_y_lines + 1, 4);
		VSYNC_WR_MPEG_REG(AFBC_VD_CFMT_H,
		    (b_aligned - t_aligned) / 2 / v_skip);

		VSYNC_WR_MPEG_REG(AFBC_MIF_VER_SCOPE,
		    ((t_aligned / 4) << 16) |
		    ((b_aligned / 4) - 1));

		VSYNC_WR_MPEG_REG(AFBC_PIXEL_VER_SCOPE,
		    ((zoom_start_y_lines - t_aligned) << 16) |
		    (zoom_end_y_lines - t_aligned));
	/* afbc pixel vertical output region must be
	 * [0, zoom_end_y_lines - zoom_start_y_lines]
	 */
		VSYNC_WR_MPEG_REG(AFBC_PIXEL_VER_SCOPE,
		(zoom_end_y_lines - zoom_start_y_lines));

		VSYNC_WR_MPEG_REG(AFBC_SIZE_IN,
			(VSYNC_RD_MPEG_REG(AFBC_SIZE_IN)
			& 0xffff0000) |
			(ori_b_aligned - ori_t_aligned));
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
			VSYNC_WR_MPEG_REG(AFBC_SIZE_OUT,
				(VSYNC_RD_MPEG_REG(AFBC_SIZE_OUT) &
				0xffff0000) |
				((b_aligned - t_aligned) / v_skip));
		}
	}
}

static void vd2_zoom_display_vert(void)
{

	u32 ls, le, rs, re;

	ls = rs = zoom2_start_y_lines;
	le = re = zoom2_end_y_lines;

	/* vd2 */
	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA_Y0 + cur_dev->viu_off,
		(ls << VDIF_PIC_START_BIT) |
		(le << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA_Y0 + cur_dev->viu_off,
		((ls / 2) << VDIF_PIC_START_BIT) |
		((le / 2) << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA_Y1 + cur_dev->viu_off,
		(rs << VDIF_PIC_START_BIT) |
		(re << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA_Y1 + cur_dev->viu_off,
		((rs / 2) << VDIF_PIC_START_BIT) |
		((re / 2) << VDIF_PIC_END_BIT));

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		int t_aligned;
		int b_aligned;
		int ori_t_aligned;
		int ori_b_aligned;
		int v_skip = cur_frame_par->vscale_skip_count + 1;
		t_aligned = round_down(zoom2_start_y_lines, 4);
		b_aligned = round_up(zoom2_end_y_lines + 1, 4);

		ori_t_aligned = round_down(ori2_start_y_lines, 4);
		ori_b_aligned = round_up(ori2_end_y_lines + 1, 4);

		/* TODO: afbc setting only support 420 for now */
		VSYNC_WR_MPEG_REG(VD2_AFBC_VD_CFMT_H,
		   (b_aligned - t_aligned) / 2 / v_skip);

		VSYNC_WR_MPEG_REG(VD2_AFBC_MIF_VER_SCOPE,
		    ((t_aligned / 4) << 16) |
		    ((b_aligned / 4) - 1));

		VSYNC_WR_MPEG_REG(VD2_AFBC_PIXEL_VER_SCOPE,
		    ((zoom2_start_y_lines - t_aligned) << 16) |
		    (zoom2_end_y_lines - t_aligned));

		VSYNC_WR_MPEG_REG(VD2_AFBC_SIZE_IN,
			(VSYNC_RD_MPEG_REG(VD2_AFBC_SIZE_IN)
			& 0xffff0000) |
			(ori_b_aligned - ori_t_aligned));
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
			VSYNC_WR_MPEG_REG(VD2_AFBC_SIZE_OUT,
			(VSYNC_RD_MPEG_REG(VD2_AFBC_SIZE_OUT)
			& 0xffff0000) |
			((b_aligned - t_aligned) / v_skip));
		}
	}
}

#ifdef TV_3D_FUNCTION_OPEN
/* judge the out mode is 240:LBRBLRBR  or 120:LRLRLR */
static void judge_3d_fa_out_mode(void)
{
	if ((process_3d_type & MODE_3D_OUT_FA_MASK)
	    && pause_one_3d_fl_frame == 2)
		toggle_3d_fa_frame = OUT_FA_B_FRAME;
	else if ((process_3d_type & MODE_3D_OUT_FA_MASK)
		 && pause_one_3d_fl_frame == 1)
		toggle_3d_fa_frame = OUT_FA_A_FRAME;
	else if ((process_3d_type & MODE_3D_OUT_FA_MASK)
		 && pause_one_3d_fl_frame == 0) {
		/* toggle_3d_fa_frame  determine*/
		/*the out frame is L or R or blank */
		if ((process_3d_type & MODE_3D_OUT_FA_L_FIRST)) {
			if ((vsync_count % 2) == 0)
				toggle_3d_fa_frame = OUT_FA_A_FRAME;
			else
				toggle_3d_fa_frame = OUT_FA_B_FRAME;
		} else if ((process_3d_type & MODE_3D_OUT_FA_R_FIRST)) {
			if ((vsync_count % 2) == 0)
				toggle_3d_fa_frame = OUT_FA_B_FRAME;
			else
				toggle_3d_fa_frame = OUT_FA_A_FRAME;
		} else if ((process_3d_type & MODE_3D_OUT_FA_LB_FIRST)) {
			if ((vsync_count % 4) == 0)
				toggle_3d_fa_frame = OUT_FA_A_FRAME;
			else if ((vsync_count % 4) == 2)
				toggle_3d_fa_frame = OUT_FA_B_FRAME;
			else
				toggle_3d_fa_frame = OUT_FA_BANK_FRAME;
		} else if ((process_3d_type & MODE_3D_OUT_FA_RB_FIRST)) {
			if ((vsync_count % 4) == 0)
				toggle_3d_fa_frame = OUT_FA_B_FRAME;
			else if ((vsync_count % 4) == 2)
				toggle_3d_fa_frame = OUT_FA_A_FRAME;
			else
				toggle_3d_fa_frame = OUT_FA_BANK_FRAME;
		}
	} else
		toggle_3d_fa_frame = OUT_FA_A_FRAME;
}

#endif

static void vframe_canvas_set(struct canvas_config_s *config, u32 planes,
				u32 *index)
{
	int i;
	u32 *canvas_index = index;

	struct canvas_config_s *cfg = config;

	for (i = 0; i < planes; i++, canvas_index++, cfg++)
		canvas_config_config(*canvas_index, cfg);
}

#ifdef PTS_LOGGING
static void log_vsync_video_pattern(int pattern)
{
	int factor1 = 0, factor2 = 0, pattern_range = 0;

	if (pattern >= PTS_MAX_NUM_PATTERNS)
		return;

	if (pattern == PTS_32_PATTERN) {
		factor1 = 3;
		factor2 = 2;
		pattern_range =  PTS_32_PATTERN_DETECT_RANGE;
	} else if (pattern == PTS_22_PATTERN) {
		factor1 = 2;
		factor2 = 2;
		pattern_range =  PTS_22_PATTERN_DETECT_RANGE;
	} else if (pattern == PTS_41_PATTERN) {
		/* update 2111 mode detection */
		if (pts_trace == 2) {
			if ((pts_41_pattern_sink[1] == 1) &&
				(pts_41_pattern_sink[2] == 1) &&
				(pts_41_pattern_sink[3] == 1) &&
				(pts_pattern[PTS_41_PATTERN] <
				PTS_41_PATTERN_DETECT_RANGE)) {
				pts_pattern[PTS_41_PATTERN]++;
				if (pts_pattern[PTS_41_PATTERN] ==
					PTS_41_PATTERN_DETECT_RANGE) {
					pts_pattern_enter_cnt[PTS_41_PATTERN]++;
					pts_pattern_detected = pattern;
					if (pts_log_enable[PTS_41_PATTERN])
						pr_info("video 4:1 mode detected\n");
				}
			}
			pts_41_pattern_sink[0] = 2;
			pts_41_pattern_sink_index = 1;
		} else if (pts_trace == 1) {
			if ((pts_41_pattern_sink_index <
				PTS_41_PATTERN_SINK_MAX) &&
				(pts_41_pattern_sink_index > 0)) {
				pts_41_pattern_sink[pts_41_pattern_sink_index]
					= 1;
				pts_41_pattern_sink_index++;
			} else if (pts_pattern[PTS_41_PATTERN] ==
				PTS_41_PATTERN_DETECT_RANGE) {
				pts_pattern[PTS_41_PATTERN] = 0;
				pts_41_pattern_sink_index = 0;
				pts_pattern_exit_cnt[PTS_41_PATTERN]++;
				memset(&pts_41_pattern_sink[0], 0,
					PTS_41_PATTERN_SINK_MAX);
				if (pts_log_enable[PTS_41_PATTERN])
					pr_info("video 4:1 mode broken\n");
			} else {
				pts_pattern[PTS_41_PATTERN] = 0;
				pts_41_pattern_sink_index = 0;
				memset(&pts_41_pattern_sink[0], 0,
					PTS_41_PATTERN_SINK_MAX);
			}
		} else if (pts_pattern[PTS_41_PATTERN] ==
			PTS_41_PATTERN_DETECT_RANGE) {
			pts_pattern[PTS_41_PATTERN] = 0;
			pts_41_pattern_sink_index = 0;
			memset(&pts_41_pattern_sink[0], 0,
				PTS_41_PATTERN_SINK_MAX);
			pts_pattern_exit_cnt[PTS_41_PATTERN]++;
			if (pts_log_enable[PTS_41_PATTERN])
				pr_info("video 4:1 mode broken\n");
		} else {
			pts_pattern[PTS_41_PATTERN] = 0;
			pts_41_pattern_sink_index = 0;
			memset(&pts_41_pattern_sink[0], 0,
				PTS_41_PATTERN_SINK_MAX);
		}
		return;
	}


	/* update 3:2 or 2:2 mode detection */
	if (((pre_pts_trace == factor1) && (pts_trace == factor2)) ||
		((pre_pts_trace == factor2) && (pts_trace == factor1))) {
		if (pts_pattern[pattern] < pattern_range) {
			pts_pattern[pattern]++;
			if (pts_pattern[pattern] == pattern_range) {
				pts_pattern_enter_cnt[pattern]++;
				pts_pattern_detected = pattern;
				if (pts_log_enable[pattern])
					pr_info("video %d:%d mode detected\n",
						factor1, factor2);
			}
		}
	} else if (pts_pattern[pattern] == pattern_range) {
		pts_pattern[pattern] = 0;
		pts_pattern_exit_cnt[pattern]++;
		if (pts_log_enable[pattern])
			pr_info("video %d:%d mode broken\n", factor1, factor2);
	} else
		pts_pattern[pattern] = 0;
}

static void vsync_video_pattern(void)
{
	/* Check for 3:2*/
	log_vsync_video_pattern(PTS_32_PATTERN);
	/* Check for 2:2*/
	log_vsync_video_pattern(PTS_22_PATTERN);
	/* Check for 4:1*/
	log_vsync_video_pattern(PTS_41_PATTERN);
}
#endif

/* for sdr/hdr/single dv switch with dual dv */
static u32 last_el_status;
/* for dual dv switch with different el size */
static u32 last_el_w;
bool has_enhanced_layer(struct vframe_s *vf)
{
	struct provider_aux_req_s req;

	if (!vf)
		return 0;
	if (vf->source_type != VFRAME_SOURCE_TYPE_OTHERS)
		return 0;
	if (!is_dolby_vision_on())
		return 0;

	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;
	vf_notify_provider_by_name("dvbldec",
		VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
		(void *)&req);
	return req.dv_enhance_exist;
}
u32 property_changed_true;

static u64 func_div(u64 number, u32 divid)
{
	u64 tmp = number;

	do_div(tmp, divid);
	return tmp;
}

static void vsync_toggle_frame(struct vframe_s *vf)
{
	u32 first_picture = 0;
	unsigned long flags = 0;
	bool vf_with_el = false;
	bool force_toggle = false;
	long long *clk_array;

	if (vf == NULL)
		return;
	frame_count++;
	toggle_count++;

	if (is_dolby_vision_enable())
		vf_with_el = has_enhanced_layer(vf);

#ifdef PTS_TRACE_DEBUG
#ifdef PTS_TRACE_START
		if (pts_trace_his_rd < 16) {
#endif
			pts_trace_his[pts_trace_his_rd] = pts_trace;
			pts_his[pts_trace_his_rd] = vf->pts;
			scr_his[pts_trace_his_rd] = timestamp_pcrscr_get();
			pts_trace_his_rd++;
			if (pts_trace_his_rd >= 16)
				pts_trace_his_rd = 0;
#ifdef PTS_TRACE_START
		}
#endif
#endif

#ifdef PTS_LOGGING
	vsync_video_pattern();
	pre_pts_trace = pts_trace;
#endif

#if defined(PTS_LOGGING) || defined(PTS_TRACE_DEBUG)
	pts_trace = 0;
#endif

	ori_start_x_lines = 0;
	ori_end_x_lines = ((vf->type & VIDTYPE_COMPRESS) ?
		vf->compWidth : vf->width) - 1;
	ori_start_y_lines = 0;
	ori_end_y_lines = ((vf->type & VIDTYPE_COMPRESS) ?
		vf->compHeight : vf->height) - 1;
	if (debug_flag & DEBUG_FLAG_PRINT_TOGGLE_FRAME) {
		u32 pcr = timestamp_pcrscr_get();
		u32 vpts = timestamp_vpts_get();
		u32 apts = timestamp_apts_get();

		pr_info("%s pts:%d.%06d pcr:%d.%06d vpts:%d.%06d apts:%d.%06d\n",
				__func__, (vf->pts) / 90000,
				((vf->pts) % 90000) * 1000 / 90, (pcr) / 90000,
				((pcr) % 90000) * 1000 / 90, (vpts) / 90000,
				((vpts) % 90000) * 1000 / 90, (apts) / 90000,
				((apts) % 90000) * 1000 / 90);
	}

	if (trickmode_i || trickmode_fffb)
		trickmode_duration_count = trickmode_duration;

	if (vf->early_process_fun) {
		if (vf->early_process_fun(vf->private_data, vf) == 1) {
			/* video_property_changed = true; */
			first_picture = 1;
		}
	} else {
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
		if ((DI_POST_REG_RD(DI_IF1_GEN_REG) & 0x1) != 0) {
			/* check mif enable status, disable post di */
			VSYNC_WR_MPEG_REG(DI_POST_CTRL, 0x3 << 30);
			VSYNC_WR_MPEG_REG(DI_POST_SIZE,
					  (32 - 1) | ((128 - 1) << 16));
			VSYNC_WR_MPEG_REG(DI_IF1_GEN_REG,
					  READ_VCBUS_REG(DI_IF1_GEN_REG) &
					  0xfffffffe);
		}
#endif
	}

	timer_count = 0;
	if ((vf->width == 0) && (vf->height == 0)) {
		amlog_level(LOG_LEVEL_ERROR,
			    "Video: invalid frame dimension\n");
		return;
	}
	if ((cur_dispbuf) && (cur_dispbuf != &vf_local) && (cur_dispbuf != vf)
	    && (video_property_changed != 2)) {
		if (cur_dispbuf->source_type == VFRAME_SOURCE_TYPE_OSD) {
			if (osd_prov && osd_prov->ops && osd_prov->ops->put) {
				osd_prov->ops->put(cur_dispbuf,
						   osd_prov->op_arg);
				if (debug_flag & DEBUG_FLAG_BLACKOUT) {
					pr_info(
					"[v4o]pre vf is osd,put it\n");
				}
			}
			first_picture = 1;
			if (debug_flag & DEBUG_FLAG_BLACKOUT) {
				pr_info(
				"[v4o] pre vf is osd, clear it to NULL\n");
			}
		} else {
			new_frame_count++;
			if (new_frame_count == 1)
				first_picture = 1;
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
			if (is_vsync_rdma_enable()) {
#ifdef RDMA_RECYCLE_ORDERED_VFRAMES
				if (dispbuf_to_put_num < DISPBUF_TO_PUT_MAX) {
					dispbuf_to_put[dispbuf_to_put_num] =
					    cur_dispbuf;
					dispbuf_to_put_num++;
				} else
					video_vf_put(cur_dispbuf);
#else
				if (cur_rdma_buf == cur_dispbuf) {
					dispbuf_to_put[0] = cur_dispbuf;
					dispbuf_to_put_num = 1;
				} else
					video_vf_put(cur_dispbuf);
#endif
			} else {
				int i;

				for (i = 0; i < dispbuf_to_put_num; i++) {
					if (dispbuf_to_put[i]) {
						video_vf_put(
							dispbuf_to_put[i]);
						dispbuf_to_put[i] = NULL;
					}
					dispbuf_to_put_num = 0;
				}
				video_vf_put(cur_dispbuf);
			}
#else
			video_vf_put(cur_dispbuf);
#endif
			if (debug_flag & DEBUG_FLAG_LATENCY) {
				vf->ready_clock[3] = sched_clock();
				pr_info("video toggle latency %lld ms,"
					"video get latency %lld ms,"
					"vdin put latency %lld ms,"
					"first %lld ms.\n",
					func_div(vf->ready_clock[3], 1000),
					func_div(vf->ready_clock[2], 1000),
					func_div(vf->ready_clock[1], 1000),
					func_div(vf->ready_clock[0], 1000));
				cur_dispbuf->ready_clock[4] = sched_clock();
				clk_array = cur_dispbuf->ready_clock;
				pr_info("video put latency %lld ms,"
					"video toggle latency %lld ms,"
					"video get latency %lld ms,"
					"vdin put latency %lld ms,"
					"first %lld ms.\n",
					func_div(*(clk_array + 4), 1000),
					func_div(*(clk_array + 3), 1000),
					func_div(*(clk_array + 2), 1000),
					func_div(*(clk_array + 1), 1000),
					func_div(*clk_array, 1000));
			}
		}

	} else
		first_picture = 1;

	if (video_property_changed) {
		property_changed_true = 2;
		video_property_changed = false;
		first_picture = 1;
	}
	if (property_changed_true > 0) {
		property_changed_true--;
		first_picture = 1;
	}

	if (debug_flag & DEBUG_FLAG_BLACKOUT) {
		if (first_picture) {
			pr_info
			    ("[video4osd] first %s picture {%d,%d} pts:%x,\n",
			     (vf->source_type ==
			      VFRAME_SOURCE_TYPE_OSD) ? "OSD" : "", vf->width,
			     vf->height, vf->pts);
		}
	}
	/* switch buffer */
	post_canvas = vf->canvas0Addr;
	if ((get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) &&
		(vf->type & VIDTYPE_COMPRESS)) {
		VSYNC_WR_MPEG_REG(AFBC_HEAD_BADDR, vf->compHeadAddr>>4);
		VSYNC_WR_MPEG_REG(AFBC_BODY_BADDR, vf->compBodyAddr>>4);
	}
	if ((vf->canvas0Addr != 0)
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
		&& ((DI_POST_REG_RD(DI_POST_CTRL) & 0x1000) == 0)
		/* check di_post_viu_link   */
#endif
		) {

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		if (vf->canvas0Addr != (u32)-1) {
			canvas_copy(vf->canvas0Addr & 0xff,
				disp_canvas_index[rdma_canvas_id][0]);
			canvas_copy((vf->canvas0Addr >> 8) & 0xff,
				disp_canvas_index[rdma_canvas_id][1]);
			canvas_copy((vf->canvas0Addr >> 16) & 0xff,
				disp_canvas_index[rdma_canvas_id][2]);
		} else {
			vframe_canvas_set(&vf->canvas0_config[0],
				vf->plane_num,
				&disp_canvas_index[rdma_canvas_id][0]);
		}
		if (!vf_with_el) {
			if (vf->canvas1Addr != (u32)-1) {
				canvas_copy(vf->canvas1Addr & 0xff,
					disp_canvas_index[rdma_canvas_id][3]);
				canvas_copy((vf->canvas1Addr >> 8) & 0xff,
					disp_canvas_index[rdma_canvas_id][4]);
				canvas_copy((vf->canvas1Addr >> 16) & 0xff,
					disp_canvas_index[rdma_canvas_id][5]);
			} else {
				vframe_canvas_set(&vf->canvas1_config[0],
					vf->plane_num,
					&disp_canvas_index[rdma_canvas_id][3]);
			}
		}

		VSYNC_WR_MPEG_REG(
			VD1_IF0_CANVAS0 + cur_dev->viu_off,
			disp_canvas[rdma_canvas_id][0]);
		if (platform_type == 0) {
			VSYNC_WR_MPEG_REG(VD1_IF0_CANVAS1 + cur_dev->viu_off,
					  disp_canvas[rdma_canvas_id][0]);
			if (!vf_with_el) {
				VSYNC_WR_MPEG_REG(
					VD2_IF0_CANVAS0 + cur_dev->viu_off,
					disp_canvas[rdma_canvas_id][1]);
				VSYNC_WR_MPEG_REG(
					VD2_IF0_CANVAS1 + cur_dev->viu_off,
					disp_canvas[rdma_canvas_id][1]);
			}
		} else {
			if (!vf_with_el)
				VSYNC_WR_MPEG_REG(
					VD2_IF0_CANVAS0 + cur_dev->viu_off,
					disp_canvas[rdma_canvas_id][1]);
			if (cur_frame_par &&
			(cur_frame_par->vpp_2pic_mode == 1)) {
				VSYNC_WR_MPEG_REG(VD1_IF0_CANVAS1 +
				cur_dev->viu_off,
				disp_canvas[rdma_canvas_id][0]);
				if (!vf_with_el)
					VSYNC_WR_MPEG_REG(VD2_IF0_CANVAS1 +
					cur_dev->viu_off,
					disp_canvas[rdma_canvas_id][0]);
			} else {
				VSYNC_WR_MPEG_REG(VD1_IF0_CANVAS1 +
				cur_dev->viu_off,
				disp_canvas[rdma_canvas_id][1]);
				if (!vf_with_el)
					VSYNC_WR_MPEG_REG(VD2_IF0_CANVAS1 +
					cur_dev->viu_off,
					disp_canvas[rdma_canvas_id][0]);
			}
		}
		if (cur_frame_par
		&& (process_3d_type & MODE_3D_ENABLE)
		&& (process_3d_type & MODE_3D_TO_2D_R)
		&& (cur_frame_par->vpp_2pic_mode == VPP_SELECT_PIC1)
		&& !vf_with_el) {
			VSYNC_WR_MPEG_REG(VD1_IF0_CANVAS0 + cur_dev->viu_off,
					  disp_canvas[rdma_canvas_id][1]);
			VSYNC_WR_MPEG_REG(VD1_IF0_CANVAS1 + cur_dev->viu_off,
					  disp_canvas[rdma_canvas_id][1]);
			VSYNC_WR_MPEG_REG(VD2_IF0_CANVAS0 + cur_dev->viu_off,
					  disp_canvas[rdma_canvas_id][1]);
			VSYNC_WR_MPEG_REG(VD2_IF0_CANVAS1 + cur_dev->viu_off,
					  disp_canvas[rdma_canvas_id][1]);
		}
		/* VSYNC_WR_MPEG_REG(VD2_IF0_CANVAS1,*/
		/*disp_canvas[rdma_canvas_id][1]); */
		next_rdma_canvas_id = rdma_canvas_id ? 0 : 1;
#else
		canvas_copy(vf->canvas0Addr & 0xff, disp_canvas_index[0]);
		canvas_copy((vf->canvas0Addr >> 8) & 0xff,
			    disp_canvas_index[1]);
		canvas_copy((vf->canvas0Addr >> 16) & 0xff,
			    disp_canvas_index[2]);
		canvas_copy(vf->canvas1Addr & 0xff, disp_canvas_index[3]);
		canvas_copy((vf->canvas1Addr >> 8) & 0xff,
			    disp_canvas_index[4]);
		canvas_copy((vf->canvas1Addr >> 16) & 0xff,
			    disp_canvas_index[5]);
		if (platform_type == 0) {
			VSYNC_WR_MPEG_REG(
				VD1_IF0_CANVAS0 + cur_dev->viu_off,
				disp_canvas[0]);
			VSYNC_WR_MPEG_REG(
				VD1_IF0_CANVAS1 + cur_dev->viu_off,
				disp_canvas[0]);
			if (!vf_with_el) {
				VSYNC_WR_MPEG_REG(
					VD2_IF0_CANVAS0 + cur_dev->viu_off,
					disp_canvas[1]);
				VSYNC_WR_MPEG_REG(
					VD2_IF0_CANVAS1 + cur_dev->viu_off,
					disp_canvas[1]);
			}
		} else {
			VSYNC_WR_MPEG_REG(VD1_IF0_CANVAS0 + cur_dev->viu_off,
					  disp_canvas[0]);
			if (!vf_with_el)
				VSYNC_WR_MPEG_REG(
					VD2_IF0_CANVAS0 + cur_dev->viu_off,
					disp_canvas[0]);
			if (cur_frame_par &&
			(cur_frame_par->vpp_2pic_mode == 1)) {
				VSYNC_WR_MPEG_REG(VD1_IF0_CANVAS1 +
				cur_dev->viu_off,
				disp_canvas[0]);
				if (!vf_with_el)
					VSYNC_WR_MPEG_REG(VD2_IF0_CANVAS1 +
					cur_dev->viu_off,
					disp_canvas[0]);
			} else {
				VSYNC_WR_MPEG_REG(VD1_IF0_CANVAS1 +
				cur_dev->viu_off,
				disp_canvas[1]);
				if (!vf_with_el)
					VSYNC_WR_MPEG_REG(VD2_IF0_CANVAS1 +
					cur_dev->viu_off,
					disp_canvas[1]);
			}
			/* VSYNC_WR_MPEG_REG(VD2_IF0_CANVAS0 +*/
			/*cur_dev->viu_off, disp_canvas[0]); */
			/* VSYNC_WR_MPEG_REG(VD2_IF0_CANVAS1 +*/
			/*cur_dev->viu_off, disp_canvas[1]); */
		}
#endif
	}
	/* set video PTS */
	if (cur_dispbuf != vf) {
		if (vf->source_type != VFRAME_SOURCE_TYPE_OSD) {
			if (vf->pts != 0) {
				amlog_mask(LOG_MASK_TIMESTAMP,
				"vpts to: 0x%x, scr: 0x%x, abs_scr: 0x%x\n",
					   vf->pts, timestamp_pcrscr_get(),
					   READ_MPEG_REG(SCR_HIU));

				timestamp_vpts_set(vf->pts);
			} else if (cur_dispbuf) {
				amlog_mask(LOG_MASK_TIMESTAMP,
				"vpts inc: 0x%x, scr: 0x%x, abs_scr: 0x%x\n",
					   timestamp_vpts_get() +
					   DUR2PTS(cur_dispbuf->duration),
					   timestamp_pcrscr_get(),
					   READ_MPEG_REG(SCR_HIU));

				timestamp_vpts_inc(DUR2PTS
						   (cur_dispbuf->duration));

				vpts_remainder +=
				    DUR2PTS_RM(cur_dispbuf->duration);
				if (vpts_remainder >= 0xf) {
					vpts_remainder -= 0xf;
					timestamp_vpts_inc(-1);
				}
			}
		} else {
			first_picture = 1;
			if (debug_flag & DEBUG_FLAG_BLACKOUT) {
				pr_info(
				"[v4o] cur vframe is osd, do not set PTS\n");
			}
		}
		vf->type_backup = vf->type;
	}

	if (cur_dispbuf && vf &&
		(cur_dispbuf->ratio_control &
		DISP_RATIO_ADAPTED_PICMODE) &&
		(cur_dispbuf->ratio_control ==
		vf->ratio_control) &&
		memcmp(&cur_dispbuf->pic_mode, &vf->pic_mode,
		sizeof(struct vframe_pic_mode_s)))
		force_toggle = true;

	if ((last_process_3d_type != process_3d_type)
		|| (last_el_status != vf_with_el))
		force_toggle = true;

	/* enable new config on the new frames */
	if (first_picture || force_toggle ||
		(cur_dispbuf &&
		((cur_dispbuf->bufWidth != vf->bufWidth) ||
		(cur_dispbuf->width != vf->width) ||
		(cur_dispbuf->height != vf->height) ||
		(cur_dispbuf->bitdepth != vf->bitdepth) ||
		(cur_dispbuf->trans_fmt != vf->trans_fmt) ||
		(cur_dispbuf->ratio_control != vf->ratio_control) ||
		((cur_dispbuf->type_backup & VIDTYPE_INTERLACE) !=
		(vf->type_backup & VIDTYPE_INTERLACE)) ||
		(cur_dispbuf->type != vf->type)))) {
		last_process_3d_type = process_3d_type;
		atomic_inc(&video_sizechange);
		wake_up_interruptible(&amvideo_sizechange_wait);
		amlog_mask(LOG_MASK_FRAMEINFO,
			   "%s %dx%d  ar=0x%x\n",
			   ((vf->type & VIDTYPE_TYPEMASK) ==
			    VIDTYPE_INTERLACE_TOP) ? "interlace-top"
			   : ((vf->type & VIDTYPE_TYPEMASK)
			      == VIDTYPE_INTERLACE_BOTTOM)
			   ? "interlace-bottom" : "progressive", vf->width,
			   vf->height, vf->ratio_control);
#ifdef TV_3D_FUNCTION_OPEN
		amlog_mask(LOG_MASK_FRAMEINFO,
			   "%s trans_fmt=%u\n", __func__, vf->trans_fmt);

#endif
		next_frame_par = (&frame_parms[0] == next_frame_par) ?
		    &frame_parms[1] : &frame_parms[0];

		vpp_set_filters(process_3d_type, wide_setting, vf,
			next_frame_par, vinfo,
			(is_dolby_vision_on() &&
			is_dolby_vision_stb_mode()));

		/* apply new vpp settings */
		frame_par_ready_to_set = 1;

		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
		if ((get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		    && !is_meson_mtvd_cpu()) {
			if (((vf->width > 1920) && (vf->height > 1088)) ||
				((vf->type & VIDTYPE_COMPRESS) &&
				 (vf->compWidth > 1920) &&
				 (vf->compHeight > 1080))) {
				if (vpu_clk_level == 0) {
					vpu_clk_level = 1;

					spin_lock_irqsave(&lock, flags);
					vpu_delay_work_flag |=
					    VPU_DELAYWORK_VPU_CLK;
					spin_unlock_irqrestore(&lock, flags);
				}
			} else {
				if (vpu_clk_level == 1) {
					vpu_clk_level = 0;

					spin_lock_irqsave(&lock, flags);
					vpu_delay_work_flag |=
					    VPU_DELAYWORK_VPU_CLK;
					spin_unlock_irqrestore(&lock, flags);
				}
			}
		}
		/* #endif */

	}
	last_el_status = vf_with_el;

	if (((vf->type & VIDTYPE_NO_VIDEO_ENABLE) == 0) &&
	    ((!property_changed_true) || (vf != cur_dispbuf))) {
		if (disable_video == VIDEO_DISABLE_FORNEXT) {
			EnableVideoLayer();
			disable_video = VIDEO_DISABLE_NONE;
		}
		if (first_picture && (disable_video != VIDEO_DISABLE_NORMAL)) {
			EnableVideoLayer();

			if ((vf->type & VIDTYPE_MVC) ||
			(cur_dispbuf2 && (cur_dispbuf2->type & VIDTYPE_VD2)))
				EnableVideoLayer2();
			else if (cur_dispbuf2 &&
			!(cur_dispbuf2->type & VIDTYPE_COMPRESS))
				VD2_MEM_POWER_ON();
			else if (vf_with_el)
				EnableVideoLayer2();
			else if (need_disable_vd2)
				DisableVideoLayer2();
		}
	}
	if (cur_dispbuf && (cur_dispbuf->type != vf->type)) {
		if ((vf->type & VIDTYPE_MVC) ||
		(cur_dispbuf2 && (cur_dispbuf2->type & VIDTYPE_VD2)))
			EnableVideoLayer2();
		else {
			if (cur_dispbuf2 &&
			!(cur_dispbuf2->type & VIDTYPE_COMPRESS))
				VD2_MEM_POWER_ON();
			else if (vf_with_el)
				EnableVideoLayer2();
			else if (need_disable_vd2)
				DisableVideoLayer2();
		}
	}
	cur_dispbuf = vf;
	if (first_picture) {
		frame_par_ready_to_set = 1;
		first_frame_toggled = 1;

#ifdef VIDEO_PTS_CHASE
		av_sync_flag = 0;
#endif
	}
	if (cur_dispbuf != &vf_local)
		video_keeper_new_frame_notify();

	if ((vf != &vf_local) && (vf) && !vsync_pts_aligned) {
#ifdef PTS_TRACE_DEBUG
		pr_info("####timestamp_pcrscr_get() = 0x%x, vf->pts = 0x%x, vsync_pts_inc = %d\n",
			timestamp_pcrscr_get(), vf->pts, vsync_pts_inc);
#endif
		if ((abs(timestamp_pcrscr_get() - vf->pts) <= (vsync_pts_inc))
			  && ((int)(timestamp_pcrscr_get() - vf->pts) >= 0)) {
			vsync_pts_align =  vsync_pts_inc / 4 -
				(timestamp_pcrscr_get() - vf->pts);
			vsync_pts_aligned = true;
#ifdef PTS_TRACE_DEBUG
			pts_trace_his_rd = 0;
			pr_info("####vsync_pts_align set to %d\n",
				vsync_pts_align);
#endif
		}
	}
}
static inline void vd1_path_select(bool afbc)
{
	u32 misc_off = cur_dev->vpp_off;

	if (!legacy_vpp) {
		VSYNC_WR_MPEG_REG_BITS(
			VD1_AFBCD0_MISC_CTRL,
			/* go field sel */
			(0 << 20) |
			/* linebuffer en */
			(0 << 16) |
			/* vd1 -> dolby -> vpp top */
			(0 << 14) |
			/* axi sel: vd1 mif or afbc */
			((afbc ? 1 : 0) << 12) |
			/* data sel: vd1 & afbc0 (not osd4) */
			(0 << 11) |
			/* data sel: afbc0 or vd1 */
			((afbc ? 1 : 0) << 10) |
			/* afbc0 to vd1 (not di) */
			(0 << 9) |
			/* vd1 mif to vpp (not di) */
			(0 << 8) |
			/* afbc0 gclk ctrl */
			(0 << 0),
			0, 22);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			VSYNC_WR_MPEG_REG_BITS(
				VD1_AFBCD0_MISC_CTRL,
				/* Vd1_afbc0_mem_sel */
				(afbc ? 1 : 0),
				22, 1);

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
		if (!cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
			return;
		if ((DI_POST_REG_RD(DI_POST_CTRL) & 0x100) != 0) {
			/* check di_vpp_out_en bit */
			VSYNC_WR_MPEG_REG_BITS(
				VD1_AFBCD0_MISC_CTRL,
				/* vd1 mif to di */
				1,
				8, 2);
			VSYNC_WR_MPEG_REG_BITS(
				VD1_AFBCD0_MISC_CTRL,
				/* go field select di post */
				1,
				20, 2);
		}
#endif
	} else {
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
		if ((DI_POST_REG_RD(DI_POST_CTRL) & 0x100) == 0)
		/* mif sel */
#endif
			VSYNC_WR_MPEG_REG_BITS(
				VIU_MISC_CTRL0 + misc_off,
				0, 16, 3);
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL0 + misc_off,
			(afbc ? 1 : 0), 20, 1);
	}
}

static inline void vd2_path_select(bool afbc)
{
	u32 misc_off = cur_dev->vpp_off;

	if (!legacy_vpp) {
		VSYNC_WR_MPEG_REG_BITS(
			VD2_AFBCD1_MISC_CTRL,
			/* go field sel */
			(0 << 20) |
			/* linebuffer en */
			(0 << 16) |
			/* TODO: vd2 -> dolby -> vpp top ?? */
			(0 << 14) |
			/* axi sel: vd2 mif */
			((afbc ? 1 : 0) << 12) |
			/* data sel: vd2 & afbc1 (not osd4) */
			(0 << 11) |
			/* data sel: afbc1 */
			((afbc ? 1 : 0) << 10) |
			/* afbc1 to vd2 (not di) */
			(0 << 9) |
			/* vd2 mif to vpp (not di) */
			(0 << 8) |
			/* afbc1 gclk ctrl */
			(0 << 0),
			0, 22);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			VSYNC_WR_MPEG_REG_BITS(
				VD2_AFBCD1_MISC_CTRL,
				/* Vd2_afbc0_mem_sel */
				(afbc ? 1 : 0),
				22, 1);
	} else {
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL1 + misc_off,
			(afbc ? 2 : 0), 0, 2);
	}
}

static void viu_set_dcu(struct vpp_frame_par_s *frame_par, struct vframe_s *vf)
{
	u32 r;
	u32 vphase, vini_phase, vformatter;
	u32 pat, loop;
	static const u32 vpat[MAX_VSKIP_COUNT + 1] = {
		0, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
	u32 u, v;
	u32 type, bit_mode = 0;
	bool vf_with_el = false;

	if (vf == NULL) {
		pr_info("viu_set_dcu vf NULL, return\n");
		return;
	}
	type = vf->type;
	pr_debug("set dcu for vd1 %p, type:0x%x\n", vf, type);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		if (frame_par->nocomp)
			type &= ~VIDTYPE_COMPRESS;

		if (type & VIDTYPE_COMPRESS) {
			r = (3 << 24) |
			    (vpp_hold_line << 16) |
			    ((legacy_vpp ? 1 : 2) << 14) | /* burst1 */
			    (vf->bitdepth & BITDEPTH_MASK);

			if (frame_par->hscale_skip_count)
				r |= 0x33;
			if (frame_par->vscale_skip_count)
				r |= 0xcc;
#ifdef TV_REVERSE
			if (reverse)
				r |= (1<<26)|(1<<27);
#endif
			if (vf->bitdepth & BITDEPTH_SAVING_MODE)
				r |= (1<<28); /* mem_saving_mode */
			if (type & VIDTYPE_SCATTER)
				r |= (1<<29);
			VSYNC_WR_MPEG_REG(AFBC_MODE, r);
			VSYNC_WR_MPEG_REG(AFBC_ENABLE, 0x1700);

			r = 0x100;
			/* need check the vf->type 444/422/420 */
			/* current use 420 as default for tl1 */
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
				r |= (2 << 12);
			VSYNC_WR_MPEG_REG(AFBC_CONV_CTRL, r);

			u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
			v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
			VSYNC_WR_MPEG_REG(AFBC_DEC_DEF_COLOR,
				0x3FF00000 | /*Y,bit20+*/
				0x80 << (u + 10) |
				0x80 << v);
			/* chroma formatter */
			/* TODO: afbc setting only cover 420 for now */
/*
#ifdef TV_REVERSE
			if (reverse) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
				if (is_meson_txlx_package_962X()
				&& !is_dolby_vision_stb_mode()
				&& is_dolby_vision_on())
					VSYNC_WR_MPEG_REG(
						AFBC_VD_CFMT_CTRL,
						HFORMATTER_REPEAT |
						HFORMATTER_YC_RATIO_2_1 |
						HFORMATTER_EN |
						VFORMATTER_ALWAYS_RPT |
						(0 << VFORMATTER_INIPHASE_BIT) |
						(0x8 << VFORMATTER_PHASE_BIT) |
						VFORMATTER_EN);
				else
#endif
					VSYNC_WR_MPEG_REG(AFBC_VD_CFMT_CTRL,
						HFORMATTER_REPEAT |
					(0xc << VFORMATTER_INIPHASE_BIT) |
						HFORMATTER_YC_RATIO_2_1 |
						HFORMATTER_EN |
						VFORMATTER_RPTLINE0_EN |
						(0x8 << VFORMATTER_PHASE_BIT) |
						VFORMATTER_EN);
			} else
#endif
*/
			{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
				if (is_meson_txlx_package_962X()
				&& !is_dolby_vision_stb_mode()
				&& is_dolby_vision_on())
					VSYNC_WR_MPEG_REG(
						AFBC_VD_CFMT_CTRL,
						HFORMATTER_REPEAT |
						HFORMATTER_YC_RATIO_2_1 |
						HFORMATTER_EN |
						VFORMATTER_ALWAYS_RPT |
						(0 << VFORMATTER_INIPHASE_BIT) |
						(0x8 << VFORMATTER_PHASE_BIT) |
						VFORMATTER_EN);
				else
#endif
					VSYNC_WR_MPEG_REG(AFBC_VD_CFMT_CTRL,
						HFORMATTER_REPEAT |
					(0xc << VFORMATTER_INIPHASE_BIT) |
						HFORMATTER_YC_RATIO_2_1 |
						HFORMATTER_EN |
						VFORMATTER_RPTLINE0_EN |
						(0x8 << VFORMATTER_PHASE_BIT) |
						VFORMATTER_EN);
			}
			vd1_path_select(true);

			VSYNC_WR_MPEG_REG(
				VD1_IF0_GEN_REG + cur_dev->viu_off, 0);
			return;

		} else {
			if ((vf->bitdepth & BITDEPTH_Y10) &&
			(!frame_par->nocomp)) {
				if (vf->type & VIDTYPE_VIU_444) {
					bit_mode = 2;
				} else {
					if (vf->bitdepth & FULL_PACK_422_MODE)
						bit_mode = 3;
					else
						bit_mode = 1;
				}
			} else {
				bit_mode = 0;
			}
			if (!legacy_vpp) {
				VSYNC_WR_MPEG_REG_BITS(
					G12_VD1_IF0_GEN_REG3,
					(bit_mode & 0x3), 8, 2);
				if ((vf->type & VIDTYPE_MVC) && (!vf_with_el))
					VSYNC_WR_MPEG_REG_BITS(
						G12_VD2_IF0_GEN_REG3,
						(bit_mode & 0x3), 8, 2);
			} else {
				VSYNC_WR_MPEG_REG_BITS(
					VD1_IF0_GEN_REG3 +
					cur_dev->viu_off,
					(bit_mode & 0x3), 8, 2);
				if ((vf->type & VIDTYPE_MVC) && (!vf_with_el))
					VSYNC_WR_MPEG_REG_BITS(
						VD2_IF0_GEN_REG3 +
						cur_dev->viu_off,
						(bit_mode & 0x3), 8, 2);
			}
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
			DI_POST_WR_REG_BITS(DI_IF1_GEN_REG3,
				(bit_mode&0x3), 8, 2);
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
				DI_POST_WR_REG_BITS(DI_IF2_GEN_REG3,
					(bit_mode & 0x3), 8, 2);
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
				DI_POST_WR_REG_BITS(DI_IF0_GEN_REG3,
					(bit_mode & 0x3), 8, 2);
#endif
			vd1_path_select(false);
			VSYNC_WR_MPEG_REG(AFBC_ENABLE, 0);
		}
	}

	r = (3 << VDIF_URGENT_BIT) |
	    (vpp_hold_line << VDIF_HOLD_LINES_BIT) |
	    VDIF_FORMAT_SPLIT |
	    VDIF_CHRO_RPT_LAST | VDIF_ENABLE;
	/*  | VDIF_RESET_ON_GO_FIELD;*/
	if (debug_flag & DEBUG_FLAG_GOFIELD_MANUL)
		r |= 1<<7; /*for manul triggle gofiled.*/

	if ((type & VIDTYPE_VIU_SINGLE_PLANE) == 0)
		r |= VDIF_SEPARATE_EN;
	else {
		if (type & VIDTYPE_VIU_422)
			r |= VDIF_FORMAT_422;
		else {
			r |= VDIF_FORMAT_RGB888_YUV444 |
			    VDIF_DEMUX_MODE_RGB_444;
		}
	}
	if (is_dolby_vision_enable())
		vf_with_el = has_enhanced_layer(vf);
	if (frame_par->hscale_skip_count)
		r |= VDIF_CHROMA_HZ_AVG | VDIF_LUMA_HZ_AVG;

	/*enable go field reset default according to vlsi*/
	r |= VDIF_RESET_ON_GO_FIELD;
	VSYNC_WR_MPEG_REG(VD1_IF0_GEN_REG + cur_dev->viu_off, r);
	if (!vf_with_el)
		VSYNC_WR_MPEG_REG(
			VD2_IF0_GEN_REG + cur_dev->viu_off, r);

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		if (type & VIDTYPE_VIU_NV21) {
			VSYNC_WR_MPEG_REG_BITS(VD1_IF0_GEN_REG2 +
				cur_dev->viu_off, 1, 0, 1);
		} else {
			VSYNC_WR_MPEG_REG_BITS(VD1_IF0_GEN_REG2 +
				cur_dev->viu_off, 0, 0, 1);
		}
#if HAS_VPU_PROT
		if (has_vpu_prot()) {
			if (use_prot) {
				if (vf->video_angle == 2) {
					VSYNC_WR_MPEG_REG_BITS(VD1_IF0_GEN_REG2
						+
					cur_dev->viu_off,
						0xf, 2, 4);
				} else {
					VSYNC_WR_MPEG_REG_BITS(VD1_IF0_GEN_REG2
						+
					cur_dev->viu_off,
					0, 2, 4);
				}
			}
		}
#else
#ifdef TV_REVERSE
		if (reverse) {
			VSYNC_WR_MPEG_REG_BITS((VD1_IF0_GEN_REG2 +
				cur_dev->viu_off), 0xf, 2, 4);
			if ((vf->type & VIDTYPE_MVC) && (!vf_with_el))
				VSYNC_WR_MPEG_REG_BITS((VD2_IF0_GEN_REG2 +
					cur_dev->viu_off), 0xf, 2, 4);
		} else {
			VSYNC_WR_MPEG_REG_BITS((VD1_IF0_GEN_REG2 +
				cur_dev->viu_off), 0, 2, 4);
			if ((vf->type & VIDTYPE_MVC) && (!vf_with_el))
				VSYNC_WR_MPEG_REG_BITS((VD2_IF0_GEN_REG2 +
					cur_dev->viu_off), 0, 2, 4);
		}
#endif
#endif
	}
	/* #endif */

	/* chroma formatter */
	if (type & VIDTYPE_VIU_444) {
		VSYNC_WR_MPEG_REG(VIU_VD1_FMT_CTRL + cur_dev->viu_off,
				  HFORMATTER_YC_RATIO_1_1);
		if (!vf_with_el)
			VSYNC_WR_MPEG_REG(VIU_VD2_FMT_CTRL + cur_dev->viu_off,
				  HFORMATTER_YC_RATIO_1_1);
	} else if (type & VIDTYPE_VIU_FIELD) {
		vini_phase = 0xc << VFORMATTER_INIPHASE_BIT;
		vphase =
		    ((type & VIDTYPE_VIU_422) ? 0x10 : 0x08) <<
		    VFORMATTER_PHASE_BIT;

		/*vlsi suggest only for yuv420 vformatter shold be 1*/
		if (type & VIDTYPE_VIU_NV21)
			vformatter = VFORMATTER_EN;
		else
			vformatter = 0;
		if (is_meson_txlx_package_962X()
		&& !is_dolby_vision_stb_mode()
		&& is_dolby_vision_on()) {
			VSYNC_WR_MPEG_REG(
				VIU_VD1_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_REPEAT |
				HFORMATTER_YC_RATIO_2_1 |
				HFORMATTER_EN |
				((type & VIDTYPE_VIU_422) ?
				VFORMATTER_RPTLINE0_EN :
				VFORMATTER_ALWAYS_RPT) |
				(0 << VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) |
				((type & VIDTYPE_VIU_422) ?
				0 :
				VFORMATTER_EN));
			pr_debug("\tvd1 set fmt(dovi tv)\n");
		} else if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu() ||
			is_meson_txlx_cpu()) {
			if ((vf->width >= 3840) &&
			(vf->height >= 2160) &&
			(type & VIDTYPE_VIU_422)) {
				VSYNC_WR_MPEG_REG(
				VIU_VD1_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_YC_RATIO_2_1 | HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN | vini_phase | vphase);
				if (!vf_with_el)
					VSYNC_WR_MPEG_REG(
					VIU_VD2_FMT_CTRL + cur_dev->viu_off,
					HFORMATTER_RRT_PIXEL0 |
					HFORMATTER_YC_RATIO_2_1 |
					HFORMATTER_EN |
					VFORMATTER_RPTLINE0_EN |
					vini_phase | vphase);
			} else {
				VSYNC_WR_MPEG_REG(
				VIU_VD1_FMT_CTRL + cur_dev->viu_off,
				(is_dolby_vision_on() ?
				HFORMATTER_REPEAT : 0) |
				HFORMATTER_YC_RATIO_2_1 | HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN | vini_phase | vphase |
				vformatter);
				if (!vf_with_el)
					VSYNC_WR_MPEG_REG(
					VIU_VD2_FMT_CTRL + cur_dev->viu_off,
					HFORMATTER_YC_RATIO_2_1 |
					HFORMATTER_EN |
					VFORMATTER_RPTLINE0_EN |
					vini_phase | vphase |
					vformatter);
			}
		} else {
			VSYNC_WR_MPEG_REG(
				VIU_VD1_FMT_CTRL + cur_dev->viu_off,
				(is_dolby_vision_on() ?
				HFORMATTER_REPEAT : 0) |
				HFORMATTER_YC_RATIO_2_1 | HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN |
				vini_phase | vphase |
				vformatter);
			if (!vf_with_el)
				VSYNC_WR_MPEG_REG(
					VIU_VD2_FMT_CTRL + cur_dev->viu_off,
					HFORMATTER_YC_RATIO_2_1 |
					HFORMATTER_EN |
					VFORMATTER_RPTLINE0_EN |
					vini_phase | vphase |
					vformatter);
		}
	} else if (type & VIDTYPE_MVC) {
		VSYNC_WR_MPEG_REG(VIU_VD1_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_YC_RATIO_2_1 |
				HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN |
				(0xe << VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) | VFORMATTER_EN);
		if (!vf_with_el)
			VSYNC_WR_MPEG_REG(VIU_VD2_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_YC_RATIO_2_1 | HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN | (0xa <<
				VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) | VFORMATTER_EN);
	} else if ((type & VIDTYPE_INTERLACE)
	&& (((type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP))) {
		VSYNC_WR_MPEG_REG(VIU_VD1_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_YC_RATIO_2_1 | HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN | (0xe <<
				VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) | VFORMATTER_EN);
		if (!vf_with_el)
			VSYNC_WR_MPEG_REG(VIU_VD2_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_YC_RATIO_2_1 |
				HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN |
				(0xe << VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) | VFORMATTER_EN);
	} else {
		if (is_meson_txlx_package_962X()
		&& !is_dolby_vision_stb_mode()
		&& is_dolby_vision_on()) {
			VSYNC_WR_MPEG_REG(
				VIU_VD1_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_REPEAT |
				HFORMATTER_YC_RATIO_2_1 |
				HFORMATTER_EN |
				VFORMATTER_ALWAYS_RPT |
				(0 << VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) |
				VFORMATTER_EN);
		} else {
			VSYNC_WR_MPEG_REG(
				VIU_VD1_FMT_CTRL + cur_dev->viu_off,
				(is_dolby_vision_on() ?
				HFORMATTER_REPEAT : 0) |
				HFORMATTER_YC_RATIO_2_1 |
				HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN |
				(0xa << VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) | VFORMATTER_EN);
		}
		if (!vf_with_el)
			VSYNC_WR_MPEG_REG(VIU_VD2_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_YC_RATIO_2_1 |
				HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN |
				(0xa << VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) | VFORMATTER_EN);
	}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if ((is_meson_txlx_cpu()
		|| is_meson_g12a_cpu()
		|| is_meson_g12b_cpu())
		&& is_dolby_vision_on()
		&& is_dolby_vision_stb_mode()
		&& (vf->source_type ==
		VFRAME_SOURCE_TYPE_OTHERS))
		VSYNC_WR_MPEG_REG_BITS(
			VIU_VD1_FMT_CTRL + cur_dev->viu_off,
			1, 29, 1);
#endif
	/* LOOP/SKIP pattern */
	pat = vpat[frame_par->vscale_skip_count];

	if (type & VIDTYPE_VIU_FIELD) {
		loop = 0;

		if (type & VIDTYPE_INTERLACE)
			pat = vpat[frame_par->vscale_skip_count >> 1];
	} else if (type & VIDTYPE_MVC) {
		loop = 0x11;
		if (is_need_framepacking_output()) {
			pat = 0;
		} else
			pat = 0x80;
	} else if ((type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) {
		loop = 0x11;
		pat <<= 4;
	} else
		loop = 0;

	VSYNC_WR_MPEG_REG(
		VD1_IF0_RPT_LOOP + cur_dev->viu_off,
		(loop << VDIF_CHROMA_LOOP1_BIT) |
		(loop << VDIF_LUMA_LOOP1_BIT) |
		(loop << VDIF_CHROMA_LOOP0_BIT) |
		(loop << VDIF_LUMA_LOOP0_BIT));
	if (!vf_with_el)
		VSYNC_WR_MPEG_REG(
			VD2_IF0_RPT_LOOP + cur_dev->viu_off,
			(loop << VDIF_CHROMA_LOOP1_BIT) |
			(loop << VDIF_LUMA_LOOP1_BIT) |
			(loop << VDIF_CHROMA_LOOP0_BIT) |
			(loop << VDIF_LUMA_LOOP0_BIT));

	VSYNC_WR_MPEG_REG(VD1_IF0_LUMA0_RPT_PAT + cur_dev->viu_off, pat);
	VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA0_RPT_PAT + cur_dev->viu_off, pat);
	VSYNC_WR_MPEG_REG(VD1_IF0_LUMA1_RPT_PAT + cur_dev->viu_off, pat);
	VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA1_RPT_PAT + cur_dev->viu_off, pat);

	if (type & VIDTYPE_MVC) {
		if (is_need_framepacking_output())
			pat = 0;
		else
			pat = 0x88;
	}
	if (!vf_with_el) {
		VSYNC_WR_MPEG_REG(
			VD2_IF0_LUMA0_RPT_PAT + cur_dev->viu_off, pat);
		VSYNC_WR_MPEG_REG(
			VD2_IF0_CHROMA0_RPT_PAT + cur_dev->viu_off, pat);
		VSYNC_WR_MPEG_REG(
			VD2_IF0_LUMA1_RPT_PAT + cur_dev->viu_off, pat);
		VSYNC_WR_MPEG_REG(
			VD2_IF0_CHROMA1_RPT_PAT + cur_dev->viu_off, pat);
	}

	if (platform_type == 0) {
		/* picture 0/1 control */
		if (((type & VIDTYPE_INTERLACE) == 0) &&
			((type & VIDTYPE_VIU_FIELD) == 0) &&
			((type & VIDTYPE_MVC) == 0)) {
			/* progressive frame in two pictures */
			VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_PSEL +
			cur_dev->viu_off, (2 << 26) |	/* two pic mode */
			(2 << 24) |	/* use own last line */
			(2 << 8) |	/* toggle pic 0 and 1, use pic0 first */
			(0x01));	/* loop pattern */
			VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_PSEL +
			cur_dev->viu_off,
			(2 << 26) |	/* two pic mode */
			(2 << 24) |	/* use own last line */
			(2 << 8) |	/* toggle pic 0 and 1, use pic0 first */
			(0x01));	/* loop pattern */
		} else {
			VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_PSEL +
			cur_dev->viu_off, 0);
			VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_PSEL +
			cur_dev->viu_off, 0);
			if (!vf_with_el) {
				VSYNC_WR_MPEG_REG(
					VD2_IF0_LUMA_PSEL +
					cur_dev->viu_off, 0);
				VSYNC_WR_MPEG_REG(
					VD2_IF0_CHROMA_PSEL +
					cur_dev->viu_off, 0);
			}
		}
	} else {
		/* picture 0/1 control */
		if ((((type & VIDTYPE_INTERLACE) == 0) &&
			 ((type & VIDTYPE_VIU_FIELD) == 0) &&
			 ((type & VIDTYPE_MVC) == 0)) ||
			(frame_par->vpp_2pic_mode & 0x3)) {
			/* progressive frame in two pictures */
			if (frame_par->vpp_2pic_mode & VPP_PIC1_FIRST) {
				VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_PSEL +
				cur_dev->viu_off, (2 << 26) |
				/* two pic mode */
				(2 << 24) |
				/* use own last line */
				(1 << 8) |
				/* toggle pic 0 and 1, use pic1 first*/
				(0x01));
				/* loop pattern */
				VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_PSEL +
				cur_dev->viu_off, (2 << 26) |
				/* two pic mode */
				(2 << 24) |
				/* use own last line */
				(1 << 8) |
				/* toggle pic 0 and 1,use pic1 first */
				(0x01));
				/* loop pattern */
			} else {
				VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_PSEL +
				cur_dev->viu_off, (2 << 26) |
				/* two pic mode */
				(2 << 24) |
				/* use own last line */
				(2 << 8) |
				/* toggle pic 0 and 1, use pic0 first */
				(0x01));
				/* loop pattern */
				VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_PSEL +
				cur_dev->viu_off, (2 << 26) |
				/* two pic mode */
				(2 << 24) |
				/* use own last line */
				(2 << 8) |
				/* toggle pic 0 and 1, use pic0 first */
				(0x01));
				/* loop pattern */
			}
		} else if (process_3d_type & MODE_3D_OUT_FA_MASK) {
			/*FA LR/TB output , do nothing*/
		} else {
			if (frame_par->vpp_2pic_mode & VPP_SELECT_PIC1) {
				VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_PSEL +
				cur_dev->viu_off, 0);
				VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_PSEL +
				cur_dev->viu_off, 0);
				if (!vf_with_el) {
					VSYNC_WR_MPEG_REG(VD2_IF0_LUMA_PSEL +
					cur_dev->viu_off, 0);
					VSYNC_WR_MPEG_REG(VD2_IF0_CHROMA_PSEL +
					cur_dev->viu_off, 0);
				}
			} else {
				VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_PSEL +
				cur_dev->viu_off, 0);
				VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_PSEL +
				cur_dev->viu_off, 0);
				if (!vf_with_el) {
					VSYNC_WR_MPEG_REG(
						VD2_IF0_LUMA_PSEL +
						cur_dev->viu_off, 0);
					VSYNC_WR_MPEG_REG(
						VD2_IF0_CHROMA_PSEL +
						cur_dev->viu_off, 0);
				}
			}
		}
	}
}

static void vd2_set_dcu(struct vpp_frame_par_s *frame_par, struct vframe_s *vf)
{
	u32 r;
	u32 vphase, vini_phase;
	u32 pat, loop;
	static const u32 vpat[MAX_VSKIP_COUNT + 1] = {
		0, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
	u32 u, v;
	u32 type = vf->type, bit_mode = 0;

	pr_debug("set dcu for vd2 %p, type:0x%x\n", vf, type);
	last_el_w = (vf->type
		& VIDTYPE_COMPRESS) ?
		vf->compWidth :
		vf->width;
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		if (type & VIDTYPE_COMPRESS) {
			r = (3 << 24) |
			    (vpp_hold_line << 16) |
			    ((legacy_vpp ? 1 : 2) << 14) | /* burst1 */
			    (vf->bitdepth & BITDEPTH_MASK);

			if (frame_par->hscale_skip_count)
				r |= 0x33;
			if (frame_par->vscale_skip_count)
				r |= 0xcc;

#ifdef TV_REVERSE
			if (reverse)
				r |= (1<<26)|(1<<27);
#endif
			if (vf->bitdepth & BITDEPTH_SAVING_MODE)
				r |= (1<<28); /* mem_saving_mode */
			if (type & VIDTYPE_SCATTER)
				r |= (1<<29);
			VSYNC_WR_MPEG_REG(VD2_AFBC_MODE, r);
			VSYNC_WR_MPEG_REG(VD2_AFBC_ENABLE, 0x1700);

			r = 0x100;
			/* need check the vf->type 444/422/420 */
			/* current use 420 as default for tl1 */
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
				r |= (2 << 12);
			VSYNC_WR_MPEG_REG(VD2_AFBC_CONV_CTRL, r);

			u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
			v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
			VSYNC_WR_MPEG_REG(VD2_AFBC_DEC_DEF_COLOR,
				0x3FF00000 | /*Y,bit20+*/
				0x80 << (u + 10) |
				0x80 << v);
			/* chroma formatter */
			/* TODO: afbc setting only cover 420 for now */
#ifdef TV_REVERSE
			if (reverse) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
				if (is_meson_txlx_package_962X()
				&& !is_dolby_vision_stb_mode()
				&& is_dolby_vision_on()) {
					VSYNC_WR_MPEG_REG(
						VD2_AFBC_VD_CFMT_CTRL,
						HFORMATTER_REPEAT |
						HFORMATTER_YC_RATIO_2_1 |
						HFORMATTER_EN |
						VFORMATTER_ALWAYS_RPT |
						(0 << VFORMATTER_INIPHASE_BIT) |
						(0x8 << VFORMATTER_PHASE_BIT) |
						VFORMATTER_EN);
				} else
#endif
					VSYNC_WR_MPEG_REG(VD2_AFBC_VD_CFMT_CTRL,
					(is_dolby_vision_on() ?
						HFORMATTER_REPEAT :
						HFORMATTER_RRT_PIXEL0) |
						HFORMATTER_YC_RATIO_2_1 |
						HFORMATTER_EN |
						VFORMATTER_RPTLINE0_EN |
					(is_dolby_vision_on() ?
					(0xc << VFORMATTER_INIPHASE_BIT) : 0) |
						(0x8 << VFORMATTER_PHASE_BIT) |
						VFORMATTER_EN);
			} else {
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
				if (is_meson_txlx_package_962X()
				&& !is_dolby_vision_stb_mode()
				&& is_dolby_vision_on()) {
					VSYNC_WR_MPEG_REG(
						VD2_AFBC_VD_CFMT_CTRL,
						HFORMATTER_REPEAT |
						HFORMATTER_YC_RATIO_2_1 |
						HFORMATTER_EN |
						VFORMATTER_ALWAYS_RPT |
						(0 << VFORMATTER_INIPHASE_BIT) |
						(0x8 << VFORMATTER_PHASE_BIT) |
						VFORMATTER_EN);
				} else
#endif
					VSYNC_WR_MPEG_REG(VD2_AFBC_VD_CFMT_CTRL,
					(is_dolby_vision_on() ?
					HFORMATTER_REPEAT :
					HFORMATTER_RRT_PIXEL0) |
					HFORMATTER_YC_RATIO_2_1 |
					HFORMATTER_EN |
					VFORMATTER_RPTLINE0_EN |
					(is_dolby_vision_on() ?
					(0xc << VFORMATTER_INIPHASE_BIT) : 0) |
					(0x8 << VFORMATTER_PHASE_BIT) |
					VFORMATTER_EN);
#ifdef TV_REVERSE
			}
#endif
			vd2_path_select(true);
			VSYNC_WR_MPEG_REG(VD2_IF0_GEN_REG +
					 cur_dev->viu_off, 0);
			return;
		} else {
			if ((vf->bitdepth & BITDEPTH_Y10) &&
			(!frame_par->nocomp)) {
				if (vf->type & VIDTYPE_VIU_444) {
					bit_mode = 2;
				} else {
					if (vf->bitdepth & FULL_PACK_422_MODE)
						bit_mode = 3;
					else
						bit_mode = 1;
				}
			} else {
				bit_mode = 0;
			}
			vd2_path_select(false);
			if (!legacy_vpp) {
				VSYNC_WR_MPEG_REG_BITS(
					G12_VD2_IF0_GEN_REG3,
					(bit_mode & 0x3), 8, 2);
			} else {
				VSYNC_WR_MPEG_REG_BITS(
					VD2_IF0_GEN_REG3 + cur_dev->viu_off,
					(bit_mode&0x3), 8, 2);
			}
			if (!(VSYNC_RD_MPEG_REG(VIU_MISC_CTRL1) & 0x1))
				VSYNC_WR_MPEG_REG(VD2_AFBC_ENABLE, 0);
			if (type & VIDTYPE_VIU_NV21)
				VSYNC_WR_MPEG_REG_BITS(
					VD2_IF0_GEN_REG2 +
					cur_dev->viu_off, 1, 0, 1);
			else
				VSYNC_WR_MPEG_REG_BITS(
					VD2_IF0_GEN_REG2 +
					cur_dev->viu_off, 0, 0, 1);
		}
	}

	r = (3 << VDIF_URGENT_BIT) |
	    (vpp_hold_line << VDIF_HOLD_LINES_BIT) |
	    VDIF_FORMAT_SPLIT |
	    VDIF_CHRO_RPT_LAST | VDIF_ENABLE;
	/*  | VDIF_RESET_ON_GO_FIELD;*/
	if (debug_flag & DEBUG_FLAG_GOFIELD_MANUL)
		r |= 1<<7; /*for manul triggle gofiled.*/

	if ((type & VIDTYPE_VIU_SINGLE_PLANE) == 0)
		r |= VDIF_SEPARATE_EN;
	else {
		if (type & VIDTYPE_VIU_422)
			r |= VDIF_FORMAT_422;
		else {
			r |= VDIF_FORMAT_RGB888_YUV444 |
			    VDIF_DEMUX_MODE_RGB_444;
		}
	}

	if (frame_par->hscale_skip_count)
		r |= VDIF_CHROMA_HZ_AVG | VDIF_LUMA_HZ_AVG;

	VSYNC_WR_MPEG_REG(VD2_IF0_GEN_REG + cur_dev->viu_off, r);

#ifdef TV_REVERSE
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		if (reverse) {
			VSYNC_WR_MPEG_REG_BITS((VD2_IF0_GEN_REG2 +
				cur_dev->viu_off), 0xf, 2, 4);
		} else {
			VSYNC_WR_MPEG_REG_BITS((VD2_IF0_GEN_REG2 +
				cur_dev->viu_off), 0, 2, 4);
		}
	}
#endif

	/* chroma formatter */
	if (type & VIDTYPE_VIU_444) {
		VSYNC_WR_MPEG_REG(VIU_VD2_FMT_CTRL + cur_dev->viu_off,
				  HFORMATTER_YC_RATIO_1_1);
	} else if (type & VIDTYPE_VIU_FIELD) {
		vini_phase = 0xc << VFORMATTER_INIPHASE_BIT;
		vphase =
		    ((type & VIDTYPE_VIU_422) ? 0x10 : 0x08) <<
		    VFORMATTER_PHASE_BIT;
		if (is_meson_txlx_package_962X()
		&& !is_dolby_vision_stb_mode()
		&& is_dolby_vision_on()) {
			VSYNC_WR_MPEG_REG(
				VIU_VD2_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_REPEAT |
				HFORMATTER_YC_RATIO_2_1 |
				HFORMATTER_EN |
				VFORMATTER_ALWAYS_RPT |
				(0 << VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) |
				VFORMATTER_EN);
			pr_debug("\tvd2 set fmt(dovi tv)\n");
		} else if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu() ||
		is_meson_txlx_cpu()) {
			if ((vf->width >= 3840) &&
				(vf->height >= 2160) &&
				(type & VIDTYPE_VIU_422)) {
				VSYNC_WR_MPEG_REG(
				VIU_VD2_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_RRT_PIXEL0 |
				HFORMATTER_YC_RATIO_2_1 |
				HFORMATTER_EN | VFORMATTER_RPTLINE0_EN |
				vini_phase | vphase);
			} else {
				VSYNC_WR_MPEG_REG(
				VIU_VD2_FMT_CTRL + cur_dev->viu_off,
				(is_dolby_vision_on() ?
				HFORMATTER_REPEAT : 0) |
				HFORMATTER_YC_RATIO_2_1 | HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN |
				vini_phase | vphase |
				VFORMATTER_EN);
			}
		} else {
			VSYNC_WR_MPEG_REG(
				VIU_VD2_FMT_CTRL + cur_dev->viu_off,
				(is_dolby_vision_on() ?
				HFORMATTER_REPEAT : 0) |
				HFORMATTER_YC_RATIO_2_1 | HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN | vini_phase | vphase |
				VFORMATTER_EN);
		}
	} else if (type & VIDTYPE_MVC) {
		VSYNC_WR_MPEG_REG(VIU_VD1_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_YC_RATIO_2_1 |
				HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN |
				(0xe << VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) | VFORMATTER_EN);
		VSYNC_WR_MPEG_REG(VIU_VD2_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_YC_RATIO_2_1 | HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN | (0xa <<
				VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) | VFORMATTER_EN);
	} else if ((type & VIDTYPE_INTERLACE)
		   &&
		   (((type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP))) {
		VSYNC_WR_MPEG_REG(VIU_VD2_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_YC_RATIO_2_1 |
				HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN |
				(0xe << VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) |
				VFORMATTER_EN);
	} else {
		if (is_meson_txlx_package_962X()
		&& !is_dolby_vision_stb_mode()
		&& is_dolby_vision_on()) {
			VSYNC_WR_MPEG_REG(
				VIU_VD2_FMT_CTRL + cur_dev->viu_off,
				HFORMATTER_REPEAT |
				HFORMATTER_YC_RATIO_2_1 |
				HFORMATTER_EN |
				VFORMATTER_ALWAYS_RPT |
				(0 << VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) |
				VFORMATTER_EN);
			pr_info("\tvd2 set fmt(dovi tv)\n");
		} else {
			VSYNC_WR_MPEG_REG(
				VIU_VD2_FMT_CTRL + cur_dev->viu_off,
				(is_dolby_vision_on() ?
				HFORMATTER_REPEAT : 0) |
				HFORMATTER_YC_RATIO_2_1 |
				HFORMATTER_EN |
				VFORMATTER_RPTLINE0_EN |
				(0xc << VFORMATTER_INIPHASE_BIT) |
				(((type & VIDTYPE_VIU_422) ? 0x10 : 0x08)
				<< VFORMATTER_PHASE_BIT) | VFORMATTER_EN);
			pr_info("\tvd2 set fmt(dovi:%d)\n",
				/*is_dolby_vision_on()*/0);
		}
	}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if ((is_meson_txlx_cpu()
		|| is_meson_g12a_cpu()
		|| is_meson_g12b_cpu())
		&& is_dolby_vision_on()
		&& is_dolby_vision_stb_mode()
		&& (vf->source_type ==
		VFRAME_SOURCE_TYPE_OTHERS))
		VSYNC_WR_MPEG_REG_BITS(
			VIU_VD2_FMT_CTRL + cur_dev->viu_off,
			1, 29, 1);
#endif
	/* LOOP/SKIP pattern */
	pat = vpat[frame_par->vscale_skip_count];

	if (type & VIDTYPE_VIU_FIELD) {
		loop = 0;

		if (type & VIDTYPE_INTERLACE)
			pat = vpat[frame_par->vscale_skip_count >> 1];
	} else if (type & VIDTYPE_MVC) {
		loop = 0x11;
		pat = 0x80;
	} else if ((type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) {
		loop = 0x11;
		pat <<= 4;
	} else
		loop = 0;

	VSYNC_WR_MPEG_REG(
		VD2_IF0_RPT_LOOP + cur_dev->viu_off,
		(loop << VDIF_CHROMA_LOOP1_BIT) |
		(loop << VDIF_LUMA_LOOP1_BIT) |
		(loop << VDIF_CHROMA_LOOP0_BIT) |
		(loop << VDIF_LUMA_LOOP0_BIT));

	if (type & VIDTYPE_MVC)
		pat = 0x88;

	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA0_RPT_PAT + cur_dev->viu_off, pat);
	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA0_RPT_PAT + cur_dev->viu_off, pat);
	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA1_RPT_PAT + cur_dev->viu_off, pat);
	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA1_RPT_PAT + cur_dev->viu_off, pat);

	if (platform_type == 0) {
		/* picture 0/1 control */
		if (((type & VIDTYPE_INTERLACE) == 0) &&
			((type & VIDTYPE_VIU_FIELD) == 0) &&
			((type & VIDTYPE_MVC) == 0)) {
			/* progressive frame in two pictures */

		} else {
			VSYNC_WR_MPEG_REG(
				VD2_IF0_LUMA_PSEL + cur_dev->viu_off, 0);
			VSYNC_WR_MPEG_REG(
				VD2_IF0_CHROMA_PSEL + cur_dev->viu_off, 0);
		}
	} else {
		/* picture 0/1 control */
		if ((((type & VIDTYPE_INTERLACE) == 0) &&
			 ((type & VIDTYPE_VIU_FIELD) == 0) &&
			 ((type & VIDTYPE_MVC) == 0)) ||
			(next_frame_par->vpp_2pic_mode & 0x3)) {
			/* progressive frame in two pictures */

		} else {
			if (next_frame_par->vpp_2pic_mode & VPP_SELECT_PIC1) {
				VSYNC_WR_MPEG_REG(VD2_IF0_LUMA_PSEL +
				cur_dev->viu_off, 0);
				VSYNC_WR_MPEG_REG(VD2_IF0_CHROMA_PSEL +
				cur_dev->viu_off, 0);
			} else {
				VSYNC_WR_MPEG_REG(
					VD2_IF0_LUMA_PSEL +
					cur_dev->viu_off, 0);
				VSYNC_WR_MPEG_REG(
					VD2_IF0_CHROMA_PSEL +
					cur_dev->viu_off, 0);
			}
		}
	}
}

#if 1				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */

static int detect_vout_type(void)
{
	int vout_type = VOUT_TYPE_PROG;
	if ((vinfo) && (vinfo->field_height != vinfo->height)) {
		if ((vinfo->height == 576) || (vinfo->height == 480))
			vout_type = (READ_VCBUS_REG(ENCI_INFO_READ) &
				(1 << 29)) ?
				VOUT_TYPE_BOT_FIELD : VOUT_TYPE_TOP_FIELD;
		else if (vinfo->height == 1080)
			vout_type = (((READ_VCBUS_REG(ENCP_INFO_READ) >> 16) &
				0x1fff) < 562) ?
				VOUT_TYPE_TOP_FIELD : VOUT_TYPE_BOT_FIELD;

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		if (is_vsync_rdma_enable()) {
			if (vout_type == VOUT_TYPE_TOP_FIELD)
				vout_type = VOUT_TYPE_BOT_FIELD;
			else if (vout_type == VOUT_TYPE_BOT_FIELD)
				vout_type = VOUT_TYPE_TOP_FIELD;
		}
#endif
	}
	return vout_type;
}

#else
static int detect_vout_type(void)
{
#if defined(CONFIG_AM_TCON_OUTPUT)
	return VOUT_TYPE_PROG;
#else
	int vout_type;
	int encp_enable = READ_VCBUS_REG(ENCP_VIDEO_EN) & 1;

	if (encp_enable) {
		if (READ_VCBUS_REG(ENCP_VIDEO_MODE) & (1 << 12)) {
			/* 1080I */
			if (READ_VCBUS_REG(VENC_ENCP_LINE) < 562)
				vout_type = VOUT_TYPE_TOP_FIELD;

			else
				vout_type = VOUT_TYPE_BOT_FIELD;

		} else
			vout_type = VOUT_TYPE_PROG;

	} else {
		vout_type = (READ_VCBUS_REG(VENC_STATA) & 1) ?
		    VOUT_TYPE_BOT_FIELD : VOUT_TYPE_TOP_FIELD;
	}

	return vout_type;
#endif
}
#endif

#ifdef INTERLACE_FIELD_MATCH_PROCESS
static inline bool interlace_field_type_need_match(int vout_type,
						   struct vframe_s *vf)
{
	if (DUR2PTS(vf->duration) != vsync_pts_inc)
		return false;

	if ((vout_type == VOUT_TYPE_TOP_FIELD) &&
	    ((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_BOTTOM))
		return true;
	else if ((vout_type == VOUT_TYPE_BOT_FIELD) &&
		 ((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP))
		return true;

	return false;
}
#endif

static int calc_hold_line(void)
{
	if ((READ_VCBUS_REG(ENCI_VIDEO_EN) & 1) == 0)
		return READ_VCBUS_REG(ENCP_VIDEO_VAVON_BLINE) >> 1;
	else
		return READ_VCBUS_REG(ENCP_VFIFO2VD_LINE_TOP_START) >> 1;
}

/* add a new function to check if current display frame has been*/
/*displayed for its duration */
static inline bool duration_expire(struct vframe_s *cur_vf,
				   struct vframe_s *next_vf, u32 dur)
{
	u32 pts;
	s32 dur_disp;
	static s32 rpt_tab_idx;
	static const u32 rpt_tab[4] = { 0x100, 0x100, 0x300, 0x300 };

	/* do not switch to new frames in none-normal speed */
	if (vsync_slow_factor > 1000)
		return false;

	if ((cur_vf == NULL) || (cur_dispbuf == &vf_local))
		return true;

	pts = next_vf->pts;
	if (pts == 0)
		dur_disp = DUR2PTS(cur_vf->duration);
	else
		dur_disp = pts - timestamp_vpts_get();

	if ((dur << 8) >= (dur_disp * rpt_tab[rpt_tab_idx & 3])) {
		rpt_tab_idx = (rpt_tab_idx + 1) & 3;
		return true;
	} else
		return false;
}

#define VPTS_RESET_THRO

#ifdef PTS_LOGGING
static inline void vpts_perform_pulldown(struct vframe_s *next_vf,
					bool *expired)
{
	int pattern_range, expected_curr_interval;
	int expected_prev_interval;

	/* Dont do anything if we have invalid data */
	if (!next_vf || !next_vf->pts || !next_vf->next_vf_pts_valid)
		return;

	switch (pts_pattern_detected) {
	case PTS_32_PATTERN:
		pattern_range = PTS_32_PATTERN_DETECT_RANGE;
		switch (pre_pts_trace) {
		case 3:
			expected_prev_interval = 3;
			expected_curr_interval = 2;
			break;
		case 2:
			expected_prev_interval = 2;
			expected_curr_interval = 3;
			break;
		default:
			return;
		}
		break;
	case PTS_22_PATTERN:
		if (pre_pts_trace != 2)
			return;
		pattern_range =  PTS_22_PATTERN_DETECT_RANGE;
		expected_prev_interval = 2;
		expected_curr_interval = 2;
		break;
	case PTS_41_PATTERN:
		/* TODO */
	default:
		return;
	}

	/* We do nothing if  we dont have enough data*/
	if (pts_pattern[pts_pattern_detected] != pattern_range)
		return;

	if (*expired) {
		if (pts_trace < expected_curr_interval) {
			/* 232323...223 -> 232323...232 */
			/* check if next frame will toggle after 3 vsyncs */
			int nextPts = timestamp_pcrscr_get() + vsync_pts_align;

			if (((int)(nextPts + expected_prev_interval *
				vsync_pts_inc - next_vf->next_vf_pts) < 0) &&
				((int)(nextPts + expected_curr_interval *
				vsync_pts_inc - next_vf->next_vf_pts) >= 0)) {
				*expired = false;
				if (pts_log_enable[PTS_32_PATTERN])
					pr_info("hold frame for pattern: %d",
						pts_pattern_detected);
			}
		}
	} else {
		if (pts_trace == expected_curr_interval) {
			/* 232323...332 -> 232323...323 */
			/* check if this frame will expire next vsyncs and */
			/* next frame will expire after 2 vsyncs */
			int nextPts = timestamp_pcrscr_get() + vsync_pts_align;

			if (((int)(nextPts + vsync_pts_inc - next_vf->pts)
				>= 0) &&
			    ((int)(nextPts + vsync_pts_inc -
				next_vf->next_vf_pts) < 0) &&
			    ((int)(nextPts + expected_curr_interval *
				vsync_pts_inc - next_vf->next_vf_pts) >= 0)) {
				*expired = true;
				if (pts_log_enable[PTS_32_PATTERN])
					pr_info("pull frame for pattern: %d",
						pts_pattern_detected);
			}
		}
	}
}
#endif

static inline bool vpts_expire(struct vframe_s *cur_vf,
			       struct vframe_s *next_vf,
			       int toggled_cnt)
{
	u32 pts;
#ifdef VIDEO_PTS_CHASE
	u32 vid_pts, scr_pts;
#endif
	u32 systime;
	u32 adjust_pts, org_vpts;
	bool expired;

	if (next_vf == NULL)
		return false;

	if (videopeek) {
		videopeek = false;
		pr_info("video peek toogle the first frame\n");
		return true;
	}

	if (debug_flag & DEBUG_FLAG_TOGGLE_FRAME_PER_VSYNC)
		return true;
	if (/*(cur_vf == NULL) || (cur_dispbuf == &vf_local) ||*/ debugflags &
	    DEBUG_FLAG_FFPLAY)
		return true;

	if ((freerun_mode == FREERUN_NODUR) || hdmi_in_onvideo)
		return true;
	/*freerun for game mode*/
	if (next_vf->flag & VFRAME_FLAG_GAME_MODE)
		return true;

	if (step_enable) {
		if (step_flag)
			return false;
		if (!step_flag) {
			step_flag = 1;
			return true;
		}
	}

	if ((trickmode_i == 1) || ((trickmode_fffb == 1))) {
		if (((atomic_read(&trickmode_framedone) == 0)
		     || (trickmode_i == 1)) && (!to_notify_trick_wait)
		    && (trickmode_duration_count <= 0)) {
#if 0
			if (cur_vf)
				pts = timestamp_vpts_get() +
				trickmode_duration;
			else
				return true;
#else
			return true;
#endif
		} else
			return false;
	}
	if (omx_secret_mode && (!omx_run || !omx_drop_done))
		return false;

	if (next_vf->duration == 0)

		return true;

	systime = timestamp_pcrscr_get();
	pts = next_vf->pts;

	if (((pts == 0) && (cur_dispbuf != &vf_local))
	    || (freerun_mode == FREERUN_DUR)) {
		pts =
		    timestamp_vpts_get() +
		    (cur_vf ? DUR2PTS(cur_vf->duration) : 0);
	}
	/* check video PTS discontinuity */
	else if ((enable_video_discontinue_report) &&
		 (first_frame_toggled) &&
		 (abs(systime - pts) > tsync_vpts_discontinuity_margin()) &&
		 ((next_vf->flag & VFRAME_FLAG_NO_DISCONTINUE) == 0)) {
		/*
		 * if paused ignore discontinue
		 */
		if (!timestamp_pcrscr_enable_state()) {
			/*pr_info("video pts discontinue,
			 * but pcrscr is disabled,
			 * return false\n");
			 */
			return false;
		}
		pts =
		    timestamp_vpts_get() +
		    (cur_vf ? DUR2PTS(cur_vf->duration) : 0);
		/* pr_info("system=0x%x vpts=0x%x\n", systime,*/
		/*timestamp_vpts_get()); */
		if ((int)(systime - pts) >= 0) {
			if (next_vf->pts != 0)
				tsync_avevent_locked(VIDEO_TSTAMP_DISCONTINUITY,
						     next_vf->pts);
			else if (next_vf->pts == 0 &&
				(tsync_get_mode() != TSYNC_MODE_PCRMASTER))
				tsync_avevent_locked(VIDEO_TSTAMP_DISCONTINUITY,
						     pts);

			/* pr_info("discontinue,
			 *   systime=0x%x vpts=0x%x next_vf->pts = 0x%x\n",
			 *	systime,
			 *	pts,
			 *	next_vf->pts);
			 */

			/* pts==0 is a keep frame maybe. */
			if (systime > next_vf->pts || next_vf->pts == 0)
				return true;
			if (omx_secret_mode == true)
				return true;

			return false;
		} else if (omx_secret_mode == true)
			return true;
	} else if (omx_run
			&& omx_secret_mode
			&& (omx_pts + omx_pts_interval_upper < next_vf->pts)
			&& (abs(omx_pts_set_index - next_vf->omx_index) <= 16)
			&& (omx_pts_set_index >= next_vf->omx_index)) {
		pr_info("omx, omx_pts=%d omx_pts_set_index=%d pts=%d omx_index=%d\n",
					omx_pts,
					omx_pts_set_index,
					next_vf->pts,
					next_vf->omx_index);
		return true;
	}
#if 1
	if (vsync_pts_inc_upint && (!freerun_mode)) {
		struct vframe_states frame_states;
		u32 delayed_ms, t1, t2;

		delayed_ms =
		    calculation_stream_delayed_ms(PTS_TYPE_VIDEO, &t1, &t2);
		if (video_vf_get_states(&frame_states) == 0) {
			u32 pcr = timestamp_pcrscr_get();
			u32 vpts = timestamp_vpts_get();
			u32 diff = pcr - vpts;

			if (delayed_ms > 200) {
				vsync_freerun++;
				if (pcr < next_vf->pts
				    || pcr < vpts + next_vf->duration) {
					if (next_vf->pts > 0) {
						timestamp_pcrscr_set
						    (next_vf->pts);
					} else {
						timestamp_pcrscr_set(vpts +
							next_vf->duration);
					}
				}
				return true;
			} else if ((frame_states.buf_avail_num >= 3)
				   && diff < vsync_pts_inc << 2) {
				vsync_pts_inc_adj =
				    vsync_pts_inc + (vsync_pts_inc >> 2);
				vsync_pts_125++;
			} else if ((frame_states.buf_avail_num >= 2
				    && diff < vsync_pts_inc << 1)) {
				vsync_pts_inc_adj =
				    vsync_pts_inc + (vsync_pts_inc >> 3);
				vsync_pts_112++;
			} else if (frame_states.buf_avail_num >= 1
				   && diff < vsync_pts_inc - 20) {
				vsync_pts_inc_adj = vsync_pts_inc + 10;
				vsync_pts_101++;
			} else {
				vsync_pts_inc_adj = 0;
				vsync_pts_100++;
			}
		}
	}
#endif

#ifdef VIDEO_PTS_CHASE
	vid_pts = timestamp_vpts_get();
	scr_pts = timestamp_pcrscr_get();
	vid_pts += vsync_pts_inc;

	if (av_sync_flag) {
		if (vpts_chase) {
			if ((abs(vid_pts - scr_pts) < 6000)
			    || (abs(vid_pts - scr_pts) > 90000)) {
				vpts_chase = 0;
				pr_info("leave vpts chase mode, diff:%d\n",
				       vid_pts - scr_pts);
			}
		} else if ((abs(vid_pts - scr_pts) > 9000)
			   && (abs(vid_pts - scr_pts) < 90000)) {
			vpts_chase = 1;
			if (vid_pts < scr_pts)
				vpts_chase_pts_diff = 50;
			else
				vpts_chase_pts_diff = -50;
			vpts_chase_counter =
			    ((int)(scr_pts - vid_pts)) / vpts_chase_pts_diff;
			pr_info("enter vpts chase mode, diff:%d\n",
			       vid_pts - scr_pts);
		} else if (abs(vid_pts - scr_pts) >= 90000) {
			pr_info("video pts discontinue, diff:%d\n",
			       vid_pts - scr_pts);
		}
	} else
		vpts_chase = 0;

	if (vpts_chase) {
		u32 curr_pts =
		    scr_pts - vpts_chase_pts_diff * vpts_chase_counter;

	/* pr_info("vchase pts %d, %d, %d, %d, %d\n",*/
	/*curr_pts, scr_pts, curr_pts-scr_pts, vid_pts, vpts_chase_counter); */
		return ((int)(curr_pts - pts)) >= 0;
	} else {
		int aud_start = (timestamp_apts_get() != -1);

	if (!av_sync_flag && aud_start && (abs(scr_pts - pts) < 9000)
	    && ((int)(scr_pts - pts) < 0)) {
		av_sync_flag = 1;
		pr_info("av sync ok\n");
	}
	return ((int)(scr_pts - pts)) >= 0;
	}
#else
	if (smooth_sync_enable) {
		org_vpts = timestamp_vpts_get();
		if ((abs(org_vpts + vsync_pts_inc - systime) <
			M_PTS_SMOOTH_MAX)
		    && (abs(org_vpts + vsync_pts_inc - systime) >
			M_PTS_SMOOTH_MIN)) {

			if (!video_frame_repeat_count) {
				vpts_ref = org_vpts;
				video_frame_repeat_count++;
			}

			if ((int)(org_vpts + vsync_pts_inc - systime) > 0) {
				adjust_pts =
				    vpts_ref + (vsync_pts_inc -
						M_PTS_SMOOTH_ADJUST) *
				    video_frame_repeat_count;
			} else {
				adjust_pts =
				    vpts_ref + (vsync_pts_inc +
						M_PTS_SMOOTH_ADJUST) *
				    video_frame_repeat_count;
			}

			return (int)(adjust_pts - pts) >= 0;
		}

		if (video_frame_repeat_count) {
			vpts_ref = 0;
			video_frame_repeat_count = 0;
		}
	}

	expired = (int)(timestamp_pcrscr_get() + vsync_pts_align - pts) >= 0;

#ifdef PTS_THROTTLE
	if (expired && next_vf && next_vf->next_vf_pts_valid &&
		(vsync_slow_factor == 1) &&
		next_vf->next_vf_pts &&
		(toggled_cnt > 0) &&
		((int)(timestamp_pcrscr_get() + vsync_pts_inc +
		vsync_pts_align - next_vf->next_vf_pts) < 0)) {
		expired = false;
	} else if (!expired && next_vf && next_vf->next_vf_pts_valid &&
		(vsync_slow_factor == 1) &&
		next_vf->next_vf_pts &&
		(toggled_cnt == 0) &&
		((int)(timestamp_pcrscr_get() + vsync_pts_inc +
		vsync_pts_align - next_vf->next_vf_pts) >= 0)) {
		expired = true;
	}
#endif

#ifdef PTS_LOGGING
	if (pts_enforce_pulldown) {
		/* Perform Pulldown if needed*/
		vpts_perform_pulldown(next_vf, &expired);
	}
#endif
	return expired;
#endif
}

static void vsync_notify(void)
{
	if (video_notify_flag & VIDEO_NOTIFY_TRICK_WAIT) {
		wake_up_interruptible(&amvideo_trick_wait);
		video_notify_flag &= ~VIDEO_NOTIFY_TRICK_WAIT;
	}
	if (video_notify_flag & VIDEO_NOTIFY_FRAME_WAIT) {
		video_notify_flag &= ~VIDEO_NOTIFY_FRAME_WAIT;
		vf_notify_provider(RECEIVER_NAME,
				   VFRAME_EVENT_RECEIVER_FRAME_WAIT, NULL);
	}
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
	if (video_notify_flag & VIDEO_NOTIFY_POS_CHANGED) {
		video_notify_flag &= ~VIDEO_NOTIFY_POS_CHANGED;
		vf_notify_provider(RECEIVER_NAME,
				   VFRAME_EVENT_RECEIVER_POS_CHANGED, NULL);
	}
#endif
	if (video_notify_flag &
	    (VIDEO_NOTIFY_PROVIDER_GET | VIDEO_NOTIFY_PROVIDER_PUT)) {
		int event = 0;

		if (video_notify_flag & VIDEO_NOTIFY_PROVIDER_GET)
			event |= VFRAME_EVENT_RECEIVER_GET;
		if (video_notify_flag & VIDEO_NOTIFY_PROVIDER_PUT)
			event |= VFRAME_EVENT_RECEIVER_PUT;

		vf_notify_provider(RECEIVER_NAME, event, NULL);

		video_notify_flag &=
		    ~(VIDEO_NOTIFY_PROVIDER_GET | VIDEO_NOTIFY_PROVIDER_PUT);
	}
#ifdef CONFIG_CLK81_DFS
	check_and_set_clk81();
#endif

#ifdef CONFIG_GAMMA_PROC
	gamma_adjust();
#endif

}

#ifdef FIQ_VSYNC
static irqreturn_t vsync_bridge_isr(int irq, void *dev_id)
{
	vsync_notify();

	return IRQ_HANDLED;
}
#endif

int get_vsync_count(unsigned char reset)
{
	if (reset)
		vsync_count = 0;
	return vsync_count;
}
EXPORT_SYMBOL(get_vsync_count);

int get_vsync_pts_inc_mode(void)
{
	return vsync_pts_inc_upint;
}
EXPORT_SYMBOL(get_vsync_pts_inc_mode);

void set_vsync_pts_inc_mode(int inc)
{
	vsync_pts_inc_upint = inc;
}
EXPORT_SYMBOL(set_vsync_pts_inc_mode);

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
void vsync_rdma_process(void)
{
	vsync_rdma_config();
}
#endif

/* #ifdef CONFIG_SUPPORT_VIDEO_ON_VPP2 */
static enum vmode_e old_vmode = VMODE_MAX;
/* #endif */
static enum vmode_e new_vmode = VMODE_MAX;
static inline bool video_vf_disp_mode_check(struct vframe_s *vf)
{
	struct provider_disp_mode_req_s req;
	int ret = -1;
	req.vf = vf;
	req.disp_mode = 0;
	req.req_mode = 1;

	if (is_dolby_vision_enable()) {
		ret = vf_notify_provider_by_name("dv_vdin",
			VFRAME_EVENT_RECEIVER_DISP_MODE, (void *)&req);
		if (ret == -1)
			vf_notify_provider_by_name("vdin0",
				VFRAME_EVENT_RECEIVER_DISP_MODE, (void *)&req);
	} else
		vf_notify_provider_by_name("vdin0",
			VFRAME_EVENT_RECEIVER_DISP_MODE, (void *)&req);
	if (req.disp_mode == VFRAME_DISP_MODE_OK)
		return false;
	/*whether need to check pts??*/
	video_vf_put(vf);
	return true;
}
static enum vframe_disp_mode_e video_vf_disp_mode_get(struct vframe_s *vf)
{
	struct provider_disp_mode_req_s req;
	int ret = -1;
	req.vf = vf;
	req.disp_mode = 0;
	req.req_mode = 0;

	if (is_dolby_vision_enable()) {
		ret = vf_notify_provider_by_name("dv_vdin",
			VFRAME_EVENT_RECEIVER_DISP_MODE, (void *)&req);
		if (ret == -1)
			vf_notify_provider_by_name("vdin0",
				VFRAME_EVENT_RECEIVER_DISP_MODE, (void *)&req);
	} else
		vf_notify_provider_by_name("vdin0",
			VFRAME_EVENT_RECEIVER_DISP_MODE, (void *)&req);
	return req.disp_mode;
}
static inline bool video_vf_dirty_put(struct vframe_s *vf)
{
	if (!vf->frame_dirty)
		return false;
	if (cur_dispbuf != vf) {
		if (vf->source_type != VFRAME_SOURCE_TYPE_OSD) {
			if (vf->pts != 0) {
				amlog_mask(LOG_MASK_TIMESTAMP,
				"vpts to vf->pts:0x%x,scr:0x%x,abs_scr: 0x%x\n",
				vf->pts, timestamp_pcrscr_get(),
				READ_MPEG_REG(SCR_HIU));
				timestamp_vpts_set(vf->pts);
			} else if (cur_dispbuf) {
				amlog_mask(LOG_MASK_TIMESTAMP,
				     "vpts inc:0x%x,scr: 0x%x, abs_scr: 0x%x\n",
				     timestamp_vpts_get() +
				     DUR2PTS(cur_dispbuf->duration),
				     timestamp_pcrscr_get(),
				     READ_MPEG_REG(SCR_HIU));
				timestamp_vpts_inc(
						DUR2PTS(cur_dispbuf->duration));

				vpts_remainder +=
					DUR2PTS_RM(cur_dispbuf->duration);
				if (vpts_remainder >= 0xf) {
					vpts_remainder -= 0xf;
					timestamp_vpts_inc(-1);
				}
			}
		}
	}
	video_vf_put(vf);
	return true;

}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
struct vframe_s *dolby_vision_toggle_frame(struct vframe_s *vf)
{
	struct vframe_s *toggle_vf = NULL;
	int width_bl, width_el;
	int height_bl, height_el;
	int ret = dolby_vision_update_metadata(vf);

	cur_dispbuf2 = dolby_vision_vf_peek_el(vf);
	if (cur_dispbuf2) {
		if (cur_dispbuf2->type & VIDTYPE_COMPRESS) {
			VSYNC_WR_MPEG_REG(VD2_AFBC_HEAD_BADDR,
				cur_dispbuf2->compHeadAddr>>4);
			VSYNC_WR_MPEG_REG(VD2_AFBC_BODY_BADDR,
				cur_dispbuf2->compBodyAddr>>4);
		} else {
			vframe_canvas_set(&cur_dispbuf2->canvas0_config[0],
				cur_dispbuf2->plane_num,
				&disp_canvas_index[rdma_canvas_id][3]);
			VSYNC_WR_MPEG_REG(VD2_IF0_CANVAS0 + cur_dev->viu_off,
				disp_canvas[rdma_canvas_id][1]);
			VSYNC_WR_MPEG_REG(VD2_IF0_CANVAS1 + cur_dev->viu_off,
			  disp_canvas[rdma_canvas_id][1]);
		}

		width_el = (cur_dispbuf2->type
			& VIDTYPE_COMPRESS) ?
			cur_dispbuf2->compWidth :
			cur_dispbuf2->width;
		if (!(cur_dispbuf2->type & VIDTYPE_VD2)) {
			width_bl = (vf->type
				& VIDTYPE_COMPRESS) ?
				vf->compWidth :
				vf->width;
			if (width_el >= width_bl)
				width_el = width_bl;
			else if (width_el != width_bl / 2)
				width_el = width_bl / 2;
		}
		ori2_start_x_lines = 0;
		ori2_end_x_lines =
			width_el - 1;

		height_el = (cur_dispbuf2->type
			& VIDTYPE_COMPRESS) ?
			cur_dispbuf2->compHeight :
			cur_dispbuf2->height;
		if (!(cur_dispbuf2->type & VIDTYPE_VD2)) {
			height_bl =	(vf->type
				& VIDTYPE_COMPRESS) ?
				vf->compHeight :
				vf->height;
			if (height_el >= height_bl)
				height_el = height_bl;
			else if (height_el != height_bl / 2)
				height_el = height_bl / 2;
		}
		ori2_start_y_lines = 0;
		ori2_end_y_lines =
			height_el - 1;
	}
	if (ret == 0) {
		/* setting generated for this frame */
		/* or DOVI in bypass mode */
		toggle_vf = vf;
		dolby_vision_set_toggle_flag(1);
	} else {
		/* fail generating setting for this frame */
		toggle_vf = NULL;
		dolby_vision_set_toggle_flag(0);
	}
	return toggle_vf;
}

static int dolby_vision_need_wait(void)
{
	struct vframe_s *vf;

	vf = video_vf_peek();
	if (!vf || (dolby_vision_wait_metadata(vf) == 1))
		return 1;
	return 0;
}
#endif
/* patch for 4k2k bandwidth issue, skiw mali and vpu mif */
static void dmc_adjust_for_mali_vpu(unsigned int width,
	unsigned int height, bool force_adjust)
{
	if (toggle_count == last_toggle_count)
		toggle_same_count++;
	else {
		last_toggle_count = toggle_count;
		toggle_same_count = 0;
	}
	/*avoid 3840x2160 crop*/
	if ((width >= 2000) && (height >= 1400) &&
		(((dmc_config_state != 1) && (toggle_same_count < 30))
		|| force_adjust)) {
		if (0) {/* if (is_dolby_vision_enable()) { */
			/* vpu dmc */
			WRITE_DMCREG(
				DMC_AM0_CHAN_CTRL,
				0x85f403f4);
			WRITE_DMCREG(
				DMC_AM1_CHAN_CTRL,
				0x85f403f4);
			WRITE_DMCREG(
				DMC_AM2_CHAN_CTRL,
				0x85f403f4);

			/* mali dmc */
			WRITE_DMCREG(
				DMC_AXI1_CHAN_CTRL,
				0xff10ff4);
			WRITE_DMCREG(
				DMC_AXI2_CHAN_CTRL,
				0xff10ff4);
			WRITE_DMCREG(
				DMC_AXI1_HOLD_CTRL,
				0x08040804);
			WRITE_DMCREG(
				DMC_AXI2_HOLD_CTRL,
				0x08040804);
		} else {
			/* vpu dmc */
			WRITE_DMCREG(
				DMC_AM1_CHAN_CTRL,
				0x43028);
			WRITE_DMCREG(
				DMC_AM1_HOLD_CTRL,
				0x18101818);
			WRITE_DMCREG(
				DMC_AM3_CHAN_CTRL,
				0x85f403f4);
			WRITE_DMCREG(
				DMC_AM4_CHAN_CTRL,
				0x85f403f4);
			/* mali dmc */
			WRITE_DMCREG(
				DMC_AXI1_HOLD_CTRL,
				0x10080804);
			WRITE_DMCREG(
				DMC_AXI2_HOLD_CTRL,
				0x10080804);
		}
		dmc_config_state = 1;
	} else if (((toggle_same_count >= 30) ||
		((width < 2000) && (height < 1400))) &&
		(dmc_config_state != 2)) {
		/* vpu dmc */
		WRITE_DMCREG(
			DMC_AM0_CHAN_CTRL,
			0x8FF003C4);
		WRITE_DMCREG(
			DMC_AM1_CHAN_CTRL,
			0x3028);
		WRITE_DMCREG(
			DMC_AM1_HOLD_CTRL,
			0x18101810);
		WRITE_DMCREG(
			DMC_AM2_CHAN_CTRL,
			0x8FF003C4);
		WRITE_DMCREG(
			DMC_AM2_HOLD_CTRL,
			0x3028);
		WRITE_DMCREG(
			DMC_AM3_CHAN_CTRL,
			0x85f003f4);
		WRITE_DMCREG(
			DMC_AM4_CHAN_CTRL,
			0x85f003f4);

		/* mali dmc */
		WRITE_DMCREG(
			DMC_AXI1_CHAN_CTRL,
			0x8FF00FF4);
		WRITE_DMCREG(
			DMC_AXI2_CHAN_CTRL,
			0x8FF00FF4);
		WRITE_DMCREG(
			DMC_AXI1_HOLD_CTRL,
			0x18101810);
		WRITE_DMCREG(
			DMC_AXI2_HOLD_CTRL,
			0x18101810);
		toggle_same_count = 30;
		dmc_config_state = 2;
	}
}

static bool is_sc_enable_before_pps(struct vpp_frame_par_s *par)
{
	bool ret = false;

	if (par) {
		if (par->supsc0_enable &&
			((par->supscl_path == CORE0_PPS_CORE1)
			|| (par->supscl_path == CORE0_BEFORE_PPS)))
			ret = true;
		else if (par->supsc1_enable &&
			(par->supscl_path == CORE1_BEFORE_PPS))
			ret = true;
		else if ((par->supsc0_enable || par->supsc1_enable)
			&& (par->supscl_path == CORE0_CORE1_PPS))
			ret = true;
	}
	return ret;
}

void correct_vd1_mif_size_for_DV(struct vpp_frame_par_s *par)
{
	u32 aligned_mask = 0xfffffffe;
	u32 old_len;
	if ((is_dolby_vision_on() == true)
		&& (par->VPP_line_in_length_ > 0)
		&& !is_sc_enable_before_pps(par)) {
		/* work around to skip the size check when sc enable */
		if (cur_dispbuf2) {
			/*
			 *if (cur_dispbuf2->type
			 *	& VIDTYPE_COMPRESS)
			 *	aligned_mask = 0xffffffc0;
			 *else
			 */
			aligned_mask = 0xfffffffc;
		}
#if 0 /* def TV_REVERSE */
		if (reverse) {
			par->VPP_line_in_length_
				&= 0xfffffffe;
			par->VPP_hd_end_lines_
				&= 0xfffffffe;
			par->VPP_hd_start_lines_ =
				par->VPP_hd_end_lines_ + 1
				- par->VPP_line_in_length_;
		} else
#endif
		{
		par->VPP_line_in_length_
			&= aligned_mask;
		par->VPP_hd_start_lines_
			&= aligned_mask;
		par->VPP_hd_end_lines_ =
			par->VPP_hd_start_lines_ +
			par->VPP_line_in_length_ - 1;
		/* if have el layer, need 2 pixel align by height */
		if (cur_dispbuf2) {
			old_len =
				par->VPP_vd_end_lines_ -
				par->VPP_vd_start_lines_ + 1;
			if (old_len & 1)
				par->VPP_vd_end_lines_--;
			if (par->VPP_vd_start_lines_ & 1) {
				par->VPP_vd_start_lines_--;
				par->VPP_vd_end_lines_--;
			}
			old_len =
				par->VPP_vd_end_lines_ -
				par->VPP_vd_start_lines_ + 1;
			old_len = old_len >> par->vscale_skip_count;
			if (par->VPP_pic_in_height_ < old_len)
				par->VPP_pic_in_height_ = old_len;
		}
		}
	}
}

void correct_vd2_mif_size_for_DV(
	struct vpp_frame_par_s *par,
	struct vframe_s *bl_vf)
{
	int width_bl, width_el, line_in_length;
	int shift;
	if ((is_dolby_vision_on() == true)
		&& (par->VPP_line_in_length_ > 0)
		&& !is_sc_enable_before_pps(par)) {
		/* work around to skip the size check when sc enable */
		width_el = (cur_dispbuf2->type
			& VIDTYPE_COMPRESS) ?
			cur_dispbuf2->compWidth :
			cur_dispbuf2->width;
		width_bl = (bl_vf->type
			& VIDTYPE_COMPRESS) ?
			bl_vf->compWidth :
			bl_vf->width;
		if (width_el >= width_bl)
			shift = 0;
		else
			shift = 1;
		zoom2_start_x_lines =
			par->VPP_hd_start_lines_ >> shift;
		line_in_length =
			par->VPP_line_in_length_ >> shift;
		zoom2_end_x_lines
			&= 0xfffffffe;
		line_in_length
			&= 0xfffffffe;
		if (line_in_length > 1)
			zoom2_end_x_lines =
				zoom2_start_x_lines +
				line_in_length - 1;
		else
			zoom2_end_x_lines = zoom2_start_x_lines;

		zoom2_start_y_lines =
			par->VPP_vd_start_lines_ >> shift;
		if (zoom2_start_y_lines >= zoom2_end_y_lines)
			zoom2_end_y_lines = zoom2_start_y_lines;
		/* TODO: if el len is 0, need disable bl */
	}
}
#if ENABLE_UPDATE_HDR_FROM_USER
void set_hdr_to_frame(struct vframe_s *vf)
{
	unsigned long flags;

	spin_lock_irqsave(&omx_hdr_lock, flags);

	if (has_hdr_info) {
		vf->prop.master_display_colour = vf_hdr;

		//config static signal_type for vp9
		vf->signal_type = (1 << 29)
			| (5 << 26) /* unspecified */
			| (0 << 25) /* limit */
			| (1 << 24) /* color available */
			| (9 << 16) /* 2020 */
			| (16 << 8) /* 2084 */
			| (9 << 0); /* 2020 */

		//pr_info("set_hdr_to_frame %d, signal_type 0x%x",
		//vf->prop.master_display_colour.present_flag,vf->signal_type);
	}
	spin_unlock_irqrestore(&omx_hdr_lock, flags);
}
#endif

#ifdef FIQ_VSYNC
void vsync_fisr_in(void)
#else
static irqreturn_t vsync_isr_in(int irq, void *dev_id)
#endif
{
	int hold_line;
	int enc_line;
	unsigned char frame_par_di_set = 0;
	s32 i, vout_type;
	struct vframe_s *vf;
	unsigned long flags;
#ifdef CONFIG_TVIN_VDIN
	struct vdin_v4l2_ops_s *vdin_ops = NULL;
	struct vdin_arg_s arg;
#endif
	bool show_nosync = false;
	u32 vpp_misc_save, vpp_misc_set;
	int first_set = 0;
	int toggle_cnt;
	struct vframe_s *toggle_vf = NULL;
	struct vframe_s *toggle_frame = NULL;
	int video1_off_req = 0;
	int video2_off_req = 0;
	struct vframe_s *cur_dispbuf_back = cur_dispbuf;
	static  struct vframe_s *pause_vf;

	if (debug_flag & DEBUG_FLAG_VSYNC_DONONE)
		return IRQ_HANDLED;

#ifdef CONFIG_SUPPORT_VIDEO_ON_VPP2
	const char *dev_id_s = (const char *)dev_id;
	int dev_id_len = strlen(dev_id_s);

	if (cur_dev == &video_dev[1]) {
		if (cur_dev_idx == 0) {
			cur_dev = &video_dev[0];
			vinfo = get_current_vinfo();
			vsync_pts_inc =
			    90000 * vinfo->sync_duration_den /
			    vinfo->sync_duration_num;
			vsync_pts_inc_scale = vinfo->sync_duration_den;
			vsync_pts_inc_scale_base = vinfo->sync_duration_num;
			video_property_changed = true;
			pr_info("Change to video 0\n");
		}
	} else {
		if (cur_dev_idx != 0) {
			cur_dev = &video_dev[1];
			vinfo = get_current_vinfo2();
			vsync_pts_inc =
			    90000 * vinfo->sync_duration_den /
			    vinfo->sync_duration_num;
			vsync_pts_inc_scale = vinfo->sync_duration_den;
			vsync_pts_inc_scale_base = vinfo->sync_duration_num;
			video_property_changed = true;
			pr_info("Change to video 1\n");
		}
	}

	if ((dev_id_s[dev_id_len - 1] == '2' && cur_dev_idx == 0) ||
	    (dev_id_s[dev_id_len - 1] != '2' && cur_dev_idx != 0))
		return IRQ_HANDLED;
	/* pr_info("%s: %s\n", __func__, dev_id_s); */
#endif

	if (omx_need_drop_frame_num > 0 && !omx_drop_done && omx_secret_mode) {
		struct vframe_s *vf = NULL;

		while (1) {
			vf = video_vf_peek();

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if (is_dolby_vision_enable()
				&& vf && is_dovi_frame(vf)) {
				pr_info("vsync_isr_in, ignore the omx %d frames drop for dv frame\n",
					omx_need_drop_frame_num);
				omx_need_drop_frame_num = 0;
				omx_drop_done = true;
				break;
			}
#endif
			if (vf) {
				if (omx_need_drop_frame_num >= vf->omx_index) {
					//pr_info("vsync drop omx_index %d\n",
						//vf->omx_index);
					vf = video_vf_get();
					video_vf_put(vf);
				} else {
					omx_drop_done = true;
					break;
				}
			} else
				break;
		}
	}

	vf = video_vf_peek();
	if ((vf) && ((vf->type & VIDTYPE_NO_VIDEO_ENABLE) == 0)) {
		if ((old_vmode != new_vmode) || (debug_flag == 8)) {
			debug_flag = 1;
			video_property_changed = true;
			pr_info("detect vout mode change!!!!!!!!!!!!\n");
			old_vmode = new_vmode;
		}
	}
	toggle_cnt = 0;
	vsync_count++;
	timer_count++;

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
	vlock_process(vf);/*need call every vsync*/
#endif

	switch (READ_VCBUS_REG(VPU_VIU_VENC_MUX_CTRL) & 0x3) {
	case 0:
		enc_line = (READ_VCBUS_REG(ENCL_INFO_READ) >> 16) & 0x1fff;
		break;
	case 1:
		enc_line = (READ_VCBUS_REG(ENCI_INFO_READ) >> 16) & 0x1fff;
		break;
	case 2:
		enc_line = (READ_VCBUS_REG(ENCP_INFO_READ) >> 16) & 0x1fff;
		break;
	case 3:
		enc_line = (READ_VCBUS_REG(ENCT_INFO_READ) >> 16) & 0x1fff;
		break;
	}
	if (enc_line > vsync_enter_line_max)
		vsync_enter_line_max = enc_line;

	if (is_meson_txlx_cpu() && dmc_adjust) {
		bool force_adjust = false;
		struct vframe_s *chk_vf;

		chk_vf = (vf != NULL) ? vf : cur_dispbuf;
		if (chk_vf)
			force_adjust =
				(chk_vf->type & VIDTYPE_VIU_444) ? true : false;
		if (chk_vf)
			dmc_adjust_for_mali_vpu(
				chk_vf->width,
				chk_vf->height,
				force_adjust);
		else
			dmc_adjust_for_mali_vpu(
				0, 0, force_adjust);
	}
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	vsync_rdma_config_pre();

	if (to_notify_trick_wait) {
		atomic_set(&trickmode_framedone, 1);
		video_notify_flag |= VIDEO_NOTIFY_TRICK_WAIT;
		to_notify_trick_wait = false;
		goto exit;
	}

	if (debug_flag & DEBUG_FLAG_PRINT_RDMA) {
		if (video_property_changed) {
			enable_rdma_log_count = 5;
			enable_rdma_log(1);
		}
		if (enable_rdma_log_count > 0)
			enable_rdma_log_count--;
	}
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	/* check video frame before VECM process */
	if (is_dolby_vision_enable() && vf)
		dolby_vision_check_hdr10(vf);
#endif
#ifdef CONFIG_TVIN_VDIN
	/* patch for m8 4k2k wifidisplay bandwidth bottleneck */
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_M8) {
		vdin_ops = get_vdin_v4l2_ops();
		if (vdin_ops && vdin_ops->tvin_vdin_func) {
			arg.cmd = VDIN_CMD_ISR;
			vdin_ops->tvin_vdin_func(1, &arg);
#ifdef CONFIG_AM_VIDEO2
			vdin_ops->tvin_vdin_func(0, &arg);
#endif
		}
	}
#endif
	vout_type = detect_vout_type();
	hold_line = calc_hold_line();
	if (vsync_pts_inc_upint) {
		if (vsync_pts_inc_adj) {
			/* pr_info("adj %d, org %d\n",*/
			/*vsync_pts_inc_adj, vsync_pts_inc); */
			timestamp_pcrscr_inc(vsync_pts_inc_adj);
			timestamp_apts_inc(vsync_pts_inc_adj);
		} else {
			timestamp_pcrscr_inc(vsync_pts_inc + 1);
			timestamp_apts_inc(vsync_pts_inc + 1);
		}
	} else {
		if (vsync_slow_factor == 0) {
			pr_info("invalid vsync_slow_factor, set to 1\n");
			vsync_slow_factor = 1;
		}

		if (vsync_slow_factor == 1) {
			timestamp_pcrscr_inc_scale(vsync_pts_inc_scale,
					vsync_pts_inc_scale_base);
			timestamp_apts_inc(vsync_pts_inc / vsync_slow_factor);
		} else if (vsync_slow_factor > 1000) {
			u32 inc = (vsync_slow_factor / 1000)
				* vsync_pts_inc / 1000;

			timestamp_pcrscr_inc(inc);
			timestamp_apts_inc(inc);
		} else {
			timestamp_pcrscr_inc(vsync_pts_inc / vsync_slow_factor);
			timestamp_apts_inc(vsync_pts_inc / vsync_slow_factor);
		}
	}
	if (omx_secret_mode == true) {
		u32 system_time = timestamp_pcrscr_get();
		int diff = system_time - omx_pts;

		if ((diff - omx_pts_interval_upper) > 0
			|| (diff - omx_pts_interval_lower) < 0
			|| (omx_pts_set_from_hwc_count <
			OMX_MAX_COUNT_RESET_SYSTEMTIME)) {
			timestamp_pcrscr_enable(1);
			/*pr_info("system_time=%d, omx_pts=%d, diff=%d\n",*/
			/*system_time, omx_pts, diff);*/
			/*add  greatest common divisor of duration*/
			/*1500(60fps) 3000(30fps) 3750(24fps) for some video*/
			/*that pts is not evenly*/
			timestamp_pcrscr_set(omx_pts + DURATION_GCD);
		}
	} else
		omx_pts = 0;
	if (trickmode_duration_count > 0)
		trickmode_duration_count -= vsync_pts_inc;
#ifdef VIDEO_PTS_CHASE
	if (vpts_chase)
		vpts_chase_counter--;
#endif

	if (slowsync_repeat_enable)
		frame_repeat_count++;

	if (smooth_sync_enable) {
		if (video_frame_repeat_count)
			video_frame_repeat_count++;
	}

	if (atomic_read(&video_unreg_flag))
		goto exit;

	if (atomic_read(&video_pause_flag)
		&& (!((video_global_output == 1)
		&& (video_enabled != video_status_saved))))
		goto exit;

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	if (is_vsync_rdma_enable())
		rdma_canvas_id = next_rdma_canvas_id;
	else {
		if (rdma_enable_pre) {
			/*do not write register directly before RDMA is done */
			/*#if MESON_CPU_TYPE == MESON_CPU_TYPE_MESON6TV */
			if (is_meson_mtvd_cpu()) {
				if (debug_flag & DEBUG_FLAG_RDMA_WAIT_1) {
					while
					(
					((READ_VCBUS_REG(ENCL_INFO_READ) >> 16)
					& 0x1fff) < 50);
				}
			}
			/* #endif */
			goto exit;
		}
		rdma_canvas_id = 0;
		next_rdma_canvas_id = 1;
	}

	for (i = 0; i < dispbuf_to_put_num; i++) {
		if (dispbuf_to_put[i]) {
			video_vf_put(dispbuf_to_put[i]);
			dispbuf_to_put[i] = NULL;
		}
		dispbuf_to_put_num = 0;
	}
#endif

	if (osd_prov && osd_prov->ops && osd_prov->ops->get) {
		vf = osd_prov->ops->get(osd_prov->op_arg);
		if (vf) {
			vf->source_type = VFRAME_SOURCE_TYPE_OSD;
			vsync_toggle_frame(vf);
			if (debug_flag & DEBUG_FLAG_BLACKOUT) {
				pr_info
				    ("[video4osd] toggle osd_vframe {%d,%d}\n",
				     vf->width, vf->height);
			}
			goto SET_FILTER;
		}
	}

	if ((!cur_dispbuf) || (cur_dispbuf == &vf_local)) {

		vf = video_vf_peek();

		if (vf) {
			if (hdmi_in_onvideo == 0) {
				tsync_avevent_locked(VIDEO_START,
						     (vf->pts) ? vf->pts :
						     timestamp_vpts_get());
				video_start_post = true;
			}

			if (show_first_frame_nosync || show_first_picture)
				show_nosync = true;

			if (slowsync_repeat_enable)
				frame_repeat_count = 0;

		} else if ((cur_dispbuf == &vf_local)
			   && (video_property_changed)) {
			if (!(blackout | force_blackout)) {
				if (cur_dispbuf
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
				&& ((DI_POST_REG_RD(DI_IF1_GEN_REG) & 0x1)
					== 0)
#endif
					) {
					/* setting video display*/
					/*property in unregister mode */
					u32 cur_index =
					    READ_VCBUS_REG(VD1_IF0_CANVAS0 +
							   cur_dev->viu_off);
					if ((get_cpu_type() >=
					MESON_CPU_MAJOR_ID_GXBB) &&
					(cur_dispbuf->type &
					VIDTYPE_COMPRESS)) {
						cur_dispbuf->compHeadAddr =
						READ_VCBUS_REG(AFBC_HEAD_BADDR)
						<< 4;
					} else {
						cur_dispbuf->canvas0Addr =
						cur_index;
					}
				}
				vsync_toggle_frame(cur_dispbuf);
			} else
				video_property_changed = false;
		} else {
			goto SET_FILTER;
		}
	}

	/* buffer switch management */
	vf = video_vf_peek();

	/* setting video display property in underflow mode */
	if ((!vf) && cur_dispbuf && (video_property_changed))
		vsync_toggle_frame(cur_dispbuf);

	/*debug info for skip & repeate vframe case*/
	if (!vf) {
		underflow++;
		if (video_dbg_vf&(1<<0))
			dump_vframe_status("vdin0");
		if (video_dbg_vf&(1<<1))
			dump_vframe_status("deinterlace");
		if (video_dbg_vf&(1<<2))
			dump_vframe_status("amlvideo2");
		if (video_dbg_vf&(1<<3))
			dump_vframe_status("ppmgr");
		if (video_dbg_vf&(1<<4))
			dump_vdin_reg();
	}
	video_get_vf_cnt = 0;
	if (platform_type == 1) {
		/* toggle_3d_fa_frame*/
		/* determine the out frame is L or R or blank */
		judge_3d_fa_out_mode();
	}
	while (vf) {
		if (vpts_expire(cur_dispbuf, vf, toggle_cnt) || show_nosync) {
			amlog_mask(LOG_MASK_TIMESTAMP,
			"vpts = 0x%x, c.dur=0x%x, n.pts=0x%x, scr = 0x%x\n",
				   timestamp_vpts_get(),
				   (cur_dispbuf) ? cur_dispbuf->duration : 0,
				   vf->pts, timestamp_pcrscr_get());

			amlog_mask_if(toggle_cnt > 0, LOG_MASK_FRAMESKIP,
				      "skipped\n");
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if (is_dolby_vision_enable()
				&& dolby_vision_need_wait())
				break;
#endif
#if ENABLE_UPDATE_HDR_FROM_USER
			set_hdr_to_frame(vf);
#endif

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
			refresh_on_vs(vf);
			if (amvecm_on_vs(
				(cur_dispbuf != &vf_local)
				? cur_dispbuf : NULL,
				vf, CSC_FLAG_CHECK_OUTPUT,
				cur_frame_par ?
				cur_frame_par->supsc1_hori_ratio :
				0,
				cur_frame_par ?
				cur_frame_par->supsc1_vert_ratio :
				0) == 1)
				break;
#endif
			/*
			 *two special case:
			 *case1:4k display case,input buffer not enough &
			 *	quickly for display
			 *case2:input buffer all not OK
			 */
			if (vf && hdmiin_frame_check &&
				(vf->source_type == VFRAME_SOURCE_TYPE_HDMI) &&
				(video_vf_disp_mode_get(vf) ==
				VFRAME_DISP_MODE_UNKNOWN) &&
				(hdmiin_frame_check_cnt++ < 10))
				break;
			else
				hdmiin_frame_check_cnt = 0;

			vf = video_vf_get();
			if (!vf)
				break;
			if (debug_flag & DEBUG_FLAG_LATENCY) {
				vf->ready_clock[2] = sched_clock();
				pr_info("video get latency %lld ms vdin put latency %lld ms. first %lld ms.\n",
				func_div(vf->ready_clock[2], 1000),
				func_div(vf->ready_clock[1], 1000),
				func_div(vf->ready_clock[0], 1000));
			}
			if (video_vf_dirty_put(vf))
				break;
			if (vf && hdmiin_frame_check && (vf->source_type ==
				VFRAME_SOURCE_TYPE_HDMI) &&
				video_vf_disp_mode_check(vf))
				break;
			force_blackout = 0;
			if ((platform_type == 1) ||
			(platform_type == 0)) {
				if (vf) {
					if (last_mode_3d !=
					vf->mode_3d_enable) {
						last_mode_3d =
						vf->mode_3d_enable;
						mode_3d_changed = 1;
					}
					video_3d_format = vf->trans_fmt;
				}
			}
			vsync_toggle_frame(vf);
			toggle_frame = vf;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if (is_dolby_vision_enable()) {
				toggle_vf = dolby_vision_toggle_frame(vf);
				video_pause_global = 0;
			} else
#endif
			{
				cur_dispbuf2 = NULL;
				video_pause_global = 2;
				pause_vf = NULL;
			}
			if (trickmode_fffb == 1) {
				trickmode_vpts = vf->pts;
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
				if ((DI_POST_REG_RD(DI_IF1_GEN_REG) & 0x1)
					!= 0) {
					atomic_set(&trickmode_framedone, 1);
					video_notify_flag |=
					    VIDEO_NOTIFY_TRICK_WAIT;
				} else
	#endif
					to_notify_trick_wait = true;
#else
				atomic_set(&trickmode_framedone, 1);
				video_notify_flag |= VIDEO_NOTIFY_TRICK_WAIT;
#endif
				break;
			}
			if (slowsync_repeat_enable)
				frame_repeat_count = 0;
			vf = video_vf_peek();
			if (!vf)
				next_peek_underflow++;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if (for_dolby_vision_certification()
			&& toggle_vf)
				break;
#endif
			if (debug_flag & DEBUG_FLAG_TOGGLE_FRAME_PER_VSYNC)
				break;
			video_get_vf_cnt++;
			if (video_get_vf_cnt >= 2)
				video_drop_vf_cnt++;
		} else {
			/* check if current frame's duration has expired,
			 *in this example
			 * it compares current frame display duration
			 * with 1/1/1/1.5 frame duration
			 * every 4 frames there will be one frame play
			 * longer than usual.
			 * you can adjust this array for any slow sync
			 * control as you want.
			 * The playback can be smoother than previous method.
			 */
			if (slowsync_repeat_enable) {
				if (duration_expire
				    (cur_dispbuf, vf,
				     frame_repeat_count * vsync_pts_inc)
				    && timestamp_pcrscr_enable_state()) {
					amlog_mask(LOG_MASK_SLOWSYNC,
					"slow sync toggle,repeat_count = %d\n",
					frame_repeat_count);
					amlog_mask(LOG_MASK_SLOWSYNC,
					"sys.time = 0x%x, video time = 0x%x\n",
					timestamp_pcrscr_get(),
					timestamp_vpts_get());
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
					if (is_dolby_vision_enable()
					&& dolby_vision_need_wait())
						break;
#endif
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
					refresh_on_vs(vf);
					if (amvecm_on_vs(
						(cur_dispbuf != &vf_local)
						? cur_dispbuf : NULL,
						vf, CSC_FLAG_CHECK_OUTPUT,
						cur_frame_par ?
						cur_frame_par->supsc1_hori_ratio
						: 0,
						cur_frame_par ?
						cur_frame_par->supsc1_vert_ratio
						: 0) == 1)
						break;
#endif
					vf = video_vf_get();
					if (!vf)
						break;
					vsync_toggle_frame(vf);
					toggle_frame = vf;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
					if (is_dolby_vision_enable())
						toggle_vf =
						dolby_vision_toggle_frame(vf);
					else
#endif
						cur_dispbuf2 = NULL;
					frame_repeat_count = 0;

					vf = video_vf_peek();
				} else if ((cur_dispbuf) &&
					(cur_dispbuf->duration_pulldown >
					vsync_pts_inc)) {
					frame_count++;
					cur_dispbuf->duration_pulldown -=
					    PTS2DUR(vsync_pts_inc);
				}
			} else {
				if ((cur_dispbuf)
				    && (cur_dispbuf->duration_pulldown >
					vsync_pts_inc)) {
					frame_count++;
					cur_dispbuf->duration_pulldown -=
					    PTS2DUR(vsync_pts_inc);
				}
			}
			/*  setting video display property in pause mode */
			if (video_property_changed && cur_dispbuf) {
				if (blackout | force_blackout) {
					if (cur_dispbuf != &vf_local)
						vsync_toggle_frame(
								cur_dispbuf);
				} else
					vsync_toggle_frame(cur_dispbuf);
				if (is_dolby_vision_enable()) {
					pause_vf = cur_dispbuf;
					video_pause_global = 1;
				} else {
					pause_vf = NULL;
					video_pause_global = 2;
				}
			}
			if (pause_vf && (video_pause_global == 1)
			    && is_dolby_vision_enable()) {
				toggle_vf = pause_vf;
				dolby_vision_parse_metadata(
					cur_dispbuf, 0, false);
				dolby_vision_set_toggle_flag(1);
			}
			break;
		}

		toggle_cnt++;
	}

#ifdef INTERLACE_FIELD_MATCH_PROCESS
	if (interlace_field_type_need_match(vout_type, vf)) {
		if (field_matching_count++ == FIELD_MATCH_THRESHOLD) {
			field_matching_count = 0;
			/* adjust system time to get one more field toggle */
			/* at next vsync to match field */
			timestamp_pcrscr_inc(vsync_pts_inc);
		}
	} else
		field_matching_count = 0;
#endif

SET_FILTER:
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
		amvecm_on_vs(
			(cur_dispbuf != &vf_local)
			? cur_dispbuf : NULL,
			toggle_frame,
			toggle_frame ? CSC_FLAG_TOGGLE_FRAME : 0,
			cur_frame_par ?
			cur_frame_par->supsc1_hori_ratio :
			0,
			cur_frame_par ?
			cur_frame_par->supsc1_vert_ratio :
			0);
#endif
	/* filter setting management */
	if ((frame_par_ready_to_set) || (frame_par_force_to_set)) {
		cur_frame_par = next_frame_par;
		frame_par_di_set = 1;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (is_dolby_vision_enable()) {
		u32 frame_size = 0, h_size, v_size;
		u8 pps_state = 0; /* pps no change */

		/* force toggle when keeping frame after playing */
		if ((cur_dispbuf == &vf_local)
			&& !toggle_vf
			&& is_dolby_vision_on()) {
			toggle_vf = cur_dispbuf;
			dolby_vision_parse_metadata(
				cur_dispbuf, 2, false);
			dolby_vision_set_toggle_flag(1);
		}
/* pause mode was moved to video display property */
#if 0
		/* force toggle in pause mode */
		if (cur_dispbuf
			&& (cur_dispbuf != &vf_local)
			&& !toggle_vf
			&& is_dolby_vision_on()
			&& !for_dolby_vision_certification()) {
			toggle_vf = cur_dispbuf;
			dolby_vision_parse_metadata(
				cur_dispbuf, 0, false);
			dolby_vision_set_toggle_flag(1);
		}
#endif
		if (cur_frame_par) {
			if (frame_par_ready_to_set || frame_par_force_to_set) {
				struct vppfilter_mode_s *vpp_filter =
					&cur_frame_par->vpp_filter;
				if ((vpp_filter->vpp_hsc_start_phase_step
					== 0x1000000) &&
					(vpp_filter->vpp_vsc_start_phase_step
					== 0x1000000) &&
					(vpp_filter->vpp_hsc_start_phase_step ==
					vpp_filter->vpp_hf_start_phase_step) &&
					!vpp_filter->vpp_pre_vsc_en &&
					!vpp_filter->vpp_pre_hsc_en &&
					!cur_frame_par->supsc0_enable &&
					!cur_frame_par->supsc1_enable &&
					bypass_pps)
					pps_state = 2; /* pps disable */
				else
					pps_state = 1; /* pps enable */
			}
			if (cur_frame_par->VPP_hd_start_lines_
				>=  cur_frame_par->VPP_hd_end_lines_)
				h_size = 0;
			else
				h_size = cur_frame_par->VPP_hd_end_lines_
				- cur_frame_par->VPP_hd_start_lines_ + 1;
			h_size /= (cur_frame_par->hscale_skip_count + 1);
			if (cur_frame_par->VPP_vd_start_lines_
				>=  cur_frame_par->VPP_vd_end_lines_)
				v_size = 0;
			else
				v_size = cur_frame_par->VPP_vd_end_lines_
				- cur_frame_par->VPP_vd_start_lines_ + 1;
			v_size /= (cur_frame_par->vscale_skip_count + 1);
			frame_size = (h_size << 16) | v_size;
		} else if (toggle_vf) {
			h_size = (toggle_vf->type & VIDTYPE_COMPRESS) ?
				toggle_vf->compWidth : toggle_vf->width;
			v_size = (toggle_vf->type & VIDTYPE_COMPRESS) ?
				toggle_vf->compHeight : toggle_vf->height;
			frame_size = (h_size << 16) | v_size;
		}
		dolby_vision_process(toggle_vf, frame_size, pps_state);
		dolby_vision_update_setting();
	}
#endif
	if ((platform_type == 1) || (platform_type == 0)) {
		if (mode_3d_changed) {
			mode_3d_changed = 0;
			frame_par_force_to_set = 1;
		}
	}
	if (cur_dispbuf_back != cur_dispbuf) {
		display_frame_count++;
		drop_frame_count = receive_frame_count - display_frame_count;
	}
	if (cur_dispbuf) {
		struct f2v_vphase_s *vphase;
		u32 vin_type = cur_dispbuf->type & VIDTYPE_TYPEMASK;
		{
			int need_afbc = (cur_dispbuf->type & VIDTYPE_COMPRESS);
			int afbc_need_reset =
				video_enabled &&
				need_afbc &&
				(!(READ_VCBUS_REG(AFBC_ENABLE) & 0x100));
			/*video on && afbc is off && is compress frame.*/
			if (frame_par_ready_to_set || afbc_need_reset) {
				if (cur_frame_par) {
					viu_set_dcu(cur_frame_par, cur_dispbuf);
					if (cur_dispbuf2)
						vd2_set_dcu(cur_frame_par,
							cur_dispbuf2);
				}
			} else if (cur_dispbuf2) {
				u32 new_el_w =
					(cur_dispbuf2->type
					& VIDTYPE_COMPRESS) ?
					cur_dispbuf2->compWidth :
					cur_dispbuf2->width;
				if (new_el_w != last_el_w) {
					pr_info("reset vd2 dcu for el change, %d->%d, %p--%p\n",
						last_el_w, new_el_w,
						cur_dispbuf, cur_dispbuf2);
					vd2_set_dcu(cur_frame_par,
						cur_dispbuf2);
				}
			} else {
				last_el_w = 0;
				last_el_status = 0;
			}
		}
		{
#if 0
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
			if (cur_dispbuf->type & VIDTYPE_COMPRESS) {
				/*SET_VCBUS_REG_MASK(VIU_MISC_CTRL0,*/
				    /*VIU_MISC_AFBC_VD1);*/
				VSYNC_WR_MPEG_REG_BITS(VIU_MISC_CTRL0 +
					cur_dev->viu_off, 1, 20, 1);
			} else {
				/*CLEAR_VCBUS_REG_MASK(VIU_MISC_CTRL0,*/
				    /*VIU_MISC_AFBC_VD1);*/
				VSYNC_WR_MPEG_REG_BITS(VIU_MISC_CTRL0 +
					cur_dev->viu_off, 0, 20, 1);
			}
		}
#endif

		if (platform_type == 1) {
			if (cur_frame_par && cur_frame_par->hscale_skip_count) {
				VSYNC_WR_MPEG_REG_BITS(VIU_VD1_FMT_CTRL +
					cur_dev->viu_off, 1, 20, 1);
				/* HFORMATTER_EN */
				VSYNC_WR_MPEG_REG_BITS(VIU_VD2_FMT_CTRL +
					cur_dev->viu_off, 1, 20, 1);
				/* HFORMATTER_EN */
			}
			if (process_3d_type & MODE_3D_OUT_FA_MASK) {
				if (toggle_3d_fa_frame == OUT_FA_A_FRAME) {
					VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
						cur_dev->vpp_off, 1, 14, 1);
					/* VPP_VD1_PREBLEND disable */
					VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
						cur_dev->vpp_off, 1, 10, 1);
					/* VPP_VD1_POSTBLEND disable */
					VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_PSEL +
					cur_dev->viu_off, 0x4000000);
					VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_PSEL +
					cur_dev->viu_off, 0x4000000);
					VSYNC_WR_MPEG_REG(VD2_IF0_LUMA_PSEL +
					cur_dev->viu_off, 0x4000000);
					VSYNC_WR_MPEG_REG(VD2_IF0_CHROMA_PSEL +
					cur_dev->viu_off, 0x4000000);
				} else if (OUT_FA_B_FRAME ==
				toggle_3d_fa_frame) {
					VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
					cur_dev->vpp_off, 1, 14, 1);
					/* VPP_VD1_PREBLEND disable */
					VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
					cur_dev->vpp_off, 1, 10, 1);
					/* VPP_VD1_POSTBLEND disable */
					VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_PSEL +
					cur_dev->viu_off, 0);
					VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_PSEL +
					cur_dev->viu_off, 0);
					VSYNC_WR_MPEG_REG(
						VD2_IF0_LUMA_PSEL +
						cur_dev->viu_off, 0);
					VSYNC_WR_MPEG_REG(
						VD2_IF0_CHROMA_PSEL +
						cur_dev->viu_off, 0);
				} else if (toggle_3d_fa_frame ==
				OUT_FA_BANK_FRAME) {
					/* output a banking frame */
					VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
						cur_dev->vpp_off, 0, 14, 1);
					/* VPP_VD1_PREBLEND disable */
					VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
					cur_dev->vpp_off, 0, 10, 1);
					/* VPP_VD1_POSTBLEND disable */
				}
			}
			if ((process_3d_type & MODE_3D_OUT_TB)
				|| (process_3d_type & MODE_3D_OUT_LR)) {
				if (cur_frame_par &&
					(cur_frame_par->vpp_2pic_mode &
				VPP_PIC1_FIRST)) {
					VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_PSEL +
					cur_dev->viu_off, 0x4000000);
					VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_PSEL +
					cur_dev->viu_off, 0x4000000);
					VSYNC_WR_MPEG_REG(VD2_IF0_LUMA_PSEL +
					cur_dev->viu_off, 0);
					VSYNC_WR_MPEG_REG(VD2_IF0_CHROMA_PSEL +
					cur_dev->viu_off, 0);
				} else {
					VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_PSEL +
					cur_dev->viu_off, 0);
					VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_PSEL +
					cur_dev->viu_off, 0);
					VSYNC_WR_MPEG_REG(VD2_IF0_LUMA_PSEL +
					cur_dev->viu_off, 0x4000000);
					VSYNC_WR_MPEG_REG(VD2_IF0_CHROMA_PSEL +
					cur_dev->viu_off, 0x4000000);
				}
/*
 *VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
 *cur_dev->vpp_off,1,15,1);//VPP_VD2_PREBLEND enable
 *VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
 *cur_dev->vpp_off,1,11,1);//VPP_VD2_POSTBLEND enable
 *VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
 *cur_dev->vpp_off,1,6,1);//PREBLEND enable must be set!
 *VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
 *cur_dev->vpp_off,0x1ff,
 *VPP_VD2_ALPHA_BIT,9);//vd2 alpha must set
 */
			}

/*
 *else{
 *VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
 *cur_dev->vpp_off,0,15,1);//VPP_VD2_PREBLEND enable
 *VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
 *cur_dev->vpp_off,1,11,1);//VPP_VD2_POSTBLEND enable
 *VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
 *cur_dev->vpp_off,0,6,1);//PREBLEND enable
 *VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
 *cur_dev->vpp_off,0,VPP_VD2_ALPHA_BIT,9);//vd2 alpha must set
 *}
 */
		}
			/* vertical phase */
			vphase =
			&cur_frame_par->VPP_vf_ini_phase_
			[vpp_phase_table[vin_type]
			[vout_type]];
			VSYNC_WR_MPEG_REG(VPP_VSC_INI_PHASE + cur_dev->vpp_off,
					  ((u32) (vphase->phase) << 8));

			if (vphase->repeat_skip >= 0) {
				/* skip lines */
				VSYNC_WR_MPEG_REG_BITS(VPP_VSC_PHASE_CTRL +
				cur_dev->vpp_off,
				skip_tab[vphase->repeat_skip],
				VPP_PHASECTL_INIRCVNUMT_BIT,
				VPP_PHASECTL_INIRCVNUM_WID +
				VPP_PHASECTL_INIRPTNUM_WID);

			} else {
				/* repeat first line */
				VSYNC_WR_MPEG_REG_BITS(VPP_VSC_PHASE_CTRL +
				cur_dev->vpp_off, 4,
				VPP_PHASECTL_INIRCVNUMT_BIT,
				VPP_PHASECTL_INIRCVNUM_WID);
				VSYNC_WR_MPEG_REG_BITS(VPP_VSC_PHASE_CTRL +
				cur_dev->vpp_off,
				1 - vphase->repeat_skip,
				VPP_PHASECTL_INIRPTNUMT_BIT,
				VPP_PHASECTL_INIRPTNUM_WID);
			}
			if (platform_type == 1) {
				if (force_3d_scaler == 3 &&
					cur_frame_par &&
					cur_frame_par->vpp_3d_scale) {
					VSYNC_WR_MPEG_REG_BITS(
					VPP_VSC_PHASE_CTRL, 3,
					VPP_PHASECTL_DOUBLELINE_BIT, 2);
				} else if (force_3d_scaler == 1 &&
					cur_frame_par &&
					cur_frame_par->vpp_3d_scale) {
					VSYNC_WR_MPEG_REG_BITS(
					VPP_VSC_PHASE_CTRL, 1,
					VPP_PHASECTL_DOUBLELINE_BIT,
					VPP_PHASECTL_DOUBLELINE_WID);
				} else if (force_3d_scaler == 2 &&
					cur_frame_par &&
					cur_frame_par->vpp_3d_scale) {
					VSYNC_WR_MPEG_REG_BITS(
					VPP_VSC_PHASE_CTRL, 2,
					VPP_PHASECTL_DOUBLELINE_BIT, 2);
				} else {
					VSYNC_WR_MPEG_REG_BITS(
					VPP_VSC_PHASE_CTRL, 0,
					VPP_PHASECTL_DOUBLELINE_BIT, 2);
				}
			}
		}
	}

	if (((frame_par_ready_to_set) || (frame_par_force_to_set)) &&
	    (cur_frame_par)) {
		struct vppfilter_mode_s *vpp_filter =
		    &cur_frame_par->vpp_filter;

		if (cur_dispbuf) {
			u32 zoom_start_y, zoom_end_y;
			correct_vd1_mif_size_for_DV(cur_frame_par);
			if (cur_dispbuf->type & VIDTYPE_INTERLACE) {
				if (cur_dispbuf->type & VIDTYPE_VIU_FIELD) {
					zoom_start_y =
					cur_frame_par->VPP_vd_start_lines_
					>> 1;
					zoom_end_y =
					((cur_frame_par->VPP_vd_end_lines_ + 1)
					>> 1) - 1;
				} else {
					zoom_start_y =
					cur_frame_par->VPP_vd_start_lines_;
					zoom_end_y =
					cur_frame_par->VPP_vd_end_lines_;
				}
			} else {
				if (cur_dispbuf->type & VIDTYPE_VIU_FIELD) {
					zoom_start_y =
					cur_frame_par->VPP_vd_start_lines_;
					zoom_end_y =
					cur_frame_par->VPP_vd_end_lines_;
				} else {
					if (is_need_framepacking_output()) {
						zoom_start_y =
	cur_frame_par->VPP_vd_start_lines_ >> 1;
						zoom_end_y =
	((cur_frame_par->VPP_vd_end_lines_
	- framepacking_blank + 1) >> 1) - 1;
					} else {
						zoom_start_y =
	cur_frame_par->VPP_vd_start_lines_ >> 1;
						zoom_end_y =
	((cur_frame_par->VPP_vd_end_lines_ + 1) >> 1) - 1;
					}
				}
			}

			zoom_start_x_lines =
					cur_frame_par->VPP_hd_start_lines_;
			zoom_end_x_lines = cur_frame_par->VPP_hd_end_lines_;
			zoom_display_horz(cur_frame_par->hscale_skip_count);

			zoom_start_y_lines = zoom_start_y;
			zoom_end_y_lines = zoom_end_y;
			zoom_display_vert();
			if (is_dolby_vision_enable() && cur_dispbuf2) {
				zoom2_start_x_lines = ori2_start_x_lines;
				zoom2_end_x_lines = ori2_end_x_lines;
				zoom2_start_y_lines = ori2_start_y_lines;
				zoom2_end_y_lines = ori2_end_y_lines;
				correct_vd2_mif_size_for_DV(
					cur_frame_par, cur_dispbuf);
				vd2_zoom_display_horz(0);
				vd2_zoom_display_vert();
			}
		}
		/*vpp input size setting*/
		VSYNC_WR_MPEG_REG(VPP_IN_H_V_SIZE,
			((cur_frame_par->video_input_w & 0x1fff) <<
			16) | (cur_frame_par->video_input_h & 0x1fff));

		/* vpp super scaler */
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
			vpp_set_super_scaler_regs(cur_frame_par->supscl_path,
				cur_frame_par->supsc0_enable,
				cur_frame_par->spsc0_w_in,
				cur_frame_par->spsc0_h_in,
				cur_frame_par->supsc0_hori_ratio,
				cur_frame_par->supsc0_vert_ratio,
				cur_frame_par->supsc1_enable,
				cur_frame_par->spsc1_w_in,
				cur_frame_par->spsc1_h_in,
				cur_frame_par->supsc1_hori_ratio,
				cur_frame_par->supsc1_vert_ratio,
				vinfo->width,
				vinfo->height);
			if (is_dolby_vision_on() &&
				is_dolby_vision_stb_mode() &&
				!cur_frame_par->supsc0_enable &&
				!cur_frame_par->supsc1_enable) {
				VSYNC_WR_MPEG_REG(VPP_SRSHARP0_CTRL, 0);
				VSYNC_WR_MPEG_REG(VPP_SRSHARP1_CTRL, 0);
			}
		}

		/* vpp filters */
		/* SET_MPEG_REG_MASK(VPP_SC_MISC + cur_dev->vpp_off, */
		/* VPP_SC_TOP_EN | VPP_SC_VERT_EN | VPP_SC_HORZ_EN); */
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (for_dolby_vision_certification()) {
			/* turn off PPS for Dolby Vision certification */
			VSYNC_WR_MPEG_REG_BITS(VPP_SC_MISC + cur_dev->vpp_off,
				0, VPP_SC_TOP_EN_BIT, VPP_SC_TOP_EN_WID);
		} else
#endif
		{
			VSYNC_WR_MPEG_REG(VPP_SC_MISC + cur_dev->vpp_off,
				  READ_VCBUS_REG(VPP_SC_MISC +
						 cur_dev->vpp_off) |
				  VPP_SC_TOP_EN | VPP_SC_VERT_EN |
				  VPP_SC_HORZ_EN);

			/* pps pre hsc&vsc en */
			VSYNC_WR_MPEG_REG_BITS(VPP_SC_MISC + cur_dev->vpp_off,
				vpp_filter->vpp_pre_hsc_en,
				VPP_SC_PREHORZ_EN_BIT, 1);
			VSYNC_WR_MPEG_REG_BITS(VPP_SC_MISC + cur_dev->vpp_off,
				vpp_filter->vpp_pre_vsc_en,
				VPP_SC_PREVERT_EN_BIT, 1);
			VSYNC_WR_MPEG_REG_BITS(VPP_SC_MISC + cur_dev->vpp_off,
				vpp_filter->vpp_pre_vsc_en,
				VPP_LINE_BUFFER_EN_BIT, 1);
		}
		/* for bypass pps debug */
		if ((vpp_filter->vpp_hsc_start_phase_step == 0x1000000) &&
			(vpp_filter->vpp_vsc_start_phase_step == 0x1000000) &&
			(vpp_filter->vpp_hsc_start_phase_step ==
			vpp_filter->vpp_hf_start_phase_step) &&
			!vpp_filter->vpp_pre_vsc_en &&
			!vpp_filter->vpp_pre_hsc_en &&
			bypass_pps)
			VSYNC_WR_MPEG_REG_BITS(VPP_SC_MISC + cur_dev->vpp_off,
				0, VPP_SC_TOP_EN_BIT, VPP_SC_TOP_EN_WID);
	/*turn off vertical scaler when 3d display */
	/* CLEAR_MPEG_REG_MASK(VPP_SC_MISC,VPP_SC_VERT_EN); */
		if (platform_type == 1) {
			if (last_mode_3d) {
				VSYNC_WR_MPEG_REG(
				VPP_SC_MISC + cur_dev->vpp_off,
				READ_MPEG_REG(VPP_SC_MISC +
				cur_dev->vpp_off) &
				(~VPP_SC_VERT_EN));
			}
		}
		/* horitontal filter settings */
		VSYNC_WR_MPEG_REG_BITS(
		VPP_SC_MISC + cur_dev->vpp_off,
		vpp_filter->vpp_horz_coeff[0],
		VPP_SC_HBANK_LENGTH_BIT,
		VPP_SC_BANK_LENGTH_WID);

		/* fix the pps last line dummy issue */
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B))
			VSYNC_WR_MPEG_REG_BITS(
				VPP_SC_MISC + cur_dev->vpp_off,
				1, 24, 1);

		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
		if ((get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		    && !is_meson_mtvd_cpu()) {
			VSYNC_WR_MPEG_REG_BITS(VPP_VSC_PHASE_CTRL +
				cur_dev->vpp_off,
				(vpp_filter->vpp_vert_coeff[0] == 2) ? 1 : 0,
				VPP_PHASECTL_DOUBLELINE_BIT,
				VPP_PHASECTL_DOUBLELINE_WID);
		}
		/* #endif */

		if (vpp_filter->vpp_horz_coeff[1] & 0x8000) {
			VSYNC_WR_MPEG_REG(VPP_SCALE_COEF_IDX +
				cur_dev->vpp_off,
				VPP_COEF_HORZ | VPP_COEF_9BIT);
		} else {
			VSYNC_WR_MPEG_REG(VPP_SCALE_COEF_IDX +
				cur_dev->vpp_off,
				VPP_COEF_HORZ);
		}

		for (i = 0; i < (vpp_filter->vpp_horz_coeff[1] & 0xff); i++) {
			VSYNC_WR_MPEG_REG(VPP_SCALE_COEF + cur_dev->vpp_off,
					  vpp_filter->vpp_horz_coeff[i + 2]);
		}

		/* vertical filter settings */
		VSYNC_WR_MPEG_REG_BITS(VPP_SC_MISC + cur_dev->vpp_off,
				       vpp_filter->vpp_vert_coeff[0],
				       VPP_SC_VBANK_LENGTH_BIT,
				       VPP_SC_BANK_LENGTH_WID);

		VSYNC_WR_MPEG_REG(VPP_SCALE_COEF_IDX + cur_dev->vpp_off,
				  VPP_COEF_VERT);
		for (i = 0; i < vpp_filter->vpp_vert_coeff[1]; i++) {
			VSYNC_WR_MPEG_REG(VPP_SCALE_COEF + cur_dev->vpp_off,
					  vpp_filter->vpp_vert_coeff[i + 2]);
		}

		/* vertical chroma filter settings */
		if (vpp_filter->vpp_vert_chroma_filter_en) {
			const u32 *pCoeff = vpp_filter->vpp_vert_chroma_coeff;

			VSYNC_WR_MPEG_REG(
				VPP_SCALE_COEF_IDX + cur_dev->vpp_off,
				VPP_COEF_VERT_CHROMA|VPP_COEF_SEP_EN);
			for (i = 0; i < pCoeff[1]; i++)
				VSYNC_WR_MPEG_REG(
					VPP_SCALE_COEF + cur_dev->vpp_off,
					pCoeff[i + 2]);
		}
		/* work around to cut the last green line
		 *when two layer dv display and do vskip
		 */
		if (is_dolby_vision_on() &&
			(cur_frame_par->vscale_skip_count > 0)
			&& cur_dispbuf2
			&& (cur_frame_par->VPP_pic_in_height_ > 0))
			cur_frame_par->VPP_pic_in_height_--;
		VSYNC_WR_MPEG_REG(VPP_PIC_IN_HEIGHT + cur_dev->vpp_off,
				  cur_frame_par->VPP_pic_in_height_);

		VSYNC_WR_MPEG_REG_BITS(VPP_HSC_PHASE_CTRL + cur_dev->vpp_off,
				       cur_frame_par->VPP_hf_ini_phase_,
				       VPP_HSC_TOP_INI_PHASE_BIT,
				       VPP_HSC_TOP_INI_PHASE_WID);
		VSYNC_WR_MPEG_REG(VPP_POSTBLEND_VD1_H_START_END +
				  cur_dev->vpp_off,
				  ((cur_frame_par->VPP_post_blend_vd_h_start_ &
				    VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
				  ((cur_frame_par->VPP_post_blend_vd_h_end_ &
				    VPP_VD_SIZE_MASK)
				   << VPP_VD1_END_BIT));
		VSYNC_WR_MPEG_REG(VPP_POSTBLEND_VD1_V_START_END +
				  cur_dev->vpp_off,
				  ((cur_frame_par->VPP_post_blend_vd_v_start_ &
				    VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
				  ((cur_frame_par->VPP_post_blend_vd_v_end_ &
				    VPP_VD_SIZE_MASK)
				   << VPP_VD1_END_BIT));

		if ((cur_frame_par->VPP_post_blend_vd_v_end_ -
		     cur_frame_par->VPP_post_blend_vd_v_start_ + 1) > 1080) {
			VSYNC_WR_MPEG_REG(VPP_PREBLEND_VD1_V_START_END +
			cur_dev->vpp_off,
			((cur_frame_par->VPP_post_blend_vd_v_start_ &
			VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
			((cur_frame_par->VPP_post_blend_vd_v_end_ &
			VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
		} else {
			VSYNC_WR_MPEG_REG(VPP_PREBLEND_VD1_V_START_END +
				cur_dev->vpp_off,
				((0 & VPP_VD_SIZE_MASK) <<
				VPP_VD1_START_BIT) | ((1079 &
				VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
		}

		if (!legacy_vpp) {
			VSYNC_WR_MPEG_REG(
				VPP_PREBLEND_H_SIZE + cur_dev->vpp_off,
				(cur_frame_par->video_input_h << 16)
				| cur_frame_par->video_input_w);
			VSYNC_WR_MPEG_REG(
				VPP_POSTBLEND_H_SIZE + cur_dev->vpp_off,
				((cur_frame_par->VPP_post_blend_vd_v_end_ + 1)
				<< 16) |
				cur_frame_par->VPP_post_blend_h_size_);
		} else {
			VSYNC_WR_MPEG_REG(
				VPP_PREBLEND_H_SIZE + cur_dev->vpp_off,
				cur_frame_par->VPP_line_in_length_);
			VSYNC_WR_MPEG_REG(
				VPP_POSTBLEND_H_SIZE + cur_dev->vpp_off,
				cur_frame_par->VPP_post_blend_h_size_);
		}

		vpp_settings_h(cur_frame_par);
		vpp_settings_v(cur_frame_par);
		if (is_dolby_vision_enable() && cur_dispbuf2) {
			vd2_settings_h(cur_dispbuf2);
			vd2_settings_v(cur_dispbuf2);
		}
		frame_par_ready_to_set = 0;
		frame_par_force_to_set = 0;
		first_set = 1;
	}
	/* VPP one time settings */
	wait_sync = 0;

	if (!legacy_vpp && vinfo) {
		u32 read_value = VSYNC_RD_MPEG_REG(
			VPP_POSTBLEND_H_SIZE + cur_dev->vpp_off);
		if (((vinfo->field_height << 16) | vinfo->width)
			!= read_value)
			VSYNC_WR_MPEG_REG(
				VPP_POSTBLEND_H_SIZE + cur_dev->vpp_off,
				((vinfo->field_height << 16) | vinfo->width));
	} else if (vinfo) {
		if (VSYNC_RD_MPEG_REG(
			VPP_POSTBLEND_H_SIZE + cur_dev->vpp_off)
			!= vinfo->width)
			VSYNC_WR_MPEG_REG(
				VPP_POSTBLEND_H_SIZE + cur_dev->vpp_off,
				vinfo->width);
	}

	if (cur_dispbuf && cur_dispbuf->process_fun) {
		/* for new deinterlace driver */
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		if (debug_flag & DEBUG_FLAG_PRINT_RDMA) {
			if (enable_rdma_log_count > 0)
				pr_info("call process_fun\n");
		}
#endif
		cur_dispbuf->process_fun(cur_dispbuf->private_data,
					 zoom_start_x_lines |
					 (cur_frame_par->vscale_skip_count <<
					  24) | (frame_par_di_set << 16),
					 zoom_end_x_lines, zoom_start_y_lines,
					 zoom_end_y_lines, cur_dispbuf);
	}

 exit:
#if defined(PTS_LOGGING) || defined(PTS_TRACE_DEBUG)
		pts_trace++;
#endif
	vpp_misc_save = READ_VCBUS_REG(VPP_MISC + cur_dev->vpp_off);
	vpp_misc_set = vpp_misc_save;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
	if (!is_dolby_vision_on())
		vpp_misc_set |= VPP_CM_ENABLE;
	else
		vpp_misc_set &= ~VPP_CM_ENABLE;
#endif

	if (bypass_cm)
		vpp_misc_set &= ~VPP_CM_ENABLE;

	if (update_osd_vpp_misc && legacy_vpp) {
		vpp_misc_set &= ~osd_vpp_misc_mask;
		vpp_misc_set |=
			(osd_vpp_misc & osd_vpp_misc_mask);
		if (vpp_misc_set &
			(VPP_OSD1_POSTBLEND | VPP_OSD2_POSTBLEND))
			vpp_misc_set |= VPP_POSTBLEND_EN;
	}
	if ((video_enabled == 1) && ((vpp_misc_save & VPP_VD1_POSTBLEND) == 0)
		&& (video_onoff_state == VIDEO_ENABLE_STATE_IDLE)) {
		vpp_misc_set |=
			VPP_VD1_PREBLEND |
			VPP_VD1_POSTBLEND |
			VPP_POSTBLEND_EN;
	}
	if ((video_enabled == 1) && cur_frame_par
	&& (cur_dispbuf != &vf_local) && (first_set == 0)
	&& (video_onoff_state == VIDEO_ENABLE_STATE_IDLE)) {
		struct vppfilter_mode_s *vpp_filter =
		    &cur_frame_par->vpp_filter;
		u32 h_phase_step, v_phase_step;

		h_phase_step = READ_VCBUS_REG(
		VPP_HSC_START_PHASE_STEP + cur_dev->vpp_off);
		v_phase_step = READ_VCBUS_REG(
		VPP_VSC_START_PHASE_STEP + cur_dev->vpp_off);
		if ((vpp_filter->vpp_hf_start_phase_step != h_phase_step) ||
		(vpp_filter->vpp_vsc_start_phase_step != v_phase_step)) {
			video_property_changed = true;
			/*pr_info("frame info register rdma write fail!\n");*/
		}
	}
	if (likely(video_onoff_state != VIDEO_ENABLE_STATE_IDLE)) {
		/* state change for video layer enable/disable */

		spin_lock_irqsave(&video_onoff_lock, flags);

		if (video_onoff_state == VIDEO_ENABLE_STATE_ON_REQ) {
			/*
			 * the video layer is enabled one vsync later,assumming
			 * all registers are ready from RDMA.
			 */
			video_onoff_state = VIDEO_ENABLE_STATE_ON_PENDING;
		} else if (video_onoff_state ==
			VIDEO_ENABLE_STATE_ON_PENDING) {
			vpp_misc_set |= VPP_VD1_PREBLEND |
				VPP_VD1_POSTBLEND |
				VPP_POSTBLEND_EN;

			video_onoff_state = VIDEO_ENABLE_STATE_IDLE;
			video_onoff_time = jiffies_to_msecs(jiffies);

			if (debug_flag & DEBUG_FLAG_BLACKOUT)
				pr_info("VsyncEnableVideoLayer\n");
			vpu_delay_work_flag |=
				VPU_VIDEO_LAYER1_CHANGED;
		} else if (video_onoff_state == VIDEO_ENABLE_STATE_OFF_REQ) {
			vpp_misc_set &= ~(VPP_VD1_PREBLEND |
				  VPP_VD1_POSTBLEND);
			if (process_3d_type)
				vpp_misc_set &= ~(VPP_VD2_PREBLEND |
					VPP_VD2_POSTBLEND | VPP_PREBLEND_EN);
			/*auto disable sr when video off*/
			VSYNC_WR_MPEG_REG(VPP_SRSHARP0_CTRL, 0);
			VSYNC_WR_MPEG_REG(VPP_SRSHARP1_CTRL, 0);
			video_onoff_state = VIDEO_ENABLE_STATE_IDLE;
			video_onoff_time = jiffies_to_msecs(jiffies);
			vpu_delay_work_flag |=
				VPU_VIDEO_LAYER1_CHANGED;
			if (debug_flag & DEBUG_FLAG_BLACKOUT)
				pr_info("VsyncDisableVideoLayer\n");
			video1_off_req = 1;
		}

		spin_unlock_irqrestore(&video_onoff_lock, flags);
	}

	if (likely(video2_onoff_state != VIDEO_ENABLE_STATE_IDLE)) {
		/* state change for video layer2 enable/disable */

		spin_lock_irqsave(&video2_onoff_lock, flags);

		if (video2_onoff_state == VIDEO_ENABLE_STATE_ON_REQ) {
			/*
			 * the video layer 2
			 * is enabled one vsync later, assumming
			 * all registers are ready from RDMA.
			 */
			video2_onoff_state = VIDEO_ENABLE_STATE_ON_PENDING;
		} else if (video2_onoff_state ==
			VIDEO_ENABLE_STATE_ON_PENDING) {
			if (is_dolby_vision_on())
				vpp_misc_set &= ~(VPP_VD2_PREBLEND |
					VPP_VD2_POSTBLEND | VPP_PREBLEND_EN);
			else if (process_3d_type)
				vpp_misc_set |= VPP_VD2_PREBLEND |
					VPP_PREBLEND_EN;
			else if (!legacy_vpp)
				vpp_misc_set |= VPP_VD2_POSTBLEND |
					VPP_POSTBLEND_EN;
			else
				vpp_misc_set |= VPP_VD2_PREBLEND |
					VPP_PREBLEND_EN;

			/* g12a has no alpha overflow check in hardware */
			if (!legacy_vpp)
				vpp_misc_set |= (0x100 << VPP_VD2_ALPHA_BIT);
			else
				vpp_misc_set |= (0x1ff << VPP_VD2_ALPHA_BIT);
			video2_onoff_state = VIDEO_ENABLE_STATE_IDLE;
			video_onoff_time = jiffies_to_msecs(jiffies);

			if (debug_flag & DEBUG_FLAG_BLACKOUT)
				pr_info("VsyncEnableVideoLayer2\n");
		} else if (video2_onoff_state == VIDEO_ENABLE_STATE_OFF_REQ) {
			vpp_misc_set &= ~(VPP_VD2_PREBLEND |
				VPP_VD2_POSTBLEND | VPP_PREBLEND_EN);
			video2_onoff_state = VIDEO_ENABLE_STATE_IDLE;
			video_onoff_time = jiffies_to_msecs(jiffies);

			if (debug_flag & DEBUG_FLAG_BLACKOUT)
				pr_info("VsyncDisableVideoLayer2\n");
			video2_off_req = 1;
		}
		spin_unlock_irqrestore(&video2_onoff_lock, flags);
	}

	if (video_global_output == 0) {
		video_enabled = 0;
		vpp_misc_set &= ~(VPP_VD1_PREBLEND |
			VPP_VD2_PREBLEND |
			VPP_VD2_POSTBLEND |
			VPP_VD1_POSTBLEND |
			VPP_PREBLEND_EN);
	} else {
		video_enabled = video_status_saved;
	}

	if (!video_enabled &&
		(vpp_misc_set & VPP_VD1_POSTBLEND))
		vpp_misc_set &= ~(VPP_VD1_PREBLEND |
			VPP_VD2_PREBLEND |
			VPP_VD2_POSTBLEND |
			VPP_VD1_POSTBLEND |
			VPP_PREBLEND_EN);

	if (!legacy_vpp) {
		u32 set_value = 0;

		vpp_misc_set &=
			((1 << 29) | VPP_CM_ENABLE |
			(0x1ff << VPP_VD2_ALPHA_BIT) |
			VPP_VD2_PREBLEND |
			VPP_VD1_PREBLEND |
			VPP_VD2_POSTBLEND |
			VPP_VD1_POSTBLEND |
			VPP_PREBLEND_EN |
			VPP_POSTBLEND_EN |
			0xf);
		vpp_misc_save &=
			((1 << 29) | VPP_CM_ENABLE |
			(0x1ff << VPP_VD2_ALPHA_BIT) |
			VPP_VD2_PREBLEND |
			VPP_VD1_PREBLEND |
			VPP_VD2_POSTBLEND |
			VPP_VD1_POSTBLEND |
			VPP_PREBLEND_EN |
			VPP_POSTBLEND_EN |
			0xf);
		if (vpp_misc_set != vpp_misc_save) {
			/* vd1 need always enable pre bld */
			if (vpp_misc_set & VPP_VD1_POSTBLEND)
				set_value =
					((1 << 16) | /* post bld premult*/
					(1 << 8) | /* post src */
					(1 << 4) | /* pre bld premult*/
					(1 << 0)); /* pre bld src 1 */
			VSYNC_WR_MPEG_REG(
				VD1_BLEND_SRC_CTRL + cur_dev->vpp_off,
				set_value);

			set_value = 0;
			if (vpp_misc_set & VPP_VD2_POSTBLEND)
				set_value =
					((1 << 20) |
					(1 << 16) | /* post bld premult*/
					(2 << 8)); /* post src */
			else if (vpp_misc_set & VPP_VD2_PREBLEND)
				set_value =
					((1 << 4) | /* pre bld premult*/
					(2 << 0)); /* pre bld src 1 */
			VSYNC_WR_MPEG_REG(
				VD2_BLEND_SRC_CTRL + cur_dev->vpp_off,
				set_value);
			set_value = vpp_misc_set;
			set_value &=
				((1 << 29) | VPP_CM_ENABLE |
				(0x1ff << VPP_VD2_ALPHA_BIT) |
				VPP_VD2_PREBLEND |
				VPP_VD1_PREBLEND |
				VPP_VD2_POSTBLEND |
				VPP_VD1_POSTBLEND |
				7);
			if ((vpp_misc_set & VPP_VD2_PREBLEND)
				&& (vpp_misc_set & VPP_VD1_PREBLEND))
				set_value |= VPP_PREBLEND_EN;
			if (bypass_cm)
				set_value &= ~VPP_CM_ENABLE;
			set_value |= VPP_POSTBLEND_EN;
			VSYNC_WR_MPEG_REG(
				VPP_MISC + cur_dev->vpp_off,
				set_value);
		}
	} else if (vpp_misc_save != vpp_misc_set)
		VSYNC_WR_MPEG_REG(
			VPP_MISC + cur_dev->vpp_off,
			vpp_misc_set);

	/*vpp_misc_set maybe have same,but need off.*/
	/* if vd1 off, disable vd2 also */
	if (video2_off_req || video1_off_req) {
		if ((debug_flag & DEBUG_FLAG_BLACKOUT)
			&& video2_off_req)
			pr_info("VD2 AFBC off now.\n");
		VSYNC_WR_MPEG_REG(VD2_AFBC_ENABLE, 0);
		VSYNC_WR_MPEG_REG(
			VD2_IF0_GEN_REG + cur_dev->viu_off, 0);
		if (!legacy_vpp) {
			VSYNC_WR_MPEG_REG(
				VD2_BLEND_SRC_CTRL + cur_dev->vpp_off, 0);
		}
		last_el_w = 0;
		last_el_status = 0;
		if (cur_dispbuf2 && (cur_dispbuf2 == &vf_local2))
			cur_dispbuf2 = NULL;
		need_disable_vd2 = false;
	}
	if (video1_off_req) {
		/*
		 * video layer off, swith off afbc,
		 * will enabled on new frame coming.
		 */
		if (debug_flag & DEBUG_FLAG_BLACKOUT)
			pr_info("AFBC off now.\n");
		VSYNC_WR_MPEG_REG(AFBC_ENABLE, 0);
		VSYNC_WR_MPEG_REG(
			VD1_IF0_GEN_REG + cur_dev->viu_off, 0);
		if (!legacy_vpp) {
			VSYNC_WR_MPEG_REG(
				VD1_BLEND_SRC_CTRL + cur_dev->vpp_off, 0);
		}
		if (is_dolby_vision_enable()) {
			if (is_meson_txlx_stbmode() ||
				is_meson_gxm())
				VSYNC_WR_MPEG_REG_BITS(
					VIU_MISC_CTRL1,
					1, 16, 1); /* bypass core1 */
			else if (is_meson_g12a_cpu()
				|| is_meson_g12b_cpu())
				VSYNC_WR_MPEG_REG_BITS(
					DOLBY_PATH_CTRL, 1, 0, 1);
		}
		if (cur_dispbuf && (cur_dispbuf == &vf_local))
			cur_dispbuf = NULL;
		}
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	cur_rdma_buf = cur_dispbuf;
	/* vsync_rdma_config(); */
	vsync_rdma_process();
	if (debug_flag & DEBUG_FLAG_PRINT_RDMA) {
		if (enable_rdma_log_count == 0)
			enable_rdma_log(0);
	}
	rdma_enable_pre = is_vsync_rdma_enable();
#endif

	if (timer_count > 50) {
		timer_count = 0;
		video_notify_flag |= VIDEO_NOTIFY_FRAME_WAIT;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
		if ((video_scaler_mode) && (scaler_pos_changed)) {
			video_notify_flag |= VIDEO_NOTIFY_POS_CHANGED;
			scaler_pos_changed = 0;
		} else {
			scaler_pos_changed = 0;
			video_notify_flag &= ~VIDEO_NOTIFY_POS_CHANGED;
		}
#endif
	}

	switch (READ_VCBUS_REG(VPU_VIU_VENC_MUX_CTRL) & 0x3) {
	case 0:
		enc_line = (READ_VCBUS_REG(ENCL_INFO_READ) >> 16) & 0x1fff;
		break;
	case 1:
		enc_line = (READ_VCBUS_REG(ENCI_INFO_READ) >> 16) & 0x1fff;
		break;
	case 2:
		enc_line = (READ_VCBUS_REG(ENCP_INFO_READ) >> 16) & 0x1fff;
		break;
	case 3:
		enc_line = (READ_VCBUS_REG(ENCT_INFO_READ) >> 16) & 0x1fff;
		break;
	}
	if (enc_line > vsync_exit_line_max)
		vsync_exit_line_max = enc_line;

#ifdef FIQ_VSYNC
	if (video_notify_flag)
		fiq_bridge_pulse_trigger(&vsync_fiq_bridge);
#else
	if (video_notify_flag)
		vsync_notify();

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	if ((get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) && !is_meson_mtvd_cpu()) {
		if (vpu_delay_work_flag)
			schedule_work(&vpu_delay_work);
	}
	/* #endif */

	return IRQ_HANDLED;
#endif

}


#ifdef FIQ_VSYNC
void vsync_fisr(void)
{
	atomic_set(&video_inirq_flag, 1);
	vsync_fisr_in();
	atomic_set(&video_inirq_flag, 0);
}
#else
static irqreturn_t vsync_isr(int irq, void *dev_id)
{
	irqreturn_t ret;

	atomic_set(&video_inirq_flag, 1);
	ret = vsync_isr_in(irq, dev_id);
	atomic_set(&video_inirq_flag, 0);
	return ret;
}
#endif


/*********************************************************
 * FIQ Routines
 *********************************************************/

static void vsync_fiq_up(void)
{
#ifdef FIQ_VSYNC
	request_fiq(INT_VIU_VSYNC, &vsync_fisr);
#else
	int r;
	/*TODO irq     */
	r = request_irq(video_vsync, &vsync_isr,
		IRQF_SHARED, "vsync", (void *)video_dev_id);

#ifdef CONFIG_MESON_TRUSTZONE
	if (num_online_cpus() > 1)
		irq_set_affinity(INT_VIU_VSYNC, cpumask_of(1));
#endif
#endif
}

static void vsync_fiq_down(void)
{
#ifdef FIQ_VSYNC
	free_fiq(INT_VIU_VSYNC, &vsync_fisr);
#else
	/*TODO irq */
	free_irq(video_vsync, (void *)video_dev_id);
#endif
}

#ifdef CONFIG_SUPPORT_VIDEO_ON_VPP2
static void vsync2_fiq_up(void)
{
	int r;

	r = request_irq(INT_VIU2_VSYNC, &vsync_isr,
			IRQF_SHARED, "vsync", (void *)video_dev_id2);
}

static void vsync2_fiq_down(void)
{
	free_irq(INT_VIU2_VSYNC, (void *)video_dev_id2);
}

#endif

int get_curren_frame_para(int *top, int *left, int *bottom, int *right)
{
	if (!cur_frame_par)
		return -1;
	*top = cur_frame_par->VPP_vd_start_lines_;
	*left = cur_frame_par->VPP_hd_start_lines_;
	*bottom = cur_frame_par->VPP_vd_end_lines_;
	*right = cur_frame_par->VPP_hd_end_lines_;
	return 0;
}

int get_current_vscale_skip_count(struct vframe_s *vf)
{
	int ret = 0;
	static struct vpp_frame_par_s frame_par;

	vpp_set_filters(process_3d_type, wide_setting, vf, &frame_par, vinfo,
		(is_dolby_vision_on() &&
		is_dolby_vision_stb_mode()));
	ret = frame_par.vscale_skip_count;
	if (cur_frame_par && (process_3d_type & MODE_3D_ENABLE))
		ret |= (cur_frame_par->vpp_3d_mode<<8);
	return ret;
}

int query_video_status(int type, int *value)
{
	if (value == NULL)
		return -1;
	switch (type) {
	case 0:
		*value = trickmode_fffb;
		break;
	case 1:
		*value = trickmode_i;
		break;
	default:
		break;
	}
	return 0;
}
EXPORT_SYMBOL(query_video_status);

static void video_vf_unreg_provider(void)
{
	ulong flags;
	struct vframe_s *el_vf = NULL;
	int keeped = 0;
	new_frame_count = 0;
	first_frame_toggled = 0;
	videopeek = 0;

	atomic_set(&video_unreg_flag, 1);
	while (atomic_read(&video_inirq_flag) > 0)
		schedule();
	spin_lock_irqsave(&lock, flags);

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	dispbuf_to_put_num = DISPBUF_TO_PUT_MAX;
	while (dispbuf_to_put_num > 0) {
		dispbuf_to_put_num--;
		dispbuf_to_put[dispbuf_to_put_num] = NULL;
	}
	cur_rdma_buf = NULL;
#endif
	if (cur_dispbuf) {
		vf_local = *cur_dispbuf;
		cur_dispbuf = &vf_local;
		cur_dispbuf->video_angle = 0;
	}
	if (cur_dispbuf2)
		need_disable_vd2 = true;
	if (is_dolby_vision_enable()) {
		if (cur_dispbuf2 == &vf_local2)
			cur_dispbuf2 = NULL;
		else if (cur_dispbuf2 != NULL) {
			vf_local2 = *cur_dispbuf2;
			el_vf = &vf_local2;
		}
		cur_dispbuf2 = NULL;
	}
	if (trickmode_fffb) {
		atomic_set(&trickmode_framedone, 0);
		to_notify_trick_wait = false;
	}

	vsync_pts_100 = 0;
	vsync_pts_112 = 0;
	vsync_pts_125 = 0;
	vsync_freerun = 0;
	vsync_pts_align = 0;
	vsync_pts_aligned = false;
	video_prot.video_started = 0;
	spin_unlock_irqrestore(&lock, flags);

	if (blackout | force_blackout) {
		safe_disble_videolayer();
		try_free_keep_video(1);
	}

#ifdef CONFIG_GE2D_KEEP_FRAME
	if (cur_dispbuf) {
		/* TODO: mod gate */
		/* switch_mod_gate_by_name("ge2d", 1); */
		keeped = vf_keep_current(cur_dispbuf, el_vf);
		/* TODO: mod gate */
		/* switch_mod_gate_by_name("ge2d", 0); */
	}
	if ((hdmi_in_onvideo == 0) && (video_start_post)) {
		tsync_avevent(VIDEO_STOP, 0);
		video_start_post = false;
	}
#else
	/* if (!trickmode_fffb) */
	if (cur_dispbuf)
		keeped = vf_keep_current(cur_dispbuf, el_vf);
	if ((hdmi_in_onvideo == 0) && (video_start_post)) {
		tsync_avevent(VIDEO_STOP, 0);
		video_start_post = false;
	}
#endif
	if (keeped < 0) {/*keep failed.*/
		pr_info("video keep failed, disable video now!\n");
		safe_disble_videolayer();
		try_free_keep_video(1);
	}
	atomic_set(&video_unreg_flag, 0);
	pr_info("VD1 AFBC 0x%x.\n", READ_VCBUS_REG(AFBC_ENABLE));
	enable_video_discontinue_report = 1;
	show_first_picture = false;
	show_first_frame_nosync = false;

#ifdef PTS_LOGGING
	{
		int pattern;
		/* Print what we have right now*/
		if (pts_pattern_detected >= PTS_32_PATTERN &&
			pts_pattern_detected < PTS_MAX_NUM_PATTERNS) {
			pr_info("pattern detected = %d, pts_enter_pattern_cnt =%d, pts_exit_pattern_cnt =%d",
				pts_pattern_detected,
				pts_pattern_enter_cnt[pts_pattern_detected],
				pts_pattern_exit_cnt[pts_pattern_detected]);
		}
		/* Reset all metrics now*/
		for (pattern = 0; pattern < PTS_MAX_NUM_PATTERNS; pattern++) {
			pts_pattern[pattern] = 0;
			pts_pattern_exit_cnt[pattern] = 0;
			pts_pattern_enter_cnt[pattern] = 0;
		}
		/* Reset 4:1 data*/
		memset(&pts_41_pattern_sink[0], 0, PTS_41_PATTERN_SINK_MAX);
		pts_pattern_detected = -1;
		pre_pts_trace = 0;
	}
#endif
}

static void video_vf_light_unreg_provider(int need_keep_frame)
{
	ulong flags;

	if (need_keep_frame) {
		/* wait for the end of the last toggled frame*/
		atomic_set(&video_unreg_flag, 1);
		while (atomic_read(&video_inirq_flag) > 0)
			schedule();
	}

	spin_lock_irqsave(&lock, flags);
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	dispbuf_to_put_num = DISPBUF_TO_PUT_MAX;
	while (dispbuf_to_put_num > 0) {
		dispbuf_to_put_num--;
		dispbuf_to_put[dispbuf_to_put_num] = NULL;
	}
	cur_rdma_buf = NULL;
#endif

	if (cur_dispbuf) {
		vf_local = *cur_dispbuf;
		cur_dispbuf = &vf_local;
	}
	spin_unlock_irqrestore(&lock, flags);

	if (need_keep_frame) {
		/* keep the last toggled frame*/
		if (cur_dispbuf) {
			unsigned int result;

			result = vf_keep_current(cur_dispbuf, NULL);
			if (result == 0)
				pr_info("%s: keep cur_disbuf failed\n",
					__func__);
		}
		atomic_set(&video_unreg_flag, 0);
	}
}

static int  get_display_info(void *data)
{
	s32 w, h, x, y;
	struct vdisplay_info_s  *info_para = (struct vdisplay_info_s *)data;
	const struct vinfo_s *info = get_current_vinfo();

	if ((!cur_frame_par) || (!info))
		return -1;
	vpp_get_video_layer_position(&x, &y, &w, &h);
	if ((w == 0) || (w  > info->width))
		w =  info->width;
	if ((h == 0) || (h  > info->height))
		h =  info->height;

	info_para->frame_hd_start_lines_ = cur_frame_par->VPP_hd_start_lines_;
	info_para->frame_hd_end_lines_ = cur_frame_par->VPP_hd_end_lines_;
	info_para->frame_vd_start_lines_ = cur_frame_par->VPP_vd_start_lines_;
	info_para->frame_vd_end_lines_ = cur_frame_par->VPP_vd_end_lines_;
	info_para->display_hsc_startp = cur_frame_par->VPP_hsc_startp - x;
	info_para->display_hsc_endp =
	cur_frame_par->VPP_hsc_endp + (info->width - x - w);
	info_para->display_vsc_startp = cur_frame_par->VPP_vsc_startp - y;
	info_para->display_vsc_endp =
	cur_frame_par->VPP_vsc_endp + (info->height - y - h);
	info_para->screen_vd_h_start_ =
	cur_frame_par->VPP_post_blend_vd_h_start_;
	info_para->screen_vd_h_end_ =
	cur_frame_par->VPP_post_blend_vd_h_end_;
	info_para->screen_vd_v_start_ =
	cur_frame_par->VPP_post_blend_vd_v_start_;
	info_para->screen_vd_v_end_ = cur_frame_par->VPP_post_blend_vd_v_end_;

	return 0;
}
#if ENABLE_UPDATE_HDR_FROM_USER
void init_hdr_info(void)
{
	unsigned long flags;

	spin_lock_irqsave(&omx_hdr_lock, flags);

	has_hdr_info = false;
	memset(&vf_hdr, 0, sizeof(vf_hdr));

	spin_unlock_irqrestore(&omx_hdr_lock, flags);
}
#endif
static int video_receiver_event_fun(int type, void *data, void *private_data)
{
#ifdef CONFIG_AM_VIDEO2
	char *provider_name;
#endif
	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		video_vf_unreg_provider();
#ifdef CONFIG_AM_VIDEO2
		set_clone_frame_rate(android_clone_rate, 200);
#endif
		drop_frame_count = 0;
		receive_frame_count = 0;
		display_frame_count = 0;
		//init_hdr_info();

	} else if (type == VFRAME_EVENT_PROVIDER_RESET) {
		video_vf_light_unreg_provider(1);
	} else if (type == VFRAME_EVENT_PROVIDER_LIGHT_UNREG)
		video_vf_light_unreg_provider(0);
	else if (type == VFRAME_EVENT_PROVIDER_REG) {
		enable_video_discontinue_report = 1;
		drop_frame_count = 0;
		receive_frame_count = 0;
		display_frame_count = 0;
		omx_run = false;
		omx_pts_set_from_hwc_count = 0;
		omx_check_previous_session = true;
		omx_need_drop_frame_num = 0;
		omx_drop_done = false;
		omx_pts_set_index = 0;
		//init_hdr_info();

#ifdef CONFIG_AM_VIDEO2
		provider_name = (char *)data;
		if (strncmp(provider_name, "decoder", 7) == 0
		    || strncmp(provider_name, "ppmgr", 5) == 0
		    || strncmp(provider_name, "deinterlace", 11) == 0
		    || strncmp(provider_name, "d2d3", 11) == 0) {
			set_clone_frame_rate(noneseamless_play_clone_rate, 0);
			set_clone_frame_rate(video_play_clone_rate, 100);
		}
#endif
/*notify di 3d mode is frame*/
/*alternative mode,passing two buffer in one frame */
		if (platform_type == 1) {
			if ((process_3d_type & MODE_3D_FA) &&
			!cur_dispbuf->trans_fmt)
				vf_notify_receiver_by_name("deinterlace",
			VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE,
				(void *)1);
		}

		video_vf_light_unreg_provider(0);
	} else if (type == VFRAME_EVENT_PROVIDER_FORCE_BLACKOUT) {
		force_blackout = 1;
		if (debug_flag & DEBUG_FLAG_BLACKOUT) {
			pr_info("%s VFRAME_EVENT_PROVIDER_FORCE_BLACKOUT\n",
			       __func__);
		}
	} else if (type == VFRAME_EVENT_PROVIDER_FR_HINT) {
#ifdef CONFIG_AM_VOUT
		if ((data != NULL) && (video_seek_flag == 0))
			set_vframe_rate_hint((unsigned long)data);
#endif
	} else if (type == VFRAME_EVENT_PROVIDER_FR_END_HINT) {
#ifdef CONFIG_AM_VOUT
		if (video_seek_flag == 0)
			set_vframe_rate_end_hint();
#endif
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_DISPLAY_INFO) {
		get_display_info(data);
	}
	return 0;
}

static int video4osd_receiver_event_fun(int type, void *data,
					void *private_data)
{
	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		osd_prov = NULL;
		if (debug_flag & DEBUG_FLAG_BLACKOUT)
			pr_info("[video4osd] clear osd_prov\n");
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		osd_prov = vf_get_provider(RECEIVER4OSD_NAME);

		if (debug_flag & DEBUG_FLAG_BLACKOUT)
			pr_info("[video4osd] set osd_prov\n");
	}
	return 0;
}

unsigned int get_post_canvas(void)
{
	return post_canvas;
}
EXPORT_SYMBOL(get_post_canvas);


u32 get_blackout_policy(void)
{
	return blackout | force_blackout;
}
EXPORT_SYMBOL(get_blackout_policy);

u32 set_blackout_policy(int policy)
{
	blackout = policy;
	return 0;
}
EXPORT_SYMBOL(set_blackout_policy);

u8 is_vpp_postblend(void)
{
	if (READ_VCBUS_REG(VPP_MISC + cur_dev->vpp_off) & VPP_VD1_POSTBLEND)
		return 1;
	return 0;
}
EXPORT_SYMBOL(is_vpp_postblend);

void pause_video(unsigned char pause_flag)
{
	atomic_set(&video_pause_flag, pause_flag ? 1 : 0);
}
EXPORT_SYMBOL(pause_video);
/*********************************************************
 * Utilities
 *********************************************************/
int _video_set_disable(u32 val)
{
	if (val > VIDEO_DISABLE_FORNEXT)
		return -EINVAL;

	disable_video = val;

	if (disable_video != VIDEO_DISABLE_NONE) {
		safe_disble_videolayer();

		if ((disable_video == VIDEO_DISABLE_FORNEXT) && cur_dispbuf
		    && (cur_dispbuf != &vf_local))
			video_property_changed = true;
		try_free_keep_video(0);
	} else {
		if (cur_dispbuf && (cur_dispbuf != &vf_local)) {
			EnableVideoLayer();
			video_property_changed = true;
		}
	}

	return 0;
}

static void _set_video_crop(int *p)
{
	int last_l, last_r, last_t, last_b;
	int new_l, new_r, new_t, new_b;

	vpp_get_video_source_crop(&last_t, &last_l, &last_b, &last_r);
	vpp_set_video_source_crop(p[0], p[1], p[2], p[3]);
	vpp_get_video_source_crop(&new_t, &new_l, &new_b, &new_r);
	if ((new_t != last_t) || (new_l != last_l)
	|| (new_b != last_b) || (new_r != last_r)) {
		video_property_changed = true;
	}
}

static void _set_video_window(int *p)
{
	int w, h;
	int *parsed = p;
	int last_x, last_y, last_w, last_h;
	int new_x, new_y, new_w, new_h;
#ifdef TV_REVERSE
	int temp, temp1;
	const struct vinfo_s *info = get_current_vinfo();

	/* pr_info(KERN_DEBUG "%s: %u*/
	/*get vinfo(%d,%d).\n", __func__, __LINE__,*/
	/*info->width, info->height); */
	if (reverse) {
		temp = parsed[0];
		temp1 = parsed[1];
		parsed[0] = info->width - parsed[2] - 1;
		parsed[1] = info->height - parsed[3] - 1;
		parsed[2] = info->width - temp - 1;
		parsed[3] = info->height - temp1 - 1;
	}
#endif
	vpp_get_video_layer_position(&last_x, &last_y, &last_w, &last_h);
	if (parsed[0] < 0 && parsed[2] < 2) {
		parsed[2] = 2;
		parsed[0] = 0;
	}
	if (parsed[1] < 0 && parsed[3] < 2) {
		parsed[3] = 2;
		parsed[1] = 0;
	}
	w = parsed[2] - parsed[0] + 1;
	h = parsed[3] - parsed[1] + 1;

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
	if (video_scaler_mode) {
		if ((w == 1) && (h == 1)) {
			w = 0;
			h = 0;
		}
		if ((content_left != parsed[0]) || (content_top != parsed[1])
		    || (content_w != w) || (content_h != h))
			scaler_pos_changed = 1;
		content_left = parsed[0];
		content_top = parsed[1];
		content_w = w;
		content_h = h;
		/* video_notify_flag =*/
		/*video_notify_flag|VIDEO_NOTIFY_POS_CHANGED; */
	} else
#endif
	{
		if ((w == 1) && (h == 1)) {
			w = h = 0;
			vpp_set_video_layer_position(parsed[0], parsed[1], 0,
						     0);
		} else if ((w > 0) && (h > 0)) {
			vpp_set_video_layer_position(parsed[0], parsed[1], w,
						     h);
		}
	}
	vpp_get_video_layer_position(&new_x, &new_y, &new_w, &new_h);
	if ((last_x != new_x) || (last_y != new_y)
	|| (last_w != new_w) || (last_h != new_h)) {
		video_property_changed = true;
	}
}
#if ENABLE_UPDATE_HDR_FROM_USER
static void config_hdr_info(const struct vframe_master_display_colour_s p)
{
	struct vframe_master_display_colour_s tmp = {0};
	bool valid_hdr = false;
	unsigned long flags;

	tmp.present_flag = p.present_flag;
	if (tmp.present_flag == 1) {
		tmp = p;

		if (tmp.primaries[0][0] == 0 &&
			tmp.primaries[0][1] == 0 &&
			tmp.primaries[1][0] == 0 &&
			tmp.primaries[1][1] == 0 &&
			tmp.primaries[2][0] == 0 &&
			tmp.primaries[2][1] == 0 &&
			tmp.white_point[0] == 0 &&
			tmp.white_point[1] == 0 &&
			tmp.luminance[0] == 0 &&
			tmp.luminance[1] == 0 &&
			tmp.content_light_level.max_content == 0 &&
			tmp.content_light_level.max_pic_average == 0) {
			valid_hdr = false;
		} else {
			valid_hdr = true;
		}
	}

	spin_lock_irqsave(&omx_hdr_lock, flags);
	vf_hdr = tmp;
	has_hdr_info = valid_hdr;
	spin_unlock_irqrestore(&omx_hdr_lock, flags);

	pr_debug("has_hdr_info %d\n", has_hdr_info);
}
#endif
static void set_omx_pts(u32 *p)
{
	u32 tmp_pts = p[0];
	/*u32 vision = p[1];*/
	u32 set_from_hwc = p[2];
	u32 frame_num = p[3];
	u32 not_reset = p[4];
	u32 session = p[5];
	unsigned int try_cnt = 0x1000;

	mutex_lock(&omx_mutex);
	if (omx_pts_set_index < frame_num)
		omx_pts_set_index = frame_num;

	if (omx_check_previous_session) {
		if (session != omx_cur_session) {
			omx_cur_session = session;
			omx_check_previous_session = false;
		} else {
			mutex_unlock(&omx_mutex);
			pr_info("check session return: tmp_pts %d"
				"session=0x%x\n", tmp_pts, omx_cur_session);
			return;
		}
	}

	if (not_reset == 0)
		omx_pts = tmp_pts;
	/* kodi may render first frame, then drop dozens of frames */
	if (set_from_hwc == 0 && omx_run == true && frame_num <= 2
			&& not_reset == 0) {
		pr_info("reset omx_run to false.\n");
		omx_run = false;
	}
	if (set_from_hwc == 1) {
		if (!omx_run) {
			omx_need_drop_frame_num =
				frame_num > 0 ? frame_num-1 : 0;
			if (omx_need_drop_frame_num == 0)
				omx_drop_done = true;
			pr_info("omx_need_drop_frame_num %d\n",
			omx_need_drop_frame_num);
		}
		omx_run = true;
		if (omx_pts_set_from_hwc_count < OMX_MAX_COUNT_RESET_SYSTEMTIME)
			omx_pts_set_from_hwc_count++;

	} else if (set_from_hwc == 0 && !omx_run) {
		struct vframe_s *vf = NULL;
		u32 donot_drop = 0;

		while (try_cnt--) {
			vf = vf_peek(RECEIVER_NAME);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if (is_dolby_vision_enable()
				&& vf && is_dovi_frame(vf)) {
				pr_info("set_omx_pts ignore the omx %d frames drop for dv frame\n",
					frame_num);
				donot_drop = 1;
				break;
			}
#endif
			if (vf) {
				pr_debug("drop frame_num=%d, vf->omx_index=%d\n",
						frame_num, vf->omx_index);
				if (frame_num >= vf->omx_index) {
					vf = vf_get(RECEIVER_NAME);
					if (vf)
						vf_put(vf, RECEIVER_NAME);
				} else
					break;
			} else
				break;
		}
		if (donot_drop && omx_pts_set_from_hwc_count > 0) {
			pr_info("reset omx_run to true.\n");
			omx_run = true;
		}
	}
	mutex_unlock(&omx_mutex);
}


/*********************************************************
 * /dev/amvideo APIs
 ********************************************************
 */
static int amvideo_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int amvideo_poll_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int amvideo_release(struct inode *inode, struct file *file)
{
	if (blackout | force_blackout) {
		/*	DisableVideoLayer();*/
		/*don't need it ,it have problem on  pure music playing */
	}
	return 0;
}

static int amvideo_poll_release(struct inode *inode, struct file *file)
{
	if (blackout | force_blackout) {
		/*	DisableVideoLayer();*/
		/*don't need it ,it have problem on  pure music playing */
	}
	return 0;
}

static long amvideo_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case AMSTREAM_IOC_SET_HDR_INFO:{
#if ENABLE_UPDATE_HDR_FROM_USER
			struct vframe_master_display_colour_s tmp;
			if (copy_from_user(&tmp, argp, sizeof(tmp)) == 0)
				config_hdr_info(tmp);
#endif
		}
		break;
	case AMSTREAM_IOC_SET_OMX_VPTS:{
			u32 pts[6];
			if (copy_from_user(pts, argp, sizeof(pts)) == 0)
				set_omx_pts(pts);
		}
		break;

	case AMSTREAM_IOC_GET_OMX_VPTS:
		put_user(omx_pts, (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_GET_OMX_VERSION:
		put_user(omx_version, (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_GET_OMX_INFO:
		put_user(omx_info, (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_TRICKMODE:
		if (arg == TRICKMODE_I)
			trickmode_i = 1;
		else if (arg == TRICKMODE_FFFB)
			trickmode_fffb = 1;
		else {
			trickmode_i = 0;
			trickmode_fffb = 0;
		}
		to_notify_trick_wait = false;
		atomic_set(&trickmode_framedone, 0);
		tsync_trick_mode(trickmode_fffb);
		break;

	case AMSTREAM_IOC_TRICK_STAT:
		put_user(atomic_read(&trickmode_framedone),
			 (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_GET_TRICK_VPTS:
		put_user(trickmode_vpts, (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_VPAUSE:
		tsync_avevent(VIDEO_PAUSE, arg);
		break;

	case AMSTREAM_IOC_AVTHRESH:
		tsync_set_avthresh(arg);
		break;

	case AMSTREAM_IOC_SYNCTHRESH:
		tsync_set_syncthresh(arg);
		break;

	case AMSTREAM_IOC_SYNCENABLE:
		tsync_set_enable(arg);
		break;

	case AMSTREAM_IOC_SET_SYNC_ADISCON:
		tsync_set_sync_adiscont(arg);
		break;

	case AMSTREAM_IOC_SET_SYNC_VDISCON:
		tsync_set_sync_vdiscont(arg);
		break;

	case AMSTREAM_IOC_GET_SYNC_ADISCON:
		put_user(tsync_get_sync_adiscont(), (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_GET_SYNC_VDISCON:
		put_user(tsync_get_sync_vdiscont(), (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_GET_SYNC_ADISCON_DIFF:
		put_user(tsync_get_sync_adiscont_diff(), (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_GET_SYNC_VDISCON_DIFF:
		put_user(tsync_get_sync_vdiscont_diff(), (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_SET_SYNC_ADISCON_DIFF:
		tsync_set_sync_adiscont_diff(arg);
		break;

	case AMSTREAM_IOC_SET_SYNC_VDISCON_DIFF:
		tsync_set_sync_vdiscont_diff(arg);
		break;

	case AMSTREAM_IOC_VF_STATUS:{
			struct vframe_states vfsta;
			struct vframe_states states;

			video_vf_get_states(&vfsta);
			states.vf_pool_size = vfsta.vf_pool_size;
			states.buf_avail_num = vfsta.buf_avail_num;
			states.buf_free_num = vfsta.buf_free_num;
			states.buf_recycle_num = vfsta.buf_recycle_num;
			if (copy_to_user(argp, &states, sizeof(states)))
				ret = -EFAULT;
		}
		break;

	case AMSTREAM_IOC_GET_VIDEO_DISABLE:
		put_user(disable_video, (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_SET_VIDEO_DISABLE:{
		u32 val;

		if (copy_from_user(&val, argp, sizeof(u32)) == 0)
			ret = _video_set_disable(val);
		else
			ret = -EFAULT;
		break;
	}

	case AMSTREAM_IOC_GET_VIDEO_DISCONTINUE_REPORT:
		put_user(enable_video_discontinue_report, (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_SET_VIDEO_DISCONTINUE_REPORT:
		enable_video_discontinue_report = (arg == 0) ? 0 : 1;
		break;

	case AMSTREAM_IOC_GET_VIDEO_AXIS:{
			int axis[4];
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
			if (video_scaler_mode) {
				axis[0] = content_left;
				axis[1] = content_top;
				axis[2] = content_w;
				axis[3] = content_h;
			} else
#endif
			{
				vpp_get_video_layer_position(
					&axis[0], &axis[1],
					&axis[2],
					&axis[3]);
			}

			axis[2] = axis[0] + axis[2] - 1;
			axis[3] = axis[1] + axis[3] - 1;

			if (copy_to_user(argp, &axis[0], sizeof(axis)) != 0)
				ret = -EFAULT;
		}
		break;

	case AMSTREAM_IOC_SET_VIDEO_AXIS:{
			int axis[4];

			if (copy_from_user(axis, argp, sizeof(axis)) == 0)
				_set_video_window(axis);
			else
				ret = -EFAULT;
		}
		break;

	case AMSTREAM_IOC_GET_VIDEO_CROP:{
			int crop[4];
			{
				vpp_get_video_source_crop(&crop[0], &crop[1],
							  &crop[2], &crop[3]);
			}

			if (copy_to_user(argp, &crop[0], sizeof(crop)) != 0)
				ret = -EFAULT;
		}
		break;

	case AMSTREAM_IOC_SET_VIDEO_CROP:{
			int crop[4];

			if (copy_from_user(crop, argp, sizeof(crop)) == 0)
				_set_video_crop(crop);
			else
				ret = -EFAULT;
		}
		break;

	case AMSTREAM_IOC_GET_SCREEN_MODE:
		if (copy_to_user(argp, &wide_setting, sizeof(u32)) != 0)
			ret = -EFAULT;
		break;

	case AMSTREAM_IOC_SET_SCREEN_MODE:{
			u32 mode;

			if (copy_from_user(&mode, argp, sizeof(u32)) == 0) {
				if (mode >= VIDEO_WIDEOPTION_MAX)
					ret = -EINVAL;
				else if (mode != wide_setting) {
					wide_setting = mode;
					video_property_changed = true;
				}
			} else
				ret = -EFAULT;
		}
		break;

	case AMSTREAM_IOC_GET_BLACKOUT_POLICY:
		if (copy_to_user(argp, &blackout, sizeof(u32)) != 0)
			ret = -EFAULT;
		break;

	case AMSTREAM_IOC_SET_BLACKOUT_POLICY:{
			u32 mode;

			if (copy_from_user(&mode, argp, sizeof(u32)) == 0) {
				if (mode > 2)
					ret = -EINVAL;
				else
					blackout = mode;
			} else
				ret = -EFAULT;
		}
		break;

	case AMSTREAM_IOC_CLEAR_VBUF:{
			unsigned long flags;
			while (atomic_read(&video_inirq_flag) > 0 ||
				atomic_read(&video_unreg_flag) > 0)
				schedule();
			spin_lock_irqsave(&lock, flags);
			cur_dispbuf = NULL;
			spin_unlock_irqrestore(&lock, flags);
		}
		break;

	case AMSTREAM_IOC_CLEAR_VIDEO:
		if (blackout)
			safe_disble_videolayer();
		break;

	case AMSTREAM_IOC_SET_FREERUN_MODE:
		if (arg > FREERUN_DUR)
			ret = -EFAULT;
		else
			freerun_mode = arg;
		break;

	case AMSTREAM_IOC_GET_FREERUN_MODE:
		put_user(freerun_mode, (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_DISABLE_SLOW_SYNC:
		if (arg)
			disable_slow_sync = 1;
		else
			disable_slow_sync = 0;
		break;
	/*
	 ***************************************************************
	 *3d process ioctl
	 ****************************************************************
	 */
	case AMSTREAM_IOC_SET_3D_TYPE:
		{
#ifdef TV_3D_FUNCTION_OPEN
			unsigned int set_3d =
				VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE;
			unsigned int type = (unsigned int)arg;

			if (type != process_3d_type) {
				process_3d_type = type;
				if (mvc_flag)
					process_3d_type |= MODE_3D_MVC;
				video_property_changed = true;
				if ((process_3d_type & MODE_3D_FA)
					&& cur_dispbuf
					&& !cur_dispbuf->trans_fmt)
					/*notify di 3d mode is frame*/
					  /*alternative mode,passing two*/
					  /*buffer in one frame */
					vf_notify_receiver_by_name(
							"deinterlace",
							set_3d,
							(void *)1);
				else
					vf_notify_receiver_by_name(
							"deinterlace",
							set_3d,
							(void *)0);
			}
#endif
			break;
		}
	case AMSTREAM_IOC_GET_3D_TYPE:
#ifdef TV_3D_FUNCTION_OPEN
		put_user(process_3d_type, (u32 __user *)argp);

#endif
		break;
	case AMSTREAM_IOC_GET_SOURCE_VIDEO_3D_TYPE:
#ifdef TV_3D_FUNCTION_OPEN
	{
		int source_video_3d_type = VPP_3D_MODE_NULL;

		if (!cur_frame_par)
			source_video_3d_type =
		VPP_3D_MODE_NULL;
		else
			get_vpp_3d_mode(process_3d_type,
			cur_frame_par->trans_fmt, &source_video_3d_type);
		put_user(source_video_3d_type, (u32 __user *)argp);
	}
#endif
		break;
	case AMSTREAM_IOC_SET_VSYNC_UPINT:
		vsync_pts_inc_upint = arg;
		break;

	case AMSTREAM_IOC_GET_VSYNC_SLOW_FACTOR:
		put_user(vsync_slow_factor, (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_SET_VSYNC_SLOW_FACTOR:
		vsync_slow_factor = arg;
		break;

	case AMSTREAM_IOC_GLOBAL_SET_VIDEO_OUTPUT:
		if (arg != 0)
			video_global_output = 1;
		else
			video_global_output = 0;
		break;

	case AMSTREAM_IOC_GLOBAL_GET_VIDEO_OUTPUT:
		put_user(video_global_output, (u32 __user *)argp);
		break;

	case AMSTREAM_IOC_GET_VIDEO_LAYER1_ON: {
			u32 vsync_duration;
			u32 video_onoff_diff = 0;

			vsync_duration = vsync_pts_inc / 90;
			video_onoff_diff =
				jiffies_to_msecs(jiffies) - video_onoff_time;

			if (video_onoff_state == VIDEO_ENABLE_STATE_IDLE) {
				/* wait until 5ms after next vsync */
				msleep(video_onoff_diff < vsync_duration
					? vsync_duration - video_onoff_diff + 5
					: 0);
			}
			put_user(video_onoff_state, (u32 __user *)argp);
			break;
		}

	case AMSTREAM_IOC_SET_VIDEOPEEK:
		videopeek = true;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long amvideo_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;

	switch (cmd) {
	case AMSTREAM_IOC_SET_HDR_INFO:
	case AMSTREAM_IOC_SET_OMX_VPTS:
	case AMSTREAM_IOC_GET_OMX_VPTS:
	case AMSTREAM_IOC_GET_OMX_VERSION:
	case AMSTREAM_IOC_GET_OMX_INFO:
	case AMSTREAM_IOC_TRICK_STAT:
	case AMSTREAM_IOC_GET_TRICK_VPTS:
	case AMSTREAM_IOC_GET_SYNC_ADISCON:
	case AMSTREAM_IOC_GET_SYNC_VDISCON:
	case AMSTREAM_IOC_GET_SYNC_ADISCON_DIFF:
	case AMSTREAM_IOC_GET_SYNC_VDISCON_DIFF:
	case AMSTREAM_IOC_VF_STATUS:
	case AMSTREAM_IOC_GET_VIDEO_DISABLE:
	case AMSTREAM_IOC_GET_VIDEO_DISCONTINUE_REPORT:
	case AMSTREAM_IOC_GET_VIDEO_AXIS:
	case AMSTREAM_IOC_SET_VIDEO_AXIS:
	case AMSTREAM_IOC_GET_VIDEO_CROP:
	case AMSTREAM_IOC_SET_VIDEO_CROP:
	case AMSTREAM_IOC_GET_SCREEN_MODE:
	case AMSTREAM_IOC_SET_SCREEN_MODE:
	case AMSTREAM_IOC_GET_BLACKOUT_POLICY:
	case AMSTREAM_IOC_SET_BLACKOUT_POLICY:
	case AMSTREAM_IOC_GET_FREERUN_MODE:
	case AMSTREAM_IOC_GET_3D_TYPE:
	case AMSTREAM_IOC_GET_SOURCE_VIDEO_3D_TYPE:
	case AMSTREAM_IOC_GET_VSYNC_SLOW_FACTOR:
	case AMSTREAM_IOC_GLOBAL_GET_VIDEO_OUTPUT:
	case AMSTREAM_IOC_GET_VIDEO_LAYER1_ON:
		arg = (unsigned long) compat_ptr(arg);
	case AMSTREAM_IOC_TRICKMODE:
	case AMSTREAM_IOC_VPAUSE:
	case AMSTREAM_IOC_AVTHRESH:
	case AMSTREAM_IOC_SYNCTHRESH:
	case AMSTREAM_IOC_SYNCENABLE:
	case AMSTREAM_IOC_SET_SYNC_ADISCON:
	case AMSTREAM_IOC_SET_SYNC_VDISCON:
	case AMSTREAM_IOC_SET_SYNC_ADISCON_DIFF:
	case AMSTREAM_IOC_SET_SYNC_VDISCON_DIFF:
	case AMSTREAM_IOC_SET_VIDEO_DISABLE:
	case AMSTREAM_IOC_SET_VIDEO_DISCONTINUE_REPORT:
	case AMSTREAM_IOC_CLEAR_VBUF:
	case AMSTREAM_IOC_CLEAR_VIDEO:
	case AMSTREAM_IOC_SET_FREERUN_MODE:
	case AMSTREAM_IOC_DISABLE_SLOW_SYNC:
	case AMSTREAM_IOC_SET_3D_TYPE:
	case AMSTREAM_IOC_SET_VSYNC_UPINT:
	case AMSTREAM_IOC_SET_VSYNC_SLOW_FACTOR:
	case AMSTREAM_IOC_GLOBAL_SET_VIDEO_OUTPUT:
	case AMSTREAM_IOC_SET_VIDEOPEEK:
		return amvideo_ioctl(file, cmd, arg);
	default:
		return -EINVAL;
	}

	return ret;
}
#endif

static unsigned int amvideo_poll(struct file *file, poll_table *wait_table)
{
	poll_wait(file, &amvideo_trick_wait, wait_table);

	if (atomic_read(&trickmode_framedone)) {
		atomic_set(&trickmode_framedone, 0);
		return POLLOUT | POLLWRNORM;
	}

	return 0;
}

static unsigned int amvideo_poll_poll(struct file *file, poll_table *wait_table)
{
	poll_wait(file, &amvideo_sizechange_wait, wait_table);

	if (atomic_read(&video_sizechange)) {
		atomic_set(&video_sizechange, 0);
		return POLLIN | POLLWRNORM;
	}

	return 0;
}

static const struct file_operations amvideo_fops = {
	.owner = THIS_MODULE,
	.open = amvideo_open,
	.release = amvideo_release,
	.unlocked_ioctl = amvideo_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amvideo_compat_ioctl,
#endif
	.poll = amvideo_poll,
};

static const struct file_operations amvideo_poll_fops = {
	.owner = THIS_MODULE,
	.open = amvideo_poll_open,
	.release = amvideo_poll_release,
	.poll = amvideo_poll_poll,
};

/*********************************************************
 * SYSFS property functions
 *********************************************************/
#define MAX_NUMBER_PARA 10
#define AMVIDEO_CLASS_NAME "video"
#define AMVIDEO_POLL_CLASS_NAME "video_poll"

static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	if (token) {
		len = strlen(token);
		do {
			token = strsep(&params, " ");
			if (!token)
				break;
			while (token && (isspace(*token)
					|| !isgraph(*token)) && len) {
				token++;
				len--;
			}
			if (len == 0)
				break;
			ret = kstrtoint(token, 0, &res);
			if (ret < 0)
				break;
			len = strlen(token);
			*out++ = res;
			count++;
		} while ((count < para_num) && (len > 0));
	}

	kfree(params_base);
	return count;
}

static void set_video_crop(const char *para)
{
	int parsed[4];

	if (likely(parse_para(para, 4, parsed) == 4))
		_set_video_crop(parsed);
	amlog_mask(LOG_MASK_SYSFS,
		   "video crop=>x0:%d,y0:%d,x1:%d,y1:%d\n ",
		   parsed[0], parsed[1], parsed[2], parsed[3]);
}

static void set_video_speed_check(const char *para)
{
	int parsed[2];

	if (likely(parse_para(para, 2, parsed) == 2))
		vpp_set_video_speed_check(parsed[0], parsed[1]);
	amlog_mask(LOG_MASK_SYSFS,
		   "video speed_check=>h:%d,w:%d\n ", parsed[0], parsed[1]);
}

static void set_video_window(const char *para)
{
	int parsed[4];

	if (likely(parse_para(para, 4, parsed) == 4))
		_set_video_window(parsed);
	amlog_mask(LOG_MASK_SYSFS,
		   "video=>x0:%d,y0:%d,x1:%d,y1:%d\n ",
		   parsed[0], parsed[1], parsed[2], parsed[3]);
}

static ssize_t video_3d_scale_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
#ifdef TV_3D_FUNCTION_OPEN
	u32 enable;
	int r;

	r = kstrtouint(buf, 0, &enable);
	if (r < 0)
		return -EINVAL;

	vpp_set_3d_scale(enable);
	video_property_changed = true;
	amlog_mask(LOG_MASK_SYSFS, "%s:%s 3d scale.\n", __func__,
		   enable ? "enable" : "disable");
#endif
	return count;
}

static ssize_t video_sr_show(struct class *cla, struct class_attribute *attr,
			       char *buf)
{
	return sprintf(buf, "super_scaler:%d\n", super_scaler);
}

static ssize_t video_sr_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	int parsed[1];

	mutex_lock(&video_module_mutex);
	if (likely(parse_para(buf, 1, parsed) == 1)) {
		if (super_scaler != (parsed[0] & 0x1)) {
			super_scaler = parsed[0] & 0x1;
			video_property_changed = true;
		}
	}
	mutex_unlock(&video_module_mutex);

	return strnlen(buf, count);
}

static ssize_t video_crop_show(struct class *cla, struct class_attribute *attr,
			       char *buf)
{
	u32 t, l, b, r;

	vpp_get_video_source_crop(&t, &l, &b, &r);
	return snprintf(buf, 40, "%d %d %d %d\n", t, l, b, r);
}

static ssize_t video_crop_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	mutex_lock(&video_module_mutex);

	set_video_crop(buf);

	mutex_unlock(&video_module_mutex);

	return strnlen(buf, count);
}

static ssize_t video_state_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	ssize_t len = 0;
	struct vppfilter_mode_s *vpp_filter = NULL;

	if (!cur_frame_par)
		return len;
	vpp_filter = &cur_frame_par->vpp_filter;
	len += sprintf(buf + len,
		"zoom_start_x_lines:%u.zoom_end_x_lines:%u.\n",
		zoom_start_x_lines, zoom_end_x_lines);
	len += sprintf(buf + len,
		"zoom_start_y_lines:%u.zoom_end_y_lines:%u.\n",
		    zoom_start_y_lines, zoom_end_y_lines);
	len += sprintf(buf + len, "frame parameters: pic_in_height %u.\n",
		    cur_frame_par->VPP_pic_in_height_);
	len += sprintf(buf + len,
		"frame parameters: VPP_line_in_length_ %u.\n",
		cur_frame_par->VPP_line_in_length_);
	len += sprintf(buf + len, "vscale_skip_count %u.\n",
		    cur_frame_par->vscale_skip_count);
	len += sprintf(buf + len, "hscale_skip_count %u.\n",
		    cur_frame_par->hscale_skip_count);
	len += sprintf(buf + len, "supscl_path %u.\n",
		    cur_frame_par->supscl_path);
	len += sprintf(buf + len, "supsc0_enable %u.\n",
		    cur_frame_par->supsc0_enable);
	len += sprintf(buf + len, "supsc1_enable %u.\n",
		    cur_frame_par->supsc1_enable);
	len += sprintf(buf + len, "supsc0_hori_ratio %u.\n",
		    cur_frame_par->supsc0_hori_ratio);
	len += sprintf(buf + len, "supsc1_hori_ratio %u.\n",
		    cur_frame_par->supsc1_hori_ratio);
	len += sprintf(buf + len, "supsc0_vert_ratio %u.\n",
		    cur_frame_par->supsc0_vert_ratio);
	len += sprintf(buf + len, "supsc1_vert_ratio %u.\n",
		    cur_frame_par->supsc1_vert_ratio);
	len += sprintf(buf + len, "spsc0_h_in %u.\n",
		    cur_frame_par->spsc0_h_in);
	len += sprintf(buf + len, "spsc1_h_in %u.\n",
		    cur_frame_par->spsc1_h_in);
	len += sprintf(buf + len, "spsc0_w_in %u.\n",
		    cur_frame_par->spsc0_w_in);
	len += sprintf(buf + len, "spsc1_w_in %u.\n",
		    cur_frame_par->spsc1_w_in);
	len += sprintf(buf + len, "video_input_w %u.\n",
		    cur_frame_par->video_input_w);
	len += sprintf(buf + len, "video_input_h %u.\n",
		    cur_frame_par->video_input_h);
	len += sprintf(buf + len, "clk_in_pps %u.\n",
			    cur_frame_par->clk_in_pps);
#ifdef TV_3D_FUNCTION_OPEN
	len += sprintf(buf + len, "vpp_2pic_mode %u.\n",
		    cur_frame_par->vpp_2pic_mode);
	len += sprintf(buf + len, "vpp_3d_scale %u.\n",
		    cur_frame_par->vpp_3d_scale);
	len += sprintf(buf + len,
		"vpp_3d_mode %u.\n", cur_frame_par->vpp_3d_mode);
#endif
	len +=
	    sprintf(buf + len, "hscale phase step 0x%x.\n",
		    vpp_filter->vpp_hsc_start_phase_step);
	len +=
	    sprintf(buf + len, "vscale phase step 0x%x.\n",
		    vpp_filter->vpp_vsc_start_phase_step);
	len +=
	    sprintf(buf + len, "pps pre hsc enable %d.\n",
		    vpp_filter->vpp_pre_hsc_en);
	len +=
	    sprintf(buf + len, "pps pre vsc enable %d.\n",
		    vpp_filter->vpp_pre_vsc_en);
	len +=
	    sprintf(buf + len, "hscale filter coef %d.\n",
		    vpp_filter->vpp_horz_filter);
	len +=
	    sprintf(buf + len, "vscale filter coef %d.\n",
		    vpp_filter->vpp_vert_filter);
	len +=
	    sprintf(buf + len, "vpp_vert_chroma_filter_en %d.\n",
		    vpp_filter->vpp_vert_chroma_filter_en);
	len +=
	    sprintf(buf + len, "post_blend_vd_h_start 0x%x.\n",
		    cur_frame_par->VPP_post_blend_vd_h_start_);
	len +=
	    sprintf(buf + len, "post_blend_vd_h_end 0x%x.\n",
		    cur_frame_par->VPP_post_blend_vd_h_end_);
	len +=
	    sprintf(buf + len, "post_blend_vd_v_start 0x%x.\n",
		    cur_frame_par->VPP_post_blend_vd_v_start_);
	len +=
	    sprintf(buf + len, "post_blend_vd_v_end 0x%x.\n",
		    cur_frame_par->VPP_post_blend_vd_v_end_);
	len +=
	    sprintf(buf + len, "VPP_hd_start_lines_ 0x%x.\n",
		    cur_frame_par->VPP_hd_start_lines_);
	len +=
	    sprintf(buf + len, "VPP_hd_end_lines_ 0x%x.\n",
		    cur_frame_par->VPP_hd_end_lines_);
	len +=
	    sprintf(buf + len, "VPP_vd_start_lines_ 0x%x.\n",
		    cur_frame_par->VPP_vd_start_lines_);
	len +=
	    sprintf(buf + len, "VPP_vd_end_lines_ 0x%x.\n",
		    cur_frame_par->VPP_vd_end_lines_);
	len +=
	    sprintf(buf + len, "VPP_hsc_startp 0x%x.\n",
		    cur_frame_par->VPP_hsc_startp);
	len +=
	    sprintf(buf + len, "VPP_hsc_endp 0x%x.\n",
		    cur_frame_par->VPP_hsc_endp);
	len +=
	    sprintf(buf + len, "VPP_vsc_startp 0x%x.\n",
		    cur_frame_par->VPP_vsc_startp);
	len +=
	    sprintf(buf + len, "VPP_vsc_endp 0x%x.\n",
		    cur_frame_par->VPP_vsc_endp);
	return len;
}

static ssize_t video_axis_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	int x, y, w, h;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
	if (video_scaler_mode) {
		x = content_left;
		y = content_top;
		w = content_w;
		h = content_h;
	} else
#endif
	{
		vpp_get_video_layer_position(&x, &y, &w, &h);
	}
	return snprintf(buf, 40, "%d %d %d %d\n", x, y, x + w - 1, y + h - 1);
}

static ssize_t video_axis_store(struct class *cla,
		struct class_attribute *attr,
			const char *buf, size_t count)
{
	mutex_lock(&video_module_mutex);

	set_video_window(buf);

	mutex_unlock(&video_module_mutex);

	return strnlen(buf, count);
}

static ssize_t video_global_offset_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int x, y;

	vpp_get_global_offset(&x, &y);

	return snprintf(buf, 40, "%d %d\n", x, y);
}

static ssize_t video_global_offset_store(struct class *cla,
					 struct class_attribute *attr,
					 const char *buf, size_t count)
{
	int parsed[2];

	mutex_lock(&video_module_mutex);

	if (likely(parse_para(buf, 2, parsed) == 2)) {
		vpp_set_global_offset(parsed[0], parsed[1]);
		video_property_changed = true;

		amlog_mask(LOG_MASK_SYSFS,
			   "video_offset=>x0:%d,y0:%d\n ",
			   parsed[0], parsed[1]);
	}

	mutex_unlock(&video_module_mutex);

	return count;
}

static ssize_t video_zoom_show(struct class *cla,
			struct class_attribute *attr,
			char *buf)
{
	u32 r = vpp_get_zoom_ratio();

	return snprintf(buf, 40, "%d\n", r);
}

static ssize_t video_zoom_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long r;
	int ret = 0;

	ret = kstrtoul(buf, 0, (unsigned long *)&r);
	if (ret < 0)
		return -EINVAL;

	if ((r <= MAX_ZOOM_RATIO) && (r != vpp_get_zoom_ratio())) {
		vpp_set_zoom_ratio(r);
		video_property_changed = true;
	}

	return count;
}

static ssize_t video_screen_mode_show(struct class *cla,
				      struct class_attribute *attr, char *buf)
{
	static const char * const wide_str[] = {
		"normal", "full stretch", "4-3", "16-9", "non-linear",
		"normal-noscaleup",
		"4-3 ignore", "4-3 letter box", "4-3 pan scan", "4-3 combined",
		"16-9 ignore", "16-9 letter box", "16-9 pan scan",
		"16-9 combined", "Custom AR", "AFD"
	};

	if (wide_setting < ARRAY_SIZE(wide_str)) {
		return sprintf(buf, "%d:%s\n", wide_setting,
			       wide_str[wide_setting]);
	} else
		return 0;
}

static ssize_t video_screen_mode_store(struct class *cla,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	unsigned long mode;
	int ret = 0;

	ret = kstrtoul(buf, 0, (unsigned long *)&mode);
	if (ret < 0)
		return -EINVAL;

	if ((mode < VIDEO_WIDEOPTION_MAX) && (mode != wide_setting)) {
		wide_setting = mode;
		video_property_changed = true;
	}

	return count;
}

static ssize_t video_blackout_policy_show(struct class *cla,
					  struct class_attribute *attr,
					  char *buf)
{
	return sprintf(buf, "%d\n", blackout);
}

static ssize_t video_blackout_policy_store(struct class *cla,
					   struct class_attribute *attr,
					   const char *buf, size_t count)
{
	int r;

	r = kstrtoint(buf, 0, &blackout);
	if (r < 0)
		return -EINVAL;

	if (debug_flag & DEBUG_FLAG_BLACKOUT)
		pr_info("%s(%d)\n", __func__, blackout);

	return count;
}

static ssize_t video_seek_flag_show(struct class *cla,
					  struct class_attribute *attr,
					  char *buf)
{
	return sprintf(buf, "%d\n", video_seek_flag);
}

static ssize_t video_seek_flag_store(struct class *cla,
					   struct class_attribute *attr,
					   const char *buf, size_t count)
{
	int r;

	r = kstrtoint(buf, 0, &video_seek_flag);
	if (r < 0)
		return -EINVAL;

	return count;
}

#ifdef PTS_TRACE_DEBUG
static ssize_t pts_trace_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d %d %d %d %d %d %d %d\n"
				"%d %d %d %d %d %d %d %d\n"
				"%0x %0x %0x %0x %0x %0x %0x %0x\n"
				"%0x %0x %0x %0x %0x %0x %0x %0x\n"
				"%0x %0x %0x %0x %0x %0x %0x %0x\n"
				"%0x %0x %0x %0x %0x %0x %0x %0x\n",
		pts_trace_his[0], pts_trace_his[1], pts_trace_his[2],
		pts_trace_his[3], pts_trace_his[4], pts_trace_his[5],
		pts_trace_his[6], pts_trace_his[7], pts_trace_his[8],
		pts_trace_his[9], pts_trace_his[10], pts_trace_his[11],
		pts_trace_his[12], pts_trace_his[13], pts_trace_his[14],
		pts_trace_his[15],
		pts_his[0], pts_his[1], pts_his[2], pts_his[3],
		pts_his[4], pts_his[5], pts_his[6], pts_his[7],
		pts_his[8], pts_his[9], pts_his[10], pts_his[11],
		pts_his[12], pts_his[13], pts_his[14], pts_his[15],
		scr_his[0], scr_his[1], scr_his[2], scr_his[3],
		scr_his[4], scr_his[5], scr_his[6], scr_his[7],
		scr_his[8], scr_his[9], scr_his[10], scr_his[11],
		scr_his[12], scr_his[13], scr_his[14], scr_his[15]);
}
#endif

static ssize_t video_brightness_show(struct class *cla,
				     struct class_attribute *attr, char *buf)
{
	s32 val = (READ_VCBUS_REG(VPP_VADJ1_Y + cur_dev->vpp_off) >> 8) &
			0x1ff;

	val = (val << 23) >> 23;

	return sprintf(buf, "%d\n", val);
}

static ssize_t video_brightness_store(struct class *cla,
				      struct class_attribute *attr,
				      const char *buf, size_t count)
{
	int r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if ((r < 0) || (val < -255) || (val > 255))
		return -EINVAL;

	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB)
		WRITE_VCBUS_REG_BITS(VPP_VADJ1_Y +
			cur_dev->vpp_off, val, 8, 9);
	else
		WRITE_VCBUS_REG_BITS(VPP_VADJ1_Y +
			cur_dev->vpp_off, val << 1, 8, 10);

	WRITE_VCBUS_REG(VPP_VADJ_CTRL + cur_dev->vpp_off, VPP_VADJ1_EN);

	return count;
}

static ssize_t video_contrast_show(struct class *cla,
				   struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",
		       (int)(READ_VCBUS_REG(VPP_VADJ1_Y + cur_dev->vpp_off) &
			     0xff) - 0x80);
}

static ssize_t video_contrast_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	int r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if ((r < 0) || (val < -127) || (val > 127))
		return -EINVAL;

	val += 0x80;

	WRITE_VCBUS_REG_BITS(VPP_VADJ1_Y + cur_dev->vpp_off, val, 0, 8);
	WRITE_VCBUS_REG(VPP_VADJ_CTRL + cur_dev->vpp_off, VPP_VADJ1_EN);

	return count;
}

static ssize_t vpp_brightness_show(struct class *cla,
				   struct class_attribute *attr, char *buf)
{
	s32 val = (READ_VCBUS_REG(VPP_VADJ2_Y +
			cur_dev->vpp_off) >> 8) & 0x1ff;

	val = (val << 23) >> 23;

	return sprintf(buf, "%d\n", val);
}

static ssize_t vpp_brightness_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	int r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if ((r < 0) || (val < -255) || (val > 255))
		return -EINVAL;

	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB)
		WRITE_VCBUS_REG_BITS(VPP_VADJ2_Y +
			cur_dev->vpp_off, val, 8, 9);
	else
		WRITE_VCBUS_REG_BITS(VPP_VADJ2_Y +
			cur_dev->vpp_off, val << 1, 8, 10);

	WRITE_VCBUS_REG(VPP_VADJ_CTRL + cur_dev->vpp_off, VPP_VADJ2_EN);

	return count;
}

static ssize_t vpp_contrast_show(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",
		       (int)(READ_VCBUS_REG(VPP_VADJ2_Y + cur_dev->vpp_off) &
			     0xff) - 0x80);
}

static ssize_t vpp_contrast_store(struct class *cla,
			struct class_attribute *attr, const char *buf,
			size_t count)
{
	int r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if ((r < 0) || (val < -127) || (val > 127))
		return -EINVAL;

	val += 0x80;

	WRITE_VCBUS_REG_BITS(VPP_VADJ2_Y + cur_dev->vpp_off, val, 0, 8);
	WRITE_VCBUS_REG(VPP_VADJ_CTRL + cur_dev->vpp_off, VPP_VADJ2_EN);

	return count;
}

static ssize_t video_saturation_show(struct class *cla,
				     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",
		READ_VCBUS_REG(VPP_VADJ1_Y + cur_dev->vpp_off) & 0xff);
}

static ssize_t video_saturation_store(struct class *cla,
				      struct class_attribute *attr,
				      const char *buf, size_t count)
{
	int r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if ((r < 0) || (val < -127) || (val > 127))
		return -EINVAL;

	WRITE_VCBUS_REG_BITS(VPP_VADJ1_Y + cur_dev->vpp_off, val, 0, 8);
	WRITE_VCBUS_REG(VPP_VADJ_CTRL + cur_dev->vpp_off, VPP_VADJ1_EN);

	return count;
}

static ssize_t vpp_saturation_hue_show(struct class *cla,
				       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", READ_VCBUS_REG(VPP_VADJ2_MA_MB));
}

static ssize_t vpp_saturation_hue_store(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int r;
	s32 mab = 0;
	s16 mc = 0, md = 0;

	r = kstrtoint(buf, 0, &mab);
	if ((r < 0) || (mab & 0xfc00fc00))
		return -EINVAL;

	WRITE_VCBUS_REG(VPP_VADJ2_MA_MB, mab);
	mc = (s16) ((mab << 22) >> 22);	/* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc < -512)
		mc = -512;
	md = (s16) ((mab << 6) >> 22);	/* md =  ma; */
	mab = ((mc & 0x3ff) << 16) | (md & 0x3ff);
	WRITE_VCBUS_REG(VPP_VADJ2_MC_MD, mab);
	/* WRITE_MPEG_REG(VPP_VADJ_CTRL, 1); */
	WRITE_VCBUS_REG_BITS(VPP_VADJ_CTRL + cur_dev->vpp_off, 1, 2, 1);
#ifdef PQ_DEBUG_EN
	pr_info("\n[amvideo..] set vpp_saturation OK!!!\n");
#endif
	return count;
}

/* [   24] 1/enable, 0/disable */
/* [23:16] Y */
/* [15: 8] Cb */
/* [ 7: 0] Cr */
static ssize_t video_test_screen_show(struct class *cla,
				      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", test_screen);
}
static ssize_t video_rgb_screen_show(struct class *cla,
				      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", rgb_screen);
}

#define SCALE 6

static short R_Cr[] = { -11484, -11394, -11305, -11215, -11125,
-11036, -10946, -10856, -10766, -10677, -10587, -10497, -10407,
-10318, -10228, -10138, -10049, -9959, -9869, -9779, -9690, -9600,
-9510, -9420, -9331, -9241, -9151, -9062, -8972, -8882, -8792, -8703,
-8613, -8523, -8433, -8344, -8254, -8164, -8075, -7985, -7895, -7805,
-7716, -7626, -7536, -7446, -7357, -7267, -7177, -7088, -6998, -6908,
-6818, -6729, -6639, -6549, -6459, -6370, -6280, -6190, -6101, -6011,
-5921, -5831, -5742, -5652, -5562, -5472, -5383, -5293, -5203, -5113,
-5024, -4934, -4844, -4755, -4665, -4575, -4485, -4396, -4306, -4216,
-4126, -4037, -3947, -3857, -3768, -3678, -3588, -3498, -3409, -3319,
-3229, -3139, -3050, -2960, -2870, -2781, -2691, -2601, -2511, -2422,
-2332, -2242, -2152, -2063, -1973, -1883, -1794, -1704, -1614, -1524,
-1435, -1345, -1255, -1165, -1076, -986, -896, -807, -717, -627, -537,
-448, -358, -268, -178, -89, 0, 90, 179, 269, 359, 449, 538, 628, 718,
808, 897, 987, 1077, 1166, 1256, 1346, 1436, 1525, 1615, 1705, 1795,
1884, 1974, 2064, 2153, 2243, 2333, 2423, 2512, 2602, 2692, 2782,
2871, 2961, 3051, 3140, 3230, 3320, 3410, 3499, 3589, 3679, 3769,
3858, 3948, 4038, 4127, 4217, 4307, 4397, 4486, 4576, 4666, 4756,
4845, 4935, 5025, 5114, 5204, 5294, 5384, 5473, 5563, 5653, 5743,
5832, 5922, 6012, 6102, 6191, 6281, 6371, 6460, 6550, 6640, 6730,
6819, 6909, 6999, 7089, 7178, 7268, 7358, 7447, 7537, 7627, 7717,
7806, 7896, 7986, 8076, 8165, 8255, 8345, 8434, 8524, 8614, 8704,
8793, 8883, 8973, 9063, 9152, 9242, 9332, 9421, 9511, 9601, 9691,
9780, 9870, 9960, 10050, 10139, 10229, 10319, 10408, 10498, 10588,
10678, 10767, 10857, 10947, 11037, 11126, 11216, 11306, 11395 };

static short G_Cb[] = { 2819, 2797, 2775, 2753, 2731, 2709, 2687,
2665, 2643, 2621, 2599, 2577, 2555, 2533, 2511, 2489, 2467, 2445,
2423, 2401, 2379, 2357, 2335, 2313, 2291, 2269, 2247, 2225, 2202,
2180, 2158, 2136, 2114, 2092, 2070, 2048, 2026, 2004, 1982, 1960,
1938, 1916, 1894, 1872, 1850, 1828, 1806, 1784, 1762, 1740, 1718,
1696, 1674, 1652, 1630, 1608, 1586, 1564, 1542, 1520, 1498, 1476,
1454, 1432, 1410, 1388, 1366, 1344, 1321, 1299, 1277, 1255, 1233,
1211, 1189, 1167, 1145, 1123, 1101, 1079, 1057, 1035, 1013, 991, 969,
947, 925, 903, 881, 859, 837, 815, 793, 771, 749, 727, 705, 683, 661,
639, 617, 595, 573, 551, 529, 507, 485, 463, 440, 418, 396, 374, 352,
330, 308, 286, 264, 242, 220, 198, 176, 154, 132, 110, 88, 66, 44, 22,
0, -21, -43, -65, -87, -109, -131, -153, -175, -197, -219, -241, -263,
-285, -307, -329, -351, -373, -395, -417, -439, -462, -484, -506,
-528, -550, -572, -594, -616, -638, -660, -682, -704, -726, -748,
-770, -792, -814, -836, -858, -880, -902, -924, -946, -968, -990,
-1012, -1034, -1056, -1078, -1100, -1122, -1144, -1166, -1188, -1210,
-1232, -1254, -1276, -1298, -1320, -1343, -1365, -1387, -1409, -1431,
-1453, -1475, -1497, -1519, -1541, -1563, -1585, -1607, -1629, -1651,
-1673, -1695, -1717, -1739, -1761, -1783, -1805, -1827, -1849, -1871,
-1893, -1915, -1937, -1959, -1981, -2003, -2025, -2047, -2069, -2091,
-2113, -2135, -2157, -2179, -2201, -2224, -2246, -2268, -2290, -2312,
-2334, -2356, -2378, -2400, -2422, -2444, -2466, -2488, -2510, -2532,
-2554, -2576, -2598, -2620, -2642, -2664, -2686, -2708, -2730, -2752,
-2774, -2796 };

static short G_Cr[] = { 5850, 5805, 5759, 5713, 5667, 5622, 5576,
5530, 5485, 5439, 5393, 5347, 5302, 5256, 5210, 5165, 5119, 5073,
5028, 4982, 4936, 4890, 4845, 4799, 4753, 4708, 4662, 4616, 4570,
4525, 4479, 4433, 4388, 4342, 4296, 4251, 4205, 4159, 4113, 4068,
4022, 3976, 3931, 3885, 3839, 3794, 3748, 3702, 3656, 3611, 3565,
3519, 3474, 3428, 3382, 3336, 3291, 3245, 3199, 3154, 3108, 3062,
3017, 2971, 2925, 2879, 2834, 2788, 2742, 2697, 2651, 2605, 2559,
2514, 2468, 2422, 2377, 2331, 2285, 2240, 2194, 2148, 2102, 2057,
2011, 1965, 1920, 1874, 1828, 1782, 1737, 1691, 1645, 1600, 1554,
1508, 1463, 1417, 1371, 1325, 1280, 1234, 1188, 1143, 1097, 1051,
1006, 960, 914, 868, 823, 777, 731, 686, 640, 594, 548, 503, 457, 411,
366, 320, 274, 229, 183, 137, 91, 46, 0, -45, -90, -136, -182, -228,
-273, -319, -365, -410, -456, -502, -547, -593, -639, -685, -730,
-776, -822, -867, -913, -959, -1005, -1050, -1096, -1142, -1187,
-1233, -1279, -1324, -1370, -1416, -1462, -1507, -1553, -1599, -1644,
-1690, -1736, -1781, -1827, -1873, -1919, -1964, -2010, -2056, -2101,
-2147, -2193, -2239, -2284, -2330, -2376, -2421, -2467, -2513, -2558,
-2604, -2650, -2696, -2741, -2787, -2833, -2878, -2924, -2970, -3016,
-3061, -3107, -3153, -3198, -3244, -3290, -3335, -3381, -3427, -3473,
-3518, -3564, -3610, -3655, -3701, -3747, -3793, -3838, -3884, -3930,
-3975, -4021, -4067, -4112, -4158, -4204, -4250, -4295, -4341, -4387,
-4432, -4478, -4524, -4569, -4615, -4661, -4707, -4752, -4798, -4844,
-4889, -4935, -4981, -5027, -5072, -5118, -5164, -5209, -5255, -5301,
-5346, -5392, -5438, -5484, -5529, -5575, -5621, -5666, -5712, -5758,
-5804 };

static short B_Cb[] = { -14515, -14402, -14288, -14175, -14062,
-13948, -13835, -13721, -13608, -13495, -13381, -13268, -13154,
-13041, -12928, -12814, -12701, -12587, -12474, -12360, -12247,
-12134, -12020, -11907, -11793, -11680, -11567, -11453, -11340,
-11226, -11113, -11000, -10886, -10773, -10659, -10546, -10433,
-10319, -10206, -10092, -9979, -9865, -9752, -9639, -9525, -9412,
-9298, -9185, -9072, -8958, -8845, -8731, -8618, -8505, -8391, -8278,
-8164, -8051, -7938, -7824, -7711, -7597, -7484, -7371, -7257, -7144,
-7030, -6917, -6803, -6690, -6577, -6463, -6350, -6236, -6123, -6010,
-5896, -5783, -5669, -5556, -5443, -5329, -5216, -5102, -4989, -4876,
-4762, -4649, -4535, -4422, -4309, -4195, -4082, -3968, -3855, -3741,
-3628, -3515, -3401, -3288, -3174, -3061, -2948, -2834, -2721, -2607,
-2494, -2381, -2267, -2154, -2040, -1927, -1814, -1700, -1587, -1473,
-1360, -1246, -1133, -1020, -906, -793, -679, -566, -453, -339, -226,
-112, 0, 113, 227, 340, 454, 567, 680, 794, 907, 1021, 1134, 1247,
1361, 1474, 1588, 1701, 1815, 1928, 2041, 2155, 2268, 2382, 2495,
2608, 2722, 2835, 2949, 3062, 3175, 3289, 3402, 3516, 3629, 3742,
3856, 3969, 4083, 4196, 4310, 4423, 4536, 4650, 4763, 4877, 4990,
5103, 5217, 5330, 5444, 5557, 5670, 5784, 5897, 6011, 6124, 6237,
6351, 6464, 6578, 6691, 6804, 6918, 7031, 7145, 7258, 7372, 7485,
7598, 7712, 7825, 7939, 8052, 8165, 8279, 8392, 8506, 8619, 8732,
8846, 8959, 9073, 9186, 9299, 9413, 9526, 9640, 9753, 9866, 9980,
10093, 10207, 10320, 10434, 10547, 10660, 10774, 10887, 11001, 11114,
11227, 11341, 11454, 11568, 11681, 11794, 11908, 12021, 12135, 12248,
12361, 12475, 12588, 12702, 12815, 12929, 13042, 13155, 13269, 13382,
13496, 13609, 13722, 13836, 13949, 14063, 14176, 14289, 14403
};

static u32 yuv2rgb(u32 yuv)
{
	int y = (yuv >> 16) & 0xff;
	int cb = (yuv >> 8) & 0xff;
	int cr = yuv & 0xff;
	int r, g, b;

	r = y + ((R_Cr[cr]) >> SCALE);
	g = y + ((G_Cb[cb] + G_Cr[cr]) >> SCALE);
	b = y + ((B_Cb[cb]) >> SCALE);

	r = r - 16;
	if (r < 0)
		r = 0;
	r = r*1164/1000;
	g = g - 16;
	if (g < 0)
		g = 0;
	g = g*1164/1000;
	b = b - 16;
	if (b < 0)
		b = 0;
	b = b*1164/1000;

	r = (r <= 0) ? 0 : (r >= 255) ? 255 : r;
	g = (g <= 0) ? 0 : (g >= 255) ? 255 : g;
	b = (b <= 0) ? 0 : (b >= 255) ? 255 : b;

	return  (r << 16) | (g << 8) | b;
}
/* 8bit convert to 10bit */
static u32 eight2ten(u32 yuv)
{
	int y = (yuv >> 16) & 0xff;
	int cb = (yuv >> 8) & 0xff;
	int cr = yuv & 0xff;
	u32 data32;

	/* txlx need check vd1 path bit width by s2u registers */
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_TXLX) {
		data32 = READ_VCBUS_REG(0x1d94) & 0xffff;
		if ((data32 == 0x2000) ||
			(data32 == 0x800))
			return  ((y << 20)<<2) | ((cb << 10)<<2) | (cr<<2);
		else
			return  (y << 20) | (cb << 10) | cr;
	} else
		return  (y << 20) | (cb << 10) | cr;
}

static u32 rgb2yuv(u32 rgb)
{
	int r = (rgb >> 16) & 0xff;
	int g = (rgb >> 8) & 0xff;
	int b = rgb & 0xff;
	int y, u, v;

	y = ((47*r + 157*g + 16*b + 128) >> 8) + 16;
	u = ((-26*r - 87*g + 112*b + 128) >> 8) + 128;
	v = ((112*r - 102*g - 10*b + 128) >> 8) + 128;

	return  (y << 16) | (u << 8) | v;
}

static ssize_t video_test_screen_store(struct class *cla,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	int r;
	unsigned int data = 0x0;

	r = kstrtoint(buf, 0, &test_screen);
	if (r < 0)
		return -EINVAL;
#if 0/*no use now*/
	/* vdin0 pre post blend enable or disabled */
	data = READ_VCBUS_REG(VPP_MISC);
	if (test_screen & 0x01000000)
		data |= VPP_VD1_PREBLEND;
	else
		data &= (~VPP_VD1_PREBLEND);

	if (test_screen & 0x02000000)
		data |= VPP_VD1_POSTBLEND;
	else
		data &= (~VPP_VD1_POSTBLEND);
#endif

#if DEBUG_TMP
	if (test_screen & 0x04000000)
		data |= VPP_VD2_PREBLEND;
	else
		data &= (~VPP_VD2_PREBLEND);

	if (test_screen & 0x08000000)
		data |= VPP_VD2_POSTBLEND;
	else
		data &= (~VPP_VD2_POSTBLEND);
#endif

	/* show test screen  YUV blend*/
	if (is_meson_gxm_cpu() ||
		(get_cpu_type() == MESON_CPU_MAJOR_ID_TXLX))
		/* bit width change to 10bit in gxm, 10/12 in txlx*/
		WRITE_VCBUS_REG(VPP_DUMMY_DATA1,
			eight2ten(test_screen & 0x00ffffff));
	else if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB)
		WRITE_VCBUS_REG(VPP_DUMMY_DATA1,
			yuv2rgb(test_screen & 0x00ffffff));
	else if (get_cpu_type() < MESON_CPU_MAJOR_ID_GXTVBB)
		if (READ_VCBUS_REG(VIU_OSD1_BLK0_CFG_W0) & 0x80)
			WRITE_VCBUS_REG(VPP_DUMMY_DATA1,
				test_screen & 0x00ffffff);
		else /* RGB blend */
			WRITE_VCBUS_REG(VPP_DUMMY_DATA1,
				yuv2rgb(test_screen & 0x00ffffff));
	else
		WRITE_VCBUS_REG(VPP_DUMMY_DATA1,
			test_screen & 0x00ffffff);
#if 0/*no use*/
	WRITE_VCBUS_REG(VPP_MISC, data);
#endif
	if (debug_flag & DEBUG_FLAG_BLACKOUT) {
		pr_info("%s write(VPP_MISC,%x) write(VPP_DUMMY_DATA1, %x)\n",
		       __func__, data, test_screen & 0x00ffffff);
	}
	return count;
}

static ssize_t video_rgb_screen_store(struct class *cla,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	int r;
	u32 yuv_eight;

	/* unsigned data = 0x0; */
	r = kstrtoint(buf, 0, &rgb_screen);
	if (r < 0)
		return -EINVAL;

#if DEBUG_TMP
	/* vdin0 pre post blend enable or disabled */

	data = READ_VCBUS_REG(VPP_MISC);
	if (rgb_screen & 0x01000000)
		data |= VPP_VD1_PREBLEND;
	else
		data &= (~VPP_VD1_PREBLEND);

	if (rgb_screen & 0x02000000)
		data |= VPP_VD1_POSTBLEND;
	else
		data &= (~VPP_VD1_POSTBLEND);

	if (test_screen & 0x04000000)
		data |= VPP_VD2_PREBLEND;
	else
		data &= (~VPP_VD2_PREBLEND);

	if (test_screen & 0x08000000)
		data |= VPP_VD2_POSTBLEND;
	else
		data &= (~VPP_VD2_POSTBLEND);
#endif
	/* show test screen  YUV blend*/
	yuv_eight = rgb2yuv(rgb_screen & 0x00ffffff);
	if (is_meson_gxtvbb_cpu())   {
		WRITE_VCBUS_REG(VPP_DUMMY_DATA1,
			rgb_screen & 0x00ffffff);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		WRITE_VCBUS_REG(VPP_DUMMY_DATA1,
			eight2ten(yuv_eight & 0x00ffffff));
	}
	/* WRITE_VCBUS_REG(VPP_MISC, data); */

	if (debug_flag & DEBUG_FLAG_BLACKOUT) {
		pr_info("%s write(VPP_DUMMY_DATA1, %x)\n",
		       __func__, rgb_screen & 0x00ffffff);
	}
	return count;
}



static ssize_t video_nonlinear_factor_show(struct class *cla,
					   struct class_attribute *attr,
					   char *buf)
{
	return sprintf(buf, "%d\n", vpp_get_nonlinear_factor());
}

static ssize_t video_nonlinear_factor_store(struct class *cla,
					    struct class_attribute *attr,
					    const char *buf, size_t count)
{
	int r;
	u32 factor;

	r = kstrtoint(buf, 0, &factor);
	if (r < 0)
		return -EINVAL;

	if (vpp_set_nonlinear_factor(factor) == 0)
		video_property_changed = true;

	return count;
}

static ssize_t video_disable_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", disable_video);
}

static ssize_t video_disable_store(struct class *cla,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	int r;
	int val;

	if (debug_flag & DEBUG_FLAG_BLACKOUT)
		pr_info("%s(%s)\n", __func__, buf);

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	if (_video_set_disable(val) < 0)
		return -EINVAL;

	return count;
}

static ssize_t video_global_output_show(struct class *cla,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", video_global_output);
}

static ssize_t video_global_output_store(struct class *cla,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	int r;

	r = kstrtoint(buf, 0, &video_global_output);
	if (r < 0)
		return -EINVAL;

	pr_info("%s(%d)\n", __func__, video_global_output);

	return count;
}

static ssize_t video_freerun_mode_show(struct class *cla,
				       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", freerun_mode);
}

static ssize_t video_freerun_mode_store(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int r;

	r = kstrtoint(buf, 0, &freerun_mode);
	if (r < 0)
		return -EINVAL;

	if (debug_flag)
		pr_info("%s(%d)\n", __func__, freerun_mode);

	return count;
}

static ssize_t video_speed_check_show(struct class *cla,
				      struct class_attribute *attr, char *buf)
{
	u32 h, w;

	vpp_get_video_speed_check(&h, &w);

	return snprintf(buf, 40, "%d %d\n", h, w);
}

static ssize_t video_speed_check_store(struct class *cla,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{

	set_video_speed_check(buf);

	return strnlen(buf, count);
}

static ssize_t threedim_mode_store(struct class *cla,
				   struct class_attribute *attr,
				   const char *buf, size_t len)
{
#ifdef TV_3D_FUNCTION_OPEN

	u32 type;
	int r;

	r = kstrtoint(buf, 0, &type);
	if (r < 0)
		return -EINVAL;

	if (type != process_3d_type) {
		process_3d_type = type;
		if (mvc_flag)
			process_3d_type |= MODE_3D_MVC;
		video_property_changed = true;
		if ((process_3d_type & MODE_3D_FA)
			&& cur_dispbuf && !cur_dispbuf->trans_fmt)
			/*notify di 3d mode is frame alternative mode,1*/
			/*passing two buffer in one frame */
			vf_notify_receiver_by_name("deinterlace",
			VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE,
			(void *)1);
		else
			vf_notify_receiver_by_name("deinterlace",
			VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE,
			(void *)0);
	}
#endif
	return len;
}

static ssize_t threedim_mode_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
#ifdef TV_3D_FUNCTION_OPEN
	return sprintf(buf, "process type 0x%x,trans fmt %u.\n",
		       process_3d_type, video_3d_format);
#else
	return 0;
#endif
}

static ssize_t frame_addr_show(struct class *cla, struct class_attribute *attr,
			       char *buf)
{
	struct canvas_s canvas;
	u32 addr[3];

	if (cur_dispbuf) {
		canvas_read(cur_dispbuf->canvas0Addr & 0xff, &canvas);
		addr[0] = canvas.addr;
		canvas_read((cur_dispbuf->canvas0Addr >> 8) & 0xff, &canvas);
		addr[1] = canvas.addr;
		canvas_read((cur_dispbuf->canvas0Addr >> 16) & 0xff, &canvas);
		addr[2] = canvas.addr;

		return sprintf(buf, "0x%x-0x%x-0x%x\n", addr[0], addr[1],
			       addr[2]);
	}

	return sprintf(buf, "NA\n");
}

static ssize_t frame_canvas_width_show(struct class *cla,
				       struct class_attribute *attr, char *buf)
{
	struct canvas_s canvas;
	u32 width[3];

	if (cur_dispbuf) {
		canvas_read(cur_dispbuf->canvas0Addr & 0xff, &canvas);
		width[0] = canvas.width;
		canvas_read((cur_dispbuf->canvas0Addr >> 8) & 0xff, &canvas);
		width[1] = canvas.width;
		canvas_read((cur_dispbuf->canvas0Addr >> 16) & 0xff, &canvas);
		width[2] = canvas.width;

		return sprintf(buf, "%d-%d-%d\n",
			width[0], width[1], width[2]);
	}

	return sprintf(buf, "NA\n");
}

static ssize_t frame_canvas_height_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	struct canvas_s canvas;
	u32 height[3];

	if (cur_dispbuf) {
		canvas_read(cur_dispbuf->canvas0Addr & 0xff, &canvas);
		height[0] = canvas.height;
		canvas_read((cur_dispbuf->canvas0Addr >> 8) & 0xff, &canvas);
		height[1] = canvas.height;
		canvas_read((cur_dispbuf->canvas0Addr >> 16) & 0xff, &canvas);
		height[2] = canvas.height;

		return sprintf(buf, "%d-%d-%d\n", height[0], height[1],
			       height[2]);
	}

	return sprintf(buf, "NA\n");
}

static ssize_t frame_width_show(struct class *cla,
			struct class_attribute *attr,
			char *buf)
{
	if (cur_dispbuf) {
		if (cur_dispbuf->type & VIDTYPE_COMPRESS)
			return sprintf(buf, "%d\n", cur_dispbuf->compWidth);
		else
			return sprintf(buf, "%d\n", cur_dispbuf->width);
	}

	return sprintf(buf, "NA\n");
}

static ssize_t frame_height_show(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	if (cur_dispbuf) {
		if (cur_dispbuf->type & VIDTYPE_COMPRESS)
			return sprintf(buf, "%d\n", cur_dispbuf->compHeight);
		else
			return sprintf(buf, "%d\n", cur_dispbuf->height);
	}

	return sprintf(buf, "NA\n");
}

static ssize_t frame_format_show(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	if (cur_dispbuf) {
		if ((cur_dispbuf->type & VIDTYPE_TYPEMASK) ==
		    VIDTYPE_INTERLACE_TOP)
			ret = sprintf(buf, "interlace-top\n");
		else if ((cur_dispbuf->type & VIDTYPE_TYPEMASK) ==
			 VIDTYPE_INTERLACE_BOTTOM)
			ret = sprintf(buf, "interlace-bottom\n");
		else
			ret = sprintf(buf, "progressive\n");

		if (cur_dispbuf->type & VIDTYPE_COMPRESS)
			ret += sprintf(buf + ret, "Compressed\n");

		return ret;
	}

	return sprintf(buf, "NA\n");
}

static ssize_t frame_aspect_ratio_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	if (cur_dispbuf) {
		u32 ar = (cur_dispbuf->ratio_control &
			DISP_RATIO_ASPECT_RATIO_MASK) >>
			DISP_RATIO_ASPECT_RATIO_BIT;

		if (ar)
			return sprintf(buf, "0x%x\n", ar);
		else
			return sprintf(buf, "0x%x\n",
				       (cur_dispbuf->width << 8) /
				       cur_dispbuf->height);
	}

	return sprintf(buf, "NA\n");
}

static ssize_t frame_rate_show(struct class *cla, struct class_attribute *attr,
			       char *buf)
{
	u32 cnt = frame_count - last_frame_count;
	u32 time = jiffies;
	u32 tmp = time;
	u32 rate = 0;
	u32 vsync_rate;
	ssize_t ret = 0;

	time -= last_frame_time;
	last_frame_time = tmp;
	last_frame_count = frame_count;
	if (time == 0)
		return 0;
	rate = 100 * cnt * HZ / time;
	vsync_rate = 100 * vsync_count * HZ / time;
	if (vinfo->sync_duration_den > 0) {
		ret =
		    sprintf(buf,
		"VF.fps=%d.%02d panel fps %d, dur/is: %d,v/s=%d.%02d,inc=%d\n",
				rate / 100, rate % 100,
				vinfo->sync_duration_num /
				vinfo->sync_duration_den,
				time, vsync_rate / 100, vsync_rate % 100,
				vsync_pts_inc);
	}
	if ((debugflags & DEBUG_FLAG_CALC_PTS_INC) && time > HZ * 10
	    && vsync_rate > 0) {
		if ((vsync_rate * vsync_pts_inc / 100) != 90000)
			vsync_pts_inc = 90000 * 100 / (vsync_rate);
	}
	vsync_count = 0;
	return ret;
}

static ssize_t vframe_states_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	int ret = 0;
	struct vframe_states states;
	unsigned long flags;
	struct vframe_s *vf;

	if (video_vf_get_states(&states) == 0) {
		ret += sprintf(buf + ret, "vframe_pool_size=%d\n",
			states.vf_pool_size);
		ret += sprintf(buf + ret, "vframe buf_free_num=%d\n",
			states.buf_free_num);
		ret += sprintf(buf + ret, "vframe buf_recycle_num=%d\n",
			states.buf_recycle_num);
		ret += sprintf(buf + ret, "vframe buf_avail_num=%d\n",
			states.buf_avail_num);

		spin_lock_irqsave(&lock, flags);

		vf = video_vf_peek();
		if (vf) {
			ret += sprintf(buf + ret,
				"vframe ready frame delayed =%dms\n",
				(int)(jiffies_64 -
				vf->ready_jiffies64) * 1000 /
				HZ);
			ret += sprintf(buf + ret,
				"vf index=%d\n", vf->index);
			ret += sprintf(buf + ret,
				"vf->pts=%d\n", vf->pts);
			ret += sprintf(buf + ret,
				"cur vpts=%d\n",
				timestamp_vpts_get());
			ret += sprintf(buf + ret,
				"vf type=%d\n",
				vf->type);
			if (vf->type & VIDTYPE_COMPRESS) {
				ret += sprintf(buf + ret,
					"vf compHeadAddr=%x\n",
						vf->compHeadAddr);
				ret += sprintf(buf + ret,
					"vf compBodyAddr =%x\n",
						vf->compBodyAddr);
			} else {
				ret += sprintf(buf + ret,
					"vf canvas0Addr=%x\n",
						vf->canvas0Addr);
				ret += sprintf(buf + ret,
					"vf canvas1Addr=%x\n",
						vf->canvas1Addr);
				ret += sprintf(buf + ret,
					"vf canvas0Addr.y.addr=%x(%d)\n",
					canvas_get_addr(
					canvasY(vf->canvas0Addr)),
					canvas_get_addr(
					canvasY(vf->canvas0Addr)));
				ret += sprintf(buf + ret,
					"vf canvas0Adr.uv.adr=%x(%d)\n",
					canvas_get_addr(
					canvasUV(vf->canvas0Addr)),
					canvas_get_addr(
					canvasUV(vf->canvas0Addr)));
			}
		}
		spin_unlock_irqrestore(&lock, flags);

	} else
		ret += sprintf(buf + ret, "vframe no states\n");

	return ret;
}

#ifdef CONFIG_AM_VOUT
static ssize_t device_resolution_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
#ifdef CONFIG_SUPPORT_VIDEO_ON_VPP2
	const struct vinfo_s *info;

	if (cur_dev == &video_dev[0])
		info = get_current_vinfo();
	else
		info = get_current_vinfo2();
#else
	const struct vinfo_s *info = get_current_vinfo();
#endif

	if (info != NULL)
		return sprintf(buf, "%dx%d\n", info->width, info->height);
	else
		return sprintf(buf, "0x0\n");
}
#endif

static ssize_t video_filename_show(struct class *cla,
				   struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", file_name);
}

static ssize_t video_filename_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	size_t r;

	/* check input buf to mitigate buffer overflow issue */
	if (strlen(buf) >= sizeof(file_name)) {
		memcpy(file_name, buf, sizeof(file_name));
		file_name[sizeof(file_name)-1] = '\0';
		r = 1;
	} else
		r = sscanf(buf, "%s", file_name);
	if (r != 1)
		return -EINVAL;

	return r;
}

static ssize_t video_debugflags_show(struct class *cla,
				     struct class_attribute *attr, char *buf)
{
	int len = 0;

	len += sprintf(buf + len, "value=%d\n", debugflags);
	len += sprintf(buf + len, "bit0:playing as fast!\n");
	len += sprintf(buf + len,
		"bit1:enable calc pts inc in frame rate show\n");
	return len;
}

static ssize_t video_debugflags_store(struct class *cla,
				      struct class_attribute *attr,
				      const char *buf, size_t count)
{
	int r;
	int value = -1;

/*
 *	r = sscanf(buf, "%d", &value);
 *	if (r == 1) {
 *		debugflags = value;
 *		seted = 1;
 *	} else {
 *		r = sscanf(buf, "0x%x", &value);
 *		if (r == 1) {
 *			debugflags = value;
 *			seted = 1;
 *		}
 *	}
 */

	r = kstrtoint(buf, 0, &value);
	if (r < 0)
		return -EINVAL;

	debugflags = value;

	pr_info("debugflags changed to %d(%x)\n", debugflags,
	       debugflags);
	return count;

}

static ssize_t trickmode_duration_show(struct class *cla,
				       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "trickmode frame duration %d\n",
		       trickmode_duration / 9000);
}

static ssize_t trickmode_duration_store(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int r;
	u32 s_value;

	r = kstrtoint(buf, 0, &s_value);
	if (r < 0)
		return -EINVAL;

	trickmode_duration = s_value * 9000;

	return count;
}

static ssize_t video_vsync_pts_inc_upint_show(struct class *cla,
					      struct class_attribute *attr,
					      char *buf)
{
	if (vsync_pts_inc_upint)
		return sprintf(buf,
		"%d,freerun %d,1.25xInc %d,1.12xInc %d,inc+10 %d,1xInc %d\n",
		vsync_pts_inc_upint, vsync_freerun,
		vsync_pts_125, vsync_pts_112, vsync_pts_101,
		vsync_pts_100);
	else
		return sprintf(buf, "%d\n", vsync_pts_inc_upint);
}

static ssize_t video_vsync_pts_inc_upint_store(struct class *cla,
					       struct class_attribute *attr,
					       const char *buf, size_t count)
{
	int r;

	r = kstrtoint(buf, 0, &vsync_pts_inc_upint);
	if (r < 0)
		return -EINVAL;

	if (debug_flag)
		pr_info("%s(%d)\n", __func__, vsync_pts_inc_upint);

	return count;
}

static ssize_t slowsync_repeat_enable_show(struct class *cla,
					   struct class_attribute *attr,
					   char *buf)
{
	return sprintf(buf, "slowsync repeate enable = %d\n",
		       slowsync_repeat_enable);
}

static ssize_t slowsync_repeat_enable_store(struct class *cla,
					    struct class_attribute *attr,
					    const char *buf, size_t count)
{
	int r;

	r = kstrtoint(buf, 0, &slowsync_repeat_enable);
	if (r < 0)
		return -EINVAL;

	if (debug_flag)
		pr_info("%s(%d)\n", __func__, slowsync_repeat_enable);

	return count;
}

static ssize_t video_vsync_slow_factor_show(struct class *cla,
					    struct class_attribute *attr,
					    char *buf)
{
	return sprintf(buf, "%d\n", vsync_slow_factor);
}

static ssize_t video_vsync_slow_factor_store(struct class *cla,
					     struct class_attribute *attr,
					     const char *buf, size_t count)
{
	int r;

	r = kstrtoint(buf, 0, &vsync_slow_factor);
	if (r < 0)
		return -EINVAL;

	if (debug_flag)
		pr_info("%s(%d)\n", __func__, vsync_slow_factor);

	return count;
}

static ssize_t fps_info_show(struct class *cla, struct class_attribute *attr,
			     char *buf)
{
	u32 cnt = frame_count - last_frame_count;
	u32 time = jiffies;
	u32 input_fps = 0;
	u32 tmp = time;

	time -= last_frame_time;
	last_frame_time = tmp;
	last_frame_count = frame_count;
	if (time != 0)
		output_fps = cnt * HZ / time;
	if (cur_dispbuf && cur_dispbuf->duration > 0) {
		input_fps = 96000 / cur_dispbuf->duration;
		if (output_fps > input_fps)
			output_fps = input_fps;
	} else
		input_fps = output_fps;
	return sprintf(buf, "input_fps:0x%x output_fps:0x%x drop_fps:0x%x\n",
		       input_fps, output_fps, input_fps - output_fps);
}

static ssize_t video_layer1_state_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	/*return sprintf(buf, "%d\n",*/
				/*(READ_VCBUS_REG(VPP_MISC + cur_dev->vpp_off)*/
				/*& VPP_VD1_PREBLEND) ? 1 : 0);*/
	return sprintf(buf, "%d\n", video_enabled);
}

void set_video_angle(u32 s_value)
{
	if ((s_value <= 3) && (video_angle != s_value)) {
		video_angle = s_value;
		video_prot.angle_changed = 1;
		video_prot.video_started = 1;
		pr_info("video_prot angle:%d\n", video_angle);
	}
}
EXPORT_SYMBOL(set_video_angle);

static ssize_t video_angle_show(struct class *cla, struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 40, "%d\n", video_angle);
}

static ssize_t video_angle_store(struct class *cla,
				 struct class_attribute *attr, const char *buf,
				 size_t count)
{
	int r;
	u32 s_value;

	r = kstrtoint(buf, 0, &s_value);
	if (r < 0)
		return -EINVAL;

	set_video_angle(s_value);
	return strnlen(buf, count);
}

static ssize_t show_first_frame_nosync_show(struct class *cla,
					    struct class_attribute *attr,
					    char *buf)
{
	return sprintf(buf, "%d\n", show_first_frame_nosync ? 1 : 0);
}

static ssize_t show_first_frame_nosync_store(struct class *cla,
					     struct class_attribute *attr,
					     const char *buf, size_t count)
{
	int r;
	int value;

	r = kstrtoint(buf, 0, &value);
	if (r < 0)
		return -EINVAL;

	if (value == 0)
		show_first_frame_nosync = false;
	else
		show_first_frame_nosync = true;

	return count;
}

static ssize_t show_first_picture_store(struct class *cla,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	int r;
	int value;

	r = kstrtoint(buf, 0, &value);
	if (r < 0)
		return -EINVAL;

	if (value == 0)
		show_first_picture = false;
	else
		show_first_picture = true;

	return count;
}

static ssize_t video_free_keep_buffer_store(struct class *cla,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	int r;
	int val;

	if (debug_flag & DEBUG_FLAG_BLACKOUT)
		pr_info("%s(%s)\n", __func__, buf);
	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	if (val == 1)
		try_free_keep_video(1);
	return count;
}


static ssize_t free_cma_buffer_store(struct class *cla,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	int r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	if (val == 1) {
		pr_info("start to free cma buffer\n");
#if DEBUG_TMP
		vh265_free_cmabuf();
		vh264_4k_free_cmabuf();
		vdec_free_cmabuf();
#endif
	}
	return count;
}

static ssize_t pic_mode_info_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int ret = 0;

	if (cur_dispbuf) {
		u32 adapted_mode = (cur_dispbuf->ratio_control
			& DISP_RATIO_ADAPTED_PICMODE) ? 1 : 0;
		u32 info_frame = (cur_dispbuf->ratio_control
			& DISP_RATIO_INFOFRAME_AVAIL) ? 1 : 0;

		ret += sprintf(buf + ret, "ratio_control=0x%x\n",
			cur_dispbuf->ratio_control);
		ret += sprintf(buf + ret, "adapted_mode=%d\n",
			adapted_mode);
		ret += sprintf(buf + ret, "info_frame=%d\n",
			info_frame);
		ret += sprintf(buf + ret,
			"hs=%d, he=%d, vs=%d, ve=%d\n",
			cur_dispbuf->pic_mode.hs,
			cur_dispbuf->pic_mode.he,
			cur_dispbuf->pic_mode.vs,
			cur_dispbuf->pic_mode.ve);
		ret += sprintf(buf + ret, "screen_mode=%d\n",
			cur_dispbuf->pic_mode.screen_mode);
		ret += sprintf(buf + ret, "custom_ar=%d\n",
			cur_dispbuf->pic_mode.custom_ar);
		ret += sprintf(buf + ret, "AFD_enable=%d\n",
			cur_dispbuf->pic_mode.AFD_enable);
		return ret;
	}
	return sprintf(buf, "NA\n");
}

static ssize_t video_inuse_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	size_t r;

	mutex_lock(&video_inuse_mutex);
	if (video_inuse == 0) {
		r = sprintf(buf, "%d\n", video_inuse);
		video_inuse = 1;
		pr_info("video_inuse return 0,set 1\n");
	} else {
		r = sprintf(buf, "%d\n", video_inuse);
		pr_info("video_inuse = %d\n", video_inuse);
	}
	mutex_unlock(&video_inuse_mutex);
	return r;
}

static ssize_t video_inuse_store(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	mutex_lock(&video_inuse_mutex);
	r = kstrtoint(buf, 0, &val);
	pr_info("set video_inuse val:%d\n", val);
	video_inuse = val;
	mutex_unlock(&video_inuse_mutex);
	if (r != 1)
		return -EINVAL;

	return count;
}

static struct class_attribute amvideo_class_attrs[] = {
	__ATTR(axis,
	       0664,
	       video_axis_show,
	       video_axis_store),
	__ATTR(crop,
	       0644,
	       video_crop_show,
	       video_crop_store),
	__ATTR(sr,
	       0644,
	       video_sr_show,
	       video_sr_store),
	__ATTR(global_offset,
	       0644,
	       video_global_offset_show,
	       video_global_offset_store),
	__ATTR(screen_mode,
	       0664,
	       video_screen_mode_show,
	       video_screen_mode_store),
	__ATTR(blackout_policy,
	       0664,
	       video_blackout_policy_show,
	       video_blackout_policy_store),
	__ATTR(video_seek_flag,
	       0664,
	       video_seek_flag_show,
	       video_seek_flag_store),
	__ATTR(disable_video,
	       0664,
	       video_disable_show,
	       video_disable_store),
	 __ATTR(video_global_output,
			0664,
			video_global_output_show,
			video_global_output_store),
	__ATTR(zoom,
	       0664,
	       video_zoom_show,
	       video_zoom_store),
	__ATTR(brightness,
	       0644,
	       video_brightness_show,
	       video_brightness_store),
	__ATTR(contrast,
	       0644,
	       video_contrast_show,
	       video_contrast_store),
	__ATTR(vpp_brightness,
	       0644,
	       vpp_brightness_show,
	       vpp_brightness_store),
	__ATTR(vpp_contrast,
	       0644,
	       vpp_contrast_show,
	       vpp_contrast_store),
	__ATTR(saturation,
	       0644,
	       video_saturation_show,
	       video_saturation_store),
	__ATTR(vpp_saturation_hue,
	       0644,
	       vpp_saturation_hue_show,
	       vpp_saturation_hue_store),
	__ATTR(test_screen,
	       0644,
	       video_test_screen_show,
	       video_test_screen_store),
	__ATTR(rgb_screen,
	       0644,
	       video_rgb_screen_show,
	       video_rgb_screen_store),
	__ATTR(file_name,
	       0644,
	       video_filename_show,
	       video_filename_store),
	__ATTR(debugflags,
	       0644,
	       video_debugflags_show,
	       video_debugflags_store),
	__ATTR(trickmode_duration,
	       0644,
	       trickmode_duration_show,
	       trickmode_duration_store),
	__ATTR(nonlinear_factor,
	       0644,
	       video_nonlinear_factor_show,
	       video_nonlinear_factor_store),
	__ATTR(freerun_mode,
	       0644,
	       video_freerun_mode_show,
	       video_freerun_mode_store),
	__ATTR(video_speed_check_h_w,
	       0644,
	       video_speed_check_show,
	       video_speed_check_store),
	__ATTR(threedim_mode,
	       0644,
	       threedim_mode_show,
	       threedim_mode_store),
	__ATTR(vsync_pts_inc_upint,
	       0644,
	       video_vsync_pts_inc_upint_show,
	       video_vsync_pts_inc_upint_store),
	__ATTR(vsync_slow_factor,
	       0644,
	       video_vsync_slow_factor_show,
	       video_vsync_slow_factor_store),
	__ATTR(angle,
	       0644,
	       video_angle_show,
	       video_angle_store),
	__ATTR(stereo_scaler,
	       0644, NULL,
	       video_3d_scale_store),
	__ATTR(show_first_frame_nosync,
	       0644,
	       show_first_frame_nosync_show,
	       show_first_frame_nosync_store),
	__ATTR(show_first_picture,
	       0664, NULL,
	       show_first_picture_store),
	__ATTR(slowsync_repeat_enable,
	       0644,
	       slowsync_repeat_enable_show,
	       slowsync_repeat_enable_store),
	__ATTR(free_keep_buffer,
	       0664, NULL,
	       video_free_keep_buffer_store),
	__ATTR(free_cma_buffer,
	       0664, NULL,
	       free_cma_buffer_store),
#ifdef CONFIG_AM_VOUT
	__ATTR_RO(device_resolution),
#endif
#ifdef PTS_TRACE_DEBUG
	__ATTR_RO(pts_trace),
#endif
	__ATTR(video_inuse,
	       0664,
	       video_inuse_show,
	       video_inuse_store),
	__ATTR_RO(frame_addr),
	__ATTR_RO(frame_canvas_width),
	__ATTR_RO(frame_canvas_height),
	__ATTR_RO(frame_width),
	__ATTR_RO(frame_height),
	__ATTR_RO(frame_format),
	__ATTR_RO(frame_aspect_ratio),
	__ATTR_RO(frame_rate),
	__ATTR_RO(vframe_states),
	__ATTR_RO(video_state),
	__ATTR_RO(fps_info),
	__ATTR_RO(video_layer1_state),
	__ATTR_RO(pic_mode_info),
	__ATTR_NULL
};

static struct class_attribute amvideo_poll_class_attrs[] = {
	__ATTR_RO(frame_width),
	__ATTR_RO(frame_height),
	__ATTR_RO(vframe_states),
	__ATTR_RO(video_state),
	__ATTR_NULL
};

#ifdef CONFIG_PM
static int amvideo_class_suspend(struct device *dev, pm_message_t state)
{
#if 0

	pm_state.event = state.event;

	if (state.event == PM_EVENT_SUSPEND) {
		pm_state.vpp_misc =
			READ_VCBUS_REG(VPP_MISC + cur_dev->vpp_off);

		DisableVideoLayer_NoDelay();

		msleep(50);
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
		if ((get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		    && !is_meson_mtvd_cpu()) {
			vpu_delay_work_flag = 0;
		}
		/* #endif */

	}
#endif
	return 0;
}

static int amvideo_class_resume(struct device *dev)
{
#if 0
#define VPP_MISC_VIDEO_BITS_MASK \
	((VPP_VD2_ALPHA_MASK << VPP_VD2_ALPHA_BIT) | \
	VPP_VD2_PREBLEND | VPP_VD1_PREBLEND |\
	VPP_VD2_POSTBLEND | VPP_VD1_POSTBLEND | VPP_POSTBLEND_EN)\

	if (pm_state.event == PM_EVENT_SUSPEND) {
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
		if ((get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		    && !is_meson_mtvd_cpu()) {
			switch_vpu_mem_pd_vmod(VPU_VIU_VD1,
					       pm_state.mem_pd_vd1);
			switch_vpu_mem_pd_vmod(VPU_VIU_VD2,
					       pm_state.mem_pd_vd2);
			switch_vpu_mem_pd_vmod(VPU_DI_POST,
					       pm_state.mem_pd_di_post);
		}
		/* #endif */
		WRITE_VCBUS_REG(VPP_MISC + cur_dev->vpp_off,
			(READ_VCBUS_REG(VPP_MISC + cur_dev->vpp_off) &
			(~VPP_MISC_VIDEO_BITS_MASK)) |
			(pm_state.vpp_misc & VPP_MISC_VIDEO_BITS_MASK));
		WRITE_VCBUS_REG(VPP_MISC +
			cur_dev->vpp_off, pm_state.vpp_misc);

		pm_state.event = -1;
		if (debug_flag & DEBUG_FLAG_BLACKOUT) {
			pr_info("%s write(VPP_MISC,%x)\n", __func__,
			       pm_state.vpp_misc);
		}
	}
#ifdef CONFIG_SCREEN_ON_EARLY
	if (power_key_pressed) {
		vout_pll_resume_early();
		osd_resume_early();
		resume_vout_early();
		power_key_pressed = 0;
	}
#endif
#endif
	return 0;
}
#endif

static struct class amvideo_class = {
	.name = AMVIDEO_CLASS_NAME,
	.class_attrs = amvideo_class_attrs,
#ifdef CONFIG_PM
	.suspend = amvideo_class_suspend,
	.resume = amvideo_class_resume,
#endif
};

static struct class amvideo_poll_class = {
	.name = AMVIDEO_POLL_CLASS_NAME,
	.class_attrs = amvideo_poll_class_attrs,
};

#ifdef TV_REVERSE
static int __init vpp_axis_reverse(char *str)
{
	unsigned char *ptr = str;

	pr_info("%s: bootargs is %s\n", __func__, str);
	if (strstr(ptr, "1"))
		reverse = true;
	else
		reverse = false;

	return 0;
}

__setup("video_reverse=", vpp_axis_reverse);
#endif

struct vframe_s *get_cur_dispbuf(void)
{
	return cur_dispbuf;
}

#ifdef CONFIG_AM_VOUT
int vout_notify_callback(struct notifier_block *block, unsigned long cmd,
			 void *para)
{
	const struct vinfo_s *info;
	ulong flags;

#ifdef CONFIG_SUPPORT_VIDEO_ON_VPP2
	if (cur_dev != &video_dev[0])
		return 0;
#endif
	switch (cmd) {
	case VOUT_EVENT_MODE_CHANGE:
		info = get_current_vinfo();
		spin_lock_irqsave(&lock, flags);
		vinfo = info;
		/* pre-calculate vsync_pts_inc in 90k unit */
		vsync_pts_inc = 90000 * vinfo->sync_duration_den /
				vinfo->sync_duration_num;
		vsync_pts_inc_scale = vinfo->sync_duration_den;
		vsync_pts_inc_scale_base = vinfo->sync_duration_num;
		spin_unlock_irqrestore(&lock, flags);
		new_vmode = vinfo->mode;
		break;
	case VOUT_EVENT_OSD_PREBLEND_ENABLE:
		vpp_set_osd_layer_preblend(para);
		break;
	case VOUT_EVENT_OSD_DISP_AXIS:
		vpp_set_osd_layer_position(para);
		break;
	}
	return 0;
}

#ifdef CONFIG_SUPPORT_VIDEO_ON_VPP2
int vout2_notify_callback(struct notifier_block *block, unsigned long cmd,
			  void *para)
{
	const struct vinfo_s *info;
	ulong flags;

	if (cur_dev != &video_dev[1])
		return 0;

	switch (cmd) {
	case VOUT_EVENT_MODE_CHANGE:
		info = get_current_vinfo2();
		spin_lock_irqsave(&lock, flags);
		vinfo = info;
		/* pre-calculate vsync_pts_inc in 90k unit */
		vsync_pts_inc = 90000 * vinfo->sync_duration_den /
				vinfo->sync_duration_num;
		vsync_pts_inc_scale = vinfo->sync_duration_den;
		vsync_pts_inc_scale_base = vinfo->sync_duration_num;
		spin_unlock_irqrestore(&lock, flags);
		break;
	case VOUT_EVENT_OSD_PREBLEND_ENABLE:
		vpp_set_osd_layer_preblend(para);
		break;
	case VOUT_EVENT_OSD_DISP_AXIS:
		vpp_set_osd_layer_position(para);
		break;
	}
	return 0;
}
#endif


static struct notifier_block vout_notifier = {
	.notifier_call = vout_notify_callback,
};

#ifdef CONFIG_SUPPORT_VIDEO_ON_VPP2
static struct notifier_block vout2_notifier = {
	.notifier_call = vout2_notify_callback,
};
#endif


static void vout_hook(void)
{
	vout_register_client(&vout_notifier);

#ifdef CONFIG_SUPPORT_VIDEO_ON_VPP2
	vout2_register_client(&vout2_notifier);
#endif

	vinfo = get_current_vinfo();

	if (!vinfo) {
#if DEBUG_TMP
		set_current_vmode(VMODE_720P);
#endif
		vinfo = get_current_vinfo();
	}

	if (vinfo) {
		vsync_pts_inc = 90000 * vinfo->sync_duration_den /
			vinfo->sync_duration_num;
		vsync_pts_inc_scale = vinfo->sync_duration_den;
		vsync_pts_inc_scale_base = vinfo->sync_duration_num;
		old_vmode = new_vmode = vinfo->mode;
	}
#ifdef CONFIG_AM_VIDEO_LOG
	if (vinfo) {
		amlog_mask(LOG_MASK_VINFO, "vinfo = %p\n", vinfo);
		amlog_mask(LOG_MASK_VINFO, "display platform %s:\n",
			   vinfo->name);
		amlog_mask(LOG_MASK_VINFO, "\tresolution %d x %d\n",
			   vinfo->width, vinfo->height);
		amlog_mask(LOG_MASK_VINFO, "\taspect ratio %d : %d\n",
			   vinfo->aspect_ratio_num, vinfo->aspect_ratio_den);
		amlog_mask(LOG_MASK_VINFO, "\tsync duration %d : %d\n",
			   vinfo->sync_duration_num, vinfo->sync_duration_den);
	}
#endif
}
#endif

static int amvideo_notify_callback(
	struct notifier_block *block,
	unsigned long cmd,
	void *para)
{
	u32 *p, val;

	switch (cmd) {
	case AMVIDEO_UPDATE_OSD_MODE:
		p = (u32 *)para;
		if (!update_osd_vpp_misc)
			osd_vpp_misc_mask = p[1];
		val = osd_vpp_misc
			& (~osd_vpp_misc_mask);
		val |= (p[0] & osd_vpp_misc_mask);
		osd_vpp_misc = val;
		if (!update_osd_vpp_misc)
			update_osd_vpp_misc = true;
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block amvideo_notifier = {
	.notifier_call = amvideo_notify_callback,
};

static RAW_NOTIFIER_HEAD(amvideo_notifier_list);
int amvideo_register_client(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&amvideo_notifier_list, nb);
}
EXPORT_SYMBOL(amvideo_register_client);

int amvideo_unregister_client(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&amvideo_notifier_list, nb);
}
EXPORT_SYMBOL(amvideo_unregister_client);

int amvideo_notifier_call_chain(unsigned long val, void *v)
{
	return raw_notifier_call_chain(&amvideo_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(amvideo_notifier_call_chain);

#if 1		/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */

static void do_vpu_delay_work(struct work_struct *work)
{
	unsigned long flags;
	unsigned int r;

#if DEBUG_TMP
	if (vpu_delay_work_flag & VPU_VIDEO_LAYER1_CHANGED) {
		vpu_delay_work_flag &= ~VPU_VIDEO_LAYER1_CHANGED;

		switch_set_state(&video1_state_sdev, !!video_enabled);
	}
#endif
	spin_lock_irqsave(&delay_work_lock, flags);

	if (vpu_delay_work_flag & VPU_DELAYWORK_VPU_CLK) {
		vpu_delay_work_flag &= ~VPU_DELAYWORK_VPU_CLK;

		spin_unlock_irqrestore(&delay_work_lock, flags);

		if (vpu_clk_level > 0)
			request_vpu_clk_vmod(360000000, VPU_VIU_VD1);
		else
			release_vpu_clk_vmod(VPU_VIU_VD1);

		spin_lock_irqsave(&delay_work_lock, flags);
	}

	r = READ_VCBUS_REG(VPP_MISC + cur_dev->vpp_off);

	if (vpu_mem_power_off_count > 0) {
		vpu_mem_power_off_count--;

		if (vpu_mem_power_off_count == 0) {
			if ((vpu_delay_work_flag &
			     VPU_DELAYWORK_MEM_POWER_OFF_VD1)
			    && ((r & VPP_VD1_PREBLEND) == 0)) {
				vpu_delay_work_flag &=
				    ~VPU_DELAYWORK_MEM_POWER_OFF_VD1;

				switch_vpu_mem_pd_vmod(
					VPU_VIU_VD1,
					VPU_MEM_POWER_DOWN);
				switch_vpu_mem_pd_vmod(
					VPU_AFBC_DEC,
					VPU_MEM_POWER_DOWN);
				switch_vpu_mem_pd_vmod(
					VPU_DI_POST,
					VPU_MEM_POWER_DOWN);
				if (!legacy_vpp)
					switch_vpu_mem_pd_vmod(
						VPU_VD1_SCALE,
						VPU_MEM_POWER_DOWN);
			}

			if ((vpu_delay_work_flag &
			     VPU_DELAYWORK_MEM_POWER_OFF_VD2)
			    && ((r & VPP_VD2_PREBLEND) == 0)) {
				vpu_delay_work_flag &=
				    ~VPU_DELAYWORK_MEM_POWER_OFF_VD2;

				switch_vpu_mem_pd_vmod(
					VPU_VIU_VD2,
					VPU_MEM_POWER_DOWN);
				switch_vpu_mem_pd_vmod(
					VPU_AFBC_DEC1,
					VPU_MEM_POWER_DOWN);
				if (!legacy_vpp)
					switch_vpu_mem_pd_vmod(
						VPU_VD2_SCALE,
						VPU_MEM_POWER_DOWN);
			}

			if ((vpu_delay_work_flag &
			     VPU_DELAYWORK_MEM_POWER_OFF_PROT)
			    && ((r & VPP_VD1_PREBLEND) == 0)) {
				vpu_delay_work_flag &=
				    ~VPU_DELAYWORK_MEM_POWER_OFF_PROT;
			}
		}
	}

	spin_unlock_irqrestore(&delay_work_lock, flags);
}
#endif

/*********************************************************/
struct device *get_video_device(void)
{
	return amvideo_dev;
}

static int __init video_early_init(void)
{
	/* todo: move this to clock tree, enable VPU clock */
	/* WRITE_CBUS_REG(HHI_VPU_CLK_CNTL,*/
	/*(1<<9) | (1<<8) | (3)); // fclk_div3/4 = ~200M */
	/* WRITE_CBUS_REG(HHI_VPU_CLK_CNTL,*/
	/*(3<<9) | (1<<8) | (0)); // fclk_div7/1 = 364M*/
	/*moved to vpu.c, default config by dts */

	u32 cur_hold_line;

#if 0	/* if (0 >= VMODE_MAX) //DEBUG_TMP */
#if 1				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
			WRITE_VCBUS_REG_BITS(VPP_OFIFO_SIZE, 0xfff,
				VPP_OFIFO_SIZE_BIT, VPP_OFIFO_SIZE_WID);
		else
			WRITE_VCBUS_REG_BITS(VPP_OFIFO_SIZE, 0x77f,
				VPP_OFIFO_SIZE_BIT, VPP_OFIFO_SIZE_WID);
#if 0			/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV */
		WRITE_VCBUS_REG_BITS(VPP_OFIFO_SIZE, 0x800, VPP_OFIFO_SIZE_BIT,
				     VPP_OFIFO_SIZE_WID);
#endif
#endif			/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
#else
	if (!legacy_vpp) {
		WRITE_VCBUS_REG_BITS(VPP_OFIFO_SIZE, 0x1000,
			VPP_OFIFO_SIZE_BIT, VPP_OFIFO_SIZE_WID);
		WRITE_VCBUS_REG_BITS(
			VPP_MATRIX_CTRL, 0, 10, 5);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
		WRITE_VCBUS_REG_BITS(VPP_OFIFO_SIZE, 0xfff,
			VPP_OFIFO_SIZE_BIT, VPP_OFIFO_SIZE_WID);
#endif

#if 1			/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	WRITE_VCBUS_REG(VPP_PREBLEND_VD1_H_START_END, 4096);
	WRITE_VCBUS_REG(VPP_BLEND_VD2_H_START_END, 4096);
#endif
	if (is_meson_txl_cpu() || is_meson_txlx_cpu()) {
		/* fifo max size on txl :128*3=384[0x180]  */
		WRITE_VCBUS_REG(
			VD1_IF0_LUMA_FIFO_SIZE + cur_dev->viu_off, 0x180);
		WRITE_VCBUS_REG(
			VD2_IF0_LUMA_FIFO_SIZE + cur_dev->viu_off, 0x180);
	}

#if 0	/* if (0 >= VMODE_MAX) //DEBUG_TMP */
		CLEAR_VCBUS_REG_MASK(VPP_VSC_PHASE_CTRL,
				     VPP_PHASECTL_TYPE_INTERLACE);
		SET_VCBUS_REG_MASK(VPP_MISC, VPP_OUT_SATURATE);
		WRITE_VCBUS_REG(VPP_HOLD_LINES + cur_dev->vpp_off, 0x08080808);
#endif

#ifdef CONFIG_SUPPORT_VIDEO_ON_VPP2
#if 0	/* if (0 >= VMODE_MAX) //DEBUG_TMP */
		CLEAR_VCBUS_REG_MASK(VPP2_VSC_PHASE_CTRL,
				     VPP_PHASECTL_TYPE_INTERLACE);
		SET_VCBUS_REG_MASK(VPP2_MISC, VPP_OUT_SATURATE);
		WRITE_VCBUS_REG(VPP2_HOLD_LINES, 0x08080808);
#endif
#if 1				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	WRITE_VCBUS_REG_BITS(VPP2_OFIFO_SIZE, 0x800,
			     VPP_OFIFO_SIZE_BIT, VPP_OFIFO_SIZE_WID);
#else
	WRITE_VCBUS_REG_BITS(VPP2_OFIFO_SIZE, 0x780,
			     VPP_OFIFO_SIZE_BIT, VPP_OFIFO_SIZE_WID);
#endif
	/*
	 *WRITE_VCBUS_REG_BITS(VPU_OSD3_MMC_CTRL, 1, 12, 2);
	 *select vdisp_mmc_arb for VIU2_OSD1 request
	 */
	WRITE_VCBUS_REG_BITS(VPU_OSD3_MMC_CTRL, 2, 12, 2);
	/* select vdin_mmc_arb for VIU2_OSD1 request */
#endif
	/* default 10bit setting for gxm */
	if (is_meson_gxm_cpu()) {
		WRITE_VCBUS_REG_BITS(VIU_MISC_CTRL1, 0xff, 16, 8);
		WRITE_VCBUS_REG(VPP_DOLBY_CTRL, 0x22000);
		/*
		 *default setting is black for dummy data1& dumy data0,
		 *for dummy data1 the y/cb/cr data width is 10bit on gxm,
		 *for dummy data the y/cb/cr data width is 8bit but
		 *vpp_dummy_data will be left shift 2bit auto on gxm!!!
		 */
		WRITE_VCBUS_REG(VPP_DUMMY_DATA1, 0x1020080);
		WRITE_VCBUS_REG(VPP_DUMMY_DATA, 0x42020);
	} else if (is_meson_txlx_cpu() ||
		cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		/*black 10bit*/
		WRITE_VCBUS_REG(VPP_DUMMY_DATA, 0x4080200);
	}
	/* temp: enable VPU arb mem */
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)
		switch_vpu_mem_pd_vmod(VPU_VPU_ARB, VPU_MEM_POWER_ON);

	/*disable sr default when power up*/
	WRITE_VCBUS_REG(VPP_SRSHARP0_CTRL, 0);
	WRITE_VCBUS_REG(VPP_SRSHARP1_CTRL, 0);

	cur_hold_line = READ_VCBUS_REG(VPP_HOLD_LINES + cur_dev->vpp_off);
	cur_hold_line = cur_hold_line & 0xff;

	if (cur_hold_line > 0x1f)
		vpp_hold_line = 0x1f;
	else
		vpp_hold_line = cur_hold_line;

	/* Temp force set dmc */
	if (!legacy_vpp)
		WRITE_DMCREG(
			DMC_AM0_CHAN_CTRL,
			0x8ff403cf);

	/* force bypass dolby for TL1. There is no dolby function */
	if (is_meson_tl1_cpu())
		WRITE_VCBUS_REG_BITS(
			DOLBY_PATH_CTRL, 0xf, 0, 6);
	return 0;
}

static struct mconfig video_configs[] = {
	MC_PI32("pause_one_3d_fl_frame", &pause_one_3d_fl_frame),
	MC_PI32("debug_flag", &debug_flag),
	MC_PU32("force_3d_scaler", &force_3d_scaler),
	MC_PU32("video_3d_format", &video_3d_format),
	MC_PI32("vsync_enter_line_max", &vsync_enter_line_max),
	MC_PI32("vsync_exit_line_max", &vsync_exit_line_max),
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	MC_PI32("vsync_rdma_line_max", &vsync_rdma_line_max),
#endif
	MC_PU32("underflow", &underflow),
	MC_PU32("next_peek_underflow", &next_peek_underflow),
	MC_PU32("smooth_sync_enable", &smooth_sync_enable),
	MC_PU32("hdmi_in_onvideo", &hdmi_in_onvideo),
#ifdef CONFIG_AM_VIDEO2
	MC_PI32("video_play_clone_rate", &video_play_clone_rate),
	MC_PI32("android_clone_rate", &android_clone_rate),
	MC_PI32("video_play_clone_rate", &video_play_clone_rate),
	MC_PI32("android_clone_rate", &android_clone_rate),
	MC_PI32("noneseamless_play_clone_rate", &noneseamless_play_clone_rate),
#endif
	MC_PU32("smooth_sync_enable", &smooth_sync_enable),
	MC_PU32("hdmi_in_onvideo", &hdmi_in_onvideo),
	MC_PI32("cur_dev_idx", &cur_dev_idx),
	MC_PU32("new_frame_count", &new_frame_count),
	MC_PU32("omx_pts", &omx_pts),
	MC_PU32("omx_pts_set_index", &omx_pts_set_index),
	MC_PBOOL("omx_run", &omx_run),
	MC_PU32("omx_version", &omx_version),
	MC_PU32("omx_info", &omx_info),
	MC_PI32("omx_need_drop_frame_num", &omx_need_drop_frame_num),
	MC_PBOOL("omx_drop_done", &omx_drop_done),
	MC_PI32("omx_pts_interval_upper", &omx_pts_interval_upper),
	MC_PI32("omx_pts_interval_lower", &omx_pts_interval_lower),
	MC_PBOOL("bypass_pps", &bypass_pps),
	MC_PBOOL("platform_type", &platform_type),
	MC_PU32("process_3d_type", &process_3d_type),
	MC_PU32("omx_pts", &omx_pts),
	MC_PU32("framepacking_support", &framepacking_support),
	MC_PU32("framepacking_width", &framepacking_width),
	MC_PU32("framepacking_height", &framepacking_height),
	MC_PU32("framepacking_blank", &framepacking_blank),
	MC_PU32("video_seek_flag", &video_seek_flag),
	MC_PU32("slowsync_repeat_enable", &slowsync_repeat_enable),
	MC_PU32("toggle_count", &toggle_count),
	MC_PBOOL("show_first_frame_nosync", &show_first_frame_nosync),
#ifdef TV_REVERSE
	MC_PBOOL("reverse", &reverse),
#endif
};

static int amvideom_probe(struct platform_device *pdev)
{
	int ret = 0;

	video_early_init();

	DisableVideoLayer();
	DisableVideoLayer2();

	/* get interrupt resource */
	video_vsync = platform_get_irq_byname(pdev, "vsync");
	if (video_vsync  == -ENXIO) {
		pr_info("cannot get amvideom irq resource\n");

		return video_vsync;
	}

	pr_info("amvideom vsync irq: %d\n", video_vsync);

	return ret;
}

static int amvideom_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id amlogic_amvideom_dt_match[] = {
	{
		.compatible = "amlogic, amvideom",
	},
	{},
};

static struct platform_driver amvideom_driver = {
	.probe = amvideom_probe,
	.remove = amvideom_remove,
	.driver = {
		.name = "amvideom",
		.of_match_table = amlogic_amvideom_dt_match,
	}
};

static int __init video_init(void)
{
	int r = 0;
	/*
	 *#ifdef CONFIG_ARCH_MESON1
	 *ulong clk = clk_get_rate(clk_get_sys("clk_other_pll", NULL));
	 *#elif !defined(CONFIG_ARCH_MESON3) && !defined(CONFIG_ARCH_MESON6)
	 *ulong clk = clk_get_rate(clk_get_sys("clk_misc_pll", NULL));
	 *#endif
	 */

#ifdef CONFIG_ARCH_MESON1
	no to here ulong clk =
		clk_get_rate(clk_get_sys("clk_other_pll", NULL));
#elif defined(CONFIG_ARCH_MESON2)
	not to here ulong clk =
		clk_get_rate(clk_get_sys("clk_misc_pll", NULL));
#endif
	/* #if !defined(CONFIG_ARCH_MESON3) && !defined(CONFIG_ARCH_MESON6) */
#if 0				/* MESON_CPU_TYPE <= MESON_CPU_TYPE_MESON2 */
	/* MALI clock settings */
	if ((clk <= 750000000) && (clk >= 600000000)) {
		WRITE_VCBUS_REG(HHI_MALI_CLK_CNTL,
		(2 << 9) |	/* select misc pll as clock source */
		(1 << 8) |	/* enable clock gating */
		(2 << 0));	/* Misc clk / 3 */
	} else {
		WRITE_VCBUS_REG(HHI_MALI_CLK_CNTL,
		(3 << 9) |	/* select DDR clock as clock source */
		(1 << 8) |	/* enable clock gating */
		(1 << 0));	/* DDR clk / 2 */
	}
#endif

	if (is_meson_g12a_cpu() || is_meson_g12b_cpu()
		|| is_meson_tl1_cpu()) {
		cur_dev->viu_off = 0x3200 - 0x1a50;
		legacy_vpp = false;
	}
	if (platform_driver_register(&amvideom_driver)) {
		pr_info("failed to amvideom driver!\n");
		return -ENODEV;
	}

	/* check super scaler support status */
	vpp_super_scaler_support();
	/* adaptive config bypass ratio */
	vpp_bypass_ratio_config();

#ifndef CONFIG_AM_VIDEO2
	/*DisableVPP2VideoLayer();*/
#endif

	cur_dispbuf = NULL;
	cur_dispbuf2 = NULL;
	amvideo_register_client(&amvideo_notifier);

#ifdef FIQ_VSYNC
	/* enable fiq bridge */
	vsync_fiq_bridge.handle = vsync_bridge_isr;
	vsync_fiq_bridge.key = (u32) vsync_bridge_isr;
	vsync_fiq_bridge.name = "vsync_bridge_isr";

	r = register_fiq_bridge_handle(&vsync_fiq_bridge);

	if (r) {
		amlog_level(LOG_LEVEL_ERROR,
			    "video fiq bridge register error.\n");
		r = -ENOENT;
		goto err0;
	}
#endif

    /* sysfs node creation */
	r = class_register(&amvideo_poll_class);
	if (r) {
		amlog_level(LOG_LEVEL_ERROR, "create video_poll class fail.\n");
#ifdef FIQ_VSYNC
		free_irq(BRIDGE_IRQ, (void *)video_dev_id);
#else
		free_irq(INT_VIU_VSYNC, (void *)video_dev_id);
#endif
		goto err1;
	}

	r = class_register(&amvideo_class);
	if (r) {
		amlog_level(LOG_LEVEL_ERROR, "create video class fail.\n");
#ifdef FIQ_VSYNC
		free_irq(BRIDGE_IRQ, (void *)video_dev_id);
#else
		free_irq(INT_VIU_VSYNC, (void *)video_dev_id);
#endif
		goto err1;
	}

	/* create video device */
	r = register_chrdev(AMVIDEO_MAJOR, "amvideo", &amvideo_fops);
	if (r < 0) {
		amlog_level(LOG_LEVEL_ERROR,
			    "Can't register major for amvideo device\n");
		goto err2;
	}

	r = register_chrdev(0, "amvideo_poll", &amvideo_poll_fops);
	if (r < 0) {
		amlog_level(LOG_LEVEL_ERROR,
			"Can't register major for amvideo_poll device\n");
		goto err3;
	}

	amvideo_poll_major = r;

	amvideo_dev = device_create(&amvideo_class, NULL,
		MKDEV(AMVIDEO_MAJOR, 0), NULL, DEVICE_NAME);

	if (IS_ERR(amvideo_dev)) {
		amlog_level(LOG_LEVEL_ERROR, "Can't create amvideo device\n");
		goto err4;
	}

	amvideo_poll_dev = device_create(&amvideo_poll_class, NULL,
		MKDEV(amvideo_poll_major, 0), NULL, "amvideo_poll");

	if (IS_ERR(amvideo_poll_dev)) {
		amlog_level(LOG_LEVEL_ERROR,
			"Can't create amvideo_poll device\n");
		goto err5;
	}

	init_waitqueue_head(&amvideo_trick_wait);
	init_waitqueue_head(&amvideo_sizechange_wait);
#if 1				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	INIT_WORK(&vpu_delay_work, do_vpu_delay_work);
#endif

#ifdef CONFIG_AM_VOUT
	vout_hook();
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	dispbuf_to_put_num = DISPBUF_TO_PUT_MAX;
	while (dispbuf_to_put_num > 0) {
		dispbuf_to_put_num--;
		dispbuf_to_put[dispbuf_to_put_num] = NULL;
	}

	disp_canvas[0][0] =
	    (disp_canvas_index[0][2] << 16) | (disp_canvas_index[0][1] << 8) |
	    disp_canvas_index[0][0];
	disp_canvas[0][1] =
	    (disp_canvas_index[0][5] << 16) | (disp_canvas_index[0][4] << 8) |
	    disp_canvas_index[0][3];

	disp_canvas[1][0] =
	    (disp_canvas_index[1][2] << 16) | (disp_canvas_index[1][1] << 8) |
	    disp_canvas_index[1][0];
	disp_canvas[1][1] =
	    (disp_canvas_index[1][5] << 16) | (disp_canvas_index[1][4] << 8) |
	    disp_canvas_index[1][3];
#else

	disp_canvas[0] =
	    (disp_canvas_index[2] << 16) | (disp_canvas_index[1] << 8) |
	    disp_canvas_index[0];
	disp_canvas[1] =
	    (disp_canvas_index[5] << 16) | (disp_canvas_index[4] << 8) |
	    disp_canvas_index[3];
#endif

	vsync_fiq_up();
#ifdef CONFIG_SUPPORT_VIDEO_ON_VPP2
	vsync2_fiq_up();
#endif

	vf_receiver_init(&video_vf_recv, RECEIVER_NAME, &video_vf_receiver,
			 NULL);
	vf_reg_receiver(&video_vf_recv);

	vf_receiver_init(&video4osd_vf_recv, RECEIVER4OSD_NAME,
			 &video4osd_vf_receiver, NULL);
	vf_reg_receiver(&video4osd_vf_recv);
#if DEBUG_TMP
	switch_dev_register(&video1_state_sdev);
	switch_set_state(&video1_state_sdev, 0);
#endif
	video_keeper_init();
#ifdef CONFIG_AM_VIDEO2
	set_clone_frame_rate(android_clone_rate, 0);
#endif
	REG_PATH_CONFIGS("media.video", video_configs);
	video_debugfs_init();
	return 0;
 err5:
	device_destroy(&amvideo_class, MKDEV(AMVIDEO_MAJOR, 0));
 err4:
	unregister_chrdev(amvideo_poll_major, "amvideo_poll");
 err3:
	unregister_chrdev(AMVIDEO_MAJOR, DEVICE_NAME);

 err2:
#ifdef FIQ_VSYNC
	unregister_fiq_bridge_handle(&vsync_fiq_bridge);
#endif
	class_unregister(&amvideo_class);
 err1:
	class_unregister(&amvideo_poll_class);
#ifdef FIQ_VSYNC
 err0:
#endif
	amvideo_unregister_client(&amvideo_notifier);
	platform_driver_unregister(&amvideom_driver);

	return r;
}


static void __exit video_exit(void)
{
	video_debugfs_exit();
	vf_unreg_receiver(&video_vf_recv);

	vf_unreg_receiver(&video4osd_vf_recv);
	DisableVideoLayer();
	DisableVideoLayer2();

	vsync_fiq_down();
#ifdef CONFIG_SUPPORT_VIDEO_ON_VPP2
	vsync2_fiq_down();
#endif
	device_destroy(&amvideo_class, MKDEV(AMVIDEO_MAJOR, 0));
	device_destroy(&amvideo_poll_class, MKDEV(amvideo_poll_major, 0));

	unregister_chrdev(AMVIDEO_MAJOR, DEVICE_NAME);
	unregister_chrdev(amvideo_poll_major, "amvideo_poll");

#ifdef FIQ_VSYNC
	unregister_fiq_bridge_handle(&vsync_fiq_bridge);
#endif

	class_unregister(&amvideo_class);
	class_unregister(&amvideo_poll_class);
	amvideo_unregister_client(&amvideo_notifier);
}



MODULE_PARM_DESC(debug_flag, "\n debug_flag\n");
module_param(debug_flag, uint, 0664);

#ifdef TV_3D_FUNCTION_OPEN
MODULE_PARM_DESC(force_3d_scaler, "\n force_3d_scaler\n");
module_param(force_3d_scaler, uint, 0664);

MODULE_PARM_DESC(video_3d_format, "\n video_3d_format\n");
module_param(video_3d_format, uint, 0664);

#endif

MODULE_PARM_DESC(vsync_enter_line_max, "\n vsync_enter_line_max\n");
module_param(vsync_enter_line_max, uint, 0664);

MODULE_PARM_DESC(vsync_exit_line_max, "\n vsync_exit_line_max\n");
module_param(vsync_exit_line_max, uint, 0664);

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
MODULE_PARM_DESC(vsync_rdma_line_max, "\n vsync_rdma_line_max\n");
module_param(vsync_rdma_line_max, uint, 0664);
#endif

module_param(underflow, uint, 0664);
MODULE_PARM_DESC(underflow, "\n Underflow count\n");

module_param(next_peek_underflow, uint, 0664);
MODULE_PARM_DESC(skip, "\n Underflow count\n");

module_param(hdmiin_frame_check, uint, 0664);
MODULE_PARM_DESC(hdmiin_frame_check, "\n hdmiin_frame_check\n");

module_param(step_enable, uint, 0664);
MODULE_PARM_DESC(step_enable, "\n step_enable\n");

module_param(step_flag, uint, 0664);
MODULE_PARM_DESC(step_flag, "\n step_flag\n");

/*arch_initcall(video_early_init);*/

module_init(video_init);
module_exit(video_exit);

MODULE_PARM_DESC(smooth_sync_enable, "\n smooth_sync_enable\n");
module_param(smooth_sync_enable, uint, 0664);

MODULE_PARM_DESC(hdmi_in_onvideo, "\n hdmi_in_onvideo\n");
module_param(hdmi_in_onvideo, uint, 0664);

#ifdef CONFIG_AM_VIDEO2
MODULE_PARM_DESC(video_play_clone_rate, "\n video_play_clone_rate\n");
module_param(video_play_clone_rate, uint, 0664);

MODULE_PARM_DESC(android_clone_rate, "\n android_clone_rate\n");
module_param(android_clone_rate, uint, 0664);

MODULE_PARM_DESC(noneseamless_play_clone_rate,
		 "\n noneseamless_play_clone_rate\n");
module_param(noneseamless_play_clone_rate, uint, 0664);

#endif
MODULE_PARM_DESC(vsync_count, "\n vsync_count\n");
module_param(vsync_count, uint, 0664);


MODULE_PARM_DESC(cur_dev_idx, "\n cur_dev_idx\n");
module_param(cur_dev_idx, uint, 0664);

MODULE_PARM_DESC(new_frame_count, "\n new_frame_count\n");
module_param(new_frame_count, uint, 0664);

MODULE_PARM_DESC(first_frame_toggled, "\n first_frame_toggled\n");
module_param(first_frame_toggled, uint, 0664);

MODULE_PARM_DESC(omx_pts, "\n omx_pts\n");
module_param(omx_pts, uint, 0664);

MODULE_PARM_DESC(omx_run, "\n omx_run\n");
module_param(omx_run, bool, 0664);

MODULE_PARM_DESC(omx_pts_set_index, "\n omx_pts_set_index\n");
module_param(omx_pts_set_index, uint, 0664);

MODULE_PARM_DESC(omx_version, "\n omx_version\n");
module_param(omx_version, uint, 0664);

MODULE_PARM_DESC(omx_info, "\n omx_info\n");
module_param(omx_info, uint, 0664);

MODULE_PARM_DESC(omx_need_drop_frame_num, "\n omx_need_drop_frame_num\n");
module_param(omx_need_drop_frame_num, int, 0664);

MODULE_PARM_DESC(omx_drop_done, "\n omx_drop_done\n");
module_param(omx_drop_done, bool, 0664);

MODULE_PARM_DESC(omx_pts_interval_upper, "\n omx_pts_interval\n");
module_param(omx_pts_interval_upper, int, 0664);

MODULE_PARM_DESC(omx_pts_interval_lower, "\n omx_pts_interval\n");
module_param(omx_pts_interval_lower, int, 0664);

MODULE_PARM_DESC(drop_frame_count, "\n drop_frame_count\n");
module_param(drop_frame_count, int, 0664);

MODULE_PARM_DESC(receive_frame_count, "\n receive_frame_count\n");
module_param(receive_frame_count, int, 0664);

MODULE_PARM_DESC(display_frame_count, "\n display_frame_count\n");
module_param(display_frame_count, int, 0664);

MODULE_PARM_DESC(bypass_pps, "\n pps_bypass\n");
module_param(bypass_pps, bool, 0664);

MODULE_PARM_DESC(platform_type, "\n platform_type\n");
module_param(platform_type, bool, 0664);

MODULE_PARM_DESC(process_3d_type, "\n process_3d_type\n");
module_param(process_3d_type, uint, 0664);


MODULE_PARM_DESC(framepacking_support, "\n framepacking_support\n");
module_param(framepacking_support, uint, 0664);

MODULE_PARM_DESC(framepacking_width, "\n framepacking_width\n");
module_param(framepacking_width, uint, 0664);

MODULE_PARM_DESC(framepacking_height, "\n framepacking_height\n");
module_param(framepacking_height, uint, 0664);

MODULE_PARM_DESC(framepacking_blank, "\n framepacking_blank\n");
module_param(framepacking_blank, uint, 0664);

MODULE_PARM_DESC(bypass_cm, "\n bypass_cm\n");
module_param(bypass_cm, bool, 0664);

#ifdef TV_REVERSE
module_param(reverse, bool, 0644);
MODULE_PARM_DESC(reverse, "reverse /disable reverse");
#endif

MODULE_PARM_DESC(toggle_count, "\n toggle count\n");
module_param(toggle_count, uint, 0664);

MODULE_PARM_DESC(vpp_hold_line, "\n vpp_hold_line\n");
module_param(vpp_hold_line, uint, 0664);

MODULE_DESCRIPTION("AMLOGIC video output driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
