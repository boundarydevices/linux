/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define DEBUG
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
//#include "msm_dba_internal.h"
//#include <linux/mdss_io_util.h>

#define EDID_ADDR 0x50
#define LT8912_REG_CHIP_REVISION_0 (0x00)
#define LT8912_REG_CHIP_REVISION_1 (0x01)

#define LT8912_VAL_CHIP_REVISION_0 (0x12)
#define LT8912_VAL_CHIP_REVISION_1 (0xB2)
#define LT8912_DSI_CEC_I2C_ADDR_REG (0xE1)
#define LT8912_RESET_DELAY (100)

#define PINCTRL_STATE_ACTIVE    "pmx_lt8912_active"
#define PINCTRL_STATE_SUSPEND   "pmx_lt8912_suspend"

#define MDSS_MAX_PANEL_LEN      256
#define EDID_SEG_SIZE 0x100
/* size of audio and speaker info Block */
#define AUDIO_DATA_SIZE 32

#define MAIN_LANE_EN	0x40

#define CEC_DSI_SETTLE	0x11
#define CEC_DSI_LANE	0x13
#define CEC_DSI_HSYNC	0x18
#define CEC_DSI_VSYNC	0x19
#define CEC_DSI_HACTIVE	0x1c
#define CEC_DSI_HTOTAL	0x34
#define CEC_DSI_VTOTAL	0x36
#define CEC_DSI_VBP	0x38
#define CEC_DSI_VFP	0x3a
#define CEC_DSI_HBP	0x3c
#define CEC_DSI_HFP	0x3e

/* 0x94 interrupts */
#define HPD_INT_ENABLE           BIT(7)
#define MONITOR_SENSE_INT_ENABLE BIT(6)
#define ACTIVE_VSYNC_EDGE        BIT(5)
#define AUDIO_FIFO_FULL          BIT(4)
#define EDID_READY_INT_ENABLE    BIT(2)

#define MAX_WAIT_TIME (100)
#define MAX_RW_TRIES (3)

/* 0x95 interrupts */
#define CEC_TX_READY             BIT(5)
#define CEC_TX_ARB_LOST          BIT(4)
#define CEC_TX_RETRY_TIMEOUT     BIT(3)
#define CEC_TX_RX_BUF3_READY     BIT(2)
#define CEC_TX_RX_BUF2_READY     BIT(1)
#define CEC_TX_RX_BUF1_READY     BIT(0)

#define HPD_INTERRUPTS           (HPD_INT_ENABLE | \
					MONITOR_SENSE_INT_ENABLE)
#define EDID_INTERRUPTS          EDID_READY_INT_ENABLE
#define CEC_INTERRUPTS           (CEC_TX_READY | \
					CEC_TX_ARB_LOST | \
					CEC_TX_RETRY_TIMEOUT | \
					CEC_TX_RX_BUF3_READY | \
					CEC_TX_RX_BUF2_READY | \
					CEC_TX_RX_BUF1_READY)

#define CFG_HPD_INTERRUPTS       BIT(0)
#define CFG_EDID_INTERRUPTS      BIT(1)
#define CFG_CEC_INTERRUPTS       BIT(3)

#define MAX_OPERAND_SIZE	14
#define CEC_MSG_SIZE            (MAX_OPERAND_SIZE + 2)


enum lt8912_i2c_addr {
	I2C_ADDR_MAIN = 0x48,
	I2C_ADDR_CEC_DSI = 0x49,
	I2C_ADDR_I2S = 0x4a,
};

enum lt8912_cec_buf {
	LT8912_CEC_BUF1,
	LT8912_CEC_BUF2,
	LT8912_CEC_BUF3,
	LT8912_CEC_BUF_MAX,
};

struct lt8912_reg_cfg {
	u8 i2c_addr;
	u8 reg;
	u8 val;
	int sleep_in_ms;
};

struct lt8912_cec_msg {
	u8 buf[CEC_MSG_SIZE];
	u8 timestamp;
	bool pending;
};

struct lt {
	struct i2c_client *client;
	u8 video_mode;
	struct gpio_desc *gp_reset;
	int irq;
	struct regulator *avdd;
	bool audio;
	bool disable_gpios;
	bool is_power_on;
	void *edid_data;
	u8 edid_buf[EDID_SEG_SIZE];
	u8 audio_spkr_data[AUDIO_DATA_SIZE];
	struct workqueue_struct *workq;
	//struct delayed_work lt8912_intr_work_id;
	struct delayed_work lt8912_check_hpd_work_id;
//	struct msm_dba_device_info dev_info;
	struct lt8912_cec_msg cec_msg[LT8912_CEC_BUF_MAX];
	struct i2c_client *i2c_client;
	struct mutex ops_mutex;
	int prev_connected;
	struct device_node *disp_dsi;
	struct i2c_adapter *ddc;
	u8 last_reg;
	u8 last_reg_cec;
	u8 dsi_lanes;
};

static char mdss_mdp_panel[MDSS_MAX_PANEL_LEN];

static struct lt8912_reg_cfg lt8912_init_setup[] = {
/* Digital clock en*/
	/* power down */
	{I2C_ADDR_MAIN, 0x08, 0xff, 0},
	/* HPD override */
	{I2C_ADDR_MAIN, 0x09, 0xff, 0},
	/* color space */
	{I2C_ADDR_MAIN, 0x0a, 0xff, 0},
	/* Fixed */
	{I2C_ADDR_MAIN, 0x0b, 0x7c, 0},
	/* HDCP */
	{I2C_ADDR_MAIN, 0x0c, 0xff, 0},
	{I2C_ADDR_MAIN, 0x42, 0x04, 0},

/*Tx Analog*/
	/* Fixed */
	{I2C_ADDR_MAIN, 0x31, 0xb1, 0},  
	/* V1P2 */
	{I2C_ADDR_MAIN, 0x32, 0xb1, 0}, 
	/* Fixed */
	{I2C_ADDR_MAIN, 0x33, 0x0e, 0},  
	/* Fixed */
	{I2C_ADDR_MAIN, 0x37, 0x00, 0},
	/* Fixed */
	{I2C_ADDR_MAIN, 0x38, 0x22, 0},
	/* Fixed */
	{I2C_ADDR_MAIN, 0x60, 0x82, 0},
/*Cbus Analog*/
	/* Fixed */
	{I2C_ADDR_MAIN, 0x39, 0x45, 0},
	{I2C_ADDR_MAIN, 0x3a, 0x00, 0}, //20180718
	{I2C_ADDR_MAIN, 0x3b, 0x00, 0},
/*HDMI Pll Analog*/	
	{I2C_ADDR_MAIN, 0x44, 0x31, 0},
	/* Fixed */
	{I2C_ADDR_MAIN, 0x55, 0x44, 0},
	/* Fixed */
	{I2C_ADDR_MAIN, 0x57, 0x01, 0},
	{I2C_ADDR_MAIN, 0x5a, 0x02, 0},
/*MIPI Analog*/
	/* Fixed */
	{I2C_ADDR_MAIN, 0x3e, 0xd6, 0},  //0xde.  //0xf6 = pn swap
	{I2C_ADDR_MAIN, 0x3f, 0xd4, 0},
	{I2C_ADDR_MAIN, 0x41, 0x3c, 0},
	{I2C_ADDR_MAIN, 0xB2, 0x00, 0},	/* 0 DVI, 1 HDMI */
	{I2C_ADDR_CEC_DSI, 0x12,0x04, 0},
};

