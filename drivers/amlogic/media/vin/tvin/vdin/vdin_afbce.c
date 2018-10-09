/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_afbce.c
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

/******************** READ ME ************************
 *
 * at afbce mode, 1 block = 32 * 4 pixel
 * there is a header in one block.
 * for example at 1080p,
 * header nembers = block nembers = 1920 * 1080 / (32 * 4)
 *
 * table map(only at non-mmu mode):
 * afbce data was saved at "body" region,
 * body region has been divided for every 4K(4096 bytes) and 4K unit,
 * table map contents is : (body addr >> 12)
 *
 * at non-mmu mode(just vdin non-mmu mode):
 * ------------------------------
 *          header
 *     (can analysis body addr)
 * ------------------------------
 *          table map
 *     (save body addr)
 * ------------------------------
 *          body
 *     (save afbce data)
 * ------------------------------
 */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/slab.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "vdin_ctl.h"
#include "vdin_regs.h"
#include "vdin_drv.h"
#include "vdin_vf.h"
#include "vdin_canvas.h"
#include "vdin_afbce.h"

static unsigned int max_buf_num = VDIN_CANVAS_MAX_CNT;
static unsigned int min_buf_num = 4;
static unsigned int max_buf_width = VDIN_CANVAS_MAX_WIDTH_HD;
static unsigned int max_buf_height = VDIN_CANVAS_MAX_HEIGH;
/* one frame max metadata size:32x280 bits = 1120bytes(0x460) */
unsigned int dolby_size_bytes = PAGE_SIZE;

