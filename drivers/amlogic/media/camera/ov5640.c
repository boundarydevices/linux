/*
 * drivers/amlogic/media/camera/ov5640.c
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
#include "ov5640_firmware.h"

#define OV5640_CAMERA_MODULE_NAME   "ov5640"

#define WAKE_NUMERATOR              30
#define WAKE_DENOMINATOR            1001
#define BUFFER_TIMEOUT              msecs_to_jiffies(500)

#define MAGIC_RE_MEM                0x123039dc
#define OV5640_RES0_CANVAS_INDEX    0x4e

#define OV5640_CAMERA_MAJOR_VERSION 0
#define OV5640_CAMERA_MINOR_VERSION 7
#define OV5640_CAMERA_RELEASE       0
#define OV5640_CAMERA_VERSION                       \
	KERNEL_VERSION(OV5640_CAMERA_MAJOR_VERSION, \
	OV5640_CAMERA_MINOR_VERSION,                \
	OV5640_CAMERA_RELEASE)

MODULE_DESCRIPTION("ov5640 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");

#define OV5640_DRIVER_VERSION "OV5640-COMMON-01-140717"

static unsigned int video_nr = -1;

static unsigned int debug;

static unsigned int vid_limit = 32;

static int ov5640_have_opened;

static void do_download(struct work_struct *work);
static DECLARE_DELAYED_WORK(dl_work, do_download);

static struct vdin_v4l2_ops_s *vops;

static bool bDoingAutoFocusMode;
static int temp_frame = -1;
static struct v4l2_fract ov5640_frmintervals_active = {
	.numerator = 1,
	.denominator = 15,
};

static struct v4l2_frmivalenum ov5640_frmivalenum[] = {
	{
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
				.denominator = 5,
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

static struct v4l2_queryctrl ov5640_qctrl[] = {
	{
		.id            = V4L2_CID_BRIGHTNESS,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Brightness",
		.minimum       = 0,
		.maximum       = 255,
		.step          = 1,
		.default_value = 127,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_CONTRAST,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Contrast",
		.minimum       = 0x10,
		.maximum       = 0x60,
		.step          = 0xa,
		.default_value = 0x30,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_HFLIP,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "flip on horizontal",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_DISABLED,
	}, {
		.id            = V4L2_CID_VFLIP,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "flip on vertical",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_DISABLED,
	}, {
		.id            = V4L2_CID_DO_WHITE_BALANCE,
		.type          = V4L2_CTRL_TYPE_MENU,
		.name          = "white balance",
		.minimum       = 0,
		.maximum       = 6,
		.step          = 0x1,
		.default_value = 0,
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
		.id            = V4L2_CID_WHITENESS,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "banding",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_FOCUS_AUTO,
		.type          = V4L2_CTRL_TYPE_MENU,
		.name          = "auto focus",
		.minimum       = CAM_FOCUS_MODE_RELEASE,
		.maximum       = CAM_FOCUS_MODE_CONTI_PIC,
		.step          = 0x1,
		.default_value = CAM_FOCUS_MODE_CONTI_PIC,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_BACKLIGHT_COMPENSATION,
		.type          = V4L2_CTRL_TYPE_MENU,
		.name          = "flash",
		.minimum       = FLASHLIGHT_ON,
		.maximum       = FLASHLIGHT_TORCH,
		.step          = 0x1,
		.default_value = FLASHLIGHT_OFF,
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
		.id            = V4L2_CID_ROTATE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Rotate",
		.minimum       = 0,
		.maximum       = 270,
		.step          = 90,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_AUTO_FOCUS_STATUS,
		.type          = 8,
		.name          = "focus status",
		.minimum       = 0,
		.maximum       = ~3,
		.step          = 0x1,
		.default_value = V4L2_AUTO_FOCUS_STATUS_IDLE,
		.flags         = V4L2_CTRL_FLAG_READ_ONLY,
	}, {
		.id            = V4L2_CID_FOCUS_ABSOLUTE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "focus center",
		.minimum       = 0,
		.maximum       = ((2000) << 16) | 2000,
		.step          = 1,
		.default_value = (1000 << 16) | 1000,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}
};

struct v4l2_querymenu ov5640_qmenu_autofocus[] = {
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

struct v4l2_querymenu ov5640_qmenu_flashmode[] = {
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

struct v4l2_querymenu ov5640_qmenu_wbmode[] = {
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

struct ov5640_qmenu_s {
	__u32   id;
	int     num;
	struct v4l2_querymenu *ov5640_qmenu;
};

struct ov5640_qmenu_s ov5640_qmenu_set[] = {
	{
		.id             = V4L2_CID_FOCUS_AUTO,
		.num            = ARRAY_SIZE(ov5640_qmenu_autofocus),
		.ov5640_qmenu   = ov5640_qmenu_autofocus,
	}, {
		.id             = V4L2_CID_BACKLIGHT_COMPENSATION,
		.num            = ARRAY_SIZE(ov5640_qmenu_flashmode),
		.ov5640_qmenu   = ov5640_qmenu_flashmode,
	}, {
		.id             = V4L2_CID_DO_WHITE_BALANCE,
		.num            = ARRAY_SIZE(ov5640_qmenu_wbmode),
		.ov5640_qmenu   = ov5640_qmenu_wbmode,
	}
};

#define dprintk(dev, level, fmt, arg...) \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)


struct ov5640_fmt {
	char  *name;
	u32   fourcc;
	int   depth;
};

static struct ov5640_fmt formats[] = {
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

static struct ov5640_fmt *get_format(struct v4l2_format *f)
{
	struct ov5640_fmt *fmt;
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


struct ov5640_buffer {
	struct videobuf_buffer vb;
	struct ov5640_fmt        *fmt;
	unsigned int canvas_id;
};

struct ov5640_dmaqueue {
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

static LIST_HEAD(ov5640_devicelist);

struct ov5640_device {
	struct list_head        ov5640_devicelist;
	struct v4l2_subdev      sd;
	struct v4l2_device      v4l2_dev;

	spinlock_t              slock;
	struct mutex            mutex;

	int                     users;

	struct video_device     *vdev;

	struct ov5640_dmaqueue  vidq;

	unsigned long           jiffies;

	int                     input;

	struct aml_cam_info_s   cam_info;

	int                     qctl_regs[ARRAY_SIZE(ov5640_qctrl)];

	struct resolution_param *cur_resolution_param;

#ifdef CONFIG_HAS_WAKELOCK
	struct wake_lock        wake_lock;
#endif

	struct work_struct      dl_work;

	int                     firmware_ready;

	struct vm_init_s        vminfo;
};

static DEFINE_MUTEX(firmware_mutex);

static inline struct ov5640_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov5640_device, sd);
}

struct ov5640_fh {
	struct ov5640_device         *dev;

	struct ov5640_fmt            *fmt;
	unsigned int                 width, height;
	struct videobuf_queue        vb_vidq;

	struct videobuf_res_privdata res;

	enum v4l2_buf_type           type;
	int                          input;
	int                          stream_on;
	unsigned int                 f_flags;
};

static struct aml_camera_i2c_fig_s OV5640_script[] = {
	{0xffff, 0xff}
};

static struct aml_camera_i2c_fig_s OV5640_preview_720P_script[] = {
	{0x3103, 0x11},
	{0x3008, 0x82},
	{0x3008, 0x42},
	{0x3103, 0x03},
	{0x3017, 0x00},
	{0x3018, 0x00},
	{0x3034, 0x18},
	{0x3035, 0x11},
	{0x3036, 0x54},
	{0x3037, 0x13},
	{0x3108, 0x01},
	{0x3630, 0x36},
	{0x3631, 0x0e},
	{0x3632, 0xe2},
	{0x3633, 0x12},
	{0x3621, 0xe0},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3905, 0x02},
	{0x3906, 0x10},
	{0x3901, 0x0a},
	{0x3731, 0x12},
	{0x3600, 0x08},
	{0x3601, 0x33},
	{0x302d, 0x60},
	{0x3620, 0x52},
	{0x371b, 0x20},
	{0x471c, 0x50},
	{0x3a13, 0x43},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3635, 0x13},
	{0x3636, 0x03},
	{0x3634, 0x40},
	{0x3622, 0x01},
	{0x3c01, 0x34},
	{0x3c04, 0x28},
	{0x3c05, 0x98},
	{0x3c06, 0x00},
	{0x3c07, 0x07},
	{0x3c08, 0x00},
	{0x3c09, 0x1c},
	{0x3c0a, 0x9c},
	{0x3c0b, 0x40},
	{0x3820, 0x41},
	{0x3821, 0x07},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0xfa},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x06},
	{0x3807, 0xa9},
	{0x3808, 0x05},
	{0x3809, 0x00},
	{0x380a, 0x02},
	{0x380b, 0xd0},
	{0x380c, 0x07},
	{0x380d, 0x64},
	{0x380e, 0x02},
	{0x380f, 0xe4},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3618, 0x00},
	{0x3612, 0x29},
	{0x3708, 0x66},
	{0x3709, 0x12},
	{0x370c, 0x03},
	{0x3a02, 0x02},
	{0x3a03, 0xe4},
	{0x3a08, 0x01},
	{0x3a09, 0xbc},
	{0x3a0a, 0x01},
	{0x3a0b, 0x72},
	{0x3a0e, 0x01},
	{0x3a0d, 0x02},
	{0x3a14, 0x02},
	{0x3a15, 0xe4},
	{0x4001, 0x02},
	{0x4004, 0x02},
	{0x4050, 0x6e},
	{0x4051, 0x8f},
	{0x3000, 0x00},
	{0x3002, 0x1c},
	{0x3004, 0xff},
	{0x3006, 0xc3},
	{0x300e, 0x45},
	{0x302e, 0x08},
	{0x4300, 0x32},
	{0x501f, 0x00},
	{0x5684, 0x05},
	{0x5685, 0x20},
	{0x5686, 0x02},
	{0x5687, 0xd8},
	{0x4713, 0x02},
	{0x4407, 0x04},
	{0x440e, 0x00},
	{0x460b, 0x37},
	{0x460c, 0x20},
	{0x4827, 0x16},
	{0x3824, 0x04},
	{0x5000, 0xa7},
	{0x5001, 0x83},
	{0x5180, 0xff},
	{0x5181, 0xf2},
	{0x5182, 0x00},
	{0x5183, 0x14},
	{0x5184, 0x25},
	{0x5185, 0x24},
	{0x5186, 0x09},
	{0x5187, 0x09},
	{0x5188, 0x09},
	{0x5189, 0x75},
	{0x518a, 0x54},
	{0x518b, 0xe0},
	{0x518c, 0xb2},
	{0x518d, 0x42},
	{0x518e, 0x3d},
	{0x518f, 0x56},
	{0x5190, 0x46},
	{0x5191, 0xf8},
	{0x5192, 0x04},
	{0x5193, 0x70},
	{0x5194, 0xf0},
	{0x5195, 0xf0},
	{0x5196, 0x03},
	{0x5197, 0x01},
	{0x5198, 0x04},
	{0x5199, 0x12},
	{0x519a, 0x04},
	{0x519b, 0x00},
	{0x519c, 0x06},
	{0x519d, 0x82},
	{0x519e, 0x38},
	{0x5381, 0x1e},
	{0x5382, 0x5b},
	{0x5383, 0x08},
	{0x5384, 0x0a},
	{0x5385, 0x7e},
	{0x5386, 0x88},
	{0x5387, 0x7c},
	{0x5388, 0x6c},
	{0x5389, 0x10},
	{0x538a, 0x01},
	{0x538b, 0x98},
	{0x5300, 0x08},
	{0x5301, 0x30},
	{0x5302, 0x10},
	{0x5303, 0x00},
	{0x5304, 0x08},
	{0x5305, 0x30},
	{0x5306, 0x08},
	{0x5307, 0x16},
	{0x5309, 0x08},
	{0x530a, 0x30},
	{0x530b, 0x04},
	{0x530c, 0x06},
	{0x5480, 0x01},
	{0x5481, 0x08},
	{0x5482, 0x14},
	{0x5483, 0x28},
	{0x5484, 0x51},
	{0x5485, 0x65},
	{0x5486, 0x71},
	{0x5487, 0x7d},
	{0x5488, 0x87},
	{0x5489, 0x91},
	{0x548a, 0x9a},
	{0x548b, 0xaa},
	{0x548c, 0xb8},
	{0x548d, 0xcd},
	{0x548e, 0xdd},
	{0x548f, 0xea},
	{0x5490, 0x1d},
	{0x5580, 0x02},
	{0x5583, 0x40},
	{0x5584, 0x10},
	{0x5589, 0x10},
	{0x558a, 0x00},
	{0x558b, 0xf8},
	{0x5800, 0x23},
	{0x5801, 0x14},
	{0x5802, 0x0f},
	{0x5803, 0x0f},
	{0x5804, 0x12},
	{0x5805, 0x26},
	{0x5806, 0x0c},
	{0x5807, 0x08},
	{0x5808, 0x05},
	{0x5809, 0x05},
	{0x580a, 0x08},
	{0x580b, 0x0d},
	{0x580c, 0x08},
	{0x580d, 0x03},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x03},
	{0x5811, 0x09},
	{0x5812, 0x07},
	{0x5813, 0x03},
	{0x5814, 0x00},
	{0x5815, 0x01},
	{0x5816, 0x03},
	{0x5817, 0x08},
	{0x5818, 0x0d},
	{0x5819, 0x08},
	{0x581a, 0x05},
	{0x581b, 0x06},
	{0x581c, 0x08},
	{0x581d, 0x0e},
	{0x581e, 0x29},
	{0x581f, 0x17},
	{0x5820, 0x11},
	{0x5821, 0x11},
	{0x5822, 0x15},
	{0x5823, 0x28},
	{0x5824, 0x46},
	{0x5825, 0x26},
	{0x5826, 0x08},
	{0x5827, 0x26},
	{0x5828, 0x64},
	{0x5829, 0x26},
	{0x582a, 0x24},
	{0x582b, 0x22},
	{0x582c, 0x24},
	{0x582d, 0x24},
	{0x582e, 0x06},
	{0x582f, 0x22},
	{0x5830, 0x40},
	{0x5831, 0x42},
	{0x5832, 0x24},
	{0x5833, 0x26},
	{0x5834, 0x24},
	{0x5835, 0x22},
	{0x5836, 0x22},
	{0x5837, 0x26},
	{0x5838, 0x44},
	{0x5839, 0x24},
	{0x583a, 0x26},
	{0x583b, 0x28},
	{0x583c, 0x42},
	{0x583d, 0xce},
	{0x5025, 0x00},
	{0x3a0f, 0x30},
	{0x3a10, 0x28},
	{0x3a1b, 0x30},
	{0x3a1e, 0x26},
	{0x3a11, 0x60},
	{0x3a1f, 0x14},
	{0x3008, 0x02},
	{0xffff, 0xff}
};

static struct aml_camera_i2c_fig_s OV5640_preview_1080P_script[] = {
	{0x3103, 0x11},
	{0x3008, 0x82},
	{0x3008, 0x42},
	{0x3103, 0x03},
	{0x3017, 0x00},
	{0x3018, 0x00},
	{0x3034, 0x18},
	{0x3035, 0x11},
	{0x3036, 0x54},
	{0x3037, 0x13},
	{0x3108, 0x01},
	{0x3630, 0x36},
	{0x3631, 0x0e},
	{0x3632, 0xe2},
	{0x3633, 0x12},
	{0x3621, 0xe0},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3905, 0x02},
	{0x3906, 0x10},
	{0x3901, 0x0a},
	{0x3731, 0x12},
	{0x3600, 0x08},
	{0x3601, 0x33},
	{0x302d, 0x60},
	{0x3620, 0x52},
	{0x371b, 0x20},
	{0x471c, 0x50},
	{0x3a13, 0x43},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3635, 0x13},
	{0x3636, 0x03},
	{0x3634, 0x40},
	{0x3622, 0x01},
	{0x3c01, 0x34},
	{0x3c04, 0x28},
	{0x3c05, 0x98},
	{0x3c06, 0x00},
	{0x3c07, 0x07},
	{0x3c08, 0x00},
	{0x3c09, 0x1c},
	{0x3c0a, 0x9c},
	{0x3c0b, 0x40},
	{0x3820, 0x40},
	{0x3821, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3800, 0x01},
	{0x3801, 0x50},
	{0x3802, 0x01},
	{0x3803, 0xb2},
	{0x3804, 0x08},
	{0x3805, 0xef},
	{0x3806, 0x05},
	{0x3807, 0xf1},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x38},
	{0x380c, 0x09},
	{0x380d, 0xc4},
	{0x380e, 0x04},
	{0x380f, 0x60},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3618, 0x04},
	{0x3612, 0x2b},
	{0x3708, 0x63},
	{0x3709, 0x12},
	{0x370c, 0x00},
	{0x3a02, 0x04},
	{0x3a03, 0x60},
	{0x3a08, 0x01},
	{0x3a09, 0x50},
	{0x3a0a, 0x01},
	{0x3a0b, 0x18},
	{0x3a0e, 0x03},
	{0x3a0d, 0x04},
	{0x3a14, 0x04},
	{0x3a15, 0x60},
	{0x4001, 0x02},
	{0x4004, 0x06},
	{0x4050, 0x6e},
	{0x4051, 0x8f},
	{0x3000, 0x00},
	{0x3002, 0x1c},
	{0x3004, 0xff},
	{0x3006, 0xc3},
	{0x300e, 0x45},
	{0x302e, 0x08},
	{0x4300, 0x32},
	{0x501f, 0x00},
	{0x5684, 0x07},
	{0x5685, 0xa0},
	{0x5686, 0x04},
	{0x5687, 0x40},
	{0x4713, 0x02},
	{0x4407, 0x04},
	{0x440e, 0x00},
	{0x460b, 0x37},
	{0x460c, 0x20},
	{0x4837, 0x0a},
	{0x3824, 0x04},
	{0x5000, 0xa7},
	{0x5001, 0x83},
	{0x5180, 0xff},
	{0x5181, 0xf2},
	{0x5182, 0x00},
	{0x5183, 0x14},
	{0x5184, 0x25},
	{0x5185, 0x24},
	{0x5186, 0x09},
	{0x5187, 0x09},
	{0x5188, 0x09},
	{0x5189, 0x75},
	{0x518a, 0x54},
	{0x518b, 0xe0},
	{0x518c, 0xb2},
	{0x518d, 0x42},
	{0x518e, 0x3d},
	{0x518f, 0x56},
	{0x5190, 0x46},
	{0x5191, 0xf8},
	{0x5192, 0x04},
	{0x5193, 0x70},
	{0x5194, 0xf0},
	{0x5195, 0xf0},
	{0x5196, 0x03},
	{0x5197, 0x01},
	{0x5198, 0x04},
	{0x5199, 0x12},
	{0x519a, 0x04},
	{0x519b, 0x00},
	{0x519c, 0x06},
	{0x519d, 0x82},
	{0x519e, 0x38},
	{0x5381, 0x1e},
	{0x5382, 0x5b},
	{0x5383, 0x08},
	{0x5384, 0x0a},
	{0x5385, 0x7e},
	{0x5386, 0x88},
	{0x5387, 0x7c},
	{0x5388, 0x6c},
	{0x5389, 0x10},
	{0x538a, 0x01},
	{0x538b, 0x98},
	{0x5300, 0x08},
	{0x5301, 0x30},
	{0x5302, 0x10},
	{0x5303, 0x00},
	{0x5304, 0x08},
	{0x5305, 0x30},
	{0x5306, 0x08},
	{0x5307, 0x16},
	{0x5309, 0x08},
	{0x530a, 0x30},
	{0x530b, 0x04},
	{0x530c, 0x06},
	{0x5480, 0x01},
	{0x5481, 0x08},
	{0x5482, 0x14},
	{0x5483, 0x28},
	{0x5484, 0x51},
	{0x5485, 0x65},
	{0x5486, 0x71},
	{0x5487, 0x7d},
	{0x5488, 0x87},
	{0x5489, 0x91},
	{0x548a, 0x9a},
	{0x548b, 0xaa},
	{0x548c, 0xb8},
	{0x548d, 0xcd},
	{0x548e, 0xdd},
	{0x548f, 0xea},
	{0x5490, 0x1d},
	{0x5580, 0x02},
	{0x5583, 0x40},
	{0x5584, 0x10},
	{0x5589, 0x10},
	{0x558a, 0x00},
	{0x558b, 0xf8},
	{0x5800, 0x23},
	{0x5801, 0x14},
	{0x5802, 0x0f},
	{0x5803, 0x0f},
	{0x5804, 0x12},
	{0x5805, 0x26},
	{0x5806, 0x0c},
	{0x5807, 0x08},
	{0x5808, 0x05},
	{0x5809, 0x05},
	{0x580a, 0x08},
	{0x580b, 0x0d},
	{0x580c, 0x08},
	{0x580d, 0x03},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x03},
	{0x5811, 0x09},
	{0x5812, 0x07},
	{0x5813, 0x03},
	{0x5814, 0x00},
	{0x5815, 0x01},
	{0x5816, 0x03},
	{0x5817, 0x08},
	{0x5818, 0x0d},
	{0x5819, 0x08},
	{0x581a, 0x05},
	{0x581b, 0x06},
	{0x581c, 0x08},
	{0x581d, 0x0e},
	{0x581e, 0x29},
	{0x581f, 0x17},
	{0x5820, 0x11},
	{0x5821, 0x11},
	{0x5822, 0x15},
	{0x5823, 0x28},
	{0x5824, 0x46},
	{0x5825, 0x26},
	{0x5826, 0x08},
	{0x5827, 0x26},
	{0x5828, 0x64},
	{0x5829, 0x26},
	{0x582a, 0x24},
	{0x582b, 0x22},
	{0x582c, 0x24},
	{0x582d, 0x24},
	{0x582e, 0x06},
	{0x582f, 0x22},
	{0x5830, 0x40},
	{0x5831, 0x42},
	{0x5832, 0x24},
	{0x5833, 0x26},
	{0x5834, 0x24},
	{0x5835, 0x22},
	{0x5836, 0x22},
	{0x5837, 0x26},
	{0x5838, 0x44},
	{0x5839, 0x24},
	{0x583a, 0x26},
	{0x583b, 0x28},
	{0x583c, 0x42},
	{0x583d, 0xce},
	{0x5025, 0x00},
	{0x3a0f, 0x30},
	{0x3a10, 0x28},
	{0x3a1b, 0x30},
	{0x3a1e, 0x26},
	{0x3a11, 0x60},
	{0x3a1f, 0x14},
	{0x3008, 0x02},
	{0xffff, 0xff}
};

static struct aml_camera_i2c_fig_s OV5640_capture_5M_script[] = {
	{0x3103, 0x11},
	{0x3008, 0x82},
	{0x3008, 0x42},
	{0x3103, 0x03},
	{0x3017, 0x00},
	{0x3018, 0x00},
	{0x3034, 0x18},
	{0x3035, 0x11},
	{0x3036, 0x54},
	{0x3037, 0x13},
	{0x3108, 0x01},
	{0x3630, 0x36},
	{0x3631, 0x0e},
	{0x3632, 0xe2},
	{0x3633, 0x12},
	{0x3621, 0xe0},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3905, 0x02},
	{0x3906, 0x10},
	{0x3901, 0x0a},
	{0x3731, 0x12},
	{0x3600, 0x08},
	{0x3601, 0x33},
	{0x302d, 0x60},
	{0x3620, 0x52},
	{0x371b, 0x20},
	{0x471c, 0x50},
	{0x3a13, 0x43},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3635, 0x13},
	{0x3636, 0x03},
	{0x3634, 0x40},
	{0x3622, 0x01},
	{0x3c01, 0x34},
	{0x3c04, 0x28},
	{0x3c05, 0x98},
	{0x3c06, 0x00},
	{0x3c07, 0x07},
	{0x3c08, 0x00},
	{0x3c09, 0x1c},
	{0x3c0a, 0x9c},
	{0x3c0b, 0x40},
	{0x3820, 0x40},
	{0x3821, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x07},
	{0x3807, 0x9f},
	{0x3808, 0x0a},
	{0x3809, 0x20},
	{0x380a, 0x07},
	{0x380b, 0x98},
	{0x380c, 0x0b},
	{0x380d, 0x1c},
	{0x380e, 0x07},
	{0x380f, 0xb0},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3618, 0x04},
	{0x3612, 0x2b},
	{0x3708, 0x63},
	{0x3709, 0x12},
	{0x370c, 0x00},
	{0x3a02, 0x07},
	{0x3a03, 0xb0},
	{0x3a08, 0x01},
	{0x3a09, 0x27},
	{0x3a0a, 0x00},
	{0x3a0b, 0xf6},
	{0x3a0e, 0x06},
	{0x3a0d, 0x08},
	{0x3a14, 0x07},
	{0x3a15, 0xb0},
	{0x4001, 0x02},
	{0x4004, 0x06},
	{0x4050, 0x6e},
	{0x4051, 0x8f},
	{0x3000, 0x00},
	{0x3002, 0x1c},
	{0x3004, 0xff},
	{0x3006, 0xc3},
	{0x300e, 0x45},
	{0x302e, 0x08},
	{0x4300, 0x32},
	{0x4837, 0x0a},
	{0x501f, 0x00},
	{0x5684, 0x0a},
	{0x5685, 0x20},
	{0x5686, 0x07},
	{0x5687, 0x98},
	{0x440e, 0x00},
	{0x5000, 0xa7},
	{0x5001, 0x83},
	{0x5180, 0xff},
	{0x5181, 0xf2},
	{0x5182, 0x00},
	{0x5183, 0x14},
	{0x5184, 0x25},
	{0x5185, 0x24},
	{0x5186, 0x09},
	{0x5187, 0x09},
	{0x5188, 0x09},
	{0x5189, 0x75},
	{0x518a, 0x54},
	{0x518b, 0xe0},
	{0x518c, 0xb2},
	{0x518d, 0x42},
	{0x518e, 0x3d},
	{0x518f, 0x56},
	{0x5190, 0x46},
	{0x5191, 0xf8},
	{0x5192, 0x04},
	{0x5193, 0x70},
	{0x5194, 0xf0},
	{0x5195, 0xf0},
	{0x5196, 0x03},
	{0x5197, 0x01},
	{0x5198, 0x04},
	{0x5199, 0x12},
	{0x519a, 0x04},
	{0x519b, 0x00},
	{0x519c, 0x06},
	{0x519d, 0x82},
	{0x519e, 0x38},
	{0x5381, 0x1e},
	{0x5382, 0x5b},
	{0x5383, 0x08},
	{0x5384, 0x0a},
	{0x5385, 0x7e},
	{0x5386, 0x88},
	{0x5387, 0x7c},
	{0x5388, 0x6c},
	{0x5389, 0x10},
	{0x538a, 0x01},
	{0x538b, 0x98},
	{0x5300, 0x08},
	{0x5301, 0x30},
	{0x5302, 0x10},
	{0x5303, 0x00},
	{0x5304, 0x08},
	{0x5305, 0x30},
	{0x5306, 0x08},
	{0x5307, 0x16},
	{0x5309, 0x08},
	{0x530a, 0x30},
	{0x530b, 0x04},
	{0x530c, 0x06},
	{0x5480, 0x01},
	{0x5481, 0x08},
	{0x5482, 0x14},
	{0x5483, 0x28},
	{0x5484, 0x51},
	{0x5485, 0x65},
	{0x5486, 0x71},
	{0x5487, 0x7d},
	{0x5488, 0x87},
	{0x5489, 0x91},
	{0x548a, 0x9a},
	{0x548b, 0xaa},
	{0x548c, 0xb8},
	{0x548d, 0xcd},
	{0x548e, 0xdd},
	{0x548f, 0xea},
	{0x5490, 0x1d},
	{0x5580, 0x02},
	{0x5583, 0x40},
	{0x5584, 0x10},
	{0x5589, 0x10},
	{0x558a, 0x00},
	{0x558b, 0xf8},
	{0x5800, 0x23},
	{0x5801, 0x14},
	{0x5802, 0x0f},
	{0x5803, 0x0f},
	{0x5804, 0x12},
	{0x5805, 0x26},
	{0x5806, 0x0c},
	{0x5807, 0x08},
	{0x5808, 0x05},
	{0x5809, 0x05},
	{0x580a, 0x08},
	{0x580b, 0x0d},
	{0x580c, 0x08},
	{0x580d, 0x03},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x03},
	{0x5811, 0x09},
	{0x5812, 0x07},
	{0x5813, 0x03},
	{0x5814, 0x00},
	{0x5815, 0x01},
	{0x5816, 0x03},
	{0x5817, 0x08},
	{0x5818, 0x0d},
	{0x5819, 0x08},
	{0x581a, 0x05},
	{0x581b, 0x06},
	{0x581c, 0x08},
	{0x581d, 0x0e},
	{0x581e, 0x29},
	{0x581f, 0x17},
	{0x5820, 0x11},
	{0x5821, 0x11},
	{0x5822, 0x15},
	{0x5823, 0x28},
	{0x5824, 0x46},
	{0x5825, 0x26},
	{0x5826, 0x08},
	{0x5827, 0x26},
	{0x5828, 0x64},
	{0x5829, 0x26},
	{0x582a, 0x24},
	{0x582b, 0x22},
	{0x582c, 0x24},
	{0x582d, 0x24},
	{0x582e, 0x06},
	{0x582f, 0x22},
	{0x5830, 0x40},
	{0x5831, 0x42},
	{0x5832, 0x24},
	{0x5833, 0x26},
	{0x5834, 0x24},
	{0x5835, 0x22},
	{0x5836, 0x22},
	{0x5837, 0x26},
	{0x5838, 0x44},
	{0x5839, 0x24},
	{0x583a, 0x26},
	{0x583b, 0x28},
	{0x583c, 0x42},
	{0x583d, 0xce},
	{0x5025, 0x00},
	{0x3a0f, 0x30},
	{0x3a10, 0x28},
	{0x3a1b, 0x30},
	{0x3a1e, 0x26},
	{0x3a11, 0x60},
	{0x3a1f, 0x14},
	{0x3008, 0x02},
	{0xffff, 0xff}
};

/*mipi lane bps : M*/
static struct resolution_param  prev_resolution_array[] = {
	{
		.frmsize            = {1920, 1080},
		.active_frmsize     = {1920, 1080},
		.active_fps         = 30,
		.lanes              = 2,
		.bps                = 672,
		.size_type          = SIZE_1920X1080,
		.reg_script         = OV5640_preview_1080P_script,
	}, {
		.frmsize            = {1280, 720},
		.active_frmsize     = {1280, 720},
		.active_fps         = 60,
		.lanes              = 2,
		.bps                = 672,
		.size_type          = SIZE_1280X720,
		.reg_script         = OV5640_preview_720P_script,
	},
};

