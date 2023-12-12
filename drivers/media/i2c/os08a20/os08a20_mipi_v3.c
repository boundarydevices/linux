/*
 * Copyright (C) 2012-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2018 NXP
 * Copyright (c) 2020 VeriSilicon Holdings Co., Ltd.
 */
/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of_graph.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/v4l2-mediabus.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include "vvsensor.h"

#include "os08a20_regs_1080p.h"
#include "os08a20_regs_1080p_hdr.h"
#include "os08a20_regs_4k.h"
#include "os08a20_regs_4k_hdr.h"

#define OS08A20_VOLTAGE_ANALOG			2800000
#define OS08A20_VOLTAGE_DIGITAL_CORE		1500000
#define OS08A20_VOLTAGE_DIGITAL_IO		1800000

#define OS08A20_XCLK_MIN 6000000
#define OS08A20_XCLK_MAX 24000000

#define OS08A20_SENS_PAD_SOURCE	0
#define OS08A20_SENS_PADS_NUM	1

#define client_to_os08a20(client)\
	container_of(i2c_get_clientdata(client), struct os08a20, subdev)

/*
Use USER_TO_KERNEL/KERNEL_TO_USER to fix "uaccess" exception on run time.
Also, use "copy_ret" to fix the build issue as below.
error: ignoring return value of function declared with 'warn_unused_result' attribute.
*/

#ifdef CONFIG_HARDENED_USERCOPY
#define USER_TO_KERNEL(TYPE) \
	do {\
		TYPE tmp; \
		unsigned long copy_ret; \
		arg = (void *)(&tmp); \
		copy_ret = copy_from_user(arg, arg_user, sizeof(TYPE));\
	} while (0)

#define KERNEL_TO_USER(TYPE) \
	do {\
		unsigned long copy_ret; \
		copy_ret = copy_to_user(arg_user, arg, sizeof(TYPE));\
	} while (0)

#define USER_TO_KERNEL_VMALLOC(TYPE) \
	do {\
		unsigned long copy_ret; \
		arg = vmalloc(sizeof(TYPE)); \
		copy_ret = copy_from_user(arg, arg_user, sizeof(TYPE));\
	} while (0)

#define KERNEL_TO_USER_VMALLOC(TYPE) \
	do {\
		unsigned long copy_ret; \
		copy_ret = copy_to_user(arg_user, arg, sizeof(TYPE));\
		vfree(arg); \
	} while (0)

#else
#define USER_TO_KERNEL(TYPE)
#define KERNEL_TO_USER(TYPE)
#define USER_TO_KERNEL_VMALLOC(TYPE)
#define KERNEL_TO_USER_VMALLOC(TYPE)
#endif


struct os08a20_capture_properties {
	__u64 max_lane_frequency;
	__u64 max_pixel_frequency;
	__u64 max_data_rate;
};

struct os08a20_ctrls {
	struct v4l2_ctrl_handler handler;
	struct v4l2_ctrl *link_freq;
};

struct os08a20 {
	struct i2c_client *i2c_client;
	struct regulator *io_regulator;
	struct regulator *core_regulator;
	struct regulator *analog_regulator;
	unsigned int pwn_gpio;
	unsigned int rst_gpio;
	unsigned int mclk;
	unsigned int mclk_source;
	struct clk *sensor_clk;
	unsigned int csi_id;
	struct os08a20_capture_properties ocp;

	struct v4l2_subdev subdev;
	struct media_pad pads[OS08A20_SENS_PADS_NUM];

	struct v4l2_mbus_framefmt format;
	vvcam_mode_info_t cur_mode;
	sensor_blc_t blc;
	sensor_white_balance_t wb;
	struct mutex lock;
	u32 stream_status;
	u32 resume_status;
	struct os08a20_ctrls ctrls;
};

