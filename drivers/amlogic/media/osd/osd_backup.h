/*
 * drivers/amlogic/media/osd/osd_backup.h
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

#ifndef _OSD_BACKUP_H_
#define _OSD_BACKUP_H_

#include "osd_reg.h"

#define OSD_REG_BACKUP_COUNT 12
#define OSD_AFBC_REG_BACKUP_COUNT 9
#define MALI_AFBC_REG_BACKUP_COUNT 41
#define OSD_VALUE_COUNT (VIU_OSD1_CTRL_STAT2 - VIU_OSD1_CTRL_STAT + 1)
#define OSD_AFBC_VALUE_COUNT (OSD1_AFBCD_PIXEL_VSCOPE - OSD1_AFBCD_ENABLE + 1)
#define MALI_AFBC_VALUE_COUNT \
	(VPU_MAFBC_PREFETCH_CFG_S2 - VPU_MAFBC_IRQ_MASK + 1)

extern const u16 osd_reg_backup[OSD_REG_BACKUP_COUNT];
extern const u16 osd_afbc_reg_backup[OSD_AFBC_REG_BACKUP_COUNT];
extern const u16 mali_afbc_reg_backup[MALI_AFBC_REG_BACKUP_COUNT];
extern u32 osd_backup[OSD_VALUE_COUNT];
extern u32 osd_afbc_backup[OSD_AFBC_VALUE_COUNT];
extern u32 mali_afbc_backup[MALI_AFBC_VALUE_COUNT];

enum hw_reset_flag_e {
	HW_RESET_NONE = 0,
	HW_RESET_AFBCD_REGS = 0x80000000,
	HW_RESET_OSD1_REGS = 0x00000001,
	HW_RESET_OSD2_REGS = 0x00000002,
	HW_RESET_OSD3_REGS = 0x00000004,
	HW_RESET_AFBCD_HARDWARE = 0x80000000,
	HW_RESET_MALI_AFBCD_REGS = 0x280000,
};

struct reg_item {
	u32 addr;
	u32 val;
	u32 mask;
	int recovery;
};

struct reg_recovery_table {
	u32 base_addr;
	u32 size;
	struct reg_item *table;
};

extern void update_backup_reg(u32 addr, u32 value);
extern s32 get_backup_reg(u32 addr, u32 *value);
extern void backup_regs_init(u32 backup_mask);
extern u32 is_backup(void);

extern void recovery_regs_init(void);
extern int update_recovery_item(u32 addr, u32 value);
extern s32 get_recovery_item(u32 addr, u32 *value, u32 *mask);
#endif

