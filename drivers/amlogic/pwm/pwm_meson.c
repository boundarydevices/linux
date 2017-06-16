/*
 * drivers/amlogic/pwm/pwm_meson.c
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

#undef pr_fmt
#define pr_fmt(fmt) "pwm: " fmt

#include <linux/amlogic/pwm_meson.h>
#include "pwm_meson_util.h"

static DEFINE_SPINLOCK(aml_pwm_lock);


void pwm_set_reg_bits(void __iomem  *reg,
						unsigned int mask,
						const unsigned int val)
{
	unsigned int tmp, orig;

	orig = readl(reg);
	tmp = orig & ~mask;
	tmp |= val;
	writel(tmp, reg);
}

void pwm_write_reg(void __iomem *reg,
						const unsigned int  val)
{
	unsigned int tmp, orig;

	orig = readl(reg);
	tmp = orig & ~(0xffffffff);
	tmp |= val;
	writel(tmp, reg);
};

void pwm_clear_reg_bits(void __iomem *reg, const unsigned int val)
{
	unsigned int tmp, orig;

	orig = readl(reg);
	tmp = orig & ~val;
	writel(tmp, reg);
}

void pwm_write_reg1(void __iomem *reg, const unsigned int  val)
{
	unsigned int tmp = 0, orig;

	orig = readl(reg);
	tmp = orig;
	tmp |= val;
	writel(tmp, reg);
};

struct aml_pwm_chip *to_aml_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct aml_pwm_chip, chip);
}

static
struct aml_pwm_channel *pwm_aml_calc(struct aml_pwm_chip *chip,
						struct pwm_device *pwm,
						unsigned int duty_ns,
						unsigned int period_ns,
						struct clk	*clk)
{
	struct aml_pwm_channel *our_chan = pwm_get_chip_data(pwm);
	unsigned int fout_freq = 0, pwm_pre_div = 0;
	unsigned int i = 0;
	unsigned long  temp = 0;
	unsigned long pwm_cnt = 0;
	unsigned long rate = 0;
	unsigned int pwm_freq;
	unsigned long freq_div;

	if ((duty_ns < 0) || (duty_ns > period_ns)) {
		dev_err(chip->chip.dev, "Not available duty error!\n");
		return NULL;
	}

	if (!IS_ERR(clk))
		rate = clk_get_rate(clk);

	pwm_freq = NSEC_PER_SEC / period_ns;

	fout_freq = ((pwm_freq >= ((rate/1000) * 500)) ?
					((rate/1000) * 500) : pwm_freq);
	for (i = 0; i < 0x7f; i++) {
		pwm_pre_div = i;
		freq_div = rate / (pwm_pre_div + 1);
		if (freq_div < pwm_freq)
			continue;
		pwm_cnt = freq_div / pwm_freq;
		if (pwm_cnt <= 0xffff)
			break;
	}

	our_chan->pwm_pre_div = pwm_pre_div;
	if (duty_ns == 0) {
		our_chan->pwm_hi = 0;
		our_chan->pwm_lo = pwm_cnt;
		return our_chan;
	} else if (duty_ns == period_ns) {
		our_chan->pwm_hi = pwm_cnt;
		our_chan->pwm_lo = 0;
		return our_chan;
	}

	temp = (unsigned long)(pwm_cnt * duty_ns);
	temp /= period_ns;

	our_chan->pwm_hi = (unsigned int)temp-1;
	our_chan->pwm_lo = pwm_cnt - (unsigned int)temp-1;
	our_chan->pwm_freq = pwm_freq;

	return our_chan;

}

static int pwm_aml_request(struct pwm_chip *chip,
						struct pwm_device *pwm)
{
	struct aml_pwm_chip *our_chip = to_aml_pwm_chip(chip);
	struct aml_pwm_channel *our_chan;

	if (!(our_chip->variant.output_mask & BIT(pwm->hwpwm))) {
		dev_warn(chip->dev,
		"tried to request PWM channel %d without output\n",
		pwm->hwpwm);
		return -EINVAL;
	}
	our_chan = devm_kzalloc(chip->dev, sizeof(*our_chan), GFP_KERNEL);
	if (!our_chan)
		return -ENOMEM;

	pwm_set_chip_data(pwm, our_chan);

	return 0;
}

static void pwm_aml_free(struct pwm_chip *chip,
							struct pwm_device *pwm)
{
	devm_kfree(chip->dev, pwm_get_chip_data(pwm));
	pwm_set_chip_data(pwm, NULL);
}
/*
 *do it for hardware defect,
 * PWM_A and PWM_B enable bit should be setted together.
*/
static int pwm_gxtvbb_enable(struct aml_pwm_chip *aml_chip,
		unsigned int id)
{
	struct pwm_aml_regs *aml_reg =
	(struct pwm_aml_regs *)pwm_id_to_reg(id, aml_chip);
	unsigned int val;

	switch (id) {
	case PWM_A:
	case PWM_B:
	case PWM_C:
	case PWM_D:
	case PWM_E:
	case PWM_F:
	case PWM_AO_A:
	case PWM_AO_B:
		val = 0x3 << 0;
		break;
	case PWM_A2:
	case PWM_C2:
	case PWM_E2:
	case PWM_AO_A2:
		val = 1 << 25;
		break;
	case PWM_B2:
	case PWM_D2:
	case PWM_F2:
	case PWM_AO_B2:
		val = 1 << 24;
		break;
	default:
		dev_err(aml_chip->chip.dev,
				"enable,index is not legal\n");
		return -EINVAL;
	}
	pwm_set_reg_bits(&aml_reg->miscr, val, val);

	return 0;
}
/*add pwm AO C/D for txlx addtional*/
static int pwm_meson_enable(struct aml_pwm_chip *aml_chip,
								unsigned int id)
{
	struct pwm_aml_regs *aml_reg =
	(struct pwm_aml_regs *)pwm_id_to_reg(id, aml_chip);
	unsigned int val;

	switch (id) {
	case PWM_A:
	case PWM_C:
	case PWM_E:
	case PWM_AO_A:
	case PWM_AO_C:
		val = 1 << 0;
		break;
	case PWM_B:
	case PWM_D:
	case PWM_F:
	case PWM_AO_B:
	case PWM_AO_D:
		val = 1 << 1;
		break;
	case PWM_A2:
	case PWM_C2:
	case PWM_E2:
	case PWM_AO_A2:
	case PWM_AO_C2:
		val = 1 << 25;
		break;
	case PWM_B2:
	case PWM_D2:
	case PWM_F2:
	case PWM_AO_B2:
	case PWM_AO_D2:
		val = 1 << 24;
		break;
	default:
		dev_err(aml_chip->chip.dev,
				"enable,index is not legal\n");
		return -EINVAL;
	}
	pwm_set_reg_bits(&aml_reg->miscr, val, val);

	return 0;
}