unsigned int vdin_afbce_cma_alloc(struct vdin_dev_s *devp)
{
	char vdin_name[6];
	unsigned int mem_size, h_size, v_size;
	int flags = CODEC_MM_FLAGS_CMA_FIRST|CODEC_MM_FLAGS_CMA_CLEAR|
		CODEC_MM_FLAGS_CPU;
	unsigned int max_buffer_num = min_buf_num;
	unsigned int i;
	/*afbce head need 1036800 byte at most*/
	unsigned int afbce_head_size_byte = PAGE_SIZE * 300;/*1.2M*/
	/*afbce map_table need 218700 byte at most*/
	unsigned int afbce_table_size_byte = PAGE_SIZE * 60;/*0.3M*/
	unsigned int afbce_mem_used;
	unsigned int frame_head_size;
	unsigned int mmu_used;
	//unsigned long afbce_head_phy_addr;
	//unsigned long afbce_table_phy_addr;
	unsigned long body_start_paddr;

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
	devp->canvas_max_num = max_buffer_num;

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
	devp->vfmem_size = PAGE_ALIGN(mem_size) + dolby_size_bytes;
	devp->vfmem_size = (devp->vfmem_size/PAGE_SIZE + 1)*PAGE_SIZE;

	mem_size = PAGE_ALIGN(mem_size) * max_buffer_num +
		dolby_size_bytes * max_buffer_num;
	mem_size = (mem_size/PAGE_SIZE + 1)*PAGE_SIZE;
	if (mem_size > devp->cma_mem_size)
		mem_size = devp->cma_mem_size;
	if (devp->index == 0)
		strcpy(vdin_name, "vdin0");
	else if (devp->index == 1)
		strcpy(vdin_name, "vdin1");


	if (devp->cma_config_flag == 0x101) {
		devp->afbce_info->head_paddr = codec_mm_alloc_for_dma(
			vdin_name, afbce_head_size_byte/PAGE_SIZE, 0, flags);
		devp->afbce_info->table_paddr = codec_mm_alloc_for_dma(
			vdin_name, afbce_table_size_byte/PAGE_SIZE, 0, flags);
		devp->afbce_info->head_size = afbce_head_size_byte;
		devp->afbce_info->table_size = afbce_table_size_byte;
		devp->afbce_info->frame_body_size = devp->vfmem_size;

		pr_info("vdin%d head_start = 0x%lx, head_size = 0x%x\n",
			devp->index, devp->afbce_info->head_paddr,
			devp->afbce_info->head_size);
		pr_info("vdin%d table_start = 0x%lx, table_size = 0x%x\n",
			devp->index, devp->afbce_info->table_paddr,
			devp->afbce_info->table_size);

		/* set fm_body_paddr */
		for (i = 0; i < max_buffer_num; i++) {
			devp->afbce_info->fm_body_paddr[i] =
				codec_mm_alloc_for_dma(vdin_name,
				devp->vfmem_size/PAGE_SIZE, 0, flags);
			if (devp->afbce_info->fm_body_paddr[i] == 0) {
				pr_err("\nvdin%d-afbce buf[%d]codec alloc fail!!!\n",
					devp->index, i);
				devp->cma_mem_alloc = 0;
			} else {
				devp->cma_mem_alloc = 1;
				pr_info("vdin%d fm_body_paddr[%d] = 0x%lx, body_size = 0x%x\n",
					devp->index, i,
					devp->afbce_info->fm_body_paddr[i],
					devp->afbce_info->frame_body_size);
			}

			if (devp->cma_mem_alloc == 0)
				return 1;
		}
		pr_info("vdin%d-afbce codec cma alloc ok!\n", devp->index);
		devp->mem_size = mem_size;
	} else if (devp->cma_config_flag == 0) {
		devp->venc_pages = dma_alloc_from_contiguous(
			&(devp->this_pdev->dev),
			devp->cma_mem_size >> PAGE_SHIFT, 0);
		if (devp->venc_pages) {
			devp->mem_start =
				page_to_phys(devp->venc_pages);
			devp->mem_size  = mem_size;
			devp->cma_mem_alloc = 1;

			devp->afbce_info->head_paddr = devp->mem_start;
			devp->afbce_info->head_size = 2*SZ_1M;/*2M*/
			devp->afbce_info->table_paddr =
				devp->mem_start + devp->afbce_info->head_paddr;
			devp->afbce_info->table_size = 2*SZ_1M;/*2M*/
			devp->afbce_info->frame_body_size = devp->vfmem_size;

			body_start_paddr = devp->afbce_info->table_paddr +
				devp->afbce_info->table_size;

			pr_info("vdin%d head_start = 0x%lx, head_size = 0x%x\n",
				devp->index, devp->afbce_info->head_paddr,
				devp->afbce_info->head_size);
			pr_info("vdin%d table_start = 0x%lx, table_size = 0x%x\n",
				devp->index, devp->afbce_info->table_paddr,
				devp->afbce_info->table_size);

			/* set fm_body_paddr */
			for (i = 0; i < max_buffer_num; i++) {
				devp->afbce_info->fm_body_paddr[i] =
				body_start_paddr + (devp->vfmem_size * i);

				pr_info("vdin%d body[%d]_start = 0x%lx, body_size = 0x%x\n",
					devp->index, i,
					devp->afbce_info->fm_body_paddr[i],
					devp->afbce_info->frame_body_size);
			}

			/*check memory over the boundary*/
			afbce_mem_used =
				devp->afbce_info->fm_body_paddr[max_buffer_num]
				+ devp->afbce_info->frame_body_size -
				devp->afbce_info->head_paddr;
			if (afbce_mem_used > devp->cma_mem_size) {
				pr_info("afbce mem: afbce_mem_used(%d) > cma_mem_size(%d)\n",
					afbce_mem_used, devp->cma_mem_size);
				return 1;
			}
			devp->cma_mem_alloc = 1;
			pr_info("vdin%d cma alloc ok!\n", devp->index);
		} else {
			devp->cma_mem_alloc = 0;
			pr_err("\nvdin%d-afbce cma mem undefined2.\n",
				devp->index);
			return 1;
		}
	}

	/* 1 block = 32 * 4 pixle = 128 pixel */
	/* there is a header in one block, a header has 4 bytes */
	/* set fm_head_paddr start */
	frame_head_size = roundup(devp->h_active * devp->v_active, 128);
	/*h_active * v_active / 128 * 4 = frame_head_size*/
	frame_head_size = devp->h_active * devp->v_active / 32;
	frame_head_size = PAGE_ALIGN(frame_head_size);

	devp->afbce_info->frame_head_size = frame_head_size;

	for (i = 0; i < max_buffer_num; i++) {
		devp->afbce_info->fm_head_paddr[i] =
			devp->afbce_info->head_paddr + (frame_head_size*i);

		pr_info("vdin%d fm_head_paddr[%d] = 0x%lx, frame_head_size = 0x%x\n",
			devp->index, i,
			devp->afbce_info->fm_head_paddr[i],
			frame_head_size);
	}
	/* set fm_head_paddr end */

	/* set fm_table_paddr start */
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
	/* set fm_table_paddr end */

	return 0;
}

