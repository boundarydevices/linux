// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Jie Qiu <jie.qiu@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/media-bus-format.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-mmsys.h>
#include <linux/types.h>

#include <video/videomode.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_bridge_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_simple_kms_helper.h>

#include "mtk_dvo_regs.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"

enum mtk_dvo_out_bit_num {
	MTK_DVO_OUT_BIT_NUM_8BITS,
	MTK_DVO_OUT_BIT_NUM_10BITS,
	MTK_DVO_OUT_BIT_NUM_12BITS,
	MTK_DVO_OUT_BIT_NUM_16BITS
};

enum mtk_dvo_out_yc_map {
	MTK_DVO_OUT_YC_MAP_RGB,
	MTK_DVO_OUT_YC_MAP_CYCY,
	MTK_DVO_OUT_YC_MAP_YCYC,
	MTK_DVO_OUT_YC_MAP_CY,
	MTK_DVO_OUT_YC_MAP_YC
};

enum mtk_dvo_out_channel_swap {
	MTK_DVO_OUT_CHANNEL_SWAP_RGB,
	MTK_DVO_OUT_CHANNEL_SWAP_GBR,
	MTK_DVO_OUT_CHANNEL_SWAP_BRG,
	MTK_DVO_OUT_CHANNEL_SWAP_RBG,
	MTK_DVO_OUT_CHANNEL_SWAP_GRB,
	MTK_DVO_OUT_CHANNEL_SWAP_BGR
};

enum mtk_dvo_out_color_format {
	MTK_DVO_COLOR_FORMAT_RGB,
	MTK_DVO_COLOR_FORMAT_YCBCR_422
};

enum TVDPLL_CLK {
	TVDPLL_PLL = 0,
	TVDPLL_D2 = 2,
	TVDPLL_D4 = 4,
	TVDPLL_D8 = 8,
	TVDPLL_D16 = 16,
};

enum mtk_dvo_golden_setting_level {
	MTK_DVO_FHD_60FPS_1920 = 0,
	MTK_DVO_FHD_60FPS_2180,
	MTK_DVO_FHD_60FPS_2400,
	MTK_DVO_FHD_60FPS_2520,
	MTK_DVO_FHD_90FPS,
	MTK_DVO_FHD_120FPS,
	MTK_DVO_WQHD_60FPS,
	MTK_DVO_WQHD_120FPS,
	MTK_DVO_8K_30FPS,
	MTK_DVO_GSL_MAX,
};

struct mtk_dvo_gs_info {
	u32 dvo_buf_sodi_high;
	u32 dvo_buf_sodi_low;
};

struct mtk_dvo {
	struct mtk_ddp_comp ddp_comp;
	struct drm_encoder encoder;
	struct drm_bridge bridge;
	struct drm_bridge *next_bridge;
	struct drm_connector *connector;
	void __iomem *regs;
	struct device *dev;
	struct device *mmsys_dev;
	struct clk *engine_clk;
	struct clk *pixel_clk;
	struct clk *tvd_clk;
	struct clk *dvo_clk;
	struct clk *hf_fdvo_clk;
	struct clk *pclk_src[5];
	int irq;
	struct drm_display_mode mode;
	const struct mtk_dvo_conf *conf;
	enum mtk_dvo_out_color_format color_format;
	enum mtk_dvo_out_yc_map yc_map;
	enum mtk_dvo_out_bit_num bit_num;
	enum mtk_dvo_out_channel_swap channel_swap;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_gpio;
	struct pinctrl_state *pins_dvo;
	u32 output_fmt;
	int refcount;
	enum mtk_dvo_golden_setting_level gs_level;
};

static inline struct mtk_dvo *bridge_to_dvo(struct drm_bridge *b)
{
	return container_of(b, struct mtk_dvo, bridge);
}

enum mtk_dvo_polarity {
	MTK_DVO_POLARITY_RISING,
	MTK_DVO_POLARITY_FALLING,
};

struct mtk_dvo_polarities {
	enum mtk_dvo_polarity de_pol;
	enum mtk_dvo_polarity ck_pol;
	enum mtk_dvo_polarity hsync_pol;
	enum mtk_dvo_polarity vsync_pol;
};

