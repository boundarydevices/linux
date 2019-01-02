/*
 * drivers/amlogic/media/camera/gc2145.c
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

#include <linux/sizes.h>
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
#include <linux/amlogic/media/v4l_util/videobuf-res.h>
#include <media/videobuf-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/amlogic/media/camera/aml_cam_info.h>
#include <linux/amlogic/media/camera/vmapi.h>
#include "common/plat_ctrl.h"
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/old_cpu_version.h>
#include "common/vm.h"

#define GC2145_CAMERA_MODULE_NAME "gc2145"

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001
#define BUFFER_TIMEOUT     msecs_to_jiffies(500)  /* 0.5 seconds */

#define MAGIC_RE_MEM 0x123039dc
#define CAMERA_USER_CANVAS_INDEX	0x4e

#define GC2145_CAMERA_MAJOR_VERSION 0
#define GC2145_CAMERA_MINOR_VERSION 7
#define GC2145_CAMERA_RELEASE 0
#define GC2145_CAMERA_VERSION	\
		KERNEL_VERSION(GC2145_CAMERA_MAJOR_VERSION,	\
		GC2145_CAMERA_MINOR_VERSION,	\
		GC2145_CAMERA_RELEASE)

#define GC2145_DRIVER_VERSION "GC2145-COMMON-01-140722"

/*unsigned short DGain_shutter,AGain_shutter,*/
/*DGain_shutterH,DGain_shutterL,AGain_shutterH,*/
/*AGain_shutterL,shutterH,shutterL,shutter;*/
/*unsigned short UXGA_Cap = 0;*/

static struct i2c_client *g_i2c_client;
static u32 cur_reg;
static u8 cur_val;
static u8 is_first_time_open;

enum DCAMERA_FLICKER {

	DCAMERA_FLICKER_50HZ = 0,

	DCAMERA_FLICKER_60HZ,

	FLICKER_MAX

};

/*static unsigned short Antiflicker = DCAMERA_FLICKER_50HZ;*/

#define GC2145_NORMAL_Y0ffset 0x08
#define GC2145_LOWLIGHT_Y0ffset  0x20

MODULE_DESCRIPTION("gc2145 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");

static unsigned int video_nr = -1;  /* videoX start number, -1 is autodetect. */

static unsigned int debug;
/* module_param(debug, uint, 0644); */
/* MODULE_PARM_DESC(debug, "activates debug info"); */

static unsigned int vid_limit = 16;
/* module_param(vid_limit, uint, 0644); */
/* MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes"); */

//static int vidio_set_fmt_ticks;

static int GC2145_h_active = 640; /* 800; */
static int GC2145_v_active = 480; /* 600; */

static int gc2145_have_open;

static struct v4l2_fract gc2145_frmintervals_active = {
	.numerator = 1,
	.denominator = 15,
};

static int gc2145_night_or_normal;  /* add by sp_yjp,20120905 */
static struct vdin_v4l2_ops_s *vops;
/* supported controls */
static struct v4l2_queryctrl gc2145_qctrl[] = {
	{
		.id            = V4L2_CID_DO_WHITE_BALANCE,
		.type          = V4L2_CTRL_TYPE_MENU,
		.name          = "white balance",
		.minimum       = CAM_WB_AUTO,
		.maximum       = CAM_WB_FLUORESCENT,
		.step          = 0x1,
		.default_value = CAM_WB_AUTO,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_EXPOSURE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "exposure",
		.minimum       = 0,
		.maximum       = 8,
		.step          = 0x1,
		.default_value = 4,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_COLORFX,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "effect",
		.minimum       = 0,
		.maximum       = 6,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_POWER_LINE_FREQUENCY,
		.type          = V4L2_CTRL_TYPE_MENU,
		.name          = "banding",
		.minimum       = CAM_BANDING_50HZ,
		.maximum       = CAM_BANDING_60HZ,
		.step          = 0x1,
		.default_value = CAM_BANDING_50HZ,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_BLUE_BALANCE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "scene mode",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_HFLIP,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "flip on horizontal",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_VFLIP,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "flip on vertical",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_ZOOM_ABSOLUTE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Zoom, Absolute",
		.minimum       = 100,
		.maximum       = 300,
		.step          = 20,
		.default_value = 100,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id     = V4L2_CID_ROTATE,
		.type       = V4L2_CTRL_TYPE_INTEGER,
		.name       = "Rotate",
		.minimum    = 0,
		.maximum    = 270,
		.step       = 90,
		.default_value  = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}
};
struct v4l2_querymenu gc2145_qmenu_wbmode[] = {
	{
		.id         = V4L2_CID_DO_WHITE_BALANCE,
		.index      = CAM_WB_AUTO,
		.name       = "auto",
		.reserved   = 0,
	}, {
		.id         = V4L2_CID_DO_WHITE_BALANCE,
		.index      = CAM_WB_CLOUD,
		.name       = "cloudy-daylight",
		.reserved   = 0,
	}, {
		.id         = V4L2_CID_DO_WHITE_BALANCE,
		.index      = CAM_WB_INCANDESCENCE,
		.name       = "incandescent",
		.reserved   = 0,
	}, {
		.id         = V4L2_CID_DO_WHITE_BALANCE,
		.index      = CAM_WB_DAYLIGHT,
		.name       = "daylight",
		.reserved   = 0,
	}, {
		.id         = V4L2_CID_DO_WHITE_BALANCE,
		.index      = CAM_WB_FLUORESCENT,
		.name       = "fluorescent",
		.reserved   = 0,
	}, {
		.id         = V4L2_CID_DO_WHITE_BALANCE,
		.index      = CAM_WB_WARM_FLUORESCENT,
		.name       = "warm-fluorescent",
		.reserved   = 0,
	},
};

struct v4l2_querymenu gc2145_qmenu_anti_banding_mode[] = {
	{
		.id         = V4L2_CID_POWER_LINE_FREQUENCY,
		.index      = CAM_BANDING_50HZ,
		.name       = "50hz",
		.reserved   = 0,
	}, {
		.id         = V4L2_CID_POWER_LINE_FREQUENCY,
		.index      = CAM_BANDING_60HZ,
		.name       = "60hz",
		.reserved   = 0,
	},
};
static struct v4l2_frmivalenum gc2145_frmivalenum[] = {
	{
		.index		= 0,
		.pixel_format	= V4L2_PIX_FMT_NV21,
		.width		= 352,
		.height		= 288,
		.type		= V4L2_FRMIVAL_TYPE_DISCRETE,
		{
			.discrete = {
				.numerator = 1,
				.denominator = 30,/* 15 */
			}
		}
	}, {
		.index      = 0,
		.pixel_format   = V4L2_PIX_FMT_NV21,
		.width      = 640,
		.height     = 480,
		.type       = V4L2_FRMIVAL_TYPE_DISCRETE,
		{
			.discrete   = {
				.numerator  = 1,
				.denominator    = 30,/* 15 */
			}
		}
	}, {
		.index      = 1,
		.pixel_format   = V4L2_PIX_FMT_NV21,
		.width      = 1600,
		.height     = 1200,
		.type       = V4L2_FRMIVAL_TYPE_DISCRETE,
		{
			.discrete   = {
				.numerator  = 1,
				.denominator    = 5,
			}
		}
	},
};


struct gc2145_qmenu_set_s {
	__u32   id;
	int     num;
	struct v4l2_querymenu *gc2145_qmenu;
};

struct gc2145_qmenu_set_s gc2145_qmenu_set[] = {
	{
		.id             = V4L2_CID_DO_WHITE_BALANCE,
		.num            = ARRAY_SIZE(gc2145_qmenu_wbmode),
		.gc2145_qmenu   = gc2145_qmenu_wbmode,
	}, {
		.id             = V4L2_CID_POWER_LINE_FREQUENCY,
		.num            = ARRAY_SIZE(gc2145_qmenu_anti_banding_mode),
		.gc2145_qmenu   = gc2145_qmenu_anti_banding_mode,
	},
};

static int vidioc_querymenu(struct file *file, void *priv,
			    struct v4l2_querymenu *a)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(gc2145_qmenu_set); i++) {
		if (a->id && a->id == gc2145_qmenu_set[i].id) {
			for (j = 0; j < gc2145_qmenu_set[i].num; j++) {
				if (a->index ==
				gc2145_qmenu_set[i].gc2145_qmenu[j].index) {
					memcpy(a,
					&(gc2145_qmenu_set[i].gc2145_qmenu[j]),
					sizeof(*a));
					return 0;
				}
			}
		}
	}
	return -EINVAL;
}

#define dprintk(dev, level, fmt, arg...)	\
		v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)

/*
 * ------------------------------------------------------------------
 *    Basic structures
 * ------------------------------------------------------------------
 */

struct gc2145_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct gc2145_fmt formats[] = {
	{
		.name     = "RGB565 (BE)",
		.fourcc   = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
		.depth    = 16,
	}, {
		.name     = "RGB888 (24)",
		.fourcc   = V4L2_PIX_FMT_RGB24, /* 24  RGB-8-8-8 */
		.depth    = 24,
	}, {
		.name     = "BGR888 (24)",
		.fourcc   = V4L2_PIX_FMT_BGR24, /* 24  BGR-8-8-8 */
		.depth    = 24,
	}, {
		.name     = "12  Y/CbCr 4:2:0",
		.fourcc   = V4L2_PIX_FMT_NV12,
		.depth    = 12,
	}, {
		.name     = "12  Y/CbCr 4:2:0",
		.fourcc   = V4L2_PIX_FMT_NV21,
		.depth    = 12,
	}, {
		.name     = "YUV420P",
		.fourcc   = V4L2_PIX_FMT_YUV420,
		.depth    = 12,
	}, {
		.name     = "YVU420P",
		.fourcc   = V4L2_PIX_FMT_YVU420,
		.depth    = 12,
	}
};

static struct gc2145_fmt *get_format(struct v4l2_format *f)
{
	struct gc2145_fmt *fmt;
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

struct sg_to_addr {
	int pos;
	struct scatterlist *sg;
};

/* buffer for one video frame */
struct gc2145_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct gc2145_fmt        *fmt;

	unsigned int canvas_id;
};

struct gc2145_dmaqueue {
	struct list_head       active;

	/* thread for generating video stream*/
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
	/* Counters to control fps rate */
	int                        frame;
	int                        ini_jiffies;
};

static LIST_HEAD(gc2145_devicelist);

struct gc2145_device {
	struct list_head            gc2145_devicelist;
	struct v4l2_subdev          sd;
	struct v4l2_device          v4l2_dev;

	spinlock_t                 slock;
	struct mutex                mutex;

	int                        users;

	/* various device info */
	struct video_device        *vdev;

	struct gc2145_dmaqueue       vidq;

	/* Several counters */
	unsigned long              jiffies;

	/* Input Number */
	int            input;

	/* platform device data from board initting. */
	struct aml_cam_info_s  cam_info;

#ifdef CONFIG_HAS_WAKELOCK
	/* wake lock */
	struct wake_lock    wake_lock;
#endif

	/* Control 'registers' */
	int                qctl_regs[ARRAY_SIZE(gc2145_qctrl)];
	struct vm_init_s vminfo;
};

static inline struct gc2145_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc2145_device, sd);
}

struct gc2145_fh {
	struct gc2145_device            *dev;

