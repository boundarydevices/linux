/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


/*
 * mx53_nitrogen_pmic_da9053.c  --  i.MX53 Nitrogen driver for pmic da9053
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/da9052/da9052.h>
#include <linux/mfd/da9052/pm.h>
#include <linux/mfd/da9052/led.h>
#include <linux/mfd/da9052/tsi.h>
#include <mach/irqs.h>
#include <mach/iomux-mx53.h>

#define mV_to_uV(mV) (mV * 1000)

static struct regulator_consumer_supply vbuckmem_consumers[] = {
	{
		/* sgtl5000 */
		.supply = "VDDA",
		.dev_name = "2-000a",
	},
};

static struct regulator_consumer_supply ldo4_consumers[] = {
	{
		/* audio amp */
		.supply = "VDD_AMP",
		.dev_name = "imx-3stack-sgtl5000.0",
	},
};

static struct regulator_consumer_supply ldo5_consumers[] = {
	{
		/* sata */
		.supply = "VDD_CORE",
		.dev_name = "ahci.0",
	},
};

static struct regulator_consumer_supply ldo7_consumers[] = {
	{
		/* sgtl5000 */
		.supply = "VDDIO",
		.dev_name = "2-000a",
	},
};

struct regulator_consumer_supply ldo8_consumers[] = {
	{
		/* Camera 1.8V  */
		.supply = "VDD_IO",
		.dev_name = "1-003c",	/* Nitrogen A/K change this */
	},
};

struct regulator_consumer_supply ldo9_consumers[] = {
	{
		/* Camera 2.75V */
		.supply = "VDD_A",
		.dev_name = "1-003c",	/* Nitrogen A/K change this */
	},
#if 1
	{
		/* Touch screen */
		.supply = "VDD_A",
		.dev_name = "da9052_tsi",
	},
#endif
};