struct mtk_dvo_sync_param {
	u32 sync_width;
	u32 front_porch;
	u32 back_porch;
	bool shift_half_line;
};

struct mtk_dvo_yc_limit {
	u16 y_top;
	u16 y_bottom;
	u16 c_top;
	u16 c_bottom;
};

/**
 * struct mtk_dvo_conf - Configuration of mediatek dvo.
 * @cal_factor: Callback function to calculate factor value.
 * @reg_h_fre_con: Register address of frequency control.
 * @max_clock_khz: Max clock frequency supported for this SoCs in khz units.
 * @edge_sel_en: Enable of edge selection.
 * @output_fmts: Array of supported output formats.
 * @num_output_fmts: Quantity of supported output formats.
 * @is_ck_de_pol: Support CK/DE polarity.
 * @swap_input_support: Support input swap function.
 * @support_direct_pin: IP supports direct connection to dvo panels.
 * @input_2pixel: Input pixel of dp_intf is 2 pixel per round, so enable this
 *		  config to enable this feature.
 * @dimension_mask: Mask used for HWIDTH, HPORCH, VSYNC_WIDTH and VSYNC_PORCH
 *		    (no shift).
 * @hvsize_mask: Mask of HSIZE and VSIZE mask (no shift).
 * @channel_swap_shift: Shift value of channel swap.
 * @yuv422_en_bit: Enable bit of yuv422.
 * @csc_enable_bit: Enable bit of CSC.
 * @pixels_per_iter: Quantity of transferred pixels per iteration.
 * @edge_cfg_in_mmsys: If the edge configuration for DVO's output needs to be set in MMSYS.
 */
struct mtk_dvo_conf {
	unsigned int (*cal_factor)(int clock);
	u32 reg_h_fre_con;
	u32 max_clock_khz;
	bool edge_sel_en;
	const u32 *output_fmts;
	u32 num_output_fmts;
	bool is_ck_de_pol;
	bool swap_input_support;
	bool support_direct_pin;
	bool input_2pixel;
	u32 out_np_sel;
	u32 dimension_mask;
	u32 hvsize_mask;
	u32 channel_swap_shift;
	u32 yuv422_en_bit;
	u32 csc_enable_bit;
	u32 pixels_per_iter;
	bool edge_cfg_in_mmsys;
	bool has_commit;
	bool is_dp;
};

static struct mtk_dvo_gs_info mtk_dvo_gs[MTK_DVO_GSL_MAX] = {
	[MTK_DVO_FHD_60FPS_1920] = {6880, 511},
	[MTK_DVO_8K_30FPS] = {5255, 3899},
};

static void mtk_dvo_mask(struct mtk_dvo *dvo, u32 offset, u32 val, u32 mask)
{
	u32 tmp = readl(dvo->regs + offset) & ~mask;

	tmp |= (val & mask);
	writel(tmp, dvo->regs + offset);
}

static void mtk_dvo_sw_reset(struct mtk_dvo *dvo, bool reset)
{
	mtk_dvo_mask(dvo, DVO_RET, reset ? SWRST : 0, SWRST);
}

static void mtk_dvo_enable(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_EN, EN, EN);
}

static void mtk_dvo_disable(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_EN, 0, EN);
}

static void mtk_dvo_irq_enable(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_INTEN, INT_VDE_END_EN, INT_VDE_END_EN);
}

static void mtk_dvo_info_queue_start(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_TGEN_INFOQ_LATENCY, 0,
		     INFOQ_START_LATENCY_MASK | INFOQ_END_LATENCY_MASK);
}

static void mtk_dvo_buffer_ctrl(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_BUF_CON0, DISP_BUF_EN, DISP_BUF_EN);
	mtk_dvo_mask(dvo, DVO_BUF_CON0, FIFO_UNDERFLOW_DONE_BLOCK, FIFO_UNDERFLOW_DONE_BLOCK);
}

static void mtk_dvo_trailing_blank_setting(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_TGEN_V_LAST_TRAILING_BLANK, 0x20, V_LAST_TRAILING_BLANK_MASK);
	mtk_dvo_mask(dvo, DVO_TGEN_OUTPUT_DELAY_LINE, 0x20, EXT_TG_DLY_LINE_MASK);
}

