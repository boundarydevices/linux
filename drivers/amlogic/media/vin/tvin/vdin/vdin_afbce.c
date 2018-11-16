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

/* fixed config mif by default */
void vdin_mif_config_init(struct vdin_dev_s *devp)
{
	if (devp->index == 0) {
		W_VCBUS_BIT(VDIN_MISC_CTRL,
			1, VDIN0_MIF_ENABLE_BIT, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL,
			0, VDIN0_OUT_AFBCE_BIT, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL,
			1, VDIN0_OUT_MIF_BIT, 1);
	} else {
		W_VCBUS_BIT(VDIN_MISC_CTRL,
			1, VDIN1_MIF_ENABLE_BIT, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL,
			0, VDIN1_OUT_AFBCE_BIT, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL,
			1, VDIN1_OUT_MIF_BIT, 1);
	}
}

/* only support init vdin0 mif/afbce */
void vdin_write_mif_or_afbce_init(struct vdin_dev_s *devp)
{
	enum vdin_output_mif_e sel;

	if ((devp->afbce_flag & VDIN_AFBCE_EN) == 0)
		return;

	if (devp->afbce_mode == 0)
		sel = VDIN_OUTPUT_TO_MIF;
	else
		sel = VDIN_OUTPUT_TO_AFBCE;

	if (devp->index == 0) {
		if (sel == VDIN_OUTPUT_TO_MIF) {
			vdin_afbce_hw_disable();

			W_VCBUS_BIT(VDIN_MISC_CTRL,
				1, VDIN0_MIF_ENABLE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL,
				0, VDIN0_OUT_AFBCE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL,
				1, VDIN0_OUT_MIF_BIT, 1);
		} else if (sel == VDIN_OUTPUT_TO_AFBCE) {
			W_VCBUS_BIT(VDIN_MISC_CTRL,
				1, VDIN0_MIF_ENABLE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL,
				0, VDIN0_OUT_MIF_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL,
				1, VDIN0_OUT_AFBCE_BIT, 1);

			vdin_afbce_hw_enable();
		}
	}
}

/* only support config vdin0 mif/afbce dynamically */
void vdin_write_mif_or_afbce(struct vdin_dev_s *devp,
	enum vdin_output_mif_e sel)
{

	if ((devp->afbce_flag & VDIN_AFBCE_EN) == 0)
		return;

	if (sel == VDIN_OUTPUT_TO_MIF) {
		rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 0, 8, 1);
		rdma_write_reg_bits(devp->rdma_handle, VDIN_MISC_CTRL,
			0, VDIN0_OUT_AFBCE_BIT, 1);
		rdma_write_reg_bits(devp->rdma_handle, VDIN_MISC_CTRL,
			1, VDIN0_OUT_MIF_BIT, 1);
	} else if (sel == VDIN_OUTPUT_TO_AFBCE) {
		rdma_write_reg_bits(devp->rdma_handle, VDIN_MISC_CTRL,
			0, VDIN0_OUT_MIF_BIT, 1);
		rdma_write_reg_bits(devp->rdma_handle, VDIN_MISC_CTRL,
			1, VDIN0_OUT_AFBCE_BIT, 1);
		rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 1, 8, 1);
	}
}
/*
static void afbce_wr(uint32_t reg, const uint32_t val)
{
	wr(0, reg, val);
}
*/
#define VDIN_AFBCE_HOLD_LINE_NUM    4
void vdin_afbce_update(struct vdin_dev_s *devp)
{
	int hold_line_num = VDIN_AFBCE_HOLD_LINE_NUM;
	int reg_format_mode;/* 0:444 1:422 2:420 */
	int reg_fmt444_comb;
	int sblk_num;
	int uncmp_bits;
	int uncmp_size;

	if (!devp->afbce_info)
		return;

#ifndef CONFIG_AMLOGIC_MEDIA_RDMA
	pr_info("##############################################\n");
	pr_info("vdin afbce must use RDMA,but it not be opened\n");
	pr_info("##############################################\n");
#endif

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

	/* bit size of uncompression mode */
	uncmp_size = (((((16*uncmp_bits*sblk_num)+7)>>3)+31)/32)<<1;
	/*
	 *pr_info("%s: dest_cfmt=%d, reg_format_mode=%d, uncmp_bits=%d,
	 *         sblk_num=%d, uncmp_size=%d\n",
	 *	__func__, devp->prop.dest_cfmt, reg_format_mode,
	 *	uncmp_bits, sblk_num, uncmp_size);
	 */

	rdma_write_reg(devp->rdma_handle, AFBCE_MODE,
		(0 & 0x7) << 29 | (0 & 0x3) << 26 | (3 & 0x3) << 24 |
		(hold_line_num & 0x7f) << 16 |
		(2 & 0x3) << 14 | (reg_fmt444_comb & 0x1));

	rdma_write_reg_bits(devp->rdma_handle,
		AFBCE_MIF_SIZE, (uncmp_size & 0x1fff), 16, 5);/* uncmp_size */

	rdma_write_reg(devp->rdma_handle, AFBCE_FORMAT,
		(reg_format_mode  & 0x3) << 8 |
		(uncmp_bits & 0xf) << 4 |
		(uncmp_bits & 0xf));
}