void vdin_afbce_cma_release(struct vdin_dev_s *devp)
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
		codec_mm_free_for_dma(vdin_name, devp->afbce_info->head_paddr);
		codec_mm_free_for_dma(vdin_name, devp->afbce_info->table_paddr);
		for (i = 0; i < devp->vfmem_max_cnt; i++)
			codec_mm_free_for_dma(vdin_name,
				devp->afbce_info->fm_body_paddr[i]);
		pr_info("vdin%d-afbce codec cma release ok!\n", devp->index);
	} else if (devp->venc_pages
		&& devp->cma_mem_size
		&& (devp->cma_config_flag == 0)) {
		dma_release_from_contiguous(
			&(devp->this_pdev->dev),
			devp->venc_pages,
			devp->cma_mem_size >> PAGE_SHIFT);
		pr_info("vdin%d-afbce cma release ok!\n", devp->index);
	} else {
		pr_err("\nvdin%d %s fail for (%d,0x%x,0x%lx)!!!\n",
			devp->index, __func__, devp->cma_mem_size,
			devp->cma_config_flag, devp->mem_start);
	}
	devp->mem_start = 0;
	devp->mem_size = 0;
	devp->cma_mem_alloc = 0;
}

void vdin_write_mif_or_afbce(struct vdin_dev_s *devp,
	enum vdin_output_mif_e sel)
{
	unsigned int offset = devp->addr_offset;

	if (offset == 0) {
		if (sel == VDIN_OUTPUT_TO_MIF) {
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_MIF_ENABLE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_OUT_MIF_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 0, VDIN0_OUT_AFBCE_BIT, 1);
		} else if (sel == VDIN_OUTPUT_TO_AFBCE) {
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_MIF_ENABLE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 0, VDIN0_OUT_MIF_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_OUT_AFBCE_BIT, 1);
		}
	} else {
		if (sel == VDIN_OUTPUT_TO_MIF) {
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN1_MIF_ENABLE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN1_OUT_MIF_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 0, VDIN1_OUT_AFBCE_BIT, 1);
		} else if (sel == VDIN_OUTPUT_TO_AFBCE) {
			/*sel vdin1 afbce: not support in sw now,
			 *just reserved interface
			 */
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN1_MIF_ENABLE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 0, VDIN1_OUT_MIF_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN1_OUT_AFBCE_BIT, 1);
		}
	}
}

static void afbce_wr(uint32_t reg, const uint32_t val)
{
	wr(0, reg, val);
}

