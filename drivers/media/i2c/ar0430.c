// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for AR0430 CMOS Image Sensor from Aptina
 *
 * Copyright (C) 2011, Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 * Copyright (C) 2011, Javier Martin <javier.martin@vista-silicon.com>
 * Copyright (C) 2011, Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 *
 * Based on the AR0330 driver
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/videodev2.h>

#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

#define AR0430_PIXEL_ARRAY_WIDTH		2316U
#define AR0430_PIXEL_ARRAY_HEIGHT		1746U

#define AR0430_WINDOW_LEFT_BORDER		6U
#define AR0430_WINDOW_TOP_BORDER		6U

#define AR0430_WINDOW_WIDTH_MIN			32U
#define AR0430_WINDOW_WIDTH_DEF			2304U
#define AR0430_WINDOW_WIDTH_MAX			2316U
#define AR0430_WINDOW_HEIGHT_MIN		32U
#define AR0430_WINDOW_HEIGHT_DEF		1296U
#define AR0430_WINDOW_HEIGHT_MAX		1746U

#define AR0430_HBLANK_DEF			192
#define AR0430_VBLANK_DEF			60

#define AR0430_CHIP_VERSION			0x3000
#define AR0430_CHIP_VERSION_VALUE		0x1550

#define AR0430_X_ADDR_START			0x0344
#define AR0430_Y_ADDR_START			0x0346
#define AR0430_X_ADDR_END			0x0348
#define AR0430_Y_ADDR_END			0x034a
#define AR0430_FRAME_LENGTH_LINES		0x300a
#define AR0430_LINE_LENGTH_PCK			0x300c
#define AR0430_CHIP_REVISION			0x31fe
#define AR0430_CHIP_REVISION_1x			0x10
#define AR0430_CHIP_REVISION_2x			0x20
#define AR0430_LOCK_CONTROL			0x3010
#define AR0430_LOCK_CONTROL_UNLOCK		0xbeef
#define AR0430_COARSE_INTEGRATION_TIME		0x3012
#define AR0430_COARSE_INTEGRATION_TIME_MIN	1U
#define AR0430_COARSE_INTEGRATION_TIME_DEF	16U
#define AR0430_COARSE_INTEGRATION_TIME_MAX	65535U
#define AR0430_RESET				0x301a
#define AR0430_RESET_SMIA_DIS			(1 << 12)
#define AR0430_RESET_FORCE_PLL_ON		(1 << 11)
#define AR0430_RESET_RESTART_BAD		(1 << 10)
#define AR0430_RESET_MASK_BAD			(1 << 9)
#define AR0430_RESET_GPI_EN			(1 << 8)
#define AR0430_RESET_PARALLEL_EN		(1 << 7)
#define AR0430_RESET_DRIVE_PINS			(1 << 6)
#define AR0430_RESET_LOCK_REG			(1 << 3)
#define AR0430_RESET_STREAM			(1 << 2)
#define AR0430_RESET_RESTART			(1 << 1)
#define AR0430_RESET_RESET			(1 << 0)
/* AR0430_MODE_SELECT is an alias for AR0430_RESET_STREAM */
#define AR0430_MODE_SELECT			0x301c
#define AR0430_MODE_SELECT_STREAM		(1 << 0)

#define AR0430_VT_PIX_CLK_DIV			0x0300
#define AR0430_VT_SYS_CLK_DIV			0x0302
#define AR0430_PRE_PLL_CLK_DIV			0x0304
#define AR0430_PLL_MULTIPLIER			0x0306
#define AR0430_OP_PIX_CLK_DIV			0x0308
#define AR0430_OP_SYS_CLK_DIV			0x030a
#define AR0430_FRAME_COUNT			0x303b
#define AR0430_READ_MODE			0x3040
#define AR0430_READ_MODE_VERT_FLIP		(1 << 15)
#define AR0430_READ_MODE_HORIZ_MIRROR		(1 << 14)
#define AR0430_READ_MODE_COL_BIN		(1 << 13)
#define AR0430_READ_MODE_ROW_BIN		(1 << 12)
#define AR0430_READ_MODE_COL_SF_BIN		(1 << 9)
#define AR0430_READ_MODE_COL_SUM		(1 << 5)
#define AR0430_GREEN1_GAIN			0x3056
#define AR0430_BLUE_GAIN			0x3058
#define AR0430_RED_GAIN				0x305a
#define AR0430_GREEN2_GAIN			0x305c
#define AR0430_GLOBAL_GAIN			0x305e
#define AR0430_GLOBAL_GAIN_MIN			128U
#define AR0430_GLOBAL_GAIN_DEF			128U
#define AR0430_GLOBAL_GAIN_MAX			2047U
#define AR0430_ANALOG_GAIN			0x3028
#define AR0430_ANALOG_GAIN_MIN			0U
#define AR0430_ANALOG_GAIN_DEF			4U
#define AR0430_ANALOG_GAIN_MAX			32U
#define AR0430_DATAPATH_SELECT			0x306e
#define AR0430_DATAPATH_SLEW_DOUT_MASK		(7 << 13)
#define AR0430_DATAPATH_SLEW_DOUT_SHIFT		13
#define AR0430_DATAPATH_SLEW_PCLK_MASK		(7 << 10)
#define AR0430_DATAPATH_SLEW_PCLK_SHIFT		10
#define AR0430_DATAPATH_HIGH_VCM		(1 << 9)
#define AR0430_TEST_PATTERN_MODE		0x0600
#define AR0430_TEST_DATA_RED			0x0602
#define AR0430_TEST_DATA_GREENR			0x0604
#define AR0430_TEST_DATA_BLUE			0x0606
#define AR0430_TEST_DATA_GREENB			0x0608
#define AR0430_SEQ_DATA_PORT			0x3086
#define AR0430_SEQ_CTRL_PORT			0x3088
#define AR0430_X_ODD_INC			0x30a2
#define AR0430_Y_ODD_INC			0x30a6
#define AR0430_X_ODD_INC			0x30a2
#define AR0430_DATA_FORMAT_BITS			0x31ac
#define AR0430_SERIAL_FORMAT			0x31ae
#define AR0430_FRAME_PREAMBLE			0x31b0
#define AR0430_LINE_PREAMBLE			0x31b2
#define AR0430_MIPI_TIMING_0			0x31b4
#define AR0430_MIPI_TIMING_1			0x31b6
#define AR0430_MIPI_TIMING_2			0x31b8
#define AR0430_MIPI_TIMING_3			0x31ba
#define AR0430_MIPI_TIMING_4			0x31bc
#define AR0430_MIPI_CONFIG_STATUS		0x31be
#define AR0430_COMPRESSION			0x31d0
#define AR0430_COMPRESSION_ENABLE		(1 << 0)
#define AR0430_POLY_SC				0x3780
#define AR0430_POLY_SC_ENABLE			(1 << 15)

