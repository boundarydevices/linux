/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/hdmi_tx_ddc.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_ddc.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>

#include "hdmi_tx_reg.h"

static DEFINE_MUTEX(ddc_mutex);
static uint32_t ddc_write_1byte(uint8_t slave, uint8_t offset_addr,
	uint8_t data)
{
	uint32_t st = 0;

	mutex_lock(&ddc_mutex);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, slave);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, offset_addr);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_DATAO, data);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 4);
	mdelay(2);
	if (hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 0)) {
		st = 0;
		pr_info("hdmitx: ddc w1b error 0x%02x 0x%02x 0x%02x\n",
			slave, offset_addr, data);
	} else
		st = 1;
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 0x7);

	mutex_unlock(&ddc_mutex);
	return st;
}

#if 0
static uint32_t ddc_readext_8byte(uint8_t slave, uint8_t offset_addr,
	uint8_t *data)
{
	uint32_t st = 0;
	int32_t i;

	mutex_lock(&ddc_mutex);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, slave);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, offset_addr);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGADDR,  EDIDSEG_ADR);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGPTR, 0x00);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 3);
	mdelay(2);
	if (hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 0)) {
		st = 0;
		pr_info("hdmitx: ddc rdext8b error 0x%02x 0x%02x\n",
			slave, offset_addr);
	} else
		st = 1;
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 0x7);
	for (i = 0; i < 8; i++)
		data[i] = hdmitx_rd_reg(HDMITX_DWC_I2CM_READ_BUFF0 + i);

	mutex_unlock(&ddc_mutex);
	return st;
}
#endif

static uint32_t ddc_read_8byte(uint8_t slave, uint8_t offset_addr,
	uint8_t *data)
{
	uint32_t st = 0;
	int32_t i;

	mutex_lock(&ddc_mutex);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, slave);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, offset_addr);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 2);
	mdelay(2);
	if (hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 0)) {
		st = 0;
		pr_info("hdmitx: ddc rd8b error 0x%02x 0x%02x 0x%02x\n",
			slave, offset_addr, *data);
	} else
		st = 1;
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 0x7);
	for (i = 0; i < 8; i++)
		data[i] = hdmitx_rd_reg(HDMITX_DWC_I2CM_READ_BUFF0 + i);

	mutex_unlock(&ddc_mutex);
	return st;
}

#if 0
static uint32_t ddc_readext_1byte(uint8_t slave, uint8_t address, uint8_t *data)
{
	uint32_t st = 0;
	int32_t i;

	mutex_lock(&ddc_mutex);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, slave);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, offset_addr);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGADDR,  EDIDSEG_ADR);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGPTR, 0x00);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 1);
	mdelay(2);
	if (hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 0)) {
		st = 0;
		pr_info("hdmitx: ddc rd8b error 0x%02x 0x%02x\n",
			slave, offset_addr);
	} else
		st = 1;
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 0x7);
	*data = hdmitx_rd_reg(HDMITX_DWC_I2CM_DATAI);

	mutex_unlock(&ddc_mutex);
	return st;
}
#endif

static uint32_t ddc_read_1byte(uint8_t slave, uint8_t offset_addr,
	uint8_t *data)
{
	uint32_t st = 0;

	mutex_lock(&ddc_mutex);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, slave);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, offset_addr);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 0);
	mdelay(2);
	if (hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 0)) {
		st = 0;
		pr_info("hdmitx: ddc rd8b error 0x%02x 0x%02x\n",
			slave, offset_addr);
	} else
		st = 1;
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 0x7);
	*data = hdmitx_rd_reg(HDMITX_DWC_I2CM_DATAI);

	mutex_unlock(&ddc_mutex);
	return st;
}

static uint32_t hdcp_rd_bksv(uint8_t *data)
{
	return ddc_read_8byte(HDCP_SLAVE, HDCP14_BKSV, data);
}

void scdc_rd_sink(uint8_t adr, uint8_t *val)
{
	hdmitx_ddc_hw_op(DDC_MUX_DDC);
	ddc_read_1byte(SCDC_SLAVE, adr, val);
}

void scdc_wr_sink(uint8_t adr, uint8_t val)
{
	hdmitx_ddc_hw_op(DDC_MUX_DDC);
	ddc_write_1byte(SCDC_SLAVE, adr, val);
}

uint32_t hdcp_rd_hdcp14_ver(void)
{
	int ret = 0;
	uint8_t bksv[8] = {0};

	hdmitx_ddc_hw_op(DDC_MUX_DDC);
	ret = hdcp_rd_bksv(&bksv[0]);
	if (ret)
		return 1;
	ret = hdcp_rd_bksv(&bksv[0]);
	if (ret)
		return 1;

	return 0;
}

uint32_t hdcp_rd_hdcp22_ver(void)
{
	uint32_t ret;
	uint8_t ver;

	hdmitx_ddc_hw_op(DDC_MUX_DDC);
	ret = ddc_read_1byte(HDCP_SLAVE, HDCP2_VERSION, &ver);
	if (ret)
		return ver == 0x04;
	ret = ddc_read_1byte(HDCP_SLAVE, HDCP2_VERSION, &ver);
	if (ret)
		return ver == 0x04;

	return 0;
}

/* only for I2C reactive using */
void edid_read_head_8bytes(void)
{
	uint8_t head[8] = {0};

	hdmitx_ddc_hw_op(DDC_MUX_DDC);
	ddc_read_8byte(EDID_SLAVE, 0x00, head);
}

