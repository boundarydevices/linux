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
#include "vpu_clk.h"
#include "vpu_module.h"

/* v01: initial version */
#define VPU_VERION        "v01"

enum vpu_chip_e vpu_chip_type;
int vpu_debug_print_flag;
static spinlock_t vpu_lock;
static DEFINE_MUTEX(vpu_mutex);

static unsigned int clk_vmod[VPU_MAX];

static struct vpu_conf_s vpu_conf = {
	.mem_pd0 = 0,
	.mem_pd1 = 0,
	.clk_level_dft = 0,
	.clk_level_max = 1,
	.clk_level = 0,
	/*.fclk_type = 0,*/
};

int vpu_chip_valid_check(void)
{
	int ret = 0;

	if (vpu_chip_type == VPU_CHIP_MAX) {
		VPUERR("invalid VPU in current chip\n");
		ret = -1;
	}
	return ret;
}

static void vpu_chip_detect(void)
{
#if 0
	unsigned int cpu_type;

	cpu_type = get_cpu_type();
	switch (cpu_type) {
	case MESON_CPU_MAJOR_ID_GXBB:
		vpu_chip_type = VPU_CHIP_GXBB;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_GXBB;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_GXBB;
		break;
	case MESON_CPU_MAJOR_ID_GXTVBB:
		vpu_chip_type = VPU_CHIP_GXTVBB;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_GXTVBB;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_GXTVBB;
		break;
	case MESON_CPU_MAJOR_ID_GXL:
	case MESON_CPU_MAJOR_ID_GXM:
		vpu_chip_type = VPU_CHIP_GXL;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_GXL;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_GXL;
		break;
	case MESON_CPU_MAJOR_ID_TXL:
		vpu_chip_type = VPU_CHIP_TXL;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_TXL;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_TXL;
		break;
	default:
		vpu_chip_type = VPU_CHIP_MAX;
		vpu_conf.clk_level_dft = 0;
		vpu_conf.clk_level_max = 1;
	}
#else
	vpu_chip_type = VPU_CHIP_GXL;
	vpu_conf.clk_level_dft = CLK_LEVEL_DFT_GXL;
	vpu_conf.clk_level_max = CLK_LEVEL_MAX_GXL;
#endif

	if (vpu_debug_print_flag)
		VPUPR("detect chip type: %d\n", vpu_chip_type);
}

static unsigned int get_vpu_clk_level_max_vmod(void)
{
	unsigned int max_level;
	int i;

	max_level = 0;
	for (i = 0; i < VPU_MAX; i++) {
		if (clk_vmod[i] > max_level)
			max_level = clk_vmod[i];
	}

	return max_level;
}

static unsigned int get_vpu_clk_level(unsigned int video_clk)
{
	unsigned int video_bw;
	unsigned int clk_level;
	int i;

	video_bw = video_clk + 1000000;

	for (i = 0; i < vpu_conf.clk_level_max; i++) {
		if (video_bw <= vpu_clk_table[i][0])
			break;
	}
	clk_level = i;

	return clk_level;
}

unsigned int get_vpu_clk(void)
{
	unsigned int clk_freq;
	unsigned int fclk, clk_source;
	unsigned int mux, div;

	fclk = CLK_FPLL_FREQ * 100; /* 0.01M resolution */
	mux = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 9, 3);
	switch (mux) {
	case FCLK_DIV4:
	case FCLK_DIV3:
	case FCLK_DIV5:
	case FCLK_DIV7:
		clk_source = fclk / fclk_div_table[mux];
		break;
	case GPLL_CLK:
		if (vpu_chip_type == VPU_CHIP_GXTVBB)
			clk_source = 696 * 100; /* 0.01MHz */
		else
			clk_source = 0;
		break;
	default:
		clk_source = 0;
		break;
	}

	div = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 0, 7) + 1;
	clk_freq = (clk_source / div) * 10 * 1000; /* change to Hz */

	return clk_freq;
}

