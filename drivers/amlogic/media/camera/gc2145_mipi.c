/*
 * drivers/amlogic/media/camera/gc2145_mipi.c
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

#define GC2145_CAMERA_MODULE_NAME   "gc2145_mipi"

#define WAKE_NUMERATOR              30
#define WAKE_DENOMINATOR            1001
#define BUFFER_TIMEOUT              msecs_to_jiffies(500)

#define MAGIC_RE_MEM                0x123039dc
#define GC2145_RES0_CANVAS_INDEX    0x4e

#define GC2145_CAMERA_MAJOR_VERSION 0
#define GC2145_CAMERA_MINOR_VERSION 7
#define GC2145_CAMERA_RELEASE       0
#define GC2145_CAMERA_VERSION                       \
	KERNEL_VERSION(GC2145_CAMERA_MAJOR_VERSION, \
	GC2145_CAMERA_MINOR_VERSION,                \
	GC2145_CAMERA_RELEASE)

MODULE_DESCRIPTION("gc2145 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");

#define GC2145_DRIVER_VERSION "GC2145-COMMON-01-140717"

static unsigned int video_nr = -1;

static unsigned int debug;

static unsigned int vid_limit = 32;

static int gc2145_have_opened;

static struct vdin_v4l2_ops_s *vops;
static bool bDoingAutoFocusMode;
static int temp_frame = -1;

enum flip_type {
	NORMAL = 0,
	H_MIRROR,
	V_MIRROR,
	HV_MIRROR,
};

static struct v4l2_fract gc2145_frmintervals_active = {
	.numerator = 1,
	.denominator = 15,
};

static struct v4l2_frmivalenum gc2145_frmivalenum[] = {
	{
		.index        = 0,
		.pixel_format = V4L2_PIX_FMT_NV21,
		.width        = 352,
		.height       = 288,
		.type         = V4L2_FRMIVAL_TYPE_DISCRETE,
		{
			.discrete = {
				.numerator   = 1,
				.denominator = 30,
			}
		}
	}, {
		.index        = 0,
		.pixel_format = V4L2_PIX_FMT_NV21,
		.width        = 640,
		.height       = 480,
		.type         = V4L2_FRMIVAL_TYPE_DISCRETE,
		{
			.discrete = {
				.numerator   = 1,
				.denominator = 30,
			}
		}
	}, {
		.index        = 0,
		.pixel_format = V4L2_PIX_FMT_NV21,
		.width        = 1024,
		.height       = 768,
		.type         = V4L2_FRMIVAL_TYPE_DISCRETE,
		{
			.discrete = {
				.numerator   = 1,
				.denominator = 30,
			}
		}
	}, {
		.index        = 0,
		.pixel_format = V4L2_PIX_FMT_NV21,
		.width        = 1280,
		.height       = 720,
		.type         = V4L2_FRMIVAL_TYPE_DISCRETE,
		{
			.discrete = {
				.numerator   = 1,
				.denominator = 30,
			}
		}
	}, {
		.index        = 0,
		.pixel_format = V4L2_PIX_FMT_NV21,
		.width        = 1920,
		.height       = 1080,
		.type         = V4L2_FRMIVAL_TYPE_DISCRETE,
		{
			.discrete = {
				.numerator   = 1,
				.denominator = 30,
			}
		}
	}, {
		.index        = 1,
		.pixel_format = V4L2_PIX_FMT_NV21,
		.width        = 1600,
		.height       = 1200,
		.type         = V4L2_FRMIVAL_TYPE_DISCRETE,
		{
			.discrete = {
				.numerator   = 1,
				.denominator = 20,
			}
		}
	}, {
		.index        = 1,
		.pixel_format = V4L2_PIX_FMT_NV21,
		.width        = 2048,
		.height       = 1536,
		.type         = V4L2_FRMIVAL_TYPE_DISCRETE,
		{
			.discrete = {
				.numerator   = 1,
				.denominator = 5,
			}
		}
	}, {
		.index        = 1,
		.pixel_format = V4L2_PIX_FMT_NV21,
		.width        = 2592,
		.height       = 1944,
		.type         = V4L2_FRMIVAL_TYPE_DISCRETE,
		{
			.discrete = {
				.numerator   = 1,
				.denominator = 5,
			}
		}
	},
};

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

struct v4l2_querymenu gc2145_qmenu_autofocus[] = {
	{
		.id         = V4L2_CID_FOCUS_AUTO,
		.index      = CAM_FOCUS_MODE_INFINITY,
		.name       = "infinity",
		.reserved   = 0,
	}, {
		.id         = V4L2_CID_FOCUS_AUTO,
		.index      = CAM_FOCUS_MODE_AUTO,
		.name       = "auto",
		.reserved   = 0,
	}, {
		.id         = V4L2_CID_FOCUS_AUTO,
		.index      = CAM_FOCUS_MODE_CONTI_VID,
		.name       = "continuous-video",
		.reserved   = 0,
	}, {
		.id         = V4L2_CID_FOCUS_AUTO,
		.index      = CAM_FOCUS_MODE_CONTI_PIC,
		.name       = "continuous-picture",
		.reserved   = 0,
	}
};

struct v4l2_querymenu gc2145_qmenu_flashmode[] = {
	{
		.id         = V4L2_CID_BACKLIGHT_COMPENSATION,
		.index      = FLASHLIGHT_ON,
		.name       = "on",
		.reserved   = 0,
	}, {
		.id         = V4L2_CID_BACKLIGHT_COMPENSATION,
		.index      = FLASHLIGHT_OFF,
		.name       = "off",
		.reserved   = 0,
	}, {
		.id         = V4L2_CID_BACKLIGHT_COMPENSATION,
		.index      = FLASHLIGHT_TORCH,
		.name       = "torch",
		.reserved   = 0,
	}
};

static struct v4l2_querymenu gc2145_qmenu_wbmode[] = {
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
		.index      = CAM_WB_FLUORESCENT,
		.name       = "warm-fluorescent",
		.reserved   = 0,
	},
};

struct gc2145_qmenu_s {
	__u32   id;
	int     num;
	struct v4l2_querymenu *gc2145_qmenu;
};

static struct gc2145_qmenu_s gc2145_qmenu_set[] = {
	{
		.id             = V4L2_CID_FOCUS_AUTO,
		.num            = ARRAY_SIZE(gc2145_qmenu_autofocus),
		.gc2145_qmenu   = gc2145_qmenu_autofocus,
	}, {
		.id             = V4L2_CID_BACKLIGHT_COMPENSATION,
		.num            = ARRAY_SIZE(gc2145_qmenu_flashmode),
		.gc2145_qmenu   = gc2145_qmenu_flashmode,
	}, {
		.id             = V4L2_CID_DO_WHITE_BALANCE,
		.num            = ARRAY_SIZE(gc2145_qmenu_wbmode),
		.gc2145_qmenu   = gc2145_qmenu_wbmode,
	}
};

#define dprintk(dev, level, fmt, arg...) \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)


struct gc2145_fmt {
	char  *name;
	u32   fourcc;
	int   depth;
};

static struct gc2145_fmt formats[] = {
	{
		.name     = "RGB565 (BE)",
		.fourcc   = V4L2_PIX_FMT_RGB565X,
		.depth    = 16,
	}, {
		.name     = "RGB888 (24)",
		.fourcc   = V4L2_PIX_FMT_RGB24,
		.depth    = 24,
	}, {
		.name     = "BGR888 (24)",
		.fourcc   = V4L2_PIX_FMT_BGR24,
		.depth    = 24,
	}, {
		.name     = "12  Y/CbCr 4:2:0SP",
		.fourcc   = V4L2_PIX_FMT_NV12,
		.depth    = 12,
	}, {
		.name     = "12  Y/CbCr 4:2:0SP",
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


struct gc2145_buffer {
	struct videobuf_buffer vb;
	struct gc2145_fmt        *fmt;
	unsigned int canvas_id;
};

struct gc2145_dmaqueue {
	struct list_head      active;
	struct task_struct    *kthread;
	wait_queue_head_t     wq;
	int                   frame;
	int                   ini_jiffies;
};

struct resolution_param {
	struct v4l2_frmsize_discrete frmsize;
	struct v4l2_frmsize_discrete active_frmsize;
	int active_fps;
	int lanes;
	int bps;
	enum resolution_size size_type;
	struct aml_camera_i2c_fig_s *reg_script;
};

static LIST_HEAD(gc2145_devicelist);

struct gc2145_device {
	struct list_head        gc2145_devicelist;
	struct v4l2_subdev      sd;
	struct v4l2_device      v4l2_dev;

	spinlock_t              slock;
	struct mutex            mutex;

	int                     users;

	struct video_device     *vdev;

	struct gc2145_dmaqueue  vidq;

	unsigned long           jiffies;

	int                     input;

	struct aml_cam_info_s   cam_info;

	int                     qctl_regs[ARRAY_SIZE(gc2145_qctrl)];

	struct resolution_param *cur_resolution_param;

#ifdef CONFIG_HAS_WAKELOCK
	struct wake_lock        wake_lock;
#endif

	struct work_struct      dl_work;

	int                     firmware_ready;

	struct vm_init_s        vminfo;
};

static DEFINE_MUTEX(firmware_mutex);

static inline struct gc2145_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc2145_device, sd);
}

struct gc2145_fh {
	struct gc2145_device         *dev;

	struct gc2145_fmt            *fmt;
	unsigned int                 width, height;
	struct videobuf_queue        vb_vidq;

	struct videobuf_res_privdata res;

	enum v4l2_buf_type           type;
	int                          input;
	int                          stream_on;
	unsigned int                 f_flags;
};

static struct aml_camera_i2c_fig_s GC2145_script[] = {
	{0xfe, 0xf0},
	{0xfe, 0xf0},
	{0xfe, 0xf0},
	{0xfc, 0x06},
	{0xf6, 0x00},
	{0xf7, 0x1d},
	{0xf8, 0x84},
	{0xfa, 0x00},
	{0xf9, 0x8e},
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
	{0x84, 0x00},
	{0x86, 0x02},
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
	/*GAMMA*/
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
	{0xd1, 0x28},/*0x32-->0x28*/
	{0xd2, 0x28},/*0x32-->0x28*/
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
	/*{0x4c, 0x01},*/
	/*{0x4d, 0x89},*/
	/*{0x4e, 0x04},*/
	/*{0x4c, 0x01},*/
	/*{0x4d, 0xa9},*/
	/*{0x4e, 0x04},*/
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
	/*{0x4c, 0x02},*/
	/*{0x4d, 0x6a},*/
	/*{0x4e, 0x06},*/
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
	{0xc4, 0xf0},
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
	{0xf2, 0x00},
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
	/*dark sun*/
	{0x18, 0x22},
	{0xfe, 0x02},
	{0x40, 0xbf},
	{0x46, 0xcf},
	{0xfe, 0x00},
	/*MIPI*/
	{0xfe, 0x03},
	{0x02, 0x22},
	{0x03, 0x10}, /*0x12*/
	{0x04, 0x10}, /*0x01*/
	{0x05, 0x00},
	{0x06, 0x88},
	{0x01, 0x87},
	/*{0x10, 0x85},*/
	{0x11, 0x1e},
	{0x12, 0x80},
	{0x13, 0x0c},
	{0x15, 0x12},
	{0x17, 0xf0},
	{0x21, 0x10},
	{0x22, 0x04},
	{0x23, 0x10},
	{0x24, 0x10},
	{0x25, 0x10},
	{0x26, 0x05},
	{0x29, 0x03},
	{0x2a, 0x0a},
	{0x2b, 0x06},
	{0xfe, 0x00},
	/*banding--fix 20fps*/
	{0x05, 0x01},/*hb*/
	{0x06, 0x56},
	{0x07, 0x00},/*vb*/
	{0x08, 0x32},
	{0xfe, 0x01},
	{0x25, 0x00},/*step*/
	{0x26, 0xfa},
	{0x27, 0x04},/*level1*/
	{0x28, 0xe2},
	{0x29, 0x04},/*level2*/
	{0x2a, 0xe2},
	{0x2b, 0x04},/*level3*/
	{0x2c, 0xe2},
	{0x2d, 0x04},/*level4*/
	{0x2e, 0xe2},
	{0xfe, 0x00},
	{0xff, 0xff}
};

