/*
 * drivers/amlogic/media/video_processor/video_dev/amlvideo2.c
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>
#include <media/videobuf-core.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <linux/types.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include "common/vfp-queue.h"
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/v4l_util/videobuf-res.h>
#include <linux/of_reserved_mem.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include "amlvideo2.h"

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
/* #include <mach/mod_gate.h> */
/* #endif */

#include <linux/of.h>
#include <linux/of_fdt.h>


#include <linux/semaphore.h>
#include <linux/sched/rt.h>

#define AVMLVIDEO2_MODULE_NAME "amlvideo2"
#define AMLVIDEO2_RES_CANVAS 0xc0
#define AMLVIDEO2_1_RES_CANVAS 0xcc

#define MULTI_NODE
/* #define USE_SEMA_QBUF */
/* #define USE_VDIN_PTS */

/* #define MULTI_NODE */
#ifdef MULTI_NODE
#define MAX_SUB_DEV_NODE 2
#else
#define MAX_SUB_DEV_NODE 1
#endif

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001

#define AMLVIDEO2_MAJOR_VERSION 0
#define AMLVIDEO2_MINOR_VERSION 7
#define AMLVIDEO2_RELEASE 1
#define AMLVIDEO2_VERSION \
KERNEL_VERSION(\
	       AMLVIDEO2_MAJOR_VERSION, AMLVIDEO2_MINOR_VERSION,\
	       AMLVIDEO2_RELEASE)

#define MAGIC_SG_MEM 0x17890714
#define MAGIC_DC_MEM 0x0733ac61
#define MAGIC_VMAL_MEM 0x18221223
#define MAGIC_RE_MEM 0x123039dc

#ifdef MULTI_NODE
#define DEVICE_NAME0 "amlvideo2.0"
#define DEVICE_NAME1 "amlvideo2.1"
#else
#define RECEIVER_NAME "amlvideo2.0"
#define DEVICE_NAME   "amlvideo2.0"
#endif

#define AMLVIDEO2_RES0_CANVAS_INDEX AMLVIDEO2_RES_CANVAS
#define AMLVIDEO2_RES1_CANVAS_INDEX AMLVIDEO2_1_RES_CANVAS

#define DUR2PTS(x) ((x) - ((x) >> 4))
#define DUR2PTS_RM(x) ((x) & 0xf)

#define CMA_ALLOC_SIZE 24

#define CANVAS_WIDTH_ALIGN 32

MODULE_DESCRIPTION(
	"pass a frame of amlogic video2 codec device  to user in style of v4l2");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL");
static unsigned int video_nr = 11;
/* module_param(video_nr, uint, 0644); */
/* MODULE_PARM_DESC(video_nr, "videoX start number, 10 is defaut"); */

static unsigned int debug;
/* module_param(debug, uint, 0644); */
/* MODULE_PARM_DESC(debug, "activates debug info"); */

#define DEF_FRAMERATE 30

static unsigned int vid_limit = 32;
module_param(vid_limit, uint, 0644);
MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");

static unsigned int amlvideo2_dbg_en;
module_param(amlvideo2_dbg_en, uint, 0664);
MODULE_PARM_DESC(amlvideo2_dbg_en, "enable/disable amlvideo2 debug information");

static unsigned int amlvideo2_scaledown1 = 2;
module_param(amlvideo2_scaledown1, uint, 0664);
MODULE_PARM_DESC(amlvideo2_scaledown1, "amlvideo2_scaledown1");
static unsigned int amlvideo2_scaledown2 = 2;
module_param(amlvideo2_scaledown2, uint, 0664);
MODULE_PARM_DESC(amlvideo2_scaledown2, "amlvideo2_scaledown2");


static struct v4l2_fract amlvideo2_frmintervals_active = {
	.numerator = 1, .denominator = DEF_FRAMERATE, };

/* supported controls */
static struct v4l2_queryctrl amlvideo2_node_qctrl[] = {{
	.id = V4L2_CID_ROTATE, .type = V4L2_CTRL_TYPE_INTEGER, .name = "Rotate",
	.minimum = 0, .maximum = 270, .step = 90, .default_value = 0, .flags =
		V4L2_CTRL_FLAG_SLIDER, } };

static struct v4l2_frmivalenum amlvideo2_frmivalenum[] = {{
	.index = 0, .pixel_format = V4L2_PIX_FMT_NV21, .width = 1920, .height =
		1080, .type = V4L2_FRMIVAL_TYPE_DISCRETE, {.discrete = {
		.numerator = 1, .denominator = 30, } } }, {
	.index = 1, .pixel_format = V4L2_PIX_FMT_NV21, .width = 1600, .height =
		1200, .type = V4L2_FRMIVAL_TYPE_DISCRETE, {.discrete = {
		.numerator = 1, .denominator = 5, } } }, };
#define dpr_err(dev, level, fmt, arg...) \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)

/* ------------------------------------------------------------------
 * Basic structures
 * ------------------------------------------------------------------
 */

enum aml_provider_type_e {
	AML_PROVIDE_NONE = 0,
	AML_PROVIDE_VDIN0 = 1,
	AML_PROVIDE_VDIN1 = 2,
	AML_PROVIDE_DECODE = 3,
	AML_PROVIDE_PPMGR = 4,
	AML_PROVIDE_MAX = 5
};

enum aml_receiver_type_e {
	AML_RECEIVER_NONE = 0,
	AML_RECEIVER_PPMGR,
	AML_RECEIVER_DI,
	AML_RECEIVER_MAX
};

enum aml_screen_mode_e {
	AML_SCREEN_MODE_RATIO = 0,
	AML_SCREEN_MODE_FULL,
	AML_SCREEN_MODE_ADAPTIVE,
	AML_SCREEN_MODE_MAX
};

struct amlvideo2_fmt {
	char *name;
	u32 fourcc; /* v4l2 format id */
	int depth;
};

static struct amlvideo2_fmt formats[] = {
{.name = "RGB565 (BE)",
.fourcc = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
.depth = 16, },

{.name = "RGB888 (24)",
.fourcc = V4L2_PIX_FMT_RGB24, /* 24  RGB-8-8-8 */
.depth = 24, },

{.name = "BGR888 (24)",
.fourcc = V4L2_PIX_FMT_BGR24, /* 24  BGR-8-8-8 */
.depth = 24, },

{.name = "RGBA888 (32)",
.fourcc = V4L2_PIX_FMT_RGB32, /* 32  RGBA-8-8-8 */
.depth = 32, },

{.name = "12  Y/CbCr 4:2:0",
.fourcc = V4L2_PIX_FMT_NV12,
.depth = 12, },

{.name = "12  Y/CbCr 4:2:0",
.fourcc = V4L2_PIX_FMT_NV21,
.depth = 12, },

{.name = "YUV420P",
.fourcc = V4L2_PIX_FMT_YUV420,
.depth = 12, },

{.name = "YVU420P",
.fourcc = V4L2_PIX_FMT_YVU420,
.depth = 12, }

#if 0
	{
		.name = "RGBA8888 (32)",
		.fourcc = V4L2_PIX_FMT_RGB32, /* 24  RGBA-8-8-8-8 */
		.depth = 32,
	},
	{
		.name = "RGB565 (BE)",
		.fourcc = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
		.depth = 16,
	},
	{
		.name = "BGR888 (24)",
		.fourcc = V4L2_PIX_FMT_BGR24, /* 24  BGR-8-8-8 */
		.depth = 24,
	},
	{
		.name = "YUV420P",
		.fourcc = V4L2_PIX_FMT_YUV420,
		.depth = 12,
	},
#endif
};

static struct amlvideo2_fmt *get_format(struct v4l2_format *f)
{
	struct amlvideo2_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(formats))
		return NULL;

	return &formats[k];
}

/* buffer for one video frame */
struct amlvideo2_frame_info {
	int x;
	int y;
	int w;
	int h;
};

struct amlvideo2_latency_info {
	int total_latency;
	int total_latency_out;
	long long cur_time;
	long long cur_time_out;
	int frame_num;
	int frame_num_out;
	struct timeval test_time;
};

struct amlvideo2_node_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct amlvideo2_fmt *fmt;
	int canvas_id;
	struct amlvideo2_frame_info axis;
};

struct amlvideo2_node_dmaqueue {
	struct list_head active;

	/* thread for generating video stream*/
	struct task_struct *kthread;
	wait_queue_head_t wq;
	unsigned char task_running;
#ifdef USE_SEMA_QBUF
	struct completion qbuf_comp;
#endif
};

struct amlvideo2_device {
struct mutex mutex;
struct v4l2_device v4l2_dev;
struct platform_device *pdev;
struct amlvideo2_node *node[MAX_SUB_DEV_NODE];
int node_num;
resource_size_t buffer_start;
unsigned int buffer_size;
struct page *cma_pages;
struct resource memobj;
int cma_mode;
int node_id;
bool use_reserve;
};

struct crop_info_s {
	int source_top_crop;
	int source_left_crop;
	int source_width_crop;
	int source_height_crop;
	int capture_crop_enable;
};

struct screen_display_info_s {
	struct vdisplay_info_s display_info;
	enum aml_screen_mode_e mode;
};

struct amlvideo2_fh;
struct amlvideo2_node {

spinlock_t slock;
struct mutex mutex;
int vid;
int users;

struct amlvideo2_device *vid_dev;

/* various device info */
struct video_device *vfd;

struct amlvideo2_node_dmaqueue vidq;

/* Control 'registers' */
int qctl_regs[ARRAY_SIZE(amlvideo2_node_qctrl)];

struct videobuf_res_privdata res;
struct vframe_receiver_s recv;
struct vframe_receiver_s *sub_recv;
struct vframe_provider_s *provider;
enum aml_provider_type_e p_type;
enum aml_receiver_type_e r_type;
int provide_ready;

struct amlvideo2_fh *fh;
unsigned int input; /* 0:mirrocast; 1:hdmiin */
enum tvin_port_e porttype;
unsigned int start_vdin_flag;
struct ge2d_context_s *context;
struct vdin_v4l2_ops_s vops;
int vdin_device_num;
struct vdisplay_info_s display_info;
struct crop_info_s crop_info;
enum aml_screen_mode_e mode;
bool has_amvideo_node;

struct vframe_provider_s amlvideo2_vf_prov;

int amlvideo2_pool_size;
struct vfq_s q_ready;
struct vframe_s *amlvideo2_pool_ready;

bool video_blocking;
struct timeval thread_ts1;
struct timeval thread_ts2;
int frameInv_adjust;
int frameInv;
struct vframe_s *tmp_vf;
int frame_inittime;
struct amlvideo2_latency_info latency_info;
bool pflag;
struct completion plug_sema;
bool field_flag;
bool field_condition_flag;
bool ge2d_multi_process_flag;
};

struct amlvideo2_fh {
struct amlvideo2_node *node;

/* video capture */
struct amlvideo2_fmt *fmt;
unsigned int width, height;
unsigned int src_width, src_height;
bool set_format_flag;
struct videobuf_queue vb_vidq;
unsigned int is_streamed_on;

suseconds_t frm_save_time_us; /* us */
unsigned int f_flags;
enum v4l2_buf_type type;
};

struct amlvideo2_output {
int canvas_id;
void *vbuf;
int width;
int height;
u32 v4l2_format;
int angle;
struct screen_display_info_s info;
struct amlvideo2_frame_info *frame;
};

static struct v4l2_frmsize_discrete amlvideo2_prev_resolution[] = {{160, 120}, {
320, 240}, {640, 480}, {1280, 720}, };

static struct v4l2_frmsize_discrete amlvideo2_pic_resolution[] = {{1280, 960} };

static unsigned int print_cvs_idx;
module_param(print_cvs_idx, uint, 0644);
MODULE_PARM_DESC(print_cvs_idx, "print canvas index\n");

int get_amlvideo2_canvas_index(struct amlvideo2_output *output,
				int start_canvas)
{
int canvas = start_canvas;
int v4l2_format = output->v4l2_format;
void *buf = (void *)output->vbuf;
int width = output->width;
int height = output->height;
int canvas_height = height;

switch (v4l2_format) {
case V4L2_PIX_FMT_RGB565X:
case V4L2_PIX_FMT_VYUY:
	canvas = start_canvas;
	canvas_config(canvas, (unsigned long)buf, width * 2, canvas_height,
	CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	break;
case V4L2_PIX_FMT_YUV444:
case V4L2_PIX_FMT_BGR24:
case V4L2_PIX_FMT_RGB24:
	canvas = start_canvas;
	canvas_config(canvas, (unsigned long)buf, width * 3, canvas_height,
	CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	break;
case V4L2_PIX_FMT_RGB32:
	canvas = start_canvas;
	canvas_config(canvas, (unsigned long)buf, width * 4, canvas_height,
	CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	break;
case V4L2_PIX_FMT_NV12:
case V4L2_PIX_FMT_NV21:
	canvas_config(start_canvas, (unsigned long)buf, width, canvas_height,
	CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	canvas_config(start_canvas + 1, (unsigned long)(buf + width * height),
			width, canvas_height / 2,
			CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	canvas = start_canvas | ((start_canvas + 1) << 8);
	break;
case V4L2_PIX_FMT_YVU420:
case V4L2_PIX_FMT_YUV420:
	canvas_config(start_canvas, (unsigned long)buf, width, canvas_height,
	CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	canvas_config(start_canvas + 1, (unsigned long)(buf + width * height),
			width / 2, canvas_height / 2,
			CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	canvas_config(start_canvas + 2,
			(unsigned long)(buf + width * height * 5 / 4),
			width / 2, canvas_height / 2,
			CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	if (v4l2_format == V4L2_PIX_FMT_YUV420) {
		canvas = start_canvas | ((start_canvas + 1) << 8)
				| ((start_canvas + 2) << 16);
	} else {
		canvas = start_canvas | ((start_canvas + 2) << 8)
				| ((start_canvas + 1) << 16);
	}
	break;
default:
	break;
}
if (print_cvs_idx == 1) {
	pr_err("v4l2_format=%x, canvas=%x\n", v4l2_format, canvas);
	print_cvs_idx = 0;
}
return canvas;
}
/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
int convert_canvas_index(struct amlvideo2_output *output, int start_canvas)
{
	int canvas = start_canvas;
	int v4l2_format = output->v4l2_format;

	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_VYUY:
		canvas = start_canvas;
		break;
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_RGB32:
		canvas = start_canvas;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		canvas = start_canvas | ((start_canvas + 1) << 8);
		break;
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420:
		if (v4l2_format == V4L2_PIX_FMT_YUV420) {
			canvas = start_canvas | ((start_canvas + 1) << 8)
					| ((start_canvas + 2) << 16);
		} else {
			canvas = start_canvas | ((start_canvas + 2) << 8)
					| ((start_canvas + 1) << 16);
		}
		break;
	default:
		break;
	}
	return canvas;
}
/* #endif */

static unsigned int print_ifmt;
module_param(print_ifmt, uint, 0644);
MODULE_PARM_DESC(print_ifmt, "print input format\n");

static int get_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;

	if (vf->type & VIDTYPE_VIU_422) {
		if (vf->type & VIDTYPE_INTERLACE_BOTTOM) {
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422B & (3 << 3));
		} else if (vf->type & VIDTYPE_INTERLACE_TOP) {
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		} else {
			format = GE2D_FORMAT_S16_YUV422;
		}
	} else if (vf->type & VIDTYPE_VIU_NV21) {
		if (vf->type & VIDTYPE_INTERLACE_BOTTOM) {
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21B & (3 << 3));
		} else if (vf->type & VIDTYPE_INTERLACE_TOP) {
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		} else {
			format = GE2D_FORMAT_M24_NV21;
		}
	} else {
		if (vf->type & VIDTYPE_INTERLACE_BOTTOM) {
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420B & (3 << 3));
		} else if (vf->type & VIDTYPE_INTERLACE_TOP) {
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		} else {
			format = GE2D_FORMAT_M24_YUV420;
		}
	}
	if (print_ifmt == 1) {
		pr_err(
		"vf->type=%x, format=%x, w*h=%dx%d, canvas0=%x, canvas1=%x\n",
		vf->type, format, vf->width, vf->height, vf->canvas0Addr,
		vf->canvas1Addr);

		pr_err(
	"vf->type=%x, VIDTYPE_INTERLACE_BOTTOM=%x, VIDTYPE_INTERLACE_TOP=%x\n",
	vf->type, VIDTYPE_INTERLACE_BOTTOM, VIDTYPE_INTERLACE_TOP);
		print_ifmt = 0;
	}
	return format;
}

static int get_input_format_no_interlace(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;

	if (vf->type & VIDTYPE_VIU_422)
		format = GE2D_FORMAT_S16_YUV422;
	else if (vf->type & VIDTYPE_VIU_NV21)
		format = GE2D_FORMAT_M24_NV21;
	else
		format = GE2D_FORMAT_M24_YUV420;

	if (print_ifmt == 1) {
		pr_err(
		"vf->type=%x, format=%x, w*h=%dx%d, canvas0=%x, canvas1=%x\n",
		vf->type, format, vf->width, vf->height, vf->canvas0Addr,
		vf->canvas1Addr);

		pr_err(
	"vf->type=%x, VIDTYPE_INTERLACE_BOTTOM=%x, VIDTYPE_INTERLACE_TOP=%x\n",
	vf->type, VIDTYPE_INTERLACE_BOTTOM, VIDTYPE_INTERLACE_TOP);
		print_ifmt = 0;
	}
	return format;
}

static int get_interlace_input_format(struct vframe_s *vf,
					struct amlvideo2_output *output)
{
	int format = GE2D_FORMAT_M24_NV21;

	if (vf->type & VIDTYPE_VIU_422) {
		format = GE2D_FORMAT_S16_YUV422;
		if (vf->height >= output->height << 2) {
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		}
	} else if (vf->type & VIDTYPE_VIU_NV21) {
		format = GE2D_FORMAT_M24_NV21;
		if (vf->height >= output->height << 2) {
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		}
	} else {
		format = GE2D_FORMAT_M24_YUV420;
		if (vf->height >= output->height << 2) {
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		}
	}
	if (print_ifmt == 1) {
		pr_err(
		"vf->type=%x, format=%x, w*h=%dx%d, canvas0=%x, canvas1=%x\n",
		vf->type, format, vf->width, vf->height, vf->canvas0Addr,
		vf->canvas1Addr);
		print_ifmt = 0;
	}
	return format;
}

static inline int get_top_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;

	if (vf->type & VIDTYPE_VIU_422)
		format = GE2D_FORMAT_S16_YUV422 |
			(GE2D_FORMAT_S16_YUV422T & (3 << 3));
	else if (vf->type & VIDTYPE_VIU_NV21)
		format = GE2D_FORMAT_M24_NV21 |
			(GE2D_FORMAT_M24_NV21T & (3 << 3));
	else
		format = GE2D_FORMAT_M24_YUV420 |
			(GE2D_FORMAT_M24_YUV420T & (3 << 3));

	if (print_ifmt == 1) {
		pr_err(
		"vf->type=%x, format=%x, w*h=%dx%d, canvas0=%x, canvas1=%x\n",
		vf->type, format, vf->width, vf->height, vf->canvas0Addr,
		vf->canvas1Addr);
		print_ifmt = 0;
	}
	return format;
}

static inline int get_bottom_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;

	if (vf->type & VIDTYPE_VIU_422)
		format = GE2D_FORMAT_S16_YUV422 |
			(GE2D_FORMAT_S16_YUV422B & (3 << 3));
	else if (vf->type & VIDTYPE_VIU_NV21)
		format = GE2D_FORMAT_M24_NV21 |
			(GE2D_FORMAT_M24_NV21B & (3 << 3));
	else
		format =
			GE2D_FORMAT_M24_YUV420 |
			(GE2D_FMT_M24_YUV420B & (3 << 3));


	if (print_ifmt == 1) {
		pr_err(
		"vf->type=%x, format=%x, w*h=%dx%d, canvas0=%x, canvas1=%x\n",
		vf->type, format, vf->width, vf->height, vf->canvas0Addr,
		vf->canvas1Addr);
		print_ifmt = 0;
	}
	return format;
}

static unsigned int print_ofmt;
module_param(print_ofmt, uint, 0644);
MODULE_PARM_DESC(print_ofmt, "print output format\n");

static int get_output_format(int v4l2_format)
{
	int format = GE2D_FORMAT_S24_YUV444;

	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
		format = GE2D_FORMAT_S16_RGB_565;
		break;
	case V4L2_PIX_FMT_YUV444:
		format = GE2D_FORMAT_S24_YUV444;
		break;
	case V4L2_PIX_FMT_VYUY:
		format = GE2D_FORMAT_S16_YUV422;
		break;
	case V4L2_PIX_FMT_BGR24:
		format = GE2D_FORMAT_S24_RGB;
		break;
	case V4L2_PIX_FMT_RGB24:
		format = GE2D_FORMAT_S24_BGR;
		break;
	case V4L2_PIX_FMT_RGB32:
		format = GE2D_FORMAT_S32_ABGR;
		break;
	case V4L2_PIX_FMT_NV12:
		format = GE2D_FORMAT_M24_NV12;
		break;
	case V4L2_PIX_FMT_NV21:
		format = GE2D_FORMAT_M24_NV21;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		format = GE2D_FORMAT_S8_Y;
		break;
	default:
		break;
	}
	if (print_ofmt == 1) {
		pr_err("outputformat, v4l2_format=%x, format=%x\n", v4l2_format,
			format);
		print_ofmt = 0;
	}
	return format;
}
static void src_axis_adjust(int  *src_top,  int  *src_left,
				int  *src_w,  int  *src_h,
				struct amlvideo2_output *output)
{
	*src_top = output->info.display_info.frame_vd_start_lines_;
	*src_left = output->info.display_info.frame_hd_start_lines_;
	*src_w = output->info.display_info.frame_hd_end_lines_ -
	output->info.display_info.frame_hd_start_lines_ + 1;
	*src_h = output->info.display_info.frame_vd_end_lines_ -
	output->info.display_info.frame_vd_start_lines_ + 1;
}

