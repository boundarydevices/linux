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
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <mach/hardware.h>

#define REG_SET		0x4
#define REG_CLR		0x8
#define ANA_MISC2	0x170
#define ANA_MISC0	0x150
#define ANA_REG_CORE	0x140
#define ANA_REG_2P5		0x130

#define ANA_USB1_CHRG_DETECT	0x1b0
#define ANA_USB2_CHRG_DETECT	0x210

#define BM_ANADIG_USB_CHRG_DETECT_EN_B	0x100000
#define BM_ANADIG_USB_CHRG_DETECT_CHK_CHRG_B	0x80000

#define ANA_REG_CORE_REG2_OFFSET	18/* VDDSOC */
#define ANA_REG_CORE_REG1_OFFSET	9 /* VDDPU */
#define ANA_REG_CORE_REG0_OFFSET	0 /* VDDARM */
#define ANA_REG_CORE_REG_MASK		0x1f

#define ANA_MISC2_REG1_STEP_OFFSET	26
#define ANA_MISC2_REG_STEP_MASK		0x3

#define LDO_RAMP_UP_UNIT_IN_CYCLES      64 /* 64 cycles per step */
#define LDO_RAMP_UP_FREQ_IN_MHZ         24 /* time base on 24M OSC */
static struct regmap *anatop;

void imx_anatop_pu_vol(bool enable)
{
	unsigned int val;
	unsigned int vddsoc;

	regmap_read(anatop, ANA_REG_CORE, &val);
	if (enable) {
		/* VDDPU track with VDDSOC if enable */
		val &= ~(ANA_REG_CORE_REG_MASK << ANA_REG_CORE_REG1_OFFSET);
		vddsoc = val & (ANA_REG_CORE_REG_MASK <<
			ANA_REG_CORE_REG2_OFFSET);
		val |= vddsoc >> (ANA_REG_CORE_REG2_OFFSET -
			ANA_REG_CORE_REG1_OFFSET);
	} else
		/* power off pu */
		val &= ~(ANA_REG_CORE_REG_MASK << ANA_REG_CORE_REG1_OFFSET);
	regmap_write(anatop, ANA_REG_CORE, val);

	if (enable) {
		/*
		 * the delay time for LDO ramp up time is
		 * based on the register setting, we need
		 * to calculate how many steps LDO need to
		 * ramp up, and how much delay needed. (us)
		 */
		static unsigned int new_vol, delay_u;
		regmap_read(anatop, ANA_MISC2, &val);
		val = (val >> ANA_MISC2_REG1_STEP_OFFSET) &
			ANA_MISC2_REG_STEP_MASK;
		new_vol = (vddsoc >> ANA_REG_CORE_REG2_OFFSET) &
				ANA_REG_CORE_REG_MASK;
		/*
		 * Because we can't know what the exact voltage of PU here if
		 * use bypass mode(0x1f), the delay time for LDO ramp up should
		 * rely on external pmic ,here we validate on pfuze. On Sabre
		 * board VDDPU_IN and VDDSOC_IN connected, so max steps on
		 * VDDPU_IN in bypass mode is  4 steps of 25mv(1.175V->1.275V),
		 * and the delay time is 4us*4=16us. You may fine tune it,
		 * especially, you disconnect VDDPU_IN and VDDSOC_IN.
		 * if you want to power on/off external PU regulator to minimal
		 * power leakage, you can add your specific code here.
		 * But in our case, VDDPU_IN and VDDSOC_IN share the same input
		 * from pfuze, and VDDSOC_IN can't be turned off, then the
		 * delay time is decided by VDDPU switch from 0 to bypass, seem
		 * fixed on 50us, enlarge to 70us for safe.
		 */
		if (new_vol == 0x1f)
			delay_u = 70;
		else
			delay_u = new_vol  * ((LDO_RAMP_UP_UNIT_IN_CYCLES <<
					val) / LDO_RAMP_UP_FREQ_IN_MHZ + 1);
		udelay(delay_u);
	}
}

int imx_anatop_pu_is_enabled(void)
{
	unsigned int val;
	regmap_read(anatop, ANA_REG_CORE, &val);
	return (val & ANA_REG_CORE_REG_MASK << ANA_REG_CORE_REG1_OFFSET)
		? 1 : 0;
}

void imx_anatop_bypass_ldo(void)
{
	regmap_update_bits(anatop, ANA_REG_CORE,
		ANA_REG_CORE_REG_MASK << ANA_REG_CORE_REG0_OFFSET |
		ANA_REG_CORE_REG_MASK << ANA_REG_CORE_REG1_OFFSET |
		ANA_REG_CORE_REG_MASK << ANA_REG_CORE_REG2_OFFSET ,
		ANA_REG_CORE_REG_MASK << ANA_REG_CORE_REG0_OFFSET |
		ANA_REG_CORE_REG_MASK << ANA_REG_CORE_REG1_OFFSET |
		ANA_REG_CORE_REG_MASK << ANA_REG_CORE_REG2_OFFSET);
}

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