#if 0
static struct aml_camera_i2c_fig_s GC2145_preview_CIF_script[] = {
	{0xfe, 0x00},
	{0xfd, 0x01},
	{0xfa, 0x00},
	/*crop window*/
	{0xfe, 0x00},
	{0x99, 0x22},
	{0x9a, 0x06},
	{0x9b, 0x00},
	{0x9c, 0x00},
	{0x9d, 0x00},
	{0x9e, 0x00},
	{0x9f, 0x00},
	{0xa0, 0x00},
	{0xa1, 0x00},
	{0xa2, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x06},
	{0x93, 0x00},
	{0x94, 0x18},
	{0x95, 0x01},
	{0x96, 0x20},
	{0x97, 0x01},
	{0x98, 0x60},
	/*AWB*/
	{0xfe, 0x00},
	{0xec, 0x02},
	{0xed, 0x02},
	{0xee, 0x30},
	{0xef, 0x48},
	{0xfe, 0x02},
	{0x9d, 0x0b},
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
	/*mipi*/
	{0xfe, 0x03},
	{0x12, 0xc0},
	{0x13, 0x02},
	{0x04, 0x90},
	{0x05, 0x01},
	{0x10, 0x95},
	{0xfe, 0x00},
	{0xff, 0xff}
};
#endif

