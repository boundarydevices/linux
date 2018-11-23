/*
 * drivers/amlogic/media/vin/tvin/tvafe/tvafe.c
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
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
/*#include <asm/uaccess.h>*/
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/amlogic/iomap.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/clk.h>
/* Amlogic headers */
#include <linux/amlogic/media/canvas/canvas.h>
/*#include <mach/am_regs.h>*/
#include <linux/amlogic/media/vfm/vframe.h>

/* Local include */
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "../tvin_frontend.h"
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "tvafe_regs.h"
#include "tvafe_cvd.h"
#include "tvafe_general.h"
#include "tvafe.h"
#include "tvafe_debug.h"
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
#include "tvafe_avin_detect.h"
#endif
#include "../vdin/vdin_sm.h"


#define TVAFE_NAME               "tvafe"
#define TVAFE_DRIVER_NAME        "tvafe"
#define TVAFE_MODULE_NAME        "tvafe"
#define TVAFE_DEVICE_NAME        "tvafe"
#define TVAFE_CLASS_NAME         "tvafe"

static dev_t tvafe_devno;
static struct class *tvafe_clsp;
struct mutex pll_mutex;

#define TVAFE_TIMER_INTERVAL    (HZ/100)   /* 10ms, #define HZ 100 */
#define TVAFE_RATIO_CNT			40

static struct am_regs_s tvaferegs;
static struct tvafe_pin_mux_s tvafe_pinmux;
static struct meson_tvafe_data *s_tvafe_data;
static struct tvafe_clkgate_type tvafe_clkgate;

static bool enable_db_reg = true;
module_param(enable_db_reg, bool, 0644);
MODULE_PARM_DESC(enable_db_reg, "enable/disable tvafe load reg");

bool tvafe_dbg_enable;
module_param(tvafe_dbg_enable, bool, 0644);
MODULE_PARM_DESC(tvafe_dbg_enable, "enable/disable tvafe debug enable");

static int cutwindow_val_v = TVAFE_VS_VE_VAL;
static int cutwindow_val_v_level0 = 4;
static int cutwindow_val_v_level1 = 8;
static int cutwindow_val_v_level2 = 14;
static int cutwindow_val_v_level3 = 16;
static int cutwindow_val_v_level4 = 24;
static int cutwindow_val_h_level1 = 10;
static int cutwindow_val_h_level2 = 18;
static int cutwindow_val_h_level3 = 20;
static int cutwindow_val_h_level4 = 62;/*48-->62 for ntsc-m*/

/*1: snow function on;*/
/*0: off snow function*/
bool tvafe_snow_function_flag;

/*1: tvafe clk enabled;*/
/*0: tvafe clk disabled*/
/*read write cvd acd reg will crash when clk disabled*/
bool tvafe_clk_status;

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
/*opened port,1:av1, 2:av2, 0:none av*/
unsigned int avport_opened;
/*0:in, 1:out*/
unsigned int av1_plugin_state;
unsigned int av2_plugin_state;
#endif

#ifdef CONFIG_AMLOGIC_ATV_DEMOD
static struct tvafe_info_s *g_tvafe_info;
#endif

/*
 * tvafe check support port
 */
int tvafe_dec_support(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct tvafe_dev_s *devp = container_of(fe,
				struct tvafe_dev_s, frontend);

	/* check afe port and index */
	if (((port < TVIN_PORT_CVBS0) || (port > TVIN_PORT_CVBS3)) ||
		(fe->index != devp->index))
		return -1;

	return 0;
}

#ifdef CONFIG_CMA
void tvafe_cma_alloc(struct tvafe_dev_s *devp)
{
	unsigned int mem_size = devp->cma_mem_size;
	int flags = CODEC_MM_FLAGS_CMA_FIRST|CODEC_MM_FLAGS_CMA_CLEAR|
		CODEC_MM_FLAGS_CPU;
	if (devp->cma_config_en == 0)
		return;
	if (devp->cma_mem_alloc == 1)
		return;
	devp->cma_mem_alloc = 1;
	if (devp->cma_config_flag == 1) {
		devp->mem.start = codec_mm_alloc_for_dma("tvafe",
			mem_size/PAGE_SIZE, 0, flags);
		devp->mem.size = mem_size;
		if (devp->mem.start == 0)
			tvafe_pr_err("tvafe codec alloc fail!!!\n");
		else {
			tvafe_pr_info("mem_start = 0x%x, mem_size = 0x%x\n",
				devp->mem.start, devp->mem.size);
			tvafe_pr_info("codec cma alloc ok!\n");
		}
	} else if (devp->cma_config_flag == 0) {
		devp->venc_pages =
			dma_alloc_from_contiguous(&(devp->this_pdev->dev),
			mem_size >> PAGE_SHIFT, 0);
		if (devp->venc_pages) {
			devp->mem.start = page_to_phys(devp->venc_pages);
			devp->mem.size  = mem_size;
			tvafe_pr_info("mem_start = 0x%x, mem_size = 0x%x\n",
				devp->mem.start, devp->mem.size);
			tvafe_pr_info("cma alloc ok!\n");
		} else {
			tvafe_pr_err("tvafe cma mem undefined2.\n");
		}
	}
}

void tvafe_cma_release(struct tvafe_dev_s *devp)
{
	if (devp->cma_config_en == 0)
		return;
	if ((devp->cma_config_flag == 1) && devp->mem.start
		&& (devp->cma_mem_alloc == 1)) {
		codec_mm_free_for_dma("tvafe", devp->mem.start);
		devp->mem.start = 0;
		devp->mem.size = 0;
		devp->cma_mem_alloc = 0;
		tvafe_pr_info("codec cma release ok!\n");
	} else if (devp->venc_pages
		&& devp->cma_mem_size
		&& (devp->cma_mem_alloc == 1)
		&& (devp->cma_config_flag == 0)) {
		devp->cma_mem_alloc = 0;
		dma_release_from_contiguous(&(devp->this_pdev->dev),
			devp->venc_pages,
			devp->cma_mem_size>>PAGE_SHIFT);
		tvafe_pr_info("cma release ok!\n");
	}
}
#endif

#ifdef CONFIG_AMLOGIC_ATV_DEMOD
static int tvafe_get_v_fmt(void)
{
	int fmt = 0;

	if (tvin_get_sm_status(0) != TVIN_SM_STATUS_STABLE) {
		tvafe_pr_info("%s tvafe is not STABLE\n", __func__);
		return 0;
	}
	if (g_tvafe_info)
		fmt = tvafe_cvd2_get_format(&g_tvafe_info->cvd2);
	return fmt;
}
#endif
/*
 * tvafe open port and init register
 */