static int switch_gp1_pll_gxtvbb(int flag)
{
	int cnt = 100;
	int ret = 0;

	if (flag) { /* enable gp1_pll */
		/* GP1 DPLL 696MHz output*/
		vpu_hiu_write(HHI_GP1_PLL_CNTL2, 0x69c80000);
		vpu_hiu_write(HHI_GP1_PLL_CNTL3, 0x0a5590c4);
		vpu_hiu_write(HHI_GP1_PLL_CNTL4, 0x0000500d);
		vpu_hiu_write(HHI_GP1_PLL_CNTL, 0x6a01023a);
		vpu_hiu_write(HHI_GP1_PLL_CNTL, 0x4a01023a);
		do {
			udelay(10);
			vpu_hiu_setb(HHI_GP1_PLL_CNTL, 1, 29, 1); /* reset */
			udelay(50);
			/* release reset */
			vpu_hiu_setb(HHI_GP1_PLL_CNTL, 0, 29, 1);
			udelay(50);
			cnt--;
			if (cnt == 0)
				break;
		} while (vpu_hiu_getb(HHI_GP1_PLL_CNTL, 31, 1) == 0);
		if (vpu_hiu_getb(HHI_GP1_PLL_CNTL, 31, 1) == 0) {
			ret = -1;
			vpu_hiu_setb(HHI_GP1_PLL_CNTL, 0, 30, 1);
			VPUERR("GP1_PLL lock failed\n");
		}
	} else { /* disable gp1_pll */
		vpu_hiu_setb(HHI_GP1_PLL_CNTL, 0, 30, 1);
	}

	return ret;
}

static int switch_gp_pll(int flag)
{
	int ret = 0;

	switch (vpu_chip_type) {
	case VPU_CHIP_GXTVBB:
		ret = switch_gp1_pll_gxtvbb(flag);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

#define VPU_CLKB_MAX    350  /* unit: MHz */

static int adjust_vpu_clk(unsigned int clk_level)
{
	unsigned long flags = 0;
	unsigned int mux, div;
	int ret = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;

	spin_lock_irqsave(&vpu_lock, flags);

	mux = vpu_clk_table[vpu_conf.clk_level][1];
	if (mux == GPLL_CLK) {
		ret = switch_gp_pll(1);
		if (ret) {
			VPUERR("%s exit\n", __func__);
			spin_unlock_irqrestore(&vpu_lock, flags);
			return ret;
		}
	}

	vpu_conf.clk_level = clk_level;

	/* step 1:  switch to 2nd vpu clk patch */
	mux = vpu_clk_table[0][1];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, mux, 25, 3);
	div = vpu_clk_table[0][2];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, div, 16, 7);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 1, 24, 1);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 1, 31, 1);
	udelay(10);
	/* step 2:  adjust 1st vpu clk frequency */
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 0, 8, 1);
	mux = vpu_clk_table[vpu_conf.clk_level][1];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, mux, 9, 3);
	div = vpu_clk_table[vpu_conf.clk_level][2];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, div, 0, 7);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 1, 8, 1);
	udelay(20);
	/* step 3:  switch back to 1st vpu clk patch */
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 0, 31, 1);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 0, 24, 1);

	VPUPR("set vpu clk: %uHz, readback: %uHz(0x%x)\n",
		vpu_clk_table[vpu_conf.clk_level][0],
		get_vpu_clk(), (vpu_hiu_read(HHI_VPU_CLK_CNTL)));

	spin_unlock_irqrestore(&vpu_lock, flags);
	return ret;
}

static int set_vpu_clk(unsigned int vclk)
{
	int ret = 0;
	unsigned int clk_level, mux, div;

	mutex_lock(&vpu_mutex);

	if (vclk >= 100) { /* regard as vpu_clk */
		clk_level = get_vpu_clk_level(vclk);
	} else { /* regard as clk_level */
		clk_level = vclk;
	}

	if (clk_level >= vpu_conf.clk_level_max) {
		ret = 1;
		VPUERR("set vpu clk out of supported range\n");
		goto set_vpu_clk_limit;
	} else if (clk_level < vpu_conf.clk_level_dft) {
#ifdef LIMIT_VPU_CLK_LOW
		ret = 3;
		VPUERR("set vpu clk less than system default\n");
		goto set_vpu_clk_limit;
#endif
	}

	mux = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 9, 3);
	div = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 0, 7);
	if ((mux != vpu_clk_table[clk_level][1])
		|| (div != vpu_clk_table[clk_level][2])) {
		ret = adjust_vpu_clk(clk_level);
	}

