/*
 * arch/arm/mach-meson/platsmp.h
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

#ifndef __plftsmp_h_
#define __plftsmp_h_

#include <asm/io.h>


#define AOBUS_REG_ADDR(reg)      (io_aobus_base + (reg))
#define CBUS_REG_ADDR(reg)		(io_cbus_base + ((reg) << 2))

#define AO_RTI_PWR_A9_CNTL0 ((0x00 << 10) | (0x38 << 2))
#define AO_RTI_PWR_A9_MEM_PD0 ((0x00 << 10) | (0x3d << 2))
#define AO_RTI_PWR_A9_CNTL1 ((0x00 << 10) | (0x39 << 2))

#define HHI_SYS_CPU_CLK_CNTL 0x1067
#define HHI_SYS_CPU_CLK_CNTL1 0x1057

#define P_AO_RTI_PWR_A9_CNTL0                AOBUS_REG_ADDR(AO_RTI_PWR_A9_CNTL0)
#define P_HHI_SYS_CPU_CLK_CNTL		CBUS_REG_ADDR(HHI_SYS_CPU_CLK_CNTL)
#define P_AO_RTI_PWR_A9_MEM_PD0         AOBUS_REG_ADDR(AO_RTI_PWR_A9_MEM_PD0)
#define P_AO_RTI_PWR_A9_CNTL1       AOBUS_REG_ADDR(AO_RTI_PWR_A9_CNTL1)

#define CPU_POWER_CTRL_REG (uint32_t)(periph_membase + 0x8)
#define CPU1_CONTROL_ADDR_REG (uint32_t)(sram_membase + 0x1ff84)
#define CPU_CONTROL_REG (uint32_t)(sram_membase + 0x1ff80)


static inline void aml_set_reg32_bits(uint32_t _reg, const uint32_t _value,
	const uint32_t _start, const uint32_t	_len)
{
	writel(((readl((void *)_reg)
		& ~(((1L << (_len))-1) << (_start)))
		| ((unsigned int)((_value)&((1L<<(_len))-1)) << (_start))),
		(void *)_reg);
}

extern void meson_cleanup(void);
extern void meson_secondary_startup(void);

#endif
