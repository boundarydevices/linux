/*
 * Copyright (C) 2011-2012 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2014 Boundary Devices
 */

/*
 * Modifyed by: Edison Fernández <edison.fernandez@ridgerun.com>
 * Added support to use it with Nitrogen6x
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#define DEBUG 1
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/fsl_devices.h>
#include <linux/mutex.h>
#include <linux/mipi_csi2.h>
#include <media/v4l2-chip-ident.h>
#include "v4l2-int-device.h"
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/soc-dapm.h>
#include <asm/mach-types.h>
//#include <mach/audmux.h>
#include <linux/slab.h>
#include "mxc_v4l2_capture.h"

#define CODEC_CLOCK 16500000
/* SSI clock sources */
#define IMX_SSP_SYS_CLK		0

#define MIN_FPS 30
#define MAX_FPS 60
#define DEFAULT_FPS 60

#define TC358743_XCLK_MIN 27000000
#define TC358743_XCLK_MAX 42000000

#define TC358743_CHIP_ID_HIGH_BYTE		0x0
#define TC358743_CHIP_ID_LOW_BYTE		0x0
#define TC3587430_HDMI_DETECT			0x0f //0x10

#define TC_VOLTAGE_DIGITAL_IO           1800000
#define TC_VOLTAGE_DIGITAL_CORE         1500000
#define TC_VOLTAGE_DIGITAL_GPO		1500000
#define TC_VOLTAGE_ANALOG               2800000

#define MAX_COLORBAR	tc358743_mode_INIT6
#define IS_COLORBAR(a) (a <= MAX_COLORBAR)

enum tc358743_mode {
	tc358743_mode_INIT,
	tc358743_mode_INIT1,
	tc358743_mode_INIT2,
	tc358743_mode_INIT3,
	tc358743_mode_INIT4,
	tc358743_mode_INIT5,
	tc358743_mode_INIT6,
	tc358743_mode_480P_720_480,
	tc358743_mode_720P_60_1280_720,
	tc358743_mode_480P_640_480,
	tc358743_mode_1080P_1920_1080,
	tc358743_mode_720P_1280_720,
	tc358743_mode_1024x768,
	tc358743_mode_MAX,
};

enum tc358743_frame_rate {
	tc358743_60_fps,
	tc358743_30_fps,
	tc358743_max_fps
};

struct reg_value {
	u16 u16RegAddr;
	u32 u32Val;
	u32 u32Mask;
	u8  u8Length;
	u32 u32Delay_ms;
};

struct tc358743_mode_info {
	const char *name;
	enum tc358743_mode mode;
	u32 width;
	u32 height;
	u32 vformat;
	u32 fps;
	u32 lanes;
	u32 freq;
	const struct reg_value *init_data_ptr;
	u32 init_data_size;
	__u32 flags;
};

/*!
 * Maintains the information on the current state of the sensor.
 */
struct tc_data {
	struct sensor_data sensor;
	struct delayed_work det_work;
	struct mutex access_lock;
	int det_work_enable;
	int det_work_timeout;
	int det_changed;
#define REGULATOR_IO		0
#define REGULATOR_CORE		1
#define REGULATOR_GPO		2
#define REGULATOR_ANALOG	3
#define REGULATOR_CNT		4
	struct regulator *regulator[REGULATOR_CNT];
	u32 lock;
	u32 bounce;
	enum tc358743_mode mode;
	u32 fps;
	u32 audio;
	int pwn_gpio;
	int rst_gpio;
	u16 hpd_active;
	int edid_initialized;
};

static struct tc_data *g_td;


#define DET_WORK_TIMEOUT_DEFAULT 100
#define DET_WORK_TIMEOUT_DEFERRED 2000
#define MAX_BOUNCE 5

static void tc_standby(struct tc_data *td, s32 standby)
{
	if (gpio_is_valid(td->pwn_gpio))
		gpio_set_value(td->pwn_gpio, standby ? 1 : 0);
	pr_debug("tc_standby: powerdown=%x, power_gp=0x%x\n", standby, td->pwn_gpio);
	msleep(2);
}

static void tc_reset(struct tc_data *td)
{
	/* camera reset */
	gpio_set_value(td->rst_gpio, 1);

	/* camera power dowmn */
	if (gpio_is_valid(td->pwn_gpio)) {
		gpio_set_value(td->pwn_gpio, 1);
		msleep(5);

		gpio_set_value(td->pwn_gpio, 0);
	}
	msleep(5);

	gpio_set_value(td->rst_gpio, 0);
	msleep(1);

	gpio_set_value(td->rst_gpio, 1);
	msleep(20);

	if (gpio_is_valid(td->pwn_gpio))
		gpio_set_value(td->pwn_gpio, 1);
}

static void tc_io_init(void)
{
	return tc_reset(g_td);
}

const char * const sregulator[REGULATOR_CNT] = {
	[REGULATOR_IO] = "DOVDD",
	[REGULATOR_CORE] = "DVDD",
	[REGULATOR_GPO] = "DGPO",
	[REGULATOR_ANALOG] = "AVDD",
};

static const int voltages[REGULATOR_CNT] = {
	[REGULATOR_IO] = TC_VOLTAGE_DIGITAL_IO,
	[REGULATOR_CORE] = TC_VOLTAGE_DIGITAL_CORE,
	[REGULATOR_GPO] = TC_VOLTAGE_DIGITAL_GPO,
	[REGULATOR_ANALOG] = TC_VOLTAGE_ANALOG,
};

static int tc_regulator_init(struct tc_data *td, struct device *dev)
{
	int i;
	int ret = 0;

	for (i = 0; i < REGULATOR_CNT; i++) {
		td->regulator[i] = devm_regulator_get(dev, sregulator[i]);
		if (!IS_ERR(td->regulator[i])) {
			regulator_set_voltage(td->regulator[i],
				voltages[i], voltages[i]);
		} else {
			pr_err("%s:%s devm_regulator_get failed\n", __func__, sregulator[i]);
			td->regulator[i] = NULL;
		}
	}
	return ret;
}

static void det_work_enable(struct tc_data *td, int enable)
{
	td->det_work_enable = enable;
	td->det_work_timeout = DET_WORK_TIMEOUT_DEFERRED;
	if (enable)
		schedule_delayed_work(&td->det_work, msecs_to_jiffies(10));
	pr_debug("%s: %d %d\n", __func__, td->det_work_enable, td->det_work_timeout);
}

static const u8 cHDMIEDID[256] = {
	/* FIXME! This is the edid that my ASUS HDMI monitor returns */
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,		//0 - header[8] - fixed pattern
	0x04, 0x69,						//8 - manufacturer_name[2]
	0xf3, 0x24,						//10 - product_code[2]
	0xd6, 0x12, 0x00, 0x00,					//12 - serial_number[4]
	0x16,							//16 - week
	0x16,							//17 - year 22+1990=2012
	0x01,							//18 - version
	0x03,							//19 - revision
	0x80,							//20 - video_input_definition(digital input)
	0x34,							//21 - max_size_horizontal 52cm
	0x1d,							//22 - max_size_vertical 29cm
	0x78,							//23 - gamma
	0x3a,							//24 - feature_support (RGB 4:4:4 + YCrCb 4:4:4 + YCrCb 4:2:2)
	0xc7, 0x20, 0xa4, 0x55, 0x49, 0x99, 0x27, 0x13, 0x50, 0x54,	//25 - color_characteristics[10]
	0xbf, 0xef, 0x00,					//35 - established_timings[3]
	0x71, 0x4f, 0x81, 0x40, 0x81, 0x80, 0x95, 0x00, 0xb3, 0x00, 0xd1, 0xc0, 0x01, 0x01, 0x01, 0x01,	//38 - standard_timings[8]
/* 1080P */
	0x02, 0x3a,						//54(0) - descriptor[0], 0x3a02 = 148.50 MHz
	0x80,							//56(2) h - active 0x780 (1920)
	0x18,							//57(3) h - blank 0x118 (280)
	0x71,							//58(4)
	0x38,							//59(5) v - active 0x438 (1080)
	0x2d,							//60(6) v - blank 0x02d(45)
	0x40,							//61(7)
	0x58,							//62(8) - h sync offset(0x58)
	0x2c,							//63(9) - h sync width(0x2c)
	0x45,							//64(10) - v sync offset(0x4), v sync width(0x5)
	0x00,							//65(11)
	0x09,							//66(12) - h display size (0x209) 521 mm
	0x25,							//67(13) - v display size (0x125) 293 mm
	0x21,							//68(14)
	0x00,							//69(15) - h border pixels
	0x00,							//70(16) - v border pixels
	0x1e,							//71(17) - no stereo, digital separate, hsync+, vsync+
	0x00, 0x00, 0x00, 0xff, 0x00,							//72 - descriptor[1]
	0x43, 0x36, 0x4c, 0x4d, 0x54, 0x46, 0x30, 0x30, 0x34, 0x38, 0x32, 0x32,	0x0a,	//"C6LMTF004822\n"
	0x00, 0x00, 0x00, 0xfd, 0x00,							//90 - descriptor[2]
	0x37, 0x4b, 0x1e, 0x55, 0x10, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x00, 0x00, 0x00, 0xfc, 0x00, 							//108 - descriptor[3]
	0x41, 0x53, 0x55, 0x53, 0x20, 0x56, 0x48, 0x32, 0x34, 0x32, 0x48, 0x0a, 0x20, 	//"ASUS VH242H\n "
	0x01,							//126 - extension_flag
	0x68,							//127 - checksum

	0x02,							//0 - cea-861 extension
	0x03,							//1 - rev 3
	0x22,							//2 - detailed timings at offset 34
	0x71,							//3 - # of detailed timing descriptor
	0x4f, 0x01, 0x02, 0x03, 0x11, 0x12,			//4
	0x13, 0x04, 0x14, 0x05, 0x0e, 0x0f,			//10
	0x1d, 0x1e, 0x1f, 0x10, 0x23, 0x09,			//16
	0x07, 0x01, 0x83, 0x01, 0x00, 0x00,			//22
	0x65, 0x03, 0x0c, 0x00, 0x10, 0x00,			//28
/* 720x480@59.94  27000000/858/525 = 59.94 Hz */
	0x8c, 0x0a,				//34 - descriptor[0] - 0x0a8c - 27 Mhz
	0xd0,					//h - active 0x2d0 (720)
	0x8a,					//h - blank 0x8a(138)
	0x20,
	0xe0,					//v - active 0x1e0 (480)
	0x2d,					//v - blank 0x2d (45)
	0x10,
	0x10, 0x3e, 0x96, 0x00, 0x09, 0x25, 0x21, 0x00, 0x00, 0x18,
/* 1280x720@60  74250000/1650/750 = 60 Hz*/
	0x01, 0x1d,				//52 - 0x1d01 74.25MHz
	0x00,					//h - active (0x500)1280
	0x72,					//h - blank (0x172)370
	0x51,
	0xd0,					//v active 0x2d0(720)
	0x1e,					//v blank 0x1e(30)
	0x20,
	0x6e, 0x28, 0x55, 0x00, 0x09, 0x25, 0x21, 0x00, 0x00, 0x1e,
/* 1280x720@50  74250000/1980/750 = 50 Hz  */
	0x01, 0x1d,				//70 - 0x1d01 74.25MHz
	0x00,					//h - active (0x500)1280
	0xbc,					//h - blank (0x2bc)700
	0x52,
	0xd0,					//v active 0x2d0 (720)
	0x1e,					//v blank 0x1e(30)
	0x20,
	0xb8, 0x28, 0x55, 0x40, 0x09, 0x25, 0x21, 0x00, 0x00, 0x1e,
/* 720x576@50 27000000/864/625 = 50 Hz */
	0x8c, 0x0a,				//88 0x0a8c - 27 Mhz
	0xd0,					//h - active 0x2d0(720)
	0x90,					//h - blank 0x90(144)
	0x20,
	0x40,					//v active 0x240(576)
	0x31,					//v blanking 0x31(49)
	0x20,
	0x0c, 0x40, 0x55, 0x00, 0x09, 0x25, 0x21, 0x00, 0x00, 0x18,
/* done */
	0x00, 0x00, 0x00, 0x00, 0x00,				//106
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,						//124
	0x00,							//126 - extension_flag
	0x73,							//127 - checksum
};

static const struct reg_value tc358743_setting_YUV422_2lane_30fps_720P_1280_720_125MHz[] = {
  {0x0006, 0x00000040, 0x00000000, 2, 0},
  {0x0014, 0x00000000, 0x00000000, 2, 0},
  {0x0016, 0x000005ff, 0x00000000, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x0000402d, 0x00000000, 2, 0},
  {0x0022, 0x00000213, 0x00000000, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00000e00, 0x00000000, 4, 0},
  {0x0214, 0x00000001, 0x00000000, 4, 0},
  {0x0218, 0x00000801, 0x00000000, 4, 0},
  {0x021c, 0x00000001, 0x00000000, 4, 0},
  {0x0220, 0x00000001, 0x00000000, 4, 0},
  {0x0224, 0x00004800, 0x00000000, 4, 0},
  {0x0228, 0x00000005, 0x00000000, 4, 0},
  {0x022c, 0x00000000, 0x00000000, 4, 0},
  {0x0234, 0x0000001f, 0x00000000, 4, 0},
  {0x0238, 0x00000001, 0x00000000, 4, 0}, //non-continuous clock
  {0x0204, 0x00000001, 0x00000000, 4, 0},
  {0x0518, 0x00000001, 0x00000000, 4, 0},
  {0x0500, 0xa300be82, 0x00000000, 4, 0},
// HDMI Interrupt Mask
  {0x8502, 0x00000001, 0x00000000, 1, 0},
  {0x8512, 0x000000fe, 0x00000000, 1, 0},
  {0x8514, 0x00000000, 0x00000000, 1, 0},
  {0x8515, 0x00000000, 0x00000000, 1, 0},
  {0x8516, 0x00000000, 0x00000000, 1, 0},
// HDMI Audio
  {0x8531, 0x00000001, 0x00000000, 1, 0},
  {0x8630, 0x000000b0, 0x00000000, 1, 0},
  {0x8631, 0x0000001e, 0x00000000, 1, 0},
  {0x8632, 0x00000004, 0x00000000, 1, 0},
  {0x8670, 0x00000001, 0x00000000, 1, 0},
// HDMI PHY
  {0x8532, 0x00000080, 0x00000000, 1, 0},
  {0x8536, 0x00000040, 0x00000000, 1, 0},
  {0x853f, 0x0000000a, 0x00000000, 1, 0},
// HDMI System
  {0x8545, 0x00000031, 0x00000000, 1, 0},
  {0x8546, 0x0000002d, 0x00000000, 1, 0},
// HDCP Setting
  {0x85d1, 0x00000001, 0x00000000, 1, 0},
  {0x8560, 0x00000024, 0x00000000, 1, 0},
  {0x8563, 0x00000011, 0x00000000, 1, 0},
  {0x8564, 0x0000000f, 0x00000000, 1, 0},
// Video settings
  {0x8573, 0x00000081, 0x00000000, 1, 0},
  {0x8571, 0x00000002, 0x00000000, 1, 0},
// HDMI Audio In Setting
  {0x8600, 0x00000000, 0x00000000, 1, 0},
  {0x8602, 0x000000f3, 0x00000000, 1, 0},
  {0x8603, 0x00000002, 0x00000000, 1, 0},
  {0x8604, 0x0000000c, 0x00000000, 1, 0},
  {0x8606, 0x00000005, 0x00000000, 1, 0},
  {0x8607, 0x00000000, 0x00000000, 1, 0},
  {0x8620, 0x00000022, 0x00000000, 1, 0},
  {0x8640, 0x00000001, 0x00000000, 1, 0},
  {0x8641, 0x00000065, 0x00000000, 1, 0},
  {0x8642, 0x00000007, 0x00000000, 1, 0},
//  {0x8651, 0x00000003, 0x00000000, 1, 0},	// Inverted LRCK polarity - (Sony) format
  {0x8652, 0x00000002, 0x00000000, 1, 0},	// Left-justified I2S (Phillips) format
//  {0x8652, 0x00000000, 0x00000000, 1, 0},	// Right-justified (Sony) format
  {0x8665, 0x00000010, 0x00000000, 1, 0},
// InfoFrame Extraction
  {0x8709, 0x000000ff, 0x00000000, 1, 0},
  {0x870b, 0x0000002c, 0x00000000, 1, 0},
  {0x870c, 0x00000053, 0x00000000, 1, 0},
  {0x870d, 0x00000001, 0x00000000, 1, 0},
  {0x870e, 0x00000030, 0x00000000, 1, 0},
  {0x9007, 0x00000010, 0x00000000, 1, 0},
  {0x854a, 0x00000001, 0x00000000, 1, 0},
// Output Control
  {0x0004, 0x00000cf7, 0x00000000, 2, 0},
  };

