#ifndef __AVT_CSI2_H__
#define __AVT_CSI2_H__

#include "avt_csi2_regs.h"

#define AVT_MAX_CTRLS 17

struct avt_frame_param {
	/* crop settings */
	struct v4l2_rect r;

	/* min/max/step values for frame size */
	uint32_t minh;
	uint32_t maxh;
	uint32_t sh;
	uint32_t minw;
	uint32_t maxw;
	uint32_t sw;
	uint32_t minhoff;
	uint32_t maxhoff;
	uint32_t shoff;
	uint32_t minwoff;
	uint32_t maxwoff;
	uint32_t swoff;
};

enum avt_mode {
	AVT_BCRM_MODE,
	AVT_GENCP_MODE,
};

#define AVT_CSI2_SENS_PAD_SOURCE_0	0
#define AVT_CSI2_SENS_PAD_SOURCE_1	1
#define AVT_CSI2_SENS_PAD_SOURCE_2	2
#define AVT_CSI2_SENS_PAD_SOURCE_3	3
#define AVT_CSI2_SENS_PADS_NUM		4

struct avt_csi2_priv {
	struct v4l2_subdev		subdev;
	struct media_pad		pads[AVT_CSI2_SENS_PADS_NUM];
	struct i2c_client		*client;
	u32				mbus_fmt_code;

	struct v4l2_captureparm		streamcap;
	struct v4l2_ctrl_handler	hdl;

	struct v4l2_ctrl_config		ctrl_cfg[AVT_MAX_CTRLS];
	struct v4l2_ctrl		*ctrls[AVT_MAX_CTRLS];

	bool is_open;
	bool stream_on;
	bool cross_update;
	bool write_handshake_available;

	uint32_t csi_clk_freq;
	int numlanes;
	struct avt_frame_param frmp;

	struct cci_reg cci_reg;
	struct gencp_reg gencp_reg;

	enum avt_mode mode;

	int32_t *available_fmts;
	uint32_t available_fmts_cnt;
};

struct avt_ctrl {
	__u32		id;
	__u32		value0;
	__u32		value1;
};

#define V4L2_AV_CSI2_BASE		0x1000
#define V4L2_AV_CSI2_WIDTH_R		(V4L2_AV_CSI2_BASE+0x0001)
#define V4L2_AV_CSI2_WIDTH_W		(V4L2_AV_CSI2_BASE+0x0002)
#define V4L2_AV_CSI2_WIDTH_MINVAL_R	(V4L2_AV_CSI2_BASE+0x0003)
#define V4L2_AV_CSI2_WIDTH_MAXVAL_R	(V4L2_AV_CSI2_BASE+0x0004)
#define V4L2_AV_CSI2_WIDTH_INCVAL_R	(V4L2_AV_CSI2_BASE+0x0005)
#define V4L2_AV_CSI2_HEIGHT_R		(V4L2_AV_CSI2_BASE+0x0006)
#define V4L2_AV_CSI2_HEIGHT_W		(V4L2_AV_CSI2_BASE+0x0007)
#define V4L2_AV_CSI2_HEIGHT_MINVAL_R	(V4L2_AV_CSI2_BASE+0x0008)
#define V4L2_AV_CSI2_HEIGHT_MAXVAL_R	(V4L2_AV_CSI2_BASE+0x0009)
#define V4L2_AV_CSI2_HEIGHT_INCVAL_R	(V4L2_AV_CSI2_BASE+0x000A)
#define V4L2_AV_CSI2_PIXELFORMAT_R	(V4L2_AV_CSI2_BASE+0x000B)
#define V4L2_AV_CSI2_PIXELFORMAT_W	(V4L2_AV_CSI2_BASE+0x000C)
#define V4L2_AV_CSI2_PALYLOADSIZE_R	(V4L2_AV_CSI2_BASE+0x000D)
#define V4L2_AV_CSI2_STREAMON_W		(V4L2_AV_CSI2_BASE+0x000E)
#define V4L2_AV_CSI2_STREAMOFF_W	(V4L2_AV_CSI2_BASE+0x000F)
#define V4L2_AV_CSI2_ABORT_W		(V4L2_AV_CSI2_BASE+0x0010)
#define V4L2_AV_CSI2_ACQ_STATUS_R	(V4L2_AV_CSI2_BASE+0x0011)
#define V4L2_AV_CSI2_HFLIP_R		(V4L2_AV_CSI2_BASE+0x0012)
#define V4L2_AV_CSI2_HFLIP_W		(V4L2_AV_CSI2_BASE+0x0013)
#define V4L2_AV_CSI2_VFLIP_R		(V4L2_AV_CSI2_BASE+0x0014)
#define V4L2_AV_CSI2_VFLIP_W		(V4L2_AV_CSI2_BASE+0x0015)
#define V4L2_AV_CSI2_OFFSET_X_W		(V4L2_AV_CSI2_BASE+0x0016)
#define V4L2_AV_CSI2_OFFSET_X_R		(V4L2_AV_CSI2_BASE+0x0017)
#define V4L2_AV_CSI2_OFFSET_X_MIN_R	(V4L2_AV_CSI2_BASE+0x0018)
#define V4L2_AV_CSI2_OFFSET_X_MAX_R	(V4L2_AV_CSI2_BASE+0x0019)
#define V4L2_AV_CSI2_OFFSET_X_INC_R	(V4L2_AV_CSI2_BASE+0x001A)
#define V4L2_AV_CSI2_OFFSET_Y_W		(V4L2_AV_CSI2_BASE+0x001B)
#define V4L2_AV_CSI2_OFFSET_Y_R		(V4L2_AV_CSI2_BASE+0x001C)
#define V4L2_AV_CSI2_OFFSET_Y_MIN_R	(V4L2_AV_CSI2_BASE+0x001D)
#define V4L2_AV_CSI2_OFFSET_Y_MAX_R	(V4L2_AV_CSI2_BASE+0x001E)
#define V4L2_AV_CSI2_OFFSET_Y_INC_R	(V4L2_AV_CSI2_BASE+0x001F)
#define V4L2_AV_CSI2_SENSOR_WIDTH_R	(V4L2_AV_CSI2_BASE+0x0020)
#define V4L2_AV_CSI2_SENSOR_HEIGHT_R	(V4L2_AV_CSI2_BASE+0x0021)
#define V4L2_AV_CSI2_MAX_WIDTH_R	(V4L2_AV_CSI2_BASE+0x0022)
#define V4L2_AV_CSI2_MAX_HEIGHT_R	(V4L2_AV_CSI2_BASE+0x0023)
#define V4L2_AV_CSI2_CURRENTMODE_R	(V4L2_AV_CSI2_BASE+0x0024)
#define V4L2_AV_CSI2_CHANGEMODE_W	(V4L2_AV_CSI2_BASE+0x0025)
#define V4L2_AV_CSI2_BAYER_PATTERN_R	(V4L2_AV_CSI2_BASE+0x0026)
#define V4L2_AV_CSI2_BAYER_PATTERN_W	(V4L2_AV_CSI2_BASE+0x0027)

