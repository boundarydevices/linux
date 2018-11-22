/*
 * drivers/amlogic/media/video_processor/pic_dev/picdec.c
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
#include <linux/of_reserved_mem.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/sched/rt.h>
#include <linux/platform_device.h>
#include "picdec.h"
#include <linux/videodev2.h>
#include <media/videobuf-core.h>
#include <media/videobuf2-core.h>
#include <media/videobuf-dma-contig.h>
#include <media/videobuf-vmalloc.h>
#include <media/videobuf-dma-sg.h>
#include <linux/amlogic/media/v4l_util/videobuf-res.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/ctype.h>
#include <linux/of.h>
#include <linux/sizes.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/dma-mapping.h>
#include <linux/of_fdt.h>
#include <linux/dma-contiguous.h>

static int debug_flag;
static int dump_file_flag;
static int p2p_mode = 2;
static int output_format_mode = 1;
static int txlx_output_format_mode = 1;
static int cma_layout_flag = 1;

#define NO_TASK_MODE

#define aml_pr_info(level, fmt, arg...)	\
do {					\
	if (debug_flag >= level)	\
		pr_info(fmt, ## arg);	\
} while (0)

#define DEBUG
#ifdef DEBUG
#define  AMLOG   1
#define LOG_LEVEL_VAR amlog_level_picdec
#define LOG_MASK_VAR amlog_mask_picdec
#endif

#define LOG_LEVEL_HIGH          0x00f
#define LOG_LEVEL_1             0x001
#define LOG_LEVEL_LOW           0x000

#define LOG_LEVEL_DESC \
	"[0x00]LOW[0X01]LEVEL1[0xf]HIGH"
#define LOG_MASK_INIT           0x001
#define LOG_MASK_IOCTL          0x002
#define LOG_MASK_HARDWARE       0x004
#define LOG_MASK_CONFIG         0x008
#define LOG_MASK_WORK           0x010
#define LOG_MASK_DESC \
"[0x01]:INIT,[0x02]:IOCTL,[0x04]:HARDWARE,[0x08]LOG_MASK_CONFIG[0x10]LOG_MASK_WORK"

static int task_running;

#define MAX_VF_POOL_SIZE 2

#define PIC_DEC_CANVAS_START 3
#define PIC_DEC_CANVAS_Y_FRONT (PIC_DEC_CANVAS_START + 1)
#define PIC_DEC_CANVAS_UV_FRONT (PIC_DEC_CANVAS_Y_FRONT + 1)

#define PIC_DEC_CANVAS_Y_BACK (PIC_DEC_CANVAS_UV_FRONT + 1)
#define PIC_DEC_CANVAS_UV_BACK (PIC_DEC_CANVAS_Y_BACK + 1)
#define PIC_DEC_SOURCE_CANVAS  (PIC_DEC_CANVAS_UV_BACK + 1)

/*same as tvin pool*/
static int PICDEC_POOL_SIZE = 2;
static int VF_POOL_SIZE = 2;
static int ZOOM_WIDTH = 1280;
static int ZOOM_HEIGHT = 720;
/*same as tvin pool*/

static struct picdec_device_s picdec_device;
static struct source_input_s picdec_input;
static int picdec_buffer_status;
#define INCPTR(p) ptr_atomic_wrap_inc(&p)

#define GE2D_ENDIAN_SHIFT               24
#define GE2D_ENDIAN_MASK            (0x1 << GE2D_ENDIAN_SHIFT)
#define GE2D_BIG_ENDIAN             (0 << GE2D_ENDIAN_SHIFT)
#define GE2D_LITTLE_ENDIAN          (1 << GE2D_ENDIAN_SHIFT)

#define PROVIDER_NAME "decoder.pic"
static DEFINE_SPINLOCK(lock);
static struct vframe_receiver_op_s *picdec_stop(void);
static inline void ptr_atomic_wrap_inc(u32 *ptr)
{
	u32 i = *ptr;

	i++;
	if (i >= PICDEC_POOL_SIZE)
		i = 0;
	*ptr = i;
}

static struct vframe_s vfpool[MAX_VF_POOL_SIZE];
static s32 vfbuf_use[MAX_VF_POOL_SIZE];
static s32 fill_ptr, get_ptr, putting_ptr, put_ptr;

struct semaphore pic_vb_start_sema;
struct semaphore pic_vb_done_sema;
static wait_queue_head_t frame_ready;

static int render_frame(struct ge2d_context_s *context,
		struct config_para_ex_s *ge2d_config);
static void post_frame(void);

/************************************************
 *
 *   buffer op for video sink.
 *
 ************************************************
 */
static int picdec_canvas_table[2];
static inline u32 index2canvas(u32 index)
{
	return picdec_canvas_table[index];
}

static struct vframe_s *picdec_vf_peek(void *);
static int picdec_vf_states(struct vframe_states *states, void *op_arg);
static struct vframe_s *picdec_vf_get(void *);
static void picdec_vf_put(struct vframe_s *vf, void *);
static int picdec_vf_states(struct vframe_states *states, void *op_arg)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
	states->vf_pool_size = VF_POOL_SIZE;
	i = fill_ptr - get_ptr;
	if (i < 0)
		i += VF_POOL_SIZE;
	states->buf_avail_num = i;
	spin_unlock_irqrestore(&lock, flags);
	return 0;
}

static struct vframe_s *picdec_vf_peek(void *op_arg)
{
	if (get_ptr == fill_ptr)
		return NULL;
	return &vfpool[get_ptr];
}

static struct vframe_s *picdec_vf_get(void *op_arg)
{
	struct vframe_s *vf;

	if (get_ptr == fill_ptr)
		return NULL;
	vf = &vfpool[get_ptr];
	INCPTR(get_ptr);
	return vf;
}

static void picdec_vf_put(struct vframe_s *vf, void *op_arg)
{
	int i;
	int canvas_addr;

	if (!vf)
		return;
	INCPTR(putting_ptr);
	for (i = 0; i < VF_POOL_SIZE; i++) {
		canvas_addr = index2canvas(i);
		if (vf->canvas0Addr == canvas_addr) {
			vfbuf_use[i] = 0;
			pr_info
			("******recycle buffer index : %d ******\n", i);
		}
	}
}

static int picdec_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_FORCE_UNREG) {
		picdec_stop();
		aml_pr_info(0, "picdec device force to quit\n");
	}
	return 0;
}

static const struct vframe_operations_s picdec_vf_provider = {
	.peek = picdec_vf_peek,
	.get = picdec_vf_get,
	.put = picdec_vf_put,
	.event_cb = picdec_event_cb,
	.vf_states = picdec_vf_states,
};

static struct vframe_provider_s picdec_vf_prov;

int get_unused_picdec_index(void)
{
	int i;

	for (i = 0; i < VF_POOL_SIZE; i++) {
		if (vfbuf_use[i] == 0)
			return i;
	}
	return -1;
}

static int render_frame(struct ge2d_context_s *context,
		struct config_para_ex_s *ge2d_config)
{
	struct vframe_s *new_vf;
	int index;
	struct timeval start;
	struct timeval end;

	unsigned long time_use = 0;
	int temp;
	unsigned int phase = 0;
	unsigned int h_phase, v_phase;
	struct picdec_device_s *dev =  &picdec_device;
	struct source_input_s *input = &picdec_input;

