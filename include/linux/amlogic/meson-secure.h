/*
 * include/linux/amlogic/meson-secure.h
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

#ifndef _MESON_SECURE_H_
#define _MESON_SECURE_H_

/* Meson Secure Monitor/HAL APIs */
#define CALL_TRUSTZONE_API                      0x1
#define CALL_TRUSTZONE_MON                      0x4
#define CALL_TRUSTZONE_HAL_API                  0x5

/* Secure Monitor mode APIs */
#define TRUSTZONE_MON_TYPE_MASK                 0xF00
#define TRUSTZONE_MON_FUNC_MASK                 0x0FF
#define TRUSTZONE_MON_L2X0                      0x100
#define TRUSTZONE_MON_L2X0_CTRL_INDEX           0x101
#define TRUSTZONE_MON_L2X0_AUXCTRL_INDEX        0x102
#define TRUSTZONE_MON_L2X0_PREFETCH_INDEX       0x103
#define TRUSTZONE_MON_L2X0_TAGLATENCY_INDEX     0x104
#define TRUSTZONE_MON_L2X0_DATALATENCY_INDEX    0x105
#define TRUSTZONE_MON_L2X0_FILTERSTART_INDEX    0x106
#define TRUSTZONE_MON_L2X0_FILTEREND_INDEX      0x107
#define TRUSTZONE_MON_L2X0_DEBUG_INDEX          0x108
#define TRUSTZONE_MON_L2X0_POWER_INDEX          0x109

#define TRUSTZONE_MON_CORE                      0x200
#define TRUSTZONE_MON_CORE_RD_CTRL_INDEX        0x201
#define TRUSTZONE_MON_CORE_WR_CTRL_INDEX        0x202
#define TRUSTZONE_MON_CORE_RD_STATUS0_INDEX     0x203
#define TRUSTZONE_MON_CORE_WR_STATUS0_INDEX     0x204
#define TRUSTZONE_MON_CORE_RD_STATUS1_INDEX     0x205
#define TRUSTZONE_MON_CORE_WR_STATUS1_INDEX     0x206
#define TRUSTZONE_MON_CORE_BOOTADDR_INDEX       0x207
#define TRUSTZONE_MON_CORE_DDR_INDEX            0x208
#define TRUSTZONE_MON_CORE_RD_SOC_REV1          0x209
#define TRUSTZONE_MON_CORE_RD_SOC_REV2          0x20A
#define TRUSTZONE_MON_CORE_OFF                  0x20B

#define TRUSTZONE_MON_SUSPNED_FIRMWARE          0x300
#define TRUSTZONE_MON_SAVE_CPU_GIC              0x400

#define TRUSTZONE_MON_RTC                       0x500
#define TRUSTZONE_MON_RTC_RD_REG_INDEX          0x501
#define TRUSTZONE_MON_RTC_WR_REG_INDEX          0x502

#define TRUSTZONE_MON_REG                       0x600
#define TRUSTZONE_MON_REG_RD_INDEX              0x601
#define TRUSTZONE_MON_REG_WR_INDEX              0x602

#define TRUSTZONE_MON_MEM                       0x700
#define TRUSTZONE_MON_MEM_BASE                  0x701
#define TRUSTZONE_MON_MEM_TOTAL_SIZE            0x702
#define TRUSTZONE_MON_MEM_FLASH                 0x703
#define TRUSTZONE_MON_MEM_FLASH_SIZE            0x704
#define TRUSTZONE_MON_MEM_GE2D                  0x705

#define TRUSTZONE_MON_JTAG                      0x800
#define TRUSTZONE_MON_JTAG_DISABLE              0x801
#define TRUSTZONE_MON_JTAG_APAO                 0x802
#define TRUSTZONE_MON_JTAG_APEE                 0x803

/* Secure HAL APIs*/
#define TRUSTZONE_HAL_API_EFUSE                 0x100
#define TRUSTZONE_HAL_API_STORAGE               0x200
#define TRUSTZONE_HAL_API_MEMCONFIG             0x300
#define TRUSTZONE_HAL_API_MEMCONFIG_GE2D        0x301

#ifndef __ASSEMBLER__
extern int meson_smc1(uint32_t fn, uint32_t arg);
extern int meson_smc_hal_api(uint32_t cmdidx, uint32_t arg);
extern int meson_smc2(uint32_t arg);
extern int meson_smc3(uint32_t arg1, uint32_t arg2);
extern bool meson_secure_enabled(void);
extern uint32_t meson_read_corectrl(void);
extern uint32_t meson_modify_corectrl(uint32_t arg);
extern uint32_t meson_read_corestatus(uint32_t cpu);
extern uint32_t meson_modify_corestatus(uint32_t cpu, uint32_t arg);
extern void meson_auxcoreboot_addr(uint32_t cpu, uint32_t arg);
extern void meson_suspend_firmware(void);
extern uint32_t meson_secure_reg_read(uint32_t addr);
extern uint32_t meson_secure_reg_write(uint32_t addr, uint32_t val);
extern uint32_t meson_read_socrev1(void);
extern uint32_t meson_read_socrev2(void);
extern uint32_t meson_secure_mem_base_start(void);
extern uint32_t meson_secure_mem_total_size(void);
extern uint32_t meson_secure_mem_flash_start(void);
extern uint32_t meson_secure_mem_flash_size(void);
extern int32_t meson_secure_mem_ge2d_access(uint32_t msec);

extern uint32_t meson_secure_jtag_disable(void);
extern uint32_t meson_secure_jtag_apao(void);
extern uint32_t meson_secure_jtag_apee(void);

extern int meson_trustzone_efuse(void *arg);

#endif /* __ASSEMBLER__ */

#endif /* _MESON_SECURE_H_ */
