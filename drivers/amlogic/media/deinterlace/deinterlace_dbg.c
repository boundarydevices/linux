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
#include "register.h"
#include "deinterlace_dbg.h"
#include "di_pps.h"
#include "nr_downscale.h"
#include <linux/amlogic/media/vfm/vframe_provider.h>


/*2018-07-18 add debugfs*/
#include <linux/seq_file.h>
#include <linux/debugfs.h>
/*2018-07-18 -----------*/


void parse_cmd_params(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
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
const unsigned int size_reg_addr1[] = {
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
	0x2013, 0x2014, 0x2015,
	0xffff
};

/*g12 new added*/
const unsigned int size_reg_addr2[] = {
	0x37d2, 0x37d3, 0x37d7,
	0x37d8, 0x37dc, 0x37dd,
	0x37e1, 0x37e2, 0x37e6,
	0x37e7, 0x37e9, 0x37ea,
	0x37ed, 0x37ee, 0x37f1,
	0x37f2, 0x37f4, 0x37f5,
	0x37f6, 0x37f8, 0x3751,
	0x3752, 0x376e, 0x376f,
	0x37f9, 0x37fa, 0x37fc,
	0x3740, 0x3757, 0x3762,
	0xffff
};

/*2018-08-17 add debugfs*/
static int seq_file_dump_di_reg_show(struct seq_file *seq, void *v)
{
	unsigned int i = 0, base_addr = 0;

	if (is_meson_txlx_cpu() || is_meson_txhd_cpu())
		base_addr = 0xff900000;
	else
		base_addr = 0xd0100000;

	seq_puts(seq, "----dump di reg----\n");
	seq_puts(seq, "----dump size reg---\n");
	/*txl crash when dump 0x37d2~0x3762 of size_reg_addr*/
	for (i = 0; size_reg_addr1[i] != 0xffff; i++)
		seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
			base_addr + ((size_reg_addr1[i]) << 2),
			size_reg_addr1[i], RDMA_RD(size_reg_addr1[i]));
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		for (i = 0; size_reg_addr2[i] != 0xffff; i++)
			seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
				base_addr + ((size_reg_addr2[i]) << 2),
				size_reg_addr2[i], RDMA_RD(size_reg_addr2[i]));
	}
	for (i = 0; i < 255; i++) {
		if (i == 0x45)
			seq_puts(seq, "----nr reg----");
		if (i == 0x80)
			seq_puts(seq, "----3d reg----");
		if (i == 0x9e)
			seq_puts(seq, "---nr reg done---");
		if (i == 0x9c)
			seq_puts(seq, "---3d reg done---");
		seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x1700 + i) << 2),
			0x1700 + i, RDMA_RD(0x1700 + i));
	}
	for (i = 0; i < 4; i++) {
		seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x20ab + i) << 2),
			0x20ab + i, RDMA_RD(0x20ab + i));
	}
	seq_puts(seq, "----dump mcdi reg----\n");
	for (i = 0; i < 201; i++)
		seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2f00 + i) << 2),
			0x2f00 + i, RDMA_RD(0x2f00 + i));
	seq_puts(seq, "----dump pulldown reg----\n");
	for (i = 0; i < 26; i++)
		seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2fd0 + i) << 2),
			0x2fd0 + i, RDMA_RD(0x2fd0 + i));
	seq_puts(seq, "----dump bit mode reg----\n");
	for (i = 0; i < 4; i++)
		seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x20a7 + i) << 2),
			0x20a7 + i, RDMA_RD(0x20a7 + i));
	seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
		base_addr + (0x2022 << 2),
		0x2022, RDMA_RD(0x2022));
	seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
		base_addr + (0x17c1 << 2),
		0x17c1, RDMA_RD(0x17c1));
	seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
		base_addr + (0x17c2 << 2),
		0x17c2, RDMA_RD(0x17c2));
	seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
		base_addr + (0x1aa7 << 2),
		0x1aa7, RDMA_RD(0x1aa7));
	seq_puts(seq, "----dump dnr reg----\n");
	for (i = 0; i < 29; i++)
		seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2d00 + i) << 2),
			0x2d00 + i, RDMA_RD(0x2d00 + i));
	seq_puts(seq, "----dump if0 reg----\n");
	for (i = 0; i < 26; i++)
		seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x1a60 + i) << 2),
			0x1a50 + i, RDMA_RD(0x1a50 + i));
	seq_puts(seq, "----dump gate reg----\n");
	seq_printf(seq, "[0x%x][0x1718]=0x%x\n",
			base_addr + ((0x1718) << 2),
			RDMA_RD(0x1718));
	for (i = 0; i < 5; i++)
		seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2006 + i) << 2),
			0x2006 + i, RDMA_RD(0x2006 + i));
	seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
		base_addr + ((0x2dff) << 2),
		0x2dff, RDMA_RD(0x2dff));
	seq_puts(seq, "----dump if2 reg----\n");
	for (i = 0; i < 29; i++)
		seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2010 + i) << 2),
			0x2010 + i, RDMA_RD(0x2010 + i));
	if (!is_meson_txl_cpu()) {
		seq_puts(seq, "----dump nr4 reg----\n");
	for (i = 0x2da4; i < 0x2df6; i++)
		seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
			base_addr + (i << 2),
			i, RDMA_RD(i));
	for (i = 0x3700; i < 0x373f; i++)
		seq_printf(seq, "[0x%x][0x%x]=0x%x\n",
			base_addr + (i << 2),
			i, RDMA_RD(i));
	}
	seq_puts(seq, "----dump reg done----\n");
	return 0;
}

