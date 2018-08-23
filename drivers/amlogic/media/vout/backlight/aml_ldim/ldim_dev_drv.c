/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ldim_dev_drv.c
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
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include "ldim_drv.h"
#include "ldim_dev_drv.h"
#include "../aml_bl_reg.h"

struct bl_gpio_s ldim_gpio[BL_GPIO_NUM_MAX] = {
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
};

static struct spi_board_info ldim_spi_dev = {
	.modalias = "ldim_dev",
	.mode = SPI_MODE_0,
	.max_speed_hz = 1000000, /* 1MHz */
	.bus_num = 0, /* SPI bus No. */
	.chip_select = 0, /* the device index on the spi bus */
	.controller_data = NULL,
};

static unsigned char ldim_ini_data_on[LDIM_SPI_INIT_ON_SIZE];
static unsigned char ldim_ini_data_off[LDIM_SPI_INIT_OFF_SIZE];

struct ldim_dev_config_s ldim_dev_config = {
	.type = LDIM_DEV_TYPE_NORMAL,
	.cs_hold_delay = 0,
	.cs_clk_delay = 0,
	.en_gpio = 0xff,
	.en_gpio_on = 1,
	.en_gpio_off = 0,
	.lamp_err_gpio = 0xff,
	.fault_check = 0,
	.write_check = 0,
	.dim_min = 0x7f, /* min 3% duty */
	.dim_max = 0xfff,
	.cmd_size = 4,
	.init_on = ldim_ini_data_on,
	.init_off = ldim_ini_data_off,
	.pwm_config = {
		.pwm_method = BL_PWM_POSITIVE,
		.pwm_port = BL_PWM_MAX,
		.pwm_duty_max = 100,
		.pwm_duty_min = 1,
	},

	.bl_regnum = 0,
};

#if 0
static void ldim_gpio_release(int index)
{
	struct bl_gpio_s *ld_gpio;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	if (index >= BL_GPIO_NUM_MAX) {
		LDIMERR("gpio index %d, exit\n", index);
		return;
	}
	ld_gpio = &ldim_gpio[index];
	if (ld_gpio->flag == 0) {
		if (ldim_debug_print) {
			LDIMPR("gpio %s[%d] is not registered\n",
				ld_gpio->name, index);
		}
		return;
	}
	if (IS_ERR(ld_gpio->gpio)) {
		LDIMERR("gpio %s[%d]: %p, err: %ld\n",
			ld_gpio->name, index, ld_gpio->gpio,
			PTR_ERR(ld_gpio->gpio));
		return;
	}

	/* release gpio */
	devm_gpiod_put(ldim_drv->dev, ld_gpio->gpio);
	ld_gpio->flag = 0;
	if (ldim_debug_print)
		LDIMPR("release gpio %s[%d]\n", ld_gpio->name, index);
}
#endif

static void ldim_gpio_probe(int index)
{
	struct bl_gpio_s *ld_gpio;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	const char *str;
	int ret;

	if (index >= BL_GPIO_NUM_MAX) {
		LDIMERR("gpio index %d, exit\n", index);
		return;
	}
	ld_gpio = &ldim_gpio[index];
	if (ld_gpio->probe_flag) {
		if (ldim_debug_print) {
			LDIMPR("gpio %s[%d] is already registered\n",
				ld_gpio->name, index);
		}
		return;
	}

	/* get gpio name */
	ret = of_property_read_string_index(ldim_drv->dev->of_node,
		"ldim_dev_gpio_names", index, &str);
	if (ret) {
		LDIMERR("failed to get ldim_dev_gpio_names: %d\n", index);
		str = "unknown";
	}
	strcpy(ld_gpio->name, str);

	/* init gpio flag */
	ld_gpio->probe_flag = 1;
	ld_gpio->register_flag = 0;
}

