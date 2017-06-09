/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_drv.c
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

/* Standard Linux Headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/mm.h>
/* #include <asm/fiq.h> */
#include <asm/div64.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>
/* Amlogic Headers */
/* #include <linux/amlogic/media/canvas/canvas.h> */
/* #include <mach/am_regs.h> */
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
/* #include <linux/amlogic/aml_common.h> */
/* #include <mach/irqs.h> */
/* #include <mach/mod_gate.h> */
/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
/* #include <mach/vpu.h> */
/* #endif */
/* Local Headers */
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "../tvin_frontend.h"
#include "../tvin_global.h"
#include "vdin_regs.h"
#include "vdin_drv.h"
#include "vdin_ctl.h"
#include "vdin_sm.h"
#include "vdin_vf.h"
#include "vdin_canvas.h"


#define VDIN_NAME		"vdin"
#define VDIN_DRV_NAME		"vdin"
#define VDIN_MOD_NAME		"vdin"
#define VDIN_DEV_NAME		"vdin"
#define VDIN_CLS_NAME		"vdin"
#define PROVIDER_NAME		"vdin"

#define SMOOTH_DEBUG

#define VDIN_PUT_INTERVAL		(HZ/100)   /* 10ms, #define HZ 100 */

#define INVALID_VDIN_INPUT		0xffffffff

#define VDIN0_ALLOC_SIZE		SZ_64M

static dev_t vdin_devno;
static struct class *vdin_clsp;

static unsigned int vdin_addr_offset[VDIN_MAX_DEVS] = {0, 0x80};
struct vdin_dev_s *vdin_devp[VDIN_MAX_DEVS];
static int callmaster_status;

/*
 * canvas_config_mode
 * 0: canvas_config in driver probe
 * 1: start cofig
 * 2: auto config
 */
static int canvas_config_mode = 2;
module_param(canvas_config_mode, int, 0664);
MODULE_PARM_DESC(canvas_config_mode, "canvas configure mode");

static bool work_mode_simple;
module_param(work_mode_simple, bool, 0664);
MODULE_PARM_DESC(work_mode_simple, "enable/disable simple work mode");

static char *first_field_type;
module_param(first_field_type, charp, 0664);
MODULE_PARM_DESC(first_field_type, "first field type in simple work mode");

static int max_ignore_frames = 2;
module_param(max_ignore_frames, int, 0664);
MODULE_PARM_DESC(max_ignore_frames, "ignore first <n> frames");

static int ignore_frames;
module_param(ignore_frames, int, 0664);
MODULE_PARM_DESC(ignore_frames, "ignore first <n> frames");

static int start_provider_delay;
module_param(start_provider_delay, int, 0664);
MODULE_PARM_DESC(start_provider_delay, "ignore first <n> frames");
static bool vdin_dbg_en;
module_param(vdin_dbg_en, bool, 0664);
MODULE_PARM_DESC(vdin_dbg_en, "enable/disable vdin debug information");

static bool time_en;
module_param(time_en, bool, 0664);
MODULE_PARM_DESC(time_en, "enable/disable vdin debug information");
static bool invert_top_bot;
module_param(invert_top_bot, bool, 0644);
MODULE_PARM_DESC(invert_top_bot, "invert field type top or bottom");

/*
 * the check flag in vdin_isr
 * bit0:bypass stop check,bit1:bypass cyc check
 * bit2:bypass vsync check,bit3:bypass vga check
 */
static unsigned int isr_flag;
module_param(isr_flag, uint, 0664);
MODULE_PARM_DESC(isr_flag, "flag which affect the skip field");

static unsigned int irq_cnt;
module_param(irq_cnt, uint, 0664);
MODULE_PARM_DESC(irq_cnt, "counter of irq");

static unsigned int rdma_irq_cnt;
module_param(rdma_irq_cnt, uint, 0664);
MODULE_PARM_DESC(rdma_irq_cnt, "counter of rdma irq");

static unsigned int vdin_irq_flag;
module_param(vdin_irq_flag, uint, 0664);
MODULE_PARM_DESC(vdin_irq_flag, "vdin_irq_flag");

/* viu isr select:
 * enable viu_hw_irq for the bandwidth is enough on gxbb/gxtvbb and laters ic
 */
static unsigned int viu_hw_irq = 1;
module_param(viu_hw_irq, uint, 0664);
MODULE_PARM_DESC(viu_hw_irq, "viu_hw_irq");

static unsigned int vdin_reset_flag;
module_param(vdin_reset_flag, uint, 0664);
MODULE_PARM_DESC(vdin_reset_flag, "vdin_reset_flag");

static unsigned int vdin_reset_flag1;
module_param(vdin_reset_flag1, uint, 0664);
MODULE_PARM_DESC(vdin_reset_flag1, "vdin_reset_flag1");

/*
 * 2:enable manul rdam
 * 1:enable auto rdma
 * 0:no support rdma
 */
static unsigned int rdma_enable;
module_param(rdma_enable, uint, 0664);
MODULE_PARM_DESC(rdma_enable, "rdma_enable");


static int irq_max_count;
static void vdin_backup_histgram(struct vframe_s *vf, struct vdin_dev_s *devp);

char *vf_get_receiver_name(const char *provider_name);

static void vdin_set_drm_data(struct vdin_dev_s *devp,
		struct vframe_s *vf)
{
	if (devp->prop.hdr_info.hdr_state == HDR_STATE_GET) {
		memcpy(vf->prop.master_display_colour.primaries,
			devp->prop.hdr_info.hdr_data.primaries,
			sizeof(u32)*6);
		memcpy(vf->prop.master_display_colour.white_point,
			&devp->prop.hdr_info.hdr_data.white_points,
			sizeof(u32)*2);
		memcpy(vf->prop.master_display_colour.luminance,
			&devp->prop.hdr_info.hdr_data.master_lum,
			sizeof(u32)*2);

		vf->prop.master_display_colour.present_flag = true;

		if ((devp->prop.hdr_info.hdr_data.eotf == EOTF_SMPTE_ST_2048) ||
			(devp->prop.hdr_info.hdr_data.eotf == EOTF_HDR)) {
			vf->signal_type |= (1 << 29);
			vf->signal_type |= (0 << 25);/*0:limit*/
			vf->signal_type = ((9 << 16) |
				(vf->signal_type & (~0xFF0000)));
			vf->signal_type = ((16 << 8) |
				(vf->signal_type & (~0xFF00)));
			vf->signal_type = ((9 << 0) |
				(vf->signal_type & (~0xFF)));
		} else {
			vf->signal_type &= ~(1 << 29);
			vf->signal_type &= ~(1 << 25);
		}

		devp->prop.vdin_hdr_Flag = true;

		devp->prop.hdr_info.hdr_state = HDR_STATE_SET;
	} else if (devp->prop.hdr_info.hdr_state == HDR_STATE_NULL) {
		devp->prop.vdin_hdr_Flag = false;
		vf->prop.master_display_colour.present_flag = false;
		vf->signal_type &= ~(1 << 29);
		vf->signal_type &= ~(1 << 25);
	}
}

static u32 vdin_get_curr_field_type(struct vdin_dev_s *devp)
{
	u32 field_status;
	u32 type = VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD;
	enum vdin_format_convert_e	format_convert;

	/* struct tvin_parm_s *parm = &devp->parm; */
	const struct tvin_format_s *fmt_info = devp->fmt_info_p;

	if (fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) {
		type |= VIDTYPE_PROGRESSIVE;
	} else {
		field_status = vdin_get_field_type(devp->addr_offset);
		if (invert_top_bot)
			type |=	field_status ?
			VIDTYPE_INTERLACE_TOP :	VIDTYPE_INTERLACE_BOTTOM;
		else
			type |= field_status ?
			VIDTYPE_INTERLACE_BOTTOM : VIDTYPE_INTERLACE_TOP;
	}
	format_convert = devp->format_convert;
	if ((format_convert == VDIN_FORMAT_CONVERT_YUV_YUV444) ||
			(format_convert == VDIN_FORMAT_CONVERT_RGB_YUV444))
		type |= VIDTYPE_VIU_444;
	else if ((format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422) ||
			(format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422))
		type |= VIDTYPE_VIU_422;
	else if (devp->prop.dest_cfmt == TVIN_NV21) {
		type |= VIDTYPE_VIU_NV21;
		type &= (~VIDTYPE_VIU_SINGLE_PLANE);
	} else if (devp->prop.dest_cfmt == TVIN_NV12) {
		/* type |= VIDTYPE_VIU_NV12; */
		type &= (~VIDTYPE_VIU_SINGLE_PLANE);

	}
	return type;
}

void set_invert_top_bot(bool invert_flag)
{
	invert_top_bot = invert_flag;
}

void vdin_timer_func(unsigned long arg)
{
	struct vdin_dev_s *devp = (struct vdin_dev_s *)arg;

	/* state machine routine */
	tvin_smr(devp);
	/* add timer */
	devp->timer.expires = jiffies + VDIN_PUT_INTERVAL;
	add_timer(&devp->timer);
}

static const struct vframe_operations_s vdin_vf_ops = {
	.peek = vdin_vf_peek,
	.get  = vdin_vf_get,
	.put  = vdin_vf_put,
	.vf_states = vdin_vf_states,
};

#ifdef CONFIG_CMA
void vdin_cma_alloc(struct vdin_dev_s *devp)
{
	char vdin_name[5];
	unsigned int mem_size, h_size;
	int flags = CODEC_MM_FLAGS_CMA_FIRST|CODEC_MM_FLAGS_CMA_CLEAR|
		CODEC_MM_FLAGS_CPU;
	if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_RGB) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_RGB)) {
		if (devp->source_bitdepth > 8)
			h_size = roundup(devp->h_active * 4, 32);
		else
			h_size = roundup(devp->h_active * 3, 32);
	} else {
		/* txl new add mode yuv422 pack mode:canvas-w=h*2*10/8
		 * canvas_w must ensure divided exact by 256bit(32byte)
		 */
		if ((devp->source_bitdepth > 8) &&
		((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422)) &&
		(devp->color_depth_mode == 1))
			h_size = roundup((devp->h_active * 5)/2, 32);
		else if ((devp->source_bitdepth > 8) &&
			(devp->color_depth_mode == 0))
			h_size = roundup(devp->h_active * 3, 32);
		else
			h_size = roundup(devp->h_active * 2, 32);
	}
	mem_size = h_size * devp->v_active;
	mem_size = PAGE_ALIGN(mem_size)*max_buf_num;
	mem_size = (mem_size/PAGE_SIZE + 1)*PAGE_SIZE;
	if (mem_size > devp->cma_mem_size[devp->index])
		mem_size = devp->cma_mem_size[devp->index];
	if (devp->cma_config_en == 0)
		return;
	if (devp->cma_mem_alloc[devp->index] == 1)
		return;
	devp->cma_mem_alloc[devp->index] = 1;
	if (devp->cma_config_flag == 1) {
		if (devp->index == 0)
			strcpy(vdin_name, "vdin0");
		else if (devp->index == 1)
			strcpy(vdin_name, "vdin1");
		devp->mem_start = codec_mm_alloc_for_dma(vdin_name,
			mem_size/PAGE_SIZE, 0, flags);
		devp->mem_size = mem_size;
		if (devp->mem_start == 0)
			pr_err("\nvdin%d codec alloc fail!!!\n",
				devp->index);
		else {
			pr_info("vdin%d mem_start = 0x%x, mem_size = 0x%x\n",
				devp->index, devp->mem_start, devp->mem_size);
			pr_info("vdin%d codec cma alloc ok!\n", devp->index);
		}
	} else if (devp->cma_config_flag == 0) {
		devp->venc_pages[devp->index] = dma_alloc_from_contiguous(
			&(devp->this_pdev[devp->index]->dev),
			devp->cma_mem_size[devp->index] >> PAGE_SHIFT, 0);
		if (devp->venc_pages) {
			devp->mem_start =
				page_to_phys(devp->venc_pages[devp->index]);
			devp->mem_size  = mem_size;
			pr_info("vdin%d mem_start = 0x%x, mem_size = 0x%x\n",
				devp->index, devp->mem_start, devp->mem_size);
			pr_info("vdin%d cma alloc ok!\n", devp->index);
		} else {
			pr_err("\nvdin%d cma mem undefined2.\n",
				devp->index);
		}
	}
}

void vdin_cma_release(struct vdin_dev_s *devp)
{
	char vdin_name[5];

	if (devp->cma_config_en == 0)
		return;
	if ((devp->cma_config_flag == 1) && devp->mem_start
		&& (devp->cma_mem_alloc[devp->index] == 1)) {
		if (devp->index == 0)
			strcpy(vdin_name, "vdin0");
		else if (devp->index == 1)
			strcpy(vdin_name, "vdin1");
		codec_mm_free_for_dma(vdin_name, devp->mem_start);
		devp->mem_start = 0;
		devp->mem_size = 0;
		devp->cma_mem_alloc[devp->index] = 0;
		pr_info("vdin%d codec cma release ok!\n", devp->index);
	} else if (devp->venc_pages[devp->index]
		&& devp->cma_mem_size[devp->index]
		&& (devp->cma_mem_alloc[devp->index] == 1)
		&& (devp->cma_config_flag == 0)) {
		devp->cma_mem_alloc[devp->index] = 0;
		dma_release_from_contiguous(
			&(devp->this_pdev[devp->index]->dev),
			devp->venc_pages[devp->index],
			devp->cma_mem_size[devp->index] >> PAGE_SHIFT);
		pr_info("vdin%d cma release ok!\n", devp->index);
	}
}
#endif
/*
 * 1. find the corresponding frontend according to the port & save it.
 * 2. set default register, including:
 *		a. set default write canvas address.
 *		b. mux null input.
 *		c. set clock auto.
 *		a&b will enable hw work.
 * 3. call the callback function of the frontend to open.
 * 4. regiseter provider.
 * 5. create timer for state machine.
 *
 * port: the port supported by frontend
 * index: the index of frontend
 * 0 success, otherwise failed
 */