	do_gettimeofday(&start);
	index = get_unused_picdec_index();
	if (index < 0) {
		pr_info("no buffer available, need post ASAP\n");
		/* return -1; */
		index = fill_ptr;
	}
	dev->origin_width = input->frame_width;
	dev->origin_height = input->frame_height;
	if ((picdec_input.rotate == 90) || (picdec_input.rotate == 270)) {
		temp = input->frame_width;
		input->frame_width = input->frame_height;
		input->frame_height = temp;
	}
	h_phase = (input->frame_width << 18) / dev->disp_width;
	v_phase = (input->frame_height << 18) / dev->disp_height;
	phase = max(h_phase, v_phase);
	dev->p2p_mode = p2p_mode;
	switch (dev->p2p_mode) {
	case 0:
		if ((input->frame_width < dev->disp_width) &&
		(input->frame_height < dev->disp_height)) {
			dev->target_width = input->frame_width;
			dev->target_height = input->frame_height;
		} else {
			dev->target_width = dev->disp_width;
			dev->target_height = dev->disp_height;
		}
		break;
	case 1:
		if ((input->frame_width >= ZOOM_WIDTH) &&
			(input->frame_height >= ZOOM_HEIGHT)) {
			if ((input->frame_width < dev->disp_width) &&
			(input->frame_height < dev->disp_height)) {
				dev->target_width = input->frame_width;
				dev->target_height = input->frame_height;
			} else {
				dev->target_width = dev->disp_width;
				dev->target_height = dev->disp_height;
			}
		} else {
			dev->target_width = dev->disp_width;
			dev->target_height = dev->disp_height;
		}
		break;
	case 2:
		if ((phase <= (1 << 18)) && (phase >= (1 << 16))) {
			input->frame_width  =
			(input->frame_width << 18) / phase;
			input->frame_height =
			(input->frame_height << 18) / phase;
		} else if (phase < (1 << 16)) {
			input->frame_width  <<= 2;
			input->frame_height <<= 2;
		}
		dev->target_width = dev->disp_width;
		dev->target_height = dev->disp_height;
		break;
	default:
		dev->target_width  = dev->disp_width;
		dev->target_height = dev->disp_height;
		break;
	}
	aml_pr_info(1, "p2p_mode :%d ----render buffer index is %d\n",
		dev->p2p_mode, index);
	aml_pr_info(1, "render target width: %d ; target height: %d\n",
		dev->target_width, dev->target_height);
	new_vf = &vfpool[fill_ptr];
	new_vf->canvas0Addr = new_vf->canvas1Addr = index2canvas(index);
	new_vf->width =  dev->target_width;
	new_vf->height = dev->target_height;
	if (dev->output_format_mode)
		new_vf->type =
			VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE
			| VIDTYPE_VIU_FIELD | VIDTYPE_PIC;
	else
		new_vf->type =
			VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
			VIDTYPE_VIU_NV21 | VIDTYPE_PIC;
	new_vf->duration_pulldown = 0;
	new_vf->index = index;
	new_vf->pts = 0;
	new_vf->pts_us64 = 0;
	new_vf->ratio_control = DISP_RATIO_FORCECONFIG |
	DISP_RATIO_FORCE_NORMALWIDE;
	new_vf->ratio_control = 0;
	vfbuf_use[index]++;
	picdec_fill_buffer(new_vf, context, ge2d_config);
	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
			   (end.tv_usec - start.tv_usec) / 1000;
	pr_info("render frame time use: %ldms\n", time_use);
	return 0;
}

static int render_frame_block(void)
{
	struct vframe_s *new_vf;
	int index;
	struct timeval start;
	struct timeval end;
	int temp;
	unsigned int phase = 0;
	unsigned int h_phase, v_phase;
	unsigned long time_use = 0;
	struct config_para_ex_s ge2d_config;
	struct ge2d_context_s *context = picdec_device.context;
	struct picdec_device_s *dev = &picdec_device;
	struct source_input_s	*input = &picdec_input;

	do_gettimeofday(&start);
	memset(&ge2d_config, 0, sizeof(struct config_para_ex_s));
	index = get_unused_picdec_index();
	if (index < 0) {
		pr_info("no buffer available, need post ASAP\n");
		/* return -1; */
		index = fill_ptr;
	}
	dev->origin_width = input->frame_width;
	dev->origin_height = input->frame_height;
	if ((picdec_input.rotate == 90) || (picdec_input.rotate == 270)) {
		temp = input->frame_width;
		input->frame_width = input->frame_height;
		input->frame_height = temp;
	}
	h_phase = (input->frame_width << 18) / dev->disp_width;
	v_phase = (input->frame_height << 18) / dev->disp_height;
	phase = max(h_phase, v_phase);
	dev->p2p_mode = p2p_mode;
	switch (dev->p2p_mode) {
	case 0:
		if ((input->frame_width < dev->disp_width) &&
		(input->frame_height < dev->disp_height)) {
			dev->target_width  = input->frame_width;
			dev->target_height = input->frame_height;
		} else {
			dev->target_width  = dev->disp_width;
			dev->target_height = dev->disp_height;
		}
		break;
	case 1:
		if ((input->frame_width >=  ZOOM_WIDTH) &&
			(input->frame_height >= ZOOM_HEIGHT)) {
			if ((input->frame_width < dev->disp_width) &&
			(input->frame_height < dev->disp_height)) {
				dev->target_width  = input->frame_width;
				dev->target_height = input->frame_height;
			} else {
				dev->target_width  = dev->disp_width;
				dev->target_height = dev->disp_height;
			}
		} else {
			dev->target_width  = dev->disp_width;
			dev->target_height = dev->disp_height;
		}
		break;
	case 2:
		if ((phase <= (1 << 18)) && (phase >= (1 << 16))) {
			input->frame_width  =
			(input->frame_width << 18) / phase;
			input->frame_height =
			(input->frame_height << 18) / phase;
		} else if (phase < (1 << 16)) {
			input->frame_width  <<= 2;
			input->frame_height <<= 2;
		}
		dev->target_width = dev->disp_width;
		dev->target_height = dev->disp_height;
		break;
	default:
		dev->target_width  = dev->disp_width;
		dev->target_height = dev->disp_height;
		break;
	}
	aml_pr_info(1, "p2p_mode :%d ----render buffer index is %d\n",
		dev->p2p_mode, index);
	aml_pr_info(1, "render target width: %d ; target height: %d\n",
		dev->target_width, dev->target_height);
	new_vf = &vfpool[fill_ptr];
	new_vf->canvas0Addr = new_vf->canvas1Addr = index2canvas(index);
	new_vf->width = picdec_device.target_width;
	new_vf->height = picdec_device.target_height;
	if (dev->output_format_mode)
		new_vf->type =
			VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE
			| VIDTYPE_VIU_FIELD | VIDTYPE_PIC;
	else
		new_vf->type =
			VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
			VIDTYPE_VIU_NV21 | VIDTYPE_PIC;
	/* indicate the vframe is a full range frame */
	new_vf->signal_type =
		/* HD default 709 limit */
		  (1 << 29) /* video available */
		| (5 << 26) /* unspecified */
		| (1 << 25) /* full */
		| (1 << 24) /* color available */
		| (1 << 16) /* bt709 */
		| (1 << 8)  /* bt709 */
		| (1 << 0); /* bt709 */
	new_vf->duration_pulldown = 0;
	new_vf->index = index;
	new_vf->pts = 0;
	new_vf->pts_us64 = 0;
	new_vf->ratio_control = DISP_RATIO_FORCECONFIG |
	DISP_RATIO_FORCE_NORMALWIDE;
	picdec_device.cur_index = index;
	aml_pr_info(1, "picdec_fill_buffer start\n");
	picdec_fill_buffer(new_vf, context, &ge2d_config);
	aml_pr_info(1, "picdec_fill_buffer finish\n");
	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
			   (end.tv_usec - start.tv_usec) / 1000;
	aml_pr_info(1, "Total render frame time use: %ldms\n", time_use);
	return 0;
}

