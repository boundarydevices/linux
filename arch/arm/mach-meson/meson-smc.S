/*
 * Copyright (C) 2014-2017 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/linkage.h>
#include <linux/amlogic/meson-secure.h>

.arch_extension sec

ENTRY(meson_smc1)
	stmfd sp!, {r2-r12, lr}
	mov r2, r1
	mov r1, r0
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_smc1)

ENTRY(meson_smc_hal_api)
	stmfd sp!, {r3-r12, lr}
	mov r2, r1
	mov r1, r0
	ldr r0, =CALL_TRUSTZONE_HAL_API
	dsb
	smc #0
	ldmfd sp!, {r3-r12, pc}
ENDPROC(meson_smc_hal_api)

ENTRY(meson_smc2)
	stmfd sp!, {r2-r12, lr}
	mov r1, r0
	ldr r0, =0x2
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_smc2)

ENTRY(meson_smc3)
	stmfd sp!, {r2-r12, lr}
	mov r2, r1
	mov r1, r0
	ldr r0, =0x3
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_smc3)

ENTRY(meson_read_corectrl)
	stmfd sp!, {r2-r12, lr}
	ldr r1, =TRUSTZONE_MON_CORE_RD_CTRL_INDEX
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_read_corectrl)

ENTRY(meson_modify_corectrl)
	stmfd sp!, {r2-r12, lr}
	mov r2, r0
	ldr r1, =TRUSTZONE_MON_CORE_WR_CTRL_INDEX
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_modify_corectrl)

ENTRY(meson_read_corestatus)
	stmfd sp!, {r2-r12, lr}
	ldr r1, =TRUSTZONE_MON_CORE_RD_STATUS0_INDEX
	add r1, r1, r0, lsl #1
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_read_corestatus)

ENTRY(meson_modify_corestatus)
	stmfd sp!, {r2-r12, lr}
	mov r2, r1
	ldr r1, =TRUSTZONE_MON_CORE_WR_STATUS0_INDEX
	add r1, r1, r0, lsl #1
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_modify_corestatus)

ENTRY(meson_auxcoreboot_addr)
	stmfd sp!, {r2-r12, lr}
	mov r3, r1
	mov r2, r0
	ldr r1, =TRUSTZONE_MON_CORE_BOOTADDR_INDEX
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_auxcoreboot_addr)

ENTRY(meson_suspend_firmware)
	stmfd sp!, {r2-r12, lr}
	mov r2, r0
	ldr r1, =TRUSTZONE_MON_SUSPNED_FIRMWARE
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_suspend_firmware)

ENTRY(meson_read_socrev1)
	stmfd sp!, {r2-r12, lr}
	ldr r1, =TRUSTZONE_MON_CORE_RD_SOC_REV1
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_read_socrev1)

ENTRY(meson_read_socrev2)
	stmfd sp!, {r2-r12, lr}
	ldr r1, =TRUSTZONE_MON_CORE_RD_SOC_REV2
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_read_socrev2)