int vdin_open_fe(enum tvin_port_e port, int index,  struct vdin_dev_s *devp)
{
	struct tvin_frontend_s *fe = tvin_get_frontend(port, index);
	int ret = 0;

	if (!fe) {
		pr_err("%s(%d): not supported port 0x%x\n",
				__func__, devp->index, port);
		return -1;
	}

	devp->frontend = fe;
	devp->parm.port        = port;
	/* for atv snow function */
	if ((port == TVIN_PORT_CVBS3) &&
		(devp->parm.info.fmt == TVIN_SIG_FMT_NULL))
		devp->parm.info.fmt = TVIN_SIG_FMT_CVBS_NTSC_M;
	else
		devp->parm.info.fmt = TVIN_SIG_FMT_NULL;
	devp->parm.info.status = TVIN_SIG_STATUS_NULL;
	if (devp->pre_info.status != TVIN_SIG_STATUS_NULL) {
		devp->pre_info.status = TVIN_SIG_STATUS_NULL;
		#if 0
		switch_set_state(&devp->sig_sdev, TVIN_SIG_STATUS_NULL);
		#endif
	}
	devp->dec_enable = 1;  /* enable decoder */
	/* clear color para*/
	memset(&devp->pre_prop, 0, sizeof(devp->pre_prop));
	/* clear color para*/
	memset(&devp->prop, 0, sizeof(devp->prop));

	vdin_set_default_regmap(devp->addr_offset);

	/* vdin msr clock gate enable */
	clk_prepare_enable(devp->msr_clk);

	if (devp->frontend->dec_ops && devp->frontend->dec_ops->open)
		ret = devp->frontend->dec_ops->open(devp->frontend, port);
	/* check open status */
	if (ret)
		return 1;
	/* init vdin state machine */
	tvin_smr_init(devp->index);
	init_timer(&devp->timer);
	devp->timer.data = (ulong) devp;
	devp->timer.function = vdin_timer_func;
	devp->timer.expires = jiffies + VDIN_PUT_INTERVAL;
	add_timer(&devp->timer);

	pr_info("%s port:0x%x ok\n", __func__, port);
	return 0;
}

/*
 * 1. disable hw work, including:
 *		a. mux null input.
 *		b. set clock off.
 * 2. delete timer for state machine.
 * 3. unregiseter provider & notify receiver.
 * 4. call the callback function of the frontend to close.
 *
 */
void vdin_close_fe(struct vdin_dev_s *devp)
{
	/* avoid null pointer oops */
	if (!devp || !devp->frontend) {
		pr_info("%s: null pointer\n", __func__);
		return;
	}
	devp->dec_enable = 0;  /* disable decoder */

	/* bt656 clock gate disable */
	clk_disable_unprepare(devp->msr_clk);

	vdin_hw_disable(devp->addr_offset);
	del_timer_sync(&devp->timer);
	if (devp->frontend && devp->frontend->dec_ops->close) {
		devp->frontend->dec_ops->close(devp->frontend);
		devp->frontend = NULL;
	}
	devp->parm.port = TVIN_PORT_NULL;
	devp->parm.info.fmt  = TVIN_SIG_FMT_NULL;
	devp->parm.info.status = TVIN_SIG_STATUS_NULL;
	if (devp->pre_info.status != TVIN_SIG_STATUS_NULL) {
		devp->pre_info.status = TVIN_SIG_STATUS_NULL;
		#if 0
		switch_set_state(&devp->sig_sdev, TVIN_SIG_STATUS_NULL);
		#endif
	}

	pr_info("%s ok\n", __func__);
}

static inline void vdin_set_source_type(struct vdin_dev_s *devp,
		struct vframe_s *vf)
{
	switch (devp->parm.port) {
	case TVIN_PORT_CVBS3:
	case TVIN_PORT_CVBS0:
		vf->source_type = VFRAME_SOURCE_TYPE_TUNER;
		break;
	case TVIN_PORT_CVBS1:
	case TVIN_PORT_CVBS2:
	case TVIN_PORT_CVBS4:
	case TVIN_PORT_CVBS5:
	case TVIN_PORT_CVBS6:
	case TVIN_PORT_CVBS7:
		vf->source_type = VFRAME_SOURCE_TYPE_CVBS;
		break;
	case TVIN_PORT_COMP0:
	case TVIN_PORT_COMP1:
	case TVIN_PORT_COMP2:
	case TVIN_PORT_COMP3:
	case TVIN_PORT_COMP4:
	case TVIN_PORT_COMP5:
	case TVIN_PORT_COMP6:
	case TVIN_PORT_COMP7:
		vf->source_type = VFRAME_SOURCE_TYPE_COMP;
		break;
	case TVIN_PORT_HDMI0:
	case TVIN_PORT_HDMI1:
	case TVIN_PORT_HDMI2:
	case TVIN_PORT_HDMI3:
	case TVIN_PORT_HDMI4:
	case TVIN_PORT_HDMI5:
	case TVIN_PORT_HDMI6:
	case TVIN_PORT_HDMI7:
	case TVIN_PORT_DVIN0:/* external hdmiin used default */
		vf->source_type = VFRAME_SOURCE_TYPE_HDMI;
		break;
	default:
		vf->source_type = VFRAME_SOURCE_TYPE_OTHERS;
		break;
	}
}



static inline void vdin_set_source_mode(struct vdin_dev_s *devp,
		struct vframe_s *vf)
{
	switch (devp->parm.info.fmt) {
	case TVIN_SIG_FMT_CVBS_NTSC_M:
	case TVIN_SIG_FMT_CVBS_NTSC_443:
		vf->source_mode = VFRAME_SOURCE_MODE_NTSC;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_I:
	case TVIN_SIG_FMT_CVBS_PAL_M:
	case TVIN_SIG_FMT_CVBS_PAL_60:
	case TVIN_SIG_FMT_CVBS_PAL_CN:
		vf->source_mode = VFRAME_SOURCE_MODE_PAL;
		break;
	case TVIN_SIG_FMT_CVBS_SECAM:
		vf->source_mode = VFRAME_SOURCE_MODE_SECAM;
		break;
	default:
		vf->source_mode = VFRAME_SOURCE_MODE_OTHERS;
		break;
	}
}

/*
 * 480p/i = 9:8
 * 576p/i = 16:15
 * 720p and 1080p/i = 1:1
 * All VGA format = 1:1
 */
static void vdin_set_pixel_aspect_ratio(struct vdin_dev_s *devp,
		struct vframe_s *vf)
{
	switch (devp->parm.info.fmt) {
	/* 480P */
	case TVIN_SIG_FMT_COMP_480P_60HZ_D000:
	case TVIN_SIG_FMT_HDMI_640X480P_60HZ:
	case TVIN_SIG_FMT_HDMI_720X480P_60HZ:
	case TVIN_SIG_FMT_HDMI_1440X480P_60HZ:
	case TVIN_SIG_FMT_HDMI_2880X480P_60HZ:
	case TVIN_SIG_FMT_HDMI_720X480P_120HZ:
	case TVIN_SIG_FMT_HDMI_720X480P_240HZ:
	case TVIN_SIG_FMT_HDMI_720X480P_60HZ_FRAME_PACKING:
	case TVIN_SIG_FMT_CAMERA_640X480P_30HZ:
		/* 480I */
	case TVIN_SIG_FMT_CVBS_NTSC_M:
	case TVIN_SIG_FMT_CVBS_NTSC_443:
	case TVIN_SIG_FMT_CVBS_PAL_M:
	case TVIN_SIG_FMT_CVBS_PAL_60:
	case TVIN_SIG_FMT_COMP_480I_59HZ_D940:
	case TVIN_SIG_FMT_HDMI_1440X480I_60HZ:
	case TVIN_SIG_FMT_HDMI_2880X480I_60HZ:
	case TVIN_SIG_FMT_HDMI_1440X480I_120HZ:
	case TVIN_SIG_FMT_HDMI_1440X480I_240HZ:
	case TVIN_SIG_FMT_BT656IN_480I_60HZ:
	case TVIN_SIG_FMT_BT601IN_480I_60HZ:
		vf->pixel_ratio = PIXEL_ASPECT_RATIO_8_9;
		break;
		/* 576P */
	case TVIN_SIG_FMT_COMP_576P_50HZ_D000:
	case TVIN_SIG_FMT_HDMI_720X576P_50HZ:
	case TVIN_SIG_FMT_HDMI_1440X576P_50HZ:
	case TVIN_SIG_FMT_HDMI_2880X576P_50HZ:
	case TVIN_SIG_FMT_HDMI_720X576P_100HZ:
	case TVIN_SIG_FMT_HDMI_720X576P_200HZ:
	case TVIN_SIG_FMT_HDMI_720X576P_50HZ_FRAME_PACKING:
		/* 576I */
	case TVIN_SIG_FMT_CVBS_PAL_I:
	case TVIN_SIG_FMT_CVBS_PAL_CN:
	case TVIN_SIG_FMT_CVBS_SECAM:
	case TVIN_SIG_FMT_COMP_576I_50HZ_D000:
	case TVIN_SIG_FMT_HDMI_1440X576I_50HZ:
	case TVIN_SIG_FMT_HDMI_2880X576I_50HZ:
	case TVIN_SIG_FMT_HDMI_1440X576I_100HZ:
	case TVIN_SIG_FMT_HDMI_1440X576I_200HZ:
	case TVIN_SIG_FMT_BT656IN_576I_50HZ:
	case TVIN_SIG_FMT_BT601IN_576I_50HZ:
		vf->pixel_ratio = PIXEL_ASPECT_RATIO_16_15;
		break;
	default:
		vf->pixel_ratio = PIXEL_ASPECT_RATIO_1_1;
		break;
	}
}
/*
 * based on the bellow parameters:
 * 1.h_active
 * 2.v_active
 */
static void vdin_set_display_ratio(struct vframe_s *vf)
{
	unsigned int re = 0;

	if (vf->width == 0 || vf->height == 0)
		return;

	re = (vf->width * 36)/vf->height;
	if ((re > 36 && re <= 56) || (re == 90) || (re == 108))
		vf->ratio_control = 0xc0 << DISP_RATIO_ASPECT_RATIO_BIT;
	else if (re > 56)
		vf->ratio_control = 0x90 << DISP_RATIO_ASPECT_RATIO_BIT;
	else
		vf->ratio_control = 0x100 << DISP_RATIO_ASPECT_RATIO_BIT;
}

static inline void vdin_set_source_bitdepth(struct vdin_dev_s *devp,
		struct vframe_s *vf)
{
	switch (devp->source_bitdepth) {
	case 10:
		vf->bitdepth = BITDEPTH_Y10 | BITDEPTH_U10 | BITDEPTH_V10;
		break;
	case 9:
		vf->bitdepth = BITDEPTH_Y9 | BITDEPTH_U9 | BITDEPTH_V9;
		break;
	case 8:
		vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		break;
	default:
		vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		break;
	}
	if (devp->color_depth_mode && (devp->source_bitdepth > 8) &&
		((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422)))
		vf->bitdepth |= FULL_PACK_422_MODE;
}

/* based on the bellow parameters:
 * 1.h_active
 * 2.v_active
 */
static void vdin_vf_init(struct vdin_dev_s *devp)
{
	int i = 0;
	unsigned int chromaid, addr, index;
	struct vf_entry *master, *slave;
	struct vframe_s *vf;
	struct vf_pool *p = devp->vfp;
	enum tvin_scan_mode_e  scan_mode;

	index = devp->index;
	/* const struct tvin_format_s *fmt_info = tvin_get_fmt_info(fmt); */
	if (vdin_dbg_en)
		pr_info("vdin.%d vframe initial information table: (%d of %d)\n",
			 index, p->size, p->max_size);

	for (i = 0; i < p->size; ++i) {
		master = vf_get_master(p, i);
		master->flag |= VF_FLAG_NORMAL_FRAME;
		vf = &master->vf;
		memset(vf, 0, sizeof(struct vframe_s));
		vf->index = i;
		vf->width = devp->h_active;
		vf->height = devp->v_active;
		scan_mode = devp->fmt_info_p->scan_mode;
		if (((scan_mode == TVIN_SCAN_MODE_INTERLACED) &&
		    (!(devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) &&
		      (devp->parm.info.fmt != TVIN_SIG_FMT_NULL))) ||
		      (devp->parm.port == TVIN_PORT_CVBS3))
			vf->height <<= 1;
#ifndef VDIN_DYNAMIC_DURATION
		vf->duration = devp->fmt_info_p->duration;
#endif
		/* if output fmt is nv21 or nv12 ,
		 * use the two continuous canvas for one field
		 */
		if ((devp->prop.dest_cfmt == TVIN_NV12) ||
		   (devp->prop.dest_cfmt == TVIN_NV21)) {
			chromaid =
				(vdin_canvas_ids[index][(vf->index<<1)+1])<<8;
			addr =
				vdin_canvas_ids[index][vf->index<<1] |
				chromaid;
		} else
			addr = vdin_canvas_ids[index][vf->index];

		vf->canvas0Addr = vf->canvas1Addr = addr;

		/* set source type & mode */
		vdin_set_source_type(devp, vf);
		vdin_set_source_mode(devp, vf);
		/* set source signal format */
		vf->sig_fmt = devp->parm.info.fmt;
		/* set pixel aspect ratio */
		vdin_set_pixel_aspect_ratio(devp, vf);
		/*set display ratio control */
		vdin_set_display_ratio(vf);
		vdin_set_source_bitdepth(devp, vf);
		/* init slave vframe */
		slave  = vf_get_slave(p, i);
		if (slave == NULL)
			continue;
		slave->flag = master->flag;
		memset(&slave->vf, 0, sizeof(struct vframe_s));
		slave->vf.index	  = vf->index;
		slave->vf.width	  = vf->width;
		slave->vf.height	  = vf->height;
		slave->vf.duration	  = vf->duration;
		slave->vf.ratio_control   = vf->ratio_control;
		slave->vf.canvas0Addr = vf->canvas0Addr;
		slave->vf.canvas1Addr = vf->canvas1Addr;
		/* set slave vf source type & mode */
		slave->vf.source_type = vf->source_type;
		slave->vf.source_mode = vf->source_mode;
		slave->vf.bitdepth = vf->bitdepth;
		if (vdin_dbg_en)
			pr_info("\t%2d: 0x%2x %ux%u, duration = %u\n",
					vf->index, vf->canvas0Addr, vf->width,
					vf->height, vf->duration);
	}
}
#ifdef CONFIG_AML_RDMA
static void vdin_rdma_irq(void *arg)
{
	rdma_irq_cnt++;
}

static struct rdma_op_s vdin_rdma_op = {
	vdin_rdma_irq,
	NULL
};
#endif

/*
 * 1. config canvas for video frame.
 * 2. enable hw work, including:
 *		a. mux null input.
 *		b. set clock auto.
 * 3. set all registeres including:
 *		a. mux input.
 * 4. call the callback function of the frontend to start.
 * 5. enable irq .
 *
 */