static int ldim_gpio_register(int index, int init_value)
{
	struct bl_gpio_s *ld_gpio;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int value;

	if (index >= BL_GPIO_NUM_MAX) {
		LDIMERR("%s: gpio index %d, exit\n", __func__, index);
		return -1;
	}
	ld_gpio = &ldim_gpio[index];
	if (ld_gpio->probe_flag == 0) {
		LDIMERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (ld_gpio->register_flag) {
		if (ldim_debug_print) {
			LDIMPR("%s: gpio %s[%d] is already registered\n",
				__func__, ld_gpio->name, index);
		}
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
	ld_gpio->gpio = devm_gpiod_get_index(ldim_drv->dev,
		"ldim_dev", index, value);
	if (IS_ERR(ld_gpio->gpio)) {
		LDIMERR("register gpio %s[%d]: %p, err: %d\n",
			ld_gpio->name, index, ld_gpio->gpio,
			IS_ERR(ld_gpio->gpio));
		return -1;
	}
	ld_gpio->register_flag = 1;
	if (ldim_debug_print) {
		LDIMPR("register gpio %s[%d]: %p, init value: %d\n",
			ld_gpio->name, index, ld_gpio->gpio, init_value);
	}

	return 0;
}

void ldim_gpio_set(int index, int value)
{
	struct bl_gpio_s *ld_gpio;

	if (index >= BL_GPIO_NUM_MAX) {
		LDIMERR("gpio index %d, exit\n", index);
		return;
	}
	ld_gpio = &ldim_gpio[index];
	if (ld_gpio->probe_flag == 0) {
		BLERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return;
	}
	if (ld_gpio->register_flag == 0) {
		ldim_gpio_register(index, value);
		return;
	}
	if (IS_ERR_OR_NULL(ld_gpio->gpio)) {
		LDIMERR("gpio %s[%d]: %p, err: %ld\n",
			ld_gpio->name, index, ld_gpio->gpio,
			PTR_ERR(ld_gpio->gpio));
		return;
	}

	switch (value) {
	case BL_GPIO_OUTPUT_LOW:
	case BL_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(ld_gpio->gpio, value);
		break;
	case BL_GPIO_INPUT:
	default:
		gpiod_direction_input(ld_gpio->gpio);
		break;
	}
	if (ldim_debug_print) {
		LDIMPR("set gpio %s[%d] value: %d\n",
			ld_gpio->name, index, value);
	}
}

unsigned int ldim_gpio_get(int index)
{
	struct bl_gpio_s *ld_gpio;

	if (index >= BL_GPIO_NUM_MAX) {
		LDIMERR("gpio index %d, exit\n", index);
		return -1;
	}

	ld_gpio = &ldim_gpio[index];
	if (ld_gpio->probe_flag == 0) {
		LDIMERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (ld_gpio->register_flag == 0) {
		LDIMERR("%s: gpio %s[%d] is not registered\n",
			__func__, ld_gpio->name, index);
		return -1;
	}
	if (IS_ERR_OR_NULL(ld_gpio->gpio)) {
		LDIMERR("gpio %s[%d]: %p, err: %ld\n",
			ld_gpio->name, index,
			ld_gpio->gpio, PTR_ERR(ld_gpio->gpio));
		return -1;
	}

	return gpiod_get_value(ld_gpio->gpio);
}

void ldim_set_duty_pwm(struct bl_pwm_config_s *bl_pwm)
{
	unsigned long temp;

	if (bl_pwm->pwm_port >= BL_PWM_MAX)
		return;

	temp = bl_pwm->pwm_cnt;
	temp = (((temp * bl_pwm->pwm_duty) + 50) / 100);
	bl_pwm->pwm_level = (unsigned int)temp;

	if (ldim_debug_print) {
		LDIMPR(
	"pwm port %d: duty=%d%%, pwm_max=%d, pwm_min=%d, pwm_level=%d\n",
			bl_pwm->pwm_port, bl_pwm->pwm_duty,
			bl_pwm->pwm_max, bl_pwm->pwm_min, bl_pwm->pwm_level);
	}

	bl_pwm_ctrl(bl_pwm, 1);
}

void ldim_pwm_off(struct bl_pwm_config_s *bl_pwm)
{
	if (bl_pwm->pwm_port >= BL_PWM_MAX)
		return;

	bl_pwm_ctrl(bl_pwm, 0);
}

/* ****************************************************** */
static char *ldim_pinmux_str[] = {
	"ldim_pwm",               /* 0 */
	"ldim_pwm_vs",            /* 1 */
	"ldim_pwm_off",            /* 1 */
	"none",
};

static int ldim_pwm_pinmux_ctrl(int status)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct bl_pwm_config_s *bl_pwm;
	int ret = 0, index = 0xff;

	if (strcmp(ldim_drv->ldev_conf->pinmux_name, "invalid") == 0)
		return 0;

	bl_pwm = &ldim_drv->ldev_conf->pwm_config;
	if (bl_pwm->pwm_port >= BL_PWM_MAX)
		return 0;

	if (bl_pwm->pwm_port == BL_PWM_VS)
		index = (status) ? 1 : 2;
	else
		index = (status) ? 0 : 2;

	if (ldim_drv->pinmux_flag == index) {
		LDIMPR("pinmux %s is already selected\n",
			ldim_pinmux_str[index]);
		return 0;
	}

	/* request pwm pinmux */
	ldim_drv->pin = devm_pinctrl_get_select(ldim_drv->dev,
		ldim_pinmux_str[index]);
	if (IS_ERR(ldim_drv->pin)) {
		LDIMERR("set pinmux %s error\n", ldim_pinmux_str[index]);
	} else {
		LDIMPR("set pinmux %s: %p\n",
			ldim_pinmux_str[index], ldim_drv->pin);
	}
	ldim_drv->pinmux_flag = index;

	return ret;
}

static int ldim_pwm_vs_update(void)
{
	struct bl_pwm_config_s *bl_pwm = NULL;
	int ret = 0;

	bl_pwm = &ldim_dev_config.pwm_config;
	bl_pwm_config_init(bl_pwm);
	ldim_set_duty_pwm(bl_pwm);

	return ret;
}

static void ldim_config_print(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();
	struct bl_pwm_config_s *bl_pwm;
	struct pwm_state pstate;
	unsigned int value;
	int i, n, len = 0;
	char *str = NULL;

	LDIMPR("%s:\n", __func__);

	pr_info("valid_flag            = %d\n"
		"dev_index             = %d\n",
		ldim_drv->valid_flag,
		ldim_drv->dev_index);
	if (ldim_drv->ldev_conf) {
		bl_pwm = &ldim_drv->ldev_conf->pwm_config;
		pr_info("dev_name              = %s\n"
			"type                  = %d\n"
			"en_gpio               = %d\n"
			"en_gpio_on            = %d\n"
			"en_gpio_off           = %d\n"
			"dim_min               = 0x%03x\n"
			"dim_max               = 0x%03x\n",
			ldim_drv->ldev_conf->name,
			ldim_drv->ldev_conf->type,
			ldim_drv->ldev_conf->en_gpio,
			ldim_drv->ldev_conf->en_gpio_on,
			ldim_drv->ldev_conf->en_gpio_off,
			ldim_drv->ldev_conf->dim_min,
			ldim_drv->ldev_conf->dim_max);
		pr_info("region_num            = %d\n",
			ldim_drv->ldev_conf->bl_regnum);
		n = ldim_drv->ldev_conf->bl_regnum;
		len = (n * 4) + 50;
		str = kcalloc(len, sizeof(char), GFP_KERNEL);
		if (str == NULL) {
			pr_info("%s: buf malloc error\n", __func__);
		} else {
			len = sprintf(str, "region_mapping:\n  ");
			for (i = 0; i < n; i++) {
				len += sprintf(str+len, "%d,",
					ldim_drv->ldev_conf->bl_mapping[i]);
			}
			pr_info("%s\n\n", str);
			kfree(str);
		}

		switch (ldim_drv->ldev_conf->type) {
		case LDIM_DEV_TYPE_SPI:
			pr_info("spi_modalias          = %s\n"
				"spi_mode              = %d\n"
				"spi_max_speed_hz      = %d\n"
				"spi_bus_num           = %d\n"
				"spi_chip_select       = %d\n"
				"cs_hold_delay         = %d\n"
				"cs_clk_delay          = %d\n"
				"lamp_err_gpio         = %d\n"
				"fault_check           = %d\n"
				"write_check           = %d\n"
				"cmd_size              = %d\n\n",
				ldim_drv->spi_dev->modalias,
				ldim_drv->spi_dev->mode,
				ldim_drv->spi_dev->max_speed_hz,
				ldim_drv->spi_dev->bus_num,
				ldim_drv->spi_dev->chip_select,
				ldim_drv->ldev_conf->cs_hold_delay,
				ldim_drv->ldev_conf->cs_clk_delay,
				ldim_drv->ldev_conf->lamp_err_gpio,
				ldim_drv->ldev_conf->fault_check,
				ldim_drv->ldev_conf->write_check,
				ldim_drv->ldev_conf->cmd_size);
			break;
		case LDIM_DEV_TYPE_I2C:
			break;
		case LDIM_DEV_TYPE_NORMAL:
			break;
		default:
			break;
		}
		if (bl_pwm->pwm_port < BL_PWM_MAX) {
			pr_info("pwm_port:           %d\n"
				"pwm_pol:            %d\n"
				"pwm_freq:           %d\n"
				"pwm_cnt:            %d\n"
				"pwm_level:          %d\n"
				"pwm_duty:           %d%%\n",
				bl_pwm->pwm_port, bl_pwm->pwm_method,
				bl_pwm->pwm_freq, bl_pwm->pwm_cnt,
				bl_pwm->pwm_level, bl_pwm->pwm_duty);
			switch (bl_pwm->pwm_port) {
			case BL_PWM_A:
			case BL_PWM_B:
			case BL_PWM_C:
			case BL_PWM_D:
			case BL_PWM_E:
			case BL_PWM_F:
				if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
					pr_info("pwm invalid\n");
					break;
				}
				pr_info("pwm_pointer:        %p\n",
					bl_pwm->pwm_data.pwm);
				pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
				pr_info("pwm state:\n"
					"  period:           %d\n"
					"  duty_cycle:       %d\n"
					"  polarity:         %d\n"
					"  enabled:          %d\n",
					pstate.period, pstate.duty_cycle,
					pstate.polarity, pstate.enabled);
				value = bl_cbus_read(bl_drv->data->pwm_reg[
					bl_pwm->pwm_port]);
				pr_info("pwm_reg:            0x%08x\n",
					value);
				break;
			case BL_PWM_VS:
				pr_info("pwm_reg0:           0x%08x\n"
					"pwm_reg1:           0x%08x\n"
					"pwm_reg2:           0x%08x\n"
					"pwm_reg3:           0x%08x\n",
					bl_vcbus_read(VPU_VPU_PWM_V0),
					bl_vcbus_read(VPU_VPU_PWM_V1),
					bl_vcbus_read(VPU_VPU_PWM_V2),
					bl_vcbus_read(VPU_VPU_PWM_V3));
				break;
			default:
				break;
			}
		}
		pr_info("pinmux_flag:        %d\n"
			"pinmux_pointer:     0x%p\n",
			ldim_drv->pinmux_flag,
			ldim_drv->pin);
	} else {
		pr_info("device config is null\n");
	}
}

static int ldim_dev_pwm_channel_register(struct bl_pwm_config_s *bl_pwm,
		struct device_node *blnode)
{
	int ret = 0;
	int index0 = BL_PWM_MAX;
	int index1 = BL_PWM_MAX;
	phandle pwm_phandle;
	struct device_node *pnode = NULL;
	struct device_node *child;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ret = of_property_read_u32(blnode, "bl_pwm_config", &pwm_phandle);
	if (ret) {
		LDIMERR("not match bl_pwm_config node\n");
		return -1;
	}
	pnode = of_find_node_by_phandle(pwm_phandle);
	if (!pnode) {
		LDIMERR("can't find bl_pwm_config node\n");
		return -1;
	}

	/*request for pwm device */
	for_each_child_of_node(pnode, child) {
		ret = of_property_read_u32(child, "pwm_port_index", &index0);
		if (ret) {
			LDIMERR("failed to get pwm_port_index\n");
			return ret;
		}
		ret = of_property_read_u32_index(child, "pwms", 1, &index1);
		if (ret) {
			LDIMERR("failed to get meson_pwm_index\n");
			return ret;
		}

		if (index0 >= BL_PWM_VS)
			continue;
		if ((index0 % 2) != index1)
			continue;
		if (index0 != bl_pwm->pwm_port)
			continue;

		bl_pwm->pwm_data.port_index = index0;
		bl_pwm->pwm_data.meson_index = index1;
		bl_pwm->pwm_data.pwm = devm_of_pwm_get(
			ldim_drv->dev, child, NULL);
		if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
			ret = PTR_ERR(bl_pwm->pwm_data.pwm);
			LDIMERR("unable to request bl_pwm\n");
			return ret;
		}
		bl_pwm->pwm_data.meson = to_meson_pwm(
			bl_pwm->pwm_data.pwm->chip);
		pwm_init_state(bl_pwm->pwm_data.pwm, &(bl_pwm->pwm_data.state));
		LDIMPR("register pwm_ch(%d) 0x%p\n",
			bl_pwm->pwm_data.port_index, bl_pwm->pwm_data.pwm);
	}

	LDIMPR("%s ok\n", __func__);

	return ret;

}

