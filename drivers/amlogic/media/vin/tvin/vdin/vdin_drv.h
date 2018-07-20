/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_drv.h
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

#ifndef __TVIN_VDIN_DRV_H
#define __TVIN_VDIN_DRV_H

/* Standard Linux Headers */
#include <linux/cdev.h>
#include <linux/irqreturn.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/workqueue.h>

/* Amlogic Headers */
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#ifdef CONFIG_AML_RDMA
#include <linux/amlogic/rdma/rdma_mgr.h>
#endif

/* Local Headers */
#include "../tvin_frontend.h"
#include "vdin_vf.h"
#include "vdin_regs.h"

#define VDIN_VER "Ref.2018/04/28"

/*the counter of vdin*/
#define VDIN_MAX_DEVS			2
/* #define VDIN_CRYSTAL               24000000 */
/* #define VDIN_MEAS_CLK              50000000 */
/* values of vdin_dev_t.flags */
#define VDIN_FLAG_NULL			0x00000000
#define VDIN_FLAG_DEC_INIT		0x00000001
#define VDIN_FLAG_DEC_STARTED		0x00000002
#define VDIN_FLAG_DEC_OPENED		0x00000004
#define VDIN_FLAG_DEC_REGED		0x00000008
#define VDIN_FLAG_DEC_STOP_ISR		0x00000010
#define VDIN_FLAG_FORCE_UNSTABLE	0x00000020
#define VDIN_FLAG_FS_OPENED		0x00000100
#define VDIN_FLAG_SKIP_ISR              0x00000200
/*flag for vdin0 output*/
#define VDIN_FLAG_OUTPUT_TO_NR		0x00000400
/*flag for force vdin buffer recycle*/
#define VDIN_FLAG_FORCE_RECYCLE         0x00000800
/*flag for vdin scale down,color fmt conversion*/
#define VDIN_FLAG_MANUAL_CONVERSION     0x00001000
/*flag for vdin rdma enable*/
#define VDIN_FLAG_RDMA_ENABLE           0x00002000
/*flag for vdin snow on&off*/
#define VDIN_FLAG_SNOW_FLAG             0x00004000
/*flag for disable vdin sm*/
#define VDIN_FLAG_SM_DISABLE            0x00008000
/*flag for vdin suspend state*/
#define VDIN_FLAG_SUSPEND               0x00010000
/*flag for vdin-v4l2 debug*/
#define VDIN_FLAG_V4L2_DEBUG            0x00020000
/*flag for isr req&free*/
#define VDIN_FLAG_ISR_REQ               0x00040000
/*values of vdin isr bypass check flag */
#define VDIN_BYPASS_STOP_CHECK          0x00000001
#define VDIN_BYPASS_CYC_CHECK           0x00000002
#define VDIN_BYPASS_VGA_CHECK           0x00000008
#define VDIN_CANVAS_MAX_CNT				9

/*flag for flush vdin buff*/
#define VDIN_FLAG_BLACK_SCREEN_ON	1
#define VDIN_FLAG_BLACK_SCREEN_OFF	0
/* size for rdma table */
#define RDMA_TABLE_SIZE (PAGE_SIZE>>3)
/* #define VDIN_DEBUG */

/*vdin write mem color-depth support*/
#define VDIN_WR_COLOR_DEPTH_8BIT	1
#define VDIN_WR_COLOR_DEPTH_9BIT	(1 << 1)
#define VDIN_WR_COLOR_DEPTH_10BIT	(1 << 2)
#define VDIN_WR_COLOR_DEPTH_12BIT	(1 << 3)
/*TXL new add*/
#define VDIN_WR_COLOR_DEPTH_10BIT_FULL_PCAK_MODE	(1 << 4)


