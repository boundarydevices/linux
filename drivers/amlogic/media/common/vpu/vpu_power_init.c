/*
 * drivers/amlogic/media/common/vpu/vpu_power_init.c
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
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "vpu_reg.h"
#include "vpu.h"

void vpu_mem_pd_init_off(void)
{
	return;

	VPUPR("%s\n", __func__);
}

void vpu_module_init_config(void)
{
	struct vpu_ctrl_s *ctrl_table;
	unsigned int _reg, _val, _bit, _len;
	int i = 0, cnt;

	VPUPR("%s\n", __func__);

	/* vpu clk gate init off */
	cnt = vpu_conf.data->module_init_table_cnt;
	ctrl_table = vpu_conf.data->module_init_table;
	if (ctrl_table) {
		while (i < cnt) {
			if (ctrl_table[i].reg == VPU_REG_END)
				break;
			_reg = ctrl_table[i].reg;
			_val = ctrl_table[i].val;
			_bit = ctrl_table[i].bit;
			_len = ctrl_table[i].len;
			vpu_vcbus_setb(_reg, _val, _bit, _len);
			i++;
		}
	}

	/* dmc_arb_config */
	vpu_vcbus_write(VPU_RDARB_MODE_L1C1, 0x210000);
	vpu_vcbus_write(VPU_RDARB_MODE_L1C2, 0x10000);
	vpu_vcbus_write(VPU_RDARB_MODE_L2C1, 0x900000);
  	/*from vlsi feijun*/
	vpu_vcbus_write(VPU_WRARB_MODE_L2C1, 0x170000/*0x20000*/);

	if (vpu_debug_print_flag)
		VPUPR("%s finish\n", __func__);
}

void vpu_power_on(void)
{
	struct vpu_ctrl_s *ctrl_table;
	struct vpu_reset_s *reset_table;
	unsigned int _reg, _bit, _len, mask;
	int i = 0, cnt;

	VPUPR("vpu_power_on\n");

	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 0, 8, 1); /* [8] power on */
	udelay(20);

	/* power up memories */
	cnt = vpu_conf.data->mem_pd_table_cnt;
	ctrl_table = vpu_conf.data->mem_pd_table;
	while (i < cnt) {
		if (ctrl_table[i].vmod == VPU_MOD_MAX)
			break;
		_reg = ctrl_table[i].reg;
		_bit = ctrl_table[i].bit;
		_len = ctrl_table[i].len;
		vpu_hiu_setb(_reg, 0x0, _bit, _len);
		udelay(5);
		i++;
	}
	for (i = 8; i < 16; i++) {
		vpu_hiu_setb(HHI_MEM_PD_REG0, 0, i, 1);
		udelay(5);
	}
	udelay(20);

	/* Reset VIU + VENC */
	/* Reset VENCI + VENCP + VADC + VENCL */
	/* Reset HDMI-APB + HDMI-SYS + HDMI-TX + HDMI-CEC */
	reset_table = vpu_conf.data->reset_table;
	i = 0;
	while (i < VPU_RESET_CNT_MAX) {
		if (reset_table[i].reg == VPU_REG_END)
			break;
		_reg = reset_table[i].reg;
		mask = reset_table[i].mask;
		vpu_cbus_clr_mask(_reg, mask);
		i++;
	}
	udelay(5);
	/* release Reset */
	i = 0;
	while (i < VPU_RESET_CNT_MAX) {
		if (reset_table[i].reg == VPU_REG_END)
			break;
		_reg = reset_table[i].reg;
		mask = reset_table[i].mask;
		vpu_cbus_set_mask(_reg, mask);
		i++;
	}

	/* Remove VPU_HDMI ISO */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 0, 9, 1); /* [9] VPU_HDMI */

	if (vpu_debug_print_flag)
		VPUPR("%s finish\n", __func__);
}

void vpu_power_off(void)
{
	struct vpu_ctrl_s *ctrl_table;
	unsigned int _val, _reg, _bit, _len;
	int i = 0, cnt;

	VPUPR("vpu_power_off\n");

	/* Power down VPU_HDMI */
	/* Enable Isolation */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 1, 9, 1); /* ISO */
	udelay(20);

	/* power down memories */
	cnt = vpu_conf.data->mem_pd_table_cnt;
	ctrl_table = vpu_conf.data->mem_pd_table;
	while (i < cnt) {
		if (ctrl_table[i].vmod == VPU_MOD_MAX)
			break;
		_reg = ctrl_table[i].reg;
		_bit = ctrl_table[i].bit;
		_len = ctrl_table[i].len;
		if (_len == 32)
			_val = 0xffffffff;
		else
			_val = (1 << _len) - 1;
		vpu_hiu_setb(_reg, _val, _bit, _len);
		udelay(5);
		i++;
	}
	for (i = 8; i < 16; i++) {
		vpu_hiu_setb(HHI_MEM_PD_REG0, 0x1, i, 1);
		udelay(5);
	}
	udelay(20);

	/* Power down VPU domain */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 1, 8, 1); /* PDN */

	vpu_hiu_setb(HHI_VAPBCLK_CNTL, 0, 8, 1);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 0, 8, 1);

	if (vpu_debug_print_flag)
		VPUPR("%s finish\n", __func__);
}

