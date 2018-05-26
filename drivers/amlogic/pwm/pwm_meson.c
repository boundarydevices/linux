/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2014 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * BSD LICENSE
 *
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2014 Amlogic, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#undef pr_fmt
#define pr_fmt(fmt) "pwm: " fmt

#include <linux/amlogic/pwm_meson.h>
#include <linux/of_device.h>

struct meson_pwm_channel {
	unsigned int hi;
	unsigned int lo;
	u8 pre_div;

	struct pwm_state state;

	struct clk *clk_parent;
	struct clk_mux mux;
	struct clk *clk;
};

struct meson_pwm *to_meson_pwm(struct pwm_chip *chip)
{
	return container_of(chip, struct meson_pwm, chip);
}

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

static int meson_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct meson_pwm_channel *channel = pwm_get_chip_data(pwm);
	struct device *dev = chip->dev;
	int err;

	if (!channel)
		return -ENODEV;

	if (channel->clk_parent) {
		err = clk_set_parent(channel->clk, channel->clk_parent);
		if (err < 0) {
			dev_err(dev, "failed to set parent %s for %s: %d\n",
				__clk_get_name(channel->clk_parent),
				__clk_get_name(channel->clk), err);
				return err;
		}
	}

	err = clk_prepare_enable(channel->clk);
	if (err < 0) {
		dev_err(dev, "failed to enable clock %s: %d\n",
			__clk_get_name(channel->clk), err);
		return err;
	}

	chip->ops->get_state(chip, pwm, &channel->state);

	return 0;
}

static void meson_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct meson_pwm_channel *channel = pwm_get_chip_data(pwm);

	if (channel)
		clk_disable_unprepare(channel->clk);
}

static int meson_pwm_calc(struct meson_pwm *meson,
			  struct meson_pwm_channel *channel, unsigned int id,
			  unsigned int duty, unsigned int period)
{
	unsigned int pre_div, cnt, duty_cnt;
	unsigned long fin_freq = -1;
	u64 fin_ps;

	if (~(meson->inverter_mask >> id) & 0x1)
		duty = period - duty;

	if (period == channel->state.period &&
	    duty == channel->state.duty_cycle)
		return 0;

	fin_freq = clk_get_rate(channel->clk);
	if (fin_freq == 0) {
		dev_err(meson->chip.dev, "invalid source clock frequency\n");
		return -EINVAL;
	}

	dev_dbg(meson->chip.dev, "fin_freq: %lu Hz\n", fin_freq);
	fin_ps = (u64)NSEC_PER_SEC * 1000;
	do_div(fin_ps, fin_freq);

	/* Calc pre_div with the period */
	for (pre_div = 0; pre_div < MISC_CLK_DIV_MASK; pre_div++) {
		cnt = DIV_ROUND_CLOSEST_ULL((u64)period * 1000,
				fin_ps * (pre_div + 1));
		dev_dbg(meson->chip.dev, "fin_ns=%llu pre_div=%u cnt=%u\n",
			fin_ps, pre_div, cnt);
		if (cnt <= 0xffff)
			break;
	}

	if (pre_div == MISC_CLK_DIV_MASK) {
		dev_err(meson->chip.dev, "unable to get period pre_div\n");
		return -EINVAL;
	}

	dev_dbg(meson->chip.dev, "period=%u pre_div=%u cnt=%u\n", period,
		pre_div, cnt);

	if (duty == period) {
		channel->pre_div = pre_div;
		channel->hi = cnt;
		channel->lo = 0;
	} else if (duty == 0) {
		channel->pre_div = pre_div;
		channel->hi = 0;
		channel->lo = cnt;
	} else {
		/* Then check is we can have the duty with the same pre_div */
		duty_cnt = DIV_ROUND_CLOSEST_ULL((u64)duty * 1000,
					fin_ps * (pre_div + 1));
		if (duty_cnt > 0xffff) {
			dev_err(meson->chip.dev, "unable to get duty cycle\n");
			return -EINVAL;
		}

		dev_dbg(meson->chip.dev, "duty=%u pre_div=%u duty_cnt=%u\n",
			duty, pre_div, duty_cnt);

		channel->pre_div = pre_div;
		channel->hi = duty_cnt - 1;
		channel->lo = cnt - duty_cnt - 1;
	}
	/*
	 * duty_cycle equal 0% and 100%,constant should be enabled,
	 * high and low count will not incease one;
	 * otherwise, high and low count increase one.
	 */
	if (duty == period || duty == 0)
		pwm_constant_enable(meson, id);
	else
		pwm_constant_disable(meson, id);


	return 0;
}

/**
 * pwm_aml_enable()
 *	- enable pwm output
 * @chip: aml_pwm_chip struct
 * @index: pwm channel to choose,like PWM_A or PWM_B
 * PWM_A2 PWM_B2
 */