static struct vvcam_mode_info_s pos08a20_mode_info[] = {
	{
		.index	        = 0,
		.size           = {
			.bounds_width  = 1920,
			.bounds_height = 1080,
			.top           = 0,
			.left          = 0,
			.width         = 1920,
			.height        = 1080,
		},
		.hdr_mode       = SENSOR_MODE_LINEAR,
		.bit_width      = 10,
		.data_compress  = {
			.enable = 0,
		},
		.bayer_pattern  = BAYER_BGGR,
		.ae_info = {
			.def_frm_len_lines     = 0x4a4,
			.curr_frm_len_lines    = 0x4a4,
			.one_line_exp_time_ns  = 14029,
			.max_integration_line  = 0x4a4 - 8,
			.min_integration_line  = 8,
			.max_again             = 15.5 * (1 << SENSOR_FIX_FRACBITS),
			.min_again             = 1    * (1 << SENSOR_FIX_FRACBITS),
			.max_dgain             = 4    * (1 << SENSOR_FIX_FRACBITS),
			.min_dgain             = 1    * (1 << SENSOR_FIX_FRACBITS),
			.start_exposure        = 8 * 100 * (1 << SENSOR_FIX_FRACBITS),
			.cur_fps               = 60   * (1 << SENSOR_FIX_FRACBITS),
			.max_fps               = 60   * (1 << SENSOR_FIX_FRACBITS),
			.min_fps               = 1    * (1 << SENSOR_FIX_FRACBITS),
			.min_afps              = 5    * (1 << SENSOR_FIX_FRACBITS),
			.int_update_delay_frm  = 1,
			.gain_update_delay_frm = 1,
		},
		.mipi_info = {
			.mipi_lane = 4,
		},
		.preg_data      = os08a20_init_setting_1080p,
		.reg_data_count = ARRAY_SIZE(os08a20_init_setting_1080p),
	},
	{
		.index	        = 1,
		.size           = {
			.bounds_width  = 1920,
			.bounds_height = 1080,
			.top           = 0,
			.left          = 0,
			.width         = 1920,
			.height        = 1080,
		},
		.hdr_mode       = SENSOR_MODE_HDR_STITCH,
		.stitching_mode = SENSOR_STITCHING_L_AND_S,
		.bit_width      = 10,
		.data_compress  = {
			.enable = 0,
		},
		.bayer_pattern  = BAYER_BGGR,
		.ae_info = {
			.def_frm_len_lines      = 0x492,
			.curr_frm_len_lines     = 0x492,
			.one_line_exp_time_ns   = 14250,

			.max_integration_line   = 0x400,
			.min_integration_line   = 8 * 16,
			.max_vsintegration_line = 56,
			.min_vsintegration_line = 8,

			.max_again             = 15.5 * (1 << SENSOR_FIX_FRACBITS),
			.min_again             = 1    * (1 << SENSOR_FIX_FRACBITS),
			.max_dgain             = 4    * (1 << SENSOR_FIX_FRACBITS),
			.min_dgain             = 1    * (1 << SENSOR_FIX_FRACBITS),

			.max_short_again       = 15.5 * (1 << SENSOR_FIX_FRACBITS),
			.min_short_again       = 1    * (1 << SENSOR_FIX_FRACBITS),
			.max_short_dgain       = 4    * (1 << SENSOR_FIX_FRACBITS),
			.min_short_dgain       = 1    * (1 << SENSOR_FIX_FRACBITS),

			.start_exposure        = 8 * 800 * (1 << SENSOR_FIX_FRACBITS),
			.cur_fps               = 30 * (1 << SENSOR_FIX_FRACBITS),
			.max_fps               = 30 * (1 << SENSOR_FIX_FRACBITS),
			.min_fps               = 1  * (1 << SENSOR_FIX_FRACBITS),
			.min_afps              = 5  * (1 << SENSOR_FIX_FRACBITS),
			.hdr_ratio             = {
				.ratio_l_s     = 0,
				.ratio_s_vs    = 8 * (1 << SENSOR_FIX_FRACBITS),
				.accuracy      = (1 << SENSOR_FIX_FRACBITS),
			},
			.int_update_delay_frm  = 1,
			.gain_update_delay_frm = 1,
		},
		.mipi_info = {
			.mipi_lane = 4,
		},
		.preg_data      = os08a20_init_setting_1080p_hdr,
		.reg_data_count = ARRAY_SIZE(os08a20_init_setting_1080p_hdr),
	},
	{
		.index	        = 2,
		.size           = {
			.bounds_width  = 3840,
			.bounds_height = 2160,
			.top           = 0,
			.left          = 0,
			.width         = 3840,
			.height        = 2160,
		},
		.hdr_mode       = SENSOR_MODE_LINEAR,
		.bit_width      = 12,
		.data_compress  = {
			.enable = 0,
		},
		.bayer_pattern  = BAYER_BGGR,
		.ae_info = {
			.def_frm_len_lines     = 0x8f0,
			.curr_frm_len_lines    = 0x8f0,
			.one_line_exp_time_ns  = 14563,
			.max_integration_line  = 0x8f0 - 8,
			.min_integration_line  = 8,

			.max_again             = 15.5 * (1 << SENSOR_FIX_FRACBITS),
			.min_again             = 1    * (1 << SENSOR_FIX_FRACBITS),
			.max_dgain             = 4    * (1 << SENSOR_FIX_FRACBITS),
			.min_dgain             = 1    * (1 << SENSOR_FIX_FRACBITS),

			.start_exposure        = 8 * 100 * (1 << SENSOR_FIX_FRACBITS),
			.cur_fps               = 30 * (1 << SENSOR_FIX_FRACBITS),
			.max_fps               = 30 * (1 << SENSOR_FIX_FRACBITS),
			.min_fps               = 1  * (1 << SENSOR_FIX_FRACBITS),
			.min_afps              = 5  * (1 << SENSOR_FIX_FRACBITS),
			.int_update_delay_frm  = 1,
			.gain_update_delay_frm = 1,
		},
		.mipi_info = {
			.mipi_lane = 4,
		},
		.preg_data      = os08a20_init_setting_4k,
		.reg_data_count = ARRAY_SIZE(os08a20_init_setting_4k),
	},
	{
		.index	        = 3,
		.size           = {
			.bounds_width  = 3840,
			.bounds_height = 2160,
			.top           = 0,
			.left          = 0,
			.width         = 3840,
			.height        = 2160,
		},
		.hdr_mode       = SENSOR_MODE_HDR_STITCH,
		.stitching_mode = SENSOR_STITCHING_L_AND_S,
		.bit_width      = 10,
		.data_compress  = {
			.enable = 0,
		},
		.bayer_pattern  = BAYER_BGGR,
		.ae_info = {
			.def_frm_len_lines      = 0x90a,
			.curr_frm_len_lines     = 0x90a,
			.one_line_exp_time_ns   = 14389,

			.max_integration_line   = 0x800,
			.min_integration_line   = 8 * 16,
			.max_vsintegration_line = 56,
			.min_vsintegration_line = 8,

			.max_again             = 15.5 * (1 << SENSOR_FIX_FRACBITS),
			.min_again             = 1    * (1 << SENSOR_FIX_FRACBITS),
			.max_dgain             = 4    * (1 << SENSOR_FIX_FRACBITS),
			.min_dgain             = 1    * (1 << SENSOR_FIX_FRACBITS),

			.max_short_again       = 15.5 * (1 << SENSOR_FIX_FRACBITS),
			.min_short_again       = 1    * (1 << SENSOR_FIX_FRACBITS),
			.max_short_dgain       = 4    * (1 << SENSOR_FIX_FRACBITS),
			.min_short_dgain       = 1    * (1 << SENSOR_FIX_FRACBITS),

			.start_exposure        = 8 * 800 * (1 << SENSOR_FIX_FRACBITS),
			.cur_fps               = 15 * (1 << SENSOR_FIX_FRACBITS),
			.max_fps               = 15 * (1 << SENSOR_FIX_FRACBITS),
			.min_fps               = 1  * (1 << SENSOR_FIX_FRACBITS),
			.min_afps              = 5  * (1 << SENSOR_FIX_FRACBITS),
			.hdr_ratio             = {
				.ratio_l_s     = 0,
				.ratio_s_vs    = 8 * (1 << SENSOR_FIX_FRACBITS),
				.accuracy      = (1 << SENSOR_FIX_FRACBITS),
			},
			.int_update_delay_frm  = 1,
			.gain_update_delay_frm = 1,
		},
		.mipi_info = {
			.mipi_lane = 4,
		},
		.preg_data      = os08a20_init_setting_4k_hdr,
		.reg_data_count = ARRAY_SIZE(os08a20_init_setting_4k_hdr),
	},
};

static int os08a20_power_on(struct os08a20 *sensor)
{
	int ret;
	pr_debug("enter %s\n", __func__);

	if (gpio_is_valid(sensor->pwn_gpio))
		gpio_set_value_cansleep(sensor->pwn_gpio, 1);

	ret = clk_prepare_enable(sensor->sensor_clk);
	if (ret < 0)
		pr_err("%s: enable sensor clk fail\n", __func__);

	return ret;
}

