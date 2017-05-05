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

DT_MACHINE_START(MESON, "Amlogic Meson platform")
	.dt_compat	= meson_common_board_compat,
	.l2c_aux_val	= 0,
	.l2c_aux_mask	= ~0,
#ifdef CONFIG_AMLOGIC_M8B_SUSPEND
	.map_io         = meson_map_io,
#endif
MACHINE_END
