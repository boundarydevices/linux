/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/reg_ops.c
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
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include "common.h"
#include "hdmi_tx_reg.h"
#include "reg_ops.h"

/* For gxb/gxl/gxm */
static struct reg_map reg_maps_def[] = {
	[CBUS_REG_IDX] = { /* CBUS */
		.phy_addr = 0xc1100000,
		.size = 0xa00000,
	},
	[PERIPHS_REG_IDX] = { /* PERIPHS */
		.phy_addr = 0xc8834400,
		.size = 0x200,
	},
	[VCBUS_REG_IDX] = { /* VPU */
		.phy_addr = 0xd0100000,
		.size = 0x40000,
	},
	[AOBUS_REG_IDX] = { /* RTI */
		.phy_addr = 0xc8100000,
		.size = 0x100000,
	},
	[HHI_REG_IDX] = { /* HIU */
		.phy_addr = 0xc883c000,
		.size = 0x2000,
	},
	[RESET_CBUS_REG_IDX] = { /* RESET */
		.phy_addr = 0xc1104400,
		.size = 0x100,
	},
	[HDMITX_SEC_REG_IDX] = { /* HDMITX SECURE */
		.phy_addr = 0xda83a000,
		.size = 0x2000,
	},
	[HDMITX_REG_IDX] = { /* HDMITX NON-SECURE*/
		.phy_addr = 0xc883a000,
		.size = 0x2000,
	},
	[ELP_ESM_REG_IDX] = {
		.phy_addr = 0xd0044000,
		.size = 0x100,
	},
};

/* For txlx */
static struct reg_map reg_maps_txlx[] = {
	[CBUS_REG_IDX] = { /* CBUS */
		.phy_addr = 0xffd0f000,
		.size = 0xa00000,
	},
	[PERIPHS_REG_IDX] = { /* PERIPHS */
		.phy_addr = 0xff634400,
		.size = 0x2000,
	},
	[VCBUS_REG_IDX] = { /* VPU */
		.phy_addr = 0xff900000,
		.size = 0x40000,
	},
	[AOBUS_REG_IDX] = { /* RTI */
		.phy_addr = 0xff800000,
		.size = 0x100000,
	},
	[HHI_REG_IDX] = { /* HIU */
		.phy_addr = 0xff63c000,
		.size = 0x2000,
	},
	[RESET_CBUS_REG_IDX] = { /* RESET */
		.phy_addr = 0xffd01000,
		.size = 0x100,
	},
	[HDMITX_SEC_REG_IDX] = { /* HDMITX SECURE */
		.phy_addr = 0xff63a000,
		.size = 0x2000,
	},
	[HDMITX_REG_IDX] = { /* HDMITX NON-SECURE*/
		.phy_addr = 0xff63a000,
		.size = 0x2000,
	},
	[ELP_ESM_REG_IDX] = {
		.phy_addr = 0xffe01000,
		.size = 0x100,
	},
};

static struct reg_map *map;

void init_reg_map(unsigned int type)
{
	int i;

	switch (type) {
	case MESON_CPU_ID_TXLX:
		map = reg_maps_txlx;
		break;
	default:
		map = reg_maps_def;
		for (i = 0; i < REG_IDX_END; i++) {
			map[i].p = ioremap(map[i].phy_addr, map[i].size);
			if (!map[i].p) {
				pr_info("hdmitx20: failed Mapped PHY: 0x%x\n",
					map[i].phy_addr);
			} else {
				pr_info("hdmitx20: Mapped PHY: 0x%x\n",
					map[i].phy_addr);
			}
		}
		break;
	}
}

unsigned int get_hdcp22_base(void)
{
	if (map)
		return map[ELP_ESM_REG_IDX].phy_addr;
	else
		return reg_maps_def[ELP_ESM_REG_IDX].phy_addr;
}

#define TO_PHY_ADDR(addr) \
	(map[(addr) >> BASE_REG_OFFSET].phy_addr + \
	((addr) & (((1 << BASE_REG_OFFSET) - 1))))
#define TO_PMAP_ADDR(addr) \
	(map[(addr) >> BASE_REG_OFFSET].p + \
	((addr) & (((1 << BASE_REG_OFFSET) - 1))))

