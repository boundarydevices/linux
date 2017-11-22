/*
 * drivers/amlogic/media/deinterlace/deinterlace_dbg.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include "deinterlace_dbg.h"
#include "register.h"

void parse_cmd_params(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[2] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	strcat(delim1, delim2);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

void dump_di_reg(void)
{
	unsigned int i = 0, base_addr = 0;
	unsigned int size_reg_addr[57] = {
		0x1702, 0x1703, 0x2d01,
		0x2d01, 0x2d8f, 0x2d08,
		0x2d09, 0x2f00, 0x2f01,
		0x17d0, 0x17d1, 0x17d2,
		0x17d3, 0x17dd, 0x17de,
		0x17df, 0x17e0, 0x17f7,
		0x17f8, 0x17f9, 0x17fa,
		0x17c0, 0x17c1,	0x17a0,
		0x17a1, 0x17c3, 0x17c4,
		0x17cb, 0x17cc, 0x17a3,
		0x17a4, 0x17a5, 0x17a6,
		0x2f92, 0x2f93, 0x2f95,
		0x2f96, 0x2f98,	0x2f99,
		0x2f9b, 0x2f9c, 0x2f65,
		0x2f66, 0x2f67, 0x2f68,
		0x1a53, 0x1a54, 0x1a55,
		0x1a56, 0x17ea, 0x17eb,
		0x17ec, 0x17ed, 0x2012,
		0x2013, 0x2014, 0x2015
	};
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu())
		base_addr = 0xff900000;
	else
		base_addr = 0xd0100000;

	pr_info("----dump di reg----\n");
	pr_info("----dump size reg---");
	for (i = 0; i < 57; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((size_reg_addr[i]) << 2),
			size_reg_addr[i], RDMA_RD(size_reg_addr[i]));
	for (i = 0; i < 255; i++) {
		if (i == 0x45)
			pr_info("----nr reg----");
		if (i == 0x80)
			pr_info("----3d reg----");
		if (i == 0x9e)
			pr_info("---nr reg done---");
		if (i == 0x9c)
			pr_info("---3d reg done---");
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x1700 + i) << 2),
			0x1700 + i, RDMA_RD(0x1700 + i));
	}
	pr_info("----dump mcdi reg----\n");
	for (i = 0; i < 201; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2f00 + i) << 2),
			0x2f00 + i, RDMA_RD(0x2f00 + i));
	pr_info("----dump pulldown reg----\n");
	for (i = 0; i < 26; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2fd0 + i) << 2),
			0x2fd0 + i, RDMA_RD(0x2fd0 + i));
	pr_info("----dump bit mode reg----\n");
	for (i = 0; i < 4; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x20a7 + i) << 2),
			0x20a7 + i, RDMA_RD(0x20a7 + i));
	pr_info("[0x%x][0x%x]=0x%x\n",
		base_addr + (0x2022 << 2),
		0x2022, RDMA_RD(0x2022));
	pr_info("[0x%x][0x%x]=0x%x\n",
		base_addr + (0x17c1 << 2),
		0x17c1, RDMA_RD(0x17c1));
	pr_info("[0x%x][0x%x]=0x%x\n",
		base_addr + (0x17c2 << 2),
		0x17c2, RDMA_RD(0x17c2));
	pr_info("[0x%x][0x%x]=0x%x\n",
		base_addr + (0x1aa7 << 2),
		0x1aa7, RDMA_RD(0x1aa7));
	pr_info("----dump dnr reg----\n");
	for (i = 0; i < 29; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2d00 + i) << 2),
			0x2d00 + i, RDMA_RD(0x2d00 + i));
	pr_info("----dump if0 reg----\n");
	for (i = 0; i < 26; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x1a60 + i) << 2),
			0x1a50 + i, RDMA_RD(0x1a50 + i));
	pr_info("----dump gate reg----\n");
	pr_info("[0x%x][0x1718]=0x%x\n",
			base_addr + ((0x1718) << 2),
			RDMA_RD(0x1718));
	for (i = 0; i < 5; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2006 + i) << 2),
			0x2006 + i, RDMA_RD(0x2006 + i));
	pr_info("[0x%x][0x%x]=0x%x\n",
		base_addr + ((0x2dff) << 2),
		0x2dff, RDMA_RD(0x2dff));
	pr_info("----dump if2 reg----\n");
	for (i = 0; i < 29; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2010 + i) << 2),
			0x2010 + i, RDMA_RD(0x2010 + i));
	pr_info("----dump nr4 reg----\n");
	for (i = 0x2da4; i < 0x2df6; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + (i << 2),
			i, RDMA_RD(i));
	for (i = 0x3700; i < 0x373f; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + (i << 2),
			i, RDMA_RD(i));
	for (i = 0; i < 57; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((size_reg_addr[i]) << 2),
			size_reg_addr[i], RDMA_RD(size_reg_addr[i]));
	pr_info("----dump reg done----\n");
}

void dump_di_pre_stru(struct di_pre_stru_s *di_pre_stru_p)
{
	pr_info("di_pre_stru:\n");
	pr_info("di_mem_buf_dup_p	   = 0x%p\n",
		di_pre_stru_p->di_mem_buf_dup_p);
	pr_info("di_chan2_buf_dup_p	   = 0x%p\n",
		di_pre_stru_p->di_chan2_buf_dup_p);
	pr_info("in_seq				   = %d\n",
		di_pre_stru_p->in_seq);
	pr_info("recycle_seq		   = %d\n",
		di_pre_stru_p->recycle_seq);
	pr_info("pre_ready_seq		   = %d\n",
		di_pre_stru_p->pre_ready_seq);
	pr_info("pre_de_busy		   = %d\n",
		di_pre_stru_p->pre_de_busy);
	pr_info("pre_de_busy_timer_count= %d\n",
		di_pre_stru_p->pre_de_busy_timer_count);
	pr_info("pre_de_process_done   = %d\n",
		di_pre_stru_p->pre_de_process_done);
	pr_info("pre_de_irq_timeout_count=%d\n",
		di_pre_stru_p->pre_de_irq_timeout_count);
	pr_info("unreg_req_flag		   = %d\n",
		di_pre_stru_p->unreg_req_flag);
	pr_info("unreg_req_flag_irq		   = %d\n",
		di_pre_stru_p->unreg_req_flag_irq);
	pr_info("reg_req_flag		   = %d\n",
		di_pre_stru_p->reg_req_flag);
	pr_info("reg_req_flag_irq		   = %d\n",
		di_pre_stru_p->reg_req_flag_irq);
	pr_info("cur_width			   = %d\n",
		di_pre_stru_p->cur_width);
	pr_info("cur_height			   = %d\n",
		di_pre_stru_p->cur_height);
	pr_info("cur_inp_type		   = 0x%x\n",
		di_pre_stru_p->cur_inp_type);
	pr_info("cur_source_type	   = %d\n",
		di_pre_stru_p->cur_source_type);
	pr_info("cur_prog_flag		   = %d\n",
		di_pre_stru_p->cur_prog_flag);
	pr_info("source_change_flag	   = %d\n",
		di_pre_stru_p->source_change_flag);
	pr_info("prog_proc_type		   = %d\n",
		di_pre_stru_p->prog_proc_type);
	pr_info("enable_mtnwr		   = %d\n",
		di_pre_stru_p->enable_mtnwr);
	pr_info("enable_pulldown_check	= %d\n",
		di_pre_stru_p->enable_pulldown_check);
#ifdef DET3D
	pr_info("vframe_interleave_flag = %d\n",
		di_pre_stru_p->vframe_interleave_flag);
#endif
	pr_info("left_right		   = %d\n",
		di_pre_stru_p->left_right);
	pr_info("force_interlace   = %s\n",
		di_pre_stru_p->force_interlace ? "true" : "false");
	pr_info("vdin2nr		   = %d\n",
		di_pre_stru_p->vdin2nr);
	pr_info("bypass_pre		   = %s\n",
		di_pre_stru_p->bypass_pre ? "true" : "false");
	pr_info("invert_flag	   = %s\n",
		di_pre_stru_p->invert_flag ? "true" : "false");
}

void dump_di_post_stru(struct di_post_stru_s *di_post_stru_p)
{
	di_pr_info("\ndi_post_stru:\n");
	di_pr_info("run_early_proc_fun_flag	= %d\n",
		di_post_stru_p->run_early_proc_fun_flag);
	di_pr_info("cur_disp_index = %d\n",
		di_post_stru_p->cur_disp_index);
	di_pr_info("post_de_busy			= %d\n",
		di_post_stru_p->post_de_busy);
	di_pr_info("de_post_process_done	= %d\n",
		di_post_stru_p->de_post_process_done);
	di_pr_info("cur_post_buf			= 0x%p\n,",
		di_post_stru_p->cur_post_buf);
}

void dump_di_buf(struct di_buf_s *di_buf)
{
	pr_info("di_buf %p vframe %p:\n", di_buf, di_buf->vframe);
	pr_info("index %d, post_proc_flag %d, new_format_flag %d, type %d,",
		di_buf->index, di_buf->post_proc_flag,
		di_buf->new_format_flag, di_buf->type);
	pr_info("seq %d, pre_ref_count %d,post_ref_count %d, queue_index %d,",
		di_buf->seq, di_buf->pre_ref_count, di_buf->post_ref_count,
		di_buf->queue_index);
	pr_info("pulldown_mode %d process_fun_index %d\n",
		di_buf->pd_config.global_mode, di_buf->process_fun_index);
	pr_info("di_buf: %p, %p, di_buf_dup_p: %p, %p, %p, %p, %p\n",
		di_buf->di_buf[0], di_buf->di_buf[1], di_buf->di_buf_dup_p[0],
		di_buf->di_buf_dup_p[1], di_buf->di_buf_dup_p[2],
		di_buf->di_buf_dup_p[3], di_buf->di_buf_dup_p[4]);
	pr_info(
	"nr_adr 0x%lx, nr_canvas_idx 0x%x, mtn_adr 0x%lx, mtn_canvas_idx 0x%x",
		di_buf->nr_adr, di_buf->nr_canvas_idx, di_buf->mtn_adr,
		di_buf->mtn_canvas_idx);
	pr_info("cnt_adr 0x%lx, cnt_canvas_idx 0x%x\n",
		di_buf->cnt_adr, di_buf->cnt_canvas_idx);
	pr_info("di_cnt %d, priveated %u.\n",
			atomic_read(&di_buf->di_cnt), di_buf->privated);
}

void dump_pool(struct queue_s *q)
{
	int j;

	pr_info("queue: in_idx %d, out_idx %d, num %d, type %d\n",
		q->in_idx, q->out_idx, q->num, q->type);
	for (j = 0; j < MAX_QUEUE_POOL_SIZE; j++) {
		pr_info("0x%x ", q->pool[j]);
		if (((j + 1) % 16) == 0)
			pr_debug("\n");
	}
	pr_info("\n");
}

void dump_vframe(struct vframe_s *vf)
{
	pr_info("vframe %p:\n", vf);
	pr_info("index %d, type 0x%x, type_backup 0x%x, blend_mode %d bitdepth %d\n",
		vf->index, vf->type, vf->type_backup,
		vf->blend_mode, (vf->bitdepth&BITDEPTH_Y10)?10:8);
	pr_info("duration %d, duration_pulldown %d, pts %d, flag 0x%x\n",
		vf->duration, vf->duration_pulldown, vf->pts, vf->flag);
	pr_info("canvas0Addr 0x%x, canvas1Addr 0x%x, bufWidth %d\n",
		vf->canvas0Addr, vf->canvas1Addr, vf->bufWidth);
	pr_info("width %d, height %d, ratio_control 0x%x, orientation 0x%x\n",
		vf->width, vf->height, vf->ratio_control, vf->orientation);
	pr_info("source_type %d, phase %d, soruce_mode %d, sig_fmt %d\n",
		vf->source_type, vf->phase, vf->source_mode, vf->sig_fmt);
	pr_info(
		"trans_fmt 0x%x, lefteye(%d %d %d %d), righteye(%d %d %d %d)\n",
		vf->trans_fmt, vf->left_eye.start_x, vf->left_eye.start_y,
		vf->left_eye.width, vf->left_eye.height,
		vf->right_eye.start_x, vf->right_eye.start_y,
		vf->right_eye.width, vf->right_eye.height);
	pr_info("mode_3d_enable %d, use_cnt %d,",
		vf->mode_3d_enable, atomic_read(&vf->use_cnt));
	pr_info("early_process_fun 0x%p, process_fun 0x%p, private_data %p\n",
		vf->early_process_fun,
		vf->process_fun, vf->private_data);
	pr_info("pixel_ratio %d list %p\n",
		vf->pixel_ratio, &vf->list);
}

void print_di_buf(struct di_buf_s *di_buf, int format)
{
	if (!di_buf)
		return;
	if (format == 1) {
		pr_info(
		"\t+index %d, 0x%p, type %d, vframetype 0x%x, trans_fmt %u,bitdepath %d\n",
			di_buf->index,
			di_buf,
			di_buf->type,
			di_buf->vframe->type,
			di_buf->vframe->trans_fmt,
			di_buf->vframe->bitdepth);
		if (di_buf->di_wr_linked_buf) {
			pr_info("\tlinked  +index %d, 0x%p, type %d\n",
				di_buf->di_wr_linked_buf->index,
				di_buf->di_wr_linked_buf,
				di_buf->di_wr_linked_buf->type);
		}
	} else if (format == 2) {
		pr_info("index %d, 0x%p(vframe 0x%p), type %d\n",
			di_buf->index, di_buf,
			di_buf->vframe, di_buf->type);
		pr_info("vframetype 0x%x, trans_fmt %u,duration %d pts %d,bitdepth %d\n",
			di_buf->vframe->type,
			di_buf->vframe->trans_fmt,
			di_buf->vframe->duration,
			di_buf->vframe->pts,
			di_buf->vframe->bitdepth);
		if (di_buf->di_wr_linked_buf) {
			pr_info("linked index %d, 0x%p, type %d\n",
				di_buf->di_wr_linked_buf->index,
				di_buf->di_wr_linked_buf,
				di_buf->di_wr_linked_buf->type);
		}
	}
}
void dump_pre_mif_state(void)
{
	unsigned int i = 0;

	Wr_reg_bits(DI_INP_GEN_REG3, 3, 10, 2);
	Wr_reg_bits(DI_MEM_GEN_REG3, 3, 10, 2);
	Wr_reg_bits(DI_CHAN2_GEN_REG3, 3, 10, 2);
	pr_info("DI_INP_GEN_REG2=0x%x.\n", Rd(DI_INP_GEN_REG2));
	pr_info("DI_INP_GEN_REG3=0x%x.\n", Rd(DI_INP_GEN_REG3));
	pr_info("DI_INP_LUMA_FIFO_SIZE=0x%x.\n", Rd(DI_INP_LUMA_FIFO_SIZE));
	pr_info("DI_INP_RANGE_MAP_Y=0x%x.\n", Rd(DI_INP_RANGE_MAP_Y));
	pr_info("DI_INP_RANGE_MAP_CB=0x%x.\n", Rd(DI_INP_RANGE_MAP_CB));
	pr_info("DI_INP_RANGE_MAP_CR=0x%x.\n", Rd(DI_INP_RANGE_MAP_CR));
	pr_info("DI_INP_URGENT_CTRL=0x%x.\n", Rd(DI_INP_URGENT_CTRL));
	for (i = 0; i < 10; i++)
		pr_info("0x%x=0x%x.\n", 0x17ce + i, Rd(0x17ce + i));
	pr_info("DI_MEM_GEN_REG2=0x%x.\n", Rd(DI_MEM_GEN_REG2));
	pr_info("DI_MEM_GEN_REG3=0x%x.\n", Rd(DI_MEM_GEN_REG3));
	pr_info("DI_MEM_LUMA_FIFO_SIZE=0x%x.\n", Rd(DI_MEM_LUMA_FIFO_SIZE));
	pr_info("DI_MEM_RANGE_MAP_CB=0x%x.\n", Rd(DI_MEM_RANGE_MAP_CB));
	pr_info("DI_MEM_RANGE_MAP_CR=0x%x.\n", Rd(DI_MEM_RANGE_MAP_CR));
	pr_info("DI_MEM_URGENT_CTRL=0x%x.\n", Rd(DI_MEM_URGENT_CTRL));
	for (i = 0; i < 10; i++)
		pr_info("0x%x=0x%x.\n", 0x17db + i, Rd(0x17db + i));
	pr_info("DI_CHAN2_GEN_REG2=0x%x.\n", Rd(DI_CHAN2_GEN_REG2));
	pr_info("DI_CHAN2_GEN_REG3=0x%x.\n", Rd(DI_CHAN2_GEN_REG3));
	pr_info("DI_CHAN2_LUMA_FIFO_SIZE=0x%x.\n", Rd(DI_CHAN2_LUMA_FIFO_SIZE));
	pr_info("DI_CHAN2_RANGE_MAP_CB=0x%x.\n", Rd(DI_CHAN2_RANGE_MAP_CB));
	pr_info("DI_CHAN2_RANGE_MAP_CR=0x%x.\n", Rd(DI_CHAN2_RANGE_MAP_CR));
	pr_info("DI_CHAN2_URGENT_CTRL=0x%x.\n", Rd(DI_CHAN2_URGENT_CTRL));
	for (i = 0; i < 10; i++)
		pr_info("0x%x=0x%x.\n", 0x17f5 + i, Rd(0x17f5 + i));
}

void dump_post_mif_reg(void)
{
	pr_info("VIU_MISC_CTRL0=0x%x\n", Rd(VIU_MISC_CTRL0));

	pr_info("VD1_IF0_GEN_REG=0x%x\n", Rd(VD1_IF0_GEN_REG));
	pr_info("VD1_IF0_GEN_REG2=0x%x\n", Rd(VD1_IF0_GEN_REG2));
	pr_info("VD1_IF0_GEN_REG3=0x%x\n", Rd(VD1_IF0_GEN_REG3));
	pr_info("VD1_IF0_LUMA_X0=0x%x\n", Rd(VD1_IF0_LUMA_X0));
	pr_info("VD1_IF0_LUMA_Y0=0x%x\n", Rd(VD1_IF0_LUMA_Y0));
	pr_info("VD1_IF0_CHROMA_X0=0x%x\n", Rd(VD1_IF0_CHROMA_X0));
	pr_info("VD1_IF0_CHROMA_Y0=0x%x\n", Rd(VD1_IF0_CHROMA_Y0));
	pr_info("VD1_IF0_LUMA_X1=0x%x\n", Rd(VD1_IF0_LUMA_X1));
	pr_info("VD1_IF0_LUMA_Y1=0x%x\n", Rd(VD1_IF0_LUMA_Y1));
	pr_info("VD1_IF0_CHROMA_X1=0x%x\n", Rd(VD1_IF0_CHROMA_X1));
	pr_info("VD1_IF0_CHROMA_Y1=0x%x\n", Rd(VD1_IF0_CHROMA_Y1));
	pr_info("VD1_IF0_REPEAT_LOOP=0x%x\n", Rd(VD1_IF0_RPT_LOOP));
	pr_info("VD1_IF0_LUMA0_RPT_PAT=0x%x\n", Rd(VD1_IF0_LUMA0_RPT_PAT));
	pr_info("VD1_IF0_CHROMA0_RPT_PAT=0x%x\n", Rd(VD1_IF0_CHROMA0_RPT_PAT));
	pr_info("VD1_IF0_LUMA_PSEL=0x%x\n", Rd(VD1_IF0_LUMA_PSEL));
	pr_info("VD1_IF0_CHROMA_PSEL=0x%x\n", Rd(VD1_IF0_CHROMA_PSEL));
	pr_info("VIU_VD1_FMT_CTRL=0x%x\n", Rd(VIU_VD1_FMT_CTRL));
	pr_info("VIU_VD1_FMT_W=0x%x\n", Rd(VIU_VD1_FMT_W));

	pr_info("DI_IF1_GEN_REG=0x%x\n", Rd(DI_IF1_GEN_REG));
	pr_info("DI_IF1_GEN_REG2=0x%x\n", Rd(DI_IF1_GEN_REG2));
	pr_info("DI_IF1_GEN_REG3=0x%x\n", Rd(DI_IF1_GEN_REG3));
	pr_info("DI_IF1_CANVAS0=0x%x\n", Rd(DI_IF1_CANVAS0));
	pr_info("DI_IF1_LUMA_X0=0x%x\n", Rd(DI_IF1_LUMA_X0));
	pr_info("DI_IF1_LUMA_Y0=0x%x\n", Rd(DI_IF1_LUMA_Y0));
	pr_info("DI_IF1_CHROMA_X0=0x%x\n", Rd(DI_IF1_CHROMA_X0));
	pr_info("DI_IF1_CHROMA_Y0=0x%x\n", Rd(DI_IF1_CHROMA_Y0));
	pr_info("DI_IF1_LUMA0_RPT_PAT=0x%x\n", Rd(DI_IF1_LUMA0_RPT_PAT));
	pr_info("DI_IF1_CHROMA0_RPT_PAT=0x%x\n", Rd(DI_IF1_LUMA0_RPT_PAT));
	pr_info("DI_IF1_FMT_CTRL=0x%x\n", Rd(DI_IF1_FMT_CTRL));
	pr_info("DI_IF1_FMT_W=0x%x\n", Rd(DI_IF1_FMT_W));

	pr_info("DI_IF2_GEN_REG=0x%x\n", Rd(DI_IF2_GEN_REG));
	pr_info("DI_IF2_GEN_REG2=0x%x\n", Rd(DI_IF2_GEN_REG2));
	pr_info("DI_IF2_GEN_REG3=0x%x\n", Rd(DI_IF2_GEN_REG3));
	pr_info("DI_IF2_CANVAS0=0x%x\n", Rd(DI_IF2_CANVAS0));
	pr_info("DI_IF2_LUMA_X0=0x%x\n", Rd(DI_IF2_LUMA_X0));
	pr_info("DI_IF2_LUMA_Y0=0x%x\n", Rd(DI_IF2_LUMA_Y0));
	pr_info("DI_IF2_CHROMA_X0=0x%x\n", Rd(DI_IF2_CHROMA_X0));
	pr_info("DI_IF2_CHROMA_Y0=0x%x\n", Rd(DI_IF2_CHROMA_Y0));
	pr_info("DI_IF2_LUMA0_RPT_PAT=0x%x\n", Rd(DI_IF2_LUMA0_RPT_PAT));
	pr_info("DI_IF2_CHROMA0_RPT_PAT=0x%x\n", Rd(DI_IF2_LUMA0_RPT_PAT));
	pr_info("DI_IF2_FMT_CTRL=0x%x\n", Rd(DI_IF2_FMT_CTRL));
	pr_info("DI_IF2_FMT_W=0x%x\n", Rd(DI_IF2_FMT_W));

	pr_info("DI_DIWR_Y=0x%x\n", Rd(DI_DIWR_Y));
	pr_info("DI_DIWR_CTRL=0x%x", Rd(DI_DIWR_CTRL));
	pr_info("DI_DIWR_X=0x%x.\n", Rd(DI_DIWR_X));
}