static const struct reg_value tc358743_setting_YUV422_4lane_1024x768_60fps_125MHz[] = {
  {0x0006, 0x00000000, 0x00000000, 2, 0},
  {0x0014, 0x0000ffff, 0x00000000, 2, 0},
  {0x0016, 0x000005ff, 0x00000000, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x0000405c, 0x00000000, 2, 0},	/* Input divide 5(4+1), Feedback divide 92(0x5c+1)*/
  {0x0022, 0x00000613, 0x00000000, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00000d00, 0x00000000, 4, 0},
  {0x0214, 0x00000001, 0x00000000, 4, 0},
  {0x0218, 0x00000701, 0x00000000, 4, 0},
  {0x021c, 0x00000000, 0x00000000, 4, 0},
  {0x0220, 0x00000001, 0x00000000, 4, 0},
  {0x0224, 0x00004000, 0x00000000, 4, 0},
  {0x0228, 0x00000005, 0x00000000, 4, 0},
  {0x022c, 0x00000000, 0x00000000, 4, 0},
  {0x0234, 0x0000001f, 0x00000000, 4, 0},
  {0x0238, 0x00000001, 0x00000000, 4, 0},
  {0x0204, 0x00000001, 0x00000000, 4, 0},
  {0x0518, 0x00000001, 0x00000000, 4, 0},
  {0x0500, 0xa300be86, 0x00000000, 4, 0},
// HDMI Interrupt Mask
  {0x8502, 0x00000001, 0x00000000, 1, 0},
  {0x8512, 0x000000fe, 0x00000000, 1, 0},
  {0x8514, 0x00000000, 0x00000000, 1, 0},
  {0x8515, 0x00000000, 0x00000000, 1, 0},
  {0x8516, 0x00000000, 0x00000000, 1, 0},
// HDMI Audio
  {0x8531, 0x00000001, 0x00000000, 1, 0},
  {0x8630, 0x000000b0, 0x00000000, 1, 0},
  {0x8631, 0x0000001e, 0x00000000, 1, 0},
  {0x8632, 0x00000004, 0x00000000, 1, 0},
  {0x8670, 0x00000001, 0x00000000, 1, 0},
// HDMI PHY
  {0x8532, 0x00000080, 0x00000000, 1, 0},
  {0x8536, 0x00000040, 0x00000000, 1, 0},
  {0x853f, 0x0000000a, 0x00000000, 1, 0},
// HDMI System
  {0x8545, 0x00000031, 0x00000000, 1, 0},
  {0x8546, 0x0000002d, 0x00000000, 1, 0},
// HDCP Setting
  {0x85d1, 0x00000001, 0x00000000, 1, 0},
  {0x8560, 0x00000024, 0x00000000, 1, 0},
  {0x8563, 0x00000011, 0x00000000, 1, 0},
  {0x8564, 0x0000000f, 0x00000000, 1, 0},
// RGB --> YUV Conversion
//  {0x8574, 0x00000000, 0x00000000, 1, 0},
  {0x8573, 0x00000081, 0x00000000, 1, 0},
  {0x8571, 0x00000002, 0x00000000, 1, 0},
// HDMI Audio In Setting
  {0x8600, 0x00000000, 0x00000000, 1, 0},
  {0x8602, 0x000000f3, 0x00000000, 1, 0},
  {0x8603, 0x00000002, 0x00000000, 1, 0},
  {0x8604, 0x0000000c, 0x00000000, 1, 0},
  {0x8606, 0x00000005, 0x00000000, 1, 0},
  {0x8607, 0x00000000, 0x00000000, 1, 0},
  {0x8620, 0x00000022, 0x00000000, 1, 0},
  {0x8640, 0x00000001, 0x00000000, 1, 0},
  {0x8641, 0x00000065, 0x00000000, 1, 0},
  {0x8642, 0x00000007, 0x00000000, 1, 0},
  {0x8652, 0x00000002, 0x00000000, 1, 0},
  {0x8665, 0x00000010, 0x00000000, 1, 0},
// InfoFrame Extraction
  {0x8709, 0x000000ff, 0x00000000, 1, 0},
  {0x870b, 0x0000002c, 0x00000000, 1, 0},
  {0x870c, 0x00000053, 0x00000000, 1, 0},
  {0x870d, 0x00000001, 0x00000000, 1, 0},
  {0x870e, 0x00000030, 0x00000000, 1, 0},
  {0x9007, 0x00000010, 0x00000000, 1, 0},
  {0x854a, 0x00000001, 0x00000000, 1, 0},
// Output Control
  {0x0004, 0x00000cf7, 0x00000000, 2, 0},

};

static const struct reg_value tc358743_setting_YUV422_4lane_720P_60fps_1280_720_133Mhz[] = {
  {0x0006, 0x00000000, 0x00000000, 2, 0},
  {0x0014, 0x0000ffff, 0x00000000, 2, 0},
  {0x0016, 0x000005ff, 0x00000000, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x00004062, 0x00000000, 2, 0},
  {0x0022, 0x00000613, 0x00000000, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00000d00, 0x00000000, 4, 0},
  {0x0214, 0x00000001, 0x00000000, 4, 0},
  {0x0218, 0x00000701, 0x00000000, 4, 0},
  {0x021c, 0x00000000, 0x00000000, 4, 0},
  {0x0220, 0x00000001, 0x00000000, 4, 0},
  {0x0224, 0x00004000, 0x00000000, 4, 0},
  {0x0228, 0x00000005, 0x00000000, 4, 0},
  {0x022c, 0x00000000, 0x00000000, 4, 0},
  {0x0234, 0x0000001f, 0x00000000, 4, 0},
  {0x0238, 0x00000001, 0x00000000, 4, 0},
  {0x0204, 0x00000001, 0x00000000, 4, 0},
  {0x0518, 0x00000001, 0x00000000, 4, 0},
  {0x0500, 0xa300be86, 0x00000000, 4, 0},
// HDMI Interrupt Mask
  {0x8502, 0x00000001, 0x00000000, 1, 0},
  {0x8512, 0x000000fe, 0x00000000, 1, 0},
  {0x8514, 0x00000000, 0x00000000, 1, 0},
  {0x8515, 0x00000000, 0x00000000, 1, 0},
  {0x8516, 0x00000000, 0x00000000, 1, 0},
// HDMI Audio
  {0x8531, 0x00000001, 0x00000000, 1, 0},
  {0x8630, 0x000000b0, 0x00000000, 1, 0},
  {0x8631, 0x0000001e, 0x00000000, 1, 0},
  {0x8632, 0x00000004, 0x00000000, 1, 0},
  {0x8670, 0x00000001, 0x00000000, 1, 0},
// HDMI PHY
  {0x8532, 0x00000080, 0x00000000, 1, 0},
  {0x8536, 0x00000040, 0x00000000, 1, 0},
  {0x853f, 0x0000000a, 0x00000000, 1, 0},
// HDMI System
  {0x8545, 0x00000031, 0x00000000, 1, 0},
  {0x8546, 0x0000002d, 0x00000000, 1, 0},
// HDCP Setting
  {0x85d1, 0x00000001, 0x00000000, 1, 0},
  {0x8560, 0x00000024, 0x00000000, 1, 0},
  {0x8563, 0x00000011, 0x00000000, 1, 0},
  {0x8564, 0x0000000f, 0x00000000, 1, 0},
// RGB --> YUV Conversion
//  {0x8574, 0x00000000, 0x00000000, 1, 0},
  {0x8573, 0x00000081, 0x00000000, 1, 0},
  {0x8571, 0x00000002, 0x00000000, 1, 0},
// HDMI Audio In Setting
  {0x8600, 0x00000000, 0x00000000, 1, 0},
  {0x8602, 0x000000f3, 0x00000000, 1, 0},
  {0x8603, 0x00000002, 0x00000000, 1, 0},
  {0x8604, 0x0000000c, 0x00000000, 1, 0},
  {0x8606, 0x00000005, 0x00000000, 1, 0},
  {0x8607, 0x00000000, 0x00000000, 1, 0},
  {0x8620, 0x00000022, 0x00000000, 1, 0},
  {0x8640, 0x00000001, 0x00000000, 1, 0},
  {0x8641, 0x00000065, 0x00000000, 1, 0},
  {0x8642, 0x00000007, 0x00000000, 1, 0},
  {0x8652, 0x00000002, 0x00000000, 1, 0},
  {0x8665, 0x00000010, 0x00000000, 1, 0},
// InfoFrame Extraction
  {0x8709, 0x000000ff, 0x00000000, 1, 0},
  {0x870b, 0x0000002c, 0x00000000, 1, 0},
  {0x870c, 0x00000053, 0x00000000, 1, 0},
  {0x870d, 0x00000001, 0x00000000, 1, 0},
  {0x870e, 0x00000030, 0x00000000, 1, 0},
  {0x9007, 0x00000010, 0x00000000, 1, 0},
  {0x854a, 0x00000001, 0x00000000, 1, 0},
// Output Control
  {0x0004, 0x00000cf7, 0x00000000, 2, 0},
};

