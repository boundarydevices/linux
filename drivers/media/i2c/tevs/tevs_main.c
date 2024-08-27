#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/media.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include "tevs_tbls.h"

#define DRIVER_NAME "tevs"

/* Define host command register of TEVS information page */
#define HOST_COMMAND_TEVS_INFO_VERSION_MSB 						(0x3000)
#define HOST_COMMAND_TEVS_INFO_VERSION_LSB 						(0x3002)
#define HOST_COMMAND_TEVS_BOOT_STATE 							(0x3004)

/* Define host command register of ISP control page */
#define HOST_COMMAND_ISP_CTRL_PREVIEW_WIDTH 					(0x3100)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_HEIGHT 					(0x3102)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_FORMAT 					(0x3104)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_SENSOR_MODE 				(0x3106)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_THROUGHPUT 				(0x3108)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_MAX_FPS 					(0x310A)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_UPPER_MSB 		(0x310C)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_UPPER_LSB 		(0x310E)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_MAX_MSB 			(0x3110)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_MAX_LSB 			(0x3112)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_HINF_CTRL 				(0x3114)
#define HOST_COMMAND_ISP_CTRL_AE_MODE 							(0x3116)
#define HOST_COMMAND_ISP_CTRL_EXP_TIME_MSB 						(0x3118)
#define HOST_COMMAND_ISP_CTRL_EXP_TIME_LSB 						(0x311A)
#define HOST_COMMAND_ISP_CTRL_EXP_TIME_MAX_MSB 					(0x311C)
#define HOST_COMMAND_ISP_CTRL_EXP_TIME_MAX_LSB 					(0x311E)
#define HOST_COMMAND_ISP_CTRL_EXP_TIME_MIN_MSB 					(0x3120)
#define HOST_COMMAND_ISP_CTRL_EXP_TIME_MIN_LSB 					(0x3122)
#define HOST_COMMAND_ISP_CTRL_EXP_GAIN						 	(0x3124)
#define HOST_COMMAND_ISP_CTRL_EXP_GAIN_MAX 						(0x3126)
#define HOST_COMMAND_ISP_CTRL_EXP_GAIN_MIN 						(0x3128)
#define HOST_COMMAND_ISP_CTRL_CURRENT_EXP_TIME_MSB 				(0x312A)
#define HOST_COMMAND_ISP_CTRL_CURRENT_EXP_TIME_LSB 				(0x312C)
#define HOST_COMMAND_ISP_CTRL_CURRENT_EXP_GAIN 					(0x312E)
#define HOST_COMMAND_ISP_CTRL_BACKLIGHT_COMPENSATION 			(0x3130)
#define HOST_COMMAND_ISP_CTRL_BACKLIGHT_COMPENSATION_MAX 		(0x3132)
#define HOST_COMMAND_ISP_CTRL_BACKLIGHT_COMPENSATION_MIN 		(0x3134)
#define HOST_COMMAND_ISP_CTRL_AWB_MODE 							(0x3136)
#define HOST_COMMAND_ISP_CTRL_AWB_TEMP 							(0x3138)
#define HOST_COMMAND_ISP_CTRL_AWB_TEMP_MAX 						(0x313A)
#define HOST_COMMAND_ISP_CTRL_AWB_TEMP_MIN 						(0x313C)
#define HOST_COMMAND_ISP_CTRL_BRIGHTNESS 						(0x313E)
#define HOST_COMMAND_ISP_CTRL_BRIGHTNESS_MAX 					(0x3140)
#define HOST_COMMAND_ISP_CTRL_BRIGHTNESS_MIN 					(0x3142)
#define HOST_COMMAND_ISP_CTRL_CONTRAST 							(0x3144)
#define HOST_COMMAND_ISP_CTRL_CONTRAST_MAX 						(0x3146)
#define HOST_COMMAND_ISP_CTRL_CONTRAST_MIN 						(0x3148)
#define HOST_COMMAND_ISP_CTRL_SATURATION 						(0x314A)
#define HOST_COMMAND_ISP_CTRL_SATURATION_MAX 					(0x314C)
#define HOST_COMMAND_ISP_CTRL_SATURATION_MIN 					(0x314E)
#define HOST_COMMAND_ISP_CTRL_GAMMA 							(0x3150)
#define HOST_COMMAND_ISP_CTRL_GAMMA_MAX 						(0x3152)
#define HOST_COMMAND_ISP_CTRL_GAMMA_MIN 						(0x3154)
#define HOST_COMMAND_ISP_CTRL_DENOISE 							(0x3156)
#define HOST_COMMAND_ISP_CTRL_DENOISE_MAX 						(0x3158)
#define HOST_COMMAND_ISP_CTRL_DENOISE_MIN 						(0x315A)
#define HOST_COMMAND_ISP_CTRL_SHARPEN 							(0x315C)
#define HOST_COMMAND_ISP_CTRL_SHARPEN_MAX 						(0x315E)
#define HOST_COMMAND_ISP_CTRL_SHARPEN_MIN 						(0x3160)
#define HOST_COMMAND_ISP_CTRL_FLIP 								(0x3162)
#define HOST_COMMAND_ISP_CTRL_EFFECT 							(0x3164)
#define HOST_COMMAND_ISP_CTRL_ZOOM_TYPE 						(0x3166)
#define HOST_COMMAND_ISP_CTRL_ZOOM_TIMES 						(0x3168)
#define HOST_COMMAND_ISP_CTRL_ZOOM_TIMES_MAX 					(0x316A)
#define HOST_COMMAND_ISP_CTRL_ZOOM_TIMES_MIN 					(0x316C)
#define HOST_COMMAND_ISP_CTRL_CT_X 								(0x316E)
#define HOST_COMMAND_ISP_CTRL_CT_Y 								(0x3170)
#define HOST_COMMAND_ISP_CTRL_CT_MAX 							(0x3172)
#define HOST_COMMAND_ISP_CTRL_CT_MIN 							(0x3174)
#define HOST_COMMAND_ISP_CTRL_SYSTEM_START 						(0x3176)
#define HOST_COMMAND_ISP_CTRL_ISP_RESET 						(0x3178)
#define HOST_COMMAND_ISP_CTRL_TRIGGER_MODE 						(0x317A)
#define HOST_COMMAND_ISP_CTRL_FLICK_CTRL					 	(0x317C)
#define HOST_COMMAND_ISP_CTRL_MIPI_FREQ 						(0x317E)

/* Define host command register of ISP bootdata page */
#define HOST_COMMAND_ISP_BOOTDATA_1                             (0x4000)
#define HOST_COMMAND_ISP_BOOTDATA_2                             (0x4002)
#define HOST_COMMAND_ISP_BOOTDATA_3                             (0x4004)
#define HOST_COMMAND_ISP_BOOTDATA_4                             (0x4006)
#define HOST_COMMAND_ISP_BOOTDATA_5                             (0x4008)
#define HOST_COMMAND_ISP_BOOTDATA_6                             (0x400A)
#define HOST_COMMAND_ISP_BOOTDATA_7                             (0x400C)
#define HOST_COMMAND_ISP_BOOTDATA_8                             (0x400E)
#define HOST_COMMAND_ISP_BOOTDATA_9                             (0x4010)
#define HOST_COMMAND_ISP_BOOTDATA_10                            (0x4012)
#define HOST_COMMAND_ISP_BOOTDATA_11                            (0x4014)
#define HOST_COMMAND_ISP_BOOTDATA_12                            (0x4016)
#define HOST_COMMAND_ISP_BOOTDATA_13                            (0x4018)
#define HOST_COMMAND_ISP_BOOTDATA_14                            (0x401A)
#define HOST_COMMAND_ISP_BOOTDATA_15                            (0x401C)
#define HOST_COMMAND_ISP_BOOTDATA_16                            (0x401E)
#define HOST_COMMAND_ISP_BOOTDATA_17                            (0x4020)
#define HOST_COMMAND_ISP_BOOTDATA_18                            (0x4022)
#define HOST_COMMAND_ISP_BOOTDATA_19                            (0x4024)
#define HOST_COMMAND_ISP_BOOTDATA_20                            (0x4026)
#define HOST_COMMAND_ISP_BOOTDATA_21                            (0x4028)
#define HOST_COMMAND_ISP_BOOTDATA_22                            (0x402A)
#define HOST_COMMAND_ISP_BOOTDATA_23                            (0x402C)
#define HOST_COMMAND_ISP_BOOTDATA_24                            (0x402E)
#define HOST_COMMAND_ISP_BOOTDATA_25                            (0x4030)
#define HOST_COMMAND_ISP_BOOTDATA_26                            (0x4032)
#define HOST_COMMAND_ISP_BOOTDATA_27                            (0x4034)
#define HOST_COMMAND_ISP_BOOTDATA_28                            (0x4036)
#define HOST_COMMAND_ISP_BOOTDATA_29                            (0x4038)
#define HOST_COMMAND_ISP_BOOTDATA_30                            (0x403A)
#define HOST_COMMAND_ISP_BOOTDATA_31                            (0x403C)
#define HOST_COMMAND_ISP_BOOTDATA_32                            (0x403E)
#define HOST_COMMAND_ISP_BOOTDATA_33                            (0x4040)
#define HOST_COMMAND_ISP_BOOTDATA_34                            (0x4042)
#define HOST_COMMAND_ISP_BOOTDATA_35                            (0x4044)
#define HOST_COMMAND_ISP_BOOTDATA_36                            (0x4046)
#define HOST_COMMAND_ISP_BOOTDATA_37                            (0x4048)
#define HOST_COMMAND_ISP_BOOTDATA_38                            (0x404A)
#define HOST_COMMAND_ISP_BOOTDATA_39                            (0x404C)
#define HOST_COMMAND_ISP_BOOTDATA_40                            (0x404E)
#define HOST_COMMAND_ISP_BOOTDATA_41                            (0x4050)
#define HOST_COMMAND_ISP_BOOTDATA_42                            (0x4052)
#define HOST_COMMAND_ISP_BOOTDATA_43                            (0x4054)
#define HOST_COMMAND_ISP_BOOTDATA_44                            (0x4056)
#define HOST_COMMAND_ISP_BOOTDATA_45                            (0x4058)
#define HOST_COMMAND_ISP_BOOTDATA_46                            (0x405A)
#define HOST_COMMAND_ISP_BOOTDATA_47                            (0x405C)
#define HOST_COMMAND_ISP_BOOTDATA_48                            (0x405E)
#define HOST_COMMAND_ISP_BOOTDATA_49                            (0x4060)
#define HOST_COMMAND_ISP_BOOTDATA_50                            (0x4062)
#define HOST_COMMAND_ISP_BOOTDATA_51                            (0x4064)
#define HOST_COMMAND_ISP_BOOTDATA_52                            (0x4066)
#define HOST_COMMAND_ISP_BOOTDATA_53                            (0x4068)
#define HOST_COMMAND_ISP_BOOTDATA_54                            (0x406A)
#define HOST_COMMAND_ISP_BOOTDATA_55                            (0x406C)
#define HOST_COMMAND_ISP_BOOTDATA_56                            (0x406E)
#define HOST_COMMAND_ISP_BOOTDATA_57                            (0x4070)
#define HOST_COMMAND_ISP_BOOTDATA_58                            (0x4072)
#define HOST_COMMAND_ISP_BOOTDATA_59                            (0x4074)
#define HOST_COMMAND_ISP_BOOTDATA_60                            (0x4076)
#define HOST_COMMAND_ISP_BOOTDATA_61                            (0x4078)
#define HOST_COMMAND_ISP_BOOTDATA_62                            (0x407A)
#define HOST_COMMAND_ISP_BOOTDATA_63                            (0x407C)