static inline const char *vdin_fmt_convert_str(
		enum vdin_format_convert_e fmt_cvt)
{
	switch (fmt_cvt) {
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
		return "FMT_CONVERT_YUV_YUV422";
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
		return "FMT_CONVERT_YUV_YUV444";
		break;
	case VDIN_FORMAT_CONVERT_YUV_RGB:
		return "FMT_CONVERT_YUV_RGB";
		break;
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
		return "FMT_CONVERT_RGB_YUV422";
		break;
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
		return "FMT_CONVERT_RGB_YUV444";
		break;
	case VDIN_FORMAT_CONVERT_RGB_RGB:
		return "FMT_CONVERT_RGB_RGB";
		break;
	case VDIN_FORMAT_CONVERT_YUV_NV12:
		return "VDIN_FORMAT_CONVERT_YUV_NV12";
		break;
	case VDIN_FORMAT_CONVERT_YUV_NV21:
		return "VDIN_FORMAT_CONVERT_YUV_NV21";
		break;
	case VDIN_FORMAT_CONVERT_RGB_NV12:
		return "VDIN_FORMAT_CONVERT_RGB_NV12";
		break;
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		return "VDIN_FORMAT_CONVERT_RGB_NV21";
		break;
	default:
		return "FMT_CONVERT_NULL";
		break;
	}
}

/*******for debug **********/
struct vdin_debug_s {
	struct tvin_cutwin_s cutwin;
	unsigned short scaler4h;/* for vscaler */
	unsigned short scaler4w;/* for hscaler */
	unsigned short dest_cfmt;/* for color fmt conversion */
};
struct vdin_dv_s {
	struct vframe_provider_s vprov_dv;
	struct delayed_work dv_dwork;
	unsigned int dv_cur_index;
	unsigned int dv_next_index;
	unsigned int dolby_input;
	dma_addr_t dv_dma_paddr;
	void *dv_dma_vaddr;
	unsigned int dv_flag_cnt;/*cnt for no dv input*/
	bool dv_flag;
	bool dv_config;
	bool dv_crc_check;/*0:fail;1:ok*/
};
struct vdin_dev_s {
	struct cdev cdev;
	struct device *dev;
	struct tvin_parm_s parm;
	struct tvin_format_s *fmt_info_p;
	struct vf_pool *vfp;
	struct tvin_frontend_s *frontend;
	struct tvin_sig_property_s pre_prop;
	struct tvin_sig_property_s prop;
	struct vframe_provider_s vprov;
	struct vdin_dv_s dv;

	 /* 0:from gpio A,1:from csi2 , 2:gpio B*/
	enum bt_path_e bt_path;

	struct timer_list timer;
	spinlock_t isr_lock;
	struct mutex fe_lock;
	struct clk *msr_clk;
	unsigned int msr_clk_val;

	struct vdin_debug_s debug;
	enum vdin_format_convert_e format_convert;
	unsigned int source_bitdepth;

	struct vf_entry *curr_wr_vfe;
	struct vf_entry *last_wr_vfe;
	unsigned int curr_field_type;

	char name[15];
	/* bit0 TVIN_PARM_FLAG_CAP bit31: TVIN_PARM_FLAG_WORK_ON */
	unsigned int flags;
	unsigned int index;
	unsigned int vdin_max_pixelclk;

	unsigned long mem_start;
	unsigned int mem_size;
	unsigned long vfmem_start[VDIN_CANVAS_MAX_CNT];
	struct page *vfvenc_pages[VDIN_CANVAS_MAX_CNT];
	unsigned int vfmem_size;
	unsigned int vfmem_max_cnt;

	unsigned int h_active;
	unsigned int v_active;
	unsigned int canvas_h;
	unsigned int canvas_w;
	unsigned int canvas_active_w;
	unsigned int canvas_alin_w;
	unsigned int canvas_max_size;
	unsigned int canvas_max_num;
	/*before G12A:32byte(256bit)align;
	 *after G12A:64byte(512bit)align
	 */
	unsigned int canvas_align;

	unsigned int irq;
	unsigned int rdma_irq;
	char irq_name[12];
	/* address offset(vdin0/vdin1/...) */
	unsigned int addr_offset;

	unsigned int unstable_flag;
	unsigned int abnormal_cnt;
	unsigned int stamp;
	unsigned int hcnt64;
	unsigned int cycle;
	unsigned int start_time;/* ms vdin start time */
	int rdma_handle;