static struct resolution_param  capture_resolution_array[] = {
	{
		.frmsize            = {2592, 1944},
		.active_frmsize     = {2592, 1944},
		.active_fps         = 15,
		.lanes              = 2,
		.bps                = 672,
		.size_type          = SIZE_2592X1944,
		.reg_script         = OV5640_capture_5M_script,
	},
};

int OV5640_download_firmware(struct ov5640_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int i = 0;

	while (1 && ov5640_have_opened) {
		if (OV5640_AF_firmware[i].val == 0xff &&
			OV5640_AF_firmware[i].addr == 0xffff) {
			pr_info("download firmware success in initial OV5640.\n");
			break;
		}
		if ((i2c_put_byte(client, OV5640_AF_firmware[i].addr,
			OV5640_AF_firmware[i].val)) < 0) {
			pr_info("fail in download firmware OV5640. i=%d\n", i);
			break;
		}
		i++;
	}
	return 0;
}

static enum camera_focus_mode_e start_focus_mode = CAM_FOCUS_MODE_RELEASE;
static int OV5640_AutoFocus(struct ov5640_device *dev, int focus_mode);

static void do_download(struct work_struct *work)
{
	struct ov5640_device *dev =
		container_of(work, struct ov5640_device, dl_work);
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int ret;
	int i = 10;

	mutex_lock(&firmware_mutex);
	if (ov5640_have_opened) {
		if (OV5640_download_firmware(dev) >= 0) {
			while (i-- && ov5640_have_opened) {
				ret = i2c_get_byte(client, 0x3029);
				if (ret == 0x70)
				break;
				else if (ret == -1)
					continue;
				msleep(20);
			}
		}
	}
	dev->firmware_ready = 1;
	mutex_unlock(&firmware_mutex);
	if (start_focus_mode) {
		OV5640_AutoFocus(dev, (int)start_focus_mode);
		start_focus_mode = CAM_FOCUS_MODE_RELEASE;
	}
}

