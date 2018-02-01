/*
 * drivers/amlogic/usb/phy/usbphy.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/amlogic/gpio-amlogic.h>
#include <linux/amlogic/usbtype.h>
#include <linux/amlogic/iomap.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <linux/amlogic/usb-gxbb.h>
#include <linux/amlogic/usb-gxbbtv.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

/*
 * M chip USB clock setting
 */
static int init_count;

struct clk_reset {
	struct clk *usb_reset_usb_general;
	struct clk *usb_reset_usb;
	struct clk *usb_reset_usb_to_ddr;
};

struct clk_reset p_clk_reset[4];

/*ret 1: device plug in
   ret  0: device plug out*/
int device_status(unsigned long usb_peri_reg)
{
	struct u2p_aml_regs_t u2p_aml_regs;
	union u2p_r2_t reg2;
	int ret = 1;

	u2p_aml_regs.u2p_r[2] = (void __iomem	*)
				((unsigned long)usb_peri_reg +
					PHY_REGISTER_SIZE + 0x8);
	reg2.d32 = readl(u2p_aml_regs.u2p_r[2]);
	if (!reg2.b.device_sess_vld)
		ret = 0;

	return ret;
}
EXPORT_SYMBOL(device_status);

/* ret 1: device plug in */
/* ret 0: device plug out */
int device_status_v2(unsigned long usb_peri_reg)
{
	struct u2p_aml_regs_v2 u2p_aml_regs;
	union u2p_r1_v2 reg1;
	int ret = 1;

	u2p_aml_regs.u2p_r_v2[1] = (void __iomem	*)
				((unsigned long)usb_peri_reg +
					PHY_REGISTER_SIZE + 0x4);
	reg1.d32 = readl(u2p_aml_regs.u2p_r_v2[1]);
	if (!reg1.b.OTGSESSVLD0)
		ret = 0;

	return ret;
}
EXPORT_SYMBOL(device_status_v2);