	/* video capture */
	struct gc2145_fmt            *fmt;
	unsigned int               width, height;
	struct videobuf_queue      vb_vidq;

	struct videobuf_res_privdata res;
	enum v4l2_buf_type         type;
	int            input;   /* Input Number on bars */
	int  stream_on;
	unsigned int        f_flags;
};

/*
 *static inline struct gc2145_fh *to_fh(struct gc2145_device *dev)
 *{
 *    return container_of(dev, struct gc2145_fh, dev);
 *}
 */

static struct v4l2_frmsize_discrete
	gc2145_prev_resolution[] = {
	/* should include 352x288 and 640x480,*/
	/*those two size are used for recording*/
	{352, 288},
	{640, 480},
};

static struct v4l2_frmsize_discrete gc2145_pic_resolution[] = {
	{1600, 1200},
	{800, 600}
};

#ifndef GC2145_MIRROR
#define GC2145_MIRROR   0
#endif
#ifndef GC2145_FLIP
#define GC2145_FLIP   0
#endif

/*
 *------------------------------------------------------------------
 *    reg spec of GC2145
 *   ------------------------------------------------------------------
 */

struct aml_camera_i2c_fig_s GC2145_script[] = {
	/*SENSORDB("GC2145_Sensor_Init"}*/
	{0xfe, 0xf0},
	{0xfe, 0xf0},
	{0xfe, 0xf0},
	{0xfc, 0x06},
	{0xf6, 0x00},
	{0xf7, 0x1d},
	{0xf8, 0x84},
	{0xfa, 0x00},
	{0xf9, 0xfe},
	{0xf2, 0x00},
	/*ISP reg*/
	{0xfe, 0x00},
	{0x03, 0x04},
	{0x04, 0xe2},
	{0x09, 0x00},
	{0x0a, 0x00},
	{0x0b, 0x00},
	{0x0c, 0x00},
	{0x0d, 0x04},
	{0x0e, 0xc0},
	{0x0f, 0x06},
	{0x10, 0x52},
	{0x12, 0x2e},
	{0x17, 0x14}, /*mirror*/
	{0x18, 0x22},
	{0x19, 0x0e},
	{0x1a, 0x01},
	{0x1b, 0x4b},
	{0x1c, 0x07},
	{0x1d, 0x10},
	{0x1e, 0x88},
	{0x1f, 0x78},
	{0x20, 0x03},
	{0x21, 0x40},
	{0x22, 0xa0},
	{0x24, 0x16},
	{0x25, 0x01},
	{0x26, 0x10},
	{0x2d, 0x60},
	{0x30, 0x01},
	{0x31, 0x90},
	{0x33, 0x06},
	{0x34, 0x01},
	/*ISP reg*/
	{0xfe, 0x00},
	{0x80, 0x7f},
	{0x81, 0x26},
	{0x82, 0xfa},
	{0x83, 0x00},
	{0x84, 0x02},
	{0x86, 0x01},/*0x03*/
	{0x88, 0x03},
	{0x89, 0x03},
	{0x85, 0x08},
	{0x8a, 0x00},
	{0x8b, 0x00},
	{0xb0, 0x55},
	{0xc3, 0x00},
	{0xc4, 0x80},
	{0xc5, 0x90},
	{0xc6, 0x3b},
	{0xc7, 0x46},
	{0xec, 0x06},
	{0xed, 0x04},
	{0xee, 0x60},
	{0xef, 0x90},
	{0xb6, 0x01},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x04},
	{0x96, 0xb0},
	{0x97, 0x06},
	{0x98, 0x40},
	/*BLK*/
	{0xfe, 0x00},
	{0x40, 0x42},
	{0x41, 0x00},
	{0x43, 0x5b},
	{0x5e, 0x00},
	{0x5f, 0x00},
	{0x60, 0x00},
	{0x61, 0x00},
	{0x62, 0x00},
	{0x63, 0x00},
	{0x64, 0x00},
	{0x65, 0x00},
	{0x66, 0x20},
	{0x67, 0x20},
	{0x68, 0x20},
	{0x69, 0x20},
	{0x76, 0x00},
	{0x6a, 0x08},
	{0x6b, 0x08},
	{0x6c, 0x08},
	{0x6d, 0x08},
	{0x6e, 0x08},
	{0x6f, 0x08},
	{0x70, 0x08},
	{0x71, 0x08},
	{0x76, 0x00},
	{0x72, 0xf0},
	{0x7e, 0x3c},
	{0x7f, 0x00},
	{0xfe, 0x02},
	{0x48, 0x15},
	{0x49, 0x00},
	{0x4b, 0x0b},
	{0xfe, 0x00},
	/*AEC*/
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0xc0},
	{0x03, 0x04},
	{0x04, 0x90},
	{0x05, 0x30},
	{0x06, 0x90},
	{0x07, 0x30},
	{0x08, 0x80},
	{0x09, 0x00},
	{0x0a, 0x82},
	{0x0b, 0x11},
	{0x0c, 0x10},
	{0x11, 0x10},
	{0x13, 0x7b},
	{0x17, 0x00},
	{0x1c, 0x11},
	{0x1e, 0x61},
	{0x1f, 0x35},
	{0x20, 0x40},
	{0x22, 0x40},
	{0x23, 0x20},
	{0xfe, 0x02},
	{0x0f, 0x04},
	{0xfe, 0x01},
	{0x12, 0x35},
	{0x15, 0xb0},
	{0x10, 0x31},
	{0x3e, 0x28},
	{0x3f, 0xb0},
	{0x40, 0x90},
	{0x41, 0x0f},
	/*INTPEE*/
	{0xfe, 0x02},
	{0x90, 0x6c},
	{0x91, 0x03},
	{0x92, 0xcb},
	{0x94, 0x33},
	{0x95, 0x84},
	{0x97, 0x65},
	{0xa2, 0x11},
	{0xfe, 0x00},
	/*DNDD*/
	{0xfe, 0x02},
	{0x80, 0xc1},
	{0x81, 0x08},
	{0x82, 0x05},
	{0x83, 0x08},
	{0x84, 0x0a},
	{0x86, 0xf0},
	{0x87, 0x50},
	{0x88, 0x15},
	{0x89, 0xb0},
	{0x8a, 0x30},
	{0x8b, 0x10},
	/*ASDE*/
	{0xfe, 0x01},
	{0x21, 0x04},
	{0xfe, 0x02},
	{0xa3, 0x50},
	{0xa4, 0x20},
	{0xa5, 0x40},
	{0xa6, 0x80},
	{0xab, 0x40},
	{0xae, 0x0c},
	{0xb3, 0x46},
	{0xb4, 0x64},
	{0xb6, 0x38},
	{0xb7, 0x01},
	{0xb9, 0x2b},
	{0x3c, 0x04},
	{0x3d, 0x15},
	{0x4b, 0x06},
	{0x4c, 0x20},
	{0xfe, 0x00},

	/*gamma1*/
#if 1
	{0xfe, 0x02},
	{0x10, 0x09},
	{0x11, 0x0d},
	{0x12, 0x13},
	{0x13, 0x19},
	{0x14, 0x27},
	{0x15, 0x37},
	{0x16, 0x45},
	{0x17, 0x53},
	{0x18, 0x69},
	{0x19, 0x7d},
	{0x1a, 0x8f},
	{0x1b, 0x9d},
	{0x1c, 0xa9},
	{0x1d, 0xbd},
	{0x1e, 0xcd},
	{0x1f, 0xd9},
	{0x20, 0xe3},
	{0x21, 0xea},
	{0x22, 0xef},
	{0x23, 0xf5},
	{0x24, 0xf9},
	{0x25, 0xff},
#else
	{0xfe, 0x02},
	{0x10, 0x0a},
	{0x11, 0x12},
	{0x12, 0x19},
	{0x13, 0x1f},
	{0x14, 0x2c},
	{0x15, 0x38},
	{0x16, 0x42},
	{0x17, 0x4e},
	{0x18, 0x63},
	{0x19, 0x76},
	{0x1a, 0x87},
	{0x1b, 0x96},
	{0x1c, 0xa2},
	{0x1d, 0xb8},
	{0x1e, 0xcb},
	{0x1f, 0xd8},
	{0x20, 0xe2},
	{0x21, 0xe9},
	{0x22, 0xf0},
	{0x23, 0xf8},
	{0x24, 0xfd},
	{0x25, 0xff},
	{0xfe, 0x00},
#endif
	{0xfe, 0x00},
	{0xc6, 0x20},
	{0xc7, 0x2b},
	/*gamma2*/
#if 1
	{0xfe, 0x02},
	{0x26, 0x0f},
	{0x27, 0x14},
	{0x28, 0x19},
	{0x29, 0x1e},
	{0x2a, 0x27},
	{0x2b, 0x33},
	{0x2c, 0x3b},
	{0x2d, 0x45},
	{0x2e, 0x59},
	{0x2f, 0x69},
	{0x30, 0x7c},
	{0x31, 0x89},
	{0x32, 0x98},
	{0x33, 0xae},
	{0x34, 0xc0},
	{0x35, 0xcf},
	{0x36, 0xda},
	{0x37, 0xe2},
	{0x38, 0xe9},
	{0x39, 0xf3},
	{0x3a, 0xf9},
	{0x3b, 0xff},
#else
	/*Gamma outdoor*/
	{0xfe, 0x02},
	{0x26, 0x17},
	{0x27, 0x18},
	{0x28, 0x1c},
	{0x29, 0x20},
	{0x2a, 0x28},
	{0x2b, 0x34},
	{0x2c, 0x40},
	{0x2d, 0x49},
	{0x2e, 0x5b},
	{0x2f, 0x6d},
	{0x30, 0x7d},
	{0x31, 0x89},
	{0x32, 0x97},
	{0x33, 0xac},
	{0x34, 0xc0},
	{0x35, 0xcf},
	{0x36, 0xda},
	{0x37, 0xe5},
	{0x38, 0xec},
	{0x39, 0xf8},
	{0x3a, 0xfd},
	{0x3b, 0xff},
