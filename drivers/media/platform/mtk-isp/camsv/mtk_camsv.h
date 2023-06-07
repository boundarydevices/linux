/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 BayLibre
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_CAMSV_H__
#define __MTK_CAMSV_H__

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/of_graph.h>
#include <linux/pm_runtime.h>
#include <linux/videodev2.h>
#include <media/media-entity.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-v4l2.h>
#include <soc/mediatek/smi.h>

#define IMG_MAX_WIDTH			5376
#define IMG_MAX_HEIGHT			4032
#define IMG_DEF_WIDTH			1920
#define IMG_DEF_HEIGHT			1080
#define IMG_MIN_WIDTH			80
#define IMG_MIN_HEIGHT			60

enum TEST_MODE { TEST_PATTERN_DISABLED = 0x0, TEST_PATTERN_SENINF };

#define MTK_CAM_CIO_PAD_SENINF		0
#define MTK_CAM_CIO_PAD_VIDEO		1
#define MTK_CAM_CIO_NUM_PADS		2

struct mtk_cam_format_info {
	u32 code;
	u32 fourcc;
	bool packed;
	unsigned int bpp;
};

struct mtk_cam_dev_buffer {
	struct vb2_v4l2_buffer v4l2_buf;
	struct list_head list;
	dma_addr_t daddr;
	dma_addr_t fhaddr;
};

struct mtk_cam_sparams {
	unsigned int w_factor;
	unsigned int module_en_pak;
	unsigned int fmt_sel;
	unsigned int pak;
	unsigned int imgo_stride;
};

/*
 * struct mtk_cam_vdev_desc - MTK camera device descriptor
 *
 * @name: name of the node
 * @cap: supported V4L2 capabilities
 * @buf_type: supported V4L2 buffer type
 * @link_flags: default media link flags
 * @def_width: the default format width
 * @def_height: the default format height
 * @num_fmts: the number of supported node formats
 * @max_buf_count: maximum VB2 buffer count
 * @ioctl_ops:  mapped to v4l2_ioctl_ops
 * @fmts: supported format
 * @frmsizes: supported V4L2 frame size number
 *
 */
struct mtk_cam_vdev_desc {
	const char *name;
	u32 cap;
	u32 buf_type;
	u32 link_flags;
	u32 def_width;
	u32 def_height;
	u8 num_fmts;
	u8 max_buf_count;
	const struct v4l2_ioctl_ops *ioctl_ops;
	const u32 *fmts;
	const struct v4l2_frmsizeenum *frmsizes;
};

/*
 * struct mtk_cam_video_device - MediaTek video device structure
 *
 * @desc: The node description of video device
 * @vdev_pad: The media pad graph object of video device
 * @vdev: The video device instance
 * @vbq: A videobuf queue of video device
 * @vdev_lock: Serializes vb2 queue and video device operations
 * @format: The V4L2 format of video device
 * @fmtinfo: Information about the current format
 */
struct mtk_cam_video_device {
	const struct mtk_cam_vdev_desc *desc;

	struct media_pad vdev_pad;
	struct video_device vdev;
	struct vb2_queue vbq;

	/* Serializes vb2 queue and video device operations */
	struct mutex vdev_lock;

	struct v4l2_pix_format_mplane format;
	const struct mtk_cam_format_info *fmtinfo;
};

/*
 * struct mtk_cam_dev - MediaTek camera device structure.
 *
 * @dev: Pointer to device.
 * @pipeline: Media pipeline information.
 * @subdev: The V4L2 sub-device instance.
 * @subdev_pads: Media pads of this sub-device.
 * @formats: Media bus format for all pads.
 * @vdev: The video device node.
 * @seninf: Pointer to the seninf pad.
 * @streaming: Indicate the overall streaming status is on or off.
 * @stream_count: Number of streaming video nodes
 * @sequence: Buffer sequence number
 * @op_lock: Serializes driver's VB2 callback operations.
 *
 */
struct mtk_cam_dev {
	struct device *dev;
	void __iomem *regs;
	void __iomem *regs_img0;
	void __iomem *regs_tg;
	unsigned int num_clks;
	struct clk_bulk_data *clks;
	struct device *larb_ipu;
	struct device *larb_cam;
	unsigned int irq;
	const struct mtk_cam_conf *conf;

	struct media_pipeline pipeline;
	struct v4l2_subdev subdev;
	struct media_pad subdev_pads[MTK_CAM_CIO_NUM_PADS];
	struct v4l2_mbus_framefmt formats[MTK_CAM_CIO_NUM_PADS];
	struct mtk_cam_video_device vdev;
	struct media_pad *seninf;
	unsigned int streaming;
	unsigned int stream_count;
	unsigned int sequence;

	struct mutex op_lock;
	struct mutex protect_mutex;
	spinlock_t irqlock;

	struct list_head buf_list;

	struct mtk_cam_hw_functions *hw_functions;

	struct mtk_cam_dev_buffer dummy;
	unsigned int dummy_size;
	bool is_dummy_used;
};

struct mtk_cam_conf {
	unsigned int tg_sen_mode;
	unsigned int module_en;
	unsigned int pak;
	unsigned int dma_special_fun;
	unsigned int imgo_con;
	unsigned int imgo_con2;
	unsigned int imgo_con3;
	bool enableFH;
};

struct mtk_cam_hw_functions {
	void (*mtk_cam_setup)(struct mtk_cam_dev *cam_dev, u32 width,
			      u32 height, u32 bpl, u32 mbus_fmt);
	void (*mtk_cam_update_buffers_add)(struct mtk_cam_dev *cam_dev,
					   struct mtk_cam_dev_buffer *buf);
	void (*mtk_cam_cmos_vf_hw_enable)(struct mtk_cam_dev *cam_dev,
					  bool pak_en);
	void (*mtk_cam_cmos_vf_hw_disable)(struct mtk_cam_dev *cam_dev,
					   bool pak_en);
};

int mtk_cam_dev_init(struct mtk_cam_dev *cam_dev);
void mtk_cam_dev_cleanup(struct mtk_cam_dev *cam_dev);
int mtk_cam_video_register(struct mtk_cam_dev *cam_dev);
void mtk_cam_video_unregister(struct mtk_cam_video_device *vdev);

#endif /* __MTK_CAMSV_H__ */
