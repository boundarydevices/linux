/*
 * drivers/amlogic/media/common/vpu/vpu.c
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
#include "vpu_ctrl.h"
#include "vpu_module.h"

/* v01: initial version */
/* v02: add axg support */
/* v03: add txlx support */
/* v04: add g12a support */
/* v05: add txl support */
/* v20180925: add tl1 support */
/* v20190314: add sm1 support */
#define VPU_VERION        "v20190314"

int vpu_debug_print_flag;
static spinlock_t vpu_mem_lock;
static spinlock_t vpu_clk_gate_lock;
static DEFINE_MUTEX(vpu_clk_mutex);

struct vpu_conf_s vpu_conf = {
	.data = NULL,
	.clk_level = 0,
	.mem_pd0 = 0,
	.mem_pd1 = 0,
	.mem_pd2 = 0,

	/* clocktree */
	.gp_pll = NULL,
	.vpu_clk0 = NULL,
	.vpu_clk1 = NULL,
	.vpu_clk = NULL,

	.clk_vmod = NULL,
};

int vpu_chip_valid_check(void)
{
	int ret = 0;

	if (vpu_conf.data == NULL) {
		VPUERR("invalid VPU in current chip\n");
		ret = -1;
	}
	return ret;
}

static unsigned int get_vpu_clk_level_max_vmod(void)
{
	unsigned int max_level = 0;
	int i;

	if (vpu_conf.clk_vmod == NULL)
		return max_level;

	max_level = 0;
	for (i = 0; i < VPU_MOD_MAX; i++) {
		if (vpu_conf.clk_vmod[i] > max_level)
			max_level = vpu_conf.clk_vmod[i];
	}

	return max_level;
}

static unsigned int get_vpu_clk_level(unsigned int video_clk)
{
	unsigned int clk_level;
	int i;

	for (i = 0; i < vpu_conf.data->clk_level_max; i++) {
		if (video_clk <= vpu_clk_table[i].freq)
			break;
	}
	clk_level = i;

	return clk_level;
}

static unsigned int get_fclk_div_freq(unsigned int mux_id)
{
	struct fclk_div_s *fclk_div;
	unsigned int fclk, div, clk_source = 0;
	unsigned int i;

	fclk = CLK_FPLL_FREQ * 1000000;

	for (i = 0; i < FCLK_DIV_MAX; i++) {
		fclk_div = vpu_conf.data->fclk_div_table + i;
		if (fclk_div->fclk_id == mux_id) {
			div = fclk_div->fclk_div;
			clk_source = fclk / div;
			break;
		}
		if (fclk_div->fclk_id == FCLK_DIV_MAX)
			break;
	}

	return clk_source;
}

static unsigned int get_vpu_clk_mux_id(void)
{
	struct fclk_div_s *fclk_div;
	unsigned int i, mux, mux_id;

	mux = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 9, 3);
	mux_id = mux;
	for (i = 0; i < FCLK_DIV_MAX; i++) {
		fclk_div = vpu_conf.data->fclk_div_table + i;
		if (fclk_div->fclk_mux == mux) {
			mux_id = fclk_div->fclk_id;
			break;
		}
		if (fclk_div->fclk_id == FCLK_DIV_MAX)
			break;
	}

	return mux_id;
}

unsigned int get_vpu_clk(void)
{
	unsigned int clk_freq;
	unsigned int clk_source, div;
	unsigned int mux_id;
	struct clk_hw *hw;

	if (IS_ERR_OR_NULL(vpu_conf.vpu_clk)) {
		VPUERR("%s: vpu_clk\n", __func__);
		mux_id = get_vpu_clk_mux_id();
		switch (mux_id) {
		case FCLK_DIV4:
		case FCLK_DIV3:
		case FCLK_DIV5:
		case FCLK_DIV7:
			clk_source = get_fclk_div_freq(mux_id);
			break;
		case GPLL_CLK:
			clk_source = vpu_clk_table[8].freq;
			break;
		default:
			clk_source = 0;
			break;
		}

		div = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 0, 7) + 1;
		clk_freq = clk_source / div;

		return clk_freq;
	}

	hw = __clk_get_hw(vpu_conf.vpu_clk);
	clk_freq = clk_hw_get_rate(hw);
	return clk_freq;
}

static int switch_gp_pll(int flag)
{
	unsigned int clk;

	if (IS_ERR_OR_NULL(vpu_conf.gp_pll)) {
		VPUERR("%s: gp_pll\n", __func__);
		return -1;
	}

	if (flag) { /* enable */
		clk = vpu_clk_table[8].freq;
		clk_set_rate(vpu_conf.gp_pll, clk);
		clk_prepare_enable(vpu_conf.gp_pll);
	} else { /* disable */
		clk_disable_unprepare(vpu_conf.gp_pll);
	}

	return 0;
}

static int adjust_vpu_clk(unsigned int clk_level)
{
	unsigned int clk;
	int ret = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;

	if (vpu_clk_table[vpu_conf.clk_level].mux == GPLL_CLK) {
		if (vpu_conf.data->gp_pll_valid == 0) {
			VPUERR("gp_pll is invalid\n");
			return -1;
		}

		ret = switch_gp_pll(1);
		if (ret) {
			VPUERR("%s: exit\n", __func__);
			return ret;
		}
	}

	if ((IS_ERR_OR_NULL(vpu_conf.vpu_clk0)) ||
		(IS_ERR_OR_NULL(vpu_conf.vpu_clk1)) ||
		(IS_ERR_OR_NULL(vpu_conf.vpu_clk))) {
		VPUERR("%s: vpu_clk\n", __func__);
		return -1;
	}

	vpu_conf.clk_level = clk_level;

	/* step 1:  switch to 2nd vpu clk patch */
	clk = vpu_clk_table[vpu_conf.data->clk_level_dft].freq;
	clk_set_rate(vpu_conf.vpu_clk1, clk);
	clk_set_parent(vpu_conf.vpu_clk, vpu_conf.vpu_clk1);
	udelay(10);
	/* step 2:  adjust 1st vpu clk frequency */
	clk = vpu_clk_table[vpu_conf.clk_level].freq;
	clk_set_rate(vpu_conf.vpu_clk0, clk);
	udelay(20);
	/* step 3:  switch back to 1st vpu clk patch */
	clk_set_parent(vpu_conf.vpu_clk, vpu_conf.vpu_clk0);

	clk = clk_get_rate(vpu_conf.vpu_clk);
	VPUPR("set vpu clk: %uHz(%d), readback: %uHz(0x%x)\n",
		vpu_clk_table[vpu_conf.clk_level].freq, vpu_conf.clk_level,
		clk, (vpu_hiu_read(HHI_VPU_CLK_CNTL)));

	return ret;
}

