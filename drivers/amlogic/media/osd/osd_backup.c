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
#include "osd_hw.h"

const u16 osd_reg_backup[OSD_REG_BACKUP_COUNT] = {
	0x1a10, 0x1a13,
	0x1a17, 0x1a18, 0x1a19, 0x1a1a, 0x1a1b, 0x1a1c, 0x1a1d, 0x1a1e,
	0x1a2b, 0x1a2d
};

const u16 osd_afbc_reg_backup[OSD_AFBC_REG_BACKUP_COUNT] = {
	0x31aa, 0x31a9,
	0x31a6, 0x31a5, 0x31a4, 0x31a3, 0x31a2, 0x31a1, 0x31a0
};

const u16 mali_afbc_reg_backup[MALI_AFBC_REG_BACKUP_COUNT] = {
	0x3a03, 0x3a07,
	0x3a10, 0x3a11, 0x3a12, 0x3a13, 0x3a14, 0x3a15, 0x3a16,
	0x3a17, 0x3a18, 0x3a19, 0x3a1a, 0x3a1b, 0x3a1c,
	0x3a30, 0x3a31, 0x3a32, 0x3a33, 0x3a34, 0x3a35, 0x3a36,
	0x3a37, 0x3a38, 0x3a39, 0x3a3a, 0x3a3b, 0x3a3c,
	0x3a50, 0x3a51, 0x3a52, 0x3a53, 0x3a54, 0x3a55, 0x3a56,
	0x3a57, 0x3a58, 0x3a59, 0x3a5a, 0x3a5b, 0x3a5c
};
static u32 osd_backup_count = OSD_VALUE_COUNT;
static u32 afbc_backup_count = OSD_AFBC_VALUE_COUNT;
static u32 mali_afbc_backup_count = MALI_AFBC_REG_BACKUP_COUNT;
u32 osd_backup[OSD_VALUE_COUNT];
u32 osd_afbc_backup[OSD_AFBC_VALUE_COUNT];
u32 mali_afbc_backup[MALI_AFBC_VALUE_COUNT];
module_param_array(osd_backup, uint, &osd_backup_count, 0444);
MODULE_PARM_DESC(osd_backup, "\n osd register backup\n");
module_param_array(osd_afbc_backup, uint, &afbc_backup_count, 0444);
MODULE_PARM_DESC(osd_afbc_backup, "\n osd afbc register backup\n");
module_param_array(mali_afbc_backup, uint, &mali_afbc_backup_count, 0444);
MODULE_PARM_DESC(mali_afbc_backup, "\n mali afbc register backup\n");

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
		return;
	}
	base = VIU_OSD1_CTRL_STAT;
	if ((addr >= VIU_OSD1_CTRL_STAT) &&
		(addr <= VIU_OSD1_CTRL_STAT2) &&
		(backup_enable & HW_RESET_OSD1_REGS)) {
		osd_backup[addr - base] = value;
		return;
	}
	base = VPU_MAFBC_IRQ_MASK;
	if ((addr >= VPU_MAFBC_IRQ_MASK)
		&& (addr <= VPU_MAFBC_PREFETCH_CFG_S2) &&
		(backup_enable & HW_RESET_MALI_AFBCD_REGS)) {
		mali_afbc_backup[addr - base] = value;
		return;
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
	base = VPU_MAFBC_IRQ_MASK;
	if ((addr >= VPU_MAFBC_IRQ_MASK) &&
		(addr <= VPU_MAFBC_PREFETCH_CFG_S2) &&
		(backup_enable & HW_RESET_MALI_AFBCD_REGS)) {
		for (i = 0; i < MALI_AFBC_REG_BACKUP_COUNT; i++)
			if (addr == mali_afbc_reg_backup[i]) {
				*value = mali_afbc_backup[addr - base];
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
	i = 0;
	base = VPU_MAFBC_IRQ_MASK;
	while ((backup_mask & HW_RESET_MALI_AFBCD_REGS)
		&& (i < MALI_AFBC_REG_BACKUP_COUNT)) {
		addr = mali_afbc_reg_backup[i];
		mali_afbc_backup[addr - base] =
			osd_reg_read(addr);
		i++;
	}
	backup_enable = backup_mask;
}

u32 is_backup(void)
{
	return backup_enable;
}

/* recovery section */
#define INVAILD_REG_ITEM {0xffff, 0x0, 0x0, 0x0}
#define REG_RECOVERY_TABLE 12

static struct reg_recovery_table gRecovery[REG_RECOVERY_TABLE];
static u32 recovery_enable;

/* Before G12A Chip */
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
	{VIU_MISC_CTRL1, 0x0, 0x0000ff00, 1},
	{DOLBY_CORE2A_SWAP_CTRL1, 0x0, 0x0fffffff, 1},
	{DOLBY_CORE2A_SWAP_CTRL2, 0x0, 0xffffffff, 1}
};

/* After G12A Chip */
static struct reg_item osd12_recovery_table_g12a[] = {
	/* osd1 */
	{VIU_OSD1_CTRL_STAT, 0x0, 0xc01ff9f7, 1},
	INVAILD_REG_ITEM, /* VIU_OSD1_COLOR_ADDR 0x1a11 */
	INVAILD_REG_ITEM, /* VIU_OSD1_COLOR 0x1a12 */
	{VIU_OSD1_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{VIU_OSD1_BLK1_CFG_W4, 0x0, 0xffffffff, 1},
	{VIU_OSD1_BLK2_CFG_W4, 0x0, 0xffffffff, 1},
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W4 0x1a16 */
	{VIU_OSD1_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{VIU_OSD1_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{VIU_OSD1_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{VIU_OSD1_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{VIU_OSD1_BLK0_CFG_W0, 0x0, 0x70ffff7f, 1},
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
	{VIU_OSD1_FIFO_CTRL_STAT, 0x0, 0xffc7ffff, 1},
	INVAILD_REG_ITEM, /* VIU_OSD1_TEST_RDDATA 0x1a2c */
	{VIU_OSD1_CTRL_STAT2, 0x0, 0x0000ffff, 1},
	{VIU_OSD1_PROT_CTRL, 0x0, 0xffff0000, 1},
	{VIU_OSD1_MALI_UNPACK_CTRL, 0x0, 0x9f01ffff, 1},
	/* osd2 */
	{VIU_OSD2_CTRL_STAT, 0x0, 0xc01ff9f7, 1},
	INVAILD_REG_ITEM, /* VIU_OSD2_COLOR_ADDR 0x1a31 */
	INVAILD_REG_ITEM, /* VIU_OSD2_COLOR 0x1a32 */
	INVAILD_REG_ITEM, /* 0x1a33 */
	INVAILD_REG_ITEM, /* 0x1a34 */
	INVAILD_REG_ITEM, /* 0x1a35 */
	INVAILD_REG_ITEM, /* 0x1a36 */
	{VIU_OSD2_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{VIU_OSD2_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{VIU_OSD2_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{VIU_OSD2_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{VIU_OSD2_BLK0_CFG_W0, 0x0, 0x70ffff7f, 1},
	{VIU_OSD2_BLK0_CFG_W1, 0x0, 0x1fff1fff, 1},
	{VIU_OSD2_BLK0_CFG_W2, 0x0, 0x1fff1fff, 1},
	{VIU_OSD2_BLK0_CFG_W3, 0x0, 0x0fff0fff, 1},
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W0 0x1a3f */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W1 0x1a40 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W2 0x1a41 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W3 0x1a42 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W0 0x1a43 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W1 0x1a44 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W2 0x1a45 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W3 0x1a46 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W0 0x1a47 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W1 0x1a48 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W2 0x1a49 */
	INVAILD_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W3 0x1a4a */
	{VIU_OSD2_FIFO_CTRL_STAT, 0x0, 0xffc7ffff, 1},
	INVAILD_REG_ITEM, /* VIU_OSD2_TEST_RDDATA 0x1a4c */
	{VIU_OSD2_CTRL_STAT2, 0x0, 0x0000ffff, 1},
	{VIU_OSD2_PROT_CTRL, 0x0, 0xffff0000, 1},
};

static struct reg_item osd3_recovery_table_g12a[] = {
	{VIU_OSD3_CTRL_STAT, 0x0, 0xc01ff9f7, 1},
	{VIU_OSD3_CTRL_STAT2, 0x0, 0x0000ffff, 1},
	INVAILD_REG_ITEM, /* VIU_OSD3_COLOR_ADDR 0x3d82 */
	INVAILD_REG_ITEM, /* VIU_OSD3_COLOR 0x3d83 */
	{VIU_OSD3_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{VIU_OSD3_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{VIU_OSD3_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{VIU_OSD3_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{VIU_OSD3_BLK0_CFG_W0, 0x0, 0x70ffff7f, 1},
	INVAILD_REG_ITEM, /* 0x3d89 */
	INVAILD_REG_ITEM, /* 0x3d8a */
	INVAILD_REG_ITEM, /* 0x3d8b */
	{VIU_OSD3_BLK0_CFG_W1, 0x0, 0x1fff1fff, 1},
	INVAILD_REG_ITEM, /* 0x3d8d */
	INVAILD_REG_ITEM, /* 0x3d8e */
	INVAILD_REG_ITEM, /* 0x3d8f */
	{VIU_OSD3_BLK0_CFG_W2, 0x0, 0x1fff1fff, 1},
	INVAILD_REG_ITEM, /* 0x3d91 */
	INVAILD_REG_ITEM, /* 0x3d92 */
	INVAILD_REG_ITEM, /* 0x3d93 */
	{VIU_OSD3_BLK0_CFG_W3, 0x0, 0x0fff0fff, 1},
	INVAILD_REG_ITEM, /* 0x3d95 */
	INVAILD_REG_ITEM, /* 0x3d96 */
	INVAILD_REG_ITEM, /* 0x3d97 */
	{VIU_OSD3_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{VIU_OSD3_BLK1_CFG_W4, 0x0, 0xffffffff, 1},
	{VIU_OSD3_BLK2_CFG_W4, 0x0, 0xffffffff, 1},
	INVAILD_REG_ITEM, /* 0x3d9b */
	{VIU_OSD2_FIFO_CTRL_STAT, 0x0, 0xffc7ffff, 1},
	INVAILD_REG_ITEM, /* VIU_OSD3_TEST_RDDATA 0x3d9d */
	{VIU_OSD3_PROT_CTRL, 0x0, 0xffff0000, 1},
	{VIU_OSD3_MALI_UNPACK_CTRL, 0x0, 0x9f01ffff, 1},
	{VIU_OSD3_DIMM_CTRL, 0x0, 0x7fffffff, 1},
};

static struct reg_item osd1_sc_recovery_table_g12a[] = {
	{VPP_OSD_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{VPP_OSD_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{VPP_OSD_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{VPP_OSD_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{VPP_OSD_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{VPP_OSD_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{VPP_OSD_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{VPP_OSD_SC_DUMMY_DATA, 0x0, 0xffffffff, 0},
	{VPP_OSD_SC_CTRL0, 0x0, 0x0fff3ffc, 1},
	{VPP_OSD_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{VPP_OSD_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{VPP_OSD_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	{VPP_OSD_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{VPP_OSD_SCALE_COEF, 0x0, 0xffffffff, 0}
};

static struct reg_item osd2_sc_recovery_table_g12a[] = {
	{OSD2_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD2_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD2_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{OSD2_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD2_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD2_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{OSD2_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{OSD2_SC_DUMMY_DATA, 0x0, 0xffffffff, 0},
	{OSD2_SC_CTRL0, 0x0, 0x0fff3ffc, 1},
	{OSD2_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{OSD2_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{OSD2_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	INVAILD_REG_ITEM, /* 0x3d0c */
	INVAILD_REG_ITEM, /* 0x3d0d */
	INVAILD_REG_ITEM, /* 0x3d0e */
	INVAILD_REG_ITEM, /* 0x3d0f */
	INVAILD_REG_ITEM, /* 0x3d10 */
	INVAILD_REG_ITEM, /* 0x3d11 */
	INVAILD_REG_ITEM, /* 0x3d12 */
	INVAILD_REG_ITEM, /* 0x3d13 */
	INVAILD_REG_ITEM, /* 0x3d14 */
	INVAILD_REG_ITEM, /* 0x3d15 */
	INVAILD_REG_ITEM, /* 0x3d16 */
	INVAILD_REG_ITEM, /* 0x3d17 */
	{OSD2_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{OSD2_SCALE_COEF, 0x0, 0xffffffff, 0},
};

static struct reg_item osd3_sc_recovery_table_g12a[] = {
	{OSD34_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{OSD34_SCALE_COEF, 0x0, 0xffffffff, 0},
	{OSD34_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD34_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD34_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{OSD34_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD34_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD34_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{OSD34_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{OSD34_SC_DUMMY_DATA, 0x0, 0xffffffff, 0},
	{OSD34_SC_CTRL0, 0x0, 0x0fff3ffc, 1},
	{OSD34_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{OSD34_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{OSD34_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
};

static struct reg_item vpu_afbcd_recovery_table_g12a[] = {
	{
		VPU_MAFBC_BLOCK_ID,
		0x0, 0xffffffff, 0
	},
	{
		VPU_MAFBC_IRQ_RAW_STATUS,
		0x0, 0x0000003f, 0
	},
	{
		VPU_MAFBC_IRQ_CLEAR,
		0x0, 0x0000003f, 0
	},
	{
		VPU_MAFBC_IRQ_MASK,
		0x0, 0x0000003f, 1
	},
	{
		VPU_MAFBC_IRQ_STATUS,
		0x0, 0x0000003f, 0
	},
	{
		VPU_MAFBC_COMMAND,
		0x0, 0x00000003, 0
	},
	{
		VPU_MAFBC_STATUS,
		0x0, 0x00000007, 0
	},
	{
		VPU_MAFBC_SURFACE_CFG,
		0x0, 0x0001000f, 1
	}
};

static struct reg_item osd1_afbcd_recovery_table_g12a[] = {
	{
		VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S0,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_FORMAT_SPECIFIER_S0,
		0x0, 0x000f030f, 1
	},
	{
		VPU_MAFBC_BUFFER_WIDTH_S0,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC_BUFFER_HEIGHT_S0,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_X_START_S0,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_X_END_S0,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_Y_START_S0,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_Y_END_S0,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S0,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S0,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_STRIDE_S0,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_PREFETCH_CFG_S0, 0x0, 3, 1
	}
};

static struct reg_item osd2_afbcd_recovery_table_g12a[] = {
	{
		VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S1,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_FORMAT_SPECIFIER_S1,
		0x0, 0x000f030f, 1
	},
	{
		VPU_MAFBC_BUFFER_WIDTH_S1,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC_BUFFER_HEIGHT_S1,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_X_START_S1,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_X_END_S1,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_Y_START_S1,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_Y_END_S1,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S1,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S1,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_STRIDE_S1,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_PREFETCH_CFG_S1, 0x0, 3, 1
	}
};

static struct reg_item osd3_afbcd_recovery_table_g12a[] = {
	{
		VPU_MAFBC_HEADER_BUF_ADDR_LOW_S2,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S2,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_FORMAT_SPECIFIER_S2,
		0x0, 0x000f030f, 1
	},
	{
		VPU_MAFBC_BUFFER_WIDTH_S2,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC_BUFFER_HEIGHT_S2,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_X_START_S2,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_X_END_S2,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_Y_START_S2,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_Y_END_S2,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S2,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S2,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_STRIDE_S2,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_PREFETCH_CFG_S2, 0x0, 3, 1
	}
};

static struct reg_item blend_recovery_table_g12a[] = {
	{VIU_OSD_BLEND_CTRL, 0x0, 0xffffffff, 1},
	{VIU_OSD_BLEND_DIN0_SCOPE_H, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN0_SCOPE_V, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN1_SCOPE_H, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN1_SCOPE_V, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN2_SCOPE_H, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN2_SCOPE_V, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN3_SCOPE_H, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN3_SCOPE_V, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DUMMY_DATA0, 0x0, 0x00ffffff, 1},
	{VIU_OSD_BLEND_DUMMY_ALPHA, 0x0, 0x1fffffff, 1},
	{VIU_OSD_BLEND_BLEND0_SIZE, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_BLEND1_SIZE, 0x0, 0x1fff1fff, 1},
	INVAILD_REG_ITEM, /* 0x39bd */
	INVAILD_REG_ITEM, /* 0x39be */
	INVAILD_REG_ITEM, /* 0x39bf */
	{VIU_OSD_BLEND_CTRL1, 0x0, 0x00037337, 1},
};

static struct reg_item post_blend_recovery_table_g12a[] = {
	{VPP_VD2_HDR_IN_SIZE, 0x0, 0x1fff1fff, 0},
	{VPP_OSD1_IN_SIZE, 0x0, 0x1fff1fff, 1},
	INVAILD_REG_ITEM, /* 0x1df2 */
	INVAILD_REG_ITEM, /* 0x1df3 */
	INVAILD_REG_ITEM, /* 0x1df4 */
	{VPP_OSD1_BLD_H_SCOPE, 0x0, 0x1fff1fff, 1},
	{VPP_OSD1_BLD_V_SCOPE, 0x0, 0x1fff1fff, 1},
	{VPP_OSD2_BLD_H_SCOPE, 0x0, 0x1fff1fff, 1},
	{VPP_OSD2_BLD_V_SCOPE, 0x0, 0x1fff1fff, 1},
	INVAILD_REG_ITEM, /* 0x1df9 */
	INVAILD_REG_ITEM, /* 0x1dfa */
	INVAILD_REG_ITEM, /* 0x1dfb */
	INVAILD_REG_ITEM, /* 0x1dfc */
	{OSD1_BLEND_SRC_CTRL, 0x0, 0x00110f1f, 1},
	{OSD2_BLEND_SRC_CTRL, 0x0, 0x00110f1f, 1},
};

static struct reg_item misc_recovery_table_g12a[] = {
	{DOLBY_PATH_CTRL, 0x0, 0x000000cc, 1},
	{OSD_PATH_MISC_CTRL, 0x0, 0x000000ff, 1},
	{VIU_OSD1_DIMM_CTRL, 0x0, 0x7fffffff, 1},
	{VIU_OSD2_DIMM_CTRL, 0x0, 0x7fffffff, 1},
	{VIU_OSD2_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{VIU_OSD2_BLK1_CFG_W4, 0x0, 0xffffffff, 1},
	{VIU_OSD2_BLK2_CFG_W4, 0x0, 0xffffffff, 1},
	{VIU_OSD2_MALI_UNPACK_CTRL, 0x0, 0x9f01ffff, 1},
	{DOLBY_CORE2A_SWAP_CTRL1, 0x0, 0x0fffffff, 1},
	{DOLBY_CORE2A_SWAP_CTRL2, 0x0, 0xffffffff, 1},
};

static void recovery_regs_init_old(void)
{
	int i = 0, j;
	int cpu_id = osd_hw.osd_meson_dev.cpu_id;

	gRecovery[i].base_addr = VIU_OSD1_CTRL_STAT;
	gRecovery[i].size = sizeof(osd1_recovery_table)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd1_recovery_table[0];

	if ((cpu_id == __MESON_CPU_MAJOR_ID_TXLX)
		|| (cpu_id == __MESON_CPU_MAJOR_ID_TXL)
		|| (cpu_id == __MESON_CPU_MAJOR_ID_TXHD)) {
		for (j = 0; j < gRecovery[i].size; j++) {
			if (gRecovery[i].table[j].addr ==
				VIU_OSD1_FIFO_CTRL_STAT) {
				gRecovery[i].table[j].mask = 0xffc7ffff;
				break;
			}
		}
	}
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
}

static void recovery_regs_init_g12a(void)
{
	int i = 0;

	gRecovery[i].base_addr = VIU_OSD1_CTRL_STAT;
	gRecovery[i].size = sizeof(osd12_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd12_recovery_table_g12a[0];

	if ((osd_hw.osd_meson_dev.viu1_osd_count - 1) == DEV_OSD3) {
		i++;
		gRecovery[i].base_addr = VIU_OSD3_CTRL_STAT;
		gRecovery[i].size = sizeof(osd3_recovery_table_g12a)
			/ sizeof(struct reg_item);
		gRecovery[i].table =
			(struct reg_item *)&osd3_recovery_table_g12a[0];
	}

	i++;
	gRecovery[i].base_addr = VPP_OSD_VSC_PHASE_STEP;
	gRecovery[i].size = sizeof(osd1_sc_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd1_sc_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = OSD2_VSC_PHASE_STEP;
	gRecovery[i].size = sizeof(osd2_sc_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd2_sc_recovery_table_g12a[0];

	if ((osd_hw.osd_meson_dev.viu1_osd_count - 1) == DEV_OSD3) {
		i++;
		gRecovery[i].base_addr = OSD34_SCALE_COEF_IDX;
		gRecovery[i].size = sizeof(osd3_sc_recovery_table_g12a)
			/ sizeof(struct reg_item);
		gRecovery[i].table =
			(struct reg_item *)&osd3_sc_recovery_table_g12a[0];
	}

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_BLOCK_ID;
	gRecovery[i].size = sizeof(vpu_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&vpu_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0;
	gRecovery[i].size = sizeof(osd1_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd1_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1;
	gRecovery[i].size = sizeof(osd2_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd2_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_HEADER_BUF_ADDR_LOW_S2;
	gRecovery[i].size = sizeof(osd3_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd3_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VIU_OSD_BLEND_CTRL;
	gRecovery[i].size = sizeof(blend_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&blend_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPP_VD2_HDR_IN_SIZE;
	gRecovery[i].size = sizeof(post_blend_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&post_blend_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = 0xffffffff; /* not base addr */
	gRecovery[i].size = sizeof(misc_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&misc_recovery_table_g12a[0];
}

void recovery_regs_init(void)
{
	int cpu_id = osd_hw.osd_meson_dev.cpu_id;

	if (recovery_enable)
		return;
	memset(gRecovery, 0, sizeof(gRecovery));

	if (cpu_id >= __MESON_CPU_MAJOR_ID_G12A)
		recovery_regs_init_g12a();
	else
		recovery_regs_init_old();
	recovery_enable = 1;
}

static int update_recovery_item_old(u32 addr, u32 value)
{
	u32 base, size;
	int i;
	struct reg_item *table = NULL;
	int ret = -1;
	int cpu_id = osd_hw.osd_meson_dev.cpu_id;

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
	if (((addr == DOLBY_CORE2A_SWAP_CTRL1)
		|| (addr == DOLBY_CORE2A_SWAP_CTRL2))
		&& (cpu_id != __MESON_CPU_MAJOR_ID_TXLX)
		&& (cpu_id != __MESON_CPU_MAJOR_ID_GXM))
		return ret;
	if ((addr == VIU_OSD2_BLK0_CFG_W4) ||
		(addr == VIU_OSD2_BLK1_CFG_W4) ||
		(addr == VIU_OSD2_BLK2_CFG_W4) ||
		(addr == VIU_OSD2_BLK3_CFG_W4) ||
		(addr == VPU_RDARB_MODE_L1C2) ||
		(addr == VIU_MISC_CTRL1) ||
		(addr ==
		DOLBY_CORE2A_SWAP_CTRL1) ||
		(addr ==
		DOLBY_CORE2A_SWAP_CTRL2)) {
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

static s32 get_recovery_item_old(u32 addr, u32 *value, u32 *mask)
{
	u32 base, size;
	int i;
	struct reg_item *table = NULL;
	int ret = -1;
	int cpu_id = osd_hw.osd_meson_dev.cpu_id;

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

	if (((addr == DOLBY_CORE2A_SWAP_CTRL1)
		|| (addr == DOLBY_CORE2A_SWAP_CTRL2))
		&& (cpu_id != __MESON_CPU_MAJOR_ID_TXLX)
		&& (cpu_id != __MESON_CPU_MAJOR_ID_GXM))
		return ret;

	if ((addr == VIU_OSD2_BLK0_CFG_W4) ||
		(addr == VIU_OSD2_BLK1_CFG_W4) ||
		(addr == VIU_OSD2_BLK2_CFG_W4) ||
		(addr == VIU_OSD2_BLK3_CFG_W4) ||
		(addr == VPU_RDARB_MODE_L1C2) ||
		(addr == VIU_MISC_CTRL1) ||
		(addr ==
		DOLBY_CORE2A_SWAP_CTRL1) ||
		(addr ==
		DOLBY_CORE2A_SWAP_CTRL2)) {
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

static int update_recovery_item_g12a(u32 addr, u32 value)
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
		if (backup_enable &
			HW_RESET_OSD2_REGS) {
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
	case VIU_OSD3_CTRL_STAT:
	case VIU_OSD3_BLK0_CFG_W2:
		/* osd3 */
		if (backup_enable &
			HW_RESET_OSD3_REGS) {
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
		/* osd1 sc */
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
	case OSD2_VSC_PHASE_STEP:
		/* osd2 osd 3 sc */
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
	case OSD34_VSC_PHASE_STEP:
		/* osd2 osd 3 sc */
		base = gRecovery[4].base_addr;
		size = gRecovery[4].size;
		table = gRecovery[4].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC_BLOCK_ID:
		/* vpu mali common */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[5].base_addr;
		size = gRecovery[5].size;
		table = gRecovery[5].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0:
		/* vpu mali src0 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[6].base_addr;
		size = gRecovery[6].size;
		table = gRecovery[6].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1:
		/* vpu mali src1 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[7].base_addr;
		size = gRecovery[7].size;
		table = gRecovery[7].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S2:
		/* vpu mali src2 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[8].base_addr;
		size = gRecovery[8].size;
		table = gRecovery[8].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VIU_OSD_BLEND_CTRL:
	case VIU_OSD_BLEND_CTRL1:
		/* osd blend ctrl */
		base = gRecovery[9].base_addr;
		size = gRecovery[9].size;
		table = gRecovery[9].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPP_VD2_HDR_IN_SIZE:
		/* vpp blend ctrl */
		base = gRecovery[10].base_addr;
		size = gRecovery[10].size;
		table = gRecovery[10].table;
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

	if ((addr == OSD_PATH_MISC_CTRL) ||
		(addr == VIU_OSD1_DIMM_CTRL) ||
		(addr == VIU_OSD2_DIMM_CTRL) ||
		(addr == VIU_OSD2_BLK0_CFG_W4) ||
		(addr == VIU_OSD2_BLK1_CFG_W4) ||
		(addr == VIU_OSD2_BLK2_CFG_W4) ||
		(addr == VIU_OSD2_MALI_UNPACK_CTRL) ||
		(addr == DOLBY_CORE2A_SWAP_CTRL1) ||
		(addr == DOLBY_CORE2A_SWAP_CTRL2)) {
		table = gRecovery[11].table;
		for (i = 0; i <  gRecovery[11].size; i++) {
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

static s32 get_recovery_item_g12a(u32 addr, u32 *value, u32 *mask)
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
		if (backup_enable &
			HW_RESET_OSD2_REGS) {
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
	case VIU_OSD3_CTRL_STAT:
	case VIU_OSD3_BLK0_CFG_W2:
		/* osd3 */
		if (backup_enable &
			HW_RESET_OSD3_REGS) {
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
		/* osd1 sc */
		base = gRecovery[2].base_addr;
		size = gRecovery[2].size;
		table = gRecovery[2].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case OSD2_VSC_PHASE_STEP:
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
	case OSD34_VSC_PHASE_STEP:
		/* osd3 sc */
		base = gRecovery[4].base_addr;
		size = gRecovery[4].size;
		table = gRecovery[4].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_BLOCK_ID:
		/* vpu mali common */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[5].base_addr;
		size = gRecovery[5].size;
		table = gRecovery[5].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0:
		/* vpu mali src0 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[6].base_addr;
		size = gRecovery[6].size;
		table = gRecovery[6].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1:
		/* vpu mali src1 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[7].base_addr;
		size = gRecovery[7].size;
		table = gRecovery[7].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S2:
		/* vpu mali src2 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[8].base_addr;
		size = gRecovery[8].size;
		table = gRecovery[8].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VIU_OSD_BLEND_CTRL:
	case VIU_OSD_BLEND_CTRL1:
		/* osd blend ctrl */
		base = gRecovery[9].base_addr;
		size = gRecovery[9].size;
		table = gRecovery[9].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPP_VD2_HDR_IN_SIZE:
		/* vpp blend ctrl */
		base = gRecovery[10].base_addr;
		size = gRecovery[10].size;
		table = gRecovery[10].table;
		if ((addr >= base) &&
			(addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	default:
		break;
	}

	if ((addr == OSD_PATH_MISC_CTRL) ||
		(addr == VIU_OSD1_DIMM_CTRL) ||
		(addr == VIU_OSD2_DIMM_CTRL) ||
		(addr == VIU_OSD2_BLK0_CFG_W4) ||
		(addr == VIU_OSD2_BLK1_CFG_W4) ||
		(addr == VIU_OSD2_BLK2_CFG_W4) ||
		(addr == VIU_OSD2_MALI_UNPACK_CTRL) ||
		(addr == DOLBY_CORE2A_SWAP_CTRL1) ||
		(addr == DOLBY_CORE2A_SWAP_CTRL2)) {
		table = gRecovery[11].table;
		for (i = 0; i <  gRecovery[11].size; i++) {
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

			if (enable_vd_zorder &&
				(addr == OSD2_BLEND_SRC_CTRL)) {
				ret = 1;
				table->recovery = 0;
			} else if ((real_value & regmask)
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

int update_recovery_item(u32 addr, u32 value)
{
	int ret = -1;
	int cpu_id = osd_hw.osd_meson_dev.cpu_id;

	if (!recovery_enable)
		return ret;

	if (cpu_id >= __MESON_CPU_MAJOR_ID_G12A)
		ret = update_recovery_item_g12a(addr, value);
	else
		ret = update_recovery_item_old(addr, value);

	return ret;
}

s32 get_recovery_item(u32 addr, u32 *value, u32 *mask)
{
	int ret = -1;
	int cpu_id = osd_hw.osd_meson_dev.cpu_id;

	if (!recovery_enable)
		return ret;

	if (cpu_id >= __MESON_CPU_MAJOR_ID_G12A)
		ret = get_recovery_item_g12a(addr, value, mask);
	else
		ret = get_recovery_item_old(addr, value, mask);

	return ret;
}
#if 0
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