void dump_di_reg_g12(void)
{
	unsigned int i = 0, base_addr = 0;
	unsigned int size_reg_addr[] = {
		0x1700, 0x1701,	0x1702,
		0x1703, 0x37d2, 0x27d3,
		0x37d7, 0x37d8,	0x37dc,
		0x37dd, 0x37e1, 0x37e2,
		0x37e6, 0x37e7, 0x37e9,
		0x37ea, 0x37ec, 0x37ed,
		0x37ee, 0x37f0, 0x37f1,
		0x37f2, 0x37f4, 0x37f5,
		0x37f6, 0x37f8, 0x2032,
		0x2033, 0x2034, 0x2035,
		0x2d01, 0x2d02, 0x2d8f,
		0x2d08, 0x2d09, 0x2f00,
		0x2f01, 0x17d0, 0x17d1,
		0x17d2, 0x17d3, 0x17dd,
		0x17de, 0x17df, 0x17e0,
		0x17f7, 0x17f8, 0x17f9,
		0x17fa, 0x17c0, 0x17c1,
		0x17c6, 0x17c7, 0x3253,
		0x3254, 0x3255, 0x3256,
		0x17ea, 0x17eb, 0x17ec,
		0x17ed, 0x2012, 0x2013,
		0x2014, 0x2015, 0x37d2,
		0x37d3, 0x37d7, 0x37d8,
		0x37dc, 0x37dd, 0x37e1,
		0x37e2, 0x37e6, 0x37e7,
		0x37e9, 0x37ea, 0x37ed,
		0x37ee, 0x37f1, 0x37f2,
		0x37f4, 0x37f5,	0x37f6,
		0x37f8, 0x3751,	0x3752,
		0x376e, 0x376f,	0x37f9,
		0x37fa, 0x37fc, 0x3740,
		0x3757, 0x3762, 0x3755,
		0x3757, 0x3760, 0x3762,
		0x376e, 0x376f, 0x37f9,
		0x37fa, 0x37fc,	0xffff
	};
	if (is_meson_txlx_cpu() ||
		is_meson_txhd_cpu() ||
		is_meson_g12a_cpu() ||
		is_meson_g12b_cpu() ||
		is_meson_tl1_cpu() ||
		is_meson_sm1_cpu())
		base_addr = 0xff900000;
	else
		base_addr = 0xd0100000;

	pr_info("----dump di reg----\n");
	pr_info("----dump size reg---");
	for (i = 0; size_reg_addr[i] != 0xffff; i++)
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
	dump_pps_reg(base_addr);
	dump_nrds_reg(base_addr);
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
			base_addr + ((0x3200 + i) << 2),
			0x3200 + i, RDMA_RD(0x3200 + i));
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
	pr_info("[0x%x][0x%x]=0x%x\n",
		base_addr + ((0x2fff) << 2),
		0x2fff, RDMA_RD(0x2fff));
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
	pr_info("----dump arb reg----\n");
	for (i = 0; i < 14; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x3750 + i) << 2),
			0x3750 + i, RDMA_RD(0x3750 + i));
	for (i = 0; i < 14; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x37c0 + i) << 2),
			0x37c0 + i, RDMA_RD(0x37c0 + i));

	pr_info("----dump pps reg----\n");
	for (i = 0; i < 32; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x374e + i) << 2),
			0x374e + i, RDMA_RD(0x374e + i));

	pr_info("----dump di hdr reg----\n");
	for (i = 0; i < 62; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x376e + i) << 2),
			0x376e + i, RDMA_RD(0x376e + i));
	pr_info("----dump reg done----\n");
}

static void dump_mif_state(struct DI_MIF_s *mif)
{
	pr_info("luma <%u, %u> <%u %u>.\n",
		mif->luma_x_start0, mif->luma_x_end0,
		mif->luma_y_start0, mif->luma_y_end0);
	pr_info("chroma <%u, %u> <%u %u>.\n",
		mif->chroma_x_start0, mif->chroma_x_end0,
		mif->chroma_y_start0, mif->chroma_y_end0);
	pr_info("canvas id <%u %u %u>.\n",
		mif->canvas0_addr0,
		mif->canvas0_addr1,
		mif->canvas0_addr2);
}

/*2018-08-17 add debugfs*/
/*same as dump_mif_state*/
static void dump_mif_state_seq(struct DI_MIF_s *mif,
		struct seq_file *seq)
{
	seq_printf(seq, "luma <%u, %u> <%u %u>.\n",
		mif->luma_x_start0, mif->luma_x_end0,
		mif->luma_y_start0, mif->luma_y_end0);
	seq_printf(seq, "chroma <%u, %u> <%u %u>.\n",
		mif->chroma_x_start0, mif->chroma_x_end0,
		mif->chroma_y_start0, mif->chroma_y_end0);
	seq_printf(seq, "canvas id <%u %u %u>.\n",
		mif->canvas0_addr0,
		mif->canvas0_addr1,
		mif->canvas0_addr2);
}

static void dump_simple_mif_state(struct DI_SIM_MIF_s *simp_mif)
{
	pr_info("<%u %u> <%u %u>.\n",
		simp_mif->start_x, simp_mif->end_x,
		simp_mif->start_y, simp_mif->end_y);
	pr_info("canvas num <%u>.\n",
		simp_mif->canvas_num);
}

/*2018-08-17 add debugfs*/
/*same as dump_simple_mif_state*/
static void dump_simple_mif_state_seq(struct DI_SIM_MIF_s *simp_mif,
		struct seq_file *seq)
{
	seq_printf(seq, "<%u %u> <%u %u>.\n",
		simp_mif->start_x, simp_mif->end_x,
		simp_mif->start_y, simp_mif->end_y);
	seq_printf(seq, "canvas num <%u>.\n",
		simp_mif->canvas_num);
}