static int pwm_aml_enable(struct pwm_chip *chip,
							struct pwm_device *pwm)
{
	struct aml_pwm_chip *our_chip = to_aml_pwm_chip(chip);
	unsigned int id = pwm->hwpwm;
	unsigned long flags;

	spin_lock_irqsave(&aml_pwm_lock, flags);
	if (is_meson_gxtvbb_cpu())/*for gxtvbb hardware defect*/
		pwm_gxtvbb_enable(our_chip, id);
	else/*m8bb gxbb gxl gxm txlx txl enable*/
		pwm_meson_enable(our_chip, id);

	spin_unlock_irqrestore(&aml_pwm_lock, flags);
	return 0;
}

static void pwm_aml_disable(struct pwm_chip *chip,
						struct pwm_device *pwm)
{
	struct aml_pwm_chip *aml_chip = to_aml_pwm_chip(chip);
	unsigned int id = pwm->hwpwm;
	struct pwm_aml_regs *aml_reg =
	(struct pwm_aml_regs *)pwm_id_to_reg(id, aml_chip);
	unsigned long flags;
	unsigned int val;
	unsigned int mask;

	spin_lock_irqsave(&aml_pwm_lock, flags);
	switch (id) {
	case PWM_A:
	case PWM_C:
	case PWM_E:
	case PWM_AO_A:
	case PWM_AO_C:
		val = 0 << 0;
		mask = 1 << 0;
		break;
	case PWM_B:
	case PWM_D:
	case PWM_F:
	case PWM_AO_B:
	case PWM_AO_D:
		val = 0 << 1;
		mask = 1 << 1;
		break;
	case PWM_A2:
	case PWM_C2:
	case PWM_E2:
	case PWM_AO_A2:
	case PWM_AO_C2:
		val = 0 << 25;
		mask = 1 << 25;
		break;
	case PWM_B2:
	case PWM_D2:
	case PWM_F2:
	case PWM_AO_B2:
	case PWM_AO_D2:
		val = 0 << 24;
		mask = 1 << 24;
		break;
	default:
		val = 0 << 0;
		mask = 1 << 0;/*pwm_disable return void,add default value*/
		dev_err(aml_chip->chip.dev,
				"disable,index is not legal\n");
	break;
	}
	pwm_set_reg_bits(&aml_reg->miscr, mask, val);
	spin_unlock_irqrestore(&aml_pwm_lock, flags);

}

