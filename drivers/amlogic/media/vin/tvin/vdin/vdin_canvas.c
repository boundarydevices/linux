/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_canvas.c
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

/* Standard Linux headers */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>

/* Amlogic headers */
#include <linux/amlogic/media/vfm/vframe.h>

/* Local headers */
#include "../tvin_format_table.h"
#include "vdin_drv.h"
#include "vdin_canvas.h"
#include "vdin_ctl.h"
/*the value depending on dts config mem limit
 *for skip two vframe case,need +2
 */
static unsigned int max_buf_num = VDIN_CANVAS_MAX_CNT;
static unsigned int min_buf_num = 4;
static unsigned int max_buf_width = VDIN_CANVAS_MAX_WIDTH_HD;
static unsigned int max_buf_height = VDIN_CANVAS_MAX_HEIGH;
/* one frame max metadata size:32x280 bits = 1120bytes(0x460) */
unsigned int dolby_size_byte = PAGE_SIZE;

#ifdef DEBUG_SUPPORT
module_param(max_buf_num, uint, 0664);
MODULE_PARM_DESC(max_buf_num, "vdin max buf num.\n");

module_param(min_buf_num, uint, 0664);
MODULE_PARM_DESC(min_buf_num, "vdin min buf num.\n");

module_param(max_buf_width, uint, 0664);
MODULE_PARM_DESC(max_buf_width, "vdin max buf width.\n");

module_param(max_buf_height, uint, 0664);
MODULE_PARM_DESC(max_buf_height, "vdin max buf height.\n");

module_param(dolby_size_byte, uint, 0664);
MODULE_PARM_DESC(dolby_size_byte, "dolby_size_byte.\n");
#endif

const unsigned int vdin_canvas_ids[2][VDIN_CANVAS_MAX_CNT] = {
	{
		38, 39, 40, 41, 42,
		43, 44, 45, 46,
	},
	{
		47, 48, 49, 50, 51,
		52, 53, 54, 55,
	},
};

/*function:
 *	1.set canvas_max_w & canvas_max_h
 *	2.set canvas_max_size & canvas_max_num
 *	3.set canvas_id & canvas_addr
 */