struct ar0430 {
	struct i2c_client *client;
	struct device *dev;

	unsigned int version;

	struct clk *clock;
	struct gpio_desc *reset;
	struct regulator_bulk_data supplies[3];

	struct v4l2_subdev subdev;
	struct media_pad pad;

	struct v4l2_rect crop; /* Sensor window */
	struct v4l2_mbus_framefmt format;

	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *flip[2];
	struct v4l2_ctrl *pixel_rate;

	bool streaming;

	/* Registers cache */
	u16 read_mode;
};

static struct ar0430 *to_ar0430(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ar0430, subdev);
}

static const char *const ar0430_supply_names[] = {
	"vaa",
	"vddio",
	"vdd",
};

/*
 * Register access
 */

static int __ar0430_read(struct ar0430 *ar0430, u16 reg, size_t size)
{
	struct i2c_client *client = ar0430->client;
	u8 data[2];
	struct i2c_msg msg[2] = {
		{ client->addr, 0, 2, data },
		{ client->addr, I2C_M_RD, size, data },
	};
	int ret;

	data[0] = reg >> 8;
	data[1] = reg & 0xff;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		dev_err(ar0430->dev, "%s(0x%04x) failed (%d)\n", __func__, reg,
			ret);
		return ret;
	}

	if (size == 2)
		return (data[0] << 8) | data[1];
	else
		return data[0];
}

static int __ar0430_write(struct ar0430 *ar0430, u16 reg, u16 value,
			  size_t size, int *err)
{
	struct i2c_client *client = ar0430->client;
	u8 data[4];
	struct i2c_msg msg = { client->addr, 0, 2 + size, data };
	int ret;

	if (err && *err)
		return *err;

	dev_dbg(ar0430->dev, "writing 0x%04x to 0x%04x\n", value, reg);

	data[0] = reg >> 8;
	data[1] = reg & 0xff;
	if (size == 2) {
		data[2] = value >> 8;
		data[3] = value & 0xff;
	} else {
		data[2] = value & 0xff;
	}

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(ar0430->dev, "%s(0x%04x) failed (%d)\n", __func__, reg,
			ret);
		if (err)
			*err = ret;
		return ret;
	}

	return 0;
}

static inline int ar0430_read8(struct ar0430 *ar0430, u16 reg)
{
	return __ar0430_read(ar0430, reg, 1);
}

static inline int ar0430_write8(struct ar0430 *ar0430, u16 reg, u8 value)
{
	return __ar0430_write(ar0430, reg, value, 1, NULL);
}

static inline int ar0430_read16(struct ar0430 *ar0430, u16 reg)
{
	return __ar0430_read(ar0430, reg, 2);
}

static inline int ar0430_write16(struct ar0430 *ar0430, u16 reg, u16 value,
				 int *err)
{
	return __ar0430_write(ar0430, reg, value, 2, err);
}

static int ar0430_set_read_mode(struct ar0430 *ar0430, u16 clear, u16 set)
{
	u16 value = (ar0430->read_mode & ~clear) | set;
	int ret;

	ret = ar0430_write16(ar0430, AR0430_READ_MODE, value, NULL);
	if (ret < 0)
		return ret;

	ar0430->read_mode = value;
	return 0;
}