void OV5640_init_regs(struct ov5640_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int i = 0;

	while (1) {
		if (OV5640_script[i].val == 0xff &&
			OV5640_script[i].addr == 0xffff) {
			pr_info("success in initial OV5640\n");
			break;
		}
		if ((i2c_put_byte(client, OV5640_script[i].addr,
			OV5640_script[i].val)) < 0) {
			pr_info("fail in initial OV5640\n");
			return;
		}
		i++;
	}

}

static unsigned long ov5640_preview_exposure;
static unsigned long ov5640_preview_extra_lines;
static unsigned long ov5640_gain;
static unsigned long ov5640_preview_maxlines;

static int Get_preview_exposure_gain(struct ov5640_device *dev)
{
	int rc = 0;
	unsigned int ret_l, ret_m, ret_h;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	ret_h = ret_m = ret_l = 0;
	ret_h = i2c_get_byte(client, 0x350c);
	ret_l = i2c_get_byte(client, 0x350d);
	ov5640_preview_extra_lines = ((ret_h << 8) + ret_l);
	i2c_put_byte(client, 0x3503, 0x07);
	ret_h = ret_m = ret_l = 0;
	ov5640_preview_exposure = 0;
	ret_h = i2c_get_byte(client, 0x3500);
	ret_m = i2c_get_byte(client, 0x3501);
	ret_l = i2c_get_byte(client, 0x3502);
	ov5640_preview_exposure = (ret_h << 12) + (ret_m << 4) + (ret_l >> 4);
	ret_h = ret_m = ret_l = 0;
	ov5640_preview_exposure =
	ov5640_preview_exposure + (ov5640_preview_extra_lines) / 16;
	ret_h = ret_m = ret_l = 0;
	ov5640_preview_maxlines = 0;
	ret_h = i2c_get_byte(client, 0x380e);
	ret_l = i2c_get_byte(client, 0x380f);
	ov5640_preview_maxlines = (ret_h << 8) + ret_l;
	ov5640_gain = 0;
	ov5640_gain = i2c_get_byte(client, 0x350b);

	return rc;
}