static struct lt8912_reg_cfg lt8912_mipi_basic_set[] = {
	{I2C_ADDR_CEC_DSI, 0x14,0x00, 0}, 
	{I2C_ADDR_CEC_DSI, 0x15,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x1a,0x03, 0}, 
	{I2C_ADDR_CEC_DSI, 0x1b,0x03, 0}, 
};

static struct lt8912_reg_cfg lt8912_ddsconfig[] = {
	{I2C_ADDR_CEC_DSI, 0x4e,0xff, 0},
	{I2C_ADDR_CEC_DSI, 0x4f,0x56, 0},
	{I2C_ADDR_CEC_DSI, 0x50,0x69, 0},
	{I2C_ADDR_CEC_DSI, 0x51,0x80, 0},
	/*
	{I2C_ADDR_CEC_DSI, 0x1f,0x90, 0},
	{I2C_ADDR_CEC_DSI, 0x20,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x21,0x68, 0},
	{I2C_ADDR_CEC_DSI, 0x22,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x23,0x5E, 0},
	{I2C_ADDR_CEC_DSI, 0x24,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x25,0x54, 0},
	{I2C_ADDR_CEC_DSI, 0x26,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x27,0x90, 0},
	{I2C_ADDR_CEC_DSI, 0x28,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x29,0x68, 0},
	{I2C_ADDR_CEC_DSI, 0x2a,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x2b,0x5E, 0},
	{I2C_ADDR_CEC_DSI, 0x2c,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x2d,0x54, 0},
	{I2C_ADDR_CEC_DSI, 0x2e,0x01, 0},
	*/
	{I2C_ADDR_CEC_DSI, 0x1f,0x5e, 0},
	{I2C_ADDR_CEC_DSI, 0x20,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x21,0x2c, 0},
	{I2C_ADDR_CEC_DSI, 0x22,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x23,0xfa, 0},
	{I2C_ADDR_CEC_DSI, 0x24,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x25,0xc8, 0},
	{I2C_ADDR_CEC_DSI, 0x26,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x27,0x5e, 0},
#if 1
	{I2C_ADDR_CEC_DSI, 0x28,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x29,0x2c, 0},
	{I2C_ADDR_CEC_DSI, 0x2a,0x01, 0},
	{I2C_ADDR_CEC_DSI, 0x2b,0xfa, 0},
	{I2C_ADDR_CEC_DSI, 0x2c,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x2d,0xc8, 0},
	{I2C_ADDR_CEC_DSI, 0x2e,0x00, 0},
#endif
	{I2C_ADDR_CEC_DSI, 0x42,0x64, 0},
	{I2C_ADDR_CEC_DSI, 0x43,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x44,0x04, 0},
	{I2C_ADDR_CEC_DSI, 0x45,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x46,0x59, 0},
	{I2C_ADDR_CEC_DSI, 0x47,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x48,0xf2, 0},
	{I2C_ADDR_CEC_DSI, 0x49,0x06, 0},
	{I2C_ADDR_CEC_DSI, 0x4a,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x4b,0x72, 0},
	{I2C_ADDR_CEC_DSI, 0x4c,0x45, 0},
	{I2C_ADDR_CEC_DSI, 0x4d,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x52,0x08, 0},
	{I2C_ADDR_CEC_DSI, 0x53,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x54,0xb2, 0},
	{I2C_ADDR_CEC_DSI, 0x55,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x56,0xe4, 0},
	{I2C_ADDR_CEC_DSI, 0x57,0x0d, 0},
	{I2C_ADDR_CEC_DSI, 0x58,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x59,0xe4, 0},
	{I2C_ADDR_CEC_DSI, 0x5a,0x8a, 0},
	{I2C_ADDR_CEC_DSI, 0x5b,0x00, 0},
	{I2C_ADDR_CEC_DSI, 0x5c,0x34, 0},
	{I2C_ADDR_CEC_DSI, 0x1e,0x4f, 0},
	{I2C_ADDR_CEC_DSI, 0x51,0x00, 0},
};

static struct lt8912_reg_cfg lt8912_rxlogicres[] = {
	{I2C_ADDR_MAIN, 0x03,0x7f, 100},
	{I2C_ADDR_MAIN, 0x03,0xff, 0},
};

static struct lt8912_reg_cfg I2S_cfg[] = {
	{I2C_ADDR_MAIN, 0xB2, 0x01, 0},
	{I2C_ADDR_I2S, 0x06, 0x08, 0},
	{I2C_ADDR_I2S, 0x07, 0xf0, 0},

	{I2C_ADDR_I2S, 0x0f, 0x28, 0}, //Audio 16bit, 48K 
	
	{I2C_ADDR_I2S, 0x34, 0xe2, 0}, //sclk = 64fs, 0xd2; sclk = 32fs, 0xe2.
};


static struct lt8912_reg_cfg lt8912_lvds_bypass_cfg[] = {
	 //lvds power up
	{I2C_ADDR_MAIN, 0x44, 0x30, 0},
	{I2C_ADDR_MAIN, 0x51, 0x05, 0},

	 //core pll bypass
	{I2C_ADDR_MAIN, 0x50, 0x24, 0},//cp=50uA
	{I2C_ADDR_MAIN, 0x51, 0x2d, 0},//Pix_clk as reference,second order passive LPF PLL
	{I2C_ADDR_MAIN, 0x52, 0x04, 0},//loopdiv=0;use second-order PLL
	{I2C_ADDR_MAIN, 0x69, 0x0e, 0},//CP_PRESET_DIV_RATIO
	{I2C_ADDR_MAIN, 0x69, 0x8e, 0},
	{I2C_ADDR_MAIN, 0x6a, 0x00, 0},
	{I2C_ADDR_MAIN, 0x6c, 0xb8, 0},//RGD_CP_SOFT_K_EN,RGD_CP_SOFT_K[13:8]
	{I2C_ADDR_MAIN, 0x6b, 0x51, 0},

	{I2C_ADDR_MAIN, 0x04, 0xfb, 0},//core pll reset
	{I2C_ADDR_MAIN, 0x04, 0xff, 0},

	//scaler bypass
	{I2C_ADDR_MAIN, 0x7f, 0x00, 0},//disable scaler
	{I2C_ADDR_MAIN, 0xa8, 0x13, 0},//0x13 : JEIDA, 0x33:VSEA


	{I2C_ADDR_MAIN, 0x02, 0xf7, 0},//lvds pll reset
	{I2C_ADDR_MAIN, 0x02, 0xff, 0},
	{I2C_ADDR_MAIN, 0x03, 0xcf, 0},
	{I2C_ADDR_MAIN, 0x03, 0xff, 0},
};