#endif
	/*YCP*/
	{0xfe, 0x02},
	{0xd1, 0x32},
	{0xd2, 0x32},
	{0xd3, 0x40},
	{0xd6, 0xf0},
	{0xd7, 0x10},
	{0xd8, 0xda},
	{0xdd, 0x14},
	{0xde, 0x86},
	{0xed, 0x80},
	{0xee, 0x00},
	{0xef, 0x3f},
	{0xd8, 0xd8},
	/*abs*/
	{0xfe, 0x01},
	{0x9f, 0x40},
	/*LSC*/
	{0xfe, 0x01},
	{0xc2, 0x14},
	{0xc3, 0x0d},
	{0xc4, 0x0c},
	{0xc8, 0x15},
	{0xc9, 0x0d},
	{0xca, 0x0a},
	{0xbc, 0x24},
	{0xbd, 0x10},
	{0xbe, 0x0b},
	{0xb6, 0x25},
	{0xb7, 0x16},
	{0xb8, 0x15},
	{0xc5, 0x00},
	{0xc6, 0x00},
	{0xc7, 0x00},
	{0xcb, 0x00},
	{0xcc, 0x00},
	{0xcd, 0x00},
	{0xbf, 0x07},
	{0xc0, 0x00},
	{0xc1, 0x00},
	{0xb9, 0x00},
	{0xba, 0x00},
	{0xbb, 0x00},
	{0xaa, 0x01},
	{0xab, 0x01},
	{0xac, 0x00},
	{0xad, 0x05},
	{0xae, 0x06},
	{0xaf, 0x0e},
	{0xb0, 0x0b},
	{0xb1, 0x07},
	{0xb2, 0x06},
	{0xb3, 0x17},
	{0xb4, 0x0e},
	{0xb5, 0x0e},
	{0xd0, 0x09},
	{0xd1, 0x00},
	{0xd2, 0x00},
	{0xd6, 0x08},
	{0xd7, 0x00},
	{0xd8, 0x00},
	{0xd9, 0x00},
	{0xda, 0x00},
	{0xdb, 0x00},
	{0xd3, 0x0a},
	{0xd4, 0x00},
	{0xd5, 0x00},
	{0xa4, 0x00},
	{0xa5, 0x00},
	{0xa6, 0x77},
	{0xa7, 0x77},
	{0xa8, 0x77},
	{0xa9, 0x77},
	{0xa1, 0x80},
	{0xa2, 0x80},
	{0xfe, 0x01},
	{0xdf, 0x0d},
	{0xdc, 0x25},
	{0xdd, 0x30},
	{0xe0, 0x77},
	{0xe1, 0x80},
	{0xe2, 0x77},
	{0xe3, 0x90},
	{0xe6, 0x90},
	{0xe7, 0xa0},
	{0xe8, 0x90},
	{0xe9, 0xa0},
	{0xfe, 0x00},
	/*AWB*/
	{0xfe, 0x01},
	{0x4f, 0x00},
	{0x4f, 0x00},
	{0x4b, 0x01},
	{0x4f, 0x00},
	{0x4c, 0x01}, /*D75*/
	{0x4d, 0x71},
	{0x4e, 0x01},
	{0x4c, 0x01},
	{0x4d, 0x91},
	{0x4e, 0x01},
	{0x4c, 0x01},
	{0x4d, 0x70},
	{0x4e, 0x01},
	{0x4c, 0x01}, /*D65*/
	{0x4d, 0x90},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xb0},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0x8f},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0x6f},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xaf},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xd0},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xf0},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xcf},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xef},
	{0x4e, 0x02},
	{0x4c, 0x01},/*D50*/
	{0x4d, 0x6e},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8e},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xae},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xce},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x4d},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x6d},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8d},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xad},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xcd},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x4c},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x6c},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8c},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xac},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xcc},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xcb},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x4b},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x6b},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8b},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xab},
	{0x4e, 0x03},
	{0x4c, 0x01},/*CWF*/
	{0x4d, 0x8a},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xaa},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xca},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xca},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xc9},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0x8a},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0x89},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xa9},
	{0x4e, 0x04},
	{0x4c, 0x02},/*tl84*/
	{0x4d, 0x0b},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x0a},
	{0x4e, 0x05},
	{0x4c, 0x01},
	{0x4d, 0xeb},
	{0x4e, 0x05},
	{0x4c, 0x01},
	{0x4d, 0xea},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x09},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x29},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x2a},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x4a},
	{0x4e, 0x05},
	/*{0x4c , 0x02},*/
	/*{0x4d , 0x6a},*/
	/*{0x4e , 0x06},*/
	{0x4c, 0x02},
	{0x4d, 0x8a},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x49},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x69},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x89},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0xa9},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x48},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x68},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x69},
	{0x4e, 0x06},
	{0x4c, 0x02},/*H*/
	{0x4d, 0xca},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xc9},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xe9},
	{0x4e, 0x07},
	{0x4c, 0x03},
	{0x4d, 0x09},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xc8},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xe8},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xa7},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xc7},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xe7},
	{0x4e, 0x07},
	{0x4c, 0x03},
	{0x4d, 0x07},
	{0x4e, 0x07},
	{0x4f, 0x01},
	{0x50, 0x80},
	{0x51, 0xa8},
	{0x52, 0x47},
	{0x53, 0x38},
	{0x54, 0xc7},
	{0x56, 0x0e},
	{0x58, 0x08},
	{0x5b, 0x00},
	{0x5c, 0x74},
	{0x5d, 0x8b},
	{0x61, 0xdb},
	{0x62, 0xb8},
	{0x63, 0x86},
	{0x64, 0xc0},
	{0x65, 0x04},
	{0x67, 0xa8},
	{0x68, 0xb0},
	{0x69, 0x00},
	{0x6a, 0xa8},
	{0x6b, 0xb0},
	{0x6c, 0xaf},
	{0x6d, 0x8b},
	{0x6e, 0x50},
	{0x6f, 0x18},
	{0x73, 0xf0},
	{0x70, 0x0d},
	{0x71, 0x60},
	{0x72, 0x80},
	{0x74, 0x01},
	{0x75, 0x01},
	{0x7f, 0x0c},
	{0x76, 0x70},
	{0x77, 0x58},
	{0x78, 0xa0},
	{0x79, 0x5e},
	{0x7a, 0x54},
	{0x7b, 0x58},
	{0xfe, 0x00},
	/*CC*/
	{0xfe, 0x02},
	{0xc0, 0x01},
	{0xc1, 0x44},
	{0xc2, 0xfd},
	{0xc3, 0x04},
	{0xc4, 0xF0},
	{0xc5, 0x48},
	{0xc6, 0xfd},
	{0xc7, 0x46},
	{0xc8, 0xfd},
	{0xc9, 0x02},
	{0xca, 0xe0},
	{0xcb, 0x45},
	{0xcc, 0xec},
	{0xcd, 0x48},
	{0xce, 0xf0},
	{0xcf, 0xf0},
	{0xe3, 0x0c},
	{0xe4, 0x4b},
	{0xe5, 0xe0},
	/*ABS*/
	{0xfe, 0x01},
	{0x9f, 0x40},
	{0xfe, 0x00},
	/*OUTPUT*/
	{0xfe, 0x00},
	{0xf2, 0x0f},
	/*dark sun*/
	{0xfe, 0x02},
	{0x40, 0xbf},
	{0x46, 0xcf},
	{0xfe, 0x00},
	/*frame rate 50Hz*/
	{0xfe, 0x00},
	{0x05, 0x01},
	{0x06, 0x56},
	{0x07, 0x00},
	{0x08, 0x32},
	{0xfe, 0x01},
	{0x25, 0x00},
	{0x26, 0xfa},
	{0x27, 0x04},
	{0x28, 0xe2}, /*20fps*/
	{0x29, 0x06},
	{0x2a, 0xd6}, /*14fps*/
	{0x2b, 0x07},
	{0x2c, 0xd0}, /*12fps*/
	{0x2d, 0x0b},
	{0x2e, 0xb8}, /*8fps*/
	{0xfe, 0x00},

	/*SENSORDB("GC2145_Sensor_SVGA"},*/

	{0xfe, 0x00},
	{0xfd, 0x01},
	{0xfa, 0x00},
	/*crop window*/
	{0xfe, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x02},
	{0x96, 0x58},
	{0x97, 0x03},
	{0x98, 0x20},
	{0x99, 0x11},
	{0x9a, 0x06},
	/*AWB*/
	{0xfe, 0x00},
	{0xec, 0x02},
	{0xed, 0x02},
	{0xee, 0x30},
	{0xef, 0x48},
	{0xfe, 0x02},
	{0x9d, 0x08},
	{0xfe, 0x01},
	{0x74, 0x00},
	/*AEC*/
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0x60},
	{0x03, 0x02},
	{0x04, 0x48},
	{0x05, 0x18},
	{0x06, 0x50},
	{0x07, 0x10},
	{0x08, 0x38},
	{0x0a, 0x80},
	{0x21, 0x04},
	{0xfe, 0x00},
	{0x20, 0x03},
	{0xfe, 0x00},
	{0xff, 0xff},
};

struct aml_camera_i2c_fig_s gc2145_svga[] = {
	{0xfe, 0x00},
	{0xb6, 0x01},
	{0xfd, 0x01},
	{0xfa, 0x11},
	/*crop window*/
	{0xfe, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x02},
	{0x96, 0x5a},
	{0x97, 0x03},
	{0x98, 0x22},
	{0x99, 0x11},
	{0x9a, 0x06},
	/*AWB*/
	{0xfe, 0x00},
	{0xec, 0x02},
	{0xed, 0x02},
	{0xee, 0x30},
	{0xef, 0x48},
	{0xfe, 0x02},
	{0x9d, 0x08},
	{0xfe, 0x01},
	{0x74, 0x00},
	/*AEC*/
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0x60},
	{0x03, 0x02},
	{0x04, 0x48},
	{0x05, 0x18},
	{0x06, 0x50},
	{0x07, 0x10},
	{0x08, 0x38},
	{0x0a, 0x80},
	{0x21, 0x04},
	{0xfe, 0x00},
	{0x20, 0x03},
	{0xfe, 0x00},
	{0xff, 0xff},
};

struct aml_camera_i2c_fig_s gc2145_uxga[] = {
	{0xfe, 0x00},
	{0xfd, 0x00},
	{0xfa, 0x11},
	/*crop window*/
	{0xfe, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x04},
	{0x96, 0xb2},
	{0x97, 0x06},
	{0x98, 0x42},
	{0x99, 0x11},
	{0x9a, 0x06},
	/*AWB*/
	{0xfe, 0x00},
	{0xec, 0x06},
	{0xed, 0x04},
	{0xee, 0x60},
	{0xef, 0x90},
	{0xfe, 0x01},
	{0x74, 0x01},
	/*AEC*/
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0xc0},
	{0x03, 0x04},
	{0x04, 0x90},
	{0x05, 0x30},
	{0x06, 0x90},
	{0x07, 0x30},
	{0x08, 0x80},
	{0x0a, 0x82},
	{0xfe, 0x01},
	{0x21, 0x15},
	{0xfe, 0x00},
	{0x20, 0x15},
	{0xfe, 0x00},
	{0xff, 0xff},
};

struct aml_camera_i2c_fig_s gc2145_1280x960[] = {
	/*1280X960*/
	{0xfe, 0x00},
	{0xfa, 0x11},
	{0xfd, 0x00},
	{0x1c, 0x05},
	/*crop window*/
	{0xfe, 0x00},
	{0x99, 0x55},
	{0x9a, 0x06},
	{0x9b, 0x00},
	{0x9c, 0x00},
	{0x9d, 0x01},
	{0x9e, 0x23},
	{0x9f, 0x00},
	{0xa0, 0x00},
	{0xa1, 0x01},
	{0xa2, 0x23},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x03},
	{0x96, 0xc0},
	{0x97, 0x05},
	{0x98, 0x00},

	/*AWB*/
	{0xfe, 0x00},
	{0xec, 0x06},
	{0xed, 0x04},
	{0xee, 0x60},
	{0xef, 0x90},
	{0xfe, 0x01},
	{0x74, 0x01},
	/*AEC*/
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0xc0},
	{0x03, 0x04},
	{0x04, 0x90},
	{0x05, 0x30},
	{0x06, 0x90},
	{0x07, 0x30},
	{0x08, 0x80},
	{0x0a, 0x82},
	{0x21, 0x15},
	{0xfe, 0x00},
	{0x20, 0x15},
	{0xfe, 0x00},
};