/* Driver release version */
#define	MAJOR_DRV	0
#define	MINOR_DRV	6/* STREAMON_EX is modified */
#define	PATCH_DRV	0
#define	BUILD_DRV	0
#define DRIVER_VERSION	"0.6.0.0"	/* Major:Minor:Patch:Build */

#define BCRM_DEVICE_VERSION	0x00010000
#define BCRM_MAJOR_VERSION	0x0001
#define BCRM_MINOR_VERSION	0x0000

#define GCPRM_DEVICE_VERSION	0x00010000
#define GCPRM_MAJOR_VERSION	0x0001
#define GCPRM_MINOR_VERSION	0x0000

/* D-PHY 1.2 clock frequency range (up to 2.5 Gbps per lane, DDR) */
#define CSI_HOST_CLK_MIN_FREQ		40000000
#define CSI_HOST_CLK_MAX_FREQ		750000000
#define CSI_HOST_CLK_MAX_FREQ_4L	681250000
#define CSI_HOST_CLK_MAX_FREQ_4L_RGB24	425000000
#define CSI_HOST_CLK_MAX_FREQ_4L_BA10	425000000
#define CSI_HOST_CLK_MAX_FREQ_4L_RAW8	225000000

/* MIPI CSI-2 data types */
#define MIPI_DT_YUV420		0x18 /* YYY.../UYVY.... */
#define MIPI_DT_YUV420_LEGACY	0x1a /* UYY.../VYY...   */
#define MIPI_DT_YUV422		0x1e /* UYVY...         */
#define MIPI_DT_RGB444		0x20
#define MIPI_DT_RGB555		0x21
#define MIPI_DT_RGB565		0x22
#define MIPI_DT_RGB666		0x23
#define MIPI_DT_RGB888		0x24
#define MIPI_DT_RAW6		0x28
#define MIPI_DT_RAW7		0x29
#define MIPI_DT_RAW8		0x2a
#define MIPI_DT_RAW10		0x2b
#define MIPI_DT_RAW12		0x2c
#define MIPI_DT_RAW14		0x2d
#define MIPI_DT_CUSTOM		0x31

enum bayer_format {
	monochrome,/* 0 */
	bayer_gr,
	bayer_rg,
	bayer_gb,
	bayer_bg,
};

