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

#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <mach/hardware.h>

#define REG_SET		0x4
#define REG_CLR		0x8
#define ANA_MISC0	0x150
#define ANA_REG_CORE	0x140
#define ANA_REG_2P5		0x130

#define ANA_USB1_CHRG_DETECT	0x1b0
#define ANA_USB2_CHRG_DETECT	0x210

#define BM_ANADIG_USB_CHRG_DETECT_EN_B	0x100000
#define BM_ANADIG_USB_CHRG_DETECT_CHK_CHRG_B	0x80000

static struct regmap *anatop;

void imx_anatop_set_stop_mode_config(bool enable)
{
	regmap_write(anatop, ANA_MISC0 + (enable ?
		REG_SET : REG_CLR), 0x1 << 12);
}

void imx_anatop_enable_weak2p5(bool enable)
{
	unsigned int val;

	regmap_read(anatop, ANA_MISC0, &val);

	/* can only be enabled when stop_mode_config is clear */
	regmap_write(anatop, ANA_REG_2P5 + ((enable &&
		((val & (0x1 << 12)) == 0)) ? REG_SET :
		REG_CLR), 0x1 << 18);
}

void imx_anatop_enable_fet_odrive(bool enable)
{
	regmap_write(anatop, ANA_REG_CORE + (enable ?
		REG_SET : REG_CLR), 0x1 << 29);
}

void imx_anatop_pre_suspend(void)
{
	imx_anatop_enable_weak2p5(true);
	if (imx6q_revision() > IMX_CHIP_REVISION_1_0)
		imx_anatop_enable_fet_odrive(true);
}

void imx_anatop_post_resume(void)
{
	imx_anatop_enable_weak2p5(false);
	if (imx6q_revision() > IMX_CHIP_REVISION_1_0)
		imx_anatop_enable_fet_odrive(false);
}

unsigned int imx_anatop_get_core_reg_setting(void)
{
	unsigned int val;

	regmap_read(anatop, ANA_REG_CORE, &val);
	return val;
}

void imx_anatop_usb_chrg_detect_disable(void)
{
	regmap_write(anatop, ANA_USB1_CHRG_DETECT,
		BM_ANADIG_USB_CHRG_DETECT_EN_B
		| BM_ANADIG_USB_CHRG_DETECT_CHK_CHRG_B);
	regmap_write(anatop, ANA_USB2_CHRG_DETECT,
		BM_ANADIG_USB_CHRG_DETECT_EN_B |
		BM_ANADIG_USB_CHRG_DETECT_CHK_CHRG_B);
}

void __init imx_anatop_init(void)
{
	anatop = syscon_regmap_lookup_by_compatible("fsl,imx6q-anatop");
	if (IS_ERR(anatop)) {
		pr_err("failed to find imx6q-anatop regmap!\n");
		return;
	}
}
