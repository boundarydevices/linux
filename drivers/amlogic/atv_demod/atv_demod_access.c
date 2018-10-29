/*
 * drivers/amlogic/atv_demod/atv_demod_access.c
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

#include <linux/types.h>
#include <linux/io.h>
#include <linux/amlogic/cpu_version.h>

#include "drivers/amlogic/media/vin/tvin/tvafe/tvafe_general.h"

#include "atvdemod_func.h"
#include "atv_demod_access.h"
#include "atv_demod_debug.h"
#include "atv_demod_driver.h"


int amlatvdemod_reg_read(unsigned int reg, unsigned int *val)
{
#if 0 /* Don't need to check the CLK and PLL states, it's done in init */
	int ret = 0;

	if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
		amlatvdemod_hiu_reg_read(HHI_GCLK_MPEG0, &ret);
		if (0 == ((1 << 29) & ret)) {
			pr_err("%s GCLK_MPEG0:0x%x\n", __func__, ret);
			return 0;
		}
	} else if (0 == (ADC_EN_ATV_DEMOD & tvafe_adc_get_pll_flag())) {
		/* pr_dbg("%s atv demod pll not init\n", __func__); */
		return 0;
	}
#endif

	*val = readl(amlatvdemod_devp->demod_reg_base + reg);

	return 0;
}

int amlatvdemod_reg_write(unsigned int reg, unsigned int val)
{
#if 0 /* Don't need to check the CLK and PLL states, it's done in init */
	int ret = 0;

	if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
		amlatvdemod_hiu_reg_read(HHI_GCLK_MPEG0, &ret);
		if (0 == ((1 << 29) & ret)) {
			pr_err("%s GCLK_MPEG0:0x%x\n", __func__, ret);
			return 0;
		}
	} else if (0 == (ADC_EN_ATV_DEMOD & tvafe_adc_get_pll_flag())) {
		/* pr_dbg("%s atv demod pll not init\n", __func__); */
		return 0;
	}
#endif

	writel(val, (amlatvdemod_devp->demod_reg_base + reg));

	return 0;
}

int atvaudiodem_reg_read(unsigned int reg, unsigned int *val)
{
	int ret = 0;

	if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
		amlatvdemod_hiu_reg_read(HHI_GCLK_MPEG0, &ret);
		if (0 == ((1 << 31) & ret)) {
			pr_err("%s GCLK_MPEG0:0x%x\n", __func__, ret);
			return 0;
		}
	}

	if (amlatvdemod_devp->audio_reg_base)
		*val = readl(amlatvdemod_devp->audio_reg_base + reg);

	return 0;
}

int atvaudiodem_reg_write(unsigned int reg, unsigned int val)
{
	int ret = 0;

	if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
		amlatvdemod_hiu_reg_read(HHI_GCLK_MPEG0, &ret);
		if (0 == ((1 << 31) & ret)) {
			pr_err("%s GCLK_MPEG0:0x%x\n", __func__, ret);
			return 0;
		}
	}

	if (amlatvdemod_devp->audio_reg_base)
		writel(val, (amlatvdemod_devp->audio_reg_base + reg));

	return 0;
}

int amlatvdemod_hiu_reg_read(unsigned int reg, unsigned int *val)
{
	if (amlatvdemod_devp->hiu_reg_base)
		*val = readl(amlatvdemod_devp->hiu_reg_base +
				((reg - 0x1000) << 2));

	return 0;
}

int amlatvdemod_hiu_reg_write(unsigned int reg, unsigned int val)
{
	if (amlatvdemod_devp->hiu_reg_base)
		writel(val, (amlatvdemod_devp->hiu_reg_base +
				((reg - 0x1000) << 2)));

	return 0;
}

int amlatvdemod_periphs_reg_read(unsigned int reg, unsigned int *val)
{
	if (amlatvdemod_devp->periphs_reg_base)
		*val = readl(amlatvdemod_devp->periphs_reg_base +
				((reg - 0x1000) << 2));

	return 0;
}

int amlatvdemod_periphs_reg_write(unsigned int reg, unsigned int val)
{
	if (amlatvdemod_devp->periphs_reg_base)
		writel(val, (amlatvdemod_devp->periphs_reg_base +
				((reg - 0x1000) << 2)));

	return 0;
}

