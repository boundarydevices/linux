/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __MTK_DP_V2_H__
#define __MTK_DP_V2_H__

#include <drm/drm_bridge.h>
#include <drm/drm_encoder.h>
#include <drm/drm_atomic_helper.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/display/drm_dp_mst_helper.h>
#include <drm/display/drm_hdcp_helper.h>

#include <sound/hdmi-codec.h>

#include <linux/media-bus-format.h>
#include <linux/usb/typec_mux.h>

#define SUPPORT_YUV422		1

#define DP_ENCODER_NUM		2
#define DP_CONNECTOR_NUM	16
#define DP_PAYLOAD_MAX		8
#define DP_FIRST_CON		0
#define DP_SST_ENCODER_PORT	0
#define DP_ENCODER_ENDPOINT	0

#define DP_PHY_LEVEL_COUNT	10
#define DP_REG_OFFSET(a)	((a) * 0xa00)

#define HPD_CONNECT			BIT(0)
#define HPD_DISCONNECT		BIT(10)
#define HPD_INITIAL_STATE	0
#define HPD_INT_EVNET		BIT(2)

#define MTK_DP_HPD_DISCONNECT		BIT(1)
#define MTK_DP_HPD_CONNECT		BIT(2)
#define MTK_DP_HPD_INTERRUPT		BIT(3)

#define DP_HPD_DEBOUNCE		100

#define DP_DSC_BPP			8

#define DPCD_00021			0x00021

#define DP_BITWIDTH_16		BIT(0)
#define DP_BITWIDTH_20		BIT(1)
#define DP_BITWIDTH_24		BIT(2)

#define PHY_READ_BYTE(mtk_dp, reg) mtk_dp_phy_read(mtk_dp, reg)
#define PHY_WRITE_BYTE(mtk_dp, reg, val) \
	mtk_dp_phy_write_byte(mtk_dp, reg, val, 0xFF)
#define PHY_WRITE_2BYTE(mtk_dp, reg, val) \
	mtk_dp_phy_mask(mtk_dp, reg, val, 0xFFFF)
#define PHY_WRITE_4BYTE(mtk_dp, reg, val) \
	mtk_dp_phy_write(mtk_dp, reg, val)
#define PHY_WRITE_BYTE_MASK(mtk_dp, addr, val, mask) \
	mtk_dp_phy_write_byte(mtk_dp, addr, val, mask)
#define PHY_WRITE_2BYTE_MASK(mtk_dp, addr, val, mask) \
	mtk_dp_phy_mask(mtk_dp, addr, val, mask)
#define PHY_WRITE_4BYTE_MASK(mtk_dp, addr, val, mask) \
	mtk_dp_phy_mask(mtk_dp, addr, val, mask)
#define READ_BYTE(mtk_dp, reg) \
	(mtk_dp_read(mtk_dp, reg) & 0xFF)
#define READ_2BYTE(mtk_dp, reg) \
	(mtk_dp_read(mtk_dp, reg) & 0xFFFF)
#define READ_4BYTE(mtk_dp, reg) \
	mtk_dp_read(mtk_dp, reg)
#define WRITE_BYTE(mtk_dp, reg, val) \
	mtk_dp_write_byte(mtk_dp, reg, val, 0xFF)
#define WRITE_2BYTE(mtk_dp, reg, val) \
	mtk_dp_mask(mtk_dp, reg, val, 0xFFFF)
#define WRITE_4BYTE(mtk_dp, reg, val) \
	mtk_dp_write(mtk_dp, reg, val)
#define WRITE_BYTE_MASK(mtk_dp, addr, val, mask) \
	mtk_dp_write_byte(mtk_dp, addr, val, mask)
#define WRITE_2BYTE_MASK(mtk_dp, addr, val, mask) \
	mtk_dp_mask(mtk_dp, addr, val, mask)
#define WRITE_4BYTE_MASK(mtk_dp, addr, val, mask) \
	mtk_dp_mask(mtk_dp, addr, val, mask)

enum audio_m_div {
	DP_AUDIO_M_DIV_M,
	DP_AUDIO_M_DIV_M2,
	DP_AUDIO_M_DIV_M4,
	DP_AUDIO_M_DIV_M8,
	DP_AUDIO_M_DIV_D2,
	DP_AUDIO_M_DIV_D4,
	DP_AUDIO_M_DIV_NA,
	DP_AUDIO_M_DIV_D8,
};