static int ar0430_analog_setup_recommended(struct ar0430 *ar0430)
{
	int ret = 0;

	dev_dbg(ar0430->dev, "In function [%s]: line %d", __func__, __LINE__);

	ar0430_write16(ar0430, 0x3EB4, 0x676A, &ret);
	ar0430_write16(ar0430, 0x3EBC, 0x0010, &ret);
	ar0430_write16(ar0430, 0x3ECE, 0x008B, &ret);
	ar0430_write16(ar0430, 0x3ED0, 0x0071, &ret);
	ar0430_write16(ar0430, 0x3ED2, 0xB85C, &ret);
	ar0430_write16(ar0430, 0x3ED4, 0x120A, &ret);
	ar0430_write16(ar0430, 0x3EDC, 0x7D04, &ret);
	ar0430_write16(ar0430, 0x30EE, 0x1140, &ret);
	ar0430_write16(ar0430, 0x3120, 0x0001, &ret);
	ar0430_write16(ar0430, 0x3044, 0x05E0, &ret);
	ar0430_write16(ar0430, 0x30C0, 0x0004, &ret);
	ar0430_write16(ar0430, 0x30C2, 0x0100, &ret);
	ar0430_write16(ar0430, 0x316E, 0x8400, &ret);
	ar0430_write16(ar0430, 0x3EF0, 0x3E3E, &ret);
	ar0430_write16(ar0430, 0x3EF2, 0x8082, &ret);
	ar0430_write16(ar0430, 0x3EAE, 0x003F, &ret);
	ar0430_write16(ar0430, 0x3EC0, 0x8888, &ret);
	ar0430_write16(ar0430, 0x3EC2, 0x008B, &ret);
	ar0430_write16(ar0430, 0x3EC4, 0x2763, &ret);
	ar0430_write16(ar0430, 0x3EC6, 0x913D, &ret);
	ar0430_write16(ar0430, 0x3ECA, 0x90B7, &ret);
	ar0430_write16(ar0430, 0x3EBA, 0x0C11, &ret);
	ar0430_write16(ar0430, 0x3EBE, 0x5504, &ret);
	ar0430_write16(ar0430, 0x3ED8, 0x9844, &ret);
	ar0430_write16(ar0430, 0x3EDA, 0xB21D, &ret);
	ar0430_write16(ar0430, 0x3F3A, 0x0080, &ret);

	if (ret < 0)
		return ret;

	return 0;
}
static int ar0430_pixel_timings_recommended_10bit(struct ar0430 *ar0430)
{
	int ret = 0;

	dev_dbg(ar0430->dev, "In function [%s]: line %d", __func__, __LINE__);

	ar0430_write16(ar0430, 0x3D00, 0x046C, &ret);
	ar0430_write16(ar0430, 0x3D02, 0xFF72, &ret);
	ar0430_write16(ar0430, 0x3D04, 0xFFFF, &ret);
	ar0430_write16(ar0430, 0x3D06, 0xFFFF, &ret);
	ar0430_write16(ar0430, 0x3D08, 0x8000, &ret);
	ar0430_write16(ar0430, 0x3D0A, 0x3251, &ret);
	ar0430_write16(ar0430, 0x3D0C, 0x2328, &ret);
	ar0430_write16(ar0430, 0x3D0E, 0x080C, &ret);
	ar0430_write16(ar0430, 0x3D10, 0x6280, &ret);
	ar0430_write16(ar0430, 0x3D12, 0x3105, &ret);
	ar0430_write16(ar0430, 0x3D14, 0x4B73, &ret);
	ar0430_write16(ar0430, 0x3D16, 0x11C0, &ret);
	ar0430_write16(ar0430, 0x3D18, 0x1013, &ret);
	ar0430_write16(ar0430, 0x3D1A, 0x3108, &ret);
	ar0430_write16(ar0430, 0x3D1C, 0x2086, &ret);
	ar0430_write16(ar0430, 0x3D1E, 0x8173, &ret);
	ar0430_write16(ar0430, 0x3D20, 0x8073, &ret);
	ar0430_write16(ar0430, 0x3D22, 0x821C, &ret);
	ar0430_write16(ar0430, 0x3D24, 0x0082, &ret);
	ar0430_write16(ar0430, 0x3D26, 0x588A, &ret);
	ar0430_write16(ar0430, 0x3D28, 0x499E, &ret);
	ar0430_write16(ar0430, 0x3D2A, 0x4380, &ret);
	ar0430_write16(ar0430, 0x3D2C, 0x1103, &ret);
	ar0430_write16(ar0430, 0x3D2E, 0xC560, &ret);
	ar0430_write16(ar0430, 0x3D30, 0x8F73, &ret);
	ar0430_write16(ar0430, 0x3D32, 0x9447, &ret);
	ar0430_write16(ar0430, 0x3D34, 0x8568, &ret);
	ar0430_write16(ar0430, 0x3D36, 0x8061, &ret);
	ar0430_write16(ar0430, 0x3D38, 0x8049, &ret);
	ar0430_write16(ar0430, 0x3D3A, 0x4759, &ret);
	ar0430_write16(ar0430, 0x3D3C, 0x6880, &ret);
	ar0430_write16(ar0430, 0x3D3E, 0x5680, &ret);
	ar0430_write16(ar0430, 0x3D40, 0x558E, &ret);
	ar0430_write16(ar0430, 0x3D42, 0x7384, &ret);
	ar0430_write16(ar0430, 0x3D44, 0x4686, &ret);
	ar0430_write16(ar0430, 0x3D46, 0x100C, &ret);
	ar0430_write16(ar0430, 0x3D48, 0x8B6A, &ret);
	ar0430_write16(ar0430, 0x3D4A, 0x8028, &ret);
	ar0430_write16(ar0430, 0x3D4C, 0x4097, &ret);
	ar0430_write16(ar0430, 0x3D4E, 0x100C, &ret);
	ar0430_write16(ar0430, 0x3D50, 0x847B, &ret);
	ar0430_write16(ar0430, 0x3D52, 0x8246, &ret);
	ar0430_write16(ar0430, 0x3D54, 0x8273, &ret);
	ar0430_write16(ar0430, 0x3D56, 0x9928, &ret);
	ar0430_write16(ar0430, 0x3D58, 0x4055, &ret);
	ar0430_write16(ar0430, 0x3D5A, 0x6A59, &ret);
	ar0430_write16(ar0430, 0x3D5C, 0x8349, &ret);
	ar0430_write16(ar0430, 0x3D5E, 0x6B82, &ret);
	ar0430_write16(ar0430, 0x3D60, 0x6B82, &ret);
	ar0430_write16(ar0430, 0x3D62, 0x6D82, &ret);
	ar0430_write16(ar0430, 0x3D64, 0x7382, &ret);
	ar0430_write16(ar0430, 0x3D66, 0x73AC, &ret);
	ar0430_write16(ar0430, 0x3D68, 0x7388, &ret);
	ar0430_write16(ar0430, 0x3D6A, 0x4785, &ret);
	ar0430_write16(ar0430, 0x3D6C, 0x6882, &ret);
	ar0430_write16(ar0430, 0x3D6E, 0x4947, &ret);
	ar0430_write16(ar0430, 0x3D70, 0x5968, &ret);
	ar0430_write16(ar0430, 0x3D72, 0x8358, &ret);
	ar0430_write16(ar0430, 0x3D74, 0x801A, &ret);
	ar0430_write16(ar0430, 0x3D76, 0x0081, &ret);
	ar0430_write16(ar0430, 0x3D78, 0x738C, &ret);
	ar0430_write16(ar0430, 0x3D7A, 0x200C, &ret);
	ar0430_write16(ar0430, 0x3D7C, 0x8E10, &ret);
	ar0430_write16(ar0430, 0x3D7E, 0xF080, &ret);
	ar0430_write16(ar0430, 0x3D80, 0x4585, &ret);
	ar0430_write16(ar0430, 0x3D82, 0x6A80, &ret);
	ar0430_write16(ar0430, 0x3D84, 0x2840, &ret);
	ar0430_write16(ar0430, 0x3D86, 0x4182, &ret);
	ar0430_write16(ar0430, 0x3D88, 0x4181, &ret);
	ar0430_write16(ar0430, 0x3D8A, 0x4354, &ret);
	ar0430_write16(ar0430, 0x3D8C, 0x8110, &ret);
	ar0430_write16(ar0430, 0x3D8E, 0x0381, &ret);
	ar0430_write16(ar0430, 0x3D90, 0x1030, &ret);
	ar0430_write16(ar0430, 0x3D92, 0x8446, &ret);
	ar0430_write16(ar0430, 0x3D94, 0x8210, &ret);
	ar0430_write16(ar0430, 0x3D96, 0x0CA4, &ret);
	ar0430_write16(ar0430, 0x3D98, 0x4A85, &ret);
	ar0430_write16(ar0430, 0x3D9A, 0x7B89, &ret);
	ar0430_write16(ar0430, 0x3D9C, 0x4A80, &ret);
	ar0430_write16(ar0430, 0x3D9E, 0x6783, &ret);
	ar0430_write16(ar0430, 0x3DA0, 0x7393, &ret);
	ar0430_write16(ar0430, 0x3DA2, 0x100C, &ret);
	ar0430_write16(ar0430, 0x3DA4, 0x8146, &ret);
	ar0430_write16(ar0430, 0x3DA6, 0x8110, &ret);
	ar0430_write16(ar0430, 0x3DA8, 0x3081, &ret);
	ar0430_write16(ar0430, 0x3DAA, 0x4311, &ret);
	ar0430_write16(ar0430, 0x3DAC, 0x0341, &ret);
	ar0430_write16(ar0430, 0x3DAE, 0x8100, &ret);
	ar0430_write16(ar0430, 0x3DB0, 0x0A86, &ret);
	ar0430_write16(ar0430, 0x3DB2, 0x1133, &ret);
	ar0430_write16(ar0430, 0x3DB4, 0x8046, &ret);
	ar0430_write16(ar0430, 0x3DB6, 0x8210, &ret);
	ar0430_write16(ar0430, 0x3DB8, 0x0CAE, &ret);
	ar0430_write16(ar0430, 0x3DBA, 0x4A88, &ret);
	ar0430_write16(ar0430, 0x3DBC, 0x7373, &ret);
	ar0430_write16(ar0430, 0x3DBE, 0x8273, &ret);
	ar0430_write16(ar0430, 0x3DC0, 0x834A, &ret);
	ar0430_write16(ar0430, 0x3DC2, 0x9010, &ret);
	ar0430_write16(ar0430, 0x3DC4, 0x0C46, &ret);
	ar0430_write16(ar0430, 0x3DC6, 0x1133, &ret);
	ar0430_write16(ar0430, 0x3DC8, 0x8100, &ret);
	ar0430_write16(ar0430, 0x3DCA, 0x1800, &ret);
	ar0430_write16(ar0430, 0x3DCC, 0x0681, &ret);
	ar0430_write16(ar0430, 0x3DCE, 0x5581, &ret);
	ar0430_write16(ar0430, 0x3DD0, 0x2CE0, &ret);
	ar0430_write16(ar0430, 0x3DD2, 0x6E80, &ret);
	ar0430_write16(ar0430, 0x3DD4, 0x3645, &ret);
	ar0430_write16(ar0430, 0x3DD6, 0x7000, &ret);
	ar0430_write16(ar0430, 0x3DD8, 0x8000, &ret);
	ar0430_write16(ar0430, 0x3DDA, 0x0382, &ret);
	ar0430_write16(ar0430, 0x3DDC, 0x4BC3, &ret);
	ar0430_write16(ar0430, 0x3DDE, 0x4B82, &ret);
	ar0430_write16(ar0430, 0x3DE0, 0x0003, &ret);
	ar0430_write16(ar0430, 0x3DE2, 0x8070, &ret);
	ar0430_write16(ar0430, 0x3DE4, 0xFFFF, &ret);
	ar0430_write16(ar0430, 0x3DE6, 0x937B, &ret);
	ar0430_write16(ar0430, 0x3DE8, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3DEA, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3DEC, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3DEE, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3DF0, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3DF2, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3DF4, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3DF6, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3DF8, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3DFA, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3DFC, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3DFE, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E00, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E02, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E04, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E06, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E08, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E0A, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E0C, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E0E, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E10, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E12, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E14, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E16, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E18, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E1A, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E1C, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E1E, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E20, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E22, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E24, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E26, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E28, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E2A, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E2C, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E2E, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E30, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E32, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E34, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E36, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E38, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E3A, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E3C, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E3E, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E40, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E42, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E44, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E46, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E48, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E4A, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E4C, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E4E, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E50, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E52, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E54, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E56, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E58, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E5A, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E5C, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E5E, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E60, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E62, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E64, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E66, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E68, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E6A, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E6C, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E6E, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E70, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E72, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E74, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E76, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E78, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E7A, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E7C, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E7E, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E80, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E82, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E84, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E86, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E88, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E8A, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E8C, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E8E, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E90, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E92, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E94, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E96, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E98, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E9A, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E9C, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3E9E, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3EA0, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3EA2, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3EA4, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3EA6, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3EA8, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3EAA, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3EAC, 0x0000, &ret);

	if (ret < 0)
		return ret;

	return 0;
}
/*
 * PLL configuration
 */

