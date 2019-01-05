/*
 * drivers/amlogic/media/camera/common/vm.c
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
#include <linux/time.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vfm_ext.h>
#include <linux/amlogic/media/ge2d/ge2d_cmd.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/sched/rt.h>
#include <linux/platform_device.h>
#include "vm_log.h"
#include "vm.h"
#include <linux/ctype.h>
#include <linux/videodev2.h>
#include <media/videobuf-core.h>
#include <media/videobuf2-core.h>
#include <media/videobuf-dma-contig.h>
#include <media/videobuf-vmalloc.h>
#include <media/videobuf-dma-sg.h>
#include <linux/amlogic/media/v4l_util/videobuf-res.h>

#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/media/camera/vmapi.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/ctype.h>
#include <linux/of.h>
#include <linux/cdev.h>

#include <linux/sizes.h>
#include <linux/dma-mapping.h>
#include <linux/of_fdt.h>
#include <linux/dma-contiguous.h>
#include <linux/module.h>
#include <linux/of_reserved_mem.h>
#include <linux/list.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>

/*class property info.*/
#include "vmcls.h"

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
#if 1
#define GE2D_NV
#endif

#if 0
static unsigned int amlvm_time_log_enable;
module_param(amlvm_time_log_enable, uint, 0644);
MODULE_PARM_DESC(amlvm_time_log_enable, "enable vm time log when get frames");
#endif

#define MAX_VF_POOL_SIZE 8

#ifndef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
/*same as tvin pool*/
static int VM_POOL_SIZE = 6;
static int VF_POOL_SIZE = 6;
static int VM_CANVAS_ID = 24;
/*same as tvin pool*/
#endif


/*the counter of VM*/
#define VM_MAX_DEVS		2

static struct vm_device_s  *vm_device[VM_MAX_DEVS];

/* static bool isvmused; */

static void vm_cache_this_flush(unsigned int buf_start,
				unsigned int buf_size,
				struct vm_init_s *info);

static inline void vm_vf_put_from_provider(struct vframe_s *vf,
		unsigned int vdin_id);
#ifndef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
#define INCPTR(p) ptr_atomic_wrap_inc(&p)
#endif

#ifdef CONFIG_AMLOGIC_CAPTURE_FRAME_ROTATE
static int vmdecbuf_size[] = {
	0x13B3000,/* 5M */
	0xc00000,/* 3M */
	0x753000,/* 2M */
	0x4b0000,/* 1M3 */
	0x300000,/* 1M */
	0x12c000,/* VGA */
	0x4b000,/* QVGA */
};
static struct v4l2_frmsize_discrete canvas_config_wh[] = {
	{2624, 2624},
	{2048, 2048},
	{1600, 1600},
	{1280, 1280},
	{1024, 1024},
	{640, 640},
	{320, 320},
};
#else
static int vmdecbuf_size[] = {
	0xEE5000,/* 5M */
	0x900000,/* 3M */
	0x591000,/* 2M */
	0x384000,/* 1M3 */
	0x240000,/* 1M */
	0xF0000,/* VGA */
	0x3C000,/* QVGA */
};
static struct v4l2_frmsize_discrete canvas_config_wh[] = {
	{2624, 1984},
	{2048, 1536},
	{1600, 1216},
	{1280, 960},
	{1024, 768},
	{640, 512},
	{320, 256},
};
#endif
#define GE2D_ENDIAN_SHIFT        24
#define GE2D_ENDIAN_MASK            (0x1 << GE2D_ENDIAN_SHIFT)
#define GE2D_BIG_ENDIAN             (0 << GE2D_ENDIAN_SHIFT)
#define GE2D_LITTLE_ENDIAN          (1 << GE2D_ENDIAN_SHIFT)

#define PROVIDER_NAME "vm"

static dev_t vm_devno;
static struct class *vm_clsp;

#define VM_DEV_NAME	   "vm"
#define RECEIVER_NAME  "vm"
#define VM_CLS_NAME	   "vm"

static DEFINE_SPINLOCK(lock);

#ifndef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
static inline void ptr_atomic_wrap_inc(u32 *ptr)
{
	u32 i = *ptr;

	i++;
	if (i >= VM_POOL_SIZE)
		i = 0;
	*ptr = i;
}
#endif

#ifndef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
static struct vframe_s vfpool[MAX_VF_POOL_SIZE];
static s32 vfbuf_use[MAX_VF_POOL_SIZE];
static s32 fill_ptr, get_ptr, putting_ptr, put_ptr;
#endif

atomic_t waiting_flag = ATOMIC_INIT(0);

static inline struct vframe_s *vm_vf_get_from_provider(unsigned int vdin_id);
static inline struct vframe_s *vm_vf_peek_from_provider(unsigned int vdin_id);
static inline void vm_vf_put_from_provider(struct vframe_s *vf,
		unsigned int vdin_id);
static struct vframe_receiver_op_s *vf_vm_unreg_provider(
	struct vm_device_s *vdevp);
static struct vframe_receiver_op_s *vf_vm_reg_provider(struct vm_device_s
		*vdevp);
static void stop_vm_task(struct vm_device_s *vdevp);
#ifndef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
static int prepare_vframe(struct vframe_s *vf);
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/*
 ***********************************************
 *
 *   buffer op for video sink.
 *
 ***********************************************
 */

#ifdef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
static struct vframe_s *local_vf_peek(unsigned int vdin_id)
{
	struct vframe_s *vf = NULL;

	vf = vm_vf_peek_from_provider(vdin_id);
	if (vf) {
		if (vm_device[vdin_id]->vm_skip_count > 0) {
			vm_device[vdin_id]->vm_skip_count--;
			vm_vf_get_from_provider(vdin_id);
			vm_vf_put_from_provider(vf, vdin_id);
			vf = NULL;
		}
	}
	return vf;
}

static struct vframe_s *local_vf_get(unsigned int vdin_id)
{
	return vm_vf_get_from_provider(vdin_id);
}

static void local_vf_put(struct vframe_s *vf, unsigned int vdin_id)
{
	if (vf)
		vm_vf_put_from_provider(vf, vdin_id);
}
#else

static inline u32 index2canvas(u32 index)
{
	int i;
	int start_canvas, count;
	u32 canvas_tab[6];
	struct vdin_v4l2_ops_s *vops = get_vdin_v4l2_ops();

	vops->get_tvin_canvas_info(&start_canvas, &count);
	VM_POOL_SIZE  = count;
	VF_POOL_SIZE  = count;
	VM_CANVAS_ID = start_canvas;
	for (i = 0; i < count; i++)
		canvas_tab[i] =  VM_CANVAS_INDEX + i;
	return canvas_tab[index];
}

static struct vframe_s *vm_vf_peek(void *op_arg, unsigned int vdin_id)
{
	struct vframe_s *vf = NULL;

	vf = vm_vf_peek_from_provider(vdin_id);
	if (vf) {
		if (vm_device[vdin_id]->vm_skip_count > 0) {
			vm_device[vdin_id]->vm_skip_count--;
			vm_vf_get_from_provider(vdin_id);
			vm_vf_put_from_provider(vf, vdin_id);
			vf = NULL;
		}
	}
	return vf;
}

static struct vframe_s *vm_vf_get(void *op_arg, unsigned int vdin_id)
{
	return vm_vf_get_from_provider(vdin_id);
}

static void vm_vf_put(struct vframe_s *vf, void *op_arg)
{
	prepare_vframe(vf);
}

static int vm_vf_states(struct vframe_states *states, void *op_arg)
{
	return 0;
}

static struct vframe_s *local_vf_peek(void)
{
	if (get_ptr == fill_ptr)
		return NULL;
	return &vfpool[get_ptr];
}

static struct vframe_s *local_vf_get(unsigned int vdin_id)
{
	struct vframe_s *vf;

	if (get_ptr == fill_ptr)
		return NULL;
	vf = &vfpool[get_ptr];
	INCPTR(get_ptr);
	return vf;
}

