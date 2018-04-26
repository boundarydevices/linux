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
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/mm.h>
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
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
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

#define VDIN_DRV_NAME		"vdin"
#define VDIN_DEV_NAME		"vdin"
#define VDIN_CLS_NAME		"vdin"
#define PROVIDER_NAME		"vdin"

#define VDIN_PUT_INTERVAL (HZ/100)   /* 10ms, #define HZ 100 */

static dev_t vdin_devno;
static struct class *vdin_clsp;
static unsigned int vdin_addr_offset[VDIN_MAX_DEVS] = {0, 0x80};
static struct vdin_dev_s *vdin_devp[VDIN_MAX_DEVS];
static unsigned long mem_start, mem_end;
static unsigned int use_reserved_mem;
/*
 * canvas_config_mode
 * 0: canvas_config in driver probe
 * 1: start cofig
 * 2: auto config
 */
static int canvas_config_mode = 2;
static bool work_mode_simple;
static int max_ignore_frames = 2;
static int ignore_frames;
static unsigned int dv_work_delby;
/* viu isr select:
 * enable viu_hw_irq for the bandwidth is enough on gxbb/gxtvbb and laters ic
 */
static bool viu_hw_irq = 1;

#ifdef DEBUG_SUPPORT
module_param(canvas_config_mode, int, 0664);
MODULE_PARM_DESC(canvas_config_mode, "canvas configure mode");

module_param(work_mode_simple, bool, 0664);
MODULE_PARM_DESC(work_mode_simple, "enable/disable simple work mode");

module_param(max_ignore_frames, int, 0664);
MODULE_PARM_DESC(max_ignore_frames, "ignore first <n> frames");

module_param(ignore_frames, int, 0664);
MODULE_PARM_DESC(ignore_frames, "ignore first <n> frames");

module_param(dv_work_delby, uint, 0664);
MODULE_PARM_DESC(dv_work_delby, "dv_work_delby");

module_param(viu_hw_irq, bool, 0664);
MODULE_PARM_DESC(viu_hw_irq, "viu_hw_irq");
#endif

static bool vdin_dbg_en;
module_param(vdin_dbg_en, bool, 0664);
MODULE_PARM_DESC(vdin_dbg_en, "enable/disable vdin debug information");

static bool time_en;
module_param(time_en, bool, 0664);
MODULE_PARM_DESC(time_en, "enable/disable vdin debug information");
/*
*the check flag in vdin_isr
*bit0:bypass stop check,bit1:bypass cyc check
*bit2:bypass vsync check,bit3:bypass vga check
*/
static unsigned int isr_flag;
module_param(isr_flag, uint, 0664);
MODULE_PARM_DESC(isr_flag, "flag which affect the skip field");

static unsigned int vdin_drop_cnt;
module_param(vdin_drop_cnt, uint, 0664);
MODULE_PARM_DESC(vdin_drop_cnt, "vdin_drop_cnt");

static bool game_mode;
module_param(game_mode, bool, 0664);
MODULE_PARM_DESC(game_mode, "game_mode");

static int irq_max_count;

enum tvin_force_color_range_e color_range_force = COLOR_RANGE_AUTO;

static void vdin_backup_histgram(struct vframe_s *vf, struct vdin_dev_s *devp);

char *vf_get_receiver_name(const char *provider_name);

static void vdin_timer_func(unsigned long arg)
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
	.event_cb = vdin_event_cb,
	.vf_states = vdin_vf_states,
};

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
	/* clear color para*/
	memset(&devp->pre_prop, 0, sizeof(devp->pre_prop));
	 /* clear color para*/
	memset(&devp->prop, 0, sizeof(devp->prop));

	/*enable clk*/
	vdin_clk_onoff(devp, true);
	vdin_set_default_regmap(devp->addr_offset);
	/*only for vdin0*/
	if (devp->urgent_en && (devp->index == 0))
		vdin_urgent_patch_resume(devp->addr_offset);

	/* vdin msr clock gate enable */
	if (devp->msr_clk != NULL)
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

/* 1. disable hw work, including:
 *		a. mux null input.
 *		b. set clock off.
 * 2. delete timer for state machine.
 * 3. unregiseter provider & notify receiver.
 * 4. call the callback function of the frontend to close.
 */
void vdin_close_fe(struct vdin_dev_s *devp)
{
	/* avoid null pointer oops */
	if (!devp || !devp->frontend) {
		pr_info("%s: null pointer\n", __func__);
		return;
	}
	/* bt656 clock gate disable */
	if (devp->msr_clk != NULL)
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

	pr_info("%s ok\n", __func__);
}

/*
 *based on the bellow parameters:
 *	a.h_active	(vf->width = devp->h_active)
 *	b.v_active	(vf->height = devp->v_active)
 *function: init vframe
 *1.set source type & mode
 *2.set source signal format
 *3.set pixel aspect ratio
 *4.set display ratio control
 *5.init slave vframe
 *6.set slave vf source type & mode
 */
static void vdin_vf_init(struct vdin_dev_s *devp)
{
	int i = 0;
	unsigned int chromaid, addr, index;
	struct vf_entry *master, *slave;
	struct vframe_s *vf;
	struct vf_pool *p = devp->vfp;
	enum tvin_scan_mode_e	scan_mode;

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
		/*if output fmt is nv21 or nv12 ,
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
		vdin_set_display_ratio(devp, vf);
		vdin_set_source_bitdepth(devp, vf);
		/* init slave vframe */
		slave	= vf_get_slave(p, i);
		if (slave == NULL)
			continue;
		slave->flag = master->flag;
		memset(&slave->vf, 0, sizeof(struct vframe_s));
		slave->vf.index   = vf->index;
		slave->vf.width   = vf->width;
		slave->vf.height	   = vf->height;
		slave->vf.duration    = vf->duration;
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
	struct vdin_dev_s *devp = arg;

	devp->rdma_irq_cnt++;
	return;
}

static struct rdma_op_s vdin_rdma_op = {
	vdin_rdma_irq,
	NULL
};
#endif