static int ar0430_pll_configure(struct ar0430 *ar0430)
{
	int ret = 0;

	ar0430_write16(ar0430, AR0430_VT_PIX_CLK_DIV, 0x06, &ret);
	ar0430_write16(ar0430, AR0430_VT_SYS_CLK_DIV, 0x0C01, &ret);
	ar0430_write16(ar0430, AR0430_PRE_PLL_CLK_DIV, 0x0A02, &ret);
	ar0430_write16(ar0430, AR0430_PLL_MULTIPLIER, 0x184B, &ret);
	ar0430_write16(ar0430, AR0430_OP_PIX_CLK_DIV, 0x03, &ret);
	ar0430_write16(ar0430, AR0430_OP_SYS_CLK_DIV, 0x01, &ret);
	ar0430_write16(ar0430, 0x030c, 0x1871, &ret); // MIPI_INT_PHY_PLL
	ar0430_write16(ar0430, 0x030E, 0x03, &ret); //PLL_CTRL

	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);

	return 0;
}

/* corrections recommended in ar0430 ini file */
static int ar0430_corrections(struct ar0430 *ar0430)
{
	int ret = 0;

	dev_dbg(ar0430->dev, "In function [%s]: line %d", __func__, __LINE__);

	ar0430_write16(ar0430, 0x3042, 0x0004, &ret);
	ar0430_write16(ar0430, 0x3220, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3222, 0x6060, &ret);
	ar0430_write16(ar0430, 0x3C50, 0x0001, &ret);
	ar0430_write16(ar0430, 0x3C66, 0x0FFF, &ret);
	ar0430_write16(ar0430, 0x3C68, 0x000A, &ret);
	ar0430_write16(ar0430, 0x3C6A, 0x0028, &ret);
	ar0430_write16(ar0430, 0x3C6C, 0x0500, &ret);
	ar0430_write16(ar0430, 0x3C6E, 0xECA0, &ret);
	ar0430_write16(ar0430, 0x3C70, 0x003C, &ret);
	ar0430_write16(ar0430, 0x3C72, 0x00A8, &ret);
	ar0430_write16(ar0430, 0x3C74, 0x000A, &ret);
	ar0430_write16(ar0430, 0x3C76, 0x0002, &ret);
	ar0430_write16(ar0430, 0x3C78, 0x0003, &ret);
	ar0430_write16(ar0430, 0x3C7A, 0x0004, &ret);
	ar0430_write16(ar0430, 0x3C7C, 0x0007, &ret);
	ar0430_write16(ar0430, 0x3C7E, 0x4020, &ret);
	ar0430_write16(ar0430, 0x3C80, 0x007B, &ret);
	ar0430_write16(ar0430, 0x3C82, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3C84, 0x0210, &ret);
	ar0430_write16(ar0430, 0x3C86, 0x000A, &ret);
	ar0430_write16(ar0430, 0x3C88, 0x0A0A, &ret);
	ar0430_write16(ar0430, 0x3C8A, 0x0A0A, &ret);
	ar0430_write16(ar0430, 0x3C8C, 0x1E1E, &ret);
	ar0430_write16(ar0430, 0x3C8E, 0x1E1E, &ret);
	ar0430_write16(ar0430, 0x3C90, 0x4444, &ret);
	ar0430_write16(ar0430, 0x3C92, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3C94, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3C96, 0x0010, &ret);
	ar0430_write16(ar0430, 0x3C98, 0x0FD7, &ret);
	ar0430_write16(ar0430, 0x3C9A, 0x0810, &ret);
	ar0430_write16(ar0430, 0x3C9E, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3C9C, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3CC2, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3CC4, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3CC6, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3CA0, 0x0000, &ret);
	ar0430_write16(ar0430, 0x3172, 0x0601, &ret);

	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);

	return 0;
}