static void local_vf_put(struct vframe_s *vf, unsigned int vdin_id)
{
	int i;
	int  canvas_addr;

	if (!vf)
		return;
	INCPTR(putting_ptr);
	for (i = 0; i < VF_POOL_SIZE; i++) {
		canvas_addr = index2canvas(i);
		if (vf->canvas0Addr == canvas_addr) {
			vfbuf_use[i] = 0;
			vm_vf_put_from_provider(vf, vdin_id);
		}
	}
}
#endif

#if 0
static int  local_vf_states(struct vframe_states *states)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&lock, flags);
	states->vf_pool_size = VF_POOL_SIZE;

	i = put_ptr - fill_ptr;
	if  (i < 0)
		i += VF_POOL_SIZE;
	states->buf_free_num = i;

	i = putting_ptr - put_ptr;
	if  (i < 0)
		i += VF_POOL_SIZE;
	states->buf_recycle_num = i;

	i = fill_ptr - get_ptr;
	if  (i < 0)
		i += VF_POOL_SIZE;
	states->buf_avail_num = i;

	spin_unlock_irqrestore(&lock, flags);
	return 0;
}
#endif

static int vm_receiver_event_fun(int type, void *data, void *private_data)
{
	struct vm_device_s *vdevp = (struct vm_device_s *)private_data;

	switch (type) {
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		/* if  (atomic_read(&waiting_flag))  { */
		wake_up_interruptible(&vdevp->frame_ready);
		/* atomic_set(&waiting_flag, 0); */
		/* } */
		/* up(&vb_start_sema); */
		/* printk("vdin %d frame ready !!!!!\n", vdevp->index); */
		break;
	case VFRAME_EVENT_PROVIDER_START:
		/* printk("vm register!!!!!\n"); */
		vf_vm_reg_provider(vdevp);
		vdevp->vm_skip_count = 0;
		vdevp->test_zoom = 0;
		break;
	case VFRAME_EVENT_PROVIDER_UNREG:
		/* printk("vm unregister!!!!!\n"); */
		vm_local_init();
		vf_vm_unreg_provider(vdevp);
		/* printk("vm unregister succeed!!!!!\n"); */
		break;
	default:
		break;
	}
	return 0;
}

static struct vframe_receiver_op_s vm_vf_receiver = {
	.event_cb = vm_receiver_event_fun
};

#ifndef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
static const struct vframe_operations_s vm_vf_provider = {
	.peek = vm_vf_peek,
	.get  = vm_vf_get,
	.put  = vm_vf_put,
	.vf_states = vm_vf_states,
};

static struct vframe_provider_s vm_vf_prov;
#endif


#ifndef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
int get_unused_vm_index(void)
{
	int i;

	for (i = 0; i < VF_POOL_SIZE; i++) {
		if (vfbuf_use[i] == 0)
			return i;
	}
	return -1;
}
static int prepare_vframe(struct vframe_s *vf)
{
	struct vframe_s *new_vf;
	int index;

	index = get_unused_vm_index();
	if (index < 0)
		return -1;
	new_vf = &vfpool[fill_ptr];
	memcpy(new_vf, vf, sizeof(struct vframe_s));
	vfbuf_use[index]++;
	INCPTR(fill_ptr);
	return 0;
}
#endif

/*
 ************************************************
 *
 *   buffer op for decoder, camera, etc.
 *
 **************************************************
 */

/* static const vframe_provider_t *vfp = NULL; */

void vm_local_init(void)
{
#ifndef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
	int i;

	for (i = 0; i < MAX_VF_POOL_SIZE; i++)
		vfbuf_use[i] = 0;
	fill_ptr = get_ptr = putting_ptr = put_ptr = 0;
#endif
}

static struct vframe_receiver_op_s *vf_vm_unreg_provider(
	struct vm_device_s *vdevp)
{
	/* ulong flags; */
#ifndef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
	vf_unreg_provider(&vm_vf_prov);
#endif
	stop_vm_task(vdevp);
	/* spin_lock_irqsave(&lock, flags); */
	/* vfp = NULL; */
	/* spin_unlock_irqrestore(&lock, flags); */
	return (struct vframe_receiver_op_s *)NULL;
}
EXPORT_SYMBOL(vf_vm_unreg_provider);

static struct vframe_receiver_op_s *vf_vm_reg_provider(struct vm_device_s
		*vdevp)
{
	ulong flags;
	/* int ret; */
	spin_lock_irqsave(&lock, flags);
	spin_unlock_irqrestore(&lock, flags);
	vm_buffer_init(vdevp);
#ifndef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
	vf_reg_provider(&vm_vf_prov);
#endif
	start_vm_task(vdevp);
#if 0
	start_simulate_task();
#endif
	return &vm_vf_receiver;
}
EXPORT_SYMBOL(vf_vm_reg_provider);

static inline struct vframe_s *vm_vf_peek_from_provider(unsigned int vdin_id)
{
	struct vframe_provider_s *vfp;
	struct vframe_s *vf;
	char name[20];

	sprintf(name, "%s%d", RECEIVER_NAME, vdin_id);
	vfp = vf_get_provider(name);
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return NULL;
	vf  = vfp->ops->peek(vfp->op_arg);
	return vf;
}

static inline struct vframe_s *vm_vf_get_from_provider(unsigned int vdin_id)
{
	struct vframe_provider_s *vfp;
	char name[20];

	sprintf(name, "%s%d", RECEIVER_NAME, vdin_id);
	vfp = vf_get_provider(name);
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return NULL;
	return vfp->ops->get(vfp->op_arg);
}

static inline void vm_vf_put_from_provider(struct vframe_s *vf,
		unsigned int vdin_id)
{
	struct vframe_provider_s *vfp;
	char name[20];

	sprintf(name, "%s%d", RECEIVER_NAME, vdin_id);
	vfp = vf_get_provider(name);
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return;
	vfp->ops->put(vf, vfp->op_arg);
}

/*
 ***********************************************
 *
 *   main task functions.
 *
 ***********************************************
 */

static unsigned int print_ifmt;
/* module_param(print_ifmt, unsigned int, 0644); */
/* MODULE_PARM_DESC(print_ifmt, "print input format\n"); */