static struct aml_camera_i2c_fig_s GC2145_preview_720P_script[] = {
	{0xfe, 0x00},
	{0xfd, 0x00},
	{0xfa, 0x00},
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
	{0x92, 0x02},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x02},
	{0x96, 0xd0},
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
	{0x21, 0x04},
	{0xfe, 0x00},
	{0x20, 0x03},
	/*mipi*/
	{0xfe, 0x03},
	{0x12, 0x00},
	{0x13, 0x0a},
	{0x04, 0x40},
	{0x05, 0x01},
	{0x10, 0x95},/*output enable*/
	{0xfe, 0x00},
	/*adjust*/
	{0xfe, 0x02},
	{0x97, 0x32},
	{0xfe, 0x00},
	{0xff, 0xff}
};

static struct aml_camera_i2c_fig_s GC2145_preview_VGA_script[] = {
	{0xfe, 0x00},
	{0xfd, 0x00},
	{0xfa, 0x00},
	/*crop window*/
	{0xfe, 0x00},
	{0x99, 0x55},
	{0x9a, 0x06},
	{0x9b, 0x02},
	{0x9c, 0x00},
	{0x9d, 0x00},
	{0x9e, 0x00},
	{0x9f, 0x02},
	{0xa0, 0x00},
	{0xa1, 0x00},
	{0xa2, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x01},
	{0x96, 0xe0},
	{0x97, 0x02},
	{0x98, 0x80},
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
	{0x21, 0x04},
	{0xfe, 0x00},
	{0x20, 0x03},
	/*mipi*/
	{0xfe, 0x03},
	{0x12, 0x00},
	{0x13, 0x05},
	{0x04, 0x80},
	{0x05, 0x01},
	{0x10, 0x95},/*output enable*/
	{0xfe, 0x00},
	/*adjust*/
	{0xfe, 0x02},
	{0x97, 0x11},
	{0xfe, 0x00},
	{0xff, 0xff}
};

static struct aml_camera_i2c_fig_s GC2145_capture_SVGA_script[] = {
	{0xfe, 0x00},
	{0xfd, 0x01},
	{0xfa, 0x00},
	/*crop window*/
	{0xfe, 0x00},
	{0x99, 0x11},
	{0x9a, 0x06},
	{0x9b, 0x00},
	{0x9c, 0x00},
	{0x9d, 0x00},
	{0x9e, 0x00},
	{0x9f, 0x00},
	{0xa0, 0x00},
	{0xa1, 0x00},
	{0xa2, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x02},
	{0x96, 0x58},
	{0x97, 0x03},
	{0x98, 0x20},
	/*AWB*/
	{0xfe, 0x00},
	{0xec, 0x02},
	{0xed, 0x02},
	{0xee, 0x30},
	{0xef, 0x48},
	{0xfe, 0x02},
	{0x9d, 0x0b},
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
	/*mipi*/
	{0xfe, 0x03},
	{0x12, 0x40},
	{0x13, 0x06},
	{0x04, 0x90},
	{0x05, 0x01},
	{0xfe, 0x00},
	{0xff, 0xff}
};

static struct aml_camera_i2c_fig_s GC2145_capture_2M_script[] = {
	{0xfe, 0x00},
	{0xfd, 0x00},
	{0xfa, 0x00},
	/*crop window*/
	{0xfe, 0x00},
	{0x99, 0x11},
	{0x9a, 0x06},
	{0x9b, 0x00},
	{0x9c, 0x00},
	{0x9d, 0x00},
	{0x9e, 0x00},
	{0x9f, 0x00},
	{0xa0, 0x00},
	{0xa1, 0x00},
	{0xa2, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x04},
	{0x96, 0xb0},
	{0x97, 0x06},
	{0x98, 0x40},
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
	{0x21, 0x04},
	{0xfe, 0x00},
	{0x20, 0x03},
	/*mipi*/
	{0xfe, 0x03},
	{0x12, 0x80},
	{0x13, 0x0c},
	{0x04, 0x01},
	{0x05, 0x00},
	{0x10, 0x95}, /*output enable*/
	{0xfe, 0x00},
	{0xff, 0xff}
};

/*mipi lane bps : M*/
static struct resolution_param  prev_resolution_array[] = {
	{
		.frmsize            = {640, 480},
		.active_frmsize     = {640, 480},
		.active_fps         = 30,
		.lanes              = 2,
		.bps                = 480,
		.size_type          = SIZE_640X480,
		.reg_script         = GC2145_preview_VGA_script,
	}, {
		.frmsize            = {1280, 720},
		.active_frmsize     = {1280, 720},
		.active_fps         = 30,
		.lanes              = 2,
		.bps                = 480,
		.size_type          = SIZE_1280X720,
		.reg_script         = GC2145_preview_720P_script,
	},
};

static struct resolution_param  capture_resolution_array[] = {
	{
		.frmsize            = {640, 480},
		.active_frmsize     = {640, 480},
		.active_fps         = 20,
		.lanes              = 2,
		.bps                = 480,
		.size_type          = SIZE_640X480,
		.reg_script         = GC2145_preview_VGA_script,
	}, {
		.frmsize            = {800, 600},
		.active_frmsize     = {800, 600},
		.active_fps         = 20,
		.lanes              = 2,
		.bps                = 480,
		.size_type          = SIZE_800X600,
		.reg_script         = GC2145_capture_SVGA_script,
	}, {
		.frmsize            = {1280, 720},
		.active_frmsize     = {1280, 720},
		.active_fps         = 30,
		.lanes              = 2,
		.bps                = 480,
		.size_type          = SIZE_1280X720,
		.reg_script         = GC2145_preview_720P_script,
	}, {
		.frmsize            = {1600, 1200},
		.active_frmsize     = {1600, 1200},
		.active_fps         = 20,
		.lanes              = 2,
		.bps                = 480,
		.size_type          = SIZE_1600X1200,
		.reg_script         = GC2145_capture_2M_script,
	},
};