static int ar0430_set_params(struct ar0430 *ar0430)
{
	struct v4l2_mbus_framefmt *format = &ar0430->format;
	const struct v4l2_rect *crop = &ar0430->crop;
	unsigned int xskip;
	unsigned int yskip;
	int ret = 0;

	/* Serial interface. */
	ar0430_write16(ar0430, AR0430_FRAME_PREAMBLE, 0x3B, &ret);
	ar0430_write16(ar0430, AR0430_LINE_PREAMBLE, 0x3B, &ret);
	ar0430_write16(ar0430, AR0430_MIPI_TIMING_0, 0x130C, &ret);
	ar0430_write16(ar0430, AR0430_MIPI_TIMING_1, 0x16CB, &ret);
	ar0430_write16(ar0430, AR0430_MIPI_TIMING_2, 0x1253, &ret);
	ar0430_write16(ar0430, AR0430_MIPI_TIMING_3, 0xB808, &ret);
	ar0430_write16(ar0430, AR0430_MIPI_TIMING_4, 0x4D3D, &ret);
	ret = ar0430_corrections(ar0430);

	/*
	 * Windows position and size.
	 *
	 * TODO: Make sure the start coordinates and window size match the
	 * skipping and mirroring requirements.
	 */

	ar0430_write16(ar0430, AR0430_X_ADDR_START, 0x0018, &ret);
	ar0430_write16(ar0430, AR0430_X_ADDR_END, 0x1216, &ret);
	ar0430_write16(ar0430, AR0430_Y_ADDR_START, 0x01C8, &ret);
	ar0430_write16(ar0430, AR0430_Y_ADDR_END, 0x0BE6, &ret);

	// from the .INI
	ar0430_write16(ar0430, 0x034C, 0x0900, &ret);
	ar0430_write16(ar0430, 0x034E, 0x0510, &ret);
	ar0430_write16(ar0430, 0x3040, 0x00C3, &ret);
	ar0430_write16(ar0430, 0x0342, 0x0B10, &ret);

	ar0430_write16(ar0430, 0x0340, 0x1B96, &ret);
	ar0430_write16(ar0430, 0x0202, 0x1B96, &ret);

	return ret;
}

/*
 * V4L2 subdev video operations
 */

extern int nk_pattern_count = 0;
static int ar0430_s_stream(struct v4l2_subdev *subdev, int enable)
{
	int r = 0;
	int g1 = 0;
	int b = 0;
	int g2 = 0;

	struct ar0430 *ar0430 = to_ar0430(subdev);
	int ret;

	dev_dbg(ar0430->dev, "%s: frame count is %d\n", __func__,
		ar0430_read16(ar0430, AR0430_FRAME_COUNT));

	if (!enable) {
		ret = ar0430_write8(ar0430, AR0430_MODE_SELECT, 0);
		pm_runtime_mark_last_busy(ar0430->dev);
		pm_runtime_put_autosuspend(ar0430->dev);

		mutex_lock(ar0430->ctrls.lock);
		ar0430->streaming = false;
		mutex_unlock(ar0430->ctrls.lock);

		return ret;
	}

	mutex_lock(ar0430->ctrls.lock);

	/*
	 * Set streaming to true early, protected by the controls lock, to
	 * ensure __v4l2_ctrl_handler_setup() will set the controls. The flag
	 * is reset to false further down if an error occurs.
	 */
	ar0430->streaming = true;

	ret = pm_runtime_get_sync(ar0430->dev);
	if (ret < 0)
		goto done;

	usleep_range(20000, 40000);
	ar0430_write16(ar0430, 0x0103, 0x01, &ret);
	usleep_range(5000, 10000);
	ar0430_write16(ar0430, 0x31E0, 0x0000, &ret);
	usleep_range(50000, 100000);

	ret = ar0430_analog_setup_recommended(ar0430);
	if (ret < 0)
		goto done;

	usleep_range(100000, 200000);

	ret = ar0430_pixel_timings_recommended_10bit(ar0430);
	if (ret < 0)
		goto done;

	ret = ar0430_pll_configure(ar0430);
	if (ret < 0)
		goto done;

	ret = __v4l2_ctrl_handler_setup(&ar0430->ctrls);
	if (ret < 0)
		goto done;

	ret = ar0430_set_params(ar0430);
	if (ret < 0)
		goto done;

	ar0430_write16(ar0430, AR0430_TEST_PATTERN_MODE, 0, &ret);

	ret = ar0430_write8(ar0430, AR0430_MODE_SELECT,
			    AR0430_MODE_SELECT_STREAM);

done:
	if (ret < 0) {
		/*
		 * In case of error, turn the power off synchronously as the
		 * device likely has no other chance to recover.
		 */
		dev_err(ar0430->dev, "%s: oups", __func__);
		pm_runtime_put_sync(ar0430->dev);
		ar0430->streaming = false;
	}

	mutex_unlock(ar0430->ctrls.lock);

	return ret;
}

/*
 * V4L2 subdev pad operations
 */

static struct v4l2_mbus_framefmt *
__ar0430_get_pad_format(struct ar0430 *ar0430,
			struct v4l2_subdev_state *sd_state, unsigned int pad,
			u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(&ar0430->subdev, sd_state,
						  pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &ar0430->format;
	default:
		return NULL;
	}
}

static struct v4l2_rect *
__ar0430_get_pad_crop(struct ar0430 *ar0430, struct v4l2_subdev_state *sd_state,
		      unsigned int pad, u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_crop(&ar0430->subdev, sd_state, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &ar0430->crop;
	default:
		return NULL;
	}
}

static int ar0430_init_cfg(struct v4l2_subdev *subdev,
			   struct v4l2_subdev_state *sd_state)
{
	u32 which =
		sd_state ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	struct ar0430 *ar0430 = to_ar0430(subdev);
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *crop;

	crop = __ar0430_get_pad_crop(ar0430, sd_state, 0, which);
	crop->left = (AR0430_WINDOW_WIDTH_MAX - AR0430_WINDOW_WIDTH_DEF) / 2 +
		     AR0430_WINDOW_LEFT_BORDER;
	crop->top = (AR0430_WINDOW_HEIGHT_MAX - AR0430_WINDOW_HEIGHT_DEF) / 2 +
		    AR0430_WINDOW_TOP_BORDER;
	crop->width = AR0430_WINDOW_WIDTH_DEF;
	crop->height = AR0430_WINDOW_HEIGHT_DEF;