static int set_vpu_clk(unsigned int vclk)
{
	int ret = 0;
	unsigned int clk_level, clk;

	mutex_lock(&vpu_clk_mutex);

	if (vclk >= 100) { /* regard as vpu_clk */
		clk_level = get_vpu_clk_level(vclk);
	} else { /* regard as clk_level */
		clk_level = vclk;
	}

	if (clk_level >= vpu_conf.data->clk_level_max) {
		ret = 1;
		VPUERR("set vpu clk out of supported range\n");
		goto set_vpu_clk_limit;
	}
#ifdef LIMIT_VPU_CLK_LOW
	if (clk_level < vpu_conf.data->clk_level_dft) {
		ret = 3;
		VPUERR("set vpu clk less than system default\n");
		goto set_vpu_clk_limit;
	}
#endif
	if (clk_level >= sizeof(vpu_clk_table) / sizeof(struct vpu_clk_s)) {
		ret = 7;
		VPUERR("clk_level %d is invalid\n", clk_level);
		goto set_vpu_clk_limit;
	}

	clk = get_vpu_clk();
	if ((clk > (vpu_clk_table[clk_level].freq + VPU_CLK_TOLERANCE)) ||
		(clk < (vpu_clk_table[clk_level].freq - VPU_CLK_TOLERANCE)))
		ret = adjust_vpu_clk(clk_level);

set_vpu_clk_limit:
	mutex_unlock(&vpu_clk_mutex);
	return ret;
}

/* *********************************************** */
/* VPU_CLK control */
/* *********************************************** */
/*
 *  Function: get_vpu_clk_vmod
 *      Get vpu clk holding frequency with specified vmod
 *
 *      Parameters:
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *
 *  Returns:
 *      unsigned int, vpu clk frequency unit in Hz
 *
 *      Example:
 *      video_clk = get_vpu_clk_vmod(VPU_VENCP);
 *      video_clk = get_vpu_clk_vmod(VPU_VIU_OSD1);
 *
 */
unsigned int get_vpu_clk_vmod(unsigned int vmod)
{
	unsigned int vpu_clk;
	int ret = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return 0;
	if (vpu_conf.clk_vmod == NULL)
		return 0;

	mutex_lock(&vpu_clk_mutex);

	if (vmod < VPU_MOD_MAX) {
		vpu_clk = vpu_conf.clk_vmod[vmod];
		vpu_clk = vpu_clk_table[vpu_clk].freq;
	} else {
		vpu_clk = 0;
		VPUERR("invalid vmod\n");
	}

	mutex_unlock(&vpu_clk_mutex);
	return vpu_clk;
}

/*
 *  Function: request_vpu_clk_vmod
 *      Request a new vpu clk holding frequency with specified vmod
 *      Will change vpu clk if the max level in all vmod vpu clk holdings
 *      is unequal to current vpu clk level
 *
 *  Parameters:
 *      vclk - unsigned int, vpu clk frequency unit in Hz
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *
 *  Returns:
 *      int, 0 for success, 1 for failed
 *
 *  Example:
 *      ret = request_vpu_clk_vmod(100000000, VPU_VENCP);
 *      ret = request_vpu_clk_vmod(300000000, VPU_VIU_OSD1);
 *
 */
int request_vpu_clk_vmod(unsigned int vclk, unsigned int vmod)
{
	int ret = 0;
#ifdef CONFIG_VPU_DYNAMIC_ADJ
	unsigned int clk_level;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;
	if (vmod >= VPU_MOD_MAX) {
		VPUERR("request_vpu_clk: invalid vmod: %d\n", vmod);
		return -1;
	}
	if (vpu_conf.clk_vmod == NULL) {
		VPUERR("request_vpu_clk: clk_vmod is null\n");
		return -1;
	}

	mutex_lock(&vpu_clk_mutex);

	if (vclk >= 100) /* regard as vpu_clk */
		clk_level = get_vpu_clk_level(vclk);
	else /* regard as clk_level */
		clk_level = vclk;

	if (clk_level >= vpu_conf.data->clk_level_max) {
		VPUERR("request_vpu_clk: set clk is out of supported\n");
		mutex_unlock(&vpu_clk_mutex);
		return -1;
	}

	vpu_conf.clk_vmod[vmod] = clk_level;
	if (vpu_debug_print_flag) {
		if (vmod < VPU_MOD_MAX) {
			VPUPR("request vpu clk: %s %uHz\n",
				vpu_mod_table[vmod],
				vpu_clk_table[clk_level].freq);
			dump_stack();
		}
	}

	clk_level = get_vpu_clk_level_max_vmod();
	if (clk_level != vpu_conf.clk_level)
		adjust_vpu_clk(clk_level);

	mutex_unlock(&vpu_clk_mutex);
#endif
	return ret;
}
EXPORT_SYMBOL(request_vpu_clk_vmod);

/*
 *  Function: release_vpu_clk_vmod
 *      Release vpu clk holding frequency to 0 with specified vmod
 *      Will change vpu clk if the max level in all vmod vpu clk holdings
 *      is unequal to current vpu clk level
 *
 *  Parameters:
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *
 *  Returns:
 *      int, 0 for success, 1 for failed
 *
 *  Example:
 *      ret = release_vpu_clk_vmod(VPU_VENCP);
 *      ret = release_vpu_clk_vmod(VPU_VIU_OSD1);
 *
 */
int release_vpu_clk_vmod(unsigned int vmod)
{
	int ret = 0;
#ifdef CONFIG_VPU_DYNAMIC_ADJ
	unsigned int clk_level;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;
	if (vmod >= VPU_MOD_MAX) {
		VPUERR("release_vpu_clk: invalid vmod: %d\n", vmod);
		return -1;
	}
	if (vpu_conf.clk_vmod == NULL) {
		VPUERR("release_vpu_clk: clk_vmod is null\n");
		return -1;
	}

	mutex_lock(&vpu_clk_mutex);

	clk_level = 0;
	vpu_conf.clk_vmod[vmod] = clk_level;
	if (vpu_debug_print_flag) {
		if (vmod < VPU_MOD_MAX) {
			VPUPR("release vpu clk: %s\n", vpu_mod_table[vmod]);
			dump_stack();
		}
	}

	clk_level = get_vpu_clk_level_max_vmod();
	if (clk_level != vpu_conf.clk_level)
		adjust_vpu_clk(clk_level);

	mutex_unlock(&vpu_clk_mutex);
#endif
	return ret;
}

/* *********************************************** */

/* *********************************************** */
/* VPU_MEM_PD control */
/* *********************************************** */
/*
 *  Function: switch_vpu_mem_pd_vmod
 *      switch vpu memory power down by specified vmod
 *
 *  Parameters:
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *      flag - int, on/off switch flag, must be one of the following constants:
 *                 VPU_MEM_POWER_ON
 *                 VPU_MEM_POWER_DOWN
 *
 *  Example:
 *      switch_vpu_mem_pd_vmod(VPU_VENCP, VPU_MEM_POWER_ON);
 *      switch_vpu_mem_pd_vmod(VPU_VIU_OSD1, VPU_MEM_POWER_DOWN);
 *
 */