static void GC2145_init_regs(struct gc2145_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int i = 0;

	while (1) {
		if (GC2145_script[i].val == 0xff &&
			GC2145_script[i].addr == 0xff) {
			pr_info("success in initial GC2145\n");
			break;
		}
		if ((i2c_put_byte_add8_new(client, GC2145_script[i].addr,
			GC2145_script[i].val)) < 0) {
			pr_info("fail in initial GC2145\n");
			return;
		}
		i++;
	}

}


static void GC2145_set_param_wb(struct gc2145_device *dev,
		enum camera_wb_flip_e para)
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
		i2c_put_byte_add8_new(client, 0xb3, 0x58);
		i2c_put_byte_add8_new(client, 0xb4, 0x40);
		i2c_put_byte_add8_new(client, 0xb5, 0x50);
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
}

static void GC2145_set_param_exposure(struct gc2145_device *dev,
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
	}
}

static void GC2145_set_param_effect(struct gc2145_device *dev,
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
}

void GC2145_set_night_mode(struct gc2145_device *dev,
			   enum  camera_night_mode_flip_e enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	if (enable) {
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x3c, 0x60);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
	} else {
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x3c, 0x00);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
	}
}

static void GC2145_set_param_banding(struct gc2145_device *dev,
		enum  camera_banding_flip_e banding)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	switch (banding) {
	case CAM_BANDING_50HZ:
		i2c_put_byte_add8_new(client, 0x05, 0x01);/*hb*/
		i2c_put_byte_add8_new(client, 0x06, 0x56);
		i2c_put_byte_add8_new(client, 0x07, 0x00);/*vb*/
		i2c_put_byte_add8_new(client, 0x08, 0x32);
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x25, 0x00);/*step*/
		i2c_put_byte_add8_new(client, 0x26, 0xfa);
		i2c_put_byte_add8_new(client, 0x27, 0x04);/*level1*/
		i2c_put_byte_add8_new(client, 0x28, 0xe2);
		i2c_put_byte_add8_new(client, 0x29, 0x04);/*level2*/
		i2c_put_byte_add8_new(client, 0x2a, 0xe2);
		i2c_put_byte_add8_new(client, 0x2b, 0x04);/*level3*/
		i2c_put_byte_add8_new(client, 0x2c, 0xe2);
		i2c_put_byte_add8_new(client, 0x2d, 0x09);/*level4*/
		i2c_put_byte_add8_new(client, 0x2e, 0xc4);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case CAM_BANDING_60HZ:
		i2c_put_byte_add8_new(client, 0x05, 0x01);/*hb*/
		i2c_put_byte_add8_new(client, 0x06, 0x58);
		i2c_put_byte_add8_new(client, 0x07, 0x00);/*vb*/
		i2c_put_byte_add8_new(client, 0x08, 0x32);
		i2c_put_byte_add8_new(client, 0xfe, 0x01);
		i2c_put_byte_add8_new(client, 0x25, 0x00);/*step*/
		i2c_put_byte_add8_new(client, 0x26, 0xd0);
		i2c_put_byte_add8_new(client, 0x27, 0x04);/*level1*/
		i2c_put_byte_add8_new(client, 0x28, 0xe0);
		i2c_put_byte_add8_new(client, 0x29, 0x04);/*level2*/
		i2c_put_byte_add8_new(client, 0x2a, 0xe0);
		i2c_put_byte_add8_new(client, 0x2b, 0x04);/*level3*/
		i2c_put_byte_add8_new(client, 0x2c, 0xe0);
		i2c_put_byte_add8_new(client, 0x2d, 0x08);/*level4*/
		i2c_put_byte_add8_new(client, 0x2e, 0xf0);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	default:
		break;
	}
}

static enum resolution_size get_size_type(int width, int height)
{
	enum resolution_size rv = SIZE_NULL;

	if (width * height >= 2500 * 1900)
		rv = SIZE_2592X1944;
	else if (width * height >= 2000 * 1500)
		rv = SIZE_2048X1536;
	else if (width * height >= 1920 * 1080)
		rv = SIZE_1920X1080;
	else if (width * height >= 1600 * 1200)
		rv = SIZE_1600X1200;
	else if (width * height >= 1280 * 960)
		rv = SIZE_1280X960;
	else if (width * height >= 1280 * 720)
		rv = SIZE_1280X720;
	else if (width * height >= 1024 * 768)
		rv = SIZE_1024X768;
	else if (width * height >= 800 * 600)
		rv = SIZE_800X600;
	else if (width * height >= 600 * 400)
		rv = SIZE_640X480;
	else if (width * height >= 352 * 288)
		rv = SIZE_352X288;
	return rv;
}

static int set_flip(struct gc2145_device *dev, enum flip_type type)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	i2c_put_byte_add8_new(client, 0xfe, 0x00);

	switch (type) {
	case NORMAL:
		i2c_put_byte_add8_new(client, 0x17, 0x14);
		break;
	case H_MIRROR:
		i2c_put_byte_add8_new(client, 0x17, 0x15);
		break;
	case V_MIRROR:
		i2c_put_byte_add8_new(client, 0x15, 0x16);
		break;
	case HV_MIRROR:
		i2c_put_byte_add8_new(client, 0x17, 0x17);
		break;
	default:
		i2c_put_byte_add8_new(client, 0x17, 0x14);
		break;
	}
	return 0;
}

static struct resolution_param *get_resolution_param(
		struct gc2145_device *dev,
		int is_capture, int width, int height)
{
	int i = 0;
	int arry_size = 0;
	struct resolution_param *tmp_resolution_param = NULL;
	enum resolution_size res_type = SIZE_NULL;

	res_type = get_size_type(width, height);
	if (res_type == SIZE_NULL)
		return NULL;
	if (is_capture) {
		tmp_resolution_param = capture_resolution_array;
		arry_size = sizeof(capture_resolution_array)/
			sizeof(capture_resolution_array[0]);
	} else {
		tmp_resolution_param = prev_resolution_array;
		arry_size = sizeof(prev_resolution_array)/
			sizeof(prev_resolution_array[0]);
		gc2145_frmintervals_active.denominator = 20;
		gc2145_frmintervals_active.numerator = 1;
	}

	for (i = 0; i < arry_size; i++) {
		if (tmp_resolution_param[i].size_type == res_type) {
			gc2145_frmintervals_active.denominator =
				tmp_resolution_param[i].active_fps;
			gc2145_frmintervals_active.numerator = 1;
			return &tmp_resolution_param[i];
		}
	}
	return NULL;
}