static void post_frame(void)
{
	vfbuf_use[picdec_device.cur_index]++;
	INCPTR(fill_ptr);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_VFRAME_READY,
					   NULL);
}

/*************************************************
 *
 *   buffer op for decoder, camera, etc.
 *
 *************************************************
 */
void picdec_local_init(void)
{
	int i;

	for (i = 0; i < MAX_VF_POOL_SIZE; i++)
		vfbuf_use[i] = 0;
	fill_ptr = get_ptr = putting_ptr = put_ptr = 0;
	memset((char *)&picdec_input, 0, sizeof(struct source_input_s));
	picdec_device.context = NULL;
	picdec_device.cur_index = 0;
}

static struct vframe_receiver_op_s *picdec_stop(void)
{
	/* ulong flags; */
	vf_unreg_provider(&picdec_vf_prov);
	pr_info("vf_unreg_provider : picdec\n");
#ifndef NO_TASK_MODE
	stop_picdec_task();
#else
	destroy_ge2d_work_queue(picdec_device.context);
#endif
	picdec_cma_buf_uninit();
	set_freerun_mode(0);
	picdec_local_init();
	if (picdec_device.use_reserved) {
		if (picdec_device.mapping) {
			io_mapping_free(picdec_device.mapping);
			picdec_device.mapping = 0;
		}
	}
	pr_info("stop picdec task\n");
	return (struct vframe_receiver_op_s *)NULL;
}

static int picdec_start(void)
{
	resource_size_t buf_start;
	unsigned int buf_size;
	unsigned int map_start, map_size = 0;

	if (picdec_buffer_init() < 0) {
		aml_pr_info(0, "no memory, open fail\n");
		return -1;
	}
	get_picdec_buf_info(&buf_start, &buf_size, NULL);
	map_start = picdec_device.assit_buf_start;
	map_size = buf_size - (picdec_device.assit_buf_start - buf_start);
	if (map_size > 0x1800000)	/* max size is 3840*2160*3 */
		map_size = 0x1800000;
	if (picdec_device.use_reserved) {
		picdec_device.mapping = io_mapping_create_wc(map_start,
		map_size);
		if (picdec_device.mapping <= 0) {
			aml_pr_info(1, "mapping failed!!!!!!!!!!!!\n");
			return -1;
		}
		aml_pr_info(1, "mapping addr is %x : %p , mapping size is %d\n ",
			map_start, picdec_device.mapping, map_size);
	} else{
		picdec_device.mapping = NULL;
		if (!cma_layout_flag)
			picdec_device.vir_addr = phys_to_virt(map_start);
	}
	vf_provider_init(&picdec_vf_prov, PROVIDER_NAME, &picdec_vf_provider,
					 NULL);
	vf_reg_provider(&picdec_vf_prov);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_START, NULL);
	set_freerun_mode(1);
	picdec_local_init();
#ifndef NO_TASK_MODE
	start_picdec_task();
#else
	picdec_device.context = create_ge2d_work_queue();
#endif
	return 0;
}

static int test_r = 0xff;
static int test_g = 0xff;
static int test_b = 0xff;

/*************************************************
 *   main task functions.
 *************************************************
 */
static unsigned int print_ifmt;
module_param(print_ifmt, uint, 0644);
MODULE_PARM_DESC(print_ifmt, "print input format\n");

/* fill the RGB user buffer to physical buffer */

static void dma_flush(u32 buf_start, u32 buf_size)
{
	dma_sync_single_for_device(
		&picdec_device.pdev->dev, buf_start,
		buf_size, DMA_TO_DEVICE);
}

int picdec_pre_process(void)
{
	struct io_mapping *mapping_wc;
	void __iomem *buffer_start;
	int i, j;
	unsigned int dst_addr_off = 0;
	char *p;
	char *q;
	char *ref;
	int ret = 0;
	int frame_width = picdec_device.origin_width;
	int frame_height = picdec_device.origin_height;
	int bp = ((frame_width + 0x1f) & ~0x1f) * 3;
	struct timeval start;
	struct timeval end;
	unsigned long time_use = 0;

	do_gettimeofday(&start);
	get_picdec_buf_info(NULL, NULL, &mapping_wc);
	if (picdec_device.use_reserved) {
		if (!mapping_wc)
			return -1;
		buffer_start = io_mapping_map_atomic_wc(mapping_wc, 0);
	} else{
		buffer_start = phys_to_virt(picdec_device.assit_buf_start);
	}
	if (buffer_start == NULL) {
		pr_info(" assit buffer mapping error\n");
		return -1;
	}
	aml_pr_info(1, "picdec_input width is %d , height is %d\n",
		   frame_width, frame_height);
	if (picdec_input.input == NULL) {
		for (j = 0; j < frame_height; j++) {
			for (i = 0; i < frame_width * 3; i += 3) {
				dst_addr_off = i + bp * j;
				*(char *)(buffer_start + dst_addr_off) = test_r;
				*(char *)(buffer_start + dst_addr_off + 1) =
					test_g;
				*(char *)(buffer_start + dst_addr_off + 2) =
					test_b;
			}
		}
		aml_pr_info(1, "test color copy finish ###\n");
	} else {
		switch (picdec_input.format) {
		case 0:	/* RGB */
			p = (char *)buffer_start;
			q = (char *)picdec_input.input;

			for (j = 0; j < frame_height; j++) {
				ret = copy_from_user(p, (void __user *)q,
						frame_width * 3);
				q += frame_width * 3;
				p += bp;
			}
			break;
		case 1:	/* RGBA */
			p = ref = (char *)buffer_start;
			q = (char *)picdec_input.input;

			for (j = 0; j < frame_height; j++) {
				p = ref;
				for (i = 0; i < frame_width; i++) {
					ret = copy_from_user(p,
							(void __user *)q, 3);
					q += 4;
					p += 3;
				}
				ref += bp;
			}
			aml_pr_info(1, "RGBA copy finish #########################\n");
			break;
		case 2:	/* ARGB */
			p = ref = (char *)buffer_start;
			q = (char *)picdec_input.input;

			for (j = 0; j < frame_height; j++) {
				p = ref;
				for (i = 0; i < frame_width; i++) {
					ret = copy_from_user(p,
							(void __user *)(q + 1),
							3);
					q += 4;
					p += 3;
				}
				ref += bp;
			}
			aml_pr_info(1, "ARGB copy finish#########################\n");
			break;
		default:
			p = (char *)buffer_start;
			q = (char *)picdec_input.input;

			for (j = 0; j < frame_height; j++) {
				ret = copy_from_user(p, (void __user *)q,
						frame_width * 3);
				q += frame_width * 3;
				p += bp;
			}
			break;
		}
	}
	if (picdec_device.use_reserved) {
		io_mapping_unmap_atomic(buffer_start);
	} else {
		dma_flush(picdec_device.assit_buf_start,
		bp * frame_height);
	}
	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
			   (end.tv_usec - start.tv_usec) / 1000;
	aml_pr_info(2, "picdec_pre_process time use: %ldms\n", time_use);
	return 0;
}

