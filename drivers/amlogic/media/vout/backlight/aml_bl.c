/*
 * drivers/amlogic/media/vout/backlight/aml_bl.c
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/backlight.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/pwm.h>
#include <linux/amlogic/pwm_meson.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#ifdef CONFIG_AMLOGIC_LCD
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>
#endif
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#endif

#include "aml_bl_reg.h"

/* #define AML_BACKLIGHT_DEBUG */
unsigned int bl_debug_print_flag;

static struct aml_bl_drv_s *bl_drv;

static unsigned int bl_key_valid;
static unsigned char bl_config_load;

/* bl_off_policy support */
static int aml_bl_off_policy_cnt;

static unsigned int bl_off_policy;
module_param(bl_off_policy, uint, 0664);
MODULE_PARM_DESC(bl_off_policy, "bl_off_policy");

static unsigned int bl_level;
module_param(bl_level, uint, 0664);
MODULE_PARM_DESC(bl_level, "bl_level");

static unsigned int bl_level_uboot;
static unsigned int brightness_bypass;
module_param(brightness_bypass, uint, 0664);
MODULE_PARM_DESC(brightness_bypass, "bl_brightness_bypass");

static unsigned char bl_pwm_bypass; /* debug flag */
static unsigned char bl_pwm_duty_free; /* debug flag */
static unsigned char bl_on_request; /* for lcd power sequence */
static unsigned char bl_step_on_flag;
static unsigned int bl_on_level;

static DEFINE_MUTEX(bl_power_mutex);
static DEFINE_MUTEX(bl_level_mutex);

static struct bl_config_s bl_config = {
	.level_default = 128,
	.level_mid = 128,
	.level_mid_mapping = 128,
	.level_min = 10,
	.level_max = 255,
	.power_on_delay = 100,
	.power_off_delay = 30,
	.method = BL_CTRL_MAX,
	.ldim_flag = 0,

	.bl_pwm = NULL,
	.bl_pwm_combo0 = NULL,
	.bl_pwm_combo1 = NULL,
	.pwm_on_delay = 0,
	.pwm_off_delay = 0,

	.bl_gpio = {
		{.probe_flag = 0, .register_flag = 0,},
		{.probe_flag = 0, .register_flag = 0,},
		{.probe_flag = 0, .register_flag = 0,},
		{.probe_flag = 0, .register_flag = 0,},
		{.probe_flag = 0, .register_flag = 0,},
	},

	.pinmux_flag = 0xff,
};

const char *bl_chip_table[] = {
	"GXTVBB",
	"GXM",
	"GXL",
	"TXL",
	"TXLX",
	"AXG",
	"invalid",
};

struct bl_method_match_s {
	char *name;
	enum bl_ctrl_method_e type;
};

static struct bl_method_match_s bl_method_match_table[] = {
	{"gpio",          BL_CTRL_GPIO},
	{"pwm",           BL_CTRL_PWM},
	{"pwm_combo",     BL_CTRL_PWM_COMBO},
	{"local_dimming", BL_CTRL_LOCAL_DIMMING},
	{"extern",        BL_CTRL_EXTERN},
	{"invalid",       BL_CTRL_MAX},
};

static char *bl_method_type_to_str(int type)
{
	int i;
	char *str = bl_method_match_table[BL_CTRL_MAX].name;

	for (i = 0; i < BL_CTRL_MAX; i++) {
		if (type == bl_method_match_table[i].type) {
			str = bl_method_match_table[i].name;
			break;
		}
	}
	return str;
}

static unsigned int pwm_reg_txl[6] = {
	PWM_PWM_A,
	PWM_PWM_B,
	PWM_PWM_C,
	PWM_PWM_D,
	PWM_PWM_E,
	PWM_PWM_F,
};

static unsigned int pwm_reg_txlx[6] = {
	PWM_PWM_A_TXLX,
	PWM_PWM_B_TXLX,
	PWM_PWM_C_TXLX,
	PWM_PWM_D_TXLX,
	PWM_PWM_E_TXLX,
	PWM_PWM_F_TXLX,
};

