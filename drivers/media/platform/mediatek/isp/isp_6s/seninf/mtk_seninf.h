/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __MTK_CAM_SENINF_H__
#define __MTK_CAM_SENINF_H__

#include <linux/kthread.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>

#include <linux/kernel.h>
#include <linux/phy/phy.h>

#define MEDIA_DRVNAME "mtk-seninf"

#define SENINF_DEBUG

#define SENINF_BITS(base, reg, field, val) do { \
	u32 __iomem *__p = (base) + (reg); \
	u32 __v = readl(__p); \
	__v &= ~field##_MASK; \
	__v |= (((val) << field##_SHIFT) & field##_MASK); \
	writel(__v, __p); \
} while (0)

#define SENINF_READ_BITS(base, reg, field) \
({ \
	u32 __iomem *__p = (base) + (reg); \
	u32 __v = readl(__p); \
	__v &= field##_MASK; \
	__v >>= field##_SHIFT; \
	__v; \
})

#define SENINF_READ_REG(base, reg) \
({ \
	u32 __iomem *__p = (base) + (reg); \
	u32 __v = readl(__p); \
	__v; \
})

#define SENINF_WRITE_REG(base, reg, val) { \
	u32 __iomem *__p = (base) + (reg); \
	writel(val, __p); \
}

#define SENINF_VC_ROUTING
#define SENINF_VC_COUNT 1

#define SENINF_VC_MAXCNT 4
/* seninf mux/cam_mux pixel mode
 * 0: 1 pixel mode
 * 1: 2 pixel mode
 * 2: 4 pixel mode
 * 3: 8 pixel mode
 */
#define SENINF_DEF_PIXEL_MODE 1

#define SENINF_CLK_MARGIN_IN_PERCENT 0
#define HW_BUF_EFFECT 10

/* under 1.45 Gbps, trail should be enable */
#define ISP_CLK_LOW 273000000

#define DEFAULT_WIDTH			1600
#define DEFAULT_HEIGHT			1200

enum VC_FEATURE {
	VC_NONE = 0,
	VC_MIN_NUM,

	VC_GENERAL_DATA_MIN_NUM = VC_MIN_NUM,
	VC_GENERAL_EMBEDDED = VC_GENERAL_DATA_MIN_NUM,
	VC_GENERAL_DATA_MAX_NUM,

	VC_MAX_NUM = VC_GENERAL_DATA_MAX_NUM,
};

enum {
	PAD_SINK = 0,
	PAD_SRC_GENERAL0,
	PAD_MAXCNT,
	PAD_ERR = 0xffff,
};

enum CSI_PORT {
	CSI_PORT_0 = 0,
	CSI_PORT_1,
	CSI_PORT_0A,
	CSI_PORT_0B,
	CSI_PORT_1A,
	CSI_PORT_1B,
	CSI_PORT_MAX_NUM,
};

#define SENINF_CSI_PORT_NAMES \
	"0", \
	"1", \
	"0A", \
	"0B", \
	"1A", \
	"1B", \

enum SENINF_PHY_VER_ENUM {
	SENINF_PHY_2_1,
	SENINF_PHY_VER_NUM,
};

#define MTK_CSI_PHY_VERSIONS "mtk-csi-phy-2-1"

enum SENINF_ENUM {
	SENINF_1,
	SENINF_2,
	SENINF_3,
	SENINF_4,
	SENINF_NUM,
};

enum SENINF_MUX_ENUM {
	SENINF_MUX1,
	SENINF_MUX2,
	SENINF_MUX3,
	SENINF_MUX4,
	SENINF_MUX5,
	SENINF_MUX6,
	SENINF_MUX7,
	SENINF_MUX8,
	SENINF_MUX_NUM,

	SENINF_MUX_ERROR = -1,
};

enum SENINF_CAM_MUX_ENUM {
	SENINF_CAM_MUX0,
	SENINF_CAM_MUX1,
	SENINF_CAM_MUX2,
	SENINF_CAM_MUX3,
	SENINF_CAM_MUX4,
	SENINF_CAM_MUX5,
	SENINF_CAM_MUX6,
	SENINF_CAM_MUX7,
	SENINF_CAM_MUX_NUM,

	SENINF_CAM_MUX_ERR = 0xff
};