int tvafe_dec_open(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;

	mutex_lock(&devp->afe_mutex);
	if (devp->flags & TVAFE_FLAG_DEV_OPENED) {

		tvafe_pr_err("%s(%d), %s opened already\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return 1;
	}
	if ((port < TVIN_PORT_CVBS0) || (port > TVIN_PORT_CVBS3)) {

		tvafe_pr_err("%s(%d), %s unsupport\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return 1;
	}

#ifdef CONFIG_CMA
	tvafe_cma_alloc(devp);
#endif
	/* init variable */
	memset(tvafe, 0, sizeof(struct tvafe_info_s));
	/**enable and reset tvafe clock**/
	tvafe_enable_module(true);
	devp->flags &= (~TVAFE_POWERDOWN_IN_IDLE);

	/* set APB bus register accessing error exception */
	tvafe_set_apb_bus_err_ctrl();
	/**set cvd2 reset to high**/
	/*tvafe_cvd2_hold_rst();?????*/

	/* init tvafe registers */
	tvafe_init_reg(&tvafe->cvd2, &devp->mem, port, devp->pinmux);

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
	/*only txlx chip enabled*/
	if (tvafe_cpu_type() == CPU_TYPE_TXLX ||
		tvafe_cpu_type() == CPU_TYPE_TL1) {
		/*synctip set to 0 when tvafe working&&av connected*/
		/*enable clamp if av connected*/
		if (port == TVIN_PORT_CVBS1) {
			avport_opened = TVAFE_PORT_AV1;
			if (av1_plugin_state == 0) {
				W_APB_BIT(TVFE_CLAMP_INTF, 1,
						CLAMP_EN_BIT, CLAMP_EN_WID);
				/*channel1*/
				tvafe_cha1_SYNCTIP_close_config();
			} else
				W_APB_BIT(TVFE_CLAMP_INTF, 0,
						CLAMP_EN_BIT, CLAMP_EN_WID);
		} else if (port == TVIN_PORT_CVBS2) {
			avport_opened = TVAFE_PORT_AV2;
			if (av2_plugin_state == 0) {
				W_APB_BIT(TVFE_CLAMP_INTF, 1,
						CLAMP_EN_BIT, CLAMP_EN_WID);
				/*channel2*/
				tvafe_cha2_SYNCTIP_close_config();
			} else
				W_APB_BIT(TVFE_CLAMP_INTF, 0,
						CLAMP_EN_BIT, CLAMP_EN_WID);
		} else {
			avport_opened = 0;
			W_APB_BIT(TVFE_CLAMP_INTF, 1,
					CLAMP_EN_BIT, CLAMP_EN_WID);
		}
	} else
		W_APB_BIT(TVFE_CLAMP_INTF, 1,
					CLAMP_EN_BIT, CLAMP_EN_WID);
#else
	W_APB_BIT(TVFE_CLAMP_INTF, 1,
					CLAMP_EN_BIT, CLAMP_EN_WID);
#endif
	tvafe->parm.port = port;

	/* set the flag to enabble ioctl access */
	devp->flags |= TVAFE_FLAG_DEV_OPENED;
	tvafe_clk_status = true;
#ifdef CONFIG_AMLOGIC_ATV_DEMOD
	g_tvafe_info = tvafe;
	/* register aml_fe hook for atv search */
	aml_fe_hook_cvd(tvafe_cvd2_get_atv_format, tvafe_cvd2_get_hv_lock,
		tvafe_get_v_fmt);
#endif
	tvafe_pr_info("%s open port:0x%x ok.\n", __func__, port);

	mutex_unlock(&devp->afe_mutex);

	return 0;
}

/*
 * tvafe start after signal stable
 */
void tvafe_dec_start(struct tvin_frontend_s *fe, enum tvin_sig_fmt_e fmt)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = devp->tvafe.parm.port;

	mutex_lock(&devp->afe_mutex);
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		tvafe_pr_err("tvafe_dec_start(%d) decode havn't opened\n",
			devp->index);
		mutex_unlock(&devp->afe_mutex);
		return;
	}
	if ((port < TVIN_PORT_CVBS0) || (port > TVIN_PORT_CVBS3)) {

		tvafe_pr_err("%s(%d), %s unsupport\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return;
	}

	if (devp->flags & TVAFE_FLAG_DEV_STARTED) {

		tvafe_pr_err("%s(%d), %s started already\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return;
	}

	/*fix vg877 machine ntsc443 flash*/
	if ((fmt == TVIN_SIG_FMT_CVBS_NTSC_443) &&
		((port == TVIN_PORT_CVBS1) || (port == TVIN_PORT_CVBS2)))
		W_APB_REG(CVD2_H_LOOP_MAXSTATE, 0x9);

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
	if (tvafe_cpu_type() == CPU_TYPE_TXLX) {
		if (port == TVIN_PORT_CVBS1)
			tvafe_avin_detect_ch1_anlog_enable(0);
		else if (port == TVIN_PORT_CVBS2)
			tvafe_avin_detect_ch2_anlog_enable(0);
	}
#endif

	tvafe->parm.info.fmt = fmt;
	tvafe->parm.info.status = TVIN_SIG_STATUS_STABLE;

	devp->flags |= TVAFE_FLAG_DEV_STARTED;

	tvafe_pr_info("%s start fmt:%s ok.\n",
			__func__, tvin_sig_fmt_str(fmt));

	mutex_unlock(&devp->afe_mutex);
}

/*
 * tvafe stop port
 */
void tvafe_dec_stop(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;

	mutex_lock(&devp->afe_mutex);
	if (!(devp->flags & TVAFE_FLAG_DEV_STARTED)) {

		tvafe_pr_err("tvafe_dec_stop(%d) decode havn't started\n",
				devp->index);
		mutex_unlock(&devp->afe_mutex);
		return;
	}
	if ((port < TVIN_PORT_CVBS0) || (port > TVIN_PORT_CVBS3)) {

		tvafe_pr_err("%s(%d), %s unsupport\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return;
	}

	/* init variable */
	memset(&tvafe->cvd2.info, 0, sizeof(struct tvafe_cvd2_info_s));
	memset(&tvafe->parm.info, 0, sizeof(struct tvin_info_s));

	tvafe->parm.port = port;
	/* need to do ... */
	/** write 7740 register for cvbs clamp **/
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS3) &&
		!(devp->flags & TVAFE_POWERDOWN_IN_IDLE)) {

		tvafe->cvd2.fmt_loop_cnt = 0;
		/* reset loop cnt after channel switch */
#ifdef TVAFE_SET_CVBS_PGA_EN
		tvafe_cvd2_reset_pga();
#endif

#ifdef TVAFE_SET_CVBS_CDTO_EN
		tvafe_cvd2_set_default_cdto(&tvafe->cvd2);
#endif
		tvafe_cvd2_set_default_de(&tvafe->cvd2);
	}
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
	if (tvafe_cpu_type() == CPU_TYPE_TXLX) {
		if (port == TVIN_PORT_CVBS1)
			tvafe_avin_detect_ch1_anlog_enable(1);
		else if (port == TVIN_PORT_CVBS2)
			tvafe_avin_detect_ch2_anlog_enable(1);
	}
#endif

	devp->flags &= (~TVAFE_FLAG_DEV_STARTED);

	tvafe_pr_info("%s stop port:0x%x ok.\n", __func__, port);

	mutex_unlock(&devp->afe_mutex);
}

/*
 * tvafe close port
 */
void tvafe_dec_close(struct tvin_frontend_s *fe)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;

	tvafe_pr_info("%s begin to close afe.\n", __func__);
	mutex_lock(&devp->afe_mutex);
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED) ||
		(devp->flags & TVAFE_POWERDOWN_IN_IDLE)) {
		tvafe_pr_err("tvafe_dec_close(%d) decode havn't opened\n",
				devp->index);
		mutex_unlock(&devp->afe_mutex);
		return;
	}
	if ((tvafe->parm.port < TVIN_PORT_CVBS0) ||
		(tvafe->parm.port > TVIN_PORT_CVBS3)) {

		tvafe_pr_err("%s(%d), %s unsupport\n", __func__,
				devp->index, tvin_port_str(tvafe->parm.port));
		mutex_unlock(&devp->afe_mutex);
		return;
	}
	tvafe_clk_status = false;
	/*del_timer_sync(&devp->timer);*/