static int pwm_aml_clk(struct aml_pwm_chip *our_chip,
						struct pwm_device *pwm,
						unsigned int duty_ns,
						unsigned int period_ns,
						unsigned int offset)
{
	struct aml_pwm_channel *our_chan = pwm_get_chip_data(pwm);
	struct clk	*clk;

	switch ((our_chip->clk_mask >> offset)&0x3) {
	case 0x0:
		clk = our_chip->xtal_clk;
		break;
	case 0x1:
		clk = our_chip->vid_pll_clk;
		break;
	case 0x2:
		clk = our_chip->fclk_div4_clk;
		break;
	case 0x3:
		clk = our_chip->fclk_div3_clk;
		break;
	default:
		clk = our_chip->xtal_clk;
		break;
	}

	our_chan = pwm_aml_calc(our_chip, pwm, duty_ns, period_ns, clk);
	if (our_chan == NULL)
		return -EINVAL;

	return 0;
}

/*
 * 8 base channels configuration for gxbb, gxtvbb, gxl, gxm and txl
 */
static int pwm_meson_config(struct aml_pwm_chip *aml_chip,
			struct aml_pwm_channel *our_chan,
			unsigned int id)
{
	unsigned int clk_source_mask;
	unsigned int clk_source_val;
	unsigned int clk_mask;
	unsigned int clk_val;
	void __iomem  *duty_reg;
	unsigned int duty_val =
	(our_chan->pwm_hi << 16) | (our_chan->pwm_lo);
	struct pwm_aml_regs *aml_reg =
	(struct pwm_aml_regs *)pwm_id_to_reg(id, aml_chip);

	switch (id) {
	case PWM_A:
	case PWM_C:
	case PWM_E:
	case PWM_AO_A:
	case PWM_AO_C:
		clk_source_mask = 0x3 << 4;
		clk_source_val = ((aml_chip->clk_mask)&0x3) << 4;
		clk_mask = (0x7f << 8) | (1 << 15);
		clk_val = (our_chan->pwm_pre_div << 8) | (1 << 15);
		duty_reg = &aml_reg->dar;
		break;
	case PWM_B:
	case PWM_D:
	case PWM_F:
	case PWM_AO_B:
	case PWM_AO_D:
		clk_source_mask = 0x3 << 6;
		clk_source_val = ((aml_chip->clk_mask >> 2)&0x3) << 6;
		clk_mask = (0x7f << 16)|(1 << 23);
		clk_val = (our_chan->pwm_pre_div << 16)|(1 << 23);
		duty_reg = &aml_reg->dbr;
		break;
	default:
		dev_err(aml_chip->chip.dev,
				"config,index is not legal\n");
		return -EINVAL;
	break;
	}
	pwm_set_reg_bits(&aml_reg->miscr, clk_source_mask, clk_source_val);
	pwm_set_reg_bits(&aml_reg->miscr, clk_mask, clk_val);
	pwm_write_reg(duty_reg, duty_val);

	return 0;
}

/*
 * Additional 8 channels configuration for txl
 */
