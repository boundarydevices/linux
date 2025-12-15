// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <drm/display/drm_dp.h>
#include <drm/display/drm_dp_aux_bus.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/display/drm_dsc_helper.h>
#include <drm/display/drm_dsc.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_modes.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>

#include <linux/arm-smccc.h>
#include <linux/atomic.h>
#include <linux/bpf.h>
#include <linux/capability.h>
#include <linux/clk.h>
#include <linux/compat.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/extcon.h>
#include <linux/if_vlan.h>
#include <linux/io.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/linkage.h>
#include <linux/mfd/syscon.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/regmap.h>
#include <linux/refcount.h>
#include <linux/sched.h>
#include <linux/set_memory.h>
#include <linux/skbuff.h>
#include <linux/sockptr.h>
#include <linux/soc/mediatek/mtk-mmsys.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/init.h>
#include <linux/completion.h>

#include <video/videomode.h>

#include "mtk_drm_crtc.h"
#include "mtk_dp_reg_v2.h"
#include "mtk_dp_v2.h"


#define AUX_CMD_I2C_R				0x05
#define AUX_CMD_I2C_R_MOT0			0x01
#define AUX_CMD_NATIVE_R			0x09
#define AUX_CMD_NATIVE_W			0x08
#define AUX_NO_REPLY_WAIT_TIME		3200
#define AUX_WRITE_READ_WAIT_TIME	20 /* us */
#define AUX_WAIT_REPLY_LP_CNT_NUM	20000
#define IEC_CH_STATUS_LEN			5
#define DP_TBC_BUF_READ_START_ADR_THRD	0x08
#define MTK_DP_SIP_CONTROL_AARCH32	MTK_SIP_SMC_CMD(0x523)
#define MAX_MAC_REG_RANG			0x8000
#define MAX_PHYD_REG_RANG			0x1500

#define ENCODER_IRQ_MSK				BIT(0)
#define TRANS_IRQ_MSK				BIT(1)
#define ENCODER_1_IRQ_MSK			BIT(3)

#define MTK_DP_SIP_CONTROL_AARCH32	MTK_SIP_SMC_CMD(0x523)
#define MTK_DP_SIP_ATF_EDP_VIDEO_UNMUTE	(BIT(0) | BIT(5))
#define MTK_DP_SIP_ATF_VIDEO_UNMUTE	BIT(5)

#define MTK_DP_AUX_WAIT_REPLY_COUNT 20

#define MTK_DP_VERSION 0x11
#define MTK_HPD_DEBOUNCE 100

#define HPD_DISCONNECT	BIT(10)
#define HPD_CONNECT		BIT(0)
#define HPD_INTERRUPT	BIT(2)

#define DP_PHY_REG_COUNT			6
#define MTK_DP_CHECK_SINK_CAP_TIMEOUT_COUNT	3
#define MTK_DP_1000_MSECS			1000

#define DP_CTS_RETRAIN_TIMES_14			12
#define DP_CTS_RETRAIN_TIMES_DEFAULT	6
#define DP_LT_RETRY_LIMIT		0x8
#define DP_LT_MAX_LOOP			0x4
#define DP_LT_MAX_CR_LOOP		0x9
#define DP_LT_MAX_EQ_LOOP		0x6

#define ENABLE_DP_EF_MODE		0x1
#if (ENABLE_DP_EF_MODE == 0x01)
#define DP_AUX_SET_ENAHNCED_FRAME	0x80
#else
#define DP_AUX_SET_ENAHNCED_FRAME	0x00
#endif

/**
 * for_each_of_graph_port - iterate over every port in a device or ports node
 * @parent: parent device or ports node containing port
 * @child: loop variable pointing to the current port node
 *
 * When breaking out of the loop, and continue to use the @child, you need to
 * use return_ptr(@child) or no_free_ptr(@child) not to call __free() for it.
 */
#define for_each_of_graph_port(parent, child) \
	for (child = of_graph_get_next_port(parent, NULL);\
	     child != NULL; child = of_graph_get_next_port(parent, child))

struct completion mux_completion;
bool first_ac_on = true;

enum {
	MTK_DP_CAL_GLB_BIAS_TRIM = 0,
	MTK_DP_CAL_CLKTX_IMPSE,
	MTK_DP_CAL_LN_TX_IMPSEL_PMOS_0,
	MTK_DP_CAL_LN_TX_IMPSEL_PMOS_1,
	MTK_DP_CAL_LN_TX_IMPSEL_PMOS_2,
	MTK_DP_CAL_LN_TX_IMPSEL_PMOS_3,
	MTK_DP_CAL_LN_TX_IMPSEL_NMOS_0,
	MTK_DP_CAL_LN_TX_IMPSEL_NMOS_1,
	MTK_DP_CAL_LN_TX_IMPSEL_NMOS_2,
	MTK_DP_CAL_LN_TX_IMPSEL_NMOS_3,
	MTK_DP_CAL_MAX,
};

enum dp_disp_state {
	DP_DISP_STATE_NONE,
	DP_DISP_STATE_RESUME,
	DP_DISP_STATE_SUSPEND,
	DP_DISP_STATE_SUSPENDING,
};

enum aux_reply_cmd {
	AUX_REPLY_ACK = 0x00,
	AUX_DPCD_NACK = BIT(0),
	AUX_DPCD_DEFER = BIT(1),
	AUX_EDID_NACK = BIT(2),
	AUX_EDID_DEFER = BIT(3),
	AUX_HW_FAILED = BIT(4),
	AUX_INVALID_CMD = BIT(5),
};

enum mtk_dp_train_state {
	MTK_DP_TRAIN_STATE_STARTUP = 0,
	MTK_DP_TRAIN_STATE_CHECKCAP,
	MTK_DP_TRAIN_STATE_CHECKEDID,
	MTK_DP_TRAIN_STATE_TRAINING_PRE,
	MTK_DP_TRAIN_STATE_TRAINING,
	MTK_DP_TRAIN_STATE_CHECKTIMING,
	MTK_DP_TRAIN_STATE_NORMAL,
	MTK_DP_TRAIN_STATE_POWERSAVE,
	MTK_DP_TRAIN_STATE_DPIDLE,
};

enum dp_train_stage {
	DP_LT_NONE			= 0x0000,
	DP_LT_CR_L0_FAIL	= 0x0008,
	DP_LT_CR_L1_FAIL	= 0x0009,
	DP_LT_CR_L2_FAIL	= 0x000A,
	DP_LT_EQ_L0_FAIL	= 0x0080,
	DP_LT_EQ_L1_FAIL	= 0x0090,
	DP_LT_EQ_L2_FAIL	= 0x00A0,
	DP_LT_PASS			= 0x7777,
};

enum dp_fec_error_count_type {
	FEC_ERROR_COUNT_DISABLE = 0x0,
	FEC_UNCORRECTED_BLOCK_ERROR_COUNT = 0x1,
	FEC_CORRECTED_BLOCK_ERROR_COUNT = 0x2,
	FEC_BIT_ERROR_COUNT = 0x3,
	FEC_PARITY_BLOCK_ERROR_COUNT = 0x4,
	FEC_PARITY_BIT_ERROR_COUNT = 0x5,
};

enum dp_version {
	DP_VER_11 = 0x11,
	DP_VER_12 = 0x12,
	DP_VER_14 = 0x14,
	DP_VER_12_14 = 0x16,
	DP_VER_14_14 = 0x17,
	DP_VER_MAX,
};

union dp_rx_audio_chsts {
	struct{
		u8 rev : 1;
		u8 is_lpcm : 1;
		u8 copy_right : 1;
		u8 addition_format_info : 3;
		u8 channel_status_mode : 2;
		u8 category_code;
		u8 source_number : 4;
		u8 channel_number : 4;
		u8 sampling_freq : 4;
		u8 clock_accuary : 2;
		u8 rev2 : 2;
		u8 word_len : 4;
		u8 original_sampling_freq : 4;
	} audio_chsts;

	u8 audio_chsts_raw[IEC_CH_STATUS_LEN];
};

enum dp_sdp_asp_hb3_auch {
	DP_SDP_ASP_HB3_AU02CH = 0x01,
	DP_SDP_ASP_HB3_AU08CH = 0x07,
};

enum dp_sdp_hb1_pkg_type {
	DP_SDP_HB1_PKG_RESERVE = 0x00,
	DP_SDP_HB1_PKG_AUDIO_TS = 0x01,
	DP_SDP_HB1_PKG_AUDIO = 0x02,
	DP_SDP_HB1_PKG_EXT = 0x04,
	DP_SDP_HB1_PKG_ACM = 0x05,
	DP_SDP_HB1_PKG_ISRC = 0x06,
	DP_SDP_HB1_PKG_VSC = 0x07,
	DP_SDP_HB1_PKG_CAMERA = 0x08,
	DP_SDP_HB1_PKG_PPS = 0x10,
	DP_SDP_HB1_PKG_EXT_VESA = 0x20,
	DP_SDP_HB1_PKG_EXT_CEA = 0x21,
	DP_SDP_HB1_PKG_NON_AINFO = 0x80,
	DP_SDP_HB1_PKG_VS_INFO = 0x81,
	DP_SDP_HB1_PKG_AVI_INFO = 0x82,
	DP_SDP_HB1_PKG_SPD_INFO = 0x83,
	DP_SDP_HB1_PKG_AINFO = 0x84,
	DP_SDP_HB1_PKG_MPG_INFO = 0x85,
	DP_SDP_HB1_PKG_NTSC_INFO = 0x86,
	DP_SDP_HB1_PKG_DRM_INFO = 0x87,
	DP_SDP_HB1_PKG_MAX_NUM
};

enum dp_sdp_pkg_type {
	DP_SDP_PKG_NONE,
	DP_SDP_PKG_ACM,
	DP_SDP_PKG_ISRC,
	DP_SDP_PKG_AVI,
	DP_SDP_PKG_AUI,
	DP_SDP_PKG_SPD,
	DP_SDP_PKG_MPEG,
	DP_SDP_PKG_NTSC,
	DP_SDP_PKG_VSP,
	DP_SDP_PKG_VSC,
	DP_SDP_PKG_EXT,
	DP_SDP_PKG_PPS0,
	DP_SDP_PKG_PPS1,
	DP_SDP_PKG_PPS2,
	DP_SDP_PKG_PPS3,
	DP_SDP_PKG_RESERVED,
	DP_SDP_PKG_DRM,
	DP_SDP_PKG_ADS,
	DP_SDP_PKG_MAX_NUM
};

enum dp_pg_location {
	DP_PG_LOCATION_NONE,
	DP_PG_LOCATION_ALL,
	DP_PG_LOCATION_TOP,
	DP_PG_LOCATION_BOTTOM,
	DP_PG_LOCATION_LEFT_OF_TOP,
	DP_PG_LOCATION_LEFT_OF_BOTTOM,
	DP_PG_LOCATION_LEFT,
	DP_PG_LOCATION_RIGHT,
	DP_PG_LOCATION_LEFT_OF_LEFT,
	DP_PG_LOCATION_RIGHT_OF_LEFT,
	DP_PG_LOCATION_LEFT_OF_RIGHT,
	DP_PG_LOCATION_RIGHT_OF_RIGHT,
	DP_PG_LOCATION_MAX,
};

enum dp_pg_pixel_mask {
	DP_PG_PIXEL_MASK_NONE,
	DP_PG_PIXEL_ODD_MASK,
	DP_PG_PIXEL_EVEN_MASK,
	DP_PG_PIXEL_MASK_MAX,
};

enum dp_pg_purecolor {
	DP_PG_PURECOLOR_NONE,
	DP_PG_PURECOLOR_BLUE,
	DP_PG_PURECOLOR_GREEN,
	DP_PG_PURECOLOR_RED,
	DP_PG_PURECOLOR_MAX,
};

enum dp_pg_sel {
	DP_PG_20BIT,
	DP_PG_80BIT,
	DP_PG_11BIT,
	DP_PG_8BIT,
	DP_PG_PRBS7,
};

static const u32 mt8196_input_fmts[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUV8_1X24,
	MEDIA_BUS_FMT_YUYV8_1X16,
};

static const u32 mt8196_output_fmts[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUV8_1X24,
	MEDIA_BUS_FMT_YUYV8_1X16,
};

static const struct mtk_dp_efuse_fmt mt8188_dp_efuse_fmt[MTK_DP_CAL_MAX] = {
	[MTK_DP_CAL_GLB_BIAS_TRIM] = {
		.idx = 0,
		.shift = 10,
		.mask = 0x1f,
		.min_val = 1,
		.max_val = 0x1e,
		.default_val = 0xf,
	},
	[MTK_DP_CAL_CLKTX_IMPSE] = {
		.idx = 0,
		.shift = 15,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_0] = {
		.idx = 1,
		.shift = 0,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_1] = {
		.idx = 1,
		.shift = 8,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_2] = {
		.idx = 1,
		.shift = 16,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_3] = {
		.idx = 1,
		.shift = 24,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_0] = {
		.idx = 1,
		.shift = 4,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_1] = {
		.idx = 1,
		.shift = 12,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_2] = {
		.idx = 1,
		.shift = 20,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_3] = {
		.idx = 1,
		.shift = 28,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
};

static const struct mtk_dp_efuse_fmt mt8195_edp_efuse_fmt[MTK_DP_CAL_MAX] = {
	[MTK_DP_CAL_GLB_BIAS_TRIM] = {
		.idx = 3,
		.shift = 27,
		.mask = 0x1f,
		.min_val = 1,
		.max_val = 0x1e,
		.default_val = 0xf,
	},
	[MTK_DP_CAL_CLKTX_IMPSE] = {
		.idx = 0,
		.shift = 9,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_0] = {
		.idx = 2,
		.shift = 28,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_1] = {
		.idx = 2,
		.shift = 20,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_2] = {
		.idx = 2,
		.shift = 12,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_3] = {
		.idx = 2,
		.shift = 4,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_0] = {
		.idx = 2,
		.shift = 24,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_1] = {
		.idx = 2,
		.shift = 16,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_2] = {
		.idx = 2,
		.shift = 8,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_3] = {
		.idx = 2,
		.shift = 0,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
};

static const struct mtk_dp_efuse_fmt mt8195_dp_efuse_fmt[MTK_DP_CAL_MAX] = {
	[MTK_DP_CAL_GLB_BIAS_TRIM] = {
		.idx = 0,
		.shift = 27,
		.mask = 0x1f,
		.min_val = 1,
		.max_val = 0x1e,
		.default_val = 0xf,
	},
	[MTK_DP_CAL_CLKTX_IMPSE] = {
		.idx = 0,
		.shift = 13,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_0] = {
		.idx = 1,
		.shift = 28,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_1] = {
		.idx = 1,
		.shift = 20,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_2] = {
		.idx = 1,
		.shift = 12,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_3] = {
		.idx = 1,
		.shift = 4,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_0] = {
		.idx = 1,
		.shift = 24,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_1] = {
		.idx = 1,
		.shift = 16,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_2] = {
		.idx = 1,
		.shift = 8,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_3] = {
		.idx = 1,
		.shift = 0,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
};

/**
 * of_graph_get_next_port() - get next port node.
 * @parent: pointer to the parent device node, or parent ports node
 * @prev: previous port node, or NULL to get first
 *
 * Parent device node can be used as @parent whether device node has ports node
 * or not. It will work same as ports@0 node.
 *
 * Return: A 'port' node pointer with refcount incremented. Refcount
 * of the passed @prev node is decremented.
 */
struct device_node *of_graph_get_next_port(const struct device_node *parent,
					   struct device_node *prev)
{
	if (!parent)
		return NULL;

	if (!prev) {
		struct device_node *node;

		node = of_get_child_by_name(parent, "ports");
		if (node)
			parent = node;

		return of_get_child_by_name(parent, "port");
	}

	do {
		prev = of_get_next_child(parent, prev);
		if (!prev)
			break;
	} while (!of_node_name_eq(prev, "port"));

	return prev;
}

static struct mtk_dp *mtk_dp_from_bridge(struct drm_bridge *b)
{
	return container_of(b, struct mtk_dp, bridge);
}

static u32 mtk_dp_read(struct mtk_dp *mtk_dp, u32 offset)
{
	u32 read_val = 0;

	if (offset > MAX_MAC_REG_RANG) {
		dev_err(mtk_dp->dev, "[DPTX] %s, error reg:0x%p, offset:0x%x\n",
			__func__, mtk_dp->regs, offset);
		return 0;
	}

	read_val = readl(mtk_dp->regs + offset - (offset % 4))
			 >> ((offset % 4) * 8);

	return read_val;
}

static void mtk_dp_write(struct mtk_dp *mtk_dp, u32 offset, u32 val)
{
	if ((offset % 4 != 0) || offset > MAX_MAC_REG_RANG) {
		dev_err(mtk_dp->dev, "[DPTX] %s, error reg:0x%p, offset:0x%x, value:0x%x\n",
			__func__, mtk_dp->regs, offset, val);
		return;
	}

	writel(val, mtk_dp->regs + offset);
}

void mtk_dp_mask(struct mtk_dp *mtk_dp, u32 offset, u32 val, u32 mask)
{
	void __iomem *reg = mtk_dp->regs + offset;
	u32 tmp;

	if ((offset % 4 != 0) || offset > MAX_MAC_REG_RANG) {
		dev_err(mtk_dp->dev, "[DPTX] %s, error reg:0x%p, offset:0x%x, value:0x%x\n",
			__func__, mtk_dp->regs, offset, val);
		return;
	}

	tmp = readl(reg);
	tmp = (tmp & ~mask) | (val & mask);
	writel(tmp, reg);
}

static int mtk_dp_update_bits(struct mtk_dp *mtk_dp, u32 offset,
			      u32 val, u32 mask)
{
	mtk_dp_mask(mtk_dp, offset, val, mask);

	return 0;
}

void mtk_dp_write_byte(struct mtk_dp *mtk_dp,
			  u32 addr, u8 val, u32 mask)
{
	if (addr % 2)
		mtk_dp_mask(mtk_dp, addr - 1, (u32)(val << 8), (mask << 8));
	else
		mtk_dp_mask(mtk_dp, addr, (u32)val, mask);
}

static u32 mtk_dp_phy_read(struct mtk_dp *mtk_dp, u32 offset)
{
	u32 read_val = 0;

	if (offset > MAX_PHYD_REG_RANG) {
		dev_err(mtk_dp->dev, "[DPTX] %s, error offset:0x%x\n",
			__func__, offset);
		return 0;
	}

	read_val = readl(mtk_dp->phyd_regs + offset - (offset % 4))
			 >> ((offset % 4) * 8);

	return read_val;
}

static void mtk_dp_phy_write(struct mtk_dp *mtk_dp, u32 offset, u32 val)
{
	if ((offset % 4 != 0) || offset > MAX_PHYD_REG_RANG) {
		dev_err(mtk_dp->dev, "[DPTX] %s, error offset:0x%x, value:0x%x\n",
			__func__, offset, val);
		return;
	}

	writel(val, mtk_dp->phyd_regs + offset);
}

static void mtk_dp_phy_mask(struct mtk_dp *mtk_dp, u32 offset, u32 val, u32 mask)
{
	void __iomem *reg = mtk_dp->phyd_regs + offset;
	u32 tmp;

	if ((offset % 4 != 0) || offset > MAX_PHYD_REG_RANG) {
		dev_err(mtk_dp->dev, "[DPTX] %s, error reg:0x%p, offset:0x%x, value:0x%x\n",
			__func__, mtk_dp->phyd_regs, offset, val);
		return;
	}

	tmp = readl(reg);
	tmp = (tmp & ~mask) | (val & mask);
	writel(tmp, reg);
}

static void mtk_dp_phy_write_byte(struct mtk_dp *mtk_dp,
				  u32 addr, u8 val, u32 mask)
{
	if (addr % 2)
		mtk_dp_phy_mask(mtk_dp, addr - 1, (u32)(val << 8), (mask << 8));
	else
		mtk_dp_phy_mask(mtk_dp, addr, (u32)val, mask);
}

static void mtk_dp_color_set_format(struct mtk_dp *mtk_dp,
				    const enum dp_encoder_id encoder_id, u8 color_format)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	dev_dbg(mtk_dp->dev, "[DPTX] Set Color Format:0x%x\n", color_format);

	if (color_format == DP_PIXELFORMAT_RGB ||
	    color_format == DP_PIXELFORMAT_YUV444)
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_303C + 1 + reg_offset,
				(0), GENMASK(6, 4));
	else if (color_format == DP_PIXELFORMAT_YUV422)
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_303C + 1 + reg_offset,
				(BIT(4)), GENMASK(6, 4));
	else if (color_format == DP_PIXELFORMAT_YUV420)
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_303C + 1 + reg_offset,
				(BIT(5)), GENMASK(6, 4));
}

static void mtk_dp_color_set_depth(struct mtk_dp *mtk_dp,
				   const enum dp_encoder_id encoder_id, u8 color_depth)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	dev_dbg(mtk_dp->dev,
		    "[DPTX] Set Color Depth:%d (0~4=6/8/10/12/16 bpp)\n", color_depth);

	switch (color_depth) {
	case DP_COLOR_DEPTH_6BIT:
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_303C + 1 + reg_offset, 4, 0x07);
		break;
	case DP_COLOR_DEPTH_8BIT:
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_303C + 1 + reg_offset, 3, 0x07);
		break;
	case DP_COLOR_DEPTH_10BIT:
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_303C + 1 + reg_offset, 2, 0x07);
		break;
	case DP_COLOR_DEPTH_12BIT:
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_303C + 1 + reg_offset, 1, 0x07);
		break;
	case DP_COLOR_DEPTH_16BIT:
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_303C + 1 + reg_offset, 0, 0x07);
		break;
	default:
		break;
	}
}

static void mtk_dp_pg_enable(struct mtk_dp *mtk_dp,
			     const enum dp_encoder_id encoder_id, bool enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	if (enable)
		WRITE_BYTE_MASK(mtk_dp,
				MTK_DP_ENC0_P0_3038 + 1 + reg_offset, BIT(3), BIT(3));
	else
		WRITE_BYTE_MASK(mtk_dp,
				MTK_DP_ENC0_P0_3038 + 1 + reg_offset, 0, BIT(3));
}

static void mtk_dp_set_swing_pre_emphasis(struct mtk_dp *mtk_dp, int lane_num,
					  int swing_val, int preemphasis)
{
	u32 lane_shift = lane_num * DP_TX1_VOLT_SWING_SHIFT;

	dev_dbg(mtk_dp->dev,
		"link training: swing_val = 0x%x, pre-emphasis = 0x%x\n",
		swing_val, preemphasis);

	mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_SWING_EMP,
			   swing_val << (DP_TX0_VOLT_SWING_SHIFT + lane_shift),
			   DP_TX0_VOLT_SWING_MASK << lane_shift);
	mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_SWING_EMP,
			   preemphasis << (DP_TX0_PRE_EMPH_SHIFT + lane_shift),
			   DP_TX0_PRE_EMPH_MASK << lane_shift);
}

static void mtk_dp_phy_reset_swing_pre(struct mtk_dp *mtk_dp)
{
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan0_offset + DRIVING_FORCE,
					(0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
					DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan0_offset + DRIVING_FORCE,
					(0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
					DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan1_offset + DRIVING_FORCE,
					(0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
					DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan1_offset + DRIVING_FORCE,
					(0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
					DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan2_offset + DRIVING_FORCE,
					(0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
					DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan2_offset + DRIVING_FORCE,
					(0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
					DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan3_offset + DRIVING_FORCE,
					(0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
					DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan3_offset + DRIVING_FORCE,
					(0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
					DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);

	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan0_offset + DRIVING_FORCE,
					(0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
					DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan0_offset + DRIVING_FORCE,
					(0x0  << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
					DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan1_offset + DRIVING_FORCE,
					(0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
					DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan1_offset + DRIVING_FORCE,
					(0x0 << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
					DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan2_offset + DRIVING_FORCE,
					(0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
					DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan2_offset + DRIVING_FORCE,
					(0x0 << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
					DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan3_offset + DRIVING_FORCE,
					(0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
					DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
	PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan3_offset + DRIVING_FORCE,
					(0x0 << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
					DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
}

static u32 mtk_dp_hpd_get_irq_status(struct mtk_dp *mtk_dp)
{
	return mtk_dp_read(mtk_dp, MTK_DP_AUX_P0_3608);
}

static void mtk_dp_hpd_interrupt_clr(struct mtk_dp *mtk_dp, u32 irq_status)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3668, irq_status,
			DP_TX_INT_CLR_AUX_TX_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3668, 0,
			DP_TX_INT_CLR_AUX_TX_P0_MASK);
}

static void mtk_dp_hpd_interrupt_enable(struct mtk_dp *mtk_dp, bool enable)
{
	WRITE_4BYTE_MASK(mtk_dp, MTK_DP_TOP_IRQ_MASK_CTRL,
			 TRANS_IRQ_MSK | ENCODER_IRQ_MSK,
			 TRANS_IRQ_MSK | ENCODER_IRQ_MSK);

	/* [7]:int[6]:Con[5]DisCon[4]No-Use:UnMASK HPD Port */
	if (enable)
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3660, 0x0,
				 HPD_DISCONNECT | HPD_CONNECT | HPD_INT_EVNET);
	else
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3660,
				 DP_TX_INT_MASK_AUX_TX_P0_MASK,
				 DP_TX_INT_MASK_AUX_TX_P0_MASK);
}

static void mtk_dp_fec_enable(struct mtk_dp *mtk_dp)
{
	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] FEC enable\n");

	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_3540, BIT(0), FEC_EN_DP_TRANS_P0_MASK);
}

static void mtk_dp_fec_disable(struct mtk_dp *mtk_dp)
{
	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] FEC disable\n");

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_TRANS_P0_3540, 0, BIT(0));
}

static void mtk_dp_fec_init_setting(struct mtk_dp *mtk_dp)
{
	WRITE_4BYTE_MASK(mtk_dp, MTK_DP_TRANS_P0_3540,
			 1 << FEC_CLOCK_EN_MODE_DP_TRANS_P0_FLDMASK_POS,
			 FEC_CLOCK_EN_MODE_DP_TRANS_P0);
	WRITE_4BYTE_MASK(mtk_dp, MTK_DP_TRANS_P0_3540,
			 2 << FEC_FIFO_UNDER_POINT_DP_TRANS_P0_FLDMASK_POS,
			 FEC_FIFO_UNDER_POINT_DP_TRANS_P0);
}

static void mtk_dp_initialize_settings(struct mtk_dp *mtk_dp)
{
	enum dp_encoder_id encoder_id;
	u32 reg_offset;

	WRITE_4BYTE_MASK(mtk_dp, MTK_DP_TOP_PWR_STATE,
			 (0x3 << DP_PWR_STATE_FLDMASK_POS), DP_PWR_STATE_MASK);

	/* 26M xtal clock */
	WRITE_BYTE(mtk_dp, MTK_DP_TRANS_P0_342C, 0x68);

	dev_dbg(mtk_dp->dev, " %s 0x342C = 0x%x\n", __func__,
		mtk_dp_read(mtk_dp, MTK_DP_TRANS_P0_342C));

	mtk_dp_fec_init_setting(mtk_dp);

	for (encoder_id = 0; encoder_id < mtk_dp->data->encoder_num; encoder_id++) {
		reg_offset = MTK_DP_REG_OFFSET(encoder_id);
		WRITE_4BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_31EC + reg_offset, BIT(4), BIT(4));
		WRITE_4BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_304C + reg_offset, 0, BIT(8));
		WRITE_4BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_304C + reg_offset, BIT(3), BIT(3));
	}
}

static void mtk_dp_initialize_hpd_detect_settings(struct mtk_dp *mtk_dp)
{
	/* Crystal frequency value for 1us timing normalization */
	/* [7:2]: Integer value */
	/* [1:0]: Fractional value */
	/* 0x30: 12.0us, 0x68: 26us */
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_366C,
			   XTAL_FREQ_VAL << XTAL_FREQ_AUX_TX_P0_FLDMASK_POS,
			   XTAL_FREQ_AUX_TX_P0_MASK);

	/* Adjust Tx reg_hpd_disc_thd to 2ms */
	/* it is because of the spec "HPD pulse" description */
	/* Low Bound: 3'b010 ~ 500us */
	/* Up Bound: 3'b110 ~1.9ms */
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_364C,
			   HPD_INT_THD_VAL << HPD_INT_THD_AUX_TX_P0_FLDMASK_POS,
			   HPD_INT_THD_AUX_TX_P0_MASK);
}

static void mtk_dp_initialize_aux_settings(struct mtk_dp *mtk_dp)
{
	/* modify timeout threshold = 0x1D0C */
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_360C,
			 AUX_TIMEOUT_THR_AUX_TX_P0_VAL,
			 AUX_TIMEOUT_THR_AUX_TX_P0_MASK);

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3658, 0, BIT(0));

	WRITE_2BYTE(mtk_dp, MTK_DP_AUX_P0_36A0, 0xfffc);

	/* 25 for 26M */
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3634,
			 0x19 << AUX_TX_OVER_SAMPLE_RATE_AUX_TX_P0_FLDMASK_POS,
			 AUX_TX_OVER_SAMPLE_RATE_AUX_TX_P0_MASK);

	/* 13 for 26M */
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3614,
			0x0D << AUX_RX_UI_CNT_THR_AUX_TX_P0_FLDMASK_POS,
			AUX_RX_UI_CNT_THR_AUX_TX_P0_MASK);

	WRITE_4BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_37C8, MTK_ATOP_EN_AUX_TX_P0,
			 MTK_ATOP_EN_AUX_TX_P0);

	/* disable aux sync_stop detect function */
	WRITE_4BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3690,
			 0x1 << RX_REPLY_COMPLETE_MODE_AUX_TX_P0_FLDMASK_POS,
			 RX_REPLY_COMPLETE_MODE_AUX_TX_P0);

	/* Con Thd = 1.5ms+Vx0.1ms */
	WRITE_4BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_367C,
			 5 << HPD_CONN_THD_AUX_TX_P0_FLDMASK_POS,
			 HPD_CONN_THD_AUX_TX_P0_MASK);
	/* DisCon Thd = 1.5ms+Vx0.1ms */
	WRITE_4BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_37A0,
			 5 << HPD_DISC_THD_AUX_TX_P0_FLDMASK_POS,
			 HPD_DISC_THD_AUX_TX_P0_MASK);

	WRITE_4BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3690,
			 RX_REPLY_COMPLETE_MODE_AUX_TX_P0,
			 RX_REPLY_COMPLETE_MODE_AUX_TX_P0);
}