#ifdef CONFIG_AMLOGIC_ATV_DEMOD
	g_tvafe_info = NULL;
	/* register aml_fe hook for atv search */
	aml_fe_hook_cvd(NULL, NULL, NULL);
#endif
	/**set cvd2 reset to high**/
	tvafe_cvd2_hold_rst();
	/**disable av out**/
	tvafe_enable_avout(tvafe->parm.port, false);

#ifdef TVAFE_POWERDOWN_IN_IDLE
	/**disable tvafe clock**/
	devp->flags |= TVAFE_POWERDOWN_IN_IDLE;
	tvafe_enable_module(false);
	if (tvafe->parm.port == TVIN_PORT_CVBS3)
		adc_set_pll_cntl(0, ADC_EN_ATV_DEMOD, NULL);
	else if ((tvafe->parm.port >= TVIN_PORT_CVBS0) &&
		(tvafe->parm.port <= TVIN_PORT_CVBS2))
		adc_set_pll_cntl(0, ADC_EN_TVAFE, NULL);
#endif

#ifdef CONFIG_CMA
	tvafe_cma_release(devp);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
	if (tvafe_cpu_type() == CPU_TYPE_TXLX ||
		tvafe_cpu_type() == CPU_TYPE_TL1) {
		/*avsync tip set 1 to resume av detect*/
		if (tvafe->parm.port == TVIN_PORT_CVBS1) {
			avport_opened = 0;
			/*channel1*/
			tvafe_cha1_detect_restart_config();
		} else if (tvafe->parm.port == TVIN_PORT_CVBS2) {
			avport_opened = 0;
			/*channel2*/
			tvafe_cha2_detect_restart_config();
		} else {
			tvafe_cha1_detect_restart_config();
			tvafe_cha2_detect_restart_config();
		}
	}
#endif
	/* init variable */
	memset(tvafe, 0, sizeof(struct tvafe_info_s));

	devp->flags &= (~TVAFE_FLAG_DEV_STARTED);
	devp->flags &= (~TVAFE_FLAG_DEV_OPENED);

	tvafe_pr_info("%s close afe ok.\n", __func__);

	mutex_unlock(&devp->afe_mutex);
}

/*
 * tvafe vsync interrupt function
 */
int tvafe_dec_isr(struct tvin_frontend_s *fe, unsigned int hcnt64)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;
	enum tvin_aspect_ratio_e aspect_ratio = TVIN_ASPECT_NULL;
	static int count[10] = {0};
	int i;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED) ||
		(devp->flags & TVAFE_POWERDOWN_IN_IDLE)) {
		tvafe_pr_err("tvafe havn't opened, isr error!!!\n");
		return TVIN_BUF_SKIP;
	}

	if (force_stable)
		return TVIN_BUF_NULL;

	/* if there is any error or overflow, do some reset, then rerurn -1;*/
	if ((tvafe->parm.info.status != TVIN_SIG_STATUS_STABLE) ||
		(tvafe->parm.info.fmt == TVIN_SIG_FMT_NULL)) {
		if (devp->flags & TVAFE_FLAG_DEV_SNOW_FLAG)
			return TVIN_BUF_NULL;
		else
			return TVIN_BUF_SKIP;
	}

	/* TVAFE CVD2 3D works abnormally => reset cvd2 */
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS3))
		tvafe_cvd2_check_3d_comb(&tvafe->cvd2);

#ifdef TVAFE_SET_CVBS_PGA_EN
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS3) &&
		(port != TVIN_PORT_CVBS3))
		tvafe_cvd2_adj_pga(&tvafe->cvd2);
#endif
#ifdef TVAFE_SET_CVBS_CDTO_EN
	if (tvafe->parm.info.fmt == TVIN_SIG_FMT_CVBS_PAL_I)
		tvafe_cvd2_adj_cdto(&tvafe->cvd2, hcnt64);