void vdin_canvas_init(struct vdin_dev_s *devp)
{
	int i, canvas_id;
	unsigned int canvas_addr;
	int canvas_max_w = 0;
	int canvas_max_h = VDIN_CANVAS_MAX_HEIGH;

	canvas_max_w = VDIN_CANVAS_MAX_WIDTH_HD << 1;

	devp->canvas_max_size = PAGE_ALIGN(canvas_max_w*canvas_max_h);
	devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size;

	if (devp->canvas_max_num > VDIN_CANVAS_MAX_CNT)
		devp->canvas_max_num = VDIN_CANVAS_MAX_CNT;

	devp->mem_start = roundup(devp->mem_start, devp->canvas_align);
	pr_info("vdin.%d canvas initial table:\n", devp->index);
	for (i = 0; i < devp->canvas_max_num; i++) {
		canvas_id = vdin_canvas_ids[devp->index][i];
		canvas_addr = devp->mem_start + devp->canvas_max_size * i;

		canvas_config(canvas_id, canvas_addr,
			canvas_max_w, canvas_max_h,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		pr_info("\t%d: 0x%x-0x%x  %dx%d (%d KB)\n",
			canvas_id, canvas_addr,
			(canvas_addr + devp->canvas_max_size),
			canvas_max_w, canvas_max_h,
			(devp->canvas_max_size >> 10));
	}
}

/*function:canvas_config when canvas_config_mode=1
 *	1.set canvas_w and canvas_h
 *	2.set canvas_max_size and canvas_max_num
 *	3.when dest_cfmt is TVIN_NV12/TVIN_NV21,
 *		buf width add canvas_w*canvas_h
 *based on parameters:
 *	format_convert/	source_bitdepth/
 *	v_active color_depth_mode/	prop.dest_cfmt
 */
void vdin_canvas_start_config(struct vdin_dev_s *devp)
{
	int i = 0;
	int canvas_id;
	unsigned long canvas_addr;
	unsigned int chroma_size = 0;
	unsigned int canvas_step = 1;
	unsigned int canvas_num = VDIN_CANVAS_MAX_CNT;
	unsigned int max_buffer_num = max_buf_num;

	/* todo: if new add output YUV444 format,this place should add too!!*/
	if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_RGB) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_RGB) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_GBR) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_BRG)) {
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH)
			devp->canvas_w = max_buf_width *
				VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = max_buf_height *
				VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
	} else if ((devp->prop.dest_cfmt == TVIN_NV12) ||
		(devp->prop.dest_cfmt == TVIN_NV21)) {
		devp->canvas_w = max_buf_width;
		canvas_num = canvas_num/2;
		canvas_step = 2;
	} else{/*YUV422*/
		/* txl new add yuv422 pack mode:canvas_w=h*2*10/8*/
		if ((devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) &&
		((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422)) &&
		(devp->color_depth_mode == 1))
			devp->canvas_w = (max_buf_width * 5)/2;
		else if ((devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) &&
			(devp->color_depth_mode == 0))
			devp->canvas_w = max_buf_width *
				VDIN_YUV422_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = max_buf_width *
				VDIN_YUV422_8BIT_PER_PIXEL_BYTE;
	}
	/*backup before roundup*/
	devp->canvas_active_w = devp->canvas_w;
	if (devp->force_yuv444_malloc == 1) {
		if ((devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) &&
			(devp->color_depth_mode != 1))
			devp->canvas_w = devp->h_active *
				VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = devp->h_active *
				VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
	}
	/*canvas_w must ensure divided exact by 256bit(32byte)*/
	devp->canvas_w = roundup(devp->canvas_w, devp->canvas_align);
	devp->canvas_h = devp->v_active;

	if ((devp->prop.dest_cfmt == TVIN_NV12) ||
		(devp->prop.dest_cfmt == TVIN_NV21))
		chroma_size = devp->canvas_w*devp->canvas_h/2;

	devp->canvas_max_size = PAGE_ALIGN(devp->canvas_w*
			devp->canvas_h+chroma_size);
	devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size;

	devp->canvas_max_num = min(devp->canvas_max_num, canvas_num);
	devp->canvas_max_num = min(devp->canvas_max_num, max_buffer_num);

	if ((devp->cma_config_en != 1) || !(devp->cma_config_flag & 0x100)) {
		/*use_reserved_mem or alloc_from_contiguous*/
		devp->mem_start = roundup(devp->mem_start, devp->canvas_align);
#ifdef VDIN_DEBUG
		pr_info("vdin%d cnavas start configuration table:\n",
			devp->index);
#endif
		for (i = 0; i < devp->canvas_max_num; i++) {
			canvas_id = vdin_canvas_ids[devp->index][i*canvas_step];
			canvas_addr = devp->mem_start +
				devp->canvas_max_size * i;
			canvas_config(canvas_id, canvas_addr,
				devp->canvas_w, devp->canvas_h,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
			if (chroma_size)
				canvas_config(canvas_id+1,
					canvas_addr +
					devp->canvas_w*devp->canvas_h,
					devp->canvas_w,
					devp->canvas_h/2,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);
#ifdef VDIN_DEBUG
			pr_info("\t%3d: 0x%lx-0x%lx %ux%u\n",
				canvas_id, canvas_addr,
				canvas_addr + devp->canvas_max_size,
				devp->canvas_w, devp->canvas_h);
#endif
		}
	} else if (devp->cma_config_flag & 0x100) {
#ifdef VDIN_DEBUG
		pr_info("vdin%d cnavas start configuration table:\n",
			devp->index);
#endif
		for (i = 0; i < devp->canvas_max_num; i++) {
			devp->vfmem_start[i] =
				roundup(devp->vfmem_start[i],
					devp->canvas_align);
			canvas_id = vdin_canvas_ids[devp->index][i*canvas_step];
			canvas_addr = devp->vfmem_start[i];
			canvas_config(canvas_id, canvas_addr,
				devp->canvas_w, devp->canvas_h,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
			if (chroma_size)
				canvas_config(canvas_id+1,
					canvas_addr +
					devp->canvas_w*devp->canvas_h,
					devp->canvas_w,
					devp->canvas_h/2,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);
#ifdef VDIN_DEBUG
			pr_info("\t%3d: 0x%lx-0x%lx %ux%u\n",
				canvas_id, canvas_addr,
				canvas_addr + devp->canvas_max_size,
				devp->canvas_w, devp->canvas_h);
#endif
		}
	}
}

/*
*this function used for configure canvas when canvas_config_mode=2
*base on the input format
*also used for input resalution over 1080p such as camera input 200M,500M
*YUV422-8BIT:1pixel = 2byte;
*YUV422-10BIT:1pixel = 3byte;
*YUV422-10BIT-FULLPACK:1pixel = 2.5byte;
*YUV444-8BIT:1pixel = 3byte;
*YUV444-10BIT:1pixel = 4byte
*/
void vdin_canvas_auto_config(struct vdin_dev_s *devp)
{
	int i = 0;
	int canvas_id;
	unsigned long canvas_addr;
	unsigned int chroma_size = 0;
	unsigned int canvas_step = 1;
	unsigned int canvas_num = VDIN_CANVAS_MAX_CNT;
	unsigned int max_buffer_num = max_buf_num;

	/* todo: if new add output YUV444 format,this place should add too!!*/
	if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_RGB) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_RGB) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_GBR) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_BRG)) {
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH)
			devp->canvas_w = devp->h_active *
				VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = devp->h_active *
				VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
	} else if ((devp->prop.dest_cfmt == TVIN_NV12) ||
		(devp->prop.dest_cfmt == TVIN_NV21)) {
		canvas_num = canvas_num/2;
		canvas_step = 2;
		devp->canvas_w = devp->h_active;
		/* nv21/nv12 only have 8bit mode */
	} else {/*YUV422*/
		/* txl new add yuv422 pack mode:canvas-w=h*2*10/8*/
		if ((devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) &&
		((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422)) &&
		(devp->color_depth_mode == 1))
			devp->canvas_w = (devp->h_active * 5)/2;
		else if ((devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) &&
			(devp->color_depth_mode == 0))
			devp->canvas_w = devp->h_active *
				VDIN_YUV422_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = devp->h_active *
				VDIN_YUV422_8BIT_PER_PIXEL_BYTE;
	}
	/*backup before roundup*/
	devp->canvas_active_w = devp->canvas_w;
	if (devp->force_yuv444_malloc == 1) {
		if ((devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) &&
			(devp->color_depth_mode != 1))
			devp->canvas_w = devp->h_active *
				VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = devp->h_active *
				VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
	}
	/*canvas_w must ensure divided exact by 256bit(32byte)*/
	devp->canvas_w = roundup(devp->canvas_w, devp->canvas_align);
	devp->canvas_h = devp->v_active;

	if ((devp->prop.dest_cfmt == TVIN_NV12) ||
		(devp->prop.dest_cfmt == TVIN_NV21))
		chroma_size = devp->canvas_w*devp->canvas_h/2;

	devp->canvas_max_size = PAGE_ALIGN(devp->canvas_w*
			devp->canvas_h+chroma_size);
	devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size;

	devp->canvas_max_num = min(devp->canvas_max_num, canvas_num);
	devp->canvas_max_num = min(devp->canvas_max_num, max_buffer_num);
	if (devp->canvas_max_num < devp->vfmem_max_cnt) {
		pr_err("\nvdin%d canvas_max_num %d less than vfmem_max_cnt %d\n",
			devp->index, devp->canvas_max_num, devp->vfmem_max_cnt);
	}
	devp->vfmem_max_cnt = min(devp->vfmem_max_cnt, devp->canvas_max_num);

	if (devp->set_canvas_manual == 1) {
		for (i = 0; i < 4; i++) {
			canvas_id =
				vdin_canvas_ids[devp->index][i * canvas_step];
			canvas_addr = vdin_set_canvas_addr[i].paddr;
			canvas_config(canvas_id, canvas_addr,
				devp->canvas_w, devp->canvas_h,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
			pr_info("canvas index=%d- %3d: 0x%lx-0x%lx %ux%u\n",
				i, canvas_id, canvas_addr,
				canvas_addr + devp->canvas_max_size,
				devp->canvas_w, devp->canvas_h);
		}
		return;
	}

	if ((devp->cma_config_en != 1) || !(devp->cma_config_flag & 0x100)) {
		/*use_reserved_mem or alloc_from_contiguous*/
		devp->mem_start = roundup(devp->mem_start, devp->canvas_align);
#ifdef VDIN_DEBUG
		pr_info("vdin%d cnavas auto configuration table:\n",
			devp->index);
#endif
		for (i = 0; i < devp->canvas_max_num; i++) {
			canvas_id = vdin_canvas_ids[devp->index][i*canvas_step];
			canvas_addr = devp->mem_start +
				devp->canvas_max_size * i;
			canvas_config(canvas_id, canvas_addr,
				devp->canvas_w, devp->canvas_h,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
			if (chroma_size)
				canvas_config(canvas_id+1,
					canvas_addr +
					devp->canvas_w*devp->canvas_h,
					devp->canvas_w,
					devp->canvas_h/2,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);
#ifdef VDIN_DEBUG
			pr_info("\t%3d: 0x%lx-0x%lx %ux%u\n",
				canvas_id, canvas_addr,
				canvas_addr + devp->canvas_max_size,
				devp->canvas_w, devp->canvas_h);
#endif
		}
	} else if (devp->cma_config_flag & 0x100) {
#ifdef VDIN_DEBUG
		pr_info("vdin%d cnavas auto configuration table:\n",
			devp->index);
#endif
		for (i = 0; i < devp->canvas_max_num; i++) {
			devp->vfmem_start[i] =
				roundup(devp->vfmem_start[i],
					devp->canvas_align);
			canvas_id = vdin_canvas_ids[devp->index][i*canvas_step];
			canvas_addr = devp->vfmem_start[i];
			canvas_config(canvas_id, canvas_addr,
				devp->canvas_w, devp->canvas_h,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
			if (chroma_size)
				canvas_config(canvas_id+1,
					canvas_addr +
					devp->canvas_w*devp->canvas_h,
					devp->canvas_w,
					devp->canvas_h/2,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);
#ifdef VDIN_DEBUG
			pr_info("\t%3d: 0x%lx-0x%lx %ux%u\n",
				canvas_id, canvas_addr,
				canvas_addr + devp->canvas_max_size,
				devp->canvas_w, devp->canvas_h);
#endif
		}
	}
}

#ifdef CONFIG_CMA
/* need to be static for pointer use in codec_mm */
static char vdin_name[6];
/* return val:1: fail;0: ok */
/* combined canvas and afbce memory */
unsigned int vdin_cma_alloc(struct vdin_dev_s *devp)
{
	unsigned int mem_size, h_size, v_size;
	int flags = CODEC_MM_FLAGS_CMA_FIRST|CODEC_MM_FLAGS_CMA_CLEAR|
		CODEC_MM_FLAGS_DMA;
	unsigned int max_buffer_num = min_buf_num;
	unsigned int i;
	/*head_size:3840*2160*3*9/32*/
	unsigned int afbce_head_size_byte = PAGE_SIZE * 1712;
	/*afbce map_table need 218700 byte at most*/
	unsigned int afbce_table_size_byte = PAGE_SIZE * 60;/*0.3M*/
	unsigned long ref_paddr;
	unsigned int mem_used;
	unsigned int frame_head_size;
	unsigned int mmu_used;

	if (devp->rdma_enable)
		max_buffer_num++;
	/*todo: need update if vf_skip_cnt used by other port*/
	if (devp->vfp->skip_vf_num &&
		(((devp->parm.port >= TVIN_PORT_HDMI0) &&
			(devp->parm.port <= TVIN_PORT_HDMI7)) ||
			((devp->parm.port >= TVIN_PORT_CVBS0) &&
			(devp->parm.port <= TVIN_PORT_CVBS3))))
		max_buffer_num += devp->vfp->skip_vf_num;
	if (max_buffer_num > max_buf_num)
		max_buffer_num = max_buf_num;
	devp->vfmem_max_cnt = max_buffer_num;

	if ((devp->cma_config_en == 0) ||
		(devp->cma_mem_alloc == 1)) {
		pr_info("\nvdin%d %s use_reserved mem or cma already alloced (%d,%d)!!!\n",
			devp->index, __func__, devp->cma_config_en,
			devp->cma_mem_alloc);
		return 0;
	}
	h_size = devp->h_active;
	v_size = devp->v_active;
	if (devp->canvas_config_mode == 1) {
		h_size = max_buf_width;
		v_size = max_buf_height;
	}
	if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_RGB) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_RGB) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_GBR) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_BRG) ||
		(devp->force_yuv444_malloc == 1)) {
		if ((devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) &&
			(devp->color_depth_mode != 1)) {
			h_size = roundup(h_size *
				VDIN_YUV444_10BIT_PER_PIXEL_BYTE,
				devp->canvas_align);
			devp->canvas_alin_w = h_size /
				VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		} else {
			h_size = roundup(h_size *
				VDIN_YUV444_8BIT_PER_PIXEL_BYTE,
				devp->canvas_align);
			devp->canvas_alin_w = h_size /
				VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
		}
	} else if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_NV12) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_NV21) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_NV12) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_NV21)) {
		h_size = roundup(h_size, devp->canvas_align);
		devp->canvas_alin_w = h_size;
		/*todo change with canvas alloc!!*/
		/* nv21/nv12 only have 8bit mode */
	} else {
		/* txl new add mode yuv422 pack mode:canvas-w=h*2*10/8
		 *canvas_w must ensure divided exact by 256bit(32byte
		 */
		if ((devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) &&
		((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422)) &&
		(devp->color_depth_mode == 1)) {
			h_size = roundup((h_size * 5)/2, devp->canvas_align);
			devp->canvas_alin_w = (h_size * 2) / 5;
		} else if ((devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) &&
			(devp->color_depth_mode == 0)) {
			h_size = roundup(h_size *
				VDIN_YUV422_10BIT_PER_PIXEL_BYTE,
				devp->canvas_align);
			devp->canvas_alin_w = h_size /
				VDIN_YUV422_10BIT_PER_PIXEL_BYTE;
		} else {
			h_size = roundup(h_size *
				VDIN_YUV422_8BIT_PER_PIXEL_BYTE,
				devp->canvas_align);
			devp->canvas_alin_w = h_size /
				VDIN_YUV422_8BIT_PER_PIXEL_BYTE;
		}
	}
	mem_size = h_size * v_size;
	if ((devp->format_convert >= VDIN_FORMAT_CONVERT_YUV_NV12) &&
		(devp->format_convert <= VDIN_FORMAT_CONVERT_RGB_NV21))
		mem_size = (mem_size * 3)/2;
	devp->vfmem_size = PAGE_ALIGN(mem_size) + dolby_size_byte;
	devp->vfmem_size = (devp->vfmem_size/PAGE_SIZE + 1)*PAGE_SIZE;

	if (devp->set_canvas_manual == 1) {
		for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++) {
			if (vdin_set_canvas_addr[i].dmabuff == NULL)
				break;

			vdin_set_canvas_addr[i].paddr =
				roundup(vdin_set_canvas_addr[i].paddr,
					devp->canvas_align);
		}

		devp->canvas_max_num = max_buffer_num = i;
		devp->vfmem_max_cnt = max_buffer_num;
	}


	mem_size = PAGE_ALIGN(mem_size) * max_buffer_num +
		dolby_size_byte * max_buffer_num;
	mem_size = (mem_size/PAGE_SIZE + 1)*PAGE_SIZE;

	if (mem_size > devp->cma_mem_size) {
		mem_size = devp->cma_mem_size;
		pr_err("\nvdin%d cma_mem_size is not enough!!!\n", devp->index);
		devp->cma_mem_alloc = 0;
		return 1;
	}
	if (devp->index == 0)
		strcpy(vdin_name, "vdin0");
	else if (devp->index == 1)
		strcpy(vdin_name, "vdin1");

	if (devp->cma_config_flag == 0x101) {
		/* canvas or afbce paddr */
		for (i = 0; i < max_buffer_num; i++) {
			devp->vfmem_start[i] = codec_mm_alloc_for_dma(vdin_name,
				devp->vfmem_size/PAGE_SIZE, 0, flags);
			if (devp->vfmem_start[i] == 0) {
				pr_err("\nvdin%d buf[%d]codec alloc fail!!!\n",
					devp->index, i);
				devp->cma_mem_alloc = 0;
				return 1;
			}
			if (devp->afbce_info) {
				devp->afbce_info->fm_body_paddr[i] =
					devp->vfmem_start[i];
			}
			pr_info("vdin%d buf[%d] mem_start = 0x%lx, mem_size = 0x%x\n",
				devp->index, i,
				devp->vfmem_start[i], devp->vfmem_size);
		}
		if (devp->afbce_info)
			devp->afbce_info->frame_body_size = devp->vfmem_size;
		devp->mem_size = mem_size;

		if (devp->afbce_info) {
			devp->afbce_info->head_paddr = codec_mm_alloc_for_dma(
				vdin_name, afbce_head_size_byte/PAGE_SIZE,
				0, flags);
			if (devp->afbce_info->head_paddr == 0) {
				pr_err("\nvdin%d header codec alloc fail!!!\n",
					devp->index);
				devp->cma_mem_alloc = 0;
				return 1;
			}
			devp->afbce_info->table_paddr = codec_mm_alloc_for_dma(
				vdin_name, afbce_table_size_byte/PAGE_SIZE,
				0, flags);
			if (devp->afbce_info->table_paddr == 0) {
				pr_err("\nvdin%d table codec alloc fail!!!\n",
					devp->index);
				codec_mm_free_for_dma(vdin_name,
					devp->afbce_info->head_paddr);
				devp->cma_mem_alloc = 0;
				return 1;
			}
			devp->afbce_info->head_size = afbce_head_size_byte;
			devp->afbce_info->table_size = afbce_table_size_byte;

			pr_info("vdin%d head_start = 0x%lx, head_size = 0x%x\n",
				devp->index, devp->afbce_info->head_paddr,
				devp->afbce_info->head_size);
			pr_info("vdin%d table_start = 0x%lx, table_size = 0x%x\n",
				devp->index, devp->afbce_info->table_paddr,
				devp->afbce_info->table_size);
		}

		devp->cma_mem_alloc = 1;
	} else if (devp->cma_config_flag == 0x1) {
		devp->mem_start = codec_mm_alloc_for_dma(vdin_name,
			mem_size/PAGE_SIZE, 0, flags);
		if (devp->mem_start == 0) {
			pr_err("\nvdin%d codec alloc fail!!!\n",
				devp->index);
			devp->cma_mem_alloc = 0;
			return 1;
		}
		devp->mem_size = mem_size;
		devp->cma_mem_alloc = 1;
		pr_info("vdin%d mem_start = 0x%lx, mem_size = 0x%x\n",
			devp->index, devp->mem_start, devp->mem_size);
	} else if (devp->cma_config_flag == 0x100) {
		for (i = 0; i < max_buffer_num; i++) {
			devp->vfvenc_pages[i] = dma_alloc_from_contiguous(
				&(devp->this_pdev->dev),
				devp->vfmem_size >> PAGE_SHIFT, 0);
			if (!devp->vfvenc_pages[i]) {
				devp->cma_mem_alloc = 0;
				pr_err("\nvdin%d cma mem undefined2.\n",
					devp->index);
				return 1;
			}
			devp->vfmem_start[i] =
				page_to_phys(devp->vfvenc_pages[i]);
			pr_info("vdin%d buf[%d]mem_start = 0x%lx, mem_size = 0x%x\n",
				devp->index, i,
				devp->vfmem_start[i], devp->vfmem_size);
		}
		devp->mem_size = mem_size;
		devp->cma_mem_alloc = 1;
	} else {
		/* canvas or afbce paddr */
		devp->venc_pages = dma_alloc_from_contiguous(
			&(devp->this_pdev->dev),
			devp->cma_mem_size >> PAGE_SHIFT, 0);
		if (!devp->venc_pages) {
			devp->cma_mem_alloc = 0;
			pr_err("\nvdin%d cma mem undefined2.\n",
				devp->index);
			return 1;
		}
		devp->mem_start = page_to_phys(devp->venc_pages);
		devp->mem_size  = mem_size;

		/* set fm_body_paddr */
		if (devp->afbce_info) {
			ref_paddr = devp->mem_start;
			devp->afbce_info->frame_body_size = devp->vfmem_size;
			for (i = 0; i < max_buffer_num; i++) {
				ref_paddr = devp->mem_start +
					(devp->vfmem_size * i);
				devp->afbce_info->fm_body_paddr[i] = ref_paddr;

				pr_info("vdin%d body[%d]_start = 0x%lx, body_size = 0x%x\n",
					devp->index, i,
					devp->afbce_info->fm_body_paddr[i],
					devp->afbce_info->frame_body_size);
			}

			/* afbce header & table paddr */
			devp->afbce_info->head_paddr = ref_paddr +
				devp->vfmem_size;
			devp->afbce_info->head_size = 2 * SZ_1M;/*2M*/
			devp->afbce_info->table_paddr =
				devp->afbce_info->head_paddr +
				devp->afbce_info->head_size;
			devp->afbce_info->table_size = 2 * SZ_1M;/*2M*/

			pr_info("vdin%d head_start = 0x%lx, head_size = 0x%x\n",
				devp->index, devp->afbce_info->head_paddr,
				devp->afbce_info->head_size);
			pr_info("vdin%d table_start = 0x%lx, table_size = 0x%x\n",
				devp->index, devp->afbce_info->table_paddr,
				devp->afbce_info->table_size);

			/*check memory over the boundary*/
			mem_used = devp->afbce_info->table_paddr +
				devp->afbce_info->table_size -
				devp->afbce_info->fm_body_paddr[0];
			if (mem_used > devp->cma_mem_size) {
				pr_err("vdin%d error: mem_used(%d) > cma_mem_size(%d)\n",
					devp->index, mem_used,
					devp->cma_mem_size);
				return 1;
			}
		}

		devp->cma_mem_alloc = 1;
		pr_info("vdin%d mem_start = 0x%lx, mem_size = 0x%x\n",
			devp->index, devp->mem_start, devp->mem_size);
	}

	/* set afbce head paddr */
	if (devp->afbce_info) {
		/* 1 block = 32 * 4 pixle = 128 pixel */
		/* there is a header in one block, a header has 4 bytes */
		frame_head_size = (int)roundup(devp->vfmem_size, 128);
		/*h_active * v_active / 128 * 4 = frame_head_size*/
		frame_head_size = PAGE_ALIGN(frame_head_size / 32);

		devp->afbce_info->frame_head_size = frame_head_size;

		for (i = 0; i < max_buffer_num; i++) {
			devp->afbce_info->fm_head_paddr[i] =
				devp->afbce_info->head_paddr +
				(frame_head_size*i);

			pr_info("vdin%d fm_head_paddr[%d] = 0x%lx, frame_head_size = 0x%x\n",
				devp->index, i,
				devp->afbce_info->fm_head_paddr[i],
				frame_head_size);
		}

		/* set afbce table paddr */
		mmu_used = devp->afbce_info->frame_body_size >> 12;
		mmu_used = mmu_used * 4;
		mmu_used = PAGE_ALIGN(mmu_used);
		devp->afbce_info->frame_table_size = mmu_used;

		for (i = 0; i < max_buffer_num; i++) {
			devp->afbce_info->fm_table_paddr[i] =
				devp->afbce_info->table_paddr + (mmu_used*i);

			pr_info("vdin%d fm_table_paddr[%d]=0x%lx, frame_table_size = 0x%x\n",
				devp->index, i,
				devp->afbce_info->fm_table_paddr[i],
				devp->afbce_info->frame_table_size);
		}
	}

	pr_info("vdin%d cma alloc ok!\n", devp->index);
	return 0;
}

