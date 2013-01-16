/*
 * Copyright (C) 2010-2013 Freescale Semiconductor, Inc. All Rights Reserved.
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

	.macro	phy2Virt
	/* phy io map to vitr according to IMX_IO_P2V define */
	and	r4, r8, #0x80000000
	and	r5, r8, #0x50000000
	and	r6, r8, #0xb000000
	mov	r7, #0x100000
	sub	r7, r7, #0x1
	and	r7, r8, r7
	add	r7, r7, r5, LSR #6
	add	r7, r7, r6, LSR #4
	add	r7, r7, #0xf4000000
	add	r8, r7, r4, LSR #7

	.endm

	.macro	dq_ddr_io_save

	ldr	r4, [r8, #0x5ac] /* DRAM_DQM0 */
	ldr	r5, [r8, #0x5b4] /* DRAM_DQM1 */
	ldr	r6, [r8, #0x528] /* DRAM_DQM2 */
	ldr	r7, [r8, #0x520] /* DRAM_DQM3 */
	stmfd	r10!, {r4-r7}

	ldr	r4, [r8, #0x514] /* DRAM_DQM4 */
	ldr	r5, [r8, #0x510] /* DRAM_DQM5 */
	ldr	r6, [r8, #0x5bc] /* DRAM_DQM6 */
	ldr	r7, [r8, #0x5c4] /* DRAM_DQM7 */
	stmfd	r10!, {r4-r7}

	ldr	r4, [r8, #0x56c] /* DRAM_CAS */
	ldr	r5, [r8, #0x578] /* DRAM_RAS */
	ldr	r6, [r8, #0x588] /* DRAM_SDCLK_0 */
	ldr	r7, [r8, #0x594] /* DRAM_SDCLK_1 */
	stmfd	r10!, {r4-r7}

	ldr	r5, [r8, #0x750] /* DDRMODE_CTL */
	ldr	r6, [r8, #0x774] /* DDRMODE */
	stmfd	r10!, {r5-r6}

	ldr	r4, [r8, #0x5a8] /* DRAM_SDQS0 */
	ldr	r5, [r8, #0x5b0] /* DRAM_SDQS1 */
	ldr	r6, [r8, #0x524] /* DRAM_SDQS2 */
	ldr	r7, [r8, #0x51c] /* DRAM_SDQS3 */
	stmfd	r10!, {r4-r7}

	ldr	r4, [r8, #0x518] /* DRAM_SDQS4 */
	ldr	r5, [r8, #0x50c] /* DRAM_SDQS5 */
	ldr	r6, [r8, #0x5b8] /* DRAM_SDQS6 */
	ldr	r7, [r8, #0x5c0] /* DRAM_SDQS7 */
	stmfd	r10!, {r4-r7}

	ldr	r4, [r8, #0x784] /* GPR_B0DS */
	ldr	r5, [r8, #0x788] /* GPR_B1DS */
	ldr	r6, [r8, #0x794] /* GPR_B2DS */
	ldr	r7, [r8, #0x79c] /* GPR_B3DS */
	stmfd	r10!, {r4-r7}

	ldr	r4, [r8, #0x7a0] /* GPR_B4DS */
	ldr	r5, [r8, #0x7a4] /* GPR_B5DS */
	ldr	r6, [r8, #0x7a8] /* GPR_B6DS */
	ldr	r7, [r8, #0x748] /* GPR_B7DS */
	stmfd	r10!, {r4-r7}

	ldr	r5, [r8, #0x74c] /* GPR_ADDS*/
	ldr	r6, [r8, #0x59c] /* DRAM_SODT0*/
	ldr	r7, [r8, #0x5a0] /* DRAM_SODT1*/
	stmfd	r10!, {r5-r7}

	.endm

	.macro	dq_ddr_io_restore

	ldmea	r10!, {r4-r7}
	str	r4, [r8, #0x5ac] /* DRAM_DQM0 */
	str	r5, [r8, #0x5b4] /* DRAM_DQM1 */
	str	r6, [r8, #0x528] /* DRAM_DQM2 */
	str	r7, [r8, #0x520] /* DRAM_DQM3 */

	ldmea	r10!, {r4-r7}
	str	r4, [r8, #0x514] /* DRAM_DQM4 */
	str	r5, [r8, #0x510] /* DRAM_DQM5 */
	str	r6, [r8, #0x5bc] /* DRAM_DQM6 */
	str	r7, [r8, #0x5c4] /* DRAM_DQM7 */

	ldmea	r10!, {r4-r7}
	str	r4, [r8, #0x56c] /* DRAM_CAS */
	str	r5, [r8, #0x578] /* DRAM_RAS */
	str	r6, [r8, #0x588] /* DRAM_SDCLK_0 */
	str	r7, [r8, #0x594] /* DRAM_SDCLK_1 */

	ldmea	r10!, {r5-r6}
	str	r5, [r8, #0x750] /* DDRMODE_CTL */
	str	r6, [r8, #0x774] /* DDRMODE */

	ldmea	r10!, {r4-r7}
	str	r4, [r8, #0x5a8] /* DRAM_SDQS0 */
	str	r5, [r8, #0x5b0] /* DRAM_SDQS1 */
	str	r6, [r8, #0x524] /* DRAM_SDQS2 */
	str	r7, [r8, #0x51c] /* DRAM_SDQS3 */

	ldmea	r10!, {r4-r7}
	str	r4, [r8, #0x518] /* DRAM_SDQS4 */
	str	r5, [r8, #0x50c] /* DRAM_SDQS5 */
	str	r6, [r8, #0x5b8] /* DRAM_SDQS6 */
	str	r7, [r8, #0x5c0] /* DRAM_SDQS7 */

	ldmea	r10!, {r4-r7}
	str	r4, [r8, #0x784] /* GPR_B0DS */
	str	r5, [r8, #0x788] /* GPR_B1DS */
	str	r6, [r8, #0x794] /* GPR_B2DS */
	str	r7, [r8, #0x79c] /* GPR_B3DS */

	ldmea	r10!, {r4-r7}
	str	r4, [r8, #0x7a0] /* GPR_B4DS */
	str	r5, [r8, #0x7a4] /* GPR_B5DS */
	str	r6, [r8, #0x7a8] /* GPR_B6DS */
	str	r7, [r8, #0x748] /* GPR_B7DS */

	ldmea	r10!, {r5-r7}
	str	r5, [r8, #0x74c] /* GPR_ADDS*/
	str	r6, [r8, #0x59c] /* DRAM_SODT0*/
	str	r7, [r8, #0x5a0] /* DRAM_SODT1*/

	.endm

	.macro	dq_ddr_io_set_lpm

	mov	r10, #0
	str	r10, [r8, #0x5ac] /* DRAM_DQM0 */
	str	r10, [r8, #0x5b4] /* DRAM_DQM1 */
	str	r10, [r8, #0x528] /* DRAM_DQM2 */
	str	r10, [r8, #0x520] /* DRAM_DQM3 */

	str	r10, [r8, #0x514] /* DRAM_DQM4 */
	str	r10, [r8, #0x510] /* DRAM_DQM5 */
	str	r10, [r8, #0x5bc] /* DRAM_DQM6 */
	str	r10, [r8, #0x5c4] /* DRAM_DQM7 */

	str	r10, [r8, #0x56c] /* DRAM_CAS */
	str	r10, [r8, #0x578] /* DRAM_RAS */
	str	r10, [r8, #0x588] /* DRAM_SDCLK_0 */
	str	r10, [r8, #0x594] /* DRAM_SDCLK_1 */

	str	r10, [r8, #0x750] /* DDRMODE_CTL */
	str	r10, [r8, #0x774] /* DDRMODE */

	str	r10, [r8, #0x5a8] /* DRAM_SDQS0 */
	str	r10, [r8, #0x5b0] /* DRAM_SDQS1 */
	str	r10, [r8, #0x524] /* DRAM_SDQS2 */
	str	r10, [r8, #0x51c] /* DRAM_SDQS3 */

	str	r10, [r8, #0x518] /* DRAM_SDQS4 */
	str	r10, [r8, #0x50c] /* DRAM_SDQS5 */
	str	r10, [r8, #0x5b8] /* DRAM_SDQS6 */
	str	r10, [r8, #0x5c0] /* DRAM_SDQS7 */

	str	r10, [r8, #0x784] /* GPR_B0DS */
	str	r10, [r8, #0x788] /* GPR_B1DS */
	str	r10, [r8, #0x794] /* GPR_B2DS */
	str	r10, [r8, #0x79c] /* GPR_B3DS */

	str	r10, [r8, #0x7a0] /* GPR_B4DS */
	str	r10, [r8, #0x7a4] /* GPR_B5DS */
	str	r10, [r8, #0x7a8] /* GPR_B6DS */
	str	r10, [r8, #0x748] /* GPR_B7DS */

	str	r10, [r8, #0x74c] /* GPR_ADDS*/
	str	r10, [r8, #0x59c] /* DRAM_SODT0*/
	str	r10, [r8, #0x5a0] /* DRAM_SODT1*/

	.endm
ENTRY(imx_suspend)

	/* counting the resume address in iram
	 * to set it in SRC register */
	ldr	r4, =imx_suspend
	ldr	r5, =resume
	sub	r5, r5, r4
	add	r9, r1, r5

	/* make sure TLB contian the addr we want,
	 * as we will access after DDR IO floated.
	 */
	ldr	r8, =MX6Q_ANATOP_BASE_ADDR
	phy2virt
	ldr	r7, [r8, #0x140]

	/* must use r8 to store the IO address */
	ldr	r8, =MX6Q_SRC_BASE_ADDR
	phy2virt

	/* read previous resume address from SRC
	 * register, which is v7_cpu_resume, this
	 * is for the jump when we finish DDR IO
	 * restore */
	ldr	r5, [r8, #0x20]
	add	r10, r0, #0x1000
	stmfd	r10!, {r5}

	str	r9, [r8, #0x20]
	add	r3, r1, #0x1000
	str	r3, [r8, #0x24]

	ldr	r8, =MX6Q_IOMUXC_BASE_ADDR
	phy2virt

	dq_ddr_io_save

	ldr	r8, =MX6Q_MMDC_P0_BASE_ADDR
	phy2virt

	/* Put DDR explicitly into self-refresh. */
	/* Disable Automatic power savings. */
	ldr	r7, [r8, #0x404]
	orr	r7, r7, #0x01
	str	r7, [r8, #0x404]

	/* Make the DDR explicitly enter self-refresh. */
	ldr	r7, [r8, #0x404]
	orr	r7, r7, #0x200000
	str	r7, [r8, #0x404]

poll_dvfs_set_1:
	ldr	r7, [r8, #0x404]
	and	r7, r7, #0x2000000
	cmp	r7, #0x2000000
	bne	poll_dvfs_set_1

	ldr	r8, =MX6Q_IOMUXC_BASE_ADDR
	phy2virt
	dq_ddr_io_set_lpm

	/* if internal ldo(VDDARM) bypassed,analog bypass
	 * it for DSM(0x1e) and restore it when resume(0x1f).
	 */
	ldr	r8, =MX6Q_ANATOP_BASE_ADDR
	phy2virt
	ldr	r7, [r8, #0x140]
	and	r7, r7, #0x1f
	cmp	r7, #0x1f
	bne	ldo_check_done1
ldo_analog_bypass:
	ldr	r7, [r8, #0x140]
	bic	r7, r7, #0x1f
	orr	r7, r7, #0x1e
	str	r7, [r8, #0x140]
ldo_check_done1:

	/* Zzz, enter stop mode */
	wfi
	nop
	nop
	nop
	nop

/*******************************************************
run to here means there is pending wakeup source, system
should auto resume, we need to restore DDR IO first
********************************************************/
	/* restore it with 0x1f if use ldo bypass mode.*/
	ldr	r8, =MX6Q_ANATOP_BASE_ADDR
	phy2virt
	ldr	r7, [r8, #0x140]
	and	r7, r7, #0x1f
	cmp	r7, #0x1e
	bne	ldo_check_done2
ldo_bypass_restore:
	ldr	r7, [r8, #0x140]
	orr	r7, r7, #0x1f
	str	r7, [r8, #0x140]
ldo_check_done2:

	add	r10, r0, #0x1000
	/* skip the lr saved in iram */
	sub	r10, r10, #0x4

	ldr	r8, =MX6Q_IOMUXC_BASE_ADDR
	phy2virt
	dq_ddr_io_restore

	ldr	r8, =MX6Q_MMDC_P0_BASE_ADDR
	phy2virt
	/* let DDR out of self-refresh. */
	ldr	r7, [r8, #0x404]
	bic	r7, r7, #0x200000
	str	r7, [r8, #0x404]

poll_dvfs_clear_2:
	ldr	r7, [r8, #0x404]
	and	r7, r7, #0x2000000
	cmp	r7, #0x2000000
	beq     poll_dvfs_clear_2
	/* enable DDR auto power saving */
	ldr	r7, [r8, #0x404]
	bic	r7, r7, #0x1
	str	r7, [r8, #0x404]
	/* return to suspend finish */
	mov	pc, lr

resume:
	/* restore it with 0x1f if use ldo bypass mode.*/
	ldr	r8, =MX6Q_ANATOP_BASE_ADDR
	ldr	r7, [r8, #0x140]
	and	r7, r7, #0x1f
	cmp	r7, #0x1e
	bne	ldo_check_done3
	ldr	r7, [r8, #0x140]
	orr	r7, r7, #0x1f
	str	r7, [r8, #0x140]
ldo_check_done3:

	ldr	r5, =MX6Q_SRC_BASE_ADDR
	ldr	r10, [r5, #0x24]
	ldmea	r10!, {lr}

	ldr	r8, =MX6Q_IOMUXC_BASE_ADDR
	dq_ddr_io_restore

	ldr	r8, =MX6Q_MMDC_P0_BASE_ADDR
	/* let DDR out of self-refresh */
	ldr	r7, [r8, #0x404]
	bic	r7, r7, #0x200000
	str	r7, [r8, #0x404]

poll_dvfs_clear_1:
	ldr	r7, [r8, #0x404]
	and	r7, r7, #0x2000000
	cmp	r7, #0x2000000
	beq	poll_dvfs_clear_1
	/* enable DDR auto power saving */
	ldr	r7, [r8, #0x404]
	bic	r7, r7, #0x1
	str	r7, [r8, #0x404]
	mov	pc, lr

	.type	imx_do_suspend, #object
ENTRY(imx_do_suspend)
	.word	imx_suspend
	.size	imx_suspend, . - imx_suspend
