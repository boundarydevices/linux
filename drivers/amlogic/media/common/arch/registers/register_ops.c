/*
 * drivers/amlogic/media/common/arch/registers/register_ops.c
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
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/utils/log.h>

#include <linux/amlogic/media/registers/register_ops.h>

static struct chip_register_ops *amports_ops[BUS_MAX];

/*#define DEBUG_REG_OPS*/
#define DEBUG_PRINT_CNT 10

#ifdef DEBUG_REG_OPS
#define CODEC_OPS_START(bus, reg, c) do {\
	if (c < DEBUG_PRINT_CNT)\
		pr_err("try %s bus.%d,%x, cnt=%d\n", __func__, bus, reg, c);\
} while (0)

#define CODEC_OPS_ERROR(bus, reg, c) do {\
	if (c < DEBUG_PRINT_CNT)\
		pr_err("failed on %s bus.%d,%x, cnt=%d\n",\
		__func__, bus, reg, c);\
} while (0)
#else

#define CODEC_OPS_START(bus, reg, c)
#define CODEC_OPS_ERROR(bus, reg, c)
#endif

int codec_reg_read(u32 bus_type, unsigned int reg)
{
	struct chip_register_ops *ops = amports_ops[bus_type];

	ops->r_cnt++;
	CODEC_OPS_START(bus_type, reg, ops->r_cnt);
	if (ops && ops->read)
		return ops->read(ops->ext_offset + reg);
	CODEC_OPS_ERROR(bus_type, reg, ops->r_cnt);
	return 0;
}
EXPORT_SYMBOL(codec_reg_read);

void codec_reg_write(u32 bus_type, unsigned int reg, unsigned int val)
{
	struct chip_register_ops *ops = amports_ops[bus_type];

	ops->w_cnt++;
	CODEC_OPS_START(bus_type, reg, ops->w_cnt);
	if (ops && ops->write)
		return ops->write(ops->ext_offset + reg, val);
	CODEC_OPS_ERROR(bus_type, reg, ops->w_cnt);
}
EXPORT_SYMBOL(codec_reg_write);

void codec_reg_write_bits(u32 bus_type, unsigned int reg,
	u32 val, int start, int len)
{
	/*
	 *#define WRITE_SEC_REG_BITS(r, val, start, len) \
	 *WRITE_SEC_REG(r, (READ_SEC_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
	 *((unsigned)((val)&((1L<<(len))-1)) << (start)))
	 */
	u32 toval = codec_reg_read(bus_type, reg);
	u32 mask = (((1L << (len)) - 1) << (start));

	toval &= ~mask;
	toval |= (val << start) & mask;
	codec_reg_write(bus_type, reg, toval);
}
EXPORT_SYMBOL(codec_reg_write_bits);

static int register_reg_onebus_ops(struct chip_register_ops *ops)
{
	if (ops->bus_type >= BUS_MAX)
		return -1;
	pr_debug("register amports ops for bus[%d]\n", ops->bus_type);
	if (amports_ops[ops->bus_type] != NULL)
		;
	/*
	 *TODO.
	 *kfree(amports_ops[ops->bus_type]);
	 */
	amports_ops[ops->bus_type] = ops;
	return 0;
}

int register_reg_ops_per_cpu(int cputype,
	struct chip_register_ops *sops, int ops_size)
{
	struct chip_register_ops *ops;
	int i, size = 0;

	if (cputype != get_cpu_type()) {
		pr_debug("ignore bus ops for cpu=%d\n", cputype);
		return 0;	/* ignore don't needed firmare. */
	}
	/*ops = kmalloc(sizeof(struct chip_register_ops) * ops_size,*/
		/*GFP_KERNEL);*/
	size = sizeof(struct chip_register_ops);
	ops = kmalloc_array(ops_size, size, GFP_KERNEL);
	if (!ops)
		return -ENOMEM;
	memcpy(ops, sops, size * ops_size);
	for (i = 0; i < ops_size; i++)
		register_reg_onebus_ops(&ops[i]);
	return 0;
}
EXPORT_SYMBOL(register_reg_ops_per_cpu);

int register_reg_ops_mgr(int cputype[],
	struct chip_register_ops *sops_list, int ops_size)
{
	int i = 0;

	while (cputype[i] > 0) {
		register_reg_ops_per_cpu(cputype[i], sops_list, ops_size);
		i++;
	}
	return 0;
}
EXPORT_SYMBOL(register_reg_ops_mgr);

int register_reg_ex_ops_mgr(int cputype[],
	struct chip_register_ops *ex_ops_list, int ops_size)
{
	int i = 0;

	while (cputype[i] > 0) {
		register_reg_ops_per_cpu(cputype[i], ex_ops_list, ops_size);
		i++;
	}
	return 0;
}
EXPORT_SYMBOL(register_reg_ex_ops_mgr);