/*
 * 1. config canvas base on  canvas_config_mode
 *		0: canvas_config in driver probe
 *		1: start cofig
 *		2: auto config
 * 2. recalculate h_active and v_active, including:
 *		a. vdin_set_decimation.
 *		b. vdin_set_cutwin.
 *		c. vdin_set_hvscale.
 * 3. enable hw work, including:
 *		a. mux null input.
 *		b. set clock auto.
 * 4. set all registeres including:
 *		a. mux input.
 * 5. call the callback function of the frontend to start.
 * 6. enable irq .
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

		if (!(devp->flags & VDIN_FLAG_V4L2_DEBUG))
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

	/*gxbb/gxl/gxm use clkb as vdin clk,
	 *for clkb is low speed,wich is enough for 1080p process,
	 *gxtvbb/txl use vpu clk for process 4k
	 *g12a use vpu clk for process 4K input buf can't output 4k
	 */
	if (is_meson_gxl_cpu() || is_meson_gxm_cpu() || is_meson_gxbb_cpu() ||
		is_meson_txhd_cpu())
		switch_vpu_clk_gate_vmod(VPU_VPU_CLKB, VPU_CLK_GATE_ON);

	vdin_get_format_convert(devp);
	devp->curr_wr_vfe = NULL;
	devp->game_mode = game_mode;
	if (game_mode)
		devp->vfp->skip_vf_num = 1;
	else
		devp->vfp->skip_vf_num = devp->prop.skip_vf_num;
	if (devp->vfp->skip_vf_num >= VDIN_CANVAS_MAX_CNT)
		devp->vfp->skip_vf_num = 0;
	devp->canvas_config_mode = canvas_config_mode;
	/* h_active/v_active will be recalculated by bellow calling */
	vdin_set_decimation(devp);
	vdin_set_cutwin(devp);
	vdin_set_hvscale(devp);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
		vdin_set_bitdepth(devp);
	/* txl new add fix for hdmi switch resolution cause cpu holding */
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL)
		vdin_fix_nonstd_vsync(devp);


    /*reverse / disable reverse write buffer*/
	vdin_wr_reverse(devp->addr_offset,
				devp->parm.h_reverse,
				devp->parm.v_reverse);
#ifdef CONFIG_CMA
	vdin_cma_malloc_mode(devp);
	if (vdin_cma_alloc(devp)) {
		pr_err("\nvdin%d %s fail for cma alloc fail!!!\n",
			devp->index, __func__);
		return;
	}
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
	if (devp->prop.fps &&
		devp->parm.port >= TVIN_PORT_HDMI0 &&
		devp->parm.port <= TVIN_PORT_HDMI7)
		devp->duration = 96000/(devp->prop.fps);
	else
		devp->duration = devp->fmt_info_p->duration;

	devp->vfp->size = devp->canvas_max_num;
	vf_pool_init(devp->vfp, devp->vfp->size);
	vdin_vf_init(devp);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if ((devp->dv.dolby_input & (1 << devp->index)) ||
		(devp->dv.dv_flag && is_dolby_vision_enable())) {
		/* config dolby mem base */
		vdin_dolby_addr_alloc(devp, devp->vfp->size);
		/* config dolby vision */
		vdin_dolby_config(devp);
		if (vdin_dbg_en)
			pr_info("vdin start dec dv input config\n");
	}
#endif
	devp->abnormal_cnt = 0;
	devp->last_wr_vfe = NULL;
	irq_max_count = 0;
	/* devp->stamp_valid = false; */
	devp->stamp = 0;
	devp->cycle = 0;
	devp->hcnt64 = 0;

	memset(&devp->parm.histgram[0], 0, sizeof(unsigned short) * 64);

	devp->curr_field_type = vdin_get_curr_field_type(devp);
	/* configure regs and enable hw */
	switch_vpu_mem_pd_vmod(devp->addr_offset?VPU_VIU_VDIN1:VPU_VIU_VDIN0,
			VPU_MEM_POWER_ON);

	vdin_hw_enable(devp->addr_offset);
	vdin_set_all_regs(devp);
	if (!(devp->parm.flag & TVIN_PARM_FLAG_CAP) &&
		(devp->frontend) &&
		devp->frontend->dec_ops &&
		devp->frontend->dec_ops->start &&
		((devp->parm.port != TVIN_PORT_CVBS3) ||
		((devp->flags & VDIN_FLAG_SNOW_FLAG) == 0)))
		devp->frontend->dec_ops->start(devp->frontend,
				devp->parm.info.fmt);

#ifdef CONFIG_AML_RDMA
	/*it is better put after all reg init*/
	if (devp->rdma_enable && devp->rdma_handle > 0)
		devp->flags |= VDIN_FLAG_RDMA_ENABLE;
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	/*only for vdin0;vdin1 used for debug*/
	if ((devp->dv.dolby_input & (1 << 0)) ||
		(devp->dv.dv_flag && is_dolby_vision_enable()))
		vf_reg_provider(&devp->dv.vprov_dv);
	else
#endif
		vf_reg_provider(&devp->vprov);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if ((devp->dv.dolby_input & (1 << devp->index)) ||
		(devp->dv.dv_flag && is_dolby_vision_enable()))
		vf_notify_receiver("dv_vdin",
			VFRAME_EVENT_PROVIDER_START, NULL);
	else
