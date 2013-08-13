/*
 * Copyright (C) 2013 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include "common.h"
#include "hardware.h"

#define REG_SET		0x4
#define REG_CLR		0x8

#define ANADIG_REG_2P5		0x130
#define ANADIG_REG_CORE		0x140
#define ANADIG_ANA_MISC0	0x150
#define ANADIG_ANA_MISC2	0x170
#define ANADIG_USB1_CHRG_DETECT	0x1b0
#define ANADIG_USB2_CHRG_DETECT	0x210
#define ANADIG_DIGPROG		0x260

#define BM_ANADIG_REG_2P5_ENABLE_WEAK_LINREG	0x40000
#define BM_ANADIG_REG_CORE_FET_ODRIVE		0x20000000
#define BM_ANADIG_ANA_MISC0_STOP_MODE_CONFIG	0x1000
#define BM_ANADIG_USB_CHRG_DETECT_CHK_CHRG_B	0x80000
#define BM_ANADIG_USB_CHRG_DETECT_EN_B		0x100000

#define ANADIG_ANA_MISC2_REG1_STEP_OFFSET	26
#define ANADIG_ANA_MISC2_REG_STEP_MASK		0x3
#define ANADIG_REG_CORE_REG_MASK		0x1f
#define ANADIG_REG_CORE_REG2_OFFSET		18	/* VDDSOC */
#define ANADIG_REG_CORE_REG1_OFFSET		9	/* VDDPU */

#define LDO_RAMP_UP_UNIT_IN_CYCLES		64	/* 64 cycles per step */
#define LDO_RAMP_UP_FREQ_IN_MHZ			24	/* base on 24M OSC */

static struct regmap *anatop;

static void imx_anatop_pu_enable(bool enable)
{
	u32 val, vddsoc;

	regmap_read(anatop, ANADIG_REG_CORE, &val);
	if (enable) {
		/* VDDPU track with VDDSOC if enable */
		val &= ~(ANADIG_REG_CORE_REG_MASK <<
			ANADIG_REG_CORE_REG1_OFFSET);
		vddsoc = val & (ANADIG_REG_CORE_REG_MASK <<
			ANADIG_REG_CORE_REG2_OFFSET);
		val |= vddsoc >> (ANADIG_REG_CORE_REG2_OFFSET -
			ANADIG_REG_CORE_REG1_OFFSET);
	} else
		/* power off pu */
		val &= ~(ANADIG_REG_CORE_REG_MASK <<
			ANADIG_REG_CORE_REG1_OFFSET);
	regmap_write(anatop, ANADIG_REG_CORE, val);

	if (enable) {
		/*
		 * the delay time for LDO ramp up time is
		 * based on the register setting, we need
		 * to calculate how many steps LDO need to
		 * ramp up, and how much delay needed. (us)
		 */
		static u32 new_vol, delay_u;
		regmap_read(anatop, ANADIG_ANA_MISC2, &val);
		val = (val >> ANADIG_ANA_MISC2_REG1_STEP_OFFSET) &
			ANADIG_ANA_MISC2_REG_STEP_MASK;
		new_vol = (vddsoc >> ANADIG_REG_CORE_REG2_OFFSET) &
			ANADIG_REG_CORE_REG_MASK;
		delay_u = new_vol * ((LDO_RAMP_UP_UNIT_IN_CYCLES <<
			val) / LDO_RAMP_UP_FREQ_IN_MHZ + 1);
		udelay(delay_u);
	}
}

static void imx_anatop_enable_weak2p5(bool enable)
{
	u32 reg, val;

	regmap_read(anatop, ANADIG_ANA_MISC0, &val);

	/* can only be enabled when stop_mode_config is clear. */
	reg = ANADIG_REG_2P5;
	reg += (enable && (val & BM_ANADIG_ANA_MISC0_STOP_MODE_CONFIG) == 0) ?
		REG_SET : REG_CLR;
	regmap_write(anatop, reg, BM_ANADIG_REG_2P5_ENABLE_WEAK_LINREG);
}

static void imx_anatop_enable_fet_odrive(bool enable)
{
	regmap_write(anatop, ANADIG_REG_CORE + (enable ? REG_SET : REG_CLR),
		BM_ANADIG_REG_CORE_FET_ODRIVE);
}

void imx_anatop_pre_suspend(void)
{
	imx_anatop_enable_weak2p5(true);
	imx_anatop_enable_fet_odrive(true);
}

void imx_anatop_post_resume(void)
{
	imx_anatop_enable_fet_odrive(false);
	imx_anatop_enable_weak2p5(false);
}

void imx_anatop_usb_chrg_detect_disable(void)
{
	regmap_write(anatop, ANADIG_USB1_CHRG_DETECT,
		BM_ANADIG_USB_CHRG_DETECT_EN_B
		| BM_ANADIG_USB_CHRG_DETECT_CHK_CHRG_B);
	regmap_write(anatop, ANADIG_USB2_CHRG_DETECT,
		BM_ANADIG_USB_CHRG_DETECT_EN_B |
		BM_ANADIG_USB_CHRG_DETECT_CHK_CHRG_B);
}

void __init imx_init_revision_from_anatop(void)
{
	struct device_node *np;
	void __iomem *anatop_base;
	unsigned int revision;
	u32 digprog;

	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-anatop");
	anatop_base = of_iomap(np, 0);
	WARN_ON(!anatop_base);
	digprog = readl_relaxed(anatop_base + ANADIG_DIGPROG);
	iounmap(anatop_base);

	switch (digprog & 0xff) {
	case 0:
		revision = IMX_CHIP_REVISION_1_0;
		break;
	case 1:
		revision = IMX_CHIP_REVISION_1_1;
		break;
	case 2:
		revision = IMX_CHIP_REVISION_1_2;
		break;
	default:
		revision = IMX_CHIP_REVISION_UNKNOWN;
	}

	mxc_set_cpu_type(digprog >> 16 & 0xff);
	imx_set_soc_revision(revision);
}

void __init imx_anatop_init(void)
{
	anatop = syscon_regmap_lookup_by_compatible("fsl,imx6q-anatop");
	if (IS_ERR(anatop)) {
		pr_err("%s: failed to find imx6q-anatop regmap!\n", __func__);
		return;
	}

	/*
	 * PU is turned off in uboot, so we need to turn it on here
	 * to avoid kernel hang during GPU init, will remove
	 * this code after PU power management done.
	 */
	imx_anatop_pu_enable(true);
	imx_gpc_xpu_enable();
}