int clk_enable_usb_meson8(struct platform_device *pdev,
			const char *s_clock_name, unsigned long usb_peri_reg)
{
	int port_idx;
	const char *clk_name;
	usb_peri_reg_t *peri;
	usb_config_data_t config;
	usb_ctrl_data_t control;
	usb_adp_bc_data_t adp_bc;
	int clk_sel, clk_div, clk_src;
	int time_dly = 500;
	int i = 0;
	struct clk *usb_reset;

	if (!init_count) {
		init_count++;
		aml_cbus_update_bits(0x1102, 0x1<<2, 0x1<<2);
		for (i = 0; i < 1000; i++)
			udelay(time_dly);
	}

	clk_name = s_clock_name;

	if (!strcmp(clk_name, "usb0")) {
		usb_reset = devm_clk_get(&pdev->dev, "usb_general");
		clk_prepare_enable(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;
		usb_reset = devm_clk_get(&pdev->dev, "usb0");
		clk_prepare_enable(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb = usb_reset;
		usb_reset = devm_clk_get(&pdev->dev, "usb0_to_ddr");
		clk_prepare_enable(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_to_ddr = usb_reset;
		peri = (usb_peri_reg_t *)usb_peri_reg;
		port_idx = USB_PORT_IDX_A;
	} else if (!strcmp(clk_name, "usb1")) {
		usb_reset = devm_clk_get(&pdev->dev, "usb_general");
		clk_prepare_enable(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;
		usb_reset = devm_clk_get(&pdev->dev, "usb1");
		clk_prepare_enable(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb = usb_reset;
		usb_reset = devm_clk_get(&pdev->dev, "usb1_to_ddr");
		clk_prepare_enable(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_to_ddr = usb_reset;
		peri = (usb_peri_reg_t *)usb_peri_reg;
		port_idx = USB_PORT_IDX_B;
	} else {
		dev_err(&pdev->dev, "bad usb clk name: %s\n", clk_name);
		return -1;
	}

	clk_sel = USB_PHY_CLK_SEL_XTAL;
	clk_div = 1;
	clk_src = 24000000;

	config.d32 = peri->config;
	config.b.clk_32k_alt_sel = 1;
	peri->config = config.d32;

	control.d32 = peri->ctrl;
	control.b.fsel = 5;
	control.b.por = 1;
	peri->ctrl = control.d32;
	udelay(time_dly);
	control.b.por = 0;
	peri->ctrl = control.d32;
	udelay(time_dly);

	/* read back clock detected flag*/
	control.d32 = peri->ctrl;
#if 0
	if (is_meson_m8m2_cpu() && (port_idx == USB_PORT_IDX_B)) {
#endif
	if ((port_idx == USB_PORT_IDX_B)) {
		adp_bc.d32 = peri->adp_bc;
		adp_bc.b.aca_enable = 1;
		peri->adp_bc = adp_bc.d32;
		udelay(50);
		adp_bc.d32 = peri->adp_bc;
		if (adp_bc.b.aca_pin_float) {
			dev_err(&pdev->dev, "USB-B ID detect failed!\n");
			dev_err(&pdev->dev, "Please use the chip after version RevA1!\n");
			return -1;
		}
	}

	dmb(4);

	return 0;
}

void clk_disable_usb_meson8(struct platform_device *pdev,
				const char *s_clock_name,
				unsigned long usb_peri_reg)
{
	return;
}

int clk_resume_usb_meson8(struct platform_device *pdev,
			const char *s_clock_name,
			unsigned long usb_peri_reg)
{
	const char *clk_name;
	struct clk *usb_reset;

	clk_name = s_clock_name;

	if (0 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_prepare_enable(usb_reset);
	} else if (1 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_prepare_enable(usb_reset);
	} else {
		dev_err(&pdev->dev, "bad usb clk name: %s\n", clk_name);
		return -1;
	}

	dmb(4);

	return 0;
}


int clk_enable_usb_gxbaby(struct platform_device *pdev,
			const char *s_clock_name,
			unsigned long usb_peri_reg)
{
	int port_idx;
	const char *clk_name;
	usb_peri_reg_t *peri;
	usb_config_data_t config;
	usb_ctrl_data_t control;
	usb_adp_bc_data_t adp_bc;
	int clk_sel, clk_div, clk_src;
	int time_dly = 500;
	int i = 0;
	struct clk *usb_reset;

	if (!init_count) {
		init_count++;
		aml_cbus_update_bits(0x1102, 0x1<<2, 0x1<<2);
		for (i = 0; i < 1000; i++)
			udelay(time_dly);
	}

	clk_name = s_clock_name;

	if (!strcmp(clk_name, "usb0")) {
		usb_reset = devm_clk_get(&pdev->dev, "usb_general");
		clk_prepare_enable(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;
		usb_reset = devm_clk_get(&pdev->dev, "usb0");
		clk_prepare_enable(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb = usb_reset;
		usb_reset = devm_clk_get(&pdev->dev, "usb0_to_ddr");
		clk_prepare_enable(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_to_ddr = usb_reset;

		peri = (usb_peri_reg_t *)usb_peri_reg;
		port_idx = USB_PORT_IDX_A;
	} else if (!strcmp(clk_name, "usb1")) {
		usb_reset = devm_clk_get(&pdev->dev, "usb_general");
		clk_prepare_enable(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;
		usb_reset = devm_clk_get(&pdev->dev, "usb1");
		clk_prepare_enable(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb = usb_reset;
		usb_reset = devm_clk_get(&pdev->dev, "usb1_to_ddr");
		clk_prepare_enable(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_to_ddr = usb_reset;

		peri = (usb_peri_reg_t *)usb_peri_reg;
		port_idx = USB_PORT_IDX_B;
	} else {
		dev_err(&pdev->dev, "bad usb clk name: %s\n", clk_name);
		return -1;
	}

	clk_sel = USB_PHY_CLK_SEL_XTAL;
	clk_div = 1;
	clk_src = 24000000;


	config.d32 = peri->config;
	config.b.clk_32k_alt_sel = 1;
	peri->config = config.d32;

	control.d32 = peri->ctrl;
	control.b.fsel = 5;
	control.b.por = 1;
	peri->ctrl = control.d32;
	udelay(time_dly);
	control.b.por = 0;
	peri->ctrl = control.d32;
	udelay(time_dly);

	/* read back clock detected flag*/
	control.d32 = peri->ctrl;

	udelay(time_dly);

	/*to do. set usb port config*/
	if (port_idx == USB_PORT_IDX_B) {
		adp_bc.d32 = peri->adp_bc;
		adp_bc.b.aca_enable = 1;
		peri->adp_bc = adp_bc.d32;
		udelay(50);
		adp_bc.d32 = peri->adp_bc;
		if (adp_bc.b.aca_pin_float) {
			dev_err(&pdev->dev, "USB-B ID detect failed!\n");
			dev_err(&pdev->dev, "Please use the chip after version RevA1!\n");
			return -1;
		}
	}
	dmb(4);

	return 0;
}


void clk_disable_usb_gxbaby(struct platform_device *pdev,
				const char *s_clock_name,
				unsigned long usb_peri_reg)
{
	struct clk *usb_reset;

	if (0 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_disable_unprepare(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb;
		clk_disable_unprepare(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_disable_unprepare(usb_reset);
	} else if (1 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_disable_unprepare(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb;
		clk_disable_unprepare(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_disable_unprepare(usb_reset);
	} else {
		dev_err(&pdev->dev, "bad usb clk name.\n");
		return;
	}

	return;
}

static void set_device_mode(struct platform_device *pdev,
				unsigned long reg_addr, int controller_type)
{
	struct u2p_aml_regs_t u2p_aml_regs;
	struct usb_aml_regs_t usb_aml_regs;
	union u2p_r0_t reg0;
	union usb_r0_t r0 = {.d32 = 0};
	union usb_r1_t r1 = {.d32 = 0};
	union usb_r4_t r4 = {.d32 = 0};

	u2p_aml_regs.u2p_r[0] = (void __iomem	*)
				((unsigned long)reg_addr + PHY_REGISTER_SIZE);
	reg0.d32 = readl(u2p_aml_regs.u2p_r[0]);
	reg0.b.dmpulldown = 0;
	reg0.b.dppulldown = 0;
	writel(reg0.d32, u2p_aml_regs.u2p_r[0]);

	usb_aml_regs.usb_r[0] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*0);
	usb_aml_regs.usb_r[1] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*1);
	usb_aml_regs.usb_r[4] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*4);
	r0.d32 = readl(usb_aml_regs.usb_r[0]);
	r0.b.u2d_act = 1;
	writel(r0.d32, usb_aml_regs.usb_r[0]);

	r4.d32 = readl(usb_aml_regs.usb_r[4]);
	r4.b.p21_SLEEPM0 = 0x1;
	writel(r4.d32, usb_aml_regs.usb_r[4]);

	if (USB_OTG != controller_type) {
		r1.d32 = readl(usb_aml_regs.usb_r[1]);
		r1.b.u3h_host_u2_port_disable = 0x2;
		writel(r1.d32, usb_aml_regs.usb_r[1]);
	}
}

static void set_device_mode_v2(struct platform_device *pdev,
				unsigned long reg_addr, int controller_type)
{
	struct u2p_aml_regs_v2 u2p_aml_regs;
	struct usb_aml_regs_v2 usb_aml_regs;
	union u2p_r0_v2 reg0;
	union usb_r0_v2 r0 = {.d32 = 0};
	union usb_r1_v2 r1 = {.d32 = 0};
	union usb_r4_v2 r4 = {.d32 = 0};

	u2p_aml_regs.u2p_r_v2[0] = (void __iomem	*)
				((unsigned long)reg_addr + PHY_REGISTER_SIZE);
	usb_aml_regs.usb_r_v2[0] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*0);
	usb_aml_regs.usb_r_v2[1] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*1);
	usb_aml_regs.usb_r_v2[4] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*4);
	r0.d32 = readl(usb_aml_regs.usb_r_v2[0]);
	r0.b.u2d_act = 1;
	r0.b.u2d_ss_scaledown_mode = 0;
	writel(r0.d32, usb_aml_regs.usb_r_v2[0]);

	r4.d32 = readl(usb_aml_regs.usb_r_v2[4]);
	r4.b.p21_SLEEPM0 = 0x1;
	writel(r4.d32, usb_aml_regs.usb_r_v2[4]);

	if (controller_type != USB_OTG) {
		r1.d32 = readl(usb_aml_regs.usb_r_v2[1]);
		r1.b.u3h_host_u2_port_disable = 0x2;
		writel(r1.d32, usb_aml_regs.usb_r_v2[1]);
	}

	reg0.d32 = readl(u2p_aml_regs.u2p_r_v2[0]);
	reg0.b.host_device = 0;
	reg0.b.POR = 0;
	writel(reg0.d32, u2p_aml_regs.u2p_r_v2[0]);
}


int clk_enable_usb_gxbabytv(struct platform_device *pdev,
			const char *s_clock_name, unsigned long usb_peri_reg,
			int controller_type)
{
	struct clk *usb_reset;
	usb_reset = devm_clk_get(&pdev->dev, "usb_general");
	clk_prepare_enable(usb_reset);
	p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;
	usb_reset = devm_clk_get(&pdev->dev, "usb1");
	clk_prepare_enable(usb_reset);
	p_clk_reset[pdev->id].usb_reset_usb = usb_reset;
	usb_reset = devm_clk_get(&pdev->dev, "usb1_to_ddr");
	clk_prepare_enable(usb_reset);
	p_clk_reset[pdev->id].usb_reset_usb_to_ddr = usb_reset;
	set_device_mode(pdev, usb_peri_reg, controller_type);
	return 0;
}


int clk_enable_usb_gxl(struct platform_device *pdev,
			const char *s_clock_name, unsigned long usb_peri_reg,
			int controller_type)
{
	struct clk *usb_reset;

	usb_reset = devm_clk_get(&pdev->dev, "usb_general");
	clk_prepare_enable(usb_reset);
	p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;
	usb_reset = devm_clk_get(&pdev->dev, "usb1");
	clk_prepare_enable(usb_reset);
	p_clk_reset[pdev->id].usb_reset_usb_to_ddr = usb_reset;
	set_device_mode(pdev, usb_peri_reg, controller_type);
	return 0;
}


int clk_enable_usb_v2(struct platform_device *pdev,
			const char *s_clock_name, unsigned long usb_peri_reg,
			int controller_type)
{
	struct clk *usb_reset;

	usb_reset = devm_clk_get(&pdev->dev, "usb_general");
	clk_prepare_enable(usb_reset);
	p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;
	usb_reset = devm_clk_get(&pdev->dev, "usb1");
	clk_prepare_enable(usb_reset);
	p_clk_reset[pdev->id].usb_reset_usb_to_ddr = usb_reset;
	set_device_mode_v2(pdev, usb_peri_reg, controller_type);
	return 0;
}


void clk_disable_usb_gxbabytv(struct platform_device *pdev,
				const char *s_clock_name,
				unsigned long usb_peri_reg)
{
	struct clk *usb_reset;

	usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
	clk_disable_unprepare(usb_reset);
	usb_reset = p_clk_reset[pdev->id].usb_reset_usb;
	clk_disable_unprepare(usb_reset);
	usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
	clk_disable_unprepare(usb_reset);
	return;
}

void clk_disable_usb_gxl(struct platform_device *pdev,
				const char *s_clock_name,
				unsigned long usb_peri_reg)
{
	struct clk *usb_reset;

	usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
	clk_disable_unprepare(usb_reset);
	usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
	clk_disable_unprepare(usb_reset);
	return;
}

void clk_disable_usb_v2(struct platform_device *pdev,
				const char *s_clock_name,
				unsigned long usb_peri_reg)
{
	struct clk *usb_reset;

	usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
	clk_disable_unprepare(usb_reset);
	usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
	clk_disable_unprepare(usb_reset);
}


int clk_resume_usb_gxbaby(struct platform_device *pdev,
			const char *s_clock_name,
			unsigned long usb_peri_reg)
{
	struct clk *usb_reset;

	if (0 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_prepare_enable(usb_reset);
	} else if (1 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_prepare_enable(usb_reset);
	} else {
		dev_err(&pdev->dev, "bad usb clk name.\n");
		return -1;
	}

	dmb(4);

	return 0;
}

int clk_resume_usb_gxl(struct platform_device *pdev,
			const char *s_clock_name,
			unsigned long usb_peri_reg)
{
	struct clk *usb_reset;

	if (0 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_prepare_enable(usb_reset);
	} else if (1 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_prepare_enable(usb_reset);
	} else {
		dev_err(&pdev->dev, "bad usb clk name.\n");
		return -1;
	}

	dmb(4);

	return 0;
}


int clk_resume_usb_v2(struct platform_device *pdev,
			const char *s_clock_name,
			unsigned long usb_peri_reg)
{
	struct clk *usb_reset;

	if (pdev->id == 0) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_prepare_enable(usb_reset);
	} else if (pdev->id == 1) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_prepare_enable(usb_reset);
	} else {
		dev_err(&pdev->dev, "bad usb clk name.\n");
		return -1;
	}

	dmb(4);

	return 0;
}


int clk_enable_usb(struct platform_device *pdev, const char *s_clock_name,
		unsigned long usb_peri_reg, const char *cpu_type,
		int controller_type)
{
	int ret = 0;

	if (!pdev)
		return -1;

	if (!strcmp(cpu_type, MESON8))
		ret = clk_enable_usb_meson8(pdev,
				s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXBABY))
		ret = clk_enable_usb_gxbaby(pdev,
				s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXBABYTV))
		ret = clk_enable_usb_gxbabytv(pdev,
				s_clock_name, usb_peri_reg, controller_type);
	else if (!strcmp(cpu_type, GXL))
		ret = clk_enable_usb_gxl(pdev,
				s_clock_name, usb_peri_reg, controller_type);
	else if (!strcmp(cpu_type, V2))
		ret = clk_enable_usb_v2(pdev,
				s_clock_name, usb_peri_reg, controller_type);

	/*add other cpu type's usb clock enable*/

	return ret;
}
EXPORT_SYMBOL(clk_enable_usb);

int clk_disable_usb(struct platform_device *pdev, const char *s_clock_name,
		unsigned long usb_peri_reg,
		const char *cpu_type)
{
	if (!pdev)
		return -1;

	if (!strcmp(cpu_type, MESON8))
			clk_disable_usb_meson8(pdev,
				s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXBABY))
			clk_disable_usb_gxbaby(pdev,
				s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXBABYTV))
			clk_disable_usb_gxbabytv(pdev,
				s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXL))
			clk_disable_usb_gxl(pdev,
				s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, V2))
		clk_disable_usb_v2(pdev,
			s_clock_name, usb_peri_reg);

	dmb(4);
	return 0;
}
EXPORT_SYMBOL(clk_disable_usb);

int clk_resume_usb(struct platform_device *pdev, const char *s_clock_name,
		unsigned long usb_peri_reg,
		const char *cpu_type)
{
	int ret = 0;

	if (!pdev)
		return -1;

	if (!strcmp(cpu_type, MESON8))
		ret = clk_resume_usb_meson8(pdev,
			s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXBABY))
		ret = clk_resume_usb_gxbaby(pdev,
			s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXL))
		ret = clk_resume_usb_gxl(pdev,
			s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, V2))
		ret = clk_resume_usb_v2(pdev,
			s_clock_name, usb_peri_reg);

	/*add other cpu type's usb clock enable*/

	return ret;
}
EXPORT_SYMBOL(clk_resume_usb);

int clk_suspend_usb(struct platform_device *pdev, const char *s_clock_name,
		unsigned long usb_peri_reg,
		const char *cpu_type)
{
	if (!pdev)
		return -1;

	if (!strcmp(cpu_type, MESON8))
			clk_disable_usb_meson8(pdev,
				s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXBABY))
			clk_disable_usb_gxbaby(pdev,
				s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXL))
			clk_disable_usb_gxl(pdev,
				s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, V2))
		clk_disable_usb_v2(pdev,
			s_clock_name, usb_peri_reg);

	dmb(4);
	return 0;
}
EXPORT_SYMBOL(clk_suspend_usb);