#endif
		vf_notify_receiver(devp->name,
			VFRAME_EVENT_PROVIDER_START, NULL);
	if ((devp->parm.port != TVIN_PORT_VIU1) ||
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
	if (devp->parm.port != TVIN_PORT_VIU1) {
		/*disable audio&video sync used for libplayer*/
		tsync_set_enable(0);
		/* enable system_time */
		timestamp_pcrscr_enable(1);
		pr_info("****[%s]disable tysnc& enable system time!****\n",
			__func__);
	}
#endif
	devp->irq_cnt = 0;
	devp->rdma_irq_cnt = 0;
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
 * 3. reset default canvas
 * 4.call the callback function of the frontend to stop vdin.
 */
void vdin_stop_dec(struct vdin_dev_s *devp)
{
	/* avoid null pointer oops */
	if (!devp || !devp->frontend)
		return;
#ifdef CONFIG_CMA
	if ((devp->cma_mem_alloc == 0) && devp->cma_config_en) {
		pr_info("%s:cma not alloc,don't need do others!\n", __func__);
		return;
	}
#endif
	disable_irq_nosync(devp->irq);
	vdin_hw_disable(devp->addr_offset);
	if (!(devp->parm.flag & TVIN_PARM_FLAG_CAP) &&
		devp->frontend->dec_ops &&
		devp->frontend->dec_ops->stop &&
		((devp->parm.port != TVIN_PORT_CVBS3) ||
		((devp->flags & VDIN_FLAG_SNOW_FLAG) == 0)))
		devp->frontend->dec_ops->stop(devp->frontend, devp->parm.port);

	vdin_set_default_regmap(devp->addr_offset);
	/*only for vdin0*/
	if (devp->urgent_en && (devp->index == 0))
		vdin_urgent_patch_resume(devp->addr_offset);

	/* reset default canvas  */
	vdin_set_def_wr_canvas(devp);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (((devp->dv.dolby_input & (1 << devp->index)) ||
		is_dolby_vision_enable()) &&
		(devp->dv.dv_config == true))
		vf_unreg_provider(&devp->dv.vprov_dv);
	else
#endif
		vf_unreg_provider(&devp->vprov);
	devp->dv.dv_config = 0;
#ifdef CONFIG_CMA
	vdin_cma_release(devp);
#endif
	vdin_dolby_addr_release(devp, devp->vfp->size);


	switch_vpu_mem_pd_vmod(devp->addr_offset?VPU_VIU_VDIN1:VPU_VIU_VDIN0,
			VPU_MEM_POWER_DOWN);
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

/*
 *config the vdin use default regmap
 *call vdin_start_dec to start vdin
 */
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
	if ((para->port != TVIN_PORT_VIU1) ||
		(viu_hw_irq != 0)) {
		ret = request_irq(devp->irq, vdin_v4l2_isr, IRQF_SHARED,
				devp->irq_name, (void *)devp);
		devp->flags |= VDIN_FLAG_ISR_REQ;
		/*disable vsync irq until vdin configured completely*/
		disable_irq_nosync(devp->irq);
	}
	vdin_clk_onoff(devp, true);
	/*config the vdin use default value*/
	vdin_set_default_regmap(devp->addr_offset);
	/*only for vdin0*/
	if (devp->urgent_en && (devp->index == 0))
		vdin_urgent_patch_resume(devp->addr_offset);

	devp->parm.port = para->port;
	devp->parm.info.fmt = para->fmt;
	fmt = devp->parm.info.fmt;
	/* add for camera random resolution */
	if (para->fmt >= TVIN_SIG_FMT_MAX) {
		devp->fmt_info_p = kmalloc(sizeof(struct tvin_format_s),
				GFP_KERNEL);
		if (!devp->fmt_info_p) {
			pr_err("[vdin]%s kmalloc error.\n", __func__);
			return -ENOMEM;
		}
		devp->fmt_info_p->hs_bp     = para->hs_bp;
		devp->fmt_info_p->vs_bp     = para->vs_bp;
		devp->fmt_info_p->hs_pol    = para->hsync_phase;
		devp->fmt_info_p->vs_pol    = para->vsync_phase;
		if ((para->h_active * para->v_active * para->frame_rate)
			> devp->vdin_max_pixelclk)
			para->h_active >>= 1;
		devp->fmt_info_p->h_active  = para->h_active;
		devp->fmt_info_p->v_active  = para->v_active;
		if ((devp->parm.port == TVIN_PORT_VIU1_VIDEO) &&
			(!(devp->flags & VDIN_FLAG_V4L2_DEBUG))) {
			devp->fmt_info_p->v_active =
				((rd(0, VPP_POSTBLEND_VD1_V_START_END) &
				0xfff) - ((rd(0, VPP_POSTBLEND_VD1_V_START_END)
				>> 16) & 0xfff) + 1);
			devp->fmt_info_p->h_active =
				((rd(0, VPP_POSTBLEND_VD1_H_START_END) &
				0xfff) - ((rd(0, VPP_POSTBLEND_VD1_H_START_END)
				>> 16) & 0xfff) + 1);
		}
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
		if (devp->msr_clk != NULL)
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

/*
 *call vdin_stop_dec to stop the frontend
 *close frontend
 *free the memory allocated in start tvin service
 */
int stop_tvin_service(int no)
{
	struct vdin_dev_s *devp;
	unsigned int end_time;

	devp = vdin_devp[no];
	if (!(devp->flags&VDIN_FLAG_DEC_STARTED)) {
		pr_err("%s:decode hasn't started.\n", __func__);
		return -EBUSY;
	}
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
	if ((devp->parm.port != TVIN_PORT_VIU1) ||
		(viu_hw_irq != 0)) {
		free_irq(devp->irq, (void *)devp);
		devp->flags &= (~VDIN_FLAG_ISR_REQ);
	}
	end_time = jiffies_to_msecs(jiffies);
	if (time_en)
		pr_info("[vdin]:vdin start time:%ums,stop time:%ums,run time:%u.\n",
				devp->start_time,
				end_time,
				end_time-devp->start_time);

	return 0;
}
EXPORT_SYMBOL(stop_tvin_service);

static void get_tvin_canvas_info(int *start, int *num)
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

/*
 * if parm.port is TVIN_PORT_VIU1,call vdin_v4l2_isr
 *	vdin_v4l2_isr is used to the sample
 *	v4l2 application such as camera,viu
 */
static void vdin_rdma_isr(struct vdin_dev_s *devp)
{
	if (devp->parm.port == TVIN_PORT_VIU1)
		vdin_v4l2_isr(devp->irq, devp);
}

/*based on parameter
 *parm->cmd :
 *	VDIN_CMD_SET_CSC
 *	VDIN_CMD_SET_CM2
 *	VDIN_CMD_ISR
 *	VDIN_CMD_MPEGIN_START
 *	VDIN_CMD_GET_HISTGRAM
 *	VDIN_CMD_MPEGIN_STOP
 *	VDIN_CMD_FORCE_GO_FIELD
 */
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

/*call vdin_hw_disable to pause hw*/
void vdin_pause_dec(struct vdin_dev_s *devp)
{
	vdin_hw_disable(devp->addr_offset);
}

/*call vdin_hw_enable to resume hw*/
void vdin_resume_dec(struct vdin_dev_s *devp)
{
	vdin_hw_enable(devp->addr_offset);
}
/*register provider & notify receiver */
void vdin_vf_reg(struct vdin_dev_s *devp)
{
	vf_reg_provider(&devp->vprov);
	vf_notify_receiver(devp->name, VFRAME_EVENT_PROVIDER_START, NULL);
}

/*unregister provider*/
void vdin_vf_unreg(struct vdin_dev_s *devp)
{
	vf_unreg_provider(&devp->vprov);
}

/*
 * set vframe_view base on parameters:
 * left_eye, right_eye, parm.info.trans_fmt
 * set view:
 * start_x, start_y, width, height
 */
inline void vdin_set_view(struct vdin_dev_s *devp, struct vframe_s *vf)
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

	memset(left_eye, 0, sizeof(struct vframe_view_s));
	memset(right_eye, 0, sizeof(struct vframe_view_s));

	switch (devp->parm.info.trans_fmt) {
	case TVIN_TFMT_3D_LRH_OLOR:
	case TVIN_TFMT_3D_LRH_OLER:
		left_eye->width  = devp->h_active >> 1;
		left_eye->height	  = devp->v_active;
		right_eye->start_x   = devp->h_active >> 1;
		right_eye->width	  = devp->h_active >> 1;
		right_eye->height	  = devp->v_active;
		break;
	case TVIN_TFMT_3D_TB:
		left_eye->width  = devp->h_active;
		left_eye->height	  = devp->v_active >> 1;
		right_eye->start_y   = devp->v_active >> 1;
		right_eye->width	  = devp->h_active;
		right_eye->height	  = devp->v_active >> 1;
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
			slave->vf.left_eye.width	 = devp->h_active;
			slave->vf.left_eye.height	 = vactive;
			slave->vf.right_eye.start_x = 0;
			slave->vf.right_eye.start_y = vactive + vspace +
					vactive + vspace + vactive + vspace - 1;
			slave->vf.right_eye.width	 = devp->h_active;
			slave->vf.right_eye.height  = vactive;
		} else
			vactive = (fmt_info->v_active - vspace) >> 1;
		left_eye->width  = devp->h_active;
		left_eye->height	  = vactive;
		right_eye->start_y   = vactive + vspace;
		right_eye->width	  = devp->h_active;
		right_eye->height	  = vactive;
		break;
	}
	case TVIN_TFMT_3D_FA: {
		unsigned int vactive = 0;
		unsigned int vspace = 0;

		vspace = fmt_info->vs_front + fmt_info->vs_width +
				fmt_info->vs_bp;
		vactive = (fmt_info->v_active - vspace + 1) >> 1;
		left_eye->width	  = devp->h_active;
		left_eye->height	  = vactive;
		right_eye->start_y   = vactive + vspace;
		right_eye->width	  = devp->h_active;
		right_eye->height	  = vactive;
		break;
	}
	case TVIN_TFMT_3D_LA: {
		left_eye->width	  = devp->h_active;
		left_eye->height	  = (devp->v_active) >> 1;
		right_eye->width	  = devp->h_active;
		right_eye->height	  = (devp->v_active) >> 1;
		break;
	}
	default:
		break;
	}
}

