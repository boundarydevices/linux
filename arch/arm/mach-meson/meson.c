/*
 * arch/arm/mach-meson/meson.c
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

#include <linux/of_platform.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <linux/amlogic/pm.h>
#include <linux/amlogic/meson-secure.h>
#include <asm/hardware/cache-l2x0.h>

static const char * const meson_common_board_compat[] = {
	"amlogic,meson6",
	"amlogic,meson8",
	"amlogic,meson8b",
	NULL,
};

#ifdef CONFIG_AMLOGIC_M8B_SUSPEND
static void __init meson_map_io(void)
{
	struct map_desc map;

	map.pfn = __phys_to_pfn(0x4f00000);
	map.virtual = 0xc4d00000;
	map.length = 0x100000;
	map.type = MT_MEMORY_RWX_NONCACHED;
	iotable_init(&map, 1);

	map.pfn = __phys_to_pfn(0xc4200000);
	map.virtual = IO_PL310_BASE;
	map.length = 0x1000;
	map.type = MT_DEVICE;
	iotable_init(&map, 1);
}
#endif

static void meson_l2c310_write_sec(unsigned long val, unsigned int reg)
{
#define MESON_L2C310_BASE_REG 0xc4200000
	static void __iomem *l2x0_base;

	if (!meson_secure_enabled()) {
		if (!l2x0_base)
			l2x0_base = ioremap(MESON_L2C310_BASE_REG, 0x1000);

		if (l2x0_base)
			writel_relaxed(val, (l2x0_base + reg));

		return;
	}

	switch (reg) {
	case L2X0_CTRL:
		meson_smc1(TRUSTZONE_MON_L2X0_CTRL_INDEX, val);
		break;
	case L2X0_AUX_CTRL:
		meson_smc1(TRUSTZONE_MON_L2X0_AUXCTRL_INDEX, val);
		break;
	case L2X0_DEBUG_CTRL:
		meson_smc1(TRUSTZONE_MON_L2X0_DEBUG_INDEX, val);
		break;
	case L310_TAG_LATENCY_CTRL:
		meson_smc1(TRUSTZONE_MON_L2X0_TAGLATENCY_INDEX, val);
		break;
	case L310_DATA_LATENCY_CTRL:
		meson_smc1(TRUSTZONE_MON_L2X0_DATALATENCY_INDEX, val);
		break;
	case L310_ADDR_FILTER_START:
		meson_smc1(TRUSTZONE_MON_L2X0_FILTERSTART_INDEX, val);
		break;
	case L310_ADDR_FILTER_END:
		meson_smc1(TRUSTZONE_MON_L2X0_FILTEREND_INDEX, val);
		break;
	case L310_PREFETCH_CTRL:
		meson_smc1(TRUSTZONE_MON_L2X0_PREFETCH_INDEX, val);
		break;
	case L310_POWER_CTRL:
		meson_smc1(TRUSTZONE_MON_L2X0_POWER_INDEX, val);
		break;

	default:
		WARN_ONCE(1, "Meson L2C310: ignoring write to reg 0x%x\n", reg);
		break;
	}
}

DT_MACHINE_START(MESON, "Amlogic Meson platform")
	.dt_compat	= meson_common_board_compat,
	.l2c_aux_val	= 0,
	.l2c_aux_mask	= ~0,
	.l2c_write_sec	= meson_l2c310_write_sec,
#ifdef CONFIG_AMLOGIC_M8B_SUSPEND
	.map_io         = meson_map_io,
#endif
MACHINE_END