static int os08a20_power_off(struct os08a20 *sensor)
{
	pr_debug("enter %s\n", __func__);
	if (gpio_is_valid(sensor->pwn_gpio))
		gpio_set_value_cansleep(sensor->pwn_gpio, 0);
	clk_disable_unprepare(sensor->sensor_clk);

	return 0;
}

static int os08a20_runtime_suspend(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct os08a20 *sensor = client_to_os08a20(client);

	pr_debug("enter %s\n", __func__);
	return os08a20_power_off(sensor);
}

static int os08a20_runtime_resume(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct os08a20 *sensor = client_to_os08a20(client);

	pr_debug("enter %s\n", __func__);
	return os08a20_power_on(sensor);
}

static int os08a20_get_clk(struct os08a20 *sensor, void *clk)
{
	struct vvcam_clk_s vvcam_clk;
	int ret = 0;
	vvcam_clk.sensor_mclk = clk_get_rate(sensor->sensor_clk);
	vvcam_clk.csi_max_pixel_clk = sensor->ocp.max_pixel_frequency;
	ret = copy_to_user(clk, &vvcam_clk, sizeof(struct vvcam_clk_s));
	if (ret != 0)
		ret = -EINVAL;
	return ret;
}

static int os08a20_write_reg(struct os08a20 *sensor, u16 reg, u8 val)
{
	struct device *dev = &sensor->i2c_client->dev;
	u8 au8Buf[3] = { 0 };

	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xff;
	au8Buf[2] = val;

	if (i2c_master_send(sensor->i2c_client, au8Buf, 3) < 0) {
		dev_err(dev, "Write reg error: reg=%x, val=%x\n", reg, val);
		return -1;
	}

	return 0;
}

static int os08a20_read_reg(struct os08a20 *sensor, u16 reg, u8 *val)
{
	struct device *dev = &sensor->i2c_client->dev;
	u8 au8RegBuf[2] = { 0 };
	u8 u8RdVal = 0;

	au8RegBuf[0] = reg >> 8;
	au8RegBuf[1] = reg & 0xff;

	if (i2c_master_send(sensor->i2c_client, au8RegBuf, 2) != 2) {
		dev_err(dev, "Read reg error: reg=%x\n", reg);
		return -1;
	}

	if (i2c_master_recv(sensor->i2c_client, &u8RdVal, 1) != 1) {
		dev_err(dev, "Read reg error: reg=%x, val=%x\n", reg, u8RdVal);
		return -1;
	}

	*val = u8RdVal;

	return 0;
}

static int os08a20_write_reg_arry(struct os08a20 *sensor,
				  struct vvcam_sccb_data_s *reg_arry,
				  u32 size)
{
	int i = 0;
	int ret = 0;
	struct i2c_msg msg;
	u8 *send_buf;
	u32 send_buf_len = 0;
	struct i2c_client *i2c_client = sensor->i2c_client;

	send_buf = (u8 *)kmalloc(size + 2, GFP_KERNEL);
	if (!send_buf)
		return -ENOMEM;

	send_buf[send_buf_len++] = (reg_arry[0].addr >> 8) & 0xff;
	send_buf[send_buf_len++] = reg_arry[0].addr & 0xff;
	send_buf[send_buf_len++] = reg_arry[0].data & 0xff;
	for (i=1; i < size; i++) {
		if (reg_arry[i].addr == (reg_arry[i-1].addr + 1)){
			send_buf[send_buf_len++] = reg_arry[i].data & 0xff;
		} else {
			msg.addr  = i2c_client->addr;
			msg.flags = i2c_client->flags;
			msg.buf   = send_buf;
			msg.len   = send_buf_len;
			ret = i2c_transfer(i2c_client->adapter, &msg, 1);
			if (ret < 0) {
				pr_err("%s:i2c transfer error\n",__func__);
				kfree(send_buf);
				return ret;
			}
			send_buf_len = 0;
			send_buf[send_buf_len++] =
				(reg_arry[i].addr >> 8) & 0xff;
			send_buf[send_buf_len++] =
				reg_arry[i].addr & 0xff;
			send_buf[send_buf_len++] =
				reg_arry[i].data & 0xff;
		}
	}

	if (send_buf_len > 0) {
		msg.addr  = i2c_client->addr;
		msg.flags = i2c_client->flags;
		msg.buf   = send_buf;
		msg.len   = send_buf_len;
		ret = i2c_transfer(i2c_client->adapter, &msg, 1);
		if (ret < 0)
			pr_err("%s:i2c transfer end meg error\n",__func__);
		else
			ret = 0;

	}
	kfree(send_buf);
	return ret;
}

static int os08a20_query_capability(struct os08a20 *sensor, void *arg)
{
	struct v4l2_capability *pcap = (struct v4l2_capability *)arg;

	strcpy((char *)pcap->driver, "os08a20");
	sprintf((char *)pcap->bus_info, "csi%d",sensor->csi_id);
	if(sensor->i2c_client->adapter) {
		pcap->bus_info[VVCAM_CAP_BUS_INFO_I2C_ADAPTER_NR_POS] =
			(__u8)sensor->i2c_client->adapter->nr;
	} else {
		pcap->bus_info[VVCAM_CAP_BUS_INFO_I2C_ADAPTER_NR_POS] = 0xFF;
	}

	return 0;
}

static int os08a20_query_supports(struct os08a20 *sensor, void* parry)
{
	struct vvcam_mode_info_array_s *psensor_mode_arry = parry;

	psensor_mode_arry->count = ARRAY_SIZE(pos08a20_mode_info);
	memcpy((void *)&psensor_mode_arry->modes, (void *)pos08a20_mode_info, sizeof(pos08a20_mode_info));

	return 0;
}

static int os08a20_get_sensor_id(struct os08a20 *sensor, void* pchip_id)
{
	int ret = 0;
	u32 chip_id;
	u8 chip_id_high = 0;
	u8 chip_id_middle = 0;
	u8 chip_id_low = 0;
	ret  = os08a20_read_reg(sensor, 0x300a, &chip_id_high);
	ret |= os08a20_read_reg(sensor, 0x300b, &chip_id_middle);
	ret |= os08a20_read_reg(sensor, 0x300c, &chip_id_low);

	chip_id = ((chip_id_high & 0xff) << 16) | 
	        ((chip_id_middle & 0xff) << 8) | (chip_id_low & 0xff);

	ret = copy_to_user(pchip_id, &chip_id, sizeof(u32));
	if (ret != 0)
		ret = -ENOMEM;
	return ret;
}