static void mtk_dvo_get_gs_level(struct mtk_dvo *dvo)
{
	struct drm_display_mode *mode = &dvo->mode;
	enum mtk_dvo_golden_setting_level *gsl = &dvo->gs_level;

	if (mode->hdisplay == 1920 && mode->vdisplay == 1080)
		*gsl = MTK_DVO_FHD_60FPS_1920;
	else
		*gsl = MTK_DVO_8K_30FPS;
}

static void mtk_dvo_golden_setting(struct mtk_dvo *dvo)
{
	struct mtk_dvo_gs_info *gs_info = NULL;

	if (dvo->gs_level >= MTK_DVO_GSL_MAX) {
		dev_info(dvo->dev, "%s invalid gs_level %d\n",
			 __func__, dvo->gs_level);
		return;
	}

	gs_info = &mtk_dvo_gs[dvo->gs_level];

	mtk_dvo_mask(dvo, DVO_BUF_SODI_HIGHT, gs_info->dvo_buf_sodi_high, GENMASK(31, 0));
	mtk_dvo_mask(dvo, DVO_BUF_SODI_LOW, gs_info->dvo_buf_sodi_low, GENMASK(31, 0));
}

static void mtk_dvo_shadow_ctrl(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_SHADOW_CTRL, 0, BYPASS_SHADOW);
	mtk_dvo_mask(dvo, DVO_SHADOW_CTRL, FORCE_COMMIT, FORCE_COMMIT);
}

static void mtk_dvo_config_hsync(struct mtk_dvo *dvo,
				 struct mtk_dvo_sync_param *sync)
{
	mtk_dvo_mask(dvo, DVO_TGEN_H0, sync->sync_width << HSYNC,
		     dvo->conf->dimension_mask << HSYNC);
	mtk_dvo_mask(dvo, DVO_TGEN_H0, sync->front_porch << HFP,
		     dvo->conf->dimension_mask << HFP);
	mtk_dvo_mask(dvo, DVO_TGEN_H1, (sync->back_porch + sync->sync_width) << HSYNC2ACT,
		     dvo->conf->dimension_mask << HSYNC2ACT);
}

static void mtk_dvo_config_vsync(struct mtk_dvo *dvo,
				 struct mtk_dvo_sync_param *sync,
				 u32 width_addr, u32 porch_addr)
{
	mtk_dvo_mask(dvo, width_addr,
		     sync->sync_width << VSYNC,
		     dvo->conf->dimension_mask << VSYNC);
	mtk_dvo_mask(dvo, width_addr,
		     sync->front_porch << VFP,
		     dvo->conf->dimension_mask << VFP);
	mtk_dvo_mask(dvo, porch_addr,
		     (sync->back_porch + sync->sync_width) << VSYNC2ACT,
	     dvo->conf->dimension_mask << VSYNC2ACT);
}

static void mtk_dvo_config_vsync_lodd(struct mtk_dvo *dvo,
				      struct mtk_dvo_sync_param *sync)
{
	mtk_dvo_config_vsync(dvo, sync, DVO_TGEN_V0, DVO_TGEN_V1);
}

static void mtk_dvo_config_interface(struct mtk_dvo *dvo, bool inter)
{
	mtk_dvo_mask(dvo, DVO_CON, inter ? INTL_EN : 0, INTL_EN);
}

static void mtk_dvo_config_fb_size(struct mtk_dvo *dvo, u32 width, u32 height)
{
	mtk_dvo_mask(dvo, DVO_SRC_SIZE, width << SRC_HSIZE,
		     dvo->conf->hvsize_mask << SRC_HSIZE);
	mtk_dvo_mask(dvo, DVO_SRC_SIZE, height << SRC_VSIZE,
		     dvo->conf->hvsize_mask << SRC_VSIZE);

	mtk_dvo_mask(dvo, DVO_PIC_SIZE, width << PIC_HSIZE,
		     dvo->conf->hvsize_mask << PIC_HSIZE);
	mtk_dvo_mask(dvo, DVO_PIC_SIZE, height << PIC_VSIZE,
		     dvo->conf->hvsize_mask << PIC_VSIZE);

	mtk_dvo_mask(dvo, DVO_TGEN_H1, DIV_ROUND_UP(width, dvo->conf->pixels_per_iter) << HACT,
		     dvo->conf->hvsize_mask << HACT);

	mtk_dvo_mask(dvo, DVO_TGEN_V1, height << VACT,
		     dvo->conf->hvsize_mask << VACT);
}