static void output_axis_adjust(int src_w, int src_h, int *dst_w, int *dst_h,
				int angle, struct amlvideo2_output *output)
{
	int w = 0, h = 0, disp_w = 0, disp_h = 0;
	int post_blend_w = 0, post_blend_h = 0, window_w = 0, window_h = 0;

	disp_w = *dst_w;
	disp_h = *dst_h;
	if (output != NULL) {
		if (output->info.mode == AML_SCREEN_MODE_RATIO) {
			if (angle % 180 != 0) {
				h = min_t(int, (int)src_w, disp_h);
				w = src_h * h / src_w;
				if (w > disp_w) {
					h = (h * disp_w) / w;
					w = disp_w;
				}
			} else {
				if ((src_w < disp_w) && (src_h < disp_h)) {
					w = src_w;
					h = src_h;
				} else if ((src_w * disp_h) >
					(disp_w * src_h)) {
					w = disp_w;
					h = disp_w * src_h / src_w;
				} else {
					h = disp_h;
					w = disp_h * src_w / src_h;
				}
			}
			*dst_w = w;
			*dst_h = h;
		} else  if (output->info.mode == AML_SCREEN_MODE_FULL) {
			h = disp_h;
			w = disp_w;
			*dst_w = w;
			*dst_h = h;
		} else if (output->info.mode == AML_SCREEN_MODE_ADAPTIVE)  {
			post_blend_w =
			output->info.display_info.screen_vd_h_end_ -
			output->info.display_info.screen_vd_h_start_ + 1;
			post_blend_h =
			output->info.display_info.screen_vd_v_end_ -
			output->info.display_info.screen_vd_v_start_ + 1;
			window_w = output->info.display_info.display_hsc_endp -
			output->info.display_info.display_hsc_startp + 1;
			window_h = output->info.display_info.display_vsc_endp -
			output->info.display_info.display_vsc_startp + 1;
			*dst_w = (window_w * disp_w) / post_blend_w;
			*dst_h = (window_h * disp_h) / post_blend_h;
		}
	} else {
			if (angle % 180 != 0) {
				h = min_t(int, (int)src_w, disp_h);
				w = src_h * h / src_w;
				if (w > disp_w) {
					h = (h * disp_w) / w;
					w = disp_w;
				}
			} else {
				if ((src_w < disp_w) && (src_h < disp_h)) {
					w = src_w;
					h = src_h;
				} else if ((src_w * disp_h) >
					(disp_w * src_h)) {
					w = disp_w;
					h = disp_w * src_h / src_w;
				} else {
					h = disp_h;
					w = disp_h * src_w / src_h;
				}
			}
			*dst_w = w;
			*dst_h = h;
	}
}

int amlvideo2_ge2d_interlace_two_canvasAddr_process(
struct vframe_s *vf, struct ge2d_context_s *context,
struct config_para_ex_s *ge2d_config, struct amlvideo2_output *output,
struct amlvideo2_node *node)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror = 0;
	int cur_angle = 0;
	int output_canvas = vf->canvas0Addr;

	/* ============================ */
	/* top field */
	/* ============================ */
	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	/* src_height = vf->height/2; */
	src_height = vf->height;
	if (amlvideo2_dbg_en & 4) {
		pr_info("vf->width = %d, vf->height = %d\n",
			vf->width, vf->height);
		pr_info("vf->type = %x, vf->src_canvas = %x\n",
			vf->type, vf->canvas0Addr);
		pr_info("crop_enable = %d\n",
			node->crop_info.capture_crop_enable);
		pr_info("crop_top = %d, crop_left = %d\n",
			node->crop_info.source_top_crop,
			node->crop_info.source_left_crop);
		pr_info("crop_width = %d, crop_height = %d\n\n",
			node->crop_info.source_width_crop,
			node->crop_info.source_height_crop);
	}
	if (node->crop_info.capture_crop_enable == 1) {
		if ((node->crop_info.source_top_crop > 0) &&
			(node->crop_info.source_top_crop < vf->height))
			src_top = node->crop_info.source_top_crop;
		else
			src_top = 0;

		if ((node->crop_info.source_left_crop > 0) &&
			(node->crop_info.source_left_crop < vf->width))
			src_left = node->crop_info.source_left_crop;
		else
			src_left = 0;

		if ((node->crop_info.source_width_crop > 0) &&
			(node->crop_info.source_width_crop <
			(vf->width - src_left)))
			src_width = node->crop_info.source_width_crop;
		else
			src_width = vf->width - src_left;

		if (node->crop_info.source_height_crop > 0 &&
			(node->crop_info.source_height_crop <
			(vf->height - src_top)))
			src_height = node->crop_info.source_height_crop;
		else
			src_height = vf->height - src_top;
	} else {
		if (node->has_amvideo_node) {
			src_axis_adjust(&src_top, &src_left,
				&src_width, &src_height, output);
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("src_width = %d, src_left = %d\n",
				src_width, src_left);
			pr_info("src_height = %d, src_top = %d\n",
				src_height, src_top);
		}
		if (((src_width + src_left) > vf->width) ||
			((src_height + src_top) > vf->height) ||
			(src_top < 0) || (src_left < 0) ||
			(src_width <= 0) || (src_height <= 0)) {
			pr_info("amlvideo2:parameters is not match\n");
			src_width = vf->width;
			src_height = vf->height;
			src_top = 0;
			src_left = 0;
		}
		src_top = src_top & 0xfffffffe;
		src_left = src_left & 0xfffffffe;
		src_width = src_width & 0xfffffffe;
		src_height = src_height & 0xfffffffe;
	}



	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = 0;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	if (node->porttype == TVIN_PORT_VIU) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	if ((node->crop_info.capture_crop_enable == 0) &&
		(node->porttype != TVIN_PORT_VIDEO)) {
		output_axis_adjust(
			src_width, src_height,
			&dst_width, &dst_height,
			cur_angle, output);
	}
	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	node->crop_info.source_top_crop = dst_top;
	node->crop_info.source_left_crop = dst_left;
	node->crop_info.source_width_crop = dst_width;
	node->crop_info.source_height_crop = dst_height;

	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if ((dst_left != output->frame->x) || (dst_top != output->frame->y)
		|| (dst_width != output->frame->w)
		|| (dst_height != output->frame->h)) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->width;
		ge2d_config->src_para.height = output->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->width;
		ge2d_config->dst_para.height = output->height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("output_width = %d , output_height = %d\n",
				output->width, output->height);
			pr_info("dst_format = %x\n",
				ge2d_config->dst_para.format);
		}
		fillrect(
			context,
			0,
			0,
			output->width,
			output->height,
			(ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
				0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
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
	canvas_read(output_canvas & 0xff, &cd);
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
	ge2d_config->src_para.height = vf->height / 2;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if ((output->v4l2_format != V4L2_PIX_FMT_YUV420) && (output->v4l2_format
		!= V4L2_PIX_FMT_YVU420))
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = (get_output_format(output->v4l2_format)
		| GE2D_LITTLE_ENDIAN)
					| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height / 2;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	if (amlvideo2_dbg_en & 4) {
		pr_info("src0_addr = %lx, w = %d, h = %d\n",
			cs0.addr, cs0.width, cs0.height);
		pr_info("src1_addr = %lx, w = %d, h = %d\n",
			cs1.addr, cs1.width, cs1.height);
		pr_info("src2_addr = %lx, w = %d, h = %d\n",
			cs2.addr, cs2.width, cs2.height);
		pr_info("output: w = %d, h = %d, output_canvas = %x\n\n",
			output->width, output->height, output_canvas);

		pr_info("%s:node->id = %d ,src_left = %d ,src_top = %d\n",
			__func__, node->vid, src_left, src_top);
		pr_info("src_width=%d ,src_height=%d ,output->frame->x=%d\n",
			src_width, src_height, output->frame->x);
		pr_info("output->frame->y=%d ,frame->w=%d ,frame->h=%d\n",
			output->frame->y, output->frame->w, output->frame->h);
	}
	stretchblt_noalpha(
		context, src_left, src_top / 2, src_width, src_height / 2,
		output->frame->x, output->frame->y / 2,
		output->frame->w, output->frame->h / 2);

	/* for cr of  yuv420p or yuv420sp. */
	if ((output->v4l2_format == V4L2_PIX_FMT_YUV420) || (output->v4l2_format
		== V4L2_PIX_FMT_YVU420)) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			(GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN)
			| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 4;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 4, output->frame->w / 2,
			output->frame->h / 4);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			(GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN)
			| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 4;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 4, output->frame->w / 2,
			output->frame->h / 4);
	}

	/* ============================ */
	/* bottom field */
	/* ============================ */
	output_canvas = vf->canvas1Addr;
	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	src_height = vf->height;

	if (node->crop_info.capture_crop_enable == 1) {
		if ((node->crop_info.source_top_crop > 0) &&
			(node->crop_info.source_top_crop < vf->height))
			src_top = node->crop_info.source_top_crop;
		else
			src_top = 0;

		if ((node->crop_info.source_left_crop > 0) &&
			(node->crop_info.source_left_crop < vf->width))
			src_left = node->crop_info.source_left_crop;
		else
			src_left = 0;

		if ((node->crop_info.source_width_crop > 0) &&
			(node->crop_info.source_width_crop <
			(vf->width - src_left)))
			src_width = node->crop_info.source_width_crop;
		else
			src_width = vf->width - src_left;

		if (node->crop_info.source_height_crop > 0 &&
			(node->crop_info.source_height_crop <
			(vf->height - src_top)))
			src_height = node->crop_info.source_height_crop;
		else
			src_height = vf->height - src_top;
	} else {
		if (node->has_amvideo_node) {
			src_axis_adjust(&src_top, &src_left,
				&src_width, &src_height, output);
		}
		src_top = src_top & 0xfffffffe;
		src_left = src_left & 0xfffffffe;
		src_width = src_width & 0xfffffffe;
		src_height = src_height & 0xfffffffe;
	}


	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = 0;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	if (src_width < src_height)
		cur_angle = (cur_angle + 90) % 360;
	if ((node->crop_info.capture_crop_enable == 0) &&
		(node->porttype != TVIN_PORT_VIDEO)) {
		output_axis_adjust(
			src_width, src_height,
			&dst_width, &dst_height,
			cur_angle, output);
	}
	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if ((dst_left != output->frame->x) || (dst_top != output->frame->y)
		|| (dst_width != output->frame->w)
		|| (dst_height != output->frame->h)) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->width;
		ge2d_config->src_para.height = output->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->width;
		ge2d_config->dst_para.height = output->height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		fillrect(
			context,
			0,
			0,
			output->width,
			output->height,
			(ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
				0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
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
	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = vf->canvas1Addr;
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
	ge2d_config->src_para.height = vf->height / 2;
	/* pr_err("vf_width is %d ,
	 *  vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if ((output->v4l2_format != V4L2_PIX_FMT_YUV420) && (output->v4l2_format
		!= V4L2_PIX_FMT_YVU420))
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = (get_output_format(output->v4l2_format)
		| GE2D_LITTLE_ENDIAN)
					| (GE2D_FORMAT_M24_YUV420B & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height / 2;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(
		context, src_left, src_top / 2, src_width, src_height / 2,
		output->frame->x, output->frame->y / 2,
		output->frame->w, output->frame->h / 2);

	/* for cr of  yuv420p or yuv420sp. */
	if ((output->v4l2_format == V4L2_PIX_FMT_YUV420) || (output->v4l2_format
		== V4L2_PIX_FMT_YVU420)) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			(GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN)
			| (GE2D_FORMAT_M24_YUV420B & (3 << 3));
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 4;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 4, output->frame->w / 2,
			output->frame->h / 4);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			(GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN)
			| (GE2D_FORMAT_M24_YUV420B & (3 << 3));
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 4;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 4, output->frame->w / 2,
			output->frame->h / 4);
	}

	return output_canvas;
}

int amlvideo2_ge2d_interlace_vdindata_process(
struct vframe_s *vf, struct ge2d_context_s *context,
struct config_para_ex_s *ge2d_config, struct amlvideo2_output *output,
struct amlvideo2_node *node)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror = 0;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;

	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	src_height = vf->height;
	if (amlvideo2_dbg_en & 4) {
		pr_info("vf->width = %d, vf->height = %d\n",
			vf->width, vf->height);
		pr_info("vf->type = %x, vf->src_canvas = %x\n",
			vf->type, vf->canvas0Addr);
		pr_info("crop_enable = %d\n",
			node->crop_info.capture_crop_enable);
		pr_info("crop_top = %d, crop_left = %d\n",
			node->crop_info.source_top_crop,
			node->crop_info.source_left_crop);
		pr_info("crop_width = %d, crop_height = %d\n\n",
			node->crop_info.source_width_crop,
			node->crop_info.source_height_crop);
	}
	if (node->crop_info.capture_crop_enable == 1) {
		if ((node->crop_info.source_top_crop > 0) &&
			(node->crop_info.source_top_crop < vf->height))
			src_top = node->crop_info.source_top_crop;
		else
			src_top = 0;

		if ((node->crop_info.source_left_crop > 0) &&
			(node->crop_info.source_left_crop < vf->width))
			src_left = node->crop_info.source_left_crop;
		else
			src_left = 0;

		if ((node->crop_info.source_width_crop > 0) &&
			(node->crop_info.source_width_crop <
			(vf->width - src_left)))
			src_width = node->crop_info.source_width_crop;
		else
			src_width = vf->width - src_left;

		if (node->crop_info.source_height_crop > 0 &&
			(node->crop_info.source_height_crop <
			(vf->height - src_top)))
			src_height = node->crop_info.source_height_crop;
		else
			src_height = vf->height - src_top;
	} else {
		if (node->has_amvideo_node) {
			src_axis_adjust(&src_top, &src_left,
				&src_width, &src_height, output);
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("src_width = %d, src_left = %d\n",
				src_width, src_left);
			pr_info("src_height = %d, src_top = %d\n",
				src_height, src_top);
		}
		if (((src_width + src_left) > vf->width) ||
			((src_height + src_top) > vf->height) ||
			(src_top < 0) || (src_left < 0) ||
			(src_width <= 0) || (src_height <= 0)) {
			pr_info("amlvideo2:parameters is not match\n");
			src_width = vf->width;
			src_height = vf->height;
			src_top = 0;
			src_left = 0;
		}
		src_top = src_top & 0xfffffffe;
		src_left = src_left & 0xfffffffe;
		src_width = src_width & 0xfffffffe;
		src_height = src_height & 0xfffffffe;
	}


	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = 0;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	if (node->porttype == TVIN_PORT_VIU) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	if ((node->crop_info.capture_crop_enable == 0) &&
		(node->porttype != TVIN_PORT_VIDEO)) {
		output_axis_adjust(
			src_width, src_height,
			&dst_width, &dst_height,
			cur_angle, output);
	}

	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	node->crop_info.source_top_crop = dst_top;
	node->crop_info.source_left_crop = dst_left;
	node->crop_info.source_width_crop = dst_width;
	node->crop_info.source_height_crop = dst_height;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if ((dst_left != output->frame->x) || (dst_top != output->frame->y)
		|| (dst_width != output->frame->w)
		|| (dst_height != output->frame->h)) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format(
			output->v4l2_format) | GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->width;
		ge2d_config->src_para.height = output->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->width;
		ge2d_config->dst_para.height = output->height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("output_width = %d , output_height = %d\n",
				output->width, output->height);
			pr_info("dst_format = %x\n",
				ge2d_config->dst_para.format);
		}
		fillrect(
			context,
			0,
			0,
			output->width,
			output->height,
			(ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
				0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	src_height = src_height / 2;
	src_top = src_top / 2;
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
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
	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_interlace_input_format(vf, output);
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height/2;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if ((output->v4l2_format != V4L2_PIX_FMT_YUV420) && (output->v4l2_format
		!= V4L2_PIX_FMT_YVU420))
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = get_output_format(
		output->v4l2_format)|GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	if (amlvideo2_dbg_en & 4) {
		pr_info("src0_addr = %lx, w = %d, h = %d\n",
			cs0.addr, cs0.width, cs0.height);
		pr_info("src1_addr = %lx, w = %d, h = %d\n",
			cs1.addr, cs1.width, cs1.height);
		pr_info("src2_addr = %lx, w = %d, h = %d\n",
			cs2.addr, cs2.width, cs2.height);
		pr_info("output: w = %d, h = %d, output_canvas = %x\n\n",
			output->width, output->height, output_canvas);

		pr_info("%s:node->id = %d ,src_left = %d ,src_top = %d\n",
			__func__, node->vid, src_left, src_top);
		pr_info("src_width=%d ,src_height=%d ,output->frame->x=%d\n",
			src_width, src_height, output->frame->x);
		pr_info("output->frame->y=%d ,frame->w=%d ,frame->h=%d\n",
			output->frame->y, output->frame->w, output->frame->h);
	}
	stretchblt_noalpha(
		context, src_left, src_top, src_width, src_height,
		output->frame->x, output->frame->y, output->frame->w,
		output->frame->h);

	/* for cr of  yuv420p or yuv420sp. */
	if ((output->v4l2_format == V4L2_PIX_FMT_YUV420) || (output->v4l2_format
		== V4L2_PIX_FMT_YVU420)) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top, src_width, src_height,
			output->frame->x / 2, output->frame->y / 2,
			output->frame->w / 2, output->frame->h / 2);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top, src_width, src_height,
			output->frame->x / 2, output->frame->y / 2,
			output->frame->w / 2, output->frame->h / 2);
	}
	return output_canvas;
}

