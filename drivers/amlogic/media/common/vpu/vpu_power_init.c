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
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "vpu_reg.h"
#include "vpu.h"

void vpu_mem_pd_init_off(void)
{
	return;

	VPUPR("%s\n", __func__);
}

void vpu_clk_gate_init_off(void)
{
	VPUPR("%s\n", __func__);

	switch (vpu_conf.data->chip_type) {
	case VPU_CHIP_TXLX:
		/* dolby core1 */
		vpu_vcbus_setb(DOLBY_TV_CLKGATE_CTRL, 1, 10, 2);
		vpu_vcbus_setb(DOLBY_TV_CLKGATE_CTRL, 1, 2, 2);
		vpu_vcbus_setb(DOLBY_TV_CLKGATE_CTRL, 1, 4, 2);
		/* dolby core2 */
		vpu_vcbus_setb(DOLBY_CORE2A_CLKGATE_CTRL, 1, 10, 2);
		vpu_vcbus_setb(DOLBY_CORE2A_CLKGATE_CTRL, 1, 2, 2);
		vpu_vcbus_setb(DOLBY_CORE2A_CLKGATE_CTRL, 1, 4, 2);
		/* dolby core3 */
		vpu_vcbus_setb(DOLBY_CORE3_CLKGATE_CTRL, 0, 1, 1);
		vpu_vcbus_setb(DOLBY_CORE3_CLKGATE_CTRL, 1, 2, 2);
		break;
	case VPU_CHIP_GXM:
		vpu_vcbus_write(DOLBY_CORE1_CLKGATE_CTRL, 0x55555555);
		vpu_vcbus_write(DOLBY_CORE2A_CLKGATE_CTRL, 0x55555555);
		vpu_vcbus_write(DOLBY_CORE3_CLKGATE_CTRL, 0x55555555);
		break;
	default:
		break;
	}

	if (vpu_debug_print_flag)
		VPUPR("%s finish\n", __func__);
}

void vpu_module_init_config(void)
{
	VPUPR("%s\n", __func__);

	/* dmc_arb_config */
	vpu_vcbus_write(VPU_RDARB_MODE_L1C1, 0x210000);
	vpu_vcbus_write(VPU_RDARB_MODE_L1C2, 0x10000);
	vpu_vcbus_write(VPU_RDARB_MODE_L2C1, 0x900000);
	vpu_vcbus_write(VPU_WRARB_MODE_L2C1, 0x20000);

	if (vpu_debug_print_flag)
		VPUPR("%s finish\n", __func__);
}

void vpu_power_on_gx(void)
{
	struct vpu_ctrl_s *table;
	unsigned int _reg, _bit, _len;
	int i = 0, cnt;

	VPUPR("vpu_power_on\n");

	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 0, 8, 1); /* [8] power on */
	udelay(20);

	/* power up memories */
	cnt = vpu_conf.data->mem_pd_table_cnt;
	table = vpu_conf.data->mem_pd_table;
	while (i < cnt) {
		if (table[i].vmod == VPU_MOD_MAX)
			break;
		_reg = table[i].reg;
		_bit = table[i].bit;
		_len = table[i].len;
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
	vpu_cbus_clr_mask(RESET0_LEVEL, ((1<<5) | (1<<10) | (1<<19) | (1<<13)));
	vpu_cbus_clr_mask(RESET1_LEVEL, (1<<5));
	vpu_cbus_clr_mask(RESET2_LEVEL, (1<<15));
	vpu_cbus_clr_mask(RESET4_LEVEL, ((1<<6) | (1<<7) | (1<<13) | (1<<5) |
					(1<<9) | (1<<4) | (1<<12)));
	vpu_cbus_clr_mask(RESET7_LEVEL, (1<<7));

	/* Remove VPU_HDMI ISO */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 0, 9, 1); /* [9] VPU_HDMI */

	/* release Reset */
	vpu_cbus_set_mask(RESET0_LEVEL, ((1 << 5) | (1<<10) | (1<<19) |
					(1<<13)));
	vpu_cbus_set_mask(RESET1_LEVEL, (1<<5));
	vpu_cbus_set_mask(RESET2_LEVEL, (1<<15));
	vpu_cbus_set_mask(RESET4_LEVEL, ((1<<6) | (1<<7) | (1<<13) | (1<<5) |
					(1<<9) | (1<<4) | (1<<12)));
	vpu_cbus_set_mask(RESET7_LEVEL, (1<<7));

	if (vpu_debug_print_flag)
		VPUPR("%s finish\n", __func__);
}

