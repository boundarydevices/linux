/*
 * drivers/amlogic/media/vout/lcd/lcd_extern/i2c_anx6345.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/amlogic/i2c-amlogic.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include "lcd_extern.h"
#include "i2c_anx6345.h"

static struct lcd_extern_config_s *ext_config;
static struct aml_lcd_extern_i2c_dev_s *i2c0_dev;
static struct aml_lcd_extern_i2c_dev_s *i2c1_dev;

static unsigned int edp_tx_lane = 1;

#define LCD_EXTERN_NAME           "lcd_anx6345"

#define LCD_EXTERN_I2C_ADDR       (0x38) /* 7bit address */
#define LCD_EXTERN_I2C_ADDR2      (0x39) /* 7bit address */
#define LCD_EXTERN_I2C_BUS        LCD_EXTERN_I2C_BUS_2

static int SP_TX_Write_Reg(unsigned char addr, unsigned char reg,
		unsigned char data)
{
	struct i2c_client *client = aml_anx6345_70_client;
	unsigned char buff[2];
	int ret;

	buff[0] = reg;
	buff[1] = data;
	if (addr == 0x70)
		client = i2c0_dev->client;
	else if (addr == 0x72)
		client = i2c1_dev->client;

	if (client == NULL) {
		EXTERR("%s: invalid i2c client\n", __func__);
		return -1;
	}
	ret = lcd_extern_i2c_write(client, buff, 1);
	return ret;
}

static int SP_TX_Read_Reg(unsigned char addr, unsigned char reg,
		unsigned char *data)
{
	struct i2c_client *client = aml_anx6345_70_client;
	int ret;

	*data = reg;
	if (addr == 0x70)
		client = i2c0_dev->client;
	else if (addr == 0x72)
		client = i2c1_dev->client;

	if (client == NULL) {
		EXTERR("%s: invalid i2c client\n", __func__);
		return -1;
	}
	ret = lcd_extern_i2c_read(client, data, 1);
	return ret;
}

static int SP_TX_Wait_AUX_Finished(void)
{
	unsigned char c;
	unsigned char cCnt;

	cCnt = 0;
	SP_TX_Read_Reg(0x70, SP_TX_AUX_STATUS, &c);
	while (c & 0x10) {/* aux busy */
		cCnt++;
		SP_TX_Read_Reg(0x70, SP_TX_AUX_STATUS, &c);
		if (cCnt > 100)
			return -1; /* aux fail */
	}

	return 0; /* aux ok */
}

static int SP_TX_AUX_DPCDRead_Bytes(unsigned char addrh, unsigned char addrm,
		unsigned char addrl, unsigned char cCount, unsigned char *pBuf)
{
	unsigned char c, i;

	/* clr buffer */
	SP_TX_Write_Reg(0x70, SP_TX_BUF_DATA_COUNT_REG, 0x80);

	/* set read cmd and count */
	SP_TX_Write_Reg(0x70, SP_TX_AUX_CTRL_REG, (((cCount-1) << 4) | 0x09));

	/* set aux address15:0 */
	SP_TX_Write_Reg(0x70, SP_TX_AUX_ADDR_7_0_REG, addrl);
	SP_TX_Write_Reg(0x70, SP_TX_AUX_ADDR_15_8_REG, addrm);

	/* set address19:16 and enable aux */
	SP_TX_Read_Reg(0x70, SP_TX_AUX_ADDR_19_16_REG, &c);
	SP_TX_Write_Reg(0x70, SP_TX_AUX_ADDR_19_16_REG, ((c & 0xf0) | addrh));

	/* Enable Aux */
	SP_TX_Read_Reg(0x70, SP_TX_AUX_CTRL_REG2, &c);
	SP_TX_Write_Reg(0x70, SP_TX_AUX_CTRL_REG2, (c | 0x01));

	mdelay(5);
	if (SP_TX_Wait_AUX_Finished())
		return -1;

	for (i = 0; i < cCount; i++) {
		SP_TX_Read_Reg(0x70, SP_TX_BUF_DATA_0_REG+i, &c);
		*(pBuf+i) = c;
		if (i >= MAX_BUF_CNT)
			break;
	}

	return 0; /* aux ok */
}