int amlvideo2_ge2d_interlace_one_canvasAddr_process(
struct vframe_s *vf, struct ge2d_context_s *context,
struct config_para_ex_s *ge2d_config, struct amlvideo2_output *output,
struct amlvideo2_node *node)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror = 0;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;

	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	/* src_height = vf->height/2; */
	src_height = vf->height;
	if (amlvideo2_dbg_en & 4) {
		pr_info("vf->width = %d, vf->height = %d\n",
			vf->width, vf->height);
		pr_info("vf->type = %x, vf->src_canvas = %x\n",
			vf->type, vf->canvas0Addr);
		pr_info("crop_enable = %d\n",
			node->crop_info.capture_crop_enable);
		pr_info("crop_top = %d, crop_left = %d\n",
			node->crop_info.source_top_crop,
			node->crop_info.source_left_crop);
		pr_info("crop_width = %d, crop_height = %d\n\n",
			node->crop_info.source_width_crop,
			node->crop_info.source_height_crop);
	}
	if (node->crop_info.capture_crop_enable == 1) {
		if ((node->crop_info.source_top_crop > 0) &&
			(node->crop_info.source_top_crop < vf->height))
			src_top = node->crop_info.source_top_crop;
		else
			src_top = 0;

		if ((node->crop_info.source_left_crop > 0) &&
			(node->crop_info.source_left_crop < vf->width))
			src_left = node->crop_info.source_left_crop;
		else
			src_left = 0;

		if ((node->crop_info.source_width_crop > 0) &&
			(node->crop_info.source_width_crop <
			(vf->width - src_left)))
			src_width = node->crop_info.source_width_crop;
		else
			src_width = vf->width - src_left;

		if (node->crop_info.source_height_crop > 0 &&
			(node->crop_info.source_height_crop <
			(vf->height - src_top)))
			src_height = node->crop_info.source_height_crop;
		else
			src_height = vf->height - src_top;
	} else {
		if (node->has_amvideo_node) {
			src_axis_adjust(&src_top, &src_left,
				&src_width, &src_height, output);
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("src_width = %d, src_left = %d\n",
				src_width, src_left);
			pr_info("src_height = %d, src_top = %d\n",
				src_height, src_top);
		}
		if (((src_width + src_left) > vf->width) ||
			((src_height + src_top) > vf->height) ||
			(src_top < 0) || (src_left < 0) ||
			(src_width <= 0) || (src_height <= 0)) {
			pr_info("amlvideo2:parameters is not match\n");
			src_width = vf->width;
			src_height = vf->height;
			src_top = 0;
			src_left = 0;
		}
		src_top = src_top & 0xfffffffe;
		src_left = src_left & 0xfffffffe;
		src_width = src_width & 0xfffffffe;
		src_height = src_height & 0xfffffffe;
	}


	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = 0;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	if (node->porttype == TVIN_PORT_VIU) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	if ((node->crop_info.capture_crop_enable == 0) &&
		(node->porttype != TVIN_PORT_VIDEO)) {
		output_axis_adjust(
			src_width, src_height,
			&dst_width, &dst_height,
			cur_angle, output);
	}

	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	node->crop_info.source_top_crop = dst_top;
	node->crop_info.source_left_crop = dst_left;
	node->crop_info.source_width_crop = dst_width;
	node->crop_info.source_height_crop = dst_height;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if ((dst_left != output->frame->x) || (dst_top != output->frame->y)
		|| (dst_width != output->frame->w)
		|| (dst_height != output->frame->h)) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->width;
		ge2d_config->src_para.height = output->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->width;
		ge2d_config->dst_para.height = output->height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("output_width = %d , output_height = %d\n",
				output->width, output->height);
			pr_info("dst_format = %x\n",
				ge2d_config->dst_para.format);
		}
		fillrect(
			context,
			0,
			0,
			output->width,
			output->height,
			(ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
				0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
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
	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_input_format_no_interlace(vf);
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if ((output->v4l2_format != V4L2_PIX_FMT_YUV420) && (output->v4l2_format
		!= V4L2_PIX_FMT_YVU420))
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = get_output_format(
		output->v4l2_format)|GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	if (amlvideo2_dbg_en & 4) {
		pr_info("src0_addr = %lx, w = %d, h = %d\n",
			cs0.addr, cs0.width, cs0.height);
		pr_info("src1_addr = %lx, w = %d, h = %d\n",
			cs1.addr, cs1.width, cs1.height);
		pr_info("src2_addr = %lx, w = %d, h = %d\n",
			cs2.addr, cs2.width, cs2.height);
		pr_info("output: w = %d, h = %d, output_canvas = %x\n\n",
			output->width, output->height, output_canvas);

		pr_info("%s:node->id = %d ,src_left = %d ,src_top = %d\n",
			__func__, node->vid, src_left, src_top);
		pr_info("src_width=%d ,src_height=%d ,output->frame->x=%d\n",
			src_width, src_height, output->frame->x);
		pr_info("output->frame->y=%d ,frame->w=%d ,frame->h=%d\n",
			output->frame->y, output->frame->w, output->frame->h);
	}
	stretchblt_noalpha(
		context, src_left, src_top / 2, src_width, src_height / 2,
		output->frame->x, output->frame->y, output->frame->w,
		output->frame->h);

	/* for cr of  yuv420p or yuv420sp. */
	if ((output->v4l2_format == V4L2_PIX_FMT_YUV420) || (output->v4l2_format
		== V4L2_PIX_FMT_YVU420)) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 2, output->frame->w / 2,
			output->frame->h / 2);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 2, output->frame->w / 2,
			output->frame->h / 2);
	}
	return output_canvas;
}

int amlvideo2_ge2d_interlace_dtv_process(
struct vframe_s *vf, struct ge2d_context_s *context,
struct config_para_ex_s *ge2d_config, struct amlvideo2_output *output,
struct amlvideo2_node *node)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror = 0;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;

	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	/* src_height = vf->height/2; */
	src_height = vf->height;
	if (amlvideo2_dbg_en & 4) {
		pr_info("vf->width = %d, vf->height = %d\n",
			vf->width, vf->height);
		pr_info("vf->type = %x, vf->src_canvas = %x\n",
			vf->type, vf->canvas0Addr);
		pr_info("crop_enable = %d\n",
			node->crop_info.capture_crop_enable);
		pr_info("crop_top = %d, crop_left = %d\n",
			node->crop_info.source_top_crop,
			node->crop_info.source_left_crop);
		pr_info("crop_width = %d, crop_height = %d\n\n",
			node->crop_info.source_width_crop,
			node->crop_info.source_height_crop);
	}
	if (node->crop_info.capture_crop_enable == 1) {
		if ((node->crop_info.source_top_crop > 0) &&
			(node->crop_info.source_top_crop < vf->height))
			src_top = node->crop_info.source_top_crop;
		else
			src_top = 0;

		if ((node->crop_info.source_left_crop > 0) &&
			(node->crop_info.source_left_crop < vf->width))
			src_left = node->crop_info.source_left_crop;
		else
			src_left = 0;

		if ((node->crop_info.source_width_crop > 0) &&
			(node->crop_info.source_width_crop <
			(vf->width - src_left)))
			src_width = node->crop_info.source_width_crop;
		else
			src_width = vf->width - src_left;

		if (node->crop_info.source_height_crop > 0 &&
			(node->crop_info.source_height_crop <
			(vf->height - src_top)))
			src_height = node->crop_info.source_height_crop;
		else
			src_height = vf->height - src_top;
	} else {
		if (node->has_amvideo_node) {
			src_axis_adjust(&src_top, &src_left,
				&src_width, &src_height, output);
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("src_width = %d, src_left = %d\n",
				src_width, src_left);
			pr_info("src_height = %d, src_top = %d\n",
				src_height, src_top);
		}
		if (((src_width + src_left) > vf->width) ||
			((src_height + src_top) > vf->height) ||
			(src_top < 0) || (src_left < 0) ||
			(src_width <= 0) || (src_height <= 0)) {
			pr_info("amlvideo2:parameters is not match\n");
			src_width = vf->width;
			src_height = vf->height;
			src_top = 0;
			src_left = 0;
		}
		src_top = src_top & 0xfffffffe;
		src_left = src_left & 0xfffffffe;
		src_width = src_width & 0xfffffffe;
		src_height = src_height & 0xfffffffe;
	}


	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = 0;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	if (node->porttype == TVIN_PORT_VIU) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	if ((node->crop_info.capture_crop_enable == 0) &&
		(node->porttype != TVIN_PORT_VIDEO)) {
		output_axis_adjust(
			src_width, src_height,
			&dst_width, &dst_height,
			cur_angle, output);
	}

	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	node->crop_info.source_top_crop = dst_top;
	node->crop_info.source_left_crop = dst_left;
	node->crop_info.source_width_crop = dst_width;
	node->crop_info.source_height_crop = dst_height;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if ((dst_left != output->frame->x) || (dst_top != output->frame->y)
		|| (dst_width != output->frame->w)
		|| (dst_height != output->frame->h)) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->width;
		ge2d_config->src_para.height = output->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->width;
		ge2d_config->dst_para.height = output->height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("output_width = %d , output_height = %d\n",
				output->width, output->height);
			pr_info("dst_format = %x\n",
				ge2d_config->dst_para.format);
		}
		fillrect(
			context,
			0,
			0,
			output->width,
			output->height,
			(ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
				0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
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
	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_interlace_input_format(vf, output);
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if ((output->v4l2_format != V4L2_PIX_FMT_YUV420) && (output->v4l2_format
		!= V4L2_PIX_FMT_YVU420))
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = get_output_format(
		output->v4l2_format)|GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	if (amlvideo2_dbg_en & 4) {
		pr_info("src0_addr = %lx, w = %d, h = %d\n",
			cs0.addr, cs0.width, cs0.height);
		pr_info("src1_addr = %lx, w = %d, h = %d\n",
			cs1.addr, cs1.width, cs1.height);
		pr_info("src2_addr = %lx, w = %d, h = %d\n",
			cs2.addr, cs2.width, cs2.height);
		pr_info("output: w = %d, h = %d, output_canvas = %x\n\n",
			output->width, output->height, output_canvas);

		pr_info("%s:node->id = %d ,src_left = %d ,src_top = %d\n",
			__func__, node->vid, src_left, src_top);
		pr_info("src_width=%d ,src_height=%d ,output->frame->x=%d\n",
			src_width, src_height, output->frame->x);
		pr_info("output->frame->y=%d ,frame->w=%d ,frame->h=%d\n",
			output->frame->y, output->frame->w, output->frame->h);
	}
	stretchblt_noalpha(
		context, src_left, src_top, src_width,  src_height,
		output->frame->x, output->frame->y, output->frame->w,
		output->frame->h);

	/* for cr of  yuv420p or yuv420sp. */
	if ((output->v4l2_format == V4L2_PIX_FMT_YUV420) || (output->v4l2_format
		== V4L2_PIX_FMT_YVU420)) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 2, output->frame->w / 2,
			output->frame->h / 2);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 2, output->frame->w / 2,
			output->frame->h / 2);
	}
	return output_canvas;
}

int amlvideo2_ge2d_multi_pre_process(struct vframe_s *vf,
				struct ge2d_context_s *context,
				struct config_para_ex_s *ge2d_config,
				struct amlvideo2_output *output,
				struct amlvideo2_node *node)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror = 0;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;
	int temp_canvas = AMLVIDEO2_1_RES_CANVAS + 8;
	unsigned long temp_start = node->vid_dev->buffer_start +
		(CMA_ALLOC_SIZE * SZ_1M);
	int temp_w = vf->width/4;
	int temp_h = vf->height/4;
	u32 h_scale_coef_type =
		context->config.h_scale_coef_type;
	u32 v_scale_coef_type =
		context->config.v_scale_coef_type;

	temp_w = (temp_w + 31) & (~31);
	temp_h = (temp_h + 1) & (~1);

	if (amlvideo2_dbg_en & 4) {
		pr_info("vf->width = %d, vf->height = %d\n",
			vf->width, vf->height);
	}

	if (amlvideo2_scaledown1 == 0) {
		context->config.h_scale_coef_type = FILTER_TYPE_BICUBIC;
		context->config.v_scale_coef_type = FILTER_TYPE_BICUBIC;
	} else if (amlvideo2_scaledown1 == 1) {
		context->config.h_scale_coef_type = FILTER_TYPE_BILINEAR;
		context->config.v_scale_coef_type = FILTER_TYPE_BILINEAR;
	} else if (amlvideo2_scaledown1 == 2) {
		context->config.h_scale_coef_type = FILTER_TYPE_TRIANGLE;
		context->config.v_scale_coef_type = FILTER_TYPE_TRIANGLE;
	} else {
		context->config.h_scale_coef_type = FILTER_TYPE_BICUBIC;
		context->config.v_scale_coef_type = FILTER_TYPE_BICUBIC;
	}
	canvas_config(temp_canvas,
		(unsigned long)temp_start,
		temp_w, temp_h,
		CANVAS_ADDR_NOWRAP,
		CANVAS_BLKMODE_LINEAR);

	src_top = 4;
	src_left = 4;
	src_width = vf->width;
	src_height = vf->height;
	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = 0;
	cur_angle = 0;


	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	node->crop_info.source_top_crop = dst_top;
	node->crop_info.source_left_crop = dst_left;
	node->crop_info.source_width_crop = dst_width;
	node->crop_info.source_height_crop = dst_height;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
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
	canvas_read(output_canvas & 0xff, &cd);
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
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = temp_canvas & 0xff;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FORMAT_S8_Y | GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = temp_w;
	ge2d_config->dst_para.height = temp_h;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, src_left, src_top, src_width - src_left,
		src_height - src_top, 0, 0, temp_w,
		temp_h);

	if (amlvideo2_scaledown2 == 0) {
		context->config.h_scale_coef_type = FILTER_TYPE_BICUBIC;
		context->config.v_scale_coef_type = FILTER_TYPE_BICUBIC;
	} else if (amlvideo2_scaledown2 == 1) {
		context->config.h_scale_coef_type = FILTER_TYPE_BILINEAR;
		context->config.v_scale_coef_type = FILTER_TYPE_BILINEAR;
	} else if (amlvideo2_scaledown2 == 2) {
		context->config.h_scale_coef_type = FILTER_TYPE_TRIANGLE;
		context->config.v_scale_coef_type = FILTER_TYPE_TRIANGLE;
	}
	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	canvas_read(temp_canvas & 0xff, &cs0);
	ge2d_config->src_planes[0].addr = cs0.addr;
	ge2d_config->src_planes[0].w = cs0.width;
	ge2d_config->src_planes[0].h = cs0.height;
	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = temp_canvas;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FORMAT_S8_Y | GE2D_LITTLE_ENDIAN;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = temp_w;
	ge2d_config->src_para.height = temp_h;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FORMAT_S8_Y | GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, 0, 0, temp_w, temp_h,
		0, 0, output->width, output->height);

	context->config.h_scale_coef_type =
		h_scale_coef_type;
	context->config.v_scale_coef_type =
		v_scale_coef_type;
	return output_canvas;
}