int fill_black_color_by_ge2d(struct vframe_s *vf,
					struct ge2d_context_s *context,
					struct config_para_ex_s *ge2d_config) {
		struct canvas_s cd;

		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;
		ge2d_config->dst_xy_swap = 0;
		canvas_read(vf->canvas0Addr & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;
		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = vf->width;
		ge2d_config->src_para.height = vf->height;
		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.canvas_index = vf->canvas0Addr;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = vf->width;
		ge2d_config->dst_para.height = vf->height;
		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		fillrect(
			context,
			0,
			0,
			vf->width,
			vf->height,
			0x008080ff);
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
		return 0;
}

static int picdec_memset_phyaddr(ulong phys, u32 size, u32 val)
{
	u32 i, span = SZ_1M;
	u32 count = size / PAGE_ALIGN(span);
	ulong addr = phys;
	u8 *p;

	for (i = 0; i < count; i++) {
		addr = phys + i * span;
		p = codec_mm_vmap(addr, span);
		if (!p)
			return -1;
		memset(p, val, span);
		codec_mm_unmap_phyaddr(p);
	}
	return 0;
}

int fill_color(struct vframe_s *vf, struct ge2d_context_s *context,
			   struct config_para_ex_s *ge2d_config)
{
	struct io_mapping *mapping_wc;
	struct canvas_s cs0, cs1, cs2;
	struct timeval start;
	struct timeval end;
	unsigned long time_use = 0;
	void __iomem *p;
	int ret = 0;

	do_gettimeofday(&start);
	get_picdec_buf_info(NULL, NULL, &mapping_wc);
	if (picdec_device.use_reserved) {
		if (!mapping_wc)
			return -1;
	}
	canvas_read(vf->canvas0Addr & 0xff, &cs0);
	canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
	canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
	if (picdec_device.use_reserved) {
		p = ioremap_nocache(cs0.addr, cs0.width * cs0.height);
		memset(p, 0, cs0.width * cs0.height);
		iounmap(p);
		p = ioremap_nocache(cs1.addr, cs1.width * cs1.height);
		memset(p, 0x80, cs1.width * cs1.height);
		iounmap(p);
	} else {
		if (picdec_device.output_format_mode) {
			ret = fill_black_color_by_ge2d(vf, context,
						ge2d_config);
			if (ret < 0)
				pr_err("fill black color by ge2d failed\n");
		} else {
			if (!cma_layout_flag) {
				p = phys_to_virt(cs0.addr);
				memset(p, 0, cs0.width * cs0.height);
				p = phys_to_virt(cs1.addr);
				memset(p, 0x80, cs1.width * cs1.height);
			} else {
				picdec_memset_phyaddr(cs0.addr,
					(cs0.width * cs0.height), 0);
				picdec_memset_phyaddr(cs1.addr,
					(cs1.width * cs1.height), 0x80);
			}
		}
	}
	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
			   (end.tv_usec - start.tv_usec) / 1000;
	if (debug_flag)
		pr_info("clear background time use: %ldms\n", time_use);
	return 0;
}

static void rotate_adjust(int w_in, int h_in, int *w_out, int *h_out, int angle)
{
	int w = 0, h = 0, disp_w = 0, disp_h = 0;

	disp_w = *w_out;
	disp_h = *h_out;
	if ((angle == 90) || (angle == 270)) {
		if ((w_in < disp_h) && (h_in < disp_w)) {
			w = h_in;
			h = w_in;
		} else {
			h = min_t(int, w_in, disp_h);
			w = h_in * h / w_in;
			if (w > disp_w) {
				h = (h * disp_w) / w;
				w = disp_w;
			}
		}
	} else {
		if ((w_in < disp_w) && (h_in < disp_h)) {
			w = w_in;
			h = h_in;
		} else {
			if ((w_in * disp_h) > (disp_w * h_in)) {
				w = disp_w;
				h = disp_w * h_in / w_in;
			} else {
				h = disp_h;
				w = disp_h * w_in / h_in;
			}
		}
	}
	*w_out = w;
	*h_out = h;
}

static int copy_phybuf_to_file(ulong phys, u32 size,
					   struct file *fp, loff_t pos)
{
	u32 i, span = SZ_1M;
	u32 count = size / PAGE_ALIGN(span);
	ulong addr = phys;
	u8 *p;

	for (i = 0; i < count; i++) {
		addr = phys + i * span;
		p = codec_mm_vmap(addr, span);
		if (!p)
			return -1;
		vfs_write(fp, (char *)p,
			span, &pos);
		pos += span;
		codec_mm_unmap_phyaddr(p);
	}
	return 0;
}

int picdec_fill_buffer(struct vframe_s *vf, struct ge2d_context_s *context,
					   struct config_para_ex_s *ge2d_config)
{
	struct canvas_s cs0, cs1, cs2;
	int canvas_id = PIC_DEC_SOURCE_CANVAS;
	int canvas_width = (picdec_device.origin_width + 0x1f) & ~0x1f;
	int canvas_height = (picdec_device.origin_height + 0xf) & ~0xf;
	int frame_width = picdec_input.frame_width;
	int frame_height = picdec_input.frame_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct file *filp = NULL;
	loff_t pos = 0;
	int ret = 0;
	void __iomem *p;
	mm_segment_t old_fs = get_fs();

	fill_color(vf, context, ge2d_config);
	canvas_config(PIC_DEC_SOURCE_CANVAS,
				  (ulong)(picdec_device.assit_buf_start),
				  canvas_width * 3, canvas_height,
				  CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	if (!cma_layout_flag) {
		aml_pr_info(1, "picdec_pre_process start\n");
		picdec_pre_process();
		aml_pr_info(1, "picdec_pre_process finish\n");
	} else {
		dma_flush(picdec_device.assit_buf_start,
		(canvas_width * picdec_device.origin_height * 3));
	}
	ge2d_config->alu_const_color = 0;	/* 0x000000ff; */
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;	/* 0xff; */
	ge2d_config->dst_xy_swap = 0;
	canvas_read((canvas_id & 0xff), &cs0);
	canvas_read(((canvas_id >> 8) & 0xff), &cs1);
	canvas_read(((canvas_id >> 16) & 0xff), &cs2);
	ge2d_config->src_planes[0].addr = cs0.addr;
	ge2d_config->src_planes[0].w = cs0.width;
	ge2d_config->src_planes[0].h = cs0.height;
	ge2d_config->src_planes[1].addr = cs1.addr;
	ge2d_config->src_planes[1].w = cs1.width;
	ge2d_config->src_planes[1].h = cs1.height;
	ge2d_config->src_planes[2].addr = cs2.addr;
	ge2d_config->src_planes[2].w = cs2.width;
	ge2d_config->src_planes[2].h = cs2.height;
	aml_pr_info(1, "src addr is %lx , plane 0 width is %d , height is %d\n",
	   cs0.addr, cs0.width, cs0.height);
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = PIC_DEC_SOURCE_CANVAS;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FORMAT_S24_BGR;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = picdec_device.origin_width;
	ge2d_config->src_para.height = picdec_device.origin_height;
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = vf->canvas0Addr;
	canvas_read(vf->canvas0Addr & 0xff, &cs0);
	canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
	canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
	ge2d_config->dst_planes[0].addr = cs0.addr;
	ge2d_config->dst_planes[0].w = cs0.width;
	ge2d_config->dst_planes[0].h = cs0.height;
	aml_pr_info(1, "dst 0 addr  is %x\n", (unsigned int)cs0.addr);
	ge2d_config->dst_planes[1].addr = cs1.addr;
	ge2d_config->dst_planes[1].w = cs1.width;
	ge2d_config->dst_planes[1].h = cs1.height;
	aml_pr_info(1, "dst 1 addr  is %x\n", (unsigned int)cs1.addr);
	ge2d_config->dst_planes[2].addr = cs2.addr;
	ge2d_config->dst_planes[2].w = cs2.width;
	ge2d_config->dst_planes[2].h = cs2.height;
	aml_pr_info(1, "dst 2 addr  is %x\n", (unsigned int)cs2.addr);
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	if (picdec_device.output_format_mode)
		ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
	else
		ge2d_config->dst_para.format = GE2D_FORMAT_M24_NV21;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = picdec_device.disp_width;
	ge2d_config->dst_para.height = picdec_device.disp_height;
	if (picdec_input.rotate == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (picdec_input.rotate == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (picdec_input.rotate == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}
	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_info("++ge2d configing error.\n");
		return -1;
	}
	dst_top = 0;
	dst_left = 0;
	dst_width =  picdec_device.target_width;
	dst_height = picdec_device.target_height;
	rotate_adjust(frame_width, frame_height, &dst_width, &dst_height,
				  0);
	dst_width  = (dst_width + 0x1) & ~0x1;
	dst_height = (dst_height + 0x1) & ~0x1;
	switch (picdec_device.p2p_mode) {
	case 0:
		dst_left = 0;
		dst_top = 0;
		vf->width  = dst_width;
		vf->height = dst_height;
		break;
	case 1:
		if ((picdec_input.frame_width >=  ZOOM_WIDTH) &&
		(picdec_input.frame_height >= ZOOM_HEIGHT)) {
			dst_left = 0;
			dst_top = 0;
			vf->width  = dst_width;
			vf->height = dst_height;
		} else {
			dst_left = (picdec_device.disp_width - dst_width) >> 1;
			dst_top = (picdec_device.disp_height - dst_height) >> 1;
		}
		break;
	case 2:
		dst_left = (picdec_device.disp_width - dst_width) >> 1;
		dst_top = (picdec_device.disp_height - dst_height) >> 1;
		break;
	default:
		dst_left = (picdec_device.disp_width - dst_width) >> 1;
		dst_top = (picdec_device.disp_height - dst_height) >> 1;
		break;
	}
	stretchblt_noalpha(context, 0, 0, picdec_device.origin_width,
					picdec_device.origin_height,
					dst_left, dst_top,
					dst_width, dst_height);
/*dump ge2d output buffer data. use this function should close selinux*/
	if ((dump_file_flag) && (picdec_device.origin_width > 100)) {
		if (picdec_device.output_format_mode) {
			set_fs(KERNEL_DS);
			filp = filp_open("/data/temp/yuv444.yuv",
				O_RDWR|O_CREAT, 0666);
			if (IS_ERR(filp)) {
				aml_pr_info(0, "open file failed\n");
			} else {
				if (!cma_layout_flag) {
					p = phys_to_virt(cs0.addr);
					vfs_write(filp, (char *)p,
						cs0.width * cs0.height, &pos);
				} else {
					ret = copy_phybuf_to_file(cs0.addr,
						cs0.width * cs0.height,
						filp, 0);
					if (ret < 0)
						pr_err("write yuv444 file failed.\n");
				}
				vfs_fsync(filp, 0);
				filp_close(filp, NULL);
				set_fs(old_fs);
			}
		} else {
			set_fs(KERNEL_DS);
			filp = filp_open("/data/temp/NV21.yuv",
				O_RDWR|O_CREAT, 0666);
			if (IS_ERR(filp)) {
				aml_pr_info(0, "open file failed\n");
			} else {
				if (!cma_layout_flag) {
					p = phys_to_virt(cs0.addr);
					vfs_write(filp, (char *)p,
						cs0.width * cs0.height, &pos);
				} else {
					ret = copy_phybuf_to_file(cs0.addr,
						cs0.width * cs0.height,
						filp, 0);
					if (ret < 0)
						pr_err("write NV21 Y to file failed\n");
				}
				pos = cs0.width * cs0.height;
				if (!cma_layout_flag) {
					p = phys_to_virt(cs1.addr);
					vfs_write(filp, (char *)p,
						cs1.width * cs1.height, &pos);
				} else {
					ret = copy_phybuf_to_file(cs1.addr,
						cs1.width * cs1.height,
						filp, pos);
					if (ret < 0)
						pr_err("write NV21 UV to file failed\n");
				}
				vfs_fsync(filp, 0);
				filp_close(filp, NULL);
				set_fs(old_fs);
			}
		}
	}

	return 0;
}

static struct task_struct *task;
static int picdec_task(void *data)
{
	int ret = 0;
	struct sched_param param = {.sched_priority = MAX_USER_RT_PRIO + 1 };
	struct ge2d_context_s *context = create_ge2d_work_queue();
	struct config_para_ex_s ge2d_config;
#ifdef CONFIG_AMLCAP_LOG_TIME_USEFORFRAMES
	struct timeval start;
	struct timeval end;
	unsigned long time_use = 0;
#endif

	memset(&ge2d_config, 0, sizeof(struct config_para_ex_s));
	amlog_level(LOG_LEVEL_HIGH, "picdec task is running\n ");
	sched_setscheduler(current, SCHED_FIFO, &param);
	allow_signal(SIGTERM);
	while (1) {
		ret = down_interruptible(&pic_vb_start_sema);
		if (kthread_should_stop()) {
			up(&pic_vb_done_sema);
			break;
		}
		if (!task_running) {
			up(&pic_vb_done_sema);
			goto picdec_exit;
		}
		render_frame(context, &ge2d_config);
		if (kthread_should_stop()) {
			up(&pic_vb_done_sema);
			break;
		}
		up(&pic_vb_done_sema);
	}
picdec_exit:
	destroy_ge2d_work_queue(context);
	picdec_cma_buf_uninit();
	while (!kthread_should_stop()) {
/* may not call stop, wait..
 *  it is killed by SIGTERM,eixt on down_interruptible
 *  if not call stop,this thread may on do_exit and
 *  kthread_stop may not work good;
 */
		msleep(20);
	}
	return ret;
}

/************************************************
 *
 *   init functions.
 *
 ************************************************
 */
int picdec_cma_buf_init(void)
{
	int flags;

	if (!picdec_device.use_reserved) {
		switch (picdec_buffer_status) {
		case 0:/* not config */
			break;
		case 1:/* config before , return ok */
			return 0;
		case 2:/* config fail, won't retry , return failure */
			return -1;
		default:
			return -1;
		}
		aml_pr_info(0, "reserved memory config fail , use CMA.\n");
		if (picdec_device.output_format_mode) {
			if (picdec_device.cma_mode == 0) {
				picdec_device.cma_pages =
					dma_alloc_from_contiguous(
					&(picdec_device.pdev->dev),
					(72*SZ_1M) >> PAGE_SHIFT, 0);
				picdec_device.buffer_start = page_to_phys(
				picdec_device.cma_pages);
				picdec_device.buffer_size = (72*SZ_1M);
			} else{
				flags = CODEC_MM_FLAGS_DMA |
					CODEC_MM_FLAGS_CMA_CLEAR;
				picdec_device.buffer_start =
					codec_mm_alloc_for_dma("picdec",
					(72*SZ_1M)/PAGE_SIZE, 0, flags);
				picdec_device.buffer_size = (72*SZ_1M);
			}
		} else {
			if (picdec_device.cma_mode == 0) {
				picdec_device.cma_pages =
					dma_alloc_from_contiguous(
					&(picdec_device.pdev->dev),
					(48*SZ_1M) >> PAGE_SHIFT, 0);
				picdec_device.buffer_start = page_to_phys(
				picdec_device.cma_pages);
				picdec_device.buffer_size = (48*SZ_1M);
			} else{
				flags = CODEC_MM_FLAGS_DMA |
					CODEC_MM_FLAGS_CMA_CLEAR;
				picdec_device.buffer_start =
					codec_mm_alloc_for_dma("picdec",
					(48*SZ_1M)/PAGE_SIZE, 0, flags);
				picdec_device.buffer_size = (48*SZ_1M);
			}
		}
		aml_pr_info(0, "cma memory is %x , size is  %x\n",
		(unsigned int)picdec_device.buffer_start,
		(unsigned int)picdec_device.buffer_size);
		if (picdec_device.buffer_start == 0) {
			picdec_buffer_status = 2;
			aml_pr_info(0, "cma memory allocate fail\n");
			return -1;
		}
		picdec_buffer_status = 1;
		aml_pr_info(0, "cma memory allocate succeed\n");
		return 0;
	}
	return 0;
}

int picdec_cma_buf_uninit(void)
{
	if (!picdec_device.use_reserved) {
		if (picdec_device.output_format_mode) {
			if (picdec_device.cma_mode == 0) {
				if (picdec_device.cma_pages) {
					dma_release_from_contiguous(
						&picdec_device.pdev->dev,
						picdec_device.cma_pages,
						(72*SZ_1M) >> PAGE_SHIFT);
					picdec_device.cma_pages = NULL;
				}
			} else {
				if (picdec_device.buffer_start != 0) {
					codec_mm_free_for_dma(
					"picdec",
					picdec_device.buffer_start);
					picdec_device.buffer_start = 0;
					picdec_device.buffer_size = 0;
					aml_pr_info(0,
					"cma memory release succeed\n");
				}
			}
		} else {
			if (picdec_device.cma_mode == 0) {
				if (picdec_device.cma_pages) {
					dma_release_from_contiguous(
						&picdec_device.pdev->dev,
						picdec_device.cma_pages,
						(48*SZ_1M) >> PAGE_SHIFT);
					picdec_device.cma_pages = NULL;
				}
			} else {
				if (picdec_device.buffer_start != 0) {
					codec_mm_free_for_dma(
					"picdec",
					picdec_device.buffer_start);
					picdec_device.buffer_start = 0;
					picdec_device.buffer_size = 0;
					aml_pr_info(0,
					"cma memory release succeed\n");
				}
			}
		}
	}
	picdec_buffer_status = 0;
	return 0;
}

int picdec_buffer_init(void)
{
	int i;
	int ret = 0;
	u32 canvas_width, canvas_height;
	u32 decbuf_size;
	resource_size_t buf_start;
	unsigned int buf_size;
	unsigned int offset = 0;

	picdec_buffer_status = 0;
	picdec_device.vinfo = get_current_vinfo();
	picdec_device.disp_width = picdec_device.vinfo->width;
	picdec_device.disp_height = picdec_device.vinfo->height;
	if ((get_cpu_type() == MESON_CPU_MAJOR_ID_TXLX) &&
	((picdec_device.disp_width * picdec_device.disp_height) >=
	1920 * 1080))
		picdec_device.output_format_mode = txlx_output_format_mode;
	else
		picdec_device.output_format_mode = output_format_mode;
	picdec_cma_buf_init();
	get_picdec_buf_info(&buf_start, &buf_size, NULL);
	aml_pr_info(1, "picdec buffer size is %x\n", buf_size);
	sema_init(&pic_vb_start_sema, 1);/*init 1*/
	sema_init(&pic_vb_done_sema, 1);/*init 1*/
	if (!buf_start || !buf_size) {
		ret = -1;
		goto exit;
	}
	canvas_width = (picdec_device.disp_width + 0x1f) & ~0x1f;
	canvas_height = (picdec_device.disp_height + 0xf) & ~0xf;
	decbuf_size = canvas_width * canvas_height;
	if (picdec_device.output_format_mode) {
		for (i = 0; i < MAX_VF_POOL_SIZE; i++) {
			canvas_config(PIC_DEC_CANVAS_START + i,
					(unsigned int)(buf_start + offset),
					canvas_width * 3,
					canvas_height,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);
			offset = canvas_width * canvas_height * 3;
			picdec_canvas_table[i] =
				PIC_DEC_CANVAS_START + i;
			vfbuf_use[i] = 0;
		}
	} else {
		for (i = 0; i < MAX_VF_POOL_SIZE; i++) {
			pr_info("%d addr is %x################\n",
				   i, (unsigned int)(buf_start + offset));
			canvas_config(PIC_DEC_CANVAS_START + 2 * i,
					(unsigned int)(buf_start + offset),
					canvas_width,
					canvas_height,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);
			offset += canvas_width * canvas_height;
			canvas_config(PIC_DEC_CANVAS_START + 2 * i + 1,
					(unsigned int)(buf_start + offset),
					canvas_width,
					canvas_height / 2,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);
			offset += canvas_width * canvas_height / 2;
			picdec_canvas_table[i] =
				(PIC_DEC_CANVAS_START + 2 * i) |
				((PIC_DEC_CANVAS_START + 2 * i + 1) << 8);
			vfbuf_use[i] = 0;
		}
	}

	if (picdec_device.output_format_mode)
		picdec_device.assit_buf_start =
			buf_start + canvas_width * canvas_height * 6;
	else
		picdec_device.assit_buf_start =
			buf_start + canvas_width * canvas_height * 3;
exit:
	return ret;
}

int start_picdec_task(void)
{
	/* init the device. */
	if (!task) {
		task = kthread_create(picdec_task, &picdec_device, "picdec");
		if (IS_ERR(task)) {
			amlog_level(LOG_LEVEL_HIGH, "thread creating error.\n");
			return -1;
		}
		init_waitqueue_head(&frame_ready);
		wake_up_process(task);
	}
	task_running = 1;
	picdec_device.task_running = task_running;
	return 0;
}
void stop_picdec_task(void)
{
	if (task) {
		task_running = 0;
		picdec_device.task_running = task_running;
		send_sig(SIGTERM, task, 1);
		up(&pic_vb_start_sema);
		wake_up_interruptible(&frame_ready);
		kthread_stop(task);
		task = NULL;
	}
}

/***********************************************************************
 *
 * global status.
 *
 ***********************************************************************
 */
static int picdec_enable_flag;
int get_picdec_status(void)
{
	return picdec_enable_flag;
}

void set_picdec_status(int flag)
{
	if (flag >= 0)
		picdec_enable_flag = flag;
	else
		picdec_enable_flag = 0;
}

/***********************************************************************
 *
 * file op section.
 *
 ***********************************************************************
 */

void set_picdec_buf_info(resource_size_t start, unsigned int size)
{
	picdec_device.buffer_start = start;
	picdec_device.buffer_size = size;
}

void unset_picdec_buf_info(void)
{
	if (picdec_device.use_reserved) {
		if (picdec_device.mapping)  {
			io_mapping_free(picdec_device.mapping);
			picdec_device.mapping = 0;
		}
	}
	picdec_device.buffer_start = 0;
	picdec_device.buffer_size = 0;
}

void get_picdec_buf_info(resource_size_t *start, unsigned int *size,
						 struct io_mapping **mapping)
{
	if (start)
		*start = picdec_device.buffer_start;
	if (size)
		*size = picdec_device.buffer_size;
	if (mapping)
		*mapping = picdec_device.mapping;
}

static int picdec_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	if (!cma_layout_flag) {
		struct ge2d_context_s *context = NULL;

		aml_pr_info(1, "open one picdec device\n");
		file->private_data = context;
		picdec_device.open_count++;
		ret = picdec_start();
	} else {
		struct picdec_private_s *priv;

		aml_pr_info(1, "open one picdec device\n");
		priv = kmalloc(sizeof(struct picdec_private_s), GFP_KERNEL);
		if (!priv) {
			pr_err("alloc memory failed for amvideo cap\n");
			return -ENOMEM;
		}
		memset(priv, 0, sizeof(struct picdec_private_s));
		picdec_device.open_count++;
		ret = picdec_start();
		priv->context = NULL;
		priv->phyaddr = (unsigned long)picdec_device.assit_buf_start;
		priv->buf_len = (picdec_device.buffer_size/3);
		file->private_data = priv;
	}
	return ret;
}

static int picdec_mmap(struct file *file,
	struct vm_area_struct *vma)
{
	struct picdec_private_s *priv = file->private_data;
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	unsigned int vm_size = vma->vm_end - vma->vm_start;

	if (!priv->phyaddr)
		return -EIO;

	aml_pr_info(1, "priv->phyaddr = %lx , vm_size = %d\n",
		priv->phyaddr, vm_size);

	if (vm_size == 0)
		return -EAGAIN;

	off += priv->phyaddr;

	if ((off + vm_size) > (priv->phyaddr + priv->buf_len))
		return -ENOMEM;

	/*vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);*/
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP | VM_IO;

	if (remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot)) {
		pr_err("set_cached: failed remap_pfn_range\n");
		return -EAGAIN;
	}
	aml_pr_info(1, "picdec_mmap ok\n");
	return 0;
}

static long picdec_ioctl(struct file *filp, unsigned int cmd,
						 unsigned long args)
{
	int ret = 0;
	void __user *argp;
	struct ge2d_context_s *context;
	struct picdec_private_s *priv;

	if (!cma_layout_flag) {
		context = (struct ge2d_context_s *) filp->private_data;
	} else {
		priv = filp->private_data;
		context = (struct ge2d_context_s *) priv->context;
	}
	argp = (void __user *)args;
	switch (cmd) {
#ifdef CONFIG_COMPAT
	case PICDEC_IOC_FRAME_RENDER32:
		aml_pr_info(1, "PICDEC_IOC_FRAME_RENDER32\n");
		{
			ulong paddr, r;
			struct compat_source_input_s __user *uf =
				(struct compat_source_input_s *)argp;
			memset(&picdec_input, 0, sizeof(struct source_input_s));
			r = get_user(paddr, &uf->input);
			picdec_input.input = (char *)paddr;
			r |= get_user(picdec_input.frame_width,
				&uf->frame_width);
			r |= get_user(picdec_input.frame_height,
				&uf->frame_height);
			r |= get_user(picdec_input.format, &uf->format);
			r |= get_user(picdec_input.rotate, &uf->rotate);
			if (r)
				return -EFAULT;
			render_frame_block();
		}
		break;
#endif
	case PICDEC_IOC_FRAME_RENDER:
		aml_pr_info(1, "PICDEC_IOC_FRAME_RENDER\n");
		if (copy_from_user
			(&picdec_input, (void *)argp,
			 sizeof(struct source_input_s)))
			return -EFAULT;
		aml_pr_info(1, "w:%d,h:%d,format:%d,rotate:%d\n",
		picdec_input.frame_width,
		picdec_input.frame_height,
		picdec_input.format,
		picdec_input.rotate);
		render_frame_block();
		break;
	case PICDEC_IOC_FRAME_POST:
		pr_info("PICDEC_IOC_FRAME_POST\n");
		post_frame();
		break;
	case PICDEC_IOC_CONFIG_FRAME:
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long picdec_compat_ioctl(struct file *filp,
			      unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = picdec_ioctl(filp, cmd, args);
	return ret;
}
#endif

static int picdec_release(struct inode *inode, struct file *file)
{
	struct ge2d_context_s *context;
	struct picdec_private_s *priv = file->private_data;

	if (!cma_layout_flag)
		context = (struct ge2d_context_s *) file->private_data;
	else
		context = (struct ge2d_context_s *) priv->context;

	aml_pr_info(1, "picdec stop start");
	picdec_stop();
	if (context && (destroy_ge2d_work_queue(context) == 0)) {
		picdec_device.open_count--;
		return 0;
	}
	if (cma_layout_flag) {
		kfree(priv);
		priv = NULL;
	}
	aml_pr_info(0, "release one picdec device\n");
	return -1;
}

#ifdef CONFIG_COMPAT
static long picdec_compat_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long args);
#endif

/***********************************************************************
 *
 * file op init section.
 *
 ***********************************************************************
 */
static const struct file_operations picdec_fops = {
	.owner = THIS_MODULE,
	.open = picdec_open,
	.mmap = picdec_mmap,
	.unlocked_ioctl = picdec_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = picdec_compat_ioctl,
#endif
	.release = picdec_release,
};

/*sys attribute*/
static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;
	params = kstrdup(para, GFP_KERNEL);
	token = params;
	len = strlen(token);
	do {
		token = strsep(&params, " ");
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
	} while ((token) && (count < para_num) && (len > 0));
	kfree(params);
	return count;
}

static ssize_t frame_render_read(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80,
			"render width:%d\nheight:%d\nformat:%d\ndirection:%d\n",
			picdec_input.frame_width, picdec_input.frame_height,
			picdec_input.format, picdec_input.rotate);
}