static const struct reg_value tc358743_setting_YUV422_2lane_color_bar_1280_720_125MHz[] = {
  {0x0006, 0x00000000, 0x00000000, 2, 0},
  {0x0004, 0x00000084, 0x00000000, 2, 0},
  {0x0010, 0x0000001e, 0x00000000, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x0000405c, 0x00000000, 2, 0},
  {0x0022, 0x00000613, 0x00000000, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00000e00, 0x00000000, 4, 0},
  {0x0214, 0x00000001, 0x00000000, 4, 0},
  {0x0218, 0x00000801, 0x00000000, 4, 0},
  {0x021c, 0x00000000, 0x00000000, 4, 0},
  {0x0220, 0x00000001, 0x00000000, 4, 0},
  {0x0224, 0x00004000, 0x00000000, 4, 0},
  {0x0228, 0x00000006, 0x00000000, 4, 0},
  {0x022c, 0x00000000, 0x00000000, 4, 0},
  {0x0234, 0x00000007, 0x00000000, 4, 0},
  {0x0238, 0x00000001, 0x00000000, 4, 0}, //non-continuous clock
  {0x0204, 0x00000001, 0x00000000, 4, 0},
  {0x0518, 0x00000001, 0x00000000, 4, 0},
  {0x0500, 0xa30080a2, 0x00000000, 4, 0},
// 1280x720 colorbar
  {0x000a, 0x00000a00, 0x00000000, 2, 0},
  {0x7080, 0x00000082, 0x00000000, 2, 0},
// 128 pixel black - repeat 128 times
  {0x7000, 0x0000007f, 0x00000000, 2, (1<<24)|(128<<16)},
// 128 pixel blue - repeat 64 times
  {0x7000, 0x000000ff, 0x00000000, 2, 0},
  {0x7000, 0x00000000, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel red - repeat 64 times
  {0x7000, 0x00000000, 0x00000000, 2, 0},
  {0x7000, 0x000000ff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel pink - repeat 64 times
  {0x7000, 0x00007fff, 0x00000000, 2, 0},
  {0x7000, 0x00007fff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel green - repeat 64 times
  {0x7000, 0x00007f00, 0x00000000, 2, 0},
  {0x7000, 0x00007f00, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel light blue - repeat 64 times
  {0x7000, 0x0000c0ff, 0x00000000, 2, 0},
  {0x7000, 0x0000c000, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel yellow - repeat 64 times
  {0x7000, 0x0000ff00, 0x00000000, 2, 0},
  {0x7000, 0x0000ffff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel white - repeat 64 times
  {0x7000, 0x0000ff7f, 0x00000000, 2, 0},
  {0x7000, 0x0000ff7f, 0x00000000, 2, (2<<24)|(64<<16)},
// 720 lines
  {0x7090, 0x000002cf, 0x00000000, 2, 0},
  {0x7092, 0x00000580, 0x00000000, 2, 0},
  {0x7094, 0x00000010, 0x00000000, 2, 0},
  {0x7080, 0x00000083, 0x00000000, 2, 0},
};

static const struct reg_value tc358743_setting_YUV422_4lane_color_bar_1280_720_125MHz[] = {
  {0x0006, 0x00000000, 0x00000000, 2, 0},
  {0x0004, 0x00000084, 0x00000000, 2, 0},
  {0x0010, 0x0000001e, 0x00000000, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x0000405c, 0x00000000, 2, 0},
  {0x0022, 0x00000613, 0x00000000, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00000e00, 0x00000000, 4, 0},
  {0x0214, 0x00000001, 0x00000000, 4, 0},
  {0x0218, 0x00000801, 0x00000000, 4, 0},
  {0x021c, 0x00000000, 0x00000000, 4, 0},
  {0x0220, 0x00000001, 0x00000000, 4, 0},
  {0x0224, 0x00004000, 0x00000000, 4, 0},
  {0x0228, 0x00000006, 0x00000000, 4, 0},
  {0x022c, 0x00000000, 0x00000000, 4, 0},
  {0x0234, 0x0000001F, 0x00000000, 4, 0}, //{0x0234, 0x00000007, 0x00000000, 4, 0},
  {0x0238, 0x00000001, 0x00000000, 4, 0}, //non-continuous clock
  {0x0204, 0x00000001, 0x00000000, 4, 0},
  {0x0518, 0x00000001, 0x00000000, 4, 0},
  {0x0500, 0xa30080a6, 0x00000000, 4, 0}, //{0x0500, 0xa30080a2, 0x00000000, 4, 0},
// 1280x720 colorbar
  {0x000a, 0x00000a00, 0x00000000, 2, 0},
  {0x7080, 0x00000082, 0x00000000, 2, 0},
// 128 pixel black - repeat 128 times
  {0x7000, 0x0000007f, 0x00000000, 2, (1<<24)|(128<<16)},
// 128 pixel blue - repeat 64 times
  {0x7000, 0x000000ff, 0x00000000, 2, 0},
  {0x7000, 0x00000000, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel red - repeat 64 times
  {0x7000, 0x00000000, 0x00000000, 2, 0},
  {0x7000, 0x000000ff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel pink - repeat 64 times
  {0x7000, 0x00007fff, 0x00000000, 2, 0},
  {0x7000, 0x00007fff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel green - repeat 64 times
  {0x7000, 0x00007f00, 0x00000000, 2, 0},
  {0x7000, 0x00007f00, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel light blue - repeat 64 times
  {0x7000, 0x0000c0ff, 0x00000000, 2, 0},
  {0x7000, 0x0000c000, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel yellow - repeat 64 times
  {0x7000, 0x0000ff00, 0x00000000, 2, 0},
  {0x7000, 0x0000ffff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel white - repeat 64 times
  {0x7000, 0x0000ff7f, 0x00000000, 2, 0},
  {0x7000, 0x0000ff7f, 0x00000000, 2, (2<<24)|(64<<16)},
// 720 lines
  {0x7090, 0x000002cf, 0x00000000, 2, 0},
  {0x7092, 0x00000300, 0x00000000, 2, 0}, //{0x7092, 0x00000580, 0x00000000, 2, 0},
  {0x7094, 0x00000010, 0x00000000, 2, 0},
  {0x7080, 0x00000083, 0x00000000, 2, 0},
};


static const struct reg_value tc358743_setting_YUV422_4lane_color_bar_1024_720_200MHz[] = {
  {0x0006, 0x00000000, 0x00000000, 2, 0},
  {0x0004, 0x00000084, 0x00000000, 2, 0},
  {0x0010, 0x0000001e, 0x00000000, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x00004050, 0x00000000, 2, 0},
  {0x0022, 0x00000213, 0x00000000, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00001800, 0x00000000, 4, 0},
  {0x0214, 0x00000002, 0x00000000, 4, 0},
  {0x0218, 0x00001102, 0x00000000, 4, 0},
  {0x021c, 0x00000000, 0x00000000, 4, 0},
  {0x0220, 0x00000003, 0x00000000, 4, 0},
  {0x0224, 0x00004000, 0x00000000, 4, 0},
  {0x0228, 0x00000007, 0x00000000, 4, 0},
  {0x022c, 0x00000001, 0x00000000, 4, 0},
  {0x0234, 0x0000001f, 0x00000000, 4, 0},
  {0x0238, 0x00000001, 0x00000000, 4, 0}, //non-continuous clock
  {0x0204, 0x00000001, 0x00000000, 4, 0},
  {0x0518, 0x00000001, 0x00000000, 4, 0},
  {0x0500, 0xa30080a6, 0x00000000, 4, 0},
// 1280x720 colorbar
  {0x000a, 0x00000800, 0x00000000, 2, 0},
  {0x7080, 0x00000082, 0x00000000, 2, 0},
// 128 pixel black - repeat 128 times
  {0x7000, 0x0000007f, 0x00000000, 2, (1<<24)|(128<<16)},
// 128 pixel blue - repeat 64 times
  {0x7000, 0x000000ff, 0x00000000, 2, 0},
  {0x7000, 0x00000000, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel red - repeat 64 times
  {0x7000, 0x00000000, 0x00000000, 2, 0},
  {0x7000, 0x000000ff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel pink - repeat 64 times
  {0x7000, 0x00007fff, 0x00000000, 2, 0},
  {0x7000, 0x00007fff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel green - repeat 64 times
  {0x7000, 0x00007f00, 0x00000000, 2, 0},
  {0x7000, 0x00007f00, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel light blue - repeat 64 times
  {0x7000, 0x0000c0ff, 0x00000000, 2, 0},
  {0x7000, 0x0000c000, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel yellow - repeat 64 times
  {0x7000, 0x0000ff00, 0x00000000, 2, 0},
  {0x7000, 0x0000ffff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel white - repeat 64 times
  {0x7000, 0x0000ff7f, 0x00000000, 2, 0},
  {0x7000, 0x0000ff7f, 0x00000000, 2, (2<<24)|(64<<16)},
// 720 lines
  {0x0020, 0x0000406f, 0x00000000, 2, 100},
  {0x7090, 0x000002cf, 0x00000000, 2, 0},
  {0x7092, 0x00000540, 0x00000000, 2, 0},
  {0x7094, 0x00000010, 0x00000000, 2, 0},
  {0x7080, 0x00000083, 0x00000000, 2, 0},
};

static const struct reg_value tc358743_setting_YUV422_4lane_color_bar_1280_720_300MHz[] = {
  {0x0006, 0x00000000, 0x00000000, 2, 0},
  {0x0004, 0x00000084, 0x00000000, 2, 0},
  {0x0010, 0x0000001e, 0x00000000, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x000080c7, 0x00000000, 2, 0},
  {0x0022, 0x00000213, 0x00000000, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00001e00, 0x00000000, 4, 0},
  {0x0214, 0x00000003, 0x00000000, 4, 0},
  {0x0218, 0x00001402, 0x00000000, 4, 0},
  {0x021c, 0x00000000, 0x00000000, 4, 0},
  {0x0220, 0x00000003, 0x00000000, 4, 0},
  {0x0224, 0x00004a00, 0x00000000, 4, 0},
  {0x0228, 0x00000008, 0x00000000, 4, 0},
  {0x022c, 0x00000002, 0x00000000, 4, 0},
  {0x0234, 0x0000001f, 0x00000000, 4, 0},
  {0x0238, 0x00000001, 0x00000000, 4, 0}, //non-continuous clock
  {0x0204, 0x00000001, 0x00000000, 4, 0},
  {0x0518, 0x00000001, 0x00000000, 4, 0},
  {0x0500, 0xa30080a6, 0x00000000, 4, 0},
// 1280x720 colorbar
  {0x000a, 0x00000a00, 0x00000000, 2, 0},
  {0x7080, 0x00000082, 0x00000000, 2, 0},
// 128 pixel black - repeat 128 times
  {0x7000, 0x0000007f, 0x00000000, 2, (1<<24)|(128<<16)},
// 128 pixel blue - repeat 64 times
  {0x7000, 0x000000ff, 0x00000000, 2, 0},
  {0x7000, 0x00000000, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel red - repeat 64 times
  {0x7000, 0x00000000, 0x00000000, 2, 0},
  {0x7000, 0x000000ff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel pink - repeat 64 times
  {0x7000, 0x00007fff, 0x00000000, 2, 0},
  {0x7000, 0x00007fff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel green - repeat 64 times
  {0x7000, 0x00007f00, 0x00000000, 2, 0},
  {0x7000, 0x00007f00, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel light blue - repeat 64 times
  {0x7000, 0x0000c0ff, 0x00000000, 2, 0},
  {0x7000, 0x0000c000, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel yellow - repeat 64 times
  {0x7000, 0x0000ff00, 0x00000000, 2, 0},
  {0x7000, 0x0000ffff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel white - repeat 64 times
  {0x7000, 0x0000ff7f, 0x00000000, 2, 0},
  {0x7000, 0x0000ff7f, 0x00000000, 2, (2<<24)|(64<<16)},
// 720 lines
  {0x7090, 0x000002cf, 0x00000000, 2, 0},
  {0x7092, 0x000006b8, 0x00000000, 2, 0},
  {0x7094, 0x00000010, 0x00000000, 2, 0},
  {0x7080, 0x00000083, 0x00000000, 2, 0},
};

static const struct reg_value tc358743_setting_YUV422_4lane_color_bar_1920_1023_300MHz[] = {
  {0x0006, 0x00000000, 0x00000000, 2, 0},
  {0x0004, 0x00000084, 0x00000000, 2, 0},
  {0x0010, 0x0000001e, 0x00000000, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x000080c7, 0x00000000, 2, 0},
  {0x0022, 0x00000213, 0x00000000, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00001e00, 0x00000000, 4, 0},
  {0x0214, 0x00000003, 0x00000000, 4, 0},
  {0x0218, 0x00001402, 0x00000000, 4, 0},
  {0x021c, 0x00000000, 0x00000000, 4, 0},
  {0x0220, 0x00000003, 0x00000000, 4, 0},
  {0x0224, 0x00004a00, 0x00000000, 4, 0},
  {0x0228, 0x00000008, 0x00000000, 4, 0},
  {0x022c, 0x00000002, 0x00000000, 4, 0},
  {0x0234, 0x0000001f, 0x00000000, 4, 0},
  {0x0238, 0x00000001, 0x00000000, 4, 0}, //non-continuous clock
  {0x0204, 0x00000001, 0x00000000, 4, 0},
  {0x0518, 0x00000001, 0x00000000, 4, 0},
  {0x0500, 0xa30080a6, 0x00000000, 4, 0},
// 1920x1023 colorbar
  {0x000a, 0x00000f00, 0x00000000, 2, 0},
  {0x7080, 0x00000082, 0x00000000, 2, 0},
// 128 pixel black - repeat 128 times
  {0x7000, 0x0000007f, 0x00000000, 2, (1<<24)|(128<<16)},
// 128 pixel blue - repeat 64 times
  {0x7000, 0x000000ff, 0x00000000, 2, 0},
  {0x7000, 0x00000000, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel red - repeat 64 times
  {0x7000, 0x00000000, 0x00000000, 2, 0},
  {0x7000, 0x000000ff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel pink - repeat 64 times
  {0x7000, 0x00007fff, 0x00000000, 2, 0},
  {0x7000, 0x00007fff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel green - repeat 64 times
  {0x7000, 0x00007f00, 0x00000000, 2, 0},
  {0x7000, 0x00007f00, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel light blue - repeat 64 times
  {0x7000, 0x0000c0ff, 0x00000000, 2, 0},
  {0x7000, 0x0000c000, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel yellow - repeat 64 times
  {0x7000, 0x0000ff00, 0x00000000, 2, 0},
  {0x7000, 0x0000ffff, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel white - repeat 64 times
  {0x7000, 0x0000ff7f, 0x00000000, 2, 0},
  {0x7000, 0x0000ff7f, 0x00000000, 2, (2<<24)|(64<<16)},
// 1023 lines
  {0x7090, 0x000003fe, 0x00000000, 2, 0},
  {0x7092, 0x000004d8, 0x00000000, 2, 0},
  {0x7094, 0x0000002d, 0x00000000, 2, 0},
  {0x7080, 0x00000083, 0x00000000, 2, 0},
};

static const struct reg_value tc358743_setting_YUV422_2lane_color_bar_640_480_174MHz[] = {
  {0x0006, 0x00000000, 0x00000000, 2, 0},
  {0x0004, 0x00000084, 0x00000000, 2, 0},
  {0x0010, 0x0000001e, 0x00000000, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x00008073, 0x00000000, 2, 0},
  {0x0022, 0x00000213, 0x00000000, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 0x00000000, 4, 0},
//  {0x014c, 0x00000000, 0x00000000, 4, 0},
//  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00001200, 0x00000000, 4, 0},
  {0x0214, 0x00000002, 0x00000000, 4, 0},
  {0x0218, 0x00000b02, 0x00000000, 4, 0},
  {0x021c, 0x00000001, 0x00000000, 4, 0},
  {0x0220, 0x00000103, 0x00000000, 4, 0},
  {0x0224, 0x00004000, 0x00000000, 4, 0},
  {0x0228, 0x00000008, 0x00000000, 4, 0},
  {0x022c, 0x00000002, 0x00000000, 4, 0},
  {0x0234, 0x0000001f, 0x00000000, 4, 0},
  {0x0238, 0x00000000, 0x00000000, 4, 0},
  {0x0204, 0x00000001, 0x00000000, 4, 0},
  {0x0518, 0x00000001, 0x00000000, 4, 0},
  {0x0500, 0xA3008082, 0x00000000, 4, 0},
// 640x480 colorbar
  {0x000a, 0x00000500, 0x00000000, 2, 0},
  {0x7080, 0x00000082, 0x00000000, 2, 0},
// 80 pixel black - repeate 80 times
  {0x7000, 0x0000007f, 0x00000000, 2, (1<<24)|(80<<16)},
// 80 pixel blue - repeate 40 times
  {0x7000, 0x000000ff, 0x00000000, 2, 0},
  {0x7000, 0x00000000, 0x00000000, 2, (2<<24)|(40<<16)},
// 80 pixel red - repeate 40 times
  {0x7000, 0x00000000, 0x00000000, 2, 0},
  {0x7000, 0x000000ff, 0x00000000, 2, (2<<24)|(40<<16)},
// 80 pixel pink - repeate 40 times
  {0x7000, 0x00007fff, 0x00000000, 2, 0},
  {0x7000, 0x00007fff, 0x00000000, 2, (2<<24)|(40<<16)},
// 80 pixel green - repeate 40 times
  {0x7000, 0x00007f00, 0x00000000, 2, 0},
  {0x7000, 0x00007f00, 0x00000000, 2, (2<<24)|(40<<16)},
// 80 pixel light blue - repeate 40 times
  {0x7000, 0x0000c0ff, 0x00000000, 2, 0},
  {0x7000, 0x0000c000, 0x00000000, 2, (2<<24)|(40<<16)},
// 80 pixel yellow - repeate 40 times
  {0x7000, 0x0000ff00, 0x00000000, 2, 0},
  {0x7000, 0x0000ffff, 0x00000000, 2, (2<<24)|(40<<16)},
// 80 pixel white - repeate 40 times
  {0x7000, 0x0000ff7f, 0x00000000, 2, 0},
  {0x7000, 0x0000ff7f, 0x00000000, 2, (2<<24)|(40<<16)},
// 480 lines
  {0x7090, 0x000001df, 0x00000000, 2, 0},
  {0x7092, 0x00000898, 0x00000000, 2, 0},
  {0x7094, 0x00000285, 0x00000000, 2, 0},
  {0x7080, 0x00000083, 0x00000000, 2, 0},
};

static const struct reg_value tc358743_setting_YUV422_2lane_color_bar_640_480_108MHz_cont[] = {
  {0x0006, 0x00000000, 0x00000000, 2, 0},
  {0x0004, 0x00000084, 0x00000000, 2, 0},
  {0x0010, 0x0000001e, 0x00000000, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x0000404F, 0x00000000, 2, 0},
  {0x0022, 0x00000613, 0x00000000, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00001800, 0x00000000, 4, 0},
  {0x0214, 0x00000002, 0x00000000, 4, 0},
  {0x0218, 0x00001102, 0x00000000, 4, 0},
  {0x021c, 0x00000000, 0x00000000, 4, 0},
  {0x0220, 0x00000003, 0x00000000, 4, 0},
  {0x0224, 0x00004000, 0x00000000, 4, 0},
  {0x0228, 0x00000007, 0x00000000, 4, 0},
  {0x022c, 0x00000001, 0x00000000, 4, 0},
  {0x0234, 0x0000001f, 0x00000000, 4, 0},
  {0x0238, 0x00000001, 0x00000000, 4, 0},
  {0x0204, 0x00000001, 0x00000000, 4, 0},
  {0x0518, 0x00000001, 0x00000000, 4, 0},
  {0x0500, 0xA30080A2, 0x00000000, 4, 0},
// 640x480 colorbar
  {0x000a, 0x00000500, 0x00000000, 2, 0},
  {0x7080, 0x00000082, 0x00000000, 2, 0},
// 80 pixel black - repeate 80 times
  {0x7000, 0x0000007f, 0x00000000, 2, (1<<24)|(80<<16)},
// 80 pixel blue - repeate 40 times
  {0x7000, 0x000000ff, 0x00000000, 2, 0},
  {0x7000, 0x00000000, 0x00000000, 2, (2<<24)|(40<<16)},
// 80 pixel red - repeate 40 times
  {0x7000, 0x00000000, 0x00000000, 2, 0},
  {0x7000, 0x000000ff, 0x00000000, 2, (2<<24)|(40<<16)},
// 80 pixel pink - repeate 40 times
  {0x7000, 0x00007fff, 0x00000000, 2, 0},
  {0x7000, 0x00007fff, 0x00000000, 2, (2<<24)|(40<<16)},
// 80 pixel green - repeate 40 times
  {0x7000, 0x00007f00, 0x00000000, 2, 0},
  {0x7000, 0x00007f00, 0x00000000, 2, (2<<24)|(40<<16)},
// 80 pixel light blue - repeate 40 times
  {0x7000, 0x0000c0ff, 0x00000000, 2, 0},
  {0x7000, 0x0000c000, 0x00000000, 2, (2<<24)|(40<<16)},
// 80 pixel yellow - repeate 40 times
  {0x7000, 0x0000ff00, 0x00000000, 2, 0},
  {0x7000, 0x0000ffff, 0x00000000, 2, (2<<24)|(40<<16)},
// 80 pixel white - repeate 40 times
  {0x7000, 0x0000ff7f, 0x00000000, 2, 0},
  {0x7000, 0x0000ff7f, 0x00000000, 2, (2<<24)|(40<<16)},
// 480 lines
  {0x7090, 0x000001df, 0x00000000, 2, 0},
  {0x7092, 0x00000700, 0x00000000, 2, 0},
  {0x7094, 0x00000010, 0x00000000, 2, 0},
  {0x7080, 0x00000083, 0x00000000, 2, 0},
};

//480p RGB2YUV442
static const struct reg_value tc358743_setting_YUV422_2lane_60fps_640_480_125Mhz[] = {
  {0x0006, 0x00000040, 0x00000000, 2, 0},
//  {0x000a, 0x000005a0, 0x00000000, 2, 0},
//  {0x0010, 0x0000001e, 0x00000000, 2, 0},
  {0x0014, 0x00000000, 0x00000000, 2, 0},
  {0x0016, 0x000005ff, 0x00000000, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x0000405c, 0x00000000, 2, 0},
  {0x0022, 0x00000613, 0x00000000, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00000d00, 0x00000000, 4, 0},
  {0x0214, 0x00000001, 0x00000000, 4, 0},
  {0x0218, 0x00000701, 0x00000000, 4, 0},
  {0x021c, 0x00000000, 0x00000000, 4, 0},
  {0x0220, 0x00000001, 0x00000000, 4, 0},
  {0x0224, 0x00004000, 0x00000000, 4, 0},
  {0x0228, 0x00000005, 0x00000000, 4, 0},
  {0x022c, 0x00000000, 0x00000000, 4, 0},
  {0x0234, 0x0000001f, 0x00000000, 4, 0},
  {0x0238, 0x00000001, 0x00000000, 4, 0},
  {0x0204, 0x00000001, 0x00000000, 4, 0},
  {0x0518, 0x00000001, 0x00000000, 4, 0},
  {0x0500, 0xA30080A2, 0x00000000, 4, 0},
// HDMI Interrupt Mask
  {0x8502, 0x00000001, 0x00000000, 1, 0},
  {0x8512, 0x000000fe, 0x00000000, 1, 0},
  {0x8514, 0x00000000, 0x00000000, 1, 0},
  {0x8515, 0x00000000, 0x00000000, 1, 0},
  {0x8516, 0x00000000, 0x00000000, 1, 0},
// HDMI Audio
  {0x8531, 0x00000001, 0x00000000, 1, 0},
  {0x8630, 0x000000b0, 0x00000000, 1, 0},
  {0x8631, 0x0000001e, 0x00000000, 1, 0},
  {0x8632, 0x00000004, 0x00000000, 1, 0},
  {0x8670, 0x00000001, 0x00000000, 1, 0},
// HDMI PHY
  {0x8532, 0x00000080, 0x00000000, 1, 0},
  {0x8536, 0x00000040, 0x00000000, 1, 0},
  {0x853f, 0x0000000a, 0x00000000, 1, 0},
// HDMI System
  {0x8545, 0x00000031, 0x00000000, 1, 0},
  {0x8546, 0x0000002d, 0x00000000, 1, 0},
// HDCP Setting
  {0x85d1, 0x00000001, 0x00000000, 1, 0},
  {0x8560, 0x00000024, 0x00000000, 1, 0},
  {0x8563, 0x00000011, 0x00000000, 1, 0},
  {0x8564, 0x0000000f, 0x00000000, 1, 0},
// RGB --> YUV Conversion
  {0x8573, 0x00000081, 0x00000000, 1, 0},
  {0x8571, 0x00000002, 0x00000000, 1, 0},
// HDMI Audio In Setting
  {0x8600, 0x00000000, 0x00000000, 1, 0},
  {0x8602, 0x000000f3, 0x00000000, 1, 0},
  {0x8603, 0x00000002, 0x00000000, 1, 0},
  {0x8604, 0x0000000c, 0x00000000, 1, 0},
  {0x8606, 0x00000005, 0x00000000, 1, 0},
  {0x8607, 0x00000000, 0x00000000, 1, 0},
  {0x8620, 0x00000022, 0x00000000, 1, 0},
  {0x8640, 0x00000001, 0x00000000, 1, 0},
  {0x8641, 0x00000065, 0x00000000, 1, 0},
  {0x8642, 0x00000007, 0x00000000, 1, 0},
  {0x8652, 0x00000002, 0x00000000, 1, 0},
  {0x8665, 0x00000010, 0x00000000, 1, 0},
// InfoFrame Extraction
  {0x8709, 0x000000ff, 0x00000000, 1, 0},
  {0x870b, 0x0000002c, 0x00000000, 1, 0},
  {0x870c, 0x00000053, 0x00000000, 1, 0},
  {0x870d, 0x00000001, 0x00000000, 1, 0},
  {0x870e, 0x00000030, 0x00000000, 1, 0},
  {0x9007, 0x00000010, 0x00000000, 1, 0},
  {0x854a, 0x00000001, 0x00000000, 1, 0},
// Output Control
  {0x0004, 0x00000cf7, 0x00000000, 2, 0},
  };

//480p RGB2YUV442
static const struct reg_value tc358743_setting_YUV422_2lane_60fps_720_480_125Mhz[] = {
  {0x0006, 0x00000040, 0x00000000, 2, 0},
  {0x000a, 0x000005a0, 0x00000000, 2, 0},
//  {0x0010, 0x0000001e, 0x00000000, 2, 0},
  {0x0014, 0x00000000, 0x00000000, 2, 0},
  {0x0016, 0x000005ff, 0x00000000, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x0000405b, 0x00000000, 2, 0},
  {0x0022, 0x00000613, 0x00000000, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00000d00, 0x00000000, 4, 0},
  {0x0214, 0x00000001, 0x00000000, 4, 0},
  {0x0218, 0x00000701, 0x00000000, 4, 0},
  {0x021c, 0x00000000, 0x00000000, 4, 0},
  {0x0220, 0x00000001, 0x00000000, 4, 0},
  {0x0224, 0x00004000, 0x00000000, 4, 0},
  {0x0228, 0x00000005, 0x00000000, 4, 0},
  {0x022c, 0x00000000, 0x00000000, 4, 0},
  {0x0234, 0x0000001f, 0x00000000, 4, 0},
  {0x0238, 0x00000001, 0x00000000, 4, 0},
  {0x0204, 0x00000001, 0x00000000, 4, 0},
  {0x0518, 0x00000001, 0x00000000, 4, 0},
  {0x0500, 0xA30080A2, 0x00000000, 4, 0},
// HDMI Interrupt Mask
  {0x8502, 0x00000001, 0x00000000, 1, 0},
  {0x8512, 0x000000fe, 0x00000000, 1, 0},
  {0x8514, 0x00000000, 0x00000000, 1, 0},
  {0x8515, 0x00000000, 0x00000000, 1, 0},
  {0x8516, 0x00000000, 0x00000000, 1, 0},
// HDMI Audio
  {0x8531, 0x00000001, 0x00000000, 1, 0},
  {0x8630, 0x000000b0, 0x00000000, 1, 0},
  {0x8631, 0x0000001e, 0x00000000, 1, 0},
  {0x8632, 0x00000004, 0x00000000, 1, 0},
  {0x8670, 0x00000001, 0x00000000, 1, 0},
// HDMI PHY
  {0x8532, 0x00000080, 0x00000000, 1, 0},
  {0x8536, 0x00000040, 0x00000000, 1, 0},
  {0x853f, 0x0000000a, 0x00000000, 1, 0},
// HDMI System
  {0x8545, 0x00000031, 0x00000000, 1, 0},
  {0x8546, 0x0000002d, 0x00000000, 1, 0},
// HDCP Setting
  {0x85d1, 0x00000001, 0x00000000, 1, 0},
  {0x8560, 0x00000024, 0x00000000, 1, 0},
  {0x8563, 0x00000011, 0x00000000, 1, 0},
  {0x8564, 0x0000000f, 0x00000000, 1, 0},
// RGB --> YUV Conversion
  {0x8573, 0x00000081, 0x00000000, 1, 0},
  {0x8571, 0x00000002, 0x00000000, 1, 0},
// HDMI Audio In Setting
  {0x8600, 0x00000000, 0x00000000, 1, 0},
  {0x8602, 0x000000f3, 0x00000000, 1, 0},
  {0x8603, 0x00000002, 0x00000000, 1, 0},
  {0x8604, 0x0000000c, 0x00000000, 1, 0},
  {0x8606, 0x00000005, 0x00000000, 1, 0},
  {0x8607, 0x00000000, 0x00000000, 1, 0},
  {0x8620, 0x00000022, 0x00000000, 1, 0},
  {0x8640, 0x00000001, 0x00000000, 1, 0},
  {0x8641, 0x00000065, 0x00000000, 1, 0},
  {0x8642, 0x00000007, 0x00000000, 1, 0},
  {0x8652, 0x00000002, 0x00000000, 1, 0},
  {0x8665, 0x00000010, 0x00000000, 1, 0},
// InfoFrame Extraction
  {0x8709, 0x000000ff, 0x00000000, 1, 0},
  {0x870b, 0x0000002c, 0x00000000, 1, 0},
  {0x870c, 0x00000053, 0x00000000, 1, 0},
  {0x870d, 0x00000001, 0x00000000, 1, 0},
  {0x870e, 0x00000030, 0x00000000, 1, 0},
  {0x9007, 0x00000010, 0x00000000, 1, 0},
  {0x854a, 0x00000001, 0x00000000, 1, 0},
// Output Control
  {0x0004, 0x00000cf7, 0x00000000, 2, 0},
  };

static const struct reg_value tc358743_setting_YUV422_4lane_1080P_60fps_1920_1080_300MHz[] = {
  {0x0004, 0x00000084, 0x00000000, 2, 0},
  {0x0006, 0x00000000, 0x00000000, 2, 0},
  {0x0014, 0x00000000, 0x00000000, 2, 0},
  {0x0016, 0x000005ff, 0x00000000, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x000080c7, 0x00000000, 2, 0},
  {0x0022, 0x00000213, 0x00000000, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00001e00, 0x00000000, 4, 0},
  {0x0214, 0x00000003, 0x00000000, 4, 0},
  {0x0218, 0x00001402, 0x00000000, 4, 0},
  {0x021c, 0x00000000, 0x00000000, 4, 0},
  {0x0220, 0x00000003, 0x00000000, 4, 0},
  {0x0224, 0x00004a00, 0x00000000, 4, 0},
  {0x0228, 0x00000008, 0x00000000, 4, 0},
  {0x022c, 0x00000002, 0x00000000, 4, 0},
  {0x0234, 0x0000001f, 0x00000000, 4, 0},
  {0x0238, 0x00000001, 0x00000000, 4, 0},
  {0x0204, 0x00000001, 0x00000000, 4, 0},
  {0x0518, 0x00000001, 0x00000000, 4, 0},
  {0x0500, 0xa30080a6, 0x00000000, 4, 0},
// HDMI Interrupt Mask
  {0x8502, 0x00000001, 0x00000000, 1, 0},
  {0x8512, 0x000000fe, 0x00000000, 1, 0},
  {0x8514, 0x00000000, 0x00000000, 1, 0},
  {0x8515, 0x00000000, 0x00000000, 1, 0},
  {0x8516, 0x00000000, 0x00000000, 1, 0},
// HDMI Audio
  {0x8531, 0x00000001, 0x00000000, 1, 0},
  {0x8630, 0x000000b0, 0x00000000, 1, 0},
  {0x8631, 0x0000001e, 0x00000000, 1, 0},
  {0x8632, 0x00000004, 0x00000000, 1, 0},
  {0x8670, 0x00000001, 0x00000000, 1, 0},
// HDMI PHY
  {0x8532, 0x00000080, 0x00000000, 1, 0},
  {0x8536, 0x00000040, 0x00000000, 1, 0},
  {0x853f, 0x0000000a, 0x00000000, 1, 0},
// HDMI System
  {0x8545, 0x00000031, 0x00000000, 1, 0},
  {0x8546, 0x0000002d, 0x00000000, 1, 0},
// HDCP Setting
  {0x85d1, 0x00000001, 0x00000000, 1, 0},
  {0x8560, 0x00000024, 0x00000000, 1, 0},
  {0x8563, 0x00000011, 0x00000000, 1, 0},
  {0x8564, 0x0000000f, 0x00000000, 1, 0},
// RGB --> YUV Conversion
  {0x8571, 0x00000002, 0x00000000, 1, 0},
  {0x8573, 0x00000081, 0x00000000, 1, 0},
  {0x8576, 0x00000060, 0x00000000, 1, 0},
// HDMI Audio In Setting
  {0x8600, 0x00000000, 0x00000000, 1, 0},
  {0x8602, 0x000000f3, 0x00000000, 1, 0},
  {0x8603, 0x00000002, 0x00000000, 1, 0},
  {0x8604, 0x0000000c, 0x00000000, 1, 0},
  {0x8606, 0x00000005, 0x00000000, 1, 0},
  {0x8607, 0x00000000, 0x00000000, 1, 0},
  {0x8620, 0x00000022, 0x00000000, 1, 0},
  {0x8640, 0x00000001, 0x00000000, 1, 0},
  {0x8641, 0x00000065, 0x00000000, 1, 0},
  {0x8642, 0x00000007, 0x00000000, 1, 0},
  {0x8652, 0x00000002, 0x00000000, 1, 0},
  {0x8665, 0x00000010, 0x00000000, 1, 0},
// InfoFrame Extraction
  {0x8709, 0x000000ff, 0x00000000, 1, 0},
  {0x870b, 0x0000002c, 0x00000000, 1, 0},
  {0x870c, 0x00000053, 0x00000000, 1, 0},
  {0x870d, 0x00000001, 0x00000000, 1, 0},
  {0x870e, 0x00000030, 0x00000000, 1, 0},
  {0x9007, 0x00000010, 0x00000000, 1, 0},
  {0x854a, 0x00000001, 0x00000000, 1, 0},
// Output Control
  {0x0004, 0x00000cf7, 0x00000000, 2, 0},
};

static const struct reg_value tc358743_setting_YUV422_4lane_1080P_30fps_1920_1080_300MHz[] = {
  {0x0004, 0x00000084, 0x00000000, 2, 0},		//  Internal Generated output pattern,Do not send InfoFrame data out to CSI2,Audio output to CSI2-TX i/f,I2C address index increments on every data byte transfer, disable audio and video TX buffers
  {0x0006, 0x000001f8, 0x00000000, 2, 0},		// FIFO level = 1f8 = 504
  {0x0014, 0x00000000, 0x00000000, 2, 0},		// Clear interrupt status bits
  {0x0016, 0x000005ff, 0x00000000, 2, 0},		// Mask audio mute, CSI-TX, and the other interrups
// Program CSI Tx PLL
  //{0x0020, 0x000080c7, 0x00000000, 2, 0},		// Input divider setting = 0x8 -> Division ratio = (PRD3..0) + 1 = 9, Feedback divider setting = 0xc7 -> Division ratio = (FBD8...0) + 1 = 200
  {0x0020, 0x000080c7, 0x00000000, 2, 0},		// Input divider setting = 0x8 -> Division ratio = (PRD3..0) + 1 = 9, Feedback divider setting = 0xc7 -> Division ratio = (FBD8...0) + 1 = 200
  {0x0022, 0x00000213, 0x00000000, 2, 0},		// HSCK frequency = 500MHz – 1GHz HSCK frequency, Loop bandwidth setting = 50% of maximum loop bandwidth (default), REFCLK toggling –> normal operation, REFCLK stops -> no oscillation, Bypass clock = normal operation, clocks switched off (output LOW), PLL Reset normal operation, PLL Enable = PLL on
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 0x00000000, 4, 0},		// Clock Lane DPHY Control: Bypass Lane Enable from PPI Layer enable.
  {0x0144, 0x00000000, 0x00000000, 4, 0},		// Data Lane 0 DPHY Control: Bypass Lane Enable from PPI Layer enable.
  {0x0148, 0x00000000, 0x00000000, 4, 0},		// Data Lane 1 DPHY Control: Bypass Lane Enable from PPI Layer enable.
  {0x014c, 0x00000000, 0x00000000, 4, 0},		// Data Lane 2 DPHY Control: Bypass Lane Enable from PPI Layer enable.
  {0x0150, 0x00000000, 0x00000000, 4, 0},		// Data Lane 3 DPHY Control: Bypass Lane Enable from PPI Layer enable.
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00001e00, 0x00000000, 4, 0},		// LINEINITCNT: Line Initialization Wait Counter = 0x1e00 = 7680
  {0x0214, 0x00000003, 0x00000000, 4, 0},		// LPTXTIMECNT: SYSLPTX Timing Generation Counter = 3
  {0x0218, 0x00001402, 0x00000000, 4, 0},				// TCLK_HEADERCNT: TCLK_ZERO Counter = 0x14 = 20, TCLK_PREPARE Counter = 0x02 = 2
  {0x021c, 0x00000000, 0x00000000, 4, 0},		// TCLK_TRAILCNT: TCLK_TRAIL Counter = 0
  {0x0220, 0x00000003, 0x00000000, 4, 0},		// THS_HEADERCNT: THS_ZERO Counter = 0, THS_PREPARE Counter = 3
  {0x0224, 0x00004a00, 0x00000000, 4, 0},		// TWAKEUP: TWAKEUP Counter = 0x4a00 = 18944
  {0x0228, 0x00000008, 0x00000000, 4, 0},		// TCLK_POSTCNT: TCLK_POST Counter = 8
  {0x022c, 0x00000002, 0x00000000, 4, 0},		// THS_TRAILCNT: THS_TRAIL Counter = 2
  {0x0234, 0x0000001f, 0x00000000, 4, 0},		// HSTXVREGEN: Enable voltage regulators for lanes and clk
  {0x0238, 0x00000001, 0x00000000, 4, 0},		    // TXOPTIONCNTRL: Set Continuous Clock Mode
  {0x0204, 0x00000001, 0x00000000, 4, 0},		// PPI STARTCNTRL: start PPI function
  {0x0518, 0x00000001, 0x00000000, 4, 0},		// CSI_START: start
  {0x0500, 0xa30080a6, 0x00000000, 4, 0},		// CSI Configuration Register: set register 0x040C with data 0x80a6 (CSI MOde, Disables the HTX_TO timer, High-Speed data transfer is performed to Tx, DSCClk Stays in HS mode when Data Lane goes to LP, 4 Data Lanes,The EOT packet is automatically granted at the end of HS transfer then is transmitted)
// HDMI Interrupt Mask
  {0x8502, 0x00000001, 0x00000000, 1, 0},		// SYSTEM INTERRUPT: clear DDC power change detection interrupt
  {0x8512, 0x000000fe, 0x00000000, 1, 0},		// SYS INTERRUPT MASK: DDC power change detection interrupt not masked
  {0x8514, 0x00000000, 0x00000000, 1, 0},		// PACKET INTERRUPT MASK: unmask all
  {0x8515, 0x00000000, 0x00000000, 1, 0},		// CBIT INTERRUPT MASK: unmask all
  {0x8516, 0x00000000, 0x00000000, 1, 0},		// AUDIO INTERRUPT MASK: unmask all
// HDMI Audio
  {0x8531, 0x00000001, 0x00000000, 1, 0},		// PHY CONTROL0: 27MHz, DDC5V detection operation.
  {0x8630, 0x000000b0, 0x00000000, 1, 0},		// Audio FS Lock Detect Control: for 27MHz
  {0x8631, 0x0000001e, 0x00000000, 1, 0},
  {0x8632, 0x00000004, 0x00000000, 1, 0},
  {0x8670, 0x00000001, 0x00000000, 1, 0},
// HDMI PHY
  {0x8532, 0x00000080, 0x00000000, 1, 0},		//
  {0x8536, 0x00000040, 0x00000000, 1, 0},		//
  {0x853f, 0x0000000a, 0x00000000, 1, 0},		//
// HDMI System
  {0x8545, 0x00000031, 0x00000000, 1, 0},		// ANA CONTROL: PLL charge pump setting for Audio = normal, DAC/PLL power ON/OFF setting for Audio = ON
  {0x8546, 0x0000002d, 0x00000000, 1, 0},		// AVMUTE CONTROL: AVM_CTL = 0x2d
// HDCP Setting
  {0x85d1, 0x00000001, 0x00000000, 1, 0},		//
  {0x8560, 0x00000024, 0x00000000, 1, 0},		// HDCP MODE: HDCP automatic reset when DVI⇔HDMI switched = on, HDCP Line Rekey timing switch = 7clk mode (Data island delay ON), Bcaps[5] KSVINFO_READY(0x8840[5]) auto clear mode = Auto clear using AKSV write
  {0x8563, 0x00000011, 0x00000000, 1, 0},		//
  {0x8564, 0x0000000f, 0x00000000, 1, 0},		//
// RGB --> YUV Conversion
  {0x8571, 0x00000002, 0x00000000, 1, 0},		//
  {0x8573, 0x000000c1, 0x00000000, 1, 0},		// VOUT SET2 REGISTER: 422 fixed output, Video Output 422 conversion mode selection 000: During 444 input, 3tap filter; during 422 input, simple decimation, Enable RGB888 to YUV422 Conversion (Fixed Color output)
  {0x8574, 0x00000008, 0x00000000, 1, 0},				// VOUT SET3 REGISTER (VOUT_SET3): Follow register bit 0x8573[7] setting
  {0x8576, 0x00000060, 0x00000000, 1, 0},		// VOUT_COLOR: Output Color = 601 YCbCr Limited, Input Pixel Repetition judgment = automatic, Input Pixel Repetition HOST setting = no repetition
// HDMI Audio In Setting
  {0x8600, 0x00000000, 0x00000000, 1, 0},
  {0x8602, 0x000000f3, 0x00000000, 1, 0},
  {0x8603, 0x00000002, 0x00000000, 1, 0},
  {0x8604, 0x0000000c, 0x00000000, 1, 0},
  {0x8606, 0x00000005, 0x00000000, 1, 0},
  {0x8607, 0x00000000, 0x00000000, 1, 0},
  {0x8620, 0x00000022, 0x00000000, 1, 0},
  {0x8640, 0x00000001, 0x00000000, 1, 0},
  {0x8641, 0x00000065, 0x00000000, 1, 0},
  {0x8642, 0x00000007, 0x00000000, 1, 0},
  {0x8652, 0x00000002, 0x00000000, 1, 0},
  {0x8665, 0x00000010, 0x00000000, 1, 0},
// InfoFrame Extraction
  {0x8709, 0x000000ff, 0x00000000, 1, 0},		// PACKET INTERRUPT MODE: all enable
  {0x870b, 0x0000002c, 0x00000000, 1, 0},		// NO PACKET LIMIT: NO_ACP_LIMIT = 0x2, NO_AVI_LIMIT = 0xC
  {0x870c, 0x00000053, 0x00000000, 1, 0},		// When VS receive interrupt is detected, VS storage register automatic clear, When ACP receive interrupt is detected, ACP storage register automatic clear, When AVI receive interrupt occurs, judge input video signal with RGB and no Repetition, When AVI receive interrupt is detected, AVI storage register automatic clear.
  {0x870d, 0x00000001, 0x00000000, 1, 0},		// ERROR PACKET LIMIT: Packet continuing receive error occurrence detection threshold = 1
  {0x870e, 0x00000030, 0x00000000, 1, 0},		// NO PACKET LIMIT:
  {0x9007, 0x00000010, 0x00000000, 1, 0},		//
  {0x854a, 0x00000001, 0x00000000, 1, 0},		// Initialization completed flag
// Output Control
  {0x0004, 0x00000cf7, 0x00000000, 2, 0},		// Configuration Control Register: Power Island Normal, I2S/TDM clock are free running,  Enable 2 Audio channels, Audio channel number Auto detect by HW, I2S/TDM Data no delay, Select YCbCr422 8-bit (HDMI YCbCr422 12-bit data format), Send InfoFrame data out to CSI2, Audio output to I2S i/f (valid for 2 channel only), I2C address index increments on every data byte transfer, Audio and Video tx buffres enable.
};

/* list of image formats supported by TCM825X sensor */
static const struct v4l2_fmtdesc tc358743_formats[] = {
	{
		.description	= "RGB888 (RGB24)",
		.pixelformat	= V4L2_PIX_FMT_RGB24,		/* 24  RGB-8-8-8     */
		.flags		= MIPI_DT_RGB888		//	0x24
	},
	{
		.description	= "RAW12 (Y/CbCr 4:2:0)",
		.pixelformat	= V4L2_PIX_FMT_UYVY,		/* 12  Y/CbCr 4:2:0  */
		.flags		= MIPI_DT_RAW12			//	0x2c
	},
	{
		.description	= "YUV 4:2:2 8-bit",
		.pixelformat	= V4L2_PIX_FMT_YUYV, 		/*  8  8-bit color   */
		.flags		= MIPI_DT_YUV422		//	0x1e /* UYVY...		*/
	},
};


static const struct tc358743_mode_info tc358743_mode_info_data[2][tc358743_mode_MAX] = {
/* Color bar test settings */
	[1][tc358743_mode_INIT] =
		{"cb640x480-108MHz@30", tc358743_mode_INIT,  640, 480,
		6, 1, 2, 108,
		tc358743_setting_YUV422_2lane_color_bar_640_480_108MHz_cont,
		ARRAY_SIZE(tc358743_setting_YUV422_2lane_color_bar_640_480_108MHz_cont),
		MIPI_DT_YUV422
		},
	[0][tc358743_mode_INIT] =
		{"cb640x480-108MHz@60", tc358743_mode_INIT,  640, 480,
		6, 1, 2, 108,
		tc358743_setting_YUV422_2lane_color_bar_640_480_108MHz_cont,
		ARRAY_SIZE(tc358743_setting_YUV422_2lane_color_bar_640_480_108MHz_cont),
		MIPI_DT_YUV422
		},
	[1][tc358743_mode_INIT4] =
		{"cb640x480-174Mhz@30", tc358743_mode_INIT4,  640, 480,
		6, 1, 2, 174,
		tc358743_setting_YUV422_2lane_color_bar_640_480_174MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_2lane_color_bar_640_480_174MHz),
		MIPI_DT_YUV422
		},
	[0][tc358743_mode_INIT4] =
		{"cb640x480-174MHz@60", tc358743_mode_INIT4,  640, 480,
		6, 1, 2, 174,
		tc358743_setting_YUV422_2lane_color_bar_640_480_174MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_2lane_color_bar_640_480_174MHz),
		MIPI_DT_YUV422
		},
	[1][tc358743_mode_INIT3] =
		{"cb1024x720-4lane@30", tc358743_mode_INIT3,  1024, 720,
		6, 1, 4, 300,
		tc358743_setting_YUV422_4lane_color_bar_1024_720_200MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_4lane_color_bar_1024_720_200MHz),
		MIPI_DT_YUV422
		},
	[0][tc358743_mode_INIT3] =
		{"cb1024x720-4lane@60", tc358743_mode_INIT3,  1024, 720,
		6, 1, 4, 300,
		tc358743_setting_YUV422_4lane_color_bar_1024_720_200MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_4lane_color_bar_1024_720_200MHz),
		MIPI_DT_YUV422
		},
	[1][tc358743_mode_INIT1] =
		{"cb1280x720-2lane@30", tc358743_mode_INIT1,  1280, 720,
		12, 0, 2, 125,
		tc358743_setting_YUV422_2lane_color_bar_1280_720_125MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_2lane_color_bar_1280_720_125MHz),
		MIPI_DT_YUV422
		},
	[0][tc358743_mode_INIT1] =
		{"cb1280x720-2lane@60", tc358743_mode_INIT1,  1280, 720,
		12, 0, 2, 125,
		tc358743_setting_YUV422_2lane_color_bar_1280_720_125MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_2lane_color_bar_1280_720_125MHz),
		MIPI_DT_YUV422
		},
	[1][tc358743_mode_INIT2] =
		{"cb1280x720-4lane-125MHz@30", tc358743_mode_INIT2,  1280, 720,
		12, 0, 4, 125,
		tc358743_setting_YUV422_4lane_color_bar_1280_720_125MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_4lane_color_bar_1280_720_125MHz),
		MIPI_DT_YUV422
		},
	[0][tc358743_mode_INIT2] =
		{"cb1280x720-4lane-125MHz@60", tc358743_mode_INIT2,  1280, 720,
		12, 0, 4, 125,
		tc358743_setting_YUV422_4lane_color_bar_1280_720_125MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_4lane_color_bar_1280_720_125MHz),
		MIPI_DT_YUV422
		},
	[1][tc358743_mode_INIT5] =
		{"cb1280x720-4lane-300MHz@30", tc358743_mode_INIT5,  1280, 720,
		12, 0, 4, 300,
		tc358743_setting_YUV422_4lane_color_bar_1280_720_300MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_4lane_color_bar_1280_720_300MHz),
		MIPI_DT_YUV422
		},
	[0][tc358743_mode_INIT5] =
		{"cb1280x720-4lane-300MHz@60", tc358743_mode_INIT5,  1280, 720,
		12, 0, 4, 300,
		tc358743_setting_YUV422_4lane_color_bar_1280_720_300MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_4lane_color_bar_1280_720_300MHz),
		MIPI_DT_YUV422
		},
	[1][tc358743_mode_INIT6] =
		{"cb1920x1023@30", tc358743_mode_INIT6,  1920, 1023,
		15, 0, 4, 300,
		tc358743_setting_YUV422_4lane_color_bar_1920_1023_300MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_4lane_color_bar_1920_1023_300MHz),
		MIPI_DT_YUV422
		},
	[0][tc358743_mode_INIT6] =
		{"cb1920x1023@60", tc358743_mode_INIT6,  1920, 1023,
		15, 0, 4, 300,
		tc358743_setting_YUV422_4lane_color_bar_1920_1023_300MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_4lane_color_bar_1920_1023_300MHz),
		MIPI_DT_YUV422
		},
/* Input settings */
	[tc358743_60_fps][tc358743_mode_480P_640_480] =
		{"640x480@60", tc358743_mode_480P_640_480, 640, 480,
		1, (0x02)<<8|(0x00), 2, 125,
		tc358743_setting_YUV422_2lane_60fps_640_480_125Mhz,
		ARRAY_SIZE(tc358743_setting_YUV422_2lane_60fps_640_480_125Mhz),
		MIPI_DT_YUV422,
		},
	[1][tc358743_mode_480P_720_480] =
		{"720x480@30", tc358743_mode_480P_720_480,  720, 480,
		6, (0x02)<<8|(0x00), 2, 125,
		tc358743_setting_YUV422_2lane_60fps_720_480_125Mhz,
		ARRAY_SIZE(tc358743_setting_YUV422_2lane_60fps_720_480_125Mhz),
		MIPI_DT_YUV422,
		},
	[tc358743_60_fps][tc358743_mode_480P_720_480] =
		{"720x480@60", tc358743_mode_480P_720_480,  720, 480,
		6, (0x02)<<8|(0x00), 2, 125,
		tc358743_setting_YUV422_2lane_60fps_720_480_125Mhz,
		ARRAY_SIZE(tc358743_setting_YUV422_2lane_60fps_720_480_125Mhz),
		MIPI_DT_YUV422,
		},
	[tc358743_60_fps][tc358743_mode_1024x768] =
		{"1024x768@60", tc358743_mode_1024x768,  1024, 768,
		16, 60, 4, 125,
		tc358743_setting_YUV422_4lane_1024x768_60fps_125MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_4lane_1024x768_60fps_125MHz),
		MIPI_DT_YUV422
		},
	[1][tc358743_mode_720P_1280_720] =
		{"1280x720-2lane@30", tc358743_mode_720P_1280_720,  1280, 720,
		12, (0x3e)<<8|(0x3c), 2, 125,
		tc358743_setting_YUV422_2lane_30fps_720P_1280_720_125MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_2lane_30fps_720P_1280_720_125MHz),
		MIPI_DT_YUV422,
		},
	[0][tc358743_mode_720P_1280_720] =
		{"1280x720-2lane@60", tc358743_mode_720P_1280_720,  1280, 720,
		12, (0x3e)<<8|(0x3c), 2, 125,
		tc358743_setting_YUV422_2lane_30fps_720P_1280_720_125MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_2lane_30fps_720P_1280_720_125MHz),
		MIPI_DT_YUV422,
		},
	[1][tc358743_mode_720P_60_1280_720] =
		{"1280x720-4lane-133Mhz@30", tc358743_mode_720P_60_1280_720,  1280, 720,
		12, 0, 4, 133,
		tc358743_setting_YUV422_4lane_720P_60fps_1280_720_133Mhz,
		ARRAY_SIZE(tc358743_setting_YUV422_4lane_720P_60fps_1280_720_133Mhz),
		MIPI_DT_YUV422
		},
	[tc358743_60_fps][tc358743_mode_720P_60_1280_720] =
		{"1280x720-4lane@60", tc358743_mode_720P_60_1280_720,  1280, 720,
		12, 0, 4, 133,
		tc358743_setting_YUV422_4lane_720P_60fps_1280_720_133Mhz,
		ARRAY_SIZE(tc358743_setting_YUV422_4lane_720P_60fps_1280_720_133Mhz),
		MIPI_DT_YUV422
		},
	[1][tc358743_mode_1080P_1920_1080] =
		{"1920x1080@30", tc358743_mode_1080P_1920_1080,  1920, 1080,
		15, 0xa, 4, 300,
		tc358743_setting_YUV422_4lane_1080P_30fps_1920_1080_300MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_4lane_1080P_30fps_1920_1080_300MHz),
		MIPI_DT_YUV422
		},
	[tc358743_60_fps][tc358743_mode_1080P_1920_1080] =
		{"1920x1080@60", tc358743_mode_1080P_1920_1080,  1920, 1080,
		15, 0x0b, 4, 300,
		tc358743_setting_YUV422_4lane_1080P_60fps_1920_1080_300MHz,
		ARRAY_SIZE(tc358743_setting_YUV422_4lane_1080P_60fps_1920_1080_300MHz),
		MIPI_DT_YUV422
		},
};

static int tc358743_probe(struct i2c_client *adapter,
				const struct i2c_device_id *device_id);
static int tc358743_remove(struct i2c_client *client);

static const struct i2c_device_id tc358743_id[] = {
	{"tc358743_mipi", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, tc358743_id);

static struct i2c_driver tc358743_i2c_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = "tc358743_mipi",
		  },
	.probe  = tc358743_probe,
	.remove = tc358743_remove,
	.id_table = tc358743_id,
};

struct _reg_size
{
	u16 startaddr, endaddr;
	int size;
};

static const struct _reg_size tc358743_read_reg_size[] =
{
	{0x0000, 0x005a, 2},
	{0x0100, 0x0110, 4},
	{0x0140, 0x0150, 4},
	{0x0204, 0x0238, 4},
	{0x040c, 0x0418, 4},
	{0x044c, 0x0454, 4},
	{0x0500, 0x0518, 4},
	{0x0600, 0x06cc, 4},
	{0x7000, 0x7100, 2},
	{0x8500, 0x8bff, 1},
	{0x8c00, 0x8fff, 4},
	{0x9000, 0x90ff, 1},
	{0x9100, 0x92ff, 1},
	{0, 0, 0},
};

int get_reg_size(u16 reg, int len)
{
	const struct _reg_size *p = tc358743_read_reg_size;
	int size;

#if 0	//later #ifndef DEBUG
	if (len)
		return len;
#endif
	while (p->size) {
		if ((p->startaddr <= reg) && (reg <= p->endaddr)) {
			size = p->size;
			if (len && (size != len)) {
				pr_err("%s:reg len error:reg=%x %d instead of %d\n",
						__func__, reg, len, size);
				return 0;
			}
			if (reg % size) {
				pr_err("%s:cannot read from the middle of a register, reg(%x) size(%d)\n",
						__func__, reg, size);
				return 0;
			}
			return size;
		}
		p++;
	}
	pr_err("%s:reg=%x size is not defined\n",__func__, reg);
	return 0;
}

static s32 tc358743_read_reg(struct sensor_data *sensor, u16 reg, void *rxbuf)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msgs[2];
	u8 txbuf[2];
	int ret;
	int size = get_reg_size(reg, 0);

	if (!size)
		return -EINVAL;

	txbuf[0] = reg >> 8;
	txbuf[1] = reg & 0xff;
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = txbuf;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = size;
	msgs[1].buf = rxbuf;

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0) {
		pr_err("%s:reg=%x ret=%d\n", __func__, reg, ret);
		return ret;
	}
//	pr_debug("%s:reg=%x,val=%x\n", __func__, reg, ((char *)rxbuf)[0]);
	return 0;
}

static s32 tc358743_read_reg_val(struct sensor_data *sensor, u16 reg)
{
	u32 val = 0;
	tc358743_read_reg(sensor, reg, &val);
	return val;
}

static s32 tc358743_read_reg_val16(struct sensor_data *sensor, u16 reg)
{
#if 0
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msgs[3];
	u8 txbuf[4];
	u8 rxbuf1[4];
	u8 rxbuf2[4];
	int ret;
	int size = get_reg_size(reg, 0);

	if (size != 1)
		return -EINVAL;

	txbuf[0] = reg >> 8;
	txbuf[1] = reg & 0xff;
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = txbuf;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = size;
	msgs[1].buf = rxbuf1;

	msgs[2].addr = client->addr;
	msgs[2].flags = I2C_M_RD;
	msgs[2].len = size;
	msgs[2].buf = rxbuf2;

	ret = i2c_transfer(client->adapter, msgs, 3);
	if (ret < 0) {
		pr_err("%s:reg=%x ret=%d\n", __func__, reg, ret);
		return ret;
	}
//	pr_debug("%s:reg=%x,val=%x\n", __func__, reg, ((char *)rxbuf)[0]);
	return rxbuf1[0] | (rxbuf2[0] << 8);
#else
	u32 val1 = 0;
	u32 val2 = 0;
	tc358743_read_reg(sensor, reg, &val1);
	tc358743_read_reg(sensor, reg+1, &val2);
	return val1 | (val2 << 8);

#endif
}

static s32 tc358743_write_reg(struct sensor_data *sensor, u16 reg, u32 val, int len)
{
	int ret;
	int i = 0;
	u32 data = val;
	u8 au8Buf[6] = {0};
	int size = get_reg_size(reg, len);

	if (!size)
		return -EINVAL;

	au8Buf[i++] = reg >> 8;
	au8Buf[i++] = reg & 0xff;
	while (size-- > 0) {
		au8Buf[i++] = (u8)data;
		data >>= 8;
	}

	ret = i2c_master_send(sensor->i2c_client, au8Buf, i);
	if (ret < 0) {
		pr_err("%s:write reg error(%d):reg=%x,val=%x\n",
				__func__, ret, reg, val);
		return ret;
	}
	if ((reg < 0x7000) || (reg >= 0x7100)) {
		if (0) pr_debug("%s:reg=%x,val=%x 8543=%02x\n", __func__, reg, val, tc358743_read_reg_val(sensor, 0x8543));
	}
	return 0;
}

static void tc358743_software_reset(struct sensor_data *sensor)
{
	int freq = sensor->mclk / 10000;
	tc358743_write_reg(sensor, 0x7080, 0, 2);
	tc358743_write_reg(sensor, 0x0002, 0x0f00, 2);
	msleep(100);
	tc358743_write_reg(sensor, 0x0002, 0x0000, 2);
	msleep(1000);
	tc358743_write_reg(sensor, 0x0004, 0x0004, 2);	/* autoinc */
	pr_debug("%s:freq=%d\n", __func__, freq);
	tc358743_write_reg(sensor, 0x8540, freq, 1);
	tc358743_write_reg(sensor, 0x8541, freq >> 8, 1);
}

static void tc358743_enable_edid(struct sensor_data *sensor)
{
	pr_debug("Activate EDID\n");
	// EDID
	tc358743_write_reg(sensor, 0x85c7, 0x01, 1);		// EDID MODE REGISTER: nternal EDID-RAM & DDC2B mode
	tc358743_write_reg(sensor, 0x85ca, 0x00, 1);
	tc358743_write_reg(sensor, 0x85cb, 0x01, 1);		// 0x85cb:0x85ca - EDID Length = 0x01:00 (Size = 0x100 = 256)
	tc358743_write_reg(sensor, 0x8543, 0x36, 1);		// DDC CONTROL: DDC_ACK output terminal H active, DDC5V_active detect delay 200ms
	tc358743_write_reg(sensor, 0x854a, 0x01, 1);		// mark init done
	if (0) pr_debug("%s: c7=%02x ca=%02x cb=%02x 43=%02x 4a=%02x\n", __func__,
		tc358743_read_reg_val(sensor, 0x85c7),
		tc358743_read_reg_val(sensor, 0x85ca),
		tc358743_read_reg_val(sensor, 0x85cb),
		tc358743_read_reg_val(sensor, 0x8543),
		tc358743_read_reg_val(sensor, 0x854a));
}

static int tc358743_write_edid(struct sensor_data *sensor, const u8 *edid, int len)
{
	int i = 0, off = 0;
	u8 au8Buf[16+2] = {0};
	int size = 0;
	int checksum = 0;
	u16 reg;

	reg = 0x8C00;
	off = 0;
	size = ARRAY_SIZE(au8Buf) - 2;
	pr_debug("Write EDID: %d (%d)\n", len, size);
	while (len > 0) {
		i = 0;
		au8Buf[i++] = (reg >> 8) & 0xff;
		au8Buf[i++] = reg & 0xff;
		if (size > len)
			size = len;
		while (i < size + 2) {
			u8 byte = edid[off++];
			if ((off & 0x7f) == 0) {
				checksum &= 0xff;
				if (checksum != byte) {
					pr_info("%schecksum=%x, byte=%x\n", __func__, checksum, byte);
					byte = checksum;
					checksum = 0;
				}
			} else {
				checksum -= byte;
			}
			au8Buf[i++] = byte;
		}

		if (i2c_master_send(sensor->i2c_client, au8Buf, i) < 0) {
			pr_err("%s:write reg error:reg=%x,val=%x\n",
				__func__, reg, off);
			return -1;
		}
		len -= (u8)size;
		reg += (u16)size;
	}
	tc358743_enable_edid(sensor);
	return 0;
}

static s32 power_control(struct tc_data *td, int on)
{
	struct sensor_data *sensor = &td->sensor;
	int i;
	int ret = 0;

	pr_debug("%s: %d\n", __func__, on);
	if (sensor->on != on) {
		if (on) {
			for (i = 0; i < REGULATOR_CNT; i++) {
				if (td->regulator[i]) {
					ret = regulator_enable(td->regulator[i]);
					if (ret) {
						pr_err("%s:regulator_enable failed(%d)\n",
							__func__, ret);
						on = 0;	/* power all off */
						break;
					}
				}
			}
		}
		tc_standby(td, on ? 0 : 1);
		sensor->on = on;
		if (!on) {
			for (i = REGULATOR_CNT - 1; i >= 0; i--) {
				if (td->regulator[i])
					regulator_disable(td->regulator[i]);
			}
		}
	}
	return ret;
}

static int tc358743_toggle_hpd(struct sensor_data *sensor, int active)
{
	int ret = 0;
	if (active) {
		ret += tc358743_write_reg(sensor, 0x8544, 0x00, 1);
		mdelay(500);
		ret += tc358743_write_reg(sensor, 0x8544, 0x10, 1);
	} else {
		ret += tc358743_write_reg(sensor, 0x8544, 0x10, 1);
		mdelay(500);
		ret += tc358743_write_reg(sensor, 0x8544, 0x00, 1);
	}
	return ret;
}

static int get_format_index(enum tc358743_frame_rate frame_rate, enum tc358743_mode mode)
{
	int ifmt;
	u32 flags = tc358743_mode_info_data[frame_rate][mode].flags;

	for (ifmt = 0; ifmt < ARRAY_SIZE(tc358743_formats); ifmt++) {
		if (flags == tc358743_formats[ifmt].flags)
			return ifmt;
	}
	return -1;
}

static int get_pixelformat(enum tc358743_frame_rate frame_rate, enum tc358743_mode mode)
{
	int ifmt = get_format_index(frame_rate, mode);

	if (ifmt < 0) {
		pr_debug("%s: unsupported format, %d, %d\n", __func__, frame_rate, mode);
		return 0;
	}
	pr_debug("%s: %s (%x, %x)\n", __func__,
		tc358743_formats[ifmt].description,
		tc358743_formats[ifmt].pixelformat,
		tc358743_formats[ifmt].flags);
	return tc358743_formats[ifmt].pixelformat;
}

int set_frame_rate_mode(struct tc_data *td,
		enum tc358743_frame_rate frame_rate, enum tc358743_mode mode)
{
	struct sensor_data *sensor = &td->sensor;
	const struct reg_value *pModeSetting = NULL;
	s32 i = 0;
	s32 iModeSettingArySize = 0;
	register u32 RepeateLines = 0;
	register int RepeateTimes = 0;
	register u32 Delay_ms = 0;
	register u16 RegAddr = 0;
	register u32 Mask = 0;
	register u32 Val = 0;
	u8  Length;
	int retval = 0;

	pModeSetting =
		tc358743_mode_info_data[frame_rate][mode].init_data_ptr;
	iModeSettingArySize =
		tc358743_mode_info_data[frame_rate][mode].init_data_size;

	sensor->pix.pixelformat = get_pixelformat(frame_rate, mode);
	sensor->pix.width =
		tc358743_mode_info_data[frame_rate][mode].width;
	sensor->pix.height =
		tc358743_mode_info_data[frame_rate][mode].height;
	pr_debug("%s: Set %d regs from %p for frs %d mode %d with width %d height %d\n", __func__,
		iModeSettingArySize,
		pModeSetting,
		frame_rate,
		mode,
		sensor->pix.width,
		sensor->pix.height);
	for (i = 0; i < iModeSettingArySize; ++i) {
		pModeSetting = tc358743_mode_info_data[frame_rate][mode].init_data_ptr + i;

		Delay_ms = pModeSetting->u32Delay_ms & (0xffff);
		RegAddr = pModeSetting->u16RegAddr;
		Val = pModeSetting->u32Val;
		Mask = pModeSetting->u32Mask;
		Length = pModeSetting->u8Length;
		if (Mask) {
			u32 RegVal = 0;

			retval = tc358743_read_reg(sensor, RegAddr, &RegVal);
			if (retval < 0) {
				pr_err("%s: read failed, reg=0x%x\n", __func__, RegAddr);
				return retval;
			}
			RegVal &= ~(u8)Mask;
			Val &= Mask;
			Val |= RegVal;
		}

		retval = tc358743_write_reg(sensor, RegAddr, Val, Length);
		if (retval < 0) {
			pr_err("%s: write failed, reg=0x%x\n", __func__, RegAddr);
			return retval;
		}
		if (Delay_ms)
			msleep(Delay_ms);

		if (0 != ((pModeSetting->u32Delay_ms>>16) & (0xff))) {
			if (!RepeateTimes) {
				RepeateTimes = (pModeSetting->u32Delay_ms>>16) & (0xff);
				RepeateLines = (pModeSetting->u32Delay_ms>>24) & (0xff);
			}
			if (--RepeateTimes > 0) {
				i -= RepeateLines;
			}
		}
	}
	tc358743_enable_edid(sensor);
	if (!td->edid_initialized) {
		retval = tc358743_write_edid(sensor, cHDMIEDID, ARRAY_SIZE(cHDMIEDID));
		if (retval)
			pr_err("%s: Fail to write EDID(%d) to tc35874!\n", __func__, retval);
		else
			td->edid_initialized = 1;
	}

	return retval;
}

void mipi_csi2_swreset(struct mipi_csi2_info *info);
#include "../../../../mxc/mipi/mxc_mipi_csi2.h"

int mipi_reset(void *mipi_csi2_info,
		enum tc358743_frame_rate frame_rate,
		enum tc358743_mode mode)
{
	int lanes = tc358743_mode_info_data[frame_rate][mode].lanes;

	if (!lanes)
		lanes = 4;

	pr_debug("%s: mipi_csi2_info:\n"
	"mipi_en:       %d\n"
	"datatype:      %d\n"
	"dphy_clk:      %p\n"
	"pixel_clk:     %p\n"
	"mipi_csi2_base:%p\n"
	"pdev:          %p\n"
	, __func__,
	((struct mipi_csi2_info *)mipi_csi2_info)->mipi_en,
	((struct mipi_csi2_info *)mipi_csi2_info)->datatype,
	((struct mipi_csi2_info *)mipi_csi2_info)->dphy_clk,
	((struct mipi_csi2_info *)mipi_csi2_info)->pixel_clk,
	((struct mipi_csi2_info *)mipi_csi2_info)->mipi_csi2_base,
	((struct mipi_csi2_info *)mipi_csi2_info)->pdev
	);
	if (mipi_csi2_get_status(mipi_csi2_info)) {
		mipi_csi2_disable(mipi_csi2_info);
		msleep(1);
	}
	mipi_csi2_enable(mipi_csi2_info);

	if (!mipi_csi2_get_status(mipi_csi2_info)) {
		pr_err("Can not enable mipi csi2 driver!\n");
		return -1;
	}
	lanes = mipi_csi2_set_lanes(mipi_csi2_info, lanes);
	pr_debug("Now Using %d lanes\n", lanes);

	mipi_csi2_reset(mipi_csi2_info);
	mipi_csi2_set_datatype(mipi_csi2_info, tc358743_mode_info_data[frame_rate][mode].flags);
	return 0;
}

int mipi_wait(void *mipi_csi2_info)
{
	unsigned i = 0;
	unsigned j;
	u32 mipi_reg;
	u32 mipi_reg_test[10];

	/* wait for mipi sensor ready */
	for (;;) {
		mipi_reg = mipi_csi2_dphy_status(mipi_csi2_info);
		mipi_reg_test[i++] = mipi_reg;
		if (mipi_reg != 0x200)
			break;
		if (i >= 10) {
			pr_err("mipi csi2 can not receive sensor clk!\n");
			return -1;
		}
		msleep(10);
	}

	for (j = 0; j < i; j++) {
		pr_debug("%d  mipi csi2 dphy status %x\n", j, mipi_reg_test[j]);
	}

	i = 0;

	/* wait for mipi stable */
	for (;;) {
		mipi_reg = mipi_csi2_get_error1(mipi_csi2_info);
		mipi_reg_test[i++] = mipi_reg;
		if (!mipi_reg)
			break;
		if (i >= 10) {
			pr_err("mipi csi2 can not reveive data correctly!\n");
			return -1;
		}
		msleep(10);
	}

	for (j = 0; j < i; j++) {
		pr_debug("%d  mipi csi2 err1 %x\n", j, mipi_reg_test[j]);
	}
	return 0;
}

static int tc358743_init_mode(struct tc_data *td,
		enum tc358743_frame_rate frame_rate,
		enum tc358743_mode mode)
{
	struct sensor_data *sensor = &td->sensor;
	int retval = 0;
	void *mipi_csi2_info;

	pr_debug("%s rate: %d mode: %d\n", __func__, frame_rate, mode);
	if ((mode >= tc358743_mode_MAX || mode < 0)
		&& (mode != tc358743_mode_INIT)) {
		pr_debug("%s Wrong tc358743 mode detected! %d. Set mode 0\n", __func__, mode);
		mode = 0;
	}
	/* initial mipi dphy */
	tc358743_toggle_hpd(sensor, 0);
	tc358743_software_reset(sensor);

	mipi_csi2_info = mipi_csi2_get_info();
	pr_debug("%s rate: %d mode: %d, info %p\n", __func__, frame_rate, mode, mipi_csi2_info);

	if (!mipi_csi2_info) {
		pr_err("Fail to get mipi_csi2_info!\n");
		return -1;
	}
	retval = mipi_reset(mipi_csi2_info, frame_rate, tc358743_mode_INIT);
	if (retval)
		return retval;
	retval = set_frame_rate_mode(td, frame_rate, tc358743_mode_INIT);
	if (retval)
		return retval;
	retval = mipi_wait(mipi_csi2_info);

	if (mode != tc358743_mode_INIT) {
		tc358743_software_reset(sensor);
		retval = mipi_reset(mipi_csi2_info, frame_rate, mode);
		if (retval)
			return retval;
		retval = set_frame_rate_mode(td, frame_rate, mode);
		if (retval)
			return retval;
		retval = mipi_wait(mipi_csi2_info);
	}
	if (td->hpd_active)
		tc358743_toggle_hpd(sensor, td->hpd_active);
	return retval;
}

static int tc358743_minit(struct tc_data *td)
{
	struct sensor_data *sensor = &td->sensor;
	int ret;
	enum tc358743_frame_rate frame_rate = tc358743_60_fps;
	u32 tgt_fps = sensor->streamcap.timeperframe.denominator /
			sensor->streamcap.timeperframe.numerator;

	if (tgt_fps == 60)
		frame_rate = tc358743_60_fps;
	else if (tgt_fps == 30)
		frame_rate = tc358743_30_fps;

	pr_debug("%s: capture mode: %d fps: %d\n", __func__,
		sensor->streamcap.capturemode, tgt_fps);

	ret = tc358743_init_mode(td, frame_rate, sensor->streamcap.capturemode);
	if (ret)
		pr_err("%s: Fail to init tc35874!\n", __func__);
	return ret;
}

static int tc358743_reset(struct tc_data *td)
{
	int loop = 0;
	int ret;

	det_work_enable(td, 0);
	for (;;) {
		pr_debug("%s: RESET\n", __func__);
		power_control(td, 0);
		mdelay(100);
		power_control(td, 1);
		mdelay(1000);

		ret = tc358743_minit(td);
		if (!ret)
			break;
		if (loop++ >= 3) {
			pr_err("%s:failed(%d)\n", __func__, ret);
			break;
		}
	}
	det_work_enable(td, 1);
	return ret;
}

/* --------------- IOCTL functions from v4l2_int_ioctl_desc --------------- */

static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	struct tc_data *td = s->priv;
	struct sensor_data *sensor = &td->sensor;

	pr_debug("%s\n", __func__);

	memset(p, 0, sizeof(*p));
	p->u.bt656.clock_curr = TC358743_XCLK_MIN; //sensor->mclk;
	pr_debug("%s: clock_curr=mclk=%d\n", __func__, sensor->mclk);
	p->if_type = V4L2_IF_TYPE_BT656;
	p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
	p->u.bt656.clock_min = TC358743_XCLK_MIN;
	p->u.bt656.clock_max = TC358743_XCLK_MAX;

	return 0;
}

/*!
 * ioctl_s_power - V4L2 sensor interface handler for VIDIOC_S_POWER ioctl
 * @s: pointer to standard V4L2 device structure
 * @on: indicates power mode (on or off)
 *
 * Turns the power on or off, depending on the value of on and returns the
 * appropriate error code.
 */
static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
	struct tc_data *td = s->priv;
	int ret;

	mutex_lock(&td->access_lock);
	if (on && !td->mode) {
		ret = tc358743_reset(td);
	} else {
		ret = power_control(td, on);
	}
	mutex_unlock(&td->access_lock);
	return ret;
}

/*!
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct tc_data *td = s->priv;
	struct sensor_data *sensor = &td->sensor;
	struct v4l2_captureparm *cparm = &a->parm.capture;
	int ret = 0;

	pr_debug("%s type: %x\n", __func__, a->type);
	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->streamcap.capability;
		cparm->timeperframe = sensor->streamcap.timeperframe;
		cparm->capturemode = sensor->streamcap.capturemode;
		cparm->extendedmode = sensor->streamcap.extendedmode;
		ret = 0;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}
	pr_debug("%s done %d\n", __func__, ret);
	return ret;
}

/*!
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct tc_data *td = s->priv;
	struct sensor_data *sensor = &td->sensor;
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	u32 tgt_fps;	/* target frames per secound */
	enum tc358743_frame_rate frame_rate = tc358743_60_fps, frame_rate_now = tc358743_60_fps;
	enum tc358743_mode mode;
	int ret = 0;

	pr_debug("%s\n", __func__);
	mutex_lock(&td->access_lock);
	det_work_enable(td, 0);
	/* Make sure power on */
	power_control(td, 1);

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		/* Check that the new frame rate is allowed. */
		if ((timeperframe->numerator == 0) ||
		    (timeperframe->denominator == 0)) {
			timeperframe->denominator = DEFAULT_FPS;
			timeperframe->numerator = 1;
		}

		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps > MAX_FPS) {
			timeperframe->denominator = MAX_FPS;
			timeperframe->numerator = 1;
		} else if (tgt_fps < MIN_FPS) {
			timeperframe->denominator = MIN_FPS;
			timeperframe->numerator = 1;
		}

		/* Actual frame rate we use */
		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps == 60)
			frame_rate = tc358743_60_fps;
		else if (tgt_fps == 30)
			frame_rate = tc358743_30_fps;
		else {
			pr_err(" The camera frame rate is not supported!\n");
			ret = -EINVAL;
			break;
		}

		if ((u32)a->parm.capture.capturemode >= tc358743_mode_MAX) {
			a->parm.capture.capturemode = 0;
			pr_debug("%s: Force mode: %d \n", __func__,
				(u32)a->parm.capture.capturemode);
		}

		tgt_fps = sensor->streamcap.timeperframe.denominator /
			  sensor->streamcap.timeperframe.numerator;

		if (tgt_fps == 60)
			frame_rate_now = tc358743_60_fps;
		else if (tgt_fps == 30)
			frame_rate_now = tc358743_30_fps;

		mode = td->mode;
		if (IS_COLORBAR(mode)) {
			mode = (u32)a->parm.capture.capturemode;
		} else {
			a->parm.capture.capturemode = mode;
			frame_rate = td->fps;
			timeperframe->denominator = (frame_rate == tc358743_60_fps) ? 60 : 30;
			timeperframe->numerator = 1;
		}

		if (frame_rate_now != frame_rate ||
		   sensor->streamcap.capturemode != mode ||
		   sensor->streamcap.extendedmode != (u32)a->parm.capture.extendedmode) {

			if (mode != tc358743_mode_INIT) {
				sensor->streamcap.capturemode = mode;
				sensor->streamcap.timeperframe = *timeperframe;
				sensor->streamcap.extendedmode =
						(u32)a->parm.capture.extendedmode;
				pr_debug("%s: capture mode: %d\n", __func__,
					mode);
				ret = tc358743_init_mode(td, frame_rate, mode);
			} else {
				a->parm.capture.capturemode = sensor->streamcap.capturemode;
				*timeperframe = sensor->streamcap.timeperframe;
				a->parm.capture.extendedmode = sensor->streamcap.extendedmode;
			}
		} else {
			pr_debug("%s: Keep current settings\n", __func__);
		}
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		pr_debug("   type is not " \
			"V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
			a->type);
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	det_work_enable(td, 1);
	mutex_unlock(&td->access_lock);
	return ret;
}

/*!
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	struct tc_data *td = s->priv;
	struct sensor_data *sensor = &td->sensor;
	int ret = 0;

	pr_debug("%s\n", __func__);
	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		vc->value = sensor->brightness;
		break;
	case V4L2_CID_HUE:
		vc->value = sensor->hue;
		break;
	case V4L2_CID_CONTRAST:
		vc->value = sensor->contrast;
		break;
	case V4L2_CID_SATURATION:
		vc->value = sensor->saturation;
		break;
	case V4L2_CID_RED_BALANCE:
		vc->value = sensor->red;
		break;
	case V4L2_CID_BLUE_BALANCE:
		vc->value = sensor->blue;
		break;
	case V4L2_CID_EXPOSURE:
		vc->value = sensor->ae_mode;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/*!
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int retval = 0;

	pr_debug("In tc358743:ioctl_s_ctrl %d\n",
		 vc->id);

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		break;
	case V4L2_CID_CONTRAST:
		break;
	case V4L2_CID_SATURATION:
		break;
	case V4L2_CID_HUE:
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		break;
	case V4L2_CID_RED_BALANCE:
		break;
	case V4L2_CID_BLUE_BALANCE:
		break;
	case V4L2_CID_GAMMA:
		break;
	case V4L2_CID_EXPOSURE:
		break;
	case V4L2_CID_AUTOGAIN:
		break;
	case V4L2_CID_GAIN:
		break;
	case V4L2_CID_HFLIP:
		break;
	case V4L2_CID_VFLIP:
		break;
	default:
		retval = -EPERM;
		break;
	}

	return retval;
}

/*!
 * ioctl_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
				 struct v4l2_frmsizeenum *fsize)
{
	struct tc_data *td = s->priv;
	enum tc358743_mode query_mode= fsize->index;
	enum tc358743_mode mode = td->mode;

	if (IS_COLORBAR(mode)) {
		if (query_mode > MAX_COLORBAR)
			return -EINVAL;
		mode = query_mode;
	} else {
		if (query_mode)
			return -EINVAL;
	}
	pr_debug("%s, mode: %d\n", __func__, mode);

	fsize->pixel_format = get_pixelformat(0, mode);
	fsize->discrete.width =
			    tc358743_mode_info_data[0][mode].width;
	fsize->discrete.height =
			    tc358743_mode_info_data[0][mode].height;
	pr_debug("%s %d:%d format: %x\n", __func__, fsize->discrete.width, fsize->discrete.height, fsize->pixel_format);
	return 0;
}

/*!
 * ioctl_g_chip_ident - V4L2 sensor interface handler for
 *			VIDIOC_DBG_G_CHIP_IDENT ioctl
 * @s: pointer to standard V4L2 device structure
 * @id: pointer to int
 *
 * Return 0.
 */
static int ioctl_g_chip_ident(struct v4l2_int_device *s, int *id)
{
	((struct v4l2_dbg_chip_ident *)id)->match.type =
					V4L2_CHIP_MATCH_I2C_DRIVER;
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name,
		"tc358743_mipi");

	return 0;
}

/*!
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	pr_debug("%s\n", __func__);
	return 0;
}

/*!
 * ioctl_enum_fmt_cap - V4L2 sensor interface handler for VIDIOC_ENUM_FMT
 * @s: pointer to standard V4L2 device structure
 * @fmt: pointer to standard V4L2 fmt description structure
 *
 * Return 0.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
			      struct v4l2_fmtdesc *fmt)
{
	struct tc_data *td = s->priv;
	struct sensor_data *sensor = &td->sensor;
	int index = fmt->index;

	if (!index)
		index = sensor->streamcap.capturemode;
	pr_debug("%s, INDEX: %d\n", __func__, index);
	if (index >= tc358743_mode_MAX)
		return -EINVAL;

	fmt->pixelformat = get_pixelformat(0, index);

	pr_debug("%s: format: %x\n", __func__, fmt->pixelformat);
	return 0;
}

static int ioctl_try_fmt_cap(struct v4l2_int_device *s,
			     struct v4l2_format *f)
{
	struct tc_data *td = s->priv;
	struct sensor_data *sensor = &td->sensor;
	u32 tgt_fps;	/* target frames per secound */
	enum tc358743_frame_rate frame_rate;
//	enum image_size isize;
	int ret = 0;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	int mode;

	pr_debug("%s\n", __func__);

	mutex_lock(&td->access_lock);
	tgt_fps = sensor->streamcap.timeperframe.denominator /
		  sensor->streamcap.timeperframe.numerator;

	if (tgt_fps == 60) {
		frame_rate = tc358743_60_fps;
	} else if (tgt_fps == 30) {
		frame_rate = tc358743_30_fps;
	} else {
		pr_debug("%s: %d fps (%d,%d) is not supported\n", __func__, tgt_fps, sensor->streamcap.timeperframe.denominator,sensor->streamcap.timeperframe.numerator);
		ret = -EINVAL;
		goto out;
	}
	mode = sensor->streamcap.capturemode;
	sensor->pix.pixelformat = get_pixelformat(frame_rate, mode);
	sensor->pix.width = pix->width = tc358743_mode_info_data[frame_rate][mode].width;
	sensor->pix.height = pix->height = tc358743_mode_info_data[frame_rate][mode].height;
	pr_debug("%s: %dx%d\n", __func__, sensor->pix.width, sensor->pix.height);

	pix->pixelformat = sensor->pix.pixelformat;
	pix->field = V4L2_FIELD_NONE;
	pix->bytesperline = pix->width * 4;
	pix->sizeimage = pix->bytesperline * pix->height;
	pix->priv = 0;

	switch (pix->pixelformat) {
	case V4L2_PIX_FMT_UYVY:
	default:
		pix->colorspace = V4L2_COLORSPACE_SRGB;
		break;
	}

	{
		pr_debug("SYS_STATUS: 0x%x\n", tc358743_read_reg_val(sensor, 0x8520));
		pr_debug("VI_STATUS0: 0x%x\n", tc358743_read_reg_val(sensor, 0x8521));
		pr_debug("VI_STATUS1: 0x%x\n", tc358743_read_reg_val(sensor, 0x8522));
		pr_debug("VI_STATUS2: 0x%x\n", tc358743_read_reg_val(sensor, 0x8525));
		pr_debug("VI_STATUS3: 0x%x\n", tc358743_read_reg_val(sensor, 0x8528));
		pr_debug("%s %d:%d format: %x\n", __func__, pix->width, pix->height, pix->pixelformat);
	}
out:
	mutex_unlock(&td->access_lock);
	return ret;
}

/*!
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct tc_data *td = s->priv;
	struct sensor_data *sensor = &td->sensor;
	int mode = sensor->streamcap.capturemode;

	sensor->pix.pixelformat = get_pixelformat(0, mode);
	sensor->pix.width = tc358743_mode_info_data[0][mode].width;
	sensor->pix.height = tc358743_mode_info_data[0][mode].height;

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		f->fmt.pix = sensor->pix;
		pr_debug("%s: %dx%d\n", __func__, sensor->pix.width, sensor->pix.height);
		break;

	case V4L2_BUF_TYPE_SENSOR:
		pr_debug("%s: left=%d, top=%d, %dx%d\n", __func__,
			sensor->spix.left, sensor->spix.top,
			sensor->spix.swidth, sensor->spix.sheight);
		f->fmt.spix = sensor->spix;
		break;

	case V4L2_BUF_TYPE_PRIVATE:
		pr_debug("%s: private\n", __func__);
		break;

	default:
		f->fmt.pix = sensor->pix;
		pr_debug("%s: type=%d, %dx%d\n", __func__, f->type, sensor->pix.width, sensor->pix.height);
		break;
	}
	return 0;
}

/*!
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
	struct tc_data *td = s->priv;

	if (td->det_changed) {
		mutex_lock(&td->access_lock);
		td->det_changed = 0;
		pr_debug("%s\n", __func__);
		tc358743_minit(td);
		mutex_unlock(&td->access_lock);
	}
	pr_debug("%s\n", __func__);
	return 0;
}

/*!
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the device when slave detaches to the master.
 */
static int ioctl_dev_exit(struct v4l2_int_device *s)
{
	void *mipi_csi2_info;

	mipi_csi2_info = mipi_csi2_get_info();

	/* disable mipi csi2 */
	if (mipi_csi2_info)
		if (mipi_csi2_get_status(mipi_csi2_info))
			mipi_csi2_disable(mipi_csi2_info);

	return 0;
}

/*!
 * This structure defines all the ioctls for this module and links them to the
 * enumeration.
 */
static struct v4l2_int_ioctl_desc tc358743_ioctl_desc[] = {
	{vidioc_int_dev_init_num, (v4l2_int_ioctl_func*) ioctl_dev_init},
	{vidioc_int_dev_exit_num, ioctl_dev_exit},
	{vidioc_int_s_power_num, (v4l2_int_ioctl_func*) ioctl_s_power},
	{vidioc_int_g_ifparm_num, (v4l2_int_ioctl_func*) ioctl_g_ifparm},
	{vidioc_int_init_num, (v4l2_int_ioctl_func*) ioctl_init},
	{vidioc_int_enum_fmt_cap_num,
				(v4l2_int_ioctl_func *) ioctl_enum_fmt_cap},
	{vidioc_int_try_fmt_cap_num,
				(v4l2_int_ioctl_func *)ioctl_try_fmt_cap},
	{vidioc_int_g_fmt_cap_num, (v4l2_int_ioctl_func *) ioctl_g_fmt_cap},
	{vidioc_int_g_parm_num, (v4l2_int_ioctl_func *) ioctl_g_parm},
	{vidioc_int_s_parm_num, (v4l2_int_ioctl_func *) ioctl_s_parm},
	{vidioc_int_g_ctrl_num, (v4l2_int_ioctl_func *) ioctl_g_ctrl},
	{vidioc_int_s_ctrl_num, (v4l2_int_ioctl_func *) ioctl_s_ctrl},
	{vidioc_int_enum_framesizes_num,
				(v4l2_int_ioctl_func *) ioctl_enum_framesizes},
	{vidioc_int_g_chip_ident_num,
				(v4l2_int_ioctl_func *) ioctl_g_chip_ident},
};

static struct v4l2_int_slave tc358743_slave = {
	.ioctls = tc358743_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(tc358743_ioctl_desc),
};

static struct v4l2_int_device tc358743_int_device = {
	.module = THIS_MODULE,
	.name = "tc358743",
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &tc358743_slave,
	},
};


#ifdef CONFIG_TC358743_AUDIO
struct imx_ssi {
	struct platform_device *ac97_dev;

	struct snd_soc_dai *imx_ac97;
	struct clk *clk;
	void __iomem *base;
	int irq;
	int fiq_enable;
	unsigned int offset;

	unsigned int flags;

	void (*ac97_reset) (struct snd_ac97 *ac97);
	void (*ac97_warm_reset)(struct snd_ac97 *ac97);

	struct imx_pcm_dma_params	dma_params_rx;
	struct imx_pcm_dma_params	dma_params_tx;

	int enabled;

	struct platform_device *soc_platform_pdev;
	struct platform_device *soc_platform_pdev_fiq;
};
#define SSI_SCR			0x10
#define SSI_SRCR		0x20
#define SSI_STCCR		0x24
#define SSI_SRCCR		0x28
#define SSI_SCR_I2S_MODE_NORM	(0 << 5)
#define SSI_SCR_I2S_MODE_MSTR	(1 << 5)
#define SSI_SCR_I2S_MODE_SLAVE	(2 << 5)
#define SSI_I2S_MODE_MASK	(3 << 5)
#define SSI_SCR_SYN		(1 << 4)
#define SSI_SRCR_RSHFD		(1 << 4)
#define SSI_SRCR_RSCKP		(1 << 3)
#define SSI_SRCR_RFSI		(1 << 2)
#define SSI_SRCR_REFS		(1 << 0)
#define SSI_STCCR_WL(x)		((((x) - 2) >> 1) << 13)
#define SSI_STCCR_WL_MASK	(0xf << 13)
#define SSI_SRCCR_WL(x)		((((x) - 2) >> 1) << 13)
#define SSI_SRCCR_WL_MASK	(0xf << 13)
/* Audio setup */

static int imxpac_tc358743_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
				SND_SOC_DAIFMT_NB_IF |
				SND_SOC_DAIFMT_CBM_CFM);
	if (ret) {
		pr_err("%s: failed set cpu dai format\n", __func__);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
				SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBM_CFM);
	if (ret) {
		pr_err("%s: failed set codec dai format\n", __func__);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, 0,
				     CODEC_CLOCK, SND_SOC_CLOCK_OUT);
	if (ret) {
		pr_err("%s: failed setting codec sysclk\n", __func__);
		return ret;
	}
	snd_soc_dai_set_tdm_slot(cpu_dai, 0xffffffc, 0xffffffc, 2, 0);

	ret = snd_soc_dai_set_sysclk(cpu_dai, IMX_SSP_SYS_CLK, 0,
				SND_SOC_CLOCK_IN);
	if (ret) {
		pr_err("can't set CPU system clock IMX_SSP_SYS_CLK\n");
		return ret;
	}