unsigned int hd_read_reg(unsigned int addr)
{
	unsigned int val = 0;
	unsigned int paddr = TO_PHY_ADDR(addr);
	unsigned int index = (addr) >> BASE_REG_OFFSET;

	struct hdmitx_dev *hdev = get_hdmitx_device();

	switch (hdev->chip_type) {
	case MESON_CPU_ID_TXLX:
		switch (index) {
		case CBUS_REG_IDX:
		case RESET_CBUS_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			val = aml_read_cbus(addr);
			break;
		case VCBUS_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			val = aml_read_vcbus(addr);
			break;
		case AOBUS_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			val = aml_read_aobus(addr);
			break;
		case HHI_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			val = aml_read_hiubus(addr);
			break;
		default:
			break;
		}
		break;
	case MESON_CPU_ID_GXL:
	case MESON_CPU_ID_GXM:
	default:
		val = readl(TO_PMAP_ADDR(addr));
		break;
	}

	pr_debug(REG "Rd[0x%x] 0x%x\n", paddr, val);

	return val;
}

void hd_write_reg(unsigned int addr, unsigned int val)
{
	unsigned int paddr = TO_PHY_ADDR(addr);
	unsigned int index = (addr) >> BASE_REG_OFFSET;

	struct hdmitx_dev *hdev = get_hdmitx_device();

	switch (hdev->chip_type) {
	case MESON_CPU_ID_TXLX:
		switch (index) {
		case CBUS_REG_IDX:
		case RESET_CBUS_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			aml_write_cbus(addr, val);
			break;
		case VCBUS_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			aml_write_vcbus(addr, val);
			break;
		case AOBUS_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			aml_write_aobus(addr, val);
			break;
		case HHI_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			aml_write_hiubus(addr, val);
			break;
		default:
			break;
		}
		break;
	case MESON_CPU_ID_GXL:
	case MESON_CPU_ID_GXM:
	default:
		writel(val, TO_PMAP_ADDR(addr));
		break;
	}

	pr_debug(REG "Wr[0x%x] 0x%x\n", paddr, val);
}

void hd_set_reg_bits(unsigned int addr, unsigned int value,
	unsigned int offset, unsigned int len)
{
	unsigned int data32 = 0;

	data32 = hd_read_reg(addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	hd_write_reg(addr, data32);
}

#define __asmeq(x, y)  ".ifnc " x "," y " ; .err ; .endif\n\t"

unsigned int hdmitx_rd_reg(unsigned int addr)
{
	unsigned long offset = (addr & DWC_OFFSET_MASK) >> 24;
	unsigned int data;

	register long x0 asm("x0") = 0x82000018;
	register long x1 asm("x1") = (unsigned long)addr;

	asm volatile(
		__asmeq("%0", "x0")
		__asmeq("%1", "x1")
		"smc #0\n"
		: "+r"(x0) : "r"(x1)
	);
	data = (unsigned int)(x0&0xffffffff);

	pr_debug(REG "%s rd[0x%x] 0x%x\n", offset ? "DWC" : "TOP",
			addr, data);
	return data;
}

void hdmitx_wr_reg(unsigned int addr, unsigned int data)
{
	unsigned long offset = (addr & DWC_OFFSET_MASK) >> 24;

	register long x0 asm("x0") = 0x82000019;
	register long x1 asm("x1") = (unsigned long)addr;
	register long x2 asm("x2") = data;

	asm volatile(
		__asmeq("%0", "x0")
		__asmeq("%1", "x1")
		__asmeq("%2", "x2")
		"smc #0\n"
		: : "r"(x0), "r"(x1), "r"(x2)
	);

	pr_debug("%s wr[0x%x] 0x%x\n", offset ? "DWC" : "TOP",
			addr, data);
}

void hdmitx_set_reg_bits(unsigned int addr, unsigned int value,
	unsigned int offset, unsigned int len)
{
	unsigned int data32 = 0;

	data32 = hdmitx_rd_reg(addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	hdmitx_wr_reg(addr, data32);
}

void hdmitx_poll_reg(unsigned int addr, unsigned int val, unsigned long timeout)
{
	unsigned long time = 0;

	time = jiffies;
	while ((!(hdmitx_rd_reg(addr) & val)) &&
		time_before(jiffies, time + timeout)) {
		mdelay(2);
	}
	if (time_after(jiffies, time + timeout))
		pr_info(REG "hdmitx poll:0x%x  val:0x%x T1=%lu t=%lu T2=%lu timeout\n",
			addr, val, time, timeout, jiffies);
}

void hdmitx_rd_check_reg(unsigned int addr, unsigned int exp_data,
	unsigned int mask)
{
	unsigned long rd_data;

	rd_data = hdmitx_rd_reg(addr);
	if ((rd_data | mask) != (exp_data | mask)) {
		pr_info(REG "HDMITX-DWC addr=0x%04x rd_data=0x%02x\n",
			(unsigned int)addr, (unsigned int)rd_data);
		pr_info(REG "Error: HDMITX-DWC exp_data=0x%02x mask=0x%02x\n",
			(unsigned int)exp_data, (unsigned int)mask);
	}
}