/* Define special method for controlling ISP with I2C */
#define HOST_COMMAND_ISP_CTRL_I2C_ADDR                          (0xF000)
#define HOST_COMMAND_ISP_CTRL_I2C_DATA                          (0xF002)

#define TEVS_TRIGGER_CTRL                   	HOST_COMMAND_ISP_CTRL_TRIGGER_MODE

#define TEVS_BRIGHTNESS 						HOST_COMMAND_ISP_CTRL_BRIGHTNESS
#define TEVS_BRIGHTNESS_MAX 					HOST_COMMAND_ISP_CTRL_BRIGHTNESS_MAX
#define TEVS_BRIGHTNESS_MIN 					HOST_COMMAND_ISP_CTRL_BRIGHTNESS_MIN
#define TEVS_BRIGHTNESS_MASK 					(0xFFFF)
#define TEVS_CONTRAST 							HOST_COMMAND_ISP_CTRL_CONTRAST
#define TEVS_CONTRAST_MAX 						HOST_COMMAND_ISP_CTRL_CONTRAST_MAX
#define TEVS_CONTRAST_MIN 						HOST_COMMAND_ISP_CTRL_CONTRAST_MIN
#define TEVS_CONTRAST_MASK 						(0xFFFF)
#define TEVS_SATURATION 						HOST_COMMAND_ISP_CTRL_SATURATION
#define TEVS_SATURATION_MAX 					HOST_COMMAND_ISP_CTRL_SATURATION_MAX
#define TEVS_SATURATION_MIN 					HOST_COMMAND_ISP_CTRL_SATURATION_MIN
#define TEVS_SATURATION_MASK 					(0xFFFF)
#define TEVS_AWB_CTRL_MODE 						HOST_COMMAND_ISP_CTRL_AWB_MODE
#define TEVS_AWB_CTRL_MODE_MASK 				(0x00FF)
#define TEVS_AWB_CTRL_MODE_MANUAL_TEMP 			(7U << 0)
#define TEVS_AWB_CTRL_MODE_AUTO 				(15U << 0)
#define TEVS_AWB_CTRL_MODE_MANUAL_TEMP_IDX 		(0U << 0)
#define TEVS_AWB_CTRL_MODE_AUTO_IDX 			(1U << 0)
#define TEVS_GAMMA 								HOST_COMMAND_ISP_CTRL_GAMMA
#define TEVS_GAMMA_MAX 							HOST_COMMAND_ISP_CTRL_GAMMA_MAX
#define TEVS_GAMMA_MIN 							HOST_COMMAND_ISP_CTRL_GAMMA_MIN
#define TEVS_GAMMA_MASK 						(0xFFFF)
#define TEVS_MAX_FPS							HOST_COMMAND_ISP_CTRL_PREVIEW_MAX_FPS
#define TEVS_MAX_FPS_MASK 						(0x00FF)
#define TEVS_AE_AUTO_EXP_TIME_UPPER				HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_UPPER_MSB
#define TEVS_AE_AUTO_EXP_TIME_MAX				HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_MAX_MSB
#define TEVS_AE_AUTO_EXP_TIME_MASK				(0xFFFFFFFF)
#define TEVS_AE_MANUAL_EXP_TIME 				HOST_COMMAND_ISP_CTRL_EXP_TIME_MSB
#define TEVS_AE_MANUAL_EXP_TIME_MAX 			HOST_COMMAND_ISP_CTRL_EXP_TIME_MAX_MSB
#define TEVS_AE_MANUAL_EXP_TIME_MIN 			HOST_COMMAND_ISP_CTRL_EXP_TIME_MIN_MSB
#define TEVS_AE_MANUAL_EXP_TIME_MASK 			(0xFFFFFFFF)
#define TEVS_AE_MANUAL_GAIN 					HOST_COMMAND_ISP_CTRL_EXP_GAIN
#define TEVS_AE_MANUAL_GAIN_MAX 				HOST_COMMAND_ISP_CTRL_EXP_GAIN_MAX
#define TEVS_AE_MANUAL_GAIN_MIN 				HOST_COMMAND_ISP_CTRL_EXP_GAIN_MIN
#define TEVS_AE_MANUAL_GAIN_MASK 				(0x00FF)
#define TEVS_ORIENTATION 						HOST_COMMAND_ISP_CTRL_FLIP
#define TEVS_ORIENTATION_HFLIP 					(1U << 0)
#define TEVS_ORIENTATION_VFLIP 					(1U << 1)
#define TEVS_FLICK_CTRL    						HOST_COMMAND_ISP_CTRL_FLICK_CTRL
#define TEVS_FLICK_CTRL_MASK					(0xFFFF) // TEVS_REG_16BIT(0x5440)
#define TEVS_FLICK_CTRL_FREQ(n)					((n) << 8)
#define TEVS_FLICK_CTRL_ETC_IHDR_UP				BIT(6)
#define TEVS_FLICK_CTRL_ETC_DIS					BIT(5)
#define TEVS_FLICK_CTRL_FRC_OVERRIDE_MAX_ET		BIT(4)
#define TEVS_FLICK_CTRL_FRC_OVERRIDE_UPPER_ET	BIT(3)
#define TEVS_FLICK_CTRL_FRC_EN					BIT(2)
#define TEVS_FLICK_CTRL_MODE_MASK				(3U << 0)
#define TEVS_FLICK_CTRL_MODE_DISABLED			(0U << 0)
#define TEVS_FLICK_CTRL_MODE_MANUAL				(1U << 0)
#define TEVS_FLICK_CTRL_MODE_AUTO				(2U << 0)
#define TEVS_FLICK_CTRL_FREQ_MASK			    (0xFF00)
#define TEVS_FLICK_CTRL_MODE_50HZ             	(TEVS_FLICK_CTRL_FREQ(50) | TEVS_FLICK_CTRL_MODE_MANUAL)
#define TEVS_FLICK_CTRL_MODE_60HZ             	(TEVS_FLICK_CTRL_FREQ(60) | TEVS_FLICK_CTRL_MODE_MANUAL)
#define TEVS_FLICK_MODE_DISABLED_IDX			(0U << 0)
#define TEVS_FLICK_MODE_ENABLED_IDX				(3U << 0)
#define TEVS_AWB_MANUAL_TEMP 					HOST_COMMAND_ISP_CTRL_AWB_TEMP
#define TEVS_AWB_MANUAL_TEMP_MAX 				HOST_COMMAND_ISP_CTRL_AWB_TEMP_MAX
#define TEVS_AWB_MANUAL_TEMP_MIN 				HOST_COMMAND_ISP_CTRL_AWB_TEMP_MIN
#define TEVS_AWB_MANUAL_TEMP_MASK 				(0xFFFF)
#define TEVS_SHARPEN 							HOST_COMMAND_ISP_CTRL_SHARPEN
#define TEVS_SHARPEN_MAX 						HOST_COMMAND_ISP_CTRL_SHARPEN_MAX
#define TEVS_SHARPEN_MIN 						HOST_COMMAND_ISP_CTRL_SHARPEN_MIN
#define TEVS_SHARPEN_MASK 						(0xFFFF)
#define TEVS_BACKLIGHT_COMPENSATION 			HOST_COMMAND_ISP_CTRL_BACKLIGHT_COMPENSATION
#define TEVS_BACKLIGHT_COMPENSATION_MAX 		HOST_COMMAND_ISP_CTRL_BACKLIGHT_COMPENSATION_MAX
#define TEVS_BACKLIGHT_COMPENSATION_MIN 		HOST_COMMAND_ISP_CTRL_BACKLIGHT_COMPENSATION_MIN
#define TEVS_BACKLIGHT_COMPENSATION_MASK 		(0xFFFF)
#define TEVS_DZ_TGT_FCT 						HOST_COMMAND_ISP_CTRL_ZOOM_TIMES
#define TEVS_DZ_TGT_FCT_MAX 					HOST_COMMAND_ISP_CTRL_ZOOM_TIMES_MAX
#define TEVS_DZ_TGT_FCT_MIN 					HOST_COMMAND_ISP_CTRL_ZOOM_TIMES_MIN
#define TEVS_DZ_TGT_FCT_MASK 					(0xFFFF)
#define TEVS_SFX_MODE 							HOST_COMMAND_ISP_CTRL_EFFECT
#define TEVS_SFX_MODE_SFX_MASK 					(0x00FF)
#define TEVS_SFX_MODE_SFX_NORMAL 				(0U << 0)
#define TEVS_SFX_MODE_SFX_BW 					(3U << 0)
#define TEVS_SFX_MODE_SFX_GRAYSCALE 			(6U << 0)
#define TEVS_SFX_MODE_SFX_NEGATIVE 				(7U << 0)
#define TEVS_SFX_MODE_SFX_SKETCH 				(15U << 0)
#define TEVS_SFX_MODE_SFX_NORMAL_IDX 			(0U << 0)
#define TEVS_SFX_MODE_SFX_BW_IDX 				(1U << 0)
#define TEVS_SFX_MODE_SFX_GRAYSCALE_IDX 		(2U << 0)
#define TEVS_SFX_MODE_SFX_NEGATIVE_IDX 			(3U << 0)
#define TEVS_SFX_MODE_SFX_SKETCH_IDX 			(4U << 0)
#define TEVS_AE_CTRL_MODE 						HOST_COMMAND_ISP_CTRL_AE_MODE
#define TEVS_AE_CTRL_MODE_MASK 					(0x00FF)
#define TEVS_AE_CTRL_MANUAL_EXP_TIME_GAIN 		(0U << 0)
#define TEVS_AE_CTRL_FULL_AUTO 					(12U << 0)
#define TEVS_AE_CTRL_MANUAL_EXP_TIME_GAIN_IDX 	(0U << 0)
#define TEVS_AE_CTRL_FULL_AUTO_IDX 				(1U << 0)
#define TEVS_DZ_CT_X 							HOST_COMMAND_ISP_CTRL_CT_X
#define TEVS_DZ_CT_Y 							HOST_COMMAND_ISP_CTRL_CT_Y
#define TEVS_DZ_CT_MASK 						(0xFFFF)
#define TEVS_DZ_CT_MAX 							HOST_COMMAND_ISP_CTRL_CT_MAX
#define TEVS_DZ_CT_MIN 							HOST_COMMAND_ISP_CTRL_CT_MIN