struct bcrm_to_v4l2 {
	int64_t min_bcrm;
	int64_t max_bcrm;
	int64_t step_bcrm;
	int32_t min_v4l2;
	int32_t max_v4l2;
	int32_t step_v4l2;
};

enum convert_type {
	min_enum,/* 0 */
	max_enum,
	step_enum,
};

#define CLEAR(x)       memset(&(x), 0, sizeof(x))

#define EXP_ABS		100000UL
#define UHZ_TO_HZ	1000000UL

#define CCI_REG_LAYOUT_MINVER_MASK (0x0000ffff)
#define CCI_REG_LAYOUT_MINVER_SHIFT (0)
#define CCI_REG_LAYOUT_MAJVER_MASK (0xffff0000)
#define CCI_REG_LAYOUT_MAJVER_SHIFT (16)

#define CCI_REG_LAYOUT_MINVER	0
#define CCI_REG_LAYOUT_MAJVER	1

#define AV_ATTR_REVERSE_X		{"Reverse X",		0}
#define AV_ATTR_REVERSE_Y		{"Reverse Y",		1}
#define AV_ATTR_INTENSITY_AUTO		{"Intensity Auto",	2}
#define AV_ATTR_BRIGHTNESS		{"Brightness",		3}
/* Red & Blue balance features are enabled by default since it doesn't have
 * option in BCRM FEATURE REGISTER
 */
#define AV_ATTR_RED_BALANCE		{"Red Balance",         3}
#define AV_ATTR_BLUE_BALANCE		{"Blue Balance",        3}
#define AV_ATTR_GAIN			{"Gain",		4}
#define AV_ATTR_GAMMA			{"Gamma",		5}
#define AV_ATTR_CONTRAST		{"Contrast",		6}
#define AV_ATTR_SATURATION		{"Saturation",		7}
#define AV_ATTR_HUE			{"Hue",			8}
#define AV_ATTR_WHITEBALANCE		{"White Balance",	9}
#define AV_ATTR_SHARPNESS		{"Sharpnesss",	       10}
#define AV_ATTR_EXPOSURE_AUTO		{"Exposure Auto",      11}
#define AV_ATTR_AUTOGAIN		{"Auto Gain",          12}
#define AV_ATTR_EXPOSURE		{"Exposure",	       13}
#define AV_ATTR_EXPOSURE_ABS		{"Exposure Absolute",  13}
#define AV_ATTR_WHITEBALANCE_AUTO	{"Auto White Balance", 14}

struct avt_ctrl_mapping {
	u8	reg_size;
	u8	data_size;
	u16	min_offset;
	u16	max_offset;
	u16	reg_offset;
	u16	step_offset;
	u32	id;
	u32	type;
	u32	flags;
	struct {
		s8	*name;
		u8	feature_avail;
	} attr;
};