static int lcd_extern_power_on(void)
{
	unsigned int lane_num;
	unsigned int link_rate;
	unsigned char bits;
	unsigned char device_id;
	unsigned char temp;
	unsigned char temp1;
	unsigned int count = 0;
	unsigned int count1 = 0;

	lcd_extern_pinmux_set(1);

	lane_num = edp_tx_lane; /* 1 lane */
	link_rate = VAL_EDP_TX_LINK_BW_SET_270; /* 2.7G */
	bits = 0; /* 0x00: 6bit;  0x10:8bit */
	SP_TX_Write_Reg(0x72, 0x05, 0x00);

	SP_TX_Read_Reg(0x72, 0x01, &device_id);
	if (device_id == 0xaa) {
		EXTPR("ANX6345 Chip found\n\n");
	} else {
		EXTERR("ANX6345 Chip not found\n\n");
		return -1;
	}
	temp = device_id;
	/* if aux read fail, do h/w reset */
	while (SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00, 0x00, 1, &temp1) &&
		(count < 200)) {
		/* read fail, h/w reset */
		SP_TX_Write_Reg(0x72, 0x06, 0x01);
		SP_TX_Write_Reg(0x72, 0x06, 0x00);
		SP_TX_Write_Reg(0x72, 0x05, 0x00);
		mdelay(10);
		count++;
	}

	/* software reset */
	SP_TX_Read_Reg(0x72, SP_TX_RST_CTRL_REG, &temp);
	SP_TX_Write_Reg(0x72, SP_TX_RST_CTRL_REG, (temp | SP_TX_RST_SW_RST));
	SP_TX_Write_Reg(0x72, SP_TX_RST_CTRL_REG, (temp & ~SP_TX_RST_SW_RST));

	/* EDID address for AUX access */
	SP_TX_Write_Reg(0x70, SP_TX_EXTRA_ADDR_REG, 0x50);
	/* disable HDCP polling mode. */
	SP_TX_Write_Reg(0x70, SP_TX_HDCP_CTRL, 0x00);
	/* Enable HDCP polling mode. */
	/* SP_TX_Write_Reg(0x70, SP_TX_HDCP_CTRL, 0x02); */
	/* enable M value read out */
	SP_TX_Write_Reg(0x70, SP_TX_LINK_DEBUG_REG, 0x30);

	/* SP_TX_Read_Reg(0x70, SP_TX_DEBUG_REG1, &temp); */
	SP_TX_Write_Reg(0x70, SP_TX_DEBUG_REG1, 0x00); /*disable polling HPD */

	SP_TX_Read_Reg(0x70, SP_TX_HDCP_CONTROL_0_REG, &temp);
	/* set KSV valid */
	SP_TX_Write_Reg(0x70, SP_TX_HDCP_CONTROL_0_REG, (temp | 0x03));

	SP_TX_Read_Reg(0x70, SP_TX_AUX_CTRL_REG2, &temp);
	/* set double AUX output */
	SP_TX_Write_Reg(0x70, SP_TX_AUX_CTRL_REG2, (temp | 0x08));

	/* unmask pll change int */
	SP_TX_Write_Reg(0x72, SP_COMMON_INT_MASK1, 0xbf);
	SP_TX_Write_Reg(0x72, SP_COMMON_INT_MASK2, 0xff);/* mask all int */
	SP_TX_Write_Reg(0x72, SP_COMMON_INT_MASK3, 0xff);/* mask all int */
	SP_TX_Write_Reg(0x72, SP_COMMON_INT_MASK4, 0xff);/* mask all int */

	/* reset AUX */
	SP_TX_Read_Reg(0x72, SP_TX_RST_CTRL2_REG, &temp);
	SP_TX_Write_Reg(0x72, SP_TX_RST_CTRL2_REG, temp | SP_TX_AUX_RST);
	SP_TX_Write_Reg(0x72, SP_TX_RST_CTRL2_REG, temp & (~SP_TX_AUX_RST));

	/* Chip initialization */
	SP_TX_Write_Reg(0x70, SP_TX_SYS_CTRL1_REG, 0x00);
	mdelay(10);

	SP_TX_Write_Reg(0x72, SP_TX_VID_CTRL2_REG, bits);

	/* ANX6345 chip analog setting */
	SP_TX_Write_Reg(0x70, SP_TX_PLL_CTRL_REG, 0x00);/* UPDATE: 0x07->0x00 */

	/* ANX chip analog setting */
	/* UPDATE: 0xF0->0X70 */
	/* SP_TX_Write_Reg(0x72, ANALOG_DEBUG_REG1, 0x70); */
	SP_TX_Write_Reg(0x70, SP_TX_LINK_DEBUG_REG, 0x30);

	/* force HPD */
	SP_TX_Write_Reg(0x70, SP_TX_SYS_CTRL3_REG, 0x30);

	/* enable ssc function */
	SP_TX_Write_Reg(0x70, 0xA7, 0x00); /* disable SSC first */
	SP_TX_Write_Reg(0x70, 0xD0, 0x5f); /* ssc d  0.4%, f0/4 mode */
	SP_TX_Write_Reg(0x70, 0xD1, 0x00);
	SP_TX_Write_Reg(0x70, 0xD2, 0x75); /* ctrl_th */
	SP_TX_Read_Reg(0x70, 0xA7, &temp);
	SP_TX_Write_Reg(0x70, 0xA7, (temp | 0x10)); /* enable SSC */
	SP_TX_Read_Reg(0x72, 0x07, &temp); /* reset SSC */
	SP_TX_Write_Reg(0x72, 0x07, (temp | 0x80));
	SP_TX_Write_Reg(0x72, 0x07, (temp & (~0x80)));

	/* Select 2.7G */
	/* 2.7g:0x0a; 1.62g:0x06 */
	SP_TX_Write_Reg(0x70, SP_TX_LINK_BW_SET_REG, link_rate);
	/* Select 2 lanes */
	SP_TX_Write_Reg(0x70, 0xa1, lane_num);

	SP_TX_Write_Reg(0x70, SP_TX_LINK_TRAINING_CTRL_REG,
		SP_TX_LINK_TRAINING_CTRL_EN);
	mdelay(5);
	SP_TX_Read_Reg(0x70, SP_TX_LINK_TRAINING_CTRL_REG, &temp);
	/* UPDATE: FROM 0X01 TO 0X80 */
	while ((temp & 0x80) != 0) {
		/* debug_puts("Waiting...\n"); */
		mdelay(5);
		count1++;
		if (count1 > 100) {
			EXTERR("ANX6345 Link training fail\n");
			break;
		}
		SP_TX_Read_Reg(0x70, SP_TX_LINK_TRAINING_CTRL_REG, &temp);
	}

	SP_TX_Write_Reg(0x72, 0x12, 0x2c);
	SP_TX_Write_Reg(0x72, 0x13, 0x06);
	SP_TX_Write_Reg(0x72, 0x14, 0x00);
	SP_TX_Write_Reg(0x72, 0x15, 0x06);
	SP_TX_Write_Reg(0x72, 0x16, 0x02);
	SP_TX_Write_Reg(0x72, 0x17, 0x04);
	SP_TX_Write_Reg(0x72, 0x18, 0x26);
	SP_TX_Write_Reg(0x72, 0x19, 0x50);
	SP_TX_Write_Reg(0x72, 0x1a, 0x04);
	SP_TX_Write_Reg(0x72, 0x1b, 0x00);
	SP_TX_Write_Reg(0x72, 0x1c, 0x04);
	SP_TX_Write_Reg(0x72, 0x1d, 0x18);
	SP_TX_Write_Reg(0x72, 0x1e, 0x00);
	SP_TX_Write_Reg(0x72, 0x1f, 0x10);
	SP_TX_Write_Reg(0x72, 0x20, 0x00);
	SP_TX_Write_Reg(0x72, 0x21, 0x28);

	SP_TX_Write_Reg(0x72, 0x11, 0x03);
	/* enable BIST. In normal mode, don't need to config this reg
	 * if want to open BIST, must setting
	 * right dat 0x11-0x21 base lcd timing.
	 */
	/* SP_TX_Write_Reg(0x72, 0x0b, 0x09); //colorbar:08,graystep:09 */
	SP_TX_Write_Reg(0x72, 0x08, 0x81); /* SDR:0x81;DDR:0x8f */

	/* force HPD and stream valid */
	SP_TX_Write_Reg(0x70, 0x82, 0x33);
	EXTPR("%s\n", __func__);
	return 0;
}