static int first_running = 1;
static ssize_t frame_render_write(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	ssize_t size;
	char *endp;
	int parsed[4];

	if (likely(parse_para(buf, 4, parsed) == 4)) {
		picdec_input.frame_width = parsed[0];
		picdec_input.frame_height = parsed[1];
		picdec_input.format = parsed[2];
		picdec_input.rotate = parsed[3];
	} else
		return count;
	picdec_input.input = NULL;
	if (first_running) {
		first_running = 0;
		picdec_start();
	}
#ifdef NO_TASK_MODE
	render_frame_block();
#else
	up(&pic_vb_start_sema);
#endif
	size = endp - buf;
	return count;
}

static ssize_t frame_post_read(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "post command is %d\n",
					picdec_device.frame_post);
}

static ssize_t frame_post_write(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long r;
	int ret = 0;

	ret = kstrtoul(buf, 0, (unsigned long *)&r);
	if (ret < 0)
		return -EINVAL;
	post_frame();
	return count;
}

static ssize_t dec_status_read(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "PIC decoder status is %d\n",
					(picdec_device.open_count > 0)?1:0);
}

static ssize_t test_color_read(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "post command is %d\n",
					picdec_device.frame_post);
}

static ssize_t test_color_write(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	ssize_t size;
	char *endp;
	int parsed[3];

	if (likely(parse_para(buf, 3, parsed) == 3)) {
		test_r = parsed[0];
		test_g = parsed[1];
		test_b = parsed[2];
	} else
		return count;
	size = endp - buf;
	return count;
}