void vdin_start_dec(struct vdin_dev_s *devp)
{
	struct tvin_state_machine_ops_s *sm_ops;

	/* avoid null pointer oops */
	if (!devp || !devp->fmt_info_p) {
		pr_info("[vdin]%s null error.\n", __func__);
		return;
	}
	if (devp->frontend && devp->frontend->sm_ops) {
		sm_ops = devp->frontend->sm_ops;
		sm_ops->get_sig_property(devp->frontend, &devp->prop);

		devp->parm.info.cfmt = devp->prop.color_format;
		if ((devp->parm.dest_width != 0) ||
			(devp->parm.dest_height != 0)) {
			devp->prop.scaling4w = devp->parm.dest_width;
			devp->prop.scaling4h = devp->parm.dest_height;
		}
		if (devp->flags & VDIN_FLAG_MANUAL_CONVERSION) {
			devp->prop.dest_cfmt = devp->debug.dest_cfmt;
			devp->prop.scaling4w = devp->debug.scaler4w;
			devp->prop.scaling4h = devp->debug.scaler4h;
		}
		if (devp->debug.cutwin.hs ||
			devp->debug.cutwin.he ||
			devp->debug.cutwin.vs ||
			devp->debug.cutwin.ve) {
			devp->prop.hs = devp->debug.cutwin.hs;
			devp->prop.he = devp->debug.cutwin.he;
			devp->prop.vs = devp->debug.cutwin.vs;
			devp->prop.ve = devp->debug.cutwin.ve;
		}
	}

	vdin_get_format_convert(devp);
	devp->curr_wr_vfe = NULL;
	/* h_active/v_active will be recalculated by bellow calling */
	vdin_set_decimation(devp);
	vdin_set_cutwin(devp);
	vdin_set_hvscale(devp);
	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu())
		vdin_set_bitdepth(devp);
	/* txl new add fix for hdmi switch resolution cause cpu holding */
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL)
		vdin_fix_nonstd_vsync(devp);

#ifdef CONFIG_AML_RDMA
	if (rdma_enable && devp->rdma_handle > 0)
		devp->flags |= VDIN_FLAG_RDMA_ENABLE;
#endif
    /*reverse / disable reverse write buffer*/
	vdin_wr_reverse(devp->addr_offset,
				devp->parm.h_reverse,
				devp->parm.v_reverse);
#ifdef CONFIG_CMA
	vdin_cma_alloc(devp);
#endif
	/* h_active/v_active will be used by bellow calling */
	if (canvas_config_mode == 1)
		vdin_canvas_start_config(devp);
	else if (canvas_config_mode == 2)
		vdin_canvas_auto_config(devp);
#if 0
	if ((devp->prop.dest_cfmt == TVIN_NV12) ||
		(devp->prop.dest_cfmt == TVIN_NV21))
		devp->vfp->size = devp->canvas_max_num;
	else
		devp->vfp->size = devp->canvas_max_num;
#endif
	if (devp->prop.dvi_info>>4 &&
		devp->parm.port >= TVIN_PORT_HDMI0 &&
		devp->parm.port <= TVIN_PORT_HDMI7)
		devp->duration = 96000/(devp->prop.dvi_info>>4);
	else
		devp->duration = devp->fmt_info_p->duration;

	devp->vfp->size = devp->canvas_max_num;
	vf_pool_init(devp->vfp, devp->vfp->size);
	vdin_vf_init(devp);

	devp->abnormal_cnt = 0;
	devp->last_wr_vfe = NULL;
	irq_max_count = 0;
	/* devp->stamp_valid = false; */
	devp->stamp = 0;
	devp->cycle = 0;
	devp->cycle_tag = 0;
	devp->hcnt64 = 0;
	devp->hcnt64_tag = 0;

	devp->vga_clr_cnt = devp->canvas_max_num;

	devp->vs_cnt_valid = 0;
	devp->vs_cnt_ignore = 0;

	memset(&devp->parm.histgram[0], 0, sizeof(unsigned short) * 64);

	devp->curr_field_type = vdin_get_curr_field_type(devp);
	/* pr_info("start clean_counter is %d\n",clean_counter); */
	/* configure regs and enable hw */
#ifdef CONFIG_AML_VPU
	switch_vpu_mem_pd_vmod(devp->addr_offset?VPU_VIU_VDIN1:VPU_VIU_VDIN0,
			VPU_MEM_POWER_ON);
#endif

	vdin_hw_enable(devp->addr_offset);
	vdin_set_all_regs(devp);

	if (!(devp->parm.flag & TVIN_PARM_FLAG_CAP) &&
		devp->frontend->dec_ops &&
		devp->frontend->dec_ops->start &&
		(devp->parm.port != TVIN_PORT_CVBS3))
		devp->frontend->dec_ops->start(devp->frontend,
				devp->parm.info.fmt);

	/* register provider, so the receiver can get the valid vframe */
	udelay(start_provider_delay);
	vf_reg_provider(&devp->vprov);
	if (vf_get_receiver_name(devp->name)) {
		if (strcmp(vf_get_receiver_name(devp->name), "deinterlace")
				== 0)
			devp->send2di = true;
		else
			devp->send2di = false;
	}

	vf_notify_receiver(devp->name, VFRAME_EVENT_PROVIDER_START, NULL);
	if ((devp->parm.port != TVIN_PORT_VIU) ||
		(viu_hw_irq != 0)) {
		/*enable irq */
		enable_irq(devp->irq);
		if (vdin_dbg_en)
			pr_info("****[%s]enable_irq ifdef VDIN_V2****\n",
					__func__);
	}

	if (vdin_dbg_en)
		pr_info("****[%s]ok!****\n", __func__);
#ifdef CONFIG_AM_TIMESYNC
	if (devp->parm.port != TVIN_PORT_VIU) {
		/*disable audio&video sync used for libplayer*/
		tsync_set_enable(0);
		/* enable system_time */
		timestamp_pcrscr_enable(1);
		pr_info("****[%s]disable tysnc& enable system time!****\n",
			__func__);
	}
#endif
	irq_cnt = 0;
	rdma_irq_cnt = 0;
	if (time_en)
		pr_info("vdin.%d start time: %ums, run time:%ums.\n",
				devp->index, jiffies_to_msecs(jiffies),
				jiffies_to_msecs(jiffies)-devp->start_time);
}

/*
 * 1. disable irq.
 * 2. disable hw work, including:
 *		a. mux null input.
 *		b. set clock off.
 * 3. call the callback function of the frontend to stop.
 *
 */
void vdin_stop_dec(struct vdin_dev_s *devp)
{
	/* avoid null pointer oops */
	if (!devp || !devp->frontend)
		return;

	disable_irq_nosync(devp->irq);
	vdin_hw_disable(devp->addr_offset);
	if (!(devp->parm.flag & TVIN_PARM_FLAG_CAP) &&
		devp->frontend->dec_ops &&
		devp->frontend->dec_ops->stop &&
		(devp->parm.port != TVIN_PORT_CVBS3))
		devp->frontend->dec_ops->stop(devp->frontend, devp->parm.port);
	vdin_set_default_regmap(devp->addr_offset);

	/* reset default canvas  */
	vdin_set_def_wr_canvas(devp);
	vf_unreg_provider(&devp->vprov);
#ifdef CONFIG_CMA
	vdin_cma_release(devp);
#endif

#ifdef CONFIG_AML_VPU
	switch_vpu_mem_pd_vmod(devp->addr_offset?VPU_VIU_VDIN1:VPU_VIU_VDIN0,
			VPU_MEM_POWER_DOWN);
#endif
	memset(&devp->prop, 0, sizeof(struct tvin_sig_property_s));
#ifdef CONFIG_AML_RDMA
	rdma_clear(devp->rdma_handle);
#endif
	devp->flags &= (~VDIN_FLAG_RDMA_ENABLE);
	ignore_frames = 0;
	devp->cycle = 0;
	 /* clear color para*/
	memset(&devp->prop, 0, sizeof(devp->prop));
	if (time_en)
		pr_info("vdin.%d stop time %ums,run time:%ums.\n",
				devp->index, jiffies_to_msecs(jiffies),
				jiffies_to_msecs(jiffies)-devp->start_time);

	if (vdin_dbg_en)
		pr_info("%s ok\n", __func__);
}
/* @todo */

int start_tvin_service(int no, struct vdin_parm_s  *para)
{
	struct tvin_frontend_s *fe;
	int ret = 0;
	struct vdin_dev_s *devp = vdin_devp[no];
	enum tvin_sig_fmt_e fmt;

	if (IS_ERR(devp)) {
		pr_err("[vdin]%s vdin%d has't registered,please register.\n",
				__func__, no);
		return -1;
	}
	fmt = devp->parm.info.fmt;
	if (vdin_dbg_en) {
		pr_info("**[%s]cfmt:%d;dfmt:%d;dest_hactive:%d;",
				__func__, para->cfmt,
				para->dfmt, para->dest_hactive);
		pr_info("dest_vactive:%d;frame_rate:%d;h_active:%d,",
				para->dest_vactive, para->frame_rate,
				para->h_active);
		pr_info("v_active:%d;scan_mode:%d**\n",
				para->v_active, para->scan_mode);
	}
	devp->start_time = jiffies_to_msecs(jiffies);
	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		pr_err("%s: port 0x%x, decode started already.\n",
				__func__, para->port);
		ret = -EBUSY;
		return ret;
	}
	if ((para->port != TVIN_PORT_VIU) ||
		(viu_hw_irq != 0)) {
		ret = request_irq(devp->irq, vdin_v4l2_isr, IRQF_SHARED,
				devp->irq_name, (void *)devp);
		/* disable vsync irq until vdin configured completely */
		disable_irq_nosync(devp->irq);
	}
	/* config the vdin use default value*/
	vdin_set_default_regmap(devp->addr_offset);

	devp->parm.port = para->port;
	devp->parm.info.fmt = para->fmt;
	fmt = devp->parm.info.fmt;
	/* add for camera random resolution */
	if (para->fmt >= TVIN_SIG_FMT_MAX) {
		devp->fmt_info_p = kmalloc(sizeof(struct tvin_format_s),
				GFP_KERNEL);
		if (!devp->fmt_info_p)
			return -ENOMEM;
		devp->fmt_info_p->hs_bp     = para->hs_bp;
		devp->fmt_info_p->vs_bp     = para->vs_bp;
		devp->fmt_info_p->hs_pol    = para->hsync_phase;
		devp->fmt_info_p->vs_pol    = para->vsync_phase;
		if ((para->h_active * para->v_active * para->frame_rate) >
					devp->vdin_max_pixelclk)
			para->h_active >>= 1;
		devp->fmt_info_p->h_active  = para->h_active;
		devp->fmt_info_p->v_active  = para->v_active;
		devp->fmt_info_p->scan_mode = para->scan_mode;
		devp->fmt_info_p->duration  = 96000/para->frame_rate;
		devp->fmt_info_p->pixel_clk = para->h_active *
				para->v_active * para->frame_rate;
		devp->fmt_info_p->pixel_clk /= 10000;
	} else {
		devp->fmt_info_p = (struct tvin_format_s *)
				tvin_get_fmt_info(fmt);
		/* devp->fmt_info_p = tvin_get_fmt_info(devp->parm.info.fmt); */
	}
	if (!devp->fmt_info_p) {
		pr_err("%s(%d): error, fmt is null!!!\n", __func__, no);
		return -1;
	}

	if ((is_meson_gxbb_cpu()) && (para->bt_path == BT_PATH_GPIO_B)) {
		devp->bt_path = para->bt_path;
		fe = tvin_get_frontend(para->port, 1);
	} else
		fe = tvin_get_frontend(para->port, 0);
	if (fe) {
		fe->private_data = para;
		fe->port         = para->port;
		devp->frontend   = fe;

		/* vdin msr clock gate enable */
		clk_prepare_enable(devp->msr_clk);

		if (fe->dec_ops->open)
			fe->dec_ops->open(fe, fe->port);
	} else {
		pr_err("%s(%d): not supported port 0x%x\n",
				__func__, no, para->port);
		return -1;
	}
	/* #ifdef CONFIG_ARCH_MESON6 */
	/* switch_mod_gate_by_name("vdin", 1); */
	/* #endif */
	vdin_start_dec(devp);
	devp->flags |= VDIN_FLAG_DEC_OPENED;
	devp->flags |= VDIN_FLAG_DEC_STARTED;

	return 0;
}
EXPORT_SYMBOL(start_tvin_service);
/* @todo */
int stop_tvin_service(int no)
{
	struct vdin_dev_s *devp;
	unsigned int end_time;

	devp = vdin_devp[no];
	if (!(devp->flags&VDIN_FLAG_DEC_STARTED)) {
		pr_err("%s:decode hasn't started.\n", __func__);
		return -EBUSY;
	}
/* remove for m6&m8 camera function,
 * may cause hardware disable bug on kernel 3.10
 */
/* #if MESON_CPU_TYPE < MESON_CPU_TYPE_MESON8 */
/* devp->flags |= VDIN_FLAG_DEC_STOP_ISR; */
/* #endif */
	vdin_stop_dec(devp);
	/*close fe*/
	if (devp->frontend->dec_ops->close)
		devp->frontend->dec_ops->close(devp->frontend);
	/*free the memory allocated in start tvin service*/
	if (devp->parm.info.fmt >= TVIN_SIG_FMT_MAX)
		kfree(devp->fmt_info_p);
/* #ifdef CONFIG_ARCH_MESON6 */
/* switch_mod_gate_by_name("vdin", 0); */
/* #endif */
	devp->flags &= (~VDIN_FLAG_DEC_OPENED);
	devp->flags &= (~VDIN_FLAG_DEC_STARTED);
	if ((devp->parm.port != TVIN_PORT_VIU) ||
		(viu_hw_irq != 0))
		free_irq(devp->irq, (void *)devp);
	end_time = jiffies_to_msecs(jiffies);
	if (time_en)
		pr_info("[vdin]:vdin start time:%ums,stop time:%ums,run time:%u.\n",
				devp->start_time,
				end_time,
				end_time-devp->start_time);

	return 0;
}
EXPORT_SYMBOL(stop_tvin_service);

void get_tvin_canvas_info(int *start, int *num)
{
	*start = vdin_canvas_ids[0][0];
	*num = vdin_devp[0]->canvas_max_num;
}
EXPORT_SYMBOL(get_tvin_canvas_info);