static int os08a20_get_reserve_id(struct os08a20 *sensor, void* preserve_id)
{
	int ret = 0;
	u32 reserve_id = 0x530841;
	ret = copy_to_user(preserve_id, &reserve_id, sizeof(u32));
	if (ret != 0)
		ret = -ENOMEM;
	return ret;
}

static int os08a20_get_sensor_mode(struct os08a20 *sensor, void* pmode)
{
	int ret = 0;
	ret = copy_to_user(pmode, &sensor->cur_mode,
		sizeof(struct vvcam_mode_info_s));
	if (ret != 0)
		ret = -ENOMEM;
	return ret;
}

static int os08a20_set_sensor_mode(struct os08a20 *sensor, void* pmode)
{
	int ret = 0;
	int i = 0;
	struct vvcam_mode_info_s sensor_mode;
	ret = copy_from_user(&sensor_mode, pmode,
		sizeof(struct vvcam_mode_info_s));
	if (ret != 0)
		return -ENOMEM;
	for (i = 0; i < ARRAY_SIZE(pos08a20_mode_info); i++) {
		if (pos08a20_mode_info[i].index == sensor_mode.index) {
			memcpy(&sensor->cur_mode, &pos08a20_mode_info[i],
				sizeof(struct vvcam_mode_info_s));
			return 0;
		}
	}

	return -ENXIO;
}

static int os08a20_set_exp(struct os08a20 *sensor, u32 exp)
{
	int ret = 0;

	ret |= os08a20_write_reg(sensor, 0x3501, (exp >> 8) & 0xff);
	ret |= os08a20_write_reg(sensor, 0x3502, exp & 0xff);

	return ret;
}

static int os08a20_set_vsexp(struct os08a20 *sensor, u32 exp)
{
	int ret = 0;

	ret |= os08a20_write_reg(sensor, 0x3511, (exp >> 8) & 0xff);
	ret |= os08a20_write_reg(sensor, 0x3512, exp & 0xff);

	return ret;
}

static int os08a20_set_gain(struct os08a20 *sensor, u32 total_gain)
{
	int ret = 0;
	u32 again = 0;
	u32 dgain = 0;

	if (total_gain < (1 << SENSOR_FIX_FRACBITS)) {
		again = 0x80;
		dgain = 0x400;
	} else if (total_gain < 2 * (1 << SENSOR_FIX_FRACBITS)) {
		again = ((total_gain * 16) / 0x400) * 8;
		dgain =  total_gain * 128 / again;
	} else if (total_gain < 4 * (1 << SENSOR_FIX_FRACBITS)) {
		again = ((total_gain * 8) / 0x400) * 16;
		dgain =  total_gain * 128 / again;
	} else if (total_gain < 8 * (1 << SENSOR_FIX_FRACBITS)) {
		again = ((total_gain * 4) / 0x400) * 32;
		dgain =  total_gain * 128 / again;
	} else if (total_gain < 16 * (1 << SENSOR_FIX_FRACBITS)){
		again = ((total_gain * 2) / 0x400) * 64;
		dgain =  total_gain * 128 / again;
	} else {
		again = 0x7c0;
		dgain =  total_gain * 128 / again;
	}

	ret |= os08a20_write_reg(sensor, 0x3508, (again >> 8) & 0xff);
	ret |= os08a20_write_reg(sensor, 0x3509, again & 0xff);

	ret |= os08a20_write_reg(sensor, 0x350a, (dgain >> 8) & 0xff);
	ret |= os08a20_write_reg(sensor, 0x350b, dgain & 0x00FF);

	return ret;
}

static int os08a20_set_vsgain(struct os08a20 *sensor, u32 total_gain)
{
	int ret = 0;
	u32 again = 0;
	u32 dgain = 0;

	if (total_gain < (1 << SENSOR_FIX_FRACBITS)) {
		again = 0x80;
		dgain = 0x400;
	} else if (total_gain < 2 * (1 << SENSOR_FIX_FRACBITS)) {
		again = ((total_gain * 16) / 0x400) * 8;
		dgain =  total_gain * 128 / again;
	} else if (total_gain < 4 * (1 << SENSOR_FIX_FRACBITS)) {
		again = ((total_gain * 8) / 0x400) * 16;
		dgain =  total_gain * 128 / again;
	} else if (total_gain < 8 * (1 << SENSOR_FIX_FRACBITS)) {
		again = ((total_gain * 4) / 0x400) * 32;
		dgain =  total_gain * 128 / again;
	} else if (total_gain < 16 * (1 << SENSOR_FIX_FRACBITS)){
		again = ((total_gain * 2) / 0x400) * 64;
		dgain =  total_gain * 128 / again;
	} else {
		again = 0x7c0;
		dgain =  total_gain * 128 / again;
	}

	ret |= os08a20_write_reg(sensor, 0x350c, (again >> 8) & 0xff);
	ret |= os08a20_write_reg(sensor, 0x350d, again & 0xff);

	ret |= os08a20_write_reg(sensor, 0x350e, (dgain >> 8) & 0xff);
	ret |= os08a20_write_reg(sensor, 0x350f, dgain & 0x00FF);

	return ret;
}

static int os08a20_set_fps(struct os08a20 *sensor, u32 fps)
{
	u32 vts;
	int ret = 0;

	if (fps > sensor->cur_mode.ae_info.max_fps) {
		fps = sensor->cur_mode.ae_info.max_fps;
	}
	else if (fps < sensor->cur_mode.ae_info.min_fps) {
		fps = sensor->cur_mode.ae_info.min_fps;
	}
	vts = sensor->cur_mode.ae_info.max_fps *
	      sensor->cur_mode.ae_info.def_frm_len_lines / fps;

	ret = os08a20_write_reg(sensor, 0x380e, (u8)(vts >> 8) & 0xff);
	ret |= os08a20_write_reg(sensor, 0x380f, (u8)(vts & 0xff));

	sensor->cur_mode.ae_info.cur_fps = fps;

	if (sensor->cur_mode.hdr_mode == SENSOR_MODE_LINEAR) {
		sensor->cur_mode.ae_info.max_integration_line = vts - 8;
	} else {
		sensor->cur_mode.ae_info.max_integration_line =
			vts - sensor->cur_mode.ae_info.max_vsintegration_line - 8;
	}
	sensor->cur_mode.ae_info.curr_frm_len_lines = vts;
	return ret;
}

static int os08a20_get_fps(struct os08a20 *sensor, u32 *pfps)
{
	*pfps = sensor->cur_mode.ae_info.cur_fps;
	return 0;
}