static void mtk_dp_spkg_asp_hb32_v2(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				    u8 enable, u8 HB3, u8 HB2)
{
	u32 reg_offset = MTK_DP_REG_OFFSET(encoder_id);

	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_30BC + reg_offset,
			 (enable ? 0x01 : 0x00) << ASP_HB23_SEL_DP_ENC0_P0_FLDMASK_POS,
			 ASP_HB23_SEL_DP_ENC0_P0_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_312C + reg_offset,
			 HB2 << ASP_HB2_DP_ENC0_P0_FLDMASK_POS,
			 ASP_HB2_DP_ENC0_P0_MASK);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_312C + reg_offset,
			 HB3 << ASP_HB3_DP_ENC0_P0_FLDMASK_POS,
			 ASP_HB3_DP_ENC0_P0_MASK);
}

static void mtk_dp_encoder_reset(struct mtk_dp *mtk_dp,
				 const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = MTK_DP_REG_OFFSET(encoder_id);

	/* dp tx encoder reset all sw */
	WRITE_2BYTE_MASK(mtk_dp, (MTK_DP_ENC0_P0_3004 + reg_offset),
			 1 << DP_TX_ENCODER_4P_RESET_SW_DP_ENC0_P0_FLDMASK_POS,
			 DP_TX_ENCODER_4P_RESET_SW_DP_ENC0_P0);

	mdelay(1);

	WRITE_2BYTE_MASK(mtk_dp, (MTK_DP_ENC0_P0_3004 + reg_offset),
			 0,
			 DP_TX_ENCODER_4P_RESET_SW_DP_ENC0_P0);
}

static void mtk_dp_initialize_digital_settings(struct mtk_dp *mtk_dp,
					       const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = MTK_DP_REG_OFFSET(encoder_id);

	mtk_dp_spkg_asp_hb32_v2(mtk_dp, encoder_id, false, DP_SDP_ASP_HB3_AU02CH, 0x0);

	/* Mengkun suggest: disable reg_sdp_down_cnt_new_mode */
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_304C + reg_offset, 0,
			SDP_DOWN_CNT_NEW_MODE_DP_ENC0_P0_MASK);
	/* reg_sdp_asp_insert_in_hblank: default = 1 */
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3374 + reg_offset,
			 0x1 << SDP_ASP_INSERT_IN_HBLANK_DP_ENC1_P0_FLDMASK_POS,
			 SDP_ASP_INSERT_IN_HBLANK_DP_ENC1_P0_MASK);

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_304C + reg_offset, 0,
			VBID_VIDEO_MUTE_DP_ENC0_P0_MASK);

	mtk_dp_color_set_format(mtk_dp, encoder_id, mtk_dp->info[encoder_id].format);
	mtk_dp_color_set_depth(mtk_dp, encoder_id, mtk_dp->info[encoder_id].depth);

	WRITE_4BYTE(mtk_dp, MTK_DP_ENC1_P0_3368 + reg_offset,
		    (0x1 << 15) |
		    (0x4 << BS2BS_MODE_DP_ENC1_P0_FLDMASK_POS) |
		    (0x1 << SDP_DP13_EN_DP_ENC1_P0_FLDMASK_POS) |
		    (0x1 << VIDEO_STABLE_CNT_THRD_DP_ENC1_P0_FLDMASK_POS) |
		    (0x1 << VIDEO_SRAM_FIFO_CNT_RESET_SEL_DP_ENC1_P0_FLDMASK_POS));

	mtk_dp_encoder_reset(mtk_dp, encoder_id);
}

static void mtk_dp_digital_sw_reset(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_340C,
			   DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0,
			   DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0);

	/* Wait for sw reset to complete */
	usleep_range(1000, 5000);
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_340C,
			   0, DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0);
}

static void mtk_dp_set_idle_pattern(struct mtk_dp *mtk_dp, bool enable)
{
	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] Idle pattern enable:%d\n", enable);
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_TRANS_P0_3580 + 1, enable ? 0x0f : 0x00, 0x0f);
}

static void mtk_dp_train_set_pattern(struct mtk_dp *mtk_dp, int pattern)
{
	dev_dbg(mtk_dp->dev, "[DPTX] Set Train Pattern:0x%x\n", pattern);

	if (pattern <= DP_TPS4) {
		if (pattern == DP_TPS1)
			mtk_dp_set_idle_pattern(mtk_dp, false);

		WRITE_BYTE_MASK(mtk_dp, (MTK_DP_TRANS_P0_3400 + 1), pattern, GENMASK(7, 4));
	}
	mdelay(20);
}

static unsigned long mtk_dp_atf_call(struct mtk_dp *mtk_dp, unsigned int cmd, unsigned int para)
{
	struct arm_smccc_res res;
	u32 x3 = (cmd << 16) | para;

	arm_smccc_smc(MTK_DP_SIP_CONTROL_AARCH32, cmd, para,
		      x3, 0xfefd, 0, 0, 0, &res);

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] %s, cmd:0x%x, p1:0x%x, ret:0x%lx-0x%lx",
		    __func__, cmd, para, res.a0, res.a1);

	return res.a1;
}

static void mtk_dp_training_set_scramble(struct mtk_dp *mtk_dp, bool enable)
{
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_TRANS_P0_3404, enable ? BIT(0) : 0, BIT(0));
}

static void mtk_dp_video_mute_sw(struct mtk_dp *mtk_dp,
				 const enum dp_encoder_id encoder_id, bool enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] encoder:%d, enable:%d\n", encoder_id, enable);

	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_304C + reg_offset, enable ? BIT(2) : 0, BIT(2));
}

void mtk_dp_video_mute(struct mtk_dp *mtk_dp,
		       const enum dp_encoder_id encoder_id, bool enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	dev_dbg(mtk_dp->dev, "[DPTX] encoder:%d, video mute enable:%d\n", encoder_id, enable);

	mtk_dp->info[encoder_id].video_mute = enable;

	mtk_dp_video_mute_sw(mtk_dp, encoder_id, enable);

	if (enable) {
		mtk_dp_update_bits(mtk_dp,
				   MTK_DP_ENC0_P0_3000 + reg_offset,
				   BIT(3) | BIT(2),
				   GENMASK(3, 2));

		/* Video mute enable */
		mtk_dp_atf_call(mtk_dp, MTK_DP_SIP_ATF_VIDEO_UNMUTE, 1);
	} else {
		mtk_dp_update_bits(mtk_dp,
				   MTK_DP_ENC0_P0_3000 + reg_offset,
				   BIT(3),
				   GENMASK(3, 2));

		/* [3] Sw ov Mode [2] mute value */
		mtk_dp_atf_call(mtk_dp, MTK_DP_SIP_ATF_VIDEO_UNMUTE, 0);
	}

	if (mtk_dp->dsc_enable[encoder_id])
		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_31C4 + reg_offset,
				   (enable ? 1 : 0) << DSC_BYPASS_EN_DP_ENC0_P0_FLDMASK_POS,
				   DSC_BYPASS_EN_DP_ENC0_P0_MASK);

	mtk_dp_update_bits(mtk_dp, 0x402c, 0, BIT(4));
	mtk_dp_update_bits(mtk_dp, 0x402c, 1, BIT(4));
}

static void mtk_dp_audio_mute(struct mtk_dp *mtk_dp,
			      const enum dp_encoder_id encoder_id, bool enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] [%d] enable:%d\n", encoder_id, enable);

	mtk_dp->info[encoder_id].audio_mute = enable;

	if (enable) {
		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3030 + reg_offset,
				   VBID_AUDIO_MUTE_FLAG_SW_DP_ENC0_P0,
				   VBID_AUDIO_MUTE_FLAG_SW_DP_ENC0_P0);

		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3030 + reg_offset,
				   VBID_AUDIO_MUTE_FLAG_SEL_DP_ENC0_P0,
				   VBID_AUDIO_MUTE_FLAG_SEL_DP_ENC0_P0);

		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3088 + reg_offset,
				   0x0, AU_EN_DP_ENC0_P0);
		mtk_dp_write(mtk_dp, MTK_DP_ENC0_P0_30A4 + reg_offset, 0x00);

		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_33F0 + reg_offset, BIT(9), BIT(9));
		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_33F0 + reg_offset, 0x0, BIT(9));
	} else {
		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3030 + reg_offset, 0x00,
				   VBID_AUDIO_MUTE_FLAG_SEL_DP_ENC0_P0);

		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3088 + reg_offset,
				   AU_EN_DP_ENC0_P0,
				   AU_EN_DP_ENC0_P0);

		mtk_dp_write(mtk_dp, MTK_DP_ENC0_P0_30A4 + reg_offset,
			     (1 << AUDIO_TS_SEND_EN_DP_ENC0_P0_FLDMASK_POS) |
			     (AUDIO_TS_SEND_FREQ_TYPE_ONE_IN_EVERY_N_PLUS_1_FRAMES
			     << AUDIO_TS_SEND_FREQ_TYPE_DP_ENC0_P0_FLDMASK_POS));
	}
}

static void mtk_dp_aux_panel_poweron(struct mtk_dp *mtk_dp, bool pwron)
{
	if (pwron) {
		/* power on aux */
		mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_PWR_STATE,
				   DP_PWR_STATE_BANDGAP_TPLL_LANE,
				   DP_PWR_STATE_MASK);

		/* power on panel */
		drm_dp_dpcd_writeb(&mtk_dp->aux, DP_SET_POWER, DP_SET_POWER_D0);
		usleep_range(2000, 5000);
	} else {
		/* power off panel */
		drm_dp_dpcd_writeb(&mtk_dp->aux, DP_SET_POWER, DP_SET_POWER_D3);
		usleep_range(2000, 3000);

		/* power off aux */
		mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_PWR_STATE,
				   DP_PWR_STATE_BANDGAP_TPLL,
				   DP_PWR_STATE_MASK);
	}
}

static void mtk_dp_analog_power_on(struct mtk_dp *mtk_dp)
{
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_TOP_RESET_AND_PROBE, 0, BIT(4));
	udelay(10);
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_TOP_RESET_AND_PROBE, BIT(4), BIT(4));
	WRITE_2BYTE(mtk_dp, MTK_DP_TOP_PWR_STATE, 0x0);
}

static void mtk_dp_power_enable(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_PWR_STATE,
			   DP_PWR_STATE_BANDGAP_TPLL, DP_PWR_STATE_MASK);
}

static void mtk_dp_power_disable(struct mtk_dp *mtk_dp)
{
	mtk_dp_write(mtk_dp, MTK_DP_TOP_PWR_STATE, 0);

	mtk_dp_update_bits(mtk_dp, MTK_DP_0034,
			   DA_CKM_CKTX0_EN_FORCE_EN, DA_CKM_CKTX0_EN_FORCE_EN);

	/* Disable RX */
	mtk_dp_write(mtk_dp, MTK_DP_1040, 0);
	mtk_dp_write(mtk_dp, MTK_DP_TOP_MEM_PD,
		     0x550 | FUSE_SEL | MEM_ISO_EN);
}

static void mtk_dp_init_variable(struct mtk_dp *mtk_dp)
{
	enum dp_encoder_id encoder_id;

	mtk_dp->train_info.dp_version = DP_VER_14;
	mtk_dp->train_info.max_link_rate = mtk_dp->data->support_max_linkrate;
	mtk_dp->train_info.max_link_lane_count = mtk_dp->data->support_max_lanecount;
	mtk_dp->train_info.sink_ext_cap_en = false;
	mtk_dp->train_info.sink_ssc_en = false;
	mtk_dp->train_info.tps3_support = true;
	mtk_dp->train_info.tps4_support = true;
	mtk_dp->train_info.phy_status = HPD_INITIAL_STATE;
	mtk_dp->train_info.cable_plug_in = false;
	for (encoder_id = 0; encoder_id < mtk_dp->data->encoder_num; encoder_id++) {
		mtk_dp->info[encoder_id].depth = DP_COLOR_DEPTH_8BIT;
		memset(&mtk_dp->info[encoder_id].dp_output_timing, 0,
		       sizeof(struct mtk_dp_timing_parameter));
		mtk_dp->info[encoder_id].dp_output_timing.frame_rate = 60;
		mtk_dp->dsc_enable[encoder_id] = false;
	}
	mtk_dp->dp_ready = false;
	mtk_dp->need_debounce = false;
	mtk_dp->audio_enable = false;
}

static void mtk_dp_set_lane_count(struct mtk_dp *mtk_dp, const enum dp_lane_count lane_count)
{
	const u8 value = lane_count >> 1;
	enum dp_encoder_id encoder_id;
	u32 reg_offset;

	if (value == 0) {
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_TRANS_P0_35F0, 0, GENMASK(3, 2));
	} else if (value < mtk_dp->train_info.max_link_lane_count) {
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_TRANS_P0_35F0, BIT(3), GENMASK(3, 2));
	} else {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] Un-expected lane count:%d\n", lane_count);
		return;
	}

	for (encoder_id = 0; encoder_id < mtk_dp->data->encoder_num; encoder_id++) {
		reg_offset = DP_REG_OFFSET(encoder_id);

		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3000 + reg_offset,
				value << LANE_NUM_DP_ENC0_P0_FLDMASK_POS,
				LANE_NUM_DP_ENC0_P0_MASK);
	}

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_TRANS_P0_34A4,
			value << LANE_NUM_DP_TRANS_P0_FLDMASK_POS,
			LANE_NUM_DP_TRANS_P0_MASK);
}

static bool mtk_dp_plug_state(struct mtk_dp *mtk_dp)
{
	bool plug_state = false;

	u32 ret = ((mtk_dp_read(mtk_dp, MTK_DP_AUX_P0_364C) &
		   HPD_STATUS_DP_AUX_TX_P0_MASK) >> HPD_STATUS_DP_AUX_TX_P0_FLDMASK_POS);

	if (ret > 0)
		plug_state = true;

	return plug_state;
}

static void mtk_dp_fec_ready(struct mtk_dp *mtk_dp)
{
	u8 value;

	if (!mtk_dp->mtk_con[DP_FIRST_CON])
		return;

	if (drm_dp_dpcd_readb(&mtk_dp->aux, DP_FEC_CAPABILITY, &value) < 0) {
		dev_err(mtk_dp->dev, "[DPTX] fail to read FEC DPCD\n");
		return;
	}

	mtk_dp->mtk_con[DP_FIRST_CON]->fec_cap = value;

	if (drm_dp_sink_supports_fec(mtk_dp->mtk_con[DP_FIRST_CON]->fec_cap)) {
		if (drm_dp_dpcd_writeb(&mtk_dp->aux, DP_FEC_CONFIGURATION, DP_FEC_READY) <= 0)
			dev_err(mtk_dp->dev, "[DPTX] Failed to set FEC_READY\n");

		if (drm_dp_dpcd_writeb(&mtk_dp->aux, DP_FEC_STATUS,
				DP_FEC_DECODE_EN_DETECTED | DP_FEC_DECODE_DIS_DETECTED) <= 0)
			dev_err(mtk_dp->dev, "[DPTX] Failed to clear FEC detected flags\n");
	}
}

static bool mtk_dp_ssc_check(struct mtk_dp *mtk_dp, bool *p_enable)
{
	u8 status = 0;
	int ret = 0;

	*p_enable = mtk_dp->train_info.sink_ssc_en;
	/* write DPCD_00107 = BIT4 when SSC enable */

	status = *p_enable ? BIT(4) : 0;
	ret = drm_dp_dpcd_write(&mtk_dp->aux, DP_DOWNSPREAD_CTRL, &status, 0x1);

	if (ret < 0) {
		*p_enable = false;
		dev_err(mtk_dp->dev, "[DPTX] Write DPCD_00107 Fail!!!\n");
		return false;
	}

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] %s SSC via DPCD_00107\n",
		    (*p_enable) ? "Enable" : "Disable ");

	return true;
}

static u8 mtk_dp_get_sink_count(struct mtk_dp *mtk_dp)
{
	u8 tmp = 0;
	int ret;

	if (mtk_dp->train_info.sink_ext_cap_en) {
		/* DPCD_02002 */
		ret = drm_dp_dpcd_read(&mtk_dp->aux, DP_SINK_COUNT_ESI, &tmp, 0x1);
	} else {
		/* DPCD_00200 */
		ret = drm_dp_dpcd_read(&mtk_dp->aux, DP_SINK_COUNT, &tmp, 0x1);
	}

	if (ret < 0) {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] Failed to read DPCD: %d\n", ret);
		return 0;
	}

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] sink count:%d\n", DP_GET_SINK_COUNT(tmp));
	return DP_GET_SINK_COUNT(tmp);
}

static void mtk_dsc_read_dsc_dpcd(struct mtk_dp *mtk_dp, struct drm_dp_aux *aux,
				  u8 dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE])
{
	dev_dbg(mtk_dp->dev, "[DPTX] read DPCD_00060\n");
	/* DPCD_00060 */
	if (drm_dp_dpcd_read(aux, DP_DSC_SUPPORT, dsc_dpcd, DP_DSC_RECEIVER_CAP_SIZE) < 0)
		dev_err(mtk_dp->dev, "[DPTX] Failed to read DPCD register 0x%x\n", DP_DSC_SUPPORT);
}

void mtk_dp_dsc_support(struct mtk_dp *mtk_dp)
{
	if (!mtk_dp->mtk_con[DP_FIRST_CON])
		return;

	mtk_dsc_read_dsc_dpcd(mtk_dp, &mtk_dp->aux, mtk_dp->mtk_con[DP_FIRST_CON]->dsc_dpcd);

	dev_dbg(mtk_dp->dev, "[DPTX] sink dsc capable:%d\n",
		drm_dp_sink_supports_dsc(mtk_dp->mtk_con[DP_FIRST_CON]->dsc_dpcd));
}

static bool mtk_dp_hpd_get_pin_level(struct mtk_dp *mtk_dp)
{
	bool ret = ((READ_2BYTE(mtk_dp, MTK_DP_AUX_P0_364C) &
		    HPD_STATUS_AUX_TX_P0_MASK) >>
		    HPD_STATUS_AUX_TX_P0_FLDMASK_POS);

	return ret;
}

static bool mtk_dp_check_sink_cap(struct mtk_dp *mtk_dp)
{
	u8 tmp[0x10];
	int ret;

	if (!mtk_dp_hpd_get_pin_level(mtk_dp))
		return false;

	memset(tmp, 0x0, sizeof(tmp));

	tmp[0x0] = 0x1;
	/* DPCD_00600*/
	ret = drm_dp_dpcd_write(&mtk_dp->aux, DP_SET_POWER, tmp, 0x1);
	if (ret < 0)
		return false;
	mdelay(2);

	/* DPCD_00000*/
	ret = drm_dp_dpcd_read(&mtk_dp->aux, DP_DPCD_REV, tmp, 0x10);
	if (ret < 0)
		return false;

	mtk_dp->train_info.sink_ext_cap_en = (tmp[0x0E] & BIT(7)) ?
		true : false;
	if (mtk_dp->train_info.sink_ext_cap_en) {
		/* DPCD_02200*/
		ret = drm_dp_dpcd_read(&mtk_dp->aux, DP_DP13_DPCD_REV, tmp, 0x10);
		if (ret < 0)
			return false;
	}

	mtk_dp->train_info.dpcd_rev = tmp[0x0];
	dev_dbg(mtk_dp->dev,
		"[DPTX] SINK DPCD version:0x%x\n", mtk_dp->train_info.dpcd_rev);

	if (drm_dp_read_dpcd_caps(&mtk_dp->aux, mtk_dp->rx_cap) != 0)
		return false;

	if (mtk_dp->train_info.dpcd_rev >= 0x14) {
		mtk_dp_fec_ready(mtk_dp);
		mtk_dp_dsc_support(mtk_dp);
	}

	mtk_dp->train_info.tps3_support = (tmp[0x2] & BIT(6)) >> 0x6;
	mtk_dp->train_info.tps4_support = (tmp[0x3] & BIT(7)) >> 0x7;

	mtk_dp->train_info.dwn_strm_port_present =
			(tmp[0x5] & BIT(0));

	if ((tmp[0x3] & BIT(0)) == 0x1) {
		mtk_dp->train_info.sink_ssc_en = true;
		dev_info(mtk_dp->dev, "[DPTX] SINK SUPPORT SSC\n");
	} else {
		mtk_dp->train_info.sink_ssc_en = false;
		dev_info(mtk_dp->dev, "[DPTX] SINK NOT SUPPORT SSC\n");
	}

	ret = drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00021, tmp, 0x1);
	if (ret < 0)
		return false;

	mtk_dp->train_info.dp_mst_cap = (tmp[0x0] & BIT(0));
	mtk_dp->train_info.dp_mst_branch = false;

	if (mtk_dp->train_info.dp_mst_cap == BIT(0)) {
		if (mtk_dp->train_info.dwn_strm_port_present == 0x1)
			mtk_dp->train_info.dp_mst_branch = true;

		/* DPCD_02003 */
		ret = drm_dp_dpcd_read(&mtk_dp->aux, DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0, tmp, 0x1);
		if (ret < 0)
			return false;

		if (tmp[0x0] != 0x0) {
			/* DPCD_02003 */
			ret = drm_dp_dpcd_write(&mtk_dp->aux, DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0,
						tmp, 0x1);
			if (ret < 0)
				return false;
		}

		ret = drm_dp_dpcd_read(&mtk_dp->aux, DP_MSTM_CTRL, tmp, 0x1);
		if (ret < 0)
			return false;
		mtk_dp->train_info.dp_mst_en = (tmp[0x0] & BIT(0));
	}

	dev_dbg(mtk_dp->dev,
		"[DPTX] mst_cap:%d, mst_en:%d, mst_branch:%d, port_present:%d\n",
		mtk_dp->train_info.dp_mst_cap,
		mtk_dp->train_info.dp_mst_en,
		mtk_dp->train_info.dp_mst_branch,
		mtk_dp->train_info.dwn_strm_port_present);

	/* DPCD_00600 */
	ret = drm_dp_dpcd_read(&mtk_dp->aux, DP_SET_POWER, tmp, 0x1);
	if (ret < 0)
		return false;
	if (tmp[0x0] != 0x1) {
		tmp[0x0] = 0x1;

		/* DPCD_00600 */
		ret = drm_dp_dpcd_write(&mtk_dp->aux, DP_SET_POWER, tmp, 0x1);
		if (ret < 0)
			return false;
	}

	mtk_dp->train_info.sink_count = mtk_dp_get_sink_count(mtk_dp);

	if (!mtk_dp->train_info.dp_mst_branch) {
		u8 dpcd_201 = 0;

		/* DPCD_00201*/
		ret = drm_dp_dpcd_read(&mtk_dp->aux, DP_DEVICE_SERVICE_IRQ_VECTOR, &dpcd_201, 1);
		if (ret < 0)
			return false;
	}

	return true;
}

static int mtk_dp_parse_capabilities(struct mtk_dp *mtk_dp)
{
	u8 val;
	ssize_t ret;

	/*
	 * If we're eDP and capabilities were already parsed we can skip
	 * reading again because eDP panels aren't hotpluggable hence the
	 * caps and training information won't ever change in a boot life
	 */
	if (mtk_dp->bridge.type == DRM_MODE_CONNECTOR_eDP &&
	    mtk_dp->rx_cap[DP_MAX_LINK_RATE] &&
	    mtk_dp->train_info.sink_ssc_en)
		return 0;

	ret = drm_dp_read_dpcd_caps(&mtk_dp->aux, mtk_dp->rx_cap);
	if (ret < 0)
		return ret;

	mtk_dp->train_info.sink_ssc_en = drm_dp_max_downspread(mtk_dp->rx_cap);

	ret = drm_dp_dpcd_readb(&mtk_dp->aux, DP_MSTM_CAP, &val);
	if (ret < 1) {
		drm_err(mtk_dp->drm_dev, "Read mstm cap failed\n");
		return ret == 0 ? -EIO : ret;
	}

	if (val & DP_MST_CAP) {
		/* Clear DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0 */
		ret = drm_dp_dpcd_readb(&mtk_dp->aux,
					DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0,
					&val);
		if (ret < 1) {
			drm_err(mtk_dp->drm_dev, "Read irq vector failed\n");
			return ret == 0 ? -EIO : ret;
		}

		if (val) {
			ret = drm_dp_dpcd_writeb(&mtk_dp->aux,
						 DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0,
						 val);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static void mtk_dp_check_and_set_power_state(struct mtk_dp *mtk_dp)
{
	u8 temp[0x1] = {0};

	/* DPCD_00600 */
	drm_dp_dpcd_read(&mtk_dp->aux, DP_SET_POWER, temp, 0x1);
	if (temp[0] != 0x01) {
		temp[0] = 0x01;
		drm_dp_dpcd_write(&mtk_dp->aux, DP_SET_POWER, temp, 0x1);
		mdelay(1);
	}
}

static void mtk_dp_phy_set_lane_pwr(struct mtk_dp *mtk_dp,
						enum dp_lane_count lane_count)
{
	int power_indx = lane_count - 1;
	u8 power_bmp = BIT(power_indx);

	do {
		power_bmp |= BIT(power_indx);

		PHY_WRITE_BYTE_MASK(mtk_dp,
				    (mtk_dp->data->phyd_dig_glb_offset
				    + DP_PHY_DIG_TX_CTL_0),
				    power_bmp << TX_LN_EN_FLDMASK_POS,
				    TX_LN_EN_FLDMASK);

		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] set lane pwr %x\n",
			    (PHY_READ_BYTE(mtk_dp, mtk_dp->data->phyd_dig_glb_offset
			    + DP_PHY_DIG_TX_CTL_0) &
			    TX_LN_EN_FLDMASK) >> TX_LN_EN_FLDMASK_POS);
	} while (--power_indx >= 0);
}

static void mtk_dp_phy_clear_lane_pwr(struct mtk_dp *mtk_dp)
{
	u8 power_bmp = (PHY_READ_BYTE(mtk_dp, mtk_dp->data->phyd_dig_glb_offset
			+ DP_PHY_DIG_TX_CTL_0) &
			TX_LN_EN_FLDMASK) >> TX_LN_EN_FLDMASK_POS;

	do {
		power_bmp >>= 1;

		PHY_WRITE_BYTE_MASK(mtk_dp,
				    (mtk_dp->data->phyd_dig_glb_offset
				     + DP_PHY_DIG_TX_CTL_0),
				    power_bmp << TX_LN_EN_FLDMASK_POS,
				    TX_LN_EN_FLDMASK);

		drm_dbg_kms(mtk_dp->drm_dev,
			    "[DPTX] clear lane pwr %x\n",
			    (PHY_READ_BYTE(mtk_dp, mtk_dp->data->phyd_dig_glb_offset
			     + DP_PHY_DIG_TX_CTL_0) &
			     TX_LN_EN_FLDMASK) >> TX_LN_EN_FLDMASK_POS);
	} while (power_bmp > 0);
}

static void mtk_dp_phy_power_on(struct mtk_dp *mtk_dp)
{
	PHY_WRITE_BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_glb_offset + DP_PHY_DIG_PLL_CTL_0,
			    0x1 << FORCE_PWR_STATE_EN_FLDMASK_POS, FORCE_PWR_STATE_EN_FLDMASK);
	PHY_WRITE_BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_glb_offset + DP_PHY_DIG_PLL_CTL_0,
			    0x3 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);

	dev_dbg(mtk_dp->dev, "[DPTX] DP PHYD power on\n");
}

static void mtk_dp_phyd_power_down(struct mtk_dp *mtk_dp)
{
	mtk_dp_phy_clear_lane_pwr(mtk_dp);

	PHY_WRITE_BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_glb_offset + DP_PHY_DIG_PLL_CTL_0,
			    0x1 << FORCE_PWR_STATE_EN_FLDMASK_POS, FORCE_PWR_STATE_EN_FLDMASK);
	/* power off TPLL and Lane */
	PHY_WRITE_BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_glb_offset + DP_PHY_DIG_PLL_CTL_0,
			    0x1 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);

	PHY_WRITE_BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_glb_offset + DP_PHY_DIG_SW_RST,
			    0, BIT(1) | BIT(3));
	PHY_WRITE_BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_glb_offset + DP_PHY_DIG_SW_RST,
			    BIT(1) | BIT(3), BIT(1) | BIT(3));

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] DP PHYD power down\n");
}

static void mtk_dp_verify_clock(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u64 m, n, ls_clk, pix_clk, fs;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	n = 0x8000;
	ls_clk = mtk_dp->train_info.link_rate * 27;

	m = READ_4BYTE(mtk_dp, MTK_DP_ENC1_P0_33C8 + reg_offset);
	pix_clk = div_u64(m * ls_clk * 1000 * 1000, n);
	dev_info(mtk_dp->dev,
		 "[DPTX][%d]video M:0x%llx, DP calc pixel clock:%llu Hz, dp_intf clock:%llu Hz\n",
		 encoder_id, m, pix_clk, pix_clk / 4);

	m = READ_4BYTE(mtk_dp, MTK_DP_ENC1_P0_33D0);
	fs = div_u64(m * ls_clk * 1000 * 1000, n * 512);
	dev_info(mtk_dp->dev, "[DPTX] [%d] audio M:0x%llx, fs:%llu\n", encoder_id, m, fs);
}