static int cal_exposure(struct ov5640_device *dev, int pre_fps, int cap_fps)
{
	int rc = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char ExposureLow, ExposureMid, ExposureHigh;
	unsigned char Capture_MaxLines_High, Capture_MaxLines_Low;
	unsigned int ret_l, ret_m, ret_h, Lines_10ms;
	unsigned short ulCapture_Exposure, iCapture_Gain;
	unsigned int ulCapture_Exposure_Gain, Capture_MaxLines;

	ret_h = ret_m = ret_l = 0;
	ret_h = i2c_get_byte(client, 0x380e);
	ret_l = i2c_get_byte(client, 0x380f);
	Capture_MaxLines = (ret_h << 8) + ret_l;
	Capture_MaxLines = Capture_MaxLines + (ov5640_preview_extra_lines) / 16;
	dprintk(dev, 3, "Capture_MaxLines=%d\n", Capture_MaxLines);
	if (ov5640_qctrl[7].default_value == CAM_BANDING_60HZ)
		Lines_10ms = cap_fps * Capture_MaxLines / 12000;
	else
		Lines_10ms = cap_fps * Capture_MaxLines / 10000;

	if (ov5640_preview_maxlines == 0)
		ov5640_preview_maxlines = 1;

	ulCapture_Exposure =
	(ov5640_preview_exposure * (cap_fps) * (Capture_MaxLines)) /
	(((ov5640_preview_maxlines) * (pre_fps)));
	iCapture_Gain = ov5640_gain;
	ulCapture_Exposure_Gain = ulCapture_Exposure * iCapture_Gain;
	if (ulCapture_Exposure_Gain < Capture_MaxLines * 16) {
		ulCapture_Exposure = ulCapture_Exposure_Gain / 16;
		if (ulCapture_Exposure > Lines_10ms) {
			ulCapture_Exposure /= Lines_10ms;
			ulCapture_Exposure *= Lines_10ms;
		}
	} else {
		ulCapture_Exposure = Capture_MaxLines;
	}
	if (ulCapture_Exposure == 0)
		ulCapture_Exposure = 1;

	iCapture_Gain = (ulCapture_Exposure_Gain * 2 /
		ulCapture_Exposure + 1) / 2;
	ExposureLow = ((unsigned char)ulCapture_Exposure) << 4;
	ExposureMid = (unsigned char)(ulCapture_Exposure >> 4) & 0xff;
	ExposureHigh = (unsigned char)(ulCapture_Exposure >> 12);
	Capture_MaxLines_Low = (unsigned char)(Capture_MaxLines & 0xff);
	Capture_MaxLines_High = (unsigned char)(Capture_MaxLines >> 8);
	i2c_put_byte(client, 0x380e, Capture_MaxLines_High);
	i2c_put_byte(client, 0x380f, Capture_MaxLines_Low);
	i2c_put_byte(client, 0x350b, iCapture_Gain);
	i2c_put_byte(client, 0x3502, ExposureLow);
	i2c_put_byte(client, 0x3501, ExposureMid);
	i2c_put_byte(client, 0x3500, ExposureHigh);
	dprintk(dev, 3, "iCapture_Gain=%d\n", iCapture_Gain);
	dprintk(dev, 3, "ExposureLow=%d\n", ExposureLow);
	dprintk(dev, 3, "ExposureMid=%d\n", ExposureMid);
	dprintk(dev, 3, "ExposureHigh=%d\n", ExposureHigh);

	return rc;
}

