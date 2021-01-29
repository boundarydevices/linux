/*
 *    VSI V4L2 kernel driver private header file.
 *
 *    Copyright (c) 2019, VeriSilicon Inc.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License, version 2, as
 *    published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License version 2 for more details.
 *
 *    You may obtain a copy of the GNU General Public License
 *    Version 2 at the following locations:
 *    https://opensource.org/licenses/gpl-2.0.php
 */

#ifndef VSI_V4L2_PRIV_H
#define VSI_V4L2_PRIV_H

#include <linux/version.h>
#include <linux/v4l2-controls.h>
#include <linux/imx_vpu.h>
#include "vsi-v4l2.h"

#define CTX_SEQID_UPLIMT 0x7FFFFFFF
#define CTX_ARRAY_ID(ctxid)	(ctxid & 0xFFFFFFFF)
#define CTX_SEQ_ID(ctxid)	(ctxid >> 32)

#define MIN_FRAME_4ENC	1

#define MAX_MIN_BUFFERS_FOR_CAPTURE	16
#define MAX_MIN_BUFFERS_FOR_OUTPUT	16

#define ENC_EXTRA_HEADER_SIZE 1024

#define DEFAULT_GOP_SIZE	1
#define DEFAULT_INTRA_PIC_RATE	30
#define DEFAULT_QP			30

#if KERNEL_VERSION(5, 5, 0) > LINUX_VERSION_CODE
#define VSI_DEVTYPE	VFL_TYPE_GRABBER
#else
#define VSI_DEVTYPE	VFL_TYPE_VIDEO
#endif

/* declarations */
extern struct idr vsi_inst_array;
extern int vsi_kloglvl;

enum {
	LOGLVL_ERROR = 0,
	LOGLVL_WARNING,
	LOGLVL_BRIEF,
	LOGLVL_VERBOSE,
};

#define v4l2_pr(lvl, fmt, ...) {\
	if (lvl == LOGLVL_ERROR)	\
		pr_err(fmt, ...);	\
	else if (lvl >= vsi_kloglvl)		\
		pr_info(fmt, ...);	\
}

/*this table should be consistent with that in hevcencapi.h*/
enum VCEncPictureType {
	VCENC_FMT_INVALID = -1,
	VCENC_YUV420_PLANAR = 0,                  /* YYYY... UUUU... VVVV...  */
	VCENC_YUV420_SEMIPLANAR = 1,              /* YYYY... UVUVUV...        */
	VCENC_YUV420_SEMIPLANAR_VU = 2,           /* YYYY... VUVUVU...        */
	VCENC_YUV422_INTERLEAVED_YUYV = 3,        /* YUYVYUYV...              */
	VCENC_YUV422_INTERLEAVED_UYVY = 4,        /* UYVYUYVY...              */
	VCENC_RGB565 = 5,                         /* 16-bit RGB 16bpp         */
	VCENC_BGR565 = 6,                         /* 16-bit RGB 16bpp         */
	VCENC_RGB555 = 7,                         /* 15-bit RGB 16bpp         */
	VCENC_BGR555 = 8,                         /* 15-bit RGB 16bpp         */
	VCENC_RGB888 = 11,                         /* 24-bit RGB 32bpp         */
	VCENC_BGR888 = 12,                         /* 24-bit BGR 32bpp         */
};

enum VCEncPictureRotation {
	VCENC_ROTATE_0 = 0,
	VCENC_ROTATE_90R = 1, /* Rotate 90 degrees clockwise */
	VCENC_ROTATE_90L = 2,  /* Rotate 90 degrees counter-clockwise */
	VCENC_ROTATE_180R = 3  /* Rotate 180 degrees clockwise */
};

enum VCEncProfile {
	VCENC_HEVC_MAIN_PROFILE = 0,
	VCENC_HEVC_MAIN_STILL_PICTURE_PROFILE = 1,
	VCENC_HEVC_MAIN_10_PROFILE = 2,
	/* H264 Defination*/
	VCENC_H264_BASE_PROFILE = 9,
	VCENC_H264_MAIN_PROFILE = 10,
	VCENC_H264_HIGH_PROFILE = 11,
	VCENC_H264_HIGH_10_PROFILE = 12,
	/* AV1 Defination*/
	VCENC_AV1_MAIN_PROFILE = 0, /*4:2:0 8/10 bit*/
	VCENC_AV1_HIGH_PROFILE = 1,
	VCENC_AV1_PROFESSIONAL_PROFILE = 2,