/*function:
 *	get current field type
 *	set canvas addr
 *	disable hw when irq_max_count >= canvas_max_num
 */
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

static void vdin_backup_histgram(struct vframe_s *vf, struct vdin_dev_s *devp)
{
	unsigned int i = 0;

	devp->parm.hist_pow = vf->prop.hist.hist_pow;
	devp->parm.luma_sum = vf->prop.hist.luma_sum;
	devp->parm.pixel_sum = vf->prop.hist.pixel_sum;
	for (i = 0; i < 64; i++)
		devp->parm.histgram[i] = vf->prop.hist.gamma[i];
}

/*
 *VDIN_FLAG_RDMA_ENABLE=1
 *	provider_vf_put(devp->last_wr_vfe, devp->vfp);
 *VDIN_FLAG_RDMA_ENABLE=0
 *	provider_vf_put(curr_wr_vfe, devp->vfp);
 */
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
	unsigned int offset, vf_drop_cnt;
	enum tvin_trans_fmt trans_fmt;
	struct tvin_sig_property_s *prop, *pre_prop;

	/* debug interrupt interval time
	 *
	 * this code about system time must be outside of spinlock.
	 * because the spinlock may affect the system time.
	 */

	/* ignore fake irq caused by sw reset*/
	if (devp->vdin_reset_flag) {
		devp->vdin_reset_flag = 0;
		return IRQ_HANDLED;
	}
	vf_drop_cnt = vdin_drop_cnt;

	/* avoid null pointer oops */
	if (!devp || !devp->frontend) {
		devp->vdin_irq_flag = 1;
		goto irq_handled;
	}
	offset = devp->addr_offset;

	isr_log(devp->vfp);
	devp->irq_cnt++;
	/* debug interrupt interval time
	 *
	 * this code about system time must be outside of spinlock.
	 * because the spinlock may affect the system time.
	 */

	spin_lock_irqsave(&devp->isr_lock, flags);
	/* W_VCBUS_BIT(VDIN_MISC_CTRL, 0, 0, 2); */
	devp->vdin_reset_flag = vdin_vsync_reset_mif(devp->index);
	if ((devp->flags & VDIN_FLAG_DEC_STOP_ISR) &&
		(!(isr_flag & VDIN_BYPASS_STOP_CHECK))) {
		vdin_hw_disable(offset);
		devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR;
		devp->vdin_irq_flag = 2;

		goto irq_handled;
	}
	stamp  = vdin_get_meas_vstamp(offset);
	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);
		devp->curr_wr_vfe->vf.ready_jiffies64 = jiffies_64;
		/*save the first field stamp*/
		devp->stamp = stamp;
		devp->vdin_irq_flag = 3;
		vdin_drop_cnt++;
		goto irq_handled;
	}
	if (devp->last_wr_vfe && (devp->flags&VDIN_FLAG_RDMA_ENABLE) &&
		(devp->game_mode == false)) {
		/*dolby vision metadata process*/
		if (dv_dbg_mask & DV_UPDATE_DATA_MODE_DELBY_WORK
			&& devp->dv.dv_config) {
			/* prepare for dolby vision metadata addr */
			devp->dv.dv_cur_index = devp->last_wr_vfe->vf.index;
			devp->dv.dv_next_index = devp->curr_wr_vfe->vf.index;
			schedule_delayed_work(&devp->dv.dv_dwork,
				dv_work_delby);
		} else if (((dv_dbg_mask & DV_UPDATE_DATA_MODE_DELBY_WORK) == 0)
			&& devp->dv.dv_config) {
			vdin_dolby_buffer_update(devp,
				devp->last_wr_vfe->vf.index);
			vdin_dolby_addr_update(devp,
				devp->curr_wr_vfe->vf.index);
		} else
			devp->dv.dv_crc_check = true;
		if ((devp->dv.dv_crc_check == true) ||
			(!(dv_dbg_mask & DV_CRC_CHECK)))
			provider_vf_put(devp->last_wr_vfe, devp->vfp);
		else {
			devp->vdin_irq_flag = 15;
			vdin_drop_cnt++;
			goto irq_handled;
		}
		/*skip policy process*/
		if (devp->vfp->skip_vf_num > 0)
			vdin_vf_disp_mode_update(devp->last_wr_vfe, devp->vfp);

		devp->last_wr_vfe = NULL;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (((devp->dv.dolby_input & (1 << devp->index)) ||
			(devp->dv.dv_flag && is_dolby_vision_enable())) &&
			(devp->dv.dv_config == true))
			vf_notify_receiver("dv_vdin",
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
		else
#endif
			vf_notify_receiver(devp->name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	}
	/*check vs is valid base on the time during continuous vs*/
	if (vdin_check_cycle(devp) && (!(isr_flag & VDIN_BYPASS_CYC_CHECK))
		&& (!(devp->flags & VDIN_FLAG_SNOW_FLAG))) {
		devp->vdin_irq_flag = 4;
		vdin_drop_cnt++;
		goto irq_handled;
	}

	devp->hcnt64 = vdin_get_meas_hcnt64(offset);

	sm_ops = devp->frontend->sm_ops;

	last_field_type = devp->curr_field_type;
	devp->curr_field_type = vdin_get_curr_field_type(devp);

	/* ignore the unstable signal */
	state = tvin_get_sm_status(devp->index);
	if (((devp->parm.info.status != TVIN_SIG_STATUS_STABLE) ||
		(state != TVIN_SM_STATUS_STABLE)) &&
		(!(devp->flags & VDIN_FLAG_SNOW_FLAG))) {
		devp->vdin_irq_flag = 6;
		vdin_drop_cnt++;
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
		devp->vdin_irq_flag = 7;
		vdin_drop_cnt++;
		goto irq_handled;
	}
	curr_wr_vfe = devp->curr_wr_vfe;
	curr_wr_vf  = &curr_wr_vfe->vf;

	/* change color matrix */
	if (devp->csc_cfg != 0) {
		prop = &devp->prop;
		pre_prop = &devp->pre_prop;
		if ((prop->color_format != pre_prop->color_format) ||
			(prop->vdin_hdr_Flag != pre_prop->vdin_hdr_Flag) ||
			(prop->color_fmt_range != pre_prop->color_fmt_range))
			vdin_set_matrix(devp);
		if (prop->dest_cfmt != pre_prop->dest_cfmt) {
			vdin_set_bitdepth(devp);
			vdin_source_bitdepth_reinit(devp);
			vdin_set_wr_ctrl_vsync(devp, devp->addr_offset,
				devp->format_convert,
				devp->color_depth_mode, devp->source_bitdepth,
				devp->flags&VDIN_FLAG_RDMA_ENABLE);
			vdin_set_top(devp->addr_offset, devp->parm.port,
				devp->prop.color_format, devp->h_active,
				devp->bt_path);
		}
		pre_prop->color_format = prop->color_format;
		pre_prop->vdin_hdr_Flag = prop->vdin_hdr_Flag;
		pre_prop->color_fmt_range = prop->color_fmt_range;
		pre_prop->dest_cfmt = prop->dest_cfmt;
	}
	/* change cutwindow */
	if ((devp->cutwindow_cfg != 0) && (devp->auto_cutwindow_en == 1)) {
		prop = &devp->prop;
		if (prop->pre_vs || prop->pre_ve)
			devp->v_active += (prop->pre_vs + prop->pre_ve);
		if (prop->pre_hs || prop->pre_he)
			devp->h_active += (prop->pre_hs + prop->pre_he);
		vdin_set_cutwin(devp);
		prop->pre_vs = prop->vs;
		prop->pre_ve = prop->ve;
		prop->pre_hs = prop->hs;
		prop->pre_he = prop->he;
		devp->cutwindow_cfg = 0;
	}
	if ((devp->auto_cutwindow_en == 1) &&
		(devp->parm.port >= TVIN_PORT_CVBS0) &&
		(devp->parm.port <= TVIN_PORT_CVBS3))
		curr_wr_vf->width = devp->h_active;

	decops = devp->frontend->dec_ops;
	if (decops->decode_isr(devp->frontend, devp->hcnt64) == TVIN_BUF_SKIP) {
		devp->vdin_irq_flag = 8;
		vdin_drop_cnt++;
		goto irq_handled;
	}
	if ((devp->parm.port >= TVIN_PORT_CVBS0) &&
		(devp->parm.port <= TVIN_PORT_CVBS3))
		curr_wr_vf->phase = sm_ops->get_secam_phase(devp->frontend) ?
				VFRAME_PHASE_DB : VFRAME_PHASE_DR;


	if (ignore_frames < max_ignore_frames) {
		ignore_frames++;
		devp->vdin_irq_flag = 12;
		vdin_drop_cnt++;
		goto irq_handled;
	}

	if (sm_ops->check_frame_skip &&
		sm_ops->check_frame_skip(devp->frontend)) {
		devp->vdin_irq_flag = 13;
		vdin_drop_cnt++;
		if (devp->flags&VDIN_FLAG_RDMA_ENABLE)
			ignore_frames = 0;
		goto irq_handled;
	}

	next_wr_vfe = provider_vf_peek(devp->vfp);
	if (!next_wr_vfe) {
		devp->vdin_irq_flag = 14;
		vdin_drop_cnt++;
		goto irq_handled;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (((devp->dv.dolby_input & (1 << devp->index)) ||
		(devp->dv.dv_flag && is_dolby_vision_enable())) &&
		(devp->dv.dv_config == true))
		vdin2nr = vf_notify_receiver("dv_vdin",
			VFRAME_EVENT_PROVIDER_QUREY_VDIN2NR, NULL);
	else
#endif
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
	#ifdef CONFIG_AML_LOCAL_DIMMING
	/*vdin_backup_histgram_ldim(curr_wr_vf, devp);*/
	#endif
	if ((devp->parm.port >= TVIN_PORT_HDMI0) &&
		(devp->parm.port <= TVIN_PORT_HDMI7)) {
		curr_wr_vf->trans_fmt = devp->parm.info.trans_fmt;
		vdin_set_view(devp, curr_wr_vf);
	}
#if 1
	vdin_calculate_duration(devp);
#else
	curr_wr_vf->duration = devp->duration;
#endif
	/* put for receiver
	 * ppmgr had handled master and slave vf by itself,
	 * vdin do not to declare them respectively
	 * ppmgr put the vf that included master vf and slave vf
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
	if (devp->auto_ratio_en && (devp->parm.port >= TVIN_PORT_CVBS0) &&
		(devp->parm.port <= TVIN_PORT_CVBS3))
		vdin_set_display_ratio(devp, curr_wr_vf);
	if (devp->flags&VDIN_FLAG_RDMA_ENABLE && (devp->game_mode == false)) {
		devp->last_wr_vfe = curr_wr_vfe;
	} else {
		/*dolby vision metadata process*/
		if (dv_dbg_mask & DV_UPDATE_DATA_MODE_DELBY_WORK
			&& devp->dv.dv_config) {
			/* prepare for dolby vision metadata addr */
			devp->dv.dv_cur_index = curr_wr_vfe->vf.index;
			devp->dv.dv_next_index = next_wr_vfe->vf.index;
			schedule_delayed_work(&devp->dv.dv_dwork,
				dv_work_delby);
		} else if (((dv_dbg_mask & DV_UPDATE_DATA_MODE_DELBY_WORK) == 0)
			&& devp->dv.dv_config) {
			vdin_dolby_buffer_update(devp, curr_wr_vfe->vf.index);
			vdin_dolby_addr_update(devp, next_wr_vfe->vf.index);
		} else
			devp->dv.dv_crc_check = true;
		if ((devp->dv.dv_crc_check == true) ||
			(!(dv_dbg_mask & DV_CRC_CHECK)))
			provider_vf_put(curr_wr_vfe, devp->vfp);
		else {
			devp->vdin_irq_flag = 15;
			vdin_drop_cnt++;
			goto irq_handled;
		}
		/*skip policy process*/
		if (devp->vfp->skip_vf_num > 0)
			vdin_vf_disp_mode_update(curr_wr_vfe, devp->vfp);
	}
	/* prepare for next input data */
	next_wr_vfe = provider_vf_get(devp->vfp);
	vdin_set_canvas_id(devp, (devp->flags&VDIN_FLAG_RDMA_ENABLE),
		(next_wr_vfe->vf.canvas0Addr&0xff));
	/* prepare for chroma canvas*/
	if ((devp->prop.dest_cfmt == TVIN_NV12) ||
		(devp->prop.dest_cfmt == TVIN_NV21))
		vdin_set_chma_canvas_id(devp,
			(devp->flags&VDIN_FLAG_RDMA_ENABLE),
			(next_wr_vfe->vf.canvas0Addr>>8)&0xff);

	devp->curr_wr_vfe = next_wr_vfe;
	/* debug for video latency */
	next_wr_vfe->vf.ready_jiffies64 = jiffies_64;

	if (!(devp->flags&VDIN_FLAG_RDMA_ENABLE) || (devp->game_mode == true)) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (((devp->dv.dolby_input & (1 << devp->index)) ||
			(devp->dv.dv_flag && is_dolby_vision_enable())) &&
			(devp->dv.dv_config == true))
			vf_notify_receiver("dv_vdin",
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
		else
#endif
			vf_notify_receiver(devp->name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	}

irq_handled:
	/*hdmi skip policy should adapt to all drop vframe case*/
	if ((devp->vfp->skip_vf_num > 0) &&
		(vf_drop_cnt < vdin_drop_cnt))
		vdin_vf_disp_mode_skip(devp->vfp);

	spin_unlock_irqrestore(&devp->isr_lock, flags);
#ifdef CONFIG_AML_RDMA
	if (devp->flags & VDIN_FLAG_RDMA_ENABLE)
		rdma_config(devp->rdma_handle,
			(devp->rdma_enable&1) ?
			devp->rdma_irq:RDMA_TRIGGER_MANUAL);
#endif
	isr_log(devp->vfp);

	return IRQ_HANDLED;
}
/*
* there are too much logic in vdin_isr which is useless in camera&viu
*so vdin_v4l2_isr use to the sample v4l2 application such as camera,viu
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
	if (devp->vdin_reset_flag) {
		devp->vdin_reset_flag = 0;
		return IRQ_HANDLED;
	}
	if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
		return IRQ_HANDLED;
	isr_log(devp->vfp);
	devp->irq_cnt++;
	spin_lock_irqsave(&devp->isr_lock, flags);
	devp->vdin_reset_flag = vdin_vsync_reset_mif(devp->index);
	offset = devp->addr_offset;
	if (devp)
		/* avoid null pointer oops */
		stamp  = vdin_get_meas_vstamp(offset);
	/* if win_size changed for video only */
	if (!(devp->flags & VDIN_FLAG_V4L2_DEBUG))
		vdin_set_wr_mif(devp);
	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);
		/*save the first field stamp*/
		devp->stamp = stamp;
		goto irq_handled;
	}

	if ((devp->parm.port == TVIN_PORT_VIU1) ||
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
			(devp->rdma_enable&1) ?
			devp->rdma_irq:RDMA_TRIGGER_MANUAL);