static int vdin_ioctl_fe(int no, struct fe_arg_s *parm)
{
	struct vdin_dev_s *devp = vdin_devp[no];
	int ret = 0;

	if (IS_ERR(devp)) {
		pr_err("[vdin]%s vdin%d has't registered,please register.\n",
				__func__, no);
		return -1;
	}
	if (devp->frontend &&
		devp->frontend->dec_ops &&
		devp->frontend->dec_ops->ioctl)
		ret = devp->frontend->dec_ops->ioctl(devp->frontend, parm->arg);
	else if (!devp->frontend) {
		devp->frontend = tvin_get_frontend(parm->port, parm->index);
		if (devp->frontend &&
			devp->frontend->dec_ops &&
			devp->frontend->dec_ops->ioctl)
			ret = devp->frontend->dec_ops->ioctl(devp->frontend,
					parm->arg);
	}
	return ret;
}
static void vdin_rdma_isr(struct vdin_dev_s *devp)
{
	if (devp->parm.port == TVIN_PORT_VIU)
		vdin_v4l2_isr(devp->irq, devp);
}
static int vdin_func(int no, struct vdin_arg_s *arg)
{
	struct vdin_dev_s *devp = vdin_devp[no];
	int ret = 0;
	struct vdin_arg_s *parm = NULL;
	struct vframe_s *vf = NULL;

	parm = arg;
	if (IS_ERR_OR_NULL(devp)) {
		if (vdin_dbg_en)
			pr_err("[vdin]%s vdin%d has't registered,please register.\n",
					__func__, no);
		return -1;
	} else if (!(devp->flags&VDIN_FLAG_DEC_STARTED) &&
			(parm->cmd != VDIN_CMD_MPEGIN_START)) {
		if (vdin_dbg_en)
			;/* pr_err("[vdin]%s vdin%d has't started.\n",
			  * __func__,no);
			  */
		return -1;
	}
	if (vdin_dbg_en)
		pr_info("[vdin_drv]%s vdin%d:parm->cmd : %d\n",
				__func__, no, parm->cmd);
	switch (parm->cmd) {
	/*ajust vdin1 matrix1 & matrix2 for isp to get histogram information*/
	case VDIN_CMD_SET_CSC:
		vdin_set_matrixs(devp, parm->matrix_id, parm->color_convert);
		break;
	case VDIN_CMD_SET_CM2:
		vdin_set_cm2(devp->addr_offset, devp->h_active,
				devp->v_active, parm->cm2);
		break;
	case VDIN_CMD_ISR:
		vdin_rdma_isr(devp);
		break;
	case VDIN_CMD_MPEGIN_START:
		devp->h_active = parm->h_active;
		devp->v_active = parm->v_active;
		switch_vpu_mem_pd_vmod(devp->addr_offset ? VPU_VIU_VDIN1 :
				VPU_VIU_VDIN0, VPU_MEM_POWER_ON);
		vdin_set_mpegin(devp);
		pr_info("%s:VDIN_CMD_MPEGIN_START :h_active:%d,v_active:%d\n",
				__func__, devp->h_active, devp->v_active);
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
			devp->curr_wr_vfe = kmalloc(sizeof(struct vf_entry),
					GFP_KERNEL);
			devp->flags |= VDIN_FLAG_DEC_STARTED;
		} else
			pr_info("%s: VDIN_CMD_MPEGIN_START already !!\n",
					__func__);
		break;
	case VDIN_CMD_GET_HISTGRAM:
		if (!devp->curr_wr_vfe) {
			if (vdin_dbg_en)
				pr_info("VDIN_CMD_GET_HISTGRAM:curr_wr_vfe is NULL !!!\n");
			break;
		}
		vdin_set_vframe_prop_info(&devp->curr_wr_vfe->vf, devp);
		vdin_backup_histgram(&devp->curr_wr_vfe->vf, devp);
		vf = (struct vframe_s *)parm->private;
		if (vf && devp->curr_wr_vfe)
			memcpy(&vf->prop, &devp->curr_wr_vfe->vf.prop,
					sizeof(struct vframe_prop_s));
		break;
	case VDIN_CMD_MPEGIN_STOP:
		if (devp->flags & VDIN_FLAG_DEC_STARTED) {
			pr_info("%s:warning:VDIN_CMD_MPEGIN_STOP\n",
					__func__);
			switch_vpu_mem_pd_vmod(
					devp->addr_offset ? VPU_VIU_VDIN1 :
					VPU_VIU_VDIN0, VPU_MEM_POWER_DOWN);
			vdin_hw_disable(devp->addr_offset);
			kfree(devp->curr_wr_vfe);
			devp->curr_wr_vfe = NULL;
			devp->flags &= (~VDIN_FLAG_DEC_STARTED);
		} else
			pr_info("%s:warning:VDIN_CMD_MPEGIN_STOP already\n",
					__func__);
		break;
	case VDIN_CMD_FORCE_GO_FIELD:
		vdin_force_gofiled(devp);
		break;
	default:
		break;
	}
	return ret;
}
static struct vdin_v4l2_ops_s vdin_4v4l2_ops = {
	.start_tvin_service   = start_tvin_service,
	.stop_tvin_service    = stop_tvin_service,
	.get_tvin_canvas_info = get_tvin_canvas_info,
	.set_tvin_canvas_info = NULL,
	.tvin_fe_func         = vdin_ioctl_fe,
	.tvin_vdin_func	      = vdin_func,
};

void vdin_pause_dec(struct vdin_dev_s *devp)
{
	vdin_hw_disable(devp->addr_offset);
}

void vdin_resume_dec(struct vdin_dev_s *devp)
{
	vdin_hw_enable(devp->addr_offset);
}

void vdin_vf_reg(struct vdin_dev_s *devp)
{
	vf_reg_provider(&devp->vprov);
	vf_notify_receiver(devp->name, VFRAME_EVENT_PROVIDER_START, NULL);
}

void vdin_vf_unreg(struct vdin_dev_s *devp)
{
	vf_unreg_provider(&devp->vprov);
}

static inline void vdin_set_view(struct vdin_dev_s *devp, struct vframe_s *vf)
{
	struct vframe_view_s *left_eye, *right_eye;
	enum tvin_sig_fmt_e fmt = devp->parm.info.fmt;
	const struct tvin_format_s *fmt_info = tvin_get_fmt_info(fmt);

	if (!fmt_info) {
		pr_err("[tvafe..] %s: error,fmt is null!!!\n", __func__);
		return;
	}

	left_eye  = &vf->left_eye;
	right_eye = &vf->right_eye;

	switch (devp->parm.info.trans_fmt) {
	case TVIN_TFMT_3D_LRH_OLOR:
	case TVIN_TFMT_3D_LRH_OLER:
		left_eye->start_x	 = 0;
		left_eye->start_y	 = 0;
		left_eye->width	 = devp->h_active >> 1;
		left_eye->height	 = devp->v_active;
		right_eye->start_x	 = devp->h_active >> 1;
		right_eye->start_y	 = 0;
		right_eye->width	 = devp->h_active >> 1;
		right_eye->height	 = devp->v_active;
		break;
	case TVIN_TFMT_3D_TB:
		left_eye->start_x	 = 0;
		left_eye->start_y	 = 0;
		left_eye->width	 = devp->h_active;
		left_eye->height	 = devp->v_active >> 1;
		right_eye->start_x	 = 0;
		right_eye->start_y	 = devp->v_active >> 1;
		right_eye->width	 = devp->h_active;
		right_eye->height	 = devp->v_active >> 1;
		break;
	case TVIN_TFMT_3D_FP: {
		unsigned int vactive = 0;
		unsigned int vspace = 0;
		struct vf_entry *slave = NULL;

		vspace  = fmt_info->vs_front + fmt_info->vs_width +
				fmt_info->vs_bp;
		if ((devp->parm.info.fmt ==
			TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_FRAME_PACKING) ||
			(devp->parm.info.fmt ==
			TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_FRAME_PACKING)) {
			vactive = (fmt_info->v_active - vspace -
					vspace - vspace + 1) >> 2;
			slave = vf_get_slave(devp->vfp, vf->index);
			if (slave == NULL)
				break;
			slave->vf.left_eye.start_x  = 0;
			slave->vf.left_eye.start_y  = vactive + vspace +
					vactive + vspace - 1;
			slave->vf.left_eye.width    = devp->h_active;
			slave->vf.left_eye.height   = vactive;
			slave->vf.right_eye.start_x = 0;
			slave->vf.right_eye.start_y = vactive + vspace +
					vactive + vspace + vactive + vspace - 1;
			slave->vf.right_eye.width   = devp->h_active;
			slave->vf.right_eye.height  = vactive;
		} else
			vactive = (fmt_info->v_active - vspace) >> 1;
		left_eye->start_x	 = 0;
		left_eye->start_y	 = 0;
		left_eye->width	 = devp->h_active;
		left_eye->height	 = vactive;
		right_eye->start_x	 = 0;
		right_eye->start_y	 = vactive + vspace;
		right_eye->width	 = devp->h_active;
		right_eye->height	 = vactive;
		break;
		}
	case TVIN_TFMT_3D_FA: {
		unsigned int vactive = 0;
		unsigned int vspace = 0;

		vspace = fmt_info->vs_front + fmt_info->vs_width +
				fmt_info->vs_bp;

		vactive = (fmt_info->v_active - vspace + 1) >> 1;

		left_eye->start_x    = 0;
		left_eye->start_y    = 0;
		left_eye->width      = devp->h_active;
		left_eye->height     = vactive;
		right_eye->start_x   = 0;
		right_eye->start_y   = vactive + vspace;
		right_eye->width     = devp->h_active;
		right_eye->height    = vactive;
		break;
	}
	case TVIN_TFMT_3D_LA: {
		left_eye->start_x    = 0;
		left_eye->start_y    = 0;
		left_eye->width      = devp->h_active;
		left_eye->height     = (devp->v_active) >> 1;
		right_eye->start_x   = 0;
		right_eye->start_y   = 0;
		right_eye->width     = devp->h_active;
		right_eye->height    = (devp->v_active) >> 1;
		break;
	}
	default:
		left_eye->start_x	 = 0;
		left_eye->start_y	 = 0;
		left_eye->width	 = 0;
		left_eye->height	 = 0;
		right_eye->start_x	 = 0;
		right_eye->start_y	 = 0;
		right_eye->width	 = 0;
		right_eye->height	 = 0;
		break;
	}
}
irqreturn_t vdin_isr_simple(int irq, void *dev_id)
{
	struct vdin_dev_s *devp = (struct vdin_dev_s *)dev_id;
	struct tvin_decoder_ops_s *decops;
	unsigned int last_field_type;

	if (irq_max_count >= devp->canvas_max_num) {
		vdin_hw_disable(devp->addr_offset);
		return IRQ_HANDLED;
	}

	last_field_type = devp->curr_field_type;
	devp->curr_field_type = vdin_get_curr_field_type(devp);

	decops = devp->frontend->dec_ops;
	if (decops->decode_isr(devp->frontend,
			vdin_get_meas_hcnt64(devp->addr_offset)) == -1)
		return IRQ_HANDLED;
	/* set canvas address */
	vdin_set_canvas_id(devp, devp->flags&VDIN_FLAG_RDMA_ENABLE,
			vdin_canvas_ids[devp->index][irq_max_count]);
	if (vdin_dbg_en)
		pr_info("%2d: canvas id %d, field type %s\n",
			irq_max_count,
			vdin_canvas_ids[devp->index][irq_max_count],
			((last_field_type & VIDTYPE_TYPEMASK) ==
			 VIDTYPE_INTERLACE_TOP ? "top":"buttom"));

	irq_max_count++;
	return IRQ_HANDLED;
}
#if 0
/*#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING*/
unsigned int   vdin_ldim_max_global[100] = {0};
static void vdin_backup_histgram_ldim(struct vframe_s *vf,
		struct vdin_dev_s *devp)
{
	unsigned int i = 0;

	for (i = 0; i < 100; i++)
		vdin_ldim_max_global[i] = vf->prop.hist.ldim_max[i];

}
#endif

static void vdin_backup_histgram(struct vframe_s *vf, struct vdin_dev_s *devp)
{
	unsigned int i = 0;

	devp->parm.hist_pow = vf->prop.hist.hist_pow;
	devp->parm.luma_sum = vf->prop.hist.luma_sum;
	devp->parm.pixel_sum = vf->prop.hist.pixel_sum;
	for (i = 0; i < 64; i++)
		devp->parm.histgram[i] = vf->prop.hist.gamma[i];
}
/*as use the spin_lock,
 *1--there is no sleep,
 *2--it is better to shorter the time,
 *3--it is better to shorter the time,
 */
#define VDIN_MEAS_24M_1MS 24000

