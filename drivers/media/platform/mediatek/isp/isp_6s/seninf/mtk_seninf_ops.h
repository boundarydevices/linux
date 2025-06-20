/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __MTK_CAM_SENINF_HW_H__
#define __MTK_CAM_SENINF_HW_H__

#include <linux/interrupt.h>
#include <linux/sched/signal.h>
#include <linux/sched.h>

/* seninf tg1 */
#define TM_CTL 0x0008
#define TM_EN_SHIFT 0
#define TM_EN_MASK (0x1 << 0)
#define TM_RST_SHIFT 1
#define TM_RST_MASK (0x1 << 1)
#define TM_FMT_SHIFT 2
#define TM_FMT_MASK (0x1 << 2)
#define TM_BIN_IMG_SWITCH_EN_SHIFT 3
#define TM_BIN_IMG_SWITCH_EN_MASK (0x1 << 3)
#define TM_PAT_SHIFT 4
#define TM_PAT_MASK (0x1f << 4)

#define TM_SIZE 0x000c
#define TM_PXL_SHIFT 0
#define TM_PXL_MASK (0xffff << 0)
#define TM_LINE_SHIFT 16
#define TM_LINE_MASK (0xffff << 16)

#define TM_CLK 0x0010
#define TM_CLK_CNT_SHIFT 0
#define TM_CLK_CNT_MASK (0xff << 0)
#define TM_CLRBAR_OFT_SHIFT 8
#define TM_CLRBAR_OFT_MASK (0x1fff << 8)
#define TM_CLRBAR_IDX_SHIFT 28
#define TM_CLRBAR_IDX_MASK (0x7 << 28)

#define TM_DUM 0x0018
#define TM_DUMMYPXL_SHIFT 0
#define TM_DUMMYPXL_MASK (0xffff << 0)
#define TM_VSYNC_SHIFT 16
#define TM_VSYNC_MASK (0xffff << 16)

#define TM_RAND_SEED 0x001c
#define TM_SEED_SHIFT 0
#define TM_SEED_MASK (0xffffffff << 0)

#define TM_RAND_CTL 0x0020
#define TM_DIFF_FRM_SHIFT 0
#define TM_DIFF_FRM_MASK (0x1 << 0)

#define TM_STAGGER_CTL 0x0024
#define STAGGER_MODE_EN_SHIFT 0
#define STAGGER_MODE_EN_MASK (0x1 << 0)
#define EXP_NUM_SHIFT 4
#define EXP_NUM_MASK (0x7 << 4)
#define EXP_ONE_VSYNC_SHIFT 8
#define EXP_ONE_VSYNC_MASK (0x1 << 8)

#define TM_STAGGER_CON0 0x0028
#define TM_EXP_DT0_SHIFT 0
#define TM_EXP_DT0_MASK (0x3f << 0)
#define TM_EXP_VSYNC_VC0_SHIFT 8
#define TM_EXP_VSYNC_VC0_MASK (0x1f << 8)
#define TM_EXP_HSYNC_VC0_SHIFT 16
#define TM_EXP_HSYNC_VC0_MASK (0x1f << 16)

#define TM_STAGGER_CON1 0x002c
#define TM_EXP_DT1_SHIFT 0
#define TM_EXP_DT1_MASK (0x3f << 0)
#define TM_EXP_VSYNC_VC1_SHIFT 8
#define TM_EXP_VSYNC_VC1_MASK (0x1f << 8)
#define TM_EXP_HSYNC_VC1_SHIFT 16
#define TM_EXP_HSYNC_VC1_MASK (0x1f << 16)

#define TM_STAGGER_CON2 0x0030
#define TM_EXP_DT2_SHIFT 0
#define TM_EXP_DT2_MASK (0x3f << 0)
#define TM_EXP_VSYNC_VC2_SHIFT 8
#define TM_EXP_VSYNC_VC2_MASK (0x1f << 8)
#define TM_EXP_HSYNC_VC2_SHIFT 16
#define TM_EXP_HSYNC_VC2_MASK (0x1f << 16)

#define TM_STAGGER_CON3 0x0034
#define TM_EXP_DT3_SHIFT 0
#define TM_EXP_DT3_MASK (0x3f << 0)
#define TM_EXP_VSYNC_VC3_SHIFT 8
#define TM_EXP_VSYNC_VC3_MASK (0x1f << 8)
#define TM_EXP_HSYNC_VC3_SHIFT 16
#define TM_EXP_HSYNC_VC3_MASK (0x1f << 16)

/* seninf ops */

enum SET_REG_KEYS {
	REG_KEY_MIN = 0,
	REG_KEY_SETTLE_CK = REG_KEY_MIN,
	REG_KEY_SETTLE_DT,
	REG_KEY_HS_TRAIL_EN,
	REG_KEY_HS_TRAIL_PARAM,
	REG_KEY_CSI_IRQ_STAT,
	REG_KEY_CSI_RESYNC_CYCLE,
	REG_KEY_MUX_IRQ_STAT,
	REG_KEY_CAMMUX_IRQ_STAT,
	REG_KEY_CAMMUX_VSYNC_IRQ_EN,
	REG_KEY_CSI_IRQ_EN,
	REG_KEY_MAX_NUM
};