set_vpu_clk_limit:
	mutex_unlock(&vpu_mutex);
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

	mutex_lock(&vpu_mutex);

	if (vmod < VPU_MAX) {
		vpu_clk = clk_vmod[vmod];
		vpu_clk = vpu_clk_table[vpu_clk][0];
	} else {
		vpu_clk = 0;
		VPUERR("unsupport vmod\n");
	}

	mutex_unlock(&vpu_mutex);
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
		return 1;

	mutex_lock(&vpu_mutex);

	if (vclk >= 100) /* regard as vpu_clk */
		clk_level = get_vpu_clk_level(vclk);
	else /* regard as clk_level */
		clk_level = vclk;

	if (clk_level >= vpu_conf.clk_level_max) {
		ret = 1;
		VPUERR("set vpu clk out of supported range\n");
		goto request_vpu_clk_limit;
	}

	if (vmod == VPU_MAX) {
		ret = 1;
		VPUERR("unsupport vmod\n");
		goto request_vpu_clk_limit;
	}

	clk_vmod[vmod] = clk_level;
	if (vpu_debug_print_flag) {
		VPUPR("request vpu clk: %s %uHz\n",
			vpu_mod_table[vmod],
			vpu_clk_table[clk_level][0]);
		dump_stack();
	}

	clk_level = get_vpu_clk_level_max_vmod();
	if (clk_level != vpu_conf.clk_level)
		adjust_vpu_clk(clk_level);

request_vpu_clk_limit:
	mutex_unlock(&vpu_mutex);
#endif
	return ret;
}

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
		return 1;

	mutex_lock(&vpu_mutex);

	clk_level = 0;
	if (vmod == VPU_MAX) {
		ret = 1;
		VPUERR("unsupport vmod\n");
		goto release_vpu_clk_limit;
	}

	clk_vmod[vmod] = clk_level;
	if (vpu_debug_print_flag) {
		VPUPR("release vpu clk: %s\n",
			vpu_mod_table[vmod]);
		dump_stack();
	}

	clk_level = get_vpu_clk_level_max_vmod();
	if (clk_level != vpu_conf.clk_level)
		adjust_vpu_clk(clk_level);

release_vpu_clk_limit:
	mutex_unlock(&vpu_mutex);
#endif
	return ret;
}

/* *********************************************** */

