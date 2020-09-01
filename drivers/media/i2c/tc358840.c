// SPDX-License-Identifier: GPL-2.0-only
/*
 * tc358840.c - Toshiba UH2C/D HDMI-CSI bridge driver
 *
 * Copyright (c) 2015, Armin Weiss <weii@zhaw.ch>
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 * Copyright 2015-2020 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/hdmi.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_graph.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/v4l2-dv-timings.h>
#include <linux/videodev2.h>
#include <linux/workqueue.h>

#include <media/cec.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dv-timings.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include <media/i2c/tc358840.h>

#include "tc358840_regs.h"

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "debug level (0-3)");

#define TEST_PATTERN_DISABLED		0
#define TEST_PATTERN_COLOR_BAR		1
#define TEST_PATTERN_COLOR_CHECKER	2

#define EDID_NUM_BLOCKS_MAX 8
#define EDID_BLOCK_SIZE 128

#define I2C_MAX_XFER_SIZE  (EDID_BLOCK_SIZE + 2)

#define POLLS_PER_SEC 5
#define POLL_PERIOD (HZ / POLLS_PER_SEC)

static const struct v4l2_dv_timings_cap tc358840_timings_cap_1080p60 = {
	.type = V4L2_DV_BT_656_1120,
	/* keep this initialization for compatibility with GCC < 4.4.6 */
	.reserved = { 0 },
	/* Pixel clock from REF_01 p. 20. */
	V4L2_INIT_BT_TIMINGS(160, 1920, 120, 1200, 25000000, 165000000,
			     V4L2_DV_BT_STD_CEA861 | V4L2_DV_BT_STD_DMT |
			     V4L2_DV_BT_STD_GTF | V4L2_DV_BT_STD_CVT,
			     V4L2_DV_BT_CAP_PROGRESSIVE |
			     V4L2_DV_BT_CAP_REDUCED_BLANKING |
			     V4L2_DV_BT_CAP_CUSTOM)
};

static const struct v4l2_dv_timings_cap tc358840_timings_cap_4kp30 = {
	.type = V4L2_DV_BT_656_1120,
	/* keep this initialization for compatibility with GCC < 4.4.6 */
	.reserved = { 0 },
	/* Pixel clock from REF_01 p. 20. Min/max height/width are unknown */
	V4L2_INIT_BT_TIMINGS(160, 3840, 120, 2160, 25000000, 300000000,
			     V4L2_DV_BT_STD_CEA861 | V4L2_DV_BT_STD_DMT |
			     V4L2_DV_BT_STD_GTF | V4L2_DV_BT_STD_CVT,
			     V4L2_DV_BT_CAP_PROGRESSIVE |
			     V4L2_DV_BT_CAP_REDUCED_BLANKING |
			     V4L2_DV_BT_CAP_CUSTOM)
};

enum tc358840_status {
	STATUS_FIND_SIGNAL,
	STATUS_FOUND_SIGNAL,
	STATUS_STABLE_SIGNAL,
};

struct tc358840_state {
	struct tc358840_platform_data pdata;
	struct v4l2_subdev sd;
	struct media_pad pad[2];
	struct v4l2_ctrl_handler hdl;
	struct i2c_client *i2c_client;
	/* serialize between the poll function and ops */
	struct mutex lock;
	bool suspended;

	struct cec_adapter *cec_adap;
	bool have_5v;
	enum tc358840_status status;
	bool eq_bypass;
	unsigned int found_signal_cnt[2];
	unsigned int dropped_signal_cnt[2];
	bool invalid_eq_bypass[2];
	bool enabled;
	bool found_stable_signal;
	int test_pattern;
	bool test_pattern_changed;
	bool test_dl;
	unsigned int retry_attempts_stats[6];

	/* controls */
	struct v4l2_ctrl *detect_tx_5v_ctrl;
	struct v4l2_ctrl *audio_sampling_rate_ctrl;
	struct v4l2_ctrl *audio_present_ctrl;
	struct v4l2_ctrl *tmds_present_ctrl;
	struct v4l2_ctrl *rgb_quantization_range_ctrl;
	struct v4l2_ctrl *splitter_width_ctrl;
	struct v4l2_ctrl *test_pattern_ctrl;
	struct v4l2_ctrl *eq_byps_mode_ctrl;

	struct delayed_work delayed_work_enable_hotplug;
	struct delayed_work delayed_work_poll;
	bool delayed_work_stop_polling;

	/* edid  */
	u8 edid_blocks_written;

	/* timing / mbus */
	struct v4l2_dv_timings timings;
	struct v4l2_dv_timings detected_timings;
	u32 mbus_fmt_code;
	u32 rgb_quantization_range;
};

static inline struct tc358840_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct tc358840_state, sd);
}

static void tc358840_enable_interrupts(struct v4l2_subdev *sd,
				       bool cable_connected);
static void tc358840_init_interrupts(struct v4l2_subdev *sd);
static int tc358840_s_dv_timings(struct v4l2_subdev *sd,
				 struct v4l2_dv_timings *timings);
static void tc358840_set_csi(struct v4l2_subdev *sd);
static void tc358840_set_splitter(struct v4l2_subdev *sd);
static int tc358840_set_test_pattern_timing(struct v4l2_subdev *sd,
					    struct v4l2_dv_timings *timings);
static int get_test_pattern_timing(struct v4l2_subdev *sd,
				   struct v4l2_dv_timings *timings);

/* --------------- I2C --------------- */

static void i2c_rd(struct v4l2_subdev *sd, u16 reg, u8 *values, u32 n)
{
	struct tc358840_state *state = to_state(sd);
	struct i2c_client *client = state->i2c_client;
	int err;
	u8 buf[2] = { reg >> 8, reg & 0xff };
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = n,
			.buf = values,
		},
	};

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != ARRAY_SIZE(msgs)) {
		v4l2_err(sd, "%s: reading register 0x%x from 0x%x failed\n",
			 __func__, reg, client->addr);
	}

	if (debug < 3)
		return;

	switch (n) {
	case 1:
		v4l2_info(sd, "I2C read 0x%04X = 0x%02X\n", reg, values[0]);
		break;
	case 2:
		v4l2_info(sd, "I2C read 0x%04X = 0x%02X%02X\n",
			  reg, values[1], values[0]);
		break;
	case 4:
		v4l2_info(sd, "I2C read 0x%04X = 0x%02X%02X%02X%02X\n",
			  reg, values[3], values[2], values[1], values[0]);
		break;
	default:
		v4l2_info(sd, "I2C read %d bytes from address 0x%04X\n",
			  n, reg);
		break;
	}
}

static void i2c_wr(struct v4l2_subdev *sd, u16 reg, u8 *values, u32 n)
{
	struct tc358840_state *state = to_state(sd);
	struct i2c_client *client = state->i2c_client;
	int err, i;
	struct i2c_msg msg;
	u8 data[I2C_MAX_XFER_SIZE];

	if ((2 + n) > I2C_MAX_XFER_SIZE) {
		v4l2_warn(sd, "i2c wr reg=%04x: len=%d is too big!\n",
			  reg, 2 + n);
		n = I2C_MAX_XFER_SIZE - 2;
	}

	msg.addr = client->addr;
	msg.buf = data;
	msg.len = 2 + n;
	msg.flags = 0;

	data[0] = reg >> 8;
	data[1] = reg & 0xff;

	for (i = 0; i < n; i++)
		data[2 + i] = values[i];

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err != 1) {
		v4l2_err(sd, "%s: writing register 0x%x from 0x%x failed\n",
			 __func__, reg, client->addr);
		return;
	}

	if (debug < 3)
		return;

	switch (n) {
	case 1:
		v4l2_info(sd, "I2C write 0x%04X = 0x%02X\n", reg, data[2]);
		break;
	case 2:
		v4l2_info(sd, "I2C write 0x%04X = 0x%02X%02X\n",
			  reg, data[3], data[2]);
		break;
	case 4:
		v4l2_info(sd, "I2C write 0x%04X = 0x%02X%02X%02X%02X\n",
			  reg, data[5], data[4], data[3], data[2]);
		break;
	default:
		v4l2_info(sd, "I2C write %d bytes from address 0x%04X\n",
			  n, reg);
	}
}

static u8 i2c_rd8(struct v4l2_subdev *sd, u16 reg)
{
	u8 val;

	i2c_rd(sd, reg, &val, 1);

	return val;
}

static void i2c_wr8(struct v4l2_subdev *sd, u16 reg, u8 val)
{
	i2c_wr(sd, reg, &val, 1);
}

static void i2c_wr8_and_or(struct v4l2_subdev *sd, u16 reg,
			   u8 mask, u8 val)
{
	i2c_wr8(sd, reg, (i2c_rd8(sd, reg) & mask) | val);
}

static u16 i2c_rd16(struct v4l2_subdev *sd, u16 reg)
{
	u16 val;

	i2c_rd(sd, reg, (u8 *)&val, 2);

	return val;
}

static void i2c_wr16(struct v4l2_subdev *sd, u16 reg, u16 val)
{
	i2c_wr(sd, reg, (u8 *)&val, 2);
}

static void i2c_wr16_and_or(struct v4l2_subdev *sd, u16 reg, u16 mask, u16 val)
{
	i2c_wr16(sd, reg, (i2c_rd16(sd, reg) & mask) | val);
}

static u32 i2c_rd32(struct v4l2_subdev *sd, u16 reg)
{
	u32 val;

	i2c_rd(sd, reg, (u8 *)&val, 4);

	return val;
}

static void i2c_wr32(struct v4l2_subdev *sd, u16 reg, u32 val)
{
	i2c_wr(sd, reg, (u8 *)&val, 4);
}

static void i2c_wr32_and_or(struct v4l2_subdev *sd, u32 reg, u32 mask, u32 val)
{
	i2c_wr32(sd, reg, (i2c_rd32(sd, reg) & mask) | val);
}

/* --------------- STATUS --------------- */

static inline bool is_hdmi(struct v4l2_subdev *sd)
{
	return i2c_rd8(sd, SYS_STATUS) & MASK_S_HDMI;
}

static inline bool tx_5v_power_present(struct v4l2_subdev *sd)
{
	return i2c_rd8(sd, SYS_STATUS) & MASK_S_DDC5V;
}

static inline bool no_signal(struct v4l2_subdev *sd)
{
	return !(i2c_rd8(sd, SYS_STATUS) & MASK_S_TMDS);
}

static inline bool no_sync(struct v4l2_subdev *sd)
{
	return !(i2c_rd8(sd, SYS_STATUS) & MASK_S_SYNC);
}

static inline bool audio_present(struct v4l2_subdev *sd)
{
	return i2c_rd8(sd, AU_STATUS0) & MASK_S_A_SAMPLE;
}

static int get_audio_sampling_rate(struct v4l2_subdev *sd)
{
	static const int code_to_rate[] = {
		44100, 0, 48000, 32000, 22050, 384000, 24000, 352800,
		88200, 768000, 96000, 705600, 176400, 0, 192000, 0
	};

	/* Register FS_SET is not cleared when the cable is disconnected */
	if (no_signal(sd))
		return 0;

	return code_to_rate[i2c_rd8(sd, FS_SET) & MASK_FS];
}

static unsigned int tc358840_num_csi_lanes_in_use(struct v4l2_subdev *sd)
{
	/* FIXME: Read # of lanes from both, TX0 and TX1 */
	return i2c_rd32(sd, CSITX0_BASE_ADDR + LANEEN) & MASK_LANES;
}

/* --------------- TIMINGS --------------- */

static inline unsigned int fps(const struct v4l2_bt_timings *t)
{
	if (!V4L2_DV_BT_FRAME_HEIGHT(t) || !V4L2_DV_BT_FRAME_WIDTH(t))
		return 0;

	return DIV_ROUND_CLOSEST((unsigned int)t->pixelclock,
			V4L2_DV_BT_FRAME_HEIGHT(t) * V4L2_DV_BT_FRAME_WIDTH(t));
}

/* --------------- HOTPLUG / HDCP / EDID --------------- */

static void tc358840_delayed_work_enable_hotplug(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct tc358840_state *state = container_of(dwork,
		struct tc358840_state, delayed_work_enable_hotplug);
	struct v4l2_subdev *sd = &state->sd;

	v4l2_dbg(2, debug, sd, "%s\n", __func__);

	i2c_wr8_and_or(sd, HPD_CTL, ~(u8)MASK_HPD_OUT0, MASK_HPD_OUT0);
}

static void tc358840_set_hdmi_hdcp(struct v4l2_subdev *sd, bool enable)
{
	int i;

	v4l2_info(sd, "%s: %s\n", __func__, enable ?  "enable" : "disable");

	/* Disable the HDCP repeater ready flag */
	i2c_wr8_and_or(sd, BCAPS, ~(u8)MASK_READY, 0);

	i2c_wr8_and_or(sd, HDCP_MODE, ~(u8)MASK_MANUAL_AUTHENTICATION,
		       MASK_MANUAL_AUTHENTICATION);
	i2c_wr8(sd, HDCP_BKSV, 0);
	for (i = 0; i < 5; i++)
		i2c_wr8(sd, BKSV + i, 0);
}

static void tc358840_disable_hpd(struct v4l2_subdev *sd)
{
	struct tc358840_state *state = to_state(sd);

	v4l2_dbg(2, debug, sd, "%s\n", __func__);

	cancel_delayed_work_sync(&state->delayed_work_enable_hotplug);

	i2c_wr8_and_or(sd, HPD_CTL, ~(u8)MASK_HPD_OUT0, 0x0);
}

static void tc358840_enable_hpd(struct v4l2_subdev *sd)
{
	struct tc358840_state *state = to_state(sd);

	if (state->edid_blocks_written == 0) {
		v4l2_dbg(2, debug, sd, "%s: no EDID -> no hotplug\n", __func__);
		return;
	}

	v4l2_dbg(2, debug, sd, "%s\n", __func__);

	/*
	 * Enable hotplug after ~140 ms. Minimum hpd low interval
	 * is 100 ms in the hdcp standard. Use ~140 ms to avoid
	 * problems with the hdcp compliance test suite.
	 */
	schedule_delayed_work(&state->delayed_work_enable_hotplug, HZ / 7);

	tc358840_enable_interrupts(sd, true);
}

/* --------------- infoframe --------------- */

static void print_infoframe(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct device *dev = &client->dev;
	union hdmi_infoframe frame;
	u8 buffer[32] = { };

	if (!is_hdmi(sd)) {
		v4l2_info(sd, "DVI-D signal - InfoFrames not supported\n");
		return;
	}

	i2c_rd(sd, PK_AVI_0HEAD, buffer, HDMI_INFOFRAME_SIZE(AVI));

	if (hdmi_infoframe_unpack(&frame, buffer, sizeof(buffer)) >= 0)
		hdmi_infoframe_log(KERN_INFO, dev, &frame);

	/*
	 * Both the SPD and the Vendor Specific packet sizes are the
	 * same for the tc358840. Since HDMI_INFOFRAME_SIZE(VENDOR) is
	 * larger than HDMI_INFOFRAME_SIZE(SPD) we use the latter instead.
	 * The remaining bytes in buffer[] are 0.
	 */
	i2c_rd(sd, PK_VS_0HEAD, buffer, HDMI_INFOFRAME_SIZE(SPD));

	if (hdmi_infoframe_unpack(&frame, buffer, sizeof(buffer)) >= 0)
		hdmi_infoframe_log(KERN_INFO, dev, &frame);

	i2c_rd(sd, PK_AUD_0HEAD, buffer, HDMI_INFOFRAME_SIZE(AUDIO));

	if (hdmi_infoframe_unpack(&frame, buffer, sizeof(buffer)) >= 0)
		hdmi_infoframe_log(KERN_INFO, dev, &frame);

	i2c_rd(sd, PK_SPD_0HEAD, buffer, HDMI_INFOFRAME_SIZE(SPD));

	if (hdmi_infoframe_unpack(&frame, buffer, sizeof(buffer)) >= 0)
		hdmi_infoframe_log(KERN_INFO, dev, &frame);
}

