/*
 * drivers/amlogic/media/camera/common/cam_prober.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>
#include <linux/delay.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/camera/aml_cam_info.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/old_cpu_version.h>
#include <linux/clk.h>

#define CONFIG_ARCH_MESON8

static struct platform_device *cam_pdev;
static struct clk *cam_clk;
static unsigned int bt_path_count;

static unsigned int camera0_pwdn_pin;
static unsigned int camera0_rst_pin;
static unsigned int camera1_pwdn_pin;
static unsigned int camera1_rst_pin;

static struct aml_cam_info_s *temp_cam;

static int aml_camera_read_buff(struct i2c_adapter *adapter,
				unsigned short dev_addr, char *buf,
				int addr_len, int data_len)
{
	int i2c_flag = -1;
	struct i2c_msg msgs[] = {{
			.addr = dev_addr, .flags = 0,
		    .len = addr_len, .buf = buf,
		}, {
			.addr = dev_addr, .flags = I2C_M_RD,
		    .len = data_len, .buf = buf,
		}
	};

	i2c_flag = i2c_transfer(adapter, msgs, 2);

	return i2c_flag;
}

static int aml_camera_write_buff(struct i2c_adapter *adapter,
				 unsigned short dev_addr, char *buf, int len)
{
	struct i2c_msg msg[] = {{
			.addr = dev_addr, .flags = 0,    /*I2C_M_TEN*/
			.len = len, .buf = buf,
		}
	};

	if (i2c_transfer(adapter, msg, 1) < 0)
		return -1;
	else
		return 0;
}

static int aml_i2c_get_byte(struct i2c_adapter *adapter,
			    unsigned short dev_addr, unsigned short addr)
{
	unsigned char buff[4];

	buff[0] = (unsigned char)((addr >> 8) & 0xff);
	buff[1] = (unsigned char)(addr & 0xff);

	if (aml_camera_read_buff(adapter, dev_addr, buff, 2, 1) < 0)
		return -1;
	return buff[0];
}

static int aml_i2c_put_byte(struct i2c_adapter *adapter,
			    unsigned short dev_addr, unsigned short addr,
			    unsigned char data)
{
	unsigned char buff[4];

	buff[0] = (unsigned char)((addr >> 8) & 0xff);
	buff[1] = (unsigned char)(addr & 0xff);
	buff[2] = data;
	if (aml_camera_write_buff(adapter, dev_addr, buff, 3) < 0)
		return -1;
	return 0;
}

static int aml_i2c_get_byte_add8(struct i2c_adapter *adapter,
				 unsigned short dev_addr,
				 unsigned short addr)
{
	unsigned char buff[4];

	buff[0] = (unsigned char)(addr & 0xff);

	if (aml_camera_read_buff(adapter, dev_addr, buff, 1, 1) < 0)
		return -1;
	return buff[0];
}

static int aml_i2c_put_byte_add8(struct i2c_adapter *adapter,
				 unsigned short dev_addr,
				 unsigned short addr, unsigned char data)
{
	unsigned char buff[4];

	buff[0] = (unsigned char)(addr & 0xff);
	buff[1] = data;
	if (aml_camera_write_buff(adapter, dev_addr, buff, 2) < 0)
		return -1;
	return 0;
}

int aml_i2c_put_word(struct i2c_adapter *adapter, unsigned short dev_addr,
		     unsigned short addr, unsigned short data)
{
	unsigned char buff[4];

	buff[0] = (unsigned char)((addr >> 8) & 0xff);
	buff[1] = (unsigned char)(addr & 0xff);
	buff[2] = (unsigned char)((data >> 8) & 0xff);
	buff[3] = (unsigned char)(data & 0xff);
	if (aml_camera_write_buff(adapter, dev_addr, buff, 4) < 0)
		return -1;
	return 0;
}

static int aml_i2c_get_word(struct i2c_adapter *adapter,
			    unsigned short dev_addr, unsigned short addr)
{
	int ret;
	unsigned char buff[4];

	buff[0] = (unsigned char)((addr >> 8) & 0xff);
	buff[1] = (unsigned char)(addr & 0xff);
	if (aml_camera_read_buff(adapter, dev_addr, buff, 2, 2) < 0)
		return -1;
	ret = (buff[0] << 8) | (buff[1]);
	return ret;
}

static int aml_i2c_get_word_add8(struct i2c_adapter *adapter,
				 unsigned short dev_addr,
				 unsigned short addr)
{
	int ret;
	unsigned char buff[4];

	buff[0] = (unsigned char)((addr >> 8) & 0xff);
	buff[1] = (unsigned char)(addr & 0xff);
	if (aml_camera_read_buff(adapter, dev_addr, buff, 2, 2) < 0)
		return -1;
	ret = buff[0] | (buff[1] << 8);
	return ret;
}

static int aml_i2c_put_word_add8(struct i2c_adapter *adapter,
				 unsigned short dev_addr, unsigned char addr,
				 unsigned short data)
{
	unsigned char buff[4];

	buff[0] = (unsigned char)(addr & 0xff);
	buff[1] = (unsigned char)(data >> 8 & 0xff);
	buff[2] = (unsigned char)(data & 0xff);
	if (aml_camera_write_buff(adapter, dev_addr, buff, 3) < 0)
		return -1;
	return 0;
}


extern struct i2c_client *
i2c_new_device(struct i2c_adapter *adap, struct i2c_board_info const *info);

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC0307
int gc0307_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg;

	reg = aml_i2c_get_byte_add8(adapter, 0x21, 0x00);
	if (reg == 0x99)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC0308
int gc0308_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg;

	reg = aml_i2c_get_byte_add8(adapter, 0x21, 0x00);
	if (reg == 0x9b)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC0328
int gc0328_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg;

	reg = aml_i2c_get_byte_add8(adapter, 0x21, 0xf0);
	if (reg == 0x9d)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC0329
int gc0329_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg;

	aml_i2c_put_byte_add8(adapter, 0x31, 0xfc, 0x16); /*select page 0*/
	reg = aml_i2c_get_byte_add8(adapter, 0x31, 0x00);
	if (reg == 0xc0)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC2015