/* 0:CSI2(2.5G), 3: parallel, 8:NCSI2(1.5G) */
enum SENINF_SOURCE_ENUM {
	CSI2 = 0x0, /* 2.5G support */
	TEST_MODEL = 0x1,
	CCIR656	= 0x2,
	PARALLEL_SENSOR = 0x3,
	SERIAL_SENSOR = 0x4,
	HD_TV = 0x5,
	EXT_CSI2_OUT1 = 0x6,
	EXT_CSI2_OUT2 = 0x7,
	MIPI_SENSOR = 0x8, /* 1.5G support */
	VIRTUAL_CHANNEL_1 = 0x9,
	VIRTUAL_CHANNEL_2 = 0xA,
	VIRTUAL_CHANNEL_3 = 0xB,
	VIRTUAL_CHANNEL_4 = 0xC,
};

enum {
	CLK_CAMSYS_SENINF_CGPDN = 0,
	CLK_TOP_MUX_SENINF,
	CLK_TOP_MUX_SENINF1,
	CLK_TOP_SENINF_END,
	CLK_TOP_MUX_CAMTG = CLK_TOP_SENINF_END,
	CLK_TOP_MUX_CAMTG1,
	CLK_TOP_MUX_CAMTG2,
	CLK_TOP_F26M_CK_D2,
	CLK_TOP_UNIVP_192M_D8,
	CLK_TOP_UNIVPLL_D6_D8,
	CLK_TOP_UNIVP_192M_D4,
	CLK_TOP_CLK26M,
	CLK_TOP_UNIVP_192M_D16,
	CLK_TOP_UNIVP_192M_D32,
	CLK_MAXCNT,
	CLK_TOP_CAMTM,
};

#define SENINF_CLK_NAMES \
	"CAMSYS_SENINF_CGPDN", \
	"TOP_MUX_SENINF", \
	"TOP_MUX_SENINF1", \
	"TOP_MUX_CAMTG", \
	"TOP_MUX_CAMTG1", \
	"TOP_MUX_CAMTG2", \
	"TOP_F26M_CK_D2", \
	"TOP_UNIVP_192M_D8", \
	"TOP_UNIVPLL_D6_D8", \
	"TOP_UNIVP_192M_D4", \
	"TOP_CLK26M", \
	"TOP_UNIVP_192M_D16", \
	"TOP_UNIVP_192M_D32", \

struct seninf_mux {
	struct list_head list;
	int idx;
};

struct seninf_cam_mux {
	struct list_head list;
	int idx;
};

struct seninf_vc {
	u8 vc;
	u8 dt;
	u8 feature;
	u8 out_pad;
	u8 pixel_mode;
	u8 group;
	u8 mux; /* allocated per group */
	u8 cam; /* assigned by cam driver */
	u8 enable;
	u16 exp_hsize;
	u16 exp_vsize;
};

struct seninf_vcinfo {
	struct seninf_vc vc[SENINF_VC_MAXCNT];
	int cnt;
};

struct seninf_dfs {
	struct device *dev;
	struct regulator *reg;
	unsigned long *freqs;
	unsigned long *volts;
	int cnt;
};

struct mtk_seninf_work {
	struct kthread_work work;
	struct seninf_ctx *ctx;
	union work_data_t {
		unsigned int sof;
		void *data_ptr;
	} data;
};

struct seninf_core {
	struct media_device media_dev;
	struct v4l2_device v4l2_dev;
	struct device *dev;
	int pm_domain_cnt;
	struct device **pm_domain_devs;
	struct clk *clk[CLK_MAXCNT];
	struct seninf_dfs dfs;
	struct list_head list;
	struct list_head list_mux;
	struct seninf_mux mux[SENINF_MUX_NUM];
#ifdef SENINF_DEBUG
	struct list_head list_cam_mux;
	struct seninf_cam_mux cam_mux[SENINF_CAM_MUX_NUM];
#endif
	struct mutex mutex;  /* protect seninf core operations */
	void __iomem *reg_if;
	int refcnt;

	unsigned int num_seninf_drivers;

	/* protect variables in irq handler */
	spinlock_t spinlock_irq;

	/* mipi error detection count */
	unsigned int detection_cnt;
	/* enable csi irq flag */
	unsigned int csi_irq_en_flag;
};

#define PHY_NAME_LEN 5

struct mtk_mbus_frame_desc_entry_csi2 {
	u8 channel;
	u8 data_type;
	u8 enable;
	u16 hsize;
	u16 vsize;
	u16 user_data_desc;
};

struct mtk_mbus_frame_desc_entry {
	//enum v4l2_mbus_frame_desc_flags flags;
	//u32 pixelcode;
	//u32 length;
	union {
		struct mtk_mbus_frame_desc_entry_csi2 csi2;
	} bus;
};
#define MTK_FRAME_DESC_ENTRY_MAX 8

enum mtk_mbus_frame_desc_type {
	MTK_MBUS_FRAME_DESC_TYPE_PLATFORM,
	MTK_MBUS_FRAME_DESC_TYPE_PARALLEL,
	MTK_MBUS_FRAME_DESC_TYPE_CCP2,
	MTK_MBUS_FRAME_DESC_TYPE_CSI2,
};