void switch_vpu_mem_pd_vmod(unsigned int vmod, int flag)
{
	unsigned long flags = 0;
	unsigned int _val, _reg, _bit, _len;
	struct vpu_ctrl_s *table;
	int i = 0, ret = 0, done = 0, cnt;

	ret = vpu_chip_valid_check();
	if (ret)
		return;
	if (vmod >= VPU_MOD_MAX) {
		VPUERR("switch_vpu_mem_pd: invalid vpu mod: %d\n", vmod);
		return;
	}

	spin_lock_irqsave(&vpu_mem_lock, flags);

	cnt = vpu_conf.data->mem_pd_table_cnt;
	table = vpu_conf.data->mem_pd_table;
	while (i < cnt) {
		if (table[i].vmod == VPU_MOD_MAX)
			break;
		if (table[i].vmod == vmod) {
			_reg = table[i].reg;
			_bit = table[i].bit;
			_len = table[i].len;
			if (flag == VPU_MEM_POWER_ON) {
				_val = 0x0;
			} else {
				if (_len == 32)
					_val = 0xffffffff;
				else
					_val = (1 << _len) - 1;
			}
			vpu_hiu_setb(_reg, _val, _bit, _len);
			done++;
		}
		i++;
	}

	spin_unlock_irqrestore(&vpu_mem_lock, flags);

	if (done == 0)
		VPUPR("switch_vpu_mem_pd: unsupport vpu mod: %d\n", vmod);
	if (vpu_debug_print_flag) {
		if (vmod < VPU_MOD_MAX) {
			VPUPR("switch_vpu_mem_pd: %s %s\n",
				vpu_mod_table[vmod],
				((flag == VPU_MEM_POWER_ON) ? "ON" : "OFF"));
			dump_stack();
		}
	}
}

/*
 *  Function: get_vpu_mem_pd_vmod
 *      switch vpu memory power down by specified vmod
 *
 *  Parameters:
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *
 *  Returns:
 *      int, 0 for power on, 1 for power down, -1 for error
 *
 *  Example:
 *      ret = get_vpu_mem_pd_vmod(VPU_VENCP);
 *      ret = get_vpu_mem_pd_vmod(VPU_VIU_OSD1);
 *
 */
int get_vpu_mem_pd_vmod(unsigned int vmod)
{
	unsigned int _reg, _bit, _len;
	struct vpu_ctrl_s *table;
	int i = 0, ret = 0, done = 0, cnt;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;
	if (vmod >= VPU_MOD_MAX) {
		VPUERR("get_vpu_mem_pd: invalid vpu mod: %d\n", vmod);
		return -1;
	}

	cnt = vpu_conf.data->mem_pd_table_cnt;
	table = vpu_conf.data->mem_pd_table;
	ret = 0;
	while (i < cnt) {
		if (table[i].vmod == VPU_MOD_MAX)
			break;
		if (table[i].vmod == vmod) {
			_reg = table[i].reg;
			_bit = table[i].bit;
			_len = table[i].len;
			ret += vpu_hiu_getb(_reg, _bit, _len);
			done++;
		}
		i++;
	}
	if (done == 0) {
		VPUPR("get_vpu_mem_pd: unsupport vpu mod: %d\n", vmod);
		return -1;
	}

	if (ret == 0)
		return VPU_MEM_POWER_ON;
	else
		return VPU_MEM_POWER_DOWN;
}

/* *********************************************** */
/* VPU_CLK_GATE control */
/* *********************************************** */
/*
 *  Function: switch_vpu_clk_gate_vmod
 *      switch vpu clk gate by specified vmod
 *
 *  Parameters:
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *      flag - int, on/off switch flag, must be one of the following constants:
 *                 VPU_CLK_GATE_ON
 *                 VPU_CLK_GATE_OFF
 *
 *  Example:
 *      switch_vpu_clk_gate_vmod(VPU_VENCP, VPU_CLK_GATE_ON);
 *      switch_vpu_clk_gate_vmod(VPU_VPP, VPU_CLK_GATE_OFF);
 *
 */
void switch_vpu_clk_gate_vmod(unsigned int vmod, int flag)
{
	unsigned int _val, _reg, _bit, _len;
	struct vpu_ctrl_s *table;
	int i = 0, ret = 0, done = 0, cnt;
	unsigned long flags = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return;
	if (vmod >= VPU_MAX) {
		VPUERR("switch_vpu_clk_gate: invalid vpu mod: %d\n", vmod);
		return;
	}

	spin_lock_irqsave(&vpu_clk_gate_lock, flags);

	cnt = vpu_conf.data->clk_gate_table_cnt;
	table = vpu_conf.data->clk_gate_table;
	while (i < cnt) {
		if (table[i].vmod == VPU_MAX)
			break;
		if (table[i].vmod == vmod) {
			_reg = table[i].reg;
			_bit = table[i].bit;
			_len = table[i].len;
			if (flag == VPU_CLK_GATE_ON) {
				if (_len == 32)
					_val = 0xffffffff;
				else
					_val = (1 << _len) - 1;
			} else {
				_val = 0;
			}
			vpu_vcbus_setb(_reg, _val, _bit, _len);
			done++;
		}
		i++;
	}

	spin_unlock_irqrestore(&vpu_clk_gate_lock, flags);

	if (done == 0)
		VPUPR("switch_vpu_clk_gate: unsupport vpu mod: %d\n", vmod);
	if (vpu_debug_print_flag) {
		if (vmod < VPU_MAX) {
			VPUPR("switch_vpu_clk_gate: %s %s\n",
				vpu_mod_table[vmod],
				((flag == VPU_CLK_GATE_ON) ? "ON" : "OFF"));
			dump_stack();
		}
	}
}

/* *********************************************** */

/* *********************************************** */
/* VPU sysfs function */
/* *********************************************** */
static const char *vpu_usage_str = {
"Usage:\n"
"	echo w <reg> <data> > reg ; write data to vcbus reg\n"
"	echo r <reg> > reg ; read vcbus reg\n"
"	echo d <reg> <num> > reg ; dump vcbus regs\n"
"\n"
"	echo r > mem ; read vpu memory power down status\n"
"	echo w <vmod> <flag> > mem ; write vpu memory power down\n"
"	<flag>: 0=power up, 1=power down\n"
"\n"
"	echo r > gate ; read vpu clk gate status\n"
"	echo w <vmod> <flag> > gate ; write vpu clk gate\n"
"	<flag>: 0=gate off, 1=gate on\n"
"\n"
"	echo 1 > test ; run vcbus access test\n"
"\n"
"	echo get > clk ; print current vpu clk\n"
"	echo set <vclk> > clk ; force to set vpu clk\n"
"	echo dump [vmod] > clk ; dump vpu clk by vmod, [vmod] is unnecessary\n"
"	echo request <vclk> <vmod> > clk ; request vpu clk holding by vmod\n"
"	echo release <vmod> > clk ; release vpu clk holding by vmod\n"
"\n"
"	request & release will change vpu clk if the max level in all vmod vpu clk holdings is unequal to current vpu clk level.\n"
"	vclk both support level(1~10) and frequency value (unit in Hz).\n"
"	vclk level & frequency:\n"
"		0: 100M    1: 167M    2: 200M    3: 250M\n"
"		4: 333M    5: 400M    6: 500M    7: 667M\n"
"\n"
"	echo <0|1> > print ; set debug print flag\n"
};