static void mtk_dvo_power_off(struct mtk_dvo *dvo)
{
	if (WARN_ON(dvo->refcount == 0))
		return;

	if (--dvo->refcount != 0)
		return;

	mtk_dvo_disable(dvo);

	clk_disable_unprepare(dvo->engine_clk);
	clk_disable_unprepare(dvo->tvd_clk);
	clk_disable_unprepare(dvo->hf_fdvo_clk);
}

static int mtk_dvo_power_on(struct mtk_dvo *dvo)
{
	int ret;

	if (++dvo->refcount != 1)
		return 0;

	ret = clk_prepare_enable(dvo->hf_fdvo_clk);
	if (ret) {
		dev_err(dvo->dev, "Failed to enable hf_fdvo_clk clock: %d\n", ret);
		goto err_refcount;
	}

	ret = clk_prepare_enable(dvo->tvd_clk);
	if (ret) {
		dev_err(dvo->dev, "Failed to enable tvd_clk clock: %d\n", ret);
		goto err_refcount;
	}

	ret = clk_prepare_enable(dvo->engine_clk);
	if (ret) {
		dev_err(dvo->dev, "Failed to enable engine clock: %d\n", ret);
		goto err_refcount;
	}

	ret = clk_prepare_enable(dvo->dvo_clk);
	if (ret) {
		dev_err(dvo->dev, "Failed to enable engine clock: %d\n", ret);
		goto err_refcount;
	}

	ret = clk_prepare_enable(dvo->pixel_clk);
	if (ret) {
		dev_err(dvo->dev, "Failed to enable engine clock: %d\n", ret);
		goto err_refcount;
	}

	return 0;

err_refcount:
	dvo->refcount--;
	return ret;
}

static int mtk_dvo_set_display_mode(struct mtk_dvo *dvo,
				    struct drm_display_mode *mode)
{
	struct mtk_dvo_sync_param hsync;
	struct mtk_dvo_sync_param vsync_lodd = { 0 };
	struct videomode vm = { 0 };
	unsigned long pll_rate;
	unsigned int factor;

	/* let pll_rate can fix the valid range of tvdpll (1G~2GHz) */
	factor = dvo->conf->cal_factor(mode->clock);
	drm_display_mode_to_videomode(mode, &vm);
	pll_rate = vm.pixelclock * factor;

	dev_dbg(dvo->dev, "Want PLL %lu Hz, pixel clock %lu Hz\n",
			   pll_rate, vm.pixelclock);

	clk_set_rate(dvo->tvd_clk, pll_rate);
	pll_rate = clk_get_rate(dvo->tvd_clk);

	/*
	 * Depending on the IP version, we may output a different amount of
	 * pixels for each iteration: divide the clock by this number and
	 * adjust the display porches accordingly.
	 */
	vm.pixelclock = pll_rate / factor;
	vm.pixelclock /= dvo->conf->pixels_per_iter;
	clk_set_rate(dvo->pixel_clk, vm.pixelclock);

	vm.pixelclock = clk_get_rate(dvo->pixel_clk);

	dev_dbg(dvo->dev, "Got  PLL %lu Hz, pixel clock %lu Hz\n",
			   pll_rate, vm.pixelclock);
	/*
	 * Depending on the IP version, we may output a different amount of
	 * pixels for each iteration: divide the clock by this number and
	 * adjust the display porches accordingly.
	 */
	hsync.sync_width = vm.hsync_len / dvo->conf->pixels_per_iter;
	hsync.back_porch = vm.hback_porch / dvo->conf->pixels_per_iter;
	hsync.front_porch = vm.hfront_porch / dvo->conf->pixels_per_iter;