#if 1
// clear SSI_SRCR_RXBIT0 and SSI_SRCR_RSHFD in order to push Right-justified MSB data fro 
	{
		struct imx_ssi *ssi = snd_soc_dai_get_drvdata(cpu_dai);
		u32 scr = 0, srcr = 0, stccr = 0, srccr = 0;

		pr_debug("%s: base %p\n", __func__, (void *)ssi->base);
		scr = readl(ssi->base + SSI_SCR);
		pr_debug("%s: SSI_SCR before:   %p\n", __func__, (void *)scr);
		writel(scr, ssi->base + SSI_SCR);
		pr_debug("%s: SSI_SCR after:    %p\n", __func__, (void *)scr);

		srcr = readl(ssi->base + SSI_SRCR);
		pr_debug("%s: SSI_SRCR before:  %p\n", __func__, (void *)srcr);
		writel(srcr, ssi->base + SSI_SRCR);
		pr_debug("%s: SSI_SRCR after:   %p\n", __func__, (void *)srcr);

		stccr = readl(ssi->base + SSI_STCCR);
		pr_debug("%s: SSI_STCCR before: %p\n", __func__, (void *)stccr);
		stccr &= ~SSI_STCCR_WL_MASK;
		stccr |= SSI_STCCR_WL(16);
		writel(stccr, ssi->base + SSI_STCCR);
		pr_debug("%s: SSI_STCCR after:  %p\n", __func__, (void *)stccr);

		srccr = readl(ssi->base + SSI_SRCCR);
		pr_debug("%s: SSI_SRCCR before: %p\n", __func__, (void *)srccr);
		srccr &= ~SSI_SRCCR_WL_MASK;
		srccr |= SSI_SRCCR_WL(16);
		writel(srccr, ssi->base + SSI_SRCCR);
		pr_debug("%s: SSI_SRCCR after:  %p\n", __func__, (void *)srccr);
	}