static void mtk_dp_msa_enable_bypass(struct mtk_dp *mtk_dp,
				     const enum dp_encoder_id encoder_id, bool enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3030 + reg_offset, enable ? 0 : 0x3ff,
			 GENMASK(10, 0));
}

static void mtk_dp_enable_video_interlance(struct mtk_dp *mtk_dp,
					   const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3030 + 1 + reg_offset,
			BIT(6) | BIT(5), GENMASK(6, 5));
	dev_dbg(mtk_dp->dev, "[DPTX] DP imode force-ov\n");
}

static void mtk_dp_disable_video_interlance(struct mtk_dp *mtk_dp,
					    const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3030 + 1 + reg_offset,
			BIT(6), GENMASK(6, 5));
	dev_dbg(mtk_dp->dev, "[DPTX] DP pmode force-ov\n");
}

static void mtk_dp_msa_set(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);
	struct mtk_dp_timing_parameter *dp_timing = &mtk_dp->info[encoder_id].dp_output_timing;

	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3010 + reg_offset,
		    dp_timing->htt);
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3018 + reg_offset,
		    dp_timing->hsw + dp_timing->hbp);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3028 + reg_offset,
			 dp_timing->hsw << HSW_SW_DP_ENC0_P0_FLDMASK_POS,
			 HSW_SW_DP_ENC0_P0_MASK);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3028 + reg_offset,
			 dp_timing->hsp << HSP_SW_DP_ENC0_P0_FLDMASK_POS,
			 HSP_SW_DP_ENC0_P0_MASK);
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3020 + reg_offset,
		    dp_timing->hde);
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3014 + reg_offset,
		    dp_timing->vtt);
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_301C + reg_offset,
		    dp_timing->vsw + dp_timing->vbp);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_302C + reg_offset,
			 dp_timing->vsw << VSW_SW_DP_ENC0_P0_FLDMASK_POS,
			 VSW_SW_DP_ENC0_P0_MASK);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_302C + reg_offset,
			 dp_timing->vsp << VSP_SW_DP_ENC0_P0_FLDMASK_POS,
			 VSP_SW_DP_ENC0_P0_MASK);
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3024 + reg_offset,
		    dp_timing->vde);
	if (!mtk_dp->dsc_enable[encoder_id])
		WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3064 + reg_offset,
			    dp_timing->hde);
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3154 + reg_offset,
		    (dp_timing->htt));
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3158 + reg_offset,
		    (dp_timing->hfp));
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_315C + reg_offset,
		    (dp_timing->hsw));
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3160 + reg_offset,
		    dp_timing->hbp + dp_timing->hsw);
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3164 + reg_offset,
		    (dp_timing->hde));
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3168 + reg_offset,
		    dp_timing->vtt);
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_316C + reg_offset,
		    dp_timing->vfp);
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3170 + reg_offset,
		    dp_timing->vsw);
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3174 + reg_offset,
		    dp_timing->vbp + dp_timing->vsw);
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3178 + reg_offset,
		    dp_timing->vde);

	dev_info(mtk_dp->dev,
		    "[DPTX] [%d] set MSA, Htt:%d, Vtt:%d, Hact:%d, Vact:%d, fps:%d\n",
		    encoder_id,
		    dp_timing->htt, dp_timing->vtt,
		    dp_timing->hde, dp_timing->vde, dp_timing->frame_rate);
}

static void mtk_dp_set_output_timing(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	if (mtk_dp->info[encoder_id].dp_output_timing.video_ip_mode == DP_VIDEO_INTERLACE)
		mtk_dp_enable_video_interlance(mtk_dp, encoder_id);
	else
		mtk_dp_disable_video_interlance(mtk_dp, encoder_id);

	mtk_dp_msa_set(mtk_dp, encoder_id);
}

static bool mtk_dp_mn_overwrite(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				bool enable, u64 video_m, u64 video_n)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	if (enable) {
		/* Turn-on overwrite MN */
		WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3008 + reg_offset,
			    video_m & GENMASK(15, 0));
		WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_300C + reg_offset,
			   ((video_m >> 16) & GENMASK(7, 0)));
		WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3044 + reg_offset,
			    video_n & GENMASK(15, 0));
		WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_3048 + reg_offset,
			   (video_n >> 16) & GENMASK(7, 0));

		WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3050 + reg_offset,
			    video_n & GENMASK(15, 0));
		WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_3054 + reg_offset,
			   (video_n >> 16) & GENMASK(7, 0));
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3004 + 1 + reg_offset,
				BIT(0), BIT(0));
	} else {
		/* Turn-off overwrite MN */
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3004 + 1 + reg_offset, 0, BIT(0));
	}

	return true;
}

static void mtk_dp_mvid_renew(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);
	u32 mvid, htt;

	htt = mtk_dp->info[encoder_id].dp_output_timing.htt;
	if (htt % 4 != 0) {
		mvid = READ_4BYTE(mtk_dp, MTK_DP_ENC1_P0_33C8 + reg_offset);
		dev_dbg(mtk_dp->dev, "[DPTX] Encoder:%d, Odd Htt:%d, m_vid:%d, overwrite\n",
			encoder_id, htt, mvid);
		mtk_dp_mn_overwrite(mtk_dp, encoder_id, true, mvid, 0x8000);
	}
}

static void mtk_dp_tu_set_sram_rd_start(struct mtk_dp *mtk_dp,
				 const enum dp_encoder_id encoder_id, u16 val)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	/* [5:0]video sram start address */
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_303C + reg_offset, val, GENMASK(5, 0));
}

static void mtk_dp_mn_calculate(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u8 frame_rate = 60;
	u32 pix_clk = 148500000;
	u32 pll_rate = (0x00d8 << 2) * 10; /*Base = 1Khz*/
	u32 val = 0, m_vid = 0;
	u32 n_vid = 0x8000;

	if (mtk_dp->info[encoder_id].dp_output_timing.frame_rate > 0) {
		frame_rate = mtk_dp->info[encoder_id].dp_output_timing.frame_rate;
		dev_dbg(mtk_dp->dev, "[DPTX] Frame Rate = %d\n",
			mtk_dp->info[encoder_id].dp_output_timing.frame_rate);

		pix_clk = (u32)mtk_dp->info[encoder_id].dp_output_timing.htt *
			  (u32)mtk_dp->info[encoder_id].dp_output_timing.vtt *
			  (u32)frame_rate;
	} else if (mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate > 0) {
		frame_rate = 60;
		pix_clk = mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate * 1000;
		dev_dbg(mtk_dp->dev, "[DPTX] Pix Clk (kHz) = %llu\n",
			mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate);
	} else {
		frame_rate = 60;
		pix_clk = (u32)mtk_dp->info[encoder_id].dp_output_timing.htt *
			  (u32)mtk_dp->info[encoder_id].dp_output_timing.vtt *
			  (u32)frame_rate;

		dev_dbg(mtk_dp->dev, "[DPTX] both frame_rate & pix_clk = 0\n");
	}

	dev_dbg(mtk_dp->dev, "[DPTX] pix_clk = 0x%x\r\n", pix_clk);

	val = pix_clk / (100000);

	if (val > 0) {
		m_vid = (val * n_vid) / pll_rate;

		dev_dbg(mtk_dp->dev, "[DPTX] Cal PR = %d x(1/10) Mhz\n", val);
		dev_dbg(mtk_dp->dev, "[DPTX] m_vid 0x%x\n", m_vid);
		mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate = pix_clk / 1000;
		mtk_dp->info[encoder_id].video_m = m_vid;
		mtk_dp->info[encoder_id].video_n = n_vid;
	}

	if (mtk_dp->train_info.link_rate >= DP_LINK_RATE_UHBR10) {
		mtk_dp->info[encoder_id].video_m = pix_clk >> 24;
		mtk_dp->info[encoder_id].video_n = pix_clk & GENMASK(23, 0);
		mtk_dp_mn_overwrite(mtk_dp, encoder_id, true, m_vid, n_vid);
	} else if (mtk_dp->train_info.link_rate >= DP_LINK_RATE_RBR && val > 0) {
		m_vid = (val * n_vid) / (mtk_dp->train_info.link_rate * 270);
		mtk_dp->info[encoder_id].video_m = m_vid;
		mtk_dp->info[encoder_id].video_n = n_vid;
	} else {
		dev_dbg(mtk_dp->dev,
			"[DPTX] Set video MN fail, due to invalid link rate\n");
	}
}

static void mtk_dp_pg_vertical_color_bar(struct mtk_dp *mtk_dp,
					 const enum dp_encoder_id encoder_id, u8 location)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3038 + 1 + reg_offset, BIT(3), BIT(3));
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_31B0 + reg_offset,
			3 << PGEN_PATTERN_SEL_DP_ENC0_P0_FLDMASK_POS,
			PGEN_PATTERN_SEL_MASK);

	switch (location) {
	case DP_PG_LOCATION_ALL:
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				0, GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				0, GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_LEFT:
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				0, GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_RIGHT:
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				BIT(2), GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_LEFT_OF_LEFT:
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				BIT(5) | BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				0, GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_RIGHT_OF_LEFT:
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				BIT(5) | BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				BIT(1), GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_LEFT_OF_RIGHT:
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				BIT(5) | BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				BIT(2), GENMASK(2, 0));
		break;
	case DP_PG_LOCATION_RIGHT_OF_RIGHT:
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				BIT(5) | BIT(4), GENMASK(5, 4));
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3190 + reg_offset,
				BIT(2) | BIT(1), GENMASK(2, 0));
		break;
	default:
		break;
	}
}

static void mtk_dp_pg_type_sel(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
			       int pattern_type, u8 bgr, u32 color_depth, u8 location)
{
	u16 hde, vde;

	hde = mtk_dp->info[encoder_id].dp_output_timing.hde;
	vde = mtk_dp->info[encoder_id].dp_output_timing.vde;

	switch (pattern_type) {

	case DP_PG_VERTICAL_COLOR_BAR:
		mtk_dp_pg_vertical_color_bar(mtk_dp, encoder_id, location);
		break;
	default:
		break;
	}
}

void mtk_dp_dsc_pps_send(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	/* Keep this function for possible future DSC porting */
}

void mtk_dp_dsc_enable(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] DSC enable\n");

	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_31C4 + reg_offset,
			 0,
			 PPS_HW_BYPASS_MASK_DP_ENC0_P0_MASK);

	/* [0] : DSC Enable */
	WRITE_BYTE_MASK(mtk_dp,
			MTK_DP_ENC1_P0_336C + reg_offset, BIT(0), BIT(0));
	/* 300C [9] : VB-ID[6] DSC enable */
	WRITE_BYTE_MASK(mtk_dp,
			MTK_DP_ENC0_P0_300C + 1 + reg_offset, BIT(1), BIT(1));
	/* 303C[10 : 8] : DSC color depth */
	WRITE_BYTE_MASK(mtk_dp,
			MTK_DP_ENC0_P0_303C + 1 + reg_offset,
			0x7, GENMASK(2, 0));
	/* 303C[14 : 12] : DSC color format */
	WRITE_BYTE_MASK(mtk_dp,
			MTK_DP_ENC0_P0_303C + 1 + reg_offset,
			0x7 << 4, GENMASK(6, 4));
	/* 31FC[12] : HDE last num control */
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_31FC + reg_offset,
			 0x2 << DE_LAST_NUM_SW_DP_ENC0_P0_FLDMASK_POS,
			 DE_LAST_NUM_SW_DP_ENC0_P0_MASK);
}

static void mtk_dp_set_misc(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u8 format, depth;
	union mtk_dp_misc DP_MISC;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	format = mtk_dp->info[encoder_id].format;
	depth = mtk_dp->info[encoder_id].depth;

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] format:0x%x, depth:0x%x\n", format, depth);

	/* MISC0[7:5] color depth */
	DP_MISC.misc.color_depth = depth;

	/* MISC0[3]: 0->RGB, 1->YUV */
	/* MISC0[2:1]: 01b->4:2:2, 10b->4:4:4 */
	switch (format) {
	case DP_PIXELFORMAT_YUV444:
		DP_MISC.misc.color_format = 0x2;
		DP_MISC.misc.spec_def1 = 0x1;
		break;

	case DP_PIXELFORMAT_YUV422:
		DP_MISC.misc.color_format = 0x1;
		DP_MISC.misc.spec_def1 = 0x1;
		break;

	case DP_PIXELFORMAT_YUV420:
		/* not support */
		break;

	case DP_PIXELFORMAT_RAW:
		DP_MISC.misc.color_format = 0x1;
		DP_MISC.misc.spec_def2 = 0x1;
		break;
	case DP_PIXELFORMAT_Y_ONLY:
		DP_MISC.misc.color_format = 0x0;
		DP_MISC.misc.spec_def2 = 0x1;
		break;

	case DP_PIXELFORMAT_RGB:
	default:
		DP_MISC.misc.color_format = 0x0;
		DP_MISC.misc.spec_def2 = 0x0;
		break;
	}

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3034 + reg_offset, DP_MISC.misc_raw[0],
			GENMASK(7, 1));
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3034 + 1 + reg_offset,
			DP_MISC.misc_raw[1], GENMASK(7, 0));
}

static int set_dsc_decompression_flag(struct drm_dp_aux *aux, u8 flag, bool set)
{
	int err;
	u8 val;

	err = drm_dp_dpcd_readb(aux, DP_DSC_ENABLE, &val);
	if (err < 0)
		return err;

	if (set)
		val |= flag;
	else
		val &= ~flag;

	return drm_dp_dpcd_writeb(aux, DP_DSC_ENABLE, val);
}

static void mtk_dp_video_config(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	struct mtk_dp_timing_parameter *dp_timing = &mtk_dp->info[encoder_id].dp_output_timing;
	struct videomode vm = {0};
	int con_id;
	u8 data;

	if (!mtk_dp->dp_ready) {
		dev_err(mtk_dp->dev, "[DPTX] %s, DP is not ready\n", __func__);
		return;
	}

	data = mtk_dp->dsc_enable[encoder_id];

	mtk_dp_mn_overwrite(mtk_dp, encoder_id, false, 0x0, 0x8000);

	vm.hactive = mtk_dp->mode[encoder_id].hdisplay;
	vm.hfront_porch = mtk_dp->mode[encoder_id].hsync_start - mtk_dp->mode[encoder_id].hdisplay;
	vm.hsync_len = mtk_dp->mode[encoder_id].hsync_end - mtk_dp->mode[encoder_id].hsync_start;
	vm.hback_porch = mtk_dp->mode[encoder_id].htotal - mtk_dp->mode[encoder_id].hsync_end;
	vm.vactive = mtk_dp->mode[encoder_id].vdisplay;
	vm.vfront_porch = mtk_dp->mode[encoder_id].vsync_start - mtk_dp->mode[encoder_id].vdisplay;
	vm.vsync_len = mtk_dp->mode[encoder_id].vsync_end - mtk_dp->mode[encoder_id].vsync_start;
	vm.vback_porch = mtk_dp->mode[encoder_id].vtotal - mtk_dp->mode[encoder_id].vsync_end;
	vm.pixelclock = mtk_dp->mode[encoder_id].clock * 1000;

	dp_timing->frame_rate = mtk_dp->mode[encoder_id].clock * 1000 /
				mtk_dp->mode[encoder_id].htotal / mtk_dp->mode[encoder_id].vtotal;
	dp_timing->htt = mtk_dp->mode[encoder_id].htotal;
	dp_timing->hbp = vm.hback_porch;
	dp_timing->hsw = vm.hsync_len;
	dp_timing->hsp = mtk_dp->mode[encoder_id].flags & DRM_MODE_FLAG_PHSYNC ? 0 : 1;
	dp_timing->hfp = vm.hfront_porch;
	dp_timing->hde = vm.hactive;
	dp_timing->vtt = mtk_dp->mode[encoder_id].vtotal;
	dp_timing->vbp = vm.vback_porch;
	dp_timing->vsw = vm.vsync_len;
	dp_timing->vsp = mtk_dp->mode[encoder_id].flags & DRM_MODE_FLAG_PVSYNC ? 0 : 1;
	dp_timing->vfp = vm.vfront_porch;
	dp_timing->vde = vm.vactive;

	dev_dbg(mtk_dp->dev, "[DPTX] frame_rate:%d\n", dp_timing->frame_rate);
	dev_dbg(mtk_dp->dev, "[DPTX] htt:%d\n", dp_timing->htt);
	dev_dbg(mtk_dp->dev, "[DPTX] hbp:%d\n", dp_timing->hbp);
	dev_dbg(mtk_dp->dev, "[DPTX] hsw:%d\n", dp_timing->hsw);
	dev_dbg(mtk_dp->dev, "[DPTX] hsp:%d\n", dp_timing->hsp);
	dev_dbg(mtk_dp->dev, "[DPTX] hfp:%d\n", dp_timing->hfp);
	dev_dbg(mtk_dp->dev, "[DPTX] hde:%d\n", dp_timing->hde);
	dev_dbg(mtk_dp->dev, "[DPTX] vtt:%d\n", dp_timing->vtt);
	dev_dbg(mtk_dp->dev, "[DPTX] vbp:%d\n", dp_timing->vbp);
	dev_dbg(mtk_dp->dev, "[DPTX] vsw:%d\n", dp_timing->vsw);
	dev_dbg(mtk_dp->dev, "[DPTX] vsp:%d\n", dp_timing->vsp);
	dev_dbg(mtk_dp->dev, "[DPTX] vfp:%d\n", dp_timing->vfp);
	dev_dbg(mtk_dp->dev, "[DPTX] vde:%d\n", dp_timing->vde);

	mtk_dp_mn_calculate(mtk_dp, encoder_id);

	if (mtk_dp->dsc_enable[encoder_id])
		mtk_dp_mn_overwrite(mtk_dp, encoder_id, true,
				    mtk_dp->info[encoder_id].video_m,
				    mtk_dp->info[encoder_id].video_n);

	/* interlace not support */
	dp_timing->video_ip_mode = DP_VIDEO_PROGRESSIVE;
	mtk_dp_msa_set(mtk_dp, encoder_id);

	mtk_dp_set_misc(mtk_dp, encoder_id);
	if (mtk_dp->info[encoder_id].pattern_gen)
		mtk_dp_pg_type_sel(mtk_dp, encoder_id,
				   DP_PG_VERTICAL_COLOR_BAR,
				   DP_PG_PURECOLOR_BLUE,
				   0xfff,
				   DP_PG_LOCATION_ALL);

	if (!mtk_dp->dsc_enable[encoder_id]) {
		mtk_dp_color_set_depth(mtk_dp, encoder_id, mtk_dp->info[encoder_id].depth);
		mtk_dp_color_set_format(mtk_dp, encoder_id, mtk_dp->info[encoder_id].format);
	} else {
		mtk_dp_dsc_pps_send(mtk_dp, encoder_id);
		mtk_dp_dsc_enable(mtk_dp, encoder_id);

		if (mtk_dp->mst_enable) {
			con_id = encoder_id_to_con_id(mtk_dp, encoder_id);
			if (con_id < 0)
				return;

			set_dsc_decompression_flag(mtk_dp->mtk_con[con_id]->dsc_aux,
						   DP_DECOMPRESSION_EN, true);
		} else {
			set_dsc_decompression_flag(&mtk_dp->aux, DP_DECOMPRESSION_EN, true);
		}
	}
}

static void mtk_dp_tu_set_encoder(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_303C + 1 + reg_offset, BIT(7), BIT(7));
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3040 + reg_offset, 0x2020);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3364 + reg_offset, 0x2020, GENMASK(11, 0));
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3300 + 1 + reg_offset, 0x02, GENMASK(1, 0));
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3364 + 1 + reg_offset, 0x40, GENMASK(6, 4));
}

static void mtk_dp_audio_sample_arrange(struct mtk_dp *mtk_dp,
					const enum dp_encoder_id encoder_id, u8 enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);
	u32 value = 0;
	u64 tmp = 0;

	tmp = mtk_dp->info[encoder_id].dp_output_timing.htt -
				mtk_dp->info[encoder_id].dp_output_timing.hde;

	value = div_u64(tmp * mtk_dp->train_info.link_rate * 27 * 200,
			mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate);

	if (enable) {
		WRITE_4BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3374 + reg_offset,
				 BIT(12), BIT(12));
		WRITE_4BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3374 + reg_offset,
				 (u16)value, GENMASK(11, 0));
	} else {
		WRITE_4BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3374 + reg_offset, 0, BIT(12));
		WRITE_4BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3374 + reg_offset,
				 0, GENMASK(11, 0));
	}

	dev_dbg(mtk_dp->dev,
		"[DPTX] Encoder %d, Audio arrange patch enable = %d, value = 0x%x\n",
		encoder_id, enable, value);
}

static u32 mtk_dp_calculate_sdp_down_cnt(struct mtk_dp *mtk_dp, u32 sdp_down_cnt)
{
	u32 down_cnt = 0;

	switch (mtk_dp->train_info.link_lane_count) {
	case DP_1LANE:
		down_cnt = max(sdp_down_cnt, 0x1e);
		break;

	case DP_2LANE:
		down_cnt = max(sdp_down_cnt, 0x14);
		break;

	case DP_4LANE:
		down_cnt = max(sdp_down_cnt, 0x08);
		break;

	default:
		down_cnt = max(sdp_down_cnt, 0x08);
		break;
	}

	return down_cnt;
}

static void mtk_dp_sdp_set_down_cnt_init_in_hblanking(struct mtk_dp *mtk_dp,
						      const enum dp_encoder_id encoder_id)
{
	u32 sdp_down_cnt;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	/* hblank * link_rate / pixel_clock * 0.8(margin) / 4(1T4B) */
	sdp_down_cnt = div_u64((u64)(mtk_dp->info[encoder_id].dp_output_timing.htt -
				     mtk_dp->info[encoder_id].dp_output_timing.hde) *
				     mtk_dp->train_info.link_rate * 2700 * 2,
				     mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate);

	dev_dbg(mtk_dp->dev,
		"[DPTX] [%d] htt:%d, hde:%d, link_rate:%d, pixcel_rate:%llu, color_format:0x%x\n",
		encoder_id,
		mtk_dp->info[encoder_id].dp_output_timing.htt,
		mtk_dp->info[encoder_id].dp_output_timing.hde,
		mtk_dp->train_info.link_rate,
		mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate,
		mtk_dp->info[encoder_id].format);

	if (mtk_dp->info[encoder_id].format == DP_PIXELFORMAT_YUV420)
		sdp_down_cnt = sdp_down_cnt / 2;

	sdp_down_cnt = mtk_dp_calculate_sdp_down_cnt(mtk_dp, sdp_down_cnt);

	dev_dbg(mtk_dp->dev, "[DPTX] sdp_down_cnt_blank:%x\n", sdp_down_cnt);

	/* [11 : 0]REG_sdp_down_cnt_init_in_hblank */
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3364 + reg_offset, sdp_down_cnt, GENMASK(11, 0));
}

static void mtk_dp_sdp_set_down_cnt_init(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
					 u16 sram_read_start)
{
	u32 sdp_down_cnt = 0;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	/* sram_read_start * lane_cnt * 2(pixelperaddr) * link_rate / pixel_clock * 0.8(margin) */
	sdp_down_cnt = div_u64((u64)sram_read_start * mtk_dp->train_info.link_lane_count * 2 *
			       mtk_dp->train_info.link_rate * 2700 * 8,
			       mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate);

	if (mtk_dp->info[encoder_id].format == DP_PIXELFORMAT_YUV420)
		sdp_down_cnt = sdp_down_cnt / 2;

	sdp_down_cnt = mtk_dp_calculate_sdp_down_cnt(mtk_dp, sdp_down_cnt);

	dev_dbg(mtk_dp->dev, "[DPTX] pixcel_rate:%llu sdp_down_cnt:%x\n",
		mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate, sdp_down_cnt);

	/* [11 : 0]REG_sdp_down_cnt_init */
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3040 + reg_offset, sdp_down_cnt, GENMASK(11, 0));
}

static void mtk_dp_sdp_set_asp_count_init(struct mtk_dp *mtk_dp,
					  const enum dp_encoder_id encoder_id)
{
	u16 down_asp = 0x0000;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	mtk_dp->info[encoder_id].dp_output_timing.hbk =
		mtk_dp->info[encoder_id].dp_output_timing.htt -
		mtk_dp->info[encoder_id].dp_output_timing.hde;

	if (mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate > 0) {
		if (mtk_dp->train_info.link_rate <= DP_LINK_RATE_HBR3)
			down_asp =
				div_u64((u64)mtk_dp->info[encoder_id].dp_output_timing.hbk *
					mtk_dp->train_info.link_rate * 27 * 250 * 4,
					mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate * 5);
		else
			down_asp =
				div_u64((u64)mtk_dp->info[encoder_id].dp_output_timing.hbk *
					mtk_dp->train_info.link_rate * 10 * 1000 * 250 * 4,
					mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate) *
					32 * 5;
	}

	/* [11 : 0] reg_sdp_down_asp_cnt_init */
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3374 + reg_offset,
			 down_asp << SDP_DOWN_ASP_CNT_INIT_DP_ENC1_P0_FLDMASK_POS,
			 SDP_DOWN_ASP_CNT_INIT_DP_ENC1_P0_MASK);
}

u8 mtk_dp_color_get_bpp(u8 color_format, u8 color_depth)
{
	u8 color_bpp;

	switch (color_depth) {
	case DP_COLOR_DEPTH_6BIT:
		if (color_format == DP_PIXELFORMAT_YUV422)
			color_bpp = 16;
		else if (color_format == DP_PIXELFORMAT_YUV420)
			color_bpp = 12;
		else
			color_bpp = 18;
		break;
	case DP_COLOR_DEPTH_8BIT:
		if (color_format == DP_PIXELFORMAT_YUV422)
			color_bpp = 16;
		else if (color_format == DP_PIXELFORMAT_YUV420)
			color_bpp = 12;
		else
			color_bpp = 24;
		break;
	case DP_COLOR_DEPTH_10BIT:
		if (color_format == DP_PIXELFORMAT_YUV422)
			color_bpp = 20;
		else if (color_format == DP_PIXELFORMAT_YUV420)
			color_bpp = 15;
		else
			color_bpp = 30;
		break;
	case DP_COLOR_DEPTH_12BIT:
		if (color_format == DP_PIXELFORMAT_YUV422)
			color_bpp = 24;
		else if (color_format == DP_PIXELFORMAT_YUV420)
			color_bpp = 18;
		else
			color_bpp = 36;
		break;
	case DP_COLOR_DEPTH_16BIT:
		if (color_format == DP_PIXELFORMAT_YUV422)
			color_bpp = 32;
		else if (color_format == DP_PIXELFORMAT_YUV420)
			color_bpp = 24;
		else
			color_bpp = 48;
		break;
	default:
		color_bpp = 24;
		break;
	}

	return color_bpp;
}

static void mtk_dp_tu_set(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	int tu_size = 0;
	int n_value = 0;
	int f_value = 0;
	int pixcel_rate = 0;
	u8 color_bpp;
	u16 sram_read_start = 0;

	color_bpp = mtk_dp_color_get_bpp(mtk_dp->info[encoder_id].format,
					 mtk_dp->info[encoder_id].depth);
	pixcel_rate = div_u64(mtk_dp->info[encoder_id].dp_output_timing.pixcel_rate, 1000);
	tu_size = (640 * (pixcel_rate) * color_bpp) /
		   (mtk_dp->train_info.link_rate * 27 *
		   mtk_dp->train_info.link_lane_count * 8);

	n_value = tu_size / 10;
	f_value = tu_size % 10;

	dev_dbg(mtk_dp->dev,
		"[DPTX] tu_size:%d n_value:%d f_value:%d\n", tu_size, n_value, f_value);
	if (mtk_dp->train_info.link_lane_count > 0) {
		sram_read_start = mtk_dp->info[encoder_id].dp_output_timing.hde /
			(mtk_dp->train_info.link_lane_count * 4 * 2 * 2);
		sram_read_start =
			(sram_read_start < DP_TBC_BUF_READ_START_ADR_THRD) ?
			sram_read_start : DP_TBC_BUF_READ_START_ADR_THRD;
		mtk_dp_tu_set_sram_rd_start(mtk_dp, encoder_id, sram_read_start);
	}

	mtk_dp_tu_set_encoder(mtk_dp, encoder_id);
	mtk_dp_audio_sample_arrange(mtk_dp, encoder_id, true);
	mtk_dp_sdp_set_down_cnt_init_in_hblanking(mtk_dp, encoder_id);
	mtk_dp_sdp_set_down_cnt_init(mtk_dp, encoder_id, sram_read_start);
	mtk_dp_sdp_set_asp_count_init(mtk_dp, encoder_id);
}

static void mtk_dp_set_dp_out(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	mtk_dp_video_config(mtk_dp, encoder_id);
	mtk_dp_msa_enable_bypass(mtk_dp, encoder_id, false);
	mtk_dp_set_output_timing(mtk_dp, encoder_id);
	mtk_dp_mn_calculate(mtk_dp, encoder_id);

	mtk_dp_pg_enable(mtk_dp, encoder_id, false);

	mtk_dp_mvid_renew(mtk_dp, encoder_id);
	mtk_dp_tu_set(mtk_dp, encoder_id);

	dev_dbg(mtk_dp->dev, "[DPTX] %s config done\n", __func__);
}

static void mtk_dp_video_enable(struct mtk_dp *mtk_dp, enum dp_encoder_id id)
{
	dev_dbg(mtk_dp->dev, "[DPTX] Output Video[%d] enable\n", id);

	mtk_dp_set_dp_out(mtk_dp, id);
	mtk_dp_verify_clock(mtk_dp, id);
}