	/*Vp9 Defination*/
	VCENC_VP9_MAIN_PROFILE = 0, /*4:2:0 8 bit No SRGB*/
	VCENC_VP9_MSRGB_PROFILE = 1, /*4:2:2 4:4:0 4:4:4 8 bit SRGB*/
	VCENC_VP9_HIGH_PROFILE = 2, /*4:2:0 10/12 bit No SRGB*/
	VCENC_VP9_HSRGB_PROFILE = 3, /*4:2:2 4:4:0 4:4:4 10 bit SRGB*/
};

#ifndef V4L2_MPEG_VIDEO_H264_LEVEL_5_2
#define V4L2_MPEG_VIDEO_H264_LEVEL_5_2	16
#endif
/* Picture color space conversion (RGB input) for pre-processing, used for colorConversion below*/
enum VCEncColorConversionType {
	VCENC_RGBTOYUV_BT601 = 0, /* Color conversion of limited range[16,235] according to BT.601 */
	VCENC_RGBTOYUV_BT709 = 1, /* Color conversion of limited range[16,235] according to BT.709 */
	VCENC_RGBTOYUV_USER_DEFINED = 2,   /* User defined color conversion */
	VCENC_RGBTOYUV_BT2020 = 3, /* Color conversion according to BT.2020 */
	VCENC_RGBTOYUV_BT601_FULL_RANGE = 4, /* Color conversion of full range[0,255] according to BT.601*/
	VCENC_RGBTOYUV_BT601_LIMITED_RANGE = 5, /* Color conversion of limited range[0,219] according to BT.601*/
	VCENC_RGBTOYUV_BT709_FULL_RANGE = 6 /* Color conversion of full range[0,255] according to BT.709*/
};

/****************************************************************/
/* Extension of color macros copied from NXP ticket 464. In new kernel version they may be removed */

#define V4L2_COLORSPACE_GENERIC_FILM  (V4L2_COLORSPACE_DCI_P3+1)
#define V4L2_COLORSPACE_ST428			(V4L2_COLORSPACE_DCI_P3+2)

#define V4L2_XFER_FUNC_LINEAR			(V4L2_XFER_FUNC_SMPTE2084+1)
#define V4L2_XFER_FUNC_GAMMA22		(V4L2_XFER_FUNC_SMPTE2084+2)
#define V4L2_XFER_FUNC_GAMMA28		(V4L2_XFER_FUNC_SMPTE2084+3)
#define V4L2_XFER_FUNC_HLG				(V4L2_XFER_FUNC_SMPTE2084+4)
#define V4L2_XFER_FUNC_XVYCC			(V4L2_XFER_FUNC_SMPTE2084+5)
#define V4L2_XFER_FUNC_BT1361			(V4L2_XFER_FUNC_SMPTE2084+6)
#define V4L2_XFER_FUNC_ST428			(V4L2_XFER_FUNC_SMPTE2084+7)

#define V4L2_YCBCR_ENC_BT470_6M		(V4L2_YCBCR_ENC_SMPTE240M+1)

/****************************************************************/
/************************ compound extension ctrl defines ***************************/
#define VSI_V4L2_CMPTYPE_ROI		(V4L2_CTRL_COMPOUND_TYPES + 100)
#define VSI_V4L2_CMPTYPE_IPCM		(V4L2_CTRL_COMPOUND_TYPES + 101)

/*V4L2 status following spec*/
enum CTX_STATUS {
	VSI_STATUS_INIT = 0,	/*init is a public state for dec and enc*/
	ENC_STATUS_ENCODING,
	ENC_STATUS_DRAINING,
	ENC_STATUS_STOPPED,
	ENC_STATUS_STOPPED_BYUSR,
	ENC_STATUS_RESET,

	DEC_STATUS_DECODING,
	DEC_STATUS_DRAINING,
	DEC_STATUS_STOPPED,
	DEC_STATUS_SEEK,
	DEC_STATUS_CAPSETUP,
	DEC_STATUS_ENDSTREAM,
};

struct vsi_v4l2_mem_info_internal {
	ulong size;
	dma_addr_t  busaddr;
	void *vaddr;
	struct page *pageaddr;
};

struct vsi_video_fmt {
	char *name;
	u32 fourcc;	//V4L2 video format defines
	s32 enc_fmt;	//our own enc video format defines
	s32 dec_fmt;	//our own dec video format defines
	u32 flag;
};