#define V4L2_CID_USER_TEVS_BASE				(V4L2_CID_USER_BASE + 0x2000)
#define V4L2_CID_TEVS_BSL_MODE				(V4L2_CID_USER_TEVS_BASE + 0)
#define V4L2_CID_TEVS_MAX_FPS				(V4L2_CID_USER_TEVS_BASE + 1)
#define V4L2_CID_TEVS_AE_EXP_TIME_UPPER		(V4L2_CID_USER_TEVS_BASE + 2)
#define V4L2_CID_TEVS_AE_EXP_TIME_MAX		(V4L2_CID_USER_TEVS_BASE + 3)
#define TEVS_TRIGGER_CTRL_MODE_MASK 		(0x0001)
#define TEVS_BSL_MODE_NORMAL_IDX 		    (0U << 0)
#define TEVS_BSL_MODE_FLASH_IDX 			(1U << 0)

#define DEFAULT_HEADER_VERSION 3
#define TEVS_BOOT_TIME						(250)
#define TOTAL_MICROSEC_PERSEC               (1000000)

struct header_info {
	u8 header_version;
	u16 content_offset;
	u16 sensor_type;
	u8 sensor_fuseid[16];
	u8 product_name[64];
	u8 lens_id[16];
	u16 fix_checksum;
	u8 tn_fw_version[2];
	u16 vendor_fw_version;
	u16 custom_number;
	u8 build_year;
	u8 build_month;
	u8 build_day;
	u8 build_hour;
	u8 build_minute;
	u8 build_second;
	u16 mipi_datarate;
	u32 content_len;
	u16 content_checksum;
	u16 total_checksum;
} __attribute__((packed));

struct tevs {
	struct device *dev;
	struct v4l2_subdev v4l2_subdev;
	struct media_pad pad;
	struct v4l2_mbus_framefmt fmt;
	struct regmap *regmap;
	struct header_info *header_info;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *host_pwdn_gpio;
	struct gpio_desc *standby_gpio;

	int data_lanes;
	int continuous_clock;
	int data_frequency;
	u8 selected_mode;
	u8 selected_sensor;
	bool supports_over_4k_res;
	bool hw_reset_mode;
	bool trigger_mode;
	bool fixed_fps;
	char *sensor_name;

	struct mutex lock; /* Protects formats */
	/* V4L2 Controls */
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *link_freq_ctrl;
	struct v4l2_ctrl *max_fps_ctrl;
};

static const struct regmap_config tevs_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

int tevs_i2c_read(struct tevs *tevs, u16 reg, u8 *val, u16 size)
{
	int ret;

	ret = regmap_bulk_read(tevs->regmap, reg, val, size);
	if (ret < 0) {
		dev_err(tevs->dev,
			"Failed to read from register: ret=%d, reg=0x%x\n", ret,
			reg);
		return ret;
	}

	return 0;
}

int tevs_i2c_read_16b(struct tevs *tevs, u16 reg, u16 *value)
{
	u8 v[2] = { 0 };
	int ret;

	ret = tevs_i2c_read(tevs, reg, v, 2);
	if (ret < 0) {
		dev_err(tevs->dev,
			"Failed to read from register: ret=%d, reg=0x%x\n", ret,
			reg);
		return ret;
	}

	*value = (v[0] << 8) | v[1];
	dev_dbg(tevs->dev, "%s() read reg 0x%x, value 0x%x\n", __func__, reg,
		*value);

	return 0;
}

int tevs_i2c_write(struct tevs *tevs, u16 reg, u8 *val, u16 size)
{
	int ret;

	ret = regmap_bulk_write(tevs->regmap, reg, val, size);
	if (ret < 0) {
		dev_err(tevs->dev,
			"Failed to write to register: ret=%d reg=0x%x\n", ret,
			reg);
		return ret;
	}

	return 0;
}

int tevs_i2c_write_16b(struct tevs *tevs, u16 reg, u16 val)
{
	int ret;
	u8 data[2];
	data[0] = val >> 8;
	data[1] = val & 0xFF;

	ret = regmap_bulk_write(tevs->regmap, reg, data, 2);
	if (ret < 0) {
		dev_err(tevs->dev,
			"Failed to write to register: ret=%d reg=0x%x\n", ret,
			reg);
		return ret;
	}
	dev_dbg(tevs->dev, "%s() write reg 0x%x, value 0x%x\n", __func__, reg,
		val);

	return 0;
}

int tevs_enable_trigger_mode(struct tevs *tevs, int enable)
{
	int ret = 0;
	int count = 0;
	u16 val, trigger_data;
	dev_dbg(tevs->dev, "%s(): enable:%d\n", __func__, enable);
	trigger_data = (0x300 | ((enable > 0) ? 0x82 : 0x80));

	if ((ret = tevs_i2c_write_16b(tevs, TEVS_TRIGGER_CTRL, trigger_data)) <
	    0)
		return ret;

	do {
		if ((ret = tevs_i2c_read_16b(tevs, TEVS_TRIGGER_CTRL, &val)) <
		    0)
			return ret;
		if ((val & 0x300) == 0)
			break;

	} while (count++ < 10);

	usleep_range(90000, 100000);

	return ret;
}

int tevs_check_version(struct tevs *tevs)
{
	struct device *dev = tevs->dev;
	u8 version[4] = { 0 };
	int ret = 0;

	ret = tevs_i2c_read(tevs, HOST_COMMAND_TEVS_INFO_VERSION_MSB,
			    &version[0], 4);
	if (ret < 0) {
		dev_err(dev, "can't check version\n");
		return ret;
	}
	dev_info(dev, "Version:%d.%d.%d.%d\n", version[0], version[1],
		 version[2], version[3]);

	return 0;
}

int tevs_load_header_info(struct tevs *tevs)
{
	struct device *dev = tevs->dev;
	struct header_info *header = tevs->header_info;
	u8 header_ver;
	int ret = 0;

	ret = tevs_i2c_read(tevs, HOST_COMMAND_ISP_BOOTDATA_1, &header_ver, 1);

	if (ret < 0) {
		dev_err(dev, "can't recognize header info\n");
		return ret;
	}

	if (header_ver == DEFAULT_HEADER_VERSION) {
		tevs_i2c_read(tevs, HOST_COMMAND_ISP_BOOTDATA_1, (u8 *)header,
			      sizeof(struct header_info));

		dev_info(dev, "Product:%s, HeaderVer:%d, MIPI_Rate:%d\n",
			 header->product_name, header->header_version,
			 header->mipi_datarate);

		dev_dbg(dev, "content checksum: %x, content length: %d\n",
			header->content_checksum, header->content_len);

		return 0;
	} else {
		dev_err(dev, "can't recognize header version number '0x%X'\n",
			header_ver);
		return -EINVAL;
	}
}

static int tevs_standby(struct tevs *tevs, int enable)
{
	u16 v = 0xFFFF;
	int timeout = 0;
	dev_dbg(tevs->dev, "%s():enable=%d\n", __func__, enable);

	if (enable == 1) {
		tevs_i2c_write_16b(tevs, HOST_COMMAND_ISP_CTRL_SYSTEM_START,
				   0x0000);
		while (timeout < 100) {
			tevs_i2c_read_16b(
				tevs, HOST_COMMAND_ISP_CTRL_SYSTEM_START, &v);
			if ((v & 0xFF00) == 0x0000)
				break;
			if (++timeout >= 100) {
				dev_err(tevs->dev, "timeout: line[%d]v=%x\n",
					__LINE__, v);
				return -EINVAL;
			}
			usleep_range(9000, 10000);
		}
		dev_dbg(tevs->dev, "sensor standby\n");
	} else {
		tevs_i2c_write_16b(tevs, HOST_COMMAND_ISP_CTRL_SYSTEM_START,
				   0x0001);
		while (timeout < 100) {
			tevs_i2c_read_16b(
				tevs, HOST_COMMAND_ISP_CTRL_SYSTEM_START, &v);
			if ((v & 0xFF00) == 0x0100)
				break;
			if (++timeout >= 100) {
				dev_err(tevs->dev, "timeout: line[%d]v=%x\n",
					__LINE__, v);
				return -EINVAL;
			}
			usleep_range(9000, 10000);
		}
		dev_dbg(tevs->dev, "sensor wakeup\n");
	}

	return 0;
}

static int tevs_check_boot_state(struct tevs *tevs)
{
	u16 boot_state;
	u8 timeout = 0;
	int ret = 0;

	while (timeout < 20) {
		tevs_i2c_read_16b(tevs, HOST_COMMAND_TEVS_BOOT_STATE,
				  &boot_state);
		if (boot_state == 0x08)
			break;
		dev_dbg(tevs->dev, "tevs bootup state: %d\n", boot_state);
		if (++timeout >= 20) {
			dev_err(tevs->dev,
				"tevs bootup timeout: state: 0x%02X\n",
				boot_state);
			ret = -EINVAL;
		}
		msleep(20);
	}

	return ret;
}