int amlvideo2_ge2d_pre_process(struct vframe_s *vf,
				struct ge2d_context_s *context,
				struct config_para_ex_s *ge2d_config,
				struct amlvideo2_output *output,
				struct amlvideo2_node *node)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror = 0;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;

	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	src_height = vf->height;
	if (amlvideo2_dbg_en & 4) {
		pr_info("vf->width = %d, vf->height = %d\n",
			vf->width, vf->height);
		pr_info("vf->type = %x, vf->src_canvas = %x\n",
			vf->type, vf->canvas0Addr);
		pr_info("crop_enable = %d\n",
			node->crop_info.capture_crop_enable);
		pr_info("crop_top = %d, crop_left = %d\n",
			node->crop_info.source_top_crop,
			node->crop_info.source_left_crop);
		pr_info("crop_width = %d, crop_height = %d\n\n",
			node->crop_info.source_width_crop,
			node->crop_info.source_height_crop);
	}
	if (node->crop_info.capture_crop_enable == 1) {
		if ((node->crop_info.source_top_crop > 0) &&
			(node->crop_info.source_top_crop < vf->height))
			src_top = node->crop_info.source_top_crop;
		else
			src_top = 0;

		if ((node->crop_info.source_left_crop > 0) &&
			(node->crop_info.source_left_crop < vf->width))
			src_left = node->crop_info.source_left_crop;
		else
			src_left = 0;

		if ((node->crop_info.source_width_crop > 0) &&
			(node->crop_info.source_width_crop <
			(vf->width - src_left)))
			src_width = node->crop_info.source_width_crop;
		else
			src_width = vf->width - src_left;

		if (node->crop_info.source_height_crop > 0 &&
			(node->crop_info.source_height_crop <
			(vf->height - src_top)))
			src_height = node->crop_info.source_height_crop;
		else
			src_height = vf->height - src_top;
	} else {
		if (node->has_amvideo_node) {
			src_axis_adjust(&src_top, &src_left,
				&src_width, &src_height, output);
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("src_width = %d, src_left = %d\n",
				src_width, src_left);
			pr_info("src_height = %d, src_top = %d\n",
				src_height, src_top);
		}
		if (((src_width + src_left) > vf->width) ||
			((src_height + src_top) > vf->height) ||
			(src_top < 0) || (src_left < 0) ||
			(src_width <= 0) || (src_height <= 0)) {
			pr_info("amlvideo2:parameters is not match\n");
			src_width = vf->width;
			src_height = vf->height;
			src_top = 0;
			src_left = 0;
		}
		src_top = src_top & 0xfffffffe;
		src_left = src_left & 0xfffffffe;
		src_width = src_width & 0xfffffffe;
		src_height = src_height & 0xfffffffe;
	}


	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = 0;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	if (node->porttype == TVIN_PORT_VIU) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	if ((node->crop_info.capture_crop_enable == 0) &&
		(node->porttype != TVIN_PORT_VIDEO)) {
		output_axis_adjust(
			src_width, src_height,
			&dst_width, &dst_height,
			cur_angle, output);
	}

	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	node->crop_info.source_top_crop = dst_top;
	node->crop_info.source_left_crop = dst_left;
	node->crop_info.source_width_crop = dst_width;
	node->crop_info.source_height_crop = dst_height;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if ((dst_left != output->frame->x) || (dst_top != output->frame->y)
		|| (dst_width != output->frame->w)
		|| (dst_height != output->frame->h)) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->width;
		ge2d_config->src_para.height = output->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->width;
		ge2d_config->dst_para.height = output->height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("output_width = %d , output_height = %d\n",
				output->width, output->height);
			pr_info("dst_format = %x\n",
				ge2d_config->dst_para.format);
		}
		fillrect(
			context,
			0,
			0,
			output->width,
			output->height,
			(ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
				0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
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
	canvas_read(output_canvas & 0xff, &cd);
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
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if ((output->v4l2_format != V4L2_PIX_FMT_YUV420) && (output->v4l2_format
		!= V4L2_PIX_FMT_YVU420))
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = get_output_format(
		output->v4l2_format)|GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	if (amlvideo2_dbg_en & 4) {
		pr_info("src0_addr = %lx, w = %d, h = %d\n",
			cs0.addr, cs0.width, cs0.height);
		pr_info("src1_addr = %lx, w = %d, h = %d\n",
			cs1.addr, cs1.width, cs1.height);
		pr_info("src2_addr = %lx, w = %d, h = %d\n",
			cs2.addr, cs2.width, cs2.height);
		pr_info("output: w = %d, h = %d, output_canvas = %x\n\n",
			output->width, output->height, output_canvas);

		pr_info("%s:node->id = %d ,src_left = %d ,src_top = %d\n",
			__func__, node->vid, src_left, src_top);
		pr_info("src_width=%d ,src_height=%d ,output->frame->x=%d\n",
			src_width, src_height, output->frame->x);
		pr_info("output->frame->y=%d ,frame->w=%d ,frame->h=%d\n",
			output->frame->y, output->frame->w, output->frame->h);
	}
	stretchblt_noalpha(context, src_left, src_top, src_width, src_height,
		output->frame->x, output->frame->y, output->frame->w,
		output->frame->h);

	/* for cr of  yuv420p or yuv420sp. */
	if ((output->v4l2_format == V4L2_PIX_FMT_YUV420) || (output->v4l2_format
		== V4L2_PIX_FMT_YVU420)) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top, src_width, src_height,
			output->frame->x / 2, output->frame->y / 2,
			output->frame->w / 2, output->frame->h / 2);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context,
			src_left, src_top, src_width, src_height,
			output->frame->x / 2, output->frame->y / 2,
			output->frame->w / 2, output->frame->h / 2);
	}
	return output_canvas;
}

int amlvideo2_sw_post_process(int canvas, void *addr)
{
	return 0;
}

static int amlvideo2_fillbuff(struct amlvideo2_fh *fh,
				struct amlvideo2_node_buffer *buf,
				struct vframe_s *vf)
{
	struct config_para_ex_s ge2d_config;
	struct amlvideo2_output output;
	struct amlvideo2_node *node = fh->node;
	void *vbuf = NULL;
	int src_canvas = -1;
	int magic = 0;
	int ge2d_proc = 0;
	int sw_proc = 0;

	vbuf = (void *)videobuf_to_res(&buf->vb);

	dpr_err(node->vid_dev, 1, "%s\n", __func__);
	if ((!vbuf) || (!vf))
		return -1;

	/* memset(&ge2d_config,0,sizeof(struct config_para_ex_s)); */
	memset(&output, 0, sizeof(struct amlvideo2_output));

	output.v4l2_format = fh->fmt->fourcc;
	output.vbuf = vbuf;
	output.width = buf->vb.width;
	output.height = buf->vb.height;
	output.canvas_id = buf->canvas_id;
	output.angle = node->qctl_regs[0];
	output.frame = &buf->axis;
	output.info.mode = node->mode;
	memcpy(&output.info.display_info, &(node->display_info),
			sizeof(struct vdisplay_info_s));

	magic = MAGIC_RE_MEM;
	switch (magic) {
	case MAGIC_RE_MEM:
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	#if 1
		if (output.canvas_id == 0) {
			output.canvas_id = get_amlvideo2_canvas_index(
				&output, (node->vid == 0) ?
				(AMLVIDEO2_RES0_CANVAS_INDEX + buf->vb.i * 3) :
				(AMLVIDEO2_RES1_CANVAS_INDEX + buf->vb.i * 3));
			buf->canvas_id = output.canvas_id;
		}
	#else
	#ifdef MULTI_NODE
		output.canvas_id =
			get_amlvideo2_canvas_index(
				&output, (node->vid == 0)
				? AMLVIDEO2_RES0_CANVAS_INDEX
				: AMLVIDEO2_RES1_CANVAS_INDEX);
	#else
		output.canvas_id =
			get_amlvideo2_canvas_index(
				&output, AMLVIDEO2_RES0_CANVAS_INDEX);
	#endif
	#endif
		break;
	case MAGIC_VMAL_MEM:
		/* canvas_index =
		 * get_amlvideo2_canvas_index(
		 * v4l2_format,&depth);
		 */
		/* sw_proc = 1; */
		/* break; */
	case MAGIC_DC_MEM:
	case MAGIC_SG_MEM:
	default:
		return -1;
	}

	switch (output.v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		ge2d_proc = 1;
		break;
	default:
		break;
	}
	src_canvas = vf->canvas0Addr;
	if (ge2d_proc) {
		if ((vf->type & VIDTYPE_INTERLACE_BOTTOM) || (vf->type
			& VIDTYPE_INTERLACE_TOP)) {
			if (vf->canvas0Addr == vf->canvas1Addr) {
				if ((node->p_type == AML_PROVIDE_VDIN0) &&
					(node->porttype == TVIN_PORT_VIU)) {
					src_canvas =
				amlvideo2_ge2d_interlace_vdindata_process(
					vf, node->context, &ge2d_config,
					&output, node);
				} else if ((vf->type & VIDTYPE_VIU_NV21) &&
				(node->p_type == AML_PROVIDE_DECODE)) {
					src_canvas =
				amlvideo2_ge2d_interlace_dtv_process(
				vf, node->context, &ge2d_config,
				&output, node);
				} else {
					src_canvas =
			amlvideo2_ge2d_interlace_one_canvasAddr_process(
				vf, node->context, &ge2d_config,
				&output, node);
				}
			} else {
				src_canvas =
			amlvideo2_ge2d_interlace_two_canvasAddr_process(
				vf, node->context, &ge2d_config,
				&output, node);
			}
		} else {
			if (node->ge2d_multi_process_flag)
				src_canvas = amlvideo2_ge2d_multi_pre_process(
				vf, node->context,
				&ge2d_config, &output, node);
			else
				src_canvas = amlvideo2_ge2d_pre_process(
				vf, node->context,
				&ge2d_config, &output, node);
		}
	}

	if ((sw_proc) && (src_canvas > 0))
		amlvideo2_sw_post_process(src_canvas, vbuf);

	buf->vb.state = VIDEOBUF_DONE;
	/* do_gettimeofday(&buf->vb.ts); */
	return 0;
}

static unsigned int print_ivals;
module_param(print_ivals, uint, 0644);
MODULE_PARM_DESC(print_ivals, "print current intervals!!");

#define TEST_LATENCY
#ifdef TEST_LATENCY
static unsigned int print_latecny;
module_param(print_latecny, uint, 0644);
MODULE_PARM_DESC(print_latecny, "print current latency!!");
#endif

/* ------------------------------------------------------------------
 * queue operations
 * ------------------------------------------------------------------
 */

void vf_inqueue(struct vframe_s *vf, struct amlvideo2_node *node)
{
	struct vframe_receiver_s *vfp;
	const char *name = node->recv.name;

	if (vf == NULL)
		return;

	vfp = vf_get_receiver(name);
	if (vfp == NULL) {
		vf_put(vf, name);
		return;
	}

	mutex_lock(&node->mutex);
	vfq_push(&node->q_ready, vf);
	mutex_unlock(&node->mutex);

	vf_notify_receiver(
		name,
		VFRAME_EVENT_PROVIDER_VFRAME_READY,
		NULL);
}


static int amlvideo2_thread_tick(struct amlvideo2_fh *fh)
{
	struct amlvideo2_node_buffer *buf = NULL;
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	unsigned int diff = 0;
	bool no_frame = false;
	struct vframe_s *vf = NULL;
	unsigned long flags = 0;
	int active_duration = 0;
	int i_ret = 0;

	dpr_err(node->vid_dev, 1, "Thread tick\n");
	if (amlvideo2_dbg_en & 2) {
		if (node->vid == 0)
			pr_info("Enter amlvideo2.0 thread tick\n");
		else
			pr_info("Enter amlvideo2.1 thread tick\n");
	}

	if (kthread_should_stop()) {
		if (amlvideo2_dbg_en & 2)
			pr_info("amlvideo2 thread stop\n");
		return 0;
	}
	i_ret = wait_event_interruptible_timeout(
		dma_q->wq,
		((vf_peek(node->recv.name) != NULL)
		&& (node->provide_ready)) || (node->vidq.task_running == 0),
		msecs_to_jiffies(5000));

	if (i_ret == 0 || node->vidq.task_running == 0) {
		if (node->pflag)
			complete(&node->plug_sema);
		return 0;
	}

	if ((node->r_type != AML_RECEIVER_NONE) &&
		vfq_full(&node->q_ready)) {
		if (amlvideo2_dbg_en & 2)
			pr_info("q_ready full ,receiver is none\n");
		return -1;
	}

	if ((node->video_blocking) && (node->amlvideo2_pool_ready != NULL)) {
		vfq_init(&node->q_ready,
		node->amlvideo2_pool_size + 1,
		(struct vframe_s **)&(node->amlvideo2_pool_ready[0]));
		node->video_blocking = false;
		node->tmp_vf = NULL;
		pr_err("video blocking need to reset@!!!!!\n");
		return 0;
	}

	if (!fh->is_streamed_on) {
		dpr_err(node->vid_dev, 1, "dev doesn't stream on\n");
		if (node->r_type != AML_RECEIVER_NONE) {
			if (node->tmp_vf) {
				vf_inqueue(node->tmp_vf, node);
				node->tmp_vf = NULL;
			}
		} else {
			node->tmp_vf = NULL;
		}
		while (vf_peek(node->recv.name) &&
			(!vfq_full(&node->q_ready))) {
			if (amlvideo2_dbg_en & 2)
				pr_info("while 1.\n");
			vf = vf_get(node->recv.name);
			vf_inqueue(vf, node);
			if (vf) {
				fh->src_width  =  vf->width;
				fh->src_height  =  vf->height;
			}
		}
		return 0;
	}

	if (!node->provide_ready) {
		if (amlvideo2_dbg_en & 2)
			pr_info("provide is not ready .\n");
		dpr_err(node->vid_dev, 1, "provide is not ready\n");
		if (node->pflag)
			complete(&node->plug_sema);
		return -1;
	}

	if (node->pflag) {
		complete(&node->plug_sema);
		return 0;
	}
	if (amlvideo2_dbg_en & 2)
		pr_info("start spin_lock_irqsave.\n");

	spin_lock_irqsave(&node->slock, flags);
	if (list_empty(&dma_q->active)) {
		if (amlvideo2_dbg_en & 2)
			pr_info("No active queue to serve .\n");
		dpr_err(node->vid_dev, 1, "No active queue to serve\n");
		while (vf_peek(node->recv.name) &&
			(!vfq_full(&node->q_ready))) {
			if (amlvideo2_dbg_en & 2)
				pr_info("while 2.\n");
			vf = vf_get(node->recv.name);
			vf_inqueue(vf, node);
		}
		goto unlock;
	}

	if (amlvideo2_dbg_en & 2)
		pr_info(" spin_lock_irqsave 1 .\n");
	buf = list_entry(
		dma_q->active.next,
		struct amlvideo2_node_buffer,
		vb.queue);
	if (amlvideo2_dbg_en & 2)
		pr_info("ready videobuf to fill data .\n");

	if (vf_peek(node->recv.name) == NULL) {
		no_frame = true;
	} else {
		/* drop the frame to get the last one */
		if (!vfq_full(&node->q_ready)) {
			vf = vf_get(node->recv.name);
			if (((vf->type & VIDTYPE_TYPEMASK)
				== VIDTYPE_INTERLACE_TOP) &&
				(node->field_flag)) {
				node->field_flag = false;
				node->field_condition_flag = true;
			}
			while ((vf_peek(node->recv.name) != NULL)
				&& (!node->field_condition_flag)) {
				if (amlvideo2_dbg_en & 2)
					pr_info("while 3.\n");
				vf_inqueue(vf, node);
				if (!vfq_full(&node->q_ready)) {
					vf = vf_get(node->recv.name);
					if (((vf->type & VIDTYPE_TYPEMASK)
						== VIDTYPE_INTERLACE_TOP) &&
						(node->field_flag)) {
						node->field_flag = false;
						break;
					}
				} else
					break;
			}
			if (vf != NULL) {
				if (((vf->type & VIDTYPE_TYPEMASK)
					== VIDTYPE_INTERLACE_BOTTOM) &&
					(vf->canvas0Addr == vf->canvas1Addr)) {
					vf_inqueue(vf, node);
					no_frame = true;
					vf = NULL;
					node->field_flag = true;
				}
			}
		}
		node->field_condition_flag = false;
	}

	#ifdef USE_VDIN_PTS
	if (no_frame)
		goto unlock;
	if (node->frame_inittime == 1) {
		if (node->tmp_vf) {
			vf_inqueue(node->tmp_vf, node);
			node->tmp_vf = NULL;
		}
		node->frameInv_adjust = 0;
		node->frameInv = 0;
		node->thread_ts1.tv_sec = vf->pts_us64 & 0xFFFFFFFF;
		node->thread_ts1.tv_usec = vf->pts;
		node->frame_inittime = 0;
	} else {
		diff = (vf->pts_us64 & 0xFFFFFFFF) - node->thread_ts1.tv_sec;
		diff = diff*1000000 + vf->pts - node->thread_ts1.tv_usec;
		if (diff < (int) fh->frm_save_time_us) {
			vf_inqueue(vf, node);
			goto unlock;
		}
	}
	#else
	if (node->frame_inittime == 1) {
		if (node->tmp_vf) {
			vf_inqueue(node->tmp_vf, node);
			node->tmp_vf = NULL;
		}
		if (no_frame)
			goto unlock;
		node->frameInv_adjust = 0;
		node->frameInv = 0;
		do_gettimeofday(&node->thread_ts1);
		node->frame_inittime = 0;
	} else {
		do_gettimeofday(&node->thread_ts2);
		/* thread_ts2.tv_sec = vf->pts_us64& 0xFFFFFFFF; */
		/* thread_ts2.tv_usec = vf->pts; */
		diff = node->thread_ts2.tv_sec - node->thread_ts1.tv_sec;
		diff = diff * 1000000 + node->thread_ts2.tv_usec -
			node->thread_ts1.tv_usec;
		node->frameInv += diff;
		memcpy(&node->thread_ts1, &node->thread_ts2,
			sizeof(struct timeval));
		active_duration = node->frameInv - node->frameInv_adjust;
		/* Fill buffer */
		if (active_duration + 5000 > (int)fh->frm_save_time_us) {
			/* ||(active_duration  > (int)fh->frm_save_time_us) */
			if (vf) {
				if (node->tmp_vf != NULL)
					vf_inqueue(node->tmp_vf, node);

				node->tmp_vf = NULL;
			} else if (node->tmp_vf) {
				vf = node->tmp_vf;
				node->tmp_vf = NULL;
			} else {
				goto unlock;
			}
		} else {
			if (vf) {
				if (node->tmp_vf != NULL)
					vf_inqueue(node->tmp_vf, node);

				node->tmp_vf = vf;
			}
			goto unlock;
		}
	}
	while (active_duration >= (int)fh->frm_save_time_us) {
		if (amlvideo2_dbg_en & 2)
			pr_info("while 4.\n");
		active_duration -= fh->frm_save_time_us;
	}

	if ((active_duration + 5000) > fh->frm_save_time_us)
		node->frameInv_adjust = fh->frm_save_time_us - active_duration;
	else
		node->frameInv_adjust = -active_duration;
	node->frameInv = 0;
	#endif
	if (amlvideo2_dbg_en & 2)
		pr_info(" spin_lock_irqsave 2 .\n");
	buf->vb.state = VIDEOBUF_ACTIVE;
	list_del(&buf->vb.queue);

	spin_unlock_irqrestore(&node->slock, flags);
	if (amlvideo2_dbg_en & 2)
		pr_info("finish spin_lock_irqsave.\n");

	/* test latency */
	#ifdef TEST_LATENCY
	if (print_latecny) {
		int timeNow64, timePts;

		do_gettimeofday(&node->latency_info.test_time);
		timeNow64 = ((node->latency_info.test_time.tv_sec &
				0xFFFFFFFF) * 1000 * 1000)
				+ (node->latency_info.test_time.tv_usec);
		/* timePts =  ((vf->pts_us64
		 * & 0xFFFFFFFF)*1000*1000) + (vf->pts);
		 */
		timePts = ((node->thread_ts2.tv_sec & 0xFFFFFFFF) * 1000 * 1000)
			+ (node->thread_ts2.tv_usec);
		/* pr_err("amlvideo2 in  num:%d delay:%d\n",
		 * timePts, (timeNow64 - timePts)/1000);
		 */
		if (node->latency_info.cur_time ==
		node->latency_info.test_time.tv_sec) {
			node->latency_info.total_latency +=
			(int)((timeNow64 - timePts) / 1000);
			node->latency_info.frame_num++;
		} else {
			node->latency_info.total_latency +=
			(int)((timeNow64 - timePts) / 1000);
			node->latency_info.frame_num++;
			pr_err("amvideo2 in latency:%d frame_num:%d\n",
			node->latency_info.total_latency /
			node->latency_info.frame_num,
			node->latency_info.frame_num);
			node->latency_info.total_latency = 0;
			node->latency_info.frame_num = 0;
			node->latency_info.cur_time =
			node->latency_info.test_time.tv_sec;
		}
	}
	#endif

	if (amlvideo2_dbg_en & 2) {
		if (node->vid == 0)
			pr_info("node0 fillbuff start .\n");
		else
			pr_info("node1 fillbuff start .\n");
	}
	amlvideo2_fillbuff(fh, buf, vf);
	if (amlvideo2_dbg_en & 2) {
		if (node->vid == 0)
			pr_info("node0 fillbuff end .\n");
		else
			pr_info("node1 fillbuff end .\n");
	}
	#ifdef USE_VDIN_PTS
	buf->vb.ts.tv_sec = vf->pts_us64 & 0xFFFFFFFF;
	buf->vb.ts.tv_usec = vf->pts;
	node->thread_ts1.tv_sec = vf->pts_us64 & 0xFFFFFFFF;
	node->thread_ts1.tv_usec = vf->pts;
	#else
	buf->vb.ts.tv_sec = node->thread_ts2.tv_sec & 0xFFFFFFFF;
	buf->vb.ts.tv_usec = node->thread_ts2.tv_usec;
	#endif
	vf_inqueue(vf, node);

	while ((vf_peek(node->recv.name) != NULL) &&
			(!vfq_full(&node->q_ready))) {
		if (amlvideo2_dbg_en & 2)
			pr_info("while 5.\n");
		vf = vf_get(node->recv.name);
		vf_inqueue(vf, node);
	}
	dpr_err(node->vid_dev, 1, "filled buffer %p\n", buf);
	/*wait up vb done buffer workqueue*/
	if (waitqueue_active(&buf->vb.done))
		wake_up(&buf->vb.done);

	if (node->pflag) {
		complete(&node->plug_sema);
		return 0;
	}

	if (amlvideo2_dbg_en & 2)
		pr_info("filled buffer %p\n", buf);

	/* test latency */
	#ifdef TEST_LATENCY
	if (print_latecny) {
		int timeNow64_out, timePts_out;

		do_gettimeofday(&node->latency_info.test_time);
		timeNow64_out = ((node->latency_info.test_time.tv_sec &
				0xFFFFFFFF) * 1000 * 1000)
			+ (node->latency_info.test_time.tv_usec);
		timePts_out = ((buf->vb.ts.tv_sec & 0xFFFFFFFF) * 1000 * 1000)
			+ (buf->vb.ts.tv_usec);
		/* pr_err("amlvideo2 out num:%d delay:%d\n",
		 * timePts_out, (timeNow64_out - timePts_out)/1000);
		 */
		if (node->latency_info.cur_time_out ==
		node->latency_info.test_time.tv_sec) {
			node->latency_info.total_latency_out +=
			(timeNow64_out - timePts_out) / 1000;
			node->latency_info.frame_num_out++;
		} else {
			node->latency_info.total_latency_out +=
			(timeNow64_out - timePts_out) / 1000;
			node->latency_info.frame_num_out++;
			pr_err("amvideo2 out latency:%d frame_num_out:%d\n",
			node->latency_info.total_latency_out /
			node->latency_info.frame_num_out,
			node->latency_info.frame_num_out);
			node->latency_info.total_latency_out = 0;
			node->latency_info.frame_num_out = 0;
			node->latency_info.cur_time_out =
			node->latency_info.test_time.tv_sec;
		}
	}
	#endif
	dpr_err(node->vid_dev, 2, "[%p/%d] wakeup\n", buf, buf->vb.i);
	return 0;

unlock: spin_unlock_irqrestore(&node->slock, flags);
	if (amlvideo2_dbg_en & 2)
		pr_info("unlock finish\n");
		if (node->pflag)
			complete(&node->plug_sema);
	return 0;
}