#endif
	isr_log(devp->vfp);

	return IRQ_HANDLED;
}


static void vdin_dv_dwork(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct vdin_dev_s *devp =
		container_of(dwork, struct vdin_dev_s, dv.dv_dwork);

	if (!devp || !devp->frontend) {
		pr_info("%s, dwork error !!!\n", __func__);
		return;
	}
	if (devp->dv.dv_config) {
		vdin_dolby_buffer_update(devp, devp->dv.dv_cur_index);
		vdin_dolby_addr_update(devp, devp->dv.dv_next_index);
	}

	cancel_delayed_work(&devp->dv.dv_dwork);
}

/*function:open device
 *	1.request irq to open device configure vdinx
 *	2.disable irq until vdin is configured completely
 */
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
	devp->flags |= VDIN_FLAG_ISR_REQ;
	/*disable irq until vdin is configured completely*/
	disable_irq_nosync(devp->irq);

	/*init queue*/
	init_waitqueue_head(&devp->queue);

	/* remove the hardware limit to vertical [0-max]*/
	/* WRITE_VCBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000fff); */
	if (vdin_dbg_en)
		pr_info("open device %s ok\n", dev_name(devp->dev));
	return ret;
}

/*function:
 *	close device
 *	a.vdin_stop_dec
 *	b.vdin_close_fe
 *	c.free irq
 */
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
	if (devp->flags & VDIN_FLAG_ISR_REQ)
		free_irq(devp->irq, (void *)devp);
	devp->flags &= (~VDIN_FLAG_ISR_REQ);

	file->private_data = NULL;

	/* reset the hardware limit to vertical [0-1079]  */
	/* WRITE_VCBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000437); */
	/*if (vdin_dbg_en)*/
		pr_info("close device %s ok\n", dev_name(devp->dev));
	return 0;
}