static int tevs_power_on(struct tevs *tevs)
{
	int ret = 0;
	dev_dbg(tevs->dev, "%s()\n", __func__);

	gpiod_set_value_cansleep(tevs->host_pwdn_gpio, 1);
	gpiod_set_value_cansleep(tevs->reset_gpio, 1);
	msleep(TEVS_BOOT_TIME);

	ret = tevs_check_boot_state(tevs);
	if (ret != 0) {
		goto error;
	}

	if (tevs->trigger_mode) {
		ret = tevs_enable_trigger_mode(tevs, 1);
		if (ret != 0) {
			dev_err(tevs->dev, "set trigger mode failed\n");
			return ret;
		}
	}

	return ret;

error:
	gpiod_set_value_cansleep(tevs->reset_gpio, 0);
	gpiod_set_value_cansleep(tevs->host_pwdn_gpio, 0);
	return ret;
}

static int tevs_power_off(struct tevs *tevs)
{
	dev_dbg(tevs->dev, "%s()\n", __func__);

	if (tevs->hw_reset_mode) {
		gpiod_set_value_cansleep(tevs->reset_gpio, 0);
		gpiod_set_value_cansleep(tevs->host_pwdn_gpio, 0);
	}

	return 0;
}

static int tevs_power(struct v4l2_subdev *sub_dev, int on)
{
	struct tevs *tevs = container_of(sub_dev, struct tevs, v4l2_subdev);
	dev_dbg(sub_dev->dev, "%s() [%d]\n", __func__, on);
	if (on)
		return tevs_power_on(tevs);
	else
		return tevs_power_off(tevs);
}

static int tevs_get_frame_interval(struct v4l2_subdev *sub_dev,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct tevs *tevs = container_of(sub_dev, struct tevs, v4l2_subdev);
	u32 max_fps;
	dev_dbg(sub_dev->dev, "%s()\n", __func__);

	if (fi->pad != 0)
		return -EINVAL;

	max_fps = tevs_sensor_table[tevs->selected_sensor]
			  .res_list[tevs->selected_mode]
			  .framerates;

	fi->interval.numerator = 1;
	fi->interval.denominator = max_fps;

	return 0;
}

static int tevs_set_frame_interval(struct v4l2_subdev *sub_dev,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct tevs *tevs = container_of(sub_dev, struct tevs, v4l2_subdev);
	u32 max_fps;
	dev_dbg(sub_dev->dev, "%s()\n", __func__);

	if (fi->pad != 0)
		return -EINVAL;

	max_fps = tevs_sensor_table[tevs->selected_sensor]
			  .res_list[tevs->selected_mode]
			  .framerates;

	fi->interval.numerator = 1;
	fi->interval.denominator = max_fps;

	return 0;
}

static int tevs_set_stream(struct v4l2_subdev *sub_dev, int enable)
{
	struct tevs *tevs = container_of(sub_dev, struct tevs, v4l2_subdev);
	int exp_time;
	int ret = 0;

	dev_dbg(sub_dev->dev, "%s() enable [%x]\n", __func__, enable);

	if (tevs->selected_mode >=
	    tevs_sensor_table[tevs->selected_sensor].res_list_size)
		return -EINVAL;

	if (enable == 0) {
		if (!(tevs->hw_reset_mode | tevs->trigger_mode))
			ret = tevs_standby(tevs, 1);
	} else {
		if (!(tevs->hw_reset_mode | tevs->trigger_mode))
			ret = tevs_standby(tevs, 0);

		if (ret == 0) {
			int fps = tevs_sensor_table[tevs->selected_sensor]
					  .res_list[tevs->selected_mode]
					  .framerates;
			dev_dbg(sub_dev->dev, "%s() width=%d, height=%d\n",
				__func__,
				tevs_sensor_table[tevs->selected_sensor]
					.res_list[tevs->selected_mode]
					.width,
				tevs_sensor_table[tevs->selected_sensor]
					.res_list[tevs->selected_mode]
					.height);
			tevs_i2c_write_16b(tevs,
					   HOST_COMMAND_ISP_CTRL_PREVIEW_FORMAT,
					   0x50);
			tevs_i2c_write_16b(
				tevs, HOST_COMMAND_ISP_CTRL_PREVIEW_HINF_CTRL,
				0x10 | (tevs->continuous_clock << 5) |
					(tevs->data_lanes));
			tevs_i2c_write_16b(
				tevs, HOST_COMMAND_ISP_CTRL_PREVIEW_SENSOR_MODE,
				tevs_sensor_table[tevs->selected_sensor]
					.res_list[tevs->selected_mode]
					.mode);
			tevs_i2c_write_16b(
				tevs, HOST_COMMAND_ISP_CTRL_PREVIEW_WIDTH,
				tevs_sensor_table[tevs->selected_sensor]
					.res_list[tevs->selected_mode]
					.width);
			tevs_i2c_write_16b(
				tevs, HOST_COMMAND_ISP_CTRL_PREVIEW_HEIGHT,
				tevs_sensor_table[tevs->selected_sensor]
					.res_list[tevs->selected_mode]
					.height);
			tevs_i2c_write_16b(
				tevs, HOST_COMMAND_ISP_CTRL_PREVIEW_MAX_FPS,
				fps);
			if (tevs->max_fps_ctrl)
				tevs->max_fps_ctrl->cur.val = fps;

			if (tevs->fixed_fps) {
				exp_time = TOTAL_MICROSEC_PERSEC / fps;
				dev_dbg(sub_dev->dev,
					"%s():set fixed exp time: %d us(fps:%d)\n",
					__func__, exp_time, fps);
				tevs_i2c_write_16b(
					tevs,
					HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_UPPER_MSB,
					(exp_time >> 16));
				tevs_i2c_write_16b(
					tevs,
					HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_UPPER_LSB,
					exp_time & 0xFFFF);
				tevs_i2c_write_16b(
					tevs,
					HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_MAX_MSB,
					(exp_time >> 16));
				tevs_i2c_write_16b(
					tevs,
					HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_MAX_LSB,
					exp_time & 0xFFFF);
			}
		}
	}

	return ret;
}

static int tevs_enum_mbus_code(struct v4l2_subdev *sub_dev,
			       struct v4l2_subdev_state *sd_state,
			       struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index > 0)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_UYVY8_2X8;

	return 0;
}

static int tevs_get_fmt(struct v4l2_subdev *sub_dev,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *fmt;
	struct v4l2_mbus_framefmt *mbus_fmt = &format->format;
	struct tevs *tevs = container_of(sub_dev, struct tevs, v4l2_subdev);

	if (format->pad != 0)
		return -EINVAL;

	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		fmt = v4l2_subdev_get_try_format(sub_dev, sd_state,
						 format->pad);
	else
		fmt = &tevs->fmt;

	memmove(mbus_fmt, fmt, sizeof(struct v4l2_mbus_framefmt));

	return 0;
}

static int tevs_set_fmt(struct v4l2_subdev *sub_dev,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *fmt;
	struct v4l2_mbus_framefmt *mbus_fmt = &format->format;
	struct tevs *tevs = container_of(sub_dev, struct tevs, v4l2_subdev);
	int i;

	if (format->pad != 0)
		return -EINVAL;

	for (i = 0; i < tevs_sensor_table[tevs->selected_sensor].res_list_size;
	     i++) {
		if (mbus_fmt->width == tevs_sensor_table[tevs->selected_sensor]
					       .res_list[i]
					       .width &&
		    mbus_fmt->height == tevs_sensor_table[tevs->selected_sensor]
						.res_list[i]
						.height)
			break;
	}

	if (i >= tevs_sensor_table[tevs->selected_sensor].res_list_size) {
		return -EINVAL;
	}
	tevs->selected_mode = i;
	dev_dbg(sub_dev->dev, "%s() selected mode index [%d]\n", __func__,
		tevs->selected_mode);

	mbus_fmt->width =
		tevs_sensor_table[tevs->selected_sensor].res_list[i].width;
	mbus_fmt->height =
		tevs_sensor_table[tevs->selected_sensor].res_list[i].height;
	mbus_fmt->code = MEDIA_BUS_FMT_UYVY8_2X8;
	mbus_fmt->colorspace = V4L2_COLORSPACE_SRGB;
	mbus_fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(mbus_fmt->colorspace);
	mbus_fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	mbus_fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(mbus_fmt->colorspace);
	memset(mbus_fmt->reserved, 0, sizeof(mbus_fmt->reserved));

	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		fmt = v4l2_subdev_get_try_format(sub_dev, sd_state, 0);
	else
		fmt = &tevs->fmt;

	memmove(fmt, mbus_fmt, sizeof(struct v4l2_mbus_framefmt));

	return 0;
}

static int tevs_enum_frame_size(struct v4l2_subdev *sub_dev,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_size_enum *fse)
{
	struct tevs *tevs = container_of(sub_dev, struct tevs, v4l2_subdev);

	dev_dbg(sub_dev->dev, "%s()\n", __func__);
	if ((fse->pad != 0) ||
	    (fse->index >=
	     tevs_sensor_table[tevs->selected_sensor].res_list_size))
		return -EINVAL;

	if (!tevs->supports_over_4k_res &&
	    tevs_sensor_table[tevs->selected_sensor].res_list[fse->index].width >
		    4096)
		return -EINVAL;

	fse->min_width = fse->max_width =
		tevs_sensor_table[tevs->selected_sensor]
			.res_list[fse->index]
			.width;
	fse->min_height = fse->max_height =
		tevs_sensor_table[tevs->selected_sensor]
			.res_list[fse->index]
			.height;

	return 0;
}

static int tevs_enum_frame_interval(struct v4l2_subdev *sub_dev,
				    struct v4l2_subdev_state *sd_state,
				    struct v4l2_subdev_frame_interval_enum *fie)
{
	struct tevs *tevs = container_of(sub_dev, struct tevs, v4l2_subdev);
	int i;

	if ((fie->pad != 0) || (fie->index != 0))
		return -EINVAL;
	dev_dbg(sub_dev->dev, "%s()\n", __func__);

	fie->interval.numerator = 1;