struct aml_camera_i2c_fig_s gc2145_FLICKER_50HZ[] = {
	{0xfe, 0x00},
	{0x05, 0x01},
	{0x06, 0x56},
	{0x07, 0x00},
	{0x08, 0x32},
	{0xfe, 0x01},
	{0x25, 0x00},
	{0x26, 0xfa},
	{0x27, 0x04},
	{0x28, 0xe2}, /*20fps*/
	{0x29, 0x06},
	{0x2a, 0xd6}, /*14fps*/
	{0x2b, 0x07},
	{0x2c, 0xd0}, /*12fps*/
	{0x2d, 0x0b},
	{0x2e, 0xb8}, /*8fps*/
	{0xfe, 0x00},
	{0xff, 0xff},
};
struct aml_camera_i2c_fig_s gc2145_FLICKER_60HZ[] = {
	{0x05, 0x01},/*hb*/
	{0x06, 0x58},
	{0x07, 0x00},/*vb*/
	{0x08, 0x32},
	{0xfe, 0x01},
	{0x25, 0x00},/*step*/
	{0x26, 0xd0},
	{0x27, 0x04},/*level1*/
	{0x28, 0xe0},
	{0x29, 0x06},/*level2*/
	{0x2a, 0x80},
	{0x2b, 0x08},/*level3*/
	{0x2c, 0x20},
	{0x2d, 0x0b},/*level4*/
	{0x2e, 0x60},
	{0xfe, 0x00},
	{0xff, 0xff},
};

/* load GC2145 parameters */
void GC2145_init_regs(struct gc2145_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int i = 0;

	if (!dev->vminfo.isused)
		return;

	while (1) {
		if (GC2145_script[i].val == 0xff &&
			GC2145_script[i].addr == 0xff) {
			pr_info("GC2145_write_regs success in initial.\n");
			break;
		}
		if ((i2c_put_byte_add8_new(client, GC2145_script[i].addr,
					   GC2145_script[i].val)) < 0) {
			pr_err("fail in initial GC2145.\n");
			return;
		}
		i++;
	}
}

/*
 ************************************************************************
 * FUNCTION
 *    GC2145_set_param_wb
 *
 * DESCRIPTION
 *    wb setting.
 *
 * PARAMETERS
 *    none
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************
 */

void GC2145_set_param_wb(struct gc2145_device *dev,
			 enum  camera_wb_flip_e para) /* white balance */
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned int temp = 0;

	if (!dev->vminfo.isused)
		return;

	i2c_put_byte_add8_new(client, 0xfe, 0x00);
	temp = i2c_get_byte_add8(client, 0x82);

	switch (para) {

	case CAM_WB_AUTO:/* auto */
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		i2c_put_byte_add8_new(client, 0xb3, 0x61);
		i2c_put_byte_add8_new(client, 0xb4, 0x40);
		i2c_put_byte_add8_new(client, 0xb5, 0x61);
		i2c_put_byte_add8_new(client, 0x82, temp|0x2);
		break;

	case CAM_WB_CLOUD: /* cloud */
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		i2c_put_byte_add8_new(client, 0x82, temp&(~0x02));
		i2c_put_byte_add8_new(client, 0xb3, 0x58);
		i2c_put_byte_add8_new(client, 0xb4, 0x40);
		i2c_put_byte_add8_new(client, 0xb5, 0x50);
		break;

	case CAM_WB_DAYLIGHT: /*  */
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		i2c_put_byte_add8_new(client, 0x82, temp&(~0x02));
		i2c_put_byte_add8_new(client, 0xb3, 0x70);
		i2c_put_byte_add8_new(client, 0xb4, 0x40);
		i2c_put_byte_add8_new(client, 0xb5, 0x50);
		break;

	case CAM_WB_INCANDESCENCE:
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		i2c_put_byte_add8_new(client, 0x82, temp&(~0x02));
		i2c_put_byte_add8_new(client, 0xb3, 0x50);
		i2c_put_byte_add8_new(client, 0xb4, 0x40);
		i2c_put_byte_add8_new(client, 0xb5, 0xa8);
		break;

	case CAM_WB_TUNGSTEN:
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		i2c_put_byte_add8_new(client, 0x82, temp&(~0x02));
		i2c_put_byte_add8_new(client, 0xb3, 0xa0);
		i2c_put_byte_add8_new(client, 0xb4, 0x45);
		i2c_put_byte_add8_new(client, 0xb5, 0x40);
		break;

	case CAM_WB_FLUORESCENT:
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		i2c_put_byte_add8_new(client, 0x82, temp&(~0x02));
		i2c_put_byte_add8_new(client, 0xb3, 0x72);
		i2c_put_byte_add8_new(client, 0xb4, 0x40);
		i2c_put_byte_add8_new(client, 0xb5, 0x5b);
		break;

	case CAM_WB_MANUAL:
		/* TODO */
		break;
	default:
		break;
	}


} /* GC2145_set_param_wb */

/*
 ************************************************************************
 * FUNCTION
 *    GC2145_set_param_exposure
 *
 * DESCRIPTION
 *    exposure setting.
 *
 * PARAMETERS
 *    none
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************
 */

void GC2145_set_param_exposure(struct gc2145_device *dev,
			       enum camera_exposure_e para)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	if (!dev->vminfo.isused)
		return;

	switch (para) {

	case EXPOSURE_N4_STEP:
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x13, 0x55); /*target_y */
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;



	case EXPOSURE_N3_STEP:
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x13, 0x60); /*target_y */
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;


	case EXPOSURE_N2_STEP:
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x13, 0x65); /* target_y */
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;


	case EXPOSURE_N1_STEP:
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x13, 0x70); /* target_y */
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;

	case EXPOSURE_0_STEP:
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x13, 0x7b); /* target_y */
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;

	case EXPOSURE_P1_STEP:
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x13, 0x85); /* target_y */
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;

	case EXPOSURE_P2_STEP:
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x13, 0x90); /* target_y */
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;

	case EXPOSURE_P3_STEP:
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x13, 0x95); /* target_y */
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;

	case EXPOSURE_P4_STEP:
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x13, 0xa0); /* target_y */
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;

	default:
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x13, 0x7b); /* target_y */
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
		/* break; */

	}

} /* GC2145_set_param_exposure */

/*
 ************************************************************************
 * FUNCTION
 *    GC2145_set_param_effect
 *
 * DESCRIPTION
 *    effect setting.
 *
 * PARAMETERS
 *    none
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************
 */

void GC2145_set_param_effect(struct gc2145_device *dev,
			     enum camera_effect_flip_e para)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	if (!dev->vminfo.isused)
		return;

	switch (para) {
	case CAM_EFFECT_ENC_NORMAL:
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		i2c_put_byte_add8_new(client, 0x83, 0xe0);
		break;

	case CAM_EFFECT_ENC_GRAYSCALE:
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		i2c_put_byte_add8_new(client, 0x83, 0x12);
		break;

	case CAM_EFFECT_ENC_SEPIA:
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		i2c_put_byte_add8_new(client, 0x83, 0x82);
		break;

	case CAM_EFFECT_ENC_SEPIAGREEN:
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		i2c_put_byte_add8_new(client, 0x43, 0x52);
		break;

	case CAM_EFFECT_ENC_SEPIABLUE:
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		i2c_put_byte_add8_new(client, 0x43, 0x62);
		break;

	case CAM_EFFECT_ENC_COLORINV:
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		i2c_put_byte_add8_new(client, 0x83, 0x01);
		break;

	default:
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		i2c_put_byte_add8_new(client, 0x83, 0xe0);
		break;
	}

} /* GC2145_set_param_effect */

/*
 ************************************************************************
 * FUNCTION
 *    GC2145_NightMode
 *
 * DESCRIPTION
 *    This function night mode of GC2145.
 *
 * PARAMETERS
 *    none
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************
 */

void GC2145_set_night_mode(struct gc2145_device *dev,
			   enum  camera_night_mode_flip_e enable)
{

}

void GC2145_set_param_banding(struct gc2145_device *dev,
			      enum  camera_banding_flip_e banding)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char buf[4];
	int i = 0;

	switch (banding) {
	case CAM_BANDING_50HZ:
		while (1) {
			buf[0] = gc2145_FLICKER_50HZ[i].addr;
			buf[1] = gc2145_FLICKER_50HZ[i].val;
			if (gc2145_FLICKER_50HZ[i].val == 0xff &&
			     gc2145_FLICKER_50HZ[i].addr == 0xff) {
				pr_err("success in gc2145_FLICKER_50HZ gc2145\n");
				break;
			}
			if ((i2c_put_byte_add8(client, buf, 2)) < 0) {
				pr_err("fail in gc2145_FLICKER_50HZ gc2145\n");
				return;
			}
			i++;
		}
		break;
	case CAM_BANDING_60HZ:
		while (1) {
			buf[0] = gc2145_FLICKER_60HZ[i].addr;
			buf[1] = gc2145_FLICKER_60HZ[i].val;
			if (gc2145_FLICKER_60HZ[i].val == 0xff &&
			     gc2145_FLICKER_60HZ[i].addr == 0xff) {
				pr_err("success in gc2145_FLICKER_60HZ gc2145\n");
				break;
			}
			if ((i2c_put_byte_add8(client, buf, 2)) < 0) {
				pr_err("fail in gc2145_FLICKER_60HZ gc2145\n");
				return;
			}
			i++;
		}
		break;
	default:
		break;
	}

}

static int set_flip(struct gc2145_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char temp;
	unsigned char buf[2];

	if (!dev->vminfo.isused)
		return 0;

	i2c_put_byte_add8_new(client, 0xfe, 0x00);

	temp = i2c_get_byte_add8(client, 0x17);
	temp &= 0xfc;
	temp |= dev->cam_info.m_flip << 0;
	temp |= dev->cam_info.v_flip << 1;
	buf[0] = 0x17;
	buf[1] = temp;
	if ((i2c_put_byte_add8(client, buf, 2)) < 0) {
		pr_err("fail in setting sensor orientation\n");
		return -1;
	}

	return 0;
}