static int aml_bl_check_driver(void)
{
	int ret = 0;

	if (bl_drv == NULL) {
		BLERR("no bl driver\n");
		return -1;
	}
	switch (bl_drv->bconf->method) {
	case BL_CTRL_PWM:
		if (bl_drv->bconf->bl_pwm == NULL) {
			ret = -1;
			BLERR("no bl_pwm struct\n");
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (bl_drv->bconf->bl_pwm_combo0 == NULL) {
			ret = -1;
			BLERR("no bl_pwm_combo_0 struct\n");
		}
		if (bl_drv->bconf->bl_pwm_combo1 == NULL) {
			ret = -1;
			BLERR("no bl_pwm_combo_1 struct\n");
		}
		break;
	case BL_CTRL_MAX:
		ret = -1;
		break;
	default:
		break;
	}

	return ret;
}

struct aml_bl_drv_s *aml_bl_get_driver(void)
{
	if (bl_drv == NULL)
		BLERR("no bl driver");

	return bl_drv;
}

static void bl_gpio_probe(int index)
{
	struct bl_gpio_s *bl_gpio;
	const char *str;
	int ret;

	if (index >= BL_GPIO_NUM_MAX) {
		BLERR("gpio index %d, exit\n", index);
		return;
	}
	bl_gpio = &bl_drv->bconf->bl_gpio[index];
	if (bl_gpio->probe_flag) {
		if (bl_debug_print_flag) {
			BLPR("gpio %s[%d] is already registered\n",
				bl_gpio->name, index);
		}
		return;
	}

	/* get gpio name */
	ret = of_property_read_string_index(bl_drv->dev->of_node,
		"bl_gpio_names", index, &str);
	if (ret) {
		BLERR("failed to get bl_gpio_names: %d\n", index);
		str = "unknown";
	}
	strcpy(bl_gpio->name, str);

	/* init gpio flag */
	bl_gpio->probe_flag = 1;
	bl_gpio->register_flag = 0;
}

static int bl_gpio_register(int index, int init_value)
{
	struct bl_gpio_s *bl_gpio;
	int value;

	if (index >= BL_GPIO_NUM_MAX) {
		BLERR("%s: gpio index %d, exit\n", __func__, index);
		return -1;
	}
	bl_gpio = &bl_drv->bconf->bl_gpio[index];
	if (bl_gpio->probe_flag == 0) {
		BLERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (bl_gpio->register_flag) {
		BLPR("%s: gpio %s[%d] is already registered\n",
			__func__, bl_gpio->name, index);
		return 0;
	}

	switch (init_value) {
	case BL_GPIO_OUTPUT_LOW:
		value = GPIOD_OUT_LOW;
		break;
	case BL_GPIO_OUTPUT_HIGH:
		value = GPIOD_OUT_HIGH;
		break;
	case BL_GPIO_INPUT:
	default:
		value = GPIOD_IN;
		break;
	}

	/* request gpio */
	bl_gpio->gpio = devm_gpiod_get_index(bl_drv->dev, "bl", index, value);
	if (IS_ERR(bl_gpio->gpio)) {
		BLERR("register gpio %s[%d]: %p, err: %d\n",
			bl_gpio->name, index, bl_gpio->gpio,
			IS_ERR(bl_gpio->gpio));
		return -1;
	}

	bl_gpio->register_flag = 1;
	if (bl_debug_print_flag) {
		BLPR("register gpio %s[%d]: %p, init value: %d\n",
			bl_gpio->name, index, bl_gpio->gpio, init_value);
	}

	return 0;
}

static void bl_gpio_set(int index, int value)
{
	struct bl_gpio_s *bl_gpio;

	if (index >= BL_GPIO_NUM_MAX) {
		BLERR("gpio index %d, exit\n", index);
		return;
	}
	bl_gpio = &bl_drv->bconf->bl_gpio[index];
	if (bl_gpio->probe_flag == 0) {
		BLERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return;
	}
	if (bl_gpio->register_flag == 0) {
		bl_gpio_register(index, value);
		return;
	}

	if (IS_ERR_OR_NULL(bl_gpio->gpio)) {
		BLERR("gpio %s[%d]: %p, err: %ld\n",
			bl_gpio->name, index, bl_gpio->gpio,
			PTR_ERR(bl_gpio->gpio));
		return;
	}

	switch (value) {
	case BL_GPIO_OUTPUT_LOW:
	case BL_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(bl_gpio->gpio, value);
		break;
	case BL_GPIO_INPUT:
	default:
		gpiod_direction_input(bl_gpio->gpio);
		break;
	}
	if (bl_debug_print_flag) {
		BLPR("set gpio %s[%d] value: %d\n",
			bl_gpio->name, index, value);
	}
}

static inline unsigned int bl_do_div(unsigned long num, unsigned int den)
{
	unsigned long long ret = num;

	do_div(ret, den);
	return (unsigned int)ret;
}

/* ****************************************************** */
#define BL_PINMUX_MAX    8
static char *bl_pinmux_str[BL_PINMUX_MAX] = {
	"pwm_on",               /* 0 */
	"pwm_vs_on",            /* 1 */
	"pwm_combo_0_1_on",     /* 2 */
	"pwm_combo_0_vs_1_on",  /* 3 */
	"pwm_combo_0_1_vs_on",  /* 4 */
	"pwm_off",              /* 5 */
	"pwm_combo_off",        /* 6 */
	"none",
};

static void bl_pwm_pinmux_set(struct bl_config_s *bconf, int status)
{
	int index = 0xff;

	if (bl_debug_print_flag)
		BLPR("%s\n", __func__);

	switch (bconf->method) {
	case BL_CTRL_PWM:
		if (status) {
			if (bconf->bl_pwm->pwm_port == BL_PWM_VS)
				index = 1;
			else
				index = 0;
		} else {
			index = 5;
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (status) {
			if (bconf->bl_pwm_combo0->pwm_port == BL_PWM_VS) {
				index = 3;
			} else {
				if (bconf->bl_pwm_combo1->pwm_port == BL_PWM_VS)
					index = 4;
				else
					index = 2;
			}
		} else {
			index = 6;
		}
		break;
	default:
		BLERR("%s: wrong ctrl_mothod=%d\n", __func__, bconf->method);
		break;
	}

	if (index >= BL_PINMUX_MAX) {
		BLERR("%s: pinmux index %d is invalid\n", __func__, index);
		return;
	}

	if (bconf->pinmux_flag == index) {
		BLPR("pinmux %s is already selected\n",
			bl_pinmux_str[index]);
		return;
	}

	/* request pwm pinmux */
	bconf->pin = devm_pinctrl_get_select(bl_drv->dev, bl_pinmux_str[index]);
	if (IS_ERR(bconf->pin)) {
		BLERR("set pinmux %s error\n", bl_pinmux_str[index]);
	} else {
		if (bl_debug_print_flag) {
			BLPR("set pinmux %s: %p\n",
				bl_pinmux_str[index], bconf->pin);
		}
	}
	bconf->pinmux_flag = index;
}

/* ****************************************************** */

static int bl_pwm_out_level_check(struct bl_pwm_config_s *bl_pwm)
{
	int out_level = 0xff;
	unsigned int pwm_range;

	if (bl_pwm->pwm_duty_max > 100)
		pwm_range = 255;
	else
		pwm_range = 100;

	switch (bl_pwm->pwm_method) {
	case BL_PWM_POSITIVE:
		if (bl_pwm->pwm_duty == 0)
			out_level = 0;
		else if (bl_pwm->pwm_duty == pwm_range)
			out_level = 1;
		else
			out_level = 0xff;
		break;
	case BL_PWM_NEGATIVE:
		if (bl_pwm->pwm_duty == 0)
			out_level = 1;
		else if (bl_pwm->pwm_duty == pwm_range)
			out_level = 0;
		else
			out_level = 0xff;
		break;
	default:
		BLERR("%s: port %d: invalid pwm_method %d\n",
			__func__, bl_pwm->pwm_port, bl_pwm->pwm_method);
		break;
	}

	return out_level;
}

static void bl_set_pwm_vs(struct bl_pwm_config_s *bl_pwm,
		unsigned int pol, unsigned int out_level)
{
	unsigned int pwm_hi, n, sw;
	unsigned int vs[4], ve[4];
	int i;

	if (bl_debug_print_flag) {
		BLPR("%s: pwm_duty=%d, out_level=%d\n",
			__func__, bl_pwm->pwm_duty, out_level);
	}

	if (out_level == 0) {
		for (i = 0; i < 4; i++) {
			vs[i] = 0x1fff;
			ve[i] = 0;
		}
	} else if (out_level == 1) {
		for (i = 0; i < 4; i++) {
			vs[i] = 0;
			ve[i] = 0x1fff;
		}
	} else {
		pwm_hi = bl_pwm->pwm_level;
		n = bl_pwm->pwm_freq;
		sw = (bl_pwm->pwm_cnt * 10 / n + 5) / 10;
		pwm_hi = (pwm_hi * 10 / n + 5) / 10;
		pwm_hi = (pwm_hi > 1) ? pwm_hi : 1;
		if (bl_debug_print_flag) {
			BLPR("pwm_vs: n=%d, sw=%d, pwm_high=%d\n",
				n, sw, pwm_hi);
		}
		for (i = 0; i < n; i++) {
			vs[i] = 1 + (sw * i);
			ve[i] = vs[i] + pwm_hi - 1;
		}
		for (i = n; i < 4; i++) {
			vs[i] = 0xffff;
			ve[i] = 0xffff;
		}
	}
	if (bl_debug_print_flag) {
		for (i = 0; i < 4; i++) {
			BLPR("pwm_vs: vs[%d]=%d, ve[%d]=%d\n",
				i, vs[i], i, ve[i]);
		}
	}

	bl_vcbus_write(VPU_VPU_PWM_V0, (pol << 31) |
			(2 << 14) | /* vsync latch */
			(ve[0] << 16) | (vs[0]));
	bl_vcbus_write(VPU_VPU_PWM_V1, (ve[1] << 16) | (vs[1]));
	bl_vcbus_write(VPU_VPU_PWM_V2, (ve[2] << 16) | (vs[2]));
	bl_vcbus_write(VPU_VPU_PWM_V3, (ve[3] << 16) | (vs[3]));

	if (bl_debug_print_flag) {
		BLPR("VPU_VPU_PWM_V0=0x%08x\n", bl_vcbus_read(VPU_VPU_PWM_V0));
		BLPR("VPU_VPU_PWM_V1=0x%08x\n", bl_vcbus_read(VPU_VPU_PWM_V1));
		BLPR("VPU_VPU_PWM_V2=0x%08x\n", bl_vcbus_read(VPU_VPU_PWM_V2));
		BLPR("VPU_VPU_PWM_V3=0x%08x\n", bl_vcbus_read(VPU_VPU_PWM_V3));
	}
}

static void bl_set_pwm_normal(struct bl_pwm_config_s *bl_pwm,
		unsigned int pol, unsigned int out_level)
{
	unsigned int port_index;

	if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
		BLERR("%s: invalid bl_pwm_ch\n", __func__);
		return;
	}

	port_index = bl_pwm->pwm_data.port_index;
	if (bl_debug_print_flag) {
		pr_info("pwm: pwm=0x%p, port_index=%d, meson_index=%d\n",
		bl_pwm->pwm_data.pwm, port_index, bl_pwm->pwm_data.meson_index);
	}
	if (((port_index % 2) == bl_pwm->pwm_data.meson_index) &&
		(port_index == bl_pwm->pwm_port)) {
		bl_pwm->pwm_data.state.polarity = pol;
		bl_pwm->pwm_data.state.duty_cycle = bl_pwm->pwm_level;
		bl_pwm->pwm_data.state.period =  bl_pwm->pwm_cnt;
		bl_pwm->pwm_data.state.enabled = true;
		if (bl_debug_print_flag) {
			BLPR(
	"pwm state: polarity=%d, duty_cycle=%d, period=%d, enabled=%d\n",
				bl_pwm->pwm_data.state.polarity,
				bl_pwm->pwm_data.state.duty_cycle,
				bl_pwm->pwm_data.state.period,
				bl_pwm->pwm_data.state.enabled);
		}
		if (out_level == 0xff) {
			pwm_constant_disable(bl_pwm->pwm_data.meson,
				bl_pwm->pwm_data.meson_index);
		} else {
			/* pwm duty 100% or 0% special control */
			pwm_constant_enable(bl_pwm->pwm_data.meson,
				bl_pwm->pwm_data.meson_index);
		}
		pwm_apply_state(bl_pwm->pwm_data.pwm,
			&(bl_pwm->pwm_data.state));
	}
}

void bl_pwm_ctrl(struct bl_pwm_config_s *bl_pwm, int status)
{
	struct pwm_state pstate;
	unsigned int pol = 0, out_level = 0xff;

	if (bl_pwm->pwm_method == BL_PWM_NEGATIVE)
		pol = 1;

	if (status) {
		/* enable pwm */
		out_level = bl_pwm_out_level_check(bl_pwm);
		if (bl_debug_print_flag) {
			BLPR("port %d: pwm_duty=%d, out_level=%d, pol=%s\n",
				bl_pwm->pwm_port, bl_pwm->pwm_duty, out_level,
				(pol ? "negative":"positive"));
		}

		switch (bl_pwm->pwm_port) {
		case BL_PWM_A:
		case BL_PWM_B:
		case BL_PWM_C:
		case BL_PWM_D:
		case BL_PWM_E:
		case BL_PWM_F:
			bl_set_pwm_normal(bl_pwm, pol, out_level);
			break;
		case BL_PWM_VS:
			bl_set_pwm_vs(bl_pwm, pol, out_level);
			break;
		default:
			break;
		}
	} else {
		/* disable pwm */
		switch (bl_pwm->pwm_port) {
		case BL_PWM_A:
		case BL_PWM_B:
		case BL_PWM_C:
		case BL_PWM_D:
		case BL_PWM_E:
		case BL_PWM_F:
			if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
				BLERR("%s: invalid bl_pwm_ch\n", __func__);
				return;
			}

			pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
			pwm_constant_enable(bl_pwm->pwm_data.meson,
				bl_pwm->pwm_data.meson_index);
			if (bl_pwm->pwm_method)
				pstate.polarity = 0;
			else
				pstate.polarity = 1;
			pstate.duty_cycle = 0;
			pstate.enabled = 1;
			pstate.period = bl_pwm->pwm_data.state.period;
			if (bl_debug_print_flag) {
				BLPR(
	"pwm state: polarity=%d, duty_cycle=%d, period=%d, enabled=%d\n",
					pstate.polarity, pstate.duty_cycle,
					pstate.period, pstate.enabled);
			}
			pwm_apply_state(bl_pwm->pwm_data.pwm, &(pstate));
			break;
		case BL_PWM_VS:
			bl_set_pwm_vs(bl_pwm, pol, 0);
		default:
			break;
		}
	}
}

static void bl_set_pwm(struct bl_pwm_config_s *bl_pwm)
{
	if (bl_drv->state & BL_STATE_BL_ON) {
		bl_pwm_ctrl(bl_pwm, 1);
	} else {
		if (bl_debug_print_flag)
			BLERR("%s: bl_drv state is off\n", __func__);
	}
}

static void bl_power_en_ctrl(struct bl_config_s *bconf, int status)
{
	if (status) {
		if (bconf->en_gpio < BL_GPIO_NUM_MAX)
			bl_gpio_set(bconf->en_gpio, bconf->en_gpio_on);
	} else {
		if (bconf->en_gpio < BL_GPIO_NUM_MAX)
			bl_gpio_set(bconf->en_gpio, bconf->en_gpio_off);
	}
}

static void bl_power_on(void)
{
	struct bl_config_s *bconf = bl_drv->bconf;
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	struct aml_bl_extern_driver_s *bl_ext;
#endif
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	struct aml_ldim_driver_s *ldim_drv;
#endif
	int ret;

	if (aml_bl_check_driver())
		return;

	/* bl_off_policy */
	if (bl_off_policy != BL_OFF_POLICY_NONE) {
		BLPR("bl_off_policy=%d for bl_off\n", bl_off_policy);
		return;
	}

	mutex_lock(&bl_power_mutex);

	if ((bl_drv->state & BL_STATE_LCD_ON) == 0) {
		BLPR("%s exit, for lcd is off\n", __func__);
		goto exit_power_on_bl;
	}

	if (brightness_bypass == 0) {
		if ((bl_drv->level == 0) ||
			(bl_drv->state & BL_STATE_BL_ON)) {
			goto exit_power_on_bl;
		}
	}

	ret = 0;
	switch (bconf->method) {
	case BL_CTRL_GPIO:
		bl_power_en_ctrl(bconf, 1);
		break;
	case BL_CTRL_PWM:
		if (bconf->en_sequence_reverse) {
			/* step 1: power on enable */
			bl_power_en_ctrl(bconf, 1);
			if (bconf->pwm_on_delay > 0)
				msleep(bconf->pwm_on_delay);
			/* step 2: power on pwm */
			bl_pwm_ctrl(bconf->bl_pwm, 1);
			bl_pwm_pinmux_set(bconf, 1);
		} else {
			/* step 1: power on pwm */
			bl_pwm_ctrl(bconf->bl_pwm, 1);
			bl_pwm_pinmux_set(bconf, 1);
			if (bconf->pwm_on_delay > 0)
				msleep(bconf->pwm_on_delay);
			/* step 2: power on enable */
			bl_power_en_ctrl(bconf, 1);
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (bconf->en_sequence_reverse) {
			/* step 1: power on enable */
			bl_power_en_ctrl(bconf, 1);
			if (bconf->pwm_on_delay > 0)
				msleep(bconf->pwm_on_delay);
			/* step 2: power on pwm_combo */
			bl_pwm_ctrl(bconf->bl_pwm_combo0, 1);
			bl_pwm_ctrl(bconf->bl_pwm_combo1, 1);
			bl_pwm_pinmux_set(bconf, 1);
		} else {
			/* step 1: power on pwm_combo */
			bl_pwm_ctrl(bconf->bl_pwm_combo0, 1);
			bl_pwm_ctrl(bconf->bl_pwm_combo1, 1);
			bl_pwm_pinmux_set(bconf, 1);
			if (bconf->pwm_on_delay > 0)
				msleep(bconf->pwm_on_delay);
			/* step 2: power on enable */
			bl_power_en_ctrl(bconf, 1);
		}
		break;
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	case BL_CTRL_LOCAL_DIMMING:
		ldim_drv = aml_ldim_get_driver();
		if (ldim_drv == NULL) {
			BLERR("no ldim driver\n");
			goto exit_power_on_bl;
		}
		if (bconf->en_sequence_reverse) {
			/* step 1: power on enable */
			bl_power_en_ctrl(bconf, 1);
			/* step 2: power on ldim */
			if (ldim_drv->power_on) {
				ret = ldim_drv->power_on();
				if (ret)
					BLERR("ldim: power on error\n");
			} else {
				BLPR("ldim: power on is null\n");
			}
		} else {
			/* step 1: power on ldim */
			if (ldim_drv->power_on) {
				ret = ldim_drv->power_on();
				if (ret)
					BLERR("ldim: power on error\n");
			} else {
				BLPR("ldim: power on is null\n");
			}
			/* step 2: power on enable */
			bl_power_en_ctrl(bconf, 1);
		}
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		bl_ext = aml_bl_extern_get_driver();
		if (bl_ext == NULL) {
			BLERR("no bl_extern driver\n");
			goto exit_power_on_bl;
		}
		if (bconf->en_sequence_reverse) {
			/* step 1: power on enable */
			bl_power_en_ctrl(bconf, 1);
			/* step 2: power on bl_extern */
			if (bl_ext->power_on) {
				ret = bl_ext->power_on();
				if (ret)
					BLERR("bl_extern: power on error\n");
			} else {
				BLERR("bl_extern: power on is null\n");
			}
		} else {
			/* step 1: power on bl_extern */
			if (bl_ext->power_on) {
				ret = bl_ext->power_on();
				if (ret)
					BLERR("bl_extern: power on error\n");
			} else {
				BLERR("bl_extern: power on is null\n");
			}
			/* step 2: power on enable */
			bl_power_en_ctrl(bconf, 1);
		}
		break;
#endif
	default:
		BLPR("invalid backlight control method\n");
		goto exit_power_on_bl;
	}
	bl_drv->state |= BL_STATE_BL_ON;
	BLPR("backlight power on\n");

exit_power_on_bl:
	mutex_unlock(&bl_power_mutex);
}

static void bl_power_off(void)
{
	struct bl_config_s *bconf = bl_drv->bconf;
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	struct aml_bl_extern_driver_s *bl_ext;
#endif
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	struct aml_ldim_driver_s *ldim_drv;
#endif
	int ret;

	if (aml_bl_check_driver())
		return;
	mutex_lock(&bl_power_mutex);

	if ((bl_drv->state & BL_STATE_BL_ON) == 0) {
		mutex_unlock(&bl_power_mutex);
		return;
	}

	ret = 0;
	switch (bconf->method) {
	case BL_CTRL_GPIO:
		bl_power_en_ctrl(bconf, 0);
		break;
	case BL_CTRL_PWM:
		if (bconf->en_sequence_reverse) {
			/* step 1: power off pwm */
			bl_pwm_pinmux_set(bconf, 0);
			bl_pwm_ctrl(bconf->bl_pwm, 0);
			if (bconf->pwm_off_delay > 0)
				msleep(bconf->pwm_off_delay);
			/* step 2: power off enable */
			bl_power_en_ctrl(bconf, 0);
		} else {
			/* step 1: power off enable */
			bl_power_en_ctrl(bconf, 0);
			/* step 2: power off pwm */
			if (bconf->pwm_off_delay > 0)
				msleep(bconf->pwm_off_delay);
			bl_pwm_pinmux_set(bconf, 0);
			bl_pwm_ctrl(bconf->bl_pwm, 0);
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (bconf->en_sequence_reverse) {
			/* step 1: power off pwm_combo */
			bl_pwm_pinmux_set(bconf, 0);
			bl_pwm_ctrl(bconf->bl_pwm_combo0, 0);
			bl_pwm_ctrl(bconf->bl_pwm_combo1, 0);
			if (bconf->pwm_off_delay > 0)
				msleep(bconf->pwm_off_delay);
			/* step 2: power off enable */
			bl_power_en_ctrl(bconf, 0);
		} else {
			/* step 1: power off enable */
			bl_power_en_ctrl(bconf, 0);
			/* step 2: power off pwm_combo */
			if (bconf->pwm_off_delay > 0)
				msleep(bconf->pwm_off_delay);
			bl_pwm_pinmux_set(bconf, 0);
			bl_pwm_ctrl(bconf->bl_pwm_combo0, 0);
			bl_pwm_ctrl(bconf->bl_pwm_combo1, 0);
		}
		break;
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	case BL_CTRL_LOCAL_DIMMING:
		ldim_drv = aml_ldim_get_driver();
		if (ldim_drv == NULL) {
			BLERR("no ldim driver\n");
			goto exit_power_off_bl;
		}
		if (bconf->en_sequence_reverse) {
			/* step 1: power off ldim */
			if (ldim_drv->power_off) {
				ret = ldim_drv->power_off();
				if (ret)
					BLERR("ldim: power off error\n");
			} else {
				BLERR("ldim: power off is null\n");
			}
			/* step 2: power off enable */
			bl_power_en_ctrl(bconf, 0);
		} else {
			/* step 1: power off enable */
			bl_power_en_ctrl(bconf, 0);
			/* step 2: power off ldim */
			if (ldim_drv->power_off) {
				ret = ldim_drv->power_off();
				if (ret)
					BLERR("ldim: power off error\n");
			} else {
				BLERR("ldim: power off is null\n");
			}
		}
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		bl_ext = aml_bl_extern_get_driver();
		if (bl_ext == NULL) {
			BLERR("no bl_extern driver\n");
			goto exit_power_off_bl;
		}
		if (bconf->en_sequence_reverse) {
			/* step 1: power off bl_extern */
			if (bl_ext->power_off) {
				ret = bl_ext->power_off();
				if (ret)
					BLERR("bl_extern: power off error\n");
			} else {
				BLERR("bl_extern: power off is null\n");
			}
			/* step 2: power off enable */
			bl_power_en_ctrl(bconf, 0);
		} else {
			/* step 1: power off enable */
			bl_power_en_ctrl(bconf, 0);
			/* step 2: power off bl_extern */
			if (bl_ext->power_off) {
				ret = bl_ext->power_off();
				if (ret)
					BLERR("bl_extern: power off error\n");
			} else {
				BLERR("bl_extern: power off is null\n");
			}
		}
		break;
#endif
	default:
		BLPR("invalid backlight control method\n");
		break;
	}
	if (bconf->power_off_delay > 0)
		msleep(bconf->power_off_delay);

	bl_drv->state &= ~BL_STATE_BL_ON;
	BLPR("backlight power off\n");

exit_power_off_bl:
	mutex_unlock(&bl_power_mutex);
}

static unsigned int bl_level_mapping(unsigned int level)
{
	unsigned int mid = bl_drv->bconf->level_mid;
	unsigned int mid_map = bl_drv->bconf->level_mid_mapping;
	unsigned int max = bl_drv->bconf->level_max;
	unsigned int min = bl_drv->bconf->level_min;

	if (mid == mid_map)
		return level;

	level = level > max ? max : level;
	if ((level >= mid) && (level <= max)) {
		level = (((level - mid) * (max - mid_map)) / (max - mid)) +
			mid_map;
	} else if ((level >= min) && (level < mid)) {
		level = (((level - min) * (mid_map - min)) / (mid - min)) + min;
	} else {
		level = 0;
	}
	return level;
}


static void bl_set_duty_pwm(struct bl_pwm_config_s *bl_pwm)
{
	unsigned long temp;

	if (bl_pwm_bypass)
		return;

	if (bl_pwm_duty_free) {
		if (bl_pwm->pwm_duty_max > 100) {
			if (bl_pwm->pwm_duty > 255) {
				BLERR(
			"pwm_duty %d is bigger than 255, reset to 255\n",
					bl_pwm->pwm_duty);
				bl_pwm->pwm_duty = 255;
			}
		} else {
			if (bl_pwm->pwm_duty > 100) {
				BLERR(
			"pwm_duty %d%% is bigger than 100%%, reset to 100%%\n",
					bl_pwm->pwm_duty);
				bl_pwm->pwm_duty = 100;
			}
		}
	} else {
		if (bl_pwm->pwm_duty > bl_pwm->pwm_duty_max) {
			BLERR(
		"pwm_duty %d is bigger than duty_max %d, reset to duty_max\n",
				bl_pwm->pwm_duty, bl_pwm->pwm_duty_max);
			bl_pwm->pwm_duty = bl_pwm->pwm_duty_max;
		} else if (bl_pwm->pwm_duty < bl_pwm->pwm_duty_min) {
			BLERR(
		"pwm_duty %d is smaller than duty_min %d, reset to duty_min\n",
				bl_pwm->pwm_duty, bl_pwm->pwm_duty_min);
			bl_pwm->pwm_duty = bl_pwm->pwm_duty_min;
		}
	}

	temp = bl_pwm->pwm_cnt;
	if (bl_pwm->pwm_duty_max > 100) {
		bl_pwm->pwm_level =
			bl_do_div(((temp * bl_pwm->pwm_duty) + 127), 255);
	} else {
		bl_pwm->pwm_level =
			bl_do_div(((temp * bl_pwm->pwm_duty) + 50), 100);
	}

	if (bl_debug_print_flag) {
		if (bl_pwm->pwm_duty_max > 100) {
			BLPR("pwm port %d: duty=%d, duty_max=%d, duty_min=%d\n",
				bl_pwm->pwm_port, bl_pwm->pwm_duty,
				bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min);
		} else {
			BLPR(
		"pwm port %d: duty=%d%%, duty_max=%d%%, duty_min=%d%%\n",
				bl_pwm->pwm_port, bl_pwm->pwm_duty,
				bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min);
		}
		BLPR("pwm port %d: pwm_max=%d, pwm_min=%d, pwm_level=%d\n",
			bl_pwm->pwm_port,
			bl_pwm->pwm_max, bl_pwm->pwm_min, bl_pwm->pwm_level);
	}
	bl_set_pwm(bl_pwm);
}

static void bl_set_level_pwm(struct bl_pwm_config_s *bl_pwm, unsigned int level)
{
	unsigned int min = bl_pwm->level_min;
	unsigned int max = bl_pwm->level_max;
	unsigned int pwm_max = bl_pwm->pwm_max;
	unsigned int pwm_min = bl_pwm->pwm_min;
	unsigned long temp;

	if (bl_pwm_bypass)
		return;

	level = bl_level_mapping(level);
	max = bl_level_mapping(max);
	min = bl_level_mapping(min);
	if ((max <= min) || (level < min)) {
		bl_pwm->pwm_level = pwm_min;
	} else {
		temp = pwm_max - pwm_min;
		bl_pwm->pwm_level =
			bl_do_div((temp * (level - min)), (max - min)) +
				pwm_min;
	}
	temp = bl_pwm->pwm_level;
	if (bl_pwm->pwm_duty_max > 100) {
		bl_pwm->pwm_duty =
			(bl_do_div((temp * 2550), bl_pwm->pwm_cnt) + 5) / 10;
	} else {
		bl_pwm->pwm_duty =
			(bl_do_div((temp * 1000), bl_pwm->pwm_cnt) + 5) / 10;
	}

	if (bl_debug_print_flag) {
		BLPR("pwm port %d: level=%d, level_max=%d, level_min=%d\n",
			bl_pwm->pwm_port, level, max, min);
		if (bl_pwm->pwm_duty_max > 100) {
			BLPR(
	"pwm port %d: duty=%d(%d%%), pwm_max=%d, pwm_min=%d, pwm_level=%d\n",
				bl_pwm->pwm_port, bl_pwm->pwm_duty,
				bl_pwm->pwm_duty*100/255,
				bl_pwm->pwm_max, bl_pwm->pwm_min,
				bl_pwm->pwm_level);
		} else {
			BLPR(
	"pwm port %d: duty=%d%%, pwm_max=%d, pwm_min=%d, pwm_level=%d\n",
				bl_pwm->pwm_port, bl_pwm->pwm_duty,
				bl_pwm->pwm_max, bl_pwm->pwm_min,
				bl_pwm->pwm_level);
		}
	}

	bl_set_pwm(bl_pwm);
}

#ifdef CONFIG_AMLOGIC_BL_EXTERN
static void bl_set_level_extern(unsigned int level)
{
	struct aml_bl_extern_driver_s *bl_ext;
	int ret;

	bl_ext = aml_bl_extern_get_driver();
	if (bl_ext == NULL) {
		BLERR("no bl_extern driver\n");
	} else {
		if (bl_ext->set_level) {
			ret = bl_ext->set_level(level);
			if (ret)
				BLERR("bl_extern: set_level error\n");
		} else {
			BLERR("bl_extern: set_level is null\n");
		}
	}
}
#endif

#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
static void bl_set_level_ldim(unsigned int level)
{
	struct aml_ldim_driver_s *ldim_drv;
	int ret = 0;

	ldim_drv = aml_ldim_get_driver();
	if (ldim_drv == NULL) {
		BLERR("no ldim driver\n");
	} else {
		if (ldim_drv->set_level) {
			ret = ldim_drv->set_level(level);
			if (ret)
				BLERR("ldim: set_level error\n");
		} else {
			BLERR("ldim: set_level is null\n");
		}
	}
}
#endif

static void aml_bl_set_level(unsigned int level)
{
	unsigned int temp;
	struct bl_pwm_config_s *pwm0, *pwm1;

	if (aml_bl_check_driver())
		return;

	if (bl_debug_print_flag) {
		BLPR("aml_bl_set_level=%u, last_level=%u, state=0x%x\n",
			level, bl_drv->level, bl_drv->state);
	}

	/* level range check */
	if (level > bl_drv->bconf->level_max)
		level = bl_drv->bconf->level_max;
	if (level < bl_drv->bconf->level_min) {
		if (level < BL_LEVEL_OFF)
			level = 0;
		else
			level = bl_drv->bconf->level_min;
	}
	bl_drv->level = level;

	if (level == 0)
		return;

	switch (bl_drv->bconf->method) {
	case BL_CTRL_GPIO:
		break;
	case BL_CTRL_PWM:
		bl_set_level_pwm(bl_drv->bconf->bl_pwm, level);
		break;
	case BL_CTRL_PWM_COMBO:
		pwm0 = bl_drv->bconf->bl_pwm_combo0;
		pwm1 = bl_drv->bconf->bl_pwm_combo1;
		if ((level >= pwm0->level_min) &&
			(level <= pwm0->level_max)) {
			temp = (pwm0->level_min > pwm1->level_min) ?
				pwm1->level_max : pwm1->level_min;
			if (bl_debug_print_flag) {
				BLPR("pwm0 region, level=%u, pwm1_level=%u\n",
					level, temp);
			}
			bl_set_level_pwm(pwm0, level);
			bl_set_level_pwm(pwm1, temp);
		} else if ((level >= pwm1->level_min) &&
			(level <= pwm1->level_max)) {
			temp = (pwm1->level_min > pwm0->level_min) ?
				pwm0->level_max : pwm0->level_min;
			if (bl_debug_print_flag) {
				BLPR("pwm1 region, level=%u, pwm0_level=%u\n",
					level, temp);
			}
			bl_set_level_pwm(pwm0, temp);
			bl_set_level_pwm(pwm1, level);
		}
		break;
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	case BL_CTRL_LOCAL_DIMMING:
		bl_set_level_ldim(level);
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		bl_set_level_extern(level);
		break;
#endif
	default:
		if (bl_debug_print_flag)
			BLPR("invalid backlight control method\n");
		break;
	}
}

static unsigned int aml_bl_get_level(void)
{
	if (aml_bl_check_driver())
		return 0;

	BLPR("aml bl state: 0x%x\n", bl_drv->state);
	return bl_drv->level;
}

static int aml_bl_update_status(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;

	if (brightness_bypass)
		return 0;

	mutex_lock(&bl_level_mutex);
	if (brightness < 0)
		brightness = 0;
	else if (brightness > 255)
		brightness = 255;

	if (((bl_drv->state & BL_STATE_LCD_ON) == 0) ||
		((bl_drv->state & BL_STATE_BL_POWER_ON) == 0))
		brightness = 0;

	if (bl_debug_print_flag) {
		BLPR("%s: %u, real brightness: %u, state: 0x%x\n",
			__func__, bd->props.brightness,
			brightness, bl_drv->state);
	}
	if (brightness == 0) {
		if (bl_drv->state & BL_STATE_BL_ON)
			bl_power_off();
	} else {
		aml_bl_set_level(brightness);
		if ((bl_drv->state & BL_STATE_BL_ON) == 0)
			bl_power_on();
	}
	mutex_unlock(&bl_level_mutex);
	return 0;
}

static int aml_bl_get_brightness(struct backlight_device *bd)
{
	return aml_bl_get_level();
}

static const struct backlight_ops aml_bl_ops = {
	.get_brightness = aml_bl_get_brightness,
	.update_status  = aml_bl_update_status,
};

#ifdef CONFIG_OF
static char *bl_pwm_name[] = {
	"PWM_A",
	"PWM_B",
	"PWM_C",
	"PWM_D",
	"PWM_E",
	"PWM_F",
	"PWM_VS",
	"invalid",
};

enum bl_pwm_port_e bl_pwm_str_to_pwm(const char *str)
{
	enum bl_pwm_port_e pwm_port = BL_PWM_MAX;
	int i;

	for (i = 0; i < ARRAY_SIZE(bl_pwm_name); i++) {
		if (strcmp(str, bl_pwm_name[i]) == 0) {
			pwm_port = i;
			break;
		}
	}

	return pwm_port;
}

void bl_pwm_config_init(struct bl_pwm_config_s *bl_pwm)
{
	unsigned int cnt;
	unsigned long temp;

	if (bl_debug_print_flag) {
		BLPR("%s pwm_port %d: freq = %u\n",
			__func__, bl_pwm->pwm_port, bl_pwm->pwm_freq);
	}

	switch (bl_pwm->pwm_port) {
	case BL_PWM_VS:
		cnt = bl_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;
		bl_pwm->pwm_cnt = cnt;
		break;
	default:
		/* for pwm api: pwm_period */
		bl_pwm->pwm_cnt = 1000000000 / bl_pwm->pwm_freq;
		break;
	}

	temp = bl_pwm->pwm_cnt;
	if (bl_pwm->pwm_duty_max > 100) {
		bl_pwm->pwm_max = bl_do_div((temp * bl_pwm->pwm_duty_max), 255);
		bl_pwm->pwm_min = bl_do_div((temp * bl_pwm->pwm_duty_min), 255);
	} else {
		bl_pwm->pwm_max = bl_do_div((temp * bl_pwm->pwm_duty_max), 100);
		bl_pwm->pwm_min = bl_do_div((temp * bl_pwm->pwm_duty_min), 100);
	}

	if (bl_debug_print_flag) {
		BLPR("pwm_cnt = %u, pwm_max = %u, pwm_min = %u\n",
			bl_pwm->pwm_cnt, bl_pwm->pwm_max, bl_pwm->pwm_min);
	}
}

static void aml_bl_config_print(struct bl_config_s *bconf)
{
	struct bl_pwm_config_s *bl_pwm;

	if (bconf->method == BL_CTRL_MAX) {
		BLPR("no backlight exist\n");
		return;
	}

	BLPR("name              = %s\n", bconf->name);
	BLPR("method            = %s(%d)\n",
		bl_method_type_to_str(bconf->method), bconf->method);

	if (bl_debug_print_flag == 0)
		return;

	BLPR("level_default     = %d\n", bconf->level_default);
	BLPR("level_min         = %d\n", bconf->level_min);
	BLPR("level_max         = %d\n", bconf->level_max);
	BLPR("level_mid         = %d\n", bconf->level_mid);
	BLPR("level_mid_mapping = %d\n", bconf->level_mid_mapping);

	BLPR("en_gpio           = %d\n", bconf->en_gpio);
	BLPR("en_gpio_on        = %d\n", bconf->en_gpio_on);
	BLPR("en_gpio_off       = %d\n", bconf->en_gpio_off);
	BLPR("power_on_delay    = %dms\n", bconf->power_on_delay);
	BLPR("power_off_delay   = %dms\n\n", bconf->power_off_delay);

	switch (bconf->method) {
	case BL_CTRL_PWM:
		BLPR("pwm_on_delay        = %dms\n", bconf->pwm_on_delay);
		BLPR("pwm_off_delay       = %dms\n", bconf->pwm_off_delay);
		BLPR("en_sequence_reverse = %d\n",
			bconf->en_sequence_reverse);
		if (bconf->bl_pwm) {
			bl_pwm = bconf->bl_pwm;
			BLPR("pwm_index     = %d\n", bl_pwm->index);
			BLPR("pwm_port      = %d\n", bl_pwm->pwm_port);
			BLPR("pwm_method    = %d\n", bl_pwm->pwm_method);
			if (bl_pwm->pwm_port == BL_PWM_VS) {
				BLPR("pwm_freq      = %d x vfreq\n",
					bl_pwm->pwm_freq);
			} else {
				BLPR("pwm_freq      = %uHz\n",
					bl_pwm->pwm_freq);
			}
			BLPR("pwm_level_max = %u\n", bl_pwm->level_max);
			BLPR("pwm_level_min = %u\n", bl_pwm->level_min);
			BLPR("pwm_duty_max  = %d\n", bl_pwm->pwm_duty_max);
			BLPR("pwm_duty_min  = %d\n", bl_pwm->pwm_duty_min);
			BLPR("pwm_cnt       = %u\n", bl_pwm->pwm_cnt);
			BLPR("pwm_max       = %u\n", bl_pwm->pwm_max);
			BLPR("pwm_min       = %u\n", bl_pwm->pwm_min);
		}
		break;
	case BL_CTRL_PWM_COMBO:
		BLPR("pwm_on_delay        = %dms\n", bconf->pwm_on_delay);
		BLPR("pwm_off_delay       = %dms\n", bconf->pwm_off_delay);
		BLPR("en_sequence_reverse = %d\n",
			bconf->en_sequence_reverse);
		/* pwm_combo_0 */
		if (bconf->bl_pwm_combo0) {
			bl_pwm = bconf->bl_pwm_combo0;
			BLPR("pwm_combo0_index     = %d\n", bl_pwm->index);
			BLPR("pwm_combo0_port      = %d\n", bl_pwm->pwm_port);
			BLPR("pwm_combo0_method    = %d\n", bl_pwm->pwm_method);
			if (bl_pwm->pwm_port == BL_PWM_VS) {
				BLPR("pwm_combo0_freq      = %d x vfreq\n",
					bl_pwm->pwm_freq);
			} else {
				BLPR("pwm_combo0_freq      = %uHz\n",
					bl_pwm->pwm_freq);
			}
			BLPR("pwm_combo0_level_max = %u\n", bl_pwm->level_max);
			BLPR("pwm_combo0_level_min = %u\n", bl_pwm->level_min);
			BLPR("pwm_combo0_duty_max  = %d\n",
				bl_pwm->pwm_duty_max);
			BLPR("pwm_combo0_duty_min  = %d\n",
				bl_pwm->pwm_duty_min);
			BLPR("pwm_combo0_pwm_cnt   = %u\n", bl_pwm->pwm_cnt);
			BLPR("pwm_combo0_pwm_max   = %u\n", bl_pwm->pwm_max);
			BLPR("pwm_combo0_pwm_min   = %u\n", bl_pwm->pwm_min);
		}
		/* pwm_combo_1 */
		if (bconf->bl_pwm_combo1) {
			bl_pwm = bconf->bl_pwm_combo1;
			BLPR("pwm_combo1_index     = %d\n", bl_pwm->index);
			BLPR("pwm_combo1_port      = %d\n", bl_pwm->pwm_port);
			BLPR("pwm_combo1_method    = %d\n", bl_pwm->pwm_method);
			if (bl_pwm->pwm_port == BL_PWM_VS) {
				BLPR("pwm_combo1_freq      = %d x vfreq\n",
					bl_pwm->pwm_freq);
			} else {
				BLPR("pwm_combo1_freq      = %uHz\n",
					bl_pwm->pwm_freq);
			}
			BLPR("pwm_combo1_level_max = %u\n", bl_pwm->level_max);
			BLPR("pwm_combo1_level_min = %u\n", bl_pwm->level_min);
			BLPR("pwm_combo1_duty_max  = %d\n",
				bl_pwm->pwm_duty_max);
			BLPR("pwm_combo1_duty_min  = %d\n",
				bl_pwm->pwm_duty_min);
			BLPR("pwm_combo1_pwm_cnt   = %u\n", bl_pwm->pwm_cnt);
			BLPR("pwm_combo1_pwm_max   = %u\n", bl_pwm->pwm_max);
			BLPR("pwm_combo1_pwm_min   = %u\n", bl_pwm->pwm_min);
		}
		break;
	default:
		break;
	}
}

static int aml_bl_config_load_from_dts(struct bl_config_s *bconf,
		struct platform_device *pdev)
{
	int ret = 0;
	int val;
	const char *str;
	unsigned int bl_para[10];
	char bl_propname[20];
	int index = BL_INDEX_DEFAULT;
	struct device_node *child;
	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *pwm_combo0, *pwm_combo1;

	/* select backlight by index */
#ifdef CONFIG_AMLOGIC_LCD
	aml_lcd_notifier_call_chain(LCD_EVENT_BACKLIGHT_SEL, &index);
#endif
	bl_drv->index = index;
	if (bl_drv->index == 0xff) {
		bconf->method = BL_CTRL_MAX;
		return -1;
	}
	sprintf(bl_propname, "backlight_%d", index);
	BLPR("load: %s\n", bl_propname);
	child = of_get_child_by_name(pdev->dev.of_node, bl_propname);
	if (child == NULL) {
		BLERR("failed to get %s\n", bl_propname);
		return -1;
	}

	ret = of_property_read_string(child, "bl_name", &str);
	if (ret) {
		BLERR("failed to get bl_name\n");
		str = "backlight";
	}
	strncpy(bconf->name, str, BL_NAME_MAX);
	/* ensure string ending */
	bconf->name[BL_NAME_MAX-1] = '\0';

	ret = of_property_read_u32_array(child,
		"bl_level_default_uboot_kernel", &bl_para[0], 2);
	if (ret) {
		BLERR("failed to get bl_level_default_uboot_kernel\n");
		bl_level_uboot = BL_LEVEL_DEFAULT;
		bconf->level_default = BL_LEVEL_DEFAULT;
	} else {
		bl_level_uboot = bl_para[0];
		bconf->level_default = bl_para[1];
	}

	ret = of_property_read_u32_array(child, "bl_level_attr",
		&bl_para[0], 4);
	if (ret) {
		BLERR("failed to get bl_level_attr\n");
		bconf->level_min = BL_LEVEL_MIN;
		bconf->level_max = BL_LEVEL_MAX;
		bconf->level_mid = BL_LEVEL_MID;
		bconf->level_mid_mapping = BL_LEVEL_MID_MAPPED;
	} else {
		bconf->level_max = bl_para[0];
		bconf->level_min = bl_para[1];
		bconf->level_mid = bl_para[2];
		bconf->level_mid_mapping = bl_para[3];
	}
	/* adjust brightness_bypass by level_default */
	if (bconf->level_default > bconf->level_max) {
		brightness_bypass = 1;
		BLPR("level_default > level_max, enable brightness_bypass\n");
	}

	ret = of_property_read_u32(child, "bl_ctrl_method", &val);
	if (ret) {
		BLERR("failed to get bl_ctrl_method\n");
		bconf->method = BL_CTRL_MAX;
	} else {
		bconf->method = (val >= BL_CTRL_MAX) ? BL_CTRL_MAX : val;
	}
	ret = of_property_read_u32_array(child, "bl_power_attr",
		&bl_para[0], 5);
	if (ret) {
		BLERR("failed to get bl_power_attr\n");
		bconf->en_gpio = BL_GPIO_MAX;
		bconf->en_gpio_on = BL_GPIO_OUTPUT_HIGH;
		bconf->en_gpio_off = BL_GPIO_OUTPUT_LOW;
		bconf->power_on_delay = 100;
		bconf->power_off_delay = 30;
	} else {
		if (bl_para[0] >= BL_GPIO_NUM_MAX) {
			bconf->en_gpio = BL_GPIO_MAX;
		} else {
			bconf->en_gpio = bl_para[0];
			bl_gpio_probe(bconf->en_gpio);
		}
		bconf->en_gpio_on = bl_para[1];
		bconf->en_gpio_off = bl_para[2];
		bconf->power_on_delay = bl_para[3];
		bconf->power_off_delay = bl_para[4];
	}
	ret = of_property_read_u32(child, "bl_ldim_mode", &bl_para[0]);
	if (ret == 0)
		bconf->ldim_flag = 1;

	switch (bconf->method) {
	case BL_CTRL_PWM:
		bconf->bl_pwm = kzalloc(sizeof(struct bl_pwm_config_s),
				GFP_KERNEL);
		if (bconf->bl_pwm == NULL) {
			BLERR("bl_pwm struct malloc error\n");
			return -1;
		}
		bl_pwm = bconf->bl_pwm;
		bl_pwm->index = 0;

		bl_pwm->level_max = bconf->level_max;
		bl_pwm->level_min = bconf->level_min;

		ret = of_property_read_string(child, "bl_pwm_port", &str);
		if (ret) {
			BLERR("failed to get bl_pwm_port\n");
			bl_pwm->pwm_port = BL_PWM_MAX;
		} else {
			bl_pwm->pwm_port = bl_pwm_str_to_pwm(str);
			BLPR("bl pwm_port: %s(%u)\n", str, bl_pwm->pwm_port);
		}
		ret = of_property_read_u32_array(child, "bl_pwm_attr",
			&bl_para[0], 4);
		if (ret) {
			BLERR("failed to get bl_pwm_attr\n");
			bl_pwm->pwm_method = BL_PWM_POSITIVE;
			if (bl_pwm->pwm_port == BL_PWM_VS)
				bl_pwm->pwm_freq = BL_FREQ_VS_DEFAULT;
			else
				bl_pwm->pwm_freq = BL_FREQ_DEFAULT;
			bl_pwm->pwm_duty_max = 80;
			bl_pwm->pwm_duty_min = 20;
		} else {
			bl_pwm->pwm_method = bl_para[0];
			bl_pwm->pwm_freq = bl_para[1];
			bl_pwm->pwm_duty_max = bl_para[2];
			bl_pwm->pwm_duty_min = bl_para[3];
		}
		if (bl_pwm->pwm_port == BL_PWM_VS) {
			if (bl_pwm->pwm_freq > 4) {
				BLERR("bl_pwm_vs wrong freq %d\n",
					bl_pwm->pwm_freq);
				bl_pwm->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (bl_pwm->pwm_freq > XTAL_HALF_FREQ_HZ)
				bl_pwm->pwm_freq = XTAL_HALF_FREQ_HZ;
			if (bl_pwm->pwm_freq < 50)
				bl_pwm->pwm_freq = 50;
		}
		ret = of_property_read_u32_array(child, "bl_pwm_power",
			&bl_para[0], 4);
		if (ret) {
			BLERR("failed to get bl_pwm_power\n");
			bconf->pwm_on_delay = 0;
			bconf->pwm_off_delay = 0;
		} else {
			bconf->pwm_on_delay = bl_para[2];
			bconf->pwm_off_delay = bl_para[3];
		}
		ret = of_property_read_u32(child,
			"bl_pwm_en_sequence_reverse", &val);
		if (ret) {
			BLPR("don't find bl_pwm_en_sequence_reverse\n");
			bconf->en_sequence_reverse = 0;
		} else
			bconf->en_sequence_reverse = val;

		bl_pwm->pwm_duty = bl_pwm->pwm_duty_min;
		/* init pwm config */
		bl_pwm_config_init(bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		bconf->bl_pwm_combo0 = kzalloc(sizeof(struct bl_pwm_config_s),
				GFP_KERNEL);
		if (bconf->bl_pwm_combo0 == NULL) {
			BLERR("bl_pwm_combo0 struct malloc error\n");
			return -1;
		}
		bconf->bl_pwm_combo1 = kzalloc(sizeof(struct bl_pwm_config_s),
				GFP_KERNEL);
		if (bconf->bl_pwm_combo1 == NULL) {
			BLERR("bl_pwm_combo1 struct malloc error\n");
			return -1;
		}
		pwm_combo0 = bconf->bl_pwm_combo0;
		pwm_combo1 = bconf->bl_pwm_combo1;

		pwm_combo0->index = 0;
		pwm_combo1->index = 1;

		ret = of_property_read_string_index(child, "bl_pwm_combo_port",
			0, &str);
		if (ret) {
			BLERR("failed to get bl_pwm_combo_port\n");
			pwm_combo0->pwm_port = BL_PWM_MAX;
		} else {
			pwm_combo0->pwm_port = bl_pwm_str_to_pwm(str);
		}
		ret = of_property_read_string_index(child, "bl_pwm_combo_port",
			1, &str);
		if (ret) {
			BLERR("failed to get bl_pwm_combo_port\n");
			pwm_combo1->pwm_port = BL_PWM_MAX;
		} else {
			pwm_combo1->pwm_port = bl_pwm_str_to_pwm(str);
		}
		BLPR("pwm_combo_port: %s(%u), %s(%u)\n",
			bl_pwm_name[pwm_combo0->pwm_port], pwm_combo0->pwm_port,
			bl_pwm_name[pwm_combo1->pwm_port],
			pwm_combo1->pwm_port);
		ret = of_property_read_u32_array(child,
			"bl_pwm_combo_level_mapping", &bl_para[0], 4);
		if (ret) {
			BLERR("failed to get bl_pwm_combo_level_mapping\n");
			pwm_combo0->level_max = BL_LEVEL_MAX;
			pwm_combo0->level_min = BL_LEVEL_MID;
			pwm_combo1->level_max = BL_LEVEL_MID;
			pwm_combo1->level_min = BL_LEVEL_MIN;
		} else {
			pwm_combo0->level_max = bl_para[0];
			pwm_combo0->level_min = bl_para[1];
			pwm_combo1->level_max = bl_para[2];
			pwm_combo1->level_min = bl_para[3];
		}
		ret = of_property_read_u32_array(child, "bl_pwm_combo_attr",
			&bl_para[0], 8);
		if (ret) {
			BLERR("failed to get bl_pwm_combo_attr\n");
			pwm_combo0->pwm_method = BL_PWM_POSITIVE;
			if (pwm_combo0->pwm_port == BL_PWM_VS)
				pwm_combo0->pwm_freq = BL_FREQ_VS_DEFAULT;
			else
				pwm_combo0->pwm_freq = BL_FREQ_DEFAULT;
			pwm_combo0->pwm_duty_max = 80;
			pwm_combo0->pwm_duty_min = 20;
			pwm_combo1->pwm_method = BL_PWM_NEGATIVE;
			if (pwm_combo1->pwm_port == BL_PWM_VS)
				pwm_combo1->pwm_freq = BL_FREQ_VS_DEFAULT;
			else
				pwm_combo1->pwm_freq = BL_FREQ_DEFAULT;
			pwm_combo1->pwm_duty_max = 80;
			pwm_combo1->pwm_duty_min = 20;
		} else {
			pwm_combo0->pwm_method = bl_para[0];
			pwm_combo0->pwm_freq = bl_para[1];
			pwm_combo0->pwm_duty_max = bl_para[2];
			pwm_combo0->pwm_duty_min = bl_para[3];
			pwm_combo1->pwm_method = bl_para[4];
			pwm_combo1->pwm_freq = bl_para[5];
			pwm_combo1->pwm_duty_max = bl_para[6];
			pwm_combo1->pwm_duty_min = bl_para[7];
		}
		if (pwm_combo0->pwm_port == BL_PWM_VS) {
			if (pwm_combo0->pwm_freq > 4) {
				BLERR("bl_pwm_0_vs wrong freq %d\n",
					pwm_combo0->pwm_freq);
				pwm_combo0->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (pwm_combo0->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo0->pwm_freq = XTAL_HALF_FREQ_HZ;
			if (pwm_combo0->pwm_freq < 50)
				pwm_combo0->pwm_freq = 50;
		}
		if (pwm_combo1->pwm_port == BL_PWM_VS) {
			if (pwm_combo1->pwm_freq > 4) {
				BLERR("bl_pwm_1_vs wrong freq %d\n",
					pwm_combo1->pwm_freq);
				pwm_combo1->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (pwm_combo1->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo1->pwm_freq = XTAL_HALF_FREQ_HZ;
			if (pwm_combo1->pwm_freq < 50)
				pwm_combo1->pwm_freq = 50;
		}
		ret = of_property_read_u32_array(child, "bl_pwm_combo_power",
			&bl_para[0], 6);
		if (ret) {
			BLERR("failed to get bl_pwm_combo_power\n");
			bconf->pwm_on_delay = 0;
			bconf->pwm_off_delay = 0;
		} else {
			bconf->pwm_on_delay = bl_para[4];
			bconf->pwm_off_delay = bl_para[5];
		}
		ret = of_property_read_u32(child,
			"bl_pwm_en_sequence_reverse", &val);
		if (ret) {
			BLPR("don't find bl_pwm_en_sequence_reverse\n");
			bconf->en_sequence_reverse = 0;
		} else
			bconf->en_sequence_reverse = val;

		pwm_combo0->pwm_duty = pwm_combo0->pwm_duty_min;
		pwm_combo1->pwm_duty = pwm_combo1->pwm_duty_min;
		/* init pwm config */
		bl_pwm_config_init(pwm_combo0);
		bl_pwm_config_init(pwm_combo1);
		break;
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	case BL_CTRL_LOCAL_DIMMING:
		bconf->ldim_flag = 1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		/* get bl_extern_index from dts */
		ret = of_property_read_u32(child, "bl_extern_index",
			&bl_para[0]);
		if (ret) {
			BLERR("failed to get bl_extern_index\n");
		} else {
			bconf->bl_extern_index = bl_para[0];
			BLPR("get bl_extern_index = %d\n",
				bconf->bl_extern_index);
		}
		break;
#endif
	default:
		break;
	}

#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	if (bconf->ldim_flag) {
		ret = aml_ldim_get_config_dts(child);
		if (ret < 0)
			return -1;
	}
#endif

	return 0;
}
#endif

static int aml_bl_config_load_from_unifykey(struct bl_config_s *bconf)
{
	unsigned char *para;
	int key_len, len;
	unsigned char *p;
	const char *str;
	unsigned char temp;
	struct aml_lcd_unifykey_header_s bl_header;
	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *pwm_combo0, *pwm_combo1;
	int ret;

	para = kmalloc((sizeof(unsigned char) * LCD_UKEY_BL_SIZE), GFP_KERNEL);
	if (!para) {
		BLERR("%s: Not enough memory\n", __func__);
		return -1;
	}

	key_len = LCD_UKEY_BL_SIZE;
	memset(para, 0, (sizeof(unsigned char) * key_len));
	ret = lcd_unifykey_get("backlight", para, &key_len);
	if (ret < 0) {
		kfree(para);
		return -1;
	}

	/* step 1: check header */
	len = LCD_UKEY_HEAD_SIZE;
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		BLERR("unifykey header length is incorrect\n");
		kfree(para);
		return -1;
	}

	lcd_unifykey_header_check(para, &bl_header);
	BLPR("unifykey version: 0x%04x\n", bl_header.version);
	switch (bl_header.version) {
	case 2:
		len = 10 + 30 + 12 + 8 + 32 + 10;
		break;
	default:
		len = 10 + 30 + 12 + 8 + 32;
		break;
	}
	if (bl_debug_print_flag) {
		BLPR("unifykey header:\n");
		BLPR("crc32             = 0x%08x\n", bl_header.crc32);
		BLPR("data_len          = %d\n", bl_header.data_len);
		BLPR("reserved          = 0x%04x\n", bl_header.reserved);
	}

	/* step 2: check backlight parameters */
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		BLERR("unifykey length is incorrect\n");
		kfree(para);
		return -1;
	}

	/* basic: 30byte */
	p = para;
	str = (const char *)(p + LCD_UKEY_HEAD_SIZE);
	strncpy(bconf->name, str, BL_NAME_MAX);
	/* ensure string ending */
	bconf->name[BL_NAME_MAX-1] = '\0';

	/* level: 6byte */
	bl_level_uboot = (*(p + LCD_UKEY_BL_LEVEL_UBOOT) |
		((*(p + LCD_UKEY_BL_LEVEL_UBOOT + 1)) << 8));
	bconf->level_default = (*(p + LCD_UKEY_BL_LEVEL_KERNEL) |
		((*(p + LCD_UKEY_BL_LEVEL_KERNEL + 1)) << 8));
	bconf->level_max = (*(p + LCD_UKEY_BL_LEVEL_MAX) |
		((*(p + LCD_UKEY_BL_LEVEL_MAX + 1)) << 8));
	bconf->level_min = (*(p + LCD_UKEY_BL_LEVEL_MIN) |
		((*(p  + LCD_UKEY_BL_LEVEL_MIN + 1)) << 8));
	bconf->level_mid = (*(p + LCD_UKEY_BL_LEVEL_MID) |
		((*(p + LCD_UKEY_BL_LEVEL_MID + 1)) << 8));
	bconf->level_mid_mapping = (*(p + LCD_UKEY_BL_LEVEL_MID_MAP) |
		((*(p + LCD_UKEY_BL_LEVEL_MID_MAP + 1)) << 8));

	/* adjust brightness_bypass by level_default */
	if (bconf->level_default > bconf->level_max) {
		brightness_bypass = 1;
		BLPR("level_default > level_max, enable brightness_bypass\n");
	}

	/* method: 8byte */
	temp = *(p + LCD_UKEY_BL_METHOD);
	bconf->method = (temp >= BL_CTRL_MAX) ? BL_CTRL_MAX : temp;

	if (*(p + LCD_UKEY_BL_EN_GPIO) >= BL_GPIO_NUM_MAX) {
		bconf->en_gpio = BL_GPIO_MAX;
	} else {
		bconf->en_gpio = *(p + LCD_UKEY_BL_EN_GPIO);
		bl_gpio_probe(bconf->en_gpio);
	}
	bconf->en_gpio_on = *(p + LCD_UKEY_BL_EN_GPIO_ON);
	bconf->en_gpio_off = *(p + LCD_UKEY_BL_EN_GPIO_OFF);
	bconf->power_on_delay = (*(p + LCD_UKEY_BL_ON_DELAY) |
		((*(p + LCD_UKEY_BL_ON_DELAY + 1)) << 8));
	bconf->power_off_delay = (*(p + LCD_UKEY_BL_OFF_DELAY) |
		((*(p + LCD_UKEY_BL_OFF_DELAY + 1)) << 8));

	/* pwm: 24byte */
	switch (bconf->method) {
	case BL_CTRL_PWM:
		bconf->bl_pwm = kzalloc(sizeof(struct bl_pwm_config_s),
				GFP_KERNEL);
		if (bconf->bl_pwm == NULL) {
			BLERR("bl_pwm struct malloc error\n");
			kfree(para);
			return -1;
		}
		bl_pwm = bconf->bl_pwm;
		bl_pwm->index = 0;

		bl_pwm->level_max = bconf->level_max;
		bl_pwm->level_min = bconf->level_min;

		bconf->pwm_on_delay = (*(p + LCD_UKEY_BL_PWM_ON_DELAY) |
			((*(p + LCD_UKEY_BL_PWM_ON_DELAY + 1)) << 8));
		bconf->pwm_off_delay = (*(p + LCD_UKEY_BL_PWM_OFF_DELAY) |
			((*(p + LCD_UKEY_BL_PWM_OFF_DELAY + 1)) << 8));
		bl_pwm->pwm_method =  *(p + LCD_UKEY_BL_PWM_METHOD);
		bl_pwm->pwm_port = *(p + LCD_UKEY_BL_PWM_PORT);
		bl_pwm->pwm_freq = (*(p + LCD_UKEY_BL_PWM_FREQ) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 1)) << 8) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 2)) << 8) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 3)) << 8));
		if (bl_pwm->pwm_port == BL_PWM_VS) {
			if (bl_pwm->pwm_freq > 4) {
				BLERR("bl_pwm_vs wrong freq %d\n",
					bl_pwm->pwm_freq);
				bl_pwm->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (bl_pwm->pwm_freq > XTAL_HALF_FREQ_HZ)
				bl_pwm->pwm_freq = XTAL_HALF_FREQ_HZ;
		}
		bl_pwm->pwm_duty_max = *(p + LCD_UKEY_BL_PWM_DUTY_MAX);
		bl_pwm->pwm_duty_min = *(p + LCD_UKEY_BL_PWM_DUTY_MIN);

		if (bl_header.version == 2)
			bconf->en_sequence_reverse =
				(*(p + LCD_UKEY_BL_CUST_VAL_0) |
				((*(p + LCD_UKEY_BL_CUST_VAL_0 + 1)) << 8));
		else
			bconf->en_sequence_reverse = 0;

		bl_pwm->pwm_duty = bl_pwm->pwm_duty_min;
		bl_pwm_config_init(bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		bconf->bl_pwm_combo0 = kzalloc(sizeof(struct bl_pwm_config_s),
					GFP_KERNEL);
		if (bconf->bl_pwm_combo0 == NULL) {
			BLERR("bl_pwm_combo0 struct malloc error\n");
			kfree(para);
			return -1;
		}
		bconf->bl_pwm_combo1 = kzalloc(sizeof(struct bl_pwm_config_s),
					GFP_KERNEL);
		if (bconf->bl_pwm_combo1 == NULL) {
			BLERR("bl_pwm_combo1 struct malloc error\n");
			kfree(para);
			return -1;
		}
		pwm_combo0 = bconf->bl_pwm_combo0;
		pwm_combo1 = bconf->bl_pwm_combo1;
		pwm_combo0->index = 0;
		pwm_combo1->index = 1;

		bconf->pwm_on_delay = (*(p + LCD_UKEY_BL_PWM_ON_DELAY) |
			((*(p + LCD_UKEY_BL_PWM_ON_DELAY + 1)) << 8));
		bconf->pwm_off_delay = (*(p + LCD_UKEY_BL_PWM_OFF_DELAY) |
			((*(p + LCD_UKEY_BL_PWM_OFF_DELAY + 1)) << 8));

		pwm_combo0->pwm_method = *(p + LCD_UKEY_BL_PWM_METHOD);
		pwm_combo0->pwm_port = *(p + LCD_UKEY_BL_PWM_PORT);
		pwm_combo0->pwm_freq = (*(p + LCD_UKEY_BL_PWM_FREQ) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 1)) << 8) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 2)) << 8) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 3)) << 8));
		if (pwm_combo0->pwm_port == BL_PWM_VS) {
			if (pwm_combo0->pwm_freq > 4) {
				BLERR("bl_pwm_0_vs wrong freq %d\n",
					pwm_combo0->pwm_freq);
				pwm_combo0->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (pwm_combo0->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo0->pwm_freq = XTAL_HALF_FREQ_HZ;
		}
		pwm_combo0->pwm_duty_max = *(p + LCD_UKEY_BL_PWM_DUTY_MAX);
		pwm_combo0->pwm_duty_min = *(p + LCD_UKEY_BL_PWM_DUTY_MIN);

		pwm_combo1->pwm_method = *(p + LCD_UKEY_BL_PWM2_METHOD);
		pwm_combo1->pwm_port = *(p + LCD_UKEY_BL_PWM2_PORT);
		pwm_combo1->pwm_freq = (*(p + LCD_UKEY_BL_PWM2_FREQ) |
			((*(p + LCD_UKEY_BL_PWM2_FREQ + 1)) << 8) |
			((*(p + LCD_UKEY_BL_PWM2_FREQ + 2)) << 8) |
			((*(p + LCD_UKEY_BL_PWM2_FREQ + 3)) << 8));
		if (pwm_combo1->pwm_port == BL_PWM_VS) {
			if (pwm_combo1->pwm_freq > 4) {
				BLERR("bl_pwm_1_vs wrong freq %d\n",
					pwm_combo1->pwm_freq);
				pwm_combo1->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (pwm_combo1->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo1->pwm_freq = XTAL_HALF_FREQ_HZ;
		}
		pwm_combo1->pwm_duty_max = *(p + LCD_UKEY_BL_PWM2_DUTY_MAX);
		pwm_combo1->pwm_duty_min = *(p + LCD_UKEY_BL_PWM2_DUTY_MIN);

		pwm_combo0->level_max = (*(p + LCD_UKEY_BL_PWM_LEVEL_MAX) |
			((*(p + LCD_UKEY_BL_PWM_LEVEL_MAX + 1)) << 8));
		pwm_combo0->level_min = (*(p + LCD_UKEY_BL_PWM_LEVEL_MIN) |
			((*(p + LCD_UKEY_BL_PWM_LEVEL_MIN + 1)) << 8));
		pwm_combo1->level_max = (*(p + LCD_UKEY_BL_PWM2_LEVEL_MAX) |
			((*(p + LCD_UKEY_BL_PWM2_LEVEL_MAX + 1)) << 8));
		pwm_combo1->level_min = (*(p + LCD_UKEY_BL_PWM2_LEVEL_MIN) |
			((*(p + LCD_UKEY_BL_PWM2_LEVEL_MIN + 1)) << 8));

		if (bl_header.version == 2)
			bconf->en_sequence_reverse = (*(p +
				LCD_UKEY_BL_CUST_VAL_0) |
				((*(p + LCD_UKEY_BL_CUST_VAL_0 + 1)) << 8));
		else
			bconf->en_sequence_reverse = 0;

		pwm_combo0->pwm_duty = pwm_combo0->pwm_duty_min;
		pwm_combo1->pwm_duty = pwm_combo1->pwm_duty_min;
		bl_pwm_config_init(pwm_combo0);
		bl_pwm_config_init(pwm_combo1);
		break;
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	case BL_CTRL_LOCAL_DIMMING:
		bconf->ldim_flag = 1;
		break;
#endif
	default:
		break;
	}

#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	if (bconf->ldim_flag) {
		ret = aml_ldim_get_config_unifykey(para);
		if (ret < 0) {
			kfree(para);
			return -1;
		}
	}
#endif

	kfree(para);
	return 0;
}

static int aml_bl_pwm_channel_register(struct bl_config_s *bconf,
		struct device_node *blnode)
{
	int ret = 0;
	int index0 = BL_PWM_MAX;
	int index1 = BL_PWM_MAX;
	phandle pwm_phandle;
	struct device_node *pnode = NULL;
	struct device_node *child;
	struct bl_pwm_config_s *bl_pwm = NULL;

	ret = of_property_read_u32(blnode, "bl_pwm_config", &pwm_phandle);
	if (ret) {
		BLERR("not match bl_pwm_config node\n");
		return -1;
	}
	pnode = of_find_node_by_phandle(pwm_phandle);
	if (!pnode) {
		BLERR("can't find bl_pwm_config node\n");
		return -1;
	}

	/*request for pwm device */
	for_each_child_of_node(pnode, child) {
		ret = of_property_read_u32(child, "pwm_port_index", &index0);
		if (ret) {
			BLERR("failed to get pwm_port_index\n");
			return ret;
		}
		ret = of_property_read_u32_index(child, "pwms", 1, &index1);
		if (ret) {
			BLERR("failed to get meson_pwm_index\n");
			return ret;
		}

		if (index0 >= BL_PWM_VS)
			continue;
		if ((index0 % 2) != index1)
			continue;

		bl_pwm = NULL;
		switch (bconf->method) {
		case BL_CTRL_PWM:
			if (index0 == bconf->bl_pwm->pwm_port)
				bl_pwm = bconf->bl_pwm;
			break;
		case BL_CTRL_PWM_COMBO:
			if (index0 == bconf->bl_pwm_combo0->pwm_port)
				bl_pwm = bconf->bl_pwm_combo0;
			if (index0 == bconf->bl_pwm_combo1->pwm_port)
				bl_pwm = bconf->bl_pwm_combo1;
			break;
		default:
			break;
		}
		if (bl_pwm == NULL)
			continue;

		bl_pwm->pwm_data.port_index = index0;
		bl_pwm->pwm_data.meson_index = index1;
		bl_pwm->pwm_data.pwm = devm_of_pwm_get(
			bl_drv->dev, child, NULL);
		if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
			ret = PTR_ERR(bl_pwm->pwm_data.pwm);
			BLERR("unable to request bl_pwm(%d)\n", index0);
			return ret;
		}
		bl_pwm->pwm_data.meson = to_meson_pwm(
			bl_pwm->pwm_data.pwm->chip);
		pwm_init_state(bl_pwm->pwm_data.pwm, &(bl_pwm->pwm_data.state));
		BLPR("register pwm_ch(%d) 0x%p\n",
			bl_pwm->pwm_data.port_index, bl_pwm->pwm_data.pwm);
	}

	BLPR("%s ok\n", __func__);

	return ret;

}