void atv_dmd_wr_reg(unsigned char block, unsigned char reg, unsigned long data)
{
	unsigned long reg_addr = (block << 8) + reg * 4;

	amlatvdemod_reg_write(reg_addr, data);
}

unsigned long atv_dmd_rd_reg(unsigned char block, unsigned char reg)
{
	unsigned long data = 0;
	unsigned int reg_addr = (block << 8) + reg * 4;

	amlatvdemod_reg_read(reg_addr, (unsigned int *)&data);
	return data;
}

unsigned long atv_dmd_rd_byte(unsigned long block_addr, unsigned long reg_addr)
{
	unsigned long data = 0;

	data = atv_dmd_rd_long(block_addr, reg_addr);

	if ((reg_addr & 0x3) == 0)
		data = data >> 24;
	else if ((reg_addr & 0x3) == 1)
		data = (data >> 16 & 0xff);
	else if ((reg_addr & 0x3) == 2)
		data = (data >> 8 & 0xff);
	else if ((reg_addr & 0x3) == 3)
		data = (data >> 0 & 0xff);

	return data;
}

unsigned long atv_dmd_rd_word(unsigned long block_addr, unsigned long reg_addr)
{
	unsigned long data = 0;

	data = atv_dmd_rd_long(block_addr, reg_addr);

	if ((reg_addr & 0x3) == 0)
		data = data >> 16;
	else if ((reg_addr & 0x3) == 1)
		data = (data >> 8 & 0xffff);
	else if ((reg_addr & 0x3) == 2)
		data = (data >> 0 & 0xffff);
	else if ((reg_addr & 0x3) == 3)
		data = (((data & 0xff) << 8) | ((data >> 24) & 0xff));

	return data;
}

unsigned long atv_dmd_rd_long(unsigned long block_addr, unsigned long reg_addr)
{
	unsigned long data = 0;

	data = R_ATVDEMOD_REG((((block_addr & 0xff) << 6) +
			((reg_addr & 0xff) >> 2)) << 2);

	return data;
}

void atv_dmd_wr_long(unsigned long block_addr, unsigned long reg_addr,
		unsigned long data)
{
	W_ATVDEMOD_REG((((block_addr & 0xff) << 6) +
		((reg_addr & 0xff) >> 2)) << 2, data);
}

void atv_dmd_wr_word(unsigned long block_addr, unsigned long reg_addr,
		unsigned long data)
{
	unsigned long data_tmp = 0;

	data_tmp = atv_dmd_rd_long(block_addr, reg_addr);
	data = data & 0xffff;
	if ((reg_addr & 0x3) == 0)
		data = (data << 16 | (data_tmp & 0xffff));
	else if ((reg_addr & 0x3) == 1)
		data = ((data_tmp & 0xff000000) |
				(data << 8) | (data_tmp & 0xff));
	else if ((reg_addr & 0x3) == 2)
		data = (data | (data_tmp & 0xffff0000));
	else if ((reg_addr & 0x3) == 3)
		data = (((data & 0xff) << 24) |
				((data_tmp & 0xffff0000) >> 8) |
				((data & 0xff00) >> 8));

	atv_dmd_wr_long(block_addr, reg_addr, data);
}

void atv_dmd_wr_byte(unsigned long block_addr, unsigned long reg_addr,
		unsigned long data)
{
	unsigned long data_tmp = 0;

	data_tmp = atv_dmd_rd_long(block_addr, reg_addr);

	data = data & 0xff;

	if ((reg_addr & 0x3) == 0) {
		data = (data << 24 | (data_tmp & 0xffffff));
	} else if ((reg_addr & 0x3) == 1)
		data = ((data_tmp & 0xff000000) |
				(data << 16) | (data_tmp & 0xffff));
	else if ((reg_addr & 0x3) == 2)
		data = ((data_tmp & 0xffff0000) |
				(data << 8) | (data_tmp & 0xff));
	else if ((reg_addr & 0x3) == 3)
		data = ((data_tmp & 0xffffff00) | (data & 0xff));

	atv_dmd_wr_long(block_addr, reg_addr, data);
}

void adec_wr_reg(uint32_t reg, uint32_t val)
{
	atvaudiodem_reg_write(reg << 2, val);
}

uint32_t adec_rd_reg(uint32_t addr)
{
	return R_AUDDEMOD_REG(addr);
}