int gc2015_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte_add8(adapter, 0x30, 0x00);
	reg[1] = aml_i2c_get_byte_add8(adapter, 0x30, 0x01);
	if (reg[0] == 0x20 && reg[1] == 0x05)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_HM2057
int hm2057_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte(adapter, 0x24, 0x0001);
	reg[1] = aml_i2c_get_byte(adapter, 0x24, 0x0002);
	if (reg[0] == 0x20 && reg[1] == 0x56)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC2035
int gc2035_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte_add8(adapter, 0x3c, 0xf0);
	reg[1] = aml_i2c_get_byte_add8(adapter, 0x3c, 0xf1);
	if (reg[0] == 0x20 && reg[1] == 0x35)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC2155
int gc2155_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte_add8(adapter, 0x3c, 0xf0);
	reg[1] = aml_i2c_get_byte_add8(adapter, 0x3c, 0xf1);
	if (reg[0] == 0x21 && reg[1] == 0x55)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GT2005
int gt2005_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte(adapter, 0x3c, 0x0000);
	reg[1] = aml_i2c_get_byte(adapter, 0x3c, 0x0001);
	if (reg[0] == 0x51 && reg[1] == 0x38)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV2659
int ov2659_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte(adapter, 0x30, 0x300a);
	reg[1] = aml_i2c_get_byte(adapter, 0x30, 0x300b);
	if (reg[0] == 0x26 && reg[1] == 0x56)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV3640
int ov3640_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte(adapter, 0x3c, 0x300a);
	reg[1] = aml_i2c_get_byte(adapter, 0x3c, 0x300b);
	if (reg[0] == 0x36 && reg[1] == 0x4c)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV3660
int ov3660_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte(adapter, 0x3c, 0x300a);
	reg[1] = aml_i2c_get_byte(adapter, 0x3c, 0x300b);
	if (reg[0] == 0x36 && reg[1] == 0x60)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV5640
int ov5640_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte(adapter, 0x3c, 0x300a);
	reg[1] = aml_i2c_get_byte(adapter, 0x3c, 0x300b);
	if (reg[0] == 0x56 && reg[1] == 0x40)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV5642
int ov5642_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte(adapter, 0x3c, 0x300a);
	reg[1] = aml_i2c_get_byte(adapter, 0x3c, 0x300b);
	if (reg[0] == 0x56 && reg[1] == 0x42)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV7675
int ov7675_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte_add8(adapter, 0x21, 0x0a);
	reg[1] = aml_i2c_get_byte_add8(adapter, 0x21, 0x0b);
	if (reg[0] == 0x76 && reg[1] == 0x73)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_SP0A19
int sp0a19_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg;

	reg = aml_i2c_get_byte_add8(adapter, 0x21, 0x02);
	if (reg == 0xa6)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_SP2518
int sp2518_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg;

	reg = aml_i2c_get_byte_add8(adapter, 0x30, 0x02);
	pr_info("sp2518 chip id reg = %x .\n", reg);
	if (reg == 0x53)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_SP0838
int sp0838_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg;

	reg = aml_i2c_get_byte_add8(adapter, 0x18, 0x02);
	if (reg == 0x27)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_HI253
int hi253_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg;

	reg = aml_i2c_get_byte_add8(adapter, 0x20, 0x04);
	if (reg == 0x92)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_HM5065
int hm5065_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte(adapter, 0x1F, 0x0000);
	reg[1] = aml_i2c_get_byte(adapter, 0x1F, 0x0001);
	if (reg[0] == 0x03 && reg[1] == 0x9e)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_HM1375
int hm1375_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte(adapter, 0x24, 0x0001);
	reg[1] = aml_i2c_get_byte(adapter, 0x24, 0x0002);
	pr_info("hm1375_v4l2_probe: device ID: 0x%x%x", reg[0], reg[1]);
	if ((reg[0] == 0x13 || reg[0] == 0x03) && reg[1] == 0x75)
		ret = 1;
	ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_HI2056
int hi2056_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte(adapter, 0x24, 0x0001);
	reg[1] = aml_i2c_get_byte(adapter, 0x24, 0x0002);
	pr_info("reg[0]=%x, reg[1]=%x\n", reg[0], reg[1]);
	if (reg[0] == 0x20 && reg[1] == 0x56)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV5647
int ov5647_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte(adapter, 0x36, 0x300a);
	reg[1] = aml_i2c_get_byte(adapter, 0x36, 0x300b);
	pr_info("reg[0]:%x,reg[1]:%x\n", reg[0], reg[1]);
	if (reg[0] == 0x56 && reg[1] == 0x47)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_AR0543
int ar0543_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0, reg_val;

	reg_val = aml_i2c_get_word(adapter, 0x36, 0x3000);
	pr_info("reg:0x%x\n", reg_val);
	if (reg_val == 0x4800)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_AR0833
int ar0833_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0, reg_val;

	reg_val = aml_i2c_get_word(adapter, 0x36, 0x3000);
	pr_info("reg:0x%x\n", reg_val);
	if (reg_val == 0x4B03)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_SP1628
int sp1628_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte_add8(adapter, 0x3c, 0x02);
	reg[1] = aml_i2c_get_byte_add8(adapter, 0x3c, 0xa0);
	if (reg[0] == 0x16 && reg[1] == 0x28)
		ret = 1;
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_BF3720
int bf3720_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte_add8(adapter, 0x6e, 0xfc);
	reg[1] = aml_i2c_get_byte_add8(adapter, 0x6e, 0xfd);
	if (reg[0] == 0x37 && reg[1] == 0x20)
		ret = 1;
	return ret;
}
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_BF3703
int __init bf3703_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte_add8(adapter, 0x6e, 0xfc); /*i2c addr:0x6f*/
	reg[1] = aml_i2c_get_byte_add8(adapter, 0x6e, 0xfd);
	if (reg[0] == 0x37 && reg[1] == 0x03)
		ret = 1;
	return ret;
}
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_BF3920
int __init bf3920_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte_add8(adapter, 0x6e, 0xfc); /*i2c addr:0x6f*/
	reg[1] = aml_i2c_get_byte_add8(adapter, 0x6e, 0xfd);
	if (reg[0] == 0x39 && reg[1] == 0x20)
		ret = 1;
	return ret;
}
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC2145
int __init gc2145_v4l2_probe(struct i2c_adapter *adapter)
{
	int ret = 0;
	unsigned char reg[2];

	reg[0] = aml_i2c_get_byte_add8(adapter, 0x3c, 0xf0);
	reg[1] = aml_i2c_get_byte_add8(adapter, 0x3c, 0xf1);
	/*datasheet chip id is error*/
	if (reg[0] == 0x21 && reg[1] == 0x45)
		ret = 1;
	pr_info("%s, ret = %d\n", __func__, ret);
	return ret;
}
#endif