void vpu_power_off_gx(void)
{
	struct vpu_ctrl_s *table;
	unsigned int _reg, _bit, _len;
	int i = 0, cnt;

	VPUPR("vpu_power_off\n");

	/* Power down VPU_HDMI */
	/* Enable Isolation */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 1, 9, 1); /* ISO */
	udelay(20);

	/* power down memories */
	cnt = vpu_conf.data->mem_pd_table_cnt;
	table = vpu_conf.data->mem_pd_table;
	while (i < cnt) {
		if (table[i].vmod == VPU_MOD_MAX)
			break;
		_reg = table[i].reg;
		_bit = table[i].bit;
		_len = table[i].len;
		vpu_hiu_setb(_reg, 0x3, _bit, _len);
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

void vpu_power_on_txlx(void)
{
	struct vpu_ctrl_s *table;
	unsigned int _reg, _bit, _len;
	int i = 0, cnt;

	VPUPR("vpu_power_on\n");

	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 0, 8, 1); /* [8] power on */
	udelay(20);

	/* power up memories */
	cnt = vpu_conf.data->mem_pd_table_cnt;
	table = vpu_conf.data->mem_pd_table;
	while (i < cnt) {
		if (table[i].vmod == VPU_MOD_MAX)
			break;
		_reg = table[i].reg;
		_bit = table[i].bit;
		_len = table[i].len;
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
	vpu_cbus_clr_mask(RESET0_LEVEL_TXLX, ((1<<5) | (1<<10) | (1<<19) |
					(1<<13)));
	vpu_cbus_clr_mask(RESET1_LEVEL_TXLX, (1<<5));
	vpu_cbus_clr_mask(RESET2_LEVEL_TXLX, (1<<15));
	vpu_cbus_clr_mask(RESET4_LEVEL_TXLX, ((1<<6) | (1<<7) | (1<<13) |
					(1<<5) | (1<<9) | (1<<4) | (1<<12)));
	vpu_cbus_clr_mask(RESET7_LEVEL_TXLX, (1<<7));

	/* Remove VPU_HDMI ISO */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 0, 9, 1); /* [9] VPU_HDMI */

	/* release Reset */
	vpu_cbus_set_mask(RESET0_LEVEL_TXLX, ((1 << 5) | (1<<10) | (1<<19) |
					(1<<13)));
	vpu_cbus_set_mask(RESET1_LEVEL_TXLX, (1<<5));
	vpu_cbus_set_mask(RESET2_LEVEL_TXLX, (1<<15));
	vpu_cbus_set_mask(RESET4_LEVEL_TXLX, ((1<<6) | (1<<7) | (1<<13) |
					(1<<5) | (1<<9) | (1<<4) | (1<<12)));
	vpu_cbus_set_mask(RESET7_LEVEL_TXLX, (1<<7));

	if (vpu_debug_print_flag)
		VPUPR("%s finish\n", __func__);
}

void vpu_power_off_txlx(void)
{
	struct vpu_ctrl_s *table;
	unsigned int _reg, _bit, _len;
	int i = 0, cnt;

	VPUPR("vpu_power_off\n");

	/* Power down VPU_HDMI */
	/* Enable Isolation */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 1, 9, 1); /* ISO */
	udelay(20);

	/* power down memories */
	cnt = vpu_conf.data->mem_pd_table_cnt;
	table = vpu_conf.data->mem_pd_table;
	while (i < cnt) {
		if (table[i].vmod == VPU_MOD_MAX)
			break;
		_reg = table[i].reg;
		_bit = table[i].bit;
		_len = table[i].len;
		vpu_hiu_setb(_reg, 0x3, _bit, _len);
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