	hsync.shift_half_line = false;
	vsync_lodd.sync_width = vm.vsync_len;
	vsync_lodd.back_porch = vm.vback_porch;
	vsync_lodd.front_porch = vm.vfront_porch;
	vsync_lodd.shift_half_line = false;

	mtk_dvo_sw_reset(dvo, true);
	mtk_dvo_irq_enable(dvo);

	mtk_dvo_config_hsync(dvo, &hsync);
	mtk_dvo_config_vsync_lodd(dvo, &vsync_lodd);

	mtk_dvo_config_interface(dvo, !!(vm.flags & DISPLAY_FLAGS_INTERLACED));

	if (vm.flags & DISPLAY_FLAGS_INTERLACED)
		mtk_dvo_config_fb_size(dvo, vm.hactive, vm.vactive >> 1);
	else
		mtk_dvo_config_fb_size(dvo, vm.hactive, vm.vactive);

	mtk_dvo_info_queue_start(dvo);
	mtk_dvo_buffer_ctrl(dvo);
	if (!dvo->conf->is_dp) {
		mtk_dvo_trailing_blank_setting(dvo);
		mtk_dvo_get_gs_level(dvo);
		mtk_dvo_golden_setting(dvo);
	}

	if (dvo->conf->pixels_per_iter)
		mtk_dvo_shadow_ctrl(dvo);
	if (dvo->conf->out_np_sel) {
		mtk_dvo_mask(dvo, DVO_OUTPUT_SET, dvo->conf->out_np_sel,
			     OUT_NP_SEL);
	}
	mtk_dvo_sw_reset(dvo, false);

	return 0;
}

static u32 *mtk_dvo_bridge_atomic_get_output_bus_fmts(struct drm_bridge *bridge,
						      struct drm_bridge_state *bridge_state,
						      struct drm_crtc_state *crtc_state,
						      struct drm_connector_state *conn_state,
						      unsigned int *num_output_fmts)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);
	u32 *output_fmts;

	*num_output_fmts = 0;

	if (!dvo->conf->output_fmts) {
		dev_err(dvo->dev, "output_fmts should not be null\n");
		return NULL;
	}

	output_fmts = kcalloc(dvo->conf->num_output_fmts, sizeof(*output_fmts),
			      GFP_KERNEL);
	if (!output_fmts)
		return NULL;

	*num_output_fmts = dvo->conf->num_output_fmts;

	memcpy(output_fmts, dvo->conf->output_fmts,
	       sizeof(*output_fmts) * dvo->conf->num_output_fmts);

	return output_fmts;
}

static u32 *mtk_dvo_bridge_atomic_get_input_bus_fmts(struct drm_bridge *bridge,
						     struct drm_bridge_state *bridge_state,
						     struct drm_crtc_state *crtc_state,
						     struct drm_connector_state *conn_state,
						     u32 output_fmt,
						     unsigned int *num_input_fmts)
{
	u32 *input_fmts;

	*num_input_fmts = 0;

	input_fmts = kcalloc(1, sizeof(*input_fmts),
			     GFP_KERNEL);
	if (!input_fmts)
		return NULL;

	*num_input_fmts = 1;
	input_fmts[0] = MEDIA_BUS_FMT_RGB888_1X24;

	return input_fmts;
}

static int mtk_dvo_bridge_atomic_check(struct drm_bridge *bridge,
				       struct drm_bridge_state *bridge_state,
				       struct drm_crtc_state *crtc_state,
				       struct drm_connector_state *conn_state)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);
	unsigned int out_bus_format;

	out_bus_format = bridge_state->output_bus_cfg.format;

	if (out_bus_format == MEDIA_BUS_FMT_FIXED)
		if (dvo->conf->num_output_fmts)
			out_bus_format = dvo->conf->output_fmts[0];

	dev_dbg(dvo->dev, "input format 0x%04x, output format 0x%04x\n",
		bridge_state->input_bus_cfg.format,
		bridge_state->output_bus_cfg.format);

	dvo->output_fmt = out_bus_format;
	dvo->bit_num = MTK_DVO_OUT_BIT_NUM_8BITS;
	dvo->channel_swap = MTK_DVO_OUT_CHANNEL_SWAP_RGB;
	dvo->yc_map = MTK_DVO_OUT_YC_MAP_RGB;
	if (out_bus_format == MEDIA_BUS_FMT_YUYV8_1X16)
		dvo->color_format = MTK_DVO_COLOR_FORMAT_YCBCR_422;
	else
		dvo->color_format = MTK_DVO_COLOR_FORMAT_RGB;

	return 0;
}