#endif
	if (tvafe->parm.info.fmt == TVIN_SIG_FMT_CVBS_PAL_I)
		tvafe_cvd2_adj_hs(&tvafe->cvd2, hcnt64);
	else if (tvafe->parm.info.fmt == TVIN_SIG_FMT_CVBS_NTSC_M)
		tvafe_cvd2_adj_hs_ntsc(&tvafe->cvd2, hcnt64);

	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS3)) {
		aspect_ratio = tvafe_cvd2_get_wss();
		switch (aspect_ratio) {
		case TVIN_ASPECT_NULL:
			count[TVIN_ASPECT_NULL]++;
			break;
		case TVIN_ASPECT_1x1:
			count[TVIN_ASPECT_1x1]++;
			break;
		case TVIN_ASPECT_4x3_FULL:
			count[TVIN_ASPECT_4x3_FULL]++;
			break;
		case TVIN_ASPECT_14x9_FULL:
			count[TVIN_ASPECT_14x9_FULL]++;
			break;
		case TVIN_ASPECT_14x9_LB_CENTER:
			count[TVIN_ASPECT_14x9_LB_CENTER]++;
			break;
		case TVIN_ASPECT_14x9_LB_TOP:
			count[TVIN_ASPECT_14x9_LB_TOP]++;
			break;
		case TVIN_ASPECT_16x9_FULL:
			count[TVIN_ASPECT_16x9_FULL]++;
			break;
		case TVIN_ASPECT_16x9_LB_CENTER:
			count[TVIN_ASPECT_16x9_LB_CENTER]++;
			break;
		case TVIN_ASPECT_16x9_LB_TOP:
			count[TVIN_ASPECT_16x9_LB_TOP]++;
			break;
		case TVIN_ASPECT_MAX:
			break;
		}
		/*over 30/40 times,ratio is effective*/
		if (++(tvafe->aspect_ratio_cnt) > TVAFE_RATIO_CNT) {
			for (i = 0; i < TVIN_ASPECT_MAX; i++) {
				if (count[i] > 30) {
					tvafe->aspect_ratio = i;
					break;
				}
			}
			for (i = 0; i < TVIN_ASPECT_MAX; i++)
				count[i] = 0;
			tvafe->aspect_ratio_cnt = 0;
		}
	}
	return TVIN_BUF_NULL;
}

static struct tvin_decoder_ops_s tvafe_dec_ops = {
	.support    = tvafe_dec_support,
	.open       = tvafe_dec_open,
	.start      = tvafe_dec_start,
	.stop       = tvafe_dec_stop,
	.close      = tvafe_dec_close,
	.decode_isr = tvafe_dec_isr,
	.callmaster_det = NULL,
};

/*
 * tvafe signal signal status: signal on/off
 */
bool tvafe_is_nosig(struct tvin_frontend_s *fe)
{
	bool ret = false;
	/* Get the per-device structure that contains this frontend */
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED) ||
		(devp->flags & TVAFE_POWERDOWN_IN_IDLE)) {
		tvafe_pr_err("tvafe havn't opened OR suspend:flags:0x%x!!!\n",
			devp->flags);
		return true;
	}
	if (force_stable)
		return ret;
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS3)) {
		ret = tvafe_cvd2_no_sig(&tvafe->cvd2, &devp->mem);

		/* normal sigal & adc reg error, reload source mux */
		if (tvafe->cvd2.info.adc_reload_en && !ret)
			tvafe_set_source_muxing(port, devp->pinmux);
	}

	return ret;
}

/*
 * tvafe signal mode status: change/unchange
 */
bool tvafe_fmt_chg(struct tvin_frontend_s *fe)
{
	bool ret = false;
	/* Get the per-device structure that contains this frontend */
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {
		tvafe_pr_err("tvafe havn't opened, get fmt chg error!!!\n");
		return true;
	}
	if (force_stable)
		return ret;
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS3))
		ret = tvafe_cvd2_fmt_chg(&tvafe->cvd2);

	return ret;
}

/*
 * tvafe adc lock status: lock/unlock
 */
bool tvafe_pll_lock(struct tvin_frontend_s *fe)
{
	bool ret = true;

	return ret;
}

/*
 * tvafe search format number
 */
enum tvin_sig_fmt_e tvafe_get_fmt(struct tvin_frontend_s *fe)
{
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
	/* Get the per-device structure that contains this frontend */
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		tvafe_pr_err("tvafe havn't opened, get sig fmt error!!!\n");
		return fmt;
	}
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS3))
		fmt = tvafe_cvd2_get_format(&tvafe->cvd2);

	tvafe->parm.info.fmt = fmt;
	if (tvafe_dbg_enable)
		tvafe_pr_info("%s fmt:%s.\n", __func__,
			tvin_sig_fmt_str(fmt));

	return fmt;
}

/*
 * tvafe signal property: 2D/3D, color format, aspect ratio, pixel repeat
 */
void tvafe_get_sig_property(struct tvin_frontend_s *fe,
		struct tvin_sig_property_s *prop)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;
	unsigned int hs_adj_lev = cutwindow_val_h_level1;
	unsigned int vs_adj_lev = cutwindow_val_v_level1;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED) ||
		(devp->flags & TVAFE_POWERDOWN_IN_IDLE)) {
		tvafe_pr_err("%s tvafe not opened OR suspend:flags:0x%x!\n",
			__func__, devp->flags);
		return;
	}
	prop->trans_fmt = TVIN_TFMT_2D;
	prop->color_format = TVIN_YUV444;
	prop->dest_cfmt = TVIN_YUV422;

#ifdef TVAFE_CVD2_AUTO_DE_ENABLE
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS3)) {
		if (tvafe->cvd2.info.vs_adj_en) {
			if (tvafe->cvd2.info.vs_adj_level == 0)
				vs_adj_lev = cutwindow_val_v_level0;
			else if (tvafe->cvd2.info.vs_adj_level == 1)
				vs_adj_lev = cutwindow_val_v_level1;
			else if (tvafe->cvd2.info.vs_adj_level == 2)
				vs_adj_lev = cutwindow_val_v_level2;
			else if (tvafe->cvd2.info.vs_adj_level == 3)
				vs_adj_lev = cutwindow_val_v_level3;
			else if (tvafe->cvd2.info.vs_adj_level == 4)
				vs_adj_lev = cutwindow_val_v_level4;
			else
				vs_adj_lev = 0;
			prop->vs = vs_adj_lev;
			prop->ve = vs_adj_lev;
		} else {
			prop->vs = 0;
			prop->ve = 0;
		}
		if (tvafe->cvd2.info.hs_adj_en) {
			if (tvafe->cvd2.info.hs_adj_level == 1)
				hs_adj_lev = cutwindow_val_h_level1;
			else if (tvafe->cvd2.info.hs_adj_level == 2)
				hs_adj_lev = cutwindow_val_h_level2;
			else if (tvafe->cvd2.info.hs_adj_level == 3)
				hs_adj_lev = cutwindow_val_h_level3;
			else if (tvafe->cvd2.info.hs_adj_level == 4) {
				hs_adj_lev = cutwindow_val_h_level4;
				prop->vs = cutwindow_val_v;
				prop->ve = cutwindow_val_v;
			} else
				hs_adj_lev = 0;
			if (tvafe->cvd2.info.hs_adj_dir == true) {
				prop->hs = 0;
				prop->he = hs_adj_lev;
			} else {
				prop->hs = hs_adj_lev;
				prop->he = 0;
			}
		} else {
			prop->hs = 0;
			prop->he = 0;
		}
	}