static int aml_bl_config_load(struct bl_config_s *bconf,
		struct platform_device *pdev)
{
	int load_id = 0;
	int ret = 0;

	if (pdev->dev.of_node == NULL) {
		BLERR("no backlight of_node exist\n");
		return -1;
	}
	ret = of_property_read_u32(pdev->dev.of_node,
			"key_valid", &bl_key_valid);
	if (ret) {
		if (bl_debug_print_flag)
			BLPR("failed to get key_valid\n");
		bl_key_valid = 0;
	}
	BLPR("key_valid: %d\n", bl_key_valid);

	if (bl_key_valid) {
		ret = lcd_unifykey_check("backlight");
		if (ret < 0)
			load_id = 0;
		else
			load_id = 1;
	}
	if (load_id) {
		BLPR("%s from unifykey\n", __func__);
		bl_config_load = 1;
		ret = aml_bl_config_load_from_unifykey(bconf);
	} else {
#ifdef CONFIG_OF
		BLPR("%s from dts\n", __func__);
		bl_config_load = 0;
		ret = aml_bl_config_load_from_dts(bconf, pdev);
#endif
	}
	if (bl_level)
		bl_level_uboot = bl_level;
	bl_on_level = (bl_level_uboot > 0) ? bl_level_uboot : BL_LEVEL_DEFAULT;

