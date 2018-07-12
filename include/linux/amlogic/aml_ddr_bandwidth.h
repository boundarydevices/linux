/*
 * include/linux/amlogic/aml_ddr_bandwidth.h
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

#ifndef __AML_DDR_BANDWIDTH_H__
#define __AML_DDR_BANDWIDTH_H__


#define DEFAULT_CLK_CNT			12000000
#define DEFAULT_XTAL_FREQ		24000000UL

#define DMC_QOS_IRQ			(1 << 30)
#define MAX_CHANNEL			4
#define MAX_PORTS			256
#define MAX_NAME			15
#define PORT_MAJOR			32

/*
 * register offset for chips before g12
 */
#define DMC_MON_CTRL1			(0x25 << 2)
#define DMC_MON_CTRL2			(0x26 << 2)
#define DMC_MON_CTRL3			(0x27 << 2)
#define DMC_MON_ALL_REQ_CNT		(0x28 << 2)
#define DMC_MON_ALL_GRANT_CNT		(0x29 << 2)
#define DMC_MON_ONE_GRANT_CNT		(0x2a << 2)
#define DMC_MON_SEC_GRANT_CNT		(0x2b << 2)
#define DMC_MON_THD_GRANT_CNT		(0x2c << 2)
#define DMC_MON_FOR_GRANT_CNT		(0x2d << 2)

#define DMC_MON_CTRL4			(0x18 << 2)
#define DMC_MON_CTRL5			(0x19 << 2)
#define DMC_MON_CTRL6			(0x1a << 2)

/*
 * register offset for g12a
 */
#define DMC_MON_G12_CTRL0		(0x20  << 2)
#define DMC_MON_G12_CTRL1		(0x21  << 2)
#define DMC_MON_G12_CTRL2		(0x22  << 2)
#define DMC_MON_G12_CTRL3		(0x23  << 2)
#define DMC_MON_G12_CTRL4		(0x24  << 2)
#define DMC_MON_G12_CTRL5		(0x25  << 2)
#define DMC_MON_G12_CTRL6		(0x26  << 2)
#define DMC_MON_G12_CTRL7		(0x27  << 2)
#define DMC_MON_G12_CTRL8		(0x28  << 2)

#define DMC_MON_G12_ALL_REQ_CNT		(0x29  << 2)
#define DMC_MON_G12_ALL_GRANT_CNT	(0x2a  << 2)
#define DMC_MON_G12_ONE_GRANT_CNT	(0x2b  << 2)
#define DMC_MON_G12_SEC_GRANT_CNT	(0x2c  << 2)
#define DMC_MON_G12_THD_GRANT_CNT	(0x2d  << 2)
#define DMC_MON_G12_FOR_GRANT_CNT	(0x2e  << 2)
#define DMC_MON_G12_TIMER		(0x2f  << 2)

/* data structure */
#define DDR_BANDWIDTH_DEBUG		1

struct ddr_bandwidth;

struct ddr_grant {
	unsigned int all_grant, all_req;
	unsigned int channel_grant[MAX_CHANNEL];
};

struct ddr_bandwidth_ops {
	void (*init)(struct ddr_bandwidth *db);
	void (*config_port)(struct ddr_bandwidth *db, int channel, int port);
	int  (*handle_irq)(struct ddr_bandwidth *db, struct ddr_grant *dg);
	void (*bandwidth_enable)(struct ddr_bandwidth *db);
	unsigned long (*get_freq)(struct ddr_bandwidth *db);
#if	DDR_BANDWIDTH_DEBUG
	int (*dump_reg)(struct ddr_bandwidth *db, char *buf);
#endif
};

struct ddr_port_desc {
	char port_name[MAX_NAME];
	unsigned char port_id;
};

struct ddr_bandwidth {
	void __iomem *ddr_reg;
	void __iomem *pll_reg;
	struct class *class;
	unsigned short cpu_type;
	unsigned short real_ports;
	unsigned int irq_num;
	unsigned int clock_count;
	unsigned int channels;
	unsigned int port[MAX_CHANNEL];
	unsigned int bandwidth[MAX_CHANNEL];
	unsigned int total_usage;
	unsigned int total_bandwidth;
	struct ddr_port_desc *port_desc;
	struct ddr_bandwidth_ops *ops;
};

#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH
extern unsigned int aml_get_ddr_usage(void);
extern struct ddr_bandwidth_ops g12_ddr_bw_ops;
extern struct ddr_bandwidth_ops gx_ddr_bw_ops;
extern struct ddr_bandwidth_ops gxl_ddr_bw_ops;
extern int ddr_find_port_desc(int cpu_type, struct ddr_port_desc **desc);
#else
static inline unsigned int aml_get_ddr_usage(void)
{
	return 0;
}
#endif

#endif /* __AML_DDR_BANDWIDTH_H__ */