static void dump_mc_mif_state(struct DI_MC_MIF_s *mc_mif)
{
	pr_info("startx %u,<%u %u>, size <%u %u>.\n",
		mc_mif->start_x, mc_mif->start_y,
		mc_mif->end_y, mc_mif->size_x,
		mc_mif->size_y);
}

/*2018-08-17 add debugfs*/
/*same as dump_mc_mif_state*/
static void dump_mc_mif_state_seq(struct DI_MC_MIF_s *mc_mif,
		struct seq_file *seq)
{
	seq_printf(seq, "startx %u,<%u %u>, size <%u %u>.\n",
		mc_mif->start_x, mc_mif->start_y,
		mc_mif->end_y, mc_mif->size_x,
		mc_mif->size_y);
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
	pr_info("pre_de_process_flag   = %d\n",
		di_pre_stru_p->pre_de_process_flag);
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
	pr_info("bypass_flag = %s\n",
		di_pre_stru_p->bypass_flag?"true":"false");
	pr_info("prog_proc_type		   = %d\n",
		di_pre_stru_p->prog_proc_type);
	pr_info("madi_enable		   = %u\n",
		di_pre_stru_p->madi_enable);
	pr_info("mcdi_enable	= %u\n",
		di_pre_stru_p->mcdi_enable);
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

/*2018-08-17 add debugfs*/
/*same as dump_di_pre_stru*/
static int dump_di_pre_stru_seq(struct seq_file *seq, void *v)

{
	struct di_pre_stru_s *di_pre_stru_p = get_di_pre_stru();

	seq_puts(seq, "di_pre_stru:\n");
	seq_printf(seq, "%-25s = 0x%p\n", "di_mem_buf_dup_p",
		di_pre_stru_p->di_mem_buf_dup_p);
	seq_printf(seq, "%-25s = 0x%p\n", "di_chan2_buf_dup_p",
		di_pre_stru_p->di_chan2_buf_dup_p);
	seq_printf(seq, "%-25s = %d\n", "in_seq",
		di_pre_stru_p->in_seq);
	seq_printf(seq, "%-25s = %d\n", "recycle_seq",
		di_pre_stru_p->recycle_seq);
	seq_printf(seq, "%-25s = %d\n", "pre_ready_seq",
		di_pre_stru_p->pre_ready_seq);
	seq_printf(seq, "%-25s = %d\n", "pre_de_busy",
		di_pre_stru_p->pre_de_busy);
	seq_printf(seq, "%-25s = %d\n", "pre_de_busy_timer_count",
		di_pre_stru_p->pre_de_busy_timer_count);
	seq_printf(seq, "%-25s = %d\n", "pre_de_process_done",
		di_pre_stru_p->pre_de_process_done);
	seq_printf(seq, "%-25s = %d\n", "pre_de_process_flag",
		di_pre_stru_p->pre_de_process_flag);
	seq_printf(seq, "%-25s =%d\n", "pre_de_irq_timeout_count",
		di_pre_stru_p->pre_de_irq_timeout_count);
	seq_printf(seq, "%-25s = %d\n", "unreg_req_flag",
		di_pre_stru_p->unreg_req_flag);
	seq_printf(seq, "%-25s = %d\n", "unreg_req_flag_irq",
		di_pre_stru_p->unreg_req_flag_irq);
	seq_printf(seq, "%-25s = %d\n", "reg_req_flag",
		di_pre_stru_p->reg_req_flag);
	seq_printf(seq, "%-25s = %d\n", "reg_req_flag_irq",
		di_pre_stru_p->reg_req_flag_irq);
	seq_printf(seq, "%-25s = %d\n", "cur_width",
		di_pre_stru_p->cur_width);
	seq_printf(seq, "%-25s = %d\n", "cur_height",
		di_pre_stru_p->cur_height);
	seq_printf(seq, "%-25s = 0x%x\n", "cur_inp_type",
		di_pre_stru_p->cur_inp_type);
	seq_printf(seq, "%-25s = %d\n", "cur_source_type",
		di_pre_stru_p->cur_source_type);
	seq_printf(seq, "%-25s = %d\n", "cur_prog_flag",
		di_pre_stru_p->cur_prog_flag);
	seq_printf(seq, "%-25s = %d\n", "source_change_flag",
		di_pre_stru_p->source_change_flag);
	seq_printf(seq, "%-25s = %s\n", "bypass_flag",
		di_pre_stru_p->bypass_flag?"true":"false");
	seq_printf(seq, "%-25s = %d\n", "prog_proc_type",
		di_pre_stru_p->prog_proc_type);
	seq_printf(seq, "%-25s = %d\n", "madi_enable",
		di_pre_stru_p->madi_enable);
	seq_printf(seq, "%-25s = %d\n", "mcdi_enable",
		di_pre_stru_p->mcdi_enable);
#ifdef DET3D
	seq_printf(seq, "%-25s = %d\n", "vframe_interleave_flag",
		di_pre_stru_p->vframe_interleave_flag);
#endif
	seq_printf(seq, "%-25s = %d\n", "left_right",
		di_pre_stru_p->left_right);
	seq_printf(seq, "%-25s = %s\n", "force_interlace",
		di_pre_stru_p->force_interlace ? "true" : "false");
	seq_printf(seq, "%-25s = %d\n", "vdin2nr",
		di_pre_stru_p->vdin2nr);
	seq_printf(seq, "%-25s = %s\n", "bypass_pre",
		di_pre_stru_p->bypass_pre ? "true" : "false");
	seq_printf(seq, "%-25s = %s\n", "invert_flag",
		di_pre_stru_p->invert_flag ? "true" : "false");

	return 0;
}

void dump_di_post_stru(struct di_post_stru_s *di_post_stru_p)
{
	pr_info("\ndi_post_stru:\n");
	pr_info("run_early_proc_fun_flag	= %d\n",
		di_post_stru_p->run_early_proc_fun_flag);
	pr_info("cur_disp_index = %d\n",
		di_post_stru_p->cur_disp_index);
	pr_info("post_de_busy			= %d\n",
		di_post_stru_p->post_de_busy);
	pr_info("de_post_process_done	= %d\n",
		di_post_stru_p->de_post_process_done);
	pr_info("cur_post_buf			= 0x%p\n",
		di_post_stru_p->cur_post_buf);
	pr_info("post_peek_underflow	= %u\n",
		di_post_stru_p->post_peek_underflow);
}

/*2018-08-17 add debugfs*/
/*same as dump_di_post_stru*/
static int dump_di_post_stru_seq(struct seq_file *seq, void *v)
{
	struct di_post_stru_s *di_post_stru_p = get_di_post_stru();

	seq_puts(seq, "di_post_stru:\n");
	seq_printf(seq, "run_early_proc_fun_flag	= %d\n",
		di_post_stru_p->run_early_proc_fun_flag);
	seq_printf(seq, "cur_disp_index = %d\n",
		di_post_stru_p->cur_disp_index);
	seq_printf(seq, "post_de_busy			= %d\n",
		di_post_stru_p->post_de_busy);
	seq_printf(seq, "de_post_process_done	= %d\n",
		di_post_stru_p->de_post_process_done);
	seq_printf(seq, "cur_post_buf			= 0x%p\n",
		di_post_stru_p->cur_post_buf);
	seq_printf(seq, "post_peek_underflow	= %u\n",
		di_post_stru_p->post_peek_underflow);

	return 0;
}

void dump_mif_size_state(struct di_pre_stru_s *pre_stru_p,
	struct di_post_stru_s *post_stru_p)
{
	pr_info("======pre mif status======\n");
	pr_info("DI_PRE_CTRL=0x%x\n", Rd(DI_PRE_CTRL));
	pr_info("DI_PRE_SIZE=0x%x\n", Rd(DI_PRE_SIZE));
	pr_info("DNR_HVSIZE=0x%x\n", Rd(DNR_HVSIZE));
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		pr_info("CONTWR_CAN_SIZE=0x%x\n", Rd(0x37ec));
		pr_info("MTNWR_CAN_SIZE=0x%x\n", Rd(0x37f0));
	}
	pr_info("DNR_STAT_X_START_END=0x%x\n", Rd(0x2d08));
	pr_info("DNR_STAT_Y_START_END=0x%x\n", Rd(0x2d09));
	pr_info("MCDI_HV_SIZEIN=0x%x\n", Rd(0x2f00));
	pr_info("MCDI_HV_BLKSIZEIN=0x%x\n", Rd(0x2f01));
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		pr_info("MCVECWR_CAN_SIZE=0x%x\n", Rd(0x37f4));
		pr_info("MCINFWR_CAN_SIZE=0x%x\n", Rd(0x37f8));
		pr_info("NRDSWR_CAN_SIZE=0x%x\n", Rd(0x37fc));
		pr_info("NR_DS_BUF_SIZE=0x%x\n", Rd(0x3740));
	}


	pr_info("=====inp mif:\n");