/* *********************************************** */
/* VPU sysfs function */
/* *********************************************** */
static const char *vpu_usage_str = {
"Usage:\n"
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
	unsigned int ret;
	unsigned int tmp[2], n;
	unsigned int fclk_type;

	fclk_type = vpu_conf.fclk_type;
	switch (buf[0]) {
	case 'g': /* get */
		VPUPR("get current clk: %uHz\n", get_vpu_clk());
		break;
	case 's': /* set */
		tmp[0] = 4;
		ret = sscanf(buf, "set %u", &tmp[0]);
		if (tmp[0] > 100)
			VPUPR("set clk frequency: %uHz\n", tmp[0]);
		else
			VPUPR("set clk level: %u\n", tmp[0]);
		set_vpu_clk(tmp[0]);
		break;
	case 'r':
		if (buf[2] == 'q') { /* request */
			tmp[0] = 0;
			tmp[1] = VPU_MAX;
			ret = sscanf(buf, "request %u %u", &tmp[0], &tmp[1]);
			request_vpu_clk_vmod(tmp[0], tmp[1]);
		} else if (buf[2] == 'l') { /* release */
			tmp[0] = VPU_MAX;
			ret = sscanf(buf, "release %u", &tmp[0]);
			release_vpu_clk_vmod(tmp[0]);
		}
		break;
	case 'd':
		tmp[0] = VPU_MAX;
		ret = sscanf(buf, "dump %u", &tmp[0]);
		if (tmp[0] == VPU_MAX) {
			n = get_vpu_clk_level_max_vmod();
			VPUPR("clk max holdings: %uHz(%u)\n",
				vpu_clk_table[n][0], n);
		} else {
			VPUPR("clk holdings:\n");
			pr_info("%s:  %uHz(%u)\n", vpu_mod_table[tmp[0]],
			vpu_clk_table[clk_vmod[tmp[0]]][0],
			clk_vmod[tmp[0]]);
		}
		break;
	default:
		VPUERR("wrong debug command\n");
		break;
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
	/* return 0; */
}

static ssize_t vpu_mem_debug(struct class *class, struct class_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int tmp[2];
	unsigned int _reg0, _reg1, _reg2;

	_reg0 = HHI_VPU_MEM_PD_REG0;
	_reg1 = HHI_VPU_MEM_PD_REG1;
	_reg2 = HHI_VPU_MEM_PD_REG2;
	switch (buf[0]) {
	case 'r':
		VPUPR("mem_pd0: 0x%08x\n", vpu_hiu_read(_reg0));
		VPUPR("mem_pd1: 0x%08x\n", vpu_hiu_read(_reg1));
		if ((vpu_chip_type == VPU_CHIP_GXL) ||
			(vpu_chip_type == VPU_CHIP_GXM) ||
			(vpu_chip_type == VPU_CHIP_TXL)) {
			VPUPR("mem_pd2: 0x%08x\n", vpu_hiu_read(_reg2));
		}
		break;
	case 'w':
		ret = sscanf(buf, "w %u %u", &tmp[0], &tmp[1]);
		tmp[0] = (tmp[0] > VPU_MAX) ? VPU_MAX : tmp[0];
		tmp[1] = (tmp[1] == VPU_MEM_POWER_ON) ? 0 : 1;
		VPUPR("switch_vpu_mem_pd: %s %s\n",
			vpu_mod_table[tmp[0]], (tmp[1] ? "DOWN" : "ON"));
		switch_vpu_mem_pd_vmod(tmp[0], tmp[1]);
		break;
	default:
		VPUERR("wrong mem_pd command\n");
		break;
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
	/* return 0; */
}

static ssize_t vpu_clk_gate_debug(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int tmp[2];

	switch (buf[0]) {
	case 'r':
		VPUPR("HHI_GCLK_OTHER: 0x%08x\n", vpu_hiu_read(HHI_GCLK_OTHER));
		VPUPR("VPU_CLK_GATE: 0x%08x\n", vpu_vcbus_read(VPU_CLK_GATE));
		VPUPR("VDIN0_COM_GCLK_CTRL: 0x%08x\n",
			vpu_vcbus_read(VDIN0_COM_GCLK_CTRL));
		VPUPR("VDIN0_COM_GCLK_CTRL2: 0x%08x\n",
			vpu_vcbus_read(VDIN0_COM_GCLK_CTRL2));
		VPUPR("VDIN1_COM_GCLK_CTRL: 0x%08x\n",
			vpu_vcbus_read(VDIN1_COM_GCLK_CTRL));
		VPUPR("VDIN1_COM_GCLK_CTRL2: 0x%08x\n",
			vpu_vcbus_read(VDIN1_COM_GCLK_CTRL2));
		VPUPR("DI_CLKG_CTRL: 0x%08x\n",
			vpu_vcbus_read(DI_CLKG_CTRL));
		VPUPR("VPP_GCLK_CTRL0: 0x%08x\n",
			vpu_vcbus_read(VPP_GCLK_CTRL0));
		VPUPR("VPP_GCLK_CTRL1: 0x%08x\n",
			vpu_vcbus_read(VPP_GCLK_CTRL1));
		VPUPR("VPP_SC_GCLK_CTRL: 0x%08x\n",
			vpu_vcbus_read(VPP_SC_GCLK_CTRL));
		VPUPR("VPP_XVYCC_GCLK_CTRL: 0x%08x\n",
			vpu_vcbus_read(VPP_XVYCC_GCLK_CTRL));
		break;
	case 'w':
		ret = sscanf(buf, "w %u %u", &tmp[0], &tmp[1]);
		tmp[0] = (tmp[0] > VPU_MAX) ? VPU_MAX : tmp[0];
		tmp[1] = (tmp[1] == VPU_CLK_GATE_ON) ? 1 : 0;
		VPUPR("switch_vpu_clk_gate: %s %s\n",
			vpu_mod_table[tmp[0]], (tmp[1] ? "ON" : "OFF"));
		switch_vpu_clk_gate_vmod(tmp[0], tmp[1]);
		break;
	default:
		VPUERR("wrong clk_gate command\n");
		break;
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
	/* return 0; */
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
	unsigned int ret;

	vcbus_test();

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
	/* return 0; */
}

static ssize_t vpu_print_debug(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;

	/*ret = sscanf(buf, "%u", &temp);*/
	ret = kstrtoint(buf, 10, &vpu_debug_print_flag);
	pr_info("set vpu debug_print_flag: %d\n", vpu_debug_print_flag);

	return count;
}

static struct class_attribute vpu_debug_class_attrs[] = {
	__ATTR(clk, 0644, vpu_debug_help, vpu_clk_debug),
	__ATTR(mem, 0644, vpu_debug_help, vpu_mem_debug),
	__ATTR(gate, 0644, vpu_debug_help, vpu_clk_gate_debug),
	__ATTR(test, 0644, vpu_debug_help, vpu_test_debug),
	__ATTR(print, 0644, vpu_debug_help, vpu_print_debug),
	__ATTR(help, 0644, vpu_debug_help, NULL),
};

static struct class *vpu_debug_class;
static int creat_vpu_debug_class(void)
{
	int i;

	vpu_debug_class = class_create(THIS_MODULE, "vpu");
	if (IS_ERR(vpu_debug_class)) {
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
	return 0;
}

static int vpu_resume(struct platform_device *pdev)
{
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
		if (val >= vpu_conf.clk_level_max) {
			VPUERR("clk_level is out of support, set to default\n");
			val = vpu_conf.clk_level_dft;
		}

		vpu_conf.clk_level = val;
	}
	VPUPR("load vpu_clk: %uHz(%u)\n",
		vpu_clk_table[val][0], vpu_conf.clk_level);

	return ret;
}

static void vpu_clktree_init(struct device *dev)
{
	static struct clk *vpu_clktree;

	vpu_clktree = devm_clk_get(dev, "vapb_clk");
	if (IS_ERR(vpu_clktree))
		VPUERR("%s: vapb_clk\n", __func__);
	else
		clk_prepare_enable(vpu_clktree);

	vpu_clktree = devm_clk_get(dev, "vpu_clk");
	if (IS_ERR(vpu_clktree))
		VPUERR("%s: vpu_clk\n", __func__);
	else
		clk_prepare_enable(vpu_clktree);

	VPUPR("%s\n", __func__);
}

static const struct of_device_id vpu_of_table[] = {
	{
		.compatible = "amlogic, vpu",
	},
	{},
};

static int vpu_probe(struct platform_device *pdev)
{
	int ret = 0;

#ifdef VPU_DEBUG_PRINT
	vpu_debug_print_flag = 1;
#else
	vpu_debug_print_flag = 0;
#endif
	spin_lock_init(&vpu_lock);

	VPUPR("driver version: %s\n", VPU_VERION);
	memset(clk_vmod, 0, sizeof(clk_vmod));
	vpu_chip_detect();
	ret = vpu_chip_valid_check();
	if (ret)
		return 0;

	vpu_ioremap();
	get_vpu_config(pdev);
	vpu_ctrl_probe();

	set_vpu_clk(vpu_conf.clk_level);
	vpu_clktree_init(&pdev->dev);

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