	for (i = 0; i < tevs_sensor_table[tevs->selected_sensor].res_list_size;
	     i++) {
		if (fie->width == tevs_sensor_table[tevs->selected_sensor]
					  .res_list[i]
					  .width &&
		    fie->height == tevs_sensor_table[tevs->selected_sensor]
					   .res_list[i]
					   .height) {
			fie->interval.denominator =
				tevs_sensor_table[tevs->selected_sensor]
					.res_list[i]
					.framerates;
			break;
		}
	}

	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 Controls
 */

static const s64 tevs_link_freqs[] = {
	600000000,
};

static const char *const awb_mode_strings[] = {
	"Manual Temp Mode", // TEVS_AWB_CTRL_MODE_MANUAL_TEMP
	"Auto Mode", // TEVS_AWB_CTRL_MODE_AUTO
	NULL,
};

static const char *const flick_mode_strings[] = {
	"Disabled", "50 Hz", "60 Hz", "Auto", NULL,
};

static const char *const sfx_mode_strings[] = {
	"Normal Mode", // TEVS_SFX_MODE_SFX_NORMAL
	"Black and White Mode", // TEVS_SFX_MODE_SFX_BW
	"Grayscale Mode", // TEVS_SFX_MODE_SFX_GRAYSCALE
	"Negative Mode", // TEVS_SFX_MODE_SFX_NEGATIVE
	"Sketch Mode", // TEVS_SFX_MODE_SFX_SKETCH
	NULL,
};

static const char *const ae_mode_strings[] = {
	"Manual Mode", // TEVS_AE_CTRL_MANUAL_EXP_TIME_GAIN
	"Auto Mode", // TEVS_AE_CTRL_FULL_AUTO
	NULL,
};

static const char *const bsl_mode_strings[] = {
	"Normal Mode",
	"Bootstrap Mode",
	NULL,
};

static int tevs_set_brightness(struct tevs *tevs, s32 value)
{
	// Format is u3.12
	return tevs_i2c_write_16b(tevs, TEVS_BRIGHTNESS,
				  value & TEVS_BRIGHTNESS_MASK);
}

static int tevs_set_contrast(struct tevs *tevs, s32 value)
{
	// Format is u3.12
	return tevs_i2c_write_16b(tevs, TEVS_CONTRAST,
				  value & TEVS_CONTRAST_MASK);
}

static int tevs_set_saturation(struct tevs *tevs, s32 value)
{
	// Format is u3.12
	return tevs_i2c_write_16b(tevs, TEVS_SATURATION,
				  value & TEVS_SATURATION_MASK);
}

static int tevs_set_awb_mode(struct tevs *tevs, s32 mode)
{
	u16 val = mode & TEVS_AWB_CTRL_MODE_MASK;

	switch (val) {
	case 0:
		val = TEVS_AWB_CTRL_MODE_MANUAL_TEMP;
		break;
	case 1:
		val = TEVS_AWB_CTRL_MODE_AUTO;
		break;
	default:
		val = TEVS_AWB_CTRL_MODE_AUTO;
		break;
	}

	return tevs_i2c_write_16b(tevs, TEVS_AWB_CTRL_MODE, val);
}

static int tevs_set_gamma(struct tevs *tevs, s32 value)
{
	// Format is u3.12
	return tevs_i2c_write_16b(tevs, TEVS_GAMMA, value & TEVS_GAMMA_MASK);
}

static int tevs_set_max_fps(struct tevs *tevs, s32 value)
{
	return tevs_i2c_write_16b(tevs, TEVS_MAX_FPS,
				  value & TEVS_MAX_FPS_MASK);
}

static int tevs_set_ae_auto_exp_upper(struct tevs *tevs, s32 value)
{
	u8 val[4];
	__be32 temp;
	int ret;

	temp = cpu_to_be32(value);
	memcpy(val, &temp, 4);

	ret = tevs_i2c_write(tevs, TEVS_AE_AUTO_EXP_TIME_UPPER, val, 4);
	if (ret)
		return ret;

	return 0;
}

static int tevs_set_ae_auto_exp_max(struct tevs *tevs, s32 value)
{
	u8 val[4];
	__be32 temp;
	int ret;

	temp = cpu_to_be32(value);
	memcpy(val, &temp, 4);

	ret = tevs_i2c_write(tevs, TEVS_AE_AUTO_EXP_TIME_MAX, val, 4);
	if (ret)
		return ret;

	return 0;
}

static int tevs_set_exposure(struct tevs *tevs, s32 value)
{
	u8 val[4];
	__be32 temp;
	int ret;

	temp = cpu_to_be32(value);
	memcpy(val, &temp, 4);

	ret = tevs_i2c_write(tevs, TEVS_AE_MANUAL_EXP_TIME, val, 4);
	if (ret)
		return ret;

	return 0;
}

static int tevs_set_gain(struct tevs *tevs, s32 value)
{
	return tevs_i2c_write_16b(tevs, TEVS_AE_MANUAL_GAIN,
				  value & TEVS_AE_MANUAL_GAIN_MASK);
}

static int tevs_set_hflip(struct tevs *tevs, s32 flip)
{
	u16 val;
	int ret;

	ret = tevs_i2c_read_16b(tevs, TEVS_ORIENTATION, &val);
	if (ret)
		return ret;

	val &= ~TEVS_ORIENTATION_HFLIP;
	val |= flip ? TEVS_ORIENTATION_HFLIP : 0;

	return tevs_i2c_write_16b(tevs, TEVS_ORIENTATION, val);
}

static int tevs_set_vflip(struct tevs *tevs, s32 flip)
{
	u16 val;
	int ret;

	ret = tevs_i2c_read_16b(tevs, TEVS_ORIENTATION, &val);
	if (ret)
		return ret;

	val &= ~TEVS_ORIENTATION_VFLIP;
	val |= flip ? TEVS_ORIENTATION_VFLIP : 0;

	return tevs_i2c_write_16b(tevs, TEVS_ORIENTATION, val);
}

static int tevs_set_flick_mode(struct tevs *tevs, s32 mode)
{
	u16 val = 0;
	switch (mode) {
	case 0:
		val = TEVS_FLICK_CTRL_MODE_DISABLED;
		break;
	case 1:
		val = TEVS_FLICK_CTRL_MODE_50HZ;
		break;
	case 2:
		val = TEVS_FLICK_CTRL_MODE_60HZ;
		break;
	case 3:
		val = TEVS_FLICK_CTRL_MODE_AUTO |
		      TEVS_FLICK_CTRL_FRC_OVERRIDE_UPPER_ET |
		      TEVS_FLICK_CTRL_FRC_EN;
		break;
	default:
		val = TEVS_FLICK_CTRL_MODE_DISABLED;
		break;
	}

	return tevs_i2c_write_16b(tevs, TEVS_FLICK_CTRL, val);
}

static int tevs_set_awb_temp(struct tevs *tevs, s32 value)
{
	return tevs_i2c_write_16b(tevs, TEVS_AWB_MANUAL_TEMP,
				  value & TEVS_AWB_MANUAL_TEMP_MASK);
}

static int tevs_set_sharpen(struct tevs *tevs, s32 value)
{
	// Format is u3.12
	return tevs_i2c_write_16b(tevs, TEVS_SHARPEN,
				  value & TEVS_SHARPEN_MASK);
}

static int tevs_set_backlight_compensation(struct tevs *tevs, s32 value)
{
	// Format is u3.12
	return tevs_i2c_write_16b(tevs, TEVS_BACKLIGHT_COMPENSATION,
				  value & TEVS_BACKLIGHT_COMPENSATION_MASK);
}

static int tevs_set_zoom_target(struct tevs *tevs, s32 value)
{
	// Format u7.8
	return tevs_i2c_write_16b(tevs, TEVS_DZ_TGT_FCT,
				  value & TEVS_DZ_TGT_FCT_MASK);
}

static int tevs_set_special_effect(struct tevs *tevs, s32 mode)
{
	u16 val = mode & TEVS_SFX_MODE_SFX_MASK;

	switch (val) {
	case 0:
		val = TEVS_SFX_MODE_SFX_NORMAL;
		break;
	case 1:
		val = TEVS_SFX_MODE_SFX_BW;
		break;
	case 2:
		val = TEVS_SFX_MODE_SFX_GRAYSCALE;
		break;
	case 3:
		val = TEVS_SFX_MODE_SFX_NEGATIVE;
		break;
	case 4:
		val = TEVS_SFX_MODE_SFX_SKETCH;
		break;
	default:
		val = TEVS_SFX_MODE_SFX_NORMAL;
		break;
	}

	return tevs_i2c_write_16b(tevs, TEVS_SFX_MODE, val);
}

static int tevs_set_bsl_mode(struct tevs *tevs, s32 mode)
{
	u8 val;
	u8 bootcmd[6] = { 0x00, 0x12, 0x3A, 0x61, 0x44, 0xDE };
	u8 startup[6] = { 0x00, 0x40, 0xE2, 0x51, 0x21, 0x5B };
	dev_dbg(tevs->dev, "%s(): set bls mode: %d", __func__, mode);

	switch (mode) {
	case 0:
		tevs_i2c_write(tevs, 0x8001, startup, 6);
		tevs_i2c_read(tevs, 0x8001, &val, 1);

		msleep(TEVS_BOOT_TIME);

		if (tevs_check_boot_state(tevs) != 0) {
			dev_err(tevs->dev, "check tevs bootup status failed\n");
			return -EINVAL;
		}
		break;
	case 1:
		gpiod_set_value_cansleep(tevs->reset_gpio, 0);
		usleep_range(9000, 10000);
		gpiod_set_value_cansleep(tevs->standby_gpio, 1);
		msleep(100);
		gpiod_set_value_cansleep(tevs->reset_gpio, 1);
		usleep_range(9000, 10000);
		gpiod_set_value_cansleep(tevs->standby_gpio, 0);
		msleep(100);
		tevs_i2c_write(tevs, 0x8001, bootcmd, 6);
		tevs_i2c_read(tevs, 0x8001, &val, 1);
		break;
	default:
		dev_err(tevs->dev, "%s(): set err bls mode: %d", __func__,
			mode);
		break;
	}

	return 0;
}

static int tevs_set_ae_mode(struct tevs *tevs, s32 mode)
{
	u16 val = mode & TEVS_SFX_MODE_SFX_MASK;

	switch (val) {
	case 0:
		val = TEVS_AE_CTRL_MANUAL_EXP_TIME_GAIN;
		break;
	case 1:
		val = TEVS_AE_CTRL_FULL_AUTO;
		break;
	default:
		val = TEVS_AE_CTRL_FULL_AUTO;
		break;
	}

	return tevs_i2c_write_16b(tevs, TEVS_AE_CTRL_MODE, val);
}

static int tevs_set_pan_target(struct tevs *tevs, s32 value)
{
	// Format u7.8
	return tevs_i2c_write_16b(tevs, TEVS_DZ_CT_X, value & TEVS_DZ_CT_MASK);
}

static int tevs_set_tilt_target(struct tevs *tevs, s32 value)
{
	// Format u7.8
	return tevs_i2c_write_16b(tevs, TEVS_DZ_CT_Y, value & TEVS_DZ_CT_MASK);
}

static int tevs_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct tevs *tevs = container_of(ctrl->handler, struct tevs, ctrls);

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return tevs_set_brightness(tevs, ctrl->val);