enum dp_color_depth {
	DP_COLOR_DEPTH_6BIT,
	DP_COLOR_DEPTH_8BIT,
	DP_COLOR_DEPTH_10BIT,
	DP_COLOR_DEPTH_12BIT,
	DP_COLOR_DEPTH_16BIT,
	DP_COLOR_DEPTH_UNKNOWN,
};

enum dp_encoder_id {
	DP_ENCODER_ID_0 = 0,
	DP_ENCODER_ID_1 = 1,
	DP_ENCODER_ID_MAX
};

enum dp_lane_count {
	DP_1LANE = 0x01,
	DP_2LANE = 0x02,
	DP_4LANE = 0x04,
};

enum dp_lane_num {
	DP_LANE0,
	DP_LANE1,
	DP_LANE2,
	DP_LANE3,
	DP_LANE_MAX,
};

enum dp_link_rate {
	DP_LINK_RATE_RBR = 0x6,
	DP_LINK_RATE_HBR = 0xa,
	DP_LINK_RATE_HBR2 = 0x14,
	DP_LINK_RATE_HBR25 = 0x19,
	DP_LINK_RATE_HBR3 = 0x1e,
	DP_LINK_RATE_UHBR10  = 0x20,
};

enum dp_lt_pattern {
	DP_0 = 0,
	DP_TPS1 = BIT(4),
	DP_TPS2 = BIT(5),
	DP_TPS3 = BIT(6),
	DP_TPS4 = BIT(7),
	DP_20_TPS1 = BIT(14),
	DP_20_TPS2 = BIT(15),
};

enum dp_pg_typesel {
	DP_PG_NONE,
	DP_PG_PURE_COLOR,
	DP_PG_VERTICAL_RAMPING,
	DP_PG_HORIZONTAL_RAMPING,
	DP_PG_VERTICAL_COLOR_BAR,
	DP_PG_HORIZONTAL_COLOR_BAR,
	DP_PG_CHESSBOARD_PATTERN,
	DP_PG_SUB_PIXEL_PATTERN,
	DP_PG_FRAME_PATTERN,
	DP_PG_MAX,
};

enum dp_preemphasis_num {
	DP_PREEMPHASIS0,
	DP_PREEMPHASIS1,
	DP_PREEMPHASIS2,
	DP_PREEMPHASIS3,
};

enum dp_swing_num {
	DP_SWING0,
	DP_SWING1,
	DP_SWING2,
	DP_SWING3,
};

enum dp_return_status {
	DP_RET_NOERR,
	DP_RET_PLUG_OUT,
	DP_RET_TIMEOU,
	DP_RET_AUTH_FAIL,
	DP_RET_EDID_FAIL,
	DP_RET_TRANING_FAIL,
	DP_RET_RETRANING,
};

enum dp_video_mode {
	DP_VIDEO_INTERLACE,
	DP_VIDEO_PROGRESSIVE,
};

enum dptx_video_state {
	DPTX_STATE_INITIAL	 = 0,
	DPTX_STATE_IDLE		 = 1,
	DPTX_STATE_AUTH		 = 2,
	DPTX_STATE_PREPARE	 = 3,
	DPTX_STATE_NORMAL	 = 4,
};

union mtk_dp_misc {
	struct {
		u8 is_sync_clock : 1;
		u8 color_format : 2;
		u8 spec_def1 : 2;
		u8 color_depth : 3;

		u8 interlaced : 1;
		u8 stereo_attr : 2;
		u8 reserved : 3;
		u8 is_vsc_sdp : 1;
		u8 spec_def2 : 1;
	} misc;
	u8 misc_raw[2];
};