	format = __ar0430_get_pad_format(ar0430, sd_state, 0, which);
	format->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	format->width = AR0430_WINDOW_WIDTH_DEF;
	format->height = AR0430_WINDOW_HEIGHT_DEF;
	format->field = V4L2_FIELD_NONE;
	format->colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

static int ar0430_enum_mbus_code(struct v4l2_subdev *subdev,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct ar0430 *ar0430 = to_ar0430(subdev);

	if (code->pad || code->index)
		return -EINVAL;

	code->code = ar0430->format.code;
	return 0;
}

static int ar0430_enum_frame_size(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct ar0430 *ar0430 = to_ar0430(subdev);

	if (fse->index >= 3 || fse->code != ar0430->format.code)
		return -EINVAL;

	fse->min_width = AR0430_WINDOW_WIDTH_DEF / (fse->index + 1);
	fse->max_width = fse->min_width;
	fse->min_height = AR0430_WINDOW_HEIGHT_DEF / (fse->index + 1);
	fse->max_height = fse->min_height;

	return 0;
}

static int ar0430_get_format(struct v4l2_subdev *subdev,
			     struct v4l2_subdev_state *sd_state,
			     struct v4l2_subdev_format *fmt)
{
	struct ar0430 *ar0430 = to_ar0430(subdev);

	fmt->format = *__ar0430_get_pad_format(ar0430, sd_state, fmt->pad,
					       fmt->which);
	return 0;
}

static int ar0430_set_format(struct v4l2_subdev *subdev,
			     struct v4l2_subdev_state *sd_state,
			     struct v4l2_subdev_format *format)
{
	struct ar0430 *ar0430 = to_ar0430(subdev);
	struct v4l2_mbus_framefmt *__format;
	struct v4l2_rect *__crop;
	unsigned int width;
	unsigned int height;
	unsigned int hratio;
	unsigned int vratio;

	__crop = __ar0430_get_pad_crop(ar0430, sd_state, format->pad,
				       format->which);

	/* Clamp the width and height to avoid dividing by zero. */
	width = clamp_t(unsigned int, ALIGN(format->format.width, 2),
			max(__crop->width / 3, AR0430_WINDOW_WIDTH_MIN),
			__crop->width);
	height = clamp_t(unsigned int, ALIGN(format->format.height, 2),
			 max(__crop->height / 3, AR0430_WINDOW_HEIGHT_MIN),
			 __crop->height);

	hratio = DIV_ROUND_CLOSEST(__crop->width, width);
	vratio = DIV_ROUND_CLOSEST(__crop->height, height);

	__format = __ar0430_get_pad_format(ar0430, sd_state, format->pad,
					   format->which);
	__format->width = __crop->width / hratio;
	__format->height = __crop->height / vratio;

	format->format = *__format;