static int mtk_dvo_bridge_attach(struct drm_bridge *bridge,
				 enum drm_bridge_attach_flags flags)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);
	int ret = 0;

	ret = drm_bridge_attach(bridge->encoder, dvo->next_bridge,
							&dvo->bridge, 1);
	if (ret)
		dev_err(dvo->dev, " Failed to call attach ret = %d\n", ret);

	return ret;
}

static void mtk_dvo_bridge_mode_set(struct drm_bridge *bridge,
		const struct drm_display_mode *mode,
		const struct drm_display_mode *adjusted_mode)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);

	drm_mode_copy(&dvo->mode, adjusted_mode);
}

static void mtk_dvo_bridge_disable(struct drm_bridge *bridge)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);

	mtk_dvo_power_off(dvo);

	if (dvo->pinctrl && dvo->pins_gpio)
		pinctrl_select_state(dvo->pinctrl, dvo->pins_gpio);
}

static void mtk_dvo_bridge_enable(struct drm_bridge *bridge)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);

	if (dvo->pinctrl && dvo->pins_dvo)
		pinctrl_select_state(dvo->pinctrl, dvo->pins_dvo);

	mtk_dvo_power_on(dvo);
	mtk_dvo_set_display_mode(dvo, &dvo->mode);
	mtk_dvo_enable(dvo);
	/*
	 * mtk_dvo_mask(dvo, DVO_PATTERN_CTRL, PRE_PAT_EN, PRE_PAT_EN);
	 * mtk_dvo_mask(dvo, DVO_PATTERN_CTRL, COLOR_BAR, PRE_PAT_SEL_MASK);
	 * mtk_dvo_mask(dvo, DVO_PATTERN_CTRL, PRE_PAT_FORCE_ON, PRE_PAT_FORCE_ON);
	 * mtk_dvo_mask(dvo, DVO_PATTERN_COLOR, PAT_G, PAT_G);
	 */
}

static enum drm_mode_status
mtk_dvo_bridge_mode_valid(struct drm_bridge *bridge,
			  const struct drm_display_info *info,
			  const struct drm_display_mode *mode)
{
	return MODE_OK;
}

static const struct drm_bridge_funcs mtk_dvo_bridge_funcs = {
	.attach = mtk_dvo_bridge_attach,
	.mode_set = mtk_dvo_bridge_mode_set,
	.mode_valid = mtk_dvo_bridge_mode_valid,
	.disable = mtk_dvo_bridge_disable,
	.enable = mtk_dvo_bridge_enable,
	.atomic_check = mtk_dvo_bridge_atomic_check,
	.atomic_get_output_bus_fmts = mtk_dvo_bridge_atomic_get_output_bus_fmts,
	.atomic_get_input_bus_fmts = mtk_dvo_bridge_atomic_get_input_bus_fmts,
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_reset = drm_atomic_helper_bridge_reset,
};

void mtk_dvo_start(struct device *dev)
{
	struct mtk_dvo *dvo = dev_get_drvdata(dev);

	mtk_dvo_power_on(dvo);
}

void mtk_dvo_stop(struct device *dev)
{
	struct mtk_dvo *dvo = dev_get_drvdata(dev);

	mtk_dvo_power_off(dvo);
}