#endif
	prop->color_fmt_range = TVIN_YUV_LIMIT;
	prop->aspect_ratio = tvafe->aspect_ratio;
	prop->decimation_ratio = 0;
	prop->dvi_info = 0;
	/*4 is the test result@20171101 on fluke-54200 and DVD*/
	prop->skip_vf_num = 4;
}
/*
 *get cvbs secam source's phase
 */
static bool tvafe_cvbs_get_secam_phase(struct tvin_frontend_s *fe)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;

	if (tvafe->cvd2.config_fmt == TVIN_SIG_FMT_CVBS_SECAM)
		return tvafe->cvd2.hw.secam_phase;
	else
		return 0;

}

/**check frame skip,only for av input*/
static bool tvafe_cvbs_check_frame_skip(struct tvin_frontend_s *fe)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
		frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	struct tvafe_cvd2_s *cvd2 = &tvafe->cvd2;
	enum tvin_port_e port = tvafe->parm.port;
	bool ret = false;

	if (!devp->frame_skip_enable ||
		(devp->flags&TVAFE_FLAG_DEV_SNOW_FLAG)) {
		ret = false;
	} else if ((cvd2->hw.no_sig || !cvd2->hw.h_lock || !cvd2->hw.v_lock) &&
		((port >= TVIN_PORT_CVBS1) && (port <= TVIN_PORT_CVBS2))) {
		if (tvafe_dbg_enable)
			tvafe_pr_err("cvbs signal unstable, skip frame!!!\n");
		ret = true;
	}
	return ret;
}

static struct tvin_state_machine_ops_s tvafe_sm_ops = {
	.nosig            = tvafe_is_nosig,
	.fmt_changed      = tvafe_fmt_chg,
	.get_fmt          = tvafe_get_fmt,
	.fmt_config       = NULL,
	.adc_cal          = NULL,
	.pll_lock         = tvafe_pll_lock,
	.get_sig_property  = tvafe_get_sig_property,
	.vga_set_param    = NULL,
	.vga_get_param    = NULL,
	.check_frame_skip = tvafe_cvbs_check_frame_skip,
	.get_secam_phase = tvafe_cvbs_get_secam_phase,
};

static int tvafe_open(struct inode *inode, struct file *file)
{
	struct tvafe_dev_s *devp;

	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct tvafe_dev_s, cdev);
	file->private_data = devp;

	/* ... */

	tvafe_pr_info("%s: open device\n", __func__);

	return 0;
}

static int tvafe_release(struct inode *inode, struct file *file)
{
	struct tvafe_dev_s *devp = file->private_data;

	file->private_data = NULL;

	/* Release some other fields */
	/* ... */

	tvafe_pr_info("tvafe: device %d release ok.\n", devp->index);

	return 0;
}


static long tvafe_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	struct tvafe_dev_s *devp = file->private_data;
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvafe_cvbs_video_e cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_UNLOCKED;

	if (_IOC_TYPE(cmd) != _TM_T) {
		tvafe_pr_err("%s invalid command: %u\n", __func__, cmd);
		return -EINVAL;
	}

	/* tvafe_pr_info("%s command: %u\n", __func__, cmd); */
	if (disableapi)
		return -EPERM;

	mutex_lock(&devp->afe_mutex);
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		tvafe_pr_info("%s, tvafe device is disable, ignore the command %d\n",
				__func__, cmd);
		mutex_unlock(&devp->afe_mutex);
		return -EPERM;
	}

	switch (cmd) {
	case TVIN_IOC_LOAD_REG:
		{
		if (copy_from_user(&tvaferegs, argp,
			sizeof(struct am_regs_s))) {
			tvafe_pr_info("load reg errors: can't get buffer length\n");
			ret = -EINVAL;
			break;
		}

		if (!tvaferegs.length || (tvaferegs.length > 512)) {
			tvafe_pr_info("load regs error: buffer length overflow!!!, length=0x%x\n",
				tvaferegs.length);
			ret = -EINVAL;
			break;
		}
		if (enable_db_reg)
			tvafe_set_regmap(&tvaferegs);

		break;
		}
	case TVIN_IOC_S_AFE_SONWON:
		devp->flags |= TVAFE_FLAG_DEV_SNOW_FLAG;
		tvafe_snow_function_flag = true;
		tvafe_snow_config(1);
		tvafe_snow_config_clamp(1);
		if (tvafe_dbg_enable)
			tvafe_pr_info("TVIN_IOC_S_AFE_SONWON\n");
		break;
	case TVIN_IOC_S_AFE_SONWOFF:
		tvafe_snow_config(0);
		tvafe_snow_config_clamp(0);
		devp->flags &= (~TVAFE_FLAG_DEV_SNOW_FLAG);
		if (tvafe_dbg_enable)
			tvafe_pr_info("TVIN_IOC_S_AFE_SONWOFF\n");
		break;
	case TVIN_IOC_G_AFE_CVBS_LOCK:
		{
			cvbs_lock_status =
			tvafe_cvd2_get_lock_status(&tvafe->cvd2);
			if (copy_to_user(argp,
				&cvbs_lock_status, sizeof(int))) {

				ret = -EFAULT;
				break;
			}
			tvafe_pr_info("%s: get cvd2 lock status :%d.\n",
				__func__, cvbs_lock_status);
			break;
		}
	case TVIN_IOC_S_AFE_CVBS_STD:
		{
			enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;

			if (copy_from_user(&fmt, argp,
				sizeof(enum tvin_sig_fmt_e))) {
				ret = -EFAULT;
				break;
			}
			tvafe->cvd2.manual_fmt = fmt;
			tvafe_pr_info("%s: ioctl set cvd2 manual fmt:%s.\n",
				__func__, tvin_sig_fmt_str(fmt));
			break;
		}
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	mutex_unlock(&devp->afe_mutex);
	return ret;
}
#ifdef CONFIG_COMPAT
static long tvafe_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = tvafe_ioctl(file, cmd, arg);
	return ret;
}
#endif

/* File operations structure. Defined in linux/fs.h */
static const struct file_operations tvafe_fops = {
	.owner   = THIS_MODULE,         /* Owner */
	.open    = tvafe_open,          /* Open method */
	.release = tvafe_release,       /* Release method */
	.unlocked_ioctl   = tvafe_ioctl,         /* Ioctl method */
#ifdef CONFIG_COMPAT
	.compat_ioctl = tvafe_compat_ioctl,
#endif
	/* ... */
};