	case V4L2_CID_CONTRAST:
		return tevs_set_contrast(tevs, ctrl->val);

	case V4L2_CID_SATURATION:
		return tevs_set_saturation(tevs, ctrl->val);

	case V4L2_CID_AUTO_WHITE_BALANCE:
		return tevs_set_awb_mode(tevs, ctrl->val);

	case V4L2_CID_GAMMA:
		return tevs_set_gamma(tevs, ctrl->val);

	case V4L2_CID_EXPOSURE:
		return tevs_set_exposure(tevs, ctrl->val);

	case V4L2_CID_GAIN:
		return tevs_set_gain(tevs, ctrl->val);

	case V4L2_CID_HFLIP:
		return tevs_set_hflip(tevs, ctrl->val);

	case V4L2_CID_VFLIP:
		return tevs_set_vflip(tevs, ctrl->val);

	case V4L2_CID_POWER_LINE_FREQUENCY:
		return tevs_set_flick_mode(tevs, ctrl->val);

	case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
		return tevs_set_awb_temp(tevs, ctrl->val);

	case V4L2_CID_SHARPNESS:
		return tevs_set_sharpen(tevs, ctrl->val);

	case V4L2_CID_BACKLIGHT_COMPENSATION:
		return tevs_set_backlight_compensation(tevs, ctrl->val);

	case V4L2_CID_COLORFX:
		return tevs_set_special_effect(tevs, ctrl->val);

	case V4L2_CID_EXPOSURE_AUTO:
		return tevs_set_ae_mode(tevs, ctrl->val);

	case V4L2_CID_PAN_ABSOLUTE:
		return tevs_set_pan_target(tevs, ctrl->val);

	case V4L2_CID_TILT_ABSOLUTE:
		return tevs_set_tilt_target(tevs, ctrl->val);

	case V4L2_CID_ZOOM_ABSOLUTE:
		return tevs_set_zoom_target(tevs, ctrl->val);

	case V4L2_CID_TEVS_BSL_MODE:
		return tevs_set_bsl_mode(tevs, ctrl->val);

	case V4L2_CID_TEVS_MAX_FPS:
		return tevs_set_max_fps(tevs, ctrl->val);

	case V4L2_CID_TEVS_AE_EXP_TIME_UPPER:
		return tevs_set_ae_auto_exp_upper(tevs, ctrl->val);

	case V4L2_CID_TEVS_AE_EXP_TIME_MAX:
		return tevs_set_ae_auto_exp_max(tevs, ctrl->val);

	default:
		dev_dbg(tevs->dev, "Unknown control 0x%x\n", ctrl->id);
		return -EINVAL;
	}
}

static const struct v4l2_ctrl_ops tevs_ctrl_ops = {
	.s_ctrl = tevs_s_ctrl,
};

static const struct v4l2_ctrl_config tevs_ctrls[] = {
	/* User-class control IDs */
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_BRIGHTNESS,
		.name = "Brightness",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_CONTRAST,
		.name = "Contrast",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_SATURATION,
		.name = "Saturation",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_AUTO_WHITE_BALANCE,
		.name = "White_Balance_Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.max = TEVS_AWB_CTRL_MODE_AUTO_IDX,
		.def = TEVS_AWB_CTRL_MODE_AUTO_IDX,
		.qmenu = awb_mode_strings,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_GAMMA,
		.name = "Gamma",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x2333, // 2.2
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_EXPOSURE,
		.name = "Exposure",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xF4240,
		.step = 1,
		.def = 0x8235, // 33333 us
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_GAIN,
		.name = "Gain",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x1,
		.max = 0x40,
		.step = 0x1,
		.def = 0x1,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_HFLIP,
		.name = "HFlip",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_VFLIP,
		.name = "VFlip",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_POWER_LINE_FREQUENCY,
		.name = "Power_Line_Frequency",
		.type = V4L2_CTRL_TYPE_MENU,
		.max = TEVS_FLICK_MODE_ENABLED_IDX,
		.def = TEVS_FLICK_MODE_DISABLED_IDX,
		.qmenu = flick_mode_strings,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.name = "White_Balance_Temperature",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x8FC,
		.max = 0x3A98,
		.step = 0x1,
		.def = 0x1388,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_SHARPNESS,
		.name = "Sharpness",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_BACKLIGHT_COMPENSATION,
		.name = "Backlight_Compensation",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_COLORFX,
		.name = "Special_Effect",
		.type = V4L2_CTRL_TYPE_MENU,
		.max = TEVS_SFX_MODE_SFX_SKETCH_IDX,
		.def = TEVS_SFX_MODE_SFX_NORMAL_IDX,
		.qmenu = sfx_mode_strings,
	},
	/*  Camera class control IDs */
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_EXPOSURE_AUTO,
		.name = "Exposure_Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.max = TEVS_AE_CTRL_FULL_AUTO_IDX,
		.def = TEVS_AE_CTRL_FULL_AUTO_IDX,
		.qmenu = ae_mode_strings,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_PAN_ABSOLUTE,
		.name = "Pan_Target",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_TILT_ABSOLUTE,
		.name = "Tilt_Target",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_ZOOM_ABSOLUTE,
		.name = "Zoom_Target",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	/* Image processing controls */
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_LINK_FREQ,
		.min = 0,
		.max = ARRAY_SIZE(tevs_link_freqs) - 1,
		.def = 0,
		.qmenu_int = tevs_link_freqs,
	},
	/* The base for TEVS driver controls. */
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_TEVS_BSL_MODE,
		.name = "BSL_Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.max = TEVS_BSL_MODE_FLASH_IDX,
		.def = TEVS_BSL_MODE_NORMAL_IDX,
		.qmenu = bsl_mode_strings,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_TEVS_MAX_FPS,
		.name = "Max_FPS",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 1,
		.max = 255,
		.step = 1,
		.def = 30,
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_TEVS_AE_EXP_TIME_UPPER,
		.name = "AE_ExpTime_Upper",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x1,
		.max = 0xF4240,
		.step = 1,
		.def = 0x411A, // 16666 us
	},
	{
		.ops = &tevs_ctrl_ops,
		.id = V4L2_CID_TEVS_AE_EXP_TIME_MAX,
		.name = "AE_ExpTime_Max",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x1,
		.max = 0xF4240,
		.step = 1,
		.def = 0x1046A, // 66666 us
	},
};