static int pwm_meson_config_ext(struct aml_pwm_chip *aml_chip,
					struct aml_pwm_channel *our_chan,
					unsigned int id)
{
	unsigned int clk_source_mask;
	unsigned int clk_source_val;
	unsigned int clk_mask;
	unsigned int clk_val;
	void __iomem  *duty_reg;
	unsigned int duty_val =
	(our_chan->pwm_hi << 16) | (our_chan->pwm_lo);
	struct pwm_aml_regs *aml_reg =
	(struct pwm_aml_regs *)pwm_id_to_reg(id, aml_chip);

	switch (id) {
	case PWM_A2:
	case PWM_C2:
	case PWM_E2:
	case PWM_AO_A2:
	case PWM_AO_C2:
		clk_source_mask = 0x3 << 4;
		clk_source_val = ((aml_chip->clk_mask)&0x3) << 4;
		clk_mask = (0x7f << 8)|(1 << 15);
		clk_val = (our_chan->pwm_pre_div << 8)|(1 << 15);
		duty_reg = &aml_reg->da2r;
		break;
	case PWM_B2:
	case PWM_D2:
	case PWM_F2:
	case PWM_AO_B2:
	case PWM_AO_D2:
		clk_source_mask = 0x3 << 6;
		clk_source_val = ((aml_chip->clk_mask >> 2)&0x3) << 6;
		clk_mask = (0x7f << 16)|(1 << 23);
		clk_val = (our_chan->pwm_pre_div << 16)|(1 << 23);
		duty_reg = &aml_reg->db2r;
		break;
	default:
		dev_err(aml_chip->chip.dev,
				"config_ext,index is not legal\n");
		return -EINVAL;
	}
	pwm_set_reg_bits(&aml_reg->miscr, clk_source_mask, clk_source_val);
	pwm_set_reg_bits(&aml_reg->miscr, clk_mask, clk_val);
	pwm_write_reg(duty_reg, duty_val);

	return 0;
}

static int pwm_aml_config(struct pwm_chip *chip,
							struct pwm_device *pwm,
							int duty_ns,
							int period_ns)
{
	struct aml_pwm_chip *our_chip = to_aml_pwm_chip(chip);
	struct aml_pwm_channel *our_chan = pwm_get_chip_data(pwm);
	unsigned int id = pwm->hwpwm;
	unsigned int offset;
	int ret;

	if ((~(our_chip->inverter_mask >> id) & 0x1))
		duty_ns = period_ns - duty_ns;

	if (period_ns > NSEC_PER_SEC)
		return -ERANGE;

	if (period_ns == our_chan->period_ns && duty_ns == our_chan->duty_ns)
		return 0;

	offset = id * 2;
	ret = pwm_aml_clk(our_chip, pwm, duty_ns, period_ns, offset);
	if (ret) {
		dev_err(chip->dev, "tried to calc pwm freq err\n");
		return -EINVAL;
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB) && (id > chip->npwm/2-1))
		pwm_meson_config_ext(our_chip, our_chan, id);/*double pwm*/
	else
		pwm_meson_config(our_chip, our_chan, id);/*single pwm*/

	our_chan->period_ns = period_ns;
	our_chan->duty_ns = duty_ns;

	return 0;
}

static void pwm_aml_set_invert(struct pwm_chip *chip, struct pwm_device *pwm,
				unsigned int channel, bool invert)
{
	struct aml_pwm_chip *our_chip = to_aml_pwm_chip(chip);
	unsigned long flags;
	struct aml_pwm_channel *our_chan = pwm_get_chip_data(pwm);

	spin_lock_irqsave(&aml_pwm_lock, flags);
	if (invert)
		our_chip->inverter_mask |= BIT(channel);
	else
		our_chip->inverter_mask &= ~BIT(channel);

	pwm_aml_config(chip, pwm, our_chan->duty_ns,
					our_chan->period_ns);

	spin_unlock_irqrestore(&aml_pwm_lock, flags);
}


static int pwm_aml_set_polarity(struct pwm_chip *chip,
				    struct pwm_device *pwm,
				    enum pwm_polarity polarity)
{
	bool invert = (polarity == PWM_POLARITY_NORMAL);

	/* Inverted means normal in the hardware. */
	pwm_aml_set_invert(chip, pwm, pwm->hwpwm, invert);

	return 0;
}

static const struct pwm_ops pwm_aml_ops = {
	.request	= pwm_aml_request,
	.free		= pwm_aml_free,
	.enable		= pwm_aml_enable,
	.disable	= pwm_aml_disable,
	.config		= pwm_aml_config,
	.set_polarity	= pwm_aml_set_polarity,
	.owner		= THIS_MODULE,
};

#ifdef CONFIG_OF
static const struct of_device_id aml_pwm_matches[] = {
	{ .compatible = "amlogic, meson-pwm", },
	{},
};