static int lt8912_write(struct lt *lt, u8 addr, u8 reg, u8 val)
{
	struct i2c_msg msgs[1];
	int ret = 0;
	u8 wbuf[4] = {0};

	if (!lt) {
		pr_debug("%s: Invalid argument\n", __func__);
		return -EINVAL;
	}

	wbuf[0] = reg;
	wbuf[1] = val;

	msgs[0].addr = addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = wbuf;

	ret = i2c_transfer(lt->client->adapter, msgs, 1);
	if (ret < 0) {
		pr_err("%s:addr=%x reg=%x ret=%d\n", __func__, addr, reg, ret);
		return ret;
	}
	return 0;
}

static int lt8912_write_w(struct lt *lt, u8 addr, u8 reg, u16 val)
{
	struct i2c_msg msgs[1];
	int ret = 0;
	u8 wbuf[4] = {0};

	if (!lt) {
		pr_debug("%s: Invalid argument\n", __func__);
		return -EINVAL;
	}

	wbuf[0] = reg;
	wbuf[1] = val & 0xff;
	wbuf[2] = val >> 8;

	msgs[0].addr = addr;
	msgs[0].flags = 0;
	msgs[0].len = 3;
	msgs[0].buf = wbuf;

	ret = i2c_transfer(lt->client->adapter, msgs, 1);
	if (ret < 0) {
		pr_err("%s:addr=%x reg=%x ret=%d\n", __func__, addr, reg, ret);
		return ret;
	}
	return 0;
}

static int lt8912_read_adapter(struct i2c_adapter *adapter, u8 addr, u8 reg, char *buf, u32 size)
{
	struct i2c_msg msgs[2];
	u8 wbuf[4];
	int ret;

	if (!adapter) {
		pr_debug("%s: Invalid argument\n", __func__);
		return -EINVAL;
	}

	wbuf[0] = reg;
	msgs[0].addr = addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = wbuf;

	msgs[1].addr = addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = size;
	msgs[1].buf = buf;

	ret = i2c_transfer(adapter, msgs, 2);
	if (ret < 0) {
		pr_err("%s:addr=%x reg=%x size=0x%x ret=%d\n", __func__, addr, reg, size, ret);
		return ret;
	}
	return 0;
}

static int lt8912_read(struct lt *lt, u8 addr, u8 reg, char *buf, u32 size)
{
	return lt8912_read_adapter(lt->client->adapter, addr, reg, buf, size);
}

static int lt8912_read_ddc(struct lt *lt, u8 addr, u8 reg, char *buf, u32 size)
{
	return lt8912_read_adapter(lt->ddc ? lt->ddc : lt->client->adapter, addr, reg, buf, size);
}

#if 0
static int lt8912_dump_debug_info(struct lt *lt, u32 flags)
{
	int rc = 0;
	u8 byte_val = 0;
	u16 addr = 0;

	if (!lt) {
		pr_err("%s: lt is NULL\n", __func__);
		return -EINVAL;
	}

	/* dump main addr*/
	pr_err("========Main I2C Start==========\n");
	for (addr = 0; addr <= 0xFF; addr++) {
		rc = lt8912_read(lt, I2C_ADDR_MAIN,
			(u8)addr, &byte_val, 1);
		if (rc)
			pr_err("%s: read reg=0x%02x failed @ addr=I2C_ADDR_MAIN\n",
				__func__, addr);
		else
			pr_err("0x%02x -> 0x%02X\n", addr, byte_val);
	}
	pr_err("========Main I2C End==========\n");
	/* dump CEC addr*/
	pr_err("=======CEC I2C Start=========\n");
	for (addr = 0; addr <= 0xFF; addr++) {
		rc = lt8912_read(lt, I2C_ADDR_CEC_DSI,
			(u8)addr, &byte_val, 1);
		if (rc)
			pr_err("%s: read reg=0x%02x failed @ addr=I2C_ADDR_CEC_DSI\n",
				__func__, addr);
		else
			pr_err("0x%02x -> 0x%02X\n", addr, byte_val);
	}
	pr_err("========CEC I2C End==========\n");

	return rc;
}
#endif

static void lt8912_write_array(struct lt *lt,
	struct lt8912_reg_cfg *cfg, int size)
{
	int ret = 0;
	int i;

	size = size / sizeof(struct lt8912_reg_cfg);
	for (i = 0; i < size; i++) {
		ret = lt8912_write(lt, cfg[i].i2c_addr,
			cfg[i].reg, cfg[i].val);
		if (ret != 0){
			pr_err("%s: lt reg write %02X to %02X failed.\n",
				__func__, cfg[i].val, cfg[i].reg);
		}
		if (cfg[i].sleep_in_ms)
			msleep(cfg[i].sleep_in_ms);
	}
}

static int lt8912_read_device_rev(struct lt *lt)
{
	u8 rev = 0;
	int ret;

	ret = lt8912_read(lt, I2C_ADDR_MAIN, LT8912_REG_CHIP_REVISION_0,
                                                        &rev, 1);
	if (ret || rev != LT8912_VAL_CHIP_REVISION_0) {
		pr_err("check chip revision not match reg = 0x%x, val = 0x%x expected 0x%x, %d\n",
			LT8912_REG_CHIP_REVISION_0, rev,
			LT8912_VAL_CHIP_REVISION_0, ret);
		return -ENODEV;
	}
	pr_debug("%s: LT8912_REG_CHIP_REVISION_0= %d\n", __func__, rev);
	ret = lt8912_read(lt, I2C_ADDR_MAIN, LT8912_REG_CHIP_REVISION_1,
			  &rev, 1);
	if (ret || rev != LT8912_VAL_CHIP_REVISION_1) {
		pr_err("check chip revision not match reg = 0x%x, val = 0x%x expected 0x%x, %d\n",
			LT8912_REG_CHIP_REVISION_1, rev,
			LT8912_VAL_CHIP_REVISION_1, ret);
		return -ENODEV;
	}
	pr_debug("%s: LT8912_REG_CHIP_REVISION_1= %d\n", __func__, rev);

	return ret;
}

static int lt8912_parse_dt(struct device *dev,
	struct lt *lt)
{
	struct device_node *np = dev->of_node;
	struct gpio_desc *gp_reset;
	int ret = 0;

	lt->audio = of_property_read_bool(np, "adi,enable-audio");

	gp_reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH); /* active */
	if (IS_ERR(gp_reset)) {
		ret = PTR_ERR(gp_reset);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to get reset gpio: %d\n", ret);
		return ret;
	}
	lt->gp_reset = gp_reset;

	lt->avdd = regulator_get_optional(dev, "avdd");
	if (IS_ERR(lt->avdd)) {
		ret = PTR_ERR(lt->avdd);
		if (ret == -ENODEV) {
			ret = 0;
		} else if (ret != -EPROBE_DEFER) {
			dev_err(dev, "Regulator get failed avdd:%d\n", ret);
		}
	}

	return ret;
}