void GC2145_set_resolution(struct gc2145_device *dev, int height, int width)
{
	unsigned char buf[4];
	unsigned int value;
	unsigned int pid = 0, shutter;
	unsigned int i = 0;
	static unsigned int shutter_l;
	static unsigned int shutter_h;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	pr_info("%s :wxh = %d x %d\n", __func__, width, height);

	if (!dev->vminfo.isused)
		return;

	if ((width * height < 1600 * 1200)) {
		while (1) {
			buf[0] = gc2145_svga[i].addr;
			buf[1] = gc2145_svga[i].val;
			if (gc2145_svga[i].val == 0xff &&
			  gc2145_svga[i].addr == 0xff) {
				pr_info("success in gc2145_svga.\n");
				break;
			}
			if ((i2c_put_byte_add8(client, buf, 2)) < 0) {
				pr_err("fail in gc2145_svga.\n");
				return;
			}
			i++;
		}
		gc2145_frmintervals_active.numerator = 1;
		gc2145_frmintervals_active.denominator = 15;
		GC2145_h_active = 800;
		GC2145_v_active = 600;
		mdelay(80);
	} else if (width * height >= 1200 * 1600) {
		buf[0] = 0xfe;
		buf[1] = 0x00;
		i2c_put_byte_add8(client, buf, 2);

		buf[0] = 0xb6;
		buf[1] = 0x00;
		i2c_put_byte_add8(client, buf, 2);

		buf[0] = 0x03;
		value = i2c_get_byte_add8(client, 0x03);
		shutter_l = value;
		/*printk(KERN_INFO"set camera 0x03=0x%x\n", value);*/
		pid |= (value << 8);

		buf[0] = 0x04;
		value = i2c_get_byte_add8(client, 0x04);
		shutter_h = value;
		/*printk(KERN_INFO"set camera 0x04=0x%x\n", value);*/
		pid |= (value & 0x1f);

		shutter = pid;
		/*printk(KERN_INFO "set camera shutter=0x%x\n", shutter);*/

		while (1) {
			buf[0] = gc2145_uxga[i].addr;
			buf[1] = gc2145_uxga[i].val;
			if (gc2145_uxga[i].val == 0xff &&
			    gc2145_uxga[i].addr == 0xff) {
				pr_info("gc2145_write_regs success in gc2145_uxga.\n");
				break;
			}
			if ((i2c_put_byte_add8(client, buf, 2)) < 0) {
				pr_err("fail in gc2145_uxga.\n");
				return;
			}
			i++;
		}

		shutter = shutter / 2;
		if (shutter < 1)
			shutter = 1;

		buf[0] = 0x03;
		buf[1] = ((shutter >> 8) & 0xff);
		i2c_put_byte_add8(client, buf, 2);

		buf[0] = 0x04;
		buf[1] = (shutter & 0x1f);
		i2c_put_byte_add8(client, buf, 2);

		gc2145_frmintervals_active.denominator = 5;
		gc2145_frmintervals_active.numerator = 1;
		GC2145_h_active = 1600;
		GC2145_v_active = 1200;
		mdelay(130);
	}
	set_flip(dev);
}    /* GC2145_set_resolution */

unsigned char v4l_2_gc2145(int val)
{
	int ret = val / 0x20;

	if (ret < 4)
		return ret * 0x20 + 0x80;
	else if (ret < 8)
		return ret * 0x20 + 0x20;
	else
		return 0;
}

static int convert_canvas_index(unsigned int v4l2_format,
				unsigned int start_canvas)
{
	int canvas = start_canvas;

	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_VYUY:
		canvas = start_canvas;
		break;
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
		canvas = start_canvas;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		canvas = start_canvas | ((start_canvas + 1) << 8);
		break;
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420:
		if (v4l2_format == V4L2_PIX_FMT_YUV420)
			canvas = start_canvas | ((start_canvas + 1) << 8) |
				((start_canvas + 2) << 16);
		else
			canvas = start_canvas | ((start_canvas + 2) << 8) |
				((start_canvas + 1) << 16);
		break;
	default:
		break;
	}
	return canvas;
}