#endif
	return 0;
}




static struct snd_soc_ops imxpac_tc358743_snd_ops = {
	.hw_params	= imxpac_tc358743_hw_params,
};

static struct snd_soc_dai_link imxpac_tc358743_dai = {
	.name		= "tc358743",
	.stream_name	= "TC358743",
	.codec_dai_name	= "tc358743-hifi",
	.platform_name	= "imx-pcm-audio.2",
	.codec_name	= "imx-tc358743.0",
	.cpu_dai_name	= "imx-ssi.2",
	.ops		= &imxpac_tc358743_snd_ops,
};

static const struct snd_soc_dapm_widget imx_3stack_dapm_widgets_a[] = {
};

static const struct snd_soc_dapm_route audio_map_a[] = {
};

static struct snd_soc_card imxpac_tc358743 = {
	.name		= "cpuimx-audio_hdmi_in",
	.dai_link	= &imxpac_tc358743_dai,
	.num_links	= 1,
	.dapm_widgets	= imx_3stack_dapm_widgets_a,
	.num_dapm_widgets = ARRAY_SIZE(imx_3stack_dapm_widgets_a),
	.dapm_routes	= audio_map_a,
	.num_dapm_routes = ARRAY_SIZE(audio_map_a),
};

static int imx_audmux_config(int slave, int master)
{
	unsigned int ptcr, pdcr;
	slave = slave - 1;
	master = master - 1;

	/* SSI0 mastered by port 5 */
	ptcr = MXC_AUDMUX_V2_PTCR_SYN |
		MXC_AUDMUX_V2_PTCR_TFSDIR |
		MXC_AUDMUX_V2_PTCR_TFSEL(master | 0x8) |
		MXC_AUDMUX_V2_PTCR_TCLKDIR |
	MXC_AUDMUX_V2_PTCR_RFSDIR |
	MXC_AUDMUX_V2_PTCR_RFSEL(master | 0x8) |
	MXC_AUDMUX_V2_PTCR_RCLKDIR |
	MXC_AUDMUX_V2_PTCR_RCSEL(master | 0x8) |
		MXC_AUDMUX_V2_PTCR_TCSEL(master | 0x8);
	pdcr = MXC_AUDMUX_V2_PDCR_RXDSEL(master);
	mxc_audmux_v2_configure_port(slave, ptcr, pdcr);

	ptcr = MXC_AUDMUX_V2_PTCR_SYN;
	pdcr = MXC_AUDMUX_V2_PDCR_RXDSEL(master);
	mxc_audmux_v2_configure_port(master, ptcr, pdcr);
	return 0;
}