static int lt8912_gpio_configure(struct lt *lt, bool on)
{
	gpiod_set_value_cansleep(lt->gp_reset, on ? 0 : 1);
	return 0;
}

enum hpd_callback_event {
	HPD_CONNECT,
	HPD_DISCONNECT,
};

static void lt8912_notify_clients(struct lt *lt,
		enum hpd_callback_event event)
{
#if 0
	struct msm_dba_client_info *c;
	struct list_head *pos = NULL;

	list_for_each(pos, &dev->client_list) {
		c = list_entry(pos, struct msm_dba_client_info, list);

		pr_debug("%s: notifying event %d to client %s\n", __func__,
			event, c->client_name);

		if (c && c->cb)
			c->cb(c->cb_data, event);
	}
#endif
}

int lt8912_read_edid(struct lt *lt, u32 size, char *edid_buf)
{
	u32 read_size = size / 2;
	u8 edid_addr = 0;
	int ndx;
	int ret;

	if (!lt || !edid_buf)
		return 0;

	udelay(10 * 1000);

	ret = lt8912_read(lt, I2C_ADDR_MAIN, 0x43, &edid_addr, 1);
	if (ret < 0)
		return ret;

	pr_debug("%s: edid address 0x%x\n", __func__, edid_addr);

	ret = lt8912_read_ddc(lt, EDID_ADDR, 0x00, edid_buf, read_size);
	if (ret < 0)
		return ret;

	ret = lt8912_read_ddc(lt, EDID_ADDR, read_size,
		edid_buf + read_size, read_size);
	if (ret < 0)
		return ret;

	for (ndx = 0; ndx < size; ndx += 4)
		pr_debug("%s: EDID[%02x-%02x] %02x %02x %02x %02x\n",
			__func__, ndx, ndx + 3,
			edid_buf[ndx + 0], edid_buf[ndx + 1],
			edid_buf[ndx + 2], edid_buf[ndx + 3]);

	return ret;
}

static int lt8912_video_setup(struct lt *lt)
{
	struct videomode vm;
	u32 hactive, h_total, hpw, hfp, hbp;
	u32 vactive, v_total, vpw, vfp, vbp;
	u8 settle = 0x08;
	int ret;

	if (!lt) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = of_get_videomode(lt->disp_dsi, &vm, 0);
	if (ret < 0)
		return ret;

	hactive = vm.hactive;
	hfp = vm.hfront_porch;
	hpw = vm.hsync_len;
	hbp = vm.hback_porch;
	h_total = hactive + hfp + hpw + hbp;

	vactive = vm.vactive;
	vfp = vm.vfront_porch;
	vpw = vm.vsync_len;
	vbp = vm.vback_porch;
	v_total = vactive + vfp + vpw + vbp;

	pr_debug("h_total 0x%x, xres 0x%x, hfp 0x%x, hpw 0x%x, hbp 0x%x, lanes %d\n",
		h_total, hactive, hfp, hpw, hbp, lt->dsi_lanes);

	pr_debug("v_total 0x%x, yres 0x%x, vfp 0x%x, vpw 0x%x, vbp 0x%x\n",
		v_total, vactive, vfp, vpw, vbp);

	if (vactive == 480)
		settle = 4;
	else if (vactive == 720)
		settle = 0x0a;
	else if (vactive == 1080)
		settle = 0x0a;
	else if (vactive == 800)
		settle = 0x08;
	else
		pr_err("%s: unsupported yres = %d\n", __func__, vactive);

	lt8912_write(lt, I2C_ADDR_CEC_DSI, 0x10, 0x01);
	lt8912_write(lt, I2C_ADDR_CEC_DSI, CEC_DSI_SETTLE, settle);
	lt8912_write(lt, I2C_ADDR_CEC_DSI, CEC_DSI_HSYNC, hpw);
	lt8912_write(lt, I2C_ADDR_CEC_DSI, CEC_DSI_VSYNC, vpw);
	lt8912_write_w(lt, I2C_ADDR_CEC_DSI, CEC_DSI_HACTIVE, hactive);
	lt8912_write(lt, I2C_ADDR_CEC_DSI, 0x2f, 0x0c);

	lt8912_write_w(lt, I2C_ADDR_CEC_DSI, CEC_DSI_HTOTAL, h_total);
	lt8912_write_w(lt, I2C_ADDR_CEC_DSI, CEC_DSI_VTOTAL, v_total);
	lt8912_write_w(lt, I2C_ADDR_CEC_DSI, CEC_DSI_VBP, vbp);
	lt8912_write_w(lt, I2C_ADDR_CEC_DSI, CEC_DSI_VFP, vfp);
	lt8912_write_w(lt, I2C_ADDR_CEC_DSI, CEC_DSI_HBP, hbp);
	lt8912_write_w(lt, I2C_ADDR_CEC_DSI, CEC_DSI_HFP, hfp);
	return vactive;
}

static int lt8912_video_on(struct lt *lt, bool on, u32 flags)
{
	int ret = -EINVAL;
	int yres;

	pr_debug("%s: enter\n", __func__);
	mutex_lock(&lt->ops_mutex);


	yres = lt8912_video_setup(lt);
	if (yres < 0) {
		ret = yres;
		goto end;
	}

	lt8912_write_array(lt, lt8912_ddsconfig,
				sizeof(lt8912_ddsconfig));

	lt8912_write_array(lt, lt8912_rxlogicres,
				sizeof(lt8912_rxlogicres));

	lt8912_write_array(lt,lt8912_lvds_bypass_cfg,
	 			sizeof(lt8912_lvds_bypass_cfg));

	ret = 0;

end:
	mutex_unlock(&lt->ops_mutex);
	return ret;
}

/* Device Operations */
static int lt8912_power_on(struct lt *lt, bool on, u32 flags)
{
	int ret = -EINVAL;

	pr_debug("%s: %d\n", __func__, on);
	mutex_lock(&lt->ops_mutex);

	if (on && !lt->is_power_on) {
		u32 lanes = lt->dsi_lanes;

		lt8912_write_array(lt, lt8912_init_setup,
					sizeof(lt8912_init_setup));

		lt8912_write(lt, I2C_ADDR_CEC_DSI, CEC_DSI_LANE,
			lanes & 3);
		lt8912_write(lt, I2C_ADDR_MAIN, MAIN_LANE_EN,
			5 | (((1 << lanes) - 1) << (7 - lanes)));

		lt8912_write_array(lt, lt8912_mipi_basic_set,
                                sizeof(lt8912_mipi_basic_set));

		lt->is_power_on = true;
	} else if (!on) {
		/* power down hdmi */
		lt->is_power_on = false;
		lt8912_notify_clients(lt, HPD_DISCONNECT);
	}

	mutex_unlock(&lt->ops_mutex);
	return ret;
}



/*static int lt8912_edid_read_init(struct lt *lt)
{
	int ret = -EINVAL;

	if (!lt) {
		pr_err("%s: invalid lt\n", __func__);
		goto end;
	}

	//initiate edid read in lt
	//lt8912_write(lt, I2C_ADDR_MAIN, 0x41, 0x10);
	//lt8912_write(lt, I2C_ADDR_MAIN, 0xC9, 0x13);

end:
	return ret;
}
*/