struct vsi_v4l2_mediacfg {

	/*from ctrls setting*/
	//unsigned int gopsize;
	//move to encparams.specific.enc_h26x_cmd.gopSize
	//unsigned int avcprofile;
	//move to encparams.specific.enc_h26x_cmd.profile

	/* from set format */
	//unsigned int width;
	//move to encparams.general.width
	//unsigned int height;
	//move to encparams.general.height
	//unsigned int pixelformat;
	//move to encparams.general.inputFormat
	u32 field;		/* enum v4l2_field */
	u32 srcplanes;
	u32 dstplanes;
	u32 bytesperline;	/* for padding, zero if unused */
	u32 sizeimagesrc[VB2_MAX_PLANES];	/*this is for input plane size*/
	u32 sizeimagedst[VB2_MAX_PLANES];	/*this is for output plane size*/
	u32 colorspace;	/* enum v4l2_colorspace */
	u32 priv;		/* depends on pixelformat */
	u32 flags;		/* format flags (V4L2_PIX_FMT_FLAG_*) */
	u32 quantization;	/* enum v4l2_quantization */
	u32 xfer_func;	/* enum v4l2_xfer_func */
	u32 minbuf_4capture;
	u32 minbuf_4output;
	u32 multislice_mode;
	u32 infmt_fourcc;
	u32 outfmt_fourcc;
	/*profiles for each format is put here instead of encparams to save some transfer data*/
	s32 profile_h264;
	s32 profile_hevc;
	s32 profile_vp8;
	s32 profile_vp9;

	/*for vidioc_s/g_parm*/
	struct v4l2_captureparm capparam;
	struct v4l2_outputparm   outputparam;

	/*roi and ipcm temp info */
	struct v4l2_enc_roi_params roiinfo;
	struct v4l2_enc_ipcm_params ipcminfo;

	/*internal storage*/
	struct v4l2_daemon_enc_params encparams;
	struct v4l2_daemon_dec_params decparams;
	struct v4l2_daemon_dec_params decparams_bkup;
};

struct vsi_v4l2_device {
	struct v4l2_device v4l2_dev;
	struct platform_device *pdev;
	struct device *dev;
	struct video_device *venc;
	struct video_device *vdec;
	struct mutex lock;
	struct mutex irqlock;
};

struct vsi_vpu_buf {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
};

struct vsi_queued_buf {
	struct v4l2_buffer qb;
	struct list_head list;
	struct v4l2_plane planes[VB2_MAX_PLANES];
};

/*ctx flags*/
#define CTX_FLAG_ENC				(0 << 0)
#define CTX_FLAG_DEC				(1 << 0)

enum {
	CTX_FLAG_PRE_DRAINING_BIT = 1,	//decoder_cmd stop has comes
	CTX_FLAG_ENDOFSTRM_BIT,			// got last flag received from daemon
	CTX_FLAG_CROPCHANGE_BIT,			// got crop change msg from daemon
	CTX_FLAG_DAEMONLIVE_BIT,			// has send msg to daemon (so daemon has live instance for this ctx
	CTX_FLAG_CONFIGUPDATE_BIT,		// need update info to daemon (for enc)
	CTX_FLAG_FORCEIDR_BIT,			// force idr invoked
	CTX_FLAG_SRCCHANGED_BIT,			// src change has come from daemon
	CTX_FLAG_DELAY_SRCCHANGED_BIT,	// src change has come from daemon	 but not sent to app
};

/* flag for decoder buffer*/
enum {
	BUF_FLAG_QUEUED = 0,		/*buf queued from app*/
	BUF_FLAG_DONE,			/*buf returned from daemon*/
};

struct vsi_v4l2_ctx {
	struct v4l2_fh fh;
	struct vsi_v4l2_device *dev;
	ulong ctxid;
	struct mutex ctxlock;

	s32 status;		/*hold current status*/
	s32 error;
	ulong flag;

	struct vb2_queue input_que;
	struct list_head input_list;
	struct vb2_queue output_que;
	struct list_head output_list;
	ulong vbufflag[VIDEO_MAX_FRAME];
	ulong srcvbufflag[VIDEO_MAX_FRAME];

	u32 inbufbytes[VIDEO_MAX_FRAME];
	u32 inbuflen[VIDEO_MAX_FRAME];
	u32 outbuflen[VIDEO_MAX_FRAME];
	u32 queued_srcnum;
	u32 buffed_capnum;
	u32 buffed_cropcapnum;