/* --------------- CTRLS --------------- */

static void set_rgb_quantization_range(struct v4l2_subdev *sd)
{
	struct tc358840_state *state = to_state(sd);
	u8 out;

	if (state->mbus_fmt_code == MEDIA_BUS_FMT_UYVY8_1X16)
		out = MASK_COLOR_601_YCBCR_LIMITED;
	else
		out = MASK_COLOR_RGB_FULL;

	if ((i2c_rd8(sd, PK_AVI_1BYTE) & 0xe0)) {
		/* We're receiving YCbCr, so ignore rgb quant. range */
		i2c_wr8_and_or(sd, VI_MODE, ~MASK_RGB_HDMI, MASK_RGB_HDMI);
		i2c_wr8(sd, VOUT_CSC, MASK_CSC_MODE_BUILTIN | out);
		return;
	}

	switch (state->rgb_quantization_range) {
	case V4L2_DV_RGB_RANGE_AUTO:
		i2c_wr8_and_or(sd, VI_MODE, ~MASK_RGB_HDMI, MASK_RGB_HDMI);
		i2c_wr8(sd, VOUT_CSC, MASK_CSC_MODE_BUILTIN | out);
		break;
	case V4L2_DV_RGB_RANGE_LIMITED:
		/*
		 * This will force all timings to be interpreted as
		 * limited range, with the exception of VGA (640x480).
		 * So this won't work for that timing.
		 */
		i2c_wr8_and_or(sd, VI_MODE, ~MASK_RGB_HDMI, 0);
		i2c_wr8(sd, VOUT_CSC, MASK_CSC_MODE_BUILTIN | out);
		break;
	case V4L2_DV_RGB_RANGE_FULL:
		i2c_wr8_and_or(sd, VI_MODE, ~MASK_RGB_HDMI, MASK_RGB_HDMI);
		/*
		 * This will force all timings to be interpreted as full
		 * range, except if the output is YUV: in that case only
		 * the IT timings and the CE timings 640x480 and 4kp30
		 * will be full range.
		 *
		 * The problem is that for YUV output we can't turn off the
		 * CSC matrix. And if we keep it on, then we can't override
		 * the quantization handling.
		 */
		if (state->mbus_fmt_code == MEDIA_BUS_FMT_UYVY8_1X16)
			i2c_wr8(sd, VOUT_CSC, MASK_CSC_MODE_BUILTIN | out);
		else
			i2c_wr8(sd, VOUT_CSC, MASK_CSC_MODE_OFF);
		break;
	}
}

/* --------------- INIT --------------- */

static void tc358840_reset_phy(struct v4l2_subdev *sd)
{
	v4l2_dbg(1, debug, sd, "%s\n", __func__);

	i2c_wr8_and_or(sd, PHY_RST, ~MASK_RESET_CTRL, 0);
	i2c_wr8_and_or(sd, PHY_RST, ~MASK_RESET_CTRL, MASK_RESET_CTRL);
}

static void tc358840_reset(struct v4l2_subdev *sd, u16 mask)
{
	u16 sysctl = i2c_rd16(sd, SYSCTL);

	i2c_wr16(sd, SYSCTL, sysctl | mask);
	i2c_wr16(sd, SYSCTL, sysctl & ~mask);
}

static inline void tc358840_sleep_mode(struct v4l2_subdev *sd, bool enable)
{
	v4l2_dbg(1, debug, sd, "%s(): %s\n", __func__,
		 enable ? "enable" : "disable");

	i2c_wr16_and_or(sd, SYSCTL, ~(u16)MASK_SLEEP, enable ? MASK_SLEEP : 0);
}

static int enable_stream(struct v4l2_subdev *sd, bool enable)
{
	struct tc358840_state *state = to_state(sd);
	struct tc358840_platform_data *pdata = &state->pdata;

	u32 sync_timeout_ctr;
	int retries = 0;

	v4l2_dbg(2, debug, sd, "%s: %sable\n", __func__, enable ? "en" : "dis");

	if (enable == state->enabled)
		goto out;

	state->test_dl = false;
retry:
	/* always start stream enable process with testimage turned off */
	i2c_wr16_and_or(sd, CB_CTL, ~(u16)MASK_CB_EN, 0);

	if (enable) {
		if (pdata->endpoint.bus.mipi_csi2.flags &
		    V4L2_MBUS_CSI2_NONCONTINUOUS_CLOCK) {
			i2c_wr32_and_or(sd, FUNCMODE, ~(u32)(MASK_CONTCLKMODE),
					MASK_FORCESTOP);
		} else {
			/*
			 * It is critical for CSI receiver to see lane
			 * transition LP11->HS. Set to non-continuous mode to
			 * enable clock lane LP11 state.
			 */
			i2c_wr32_and_or(sd, FUNCMODE,
					~(u32)(MASK_CONTCLKMODE), 0);
			/*
			 * Set to continuous mode to trigger LP11->HS
			 * transition
			 */
			i2c_wr32_and_or(sd, FUNCMODE, 0, MASK_CONTCLKMODE);
		}

		/* Unmute video */
		i2c_wr8(sd, VI_MUTE, MASK_AUTO_MUTE);

		/* Enable testpattern, must use TX1 */
		if (state->test_pattern)
			i2c_wr16_and_or(sd, CB_CTL,
					~(u16)(MASK_CB_EN | MASK_CB_CSEL),
					MASK_CB_CSEL_CSI_TX1 | MASK_CB_EN);
	} else {
		/*
		 * Mute video so that all data lanes go to LSP11 state.
		 * No data is output to CSI Tx block.
		 */
		i2c_wr8(sd, VI_MUTE, MASK_AUTO_MUTE | MASK_VI_MUTE);
		tc358840_set_csi(sd);
		tc358840_set_splitter(sd);
		/* Always disable testpattern */
		i2c_wr16_and_or(sd, CB_CTL, ~(u16)MASK_CB_EN, 0);
	}

	/* Wait for HDMI input to become stable */
	if (enable && !state->test_pattern) {
		sync_timeout_ctr = 100;
		while (no_sync(sd) && sync_timeout_ctr)
			sync_timeout_ctr--;

		if (sync_timeout_ctr == 0) {
			/* Disable stream again. Probably no cable inserted.. */
			v4l2_err(sd, "%s: Timeout: HDMI input sync failed.\n",
				 __func__);
			state->enabled = true;
			enable_stream(sd, false);
			return -EIO;
		}

		v4l2_dbg(2, debug, sd,
			 "%s: Stream enabled! Remaining timeout attempts: %d\n",
			 __func__, sync_timeout_ctr);
	}

	i2c_wr16_and_or(sd, CONFCTL0,
			~(u16)(MASK_VTX0EN | MASK_VTX1EN | MASK_ABUFEN),
			enable ? ((pdata->csi_port & (MASK_VTX0EN | MASK_VTX1EN)) |
				  MASK_ABUFEN | MASK_TX_MSEL | MASK_AUTOINDEX) :
			(MASK_TX_MSEL | MASK_AUTOINDEX));

	if (enable) {
		/*
		 * Wait for the data lanes to become active.
		 * Due to unknown reasons this sometimes fails, so we retry
		 * to enable streaming if we detect that the data lanes stay
		 * idle.
		 */
		u32 data_idle_ctr = 100;
		u32 v;

		/* Wait for the data lines to become busy */
		while (!((v = i2c_rd32(sd, CSITX_INTERNAL_STAT)) & PPI_DL_BUSY) &&
		       data_idle_ctr) {
			usleep_range(100, 1000);
			data_idle_ctr--;
		}
		v4l2_dbg(1, debug, sd,
			 "CSI TX internal stat 0x%02x, counter %d, attempt %d\n",
			 v, data_idle_ctr, retries + 1);
		if (data_idle_ctr == 0) {
			state->enabled = true;
			enable_stream(sd, false);
			if (retries++ < 5)
				goto retry;
			v4l2_err(sd, "Could not detect busy data lanes after %d attempts (stats: %u %u %u %u %u\n", retries,
				 state->retry_attempts_stats[1],
				 state->retry_attempts_stats[2],
				 state->retry_attempts_stats[3],
				 state->retry_attempts_stats[4],
				 state->retry_attempts_stats[5]);
			return -EIO;
		}
		state->retry_attempts_stats[retries]++;
		if (retries > 2)
			v4l2_info(sd, "Needed %d attempts to detect busy data lanes\n",
				  retries);
		state->test_dl = true;
	}

	state->enabled = enable;
out:
	return 0;
}

static void tc358840_set_splitter(struct v4l2_subdev *sd)
{
	struct tc358840_state *state = to_state(sd);
	struct tc358840_platform_data *pdata = &state->pdata;

	v4l2_dbg(3, debug, sd, "%s():\n", __func__);

	if (pdata->csi_port != CSI_TX_BOTH) {
		i2c_wr16_and_or(sd, SPLITTX0_CTRL,
				~(u16)(MASK_IFEN | MASK_LCD_CSEL), MASK_SPBP);
		i2c_wr16_and_or(sd, SPLITTX1_CTRL,
				~(u16)(MASK_IFEN | MASK_LCD_CSEL), MASK_SPBP);

		i2c_wr16_and_or(sd, SPLITTX0_SPLIT,
				(~(MASK_TX1SEL | MASK_EHW)) & 0xffff, 0);
	} else {
		i2c_wr16_and_or(sd, SPLITTX0_CTRL,
				~(u16)(MASK_IFEN | MASK_LCD_CSEL | MASK_SPBP),
				0);
		i2c_wr16_and_or(sd, SPLITTX1_CTRL,
				~(u16)(MASK_IFEN | MASK_LCD_CSEL | MASK_SPBP),
				0);

		i2c_wr16_and_or(sd, SPLITTX0_SPLIT, ~(MASK_TX1SEL), MASK_EHW);
	}
}

static int get_hsck_freq(struct tc358840_platform_data *pdata)
{
	int hsck = (pdata->refclk_hz / (pdata->pll_prd + 1) *
		(pdata->pll_fbd + 1)) / (1 << pdata->pll_frs);
	return hsck;
}

static void tc358840_set_pll(struct v4l2_subdev *sd,
			     enum tc358840_csi_port port)
{
	struct tc358840_state *state = to_state(sd);
	struct tc358840_platform_data *pdata = &state->pdata;
	u16 base_addr;
	u32 pllconf;

	if (WARN_ON(pdata->csi_port <= CSI_TX_NONE ||
		    pdata->csi_port > CSI_TX_BOTH))
		pdata->csi_port = CSI_TX_NONE;

	if (pdata->csi_port == CSI_TX_NONE) {
		v4l2_err(sd, "%s: No CSI port defined!\n", __func__);
		return;
	}

	base_addr = (port == CSI_TX_0) ? CSITX0_BASE_ADDR :
					CSITX1_BASE_ADDR;
	pllconf = SET_PLL_PRD(pdata->pll_prd) | SET_PLL_FBD(pdata->pll_fbd) |
		SET_PLL_FRS(pdata->pll_frs);

	v4l2_dbg(1, debug, sd, "%s: Updating PLL clock of CSI TX%d, hsck=%d\n",
		 __func__, port - 1, get_hsck_freq(pdata));

	/* TODO: Set MP_LBW  ? */
	i2c_wr32_and_or(sd, base_addr + PLLCONF,
			~(u32)(MASK_PLL_PRD | MASK_PLL_FBD | MASK_PLL_FRS),
			pllconf);
}

static void tc358840_set_ref_clk(struct v4l2_subdev *sd)
{
	struct tc358840_state *state = to_state(sd);
	struct tc358840_platform_data *pdata = &state->pdata;

	u32 sys_freq;
	u32 lock_ref_freq;
	u16 fh_min;
	u16 fh_max;
	u32 cec_freq;
	u32 nco;
	u16 csc;

	if (WARN_ON(pdata->refclk_hz < 40000000 ||
		    pdata->refclk_hz > 50000000))
		pdata->refclk_hz = 42000000;

	/* System Frequency */
	sys_freq = pdata->refclk_hz / 10000;
	i2c_wr8(sd, SYS_FREQ0, sys_freq & 0x00ff);
	i2c_wr8(sd, SYS_FREQ1, (sys_freq & 0xff00) >> 8);

	/* Audio System Frequency */
	lock_ref_freq = pdata->refclk_hz / 100;
	i2c_wr8(sd, LOCK_REF_FREQA, lock_ref_freq & 0xff);
	i2c_wr8(sd, LOCK_REF_FREQB, (lock_ref_freq >> 8) & 0xff);
	i2c_wr8(sd, LOCK_REF_FREQC, (lock_ref_freq >> 16) & 0x0f);

	/* Audio PLL */
	i2c_wr8(sd, NCO_F0_MOD, MASK_NCO_F0_MOD_REG);
	/* 6.144 * 2^28 = 1649267442 */
	nco = (1649267442 / (pdata->refclk_hz / 1000000));
	i2c_wr8(sd, NCO_48F0A, nco & 0xff);
	i2c_wr8(sd, NCO_48F0B, (nco >> 8) & 0xff);
	i2c_wr8(sd, NCO_48F0C, (nco >> 16) & 0xff);
	i2c_wr8(sd, NCO_48F0D, (nco >> 24) & 0xff);
	/* 5.6448 * 2^28 = 1515264462 */
	nco = (1515264462 / (pdata->refclk_hz / 1000000));
	i2c_wr8(sd, NCO_44F0A, nco & 0xff);
	i2c_wr8(sd, NCO_44F0B, (nco >> 8) & 0xff);
	i2c_wr8(sd, NCO_44F0C, (nco >> 16) & 0xff);
	i2c_wr8(sd, NCO_44F0D, (nco >> 24) & 0xff);

	fh_min = pdata->refclk_hz / 100000;
	i2c_wr8(sd, FH_MIN0, fh_min & 0x00ff);
	i2c_wr8(sd, FH_MIN1, (fh_min & 0xff00) >> 8);

	fh_max = (fh_min * 66) / 10;
	i2c_wr8(sd, FH_MAX0, fh_max & 0x00ff);
	i2c_wr8(sd, FH_MAX1, (fh_max & 0xff00) >> 8);

	/* Color Space Conversion */
	csc = pdata->refclk_hz / 10000;
	i2c_wr8(sd, SCLK_CSC0, csc & 0xff);
	i2c_wr8(sd, SCLK_CSC1, (csc >> 8) & 0xff);

	/*
	 * Trial and error suggests that the default register value
	 * of 656 is for a 42 MHz reference clock. Use that to derive
	 * a new value based on the actual reference clock.
	 */
	cec_freq = (656 * sys_freq) / 4200;
	i2c_wr16(sd, CECHCLK, cec_freq);
	i2c_wr16(sd, CECLCLK, cec_freq);
}

static void tc358840_set_csi_mbus_config(struct v4l2_subdev *sd)
{
	struct tc358840_state *state = to_state(sd);

	v4l2_dbg(3, debug, sd, "%s():\n", __func__);

	switch (state->mbus_fmt_code) {
	case MEDIA_BUS_FMT_UYVY8_1X16:
		v4l2_dbg(2, debug, sd, "%s: YCbCr 422 16-bit\n", __func__);

		i2c_wr8(sd, VOUT_FMT, MASK_OUTFMT_422 | MASK_422FMT_NORMAL);
		i2c_wr8(sd, VOUT_FIL, MASK_422FIL_3_TAP_444 |
			MASK_444FIL_2_TAP);
		i2c_wr8(sd, VOUT_SYNC0, MASK_MODE_2);
		set_rgb_quantization_range(sd);
		i2c_wr16_and_or(sd, CONFCTL0, ~(MASK_YCBCRFMT),
				MASK_YCBCRFMT_YCBCR422_8);
		i2c_wr16(sd, CONFCTL1, 0x0);
		break;

	case MEDIA_BUS_FMT_RGB888_1X24:
		v4l2_dbg(2, debug, sd, "%s: RGB 888 24-bit\n", __func__);

		i2c_wr8(sd, VOUT_FMT, MASK_OUTFMT_444_RGB);
		i2c_wr8(sd, VOUT_FIL, MASK_422FIL_3_TAP_444 |
			MASK_444FIL_2_TAP);
		i2c_wr8(sd, VOUT_SYNC0, MASK_MODE_2);
		set_rgb_quantization_range(sd);
		i2c_wr16_and_or(sd, CONFCTL0, ~(MASK_YCBCRFMT), 0x0);
		i2c_wr16_and_or(sd, CONFCTL1, 0x0, MASK_TX_OUT_FMT_RGB888);
		break;

	default:
		v4l2_dbg(2, debug, sd, "%s: Unsupported format code 0x%x\n",
			 __func__, state->mbus_fmt_code);
		break;
	}
}