static int ldim_dev_get_config_from_dts(struct device_node *np, int index)
{
	char ld_propname[20];
	struct device_node *child;
	const char *str;
	unsigned int *temp, val;
	int i, j;
	int ret = 0;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	temp = kcalloc(LD_BLKREGNUM, sizeof(unsigned int), GFP_KERNEL);
	if (temp == NULL) {
		LDIMERR("%s: buf malloc error\n", __func__);
		return -1;
	}

	memset(ldim_dev_config.init_on, 0, LDIM_SPI_INIT_ON_SIZE);
	memset(ldim_dev_config.init_off, 0, LDIM_SPI_INIT_OFF_SIZE);
	ldim_dev_config.init_on[0] = 0xff;
	ldim_dev_config.init_off[0] = 0xff;

	/* get device config */
	sprintf(ld_propname, "ldim_dev_%d", index);
	LDIMPR("load: %s\n", ld_propname);
	child = of_get_child_by_name(np, ld_propname);
	if (child == NULL) {
		LDIMERR("failed to get %s\n", ld_propname);
		goto ldim_get_config_err;
	}

	ret = of_property_read_string(child, "ldim_dev_name", &str);
	if (ret) {
		LDIMERR("failed to get ldim_dev_name\n");
		str = "ldim_dev";
	}
	strcpy(ldim_dev_config.name, str);

	ret = of_property_read_string(child, "ldim_pwm_pinmux_sel", &str);
	if (ret) {
		LDIMERR("failed to get ldim_pwm_name\n");
		strcpy(ldim_dev_config.pinmux_name, "invalid");
	} else {
		strcpy(ldim_dev_config.pinmux_name, str);
	}

	ret = of_property_read_string(child, "ldim_pwm_port", &str);
	if (ret) {
		LDIMERR("failed to get ldim_pwm_port\n");
		ldim_dev_config.pwm_config.pwm_port = BL_PWM_MAX;
	} else {
		ldim_dev_config.pwm_config.pwm_port = bl_pwm_str_to_pwm(str);
	}
	LDIMPR("pwm_port: %s(%u)\n", str, ldim_dev_config.pwm_config.pwm_port);

	if (ldim_dev_config.pwm_config.pwm_port < BL_PWM_MAX) {
		ret = of_property_read_u32_array(child, "ldim_pwm_attr",
			temp, 3);
		if (ret) {
			LDIMERR("failed to get ldim_pwm_attr\n");
			ldim_dev_config.pwm_config.pwm_method = BL_PWM_POSITIVE;
			if (ldim_dev_config.pwm_config.pwm_port == BL_PWM_VS)
				ldim_dev_config.pwm_config.pwm_freq = 1;
			else
				ldim_dev_config.pwm_config.pwm_freq = 60;
			ldim_dev_config.pwm_config.pwm_duty = 50;
		} else {
			ldim_dev_config.pwm_config.pwm_method = temp[0];
			ldim_dev_config.pwm_config.pwm_freq = temp[1];
			ldim_dev_config.pwm_config.pwm_duty = temp[2];
		}
		LDIMPR("get pwm pol = %d, freq = %d, duty = %d%%\n",
			ldim_dev_config.pwm_config.pwm_method,
			ldim_dev_config.pwm_config.pwm_freq,
			ldim_dev_config.pwm_config.pwm_duty);

		bl_pwm_config_init(&ldim_dev_config.pwm_config);

		if (ldim_dev_config.pwm_config.pwm_port < BL_PWM_VS) {
			ldim_dev_pwm_channel_register(
				&ldim_dev_config.pwm_config, np);
		}
	}

	ret = of_property_read_u32_array(child, "en_gpio_on_off", temp, 3);
	if (ret) {
		LDIMERR("failed to get en_gpio_on_off\n");
		ldim_dev_config.en_gpio = BL_GPIO_MAX;
		ldim_dev_config.en_gpio_on = BL_GPIO_OUTPUT_HIGH;
		ldim_dev_config.en_gpio_off = BL_GPIO_OUTPUT_LOW;
	} else {
		if (temp[0] >= BL_GPIO_NUM_MAX) {
			ldim_dev_config.en_gpio = BL_GPIO_MAX;
		} else {
			ldim_dev_config.en_gpio = temp[0];
			ldim_gpio_probe(ldim_dev_config.en_gpio);
		}
		ldim_dev_config.en_gpio_on = temp[1];
		ldim_dev_config.en_gpio_off = temp[2];
	}

	ret = of_property_read_u32_array(child, "dim_max_min", &temp[0], 2);
	if (ret) {
		LDIMERR("failed to get dim_max_min\n");
		ldim_dev_config.dim_max = 0xfff;
		ldim_dev_config.dim_min = 0x7f;
	} else {
		ldim_dev_config.dim_max = temp[0];
		ldim_dev_config.dim_min = temp[1];
	}

	val = ldim_drv->ldim_conf->row * ldim_drv->ldim_conf->col;
	ret = of_property_read_u32_array(child, "ldim_region_mapping",
			&temp[0], val);
	if (ret) {
		LDIMERR("failed to get ldim_region_mapping\n");
		for (i = 0; i < LD_BLKREGNUM; i++)
			ldim_dev_config.bl_mapping[i] = (unsigned short)i;
	} else {
		for (i = 0; i < val; i++)
			ldim_dev_config.bl_mapping[i] = (unsigned short)temp[i];
	}
	ldim_dev_config.bl_regnum = (unsigned short)val;

	ret = of_property_read_u32(child, "type", &val);
	if (ret) {
		LDIMERR("failed to get type\n");
		ldim_dev_config.type = LDIM_DEV_TYPE_NORMAL;
	} else {
		ldim_dev_config.type = val;
		if (ldim_debug_print)
			LDIMPR("type: %d\n", ldim_dev_config.type);
	}
	if (ldim_dev_config.type >= LDIM_DEV_TYPE_MAX) {
		LDIMERR("type num is out of support\n");
		goto ldim_get_config_err;
	}

	switch (ldim_dev_config.type) {
	case LDIM_DEV_TYPE_SPI:
		/* get spi config */
		ldim_drv->spi_dev = &ldim_spi_dev;

		ret = of_property_read_u32(child, "spi_bus_num", &val);
		if (ret) {
			LDIMERR("failed to get spi_bus_num\n");
		} else {
			ldim_spi_dev.bus_num = val;
			if (ldim_debug_print) {
				LDIMPR("spi bus_num: %d\n",
					ldim_spi_dev.bus_num);
			}
		}

		ret = of_property_read_u32(child, "spi_chip_select", &val);
		if (ret) {
			LDIMERR("failed to get spi_chip_select\n");
		} else {
			ldim_spi_dev.chip_select = val;
			if (ldim_debug_print) {
				LDIMPR("spi chip_select: %d\n",
					ldim_spi_dev.chip_select);
			}
		}

		ret = of_property_read_u32(child, "spi_max_frequency", &val);
		if (ret) {
			LDIMERR("failed to get spi_chip_select\n");
		} else {
			ldim_spi_dev.max_speed_hz = val;
			if (ldim_debug_print) {
				LDIMPR("spi max_speed_hz: %d\n",
					ldim_spi_dev.max_speed_hz);
			}
		}

		ret = of_property_read_u32(child, "spi_mode", &val);
		if (ret) {
			LDIMERR("failed to get spi_mode\n");
		} else {
			ldim_spi_dev.mode = val;
			if (ldim_debug_print)
				LDIMPR("spi mode: %d\n", ldim_spi_dev.mode);
		}

		ret = of_property_read_u32_array(child, "spi_cs_delay",
			&temp[0], 2);
		if (ret) {
			ldim_dev_config.cs_hold_delay = 0;
			ldim_dev_config.cs_clk_delay = 0;
		} else {
			ldim_dev_config.cs_hold_delay = temp[0];
			ldim_dev_config.cs_clk_delay = temp[1];
		}

		ret = of_property_read_u32(child, "lamp_err_gpio", &val);
		if (ret) {
			ldim_dev_config.lamp_err_gpio = BL_GPIO_NUM_MAX;
			ldim_dev_config.fault_check = 0;
		} else {
			if (val >= BL_GPIO_NUM_MAX) {
				ldim_dev_config.lamp_err_gpio = BL_GPIO_NUM_MAX;
				ldim_dev_config.fault_check = 0;
			} else {
				ldim_dev_config.lamp_err_gpio = val;
				ldim_dev_config.fault_check = 1;
				ldim_gpio_probe(
					ldim_dev_config.lamp_err_gpio);
				ldim_gpio_set(ldim_dev_config.lamp_err_gpio,
					BL_GPIO_INPUT);
			}
		}

		ret = of_property_read_u32(child, "spi_write_check", &val);
		if (ret)
			ldim_dev_config.write_check = 0;
		else
			ldim_dev_config.write_check = (unsigned char)val;

		/* get init_cmd */
		ret = of_property_read_u32(child, "cmd_size", &val);
		if (ret) {
			LDIMPR("no cmd_size\n");
			ldim_dev_config.cmd_size = 1;
		} else {
			if (val > 1)
				ldim_dev_config.cmd_size = (unsigned char)val;
			else
				ldim_dev_config.cmd_size = 1;
			}

		ret = of_property_read_u32_index(child, "init_on", 0, &val);
		if (ret) {
			LDIMPR("no init_on\n");
			ldim_dev_config.init_on[0] = 0xff;
			goto ldim_get_init_off;
		}
		if (ldim_dev_config.cmd_size > 1) {
			i = 0;
			while (i < LDIM_SPI_INIT_ON_SIZE) {
				for (j = 0; j < ldim_dev_config.cmd_size; j++) {
					ret = of_property_read_u32_index(child,
						"init_on", (i + j), &val);
					if (ret) {
						LDIMERR("failed init_on\n");
						ldim_dev_config.init_on[i]
							= 0xff;
						goto ldim_get_init_off;
					}
					ldim_dev_config.init_on[i + j] =
						(unsigned char)val;
				}
				if (ldim_dev_config.init_on[i] == 0xff)
					break;

				i += ldim_dev_config.cmd_size;
				}
			}
ldim_get_init_off:
		ret = of_property_read_u32_index(child, "init_off", 0, &val);
		if (ret) {
			LDIMPR("no init_off\n");
			ldim_dev_config.init_off[0] = 0xff;
			goto ldim_get_config_end;
		}
		if (ldim_dev_config.cmd_size > 1) {
			i = 0;
			while (i < LDIM_SPI_INIT_OFF_SIZE) {
				for (j = 0; j < ldim_dev_config.cmd_size; j++) {
					ret = of_property_read_u32_index(child,
						"init_off", (i + j), &val);
					if (ret) {
						LDIMERR("failed init_on\n");
						ldim_dev_config.init_off[i]
							= 0xff;
						goto ldim_get_config_end;
					}
					ldim_dev_config.init_off[i + j] =
						(unsigned char)val;
				}
				if (ldim_dev_config.init_off[i] == 0xff)
					break;

				i += ldim_dev_config.cmd_size;
			}
		}
ldim_get_config_end:
		break;
	case LDIM_DEV_TYPE_I2C:
		break;
	case LDIM_DEV_TYPE_NORMAL:
	default:
		break;
	}

	kfree(temp);
	return 0;

ldim_get_config_err:
	kfree(temp);
	return -1;
}