irqreturn_t vdin_isr(int irq, void *dev_id)
{
	ulong flags = 0;
	struct vdin_dev_s *devp = (struct vdin_dev_s *)dev_id;
	enum tvin_sm_status_e state;

	struct vf_entry *next_wr_vfe = NULL;
	struct vf_entry *curr_wr_vfe = NULL;
	struct vframe_s *curr_wr_vf = NULL;
	unsigned int last_field_type;
	struct tvin_decoder_ops_s *decops;
	unsigned int stamp = 0;
	struct tvin_state_machine_ops_s *sm_ops;
	int vdin2nr = 0;
	unsigned int offset;
	enum tvin_trans_fmt trans_fmt;
	struct tvin_sig_property_s *prop, *pre_prop;
	/* unsigned long long total_time; */
	/* unsigned long long total_tmp; */
/* unsigned int vdin0_sw_reset_flag = 0; */
/* struct vframe_provider_s *p = NULL; */
/* struct vframe_receiver_s *sub_recv = NULL; */
/* char provider_name[] = "deinterlace"; */
/* char provider_vdin0[] = "vdin0"; */

	isr_log(devp->vfp);
	irq_cnt++;
	/* debug interrupt interval time
	 *
	 * this code about system time must be outside of spinlock.
	 * because the spinlock may affect the system time.
	 */

	/* ignore fake irq caused by sw reset*/
	if (vdin_reset_flag) {
		vdin_reset_flag = 0;
		return IRQ_HANDLED;
	}

	/* avoid null pointer oops */
	if (!devp || !devp->frontend) {
		vdin_irq_flag = 1;
		goto irq_handled;
	}
	offset = devp->addr_offset;

/* bug fixed by VLSI guys. */
/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV */
/* p = vf_get_provider_by_name(provider_name); */
/* sub_recv = vf_get_receiver(provider_vdin0); */
/* if((devp->index == 0) && (p != NULL) && */
/* ((0 != strncasecmp(sub_recv->name,"deinterlace", 11))|| */
/* ((rd_bits(offset, VDIN_WR_CTRL, 25, 1) == 1)))) { */
/* if(vdin0_sw_reset_flag != 1) */
/* vdin0_sw_reset_flag = 1; */
/* #ifdef CONFIG_AML_RDMA */
/* rdma_write_reg_bits(devp->rdma_handle, */
/* VIU_SW_RESET, 1, 23, 1); */
/* rdma_write_reg_bits(devp->rdma_handle, */
/* VIU_SW_RESET, 0, 23, 1); */
/* #else */
/* W_VCBUS_BIT(VIU_SW_RESET, 1, 23, 1); */
/* W_VCBUS_BIT(VIU_SW_RESET, 0, 23, 1); */
/* #endif */
/* if(rd_bits(offset, VDIN_WR_CTRL, 23,1) != 0) */
/* wr_bits(offset, VDIN_WR_CTRL, 0, 23, 1); */
/* } else { */
/* if (vdin0_sw_reset_flag != 0) */
/* vdin0_sw_reset_flag = 0; */
/* if (rd_bits(offset, VDIN_WR_CTRL, 23,1) != 1) */
/* wr_bits(offset, VDIN_WR_CTRL, 1, 23, 1); */
/* } */
/* #endif */

	isr_log(devp->vfp);
	irq_cnt++;
	/* debug interrupt interval time
	 *
	 * this code about system time must be outside of spinlock.
	 * because the spinlock may affect the system time.
	 */

	spin_lock_irqsave(&devp->isr_lock, flags);
	/* W_VCBUS_BIT(VDIN_MISC_CTRL, 0, 0, 2); */
	vdin_reset_flag = vdin_vsync_reset_mif(devp->index);
	if ((devp->flags & VDIN_FLAG_DEC_STOP_ISR) &&
		(!(isr_flag & VDIN_BYPASS_STOP_CHECK))) {
		vdin_hw_disable(offset);
		devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR;
		vdin_irq_flag = 2;

		goto irq_handled;
	}
	stamp  = vdin_get_meas_vstamp(offset);
	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);
		/*save the first field stamp*/
		devp->stamp = stamp;
		vdin_irq_flag = 3;

		goto irq_handled;
	}
	if (devp->last_wr_vfe && (devp->flags&VDIN_FLAG_RDMA_ENABLE)) {
		provider_vf_put(devp->last_wr_vfe, devp->vfp);
		devp->last_wr_vfe = NULL;
		vf_notify_receiver(devp->name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	}
	/*check vs is valid base on the time during continuous vs*/
	if (vdin_check_cycle(devp) && (!(isr_flag & VDIN_BYPASS_CYC_CHECK))
		&& (!(devp->flags & VDIN_FLAG_SNOW_FLAG))) {
		vdin_irq_flag = 4;

		goto irq_handled;
	}

	devp->hcnt64 = vdin_get_meas_hcnt64(offset);

	/* ignore invalid vs base on the continuous fields
	 * different cnt to void screen flicker
	 */
	if (vdin_check_vs(devp) &&
		(!(isr_flag & VDIN_BYPASS_VSYNC_CHECK))
		&& (!(devp->flags & VDIN_FLAG_SNOW_FLAG))) {
		vdin_irq_flag = 5;

		goto irq_handled;
	}
	sm_ops = devp->frontend->sm_ops;

	last_field_type = devp->curr_field_type;
	devp->curr_field_type = vdin_get_curr_field_type(devp);

	/* ignore the unstable signal */
	state = tvin_get_sm_status(devp->index);
	if (((devp->parm.info.status != TVIN_SIG_STATUS_STABLE) ||
		(state != TVIN_SM_STATUS_STABLE)) &&
		(!(devp->flags & VDIN_FLAG_SNOW_FLAG))) {
		vdin_irq_flag = 6;
		goto irq_handled;
	}

	/* for 3D mode & interlaced format,
	 *  give up bottom field to avoid odd/even phase different
	 */
	trans_fmt = devp->parm.info.trans_fmt;
	if (((devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) ||
		 (trans_fmt && (trans_fmt != TVIN_TFMT_3D_FP))) &&
		((last_field_type & VIDTYPE_INTERLACE_BOTTOM) ==
				VIDTYPE_INTERLACE_BOTTOM)
	   ) {
		vdin_irq_flag = 7;
		goto irq_handled;
	}
	curr_wr_vfe = devp->curr_wr_vfe;
	curr_wr_vf  = &curr_wr_vfe->vf;

	/* change color matrix */
	if (devp->csc_cfg != 0) {
		prop = &devp->prop;
		pre_prop = &devp->pre_prop;
		vdin_set_matrix(devp);
		pre_prop->color_format = prop->color_format;
		pre_prop->vdin_hdr_Flag = prop->vdin_hdr_Flag;
		pre_prop->color_fmt_range = prop->color_fmt_range;
	}
	decops = devp->frontend->dec_ops;
	if (decops->decode_isr(devp->frontend, devp->hcnt64) == TVIN_BUF_SKIP) {
		vdin_irq_flag = 8;
		goto irq_handled;
	}
	if ((devp->parm.port >= TVIN_PORT_CVBS0) &&
		(devp->parm.port <= TVIN_PORT_CVBS7))
		curr_wr_vf->phase = sm_ops->get_secam_phase(devp->frontend) ?
				VFRAME_PHASE_DB : VFRAME_PHASE_DR;


	if (ignore_frames < max_ignore_frames) {
		ignore_frames++;
		vdin_irq_flag = 12;

		goto irq_handled;
	}

	if (sm_ops->check_frame_skip &&
		sm_ops->check_frame_skip(devp->frontend)) {
		vdin_irq_flag = 13;

		goto irq_handled;
	}

	next_wr_vfe = provider_vf_peek(devp->vfp);
	if (!next_wr_vfe) {
		vdin_irq_flag = 14;

		goto irq_handled;
	}
	vdin2nr = vf_notify_receiver(devp->name,
			VFRAME_EVENT_PROVIDER_QUREY_VDIN2NR, NULL);
	/*if vdin-nr,di must get
	 * vdin current field type which di pre will read
	 */
	if (vdin2nr || (devp->flags & VDIN_FLAG_RDMA_ENABLE))
		curr_wr_vf->type = devp->curr_field_type;
	else
		curr_wr_vf->type = last_field_type;

	/* for 2D->3D mode or hdmi 3d mode& interlaced format,
	 *  fill-in as progressive format
	 */
	if (((devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) ||
		(curr_wr_vf->trans_fmt)) &&
	    (last_field_type & VIDTYPE_INTERLACE)
	   ) {
		curr_wr_vf->type &= ~VIDTYPE_INTERLACE_TOP;
		curr_wr_vf->type |=  VIDTYPE_PROGRESSIVE;
		curr_wr_vf->type |=  VIDTYPE_PRE_INTERLACE;
	}
	curr_wr_vf->type_original = curr_wr_vf->type;

	vdin_set_drm_data(devp, curr_wr_vf);
	vdin_set_vframe_prop_info(curr_wr_vf, devp);
	vdin_backup_histgram(curr_wr_vf, devp);
	#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	/*vdin_backup_histgram_ldim(curr_wr_vf, devp);*/
	#endif
	if ((devp->parm.port >= TVIN_PORT_HDMI0) &&
		(devp->parm.port <= TVIN_PORT_HDMI7)) {
		curr_wr_vf->trans_fmt = devp->parm.info.trans_fmt;
		vdin_set_view(devp, curr_wr_vf);
	}
#if 0
	vdin_calculate_duration(devp);
#endif
	curr_wr_vf->duration = devp->duration;
	/*  put for receiver
	 *  ppmgr had handled master and slave vf by itself,
	 *  vdin do not to declare them respectively
	 *  ppmgr put the vf that included master vf and slave vf
	 */
	/*config height according to 3d mode dynamically*/
	if (((devp->fmt_info_p->scan_mode == TVIN_SCAN_MODE_INTERLACED) &&
	    (!(devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) &&
	    (devp->parm.info.fmt != TVIN_SIG_FMT_NULL))) ||
	    (devp->parm.port == TVIN_PORT_CVBS3))
		curr_wr_vf->height = devp->v_active<<1;
	else
		curr_wr_vf->height = devp->v_active;
	/*new add for atv snow:avoid flick black on bottom when atv search*/
	if ((devp->parm.port == TVIN_PORT_CVBS3) &&
		(devp->flags & VDIN_FLAG_SNOW_FLAG))
		curr_wr_vf->height = 480;
	curr_wr_vfe->flag |= VF_FLAG_NORMAL_FRAME;
	if (devp->flags&VDIN_FLAG_RDMA_ENABLE)
		devp->last_wr_vfe = curr_wr_vfe;
	else
		provider_vf_put(curr_wr_vfe, devp->vfp);

	/* prepare for next input data */
	next_wr_vfe = provider_vf_get(devp->vfp);
	/* debug for video latency */
	next_wr_vfe->vf.ready_jiffies64 = jiffies_64;
	vdin_set_canvas_id(devp, (devp->flags&VDIN_FLAG_RDMA_ENABLE),
			(next_wr_vfe->vf.canvas0Addr&0xff));
	/* prepare for chroma canvas*/
	if ((devp->prop.dest_cfmt == TVIN_NV12) ||
		(devp->prop.dest_cfmt == TVIN_NV21))
		vdin_set_chma_canvas_id(devp,
				(devp->flags&VDIN_FLAG_RDMA_ENABLE),
				(next_wr_vfe->vf.canvas0Addr>>8)&0xff);

	devp->curr_wr_vfe = next_wr_vfe;
	if (!(devp->flags&VDIN_FLAG_RDMA_ENABLE))
		vf_notify_receiver(devp->name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);


irq_handled:
	spin_unlock_irqrestore(&devp->isr_lock, flags);
#ifdef CONFIG_AML_RDMA
	if (devp->flags & VDIN_FLAG_RDMA_ENABLE)
		rdma_config(devp->rdma_handle,
			(rdma_enable&1)?devp->rdma_irq:RDMA_TRIGGER_MANUAL);
#endif
	isr_log(devp->vfp);

	return IRQ_HANDLED;
}
/*
 * there are too much logic in vdin_isr which is useless in camera&viu
 * so vdin_v4l2_isr use to the sample v4l2 application such as camera,viu
 */
static unsigned short skip_ratio = 1;
module_param(skip_ratio, ushort, 0664);
MODULE_PARM_DESC(skip_ratio,
		"\n vdin skip frame ratio 1/ratio will reserved.\n");

irqreturn_t vdin_v4l2_isr(int irq, void *dev_id)
{
	ulong flags;
	struct vdin_dev_s *devp = (struct vdin_dev_s *)dev_id;

	struct vf_entry *next_wr_vfe = NULL, *curr_wr_vfe = NULL;
	struct vframe_s *curr_wr_vf = NULL;
	unsigned int last_field_type, stamp;
	struct tvin_decoder_ops_s *decops;
	struct tvin_state_machine_ops_s *sm_ops;
	int ret = 0;
	int offset;

	if (!devp)
		return IRQ_HANDLED;
	if (vdin_reset_flag1) {
		vdin_reset_flag1 = 0;
		return IRQ_HANDLED;
	}
	isr_log(devp->vfp);
	irq_cnt++;
	spin_lock_irqsave(&devp->isr_lock, flags);
	vdin_reset_flag1 = vdin_vsync_reset_mif(devp->index);
	offset = devp->addr_offset;
	if (devp)
		/* avoid null pointer oops */
		stamp  = vdin_get_meas_vstamp(offset);
	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);
		/*save the first field stamp*/
		devp->stamp = stamp;
		goto irq_handled;
	}

	if ((devp->parm.port == TVIN_PORT_VIU) ||
		(devp->parm.port == TVIN_PORT_CAMERA)) {
		if (!vdin_write_done_check(offset, devp)) {
			if (vdin_dbg_en)
				pr_info("[vdin.%u] write undone skiped.\n",
						devp->index);
			goto irq_handled;
		}
	}

	if (devp->last_wr_vfe) {
		provider_vf_put(devp->last_wr_vfe, devp->vfp);
		devp->last_wr_vfe = NULL;
		vf_notify_receiver(devp->name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	}
	/*check vs is valid base on the time during continuous vs*/
	vdin_check_cycle(devp);

/* remove for m6&m8 camera function,
 * may cause hardware disable bug on kernel 3.10
 */
/* #if MESON_CPU_TYPE < MESON_CPU_TYPE_MESON8 */
/* if (devp->flags & VDIN_FLAG_DEC_STOP_ISR){ */
/* vdin_hw_disable(devp->addr_offset); */
/* devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR; */
/* goto irq_handled; */
/* } */
/* #endif */
	/* ignore invalid vs base on the continuous fields
	 * different cnt to void screen flicker
	 */

	last_field_type = devp->curr_field_type;
	devp->curr_field_type = vdin_get_curr_field_type(devp);

	curr_wr_vfe = devp->curr_wr_vfe;
	curr_wr_vf  = &curr_wr_vfe->vf;

	curr_wr_vf->type = last_field_type;
	curr_wr_vf->type_original = curr_wr_vf->type;

	vdin_set_vframe_prop_info(curr_wr_vf, devp);
	vdin_backup_histgram(curr_wr_vf, devp);

	if (devp->frontend && devp->frontend->dec_ops) {
		decops = devp->frontend->dec_ops;
		/*pass the histogram information to viuin frontend*/
		devp->frontend->private_data = &curr_wr_vf->prop;
		ret = decops->decode_isr(devp->frontend, devp->hcnt64);
		if (ret == TVIN_BUF_SKIP) {
			if (vdin_dbg_en)
				pr_info("%s bufffer(%u) skiped.\n",
						__func__, curr_wr_vf->index);
			goto irq_handled;
		/*if the current buffer is reserved,recycle tmp list,
		 * put current buffer to tmp list
		 */
		} else if (ret == TVIN_BUF_TMP) {
			recycle_tmp_vfs(devp->vfp);
			tmp_vf_put(curr_wr_vfe, devp->vfp);
			curr_wr_vfe = NULL;
		/*function of capture end,the reserved is the best*/
		} else if (ret == TVIN_BUF_RECYCLE_TMP) {
			tmp_to_rd(devp->vfp);
			goto irq_handled;
		}
	}
	/*check the skip frame*/
	if (devp->frontend && devp->frontend->sm_ops) {
		sm_ops = devp->frontend->sm_ops;
		if (sm_ops->check_frame_skip &&
			sm_ops->check_frame_skip(devp->frontend)) {
			goto irq_handled;
		}
	}
	next_wr_vfe = provider_vf_peek(devp->vfp);

	if (!next_wr_vfe) {
		/*add for force vdin buffer recycle*/
		if (devp->flags & VDIN_FLAG_FORCE_RECYCLE) {
			next_wr_vfe = receiver_vf_get(devp->vfp);
			if (next_wr_vfe)
				receiver_vf_put(&next_wr_vfe->vf, devp->vfp);
			else
				pr_err("[vdin.%d]force recycle error,no buffer in ready list",
						devp->index);
		} else {
			goto irq_handled;
		}
	}
	if (curr_wr_vfe) {
		curr_wr_vfe->flag |= VF_FLAG_NORMAL_FRAME;
		/* provider_vf_put(curr_wr_vfe, devp->vfp); */
		devp->last_wr_vfe = curr_wr_vfe;
	}

	/* prepare for next input data */
	next_wr_vfe = provider_vf_get(devp->vfp);
	vdin_set_canvas_id(devp, (devp->flags&VDIN_FLAG_RDMA_ENABLE),
				(next_wr_vfe->vf.canvas0Addr&0xff));
	if ((devp->prop.dest_cfmt == TVIN_NV12) ||
			(devp->prop.dest_cfmt == TVIN_NV21))
		vdin_set_chma_canvas_id(devp,
				(devp->flags&VDIN_FLAG_RDMA_ENABLE),
				(next_wr_vfe->vf.canvas0Addr>>8)&0xff);

	devp->curr_wr_vfe = next_wr_vfe;
	vf_notify_receiver(devp->name, VFRAME_EVENT_PROVIDER_VFRAME_READY,
			NULL);

irq_handled:
	spin_unlock_irqrestore(&devp->isr_lock, flags);
#ifdef CONFIG_AML_RDMA
	if (devp->flags & VDIN_FLAG_RDMA_ENABLE)
		rdma_config(devp->rdma_handle,
			(rdma_enable&1)?devp->rdma_irq:RDMA_TRIGGER_MANUAL);
#endif
	isr_log(devp->vfp);

	return IRQ_HANDLED;
}

