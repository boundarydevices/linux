/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/linkage.h>
#include <asm/asm-offsets.h>
#include "hardware.h"

/*
 * ==================== low level suspend ====================
 *
 * Better to follow below rules to use ARM registers:
 * r0: pm_info structure address;
 * r1 ~ r4: for saving pm_info members;
 * r5 ~ r10: free registers;
 * r11: io base address.
 *
 * suspend ocram space layout:
 * ======================== high address ======================
 *                              .
 *                              .
 *                              .
 *                              ^
 *                              ^
 *                              ^
 *                      imx7_suspend code
 *              PM_INFO structure(imx7_cpu_pm_info)
 * ======================== low address =======================
 */

/*
 * Below offsets are based on struct imx7_cpu_pm_info
 * which defined in arch/arm/mach-imx/pm-imx7.c, this
 * structure contains necessary pm info for low level
 * suspend related code.
 */
#define PM_INFO_PBASE_OFFSET			0x0
#define PM_INFO_RESUME_ADDR_OFFSET		0x4
#define PM_INFO_DDR_TYPE_OFFSET			0x8
#define PM_INFO_PM_INFO_SIZE_OFFSET		0xC
#define PM_INFO_MX7_DDRC_P_OFFSET		0x10
#define PM_INFO_MX7_DDRC_V_OFFSET		0x14
#define PM_INFO_MX7_SRC_P_OFFSET		0x18
#define PM_INFO_MX7_SRC_V_OFFSET		0x1C
#define PM_INFO_MX7_IOMUXC_P_OFFSET		0x20
#define PM_INFO_MX7_IOMUXC_V_OFFSET		0x24
#define PM_INFO_MX7_CCM_P_OFFSET		0x28
#define PM_INFO_MX7_CCM_V_OFFSET		0x2C
#define PM_INFO_MX7_GPC_P_OFFSET		0x30
#define PM_INFO_MX7_GPC_V_OFFSET		0x34
#define PM_INFO_MX7_L2_P_OFFSET		0x38
#define PM_INFO_MX7_L2_V_OFFSET		0x3C
#define PM_INFO_MX7_ANATOP_P_OFFSET		0x40
#define PM_INFO_MX7_ANATOP_V_OFFSET		0x44
#define PM_INFO_MX7_TTBR1_V_OFFSET		0x48
#define PM_INFO_DDRC_IO_NUM_OFFSET		0x4c
#define PM_INFO_DDRC_IO_VAL_OFFSET		0x50
/* below offsets depends on MX6_MAX_DDRC_IO_NUM(33) definition */
#define PM_INFO_DDRC_NUM_OFFSET			0x158
#define PM_INFO_DDRC_VAL_OFFSET			0x15c

#define MX7_SRC_GPR1	0x74
#define MX7_SRC_GPR2	0x78
#define MX7_ANATOP_CORE	0x140
#define DDRC_STAT	0x4
#define DDRC_PWRCTL	0x30
#define DDRC_PSTAT	0x3fc