static int mtk_dvo_bind(struct device *dev, struct device *master, void *data)
{
	struct mtk_dvo *dvo = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *priv = drm_dev->dev_private;
	int ret;

	dvo->mmsys_dev = priv->mmsys_dev;
	ret = drm_simple_encoder_init(drm_dev, &dvo->encoder,
				      DRM_MODE_ENCODER_TMDS);
	if (ret) {
		dev_err(dev, "Failed to initialize decoder: %d\n", ret);
		return ret;
	}

	dvo->encoder.possible_crtcs = mtk_drm_find_possible_crtc_by_comp(drm_dev, dvo->dev);

	ret = drm_bridge_attach(&dvo->encoder, &dvo->bridge, NULL,
				DRM_BRIDGE_ATTACH_NO_CONNECTOR);
	if (ret)
		goto err_cleanup;

	dvo->connector = drm_bridge_connector_init(drm_dev, &dvo->encoder);
	if (IS_ERR(dvo->connector)) {
		dev_err(dev, "Unable to create bridge connector\n");
		ret = PTR_ERR(dvo->connector);
		goto err_cleanup;
	}
	drm_connector_attach_encoder(dvo->connector, &dvo->encoder);

	return 0;

err_cleanup:
	drm_encoder_cleanup(&dvo->encoder);
	return ret;
}

static void mtk_dvo_unbind(struct device *dev, struct device *master,
			   void *data)
{
	struct mtk_dvo *dvo = dev_get_drvdata(dev);

	drm_encoder_cleanup(&dvo->encoder);
}

static const struct component_ops mtk_dvo_component_ops = {
	.bind = mtk_dvo_bind,
	.unbind = mtk_dvo_unbind,
};

static unsigned int mt8196_calculate_factor(int clock)
{
	if (clock < 70000)
		return 4;
	else if (clock < 200000)
		return 2;
	else
		return 1;
}

static const u32 mt8196_output_fmts[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUYV8_1X16,
};

static const struct mtk_dvo_conf mt8189_conf = {
	.cal_factor = mt8196_calculate_factor,
	.out_np_sel = 0X2,
	.reg_h_fre_con = 0xb0,
	.max_clock_khz = 640000,
	.output_fmts = mt8196_output_fmts,
	.num_output_fmts = ARRAY_SIZE(mt8196_output_fmts),
	.pixels_per_iter = 4,
	.has_commit = true,
	.dimension_mask = HFP_MASK,
	.hvsize_mask = PIC_HSIZE_MASK,
};

static const struct mtk_dvo_conf mt8189_dp_conf = {
	.cal_factor = mt8196_calculate_factor,
	.out_np_sel = 0X2,
	.reg_h_fre_con = 0xb0,
	.max_clock_khz = 640000,
	.output_fmts = mt8196_output_fmts,
	.num_output_fmts = ARRAY_SIZE(mt8196_output_fmts),
	.pixels_per_iter = 4,
	.has_commit = true,
	.dimension_mask = HFP_MASK,
	.hvsize_mask = PIC_HSIZE_MASK,
	.is_dp = true,
};