	struct list_head queued_list;

	struct vsi_v4l2_mediacfg mediacfg;

	struct v4l2_ctrl_handler ctrlhdl;
	wait_queue_head_t retbuf_queue;

	uint64_t frameidx;

	atomic_t srcframen;
	atomic_t dstframen;
};

int vsi_setup_ctrls(struct v4l2_ctrl_handler *handler);
int v4l2_release(struct file *filp);
void vsi_remove_ctx(struct vsi_v4l2_ctx *ctx);
struct vsi_v4l2_ctx *vsi_create_ctx(void);
int vsi_v4l2_reset_ctx(struct vsi_v4l2_ctx *ctx);
int vsi_v4l2_send_reschange(struct vsi_v4l2_ctx *ctx);
int vsi_v4l2_notify_reschange(struct vsi_v4l2_msg *pmsg);
int vsi_v4l2_handle_warningmsg(struct vsi_v4l2_msg *pmsg);
int vsi_v4l2_handle_cropchange(struct vsi_v4l2_msg *pmsg);
int vsi_v4l2_bufferdone(struct vsi_v4l2_msg *pmsg);
int vsi_v4l2_handleerror(unsigned long ctxtid, int error);
int vsi_v4l2_handle_picconsumed(unsigned long ctxid);
struct video_device *v4l2_probe_enc(
	struct platform_device *pdev,
	struct vsi_v4l2_device *vpu);
void v4l2_release_enc(struct video_device *venc);
struct video_device *v4l2_probe_dec(struct platform_device *pdev, struct vsi_v4l2_device *vpu);
void v4l2_release_dec(struct video_device *vdec);

u64 vsi_v4l2_getbandwidth(void);
int vsiv4l2_initdaemon(void);
void vsiv4l2_cleanupdaemon(void);
int vsi_clear_daemonmsg(int instid);
int vsiv4l2_execcmd(
	struct vsi_v4l2_ctx *ctx,
	enum v4l2_daemon_cmd_id id,
	void *args);
int vsi_v4l2_addinstance(pid_t *ppid);
int vsi_v4l2_quitinstance(void);
int vsi_v4l2_daemonalive(void);

void dec_updatevui(struct v4l2_daemon_dec_info *src, struct v4l2_daemon_dec_info *dst);
void dec_getvui(struct v4l2_format *v4l2fmt, struct v4l2_daemon_dec_info *decinfo);
void vsi_enum_encfsize(struct v4l2_frmsizeenum *f, u32 pixel_format);
void vsiv4l2_initcfg(struct vsi_v4l2_ctx *ctx);
int get_Level(struct vsi_v4l2_ctx *ctx, int mediatype, int dir, int level);
int vsiv4l2_setfmt(struct vsi_v4l2_ctx *ctx, struct v4l2_format *fmt);
int vsiv4l2_getfmt(struct vsi_v4l2_ctx *ctx, struct v4l2_format *fmt);
void vsiv4l2_buffer_config(
	struct vsi_v4l2_ctx *ctx,
	int type,
	unsigned int *nbuffers,
	unsigned int *nplanes,
	unsigned int sizes[]
);
struct vsi_video_fmt *vsi_find_format(struct vsi_v4l2_ctx *ctx, struct v4l2_format *f);
struct vsi_video_fmt *vsi_get_format_dec(int idx, int braw, struct vsi_v4l2_ctx *ctx);
struct vsi_video_fmt *vsi_get_format_enc(int idx, int braw);
int vsi_set_profile(struct vsi_v4l2_ctx *ctx, int type, int profile);
int vsi_get_profile(struct vsi_v4l2_ctx *ctx, int type);
void vsiv4l2_set_hwinfo(struct vsi_v4l2_dev_info *hwinfo);
struct vsi_v4l2_dev_info *vsiv4l2_get_hwinfo(void);
int vsiv4l2_setROI(struct vsi_v4l2_ctx *ctx, void *params);
int vsiv4l2_setIPCM(struct vsi_v4l2_ctx *ctx, void *params);
int vsiv4l2_getROIcount(void);
int vsiv4l2_getIPCMcount(void);
void convertROI(struct vsi_v4l2_ctx *ctx);
void convertIPCM(struct vsi_v4l2_ctx *ctx);
int vsiv4l2_verifycrop(struct v4l2_selection *s);
void vsi_v4l2_update_ctrlcfg(struct v4l2_ctrl_config *cfg);