	aml_bl_config_print(bconf);

#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	if (bconf->ldim_flag)
		aml_ldim_probe(pdev);
#endif

	switch (bconf->method) {
	case BL_CTRL_PWM:
	case BL_CTRL_PWM_COMBO:
		ret = aml_bl_pwm_channel_register(bconf, pdev->dev.of_node);
		break;
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		aml_bl_extern_device_load(bconf->bl_extern_index);
		break;
#endif

	default:
		break;
	}
	return ret;
}

/* lcd notify */
static void aml_bl_step_on(int brightness)
{
	mutex_lock(&bl_level_mutex);
	BLPR("bl_step_on level: %d\n", brightness);

	if (brightness == 0) {
		if (bl_drv->state & BL_STATE_BL_ON)
			bl_power_off();
	} else {
		aml_bl_set_level(brightness);
		if ((bl_drv->state & BL_STATE_BL_ON) == 0)
			bl_power_on();
	}
	msleep(120);
	mutex_unlock(&bl_level_mutex);
}

static void aml_bl_on_function(void)
{
	/* lcd power on backlight flag */
	bl_drv->state |= (BL_STATE_LCD_ON | BL_STATE_BL_POWER_ON);
	BLPR("%s: bl_level=%u, state=0x%x\n",
		__func__, bl_drv->level, bl_drv->state);
	if (brightness_bypass) {
		if ((bl_drv->state & BL_STATE_BL_ON) == 0)
			bl_power_on();
	} else {
		if (bl_step_on_flag) {
			aml_bl_step_on(bl_drv->bconf->level_default);
			BLPR("bl_on level: %d\n",
				bl_drv->bldev->props.brightness);
		}
		aml_bl_update_status(bl_drv->bldev);
	}
}

