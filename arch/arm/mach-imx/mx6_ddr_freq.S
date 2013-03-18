/*
 * Copyright (C) 2011-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/linkage.h>
#include <mach/hardware.h>

	.align 3

	.macro	phy2virt

	/* phy io map to vitr according to IMX_IO_P2V define */
	mov	r7, #0x100000
	sub	r7, r7, #0x1
	and	r7, r8, r7
	and	r6, r8, #0x50000000
	add	r7, r7, r6, LSR #6
	and	r6, r8, #0xb000000
	add	r7, r7, r6, LSR #4
	and	r6, r8, #0x80000000
	add	r7, r7, #0xf4000000
	add	r8, r7, r6, LSR #7

	.endm

	.macro	switch_to_528MHz

	/* check if periph_clk_sel is already set */
	ldr	r0, [r6, #0x14]
	and	r0, r0, #0x2000000
	cmp	r0, #0x2000000
	beq	set_ahb_podf_before_switch

	/* change periph_clk to be sourced from pll3_clk. */
	ldr	r0, [r6, #0x18]
	bic	r0, r0, #0x3000
	str	r0, [r6, #0x18]

	ldr	r0, [r6, #0x14]
	bic	r0, r0, #0x38000000
	str	r0, [r6, #0x14]

	/*
	 * set the AHB dividers before the switch,
	 * don't change AXI clock divider,
	 * set the MMDC_DIV=1, AXI_DIV = 2, AHB_DIV=4,
	 * (need to maintain GPT divider).
	 */
	ldr	r0, [r6, #0x14]
	ldr	r2, =0x3f1f00
	bic	r0, r0, r2
	orr	r0, r0, #0xd00
	orr	r0, r0, #0x10000
	str	r0, [r6, #0x14]

wait_div_update528:
	ldr	r0, [r6, #0x48]
	cmp	r0, #0
	bne	wait_div_update528

	/* now switch periph_clk to pll3_main_clk. */
	ldr	r0, [r6, #0x14]
	orr	r0, r0, #0x2000000
	str	r0, [r6, #0x14]

periph_clk_switch3:
	ldr	r0, [r6, #0x48]
	cmp	r0, #0
	bne	periph_clk_switch3

	b	switch_pre_periph_clk_528

set_ahb_podf_before_switch:
	/*
	 * set the MMDC_DIV=1, AXI_DIV = 2, AHB_DIV=4,
	 * (need to maintain GPT divider).
	 */
	ldr	r0, [r6, #0x14]
	ldr	r2, =0x3f1f00
	bic	r0, r0, r2
	orr	r0, r0, #0xd00
	orr	r0, r0, #0x10000
	str	r0, [r6, #0x14]

wait_div_update528_1:
	ldr	r0, [r6, #0x48]
	cmp	r0, #0
	bne	wait_div_update528_1

switch_pre_periph_clk_528:

	/* now switch pre_periph_clk to PLL2_528MHz. */
	ldr	r0, [r6, #0x18]
	bic	r0, r0, #0xC0000
	str	r0, [r6, #0x18]

	/* now switch periph_clk back. */
	ldr	r0, [r6, #0x14]
	bic	r0, r0, #0x2000000
	str	r0, [r6, #0x14]

periph_clk_switch4:
	ldr	r0, [r6, #0x48]
	cmp	r0, #0
	bne	periph_clk_switch4

	/* change the GPT divider so that its at 6MHz. */
	ldr	r0, [r6, #0x1C]
	bic	r0, r0, #0x3F
	orr	r0, r0, #0xA
	str	r0, [r6, #0x1C]
	.endm

	.macro	switch_to_400MHz

	/* check if periph_clk_sel is already set. */
	ldr	r0, [r6, #0x14]
	and	r0, r0, #0x2000000
	cmp	r0, #0x2000000
	beq	set_ahb_podf_before_switch1

	/* change periph_clk to be sourced from pll3_clk. */
	ldr	r0, [r6, #0x18]
	bic	r0, r0, #0x3000
	str	r0, [r6, #0x18]

	ldr	r0, [r6, #0x14]
	bic	r0, r0, #0x38000000
	str	r0, [r6, #0x14]

	/* now switch periph_clk to pll3_main_clk. */
	ldr	r0, [r6, #0x14]
	orr	r0, r0, #0x2000000
	str	r0, [r6, #0x14]

periph_clk_switch5:
	ldr	r0, [r6, #0x48]
	cmp	r0, #0
	bne	periph_clk_switch5

	b	switch_pre_periph_clk_400

set_ahb_podf_before_switch1:
	/*
	 * set the MMDC_DIV=1, AXI_DIV = 2, AHB_DIV=4,
	 * (need to maintain GPT divider).
	 */
	ldr	r0, [r6, #0x14]
	ldr	r2, =0x3f1f00
	bic	r0, r0, r2
	orr	r0, r0, #0x900
	orr	r0, r0, #0x10000
	str	r0, [r6, #0x14]

wait_div_update400_1:
	ldr	r0, [r6, #0x48]
	cmp	r0, #0
	bne	wait_div_update400_1

switch_pre_periph_clk_400:

	/* now switch pre_periph_clk to PFD_400MHz. */
	ldr	r0, [r6, #0x18]
	bic	r0, r0, #0xC0000
	orr	r0, r0, #0x40000
	str	r0, [r6, #0x18]

	/* now switch periph_clk back. */
	ldr	r0, [r6, #0x14]
	bic	r0, r0, #0x2000000
	str	r0, [r6, #0x14]

periph_clk_switch6:
	ldr	r0, [r6, #0x48]
	cmp	r0, #0
	bne	periph_clk_switch6

	/*
	 * change AHB divider so that we are at 400/3=133MHz.
	 * don't change AXI clock divider.
	 * set the MMDC_DIV=1, AXI_DIV=2, AHB_DIV=3,
	 * (need to maintain GPT divider).
	 */
	ldr	r0, [r6, #0x14]
	ldr	r2, =0x3f1f00
	bic	r0, r0, r2
	orr	r0, r0, #0x900
	orr	r0, r0, #0x10000
	str	r0, [r6, #0x14]

wait_div_update400_2:
	ldr	r0, [r6, #0x48]
	cmp	r0, #0
	bne	wait_div_update400_2

	/* change the GPT divider so that its at 6MHz. */
	ldr	r0, [r6, #0x1C]
	bic	r0, r0, #0x3F
	orr	r0, r0, #0xA
	str	r0, [r6, #0x1C]

	.endm

	.macro	switch_to_50MHz

	/* check if periph_clk_sel is already set. */
	ldr	r0, [r6, #0x14]
	and	r0, r0, #0x2000000
	cmp	r0, #0x2000000
	beq	switch_pre_periph_clk_50

	/*
	 * set the periph_clk to be sourced from PLL2_PFD_200M
	 * change periph_clk to be sourced from pll3_clk.
	 * ensure PLL3 is the source and set the divider to 1.
	 */
	ldr	r0, [r6, #0x18]
	bic	r0, r0, #0x3000
	str	r0, [r6, #0x18]

	ldr	r0, [r6, #0x14]
	bic	r0, r0, #0x38000000
	str	r0, [r6, #0x14]

	/* now switch periph_clk to pll3_main_clk. */
	ldr	r0, [r6, #0x14]
	orr	r0, r0, #0x2000000
	str	r0, [r6, #0x14]

periph_clk_switch_50:
	ldr	r0, [r6, #0x48]
	cmp	r0, #0
	bne	periph_clk_switch_50

switch_pre_periph_clk_50:

	/* now switch pre_periph_clk to PFD_200MHz. */
	ldr	r0, [r6, #0x18]
	orr	r0, r0, #0xC0000
	str	r0, [r6, #0x18]

	/*
	 * set the MMDC_DIV=4, AXI_DIV = 4, AHB_DIV=8,
	 * (need to maintain GPT divider).
	 */
	ldr	r0, [r6, #0x14]
	ldr	r2, =0x3f1f00
	bic	r0, r0, r2

	orr	r0, r0, #0x180000
	orr	r0, r0, #0x30000

	/*
	 * if changing AHB divider remember to change
	 * the IPGPER divider too below.
	 */
	orr	r0, r0, #0x1d00
	str	r0, [r6, #0x14]

wait_div_update_50:
	ldr	r0, [r6, #0x48]
	cmp	r0, #0
	bne	wait_div_update_50

	/* now switch periph_clk back. */
	ldr	r0, [r6, #0x14]
	bic	r0, r0, #0x2000000
	str	r0, [r6, #0x14]

periph_clk_switch2:
	ldr	r0, [r6, #0x48]
	cmp	r0, #0
	bne	periph_clk_switch2

	/* change the GPT divider so that its at 6MHz. */
	ldr	r0, [r6, #0x1C]
	bic	r0, r0, #0x3F
	orr	r0, r0, #0x1
	str	r0, [r6, #0x1C]

	.endm

	.macro	switch_to_24MHz
	/*
	 * change the freq now try setting DDR to 24MHz.
	 * source it from the periph_clk2 ensure the
	 * periph_clk2 is sourced from 24MHz and the
	 * divider is 1.
	 */

	ldr	r0, [r6, #0x18]
	bic	r0, r0, #0x3000
	orr	r0, r0, #0x1000
	str	r0, [r6, #0x18]

	ldr	r0, [r6, #0x14]
	bic	r0, r0, #0x38000000
	str	r0, [r6, #0x14]

	/* now switch periph_clk to 24MHz. */
	ldr	r0, [r6, #0x14]
	orr	r0, r0, #0x2000000
	str	r0, [r6, #0x14]

periph_clk_switch1:
	ldr	r0, [r6, #0x48]
	cmp	r0, #0
	bne	periph_clk_switch1

	/* change all the dividers to 1. */
	ldr	r0, [r6, #0x14]
	ldr	r2, =0x3f1f00
	bic	r0, r0, r2
	orr	r0, r0, #0x100
	str	r0, [r6, #0x14]

	/* Wait for the divider to change. */
wait_div_update:
	ldr	r0, [r6, #0x48]
	cmp	r0, #0
	bne	wait_div_update

	/* change the GPT divider so that its at 6MHz. */
	ldr	r0, [r6, #0x1C]
	bic	r0, r0, #0x3F
	orr	r0, r0, #0x1
	str	r0, [r6, #0x1C]

	.endm

/*
 *  mx6_ddr_freq_change
 *
 *  idle the processor (eg, wait for interrupt).
 *  make sure DDR is in self-refresh.
 *  IRQs are already disabled.
 */
ENTRY(mx6_ddr_freq_change)

	stmfd	sp!, {r4-r11}

	ldr	r8, =MX6Q_CCM_BASE_ADDR
	phy2virt
	mov	r4, r8

	ldr	r8, =MX6Q_MMDC_P0_BASE_ADDR
	phy2virt
	mov	r5, r8

	ldr	r8, =MX6Q_IOMUXC_BASE_ADDR
	phy2virt
	mov	r6, r4
	mov	r7, r8
	mov	r4, r0
	mov	r8, r1
	mov	r9, r2
	mov	r11, r3

ddr_freq_change:
	/*
	 * make sure no TLB miss will occur when
	 * the DDR is in self refresh. invalidate
	 * TLB single entry to ensure that the
	 * address is not already in the TLB.
	 */

	adr	r10, ddr_freq_change

	mcr	p15, 0, r10, c8, c7, 1
	mcr	p15, 0, r6, c8, c7, 1
	mcr	p15, 0, r5, c8, c7, 1
	mcr	p15, 0, r7, c8, c7, 1

	mrc	p15, 0, r0, c10, c0, 0
	orr	r0, r0, #1
	mcr	p15, 0, r0, c10, c0, 0

	ldr	r2, [r6]
	ldr	r2, [r5]
	ldr	r2, [r7]
	ldr	r2, [r8]
	ldr	r2, [r10]
	ldr	r2, [r11]

	mrc	p15, 0, r0, c10, c0, 0
	bic	r0, r0, #1
	mcr	p15, 0, r0, c10, c0, 0

	/* disable automatic power saving. */
	ldr	r0, [r5, #0x404]
	orr	r0, r0, #0x01
	str	r0, [r5, #0x404]

	/* disable MMDC power down timer. */
	ldr	r0, [r5, #0x4]
	bic	r0, r0, #0xff00
	str	r0, [r5, #0x4]

	/* delay for a while */
	ldr	r1, =4
delay1:
	ldr	r2, =0
cont1:
	ldr	r0, [r5, r2]
	add	r2, r2, #4
	cmp	r2, #16
	bne	cont1
	sub	r1, r1, #1
	cmp	r1, #0
	bgt	delay1

	/* set CON_REG */
	ldr	r0, =0x8000
	str	r0, [r5, #0x1C]
poll_conreq_set_1:
	ldr	r0, [r5, #0x1C]
	and	r0, r0, #0x4000
	cmp	r0, #0x4000
	bne	poll_conreq_set_1

	ldr	r0, =0x00008010
	str	r0, [r5, #0x1C]
	ldr	r0, =0x00008018
	str	r0, [r5, #0x1C]

	/*
	 * if requested frequency is greater than
	 * 300MHz go to DLL on mode.
	 */
	ldr	r1, =300000000
	cmp	r4, r1
	bge	dll_on_mode

dll_off_mode:

	/* if DLL is currently on, turn it off. */
	cmp	r9, #1
	beq	continue_dll_off_1

	ldr	r0, =0x00018031
	str	r0, [r5, #0x1C]

	ldr	r0, =0x00018039
	str	r0, [r5, #0x1C]

	ldr	r1, =10
delay1a:
	ldr	r2, =0
cont1a:
	ldr	r0, [r5, r2]
	add	r2, r2, #4
	cmp	r2, #16
	bne	cont1a
	sub	r1, r1, #1
	cmp	r1, #0
	bgt	delay1a

continue_dll_off_1:
	/* set DVFS - enter self refresh mode */
	ldr	r0, [r5, #0x404]
	orr	r0, r0, #0x200000
	str	r0, [r5, #0x404]

	/* de-assert con_req */
	mov	r0, #0x0
	str	r0, [r5, #0x1C]

poll_dvfs_set_1:
	ldr	r0, [r5, #0x404]
	and	r0, r0, #0x2000000
	cmp	r0, #0x2000000
	bne	poll_dvfs_set_1

	ldr	r1, =24000000
	cmp	r4, r1
	beq	switch_freq_24

	switch_to_50MHz
	b	continue_dll_off_2

switch_freq_24:
	switch_to_24MHz

continue_dll_off_2:

	/* set SBS - block ddr accesses */
	ldr	r0, [r5, #0x410]
	orr	r0, r0, #0x100
	str	r0, [r5, #0x410]

	/* clear DVFS - exit from self refresh mode */
	ldr	r0, [r5, #0x404]
	bic	r0, r0, #0x200000
	str	r0, [r5, #0x404]

poll_dvfs_clear_1:
	ldr	r0, [r5, #0x404]
	and	r0, r0, #0x2000000
	cmp	r0, #0x2000000
	beq	poll_dvfs_clear_1

	/* if DLL was previously on, continue DLL off routine. */
	cmp    	r9, #1
	beq 	continue_dll_off_3

	ldr	r0, =0x00018031
	str	r0, [r5, #0x1C]

	ldr	r0, =0x00018039
	str	r0, [r5, #0x1C]

	ldr	r0, =0x08208030
	str	r0, [r5, #0x1C]

	ldr	r0, =0x08208038
	str	r0, [r5, #0x1C]

	ldr	r0, =0x00088032
	str	r0, [r5, #0x1C]

	ldr	r0, =0x0008803A
	str	r0, [r5, #0x1C]

	/* delay for a while. */
	ldr	r1, =4
delay_1:
	ldr	r2, =0
cont_1:
	ldr	r0, [r5, r2]
	add	r2, r2, #4
	cmp	r2, #16
	bne	cont_1
	sub	r1, r1, #1
	cmp	r1, #0
	bgt	delay_1

	ldr	r0, [r5, #0x0C]
	bic	r0, r0, #0xf
	orr	r0, r0, #0x3
	str	r0, [r5, #0x0C]

	ldr	r0, [r5, #0x10]
	bic	r0, r0, #0x7
	orr	r0, r0, #0x4
	str	r0, [r5, #0x10]

	ldr	r0, =0x00091680
	str	r0, [r5, #0x18]

	/* enable dqs pull down in the IOMUX. */
	ldr	r1, [r11]
	add	r11, r11, #8
	ldr	r2, =0x3028
update_iomux:
	ldr	r0, [r11, #0x0]
	ldr	r3, [r7, r0]
	bic	r3, r3, r2
	orr	r3, r3, #0x3000
	orr	r3, r3, #0x28
	str	r3, [r7, r0]
	add	r11, r11, #8
	sub	r1, r1, #1
	cmp	r1, #0
	bgt	update_iomux

	/*  ODT disabled. */
	ldr	r0, =0x0
	ldr	r2, =0x818
	str	r0, [r5, r2]
	ldr	r2, =0x4818
	str	r0, [r5, r2]

	/* DQS gating disabled. */
	ldr	r2, =0x83c
	ldr	r0, [r5, r2]
	orr	r0, r0, #0x20000000
	str	r0, [r5, r2]

	ldr	r2, =0x483c
	ldr	r0, [r5, r2]
	orr	r0, r0, #0x20000000
	str	r0, [r5, r2]

	/* MMDC0_MAPSR adopt power down enable. */
	ldr	r0, [r5, #0x404]
	bic	r0, r0, #0x01
	str	r0, [r5, #0x404]

	/* frc_msr + mu bypass*/
	ldr	r0, =0x00000060
	str	r0, [r5, #0x8b8]
	ldr	r2, =0x48b8
	str	r0, [r5, r2]
	ldr	r0, =0x00000460
	str	r0, [r5, #0x8b8]
	ldr	r2, =0x48b8
	str	r0, [r5, r2]
	ldr	r0, =0x00000c60
	str	r0, [r5, #0x8b8]
	ldr	r2, =0x48b8
	str	r0, [r5, r2]

continue_dll_off_3:
	/* clear SBS - unblock accesses to DDR. */
	ldr	r0, [r5, #0x410]
	bic	r0, r0, #0x100
	str	r0, [r5, #0x410]

	mov	r0, #0x0
	str	r0, [r5, #0x1C]
poll_conreq_clear_1:
	ldr	r0, [r5, #0x1C]
	and	r0, r0, #0x4000
	cmp	r0, #0x4000
	beq	poll_conreq_clear_1

	b	done

dll_on_mode:
	/* assert DVFS - enter self refresh mode. */
	ldr	r0, [r5, #0x404]
	orr	r0, r0, #0x200000
	str	r0, [r5, #0x404]

	/* de-assert CON_REQ. */
	mov	r0, #0x0
	str	r0, [r5, #0x1C]

	/* poll DVFS ack. */
poll_dvfs_set_2:
	ldr	r0, [r5, #0x404]
	and	r0, r0, #0x2000000
	cmp	r0, #0x2000000
	bne	poll_dvfs_set_2

	ldr	r1, =528000000
	cmp	r4, r1
	beq	switch_freq_528

	switch_to_400MHz

	b	continue_dll_on

switch_freq_528:
	switch_to_528MHz

continue_dll_on:

	/* set SBS step-by-step mode. */
	ldr	r0, [r5, #0x410]
	orr	r0, r0, #0x100
	str	r0, [r5, #0x410]

	/* clear DVFS - exit self refresh mode. */
	ldr	r0, [r5, #0x404]
	bic	r0, r0, #0x200000
	str	r0, [r5, #0x404]

poll_dvfs_clear_2:
	ldr	r0, [r5, #0x404]
	and	r0, r0, #0x2000000
	cmp	r0, #0x2000000
	beq	poll_dvfs_clear_2

	/* if DLL is currently off, turn it back on. */
	cmp	r9, #0
	beq	update_calibration_only

	ldr	r0, =0xa5390003
	str	r0, [r5, #0x800]
	ldr	r2, =0x4800
	str	r0, [r5, r2]

	/* enable DQS gating. */
	ldr	r2, =0x83c
	ldr	r0, [r5, r2]
	bic	r0, r0, #0x20000000
	str	r0, [r5, r2]

	ldr	r2, =0x483c
	ldr	r0, [r5, r2]
	bic	r0, r0, #0x20000000
	str	r0, [r5, r2]

	/* force measure. */
	ldr	r0, =0x00000800
	str	r0, [r5, #0x8b8]
	ldr	r2, =0x48b8
	str	r0, [r5, r2]

	/* delay for while. */
	ldr	r1, =4
delay5:
	ldr	r2, =0
cont5:
	ldr	r0, [r5, r2]
	add	r2, r2, #4
	cmp	r2, #16
	bne	cont5
	sub	r1, r1, #1
	cmp	r1, #0
	bgt	delay5

	/* disable dqs pull down in the IOMUX. */
	ldr	r1, [r11]
	add	r11, r11, #8
update_iomux1:
	ldr	r0, [r11, #0x0]
	ldr	r3, [r11, #0x4]
	str	r3, [r7, r0]
	add	r11, r11, #8
	sub	r1, r1, #1
	cmp	r1, #0
	bgt	update_iomux1

	/* config ESDCTL timings to 528MHz. */
	ldr	r9, [r8]
	add	r8, r8, #8
	ldr	r0, [r8, #0x0]
	ldr	r3, [r8, #0x4]
	str	r3, [r5, r0]
	add	r8, r8, #8

	ldr	r0, [r8, #0x0]
	ldr	r3, [r8, #0x4]
	str	r3, [r5, r0]
	add	r8, r8, #8

	/* update MISC register: WALAT, RALAT */
	ldr	r0, =0x00081740
	str	r0, [r5, #0x18]

	/* configure ddr devices to dll on, odt. */
	ldr	r0, =0x00028031
	str	r0, [r5, #0x1C]

	ldr	r0, =0x00028039
	str	r0, [r5, #0x1C]

	/* delay for while. */
	ldr	r1, =4
delay7:
	ldr	r2, =0
cont7:
	ldr	r0, [r5, r2]
	add	r2, r2, #4
	cmp	r2, #16
	bne	cont7
	sub	r1, r1, #1
	cmp	r1, #0
	bgt	delay7

	/* reset dll. */
	ldr	r0, =0x09208030
	str	r0, [r5, #0x1C]

	ldr	r0, =0x09208038
	str	r0, [r5, #0x1C]

	/* delay for while. */
	ldr	r1, =100
delay8:
	ldr	r2, =0
cont8:
	ldr	r0, [r5, r2]
	add	r2, r2, #4
	cmp	r2, #16
	bne	cont8
	sub	r1, r1, #1
	cmp	r1, #0
	bgt	delay8

	ldr	r0, [r8, #0x0]
	ldr	r3, [r8, #0x4]
	str	r3, [r5, r0]
	add	r8, r8, #8

	ldr	r0, [r8, #0x0]
	ldr	r3, [r8, #0x4]
	str	r3, [r5, r0]
	add	r8, r8, #8

	ldr	r0, =0x00428031
	str	r0, [r5, #0x1C]

	ldr	r0, =0x00428039
	str	r0, [r5, #0x1C]

	ldr	r0, [r8, #0x0]
	ldr	r3, [r8, #0x4]
	str	r3, [r5, r0]
	add	r8, r8, #8

	ldr	r0, [r8, #0x0]
	ldr	r3, [r8, #0x4]
	str	r3, [r5, r0]
	add	r8, r8, #8

	/* issue a zq command. */
	ldr	r0, =0x04008040
	str	r0, [r5, #0x1C]

	ldr	r0, =0x04008048
	str	r0, [r5, #0x1C]

	/* ESDCTL ODT enable. */
	ldr	r0, [r8, #0x0]
	ldr	r3, [r8, #0x4]
	str	r3, [r5, r0]
	add	r8, r8, #8

	ldr	r2, =0x4818
	str	r0, [r5, r2]

	/* delay for while. */
	ldr	r1, =40
delay15:
	ldr	r2, =0
cont15:
	ldr	r0, [r5, r2]
	add	r2, r2, #4
	cmp	r2, #16
	bne	cont15
	sub	r1, r1, #1
	cmp	r1, #0
	bgt	delay15

	/* MMDC0_MAPSR adopt power down enable. */
	ldr	r0, [r5, #0x404]
	bic	r0, r0, #0x01
	str	r0, [r5, #0x404]

	/* enable MMDC power down timer. */
	ldr	r0, [r5, #0x4]
	orr	r0, r0, #0x5500
	str	r0, [r5, #0x4]

	b	update_calibration

update_calibration_only:
	ldr	r1, [r8]
	sub	r1, r1, #7
	add	r8, r8, #64
	b	update_calib

update_calibration:
	/* write the new calibration values. */
	mov	r1, r9
	sub	r1, r1, #7

update_calib:
	ldr	r0, [r8, #0x0]
	ldr	r3, [r8, #0x4]
	str	r3, [r5, r0]
	add	r8, r8, #8
	sub	r1, r1, #1
	cmp	r1, #0
	bgt	update_calib

	/* perform a force measurement. */
	ldr	r0, =0x800
	str	r0, [r5, #0x8B8]
	ldr	r2, =0x48B8
	str	r0, [r5, r2]

	/* clear SBS - unblock DDR accesses. */
	ldr	r0, [r5, #0x410]
	bic	r0, r0, #0x100
	str	r0, [r5, #0x410]

	mov	r0, #0x0
	str	r0, [r5, #0x1C]
poll_conreq_clear_2:
	ldr	r0, [r5, #0x1C]
	and	r0, r0, #0x4000
	cmp	r0, #0x4000
	beq	poll_conreq_clear_2

done:
	/* restore registers */

	ldmfd	sp!, {r4-r11}
	mov	pc, lr

	.type	mx6_do_ddr_freq_change, #object
ENTRY(mx6_do_ddr_freq_change)
	.word	mx6_ddr_freq_change
	.size	mx6_ddr_freq_change, . - mx6_ddr_freq_change