#define SET_REG_KEYS_NAMES \
	"RG_SETTLE_CK", \
	"RG_SETTLE_DT", \
	"RG_HS_TRAIL_EN", \
	"RG_HS_TRAIL_PARAM", \
	"RG_CSI_IRQ_STAT", \
	"RG_CSI_RESYNC_CYCLE", \
	"RG_MUX_IRQ_STAT", \
	"RG_CAMMUX_IRQ_STAT", \
	"REG_VSYNC_IRQ_EN", \
	"RG_CSI_IRQ_EN", \

struct mtk_seninf_mux_meter {
	u32 width;
	u32 height;
	u32 h_valid;
	u32 h_blank;
	u32 v_valid;
	u32 v_blank;
	s64 mipi_pixel_rate;
	s64 vb_in_us;
	s64 hb_in_us;
	s64 line_time_in_us;
};

extern int update_isp_clk(struct seninf_ctx *ctx);

struct mtk_seninf_ops {
	int (*_init_iomem)(struct seninf_ctx *ctx,
			   void __iomem *if_base);
	int (*_init_port)(struct seninf_ctx *ctx, int port);
	int (*_is_cammux_used)(struct seninf_ctx *ctx, int cam_mux);
	int (*_cammux)(struct seninf_ctx *ctx, int cam_mux);
	int (*_disable_cammux)(struct seninf_ctx *ctx, int cam_mux);
	int (*_disable_all_cammux)(struct seninf_ctx *ctx);
	int (*_set_top_mux_ctrl)(struct seninf_ctx *ctx,
				 int mux_idx, int seninf_src);
	int (*_get_top_mux_ctrl)(struct seninf_ctx *ctx, int mux_idx);
	int (*_get_cammux_ctrl)(struct seninf_ctx *ctx, int cam_mux);
	u32 (*_get_cammux_res)(struct seninf_ctx *ctx, int cam_mux);
	int (*_set_cammux_vc)(struct seninf_ctx *ctx, int cam_mux,
			      int vc_sel, int dt_sel, int vc_en, int dt_en);
	int (*_set_cammux_src)(struct seninf_ctx *ctx, int src,
			       int target, int exp_hsize, int exp_vsize);
	int (*_set_vc)(struct seninf_ctx *ctx, u32 seninfIdx,
		       struct seninf_vcinfo *vcinfo);
	int (*_set_mux_ctrl)(struct seninf_ctx *ctx, u32 mux,
			     int hsPol, int vsPol, int src_sel,
			     int pixel_mode);
	int (*_set_mux_crop)(struct seninf_ctx *ctx, u32 mux,
			     int start_x, int end_x, int enable);
	int (*_set_mux_exp)(struct seninf_ctx *ctx, u32 mux,
			    int exp_hsize, int exp_vsize);
	int (*_is_mux_used)(struct seninf_ctx *ctx, u32 mux);
	int (*_mux)(struct seninf_ctx *ctx, u32 mux);
	int (*_disable_mux)(struct seninf_ctx *ctx, u32 mux);
	int (*_disable_all_mux)(struct seninf_ctx *ctx);
	int (*_set_cammux_chk_pixel_mode)(struct seninf_ctx *ctx,
					  int cam_mux, int pixelMode);
	int (*_set_test_model)(struct seninf_ctx *ctx,
			       int mux, int cam_mux, int pixelMode);
	int (*_set_csi_mipi)(struct seninf_ctx *ctx);
	int (*_poweroff)(struct seninf_ctx *ctx);
	int (*_reset)(struct seninf_ctx *ctx, u32 seninfIdx);
	int (*_set_idle)(struct seninf_ctx *ctx);
	int (*_get_mux_meter)(struct seninf_ctx *ctx, u32 mux,
			      struct mtk_seninf_mux_meter *meter);
	ssize_t (*_show_status)(struct device *dev, struct device_attribute *attr, char *buf);
	int (*_switch_to_cammux_inner_page)(struct seninf_ctx *ctx, bool inner);
	int (*_set_cammux_next_ctrl)(struct seninf_ctx *ctx, int src, int target);
	int (*_update_mux_pixel_mode)(struct seninf_ctx *ctx, u32 mux, int pixel_mode);
	int (*_irq_handler)(int irq, void *data);
	int (*_set_sw_cfg_busy)(struct seninf_ctx *ctx, bool enable, int index);
	int (*_set_cam_mux_dyn_en)(struct seninf_ctx *ctx, bool enable, int cam_mux, int index);
	int (*_reset_cam_mux_dyn_en)(struct seninf_ctx *ctx, int index);
	int (*_enable_global_drop_irq)(struct seninf_ctx *ctx, bool enable, int index);
	int (*_enable_cam_mux_vsync_irq)(struct seninf_ctx *ctx, bool enable, int cam_mux);
	int (*_disable_all_cam_mux_vsync_irq)(struct seninf_ctx *ctx);
	int (*_debug)(struct seninf_ctx *ctx);
	int (*_set_reg)(struct seninf_ctx *ctx, u32 key, u32 val);
	ssize_t (*_show_err_status)(struct device *dev, struct device_attribute *attr, char *buf);
	unsigned int seninf_num;
	unsigned int mux_num;
	unsigned int cam_mux_num;
	unsigned int pref_mux_num;

};

extern struct mtk_seninf_ops mtk_csi_phy_2_1;
extern struct mtk_seninf_ops *g_seninf_ops;

#endif /* __MTK_CAM_SENINF_HW_H__ */