static struct snd_soc_dai_driver tc358743_dai;
static struct snd_soc_codec_driver soc_codec_dev_tc358743;

static int __devinit imx_tc358743_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mxc_audio_platform_data *plat = pdev->dev.platform_data;
	int ret = 0;

	pr_info("%s: %s entry\n", __func__, pdev->name);
	if (!g_td)
		return -EPROBE_DEFER;

	ret = snd_soc_register_codec(dev, &soc_codec_dev_tc358743,
			&tc358743_dai, 1);
	if (ret) {
		pr_err("%s:  register failed, error=%d\n",
			__func__, ret);
		return ret;
	}


	imx_audmux_config(plat->src_port, plat->ext_port);

	ret = -EINVAL;
	if (plat->init && plat->init())
		goto exit;

	imxpac_tc358743.dev = dev;
	ret = snd_soc_register_card(&imxpac_tc358743);
	if (ret)
		dev_err(dev, "snd_soc_register_card() failed: %d\n", ret);
exit:
	if (ret)
		snd_soc_unregister_codec(dev);

	return ret;
}

static int imx_tc358743_remove(struct platform_device *pdev)
{
	struct mxc_audio_platform_data *plat = pdev->dev.platform_data;

/* Audio breakdown */
	snd_soc_unregister_card(&imxpac_tc358743);

	snd_soc_unregister_codec(&pdev->dev);
	if (plat->finit)
		plat->finit();

	return 0;
}