#if 0
	Wr(DI_DBG_CTRL, 0x1b);
	Wr(DI_DBG_CTRL1, 0x640064);
	Wr_reg_bits(DI_PRE_GL_CTRL, 0, 31, 1);
	Wr_reg_bits(DI_PRE_CTRL, 0, 11, 1);
	Wr_reg_bits(DI_PRE_CTRL, 1, 31, 1);
	Wr_reg_bits(DI_PRE_GL_CTRL, 1, 31, 1);
	pr_info("DI_DBG_SRDY_INF=0x%x\n", Rd(DI_DBG_SRDY_INF));
	pr_info("DI_DBG_RRDY_INF=0x%x\n", Rd(DI_DBG_RRDY_INF));
#endif
	pr_info("DI_INP_GEN_REG=0x%x\n", Rd(DI_INP_GEN_REG));
	dump_mif_state(&pre_stru_p->di_inp_mif);
	pr_info("=====mem mif:\n");
	pr_info("DI_MEM_GEN_REG=0x%x\n", Rd(DI_MEM_GEN_REG));
	dump_mif_state(&pre_stru_p->di_mem_mif);
	pr_info("=====chan2 mif:\n");
	pr_info("DI_CHAN2_GEN_REG=0x%x\n", Rd(DI_CHAN2_GEN_REG));
	dump_mif_state(&pre_stru_p->di_chan2_mif);
	pr_info("=====nrwr mif:\n");
	pr_info("DI_NRWR_CTRL=0x%x\n", Rd(DI_NRWR_CTRL));
	dump_simple_mif_state(&pre_stru_p->di_nrwr_mif);
	pr_info("=====mtnwr mif:\n");
	dump_simple_mif_state(&pre_stru_p->di_mtnwr_mif);
	pr_info("=====contp2rd mif:\n");
	dump_simple_mif_state(&pre_stru_p->di_contp2rd_mif);
	pr_info("=====contprd mif:\n");
	dump_simple_mif_state(&pre_stru_p->di_contprd_mif);
	pr_info("=====contwr mif:\n");
	dump_simple_mif_state(&pre_stru_p->di_contwr_mif);
	pr_info("=====mcinford mif:\n");
	dump_mc_mif_state(&pre_stru_p->di_mcinford_mif);
	pr_info("=====mcinfowr mif:\n");
	dump_mc_mif_state(&pre_stru_p->di_mcinfowr_mif);
	pr_info("=====mcvecwr mif:\n");
	dump_mc_mif_state(&pre_stru_p->di_mcvecwr_mif);
	pr_info("======post mif status======\n");
	pr_info("DI_POST_SIZE=0x%x\n", Rd(DI_POST_SIZE));
	pr_info("DECOMB_FRM_SIZE=0x%x\n", Rd(0x2d8f));
	pr_info("=====if0 mif:\n");
	pr_info("DI_IF0_GEN_REG=0x%x\n", Rd(0x2030));
	dump_mif_state(&post_stru_p->di_buf0_mif);
	pr_info("=====if1 mif:\n");
	pr_info("DI_IF1_GEN_REG=0x%x\n", Rd(0x17e8));
	dump_mif_state(&post_stru_p->di_buf1_mif);
	pr_info("=====if2 mif:\n");
	pr_info("DI_IF2_GEN_REG=0x%x\n", Rd(0x2010));
	dump_mif_state(&post_stru_p->di_buf2_mif);
	pr_info("=====diwr mif:\n");
	dump_simple_mif_state(&post_stru_p->di_diwr_mif);
	pr_info("=====mtnprd mif:\n");
	dump_simple_mif_state(&post_stru_p->di_mtnprd_mif);
	pr_info("=====mcvecrd mif:\n");
	dump_mc_mif_state(&post_stru_p->di_mcvecrd_mif);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		pr_info("======pps size status======\n");
		pr_info("DI_SC_LINE_IN_LENGTH=0x%x\n", Rd(0x3751));
		pr_info("DI_SC_PIC_IN_HEIGHT=0x%x\n", Rd(0x3752));
		pr_info("DI_HDR_IN_HSIZE=0x%x\n", Rd(0x376e));
		pr_info("DI_HDR_IN_VSIZE=0x%x\n", Rd(0x376f));
	}
}