static unsigned int tc358840_num_csi_lanes_needed(struct v4l2_subdev *sd)
{
	/* Always use 4 lanes for one CSI */
	return 4;
}

static void tc358840_set_csi(struct v4l2_subdev *sd)
{
	struct tc358840_state *state = to_state(sd);
	struct tc358840_platform_data *pdata = &state->pdata;
	unsigned int lanes = tc358840_num_csi_lanes_needed(sd);
	enum tc358840_csi_port port;
	u16 base_addr;

	tc358840_reset(sd, MASK_CTXRST);

	for (port = CSI_TX_0; port <= CSI_TX_1; port++) {
		base_addr = (port == CSI_TX_0) ? CSITX0_BASE_ADDR :
						 CSITX1_BASE_ADDR;

		/* Test pattern must use TX1: enable it if pattern is active */
		if (pdata->csi_port != CSI_TX_BOTH &&
		    pdata->csi_port != port &&
		    !state->test_pattern) {
			v4l2_dbg(1, debug, sd, "%s: Disabling CSI TX%d\n",
				 __func__, port - 1);

			/* Disable CSI lanes (High Z) */
			i2c_wr32_and_or(sd, base_addr + LANEEN,
					~(u32)(MASK_CLANEEN), 0);
			continue;
		}

		v4l2_dbg(1, debug, sd,
			 "%s: Enabling CSI TX%d\n", __func__, port - 1);

		/* (0x0108) */
		i2c_wr32(sd, base_addr + CSITX_CLKEN, MASK_CSITX_EN);
		/*
		 * PLL has to be enabled between CSITX_CLKEN and
		 * LANEEN (0x02AC)
		 */
		tc358840_set_pll(sd, port);
		/* (0x02A0) */
		i2c_wr32_and_or(sd, base_addr + MIPICLKEN,
				~(MASK_MP_CKEN), MASK_MP_ENABLE);
		usleep_range(10000, 11000);
		/* (0x02A0) */
		i2c_wr32(sd, base_addr + MIPICLKEN,
			 MASK_MP_CKEN | MASK_MP_ENABLE);
		/* (0x010C) */
		i2c_wr32(sd, base_addr + PPICLKEN, MASK_HSTXCLKEN);
		/* (0x0118) */
		i2c_wr32(sd, base_addr + LANEEN,
			 (lanes & MASK_LANES) | MASK_CLANEEN);

		/* (0x0120) */
		i2c_wr32(sd, base_addr + LINEINITCNT, pdata->lineinitcnt);

		/* (0x0254) */
		i2c_wr32(sd, base_addr + LPTXTIMECNT, pdata->lptxtimecnt);
		/* (0x0258) */
		i2c_wr32(sd, base_addr + TCLK_HEADERCNT, pdata->tclk_headercnt);
		/* (0x025C) */
		i2c_wr32(sd, base_addr + TCLK_TRAILCNT, pdata->tclk_trailcnt);
		/* (0x0260) */
		i2c_wr32(sd, base_addr + THS_HEADERCNT, pdata->ths_headercnt);
		/* (0x0264) */
		i2c_wr32(sd, base_addr + TWAKEUP, pdata->twakeup);
		/* (0x0268) */
		i2c_wr32(sd, base_addr + TCLK_POSTCNT, pdata->tclk_postcnt);
		/* (0x026C) */
		i2c_wr32(sd, base_addr + THS_TRAILCNT, pdata->ths_trailcnt);
		/* (0x0270) */
		i2c_wr32(sd, base_addr + HSTXVREGCNT, pdata->hstxvregcnt);

		/* (0x0274) */
		i2c_wr32(sd, base_addr + HSTXVREGEN,
			 ((lanes > 0) ? MASK_CLM_HSTXVREGEN : 0x0) |
			 ((lanes > 0) ? MASK_D0M_HSTXVREGEN : 0x0) |
			 ((lanes > 1) ? MASK_D1M_HSTXVREGEN : 0x0) |
			 ((lanes > 2) ? MASK_D2M_HSTXVREGEN : 0x0) |
			 ((lanes > 3) ? MASK_D3M_HSTXVREGEN : 0x0));

		/*
		 * Finishing configuration by setting CSITX to start
		 * (0X011C)
		 */
		i2c_wr32(sd, base_addr + CSITX_START, 0x00000001);

		i2c_rd32(sd, base_addr + CSITX_INTERNAL_STAT);
	}
}

static void tc358840_set_hdmi_phy(struct v4l2_subdev *sd)
{
	/* Reset PHY */
	tc358840_reset_phy(sd);

	/* Enable PHY */
	i2c_wr8_and_or(sd, PHY_ENB, ~MASK_ENABLE_PHY, 0x0);
	i2c_wr8_and_or(sd, PHY_ENB, ~MASK_ENABLE_PHY, MASK_ENABLE_PHY);

	/* Enable Audio PLL */
	i2c_wr8(sd, APPL_CTL, MASK_APLL_CPCTL_NORMAL | MASK_APLL_ON);

	/* Enable DDC IO */
	i2c_wr8(sd, DDCIO_CTL, MASK_DDC_PWR_ON);
}

static void tc358840_set_hdmi_audio(struct v4l2_subdev *sd)
{
	i2c_wr8(sd, FORCE_MUTE, 0x00);
	i2c_wr8(sd, AUTO_CMD0, MASK_AUTO_MUTE7 | MASK_AUTO_MUTE6 |
			MASK_AUTO_MUTE5 | MASK_AUTO_MUTE4 |
			MASK_AUTO_MUTE1 | MASK_AUTO_MUTE0);
	i2c_wr8(sd, AUTO_CMD1, MASK_AUTO_MUTE9);
	i2c_wr8(sd, AUTO_CMD2, MASK_AUTO_PLAY3 | MASK_AUTO_PLAY2);
	i2c_wr8(sd, BUFINIT_START, SET_BUFINIT_START_MS(500));
	i2c_wr8(sd, FS_MUTE, 0x00);
	i2c_wr8(sd, FS_IMODE, MASK_NLPCM_SMODE | MASK_FS_SMODE);
	i2c_wr8(sd, ACR_MODE, MASK_CTS_MODE);
	i2c_wr8(sd, ACR_MDF0, MASK_ACR_L2MDF_1976_PPM | MASK_ACR_L1MDF_976_PPM);
	i2c_wr8(sd, ACR_MDF1, MASK_ACR_L3MDF_3906_PPM);
	/*
	 * TODO: Set output data bit length (currently 24 bit, no rounding)
	 */
	i2c_wr8(sd, SDO_MODE1, MASK_SDO_FMT_I2S | (6 << 4));
	i2c_wr8(sd, DIV_MODE, SET_DIV_DLY_MS(100));
	i2c_wr16_and_or(sd, CONFCTL0, 0xffff, MASK_AUDCHNUM_2 |
			MASK_AUDOUTSEL_I2S | MASK_AUTOINDEX);
}

static void tc358840_set_test_pattern_type(struct v4l2_subdev *sd,
					   int testpattern)
{
	u16 mask_type;

	switch (testpattern) {
	case TEST_PATTERN_COLOR_BAR:
		mask_type = MASK_CB_TYPE_COLOR_BAR;
		break;
	case TEST_PATTERN_COLOR_CHECKER:
		mask_type = MASK_CB_TYPE_COLOR_CHECKERS;
		break;
	default:
		return;
	}
	i2c_wr16_and_or(sd, CB_CTL, ~MASK_CB_TYPE, mask_type);
}

static int tc358840_set_test_pattern_timing(struct v4l2_subdev *sd,
					    struct v4l2_dv_timings *timings)
{
	struct tc358840_state *state = to_state(sd);
	struct v4l2_bt_timings *bt;
	int target_fps;
	int clk = get_hsck_freq(&state->pdata);
	int htot;
	int frame_height;
	int frame_width;
	/* Concatenated value for bytes/pixel, bits/clk.. */
	int clks_pr_pixel = 4;

	v4l2_dbg(3, debug, sd, "%s():\n", __func__);
	if (!timings)
		return -ERANGE;
	bt = &timings->bt;
	target_fps = fps(bt);
	if (!target_fps)
		return -ERANGE;
	frame_height = V4L2_DV_BT_FRAME_HEIGHT(bt);
	if (!frame_height)
		return -ERANGE;
	htot = ((clk / frame_height) / target_fps) / clks_pr_pixel;

	frame_width = V4L2_DV_BT_FRAME_WIDTH(bt);
	v4l2_dbg(3, debug, sd, "%s():htot=%d\n", __func__, htot);
	/*
	 * If bandwidth is too small: Keep timings, lower the fps
	 */
	if (htot < frame_width) {
		htot = frame_width;
		target_fps = (clk / frame_height) / (htot * clks_pr_pixel);
		v4l2_err(sd, "%s(): Bandwidth too small, fps will be %d\n",
			 __func__, target_fps);
	}

	i2c_wr16(sd, CB_HSW, bt->hsync);
	i2c_wr16(sd, CB_VSW, bt->vsync);
	i2c_wr16(sd, CB_HTOTAL, htot);
	i2c_wr16(sd, CB_VTOTAL, frame_height);
	i2c_wr16(sd, CB_HACT, bt->width);
	i2c_wr16(sd, CB_VACT, bt->height);
	i2c_wr16(sd, CB_HSTART, bt->hbackporch);
	i2c_wr16(sd, CB_VSTART, bt->vbackporch);

	return 0;
}

static int get_test_pattern_timing(struct v4l2_subdev *sd,
				   struct v4l2_dv_timings *timings)
{
	/* Return timings last set */
	struct tc358840_state *state = to_state(sd);
	*timings = state->timings;
	return 0;
}

static void tc358840_initial_setup(struct v4l2_subdev *sd)
{
	static struct v4l2_dv_timings default_timing =
		V4L2_DV_BT_CEA_1920X1080P60;
	struct tc358840_state *state = to_state(sd);
	struct tc358840_platform_data *pdata = &state->pdata;

	v4l2_dbg(3, debug, sd, "%s():\n", __func__);

	/* *** Reset *** */
	enable_stream(sd, false);

	tc358840_sleep_mode(sd, false);
	tc358840_reset(sd, MASK_RESET_ALL);

	tc358840_init_interrupts(sd);

	/* *** Init CSI *** */
	tc358840_s_dv_timings(sd, &default_timing);

	tc358840_set_ref_clk(sd);

	i2c_wr8_and_or(sd, DDC_CTL, ~(u8)(MASK_DDC_ACTION | MASK_DDC5V_MODE),
		       MASK_DDC_ACTION |
		       (pdata->ddc5v_delay & MASK_DDC5V_MODE));

	i2c_wr8_and_or(sd, EDID_MODE, ~(u8)MASK_EDID_MODE_ALL, MASK_RAM_EDDC);

	i2c_wr8_and_or(sd, HPD_CTL, ~(u8)MASK_HPD_CTL0, 0);
	state->eq_bypass = true;
	i2c_wr8_and_or(sd, EQ_BYPS, ~(u8)MASK_EQ_BYPS, MASK_EQ_BYPS);

	tc358840_set_hdmi_phy(sd);
	tc358840_set_hdmi_hdcp(sd, false);
	tc358840_set_hdmi_audio(sd);

	/* All CE and IT formats are detected as RGB full range in DVI mode */
	i2c_wr8_and_or(sd, VI_MODE, ~MASK_RGB_DVI, 0);
}

/* --------------- CEC --------------- */

#ifdef CONFIG_VIDEO_TC358840_CEC
static int tc358840_cec_adap_enable(struct cec_adapter *adap, bool enable)
{
	struct tc358840_state *state = adap->priv;
	struct v4l2_subdev *sd = &state->sd;

	i2c_wr32(sd, CECIMSK, enable ? MASK_CECTIM | MASK_CECRIM : 0);
	i2c_wr32(sd, CECICLR, MASK_CECTICLR | MASK_CECRICLR);
	i2c_wr32(sd, CECEN, enable);
	if (enable)
		i2c_wr32(sd, CECREN, MASK_CECREN);
	return 0;
}

static int tc358840_cec_adap_monitor_all_enable(struct cec_adapter *adap,
						bool enable)
{
	struct tc358840_state *state = adap->priv;
	struct v4l2_subdev *sd = &state->sd;
	u32 reg;

	reg = i2c_rd32(sd, CECRCTL1);
	if (enable)
		reg |= MASK_CECOTH;
	else
		reg &= ~MASK_CECOTH;
	i2c_wr32(sd, CECRCTL1, reg);
	return 0;
}

static int tc358840_cec_adap_log_addr(struct cec_adapter *adap, u8 log_addr)
{
	struct tc358840_state *state = adap->priv;
	struct v4l2_subdev *sd = &state->sd;
	unsigned int la = 0;

	if (log_addr != CEC_LOG_ADDR_INVALID) {
		la = i2c_rd32(sd, CECADD);
		la |= 1 << log_addr;
	}
	i2c_wr32(sd, CECADD, la);
	return 0;
}

static int tc358840_cec_adap_transmit(struct cec_adapter *adap, u8 attempts,
				      u32 signal_free_time, struct cec_msg *msg)
{
	struct tc358840_state *state = adap->priv;
	struct v4l2_subdev *sd = &state->sd;
	unsigned int i;

	i2c_wr32(sd, CECTCTL,
		 (cec_msg_is_broadcast(msg) ? MASK_CECBRD : 0) |
		 (signal_free_time - 1));
	for (i = 0; i < msg->len; i++)
		i2c_wr32(sd, CECTBUF1 + i * 4,
			 msg->msg[i] | ((i == msg->len - 1) ?
					MASK_CECTEOM : 0));
	i2c_wr32(sd, CECTEN, MASK_CECTEN);
	return 0;
}

static const struct cec_adap_ops tc358840_cec_adap_ops = {
	.adap_enable = tc358840_cec_adap_enable,
	.adap_log_addr = tc358840_cec_adap_log_addr,
	.adap_transmit = tc358840_cec_adap_transmit,
	.adap_monitor_all_enable = tc358840_cec_adap_monitor_all_enable,
};
#endif

static bool tc358840_match_dv_timings(const struct v4l2_dv_timings *t1,
				      const struct v4l2_dv_timings *t2,
				      unsigned int pclock_delta)
{
	if (t1->type != t2->type || t1->type != V4L2_DV_BT_656_1120)
		return false;
	if (t1->bt.width == t2->bt.width &&
	    t1->bt.height == t2->bt.height &&
	    t1->bt.interlaced == t2->bt.interlaced &&
	    t1->bt.polarities == t2->bt.polarities &&
	    t1->bt.pixelclock >= t2->bt.pixelclock - pclock_delta &&
	    t1->bt.pixelclock <= t2->bt.pixelclock + pclock_delta &&
	    V4L2_DV_BT_BLANKING_WIDTH(&t1->bt) ==
	    V4L2_DV_BT_BLANKING_WIDTH(&t2->bt) &&
	    V4L2_DV_BT_BLANKING_HEIGHT(&t1->bt) ==
	    V4L2_DV_BT_BLANKING_HEIGHT(&t2->bt))
		return true;
	return false;
}

/* --------------- IRQ --------------- */