static struct platform_driver imx_tc358743_audio1_driver = {
	.probe = imx_tc358743_probe,
	.remove = imx_tc358743_remove,
	.driver = {
		   .name = "imx-tc358743",
		   },
};


/* Codec setup */
static int tc358743_codec_probe(struct snd_soc_codec *codec)
{
	return 0;
}

static int tc358743_codec_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static int tc358743_codec_suspend(struct snd_soc_codec *codec)
{
//	tc358743_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int tc358743_codec_resume(struct snd_soc_codec *codec)
{
//	tc358743_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}

static int tc358743_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	return 0;
}

static const u8 tc358743_reg[0] = {
};

static struct snd_soc_codec_driver soc_codec_dev_tc358743 = {
	.set_bias_level = tc358743_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(tc358743_reg),
	.reg_word_size = sizeof(u8),
	.reg_cache_default = tc358743_reg,
	.probe = tc358743_codec_probe,
	.remove = tc358743_codec_remove,
	.suspend = tc358743_codec_suspend,
	.resume = tc358743_codec_resume,
};

#define AIC3X_RATES	SNDRV_PCM_RATE_8000_96000
#define AIC3X_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE)

static int tc358743_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	return 0;
}

static int tc358743_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

static int tc358743_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int tc358743_set_dai_fmt(struct snd_soc_dai *codec_dai,
				unsigned int fmt)
{
	return 0;
}

static struct snd_soc_dai_ops tc358743_dai_ops = {
	.hw_params	= tc358743_hw_params,
	.digital_mute	= tc358743_mute,
	.set_sysclk	= tc358743_set_dai_sysclk,
	.set_fmt	= tc358743_set_dai_fmt,
};

static struct snd_soc_dai_driver tc358743_dai = {
	.name = "tc358743-hifi",
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AIC3X_RATES,
		.formats = AIC3X_FORMATS,},
	.ops = &tc358743_dai_ops,
	.symmetric_rates = 1,
};

#endif

struct tc_mode_list {
	const char *name;
	enum tc358743_mode mode;
};

static const struct tc_mode_list tc358743_mode_list[] =
{
	{"None", 0},					/* 0 */
	{"VGA", tc358743_mode_480P_640_480},		/* 1 */
	{"240p/480i", 0},				/* 2 */
	{"288p/576i", 0},				/* 3 */
	{"W240p/480i", 0},				/* 4 */
	{"W288p/576i", 0},				/* 5 */
	{"480p", 0},					/* 6 */
	{"576p", 0},					/* 7 */
	{"W480p", tc358743_mode_480P_720_480},		/* 8 */
	{"W576p", 0},					/* 9 */
	{"WW480p", 0},					/* 10 */
	{"WW576p", 0},					/* 11 */
	{"720p", tc358743_mode_720P_60_1280_720},	/* 12 */
	{"1035i", 0},					/* 13 */
	{"1080i", 0},					/* 14 */
	{"1080p", tc358743_mode_1080P_1920_1080},	/* 15 */
};

static char tc358743_fps_list[tc358743_max_fps+1] =
{
[tc358743_60_fps] = 60,
[tc358743_30_fps] = 30,
[tc358743_max_fps] = 0
};

static int tc358743_audio_list[16] =
{
	44100,
	0,
	48000,
	32000,
	22050,
	384000,
	24000,
	352800,
	88200,
	768000,
	96000,
	705600,
	176400,
	0,
	192000,
	0
};