static void vdin_sig_dwork(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct vdin_dev_s *devp =
		container_of(dwork, struct vdin_dev_s, sig_dwork);

	struct tvin_info_s *pre_info;

	if (!devp || !devp->frontend) {
		pr_info("%s, dwork error !!!\n", __func__);
		return;
	}

	pre_info = &devp->pre_info;

	cancel_delayed_work(&devp->sig_dwork);
	#if 0
	switch_set_state(&devp->sig_sdev, pre_info->status);
	#endif
	/* if (vdin_dbg_en) */
	pr_info("%s, dwork signal status: %d\n",
			__func__, pre_info->status);
}

static int vdin_open(struct inode *inode, struct file *file)
{
	struct vdin_dev_s *devp;
	int ret = 0;
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct vdin_dev_s, cdev);
	file->private_data = devp;

	if (devp->index >= VDIN_MAX_DEVS)
		return -ENXIO;

	if (devp->flags & VDIN_FLAG_FS_OPENED) {
		if (vdin_dbg_en)
			pr_info("%s, device %s opened already\n",
					__func__, dev_name(devp->dev));
		return 0;
	}

	devp->flags |= VDIN_FLAG_FS_OPENED;

	/* request irq */
	if (work_mode_simple) {
		if (vdin_dbg_en)
			pr_info("vdin.%d work in simple mode.\n", devp->index);
		ret = request_irq(devp->irq, vdin_isr_simple, IRQF_SHARED,
				devp->irq_name, (void *)devp);
	} else {
		if (vdin_dbg_en)
			pr_info("vdin.%d work in normal mode.\n", devp->index);
		ret = request_irq(devp->irq, vdin_isr, IRQF_SHARED,
				devp->irq_name, (void *)devp);
	}
	/* disable irq until vdin is configured completely */
	disable_irq_nosync(devp->irq);

	/* remove the hardware limit to vertical [0-max] */
	/* WRITE_VCBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000fff); */
	if (vdin_dbg_en)
		pr_info("open device %s ok\n", dev_name(devp->dev));
	return ret;
}

static int vdin_release(struct inode *inode, struct file *file)
{
	struct vdin_dev_s *devp = file->private_data;

	if (!(devp->flags & VDIN_FLAG_FS_OPENED)) {
		if (vdin_dbg_en)
			pr_info("%s, device %s not opened\n",
					__func__, dev_name(devp->dev));
		return 0;
	}

	devp->flags &= (~VDIN_FLAG_FS_OPENED);
	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		devp->flags |= VDIN_FLAG_DEC_STOP_ISR;
		vdin_stop_dec(devp);
		/* init flag */
		devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR;
		/* clear the flag of decode started */
		devp->flags &= (~VDIN_FLAG_DEC_STARTED);
	}
	if (devp->flags & VDIN_FLAG_DEC_OPENED) {
		vdin_close_fe(devp);
		devp->flags &= (~VDIN_FLAG_DEC_OPENED);
	}
	devp->flags &= (~VDIN_FLAG_SNOW_FLAG);

	/* free irq */
	free_irq(devp->irq, (void *)devp);

	file->private_data = NULL;

	/* reset the hardware limit to vertical [0-1079]  */
	/* WRITE_VCBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000437); */
	if (vdin_dbg_en)
		pr_info("close device %s ok\n", dev_name(devp->dev));
	return 0;
}

static long vdin_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct vdin_dev_s *devp = NULL;
	void __user *argp = (void __user *)arg;

	if (_IOC_TYPE(cmd) != _TM_T) {
		pr_err("%s invalid command: %u\n", __func__, cmd);
		return -EFAULT;
	}

	/* Get the per-device structure that contains this cdev */
	devp = file->private_data;

	switch (cmd) {
	case TVIN_IOC_OPEN: {
		struct tvin_parm_s parm = {0};

		mutex_lock(&devp->fe_lock);
		if (copy_from_user(&parm, argp, sizeof(struct tvin_parm_s))) {
			pr_err("TVIN_IOC_OPEN(%d) invalid parameter\n",
					devp->index);
			ret = -EFAULT;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		if (time_en)
			pr_info("TVIN_IOC_OPEN %ums.\n",
					jiffies_to_msecs(jiffies));
		if (devp->flags & VDIN_FLAG_DEC_OPENED) {
			pr_err("TVIN_IOC_OPEN(%d) port %s opend already\n",
					parm.index, tvin_port_str(parm.port));
			ret = -EBUSY;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		devp->parm.index = parm.index;
		devp->parm.port  = parm.port;
		devp->unstable_flag = false;
		ret = vdin_open_fe(devp->parm.port, devp->parm.index, devp);
		if (ret) {
			pr_err("TVIN_IOC_OPEN(%d) failed to open port 0x%x\n",
					devp->index, parm.port);
			ret = -EFAULT;
			mutex_unlock(&devp->fe_lock);
			break;
		}

		devp->flags |= VDIN_FLAG_DEC_OPENED;
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_OPEN(%d) port %s opened ok\n\n",
				parm.index,	tvin_port_str(devp->parm.port));
		mutex_unlock(&devp->fe_lock);
		break;
	}
	case TVIN_IOC_START_DEC: {
		struct tvin_parm_s parm = {0};
		enum tvin_sig_fmt_e fmt;

		mutex_lock(&devp->fe_lock);
		if (devp->flags & VDIN_FLAG_DEC_STARTED) {
			pr_err("TVIN_IOC_START_DEC(%d) port 0x%x, started already\n",
					parm.index, parm.port);
			ret = -EBUSY;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		if (time_en) {
			devp->start_time = jiffies_to_msecs(jiffies);
			pr_info("TVIN_IOC_START_DEC %ums.\n", devp->start_time);
		}
		if (((devp->parm.info.status != TVIN_SIG_STATUS_STABLE) ||
			(devp->parm.info.fmt == TVIN_SIG_FMT_NULL)) &&
			(devp->parm.port != TVIN_PORT_CVBS3)) {
			pr_err("TVIN_IOC_START_DEC: port %s start invalid\n",
					tvin_port_str(devp->parm.port));
			pr_err("	status: %s, fmt: %s\n",
				tvin_sig_status_str(devp->parm.info.status),
				tvin_sig_fmt_str(devp->parm.info.fmt));
			ret = -EPERM;
				mutex_unlock(&devp->fe_lock);
				break;
		}
		if (copy_from_user(&parm, argp, sizeof(struct tvin_parm_s))) {
			pr_err("TVIN_IOC_START_DEC(%d) invalid parameter\n",
					devp->index);
			ret = -EFAULT;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		if ((devp->parm.info.fmt == TVIN_SIG_FMT_NULL) &&
			(devp->parm.port == TVIN_PORT_CVBS3))
			fmt = devp->parm.info.fmt = TVIN_SIG_FMT_CVBS_NTSC_M;
		else
			fmt = devp->parm.info.fmt = parm.info.fmt;
		devp->fmt_info_p  =
				(struct tvin_format_s *)tvin_get_fmt_info(fmt);
		/* devp->fmt_info_p  =
		 * tvin_get_fmt_info(devp->parm.info.fmt);
		 */
		if (!devp->fmt_info_p) {
			pr_err("TVIN_IOC_START_DEC(%d) error, fmt is null\n",
					devp->index);
			ret = -EFAULT;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		vdin_start_dec(devp);
		devp->flags |= VDIN_FLAG_DEC_STARTED;
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_START_DEC port %s, decode started ok\n\n",
				tvin_port_str(devp->parm.port));
		mutex_unlock(&devp->fe_lock);
		break;
	}
	case TVIN_IOC_STOP_DEC:	{
		struct tvin_parm_s *parm = &devp->parm;

		mutex_lock(&devp->fe_lock);
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
			pr_err("TVIN_IOC_STOP_DEC(%d) decode havn't started\n",
					devp->index);
			ret = -EPERM;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		if (time_en) {
			devp->start_time = jiffies_to_msecs(jiffies);
			pr_info("TVIN_IOC_STOP_DEC %ums.\n", devp->start_time);
		}
		devp->flags |= VDIN_FLAG_DEC_STOP_ISR;
		vdin_stop_dec(devp);
	    /*
	     * if (devp->flags & VDIN_FLAG_FORCE_UNSTABLE)
	     * {
	     *	set_foreign_affairs(FOREIGN_AFFAIRS_00);
	     *	pr_info("video reset vpp.\n");
	     * }
	     * delay_cnt = 7;
	     * while (get_foreign_affairs(FOREIGN_AFFAIRS_00) && delay_cnt)
	     * {
	     *	mdelay(10);
	     *	delay_cnt--;
	     * }
	     */
		/* init flag */
		devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR;
		/* devp->flags &= ~VDIN_FLAG_FORCE_UNSTABLE; */
		/* clear the flag of decode started */
		devp->flags &= (~VDIN_FLAG_DEC_STARTED);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_STOP_DEC(%d) port %s, decode stop ok\n\n",
				parm->index, tvin_port_str(parm->port));
		mutex_unlock(&devp->fe_lock);
		break;
	}
	case TVIN_IOC_VF_REG:
		if ((devp->flags & VDIN_FLAG_DEC_REGED)
				== VDIN_FLAG_DEC_REGED) {
			pr_err("TVIN_IOC_VF_REG(%d) decoder is registered already\n",
					devp->index);
			ret = -EINVAL;
			break;
		}
		devp->flags |= VDIN_FLAG_DEC_REGED;
		vdin_vf_reg(devp);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_VF_REG(%d) ok\n\n", devp->index);
		break;
	case TVIN_IOC_VF_UNREG:
		if ((devp->flags & VDIN_FLAG_DEC_REGED)
				!= VDIN_FLAG_DEC_REGED) {
			pr_err("TVIN_IOC_VF_UNREG(%d) decoder isn't registered\n",
					devp->index);
			ret = -EINVAL;
			break;
		}
		devp->flags &= (~VDIN_FLAG_DEC_REGED);
		vdin_vf_unreg(devp);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_VF_REG(%d) ok\n\n", devp->index);
		break;
	case TVIN_IOC_CLOSE: {
		struct tvin_parm_s *parm = &devp->parm;
		enum tvin_port_e port = parm->port;

		mutex_lock(&devp->fe_lock);
		if (!(devp->flags & VDIN_FLAG_DEC_OPENED)) {
			pr_err("TVIN_IOC_CLOSE(%d) you have not opened port\n",
					devp->index);
			ret = -EPERM;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		if (time_en)
			pr_info("TVIN_IOC_CLOSE %ums.\n",
					jiffies_to_msecs(jiffies));
		vdin_close_fe(devp);
		devp->flags &= (~VDIN_FLAG_DEC_OPENED);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_CLOSE(%d) port %s closed ok\n\n",
					parm->index,
				tvin_port_str(port));
		mutex_unlock(&devp->fe_lock);
		break;
	}
	case TVIN_IOC_S_PARM: {
		struct tvin_parm_s parm = {0};

		if (copy_from_user(&parm, argp, sizeof(struct tvin_parm_s))) {
			ret = -EFAULT;
			break;
		}
		devp->parm.flag = parm.flag;
		devp->parm.reserved = parm.reserved;
		break;
	}
	case TVIN_IOC_G_PARM: {
		struct tvin_parm_s parm;

		memcpy(&parm, &devp->parm, sizeof(struct tvin_parm_s));
		/* if (devp->flags & VDIN_FLAG_FORCE_UNSTABLE)
		 *		parm.info.status = TVIN_SIG_STATUS_UNSTABLE;
		 */
		if (copy_to_user(argp, &parm, sizeof(struct tvin_parm_s)))
			ret = -EFAULT;
		break;
	}
	case TVIN_IOC_G_SIG_INFO: {
		struct tvin_info_s info;

		memset(&info, 0, sizeof(struct tvin_info_s));
		mutex_lock(&devp->fe_lock);
		/* if port is not opened, ignore this command */
		if (!(devp->flags & VDIN_FLAG_DEC_OPENED)) {
			ret = -EPERM;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		memcpy(&info, &devp->parm.info, sizeof(struct tvin_info_s));
		/* if (devp->flags & VDIN_FLAG_FORCE_UNSTABLE)
		 * info.status = TVIN_SIG_STATUS_UNSTABLE;
		 */

		if (info.status != TVIN_SIG_STATUS_STABLE)
			info.fps = 0;

		if (copy_to_user(argp, &info, sizeof(struct tvin_info_s)))
			ret = -EFAULT;
		mutex_unlock(&devp->fe_lock);
		break;
	}
	case TVIN_IOC_G_BUF_INFO: {
		struct tvin_buf_info_s buf_info;

		buf_info.buf_count	= devp->canvas_max_num;
		buf_info.buf_width	= devp->canvas_w;
		buf_info.buf_height = devp->canvas_h;
		buf_info.buf_size	= devp->canvas_max_size;
		buf_info.wr_list_size = devp->vfp->wr_list_size;
		if (copy_to_user(argp, &buf_info,
				sizeof(struct tvin_buf_info_s)))
			ret = -EFAULT;
		break;
	}
	case TVIN_IOC_START_GET_BUF:
		devp->vfp->wr_next = devp->vfp->wr_list.next;
		break;
	case TVIN_IOC_GET_BUF: {
		struct tvin_video_buf_s tvbuf;
		struct vf_entry *vfe;

		vfe = list_entry(devp->vfp->wr_next, struct vf_entry, list);
		devp->vfp->wr_next = devp->vfp->wr_next->next;
		if (devp->vfp->wr_next != &devp->vfp->wr_list)
			tvbuf.index = vfe->vf.index;
		else
			tvbuf.index = -1;

		if (copy_to_user(argp, &tvbuf, sizeof(struct tvin_video_buf_s)))
			ret = -EFAULT;
		break;
	}
	case TVIN_IOC_PAUSE_DEC:
		vdin_pause_dec(devp);
		break;
	case TVIN_IOC_RESUME_DEC:
		vdin_resume_dec(devp);
		break;
	case TVIN_IOC_FREEZE_VF: {
		mutex_lock(&devp->fe_lock);
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
			pr_err("TVIN_IOC_FREEZE_BUF(%d) decode havn't started\n",
					devp->index);
			ret = -EPERM;
			mutex_unlock(&devp->fe_lock);
			break;
		}

		if (devp->fmt_info_p->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE)
			vdin_vf_freeze(devp->vfp, 1);
		else
			vdin_vf_freeze(devp->vfp, 2);
		mutex_unlock(&devp->fe_lock);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_FREEZE_VF(%d) ok\n\n", devp->index);
		break;
	}
	case TVIN_IOC_UNFREEZE_VF: {
		mutex_lock(&devp->fe_lock);
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
			pr_err("TVIN_IOC_FREEZE_BUF(%d) decode havn't started\n",
					devp->index);
			ret = -EPERM;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		vdin_vf_unfreeze(devp->vfp);
		mutex_unlock(&devp->fe_lock);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_UNFREEZE_VF(%d) ok\n\n", devp->index);
		break;
	}
	case TVIN_IOC_CALLMASTER_SET: {
		enum tvin_port_e port_call = TVIN_PORT_NULL;
		struct tvin_frontend_s *fe = NULL;
		/* struct vdin_dev_s *devp = dev_get_drvdata(dev); */
		if (copy_from_user(&port_call,
						argp,
						sizeof(enum tvin_port_e))) {
			ret = -EFAULT;
			break;
		}
		fe = tvin_get_frontend(port_call, 0);
		if (fe && fe->dec_ops && fe->dec_ops->callmaster_det) {
			/*call the frontend det function*/
			callmaster_status =
				fe->dec_ops->callmaster_det(port_call, fe);
		}
		if (vdin_dbg_en)
			pr_info("[vdin.%d]:%s callmaster_status=%d,port_call=[%s]\n",
					devp->index, __func__,
					callmaster_status,
					tvin_port_str(port_call));
		break;
	}
	case TVIN_IOC_CALLMASTER_GET: {
		if (copy_to_user(argp, &callmaster_status, sizeof(int))) {
			ret = -EFAULT;
			break;
		}
		callmaster_status = 0;
		break;
	}
	case TVIN_IOC_SNOWON:
		devp->flags |= VDIN_FLAG_SNOW_FLAG;
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_SNOWON(%d) ok\n\n", devp->index);
		break;
	case TVIN_IOC_SNOWOFF:
		devp->flags &= (~VDIN_FLAG_SNOW_FLAG);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_SNOWOFF(%d) ok\n\n", devp->index);
		break;
	default:
		ret = -ENOIOCTLCMD;
	/* pr_info("%s %d is not supported command\n", __func__, cmd); */
		break;
	}
	return ret;
}
#ifdef CONFIG_COMPAT
static long vdin_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = vdin_ioctl(file, cmd, arg);
	return ret;
}
#endif
static int vdin_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct vdin_dev_s *devp = file->private_data;
	unsigned long start, len, off;
	unsigned long pfn, size;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;

	start = devp->mem_start & PAGE_MASK;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + devp->mem_size);

	off = vma->vm_pgoff << PAGE_SHIFT;

	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;

	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;

	size = vma->vm_end - vma->vm_start;
	pfn  = off >> PAGE_SHIFT;

	if (io_remap_pfn_range(vma, vma->vm_start, pfn, size,
							vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static const struct file_operations vdin_fops = {
	.owner	         = THIS_MODULE,
	.open	         = vdin_open,
	.release         = vdin_release,
	.unlocked_ioctl  = vdin_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vdin_compat_ioctl,
#endif
	.mmap	         = vdin_mmap,
};


static int vdin_add_cdev(struct cdev *cdevp,
		const struct file_operations *fops,
		int minor)
{
	int ret;
	dev_t devno = MKDEV(MAJOR(vdin_devno), minor);

	cdev_init(cdevp, fops);
	cdevp->owner = THIS_MODULE;
	ret = cdev_add(cdevp, devno, 1);
	return ret;
}

static struct device *vdin_create_device(struct device *parent, int minor)
{
	dev_t devno = MKDEV(MAJOR(vdin_devno), minor);

	return device_create(vdin_clsp, parent, devno, NULL, "%s%d",
			VDIN_DEV_NAME, minor);
}

static void vdin_delete_device(int minor)
{
	dev_t devno = MKDEV(MAJOR(vdin_devno), minor);

	device_destroy(vdin_clsp, devno);
}

static struct resource memobj;
static unsigned long mem_start, mem_end;
static unsigned int use_reserved_mem;
static int vdin_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct vdin_dev_s *vdevp;
	struct resource *res;
	unsigned int bit_mode = 8;
	/* const void *name; */
	/* int offset, size; */
	/* struct device_node *of_node = pdev->dev.of_node; */

	/* malloc vdev */
	vdevp = kmalloc(sizeof(struct vdin_dev_s), GFP_KERNEL);
	if (!vdevp)
		goto fail_kmalloc_vdev;
	memset(vdevp, 0, sizeof(struct vdin_dev_s));
	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"vdin_id", &(vdevp->index));
		if (ret) {
			pr_err("don't find  vdin id.\n");
			goto fail_get_resource_irq;
		}
	}
	vdin_devp[vdevp->index] = vdevp;