void vdin_afbce_config(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	int hold_line_num = 4;
	int lbuf_depth = 256;
	int lossy_luma_en = 0;
	int lossy_chrm_en = 0;
	int cur_mmu_used = 0;
	int reg_format_mode;//0:444 1:422 2:420
	int reg_fmt444_comb;
	int sblk_num;
	int uncmp_bits;
	int uncmp_size;
	int def_color_0 = 0;
	int def_color_1 = 0;
	int def_color_2 = 0;
	int def_color_3 = 0;
	int hblksize_out = (devp->h_active + 31) >> 5;
	int vblksize_out = (devp->v_active + 3)  >> 2;
	int blk_out_end_h;//output blk scope
	int blk_out_bgn_h;//output blk scope
	int blk_out_end_v;//output blk scope
	int blk_out_bgn_v;//output blk scope
	int enc_win_bgn_h;//input scope
	int enc_win_end_h;//input scope
	int enc_win_bgn_v;//input scope
	int enc_win_end_v;//input scope

	if (offset != 0) {
		pr_info("cat not use afbce on vdin1 at the moment\n");
		return;
	}

	enc_win_bgn_h = 0;
	enc_win_end_h = devp->h_active - 1;
	enc_win_bgn_v = 0;
	enc_win_end_v = devp->v_active - 1;

	blk_out_end_h   =  enc_win_bgn_h      >> 5 ;//output blk scope
	blk_out_bgn_h   = (enc_win_end_h+31)  >> 5 ;//output blk scope
	blk_out_end_v   =  enc_win_bgn_v      >> 2 ;//output blk scope
	blk_out_bgn_v   = (enc_win_end_v + 3) >> 2 ;//output blk scope

	if ((devp->prop.dest_cfmt == TVIN_YUV444) && (devp->h_active > 2048))
		reg_fmt444_comb = 1;
	else
	    reg_fmt444_comb = 0;

	if ((devp->prop.dest_cfmt == TVIN_NV12) ||
		(devp->prop.dest_cfmt == TVIN_NV21)) {
		reg_format_mode = 2;
	    sblk_num = 12;
	} else if ((devp->prop.dest_cfmt == TVIN_YUV422) ||
		(devp->prop.dest_cfmt == TVIN_YUYV422) ||
		(devp->prop.dest_cfmt == TVIN_YVYU422) ||
		(devp->prop.dest_cfmt == TVIN_UYVY422) ||
		(devp->prop.dest_cfmt == TVIN_VYUY422)) {
		reg_format_mode = 1;
	    sblk_num = 16;
	} else {
		reg_format_mode = 0;
	    sblk_num = 24;
	}
	uncmp_bits = devp->source_bitdepth;

	//bit size of uncompression mode
	uncmp_size = (((((16*uncmp_bits*sblk_num)+7)>>3)+31)/32)<<1;

	W_VCBUS_BIT(VDIN_WRARB_REQEN_SLV, 0x1, 3, 1);//vpu arb axi_enable
	W_VCBUS_BIT(VDIN_WRARB_REQEN_SLV, 0x1, 7, 1);//vpu arb axi_enable

	afbce_wr(AFBCE_MODE,
		(0 & 0x7) << 29 | (0 & 0x3) << 26 | (3 & 0x3) << 24 |
		(hold_line_num & 0x7f) << 16 |
		(2 & 0x3) << 14 | (reg_fmt444_comb & 0x1));

	W_VCBUS_BIT(AFBCE_QUANT_ENABLE, (lossy_luma_en & 0x1), 0, 1);//loosy
	W_VCBUS_BIT(AFBCE_QUANT_ENABLE, (lossy_chrm_en & 0x1), 4, 1);//loosy

	afbce_wr(AFBCE_SIZE_IN,
		((devp->h_active & 0x1fff) << 16) |  // hsize_in of afbc input
		((devp->v_active & 0x1fff) << 0)    // vsize_in of afbc input
		);

	afbce_wr(AFBCE_BLK_SIZE_IN,
		((hblksize_out & 0x1fff) << 16) |  // out blk hsize
		((vblksize_out & 0x1fff) << 0)    // out blk vsize
		);

	//head addr of compressed data
	afbce_wr(AFBCE_HEAD_BADDR, devp->afbce_info->fm_head_paddr[0]);

	W_VCBUS_BIT(AFBCE_MIF_SIZE, (uncmp_size & 0x1fff), 16, 5);//uncmp_size

	/* how to set reg when we use crop ? */
	// scope of hsize_in ,should be a integer multipe of 32
	// scope of vsize_in ,should be a integer multipe of 4
	afbce_wr(AFBCE_PIXEL_IN_HOR_SCOPE,
		((enc_win_end_h & 0x1fff) << 16) |
		((enc_win_bgn_h & 0x1fff) << 0));

	// scope of hsize_in ,should be a integer multipe of 32
	// scope of vsize_in ,should be a integer multipe of 4
	afbce_wr(AFBCE_PIXEL_IN_VER_SCOPE,
		((enc_win_end_v & 0x1fff) << 16) |
		((enc_win_bgn_v & 0x1fff) << 0));

	afbce_wr(AFBCE_CONV_CTRL, lbuf_depth);//fix 256

	afbce_wr(AFBCE_MIF_HOR_SCOPE,
		((blk_out_bgn_h & 0x3ff) << 16) |  // scope of out blk hsize
		((blk_out_end_h & 0xfff) << 0)    // scope of out blk vsize
		);

	afbce_wr(AFBCE_MIF_VER_SCOPE,
		((blk_out_bgn_v & 0x3ff) << 16) |  // scope of out blk hsize
		((blk_out_end_v & 0xfff) << 0)    // scope of out blk vsize
		);

	afbce_wr(AFBCE_FORMAT,
		(reg_format_mode  & 0x3) << 8 |
		(uncmp_bits & 0xf) << 4 |
		(uncmp_bits & 0xf));

	afbce_wr(AFBCE_DEFCOLOR_1,
		((def_color_3 & 0xfff) << 12) |  // def_color_a
		((def_color_0 & 0xfff) << 0)    // def_color_y
		);

	afbce_wr(AFBCE_DEFCOLOR_2,
		((def_color_2 & 0xfff) << 12) |  // def_color_v
		((def_color_1 & 0xfff) << 0)    // def_color_u
		);

	//cur_mmu_used += Rd(AFBCE_MMU_NUM); //4k addr have used in every frame;

	W_VCBUS_BIT(AFBCE_MMU_RMIF_CTRL4, devp->afbce_info->table_paddr, 0, 32);
	W_VCBUS_BIT(AFBCE_MMU_RMIF_SCOPE_X, cur_mmu_used, 0, 12);

	W_VCBUS_BIT(AFBCE_ENABLE, 1, 8, 1);//enable afbce
}