void vdin_afbce_config(struct vdin_dev_s *devp)
{
	int hold_line_num = VDIN_AFBCE_HOLD_LINE_NUM;
	int lbuf_depth = 256;
	int lossy_luma_en = 0;
	int lossy_chrm_en = 0;
	int cur_mmu_used = 0;
	int reg_format_mode;//0:444 1:422 2:420
	int reg_fmt444_comb;
	int sblk_num;
	int uncmp_bits;
	int uncmp_size;
	int def_color_0 = 4095;
	int def_color_1 = 2048;
	int def_color_2 = 2048;
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

	if (!devp->afbce_info)
		return;

#ifndef CONFIG_AMLOGIC_MEDIA_RDMA
	pr_info("##############################################\n");
	pr_info("vdin afbce must use RDMA,but it not be opened\n");
	pr_info("##############################################\n");
#endif

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

	W_VCBUS(AFBCE_MODE,
		(0 & 0x7) << 29 | (0 & 0x3) << 26 | (3 & 0x3) << 24 |
		(hold_line_num & 0x7f) << 16 |
		(2 & 0x3) << 14 | (reg_fmt444_comb & 0x1));

	W_VCBUS_BIT(AFBCE_QUANT_ENABLE, (lossy_luma_en & 0x1), 0, 1);//loosy
	W_VCBUS_BIT(AFBCE_QUANT_ENABLE, (lossy_chrm_en & 0x1), 4, 1);//loosy

	if (devp->afbce_flag & VDIN_AFBCE_EN_LOOSY) {
		W_VCBUS(AFBCE_QUANT_ENABLE, 0xc11);
		pr_info("afbce use lossy compression mode\n");
	}

	W_VCBUS(AFBCE_SIZE_IN,
		((devp->h_active & 0x1fff) << 16) |  // hsize_in of afbc input
		((devp->v_active & 0x1fff) << 0)    // vsize_in of afbc input
		);

	W_VCBUS(AFBCE_BLK_SIZE_IN,
		((hblksize_out & 0x1fff) << 16) |  // out blk hsize
		((vblksize_out & 0x1fff) << 0)    // out blk vsize
		);

	//head addr of compressed data
	W_VCBUS(AFBCE_HEAD_BADDR,
		devp->afbce_info->fm_head_paddr[0]);

	W_VCBUS_BIT(AFBCE_MIF_SIZE, (uncmp_size & 0x1fff), 16, 5);//uncmp_size

	/* how to set reg when we use crop ? */
	// scope of hsize_in ,should be a integer multipe of 32
	// scope of vsize_in ,should be a integer multipe of 4
	W_VCBUS(AFBCE_PIXEL_IN_HOR_SCOPE,
		((enc_win_end_h & 0x1fff) << 16) |
		((enc_win_bgn_h & 0x1fff) << 0));

	// scope of hsize_in ,should be a integer multipe of 32
	// scope of vsize_in ,should be a integer multipe of 4
	W_VCBUS(AFBCE_PIXEL_IN_VER_SCOPE,
		((enc_win_end_v & 0x1fff) << 16) |
		((enc_win_bgn_v & 0x1fff) << 0));

	W_VCBUS(AFBCE_CONV_CTRL, lbuf_depth);//fix 256

	W_VCBUS(AFBCE_MIF_HOR_SCOPE,
		((blk_out_bgn_h & 0x3ff) << 16) |  // scope of out blk hsize
		((blk_out_end_h & 0xfff) << 0)    // scope of out blk vsize
		);

	W_VCBUS(AFBCE_MIF_VER_SCOPE,
		((blk_out_bgn_v & 0x3ff) << 16) |  // scope of out blk hsize
		((blk_out_end_v & 0xfff) << 0)    // scope of out blk vsize
		);

	W_VCBUS(AFBCE_FORMAT,
		(reg_format_mode  & 0x3) << 8 |
		(uncmp_bits & 0xf) << 4 |
		(uncmp_bits & 0xf));

	W_VCBUS(AFBCE_DEFCOLOR_1,
		((def_color_3 & 0xfff) << 12) |  // def_color_a
		((def_color_0 & 0xfff) << 0)    // def_color_y
		);

	W_VCBUS(AFBCE_DEFCOLOR_2,
		((def_color_2 & 0xfff) << 12) |  // def_color_v
		((def_color_1 & 0xfff) << 0)    // def_color_u
		);

	W_VCBUS_BIT(AFBCE_MMU_RMIF_CTRL4, devp->afbce_info->table_paddr, 0, 32);
	W_VCBUS_BIT(AFBCE_MMU_RMIF_SCOPE_X, cur_mmu_used, 0, 12);

	W_VCBUS_BIT(AFBCE_ENABLE, 1, 12, 1); //set afbce pulse mode
	W_VCBUS_BIT(AFBCE_ENABLE, 0, 8, 1);//disable afbce
}