static int gc2145_setting(struct gc2145_device *dev, int PROP_ID, int value)
{
	int ret = 0;
	/* unsigned char cur_val; */
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	if (!dev->vminfo.isused)
		return 0;
	/*here register not finished */
	switch (PROP_ID)  {

	case V4L2_CID_BRIGHTNESS:
		dprintk(dev, 1, "setting brightned:%d\n", v4l_2_gc2145(value));
		ret = i2c_put_byte_add8_new(client, 0xdc, v4l_2_gc2145(value));
		break;
	case V4L2_CID_CONTRAST:
		ret = i2c_put_byte_add8_new(client, 0xde, value);
		break;
	case V4L2_CID_SATURATION:
		ret = i2c_put_byte_add8_new(client, 0xd9, value);
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		if (gc2145_qctrl[0].default_value != value) {
			gc2145_qctrl[0].default_value = value;
			GC2145_set_param_wb(dev, value);
			pr_info("set camera  white_balance=%d.\n ",
				value);
		}
		break;
	case V4L2_CID_EXPOSURE:
		if (gc2145_qctrl[1].default_value != value) {
			gc2145_qctrl[1].default_value = value;
			GC2145_set_param_exposure(dev, value);
			pr_info("set camera  exposure=%d.\n",
				value);
		}
		break;
	case V4L2_CID_COLORFX:
		if (gc2145_qctrl[2].default_value != value) {
			gc2145_qctrl[2].default_value = value;
			/* GC2145_set_param_effect(dev,value); */
			pr_info("set camera  effect=%d.\n",
				value);
		}
		break;
	case V4L2_CID_WHITENESS:
		if (gc2145_qctrl[3].default_value != value) {
			gc2145_qctrl[3].default_value = value;
			pr_info("@@@SP_000:GC2145_set_param_banding,value=%d\n",
				value);
			GC2145_set_param_banding(dev, value);
			pr_info("@@@SP_111:gc2145_night_or_normal = %d",
				gc2145_night_or_normal);
			GC2145_set_night_mode(dev, gc2145_night_or_normal);
			pr_info("set camera  banding=%d.\n",
				value);
		}
		break;
	case V4L2_CID_BLUE_BALANCE:
		if (gc2145_qctrl[4].default_value != value) {
			gc2145_qctrl[4].default_value = value;
			pr_info("@@@SP_222:GC2145_set_night_mode,night mode=%d\n",
				value);
			pr_info("@@@SP_333:gc2145_night_or_normal = %d",
				gc2145_night_or_normal);
			GC2145_set_night_mode(dev, value);
			pr_info("set camera  scene mode=%d.\n",
				value);
		}
		break;
	case V4L2_CID_HFLIP:    /* set flip on H. */
		value = value & 0x3;
		if (gc2145_qctrl[5].default_value != value) {
			gc2145_qctrl[5].default_value = value;
			pr_info(" set camera  h filp =%d.\n ", value);
		}
		break;
	case V4L2_CID_VFLIP:    /* set flip on V. */
		break;
	case V4L2_CID_ZOOM_ABSOLUTE:
		if (gc2145_qctrl[7].default_value != value)
			gc2145_qctrl[7].default_value = value;
		break;
	case V4L2_CID_ROTATE:
		if (gc2145_qctrl[8].default_value != value) {
			gc2145_qctrl[8].default_value = value;
			pr_info(" set camera  rotate =%d.\n ", value);
		}
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}

static void power_down_gc2145(struct gc2145_device *dev)
{

}

/*
 *------------------------------------------------------------------
 *   DMA and thread functions
 *  ------------------------------------------------------------------
 */

#define TSTAMP_MIN_Y    24
#define TSTAMP_MAX_Y    (TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X  10
#define TSTAMP_MIN_X    (54 + TSTAMP_INPUT_X)

static void gc2145_fillbuff(struct gc2145_fh *fh, struct gc2145_buffer *buf)
{
	int ret;
	void *vbuf;
	struct vm_output_para para = {0};
	struct gc2145_device *dev = fh->dev;

	if (dev->vminfo.mem_alloc_succeed)
		vbuf = (void *)videobuf_to_res(&buf->vb);
	else
		vbuf = videobuf_to_vmalloc(&buf->vb);
	dprintk(dev, 1, "%s\n", __func__);
	if (!vbuf)
		return;
	/*  0x18221223 indicate the memory type is MAGIC_VMAL_MEM*/
	if (dev->vminfo.mem_alloc_succeed) {
		if (buf->canvas_id == 0)
			buf->canvas_id = convert_canvas_index(fh->fmt->fourcc,
		      CAMERA_USER_CANVAS_INDEX + buf->vb.i * 3);
		para.v4l2_memory = MAGIC_RE_MEM;
	} else
		para.v4l2_memory = MAGIC_VMAL_MEM;
	para.mirror = gc2145_qctrl[5].default_value & 3;/* not set */
	para.v4l2_format = fh->fmt->fourcc;
	para.zoom = gc2145_qctrl[7].default_value;
	para.angle = gc2145_qctrl[8].default_value;
	para.vaddr = (uintptr_t)vbuf;
	/* para.ext_canvas = buf->canvas_id; */
	para.ext_canvas = 0;
	para.width = buf->vb.width;
	para.height = buf->vb.height;
	dev->vminfo.vdin_id = dev->cam_info.vdin_path;
	ret = vm_fill_this_buffer(&buf->vb, &para, &dev->vminfo);
	/*if the vm is not used by sensor ,*/
	/*we let vm_fill_this_buffer() return -2*/
	if (ret == -2)
		msleep(40);
	buf->vb.state = VIDEOBUF_DONE;
}

static void gc2145_thread_tick(struct gc2145_fh *fh)
{
	struct gc2145_buffer *buf;
	struct gc2145_device *dev = fh->dev;
	struct gc2145_dmaqueue *dma_q = &dev->vidq;

	unsigned long flags = 0;

	dprintk(dev, 1, "Thread tick\n");
	if (!fh->stream_on) {
		dprintk(dev, 1, "sensor doesn't stream on\n");
		return;
	}
	spin_lock_irqsave(&dev->slock, flags);
	if (list_empty(&dma_q->active)) {
		dprintk(dev, 1, "No active queue to serve\n");
		goto unlock;
	}

	buf = list_entry(dma_q->active.next,
			 struct gc2145_buffer, vb.queue);
	dprintk(dev, 1, "%s\n", __func__);
	dprintk(dev, 1, "list entry get buf is %p\n", buf);

	if (!(fh->f_flags & O_NONBLOCK)) {
		/* Nobody is waiting on this buffer, return */
		if (!waitqueue_active(&buf->vb.done))
			goto unlock;
	}
	buf->vb.state = VIDEOBUF_ACTIVE;

	list_del(&buf->vb.queue);

	do_gettimeofday(&buf->vb.ts);

	/* Fill buffer */
	spin_unlock_irqrestore(&dev->slock, flags);
	gc2145_fillbuff(fh, buf);
	dprintk(dev, 1, "filled buffer %p\n", buf);

	wake_up(&buf->vb.done);
	dprintk(dev, 2, "[%p/%d] wakeup\n", buf, buf->vb. i);
	return;
unlock:
	spin_unlock_irqrestore(&dev->slock, flags);
}

#define frames_to_ms(frames)	\
	((frames * WAKE_NUMERATOR * 1000) / WAKE_DENOMINATOR)

static void gc2145_sleep(struct gc2145_fh *fh)
{
	struct gc2145_device *dev = fh->dev;
	struct gc2145_dmaqueue *dma_q = &dev->vidq;

	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 1, "%s dma_q=0x%08lx\n", __func__,
		(unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
		goto stop_task;

	/* Calculate time to wake up */
	/* timeout = msecs_to_jiffies(frames_to_ms(1)); */

	gc2145_thread_tick(fh);

	schedule_timeout_interruptible(2);

stop_task:
	remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int gc2145_thread(void *data)
{
	struct gc2145_fh  *fh = data;
	struct gc2145_device *dev = fh->dev;

	dprintk(dev, 1, "thread started\n");

	set_freezable();

	for (; ;) {
		gc2145_sleep(fh);

		if (kthread_should_stop())
			break;
	}
	dprintk(dev, 1, "thread: exit\n");
	return 0;
}

static int gc2145_start_thread(struct gc2145_fh *fh)
{
	struct gc2145_device *dev = fh->dev;
	struct gc2145_dmaqueue *dma_q = &dev->vidq;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;

	dprintk(dev, 1, "%s\n", __func__);

	dma_q->kthread = kthread_run(gc2145_thread, fh, "gc2145");

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}
	/* Wakes thread */
	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void gc2145_stop_thread(struct gc2145_dmaqueue  *dma_q)
{
	struct gc2145_device *dev =
		container_of(dma_q, struct gc2145_device, vidq);

	dprintk(dev, 1, "%s\n", __func__);
	/* shutdown control thread */
	if (dma_q->kthread) {
		kthread_stop(dma_q->kthread);
		dma_q->kthread = NULL;
	}
}

/*
 *------------------------------------------------------------------
 *    Videobuf operations
 *  ------------------------------------------------------------------
 */

static int
vmall_buffer_setup(struct videobuf_queue *vq, unsigned int *count,
		   unsigned int *size)
{
	struct gc2145_fh  *fh = vq->priv_data;
	struct gc2145_device *dev  = fh->dev;
	/* int bytes = fh->fmt->depth >> 3 ; */
	*size = (fh->width * fh->height * fh->fmt->depth) >> 3;
	if (*count == 0)
		*count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
		(*count)--;

	dprintk(dev, 1, "%s, count=%d, size=%d\n", __func__,
		*count, *size);

	return 0;
}

static int
res_buffer_setup(struct videobuf_queue *vq, unsigned int *count,
		 unsigned int *size)
{
	struct videobuf_res_privdata *res = vq->priv_data;
	struct gc2145_fh *fh = container_of(res, struct gc2145_fh, res);
	struct gc2145_device *dev  = fh->dev;
	/* int bytes = fh->fmt->depth >> 3 ; */
	int height = fh->height;

	if (height == 1080)
		height = 1088;
	*size = (fh->width * height * fh->fmt->depth) >> 3;
	if (*count == 0)
		*count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
		(*count)--;

	dprintk(dev, 1, "%s, count=%d, size=%d\n", __func__,
		*count, *size);

	return 0;
}

static void free_vmall_buffer(struct videobuf_queue *vq,
			      struct gc2145_buffer *buf)
{
	struct gc2145_fh *fh;
	struct gc2145_device *dev;

	fh = vq->priv_data;
	dev = fh->dev;

	dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);

	videobuf_waiton(vq, &buf->vb, 0, 0);
	if (in_interrupt())
		WARN_ON(1);
	videobuf_vmalloc_free(&buf->vb);
	dprintk(dev, 1, "free_vmall_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

static void free_res_buffer(struct videobuf_queue *vq,
			    struct gc2145_buffer *buf)
{
	struct gc2145_fh *fh;
	struct gc2145_device *dev;
	struct videobuf_res_privdata *res = vq->priv_data;

	fh = container_of(res, struct gc2145_fh, res);
	dev = fh->dev;

	dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);

	videobuf_waiton(vq, &buf->vb, 0, 0);
	if (in_interrupt())
		WARN_ON(1);
	videobuf_res_free(vq, &buf->vb);
	dprintk(dev, 1, "free_res_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define norm_maxw() 1920
#define norm_maxh() 1600
static int
vmall_buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
		     enum v4l2_field field)
{
	struct gc2145_fh *fh;
	struct gc2145_device *dev;
	struct gc2145_buffer *buf;
	int rc;

	fh  = vq->priv_data;
	dev = fh->dev;
	buf = container_of(vb, struct gc2145_buffer, vb);
	/* int bytes = fh->fmt->depth >> 3 ; */
	dprintk(dev, 1, "%s, field=%d\n", __func__, field);

	WARN_ON(fh->fmt == NULL);

	if (fh->width  < 48 || fh->width  > norm_maxw() ||
	    fh->height < 32 || fh->height > norm_maxh())
		return -EINVAL;

	buf->vb.size = (fh->width * fh->height * fh->fmt->depth) >> 3;
	if (buf->vb.baddr != 0 && buf->vb.bsize < buf->vb.size)
		return -EINVAL;

	/* These properties only change when queue is idle, see s_fmt */
	buf->fmt       = fh->fmt;
	buf->vb.width  = fh->width;
	buf->vb.height = fh->height;
	buf->vb.field  = field;

	/* precalculate_bars(fh); */

	if (buf->vb.state == VIDEOBUF_NEEDS_INIT) {
		rc = videobuf_iolock(vq, &buf->vb, NULL);
		if (rc < 0)
			goto fail;
	}

	buf->vb.state = VIDEOBUF_PREPARED;

	return 0;

fail:
	free_vmall_buffer(vq, buf);
	return rc;
}

static int
res_buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
		   enum v4l2_field field)
{
	struct gc2145_fh *fh;
	struct gc2145_device *dev;
	struct gc2145_buffer *buf;
	int rc;
	struct videobuf_res_privdata *res = vq->priv_data;

	fh = container_of(res, struct gc2145_fh, res);
	dev = fh->dev;
	buf = container_of(vb, struct gc2145_buffer, vb);
	dprintk(dev, 1, "%s, field=%d\n", __func__, field);

	WARN_ON(fh->fmt == NULL);

	if (fh->width  < 48 || fh->width  > norm_maxw() ||
	    fh->height < 32 || fh->height > norm_maxh())
		return -EINVAL;

	buf->vb.size = (fh->width * fh->height * fh->fmt->depth) >> 3;
	if (buf->vb.baddr != 0 &&  buf->vb.bsize < buf->vb.size)
		return -EINVAL;

	/* These properties only change when queue is idle, see s_fmt */
	buf->fmt       = fh->fmt;
	buf->vb.width  = fh->width;
	buf->vb.height = fh->height;
	buf->vb.field  = field;

	/* precalculate_bars(fh); */

	if (buf->vb.state == VIDEOBUF_NEEDS_INIT) {
		rc = videobuf_iolock(vq, &buf->vb, NULL);
		if (rc < 0)
			goto fail;
	}

	buf->vb.state = VIDEOBUF_PREPARED;

	return 0;

fail:
	free_res_buffer(vq, buf);
	return rc;
}

static void
res_buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct gc2145_fh *fh;
	struct gc2145_device *dev;
	struct gc2145_dmaqueue *vidq;
	struct gc2145_buffer *buf  = container_of(vb, struct gc2145_buffer, vb);
	struct videobuf_res_privdata *res = vq->priv_data;

	fh = container_of(res, struct gc2145_fh, res);
	dev = fh->dev;
	vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void
vmall_buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct gc2145_fh *fh;
	struct gc2145_device *dev;
	struct gc2145_dmaqueue *vidq;
	struct gc2145_buffer *buf  = container_of(vb, struct gc2145_buffer, vb);

	fh = vq->priv_data;
	dev = fh->dev;
	vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void res_buffer_release(struct videobuf_queue *vq,
			       struct videobuf_buffer *vb)
{
	struct gc2145_fh *fh;
	struct gc2145_device *dev;
	struct gc2145_buffer   *buf  =
		container_of(vb, struct gc2145_buffer, vb);
	struct videobuf_res_privdata *res = vq->priv_data;

	fh = container_of(res, struct gc2145_fh, res);
	dev = (struct gc2145_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_res_buffer(vq, buf);
}

static void vmall_buffer_release(struct videobuf_queue *vq,
				 struct videobuf_buffer *vb)
{
	struct gc2145_fh *fh;
	struct gc2145_device *dev;
	struct gc2145_buffer   *buf  =
		container_of(vb, struct gc2145_buffer, vb);
	fh   = vq->priv_data;
	dev = (struct gc2145_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_vmall_buffer(vq, buf);
}

static struct videobuf_queue_ops gc2145_video_vmall_qops = {
	.buf_setup      = vmall_buffer_setup,
	.buf_prepare    = vmall_buffer_prepare,
	.buf_queue      = vmall_buffer_queue,
	.buf_release    = vmall_buffer_release,
};

static struct videobuf_queue_ops gc2145_video_res_qops = {
	.buf_setup      = res_buffer_setup,
	.buf_prepare    = res_buffer_prepare,
	.buf_queue      = res_buffer_queue,
	.buf_release    = res_buffer_release,
};

/*
 *------------------------------------------------------------------
 *    IOCTL vidioc handling
 *  ------------------------------------------------------------------
 */

static int vidioc_querycap(struct file *file, void  *priv,
			   struct v4l2_capability *cap)
{
	struct gc2145_fh  *fh  = priv;
	struct gc2145_device *dev = fh->dev;

	strcpy(cap->driver, "gc2145");
	strcpy(cap->card, "gc2145.canvas");
	if (dev->cam_info.front_back == 0)
		strcat(cap->card, "back");
	else
		strcat(cap->card, "front");

	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = GC2145_CAMERA_VERSION;
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
				| V4L2_CAP_READWRITE;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
				   struct v4l2_fmtdesc *f)
{
	struct gc2145_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}
static int vidioc_enum_frameintervals(struct file *file, void *priv,
				      struct v4l2_frmivalenum *fival)
{
	unsigned int k;

	if (fival->index > ARRAY_SIZE(gc2145_frmivalenum))
		return -EINVAL;

	for (k = 0; k < ARRAY_SIZE(gc2145_frmivalenum); k++) {
		if ((fival->index == gc2145_frmivalenum[k].index) &&
		    (fival->pixel_format ==
					gc2145_frmivalenum[k].pixel_format) &&
		    (fival->width == gc2145_frmivalenum[k].width) &&
		    (fival->height == gc2145_frmivalenum[k].height)) {
			memcpy(fival, &gc2145_frmivalenum[k],
					sizeof(struct v4l2_frmivalenum));
			return 0;
		}
	}

	return -EINVAL;

}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct gc2145_fh *fh = priv;

	f->fmt.pix.width        = fh->width;
	f->fmt.pix.height       = fh->height;
	f->fmt.pix.field        = fh->vb_vidq.field;
	f->fmt.pix.pixelformat  = fh->fmt->fourcc;
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fh->fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	struct gc2145_fh  *fh  = priv;
	struct gc2145_device *dev = fh->dev;
	struct gc2145_fmt *fmt;
	enum v4l2_field field;
	unsigned int maxw, maxh;

	fmt = get_format(f);
	if (!fmt) {
		dprintk(dev, 1, "Fourcc format (0x%08x) invalid.\n",
			f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	field = f->fmt.pix.field;

	if (field == V4L2_FIELD_ANY)
		field = V4L2_FIELD_INTERLACED;
	else if (field != V4L2_FIELD_INTERLACED) {
		dprintk(dev, 1, "Field type invalid.\n");
		return -EINVAL;
	}

	maxw  = norm_maxw();
	maxh  = norm_maxh();

	f->fmt.pix.field = field;
	v4l_bound_align_image(&f->fmt.pix.width, 48, maxw, 2,
			      &f->fmt.pix.height, 32, maxh, 0, 0);
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;

	return 0;
}

/*FIXME: This seems to be generic enough to be at videodev2 */
static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct gc2145_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;
	struct gc2145_device *dev = fh->dev;
	int ret;

	f->fmt.pix.width = (f->fmt.pix.width +
					(CANVAS_WIDTH_ALIGN - 1)) &
					(~(CANVAS_WIDTH_ALIGN - 1));
	if ((f->fmt.pix.pixelformat == V4L2_PIX_FMT_YVU420) ||
	    (f->fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420))
		f->fmt.pix.width = (f->fmt.pix.width +
						(CANVAS_WIDTH_ALIGN * 2 - 1)) &
						(~(CANVAS_WIDTH_ALIGN * 2 - 1));

	ret = vidioc_try_fmt_vid_cap(file, fh, f);
	if (ret < 0)
		return ret;

	mutex_lock(&q->vb_lock);

	if (videobuf_queue_is_busy(&fh->vb_vidq)) {
		dprintk(fh->dev, 1, "%s queue busy\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	fh->fmt           = get_format(f);
	fh->width         = f->fmt.pix.width;
	fh->height        = f->fmt.pix.height;
	fh->vb_vidq.field = f->fmt.pix.field;
	fh->type          = f->type;
#if 0
	set_flip(dev);
	if (f->fmt.pix.pixelformat == V4L2_PIX_FMT_RGB24) {
		vidio_set_fmt_ticks = 1;
		GC2145_set_resolution(dev, fh->height, fh->width);
	} else if (vidio_set_fmt_ticks == 1)
		GC2145_set_resolution(dev, fh->height, fh->width);
#else
	GC2145_set_resolution(dev, fh->height, fh->width);
#endif
	ret = 0;
out:
	mutex_unlock(&q->vb_lock);

	return ret;
}

static int vidioc_g_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms)
{
	struct gc2145_fh *fh = priv;
	struct gc2145_device *dev = fh->dev;
	struct v4l2_captureparm *cp = &parms->parm.capture;

	dprintk(dev, 3, "vidioc_g_parm\n");
	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;

	cp->timeperframe = gc2145_frmintervals_active;
	pr_debug("g_parm,deno=%d, numerator=%d\n",
			cp->timeperframe.denominator,
			cp->timeperframe.numerator);
	return 0;
}


static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct gc2145_fh  *fh = priv;

	return videobuf_reqbufs(&fh->vb_vidq, p);
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc2145_fh  *fh = priv;
	int ret = videobuf_querybuf(&fh->vb_vidq, p);

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_M8) {
		if (ret == 0)
			p->reserved  = convert_canvas_index(fh->fmt->fourcc,
					CAMERA_USER_CANVAS_INDEX +
					p->index * 3);
		else
			p->reserved = 0;
	}
	return ret;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc2145_fh *fh = priv;

	return videobuf_qbuf(&fh->vb_vidq, p);
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc2145_fh  *fh = priv;

	return videobuf_dqbuf(&fh->vb_vidq, p,
			       file->f_flags & O_NONBLOCK);
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct gc2145_fh  *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct gc2145_fh  *fh = priv;
	struct vdin_parm_s para;
	unsigned int vdin_path;
	int ret = 0;

	pr_info("vidioc_streamon+++\n ");
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;

	memset(&para, 0, sizeof(para));
	para.port  = TVIN_PORT_CAMERA;
	para.fmt = TVIN_SIG_FMT_MAX;
	para.frame_rate = gc2145_frmintervals_active.denominator;
	para.h_active = GC2145_h_active;
	para.v_active = GC2145_v_active;
	para.hsync_phase = 1;
	para.vsync_phase = 1;
	para.hs_bp = 0;
	para.vs_bp = 3;
	para.bt_path = fh->dev->cam_info.bt_path;
	vdin_path = fh->dev->cam_info.vdin_path;
	para.cfmt = TVIN_YUV422;
	para.dfmt = TVIN_NV21;
	para.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;

	if (is_first_time_open == 0) {
		para.skip_count =  4;
		is_first_time_open = 1;
	} else
		para.skip_count =  0;

	ret =  videobuf_streamon(&fh->vb_vidq);
	if (ret == 0) {
		if (fh->dev->vminfo.isused) {
			vops->start_tvin_service(vdin_path, &para);
			fh->stream_on = 1;
		}
	}

	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct gc2145_fh  *fh = priv;
	unsigned int vdin_path;
	int ret = 0;

	pr_info("vidioc_streamoff+++\n ");
	vdin_path = fh->dev->cam_info.vdin_path;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;
	ret = videobuf_streamoff(&fh->vb_vidq);
	if (ret == 0) {
		if (fh->dev->vminfo.isused)
			vops->stop_tvin_service(vdin_path);
		fh->stream_on        = 0;
	}
	return ret;
}

static int vidioc_enum_framesizes(struct file *file, void *fh,
				  struct v4l2_frmsizeenum *fsize)
{
	int ret = 0, i = 0;
	struct gc2145_fmt *fmt = NULL;
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
	    || (fmt->fourcc == V4L2_PIX_FMT_YVU420)
	    || (fmt->fourcc == V4L2_PIX_FMT_YUV420)) {
		if (fsize->index >= ARRAY_SIZE(gc2145_prev_resolution))
			return -EINVAL;
		frmsize = &gc2145_prev_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	} else if (fmt->fourcc == V4L2_PIX_FMT_RGB24) {
		if (fsize->index >= ARRAY_SIZE(gc2145_pic_resolution))
			return -EINVAL;
		frmsize = &gc2145_pic_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	return ret;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id i)
{
	return 0;
}

/* only one input in this sample driver */
static int vidioc_enum_input(struct file *file, void *priv,
			     struct v4l2_input *inp)
{
	/* if  (inp->index >= NUM_INPUTS) */
	/* return -EINVAL; */

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	inp->std = V4L2_STD_525_60;
	sprintf(inp->name, "Camera %u", inp->index);

	return 0;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct gc2145_fh *fh = priv;
	struct gc2145_device *dev = fh->dev;

	*i = dev->input;

	return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct gc2145_fh *fh = priv;
	struct gc2145_device *dev = fh->dev;

	/* if  (i >= NUM_INPUTS) */
	/* return -EINVAL; */

	dev->input = i;
	/* precalculate_bars(fh); */

	return 0;
}

/* --- controls ---------------------------------------------- */
static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gc2145_qctrl); i++)
		if (qc->id && qc->id == gc2145_qctrl[i].id) {
			memcpy(qc, &(gc2145_qctrl[i]),
			       sizeof(*qc));
			return 0;
		}

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct gc2145_fh *fh = priv;
	struct gc2145_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(gc2145_qctrl); i++)
		if (ctrl->id == gc2145_qctrl[i].id) {
			ctrl->value = dev->qctl_regs[i];
			return 0;
		}

	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct gc2145_fh *fh = priv;
	struct gc2145_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(gc2145_qctrl); i++)
		if (ctrl->id == gc2145_qctrl[i].id) {
			if (ctrl->value < gc2145_qctrl[i].minimum ||
			    ctrl->value > gc2145_qctrl[i].maximum ||
			    gc2145_setting(dev, ctrl->id, ctrl->value) < 0)
				return -ERANGE;
			dev->qctl_regs[i] = ctrl->value;
			return 0;
		}
	return -EINVAL;
}