static struct class_attribute picdec_class_attrs[] = {

	__ATTR(frame_render,
	       0644,
	       frame_render_read,
	       frame_render_write),
	__ATTR(frame_post,
	       0644,
	       frame_post_read,
	       frame_post_write),
	__ATTR(test_color,
	       0644,
	       test_color_read,
	       test_color_write),
	__ATTR(dec_status,
	       0644,
	       dec_status_read,
	       NULL),
	__ATTR_NULL
};

#define PICDEC_CLASS_NAME               "picdec"
static struct class picdec_class = {
		.name = PICDEC_CLASS_NAME,
		.class_attrs = picdec_class_attrs,
};

struct class *init_picdec_cls(void)
{
	int ret = 0;

	ret = class_register(&picdec_class);
	if (ret < 0) {
		amlog_level(LOG_LEVEL_HIGH,
				"error create picdec class\n");
		return NULL;
	}
	return &picdec_class;
}

int init_picdec_device(void)
{
	int ret = 0;

	strcpy(picdec_device.name, "picdec");
	ret = register_chrdev(0, picdec_device.name, &picdec_fops);
	if (ret <= 0) {
		amlog_level(LOG_LEVEL_HIGH, "register picdec device error\n");
		return ret;
	}
	picdec_device.major = ret;
	picdec_device.dbg_enable = 0;
	amlog_level(LOG_LEVEL_LOW, "picdec_dev major:%d\n", ret);
	picdec_device.cla = init_picdec_cls();
	if (picdec_device.cla == NULL)
		return -1;
	picdec_device.dev =
		device_create(picdec_device.cla, NULL,
				MKDEV(picdec_device.major, 0),
				NULL, picdec_device.name);
	if (IS_ERR(picdec_device.dev)) {
		amlog_level(LOG_LEVEL_HIGH, "create picdec device error\n");
		goto unregister_dev;
	}
	/* device_create_file( picdec_device.dev, &dev_attr_dump); */
	picdec_device.dump = 0;
	dev_set_drvdata(picdec_device.dev, &picdec_device);
	platform_set_drvdata(picdec_device.pdev, &picdec_device);
	vf_provider_init(&picdec_vf_prov, PROVIDER_NAME, &picdec_vf_provider,
					 NULL);
	return 0;
unregister_dev:
	class_unregister(picdec_device.cla);
	return -1;
}

