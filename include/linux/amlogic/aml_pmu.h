/*
 * include/linux/amlogic/aml_pmu.h
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

#ifndef __AML_PMU_H__
#define __AML_PMU_H__

#include <linux/power_supply.h>
#include <linux/notifier.h>
/* battery capability is very low */
#define BATCAPCORRATE		5
#define ABS(x)			((x) > 0 ? (x) : -(x))
#define AML_PMU_WORK_CYCLE	2000	/* PMU work cycle */

/*
 * debug message control
 */
#define AML_PMU_DBG(format, args...) \
	pr_debug("[AML_PMU]"format, ##args)

#define AML_PMU_INFO(format, args...) \
	pr_info("[AML_PMU]"format, ##args)

#define AML_PMU_ERR(format, args...) \
	pr_err("[AML_PMU]"format, ##args)

#define AML_PMU_CHG_ATTR(_name) \
	{ \
		.attr = { .name = #_name, .mode = 0644 }, \
		.show =  _name##_show, \
		.store = _name##_store, \
	}

#ifdef CONFIG_AMLOGIC_1218
#define AML1218_ADDR				0x35

#define AML1218_DRIVER_VERSION			"v1.0"
#define AML1218_DRIVER_NAME			"aml1218_pmu"
#define AML1218_IRQ_NUM				INT_GPIO_2
#define AML1218_IRQ_NAME			"aml1218_irq"
#define AML1218_WORK_CYCLE			2000

/* add for AML1218 ctroller. */
#define AML1218_OTP_GEN_CONTROL0		0x17

#define AML1218_CHG_CTRL0			0x29
#define AML1218_CHG_CTRL1			0x2A
#define AML1218_CHG_CTRL2			0x2B
#define AML1218_CHG_CTRL3			0x2C
#define AML1218_CHG_CTRL4			0x2D
#define AML1218_CHG_CTRL5			0x2E
#define AML1218_CHG_CTRL6			0x129
#define AML1218_SAR_ADJ				0x73


#define AML1218_GEN_CNTL0			0x80
#define AML1218_GEN_CNTL1			0x81
#define AML1218_PWR_UP_SW_ENABLE		0x82
#define AML1218_PWR_DN_SW_ENABLE		0x84
#define AML1218_GEN_STATUS0			0x86
#define AML1218_GEN_STATUS1			0x87
#define AML1218_GEN_STATUS2			0x88
#define AML1218_GEN_STATUS3			0x89
#define AML1218_GEN_STATUS4			0x8A
#define AML1218_WATCH_DOG			0x8F
#define AML1218_PWR_KEY_ADDR			0x90
#define AML1218_SAR_SW_EN_FIELD			0x9A
#define AML1218_SAR_CNTL_REG0			0x9B
#define AML1218_SAR_CNTL_REG2			0x9D
#define AML1218_SAR_CNTL_REG3			0x9E
#define AML1218_SAR_CNTL_REG5			0xA0
#define AML1218_SAR_RD_IBAT_LAST		0xAB
#define AML1218_SAR_RD_VBAT_ACTIVE		0xAF
#define AML1218_SAR_RD_MANUAL			0xB1
#define AML1218_SAR_RD_IBAT_ACC			0xB5
#define AML1218_SAR_RD_IBAT_CNT			0xB9
#define AML1218_GPIO_OUTPUT_CTRL		0xC3
#define AML1218_GPIO_INPUT_STATUS		0xC4
#define AML1218_IRQ_MASK_0			0xC8
#define AML1218_IRQ_STATUS_CLR_0		0xCF
#define AML1218_SP_CHARGER_STATUS0		0xDE
#define AML1218_SP_CHARGER_STATUS1		0xDF
#define AML1218_SP_CHARGER_STATUS2		0xE0
#define AML1218_SP_CHARGER_STATUS3		0xE1
#define AML1218_SP_CHARGER_STATUS4		0xE2
#define AML1218_PIN_MUX4			0xF4

#define AML1218_DCDC1				0
#define AML1218_DCDC2				1
#define AML1218_DCDC3				2
#define AML1218_BOOST				3
#define AML1218_LDO1				4
#define AML1218_LDO2				5
#define AML1218_LDO3				6
#define AML1218_LDO4				7
#define AML1218_LDO5				8

#define AML1218_CHARGER_CHARGING		1
#define AML1218_CHARGER_DISCHARGING		2
#define AML1218_CHARGER_NONE			3

#define AML1218_DBG(format, args...) \
	pr_debug("[AML1218]"format, ##args)

#define AML1218_INFO(format, args...) \
	pr_info("[AML1218]"format, ##args)

#define AML1218_ERR(format, args...) \
	pr_err("[AML1218]"format, ##args)

#define ABS(x)					((x) > 0 ? (x) : -(x))

#define AML_ATTR(_name) \
	{ \
		.attr = { .name = #_name, .mode = 0644 }, \
		.show =  _name##_show, \
		.store = _name##_store, \
	}

extern int  aml1218_write(int32_t add, uint8_t val);
extern int  aml1218_write16(int32_t add, uint16_t val);
extern int  aml1218_writes(int32_t add, uint8_t *buff, int len);
extern int  aml1218_read(int add, uint8_t *val);
extern int  aml1218_read16(int add, uint16_t *val);
extern int  aml1218_reads(int add, uint8_t *buff, int len);
extern int  aml1218_set_bits(int addr, uint8_t bits, uint8_t mask);

extern int aml1218_get_dcdc_voltage(int dcdc, uint32_t *uV);
extern int aml1218_set_dcdc_voltage(int dcdc, uint32_t voltage);
extern struct i2c_client *g_aml1218_client;
#endif  /* CONFIG_AMLOGIC_1218 */

#endif /* __AML_PMU_H__ */