/*
 *------------------------------------------------------------------
 *   File operations for the device
 * -----------------------------------------------------------------
 */

static int gc2145_open(struct file *file)
{
	struct gc2145_device *dev = video_drvdata(file);
	struct gc2145_fh *fh = NULL;
	int retval = 0;

	dev->vminfo.vdin_id = dev->cam_info.vdin_path;
	dev->vminfo.bt_path_count = dev->cam_info.bt_path_count;

#if CONFIG_CMA
	vm_init_resource(16 * SZ_1M, &dev->vminfo);
#endif
	if (dev->vminfo.isused) {
		pr_info("%s, front_back = %d\n",
		  __func__, dev->cam_info.front_back);
		aml_cam_init(&dev->cam_info);
	}

	GC2145_init_regs(dev);
	msleep(40);
	mutex_lock(&dev->mutex);
	dev->users++;
	if (dev->users > 1) {
		dev->users--;
		pr_err("gc2145 device is opened, device is busy\n");
		mutex_unlock(&dev->mutex);
		return -EBUSY;
	}

	dprintk(dev, 1, "open %s type=%s users=%d\n",
		video_device_node_name(dev->vdev),
		v4l2_type_names[V4L2_BUF_TYPE_VIDEO_CAPTURE], dev->users);

	/* init video dma queues */
	INIT_LIST_HEAD(&dev->vidq.active);
	init_waitqueue_head(&dev->vidq.wq);
	spin_lock_init(&dev->slock);
	/* allocate + initialize per filehandle data */
	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (fh == NULL) {
		dev->users--;
		retval = -ENOMEM;
	}
	mutex_unlock(&dev->mutex);

	if (retval)
		return retval;

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock(&(dev->wake_lock));
#endif

	file->private_data = fh;
	fh->dev      = dev;

	fh->type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fh->fmt      = &formats[0];
	fh->width    = 1600;/* 640//zyy */
	fh->height   = 1198;/* 1200;//480 */
	fh->stream_on = 0;
	fh->f_flags  = file->f_flags;
	/* Resets frame counters */
	/*dev->jiffies = jiffies;*/

	/* TVIN_SIG_FMT_CAMERA_640X480P_30Hz, */
	/* TVIN_SIG_FMT_CAMERA_800X600P_30Hz, */
	/* TVIN_SIG_FMT_CAMERA_1024X768P_30Hz, // 190 */
	/* TVIN_SIG_FMT_CAMERA_1920X1080P_30Hz, */
	/* TVIN_SIG_FMT_CAMERA_1280X720P_30Hz, */

	if (dev->vminfo.mem_alloc_succeed) {
		fh->res.start = dev->vminfo.buffer_start;
		fh->res.end = dev->vminfo.buffer_start +
						dev->vminfo.vm_buf_size - 1;
		fh->res.magic = MAGIC_RE_MEM;
		fh->res.priv = NULL;
		videobuf_queue_res_init(&fh->vb_vidq,
			&gc2145_video_res_qops, NULL,
			&dev->slock, fh->type,
			V4L2_FIELD_INTERLACED,
			sizeof(struct gc2145_buffer),
			(void *)&fh->res, NULL);
	} else {
		videobuf_queue_vmalloc_init(&fh->vb_vidq,
				&gc2145_video_vmall_qops, NULL,
				&dev->slock, fh->type,
				V4L2_FIELD_INTERLACED,
				sizeof(struct gc2145_buffer), fh, NULL);
	}

	gc2145_start_thread(fh);
	gc2145_have_open = 1;
	return 0;
}

static ssize_t
gc2145_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct gc2145_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
					    file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int