/*static int lt8912_disable_interrupts(struct lt *lt, int interrupts)
{
	int ret = 0;
	u8 reg_val, init_reg_val;

	if (!lt) {
		pr_err("%s: invalid input\n", __func__);
		goto end;
	}

	lt8912_read(lt, I2C_ADDR_MAIN, 0x94, &reg_val, 1);

	init_reg_val = reg_val;

	if (interrupts & CFG_HPD_INTERRUPTS)
		reg_val &= ~HPD_INTERRUPTS;

	if (interrupts & CFG_EDID_INTERRUPTS)
		reg_val &= ~EDID_INTERRUPTS;

	if (reg_val != init_reg_val) {
		pr_debug("%s: disabling 0x94 interrupts\n", __func__);
		//lt8912_write(lt, I2C_ADDR_MAIN, 0x94, reg_val);
	}

	lt8912_read(lt, I2C_ADDR_MAIN, 0x95, &reg_val, 1);

	init_reg_val = reg_val;

	if (interrupts & CFG_CEC_INTERRUPTS)
		reg_val &= ~CEC_INTERRUPTS;

	if (reg_val != init_reg_val) {
		pr_debug("%s: disabling 0x95 interrupts\n", __func__);
		//lt8912_write(lt, I2C_ADDR_MAIN, 0x95, reg_val);
	}
end:
	return ret;
}*/

static void lt8912_check_hpd_work(struct work_struct *work)
{
	int ret;
	int connected = 0;
	struct lt *lt;
	struct delayed_work *dw = to_delayed_work(work);
	u8 reg_val = 0;

	lt = container_of(dw, struct lt,
			lt8912_check_hpd_work_id);
	if (!lt) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	/* Check if cable is already connected.
	 * Since lt8912_irq line is edge triggered,
	 * if cable is already connected by this time
	 * it won't trigger HPD interrupt.
	 */
	lt8912_read(lt, I2C_ADDR_MAIN, 0xC1, &reg_val, 1);

	connected  = (reg_val & BIT(7));
	if (lt->prev_connected != connected) {
		lt->prev_connected = connected;
		pr_debug("%s: connected = %d\n", __func__, connected);

		if (connected) {
			ret = lt8912_read_edid(lt, sizeof(lt->edid_buf),
				lt->edid_buf);
			if (ret)
				pr_err("%s: edid read failed\n", __func__);

			lt8912_notify_clients(lt, HPD_CONNECT);
			pr_err("%s: Rx CONNECTED\n", __func__);
			lt8912_power_on(lt, 1, 0);
			lt8912_video_on(lt, 1, 0);
		} else {
			pr_err("%s: Rx DISCONNECTED\n", __func__);

			lt8912_notify_clients(lt, HPD_DISCONNECT);
		} 
	}

	queue_delayed_work(lt->workq, &lt->lt8912_check_hpd_work_id, 500);

}

/*static int lt8912_cec_enable(void *client, bool cec_on, u32 flags)
{
	int ret = -EINVAL;
	struct lt *lt = i2c_get_clientdata(client);

	if (!lt) {
		pr_err("%s: invalid platform data\n", __func__);
		goto end;
	}

	if (cec_on) {
		lt8912_write_array(lt, lt8912_cec_en,
					sizeof(lt8912_cec_en));
		lt8912_write_array(lt, lt8912_cec_tg_init,
					sizeof(lt8912_cec_tg_init));
		lt8912_write_array(lt, lt8912_cec_power,
					sizeof(lt8912_cec_power));

		lt->cec_enabled = true;

		ret = lt8912_enable_interrupts(lt, CFG_CEC_INTERRUPTS);

	} else {
		lt->cec_enabled = false;
		ret = lt8912_disable_interrupts(lt, CFG_CEC_INTERRUPTS);
	}
end:
	return ret;
}*/
#if 0
static void lt8912_set_audio_block(struct lt *lt, u32 size, void *buf)
{
	if (!lt || !buf) {
		pr_err("%s: invalid data\n", __func__);
		return;
	}

	mutex_lock(&lt->ops_mutex);

	size = min_t(u32, size, AUDIO_DATA_SIZE);

	memset(lt->audio_spkr_data, 0, AUDIO_DATA_SIZE);
	memcpy(lt->audio_spkr_data, buf, size);

	mutex_unlock(&lt->ops_mutex);
}
#endif

#if 0
static void lt8912_get_audio_block(struct lt *lt, u32 size, void *buf)
{
	if (!lt || !buf) {
		pr_err("%s: invalid data\n", __func__);
		return;
	}

	mutex_lock(&lt->ops_mutex);

	size = min_t(u32, size, AUDIO_DATA_SIZE);

	memcpy(buf, lt->audio_spkr_data, size);

	mutex_unlock(&lt->ops_mutex);
}
#endif


#if 0
static int lt8912_check_hpd(struct lt *lt, u32 flags)
{
	u8 reg_val = 0;
	int connected = 0;

	if (!lt) {
		pr_err("%s: invalid platform data\n", __func__);
		return -EINVAL;
	}
	
	pr_debug("%s: enter\n", __func__);
	/* Check if cable is already connected.
	 * Since lt8912_irq line is edge triggered,
	 * if cable is already connected by this time
	 * it won't trigger HPD interrupt.
	 */
	mutex_lock(&lt->ops_mutex);
	lt8912_read(lt, I2C_ADDR_MAIN, 0xC1, &reg_val, 1);

	connected  = (reg_val & BIT(7));
	if (connected) {
		pr_debug("%s: cable is connected\n", __func__);

		//lt8912_edid_read_init(lt);
	}
	mutex_unlock(&lt->ops_mutex);

	return connected;
}
#endif

#if 0
static int lt8912_get_edid_size(struct lt *lt, u32 *size, u32 flags)
{
	int ret = 0;

	mutex_lock(&lt->ops_mutex);

	if (!size) {
		ret = -EINVAL;
		goto end;
	}

	*size = EDID_SEG_SIZE;
end:
	mutex_unlock(&lt->ops_mutex);
	return ret;
}
#endif

#if 0
static int lt8912_configure_audio(void *client,
	struct msm_dba_audio_cfg *cfg, u32 flags)
{
	int ret = -EINVAL;
	int sampling_rate = 0;
	struct lt *lt =
		i2c_get_clientdata(client);
	struct lt8912_reg_cfg reg_cfg[] = {
		{I2C_ADDR_MAIN, 0x12, 0x00, 0},
		{I2C_ADDR_MAIN, 0x13, 0x00, 0},
		{I2C_ADDR_MAIN, 0x14, 0x00, 0},
		{I2C_ADDR_MAIN, 0x15, 0x00, 0},
		{I2C_ADDR_MAIN, 0x0A, 0x00, 0},
		{I2C_ADDR_MAIN, 0x0C, 0x00, 0},
		{I2C_ADDR_MAIN, 0x0D, 0x00, 0},
		{I2C_ADDR_MAIN, 0x03, 0x00, 0},
		{I2C_ADDR_MAIN, 0x02, 0x00, 0},
		{I2C_ADDR_MAIN, 0x01, 0x00, 0},
		{I2C_ADDR_MAIN, 0x09, 0x00, 0},
		{I2C_ADDR_MAIN, 0x08, 0x00, 0},
		{I2C_ADDR_MAIN, 0x07, 0x00, 0},
		{I2C_ADDR_MAIN, 0x73, 0x00, 0},
		{I2C_ADDR_MAIN, 0x76, 0x00, 0},
	};