static int os08a20_set_ratio(struct os08a20 *sensor, void* pratio)
{
	int ret = 0;
	struct sensor_hdr_artio_s hdr_ratio;
	struct vvcam_ae_info_s *pae_info = &sensor->cur_mode.ae_info;

	ret = copy_from_user(&hdr_ratio, pratio, sizeof(hdr_ratio));

	if (hdr_ratio.ratio_s_vs != pae_info->hdr_ratio.ratio_s_vs) {
		pae_info->hdr_ratio.ratio_s_vs = hdr_ratio.ratio_s_vs;
		if (sensor->cur_mode.hdr_mode != SENSOR_MODE_LINEAR) {
			sensor->cur_mode.ae_info.max_integration_line =
				pae_info->curr_frm_len_lines - pae_info->max_vsintegration_line - 8;
		}
	}

	return 0;
}

static int os08a20_set_test_pattern(struct os08a20 *sensor, void * arg)
{
	int ret;
	struct sensor_test_pattern_s test_pattern;

	ret = copy_from_user(&test_pattern, arg, sizeof(test_pattern));
	if (ret != 0)
		return -ENOMEM;
	if (test_pattern.enable) {
		switch (test_pattern.pattern) {
		case 0:
			os08a20_write_reg(sensor, 0x5081, 0x80);
			break;
		case 1:
			os08a20_write_reg(sensor, 0x5081, 0x84);
			break;
		case 2:
			os08a20_write_reg(sensor, 0x5081, 0x82);
			break;
		case 3:
			os08a20_write_reg(sensor, 0x5081, 0x92);
			break;
		case 4:
			os08a20_write_reg(sensor, 0x5081, 0xc4);
			break;
		default:
			ret = -1;
			break;
		}
	} else {
		os08a20_write_reg(sensor, 0x5081, 0x00);	
	}
	return ret;
}

static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct os08a20,
			     ctrls.handler)->subdev;
}

static int os08a20_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_ctrl_ops os08a20_ctrl_ops = {
	.s_ctrl = os08a20_s_ctrl,
};

/*
 * MIPI CSI-2 link frequencies.
 * link_freq = (pixel_rate * bpp) / (2 * data_lanes)
 */
static const s64 os08a20_csi2_link_freqs[] = {
	200000000,
};

/* Link freq for default mode: 1080p RAW10, 4 data lanes 800 Mbps/lane. */
#define OS08A20_DEFAULT_LINK_FREQ	0

static int os08a20_init_controls(struct os08a20 *sensor)
{
	const struct v4l2_ctrl_ops *ops = &os08a20_ctrl_ops;
	struct os08a20_ctrls *ctrls = &sensor->ctrls;
	struct v4l2_ctrl_handler *hdl = &ctrls->handler;
	int ret;

	v4l2_ctrl_handler_init(hdl, 5);

	/* we can use our own mutex for the ctrl lock */
	hdl->lock = &sensor->lock;

	/* Clock related controls */
	ctrls->link_freq = v4l2_ctrl_new_int_menu(hdl, ops,
					V4L2_CID_LINK_FREQ,
					ARRAY_SIZE(os08a20_csi2_link_freqs) - 1,
					OS08A20_DEFAULT_LINK_FREQ,
					os08a20_csi2_link_freqs);

	if (hdl->error) {
		ret = hdl->error;
		goto free_ctrls;
	}

	ctrls->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	sensor->subdev.ctrl_handler = hdl;
	return 0;

free_ctrls:
	v4l2_ctrl_handler_free(hdl);
	return ret;
}

static int os08a20_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct os08a20 *sensor = client_to_os08a20(client);
	int ret = 0;

	if (enable) {
		ret = pm_runtime_resume_and_get(&client->dev);
		if (ret < 0)
			return ret;
		os08a20_write_reg(sensor, 0x0100, 0x01);
	} else {
		os08a20_write_reg(sensor, 0x0100, 0x00);
	}

	sensor->stream_status = enable;

	if (!enable || ret) {
		pm_runtime_mark_last_busy(&sensor->i2c_client->dev);
		pm_runtime_put_autosuspend(&client->dev);
	}

	return 0;
}

static int os08a20_get_format_code(struct os08a20 *sensor, u32 *code)
{
	switch (sensor->cur_mode.bayer_pattern) {
	case BAYER_RGGB:
		if (sensor->cur_mode.bit_width == 8) {
			*code = MEDIA_BUS_FMT_SRGGB8_1X8;
		} else if (sensor->cur_mode.bit_width == 10) {
			*code = MEDIA_BUS_FMT_SRGGB10_1X10;
		} else {
			*code = MEDIA_BUS_FMT_SRGGB12_1X12;
		}
		break;
	case BAYER_GRBG:
		if (sensor->cur_mode.bit_width == 8) {
			*code = MEDIA_BUS_FMT_SGRBG8_1X8;
		} else if (sensor->cur_mode.bit_width == 10) {
			*code = MEDIA_BUS_FMT_SGRBG10_1X10;
		} else {
			*code = MEDIA_BUS_FMT_SGRBG12_1X12;
		}
		break;
	case BAYER_GBRG:
		if (sensor->cur_mode.bit_width == 8) {
			*code = MEDIA_BUS_FMT_SGBRG8_1X8;
		} else if (sensor->cur_mode.bit_width == 10) {
			*code = MEDIA_BUS_FMT_SGBRG10_1X10;
		} else {
			*code = MEDIA_BUS_FMT_SGBRG12_1X12;
		}
		break;
	case BAYER_BGGR:
		if (sensor->cur_mode.bit_width == 8) {
			*code = MEDIA_BUS_FMT_SBGGR8_1X8;
		} else if (sensor->cur_mode.bit_width == 10) {
			*code = MEDIA_BUS_FMT_SBGGR10_1X10;
		} else {
			*code = MEDIA_BUS_FMT_SBGGR12_1X12;
		}
		break;
	default:
		/*nothing need to do*/
		break;
	}
	return 0;
}
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 12, 0)
static int os08a20_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_mbus_code_enum *code)
#else
static int os08a20_enum_mbus_code(struct v4l2_subdev *sd,
			         struct v4l2_subdev_pad_config *cfg,
			         struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct os08a20 *sensor = client_to_os08a20(client);

	u32 cur_code = MEDIA_BUS_FMT_SBGGR12_1X12;

	if (code->index > 0)
		return -EINVAL;
	os08a20_get_format_code(sensor,&cur_code);
	code->code = cur_code;

	return 0;
}
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 12, 0)
static int os08a20_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *state,
			   struct v4l2_subdev_format *fmt)