static void mtk_dp_audio_sdp_setting(struct mtk_dp *mtk_dp,
				     const enum dp_encoder_id encoder_id, u8 channel)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_312C + reg_offset, 0x00, GENMASK(7, 0));

	if (channel == 8)
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_312C + reg_offset, 0x0700, GENMASK(15, 8));
	else
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_312C + reg_offset, 0x0100, GENMASK(15, 8));
}

static void mtk_dp_spkg_sdp(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
			    bool enable,
			    u8 sdp_type,
			    u8 *hb,
			    u8 *db)
{
	u8  offset;
	u16 st_offset;
	u8  hb_offset;
	u8  reg_index;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	if (enable) {
		for (offset = 0; offset < 0x10; offset++)
			for (reg_index = 0; reg_index < 2; reg_index++) {
				u32 addr = MTK_DP_ENC1_P0_3200
					   + offset * 4 + reg_index + reg_offset;

				WRITE_BYTE(mtk_dp, addr, (db[offset * 2 + reg_index]));
				drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SDP address:%u, data:%d\n",
					    addr, db[offset * 2 + reg_index]);
			}

		if (sdp_type == DP_SDP_PKG_DRM) {
			for (hb_offset = 0; hb_offset < 4 / 2; hb_offset++)
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = MTK_DP_ENC0_P0_3138
						+ hb_offset * 4 + reg_index + reg_offset;
					u8 offset = hb_offset * 2 + reg_index;

					WRITE_BYTE(mtk_dp, addr, (hb[offset]));
					drm_dbg_kms(mtk_dp->drm_dev,
						    "[DPTX] W Reg addr:%x, index:%d\n",
						    addr, offset);
				}
		} else if (sdp_type >= DP_SDP_PKG_PPS0 &&
			   sdp_type <= DP_SDP_PKG_PPS3) {
			for (hb_offset = 0; hb_offset < (4 / 2); hb_offset++)
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = MTK_DP_ENC0_P0_3130
					+ hb_offset * 4 + reg_index + reg_offset;
					u8 offset = hb_offset * 2 + reg_index;

					WRITE_BYTE(mtk_dp, addr, hb[offset]);
					drm_dbg_kms(mtk_dp->drm_dev,
						    "[DPTX] W H1 Reg addr:%x, index:%d\n",
						    addr, offset);
				}
		} else if (sdp_type == DP_SDP_PKG_ADS) {
			for (hb_offset = 0; hb_offset < (4 >> 1); hb_offset++)
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = MTK_DP_ENC0_P0_31F0 + reg_offset
						+ hb_offset * 8 + reg_index;
					u8 offset = hb_offset * 2 + reg_index;

					WRITE_BYTE(mtk_dp, addr, hb[offset]);
				}
		} else {
			st_offset = (sdp_type - DP_SDP_PKG_ACM) * 8;

			for (hb_offset = 0; hb_offset < 4 / 2; hb_offset++)
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = MTK_DP_ENC0_P0_30D8
					+ st_offset
					+ hb_offset * 4 + reg_index + reg_offset;
					u8 offset = hb_offset * 2 + reg_index;

					WRITE_BYTE(mtk_dp, addr, hb[offset]);
					drm_dbg_kms(mtk_dp->drm_dev,
						    "[DPTX] W H2 Reg addr:%x, index:%d\n",
						    addr, offset);
				}
		}
	}

	switch (sdp_type) {
	case DP_SDP_PKG_NONE:
		break;

	case DP_SDP_PKG_ACM:
		WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_30B4 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_ACM,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_30B4 + reg_offset, 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE ACM\n");
		}

		break;

	case DP_SDP_PKG_ISRC:
		WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_30B4 + 1 + reg_offset), 0x00);

		if (enable) {
			WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_31EC + 1 + reg_offset), 0x1C);
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_ISRC,
					GENMASK(4, 0));

			if (hb[3] & BIT(2))
				WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_30BC + reg_offset,
						BIT(0), BIT(0));
			else
				WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_30BC + reg_offset,
						0, BIT(0));

			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_30B4 + 1 + reg_offset), 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE ISRC\n");
		}

		break;

	case DP_SDP_PKG_AVI:
		WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_30A4 + 1 + reg_offset), 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_AVI,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_30A4 + 1 + reg_offset), 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE AVI\n");
		}

		break;

	case DP_SDP_PKG_AUI:
		WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_30A8 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_AUI,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_30A8 + reg_offset, 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE AUI\n");
		}

		break;

	case DP_SDP_PKG_SPD:
		WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_30A8 + 1 + reg_offset), 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_SPD,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_30A8 + 1 + reg_offset, 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE SPD\n");
		}

		break;

	case DP_SDP_PKG_MPEG:
		WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_30AC + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_MPEG,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_30AC + reg_offset, 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE MPEG\n");
		}

		break;

	case DP_SDP_PKG_NTSC:
		WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_30AC + 1 + reg_offset), 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_NTSC,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_30AC + 1 + reg_offset), 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE NTSC\n");
		}

		break;

	case DP_SDP_PKG_VSP:
		WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_30B0 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_VSP,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_30B0 + reg_offset, 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE VSP\n");
		}

		break;

	case DP_SDP_PKG_VSC:
		WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_30B8 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_VSC,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_30B8 + reg_offset, 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE VSC\n");
		}

		break;

	case DP_SDP_PKG_EXT:
		WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_30B0 + 1 + reg_offset), 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_EXT,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_30B0 + 1 + reg_offset, 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE EXT\n");
		}

		break;

	case DP_SDP_PKG_PPS0:
		WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31E8 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_PPS0,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31E8 + reg_offset, 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE PPS0\n");
		}

		break;

	case DP_SDP_PKG_PPS1:
		WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31E8 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_PPS1,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31E8 + reg_offset, 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE PPS1\n");
		}

		break;

	case DP_SDP_PKG_PPS2:
		WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31E8 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_PPS2,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31E8 + reg_offset, 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE PPS2\n");
		}

		break;

	case DP_SDP_PKG_PPS3:
		WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31E8 + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_PPS3,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31E8 + reg_offset, 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE PPS3\n");
		}

		break;

	case DP_SDP_PKG_DRM:
		WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31DC + reg_offset, 0x00);

		if (enable) {
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_3138 + reg_offset, hb[0]);
			WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_3138 + 1 + reg_offset), hb[1]);
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_313C + reg_offset, hb[2]);
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_313C + 1 + reg_offset, hb[3]);
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_DRM,
					GENMASK(4, 0));
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					BIT(5), BIT(5));
			WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31DC + reg_offset, 0x05);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE DRM\n");
		}

		break;

	case DP_SDP_PKG_ADS:
		/* adaptive sync SDP transmit disable */
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_31EC + reg_offset, 0,
				ADS_CFG_DP_ENC0_P0_MASK);
		if (enable) {
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					DP_SDP_PKG_ADS,
					SDP_PACKET_W_DP_ENC1_P0_MASK);
			/* write sdp data trigger */
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3280 + reg_offset,
					1 << SDP_PACKET_W_DP_ENC1_P0_FLDMASK_POS,
					SDP_PACKET_W_DP_ENC1_P0_MASK);
			/* adaptive sync SDP transmit enable */
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_31EC + reg_offset,
					1 << ADS_CFG_DP_ENC0_P0_FLDMASK_POS,
					ADS_CFG_DP_ENC0_P0_MASK);
			drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] SENT SDP TYPE ADS\n");
		}

		break;

	default:
		break;
	}
}

static void mtk_dp_spkg_vsc_ext_vesa(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				     bool enable,
				     u8 hdr_num,
				     u8 *db)
{
	u8  vsc_hb1 = 0x20;	/* VESA : 0x20; CEA : 0x21 */
	u8  vsc_hb2;
	u8  pkg_cnt;
	u8  loop;
	u8  offset;
	u8  reg_index;
	u16 sdp_offset;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	if (!enable) {
		WRITE_BYTE_MASK(mtk_dp, (MTK_DP_ENC0_P0_30A0 + 1 + reg_offset), 0, BIT(0));
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_328C + reg_offset, 0, BIT(7));
		return;
	}

	vsc_hb2 = (hdr_num > 0) ? BIT(6) : 0x00;

	WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31C8 + reg_offset, 0x00);
	WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_31C8 + 1 + reg_offset), vsc_hb1);
	WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31CC + reg_offset, vsc_hb2);
	WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_31CC + 1 + reg_offset), 0x00);
	WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31D8 + reg_offset, hdr_num);

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_328C + reg_offset, BIT(0), BIT(0));
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_328C + reg_offset, BIT(2), BIT(2));

	usleep_range(50, 51);
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_328C + reg_offset, 0, BIT(2));
	usleep_range(50, 51);

	for (pkg_cnt = 0; pkg_cnt < (hdr_num + 1); pkg_cnt++) {
		sdp_offset = 0;

		for (loop = 0; loop < 4; loop++) {
			for (offset = 0; offset < 8 / 2; offset++) {
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = MTK_DP_ENC1_P0_3290
					+ offset * 4 + reg_index + reg_offset;
					u8 tmp = sdp_offset
							+ offset * 2 + reg_index;

					WRITE_BYTE(mtk_dp, addr, db[tmp]);
				}
			}

			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_328C + reg_offset,
					BIT(6), BIT(6));
			sdp_offset += 8;
		}
	}

	WRITE_BYTE_MASK(mtk_dp, (MTK_DP_ENC0_P0_30A0 + 1 + reg_offset), BIT(0), BIT(0));
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_328C + reg_offset, BIT(7), BIT(7));
}

static void mtk_dp_spkg_vsc_ext_cea(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				    bool enable,
				    u8 hdr_num,
				    u8 *db)
{
	u8  vsc_hb1 = 0x21;
	u8  vsc_hb2;
	u8  pkg_cnt;
	u8  loop;
	u8  offset;
	u8  reg_index;
	u16 sdp_offset;
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	if (!enable) {
		WRITE_BYTE_MASK(mtk_dp, (MTK_DP_ENC0_P0_30A0 + 1  + reg_offset), 0, BIT(4));
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_32A0  + reg_offset, 0, BIT(7));
		return;
	}

	vsc_hb2 = (hdr_num > 0) ? 0x40 : 0x00;

	WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31D0  + reg_offset, 0x00);
	WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_31D0 + 1  + reg_offset), vsc_hb1);
	WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_31D4  + reg_offset, vsc_hb2);
	WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_31D4 + 1  + reg_offset), 0x00);
	WRITE_BYTE(mtk_dp, (MTK_DP_ENC0_P0_31D8 + 1  + reg_offset), hdr_num);

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_32A0  + reg_offset, BIT(0), BIT(0));
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_32A0  + reg_offset, BIT(2), BIT(2));
	usleep_range(50, 51);

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_32A0  + reg_offset, 0, BIT(2));

	for (pkg_cnt = 0; pkg_cnt < (hdr_num + 1); pkg_cnt++) {
		sdp_offset = 0;

		for (loop = 0; loop < 4; loop++) {
			for (offset = 0; offset < 4; offset++) {
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = MTK_DP_ENC1_P0_32A4
					+ offset * 4 + reg_index  + reg_offset;
					u8 tmp = sdp_offset
						 + offset * 2 + reg_index;

					WRITE_BYTE(mtk_dp, addr, db[tmp]);
				}
			}

			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_32A0  + reg_offset,
					BIT(6), BIT(6));
			sdp_offset += 8;
		}
	}

	WRITE_BYTE_MASK(mtk_dp, (MTK_DP_ENC0_P0_30A0 + 1  + reg_offset), BIT(4), BIT(4));
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_32A0  + reg_offset, BIT(7), BIT(7));
}

static void mtk_dp_stop_sent_sdp(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u8 pkg_type;

	for (pkg_type = DP_SDP_PKG_ACM; pkg_type < DP_SDP_PKG_MAX_NUM; pkg_type++)
		mtk_dp_spkg_sdp(mtk_dp, encoder_id, false, pkg_type, NULL, NULL);

	mtk_dp_spkg_vsc_ext_vesa(mtk_dp, encoder_id, false, 0x00, NULL);
	mtk_dp_spkg_vsc_ext_cea(mtk_dp, encoder_id, false, 0x00, NULL);
}

static void mtk_dp_audio_sdp_config(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				    int ch, int fs, int len)
{
	u8 SDP_DB[32] = {0};
	u8 SDP_HB[4] = {0, DP_SDP_HB1_PKG_AINFO, 0x1b, 0x48};

	SDP_DB[0x0] = 0x10 | (ch - 1);

	switch (fs) {
	case 32000:
		SDP_DB[0x1] = 0x1 << 2;
		break;

	case 44100:
		SDP_DB[0x1] = 0x2 << 2;
		break;

	case 48000:
	default:
		SDP_DB[0x1] = 0x3 << 2;
		break;

	case 88200:
		SDP_DB[0x1] = 0x4 << 2;
		break;

	case 96000:
		SDP_DB[0x1] = 0x5 << 2;
		break;

	case 192000:
		SDP_DB[0x1] = 0x6 << 2;
		break;
	}

	switch (len) {
	case DP_BITWIDTH_16:
		SDP_DB[0x1] |= 0x1;
		break;

	case DP_BITWIDTH_20:
		SDP_DB[0x1] |= 0x2;
		break;

	case DP_BITWIDTH_24:
	default:
		SDP_DB[0x1] |= 0x3;
		break;
	}

	SDP_DB[0x2] = 0x0;

	if (ch == 8)
		SDP_DB[0x3] = 0x13;
	else
		SDP_DB[0x3] = 0x00;

	mtk_dp_audio_sdp_setting(mtk_dp, encoder_id, ch);
	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] [%d] I2S Set Audio Channel = %d\n", encoder_id, ch);
	mtk_dp_spkg_sdp(mtk_dp, encoder_id, true, DP_SDP_PKG_AUI, SDP_HB, SDP_DB);
}

static void mtk_dp_audio_ch_status_set(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				       int channel, int fs, int wordlength)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);
	union dp_rx_audio_chsts dp_audio;

	memset(&dp_audio, 0, sizeof(dp_audio));

	switch (channel) {
	case 2:
	default:
		dp_audio.audio_chsts.channel_number = 2;
		break;

	case 8:
		dp_audio.audio_chsts.channel_number = 8;
		break;
	}

	switch (fs) {
	case 32000:
		dp_audio.audio_chsts.sampling_freq = 3;
		dp_audio.audio_chsts.original_sampling_freq = 0xc;
		break;

	case 44100:
		dp_audio.audio_chsts.sampling_freq = 0;
		dp_audio.audio_chsts.original_sampling_freq = 0xf;
		break;

	case 48000:
	default:
		dp_audio.audio_chsts.sampling_freq = 2;
		dp_audio.audio_chsts.original_sampling_freq = 0xd;
		break;

	case 88200:
		dp_audio.audio_chsts.sampling_freq = 8;
		dp_audio.audio_chsts.original_sampling_freq = 7;
		break;

	case 96000:
		dp_audio.audio_chsts.sampling_freq = 0xa;
		dp_audio.audio_chsts.original_sampling_freq = 5;
		break;

	case 192000:
		dp_audio.audio_chsts.sampling_freq = 0xe;
		dp_audio.audio_chsts.original_sampling_freq = 1;
		break;
	}

	switch (wordlength) {
	case DP_BITWIDTH_16:
		dp_audio.audio_chsts.word_len = 0b0010;
		break;

	case DP_BITWIDTH_20:
		dp_audio.audio_chsts.word_len = 0b0011;
		break;

	case DP_BITWIDTH_24:
	default:
		dp_audio.audio_chsts.word_len = 0b1011;
		break;
	}

	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_308C + reg_offset,
		    ((dp_audio.audio_chsts_raw[1] << 8) | dp_audio.audio_chsts_raw[0]));
	WRITE_2BYTE(mtk_dp, MTK_DP_ENC0_P0_3090 + reg_offset,
		    ((dp_audio.audio_chsts_raw[3] << 8) | dp_audio.audio_chsts_raw[2]));
	WRITE_BYTE(mtk_dp, MTK_DP_ENC0_P0_3094 + reg_offset, dp_audio.audio_chsts_raw[4]);

	mdelay(1);
}

void mtk_dp_audio_pg_enable(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				int channel, int fs, u8 enable)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3320 + reg_offset,
			 0x3F << AUDIO_PATTERN_GEN_DSTB_CNT_THRD_DP_ENC1_P0_FLDMASK_POS,
			 AUDIO_PATTERN_GEN_DSTB_CNT_THRD_DP_ENC1_P0_MASK);

	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_307C + reg_offset, 0,
			 HBLANK_SPACE_FOR_SDP_HW_EN_DP_ENC0_P0_MASK);

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_33F4 + reg_offset, BIT(4), BIT(4));

	if (enable) {
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3324 + reg_offset,
				 0x3 << AUDIO_SOURCE_MUX_DP_ENC1_P0_FLDMASK_POS,
				 AUDIO_SOURCE_MUX_DP_ENC1_P0_MASK);
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_331C + reg_offset,
				 0x0, TDM_AUDIO_DATA_EN_DP_ENC1_P0_MASK);
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_33F4 + reg_offset, 0, BIT(0));
	} else {
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3324 + reg_offset,
				 0x4 << AUDIO_SOURCE_MUX_DP_ENC1_P0_FLDMASK_POS,
				 AUDIO_SOURCE_MUX_DP_ENC1_P0_MASK);

		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_331C + reg_offset,
				 TDM_AUDIO_DATA_EN_DP_ENC1_P0_MASK,
				 TDM_AUDIO_DATA_EN_DP_ENC1_P0_MASK);

		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_331C + reg_offset,
				 (0x1f << TDM_AUDIO_DATA_BIT_DP_ENC1_P0_FLDMASK_POS),
				 TDM_AUDIO_DATA_BIT_DP_ENC1_P0_MASK);
		WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_33F4 + reg_offset, BIT(0), BIT(0));
	}

	drm_dbg_kms(mtk_dp->drm_dev,
		    "[DPTX] encoder_id = %d, fs = %d, ch = %d\n", encoder_id, fs, channel);

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_33F0 + 1 + reg_offset, BIT(1), BIT(1));

	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3304 + reg_offset,
			 AU_PRTY_REGEN_DP_ENC1_P0_MASK,
			 AU_PRTY_REGEN_DP_ENC1_P0_MASK);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3304 + reg_offset,
			 AU_CH_STS_REGEN_DP_ENC1_P0_MASK,
			 AU_CH_STS_REGEN_DP_ENC1_P0_MASK);

	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3304 + reg_offset,
			 0x1000, 0x1000);

	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3088 + reg_offset,
			 AUDIO_2CH_SEL_DP_ENC0_P0_MASK,
			 AUDIO_2CH_SEL_DP_ENC0_P0_MASK);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3088 + reg_offset,
			 AUDIO_MN_GEN_EN_DP_ENC0_P0_MASK,
			 AUDIO_MN_GEN_EN_DP_ENC0_P0_MASK);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3088 + reg_offset,
			 AUDIO_8CH_SEL_DP_ENC0_P0_MASK,
			 AUDIO_8CH_SEL_DP_ENC0_P0_MASK);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3088 + reg_offset,
			 AU_EN_DP_ENC0_P0,
			 AU_EN_DP_ENC0_P0);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3040 + reg_offset,
			 AUDIO_16CH_SEL_DP_ENC0_P0_MASK,
			 AUDIO_16CH_SEL_DP_ENC0_P0_MASK);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3040 + reg_offset,
			 AUDIO_32CH_SEL_DP_ENC0_P0_MASK,
			 AUDIO_32CH_SEL_DP_ENC0_P0_MASK);

	switch (fs) {
	case 44100:
	default:
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3324 + reg_offset,
				 (0x0 << AUDIO_PATTERN_GEN_FS_SEL_DP_ENC1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENC1_P0_MASK);
		break;

	case 48000:
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3324 + reg_offset,
				 (0x1 << AUDIO_PATTERN_GEN_FS_SEL_DP_ENC1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENC1_P0_MASK);
		break;

	case 192000:
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3324 + reg_offset,
				 (0x2 << AUDIO_PATTERN_GEN_FS_SEL_DP_ENC1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENC1_P0_MASK);
		break;
	}

	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3088 + reg_offset, 0,
			 AUDIO_2CH_EN_DP_ENC0_P0_MASK);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3088 + reg_offset, 0,
			 AUDIO_8CH_EN_DP_ENC0_P0_MASK);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3040 + reg_offset, 0,
			 AUDIO_16CH_EN_DP_ENC0_P0_MASK);
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3040 + reg_offset, 0,
			 AUDIO_32CH_EN_DP_ENC0_P0_MASK);

	switch (channel) {
	case 2:
	default:
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3324 + reg_offset,
				 (0x0 << AUDIO_PATTERN_GEN_CH_NUM_DP_ENC1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENC1_P0_MASK);
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3088 + reg_offset,
				 (0x1 << AUDIO_2CH_EN_DP_ENC0_P0_FLDMASK_POS),
				 AUDIO_2CH_EN_DP_ENC0_P0_MASK);

		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_331C + reg_offset,
				 (0x1 << TDM_AUDIO_DATA_CH_NUM_DP_ENC1_P0_FLDMASK_POS),
				 TDM_AUDIO_DATA_CH_NUM_DP_ENC1_P0_MASK);
		mtk_dp_spkg_asp_hb32_v2(mtk_dp, encoder_id, TRUE, DP_SDP_ASP_HB3_AU02CH, 0x0);
		break;

	case 8:
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3324 + reg_offset,
				 (0x1 << AUDIO_PATTERN_GEN_CH_NUM_DP_ENC1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENC1_P0_MASK);
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3088 + reg_offset,
				 (0x1 << AUDIO_8CH_EN_DP_ENC0_P0_FLDMASK_POS),
				 AUDIO_8CH_EN_DP_ENC0_P0_MASK);

		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_331C + reg_offset,
				 (0x7 << TDM_AUDIO_DATA_CH_NUM_DP_ENC1_P0_FLDMASK_POS),
				 TDM_AUDIO_DATA_CH_NUM_DP_ENC1_P0_MASK);
		mtk_dp_spkg_asp_hb32_v2(mtk_dp, encoder_id, TRUE, DP_SDP_ASP_HB3_AU08CH, 0x0);
		break;

	case 16:
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3324 + reg_offset,
				 (0x2 << AUDIO_PATTERN_GEN_CH_NUM_DP_ENC1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENC1_P0_MASK);
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3040 + reg_offset,
				 (0x1 << AUDIO_16CH_EN_DP_ENC0_P0_FLDMASK_POS),
				 AUDIO_16CH_EN_DP_ENC0_P0_MASK);
		break;

	case 32:
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_3324 + reg_offset,
				 (0x3 << AUDIO_PATTERN_GEN_CH_NUM_DP_ENC1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENC1_P0_MASK);
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3040 + reg_offset,
				 (0x1 << AUDIO_32CH_EN_DP_ENC0_P0_FLDMASK_POS),
				 AUDIO_32CH_EN_DP_ENC0_P0_MASK);
		break;
	}

	/* TDM to DPTX reset [1] */
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_331C + reg_offset,
			TDM_AUDIO_RST_DP_ENC1_P0_MASK,
			TDM_AUDIO_RST_DP_ENC1_P0_MASK);
	udelay(5);
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_331C + reg_offset,
			0x0, TDM_AUDIO_RST_DP_ENC1_P0_MASK);
	/* audio channel count change reset */
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC1_P0_33F0 + 1 + reg_offset, 0, BIT(1));
}

static void mtk_dp_audio_set_mdiv(struct mtk_dp *mtk_dp,
				  const enum dp_encoder_id encoder_id, u8 div)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_30BC + reg_offset,
			 (div << AUDIO_M_CODE_MULT_DIV_SEL_DP_ENC0_P0_FLDMASK_POS),
			 AUDIO_M_CODE_MULT_DIV_SEL_DP_ENC0_P0_MASK);
}

static void mtk_dp_audio_config(struct mtk_dp *mtk_dp,
					const enum dp_encoder_id encoder_id)
{
	int ch, fs, len;
	u8 table[8][5] = {"X1", "X2", "X4", "X8",
			  "/2", "/4", "X1", "/8"};

	ch = mtk_dp->info[encoder_id].audio_cur_cfg.channels;
	fs = mtk_dp->info[encoder_id].audio_cur_cfg.sample_rate;
	len = mtk_dp->info[encoder_id].audio_cur_cfg.word_length_bits;

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] [%d] ch:%d, fs:%d, len:%d\n", encoder_id, ch, fs, len);

	mtk_dp_audio_sdp_config(mtk_dp, encoder_id, ch, fs, len);

	mtk_dp_audio_ch_status_set(mtk_dp, encoder_id, ch, fs, len);

	mtk_dp_audio_pg_enable(mtk_dp, encoder_id, ch, fs, false);

	drm_dbg_kms(mtk_dp->drm_dev,
		    "[DPTX] [%d] Set audio M div %s\n", encoder_id, table[DP_AUDIO_M_DIV_D2]);

	mtk_dp_audio_set_mdiv(mtk_dp, encoder_id, DP_AUDIO_M_DIV_D2);
}

static void mtk_dp_phy_ssc_enable(struct mtk_dp *mtk_dp, const u8 enable)
{
	if (enable)
		PHY_WRITE_BYTE_MASK(mtk_dp,
				    (mtk_dp->data->phyd_dig_glb_offset
				     + DP_PHY_DIG_PLL_CTL_1),
				    0x1 << TPLL_SSC_EN_FLDMASK_POS,
				    TPLL_SSC_EN_FLDMASK);
	else
		PHY_WRITE_BYTE_MASK(mtk_dp,
				    (mtk_dp->data->phyd_dig_glb_offset
				     + DP_PHY_DIG_PLL_CTL_1),
				    0x0, TPLL_SSC_EN_FLDMASK);

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] Phy SSC enable = %d\n", enable);
}

static void mtk_dp_phy_set_link_rate(struct mtk_dp *mtk_dp, enum dp_link_rate val)
{
	switch (val) {
	case DP_LINK_RATE_RBR:
		/* Set gear : 0x0 : RBR, 0x1 : HBR, 0x2 : HBR2, 0x3 : HBR3 */
		PHY_WRITE_4BYTE(mtk_dp,
				(mtk_dp->data->phyd_dig_glb_offset + DP_PHY_DIG_BIT_RATE), 0x0);
		break;

	case DP_LINK_RATE_HBR:
		/* Set gear : 0x0 : RBR, 0x1 : HBR, 0x2 : HBR2, 0x3 : HBR3 */
		PHY_WRITE_4BYTE(mtk_dp,
				(mtk_dp->data->phyd_dig_glb_offset + DP_PHY_DIG_BIT_RATE), 0x1);
		break;

	case DP_LINK_RATE_HBR2:
		/* Set gear : 0x0 : RBR, 0x1 : HBR, 0x2 : HBR2, 0x3 : HBR3 */
		PHY_WRITE_4BYTE(mtk_dp,
				(mtk_dp->data->phyd_dig_glb_offset + DP_PHY_DIG_BIT_RATE), 0x2);
		break;

	case DP_LINK_RATE_HBR3:
		/* Set gear : 0x0 : RBR, 0x1 : HBR, 0x2 : HBR2, 0x3 : HBR3 */
		PHY_WRITE_4BYTE(mtk_dp,
				(mtk_dp->data->phyd_dig_glb_offset + DP_PHY_DIG_BIT_RATE), 0x3);
		break;
	default:
		break;
	}
}

static void mtk_dp_phy_training_config(struct mtk_dp *mtk_dp, const u8 link_rate,
				       const u8 lane_count, const u8 ssc_enable)
{
	mtk_dp_phy_reset_swing_pre(mtk_dp);
	mtk_dp_phy_ssc_enable(mtk_dp, ssc_enable);

	/* step1: phy-d power down */
	mtk_dp_phyd_power_down(mtk_dp);

	/* step2: phy-d set link rate */
	mtk_dp_phy_set_link_rate(mtk_dp, link_rate);
	mtk_dp_phy_power_on(mtk_dp);

	/* step3: phy-d enable lane */
	mtk_dp_phy_set_lane_pwr(mtk_dp, lane_count);
}