/*2018-08-17 add debugfs*/
/*same as dump_mif_size_state*/
static int seq_file_dump_mif_size_state_show(struct seq_file *seq, void *v)
{
	struct di_pre_stru_s *di_pre_stru_p = get_di_pre_stru();
	struct di_post_stru_s *di_post_stru_p = get_di_post_stru();

	seq_puts(seq, "======pre mif status======\n");
	seq_printf(seq, "DI_PRE_CTRL=0x%x\n", Rd(DI_PRE_CTRL));
	seq_printf(seq, "DI_PRE_SIZE=0x%x\n", Rd(DI_PRE_SIZE));
	seq_printf(seq, "DNR_HVSIZE=0x%x\n", Rd(DNR_HVSIZE));
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		seq_printf(seq, "CONTWR_CAN_SIZE=0x%x\n", Rd(0x37ec));
		seq_printf(seq, "MTNWR_CAN_SIZE=0x%x\n", Rd(0x37f0));
	}
	seq_printf(seq, "DNR_STAT_X_START_END=0x%x\n", Rd(0x2d08));
	seq_printf(seq, "DNR_STAT_Y_START_END=0x%x\n", Rd(0x2d09));
	seq_printf(seq, "MCDI_HV_SIZEIN=0x%x\n", Rd(0x2f00));
	seq_printf(seq, "MCDI_HV_BLKSIZEIN=0x%x\n", Rd(0x2f01));
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		seq_printf(seq, "MCVECWR_CAN_SIZE=0x%x\n", Rd(0x37f4));
		seq_printf(seq, "MCINFWR_CAN_SIZE=0x%x\n", Rd(0x37f8));
		seq_printf(seq, "NRDSWR_CAN_SIZE=0x%x\n", Rd(0x37fc));
		seq_printf(seq, "NR_DS_BUF_SIZE=0x%x\n", Rd(0x3740));
	}

	seq_puts(seq, "=====inp mif:\n");
#if 0
	Wr(DI_DBG_CTRL, 0x1b);
	Wr(DI_DBG_CTRL1, 0x640064);
	Wr_reg_bits(DI_PRE_GL_CTRL, 0, 31, 1);
	Wr_reg_bits(DI_PRE_CTRL, 0, 11, 1);
	Wr_reg_bits(DI_PRE_CTRL, 1, 31, 1);
	Wr_reg_bits(DI_PRE_GL_CTRL, 1, 31, 1);
	pr_info("DI_DBG_SRDY_INF=0x%x\n", Rd(DI_DBG_SRDY_INF));
	pr_info("DI_DBG_RRDY_INF=0x%x\n", Rd(DI_DBG_RRDY_INF));