int uninit_picdec_device(void)
{
	if (picdec_device.cla) {
		if (picdec_device.dev)
			device_destroy(picdec_device.cla,
					   MKDEV(picdec_device.major, 0));
		picdec_device.dev = NULL;
		class_unregister(picdec_device.cla);
	}
	unregister_chrdev(picdec_device.major, picdec_device.name);
	return 0;
}

/*******************************************************************
 *
 * interface for Linux driver
 *
 * ******************************************************************/

MODULE_AMLOG(AMLOG_DEFAULT_LEVEL, 0xff, LOG_LEVEL_DESC, LOG_MASK_DESC);

/* for driver. */
static int picdec_driver_probe(struct platform_device *pdev)
{
	int r;

	picdec_device.use_reserved = 0;
	aml_pr_info(0, "picdec_driver_probe called.\n");
	r = of_reserved_mem_device_init(&pdev->dev);
	if (r == 0)
		aml_pr_info(0, "picdec_driver_probe done.\n");
	picdec_device.cma_mode = 1;
	picdec_device.pdev = pdev;
	init_picdec_device();
	picdec_device.p2p_mode = 0;
	picdec_device.output_format_mode = 0;
	txlx_output_format_mode = 1;
	return r;
}

static int picdec_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	unsigned long start, end;

	start = rmem->base;
	end = rmem->base + rmem->size - 1;
	aml_pr_info(0, "init picdec memsource %lx->%lx\n", start, end);
	picdec_device.use_reserved = 1;
	set_picdec_buf_info((resource_size_t) rmem->base, rmem->size);
	return 0;
}