static void aml_bl_delayd_on(struct work_struct *work)
{
	if (aml_bl_check_driver())
		return;

	if (bl_on_request == 0)
		return;

	aml_bl_on_function();
}

static int aml_bl_on_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct bl_config_s *bconf;

	if ((event & LCD_EVENT_BL_ON) == 0)
		return NOTIFY_DONE;
	if (bl_debug_print_flag)
		BLPR("%s: 0x%lx\n", __func__, event);

	if (aml_bl_check_driver())
		return NOTIFY_DONE;

	bconf = bl_drv->bconf;
	bl_on_request = 1;
	/* lcd power on sequence control */
	if (bconf->method < BL_CTRL_MAX) {
		if (bl_drv->workqueue) {
			queue_delayed_work(bl_drv->workqueue,
				&bl_drv->bl_delayed_work,
				msecs_to_jiffies(bconf->power_on_delay));
		} else {
			schedule_delayed_work(
				&bl_drv->bl_delayed_work,
				msecs_to_jiffies(bconf->power_on_delay));
		}
	} else
		BLERR("wrong backlight control method\n");

	return NOTIFY_OK;
}

static int aml_bl_off_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	if ((event & LCD_EVENT_BL_OFF) == 0)
		return NOTIFY_DONE;
	if (bl_debug_print_flag)
		BLPR("%s: 0x%lx\n", __func__, event);

	if (aml_bl_check_driver())
		return NOTIFY_DONE;

	bl_on_request = 0;
	bl_drv->state &= ~(BL_STATE_LCD_ON | BL_STATE_BL_POWER_ON);
	if (brightness_bypass) {
		if (bl_drv->state & BL_STATE_BL_ON)
			bl_power_off();
	} else
		aml_bl_update_status(bl_drv->bldev);

	return NOTIFY_OK;
}

