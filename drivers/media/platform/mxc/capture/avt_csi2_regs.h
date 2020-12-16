#ifndef __AVT_CSI2_REGS_H__
#define __AVT_CSI2_REGS_H__

/* General Control Registers */
#define BCRM_VERSION_REG_32R			0x0000
#define FEATURE_INQUIRY_REG_64R			0x0008
#define FIRMWARE_VERSION_REG_64R		0x0010
#define WRITE_HANDSHAKE_REG_8RW			0x0018

/* Streaming Control Registers */
#define SUPPORTED_CSI2_LANE_COUNTS_8R		0x0040
#define CSI2_LANE_COUNT_8RW			0x0044
#define CSI2_CLOCK_MIN_32R			0x0048
#define CSI2_CLOCK_MAX_32R			0x004C
#define CSI2_CLOCK_32RW				0x0050
#define BUFFER_SIZE_32R				0x0054

/* Acquisition Control Registers */
#define ACQUISITION_START_8RW			0x0080
#define ACQUISITION_STOP_8RW			0x0084
#define ACQUISITION_ABORT_8RW			0x0088
#define ACQUISITION_STATUS_8R			0x008C
#define ACQUISITION_FRAME_RATE_64RW		0x0090
#define ACQUISITION_FRAME_RATE_MIN_64R		0x0098
#define ACQUISITION_FRAME_RATE_MAX_64R		0x00A0
#define ACQUISITION_FRAME_RATE_INCREMENT_64R	0x00A8
#define FRAME_RATE_ENABLE_8RW			0x00B0
#define FRAME_START_TRIGGER_MODE_8RW		0x00B4
#define FRAME_START_TRIGGER_SOURCE_8RW		0x00B8
#define FRAME_START_TRIGGER_ACTIVATION_8RW	0x00BC
#define TRIGGER_SOFTWARE_8W			0x00C0

/* Image Format Control Registers */
#define IMG_WIDTH_32RW				0x0100
#define IMG_WIDTH_MIN_32R			0x0104
#define IMG_WIDTH_MAX_32R			0x0108
#define IMG_WIDTH_INCREMENT_32R			0x010C

#define IMG_HEIGHT_32RW				0x0110
#define IMG_HEIGHT_MIN_32R			0x0114
#define IMG_HEIGHT_MAX_32R			0x0118
#define IMG_HEIGHT_INCREMENT_32R		0x011C

#define IMG_OFFSET_X_32RW			0x0120
#define IMG_OFFSET_X_MIN_32R			0x0124
#define IMG_OFFSET_X_MAX_32R			0x0128
#define IMG_OFFSET_X_INCREMENT_32R		0x012C

#define IMG_OFFSET_Y_32RW			0x0130
#define IMG_OFFSET_Y_MIN_32R			0x0134
#define IMG_OFFSET_Y_MAX_32R			0x0138
#define IMG_OFFSET_Y_INCREMENT_32R		0x013C

#define IMG_MIPI_DATA_FORMAT_32RW		0x0140
#define IMG_AVAILABLE_MIPI_DATA_FORMATS_64R	0x0148

#define IMG_BAYER_PATTERN_INQUIRY_8R		0x0150
#define IMG_BAYER_PATTERN_8RW			0x0154

#define IMG_REVERSE_X_8RW			0x0158
#define IMG_REVERSE_Y_8RW			0x015C

#define SENSOR_WIDTH_32R			0x0160
#define SENSOR_HEIGHT_32R			0x0164

#define WIDTH_MAX_32R				0x0168
#define HEIGHT_MAX_32R				0x016C

/* Brightness Control Registers */
#define EXPOSURE_TIME_64RW			0x0180
#define EXPOSURE_TIME_MIN_64R			0x0188
#define EXPOSURE_TIME_MAX_64R			0x0190
#define EXPOSURE_TIME_INCREMENT_64R		0x0198
#define EXPOSURE_AUTO_8RW			0x01A0

#define INTENSITY_AUTO_PRECEDENCE_8RW		0x01A4
#define INTENSITY_AUTO_PRECEDENCE_VALUE_32RW	0x01A8
#define INTENSITY_AUTO_PRECEDENCE_MIN_32R	0x01AC
#define INTENSITY_AUTO_PRECEDENCE_MAX_32R	0x01B0
#define INTENSITY_AUTO_PRECEDENCE_INCREMENT_32R	0x01B4

#define BLACK_LEVEL_32RW			0x01B8
#define BLACK_LEVEL_MIN_32R			0x01BC
#define BLACK_LEVEL_MAX_32R			0x01C0
#define BLACK_LEVEL_INCREMENT_32R		0x01C4