/*
 * Background:
 *
 * Actual interrupts are only used for some audio, CEC and DDC handling.
 * For detecting video timing changes we use polling. This is done because
 * using interrupts for that proved flaky. In certain situations you could
 * get interrupt flooding and it was also possible to somehow stop interrupts
 * from arriving. It was never clear if that was due to hardware problems
 * or due to some bug in the interrupt handler.
 *
 * In the end the decision was made to switch to polling. This prevents the
 * interrupt flooding issue, and it also makes it easier to wait for the
 * signal to be stable (no need for additional workqueues).
 *
 * Another reason for using polling is the EQ_BYPS bit: this is a bit that
 * enables or disables the PHY equalizer in the chip. The recommendation is
 * that this bit is on, but unfortunately whether it should be on or off
 * depends on the HDMI cable that is used, and of course the driver has no
 * control over that.
 *
 * If the bit is in the 'wrong' position then you may never see a sync, or
 * only an intermittent sync, or the sync may glitch every so often.
 *
 * It is relatively easy to detect the first two scenarios in the polling code,
 * and it will toggle the bit if no stable sync can be achieved for three
 * seconds.
 *
 * For the glitches no solution is in place at the moment. The only thing I
 * can think of would be to time how long the glitch would take and if it
 * is a short glitch AND the old and newly detected timings are the same,
 * then assume EQ_BYPS has the wrong value.
 *
 * But I fear this is likely to introduce more problems than it solves.
 */

static const struct v4l2_event tc358840_ev_fmt = {
	.type = V4L2_EVENT_SOURCE_CHANGE,
	.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION,
};

static void tc358840_check_5v(struct v4l2_subdev *sd, bool tx_5v)
{
	struct tc358840_state *state = to_state(sd);

	if (tx_5v == state->have_5v)
		return;

	v4l2_dbg(1, debug, sd, "%s: Tx 5V power present: %s\n",
		 __func__, tx_5v ?  "yes" : "no");

	state->have_5v = tx_5v;
	if (tx_5v) {
		tc358840_enable_hpd(sd);
	} else {
		tc358840_enable_interrupts(sd, false);
		if (state->enabled)
			enable_stream(sd, false);
		state->eq_bypass = true;
		i2c_wr8_and_or(sd, EQ_BYPS, ~MASK_EQ_BYPS, MASK_EQ_BYPS);
		state->found_stable_signal = false;
		tc358840_disable_hpd(sd);
		memset(&state->timings, 0, sizeof(state->timings));
		v4l2_subdev_notify_event(sd, &tc358840_ev_fmt);
		v4l2_info(sd, "event: no 5V\n");
	}
	state->status = STATUS_FIND_SIGNAL;
}

static int tc358840_get_detected_timings(struct v4l2_subdev *sd,
					 struct v4l2_dv_timings *timings)
{
	struct v4l2_dv_timings t = { };
	struct v4l2_bt_timings *bt = &timings->bt;
	struct tc358840_state *state = to_state(sd);
	unsigned int width, height, frame_width, frame_height;
	unsigned int pol;
	u32 frame_interval, fps;

	memset(timings, 0, sizeof(struct v4l2_dv_timings));

	if (state->test_pattern)
		return get_test_pattern_timing(sd, timings);

	if (no_signal(sd)) {
		v4l2_dbg(1, debug, sd, "%s: no valid signal\n", __func__);
		return -ENOLINK;
	}
	if (no_sync(sd)) {
		v4l2_dbg(1, debug, sd, "%s: no sync on signal\n", __func__);
		return -ENOLCK;
	}

	timings->type = V4L2_DV_BT_656_1120;

	bt->interlaced = i2c_rd8(sd, VI_STATUS1) &
		MASK_S_V_INTERLACE ? V4L2_DV_INTERLACED : V4L2_DV_PROGRESSIVE;

	width = ((i2c_rd8(sd, DE_HSIZE_HI) & 0x1f) << 8) +
		i2c_rd8(sd, DE_HSIZE_LO);
	height = ((i2c_rd8(sd, DE_VSIZE_HI) & 0x1f) << 8) +
		i2c_rd8(sd, DE_VSIZE_LO);
	frame_width = ((i2c_rd8(sd, IN_HSIZE_HI) & 0x1f) << 8) +
		i2c_rd8(sd, IN_HSIZE_LO);
	frame_height = (((i2c_rd8(sd, IN_VSIZE_HI) & 0x3f) << 8) +
		i2c_rd8(sd, IN_VSIZE_LO)) / 2;
	pol = i2c_rd8(sd, CLK_STATUS);

	/*
	 * Frame interval in milliseconds * 10
	 * Require SYS_FREQ0 and SYS_FREQ1 are precisely set
	 */
	frame_interval = ((i2c_rd8(sd, FV_CNT_HI) & 0x3) << 8) +
		i2c_rd8(sd, FV_CNT_LO);
	fps = (frame_interval > 0) ? 1000000 / frame_interval : 0;

	bt->width = width;
	bt->height = height;
	bt->vsync = frame_height - height;
	bt->hsync = frame_width - width;
	bt->pixelclock = DIV_ROUND_CLOSEST(((u64)frame_width * frame_height * fps)
					   / 100, 1000) * 1000;

	if (pol & MASK_S_V_HPOL)
		bt->polarities |= V4L2_DV_HSYNC_POS_POL;
	if (pol & MASK_S_V_VPOL)
		bt->polarities |= V4L2_DV_VSYNC_POS_POL;
	if (bt->interlaced == V4L2_DV_INTERLACED) {
		bt->height *= 2;
		bt->il_vsync = bt->vsync + 1;
		bt->pixelclock >>= 1;
	}
	/* Sanity check */
	if (bt->width < 640 || bt->height < 480 ||
	    (bt->width & 1) || (bt->height & 1) ||
	    (frame_width & 1) || frame_width <= width ||
	    frame_height <= height) {
		memset(timings, 0, sizeof(struct v4l2_dv_timings));
		return -ENOLCK;
	}
	if (t.bt.width &&
	    !tc358840_match_dv_timings(timings, &t, 250000)) {
		memset(timings, 0, sizeof(struct v4l2_dv_timings));
		return -ENOLCK;
	}
	return 0;
}

#define MIN_FOUND_SIGNAL_CNT 3

/*
 * Try to find a signal by toggling the EQ_BYPS bit (unless overridden by the
 * control). Counters are kept to record is a signal was seen for the current
 * eq_bypass value. If no signal was seen, then the counter is reset to 0.
 * The first counter that reaches MIN_FOUND_SIGNAL_CNT 'wins'. However, if
 * a signal was seen for both EQ_BYPS values, then EQ_BYPS=1 wins since that
 * is the recommended value according to Toshiba.
 *
 * We also try to remember which EQ_BYPS value didn't work so we can avoid
 * further toggling in the future.
 */
static void tc358840_find_signal(struct v4l2_subdev *sd, bool have_signal)
{
	struct tc358840_state *state = to_state(sd);
	int eq_byps_mode = state->eq_byps_mode_ctrl->val;

	if (eq_byps_mode && eq_byps_mode - 1 != state->eq_bypass)
		goto toggle;

	if (have_signal) {
		state->found_signal_cnt[state->eq_bypass]++;
	} else {
		if (state->found_signal_cnt[state->eq_bypass])
			tc358840_reset_phy(sd);
		state->found_signal_cnt[state->eq_bypass] = 0;
	}

	if (state->found_signal_cnt[state->eq_bypass] >= MIN_FOUND_SIGNAL_CNT) {
		/* EQ_BYPS=1 is recommended by Toshiba, if possible */
		bool new_eq_bypass = state->found_signal_cnt[1] >=
					MIN_FOUND_SIGNAL_CNT - 1;

		if (state->invalid_eq_bypass[new_eq_bypass] &&
		    state->found_signal_cnt[!new_eq_bypass])
			new_eq_bypass = !new_eq_bypass;
		if (eq_byps_mode)
			new_eq_bypass = eq_byps_mode - 1;

		state->status = STATUS_FOUND_SIGNAL;
		v4l2_info(sd, "found signal for EQ_BYPS=%d, counters: %d, %d invalid: %d, %d\n",
			  new_eq_bypass,
			  state->found_signal_cnt[0],
			  state->found_signal_cnt[1],
			  state->invalid_eq_bypass[0],
			  state->invalid_eq_bypass[1]);
		if (state->found_signal_cnt[0] == 0)
			v4l2_info(sd, "mark EQ_BYPS 0 as invalid\n");
		if (state->found_signal_cnt[1] == 0)
			v4l2_info(sd, "mark EQ_BYPS 1 as invalid\n");
		state->invalid_eq_bypass[0] = state->invalid_eq_bypass[0] ||
			state->found_signal_cnt[0] == 0;
		state->invalid_eq_bypass[1] = state->invalid_eq_bypass[1] ||
			state->found_signal_cnt[1] == 0;
		if (state->invalid_eq_bypass[new_eq_bypass]) {
			/*
			 * This used to be invalid, but we now see a signal
			 * for new_eq_bypass and not for !new_eq_bypass, so
			 * we just hope for the best and mark this as valid
			 * again.
			 */
			v4l2_info(sd, "mark EQ_BYPS %d as valid again\n",
				  new_eq_bypass);
			state->invalid_eq_bypass[new_eq_bypass] = false;
		}
		state->dropped_signal_cnt[0] = 0;
		state->dropped_signal_cnt[1] = 0;
		if (new_eq_bypass == state->eq_bypass)
			return;
	}

	if (eq_byps_mode)
		return;

toggle:
	state->eq_bypass = !state->eq_bypass;
	i2c_wr8_and_or(sd, EQ_BYPS, ~MASK_EQ_BYPS,
		       state->eq_bypass ? MASK_EQ_BYPS : 0);
	tc358840_reset_phy(sd);
}

#define MAX_DROPPED_SIGNAL_CNT 3
#define MAX_FOUND_SIGNAL_CNT 15

/*
 * We wait until we got a valid signal for MAX_FOUND_SIGNAL_CNT consecutive
 * calls. If found, then we consider this a stable signal, ready for
 * applications to use. If the signal is lost, then we see if we can toggle
 * EQ_BYPS (if the eq_byps_mode control allows that). If not, then we go back
 * to the FIND_SIGNAL phase.
 */
static void tc358840_found_signal(struct v4l2_subdev *sd, bool have_signal,
				  unsigned int format_change)
{
	struct tc358840_state *state = to_state(sd);
	int eq_byps_mode = state->eq_byps_mode_ctrl->val;

	if (!have_signal) {
		state->dropped_signal_cnt[state->eq_bypass]++;

		v4l2_info(sd,
			  "dropped signal count for EQ_BYPS %d is %d (%x)\n",
			  state->eq_bypass,
			  state->dropped_signal_cnt[state->eq_bypass],
			  format_change);
		if (!eq_byps_mode &&
		    !state->invalid_eq_bypass[!state->eq_bypass] &&
		    !state->dropped_signal_cnt[!state->eq_bypass]) {
			state->eq_bypass = !state->eq_bypass;
			v4l2_info(sd, "switch EQ_BYPS to %d\n",
				  state->eq_bypass);
			i2c_wr8_and_or(sd, EQ_BYPS, ~MASK_EQ_BYPS,
				       state->eq_bypass ? MASK_EQ_BYPS : 0);
			tc358840_reset_phy(sd);
			return;
		}

		if (state->dropped_signal_cnt[state->eq_bypass] >=
		    MAX_DROPPED_SIGNAL_CNT) {
			v4l2_info(sd, "back to find_signal\n");
			state->dropped_signal_cnt[state->eq_bypass] = 0;
			state->status = STATUS_FIND_SIGNAL;
			state->found_signal_cnt[0] = 0;
			state->found_signal_cnt[1] = 0;
			tc358840_reset_phy(sd);
		}
		return;
	}
	state->found_signal_cnt[state->eq_bypass]++;
	if (state->found_signal_cnt[state->eq_bypass] >= MAX_FOUND_SIGNAL_CNT) {
		v4l2_info(sd, "stable signal %d\n", state->eq_bypass);
		state->status = STATUS_STABLE_SIGNAL;
		set_rgb_quantization_range(sd);
	}
}

/*
 * A stable signal was found, so notify userspace (if not already done).
 * If the stable signal was lost, then drop back to FIND_SIGNAL. and inform
 * userspace.
 *
 * It makes no sense to try and toggle EQ_BYPS if this happens since you have
 * no idea if the signal drop is because of e.g. a resolution change, or if
 * the cable is bad, or the source simply stops transmitting.
 */
static void tc358840_format_change(struct v4l2_subdev *sd, bool have_signal,
				   unsigned int format_change)
{
	struct tc358840_state *state = to_state(sd);

	if (!have_signal || format_change) {
		state->status = STATUS_FIND_SIGNAL;
		if (state->enabled)
			enable_stream(sd, false);
		state->found_signal_cnt[0] = 0;
		state->found_signal_cnt[1] = 0;
		if (state->found_stable_signal) {
			v4l2_subdev_notify_event(sd, &tc358840_ev_fmt);
			if (!have_signal)
				v4l2_info(sd, "event: stable signal was lost\n");
			else
				v4l2_info(sd, "event: stable signal became unstable\n");
		} else {
			v4l2_info(sd, "stable signal was lost (%d, %x)\n",
				  have_signal, format_change);
		}
		state->found_stable_signal = false;
		tc358840_reset_phy(sd);
		return;
	}

	if (state->found_stable_signal) {
		u32 data_idle_ctr = 100;
		u32 v;

		if (!state->test_dl)
			return;
		/*
		 * Check that the data lanes are still active.
		 * Due to unknown reasons this sometimes fails for the first
		 * frame only so we retry to enabling streaming if we detect
		 * that the data lanes stay idle, even though we had active
		 * lanes when we started streaming originally.
		 */
		state->test_dl = false;

		/* Wait for the data lines to become busy */
		while (!((v = i2c_rd32(sd, CSITX_INTERNAL_STAT)) & PPI_DL_BUSY) &&
		       data_idle_ctr) {
			usleep_range(100, 1000);
			data_idle_ctr--;
		}
		if (data_idle_ctr == 0) {
			v4l2_dbg(1, debug, sd,
				 "CSI TX internal stat 0x%02x, counter %d (retry)\n",
				 v, data_idle_ctr);
			enable_stream(sd, false);
			enable_stream(sd, true);
		}
		return;
	}

	if (state->eq_bypass)
		v4l2_print_dv_timings(sd->name,
				      "event: resolution change (EQ_BYPS=1): ",
				      &state->detected_timings, debug);
	else
		v4l2_print_dv_timings(sd->name,
				      "event: resolution change (EQ_BYPS=0): ",
				      &state->detected_timings, debug);

	state->found_stable_signal = true;
	v4l2_subdev_notify_event(sd, &tc358840_ev_fmt);
}