#else
static int os08a20_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *fmt)
#endif
{
	int ret = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct os08a20 *sensor = client_to_os08a20(client);
	mutex_lock(&sensor->lock);

	if ((fmt->format.width != sensor->cur_mode.size.bounds_width) ||
	    (fmt->format.height != sensor->cur_mode.size.bounds_height)) {
		pr_err("%s:set sensor format %dx%d error\n",
			__func__,fmt->format.width,fmt->format.height);
		mutex_unlock(&sensor->lock);
		return -EINVAL;
	}

	pm_runtime_get_noresume(&sensor->i2c_client->dev);

	os08a20_write_reg(sensor, 0x103, 0x01);
	msleep(20);

	ret = os08a20_write_reg_arry(sensor,
		(struct vvcam_sccb_data_s *)sensor->cur_mode.preg_data,
		sensor->cur_mode.reg_data_count);

	pm_runtime_mark_last_busy(&sensor->i2c_client->dev);
	pm_runtime_put_autosuspend(&sensor->i2c_client->dev);

	if (ret < 0) {
		pr_err("%s:os08a20_write_reg_arry error\n",__func__);
		mutex_unlock(&sensor->lock);
		return -EINVAL;
	}

	os08a20_get_format_code(sensor, &fmt->format.code);
	fmt->format.field = V4L2_FIELD_NONE;
	sensor->format = fmt->format;
	mutex_unlock(&sensor->lock);
	return 0;
}
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 12, 0)
static int os08a20_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *state,
			   struct v4l2_subdev_format *fmt)
#else
static int os08a20_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *fmt)
#endif
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct os08a20 *sensor = client_to_os08a20(client);

	mutex_lock(&sensor->lock);
	fmt->format = sensor->format;
	mutex_unlock(&sensor->lock);
	return 0;
}

static long os08a20_priv_ioctl(struct v4l2_subdev *sd,
                              unsigned int cmd,
                              void *arg_user)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct os08a20 *sensor = client_to_os08a20(client);
	long ret = 0;
	struct vvcam_sccb_data_s sensor_reg;
	void *arg = arg_user;
	int enable = 0;

	mutex_lock(&sensor->lock);
	switch (cmd){
	case VVSENSORIOC_S_POWER:
		ret = 0;
		break;
	case VVSENSORIOC_S_CLK:
		ret = 0;
		break;
	case VVSENSORIOC_G_CLK:
		ret = os08a20_get_clk(sensor,arg);
		break;
	case VVSENSORIOC_RESET:
		ret = 0;
		break;
	case VIDIOC_QUERYCAP:
		ret = os08a20_query_capability(sensor, arg);
		break;
	case VVSENSORIOC_QUERY:
		USER_TO_KERNEL_VMALLOC(struct vvcam_mode_info_array_s);
		ret = os08a20_query_supports(sensor, arg);
		KERNEL_TO_USER_VMALLOC(struct vvcam_mode_info_array_s);
		break;
	case VVSENSORIOC_G_CHIP_ID:
		ret = os08a20_get_sensor_id(sensor, arg);
		break;
	case VVSENSORIOC_G_RESERVE_ID:
		ret = os08a20_get_reserve_id(sensor, arg);
		break;
	case VVSENSORIOC_G_SENSOR_MODE:
		ret = os08a20_get_sensor_mode(sensor, arg);
		break;
	case VVSENSORIOC_S_SENSOR_MODE:
		ret = os08a20_set_sensor_mode(sensor, arg);
		break;
	case VVSENSORIOC_S_STREAM:
		USER_TO_KERNEL(int);
		enable = *(int *)arg;
		if ((enable && !sensor->stream_status) ||
		    (!enable && sensor->stream_status))
			ret = os08a20_s_stream(&sensor->subdev, *(int *)arg);
		break;
	case VVSENSORIOC_WRITE_REG:
		ret = copy_from_user(&sensor_reg, arg,
			sizeof(struct vvcam_sccb_data_s));
		ret |= os08a20_write_reg(sensor, sensor_reg.addr,
			sensor_reg.data);
		break;
	case VVSENSORIOC_READ_REG:
		ret = copy_from_user(&sensor_reg, arg,
			sizeof(struct vvcam_sccb_data_s));
		ret |= os08a20_read_reg(sensor, sensor_reg.addr,
			(u8 *)&sensor_reg.data);
		ret |= copy_to_user(arg, &sensor_reg,
			sizeof(struct vvcam_sccb_data_s));
		break;
	case VVSENSORIOC_S_EXP:
		USER_TO_KERNEL(u32);
		ret = os08a20_set_exp(sensor, *(u32 *)arg);
		break;
	case VVSENSORIOC_S_VSEXP:
		USER_TO_KERNEL(u32);
		ret = os08a20_set_vsexp(sensor, *(u32 *)arg);
		break;
	case VVSENSORIOC_S_GAIN:
		USER_TO_KERNEL(u32);
		ret = os08a20_set_gain(sensor, *(u32 *)arg);
		break;
	case VVSENSORIOC_S_VSGAIN:
		USER_TO_KERNEL(u32);
		ret = os08a20_set_vsgain(sensor, *(u32 *)arg);
		break;
	case VVSENSORIOC_S_FPS:
		USER_TO_KERNEL(u32);
		ret = os08a20_set_fps(sensor, *(u32 *)arg);
		break;
	case VVSENSORIOC_G_FPS:
		USER_TO_KERNEL(u32);
		ret = os08a20_get_fps(sensor, (u32 *)arg);
		KERNEL_TO_USER(u32);
		break;
	case VVSENSORIOC_S_HDR_RADIO:
		ret = os08a20_set_ratio(sensor, arg);
		break;
	case VVSENSORIOC_S_TEST_PATTERN:
		ret= os08a20_set_test_pattern(sensor, arg);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&sensor->lock);
	return ret;
}

static struct v4l2_subdev_video_ops os08a20_subdev_video_ops = {
	.s_stream = os08a20_s_stream,
};

static const struct v4l2_subdev_pad_ops os08a20_subdev_pad_ops = {
	.enum_mbus_code = os08a20_enum_mbus_code,
	.set_fmt = os08a20_set_fmt,
	.get_fmt = os08a20_get_fmt,
};

static struct v4l2_subdev_core_ops os08a20_subdev_core_ops = {
	.ioctl = os08a20_priv_ioctl,
};

static struct v4l2_subdev_ops os08a20_subdev_ops = {
	.core  = &os08a20_subdev_core_ops,
	.video = &os08a20_subdev_video_ops,
	.pad   = &os08a20_subdev_pad_ops,
};