static ssize_t vpu_debug_help(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", vpu_usage_str);
}

static ssize_t vpu_clk_debug(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int ret = 0;
	unsigned int tmp[2], n;

	switch (buf[0]) {
	case 'g': /* get */
		VPUPR("get current clk: %uHz(0x%08x)\n",
			get_vpu_clk(), (vpu_hiu_read(HHI_VPU_CLK_CNTL)));
		break;
	case 's': /* set */
		tmp[0] = 4;
		ret = sscanf(buf, "set %u", &tmp[0]);
		if (ret == 1) {
			if (tmp[0] > 100)
				VPUPR("set clk frequency: %uHz\n", tmp[0]);
			else
				VPUPR("set clk level: %u\n", tmp[0]);
			set_vpu_clk(tmp[0]);
		} else {
			VPUERR("invalid parameters\n");
		}
		break;
	case 'r':
		if (buf[2] == 'q') { /* request */
			tmp[0] = 0;
			tmp[1] = VPU_MOD_MAX;
			ret = sscanf(buf, "request %u %u", &tmp[0], &tmp[1]);
			if (ret == 2)
				request_vpu_clk_vmod(tmp[0], tmp[1]);
			else
				VPUERR("invalid parameters\n");
		} else if (buf[2] == 'l') { /* release */
			tmp[0] = VPU_MOD_MAX;
			ret = sscanf(buf, "release %u", &tmp[0]);
			if (ret == 1)
				release_vpu_clk_vmod(tmp[0]);
			else
				VPUERR("invalid parameters\n");
		}
		break;
	case 'd':
		if (vpu_conf.clk_vmod == NULL) {
			VPUERR("clk_vmod is null\n");
			return count;
		}
		tmp[0] = VPU_MOD_MAX;
		ret = sscanf(buf, "dump %u", &tmp[0]);
		if (ret == 1) {
			tmp[0] = (tmp[0] >= VPU_MOD_MAX) ? VPU_MOD_MAX : tmp[0];
			VPUPR("clk holdings:\n");
			pr_info("%s:  %uHz(%u)\n", vpu_mod_table[tmp[0]],
				vpu_clk_table[vpu_conf.clk_vmod[tmp[0]]].freq,
				vpu_conf.clk_vmod[tmp[0]]);
		} else {
			n = get_vpu_clk_level_max_vmod();
			VPUPR("clk max holdings: %uHz(%u)\n",
				vpu_clk_table[n].freq, n);
		}
		break;
	default:
		VPUERR("wrong debug command\n");
		break;
	}

	return count;
}

static ssize_t vpu_mem_debug(struct class *class, struct class_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int tmp[2], val;
	unsigned int _reg0, _reg1, _reg2, _reg3, _reg4;
	int ret = 0, i;

	_reg0 = HHI_VPU_MEM_PD_REG0;
	_reg1 = HHI_VPU_MEM_PD_REG1;
	_reg2 = HHI_VPU_MEM_PD_REG2;
	_reg3 = HHI_VPU_MEM_PD_REG3;
	_reg4 = HHI_VPU_MEM_PD_REG4;
	switch (buf[0]) {
	case 'r':
		VPUPR("mem_pd0: 0x%08x\n", vpu_hiu_read(_reg0));
		if (vpu_conf.data->mem_pd_reg1_valid)
			VPUPR("mem_pd1: 0x%08x\n", vpu_hiu_read(_reg1));
		if (vpu_conf.data->mem_pd_reg2_valid)
			VPUPR("mem_pd2: 0x%08x\n", vpu_hiu_read(_reg2));
		if (vpu_conf.data->mem_pd_reg3_valid)
			VPUPR("mem_pd2: 0x%08x\n", vpu_hiu_read(_reg3));
		if (vpu_conf.data->mem_pd_reg4_valid)
			VPUPR("mem_pd2: 0x%08x\n", vpu_hiu_read(_reg4));
		break;
	case 'w':
		ret = sscanf(buf, "w %u %u", &tmp[0], &tmp[1]);
		if (ret == 2) {
			tmp[0] = (tmp[0] > VPU_MOD_MAX) ? VPU_MOD_MAX : tmp[0];
			tmp[1] = (tmp[1] == VPU_MEM_POWER_ON) ? 0 : 1;
			VPUPR("switch_vpu_mem_pd: %s %s\n",
				vpu_mod_table[tmp[0]],
				(tmp[1] ? "DOWN" : "ON"));
			switch_vpu_mem_pd_vmod(tmp[0], tmp[1]);
		} else {
			VPUERR("invalid parameters\n");
		}
		break;
	case 'i':
		VPUPR("vpu modules:\n");
		for (i = VPU_VIU_OSD1; i < VPU_MOD_MAX; i++) {
			val = get_vpu_mem_pd_vmod(i);
			pr_info("  [%02d] %s    %s(%d)\n",
				i, vpu_mod_table[i],
				(val == VPU_MEM_POWER_ON) ? "ON" : "OFF",
				val);
		}
		break;
	default:
		VPUERR("wrong mem_pd command\n");
		break;
	}

	return count;
}

static ssize_t vpu_clk_gate_debug(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int tmp[2];
	unsigned int _reg = 0;
	struct vpu_ctrl_s *table;
	int i = 0, ret = 0;

	switch (buf[0]) {
	case 'r':
		table = vpu_conf.data->clk_gate_table;
		while (1) {
			if (table[i].vmod == VPU_MAX)
				break;
			if (table[i].reg == _reg) {
				i++;
				continue;
			}
			_reg = table[i].reg;
			pr_info("VPU_CLK_GATE: 0x%04x = 0x%08x\n",
				_reg, vpu_vcbus_read(_reg));
			i++;
		}
		break;
	case 'w':
		ret = sscanf(buf, "w %u %u", &tmp[0], &tmp[1]);
		if (ret == 2) {
			tmp[0] = (tmp[0] > VPU_MAX) ? VPU_MAX : tmp[0];
			tmp[1] = (tmp[1] == VPU_CLK_GATE_ON) ? 1 : 0;
			VPUPR("switch_vpu_clk_gate: %s %s\n",
				vpu_mod_table[tmp[0]], (tmp[1] ? "ON" : "OFF"));
			switch_vpu_clk_gate_vmod(tmp[0], tmp[1]);
		} else {
			VPUERR("invalid parameters\n");
		}
		break;
	default:
		VPUERR("wrong clk_gate command\n");
		break;
	}

	return count;
}