static int tvafe_add_cdev(struct cdev *cdevp,
		const struct file_operations *fops, int minor)
{
	int ret;
	dev_t devno = MKDEV(MAJOR(tvafe_devno), minor);

	cdev_init(cdevp, fops);
	cdevp->owner = THIS_MODULE;
	ret = cdev_add(cdevp, devno, 1);
	return ret;
}

static struct device *tvafe_create_device(struct device *parent, int id)
{
	dev_t devno = MKDEV(MAJOR(tvafe_devno),  id);

	return device_create(tvafe_clsp, parent, devno, NULL, "%s0",
			TVAFE_DEVICE_NAME);
	/* @to do this after Middleware API modified */
	/*return device_create(tvafe_clsp, parent, devno, NULL, "%s",*/
	/*  TVAFE_DEVICE_NAME); */
}

static void tvafe_delete_device(int minor)
{
	dev_t devno = MKDEV(MAJOR(tvafe_devno), minor);

	device_destroy(tvafe_clsp, devno);
}
void __iomem *tvafe_reg_base;

int tvafe_reg_read(unsigned int reg, unsigned int *val)
{
	*val = readl(tvafe_reg_base+reg);
	return 0;
}
EXPORT_SYMBOL(tvafe_reg_read);

int tvafe_reg_write(unsigned int reg, unsigned int val)
{
	writel(val, (tvafe_reg_base+reg));
	return 0;
}
EXPORT_SYMBOL(tvafe_reg_write);

int tvafe_vbi_reg_read(unsigned int reg, unsigned int *val)
{
	if (tvafe_clk_status)
		*val = readl(tvafe_reg_base+reg);
	else
		return -1;
	return 0;
}
EXPORT_SYMBOL(tvafe_vbi_reg_read);

int tvafe_vbi_reg_write(unsigned int reg, unsigned int val)
{
	if (tvafe_clk_status)
		writel(val, (tvafe_reg_base+reg));
	else
		return -1;
	return 0;
}
EXPORT_SYMBOL(tvafe_vbi_reg_write);

int tvafe_hiu_reg_read(unsigned int reg, unsigned int *val)
{
	*val =  aml_read_hiubus(reg);
	return 0;
}
EXPORT_SYMBOL(tvafe_hiu_reg_read);

int tvafe_hiu_reg_write(unsigned int reg, unsigned int val)
{
	aml_write_hiubus(reg, val);
	return 0;
}
EXPORT_SYMBOL(tvafe_hiu_reg_write);

int tvafe_cpu_type(void)
{
	return s_tvafe_data->cpu_id;
}
EXPORT_SYMBOL(tvafe_cpu_type);

void tvafe_clk_gate_ctrl(int status)
{
	if (status) {
		if (tvafe_clkgate.clk_gate_state) {
			tvafe_pr_info("clk_gate is already on\n");
			return;
		}

		if (IS_ERR(tvafe_clkgate.vdac_clk_gate))
			tvafe_pr_err("error: %s: vdac_clk_gate\n", __func__);
		else
			clk_prepare_enable(tvafe_clkgate.vdac_clk_gate);

		tvafe_clkgate.clk_gate_state = 1;
	} else {
		if (tvafe_clkgate.clk_gate_state == 0) {
			tvafe_pr_info("clk_gate is already off\n");
			return;
		}

		if (IS_ERR(tvafe_clkgate.vdac_clk_gate))
			tvafe_pr_err("error: %s: vdac_clk_gate\n", __func__);
		else
			clk_disable_unprepare(tvafe_clkgate.vdac_clk_gate);

		tvafe_clkgate.clk_gate_state = 0;
	}
}

static void tvafe_clktree_probe(struct device *dev)
{
	tvafe_clkgate.clk_gate_state = 0;

	tvafe_clkgate.vdac_clk_gate = devm_clk_get(dev, "vdac_clk_gate");
	if (IS_ERR(tvafe_clkgate.vdac_clk_gate))
		tvafe_pr_err("error: %s: clk vdac_clk_gate\n", __func__);
}

struct meson_tvafe_data meson_gxtvbb_tvafe_data = {
	.cpu_id = CPU_TYPE_GXTVBB,
	.name = "meson-gxtvbb-tvafe",
};

struct meson_tvafe_data meson_txl_tvafe_data = {
	.cpu_id = CPU_TYPE_TXL,
	.name = "meson-txl-tvafe",
};

struct meson_tvafe_data meson_txlx_tvafe_data = {
	.cpu_id = CPU_TYPE_TXLX,
	.name = "meson-txlx-tvafe",
};

struct meson_tvafe_data meson_txhd_tvafe_data = {
	.cpu_id = CPU_TYPE_TXHD,
	.name = "meson-txhd-tvafe",
};

struct meson_tvafe_data meson_tl1_tvafe_data = {
	.cpu_id = CPU_TYPE_TL1,
	.name = "meson-tl1-tvafe",
};

static const struct of_device_id meson_tvafe_dt_match[] = {
	{
		.compatible = "amlogic, tvafe-gxtvbb",
		.data		= &meson_gxtvbb_tvafe_data,
	}, {
		.compatible = "amlogic, tvafe-txl",
		.data		= &meson_txl_tvafe_data,
	}, {
		.compatible = "amlogic, tvafe-txlx",
		.data		= &meson_txlx_tvafe_data,
	}, {
		.compatible = "amlogic, tvafe-txhd",
		.data		= &meson_txhd_tvafe_data,
	}, {
		.compatible = "amlogic, tvafe-tl1",
		.data		= &meson_tl1_tvafe_data,
	},
	{},
};