gc2145_poll(struct file *file, struct poll_table_struct *wait)
{
	struct gc2145_fh        *fh = file->private_data;
	struct gc2145_device       *dev = fh->dev;
	struct videobuf_queue *q = &fh->vb_vidq;

	dprintk(dev, 1, "%s\n", __func__);

	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int gc2145_close(struct file *file)
{
	struct gc2145_fh         *fh = file->private_data;
	struct gc2145_device *dev       = fh->dev;
	struct gc2145_dmaqueue *vidq = &dev->vidq;
	struct video_device  *vdev = video_devdata(file);
	unsigned int vdin_path;

	vdin_path = fh->dev->cam_info.vdin_path;
	gc2145_have_open = 0;
	is_first_time_open = 0;

	gc2145_stop_thread(vidq);
	videobuf_stop(&fh->vb_vidq);
	if (fh->stream_on) {
		if (dev->vminfo.isused) {
			pr_info("%s, vdin_path = %d\n",
			   __func__, vdin_path);
			vops->stop_tvin_service(vdin_path);
		}
	}
	videobuf_mmap_free(&fh->vb_vidq);
	kfree(fh);

	mutex_lock(&dev->mutex);
	dev->users--;
	mutex_unlock(&dev->mutex);

	dprintk(dev, 1, "close called (dev=%s, users=%d)\n",
		video_device_node_name(vdev), dev->users);
#if 1
	GC2145_h_active = 640; /* 800//zyy */
	GC2145_v_active = 480; /* 1200;//600 */
	gc2145_qctrl[0].default_value = 0;
	gc2145_qctrl[1].default_value = 4;
	gc2145_qctrl[2].default_value = 0;
	gc2145_qctrl[3].default_value = 0;
	gc2145_qctrl[4].default_value = 0;

	gc2145_qctrl[5].default_value = 0;
	gc2145_qctrl[7].default_value = 100;
	gc2145_qctrl[8].default_value = 0;
	gc2145_frmintervals_active.numerator = 1;
	gc2145_frmintervals_active.denominator = 15;
	power_down_gc2145(dev);
#endif
	aml_cam_uninit(&dev->cam_info);
#ifdef CONFIG_HAS_WAKELOCK
	wake_unlock(&(dev->wake_lock));
#endif
#ifdef CONFIG_CMA
	vm_deinit_resource(&dev->vminfo);
#endif
	return 0;
}

static int gc2145_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct gc2145_fh  *fh = file->private_data;
	struct gc2145_device *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end - (unsigned long)vma->vm_start,
		ret);

	return ret;
}

static const struct v4l2_file_operations gc2145_fops = {
	.owner      = THIS_MODULE,
	.open           = gc2145_open,
	.release        = gc2145_close,
	.read           = gc2145_read,
	.poll       = gc2145_poll,
	.unlocked_ioctl          = video_ioctl2, /* V4L2 ioctl handler */
	.mmap           = gc2145_mmap,
};

static const struct v4l2_ioctl_ops gc2145_ioctl_ops = {
	.vidioc_querycap      = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap  = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap     = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap   = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     = vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs       = vidioc_reqbufs,
	.vidioc_querybuf      = vidioc_querybuf,
	.vidioc_qbuf          = vidioc_qbuf,
	.vidioc_dqbuf         = vidioc_dqbuf,
	.vidioc_s_std         = vidioc_s_std,
	.vidioc_enum_input    = vidioc_enum_input,
	.vidioc_g_input       = vidioc_g_input,
	.vidioc_s_input       = vidioc_s_input,
	.vidioc_queryctrl     = vidioc_queryctrl,
	.vidioc_querymenu     = vidioc_querymenu,
	.vidioc_g_ctrl        = vidioc_g_ctrl,
	.vidioc_s_ctrl        = vidioc_s_ctrl,
	.vidioc_streamon      = vidioc_streamon,
	.vidioc_streamoff     = vidioc_streamoff,
	.vidioc_enum_framesizes = vidioc_enum_framesizes,
	.vidioc_g_parm = vidioc_g_parm,
	.vidioc_enum_frameintervals = vidioc_enum_frameintervals,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf          = vidiocgmbuf,
#endif
};

static struct video_device gc2145_template = {
	.name       = "gc2145_v4l",
	.fops           = &gc2145_fops,
	.ioctl_ops  = &gc2145_ioctl_ops,
	.release    = video_device_release,
	.tvnorms    = V4L2_STD_525_60,
};

static const struct v4l2_subdev_core_ops gc2145_core_ops = {
	/*.g_chip_ident = gc2145_g_chip_ident,*/
};

static const struct v4l2_subdev_ops gc2145_ops = {
	.core = &gc2145_core_ops,
};

static ssize_t gc2145_show(struct device *dev, struct device_attribute *attr,
			   char *_buf)
{
	return sprintf(_buf, "0x%02x=0x%02x\n", cur_reg, cur_val);
}

static u32 strtol(const char *nptr, int base)
{
	u32 ret;

	if (!nptr || (base != 16 && base != 10 && base != 8)) {
		pr_err("%s(): NULL pointer input\n", __func__);
		return -1;
	}
	for (ret = 0; *nptr; nptr++) {
		if ((base == 16 && *nptr >= 'A' && *nptr <= 'F') ||
		    (base == 16 && *nptr >= 'a' && *nptr <= 'f') ||
		    (base >= 10 && *nptr >= '0' && *nptr <= '9') ||
		    (base >= 8  && *nptr >= '0' && *nptr <= '7')) {
			ret *= base;
			if (base == 16 && *nptr >= 'A' && *nptr <= 'F')
				ret += *nptr - 'A' + 10;
			else if (base == 16 && *nptr >= 'a' && *nptr <= 'f')
				ret += *nptr - 'a' + 10;
			else if (base >= 10 && *nptr >= '0' && *nptr <= '9')
				ret += *nptr - '0';
			else if (base >= 8 && *nptr >= '0' && *nptr <= '7')
				ret += *nptr - '0';
		} else
			return ret;
	}
	return ret;
}

static ssize_t gc2145_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *_buf, size_t _count)
{
	const char *p = _buf;
	u16 reg;
	u8 val;

	if (!strncmp(_buf, "get", strlen("get"))) {
		p += strlen("get");
		cur_reg = (u32)strtol(p, 16);
		val = i2c_get_byte_add8(g_i2c_client, cur_reg);
		pr_info("%s(): get 0x%04x=0x%02x\n",
				__func__, cur_reg, val);
		cur_val = val;
	} else if (!strncmp(_buf, "put", strlen("put"))) {
		p += strlen("put");
		reg = strtol(p, 16);
		p = strchr(_buf, '=');
		if (p) {
			++p;
			val = strtol(p, 16);
			i2c_put_byte_add8_new(g_i2c_client, reg, val);
			pr_info("%s(): set 0x%04x=0x%02x\n",
				__func__, reg, val);
		} else
			pr_info("%s(): Bad string format input!\n", __func__);
	} else
		pr_info("%s(): Bad string format input!\n", __func__);

	return _count;
}

static ssize_t name_show(struct device *dev,
						struct device_attribute *attr,
						char *_buf)
{
	strcpy(_buf, "GC2145");
	return 4;
}

static struct device *gc2145_dev;
static struct class   *gc2145_class;
static DEVICE_ATTR(gc2145, 0444, gc2145_show, gc2145_store);/*0666*/
static DEVICE_ATTR(name, 0444, name_show, NULL);

#define  EMDOOR_DEBUG_GC2145    1
#ifdef EMDOOR_DEBUG_GC2145
unsigned int gc2145_reg_addr;
static struct i2c_client *gc2145_client;

static ssize_t gc2145_show_mine(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	unsigned  char dat;

	dat = i2c_get_byte_add8(gc2145_client, gc2145_reg_addr);
	return sprintf(buf, "REG[0x%x]=0x%x\n", gc2145_reg_addr, dat);
}

static ssize_t gc2145_store_mine(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	int tmp;
	unsigned short reg;
	unsigned char val;

	tmp = kstrtoul(buf, 16, NULL);
	/* sscanf(buf, "%du", &tmp); */
	if (tmp < 0xff)
		gc2145_reg_addr = tmp;
	else {
		reg = (tmp >> 8) & 0xFFFF; /* reg */
		gc2145_reg_addr = reg;
		val = tmp & 0xFF;        /* val */
		i2c_put_byte_add8_new(gc2145_client, reg, val);
	}

	return count;
}


static struct kobj_attribute gc2145_attribute = __ATTR(gc2145, 0444,
		gc2145_show_mine, gc2145_store_mine);


static struct attribute *gc2145_attrs[] = {
	&gc2145_attribute.attr,
	NULL,
};


static const struct attribute_group gc2145_group = {
	.attrs = gc2145_attrs,
};
#endif

static int gc2145_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	struct gc2145_device *t;
	struct v4l2_subdev *sd;
	struct aml_cam_info_s *plat_dat;
	int ret;

	vops = get_vdin_v4l2_ops();
	v4l_info(client, "chip found @ 0x%x (%s)\n",
		client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	/* modify correct i2c addr--- 0x30 */
	client->addr = 0x3c;
	if (t == NULL) {
		pr_err("%s, no memory", __func__);
		return -ENOMEM;
	}
	snprintf(t->v4l2_dev.name, sizeof(t->v4l2_dev.name),
		"%s-%03d", "gc2145", 0);
	ret = v4l2_device_register(NULL, &t->v4l2_dev);
	if (ret) {
		pr_info("%s, v4l2 device register failed", __func__);
		kfree(t);
		kfree(client);
		return ret;
	}

	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &gc2145_ops);

	plat_dat = (struct aml_cam_info_s *)client->dev.platform_data;
	/* test if devices exist. */
	memset(&t->vminfo, 0, sizeof(struct vm_init_s));
	/* Now create a video4linux device */
	mutex_init(&t->mutex);

	/* Now create a video4linux device */
	t->vdev = video_device_alloc();
	if (t->vdev == NULL) {
		kfree(t);
		kfree(client);
		return -ENOMEM;
	}
	memcpy(t->vdev, &gc2145_template, sizeof(*t->vdev));
	t->vdev->v4l2_dev = &t->v4l2_dev;
	video_set_drvdata(t->vdev, t);

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&(t->wake_lock), WAKE_LOCK_SUSPEND, "gc2145");
#endif

	/* Register it */
	if (plat_dat) {
		memcpy(&t->cam_info, plat_dat, sizeof(struct aml_cam_info_s));
		pr_info("%s, front_back = %d\n",
		     __func__, plat_dat->front_back);
		video_nr = plat_dat->front_back;
	} else {
		pr_err("camera gc2145: have no platform data\n");
		kfree(t);
		return -1;
	}

	t->cam_info.version = GC2145_DRIVER_VERSION;
	if (aml_cam_info_reg(&t->cam_info) < 0)
		pr_err("reg caminfo error\n");
	err = video_register_device(t->vdev, VFL_TYPE_GRABBER, video_nr);
	if (err < 0) {
		video_device_release(t->vdev);
		kfree(t);
		return err;
	}

	gc2145_class = class_create(THIS_MODULE, "gc2145");
	if (IS_ERR(gc2145_class)) {
		pr_err("Create class gc2145 fail.\n");
		return -ENOMEM;
	}
	gc2145_dev = device_create(gc2145_class, NULL,
		MKDEV(0, 1), NULL, "dev");
	device_create_file(gc2145_dev, &dev_attr_gc2145);
	device_create_file(gc2145_dev, &dev_attr_name);

	g_i2c_client = client;

#ifdef EMDOOR_DEBUG_GC2145
	gc2145_client = client;
	err = sysfs_create_group(&client->dev.kobj, &gc2145_group);
#endif

	return 0;
}

static int gc2145_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2145_device *t = to_dev(sd);

	video_unregister_device(t->vdev);
	v4l2_device_unregister_subdev(sd);
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&(t->wake_lock));
#endif
	aml_cam_info_unreg(&t->cam_info);
	kfree(t);
	return 0;
}


static const struct i2c_device_id gc2145_id[] = {
	{ "gc2145", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc2145_id);

static struct i2c_driver gc2145_i2c_driver = {
	.driver = {
		.name = "gc2145",
	},
	.probe = gc2145_probe,
	.remove = gc2145_remove,
	.id_table = gc2145_id,
};

module_i2c_driver(gc2145_i2c_driver);