	if (!lt || !cfg) {
		pr_err("%s: invalid data\n", __func__);
		return ret;
	}

	mutex_lock(&lt->ops_mutex);

	if (cfg->copyright == MSM_DBA_AUDIO_COPYRIGHT_NOT_PROTECTED)
		reg_cfg[0].val |= BIT(5);

	if (cfg->pre_emphasis == MSM_DBA_AUDIO_PRE_EMPHASIS_50_15us)
		reg_cfg[0].val |= BIT(2);

	if (cfg->clock_accuracy == MSM_DBA_AUDIO_CLOCK_ACCURACY_LVL1)
		reg_cfg[0].val |= BIT(0);
	else if (cfg->clock_accuracy == MSM_DBA_AUDIO_CLOCK_ACCURACY_LVL3)
		reg_cfg[0].val |= BIT(1);

	reg_cfg[1].val = cfg->channel_status_category_code;

	reg_cfg[2].val = (cfg->channel_status_word_length & 0xF) << 0 |
		(cfg->channel_status_source_number & 0xF) << 4;

	if (cfg->sampling_rate == MSM_DBA_AUDIO_32KHZ)
		sampling_rate = 0x3;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_44P1KHZ)
		sampling_rate = 0x0;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_48KHZ)
		sampling_rate = 0x2;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_88P2KHZ)
		sampling_rate = 0x8;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_96KHZ)
		sampling_rate = 0xA;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_176P4KHZ)
		sampling_rate = 0xC;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_192KHZ)
		sampling_rate = 0xE;

	reg_cfg[3].val = (sampling_rate & 0xF) << 4;

	if (cfg->mode == MSM_DBA_AUDIO_MODE_MANUAL)
		reg_cfg[4].val |= BIT(7);

	if (cfg->interface == MSM_DBA_AUDIO_SPDIF_INTERFACE)
		reg_cfg[4].val |= BIT(4);

	if (cfg->interface == MSM_DBA_AUDIO_I2S_INTERFACE) {
		/* i2s enable */
		reg_cfg[5].val |= BIT(2);

		/* audio samp freq select */
		reg_cfg[5].val |= BIT(7);
	}

	/* format */
	reg_cfg[5].val |= cfg->i2s_fmt & 0x3;

	/* channel status override */
	reg_cfg[5].val |= (cfg->channel_status_source & 0x1) << 6;

	/* sample word lengths, default 24 */
	reg_cfg[6].val |= 0x18;

	/* endian order of incoming I2S data */
	if (cfg->word_endianness == MSM_DBA_AUDIO_WORD_LITTLE_ENDIAN)
		reg_cfg[6].val |= 0x1 << 7;

	/* compressed audio v - bit */
	reg_cfg[6].val |= (cfg->channel_status_v_bit & 0x1) << 5;

	/* ACR - N */
	reg_cfg[7].val |= (cfg->n & 0x000FF) >> 0;
	reg_cfg[8].val |= (cfg->n & 0x0FF00) >> 8;
	reg_cfg[9].val |= (cfg->n & 0xF0000) >> 16;

	/* ACR - CTS */
	reg_cfg[10].val |= (cfg->cts & 0x000FF) >> 0;
	reg_cfg[11].val |= (cfg->cts & 0x0FF00) >> 8;
	reg_cfg[12].val |= (cfg->cts & 0xF0000) >> 16;

	/* channel count */
	reg_cfg[13].val |= (cfg->channels & 0x3);

	/* CA */
	reg_cfg[14].val = cfg->channel_allocation;

	lt8912_write_array(lt, reg_cfg, sizeof(reg_cfg));

	mutex_unlock(&lt->ops_mutex);
	return ret;
}
#endif

/*static int lt8912_hdmi_cec_write(void *client, u32 size,
	char *buf, u32 flags)
{
	int ret = -EINVAL;
	struct lt *lt =
		i2c_get_clientdata(client);

	if (!lt) {
		pr_err("%s: invalid platform data\n", __func__);
		return ret;
	}

	mutex_lock(&lt->ops_mutex);

	ret = lt8912_cec_prepare_msg(lt, buf, size);
	if (ret)
		goto end;

	lt8912_write(lt, I2C_ADDR_CEC_DSI, 0x81, 0x07);
end:
	mutex_unlock(&lt->ops_mutex);
	return ret;
}*/

/*static int lt8912_hdmi_cec_read(void *client, u32 *size, char *buf, u32 flags)
{
	int ret = -EINVAL;
	int i;
	struct lt *lt =
		i2c_get_clientdata(client);

	if (!lt) {
		pr_err("%s: invalid platform data\n", __func__);
		return ret;
	}

	mutex_lock(&lt->ops_mutex);

	for (i = 0; i < LT8912_CEC_BUF_MAX; i++) {
		struct lt8912_cec_msg *msg = &lt->cec_msg[i];

		if (msg->pending && msg->timestamp) {
			memcpy(buf, msg->buf, CEC_MSG_SIZE);
			msg->pending = false;
			break;
		}
	}

	if (i < LT8912_CEC_BUF_MAX) {
		*size = CEC_MSG_SIZE;
		ret = 0;
	} else {
		pr_err("%s: no pending cec msg\n", __func__);
		*size = 0;
	}

	mutex_unlock(&lt->ops_mutex);
	return ret;
}
*/
#if 0
static int lt8912_get_raw_edid(struct lt *lt,
	u32 size, char *buf, u32 flags)
{
	if (!lt || !buf) {
		pr_err("%s: invalid data\n", __func__);
		goto end;
	}

	mutex_lock(&lt->ops_mutex);

	size = min_t(u32, size, sizeof(lt->edid_buf));

	memcpy(buf, lt->edid_buf, size);

	mutex_unlock(&lt->ops_mutex);

end:
	return 0;
}
#endif

#if 0
static int lt8912_write_reg(struct lt *lt,
		u32 reg, u32 val)
{
	int ret = -EINVAL;
	u8 i2ca = 0;

	i2ca = I2C_ADDR_MAIN;

	lt8912_write(lt, i2ca, (u8)(reg & 0xFF), (u8)(val & 0xFF));
	return ret;
}
#endif

#if 0
static int lt8912_read_reg(struct lt *lt,
		u32 reg, u32 *val)
{
	int ret = 0;
	u8 byte_val = 0;
	u8 i2ca = 0;

	i2ca = I2C_ADDR_MAIN;
	lt8912_read(lt, i2ca, (u8)(reg & 0xFF), &byte_val, 1);
	*val = (u32)byte_val;

	return ret;
}
#endif