static int mtk_dvo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *endpoint, *port;
	struct mtk_dvo *dvo;
	int ret;

	dvo = devm_kzalloc(dev, sizeof(*dvo), GFP_KERNEL);
	if (!dvo)
		return -ENOMEM;

	dvo->dev = dev;
	dvo->conf = (struct mtk_dvo_conf *)of_device_get_match_data(dev);
	dvo->output_fmt = MEDIA_BUS_FMT_RGB888_1X24;

	dvo->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(dvo->pinctrl)) {
		dvo->pinctrl = NULL;
		dev_dbg(&pdev->dev, "Cannot find pinctrl!\n");
	}
	if (dvo->pinctrl) {
		dvo->pins_gpio = pinctrl_lookup_state(dvo->pinctrl, "sleep");
		if (IS_ERR(dvo->pins_gpio)) {
			dvo->pins_gpio = NULL;
			dev_dbg(&pdev->dev, "Cannot find pinctrl idle!\n");
		}
		if (dvo->pins_gpio)
			pinctrl_select_state(dvo->pinctrl, dvo->pins_gpio);

		dvo->pins_dvo = pinctrl_lookup_state(dvo->pinctrl, "default");
		if (IS_ERR(dvo->pins_dvo)) {
			dvo->pins_dvo = NULL;
			dev_dbg(&pdev->dev, "Cannot find pinctrl active!\n");
		}
	}
	dvo->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dvo->regs))
		return dev_err_probe(dev, PTR_ERR(dvo->regs),
				     "Failed to ioremap mem resource\n");

	dvo->engine_clk = devm_clk_get(dev, "engine");
	if (IS_ERR(dvo->engine_clk))
		return dev_err_probe(dev, PTR_ERR(dvo->engine_clk),
				     "Failed to get engine clock\n");

	dvo->pixel_clk = devm_clk_get(dev, "pixel");
	if (IS_ERR(dvo->pixel_clk))
		return dev_err_probe(dev, PTR_ERR(dvo->pixel_clk),
				     "Failed to get pixel clock\n");


	dvo->tvd_clk = devm_clk_get(dev, "pll");
	if (IS_ERR(dvo->tvd_clk))
		return dev_err_probe(dev, PTR_ERR(dvo->tvd_clk),
				     "Failed to get tvdpll clock\n");

	dvo->hf_fdvo_clk = devm_clk_get(dev, "hf_fdvo_clk");
	if (IS_ERR(dvo->hf_fdvo_clk))
		return dev_err_probe(dev, PTR_ERR(dvo->hf_fdvo_clk),
				     "Failed to get hf_fdvo_clk clock\n");

	dvo->dvo_clk = devm_clk_get_optional(dev, "dvo_clk");
	if (IS_ERR(dvo->dvo_clk))
		return dev_err_probe(dev, PTR_ERR(dvo->dvo_clk),
				     "Failed to get dvo_clk clock\n");

	/* TODO: TVDPLL_D* are parent clock divisors which needs to be
	 * set dynamically in the clock framework and not acquired/changed here
	 */
	dvo->pclk_src[1] = devm_clk_get_optional(dev, "TVDPLL_D2");
	dvo->pclk_src[2] = devm_clk_get_optional(dev, "TVDPLL_D4");
	dvo->pclk_src[3] = devm_clk_get_optional(dev, "TVDPLL_D8");
	dvo->pclk_src[4] = devm_clk_get_optional(dev, "TVDPLL_D16");

	dvo->irq = platform_get_irq(pdev, 0);
	if (dvo->irq < 0)
		return dvo->irq;

	platform_set_drvdata(pdev, dvo);

	if (!dvo->conf->is_dp) {
		dvo->next_bridge = devm_drm_of_get_bridge(dev, dev->of_node, 0, 0);
		if (IS_ERR(dvo->next_bridge)) {
			return  dev_err_probe(dev, PTR_ERR(dvo->next_bridge),
					"Failed to get bridge\n");
		}
	} else {
		/* To support MST, the DP bridge (dp-tx) driver registers one bridge
		 * per input port.
		 */
		endpoint = of_graph_get_endpoint_by_regs(dev->of_node, 0, 0);
		if (!endpoint) {
			dev_err(dev, "fail to find endpoint!\n");
			return -ENODEV;
		}

		port = of_graph_get_remote_port(endpoint);
		of_node_put(endpoint);
		if (!port) {
			dev_err(dev, "fail to find remote port!\n");
			return -ENODEV;
		}

		dvo->next_bridge = of_drm_find_bridge(port);
		of_node_put(port);
		if (!dvo->next_bridge) {
			dev_err(dev, "fail to find next bridge\n");
			return -EPROBE_DEFER;
		}
	}

	dev_dbg(dev, "Found bridge node: %pOF\n", dvo->next_bridge->of_node);

	dvo->bridge.funcs = &mtk_dvo_bridge_funcs;
	dvo->bridge.of_node = dev->of_node;
	dvo->bridge.type = DRM_MODE_CONNECTOR_DPI;

	ret = devm_drm_bridge_add(dev, &dvo->bridge);
	if (ret)
		return ret;

	ret = component_add(dev, &mtk_dvo_component_ops);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to add component.\n");

	return 0;
}

static void mtk_dvo_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_dvo_component_ops);
}

static const struct of_device_id mtk_dvo_of_ids[] = {
	{ .compatible = "mediatek,mt8189-edp-dvo", .data = &mt8189_conf },
	{ .compatible = "mediatek,mt8189-dp-dvo", .data = &mt8189_dp_conf },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_dvo_of_ids);

struct platform_driver mtk_dvo_driver = {
	.probe = mtk_dvo_probe,
	.remove_new = mtk_dvo_remove,
	.driver = {
		.name = "mediatek-dvo",
		.of_match_table = mtk_dvo_of_ids,
	},
};