static unsigned int tvafe_use_reserved_mem;
static struct resource tvafe_memobj;
static int tvafe_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tvafe_dev_s *tdevp;
	int size_io_reg;
	/*const void *name;*/
	/*int offset, size, mem_size_m;*/
	struct resource *res;
	const struct of_device_id *match;
	/* struct tvin_frontend_s * frontend; */

	match = of_match_device(meson_tvafe_dt_match, &pdev->dev);
	if (match == NULL) {
		tvafe_pr_err("%s,no matched table\n", __func__);
		return -1;
	}

	s_tvafe_data = (struct meson_tvafe_data *)match->data;
	tvafe_pr_info("%s:cpu_id:%d,name:%s\n", __func__,
		s_tvafe_data->cpu_id, s_tvafe_data->name);

	tvafe_clktree_probe(&pdev->dev);

	/* allocate memory for the per-device structure */
	tdevp = kzalloc(sizeof(struct tvafe_dev_s), GFP_KERNEL);
	if (!tdevp)
		goto fail_kzalloc_tdev;

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
					"tvafe_id", &(tdevp->index));
		if (ret) {
			tvafe_pr_err("Can't find  tvafe id.\n");
			goto fail_get_id;
		}
	}
	tdevp->flags = 0;

	/* create cdev and reigser with sysfs */
	ret = tvafe_add_cdev(&tdevp->cdev, &tvafe_fops, tdevp->index);
	if (ret) {
		tvafe_pr_err("%s: failed to add cdev\n", __func__);
		goto fail_add_cdev;
	}
	/* create /dev nodes */
	tdevp->dev = tvafe_create_device(&pdev->dev, tdevp->index);
	if (IS_ERR(tdevp->dev)) {
		tvafe_pr_err("failed to create device node\n");
		/* @todo do with error */
		ret = PTR_ERR(tdevp->dev);
		goto fail_create_device;
	}

	/*create sysfs attribute files*/
	ret = tvafe_device_create_file(tdevp->dev);
	if (ret < 0) {
		tvafe_pr_err("%s: create attribute files fail.\n",
			__func__);
		goto fail_create_dbg_file;
	}

	/* get device memory */
	/* res = platform_get_resource(pdev, IORESOURCE_MEM, 0); */
	res = &tvafe_memobj;
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret == 0)
		tvafe_pr_info("tvafe memory resource done.\n");
	else
		tvafe_pr_info("can't get memory resource\n");
#ifdef CONFIG_CMA
	if (!tvafe_use_reserved_mem) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"flag_cma", &(tdevp->cma_config_flag));
		if (ret) {
			tvafe_pr_err("don't find  match flag_cma\n");
			tdevp->cma_config_flag = 0;
		}
		if (tdevp->cma_config_flag == 1) {
			ret = of_property_read_u32(pdev->dev.of_node,
				"cma_size", &(tdevp->cma_mem_size));
			if (ret)
				tvafe_pr_err("don't find  match cma_size\n");
			else
				tdevp->cma_mem_size *= SZ_1M;
		} else if (tdevp->cma_config_flag == 0)
			tdevp->cma_mem_size =
				dma_get_cma_size_int_byte(&pdev->dev);
		tdevp->this_pdev = pdev;
		tdevp->cma_mem_alloc = 0;
		tdevp->cma_config_en = 1;
		tvafe_pr_info("cma_mem_size = %d MB\n",
				(u32)tdevp->cma_mem_size/SZ_1M);
	}
#endif
	tvafe_use_reserved_mem = 0;
	if (tdevp->cma_config_en != 1) {
		tdevp->mem.start = res->start;
		tdevp->mem.size = res->end - res->start + 1;
		tvafe_pr_info("tvafe cvd memory addr is:0x%x, cvd mem_size is:0x%x .\n",
				tdevp->mem.start,
				tdevp->mem.size);
	}
	if (of_property_read_u32_array(pdev->dev.of_node, "tvafe_pin_mux",
			(u32 *)tvafe_pinmux.pin, TVAFE_SRC_SIG_MAX_NUM)) {
		tvafe_pr_err("Can't get pinmux data.\n");
	}
	tdevp->pinmux = &tvafe_pinmux;
	if (!tdevp->pinmux) {
		tvafe_pr_err("tvafe: no platform data!\n");
		ret = -ENODEV;
	}

	if (of_get_property(pdev->dev.of_node, "pinctrl-names", NULL)) {
		struct pinctrl *p = devm_pinctrl_get_select_default(&pdev->dev);

		if (IS_ERR(p))
			tvafe_pr_err("tvafe request pinmux error!\n");

	}

	/*reg mem*/
	tvafe_pr_info("%s:tvafe start get  ioremap .\n", __func__);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing memory resource\n");
		return -ENODEV;
	}
	size_io_reg = resource_size(res);
	tvafe_pr_info("%s:tvafe reg base=%p,size=%x\n",
		__func__, (void *)res->start, size_io_reg);
	if (!devm_request_mem_region(&pdev->dev,
				res->start, size_io_reg, pdev->name)) {
		dev_err(&pdev->dev, "Memory region busy\n");
		return -EBUSY;
	}
	tvafe_reg_base =
		devm_ioremap_nocache(&pdev->dev, res->start, size_io_reg);
	if (!tvafe_reg_base) {
		dev_err(&pdev->dev, "tvafe ioremap failed\n");
		return -ENOMEM;
	}
	tvafe_pr_info("%s: tvafe maped reg_base =%p, size=%x\n",
			__func__, tvafe_reg_base, size_io_reg);

	/* frontend */
	tvin_frontend_init(&tdevp->frontend, &tvafe_dec_ops,
						&tvafe_sm_ops, tdevp->index);
	sprintf(tdevp->frontend.name, "%s", TVAFE_NAME);
	tvin_reg_frontend(&tdevp->frontend);

	mutex_init(&tdevp->afe_mutex);
	mutex_init(&pll_mutex);

	dev_set_drvdata(tdevp->dev, tdevp);
	platform_set_drvdata(pdev, tdevp);

	/**disable tvafe clock**/
	tdevp->flags |= TVAFE_POWERDOWN_IN_IDLE;
	tvafe_enable_module(false);

	/*init tvafe param*/
	tdevp->frame_skip_enable = 1;

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
	avport_opened = 0;
	av1_plugin_state = 0;
	av2_plugin_state = 0;
#endif

	tdevp->sizeof_tvafe_dev_s = sizeof(struct tvafe_dev_s);

	disableapi = false;
	force_stable = false;

	tvafe_pr_info("driver probe ok\n");

	return 0;

fail_create_dbg_file:
	tvafe_delete_device(tdevp->index);
fail_create_device:
	cdev_del(&tdevp->cdev);
fail_add_cdev:
fail_get_id:
	kfree(tdevp);
fail_kzalloc_tdev:
	tvafe_pr_err("tvafe: kzalloc memory failed.\n");
	return ret;

}

static int tvafe_drv_remove(struct platform_device *pdev)
{
	struct tvafe_dev_s *tdevp;

	tdevp = platform_get_drvdata(pdev);
	if (tvafe_reg_base) {
		devm_iounmap(&pdev->dev, tvafe_reg_base);
		devm_release_mem_region(&pdev->dev, tvafe_memobj.start,
			resource_size(&tvafe_memobj));
	}
	mutex_destroy(&tdevp->afe_mutex);
	mutex_destroy(&pll_mutex);
	tvin_unreg_frontend(&tdevp->frontend);
	tvafe_remove_device_files(tdevp->dev);
	tvafe_delete_device(tdevp->index);
	cdev_del(&tdevp->cdev);
	kfree(tdevp);
	tvafe_pr_info("driver removed ok.\n");
	return 0;
}