#ifdef CONFIG_AML_RDMA
	vdin_rdma_op.arg = vdin_devp;
	vdevp->rdma_handle = rdma_register(&vdin_rdma_op,
				NULL, RDMA_TABLE_SIZE);
	pr_info("%s:vdin.%d rdma hanld %d.\n", __func__, vdevp->index,
			vdevp->rdma_handle);
#endif
	/* create cdev and reigser with sysfs */
	ret = vdin_add_cdev(&vdevp->cdev, &vdin_fops, vdevp->index);
	if (ret) {
		pr_err("%s: failed to add cdev. !!!!!!!!!!\n", __func__);
		goto fail_add_cdev;
	}
	vdevp->dev = vdin_create_device(&pdev->dev, vdevp->index);
	if (IS_ERR(vdevp->dev)) {
		pr_err("%s: failed to create device. !!!!!!!!!!\n", __func__);
		ret = PTR_ERR(vdevp->dev);
		goto fail_create_device;
	}
	ret = vdin_create_device_files(vdevp->dev);
	if (ret < 0) {
		pr_err("%s: fail to create vdin attribute files.\n", __func__);
		goto fail_create_dev_file;
	}
	/* get memory address from resource */
#if 0
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
#elif 0
	res = &memobj;
	ret = find_reserve_block(pdev->dev.of_node->name, 0);
	if (ret < 0) {
		name = of_get_property(of_node, "share-memory-name", NULL);
		if (!name) {
			pr_err("\nvdin memory resource undefined1.\n");
			ret = -EFAULT;
			goto fail_get_resource_mem;
		} else {
			ret = find_reserve_block_by_name((char *)name);
			if (ret < 0) {
				pr_err("\nvdin memory resource undefined2.\n");
				ret = -EFAULT;
				goto fail_get_resource_mem;
			}
			name = of_get_property(of_node,
					"share-memory-offset", NULL);
			if (name)
				offset = of_read_ulong(name, 1);
			else {
				pr_err("\nvdin memory resource undefined3.\n");
				ret = -EFAULT;
				goto fail_get_resource_mem;
			}
			name = of_get_property(of_node,
						"share-memory-size", NULL);
			if (name)
				size = of_read_ulong(name, 1);
			else {
				pr_err("\nvdin memory resource undefined4.\n");
				ret = -EFAULT;
				goto fail_get_resource_mem;
			}
			res->start =
				(phys_addr_t)get_reserve_block_addr(ret)+offset;
			res->end = res->start + size-1;
		}
	} else {
		res->start = (phys_addr_t)get_reserve_block_addr(ret);
		res->end = res->start +
				(phys_addr_t)get_reserve_block_size(ret)-1;
	}
#else
	res = &memobj;
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret == 0)
		pr_info("\n vdin memory resource done.\n");
	else
		pr_info("\n vdin memory resource undefined!!\n");
#endif
/* if (!res) { */
/* pr_err("%s: can't get mem resource !!!!!!!!!!\n", __func__); */
/* ret = -ENXIO; */
/* goto fail_get_resource_mem; */
/* } */
/*  */
#ifdef CONFIG_CMA
	if (!use_reserved_mem) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"flag_cma", &(vdevp->cma_config_flag));
		if (ret) {
			pr_err("don't find  match flag_cma\n");
			vdevp->cma_config_flag = 0;
		}
		if (vdevp->cma_config_flag == 1) {
			ret = of_property_read_u32(pdev->dev.of_node,
				"cma_size",
				&(vdevp->cma_mem_size[vdevp->index]));
			if (ret)
				pr_err("don't find  match cma_size\n");
			else
				vdevp->cma_mem_size[vdevp->index] *= SZ_1M;
		} else if (vdevp->cma_config_flag == 0)
			vdevp->cma_mem_size[vdevp->index] =
				dma_get_cma_size_int_byte(&pdev->dev);
		vdevp->this_pdev[vdevp->index] = pdev;
		vdevp->cma_mem_alloc[vdevp->index] = 0;
		vdevp->cma_config_en = 1;
		pr_info("vdin%d cma_mem_size = %d MB\n", vdevp->index,
				(u32)vdevp->cma_mem_size[vdevp->index]/SZ_1M);
	}
#endif
	use_reserved_mem = 0;
	if (vdevp->cma_config_en != 1) {
		vdevp->mem_start = mem_start;
		vdevp->mem_size  = mem_end - mem_start + 1;
		pr_info("vdin%d mem_start = 0x%x, mem_size = 0x%x\n",
			vdevp->index, vdevp->mem_start, vdevp->mem_size);
	}

	/* get irq from resource */
	#if 0/* def CONFIG_USE_OF */
	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"interrupts", &(res->start));
		if (ret) {
			pr_err("don't find  match irq\n");
			goto fail_get_resource_irq;
		}
		ret = of_property_read_u32(pdev->dev.of_node,
				"rdma-irq", &(vdevp->rdma_irq));
		if (ret) {
			pr_err("don't find  match rdma irq, disable rdma\n");
			vdevp->rdma_irq = 0;
		}
		res->end = res->start;
		res->flags = IORESOURCE_IRQ;
	}
	#else
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("%s: can't get irq resource\n", __func__);
		ret = -ENXIO;
		goto fail_get_resource_irq;
	}
	ret = of_property_read_u32(pdev->dev.of_node,
				"rdma-irq", &(vdevp->rdma_irq));
	if (ret) {
		pr_err("don't find  match rdma irq, disable rdma\n");
		vdevp->rdma_irq = 0;
	}
	/* vdin0 for tv */
	if (vdevp->index == 0) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"tv_bit_mode", &bit_mode);
		if (ret)
			pr_info("no bit mode found, set 8bit as default\n");
		vdevp->color_depth_support = bit_mode;
		vdevp->color_depth_config = 0;
	}
	if (vdevp->color_depth_support&VDIN_WR_COLOR_DEPTH_10BIT_FULL_PCAK_MODE)
		vdevp->color_depth_mode = 1;
	else
		vdevp->color_depth_mode = 0;
	#endif
	vdevp->irq = res->start;
	snprintf(vdevp->irq_name, sizeof(vdevp->irq_name),
			"vdin%d-irq", vdevp->index);
	pr_info("vdin%d irq: %d rdma irq: %d\n", vdevp->index,
			vdevp->irq, vdevp->rdma_irq);
	/* init vdin parameters */
	vdevp->flags = VDIN_FLAG_NULL;
	vdevp->flags &= (~VDIN_FLAG_FS_OPENED);
	mutex_init(&vdevp->mm_lock);
	mutex_init(&vdevp->fe_lock);
	spin_lock_init(&vdevp->isr_lock);
	vdevp->frontend = NULL;

	/* @todo vdin_addr_offset */
	if (is_meson_gxbb_cpu() && vdevp->index)
		vdin_addr_offset[vdevp->index] = 0x70;
	vdevp->addr_offset = vdin_addr_offset[vdevp->index];

	vdevp->flags = 0;

	/*mif reset patch for vdin wr ram bug on gxtvbb*/
	if (is_meson_gxtvbb_cpu())
		enable_reset = 1;
	else
		enable_reset = 0;

	/* create vf pool */
	vdevp->vfp = vf_pool_alloc(VDIN_CANVAS_MAX_CNT);
	if (vdevp->vfp == NULL) {
		pr_err("%s: fail to alloc vf pool.\n", __func__);
		goto fail_alloc_vf_pool;
	}


	/* init vframe provider */
	/* @todo provider name */
	sprintf(vdevp->name, "%s%d", PROVIDER_NAME, vdevp->index);
	vf_provider_init(&vdevp->vprov, vdevp->name, &vdin_vf_ops, vdevp->vfp);
	/* @todo canvas_config_mode */
	if (canvas_config_mode == 0 || canvas_config_mode == 1)
		vdin_canvas_init(vdevp);

	/* set drvdata */
	dev_set_drvdata(vdevp->dev, vdevp);
	platform_set_drvdata(pdev, vdevp);

	/* set max pixel clk of vdin */
	vdin_set_config(vdevp);

	/* vdin measure clock */
	if (is_meson_gxbb_cpu()) {
		struct clk *clk;

		clk = clk_get(&pdev->dev, "xtal");
		vdevp->msr_clk = clk_get(&pdev->dev, "cts_vid_lock_clk");
		if (IS_ERR(clk) || IS_ERR(vdevp->msr_clk)) {
			pr_err("%s: vdin cannot get msr clk !!!\n", __func__);
			clk = NULL;
			vdevp->msr_clk = NULL;
		} else {
			clk_set_parent(vdevp->msr_clk, clk);
			vdevp->msr_clk_val = clk_get_rate(vdevp->msr_clk);
			clk_put(vdevp->msr_clk);
			pr_info("%s: vdin msr clock is %d MHZ\n", __func__,
					vdevp->msr_clk_val/1000000);
		}
	} else {
		struct clk *fclk_div5;
		unsigned int clk_rate;

		fclk_div5 = clk_get(&pdev->dev, "fclk_div5");
		if (IS_ERR(fclk_div5))
			pr_err("get fclk_div5 err\n");
		else {
			clk_rate = clk_get_rate(fclk_div5);
			pr_info("%s: fclk_div5 is %d MHZ\n", __func__,
					clk_rate/1000000);
		}
		vdevp->msr_clk = clk_get(&pdev->dev, "cts_vdin_meas_clk");
		if (IS_ERR(fclk_div5) || IS_ERR(vdevp->msr_clk)) {
			pr_err("%s: vdin cannot get msr clk !!!\n", __func__);
			fclk_div5 = NULL;
			vdevp->msr_clk = NULL;
		} else {
			clk_set_parent(vdevp->msr_clk, fclk_div5);
			clk_set_rate(vdevp->msr_clk, 50000000);
			if (!IS_ERR(vdevp->msr_clk)) {
				vdevp->msr_clk_val =
						clk_get_rate(vdevp->msr_clk);
				clk_put(vdevp->msr_clk);
				pr_info("%s: vdin[%d] clock is %d MHZ\n",
						__func__, vdevp->index,
						vdevp->msr_clk_val/1000000);
			} else
				pr_err("%s: vdin[%d] cannot get clock !!!\n",
						__func__, vdevp->index);
		}
	}
	/*disable vdin hardware*/
	vdin_enable_module(vdevp->addr_offset, false);

	vdevp->sig_wq = create_singlethread_workqueue(vdevp->name);
	INIT_DELAYED_WORK(&vdevp->sig_dwork, vdin_sig_dwork);
	#if 0
	vdevp->sig_sdev.name = vdevp->name;
	ret = switch_dev_register(&vdevp->sig_sdev);
	if (ret < 0)
		pr_err("[vdin] %d: register sdev error.\n", vdevp->index);
	#endif
	pr_info("%s: driver initialized ok\n", __func__);
	return 0;

