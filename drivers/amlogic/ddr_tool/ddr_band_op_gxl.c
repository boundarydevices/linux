/*
 * drivers/amlogic/ddr_tool/ddr_band_op_gxl.c
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
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_ddr_bandwidth.h>
#include <linux/io.h>
#include <linux/slab.h>

#undef pr_fmt
#define pr_fmt(fmt) "ddr_tool: " fmt

static void gxl_dmc_port_config(struct ddr_bandwidth *db, int channel, int port)
{
	unsigned int val;
	unsigned int port_reg[MAX_CHANNEL] = {DMC_MON_CTRL1, DMC_MON_CTRL4,
					      DMC_MON_CTRL5, DMC_MON_CTRL6};
	int subport = -1;

	if (port >= PORT_MAJOR)
		subport = port - PORT_MAJOR;

	/* clear all port mask */
	if (port < 0) {
		writel(0, db->ddr_reg + port_reg[channel]);
		return;
	}

	val = readl(db->ddr_reg + port_reg[channel]);
	if (port < 16) {
		val |= ((1 << (16 + port)) | 0xffff);
	} else if (subport > 0) {
		val &= ~(0xffffffff);
		val |= (1 << 23) | (1 << subport);
	} else {
		pr_err("port config fail, %s: %d\n", __func__, __LINE__);
		return;
	}
	writel(val, db->ddr_reg + port_reg[channel]);
}

static unsigned long gxl_get_dmc_freq_quick(struct ddr_bandwidth *db)
{
	unsigned int val;
	unsigned int od, n, m, od1;
	unsigned long freq = 0;

	val  = readl(db->pll_reg);
	od   = (val >>  2) & 0x03;
	m    = (val >>  4) & 0x1ff;
	n    = (val >> 16) & 0x1f;
	od1  = (val >>  0) & 0x03;
	freq = DEFAULT_XTAL_FREQ / 1000;	/* avoid overflow */
	freq = ((freq * m / (n * (1 + od))) >> od1) * 1000;
	return freq;
}

static void gxl_dmc_bandwidth_enable(struct ddr_bandwidth *db)
{
	unsigned int val;

	/* enable all channel */
	val =  (0x01 << 31) |	/* enable bit */
	       (0x01 << 20) |	/* use timer  */
	       (0x0f <<  0);
	writel(val, db->ddr_reg + DMC_MON_CTRL2);
}

static void gxl_dmc_bandwidth_init(struct ddr_bandwidth *db)
{
	unsigned int i;

	/* set timer trigger clock_cnt */
	writel(db->clock_count, db->ddr_reg + DMC_MON_CTRL3);
	gxl_dmc_bandwidth_enable(db);

	for (i = 0; i < db->channels; i++)
		gxl_dmc_port_config(db, i, -1);
}


static int gxl_handle_irq(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	unsigned int reg, i, val;
	int ret = -1;

	val = readl(db->ddr_reg + DMC_MON_CTRL2);
	if (val & DMC_QOS_IRQ) {
		/*
		 * get total bytes by each channel, each cycle 16 bytes;
		 */
		dg->all_grant = readl(db->ddr_reg + DMC_MON_ALL_GRANT_CNT) * 16;
		dg->all_req   = readl(db->ddr_reg + DMC_MON_ALL_REQ_CNT) * 16;
		for (i = 0; i < db->channels; i++) {
			reg = DMC_MON_ONE_GRANT_CNT + (i << 2);
			dg->channel_grant[i] = readl(db->ddr_reg + reg) * 16;
		}
		/* clear irq flags */
		writel(val, db->ddr_reg + DMC_MON_CTRL2);
		gxl_dmc_bandwidth_enable(db);
		ret = 0;
	}
	return ret;
}

#if DDR_BANDWIDTH_DEBUG
static int gxl_dump_reg(struct ddr_bandwidth *db, char *buf)
{
	int s = 0;
	unsigned int r;


	r  = readl(db->ddr_reg + DMC_MON_CTRL1);
	s += sprintf(buf + s, "DMC_MON_CTRL1:        %08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_CTRL2);
	s += sprintf(buf + s, "DMC_MON_CTRL2:        %08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_CTRL3);
	s += sprintf(buf + s, "DMC_MON_CTRL3:        %08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_CTRL4);
	s += sprintf(buf + s, "DMC_MON_CTRL4:        %08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_CTRL5);
	s += sprintf(buf + s, "DMC_MON_CTRL5:        %08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_CTRL6);
	s += sprintf(buf + s, "DMC_MON_CTRL6:        %08x\n", r);

	r  = readl(db->ddr_reg + DMC_MON_ALL_REQ_CNT);
	s += sprintf(buf + s, "DMC_MON_ALL_REQ_CNT:  %08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_ALL_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_ALL_GRANT_CNT:%08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_ONE_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_ONE_GRANT_CNT:%08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_SEC_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_SEC_GRANT_CNT:%08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_THD_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_THD_GRANT_CNT:%08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_FOR_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_FOR_GRANT_CNT:%08x\n", r);

	return s;
}
#endif

struct ddr_bandwidth_ops gxl_ddr_bw_ops = {
	.init             = gxl_dmc_bandwidth_init,
	.config_port      = gxl_dmc_port_config,
	.get_freq         = gxl_get_dmc_freq_quick,
	.handle_irq       = gxl_handle_irq,
	.bandwidth_enable = gxl_dmc_bandwidth_enable,
#if DDR_BANDWIDTH_DEBUG
	.dump_reg         = gxl_dump_reg,
#endif
};
