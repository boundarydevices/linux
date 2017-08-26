/*
 * stbcfg01-private.h - Voltage regulator driver for the STBCFG01 driver
 *
 *  Copyright (C) 2009-2010 Samsung Electrnoics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_MFD_STBCFG01_PRIV_H
#define __LINUX_MFD_STBCFG01_PRIV_H

#include <linux/i2c.h>

#define I2C_ADDR_FUELGAUGE                0x36
#define I2C_ADDR_CHARGER                0x69

#define MAX77823_IRQSRC_CHG             (1 << 0)
#define MAX77823_IRQSRC_FG              (1 << 1)

#define MAX77823_REG_INVALID		(0xff)

/* Slave addr = 0xCC : PMIC */
enum max77823_pmic_reg {
	MAX77823_PMIC_ID                             = 0x20,
	MAX77823_PMIC_Ver                            = 0x21,
	MAX77823_PMIC_INT                            = 0x22,
	MAX77823_PMIC_INT_MASK                       = 0x23,
	MAX77823_PMIC_SYS_INT                        = 0x24,
	MAX77823_PMIC_SYS_INT_MASK                   = 0x26,
	MAX77823_PMIC_SAFEOUT_LDO_Control            = 0xC6,

	MAX77823_PMIC_END,
};

/* Slave addr = 0xD2 : Charger */
enum max77823_charger_reg {
	MAX77823_CHG_INT                             = 0xB0,
	MAX77823_CHG_INT_MASK                        = 0xB1,
	MAX77823_CHG_INT_OK                          = 0xB2,
	MAX77823_CHG_DETAILS_00                      = 0xB3,
	MAX77823_CHG_DETAILS_01                      = 0xB4,
	MAX77823_CHG_DETAILS_02                      = 0xB5,
	MAX77823_CHG_CNFG_00                         = 0xB7,
	MAX77823_CHG_CNFG_01                         = 0xB8,
	MAX77823_CHG_CNFG_02                         = 0xB9,
	MAX77823_CHG_CNFG_03                         = 0xBA,
	MAX77823_CHG_CNFG_04                         = 0xBB,
	MAX77823_CHG_CNFG_06                         = 0xBD,
	MAX77823_CHG_CNFG_07                         = 0xBE,
	MAX77823_CHG_CNFG_09                         = 0xC0,
	MAX77823_CHG_CNFG_10                         = 0xC1,
	MAX77823_CHG_CNFG_11                         = 0xC2,
	MAX77823_CHG_CNFG_12                         = 0xC3,

	MAX77823_CHG_END,
};

/* Slave addr = 0x6C : Fuelgauge */
enum max77823_fuelgauge_reg {
	MAX77823_FUEL_INT                             = 0xB0,
	MAX77823_FUEL_INT_MASK                        = 0xB1,
};

enum max77823_irq_source {
	CHG_IRQ = 0,
	FUEL_IRQ,

	MAX77823_IRQ_GROUP_NR,
};

enum max77823_irq {
	/* Charger */
	MAX77823_CHG_IRQ_BYP_I,
	MAX77823_CHG_IRQ_BATP_I,
	MAX77823_CHG_IRQ_BAT_I,
	MAX77823_CHG_IRQ_CHG_I,
	MAX77823_CHG_IRQ_WCIN_I,
	MAX77823_CHG_IRQ_CHGIN_I,

	/* Fuelgauge */
	MAX77823_FG_IRQ_ALERT,

	MAX77823_IRQ_NR,
};

struct max77823_dev {
	struct device *dev;
	struct i2c_client *i2c;
	struct i2c_client *charger;
	struct i2c_client *fuelgauge;
	struct max77823_platform_data *pdata;
	struct mutex i2c_lock;

	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[MAX77823_IRQ_GROUP_NR];
	int irq_masks_cache[MAX77823_IRQ_GROUP_NR];
};

/* MAX77823 shared i2c API function */
extern int max77823_read_reg(struct i2c_client *i2c, u8 reg);
extern int max77823_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77823_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int max77823_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77823_write_word(struct i2c_client *i2c, u8 reg, u16 value);
extern int max77823_read_word(struct i2c_client *i2c, u8 reg);

extern int max77823_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
#endif /*  __LINUX_MFD_STBCFG01_PRIV_H */