static inline int isencoder(struct vsi_v4l2_ctx *ctx)
{
	return ((ctx->flag & CTX_FLAG_DEC) == 0);
}

static inline int isdecoder(struct vsi_v4l2_ctx *ctx)
{
	return ((ctx->flag & CTX_FLAG_DEC) != 0);
}

static inline int isvalidtype(int buftype, int ctxtype)
{
	if (((ctxtype & CTX_FLAG_DEC) == 0) &&
		(buftype == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ||
		buftype == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE))
		return 1;
	if (((ctxtype & CTX_FLAG_DEC) != 0) &&
		(buftype == V4L2_BUF_TYPE_VIDEO_OUTPUT ||
		buftype == V4L2_BUF_TYPE_VIDEO_CAPTURE))
		return 1;
	return 0;
}

static inline int binputqueue(int qtype)
{
	return V4L2_TYPE_IS_OUTPUT(qtype);
}

static inline int brawfmt(int ctxtype, int qtype)
{
	if ((((ctxtype & CTX_FLAG_DEC) == 0) && qtype == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) ||
		(((ctxtype & CTX_FLAG_DEC) != 0) && qtype == V4L2_BUF_TYPE_VIDEO_CAPTURE))
		return 1;
	return 0;
}

static inline struct vsi_vpu_buf *vb_to_vsibuf(struct vb2_buffer *vb)
{
	return container_of(to_vb2_v4l2_buffer(vb), struct vsi_vpu_buf, vb);
}

static inline struct vsi_v4l2_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct vsi_v4l2_ctx, fh);
}

static inline struct vsi_v4l2_ctx *ctrl_to_ctx(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct vsi_v4l2_ctx, ctrlhdl);
}

static inline void printbufinfo(struct vb2_queue *vq)
{
	int i;
	struct vb2_buffer *vb;
	struct vsi_vpu_buf *buf, *node;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(vq->drv_priv);

	pr_debug("#################################################");
	pr_debug("que has %d vb2 buffers, que count = %d", vq->num_buffers, vq->queued_count);
	for (i = 0; i < vq->num_buffers; i++) {
		vb = vq->bufs[i];
		pr_debug("vb2 buffer %d:num planes = %d", i, vb->num_planes);
	}
	pr_debug("input_list:");
	if (!list_empty(&ctx->input_list)) {
		list_for_each_entry_safe(buf, node, &ctx->input_list, list) {
			pr_debug("list node %lx", (unsigned long)buf);
		}
	}
	pr_debug("output_list:");
	if (!list_empty(&ctx->output_list)) {
		list_for_each_entry_safe(buf, node, &ctx->output_list, list) {
			pr_debug("list node %lx", (unsigned long)buf);
		}
	}
	pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
}

static inline void return_all_buffers(struct vb2_queue *vq, int status, int bRelbuf)
{
	int i;
	struct vsi_vpu_buf *buf, *node;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(vq->drv_priv);
	struct list_head *plist;

	pr_debug("%s", __func__);
	if (mutex_lock_interruptible(&ctx->ctxlock))
		return;
	if (binputqueue(vq->type))
		plist = &ctx->input_list;
	else
		plist = &ctx->output_list;

	for (i = 0; i < vq->num_buffers; ++i) {
		if (vq->bufs[i]->state == VB2_BUF_STATE_ACTIVE)
			vb2_buffer_done(vq->bufs[i], status);
	}
	if (bRelbuf) {
		list_for_each_entry_safe(buf, node, plist, list) {
			for (i = 0; i < buf->vb.vb2_buf.num_planes; i++)
				vb2_set_plane_payload(&buf->vb.vb2_buf, i, 0);
			list_del(&buf->list);
		}
	}
	mutex_unlock(&ctx->ctxlock);
}

static inline void print_queinfo(struct vb2_queue *q)
{
	int i, k;

	pr_debug("got %d buffer", q->num_buffers);
	for (i = 0; i < q->num_buffers; i++) {
		struct vb2_buffer	*buf = q->bufs[i];

		pr_debug("buf %d%p has %d planes", i, buf, buf->num_planes);
		for (k = 0; k < buf->num_planes; k++) {
			int *data = vb2_plane_vaddr(buf, k);

			pr_debug("plane %d = %lx, size = %x, offset = %x",
				k, (unsigned long)data, buf->planes[k].length, buf->planes[k].m.offset);
		}
	}
}

#endif	//VSI_V4L2_PRIV_H