bool mtk_dp_swingt_set_pre_emphasis(struct mtk_dp *mtk_dp,
				    enum dp_lane_num lane_num,
				    enum dp_swing_num swing_level,
				    enum dp_preemphasis_num pre_emphasis_level)
{
	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] lane:%d, set Swing:0x%x, Emp:0x%x\n",
		    lane_num, swing_level, pre_emphasis_level);

	switch (lane_num) {
	case DP_LANE0:
		PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan0_offset + DRIVING_FORCE,
				     (swing_level << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
				     DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan0_offset + DRIVING_FORCE,
				     (pre_emphasis_level << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
				     DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		break;

	case DP_LANE1:
		PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan1_offset + DRIVING_FORCE,
				     (swing_level << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
				     DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan1_offset + DRIVING_FORCE,
				     (pre_emphasis_level << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
				     DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		break;

	case DP_LANE2:
		PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan2_offset + DRIVING_FORCE,
				     (swing_level << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
				     DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan2_offset + DRIVING_FORCE,
				     (pre_emphasis_level << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
				     DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		break;

	case DP_LANE3:
		PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan3_offset + DRIVING_FORCE,
				     (swing_level << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
				     DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp, mtk_dp->data->phyd_dig_lan3_offset + DRIVING_FORCE,
				     (pre_emphasis_level << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
				     DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		break;

	default:
		dev_err(mtk_dp->dev, "[DPTX] lane number is error\n");
		return false;
	}

	return true;
}

void mtk_dp_training_check_swing_pre(struct mtk_dp *mtk_dp,
				     u8 lane_count,
				     u8 *dpcd_202,
				     u8 *dpcd_buffer,
				     u8 is_adjustable_swing_pre,
				     u8 is_lttpr)
{
	u8 swing, emhasis;
	u8 lane01_adjust_offset, lane23_adjust_offset;

	if (is_lttpr) {
		lane01_adjust_offset = 3; /* F0033h-F0030h */
		lane23_adjust_offset = 4; /* F0034h-F0030h */
	} else {
		lane01_adjust_offset = 4; /* 206h-202h */
		lane23_adjust_offset = 5; /* 207h-202h */
	}

	/* lane0 */
	if (lane_count >= 0x1) {
		swing = (dpcd_202[lane01_adjust_offset] & 0x3);
		emhasis = ((dpcd_202[lane01_adjust_offset] & 0x0C) >> 2);

				/* Adjust the swing and pre-emphasis */
		if (is_adjustable_swing_pre)
			mtk_dp_swingt_set_pre_emphasis(mtk_dp, DP_LANE0, swing, emhasis);
		/* Adjust the swing and pre-emphasis done, notify Sink Side */
		dpcd_buffer[0x0] = swing | (emhasis << 3);

		/* MAX_SWING_REACHED */
		if (swing == DP_SWING3)
			dpcd_buffer[0x0] |= BIT(2);
		/* MAX_PRE-EMPHASIS_REACHED */
		if (emhasis == DP_PREEMPHASIS3)
			dpcd_buffer[0x0] |= BIT(5);
	}

	/* lane1 */
	if (lane_count >= 0x2) {
		swing = (dpcd_202[lane01_adjust_offset] & 0x30) >> 4;
		emhasis = ((dpcd_202[lane01_adjust_offset] & 0xC0) >> 6);

		/* Adjust the swing and pre-emphasis */
		if (is_adjustable_swing_pre)
			mtk_dp_swingt_set_pre_emphasis(mtk_dp, DP_LANE1, swing, emhasis);
		/* Adjust the swing and pre-emphasis done, notify Sink Side */
		dpcd_buffer[0x1] = swing | (emhasis << 3);

		/* MAX_SWING_REACHED */
		if (swing == DP_SWING3)
			dpcd_buffer[0x1] |= BIT(2);
		/* MAX_PRE-EMPHASIS_REACHED */
		if (emhasis == DP_PREEMPHASIS3)
			dpcd_buffer[0x1] |= BIT(5);
	}

	/* lane 2,3 */
	if (lane_count == 0x4) {
		swing = (dpcd_202[lane23_adjust_offset] & 0x3);
		emhasis = ((dpcd_202[lane23_adjust_offset] & 0x0C) >> 2);

		/* Adjust the swing and pre-emphasis */
		if (is_adjustable_swing_pre)
			mtk_dp_swingt_set_pre_emphasis(mtk_dp, DP_LANE2, swing, emhasis);
		/* Adjust the swing and pre-emphasis done, notify Sink Side */
		dpcd_buffer[0x2] = swing | (emhasis << 3);

		/* MAX_SWING_REACHED */
		if (swing == DP_SWING3)
			dpcd_buffer[0x2] |= BIT(2);
		/* MAX_PRE-EMPHASIS_REACHED */
		if (emhasis == DP_PREEMPHASIS3)
			dpcd_buffer[0x2] |= BIT(5);

		swing = (dpcd_202[lane23_adjust_offset] & 0x30) >> 4;
		emhasis = ((dpcd_202[lane23_adjust_offset] & 0xC0) >> 6);

		/* Adjust the swing and pre-emphasis */
		if (is_adjustable_swing_pre)
			mtk_dp_swingt_set_pre_emphasis(mtk_dp, DP_LANE3, swing, emhasis);
		/* Adjust the swing and pre-emphasis done, notify Sink Side */
		dpcd_buffer[0x3] = swing | (emhasis << 3);

		/* MAX_SWING_REACHED */
		if (swing == DP_SWING3)
			dpcd_buffer[0x3] |= BIT(2);
		/* MAX_PRE-EMPHASIS_REACHED */
		if (emhasis == DP_PREEMPHASIS3)
			dpcd_buffer[0x3] |= BIT(5);
	}

	/* Wait signal stable enough */
	mdelay(1);
}

static void mtk_dp_phy_set_param(struct mtk_dp *mtk_dp)
{
	u8 i;

	const u32 phyd_dig_lan_base_addr[4] = {
		mtk_dp->data->phyd_dig_lan0_offset, mtk_dp->data->phyd_dig_lan1_offset,
		mtk_dp->data->phyd_dig_lan2_offset, mtk_dp->data->phyd_dig_lan3_offset};

	/* 4-1. PLL */
	PHY_WRITE_BYTE_MASK(mtk_dp, 0x0614, BIT(0), BIT(0));
	/* 4-2. Unused AUX TX High-Z */
	PHY_WRITE_4BYTE_MASK(mtk_dp, 0x0700, 0x0, BIT(20));

	/* 4-4. Swing and Pre-emphasis Optimization */
	for (i = 0; i < 4; i++) {
		PHY_WRITE_4BYTE(mtk_dp, phyd_dig_lan_base_addr[i] + DRIVING_PARAM_3, 0x110e0c0a);
		PHY_WRITE_4BYTE(mtk_dp, phyd_dig_lan_base_addr[i] + DRIVING_PARAM_4, 0x1212110e);
		PHY_WRITE_4BYTE(mtk_dp, phyd_dig_lan_base_addr[i] + DRIVING_PARAM_5, 0x1815);
		PHY_WRITE_4BYTE(mtk_dp, phyd_dig_lan_base_addr[i] + DRIVING_PARAM_6, 0x7040200);
		PHY_WRITE_4BYTE(mtk_dp, phyd_dig_lan_base_addr[i] + DRIVING_PARAM_7, 0x60300);
		PHY_WRITE_4BYTE(mtk_dp, phyd_dig_lan_base_addr[i] + DRIVING_PARAM_8, 0x3);
	}
}

static void mtk_dp_phy_4lane_enable(struct mtk_dp *mtk_dp)
{
	u8 i;
	u8 lane_count;
	u16 value;
	u32 tmp;

	lane_count = 4;
	value = (BIT(12) | BIT(13));

	tmp = readl(mtk_dp->phy_mux_regs);
	tmp |= mtk_dp->data->phy_4lane_ctrl_bit;
	writel(tmp, mtk_dp->phy_mux_regs);

	if (mtk_dp->data->need_phy_lane_enable_set) {
		for (i = 1; i <= lane_count; i++)
			PHY_WRITE_2BYTE_MASK(mtk_dp, 0x0100 * i, value, (BIT(12) | BIT(13)));
	}
}

static void mtk_dp_phy_4lane_disable(struct mtk_dp *mtk_dp)
{
	u8 i;
	u8 lane_count;
	u16 value;
	u32 tmp;

	lane_count = 2;
	value = BIT(12);

	tmp = readl(mtk_dp->phy_mux_regs);
	tmp &= ~mtk_dp->data->phy_4lane_ctrl_bit;
	writel(tmp, mtk_dp->phy_mux_regs);

	if (mtk_dp->data->need_phy_lane_enable_set) {
		for (i = 1; i <= lane_count; i++)
			PHY_WRITE_2BYTE_MASK(mtk_dp, 0x0100 * i, value, (BIT(12) | BIT(13)));
	}
}

static void mtk_dp_digital_encoder_bs_symbol_cnt_reset(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3000,
				BIT(BS_SYMBOL_CNT_RESET_DP_ENC0_P0_FLDMASK_POS),
				BS_SYMBOL_CNT_RESET_DP_ENC0_P0_MASK);

	usleep_range(1000, 5000);

	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3000, 0,
				BS_SYMBOL_CNT_RESET_DP_ENC0_P0_MASK);

	dev_dbg(mtk_dp->dev, "[DPTX] encoder bs symbol cnt reset\n");
}

static enum dp_train_stage mtk_dp_check_training_res(struct mtk_dp *mtk_dp, u8 dpcd_202)
{
	enum dp_train_stage res = DP_LT_PASS;

	if (mtk_dp->train_info.cr_done == 0x0) {
		if ((dpcd_202 & 0x01) != 0x01)
			res = DP_LT_CR_L0_FAIL;
		else if ((dpcd_202 & 0x11) != 0x11)
			res = DP_LT_CR_L1_FAIL;
		else
			res = DP_LT_CR_L2_FAIL;
	} else if (mtk_dp->train_info.eq_done == 0x0) {
		if ((dpcd_202 & 0x07) != 0x07)
			res = DP_LT_EQ_L0_FAIL;
		else if ((dpcd_202 & 0x77) != 0x77)
			res = DP_LT_EQ_L1_FAIL;
		else
			res = DP_LT_EQ_L2_FAIL;
	}

	return res;
}

static void mtk_dp_set_enhanced_frame_mode(struct mtk_dp *mtk_dp, bool enable)
{
	enum dp_encoder_id encoder_id;
	u32 reg_offset;

	for (encoder_id = 0; encoder_id < mtk_dp->data->encoder_num; encoder_id++) {
		reg_offset = DP_REG_OFFSET(encoder_id);

		if (enable)
			/* [4] enhanced_frame_mode [1 : 0] lane_num */
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3000 + reg_offset,
					BIT(4), BIT(4));
		else
			/* [4] enhanced_frame_mode [1 : 0] lane_num */
			WRITE_BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_3000 + reg_offset, 0, BIT(4));
	}
}

static enum dp_train_stage mtk_dp_training_flow(struct mtk_dp *mtk_dp, u8 link_rate, u8 lane_count)
{
	u8 dpcd_buffer[0x4] = {0}, dpcd_202[0x6] = {0}, temp[0x6] = {0}, dpcd_200c[0x3] = {0};
	u8 dpcd_206 = 0xFF;
	u8 retry_times = 0;
	u8 control = 0;
	u8 loop = 0;
	u8 cr_loop = 0;
	u8 eq_loop = 0;
	bool ssc_enable = false;
	enum dp_train_stage res = DP_LT_NONE;

	memset(temp, 0x0, sizeof(temp));
	memset(dpcd_buffer, 0x0, sizeof(dpcd_buffer));
	memset(dpcd_202, 0x0, sizeof(dpcd_202));
	memset(dpcd_200c, 0x0, sizeof(dpcd_200c));

	mtk_dp_check_and_set_power_state(mtk_dp);

	temp[0] = link_rate;
	temp[1] = (lane_count | DP_AUX_SET_ENAHNCED_FRAME);

	/* DPCD_00100 */
	drm_dp_dpcd_write(&mtk_dp->aux, DP_LINK_BW_SET, temp, 0x2);

	mtk_dp_ssc_check(mtk_dp, &ssc_enable);
	mtk_dp_phy_training_config(mtk_dp, link_rate, lane_count, ssc_enable);
	mtk_dp_set_lane_count(mtk_dp, lane_count);
	mdelay(5);

	do {
		loop++;
		if (!mtk_dp->train_info.cable_plug_in) {
			dev_info(mtk_dp->dev, "[DPTX] Training Abort, HPD is low\n");
			return DP_LT_NONE;
		}

		if (mtk_dp->train_info.cr_done == 0x0) {
			dev_info(mtk_dp->dev, "[DPTX] CR Training START\n");
			mtk_dp_training_set_scramble(mtk_dp, false);

			if (control == 0x0) {
				mtk_dp_train_set_pattern(mtk_dp, DP_TPS1);
				control = 0x1;
				temp[0] = 0x21;
				/* DPCD_00102 */
				drm_dp_dpcd_write(&mtk_dp->aux, DP_TRAINING_PATTERN_SET, temp, 0x1);
				/* DPCD_00206 */
				drm_dp_dpcd_read(&mtk_dp->aux, DP_ADJUST_REQUEST_LANE0_1,
						 (temp + 4), 0x2);
				loop++;

				/* force use SWING = 0 & PRE = 0 to start 1st link training */
				temp[4] = 0x00;
				temp[5] = 0x00;
				mtk_dp_training_check_swing_pre(mtk_dp, lane_count, temp,
								dpcd_buffer, true, false);
			}

			/* DPCD_00103 */
			drm_dp_dpcd_write(&mtk_dp->aux, DP_TRAINING_LANE0_SET, dpcd_buffer,
					  lane_count);
			drm_dp_link_train_clock_recovery_delay(&mtk_dp->aux, mtk_dp->rx_cap);
			/* DPCD_00202 */
			drm_dp_dpcd_read(&mtk_dp->aux, DP_LANE0_1_STATUS, dpcd_202, 0x6);
			if (mtk_dp->train_info.sink_ext_cap_en) {
				/* DPCD_0200C */
				drm_dp_dpcd_read(&mtk_dp->aux, DP_LANE0_1_STATUS_ESI, dpcd_200c,
						 0x3);
				dpcd_202[0] = dpcd_200c[0]; /*  copy DPCD200C=>DCPD202 */
				dpcd_202[1] = dpcd_200c[1]; /*  copy DPCD200D=>DCPD203 */
				dpcd_202[2] = dpcd_200c[2]; /*  copy DPCD200E=>DCPD204 */
			}

			if (drm_dp_clock_recovery_ok(dpcd_202, lane_count)) {
				dev_info(mtk_dp->dev, "[DPTX] CR Training Success\n");

				mtk_dp->train_info.cr_done = true;

				retry_times = 0x0;
				loop = 0x1;
				eq_loop = 0;
			} else {
				/* request swing & emp is the same eith last time */
				if (dpcd_206 == dpcd_202[0x4]) {
					/* lane0 match max swing */
					if ((dpcd_206 & 0x3) == 0x3)
						loop = DP_LT_MAX_LOOP;
					else
						loop++;
				} else {
					dpcd_206 = dpcd_202[0x4];
				}

				cr_loop++;
				dev_info(mtk_dp->dev, "[DPTX] CR Training Fail\n");
			}
		} else if (mtk_dp->train_info.eq_done == 0x0) {
			dev_info(mtk_dp->dev, "[DPTX] EQ Training START\n");

			if (control == 0x1) {
				if (mtk_dp->train_info.tps4_support) {
					mtk_dp_train_set_pattern(mtk_dp, DP_TPS4);
					temp[0] = 0x07;
				} else if (mtk_dp->train_info.tps3_support) {
					mtk_dp_train_set_pattern(mtk_dp, DP_TPS3);
					temp[0] = 0x23;
				} else {
					mtk_dp_train_set_pattern(mtk_dp, DP_TPS2);
					temp[0] = 0x22;
				}
				/* DPCD_00102 */
				drm_dp_dpcd_write(&mtk_dp->aux, DP_TRAINING_PATTERN_SET, temp, 0x1);

				control = 0x2;
				/* DPCD_00206 */
				drm_dp_dpcd_read(&mtk_dp->aux, DP_ADJUST_REQUEST_LANE0_1,
						 (dpcd_202 + 4), 0x2);

				loop++;
				mtk_dp_training_check_swing_pre(mtk_dp, lane_count, dpcd_202,
								dpcd_buffer, true, false);
			}

			/* DPCD_00103 */
			drm_dp_dpcd_write(&mtk_dp->aux, DP_TRAINING_LANE0_SET, dpcd_buffer,
					  lane_count);
			drm_dp_link_train_channel_eq_delay(&mtk_dp->aux, mtk_dp->rx_cap);

			/* DPCD_00202 */
			drm_dp_dpcd_read(&mtk_dp->aux, DP_LANE0_1_STATUS, dpcd_202, 0x6);
			if (mtk_dp->train_info.sink_ext_cap_en) {
				/* DPCD_0200C */
				drm_dp_dpcd_read(&mtk_dp->aux, DP_LANE0_1_STATUS_ESI, dpcd_200c,
						 0x3);
				dpcd_202[0] = dpcd_200c[0]; /* copy DPCD200C=>DCPD202 */
				dpcd_202[1] = dpcd_200c[1]; /* copy DPCD200D=>DCPD203 */
				dpcd_202[2] = dpcd_200c[2]; /* copy DPCD200E=>DCPD204 */
			}

			if (!drm_dp_clock_recovery_ok(dpcd_202, lane_count)) {
				mtk_dp->train_info.cr_done = false;
				mtk_dp->train_info.eq_done = false;
				break;
			}

			if (drm_dp_channel_eq_ok(dpcd_202, lane_count)) {
				dev_info(mtk_dp->dev, "[DPTX] EQ Training Success\n");
				if (dpcd_202[2] & 0x1) {
					mtk_dp->train_info.eq_done = true;
					dev_info(mtk_dp->dev, "[DPTX] Inter-lane skew Success\n");
					break;
				}
			}

			dev_info(mtk_dp->dev, "[DPTX] EQ Training Fail\n");
			eq_loop++;
			if (dpcd_206 == dpcd_202[0x4])
				loop++;
			else
				dpcd_206 = dpcd_202[0x4];
		}

		mtk_dp_training_check_swing_pre(mtk_dp, lane_count, dpcd_202,
						dpcd_buffer, true, false);
		dev_info(mtk_dp->dev, "[DPTX] retry_times:%d, loop:%d\n", retry_times, loop);

	} while ((loop < DP_LT_RETRY_LIMIT) &&
		 (cr_loop < DP_LT_MAX_CR_LOOP) &&
		 (eq_loop < DP_LT_MAX_EQ_LOOP));

	temp[0] = 0x0;
	/* DPCD_00102 */
	drm_dp_dpcd_write(&mtk_dp->aux, DP_TRAINING_PATTERN_SET, temp, 0x1);
	mtk_dp_train_set_pattern(mtk_dp, DP_0);

	if (mtk_dp->train_info.eq_done) {
		mtk_dp->train_info.link_rate = link_rate;
		mtk_dp->train_info.link_lane_count = lane_count;

		mtk_dp_training_set_scramble(mtk_dp, true);
		mtk_dp_set_enhanced_frame_mode(mtk_dp, ENABLE_DP_EF_MODE);

		mtk_dp_digital_encoder_bs_symbol_cnt_reset(mtk_dp);

		dev_info(mtk_dp->dev, "[DPTX] Training PASS, link rate:0x%x, lane count:%d\n",
			mtk_dp->train_info.link_rate,
			mtk_dp->train_info.link_lane_count);

		return DP_LT_PASS;
	}

	dev_info(mtk_dp->dev, "[DPTX] Training Fail\n");

	res = mtk_dp_check_training_res(mtk_dp, dpcd_202[0]);

	return res;
}

static int mtk_dp_set_training_start(struct mtk_dp *mtk_dp)
{
	enum dp_link_rate max_link_rate = mtk_dp->train_info.max_link_rate;
	enum dp_lane_count max_lane_count = mtk_dp->train_info.max_link_lane_count;
	enum dp_link_rate link_rate;
	enum dp_lane_count lane_count;
	u32 loop;

	if (mtk_dp->train_info.dp_version == DP_VER_14)
		loop = DP_CTS_RETRAIN_TIMES_14;
	else
		loop = DP_CTS_RETRAIN_TIMES_DEFAULT;

	link_rate = mtk_dp->rx_cap[1];
	lane_count = mtk_dp->rx_cap[2] & GENMASK(4, 0);
	dev_dbg(mtk_dp->dev, "[DPTX] RX support link rate:0x%x, lane count:%d",
		link_rate, lane_count);

	link_rate = (link_rate >= max_link_rate) ?
		max_link_rate : link_rate;
	lane_count = (lane_count >= max_lane_count) ?
		max_lane_count : lane_count;

	switch (link_rate) {
	case DP_LINK_RATE_RBR:
	case DP_LINK_RATE_HBR:
	case DP_LINK_RATE_HBR2:
	case DP_LINK_RATE_HBR25:
	case DP_LINK_RATE_HBR3:
		break;

	default:
		if (link_rate > DP_LINK_RATE_HBR3)
			link_rate = DP_LINK_RATE_HBR3;
		else if (link_rate > DP_LINK_RATE_HBR2)
			link_rate = DP_LINK_RATE_HBR2;
		else if (link_rate > DP_LINK_RATE_HBR)
			link_rate = DP_LINK_RATE_HBR;
		else
			link_rate = DP_LINK_RATE_RBR;
		break;
	};

	max_link_rate = link_rate;

	do {
		if (!mtk_dp->train_info.cable_plug_in) {
			dev_dbg(mtk_dp->dev, "[DPTX] plug out, stop training");
			return DP_RET_PLUG_OUT;
		}

		mtk_dp->train_info.cr_done = false;
		mtk_dp->train_info.eq_done = false;

		dev_dbg(mtk_dp->dev, "[DPTX] training with link rate:0x%x, lane count:%d",
			link_rate, lane_count);

		mtk_dp_training_flow(mtk_dp, link_rate, lane_count);

		if (!mtk_dp->train_info.cr_done) {
			switch (link_rate) {
			case DP_LINK_RATE_RBR:
				lane_count = lane_count / 2;
				link_rate = max_link_rate;

				if (lane_count == 0x0)
					return DP_RET_TRANING_FAIL;

				break;

			case DP_LINK_RATE_HBR:
				link_rate = DP_LINK_RATE_RBR;
				break;

			case DP_LINK_RATE_HBR2:
				link_rate = DP_LINK_RATE_HBR;
				break;

			case DP_LINK_RATE_HBR3:
				link_rate = DP_LINK_RATE_HBR2;
				break;

			default:
				return DP_RET_TRANING_FAIL;
			};

			loop--;
		} else if (!mtk_dp->train_info.eq_done) {
			if (lane_count == DP_4LANE)
				lane_count = DP_2LANE;
			else if (lane_count >= DP_1LANE)
				lane_count = DP_1LANE;
			else
				return DP_RET_TRANING_FAIL;

			loop--;
		} else {
			return DP_RET_NOERR;
		}
	} while (loop > 0);

	return DP_RET_TRANING_FAIL;
}

static int mtk_dp_training(struct mtk_dp *mtk_dp)
{
	int ret = DP_RET_NOERR;

	if (!mtk_dp->train_info.cable_plug_in || mtk_dp->dp_ready)
		return DP_RET_PLUG_OUT;

	mtk_dp_fec_disable(mtk_dp);

	ret = mtk_dp_set_training_start(mtk_dp);
	if (ret == DP_RET_NOERR)
		mtk_dp->dp_ready = true;
	else
		dev_info(mtk_dp->dev, "[DPTX] Handle Training Fail 6 times\n");

	return ret;
}

static void mtk_dp_phy_setting(struct mtk_dp *mtk_dp)
{
	/* step1: phy init */
	if (mtk_dp->train_info.max_link_lane_count == DP_4LANE)
		mtk_dp_phy_4lane_enable(mtk_dp);
	else
		mtk_dp_phy_4lane_disable(mtk_dp);

	mtk_dp_phy_set_param(mtk_dp);

	if (mtk_dp->data->phy_patch)
		mtk_dp->data->phy_patch(mtk_dp);

	/* step2: phy power ON */
	mtk_dp_phy_power_on(mtk_dp);
}

void mtk_dp_dsc_disable(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] DSC disable\n");

	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_31C4 + reg_offset,
			 0,
			 PPS_HW_BYPASS_MASK_DP_ENC0_P0_MASK);

	/* DSC Disable */
	WRITE_BYTE_MASK(mtk_dp,
			MTK_DP_ENC1_P0_336C + reg_offset, 0, BIT(0));
	WRITE_BYTE_MASK(mtk_dp,
			MTK_DP_ENC0_P0_300C + 1 + reg_offset, 0, BIT(1));
	/* default 8bit */
	WRITE_BYTE_MASK(mtk_dp,
			MTK_DP_ENC0_P0_303C + 1 + reg_offset,
			0x3, GENMASK(2, 0));
	/* default RGB */
	WRITE_BYTE_MASK(mtk_dp,
			MTK_DP_ENC0_P0_303C + 1 + reg_offset,
			0x0, GENMASK(6, 4));

	/* 31FC[12] : HDE last num control */
	/* 31FC[12] : HDE last num control */
	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_ENC0_P0_31FC + reg_offset,
			 0, DE_LAST_NUM_SW_DP_ENC0_P0_MASK);
}

static void mtk_dp_init_port(struct mtk_dp *mtk_dp)
{
	enum dp_encoder_id encoder_id;

	dev_dbg(mtk_dp->dev, "[DPTX] %s+\n", __func__);

	mtk_dp_set_idle_pattern(mtk_dp, true);
	mtk_dp_init_variable(mtk_dp);

	mtk_dp_fec_disable(mtk_dp);
	for (encoder_id = 0; encoder_id < mtk_dp->data->encoder_num; encoder_id++)
		mtk_dp_dsc_disable(mtk_dp, encoder_id);

	mtk_dp_initialize_settings(mtk_dp);
	mtk_dp_initialize_aux_settings(mtk_dp);
	for (encoder_id = 0; encoder_id < mtk_dp->data->encoder_num; encoder_id++)
		mtk_dp_initialize_digital_settings(mtk_dp, encoder_id);

	mtk_dp_analog_power_on(mtk_dp);
	mtk_dp_phy_setting(mtk_dp);

	mtk_dp_initialize_hpd_detect_settings(mtk_dp);

	mtk_dp_digital_sw_reset(mtk_dp);
}

void mtk_dp_phy_patch(struct mtk_dp *mtk_dp)
{
	int lane_offset = 0x100;
	int i;

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] phy patch.\n");
	PHY_WRITE_4BYTE(mtk_dp,
			mtk_dp->data->phyd_ana_glb_offset
			+ DP_PHY_GLB_FORCE_CTRL_00, 0x00000021);
	PHY_WRITE_4BYTE(mtk_dp,
			mtk_dp->data->phyd_ana_lan0_offset
			+ DP_PHY_LANE_TX_2, 0x0001000e);
	PHY_WRITE_4BYTE(mtk_dp,
			mtk_dp->data->phyd_ana_lan1_offset
			+ DP_PHY_LANE_TX_2, 0x0001000e);
	PHY_WRITE_4BYTE(mtk_dp,
			mtk_dp->data->phyd_ana_lan2_offset
			+ DP_PHY_LANE_TX_2, 0x0001000e);
	PHY_WRITE_4BYTE(mtk_dp,
			mtk_dp->data->phyd_ana_lan3_offset
			+ DP_PHY_LANE_TX_2, 0x0001000e);

	PHY_WRITE_4BYTE(mtk_dp,
			mtk_dp->data->phyd_dig_glb_offset
			+ DP_PHY_DIG_GLB_DA_REG_00, 0x020fff00);
	PHY_WRITE_4BYTE(mtk_dp,
			mtk_dp->data->phyd_dig_glb_offset
			+ DP_PHY_DIG_GLB_DA_REG_01, 0x00020202);
	PHY_WRITE_4BYTE(mtk_dp,
			mtk_dp->data->phyd_dig_glb_offset
			+ DP_PHY_DIG_GLB_DA_REG_02, 0x00a5231a);
	PHY_WRITE_4BYTE(mtk_dp,
			mtk_dp->data->phyd_dig_glb_offset
			+ DP_PHY_DIG_GLB_DA_REG_03, 0xdde70305);

	for (i = 0; i < 4; i++) {
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_3,
				     (0x10 <<
				      XTP_LN_TX_LCTXC0_SW0_PRE0_FLDMASK_POS),
				     XTP_LN_TX_LCTXC0_SW0_PRE0_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_3,
				     (0x14 <<
				      XTP_LN_TX_LCTXC0_SW0_PRE1_FLDMASK_POS),
				     XTP_LN_TX_LCTXC0_SW0_PRE1_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_3,
				     (0x18 <<
				      XTP_LN_TX_LCTXC0_SW0_PRE2_FLDMASK_POS),
				     XTP_LN_TX_LCTXC0_SW0_PRE2_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_3,
				     (0x20 <<
				      XTP_LN_TX_LCTXC0_SW0_PRE3_FLDMASK_POS),
				     XTP_LN_TX_LCTXC0_SW0_PRE3_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_4,
				     (0x18 <<
				      XTP_LN_TX_LCTXC0_SW1_PRE0_FLDMASK_POS),
				     XTP_LN_TX_LCTXC0_SW1_PRE0_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_4,
				     (0x1e <<
				      XTP_LN_TX_LCTXC0_SW1_PRE1_FLDMASK_POS),
				     XTP_LN_TX_LCTXC0_SW1_PRE1_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_4,
				     (0x24 <<
				      XTP_LN_TX_LCTXC0_SW1_PRE2_FLDMASK_POS),
				     XTP_LN_TX_LCTXC0_SW1_PRE2_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_4,
				     (0x20 <<
				      XTP_LN_TX_LCTXC0_SW2_PRE0_FLDMASK_POS),
				     XTP_LN_TX_LCTXC0_SW2_PRE0_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_5,
				     (0x28 <<
				      XTP_LN_TX_LCTXC0_SW2_PRE1_FLDMASK_POS),
				     XTP_LN_TX_LCTXC0_SW2_PRE1_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_5,
				     (0x30 <<
				      XTP_LN_TX_LCTXC0_SW3_PRE0_FLDMASK_POS),
				     XTP_LN_TX_LCTXC0_SW3_PRE0_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_6,
				     (0x00 <<
				      XTP_LN_TX_LCTXCP1_SW0_PRE0_FLDMASK_POS),
				     XTP_LN_TX_LCTXCP1_SW0_PRE0_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_6,
				     (0x04 <<
				      XTP_LN_TX_LCTXCP1_SW0_PRE1_FLDMASK_POS),
				     XTP_LN_TX_LCTXCP1_SW0_PRE1_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_6,
				     (0x08 <<
				      XTP_LN_TX_LCTXCP1_SW0_PRE2_FLDMASK_POS),
				     XTP_LN_TX_LCTXCP1_SW0_PRE2_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_6,
				     (0x10 <<
				      XTP_LN_TX_LCTXCP1_SW0_PRE3_FLDMASK_POS),
				     XTP_LN_TX_LCTXCP1_SW0_PRE3_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_7,
				     (0x00 <<
				      XTP_LN_TX_LCTXCP1_SW1_PRE0_FLDMASK_POS),
				     XTP_LN_TX_LCTXCP1_SW1_PRE0_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_7,
				     (0x06 <<
				      XTP_LN_TX_LCTXCP1_SW1_PRE1_FLDMASK_POS),
				     XTP_LN_TX_LCTXCP1_SW1_PRE1_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_7,
				     (0x0C <<
				      XTP_LN_TX_LCTXCP1_SW1_PRE2_FLDMASK_POS),
				     XTP_LN_TX_LCTXCP1_SW1_PRE2_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_7,
				     (0x00 <<
				      XTP_LN_TX_LCTXCP1_SW2_PRE0_FLDMASK_POS),
				     XTP_LN_TX_LCTXCP1_SW2_PRE0_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_8,
				     (0x08 <<
				      XTP_LN_TX_LCTXCP1_SW2_PRE1_FLDMASK_POS),
				     XTP_LN_TX_LCTXCP1_SW2_PRE1_FLDMASK);
		PHY_WRITE_4BYTE_MASK(mtk_dp,
				     mtk_dp->data->phyd_dig_lan0_offset
				     + (i*lane_offset)
				     + DRIVING_PARAM_8,
				     (0x00 <<
				      XTP_LN_TX_LCTXCP1_SW3_PRE0_FLDMASK_POS),
				     XTP_LN_TX_LCTXCP1_SW3_PRE0_FLDMASK);
	}
}