void vdin_afbce_maptable_init(struct vdin_dev_s *devp)
{
	unsigned int i, j;
	unsigned int highmem_flag = 0;
	unsigned long ptable = 0;
	unsigned int *vtable = NULL;
	unsigned int body;
	unsigned int size;
	void *p = NULL;

	if (!devp->afbce_info)
		return;

	size = roundup(devp->afbce_info->frame_body_size, 4096);

	ptable = devp->afbce_info->fm_table_paddr[0];
	if (devp->cma_config_flag == 0x101)
		highmem_flag = PageHighMem(phys_to_page(ptable));
	else
		highmem_flag = PageHighMem(phys_to_page(ptable));

	for (i = 0; i < devp->vfmem_max_cnt; i++) {
		ptable = devp->afbce_info->fm_table_paddr[i];
		if (highmem_flag == 0) {
			if (devp->cma_config_flag == 0x101)
				vtable = codec_mm_phys_to_virt(ptable);
			else if (devp->cma_config_flag == 0)
				vtable = phys_to_virt(ptable);
			else
				vtable = phys_to_virt(ptable);
		} else {
			vtable = (unsigned int *)vdin_vmap(ptable,
				devp->afbce_info->frame_table_size);
			if (vdin_dbg_en) {
				pr_err("----vdin vmap v: %p, p: %lx, size: %d\n",
					vtable, ptable,
					devp->afbce_info->frame_table_size);
			}
			if (!vtable) {
				pr_err("vmap fail, size: %d.\n",
					devp->afbce_info->frame_table_size);
				return;
			}

		}

		p = vtable;
		body = devp->afbce_info->fm_body_paddr[i]&0xffffffff;
		for (j = 0; j < size; j += 4096) {
			*vtable = ((j + body) >> 12) & 0x000fffff;
			vtable++;
		}

		/* clean tail data. */
		memset(vtable, 0, devp->afbce_info->frame_table_size -
				((char *)vtable - (char *)p));

		vdin_dma_flush(devp, p,
			devp->afbce_info->frame_table_size,
			DMA_TO_DEVICE);

		if (highmem_flag)
			vdin_unmap_phyaddr(p);

		vtable = NULL;
	}
}

void vdin_afbce_set_next_frame(struct vdin_dev_s *devp,
	unsigned int rdma_enable, struct vf_entry *vfe)
{
	unsigned char i;

	if (!devp->afbce_info)
		return;

	i = vfe->af_num;
	vfe->vf.compHeadAddr = devp->afbce_info->fm_head_paddr[i];
	vfe->vf.compBodyAddr = devp->afbce_info->fm_body_paddr[i];

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable) {
		rdma_write_reg(devp->rdma_handle, AFBCE_HEAD_BADDR,
			devp->afbce_info->fm_head_paddr[i]);
		rdma_write_reg_bits(devp->rdma_handle, AFBCE_MMU_RMIF_CTRL4,
			devp->afbce_info->fm_table_paddr[i], 0, 32);
		rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 1, 0, 1);
	} else
#endif
	{
		pr_info("afbce must use RDMA.\n");
	}

	vdin_afbce_clear_writedown_flag(devp);
}

void vdin_afbce_clear_writedown_flag(struct vdin_dev_s *devp)
{
	rdma_write_reg(devp->rdma_handle, AFBCE_CLR_FLAG, 1);
}

/* return 1: write down*/
int vdin_afbce_read_writedown_flag(void)
{
	int val1, val2;

	val1 = rd_bits(0, AFBCE_STA_FLAGT, 0, 1);
	val2 = rd_bits(0, AFBCE_STA_FLAGT, 2, 2);

	if ((val1 == 1) || (val2 == 0))
		return 1;
	else
		return 0;
}

void vdin_afbce_hw_disable(void)
{
	/*can not use RDMA*/
	W_VCBUS_BIT(AFBCE_ENABLE, 0, 8, 1);//disable afbce
}

void vdin_afbce_hw_enable(void)
{
	W_VCBUS_BIT(AFBCE_ENABLE, 1, 8, 1);
}

void vdin_afbce_hw_disable_rdma(struct vdin_dev_s *devp)
{
	rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 0, 8, 1);
}

void vdin_afbce_hw_enable_rdma(struct vdin_dev_s *devp)
{
	if (devp->afbce_flag & VDIN_AFBCE_EN_LOOSY)
		rdma_write_reg(devp->rdma_handle, AFBCE_QUANT_ENABLE, 0xc11);
	rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 1, 8, 1);
}

void vdin_afbce_soft_reset(void)
{
	W_VCBUS_BIT(AFBCE_MODE, 0, 30, 1);
	W_VCBUS_BIT(AFBCE_MODE, 1, 30, 1);
	W_VCBUS_BIT(AFBCE_MODE, 0, 30, 1);
}