	return 0;
}

static int ar0430_get_selection(struct v4l2_subdev *subdev,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct ar0430 *ar0430 = to_ar0430(subdev);

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		sel->r = *__ar0430_get_pad_crop(ar0430, sd_state, sel->pad,
						sel->which);
		break;

	case V4L2_SEL_TGT_CROP_DEFAULT:
		sel->r.left = AR0430_WINDOW_LEFT_BORDER;
		sel->r.top = AR0430_WINDOW_TOP_BORDER;
		sel->r.width = AR0430_WINDOW_WIDTH_MAX;
		sel->r.height = AR0430_WINDOW_HEIGHT_MAX;
		break;

	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = AR0430_PIXEL_ARRAY_WIDTH;
		sel->r.height = AR0430_PIXEL_ARRAY_HEIGHT;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int ar0430_set_selection(struct v4l2_subdev *subdev,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct ar0430 *ar0430 = to_ar0430(subdev);
	struct v4l2_mbus_framefmt *__format;
	struct v4l2_rect *__crop;
	struct v4l2_rect rect;

	if (sel->target != V4L2_SEL_TGT_CROP)
		return -EINVAL;

	/*
	 * Clamp the crop rectangle boundaries and align
	 * them to a multiple of 2
	 * pixels to ensure a GRBG Bayer pattern.
	 */
	rect.left = clamp_t(unsigned int, ALIGN(sel->r.left, 2),
			    AR0430_WINDOW_LEFT_BORDER,
			    AR0430_WINDOW_WIDTH_MAX - AR0430_WINDOW_WIDTH_MIN +
			    AR0430_WINDOW_LEFT_BORDER);
	rect.top = clamp_t(unsigned int, ALIGN(sel->r.top, 2),
			   AR0430_WINDOW_TOP_BORDER,
			   AR0430_WINDOW_HEIGHT_MAX - AR0430_WINDOW_HEIGHT_MIN +
			   AR0430_WINDOW_TOP_BORDER);
	rect.width = clamp(ALIGN(sel->r.width, 2), AR0430_WINDOW_WIDTH_MIN,
			   AR0430_WINDOW_WIDTH_MAX);
	rect.height = clamp(ALIGN(sel->r.height, 2), AR0430_WINDOW_HEIGHT_MIN,
			    AR0430_WINDOW_HEIGHT_MAX);

	rect.width =
		min(rect.width, AR0430_WINDOW_WIDTH_MAX +
				AR0430_WINDOW_LEFT_BORDER - rect.left);
	rect.height =
		min(rect.height, AR0430_WINDOW_HEIGHT_MAX +
				 AR0430_WINDOW_TOP_BORDER - rect.top);

	__crop = __ar0430_get_pad_crop(ar0430, sd_state, sel->pad, sel->which);

	if (rect.width != __crop->width || rect.height != __crop->height) {
		/*
		 * Reset the output image size if the crop rectangle size has
		 * been modified.
		 */
		__format = __ar0430_get_pad_format(ar0430, sd_state, sel->pad,
						   sel->which);
		__format->width = rect.width;
		__format->height = rect.height;
	}

	*__crop = rect;
	sel->r = rect;

	return 0;
}

/*
 * V4L2 subdev control operations
 */

static int ar0430_s_ctrl(struct v4l2_ctrl *ctrl)
{
	static const u16 test_pattern[] = { 0, 1, 4, 256, };
	struct ar0430 *ar0430 =
		container_of(ctrl->handler, struct ar0430, ctrls);
	int ret = 0;

	if (!ar0430->streaming)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		return ar0430_write16(ar0430, AR0430_COARSE_INTEGRATION_TIME,
				      ctrl->val, NULL);

	case V4L2_CID_GAIN:
		return ar0430_write16(ar0430, AR0430_GLOBAL_GAIN, ctrl->val,
				      NULL);
	case V4L2_CID_ANALOGUE_GAIN:
		return ar0430_write16(ar0430, AR0430_ANALOG_GAIN, ctrl->val,
				      NULL);
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP: {
		u16 clr = 0;
		u16 set = 0;

		if (ret < 0)
			return ret;

		if (ar0430->flip[0]->val)
			set |= AR0430_READ_MODE_HORIZ_MIRROR;
		else
			clr |= AR0430_READ_MODE_HORIZ_MIRROR;

		if (ar0430->flip[1]->val)
			set |= AR0430_READ_MODE_VERT_FLIP;
		else
			clr |= AR0430_READ_MODE_VERT_FLIP;

		ret = ar0430_set_read_mode(ar0430, clr, set);

		return ret;
	}

	case V4L2_CID_TEST_PATTERN:
		return ar0430_write16(ar0430, AR0430_TEST_PATTERN_MODE,
				      test_pattern[ctrl->val], NULL);
	}
	return 0;
}

static const struct v4l2_ctrl_ops ar0430_ctrl_ops = {
	.s_ctrl = ar0430_s_ctrl,
};

static const char *const ar0430_test_pattern_menu[] = {
	"Disabled",
	"Solid Grey",
	"PN9",
	"Walking 1s",
};

/*
 * V4L2 subdev operations
 */

static const struct v4l2_subdev_video_ops ar0430_subdev_video_ops = {
	.s_stream = ar0430_s_stream,
};

static const struct v4l2_subdev_pad_ops ar0430_subdev_pad_ops = {
	.init_cfg = ar0430_init_cfg,
	.enum_mbus_code = ar0430_enum_mbus_code,
	.enum_frame_size = ar0430_enum_frame_size,
	.get_fmt = ar0430_get_format,
	.set_fmt = ar0430_set_format,
	.get_selection = ar0430_get_selection,
	.set_selection = ar0430_set_selection,
};

static const struct v4l2_subdev_ops ar0430_subdev_ops = {
	.video = &ar0430_subdev_video_ops,
	.pad = &ar0430_subdev_pad_ops,
};

static int ar0430_v4l2_init(struct ar0430 *ar0430)
{
	struct v4l2_fwnode_device_properties props;
	struct v4l2_ctrl *ctrl;
	int ret;

	/* Parse the firmware sensor properties. */
	ret = v4l2_fwnode_device_parse(ar0430->dev, &props);
	if (ret) {
		dev_err(ar0430->dev, "Failed to parse fwnode properties: %d\n",
			ret);
		return ret;
	}

	/* Create V4L2 controls. */
	v4l2_ctrl_handler_init(&ar0430->ctrls, 10);

	v4l2_ctrl_new_std(&ar0430->ctrls, &ar0430_ctrl_ops, V4L2_CID_EXPOSURE,
			  AR0430_COARSE_INTEGRATION_TIME_MIN,
			  AR0430_COARSE_INTEGRATION_TIME_MAX, 1,
			  AR0430_COARSE_INTEGRATION_TIME_DEF);
	v4l2_ctrl_new_std(&ar0430->ctrls, &ar0430_ctrl_ops, V4L2_CID_GAIN,
			  AR0430_GLOBAL_GAIN_MIN, AR0430_GLOBAL_GAIN_MAX, 1,
			  AR0430_GLOBAL_GAIN_DEF);
	v4l2_ctrl_new_std(&ar0430->ctrls, &ar0430_ctrl_ops,
			  V4L2_CID_ANALOGUE_GAIN, AR0430_ANALOG_GAIN_MIN,
			  AR0430_ANALOG_GAIN_MAX, 1, AR0430_ANALOG_GAIN_DEF);
	ar0430->flip[0] = v4l2_ctrl_new_std(&ar0430->ctrls, &ar0430_ctrl_ops,
					    V4L2_CID_HFLIP, 0, 1, 1, 0);
	ar0430->flip[1] = v4l2_ctrl_new_std(&ar0430->ctrls, &ar0430_ctrl_ops,
					    V4L2_CID_VFLIP, 0, 1, 1, 0);
	ar0430->pixel_rate = v4l2_ctrl_new_std(&ar0430->ctrls, &ar0430_ctrl_ops,
					       V4L2_CID_PIXEL_RATE, 0, 0, 1, 0);
	v4l2_ctrl_new_std_menu_items(&ar0430->ctrls, &ar0430_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(ar0430_test_pattern_menu) - 1,
				     0, 0, ar0430_test_pattern_menu);

	ctrl = v4l2_ctrl_new_std(&ar0430->ctrls, &ar0430_ctrl_ops,
				 V4L2_CID_HBLANK, AR0430_HBLANK_DEF,
				 AR0430_HBLANK_DEF, 1, AR0430_HBLANK_DEF);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	ctrl = v4l2_ctrl_new_std(&ar0430->ctrls, &ar0430_ctrl_ops,
				 V4L2_CID_VBLANK, AR0430_VBLANK_DEF,
				 AR0430_VBLANK_DEF, 1, AR0430_VBLANK_DEF);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_cluster(ARRAY_SIZE(ar0430->flip), ar0430->flip);

	v4l2_ctrl_new_fwnode_properties(&ar0430->ctrls, &ar0430_ctrl_ops,
					&props);

	if (ar0430->ctrls.error) {
		dev_err(ar0430->dev, "%s: control initialization error %d\n",
			__func__, ar0430->ctrls.error);
		return ar0430->ctrls.error;
	}

	ar0430->subdev.ctrl_handler = &ar0430->ctrls;

	/* Initialize the media entity and V4L2 subdev. */
	ar0430->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&ar0430->subdev.entity, 1, &ar0430->pad);
	if (ret < 0)
		return ret;

	v4l2_i2c_subdev_init(&ar0430->subdev, ar0430->client,
			     &ar0430_subdev_ops);
	ar0430->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ar0430->subdev.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	ar0430_init_cfg(&ar0430->subdev, NULL);

	return 0;
}

/*
 * Power management
 */

static int ar0430_power_on(struct ar0430 *ar0430)
{
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ar0430->supplies),
				    ar0430->supplies);
	if (ret < 0) {
		dev_err(ar0430->dev, "Failed to enable power supplies: %d\n",
			ret);
		return ret;
	}

	ret = clk_prepare_enable(ar0430->clock);
	if (ret < 0) {
		dev_err(ar0430->dev, "Failed to enable clock: %d\n", ret);
		regulator_bulk_disable(ARRAY_SIZE(ar0430->supplies),
				       ar0430->supplies);
		return ret;
	}

	/* Assert reset for 1ms */
	if (ar0430->reset) {
		gpiod_set_value(ar0430->reset, 1);
		usleep_range(2000, 3000);
		gpiod_set_value(ar0430->reset, 0);
		usleep_range(10000, 11000);
	}

	return 0;
}

static void ar0430_power_off(struct ar0430 *ar0430)
{
	clk_disable_unprepare(ar0430->clock);
	regulator_bulk_disable(ARRAY_SIZE(ar0430->supplies), ar0430->supplies);
}

static int ar0430_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ar0430 *ar0430 = to_ar0430(subdev);
	int ret;

	ret = ar0430_power_on(ar0430);
	if (ret < 0)
		return ret;

	if (ret < 0) {
		ar0430_power_off(ar0430);
		return ret;
	}

	return 0;
}