static void meson_pwm_enable(struct meson_pwm *meson,
			     struct meson_pwm_channel *channel,
			     unsigned int id)
{
	u32 value, clk_shift, clk_enable, enable;
	unsigned int offset;

	switch (id) {
	case MESON_PWM_0:
		clk_shift = MISC_A_CLK_DIV_SHIFT;
		clk_enable = MISC_A_CLK_EN;
		enable = MISC_A_EN;
		offset = REG_PWM_A;
		break;

	case MESON_PWM_1:
		clk_shift = MISC_B_CLK_DIV_SHIFT;
		clk_enable = MISC_B_CLK_EN;
		enable = MISC_B_EN;
		offset = REG_PWM_B;
		break;

	case MESON_PWM_2:
		clk_shift = MISC_A_CLK_DIV_SHIFT;
		clk_enable = MISC_A_CLK_EN;
		enable = MISC_A2_EN;
		offset = REG_PWM_A2;
		break;

	case MESON_PWM_3:
		clk_shift = MISC_B_CLK_DIV_SHIFT;
		clk_enable = MISC_B_CLK_EN;
		enable = MISC_B2_EN;
		offset = REG_PWM_B2;
		break;

	default:
		return;
	}


	value = readl(meson->base + REG_MISC_AB);
	value &= ~(MISC_CLK_DIV_MASK << clk_shift);
	value |= channel->pre_div << clk_shift;
	value |= clk_enable;
	writel(value, meson->base + REG_MISC_AB);

	value = (channel->hi << PWM_HIGH_SHIFT) | channel->lo;
	writel(value, meson->base + offset);

	value = readl(meson->base + REG_MISC_AB);
	value |= enable;
	writel(value, meson->base + REG_MISC_AB);
}

static void meson_pwm_disable(struct meson_pwm *meson, unsigned int id)
{
	u32 value, enable;

	switch (id) {
	case MESON_PWM_0:
		enable = MISC_A_EN;
		break;

	case MESON_PWM_1:
		enable = MISC_B_EN;
		break;

	case MESON_PWM_2:
		enable = MISC_A2_EN;
		break;

	case MESON_PWM_3:
		enable = MISC_B2_EN;
		break;

	default:
		return;
	}

	value = readl(meson->base + REG_MISC_AB);
	value &= ~enable;
	writel(value, meson->base + REG_MISC_AB);
}

static int meson_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			   struct pwm_state *state)
{
	struct meson_pwm_channel *channel = pwm_get_chip_data(pwm);
	struct meson_pwm *meson = to_meson_pwm(chip);
	int err = 0;

	if (!state)
		return -EINVAL;

	mutex_lock(&meson->lock);

	if (!state->enabled) {
		meson_pwm_disable(meson, pwm->hwpwm);
		channel->state.enabled = false;

		goto unlock;
	}

	if (state->period != channel->state.period ||
	    state->duty_cycle != channel->state.duty_cycle ||
	    state->polarity != channel->state.polarity) {
		if (channel->state.enabled) {
			meson_pwm_disable(meson, pwm->hwpwm);
			channel->state.enabled = false;
		}

		if (state->polarity != channel->state.polarity) {
			if (state->polarity == PWM_POLARITY_NORMAL)
				meson->inverter_mask |= BIT(pwm->hwpwm);
			else
				meson->inverter_mask &= ~BIT(pwm->hwpwm);
		}

		err = meson_pwm_calc(meson, channel, pwm->hwpwm,
				     state->duty_cycle, state->period);
		if (err < 0)
			goto unlock;

		channel->state.polarity = state->polarity;
		channel->state.period = state->period;
		channel->state.duty_cycle = state->duty_cycle;
	}

	if (state->enabled && !channel->state.enabled) {
		meson_pwm_enable(meson, channel, pwm->hwpwm);
		channel->state.enabled = true;
	}

unlock:
	mutex_unlock(&meson->lock);
	return err;

}

static void meson_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
				struct pwm_state *state)
{
	struct meson_pwm *meson = to_meson_pwm(chip);
	u32 value, mask;

	if (!state)
		return;

	switch (pwm->hwpwm) {
	case MESON_PWM_0:
		mask = MISC_A_EN;
		break;

	case MESON_PWM_1:
		mask = MISC_B_EN;
		break;

	case MESON_PWM_2:
		mask = MISC_A2_EN;
		break;

	case MESON_PWM_3:
		mask = MISC_B2_EN;

	default:
		return;
	}

	value = readl(meson->base + REG_MISC_AB);
	state->enabled = (value & mask) != 0;
}

