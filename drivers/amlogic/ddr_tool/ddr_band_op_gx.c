/*
 * drivers/amlogic/ddr_tool/ddr_band_op_gx.c
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


static void gx_dmc_port_config(struct ddr_bandwidth *db, int channel, int port)
{
	unsigned int val;
	int subport = -1;

	/* set to a unused port to clear bandwidth */
	if (port < 0) {
		writel(0x8016ffff, db->ddr_reg + DMC_MON_CTRL2);
		return;
	}

	if (port >= PORT_MAJOR)
		subport = port - PORT_MAJOR;

	val = readl(db->ddr_reg + DMC_MON_CTRL2);
	if (subport < 0) {
		val &= ~(0xfffff << 0);
		val |= ((port << 16) | 0xffff);
	} else {
		val &= ~(0xffff);
		if (db->cpu_type == MESON_CPU_MAJOR_ID_M8B)
			val |= (0xf << 16) | (1 << subport);
		else
			val |= (0x7 << 16) | (1 << subport);
	}
	writel(val, db->ddr_reg + DMC_MON_CTRL2);
}


static unsigned long gx_get_dmc_freq_quick(struct ddr_bandwidth *db)
{
	unsigned int val;
	unsigned int od, n, m, od1;
	unsigned long freq = 0;

	val = readl(db->pll_reg);
	od  = (val >> 16) & 0x03;
	n   = (val >>  9) & 0x1f;
	m   = (val >>  0) & 0x1ff;
	od1 = 0;
	if ((db->cpu_type >= MESON_CPU_MAJOR_ID_GXBB) &&
		    (db->cpu_type < MESON_CPU_MAJOR_ID_GXL)) {
		od1 = (val >> 14) & 0x03;
	}
	freq = DEFAULT_XTAL_FREQ / 1000;	/* avoid overflow */
	freq = ((freq * m / (n * (1 + od))) >> od1) * 1000;
	return freq;
}

static void gx_dmc_bandwidth_enable(struct ddr_bandwidth *db)
{
	/*
	 * for old chips, only 1 port can be selected
	 */
	writel(0x8010ffff, db->ddr_reg + DMC_MON_CTRL2);
}

static void gx_dmc_bandwidth_init(struct ddr_bandwidth *db)
{
	/* set timer trigger clock_cnt */
	writel(db->clock_count, db->ddr_reg + DMC_MON_CTRL3);
	gx_dmc_bandwidth_enable(db);

	gx_dmc_port_config(db, 0, -1);
}

static int gx_handle_irq(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	unsigned int reg, val;
	int ret = -1;

	val = readl(db->ddr_reg + DMC_MON_CTRL2);
	if (val & DMC_QOS_IRQ) {
		/*
		 * get total bytes by each channel, each cycle 16 bytes;
		 */
		dg->all_grant = readl(db->ddr_reg + DMC_MON_ALL_GRANT_CNT) * 16;
		dg->all_req   = readl(db->ddr_reg + DMC_MON_ALL_REQ_CNT) * 16;
		reg = DMC_MON_ONE_GRANT_CNT;
		dg->channel_grant[0] = readl(db->ddr_reg + reg) * 16;
		/* clear irq flags */
		writel(val, db->ddr_reg + DMC_MON_CTRL2);
		gx_dmc_bandwidth_enable(db);
		ret = 0;
	}
	return ret;
}

#if DDR_BANDWIDTH_DEBUG
static int gx_dump_reg(struct ddr_bandwidth *db, char *buf)
{
	int s = 0;
	unsigned int r;

	r  = readl(db->ddr_reg + DMC_MON_CTRL2);
	s += sprintf(buf + s, "DMC_MON_CTRL2:        %08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_CTRL3);
	s += sprintf(buf + s, "DMC_MON_CTRL3:        %08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_ALL_REQ_CNT);
	s += sprintf(buf + s, "DMC_MON_ALL_REQ_CNT:  %08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_ALL_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_ALL_GRANT_CNT:%08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_ONE_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_ONE_GRANT_CNT:%08x\n", r);

	return s;
}
#endif

struct ddr_bandwidth_ops gx_ddr_bw_ops = {
	.init             = gx_dmc_bandwidth_init,
	.config_port      = gx_dmc_port_config,
	.get_freq         = gx_get_dmc_freq_quick,
	.handle_irq       = gx_handle_irq,
	.bandwidth_enable = gx_dmc_bandwidth_enable,
#if DDR_BANDWIDTH_DEBUG
	.dump_reg         = gx_dump_reg,
#endif
};
