/*
 * arch/arm/mach-meson/hotplug-asm.S
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

#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/cache.h>
#include <asm/cp15.h>
#include <asm/hardware/cache-l2x0.h>

.macro cpu_id, rd
	mrc	p15, 0, \rd, c0, c0, 5
	and	\rd, \rd, #0xF
.endm

.macro mov32, reg, val
	movw	\reg, #:lower16:\val
	movt	\reg, #:upper16:\val
.endm

.macro exit_smp, tmp1, tmp2
	mrc	p15, 0, \tmp1, c1, c0, 1	@ ACTLR
	bic	\tmp1, \tmp1, #(1<<6) | (1<<0)	@ clear ACTLR.SMP | ACTLR.FW
	mcr	p15, 0, \tmp1, c1, c0, 1	@ ACTLR
	isb
	cpu_id	\tmp1
	mov	\tmp1, \tmp1, lsl #2
	mov	\tmp2, #0xf
	mov	\tmp2, \tmp2, lsl \tmp1
	mov32	\tmp1, 0xFE00100C
	str	\tmp2, [\tmp1]			@ invalidate SCU tags for CPU
	dsb
.endm

ENTRY(meson_cleanup)
	stmfd	sp!, {r0-r11, lr}

	@ disable interrupt
	mrs r4, cpsr
	orr r5, r4, #0xc0
	msr cpsr_c, r5

	@ icache disable
	mrc p15, 0, r4, c1, c0, 0
	bic r4, r4, #(CR_I)
	mcr p15, 0, r4, c1, c0, 0
	isb

	@ invalidate I-cache inner shareable
	mov	r0, #0
	mcr	p15, 0, r0, c7, c1, 0
	isb

	ldmfd	sp!, {r0-r11, pc}
ENDPROC(meson_cleanup)

