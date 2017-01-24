/*
 * drivers/amlogic/media/osd/osd_backup.c
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

/* Linux Headers */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>

/* Local Headers */
#include "osd_io.h"
#include "osd_backup.h"

const u16 osd_reg_backup[OSD_REG_BACKUP_COUNT] = {
	0x1a10, 0x1a13,
	0x1a17, 0x1a18, 0x1a19, 0x1a1a, 0x1a1b, 0x1a1c, 0x1a1d, 0x1a1e,
	0x1a2b, 0x1a2d
};

const u16 osd_afbc_reg_backup[OSD_AFBC_REG_BACKUP_COUNT] = {
	0x31aa, 0x31a9,
	0x31a6, 0x31a5, 0x31a4, 0x31a3, 0x31a2, 0x31a1, 0x31a0
};

static u32 osd_backup_count = OSD_VALUE_COUNT;
static u32 afbc_backup_count = OSD_AFBC_VALUE_COUNT;
u32 osd_backup[OSD_VALUE_COUNT];
u32 osd_afbc_backup[OSD_AFBC_VALUE_COUNT];
module_param_array(osd_backup, uint, &osd_backup_count, 0444);
MODULE_PARM_DESC(osd_backup, "\n osd register backup\n");
module_param_array(osd_afbc_backup, uint, &afbc_backup_count, 0444);
MODULE_PARM_DESC(osd_afbc_backup, "\n osd afbc register backup\n");

/* 0: not backup */
static u32 backup_enable;
module_param(backup_enable, uint, 0444);
void update_backup_reg(u32 addr, u32 value)
{
	u32 base = OSD1_AFBCD_ENABLE;

	if (!backup_enable)
		return;
	if ((addr >= OSD1_AFBCD_ENABLE) &&
		(addr <= OSD1_AFBCD_PIXEL_VSCOPE) &&
		(backup_enable & HW_RESET_AFBCD_REGS)) {
		osd_afbc_backup[addr - base] = value;
	}
	base = VIU_OSD1_CTRL_STAT;
	if ((addr >= VIU_OSD1_CTRL_STAT) &&
		(addr <= VIU_OSD1_CTRL_STAT2) &&
		(backup_enable & HW_RESET_OSD1_REGS)) {
		osd_backup[addr - base] = value;
	}
}

s32 get_backup_reg(u32 addr, u32 *value)
{
	int i;
	u32 base = OSD1_AFBCD_ENABLE;

	if (!backup_enable || !value)
		return -1;

	if ((addr >= OSD1_AFBCD_ENABLE) &&
		(addr <= OSD1_AFBCD_PIXEL_VSCOPE) &&
		(backup_enable & HW_RESET_AFBCD_REGS)) {
		for (i = 0; i < OSD_AFBC_REG_BACKUP_COUNT; i++)
			if (addr == osd_afbc_reg_backup[i]) {
				*value = osd_afbc_backup[addr - base];
				return 0;
			}
	}
	base = VIU_OSD1_CTRL_STAT;
	if ((addr >= VIU_OSD1_CTRL_STAT) &&
		(addr <= VIU_OSD1_CTRL_STAT2) &&
		(backup_enable & HW_RESET_OSD1_REGS)) {
		for (i = 0; i < OSD_REG_BACKUP_COUNT; i++)
			if (addr == osd_reg_backup[i]) {
				*value = osd_backup[addr - base];
				return 0;
			}
	}
	return -1;
}

void backup_regs_init(u32 backup_mask)
{
	int i = 0;
	u32 addr;
	u32 base = VIU_OSD1_CTRL_STAT;

	if (backup_enable)
		return;

	while ((backup_mask & HW_RESET_OSD1_REGS)
		&& (i < OSD_REG_BACKUP_COUNT)) {
		addr = osd_reg_backup[i];
		osd_backup[addr - base] =
			osd_reg_read(addr);
		i++;
	}
	i = 0;
	base = OSD1_AFBCD_ENABLE;
	while ((backup_mask & HW_RESET_AFBCD_REGS)
		&& (i < OSD_AFBC_REG_BACKUP_COUNT)) {
		addr = osd_afbc_reg_backup[i];
		osd_afbc_backup[addr - base] =
			osd_reg_read(addr);
		i++;
	}
	backup_enable = backup_mask;
}