union mtk_dp_pps {
	struct{
		u8 major : 4;
		u8 minor : 4;               /* pps0 */
		u8 pps_id : 8;              /* pps1 */
		u8 reserved1 : 8;           /* pps2 */
		u8 color_depth : 4;
		u8 buffer_depth : 4;        /* pps3 */
		u8 reserved2 : 2;
		bool bp_enable : 1;
		bool convert_rgb : 1;
		bool simple_422 : 1;
		bool vbr_enable : 1;
		u16 bit_per_pixel : 10;      /* pps4-5 */
		u16 pic_height : 16;         /* pps6-7 */
		u16 pic_width : 16;          /* pps8-9 */
		u16 slice_height : 16;       /* pps10-11 */
		u16 slice_width : 16;        /* pps12-13 */
		u16 chunk_size : 16;         /* pps14-15 */
		u8 reserved3 : 6;
		u16 init_xmit_delay : 10;    /* pps16-17 */
		u16 init_dec_delay : 16;     /* pps18-19 */
		u16 reserved4 : 10;
		u8 init_scale_val : 6;       /* pps20-21 */
		u16 scale_inc_interval : 16; /* pps22-23 */
		u8 reserved5 : 4;
		u16 scale_dec_interval : 12; /* pps24-25 */
		u16 reserved6 : 11;
		u8 first_line_offset : 5;    /* pps26-27 */
		u16 nfl_bpg_offset : 16;     /* pps28-29 */
		u16 slice_bpg_offset : 16;   /* pps30-31 */
		u16 init_offset : 16;        /* pps32-33 */
		u16 final_offset : 16;       /* pps34-35 */
		u8 reserved7 : 3;
		u8 min_qp : 5;               /* pps36 */
		u8 reserved8 : 3;
		u8 max_qp : 5;               /* pps37 */
		u8 rc_param_set[50];         /* pps38-87 */

		u8 reserved9 : 6;
		bool native_420 : 1;
		bool native_422 : 1;         /* pps88 */
		u8 reserved10 : 3;
		u8 sec_line_bpg_offset : 5;  /* pps89 */
		u16 nsl_bpg_offset : 16;     /* pps90-91 */
		u16 sec_line_offset : 16;    /* pps92-93 */
		u8 reserved11[34];           /* pps94-127 */
	} pps;

	u8 pps_raw[128];
};

struct mtk_dp_efuse_fmt {
	unsigned short idx;
	unsigned short shift;
	unsigned short mask;
	unsigned short min_val;
	unsigned short max_val;
	unsigned short default_val;
};

struct mtk_dp_timing_parameter {
	u16 htt;
	u16 hde;
	u16 hbk;
	u16 hfp;
	u16 hsw;

	bool hsp;
	u16 hbp;
	u16 vtt;
	u16 vde;
	u16 vbk;
	u16 vfp;
	u16 vsw;

	bool vsp;
	u16 vbp;
	u8 frame_rate;
	u64 pixcel_rate;
	int video_ip_mode;
	union mtk_dp_misc misc;
};

struct mtk_dp_audio_cfg {
	bool detect_monitor;
	int sad_count;
	int sample_rate;
	int word_length_bits;
	int channels;
};

struct mtk_dp_info {
	struct mtk_dp_timing_parameter dp_output_timing;
	struct mtk_dp_audio_cfg audio_cur_cfg;
	union mtk_dp_pps pps;
	bool pattern_gen : 1;
	bool audio_mute : 1;
	bool video_mute : 1;
	u8 depth;
	u8 format;
	u32 video_m;
	u32 video_n;
};

struct mtk_dp_train_info {
	bool sink_ext_cap_en : 1;
	bool tps3_support : 1;
	bool tps4_support : 1;
	bool sink_ssc_en : 1;
	bool dp_tx_auto_test_en : 1;
	bool cable_plug_in : 1;
	bool cable_state_change : 1;
	bool dp_mst_cap : 1;
	bool dp_mst_en : 1;
	bool dp_mst_branch : 1;
	bool dwn_strm_port_present : 1;
	bool cr_done : 1;
	bool eq_done : 1;

	u8 dp_version;
	u8 max_link_rate;
	u8 max_link_lane_count;
	u8 link_rate;
	u8 link_lane_count;
	u16 phy_status;
	u8 dpcd_rev;
	u8 sink_count;
	u8 check_cap_times;
	u8 ssc_delta;
};

struct mtk_dp_phy_parameter {
	unsigned char c0;
	unsigned char cp1;
};

struct mtk_dp_con {
	struct mtk_dp *mtk_dp;
	struct drm_dp_mst_port *port;
	struct drm_connector connector;
	struct drm_encoder *encoder;
	enum dp_encoder_id encoder_id;