#if 0
static int lt8912_register_dba(struct lt *lt)
{
	struct msm_dba_ops *client_ops;
	struct msm_dba_device_ops *dev_ops;

	if (!lt)
		return -EINVAL;

	client_ops = &lt->dev_info.client_ops;
	dev_ops = &lt->dev_info.dev_ops;

	client_ops->power_on        = lt8912_power_on;
	client_ops->video_on        = lt8912_video_on;
	//client_ops->configure_audio = lt8912_configure_audio;
	client_ops->hdcp_enable     = NULL;
	/*client_ops->hdmi_cec_on     = lt8912_cec_enable;
	client_ops->hdmi_cec_write  = lt8912_hdmi_cec_write;
	client_ops->hdmi_cec_read   = lt8912_hdmi_cec_read;*/
	client_ops->get_edid_size   = lt8912_get_edid_size;
	client_ops->get_raw_edid    = lt8912_get_raw_edid;
	client_ops->check_hpd	    = lt8912_check_hpd;
	client_ops->get_audio_block = lt8912_get_audio_block;
	client_ops->set_audio_block = lt8912_set_audio_block;

	dev_ops->write_reg = lt8912_write_reg;
	dev_ops->read_reg = lt8912_read_reg;
	dev_ops->dump_debug_info = lt8912_dump_debug_info;

	strlcpy(lt->dev_info.chip_name, "lt",
		sizeof(lt->dev_info.chip_name));

	mutex_init(&lt->dev_info.dev_mutex);

	INIT_LIST_HEAD(&lt->dev_info.client_list);

	return msm_dba_add_probed_device(&lt->dev_info);
}

static void lt8912_unregister_dba(struct lt *lt)
{
	if (!lt)
		return;

	msm_dba_remove_probed_device(&lt->dev_info);
}
#endif

static const unsigned char main_reg_bytes[] = {
	0x00, 0x0e, 1,
	0x31, 0x43, 1,
	0x44, 0x45, 2,
	0x50, 0x52, 1,
	0x55, 0x55, 1,
	0x57, 0x57, 1,
	0x5a, 0x5a, 1,
	0x60, 0x60, 1,
	0x69, 0x6c, 1,
	0x7f, 0x7f, 1,
	0x94, 0x95, 1,
	0x99, 0x9f, 1,
	0xa8, 0xa8, 1,
	0xab, 0xab, 1,
	0xb2, 0xb2, 1,
	0xb4, 0xb4, 1,
	0xc1, 0xc1, 1,
	0, 0
};

static const unsigned char cec_reg_bytes[] = {
	0x00, 0x1b, 1,
	0x1c, 0x1d, 2,
	0x1e, 0x2f, 1,
	0x34, 0x3f, 2,
	0x40, 0x40, 1,
	0x42, 0x4d, 1,
	0x51, 0x5c, 1,
	0x70, 0x7f, 1,
	0, 0
};

static int reg_valid(const unsigned char *range, unsigned char val)
{
	while (range[1]) {
		if (val < range[0])
			return 0;
		if (val <= range[1])
			return range[2];
		range += 3;
	}
	return -EINVAL;
}

static ssize_t store_reg(struct lt *lt, const unsigned char *tbl, const enum lt8912_i2c_addr i2caddr, u8 *last_reg, const char *buf)
{
	unsigned reg, value;
	int numscanned = sscanf(buf,"%x %x\n", &reg, &value);
	int ret;

	ret = reg_valid(tbl, reg);
	if ((ret <= 0) || (ret > 2)) {
		dev_err(&lt->client->dev,"%s: invalid register: use form 0xREG [0xVAL]\n",
			__func__);
		return -EINVAL;
	}

	if (numscanned == 2) {
		int rval;

		if (ret > 1)
			rval = lt8912_write_w(lt, i2caddr, reg, value);
		else
			rval = lt8912_write(lt, i2caddr, reg, value);
		if (rval)
			dev_err(&lt->client->dev, "%s: error %d setting reg 0x%02x to 0x%02x\n",
				__func__, rval, reg, value);
	} else if (numscanned == 1) {
		*last_reg = reg;
	} else {
		dev_err(&lt->client->dev,"%s: invalid register: use form 0xREG [0xVAL]\n",
			__func__);
	}
	return 0;
}

static ssize_t show_reg(struct lt *lt, const unsigned char *tbl, const enum lt8912_i2c_addr i2caddr, u8 *last_reg, char *buf)
{
	unsigned v;
	u8 val[4];
	int ret;
	int size;
	int n;
	int reg;

	pr_debug("%s:last_reg=%x\n", __func__, *last_reg);
	if (!*last_reg) {
		int cnt = 0;

		reg = 0;
		do {

			size = reg_valid(tbl, reg);
			if (size < 0)
				break;
			if (size) {
				val[0] = val[1] = val[2] = val[3] = 0;
				ret = lt8912_read(lt, i2caddr, reg, val, size);
				if (ret >= 0) {
					v = val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24);
					if (size == 2)
						n = sprintf(buf, "0x%02x:0x%04x\n", reg, v);
					else
						n = sprintf(buf, "0x%02x:0x%02x\n", reg, v);
					if (n > 0) {
						cnt += n;
						buf += n;
					}
				}
			}
			reg += size ? size : 1;
		} while (1);

		return cnt;
	}
	reg = *last_reg;
	size = reg_valid(tbl, reg);
	if (size <= 0)
		return 0;
	val[0] = val[1] = val[2] = val[3] = 0;
	ret = lt8912_read(lt, i2caddr, reg, val, size);
	if (ret < 0)
		return ret;
	v = val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24);
	if (size == 2)
		n = sprintf(buf, "0x%04x\n", v);
	else
		n = sprintf(buf, "0x%02x\n", v);
	return n;
}

static ssize_t main_show_reg(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct lt *lt = i2c_get_clientdata(client);

	return show_reg(lt, main_reg_bytes, I2C_ADDR_MAIN, &lt->last_reg, buf);
}

static ssize_t main_store_reg(struct device *dev, struct device_attribute *attr,
	 const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct lt *lt = i2c_get_clientdata(client);
	int ret;

	ret = store_reg(lt, main_reg_bytes, I2C_ADDR_MAIN, &lt->last_reg, buf);
	if (ret < 0)
		return ret;
	return count;
}

static DEVICE_ATTR(lt8912_reg, S_IRUGO|S_IWUSR|S_IWGRP, main_show_reg, main_store_reg);

static ssize_t cec_show_reg(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct lt *lt = i2c_get_clientdata(client);

	return show_reg(lt, cec_reg_bytes, I2C_ADDR_CEC_DSI, &lt->last_reg_cec, buf);
}

static ssize_t cec_store_reg(struct device *dev, struct device_attribute *attr,
	 const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct lt *lt = i2c_get_clientdata(client);
	int ret;

	ret = store_reg(lt, cec_reg_bytes, I2C_ADDR_CEC_DSI, &lt->last_reg_cec, buf);
	if (ret < 0)
		return ret;
	return count;
}