static unsigned int vcbus_reg[] = {
	0x1b7f, /* VENC_VDAC_TST_VAL */
	0x1c30, /* ENCP_DVI_HSO_BEGIN */
	0x1d00, /* VPP_DUMMY_DATA */
	0x2730, /* VPU_VPU_PWM_V0 */
};

static void vcbus_test(void)
{
	unsigned int val;
	unsigned int temp;
	int i, j;

	VPUPR("vcbus test:\n");
	for (i = 0; i < ARRAY_SIZE(vcbus_reg); i++) {
		for (j = 0; j < 24; j++) {
			val = vpu_vcbus_read(vcbus_reg[i]);
			pr_info("%02d read 0x%04x=0x%08x\n",
				j, vcbus_reg[i], val);
		}
		pr_info("\n");
	}
	temp = 0x5a5a5a5a;
	for (i = 0; i < ARRAY_SIZE(vcbus_reg); i++) {
		vpu_vcbus_write(vcbus_reg[i], temp);
		val = vpu_vcbus_read(vcbus_reg[i]);
		pr_info("write 0x%04x=0x%08x, readback: 0x%08x\n",
			vcbus_reg[i], temp, val);
	}
	for (i = 0; i < ARRAY_SIZE(vcbus_reg); i++) {
		for (j = 0; j < 24; j++) {
			val = vpu_vcbus_read(vcbus_reg[i]);
			pr_info("%02d read 0x%04x=0x%08x\n",
				j, vcbus_reg[i], val);
		}
		pr_info("\n");
	}
}

static ssize_t vpu_test_debug(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
{
	vcbus_test();

	return count;
}

static ssize_t vpu_print_debug(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;

	ret = kstrtoint(buf, 10, &vpu_debug_print_flag);
	pr_info("set vpu debug_print_flag: %d\n", vpu_debug_print_flag);

	return count;
}

static ssize_t vpu_debug_info(struct class *class,
		struct class_attribute *attr, char *buf)
{
	unsigned int level_max, clk;
	ssize_t len = 0;
	int ret;
#ifdef CONFIG_VPU_DYNAMIC_ADJ
	int i;
#endif

	ret = vpu_chip_valid_check();
	if (ret) {
		len = sprintf(buf, "vpu invalid\n");
		return len;
	}

	clk = get_vpu_clk();
	level_max = vpu_conf.data->clk_level_max - 1;

	len = sprintf(buf, "driver version: %s(%d-%s)\n",
		VPU_VERION,
		vpu_conf.data->chip_type,
		vpu_conf.data->chip_name);
	len += sprintf(buf+len, "actual clk:     %dHz\n"
		"clk_level:      %d(%dHz)\n"
		"clk_level dft:  %d(%dHz)\n"
		"clk_level max:  %d(%dHz)\n\n",
		clk,
		vpu_conf.clk_level, vpu_clk_table[vpu_conf.clk_level].freq,
		vpu_conf.data->clk_level_dft,
		vpu_clk_table[vpu_conf.data->clk_level_dft].freq,
		level_max, vpu_clk_table[level_max].freq);

	len += sprintf(buf+len, "mem_pd:\n"
		"  mem_pd0:      0x%08x\n",
		vpu_hiu_read(HHI_VPU_MEM_PD_REG0));
	if (vpu_conf.data->mem_pd_reg1_valid) {
		len += sprintf(buf+len, "  mem_pd1:      0x%08x\n",
			vpu_hiu_read(HHI_VPU_MEM_PD_REG1));
	}
	if (vpu_conf.data->mem_pd_reg2_valid) {
		len += sprintf(buf+len, "  mem_pd2:      0x%08x\n",
			vpu_hiu_read(HHI_VPU_MEM_PD_REG2));
	}
	if (vpu_conf.data->mem_pd_reg3_valid) {
		len += sprintf(buf+len, "  mem_pd3:      0x%08x\n",
			vpu_hiu_read(HHI_VPU_MEM_PD_REG3));
	}
	if (vpu_conf.data->mem_pd_reg4_valid) {
		len += sprintf(buf+len, "  mem_pd4:      0x%08x\n",
			vpu_hiu_read(HHI_VPU_MEM_PD_REG4));
	}

#ifdef CONFIG_VPU_DYNAMIC_ADJ
	if (vpu_conf.clk_vmod) {
		len += sprintf(buf+len, "\nclk_vmod:\n");
		for (i = 0; i < VPU_MOD_MAX; i++) {
			len += sprintf(buf+len, "  %02d:    %dHz\n",
				i, vpu_conf.clk_vmod[i]);
		}
	}
#endif
	len += sprintf(buf+len, "\n");

	return len;
}

static ssize_t vpu_debug_reg_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned int reg32 = 0, data32 = 0;
	int i;

	switch (buf[0]) {
	case 'w':
		ret = sscanf(buf, "w %x %x", &reg32, &data32);
		if (ret == 2) {
			vpu_vcbus_write(reg32, data32);
			pr_info("write vcbus[0x%04x]=0x%08x, readback 0x%08x\n",
				reg32, data32, vpu_vcbus_read(reg32));
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'r':
		ret = sscanf(buf, "r %x", &reg32);
		if (ret == 1) {
			pr_info("read vcbus[0x%04x] = 0x%08x\n",
				reg32, vpu_vcbus_read(reg32));
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'd':
		ret = sscanf(buf, "d %x %d", &reg32, &data32);
		if (ret == 2) {
			pr_info("dump vcbus regs:\n");
			for (i = 0; i < data32; i++) {
				pr_info("[0x%04x] = 0x%08x\n",
					(reg32 + i), vpu_vcbus_read(reg32 + i));
			}
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		pr_info("wrong command\n");
		break;
	}

	return count;
}

static struct class_attribute vpu_debug_class_attrs[] = {
	__ATTR(clk,   0644, vpu_debug_help, vpu_clk_debug),
	__ATTR(mem,   0644, vpu_debug_help, vpu_mem_debug),
	__ATTR(gate,  0644, vpu_debug_help, vpu_clk_gate_debug),
	__ATTR(test,  0644, vpu_debug_help, vpu_test_debug),
	__ATTR(print, 0644, vpu_debug_help, vpu_print_debug),
	__ATTR(reg,   0644, vpu_debug_help, vpu_debug_reg_store),
	__ATTR(info,  0444, vpu_debug_info, NULL),
	__ATTR(help,  0444, vpu_debug_help, NULL),
};

static struct class *vpu_debug_class;
static int creat_vpu_debug_class(void)
{
	int i;

	vpu_debug_class = class_create(THIS_MODULE, "vpu");
	if (IS_ERR_OR_NULL(vpu_debug_class)) {
		VPUERR("create vpu_debug_class failed\n");
		return -1;
	}
	for (i = 0; i < ARRAY_SIZE(vpu_debug_class_attrs); i++) {
		if (class_create_file(vpu_debug_class,
			&vpu_debug_class_attrs[i])) {
			VPUERR("create vpu debug attribute %s failed\n",
				vpu_debug_class_attrs[i].attr.name);
		}
	}
	return 0;
}

static int remove_vpu_debug_class(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vpu_debug_class_attrs); i++)
		class_remove_file(vpu_debug_class, &vpu_debug_class_attrs[i]);

	class_destroy(vpu_debug_class);
	vpu_debug_class = NULL;
	return 0;
}
/* ********************************************************* */

#ifdef CONFIG_PM
static int vpu_suspend(struct platform_device *pdev, pm_message_t state)
{
	VPUPR("suspend clk: %uHz(0x%x)\n",
		get_vpu_clk(), (vpu_hiu_read(HHI_VPU_CLK_CNTL)));
	return 0;
}

static int vpu_resume(struct platform_device *pdev)
{
	set_vpu_clk(vpu_conf.clk_level);
	VPUPR("resume clk: %uHz(0x%x)\n",
		get_vpu_clk(), (vpu_hiu_read(HHI_VPU_CLK_CNTL)));
	return 0;
}
#endif

static int get_vpu_config(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int val;
	struct device_node *vpu_np;

	vpu_np = pdev->dev.of_node;
	if (!vpu_np) {
		VPUERR("don't find vpu node\n");
		return -1;
	}

	ret = of_property_read_u32(vpu_np, "clk_level", &val);
	if (ret) {
		VPUPR("don't find clk_level, use default setting\n");
	} else {
		if (val >= vpu_conf.data->clk_level_max) {
			VPUERR("clk_level is out of support, set to default\n");
			val = vpu_conf.data->clk_level_dft;
		}

		vpu_conf.clk_level = val;
	}
	VPUPR("load vpu_clk: %uHz(%u)\n",
		vpu_clk_table[val].freq, vpu_conf.clk_level);

	return ret;
}

static void vpu_clktree_init(struct device *dev)
{
	struct clk *clk_vapb, *clk_vpu_intr;

	/* init & enable vapb_clk */
	clk_vapb = devm_clk_get(dev, "vapb_clk");
	if (IS_ERR_OR_NULL(clk_vapb))
		VPUERR("%s: vapb_clk\n", __func__);
	else
		clk_prepare_enable(clk_vapb);

	clk_vpu_intr = devm_clk_get(dev, "vpu_intr_gate");
	if (IS_ERR_OR_NULL(clk_vpu_intr))
		VPUERR("%s: vpu_intr_gate\n", __func__);
	else
		clk_prepare_enable(clk_vpu_intr);

	if (vpu_conf.data->gp_pll_valid) {
		vpu_conf.gp_pll = devm_clk_get(dev, "gp_pll");
		if (IS_ERR_OR_NULL(vpu_conf.gp_pll))
			VPUERR("%s: gp_pll\n", __func__);
	}

	/* init & enable vpu_clk */
	vpu_conf.vpu_clk0 = devm_clk_get(dev, "vpu_clk0");
	vpu_conf.vpu_clk1 = devm_clk_get(dev, "vpu_clk1");
	vpu_conf.vpu_clk = devm_clk_get(dev, "vpu_clk");
	if ((IS_ERR_OR_NULL(vpu_conf.vpu_clk0)) ||
		(IS_ERR_OR_NULL(vpu_conf.vpu_clk1)) ||
		(IS_ERR_OR_NULL(vpu_conf.vpu_clk))) {
		VPUERR("%s: vpu_clk\n", __func__);
	} else {
		clk_set_parent(vpu_conf.vpu_clk, vpu_conf.vpu_clk0);
		clk_prepare_enable(vpu_conf.vpu_clk);
	}

	VPUPR("clktree_init\n");
}

static int vpu_power_init_check(void)
{
	unsigned int val;
	int ret = 0;

	val = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 31, 1);
	if (val)
		val = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 24, 1);
	else
		val = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 8, 1);
	ret = (val == 0) ? 1 : 0;

	return ret;
}