static void mtk_dp_hpd_handle_in_isr(struct mtk_dp *mtk_dp)
{
	bool current_hpd = mtk_dp_hpd_get_pin_level(mtk_dp);

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] current_hpd:0x%x, phy_status:0x%x\n",
		    current_hpd, mtk_dp->train_info.phy_status);

	if (mtk_dp->train_info.phy_status == HPD_INITIAL_STATE)
		return;

	if ((mtk_dp->train_info.phy_status & (HPD_CONNECT | HPD_DISCONNECT))
		== (HPD_CONNECT | HPD_DISCONNECT)) {
		if (current_hpd)
			mtk_dp->train_info.phy_status &= ~HPD_DISCONNECT;
		else
			mtk_dp->train_info.phy_status &= ~HPD_CONNECT;
	}

	if ((mtk_dp->train_info.phy_status & (HPD_INT_EVNET | HPD_DISCONNECT))
		== (HPD_INT_EVNET | HPD_DISCONNECT)) {
		if (current_hpd)
			mtk_dp->train_info.phy_status &= ~HPD_DISCONNECT;
	}

	/* ignore plug-in --> plug-in event */
	if (mtk_dp->train_info.cable_plug_in)
		mtk_dp->train_info.phy_status &= ~HPD_CONNECT;
	else
		mtk_dp->train_info.phy_status &= ~HPD_DISCONNECT;

	if (mtk_dp->train_info.phy_status & HPD_CONNECT) {
		mtk_dp->train_info.phy_status &= ~HPD_CONNECT;
		mtk_dp->train_info.cable_plug_in = true;
		mtk_dp->train_info.cable_state_change = true;
		mtk_dp->need_debounce = true;

		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] HPD_CON_ISR\n");
	}

	if (mtk_dp->train_info.phy_status & HPD_DISCONNECT) {
		mtk_dp->train_info.phy_status &= ~HPD_DISCONNECT;

		mtk_dp->train_info.cable_plug_in = false;
		mtk_dp->train_info.cable_state_change = true;
		mtk_dp->need_debounce = true;

		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] HPD_DISCON_ISR\n");
	}

	/* handle IRQ in thread */
	if (mtk_dp->train_info.phy_status & HPD_INT_EVNET)
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] ****** HPD_INT ******\n");
}

static void mtk_dp_sdp_path_reset(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	u32 reg_offset = DP_REG_OFFSET(encoder_id);

	WRITE_2BYTE_MASK(mtk_dp, (MTK_DP_ENC0_P0_3004 + reg_offset),
			 (0x1 << SDP_RESET_SW_DP_ENC0_P0_FLDMASK_POS),
			 SDP_RESET_SW_DP_ENC0_P0_MASK);
	udelay(5);

	WRITE_2BYTE_MASK(mtk_dp, (MTK_DP_ENC0_P0_3004 + reg_offset),
			 (0x0 << SDP_RESET_SW_DP_ENC0_P0_FLDMASK_POS),
			 SDP_RESET_SW_DP_ENC0_P0_MASK);
}

void mtk_dp_video_disable(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id)
{
	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] Output Video[%d] disable\n", encoder_id);

	mtk_dp_stop_sent_sdp(mtk_dp, encoder_id);
	mtk_dp_sdp_path_reset(mtk_dp, encoder_id);

	mtk_dp_dsc_disable(mtk_dp, encoder_id);
}

void mtk_dp_audio_update_plugged_status(struct mtk_dp *mtk_dp)
{
	bool plugged = false;
	u8 encoder_id, con_id;

	mutex_lock(&mtk_dp->update_plugged_status_lock);
	if (!mtk_dp->mst_enable) {
		if (mtk_dp->mtk_con[DP_FIRST_CON]) {
			plugged = mtk_dp->mtk_con[DP_FIRST_CON]->video_enable &
				mtk_dp->info[DP_SST_ENCODER_PORT].audio_cur_cfg.detect_monitor;
			drm_dbg_kms(mtk_dp->drm_dev,
				    "[DPTX] SST, video enable:%d, detect monitor:%d\n",
				    mtk_dp->mtk_con[DP_FIRST_CON]->video_enable,
				    mtk_dp->info[DP_SST_ENCODER_PORT].audio_cur_cfg.detect_monitor);
		}
	} else {
		for (con_id = 0; con_id < ARRAY_SIZE(mtk_dp->mtk_con); con_id++) {
			if (mst_con_with_encoder(mtk_dp->mtk_con[con_id])) {
				encoder_id = mtk_dp->mtk_con[con_id]->encoder_id;
				plugged = mtk_dp->mtk_con[con_id]->video_enable &&
					mtk_dp->info[encoder_id].audio_cur_cfg.detect_monitor;
				drm_dbg_kms(mtk_dp->drm_dev,
					    "[DPTX] MST, enc[%d] con[%d]\n",
					    encoder_id, con_id);
				drm_dbg_kms(mtk_dp->drm_dev,
					    "[DPTX] MST, video enable:%d, detect monitor:%d\n",
					    mtk_dp->mtk_con[con_id]->video_enable,
					    mtk_dp->info[encoder_id].audio_cur_cfg.detect_monitor);

				if (plugged)
					break;
			}
		}
	}

	if (mtk_dp->plugged_cb && mtk_dp->codec_dev) {
		drm_dbg_kms(mtk_dp->drm_dev,
			    "[DPTX] audio supported:%d, audio enable:%d, plugged:%d\n",
			    mtk_dp->data->audio_supported, mtk_dp->audio_enable, plugged);
		mtk_dp->plugged_cb(mtk_dp->codec_dev, plugged);
	}
	mutex_unlock(&mtk_dp->update_plugged_status_lock);
}

static void mtk_dp_analog_power_off(struct mtk_dp *mtk_dp)
{
	udelay(10);
	PHY_WRITE_2BYTE(mtk_dp, 0x0034, 0x4aa);
	PHY_WRITE_2BYTE(mtk_dp, 0x1040, 0x0);
	PHY_WRITE_2BYTE(mtk_dp, 0x0038, 0x555);
}

static void mtk_dp_disconnect_release(struct mtk_dp *mtk_dp)
{
	int i;

	if (!mtk_dp->mst_enable) {
		mtk_dp_video_mute(mtk_dp, DP_SST_ENCODER_PORT, true);
		mtk_dp_audio_mute(mtk_dp, DP_SST_ENCODER_PORT, true);
		mtk_dp_video_disable(mtk_dp, DP_SST_ENCODER_PORT);

		if (mtk_dp->mtk_con[DP_FIRST_CON]) {
			kfree(mtk_dp->mtk_con[DP_FIRST_CON]->edid);

			mtk_dp->mtk_con[DP_FIRST_CON]->edid = NULL;
			mtk_dp->mtk_con[DP_FIRST_CON]->video_enable = false;
		}
	}

	mtk_dp_audio_update_plugged_status(mtk_dp);

	mtk_dp_set_idle_pattern(mtk_dp, true);
	mtk_dp_fec_disable(mtk_dp);

	for (i = 0; i < mtk_dp->data->encoder_num; i++)
		mtk_dp_dsc_disable(mtk_dp, i);

	mtk_dp_analog_power_off(mtk_dp);

	mtk_dp_init_variable(mtk_dp);
}

static void mtk_dp_hotplug_uevent(struct mtk_dp *mtk_dp)
{
	if (mtk_dp->drm_dev) {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] notify drm framework hotplug event\n");
		drm_helper_hpd_irq_event(mtk_dp->drm_dev);
	} else {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] there is no drm dev\n");
	}
}

static struct drm_property *mtk_dp_find_property(struct drm_encoder *encoder, u8 *name, int size)
{
	struct drm_property *property, *pt;

	list_for_each_entry_safe(property, pt, &encoder->dev->mode_config.property_list, head) {
		if (property && !strncmp(property->name, name, size))
			return property;
	}

	return NULL;
}

static void mtk_dp_init_property(struct mtk_dp *mtk_dp)
{
	struct drm_bridge *bridge;
	struct drm_property *prop;
	char dsc_enable[] = "dp_dsc_enable";
	char dsc_cfg[] = "dp_dsc_cfg";
	char result[20];
	u8 i;

	if (mtk_dp->init_property)
		return;

	for (i = 0; i < mtk_dp->data->encoder_num; i++) {
		bridge = devm_drm_of_get_bridge(mtk_dp->dev,
						mtk_dp->dev->of_node, i, DP_ENCODER_ENDPOINT);
		if (IS_ERR(bridge)) {
			dev_err(mtk_dp->dev,
				"[DPTX] find encoder, can not find bridge[%d, %d]", i, 0);
			continue;
		}
		if (!bridge->encoder) {
			dev_err(mtk_dp->dev,
				"[DPTX] find encoder, bridge have no encoder[%d, %d]", i, 0);
			continue;
		}
		dev_err(mtk_dp->dev,
			"[DPTX] find encoder, found dp_intf[%d] bridge node:%pOF\n",
			i, bridge->of_node);

		if (snprintf(result, sizeof(result), "%s%d", dsc_enable, i) > 0) {

			prop = mtk_dp_find_property(bridge->encoder,
						    result, strlen(result));

			if (prop)
				mtk_dp->prop_dsc_enable[i] = prop;
			else
				dev_err(mtk_dp->dev,
					"[DPTX] [%d] fail to find property dp_dsc_enable", i);
		}

		if (snprintf(result, sizeof(result), "%s%d", dsc_cfg, i) > 0) {
			prop = mtk_dp_find_property(bridge->encoder,
						    result, strlen(result));
			if (prop)
				mtk_dp->prop_dsc_cfg[i] = prop;
			else
				dev_err(mtk_dp->dev,
					"[DPTX] [%d] fail to find property dp_dsc_cfg", i);
		}
	}

	mtk_dp->init_property = true;
}

static void mtk_dp_check_sink_esi(struct mtk_dp *mtk_dp,
			   const enum dp_encoder_id encoder_id, u8 *dpcd_20x, u8 *dpcd_2002)
{
	u8 tmp;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
	bool handled = false;
#endif

	dev_dbg(mtk_dp->dev, "[DPTX] %s, %d\n", __func__, __LINE__);
#if (DP_AUTO_TEST_ENABLE == 0x1)
	mtk_dp->cts_req.regs = mtk_dp->regs;
	mtk_dp->cts_req.phyd_regs = mtk_dp->phyd_regs;
	mtk_dp->cts_req.aux	= &mtk_dp->aux;
	memcpy(&mtk_dp->cts_req.train_info, &mtk_dp->train_info, sizeof(struct dp_train_info));
	dev_dbg(mtk_dp->dev, "[DPTX] %s, %d\n", __func__, __LINE__);
#endif

	if (dpcd_20x[0x1] & BIT(0)) { /* not support, clrear it. */
		tmp = BIT(0);

		/* DPCD_00201 */
		drm_dp_dpcd_write(&mtk_dp->aux, DP_DEVICE_SERVICE_IRQ_VECTOR, &tmp, 0x1);
	}
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
	if (mtk_dp->is_mst_start) {
		if (dpcd_2002[0x1] & (BIT(4) | BIT(5))) {
			/* BIT(4):DOWN_REP_MSG_RDY; BIT(5): UP_REQ_MSG_RDY */
			mtk_drm_dp_mst_hpd_irq(&mtk_dp->mtk_mgr, dpcd_2002, &handled);
			tmp = (dpcd_2002[0x1] & (BIT(4) | BIT(5)));
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_02003, &tmp, 0x1);
		} else if (dpcd_20x[0x1] & (BIT(4) | BIT(5))) {
			/* BIT(4):DOWN_REP_MSG_RDY; BIT(5): UP_REQ_MSG_RDY */
			mtk_drm_dp_mst_hpd_irq(&mtk_dp->mtk_mgr, dpcd_20x, &handled);
			tmp = (dpcd_20x[0x1] & (BIT(4) | BIT(5)));
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00201, &tmp, 0x1);
		}
	}
#endif
	if (dpcd_20x[0x1] & BIT(0)) { /* not support, clrear it */
		tmp = BIT(0);

		/* DPCD_00201 */
		drm_dp_dpcd_write(&mtk_dp->aux, DP_DEVICE_SERVICE_IRQ_VECTOR, &tmp, 0x1);
	}
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
	if (dpcd_20x[0x4] & BIT(6)) {
		/* DOWNSTREAM_PORT_STATUS_CHANGED */
		if (mtk_dp->training_state > DP_TRAINING_STATE_TRAINING) {
			mtk_dp->training_state = DP_TRAINING_STATE_CHECKCAP;
			DP_MSG("Rx Link Status Change!!\n");
			mtk_dp_mst_drv_video_mute_all(mtk_dp);
		}
	}
#endif
}

static void mtk_dp_hpd_check_sink_event(struct mtk_dp *mtk_dp)
{
	u8 dpcd_20x[6];
	u8 dpcd_2002[2];
	u8 dpcd_200c[4];
	bool ret;

	memset(dpcd_20x, 0x0, sizeof(dpcd_20x));
	memset(dpcd_2002, 0x0, sizeof(dpcd_2002));
	memset(dpcd_200c, 0x0, sizeof(dpcd_200c));

	dev_dbg(mtk_dp->dev, "[DPTX] %s, %d\n", __func__, __LINE__);

	if (mtk_dp->train_info.sink_ext_cap_en) {
		/* DPCD_02002*/
		ret = drm_dp_dpcd_read(&mtk_dp->aux, DP_SINK_COUNT_ESI,
				       dpcd_2002, 0x2);
		if (!ret) {
			dev_dbg(mtk_dp->dev, "[DPTX] Read DPCD_02002 Fail\n");
			return;
		}

		/* DPCD_0200C*/
		ret = drm_dp_dpcd_read(&mtk_dp->aux, DP_LANE0_1_STATUS_ESI,
				       dpcd_200c, 0x4);
		if (!ret) {
			dev_dbg(mtk_dp->dev, "[DPTX] Read DPCD_0200C Fail\n");
			return;
		}
	}

	/* DPCD_00200*/
	ret = drm_dp_dpcd_read(&mtk_dp->aux, DP_SINK_COUNT, dpcd_20x, 0x6);
	if (!ret) {
		dev_dbg(mtk_dp->dev, "[DPTX] Read DPCD200 Fail\n");
		return;
	}

	dev_dbg(mtk_dp->dev, "[DPTX] dpcd_2002[0] 0x%x\n", dpcd_2002[0]);
	dev_dbg(mtk_dp->dev, "[DPTX] dpcd_2002[1] 0x%x\n", dpcd_2002[1]);

	dev_dbg(mtk_dp->dev, "[DPTX] dpcd_200c[0] 0x%x\n", dpcd_200c[0]);
	dev_dbg(mtk_dp->dev, "[DPTX] dpcd_200c[1] 0x%x\n", dpcd_200c[1]);
	dev_dbg(mtk_dp->dev, "[DPTX] dpcd_200c[2] 0x%x\n", dpcd_200c[2]);
	dev_dbg(mtk_dp->dev, "[DPTX] dpcd_200c[3] 0x%x\n", dpcd_200c[3]);

	dev_dbg(mtk_dp->dev, "[DPTX] dpcd_20x[0] 0x%x\n", dpcd_20x[0]);
	dev_dbg(mtk_dp->dev, "[DPTX] dpcd_20x[1] 0x%x\n", dpcd_20x[1]);
	dev_dbg(mtk_dp->dev, "[DPTX] dpcd_20x[2] 0x%x\n", dpcd_20x[2]);
	dev_dbg(mtk_dp->dev, "[DPTX] dpcd_20x[3] 0x%x\n", dpcd_20x[3]);
	dev_dbg(mtk_dp->dev, "[DPTX] dpcd_20x[4] 0x%x\n", dpcd_20x[4]);
	dev_dbg(mtk_dp->dev, "[DPTX] dpcd_20x[5] 0x%x\n", dpcd_20x[5]);

	dev_dbg(mtk_dp->dev, "[DPTX] %s, %d\n", __func__, __LINE__);
	mtk_dp_check_sink_esi(mtk_dp, DP_ENCODER_ID_0, dpcd_20x, dpcd_2002);
	dev_dbg(mtk_dp->dev, "[DPTX] %s, %d\n", __func__, __LINE__);
}

static void mtk_dp_check_device_service_irq(struct mtk_dp *mtk_dp)
{
	u8 val;

	if (mtk_dp->rx_cap[DP_DPCD_REV] < 0x11)
		return;

	/* DPCD_00201*/
	if (drm_dp_dpcd_readb(&mtk_dp->aux,
			      DP_DEVICE_SERVICE_IRQ_VECTOR, &val) != 1 || !val)
		return;

	/* DPCD_00201*/
	drm_dp_dpcd_writeb(&mtk_dp->aux, DP_DEVICE_SERVICE_IRQ_VECTOR, val);

	if (val & DP_AUTOMATED_TEST_REQUEST) {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] DP_AUTOMATED_TEST_REQUEST\n");
		dev_dbg(mtk_dp->dev, "[DPTX] %s, %d\n", __func__, __LINE__);
		mtk_dp_hpd_check_sink_event(mtk_dp);
	}

	if (val & DP_CP_IRQ)
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] DP_CP_IRQ\n");

	if (val & DP_SINK_SPECIFIC_IRQ)
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] Sink specific irq unhandled\n");
}

static bool mtk_dp_check_link_service_irq(struct mtk_dp *mtk_dp)
{
	bool reprobe_needed = false;
	u8 val;

	if (mtk_dp->rx_cap[DP_DPCD_REV] < 0x11)
		return false;

	/* DPCD_2005 */
	if (drm_dp_dpcd_readb(&mtk_dp->aux,
			      DP_LINK_SERVICE_IRQ_VECTOR_ESI0, &val) != 1 || !val)
		return false;

	/* DPCD_2005 */
	if (drm_dp_dpcd_writeb(&mtk_dp->aux,
			       DP_LINK_SERVICE_IRQ_VECTOR_ESI0, val) != 1)
		return reprobe_needed;

	if (val & HDMI_LINK_STATUS_CHANGED)
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] link status changed!\n");

	return reprobe_needed;
}

static bool mtk_dp_link_ok(struct mtk_dp *mtk_dp,
			      u8 link_status[DP_LINK_STATUS_SIZE])
{
	bool uhbr = mtk_dp->train_info.link_rate >= DP_LINK_RATE_UHBR10;
	bool ok;

	if (uhbr)
		ok = drm_dp_128b132b_lane_channel_eq_done(link_status,
							  mtk_dp->train_info.link_lane_count);
	else
		ok = drm_dp_channel_eq_ok(link_status, mtk_dp->train_info.link_lane_count);

	if (ok)
		return true;

	drm_dbg_kms(mtk_dp->drm_dev,
		    "[DPTX] ln0_1:0x%x ln2_3:0x%x align:0x%x sink:0x%x adj_req0_1:0x%x adj_req2_3:0x%x\n",
		    link_status[0], link_status[1], link_status[2],
		    link_status[3], link_status[4], link_status[5]);
	drm_dbg_kms(mtk_dp->drm_dev,
		    "[DPTX] %s link not ok, retraining\n", uhbr ? "128b/132b" : "8b/10b");

	return false;
}

static bool mtk_dp_link_status(struct mtk_dp *mtk_dp)
{
	u8 link_status[DP_LINK_STATUS_SIZE];

	if (!mtk_dp->dp_ready)
		return false;

	if (drm_dp_dpcd_read_phy_link_status(&mtk_dp->aux, DP_PHY_DPRX,
					     link_status) < 0)
		return false;

	/* Retrain if link not ok */
	return mtk_dp_link_ok(mtk_dp, link_status);
}

static bool mtk_dp_hpd_event_handler(struct mtk_dp *mtk_dp)
{
	u8 sink_cnt = 0;
	bool link_ok = true;
	bool reprobe_needed = false;

	sink_cnt = mtk_dp_get_sink_count(mtk_dp);

	if (sink_cnt == 0 || sink_cnt != mtk_dp->train_info.sink_count) {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] sink count change:%d\n", sink_cnt);
		mtk_dp->dp_ready = false;
		mdelay(200);
		return false;
	}

	mtk_dp_check_device_service_irq(mtk_dp);
	reprobe_needed = mtk_dp_check_link_service_irq(mtk_dp);

	if (!mtk_dp_link_status(mtk_dp))
		link_ok = false;

	if (!link_ok) {
		dev_err(mtk_dp->dev, "[DPTX] link fail, training again\n");
		mtk_dp->dp_ready = false;
		return false;
	}

	return !reprobe_needed;
}

static void mtk_dp_wait_for_mux_set_completion(struct mtk_dp *mtk_dp)
{
	unsigned long mux_set_timeout = 0;

	mux_set_timeout = msecs_to_jiffies(MTK_DP_1000_MSECS);
	init_completion(&mux_completion);

	if (!first_ac_on) {
		drm_dbg_kms(mtk_dp->drm_dev,
			    "[DPTX] Wait mux_set to complete\n");

		/* wait mux_set done */
		if (completion_done(&mux_completion))
			drm_dbg_kms(mtk_dp->drm_dev,
				    "[DPTX] mux_set already done\n");
		else {
			if (wait_for_completion_timeout(&mux_completion, mux_set_timeout) == 0)
				drm_dbg_kms(mtk_dp->drm_dev,
					    "[DPTX] mux_set timeout\n");
			else
				drm_dbg_kms(mtk_dp->drm_dev,
					    "[DPTX] mux_set completed\n");
		}
	} else {
		first_ac_on = false;
		drm_dbg_kms(mtk_dp->drm_dev,
			    "[DPTX] first on, don't wait\n");
	}
}

static irqreturn_t mtk_dp_hpd_event_thread(int hpd, void *dev)
{
	struct mtk_dp *mtk_dp = dev;
	bool cable_state_change;
	unsigned long flags;
	u16 phy_status;
	int i;
	bool wait_done[DP_ENCODER_NUM] = {false};
	u8 wait_done_count = 0;
	unsigned long timeout = 0;

	if (mtk_dp->need_debounce && mtk_dp->train_info.cable_plug_in) {
		msleep(MTK_HPD_DEBOUNCE);
		mtk_dp->need_debounce = false;
	}

	spin_lock_irqsave(&mtk_dp->irq_thread_lock, flags);
	cable_state_change = mtk_dp->train_info.cable_state_change;
	phy_status = mtk_dp->train_info.phy_status;
	spin_unlock_irqrestore(&mtk_dp->irq_thread_lock, flags);

	mtk_dp->train_info.cable_state_change = false;
	if (mtk_dp->train_info.phy_status & HPD_INTERRUPT)
		mtk_dp->train_info.phy_status &= ~HPD_INTERRUPT;

	dev_dbg(mtk_dp->dev, "[DPTX] cable_state_change:0x%x, phy_status:0x%x\n",
		    cable_state_change, phy_status);

	if (cable_state_change) {
		if (!mtk_dp->train_info.cable_plug_in) {
			dev_info(mtk_dp->dev, "[DPTX] HPD_DISCON\n");
			mtk_dp_hotplug_uevent(mtk_dp);
			mtk_dp_disconnect_release(mtk_dp);

			/* Wait until crtc of current encoder is disabled */
			timeout = jiffies + msecs_to_jiffies(2000);
			while (time_before(jiffies, timeout)) {
				for (i = 0; i < mtk_dp->data->encoder_num; i++) {
					if (wait_done[i])
						continue;

					if (mtk_dp->mtk_con[i] && mtk_dp->mtk_con[i]->encoder &&
					    mtk_dp->mtk_con[i]->encoder->crtc &&
					    mtk_dp->mtk_con[i]->encoder->crtc->state->enable)
						continue;

					wait_done[i] = true;
					wait_done_count++;
					dev_dbg(mtk_dp->dev,
						"[DPTX] con[%d] crtc is disabled in HPD low\n",
						i);
				}

				if (wait_done_count == mtk_dp->data->encoder_num)
					break;

				msleep(100);
			}

			for (i = 0; i < mtk_dp->data->encoder_num; i++) {
				if (wait_done[i])
					continue;

				dev_err(mtk_dp->dev,
					"[DPTX] connector[%d] crtc is not disabled in HPD low\n",
					i);
			}
		} else {
			dev_info(mtk_dp->dev, "[DPTX] HPD_CON\n");

			if (mtk_dp->data->enable_2c_feature)
				mtk_dp_wait_for_mux_set_completion(mtk_dp);

			mtk_dp_init_property(mtk_dp);
			mtk_dp_initialize_settings(mtk_dp);
			mtk_dp_analog_power_on(mtk_dp);
			mtk_dp_phy_setting(mtk_dp);

			for (i = 0; i < MTK_DP_CHECK_SINK_CAP_TIMEOUT_COUNT; i++) {
				if (mtk_dp_check_sink_cap(mtk_dp))
					break;

				if (!mtk_dp->train_info.cable_plug_in)
					goto end;

				msleep(100);
			}

			mtk_dp_training(mtk_dp);

			mtk_dp_hotplug_uevent(mtk_dp);
		}
	}

	if (phy_status & HPD_INTERRUPT) {
		/*
		 * when the link is unsuccessful (removed or unstable),
		 * returning IRQ_NONE will prevent further processing
		 */
		if (mtk_dp->mst_start)
			return IRQ_NONE;
		else if (!mtk_dp_hpd_event_handler(mtk_dp))
			return IRQ_NONE;

	}

end:
	dev_dbg(mtk_dp->dev, "[DPTX] event thread done\n");
	return IRQ_HANDLED;
}

static irqreturn_t mtk_dp_hpd_event(int hpd, void *dev)
{
	struct mtk_dp *mtk_dp = dev;
	u32 irq_status;
	u16 hw_status;
	unsigned long flags;

	irq_status = mtk_dp_read(mtk_dp, MTK_DP_TOP_IRQ_STATUS);
	if (!irq_status)
		return IRQ_HANDLED;

	if (irq_status & RGS_IRQ_STATUS_ENCODER_1)
		mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_IRQ_MASK_CTRL,
				RGS_IRQ_STATUS_ENCODER_1, RGS_IRQ_STATUS_ENCODER_1);

	if (irq_status & RGS_IRQ_STATUS_ENCODER)
		mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_IRQ_MASK_CTRL,
				RGS_IRQ_STATUS_ENCODER, RGS_IRQ_STATUS_ENCODER);

	if ((irq_status & RGS_IRQ_STATUS_TRANSMITTER) || (irq_status & RGS_IRQ_STATUS_AUXTOP)) {
		if (irq_status & RGS_IRQ_STATUS_TRANSMITTER)
			mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_IRQ_MASK_CTRL,
					RGS_IRQ_STATUS_TRANSMITTER, MTK_DP_TOP_IRQ_MASK_CTRL_MASK);

		spin_lock_irqsave(&mtk_dp->irq_thread_lock, flags);

		hw_status = mtk_dp_hpd_get_irq_status(mtk_dp);
		if (hw_status != 0)
			dev_dbg(mtk_dp->dev, "[DPTX] hw status:0x%x\n", hw_status);

		mtk_dp->train_info.phy_status |= hw_status;

		mtk_dp_hpd_handle_in_isr(mtk_dp);

		if (mtk_dp->train_info.cable_state_change)
			dev_dbg(mtk_dp->dev, "[DPTX] cable_state_change:0x%x, hw_status:%x\n",
				mtk_dp->train_info.cable_state_change, hw_status);

		if (hw_status)
			mtk_dp_hpd_interrupt_clr(mtk_dp, hw_status);

		spin_unlock_irqrestore(&mtk_dp->irq_thread_lock, flags);
	}

	return IRQ_WAKE_THREAD;
}

static void mtk_dp_phy_param_init(struct mtk_dp *mtk_dp, u32 *buffer, u32 size)
{
	u32 i = 0;
	u8  mask = GENMASK(5, 0);

	if (!buffer || size != DP_PHY_REG_COUNT) {
		dev_err(mtk_dp->dev, "[DPTX] invalid param\n");
		return;
	}

	for (i = 0; i < DP_PHY_LEVEL_COUNT; i++) {
		mtk_dp->phy_params[i].c0 = (buffer[i / 4] >> (8 * (i % 4))) & mask;
		mtk_dp->phy_params[i].cp1 = (buffer[i / 4 + 3] >> (8 * (i % 4))) & mask;
	}
}

static int mtk_dp_dt_parse(struct mtk_dp *mtk_dp,
			   struct platform_device *pdev)
{
	struct resource regs;
	struct device *dev = &pdev->dev;
	int ret = 0;
	u32 phy_params_int[DP_PHY_REG_COUNT] = {
		0x20181410, 0x20241e18, 0x00003028,
		0x10080400, 0x000c0600, 0x00000008
	};
	u32 phy_params_dts[DP_PHY_REG_COUNT];