static char str_on[80];
static void report_netlink(struct tc_data *td)
{
	struct sensor_data *sensor = &td->sensor;
	char *envp[2];
	envp[0] = &str_on[0];
	envp[1] = NULL;
	sprintf(envp[0], "HDMI RX: %d (%s) %d %d",
			td->mode,
			tc358743_mode_info_data[td->fps][td->mode].name,
			tc358743_fps_list[td->fps], tc358743_audio_list[td->audio]);
	kobject_uevent_env(&(sensor->i2c_client->dev.kobj), KOBJ_CHANGE, envp);
	td->det_work_timeout = DET_WORK_TIMEOUT_DEFAULT;
	pr_debug("%s: HDMI RX (%d) mode: %s fps: %d (%d, %d) audio: %d\n",
		__func__, td->mode,
		tc358743_mode_info_data[td->fps][td->mode].name, td->fps, td->bounce,
		td->det_work_timeout, tc358743_audio_list[td->audio]);
}

static void tc_det_worker(struct work_struct *work)
{
	struct tc_data *td = container_of(work, struct tc_data, det_work.work);
	struct sensor_data *sensor = &td->sensor;
	int ret;
	u32 u32val, u852f;
	enum tc358743_mode mode = tc358743_mode_INIT;


	if (!td->det_work_enable)
		return;
	mutex_lock(&td->access_lock);

	if (!td->det_work_enable) {
		goto out2;
	}
	u32val = 0;
	ret = tc358743_read_reg(sensor, 0x8621, &u32val);
	if (ret >= 0) {
		if (td->audio != (((unsigned char)u32val) & 0x0f)) {
			td->audio = ((unsigned char)u32val) & 0x0f;
			report_netlink(td);
		}
	}
	u852f = 0;
	ret = tc358743_read_reg(sensor, 0x852f, &u852f);
	if (ret < 0) {
		pr_err("%s: Error reading lock\n", __func__);
		td->det_work_timeout = DET_WORK_TIMEOUT_DEFERRED;
		goto out;
	}
	if (u852f & TC3587430_HDMI_DETECT) {
		td->lock = u852f & TC3587430_HDMI_DETECT;
		u32val = 0;
		ret = tc358743_read_reg(sensor, 0x8521, &u32val);
		if (ret < 0) {
			pr_err("%s: Error reading mode\n", __func__);
		}
		pr_info("%s: detect 8521=%x 852f=%x\n", __func__, u32val, u852f);
		u32val &= 0x0f;
		td->fps = tc358743_60_fps;
		if (!u32val) {
			int hsize, vsize;

			hsize = tc358743_read_reg_val16(sensor, 0x8582);
			vsize = tc358743_read_reg_val16(sensor, 0x8588);
			pr_info("%s: detect hsize=%d, vsize=%d\n", __func__, hsize, vsize);
			if ((hsize == 1024) && (vsize == 768))
				mode = tc358743_mode_1024x768;
			else if (hsize == 1280)
				mode = tc358743_mode_720P_60_1280_720;
			else if (hsize == 1920)
				mode = tc358743_mode_1080P_1920_1080;
			td->fps = tc358743_60_fps;
		} else {
			mode = tc358743_mode_list[u32val].mode;
			if (td->mode != mode)
				pr_debug("%s: %s detected\n", __func__, tc358743_mode_list[u32val].name);
			if (u852f >= 0xe)
				td->fps = ((u852f & 0x0f) > 0xa)? tc358743_60_fps: tc358743_30_fps;
		}
	} else {
		if (td->lock)
			td->lock = 0;
		u32val = 0;
		ret = tc358743_read_reg(sensor, 0x8521, &u32val);
		if (ret < 0) {
			pr_err("%s: Error reading mode\n", __func__);
		}
		pr_info("%s: lost hdmi_detect 8521=%x 852f=%x\n", __func__, u32val, u852f);
//		if (u32val)
//			mode = tc358743_mode_list[u32val].mode;
	}
	if (td->mode != mode) {
		td->det_work_timeout = DET_WORK_TIMEOUT_DEFAULT;
		td->bounce = MAX_BOUNCE;
		pr_debug("%s: HDMI RX (%d != %d) mode: %s fps: %d (%d, %d)\n",
				__func__, td->mode, mode,
				tc358743_mode_info_data[td->fps][mode].name,
				td->fps, td->bounce, td->det_work_timeout);
		td->mode = mode;
		sensor->streamcap.capturemode = mode;
		sensor->spix.swidth = tc358743_mode_info_data[td->fps][mode].width;
		sensor->spix.sheight = tc358743_mode_info_data[td->fps][mode].height;
		td->det_changed = 1;
	} else if (td->bounce) {
		td->bounce--;
		td->det_work_timeout = DET_WORK_TIMEOUT_DEFAULT;

		if (!td->bounce) {
			u32val = 0;
			ret = tc358743_read_reg(sensor, 0x8621, &u32val);
			if (ret >= 0) {
				td->audio = ((unsigned char)u32val) & 0x0f;
				report_netlink(td);
			}
			if (td->mode) {
				td->det_work_timeout = DET_WORK_TIMEOUT_DEFERRED;
				goto out2;
			}
		}
	} else if (td->mode && !td->bounce) {
		goto out2;
	}
out:
	schedule_delayed_work(&td->det_work, msecs_to_jiffies(td->det_work_timeout));
out2:
	mutex_unlock(&td->access_lock);
}

static irqreturn_t tc358743_detect_handler(int irq, void *data)
{
	struct tc_data *td = data;
	struct sensor_data *sensor = &td->sensor;

	pr_debug("%s: IRQ %d\n", __func__, sensor->i2c_client->irq);
	schedule_delayed_work(&td->det_work, msecs_to_jiffies(10));
	return IRQ_HANDLED;
}

static	u16 regoffs = 0;

static ssize_t tc358743_store_regdump(struct device *device,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct tc_data *td = g_td;
	struct sensor_data *sensor = &td->sensor;
	u32 val;
	int retval;
	int size;

	retval = sscanf(buf, "%x", &val);
	if (retval == 1) {
		size = get_reg_size(regoffs, 0);
		retval = tc358743_write_reg(sensor, regoffs, val, size);
		if (retval < 0)
			pr_info("%s: err %d\n", __func__, retval);
	}
	return count;
}

/*!
 * tc358743 I2C probe function
 *
 * @param adapter	    struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
#define DUMP_LENGTH 256

static ssize_t tc358743_show_regdump(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc_data *td = g_td;
	struct sensor_data *sensor = &td->sensor;
	int i, len = 0;
	int retval;
	int size;

	if (!td)
		return len;
	mutex_lock(&td->access_lock);

	for (i=0; i<DUMP_LENGTH; i+=size) {
		u32 u32val = 0;
		int reg = regoffs+i;

		size = get_reg_size(reg, 0);
		if (!(i & 0xf))
			len += sprintf(buf+len, "\n%04X:", reg);
		if (size == 0) {
			len += sprintf(buf+len, " xx");
			size = 1;
			continue;
		}
		retval = tc358743_read_reg(sensor, reg, &u32val);
		if (retval < 0) {
			u32val = 0xff;
			retval = 1;
		}
		if (size == 1)
			len += sprintf(buf+len, " %02X", u32val&0xff);
		else if (size == 2)
			len += sprintf(buf+len, " %04X", u32val&0xffff);
		else
			len += sprintf(buf+len, " %08X", u32val);
	}
	mutex_unlock(&td->access_lock);
	len += sprintf(buf+len, "\n");
	return len;
}

static DEVICE_ATTR(regdump, S_IRUGO|S_IWUSR, tc358743_show_regdump, tc358743_store_regdump);

static ssize_t tc358743_store_regoffs(struct device *device,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	u32 val;
	int retval;
	retval = sscanf(buf, "%x", &val);
	if (1 == retval)
		regoffs = (u16)val;
	return count;
}

static ssize_t tc358743_show_regoffs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;

	len += sprintf(buf+len, "0x%04X\n", regoffs);
	return len;
}

static DEVICE_ATTR(regoffs, S_IRUGO|S_IWUSR, tc358743_show_regoffs, tc358743_store_regoffs);

static ssize_t tc358743_store_hpd(struct device *device,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct tc_data *td = g_td;
	u32 val;
	int retval;
	retval = sscanf(buf, "%d", &val);
	if (1 == retval)
		td->hpd_active = (u16)val;
	return count;
}

static ssize_t tc358743_show_hpd(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc_data *td = g_td;
	int len = 0;

	len += sprintf(buf+len, "%d\n", td->hpd_active);
	return len;
}

static DEVICE_ATTR(hpd, S_IRUGO|S_IWUSR, tc358743_show_hpd, tc358743_store_hpd);

static ssize_t tc358743_show_hdmirx(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc_data *td = g_td;
	int len = 0;

	len += sprintf(buf+len, "%d\n", td->mode);
	return len;
}

static DEVICE_ATTR(hdmirx, S_IRUGO, tc358743_show_hdmirx, NULL);

static ssize_t tc358743_show_fps(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc_data *td = g_td;
	int len = 0;

	len += sprintf(buf+len, "%d\n", tc358743_fps_list[td->fps]);
	return len;
}

static DEVICE_ATTR(fps, S_IRUGO, tc358743_show_fps, NULL);

#ifdef CONFIG_TC358743_AUDIO
static ssize_t tc358743_show_audio(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc_data *td = g_td;
	int len = 0;

	len += sprintf(buf+len, "%d\n", tc358743_audio_list[td->audio]);
	return len;
}

static DEVICE_ATTR(audio, S_IRUGO, tc358743_show_audio, NULL);
#endif

static int tc358743_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	int retval;
	struct tc_data *td;
	struct sensor_data *sensor;
	u8 chip_id_high;
	u32 u32val;
	int mode = tc358743_mode_INIT;

	td = kzalloc(sizeof(*td), GFP_KERNEL);
	if (!td)
		return -ENOMEM;
	td->hpd_active = 1;
	td->det_work_timeout = DET_WORK_TIMEOUT_DEFAULT;
	td->audio = 2;
	mutex_init(&td->access_lock);
	mutex_lock(&td->access_lock);
	sensor = &td->sensor;

	/* request power down pin */
	td->pwn_gpio = of_get_named_gpio(dev->of_node, "pwn-gpios", 0);
	if (!gpio_is_valid(td->pwn_gpio)) {
		dev_warn(dev, "no sensor pwdn pin available");
	} else {
		retval = devm_gpio_request_one(dev, td->pwn_gpio, GPIOF_OUT_INIT_HIGH,
					"tc_mipi_pwdn");
		if (retval < 0) {
			dev_warn(dev, "request of pwn_gpio failed");
			return retval;
		}
	}
	/* request reset pin */
	td->rst_gpio = of_get_named_gpio(dev->of_node, "rst-gpios", 0);
	if (!gpio_is_valid(td->rst_gpio)) {
		dev_warn(dev, "no sensor reset pin available");
		return -EINVAL;
	}
	retval = devm_gpio_request_one(dev, td->rst_gpio, GPIOF_OUT_INIT_HIGH,
					"tc_mipi_reset");
	if (retval < 0) {
		dev_warn(dev, "request of tc_mipi_reset failed");
		return retval;
	}

	/* Set initial values for the sensor struct. */
	sensor->mipi_camera = 1;
	sensor->virtual_channel = 0;
	sensor->sensor_clk = devm_clk_get(dev, "csi_mclk");

	retval = of_property_read_u32(dev->of_node, "mclk",
					&(sensor->mclk));
	if (retval) {
		dev_err(dev, "mclk missing or invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "mclk_source",
					(u32 *) &(sensor->mclk_source));
	if (retval) {
		dev_err(dev, "mclk_source missing or invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "ipu_id",
					&sensor->ipu_id);
	if (retval) {
		dev_err(dev, "ipu_id missing or invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "csi_id",
					&(sensor->csi));
	if (retval) {
		dev_err(dev, "csi id missing or invalid\n");
		return retval;
	}
	if ((unsigned)sensor->ipu_id || (unsigned)sensor->csi) {
		dev_err(dev, "invalid ipu/csi\n");
		return -EINVAL;
	}

	if (!IS_ERR(sensor->sensor_clk))
		clk_prepare_enable(sensor->sensor_clk);

	sensor->io_init = tc_io_init;
	sensor->i2c_client = client;
	sensor->streamcap.capability = V4L2_MODE_HIGHQUALITY |
					   V4L2_CAP_TIMEPERFRAME;
	sensor->streamcap.capturemode = mode;
	sensor->streamcap.timeperframe.denominator = DEFAULT_FPS;
	sensor->streamcap.timeperframe.numerator = 1;

	sensor->pix.pixelformat = get_pixelformat(0, mode);
	sensor->pix.width = tc358743_mode_info_data[0][mode].width;
	sensor->pix.height = tc358743_mode_info_data[0][mode].height;
	pr_debug("%s: format: %x, capture mode: %d fps: %d width: %d height: %d\n",
		__func__, sensor->pix.pixelformat, mode,
		sensor->streamcap.timeperframe.denominator *
		sensor->streamcap.timeperframe.numerator,
		sensor->pix.width,
		sensor->pix.height);

	tc_regulator_init(td, dev);
	power_control(td, 1);
	tc_reset(td);

	u32val = 0;
	retval = tc358743_read_reg(sensor, TC358743_CHIP_ID_HIGH_BYTE, &u32val);
	if (retval < 0) {
		pr_err("%s:cannot find camera\n", __func__);
		retval = -ENODEV;
		goto err4;
	}
	chip_id_high = (u8)u32val;

#ifdef CONFIG_TC358743_AUDIO
/* Audio setup */
	retval = device_create_file(&client->dev, &dev_attr_audio);
#endif

	tc358743_int_device.priv = td;
	if (!g_td)
		g_td = td;

#if 1
	INIT_DELAYED_WORK(&td->det_work, tc_det_worker);
	if (sensor->i2c_client->irq) {
		retval = request_irq(sensor->i2c_client->irq, tc358743_detect_handler,
				IRQF_SHARED | IRQF_TRIGGER_FALLING,
				"tc358743_det", td);
		if (retval < 0)
			dev_warn(&sensor->i2c_client->dev,
				"cound not request det irq %d\n",
				sensor->i2c_client->irq);
	}

	schedule_delayed_work(&td->det_work, msecs_to_jiffies(td->det_work_timeout));
#endif
	retval = tc358743_reset(td);
	if (retval)
		goto err4;

	i2c_set_clientdata(client, td);
	mutex_unlock(&td->access_lock);
	retval = v4l2_int_device_register(&tc358743_int_device);
	mutex_lock(&td->access_lock);
	if (retval) {
		pr_err("%s:  v4l2_int_device_register failed, error=%d\n",
			__func__, retval);
		goto err4;
	}
	power_control(td, 0);

	retval = device_create_file(dev, &dev_attr_fps);
	retval = device_create_file(dev, &dev_attr_hdmirx);
	retval = device_create_file(dev, &dev_attr_hpd);
	retval = device_create_file(dev, &dev_attr_regoffs);
	retval = device_create_file(dev, &dev_attr_regdump);

	if (retval) {
		pr_err("%s:  create bin file failed, error=%d\n",
			__func__, retval);
		goto err3;
	}
	mutex_unlock(&td->access_lock);
	dev_err(dev, "%s: finished, error=%d\n", __func__, retval);
	return retval;

err3:
#ifdef CONFIG_TC358743_AUDIO
	device_remove_file(dev, &dev_attr_audio);
#endif
	device_remove_file(dev, &dev_attr_fps);
	device_remove_file(dev, &dev_attr_hdmirx);
	device_remove_file(dev, &dev_attr_hpd);
	device_remove_file(dev, &dev_attr_regoffs);
	device_remove_file(dev, &dev_attr_regdump);
err4:
	power_control(td, 0);
	mutex_unlock(&td->access_lock);
	pr_err("%s: failed, error=%d\n", __func__, retval);
	if (g_td == td)
		g_td = NULL;
	mutex_destroy(&td->access_lock);
	kfree(td);
	return retval;
}

/*!
 * tc358743 I2C detach function
 *
 * @param client	    struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int tc358743_remove(struct i2c_client *client)
{
	int i;
	struct tc_data *td = i2c_get_clientdata(client);
	struct sensor_data *sensor = &td->sensor;

	// Stop delayed work
	cancel_delayed_work_sync(&td->det_work);
	mutex_lock(&td->access_lock);

	power_control(td, 0);
	// Remove IRQ
	if (sensor->i2c_client->irq) {
		free_irq(sensor->i2c_client->irq,  sensor);
	}

	/*Remove sysfs entries*/
#ifdef CONFIG_TC358743_AUDIO
	device_remove_file(&client->dev, &dev_attr_audio);
#endif
	device_remove_file(&client->dev, &dev_attr_fps);
	device_remove_file(&client->dev, &dev_attr_hdmirx);
	device_remove_file(&client->dev, &dev_attr_hpd);
	device_remove_file(&client->dev, &dev_attr_regoffs);
	device_remove_file(&client->dev, &dev_attr_regdump);

	mutex_unlock(&td->access_lock);
	v4l2_int_device_unregister(&tc358743_int_device);

	for (i = REGULATOR_CNT - 1; i >= 0; i--) {
		if (td->regulator[i]) {
			regulator_disable(td->regulator[i]);
		}
	}
	mutex_destroy(&td->access_lock);
	if (g_td == td)
		g_td = NULL;
	kfree(td);
	return 0;
}

/*!
 * tc358743 init function
 * Called by insmod tc358743_camera.ko.
 *
 * @return  Error code indicating success or failure
 */
static __init int tc358743_init(void)
{
	int err;

	err = i2c_add_driver(&tc358743_i2c_driver);
	if (err != 0)
		pr_err("%s:driver registration failed, error=%d\n",
			__func__, err);
#ifdef CONFIG_TC358743_AUDIO
/* Audio setup */
	err = platform_driver_register(&imx_tc358743_audio1_driver);
	if (err) {
		pr_err("%s: Platform driver register failed, error=%d\n",
			__func__, err);
	}
#endif
	return err;
}

/*!
 * tc358743 cleanup function
 * Called on rmmod tc358743_camera.ko
 *
 * @return  Error code indicating success or failure
 */
static void __exit tc358743_clean(void)
{
#ifdef CONFIG_TC358743_AUDIO
/* Audio breakdown */
	platform_driver_unregister(&imx_tc358743_audio1_driver);
#endif
	i2c_del_driver(&tc358743_i2c_driver);
}

module_init(tc358743_init);
module_exit(tc358743_clean);

MODULE_AUTHOR("Panasonic Avionics Corp.");
MODULE_DESCRIPTION("Toshiba TC358743 HDMI-to-CSI2 Bridge MIPI Input Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("CSI");