void OV5640_set_param_wb(struct ov5640_device *dev,
		enum camera_wb_flip_e para)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	switch (para) {
	case CAM_WB_AUTO:
		i2c_put_byte(client, 0x3212, 0x03);
		i2c_put_byte(client, 0x3406, 0x00);
		i2c_put_byte(client, 0x3400, 0x04);
		i2c_put_byte(client, 0x3401, 0x00);
		i2c_put_byte(client, 0x3402, 0x04);
		i2c_put_byte(client, 0x3403, 0x00);
		i2c_put_byte(client, 0x3404, 0x04);
		i2c_put_byte(client, 0x3405, 0x00);
		i2c_put_byte(client, 0x3212, 0x13);
		i2c_put_byte(client, 0x3212, 0xa3);

		break;

	case CAM_WB_CLOUD:
		i2c_put_byte(client, 0x3212, 0x03);
		i2c_put_byte(client, 0x3406, 0x01);
		i2c_put_byte(client, 0x3400, 0x06);
		i2c_put_byte(client, 0x3401, 0x48);
		i2c_put_byte(client, 0x3402, 0x04);
		i2c_put_byte(client, 0x3403, 0x00);
		i2c_put_byte(client, 0x3404, 0x04);
		i2c_put_byte(client, 0x3405, 0xd3);
		i2c_put_byte(client, 0x3212, 0x13);
		i2c_put_byte(client, 0x3212, 0xa3);

		break;

	case CAM_WB_DAYLIGHT:
		i2c_put_byte(client, 0x3212, 0x03);
		i2c_put_byte(client, 0x3406, 0x01);
		i2c_put_byte(client, 0x3400, 0x06);
		i2c_put_byte(client, 0x3401, 0x1c);
		i2c_put_byte(client, 0x3402, 0x04);
		i2c_put_byte(client, 0x3403, 0x00);
		i2c_put_byte(client, 0x3404, 0x04);
		i2c_put_byte(client, 0x3405, 0xf3);
		i2c_put_byte(client, 0x3212, 0x13);
		i2c_put_byte(client, 0x3212, 0xa3);

		break;

	case CAM_WB_INCANDESCENCE:
		i2c_put_byte(client, 0x3212, 0x03);
		i2c_put_byte(client, 0x3406, 0x01);
		i2c_put_byte(client, 0x3400, 0x04);
		i2c_put_byte(client, 0x3401, 0x10);
		i2c_put_byte(client, 0x3402, 0x04);
		i2c_put_byte(client, 0x3403, 0x00);
		i2c_put_byte(client, 0x3404, 0x08);
		i2c_put_byte(client, 0x3405, 0x40);
		i2c_put_byte(client, 0x3212, 0x13);
		i2c_put_byte(client, 0x3212, 0xa3);

		break;

	case CAM_WB_TUNGSTEN:
		i2c_put_byte(client, 0x3212, 0x03);
		i2c_put_byte(client, 0x3406, 0x01);
		i2c_put_byte(client, 0x3400, 0x04);
		i2c_put_byte(client, 0x3401, 0x10);
		i2c_put_byte(client, 0x3402, 0x04);
		i2c_put_byte(client, 0x3403, 0x00);
		i2c_put_byte(client, 0x3404, 0x08);
		i2c_put_byte(client, 0x3405, 0x40);
		i2c_put_byte(client, 0x3212, 0x13);
		i2c_put_byte(client, 0x3212, 0xa3);

		break;

	case CAM_WB_FLUORESCENT:
		i2c_put_byte(client, 0x3212, 0x03);
		i2c_put_byte(client, 0x3406, 0x01);
		i2c_put_byte(client, 0x3400, 0x05);
		i2c_put_byte(client, 0x3401, 0x48);
		i2c_put_byte(client, 0x3402, 0x04);
		i2c_put_byte(client, 0x3403, 0x00);
		i2c_put_byte(client, 0x3404, 0x07);
		i2c_put_byte(client, 0x3405, 0xcf);
		i2c_put_byte(client, 0x3212, 0x13);
		i2c_put_byte(client, 0x3212, 0xa3);
		break;

	case CAM_WB_MANUAL:
		break;
	default:
		break;
	}
}

void OV5640_set_param_exposure(struct ov5640_device *dev,
		enum camera_exposure_e para)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	switch (para) {
	case EXPOSURE_N4_STEP:
		i2c_put_byte(client, 0x3a0f, 0x10);
		i2c_put_byte(client, 0x3a10, 0x08);
		i2c_put_byte(client, 0x3a1b, 0x10);
		i2c_put_byte(client, 0x3a1e, 0x08);
		i2c_put_byte(client, 0x3a11, 0x20);
		i2c_put_byte(client, 0x3a1f, 0x10);
		break;

	case EXPOSURE_N3_STEP:
		i2c_put_byte(client, 0x3a0f, 0x18);
		i2c_put_byte(client, 0x3a10, 0x10);
		i2c_put_byte(client, 0x3a1b, 0x18);
		i2c_put_byte(client, 0x3a1e, 0x10);
		i2c_put_byte(client, 0x3a11, 0x30);
		i2c_put_byte(client, 0x3a1f, 0x10);
		break;

	case EXPOSURE_N2_STEP:
		i2c_put_byte(client, 0x3a0f, 0x20);
		i2c_put_byte(client, 0x3a10, 0x18);
		i2c_put_byte(client, 0x3a11, 0x41);
		i2c_put_byte(client, 0x3a1b, 0x20);
		i2c_put_byte(client, 0x3a1e, 0x18);
		i2c_put_byte(client, 0x3a1f, 0x10);
		break;

	case EXPOSURE_N1_STEP:
		i2c_put_byte(client, 0x3a0f, 0x28);
		i2c_put_byte(client, 0x3a10, 0x20);
		i2c_put_byte(client, 0x3a11, 0x51);
		i2c_put_byte(client, 0x3a1b, 0x28);
		i2c_put_byte(client, 0x3a1e, 0x20);
		i2c_put_byte(client, 0x3a1f, 0x10);
		break;

	case EXPOSURE_0_STEP:
		i2c_put_byte(client, 0x3a0f, 0x38);
		i2c_put_byte(client, 0x3a10, 0x30);
		i2c_put_byte(client, 0x3a11, 0x61);
		i2c_put_byte(client, 0x3a1b, 0x38);
		i2c_put_byte(client, 0x3a1e, 0x30);
		i2c_put_byte(client, 0x3a1f, 0x10);
		break;

	case EXPOSURE_P1_STEP:
		i2c_put_byte(client, 0x3a0f, 0x48);
		i2c_put_byte(client, 0x3a10, 0x40);
		i2c_put_byte(client, 0x3a11, 0x80);
		i2c_put_byte(client, 0x3a1b, 0x48);
		i2c_put_byte(client, 0x3a1e, 0x40);
		i2c_put_byte(client, 0x3a1f, 0x20);
		break;

	case EXPOSURE_P2_STEP:
		i2c_put_byte(client, 0x3a0f, 0x50);
		i2c_put_byte(client, 0x3a10, 0x48);
		i2c_put_byte(client, 0x3a11, 0x90);
		i2c_put_byte(client, 0x3a1b, 0x50);
		i2c_put_byte(client, 0x3a1e, 0x48);
		i2c_put_byte(client, 0x3a1f, 0x20);
		break;

	case EXPOSURE_P3_STEP:
		i2c_put_byte(client, 0x3a0f, 0x58);
		i2c_put_byte(client, 0x3a10, 0x50);
		i2c_put_byte(client, 0x3a11, 0x91);
		i2c_put_byte(client, 0x3a1b, 0x58);
		i2c_put_byte(client, 0x3a1e, 0x50);
		i2c_put_byte(client, 0x3a1f, 0x20);
		break;

	case EXPOSURE_P4_STEP:
		i2c_put_byte(client, 0x3a0f, 0x60);
		i2c_put_byte(client, 0x3a10, 0x58);
		i2c_put_byte(client, 0x3a11, 0xa0);
		i2c_put_byte(client, 0x3a1b, 0x60);
		i2c_put_byte(client, 0x3a1e, 0x58);
		i2c_put_byte(client, 0x3a1f, 0x20);
		break;

	default:
		i2c_put_byte(client, 0x3a0f, 0x38);
		i2c_put_byte(client, 0x3a10, 0x30);
		i2c_put_byte(client, 0x3a11, 0x61);
		i2c_put_byte(client, 0x3a1b, 0x38);
		i2c_put_byte(client, 0x3a1e, 0x30);
		i2c_put_byte(client, 0x3a1f, 0x10);
		break;
	}
}

void OV5640_set_param_effect(struct ov5640_device *dev,
		enum camera_effect_flip_e para)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	switch (para) {
	case CAM_EFFECT_ENC_NORMAL:
		i2c_put_byte(client, 0x5001, 0x03);
		break;

	case CAM_EFFECT_ENC_GRAYSCALE:
		i2c_put_byte(client, 0x5001, 0x83);
		i2c_put_byte(client, 0x5580, 0x20);
		break;

	case CAM_EFFECT_ENC_SEPIA:
		break;

	case CAM_EFFECT_ENC_SEPIAGREEN:
		break;

	case CAM_EFFECT_ENC_SEPIABLUE:
		break;

	case CAM_EFFECT_ENC_COLORINV:
		i2c_put_byte(client, 0x5001, 0x83);
		i2c_put_byte(client, 0x5580, 0x40);
		break;

	default:
		break;
	}
}

static void OV5640_set_param_banding(struct ov5640_device *dev,
		enum  camera_banding_flip_e banding)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	switch (banding) {
	case CAM_BANDING_60HZ:
		pr_info("set banding 60Hz\n");
		i2c_put_byte(client, 0x3c00, 0x00);
		break;
	case CAM_BANDING_50HZ:
		pr_info("set banding 50Hz\n");
		i2c_put_byte(client, 0x3c00, 0x04);
		break;
	default:
		break;
	}
}