#endif
	seq_printf(seq, "DI_INP_GEN_REG=0x%x\n", Rd(DI_INP_GEN_REG));
	dump_mif_state_seq(&di_pre_stru_p->di_inp_mif, seq);/*dump_mif_state*/
	seq_puts(seq, "=====mem mif:\n");
	seq_printf(seq, "DI_MEM_GEN_REG=0x%x\n", Rd(DI_MEM_GEN_REG));
	dump_mif_state_seq(&di_pre_stru_p->di_mem_mif, seq);
	seq_puts(seq, "=====chan2 mif:\n");
	seq_printf(seq, "DI_CHAN2_GEN_REG=0x%x\n", Rd(DI_CHAN2_GEN_REG));
	dump_mif_state_seq(&di_pre_stru_p->di_chan2_mif, seq);
	seq_puts(seq, "=====nrwr mif:\n");
	seq_printf(seq, "DI_NRWR_CTRL=0x%x\n", Rd(DI_NRWR_CTRL));
	/*dump_simple_mif_state*/
	dump_simple_mif_state_seq(&di_pre_stru_p->di_nrwr_mif, seq);
	seq_puts(seq, "=====mtnwr mif:\n");
	dump_simple_mif_state_seq(&di_pre_stru_p->di_mtnwr_mif, seq);
	seq_puts(seq, "=====contp2rd mif:\n");
	dump_simple_mif_state_seq(&di_pre_stru_p->di_contp2rd_mif, seq);
	seq_puts(seq, "=====contprd mif:\n");
	dump_simple_mif_state_seq(&di_pre_stru_p->di_contprd_mif, seq);
	seq_puts(seq, "=====contwr mif:\n");
	dump_simple_mif_state_seq(&di_pre_stru_p->di_contwr_mif, seq);
	seq_puts(seq, "=====mcinford mif:\n");
	/*dump_mc_mif_state*/
	dump_mc_mif_state_seq(&di_pre_stru_p->di_mcinford_mif,
		seq);
	seq_puts(seq, "=====mcinfowr mif:\n");
	dump_mc_mif_state_seq(&di_pre_stru_p->di_mcinfowr_mif, seq);
	seq_puts(seq, "=====mcvecwr mif:\n");
	dump_mc_mif_state_seq(&di_pre_stru_p->di_mcvecwr_mif, seq);
	seq_puts(seq, "======post mif status======\n");
	seq_printf(seq, "DI_POST_SIZE=0x%x\n", Rd(DI_POST_SIZE));
	seq_printf(seq, "DECOMB_FRM_SIZE=0x%x\n", Rd(0x2d8f));
	seq_puts(seq, "=====if0 mif:\n");
	seq_printf(seq, "DI_IF0_GEN_REG=0x%x\n", Rd(0x2030));
	dump_mif_state_seq(&di_post_stru_p->di_buf0_mif, seq);
	seq_puts(seq, "=====if1 mif:\n");
	seq_printf(seq, "DI_IF1_GEN_REG=0x%x\n", Rd(0x17e8));
	dump_mif_state_seq(&di_post_stru_p->di_buf1_mif, seq);
	seq_puts(seq, "=====if2 mif:\n");
	seq_printf(seq, "DI_IF2_GEN_REG=0x%x\n", Rd(0x2010));
	dump_mif_state_seq(&di_post_stru_p->di_buf2_mif, seq);
	seq_puts(seq, "=====diwr mif:\n");
	dump_simple_mif_state_seq(&di_post_stru_p->di_diwr_mif, seq);
	seq_puts(seq, "=====mtnprd mif:\n");
	dump_simple_mif_state_seq(&di_post_stru_p->di_mtnprd_mif, seq);
	seq_puts(seq, "=====mcvecrd mif:\n");
	dump_mc_mif_state_seq(&di_post_stru_p->di_mcvecrd_mif, seq);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		seq_puts(seq, "======pps size status======\n");
		seq_printf(seq, "DI_SC_LINE_IN_LENGTH=0x%x\n", Rd(0x3751));
		seq_printf(seq, "DI_SC_PIC_IN_HEIGHT=0x%x\n", Rd(0x3752));
		seq_printf(seq, "DI_HDR_IN_HSIZE=0x%x\n", Rd(0x376e));
		seq_printf(seq, "DI_HDR_IN_VSIZE=0x%x\n", Rd(0x376f));
	}
	return 0;
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