	bool cma_config_en;
	/*cma_config_flag:
	 *bit0: (1:share with codec_mm;0:cma alone)
	 *bit8: (1:discontinuous alloc way;0:continuous alloc way)
	 */
	unsigned int cma_config_flag;
#ifdef CONFIG_CMA
	struct platform_device	*this_pdev;
	struct page *venc_pages;
	unsigned int cma_mem_size;/*BYTE*/
	unsigned int cma_mem_alloc;
	/*cma_mem_mode:0:according to input size and output fmt;
	 **1:according to input size and output fmt force as YUV444
	 */
	unsigned int cma_mem_mode;
	unsigned int force_yuv444_malloc;
#endif
	/* bit0: enable/disable; bit4: luma range info */
	bool csc_cfg;
	/* duration of current timing */
	unsigned int duration;
	/* color-depth for vdin write */
	/*vdin write mem color depth support:
	 *bit0:support 8bit
	 *bit1:support 9bit
	 *bit2:support 10bit
	 *bit3:support 12bit
	 *bit4:support yuv422 10bit full pack mode (from txl new add)
	 */
	unsigned int color_depth_support;
	/*color depth config
	 *0:auto config as frontend
	 *8:force config as 8bit
	 *10:force config as 10bit
	 *12:force config as 12bit
	 */
	unsigned int color_depth_config;
	/* new add from txl:color depth mode for 10bit
	 *1: full pack mode;config 10bit as 10bit
	 *0: config 10bit as 12bit
	 */
	unsigned int color_depth_mode;
	/* cutwindow config */
	bool cutwindow_cfg;
	bool auto_cutwindow_en;
	/*
	 *1:vdin out limit range
	 *0:vdin out full range
	 */
	unsigned int color_range_mode;
	/*auto detect av/atv input ratio*/
	unsigned int auto_ratio_en;
	/*
	 *game_mode:
	 *bit0:enable/disable
	 *bit1:for true bypas and put vframe in advance one vsync
	 */
	unsigned int game_mode;
	unsigned int rdma_enable;
	unsigned int canvas_config_mode;
	bool	prehsc_en;
	bool	vshrk_en;
	bool	urgent_en;
	bool black_bar_enable;
	bool hist_bar_enable;
	/*use frame rate to cal duraton*/
	unsigned int use_frame_rate;
	unsigned int irq_cnt;
	unsigned int rdma_irq_cnt;
	unsigned int vdin_irq_flag;
	unsigned int vdin_reset_flag;
	unsigned int vdin_dev_ssize;
	wait_queue_head_t queue;

	struct dentry *dbg_root;	/*dbg_fs*/
};

extern struct vframe_provider_s *vf_get_provider_by_name(
		const char *provider_name);
extern bool enable_reset;
extern unsigned int dolby_size_byte;
extern unsigned int dv_dbg_mask;
extern char *vf_get_receiver_name(const char *provider_name);
extern int start_tvin_service(int no, struct vdin_parm_s *para);
extern int stop_tvin_service(int no);
extern int vdin_reg_v4l2(struct vdin_v4l2_ops_s *v4l2_ops);
extern void vdin_unreg_v4l2(void);
extern int vdin_create_class_files(struct class *vdin_clsp);
extern void vdin_remove_class_files(struct class *vdin_clsp);
extern int vdin_create_device_files(struct device *dev);
extern void vdin_remove_device_files(struct device *dev);
extern int vdin_open_fe(enum tvin_port_e port, int index,
		struct vdin_dev_s *devp);
extern void vdin_close_fe(struct vdin_dev_s *devp);
extern void vdin_start_dec(struct vdin_dev_s *devp);
extern void vdin_stop_dec(struct vdin_dev_s *devp);
extern irqreturn_t vdin_isr_simple(int irq, void *dev_id);
extern irqreturn_t vdin_isr(int irq, void *dev_id);
extern irqreturn_t vdin_v4l2_isr(int irq, void *dev_id);
extern void LDIM_Initial(int pic_h, int pic_v, int BLK_Vnum,
	int BLK_Hnum, int BackLit_mode, int ldim_bl_en, int ldim_hvcnt_bypass);
extern void ldim_get_matrix(int *data, int reg_sel);
extern void ldim_set_matrix(int *data, int reg_sel);
extern void tvafe_snow_config(unsigned int onoff);
extern void tvafe_snow_config_clamp(unsigned int onoff);
extern void vdin_vf_reg(struct vdin_dev_s *devp);
extern void vdin_vf_unreg(struct vdin_dev_s *devp);
extern void vdin_pause_dec(struct vdin_dev_s *devp);
extern void vdin_resume_dec(struct vdin_dev_s *devp);
extern bool is_dolby_vision_enable(void);

extern void vdin_debugfs_init(struct vdin_dev_s *vdevp);
extern void vdin_debugfs_exit(struct vdin_dev_s *vdevp);

#endif /* __TVIN_VDIN_DRV_H */