static int ar0430_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ar0430 *ar0430 = to_ar0430(subdev);

	ar0430_power_off(ar0430);

	return 0;
}

static const struct dev_pm_ops ar0430_pm_ops = {
	SET_RUNTIME_PM_OPS(ar0430_runtime_suspend, ar0430_runtime_resume, NULL)
};

/*
 * Driver initialization and probing
 */

static int ar0430_identify(struct ar0430 *ar0430)
{
	s32 revision;
	s32 data;

	/* Read out the chip version register */
	data = ar0430_read16(ar0430, AR0430_CHIP_VERSION);
	if (data < 0)
		return data;

	if (data != AR0430_CHIP_VERSION_VALUE) {
		dev_err(ar0430->dev,
			"AR0430 not detected, wrong version 0x%04x\n", data);
		return -ENODEV;
	}

	revision = ar0430_read8(ar0430, AR0430_CHIP_REVISION);
	if (revision < 0)
		return revision;

	if (revision == 0x10)
		ar0430->version = 1;
	else if (revision != 0x20)
		ar0430->version = 2;
	else if (data == 0x0006)
		ar0430->version = 3;
	else if (data == 0x0007)
		ar0430->version = 4;
	else
		ar0430->version = 0;

	dev_info(ar0430->dev,
		 "AR0430 rev. %02x ver. %u detected at address 0x%02x\n",
		 revision, ar0430->version, ar0430->client->addr);

	return 0;
}

static int ar0430_probe(struct i2c_client *client)
{
	unsigned long rate = 24000000;
	struct ar0430 *ar0430;
	unsigned int i;
	int ret;

	ar0430 = kzalloc(sizeof(*ar0430), GFP_KERNEL);
	if (ar0430 == NULL)
		return -ENOMEM;

	ar0430->client = client;
	ar0430->dev = &client->dev;
	ar0430->read_mode = 0;

	/* Acquire resources. */
	ar0430->clock = devm_clk_get(ar0430->dev, NULL);
	if (IS_ERR(ar0430->clock)) {
		ret = PTR_ERR(ar0430->clock);
		if (ret != -EPROBE_DEFER)
			dev_err(ar0430->dev, "Failed to get clock: %d\n", ret);
		goto error_free;
	}

	rate = clk_round_rate(ar0430->clock, rate);
	ret = clk_set_rate(ar0430->clock, rate);
	if (ret < 0) {
		dev_err(ar0430->dev, "Failed to set clock rate: %d\n", ret);
		goto error_free;
	}

	dev_dbg(ar0430->dev, "clock rate set to %lu\n", rate);

	ar0430->reset =
		devm_gpiod_get_optional(ar0430->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ar0430->reset)) {
		ret = PTR_ERR(ar0430->reset);
		if (ret != -EPROBE_DEFER)
			dev_err(ar0430->dev, "Failed to get reset GPIO: %d\n",
				ret);
		goto error_free;
	}

	for (i = 0; i < ARRAY_SIZE(ar0430->supplies); ++i)
		ar0430->supplies[i].supply = ar0430_supply_names[i];

	ret = devm_regulator_bulk_get(ar0430->dev, ARRAY_SIZE(ar0430->supplies),
				      ar0430->supplies);
	if (ret < 0) {
		if (ret != -EPROBE_DEFER)
			dev_err(ar0430->dev,
				"Failed to get power supplies: %d\n", ret);
		goto error_free;
	}

	/*
	 * Enable power management. The driver supports runtime PM, but needs
	 * to work when runtime PM is disabled in the kernel. To that end,
	 * power the sensor on manually here, identify it, and fully
	 * initialize it.
	 */
	ret = ar0430_power_on(ar0430);
	if (ret < 0)
		goto error_free;

	ret = ar0430_identify(ar0430);
	if (ret < 0)
		goto error_power;

	if (ret < 0)
		goto error_power;

	/* Init the V4L2 subdev and controls. */
	ret = ar0430_v4l2_init(ar0430);
	if (ret < 0)
		goto error_media;

	/*
	 * Enable runtime PM. As the device has been powered manually, mark it
	 * as active, and increase the usage count without resuming the device.
	 */
	pm_runtime_set_active(ar0430->dev);
	pm_runtime_get_noresume(ar0430->dev);
	pm_runtime_enable(ar0430->dev);

	ret = v4l2_async_register_subdev(&ar0430->subdev);
	if (ret < 0) {
		dev_err(ar0430->dev, "Subdev registration failed\n");
		goto error_pm;
	}

	/*
	 * Finally, enable autosuspend and decrease the usage count. The device
	 * will get suspended after the autosuspend delay, turning the power
	 * off.
	 */
	pm_runtime_set_autosuspend_delay(ar0430->dev, 1000);
	pm_runtime_use_autosuspend(ar0430->dev);
	pm_runtime_put_autosuspend(ar0430->dev);

	return 0;

error_pm:
	pm_runtime_disable(ar0430->dev);
	pm_runtime_put_noidle(ar0430->dev);
error_media:
	media_entity_cleanup(&ar0430->subdev.entity);
	v4l2_ctrl_handler_free(&ar0430->ctrls);
error_power:
	ar0430_power_off(ar0430);
error_free:
	kfree(ar0430);

	return ret;
}

static void ar0430_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ar0430 *ar0430 = to_ar0430(subdev);

	v4l2_ctrl_handler_free(&ar0430->ctrls);
	v4l2_async_unregister_subdev(subdev);
	media_entity_cleanup(&subdev->entity);

	/*
	 * Disable runtime PM. In case runtime PM is disabled in the kernel,
	 * make sure to turn power off manually.
	 */
	pm_runtime_disable(ar0430->dev);
	if (!pm_runtime_status_suspended(ar0430->dev))
		ar0430_power_off(ar0430);
	pm_runtime_set_suspended(ar0430->dev);

	kfree(ar0430);

}

static const struct i2c_device_id ar0430_id[] = {
	{ "ar0430", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ar0430_id);

static const struct of_device_id ar0430_of_id[] = {
	{ .compatible = "onnn,ar0430" },
	{}
};
MODULE_DEVICE_TABLE(of, ar0430_of_id);

static struct i2c_driver ar0430_i2c_driver = {
	.driver = {
		.name = "ar0430",
		.of_match_table = ar0430_of_id,
		.pm = &ar0430_pm_ops,
	},
	.probe          = ar0430_probe,
	.remove         = ar0430_remove,
	.id_table       = ar0430_id,
};

module_i2c_driver(ar0430_i2c_driver);

MODULE_DESCRIPTION("Aptina AR0430 Camera driver");
MODULE_AUTHOR("Laurent Pinchart <laurent.pinchart@ideasonboard.com>");
MODULE_LICENSE("GPL");