#define GAIN_64RW				0x01C8
#define GAIN_MINIMUM_64R			0x01D0
#define GAIN_MAXIMUM_64R			0x01D8
#define GAIN_INCREMENT_64R			0x01E0
#define GAIN_AUTO_8RW				0x01E8

#define GAMMA_64RW				0x01F0
#define GAMMA_GAIN_MINIMUM_64R			0x01F8
#define GAMMA_GAIN_MAXIMUM_64R			0x0200
#define GAMMA_GAIN_INCREMENT_64R		0x0208

#define CONTRAST_ENABLE_8RW			0x0210
#define CONTRAST_VALUE_32RW			0x0214
#define CONTRAST_VALUE_MIN_32R			0x0218
#define CONTRAST_VALUE_MAX_32R			0x021C
#define CONTRAST_VALUE_INCREMENT_32R		0x0220

/* Color Management Registers */
#define SATURATION_32RW				0x0240
#define SATURATION_MIN_32R			0x0244
#define SATURATION_MAX_32R			0x0248
#define SATURATION_INCREMENT_32R		0x024C

#define HUE_32RW				0x0250
#define HUE_MIN_32R				0x0254
#define HUE_MAX_32R				0x0258
#define HUE_INCREMENT_32R			0x025C

#define RED_BALANCE_RATIO_64RW			0x0280
#define RED_BALANCE_RATIO_MIN_64R		0x0288
#define RED_BALANCE_RATIO_MAX_64R		0x0290
#define RED_BALANCE_RATIO_INCREMENT_64R		0x0298

#define BLUE_BALANCE_RATIO_64RW			0x02C0
#define BLUE_BALANCE_RATIO_MIN_64R		0x02C8
#define BLUE_BALANCE_RATIO_MAX_64R		0x02D0
#define BLUE_BALANCE_RATIO_INCREMENT_64R	0x02D8

#define WHITE_BALANCE_AUTO_8RW			0x02E0

/* Other Registers */
#define SHARPNESS_32RW				0x0300
#define SHARPNESS_MIN_32R			0x0304
#define SHARPNESS_MAX_32R			0x0308
#define SHARPNESS_INCREMENT_32R			0x030C

#define _BCRM_LAST_ADDR SHARPNESS_INCREMENT_32R

/* GenCP Registers */
#define GENCP_CHANGEMODE_8W			0x021C
#define GENCP_CURRENTMODE_8R			0x021D

#define GENCP_OUT_HANDSHAKE_8RW			0x0018
#define GENCP_IN_HANDSHAKE_8RW			0x001c
#define GENCP_OUT_SIZE_16W			0x0020
#define GENCP_IN_SIZE_16R			0x0024

enum CCI_REG_INFO {
	CCI_REGISTER_LAYOUT_VERSION = 0,
	RESERVED4BIT,
	DEVICE_CAPABILITIES,
	GCPRM_ADDRESS,
	RESERVED2BIT,
	BCRM_ADDRESS,
	RESERVED2BIT_2,
	DEVICE_GUID,
	MANUFACTURER_NAME,
	MODEL_NAME,
	FAMILY_NAME,
	DEVICE_VERSION,
	MANUFACTURER_INFO,
	SERIAL_NUMBER,
	USER_DEFINED_NAME,
	CHECKSUM,
	CHANGE_MODE,
	CURRENT_MODE,
	SOFT_RESET,
	MAX_CMD = SOFT_RESET
};

struct cci_cmd {
	__u8 command_index; /* diagnostc test name */
	const __u32 address; /* NULL for no alias name */
	__u32 byte_count;
};

static struct cci_cmd cci_cmd_tbl[MAX_CMD] = {
	/* command index		address */
	{ CCI_REGISTER_LAYOUT_VERSION,	0x0000, 4 },
	{ DEVICE_CAPABILITIES,		0x0008, 8 },
	{ GCPRM_ADDRESS,		0x0010, 2 },
	{ BCRM_ADDRESS,			0x0014, 2 },
	{ DEVICE_GUID,			0x0018, 64 },
	{ MANUFACTURER_NAME,		0x0058, 64 },
	{ MODEL_NAME,			0x0098, 64 },
	{ FAMILY_NAME,			0x00D8, 64 },
	{ DEVICE_VERSION,		0x0118, 64 },
	{ MANUFACTURER_INFO,		0x0158, 64 },
	{ SERIAL_NUMBER,		0x0198, 64 },
	{ USER_DEFINED_NAME,		0x01D8, 64 },
	{ CHECKSUM,			0x0218, 4 },
	{ CHANGE_MODE,			0x021C, 1 },
	{ CURRENT_MODE,			0x021D, 1 },
	{ SOFT_RESET,			0x021E, 1 },
};