/*2018-08-17 add debugfs*/
/*same as print_di_buf*/
static void print_di_buf_seq(struct di_buf_s *di_buf, int format,
					struct seq_file *seq)
{
	if (!di_buf)
		return;
	if (format == 1) {
		seq_printf(seq,
		"\t+index %d, 0x%p, type %d, vframetype 0x%x, trans_fmt %u,bitdepath %d\n",
			di_buf->index,
			di_buf,
			di_buf->type,
			di_buf->vframe->type,
			di_buf->vframe->trans_fmt,
			di_buf->vframe->bitdepth);
		if (di_buf->di_wr_linked_buf) {
			seq_printf(seq, "\tlinked  +index %d, 0x%p, type %d\n",
				di_buf->di_wr_linked_buf->index,
				di_buf->di_wr_linked_buf,
				di_buf->di_wr_linked_buf->type);
		}
	} else if (format == 2) {
		seq_printf(seq, "index %d, 0x%p(vframe 0x%p), type %d\n",
			di_buf->index, di_buf,
			di_buf->vframe, di_buf->type);
		seq_printf(seq, "vframetype 0x%x, trans_fmt %u,duration %d pts %d,bitdepth %d\n",
			di_buf->vframe->type,
			di_buf->vframe->trans_fmt,
			di_buf->vframe->duration,
			di_buf->vframe->pts,
			di_buf->vframe->bitdepth);
		if (di_buf->di_wr_linked_buf) {
			seq_printf(seq, "linked index %d, 0x%p, type %d\n",
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
	for (i = 0; i < 10; i++)
		pr_info("0x%x=0x%x.\n", 0x17ce + i, Rd(0x17ce + i));
	pr_info("DI_MEM_GEN_REG2=0x%x.\n", Rd(DI_MEM_GEN_REG2));
	pr_info("DI_MEM_GEN_REG3=0x%x.\n", Rd(DI_MEM_GEN_REG3));
	pr_info("DI_MEM_LUMA_FIFO_SIZE=0x%x.\n", Rd(DI_MEM_LUMA_FIFO_SIZE));
	for (i = 0; i < 10; i++)
		pr_info("0x%x=0x%x.\n", 0x17db + i, Rd(0x17db + i));
	pr_info("DI_CHAN2_GEN_REG2=0x%x.\n", Rd(DI_CHAN2_GEN_REG2));
	pr_info("DI_CHAN2_GEN_REG3=0x%x.\n", Rd(DI_CHAN2_GEN_REG3));
	pr_info("DI_CHAN2_LUMA_FIFO_SIZE=0x%x.\n", Rd(DI_CHAN2_LUMA_FIFO_SIZE));
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

void dump_buf_addr(struct di_buf_s *di_buf, unsigned int num)
{
	unsigned int i = 0;
	struct di_buf_s *di_buf_p = NULL;

	for (i = 0; i < num; i++) {
		di_buf_p = (di_buf+i);
		pr_info("di_buf[%d] nr_addr 0x%lx,",
			di_buf_p->index, di_buf_p->nr_adr);
		pr_info("mtn_addr 0x%lx, cnt_adr 0x%lx,",
			di_buf_p->mtn_adr, di_buf_p->cnt_adr);
		pr_info("mv_adr 0x%lx, mcinfo_adr 0x%lx.\n",
			di_buf_p->mcvec_adr, di_buf_p->mcinfo_adr);
	}
}

/*2018-08-17 add debugfs*/
/*same as dump_state*/
static int seq_file_di_state_show(struct seq_file *seq, void *v)
{
	int itmp, i;
	struct di_buf_s *p = NULL, *keep_buf;/* ptmp; */
	struct di_pre_stru_s *di_pre_stru_p = get_di_pre_stru();
	struct di_post_stru_s *di_post_stru_p = get_di_post_stru();
	struct di_dev_s *de_devp = get_di_de_devp();

	const char *version_s = get_di_version_s();
	int dump_state_flag = get_di_dump_state_flag();
	uint init_flag = get_di_init_flag();
	unsigned char recovery_flag = get_di_recovery_flag();
	unsigned int recovery_log_reason = get_di_recovery_log_reason();
	int di_blocking = get_di_di_blocking();
	unsigned int recovery_log_queue_idx = get_di_recovery_log_queue_idx();
	struct di_buf_s *recovery_log_di_buf = get_di_recovery_log_di_buf();
	int video_peek_cnt = get_di_video_peek_cnt();
	unsigned long reg_unreg_timeout_cnt = get_di_reg_unreg_timeout_cnt();
	struct vframe_s **vframe_in = get_di_vframe_in();

	dump_state_flag = 1;
	seq_printf(seq, "version %s, init_flag %d, is_bypass %d\n",
			version_s, init_flag, is_bypass(NULL));
	seq_printf(seq, "recovery_flag = %d, recovery_log_reason=%d, di_blocking=%d",
		recovery_flag, recovery_log_reason, di_blocking);
seq_printf(seq, "recovery_log_queue_idx=%d, recovery_log_di_buf=0x%p\n",
		recovery_log_queue_idx, recovery_log_di_buf);
	seq_printf(seq, "buffer_size=%d, mem_flag=%d, cma_flag=%d\n",
		de_devp->buffer_size, atomic_read(&de_devp->mem_flag),
		de_devp->flag_cma);
	keep_buf = di_post_stru_p->keep_buf;
	seq_printf(seq, "used_post_buf_index %d(0x%p),",
		IS_ERR_OR_NULL(keep_buf) ?
		-1 : keep_buf->index, keep_buf);
	if (!IS_ERR_OR_NULL(keep_buf)) {
		seq_puts(seq, "used_local_buf_index:\n");
		for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
			p = keep_buf->di_buf_dup_p[i];
			seq_printf(seq, "%d(0x%p) ",
				IS_ERR_OR_NULL(p) ? -1 : p->index, p);
		}
	}
	seq_printf(seq, "\nin_free_list (max %d):\n", MAX_IN_BUF_NUM);
	queue_for_each_entry(p, ptmp, QUEUE_IN_FREE, list) {
		seq_printf(seq, "index %2d, 0x%p, type %d\n",
			p->index, p, p->type);
	}
	seq_printf(seq, "local_free_list (max %d):\n", de_devp->buf_num_avail);
	queue_for_each_entry(p, ptmp, QUEUE_LOCAL_FREE, list) {
		seq_printf(seq, "index %2d, 0x%p, type %d\n",
			p->index, p, p->type);
	}

	seq_puts(seq, "post_doing_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_POST_DOING, list) {
		print_di_buf_seq(p, 2, seq);
	}
	seq_puts(seq, "pre_ready_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_PRE_READY, list) {
		print_di_buf_seq(p, 2, seq);
	}
	seq_printf(seq, "post_free_list (max %d):\n",
		di_post_stru_p->di_post_num);
	queue_for_each_entry(p, ptmp, QUEUE_POST_FREE, list) {
		seq_printf(seq, "index %2d, 0x%p, type %d, vframetype 0x%x\n",
			p->index, p, p->type, p->vframe->type);
	}
	seq_puts(seq, "post_ready_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_POST_READY, list) {
		print_di_buf_seq(p, 2, seq);
		print_di_buf_seq(p->di_buf[0], 1, seq);
		print_di_buf_seq(p->di_buf[1], 1, seq);
	}
	seq_puts(seq, "display_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_DISPLAY, list) {
		print_di_buf_seq(p, 2, seq);
		print_di_buf_seq(p->di_buf[0], 1, seq);
		print_di_buf_seq(p->di_buf[1], 1, seq);
	}
	seq_puts(seq, "recycle_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_RECYCLE, list) {
		seq_printf(seq,
			"index %d, 0x%p, type %d, vframetype 0x%x pre_ref_count %d post_ref_count %d\n",
			p->index, p, p->type,
			p->vframe->type,
			p->pre_ref_count,
			p->post_ref_count);
		if (p->di_wr_linked_buf) {
			seq_printf(seq,
				"linked index %2d, 0x%p, type %d pre_ref_count %d post_ref_count %d\n",
				p->di_wr_linked_buf->index,
				p->di_wr_linked_buf,
				p->di_wr_linked_buf->type,
				p->di_wr_linked_buf->pre_ref_count,
				p->di_wr_linked_buf->post_ref_count);
		}
	}
	if (di_pre_stru_p->di_inp_buf) {
		seq_printf(seq, "di_inp_buf:index %d, 0x%p, type %d\n",
			di_pre_stru_p->di_inp_buf->index,
			&di_pre_stru_p->di_inp_buf,
			di_pre_stru_p->di_inp_buf->type);
	} else {
		seq_puts(seq, "di_inp_buf: NULL\n");
	}
	if (di_pre_stru_p->di_wr_buf) {
		seq_printf(seq, "di_wr_buf:index %d, 0x%p, type %d\n",
			di_pre_stru_p->di_wr_buf->index,
			&di_pre_stru_p->di_wr_buf,
			di_pre_stru_p->di_wr_buf->type);
	} else {
		seq_puts(seq, "di_wr_buf: NULL\n");
	}
	dump_di_pre_stru_seq(seq, v);/*dump_di_pre_stru();*/
	dump_di_post_stru_seq(seq, v);/*dump_di_post_stru();*/
	seq_puts(seq, "vframe_in[]:");

	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		seq_printf(seq, "0x%p\n", *vframe_in);
		vframe_in++;
	}

	seq_puts(seq, "\n");
	seq_printf(seq, "vf_peek()=>0x%p, video_peek_cnt = %d\n",
	    vf_peek(VFM_NAME), video_peek_cnt);
	seq_printf(seq, "reg_unreg_timerout = %lu\n", reg_unreg_timeout_cnt);
	dump_state_flag = 0;
	return 0;
}

static int seq_file_afbc_show(struct seq_file *seq, void *v)
{
	seq_puts(seq, "******dump VD2 AFBC********\n");
	seq_printf(seq, "VD2_AFBC_ENABLE 0x%x.\n", RDMA_RD(VD2_AFBC_ENABLE));
	seq_printf(seq, "VD2_AFBC_STAT 0x%x.\n", RDMA_RD(VD2_AFBC_STAT));
	seq_printf(seq, "VD2_AFBCD1_MISC_CTRL 0x%x.\n",
				RDMA_RD(VD2_AFBCD1_MISC_CTRL));

	seq_puts(seq, "******dump VD1 AFBC********\n");
	seq_printf(seq, "AFBC_ENABLE 0x%x.\n", RDMA_RD(AFBC_ENABLE));
	seq_printf(seq, "AFBC_STAT 0x%x.\n", RDMA_RD(AFBC_STAT));
	seq_printf(seq, "VD1_AFBCD0_MISC_CTRL 0x%x.\n",
				RDMA_RD(VD1_AFBCD0_MISC_CTRL));
	seq_puts(seq, "***************************\n");


	seq_printf(seq, "VIU_MISC_CTRL0 0x%x.\n", RDMA_RD(VIU_MISC_CTRL0));
	seq_printf(seq, "VIU_MISC_CTRL1 0x%x.\n", RDMA_RD(VIU_MISC_CTRL1));
	seq_printf(seq, "VIUB_MISC_CTRL0 0x%x.\n", RDMA_RD(VIUB_MISC_CTRL0));

	seq_printf(seq, "DI_PRE_CTRL bit8=%d,bit 28 =%d.\n",
				RDMA_RD_BITS(DI_PRE_CTRL, 8, 1),
				RDMA_RD_BITS(DI_PRE_CTRL, 28, 1));

	return 0;
}

/*2018-08-17 add debugfs*/
#define DEFINE_SHOW_DI(__name) \
static int __name ## _open(struct inode *inode, struct file *file)	\
{ \
	return single_open(file, __name ## _show, inode->i_private);	\
} \
									\
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.read = seq_read,		\
	.llseek = seq_lseek,		\
	.release = single_release,	\
}

DEFINE_SHOW_DI(seq_file_di_state);
DEFINE_SHOW_DI(seq_file_dump_di_reg);
DEFINE_SHOW_DI(seq_file_dump_mif_size_state);
DEFINE_SHOW_DI(seq_file_afbc);

struct di_debugfs_files_t {
	const char *name;
	const umode_t mode;
	const struct file_operations *fops;
};

static struct di_debugfs_files_t di_debugfs_files[] = {
	{"state", S_IFREG | 0644, &seq_file_di_state_fops},
	{"dumpreg", S_IFREG | 0644, &seq_file_dump_di_reg_fops},
	{"dumpmif", S_IFREG | 0644, &seq_file_dump_mif_size_state_fops},
	{"dumpafbc", S_IFREG | 0644, &seq_file_afbc_fops},
};

void di_debugfs_init(void)
{
	int i;
	struct dentry *ent;
	struct di_dev_s *de_devp = get_di_de_devp();

	if (de_devp->dbg_root)
		return;

	de_devp->dbg_root = debugfs_create_dir("di", NULL);
	if (!de_devp->dbg_root) {
		pr_err("can't create debugfs dir di\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(di_debugfs_files); i++) {
		ent = debugfs_create_file(di_debugfs_files[i].name,
			di_debugfs_files[i].mode,
			de_devp->dbg_root, NULL,
			di_debugfs_files[i].fops);
		if (!ent)
			pr_err("debugfs create failed\n");
	}

}
void di_debugfs_exit(void)
{
	struct di_dev_s *de_devp = get_di_de_devp();

	if (de_devp && de_devp->dbg_root)
		debugfs_remove_recursive(de_devp->dbg_root);
}
/*-----------------------*/