/*this function used for codec cma release
 *	1.call codec_mm_free_for_dma() or
 *	  dma_release_from_contiguous() to relase cma;
 *	2.reset mem_start & mem_size & cma_mem_alloc to 0;
 */
void vdin_cma_release(struct vdin_dev_s *devp)
{
	char vdin_name[6];
	unsigned int i;

	if ((devp->cma_config_en == 0) ||
		(devp->cma_mem_alloc == 0)) {
		pr_err("\nvdin%d %s fail for (%d,%d)!!!\n",
			devp->index, __func__, devp->cma_config_en,
			devp->cma_mem_alloc);
		return;
	}
	if (devp->index == 0)
		strcpy(vdin_name, "vdin0");
	else if (devp->index == 1)
		strcpy(vdin_name, "vdin1");

	if (devp->cma_config_flag == 0x101) {
		if (devp->afbce_info) {
			if (devp->afbce_info->head_paddr) {
				codec_mm_free_for_dma(vdin_name,
					devp->afbce_info->head_paddr);
			}
			if (devp->afbce_info->table_paddr) {
				codec_mm_free_for_dma(vdin_name,
					devp->afbce_info->table_paddr);
			}
		}
		/* canvas or afbce paddr */
		for (i = 0; i < devp->vfmem_max_cnt; i++)
			codec_mm_free_for_dma(vdin_name, devp->vfmem_start[i]);
		pr_info("vdin%d codec cma release ok!\n", devp->index);
	} else if (devp->cma_config_flag == 0x1) {
		codec_mm_free_for_dma(vdin_name, devp->mem_start);
		pr_info("vdin%d codec cma release ok!\n", devp->index);
	} else if (devp->cma_config_flag == 0x100) {
		for (i = 0; i < devp->vfmem_max_cnt; i++)
			dma_release_from_contiguous(
				&(devp->this_pdev->dev),
				devp->vfvenc_pages[i],
				devp->vfmem_size >> PAGE_SHIFT);
		pr_info("vdin%d cma release ok!\n", devp->index);
	} else if (devp->venc_pages
		&& devp->cma_mem_size
		&& (devp->cma_config_flag == 0)) {
		dma_release_from_contiguous(
			&(devp->this_pdev->dev),
			devp->venc_pages,
			devp->cma_mem_size >> PAGE_SHIFT);
		pr_info("vdin%d cma release ok!\n", devp->index);
	} else {
		pr_err("\nvdin%d %s fail for (%d,0x%x,0x%lx)!!!\n",
			devp->index, __func__, devp->cma_mem_size,
			devp->cma_config_flag, devp->mem_start);
	}
	devp->mem_start = 0;
	devp->mem_size = 0;
	devp->cma_mem_alloc = 0;
}
/*@20170823 new add for the case of csc change after signal stable*/
void vdin_cma_malloc_mode(struct vdin_dev_s *devp)
{
	unsigned int h_size, v_size;

	h_size = devp->h_active;
	v_size = devp->v_active;
	if ((h_size <= VDIN_YUV444_MAX_CMA_WIDTH) &&
		(v_size <= VDIN_YUV444_MAX_CMA_HEIGH) &&
		(devp->cma_mem_mode == 1))
		devp->force_yuv444_malloc = 1;
	else
		devp->force_yuv444_malloc = 0;

}
#endif