struct __attribute__((__packed__)) cci_reg {
	__u32   layout_version;
	__u32   reserved_4bit;
	__u64   device_capabilities;
	__u16   gcprm_address;
	__u16   reserved_2bit;
	__u16   bcrm_addr;
	__u16   reserved_2bit_2;
	char    device_guid[64];
	char    manufacturer_name[64];
	char    model_name[64];
	char    family_name[64];
	char    device_version[64];
	char    manufacturer_info[64];
	char    serial_number[64];
	char    user_defined_name[64];
	__u32   checksum;
	__u8    change_mode;
	__u8    current_mode;
	__u8    soft_reset;
};

struct __attribute__((__packed__)) gencp_reg {
	__u32   gcprm_layout_version;
	__u16   gencp_out_buffer_address;
	__u16   reserved_2byte;
	__u16   gencp_out_buffer_size;
	__u16   reserved_2byte_1;
	__u16	gencp_in_buffer_address;
	__u16   reserved_2byte_2;
	__u16   gencp_in_buffer_size;
	__u16   reserved_2byte_3;
	__u32   checksum;
};

union cci_device_caps_reg {
	struct {
	unsigned long long user_name:1;
	unsigned long long bcrm:1;
	unsigned long long gencp:1;
	unsigned long long reserved:1;
	unsigned long long string_encoding:4;
	unsigned long long family_name:1;
	unsigned long long reserved2:55;
	} caps;

	unsigned long long value;
};

union bcrm_feature_reg {
	struct {
	unsigned long long reverse_x_avail:1;
	unsigned long long reverse_y_avail:1;
	unsigned long long intensity_auto_prcedence_avail:1;
	unsigned long long black_level_avail:1;
	unsigned long long gain_avail:1;
	unsigned long long gamma_avail:1;
	unsigned long long contrast_avail:1;
	unsigned long long saturation_avail:1;
	unsigned long long hue_avail:1;
	unsigned long long white_balance_avail:1;
	unsigned long long sharpness_avail:1;
	unsigned long long exposure_auto:1;
	unsigned long long gain_auto:1;
	unsigned long long white_balance_auto_avail:1;
	unsigned long long device_temperature_avail:1;
	unsigned long long acquisition_abort:1;
	unsigned long long acquisition_frame_rate:1;
	unsigned long long frame_trigger:1;
	unsigned long long reserved:46;
	} feature_inq;

	unsigned long long value;
};

union bcrm_avail_mipi_reg {
	struct {
	unsigned long long yuv420_8_leg_avail:1;
	unsigned long long yuv420_8_avail:1;
	unsigned long long yuv420_10_avail:1;
	unsigned long long yuv420_8_csps_avail:1;
	unsigned long long yuv420_10_csps_avail:1;
	unsigned long long yuv422_8_avail:1;
	unsigned long long yuv422_10_avail:1;
	unsigned long long rgb888_avail:1;
	unsigned long long rgb666_avail:1;
	unsigned long long rgb565_avail:1;
	unsigned long long rgb555_avail:1;
	unsigned long long rgb444_avail:1;
	unsigned long long raw6_avail:1;
	unsigned long long raw7_avail:1;
	unsigned long long raw8_avail:1;
	unsigned long long raw10_avail:1;
	unsigned long long raw12_avail:1;
	unsigned long long raw14_avail:1;
	unsigned long long jpeg_avail:1;
	unsigned long long reserved:45;
	} avail_mipi;

	unsigned long long value;
};

union bcrm_bayer_inquiry_reg {
	struct {
	unsigned char monochrome_avail:1;
	unsigned char bayer_GR_avail:1;
	unsigned char bayer_RG_avail:1;
	unsigned char bayer_GB_avail:1;
	unsigned char bayer_BG_avail:1;
	unsigned char reserved:3;
	} bayer_pattern;

	unsigned char value;
};

union bcrm_supported_lanecount_reg {
	struct {
	unsigned char one_lane_avail:1;
	unsigned char two_lane_avail:1;
	unsigned char three_lane_avail:1;
	unsigned char four_lane_avail:1;
	unsigned char reserved:4;
	} lane_count;

	unsigned char value;
};

#define AV_CAM_REG_SIZE		2
#define AV_CAM_DATA_SIZE_8	1
#define AV_CAM_DATA_SIZE_16	2
#define AV_CAM_DATA_SIZE_32	4
#define AV_CAM_DATA_SIZE_64	8

#endif