fail_alloc_vf_pool:
fail_get_resource_irq:
fail_create_dev_file:
	vdin_delete_device(vdevp->index);
fail_create_device:
	cdev_del(&vdevp->cdev);
fail_add_cdev:
	kfree(vdevp);
fail_kmalloc_vdev:
	return ret;
}

static int vdin_drv_remove(struct platform_device *pdev)
{
	struct vdin_dev_s *vdevp;

	vdevp = platform_get_drvdata(pdev);

#ifdef CONFIG_AML_RDMA
	rdma_unregister(vdevp->rdma_handle);
#endif
	mutex_destroy(&vdevp->mm_lock);
	mutex_destroy(&vdevp->fe_lock);

	vf_pool_free(vdevp->vfp);
	vdin_remove_device_files(vdevp->dev);
	vdin_delete_device(vdevp->index);
	cdev_del(&vdevp->cdev);
	vdin_devp[vdevp->index] = NULL;
	kfree(vdevp);

	/* free drvdata */
	dev_set_drvdata(vdevp->dev, NULL);
	platform_set_drvdata(pdev, NULL);

	pr_info("%s: driver removed ok\n", __func__);
	return 0;
}

#ifdef CONFIG_PM
struct cpumask vdinirq_mask;
static int vdin_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct vdin_dev_s *vdevp;
	struct irq_desc *desc;
	const struct cpumask *mask;

	vdevp = platform_get_drvdata(pdev);
	if (vdevp->irq) {
		desc = irq_to_desc((long)vdevp->irq);
		mask = desc->irq_data.common->affinity;
#ifdef CONFIG_GENERIC_PENDING_IRQ
		if (irqd_is_setaffinity_pending(&desc->irq_data))
			mask = desc->pending_mask;
#endif
	cpumask_copy(&vdinirq_mask, mask);
	}
	vdin_enable_module(vdevp->addr_offset, false);
	pr_info("%s ok.\n", __func__);
	return 0;
}

static int vdin_drv_resume(struct platform_device *pdev)
{
	struct vdin_dev_s *vdevp;

	vdevp = platform_get_drvdata(pdev);
	vdin_enable_module(vdevp->addr_offset, true);

	if (vdevp->irq) {
		if (!irq_can_set_affinity(vdevp->irq))
			return -EIO;
		if (cpumask_intersects(&vdinirq_mask, cpu_online_mask))
			irq_set_affinity(vdevp->irq, &vdinirq_mask);

	}
	pr_info("%s ok.\n", __func__);
	return 0;
}
#endif

static void vdin_drv_shutdown(struct platform_device *pdev)
{
	struct vdin_dev_s *vdevp;

	vdevp = platform_get_drvdata(pdev);
	vdin_enable_module(vdevp->addr_offset, false);
	pr_info("%s ok.\n", __func__);
}

static const struct of_device_id vdin_dt_match[] = {
	{       .compatible = "amlogic, vdin",   },
	{},
};

static struct platform_driver vdin_driver = {
	.probe		= vdin_drv_probe,
	.remove		= vdin_drv_remove,
#ifdef CONFIG_PM
	.suspend	= vdin_drv_suspend,
	.resume		= vdin_drv_resume,
#endif
	.shutdown   = vdin_drv_shutdown,
	.driver	= {
		.name	        = VDIN_DRV_NAME,
		.of_match_table = vdin_dt_match,
}
};

/* extern int vdin_reg_v4l2(struct vdin_v4l2_ops_s *v4l2_ops); */
/* extern void vdin_unreg_v4l2(void); */

static int __init vdin_drv_init(void)
{
	int ret = 0;

	ret = alloc_chrdev_region(&vdin_devno, 0, VDIN_MAX_DEVS, VDIN_DEV_NAME);
	if (ret < 0) {
		pr_err("%s: failed to allocate major number\n", __func__);
		goto fail_alloc_cdev_region;
	}
	pr_info("%s: major %d\n", __func__, MAJOR(vdin_devno));

	vdin_clsp = class_create(THIS_MODULE, VDIN_CLS_NAME);
	if (IS_ERR(vdin_clsp)) {
		ret = PTR_ERR(vdin_clsp);
		pr_err("%s: failed to create class\n", __func__);
		goto fail_class_create;
	}

	ret = vdin_create_class_files(vdin_clsp);
	if (ret != 0) {
		pr_err("%s: failed to create class !!\n", __func__);
		goto fail_pdrv_register;
	}
	ret = platform_driver_register(&vdin_driver);

	if (ret != 0) {
		pr_err("%s: failed to register driver\n", __func__);
		goto fail_pdrv_register;
	}
	/* register vdin for v4l2 interface */
	if (vdin_reg_v4l2(&vdin_4v4l2_ops))
		pr_err("[vdin] %s: register vdin v4l2 error.\n", __func__);
	pr_info("%s: vdin driver init done\n", __func__);
	return 0;

fail_pdrv_register:
	class_destroy(vdin_clsp);
fail_class_create:
	unregister_chrdev_region(vdin_devno, VDIN_MAX_DEVS);
fail_alloc_cdev_region:
	return ret;
}

static void __exit vdin_drv_exit(void)
{
	vdin_unreg_v4l2();
	vdin_remove_class_files(vdin_clsp);
	class_destroy(vdin_clsp);
	unregister_chrdev_region(vdin_devno, VDIN_MAX_DEVS);
	platform_driver_unregister(&vdin_driver);
}


static int vdin_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	s32 ret = 0;

	if (!rmem) {
		pr_info("Can't get reverse mem!\n");
		ret = -EFAULT;
		return ret;
	}
	mem_start = rmem->base;
	mem_end = rmem->base + rmem->size - 1;
	if (rmem->size >= 0x1100000)
		use_reserved_mem = 1;
	pr_info("init vdin memsource 0x%lx->0x%lx use_reserved_mem:%d\n",
		mem_start, mem_end, use_reserved_mem);

	return 0;
}

static const struct reserved_mem_ops rmem_vdin_ops = {
	.device_init = vdin_mem_device_init,
};

static int __init vdin_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_vdin_ops;
	pr_info("vdin share mem setup\n");
	return 0;
}

module_init(vdin_drv_init);
module_exit(vdin_drv_exit);

RESERVEDMEM_OF_DECLARE(vdin, "amlogic, vdin_memory", vdin_mem_setup);

MODULE_VERSION(VDIN_VER);

MODULE_DESCRIPTION("AMLOGIC VDIN Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xu Lin <lin.xu@amlogic.com>");

#ifdef CONFIG_TVIN_VDIN_CTRL
int vdin_ctrl_open_fe(int no, int port)
{
	int ret = 0;
	struct vdin_dev_s *devp = vdin_devp[no];

	if (IS_ERR(devp)) {
		pr_err("[vdin]%s vdin%d has't registered,please register.\n",
				__func__, no);
		return -1;
	}
	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		pr_err("%s: port 0x%x, decode started already.\n",
				__func__, port);
		ret = -EBUSY;
	}

	devp->flags |= VDIN_FLAG_FS_OPENED;
	/* request irq */
	snprintf(devp->irq_name, sizeof(devp->irq_name),
			"vdin%d-irq", devp->index);
	if (work_mode_simple) {
		pr_info("vdin work in simple mode\n");
		ret = request_irq(devp->irq, vdin_isr_simple, IRQF_SHARED,
				devp->irq_name, (void *)devp);
	} else {
		pr_info("vdin work in normal mode\n");
		ret = request_irq(devp->irq, vdin_isr, IRQF_SHARED,
				devp->irq_name, (void *)devp);
	}

	/* disable irq until vdin is configured completely */
	disable_irq_nosync(devp->irq);
	/* remove the hardware limit to vertical [0-max]*/
	/* aml_write_vcbus(VPP_PREBLEND_VD1_V_START_END, 0x00000fff); */
	pr_info("open device %s ok\n", dev_name(devp->dev));
	vdin_open_fe(port, 0, devp);
	devp->parm.port = port;
	devp->parm.info.fmt = TVIN_SIG_FMT_NULL;
	return 0;
}

int vdin_ctrl_close_fe(int no)
{
	struct vdin_dev_s *devp;

	devp = vdin_devp[no];
	del_timer_sync(&devp->timer);

	/* close fe */
	if (devp->frontend->dec_ops->close)
		devp->frontend->dec_ops->close(devp->frontend);
	/* free the memory allocated in start tvin service */
	if (devp->parm.info.fmt >= TVIN_SIG_FMT_MAX)
		kfree(devp->fmt_info_p);
	devp->flags &= (~VDIN_FLAG_DEC_OPENED);
	devp->flags &= (~VDIN_FLAG_DEC_STARTED);
	/* free irq */
	free_irq(devp->irq, (void *)devp);
	return 0;
}


int vdin_ctrl_start_fe(int no, struct vdin_parm_s  *para)
{
	struct tvin_frontend_s *fe;
	int ret = 0;
	struct vdin_dev_s *devp = vdin_devp[no];

	if (IS_ERR(devp)) {
		pr_err("[vdin]%s vdin%d has't registered,please register.\n",
				__func__, no);
		return -1;
	}
	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		pr_err("%s: port 0x%x, decode started already.\n",
				__func__, para->port);
		ret = -EBUSY;
	}

	devp->parm.port         = para->port;
	devp->parm.info.fmt     = para->fmt;
	/* add for camera random resolution */
	if (para->fmt >= TVIN_SIG_FMT_MAX) {
		devp->fmt_info_p = kmalloc(sizeof(struct tvin_format_s),
				GFP_KERNEL);
		if (!devp->fmt_info_p)
			return -ENOMEM;
		devp->fmt_info_p->hs_bp     = para->hs_bp;
		devp->fmt_info_p->vs_bp     = para->vs_bp;
		devp->fmt_info_p->hs_pol    = para->hsync_phase;
		devp->fmt_info_p->vs_pol    = para->vsync_phase;
		devp->fmt_info_p->h_active  = para->h_active;
		devp->fmt_info_p->v_active  = para->v_active;
		devp->fmt_info_p->scan_mode = para->scan_mode;
		devp->fmt_info_p->duration  = 96000/para->frame_rate;
		devp->fmt_info_p->pixel_clk =
				para->h_active*para->v_active*para->frame_rate;
		devp->fmt_info_p->pixel_clk /= 10000;
	} else {
		devp->fmt_info_p = (struct tvin_format_s *)
				tvin_get_fmt_info(devp->parm.info.fmt);
	}
	if (!devp->fmt_info_p) {
		pr_err("%s(%d): error, fmt is null!!!\n", __func__, no);
		return -1;
	}
	fe = tvin_get_frontend(para->port, 0);
	if (fe) {
		fe->private_data = para;
		fe->port         = para->port;
		devp->frontend   = fe;

	} else {
		pr_err("%s(%d): not supported port 0x%x\n",
				__func__, no, para->port);
		return -1;
	}
	/* disable cut window? */
	devp->parm.cutwin.he = 0;
	devp->parm.cutwin.hs = 0;
	devp->parm.cutwin.ve = 0;
	devp->parm.cutwin.vs = 0;
	/*add for viu loop back scaler down*/
	if (para->port == TVIN_PORT_VIU) {
		devp->scaler4w = para->reserved & 0xffff;
		devp->scaler4h = para->reserved >> 16;
	}
/* #ifdef CONFIG_ARCH_MESON6 */
/* switch_mod_gate_by_name("vdin", 1); */
/* #endif */
	vdin_start_dec(devp);
	devp->flags = VDIN_FLAG_DEC_OPENED;
	devp->flags |= VDIN_FLAG_DEC_STARTED;
	return 0;
}

int vdin_ctrl_stop_fe(int no)
{
	struct vdin_dev_s *devp;

	devp = vdin_devp[no];
	if (!(devp->flags&VDIN_FLAG_DEC_STARTED)) {
		pr_err("%s:decode hasn't started.\n", __func__);
		return -EBUSY;
	}
	devp->flags |= VDIN_FLAG_DEC_STOP_ISR;
	vdin_stop_dec(devp);

	devp->flags &= (~VDIN_FLAG_DEC_STARTED);

	return 0;
}

enum tvin_sig_fmt_e  vdin_ctrl_get_fmt(int no)
{
	struct vdin_dev_s *devp;

	devp = vdin_devp[no];
	if (devp->frontend && devp->frontend->sm_ops)
		return  devp->frontend->sm_ops->get_fmt(devp->frontend);

	return TVIN_SIG_FMT_NULL;

}
#endif