static int pwm_aml_parse_addr_m8b(struct aml_pwm_chip *chip)
{
	struct device_node *np = chip->chip.dev->of_node;

	chip->baseaddr.ab_base = of_iomap(np, 0);
	if (IS_ERR(chip->baseaddr.ab_base))
		return PTR_ERR(chip->baseaddr.ab_base);
	chip->baseaddr.cd_base = of_iomap(np, 1);
	if (IS_ERR(chip->baseaddr.cd_base))
		return PTR_ERR(chip->baseaddr.cd_base);
	chip->baseaddr.ef_base = of_iomap(np, 2);
	if (IS_ERR(chip->baseaddr.ef_base))
		return PTR_ERR(chip->baseaddr.ef_base);
	return 0;
}
static int pwm_aml_parse_addr_gxbb(struct aml_pwm_chip *chip)
{
	struct device_node *np = chip->chip.dev->of_node;

	chip->baseaddr.ab_base = of_iomap(np, 0);
	if (IS_ERR(chip->baseaddr.ab_base))
		return PTR_ERR(chip->baseaddr.ab_base);
	chip->baseaddr.cd_base = of_iomap(np, 1);
	if (IS_ERR(chip->baseaddr.cd_base))
		return PTR_ERR(chip->baseaddr.cd_base);
	chip->baseaddr.ef_base = of_iomap(np, 2);
	if (IS_ERR(chip->baseaddr.ef_base))
		return PTR_ERR(chip->baseaddr.ef_base);
	chip->baseaddr.aoab_base = of_iomap(np, 3);
	if (IS_ERR(chip->baseaddr.aoab_base))
		return PTR_ERR(chip->baseaddr.aoab_base);
	return 0;
}
static int pwm_aml_parse_addr_txl(struct aml_pwm_chip *chip)
{
	struct device_node *np = chip->chip.dev->of_node;

	chip->baseaddr.ab_base = of_iomap(np, 0);
	if (IS_ERR(chip->baseaddr.ab_base))
		return PTR_ERR(chip->baseaddr.ab_base);
	chip->baseaddr.cd_base = of_iomap(np, 1);
	if (IS_ERR(chip->baseaddr.cd_base))
		return PTR_ERR(chip->baseaddr.cd_base);
	chip->baseaddr.ef_base = of_iomap(np, 2);
	if (IS_ERR(chip->baseaddr.ef_base))
		return PTR_ERR(chip->baseaddr.ef_base);
	chip->baseaddr.aoab_base = of_iomap(np, 3);
	if (IS_ERR(chip->baseaddr.aoab_base))
		return PTR_ERR(chip->baseaddr.aoab_base);
	/*for txl ao blink register*/
	chip->ao_blink_base = of_iomap(np, 4);
	if (IS_ERR(chip->ao_blink_base))
		return PTR_ERR(chip->ao_blink_base);

	return 0;
}

static int pwm_aml_parse_addr_txlx(struct aml_pwm_chip *chip)
{
	struct device_node *np = chip->chip.dev->of_node;

	chip->baseaddr.ab_base = of_iomap(np, 0);
	if (IS_ERR(chip->baseaddr.ab_base))
		return PTR_ERR(chip->baseaddr.ab_base);

	chip->baseaddr.cd_base = of_iomap(np, 1);
	if (IS_ERR(chip->baseaddr.cd_base))
		return PTR_ERR(chip->baseaddr.cd_base);

	chip->baseaddr.ef_base = of_iomap(np, 2);
	if (IS_ERR(chip->baseaddr.ef_base))
		return PTR_ERR(chip->baseaddr.ef_base);

	chip->baseaddr.aoab_base = of_iomap(np, 3);
	if (IS_ERR(chip->baseaddr.aoab_base))
		return PTR_ERR(chip->baseaddr.aoab_base);

	chip->baseaddr.aocd_base = of_iomap(np, 4);
	if (IS_ERR(chip->baseaddr.aocd_base))
		return PTR_ERR(chip->baseaddr.aocd_base);

	return 0;
}

static int pwm_aml_parse_addr_axg(struct aml_pwm_chip *chip)
{
	struct device_node *np = chip->chip.dev->of_node;

	chip->baseaddr.ab_base = of_iomap(np, 0);
	if (IS_ERR(chip->baseaddr.ab_base))
		return PTR_ERR(chip->baseaddr.ab_base);

	chip->baseaddr.cd_base = of_iomap(np, 1);
	if (IS_ERR(chip->baseaddr.cd_base))
		return PTR_ERR(chip->baseaddr.cd_base);

	chip->baseaddr.aoab_base = of_iomap(np, 2);
	if (IS_ERR(chip->baseaddr.aoab_base))
		return PTR_ERR(chip->baseaddr.aoab_base);

	chip->baseaddr.aocd_base = of_iomap(np, 3);
	if (IS_ERR(chip->baseaddr.aocd_base))
		return PTR_ERR(chip->baseaddr.aocd_base);

	return 0;
}