static const struct pwm_ops meson_pwm_ops = {
	.request = meson_pwm_request,
	.free = meson_pwm_free,
	.apply = meson_pwm_apply,
	.get_state = meson_pwm_get_state,
	.owner = THIS_MODULE,
};

static const char * const pwm_g12a_ee_parent_names[] = {
	"xtal", "vid_pll", "fclk_div4", "fclk_div3"
};

static const char * const pwm_g12a_ao_parent_names[] = {
	"xtal", "clk81", "fclk_div4", "fclk_div5"
};

static const char * const pwm_txlx_ee_parent_names[] = {
	"xtal", "vid_pll", "fclk_div4", "fclk_div3"
};

static const char * const pwm_txlx_ao_parent_names[] = {
	"clk81", "xtal", "null", "null"
};

static const char * const pwm_axg_ee_parent_names[] = {
	"xtal", "fclk_div5", "fclk_div4", "fclk_div3"
};

static const char * const pwm_axg_ao_parent_names[] = {
	"xtal", "aoclk81", "fclk_div4", "fclk_div5"
};

static const char * const pwm_meson8b_parent_names[] = {
	"xtal", "vid_pll", "fclk_div4", "fclk_div3"
};

static const char * const pwm_gxbb_parent_names[] = {
	"xtal", "hdmi_pll", "fclk_div4", "fclk_div3"
};

static const char * const pwm_gx_ee_parent_names[] = {
	"xtal", "vid_pll", "fclk_div4", "fclk_div3"
};

static const char * const pwm_gx_ao_parent_names[] = {
	"clk81", "xtal", "null", "null"
};

static const struct meson_pwm_data pwm_g12a_ee_data = {
	.double_channel = true,
	.parent_names = pwm_g12a_ee_parent_names,
};

static const struct meson_pwm_data pwm_g12a_ao_data = {
	.double_channel = true,
	.parent_names = pwm_g12a_ao_parent_names,
};

static const struct meson_pwm_data pwm_txlx_ee_data = {
	.double_channel = true,
	.parent_names = pwm_txlx_ee_parent_names,
};

static const struct meson_pwm_data pwm_txlx_ao_data = {
	.double_channel = true,
	.parent_names = pwm_txlx_ao_parent_names,
};

static const struct meson_pwm_data pwm_axg_ee_data = {
	.double_channel = true,
	.parent_names = pwm_axg_ee_parent_names,
};

static const struct meson_pwm_data pwm_axg_ao_data = {
	.double_channel = true,
	.parent_names = pwm_axg_ao_parent_names,
};

static const struct meson_pwm_data pwm_gx_ee_data = {
	.double_channel = true,
	.parent_names = pwm_gx_ee_parent_names,
};

static const struct meson_pwm_data pwm_gx_ao_data = {
	.double_channel = true,
	.parent_names = pwm_gx_ao_parent_names,
};

static const struct meson_pwm_data pwm_gxbb_data = {
	.double_channel = false,
	.parent_names = pwm_gxbb_parent_names,
};

static const struct meson_pwm_data pwm_m8b_data = {
	.double_channel = false,
	.parent_names = pwm_meson8b_parent_names,
};

static const struct of_device_id meson_pwm_matches[] = {
	{ .compatible = "amlogic,g12a-ee-pwm", .data = &pwm_g12a_ee_data },
	{ .compatible = "amlogic,g12a-ao-pwm", .data = &pwm_g12a_ao_data },
	{ .compatible = "amlogic,g12b-ee-pwm", .data = &pwm_g12a_ee_data },
	{ .compatible = "amlogic,g12b-ao-pwm", .data = &pwm_g12a_ao_data },
	{ .compatible = "amlogic,txlx-ee-pwm", .data = &pwm_txlx_ee_data },
	{ .compatible = "amlogic,txlx-ao-pwm", .data = &pwm_txlx_ao_data },
	{ .compatible = "amlogic,axg-ee-pwm", .data = &pwm_axg_ee_data },
	{ .compatible = "amlogic,axg-ao-pwm", .data = &pwm_axg_ao_data },
	/*for gxl gxtvbb gxm*/
	{ .compatible = "amlogic,gx-ee-pwm", .data = &pwm_gx_ee_data },
	{ .compatible = "amlogic,gx-ao-pwm", .data = &pwm_gx_ao_data },

	{ .compatible = "amlogic,meson-gxbb-pwm", .data = &pwm_gxbb_data },
	{ .compatible = "amlogic,meson8b-pwm", .data = &pwm_m8b_data },
	{},
};
MODULE_DEVICE_TABLE(of, meson_pwm_matches);

static int meson_pwm_init_channels(struct meson_pwm *meson,
				   struct meson_pwm_channel *channels)
{
	struct device *dev = meson->chip.dev;
	struct device_node *np = dev->of_node;
	struct clk_init_data init;
	unsigned int i;
	char name[255];
	int err;