static int OV5640_AutoFocus(struct ov5640_device *dev, int focus_mode)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int ret = 0;

	switch (focus_mode) {
	case CAM_FOCUS_MODE_AUTO:
		i2c_put_byte(client, 0x3023, 0x1);
		i2c_put_byte(client, 0x3022, 0x3);
		bDoingAutoFocusMode = true;
		pr_info("auto mode start\n");
		break;

	case CAM_FOCUS_MODE_CONTI_VID:
	case CAM_FOCUS_MODE_CONTI_PIC:
		i2c_put_byte(client, 0x3022, 0x4);
		i2c_put_byte(client, 0x3023, 0x1);
		pr_info("start continuous focus\n");
		break;

	case CAM_FOCUS_MODE_RELEASE:
	case CAM_FOCUS_MODE_FIXED:
	default:
		i2c_put_byte(client, 0x3023, 0x1);
		i2c_put_byte(client, 0x3022, 0x8);
		pr_info("release focus to infinit\n");
		break;
	}
	return ret;

}

static int OV5640_FlashCtrl(struct ov5640_device *dev, int flash_mode)
{
	int ret = 0;

	switch (flash_mode) {
	case FLASHLIGHT_ON:
	case FLASHLIGHT_AUTO:
		if (dev->cam_info.torch_support)
			aml_cam_torch(&dev->cam_info, 1);
		aml_cam_flash(&dev->cam_info, 1);
		break;
	case FLASHLIGHT_TORCH:
		if (dev->cam_info.torch_support) {
			aml_cam_torch(&dev->cam_info, 1);
			aml_cam_flash(&dev->cam_info, 0);
		} else
			aml_cam_torch(&dev->cam_info, 1);
		break;
	case FLASHLIGHT_OFF:
		aml_cam_flash(&dev->cam_info, 0);
		if (dev->cam_info.torch_support)
			aml_cam_torch(&dev->cam_info, 0);
		break;
	default:
		pr_info("this flash mode not support yet\n");
		break;
	}
	return ret;
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
	else if (width * height >= 300 * 200)
		rv = SIZE_320X240;
	else if (width * height >= 170 * 140)
		rv = SIZE_176X144;
	return rv;
}

static int set_flip(struct ov5640_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char temp;

	temp = i2c_get_byte(client, 0x3821);
	temp &= 0xf9;
	temp |= dev->cam_info.m_flip << 1 | dev->cam_info.m_flip << 2;
	if ((i2c_put_byte(client, 0x3821, temp)) < 0) {
		pr_info("fail in setting sensor orientation\n");
		return -1;
	}
	temp = i2c_get_byte(client, 0x3820);
	temp &= 0xf9;
	temp |= dev->cam_info.v_flip << 1 | dev->cam_info.v_flip << 2;
	if ((i2c_put_byte(client, 0x3820, temp)) < 0) {
		pr_info("fail in setting sensor orientation\n");
		return -1;
	}

	return 0;
}

static struct resolution_param *get_resolution_param(
		struct ov5640_device *dev,
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
		ov5640_frmintervals_active.denominator = 23;
		ov5640_frmintervals_active.numerator = 1;
	}

	for (i = 0; i < arry_size; i++) {
		if (tmp_resolution_param[i].size_type == res_type) {
			ov5640_frmintervals_active.denominator =
				tmp_resolution_param[i].active_fps;
			ov5640_frmintervals_active.numerator = 1;
			return &tmp_resolution_param[i];
		}
	}
	return NULL;
}

static int set_resolution_param(struct ov5640_device *dev,
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
			res_param->reg_script[i].addr == 0xffff) {
			pr_info("setting resolutin param complete\n");
			break;
		}
		if ((i2c_put_byte(client, res_param->reg_script[i].addr,
			res_param->reg_script[i].val)) < 0) {
			pr_info("fail in setting resolution param.i=%d\n", i);
			break;
		}
		i++;
	}
	dev->cur_resolution_param = res_param;
	set_flip(dev);

	return 0;
}

static int set_focus_zone(struct ov5640_device *dev, int value)
{
	int xc, yc;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int retry_count = 10;
	int ret = -1;

	xc = ((value >> 16) & 0xffff) * 80 / 2000;
	yc = (value & 0xffff) * 60 / 2000;
	pr_info("xc = %d, yc = %d\n", xc, yc);
	i2c_put_byte(client, CMD_PARA0, xc);
	i2c_put_byte(client, CMD_PARA1, yc);
	i2c_put_byte(client, CMD_ACK, 0x01);
	i2c_put_byte(client, CMD_MAIN, 0x81);

	do {
		msleep(20);
		pr_info("waiting for focus zone to be set\n");
	} while (i2c_get_byte(client, CMD_ACK) && retry_count--);

	if (retry_count)
		ret = 0;
	return ret;
}

unsigned char v4l_2_ov5640(int val)
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
		canvas = start_canvas | ((start_canvas+1)<<8);
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

static int ov5640_setting(struct ov5640_device *dev, int PROP_ID, int value)
{
	int ret = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	switch (PROP_ID) {
	case V4L2_CID_BRIGHTNESS:
		mutex_lock(&firmware_mutex);
		dprintk(dev, 1, "setting brightned:%d\n", v4l_2_ov5640(value));
		ret = i2c_put_byte(client, 0x0201, v4l_2_ov5640(value));
		mutex_unlock(&firmware_mutex);
		break;
	case V4L2_CID_CONTRAST:
		mutex_lock(&firmware_mutex);
		ret = i2c_put_byte(client, 0x0200, value);
		mutex_unlock(&firmware_mutex);
		break;
	case V4L2_CID_SATURATION:
		mutex_lock(&firmware_mutex);
		ret = i2c_put_byte(client, 0x0202, value);
		mutex_unlock(&firmware_mutex);
		break;
	case V4L2_CID_HFLIP:
		value = value & 0x3;
		if (ov5640_qctrl[2].default_value != value) {
			ov5640_qctrl[2].default_value = value;
			dprintk(dev, 3, "set camera h filp =%d\n", value);
		}
		break;
	case V4L2_CID_VFLIP:
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		mutex_lock(&firmware_mutex);
		if (ov5640_qctrl[4].default_value != value) {
			ov5640_qctrl[4].default_value = value;
			OV5640_set_param_wb(dev, value);
			dprintk(dev, 3, "set camera white_balance=%d\n", value);
		}
		mutex_unlock(&firmware_mutex);
		break;
	case V4L2_CID_EXPOSURE:
		mutex_lock(&firmware_mutex);
		if (ov5640_qctrl[5].default_value != value) {
			ov5640_qctrl[5].default_value = value;
			OV5640_set_param_exposure(dev, value);
			dprintk(dev, 3, "set camera exposure=%d\n", value);
		}
		mutex_unlock(&firmware_mutex);
		break;
	case V4L2_CID_COLORFX:
		mutex_lock(&firmware_mutex);
		if (ov5640_qctrl[6].default_value != value) {
			ov5640_qctrl[6].default_value = value;
			OV5640_set_param_effect(dev, value);
			dprintk(dev, 3, "set camera effect=%d\n", value);
		}
		mutex_unlock(&firmware_mutex);
		break;
	case V4L2_CID_WHITENESS:
		mutex_lock(&firmware_mutex);
		if (ov5640_qctrl[7].default_value != value) {
			ov5640_qctrl[7].default_value = value;
			OV5640_set_param_banding(dev, value);
			dprintk(dev, 3, "set camera banding=%d\n", value);
		}
		mutex_unlock(&firmware_mutex);
		break;
	case V4L2_CID_FOCUS_AUTO:
		mutex_lock(&firmware_mutex);
		if (ov5640_have_opened) {
			if (dev->firmware_ready)
				ret = OV5640_AutoFocus(dev, value);
			else if (value == CAM_FOCUS_MODE_CONTI_VID ||
					value == CAM_FOCUS_MODE_CONTI_PIC)
				start_focus_mode = value;
			else
				ret = -1;
		}
		mutex_unlock(&firmware_mutex);
		break;
	case V4L2_CID_BACKLIGHT_COMPENSATION:
		if (dev->cam_info.flash_support)
			ret = OV5640_FlashCtrl(dev, value);
		else
			ret = -1;
		break;
	case V4L2_CID_ZOOM_ABSOLUTE:
		if (ov5640_qctrl[10].default_value != value)
			ov5640_qctrl[10].default_value = value;
		break;
	case V4L2_CID_ROTATE:
		if (ov5640_qctrl[11].default_value != value) {
			ov5640_qctrl[11].default_value = value;
			dprintk(dev, 3, "set camera rotate =%d\n", value);
		}
		break;
	case V4L2_CID_FOCUS_ABSOLUTE:
		if (ov5640_qctrl[12].default_value != value) {
			ov5640_qctrl[12].default_value = value;
			dprintk(dev, 3, "set camera focus zone =%d\n", value);
			set_focus_zone(dev, value);
		}
		break;
	default:
		ret = -1;
		break;
	}
	return ret;

}

static void power_down_ov5640(struct ov5640_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	i2c_put_byte(client, 0x3022, 0x8);
	i2c_put_byte(client, 0x3008, 0x42);
}


#define TSTAMP_MIN_Y	24
#define TSTAMP_MAX_Y	(TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X	10
#define TSTAMP_MIN_X	(54 + TSTAMP_INPUT_X)