/* currently the .state_mem.uV here takes no effects for DA9053
preset-voltage have to be done in the latest stage during
suspend*/
static struct regulator_init_data da9052_regulators_init[] = {
	{
		.constraints = {
			/*
			 * 0.6 - 1.8V, 40 mA MAX
			 * default 1.2V(0x4c), Freescale 1.3V(0x4e)
			 * R50(0x32), base 0.6V, step .05V
			 */
			.name		= "DA9052_LDO1",
			.max_uV		= mV_to_uV(DA9052_LDO1_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9052_LDO1_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.always_on = 1,
			.boot_on = 1,
			.state_mem = {
				.uV = mV_to_uV(1300),
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 1,
				.disabled = 0,
			},
		},
	},
	{
		.constraints = {
			/*
			 * 0.6 - 1.8V, 100 mA MAX
			 * default 1.2V(0x58), UBL 0.9V(0x4c)
			 * R51(0x33), base 0.6V, step .05V
			 */
			.name		= "DA9052_LDO2",
			.max_uV		= mV_to_uV(DA9052_LDO2_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9052_LDO2_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.always_on = 1,
			.boot_on = 1,
			.state_mem = {
				.uV = mV_to_uV(900),
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 1,
				.disabled = 0,
			},
		},
	},
	{
		.constraints = {
			/*
			 * 1.725 - 3.3V, 200 mA MAX
			 * default 2.85V(0x6d), Freescale 3.3V(0x7f)
			 * R52(0x34), base 1.725V, step .025V
			 */
			.name		= "DA9052_LDO3",
			.max_uV		= mV_to_uV(DA9052_LDO34_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9052_LDO34_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.always_on = 1,
			.boot_on = 1,
			.state_mem = {
				.uV = mV_to_uV(3300),
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 1,
				.disabled = 0,
			},
		},
	},
	{
		.constraints = {
			/*
			 * Audio AMP Power
			 * 1.725 - 3.3V, 150 mA MAX
			 * default 2.85V(0x6d), Freescale 2.775V(0x6a), 3.3V(0x7f) Needed
			 * R53(0x35), base 1.725V, step .025V
			 */
			.name		= "DA9052_LDO4",
			.max_uV		= mV_to_uV(DA9052_LDO34_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9052_LDO34_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.always_on = 1,
			.boot_on = 1,
			.state_mem = {
				.uV = mV_to_uV(3300),
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 1,
				.disabled = 0,
			},
		},
		.num_consumer_supplies = ARRAY_SIZE(ldo4_consumers),
		.consumer_supplies = ldo4_consumers,
	},
	{
		.constraints = {
			/*
			 * SATA Power
			 * 1.2 - 3.6V, 100 mA MAX
			 * default 3.1V(0x66), Freescale 1.3V(0x42), 1.2V(0x40) Needed
			 * R54(0x36), base 1.2V, step .05V
			 */
			.name		= "DA9052_LDO5",
			.max_uV		= mV_to_uV(DA9052_LDO567810_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9052_LDO567810_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.state_mem = {
				.uV = mV_to_uV(1200),
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 1,
				.disabled = 0,
			},
		},
		.num_consumer_supplies = ARRAY_SIZE(ldo5_consumers),
		.consumer_supplies = ldo5_consumers,
	},
	{
		.constraints = {
			/*
			 * 1.2 - 3.6V, 150 mA MAX
			 * default 1.2V(0x40), Freescale 1.3V(0x42), 1.2V(0x40) Needed
			 * R55(0x37), base 1.2V, step .05V
			 */
			.name		= "DA9052_LDO6",
			.max_uV		= mV_to_uV(DA9052_LDO567810_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9052_LDO567810_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.always_on = 1,
			.boot_on = 1,
			.state_mem = {
				.uV = mV_to_uV(1200),
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 1,
				.disabled = 0,
			},
		},
	},
	{
		.constraints = {
			/*
			 * 1.2 - 3.6V, 200 mA MAX
			 * default 3.1V(0x66), Freescale 2.75V(0x5f)
			 * R56(0x38), base 1.2V, step .05V
			 */
			.name		= "DA9052_LDO7",
			.max_uV		= mV_to_uV(DA9052_LDO567810_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9052_LDO567810_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.always_on = 1,
			.boot_on = 1,
			.state_mem = {
				.uV = mV_to_uV(2750),
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 1,
				.disabled = 0,
			},
		},
		.num_consumer_supplies = ARRAY_SIZE(ldo7_consumers),
		.consumer_supplies = ldo7_consumers,
	},
	{
		.constraints = {
			/*
			 * Camera DB Power
			 * 1.2 - 3.6V, 200 mA MAX
			 * default 2.85V(0x61), Freescale 1.8V(0x4c)
			 * R57(0x39), base 1.2V, step .05V
			 */
			.name		= "DA9052_LDO8",
			.max_uV		= mV_to_uV(DA9052_LDO567810_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9052_LDO567810_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.state_mem = {
				.uV = mV_to_uV(1800),
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 1,
				.disabled = 0,
			},
		},
		.num_consumer_supplies = ARRAY_SIZE(ldo8_consumers),
		.consumer_supplies = ldo8_consumers,
	},
	{
		.constraints = {
			/*
			 * Camera Power
			 * 1.25 - 3.6V, 100 mA MAX
			 * default 2.5V(0x59), Freescale 1.5V(0x45), need 2.8V(0x5f)
			 * R58(0x3a), base 1.2V, step .05V
			 */
			.name		= "DA9052_LDO9",
			.max_uV		= mV_to_uV(DA9052_LDO9_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9052_LDO9_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.state_mem = {
				.uV = mV_to_uV(2800),
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 1,
				.disabled = 0,
			},
		},
		.num_consumer_supplies = ARRAY_SIZE(ldo9_consumers),
		.consumer_supplies = ldo9_consumers,
	},
	{
		.constraints = {
			/*
			 * Next board, expansion connector power
			 * This board tfp410 3.3V(0x6a)
			 * 1.2 - 3.6V, 250 mA MAX
			 * default 1.8V(0x06), Freescale 1.3V(0x42), uboot 3.3V(0x6a)
			 * R59(0x3b), base 1.2V, step .05V
			 */
			.name		= "DA9052_LDO10",
			.max_uV		= mV_to_uV(DA9052_LDO567810_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9052_LDO567810_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.always_on = 1,
			.boot_on = 1,
			.state_mem = {
				.uV = mV_to_uV(3300),
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 0,
				.disabled = 0,
			},
		},
	},
	/* BUCKS */
	{
		.constraints = {
			/*
			 * 1st rev was
			 * DDR power
			 * 0.725 - 2.075V, 2 amps MAX
			 * default 1.8V(0x74), UBL 1.8V(0x74)
			 * R46(0x2e), base 0.5V, step .025V
			 *
			 * Now, it is VDD_GP, graphic processor power 1.15V
			 */
			.name		= "DA9052_BUCK_CORE",
			.max_uV		= mV_to_uV(DA9052_BUCK_CORE_PRO_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9052_BUCK_CORE_PRO_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.always_on = 1,
			.boot_on = 1,
			.state_mem = {
				.uV = mV_to_uV(1150),	//1800 for rev 1 of board
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 1,
				.disabled = 0,
			},
		},
	},
	{
		.constraints = {
			/*
			 * 0.725 - 2.075V, 1 amp MAX, need 1.3V for 1Ghz CPU
			 * default 1.2V(0x5c), Freescale 1.3V(0x60)
			 * R47(0x2f), base 0.5V, step .025V
			 */
			.name		= "DA9052_BUCK_PRO",
			.max_uV		= mV_to_uV(DA9052_BUCK_CORE_PRO_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9052_BUCK_CORE_PRO_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.always_on = 1,
			.boot_on = 1,
			.state_mem = {
				.uV = mV_to_uV(1300),
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 1,
				.disabled = 0,
			},
		},
	},
	{
		.constraints = {
			/*
			 * 0.925 - 2.5V, 1 amp MAX
			 * default 2.0V(0x6b), UBL 1.8V(0x63)
			 * R48(0x30), base 0.925V, step .025V
			 * */
			.name		= "DA9052_BUCK_MEM",
			.max_uV		= mV_to_uV(DA9052_BUCK_MEM_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9052_BUCK_MEM_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.always_on = 1,
			.boot_on = 1,
			.state_mem = {
				.uV = mV_to_uV(1800),
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 1,
				.disabled = 0,
			},
		},
		.num_consumer_supplies = ARRAY_SIZE(vbuckmem_consumers),
		.consumer_supplies = vbuckmem_consumers,
	},
	{
		.constraints = {
			/*
			 * 0.925 - 2.475V, 1 amp MAX
			 * 1.7V setting gives 1.675V
			 * 2.5V setting gives 2.475V
			 * default 1.6V(0x5b), Freescale 2.475V(0x7e)
			 * R49(0x31), base 0.925V, step .025V
			 */
			.name		= "DA9052_BUCK_PERI",
			.max_uV		= mV_to_uV(DA9053_BUCK_PERI_VOLT_UPPER),
			.min_uV		= mV_to_uV(DA9053_BUCK_PERI_VOLT_LOWER),
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL,
			.always_on = 1,
			.boot_on = 1,
			.state_mem = {
				.uV = mV_to_uV(2475),
				.mode = REGULATOR_MODE_NORMAL,
				.enabled = 1,
				.disabled = 0,
			},
		},
	},
};


struct da9052_tsi_platform_data da9052_tsi = {
	.pen_up_interval = 50,
	.tsi_delay_bit_shift = 6,
	.tsi_skip_bit_shift = 3,
	.num_gpio_tsi_register = 3,
#ifdef CONFIG_DA905X_TS_MODE
	.config_index = CONFIG_DA905X_TS_MODE,
#endif
	.tsi_supply_voltage = 2800,
	.max_tsi_delay = TSI_DELAY_4SLOTS,
	.max_tsi_skip_slot = TSI_SKIP_330SLOTS,
};

static struct da9052_led_platform_data da9052_gpio_led[] = {
	{
		.id = DA9052_LED_4,
		.name = "LED_GPIO14",
	},
	{
		.id = DA9052_LED_5,
		.name = "LED_GPIO15",
	},
};

static struct da9052_leds_platform_data da9052_gpio_leds = {
	.num_leds = ARRAY_SIZE(da9052_gpio_led),
	.led = da9052_gpio_led,
};


static struct da9052_bat_platform_data da9052_bat = {
	.sw_temp_control_en = 0,
	.monitoring_interval = 500,
	.sw_bat_temp_threshold = 60,
	.sw_junc_temp_threshold = 120,
	.hysteresis_window_size = 1,
	.current_monitoring_window = 10,
	.bat_with_no_resistor = 62,
	.bat_capacity_limit_low = 4,
	.bat_capacity_full = 100,
	.bat_capacity_limit_high = 70,
	.chg_hysteresis_const = 89,
	.hysteresis_reading_interval = 1000,
	.hysteresis_no_of_reading = 10,
	.filter_size = 4,
	.bat_volt_cutoff = 2800,
	.vbat_first_valid_detect_iteration = 3,
};

static void da9052_init_ssc_cache(struct da9052 *da9052)
{
	unsigned char cnt;

	/* First initialize all registers as Non-volatile */
	for (cnt = 0; cnt < DA9052_REG_CNT; cnt++) {
		da9052->ssc_cache[cnt].type = NON_VOLATILE;
		da9052->ssc_cache[cnt].status = INVALID;
		da9052->ssc_cache[cnt].val = 0;
	}

	/* Now selectively set type for all Volatile registers */
	/* Reg 1 - 9 */
	da9052->ssc_cache[DA9052_STATUSA_REG].type = VOLATILE;
	da9052->ssc_cache[DA9052_STATUSB_REG].type = VOLATILE;
	da9052->ssc_cache[DA9052_STATUSC_REG].type = VOLATILE;
	da9052->ssc_cache[DA9052_STATUSD_REG].type = VOLATILE;

#define DA9052_EVENTA_REG			5
#define DA9052_EVENTB_REG			6
#define DA9052_EVENTC_REG			7
#define DA9052_EVENTD_REG			8

	da9052->ssc_cache[DA9052_EVENTA_REG].type = VOLATILE;
	da9052->ssc_cache[DA9052_EVENTB_REG].type = VOLATILE;
	da9052->ssc_cache[DA9052_EVENTC_REG].type = VOLATILE;
	da9052->ssc_cache[DA9052_EVENTD_REG].type = VOLATILE;
	da9052->ssc_cache[DA9052_FAULTLOG_REG].type = VOLATILE;

	/* Reg 15 */
	da9052->ssc_cache[DA9052_CONTROLB_REG].type = VOLATILE;
	/* Reg - 17 */
	da9052->ssc_cache[DA9052_CONTROLD_REG].type = VOLATILE;
	/* Reg - 60 */
	da9052->ssc_cache[DA9052_SUPPLY_REG].type = VOLATILE;
	/* Reg - 62 */
	da9052->ssc_cache[DA9052_CHGBUCK_REG].type = VOLATILE;

	/* Reg 67 - 68 */
	da9052->ssc_cache[DA9052_INPUTCONT_REG].type = VOLATILE;
	da9052->ssc_cache[DA9052_CHGTIME_REG].type = VOLATILE;

	/* Reg - 70 */
	da9052->ssc_cache[DA9052_BOOST_REG].type = VOLATILE;

	/* Reg - 81 */
	da9052->ssc_cache[DA9052_ADCMAN_REG].type = VOLATILE;

	/* Reg - 83 - 85 */
	da9052->ssc_cache[DA9052_ADCRESL_REG].type = VOLATILE;
	da9052->ssc_cache[DA9052_ADCRESH_REG].type = VOLATILE;
	da9052->ssc_cache[DA9052_VDDRES_REG].type = VOLATILE;

	/* Reg - 87 */
	da9052->ssc_cache[DA9052_ICHGAV_REG].type = VOLATILE;

	/* Reg - 90 */
	da9052->ssc_cache[DA9052_TBATRES_REG].type = VOLATILE;

	/* Reg - 95 */
	da9052->ssc_cache[DA9052_ADCIN4RES_REG].type = VOLATILE;

	/* Reg - 98 */
	da9052->ssc_cache[DA9052_ADCIN5RES_REG].type = VOLATILE;

	/* Reg - 101 */
	da9052->ssc_cache[DA9052_ADCIN6RES_REG].type = VOLATILE;

	/* Reg - 104 */
	da9052->ssc_cache[DA9052_TJUNCRES_REG].type = VOLATILE;

	/* Reg 106 - 110 */
	da9052->ssc_cache[DA9052_TSICONTB_REG].type	= VOLATILE;
	da9052->ssc_cache[DA9052_TSIXMSB_REG].type	= VOLATILE;
	da9052->ssc_cache[DA9052_TSIYMSB_REG].type	= VOLATILE;
	da9052->ssc_cache[DA9052_TSILSB_REG].type	= VOLATILE;
	da9052->ssc_cache[DA9052_TSIZMSB_REG].type	= VOLATILE;

	/* Reg 111 - 117 */
	da9052->ssc_cache[DA9052_COUNTS_REG].type	= VOLATILE;
	da9052->ssc_cache[DA9052_COUNTMI_REG].type	= VOLATILE;
	da9052->ssc_cache[DA9052_COUNTH_REG].type	= VOLATILE;
	da9052->ssc_cache[DA9052_COUNTD_REG].type	= VOLATILE;
	da9052->ssc_cache[DA9052_COUNTMO_REG].type	= VOLATILE;
	da9052->ssc_cache[DA9052_COUNTY_REG].type	= VOLATILE;
	da9052->ssc_cache[DA9052_ALARMMI_REG].type	= VOLATILE;

	/* Reg 122 - 125 */
	da9052->ssc_cache[DA9052_SECONDA_REG].type	= VOLATILE;
	da9052->ssc_cache[DA9052_SECONDB_REG].type	= VOLATILE;
	da9052->ssc_cache[DA9052_SECONDC_REG].type	= VOLATILE;
	da9052->ssc_cache[DA9052_SECONDD_REG].type	= VOLATILE;

	/* Following addresses are not assigned to any register */
	da9052->ssc_cache[126].type			= VOLATILE;
	da9052->ssc_cache[127].type			= VOLATILE;
}



static int __init nitrogen_da9052_init(struct da9052 *da9052)
{
	da9052_init_ssc_cache(da9052);
	return 0;
}

static struct da9052_platform_data __initdata da9052_plat = {
	.init = nitrogen_da9052_init,
	.num_regulators = ARRAY_SIZE(da9052_regulators_init),
	.regulators = da9052_regulators_init,
	.led_data = &da9052_gpio_leds,
	.tsi_data = &da9052_tsi,
	.bat_data = &da9052_bat,
	/* .gpio_base = GPIO_BOARD_START, */
};


static struct i2c_board_info __initdata da9052_i2c_device = {
	I2C_BOARD_INFO(DA9052_SSC_I2C_DEVICE_NAME, DA9052_I2C_ADDR >> 1),
	.platform_data = &da9052_plat,
};

int __init mx53_nitrogen_init_da9052(unsigned da9052_irq)
{
	da9052_i2c_device.irq = da9052_irq;
	/* Set interrupt as LOW LEVEL interrupt source */
	set_irq_type(da9052_irq, IRQF_TRIGGER_LOW);
#if defined(CONFIG_PMIC_DA9052) || defined(CONFIG_PMIC_DA9052_MODULE)
	return i2c_register_board_info(0, &da9052_i2c_device, 1);
#else
	return 0 ;
#endif
}