#define MX7_MAX_DDRC_IO_NUM             33
#define MX7_MAX_DDRC_NUM                34

	.align 3

	.macro store_ttbr1

	/* Store TTBR1 to pm_info->ttbr1 */
	mrc	p15, 0, r7, c2, c0, 1
	str	r7, [r0, #PM_INFO_MX7_TTBR1_V_OFFSET]

	/* Disable Branch Prediction, Z bit in SCTLR. */
	mrc	p15, 0, r6, c1, c0, 0
	bic	r6, r6, #0x800
	mcr	p15, 0, r6, c1, c0, 0

	/* Flush the BTAC. */
	ldr	r6, =0x0
	mcr	p15, 0, r6, c7, c1, 6

	ldr	r6, =imx7_iram_tlb_phys_addr
	ldr	r6, [r6]
	dsb
	isb

	/* Store the IRAM table in TTBR1 */
	mcr	p15, 0, r6, c2, c0, 1
	/* Read TTBCR and set PD0=1, N = 1 */
	mrc	p15, 0, r6, c2, c0, 2
	orr	r6, r6, #0x11
	mcr	p15, 0, r6, c2, c0, 2

	dsb
	isb

	/* flush the TLB */
	ldr	r6, =0x0
	mcr	p15, 0, r6, c8, c3, 0

	/* Disable L1 data cache. */
	mrc	p15, 0, r6, c1, c0, 0
	bic	r6, r6, #0x4
	mcr	p15, 0, r6, c1, c0, 0

	dsb
	isb

	.endm

	.macro restore_ttbr1

	/* Enable L1 data cache. */
	mrc	p15, 0, r6, c1, c0, 0
	orr	r6, r6, #0x4
	mcr	p15, 0, r6, c1, c0, 0

	dsb
	isb

	/* Restore TTBCR */
	/* Read TTBCR and set PD0=0, N = 0 */
	mrc	p15, 0, r6, c2, c0, 2
	bic	r6, r6, #0x11
	mcr	p15, 0, r6, c2, c0, 2
	dsb
	isb

	/* flush the TLB */
	ldr	r6, =0x0
	mcr	p15, 0, r6, c8, c3, 0

	/* Enable Branch Prediction, Z bit in SCTLR. */
	mrc	p15, 0, r6, c1, c0, 0
	orr	r6, r6, #0x800
	mcr	p15, 0, r6, c1, c0, 0

	/* Flush the Branch Target Address Cache (BTAC) */
	ldr	r6, =0x0
	mcr	p15, 0, r6, c7, c1, 6

	/* Restore TTBR1, get the origin ttbr1 from pm info */
	ldr	r7, [r0, #PM_INFO_MX7_TTBR1_V_OFFSET]
	mcr	p15, 0, r7, c2, c0, 1

	.endm

	/* r11 must be DDRC base address */
	.macro ddrc_enter_self_refresh

	/* let DDR out of self-refresh */
	ldr	r7, =0x0
	str	r7, [r11, #DDRC_PWRCTL]

	/* wait rw port_busy clear */
	ldr	r6, =(0x1 << 16)
	orr	r6, r6, #0x1
1:
	ldr	r7, [r11, #DDRC_PSTAT]
	ands	r7, r7, r6
	bne	1b

	/* enter self-refresh bit 5 */
	ldr	r7, =(0x1 << 5)
	str	r7, [r11, #DDRC_PWRCTL]

	/* wait until self-refresh mode entered */
2:
	ldr	r7, [r11, #DDRC_STAT]
	and	r7, r7, #0x3
	cmp	r7, #0x3
	bne	2b
3:
	ldr	r7, [r11, #DDRC_STAT]
	ands	r7, r7, #0x20
	beq	3b

	/* disable dram clk */
	ldr	r7, [r11, #DDRC_PWRCTL]
	orr	r7, r7, #(1 << 3)
	str	r7, [r11, #DDRC_PWRCTL]

	.endm

	/* r11 must be DDRC base address */
	.macro ddrc_exit_self_refresh

	/* let DDR out of self-refresh */
	ldr	r7, =0x0
	str	r7, [r11, #DDRC_PWRCTL]

	/* wait until self-refresh mode entered */
4:
	ldr	r7, [r11, #DDRC_STAT]
	and	r7, r7, #0x3
	cmp	r7, #0x3
	beq	4b

	/* enable auto self-refresh */
	ldr	r7, [r11, #DDRC_PWRCTL]
	orr	r7, r7, #(1 << 0)
	str	r7, [r11, #DDRC_PWRCTL]

	.endm

ENTRY(imx7_suspend)
	push	{r4-r12}
	/*
	 * The value of r0 is mapped the same in origin table and IRAM table,
	 * thus no need to care r0 here.
	 */
	ldr	r1, [r0, #PM_INFO_PBASE_OFFSET]
	ldr	r2, [r0, #PM_INFO_RESUME_ADDR_OFFSET]
	ldr	r3, [r0, #PM_INFO_DDR_TYPE_OFFSET]
	ldr	r4, [r0, #PM_INFO_PM_INFO_SIZE_OFFSET]

	/*
	 * counting the resume address in iram
	 * to set it in SRC register.
	 */
	ldr	r6, =imx7_suspend
	ldr	r7, =resume
	sub	r7, r7, r6
	add	r8, r1, r4
	add	r9, r8, r7

	ldr	r11, [r0, #PM_INFO_MX7_SRC_V_OFFSET]
	/* store physical resume addr and pm_info address. */
	str	r9, [r11, #MX7_SRC_GPR1]
	str	r1, [r11, #MX7_SRC_GPR2]

	store_ttbr1

	ldr	r11, [r0, #PM_INFO_MX7_DDRC_V_OFFSET]
	ddrc_enter_self_refresh

	/* Zzz, enter stop mode */
	wfi
	nop
	nop
	nop
	nop

	ddrc_exit_self_refresh
	restore_ttbr1

	pop	{r4-r12}
	/* return to suspend finish */
	mov	pc, lr

resume:
	/* invalidate L1 I-cache first */
	mov     r6, #0x0
	mcr     p15, 0, r6, c7, c5, 0
	mcr     p15, 0, r6, c7, c5, 6
	/* enable the Icache and branch prediction */
	mov     r6, #0x1800
	mcr     p15, 0, r6, c1, c0, 0
	isb

	/* get physical resume address from pm_info. */
	ldr	lr, [r0, #PM_INFO_RESUME_ADDR_OFFSET]
	/* clear core0's entry and parameter */
	ldr	r11, [r0, #PM_INFO_MX7_SRC_P_OFFSET]
	mov	r7, #0x0
	str	r7, [r11, #MX7_SRC_GPR1]
	str	r7, [r11, #MX7_SRC_GPR2]

	ldr	r11, [r0, #PM_INFO_MX7_DDRC_P_OFFSET]
	ddrc_exit_self_refresh

	restore_ttbr1

	mov	pc, lr
ENDPROC(imx7_suspend)

ENTRY(ca7_cpu_resume)
	bl	v7_invalidate_l1
	b	cpu_resume
ENDPROC(ca7_cpu_resume)