static int lcd_extern_power_off(void)
{
	int ret = 0;

	lcd_extern_pinmux_set(0);

	return ret;
}

static int lcd_extern_driver_update(struct aml_lcd_extern_driver_s *ext_drv)
{
	if (ext_drv == NULL) {
		EXTERR("%s: driver is null\n", LCD_EXTERN_NAME);
		return -1;
	}

	ext_drv->power_on  = lcd_extern_power_on;
	ext_drv->power_off = lcd_extern_power_off;

	return 0;
}

int aml_lcd_extern_i2c_anx6345_probe(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	ext_config = ext_drv->config;

	i2c0_dev = lcd_extern_get_i2c_device(ext_config->i2c_addr);
	if (i2c0_dev == NULL) {
		EXTERR("invalid i2c0 device\n");
		return -1;
	}
	EXTPR("get i2c0 device: %s, addr 0x%02x OK\n",
		i2c0_dev->name, i2c0_dev->client->addr);

	i2c1_dev = lcd_extern_get_i2c_device(ext_config->i2c_addr2);
	if (i2c1_dev == NULL) {
		EXTERR("invalid i2c1 device\n");
		i2c0_dev = NULL;
		return -1;
	}
	EXTPR("get i2c1 device: %s, addr 0x%02x OK\n",
		i2c1_dev->name, i2c1_dev->client->addr);

	ret = lcd_extern_driver_update(ext_drv);

	if (lcd_debug_print_flag)
		EXTPR("%s: %d\n", __func__, ret);
	return ret;
}

int aml_lcd_extern_i2c_anx6345_remove(void)
{
	i2c0_dev = NULL;
	i2c1_dev = NULL;
	ext_config = NULL;

	return 0;
}