/* vdin ioctl cmd:
 *	TVIN_IOC_OPEN /TVIN_IOC_CLOSE
 *	TVIN_IOC_START_DEC /TVIN_IOC_STOP_DEC
 *	TVIN_IOC_VF_REG /TVIN_IOC_VF_UNREG
 *	TVIN_IOC_G_SIG_INFO /TVIN_IOC_G_BUF_INFO
 *	TVIN_IOC_G_PARM
 *	TVIN_IOC_START_GET_BUF /TVIN_IOC_GET_BUF
 *	TVIN_IOC_PAUSE_DEC /TVIN_IOC_RESUME_DEC
 *	TVIN_IOC_FREEZE_VF /TVIN_IOC_UNFREEZE_VF
 *	TVIN_IOC_CALLMASTER_SET
 *	TVIN_IOC_SNOWON /TVIN_IOC_SNOWOFF
 */
static long vdin_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	int callmaster_status;
	struct vdin_dev_s *devp = NULL;
	void __user *argp = (void __user *)arg;

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
		if (info.status != TVIN_SIG_STATUS_STABLE)
			info.fps = 0;

		if (copy_to_user(argp, &info, sizeof(struct tvin_info_s)))
			ret = -EFAULT;
		mutex_unlock(&devp->fe_lock);
		break;
	}
	case TVIN_IOC_G_BUF_INFO: {
		struct tvin_buf_info_s buf_info;
		memset(&buf_info, 0, sizeof(buf_info));
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
		memset(&tvbuf, 0, sizeof(tvbuf));
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
	case TVIN_IOC_GET_COLOR_RANGE:
		if (copy_to_user(argp,
						&color_range_force,
			sizeof(enum tvin_force_color_range_e))) {
			ret = -EFAULT;
			pr_info("TVIN_IOC_GET_COLOR_RANGE err\n\n");
			break;
		}
		pr_info("get color range-%d\n\n", color_range_force);
		break;
	case TVIN_IOC_SET_COLOR_RANGE:
		if (copy_from_user(&color_range_force,
						argp,
		sizeof(enum tvin_force_color_range_e))) {
			ret = -EFAULT;
			pr_info("TVIN_IOC_SET_COLOR_RANGE err\n\n");
			break;
		}
		pr_info("force color range-%d\n\n", color_range_force);
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

/*based on parameters:
 *	mem_start, mem_size
 *	vm_pgoff, vm_page_prot
 */
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

static unsigned int vdin_poll(struct file *file, poll_table *wait)
{
	struct vdin_dev_s *devp = file->private_data;
	unsigned int mask = 0;

	poll_wait(file, &devp->queue, wait);
	mask = (POLLIN | POLLRDNORM);

	return mask;
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
	.poll            = vdin_poll,
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
static int vdin_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct vdin_dev_s *vdevp;
	struct resource *res;
	unsigned int urgent_en = 0;
	unsigned int bit_mode = VDIN_WR_COLOR_DEPTH_8BIT;
	/* const void *name; */
	/* int offset, size; */
	/* struct device_node *of_node = pdev->dev.of_node; */

	/* malloc vdev */
	vdevp = kzalloc(sizeof(struct vdin_dev_s), GFP_KERNEL);
	if (!vdevp) {
		goto fail_kzalloc_vdev;
	}
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
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret == 0)
		pr_info("\n vdin memory resource done.\n");
	else
		pr_info("\n vdin memory resource undefined!!\n");
#ifdef CONFIG_CMA
	if (!use_reserved_mem) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"flag_cma", &(vdevp->cma_config_flag));
		if (ret) {
			pr_err("don't find  match flag_cma\n");
			vdevp->cma_config_flag = 0;
		}
		if (vdevp->cma_config_flag & 0x1) {
			ret = of_property_read_u32(pdev->dev.of_node,
				"cma_size",
				&(vdevp->cma_mem_size));
			if (ret)
				pr_err("don't find  match cma_size\n");
			else
				vdevp->cma_mem_size *= SZ_1M;
		} else
			vdevp->cma_mem_size =
				dma_get_cma_size_int_byte(&pdev->dev);
		vdevp->this_pdev = pdev;
		vdevp->cma_mem_alloc = 0;
		vdevp->cma_config_en = 1;
		pr_info("vdin%d cma_mem_size = %d MB\n", vdevp->index,
				(u32)vdevp->cma_mem_size/SZ_1M);
	}