static int get_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;

	if (vf->type & VIDTYPE_VIU_422) {
		if (vf->type & VIDTYPE_INTERLACE_BOTTOM)
			format =  GE2D_FORMAT_S16_YUV422 |
				  (GE2D_FORMAT_S16_YUV422B & (3 << 3));
		else if (vf->type & VIDTYPE_INTERLACE_TOP)
			format =  GE2D_FORMAT_S16_YUV422 |
				  (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		else
			format =  GE2D_FORMAT_S16_YUV422;
	} else if (vf->type & VIDTYPE_VIU_NV21) {
		if (vf->type & VIDTYPE_INTERLACE_BOTTOM)
			format =  GE2D_FORMAT_M24_NV21 |
				  (GE2D_FORMAT_M24_NV21B & (3 << 3));
		else if (vf->type & VIDTYPE_INTERLACE_TOP)
			format =  GE2D_FORMAT_M24_NV21 |
				  (GE2D_FORMAT_M24_NV21T & (3 << 3));
		else
			format =  GE2D_FORMAT_M24_NV21;
	} else {
		if (vf->type & VIDTYPE_INTERLACE_BOTTOM)
			format =  GE2D_FORMAT_M24_YUV420 |
				  (GE2D_FMT_M24_YUV420B & (3 << 3));
		else if (vf->type & VIDTYPE_INTERLACE_TOP)
			format =  GE2D_FORMAT_M24_YUV420 |
				  (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		else
			format =  GE2D_FORMAT_M24_YUV420;
	}
	if (print_ifmt == 1) {
		pr_debug("VIDTYPE_VIU_NV21=%x, vf->type=%x\n",
			 VIDTYPE_VIU_NV21, vf->type);
		pr_debug("format=%x, w=%d, h=%d\n",
			 format, vf->width, vf->height);
		print_ifmt = 0;
	}
	return format;
}

#ifdef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
static int calc_zoom(int *top, int *left, int *bottom, int *right, int zoom)
{
	u32 screen_width, screen_height;
	s32 start, end;
	s32 video_top, video_left, temp;
	u32 video_width, video_height;
	u32 ratio_x = 0;
	u32 ratio_y = 0;

	if (zoom < 100)
		zoom = 100;

	video_top = *top;
	video_left = *left;
	video_width = *right - *left + 1;
	video_height = *bottom - *top + 1;

	screen_width = video_width * zoom / 100;
	screen_height = video_height * zoom / 100;

	ratio_x = (video_width << 18) / screen_width;
	if (ratio_x * screen_width < (video_width << 18))
		ratio_x++;
	ratio_y = (video_height << 18) / screen_height;

	/* vertical */
	start = video_top + video_height / 2 - (video_height << 17) / ratio_y;
	end   = (video_height << 18) / ratio_y + start - 1;

	if (start < video_top) {
		temp = ((video_top - start) * ratio_y) >> 18;
		*top = temp;
	} else
		*top = 0;

	temp = *top + (video_height * ratio_y >> 18);
	*bottom = (temp <= (video_height - 1)) ? temp : (video_height - 1);

	/* horizontal */
	start = video_left + video_width / 2 - (video_width << 17) / ratio_x;
	end   = (video_width << 18) / ratio_x + start - 1;
	if (start < video_left) {
		temp = ((video_left - start) * ratio_x) >> 18;
		*left = temp;
	} else
		*left = 0;

	temp = *left + (video_width * ratio_x >> 18);
	*right = (temp <= (video_width - 1)) ? temp : (video_width - 1);
	return 0;
}
#endif

static int  get_input_frame(struct display_frame_s *frame, struct vframe_s *vf,
			    int zoom)
{
	int ret = 0;
	int top, left, bottom, right;

	if (!vf)
		return -1;

	frame->frame_top = 0;
	frame->frame_left = 0;
	frame->frame_width = vf->width;
	frame->frame_height = vf->height;
	top = 0;
	left = 0;
	bottom = vf->height - 1;
	right = vf->width - 1;
#ifdef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
	ret = calc_zoom(&top, &left, &bottom, &right, zoom);
#else
	ret = get_curren_frame_para(&top, &left, &bottom, &right);
#endif
	if (ret >= 0) {
		frame->content_top     =  top & (~1);
		frame->content_left    =  left & (~1);
		frame->content_width   =  vf->width - 2 * frame->content_left;
		frame->content_height  =  vf->height - 2 * frame->content_top;
	} else {
		frame->content_top     = 0;
		frame->content_left    =  0;
		frame->content_width   = vf->width;
		frame->content_height  = vf->height;
	}
	return 0;
}

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
	case V4L2_PIX_FMT_NV12:
#ifdef GE2D_NV
		format = GE2D_FORMAT_M24_NV12;
		break;
#endif
	case V4L2_PIX_FMT_NV21:
#ifdef GE2D_NV
		format = GE2D_FORMAT_M24_NV21;
		break;
#endif
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		format = GE2D_FORMAT_S8_Y;
		break;
	default:
		break;
	}
	return format;
}


struct vm_dma_contig_memory {
	u32 magic;
	void *vaddr;
	dma_addr_t dma_handle;
	unsigned long size;
	int is_userptr;
};

int is_need_ge2d_pre_process(struct vm_output_para output_para)
{
	int ret = 0;

	switch (output_para.v4l2_format) {
	case  V4L2_PIX_FMT_RGB565X:
	case  V4L2_PIX_FMT_YUV444:
	case  V4L2_PIX_FMT_VYUY:
	case  V4L2_PIX_FMT_BGR24:
	case  V4L2_PIX_FMT_RGB24:
	case  V4L2_PIX_FMT_YUV420:
	case  V4L2_PIX_FMT_YVU420:
	case  V4L2_PIX_FMT_NV12:
	case  V4L2_PIX_FMT_NV21:
		ret = 1;
		break;
	default:
		break;
	}
	return ret;
}

int is_need_sw_post_process(struct vm_output_para output_para)
{
	int ret = 0;

	switch (output_para.v4l2_memory) {
	case MAGIC_DC_MEM:
	case MAGIC_RE_MEM:
		goto exit;
	case MAGIC_SG_MEM:
	case MAGIC_VMAL_MEM:
	default:
		ret = 1;
		break;
	}
exit:
	return ret;
}

int get_canvas_index(int v4l2_format, int *depth)
{
	int canvas = vm_device[0]->vm_canvas[0];
	*depth = 16;
	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_VYUY:
		canvas = vm_device[0]->vm_canvas[0];
		*depth = 16;
		break;
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
		canvas = vm_device[0]->vm_canvas[1];
		*depth = 24;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
#ifdef GE2D_NV
		canvas = vm_device[0]->vm_canvas[2] |
		    (vm_device[0]->vm_canvas[3] << 8);
#else
		canvas = vm_device[0]->vm_canvas[2] |
		    (vm_device[0]->vm_canvas[3] << 8) |
		    (vm_device[0]->vm_canvas[4] << 16);
#endif
		*depth = 12;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		canvas = vm_device[0]->vm_canvas[5] |
		    (vm_device[0]->vm_canvas[6] << 8) |
		    (vm_device[0]->vm_canvas[7] << 16);
		*depth = 12;
		break;
	default:
		break;
	}
	return canvas;
}

