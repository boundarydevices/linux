/*
 * drivers/amlogic/media/common/arch/registers/register_ops_m8.c
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

#include <linux/amlogic/media/registers/register_ops.h>
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/amlogic/media/utils/log.h>

#define REGISTER_FOR_CPU {\
			MESON_CPU_MAJOR_ID_M8,\
			MESON_CPU_MAJOR_ID_M8M2,\
	MESON_CPU_MAJOR_ID_GXBB, \
	MESON_CPU_MAJOR_ID_GXTVBB, \
	MESON_CPU_MAJOR_ID_GXL, \
	MESON_CPU_MAJOR_ID_GXM, \
	MESON_CPU_MAJOR_ID_TXL, \
			0}
#define REGISTER_FOR_GXCPU {\
	MESON_CPU_MAJOR_ID_GXBB, \
	MESON_CPU_MAJOR_ID_GXTVBB, \
	MESON_CPU_MAJOR_ID_GXL, \
	MESON_CPU_MAJOR_ID_GXM, \
	MESON_CPU_MAJOR_ID_TXL, \
			0}
int codec_apb_read(unsigned int reg)
{
	unsigned int val = 0;

	aml_reg_read(IO_APB_BUS_BASE, reg << 2, &val);
	return val;
}
EXPORT_SYMBOL(codec_apb_read);

void codec_apb_write(unsigned int reg, unsigned int val)
{
	aml_reg_write(IO_APB_BUS_BASE, reg << 2, val);
}
EXPORT_SYMBOL(codec_apb_write);

static struct chip_register_ops m8_ops[] __initdata = {
	{IO_DOS_BUS, 0, codecio_read_dosbus, codecio_write_dosbus},
	{IO_VC_BUS, 0, codecio_read_vcbus, codecio_write_vcbus},
	{IO_C_BUS, 0, codecio_read_cbus, codecio_write_cbus},
	{IO_HHI_BUS, 0, codecio_read_cbus, codecio_write_cbus},
	{IO_AO_BUS, 0, codecio_read_aobus, codecio_write_aobus},
	{IO_VPP_BUS, 0, codecio_read_vcbus, codecio_write_vcbus},
};

static struct chip_register_ops ex_gx_ops[] __initdata = {
	/*
	 *#define HHI_VDEC_CLK_CNTL 0x1078
	 *to
	 *#define HHI_VDEC_CLK_CNTL 0x78
	 *on Gxbb.
	 *will changed later..
	 */
	{IO_HHI_BUS, -0x1000, codecio_read_hiubus, codecio_write_hiubus},
	{IO_DMC_BUS, 0, codecio_read_dmcbus, codecio_write_dmcbus},
};

static int __init vdec_reg_ops_init(void)
{
	int cpus[] = REGISTER_FOR_CPU;
	int gxcpus[] = REGISTER_FOR_GXCPU;

	register_reg_ops_mgr(cpus, m8_ops,
		sizeof(m8_ops) / sizeof(struct chip_register_ops));

	register_reg_ex_ops_mgr(gxcpus, ex_gx_ops,
		sizeof(ex_gx_ops) / sizeof(struct chip_register_ops));

	return 0;
}

postcore_initcall(vdec_reg_ops_init);