static struct notifier_block aml_bl_on_nb = {
	.notifier_call = aml_bl_on_notifier,
	.priority = LCD_PRIORITY_POWER_BL_ON,
};

static struct notifier_block aml_bl_off_nb = {
	.notifier_call = aml_bl_off_notifier,
	.priority = LCD_PRIORITY_POWER_BL_OFF,
};

static int aml_bl_lcd_update_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct bl_pwm_config_s *bl_pwm = NULL;
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
#endif

	/* If we aren't interested in this event, skip it immediately */
	if (event != LCD_EVENT_BACKLIGHT_UPDATE)
		return NOTIFY_DONE;

	if (aml_bl_check_driver())
		return NOTIFY_DONE;
	if (bl_debug_print_flag)
		BLPR("bl_lcd_update_notifier for pwm_vs\n");
	switch (bl_drv->bconf->method) {
	case BL_CTRL_PWM:
		if (bl_drv->bconf->bl_pwm->pwm_port == BL_PWM_VS) {
			bl_pwm = bl_drv->bconf->bl_pwm;
			if (bl_pwm) {
				bl_pwm_config_init(bl_pwm);
				if (brightness_bypass)
					bl_set_duty_pwm(bl_pwm);
				else
					aml_bl_update_status(bl_drv->bldev);
			}
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (bl_drv->bconf->bl_pwm_combo0->pwm_port == BL_PWM_VS)
			bl_pwm = bl_drv->bconf->bl_pwm_combo0;
		else if (bl_drv->bconf->bl_pwm_combo1->pwm_port == BL_PWM_VS)
			bl_pwm = bl_drv->bconf->bl_pwm_combo1;
		if (bl_pwm) {
			bl_pwm_config_init(bl_pwm);
			if (brightness_bypass)
				bl_set_duty_pwm(bl_pwm);
			else
				aml_bl_update_status(bl_drv->bldev);
		}
		break;
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	case BL_CTRL_LOCAL_DIMMING:
		if (ldim_drv->pwm_vs_update)
			ldim_drv->pwm_vs_update();
		break;
#endif

	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block aml_bl_lcd_update_nb = {
	.notifier_call = aml_bl_lcd_update_notifier,
};

static int aml_bl_lcd_test_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	int *flag;
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
#endif

	/* If we aren't interested in this event, skip it immediately */
	if (event != LCD_EVENT_TEST_PATTERN)
		return NOTIFY_DONE;

	if (aml_bl_check_driver())
		return NOTIFY_DONE;
	if (bl_debug_print_flag)
		BLPR("bl_lcd_test_notifier for lcd test_pattern\n");

	flag = (int *)data;
	switch (bl_drv->bconf->method) {
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	case BL_CTRL_LOCAL_DIMMING:
		if (ldim_drv->test_ctrl)
			ldim_drv->test_ctrl(*flag);
		break;
#endif
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block aml_bl_lcd_test_nb = {
	.notifier_call = aml_bl_lcd_test_notifier,
};

/* bl debug calss */
struct class *bl_debug_class;

static const char *bl_debug_usage_str = {
"Usage:\n"
"    cat status ; dump backlight config\n"
"\n"
"    echo freq <index> <pwm_freq> > pwm ; set pwm frequency(unit in Hz for pwm, vfreq multiple for pwm_vs)\n"
"    echo duty <index> <pwm_duty> > pwm ; set pwm duty cycle(unit: %)\n"
"    echo pol <index> <pwm_pol> > pwm ; set pwm polarity(unit: %)\n"
"	 echo max <index> <duty_max> > pwm ; set pwm duty_max(unit: %)\n"
"	 echo min <index> <duty_min> > pwm ; set pwm duty_min(unit: %)\n"
"    cat pwm ; dump pwm state\n"
"	 echo free <0|1> > pwm ; set bl_pwm_duty_free enable or disable\n"
"\n"
"    echo <0|1> > power ; backlight power ctrl\n"
"    cat power ; print backlight power state\n"
"\n"
"    echo <0|1> > print ; 0=disable debug print; 1=enable debug print\n"
"    cat print ; read current debug print flag\n"
};

static ssize_t bl_debug_help(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", bl_debug_usage_str);
}

static ssize_t bl_status_read(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct bl_config_s *bconf = bl_drv->bconf;
	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *pwm_combo0, *pwm_combo1;
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	struct aml_bl_extern_driver_s *bl_extern = aml_bl_extern_get_driver();
#endif

	ssize_t len = 0;

	len = sprintf(buf, "read backlight status:\n"
		"key_valid:          %d\n"
		"config_load:        %d\n"
		"index:              %d\n"
		"name:               %s\n"
		"state:              0x%x\n"
		"level:              %d\n"
		"level_uboot:        %d\n"
		"level_default:      %d\n"
		"step_on_flag        %d\n"
		"brightness_bypass:  %d\n\n"
		"level_max:          %d\n"
		"level_min:          %d\n"
		"level_mid:          %d\n"
		"level_mid_mapping:  %d\n\n"
		"method:             %s\n"
		"en_gpio:            %s(%d)\n"
		"en_gpio_on:         %d\n"
		"en_gpio_off:        %d\n"
		"power_on_delay:     %d\n"
		"power_off_delay:    %d\n\n",
		bl_key_valid, bl_config_load,
		bl_drv->index, bconf->name, bl_drv->state,
		bl_drv->level,  bl_level_uboot, bconf->level_default,
		bl_step_on_flag, brightness_bypass,
		bconf->level_max, bconf->level_min,
		bconf->level_mid, bconf->level_mid_mapping,
		bl_method_type_to_str(bconf->method),
		bconf->bl_gpio[bconf->en_gpio].name,
		bconf->en_gpio, bconf->en_gpio_on, bconf->en_gpio_off,
		bconf->power_on_delay, bconf->power_off_delay);
	switch (bconf->method) {
	case BL_CTRL_GPIO:
		len += sprintf(buf+len, "to do\n");
		break;
	case BL_CTRL_PWM:
		bl_pwm = bconf->bl_pwm;
		len += sprintf(buf+len,
			"pwm_method:         %d\n"
			"pwm_port:           %d\n"
			"pwm_freq:           %d\n"
			"pwm_duty_max:       %d\n"
			"pwm_duty_min:       %d\n"
			"pwm_on_delay:       %d\n"
			"pwm_off_delay:      %d\n"
			"en_sequence_reverse: %d\n\n",
			bl_pwm->pwm_method, bl_pwm->pwm_port, bl_pwm->pwm_freq,
			bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min,
			bconf->pwm_on_delay, bconf->pwm_off_delay,
			bconf->en_sequence_reverse);
		break;
	case BL_CTRL_PWM_COMBO:
		pwm_combo0 = bconf->bl_pwm_combo0;
		pwm_combo1 = bconf->bl_pwm_combo1;
		len += sprintf(buf+len,
			"pwm_0_level_max:    %d\n"
			"pwm_0_level_min:    %d\n"
			"pwm_0_method:       %d\n"
			"pwm_0_port:         %d\n"
			"pwm_0_freq:         %d\n"
			"pwm_0_duty_max:     %d\n"
			"pwm_0_duty_min:     %d\n"
			"pwm_1_level_max:    %d\n"
			"pwm_1_level_min:    %d\n"
			"pwm_1_method:       %d\n"
			"pwm_1_port:         %d\n"
			"pwm_1_freq:         %d\n"
			"pwm_1_duty_max:     %d\n"
			"pwm_1_duty_min:     %d\n"
			"pwm_on_delay:       %d\n"
			"pwm_off_delay:      %d\n"
			"en_sequence_reverse: %d\n\n",
			pwm_combo0->level_max, pwm_combo0->level_min,
			pwm_combo0->pwm_method, pwm_combo0->pwm_port,
			pwm_combo0->pwm_freq,
			pwm_combo0->pwm_duty_max, pwm_combo0->pwm_duty_min,
			pwm_combo1->level_max, pwm_combo1->level_min,
			pwm_combo1->pwm_method, pwm_combo1->pwm_port,
			pwm_combo1->pwm_freq,
			pwm_combo1->pwm_duty_max, pwm_combo1->pwm_duty_min,
			bconf->pwm_on_delay, bconf->pwm_off_delay,
			bconf->en_sequence_reverse);
		break;
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	case BL_CTRL_LOCAL_DIMMING:
		if (ldim_drv->config_print)
			ldim_drv->config_print();
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		if (bl_extern->config_print)
			bl_extern->config_print();
		break;
#endif
	default:
		len += sprintf(buf+len, "wrong backlight control method\n");
		break;
	}
	return len;
}

static ssize_t bl_debug_pwm_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct bl_config_s *bconf = bl_drv->bconf;
	struct bl_pwm_config_s *bl_pwm;
	struct pwm_state pstate;
	unsigned int value;
	ssize_t len = 0;

	len = sprintf(buf, "read backlight pwm state:\n");
	switch (bconf->method) {
	case BL_CTRL_PWM:
		len += sprintf(buf+len,
			"bl_pwm_bypass:      %d\n"
			"bl_pwm_duty_free:   %d\n",
			bl_pwm_bypass, bl_pwm_duty_free);
		if (bconf->bl_pwm) {
			bl_pwm = bconf->bl_pwm;
			len += sprintf(buf+len,
				"pwm_index:          %d\n"
				"pwm_port:           %d\n"
				"pwm_method:         %d\n"
				"pwm_freq:           %d\n"
				"pwm_duty_max:       %d\n"
				"pwm_duty_min:       %d\n"
				"pwm_cnt:            %d\n"
				"pwm_max:            %d\n"
				"pwm_min:            %d\n"
				"pwm_level:          %d\n",
				bl_pwm->index, bl_pwm->pwm_port,
				bl_pwm->pwm_method, bl_pwm->pwm_freq,
				bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min,
				bl_pwm->pwm_cnt,
				bl_pwm->pwm_max, bl_pwm->pwm_min,
				bl_pwm->pwm_level);
			if (bl_pwm->pwm_duty_max > 100) {
				len += sprintf(buf+len,
					"pwm_duty:           %d(%d%%)\n",
						bl_pwm->pwm_duty,
						bl_pwm->pwm_duty*100/255);
			} else {
				len += sprintf(buf+len,
					"pwm_duty:           %d%%\n",
						bl_pwm->pwm_duty);
			}
			switch (bl_pwm->pwm_port) {
			case BL_PWM_A:
			case BL_PWM_B:
			case BL_PWM_C:
			case BL_PWM_D:
			case BL_PWM_E:
			case BL_PWM_F:
				if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
					len += sprintf(buf+len,
						"pwm invalid\n");
					break;
				}
				pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
				len += sprintf(buf+len,
					"pwm state:\n"
					"  period:           %d\n"
					"  duty_cycle:       %d\n"
					"  polarity:         %d\n"
					"  enabled:          %d\n",
					pstate.period, pstate.duty_cycle,
					pstate.polarity, pstate.enabled);
				value = bl_cbus_read(bl_drv->data->pwm_reg[
					bl_pwm->pwm_port]);
				len += sprintf(buf+len,
					"pwm_reg:            0x%08x\n",
					value);
				break;
			case BL_PWM_VS:
				len += sprintf(buf+len,
					"pwm_reg0:            0x%08x\n"
					"pwm_reg1:            0x%08x\n"
					"pwm_reg2:            0x%08x\n"
					"pwm_reg3:            0x%08x\n",
					bl_vcbus_read(VPU_VPU_PWM_V0),
					bl_vcbus_read(VPU_VPU_PWM_V1),
					bl_vcbus_read(VPU_VPU_PWM_V2),
					bl_vcbus_read(VPU_VPU_PWM_V3));
				break;
			default:
				break;
			}
		}
		break;
	case BL_CTRL_PWM_COMBO:
		len += sprintf(buf+len,
			"bl_pwm_bypass:      %d\n"
			"bl_pwm_duty_free:   %d\n",
			bl_pwm_bypass, bl_pwm_duty_free);
		if (bconf->bl_pwm_combo0) {
			bl_pwm = bconf->bl_pwm_combo0;
			len += sprintf(buf+len,
				"pwm_0_index:        %d\n"
				"pwm_0_port:         %d\n"
				"pwm_0_method:       %d\n"
				"pwm_0_freq:         %d\n"
				"pwm_0_duty_max:     %d\n"
				"pwm_0_duty_min:     %d\n"
				"pwm_0_cnt:          %d\n"
				"pwm_0_max:          %d\n"
				"pwm_0_min:          %d\n"
				"pwm_0_level:        %d\n",
				bl_pwm->index, bl_pwm->pwm_port,
				bl_pwm->pwm_method, bl_pwm->pwm_freq,
				bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min,
				bl_pwm->pwm_cnt,
				bl_pwm->pwm_max, bl_pwm->pwm_min,
				bl_pwm->pwm_level);
			if (bl_pwm->pwm_duty_max > 100) {
				len += sprintf(buf+len,
					"pwm_0_duty:         %d(%d%%)\n",
						bl_pwm->pwm_duty,
						bl_pwm->pwm_duty*100/255);
			} else {
				len += sprintf(buf+len,
					"pwm_0_duty:         %d%%\n",
						bl_pwm->pwm_duty);
			}
			switch (bl_pwm->pwm_port) {
			case BL_PWM_A:
			case BL_PWM_B:
			case BL_PWM_C:
			case BL_PWM_D:
			case BL_PWM_E:
			case BL_PWM_F:
				if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
					len += sprintf(buf+len,
						"pwm invalid\n");
					break;
				}
				pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
				len += sprintf(buf+len,
					"pwm state:\n"
					"  period:           %d\n"
					"  duty_cycle:       %d\n"
					"  polarity:         %d\n"
					"  enabled:          %d\n",
					pstate.period, pstate.duty_cycle,
					pstate.polarity, pstate.enabled);
				value = bl_cbus_read(bl_drv->data->pwm_reg[
					bl_pwm->pwm_port]);
				len += sprintf(buf+len,
					"pwm_0_reg:          0x%08x\n",
					value);
				break;
			case BL_PWM_VS:
				len += sprintf(buf+len,
					"pwm_0_reg0:         0x%08x\n"
					"pwm_0_reg1:         0x%08x\n"
					"pwm_0_reg2:         0x%08x\n"
					"pwm_0_reg3:         0x%08x\n",
					bl_vcbus_read(VPU_VPU_PWM_V0),
					bl_vcbus_read(VPU_VPU_PWM_V1),
					bl_vcbus_read(VPU_VPU_PWM_V2),
					bl_vcbus_read(VPU_VPU_PWM_V3));
				break;
			default:
				break;
			}
		}
		if (bconf->bl_pwm_combo1) {
			bl_pwm = bconf->bl_pwm_combo1;
			len += sprintf(buf+len,
				"\n"
				"pwm_1_index:        %d\n"
				"pwm_1_port:         %d\n"
				"pwm_1_method:       %d\n"
				"pwm_1_freq:         %d\n"
				"pwm_1_duty_max:     %d\n"
				"pwm_1_duty_min:     %d\n"
				"pwm_1_cnt:          %d\n"
				"pwm_1_max:          %d\n"
				"pwm_1_min:          %d\n"
				"pwm_1_level:        %d\n",
				bl_pwm->index, bl_pwm->pwm_port,
				bl_pwm->pwm_method, bl_pwm->pwm_freq,
				bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min,
				bl_pwm->pwm_cnt,
				bl_pwm->pwm_max, bl_pwm->pwm_min,
				bl_pwm->pwm_level);
			if (bl_pwm->pwm_duty_max > 100) {
				len += sprintf(buf+len,
					"pwm_1_duty:         %d(%d%%)\n",
						bl_pwm->pwm_duty,
						bl_pwm->pwm_duty*100/255);
			} else {
				len += sprintf(buf+len,
					"pwm_1_duty:         %d%%\n",
						bl_pwm->pwm_duty);
			}
			switch (bl_pwm->pwm_port) {
			case BL_PWM_A:
			case BL_PWM_B:
			case BL_PWM_C:
			case BL_PWM_D:
			case BL_PWM_E:
			case BL_PWM_F:
				if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
					len += sprintf(buf+len,
						"pwm invalid\n");
					break;
				}
				pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
				len += sprintf(buf+len,
					"pwm state:\n"
					"  period:           %d\n"
					"  duty_cycle:       %d\n"
					"  polarity:         %d\n"
					"  enabled:          %d\n",
					pstate.period, pstate.duty_cycle,
					pstate.polarity, pstate.enabled);
				value = bl_cbus_read(bl_drv->data->pwm_reg[
					bl_pwm->pwm_port]);
				len += sprintf(buf+len,
					"pwm_1_reg:          0x%08x\n",
					value);
				break;
			case BL_PWM_VS:
				len += sprintf(buf+len,
					"pwm_1_reg0:         0x%08x\n"
					"pwm_1_reg1:         0x%08x\n"
					"pwm_1_reg2:         0x%08x\n"
					"pwm_1_reg3:         0x%08x\n",
					bl_vcbus_read(VPU_VPU_PWM_V0),
					bl_vcbus_read(VPU_VPU_PWM_V1),
					bl_vcbus_read(VPU_VPU_PWM_V2),
					bl_vcbus_read(VPU_VPU_PWM_V3));
				break;
			default:
				break;
			}
		}
		break;
	default:
		len += sprintf(buf+len, "not pwm control method\n");
		break;
	}
	return len;
}