static void tc358840_delayed_work_poll(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct tc358840_state *state = container_of(dwork,
		struct tc358840_state, delayed_work_poll);
	struct v4l2_subdev *sd = &state->sd;
	u8 sys_status;
	bool have_5v;
	bool have_tmds;
	bool have_sync;
	bool have_signal;
	bool have_hdcp;
	unsigned int format_change = 0;

	mutex_lock(&state->lock);

	sys_status = i2c_rd8(sd, SYS_STATUS);
	have_5v = state->test_pattern || (sys_status & MASK_S_DDC5V);
	have_tmds = state->test_pattern || (sys_status & MASK_S_TMDS);
	have_sync = state->test_pattern || (sys_status & MASK_S_SYNC);
	have_signal = have_5v && have_tmds && have_sync;
	have_hdcp = have_signal ? (sys_status & MASK_S_HDCP) : false;

	tc358840_check_5v(sd, have_5v);
	if (have_hdcp) {
		/*
		 * MASK_S_HDCP can be 1 for a short time when connecting
		 * or disconnecting: the HDCP authentication will kick in
		 * when that happens and thus MASK_S_HDCP becomes 1 until
		 * the authentication fails and it is cleared again.
		 *
		 * So if it is set, then double check that the HDCP_MODE
		 * is in manual authentication mode, because then we know
		 * the authentication will fail and this is just a glitch.
		 *
		 * Don't rely on some global setting, actually read the
		 * register here. This in case someone has been hacking.
		 */
		have_hdcp = !(i2c_rd8(sd, HDCP_MODE) &
			      MASK_MANUAL_AUTHENTICATION);
	}

	if (!have_5v) {
		state->invalid_eq_bypass[0] = false;
		state->invalid_eq_bypass[1] = false;
	}
	if (state->test_pattern) {
		state->found_stable_signal = true;
		state->status = STATUS_STABLE_SIGNAL;
	} else {
		u8 sys_int = i2c_rd8(sd, SYS_INT);
		u8 clk_int = i2c_rd8(sd, CLK_INT);
		u8 misc_int = i2c_rd8(sd, MISC_INT);
		bool was_stable = state->found_stable_signal;

		i2c_wr8(sd, MISC_INT, 0xff);
		i2c_wr8(sd, CLK_INT, 0xff);
		i2c_wr8(sd, SYS_INT, 0xff);
		v4l2_dbg(2, debug, sd,
			 "%s: sys_stat: %02x misc_int: %02x clk_int: %02x sys_int: %02x eq_byps: %s\n",
			 __func__, sys_status, misc_int, clk_int, sys_int,
			 state->eq_bypass ? "on" : "off");

		if (!have_sync)
			format_change |= 1;
		if (sys_int & (MASK_DVI | MASK_HDMI))
			format_change |= 2;
		if (clk_int & (MASK_IN_DE_CHG))
			format_change |= 4;
		if (misc_int & (MASK_SYNC_CHG))
			format_change |= 8;

		if (sys_int & MASK_HDMI) {
			v4l2_dbg(1, debug, sd, "%s: DVI->HDMI change detected\n",
				 __func__);

			i2c_wr8(sd, APPL_CTL, MASK_APLL_CPCTL_NORMAL | MASK_APLL_ON);
		}
		if (sys_int & MASK_DVI) {
			v4l2_dbg(1, debug, sd, "%s: HDMI->DVI change detected\n",
				 __func__);
		}
		if (!have_signal) {
			if (state->status != STATUS_FIND_SIGNAL)
				v4l2_info(sd, "no signal for EQ_BYPS %d\n",
					  state->eq_bypass);
			state->status = STATUS_FIND_SIGNAL;
			state->found_signal_cnt[0] = 0;
			state->found_signal_cnt[1] = 0;
			if (state->enabled)
				enable_stream(sd, false);
			if (state->found_stable_signal) {
				state->found_stable_signal = false;
				v4l2_subdev_notify_event(sd, &tc358840_ev_fmt);
				v4l2_info(sd, "event: no signal (SYS_STATUS 0x%02x)\n",
					  sys_status);
			}
			state->detected_timings.bt.width = 0;
		}
		/*
		 * Reset the HDMI PHY to try to trigger proper lock on the
		 * incoming video format. Erase BKSV to prevent that old keys
		 * are used when a new source is connected.
		 */
		if (have_5v && (format_change & 0xa) && !have_signal) {
			if (have_tmds)
				v4l2_dbg(was_stable ? 0 : 1, debug, sd,
					 "reset due to format_change (%x)\n",
					 format_change);
			tc358840_reset_phy(sd);
		}
	}

	if (have_signal) {
		struct v4l2_dv_timings timings = { };

		/*
		 * If we don't have detected_timings, or something changed, then
		 * detect timings.
		 */
		if (format_change || !state->detected_timings.bt.width)
			tc358840_get_detected_timings(sd, &timings);
		else
			timings = state->detected_timings;

		/*
		 * Store newly detected timings (if any) if we detect timings
		 * for the first time, or if the sync toggled, or if the
		 * DVI/HDMI mode changed.
		 */
		if (!state->detected_timings.bt.width || (format_change & 3)) {
			state->detected_timings = timings;
		/*
		 * Silently accept SYNC/DE changes if the timings stay the same,
		 * i.e. if only blanking changed.
		 */
		} else if (format_change &&
			   tc358840_match_dv_timings(&state->detected_timings,
						     &timings, 250000)) {
			v4l2_dbg(1, debug, sd, "ignore timings change\n");
			format_change = 0;
		} else {
			state->detected_timings = timings;
		}
		have_signal = state->detected_timings.bt.width;
	}

	if (!state->test_pattern && have_5v) {
		switch (state->status) {
		case STATUS_FIND_SIGNAL:
			tc358840_find_signal(sd, have_signal);
			break;
		case STATUS_FOUND_SIGNAL:
			tc358840_found_signal(sd, have_signal, format_change);
			break;
		case STATUS_STABLE_SIGNAL:
			tc358840_format_change(sd, have_signal, format_change);
			break;
		}
	}
	mutex_unlock(&state->lock);
	v4l2_ctrl_s_ctrl(state->tmds_present_ctrl, have_signal);
	v4l2_ctrl_s_ctrl(state->detect_tx_5v_ctrl, have_5v);
	v4l2_ctrl_s_ctrl(state->audio_present_ctrl, audio_present(sd));

	if (have_signal)
		v4l2_ctrl_s_ctrl(state->audio_sampling_rate_ctrl,
				 get_audio_sampling_rate(sd));

	if (!state->delayed_work_stop_polling)
		schedule_delayed_work(&state->delayed_work_poll, POLL_PERIOD);
}

static void tc358840_init_interrupts(struct v4l2_subdev *sd)
{
	u16 i;

	v4l2_dbg(3, debug, sd, "%s():\n", __func__);

	i2c_wr16(sd, INTMASK, MASK_INT_STATUS_MASK_ALL);

	/* clear interrupt status registers */
	for (i = SYS_INT; i <= MISC_INT; i++) {
		/* No interrupt register at Address 0x850A */
		if (i != 0x850A) {
			/* Mask interrupts */
			i2c_wr8(sd, i + 0x10, 0xff);
			/* Clear any pending interrupts */
			i2c_wr8(sd, i, 0xff);
		}
	}

	/* Clear any pending interrupts */
	i2c_wr16(sd, INTSTATUS, MASK_INT_STATUS_MASK_ALL);
}

static void tc358840_enable_interrupts(struct v4l2_subdev *sd,
				       bool cable_connected)
{
	v4l2_dbg(2, debug, sd, "%s: cable connected = %d\n", __func__,
		 cable_connected);

	i2c_wr8(sd, SYS_INTM, ~MASK_DDC & 0xff);
	if (cable_connected) {
		if (no_signal(sd) || no_sync(sd)) {
			i2c_wr8(sd, CBIT_INTM,
				~(MASK_AF_LOCK | MASK_AF_UNLOCK) & 0xff);
			i2c_wr8(sd, AUDIO_INTM, 0xff);
		} else {
			i2c_wr8(sd, CBIT_INTM, ~(MASK_CBIT_FS | MASK_AF_LOCK |
						 MASK_AF_UNLOCK) & 0xff);
			i2c_wr8(sd, AUDIO_INTM, ~MASK_BUFINIT_END);
		}
	} else {
		i2c_wr8(sd, CBIT_INTM, 0xff);
		i2c_wr8(sd, AUDIO_INTM, 0xff);
		i2c_wr8(sd, HDCP_INTM, 0xff);
	}
}

static void tc358840_hdmi_audio_int_handler(struct v4l2_subdev *sd,
					    bool *handled)
{
	u8 audio_int_mask = i2c_rd8(sd, AUDIO_INTM);
	u8 audio_int = i2c_rd8(sd, AUDIO_INT);

	i2c_wr8(sd, AUDIO_INT, audio_int);
	audio_int &= ~audio_int_mask;

	v4l2_dbg(3, debug, sd, "%s: AUDIO_INT = 0x%02x\n", __func__, audio_int);

	if (audio_int & MASK_BUFINIT_END) {
		v4l2_dbg(1, debug, sd, "%s: Audio BUFINIT_END\n", __func__);
		audio_int &= ~MASK_BUFINIT_END;
		if (handled)
			*handled = true;
	}

	if (audio_int) {
		v4l2_err(sd, "%s: Unhandled AUDIO_INT interrupts: 0x%02x\n",
			 __func__, audio_int);
	}
}

static void tc358840_hdmi_hdcp_int_handler(struct v4l2_subdev *sd,
					   bool *handled)
{
	u8 hdcp_int_mask = i2c_rd8(sd, HDCP_INTM);
	u8 hdcp_int = i2c_rd8(sd, HDCP_INT);

	i2c_wr8(sd, HDCP_INT, hdcp_int);
	hdcp_int &= ~hdcp_int_mask;

	v4l2_dbg(3, debug, sd, "%s: HDCP_INT = 0x%02x\n", __func__, hdcp_int);

	if (hdcp_int) {
		v4l2_err(sd, "%s: Unhandled HDCP_INT interrupts: 0x%02x\n",
			 __func__, hdcp_int);
	}
}

static void tc358840_hdmi_cbit_int_handler(struct v4l2_subdev *sd,
					   bool *handled)
{
	u8 cbit_int_mask = i2c_rd8(sd, CBIT_INTM);
	u8 cbit_int = i2c_rd8(sd, CBIT_INT);

	i2c_wr8(sd, CBIT_INT, cbit_int);
	cbit_int &= ~cbit_int_mask;

	v4l2_dbg(3, debug, sd, "%s: CBIT_INT = 0x%02x\n", __func__, cbit_int);

	if (cbit_int & MASK_CBIT_FS) {
		v4l2_dbg(1, debug, sd, "%s: Audio sample rate changed\n",
			 __func__);

		cbit_int &= ~MASK_CBIT_FS;
		if (handled)
			*handled = true;
	}

	if (cbit_int & (MASK_AF_LOCK | MASK_AF_UNLOCK)) {
		v4l2_dbg(1, debug, sd, "%s: Audio present changed\n",
			 __func__);
		cbit_int &= ~(MASK_AF_LOCK | MASK_AF_UNLOCK);
		if (handled)
			*handled = true;
	}

	if (cbit_int)
		v4l2_err(sd, "%s: Unhandled CBIT_INT interrupts: 0x%02x\n",
			 __func__, cbit_int);
}

static void tc358840_hdmi_sys_int_handler(struct v4l2_subdev *sd, bool *handled)
{
	u8 sys_int_mask = i2c_rd8(sd, SYS_INTM);
	u8 sys_int = i2c_rd8(sd, SYS_INT);

	i2c_wr8(sd, SYS_INT, sys_int);
	sys_int &= ~sys_int_mask;

	v4l2_dbg(3, debug, sd, "%s: SYS_INT = 0x%02x\n", __func__, sys_int);

	if (sys_int & MASK_DDC) {
		tc358840_check_5v(sd, tx_5v_power_present(sd));

		sys_int &= ~MASK_DDC;
		if (handled)
			*handled = true;
	}

	if (sys_int)
		v4l2_err(sd, "%s: Unhandled SYS_INT interrupts: 0x%02x\n",
			 __func__, sys_int);
}

/* --------------- CORE OPS --------------- */

static int tc358840_isr(struct v4l2_subdev *sd, u32 status, bool *handled)
{
	struct tc358840_state *state = to_state(sd);
	u16 intstatus, clrstatus = 0;
	int retry = 10;
	int i;

	mutex_lock(&state->lock);
	intstatus = i2c_rd16(sd, INTSTATUS);

	while (intstatus && retry--) {
		v4l2_dbg(1, debug, sd,
			 "%s: intstatus = 0x%04x\n", __func__, intstatus);

		if (intstatus & MASK_HDMI_INT) {
			u8 hdmi_int1;

			hdmi_int1 = i2c_rd8(sd, HDMI_INT1);

			if (hdmi_int1 & MASK_ACBIT)
				tc358840_hdmi_cbit_int_handler(sd, handled);
			if (hdmi_int1 & MASK_SYS)
				tc358840_hdmi_sys_int_handler(sd, handled);
			if (hdmi_int1 & MASK_AUD)
				tc358840_hdmi_audio_int_handler(sd, handled);
			if (hdmi_int1 & MASK_HDCP)
				tc358840_hdmi_hdcp_int_handler(sd, handled);

			clrstatus |= MASK_HDMI_INT;
		}

#ifdef CONFIG_VIDEO_TC358840_CEC
		if (intstatus & (MASK_CEC_RINT | MASK_CEC_TINT)) {
			unsigned int cec_rxint, cec_txint;
			unsigned int clr;

			cec_rxint = i2c_rd32(sd, CECRSTAT);
			cec_txint = i2c_rd32(sd, CECTSTAT);

			clr = 0;
			if (intstatus & MASK_CEC_RINT)
				clr |= MASK_CECRICLR;
			if (intstatus & MASK_CEC_TINT)
				clr |= MASK_CECTICLR;
			i2c_wr32(sd, CECICLR, clr);

			if ((intstatus & MASK_CEC_TINT) && cec_txint) {
				if (cec_txint & MASK_CECTIEND)
					cec_transmit_done(state->cec_adap,
							  CEC_TX_STATUS_OK,
							  0, 0, 0, 0);
				else if (cec_txint & MASK_CECTIAL)
					cec_transmit_done(state->cec_adap,
							  CEC_TX_STATUS_ARB_LOST,
							  1, 0, 0, 0);
				else if (cec_txint & MASK_CECTIACK)
					cec_transmit_done(state->cec_adap,
							  CEC_TX_STATUS_NACK,
							  0, 1, 0, 0);
				else if (cec_txint & MASK_CECTIUR) {
					/*
					 * Not sure when this bit is set. Treat
					 * it as an error for now.
					 */
					cec_transmit_done(state->cec_adap,
							  CEC_TX_STATUS_ERROR,
							  0, 0, 0, 1);
				}
				*handled = true;
			}
			if ((intstatus & MASK_CEC_RINT) &&
			    (cec_rxint & MASK_CECRIEND)) {
				struct cec_msg msg = {};
				unsigned int i;
				unsigned int v;

				v = i2c_rd32(sd, CECRCTR);
				msg.len = v & 0x1f;
				for (i = 0; i < msg.len; i++) {
					v = i2c_rd32(sd, CECRBUF1 + i * 4);
					msg.msg[i] = v & 0xff;
				}
				cec_received_msg(state->cec_adap, &msg);
				*handled = true;
			}
			clrstatus |=
				intstatus & (MASK_CEC_RINT | MASK_CEC_TINT);
		}
#endif

		if (intstatus & MASK_CSITX0_INT) {
			v4l2_dbg(3, debug, sd,
				 "%s: MASK_CSITX0_INT\n", __func__);

			clrstatus |= MASK_CSITX0_INT;
		}

		if (intstatus & MASK_CSITX1_INT) {
			v4l2_dbg(3, debug, sd,
				 "%s: MASK_CSITX1_INT\n", __func__);

			clrstatus |= MASK_CSITX1_INT;
		}

		if (clrstatus)
			i2c_wr16(sd, INTSTATUS, clrstatus);

		if (intstatus & ~clrstatus)
			v4l2_dbg(1, debug, sd,
				 "%s: Unhandled intstatus interrupts: 0x%04x\n",
				 __func__, intstatus & ~clrstatus);
		intstatus = i2c_rd16(sd, INTSTATUS);

		if (intstatus)
			v4l2_dbg(1, debug, sd,
				 "%s: retry %d intstatus = 0x%04x\n",
				 __func__, 10 - retry, intstatus);
	}
	if (intstatus == 0)
		goto unlock;

	v4l2_err(sd, "unprocessed interrupts: 0x%04x\n", intstatus);

	if (!(intstatus & MASK_HDMI_INT))
		goto unlock;

	/*
	 * If intstatus != 0, then one or more HDMI interrupts are still
	 * pending.
	 */
	for (i = 0x8502; i <= 0x850b; i++) {
		u8 irqs;

		if (i == 0x850a)
			continue;
		irqs = i2c_rd8(sd, i) & ~i2c_rd8(sd, i + 0x10);
		if (irqs) {
			v4l2_err(sd, "runaway irqs 0x%02x in reg 0x%x\n",
				 irqs, i);
			/* Mask this runaway interrupt */
			i2c_wr8(sd, i + 0x10, i2c_rd8(sd, i + 0x10) | irqs);
		}
	}
	/* Hopefully this interrupt can now be cleared */
	i2c_wr16(sd, INTSTATUS, MASK_HDMI_INT);
	/* Reset the phys as well */
	tc358840_reset_phy(sd);
	tc358840_enable_interrupts(sd, tx_5v_power_present(sd));
unlock:
	mutex_unlock(&state->lock);
	return 0;
}