static int tevs_ctrls_init(struct tevs *tevs)
{
	struct i2c_client *client = v4l2_get_subdevdata(&tevs->v4l2_subdev);
	struct v4l2_fwnode_device_properties props;
	u16 val;
	int ret;
	int i;

	ret = v4l2_ctrl_handler_init(&tevs->ctrls, ARRAY_SIZE(tevs_ctrls));
	if (ret)
		return ret;

	/* Use same lock for controls as for everything else. */
	mutex_init(&tevs->lock);
	tevs->ctrls.lock = &tevs->lock;

	for (i = 0; i < ARRAY_SIZE(tevs_ctrls); i++) {
		struct v4l2_ctrl *ctrl = v4l2_ctrl_new_custom(
			&tevs->ctrls, &tevs_ctrls[i], NULL);
		switch (ctrl->id) {
		case V4L2_CID_BRIGHTNESS:
			ret += tevs_i2c_read_16b(tevs, TEVS_BRIGHTNESS, &val);
			ctrl->default_value = ctrl->cur.val =
				val & TEVS_BRIGHTNESS_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_BRIGHTNESS_MAX,
						 &val);
			ctrl->maximum = val & TEVS_BRIGHTNESS_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_BRIGHTNESS_MIN,
						 &val);
			ctrl->minimum = val & TEVS_BRIGHTNESS_MASK;
			if (ret)
				goto error;
			break;
		case V4L2_CID_CONTRAST:
			ret += tevs_i2c_read_16b(tevs, TEVS_CONTRAST, &val);
			ctrl->default_value = ctrl->cur.val =
				val & TEVS_CONTRAST_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_CONTRAST_MAX, &val);
			ctrl->maximum = val & TEVS_CONTRAST_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_CONTRAST_MIN, &val);
			ctrl->minimum = val & TEVS_CONTRAST_MASK;
			if (ret)
				goto error;
			break;
		case V4L2_CID_SATURATION:
			ret += tevs_i2c_read_16b(tevs, TEVS_SATURATION, &val);
			ctrl->default_value = ctrl->cur.val =
				val & TEVS_SATURATION_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_SATURATION_MAX,
						 &val);
			ctrl->maximum = val & TEVS_SATURATION_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_SATURATION_MIN,
						 &val);
			ctrl->minimum = val & TEVS_SATURATION_MASK;
			if (ret)
				goto error;
			break;
		case V4L2_CID_AUTO_WHITE_BALANCE:
			ret = tevs_i2c_read_16b(tevs, TEVS_AWB_CTRL_MODE, &val);
			if (ret)
				goto error;
			switch (val & TEVS_AWB_CTRL_MODE_MASK) {
			case TEVS_AWB_CTRL_MODE_MANUAL_TEMP:
				ctrl->default_value = ctrl->cur.val = 0;
				break;
			case TEVS_AWB_CTRL_MODE_AUTO:
				ctrl->default_value = ctrl->cur.val = 1;
				break;
			default:
				ctrl->default_value = ctrl->cur.val = 1;
				break;
			}
			break;
		case V4L2_CID_GAMMA:
			ret += tevs_i2c_read_16b(tevs, TEVS_GAMMA, &val);
			ctrl->default_value = ctrl->cur.val = val &
							      TEVS_GAMMA_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_GAMMA_MAX, &val);
			ctrl->maximum = val & TEVS_GAMMA_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_GAMMA_MIN, &val);
			ctrl->minimum = val & TEVS_GAMMA_MASK;
			if (ret)
				goto error;
			break;
		case V4L2_CID_EXPOSURE: {
			u8 exp[4] = { 0 };
			ret += tevs_i2c_read(tevs, TEVS_AE_MANUAL_EXP_TIME, exp,
					     4);
			ctrl->default_value = ctrl->cur.val =
				be32_to_cpup((__be32 *)exp) &
				TEVS_AE_MANUAL_EXP_TIME_MASK;
			ret += tevs_i2c_read(tevs, TEVS_AE_MANUAL_EXP_TIME_MAX,
					     exp, 4);
			ctrl->maximum = be32_to_cpup((__be32 *)exp) &
					TEVS_AE_MANUAL_EXP_TIME_MASK;
			ret += tevs_i2c_read(tevs, TEVS_AE_MANUAL_EXP_TIME_MIN,
					     exp, 4);
			ctrl->minimum = be32_to_cpup((__be32 *)exp) &
					TEVS_AE_MANUAL_EXP_TIME_MASK;
			if (ret)
				goto error;
			break;
		}
		case V4L2_CID_GAIN:
			ret += tevs_i2c_read_16b(tevs, TEVS_AE_MANUAL_GAIN,
						 &val);
			ctrl->default_value = ctrl->cur.val =
				val & TEVS_AE_MANUAL_GAIN_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_AE_MANUAL_GAIN_MAX,
						 &val);
			ctrl->maximum = val & TEVS_AE_MANUAL_GAIN_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_AE_MANUAL_GAIN_MIN,
						 &val);
			ctrl->minimum = val & TEVS_AE_MANUAL_GAIN_MASK;
			if (ret)
				goto error;
			break;
		case V4L2_CID_HFLIP:
			ret += tevs_i2c_read_16b(tevs, TEVS_ORIENTATION, &val);
			ctrl->default_value = ctrl->cur.val =
				val & TEVS_ORIENTATION_HFLIP;
			if (ret)
				goto error;
			break;
		case V4L2_CID_VFLIP:
			ret += tevs_i2c_read_16b(tevs, TEVS_ORIENTATION, &val);
			ctrl->default_value = ctrl->cur.val =
				val & TEVS_ORIENTATION_VFLIP;
			if (ret)
				goto error;
			break;
		case V4L2_CID_POWER_LINE_FREQUENCY:
			ret = tevs_i2c_read_16b(tevs, TEVS_FLICK_CTRL, &val);
			if (ret)
				goto error;
			switch (val & TEVS_FLICK_CTRL_MODE_MASK) {
			case TEVS_FLICK_CTRL_MODE_DISABLED:
				ctrl->default_value = ctrl->cur.val = 0;
				break;
			case TEVS_FLICK_CTRL_MODE_MANUAL:
				if ((val & TEVS_FLICK_CTRL_FREQ_MASK) ==
				    TEVS_FLICK_CTRL_FREQ(50))
					ctrl->default_value = ctrl->cur.val = 1;
				else if ((val & TEVS_FLICK_CTRL_FREQ_MASK) ==
					 TEVS_FLICK_CTRL_FREQ(60))
					ctrl->default_value = ctrl->cur.val = 2;
				break;
			case TEVS_FLICK_CTRL_MODE_AUTO:
				ctrl->default_value = ctrl->cur.val = 3;
				break;
			default:
				ctrl->default_value = ctrl->cur.val = 0;
				break;
			}
			break;
		case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
			ret += tevs_i2c_read_16b(tevs, TEVS_AWB_MANUAL_TEMP,
						 &val);
			ctrl->default_value = ctrl->cur.val =
				val & TEVS_AWB_MANUAL_TEMP_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_AWB_MANUAL_TEMP_MAX,
						 &val);
			ctrl->maximum = val & TEVS_AWB_MANUAL_TEMP_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_AWB_MANUAL_TEMP_MIN,
						 &val);
			ctrl->minimum = val & TEVS_AWB_MANUAL_TEMP_MASK;
			if (ret)
				goto error;
			break;
		case V4L2_CID_SHARPNESS:
			ret += tevs_i2c_read_16b(tevs, TEVS_SHARPEN, &val);
			ctrl->default_value = ctrl->cur.val = val &
							      TEVS_SHARPEN_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_SHARPEN_MAX, &val);
			ctrl->maximum = val & TEVS_SHARPEN_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_SHARPEN_MIN, &val);
			ctrl->minimum = val & TEVS_SHARPEN_MASK;
			if (ret)
				goto error;
			break;
		case V4L2_CID_BACKLIGHT_COMPENSATION:
			ret += tevs_i2c_read_16b(
				tevs, TEVS_BACKLIGHT_COMPENSATION, &val);
			ctrl->default_value = ctrl->cur.val =
				val & TEVS_BACKLIGHT_COMPENSATION_MASK;
			ret += tevs_i2c_read_16b(
				tevs, TEVS_BACKLIGHT_COMPENSATION_MAX, &val);
			ctrl->maximum = val & TEVS_BACKLIGHT_COMPENSATION_MASK;
			ret += tevs_i2c_read_16b(
				tevs, TEVS_BACKLIGHT_COMPENSATION_MIN, &val);
			ctrl->minimum = val & TEVS_BACKLIGHT_COMPENSATION_MASK;
			if (ret)
				goto error;
			break;
		case V4L2_CID_COLORFX:
			ret = tevs_i2c_read_16b(tevs, TEVS_SFX_MODE, &val);
			if (ret)
				goto error;
			switch (val & TEVS_SFX_MODE_SFX_MASK) {
			case TEVS_SFX_MODE_SFX_NORMAL:
				ctrl->default_value = ctrl->cur.val = 0;
				break;
			case TEVS_SFX_MODE_SFX_BW:
				ctrl->default_value = ctrl->cur.val = 1;
				break;
			case TEVS_SFX_MODE_SFX_GRAYSCALE:
				ctrl->default_value = ctrl->cur.val = 2;
				break;
			case TEVS_SFX_MODE_SFX_NEGATIVE:
				ctrl->default_value = ctrl->cur.val = 3;
				break;
			case TEVS_SFX_MODE_SFX_SKETCH:
				ctrl->default_value = ctrl->cur.val = 4;
				break;
			default:
				ctrl->default_value = ctrl->cur.val = 0;
				break;
			}
			break;
		case V4L2_CID_EXPOSURE_AUTO:
			ret = tevs_i2c_read_16b(tevs, TEVS_AE_CTRL_MODE, &val);
			if (ret)
				goto error;
			switch (val & TEVS_AE_CTRL_MODE_MASK) {
			case TEVS_AE_CTRL_MANUAL_EXP_TIME_GAIN:
				ctrl->default_value = ctrl->cur.val = 0;
				break;
			case TEVS_AE_CTRL_FULL_AUTO:
				ctrl->default_value = ctrl->cur.val = 1;
				break;
			default:
				ctrl->default_value = ctrl->cur.val = 1;
				break;
			}
			break;
		case V4L2_CID_PAN_ABSOLUTE:
			ret += tevs_i2c_read_16b(tevs, TEVS_DZ_CT_X, &val);
			ctrl->default_value = ctrl->cur.val = val &
							      TEVS_DZ_CT_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_DZ_CT_MAX, &val);
			ctrl->maximum = val & TEVS_DZ_CT_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_DZ_CT_MIN, &val);
			ctrl->minimum = val & TEVS_DZ_CT_MASK;
			if (ret)
				goto error;
			break;
		case V4L2_CID_TILT_ABSOLUTE:
			ret += tevs_i2c_read_16b(tevs, TEVS_DZ_CT_Y, &val);
			ctrl->default_value = ctrl->cur.val = val &
							      TEVS_DZ_CT_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_DZ_CT_MAX, &val);
			ctrl->maximum = val & TEVS_DZ_CT_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_DZ_CT_MIN, &val);
			ctrl->minimum = val & TEVS_DZ_CT_MASK;
			if (ret)
				goto error;
			break;
		case V4L2_CID_ZOOM_ABSOLUTE:
			ret += tevs_i2c_read_16b(tevs, TEVS_DZ_TGT_FCT, &val);
			ctrl->default_value = ctrl->cur.val =
				val & TEVS_DZ_TGT_FCT_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_DZ_TGT_FCT_MAX,
						 &val);
			ctrl->maximum = val & TEVS_DZ_TGT_FCT_MASK;
			ret += tevs_i2c_read_16b(tevs, TEVS_DZ_TGT_FCT_MIN,
						 &val);
			ctrl->minimum = val & TEVS_DZ_TGT_FCT_MASK;
			if (ret)
				goto error;
			break;
		case V4L2_CID_LINK_FREQ:
			ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;
			break;
		case V4L2_CID_TEVS_BSL_MODE:
			break;
		case V4L2_CID_TEVS_MAX_FPS:
			ret += tevs_i2c_read_16b(tevs, TEVS_MAX_FPS, &val);
			ctrl->default_value = ctrl->cur.val = val &
							      TEVS_MAX_FPS_MASK;
			if (ret)
				goto error;
			tevs->max_fps_ctrl = ctrl;
			break;
		case V4L2_CID_TEVS_AE_EXP_TIME_UPPER: {
			u8 exp[4] = { 0 };
			ret += tevs_i2c_read(tevs, TEVS_AE_AUTO_EXP_TIME_UPPER,
					     exp, 4);
			ctrl->default_value = ctrl->cur.val =
				be32_to_cpup((__be32 *)exp) &
				TEVS_AE_AUTO_EXP_TIME_MASK;
			if (ret)
				goto error;
			break;
		}
		case V4L2_CID_TEVS_AE_EXP_TIME_MAX: {
			u8 exp[4] = { 0 };
			ret += tevs_i2c_read(tevs, TEVS_AE_AUTO_EXP_TIME_MAX,
					     exp, 4);
			ctrl->default_value = ctrl->cur.val =
				be32_to_cpup((__be32 *)exp) &
				TEVS_AE_AUTO_EXP_TIME_MASK;
			if (ret)
				goto error;
			break;
		}
		default:
			break;
		}
	}

	if (tevs->ctrls.error) {
		dev_err(tevs->dev, "ctrls error\n");
		ret = tevs->ctrls.error;
		v4l2_ctrl_handler_free(&tevs->ctrls);
		return ret;
	}

	ret = v4l2_fwnode_device_parse(&client->dev, &props);
	if (ret)
		goto error;

	ret = v4l2_ctrl_new_fwnode_properties(&tevs->ctrls, &tevs_ctrl_ops,
					      &props);
	if (ret)
		goto error;

	tevs->v4l2_subdev.ctrl_handler = &tevs->ctrls;

	return 0;

error:
	v4l2_ctrl_handler_free(&tevs->ctrls);
	mutex_destroy(&tevs->lock);

	return ret;
}

static void tevs_ctrls_free(struct tevs *tevs)
{
	v4l2_ctrl_handler_free(&tevs->ctrls);
	mutex_destroy(&tevs->lock);
}