static void vpu_power_init(void)
{
	int ret = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return;

	vpu_power_on();
	vpu_mem_pd_init_off();
	vpu_module_init_config();
}

static struct vpu_data_s vpu_data_gxb = {
	.chip_type = VPU_CHIP_GXBB,
	.chip_name = "gxbb",
	.clk_level_dft = CLK_LEVEL_DFT_GXBB,
	.clk_level_max = CLK_LEVEL_MAX_GXBB,
	.fclk_div_table = fclk_div_table_gxb,

	.gp_pll_valid = 1,
	.mem_pd_reg1_valid = 1,
	.mem_pd_reg2_valid = 0,
	.mem_pd_reg3_valid = 0,
	.mem_pd_reg4_valid = 0,

	.mem_pd_table_cnt =
		sizeof(vpu_mem_pd_gxb) / sizeof(struct vpu_ctrl_s),
	.clk_gate_table_cnt =
		sizeof(vpu_clk_gate_gxb) / sizeof(struct vpu_ctrl_s),
	.mem_pd_table = vpu_mem_pd_gxb,
	.clk_gate_table = vpu_clk_gate_gxb,

	.module_init_table_cnt = 0,
	.module_init_table = NULL,
	.hdmi_iso_table = vpu_hdmi_iso_gxb,
	.reset_table = vpu_reset_gx,
};

static struct vpu_data_s vpu_data_gxtvbb = {
	.chip_type = VPU_CHIP_GXTVBB,
	.chip_name = "gxtvbb",
	.clk_level_dft = CLK_LEVEL_DFT_GXTVBB,
	.clk_level_max = CLK_LEVEL_MAX_GXTVBB,
	.fclk_div_table = fclk_div_table_gxb,

	.gp_pll_valid = 1,
	.mem_pd_reg1_valid = 1,
	.mem_pd_reg2_valid = 0,
	.mem_pd_reg3_valid = 0,
	.mem_pd_reg4_valid = 0,

	.mem_pd_table_cnt =
		sizeof(vpu_mem_pd_gxtvbb) / sizeof(struct vpu_ctrl_s),
	.clk_gate_table_cnt =
		sizeof(vpu_clk_gate_gxl) / sizeof(struct vpu_ctrl_s),
	.mem_pd_table = vpu_mem_pd_gxtvbb,
	.clk_gate_table = vpu_clk_gate_gxl,

	.module_init_table_cnt = 0,
	.module_init_table = NULL,
	.hdmi_iso_table = vpu_hdmi_iso_gxb,
	.reset_table = vpu_reset_gx,
};

static struct vpu_data_s vpu_data_gxl = {
	.chip_type = VPU_CHIP_GXL,
	.chip_name = "gxl",
	.clk_level_dft = CLK_LEVEL_DFT_GXL,
	.clk_level_max = CLK_LEVEL_MAX_GXL,
	.fclk_div_table = fclk_div_table_gxb,