	struct drm_dp_aux *dsc_aux;
	u8 dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE];
	u8 fec_cap;
	u8 dsc_hblank_expansion_quirk:1;
	u8 dsc_decompression_enabled:1;

	struct edid *edid;
	//enum drm_dp_mst_mode dp_mode;
	bool video_enable;
};

struct mtk_dp_bridge {
	struct mtk_dp *mtk_dp;
	struct drm_bridge bridge;
	enum dp_encoder_id encoder_id;
};

struct mtk_dp_cts_auto_dptx_timing {
	u16 Htt;
	u16 Hde;
	u16 Hbk;
	u16 Hfp;
	u16 Hsw;

	bool bHsp;
	u16 Hbp;
	u16 Vtt;
	u16 Vde;
	u16 Vbk;
	u16 Vfp;
	u16 Vsw;

	bool bVsp;
	u16 Vbp;
	u8 FrameRate;
	u32 PixRateKhz;
	int Video_ip_mode;
};

struct mtk_dp_cts_auto_req {
	u32 base;
	struct drm_dp_aux *aux;
	struct mtk_dp_train_info train_info;
	void __iomem *regs;
	void __iomem *phyd_regs;
	u8 has_fec:1;
	enum dptx_video_state video_state;
	enum dptx_video_state video_state_pre;
	u8 swap_enable: 1;

	struct edid *edid;
	u8 rx_cap[16];
	u8 input_src;
	bool is_edp;
	int training_state;
	bool bDPTxAutoTest_EN;
	bool bSinkEXTCAP_En;
	u8 ubLinkRate;
	u8 ubLinkLaneCount;
	u8 ubSysMaxLinkRate;
	u8 ubSinkCountNum;
	bool bSinkSSC_En;
	bool cr_done;
	bool eq_done;
	bool bTPS3;
	bool bTPS4;
	unsigned int test_link_training;
	unsigned int test_pattern_req;
	unsigned int test_edid_read;
	unsigned int test_link_rate;	//06h:1.62Gbps, 0Ah:2.7Gbps
	unsigned int test_lane_count;
	//01h:color ramps,02h:black&white vertical,03h:color square
	unsigned int test_pattern;
	unsigned int test_h_total;
	unsigned int test_v_total;
	unsigned int test_h_start;
	unsigned int test_v_start;
	unsigned int test_hsync_width;
	unsigned int test_hsync_polarity;
	unsigned int test_vsync_width;
	unsigned int test_vsync_polarity;
	unsigned int test_h_width;
	unsigned int test_v_height;
	unsigned int test_sync_clk;
	unsigned int test_color_fmt;
	unsigned int test_dynamic_range;
	unsigned int test_YCbCr_coefficient;
	unsigned int test_bit_depth;
	unsigned int test_refresh_denominator;
	unsigned int test_interlaced;
	unsigned int test_refresh_rate_numerator;
	unsigned int test_aduio_channel_count;
	unsigned int test_aduio_samling_rate;
	struct mtk_dp_cts_auto_dptx_timing dp_cts_outbl;
};

struct mtk_dp {
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_bridge bridge;
	struct drm_bridge *next_bridge;
	const struct mtk_dp_data *data;
	int id;
	struct drm_dp_aux aux;
	u8 rx_cap[DP_RECEIVER_CAP_SIZE];
	struct drm_display_mode mode[DP_ENCODER_NUM];
	struct mtk_dp_info info[DP_ENCODER_NUM];
	struct mtk_dp_train_info train_info;
	struct notifier_block notifier;
	struct clk *pclk;
	struct clk *pclk_src;
	atomic_t refcount;
	int irq;

	struct device *genpd_dp_tx;
	struct device *genpd_dp_phy;
	struct device_link *genpd_dl_dp_tx;
	struct device_link *genpd_dl_dp_phy;

	/* irq_thread_lock is used to protect phy_status */
	spinlock_t irq_thread_lock;

	//struct mtk_hdcp_info hdcp_info;
	struct work_struct hdcp_enable_work;
	struct work_struct hdcp_disable_work;
	struct work_struct prop_work;
	struct delayed_work check_work;
	struct workqueue_struct *hdcp_workqueue;
	struct drm_connector_state con_state[DP_ENCODER_NUM];
	/* This mutex is used to synchronize HDCP operations in the driver */
	struct mutex hdcp_mutex;
	/* This mutex is used to synchronize MST operations in the driver */
	struct mutex mst_mutex;