	if (of_address_to_resource(dev->of_node, 0, &regs) != 0)
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] Missing reg[0] in %s node\n",
			    dev->of_node->full_name);

	if (of_address_to_resource(dev->of_node, 1, &regs) != 0)
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] Missing reg[1] in %s node\n",
			    dev->of_node->full_name);

	mtk_dp->regs = of_iomap(dev->of_node, 0);
	mtk_dp->phyd_regs = of_iomap(dev->of_node, 1);
	mtk_dp->phy_mux_regs = of_iomap(dev->of_node, 2);
	mtk_dp->mac_power_regs = of_iomap(dev->of_node, 3);

	ret = of_property_read_u32_array(dev->of_node, "dptx,phy_params",
					 phy_params_dts, ARRAY_SIZE(phy_params_dts));
	if (ret) {
		drm_dbg_kms(mtk_dp->drm_dev,
			    "[DPTX] get phy_params fail, use default val, ret:%d\n", ret);
		mtk_dp_phy_param_init(mtk_dp,
				      phy_params_int, ARRAY_SIZE(phy_params_int));
	} else {
		mtk_dp_phy_param_init(mtk_dp,
				      phy_params_dts, ARRAY_SIZE(phy_params_dts));
	}

	return 0;
}

static u8 dp_aux_write_bytes(struct mtk_dp *mtk_dp,
			     u8 cmd, u64  dpcd_addr, size_t length, u8 *data)
{
	bool vaild_cmd = false;
	u8 reply_cmd = 0x0;
	u8 aux_irq_status;
	u8 phy_status = 0x00;
	u8 i, ret = AUX_HW_FAILED;
	u16 wait_reply_count = AUX_WAIT_REPLY_LP_CNT_NUM;
	u8 reg_index;

	if (length > 16 || (cmd == AUX_CMD_NATIVE_W && length == 0x0))
		return AUX_INVALID_CMD;

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3704,
			1 << AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0_FLDMASK_POS,
			AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0);
	WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3650 + 1, 0x01);
	WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3640, 0x7F);
	usleep_range(AUX_WRITE_READ_WAIT_TIME, AUX_WRITE_READ_WAIT_TIME + 1);

	WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3650 + 1, 0x01);
	WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3644, cmd);
	WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3648, dpcd_addr & GENMASK(7, 0));
	WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3648 + 1, (dpcd_addr >> 8) & GENMASK(7, 0));
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_364C,
			dpcd_addr >> 16,
			MCU_REQUEST_ADDRESS_MSB_AUX_TX_P0_MASK);

	if (length > 0) {
		WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_362C, 0x00);
		for (i = 0x0; i < (length + 1) / 2; i++)
			for (reg_index = 0; reg_index < 2; reg_index++)
				if ((i * 2 + reg_index) < length)
					WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3708 + i * 4 + reg_index,
						   data[i * 2 + reg_index]);
		WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3650 + 1, ((length - 1) & GENMASK(3, 0)) << 4);
	} else {
		WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_362C, 0x01);
	}

	WRITE_BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3704,
			AUX_TX_FIFO_WDATA_NEW_MODE_T_AUX_TX_P0_MASK,
			AUX_TX_FIFO_WDATA_NEW_MODE_T_AUX_TX_P0_MASK);
	WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3630, 0x08);

	while (--wait_reply_count) {
		aux_irq_status = READ_BYTE(mtk_dp, MTK_DP_AUX_P0_3640);

		if (aux_irq_status & AUX_RX_AUX_RECV_COMPLETE_IRQ_AUX_TX_P0) {
			dev_dbg(mtk_dp->dev, "[AUX] Write Complete irq\n");
			vaild_cmd = true;
			break;
		}

		if (aux_irq_status & AUX_400US_TIMEOUT_IRQ_AUX_TX_P0) {
			/* for no reply should wait at least 3200 us */
			usleep_range(AUX_NO_REPLY_WAIT_TIME, AUX_NO_REPLY_WAIT_TIME + 1);
			dev_dbg(mtk_dp->dev, "[DPTX] (AUX write)HW Timeout 400us irq");
			break;
		}
		udelay(1);
	}

	if (wait_reply_count == 0x0) {
		phy_status = READ_BYTE(mtk_dp, MTK_DP_AUX_P0_3628);
		if (phy_status != 0x01)
			dev_err(mtk_dp->dev, "[DPTX] Aux Write:Aux hang, need SW reset!\n");

		WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3650 + 1, 0x01);
		WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3640, 0x7F);

		dev_dbg(mtk_dp->dev, "[DPTX] reply_cmd:0x%x, wait_reply_count:%d\n",
			    reply_cmd, wait_reply_count);
		return AUX_HW_FAILED;
	}

	reply_cmd = READ_BYTE(mtk_dp, MTK_DP_AUX_P0_3624) & GENMASK(3, 0);
	if (reply_cmd)
		dev_dbg(mtk_dp->dev, "[DPTX] reply_cmd:%x, NACK or Defer\n", reply_cmd);

	WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3650 + 1, 0x01);

	if (length == 0)
		WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_362C, 0x00);

	WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3640, 0x7F);

	if (vaild_cmd) {
		dev_dbg(mtk_dp->dev, "[AUX] Write reply_cmd:%d\n", reply_cmd);
		ret = reply_cmd;
	} else {
		dev_dbg(mtk_dp->dev,
			    "[DPTX] [AUX] Timeout, Write reply_cmd:%d\n", reply_cmd);
		ret = AUX_HW_FAILED;
	}

	return ret;
}

static bool mtk_dp_aux_write_bytes(struct mtk_dp *mtk_dp, u8 cmd,
				   u32 dpcd_addr, size_t length, u8 *data)
{
	u8 reply_status, retry_limit = 8;

	if (!mtk_dp->train_info.cable_plug_in)
		return false;

	while (--retry_limit) {
		reply_status = dp_aux_write_bytes(mtk_dp, cmd, dpcd_addr,
						  length, data);
		if (reply_status == AUX_REPLY_ACK)
			return true;

		usleep_range(50, 51);
		dev_dbg(mtk_dp->dev, "[DPTX] Remaining retries: %u\n", retry_limit);
	}

	dev_err(mtk_dp->dev, "[DPTX] Aux Write Fail, cmd:%d, addr:0x%x, len:%zu\n",
		cmd, dpcd_addr, length);

	return false;
}

static bool mtk_dp_aux_write_dpcd(struct mtk_dp *mtk_dp, u8 cmd,
				  u32 dpcd_addr, size_t length, u8 *data)
{
	bool ret = true;
	size_t i;

	for (i = 0; i + DP_AUX_MAX_PAYLOAD_BYTES <= length; i += DP_AUX_MAX_PAYLOAD_BYTES) {
		ret &= mtk_dp_aux_write_bytes(mtk_dp, cmd, dpcd_addr + i,
					      DP_AUX_MAX_PAYLOAD_BYTES,
					      data + i);
	}

	if (length % DP_AUX_MAX_PAYLOAD_BYTES) {
		ret &= mtk_dp_aux_write_bytes(mtk_dp, cmd, dpcd_addr + i,
					      length % DP_AUX_MAX_PAYLOAD_BYTES,
					      data + i);
	}

	dev_dbg(mtk_dp->dev, "Aux write cmd:%d, addr:0x%x, len:%zu, %s\n",
		cmd, dpcd_addr, length, ret ? "Success" : "Fail");

	for (i = 0; i < length; i++)
		dev_dbg(mtk_dp->dev, "DPCD%zx:0x%x", dpcd_addr + i, data[i]);

	return ret;
}

static u8 dp_aux_read_bytes(struct mtk_dp *mtk_dp, u8 cmd,
			    u64  dpcd_addr, size_t length, u8 *rx_buf)
{
	bool vaild_cmd = false;
	u8 phy_status = 0x00;
	u8 reply_cmd = 0xff;
	u8 rd_count = 0x0;
	u8 aux_irq_status = 0;
	u8 ret = AUX_HW_FAILED;
	unsigned int wait_reply_count = AUX_WAIT_REPLY_LP_CNT_NUM;

	WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3640, 0x7f);
	usleep_range(AUX_WRITE_READ_WAIT_TIME, AUX_WRITE_READ_WAIT_TIME + 1);

	if (length > 16 ||
	    (cmd == AUX_CMD_NATIVE_R && length == 0x0))
		return AUX_INVALID_CMD;

	WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3650 + 1, 0x01);
	WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3644, cmd);
	WRITE_2BYTE(mtk_dp, MTK_DP_AUX_P0_3648, dpcd_addr & GENMASK(15, 0));
	WRITE_BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_364C,
			dpcd_addr >> 16,
			MCU_REQUEST_ADDRESS_MSB_AUX_TX_P0_MASK);

	if (length > 0) {
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3650,
				 (length - 1) << MCU_REQ_DATA_NUM_AUX_TX_P0_FLDMASK_POS,
				 MCU_REQ_DATA_NUM_AUX_TX_P0_MASK);
		WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_362C, 0x00);
	}

	if (cmd == AUX_CMD_I2C_R || cmd == AUX_CMD_I2C_R_MOT0)
		if (length == 0x0)
			WRITE_2BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_362C,
					 0x01 << AUX_NO_LENGTH_AUX_TX_P0_FLDMASK_POS,
					 AUX_NO_LENGTH_AUX_TX_P0);

	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3630,
			 0x01 << AUX_TX_REQUEST_READY_AUX_TX_P0_FLDMASK_POS,
			 AUX_TX_REQUEST_READY_AUX_TX_P0);

	while (--wait_reply_count) {
		aux_irq_status = READ_BYTE(mtk_dp, MTK_DP_AUX_P0_3640);

		if (aux_irq_status & AUX_RX_AUX_RECV_COMPLETE_IRQ_AUX_TX_P0) {
			dev_dbg(mtk_dp->dev, "[AUX] Read Complete irq\n");
			vaild_cmd = true;
			break;
		}

		if (aux_irq_status & AUX_RX_EDID_RECV_COMPLETE_IRQ_AUX_TX_P0) {
			vaild_cmd = true;
			break;
		}

		if (aux_irq_status & AUX_400US_TIMEOUT_IRQ_AUX_TX_P0) {
			/* for no reply should wait at least 3200 us */
			usleep_range(AUX_NO_REPLY_WAIT_TIME, AUX_NO_REPLY_WAIT_TIME + 1);
			dev_dbg(mtk_dp->dev, "[DPTX] (AUX Read)HW Timeout 400us irq");
			break;
		}
		udelay(1);
	}

	if (wait_reply_count == 0x0) {
		phy_status = READ_BYTE(mtk_dp, MTK_DP_AUX_P0_3628);
		if (phy_status != 0x01)
			dev_err(mtk_dp->dev, "[DPTX] Aux R:Aux hang, need SW reset\n");

		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3650,
				 0x01 << MCU_ACK_TRAN_COMPLETE_AUX_TX_P0_FLDMASK_POS,
				 MCU_ACK_TRAN_COMPLETE_AUX_TX_P0);
		WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3640, 0x7F);

		dev_dbg(mtk_dp->dev,
			    "[DPTX] wait_reply_count:%x, TimeOut", wait_reply_count);
		return AUX_HW_FAILED;
	}

	reply_cmd = READ_BYTE(mtk_dp, MTK_DP_AUX_P0_3624) & GENMASK(3, 0);
	if (reply_cmd)
		dev_dbg(mtk_dp->dev, "[DPTX] reply_cmd:%x, NACK or Defer\n", reply_cmd);

	if (length == 0)
		WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_362C, 0x00);

	if (reply_cmd == AUX_REPLY_ACK) {
		WRITE_2BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3620,
				 0x0 << AUX_RD_MODE_AUX_TX_P0_FLDMASK_POS,
				 AUX_RD_MODE_AUX_TX_P0_MASK);

		for (rd_count = 0x0; rd_count < length;
				rd_count++) {
			WRITE_2BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3620,
					 0x01 << AUX_RX_FIFO_READ_PULSE_AUX_TX_P0_FLDMASK_POS,
					 AUX_RX_FIFO_READ_PULSE_TX_P0);

			*(rx_buf + rd_count) = READ_BYTE(mtk_dp, MTK_DP_AUX_P0_3620);
		}
	}

	WRITE_2BYTE_MASK(mtk_dp, MTK_DP_AUX_P0_3650,
			 0x01 << MCU_ACK_TRAN_COMPLETE_AUX_TX_P0_FLDMASK_POS,
			 MCU_ACK_TRAN_COMPLETE_AUX_TX_P0);
	WRITE_BYTE(mtk_dp, MTK_DP_AUX_P0_3640, 0x7F);

	if (vaild_cmd) {
		dev_dbg(mtk_dp->dev, "[AUX] Read reply_cmd:%d\n", reply_cmd);
		ret = reply_cmd;
	} else {
		dev_dbg(mtk_dp->dev, "[DPTX] [AUX] Timeout Read reply_cmd:%d\n", reply_cmd);
		ret = AUX_HW_FAILED;
	}

	return ret;
}

static bool mtk_dp_aux_read_bytes(struct mtk_dp *mtk_dp, u8 cmd,
				  u32 dpcd_addr, size_t length, u8 *data)
{
	u8 reply_status, retry_limit = 8;

	if (!mtk_dp->train_info.cable_plug_in)
		return false;

	while (--retry_limit) {
		reply_status = dp_aux_read_bytes(mtk_dp, cmd, dpcd_addr,
						 length, data);
		if (reply_status == AUX_REPLY_ACK)
			return true;

		usleep_range(50, 51);
		dev_dbg(mtk_dp->dev, "[DPTX] Remaining retries: %u\n", retry_limit);
	}

	dev_info(mtk_dp->dev, "[DPTX] Aux Read Fail, cmd:%d, addr:0x%x, len:%zu\n",
		cmd, dpcd_addr, length);

	return false;
}

static bool mtk_dp_aux_read_dpcd(struct mtk_dp *mtk_dp, u8 cmd,
				 u32 dpcd_addr, size_t length, u8 *data)
{
	bool ret = true;
	size_t i;

	memset(data, 0, length);

	for (i = 0; i + DP_AUX_MAX_PAYLOAD_BYTES <= length; i += DP_AUX_MAX_PAYLOAD_BYTES) {
		ret &= mtk_dp_aux_read_bytes(mtk_dp, cmd, dpcd_addr + i,
					     DP_AUX_MAX_PAYLOAD_BYTES,
					     data + i);
	}

	if (length % DP_AUX_MAX_PAYLOAD_BYTES) {
		ret &= mtk_dp_aux_read_bytes(mtk_dp, cmd, dpcd_addr + i,
					     length % DP_AUX_MAX_PAYLOAD_BYTES,
					     data + i);
	}

	dev_dbg(mtk_dp->dev, "Aux Read cmd:%d, addr:0x%x, len:%zu, %s\n",
		cmd, dpcd_addr, length, ret ? "Success" : "Fail");

	for (i = 0; i < length; i++)
		dev_dbg(mtk_dp->dev, "DPCD%zx:0x%x", dpcd_addr + i, data[i]);

	return ret;
}

static ssize_t mtk_dp_aux_transfer(struct drm_dp_aux *mtk_aux,
				   struct drm_dp_aux_msg *msg)
{
	u8 cmd;
	void *data;
	size_t length, ret = 0;
	u32 addr;
	bool ack = false;
	struct mtk_dp *mtk_dp;

	mtk_dp = container_of(mtk_aux, struct mtk_dp, aux);
	cmd = msg->request;
	addr = msg->address;
	length = msg->size;
	data = msg->buffer;

	dev_dbg(mtk_dp->dev, "msg->addr = %d, msg->size = %zu\n", msg->address, msg->size);

	if (mtk_dp->disp_state == DP_DISP_STATE_SUSPENDING ||
	    mtk_dp->disp_state == DP_DISP_STATE_SUSPEND ||
	    !mtk_dp->train_info.cable_plug_in) {
		msg->reply = DP_AUX_NATIVE_REPLY_NACK | DP_AUX_I2C_REPLY_NACK;
		return -EIO;
	}

	switch (cmd) {
	case DP_AUX_I2C_MOT:
	case DP_AUX_I2C_WRITE:
	case DP_AUX_NATIVE_WRITE:
	case DP_AUX_I2C_WRITE_STATUS_UPDATE:
	case DP_AUX_I2C_WRITE_STATUS_UPDATE | DP_AUX_I2C_MOT:
		cmd &= ~DP_AUX_I2C_WRITE_STATUS_UPDATE;
		ack = mtk_dp_aux_write_dpcd(mtk_dp, cmd,
					    addr, length, data);
		break;

	case DP_AUX_I2C_READ:
	case DP_AUX_NATIVE_READ:
	case DP_AUX_I2C_READ | DP_AUX_I2C_MOT:
		ack = mtk_dp_aux_read_dpcd(mtk_dp, cmd,
					   addr, length, data);
		break;

	default:
		dev_err(mtk_dp->dev, "[DPTX] invalid aux cmd:%d\n", cmd);
		ret = -EINVAL;
		break;
	}

	if (ack) {
		msg->reply = DP_AUX_NATIVE_REPLY_ACK | DP_AUX_I2C_REPLY_ACK;
		ret = length;
	} else {
		msg->reply = DP_AUX_NATIVE_REPLY_NACK | DP_AUX_I2C_REPLY_NACK;
		ret = -EAGAIN;
	}

	return ret;
}

static int mtk_dp_con_get_modes(struct drm_connector *connector)
{
	struct mtk_dp *mtk_dp;
	struct mtk_dp_con *mtk_con;
	int ret, num_modes = 0;
	struct mtk_dp_audio_cfg *audio_caps;
	struct cea_sad *sads;
	int encoder_id;
	unsigned long timeout = 0;

	mtk_con = container_of(connector, struct mtk_dp_con, connector);
	mtk_dp = mtk_con->mtk_dp;

	if (!mtk_con->edid) {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] get edid\n");
		timeout = jiffies + msecs_to_jiffies(2000);
		while (time_before(jiffies, timeout)) {
			mtk_con->edid = drm_get_edid(connector, &mtk_dp->aux.ddc);
			if (mtk_con->edid)
				break;

			msleep(100);
		}

		if (!mtk_con->edid) {
			dev_err(mtk_dp->dev, "[DPTX] Failed to read EDID\n");
			goto fail;
		}
	}

	/* audio caps */
	encoder_id = mtk_con->encoder_id;
	audio_caps = &mtk_dp->info[encoder_id].audio_cur_cfg;
	audio_caps->sad_count = drm_edid_to_sad(mtk_con->edid, &sads);
	kfree(sads);
	audio_caps->detect_monitor = drm_detect_monitor_audio(mtk_con->edid);

	ret = drm_connector_update_edid_property(&mtk_con->connector, mtk_con->edid);
	if (ret) {
		dev_err(mtk_dp->dev, "[DPTX] Failed to update EDID property: %d\n", ret);
		goto fail;
	}

	num_modes = drm_add_edid_modes(&mtk_con->connector, mtk_con->edid);

fail:
	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] connector[%d] SST modes:%d\n",
		    mtk_dp_con_id(mtk_dp, mtk_con), num_modes);
	return num_modes;
}

static u32 mtk_dp_dsc_cal_clock(struct drm_display_mode *mode)
{
	u16 hblank;
	u16 hactive;
	u64 htotal;
	u64 mode_htotal;
	u32 pixel_clock;

	hblank = mode->htotal - mode->hdisplay;
	hactive = ((mode->hdisplay * DP_DSC_BPP + (12 * 8 - 1)) / (12 * 8)) * 4;
	htotal = hblank + hactive;
	mode_htotal =  mode->htotal;
	pixel_clock = div_u64(mode->clock * htotal, mode_htotal);

	return pixel_clock;
}

static enum drm_mode_status mtk_dp_check_mode(struct mtk_dp *mtk_dp,
					      struct drm_display_mode *mode, int bpp, bool *dsc)
{
	u32 rate;
	u32 pixel_clock;
	u8 color_bpp;
	enum drm_mode_status ret = MODE_CLOCK_HIGH;

	if ((mtk_dp->data->max_hdisplay != 0 && mode->hdisplay > mtk_dp->data->max_hdisplay) ||
	    (mtk_dp->data->max_vdisplay != 0 && mode->vdisplay > mtk_dp->data->max_vdisplay))
		return MODE_BAD;

	/* This is for temporarily removing the timing. */
	if (mode->hdisplay < mtk_dp->data->min_hdisplay ||
	    mode->vdisplay < mtk_dp->data->min_vdisplay)
		return MODE_BAD;

	/* This is for temporarily removing the 3840x2160@60 clock = 533000 timing. */
	if (mode->clock > 400000)
		return MODE_BAD;

	*dsc = false;

	rate = drm_dp_bw_code_to_link_rate(mtk_dp->train_info.link_rate) *
		mtk_dp->train_info.link_lane_count;
	rate = rate * 97 / 100;

	if ((mode->clock * bpp / 8) < rate) {
		*dsc = false;
		ret = MODE_OK;
		goto end;
	}

	if (mtk_dp->data->dsc_support && mtk_dp->mtk_con[DP_FIRST_CON] &&
		drm_dp_sink_supports_dsc(mtk_dp->mtk_con[DP_FIRST_CON]->dsc_dpcd) &&
		drm_dp_sink_supports_fec(mtk_dp->mtk_con[DP_FIRST_CON]->fec_cap)) {
		pixel_clock = mtk_dp_dsc_cal_clock(mode);
		color_bpp = mtk_dp_color_get_bpp(DP_PIXELFORMAT_RGB, DP_COLOR_DEPTH_8BIT);
		if ((pixel_clock * color_bpp / 8) < rate) {
			*dsc = true;
			ret = MODE_OK;
		}
	}

end:
	return ret;
}

static enum drm_mode_status mtk_dp_con_mode_valid(struct drm_connector *connector,
						  struct drm_display_mode *mode)
{
	struct mtk_dp_con *mtk_con;
	struct mtk_dp *mtk_dp;
	enum drm_mode_status mode_status;
	bool dsc = false;
	u8 bpp = 24;

	mtk_con = container_of(connector, struct mtk_dp_con, connector);
	mtk_dp = mtk_con->mtk_dp;

	bpp = connector->display_info.color_formats & DRM_COLOR_FORMAT_YCBCR422 ? 16 : 24;
	mode_status = mtk_dp_check_mode(mtk_dp, mode, bpp, &dsc);

	drm_dbg_kms(mtk_dp->drm_dev,
		    "[DPTX] connector[%d] SST mode valid status:%d, dsc:%d, Htt:%d, Vtt:%d\n",
		    mtk_dp_con_id(mtk_dp, mtk_con), mode_status, dsc,
		    mode->htotal, mode->vtotal);
	drm_dbg_kms(mtk_dp->drm_dev,
		    "[DPTX] connector[%d] Hact:%d, Vact:%d, fps:%d, clk:%d, YCBCR422:%d\n",
		    mtk_dp_con_id(mtk_dp, mtk_con),
		    mode->hdisplay, mode->vdisplay,
		    drm_mode_vrefresh(mode), mode->clock,
		    connector->display_info.color_formats & DRM_COLOR_FORMAT_YCBCR422);

	return mode_status;
}

static const struct drm_connector_helper_funcs mtk_dp_con_helper_funcs = {
	.get_modes = mtk_dp_con_get_modes,
	.mode_valid = mtk_dp_con_mode_valid,
};

static enum drm_connector_status mtk_dp_con_detect
	(struct drm_connector *connector, bool force)
{
	struct mtk_dp *mtk_dp;
	struct mtk_dp_con *mtk_con;
	enum drm_connector_status ret = connector_status_disconnected;

	mtk_con = container_of(connector, struct mtk_dp_con, connector);
	mtk_dp = mtk_con->mtk_dp;

	if (!mtk_dp->train_info.cable_plug_in ||
	    !mtk_dp->dp_ready)
		goto end;

	if (mtk_dp->train_info.sink_count)
		ret = connector_status_connected;

end:
	drm_dbg_kms(mtk_dp->drm_dev,
		    "[DPTX] connector[%d], plug in:%d, sink count:%d, mst:%d, detect:%d",
		    mtk_dp_con_id(mtk_dp, mtk_con),
		    mtk_dp->train_info.cable_plug_in,
		    mtk_dp->train_info.sink_count,
		    mtk_dp->mst_enable, ret);
	return ret;
}

static void mtk_dp_con_destroy(struct drm_connector *connector)
{
	struct mtk_dp_con *mtk_con = container_of(connector, struct mtk_dp_con, connector);
	struct mtk_dp *mtk_dp = mtk_con->mtk_dp;
	int id =  mtk_dp_con_id(mtk_dp, mtk_con);

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] connector[%d] destroy\n", id);

	if (id < 0)
		return;

	drm_connector_cleanup(connector);

	kfree(mtk_con);
	mtk_dp->mtk_con[id] = NULL;
}

static int mtk_dp_con_late_register(struct drm_connector *connector)
{
	struct mtk_dp *mtk_dp;
	struct mtk_dp_con *mtk_con;

	mtk_con = container_of(connector, struct mtk_dp_con, connector);
	mtk_dp = mtk_con->mtk_dp;

	if (connector->connector_type == DRM_MODE_CONNECTOR_DisplayPort)
		mtk_dp->aux.dev = connector->kdev;

	return 0;
}

static void mtk_dp_con_early_unregister(struct drm_connector *connector)
{
	struct mtk_dp_con *mtk_con;
	struct mtk_dp *mtk_dp;

	mtk_con = container_of(connector, struct mtk_dp_con, connector);
	mtk_dp = mtk_con->mtk_dp;
	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] connector[%d] early unregister\n",
		    mtk_dp_con_id(mtk_dp, mtk_con));
}

static const struct drm_connector_funcs mtk_dp_con_funcs = {
	.reset = drm_atomic_helper_connector_reset,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = mtk_dp_con_detect,
	.destroy = mtk_dp_con_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.late_register = mtk_dp_con_late_register,
	.early_unregister = mtk_dp_con_early_unregister,
};

static struct mtk_dp_con *mtk_dp_create_connector(struct mtk_dp *mtk_dp)
{
	struct mtk_dp_con *mtk_con;
	struct drm_bridge *bridge;
	int ret;

	bridge = devm_drm_of_get_bridge(mtk_dp->dev, mtk_dp->dev->of_node,
					DP_SST_ENCODER_PORT, DP_ENCODER_ENDPOINT);
	if (IS_ERR(bridge)) {
		dev_err(mtk_dp->dev, "[DPTX] create con, can not find bridge[%d, %d]",
			DP_SST_ENCODER_PORT, DP_ENCODER_ENDPOINT);
		return NULL;
	}
	if (!bridge->encoder) {
		dev_err(mtk_dp->dev, "[DPTX] create con, bridge have no encoder[%d, %d]",
			DP_SST_ENCODER_PORT, DP_ENCODER_ENDPOINT);
		return NULL;
	}
	dev_dbg(mtk_dp->dev, "[DPTX] create con, found dp_intf[%d] bridge node:%pOF\n",
		DP_SST_ENCODER_PORT, bridge->of_node);

	mtk_con = kzalloc(sizeof(*mtk_con), GFP_KERNEL);
	if (!mtk_con)
		return NULL;

	mtk_con->mtk_dp = mtk_dp;

	ret = drm_connector_init(mtk_dp->drm_dev, &mtk_con->connector,
				 &mtk_dp_con_funcs, DRM_MODE_CONNECTOR_DisplayPort);
	if (ret) {
		dev_err(mtk_dp->dev,
			"[DPTX] create con, failed to init connector:%d\n", ret);
		kfree(mtk_con);
		return NULL;
	}

	drm_display_info_set_bus_formats(&mtk_con->connector.display_info,
					 mt8196_output_fmts,
					 ARRAY_SIZE(mt8196_output_fmts));

	drm_connector_helper_add(&mtk_con->connector,
				 &mtk_dp_con_helper_funcs);
	mtk_con->connector.polled = DRM_CONNECTOR_POLL_HPD;

	ret = drm_connector_attach_encoder(&mtk_con->connector, bridge->encoder);
	if (ret) {
		dev_err(mtk_dp->dev,
			"[DPTX] create con, failed to attach encoder:%d\n", ret);
		kfree(mtk_con);
		return NULL;
	}

	mtk_con->encoder = bridge->encoder;
	mtk_con->encoder_id = DP_SST_ENCODER_PORT;

	if (mtk_con->connector.funcs->reset)
		mtk_con->connector.funcs->reset(&mtk_con->connector);

	drm_connector_attach_content_protection_property(&mtk_con->connector, true);

	ret = drm_connector_register(&mtk_con->connector);
	if (ret) {
		dev_err(mtk_dp->dev,
			"[DPTX] create con, failed to register connector:%d\n", ret);
		kfree(mtk_con);
		return NULL;
	}

	mtk_dp->mtk_con[DP_FIRST_CON] = mtk_con;
	dev_dbg(mtk_dp->dev, "[DPTX] create con, create mtk connector[%d]\n", DP_FIRST_CON);

	return mtk_dp->mtk_con[DP_FIRST_CON];
}

static int mtk_dp_bridge_attach(struct drm_bridge *bridge,
				enum drm_bridge_attach_flags flags)
{
	struct mtk_dp_bridge *mtk_bridge = container_of(bridge, struct mtk_dp_bridge, bridge);
	struct mtk_dp *mtk_dp = mtk_bridge->mtk_dp;
	int ret;

	drm_dbg_kms(bridge->dev, "[DPTX] bridge[%d] attach, refcount:%u",
		    mtk_bridge->encoder_id, atomic_read(&mtk_dp->refcount));

	/*
	 * In a scenario that supports MST, DP will have more
	 * than one encoder, with each encoder corresponding
	 * to a bridge. DP initialization is performed only
	 * after all bridges are attached.
	 */
	if (atomic_inc_return(&mtk_dp->refcount) != mtk_dp->data->encoder_num)
		return 0;