struct aml_cam_dev_info_s {
	unsigned char addr;
	char *name;
	unsigned char pwdn;
	enum resolution_size max_cap_size;
	aml_cam_probe_fun_t probe_func;
};

static const struct aml_cam_dev_info_s cam_devs[] = {
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC0307
	{
		.addr = 0x21,
		.name = "gc0307",
		.pwdn = 1,
		.max_cap_size = SIZE_640X480,
		.probe_func = gc0307_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC0308
	{
		.addr = 0x21,
		.name = "gc0308",
		.pwdn = 1,
		.max_cap_size = SIZE_640X480,
		.probe_func = gc0308_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC0328
	{
		.addr = 0x21,
		.name = "gc0328",
		.pwdn = 1,
		.max_cap_size = SIZE_640X480,
		.probe_func = gc0328_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC0329
	{
		.addr = 0x31,
		.name = "gc0329",
		.pwdn = 1,
		.max_cap_size = SIZE_640X480,
		.probe_func = gc0329_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC2015
	{
		.addr = 0x30,
		.name = "gc2015",
		.pwdn = 1,
		.max_cap_size = SIZE_1600X1200,
		.probe_func = gc2015_v4l2_probe,
	},
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_HM2057
	{
		.addr = 0x24,
		.name = "hm2057",
		.pwdn = 1,
		.max_cap_size = SIZE_1600X1200,
		.probe_func = hm2057_v4l2_probe,
	},
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC2035
	{
		.addr = 0x3c,
		.name = "gc2035",
		.pwdn = 1,
		.max_cap_size = SIZE_1600X1200,
		.probe_func = gc2035_v4l2_probe,
	},
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC2155
	{
		.addr = 0x3c,
		.name = "gc2155",
		.pwdn = 1,
		.max_cap_size = SIZE_1600X1200,
		.probe_func = gc2155_v4l2_probe,
	},
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GT2005
	{
		.addr = 0x3c,
		.name = "gt2005",
		.pwdn = 0,
		.max_cap_size = SIZE_1600X1200,
		.probe_func = gt2005_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV2659
	{
		.addr = 0x30,
		.name = "ov2659",
		.pwdn = 1,
		.max_cap_size = SIZE_1600X1200,
		.probe_func = ov2659_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV3640
	{
		.addr = 0x3c,
		.name = "ov3640",
		.pwdn = 1,
		.max_cap_size = SIZE_2048X1536;
		.probe_func = ov3640_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV3660
	{
		.addr = 0x3c,
		.name = "ov3660",
		.pwdn = 1,
		.max_cap_size = SIZE_2048X1536,
		.probe_func = ov3660_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV5640
	{
		.addr = 0x3c,
		.name = "ov5640",
		.pwdn = 1,
		.max_cap_size = SIZE_2592X1944,
		.probe_func = ov5640_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV5642
	{
		.addr = 0x3c,
		.name = "ov5642",
		.pwdn = 1,
		.max_cap_size = SIZE_2592X1944,
		.probe_func = ov5642_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV5647
	{
		.addr = 0x36, /* really value should be 0x6c  */
		.name = "ov5647", .pwdn = 1, .max_cap_size = SIZE_2592X1944,
		.probe_func = ov5647_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_OV7675
	{
		.addr = 0x21,
		.name = "ov7675",
		.pwdn = 1,
		.max_cap_size = SIZE_640X480,
		.probe_func = ov7675_v4l2_probe,
	},
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_SP0A19
	{
		.addr = 0x21,
		.name = "sp0a19",
		.pwdn = 1,
		.max_cap_size = SIZE_640X480,
		.probe_func = sp0a19_v4l2_probe,
	},
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_SP0838
	{
		.addr = 0x18,
		.name = "sp0838",
		.pwdn = 1,
		.max_cap_size = SIZE_640X480,
		.probe_func = sp0838_v4l2_probe,
	},
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_SP2518
	{
		.addr = 0x30,
		.name = "sp2518",
		.pwdn = 1,
		.max_cap_size = SIZE_1600X1200,
		.probe_func = sp2518_v4l2_probe,
	},
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_HI253
	{
		.addr = 0x20,
		.name = "hi253",
		.pwdn = 1,
		.max_cap_size = SIZE_1600X1200,
		.probe_func = hi253_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_HM5065
	{
		.addr = 0x1f,
		.name = "hm5065",
		.pwdn = 0,
		.max_cap_size = SIZE_2592X1944,
		.probe_func = hm5065_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_HM1375
	{
		.addr = 0x24,
		.name = "hm1375",
		.pwdn = 1,
		.max_cap_size = SIZE_1280X1024,
		.probe_func = hm1375_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_HI2056
	{
		.addr = 0x24,
		.name = "mipi-hi2056",
		.pwdn = 1,
		.max_cap_size = SIZE_1600X1200,
		.probe_func = hi2056_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_AR0543
	{
		.addr = 0x36,
		.name = "ar0543",
		.pwdn = 0,
		.max_cap_size = SIZE_2592X1944,
		.probe_func = ar0543_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_AR0833
	{
		.addr = 0x36,
		.name = "ar0833",
		.pwdn = 0,
		.max_cap_size = SIZE_2592X1944,
		.probe_func = ar0833_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_SP1628
	{
		.addr = 0x3c,
		.name = "sp1628",
		.pwdn = 1,
		.max_cap_size = SIZE_1280X960,
		.probe_func = sp1628_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_BF3720
	{
		.addr = 0x6e,
		.name = "bf3720",
		.pwdn = 1,
		.max_cap_size = SIZE_1600X1200,
		.probe_func = bf3720_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_BF3703
	{
		.addr = 0x6e,
		.name = "bf3703",
		.pwdn = 1,
		.max_cap_size = SIZE_640X480,
		.probe_func = bf3703_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_BF3920
	{
		.addr = 0x6e,
		.name = "bf3920",
		.pwdn = 1,
		.max_cap_size = SIZE_1600X1200,
		.probe_func = bf3920_v4l2_probe,
	},
#endif
#ifdef CONFIG_AMLOGIC_VIDEO_CAPTURE_GC2145
	{
		.addr = 0x3c,
		.name = "gc2145",
		.pwdn = 1,
		.max_cap_size = SIZE_1600X1200,
		.probe_func = gc2145_v4l2_probe,
	},
#endif
};

static const struct aml_cam_dev_info_s *get_cam_info_by_name(const char *name)
{
	int i;

	if (!name)
		return NULL;
	/*pr_info("cam_devs num is %d\n", ARRAY_SIZE(cam_devs));*/
	for (i = 0; i < ARRAY_SIZE(cam_devs); i++) {
		if (!strcmp(name, cam_devs[i].name)) {
			pr_info("camera dev %s found\n", cam_devs[i].name);
			pr_info("camera i2c addr: 0x%x\n", cam_devs[i].addr);
			return &cam_devs[i];
		}
	}
	return NULL;
}

struct res_item {
	enum resolution_size size;
	char *name;
};

static const struct res_item res_item_array[] = {
	{SIZE_320X240, "320X240"}, {
		SIZE_640X480, "640X480"
	}, {SIZE_720X405, "720X405"}, {
		SIZE_800X600, "800X600"
	}, {SIZE_960X540, "960X540"}, {
		SIZE_1024X576, "1024X576"
	}, {SIZE_960X720, "960X720"}, {
		SIZE_1024X768, "1024X768"
	}, {SIZE_1280X720, "1280X720"}, {
		SIZE_1152X864, "1152X864"
	}, {SIZE_1366X768, "1366X768"}, {
		SIZE_1280X960, "1280X960"
	}, {SIZE_1280X1024, "1280X1024"}, {
		SIZE_1400X1050, "1400X1050"
	}, {SIZE_1600X900, "1600X900"}, {
		SIZE_1600X1200, "1600X1200"
	}, {SIZE_1920X1080, "1920X1080"}, {
		SIZE_1792X1344, "1792X1344"
	}, {SIZE_2048X1152, "2048X1152"}, {
		SIZE_2048X1536, "2048X1536"
	}, {SIZE_2304X1728, "2304X1728"}, {
		SIZE_2560X1440, "2560X1440"
	}, {SIZE_2592X1944, "2592X1944"}, {
		SIZE_3072X1728, "3072X1728"
	}, {SIZE_2816X2112, "2816X2112"}, {
		SIZE_3072X2304, "3072X2304"
	}, {SIZE_3200X2400, "3200X2400"}, {
		SIZE_3264X2448, "3264X2448"
	}, {SIZE_3840X2160, "3840X2160"}, {
		SIZE_3456X2592, "3456X2592"
	}, {SIZE_3600X2700, "3600X2700"}, {
		SIZE_4096X2304, "4096X2304"
	}, {SIZE_3672X2754, "3672X2754"}, {
		SIZE_3840X2880, "3840X2880"
	}, {SIZE_4000X3000, "4000X3000"}, {
		SIZE_4608X2592, "4608X2592"
	}, {SIZE_4096X3072, "4096X3072"}, {
		SIZE_4800X3200, "4800X3200"
	}, {SIZE_5120X2880, "5120X2880"}, {
		SIZE_5120X3840, "5120X3840"
	}, {SIZE_6400X4800, "6400X480"},

};

static enum resolution_size get_res_size(const char *res_str)
{
	enum resolution_size ret = SIZE_NULL;
	const struct res_item *item;
	int i;

	if (!res_str)
		return SIZE_NULL;
	for (i = 0; i < ARRAY_SIZE(res_item_array); i++) {
		item = &res_item_array[i];
		if (!strcmp(item->name, res_str)) {
			ret = item->size;
			return ret;
		}
	}

	return ret;
}

static inline void GXBB_cam_enable_clk(void)
{
	struct clk *clk;

	clk = clk_get(&cam_pdev->dev, "clk_camera_24");
	if (IS_ERR(clk)) {
		pr_info("cannot get camera m-clock\n");
		clk = NULL;
	} else {
		cam_clk = clk;
		clk_prepare_enable(clk);
	}
}

static inline void GXBB_cam_disable_clk(int spread_spectrum)
{
	if (cam_clk) {
		clk_disable_unprepare(cam_clk);
		clk_put(cam_clk);
	}
}

static inline void GX12_cam_enable_clk(void)
{
	struct clk *clk;
	unsigned long clk_rate;

	clk = devm_clk_get(&cam_pdev->dev, "g12a_24m");
	if (IS_ERR(clk)) {
		pr_info("cannot get camera m-clock\n");
		clk = NULL;
	} else {
		cam_clk = clk;
		clk_set_rate(clk, 24000000);
		clk_prepare_enable(clk);
		clk_rate = clk_get_rate(clk);
	}
}

static inline void GX12_cam_disable_clk(int spread_spectrum)
{
	if (cam_clk) {
		clk_disable_unprepare(cam_clk);
		devm_clk_put(&cam_pdev->dev, cam_clk);
		pr_info("Success disable mclk\n");
	}
}

static inline void cam_enable_clk(int clk, int spread_spectrum)
{
	pr_err("camera mclk enable failed, unsupport chip\n");
}

static inline void cam_disable_clk(int spread_spectrum)
{
	pr_err("camera mclk disable failed, unsupport chip\n");
}

/*static struct platform_device *cam_pdev;*/

void aml_cam_init(struct aml_cam_info_s *cam_dev)
{
	/*select XTAL as camera clock*/
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB)
		GXBB_cam_enable_clk();
	else if ((get_cpu_type() == MESON_CPU_MAJOR_ID_G12A) ||
		(get_cpu_type() == MESON_CPU_MAJOR_ID_G12B))
		GX12_cam_enable_clk();
	else
		cam_enable_clk(cam_dev->mclk, cam_dev->spread_spectrum);

	/*coding style need: msleep < 20ms can sleep for up to 20ms*/
	msleep(20);
	/*set camera power enable*/
	if (!cam_dev->front_back) {
		cam_dev->pwdn_pin = camera0_pwdn_pin;
		cam_dev->rst_pin = camera0_rst_pin;
	} else {
		cam_dev->pwdn_pin = camera1_pwdn_pin;
		cam_dev->rst_pin = camera1_rst_pin;
	}

	gpio_direction_output(cam_dev->pwdn_pin, cam_dev->pwdn_act);

	msleep(20);

	gpio_direction_output(cam_dev->pwdn_pin, !(cam_dev->pwdn_act));

	msleep(20);

	gpio_direction_output(cam_dev->rst_pin, 0);

	msleep(20);

	gpio_direction_output(cam_dev->rst_pin, 1);

	msleep(20);

	pr_info("aml_cams: %s init OK\n", cam_dev->name);

}

void aml_cam_uninit(struct aml_cam_info_s *cam_dev)
{
	int ret;

	pr_info("aml_cams: %s uninit.\n", cam_dev->name);
	/*set camera power disable*/
	/*coding style need: msleep < 20ms can sleep for up to 20ms*/
	/*msleep(20);*/

	ret = gpio_direction_output(cam_dev->pwdn_pin, cam_dev->pwdn_act);
	if (ret < 0)
		pr_info("aml_cam_uninit pwdn_pin output pwdn_act failed\n");

	msleep(20);

	ret = gpio_direction_output(cam_dev->rst_pin, 0);
	if (ret < 0)
		pr_info("aml_cam_uninit rst_pin output rst_pin failed\n");

	msleep(20);

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB)
		GXBB_cam_disable_clk(cam_dev->spread_spectrum);
	else if ((get_cpu_type() == MESON_CPU_MAJOR_ID_G12A) ||
		(get_cpu_type() == MESON_CPU_MAJOR_ID_G12B))
		GX12_cam_disable_clk(cam_dev->spread_spectrum);
	else
		cam_disable_clk(cam_dev->spread_spectrum);
}

void aml_cam_flash(struct aml_cam_info_s *cam_dev, int is_on)
{
	int ret;

	if (cam_dev->flash_support) {
		pr_info("aml_cams: %s flash %s.\n",
		    cam_dev->name, is_on ? "on" : "off");

		ret = gpio_direction_output(cam_dev->flash_ctrl_pin,
			    cam_dev->flash_ctrl_level ? is_on : !is_on);
		if (ret < 0)
			pr_info("aml_cam_flash flash_ctrl_pin output failed\n");
	}
}

void aml_cam_torch(struct aml_cam_info_s *cam_dev, int is_on)
{
	int ret;

	if (cam_dev->torch_support) {
		pr_info("aml_cams: %s torch %s.\n",
	    cam_dev->name, is_on ? "on" : "off");

		ret = gpio_direction_output(cam_dev->torch_ctrl_pin,
			    cam_dev->torch_ctrl_level ? is_on : !is_on);
		if (ret < 0)
			pr_info("aml_cam_torch torch_ctrl_pin output failed\n");
	}
}

static struct list_head cam_head = LIST_HEAD_INIT(cam_head);

#define DEBUG_DUMP_CAM_INFO

static int fill_csi_dev(struct device_node *p_node,
			struct aml_cam_info_s *cam_dev)
{
	const char *str;
	int ret = 0;

	ret = of_property_read_string(p_node, "clk_channel", &str);
	if (ret) {
		pr_info("failed to read clock channel, \"a or b\"\n");
		cam_dev->clk_channel = CLK_CHANNEL_A;
	} else {
		pr_info("clock channel:clk %s\n", str);
		if (strncmp("a", str, 1) == 0)
			cam_dev->clk_channel = CLK_CHANNEL_A;
		else
			cam_dev->clk_channel = CLK_CHANNEL_B;
	}

	return ret;

}
static int fill_cam_dev(struct device_node *p_node,
			struct aml_cam_info_s *cam_dev)
{
	const char *str;
	int ret = 0;
	const struct aml_cam_dev_info_s *cam_info = NULL;
	struct device_node *adapter_node = NULL;
	struct i2c_adapter *adapter = NULL;
	unsigned int mclk = 0;
	unsigned int vcm_mode = 0;

	if (!p_node || !cam_dev)
		return -1;

	ret = of_property_read_u32(p_node, "front_back", &cam_dev->front_back);
	if (ret) {
		pr_info("get camera name failed!\n");
		goto err_out;
	}

	ret = of_property_read_string(p_node, "cam_name", &cam_dev->name);
	if (ret) {
		pr_info("get camera name failed!\n");
		goto err_out;
	}

	cam_dev->pwdn_pin = of_get_named_gpio(p_node, "gpio_pwdn-gpios", 0);
	if (cam_dev->pwdn_pin == 0) {
		pr_info("%s: failed to map gpio_pwdn !\n", cam_dev->name);
		goto err_out;
	}

	ret = gpio_request(cam_dev->pwdn_pin, "camera");
	if (ret < 0)
		pr_info("aml_cam_init pwdn_pin request failed\n");

	if (!cam_dev->front_back)
		camera0_pwdn_pin = cam_dev->pwdn_pin;
	else
		camera1_pwdn_pin = cam_dev->pwdn_pin;

	cam_dev->rst_pin = of_get_named_gpio(p_node, "gpio_rst-gpios", 0);
	if (cam_dev->rst_pin == 0) {
		pr_info("%s: failed to map gpio_rst !\n", cam_dev->name);
		goto err_out;
	}
	ret = gpio_request(cam_dev->rst_pin, "camera");
	if (ret < 0)
		pr_info("aml_cam_init rst_pin request failed\n");

	if (!cam_dev->front_back)
		camera0_rst_pin = cam_dev->rst_pin;
	else
		camera1_rst_pin = cam_dev->rst_pin;

	cam_info = get_cam_info_by_name(cam_dev->name);
	if (cam_info == NULL) {
		pr_info("camera %s is not support\n", cam_dev->name);
		ret = -1;
		goto err_out;
	}

	of_property_read_u32(p_node, "spread_spectrum",
		&cam_dev->spread_spectrum);

	ret = of_property_read_string(p_node, "bt_path", &str);
	if (ret) {
		pr_info("failed to read bt_path\n");
		cam_dev->bt_path = BT_PATH_GPIO;
	} else {
		pr_info("bt_path :%d\n", cam_dev->bt_path);
		if (strncmp("csi", str, 3) == 0)
			cam_dev->bt_path = BT_PATH_CSI2;
		else if (strncmp("gpio_b", str, 6) == 0)
			cam_dev->bt_path = BT_PATH_GPIO_B;
		else
			cam_dev->bt_path = BT_PATH_GPIO;
	}
	of_property_read_u32(p_node, "bt_path_count", &cam_dev->bt_path_count);
	bt_path_count = cam_dev->bt_path_count;

	cam_dev->pwdn_act = cam_info->pwdn;
	cam_dev->i2c_addr = cam_info->addr;
	pr_info("camer addr: 0x%x\n", cam_dev->i2c_addr);
	pr_info("camer i2c bus: %d\n", cam_dev->i2c_bus_num);

	adapter_node = of_parse_phandle(p_node, "camera-i2c-bus", 0);
	if (adapter_node) {
		adapter = of_find_i2c_adapter_by_node(adapter_node);
		of_node_put(adapter_node);
		if (adapter == NULL) {
			pr_err("%s, failed parse camera-i2c-bus\n",
			   __func__);
			return -1;
		}
	}

	if (adapter && cam_info->probe_func) {
		aml_cam_init(cam_dev);
		if (cam_info->probe_func(adapter) != 1) {
			pr_info("camera %s not on board\n", cam_dev->name);
			ret = -1;
			aml_cam_uninit(cam_dev);
			goto err_out;
		}
		aml_cam_uninit(cam_dev);
	} else {
		pr_err("can not do probe function\n");
		ret = -1;
		goto err_out;
	}

	of_property_read_u32(p_node, "mirror_flip", &cam_dev->m_flip);
	of_property_read_u32(p_node, "vertical_flip", &cam_dev->v_flip);
	of_property_read_u32(p_node, "vdin_path", &cam_dev->vdin_path);

	ret = of_property_read_string(p_node, "max_cap_size", &str);
	if (ret)
		pr_info("failed to read max_cap_size\n");
	else {
		pr_info("max_cap_size :%s\n", str);
		cam_dev->max_cap_size = get_res_size(str);
	}
	if (cam_dev->max_cap_size == SIZE_NULL)
		cam_dev->max_cap_size = cam_info->max_cap_size;

	ret = of_property_read_u32(p_node, "mclk", &mclk);
	if (ret)
		cam_dev->mclk = 24000;
	else
		cam_dev->mclk = mclk;

	ret = of_property_read_u32(p_node, "vcm_mode", &vcm_mode);
	if (ret)
		cam_dev->vcm_mode = 0;
	else
		cam_dev->vcm_mode = vcm_mode;
	pr_info("vcm mode is %d\n", cam_dev->vcm_mode);

	ret = of_property_read_u32(p_node, "flash_support",
				   &cam_dev->flash_support);
	if (cam_dev->flash_support) {
		of_property_read_u32(p_node, "flash_ctrl_level",
				     &cam_dev->flash_ctrl_level);
		ret = of_property_read_string(p_node, "flash_ctrl_pin", &str);
		if (ret) {
			pr_info("%s: failed to get flash_ctrl_pin!\n",
			cam_dev->name);
			cam_dev->flash_support = 0;
		} else {
			cam_dev->flash_ctrl_pin = of_get_named_gpio(p_node,
			    "flash_ctrl_pin", 0);
			if (cam_dev->flash_ctrl_pin == 0) {
				pr_info("%s: failed to map flash_ctrl_pin !\n",
			    cam_dev->name);
				cam_dev->flash_support = 0;
				cam_dev->flash_ctrl_level = 0;
			}
			ret = gpio_request(cam_dev->flash_ctrl_pin, "camera");
			if (ret < 0)
				pr_info("camera flash_ctrl_pin request failed\n");
		}
	}

	ret = of_property_read_u32(p_node, "torch_support",
				   &cam_dev->torch_support);
	if (cam_dev->torch_support) {
		of_property_read_u32(p_node, "torch_ctrl_level",
				     &cam_dev->torch_ctrl_level);
		ret = of_property_read_string(p_node, "torch_ctrl_pin", &str);
		if (ret) {
			pr_info("%s: failed to get torch_ctrl_pin!\n",
			    cam_dev->name);
			cam_dev->torch_support = 0;
		} else {
			cam_dev->torch_ctrl_pin = of_get_named_gpio(p_node,
			   "torch_ctrl_level", 0);
			ret = gpio_request(cam_dev->torch_ctrl_pin, "camera");
			if (ret < 0)
				pr_info("camera torch_ctrl_pin request failed\n");
		}
	}

	ret = of_property_read_string(p_node, "interface", &str);
	if (ret) {
		pr_info("failed to read camera interface \"mipi or dvp\"\n");
		cam_dev->interface = CAM_DVP;
	} else {
		pr_info("camera interface:%s\n", str);
		if (strncmp("dvp", str, 1) == 0)
			cam_dev->interface = CAM_DVP;
		else
			cam_dev->interface = CAM_MIPI;
	}
	if (cam_dev->interface == CAM_MIPI) {
		ret = fill_csi_dev(p_node, cam_dev);
		if (ret < 0)
			goto err_out;
	}

	ret = of_property_read_string(p_node, "bayer_fmt", &str);
	if (ret) {
		pr_info("failed to read camera bayer fmt\n");
		cam_dev->bayer_fmt = TVIN_GBRG;
	} else {
		pr_info("color format:%s\n", str);
		if (strncmp("BGGR", str, 4) == 0)
			cam_dev->bayer_fmt = TVIN_BGGR;
		else if (strncmp("RGGB", str, 4) == 0)
			cam_dev->bayer_fmt = TVIN_RGGB;
		else if (strncmp("GBRG", str, 4) == 0)
			cam_dev->bayer_fmt = TVIN_GBRG;
		else if (strncmp("GRBG", str, 4) == 0)
			cam_dev->bayer_fmt = TVIN_GRBG;
		else
			cam_dev->bayer_fmt = TVIN_GBRG;
	}

	ret = of_property_read_string(p_node, "config_path", &cam_dev->config);
	if (ret)
		pr_info("failed to read config_file path\n");
	else
		pr_info("config path :%s\n", cam_dev->config);

#ifdef DEBUG_DUMP_CAM_INFO
	pr_info("=======cam %s info=======\n"
	       "i2c_bus_num: %d\n"
	       "pwdn_act: %d\n"
	       "front_back: %d\n"
	       "m_flip: %d\n"
	       "v_flip: %d\n"
	       "i2c_addr: 0x%x\n"
	       "config path:%s\n"
	       "bt_path:%d\n",
	       cam_dev->name, cam_dev->i2c_bus_num, cam_dev->pwdn_act,
	       cam_dev->front_back, cam_dev->m_flip, cam_dev->v_flip,
	       cam_dev->i2c_addr, cam_dev->config, cam_dev->bt_path);
#endif /* DEBUG_DUMP_CAM_INFO */

	ret = 0;

err_out:
	return ret;
}

static int do_read_work(char argn, char **argv)
{
	unsigned int dev_addr, reg_addr, data_len = 1, result;
	unsigned int i2c_bus;
	struct i2c_adapter *adapter;

	if (argn < 4) {
		pr_err("args num error");
		return -1;
	}

	if (!strncmp(argv[1], "i2c_bus_ao", 9))
		i2c_bus = 4;
	else if (!strncmp(argv[1], "i2c_bus_0", 9))
		i2c_bus = 0;
	else if (!strncmp(argv[1], "i2c_bus_1", 9))
		i2c_bus = 1;
	else if (!strncmp(argv[1], "i2c_bus_2", 9))
		i2c_bus = 2;
	else if (!strncmp(argv[1], "i2c_bus_3", 9))
		i2c_bus = 3;
	else {
		pr_err("bus name error!\n");
		return -1;
	}

	adapter = i2c_get_adapter(i2c_bus);

	if (adapter == NULL) {
		pr_info("no adapter!\n");
		return -1;
	}

	dev_addr = kstrtol(argv[2], 16, NULL);
	reg_addr = kstrtol(argv[3], 16, NULL);
	if (argn == 5) {
		pr_info("argv[4] is %s\n", argv[4]);
		data_len = kstrtol(argv[4], 16, NULL);
	}

	if (reg_addr > 256) {
		if (data_len != 2) {
			result = aml_i2c_get_byte(adapter, dev_addr, reg_addr);
			pr_info("register [0x%04x]=0x%02x\n", reg_addr, result);
		} else {
			result = aml_i2c_get_word(adapter, dev_addr, reg_addr);
			pr_info("register [0x%04x]=0x%04x\n", reg_addr, result);
		}
	} else {
		if (data_len != 2) {
			result = aml_i2c_get_byte_add8(adapter,
			dev_addr, reg_addr);
			pr_info("register [0x%02x]=0x%02x\n", reg_addr, result);
		} else {
			result = aml_i2c_get_word_add8(adapter,
			dev_addr, reg_addr);
			pr_info("register [0x%02x]=0x%04x\n", reg_addr, result);
		}
	}

	return 0;
}

static int do_write_work(char argn, char **argv)
{
	unsigned int dev_addr, reg_addr, reg_val, data_len = 1, ret = 0;
	unsigned int i2c_bus;
	struct i2c_adapter *adapter;

	if (argn < 5) {
		pr_err("args num error");
		return -1;
	}

	if (!strncmp(argv[1], "i2c_bus_0", 9))
		i2c_bus = 0;
	else if (!strncmp(argv[1], "i2c_bus_1", 9))
		i2c_bus = 1;
	else if (!strncmp(argv[1], "i2c_bus_2", 9))
		i2c_bus = 2;
	else if (!strncmp(argv[1], "i2c_bus_3", 9))
		i2c_bus = 3;
	else if (!strncmp(argv[1], "i2c_bus_ao", 9))
		i2c_bus = 4;
	else {
		pr_err("bus name error!\n");
		return -1;
	}

	adapter = i2c_get_adapter(i2c_bus);

	if (adapter == NULL) {
		pr_info("no adapter!\n");
		return -1;
	}

	dev_addr = kstrtol(argv[2], 16, NULL);
	reg_addr = kstrtol(argv[3], 16, NULL);
	reg_val = kstrtol(argv[4], 16, NULL);
	if (argn == 6)
		data_len = kstrtol(argv[5], 16, NULL);
	if (reg_addr > 256) {
		if (data_len != 2) {
			if (aml_i2c_put_byte(adapter, dev_addr,
			    reg_addr, reg_val) < 0) {
				pr_err("write error\n");
				ret = -1;
			} else {
				pr_info("write ok\n");
				ret = 0;
			}
		} else {
			if (aml_i2c_put_word(adapter, dev_addr,
			    reg_addr, reg_val) < 0) {
				pr_err("write error\n");
				ret = -1;
			} else {
				pr_info("write ok\n");
				ret = 0;
			}
		}
	} else {
		if (data_len != 2) {
			if (aml_i2c_put_byte_add8(adapter, dev_addr,
			    reg_addr, reg_val) < 0) {
				pr_err("write error\n");
				ret = -1;
			} else {
				pr_info("write ok\n");
				ret = 0;
			}
		} else {
			if (aml_i2c_put_word_add8(adapter, dev_addr,
			    reg_addr, reg_val) < 0) {
				pr_err("write error\n");
				ret = -1;
			} else {
				pr_info("write ok\n");
				ret = 0;
			}
		}
	}

	return ret;
}

static struct class *cam_clsp;

static ssize_t show_help(struct class *class, struct class_attribute *attr,
			 char *buf)
{
	ssize_t size = 0;

	pr_info(
		"echo [read | write] i2c_bus_type device_address register_address [value] [data_len] > i2c_debug\n"
		"i2c_bus_type are: i2c_bus_ao, i2c_bus_a, i2c_bus_b, i2c_bus_c, i2c_bus_d\n"
		"e.g.: echo read i2c_bus_ao 0x3c 0x18 1\n"
		"      echo write i2c_bus_ao 0x3c 0x18 0x24 1\n");
	return size;
}

static ssize_t store_i2c_debug(struct class *class,
			       struct class_attribute *attr, const char *buf,
			       size_t count)
{
	int argn;
	char *buf_work, *p, *para;
	char cmd;
	char *argv[6];

	buf_work = kstrdup(buf, GFP_KERNEL);
	p = buf_work;

	for (argn = 0; argn < 6; argn++) {
		para = strsep(&p, " ");
		if (para == NULL)
			break;
		argv[argn] = para;
		pr_info("argv[%d] = %s\n", argn, para);
	}

	if (argn < 4 || argn > 6)
		goto end;

	cmd = argv[0][0];
	switch (cmd) {
	case 'r':
	case 'R':
		do_read_work(argn, argv);
		break;
	case 'w':
	case 'W':
		do_write_work(argn, argv);
		break;
	}
	return count;
end:
	pr_err("error command!\n");
	kfree(buf_work);
	return -EINVAL;
}

static LIST_HEAD(info_head);

static ssize_t cam_info_show(struct class *class, struct class_attribute *attr,
			     char *buf)
{
	struct list_head *p;
	struct aml_cam_info_s *cam_info = NULL;
	int count = 0;

	if (!list_empty(&info_head)) {
		count += sprintf(&buf[count],
	    "name\t\tversion\t\t\t\tface_dir\t"
			 "i2c_addr\n");
		list_for_each(p, &info_head) {
			cam_info = list_entry(p, struct aml_cam_info_s,
			    info_entry);
		}
	}
	return count;
}

static struct class_attribute aml_cam_attrs[] = {
	__ATTR(i2c_debug, 0644, show_help, store_i2c_debug),
	__ATTR_RO(cam_info), __ATTR(help, 0644, show_help, NULL),
	__ATTR_NULL,
};

int aml_cam_info_reg(struct aml_cam_info_s *cam_info)
{
	int ret = -1;

	if (cam_info) {
		list_add(&cam_info->info_entry, &info_head);
		ret = 0;
	}
	return ret;
}

int aml_cam_info_unreg(struct aml_cam_info_s *cam_info)
{
	int ret = -1;
	struct list_head *p, *n;
	struct aml_cam_info_s *tmp_info = NULL;

	if (cam_info) {
		list_for_each_safe(p, n, &info_head) {
			tmp_info = list_entry(p, struct aml_cam_info_s,
				    info_entry);
			if (tmp_info == cam_info) {
				list_del(p);
				return 0;
			}
		}
	}
	return ret;
}

static int aml_cams_probe(struct platform_device *pdev)
{
	struct device_node *cams_node = pdev->dev.of_node;
	struct device_node *child;
	struct i2c_board_info board_info;
	struct i2c_adapter *adapter = NULL;
	struct device_node *adapter_node = NULL;
	struct timeval camera_start;
	struct timeval camera_end;
	int i;
	unsigned long time_use = 0;

	temp_cam = kzalloc(sizeof(struct aml_cam_info_s), GFP_KERNEL);
	if (!temp_cam)
		return -ENOMEM;

	cam_pdev = pdev;
	do_gettimeofday(&camera_start);
	for_each_child_of_node(cams_node, child) {

		memset(temp_cam, 0, sizeof(struct aml_cam_info_s));

		if (fill_cam_dev(child, temp_cam))
			continue;

		/*register exist camera*/
		memset(&board_info, 0, sizeof(board_info));
		strlcpy(board_info.type, temp_cam->name, I2C_NAME_SIZE);

		adapter_node = of_parse_phandle(child, "camera-i2c-bus", 0);
		if (adapter_node) {
			adapter = of_get_i2c_adapter_by_node(adapter_node);
			of_node_put(adapter_node);
			if (adapter == NULL) {
				pr_err("%s, failed parse camera-i2c-bus\n",
				   __func__);
				return -1;
			}
		} else {
			pr_err("adapter node is NULL.\n");
			return -1;
		}
		board_info.addr = temp_cam->i2c_addr;
		board_info.platform_data = temp_cam;
		pr_info("new i2c device\n");
		/*i2c_new_existing_device(adapter, &board_info)*/
		i2c_new_device(adapter, &board_info);
	}
	do_gettimeofday(&camera_end);
	time_use = (camera_end.tv_sec - camera_start.tv_sec) * 1000 +
		(camera_end.tv_usec - camera_start.tv_usec) / 1000;
	pr_info("camera probe cost time = %ldms\n", time_use);

	pr_info("aml probe finish\n");
	cam_clsp = class_create(THIS_MODULE, "aml_camera");
	for (i = 0; aml_cam_attrs[i].attr.name; i++) {
		if (class_create_file(cam_clsp, &aml_cam_attrs[i]) < 0)
			return -1;
	}

	return 0;
}

static int aml_cams_remove(struct platform_device *pdev)
{
	if (camera0_pwdn_pin != 0)
		gpio_free(camera0_pwdn_pin);
	if (camera0_rst_pin != 0)
		gpio_free(camera0_rst_pin);
	if (camera1_pwdn_pin != 0)
		gpio_free(camera1_pwdn_pin);
	if (camera1_rst_pin != 0)
		gpio_free(camera1_rst_pin);

	kfree(temp_cam);
	temp_cam = NULL;

	return 0;
}

static const struct of_device_id cams_prober_dt_match[] = {{
		.compatible =
		"amlogic, cams_prober",
	}, {},
};

static struct platform_driver aml_cams_prober_driver = {
	.probe = aml_cams_probe, .remove = aml_cams_remove, .driver = {
		.name =
		"aml_cams_prober", .owner = THIS_MODULE, .of_match_table =
		cams_prober_dt_match,
	},
};

static int __init aml_cams_prober_init(void)
{
	if (platform_driver_register(&aml_cams_prober_driver)) {
		pr_err("aml_cams_probre_driver register failed\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit aml_cams_prober_exit(void)
{
	bt_path_count = 0;
	platform_driver_unregister(&aml_cams_prober_driver);
}

module_init(aml_cams_prober_init);
module_exit(aml_cams_prober_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Amlogic Cameras prober driver");