static int os08a20_link_setup(struct media_entity *entity,
			     const struct media_pad *local,
			     const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations os08a20_sd_media_ops = {
	.link_setup = os08a20_link_setup,
};

static int os08a20_regulator_enable(struct os08a20 *sensor)
{
	int ret = 0;
	struct device *dev = &(sensor->i2c_client->dev);

	pr_debug("enter %s\n", __func__);

	if (sensor->io_regulator) {
		regulator_set_voltage(sensor->io_regulator,
				      OS08A20_VOLTAGE_DIGITAL_IO,
				      OS08A20_VOLTAGE_DIGITAL_IO);
		ret = regulator_enable(sensor->io_regulator);
		if (ret < 0) {
			dev_err(dev, "set io voltage failed\n");
			return ret;
		}
	}

	if (sensor->analog_regulator) {
		regulator_set_voltage(sensor->analog_regulator,
				      OS08A20_VOLTAGE_ANALOG,
				      OS08A20_VOLTAGE_ANALOG);
		ret = regulator_enable(sensor->analog_regulator);
		if (ret) {
			dev_err(dev, "set analog voltage failed\n");
			goto err_disable_io;
		}

	}

	if (sensor->core_regulator) {
		regulator_set_voltage(sensor->core_regulator,
				      OS08A20_VOLTAGE_DIGITAL_CORE,
				      OS08A20_VOLTAGE_DIGITAL_CORE);
		ret = regulator_enable(sensor->core_regulator);
		if (ret) {
			dev_err(dev, "set core voltage failed\n");
			goto err_disable_analog;
		}
	}

	return 0;

err_disable_analog:
	regulator_disable(sensor->analog_regulator);
err_disable_io:
	regulator_disable(sensor->io_regulator);
	return ret;
}

static void os08a20_regulator_disable(struct os08a20 *sensor)
{
	int ret = 0;
	struct device *dev = &(sensor->i2c_client->dev);

	if (sensor->core_regulator) {
		ret = regulator_disable(sensor->core_regulator);
		if (ret < 0)
			dev_err(dev, "core regulator disable failed\n");
	}

	if (sensor->analog_regulator) {
		ret = regulator_disable(sensor->analog_regulator);
		if (ret < 0)
			dev_err(dev, "analog regulator disable failed\n");
	}

	if (sensor->io_regulator) {
		ret = regulator_disable(sensor->io_regulator);
		if (ret < 0)
			dev_err(dev, "io regulator disable failed\n");
	}
	return ;
}

static int os08a20_set_clk_rate(struct os08a20 *sensor)
{
	int ret;
	unsigned int clk;

	clk = sensor->mclk;
	clk = min_t(u32, clk, (u32)OS08A20_XCLK_MAX);
	clk = max_t(u32, clk, (u32)OS08A20_XCLK_MIN);
	sensor->mclk = clk;

	pr_debug("   Setting mclk to %d MHz\n",sensor->mclk / 1000000);
	ret = clk_set_rate(sensor->sensor_clk, sensor->mclk);
	if (ret < 0)
		pr_debug("set rate filed, rate=%d\n", sensor->mclk);
	return ret;
}

static void os08a20_reset(struct os08a20 *sensor)
{
	pr_debug("enter %s\n", __func__);
	if (!gpio_is_valid(sensor->rst_gpio))
		return;

	gpio_set_value_cansleep(sensor->rst_gpio, 0);
	msleep(20);

	gpio_set_value_cansleep(sensor->rst_gpio, 1);
	msleep(20);

	return;
}

static int os08a20_retrieve_capture_properties(
			struct os08a20 *sensor,
			struct os08a20_capture_properties* ocp)
{
	struct device *dev = &sensor->i2c_client->dev;
	__u64 mlf = 0;
	__u64 mpf = 0;
	__u64 mdr = 0;

	struct device_node *ep;
	int ret;
	/*Collecting the information about limits of capture path
	* has been centralized to the sensor
	* * also into the sensor endpoint itself.
	*/

	ep = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!ep) {
		dev_err(dev, "missing endpoint node\n");
		return -ENODEV;
	}

	/*ret = fwnode_property_read_u64(of_fwnode_handle(ep),
		"max-lane-frequency", &mlf);
	if (ret || mlf == 0) {
		dev_dbg(dev, "no limit for max-lane-frequency\n");
	}*/
	ret = fwnode_property_read_u64(of_fwnode_handle(ep),
	        "max-pixel-frequency", &mpf);
	if (ret || mpf == 0) {
	        dev_dbg(dev, "no limit for max-pixel-frequency\n");
	}

	/*ret = fwnode_property_read_u64(of_fwnode_handle(ep),
	        "max-data-rate", &mdr);
	if (ret || mdr == 0) {
	        dev_dbg(dev, "no limit for max-data_rate\n");
	}*/

	ocp->max_lane_frequency = mlf;
	ocp->max_pixel_frequency = mpf;
	ocp->max_data_rate = mdr;

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
static int os08a20_probe(struct i2c_client *client)
#else
static int os08a20_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
#endif
{
	int retval;
	struct device *dev = &client->dev;
	struct v4l2_subdev *sd;
	struct os08a20 *sensor;
	u32 chip_id = 0;
	u8 reg_val = 0;

	pr_info("enter %s\n", __func__);

	sensor = devm_kmalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;
	memset(sensor, 0, sizeof(*sensor));

	sensor->i2c_client = client;

	sensor->pwn_gpio = of_get_named_gpio(dev->of_node, "pwn-gpios", 0);
	if (!gpio_is_valid(sensor->pwn_gpio))
		dev_warn(dev, "No sensor pwdn pin available");
	else {
		retval = devm_gpio_request_one(dev, sensor->pwn_gpio,
						GPIOF_OUT_INIT_HIGH,
						"os08a20_mipi_pwdn");
		if (retval < 0) {
			dev_warn(dev, "Failed to set power pin\n");
			dev_warn(dev, "retval=%d\n", retval);
			return retval;
		}
	}

	sensor->rst_gpio = of_get_named_gpio(dev->of_node, "rst-gpios", 0);
	if (!gpio_is_valid(sensor->rst_gpio))
		dev_warn(dev, "No sensor reset pin available");
	else {
		retval = devm_gpio_request_one(dev, sensor->rst_gpio,
						GPIOF_OUT_INIT_HIGH,
						"os08a20_mipi_reset");
		if (retval < 0) {
			dev_warn(dev, "Failed to set reset pin\n");
			return retval;
		}
	}

	sensor->sensor_clk = devm_clk_get(dev, "csi_mclk");
	if (IS_ERR(sensor->sensor_clk)) {
		sensor->sensor_clk = NULL;
		dev_err(dev, "clock-frequency missing or invalid\n");
		return PTR_ERR(sensor->sensor_clk);
	}

	retval = of_property_read_u32(dev->of_node, "mclk", &(sensor->mclk));
	if (retval) {
		dev_err(dev, "mclk missing or invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "mclk_source",
				(u32 *)&(sensor->mclk_source));
	if (retval) {
		dev_err(dev, "mclk_source missing or invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "csi_id", &(sensor->csi_id));
	if (retval) {
		dev_err(dev, "csi id missing or invalid\n");
		return retval;
	}

	retval = os08a20_retrieve_capture_properties(sensor,&sensor->ocp);
	if (retval) {
		dev_warn(dev, "retrive capture properties error\n");
	}

	sensor->io_regulator = devm_regulator_get(dev, "DOVDD");
	if (IS_ERR(sensor->io_regulator)) {
		dev_err(dev, "cannot get io regulator\n");
		return PTR_ERR(sensor->io_regulator);
	}

	sensor->core_regulator = devm_regulator_get(dev, "DVDD");
	if (IS_ERR(sensor->core_regulator)) {
		dev_err(dev, "cannot get core regulator\n");
		return PTR_ERR(sensor->core_regulator);
	}

	sensor->analog_regulator = devm_regulator_get(dev, "AVDD");
	if (IS_ERR(sensor->analog_regulator)) {
		dev_err(dev, "cannot get analog  regulator\n");
		return PTR_ERR(sensor->analog_regulator);
	}

	retval = os08a20_regulator_enable(sensor);
	if (retval) {
		dev_err(dev, "regulator enable failed\n");
		return retval;
	}

	os08a20_set_clk_rate(sensor);
	retval = clk_prepare_enable(sensor->sensor_clk);
	if (retval < 0) {
		dev_err(dev, "%s: enable sensor clk fail\n", __func__);
		goto probe_err_regulator_disable;
	}

	sd = &sensor->subdev;
	v4l2_i2c_subdev_init(sd, client, &os08a20_subdev_ops);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->dev = &client->dev;
	sd->entity.ops = &os08a20_sd_media_ops;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	sensor->pads[OS08A20_SENS_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	retval = media_entity_pads_init(&sd->entity, OS08A20_SENS_PADS_NUM,
					sensor->pads);
	if (retval)
		goto probe_err_regulator_disable;

	if (os08a20_init_controls(sensor))
		goto probe_err_entity_cleanup;
	
	/* os08a20 power on*/
	retval = os08a20_runtime_resume(dev);
	if (retval) {
		dev_err(dev, "failed to power on\n");
		goto probe_err_free_ctrls;
	}

	pm_runtime_set_active(dev);
	pm_runtime_get_noresume(dev);
	pm_runtime_enable(dev);
	
	os08a20_reset(sensor);

	os08a20_read_reg(sensor, 0x300a, &reg_val);
	chip_id |= reg_val << 16;
	os08a20_read_reg(sensor, 0x300b, &reg_val);
	chip_id |= reg_val << 8;
	os08a20_read_reg(sensor, 0x300c, &reg_val);
	chip_id |= reg_val;
	if (chip_id != 0x530841) {
		pr_warn("camera os08a20 is not found\n");
		retval = -ENODEV;
		goto probe_err_pm_runtime;
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 12, 0)
	retval = v4l2_async_register_subdev_sensor(sd);
#else
	retval = v4l2_async_register_subdev_sensor_common(sd);
#endif
	if (retval < 0) {
		dev_err(&client->dev,"%s--Async register failed, ret=%d\n",
			__func__,retval);
		goto probe_err_pm_runtime;
	}

	memcpy(&sensor->cur_mode, &pos08a20_mode_info[0],
			sizeof(struct vvcam_mode_info_s));

	mutex_init(&sensor->lock);
	pr_info("%s camera mipi os08a20, is found\n", __func__);


	pm_runtime_set_autosuspend_delay(dev, 1000);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;

probe_err_pm_runtime:
	pm_runtime_put_noidle(dev);
	pm_runtime_disable(dev);
	os08a20_runtime_suspend(dev);
probe_err_free_ctrls:
	v4l2_ctrl_handler_free(&sensor->ctrls.handler);
probe_err_entity_cleanup:
	media_entity_cleanup(&sd->entity);
probe_err_regulator_disable:
	os08a20_regulator_disable(sensor);

	return retval;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
static int os08a20_remove(struct i2c_client *client)
#else
static void os08a20_remove(struct i2c_client *client)
#endif
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct os08a20 *sensor = client_to_os08a20(client);
	struct device *dev = &client->dev;

	pr_info("enter %s\n", __func__);

	pm_runtime_disable(dev);
	if (!pm_runtime_status_suspended(dev))
		os08a20_runtime_suspend(dev);
	pm_runtime_set_suspended(dev);
	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&sensor->ctrls.handler);
	os08a20_regulator_disable(sensor);
	mutex_destroy(&sensor->lock);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
	return 0;
#else
#endif
}

static int __maybe_unused os08a20_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct os08a20 *sensor = client_to_os08a20(client);

	sensor->resume_status = sensor->stream_status;
	if (sensor->resume_status) {
		os08a20_s_stream(&sensor->subdev,0);
	}

	return 0;
}

static int __maybe_unused os08a20_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct os08a20 *sensor = client_to_os08a20(client);

	if (sensor->resume_status) {
		os08a20_s_stream(&sensor->subdev,1);
	}

	return 0;
}

static const struct dev_pm_ops os08a20_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(os08a20_suspend, os08a20_resume)
	SET_RUNTIME_PM_OPS(os08a20_runtime_suspend, os08a20_runtime_resume, NULL)
};

static const struct i2c_device_id os08a20_id[] = {
	{"os08a20", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, os08a20_id);

static const struct of_device_id os08a20_of_match[] = {
	{ .compatible = "ovti,os08a20" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, os08a20_of_match);

static struct i2c_driver os08a20_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = "os08a20",
		.pm = &os08a20_pm_ops,
		.of_match_table	= os08a20_of_match,
	},
	.probe	= os08a20_probe,
	.remove = os08a20_remove,
	.id_table = os08a20_id,
};

module_i2c_driver(os08a20_i2c_driver);
MODULE_DESCRIPTION("OS08a20 MIPI Camera Subdev Driver");
MODULE_LICENSE("GPL");