static DEVICE_ATTR(lt8912_cec, S_IRUGO|S_IWUSR|S_IWGRP, cec_show_reg, cec_store_reg);

static int lt8912_probe(struct i2c_client *client,
	 const struct i2c_device_id *id)
{
        struct device_node *np = client->dev.of_node;
	struct device_node *ddc;
	static struct lt *lt;
	int ret = 0;
	u32 dsi_lanes;

	pr_info("%s:\n", __func__);
	if (!client || !client->dev.of_node) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	lt = devm_kzalloc(&client->dev,
		sizeof(struct lt), GFP_KERNEL);
	if (!lt)
		return -ENOMEM;
	lt->client = client;
	mutex_init(&lt->ops_mutex);

	ret = lt8912_parse_dt(&client->dev, lt);
	if (ret) {
		pr_err("%s: Failed to parse DT, %d\n", __func__, ret);
		goto err_dt_parse;
	}

	lt->disp_dsi = of_parse_phandle(np, "display-dsi", 0);
	if (!lt->disp_dsi) {
		ret = -ENODEV;
		pr_err("%s: display not linked, %d\n", __func__, ret);
		goto err_dt_parse;
	}
	if (of_property_read_u32(lt->disp_dsi, "dsi-lanes", &dsi_lanes) < 0)
		return -EINVAL;
	if (dsi_lanes < 1 || dsi_lanes > 4)
		return -EINVAL;
	lt->dsi_lanes = dsi_lanes;

	lt->i2c_client = client;
    
	if (!IS_ERR(lt->avdd)) {
		ret = regulator_enable(lt->avdd);
		if (ret) {
			pr_err("%s: Regulator avdd enable failed ret=%d\n",__func__, ret);
		}
	}
	ddc = of_parse_phandle(client->dev.of_node, "ddc-i2c-bus", 0);
	if (ddc) {
		lt->ddc = of_find_i2c_adapter_by_node(ddc);
		of_node_put(ddc);

		if (!lt->ddc) {
			ret = -EPROBE_DEFER;
			goto err_dt_parse;
		}
	}

	msleep(200);
	ret = lt8912_gpio_configure(lt, true);
	if (ret) {
		pr_err("%s: Failed to configure GPIOs\n", __func__);
		goto err_dt_parse;
	}
	msleep(20);

	ret = lt8912_read_device_rev(lt);
	if (ret) {
		pr_err("%s: Failed to read chip rev\n", __func__);
		goto err_i2c_rev;
	}
#if 0
	ret = lt8912_register_dba(lt);
	if (ret) {
		pr_err("%s: Error registering with DBA %d\n",
			__func__, ret);
		goto err_dba_reg;
	}
#endif

	#if 0
	lt->irq = gpio_to_irq(lt->irq_gpio);
	ret = request_threaded_irq(lt->irq, NULL, lt8912_irq,
        IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "lt", lt);
    if (ret) {
        pr_err("%s: Failed to enable ADV7533 interrupt\n",
            __func__);
        goto err_irq;
    }	
	#endif

	dev_set_drvdata(&client->dev, lt);
#if 0
	ret = msm_dba_helper_sysfs_init(&client->dev);
	if (ret) {
		pr_err("%s: sysfs init failed\n", __func__);
		goto err_dba_helper;
	}
#endif
	lt->workq = create_workqueue("lt8912_workq");
	if (!lt->workq) {
		pr_err("%s: workqueue creation failed.\n", __func__);
		ret = -EPERM;
		goto err_workqueue;
	}

	if (lt->audio) {
		pr_debug("%s: enabling default audio configs\n", __func__);
		lt8912_write_array(lt, I2S_cfg, sizeof(I2S_cfg));
	}

	//INIT_DELAYED_WORK(&lt->lt8912_intr_work_id, lt8912_intr_work);
	INIT_DELAYED_WORK(&lt->lt8912_check_hpd_work_id, lt8912_check_hpd_work);  //there is issue in irq, so we use work queue to read connect status
	queue_delayed_work(lt->workq, &lt->lt8912_check_hpd_work_id, 10 * 100);

	pm_runtime_enable(&client->dev);
	pm_runtime_set_active(&client->dev);
	lt8912_power_on(lt, 1, 0);
	lt8912_video_on(lt, 1, 0);

	if (device_create_file(&client->dev, &dev_attr_lt8912_reg))
		dev_err(&client->dev, "Error on creating sysfs file for lt8912_reg\n");
	if (device_create_file(&client->dev, &dev_attr_lt8912_cec))
		dev_err(&client->dev, "Error on creating sysfs file for lt8912_cec\n");
	return 0;

err_workqueue:
#if 0
	msm_dba_helper_sysfs_remove(&client->dev);
err_dba_helper:
#endif
	disable_irq(lt->irq);
	free_irq(lt->irq, lt);
#if 0
	lt8912_unregister_dba(lt);
err_dba_reg:
#endif
err_i2c_rev:
//	lt8912_gpio_configure(lt, false);
err_dt_parse:
	if (lt->ddc)
		put_device(&lt->ddc->dev);
	devm_kfree(&client->dev, lt);
	return ret;
}

static int lt8912_remove(struct i2c_client *client)
{
	struct lt *lt = i2c_get_clientdata(client);
	int ret = -EINVAL;

	if (!client)
		goto end;

	if (!lt)
		goto end;

	device_remove_file(&client->dev,
		&dev_attr_lt8912_reg);
	device_remove_file(&client->dev,
		&dev_attr_lt8912_cec);

	cancel_delayed_work_sync(&lt->lt8912_check_hpd_work_id);
	if (lt->ddc)
		put_device(&lt->ddc->dev);
	pm_runtime_disable(&client->dev);
	disable_irq(lt->irq);
	free_irq(lt->irq, lt);

	ret = lt8912_gpio_configure(lt, false);

	mutex_destroy(&lt->ops_mutex);

	devm_kfree(&client->dev, lt);
end:
	return ret;
}

static const struct of_device_id lt8912_dt_match[] = {
        {.compatible = "lontium,lt8912"},
        {}
};
MODULE_DEVICE_TABLE(of, lt8912_dt_match);

static const struct i2c_device_id lt8912_id[] = {
	{"lt8912", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, lt8912_id);

static struct i2c_driver lt8912_driver = {
	.driver = {
		.name = "lt8912",
		.of_match_table = lt8912_dt_match,
		.owner = THIS_MODULE,
	},
	.probe = lt8912_probe,
	.remove = lt8912_remove,
	.id_table = lt8912_id,
};

static int __init lt8912_init(void)
{
	return i2c_add_driver(&lt8912_driver);
}

static void __exit lt8912_exit(void)
{
	i2c_del_driver(&lt8912_driver);
}

module_param_string(panel, mdss_mdp_panel, MDSS_MAX_PANEL_LEN, 0);

module_init(lt8912_init);
module_exit(lt8912_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("lt8912 driver");