static void ov5640_fillbuff(struct ov5640_fh *fh, struct ov5640_buffer *buf)
{
	int ret;
	struct ov5640_device *dev = fh->dev;
	void *vbuf = (void *)videobuf_to_res(&buf->vb);
	struct vm_output_para para = {0};

	dprintk(dev, 1, "%s\n", __func__);
	if (!vbuf)
		return;

	if (buf->canvas_id == 0)
		buf->canvas_id = convert_canvas_index(fh->fmt->fourcc,
				OV5640_RES0_CANVAS_INDEX + buf->vb.i * 3);

	para.mirror = ov5640_qctrl[2].default_value & 3;
	para.v4l2_format = fh->fmt->fourcc;
	para.v4l2_memory = MAGIC_RE_MEM;
	para.zoom = ov5640_qctrl[10].default_value;
	para.angle = ov5640_qctrl[11].default_value;
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

static void ov5640_thread_tick(struct ov5640_fh *fh)
{
	struct ov5640_buffer *buf;
	struct ov5640_device *dev = fh->dev;
	struct ov5640_dmaqueue *dma_q = &dev->vidq;

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
		struct ov5640_buffer, vb.queue);
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
	ov5640_fillbuff(fh, buf);
	dprintk(dev, 1, "filled buffer %p\n", buf);

	wake_up(&buf->vb.done);
	dprintk(dev, 2, "[%p/%d] wakeup\n", buf, buf->vb. i);
	return;
unlock:
	spin_unlock_irqrestore(&dev->slock, flags);
}

static void ov5640_sleep(struct ov5640_fh *fh)
{
	struct ov5640_device *dev = fh->dev;
	struct ov5640_dmaqueue *dma_q = &dev->vidq;

	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 1, "%s dma_q=0x%08lx\n", __func__,
		(unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
		goto stop_task;

	ov5640_thread_tick(fh);

	schedule_timeout_interruptible(1);

stop_task:
	remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int ov5640_thread(void *data)
{
	struct ov5640_fh  *fh = data;
	struct ov5640_device *dev = fh->dev;

	dprintk(dev, 1, "thread started\n");

	set_freezable();

	for (;;) {
		ov5640_sleep(fh);

		if (kthread_should_stop())
			break;
	}
	dprintk(dev, 1, "thread: exit\n");
	return 0;
}

static int ov5640_start_thread(struct ov5640_fh *fh)
{
	struct ov5640_device *dev = fh->dev;
	struct ov5640_dmaqueue *dma_q = &dev->vidq;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;

	dprintk(dev, 1, "%s\n", __func__);

	dma_q->kthread = kthread_run(ov5640_thread, fh, "ov5640");

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}

	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void ov5640_stop_thread(struct ov5640_dmaqueue  *dma_q)
{
	struct ov5640_device *dev =
		container_of(dma_q, struct ov5640_device, vidq);

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
	struct ov5640_fh *fh  = container_of(res, struct ov5640_fh, res);
	struct ov5640_device *dev  = fh->dev;
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

static void free_buffer(struct videobuf_queue *vq, struct ov5640_buffer *buf)
{
	struct videobuf_res_privdata *res = vq->priv_data;
	struct ov5640_fh *fh  = container_of(res, struct ov5640_fh, res);
	struct ov5640_device *dev  = fh->dev;

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
	struct ov5640_fh *fh  = container_of(res, struct ov5640_fh, res);
	struct ov5640_device    *dev = fh->dev;
	struct ov5640_buffer *buf = container_of(vb, struct ov5640_buffer, vb);
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
	struct ov5640_buffer *buf  =
		container_of(vb, struct ov5640_buffer, vb);
	struct videobuf_res_privdata *res = vq->priv_data;
	struct ov5640_fh *fh  = container_of(res, struct ov5640_fh, res);
	struct ov5640_device       *dev  = fh->dev;
	struct ov5640_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
		struct videobuf_buffer *vb)
{
	struct ov5640_buffer *buf  =
		container_of(vb, struct ov5640_buffer, vb);
	struct videobuf_res_privdata *res = vq->priv_data;
	struct ov5640_fh *fh  = container_of(res, struct ov5640_fh, res);
	struct ov5640_device *dev = (struct ov5640_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops ov5640_video_qops = {
	.buf_setup      = buffer_setup,
	.buf_prepare    = buffer_prepare,
	.buf_queue      = buffer_queue,
	.buf_release    = buffer_release,
};

static int vidioc_querycap(struct file *file, void  *priv,
		struct v4l2_capability *cap)
{
	struct ov5640_fh  *fh  = priv;
	struct ov5640_device *dev = fh->dev;

	strcpy(cap->driver, "ov5640");
	strcpy(cap->card, "ov5640.canvas");
	if (dev->cam_info.front_back == 0)
		strcat(cap->card, "back");
	else
		strcat(cap->card, "front");

	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = OV5640_CAMERA_VERSION;
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
				| V4L2_CAP_READWRITE;

	cap->capabilities =	cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
		struct v4l2_fmtdesc *f)
{
	struct ov5640_fmt *fmt;

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
	struct ov5640_fh *fh = priv;

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

	if (fival->index > ARRAY_SIZE(ov5640_frmivalenum))
		return -EINVAL;

	for (k = 0; k < ARRAY_SIZE(ov5640_frmivalenum); k++) {
		if ((fival->index == ov5640_frmivalenum[k].index) &&
			(fival->pixel_format ==
				ov5640_frmivalenum[k].pixel_format) &&
			(fival->width == ov5640_frmivalenum[k].width) &&
			(fival->height == ov5640_frmivalenum[k].height)) {
			memcpy(fival, &ov5640_frmivalenum[k],
				sizeof(struct v4l2_frmivalenum));
			return 0;
		}
	}

	return -EINVAL;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_format *f)
{
	struct ov5640_fh  *fh  = priv;
	struct ov5640_device *dev = fh->dev;
	struct ov5640_fmt *fmt;
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
	struct ov5640_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;
	struct ov5640_device *dev = fh->dev;
	struct resolution_param *res_param = NULL;
	int ret;
	int cap_fps, pre_fps;

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

		Get_preview_exposure_gain(dev);
		set_resolution_param(dev, res_param);

		if (prev_res && (prev_res->size_type == SIZE_1280X960
				|| prev_res->size_type == SIZE_1024X768)) {
			pre_fps = 1500;
		} else if (prev_res && prev_res->size_type == SIZE_1280X720) {
			pre_fps = 3000;
		} else {
			pre_fps = 1500;
		}
		cap_fps = 500;
		cal_exposure(dev, pre_fps, cap_fps);
		dprintk(dev, 3, "pre_fps=%d,cap_fps=%d\n", pre_fps, cap_fps);
	} else {
		res_param = get_resolution_param(dev, 0, fh->width, fh->height);
		if (!res_param) {
			pr_err("error, resolution param not get\n");
			goto out;
		}
		set_resolution_param(dev, res_param);
		prev_res = res_param;
	}

	ret = 0;
out:
	mutex_unlock(&q->vb_lock);

	return ret;
}

static int vidioc_g_parm(struct file *file, void *priv,
		struct v4l2_streamparm *parms)
{
	struct ov5640_fh *fh = priv;
	struct ov5640_device *dev = fh->dev;
	struct v4l2_captureparm *cp = &parms->parm.capture;

	dprintk(dev, 3, "vidioc_g_parm\n");
	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;

	cp->timeperframe = ov5640_frmintervals_active;
	pr_info("g_parm,deno=%d, numerator=%d\n", cp->timeperframe.denominator,
	cp->timeperframe.numerator);
	return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
		struct v4l2_requestbuffers *p)
{
	struct ov5640_fh  *fh = priv;

	return videobuf_reqbufs(&fh->vb_vidq, p);
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ov5640_fh  *fh = priv;
	int ret = videobuf_querybuf(&fh->vb_vidq, p);

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_M8) {
		if (ret == 0) {
			p->reserved  =
			convert_canvas_index(fh->fmt->fourcc,
			OV5640_RES0_CANVAS_INDEX + p->index * 3);
		} else {
			p->reserved = 0;
		}
	}

	return ret;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ov5640_fh *fh = priv;

	return videobuf_qbuf(&fh->vb_vidq, p);
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ov5640_fh  *fh = priv;

	return (videobuf_dqbuf(&fh->vb_vidq, p,
		file->f_flags & O_NONBLOCK));
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct ov5640_fh  *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct ov5640_fh  *fh = priv;
	struct ov5640_device *dev = fh->dev;
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
			ov5640_frmintervals_active.denominator;
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
	para.dfmt = TVIN_NV21;
	para.hsync_phase = 1;
	para.vsync_phase = 1;
	para.bt_path = dev->cam_info.bt_path;
	/*config mipi parameter*/
	para.csi_hw_info.settle = fh->dev->cur_resolution_param->bps;
	para.csi_hw_info.lanes = fh->dev->cur_resolution_param->lanes;

	pr_info("ov5640 stream on.\n");
	ret =  videobuf_streamon(&fh->vb_vidq);
	if (ret == 0) {
		pr_info("ov5640 start tvin service.\n");
		vops->start_tvin_service(vdin_path, &para);
		fh->stream_on = 1;
	}
	OV5640_set_param_wb(dev, ov5640_qctrl[4].default_value);
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct ov5640_fh  *fh = priv;
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
	struct ov5640_fmt *fmt = NULL;
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
	struct ov5640_fh *fh = priv;
	struct ov5640_device *dev = fh->dev;

	*i = dev->input;

	return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct ov5640_fh *fh = priv;
	struct ov5640_device *dev = fh->dev;

	dev->input = i;

	return 0;
}

static int vidioc_queryctrl(struct file *file, void *priv,
			struct v4l2_queryctrl *qc)
{
	int i;
	struct ov5640_fh *fh = priv;
	struct ov5640_device *dev = fh->dev;

	if (!dev->cam_info.flash_support
			&& qc->id == V4L2_CID_BACKLIGHT_COMPENSATION)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(ov5640_qctrl); i++)
		if (qc->id && qc->id == ov5640_qctrl[i].id) {
			memcpy(qc, &(ov5640_qctrl[i]),
				sizeof(*qc));
			if (ov5640_qctrl[i].type == V4L2_CTRL_TYPE_MENU)
				return ov5640_qctrl[i].maximum + 1;
			else
				return 0;
		}

	return -EINVAL;
}

static int vidioc_querymenu(struct file *file, void *priv,
		struct v4l2_querymenu *a)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(ov5640_qmenu_set); i++)
		if (a->id && a->id == ov5640_qmenu_set[i].id) {
			for (j = 0; j < ov5640_qmenu_set[i].num; j++)
				if (a->index ==
				ov5640_qmenu_set[i].ov5640_qmenu[j].index) {
					memcpy(a,
					&(ov5640_qmenu_set[i].ov5640_qmenu[j]),
					sizeof(*a));
					return 0;
				}
		}

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			struct v4l2_control *ctrl)
{
	struct ov5640_fh *fh = priv;
	struct ov5640_device *dev = fh->dev;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int i;
	int i2cret = -1;

	for (i = 0; i < ARRAY_SIZE(ov5640_qctrl); i++)
		if (ctrl->id == ov5640_qctrl[i].id) {
			if ((ctrl->id == V4L2_CID_FOCUS_AUTO)
					&& bDoingAutoFocusMode) {
				if (i2c_get_byte(client, 0x3023))
					return -EBUSY;
				bDoingAutoFocusMode = false;
				if (i2c_get_byte(client, 0x3028) == 0) {
					dprintk(dev, 3,
					"auto mode failed!\n");
					return -EAGAIN;
				}
				i2c_put_byte(client,
						0x3022, 0x6);
				i2c_put_byte(client,
						0x3023, 0x1);
				dprintk(dev, 3,
					"pause auto focus\n");
			} else if (ctrl->id == V4L2_CID_AUTO_FOCUS_STATUS) {
				i2cret = i2c_get_byte(client, 0x3029);
				if (i2cret == 0x00) {
					ctrl->value =
					V4L2_AUTO_FOCUS_STATUS_BUSY;
				} else if (i2cret == 0x10) {
					ctrl->value =
					V4L2_AUTO_FOCUS_STATUS_REACHED;
				} else if (i2cret == 0x20) {
					ctrl->value =
					V4L2_AUTO_FOCUS_STATUS_IDLE;
				} else {
					dprintk(dev, 3,
					"should resart focus\n");
					ctrl->value =
						V4L2_AUTO_FOCUS_STATUS_FAILED;
				}
				return 0;
			}
			ctrl->value = dev->qctl_regs[i];
			return 0;
		}

	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
			struct v4l2_control *ctrl)
{
	struct ov5640_fh *fh = priv;
	struct ov5640_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(ov5640_qctrl); i++)
		if (ctrl->id == ov5640_qctrl[i].id) {
			if (ctrl->value < ov5640_qctrl[i].minimum ||
			ctrl->value > ov5640_qctrl[i].maximum ||
			ov5640_setting(dev, ctrl->id, ctrl->value) < 0) {
				return -ERANGE;
			}
			dev->qctl_regs[i] = ctrl->value;
			return 0;
		}
	return -EINVAL;
}