static const struct reserved_mem_ops rmem_picdec_ops = {
	.device_init = picdec_mem_device_init,
};

static int __init picdec_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_picdec_ops;
	aml_pr_info(0, "picdec share mem setup\n");
	return 0;
}

static int picdec_drv_remove(struct platform_device *plat_dev)
{
	uninit_picdec_device();
	if (picdec_device.use_reserved) {
		if (picdec_device.mapping)
			io_mapping_free(picdec_device.mapping);
	}
	return 0;
}

static const struct of_device_id amlogic_picdec_dt_match[] = {
	{
		.compatible = "amlogic, picdec",
	},
	{},
};

/* general interface for a linux driver .*/
static struct platform_driver picdec_drv = {
	.probe = picdec_driver_probe,
	.remove = picdec_drv_remove,
	.driver = {
		.name = "picdec",
		.owner = THIS_MODULE,
		.of_match_table = amlogic_picdec_dt_match,
	}
};

static struct mconfig jpeg_configs[] = {
	MC_PU32("debug_flag", &debug_flag),
	MC_PU32("dump_file_flag", &dump_file_flag),
	MC_PU32("p2p_mode", &p2p_mode),
	MC_PU32("output_format_mode", &output_format_mode),
};
static struct mconfig_node jpeg_node;


static int __init picdec_init_module(void)
{
	int err;

	amlog_level(LOG_LEVEL_HIGH, "picdec_init\n");
	err = platform_driver_register(&picdec_drv);
	if (err) {
		pr_info("Failed to register picdec driver error=%d\n",
			   err);
		return err;
	}
	INIT_REG_NODE_CONFIGS("media.decoder", &jpeg_node,
		"jpeg", jpeg_configs, CONFIG_FOR_RW);
	return err;
}

static void __exit picdec_remove_module(void)
{
	platform_driver_unregister(&picdec_drv);
	amlog_level(LOG_LEVEL_HIGH, "picdec module removed.\n");
}

module_init(picdec_init_module);
module_exit(picdec_remove_module);

RESERVEDMEM_OF_DECLARE(picdec, "amlogic, picdec_memory", picdec_mem_setup);
MODULE_PARM_DESC(debug_flag, "\n debug_flag\n");
module_param(debug_flag, uint, 0664);

module_param(dump_file_flag, uint, 0664);
MODULE_PARM_DESC(dump_file_flag, "\n picdec dump file flag\n");

module_param(p2p_mode, uint, 0664);
MODULE_PARM_DESC(p2p_mode, "\n picdec zoom mode\n");

module_param(output_format_mode, uint, 0664);
MODULE_PARM_DESC(output_format_mode, "\n picdec output fomat mode\n");

module_param(txlx_output_format_mode, uint, 0664);
MODULE_PARM_DESC(txlx_output_format_mode, "\n txlx picdec output fomat mode\n");

module_param(cma_layout_flag, uint, 0664);
MODULE_PARM_DESC(cma_layout_flag, "\n cma layout change flag\n");

MODULE_DESCRIPTION("Amlogic picture decoder driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Zheng <simon.zheng@amlogic.com>");