void vdin_afbce_maptable_init(struct vdin_dev_s *devp)
{
	unsigned int i, j;
	unsigned int *ptable = NULL;
	unsigned int *vtable = NULL;
	unsigned int body;
	unsigned int size;

	size = roundup(devp->afbce_info->frame_body_size, 4096);

	for (i = 0; i < devp->vfmem_max_cnt; i++) {
		ptable = (unsigned int *)
			(devp->afbce_info->fm_table_paddr[i]&0xffffffff);
		if (devp->cma_config_flag == 0x101)
			vtable = codec_mm_phys_to_virt((unsigned long)ptable);
		else if (devp->cma_config_flag == 0)
			vtable = phys_to_virt((unsigned long)ptable);

		body = devp->afbce_info->fm_body_paddr[i]&0xffffffff;
		for (j = 0; j < size; j += 4096) {
			*vtable = ((j + body) >> 12) & 0x000fffff;
			vtable++;
		}
	}
}

void vdin_afbce_set_next_frame(struct vdin_dev_s *devp,
	unsigned int rdma_enable, struct vf_entry *vfe)
{
	unsigned char i;
	unsigned int cur_mmu_used;

	i = vfe->af_num;
	cur_mmu_used = devp->afbce_info->fm_table_paddr[i] / 4;

#ifdef CONFIG_AML_RDMA
	if (rdma_enable)
		rdma_write_reg_bits(devp->rdma_handle,
			AFBCE_HEAD_BADDR, devp->afbce_info->fm_head_paddr[i]);
		rdma_write_reg(devp->rdma_handle,
			AFBCE_MMU_RMIF_SCOPE_X, cur_mmu_used, 0, 12);
	else
#endif
	afbce_wr(AFBCE_HEAD_BADDR, devp->afbce_info->fm_head_paddr[i]);
	W_VCBUS_BIT(AFBCE_MMU_RMIF_SCOPE_X, cur_mmu_used, 0, 12);
}