static int set_resolution_param(struct gc2145_device *dev,
		struct resolution_param *res_param)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int i = 0;

	if (!res_param->reg_script) {
		pr_info("error, resolution reg script is NULL\n");
		return -1;
	}

	pr_info("res_para->size_type = %d, res_param->frmsize.width = %d, height = %d\n",
		res_param->size_type, res_param->frmsize.width,
		res_param->frmsize.height);

	while (1) {
		if (res_param->reg_script[i].val == 0xff &&
			res_param->reg_script[i].addr == 0xff) {
			pr_info("setting resolutin param complete\n");
			break;
		}
		if ((i2c_put_byte_add8_new(client,
			res_param->reg_script[i].addr,
			res_param->reg_script[i].val)) < 0) {
			pr_info("fail in setting resolution param.i=%d\n", i);
			break;
		}
		i++;
	}
	dev->cur_resolution_param = res_param;
	set_flip(dev, H_MIRROR);

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
			canvas =
			start_canvas |
			((start_canvas + 1) << 8) |
			((start_canvas + 2) << 16);
		else
			canvas =
			start_canvas |
			((start_canvas + 2) << 8) |
			((start_canvas + 1) << 16);
		break;
	default:
		break;
	}
	return canvas;
}

static int brightness_gc2145(struct i2c_client *client, int value)
{
	enum ev_step val = (enum ev_step)value;

	switch (val) {
	case EV_COMP_n30:/*-3*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd5, 0xd0);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_n20:/*-2*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd5, 0xe0);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_n10:/*-1*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd5, 0xf0);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_00: /*zero*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd5, 0x00);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_10:/*+1*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd5, 0x10);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_20:/*+2*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd5, 0x20);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_30:/*+3*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd5, 0x30);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	default:
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd5, 0x00);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	}
	return 0;
}

static int contrast_gc2145(struct i2c_client *client, int value)
{
	enum ev_step val = (enum ev_step)value;

	switch (val) {
	case EV_COMP_n30:/*-3*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd3, 0x28);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_n20:/*-2*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd3, 0x30);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_n10:/*-1*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd3, 0x38);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_00: /*zero*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd3, 0x40);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_10:/*+1*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd3, 0x48);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_20:/*+2*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd3, 0x50);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_30:/*+3*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd3, 0x58);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	default:
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd3, 0x40);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	}
	return 0;
}

static int saturation_gc2145(struct i2c_client *client, int value)
{
	enum ev_step val = (enum ev_step)value;

	switch (val) {
	case EV_COMP_n30:/*-3*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd1, 0x18);
		i2c_put_byte_add8_new(client, 0xd2, 0x18);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_n20:/*-2*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd1, 0x20);
		i2c_put_byte_add8_new(client, 0xd2, 0x20);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_n10:/*-1*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd1, 0x28);
		i2c_put_byte_add8_new(client, 0xd2, 0x28);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_00: /*zero*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd1, 0x32);
		i2c_put_byte_add8_new(client, 0xd2, 0x32);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_10:/*+1*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd1, 0x38);
		i2c_put_byte_add8_new(client, 0xd2, 0x38);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_20:/*+2*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd1, 0x40);
		i2c_put_byte_add8_new(client, 0xd2, 0x40);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	case EV_COMP_30:/*+3*/
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd1, 0x48);
		i2c_put_byte_add8_new(client, 0xd2, 0x48);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	default:
		i2c_put_byte_add8_new(client, 0xfe, 0x02);
		i2c_put_byte_add8_new(client, 0xd1, 0x32);
		i2c_put_byte_add8_new(client, 0xd2, 0x32);
		i2c_put_byte_add8_new(client, 0xfe, 0x00);
		break;
	}
	return 0;
}

