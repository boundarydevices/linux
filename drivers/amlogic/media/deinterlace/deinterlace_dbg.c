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
	unsigned int i = 0;

		pr_info("----dump di reg----\n");
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
				0xd0100000 + ((0x1700 + i) << 2),
				0x1700 + i, RDMA_RD(0x1700 + i));
		}
		pr_info("----dump mcdi reg----\n");
		for (i = 0; i < 201; i++)
			pr_info("[0x%x][0x%x]=0x%x\n",
				0xd0100000 + ((0x2f00 + i) << 2),
				0x2f00 + i, RDMA_RD(0x2f00 + i));
		pr_info("----dump pulldown reg----\n");
		for (i = 0; i < 26; i++)
			pr_info("[0x%x][0x%x]=0x%x\n",
				0xd0100000 + ((0x2fd0 + i) << 2),
				0x2fd0 + i, RDMA_RD(0x2fd0 + i));
		pr_info("----dump bit mode reg----\n");
		for (i = 0; i < 4; i++)
			pr_info("[0x%x][0x%x]=0x%x\n",
				0xd0100000 + ((0x20a7 + i) << 2),
				0x20a7 + i, RDMA_RD(0x20a7 + i));
		pr_info("[0x%x][0x%x]=0x%x\n",
			0xd0100000 + (0x2022 << 2),
			0x2022, RDMA_RD(0x2022));
		pr_info("[0x%x][0x%x]=0x%x\n",
			0xd0100000 + (0x17c1 << 2),
			0x17c1, RDMA_RD(0x17c1));
		pr_info("[0x%x][0x%x]=0x%x\n",
			0xd0100000 + (0x17c2 << 2),
			0x17c2, RDMA_RD(0x17c2));
		pr_info("[0x%x][0x%x]=0x%x\n",
			0xd0100000 + (0x1aa7 << 2),
			0x1aa7, RDMA_RD(0x1aa7));
		pr_info("----dump dnr reg----\n");
		for (i = 0; i < 29; i++)
			pr_info("[0x%x][0x%x]=0x%x\n",
					0xd0100000 + ((0x2d00 + i) << 2),
					0x2d00 + i, RDMA_RD(0x2d00 + i));
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
	pr_info("same_field_source_flag = %d\n",
		di_pre_stru_p->same_field_source_flag);
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
		di_buf->pulldown_mode, di_buf->process_fun_index);
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