#endif
	use_reserved_mem = 0;
	/*use reserved mem*/
	if (vdevp->cma_config_en != 1) {
		vdevp->mem_start = mem_start;
		vdevp->mem_size = mem_end - mem_start + 1;
		pr_info("vdin%d mem_start = 0x%lx, mem_size = 0x%x\n",
			vdevp->index, vdevp->mem_start, vdevp->mem_size);
	}

	/* get irq from resource */
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
	vdevp->irq = res->start;
	snprintf(vdevp->irq_name, sizeof(vdevp->irq_name),
			"vdin%d-irq", vdevp->index);
	pr_info("vdin%d irq: %d rdma irq: %d\n", vdevp->index,
			vdevp->irq, vdevp->rdma_irq);

	/*set color_depth_mode*/
	ret = of_property_read_u32(pdev->dev.of_node,
		"tv_bit_mode", &bit_mode);
	if (ret)
		pr_info("no bit mode found, set 8bit as default\n");

	vdevp->color_depth_support = bit_mode;
	vdevp->color_depth_config = 0;

	if (vdevp->color_depth_support&VDIN_WR_COLOR_DEPTH_10BIT_FULL_PCAK_MODE)
		vdevp->color_depth_mode = 1;
	else
		vdevp->color_depth_mode = 0;

	/*vdin urgent en*/
	ret = of_property_read_u32(pdev->dev.of_node,
			"urgent_en", &urgent_en);
	if (ret) {
		vdevp->urgent_en = 0;
		pr_info("no urgent_en found\n");
	} else
		vdevp->urgent_en = urgent_en;
	/* init vdin parameters */
	vdevp->flags = VDIN_FLAG_NULL;
	vdevp->flags &= (~VDIN_FLAG_FS_OPENED);
	mutex_init(&vdevp->fe_lock);
	spin_lock_init(&vdevp->isr_lock);
	vdevp->frontend = NULL;

	/* @todo vdin_addr_offset */
	if (is_meson_gxbb_cpu() && vdevp->index)
		vdin_addr_offset[vdevp->index] = 0x70;
	else if (is_meson_g12a_cpu() && vdevp->index)
		vdin_addr_offset[vdevp->index] = 0x100;
	vdevp->addr_offset = vdin_addr_offset[vdevp->index];
	vdevp->flags = 0;
	/*canvas align number*/
	if (is_meson_g12a_cpu())
		vdevp->canvas_align = 64;
	else
		vdevp->canvas_align = 32;
	/*mif reset patch for vdin wr ram bug on gxtvbb*/
	if (is_meson_gxtvbb_cpu())
		enable_reset = 1;
	else
		enable_reset = 0;
	/* 1: gxtvbb vdin out full range, */
	/* 0: >=txl vdin out limit range, */
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB)
		vdevp->color_range_mode = 1;
	else
		vdevp->color_range_mode = 0;
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

	vf_provider_init(&vdevp->dv.vprov_dv, "dv_vdin",
		&vdin_vf_ops, vdevp->vfp);
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
	vdin_clk_onoff(vdevp, false);
	/*enable auto cutwindow for atv*/
	if (vdevp->index == 0) {
		vdevp->auto_cutwindow_en = 1;
		vdevp->auto_ratio_en = 1;
		#ifdef CONFIG_CMA
		vdevp->cma_mem_mode = 1;
		#endif
	}
	vdevp->rdma_enable = 1;
	/*set vdin_dev_s size*/
	vdevp->vdin_dev_ssize = sizeof(struct vdin_dev_s);
	vdevp->game_mode = game_mode;
	vdevp->canvas_config_mode = canvas_config_mode;
	INIT_DELAYED_WORK(&vdevp->dv.dv_dwork, vdin_dv_dwork);

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
fail_kzalloc_vdev:
	return ret;
}