	.gp_pll_valid = 1,
	.mem_pd_reg1_valid = 1,
	.mem_pd_reg2_valid = 1,
	.mem_pd_reg3_valid = 0,
	.mem_pd_reg4_valid = 0,

	.mem_pd_table_cnt =
		sizeof(vpu_mem_pd_gxl) / sizeof(struct vpu_ctrl_s),
	.clk_gate_table_cnt =
		sizeof(vpu_clk_gate_gxl) / sizeof(struct vpu_ctrl_s),
	.mem_pd_table = vpu_mem_pd_gxl,
	.clk_gate_table = vpu_clk_gate_gxl,

	.module_init_table_cnt = 0,
	.module_init_table = NULL,
	.hdmi_iso_table = vpu_hdmi_iso_gxb,
	.reset_table = vpu_reset_gx,
};

static struct vpu_data_s vpu_data_gxm = {
	.chip_type = VPU_CHIP_GXM,
	.chip_name = "gxm",
	.clk_level_dft = CLK_LEVEL_DFT_GXM,
	.clk_level_max = CLK_LEVEL_MAX_GXM,
	.fclk_div_table = fclk_div_table_gxb,

	.gp_pll_valid = 1,
	.mem_pd_reg1_valid = 1,
	.mem_pd_reg2_valid = 1,
	.mem_pd_reg3_valid = 0,
	.mem_pd_reg4_valid = 0,

	.mem_pd_table_cnt =
		sizeof(vpu_mem_pd_gxl) / sizeof(struct vpu_ctrl_s),
	.clk_gate_table_cnt =
		sizeof(vpu_clk_gate_gxl) / sizeof(struct vpu_ctrl_s),
	.mem_pd_table = vpu_mem_pd_gxl,
	.clk_gate_table = vpu_clk_gate_gxl,

	.module_init_table_cnt =
		sizeof(vpu_module_init_gxm) / sizeof(struct vpu_ctrl_s),
	.module_init_table = vpu_module_init_gxm,
	.hdmi_iso_table = vpu_hdmi_iso_gxb,
	.reset_table = vpu_reset_gx,
};

static struct vpu_data_s vpu_data_txl = {
	.chip_type = VPU_CHIP_TXL,
	.chip_name = "txl",
	.clk_level_dft = CLK_LEVEL_DFT_TXLX,
	.clk_level_max = CLK_LEVEL_MAX_TXLX,
	.fclk_div_table = fclk_div_table_gxb,

	.gp_pll_valid = 0,
	.mem_pd_reg1_valid = 1,
	.mem_pd_reg2_valid = 0,
	.mem_pd_reg3_valid = 0,
	.mem_pd_reg4_valid = 0,

	.mem_pd_table_cnt =
		sizeof(vpu_mem_pd_txl) / sizeof(struct vpu_ctrl_s),
	.clk_gate_table_cnt =
		sizeof(vpu_clk_gate_txl) / sizeof(struct vpu_ctrl_s),
	.mem_pd_table = vpu_mem_pd_txl,
	.clk_gate_table = vpu_clk_gate_txl,

	.module_init_table_cnt = 0,
	.module_init_table = NULL,
	.hdmi_iso_table = vpu_hdmi_iso_gxb,
	.reset_table = vpu_reset_gx,
};

static struct vpu_data_s vpu_data_txlx = {
	.chip_type = VPU_CHIP_TXLX,
	.chip_name = "txlx",
	.clk_level_dft = CLK_LEVEL_DFT_TXLX,
	.clk_level_max = CLK_LEVEL_MAX_TXLX,
	.fclk_div_table = fclk_div_table_gxb,

	.gp_pll_valid = 1,
	.mem_pd_reg1_valid = 1,
	.mem_pd_reg2_valid = 1,
	.mem_pd_reg3_valid = 0,
	.mem_pd_reg4_valid = 0,

	.mem_pd_table_cnt =
		sizeof(vpu_mem_pd_txlx) / sizeof(struct vpu_ctrl_s),
	.clk_gate_table_cnt =
		sizeof(vpu_clk_gate_txlx) / sizeof(struct vpu_ctrl_s),
	.mem_pd_table = vpu_mem_pd_txlx,
	.clk_gate_table = vpu_clk_gate_txlx,

	.module_init_table_cnt =
		sizeof(vpu_module_init_txlx) / sizeof(struct vpu_ctrl_s),
	.module_init_table = vpu_module_init_txlx,
	.hdmi_iso_table = vpu_hdmi_iso_gxb,
	.reset_table = vpu_reset_txlx,
};

static struct vpu_data_s vpu_data_axg = {
	.chip_type = VPU_CHIP_AXG,
	.chip_name = "axg",
	.clk_level_dft = CLK_LEVEL_DFT_AXG,
	.clk_level_max = CLK_LEVEL_MAX_AXG,
	.fclk_div_table = fclk_div_table_gxb,

	.gp_pll_valid = 0,
	.mem_pd_reg1_valid = 0,
	.mem_pd_reg2_valid = 0,
	.mem_pd_reg3_valid = 0,
	.mem_pd_reg4_valid = 0,

	.mem_pd_table_cnt =
		sizeof(vpu_mem_pd_axg) / sizeof(struct vpu_ctrl_s),
	.clk_gate_table_cnt =
		sizeof(vpu_clk_gate_axg) / sizeof(struct vpu_ctrl_s),
	.mem_pd_table = vpu_mem_pd_axg,
	.clk_gate_table = vpu_clk_gate_axg,

	.module_init_table_cnt = 0,
	.module_init_table = NULL,
	.hdmi_iso_table = vpu_hdmi_iso_gxb,
	.reset_table = vpu_reset_txlx,
};

static struct vpu_data_s vpu_data_g12a = {
	.chip_type = VPU_CHIP_G12A,
	.chip_name = "g12a",
	.clk_level_dft = CLK_LEVEL_DFT_G12A,
	.clk_level_max = CLK_LEVEL_MAX_G12A,
	.fclk_div_table = fclk_div_table_g12a,

	.gp_pll_valid = 0,
	.mem_pd_reg1_valid = 1,
	.mem_pd_reg2_valid = 1,
	.mem_pd_reg3_valid = 0,
	.mem_pd_reg4_valid = 0,

	.mem_pd_table_cnt =
		sizeof(vpu_mem_pd_g12a) / sizeof(struct vpu_ctrl_s),
	.clk_gate_table_cnt =
		sizeof(vpu_clk_gate_g12a) / sizeof(struct vpu_ctrl_s),
	.mem_pd_table = vpu_mem_pd_g12a,
	.clk_gate_table = vpu_clk_gate_g12a,

	.module_init_table_cnt = 0,
	.module_init_table = NULL,
	.hdmi_iso_table = vpu_hdmi_iso_gxb,
	.reset_table = vpu_reset_txlx,
};

