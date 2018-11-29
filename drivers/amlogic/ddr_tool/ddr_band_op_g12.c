/*
 * drivers/amlogic/ddr_tool/ddr_band_op_g12.c
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

static void g12_dmc_port_config(struct ddr_bandwidth *db, int channel, int port)
{
	unsigned int val;
	unsigned int rp[MAX_CHANNEL] = {DMC_MON_G12_CTRL1, DMC_MON_G12_CTRL3,
					DMC_MON_G12_CTRL5, DMC_MON_G12_CTRL7};
	unsigned int rs[MAX_CHANNEL] = {DMC_MON_G12_CTRL2, DMC_MON_G12_CTRL4,
					DMC_MON_G12_CTRL6, DMC_MON_G12_CTRL8};
	int subport = -1;

	/* clear all port mask */
	if (port < 0) {
		writel(0, db->ddr_reg + rp[channel]);
		writel(0, db->ddr_reg + rs[channel]);
		return;
	}

	if (port >= PORT_MAJOR)
		subport = port - PORT_MAJOR;

	if (subport < 0) {
		val = readl(db->ddr_reg + rp[channel]);
		val |=  (1 << port);
		writel(val, db->ddr_reg + rp[channel]);
		val = 0xffff;
		writel(val, db->ddr_reg + rs[channel]);
	} else {
		val = (0x1 << 23);	/* select device */
		writel(val, db->ddr_reg + rp[channel]);
		val = readl(db->ddr_reg + rs[channel]);
		val |= (1 << subport);
		writel(val, db->ddr_reg + rs[channel]);
	}
}

static unsigned long g12_get_dmc_freq_quick(struct ddr_bandwidth *db)
{
	unsigned int val;
	unsigned int n, m, od1;
	unsigned int od_div = 0xfff;
	unsigned long freq = 0;

	val = readl(db->pll_reg);
	val = val & 0xfffff;
	switch ((val >> 16) & 7) {
	case 0:
		od_div = 2;
		break;

	case 1:
		od_div = 3;
		break;

	case 2:
		od_div = 4;
		break;

	case 3:
		od_div = 6;
		break;

	case 4:
		od_div = 8;
		break;

	default:
		break;
	}

	m = val & 0x1ff;
	n = ((val >> 10) & 0x1f);
	od1 = (((val >> 19) & 0x1)) == 1 ? 2 : 1;
	freq = DEFAULT_XTAL_FREQ / 1000;	/* avoid overflow */
	if (n)
		freq = ((((freq * m) / n) >> od1) / od_div) * 1000;

	return freq;
}

static void g12_dmc_bandwidth_enable(struct ddr_bandwidth *db)
{
	unsigned int val;

	/* enable all channel */
	val =  (0x01 << 31) |	/* enable bit */
	       (0x01 << 20) |	/* use timer  */
	       (0x0f <<  0);
	writel(val, db->ddr_reg + DMC_MON_G12_CTRL0);
}

static void g12_dmc_bandwidth_init(struct ddr_bandwidth *db)
{
	unsigned int i;

	/* set timer trigger clock_cnt */
	writel(db->clock_count, db->ddr_reg + DMC_MON_G12_TIMER);
	g12_dmc_bandwidth_enable(db);

	for (i = 0; i < db->channels; i++)
		g12_dmc_port_config(db, i, -1);
}

static int g12_handle_irq(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	unsigned int reg, i, val;
	int ret = -1;

	val = readl(db->ddr_reg + DMC_MON_G12_CTRL0);
	if (val & DMC_QOS_IRQ) {
		/*
		 * get total bytes by each channel, each cycle 16 bytes;
		 */
		dg->all_grant = readl(db->ddr_reg + DMC_MON_G12_ALL_GRANT_CNT);
		dg->all_req   = readl(db->ddr_reg + DMC_MON_G12_ALL_REQ_CNT);
		dg->all_grant *= 16;
		dg->all_req   *= 16;
		for (i = 0; i < db->channels; i++) {
			reg = DMC_MON_G12_ONE_GRANT_CNT + (i << 2);
			dg->channel_grant[i] = readl(db->ddr_reg + reg) * 16;
		}
		/* clear irq flags */
		writel(val, db->ddr_reg + DMC_MON_G12_CTRL0);
		g12_dmc_bandwidth_enable(db);

		ret = 0;
	}
	return ret;
}

#if DDR_BANDWIDTH_DEBUG
static int g12_dump_reg(struct ddr_bandwidth *db, char *buf)
{
	int s = 0, i;
	unsigned int r;

	for (i = 0; i < 9; i++) {
		r  = readl(db->ddr_reg + (DMC_MON_G12_CTRL0 + (i << 2)));
		s += sprintf(buf + s, "DMC_MON_CTRL%d:        %08x\n", i, r);
	}
	r  = readl(db->ddr_reg + DMC_MON_G12_ALL_REQ_CNT);
	s += sprintf(buf + s, "DMC_MON_ALL_REQ_CNT:  %08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_G12_ALL_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_ALL_GRANT_CNT:%08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_G12_ONE_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_ONE_GRANT_CNT:%08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_G12_SEC_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_SEC_GRANT_CNT:%08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_G12_THD_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_THD_GRANT_CNT:%08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_G12_FOR_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_FOR_GRANT_CNT:%08x\n", r);
	r  = readl(db->ddr_reg + DMC_MON_G12_TIMER);
	s += sprintf(buf + s, "DMC_MON_TIMER:        %08x\n", r);

	return s;
}
#endif

struct ddr_bandwidth_ops g12_ddr_bw_ops = {
	.init             = g12_dmc_bandwidth_init,
	.config_port      = g12_dmc_port_config,
	.get_freq         = g12_get_dmc_freq_quick,
	.handle_irq       = g12_handle_irq,
	.bandwidth_enable = g12_dmc_bandwidth_enable,
#if DDR_BANDWIDTH_DEBUG
	.dump_reg         = g12_dump_reg,
#endif
};