/*this function is used for removing driver
 *	free the vframe pool
 *	remove device files
 *	delet vdinx device
 *	free drvdata
 */
static int vdin_drv_remove(struct platform_device *pdev)
{
	struct vdin_dev_s *vdevp;
	vdevp = platform_get_drvdata(pdev);

#ifdef CONFIG_AML_RDMA
	rdma_unregister(vdevp->rdma_handle);
#endif
	mutex_destroy(&vdevp->fe_lock);

	vf_pool_free(vdevp->vfp);
	vdin_remove_device_files(vdevp->dev);
	vdin_delete_device(vdevp->index);
	cdev_del(&vdevp->cdev);
	vdin_devp[vdevp->index] = NULL;

	/* free drvdata */
	dev_set_drvdata(vdevp->dev, NULL);
	platform_set_drvdata(pdev, NULL);
	kfree(vdevp);
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
	vdevp->flags |= VDIN_FLAG_SUSPEND;
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
	vdevp->flags &= (~VDIN_FLAG_SUSPEND);
	pr_info("%s ok.\n", __func__);
	return 0;
}
#endif

static void vdin_drv_shutdown(struct platform_device *pdev)
{
	struct vdin_dev_s *vdevp;

	vdevp = platform_get_drvdata(pdev);
	vdevp->flags |= VDIN_FLAG_SM_DISABLE;
	vdin_enable_module(vdevp->addr_offset, false);
	pr_info("%s ok.\n", __func__);
	return;
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
#ifdef DEBUG_SUPPORT
	ret = vdin_create_class_files(vdin_clsp);
	if (ret != 0) {
		pr_err("%s: failed to create class !!\n", __func__);
		goto fail_pdrv_register;
	}
#endif
	ret = platform_driver_register(&vdin_driver);

	if (ret != 0) {
		pr_err("%s: failed to register driver\n", __func__);
		goto fail_pdrv_register;
	}
	/*register vdin for v4l2 interface*/
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
#ifdef DEBUG_SUPPORT
	vdin_remove_class_files(vdin_clsp);
#endif
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