static int ldim_dev_add_driver(struct ldim_dev_config_s *ldev_conf, int index)
{
	int ret = 0;

	if (strcmp(ldev_conf->name, "iw7027") == 0) {
		ret = ldim_dev_iw7027_probe();
		goto ldim_dev_add_driver_next;
	} else if (strcmp(ldev_conf->name, "ob3350") == 0) {
		ret = ldim_dev_ob3350_probe();
		goto ldim_dev_add_driver_next;
	} else if (strcmp(ldev_conf->name, "global") == 0) {
		ret = ldim_dev_global_probe();
		goto ldim_dev_add_driver_next;
	} else {
		LDIMERR("invalid device name: %s\n", ldev_conf->name);
		ret = -1;
	}

ldim_dev_add_driver_next:
	if (ret) {
		LDIMERR("add device driver failed %s(%d)\n",
			ldev_conf->name, index);
	} else {
		LDIMPR("add device driver %s(%d)\n", ldev_conf->name, index);
	}

	return ret;
}

static int ldim_dev_remove_driver(struct ldim_dev_config_s *ldev_conf,
		int index)
{
	int ret = 0;

	if (strcmp(ldev_conf->name, "iw7027") == 0) {
		ret = ldim_dev_iw7027_remove();
		goto ldim_dev_remove_driver_next;
	} else if (strcmp(ldev_conf->name, "ob3350") == 0) {
		ret = ldim_dev_ob3350_remove();
		goto ldim_dev_remove_driver_next;
	} else if (strcmp(ldev_conf->name, "global") == 0) {
		ret = ldim_dev_global_remove();
		goto ldim_dev_remove_driver_next;
	} else {
		LDIMERR("invalid device name: %s\n", ldev_conf->name);
		ret = -1;
	}

ldim_dev_remove_driver_next:
	if (ret) {
		LDIMERR("remove device driver failed %s(%d)\n",
			ldev_conf->name, index);
	} else {
		LDIMPR("remove device driver %s(%d)\n", ldev_conf->name, index);
	}

	return ret;
}