	void __iomem *regs;
	void __iomem *phyd_regs;
	void __iomem *phy_mux_regs;
	void __iomem *mac_power_regs;

	int disp_state;
	bool dp_ready;

	struct mtk_drm_private *priv;
	struct mtk_dp_phy_parameter phy_params[DP_PHY_LEVEL_COUNT];

	bool swap_enable;
	bool phy_flip_enable;
	bool need_debounce;

	bool init_property;
	struct drm_property *prop_dsc_enable[DP_ENCODER_NUM];
	struct drm_property *prop_dsc_cfg[DP_ENCODER_NUM];
	bool dsc_enable[DP_ENCODER_NUM];

	bool mst_enable;
	bool mst_start;
	u8 mst_enable_count;
	struct drm_dp_mst_topology_mgr mgr;
	struct mtk_dp_con *mtk_con[DP_CONNECTOR_NUM];
	struct mtk_dp_bridge *mtk_bridge[DP_ENCODER_NUM];

	/* For audio */
	bool audio_enable;
	hdmi_codec_plugged_cb plugged_cb;
	struct platform_device *audio_pdev;

	struct device *codec_dev;
	struct device *vdisp_ao_dev;
	/* protect the plugged_cb as it's used in both bridge ops and audio */
	struct mutex update_plugged_status_lock;

	struct mtk_dp_cts_auto_req cts_req;

	bool dpoc;
	bool dpoc_hpd;
	struct typec_switch_dev *sw;
};

struct mtk_dp_data {
	int bridge_type;
	unsigned int smc_cmd;
	const struct mtk_dp_efuse_fmt *efuse_fmt;
	bool audio_supported;
	bool audio_pkt_in_hblank_area;
	u16 audio_m_div2_bit;
	bool dsc_support;
	bool mst_support;
	u16 max_hdisplay;
	u16 max_vdisplay;
	u16 min_hdisplay;
	u16 min_vdisplay;
	void (*mac_power)(struct mtk_dp *mtk_dp);
	void (*phy_patch)(struct mtk_dp *mtk_dp);
	u32 phyd_dig_glb_offset;
	u32 phyd_dig_lan0_offset;
	u32 phyd_dig_lan1_offset;
	u32 phyd_dig_lan2_offset;
	u32 phyd_dig_lan3_offset;
	u32 phyd_ana_glb_offset;
	u32 phyd_ana_lan0_offset;
	u32 phyd_ana_lan1_offset;
	u32 phyd_ana_lan2_offset;
	u32 phyd_ana_lan3_offset;
	u8 support_max_linkrate;
	u8 support_max_lanecount;
	u8 encoder_num;
	bool need_phy_lane_enable_set;
	bool need_phy_flip_set;
	u32 phy_4lane_ctrl_bit;
	u32 phy_flip_ctrl_bit;
	bool enable_2c_feature;
	void (*set_2c_switch)(struct mtk_dp *mtk_dp);
};


static inline int mtk_dp_con_id(struct mtk_dp *mtk_dp, struct mtk_dp_con *mtk_con)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mtk_dp->mtk_con); i++) {
		if (mtk_dp->mtk_con[i] && mtk_dp->mtk_con[i] == mtk_con)
			return i;
	}

	dev_err(mtk_dp->dev, "[DPTX] fail to find the connector id\n");
	return -ENODEV;
}

static int encoder_id_to_con_id(struct mtk_dp *mtk_dp, int encoder_id)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mtk_dp->mtk_con); i++) {
		if (mtk_dp->mtk_con[i] && mtk_dp->mtk_con[i]->encoder &&
		    mtk_dp->mtk_con[i]->encoder_id == encoder_id)
			return i;
	}

	dev_err(mtk_dp->dev, "[DPTX] fail to find the connector id by the encoder id\n");
	return -ENODEV;
}

static inline bool mst_con_with_encoder(struct mtk_dp_con *mtk_con)
{
	return false;
}

#endif /*_MTK_DP_V2_H_*/