u32 is_backup(void)
{
	return backup_enable;
}

#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
/* recovery section */
#define INVAILD_REG_ITEM {0xffff, 0x0, 0x0, 0x0}
#define REG_RECOVERY_TABLE 5

static struct reg_recovery_table gRecovery[REG_RECOVERY_TABLE];
static u32 recovery_enable;

static struct reg_item osd1_recovery_table[] = {
	{VIU_OSD1_CTRL_STAT, 0x0, 0x401ff9f1, 1},
	INVAILD_REG_ITEM, /* VIU_OSD1_COLOR_ADDR 0x1a11 */
	INVAILD_REG_ITEM, /* VIU_OSD1_COLOR 0x1a12 */
	{VIU_OSD1_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W4 0x1a14 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W4 0x1a15 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W4 0x1a16 */
	{VIU_OSD1_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{VIU_OSD1_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{VIU_OSD1_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{VIU_OSD1_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{VIU_OSD1_BLK0_CFG_W0, 0x0, 0x30ffffff, 1},
	{VIU_OSD1_BLK0_CFG_W1, 0x0, 0x1fff1fff, 1},
	{VIU_OSD1_BLK0_CFG_W2, 0x0, 0x1fff1fff, 1},
	{VIU_OSD1_BLK0_CFG_W3, 0x0, 0x0fff0fff, 1},
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W0 0x1a1f */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W1 0x1a20 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W2 0x1a21 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W3 0x1a22 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W0 0x1a23 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W1 0x1a24 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W2 0x1a25 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W3 0x1a26 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W0 0x1a27 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W1 0x1a28 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W2 0x1a29 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W3 0x1a2a */
	{VIU_OSD1_FIFO_CTRL_STAT, 0x0, 0xffc3ffff, 1},
	INVAILD_REG_ITEM, /* VIU_OSD1_TEST_RDDATA 0x1a2c */
	{VIU_OSD1_CTRL_STAT2, 0x0, 0x0000ffff, 1},
};

static struct reg_item osd_afbcd_recovery_table[] = {
	{OSD1_AFBCD_ENABLE, 0x0, 0x0000ff01, 1},
	{OSD1_AFBCD_MODE, 0x0, 0x937f007f, 1},
	{OSD1_AFBCD_SIZE_IN, 0x0, 0xffffffff, 1},
	{OSD1_AFBCD_HDR_PTR, 0x0, 0xffffffff, 1},
	{OSD1_AFBCD_FRAME_PTR, 0x0, 0xffffffff, 1},
	{OSD1_AFBCD_CHROMA_PTR, 0x0, 0xffffffff, 1},
	{OSD1_AFBCD_CONV_CTRL, 0x0, 0x0000ffff, 1},
	INVAILD_REG_ITEM, /* unused 0x31a7 */
	INVAILD_REG_ITEM, /* OSD1_AFBCD_STATUS 0x31a8 */
	{OSD1_AFBCD_PIXEL_HSCOPE, 0x0, 0xffffffff, 1},
	{OSD1_AFBCD_PIXEL_VSCOPE, 0x0, 0xffffffff, 1}
};

static struct reg_item freescale_recovery_table[] = {
	{VPP_OSD_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{VPP_OSD_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{VPP_OSD_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{VPP_OSD_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{VPP_OSD_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{VPP_OSD_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{VPP_OSD_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{VPP_OSD_SC_DUMMY_DATA, 0x0, 0xffffffff, 0},
	{VPP_OSD_SC_CTRL0, 0x0, 0x00007ffb, 1},
	{VPP_OSD_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{VPP_OSD_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{VPP_OSD_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	{VPP_OSD_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{VPP_OSD_SCALE_COEF, 0x0, 0xffffffff, 0}
};

static struct reg_item osd2_recovery_table[] = {
	{VIU_OSD2_CTRL_STAT, 0x0, 0x401ff9f1, 1},
	INVAILD_REG_ITEM, /* VIU_OSD2_COLOR_ADDR 0x1a31 */
	INVAILD_REG_ITEM, /* VIU_OSD2_COLOR 0x1a32 */
	INVAILD_REG_ITEM, /* VIU_OSD2_HL1_H_START_END 0x1a33 */
	INVAILD_REG_ITEM, /* VIU_OSD2_HL1_V_START_END 0x1a34 */
	INVAILD_REG_ITEM, /* VIU_OSD2_HL2_H_START_END 0x1a35 */
	INVAILD_REG_ITEM, /* VIU_OSD2_HL2_V_START_END 0x1a36 */
	{VIU_OSD2_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{VIU_OSD2_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{VIU_OSD2_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{VIU_OSD2_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{VIU_OSD2_BLK0_CFG_W0, 0x0, 0x00ffffff, 1},
	{VIU_OSD2_BLK0_CFG_W1, 0x0, 0x1fff1fff, 1},
	{VIU_OSD2_BLK0_CFG_W2, 0x0, 0x1fff1fff, 1},
	{VIU_OSD2_BLK0_CFG_W3, 0x0, 0x0fff0fff, 1},
	{VIU_OSD2_BLK1_CFG_W0, 0x0, 0x00ffffff, 0},
	{VIU_OSD2_BLK1_CFG_W1, 0x0, 0x1fff1fff, 0},
	{VIU_OSD2_BLK1_CFG_W2, 0x0, 0x1fff1fff, 0},
	{VIU_OSD2_BLK1_CFG_W3, 0x0, 0x0fff0fff, 0},
	{VIU_OSD2_BLK2_CFG_W0, 0x0, 0x00ffffff, 0},
	{VIU_OSD2_BLK2_CFG_W1, 0x0, 0x1fff1fff, 0},
	{VIU_OSD2_BLK2_CFG_W2, 0x0, 0x1fff1fff, 0},
	{VIU_OSD2_BLK2_CFG_W3, 0x0, 0x0fff0fff, 0},
	{VIU_OSD2_BLK3_CFG_W0, 0x0, 0x00ffffff, 0},
	{VIU_OSD2_BLK3_CFG_W1, 0x0, 0x1fff1fff, 0},
	{VIU_OSD2_BLK3_CFG_W2, 0x0, 0x1fff1fff, 0},
	{VIU_OSD2_BLK3_CFG_W3, 0x0, 0x0fff0fff, 0},
	{VIU_OSD2_FIFO_CTRL_STAT, 0x0, 0xffc3ffff, 1},
	INVAILD_REG_ITEM, /* VIU_OSD2_TEST_RDDATA 0x1a4c */
	{VIU_OSD2_CTRL_STAT2, 0x0, 0x00007ffd, 1}
};


static struct reg_item misc_recovery_table[] = {
	{VIU_OSD2_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{VIU_OSD2_BLK1_CFG_W4, 0x0, 0xffffffff, 0},
	{VIU_OSD2_BLK2_CFG_W4, 0x0, 0xffffffff, 0},
	{VIU_OSD2_BLK3_CFG_W4, 0x0, 0xffffffff, 0},
	{VPU_RDARB_MODE_L1C2, 0x0, 0x00010000, 1},
	{VIU_MISC_CTRL1, 0x0, 0x0000ff00, 1}
};

void recovery_regs_init(void)
{
	int i = 0;

	if (recovery_enable)
		return;
	memset(gRecovery, 0, sizeof(gRecovery));
	gRecovery[i].base_addr = VIU_OSD1_CTRL_STAT;
	gRecovery[i].size = sizeof(osd1_recovery_table)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd1_recovery_table[0];

	i++;
	gRecovery[i].base_addr = OSD1_AFBCD_ENABLE;
	gRecovery[i].size = sizeof(osd_afbcd_recovery_table)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd_afbcd_recovery_table[0];

	i++;
	gRecovery[i].base_addr = VPP_OSD_VSC_PHASE_STEP;
	gRecovery[i].size = sizeof(freescale_recovery_table)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&freescale_recovery_table[0];

	i++;
	gRecovery[i].base_addr = VIU_OSD2_CTRL_STAT;
	gRecovery[i].size = sizeof(osd2_recovery_table)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd2_recovery_table[0];

	i++;
	gRecovery[i].base_addr = 0xffffffff; /* not base addr */
	gRecovery[i].size = sizeof(misc_recovery_table)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&misc_recovery_table[0];
	recovery_enable = 1;
}

int update_recovery_item(u32 addr, u32 value)
{
	u32 base, size;
	int i;
	struct reg_item *table = NULL;
	int ret = -1;

	if (!recovery_enable)
		return ret;

	base = addr & 0xfff0;
	switch (base) {
	case VIU_OSD1_CTRL_STAT:
	case VIU_OSD1_BLK1_CFG_W1:
		/* osd1 */
		if (backup_enable &
			HW_RESET_OSD1_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VIU_OSD2_CTRL_STAT:
	case VIU_OSD2_BLK1_CFG_W1:
		/* osd2 */
		base = gRecovery[3].base_addr;
		size = gRecovery[3].size;
		table = gRecovery[3].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case OSD1_AFBCD_ENABLE:
		/* osd1 afbcd */
		if (backup_enable &
			HW_RESET_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[1].base_addr;
		size = gRecovery[1].size;
		table = gRecovery[1].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPP_OSD_VSC_PHASE_STEP:
		base = gRecovery[2].base_addr;
		size = gRecovery[2].size;
		table = gRecovery[2].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	default:
		break;
	}
	if ((addr == VIU_OSD2_BLK0_CFG_W4) ||
		(addr == VIU_OSD2_BLK1_CFG_W4) ||
		(addr == VIU_OSD2_BLK2_CFG_W4) ||
		(addr == VIU_OSD2_BLK3_CFG_W4) ||
		(addr == VPU_RDARB_MODE_L1C2) ||
		(addr == VIU_MISC_CTRL1)) {
		table = gRecovery[4].table;
		for (i = 0; i <  gRecovery[4].size; i++) {
			if (addr == table[i].addr) {
				table[i].val = value;
				if (table[i].recovery)
					table[i].recovery = 1;
				ret = 0;
				break;
			}
		}
	}
	return ret;
}

s32 get_recovery_item(u32 addr, u32 *value, u32 *mask)
{
	u32 base, size;
	int i;
	struct reg_item *table = NULL;
	int ret = -1;

	if (!recovery_enable)
		return ret;

	base = addr & 0xfff0;
	switch (base) {
	case VIU_OSD1_CTRL_STAT:
	case VIU_OSD1_BLK1_CFG_W1:
		/* osd1 */
		if (backup_enable &
			HW_RESET_OSD1_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VIU_OSD2_CTRL_STAT:
	case VIU_OSD2_BLK1_CFG_W1:
		/* osd2 */
		base = gRecovery[3].base_addr;
		size = gRecovery[3].size;
		table = gRecovery[3].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case OSD1_AFBCD_ENABLE:
		/* osd1 afbcd */
		if (backup_enable &
			HW_RESET_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[1].base_addr;
		size = gRecovery[1].size;
		table = gRecovery[1].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPP_OSD_VSC_PHASE_STEP:
		base = gRecovery[2].base_addr;
		size = gRecovery[2].size;
		table = gRecovery[2].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	default:
		break;
	}

	if ((addr == VIU_OSD2_BLK0_CFG_W4) ||
		(addr == VIU_OSD2_BLK1_CFG_W4) ||
		(addr == VIU_OSD2_BLK2_CFG_W4) ||
		(addr == VIU_OSD2_BLK3_CFG_W4) ||
		(addr == VPU_RDARB_MODE_L1C2) ||
		(addr == VIU_MISC_CTRL1)) {
		table = gRecovery[4].table;
		for (i = 0; i <  gRecovery[4].size; i++) {
			if (addr == table[i].addr) {
				table += i;
				ret = 0;
				break;
			}
		}
	}
	if (ret == 0 && table) {
		if (table->recovery == 1) {
			u32 regmask = table->mask;
			u32 real_value = osd_reg_read(addr);

			if ((real_value & regmask)
				== (table->val & regmask)) {
				ret = 1;
				*mask = regmask;
				*value = real_value;
			} else {
				*mask = regmask;
				*value = real_value & ~(regmask);
				*value |= (table->val & regmask);
			}
			table->recovery = 2;
		} else if (table->recovery == 2)
			ret = 1;
		else
			ret = -1;
	}
	/* ret = 1, 2 need not recovery,
	 *	ret = 0 need recovery,
	 *	ret = -1, not find
	 */
	return ret;
}
#else
void recovery_regs_init(void)
{
}

int update_recovery_item(u32 addr, u32 value)
{
	addr = 0;
	value = 0;
	return 0;
}
s32 get_recovery_item(u32 addr, u32 *value, u32 *mask)
{
	addr = 0;
	value = NULL;
	mask = NULL;
	return 0;
}
#endif