struct mtk_mbus_frame_desc {
	enum mtk_mbus_frame_desc_type type;
	struct mtk_mbus_frame_desc_entry entry[MTK_FRAME_DESC_ENTRY_MAX];
	unsigned short num_entries;
};

struct mtk_csi_param {
	__u8 dphy_trail;
	__u8 dphy_data_settle;
	__u8 dphy_clk_settle;
	__u8 cphy_settle;
};

struct seninf_ctx {
	struct v4l2_subdev subdev;
	struct v4l2_async_notifier notifier;
	struct device *dev;
	struct v4l2_ctrl_handler ctrl_handler;
	struct media_pad pads[PAD_MAXCNT];
	struct v4l2_subdev_format fmt[PAD_MAXCNT];
	struct seninf_core *core;
	struct list_head list;

	struct phy *phy;
	char phy_name[PHY_NAME_LEN];

	u32 port;
	u32 port_a;
	u32 port_b;
	u32 port_num;
	u32 num_data_lanes;
	s64 mipi_pixel_rate;
	s64 buffered_pixel_rate;
	s64 customized_pixel_rate;
	unsigned int m_csi_efuse;

	unsigned int is_4d1c:1;
	unsigned int is_cphy:1;
	unsigned int is_test_model:4;
#ifdef SENINF_DEBUG
	unsigned int is_test_streamon:1;
#endif
	unsigned int is_secure:1;
	unsigned int sec_info_addr;
	u32 seninf_idx;
	int pad2cam[PAD_MAXCNT];

	/* remote sensor */
	struct v4l2_subdev *sensor_sd;
	u32 sensor_pad_idx;

	/* remote engine */
	struct v4l2_subdev *engine_sd;
	u32 engine_pad_idx;

	/* provided by sensor */
	struct seninf_vcinfo vcinfo;
	int fps_n;
	int fps_d;

	/* dfs */
	int isp_freq;

	void __iomem *reg_if_top;
	void __iomem *reg_if_ctrl[SENINF_NUM];
	void __iomem *reg_if_cam_mux;
	void __iomem *reg_if_cam_mux_gcsr;
	void __iomem *reg_if_cam_mux_pcsr[SENINF_CAM_MUX_NUM];
	void __iomem *reg_if_tg[SENINF_NUM];
	void __iomem *reg_if_csi2[SENINF_NUM];
	void __iomem *reg_if_mux[SENINF_MUX_NUM];

	/* resources */
	struct list_head list_mux;
	struct list_head list_cam_mux;

	/* flags */
	unsigned int streaming:1;

	/*sensor mode customized csi params*/
	struct mtk_csi_param csi_param;

	int open_refcnt;
	struct mutex mutex;  /* protect seninf context */

	/* csi irq */
	unsigned int data_not_enough_cnt;
	unsigned int err_lane_resync_cnt;
	unsigned int crc_err_cnt;
	unsigned int ecc_err_double_cnt;
	unsigned int ecc_err_corrected_cnt;
	/* seninf_mux fifo overrun irq */
	unsigned int fifo_overrun_cnt;
	/* cam_mux h/v size irq */
	unsigned int size_err_cnt;
	/* error flag */
	unsigned int data_not_enough_flag;
	unsigned int err_lane_resync_flag;
	unsigned int crc_err_flag;
	unsigned int ecc_err_double_flag;
	unsigned int ecc_err_corrected_flag;
	unsigned int fifo_overrun_flag;
	unsigned int size_err_flag;
};

/* defined in phy-mtk-mipi-csi-2-1.c */
struct mtk_mipi_cdphy_port {
	struct device *dev;
	void __iomem *base;
	struct phy *phy;
	u32 type;
	u32 mode;
	u32 num_data_lanes;

	s64 mipi_pixel_rate;
	s64 buffered_pixel_rate;
	s64 customized_pixel_rate;

	u32 port;
	u32 port_a;
	u32 port_b;
	u32 port_num;
	u32 is_4d1c:1;
	u32 is_cphy:1;
	u32 m_csi_efuse;

	void __iomem *reg_ana_csi_rx[CSI_PORT_MAX_NUM];
	void __iomem *reg_ana_dphy_top[CSI_PORT_MAX_NUM];
	void __iomem *reg_ana_cphy_top[CSI_PORT_MAX_NUM];

	u32 cphy_settle_delay_dt;
	u32 dphy_settle_delay_dt;
	u32 settle_delay_ck;
	u32 hs_trail_parameter;
};

#endif