static void amlvideo2_sleep(struct amlvideo2_fh *fh)
{
	/* struct amlvideo2_node *node = fh->node; */
	/* struct amlvideo2_node_dmaqueue *dma_q = &node->vidq; */

	/* DECLARE_WAITQUEUE(wait, current); */

	/* dpr_err(node->vid_dev, 1, "%s dma_q=0x%08lx\n", __func__, */
	/* (unsigned long)dma_q); */

	/* add_wait_queue(&dma_q->wq, &wait); */
	/* if (kthread_should_stop()) */
	/* goto stop_task; */

	/* Calculate time to wake up */
	/* timeout = msecs_to_jiffies(frames_to_ms(1)); */

	if (amlvideo2_thread_tick(fh) < 0)
		schedule_timeout_interruptible(1);

	/* stop_task: */
	/* remove_wait_queue(&dma_q->wq, &wait); */
	try_to_freeze();
}

static int amlvideo2_thread(void *data)
{
	struct amlvideo2_fh *fh = data;
	struct amlvideo2_node *node = fh->node;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};
	int ret = 0;

	sched_setscheduler(current, SCHED_FIFO, &param);
	allow_signal(SIGTERM);

	if (amlvideo2_dbg_en) {
		if (node->vid == 0)
			pr_info("start amlvideo2.0 thread.\n");
		else
			pr_info("start amlvideo2.1 thread.\n");
	}
	dpr_err(node->vid_dev, 1, "thread started\n");

	set_freezable();

	while (1) {
		if (kthread_should_stop()) {
			if (amlvideo2_dbg_en & 2) {
				if (node->vid == 0)
					pr_info("node0 kthread stop 1.\n");
				else
					pr_info("node1 kthread stop 1.\n");
			}
			break;
		}

	#ifdef USE_SEMA_QBUF
		ret = wake_up_interruptible(&node->vidq.qbuf_comp);
	#endif
		if (amlvideo2_dbg_en & 2) {
			if (node->vid == 0)
				pr_info("node0 task_running = %d\n",
				node->vidq.task_running);
			else
				pr_info("node1 task_running = %d\n",
				node->vidq.task_running);
		}
		if (!node->vidq.task_running) {
			if (amlvideo2_dbg_en & 2) {
				if (node->vid == 0)
					pr_info("node0 here break.\n");
				else
					pr_info("node1 here break.\n");
			}
			break;
		}

		amlvideo2_sleep(fh);
		if (kthread_should_stop()) {
			if (amlvideo2_dbg_en & 2) {
				if (node->vid == 0)
					pr_info("node0 kthread stop 2.\n");
				else
					pr_info("node1 kthread stop 2.\n");
			}
			break;
		}
		if (amlvideo2_dbg_en & 2) {
			if (node->vid == 0)
				pr_info("amlvideo2.0_thread while 1 .\n");
			else
				pr_info("amlvideo2.1_thread while 1 .\n");
		}
	}
	while (!kthread_should_stop()) {
		if (amlvideo2_dbg_en & 2) {
			if (node->vid == 0)
				pr_info("amlvideo2.0_thread while 2 .\n");
			else
				pr_info("amlvideo2.1_thread while 2 .\n");
		}
		usleep_range(9000, 10000);
	}
		/*msleep(10);*/

	node->tmp_vf = NULL;
	if (amlvideo2_dbg_en) {
		if (node->vid == 0)
			pr_info("amlvideo2.0 thread exit.\n");
		else
			pr_info("amlvideo2.1 thread exit.\n");
	}
	dpr_err(node->vid_dev, 1, "thread: exit\n");
	return ret;
}

static int amlvideo2_start_thread(struct amlvideo2_fh *fh)
{
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	#ifdef USE_SEMA_QBUF
	init_completion(&dma_q->qbuf_comp);
	#endif
	dpr_err(node->vid_dev, 1, "%s\n", __func__);
	if (amlvideo2_dbg_en & 1)
		pr_info("begin amlvideo2_start_thread\n");
	mutex_lock(&node->mutex);
	if (dma_q->task_running) {
		mutex_unlock(&node->mutex);
		return 0;
	}

	fh->src_width = 0;
	fh->src_height = 0;
	node->tmp_vf = NULL;
	dma_q->task_running = 1;

	#ifdef MULTI_NODE
	dma_q->kthread =
		kthread_run(amlvideo2_thread, fh,
		(node->vid == 0)?"amlvideo2.0":"amlvideo2.1");
	#else
	dma_q->kthread = kthread_run(amlvideo2_thread, fh, "amlvideo2.0");
	#endif

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&node->vid_dev->v4l2_dev, "kernel_thread() failed\n");
		dma_q->task_running = 0;
		dma_q->kthread = NULL;
		mutex_unlock(&node->mutex);
		pr_info("start thread error.....\n");
		return PTR_ERR(dma_q->kthread);
	}
	mutex_unlock(&node->mutex);
	if (amlvideo2_dbg_en & 1)
		pr_info("success create amlvideo2 thread .\n");
	/* Wakes thread */
	/* wake_up_interruptible(&dma_q->wq); */

	dpr_err(node->vid_dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void amlvideo2_stop_thread(struct amlvideo2_node_dmaqueue *dma_q)
{
	int ret = 0;
	struct amlvideo2_node *node =
		container_of(dma_q, struct amlvideo2_node, vidq);
	dpr_err(node->vid_dev, 1, "%s\n", __func__);
	if (amlvideo2_dbg_en & 1) {
		if (node->vid == 0)
			pr_info("begin to stop amlvideo2.0 thread\n");
		else
			pr_info("begin to stop amlvideo2.1 thread\n");
	}
	mutex_lock(&node->mutex);
	/* shutdown control thread */
	if (!IS_ERR(dma_q->kthread)) {
		dma_q->task_running = 0;
		send_sig(SIGTERM, dma_q->kthread, 1);
	#ifdef USE_SEMA_QBUF
		complete(&dma_q->qbuf_comp);
	#endif
		complete(&node->plug_sema);
		wake_up_interruptible(&dma_q->wq);
		if (amlvideo2_dbg_en & 1) {
			if (node->vid == 0)
				pr_info("ready to stop amlvideo2.0 thread\n");
			else
				pr_info("ready to stop amlvideo2.1 thread\n");
		}
		ret = kthread_stop(dma_q->kthread);
		if (ret < 0)
			pr_info("%s, ret = %d .\n", __func__, ret);

		dma_q->kthread = NULL;
	}
	mutex_unlock(&node->mutex);
	if (amlvideo2_dbg_en & 1) {
		if (node->vid == 0)
			pr_info("finish stop amlvideo2.0 thread\n");
		else
			pr_info("finish stop amlvideo2.1 thread\n");
	}
}

enum aml_provider_type_e get_provider_type(const char *name)
{
	enum aml_provider_type_e type = AML_PROVIDE_NONE;

	if (!name)
		return type;
	if (strncasecmp(name, "vdin0", 5) == 0)
		type = AML_PROVIDE_VDIN0;
	else if (strncasecmp(name, "vdin1", 5) == 0)
		type = AML_PROVIDE_VDIN1;
	else if (strncasecmp(name, "decoder", 7) == 0)
		type = AML_PROVIDE_DECODE;
	else if (strncasecmp(name, "ppmgr", 5) == 0)
		type = AML_PROVIDE_PPMGR;
	else
		type = AML_PROVIDE_MAX;
	return type;
}

enum aml_provider_type_e get_sub_receiver_type(const char *name)
{
	enum aml_provider_type_e type = AML_RECEIVER_NONE;

	if (!name)
		return type;
	if (strncasecmp(name, "ppmgr", 5) == 0) {
		type = AML_RECEIVER_PPMGR;
		if (amlvideo2_dbg_en)
			pr_info("type is ppmgr");
	} else if (strncasecmp(name, "deinterlace", 11) == 0) {
		type = AML_RECEIVER_DI;
		if (amlvideo2_dbg_en)
			pr_info("type is deinterlace");
	} else {
		type = AML_RECEIVER_MAX;
		if (amlvideo2_dbg_en)
			pr_info("type is not certain\n");
	}
	return type;
}
/* ------------------------------------------------------------------
 *           provider operations
 * ------------------------------------------------------------------
 */
static struct vframe_s *amlvideo2_vf_peek(void *op_arg)
{
	struct amlvideo2_node *node = (struct amlvideo2_node *)op_arg;

	if (node->video_blocking)
		return NULL;
	if (amlvideo2_dbg_en & 8)
		pr_info("amlvideo2 vf peek .\n");
	return vfq_peek(&node->q_ready);
}

static struct vframe_s *amlvideo2_vf_get(void *op_arg)
{
	struct vframe_s *vf = NULL;
	struct amlvideo2_node *node = (struct amlvideo2_node *)op_arg;

	if (node->video_blocking)
		return NULL;
	if (amlvideo2_dbg_en & 8)
		pr_info("amlvideo2 vf get .\n");
	mutex_lock(&node->mutex);
	vf = vfq_pop(&node->q_ready);
	mutex_unlock(&node->mutex);
	return vf;
}

static void amlvideo2_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct amlvideo2_node *node = (struct amlvideo2_node *)op_arg;
	char *name = (node->vid == 0) ? DEVICE_NAME0 : DEVICE_NAME1;

	if (node->video_blocking)
		return;
	if (amlvideo2_dbg_en & 8)
		pr_info("amlvideo2 vf put .\n");
	vf_put(vf, name);
}

static int amlvideo2_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_PUT) {
		/* pr_err("video put, avail=%d\n", vfq_level(&q_ready) ); */
	} else if (type & VFRAME_EVENT_RECEIVER_GET) {
		/* pr_err("video get, avail=%d\n", vfq_level(&q_ready) ); */
	} else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT) {
		/* up(&thread_sem); */
		pr_err("receiver is waiting\n");
	} else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT) {
		pr_err("frame wait\n");
	}
	return 0;
}

static int amlvideo2_vf_states(struct vframe_states *states, void *op_arg)
{
	/* unsigned long flags; */
	/* spin_lock_irqsave(&lock, flags); */
	struct amlvideo2_node *node = (struct amlvideo2_node *)op_arg;

	states->vf_pool_size = node->amlvideo2_pool_size;
	states->buf_recycle_num = 0;
	states->buf_free_num = node->amlvideo2_pool_size -
		vfq_level(&node->q_ready);
	states->buf_avail_num = vfq_level(&node->q_ready);
	/* spin_unlock_irqrestore(&lock, flags); */
	return 0;
}

static const struct vframe_operations_s amlvideo2_vf_provider = {
.peek = amlvideo2_vf_peek,
.get = amlvideo2_vf_get,
.put = amlvideo2_vf_put,
.event_cb = amlvideo2_event_cb,
.vf_states = amlvideo2_vf_states, };


/* ------------------------------------------------------------------
 * Videobuf operations
 * ------------------------------------------------------------------
 */
static int buffer_setup(struct videobuf_queue *vq, unsigned int *count,
			unsigned int *size)
{
	struct videobuf_res_privdata *res = (struct videobuf_res_privdata *)vq
		->priv_data;
	struct amlvideo2_fh *fh = (struct amlvideo2_fh *)res->priv;
	struct amlvideo2_node *node = fh->node;
	int height = fh->height;

	if (height % 16 != 0)
		height = ((height + 15) >> 4) << 4;

	*size = (fh->width * height * fh->fmt->depth) >> 3;
	if (*count == 0)
		*count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
		(*count)--;

	dpr_err(
		node->vid_dev, 1,
		"%s, count=%d, size=%d\n", __func__, *count, *size);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq,
			struct amlvideo2_node_buffer *buf)
{
	struct videobuf_res_privdata *res = (struct videobuf_res_privdata *)vq
		->priv_data;
	struct amlvideo2_fh *fh = (struct amlvideo2_fh *)res->priv;
	struct amlvideo2_node *node = fh->node;

	dpr_err(node->vid_dev, 1, "%s, state: %i\n", __func__, buf->vb.state);
	videobuf_waiton(vq, &buf->vb, 0, 0);
	if (in_interrupt())
		WARN_ON(1);
	videobuf_res_free(vq, &buf->vb);
	dpr_err(node->vid_dev, 1, "free_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define norm_maxw() 2000
#define norm_maxh() 1600

static int buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
				enum v4l2_field field)
{
	struct videobuf_res_privdata *res = (struct videobuf_res_privdata *)vq
		->priv_data;
	struct amlvideo2_fh *fh = (struct amlvideo2_fh *)res->priv;
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_buffer *buf = container_of(
		vb, struct amlvideo2_node_buffer, vb);
	int rc;

	dpr_err(node->vid_dev, 1, "%s, field=%d\n", __func__, field);

	WARN_ON(fh->fmt == NULL);

	if (fh->width < 16 || fh->width > norm_maxw() ||
	fh->height < 16 || fh->height > norm_maxh())
		return -EINVAL;

	buf->vb.size = (fh->width * fh->height * fh->fmt->depth) >> 3;
	if (buf->vb.baddr != 0 && buf->vb.bsize < buf->vb.size)
		return -EINVAL;
	/* These properties only change when queue is idle, see s_fmt */
	buf->fmt = fh->fmt;
	buf->vb.width = fh->width;
	buf->vb.height = fh->height;
	buf->vb.field = field;
	if (buf->vb.state == VIDEOBUF_NEEDS_INIT) {
		rc = videobuf_iolock(vq, &buf->vb, NULL);
		if (rc < 0)
			goto fail;
	}
	buf->vb.state = VIDEOBUF_PREPARED;
	return 0;

fail: free_buffer(vq, buf);
	return rc;
}

static void buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct amlvideo2_node_buffer *buf = container_of(
		vb, struct amlvideo2_node_buffer, vb);
	struct videobuf_res_privdata *res = (struct videobuf_res_privdata *)vq
		->priv_data;
	struct amlvideo2_fh *fh = (struct amlvideo2_fh *)res->priv;
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *vidq = &node->vidq;

	dpr_err(node->vid_dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
				struct videobuf_buffer *vb)
{
	struct amlvideo2_node_buffer *buf = container_of(
		vb, struct amlvideo2_node_buffer, vb);
	free_buffer(vq, buf);
}

static struct videobuf_queue_ops amlvideo2_qops = {
.buf_setup = buffer_setup,
.buf_prepare = buffer_prepare,
.buf_queue = buffer_queue,
.buf_release = buffer_release, };

/* ------------------------------------------------------------------
 * IOCTL vidioc handling
 * ------------------------------------------------------------------
 */
static int vidioc_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;