	for (i = 0; i < meson->chip.npwm; i++) {
		struct meson_pwm_channel *channel = &channels[i];

		snprintf(name, sizeof(name), "%s#mux%u", np->full_name, i);
		init.name = name;
		init.ops = &clk_mux_ops;
		init.flags = CLK_IS_BASIC;
		init.parent_names = meson->data->parent_names;
		init.num_parents = 1 << MISC_CLK_SEL_WIDTH;

		channel->mux.reg = meson->base + REG_MISC_AB;
		channel->mux.shift = mux_reg_shifts[i];
		channel->mux.mask = BIT(MISC_CLK_SEL_WIDTH) - 1;
		channel->mux.flags = 0;
		channel->mux.lock = &meson->pwm_lock;
		channel->mux.table = NULL;
		channel->mux.hw.init = &init;

		channel->clk = devm_clk_register(dev, &channel->mux.hw);
		if (IS_ERR(channel->clk)) {
			err = PTR_ERR(channel->clk);
			dev_err(dev, "failed to register %s: %d\n", name, err);
			return err;
		}

		snprintf(name, sizeof(name), "clkin%u", i);

		channel->clk_parent = devm_clk_get(dev, name);
		if (IS_ERR(channel->clk_parent)) {
			err = PTR_ERR(channel->clk_parent);
			if (err == -EPROBE_DEFER)
				return err;

			channel->clk_parent = NULL;
		}
	}

	return 0;
}

static void meson_pwm_add_channels(struct meson_pwm *meson,
				   struct meson_pwm_channel *channels)
{
	unsigned int i;

	for (i = 0; i < meson->chip.npwm; i++)
		pwm_set_chip_data(&meson->chip.pwms[i], &channels[i]);
}

static int meson_pwm_probe(struct platform_device *pdev)
{
	struct meson_pwm_channel *channels;
	struct meson_pwm *meson;
	struct resource *regs;
	int err;

	meson = devm_kzalloc(&pdev->dev, sizeof(*meson), GFP_KERNEL);
	if (!meson)
		return -ENOMEM;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	meson->base = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(meson->base))
		return PTR_ERR(meson->base);

	mutex_init(&meson->lock);
	spin_lock_init(&meson->pwm_lock);
	meson->chip.dev = &pdev->dev;
	meson->chip.ops = &meson_pwm_ops;
	meson->chip.base = -1;
	meson->chip.of_xlate = of_pwm_xlate_with_flags;
	meson->chip.of_pwm_n_cells = 3;
	meson->data = (struct meson_pwm_data *)
		of_device_get_match_data(&pdev->dev);
	if (meson->data->double_channel)
		meson->chip.npwm = 4;
	else
		meson->chip.npwm = 2;
	meson->inverter_mask = BIT(meson->chip.npwm) - 1;

	channels = devm_kcalloc(&pdev->dev, meson->chip.npwm, sizeof(*channels),
				GFP_KERNEL);
	if (!channels)
		return -ENOMEM;

	err = meson_pwm_init_channels(meson, channels);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to init PWM channels: %d\n", err);
		return err;
	}
	err = pwmchip_add(&meson->chip);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to register PWM chip: %d\n", err);
		return err;
	}

	/*for constant,blinks functions*/
	if (meson->data->double_channel)
		meson_pwm_sysfs_init(&pdev->dev);

	meson_pwm_add_channels(meson, channels);

	platform_set_drvdata(pdev, meson);

	return 0;
}

static int meson_pwm_remove(struct platform_device *pdev)
{
	struct meson_pwm *meson = platform_get_drvdata(pdev);

	if (meson->data->double_channel)
		meson_pwm_sysfs_exit(&pdev->dev);

	return pwmchip_remove(&meson->chip);
}

static struct platform_driver meson_pwm_driver = {
	.driver		= {
		.name	= "meson-pwm",
		.owner	= THIS_MODULE,
		.of_match_table = meson_pwm_matches,
	},
	.probe = meson_pwm_probe,
	.remove = meson_pwm_remove,
};
/*
 *need to register before wifi_dt driver
 */
static int __init meson_pwm_init(void)
{
	int ret;

	ret = platform_driver_register(&meson_pwm_driver);
	return ret;
}
static void __exit meson_pwm_exit(void)
{
	platform_driver_unregister(&meson_pwm_driver);
}
fs_initcall_sync(meson_pwm_init);
module_exit(meson_pwm_exit);

MODULE_ALIAS("platform:meson-pwm");
MODULE_DESCRIPTION("Amlogic Meson PWM Generator driver");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_LICENSE("Dual BSD/GPL");