static struct vpu_data_s vpu_data_g12b = {
	.chip_type = VPU_CHIP_G12B,
	.chip_name = "g12b",
	.clk_level_dft = CLK_LEVEL_DFT_G12A,
	.clk_level_max = CLK_LEVEL_MAX_G12A,
	.fclk_div_table = fclk_div_table_g12a,

	.gp_pll_valid = 0,
	.mem_pd_reg1_valid = 1,
	.mem_pd_reg2_valid = 1,
	.mem_pd_reg3_valid = 0,
	.mem_pd_reg4_valid = 0,

	.mem_pd_table_cnt =
		sizeof(vpu_mem_pd_g12b) / sizeof(struct vpu_ctrl_s),
	.clk_gate_table_cnt =
		sizeof(vpu_clk_gate_g12a) / sizeof(struct vpu_ctrl_s),
	.mem_pd_table = vpu_mem_pd_g12b,
	.clk_gate_table = vpu_clk_gate_g12a,

	.module_init_table_cnt = 0,
	.module_init_table = NULL,
	.hdmi_iso_table = vpu_hdmi_iso_gxb,
	.reset_table = vpu_reset_txlx,
};

static struct vpu_data_s vpu_data_tl1 = {
	.chip_type = VPU_CHIP_TL1,
	.chip_name = "tl1",
	.clk_level_dft = CLK_LEVEL_DFT_G12A,
	.clk_level_max = CLK_LEVEL_MAX_G12A,
	.fclk_div_table = fclk_div_table_g12a,

	.gp_pll_valid = 0,
	.mem_pd_reg1_valid = 1,
	.mem_pd_reg2_valid = 1,
	.mem_pd_reg3_valid = 1,
	.mem_pd_reg4_valid = 1,

	.mem_pd_table_cnt =
		sizeof(vpu_mem_pd_tl1) / sizeof(struct vpu_ctrl_s),
	.clk_gate_table_cnt =
		sizeof(vpu_clk_gate_g12a) / sizeof(struct vpu_ctrl_s),
	.mem_pd_table = vpu_mem_pd_tl1,
	.clk_gate_table = vpu_clk_gate_g12a,

	.module_init_table_cnt = 0,
	.module_init_table = NULL,
	.hdmi_iso_table = vpu_hdmi_iso_gxb,
	.reset_table = vpu_reset_tl1,
};

static struct vpu_data_s vpu_data_sm1 = {
	.chip_type = VPU_CHIP_SM1,
	.chip_name = "sm1",
	.clk_level_dft = CLK_LEVEL_DFT_G12A,
	.clk_level_max = CLK_LEVEL_MAX_G12A,
	.fclk_div_table = fclk_div_table_g12a,

	.gp_pll_valid = 0,
	.mem_pd_reg1_valid = 1,
	.mem_pd_reg2_valid = 1,
	.mem_pd_reg3_valid = 1,
	.mem_pd_reg4_valid = 1,

	.mem_pd_table_cnt =
		sizeof(vpu_mem_pd_sm1) / sizeof(struct vpu_ctrl_s),
	.clk_gate_table_cnt =
		sizeof(vpu_clk_gate_g12a) / sizeof(struct vpu_ctrl_s),
	.mem_pd_table = vpu_mem_pd_sm1,
	.clk_gate_table = vpu_clk_gate_g12a,

	.module_init_table_cnt = 0,
	.module_init_table = NULL,
	.hdmi_iso_table = vpu_hdmi_iso_sm1,
	.reset_table = vpu_reset_txlx,
};

static const struct of_device_id vpu_of_table[] = {
	{
		.compatible = "amlogic, vpu-gxbb",
		.data = &vpu_data_gxb,
	},
	{
		.compatible = "amlogic, vpu-gxtvbb",
		.data = &vpu_data_gxtvbb,
	},
	{
		.compatible = "amlogic, vpu-gxl",
		.data = &vpu_data_gxl,
	},
	{
		.compatible = "amlogic, vpu-gxm",
		.data = &vpu_data_gxm,
	},
	{
		.compatible = "amlogic, vpu-txl",
		.data = &vpu_data_txl,
	},
	{
		.compatible = "amlogic, vpu-txlx",
		.data = &vpu_data_txlx,
	},
	{
		.compatible = "amlogic, vpu-axg",
		.data = &vpu_data_axg,
	},
	{
		.compatible = "amlogic, vpu-g12a",
		.data = &vpu_data_g12a,
	},
	{
		.compatible = "amlogic, vpu-g12b",
		.data = &vpu_data_g12b,
	},
	{
		.compatible = "amlogic, vpu-tl1",
		.data = &vpu_data_tl1,
	},
	{
		.compatible = "amlogic, vpu-sm1",
		.data = &vpu_data_sm1,
	},
	{},
};

static int vpu_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	int ret = 0;

#ifdef VPU_DEBUG_PRINT
	vpu_debug_print_flag = 1;
#else
	vpu_debug_print_flag = 0;
#endif

	match = of_match_device(vpu_of_table, &pdev->dev);
	if (match == NULL) {
		VPUERR("%s: no match table\n", __func__);
		return -1;
	}
	vpu_conf.data = (struct vpu_data_s *)match->data;

	VPUPR("driver version: %s(%d-%s)\n",
		VPU_VERION,
		vpu_conf.data->chip_type,
		vpu_conf.data->chip_name);

	vpu_conf.clk_vmod = kzalloc((sizeof(unsigned int) * VPU_MOD_MAX),
		GFP_KERNEL);
	if (vpu_conf.clk_vmod == NULL)
		VPUERR("%s: Not enough memory\n", __func__);

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;

	spin_lock_init(&vpu_mem_lock);
	spin_lock_init(&vpu_clk_gate_lock);

	get_vpu_config(pdev);

	ret = vpu_power_init_check();
	vpu_clktree_init(&pdev->dev);
	set_vpu_clk(vpu_conf.clk_level);
	if (ret)
		vpu_power_init();
	creat_vpu_debug_class();

	VPUPR("%s OK\n", __func__);
	return 0;
}

static int vpu_remove(struct platform_device *pdev)
{
	remove_vpu_debug_class();
	return 0;
}

static struct platform_driver vpu_driver = {
	.driver = {
		.name = "vpu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(vpu_of_table),
	},
	.probe = vpu_probe,
	.remove = vpu_remove,
#ifdef CONFIG_PM
	.suspend    = vpu_suspend,
	.resume     = vpu_resume,
#endif
};

static int __init vpu_init(void)
{
	return platform_driver_register(&vpu_driver);
}

static void __exit vpu_exit(void)
{
	platform_driver_unregister(&vpu_driver);
}

postcore_initcall(vpu_init);
module_exit(vpu_exit);

MODULE_DESCRIPTION("meson vpu driver");
MODULE_LICENSE("GPL v2");