	strcpy(cap->driver, "amlvideo2");
	strcpy(cap->card, "amlvideo2");
	strlcpy(
		cap->bus_info,
		node->vid_dev->v4l2_dev.name,
		sizeof(cap->bus_info));
	cap->version = AMLVIDEO2_VERSION;
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE;
	cap->capabilities = cap->device_caps
		|V4L2_CAP_DEVICE_CAPS
		|V4L2_CAP_STREAMING
		| V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_fmtdesc *f)
{
	struct amlvideo2_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct amlvideo2_fh *fh = priv;

	if (fh->set_format_flag) {
		f->fmt.pix.width = fh->width;
		f->fmt.pix.height = fh->height;
		f->fmt.pix.field = fh->vb_vidq.field;
		f->fmt.pix.pixelformat = fh->fmt->fourcc;
		f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fh->fmt->depth) >> 3;
		f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;
	} else {

		if ((fh->node->start_vdin_flag) && (fh->node->provide_ready))  {
			const struct vinfo_s *vinfo;

			vinfo = get_current_vinfo();
			f->fmt.pix.width = vinfo->width;
			f->fmt.pix.height = vinfo->height;
		}  else if (fh->node->provide_ready)  {
			f->fmt.pix.width = fh->src_width;
			f->fmt.pix.height = fh->src_height;
		} else {
			f->fmt.pix.width = 0;
			f->fmt.pix.height = 0;
		}
	}
	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_fmt *fmt = NULL;
	enum v4l2_field field;
	unsigned int maxw, maxh;

	fmt = get_format(f);
	if (!fmt) {
		dpr_err(node->vid_dev, 1, "Fourcc format (0x%08x) invalid.\n",
			f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	field = f->fmt.pix.field;

	if (field == V4L2_FIELD_ANY) {
		field = V4L2_FIELD_INTERLACED;
	} else if (field != V4L2_FIELD_INTERLACED) {
		dpr_err(node->vid_dev, 1, "Field type invalid.\n");
		return -EINVAL;
	}

	maxw = norm_maxw();
	maxh = norm_maxh();

	f->fmt.pix.field = field;
	v4l_bound_align_image(
		&f->fmt.pix.width, 16,
		maxw, 2, &f->fmt.pix.height, 16,
		maxh, 0, 0);
	f->fmt.pix.bytesperline = (f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;
	return 0;
}

/*FIXME: This seems to be generic enough to be at videodev2 */
static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	int ret = 0;
	struct amlvideo2_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;

	f->fmt.pix.width = (f->fmt.pix.width + (CANVAS_WIDTH_ALIGN-1)) &
				(~(CANVAS_WIDTH_ALIGN-1));
	if ((f->fmt.pix.pixelformat == V4L2_PIX_FMT_YVU420)  ||
			(f->fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420)) {
		f->fmt.pix.width =
			(f->fmt.pix.width + (CANVAS_WIDTH_ALIGN*2-1)) &
			(~(CANVAS_WIDTH_ALIGN*2-1));
	}

	ret = vidioc_try_fmt_vid_cap(file, fh, f);
	if (ret < 0)
		return ret;

	mutex_lock(&q->vb_lock);

	if (videobuf_queue_is_busy(&fh->vb_vidq)) {
		dpr_err(fh->node->vid_dev, 1, "%s queue busy\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	fh->fmt = get_format(f);
	fh->width = f->fmt.pix.width;
	fh->height = f->fmt.pix.height;
	fh->vb_vidq.field = f->fmt.pix.field;
	fh->type = f->type;
	fh->set_format_flag = true;
	ret = 0;
out: mutex_unlock(&q->vb_lock);
	return ret;
}

/*
 * Implement G/S_PARM.  There is a "high quality" mode we could try
 * to do someday; for now, we just do the frame rate tweak.
 * V4L2_CAP_TIMEPERFRAME need to be supported furthermore.
 */
static int vidioc_g_parm(struct file *file, void *priv,
				struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;

	pr_err("vidioc_g_parm\n");
	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;

	cp->timeperframe = amlvideo2_frmintervals_active;
	pr_err("g_parm,deno=%d, numerator=%d\n", cp->timeperframe.denominator,
		cp->timeperframe.numerator);

	return 0;
}

static int vidioc_s_parm(struct file *file, void *priv,
				struct v4l2_streamparm *parms)
{
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	suseconds_t def_ival = 1000000 / DEF_FRAMERATE;
	suseconds_t ival; /* us */

	/*save the frame period. */
	if (parms->parm.capture.timeperframe.denominator == 0) {
		fh->frm_save_time_us = def_ival;
		amlvideo2_frmintervals_active.numerator = 1;
		amlvideo2_frmintervals_active.denominator = DEF_FRAMERATE;
		return -EINVAL;
	}

	ival = parms->parm.capture.timeperframe.numerator * 1000000
		/ parms->parm.capture.timeperframe.denominator;

	fh->frm_save_time_us = ival;
	amlvideo2_frmintervals_active = parms->parm.capture.timeperframe;

	dpr_err(
		node->vid_dev, 1,
		"%s,%d,type=%d\n",
		__func__, __LINE__, parms->type);
	dpr_err(node->vid_dev, 1, "setting framerate:%d/%d(fps)\n",
		amlvideo2_frmintervals_active.denominator,
		amlvideo2_frmintervals_active.numerator);

	return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
				struct v4l2_requestbuffers *p)
{
	struct amlvideo2_fh *fh = priv;
	#ifdef USE_SEMA_QBUF
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;

	init_completion(&dma_q->qbuf_comp);
	#endif
	return videobuf_reqbufs(&fh->vb_vidq, p);
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct amlvideo2_fh *fh = priv;
	int ret = videobuf_querybuf(&fh->vb_vidq, p);
	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	#if 1
	if (ret == 0) {
		struct amlvideo2_output output;

		memset(&output, 0, sizeof(struct amlvideo2_output));

		output.v4l2_format = fh->fmt->fourcc;
		output.vbuf = NULL;
		output.width = fh->width;
		output.height = fh->height;
		output.canvas_id = -1;
		p->reserved = convert_canvas_index(
			&output, fh->node->vid == 0 ?
			(AMLVIDEO2_RES0_CANVAS_INDEX + p->index * 3) :
			(AMLVIDEO2_RES1_CANVAS_INDEX + p->index * 3));
	} else {
		p->reserved = 0;
	}
	#endif
	return ret;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct amlvideo2_fh *fh = priv;
	#ifdef USE_SEMA_QBUF
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	#endif
	int ret = videobuf_qbuf(&fh->vb_vidq, p);
	#ifdef USE_SEMA_QBUF
	complete(&dma_q->qbuf_comp);
	#endif
	return ret;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct amlvideo2_fh *fh = priv;
	int ret = videobuf_dqbuf(&fh->vb_vidq, p, file->f_flags & O_NONBLOCK);
	return ret;
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct amlvideo2_fh *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif
static enum tvin_scan_mode_e vmode2scan_mode(enum vmode_e mode)
{
	enum tvin_scan_mode_e scan_mode =
		TVIN_SCAN_MODE_NULL;/* 1: progressive 2:interlaced */

#if 0 //DEBUG_TMP
	switch (mode) {
	case VMODE_480I:
	case VMODE_480CVBS:
	case VMODE_NTSC_M:
	case VMODE_576I:
	case VMODE_576CVBS:
	case VMODE_PAL_M:
	case VMODE_PAL_N:
	case VMODE_1080I:
	case VMODE_1080I_50HZ:
		scan_mode = TVIN_SCAN_MODE_INTERLACED;
		break;
	case VMODE_480P:
	case VMODE_576P:
	case VMODE_720P:
	case VMODE_1080P:
	case VMODE_720P_50HZ:
	case VMODE_1080P_50HZ:
	case VMODE_1080P_24HZ:
	case VMODE_4K2K_30HZ:
	case VMODE_4K2K_25HZ:
	case VMODE_4K2K_24HZ:
	case VMODE_4K2K_SMPTE:
	case VMODE_VGA:
	case VMODE_SVGA:
	case VMODE_XGA:
	case VMODE_SXGA:
	case VMODE_LCD:
	case VMODE_4K2K_50HZ:
	case VMODE_4K2K_50HZ_Y420:
	case VMODE_4K2K_60HZ:
	case VMODE_4K2K_60HZ_Y420:
		scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
		break;
	default:
		pr_err("unknown mode=%d\n", mode);
		break;
	}
	/* pr_err("mode=%d, scan_mode=%d\n", mode, scan_mode); */
#endif
	return scan_mode;
}

/*the counter of AMLVIDEO2*/
#define AMLVIDEO2_MAX_NODE		2
static struct amlvideo2_node  *gAmlvideo2_Node[AMLVIDEO2_MAX_NODE];
static int amlvideo2_stop_tvin_service(struct amlvideo2_node *node)
{
	int ret = 0;
	struct vdin_v4l2_ops_s *vops = &node->vops;

	if (amlvideo2_dbg_en)
		pr_info("%s , %d\n", __func__, __LINE__);

	if (node->r_type == AML_RECEIVER_NONE)
		amlvideo2_stop_thread(&node->vidq);

	if (((node->input == 0) || (node->input == 0x1000C000)) &&
			(node->r_type == AML_RECEIVER_NONE)) {
		if (amlvideo2_dbg_en)
			pr_info("stop tvin service .\n");
		if (!(vops->stop_tvin_service)) {
			pr_info("stop_tvin_service func is NULL.\n");
			return -1;
		}
		vops->stop_tvin_service(node->vdin_device_num);
	}

	node->start_vdin_flag = 0;
	return ret;
}

static int amlvideo2_start_tvin_service(struct amlvideo2_node *node)
{
	struct amlvideo2_fh *fh = node->fh;
	struct vdin_v4l2_ops_s *vops = &node->vops;
	struct vdin_parm_s para;
	const struct vinfo_s *vinfo;
	int dst_w, dst_h;

	vinfo = get_current_vinfo();

	if ((!node->start_vdin_flag) || (node->r_type != AML_RECEIVER_NONE))
		goto start;

	if (amlvideo2_dbg_en)
		pr_info("Enter %s .\n", __func__);

	memset(&para, 0, sizeof(para));
	para.port = node->porttype;
	para.fmt = TVIN_SIG_FMT_MAX;
	para.frame_rate = vinfo->sync_duration_num/vinfo->sync_duration_den;
	para.h_active = vinfo->width;
	para.v_active = vinfo->height;
	para.hsync_phase = 0;
	para.vsync_phase = 1;
	para.hs_bp = 0;
	para.vs_bp = 0;
	para.dfmt = TVIN_NV21;/* TVIN_YUV422; */
	para.scan_mode = vmode2scan_mode(
		vinfo->mode);/* TVIN_SCAN_MODE_PROGRESSIVE; */
	if (para.scan_mode == TVIN_SCAN_MODE_INTERLACED)
		para.v_active = para.v_active / 2;

	dst_w = fh->width;
	dst_h = fh->height;
	if (vinfo->width < vinfo->height) {
		if ((vinfo->width <= 768) && (vinfo->height <= 1024)) {
			dst_w = vinfo->width;
			dst_h = vinfo->height;
		} else {
			dst_w = fh->height;
			dst_h = fh->width;
		}
		output_axis_adjust(vinfo->height, vinfo->width, (int *)&dst_h,
					(int *)&dst_w, 0, NULL);
	} else {
		if ((vinfo->height <= 768) && (vinfo->width <= 1024)) {
			dst_w = vinfo->width;
			dst_h = vinfo->height;
		}
		output_axis_adjust(vinfo->width, vinfo->height, (int *)&dst_w,
					(int *)&dst_h, 0, NULL);
	}
	para.dest_hactive = dst_w;
	para.dest_vactive = dst_h;
	if (para.scan_mode == TVIN_SCAN_MODE_INTERLACED)
		para.dest_vactive = para.dest_vactive / 2;
	if (para.port == TVIN_PORT_VIDEO) {
		para.dest_hactive = 0;
		para.dest_vactive = 0;
	}
	if (amlvideo2_dbg_en) {
		pr_info("node->r_type=%d, node->p_type=%d\n",
			node->r_type, node->p_type);
		pr_info("para.h_active: %d, para.v_active: %d,",
			para.h_active, para.v_active);
		pr_info("para.dest_hactive: %d, para.dest_vactive: %d,",
			para.dest_hactive, para.dest_vactive);
		pr_info("fh->width: %d, fh->height: %d,",
			fh->width, fh->height);
		pr_info("vinfo->mode: %d,para.scan_mode: %d\n",
			vinfo->mode, para.scan_mode);
		pr_info("node->vdin_device_num = %d .\n",
			node->vdin_device_num);
	}
	if (!(vops->start_tvin_service)) {
		pr_info("start_tvin_service is NULL.\n");
		return -1;
	}
	vops->start_tvin_service(node->vdin_device_num, &para);

start: node->frame_inittime = 1;
	/* frameInv_adjust = 0; */
	/* frameInv = 0; */
	/* tmp_vf = NULL; */
	do_gettimeofday(&node->thread_ts1);
	return 0;
}
int amlvideo2_notify_callback(struct notifier_block *block, unsigned long cmd,
			void *para)
{
	struct amlvideo2_node  *node = NULL;
	struct vframe_s *recycle_vf = NULL;
	struct vframe_states states;
	struct vframe_provider_s *vfp = NULL;
	int i;
	int index = 0;
	int ret = 0;

	for (i = 0; i < AMLVIDEO2_MAX_NODE; i++) {
		if ((gAmlvideo2_Node[i] != NULL) &&
			(gAmlvideo2_Node[i]->r_type == AML_RECEIVER_NONE))
			index = 1;
	}

	if (amlvideo2_dbg_en)
		pr_info("%s , index = %d\n", __func__, index);

	if (index != 1)
		return ret;

	node = gAmlvideo2_Node[index];
	if ((node != NULL) && (!node->users))
		return ret;

	mutex_lock(&node->mutex);
	switch (cmd) {
	case  VOUT_EVENT_MODE_CHANGE:
		pr_info("mode changed in amlvideo2 .\n");
		vfp = vf_get_provider(node->recv.name);
		if ((node == NULL) || (vfp == NULL) ||
			(!node->fh->is_streamed_on)) {
			pr_info("driver is not ready or not need to screencap.\n");
			mutex_unlock(&node->mutex);
			return ret;
		}
		node->pflag = true;
		wait_for_completion_timeout(&node->plug_sema,
			msecs_to_jiffies(150));
		if (amlvideo2_dbg_en)
			pr_info("finish wait plug sema .\n");
		/* if local queue have vf , should give back to provider */
		if (vfq_empty(&node->q_ready)) {
			if (amlvideo2_dbg_en)
				pr_info("q_ready is empty .\n");
		} else {
			recycle_vf = vfq_pop(&node->q_ready);
			while (recycle_vf) {
				vf_put(recycle_vf, node->recv.name);
				recycle_vf = vfq_pop(&node->q_ready);
			}
			if (amlvideo2_dbg_en)
				pr_info("already flush local vf .\n");
		}
		/*debug provider vf state*/
		if (amlvideo2_dbg_en) {
			ret = vf_get_states(vfp, &states);
			if (ret == 0) {
				pr_info("vf_pool_size = %d, buf_free_num = %d .\n",
				states.vf_pool_size, states.buf_free_num);
				pr_info("buf_recycle_num = %d, buf_avail_num = %d .\n",
				states.buf_recycle_num, states.buf_avail_num);
			}
		}
		ret = amlvideo2_stop_tvin_service(node);
		if (ret < 0) {
			pr_err("stop tvin service failed.\n");
			mutex_unlock(&node->mutex);
			node->pflag = false;
			return ret;
		}

		if (node->r_type == AML_RECEIVER_NONE)
			amlvideo2_start_thread(node->fh);
		msleep(500);

		ret = amlvideo2_start_tvin_service(node);
		if (ret < 0) {
			pr_err("start tvin service failed.\n");
			mutex_unlock(&node->mutex);
			node->pflag = false;
			return ret;
		}
		node->pflag = false;
		break;
	}
	mutex_unlock(&node->mutex);
	if (amlvideo2_dbg_en)
		pr_info("finish amlvideo2_notify_callback .\n");
	return ret;
	return 0;
}


static struct notifier_block amlvideo2_notifier_nb = {
	.notifier_call	= amlvideo2_notify_callback,
};

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	int ret;
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	struct vdin_v4l2_ops_s *vops = &node->vops;
	struct vdin_parm_s para;
	const struct vinfo_s *vinfo;
	int dst_w, dst_h;

	vinfo = get_current_vinfo();
	if ((fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) || (i != fh->type))
		return -EINVAL;
	ret = videobuf_streamon(&fh->vb_vidq);
	if (ret < 0) {
		pr_err("%s, videobuf_streamon() ret: %d\n", __func__, ret);
		return ret;
	}

	memset(&node->display_info, 0, sizeof(struct vdisplay_info_s));
	if (!node->start_vdin_flag) {
		ret = vf_notify_receiver_by_name("amvideo",
			  VFRAME_EVENT_PROVIDER_QUREY_DISPLAY_INFO,
			 &node->display_info);
		if (ret < 0) {
			pr_err("notify amvideo failed.\n");
			node->mode = AML_SCREEN_MODE_RATIO;
			/* AML_SCREEN_MODE_RATIO; */
		} else {
			node->has_amvideo_node = true;
			node->mode = AML_SCREEN_MODE_ADAPTIVE;
			/* AML_SCREEN_MODE_ADAPTIVE; */
			if (amlvideo2_dbg_en) {
				pr_info("screen:h_start = %d,h_end = %d\n",
				node->display_info.screen_vd_h_start_,
				node->display_info.screen_vd_h_end_);
				pr_info("screen:v_start = %d,v_end = %d\n",
				node->display_info.screen_vd_v_start_,
				node->display_info.screen_vd_v_end_);
				pr_info("display:hsc_startp=%d,hsc_endp=%d\n",
				node->display_info.display_hsc_startp,
				node->display_info.display_hsc_endp);
				pr_info("display:vsc_startp=%d,vsc_endp=%d\n",
				node->display_info.display_vsc_startp,
				node->display_info.display_vsc_endp);
				pr_info("frame:hd_start_lines=%d,hd_end_lines=%d\n",
				node->display_info.frame_hd_start_lines_,
				node->display_info.frame_hd_end_lines_);
				pr_info("frame:vd_start_lines=%d,vd_end_lines=%d\n",
				node->display_info.frame_vd_start_lines_,
				node->display_info.frame_vd_end_lines_);
			}
		}
	}

	if (amlvideo2_dbg_en) {
		pr_info("amlvideo2--vidioc_streamon .\n");
		pr_info("crop_enable = %d\n",
			node->crop_info.capture_crop_enable);
		pr_info("node->r_type=%d, node->p_type=%d\n",
			node->r_type, node->p_type);
	}

	if ((!node->start_vdin_flag) || (node->r_type != AML_RECEIVER_NONE))
		goto start;

	if (node->r_type == AML_RECEIVER_NONE)
		amlvideo2_start_thread(fh);

	memset(&para, 0, sizeof(para));
	para.port = node->porttype;
	para.fmt = TVIN_SIG_FMT_MAX;
	para.frame_rate = vinfo->sync_duration_num/vinfo->sync_duration_den;
	para.h_active = vinfo->width;
	para.v_active = vinfo->height;
	para.hsync_phase = 0;
	para.vsync_phase = 1;
	para.hs_bp = 0;
	para.vs_bp = 0;
	para.dfmt = TVIN_NV21;/* TVIN_YUV422; */
	para.scan_mode = vmode2scan_mode(
		vinfo->mode);/* TVIN_SCAN_MODE_PROGRESSIVE; */
	if (para.scan_mode == TVIN_SCAN_MODE_INTERLACED)
		para.v_active = para.v_active / 2;

	dst_w = fh->width;
	dst_h = fh->height;
	if (vinfo->width < vinfo->height) {
		if ((vinfo->width <= 768) && (vinfo->height <= 1024)) {
			dst_w = vinfo->width;
			dst_h = vinfo->height;
		} else {
			dst_w = fh->height;
			dst_h = fh->width;
		}
		output_axis_adjust(vinfo->height, vinfo->width, (int *)&dst_h,
					(int *)&dst_w, 0, NULL);
	} else {
		if ((vinfo->height <= 768) && (vinfo->width <= 1024)) {
			dst_w = vinfo->width;
			dst_h = vinfo->height;
		}
		output_axis_adjust(vinfo->width, vinfo->height, (int *)&dst_w,
					(int *)&dst_h, 0, NULL);
	}
	para.dest_hactive = dst_w;
	para.dest_vactive = dst_h;
	if (para.scan_mode == TVIN_SCAN_MODE_INTERLACED)
		para.dest_vactive = para.dest_vactive / 2;
	if (para.port == TVIN_PORT_VIDEO) {
		if (node->ge2d_multi_process_flag) {
			para.dest_hactive = 384;
			para.dest_vactive = 216;
		} else {
			para.dest_hactive = 0;
			para.dest_vactive = 0;
		}
	}
	if (amlvideo2_dbg_en) {
		pr_info("para.h_active: %d, para.v_active: %d,",
			para.h_active, para.v_active);
		pr_info("para.dest_hactive: %d, para.dest_vactive: %d,",
			para.dest_hactive, para.dest_vactive);
		pr_info("fh->width: %d, fh->height: %d,",
			fh->width, fh->height);
		pr_info("vinfo->mode: %d,para.scan_mode: %d\n",
			vinfo->mode, para.scan_mode);
		pr_info("node->vdin_device_num = %d .\n",
			node->vdin_device_num);
	}
	vops->start_tvin_service(node->vdin_device_num, &para);

start: node->frame_inittime = 1;
	fh->is_streamed_on = 1;
	/* frameInv_adjust = 0; */
	/* frameInv = 0; */
	/* tmp_vf = NULL; */
	do_gettimeofday(&node->thread_ts1);
	#ifdef TEST_LATENCY
	node->latency_info.cur_time = node->latency_info.cur_time_out =
		node->thread_ts1.tv_sec;
	node->latency_info.total_latency = 0;
	node->latency_info.total_latency_out = 0;
	#endif
	return 0;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	int ret;
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	struct vdin_v4l2_ops_s *vops = &node->vops;

	if ((fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) || (i != fh->type))
		return -EINVAL;
	ret = videobuf_streamoff(&fh->vb_vidq);
	if (ret < 0)
		pr_err("videobuf stream off failed\n");
	if (amlvideo2_dbg_en) {
		pr_info("%s , %d\n", __func__, __LINE__);
		pr_info("start_vdin_flag = %d\n", node->start_vdin_flag);
		pr_info("node->r_type = %d, node->vid = %d\n",
			node->r_type, node->vid);
		pr_info("vdin_device_num = %d\n", node->vdin_device_num);
	}
	if ((node->start_vdin_flag) ||
		(node->r_type == AML_RECEIVER_NONE)) {
		if (amlvideo2_dbg_en)
			pr_info("stop tvin service .\n");
		vops->stop_tvin_service(node->vdin_device_num);
	}
	if (node->r_type == AML_RECEIVER_NONE)
		amlvideo2_stop_thread(&node->vidq);

	node->start_vdin_flag = 0;
	fh->is_streamed_on = 0;
	return ret;
}

static int vidioc_enum_framesizes(struct file *file, void *fh,
					struct v4l2_frmsizeenum *fsize)
{
	int ret = 0, i = 0;
	struct amlvideo2_fmt *fmt = NULL;
	struct v4l2_frmsize_discrete *frmsize = NULL;

	for (i = 0; i < ARRAY_SIZE(formats); i++) {
		if (formats[i].fourcc == fsize->pixel_format) {
			fmt = &formats[i];
			break;
		}
	}
	if (fmt == NULL)
		return -EINVAL;
	if ((fmt->fourcc == V4L2_PIX_FMT_NV21)
		|| (fmt->fourcc == V4L2_PIX_FMT_NV12)
		|| (fmt->fourcc == V4L2_PIX_FMT_YUV420)
		|| (fmt->fourcc == V4L2_PIX_FMT_YVU420)) {
		if (fsize->index >= ARRAY_SIZE(amlvideo2_prev_resolution))
			return -EINVAL;
		frmsize = &amlvideo2_prev_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	} else if ((fmt->fourcc == V4L2_PIX_FMT_RGB24) ||
			(fmt->fourcc == V4L2_PIX_FMT_RGB32)) {
		if (fsize->index >= ARRAY_SIZE(amlvideo2_pic_resolution))
			return -EINVAL;
		frmsize = &amlvideo2_pic_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	return ret;
}
static int vidioc_enum_frameintervals(struct file *file, void *priv,
					struct v4l2_frmivalenum *fival)
{
	unsigned int k;

	if (fival->index > ARRAY_SIZE(amlvideo2_frmivalenum))
		return -EINVAL;
	for (k = 0; k < ARRAY_SIZE(amlvideo2_frmivalenum); k++) {
		if ((fival->index == amlvideo2_frmivalenum[k].index)
			&& (fival->pixel_format
				== amlvideo2_frmivalenum[k].pixel_format)
			&& (fival->width == amlvideo2_frmivalenum[k].width)
			&& (fival->height
				== amlvideo2_frmivalenum[k].height)) {
			memcpy(fival, &amlvideo2_frmivalenum[k],
				sizeof(struct v4l2_frmivalenum));
			return 0;
		}
	}
	return -EINVAL;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id i)
{
	return 0;
}

/* --- controls ---------------------------------------------- */

static int amlvideo2_setting(struct amlvideo2_node *node, int PROP_ID,
				int value, int index)
{
	int ret = 0;

	switch (PROP_ID) {
	case V4L2_CID_ROTATE:
		if (node->qctl_regs[index] != value)
			node->qctl_regs[index] = value;

		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}

static int vidioc_queryctrl(struct file *file, void *priv,
				struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(amlvideo2_node_qctrl); i++)
		if (qc->id && qc->id == amlvideo2_node_qctrl[i].id) {
			memcpy(qc, &(amlvideo2_node_qctrl[i]), sizeof(*qc));
			return 0;
		}
	return -EINVAL;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	*i = node->input;
	return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	/*bit 28 : start tvin service flag, 1 : enable,  0 : disable*/
	node->start_vdin_flag = (i >> 28);
	/*bit 24 : vdin device num : 0 or 1 */
	node->vdin_device_num = (i >> 24) & 1;
	node->ge2d_multi_process_flag = (i >> 16) & 1;
	node->porttype = (i & 0xffff);
	if (amlvideo2_dbg_en) {
		pr_info("porttype:%x ,start_vdin_flag = %d.\n",
			node->porttype, node->start_vdin_flag);
		pr_info("%s, vdin_device_num = %d\n",
			__func__, node->vdin_device_num);
		pr_info("%s, ge2d_multi_process_flag = %d\n",
			__func__, node->ge2d_multi_process_flag);
	}
	return 0;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	int i;
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;

	for (i = 0; i < ARRAY_SIZE(amlvideo2_node_qctrl); i++)
		if (ctrl->id == amlvideo2_node_qctrl[i].id) {
			ctrl->value = node->qctl_regs[i];
			return 0;
		}
	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	int i;
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;

	for (i = 0; i < ARRAY_SIZE(amlvideo2_node_qctrl); i++)
		if (ctrl->id == amlvideo2_node_qctrl[i].id) {
			if (ctrl->value < amlvideo2_node_qctrl[i].minimum
				|| ctrl->value
				> amlvideo2_node_qctrl[i].maximum
				|| amlvideo2_setting(
					node, ctrl->id, ctrl->value, i) < 0) {
				return -ERANGE;
			}
			/* node->qctl_regs[i] = ctrl->value; */
			return 0;
		}
	return -EINVAL;
}

static int vidioc_g_crop(struct file *file, void *fh,
					struct v4l2_crop *a)
{
	struct amlvideo2_node *node = video_drvdata(file);

	a->c.top = node->crop_info.source_top_crop;
	a->c.left = node->crop_info.source_left_crop;
	a->c.width = node->crop_info.source_width_crop;
	a->c.height = node->crop_info.source_height_crop;
	return 0;
}

static int vidioc_s_crop(struct file *file, void *fh,
					const struct v4l2_crop *a)
{
	struct amlvideo2_node *node = video_drvdata(file);

	if (a->c.width == 0 || a->c.height == 0) {
		pr_info("disable capture crop\n");
		node->crop_info.capture_crop_enable = 0;
		node->crop_info.source_top_crop = 0;
		node->crop_info.source_left_crop = 0;
		node->crop_info.source_width_crop = 0;
		node->crop_info.source_height_crop = 0;
	} else {
		node->crop_info.source_top_crop = a->c.top;
		node->crop_info.source_left_crop = a->c.left;
		node->crop_info.source_width_crop = a->c.width;
		node->crop_info.source_height_crop = a->c.height;
		node->crop_info.capture_crop_enable = 1;
	}

	return 0;
}

static int vidioc_s_output(struct file *file, void *fh,
					unsigned int mode)
{
	struct amlvideo2_node *node = video_drvdata(file);

	if ((mode < AML_SCREEN_MODE_RATIO) || (mode > AML_SCREEN_MODE_MAX))
		return -1;
	node->mode = (enum aml_screen_mode_e)mode;
	return 0;
}

int amlvideo2_cma_buf_init(struct amlvideo2_device *vid_dev,  int node_id)
{
	int flags;

	if (!vid_dev->use_reserve) {
		if (vid_dev->cma_mode == 0) {
			vid_dev->cma_pages = dma_alloc_from_contiguous(
			&(vid_dev->pdev->dev),
			(CMA_ALLOC_SIZE*SZ_1M) >> PAGE_SHIFT, 0);
			if (vid_dev->cma_pages) {
				vid_dev->buffer_start = page_to_phys(
				vid_dev->cma_pages);
				vid_dev->buffer_size = (CMA_ALLOC_SIZE*SZ_1M);
			} else {
				pr_err("amlvideo2 alloc cma alone failed\n");
				return -1;
			}
		} else {
			flags = CODEC_MM_FLAGS_DMA_CPU|
				CODEC_MM_FLAGS_CMA_CLEAR;
			if (node_id == 0) {
				if (vid_dev->node[node_id]->
					ge2d_multi_process_flag == 1)
					vid_dev->buffer_start =
					codec_mm_alloc_for_dma(
					"amlvideo2.0",
					((CMA_ALLOC_SIZE +
					4) * SZ_1M)/PAGE_SIZE,
					0, flags);
				else
					vid_dev->buffer_start =
					codec_mm_alloc_for_dma(
					"amlvideo2.0",
					(CMA_ALLOC_SIZE * SZ_1M)/PAGE_SIZE,
					0, flags);
			} else {
				if (vid_dev->node[node_id]->
					ge2d_multi_process_flag == 1)
					vid_dev->buffer_start =
					codec_mm_alloc_for_dma(
					"amlvideo2.1",
					((CMA_ALLOC_SIZE +
					4) * SZ_1M)/PAGE_SIZE,
					0, flags);
				else
					vid_dev->buffer_start =
					codec_mm_alloc_for_dma(
					"amlvideo2.1",
					(CMA_ALLOC_SIZE * SZ_1M)/PAGE_SIZE,
					0, flags);
			}
			if (!(vid_dev->buffer_start)) {
				pr_err("amlvideo2 alloc cma buffer failed\n");
				return -1;
			}
			if (vid_dev->node[node_id]->ge2d_multi_process_flag
				== 1)
				vid_dev->buffer_size = ((CMA_ALLOC_SIZE
				+ 4)*SZ_1M);
			else
				vid_dev->buffer_size =
				(CMA_ALLOC_SIZE * SZ_1M);
		}
		if (node_id == 0)
			pr_info("amlvideo2.0 cma memory is %x , size is  %x\n",
				(unsigned int)vid_dev->buffer_start,
				(unsigned int)vid_dev->buffer_size);
		else
			pr_info("amlvideo2.1 cma memory is %x , size is  %x\n",
				(unsigned int)vid_dev->buffer_start,
				(unsigned int)vid_dev->buffer_size);
	}

	return 0;
}

int amlvideo2_cma_buf_uninit(struct amlvideo2_device *vid_dev, int node_id)
{
	if (!vid_dev->use_reserve) {
		if (vid_dev->cma_mode == 0) {
			if (vid_dev->cma_pages) {
				dma_release_from_contiguous(
					&vid_dev->pdev->dev,
					vid_dev->cma_pages,
					(CMA_ALLOC_SIZE*SZ_1M) >> PAGE_SHIFT);
				vid_dev->cma_pages = NULL;
			}
		} else {
			if (vid_dev->buffer_start != 0) {
				if (node_id == 0) {
					codec_mm_free_for_dma(
					"amlvideo2.0",
					vid_dev->buffer_start);
				} else {
					codec_mm_free_for_dma(
					"amlvideo2.1",
					vid_dev->buffer_start);
				}
				vid_dev->buffer_start = 0;
				vid_dev->buffer_size = 0;
				if (node_id == 0)
					pr_info("amlvideo2.0 cma memory release succeed\n");
				else
					pr_info("amlvideo2.1 cma memory release succeed\n");
			}
		}
	}
	return 0;
}

/* ------------------------------------------------------------------
 * File operations for the device
 * ------------------------------------------------------------------
 */
static int amlvideo2_open(struct file *file)
{
	struct amlvideo2_node *node = video_drvdata(file);
	struct amlvideo2_fh *fh = NULL;
	struct videobuf_res_privdata *res = NULL;
	struct resource *reserve = NULL;
	int ret;

	mutex_lock(&node->mutex);
	node->users++;
	if (node->users > 1) {
		node->users--;
		mutex_unlock(&node->mutex);
		return -EBUSY;
	}

	#if 0
	node->provider = vf_get_provider(
		(node->vid == 0)?RECEIVER_NAME0:RECEIVER_NAME1);

	if (node->provider == NULL) {
		node->users--;
		mutex_unlock(&node->mutex);
		return -ENODEV;
	}
	node->p_type = get_provider_type(node->provider->name, node->input);
	if ((node->p_type == AML_PROVIDE_NONE)
		|| (node->p_type >= AML_PROVIDE_MAX)) {
		node->users--;
		node->provider = NULL;
		mutex_unlock(&node->mutex);
		return -ENODEV;
	}
	#endif
	ret = amlvideo2_cma_buf_init(node->vid_dev, node->vid);
	if (ret < 0) {
		if (node->vid == 0)
			pr_err("alloc amlvideo2.0 cma buffer failed.\n");
		else
			pr_err("alloc amlvideo2.1 cma buffer failed.\n");
		node->users--;
		mutex_unlock(&node->mutex);
		return -ENOMEM;
	}

	fh = node->fh;
	if (fh == NULL) {
		node->users--;
		/* node->provider  = NULL; */
		amlvideo2_cma_buf_uninit(node->vid_dev, node->vid);
		mutex_unlock(&node->mutex);
		return -ENOMEM;
	}

	if (node->vid_dev->use_reserve) {
		reserve = &node->vid_dev->memobj;
		if (!reserve) {
			pr_err("alloc reserve buffer failed !\n");
			node->users--;
			amlvideo2_cma_buf_uninit(node->vid_dev, node->vid);
			mutex_unlock(&node->mutex);
			return -ENOMEM;
		}
		node->res.start = reserve->start;
		node->res.end = reserve->end;
	} else {
		node->res.start = node->vid_dev->buffer_start;
		node->res.end = node->vid_dev->buffer_start +
					node->vid_dev->buffer_size;
	}
	mutex_unlock(&node->mutex);

	node->mode = AML_SCREEN_MODE_RATIO;
	file->private_data = fh;
	fh->node = node;

	fh->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fh->fmt = &formats[0];
	fh->width = 1920;
	fh->height = 1080;


	fh->set_format_flag = false;
	fh->f_flags = file->f_flags;
	memset(&node->crop_info, 0, sizeof(struct crop_info_s));

	fh->node->res.priv = (void *)fh;
	res = &fh->node->res;
	videobuf_queue_res_init(&fh->vb_vidq, &amlvideo2_qops,
		NULL,
		&node->slock, fh->type, V4L2_FIELD_INTERLACED,
		sizeof(struct amlvideo2_node_buffer), (void *)res,
		NULL);

	v4l2_vdin_ops_init(&node->vops);
	fh->frm_save_time_us = 1000000 / DEF_FRAMERATE;
	return 0;
}

static ssize_t amlvideo2_read(struct file *file, char __user *data,
				size_t count, loff_t *ppos)
{
	struct amlvideo2_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
						file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int amlvideo2_poll(struct file *file,
					struct poll_table_struct *wait)
{
	struct amlvideo2_fh *fh = file->private_data;
	struct amlvideo2_node *node = fh->node;
	struct videobuf_queue *q = &fh->vb_vidq;

	dpr_err(node->vid_dev, 1, "%s\n", __func__);

	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int amlvideo2_close(struct file *file)
{
	struct amlvideo2_fh *fh = file->private_data;
	struct amlvideo2_node *node = fh->node;

	videobuf_stop(&fh->vb_vidq);
	videobuf_mmap_free(&fh->vb_vidq);
	amlvideo2_cma_buf_uninit(node->vid_dev, node->vid);
	mutex_lock(&node->mutex);
	if (node->r_type == AML_RECEIVER_NONE) {
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		/* switch_mod_gate_by_name("ge2d", 0); */
		/* #endif */
	}
	node->users--;
	amlvideo2_frmintervals_active.numerator = 1;
	amlvideo2_frmintervals_active.denominator = DEF_FRAMERATE;
	node->has_amvideo_node = false;
	node->crop_info.source_top_crop = 0;
	node->crop_info.source_left_crop = 0;
	node->crop_info.source_width_crop = 0;
	node->crop_info.source_height_crop = 0;
	node->crop_info.capture_crop_enable = 0;
	/* node->provider = NULL; */
	node->pflag = false;
	mutex_unlock(&node->mutex);
	return 0;
}

static int amlvideo2_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret = 0;
	struct amlvideo2_fh *fh = file->private_data;
	struct amlvideo2_node *node = fh->node;

	dpr_err(node->vid_dev, 1,
		"mmap called, vma=0x%08lx\n", (unsigned long)vma);

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dpr_err(node->vid_dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end - (unsigned long)vma->vm_start, ret);

	return ret;
}

static const struct v4l2_file_operations amlvideo2_fops = {
.owner = THIS_MODULE,
.open = amlvideo2_open,
.release = amlvideo2_close,
.read = amlvideo2_read,
.poll = amlvideo2_poll,
.unlocked_ioctl = video_ioctl2, /* V4L2 ioctl handler */
.mmap = amlvideo2_mmap, };

static const struct v4l2_ioctl_ops amlvideo2_ioctl_ops = {
.vidioc_querycap = vidioc_querycap,
.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
.vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap,
.vidioc_try_fmt_vid_cap = vidioc_try_fmt_vid_cap,
.vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
.vidioc_g_parm = vidioc_g_parm,
.vidioc_s_parm = vidioc_s_parm,
.vidioc_reqbufs = vidioc_reqbufs,
.vidioc_querybuf = vidioc_querybuf,
.vidioc_qbuf = vidioc_qbuf,
.vidioc_dqbuf = vidioc_dqbuf,
.vidioc_s_std = vidioc_s_std,
.vidioc_queryctrl = vidioc_queryctrl,
.vidioc_g_ctrl = vidioc_g_ctrl,
.vidioc_s_ctrl = vidioc_s_ctrl,
.vidioc_streamon = vidioc_streamon,
.vidioc_streamoff = vidioc_streamoff,
.vidioc_enum_framesizes = vidioc_enum_framesizes,
.vidioc_enum_frameintervals = vidioc_enum_frameintervals,
.vidioc_s_input = vidioc_s_input,
.vidioc_g_input = vidioc_g_input,
.vidioc_g_crop = vidioc_g_crop,
.vidioc_s_crop = vidioc_s_crop,
.vidioc_s_output = vidioc_s_output,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
.vidiocgmbuf = vidiocgmbuf,
#endif
};

static struct video_device amlvideo2_template = {
.name = "amlvideo2",
.fops =  &amlvideo2_fops,
.ioctl_ops = &amlvideo2_ioctl_ops,
.release = video_device_release,
.tvnorms = V4L2_STD_525_60,
/* .current_norm = V4L2_STD_NTSC_M, */
};

static int amlvideo2_vf_get_states(struct vframe_states *states, int node_index)
{
	int ret = -1;
	const char *name = (node_index == 0) ? DEVICE_NAME0 : DEVICE_NAME1;
	ret = vf_get_states_by_name(name, states);
	return ret;
}

static int amlvideo2_receiver_event_fun(int type, void *data,
					void *private_data)
{
	struct amlvideo2_node *node = (struct amlvideo2_node *)private_data;
	/* (struct amlvideo2_fh *)container_of(
	 * node, struct amlvideo2_fh, node);
	 */
	struct amlvideo2_fh *fh = node->fh;
	struct vframe_states states;
	struct vframe_states frame_states;
	const char *name = (node->vid == 0) ? DEVICE_NAME0 : DEVICE_NAME1;

	switch (type) {
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		node->provide_ready = 1;
		if (amlvideo2_dbg_en & 8)
			pr_info("provider : node->recv.name = %s\n",
				node->recv.name);
		if (vf_peek(node->recv.name) != NULL)
			wake_up_interruptible(&node->vidq.wq);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		amlvideo2_vf_states(&states, node);
		if (states.buf_avail_num > 0)
			return RECEIVER_ACTIVE;
		if (vf_notify_receiver(
			name,
			VFRAME_EVENT_PROVIDER_QUREY_STATE,
			NULL)
			== RECEIVER_ACTIVE) {
			return RECEIVER_ACTIVE;
		}
		return RECEIVER_INACTIVE;
	case VFRAME_EVENT_PROVIDER_START:
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		/* switch_mod_gate_by_name("ge2d", 1); */
		/* #endif */
		node->sub_recv = vf_get_receiver(name);
		if (amlvideo2_dbg_en) {
			pr_info("provider start : name = %s\n", name);
			pr_info("provider start : sub_recv = %p\n",
				node->sub_recv);
		}
		if (node->sub_recv) {
			node->r_type =
				get_sub_receiver_type(node->sub_recv->name);
			if (amlvideo2_dbg_en) {
				pr_info("%s , node->sub_recv->name=%s\n",
					__func__, node->sub_recv->name);
				pr_info("r_type=%d\n", node->r_type);
			}
			node->provider = vf_get_provider(name);
			if (node->provider) {
				node->p_type =
					get_provider_type(node->provider->name);
				if (amlvideo2_dbg_en) {
					pr_info("provider=%s\n",
						node->provider->name);
					pr_info("node->p_type = %d .\n",
						node->p_type);
				}
			}

			fh->node = node;

			vf_reg_provider(&node->amlvideo2_vf_prov);
			vf_notify_receiver(name,
			VFRAME_EVENT_PROVIDER_START,
						NULL);
			amlvideo2_start_thread(fh);
		}

		break;
	case VFRAME_EVENT_PROVIDER_REG:
		node->amlvideo2_pool_ready = NULL;
		node->amlvideo2_pool_size = 0;
		node->video_blocking = false;
		if (amlvideo2_vf_get_states(&frame_states, node->vid) == 0)
			node->amlvideo2_pool_size = frame_states.vf_pool_size;
		else
			node->amlvideo2_pool_size = 4;

		node->amlvideo2_pool_ready =
			kmalloc((sizeof(struct vframe_s *) *
			(node->amlvideo2_pool_size + 1)),
			GFP_KERNEL);
		if (!node->amlvideo2_pool_ready)
			pr_err("amlvideo2_pool_ready malloc failed\n");
		if (node->amlvideo2_pool_ready != NULL) {
			vfq_init(&node->q_ready,
			node->amlvideo2_pool_size + 1,
			(struct vframe_s **)&(node->amlvideo2_pool_ready[0]));
		}
		break;
	case VFRAME_EVENT_PROVIDER_UNREG:
		node->provide_ready = 0;
		if (node->r_type != AML_RECEIVER_NONE) {
			amlvideo2_stop_thread(&node->vidq);
			pr_err("%s,%dstop thread\n", __func__, __LINE__);
		}
		if (amlvideo2_dbg_en)
			pr_info("unreg amlvideo2 , pflag = %d\n", node->pflag);

		if (!node->pflag) {
			vf_unreg_provider(&node->amlvideo2_vf_prov);
			kfree(node->amlvideo2_pool_ready);
		}

		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		/* switch_mod_gate_by_name("ge2d", 0); */
		/* #endif */
		break;
	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG:
		if (amlvideo2_dbg_en)
			pr_info("provider light unreg\n");
		break;
	case VFRAME_EVENT_PROVIDER_RESET:
		if (amlvideo2_dbg_en)
			pr_info("provider reset\n");
		vf_notify_receiver(name,
		VFRAME_EVENT_PROVIDER_RESET,
					NULL);
		if (node->vidq.task_running) {
			node->video_blocking = true;
			wake_up_interruptible(&node->vidq.wq);
		}
		break;
	case VFRAME_EVENT_PROVIDER_FR_HINT:
	case VFRAME_EVENT_PROVIDER_FR_END_HINT:
		vf_notify_receiver(name, type, data);
		break;
	default:
		break;
	}
	return 0;
}

static const struct vframe_receiver_op_s video_vf_receiver = {
.event_cb = amlvideo2_receiver_event_fun};
/* -----------------------------------------------------------------
 * Initialization and module stuff
 * -----------------------------------------------------------------
 */

static int amlvideo2_release_node(struct amlvideo2_device *vid_dev)
{
	int i = 0;
	struct video_device *vfd = NULL;

	for (i = 0; i < vid_dev->node_num; i++) {
		if (vid_dev->node[i]) {
			vfd = vid_dev->node[i]->vfd;
			video_device_release(vfd);
			vf_unreg_receiver(&vid_dev->node[i]->recv);
			if (vid_dev->node[i]->context)
				destroy_ge2d_work_queue(
					vid_dev->node[i]->context);
			kfree(vid_dev->node[i]->fh);
			vid_dev->node[i]->fh = NULL;
			kfree(vid_dev->node[i]);
			vid_dev->node[i] = NULL;
		}
		gAmlvideo2_Node[i] = NULL;
	}

	return 0;
}

/* static struct resource memobj; */
static int amlvideo2_create_node(struct platform_device *pdev, int node_id)
{
	int ret = 0, j = 0;
	struct video_device *vfd = NULL;
	struct amlvideo2_node *vid_node = NULL;
	struct amlvideo2_fh *fh = NULL;
	struct resource *res = NULL;
	struct v4l2_device *v4l2_dev = platform_get_drvdata(pdev);
	struct amlvideo2_device *vid_dev = container_of(v4l2_dev,
		struct amlvideo2_device,
		v4l2_dev);
	pr_info("amlvideo2_create_node");
	vid_dev->node_num = pdev->num_resources;
	if (vid_dev->node_num > MAX_SUB_DEV_NODE)
		vid_dev->node_num = MAX_SUB_DEV_NODE;

	vid_dev->node[node_id] = NULL;
	ret = -ENOMEM;
#if 0
	res = platform_get_resource(pdev, IORESOURCE_MEM, i);
#else

	res = &vid_dev->memobj;
/*
 * ret = find_reserve_block(pdev->dev.of_node->name,i);
 * if(ret < 0){
 *   pr_err("\namlvideo2 memory resource undefined.\n");
 *   return -EFAULT;
 * }
 * res->start = (phys_addr_t)get_reserve_block_addr(ret);
 * res->end = res->start +
 * (phys_addr_t)get_reserve_block_size(ret)-1;
 */
#endif
	if (!res)
		return ret;

	vid_node = kzalloc(sizeof(struct amlvideo2_node), GFP_KERNEL);
	if (!vid_node)
		return ret;

	vid_node->res.magic = MAGIC_RE_MEM;
	vid_node->res.priv = NULL;
	vid_node->context = create_ge2d_work_queue();
	if (!vid_node->context) {
		kfree(vid_node);
		return ret;
	}
	/* init video dma queues */
	INIT_LIST_HEAD(&vid_node->vidq.active);
	init_waitqueue_head(&vid_node->vidq.wq);

	/* initialize locks */
	spin_lock_init(&vid_node->slock);
	mutex_init(&vid_node->mutex);
	init_completion(&vid_node->plug_sema);

	vfd = video_device_alloc();
	if (!vfd) {
		destroy_ge2d_work_queue(vid_node->context);
		kfree(vid_node);
		return ret;
	}
	*vfd = amlvideo2_template;
	vfd->dev_debug = debug;
	vfd->v4l2_dev = v4l2_dev;
	ret = video_register_device(vfd, VFL_TYPE_GRABBER, video_nr);
	if (ret < 0) {
		ret = -ENODEV;
		video_device_release(vfd);
		destroy_ge2d_work_queue(vid_node->context);
		kfree(vid_node);
		return ret;
	}

	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (!fh)
		return ret;

	vid_node->fh = fh;
	video_set_drvdata(vfd, vid_node);

	/* Set all controls to their default value. */
	for (j = 0; j < ARRAY_SIZE(amlvideo2_node_qctrl); j++) {
		vid_node->qctl_regs[j] =
		amlvideo2_node_qctrl[j].default_value;
	}

	vid_node->vfd = vfd;
	vid_node->vid = node_id;
	vid_node->users = 0;
	vid_node->vid_dev = vid_dev;
	video_nr++;
#ifdef MULTI_NODE
	vf_receiver_init(&vid_node->recv,
		(node_id == 0) ? DEVICE_NAME0 : DEVICE_NAME1,
		&video_vf_receiver, (void *)vid_node);
#else
	vf_receiver_init(&vid_node->recv,
		RECEIVER_NAME,
		&video_vf_receiver, (void *)vid_node);
#endif
	vf_reg_receiver(&vid_node->recv);
	vf_provider_init(&vid_node->amlvideo2_vf_prov,
	(node_id == 0) ? DEVICE_NAME0 : DEVICE_NAME1,
		&amlvideo2_vf_provider,
		(void *)vid_node);
	vid_node->pflag = false;
	vid_node->field_flag = false;
	vid_node->field_condition_flag = false;
	vid_node->ge2d_multi_process_flag = false;
	vid_node->r_type = AML_RECEIVER_NONE;
	vid_dev->node[node_id] = vid_node;
	v4l2_info(&vid_dev->v4l2_dev, "V4L2 device registered as %s\n",
		video_device_node_name(vfd));
	gAmlvideo2_Node[node_id] = vid_node;
	ret = 0;

	if (ret)
		amlvideo2_release_node(vid_dev);

	return ret;
}

static int amlvideo2_driver_probe(struct platform_device *pdev)
{
	s32 ret;
	struct amlvideo2_device *dev = NULL;

	dev = kzalloc(sizeof(struct amlvideo2_device), GFP_KERNEL);

	if (dev == NULL)
		return -ENOMEM;

	memset(dev, 0, sizeof(struct amlvideo2_device));
	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name), "%s",
			AVMLVIDEO2_MODULE_NAME);

	pr_err("amlvideo2 probe called\n");

	pdev->num_resources = MAX_SUB_DEV_NODE;
	platform_set_drvdata(pdev, &(dev->v4l2_dev));
	ret = of_reserved_mem_device_init(&pdev->dev);

	if (ret == 0)
		pr_info("amlvideo2_probe done\n");

	ret = of_property_read_u32(pdev->dev.of_node,
		"cma_mode", &(dev->cma_mode));
	if (ret) {
		pr_err("don't find  match cma_mode\n");
		dev->cma_mode = 1;
	}
	ret = of_property_read_u32(pdev->dev.of_node,
				   "amlvideo2_id", &(dev->node_id));
	if (ret)
		pr_err("don't find amlvideo2 node id.\n");
	dev->pdev = pdev;

	if (v4l2_device_register(&pdev->dev, &dev->v4l2_dev) < 0) {
		dev_err(&pdev->dev, "v4l2_device_register failed\n");
		ret = -ENODEV;
		goto probe_err0;
	}

	mutex_init(&dev->mutex);
	video_nr = 11;

	ret = amlvideo2_create_node(pdev, dev->node_id);
	if (ret)
		goto probe_err1;

	/* register vout client */
	if (dev->node_id == 1)
		vout_register_client(&amlvideo2_notifier_nb);
	return 0;

probe_err1: v4l2_device_unregister(&dev->v4l2_dev);

probe_err0: kfree(dev);
	return ret;

	/* int ret = 0; */
	/* struct amlvideo2_device *dev = NULL; */
	/*  */
	/* if(of_get_property(pdev->dev.of_node, "reserve-memory", NULL)) */
	/* pdev->num_resources = MAX_SUB_DEV_NODE; */
	/*  */
	/* if (pdev->num_resources == 0) */
	/* { */
	/* dev_err(&pdev->dev, "probed for an unknown device\n"); */
	/* return -ENODEV; */
	/* } */
	/*  */
	/* dev = kzalloc(sizeof(struct amlvideo2_device), GFP_KERNEL); */
	/*  */
	/* if (dev == NULL) */
	/* return -ENOMEM; */
	/*  */
	/* memset(dev,0,sizeof(struct amlvideo2_device)); */
	/*  */
	/* snprintf(dev->v4l2_dev.name,
	 * sizeof(dev->v4l2_dev.name),
	 * "%s", AVMLVIDEO2_MODULE_NAME);
	 */
	/*  */
	/* if (v4l2_device_register(&pdev->dev, &dev->v4l2_dev) < 0) { */
	/* dev_err(&pdev->dev, "v4l2_device_register failed\n"); */
	/* ret = -ENODEV; */
	/* goto probe_err0; */
	/* } */
	/*  */
	/* mutex_init(&dev->mutex); */
	/* video_nr = 11; */
	/*  */
	/* ret = amlvideo2_create_node(pdev); */
	/* if (ret) */
	/* goto probe_err1; */
	/*  */
	/* return 0; */
	/*  */
	/* probe_err1: */
	/* v4l2_device_unregister(&dev->v4l2_dev); */
	/*  */
	/* probe_err0: */
	/* kfree(dev); */
	/* return ret; */
}

static int amlvideo2_mem_device_init(struct reserved_mem *rmem,
					struct device *dev)
{
	struct resource *res = NULL;
	struct platform_device *pdev = container_of(dev,
			struct platform_device, dev);
	struct v4l2_device *v4l2_dev = platform_get_drvdata(pdev);
	struct amlvideo2_device *vdevp = container_of(v4l2_dev,
			struct amlvideo2_device, v4l2_dev);
	res = &(vdevp->memobj);
	res->start = rmem->base;
	res->end = rmem->base + rmem->size - 1;
	vdevp->use_reserve = 1;
	pr_info("amlvideo2 mem:%lx->%lx\n", (unsigned long)res->start,
		(unsigned long)res->end);
	return 0;
}

static const struct reserved_mem_ops rmem_amlvideo2_ops = {
.device_init = amlvideo2_mem_device_init, };

static int __init amlvideo2_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_amlvideo2_ops;
	pr_info("amlvideo2 share mem setup\n");

	return 0;
}

static int amlvideo2_drv_remove(struct platform_device *pdev)
{
	struct v4l2_device *v4l2_dev = platform_get_drvdata(pdev);
	struct amlvideo2_device *vid_dev = container_of(v4l2_dev,
							struct amlvideo2_device,
							v4l2_dev);
	/* unregister vout client */
	video_nr = 11;
	if (vid_dev->node_id == 1)
		vout_unregister_client(&amlvideo2_notifier_nb);
	amlvideo2_release_node(vid_dev);
	v4l2_device_unregister(v4l2_dev);
	platform_set_drvdata(pdev, NULL);
	kfree(vid_dev);
	return 0;
}

static const struct of_device_id amlvideo2_dt_match[] = {
{
.compatible = "amlogic, amlvideo2",
},
{},
};


/* general interface for a linux driver .*/
static struct platform_driver amlvideo2_drv = {
.probe = amlvideo2_driver_probe,
.remove = amlvideo2_drv_remove,
.driver = {.name = "amlvideo2", .owner = THIS_MODULE, .of_match_table =
amlvideo2_dt_match, } };

static int __init amlvideo2_init(void)
{
	/* amlog_level(LOG_LEVEL_HIGH,"amlvideo2_init\n"); */
	if (platform_driver_register(&amlvideo2_drv)) {
		pr_err(
			"Failed to register amlvideo2 driver\n");
			return -ENODEV;
	}

	return 0;
}

static void __exit amlvideo2_exit(void)
{
	platform_driver_unregister(&amlvideo2_drv);
	/* amlog_level(LOG_LEVEL_HIGH,"amlvideo2 module removed.\n"); */
}

RESERVEDMEM_OF_DECLARE(amlvideo2, "amlogic, amlvideo2_memory",
						 amlvideo2_mem_setup);
module_init(amlvideo2_init);
module_exit(amlvideo2_exit);