static int gc2145_setting(struct gc2145_device *dev, int PROP_ID, int value)
{
	int ret = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	if (!dev->vminfo.isused)
		return 0;

	switch (PROP_ID)  {

	case V4L2_CID_BRIGHTNESS:
		dprintk(dev, 1, "setting brightned:%d\n", value);
		ret = brightness_gc2145(client, value);
		break;
	case V4L2_CID_CONTRAST:
		ret = contrast_gc2145(client, value);
		break;
	case V4L2_CID_SATURATION:
		ret = saturation_gc2145(client, value);
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		if (gc2145_qctrl[0].default_value != value) {
			gc2145_qctrl[0].default_value = value;
			GC2145_set_param_wb(dev, value);
			pr_info("set camera white_balance=%d.\n ",
				value);
		}
		break;
	case V4L2_CID_EXPOSURE:
		if (gc2145_qctrl[1].default_value != value) {
			gc2145_qctrl[1].default_value = value;
			GC2145_set_param_exposure(dev, value);
			pr_info("set camera exposure=%d.\n",
				value);
		}
		break;
	case V4L2_CID_COLORFX:
		if (gc2145_qctrl[2].default_value != value) {
			gc2145_qctrl[2].default_value = value;
			GC2145_set_param_effect(dev, value);
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
		}
		break;
	case V4L2_CID_BLUE_BALANCE:
		if (gc2145_qctrl[4].default_value != value) {
			gc2145_qctrl[4].default_value = value;
			pr_info("@@@SP_222:GC2145_set_night_mode,night mode=%d\n",
				value);
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
		i2c_put_byte_add8_new(client, 0x17, 0x15);
		break;
	case V4L2_CID_VFLIP:    /* set flip on V. */
		i2c_put_byte_add8_new(client, 0x17, 0x16);
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


#define TSTAMP_MIN_Y	24
#define TSTAMP_MAX_Y	(TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X	10
#define TSTAMP_MIN_X	(54 + TSTAMP_INPUT_X)

static void gc2145_fillbuff(struct gc2145_fh *fh, struct gc2145_buffer *buf)
{
	int ret;
	struct gc2145_device *dev = fh->dev;
	void *vbuf = (void *)videobuf_to_res(&buf->vb);
	struct vm_output_para para = {0};

	dprintk(dev, 1, "%s\n", __func__);
	if (!vbuf)
		return;

	para.mirror = gc2145_qctrl[5].default_value & 3;
	para.v4l2_format = fh->fmt->fourcc;
	para.v4l2_memory = MAGIC_RE_MEM;
	para.zoom = gc2145_qctrl[7].default_value;
	para.angle = gc2145_qctrl[8].default_value;
	para.vaddr = (uintptr_t)vbuf;
	para.ext_canvas = 0;
	para.width = buf->vb.width;
	para.height =
	(buf->vb.height == 1080) ? 1088 : buf->vb.height;
	dev->vminfo.vdin_id = dev->cam_info.vdin_path;
	ret = vm_fill_this_buffer(&buf->vb, &para, &dev->vminfo);
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

	spin_unlock_irqrestore(&dev->slock, flags);
	gc2145_fillbuff(fh, buf);
	dprintk(dev, 1, "filled buffer %p\n", buf);

	wake_up(&buf->vb.done);
	dprintk(dev, 2, "[%p/%d] wakeup\n", buf, buf->vb. i);
	return;
unlock:
	spin_unlock_irqrestore(&dev->slock, flags);
}

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

	gc2145_thread_tick(fh);

	schedule_timeout_interruptible(1);

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

	for (;;) {
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

	dma_q->kthread = kthread_run(gc2145_thread, fh, "gc2145_mipi");

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}

	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void gc2145_stop_thread(struct gc2145_dmaqueue  *dma_q)
{
	struct gc2145_device *dev =
		container_of(dma_q, struct gc2145_device, vidq);

	dprintk(dev, 1, "%s\n", __func__);

	if (dma_q->kthread) {
		kthread_stop(dma_q->kthread);
		dma_q->kthread = NULL;
	}
}

static int
buffer_setup(struct videobuf_queue *vq, unsigned int *count, unsigned int *size)
{
	struct videobuf_res_privdata *res = vq->priv_data;
	struct gc2145_fh *fh  = container_of(res, struct gc2145_fh, res);
	struct gc2145_device *dev  = fh->dev;
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

static void free_buffer(struct videobuf_queue *vq, struct gc2145_buffer *buf)
{
	struct videobuf_res_privdata *res = vq->priv_data;
	struct gc2145_fh *fh  = container_of(res, struct gc2145_fh, res);
	struct gc2145_device *dev  = fh->dev;

	dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);
	videobuf_waiton(vq, &buf->vb, 0, 0);

	if (in_interrupt())
		WARN_ON(1);

	videobuf_res_free(vq, &buf->vb);
	dprintk(dev, 1, "free_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define norm_maxw() 3000
#define norm_maxh() 3000
static int
buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
		enum v4l2_field field)
{
	struct videobuf_res_privdata *res = vq->priv_data;
	struct gc2145_fh *fh  = container_of(res, struct gc2145_fh, res);
	struct gc2145_device    *dev = fh->dev;
	struct gc2145_buffer *buf = container_of(vb, struct gc2145_buffer, vb);
	int rc;

	dprintk(dev, 1, "%s, field=%d\n", __func__, field);

	WARN_ON(fh->fmt == NULL);

	if (fh->width  < 48 || fh->width  > norm_maxw() ||
			fh->height < 32 || fh->height > norm_maxh())
		return -EINVAL;

	buf->vb.size = (fh->width * fh->height * fh->fmt->depth) >> 3;
	if (buf->vb.baddr != 0  &&  buf->vb.bsize < buf->vb.size)
		return -EINVAL;

	buf->fmt       = fh->fmt;
	buf->vb.width  = fh->width;
	buf->vb.height = fh->height;
	buf->vb.field  = field;

	if (buf->vb.state == VIDEOBUF_NEEDS_INIT) {
		rc = videobuf_iolock(vq, &buf->vb, NULL);
		if (rc < 0)
			goto fail;
	}

	buf->vb.state = VIDEOBUF_PREPARED;

	return 0;

fail:
	free_buffer(vq, buf);
	return rc;
}

static void
buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct gc2145_buffer *buf  =
		container_of(vb, struct gc2145_buffer, vb);
	struct videobuf_res_privdata *res = vq->priv_data;
	struct gc2145_fh *fh  = container_of(res, struct gc2145_fh, res);
	struct gc2145_device       *dev  = fh->dev;
	struct gc2145_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
		struct videobuf_buffer *vb)
{
	struct gc2145_buffer *buf  =
		container_of(vb, struct gc2145_buffer, vb);
	struct videobuf_res_privdata *res = vq->priv_data;
	struct gc2145_fh *fh  = container_of(res, struct gc2145_fh, res);
	struct gc2145_device *dev = (struct gc2145_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops gc2145_video_qops = {
	.buf_setup      = buffer_setup,
	.buf_prepare    = buffer_prepare,
	.buf_queue      = buffer_queue,
	.buf_release    = buffer_release,
};

static int vidioc_querycap(struct file *file, void  *priv,
		struct v4l2_capability *cap)
{
	struct gc2145_fh  *fh  = priv;
	struct gc2145_device *dev = fh->dev;

	strcpy(cap->driver, "gc2145_mipi");
	strcpy(cap->card, "gc2145_mipi.canvas");
	if (dev->cam_info.front_back == 0)
		strcat(cap->card, "back");
	else
		strcat(cap->card, "front");

	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = GC2145_CAMERA_VERSION;
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
				| V4L2_CAP_READWRITE;

	cap->capabilities =	cap->device_caps | V4L2_CAP_DEVICE_CAPS;
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

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_format *f)
{
	struct gc2145_fh *fh = priv;

	pr_info("%s, fh->width =%d,fh->height=%d\n",
		__func__, fh->width, fh->height);
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

	if (field == V4L2_FIELD_ANY) {
		field = V4L2_FIELD_INTERLACED;
	} else if (field != V4L2_FIELD_INTERLACED) {
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

static struct resolution_param *prev_res;

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_format *f)
{
	struct gc2145_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;
	struct gc2145_device *dev = fh->dev;
	struct resolution_param *res_param = NULL;
	int ret;

	f->fmt.pix.width = (f->fmt.pix.width + (CANVAS_WIDTH_ALIGN - 1)) &
		(~(CANVAS_WIDTH_ALIGN - 1));
	if ((f->fmt.pix.pixelformat == V4L2_PIX_FMT_YVU420) ||
		(f->fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420)) {
		f->fmt.pix.width =
			(f->fmt.pix.width + (CANVAS_WIDTH_ALIGN * 2 - 1)) &
			(~(CANVAS_WIDTH_ALIGN * 2 - 1));
	}
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
	dprintk(dev, 3,
		"system aquire ...fh->height=%d, fh->width= %d\n",
		fh->height, fh->width);

	if (f->fmt.pix.pixelformat == V4L2_PIX_FMT_RGB24) {
		res_param = get_resolution_param(dev, 1, fh->width, fh->height);
		if (!res_param) {
			pr_err("error, resolution param not get\n");
			goto out;
		}

		set_resolution_param(dev, res_param);
		msleep(80);
	} else {
		res_param = get_resolution_param(dev, 0, fh->width, fh->height);
		if (!res_param) {
			pr_err("error, resolution param not get\n");
			goto out;
		}
		set_resolution_param(dev, res_param);
		prev_res = res_param;
		msleep(100);
	}

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
	pr_info("g_parm,deno=%d, numerator=%d\n", cp->timeperframe.denominator,
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
		if (ret == 0) {
			p->reserved  =
			convert_canvas_index(fh->fmt->fourcc,
			GC2145_RES0_CANVAS_INDEX + p->index * 3);
		} else {
			p->reserved = 0;
		}
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

	return (videobuf_dqbuf(&fh->vb_vidq, p,
		file->f_flags & O_NONBLOCK));
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
	struct gc2145_device *dev = fh->dev;
	struct vdin_parm_s para;
	unsigned int vdin_path;
	int ret = 0;

	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;

	memset(&para, 0, sizeof(para));
	para.port  = TVIN_PORT_MIPI;
	para.fmt = TVIN_SIG_FMT_MAX;
	if (fh->dev->cur_resolution_param) {
		para.frame_rate =
			gc2145_frmintervals_active.denominator;
		para.h_active =
			fh->dev->cur_resolution_param->active_frmsize.width;
		para.v_active =
			fh->dev->cur_resolution_param->active_frmsize.height;
		para.hs_bp = 0;
		para.vs_bp = 2;
		para.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
	} else
		pr_info("error, cur_resolution_param is NULL\n");

	vdin_path = fh->dev->cam_info.vdin_path;

	pr_info("%s: h_active=%d; v_active=%d, frame_rate=%d,vdin_path=%d\n",
		__func__, para.h_active, para.v_active,
		para.frame_rate, vdin_path);
	if (temp_frame < 0) {
		temp_frame = para.frame_rate;
		para.skip_count =  2;
	} else {
		temp_frame = para.frame_rate;
		para.skip_count =  5;
	}
	para.cfmt = TVIN_YUV422;
	para.dfmt = TVIN_YUV422;/*TVIN_NV21*/
	para.hsync_phase = 1;
	para.vsync_phase = 1;
	para.bt_path = dev->cam_info.bt_path;
	/*config mipi parameter*/
	para.csi_hw_info.settle = fh->dev->cur_resolution_param->bps;
	para.csi_hw_info.lanes = fh->dev->cur_resolution_param->lanes;

	pr_info("gc2145 stream on.\n");
	ret =  videobuf_streamon(&fh->vb_vidq);
	if (ret == 0) {
		pr_info("gc2145 start tvin service.\n");
		vops->start_tvin_service(vdin_path, &para);
		fh->stream_on = 1;
	}
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct gc2145_fh  *fh = priv;
	int ret = 0;

	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;
	ret = videobuf_streamoff(&fh->vb_vidq);
	if (ret == 0) {
		vops->stop_tvin_service(0);
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
			|| (fmt->fourcc == V4L2_PIX_FMT_YUV420)
			|| (fmt->fourcc == V4L2_PIX_FMT_YVU420)) {
		if (fsize->index >= ARRAY_SIZE(prev_resolution_array))
			return -EINVAL;
		frmsize = &prev_resolution_array[fsize->index].frmsize;
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	} else if (fmt->fourcc == V4L2_PIX_FMT_RGB24) {
		if (fsize->index >= ARRAY_SIZE(capture_resolution_array))
			return -EINVAL;
		frmsize =
		&capture_resolution_array[fsize->index].frmsize;
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

static int vidioc_enum_input(struct file *file, void *priv,
		struct v4l2_input *inp)
{
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

	dev->input = i;

	return 0;
}

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

static int vidioc_querymenu(struct file *file, void *priv,
		struct v4l2_querymenu *a)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(gc2145_qmenu_set); i++)
		if (a->id && a->id == gc2145_qmenu_set[i].id) {
			for (j = 0; j < gc2145_qmenu_set[i].num; j++)
				if (a->index ==
				gc2145_qmenu_set[i].gc2145_qmenu[j].index) {
					memcpy(a,
					&(gc2145_qmenu_set[i].gc2145_qmenu[j]),
					sizeof(*a));
					return 0;
				}
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
			gc2145_setting(dev, ctrl->id, ctrl->value) < 0) {
				return -ERANGE;
			}
			dev->qctl_regs[i] = ctrl->value;
			return 0;
		}
	return -EINVAL;
}

static int gc2145_open(struct file *file)
{
	struct gc2145_device *dev = video_drvdata(file);
	struct gc2145_fh *fh = NULL;
	int retval = 0;

	dev->vminfo.vdin_id = dev->cam_info.vdin_path;
	dev->vminfo.bt_path_count = dev->cam_info.bt_path_count;

#ifdef CONFIG_CMA
	retval = vm_init_resource(24 * SZ_1M, &dev->vminfo);
	if (retval < 0) {
		pr_err("error: no cma memory\n");
		return -1;
	}
#endif
	mutex_lock(&firmware_mutex);
	gc2145_have_opened = 1;
	mutex_unlock(&firmware_mutex);

	aml_cam_init(&dev->cam_info);

	GC2145_init_regs(dev);

	msleep(20);

	mutex_lock(&dev->mutex);
	dev->users++;
	if (dev->users > 1) {
		dev->users--;
		mutex_unlock(&dev->mutex);
		vm_deinit_resource(&(dev->vminfo));
		return -EBUSY;
	}

	dprintk(dev, 1, "open %s type=%s users=%d\n",
		video_device_node_name(dev->vdev),
		v4l2_type_names[V4L2_BUF_TYPE_VIDEO_CAPTURE], dev->users);

	INIT_LIST_HEAD(&dev->vidq.active);
	init_waitqueue_head(&dev->vidq.wq);
	spin_lock_init(&dev->slock);

	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (fh == NULL) {
		dev->users--;
		retval = -ENOMEM;
	}
	mutex_unlock(&dev->mutex);

	if (retval) {
		vm_deinit_resource(&(dev->vminfo));
		return retval;
	}

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock(&(dev->wake_lock));
#endif

	file->private_data = fh;
	fh->dev      = dev;

	fh->type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fh->fmt      = &formats[0];
	fh->width    = 640;
	fh->height   = 480;
	fh->stream_on = 0;
	fh->f_flags  = file->f_flags;

	dev->jiffies = jiffies;

	if (dev->vminfo.mem_alloc_succeed) {
		fh->res.start = dev->vminfo.buffer_start;
		fh->res.end = dev->vminfo.buffer_start +
						dev->vminfo.vm_buf_size - 1;
		fh->res.magic = MAGIC_RE_MEM;
		fh->res.priv = NULL;
		videobuf_queue_res_init(&fh->vb_vidq, &gc2145_video_qops,
						NULL, &dev->slock, fh->type,
						V4L2_FIELD_INTERLACED,
						sizeof(struct gc2145_buffer),
						(void *)&fh->res, NULL);
	}

	bDoingAutoFocusMode = false;
	gc2145_start_thread(fh);
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
	mutex_lock(&firmware_mutex);
	gc2145_have_opened = 0;
	dev->firmware_ready = 0;
	mutex_unlock(&firmware_mutex);
	gc2145_stop_thread(vidq);
	videobuf_stop(&fh->vb_vidq);
	if (fh->stream_on) {
		pr_info("%s, vdin_path = %d\n",
			   __func__, vdin_path);
		vops->stop_tvin_service(vdin_path);
	}

	videobuf_mmap_free(&fh->vb_vidq);

	kfree(fh);

	mutex_lock(&dev->mutex);
	dev->users--;
	mutex_unlock(&dev->mutex);

	dprintk(dev, 1, "close called (dev=%s, users=%d)\n",
		video_device_node_name(vdev), dev->users);

	gc2145_qctrl[0].default_value = 0;
	gc2145_qctrl[1].default_value = 4;
	gc2145_qctrl[2].default_value = 0;
	gc2145_qctrl[3].default_value = 0;
	gc2145_qctrl[4].default_value = 0;

	gc2145_qctrl[5].default_value = 0;
	gc2145_qctrl[7].default_value = 100;
	gc2145_qctrl[8].default_value = 0;
	temp_frame = -1;
	power_down_gc2145(dev);
	gc2145_frmintervals_active.numerator = 1;
	gc2145_frmintervals_active.denominator = 20;

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
		(unsigned long)vma->vm_end-(unsigned long)vma->vm_start,
		ret);

	return ret;
}

static const struct v4l2_file_operations gc2145_fops = {
	.owner	            = THIS_MODULE,
	.open               = gc2145_open,
	.release            = gc2145_close,
	.read               = gc2145_read,
	.poll	            = gc2145_poll,
	.unlocked_ioctl     = video_ioctl2,
	.mmap               = gc2145_mmap,
};

static const struct v4l2_ioctl_ops gc2145_ioctl_ops = {
	.vidioc_querycap            = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap    = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap       = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap     = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap       = vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs             = vidioc_reqbufs,
	.vidioc_querybuf            = vidioc_querybuf,
	.vidioc_qbuf                = vidioc_qbuf,
	.vidioc_dqbuf               = vidioc_dqbuf,
	.vidioc_s_std               = vidioc_s_std,
	.vidioc_enum_input          = vidioc_enum_input,
	.vidioc_g_input             = vidioc_g_input,
	.vidioc_s_input             = vidioc_s_input,
	.vidioc_queryctrl           = vidioc_queryctrl,
	.vidioc_querymenu           = vidioc_querymenu,
	.vidioc_g_ctrl              = vidioc_g_ctrl,
	.vidioc_s_ctrl              = vidioc_s_ctrl,
	.vidioc_streamon            = vidioc_streamon,
	.vidioc_streamoff           = vidioc_streamoff,
	.vidioc_enum_framesizes     = vidioc_enum_framesizes,
	.vidioc_g_parm              = vidioc_g_parm,
	.vidioc_enum_frameintervals = vidioc_enum_frameintervals,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf                = vidiocgmbuf,
#endif
};

static struct video_device gc2145_template = {
	.name	        = "gc2145_v4l",
	.fops           = &gc2145_fops,
	.ioctl_ops      = &gc2145_ioctl_ops,
	.release        = video_device_release,
	.tvnorms        = V4L2_STD_525_60,
};

static const struct v4l2_subdev_core_ops gc2145_core_ops = {
};

static const struct v4l2_subdev_ops gc2145_ops = {
	.core = &gc2145_core_ops,
};

static int gc2145_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct aml_cam_info_s *plat_dat;
	int err;
	int ret;
	struct gc2145_device *t;
	struct v4l2_subdev *sd;

	vops = get_vdin_v4l2_ops();
	v4l_info(client, "chip found @ 0x%x (%s)\n",
		client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL) {
		pr_err("gc2145 probe kzalloc failed.\n");
		return -ENOMEM;
	}

	client->addr = 0x3c;
	snprintf(t->v4l2_dev.name, sizeof(t->v4l2_dev.name),
		"%s-%03d", "gc2145_mipi", 0);
	ret = v4l2_device_register(NULL, &t->v4l2_dev);
	if (ret) {
		pr_info("%s, v4l2 device register failed", __func__);
		kfree(t);
		return ret;
	}

	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &gc2145_ops);
	mutex_init(&t->mutex);
	memset(&t->vminfo, 0, sizeof(struct vm_init_s));

	t->vdev = video_device_alloc();
	if (t->vdev == NULL) {
		kfree(t);
		return -ENOMEM;
	}
	memcpy(t->vdev, &gc2145_template, sizeof(*t->vdev));
	t->vdev->v4l2_dev = &t->v4l2_dev;
	video_set_drvdata(t->vdev, t);

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&(t->wake_lock), WAKE_LOCK_SUSPEND, "gc2145_mipi");
#endif

	plat_dat = (struct aml_cam_info_s *)client->dev.platform_data;
	if (plat_dat) {
		memcpy(&t->cam_info, plat_dat, sizeof(struct aml_cam_info_s));
		pr_info("%s, front_back = %d\n",
			__func__, plat_dat->front_back);
		video_nr = plat_dat->front_back;
	} else {
		pr_err("camera gc2145: have no platform data\n");
		video_device_release(t->vdev);
		kfree(t);
		return -1;
	}

	t->cam_info.version = GC2145_DRIVER_VERSION;
	if (aml_cam_info_reg(&t->cam_info) < 0)
		pr_err("reg caminfo error\n");

	pr_info("t->vdev = %p, video_nr = %d\n", t->vdev, video_nr);
	err = video_register_device(t->vdev, VFL_TYPE_GRABBER, video_nr);
	if (err < 0) {
		video_device_release(t->vdev);
		kfree(t);
		return err;
	}

	pr_info("gc2145_probe successful.\n");

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
	{ "gc2145_mipi", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc2145_id);

static struct i2c_driver gc2145_i2c_driver = {
	.driver = {
		.name = "gc2145_mipi",
	},
	.probe = gc2145_probe,
	.remove = gc2145_remove,
	.id_table = gc2145_id,
};

module_i2c_driver(gc2145_i2c_driver);