int get_canvas_index_res(int ext_canvas, int v4l2_format, int *depth, int width,
			 int height, unsigned int buf)
{
	int canvas = ext_canvas;
	*depth = 16;
	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_VYUY:
		canvas = ext_canvas & 0xff;
		*depth = 16;
		canvas_config(canvas,
			      (unsigned long)buf,
			      width * 2, height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		break;
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
		canvas = ext_canvas & 0xff;
		*depth = 24;
		canvas_config(canvas,
			      (unsigned long)buf,
			      width * 3, height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		canvas_config(ext_canvas & 0xff,
			      (unsigned long)buf,
			      width, height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_config((ext_canvas & 0xff00) >> 8,
			      (unsigned long)(buf + width * height),
			      width, height / 2,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas = ext_canvas & 0xffff;
		*depth = 12;
		break;
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420:
		canvas_config(ext_canvas & 0xff,
			      (unsigned long)buf,
			      width, height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_config((ext_canvas & 0xff00) >> 8,
			      (unsigned long)(buf + width * height),
			      width / 2, height / 2,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_config((ext_canvas & 0xff0000) >> 16,
			      (unsigned long)(buf + width * height * 5 / 4),
			      width / 2, height / 2,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas = ext_canvas & 0xffffff;
		*depth = 12;
		break;
	default:
		break;
	}
	return canvas;
}

#if 0
static void vm_dump_mem(char *path, void *phy_addr, struct vm_output_para *para)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	unsigned int size = para->bytesperline * para->height;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	if (IS_ERR(filp)) {
		pr_err("create %s error.\n", path);
		return;
	}


	buf = phys_to_virt((unsigned long)phy_addr);
	vfs_write(filp, buf, size, &pos);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}

static void vm_x_mem(char *path, struct vm_output_para *para)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	unsigned int size = para->bytesperline * para->height;
	unsigned int canvas_index = para->index;
	struct canvas_s cv;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_CREAT | O_RDWR | O_APPEND, 0666);

	if (IS_ERR(filp)) {
		pr_err("failed to create %s, error %p.\n",
		       path, filp);
		return;
	}

	for (; canvas_index != 0; canvas_index >>= 8) {
		canvas_read(canvas_index & 0xff, &cv);
		/* printk("index=%lx,canvas.addr=%lx, w=%d, h=%d\n", */
		/* canvas_index, cv.addr, cv.width, cv.height); */

		buf = phys_to_virt(cv.addr);

		size = cv.width * cv.height;

		vfs_write(filp, buf, size, &pos);
		vfs_fsync(filp, 0);
	}
	filp_close(filp, NULL);
	set_fs(old_fs);
}
#endif

int vm_fill_this_buffer(struct videobuf_buffer *vb,
			struct vm_output_para *para, struct vm_init_s *info)

{
	int depth = 0;
	int ret = 0;
	int canvas_index = -1;
	int v4l2_format = V4L2_PIX_FMT_YUV444;
	int magic = 0;
	int ext_canvas;
	struct videobuf_buffer buf = {0};

	if (!info)
		return -1;
	if (info->vdin_id >= VM_MAX_DEVS) {
		pr_err("beyond the device array bound .\n");
		return -1;
	}
	/* if (info->isused == false) */
	/* return -2; */
#if 0
	if (!vb)
		goto exit;
#else
	if (!vb) {
		buf.width = 640;
		buf.height = 480;
		v4l2_format =  V4L2_PIX_FMT_YUV444;
		vb = &buf;
	}

	if (!vm_device[info->vdin_id]->task_running)
		return -1;
#endif

	v4l2_format = para->v4l2_format;
	magic = para->v4l2_memory;
	switch (magic) {
	case   MAGIC_DC_MEM:
		/* mem = vb->priv; */
		canvas_config(vm_device[0]->vm_canvas[11],
			      (dma_addr_t)para->vaddr,
			      vb->bytesperline, vb->height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_index =  vm_device[0]->vm_canvas[11];
		depth = (vb->bytesperline << 3) / vb->width;
		break;
	case  MAGIC_RE_MEM:
		if (para->ext_canvas != 0)
			canvas_index = get_canvas_index_res(
			       para->ext_canvas, v4l2_format,
			       &depth, vb->width,
			       (para->height == 0) ? vb->height : para->height,
			       (unsigned int)para->vaddr);
		else if (info->vdin_id == 0) {
			ext_canvas = ((vm_device[0]->vm_canvas[3]) |
			       (vm_device[0]->vm_canvas[4] << 8) |
			       (vm_device[0]->vm_canvas[5] << 16));
			canvas_index =
				get_canvas_index_res(ext_canvas,
				     v4l2_format, &depth, vb->width,
				     vb->height, (unsigned int)para->vaddr);
		} else {
			ext_canvas = ((vm_device[1]->vm_canvas[0]) |
			       (vm_device[1]->vm_canvas[1] << 8) |
			       (vm_device[1]->vm_canvas[2] << 16));
			canvas_index =
				get_canvas_index_res(ext_canvas,
				     v4l2_format, &depth, vb->width,
				     vb->height, (unsigned int)para->vaddr);
		}
		break;
	case  MAGIC_SG_MEM:
	case  MAGIC_VMAL_MEM:
		if (vm_device[info->vdin_id]->buffer_start &&
		    vm_device[info->vdin_id]->buffer_size)
			canvas_index = get_canvas_index(v4l2_format, &depth);
		break;
	default:
		canvas_index = vm_device[0]->vm_canvas[0];
		break;
	}
	vm_device[info->vdin_id]->output_para.width = vb->width;
	vm_device[info->vdin_id]->output_para.height = vb->height;
	vm_device[info->vdin_id]->output_para.bytesperline =
		(vb->width * depth) >> 3;
	vm_device[info->vdin_id]->output_para.index = canvas_index;
	vm_device[info->vdin_id]->output_para.v4l2_format = v4l2_format;
	vm_device[info->vdin_id]->output_para.v4l2_memory = magic;
	vm_device[info->vdin_id]->output_para.mirror = para->mirror;
	vm_device[info->vdin_id]->output_para.zoom = para->zoom;
	vm_device[info->vdin_id]->output_para.angle = para->angle;
	vm_device[info->vdin_id]->output_para.vaddr = para->vaddr;
	vm_device[info->vdin_id]->output_para.ext_canvas =
		(magic == MAGIC_RE_MEM) ? para->ext_canvas : 0;
	complete(&vm_device[info->vdin_id]->vb_start_sema);
	wait_for_completion(&vm_device[info->vdin_id]->vb_done_sema);
	if (magic == MAGIC_RE_MEM)
		vm_cache_this_flush((unsigned int)para->vaddr,
				    para->bytesperline *
				    para->height, info);

	return ret;
}

/*
 *for decoder input processing
 *  1. output window should 1:1 as source frame size
 *  2. keep the frame ratio
 *  3. input format should be YUV420 , output format should be YUV444
 */

int vm_ge2d_pre_process(struct vframe_s *vf,
					struct ge2d_context_s *context,
					struct config_para_ex_s *ge2d_config,
					struct vm_output_para output_para,
					unsigned int index)
{
	int ret;
	int src_top, src_left, src_width, src_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror = 0;
	int cur_angle = 0;
	struct display_frame_s input_frame = {0};

	ret = get_input_frame(&input_frame, vf, output_para.zoom);
	src_top = input_frame.content_top;
	src_left = input_frame.content_left;
	src_width = input_frame.content_width;
	src_height = input_frame.content_height;
	if (vm_device[index]->test_zoom) {
		vm_device[index]->test_zoom = 0;
		pr_debug("top is %d , left is %d\n",
			 input_frame.content_top,
			 input_frame.content_left);
		pr_debug("width is %d , height is %d\n",
			 input_frame.content_width,
			 input_frame.content_height);
	}
#ifdef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
	current_mirror = output_para.mirror;
	if (current_mirror < 0)
		current_mirror = 0;
#else
	current_mirror = camera_mirror_flag;
#endif

	cur_angle = output_para.angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	/* data operating. */
	ge2d_config->alu_const_color = 0; /* 0x000000ff; */
	ge2d_config->bitmask_en  = 0;
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
	canvas_read(output_para.index & 0xff, &cd);
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
	ge2d_config->dst_para.canvas_index = output_para.index & 0xff;
#ifdef GE2D_NV
	if ((output_para.v4l2_format != V4L2_PIX_FMT_YUV420)
	    && (output_para.v4l2_format != V4L2_PIX_FMT_YVU420))
		ge2d_config->dst_para.canvas_index = output_para.index;
#endif
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format =
		get_output_format(output_para.v4l2_format) |
		GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output_para.width;
	ge2d_config->dst_para.height = output_para.height;

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
	stretchblt_noalpha(context, src_left, src_top,
			   src_width, src_height, 0, 0,
			   output_para.width, output_para.height);

	/* for cr of  yuv420p or yuv420sp. */
	if ((output_para.v4l2_format == V4L2_PIX_FMT_YUV420)
	    || (output_para.v4l2_format == V4L2_PIX_FMT_YVU420)) {
		/* for cb. */
		canvas_read((output_para.index >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_para.index >> 8) & 0xff;
#ifndef GE2D_NV
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
#else
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN;
#endif
		ge2d_config->dst_para.width = output_para.width / 2;
		ge2d_config->dst_para.height = output_para.height / 2;
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
		stretchblt_noalpha(context, src_left, src_top,
				   src_width, src_height,
				   0, 0, ge2d_config->dst_para.width,
				   ge2d_config->dst_para.height);
	}
#ifndef GE2D_NV
	else if (output_para.v4l2_format == V4L2_PIX_FMT_NV12 ||
		 output_para.v4l2_format == V4L2_PIX_FMT_NV21) {
		canvas_read((output_para.index >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_para.index >> 8) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output_para.width / 2;
		ge2d_config->dst_para.height = output_para.height / 2;
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
		stretchblt_noalpha(context, src_left, src_top,
				   src_width, src_height, 0, 0,
				   ge2d_config->dst_para.width,
				   ge2d_config->dst_para.height);
	}
#endif

	/* for cb of yuv420p or yuv420sp. */
	if (output_para.v4l2_format == V4L2_PIX_FMT_YUV420 ||
	    output_para.v4l2_format == V4L2_PIX_FMT_YVU420
#ifndef GE2D_NV
	    || output_para.v4l2_format == V4L2_PIX_FMT_NV12 ||
	    output_para.v4l2_format == V4L2_PIX_FMT_NV21
#endif
	   ) {
		canvas_read((output_para.index >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_para.index >> 16) & 0xff;
#ifndef GE2D_NV
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN;
#else
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
#endif
		ge2d_config->dst_para.width = output_para.width / 2;
		ge2d_config->dst_para.height = output_para.height / 2;
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
		stretchblt_noalpha(context, src_left,
				   src_top, src_width, src_height,
				   0, 0, ge2d_config->dst_para.width,
				   ge2d_config->dst_para.height);
	}
	return output_para.index;
}

int vm_sw_post_process(int canvas, uintptr_t addr, unsigned int index)
{
	int poss = 0, posd = 0;
	int i = 0;
	void __iomem *buffer_y_start;
	void __iomem *buffer_u_start;
	void __iomem *buffer_v_start = 0;
	struct io_mapping *mapping_wc;
	int offset = 0;
	struct canvas_s canvas_work_y;
	struct canvas_s canvas_work_u;
	struct canvas_s canvas_work_v;
	struct vm_output_para output_para = vm_device[index]->output_para;

	if (!addr)
		return -1;
	mapping_wc = io_mapping_create_wc(vm_device[index]->buffer_start,
					  vm_device[index]->buffer_size);
	if (!mapping_wc)
		return -1;
	canvas_read(canvas & 0xff, &canvas_work_y);
	offset = 0;
	buffer_y_start = io_mapping_map_atomic_wc(mapping_wc, offset);
	if (buffer_y_start == NULL) {
		pr_err(" vm.postprocess:mapping buffer error\n");
		io_mapping_free(mapping_wc);
		return -1;
	}
	if (output_para.v4l2_format == V4L2_PIX_FMT_BGR24 ||
	    output_para.v4l2_format == V4L2_PIX_FMT_RGB24 ||
	    output_para.v4l2_format == V4L2_PIX_FMT_RGB565X) {
		for (i = 0; i < output_para.height; i++) {
			memcpy((void *)(addr + poss),
			       (void *)(buffer_y_start + posd),
			       output_para.bytesperline);
			poss += output_para.bytesperline;
			posd += canvas_work_y.width;
		}
		io_mapping_unmap_atomic(buffer_y_start);

	} else if (output_para.v4l2_format == V4L2_PIX_FMT_NV12 ||
		   output_para.v4l2_format == V4L2_PIX_FMT_NV21) {
#ifdef GE2D_NV
		unsigned int uv_width = output_para.width;
		unsigned int uv_height = output_para.height >> 1;

		posd = 0;
		for (i = output_para.height; i > 0; i--) { /* copy y */
			memcpy((void *)(addr + poss),
			       (void *)(buffer_y_start + posd),
			       output_para.width);
			poss += output_para.width;
			posd += canvas_work_y.width;
		}
		io_mapping_unmap_atomic(buffer_y_start);

		posd = 0;
		canvas_read((canvas >> 8) & 0xff, &canvas_work_u);
		offset = canvas_work_u.addr - canvas_work_y.addr;
		buffer_u_start = io_mapping_map_atomic_wc(mapping_wc, offset);
		for (i = uv_height; i > 0; i--) { /* copy uv */
			memcpy((void *)(addr + poss),
			       (void *)(buffer_u_start + posd), uv_width);
			poss += uv_width;
			posd += canvas_work_u.width;
		}

		io_mapping_unmap_atomic(buffer_u_start);
#else
		char *dst_buff = NULL;
		char *src_buff = NULL;
		char *src2_buff = NULL;

		canvas_read((canvas >> 8) & 0xff, &canvas_work_u);
		poss = posd = 0;
		for (i = 0; i < output_para.height; i += 2) { /* copy y */
			memcpy((void *)(addr + poss),
			       (void *)(buffer_y_start + posd),
			       output_para.width);
			poss += output_para.width;
			posd += canvas_work_y.width;
			memcpy((void *)(addr + poss),
			       (void *)(buffer_y_start + posd),
			       output_para.width);
			poss += output_para.width;
			posd += canvas_work_y.width;
		}
		io_mapping_unmap_atomic(buffer_y_start);

		posd = 0;
		canvas_read((canvas >> 16) & 0xff, &canvas_work_v);
		offset = canvas_work_u.addr - canvas_work_y.addr;
		buffer_u_start = io_mapping_map_atomic_wc(mapping_wc, offset);
		offset = canvas_work_v.addr - canvas_work_y.addr;
		buffer_v_start = io_mapping_map_atomic_wc(mapping_wc, offset);

		dst_buff = (char *)addr +
			   output_para.width * output_para.height;
		src_buff = (char *)buffer_u_start;
		src2_buff = (char *)buffer_v_start;
		if (output_para.v4l2_format == V4L2_PIX_FMT_NV12) {
			for (i = 0 ; i < output_para.height / 2; i++) {
				interleave_uv(src_buff, src2_buff,
					      dst_buff, output_para.width / 2);
				src_buff +=  canvas_work_u.width;
				src2_buff +=    canvas_work_v.width;
				dst_buff += output_para.width;
			}
		} else {
			for (i = 0 ; i < output_para.height / 2; i++) {
				interleave_uv(src2_buff, src_buff,
					      dst_buff, output_para.width / 2);
				src_buff +=  canvas_work_u.width;
				src2_buff +=    canvas_work_v.width;
				dst_buff += output_para.width;
			}
		}


		io_mapping_unmap_atomic(buffer_u_start);
		io_mapping_unmap_atomic(buffer_v_start);
#endif
	} else if ((output_para.v4l2_format == V4L2_PIX_FMT_YUV420)
		   || (output_para.v4l2_format == V4L2_PIX_FMT_YVU420)) {
		int uv_width = output_para.width >> 1;
		int uv_height = output_para.height >> 1;

		posd = 0;
		for (i = output_para.height; i > 0; i--) { /* copy y */
			memcpy((void *)(addr + poss),
			       (void *)(buffer_y_start + posd),
			       output_para.width);
			poss += output_para.width;
			posd += canvas_work_y.width;
		}
		io_mapping_unmap_atomic(buffer_y_start);

		posd = 0;
		canvas_read((canvas >> 8) & 0xff, &canvas_work_u);
		offset = canvas_work_u.addr - canvas_work_y.addr;
		buffer_u_start = io_mapping_map_atomic_wc(mapping_wc, offset);

		canvas_read((canvas >> 16) & 0xff, &canvas_work_v);
		offset = canvas_work_v.addr - canvas_work_y.addr;
		buffer_v_start = io_mapping_map_atomic_wc(mapping_wc, offset);

#ifndef GE2D_NV
		if (output_para.v4l2_format == V4L2_PIX_FMT_YUV420)
#else
		if (output_para.v4l2_format == V4L2_PIX_FMT_YVU420) {
#endif
			for (i = uv_height; i > 0; i--) { /* copy y */
				memcpy((void *)(addr + poss),
				       (void *)(buffer_u_start + posd),
				       uv_width);
				poss += uv_width;
				posd += canvas_work_u.width;
			}
		posd = 0;
		for (i = uv_height; i > 0; i--) { /* copy y */
			memcpy((void *)(addr + poss),
			       (void *)(buffer_v_start + posd),
			       uv_width);
			poss += uv_width;
			posd += canvas_work_v.width;
		}
	} else {
		for (i = uv_height; i > 0; i--) { /* copy v */
			memcpy((void *)(addr + poss),
			       (void *)(buffer_v_start + posd),
			       uv_width);
			poss += uv_width;
			posd += canvas_work_v.width;
		}
		posd = 0;
		for (i = uv_height; i > 0; i--) { /* copy u */
			memcpy((void *)(addr + poss),
			       (void *)(buffer_u_start + posd),
			       uv_width);
			poss += uv_width;
			posd += canvas_work_u.width;
		}
	}
	io_mapping_unmap_atomic(buffer_u_start);
	io_mapping_unmap_atomic(buffer_v_start);
}

	if (mapping_wc)
		io_mapping_free(mapping_wc);

	return 0;
}

static struct task_struct *simulate_task_fd;

static bool is_vf_available(unsigned int vdin_id)
{
	bool ret = ((local_vf_peek(vdin_id) != NULL) ||
			(!vm_device[vdin_id]->task_running));
	return ret;
}

/* static int reset_frame = 1; */
static int vm_task(void *data)
{
	int ret = 0;
	struct vframe_s *vf;
	int src_canvas;
	struct vm_device_s *devp = (struct vm_device_s *) data;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1 };
	struct ge2d_context_s *context = create_ge2d_work_queue();
	struct config_para_ex_s ge2d_config;

#ifdef CONFIG_AMLCAP_LOG_TIME_USEFORFRAMES
	struct timeval start;
	struct timeval end;
	unsigned long time_use = 0;
#endif
	memset(&ge2d_config, 0, sizeof(struct config_para_ex_s));
	sched_setscheduler(current, SCHED_FIFO, &param);
	allow_signal(SIGTERM);
	while (1) {
		ret = wait_for_completion_interruptible(
			      &devp->vb_start_sema);

		if (kthread_should_stop()) {
			complete(&devp->vb_done_sema);
			break;
		}

		/* wait for frame from 656 provider until 500ms runs out */
		wait_event_interruptible_timeout(devp->frame_ready,
						is_vf_available(devp->index),
						msecs_to_jiffies(2000));

		if (!devp->task_running) {
			ret = -1;
			goto vm_exit;
		}

		/* start to convert frame. */
#ifdef CONFIG_AMLCAP_LOG_TIME_USEFORFRAMES
		do_gettimeofday(&start);
#endif
		vf = local_vf_get(devp->index);

		if (vf) {
			src_canvas = vf->canvas0Addr;

			/* step1 convert 422 format to other format.*/
			if (is_need_ge2d_pre_process(devp->output_para))
				src_canvas = vm_ge2d_pre_process(vf, context,
					 &ge2d_config, devp->output_para,
					 devp->index);
#if 0
			if (devp->dump == 2) {
				vm_dump_mem(devp->dump_path,
			    (void *)output_para.vaddr, &output_para);
				devp->dump = 0;
			}
#endif
			local_vf_put(vf, devp->index);
#ifdef CONFIG_AMLCAP_LOG_TIME_USEFORFRAMES
			do_gettimeofday(&end);
			time_use = (end.tv_sec - start.tv_sec) * 1000 +
				   (end.tv_usec - start.tv_usec) / 1000;
			pr_debug("step 1, ge2d use: %ldms\n", time_use);
			do_gettimeofday(&start);
#endif

			/* step2 copy to user memory. */

			if (is_need_sw_post_process(devp->output_para))
				vm_sw_post_process(src_canvas,
				   devp->output_para.vaddr, devp->index);

#ifdef CONFIG_AMLCAP_LOG_TIME_USEFORFRAMES
			do_gettimeofday(&end);
			time_use = (end.tv_sec - start.tv_sec) * 1000 +
				   (end.tv_usec - start.tv_usec) / 1000;
			pr_debug("step 2, memcpy use: %ldms\n", time_use);
#endif
		}
		if (kthread_should_stop()) {
			complete(&devp->vb_done_sema);
			break;
		}
		complete(&devp->vb_done_sema);
	}
vm_exit:
	destroy_ge2d_work_queue(context);
	while (!kthread_should_stop()) {
		/*may not call stop, wait..
		 *it is killed by SIGTERM,eixt on
		 *down_interruptible.
		 *if not call stop,this thread
		 *may on do_exit and
		 *kthread_stop may not work good;
		 */
		msleep(20);/*default 10ms*/
	}
	return ret;
}

/*simulate v4l2 device to request*/
/*filling buffer,only for test use*/
static int simulate_task(void *data)
{
	while (1) {
		msleep(50);
		pr_debug("simulate succeed\n");
	}
	return 0;
}

/*
 ***********************************************
 *
 *   init functions.
 *
 ***********************************************
 */

int alloc_vm_canvas(struct vm_device_s *vdevp)
{
	int j;
	if (vdevp->index == 0) {
		for (j = 0; j < MAX_CANVAS_INDEX; j++) {
			memset(&(vdevp->vm_canvas[j]), -1, sizeof(int));
			vdevp->vm_canvas[j] =
				canvas_pool_map_alloc_canvas("vm0");
			if (vdevp->vm_canvas[j] < 0) {
				pr_err("alloc vm0_canvas %d failed\n", j);
				return -1;
			}
		}
	} else {
		for (j = 0; j < MAX_CANVAS_INDEX; j++) {
			memset(&(vdevp->vm_canvas[j]), -1, sizeof(int));
			vdevp->vm_canvas[j] =
				canvas_pool_map_alloc_canvas("vm1");
			if (vdevp->vm_canvas[j] < 0) {
				pr_err("alloc vm1_canvas %d failed\n", j);
				return -1;
			}
		}
	}

	return 0;
}

int vm_buffer_init(struct vm_device_s *vdevp)
{
	int i;
	u32 canvas_width, canvas_height;
	u32 decbuf_size;
	resource_size_t buf_start;
	unsigned int buf_size;
	int buf_num = 0;
	int local_pool_size = 0;

	init_completion(&vdevp->vb_start_sema);
	init_completion(&vdevp->vb_done_sema);

	buf_start = vdevp->buffer_start;
	buf_size = vdevp->buffer_size;

	if (!buf_start || !buf_size)
		goto exit;

	for (i = 0; i < ARRAY_SIZE(vmdecbuf_size); i++) {
		if (buf_size >= vmdecbuf_size[i])
			break;
	}
	if (i == ARRAY_SIZE(vmdecbuf_size)) {
		pr_debug("vmbuf size=%d less than the smallest vmbuf size%d\n",
			 buf_size, vmdecbuf_size[i - 1]);
		return -1;
	}

	canvas_width = canvas_config_wh[i].width;/* 1920; */
	canvas_height = canvas_config_wh[i].height;/* 1200; */
	decbuf_size = vmdecbuf_size[i];/* 0x700000; */
	buf_num  = buf_size / decbuf_size;

	if (buf_num > 0)
		local_pool_size   = 1;
	else {
		local_pool_size = 0;
		pr_debug("need at least one buffer to handle 1920*1080 data.\n");
	}

	for (i = 0; i < local_pool_size; i++) {
		canvas_config((vdevp->vm_canvas[0] + i),
			      (unsigned long)(buf_start + i * decbuf_size),
			      canvas_width * 2, canvas_height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_config((vdevp->vm_canvas[2] + i),
			      (unsigned long)(buf_start + i * decbuf_size),
			      canvas_width * 3, canvas_height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_config((vdevp->vm_canvas[4] + i),
			      (unsigned long)(buf_start + i * decbuf_size / 2),
			      canvas_width, canvas_height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_config(vdevp->vm_canvas[6] + i,
			      (unsigned long)(buf_start +
					      (i + 1)*decbuf_size / 2),
			      canvas_width, canvas_height / 2,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);

		canvas_config((vdevp->vm_canvas[8] + i),
			      (unsigned long)(buf_start +
					      (i + 1)*decbuf_size / 2),
			      canvas_width / 2, canvas_height / 2,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_config((vdevp->vm_canvas[10] + i),
			      (unsigned long)(buf_start +
					      (i + 3)*decbuf_size / 4),
			      canvas_width / 2, canvas_height / 2,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
#ifndef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
		vfbuf_use[i] = 0;
#endif
	}

exit:
	return 0;

}

int start_vm_task(struct vm_device_s *vdevp)
{
	/* init the device. */
	vm_local_init();
	if (!vdevp->task) {
		vdevp->task = kthread_create(vm_task, vdevp, vdevp->name);
		if (IS_ERR(vdevp->task)) {
			pr_err("thread creating error.\n");
			return -1;
		}
		init_waitqueue_head(&vdevp->frame_ready);
		wake_up_process(vdevp->task);
	}
	vdevp->task_running = 1;
	return 0;
}

int start_simulate_task(void)
{
	if (!simulate_task_fd) {
		simulate_task_fd = kthread_create(simulate_task, 0, "vm");
		if (IS_ERR(simulate_task_fd)) {
			pr_err("thread creating error.\n");
			return -1;
		}
		wake_up_process(simulate_task_fd);
	}
	return 0;
}


void stop_vm_task(struct vm_device_s *vdevp)
{
	if (vdevp->task) {
		vdevp->task_running = 0;
		send_sig(SIGTERM, vdevp->task, 1);
		complete(&vdevp->vb_start_sema);
		wake_up_interruptible(&vdevp->frame_ready);
		kthread_stop(vdevp->task);
		vdevp->task = NULL;
	}
	vm_local_init();
}


/*
 **********************************************************************
 *
 * global status.
 *
 **********************************************************************
 */

static int vm_enable_flag;

int get_vm_status(void)
{
	return vm_enable_flag;
}

void set_vm_status(int flag)
{
	if (flag >= 0)
		vm_enable_flag = flag;
	else
		vm_enable_flag = 0;
}

/*
 **********************************************************************
 *
 * file op section.
 *
 **********************************************************************
 */

void unset_vm_buf_res(struct vm_init_s *info)
{
	info->buffer_start = 0;
}

static void vm_cache_this_flush(unsigned int buf_start, unsigned int buf_size,
				struct vm_init_s *info)
{
	struct vm_device_s *devp = vm_device[info->vdin_id];

	if (devp->dev) {
		if ((buf_start >= info->buffer_start) &&
		    ((buf_start + buf_size) <= (info->buffer_start +
						info->vm_buf_size)))
			dma_sync_single_for_cpu(devp->dev,
				buf_start, buf_size, DMA_FROM_DEVICE);
	}
}

static int vm_open(struct inode *inode, struct file *file)
{
	struct ge2d_context_s *context = NULL;

	pr_info("open one vm device\n");
	file->private_data = context;
	/* vm_device.open_count++; */
	return 0;
}

static long vm_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	int  ret = 0;
	struct ge2d_context_s *context;
	void  __user *argp;

	context = (struct ge2d_context_s *)filp->private_data;
	argp = (void __user *)args;
	switch (cmd) {
	case VM_IOC_2OSD0:
		break;
	case VM_IOC_ENABLE_PP:
		break;
	case VM_IOC_CONFIG_FRAME:
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return ret;
}

static int vm_release(struct inode *inode, struct file *file)
{
	struct ge2d_context_s *context =
		(struct ge2d_context_s *)file->private_data;

	if (context && (destroy_ge2d_work_queue(context) == 0)) {
		/* vm_device.open_count--; */
		return 0;
	}
	pr_info("release one vm device\n");
	return -1;
}

/*
 **********************************************************************
 *
 * file op init section.
 *
 **********************************************************************
 */

static const struct file_operations vm_fops = {
	.owner = THIS_MODULE,
	.open = vm_open,
	.unlocked_ioctl = vm_ioctl,
	.release = vm_release,
};

static ssize_t vm_attr_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	ssize_t len = 0;
	struct vm_device_s *devp;

	devp = dev_get_drvdata(dev);
	if (devp->task_running == 0) {
		len += sprintf(buf + len, "vm does not start\n");
		return len;
	}

	len += sprintf((char *)buf + len, "vm parameters below\n");

	return len;
}

static ssize_t vm_attr_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct vm_device_s *devp;
	unsigned int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[6] = {NULL};

	if (!buf)
		return len;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	/* printk(KERN_INFO "input cmd : %s",buf_orig); */
	devp = dev_get_drvdata(dev);
	if (devp->task_running == 0) {
		len += sprintf((char *)buf + len, "vm does not start\n");
		return len;
	}

	ps = buf_orig;
	while (1) {
		if (n >= ARRAY_SIZE(parm)) {
			pr_err("parm array overflow.\n");
#if 0
			pr_err("n=%d,ARRAY_SIZE(parm)=%d\n",
			       n, ARRAY_SIZE(parm));
#endif
			return len;
		}
		token = strsep(&ps, "\n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (strcmp(parm[0], "before") == 0) {
		devp->dump = 1;
		devp->dump_path = parm[1];
		pr_debug("this not support\n");
	} else if (strcmp(parm[0], "after") == 0) {
		devp->dump = 2;
		devp->dump_path = parm[1];
		pr_debug("after ge2d processed, store to %s\n", parm[1]);
	}

	kfree(buf_orig);
	return len;
}

static DEVICE_ATTR(dump, 0664, vm_attr_show, vm_attr_store);

static int vm_add_cdev(struct cdev *cdevp,
		       const struct file_operations *fops,
		       int minor)
{
	int ret;
	dev_t devno = MKDEV(MAJOR(vm_devno), minor);

	cdev_init(cdevp, fops);
	cdevp->owner = THIS_MODULE;
	ret = cdev_add(cdevp, devno, 1);
	return ret;
}

int init_vm_device(struct vm_device_s *vdevp, struct platform_device *pdev)
{
	int  ret = 0;

	ret = vm_add_cdev(&vdevp->cdev, &vm_fops, vdevp->index);
	if (ret) {
		pr_err("%s: failed to add cdev. !\n", __func__);
		goto fail_add_cdev;
	}
	vdevp->dev = device_create(vm_clsp, &pdev->dev, MKDEV(MAJOR(vm_devno),
				   vdevp->index),
				   NULL, "%s%d", VM_DEV_NAME, vdevp->index);
	if (IS_ERR(vdevp->dev)) {
		pr_err("%s: failed to create device. !\n", __func__);
		ret = PTR_ERR(vdevp->dev);
		goto fail_create_device;
	}
	ret = device_create_file(vdevp->dev, &dev_attr_dump);
	if (ret < 0) {
		pr_err("%s: fail to create vdin attribute files.\n", __func__);
		goto fail_create_dev_file;
	}

	dev_set_drvdata(vdevp->dev, vdevp);
	platform_set_drvdata(pdev, vdevp);

	/* if (vm_buffer_init(vdevp) < 0) */
	/* goto unregister_dev; */
#ifndef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
	vf_provider_init(&vm_vf_prov, PROVIDER_NAME, &vm_vf_provider, NULL);
#endif
	/* vf_reg_provider(&vm_vf_prov); */
	vdevp->task = NULL;
	vdevp->task_running = 0;
	memset(&vdevp->output_para, 0, sizeof(struct vm_output_para));
	sprintf(vdevp->name, "%s%d", RECEIVER_NAME, vdevp->index);
	sprintf(vdevp->vf_provider_name, "%s%d", "vdin", vdevp->index);

	snprintf(vdevp->vfm_map_chain, VM_MAP_NAME_SIZE,
				"%s %s", vdevp->vf_provider_name,
				vdevp->name);
	snprintf(vdevp->vfm_map_id, VM_MAP_NAME_SIZE,
				"vm-map-%d", vdevp->index);
	if (vfm_map_add(vdevp->vfm_map_id,
		vdevp->vfm_map_chain) < 0) {
		pr_err("vm pipeline map creation failed %s.\n",
			vdevp->vfm_map_id);
		goto fail_create_dev_file;
	}

	vf_receiver_init(&vdevp->vm_vf_recv, vdevp->name,
			&vm_vf_receiver, vdevp);

	vf_reg_receiver(&vdevp->vm_vf_recv);
	return 0;

	/* unregister_dev: */
fail_create_dev_file:
	device_destroy(vm_clsp, MKDEV(MAJOR(vm_devno), vdevp->index));
fail_create_device:
	cdev_del(&vdevp->cdev);
fail_add_cdev:
	kfree(vdevp);
	return ret;

}

int uninit_vm_device(struct platform_device *plat_dev)
{
	struct vm_device_s *vdevp;

	vdevp = platform_get_drvdata(plat_dev);
	/* stop_vm_task(vdevp); */
	device_remove_file(vdevp->dev, &dev_attr_dump);
	device_destroy(vm_clsp, MKDEV(MAJOR(vm_devno), vdevp->index));
	cdev_del(&vdevp->cdev);
	vm_device[vdevp->index] = NULL;

	/* free drvdata */
	dev_set_drvdata(vdevp->dev, NULL);
	platform_set_drvdata(plat_dev, NULL);
	kfree(vdevp);
	return 0;
}

int vm_init_resource(size_t size, struct vm_init_s *info)
{
	struct vm_device_s *devp;
#ifdef CONFIG_CMA
	devp = vm_device[info->vdin_id];
	if (size == 0)
		return -1;
	if (info->vm_pages && info->vm_buf_size != 0) {

		dma_release_from_contiguous(&devp->pdev->dev,
					    info->vm_pages,
					    info->vm_buf_size / PAGE_SIZE);
	}
	info->vm_pages = dma_alloc_from_contiguous(&devp->pdev->dev,
			 size / PAGE_SIZE, 0);
	if (info->vm_pages) {
		dma_addr_t phys;

		phys = page_to_phys(info->vm_pages);
		info->buffer_start = phys;
		info->vm_buf_size = size;
		if (info->bt_path_count == 1) {
			if (info->vdin_id == 0)
				info->isused = true;
			else
				info->isused = false;
		} else {
			info->isused = true;
		}
		info->mem_alloc_succeed = true;
		return 0;
	}

	info->mem_alloc_succeed = false;
	pr_err("CMA failed to allocate dma buffer\n");
	return -ENOMEM;
#else
	if (size == 0)
		return -1;
	info->buffer_start = vm_device[info->vdin_id]->buffer_start;
	info->vm_buf_size = vm_device[info->vdin_id]->buffer_size;
	info->mem_alloc_succeed = true;
	info->isused = true;
	return 0;
#endif
}
EXPORT_SYMBOL(vm_init_resource);

void vm_deinit_resource(struct vm_init_s *info)
{
#ifdef CONFIG_CMA
	struct vm_device_s *devp;

	devp = vm_device[info->vdin_id];
	if (info->vm_buf_size == 0) {
		pr_warn("vm buf size equals 0\n");
		return;
	}
	unset_vm_buf_res(info);

	if (info->vm_pages) {
		dma_release_from_contiguous(&devp->pdev->dev,
					    info->vm_pages,
					    info->vm_buf_size / PAGE_SIZE);
		info->vm_buf_size = 0;
		info->vm_pages = NULL;
		info->isused = false;
		info->mem_alloc_succeed = false;
	}
#else
	if (info->vm_buf_size) {
		info->buffer_start = 0;
		info->vm_buf_size = 0;
		info->isused = false;
		info->mem_alloc_succeed = false;
	}
#endif
}
EXPORT_SYMBOL(vm_deinit_resource);


/*******************************************************************
 *
 * interface for Linux driver
 *
 * ******************************************************************/

static int vm_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct vm_device_s *vdevp = platform_get_drvdata(pdev);
	unsigned long mem_start, mem_end;

	vdevp->buffer_start = rmem->base;
	vdevp->buffer_size = rmem->size;
	mem_start = rmem->base;
	mem_end = rmem->base + rmem->size - 1;
	pr_info("init vm memsource %lx->%lx\n", mem_start, mem_end);
	return 0;
}

static const struct reserved_mem_ops rmem_vm_ops = {
	.device_init = vm_mem_device_init,
};

static int __init vm_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_vm_ops;
	pr_info("vm share mem setup\n");
	return 0;
}

/* MODULE_AMLOG(AMLOG_DEFAULT_LEVEL, 0xff, LOG_LEVEL_DESC, LOG_MASK_DESC); */


/* for driver. */
static int vm_driver_probe(struct platform_device *pdev)
{
	int idx;
	int ret;
	struct vm_device_s *vdevp;

	pr_info("vm memory init\n");
	/* malloc vdev */
	vdevp = kmalloc(sizeof(struct vm_device_s), GFP_KERNEL);
	if (!vdevp)
		return -ENOMEM;

	memset(vdevp, 0, sizeof(struct vm_device_s));

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
					   "vm_id", &(vdevp->index));
		if (ret)
			pr_err("don't find vm id.\n");
	}
	vm_device[vdevp->index] = vdevp;
	vdevp->pdev = pdev;
	platform_set_drvdata(pdev, vdevp);
	ret = alloc_vm_canvas(vdevp);
	if (ret != 0) {
		pr_err("alloc vm canvas failed\n");
		return ret;
	}
	vm_buffer_init(vdevp);
	ret = init_vm_device(vdevp, pdev);
	if (ret != 0)
		return ret;
	idx = of_reserved_mem_device_init(&pdev->dev);
	if (idx == 0)
		pr_info("vm probe done\n");
	else
		pr_info("malloc reserved memory failed !\n");

	/* init_vm_device(vdevp, pdev); */
	return 0;
}

static int vm_drv_remove(struct platform_device *plat_dev)
{
	int i;
	struct vm_device_s *vdevp;

	vdevp = platform_get_drvdata(plat_dev);

	for (i = 0; i < MAX_CANVAS_INDEX; i++) {
		if ((vdevp != NULL) &&
			(vdevp->vm_canvas[i] >= 0)) {
			canvas_pool_map_free_canvas(
			   vdevp->vm_canvas[i]);
			memset(&(vdevp->vm_canvas[i]),
				-1, sizeof(int));
		}
	}

	if (vdevp->vfm_map_id[0]) {
		vfm_map_remove(vdevp->vfm_map_id);
		vdevp->vfm_map_id[0] = 0;
	}

	uninit_vm_device(plat_dev);
	return 0;
}

static const struct of_device_id amlogic_vm_dt_match[] = {
	{
		.compatible = "amlogic, vm",
	},
	{},
};

/* general interface for a linux driver .*/
static struct platform_driver vm_drv = {
	.probe  = vm_driver_probe,
	.remove = vm_drv_remove,
	.driver = {
		.name = "vm",
		.owner = THIS_MODULE,
		.of_match_table = amlogic_vm_dt_match,
	}
};

static int __init
vm_init_module(void)
{
	int ret = 0;

	pr_info("vm_init .\n");
	ret = alloc_chrdev_region(&vm_devno, 0, VM_MAX_DEVS, VM_DEV_NAME);
	if (ret < 0) {
		pr_err("%s: failed to allocate major number\n", __func__);
		goto fail_alloc_cdev_region;
	}
	vm_clsp = class_create(THIS_MODULE, VM_CLS_NAME);
	if (IS_ERR(vm_clsp)) {
		ret = PTR_ERR(vm_clsp);
		pr_err("%s: failed to create class\n", __func__);
		goto fail_class_create;
	}
	ret = platform_driver_register(&vm_drv);
	if (ret) {
		pr_err("Failed to register vm driver (error=%d\n",
		       ret);
		goto fail_pdrv_register;
	}
	return 0;

fail_pdrv_register:
	class_destroy(vm_clsp);
fail_class_create:
	unregister_chrdev_region(vm_devno, VM_MAX_DEVS);
fail_alloc_cdev_region:
	return ret;
}

static void __exit
vm_remove_module(void)
{
	/* class_remove_file(vm_clsp, &class_attr_dump); */
	class_destroy(vm_clsp);
	unregister_chrdev_region(vm_devno, VM_MAX_DEVS);
	platform_driver_unregister(&vm_drv);
	pr_info("vm module removed.\n");
}

module_init(vm_init_module);
module_exit(vm_remove_module);
RESERVEDMEM_OF_DECLARE(vm, "amlogic, vm_memory", vm_mem_setup);

MODULE_DESCRIPTION("Amlogic Video Input Manager");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Zheng <simon.zheng@amlogic.com>");