static int pwm_aml_parse_addr(struct aml_pwm_chip *chip)
{
	unsigned int soc_id = get_cpu_type();

	switch (soc_id) {
	case MESON_CPU_MAJOR_ID_M8B:/*3 group pwms*/
		pwm_aml_parse_addr_m8b(chip);
	break;
	case MESON_CPU_MAJOR_ID_GXBB:
	case MESON_CPU_MAJOR_ID_GXTVBB:
	case MESON_CPU_MAJOR_ID_GXL:
	case MESON_CPU_MAJOR_ID_GXM:/*4 group pwms*/
		pwm_aml_parse_addr_gxbb(chip);
	break;
	case MESON_CPU_MAJOR_ID_TXL:/*5 group pwms,ao blink reg special*/
		pwm_aml_parse_addr_txl(chip);
	break;
	case MESON_CPU_MAJOR_ID_TXLX:/*5 group pwms*/
		pwm_aml_parse_addr_txlx(chip);
	break;
	case MESON_CPU_MAJOR_ID_AXG:/*4 group pwms*/
		pwm_aml_parse_addr_axg(chip);
	break;
	default:
		dev_err(chip->chip.dev, "not support soc\n");
	break;
	}

	return 0;
}

static int pwm_aml_parse_dt(struct aml_pwm_chip *chip)
{
	struct device_node *np = chip->chip.dev->of_node;
	const struct of_device_id *match;
	int i = 0;
	struct property *prop;
	const __be32 *cur;
	int ret;
	u32 output_val;/*pwm channel val*/
	u32 output_co = 0;/*pwm outputs count*/
	u32 clock_val;/*pwm channel's clock select val*/
	u32 clock_co = 0;/*clock outputs count*/
	unsigned int soc_id = get_cpu_type();

	match = of_match_node(aml_pwm_matches, np);
	if (!match)
		return -ENODEV;

	ret = pwm_aml_parse_addr(chip);
	if (ret != 0)
		dev_err(chip->chip.dev,
		"can not parse reg addr\n");

	of_property_for_each_u32(np, "pwm-outputs", prop, cur, output_val) {
		chip->variant.output_mask |= BIT(output_val);
		output_co++;
	}
	of_property_for_each_u32(np, "clock-select", prop, cur, clock_val) {
		chip->clk_mask |= clock_val << (2 * i);
		i++;
		clock_co++;
	}
	chip->chip.npwm = output_co;

	pr_info("output_co = %d ; clock_co = %d\n", output_co, clock_co);
	/*check socs's output num*/
	switch (soc_id) {
	case MESON_CPU_MAJOR_ID_M8B:
		if ((output_co > AML_PWM_M8BB_NUM) ||
			(clock_co > AML_PWM_M8BB_NUM)) {
			goto err;
		}
	break;
	case MESON_CPU_MAJOR_ID_GXBB:
		if ((output_co > AML_PWM_GXBB_NUM) ||
			(clock_co > AML_PWM_GXBB_NUM)) {
			goto err;
		}
	break;
	case MESON_CPU_MAJOR_ID_GXTVBB:
	case MESON_CPU_MAJOR_ID_GXL:
	case MESON_CPU_MAJOR_ID_GXM:
	case MESON_CPU_MAJOR_ID_TXL:
		if ((output_co > AML_PWM_GXTVBB_NUM) ||
			(clock_co > AML_PWM_GXTVBB_NUM)) {
			goto err;
		}
	break;
	case MESON_CPU_MAJOR_ID_TXLX:
		if ((output_co > AML_PWM_TXLX_NUM) ||
			(clock_co > AML_PWM_TXLX_NUM)) {
			goto err;
		}
	case MESON_CPU_MAJOR_ID_AXG:
		if ((output_co > AML_PWM_AXG_NUM) ||
			(clock_co > AML_PWM_AXG_NUM)) {
			goto err;
		}
	break;
	default:
		dev_err(chip->chip.dev, "%s not support\n", __func__);
	break;
	}

	chip->xtal_clk = clk_get(chip->chip.dev, "xtal");
	chip->vid_pll_clk = clk_get(chip->chip.dev, "vid_pll_clk");
	chip->fclk_div4_clk = clk_get(chip->chip.dev, "fclk_div4");
	chip->fclk_div3_clk = clk_get(chip->chip.dev, "fclk_div3");

	return 0;

err:
	dev_err(chip->chip.dev,
	"%s: invalid channel index in pwm-outputs or clock-select property\n",
	__func__);
	return -ENODEV;

}
#else
static int pwm_aml_parse_dt(struct aml_pwm_chip *chip)
{
	return -ENODEV;
}
#endif