static irqreturn_t tc358840_irq_handler(int irq, void *dev_id)
{
	struct v4l2_subdev *sd = dev_id;
	bool handled = false;

	tc358840_isr(sd, 0, &handled);

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

/* --------------- PAD OPS --------------- */

static void tc358840_get_csi_color_space(struct v4l2_subdev *sd,
					 struct v4l2_mbus_framefmt *format)
{
	u8 vi_status3 = i2c_rd8(sd, VI_STATUS3);

	switch (vi_status3 & MASK_S_V_COLOR) {
	case MASK_RGB_FULL:
	case MASK_RGB_LIMITED:
	case MASK_SYCC601_FULL:
	case MASK_SYCC601_LIMITED:
	default:
		format->colorspace = V4L2_COLORSPACE_SRGB;
		format->xfer_func = V4L2_XFER_FUNC_SRGB;
		break;
	case MASK_YCBCR601_FULL:
	case MASK_YCBCR601_LIMITED:
	case MASK_XVYCC601_FULL:
	case MASK_XVYCC601_LIMITED:
		format->colorspace = V4L2_COLORSPACE_SMPTE170M;
		format->xfer_func = V4L2_XFER_FUNC_709;
		break;
	case MASK_YCBCR709_FULL:
	case MASK_YCBCR709_LIMITED:
	case MASK_XVYCC709_FULL:
	case MASK_XVYCC709_LIMITED:
		format->colorspace = V4L2_COLORSPACE_REC709;
		format->xfer_func = V4L2_XFER_FUNC_709;
		break;
	case MASK_OPRGB_FULL:
	case MASK_OPRGB_LIMITED:
	case MASK_OPYCC601_FULL:
	case MASK_OPYCC601_LIMITED:
		format->colorspace = V4L2_COLORSPACE_OPRGB;
		format->xfer_func = V4L2_XFER_FUNC_OPRGB;
		break;
	}

	switch (format->code) {
	case MEDIA_BUS_FMT_RGB888_1X24:
	default:
		format->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
		format->quantization = V4L2_QUANTIZATION_FULL_RANGE;
		break;
	case MEDIA_BUS_FMT_UYVY8_1X16:
		format->ycbcr_enc = V4L2_YCBCR_ENC_601;
		format->quantization = V4L2_QUANTIZATION_LIM_RANGE;
		break;
	}
}

static int tc358840_get_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *sd_state,
			    struct v4l2_subdev_format *format)
{
	struct tc358840_state *state = to_state(sd);
	struct v4l2_mbus_framefmt *fmt = &format->format;

	v4l2_dbg(3, debug, sd, "%s():\n", __func__);

	if (format->pad != 0)
		return -EINVAL;

	fmt->code = state->mbus_fmt_code;
	fmt->width = state->timings.bt.width;
	fmt->height = state->timings.bt.height;
	fmt->field = V4L2_FIELD_NONE;
	mutex_lock(&state->lock);
	tc358840_get_csi_color_space(sd, fmt);
	mutex_unlock(&state->lock);

	v4l2_dbg(3, debug, sd,
		 "%s(): width=%d, height=%d, code=0x%08X, field=%d\n",
		 __func__, fmt->width, fmt->height, fmt->code, fmt->field);

	return 0;
}

static int tc358840_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *sd_state,
			    struct v4l2_subdev_format *format)
{
	struct tc358840_state *state = to_state(sd);
	u32 code = format->format.code; /* is overwritten by get_fmt */
	int ret = tc358840_get_fmt(sd, sd_state, format);

	v4l2_dbg(3, debug, sd, "%s():\n", __func__);

	format->format.code = code;

	if (ret)
		return ret;

	switch (code) {
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_UYVY8_1X16:
		break;
	default:
		return -EINVAL;
	}
	mutex_lock(&state->lock);
	tc358840_get_csi_color_space(sd, &format->format);

	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		goto unlock;

	v4l2_dbg(3, debug, sd, "%s(): format->which=%d\n",
		 __func__, format->which);

	if (state->mbus_fmt_code == format->format.code)
		goto unlock;

	state->mbus_fmt_code = format->format.code;
	enable_stream(sd, false);
	tc358840_set_csi(sd);
	tc358840_set_csi_mbus_config(sd);
unlock:
	mutex_unlock(&state->lock);

	return 0;
}

static int tc358840_g_edid(struct v4l2_subdev *sd,
			   struct v4l2_subdev_edid *edid)
{
	struct tc358840_state *state = to_state(sd);

	v4l2_dbg(3, debug, sd, "%s():\n", __func__);

	memset(edid->reserved, 0, sizeof(edid->reserved));

	if (edid->pad != 0)
		return -EINVAL;

	if (edid->start_block == 0 && edid->blocks == 0) {
		edid->blocks = state->edid_blocks_written;
		return 0;
	}

	if (state->edid_blocks_written == 0)
		return -ENODATA;

	if (edid->start_block >= state->edid_blocks_written ||
	    edid->blocks == 0)
		return -EINVAL;

	if (edid->start_block + edid->blocks > state->edid_blocks_written)
		edid->blocks = state->edid_blocks_written - edid->start_block;

	mutex_lock(&state->lock);
	i2c_rd(sd, EDID_RAM + (edid->start_block * EDID_BLOCK_SIZE), edid->edid,
	       edid->blocks * EDID_BLOCK_SIZE);
	mutex_unlock(&state->lock);

	return 0;
}

static int tc358840_s_edid(struct v4l2_subdev *sd,
			   struct v4l2_subdev_edid *edid)
{
	struct tc358840_state *state = to_state(sd);
	u16 edid_len = edid->blocks * EDID_BLOCK_SIZE;
	u16 pa;
	unsigned int i;
	int err;

	v4l2_dbg(2, debug, sd, "%s, pad %d, start block %d, blocks %d\n",
		 __func__, edid->pad, edid->start_block, edid->blocks);

	memset(edid->reserved, 0, sizeof(edid->reserved));

	if (edid->pad != 0)
		return -EINVAL;

	if (edid->start_block != 0)
		return -EINVAL;

	if (edid->blocks > EDID_NUM_BLOCKS_MAX) {
		edid->blocks = EDID_NUM_BLOCKS_MAX;
		return -E2BIG;
	}
	if (edid->blocks) {
		pa = cec_get_edid_phys_addr(edid->edid, edid_len, NULL);
		err = v4l2_phys_addr_validate(pa, &pa, NULL);
		if (err)
			return err;
	}

	mutex_lock(&state->lock);
	tc358840_disable_hpd(sd);

	i2c_wr8(sd, EDID_LEN1, edid_len & 0xff);
	i2c_wr8(sd, EDID_LEN2, edid_len >> 8);

	if (edid->blocks == 0) {
		/* Enable Registers to be initialized */
		i2c_wr8_and_or(sd, INIT_END, ~(MASK_INIT_END), 0x00);

		cec_phys_addr_invalidate(state->cec_adap);
		state->edid_blocks_written = 0;
		goto unlock;
	}

	for (i = 0; i < edid_len; i += EDID_BLOCK_SIZE)
		i2c_wr(sd, EDID_RAM + i, edid->edid + i, EDID_BLOCK_SIZE);

	state->edid_blocks_written = edid->blocks;

	cec_s_phys_addr(state->cec_adap, pa, false);

	/* Signal end of initialization */
	i2c_wr8(sd, INIT_END, MASK_INIT_END);

	if (tx_5v_power_present(sd))
		tc358840_enable_hpd(sd);
unlock:
	mutex_unlock(&state->lock);

	return 0;
}

static int tc358840_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
				    struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subdev_subscribe(sd, fh, sub);
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subdev_subscribe_event(sd, fh, sub);
	default:
		return -EINVAL;
	}
}

/* --------------- VIDEO OPS --------------- */

static const struct v4l2_dv_timings_cap *
tc358840_g_timings_cap(struct tc358840_state *state)
{
	struct tc358840_platform_data *pdata = &state->pdata;

	if (pdata->csi_port == CSI_TX_BOTH)
		return &tc358840_timings_cap_4kp30;
	return &tc358840_timings_cap_1080p60;
}

static int tc358840_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct tc358840_state *state = to_state(sd);

	*status = 0;
	if (!state->test_pattern) {
		mutex_lock(&state->lock);
		*status |= no_signal(sd) ? V4L2_IN_ST_NO_SIGNAL : 0;
		*status |= (no_sync(sd) || !state->found_stable_signal) ?
				V4L2_IN_ST_NO_SYNC : 0;
		mutex_unlock(&state->lock);
	}

	v4l2_dbg(1, debug, sd, "%s: status = 0x%x\n", __func__, *status);

	return 0;
}

static int tc358840_s_dv_timings(struct v4l2_subdev *sd,
				 struct v4l2_dv_timings *timings)
{
	struct tc358840_state *state = to_state(sd);

	v4l2_dbg(3, debug, sd, "%s():\n", __func__);

	if (!timings)
		return -EINVAL;

	if (debug)
		v4l2_print_dv_timings(sd->name, "tc358840_s_dv_timings: ",
				      timings, false);

	if (state->test_pattern &&
	    tc358840_set_test_pattern_timing(sd, timings)) {
		v4l2_dbg(1, debug, sd,
			 "%s: failed to set test pattern timings\n", __func__);
		return -ERANGE;
	}

	memset(timings->bt.reserved, 0, sizeof(timings->bt.reserved));

	if (!state->test_pattern_changed &&
	    tc358840_match_dv_timings(&state->timings, timings, 0)) {
		v4l2_dbg(1, debug, sd, "%s: no change\n", __func__);
		return 0;
	}

	if (!v4l2_valid_dv_timings(timings, tc358840_g_timings_cap(state),
				   NULL, NULL)) {
		v4l2_dbg(1, debug, sd, "%s: timings out of range\n", __func__);
		return -ERANGE;
	}

	state->timings = *timings;
	state->test_pattern_changed = false;

	mutex_lock(&state->lock);
	enable_stream(sd, false);
	tc358840_set_csi(sd);
	tc358840_set_splitter(sd);
	mutex_unlock(&state->lock);

	return 0;
}

static int tc358840_g_dv_timings(struct v4l2_subdev *sd,
				 struct v4l2_dv_timings *timings)
{
	struct tc358840_state *state = to_state(sd);

	v4l2_dbg(3, debug, sd, "%s():\n", __func__);

	*timings = state->timings;

	return 0;
}

static int tc358840_enum_dv_timings(struct v4l2_subdev *sd,
				    struct v4l2_enum_dv_timings *timings)
{
	struct tc358840_state *state = to_state(sd);

	v4l2_dbg(3, debug, sd, "%s(): DUMMY\n", __func__);

	if (timings->pad != 0)
		return -EINVAL;

	return v4l2_enum_dv_timings_cap(timings,
			tc358840_g_timings_cap(state), NULL, NULL);
}

static int tc358840_query_dv_timings(struct v4l2_subdev *sd,
				     struct v4l2_dv_timings *timings)
{
	struct tc358840_state *state = to_state(sd);
	int ret;

	v4l2_dbg(3, debug, sd, "%s():\n", __func__);

	mutex_lock(&state->lock);
	ret = tc358840_get_detected_timings(sd, timings);
	mutex_unlock(&state->lock);
	if (ret)
		return ret;
	if (!state->found_stable_signal)
		return -ENOLCK;

	if (debug)
		v4l2_print_dv_timings(sd->name, "tc358840_query_dv_timings: ",
				      timings, false);
	if (!v4l2_valid_dv_timings(timings, tc358840_g_timings_cap(state),
				   NULL, NULL)) {
		v4l2_dbg(1, debug, sd, "%s: timings out of range\n", __func__);
		return -ERANGE;
	}

	return 0;
}

static int tc358840_dv_timings_cap(struct v4l2_subdev *sd,
				   struct v4l2_dv_timings_cap *cap)
{
	struct tc358840_state *state = to_state(sd);

	v4l2_dbg(3, debug, sd, "%s():\n", __func__);

	if (cap->pad != 0)
		return -EINVAL;

	*cap = *tc358840_g_timings_cap(state);

	return 0;
}

static int tc358840_get_mbus_config(struct v4l2_subdev *sd, unsigned pad,
				  struct v4l2_mbus_config *cfg)
{
	v4l2_dbg(3, debug, sd, "%s():\n", __func__);

	cfg->type = V4L2_MBUS_CSI2_DPHY;

	/* Support for non-continuous CSI-2 clock is missing in the driver */
	cfg->flags = V4L2_MBUS_CSI2_CONTINUOUS_CLOCK | V4L2_MBUS_CSI2_CHANNEL_0;

	switch (tc358840_num_csi_lanes_in_use(sd)) {
	case 1:
		cfg->flags |= V4L2_MBUS_CSI2_1_LANE;
		break;
	case 2:
		cfg->flags |= V4L2_MBUS_CSI2_2_LANE;
		break;
	case 3:
		cfg->flags |= V4L2_MBUS_CSI2_3_LANE;
		break;
	case 4:
		cfg->flags |= V4L2_MBUS_CSI2_4_LANE;
		break;
	default:
		return -EINVAL;
	}