static int tevs_media_link_setup(struct media_entity *entity,
				 const struct media_pad *local,
				 const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct v4l2_subdev_core_ops tevs_v4l2_subdev_core_ops = {
	.s_power = tevs_power,
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};
static const struct v4l2_subdev_video_ops tevs_v4l2_subdev_video_ops = {
	.g_frame_interval = tevs_get_frame_interval,
	.s_frame_interval = tevs_set_frame_interval,
	.s_stream = tevs_set_stream,
};
static const struct v4l2_subdev_pad_ops tevs_v4l2_subdev_pad_ops = {
	.enum_mbus_code = tevs_enum_mbus_code,
	.get_fmt = tevs_get_fmt,
	.set_fmt = tevs_set_fmt,
	.enum_frame_size = tevs_enum_frame_size,
	.enum_frame_interval = tevs_enum_frame_interval,
};

static const struct v4l2_subdev_ops tevs_subdev_ops = {
	.core = &tevs_v4l2_subdev_core_ops,
	.video = &tevs_v4l2_subdev_video_ops,
	.pad = &tevs_v4l2_subdev_pad_ops,
};

static const struct media_entity_operations tevs_media_entity_ops = {
	.link_setup = tevs_media_link_setup,
};

static int tevs_try_on(struct tevs *tevs)
{
	tevs_power_off(tevs);
	return tevs_power_on(tevs);
}

static int tevs_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tevs *tevs = NULL;
	struct device *dev = &client->dev;
	struct v4l2_mbus_framefmt *fmt;
	int i = ARRAY_SIZE(tevs_sensor_table);
	int ret;

	dev_info(dev, "%s() device node: %s\n", __func__,
		 client->dev.of_node->full_name);

	tevs = devm_kzalloc(dev, sizeof(struct tevs), GFP_KERNEL);
	if (tevs == NULL) {
		dev_err(dev, "allocate memory failed\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&tevs->v4l2_subdev, client, &tevs_subdev_ops);

	i2c_set_clientdata(client, tevs);
	tevs->dev = &client->dev;
	tevs->regmap = devm_regmap_init_i2c(client, &tevs_regmap_config);
	if (IS_ERR(tevs->regmap)) {
		dev_err(dev, "Unable to initialize I2C\n");
		return -ENODEV;
	}

	tevs->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(tevs->reset_gpio)) {
		ret = PTR_ERR(tevs->reset_gpio);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Cannot get reset GPIO (%d)", ret);
		return ret;
	}

	tevs->host_pwdn_gpio =
		devm_gpiod_get_optional(dev, "host-pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(tevs->host_pwdn_gpio)) {
		ret = PTR_ERR(tevs->host_pwdn_gpio);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Cannot get host-pwdn GPIO (%d)", ret);
		return ret;
	}

	tevs->standby_gpio =
		devm_gpiod_get_optional(dev, "standby", GPIOD_OUT_LOW);
	if (IS_ERR(tevs->standby_gpio)) {
		ret = PTR_ERR(tevs->standby_gpio);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Cannot get standby GPIO (%d)", ret);
		return ret;
	}

	tevs->data_lanes = 4;
	if (of_property_read_u32(dev->of_node, "data-lanes",
				 &tevs->data_lanes) == 0) {
		if ((tevs->data_lanes < 1) || (tevs->data_lanes > 4)) {
			dev_err(dev,
				"value of 'data-lanes' property is invaild\n");
			tevs->data_lanes = 4;
		}
	}

	tevs->continuous_clock = 0;
	if (of_property_read_u32(dev->of_node, "continuous-clock",
				 &tevs->continuous_clock) == 0) {
		if (tevs->continuous_clock > 1) {
			dev_err(dev,
				"value of 'continuous-clock' property is invaild\n");
			tevs->continuous_clock = 0;
		}
	}

	tevs->data_frequency = 0;
	if (of_property_read_u32(tevs->dev->of_node, "data-frequency",
				 &tevs->data_frequency) == 0) {
		if ((tevs->data_frequency != 0) &&
		    ((tevs->data_frequency < 100) ||
		     (tevs->data_frequency > 1200))) {
			dev_err(tevs->dev,
				"value of 'data-frequency = <%d>' property is invaild\n",
				tevs->data_frequency);
			return -EINVAL;
		}
	}

	tevs->supports_over_4k_res =
		of_property_read_bool(dev->of_node, "supports-over-4k-res");

	tevs->hw_reset_mode = of_property_read_bool(dev->of_node, "hw-reset");

	tevs->trigger_mode =
		of_property_read_bool(dev->of_node, "trigger-mode");

	tevs->fixed_fps = of_property_read_bool(dev->of_node, "fixed-fps");

	dev_dbg(dev,
		"data-lanes [%d] ,continuous-clock [%d], supports-over-4k-res [%d],"
		" hw-reset [%d], trigger-mode [%d], fixed-fps [%d]\n",
		tevs->data_lanes, tevs->continuous_clock,
		tevs->supports_over_4k_res, tevs->hw_reset_mode,
		tevs->trigger_mode, tevs->fixed_fps);

	if (tevs_try_on(tevs) != 0) {
		dev_err(dev, "cannot find tevs camera\n");
		return -EINVAL;
	}

	if (tevs->data_frequency != 0) {
		ret = tevs_i2c_write_16b(tevs, HOST_COMMAND_ISP_CTRL_MIPI_FREQ,
					 tevs->data_frequency);
		msleep(TEVS_BOOT_TIME);
		if (tevs_check_boot_state(tevs) != 0) {
			dev_err(tevs->dev, "check tevs bootup status failed\n");
			ret = -EINVAL;
			goto error_power_off;
		}
		if (ret < 0) {
			dev_err(tevs->dev, "set mipi frequency failed\n");
			ret = -EINVAL;
			goto error_power_off;
		}
	}

	ret = tevs_check_version(tevs);
	if (ret < 0) {
		dev_err(dev, "check device version failed\n");
		ret = -EINVAL;
		goto error_power_off;
	}

	tevs->header_info =
		devm_kzalloc(dev, sizeof(struct header_info), GFP_KERNEL);
	if (tevs->header_info == NULL) {
		dev_err(dev, "allocate header_info failed\n");
		ret = -ENOMEM;
		goto error_power_off;
	}

	ret = tevs_load_header_info(tevs);
	if (ret < 0) {
		dev_err(dev, "load header information failed\n");
		ret = -EINVAL;
		goto error_power_off;
	} else {
		for (i = 0; i < ARRAY_SIZE(tevs_sensor_table); i++) {
			if (strcmp((const char *)tevs->header_info->product_name,
				   tevs_sensor_table[i].sensor_name) == 0)
				break;
		}
	}

	if (i >= ARRAY_SIZE(tevs_sensor_table)) {
		dev_err(dev, "cannot not support the product: %s\n",
			(const char *)tevs->header_info->product_name);
		ret = -EINVAL;
		goto error_power_off;
	}

	tevs->selected_sensor = i;
	dev_dbg(dev, "selected_sensor:%d, sensor_name:%s\n", i,
		tevs->header_info->product_name);

	/* Initialize default format */
	fmt = &tevs->fmt;
	fmt->width = tevs_sensor_table[tevs->selected_sensor].res_list[0].width;
	fmt->height =
		tevs_sensor_table[tevs->selected_sensor].res_list[0].height;
	fmt->field = V4L2_FIELD_NONE;
	fmt->code = MEDIA_BUS_FMT_UYVY8_2X8;
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
	memset(fmt->reserved, 0, sizeof(fmt->reserved));

	ret = tevs_ctrls_init(tevs);
	if (ret) {
		dev_err(&client->dev, "failed to init controls: %d", ret);
		goto error_power_off;
	}

	/* Initialize subdev */
	tevs->v4l2_subdev.flags |=
		(V4L2_SUBDEV_FL_HAS_EVENTS | V4L2_SUBDEV_FL_HAS_DEVNODE);
	tevs->v4l2_subdev.entity.ops = &tevs_media_entity_ops;
	tevs->v4l2_subdev.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pads */
	tevs->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&tevs->v4l2_subdev.entity, 1, &tevs->pad);
	if (ret) {
		dev_err(tevs->dev, "failed to init entity pads: %d\n", ret);
		goto error_handler_free;
	}

	ret += v4l2_async_register_subdev(&tevs->v4l2_subdev);
	if (ret != 0) {
		dev_err(tevs->dev, "v4l2 register failed\n");
		goto error_media_entity;
	}

	if (tevs->trigger_mode) {
		ret = tevs_enable_trigger_mode(tevs, 1);
		if (ret != 0) {
			dev_err(tevs->dev, "set trigger mode failed\n");
			goto error_media_entity;
		}
	}

	if (!(tevs->hw_reset_mode | tevs->trigger_mode)) {
		ret = tevs_standby(tevs, 1);
		if (ret != 0) {
			dev_err(tevs->dev, "set standby mode failed\n");
			goto error_media_entity;
		}
	} else
		ret = tevs_power_off(tevs);
	if (ret == 0)
		dev_info(dev, "probe success\n");
	else
		dev_err(dev, "probe failed\n");

	return 0;

error_media_entity:
	media_entity_cleanup(&tevs->v4l2_subdev.entity);

error_handler_free:
	tevs_ctrls_free(tevs);

error_power_off:
	tevs_power_off(tevs);

	return ret;
}

static void tevs_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sub_dev = i2c_get_clientdata(client);
	struct tevs *tevs = container_of(sub_dev, struct tevs, v4l2_subdev);

	v4l2_async_unregister_subdev(sub_dev);
	media_entity_cleanup(&sub_dev->entity);
	tevs_ctrls_free(tevs);
}

static const struct i2c_device_id sensor_id[] = { { DRIVER_NAME, 0 }, {} };
MODULE_DEVICE_TABLE(i2c, sensor_id);

static const struct of_device_id sensor_of[] = { { .compatible =
							   "tn," DRIVER_NAME },
						 { /* sentinel */ } };
MODULE_DEVICE_TABLE(of, sensor_of);

static struct i2c_driver sensor_i2c_driver = {
	.driver = {
		.of_match_table = of_match_ptr(sensor_of),
		.name  = DRIVER_NAME,
	},
	.probe = tevs_probe,
	.remove = tevs_remove,
	.id_table = sensor_id,
};

module_i2c_driver(sensor_i2c_driver);

MODULE_AUTHOR("TECHNEXION Inc.");
MODULE_DESCRIPTION("TechNexion driver for TEVS");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("Camera");