static int pwm_aml_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aml_pwm_chip *chip;
	int ret;
	int ret_fs;
	unsigned int soc_id = get_cpu_type();

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	chip->chip.dev = &pdev->dev;
	chip->chip.ops = &pwm_aml_ops;
	chip->chip.base = -1;
	/*add for four new node*/
	chip->variant.constant = 0;
	chip->variant.blink_enable = 0;
	chip->variant.blink_times = 0;
	chip->variant.times = 0;

	if (IS_ENABLED(CONFIG_OF) && pdev->dev.of_node) {
		ret = pwm_aml_parse_dt(chip);
		if (ret)
			return ret;
	} else {
		if (!pdev->dev.platform_data) {
			dev_err(&pdev->dev, "no platform data specified\n");
			return -EINVAL;
		}
		memcpy(&chip->variant, pdev->dev.platform_data,
							sizeof(chip->variant));
	}
	pr_info("npwm= %d\n", chip->chip.npwm);
	switch (soc_id) {
	case MESON_CPU_MAJOR_ID_M8B:
	case MESON_CPU_MAJOR_ID_GXBB:
		chip->inverter_mask = BIT(chip->chip.npwm) - 1;
		break;
	case MESON_CPU_MAJOR_ID_GXTVBB:
	case MESON_CPU_MAJOR_ID_GXL:
	case MESON_CPU_MAJOR_ID_GXM:
	case MESON_CPU_MAJOR_ID_TXL:
	case MESON_CPU_MAJOR_ID_TXLX:
	case MESON_CPU_MAJOR_ID_AXG:
		chip->inverter_mask = BIT(chip->chip.npwm/2) - 1;
		break;
	default:
		dev_err(dev, "%s not support\n", __func__);
		break;
	}

	ret = pwmchip_add(&chip->chip);
	if (ret < 0) {
		dev_err(dev, "failed to register PWM chip\n");
		return ret;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		ret_fs = meson_pwm_sysfs_init(dev);
		if (ret_fs) {
			dev_err(dev, "pwm sysfs group creation failed\n");
			return ret_fs;
		}
	}
	platform_set_drvdata(pdev, chip);
	pr_info("probe ok\n");

	return 0;
}

static int pwm_aml_remove(struct platform_device *pdev)
{
	struct aml_pwm_chip *chip = platform_get_drvdata(pdev);
	int ret;

	ret = pwmchip_remove(&chip->chip);
	if (ret < 0)
		return ret;
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int pwm_aml_suspend(struct device *dev)
{
	struct aml_pwm_chip *chip = dev_get_drvdata(dev);
	unsigned int i;
	unsigned int num = chip->chip.npwm;

	/*
	 * No one preserves these values during suspend so reset them.
	 * Otherwise driver leaves PWM unconfigured if same values are
	 * passed to pwm_config() next time.
	 */
	for (i = 0; i < num; ++i) {
		struct pwm_device *pwm = &chip->chip.pwms[i];
		struct aml_pwm_channel *chan = pwm_get_chip_data(pwm);

		if (!chan)
			continue;

		chan->period_ns = 0;
		chan->duty_ns = 0;
	}

	return 0;
}

static int pwm_aml_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops pwm_aml_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pwm_aml_suspend, pwm_aml_resume)
};

static struct platform_driver pwm_aml_driver = {
	.driver		= {
		.name	= "meson-pwm",
		.owner	= THIS_MODULE,
		.pm	= &pwm_aml_pm_ops,
		.of_match_table = of_match_ptr(aml_pwm_matches),
	},
	.probe		= pwm_aml_probe,
	.remove		= pwm_aml_remove,
};
/*
 *need to register before wifi_dt driver
 */
static int __init aml_pwm_init(void)
{
	int ret;

	ret = platform_driver_register(&pwm_aml_driver);
	return ret;
}
static void __exit aml_pwm_exit(void)
{
	platform_driver_unregister(&pwm_aml_driver);
}
fs_initcall_sync(aml_pwm_init);
module_exit(aml_pwm_exit);

MODULE_ALIAS("platform:meson-pwm");
MODULE_LICENSE("GPL");