	v4l2_dbg(2, debug, sd, "%s: Lanes: 0x%02X\n",
		 __func__, cfg->flags & 0x0f);

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static u8 tc358840_reg_size(u32 addr)
{
	if ((addr >= 0x100 && addr < 0x500) ||
	    (addr >= 0x600 && addr < 0x700))
		return 4;
	if (addr >= 0x8000 && addr < 0xa000)
		return 1;
	return 2;
}

static int tc358840_g_register(struct v4l2_subdev *sd,
			       struct v4l2_dbg_register *reg)
{
	reg->size = tc358840_reg_size(reg->reg);
	switch (reg->size) {
	case 1:
		reg->val = i2c_rd8(sd, reg->reg);
		break;
	case 2:
		reg->val = i2c_rd16(sd, reg->reg);
		break;
	case 4:
	default:
		reg->val = i2c_rd32(sd, reg->reg);
		break;
	}
	return 0;
}

static int tc358840_s_register(struct v4l2_subdev *sd,
			       const struct v4l2_dbg_register *reg)
{
	u8 size = tc358840_reg_size(reg->reg);

	/*
	 * It should not be possible for the user to enable HDCP with a simple
	 * v4l2-dbg command.
	 */
	if (reg->reg == HDCP_MODE ||
	    (reg->reg & 0xff00) == 0x8800)
		return 0;

	switch (size) {
	case 1:
		i2c_wr8(sd, reg->reg, reg->val);
		break;
	case 2:
		i2c_wr16(sd, reg->reg, reg->val);
		break;
	case 4:
	default:
		i2c_wr32(sd, reg->reg, reg->val);
		break;
	}
	return 0;
}
#endif

static int tc358840_log_status(struct v4l2_subdev *sd)
{
	struct tc358840_state *state = to_state(sd);
	struct v4l2_dv_timings timings;
	u8 hdmi_sys_status =  i2c_rd8(sd, SYS_STATUS);
	u16 sysctl = i2c_rd16(sd, SYSCTL);
	u8 vi_status3 = i2c_rd8(sd, VI_STATUS3);
	u8 vout_csc = i2c_rd8(sd, VOUT_CSC);
	u8 hv_status = i2c_rd8(sd, HV_STATUS);
	const int deep_color_mode[4] = { 8, 10, 12, 16 };
	static const char * const input_color_space[] = {
		"RGB", "YCbCr 601", "Adobe RGB", "YCbCr 709", "NA (4)",
		"xvYCC 601", "NA(6)", "xvYCC 709", "NA(8)", "sYCC601",
		"NA(10)", "NA(11)", "NA(12)", "Adobe YCC 601"};
	static const char * const vout_csc_mode_str[] = {
		"Off", "On (built-in coefficients)",
		"On/Off Auto", "On (host coefficients)"
	};
	static const char * const vout_csc_color_str[] = {
		"RGB Full", "RGB Limited",
		"YCbCr 601 Full", "YCbCr 601 Limited",
		"YCbCr 709 Full", "YCbCr 709 Limited",
		"RGB Full to Limited", "RGB Limited to Full"
	};

	v4l2_ctrl_subdev_log_status(sd);
	v4l2_info(sd, "-----Chip status-----\n");
	v4l2_info(sd, "Chip ID: 0x%02x\n",
		  (i2c_rd16(sd, CHIPID_ADDR) & MASK_CHIPID) >> 8);
	v4l2_info(sd, "Chip revision: 0x%02x\n",
		  i2c_rd16(sd, CHIPID_ADDR) & MASK_REVID);
	v4l2_info(sd, "Reset: IR: %d, CEC: %d, CSI TX: %d, HDMI: %d\n",
		  !!(sysctl & MASK_IRRST),
		  !!(sysctl & MASK_CECRST),
		  !!(sysctl & MASK_CTXRST),
		  !!(sysctl & MASK_HDMIRST));
	v4l2_info(sd, "Sleep mode: %s\n", sysctl & MASK_SLEEP ? "on" : "off");
	v4l2_info(sd, "Audio mode: 0x%02x\n", i2c_rd8(sd, AU_STATUS0));
	v4l2_info(sd, "Cable detected (+5V power): %s\n",
		  hdmi_sys_status & MASK_S_DDC5V ? "yes" : "no");
	v4l2_info(sd, "Number of EDID blocks: %d\n",
		  state->edid_blocks_written);
	v4l2_info(sd, "DDC lines enabled: %s\n",
		  (i2c_rd8(sd, EDID_MODE) & MASK_EDID_MODE_ALL) ?
		  "yes" : "no");
	v4l2_info(sd, "Hotplug enabled: %s\n",
		  (i2c_rd8(sd, HPD_CTL) & MASK_HPD_OUT0) ?
		  "yes" : "no");
	v4l2_info(sd, "CEC enabled: %s\n",
		  (i2c_rd16(sd, CECEN) & MASK_CECEN) ?  "yes" : "no");
	v4l2_info(sd, "EQ_BYPS enabled: %s\n", state->eq_bypass ? "yes" : "no");
	v4l2_info(sd, "-----Signal status-----\n");
	v4l2_info(sd, "TMDS signal detected: %s\n",
		  hdmi_sys_status & MASK_S_TMDS ? "yes" : "no");
	v4l2_info(sd, "Stable sync signal: %s\n",
		  hdmi_sys_status & MASK_S_SYNC ? "yes" : "no");
	v4l2_info(sd, "PHY PLL locked: %s\n",
		  hdmi_sys_status & MASK_S_PHY_PLL ? "yes" : "no");
	v4l2_info(sd, "PHY DE detected: %s\n",
		  hdmi_sys_status & MASK_S_PHY_SCDT ? "yes" : "no");
	v4l2_info(sd, "HV Status: H cycle %s allowed range, V cycle %s allowed range\n",
		  (hv_status & MASK_S_H_RANG) ? "within" : "outside",
		  (hv_status & MASK_S_V_RANG) ? "within" : "outside");
	v4l2_info(sd, "Audio Buffer Interrupts: 0x%02x\n",
		  i2c_rd8(sd, AUDIO_INT));

	i2c_wr8(sd, AUDIO_INT, 0xfe);

	v4l2_info(sd, "Found signal counters: %d %d\n",
		  state->found_signal_cnt[0], state->found_signal_cnt[1]);

	if (tc358840_get_detected_timings(sd, &timings)) {
		v4l2_info(sd, "No video detected\n");
	} else {
		v4l2_print_dv_timings(sd->name, "Detected format: ",
				      &timings, true);
	}
	v4l2_print_dv_timings(sd->name, "Configured format: ",
			      &state->timings, true);
	v4l2_info(sd, "streaming: %s\n", state->enabled ? "yes" : "no");

	v4l2_info(sd, "-----CSI-TX status-----\n");
	v4l2_info(sd, "Lanes needed: %d\n",
		  tc358840_num_csi_lanes_needed(sd));
	v4l2_info(sd, "Lanes in use: %d\n",
		  tc358840_num_csi_lanes_in_use(sd));
	v4l2_info(sd, "Internal status: 0x%02x\n",
		  i2c_rd32(sd, CSITX_INTERNAL_STAT));
	v4l2_info(sd, "Splitter %sabled\n",
		  (i2c_rd16(sd, SPLITTX0_CTRL) & MASK_SPBP) ? "dis" : "en");
	v4l2_info(sd, "Color encoding: %s\n",
		  state->mbus_fmt_code == MEDIA_BUS_FMT_UYVY8_1X16 ?
		  "YCbCr 422 16-bit" :
		  state->mbus_fmt_code == MEDIA_BUS_FMT_RGB888_1X24 ?
		  "RGB 888 24-bit" : "Unsupported");
	v4l2_info(sd, "CSC: %s %s\n",
		  vout_csc_mode_str[vout_csc & MASK_CSC_MODE],
		  vout_csc_color_str[(vout_csc & MASK_COLOR) >> 4]);

	v4l2_info(sd, "-----%s status-----\n", is_hdmi(sd) ? "HDMI" : "DVI-D");
	v4l2_info(sd, "HDCP encrypted content: %s\n",
		  hdmi_sys_status & MASK_S_HDCP ? "yes" : "no");
	v4l2_info(sd, "Input color space: %s %s range\n",
		  input_color_space[(vi_status3 & MASK_S_V_COLOR) >> 1],
		  (vi_status3 & MASK_LIMITED) ? "limited" : "full");
	if (!is_hdmi(sd))
		return 0;
	v4l2_info(sd, "AV Mute: %s\n", hdmi_sys_status & MASK_S_AVMUTE ? "on" :
		  "off");
	v4l2_info(sd, "Deep color mode: %d-bits per channel\n",
		  deep_color_mode[(i2c_rd8(sd, VI_STATUS1) &
				   MASK_S_DEEPCOLOR) >> 2]);
	print_infoframe(sd);

	return 0;
}

static int tc358840_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct tc358840_state *state = to_state(sd);
	int err;

	v4l2_dbg(3, debug, sd, "%s():\n", __func__);

	mutex_lock(&state->lock);
	err = enable_stream(sd, enable);
	mutex_unlock(&state->lock);
	return err;
}

static int tc358840_enum_mbus_code(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_mbus_code_enum *code)
{
	v4l2_dbg(2, debug, sd, "%s()\n", __func__);

	if (code->index >= 2)
		return -EINVAL;

	switch (code->index) {
	case 0:
		code->code = MEDIA_BUS_FMT_RGB888_1X24;
		break;
	case 1:
		code->code = MEDIA_BUS_FMT_UYVY8_1X16;
		break;
	}
	return 0;
}

static int tc358840_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd =
		&container_of(ctrl->handler, struct tc358840_state, hdl)->sd;

	if (ctrl->id == V4L2_CID_DV_RX_IT_CONTENT_TYPE) {
		ctrl->val = V4L2_DV_IT_CONTENT_TYPE_NO_ITC;
		if (i2c_rd8(sd, PK_AVI_3BYTE) & 0x80)
			ctrl->val = (i2c_rd8(sd, PK_AVI_5BYTE) >> 4) & 3;
		return 0;
	}
	return -EINVAL;
}

static int tc358840_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd =
		&container_of(ctrl->handler, struct tc358840_state, hdl)->sd;
	struct tc358840_state *state = to_state(sd);
	int err = 0;

	switch (ctrl->id) {
	case V4L2_CID_TEST_PATTERN:
		v4l2_dbg(2, debug, sd, "%s() id=0x%x old_val=%d new_val=%d:\n",
			 __func__, ctrl->id, state->test_pattern, ctrl->val);
		/* Not allowed to enable/disable testpat if streaming enabled */
		if (state->enabled) {
			if (!state->test_pattern ||
			    ctrl->val == TEST_PATTERN_DISABLED) {
				err = -EBUSY;
				break;
			}
		}
		tc358840_set_test_pattern_type(sd, ctrl->val);

		/* test pattern enabled ? set/override tx_5V to true */
		if (!state->test_pattern && ctrl->val) {
			__v4l2_ctrl_s_ctrl(state->detect_tx_5v_ctrl, true);
			v4l2_subdev_notify_event(sd, &tc358840_ev_fmt);
			tc358840_set_test_pattern_timing(sd, &state->timings);
		}
		/* test pattern disabled ? set tx_5V according to HW */
		if (state->test_pattern && !ctrl->val) {
			__v4l2_ctrl_s_ctrl(state->detect_tx_5v_ctrl,
					   tx_5v_power_present(sd));
			state->found_stable_signal = false;
			v4l2_subdev_notify_event(sd, &tc358840_ev_fmt);
		}

		state->test_pattern = ctrl->val;
		state->test_pattern_changed = true;
		break;
	case  V4L2_CID_DV_RX_RGB_RANGE:
		state->rgb_quantization_range = ctrl->val;
		if (state->mbus_fmt_code == MEDIA_BUS_FMT_RGB888_1X24) {
			mutex_lock(&state->lock);
			set_rgb_quantization_range(sd);
			mutex_unlock(&state->lock);
		}
		break;
	case TC358840_CID_EQ_BYPS_MODE:
		if (ctrl->val == 0) {
			state->invalid_eq_bypass[0] = false;
			state->invalid_eq_bypass[1] = false;
		}
		break;
	}
	return err;
}

static const struct v4l2_ctrl_ops tc358840_ctrl_ops = {
	.g_volatile_ctrl = tc358840_g_volatile_ctrl,
	.s_ctrl = tc358840_s_ctrl,
};

static struct v4l2_subdev_video_ops tc358840_subdev_video_ops = {
	.g_input_status = tc358840_g_input_status,
	.s_dv_timings = tc358840_s_dv_timings,
	.g_dv_timings = tc358840_g_dv_timings,
	.query_dv_timings = tc358840_query_dv_timings,
	.s_stream = tc358840_s_stream,
};

static struct v4l2_subdev_core_ops tc358840_subdev_core_ops = {
	.log_status = tc358840_log_status,
	.interrupt_service_routine = tc358840_isr,
	.subscribe_event = tc358840_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = tc358840_g_register,
	.s_register = tc358840_s_register,
#endif
};

static const struct v4l2_subdev_pad_ops tc358840_pad_ops = {
	.set_fmt = tc358840_set_fmt,
	.get_fmt = tc358840_get_fmt,
	.get_mbus_config = tc358840_get_mbus_config,
	.enum_mbus_code = tc358840_enum_mbus_code,
	.get_edid = tc358840_g_edid,
	.set_edid = tc358840_s_edid,
	.dv_timings_cap = tc358840_dv_timings_cap,
	.enum_dv_timings = tc358840_enum_dv_timings,
};

static struct v4l2_subdev_ops tc358840_ops = {
	.core = &tc358840_subdev_core_ops,
	.video = &tc358840_subdev_video_ops,
	.pad = &tc358840_pad_ops,
};

/* --------------- CUSTOM CTRLS --------------- */

static const struct v4l2_ctrl_config tc358840_ctrl_audio_sampling_rate = {
	.id = TC358840_CID_AUDIO_SAMPLING_RATE,
	.name = "Audio Sampling Rate",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 768000,
	.step = 1,
	.def = 0,
	.flags = V4L2_CTRL_FLAG_READ_ONLY,
};

static const struct v4l2_ctrl_config tc358840_ctrl_audio_present = {
	.id = TC358840_CID_AUDIO_PRESENT,
	.name = "Audio Present",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 0,
	.flags = V4L2_CTRL_FLAG_READ_ONLY,
};

static const struct v4l2_ctrl_config tc358840_ctrl_tmds_present = {
	.id = TC358840_CID_TMDS_PRESENT,
	.name = "TMDS Present",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 0,
	.flags = V4L2_CTRL_FLAG_READ_ONLY,
};

static const struct v4l2_ctrl_config tc358840_ctrl_splitter_width = {
	.id = TC358840_CID_SPLITTER_WIDTH,
	.name = "Splitter Width",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 320,
	.max = 1920,
	.step = 16,
	.def = 1920,
	.flags = V4L2_CTRL_FLAG_READ_ONLY,
};

static const char * const eq_byps_menu[] = {
	"Automatic",
	"0",
	"1",
};

static const struct v4l2_ctrl_config tc358840_ctrl_eq_byps_mode = {
	.ops = &tc358840_ctrl_ops,
	.id = TC358840_CID_EQ_BYPS_MODE,
	.name = "EQ_BYPS Mode",
	.type = V4L2_CTRL_TYPE_MENU,
	.qmenu = eq_byps_menu,
	.max = 2,
};

static const char * const test_pattern_menu[] = {
	"Disabled",
	"Color Bar",
	"Color Checker",
};

/* --------------- PROBE / REMOVE --------------- */

#ifdef CONFIG_OF

static bool tc358840_parse_dt(struct tc358840_platform_data *pdata,
			      struct i2c_client *client)
{
	struct device_node *node = client->dev.of_node;
	struct v4l2_fwnode_endpoint endpoint = { .bus_type = 0 };
	struct device_node *ep;
	struct clk *refclk;
	int ret;

	v4l_dbg(1, debug, client, "Device Tree Parameters:\n");

	refclk = devm_clk_get(&client->dev, "refclk");
	if (IS_ERR(refclk)) {
		if (PTR_ERR(refclk) != -EPROBE_DEFER)
			dev_err(&client->dev, "failed to get refclk: %ld\n",
				PTR_ERR(refclk));
		return PTR_ERR(refclk);
	}

	pdata->reset_gpio = of_get_named_gpio(node, "reset-gpios", 0);
	if (pdata->reset_gpio == 0)
		return false;
	v4l_dbg(1, debug, client, "reset_gpio = %d\n", pdata->reset_gpio);

	ep = of_graph_get_next_endpoint(node, NULL);
	if (!ep)
		return false;

	ret = v4l2_fwnode_endpoint_alloc_parse(of_fwnode_handle(ep), &endpoint);
	if (ret)
		goto put_node;
	pdata->endpoint = endpoint;

	if (of_property_read_u32(node, "ddc5v_delay", &pdata->ddc5v_delay) ||
	    of_property_read_u32(node, "csi_port", &pdata->csi_port) ||
	    of_property_read_u32(node, "lineinitcnt", &pdata->lineinitcnt) ||
	    of_property_read_u32(node, "lptxtimecnt", &pdata->lptxtimecnt) ||
	    of_property_read_u32(node, "tclk_headercnt", &pdata->tclk_headercnt) ||
	    of_property_read_u32(node, "tclk_trailcnt", &pdata->tclk_trailcnt) ||
	    of_property_read_u32(node, "ths_headercnt", &pdata->ths_headercnt) ||
	    of_property_read_u32(node, "twakeup", &pdata->twakeup) ||
	    of_property_read_u32(node, "tclk_postcnt", &pdata->tclk_postcnt) ||
	    of_property_read_u32(node, "ths_trailcnt", &pdata->ths_trailcnt) ||
	    of_property_read_u32(node, "hstxvregcnt", &pdata->hstxvregcnt) ||
	    of_property_read_u16(node, "pll_prd", &pdata->pll_prd) ||
	    of_property_read_u16(node, "pll_frs", &pdata->pll_frs) ||
	    of_property_read_u16(node, "pll_fbd", &pdata->pll_fbd))
		goto free_endpoint;

	ret = clk_prepare_enable(refclk);
	if (ret) {
		dev_err(&client->dev, "Failed! to enable clock\n");
		goto free_endpoint;
	}
	pdata->refclk_hz = clk_get_rate(refclk);