#define BL_DEBUG_PWM_FREQ    0
#define BL_DEBUG_PWM_DUTY    1
#define BL_DEBUG_PWM_POL     2
#define BL_DEBUG_PWM_DUTY_MAX 3
#define BL_DEBUG_PWM_DUTY_MIN 4
static void bl_debug_pwm_set(unsigned int index, unsigned int value, int state)
{
	struct bl_config_s *bconf = bl_drv->bconf;
	struct bl_pwm_config_s *bl_pwm = NULL;
	unsigned long temp;
	unsigned int pwm_range, temp_duty;

	if (aml_bl_check_driver())
		return;

	switch (bconf->method) {
	case BL_CTRL_PWM:
		bl_pwm = bconf->bl_pwm;
		break;
	case BL_CTRL_PWM_COMBO:
		if (index == 0)
			bl_pwm = bconf->bl_pwm_combo0;
		else
			bl_pwm = bconf->bl_pwm_combo1;
		break;
	default:
		BLERR("not pwm control method\n");
		break;
	}
	if (bl_pwm) {
		switch (state) {
		case BL_DEBUG_PWM_FREQ:
			bl_pwm->pwm_freq = value;
			if (bl_pwm->pwm_freq < 50)
				bl_pwm->pwm_freq = 50;
			bl_pwm_config_init(bl_pwm);
			bl_set_duty_pwm(bl_pwm);
			if (bl_debug_print_flag) {
				BLPR("set index(%d) pwm_port(%d) freq: %dHz\n",
				index, bl_pwm->pwm_port, bl_pwm->pwm_freq);
			}
			break;
		case BL_DEBUG_PWM_DUTY:
			bl_pwm->pwm_duty = value;
			bl_set_duty_pwm(bl_pwm);
			if (bl_debug_print_flag) {
				if (bl_pwm->pwm_duty_max > 100) {
					BLPR(
					"set index(%d) pwm_port(%d) duty: %d\n",
						index, bl_pwm->pwm_port,
						bl_pwm->pwm_duty);
				} else {
					BLPR(
				"set index(%d) pwm_port(%d) duty: %d%%\n",
						index, bl_pwm->pwm_port,
						bl_pwm->pwm_duty);
				}
			}
			break;
		case BL_DEBUG_PWM_POL:
			bl_pwm->pwm_method = value;
			bl_pwm_config_init(bl_pwm);
			bl_set_duty_pwm(bl_pwm);
			if (bl_debug_print_flag) {
				BLPR("set index(%d) pwm_port(%d) method: %d\n",
				index, bl_pwm->pwm_port, bl_pwm->pwm_method);
			}
			break;
		case BL_DEBUG_PWM_DUTY_MAX:
			bl_pwm->pwm_duty_max = value;
			if (bl_pwm->pwm_duty_max > 100)
				pwm_range = 2550;
			else
				pwm_range = 1000;
			temp = bl_pwm->pwm_level;
			temp_duty = bl_do_div((temp * pwm_range),
				bl_pwm->pwm_cnt);
			bl_pwm->pwm_duty = (temp_duty + 5) / 10;
			temp = bl_pwm->pwm_min;
			temp_duty = bl_do_div((temp * pwm_range),
				bl_pwm->pwm_cnt);
			bl_pwm->pwm_duty_min = (temp_duty + 5) / 10;
			bl_pwm_config_init(bl_pwm);
			bl_set_duty_pwm(bl_pwm);
			if (bl_debug_print_flag) {
				if (bl_pwm->pwm_duty_max > 100) {
					BLPR(
				"set index(%d) pwm_port(%d) duty_max: %d\n",
						index, bl_pwm->pwm_port,
						bl_pwm->pwm_duty_max);
				} else {
					BLPR(
				"set index(%d) pwm_port(%d) duty_max: %d%%\n",
						index, bl_pwm->pwm_port,
						bl_pwm->pwm_duty_max);
				}
			}
			break;
		case BL_DEBUG_PWM_DUTY_MIN:
			bl_pwm->pwm_duty_min = value;
			bl_pwm_config_init(bl_pwm);
			bl_set_duty_pwm(bl_pwm);
			if (bl_debug_print_flag) {
				if (bl_pwm->pwm_duty_max > 100) {
					BLPR(
				"set index(%d) pwm_port(%d) duty_min: %d\n",
						index, bl_pwm->pwm_port,
						bl_pwm->pwm_duty_min);
				} else {
					BLPR(
				"set index(%d) pwm_port(%d) duty_min: %d%%\n",
						index, bl_pwm->pwm_port,
						bl_pwm->pwm_duty_min);
				}
			}
			break;
		default:
			break;
		}
	}
}

static ssize_t bl_debug_pwm_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int index = 0, val = 0;

	switch (buf[0]) {
	case 'f':
		if (buf[3] == 'q') { /* frequency */
			ret = sscanf(buf, "freq %d %d", &index, &val);
			if (ret == 2)
				bl_debug_pwm_set(index, val, BL_DEBUG_PWM_FREQ);
			else
				BLERR("invalid parameters\n");
		} else if (buf[3] == 'e') { /* duty free */
			ret = sscanf(buf, "free %d", &val);
			if (ret == 1) {
				bl_pwm_duty_free = (unsigned char)val;
				BLPR("set bl_pwm_duty_free: %d\n",
					bl_pwm_duty_free);
			} else {
				BLERR("invalid parameters\n");
			}
		}
		break;
	case 'd': /* duty */
		ret = sscanf(buf, "duty %d %d", &index, &val);
		if (ret == 2)
			bl_debug_pwm_set(index, val, BL_DEBUG_PWM_DUTY);
		else
			BLERR("invalid parameters\n");
		break;
	case 'p': /* polarity */
		ret = sscanf(buf, "pol %d %d", &index, &val);
		if (ret == 2)
			bl_debug_pwm_set(index, val, BL_DEBUG_PWM_POL);
		else
			BLERR("invalid parameters\n");
		break;
	case 'b': /* bypass */
		ret = sscanf(buf, "bypass %d", &val);
		if (ret == 1) {
			bl_pwm_bypass = (unsigned char)val;
			BLPR("set bl_pwm_bypass: %d\n", bl_pwm_bypass);
		} else {
			BLERR("invalid parameters\n");
		}
		break;
	case 'm':
		if (buf[1] == 'a') { /* max */
			ret = sscanf(buf, "max %d %d", &index, &val);
			if (ret == 2) {
				bl_debug_pwm_set(index, val,
					BL_DEBUG_PWM_DUTY_MAX);
			} else {
				BLERR("invalid parameters\n");
			}
		} else if (buf[1] == 'i') { /* min */
			ret = sscanf(buf, "min %d %d", &index, &val);
			if (ret == 2) {
				bl_debug_pwm_set(index, val,
					BL_DEBUG_PWM_DUTY_MIN);
			} else {
				BLERR("invalid parameters\n");
			}
		}
		break;
	default:
		BLERR("wrong command\n");
		break;
	}

	return count;
}