	mtk_dp->drm_dev = bridge->dev;

	mtk_dp->aux.drm_dev = bridge->dev;
	ret = drm_dp_aux_register(&mtk_dp->aux);
	if (ret) {
		dev_err(mtk_dp->dev, "[DPTX] failed to register DP AUX channel:%d\n", ret);
		return ret;
	}

	mtk_dp_create_connector(mtk_dp);

	enable_irq(mtk_dp->irq);
	mtk_dp_hpd_interrupt_enable(mtk_dp, true);

	return 0;
}

bool mtk_dp_parse_audio_cap(struct mtk_dp *mtk_dp, struct mtk_dp_audio_cfg *cfg)
{
	if (!mtk_dp->data->audio_supported)
		return false;

	if (cfg->sad_count <= 0) {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] The SADs is NULL\n");
		return false;
	}

	return true;
}

static void mtk_dp_resouce_free(struct mtk_dp *mtk_dp)
{
	mtk_dp_hpd_interrupt_enable(mtk_dp, false);
	disable_irq(mtk_dp->irq);

	mtk_dp_disconnect_release(mtk_dp);

	mtk_dp->drm_dev = NULL;
	drm_dp_aux_unregister(&mtk_dp->aux);
}

static void mtk_dp_bridge_detach(struct drm_bridge *bridge)
{
	struct mtk_dp_bridge *mtk_bridge = container_of(bridge, struct mtk_dp_bridge, bridge);
	struct mtk_dp *mtk_dp = mtk_bridge->mtk_dp;

	if (atomic_dec_and_test(&mtk_dp->refcount))
		mtk_dp_resouce_free(mtk_dp);
}

static void mtk_dp_bridge_atomic_enable(struct drm_bridge *bridge,
					struct drm_bridge_state *old_state)
{
	struct mtk_dp_bridge *mtk_bridge = container_of(bridge, struct mtk_dp_bridge, bridge);
	enum dp_encoder_id id = mtk_bridge->encoder_id;
	struct mtk_dp *mtk_dp = mtk_bridge->mtk_dp;
	struct drm_atomic_state *state = old_state->base.state;
	int con_id;

	con_id = encoder_id_to_con_id(mtk_dp, id);
	if (con_id < 0)
		return;

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] bridge[%d] SST enable", id);

	if (!mtk_dp->train_info.cable_plug_in || !mtk_dp->dp_ready) {
		dev_err(mtk_dp->dev, "[DPTX] bridge[%d] SST cable_plug_in:%d, dp_ready:%d",
			id, mtk_dp->train_info.cable_plug_in, mtk_dp->dp_ready);
		return;
	}

	if (drm_dp_sink_supports_fec(mtk_dp->mtk_con[con_id]->fec_cap))
		mtk_dp_fec_enable(mtk_dp);

	mtk_dp_video_mute(mtk_dp, id, true);
	mtk_dp_video_enable(mtk_dp, id);
	mtk_dp_video_mute(mtk_dp, id, false);

	mtk_dp->mtk_con[con_id]->video_enable = true;

	/* audio */
	mtk_dp->audio_enable =
		mtk_dp_parse_audio_cap(mtk_dp,
				       &mtk_dp->info[id].audio_cur_cfg);

	if (mtk_dp->audio_enable) {
		mtk_dp_audio_mute(mtk_dp, id, true);
		mtk_dp_audio_config(mtk_dp, id);
		mtk_dp_audio_mute(mtk_dp, id, false);
	} else {
		memset(&mtk_dp->info[id].audio_cur_cfg, 0,
		       sizeof(mtk_dp->info[id].audio_cur_cfg));
	}

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] bridge[%d] SST pattern_gen:%d, audio_enable:%d\n",
		    id, mtk_dp->info[id].pattern_gen, mtk_dp->audio_enable);

	mtk_dp_audio_update_plugged_status(mtk_dp);
}

static void mtk_dp_bridge_atomic_disable(struct drm_bridge *bridge,
					 struct drm_bridge_state *old_state)
{
	struct mtk_dp_bridge *mtk_bridge = container_of(bridge, struct mtk_dp_bridge, bridge);
	enum dp_encoder_id id = mtk_bridge->encoder_id;
	struct mtk_dp *mtk_dp = mtk_bridge->mtk_dp;
	struct drm_atomic_state *state = old_state->base.state;
	int con_id;

	con_id = encoder_id_to_con_id(mtk_dp, id);
	if (con_id < 0)
		return;

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] bridge[%d] SST disable", id);

	mtk_dp_video_mute(mtk_dp, id, true);
	mtk_dp_audio_mute(mtk_dp, id, true);
	mtk_dp_video_disable(mtk_dp, id);

	mtk_dp->mtk_con[con_id]->video_enable = false;

	mtk_dp_audio_update_plugged_status(mtk_dp);
}

static u32 *mtk_dp_bridge_atomic_get_output_bus_fmts(struct drm_bridge *bridge,
						     struct drm_bridge_state *bridge_state,
						     struct drm_crtc_state *crtc_state,
						     struct drm_connector_state *conn_state,
						     unsigned int *num_output_fmts)
{
	u32 *output_fmts;

	*num_output_fmts = 0;
	output_fmts = kmalloc(sizeof(*output_fmts), GFP_KERNEL);
	if (!output_fmts)
		return NULL;
	*num_output_fmts = 1;
	output_fmts[0] = MEDIA_BUS_FMT_FIXED;
	return output_fmts;
}

static const u32 mt8195_input_fmts[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUV8_1X24,
	MEDIA_BUS_FMT_YUYV8_1X16,
};

static u32 *mtk_dp_bridge_atomic_get_input_bus_fmts(struct drm_bridge *bridge,
						    struct drm_bridge_state *bridge_state,
						    struct drm_crtc_state *crtc_state,
						    struct drm_connector_state *conn_state,
						    u32 output_fmt,
						    unsigned int *num_input_fmts)
{
	u32 *input_fmts;
	bool dsc = false, use_platform_format = false;
	u8 bpp;
	bool support_422 = false;
	struct mtk_dp_bridge *mtk_bridge = container_of(bridge, struct mtk_dp_bridge, bridge);
	struct mtk_dp *mtk_dp = mtk_bridge->mtk_dp;
	struct drm_display_mode *mode = &crtc_state->adjusted_mode;
	u32 lane_count_min = mtk_dp->train_info.link_lane_count;
	u32 rate = drm_dp_bw_code_to_link_rate(mtk_dp->train_info.link_rate) * lane_count_min;
	int fmt = MEDIA_BUS_FMT_RGB888_1X24; /* default format: RGB888 */

	if (mtk_dp->mst_enable)
		goto set_fmts;

	support_422 = conn_state->connector->display_info.color_formats &
		DRM_COLOR_FORMAT_YCBCR422 ? true : false;
	bpp = support_422 ? 16 : 24;
	mtk_dp_check_mode(mtk_dp, mode, bpp, &dsc);
	if (dsc)
		goto set_fmts;

	/* SSC + FEC would occupy 3% */
	rate = rate * 97 / 100;
	if (rate > (mode->clock * 24 / 8))
		use_platform_format = true;
	else if (support_422 && rate > (mode->clock * 16 / 8))
		fmt = MEDIA_BUS_FMT_YUYV8_1X16;
	else
		goto end;

set_fmts:
	*num_input_fmts = use_platform_format ? ARRAY_SIZE(mt8196_input_fmts) : 1;
	input_fmts = kcalloc(*num_input_fmts, sizeof(*input_fmts), GFP_KERNEL);
	if (!input_fmts)
		return NULL;

	if (use_platform_format)
		memcpy(input_fmts, mt8196_input_fmts, sizeof(mt8196_input_fmts));
	else
		input_fmts[0] = fmt;

	drm_dbg_kms(mtk_dp->drm_dev,
		    "[DPTX] bridge[%d] input fmts, fmt:%s, dsc:%d, Htt:%d, Vtt:%d\n",
		    mtk_bridge->encoder_id,
		    (!use_platform_format) ?
		    (input_fmts[0] == MEDIA_BUS_FMT_YUYV8_1X16) ?
			"MEDIA_BUS_FMT_YUYV8_1X16" : "MEDIA_BUS_FMT_RGB888_1X24" :
		    "mt8196_input_fmts",
		    dsc, mode->htotal, mode->vtotal);
	drm_dbg_kms(mtk_dp->drm_dev,
		    "[DPTX] bridge[%d] Hact:%d, Vact:%d, fps:%d, clk:%d\n",
		    mtk_bridge->encoder_id,
		    mode->hdisplay, mode->vdisplay,
		    drm_mode_vrefresh(mode), mode->clock);
	return input_fmts;

end:
	*num_input_fmts = 0;
	dev_err(mtk_dp->dev, "[DPTX] bridge[%d] get_input_bus_fmts, return NULL",
		mtk_bridge->encoder_id);
	return NULL;
}

static int mtk_dp_bridge_atomic_check(struct drm_bridge *bridge,
				      struct drm_bridge_state *bridge_state,
				      struct drm_crtc_state *crtc_state,
				      struct drm_connector_state *conn_state)
{
	struct mtk_dp_bridge *mtk_bridge = container_of(bridge, struct mtk_dp_bridge, bridge);
	enum dp_encoder_id id = mtk_bridge->encoder_id;
	struct mtk_dp *mtk_dp = mtk_bridge->mtk_dp;
	bool dsc = false;
	u8 bpp;

	if (!conn_state->crtc) {
		dev_err(mtk_dp->dev,
			"[DPTX] Can't enable bridge as connector state doesn't have a crtc\n");
		return -EINVAL;
	}

	if (bridge_state->input_bus_cfg.format == MEDIA_BUS_FMT_YUYV8_1X16)
		mtk_dp->info[id].format = DP_PIXELFORMAT_YUV422;
	else
		mtk_dp->info[id].format = DP_PIXELFORMAT_RGB;

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX]bridge[%d]atomic check,[cp ct]:old[%d %d],new[%d %d]\n",
		    id, mtk_dp->con_state[id].content_protection,
		    mtk_dp->con_state[id].hdcp_content_type,
		    conn_state->content_protection, conn_state->hdcp_content_type);

	memcpy(&mtk_dp->con_state[id], conn_state, sizeof(struct drm_connector_state));

	drm_mode_copy(&mtk_dp->mode[id], &crtc_state->adjusted_mode);

	bpp = mtk_dp_color_get_bpp(mtk_dp->info[id].format,
						mtk_dp->info[id].depth);
	mtk_dp_check_mode(mtk_dp, &mtk_dp->mode[id], bpp, &dsc);
	mtk_dp->dsc_enable[id] = dsc;

	if (mtk_dp->dsc_enable[id]) {
		mtk_dp->info[id].format = DP_PIXELFORMAT_RGB;
		mtk_dp->info[id].depth = DP_COLOR_DEPTH_8BIT;
		mtk_dp->prop_dsc_enable[id]->values[0] = 1;
	} else {
		if (mtk_dp->prop_dsc_enable[id])
			mtk_dp->prop_dsc_enable[id]->values[0] = 0;
	}

	drm_dbg_kms(mtk_dp->drm_dev,
		    "[DPTX] bridge[%d] SST atomic check, dsc:%d, color:in[0x%04x] out[0x%04x]\n",
		    id, dsc, bridge_state->input_bus_cfg.format,
		    bridge_state->output_bus_cfg.format);
	drm_dbg_kms(mtk_dp->drm_dev,
		    "[DPTX] bridge[%d] SST atomic check, tt:%d %d, act:%d %d, fps:%d, clk:%d\n",
		    mtk_dp->mode[id].htotal, mtk_dp->mode[id].vtotal,
		    mtk_dp->mode[id].hdisplay, mtk_dp->mode[id].vdisplay,
		    drm_mode_vrefresh(&mtk_dp->mode[id]), mtk_dp->mode[id].clock);
	return 0;
}

static const struct drm_bridge_funcs mtk_dp_bridge_funcs = {
	.atomic_check = mtk_dp_bridge_atomic_check,
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_get_output_bus_fmts = mtk_dp_bridge_atomic_get_output_bus_fmts,
	.atomic_get_input_bus_fmts = mtk_dp_bridge_atomic_get_input_bus_fmts,
	.atomic_reset = drm_atomic_helper_bridge_reset,
	.attach = mtk_dp_bridge_attach,
	.detach = mtk_dp_bridge_detach,
	.atomic_enable = mtk_dp_bridge_atomic_enable,
	.atomic_disable = mtk_dp_bridge_atomic_disable,
};

/*
 * HDMI audio codec callbacks
 */
static int mtk_dp_audio_hw_params(struct device *dev, void *data,
				  struct hdmi_codec_daifmt *daifmt,
				  struct hdmi_codec_params *params)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);
	enum dp_encoder_id encoder_id;

	for (encoder_id = 0; encoder_id < mtk_dp->data->encoder_num; encoder_id++) {
		mtk_dp->info[encoder_id].audio_cur_cfg.channels = params->cea.channels;
		mtk_dp->info[encoder_id].audio_cur_cfg.word_length_bits = params->sample_width;
		mtk_dp->info[encoder_id].audio_cur_cfg.sample_rate = params->sample_rate;

		mtk_dp_audio_config(mtk_dp, encoder_id);
	}

	return 0;
}

static int mtk_dp_audio_startup(struct device *dev, void *data)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);
	enum dp_encoder_id encoder_id;

	for (encoder_id = 0; encoder_id < mtk_dp->data->encoder_num; encoder_id++)
		mtk_dp_audio_mute(mtk_dp, encoder_id, false);

	return 0;
}

static void mtk_dp_audio_shutdown(struct device *dev, void *data)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);
	enum dp_encoder_id encoder_id;

	for (encoder_id = 0; encoder_id < mtk_dp->data->encoder_num; encoder_id++)
		mtk_dp_audio_mute(mtk_dp, encoder_id, true);
}

static int mtk_dp_audio_get_eld(struct device *dev, void *data, uint8_t *buf,
				size_t len)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);
	int con_id = -ENODEV;
	u8 i, encoder_id;

	if (!mtk_dp->mst_enable) {
		con_id = DP_FIRST_CON;
	} else {
		for (i = 0; i < ARRAY_SIZE(mtk_dp->mtk_con); i++) {
			if (mst_con_with_encoder(mtk_dp->mtk_con[i])) {
				encoder_id = mtk_dp->mtk_con[i]->encoder_id;

				drm_dbg_kms(mtk_dp->drm_dev,
					    "[DPTX] MST, enc[%d] con[%d], video enable:%d, detect monitor:%d\n",
					    encoder_id, i, mtk_dp->mtk_con[i]->video_enable,
					    mtk_dp->info[encoder_id].audio_cur_cfg.detect_monitor);

				if (mtk_dp->mtk_con[i]->video_enable &&
				    mtk_dp->info[encoder_id].audio_cur_cfg.detect_monitor) {
					con_id = i;
					break;
				}
			}
		}
	}

	if (con_id < 0 || !mtk_dp->mtk_con[con_id]) {
		dev_info(mtk_dp->dev, "audio eld not found!\n");
		memset(buf, 0, len);
	} else {
		memcpy(buf, mtk_dp->mtk_con[con_id]->connector.eld, len);
	}

	return 0;
}

static int mtk_dp_audio_hook_plugged_cb(struct device *dev, void *data,
					hdmi_codec_plugged_cb fn,
					struct device *codec_dev)
{
	struct mtk_dp *mtk_dp = data;

	mutex_lock(&mtk_dp->update_plugged_status_lock);
	mtk_dp->plugged_cb = fn;
	mtk_dp->codec_dev = codec_dev;
	mutex_unlock(&mtk_dp->update_plugged_status_lock);

	mtk_dp_audio_update_plugged_status(mtk_dp);

	return 0;
}

static const struct hdmi_codec_ops mtk_dp_audio_codec_ops = {
	.hw_params = mtk_dp_audio_hw_params,
	.audio_startup = mtk_dp_audio_startup,
	.audio_shutdown = mtk_dp_audio_shutdown,
	.get_eld = mtk_dp_audio_get_eld,
	.hook_plugged_cb = mtk_dp_audio_hook_plugged_cb,
	.no_capture_mute = 1,
};

static int mtk_dp_register_audio_driver(struct device *dev)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);
	struct hdmi_codec_pdata codec_data = {
		.ops = &mtk_dp_audio_codec_ops,
		.max_i2s_channels = 8,
		.i2s = 1,
		.data = mtk_dp,
	};

	mtk_dp->audio_pdev = platform_device_register_data(dev,
							   HDMI_CODEC_DRV_NAME,
							   PLATFORM_DEVID_AUTO,
							   &codec_data,
							   sizeof(codec_data));
	return PTR_ERR_OR_ZERO(mtk_dp->audio_pdev);
}

static int mtk_dp_edp_link_panel(struct drm_dp_aux *mtk_aux)
{
	struct mtk_dp *mtk_dp = container_of(mtk_aux, struct mtk_dp, aux);
	struct device *dev = mtk_aux->dev;
	int ret;

	mtk_dp->next_bridge = devm_drm_of_get_bridge(dev, dev->of_node, 1, 0);

	/* Power off the DP and AUX: either detection is done, or no panel present */
	mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_PWR_STATE,
			   DP_PWR_STATE_BANDGAP_TPLL,
			   DP_PWR_STATE_MASK);
	mtk_dp_power_disable(mtk_dp);

	if (IS_ERR(mtk_dp->next_bridge)) {
		ret = PTR_ERR(mtk_dp->next_bridge);
		mtk_dp->next_bridge = NULL;
		return ret;
	}

	/* For eDP, we add the bridge only if the panel was found */
	ret = devm_drm_bridge_add(dev, &mtk_dp->bridge);
	if (ret)
		return ret;

	return 0;
}

static int mtk_drm_dp_notifier(struct notifier_block *notifier,
			       unsigned long pm_event, void *unused)
{
	struct mtk_dp *mtk_dp = container_of(notifier, struct mtk_dp, notifier);
	struct device *dev = mtk_dp->dev;

	drm_dbg_kms(mtk_dp->drm_dev,
		    "[DPTX] %s pm_event %lu dev %s usage_count %d nb priority %d\n",
		    __func__, pm_event, dev_name(dev), atomic_read(&dev->power.usage_count),
		    notifier->priority);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static void mtk_dp_aux_init(struct mtk_dp *mtk_dp)
{
	drm_dp_aux_init(&mtk_dp->aux);

	mtk_dp->aux.name = "aux_mtk_dp";
	mtk_dp->aux.transfer = mtk_dp_aux_transfer;
}

static int mtk_dp_pm_init(struct mtk_dp *mtk_dp, struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;

	mtk_dp->genpd_dp_tx = dev_pm_domain_attach_by_name(dev, "pd_dp_tx");
	if (IS_ERR_OR_NULL(mtk_dp->genpd_dp_tx)) {
		ret = PTR_ERR(mtk_dp->genpd_dp_tx) ? : -ENODATA;
		drm_dbg_kms(mtk_dp->drm_dev,
			    "[DPTX] failed to attach pd_dp_tx pm-domain: %d\n", ret);
		return -ENODEV;
	}

	mtk_dp->genpd_dp_phy = dev_pm_domain_attach_by_name(dev, "pd_dp_phy");
	if (IS_ERR_OR_NULL(mtk_dp->genpd_dp_phy)) {
		ret = PTR_ERR(mtk_dp->genpd_dp_phy) ? : -ENODATA;
		drm_dbg_kms(mtk_dp->drm_dev,
			    "[DPTX] failed to attach pd_dp_phy pm-domain: %d\n", ret);
		return -ENODEV;
	}

	mtk_dp->genpd_dl_dp_tx = device_link_add(dev, mtk_dp->genpd_dp_tx,
						 DL_FLAG_PM_RUNTIME |
						 DL_FLAG_STATELESS);
	if (!mtk_dp->genpd_dl_dp_tx) {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] failed to add dp tx link\n");
		return -ENODEV;
	}

	mtk_dp->genpd_dl_dp_phy = device_link_add(dev, mtk_dp->genpd_dp_phy,
						  DL_FLAG_PM_RUNTIME |
						  DL_FLAG_STATELESS);
	if (!mtk_dp->genpd_dl_dp_phy) {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] failed to add dp phy link\n");
		return -ENODEV;
	}

	return 0;
}

static int mtk_dp_probe_v2(struct platform_device *pdev)
{
	struct mtk_dp *mtk_dp;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *port;
	struct mtk_drm_private *mtk_priv = dev_get_drvdata(dev);
	int ret;
	int i;

	mtk_dp = devm_kzalloc(dev, sizeof(*mtk_dp), GFP_KERNEL);
	if (!mtk_dp)
		return -ENOMEM;

	memset(mtk_dp, 0, sizeof(struct mtk_dp));
	mtk_dp->id = 0x0;
	mtk_dp->dev = dev;
	mtk_dp->priv = mtk_priv;
	mtk_dp->disp_state = DP_DISP_STATE_NONE;

	mtk_dp->data = (struct mtk_dp_data *)of_device_get_match_data(dev);

	ret = mtk_dp_pm_init(mtk_dp, pdev);
	if (ret)
		return ret;

	pm_runtime_enable(mtk_dp->dev);
	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] pm_runtime_get_sync\n");
	pm_runtime_get_sync(mtk_dp->dev);

	ret = mtk_dp_dt_parse(mtk_dp, pdev);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to parse dt\n");

	/*
	 * Request the interrupt and install service routine only if we are
	 * on full DisplayPort.
	 * For eDP, polling the HPD instead is more convenient because we
	 * don't expect any (un)plug events during runtime, hence we can
	 * avoid some locking.
	 */
	if (mtk_dp->data->bridge_type != DRM_MODE_CONNECTOR_eDP) {
		mtk_dp->irq = platform_get_irq(pdev, 0);
		if (mtk_dp->irq < 0)
			return dev_err_probe(dev, mtk_dp->irq,
					     "failed to request dp irq resource\n");

		spin_lock_init(&mtk_dp->irq_thread_lock);

		irq_set_status_flags(mtk_dp->irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(dev, mtk_dp->irq, mtk_dp_hpd_event,
						mtk_dp_hpd_event_thread,
						IRQ_TYPE_LEVEL_HIGH, dev_name(dev),
						mtk_dp);
		if (ret)
			return dev_err_probe(dev, ret,
					     "failed to request mediatek dptx irq\n");
	}

	mtk_dp_aux_init(mtk_dp);

	platform_set_drvdata(pdev, mtk_dp);

	if (mtk_dp->data->audio_supported) {
		mutex_init(&mtk_dp->update_plugged_status_lock);

		ret = mtk_dp_register_audio_driver(dev);
		if (ret) {
			dev_err(dev, "Failed to register audio driver: %d\n",
				ret);
			return ret;
		}
	}

	if (mtk_dp->data->mac_power)
		mtk_dp->data->mac_power(mtk_dp);

	mtk_dp->pclk = devm_clk_get_enabled(mtk_dp->dev, "mux_dp");
	if (IS_ERR(mtk_dp->pclk))
		return dev_err_probe(dev, PTR_ERR(mtk_dp->pclk),
				     "[DPTX] Failed to get pixel clock\n");
	mtk_dp->pclk_src = devm_clk_get(mtk_dp->dev, "ck_26m");
	if (IS_ERR(mtk_dp->pclk_src))
		return dev_err_probe(dev, PTR_ERR(mtk_dp->pclk_src),
				     "[DPTX] Failed to get pixel source clock\n");

	ret = clk_set_parent(mtk_dp->pclk, mtk_dp->pclk_src);
	if (ret < 0)
		dev_err(mtk_dp->dev, "[DPTX] Failed to clk_set_parent:%d\n", ret);

	dev_dbg(mtk_dp->dev, "pclk:%ld\n", clk_get_rate(mtk_dp->pclk));

	mtk_dp->notifier.notifier_call = mtk_drm_dp_notifier;
	ret = register_pm_notifier(&mtk_dp->notifier);
	if (ret)
		dev_err(mtk_dp->dev, "[DPTX] register pm notifier failed %d", ret);


	for_each_of_graph_port(np, port) {
		if (i >= mtk_dp->data->encoder_num)
			break;

		dev_dbg(dev, "[DPTX] port:%pOF\n", port);

		mtk_dp->mtk_bridge[i] = devm_kzalloc(dev, sizeof(struct mtk_dp_bridge), GFP_KERNEL);
		if (!mtk_dp->mtk_bridge[i])
			return -ENOMEM;

		mtk_dp->mtk_bridge[i]->bridge.funcs = &mtk_dp_bridge_funcs;
		mtk_dp->mtk_bridge[i]->bridge.of_node = port;
		mtk_dp->mtk_bridge[i]->bridge.type = mtk_dp->data->bridge_type;
		drm_bridge_add(&mtk_dp->mtk_bridge[i]->bridge);

		mtk_dp->mtk_bridge[i]->mtk_dp = mtk_dp;
		mtk_dp->mtk_bridge[i]->encoder_id = i;

		i++;
	}

	atomic_set(&mtk_dp->refcount, 0);

	mtk_dp->cts_req.aux = &mtk_dp->aux;
	mtk_dp->cts_req.regs = mtk_dp->regs;

	mtk_dp_init_port(mtk_dp);

	return 0;
}

static void mtk_dp_remove_v2(struct platform_device *pdev)
{
	struct mtk_dp *mtk_dp = platform_get_drvdata(pdev);

	pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	if (mtk_dp->audio_pdev)
		platform_device_unregister(mtk_dp->audio_pdev);
}

#ifdef CONFIG_PM_SLEEP
static int mtk_dp_suspend_v2(struct device *dev)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);

	if (!mtk_dp) {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] suspend, dp not initial\n");
		return 0;
	}

	if (mtk_dp->disp_state == DP_DISP_STATE_SUSPENDING ||
	    mtk_dp->disp_state == DP_DISP_STATE_SUSPEND) {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] have suspended and do nothing\n");
		return 0;
	}

	mtk_dp->disp_state = DP_DISP_STATE_SUSPENDING;

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] suspend +\n");

	mtk_dp_hpd_interrupt_enable(mtk_dp, false);

	mtk_dp_disconnect_release(mtk_dp);

	if (mtk_dp->data->mac_power)
		mtk_dp->data->mac_power(mtk_dp);

	clk_disable_unprepare(mtk_dp->pclk);

	mtk_dp->disp_state = DP_DISP_STATE_SUSPEND;
	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] pm_runtime_put_sync\n");
	pm_runtime_put_sync(mtk_dp->dev);

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] suspend -\n");

	return 0;
}

static int mtk_dp_resume_v2(struct device *dev)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);
	int ret = 0;

	if (!mtk_dp) {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] resume, dp not initial\n");
		return 0;
	}

	if (mtk_dp->disp_state == DP_DISP_STATE_RESUME) {
		drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] have resumed and do nothing\n");
		return 0;
	}

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] pm_runtime_get_sync\n");
	pm_runtime_get_sync(dev);
	mtk_dp->disp_state = DP_DISP_STATE_RESUME;

	ret = clk_prepare_enable(mtk_dp->pclk);
	if (ret) {
		dev_err(mtk_dp->dev, "[DPTX] Failed to prepare and enable clock\n");
		return ret;
	}

	if (mtk_dp->data->mac_power)
		mtk_dp->data->mac_power(mtk_dp);

	mtk_dp_init_port(mtk_dp);

	mtk_dp_hpd_interrupt_enable(mtk_dp, true);

	drm_dbg_kms(mtk_dp->drm_dev, "[DPTX] resume done\n");

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_dp_pm_ops_v2, mtk_dp_suspend_v2, mtk_dp_resume_v2);

static const struct mtk_dp_data mt8189_dp_data = {
	.bridge_type = DRM_MODE_CONNECTOR_DisplayPort,
	.audio_supported = true,
	.audio_pkt_in_hblank_area = true,
	.audio_m_div2_bit = 0,
	.dsc_support = true,
	.mst_support = false,
	.max_hdisplay = 3840,
	.max_vdisplay = 2160,
	.min_hdisplay = 800,
	.min_vdisplay = 600,
	.phy_patch = mtk_dp_phy_patch,
	.phyd_dig_glb_offset = 0x1400,
	.phyd_dig_lan0_offset = 0x1000,
	.phyd_dig_lan1_offset = 0x1100,
	.phyd_dig_lan2_offset = 0x1200,
	.phyd_dig_lan3_offset = 0x1300,
	.phyd_ana_glb_offset = 0x400,
	.phyd_ana_lan0_offset = 0x0,
	.phyd_ana_lan1_offset = 0x100,
	.phyd_ana_lan2_offset = 0x200,
	.phyd_ana_lan3_offset = 0x300,
	.support_max_linkrate = DP_LINK_RATE_HBR2,
	.support_max_lanecount = DP_4LANE,
	.encoder_num = 0x1,
	.phy_4lane_ctrl_bit = BIT(8),
	.phy_flip_ctrl_bit = BIT(7),
};

static const struct of_device_id mtk_dp_of_match_v2[] = {
	{
		.compatible = "mediatek,mt8189-dp-tx",
		.data = &mt8189_dp_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_dp_of_match_v2);

static struct platform_driver mtk_dp_driver_v2 = {
	.probe = mtk_dp_probe_v2,
	.remove_new = mtk_dp_remove_v2,
	.driver = {
		.name = "mediatek-drm-dp-v2",
		.of_match_table = mtk_dp_of_match_v2,
		.pm = &mtk_dp_pm_ops_v2,
	},
};

module_platform_driver(mtk_dp_driver_v2);

MODULE_AUTHOR("Jitao Shi <jitao.shi@mediatek.com>");
MODULE_AUTHOR("Markus Schneider-Pargmann <msp@baylibre.com>");
MODULE_AUTHOR("Bo-Chen Chen <rex-bc.chen@mediatek.com>");
MODULE_DESCRIPTION("MediaTek DisplayPort Driver");
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: phy_mtk_dp");