const struct avt_ctrl_mapping avt_ctrl_mappings[] = {
	{
		.id			= V4L2_CID_BRIGHTNESS,
		.attr			= AV_ATTR_BRIGHTNESS,
		.min_offset		= BLACK_LEVEL_MIN_32R,
		.max_offset		= BLACK_LEVEL_MAX_32R,
		.reg_offset		= BLACK_LEVEL_32RW,
		.step_offset		= BLACK_LEVEL_INCREMENT_32R,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_32,
		.type			= V4L2_CTRL_TYPE_INTEGER,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_CONTRAST,
		.attr			= AV_ATTR_CONTRAST,
		.min_offset		= CONTRAST_VALUE_MIN_32R,
		.max_offset		= CONTRAST_VALUE_MAX_32R,
		.reg_offset		= CONTRAST_VALUE_32RW,
		.step_offset		= CONTRAST_VALUE_INCREMENT_32R,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_32,
		.type			= V4L2_CTRL_TYPE_INTEGER,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_SATURATION,
		.attr			= AV_ATTR_SATURATION,
		.min_offset		= SATURATION_MIN_32R,
		.max_offset		= SATURATION_MAX_32R,
		.reg_offset		= SATURATION_32RW,
		.step_offset		= SATURATION_INCREMENT_32R,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_32,
		.type			= V4L2_CTRL_TYPE_INTEGER,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_HUE,
		.attr			= AV_ATTR_HUE,
		.min_offset		= HUE_MIN_32R,
		.max_offset		= HUE_MAX_32R,
		.reg_offset		= HUE_32RW,
		.step_offset		= HUE_INCREMENT_32R,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_32,
		.type			= V4L2_CTRL_TYPE_INTEGER,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_AUTO_WHITE_BALANCE,
		.attr			= AV_ATTR_WHITEBALANCE_AUTO,
		.reg_offset		= WHITE_BALANCE_AUTO_8RW,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_8,
		.type			= V4L2_CTRL_TYPE_BOOLEAN,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_DO_WHITE_BALANCE,
		.attr			= AV_ATTR_WHITEBALANCE,
		.reg_offset		= WHITE_BALANCE_AUTO_8RW,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_8,
		.type			= V4L2_CTRL_TYPE_BUTTON,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_RED_BALANCE,
		.attr			= AV_ATTR_RED_BALANCE,
		.min_offset		= RED_BALANCE_RATIO_MIN_64R,
		.max_offset		= RED_BALANCE_RATIO_MAX_64R,
		.reg_offset		= RED_BALANCE_RATIO_64RW,
		.step_offset		= RED_BALANCE_RATIO_INCREMENT_64R,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_64,
		.type			= V4L2_CTRL_TYPE_INTEGER64,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_BLUE_BALANCE,
		.attr			= AV_ATTR_BLUE_BALANCE,
		.min_offset		= BLUE_BALANCE_RATIO_MIN_64R,
		.max_offset		= BLUE_BALANCE_RATIO_MAX_64R,
		.reg_offset		= BLUE_BALANCE_RATIO_64RW,
		.step_offset		= BLUE_BALANCE_RATIO_INCREMENT_64R,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_64,
		.type			= V4L2_CTRL_TYPE_INTEGER64,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_GAMMA,
		.attr			= AV_ATTR_GAMMA,
		.min_offset		= GAMMA_GAIN_MINIMUM_64R,
		.max_offset		= GAMMA_GAIN_MAXIMUM_64R,
		.reg_offset		= GAMMA_64RW,
		.step_offset		= GAMMA_GAIN_INCREMENT_64R,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_64,
		.type			= V4L2_CTRL_TYPE_INTEGER64,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_EXPOSURE,
		.attr			= AV_ATTR_EXPOSURE,
		.min_offset		= EXPOSURE_TIME_MIN_64R,
		.max_offset		= EXPOSURE_TIME_MAX_64R,
		.reg_offset		= EXPOSURE_TIME_64RW,
		.step_offset		= EXPOSURE_TIME_INCREMENT_64R,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_64,
		.type			= V4L2_CTRL_TYPE_INTEGER64,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_AUTOGAIN,
		.attr			= AV_ATTR_AUTOGAIN,
		.reg_offset		= GAIN_AUTO_8RW,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_8,
		.type			= V4L2_CTRL_TYPE_BOOLEAN,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_GAIN,
		.attr			= AV_ATTR_GAIN,
		.min_offset		= GAIN_MINIMUM_64R,
		.max_offset		= GAIN_MAXIMUM_64R,
		.reg_offset		= GAIN_64RW,
		.step_offset		= GAIN_INCREMENT_64R,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_64,
		.type			= V4L2_CTRL_TYPE_INTEGER64,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_HFLIP,
		.attr			= AV_ATTR_REVERSE_X,
		.reg_offset		= IMG_REVERSE_X_8RW,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_8,
		.type			= V4L2_CTRL_TYPE_BOOLEAN,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_VFLIP,
		.attr			= AV_ATTR_REVERSE_Y,
		.reg_offset		= IMG_REVERSE_Y_8RW,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_8,
		.type			= V4L2_CTRL_TYPE_BOOLEAN,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_SHARPNESS,
		.attr			= AV_ATTR_SHARPNESS,
		.min_offset		= SHARPNESS_MIN_32R,
		.max_offset		= SHARPNESS_MAX_32R,
		.reg_offset		= SHARPNESS_32RW,
		.step_offset		= SHARPNESS_INCREMENT_32R,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_32,
		.type			= V4L2_CTRL_TYPE_INTEGER,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_EXPOSURE_AUTO,
		.attr			= AV_ATTR_EXPOSURE_AUTO,
		.reg_offset		= EXPOSURE_AUTO_8RW,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_8,
		.type			= V4L2_CTRL_TYPE_MENU,
		.flags			= 0,
	},
	{
		.id			= V4L2_CID_EXPOSURE_ABSOLUTE,
		.attr			= AV_ATTR_EXPOSURE_ABS,
		.min_offset		= EXPOSURE_TIME_MIN_64R,
		.max_offset		= EXPOSURE_TIME_MAX_64R,
		.reg_offset		= EXPOSURE_TIME_64RW,
		.step_offset		= EXPOSURE_TIME_INCREMENT_64R,
		.reg_size		= AV_CAM_REG_SIZE,
		.data_size		= AV_CAM_DATA_SIZE_64,
		.type			= V4L2_CTRL_TYPE_INTEGER64,
		.flags			= 0,
	},
};

#endif