	if (pdata->ddc5v_delay > DDC5V_DELAY_MAX)
		pdata->ddc5v_delay = DDC5V_DELAY_MAX;
	v4l_dbg(1, debug, client, "ddc5v_delay = %u ms\n",
		50 * pdata->ddc5v_delay);
	v4l_dbg(1, debug, client, "refclk_hz = %u\n", pdata->refclk_hz);
	v4l_dbg(1, debug, client, "ddc5v_delay = %u\n", pdata->ddc5v_delay);
	v4l_dbg(1, debug, client, "csi_port = %u\n", pdata->csi_port);
	v4l_dbg(1, debug, client, "lineinitcnt = %u\n", pdata->lineinitcnt);
	v4l_dbg(1, debug, client, "lptxtimecnt = %u\n", pdata->lptxtimecnt);
	v4l_dbg(1, debug, client, "tclk_headercnt = %u\n", pdata->tclk_headercnt);
	v4l_dbg(1, debug, client, "tclk_trailcnt = %u\n", pdata->tclk_trailcnt);
	v4l_dbg(1, debug, client, "ths_headercnt = %u\n", pdata->ths_headercnt);
	v4l_dbg(1, debug, client, "twakeup = %u\n", pdata->twakeup);
	v4l_dbg(1, debug, client, "tclk_postcnt = %u\n", pdata->tclk_postcnt);
	v4l_dbg(1, debug, client, "ths_trailcnt = %u\n", pdata->ths_trailcnt);
	v4l_dbg(1, debug, client, "hstxvregcnt = %u\n", pdata->hstxvregcnt);
	v4l_dbg(1, debug, client, "pll_prd = %u\n", pdata->pll_prd);
	v4l_dbg(1, debug, client, "pll_frs = %u\n", pdata->pll_frs);
	v4l_dbg(1, debug, client, "pll_fbd = %u\n", pdata->pll_fbd);

	v4l2_fwnode_endpoint_free(&endpoint);
	of_node_put(ep);
	return true;
free_endpoint:
	v4l2_fwnode_endpoint_free(&endpoint);
put_node:
	of_node_put(ep);
	return false;
}
#else
static bool tc358840_parse_dt(struct tc358840_platform_data *pdata,
			      struct i2c_client *client)
{
	return false;
}
#endif

static int tc358840_verify_chipid(struct v4l2_subdev *sd)
{
	u16 cid = 0;

	cid = i2c_rd16(sd, CHIPID_ADDR);
	if (cid != TC358840_CHIPID) {
		v4l2_err(sd, "Invalid chip ID 0x%04X\n", cid);
		/* Try again later */
		return -EPROBE_DEFER;
	}

	v4l2_dbg(1, debug, sd, "TC358840 ChipID 0x%02x, Revision 0x%02x\n",
		 (cid & MASK_CHIPID) >> 8, cid & MASK_REVID);

	return 0;
}

static int tc358840_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s\n", __func__);
	return 0;
}

static int tc358840_registered(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tc358840_state *state = to_state(sd);

	dev_dbg(&client->dev, "%s\n", __func__);

	state->delayed_work_stop_polling = false;
	schedule_delayed_work(&state->delayed_work_poll, POLL_PERIOD);
	return 0;
}

static void tc358840_unregistered(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tc358840_state *state = to_state(sd);

	dev_dbg(&client->dev, "%s\n", __func__);

	state->delayed_work_stop_polling = true;
	cancel_delayed_work_sync(&state->delayed_work_enable_hotplug);
	cancel_delayed_work_sync(&state->delayed_work_poll);
	/*
	 * Do this again since there is a tiny race where
	 * delayed_work_stop_polling was set when tc358840_delayed_work_poll
	 * was just scheduling the next tc358840_delayed_work_poll call.
	 * So this second cancel will prevent that rescheduled work from
	 * running.
	 */
	cancel_delayed_work_sync(&state->delayed_work_poll);
}

static const struct v4l2_subdev_internal_ops tc358840_subdev_internal_ops = {
	.open = tc358840_open,
	.registered = tc358840_registered,
	.unregistered = tc358840_unregistered,
};

static const struct media_entity_operations tc358840_media_ops = {
#ifdef CONFIG_MEDIA_CONTROLLER
	.link_validate = v4l2_subdev_link_validate,
#endif
};

static void tc358840_reset_gpio(struct tc358840_state *state, bool enable)
{
	int reset_gpio = state->pdata.reset_gpio;

	if (!gpio_is_valid(reset_gpio))
		return;

	if (enable) {
		gpiod_set_value_cansleep(gpio_to_desc(reset_gpio), 0);
	} else {
		gpiod_set_value_cansleep(gpio_to_desc(reset_gpio), 0);
		usleep_range(1000, 2000);
		gpiod_set_value_cansleep(gpio_to_desc(reset_gpio), 1);
		usleep_range(20000, 30000);
	}
}

static int tc358840_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	u16 irq_mask = MASK_HDMI_INT;
	struct tc358840_state *state;
	struct v4l2_ctrl *ctrl;
	struct v4l2_subdev *sd;
	int err;

	state = devm_kzalloc(&client->dev,
			     sizeof(struct tc358840_state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	if (client->dev.of_node) {
		if (!tc358840_parse_dt(&state->pdata, client)) {
			v4l_err(client, "Couldn't parse device tree\n");
			return -ENODEV;
		}
	} else {
		if (!client->dev.platform_data) {
			v4l_err(client, "No platform data!\n");
			return -ENODEV;
		}
		state->pdata = *(struct tc358840_platform_data *)
				client->dev.platform_data;
	}

	state->i2c_client = client;
	sd = &state->sd;

	i2c_set_clientdata(client, state);

	v4l2_i2c_subdev_init(sd, client, &tc358840_ops);
	mutex_init(&state->lock);

	/* Release System Reset (pin K8) */
	v4l2_dbg(1, debug, sd, "Releasing System Reset (gpio 0x%04X)\n",
		 state->pdata.reset_gpio);
	if (!gpio_is_valid(state->pdata.reset_gpio)) {
		v4l_err(client, "Reset GPIO is invalid!\n");
		return state->pdata.reset_gpio;
	}
	err = devm_gpio_request_one(&client->dev, state->pdata.reset_gpio,
				    GPIOF_OUT_INIT_LOW, "tc358840-reset");
	if (err) {
		dev_err(&client->dev,
			"Failed to request Reset GPIO 0x%04X: %d\n",
			state->pdata.reset_gpio, err);
		return err;
	}
	tc358840_reset_gpio(state, false);

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE_DATA)) {
		err = -EIO;
		goto err_reset;
	}
	v4l_info(client, "Chip found @ 7h%02X (%s)\n",
		 client->addr, client->adapter->name);

	/* Verify chip ID */
	err = tc358840_verify_chipid(sd);
	if (err)
		goto err_reset;

#ifdef CONFIG_VIDEO_TC358840_CEC
	state->cec_adap =
		cec_allocate_adapter(&tc358840_cec_adap_ops,
				     state, dev_name(&client->dev),
				     CEC_CAP_DEFAULTS | CEC_CAP_MONITOR_ALL,
				     CEC_MAX_LOG_ADDRS);
	if (IS_ERR(state->cec_adap)) {
		err = state->cec_adap ? PTR_ERR(state->cec_adap) : -ENOMEM;
		goto err_reset;
	}
	irq_mask |= MASK_CEC_RINT | MASK_CEC_TINT;
#endif

	/* Control Handlers */
	v4l2_ctrl_handler_init(&state->hdl, 8);

	state->detect_tx_5v_ctrl =
		v4l2_ctrl_new_std(&state->hdl, NULL,
				  V4L2_CID_DV_RX_POWER_PRESENT, 0, 1, 0, 0);

	ctrl = v4l2_ctrl_new_std_menu(&state->hdl, &tc358840_ctrl_ops,
				      V4L2_CID_DV_RX_IT_CONTENT_TYPE,
				      V4L2_DV_IT_CONTENT_TYPE_NO_ITC,
				      0, V4L2_DV_IT_CONTENT_TYPE_NO_ITC);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	state->test_pattern_ctrl =
		v4l2_ctrl_new_std_menu_items(&state->hdl, &tc358840_ctrl_ops,
					     V4L2_CID_TEST_PATTERN,
					     ARRAY_SIZE(test_pattern_menu) - 1,
					     0, 0, test_pattern_menu);
	state->rgb_quantization_range_ctrl =
		v4l2_ctrl_new_std_menu(&state->hdl, &tc358840_ctrl_ops,
				       V4L2_CID_DV_RX_RGB_RANGE,
				       V4L2_DV_RGB_RANGE_FULL, 0,
				       V4L2_DV_RGB_RANGE_AUTO);

	/* Custom controls */
	state->audio_sampling_rate_ctrl =
		v4l2_ctrl_new_custom(&state->hdl,
				     &tc358840_ctrl_audio_sampling_rate, NULL);

	state->audio_present_ctrl =
		v4l2_ctrl_new_custom(&state->hdl,
				     &tc358840_ctrl_audio_present, NULL);

	state->tmds_present_ctrl =
		v4l2_ctrl_new_custom(&state->hdl,
				     &tc358840_ctrl_tmds_present, NULL);

	state->splitter_width_ctrl =
		v4l2_ctrl_new_custom(&state->hdl,
				     &tc358840_ctrl_splitter_width, NULL);

	state->eq_byps_mode_ctrl =
		v4l2_ctrl_new_custom(&state->hdl,
				     &tc358840_ctrl_eq_byps_mode, NULL);

	if (state->hdl.error) {
		err = state->hdl.error;
		goto err_hdl;
	}

	sd->ctrl_handler = &state->hdl;

	INIT_DELAYED_WORK(&state->delayed_work_enable_hotplug,
			  tc358840_delayed_work_enable_hotplug);
	INIT_DELAYED_WORK(&state->delayed_work_poll,
			  tc358840_delayed_work_poll);

	/* Initial Setup */
	state->mbus_fmt_code = MEDIA_BUS_FMT_UYVY8_1X16;
	tc358840_initial_setup(sd);

	tc358840_set_csi_mbus_config(sd);

	v4l2_ctrl_handler_setup(sd->ctrl_handler);

	/* Get interrupt */
	if (client->irq) {
		err = devm_request_threaded_irq(&state->i2c_client->dev,
				client->irq, NULL, tc358840_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				sd->name, (void *)sd);
		if (err) {
			v4l2_err(sd, "Could not request interrupt %d!\n",
				 client->irq);
			goto err_hdl;
		}
	}

	v4l2_info(sd, "%s found @ 7h%02X (%s)\n", client->name,
		  client->addr, client->adapter->name);

	sd->dev	= &client->dev;
	sd->internal_ops = &tc358840_subdev_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
#if defined(CONFIG_MEDIA_CONTROLLER)
	state->pad[0].flags = MEDIA_PAD_FL_SOURCE;
	state->pad[1].flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_VID_IF_BRIDGE;
	sd->entity.ops = &tc358840_media_ops;
	err = media_entity_pads_init(&sd->entity, 2, state->pad);
	if (err < 0) {
		dev_err(&client->dev, "unable to init media entity\n");
		goto err_hdl;
	}
#endif

	err = cec_register_adapter(state->cec_adap, &client->dev);
	if (err < 0) {
		pr_err("%s: failed to register the cec device\n", __func__);
		cec_delete_adapter(state->cec_adap);
		state->cec_adap = NULL;
		goto err_hdl;
	}

	tc358840_enable_interrupts(sd, tx_5v_power_present(sd));

	i2c_wr16(sd, INTMASK, ~irq_mask & 0x0f3f);

	/*
	 * The runtime PM initialises the device to a disabled and suspended
	 * state. Set active state but don't enabled. The standby control
	 * module will enable runtime PM when it's ready.
	 * To always reflect the actual post-system sleep status run
	 * pm_runtime_disable() before pm_runtime_set_active().
	 */
	pm_runtime_disable(&client->dev);
	err = pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);
	if (err) {
		dev_err(&client->dev, "set active error:%d\n", err);
		goto err_hdl;
	}

	err = v4l2_async_register_subdev(sd);
	if (err == 0)
		return 0;

	cec_unregister_adapter(state->cec_adap);
err_hdl:
	v4l2_ctrl_handler_free(&state->hdl);
err_reset:
	tc358840_reset_gpio(state, true);
	return err;
}

static void tc358840_shutdown(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tc358840_state *state = to_state(sd);

	v4l_dbg(1, debug, client, "%s()\n", __func__);

	state->delayed_work_stop_polling = true;
	cancel_delayed_work_sync(&state->delayed_work_enable_hotplug);
	cancel_delayed_work_sync(&state->delayed_work_poll);
	/*
	 * Do this again since there is a tiny race where
	 * delayed_work_stop_polling was set when tc358840_delayed_work_poll
	 * was just scheduling the next tc358840_delayed_work_poll call. So
	 * this second cancel will prevent that rescheduled work from running.
	 */
	cancel_delayed_work_sync(&state->delayed_work_poll);
	v4l_info(client, "shutdown tc358840 instance\n");
}

static int tc358840_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tc358840_state *state = to_state(sd);

	v4l_dbg(1, debug, client, "%s()\n", __func__);

	tc358840_shutdown(client);
	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	cec_unregister_adapter(state->cec_adap);
	tc358840_reset_gpio(state, true);
	v4l_info(client, "removed tc358840 instance\n");
	return 0;
}

static void disable_audio_block_and_i2s(struct v4l2_subdev *sd)
{
	/* Disable audio block and i2s to reduce power */
	i2c_wr16_and_or(sd, SYSCTL, ~0, MASK_I2SDIS | MASK_ABRST);
	/* Enable pull up/down on I2S bus */
	i2c_wr16_and_or(sd, I2S_PUDCTL, ~0,
			MASK_A_OSCK_PULL_DOWN | MASK_A_SCK_PULL_DOWN |
			MASK_A_WFS_PULL_DOWN | MASK_A_SD_0_PULL_DOWN |
			MASK_A_SD_1_PULL_DOWN | MASK_A_SD_2_PULL_DOWN |
			MASK_A_SD_3_PULL_DOWN);
}

static void enable_audio_block_and_i2s(struct v4l2_subdev *sd)
{
	/* Disable pull up/down on I2S bus */
	i2c_wr16_and_or(sd, I2S_PUDCTL, 0, 0);
	/* Enable audio block and i2s */
	i2c_wr16_and_or(sd, SYSCTL, ~((u16)(MASK_I2SDIS | MASK_ABRST)), 0);
}

static int tc358840_runtime_resume(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct tc358840_state *state = to_state(sd);
	struct i2c_client *client = state->i2c_client;

	v4l_dbg(1, debug, client, "%s +++\n", __func__);

	mutex_lock(&state->lock);
	if (!state->suspended)
		goto unlock;

	enable_audio_block_and_i2s(sd);

	state->suspended = false;
unlock:
	mutex_unlock(&state->lock);
	return 0;
}

static int tc358840_runtime_suspend(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct tc358840_state *state = to_state(sd);
	struct i2c_client *client = state->i2c_client;

	v4l_dbg(1, debug, client, "%s +++\n", __func__);

	mutex_lock(&state->lock);
	if (state->suspended)
		goto unlock;

	disable_audio_block_and_i2s(sd);

	state->suspended = true;
unlock:
	mutex_unlock(&state->lock);
	return 0;
}

static const struct dev_pm_ops tc358840_pm_ops = {
	SET_RUNTIME_PM_OPS(tc358840_runtime_suspend,
			   tc358840_runtime_resume, NULL)
};

static const struct i2c_device_id tc358840_id[] = {
	{ "tc358840", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, tc358840_id);

#ifdef CONFIG_OF
static const struct of_device_id tc358840_of_table[] = {
	{ .compatible = "toshiba,tc358840" },
	{ }
};
MODULE_DEVICE_TABLE(of, tc358840_of_table);
#endif

static struct i2c_driver tc358840_driver = {
	.driver = {
		.of_match_table = of_match_ptr(tc358840_of_table),
		.name = "tc358840",
		.pm = &tc358840_pm_ops,
	},
	.probe = tc358840_probe,
	.remove = tc358840_remove,
	.shutdown = tc358840_shutdown,
	.id_table = tc358840_id,
};
module_i2c_driver(tc358840_driver);

MODULE_DESCRIPTION("Driver for Toshiba TC358840 HDMI to CSI-2 Bridge");
MODULE_AUTHOR("Armin Weiss (weii@zhaw.ch)");
MODULE_LICENSE("GPL v2");