static ssize_t bl_debug_power_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int state;

	if ((bl_drv->state & BL_STATE_LCD_ON) == 0) {
		state = 0;
	} else {
		if (bl_drv->state & BL_STATE_BL_POWER_ON)
			state = 1;
		else
			state = 0;
	}
	return sprintf(buf, "backlight power state: %d\n", state);
}

static ssize_t bl_debug_power_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret != 0) {
		BLERR("invalid data\n");
		return -EINVAL;
	}

	BLPR("power control: %u\n", temp);
	if ((bl_drv->state & BL_STATE_LCD_ON) == 0) {
		temp = 0;
		BLPR("backlight force off for lcd is off\n");
	}
	if (temp == 0) {
		bl_drv->state &= ~BL_STATE_BL_POWER_ON;
		if (bl_drv->state & BL_STATE_BL_ON)
			bl_power_off();
	} else {
		bl_drv->state |= BL_STATE_BL_POWER_ON;
		if ((bl_drv->state & BL_STATE_BL_ON) == 0)
			bl_power_on();
	}

	return count;
}

static ssize_t bl_debug_step_on_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "backlight step_on: %d\n", bl_step_on_flag);
}

static ssize_t bl_debug_step_on_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret != 0) {
		BLERR("invalid data\n");
		return -EINVAL;
	}

	bl_step_on_flag = (unsigned char)temp;
	BLPR("step_on: %u\n", bl_step_on_flag);

	return count;
}

static ssize_t bl_debug_delay_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct bl_config_s *bconf = bl_drv->bconf;

	return sprintf(buf, "bl power delay: on=%dms, off=%dms\n",
		bconf->power_on_delay, bconf->power_off_delay);
}

static ssize_t bl_debug_delay_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int val[2];
	struct bl_config_s *bconf = bl_drv->bconf;

	ret = sscanf(buf, "%d %d", &val[0], &val[1]);
	if (ret == 2) {
		bconf->power_on_delay = val[0];
		bconf->power_off_delay = val[1];
		pr_info("set bl power_delay: on=%dms, off=%dms\n",
			val[0], val[1]);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t bl_debug_key_valid_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bl_key_valid);
}

static ssize_t bl_debug_config_load_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bl_config_load);
}

static ssize_t bl_debug_print_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "show bl_debug_print_flag: %d\n",
		bl_debug_print_flag);
}

static ssize_t bl_debug_print_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret == 0) {
		bl_debug_print_flag = temp;
		BLPR("set bl_debug_print_flag: %u\n", bl_debug_print_flag);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static struct class_attribute bl_debug_class_attrs[] = {
	__ATTR(help, 0444, bl_debug_help, NULL),
	__ATTR(status, 0444, bl_status_read, NULL),
	__ATTR(pwm, 0644, bl_debug_pwm_show, bl_debug_pwm_store),
	__ATTR(power, 0644, bl_debug_power_show, bl_debug_power_store),
	__ATTR(step_on, 0644, bl_debug_step_on_show, bl_debug_step_on_store),
	__ATTR(delay, 0644, bl_debug_delay_show, bl_debug_delay_store),
	__ATTR(key_valid,   0444, bl_debug_key_valid_show, NULL),
	__ATTR(config_load, 0444, bl_debug_config_load_show, NULL),
	__ATTR(print, 0644, bl_debug_print_show, bl_debug_print_store),
};

static int aml_bl_creat_class(void)
{
	int i;

	bl_debug_class = class_create(THIS_MODULE, "aml_bl");
	if (IS_ERR_OR_NULL(bl_debug_class)) {
		BLERR("create aml_bl debug class fail\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(bl_debug_class_attrs); i++) {
		if (class_create_file(bl_debug_class,
				&bl_debug_class_attrs[i])) {
			BLERR("create aml_bl debug attribute %s fail\n",
				bl_debug_class_attrs[i].attr.name);
		}
	}
	return 0;
}

static int aml_bl_remove_class(void)
{
	int i;

	if (bl_debug_class == NULL)
		return -1;

	for (i = 0; i < ARRAY_SIZE(bl_debug_class_attrs); i++)
		class_remove_file(bl_debug_class, &bl_debug_class_attrs[i]);
	class_destroy(bl_debug_class);
	bl_debug_class = NULL;
	return 0;
}
/* **************************************** */

#ifdef CONFIG_PM
static int aml_bl_suspend(struct platform_device *pdev, pm_message_t state)
{
	switch (bl_off_policy) {
	case BL_OFF_POLICY_ONCE:
		aml_bl_off_policy_cnt += 1;
		break;
	default:
		break;
	}

	if (aml_bl_off_policy_cnt == 2) {
		aml_bl_off_policy_cnt = 0;
		bl_off_policy = BL_OFF_POLICY_NONE;
		BLPR("change bl_off_policy: %d\n", bl_off_policy);
	}

	BLPR("aml_bl_suspend: bl_off_policy=%d, aml_bl_off_policy_cnt=%d\n",
		bl_off_policy, aml_bl_off_policy_cnt);
	return 0;
}

static int aml_bl_resume(struct platform_device *pdev)
{
	return 0;
}
#endif

#ifdef CONFIG_OF
static struct bl_data_s bl_data_gxl = {
	.chip_type = BL_CHIP_GXL,
	.chip_name = "gxl",
	.pwm_reg = pwm_reg_txl,
};

static struct bl_data_s bl_data_gxm = {
	.chip_type = BL_CHIP_GXM,
	.chip_name = "gxm",
	.pwm_reg = pwm_reg_txl,
};

static struct bl_data_s bl_data_txl = {
	.chip_type = BL_CHIP_TXL,
	.chip_name = "txl",
	.pwm_reg = pwm_reg_txl,
};

static struct bl_data_s bl_data_txlx = {
	.chip_type = BL_CHIP_TXLX,
	.chip_name = "txlx",
	.pwm_reg = pwm_reg_txlx,
};

static struct bl_data_s bl_data_axg = {
	.chip_type = BL_CHIP_AXG,
	.chip_name = "axg",
	.pwm_reg = pwm_reg_txlx,
};

static struct bl_data_s bl_data_g12a = {
	.chip_type = BL_CHIP_G12A,
	.chip_name = "g12a",
	.pwm_reg = pwm_reg_txlx,
};

static struct bl_data_s bl_data_g12b = {
	.chip_type = BL_CHIP_G12B,
	.chip_name = "g12b",
	.pwm_reg = pwm_reg_txlx,
};

static struct bl_data_s bl_data_tl1 = {
	.chip_type = BL_CHIP_TL1,
	.chip_name = "tl1",
	.pwm_reg = pwm_reg_txlx,
};

static struct bl_data_s bl_data_sm1 = {
	.chip_type = BL_CHIP_SM1,
	.chip_name = "sm1",
	.pwm_reg = pwm_reg_txlx,
};

static const struct of_device_id bl_dt_match_table[] = {
	{
		.compatible = "amlogic, backlight-gxl",
		.data = &bl_data_gxl,
	},
	{
		.compatible = "amlogic, backlight-gxm",
		.data = &bl_data_gxm,
	},
	{
		.compatible = "amlogic, backlight-txl",
		.data = &bl_data_txl,
	},
	{
		.compatible = "amlogic, backlight-txlx",
		.data = &bl_data_txlx,
	},
	{
		.compatible = "amlogic, backlight-axg",
		.data = &bl_data_axg,
	},
	{
		.compatible = "amlogic, backlight-g12a",
		.data = &bl_data_g12a,
	},
	{
		.compatible = "amlogic, backlight-g12b",
		.data = &bl_data_g12b,
	},
	{
		.compatible = "amlogic, backlight-tl1",
		.data = &bl_data_tl1,
	},
	{
		.compatible = "amlogic, backlight-sm1",
		.data = &bl_data_sm1,
	},
	{},
};
#endif

static void aml_bl_init_status_update(void)
{
	unsigned int state;

	state = bl_vcbus_read(ENCL_VIDEO_EN);
	if (state == 0) /* default disable lcd & backlight */
		return;

	/* update bl status */
	bl_drv->state = (BL_STATE_LCD_ON |
			BL_STATE_BL_POWER_ON | BL_STATE_BL_ON);
	bl_on_request = 1;

	if (brightness_bypass)
		aml_bl_set_level(bl_on_level);
	else
		aml_bl_update_status(bl_drv->bldev);

	switch (bl_drv->bconf->method) {
	case BL_CTRL_PWM:
	case BL_CTRL_PWM_COMBO:
		bl_pwm_pinmux_set(bl_drv->bconf, 1);
		break;
	default:
		break;
	}
}

static int aml_bl_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct backlight_properties props;
	struct backlight_device *bldev;
	int ret;

#ifdef AML_BACKLIGHT_DEBUG
	bl_debug_print_flag = 1;
#else
	bl_debug_print_flag = 0;
#endif

	aml_bl_off_policy_cnt = 0;

	/* init backlight parameters */
	brightness_bypass = 0;
	bl_pwm_bypass = 0;
	bl_pwm_duty_free = 0;
	bl_step_on_flag = 0;

	bl_drv = kzalloc(sizeof(struct aml_bl_drv_s), GFP_KERNEL);
	if (!bl_drv) {
		BLERR("driver malloc error\n");
		return -ENOMEM;
	}
	match = of_match_device(bl_dt_match_table, &pdev->dev);
	if (match == NULL) {
		BLERR("%s: no match table\n", __func__);
		return -1;
	}
	bl_drv->data = (struct bl_data_s *)match->data;
	BLPR("chip type/name: (%d-%s)\n",
		bl_drv->data->chip_type,
		bl_drv->data->chip_name);

	bl_drv->dev = &pdev->dev;
	bl_drv->bconf = &bl_config;
	ret = aml_bl_config_load(bl_drv->bconf, pdev);
	if (ret)
		goto err;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.power = FB_BLANK_UNBLANK; /* full on */
	props.max_brightness = (bl_drv->bconf->level_max > 0 ?
			bl_drv->bconf->level_max : BL_LEVEL_MAX);
	props.brightness = bl_on_level;

	bldev = backlight_device_register(AML_BL_NAME, &pdev->dev,
					bl_drv, &aml_bl_ops, &props);
	if (IS_ERR(bldev)) {
		BLERR("failed to register backlight\n");
		ret = PTR_ERR(bldev);
		goto err;
	}
	bl_drv->bldev = bldev;
	/* platform_set_drvdata(pdev, bl_drv); */

	/* init workqueue */
	INIT_DELAYED_WORK(&bl_drv->bl_delayed_work, aml_bl_delayd_on);
	bl_drv->workqueue = create_workqueue("bl_power_on_queue");
	if (bl_drv->workqueue == NULL)
		BLERR("can't create bl work queue\n");

#ifdef CONFIG_AMLOGIC_LCD
	ret = aml_lcd_notifier_register(&aml_bl_on_nb);
	if (ret)
		BLERR("register aml_bl_on_notifier failed\n");
	ret = aml_lcd_notifier_register(&aml_bl_off_nb);
	if (ret)
		BLERR("register aml_bl_off_notifier failed\n");
	ret = aml_lcd_notifier_register(&aml_bl_lcd_update_nb);
	if (ret)
		BLERR("register aml_bl_lcd_update_nb failed\n");
	ret = aml_lcd_notifier_register(&aml_bl_lcd_test_nb);
	if (ret)
		BLERR("register aml_bl_lcd_test_nb failed\n");
#endif
	aml_bl_creat_class();

	aml_bl_init_status_update();

	BLPR("probe OK\n");
	return 0;
err:
	kfree(bl_drv);
	bl_drv = NULL;
	return ret;
}

static int __exit aml_bl_remove(struct platform_device *pdev)
{
	int ret;
	/*struct aml_bl *bl_drv = platform_get_drvdata(pdev);*/

	aml_bl_remove_class();

	ret = cancel_delayed_work_sync(&bl_drv->bl_delayed_work);
	if (bl_drv->workqueue)
		destroy_workqueue(bl_drv->workqueue);

	backlight_device_unregister(bl_drv->bldev);
#ifdef CONFIG_AMLOGIC_LCD
	aml_lcd_notifier_unregister(&aml_bl_lcd_update_nb);
	aml_lcd_notifier_unregister(&aml_bl_on_nb);
	aml_lcd_notifier_unregister(&aml_bl_off_nb);
#endif
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	if (bl_drv->bconf->ldim_flag)
		aml_ldim_remove();
#endif
	/* platform_set_drvdata(pdev, NULL); */
	switch (bl_drv->bconf->method) {
	case BL_CTRL_PWM:
		kfree(bl_drv->bconf->bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		kfree(bl_drv->bconf->bl_pwm_combo0);
		kfree(bl_drv->bconf->bl_pwm_combo1);
		break;
	default:
		break;
	}
	kfree(bl_drv);
	bl_drv = NULL;
	return 0;
}

static struct platform_driver aml_bl_driver = {
	.driver = {
		.name  = AML_BL_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(bl_dt_match_table),
#endif
	},
	.probe   = aml_bl_probe,
	.remove  = __exit_p(aml_bl_remove),
#ifdef CONFIG_PM
	.suspend = aml_bl_suspend,
	.resume  = aml_bl_resume,
#endif
};

static int __init aml_bl_init(void)
{
	if (platform_driver_register(&aml_bl_driver)) {
		BLPR("failed to register bl driver module\n");
		return -ENODEV;
	}
	return 0;
}

static void __exit aml_bl_exit(void)
{
	platform_driver_unregister(&aml_bl_driver);
}

late_initcall(aml_bl_init);
module_exit(aml_bl_exit);

static int __init aml_bl_boot_para_setup(char *s)
{
	char bl_off_policy_str[10] = "none";

	bl_off_policy = BL_OFF_POLICY_NONE;
	aml_bl_off_policy_cnt = 0;
	if (s != NULL)
		sprintf(bl_off_policy_str, "%s", s);

	if (strncmp(bl_off_policy_str, "none", 2) == 0)
		bl_off_policy = BL_OFF_POLICY_NONE;
	else if (strncmp(bl_off_policy_str, "always", 2) == 0)
		bl_off_policy = BL_OFF_POLICY_ALWAYS;
	else if (strncmp(bl_off_policy_str, "once", 2) == 0)
		bl_off_policy = BL_OFF_POLICY_ONCE;
	BLPR("bl_off_policy: %s\n", bl_off_policy_str);

	return 0;
}
__setup("bl_off=", aml_bl_boot_para_setup);

static int __init aml_bl_level_setup(char *str)
{
	int ret = 0;

	if (str != NULL) {
		ret = kstrtouint(str, 10, &bl_level);
		BLPR("bl_level: %d\n", bl_level);
	}

	return ret;
}
__setup("bl_level=", aml_bl_level_setup);


MODULE_DESCRIPTION("AML Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