#ifdef CONFIG_PM
static int tvafe_drv_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct tvafe_dev_s *tdevp;
	struct tvafe_info_s *tvafe;

	tdevp = platform_get_drvdata(pdev);
	tvafe = &tdevp->tvafe;
	/* close afe port first */
	if (tdevp->flags & TVAFE_FLAG_DEV_OPENED) {

		tvafe_pr_info("suspend module, close afe port first\n");
		/* tdevp->flags &= (~TVAFE_FLAG_DEV_OPENED); */
		/*del_timer_sync(&tdevp->timer);*/

		/**set cvd2 reset to high**/
		tvafe_cvd2_hold_rst();
		/**disable av out**/
		tvafe_enable_avout(tvafe->parm.port, false);
	}
	/*disable and reset tvafe clock*/
	tdevp->flags |= TVAFE_POWERDOWN_IN_IDLE;
	tvafe_clk_status = false;
	tvafe_enable_module(false);
	adc_set_pll_reset();

	tvafe_pr_info("suspend module\n");

	return 0;
}

static int tvafe_drv_resume(struct platform_device *pdev)
{
	struct tvafe_dev_s *tdevp;

	tdevp = platform_get_drvdata(pdev);
	/*disable and reset tvafe clock*/
	adc_set_pll_reset();
	tvafe_enable_module(true);
	tdevp->flags &= (~TVAFE_POWERDOWN_IN_IDLE);
	tvafe_clk_status = false;
	tvafe_pr_info("resume module\n");
	return 0;
}
#endif

static void tvafe_drv_shutdown(struct platform_device *pdev)
{
	tvafe_pr_info("tvafe_drv_shutdown ok.\n");
}

static struct platform_driver tvafe_driver = {
	.probe      = tvafe_drv_probe,
	.remove     = tvafe_drv_remove,
#ifdef CONFIG_PM
	.suspend    = tvafe_drv_suspend,
	.resume     = tvafe_drv_resume,
#endif
	.shutdown   = tvafe_drv_shutdown,
	.driver     = {
		.name   = TVAFE_DRIVER_NAME,
		.of_match_table = meson_tvafe_dt_match,
	}
};

static int __init tvafe_drv_init(void)
{
	int ret = 0;

	ret = alloc_chrdev_region(&tvafe_devno, 0, 1, TVAFE_NAME);
	if (ret < 0) {
		tvafe_pr_err("%s: failed to allocate major number\n", __func__);
		goto fail_alloc_cdev_region;
	}
	tvafe_pr_info("%s: major %d\n", __func__, MAJOR(tvafe_devno));

	tvafe_clsp = class_create(THIS_MODULE, TVAFE_NAME);
	if (IS_ERR(tvafe_clsp)) {
		ret = PTR_ERR(tvafe_clsp);
		tvafe_pr_err("%s: failed to create class\n", __func__);
		goto fail_class_create;
	}

	ret = platform_driver_register(&tvafe_driver);
	if (ret != 0) {
		tvafe_pr_err("%s: failed to register driver\n", __func__);
		goto fail_pdrv_register;
	}
	tvafe_pr_info("tvafe_drv_init.\n");
	return 0;

fail_pdrv_register:
	class_destroy(tvafe_clsp);
fail_class_create:
	unregister_chrdev_region(tvafe_devno, 1);
fail_alloc_cdev_region:
	return ret;


}

static void __exit tvafe_drv_exit(void)
{
	class_destroy(tvafe_clsp);
	unregister_chrdev_region(tvafe_devno, 1);
	platform_driver_unregister(&tvafe_driver);
	tvafe_pr_info("tvafe_drv_exit.\n");
}

static int tvafe_mem_device_init(struct reserved_mem *rmem,
		struct device *dev)
{
	s32 ret = 0;
	struct resource *res = NULL;

	if (!rmem) {
		tvafe_pr_info("Can't get reverse mem!\n");
		ret = -EFAULT;
		return ret;
	}
	res = &tvafe_memobj;
	res->start = rmem->base;
	res->end = rmem->base + rmem->size - 1;
	if (rmem->size >= 0x500000)
		tvafe_use_reserved_mem = 1;

	tvafe_pr_info("init tvafe memsource 0x%lx->0x%lx tvafe_use_reserved_mem:%d\n",
		(unsigned long)res->start, (unsigned long)res->end,
		tvafe_use_reserved_mem);

	return 0;
}

static const struct reserved_mem_ops rmem_tvafe_ops = {
	.device_init = tvafe_mem_device_init,
};

static int __init tvafe_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_tvafe_ops;
	tvafe_pr_info("share mem setup\n");
	return 0;
}

module_init(tvafe_drv_init);
module_exit(tvafe_drv_exit);

RESERVEDMEM_OF_DECLARE(tvafe, "amlogic, tvafe_memory",
	tvafe_mem_setup);

MODULE_VERSION(TVAFE_VER);

/*only for develop debug*/
#ifdef TVAFE_DEBUG
module_param(cutwindow_val_v, int, 0664);
MODULE_PARM_DESC(cutwindow_val_v, "cutwindow_val_v");

module_param(cutwindow_val_v_level0, int, 0664);
MODULE_PARM_DESC(cutwindow_val_v_level0, "cutwindow_val_v_level0");

module_param(cutwindow_val_v_level1, int, 0664);
MODULE_PARM_DESC(cutwindow_val_v_level1, "cutwindow_val_v_level1");

module_param(cutwindow_val_v_level2, int, 0664);
MODULE_PARM_DESC(cutwindow_val_v_level2, "cutwindow_val_v_level2");

module_param(cutwindow_val_v_level3, int, 0664);
MODULE_PARM_DESC(cutwindow_val_v_level3, "cutwindow_val_v_level3");

module_param(cutwindow_val_v_level4, int, 0664);
MODULE_PARM_DESC(cutwindow_val_v_level4, "cutwindow_val_v_level4");

module_param(cutwindow_val_h_level1, int, 0664);
MODULE_PARM_DESC(cutwindow_val_h_level1, "cutwindow_val_h_level1");

module_param(cutwindow_val_h_level2, int, 0664);
MODULE_PARM_DESC(cutwindow_val_h_level2, "cutwindow_val_h_level2");

module_param(cutwindow_val_h_level3, int, 0664);
MODULE_PARM_DESC(cutwindow_val_h_level3, "cutwindow_val_h_level3");

module_param(cutwindow_val_h_level4, int, 0664);
MODULE_PARM_DESC(cutwindow_val_h_level4, "cutwindow_val_h_level4");
#endif

MODULE_DESCRIPTION("AMLOGIC TVAFE driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xu Lin <lin.xu@amlogic.com>");