static int ldim_dev_probe(struct platform_device *pdev)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	if (ldim_drv->dev_index != 0xff) {
		/* get configs */
		ldim_drv->dev = &pdev->dev;
		ldim_drv->ldev_conf = &ldim_dev_config;
		ldim_drv->pinmux_ctrl = ldim_pwm_pinmux_ctrl;
		ldim_drv->pwm_vs_update = ldim_pwm_vs_update;
		ldim_drv->config_print = ldim_config_print,
		ldim_dev_get_config_from_dts(pdev->dev.of_node,
			ldim_drv->dev_index);

		ldim_dev_add_driver(ldim_drv->ldev_conf, ldim_drv->dev_index);
	}
	/* init ldim function */
	if (ldim_drv->valid_flag)
		ldim_drv->init();
	LDIMPR("%s OK\n", __func__);

	return ret;
}

static int __exit ldim_dev_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	if (ldim_drv->dev_index != 0xff) {
		ldim_dev_remove_driver(ldim_drv->ldev_conf,
			ldim_drv->dev_index);
	}
	LDIMPR("%s OK\n", __func__);
	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id ldim_dev_dt_match[] = {
	{
		.compatible = "amlogic, ldim_dev",
	},
	{},
};
#endif

static struct platform_driver ldim_dev_platform_driver = {
	.driver = {
		.name  = "ldim_dev",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ldim_dev_dt_match,
#endif
	},
	.probe   = ldim_dev_probe,
	.remove  = __exit_p(ldim_dev_remove),
};

static int __init ldim_dev_init(void)
{
	if (platform_driver_register(&ldim_dev_platform_driver)) {
		LDIMPR("failed to register ldim_dev driver module\n");
		return -ENODEV;
	}
	return 0;
}

static void __exit ldim_dev_exit(void)
{
	platform_driver_unregister(&ldim_dev_platform_driver);
}

late_initcall(ldim_dev_init);
module_exit(ldim_dev_exit);


MODULE_DESCRIPTION("LDIM device Driver for LCD Backlight");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");