static int ov5640_open(struct file *file)
{
	struct ov5640_device *dev = video_drvdata(file);
	struct ov5640_fh *fh = NULL;
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
	ov5640_have_opened = 1;
	mutex_unlock(&firmware_mutex);

	aml_cam_init(&dev->cam_info);

	OV5640_init_regs(dev);

	msleep(20);

	mutex_lock(&dev->mutex);
	dev->users++;
	if (dev->users > 1) {
		dev->users--;
		mutex_unlock(&dev->mutex);
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

	if (retval)
		return retval;

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
		videobuf_queue_res_init(&fh->vb_vidq, &ov5640_video_qops,
						NULL, &dev->slock, fh->type,
						V4L2_FIELD_INTERLACED,
						sizeof(struct ov5640_buffer),
						(void *)&fh->res, NULL);
	}

	bDoingAutoFocusMode = false;
	ov5640_start_thread(fh);
	return 0;
}

static ssize_t
ov5640_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct ov5640_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
				file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int
ov5640_poll(struct file *file, struct poll_table_struct *wait)
{
	struct ov5640_fh        *fh = file->private_data;
	struct ov5640_device       *dev = fh->dev;
	struct videobuf_queue *q = &fh->vb_vidq;

	dprintk(dev, 1, "%s\n", __func__);

	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int ov5640_close(struct file *file)
{
	struct ov5640_fh         *fh = file->private_data;
	struct ov5640_device *dev       = fh->dev;
	struct ov5640_dmaqueue *vidq = &dev->vidq;
	struct video_device  *vdev = video_devdata(file);
	unsigned int vdin_path;

	vdin_path = fh->dev->cam_info.vdin_path;
	mutex_lock(&firmware_mutex);
	ov5640_have_opened = 0;
	dev->firmware_ready = 0;
	mutex_unlock(&firmware_mutex);
	ov5640_stop_thread(vidq);
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

	ov5640_qctrl[4].default_value = 0;
	ov5640_qctrl[5].default_value = 4;
	ov5640_qctrl[6].default_value = 0;
	ov5640_qctrl[2].default_value = 0;
	ov5640_qctrl[10].default_value = 100;
	ov5640_qctrl[11].default_value = 0;
	temp_frame = -1;
	power_down_ov5640(dev);
	ov5640_frmintervals_active.numerator = 1;
	ov5640_frmintervals_active.denominator = 25;

	aml_cam_uninit(&dev->cam_info);
#ifdef CONFIG_HAS_WAKELOCK
	wake_unlock(&(dev->wake_lock));
#endif
#ifdef CONFIG_CMA
	vm_deinit_resource(&dev->vminfo);
#endif
	return 0;
}

static int ov5640_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ov5640_fh  *fh = file->private_data;
	struct ov5640_device *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end-(unsigned long)vma->vm_start,
		ret);

	return ret;
}

static const struct v4l2_file_operations ov5640_fops = {
	.owner	    = THIS_MODULE,
	.open       = ov5640_open,
	.release    = ov5640_close,
	.read       = ov5640_read,
	.poll	    = ov5640_poll,
	.unlocked_ioctl      = video_ioctl2,
	.mmap       = ov5640_mmap,
};

static const struct v4l2_ioctl_ops ov5640_ioctl_ops = {
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

static struct video_device ov5640_template = {
	.name	        = "ov5640_v4l",
	.fops           = &ov5640_fops,
	.ioctl_ops      = &ov5640_ioctl_ops,
	.release        = video_device_release,
	.tvnorms        = V4L2_STD_525_60,
};

static const struct v4l2_subdev_core_ops ov5640_core_ops = {
};

static const struct v4l2_subdev_ops ov5640_ops = {
	.core = &ov5640_core_ops,
};

static int ov5640_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct aml_cam_info_s *plat_dat;
	int err;
	int ret;
	struct ov5640_device *t;
	struct v4l2_subdev *sd;

	vops = get_vdin_v4l2_ops();
	v4l_info(client, "chip found @ 0x%x (%s)\n",
		client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL) {
		pr_err("ov5640 probe kzalloc failed.\n");
		return -ENOMEM;
	}

	client->addr = 0x3c;
	snprintf(t->v4l2_dev.name, sizeof(t->v4l2_dev.name),
		"%s-%03d", "ov5640", 0);
	ret = v4l2_device_register(NULL, &t->v4l2_dev);
	if (ret) {
		pr_info("%s, v4l2 device register failed", __func__);
		kfree(t);
		return ret;
	}

	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &ov5640_ops);
	mutex_init(&t->mutex);
	memset(&t->vminfo, 0, sizeof(struct vm_init_s));

	t->vdev = video_device_alloc();
	if (t->vdev == NULL) {
		kfree(t);
		return -ENOMEM;
	}
	memcpy(t->vdev, &ov5640_template, sizeof(*t->vdev));
	t->vdev->v4l2_dev = &t->v4l2_dev;
	video_set_drvdata(t->vdev, t);

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&(t->wake_lock), WAKE_LOCK_SUSPEND, "ov5640");
#endif

	plat_dat = (struct aml_cam_info_s *)client->dev.platform_data;
	if (plat_dat) {
		memcpy(&t->cam_info, plat_dat, sizeof(struct aml_cam_info_s));
		pr_info("%s, front_back = %d\n",
			__func__, plat_dat->front_back);
		video_nr = plat_dat->front_back;
	} else {
		pr_err("camera ov5640: have no platform data\n");
		video_device_release(t->vdev);
		kfree(t);
		return -1;
	}

	t->cam_info.version = OV5640_DRIVER_VERSION;
	if (aml_cam_info_reg(&t->cam_info) < 0)
		pr_err("reg caminfo error\n");

	pr_info("t->vdev = %p, video_nr = %d\n", t->vdev, video_nr);
	err = video_register_device(t->vdev, VFL_TYPE_GRABBER, video_nr);
	if (err < 0) {
		video_device_release(t->vdev);
		kfree(t);
		return err;
	}

	pr_info("ov5640_probe successful.\n");

	return 0;
}

static int ov5640_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5640_device *t = to_dev(sd);

	video_unregister_device(t->vdev);
	v4l2_device_unregister_subdev(sd);
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&(t->wake_lock));
#endif
	aml_cam_info_unreg(&t->cam_info);
	kfree(t);
	return 0;
}

static const struct i2c_device_id ov5640_id[] = {
	{ "ov5640", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov5640_id);

static struct i2c_driver ov5640_i2c_driver = {
	.driver = {
		.name = "ov5640",
	},
	.probe = ov5640_probe,
	.remove = ov5640_remove,
	.id_table = ov5640_id,
};

module_i2c_driver(ov5640_i2c_driver);

