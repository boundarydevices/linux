// SPDX-License-Identifier: GPL-2.0-only
/*
 *  sn65dsi83.c - DVI output chip
 *
 *  Copyright (C) 2019 Boundary Devices. All Rights Reserved.
 */

#include <drm/drm_mode.h>
#include <linux/module.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <video/display_timing.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include "panel-sn65dsi83.h"

static const char *client_name = "sn65dsi83";

static const unsigned char registers_to_show[] = {
	SN_SOFT_RESET,	SN_CLK_SRC, SN_CLK_DIV, SN_PLL_EN,
	SN_DSI_LANES, SN_DSI_EQ, SN_DSI_CLK, SN_FORMAT,
	SN_LVDS_VOLTAGE, SN_LVDS_TERM, SN_LVDS_CM_VOLTAGE,
	0, SN_HACTIVE_LOW,
	0, SN_VACTIVE_LOW,
	0, SN_SYNC_DELAY_LOW,
	0, SN_HSYNC_LOW,
	0, SN_VSYNC_LOW,
	SN_HBP, SN_VBP,
	SN_HFP, SN_VFP,
	SN_TEST_PATTERN,
	SN_IRQ_EN, SN_IRQ_MASK, SN_IRQ_STAT,
};


/*
 * sn_i2c_read_reg - read data from a register of the i2c slave device.
 */
static int sn_i2c_read_byte(struct panel_sn65dsi83 *sn, u8 reg)
{
	u8 *buf = sn->tx_buf;
	struct i2c_msg msgs[2];
	int ret;

	buf[0] = reg;
	msgs[0].addr = sn->i2c_address;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = buf;

	msgs[1].addr = sn->i2c_address;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = sn->rx_buf;
	ret = i2c_transfer(sn->i2c, msgs, 2);
	if (ret < 0)
		dev_err(sn->dev, "%s failed(%i)\n", __func__, ret);
	return ret < 0 ? ret : sn->rx_buf[0];
}

/**
 * sn_i2c_write - write data to a register of the i2c slave device.
 *
 * @reg: the register to write to.
 * @buf: raw data buffer to write.
 * @len: length of the buffer to write
 */
static int sn_i2c_write_byte(struct panel_sn65dsi83 *sn, u8 reg, u8 val)
{
	u8 *buf = sn->tx_buf;
	struct i2c_msg msg;
	int ret;

	buf[0] = reg;
	buf[1] = val;

	msg.addr = sn->i2c_address;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;
	ret = i2c_transfer(sn->i2c, &msg, 1);
	if (ret < 0) {
		msleep(10);
		ret = i2c_transfer(sn->i2c, &msg, 1);
	}
	if (ret < 0)
		dev_err(sn->dev, "%s failed(%i)\n", __func__, ret);
	return ret < 0 ? ret : 0;
}

static void sn_disable_gp(struct panel_sn65dsi83 *sn)
{
	struct gpio_desc *gp_en;
	int i;

	sn->chip_enabled = 0;
	for (i = 0; i < ARRAY_SIZE(sn->gp_en); i++) {
		gp_en = sn->gp_en[i];
		if (!gp_en)
			break;
		gpiod_set_value(gp_en, 0);
	}
}

static void sn_disable(struct panel_sn65dsi83 *sn, int skip_irq)
{
	if (sn->irq_enabled && !skip_irq) {
		sn->irq_enabled = 0;
		disable_irq(sn->irq);
		sn->int_cnt = 0;
	}
	sn_disable_gp(sn);
}

static void sn_enable_gp(struct panel_sn65dsi83 *sn)
{
	int i;

	msleep(15);	/* disabled for at least 10 ms */

	for (i = 0; i < ARRAY_SIZE(sn->gp_en); i++) {
		if (!sn->gp_en[i])
			break;
		gpiod_set_value(sn->gp_en[i], 1);
	}
	msleep(12);	/* enabled for at least 10 ms before i2c writes */
}

static void sn_enable_irq(struct panel_sn65dsi83 *sn)
{
	sn_i2c_write_byte(sn, SN_IRQ_STAT, 0xff);
	sn_i2c_write_byte(sn, SN_IRQ_MASK, 0x7f);
	sn_i2c_write_byte(sn, SN_IRQ_EN, 1);
}

static int sn_get_dsi_clk_divider(struct panel_sn65dsi83 *sn)
{
	u32 dsi_clk_divider = 25;
	u32 mipi_clk_rate = 0;
	u8 mipi_clk_index;
	int ret;
	u32 pixelclock = sn->vm.pixelclock;

	if (sn->mipi_clk)
		mipi_clk_rate = clk_get_rate(sn->mipi_clk);
	if (!mipi_clk_rate) {
		pr_err("mipi clock is off\n");
		/* Divided by 2 because mipi output clock is DDR */
		mipi_clk_rate = pixelclock * sn->dsi_bpp / (sn->dsi_lanes * 2);
	}
	if (mipi_clk_rate > 500000000) {
		pr_err("mipi clock(%d) is too high\n", mipi_clk_rate);
		mipi_clk_rate = 500000000;
	}
	if (pixelclock)
		dsi_clk_divider = (mipi_clk_rate +  (pixelclock >> 1)) / pixelclock;
	if (sn->split_mode)
		dsi_clk_divider <<= 1;	/* double the divisor for split mode */

	if (dsi_clk_divider > 25)
		dsi_clk_divider = 25;
	else if (!dsi_clk_divider)
		dsi_clk_divider = 1;
	mipi_clk_index = mipi_clk_rate / 5000000;
	if (mipi_clk_index < 8)
		mipi_clk_index = 8;
	ret = (sn->dsi_clk_divider == dsi_clk_divider) &&
		(sn->mipi_clk_index == mipi_clk_index);
	if (!ret) {
		dev_info(sn->dev,
			"dsi_clk_divider = %d, mipi_clk_index=%d, mipi_clk_rate=%d\n",
			dsi_clk_divider, mipi_clk_index, mipi_clk_rate);
		sn->dsi_clk_divider = dsi_clk_divider;
		sn->mipi_clk_index = mipi_clk_index;
	}
	return ret;
}

static int sn_check_videomode_change(struct panel_sn65dsi83 *sn)
{
	struct videomode vm;
	u32 pixelclock;
	int ret;

	ret = of_get_videomode(sn->disp_dsi, &vm, 0);
	if (ret < 0)
		return ret;

	if (!IS_ERR(sn->pixel_clk)) {
		pixelclock = clk_get_rate(sn->pixel_clk);
		if (pixelclock)
			vm.pixelclock = pixelclock;
	}

	if (sn->vm.pixelclock != vm.pixelclock ||
	    sn->vm.hactive != vm.hactive ||
	    sn->vm.hsync_len != vm.hsync_len ||
	    sn->vm.hback_porch != vm.hback_porch ||
	    sn->vm.hfront_porch != vm.hfront_porch ||
	    sn->vm.vactive != vm.vactive ||
	    sn->vm.vsync_len != vm.vsync_len ||
	    sn->vm.vback_porch != vm.vback_porch ||
	    sn->vm.vfront_porch != vm.vfront_porch ||
	    ((sn->vm.flags ^ vm.flags) & (DISPLAY_FLAGS_DE_LOW |
			    DISPLAY_FLAGS_HSYNC_HIGH |DISPLAY_FLAGS_VSYNC_HIGH))) {
		dev_info(sn->dev,
			"%s: pixelclock=%ld %dx%d, margins=%d,%d %d,%d  syncs=%d %d flags=%x\n",
			__func__, vm.pixelclock, vm.hactive, vm.vactive,
			vm.hback_porch, vm.hfront_porch,
			vm.vback_porch, vm.vfront_porch,
			vm.hsync_len, vm.vsync_len, vm.flags);
		sn->vm = vm;
		ret = 1;
	}
	if (!sn_get_dsi_clk_divider(sn))
		ret |= 1;
	return ret;
}

static int sn_setup_regs(struct panel_sn65dsi83 *sn)
{
	unsigned i = 5;
	int format = 0x10;
	u32 pixelclock;
	int hbp, hfp, hsa;

	pixelclock = sn->vm.pixelclock;
	if (sn->split_mode)
		pixelclock >>= 1;
	if (pixelclock) {
		if (pixelclock > 37500000) {
			i = (pixelclock - 12500000) / 25000000;
			if (i > 5)
				i = 5;
		}
	}
	sn_i2c_write_byte(sn, SN_CLK_SRC, (i << 1) | 1);
	sn_i2c_write_byte(sn, SN_CLK_DIV, (sn->dsi_clk_divider - 1) << 3);

	sn_i2c_write_byte(sn, SN_DSI_LANES, ((4 - sn->dsi_lanes) << 3) | 0x20);
	sn_i2c_write_byte(sn, SN_DSI_EQ, 0);
	sn_i2c_write_byte(sn, SN_DSI_CLK, sn->mipi_clk_index);
	if (sn->vm.flags & DISPLAY_FLAGS_DE_LOW)
		format |= BIT(7);
	if (!(sn->vm.flags & DISPLAY_FLAGS_HSYNC_HIGH))
		format |= BIT(6);
	if (!(sn->vm.flags & DISPLAY_FLAGS_VSYNC_HIGH))
		format |= BIT(5);
	if (sn->dsi_bpp == 24) {
		if (sn->spwg) {
			/* lvds lane 3 has MSBs of color */
			format |= (sn->split_mode) ? BIT(3) | BIT(2) : BIT(3);
		} else if (sn->jeida) {
			/* lvds lane 3 has LSBs of color */
			format |= (sn->split_mode) ?
					BIT(3) | BIT(2) | BIT(1) | BIT(0) :
					BIT(3) | BIT(1);
		} else {
			/* unused lvds lane 3 has LSBs of color */
			format |= BIT(1);
		}
	}
	if (sn->split_mode)
		format &= ~BIT(4);
	sn_i2c_write_byte(sn, SN_FORMAT, format);

	sn_i2c_write_byte(sn, SN_LVDS_VOLTAGE, 5);
	sn_i2c_write_byte(sn, SN_LVDS_TERM, 3);
	sn_i2c_write_byte(sn, SN_LVDS_CM_VOLTAGE, 0);
	sn_i2c_write_byte(sn, SN_HACTIVE_LOW, (u8)sn->vm.hactive);
	sn_i2c_write_byte(sn, SN_HACTIVE_HIGH, (u8)(sn->vm.hactive >> 8));
	sn_i2c_write_byte(sn, SN_VACTIVE_LOW, (u8)sn->vm.vactive);
	sn_i2c_write_byte(sn, SN_VACTIVE_HIGH, (u8)(sn->vm.vactive >> 8));
	sn_i2c_write_byte(sn, SN_SYNC_DELAY_LOW, (u8)sn->sync_delay);
	sn_i2c_write_byte(sn, SN_SYNC_DELAY_HIGH, (u8)(sn->sync_delay >> 8));
	hsa = sn->vm.hsync_len;
	hbp = sn->vm.hback_porch;
	hfp = sn->vm.hfront_porch;
	if (sn->hbp) {
		int diff = sn->hbp - hbp;

		hbp += diff;
		hfp -= diff;
		if (hfp < 1) {
			diff = 1 - hfp;
			hfp += diff;
			hsa -= diff;
			if (hsa < 1) {
				diff = 1 - hsa;
				hsa += diff;
				hbp -= diff;
			}
		}
	}
	sn_i2c_write_byte(sn, SN_HSYNC_LOW, (u8)hsa);
	sn_i2c_write_byte(sn, SN_HSYNC_HIGH, (u8)(hsa >> 8));
	sn_i2c_write_byte(sn, SN_VSYNC_LOW, (u8)sn->vm.vsync_len);
	sn_i2c_write_byte(sn, SN_VSYNC_HIGH, (u8)(sn->vm.vsync_len >> 8));
	sn_i2c_write_byte(sn, SN_HBP, (u8)hbp);
	sn_i2c_write_byte(sn, SN_VBP, (u8)sn->vm.vback_porch);
	sn_i2c_write_byte(sn, SN_HFP, (u8)hfp);
	sn_i2c_write_byte(sn, SN_VFP, (u8)sn->vm.vfront_porch);
	sn_i2c_write_byte(sn, SN_TEST_PATTERN, 0);
	return 0;
}

static void sn_enable_pll(struct panel_sn65dsi83 *sn)
{
	struct device *dev = sn->dev;

	if (!sn_get_dsi_clk_divider(sn)) {
		sn_i2c_write_byte(sn, SN_CLK_DIV, (sn->dsi_clk_divider - 1) << 3);
		sn_i2c_write_byte(sn, SN_DSI_CLK, sn->mipi_clk_index);
	}
	sn_i2c_write_byte(sn, SN_PLL_EN, 1);
	msleep(12);
	sn_i2c_write_byte(sn, SN_SOFT_RESET, 1);
	msleep(12);
	dev_dbg(dev, "%s:reg0a=%02x\n", __func__, sn_i2c_read_byte(sn, SN_CLK_SRC));
}

static void sn_disable_pll(struct panel_sn65dsi83 *sn)
{
	if (sn->chip_enabled)
		sn_i2c_write_byte(sn, SN_PLL_EN, 0);
}

static void sn_init(struct panel_sn65dsi83 *sn)
{
	sn_i2c_write_byte(sn, SN_SOFT_RESET, 1);
	sn_i2c_write_byte(sn, SN_PLL_EN, 0);
	sn_i2c_write_byte(sn, SN_IRQ_MASK, 0x0);
	sn_i2c_write_byte(sn, SN_IRQ_EN, 1);
}

static void sn_standby(struct panel_sn65dsi83 *sn)
{
	if (sn->state < SN_STATE_STANDBY) {
		sn_enable_gp(sn);
		sn_init(sn);
		if (!sn->irq_enabled) {
			sn->irq_enabled = 1;
			enable_irq(sn->irq);
		}
		sn->chip_enabled = 1;
		sn_setup_regs(sn);
		sn->state = SN_STATE_STANDBY;
	}
}

static void sn_prepare(struct panel_sn65dsi83 *sn)
{
	if (sn->state < SN_STATE_STANDBY)
		sn_standby(sn);
	if (sn->state < SN_STATE_ON) {
		msleep(2);
		sn_enable_pll(sn);
		sn->state = SN_STATE_ON;
	}
}

static void sn_powerdown1(struct panel_sn65dsi83 *sn, int skip_irq)
{
	dev_dbg(sn->dev, "%s\n", __func__);
	if (sn->state) {
		mutex_lock(&sn->power_mutex);
		sn_disable_pll(sn);
		sn_disable(sn, skip_irq);
		sn->state = SN_STATE_OFF;
		mutex_unlock(&sn->power_mutex);
	}
}

void sn65_disable(struct panel_sn65dsi83 *sn)
{
	if (!sn->dev)
		return;
	sn_powerdown1(sn, 0);
}

static void sn_powerup_lock(struct panel_sn65dsi83 *sn)
{
	mutex_lock(&sn->power_mutex);
	sn_prepare(sn);
	mutex_unlock(&sn->power_mutex);
}

static void sn_enable_irq_lock(struct panel_sn65dsi83 *sn)
{
	mutex_lock(&sn->power_mutex);
	sn_enable_irq(sn);
	mutex_unlock(&sn->power_mutex);
}

void sn65_enable(struct panel_sn65dsi83 *sn)
{
	int ret;

	if (!sn->dev)
		return;
	ret = sn_check_videomode_change(sn);

	if (ret) {
		if (ret < 0) {
			dev_info(sn->dev, "%s: videomode error\n", __func__);
			return;
		}
		sn65_disable(sn);
	}
	dev_dbg(sn->dev, "%s\n", __func__);
	sn_powerup_lock(sn);
}

void sn65_enable2(struct panel_sn65dsi83 *sn)
{
	if (!sn->dev)
		return;
	msleep(5);
	dev_dbg(sn->dev, "%s\n", __func__);
	sn_enable_irq_lock(sn);
}

/*
 * We only report errors in this handler
 */
static irqreturn_t sn_irq_handler(int irq, void *id)
{
	struct panel_sn65dsi83 *sn = id;
	int status = sn_i2c_read_byte(sn, SN_IRQ_STAT);

	if (status >= 0) {
		if (status)
			sn_i2c_write_byte(sn, SN_IRQ_STAT, status);
	} else {
		dev_err(sn->dev, "%s: read error %d\n", __func__, status);
	}
	sn->int_cnt++;
	if (sn->int_cnt > 64) {
		dev_info(sn->dev, "%s: status %x %x %x\n", __func__,
			status, sn_i2c_read_byte(sn, SN_CLK_SRC),
			sn_i2c_read_byte(sn, SN_IRQ_MASK));
		if (sn->irq_enabled) {
			sn->irq_enabled = 0;
			disable_irq_nosync(sn->irq);
			dev_err(sn->dev, "%s: disabling irq, status=0x%x\n", __func__, status);
		}
		return IRQ_NONE;
	}

	if (status > 0) {
//		if (status & 1)
//			sn_i2c_write_byte(sn, SN_SOFT_RESET, 1);
		if (!(sn->int_cnt & 7) && sn->chip_enabled) {
			dev_err(sn->dev, "%s: trying to reinit, status %x %x %x\n", __func__,
				status, sn_i2c_read_byte(sn, SN_CLK_SRC),
				sn_i2c_read_byte(sn, SN_IRQ_MASK));
			sn_powerdown1(sn, 1);
			sn_powerup_lock(sn);
			sn_enable_irq_lock(sn);
		} else {
			msleep(20);
		}
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}


static ssize_t sn65dsi83_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_sn65dsi83 *sn = dev_to_panel_sn65(dev);
	int val;
	const unsigned char *p = registers_to_show;
	int i = 0;
	int total = 0;
	int reg;
	int cnt;

	if (!sn->chip_enabled)
		return -EBUSY;
	if (sn->show_reg != 0) {
		val = sn_i2c_read_byte(sn, sn->show_reg);
		return sprintf(buf, "%02x: %02x\n", sn->show_reg, val);
	}

	while (i < ARRAY_SIZE(registers_to_show)) {
		reg = *p++;
		i++;
		if (!reg) {
			reg = *p++;
			i++;
			val = sn_i2c_read_byte(sn, reg);
			val |= sn_i2c_read_byte(sn, reg + 1) << 8;
			cnt = sprintf(&buf[total], "%02x: %04x (%d)\n", reg, val, val);
		} else {
			val = sn_i2c_read_byte(sn, reg);
			cnt = sprintf(&buf[total], "%02x: %02x (%d)\n", reg, val, val);
		}
		if (cnt <= 0)
			break;
		total += cnt;
	}
	return total;
}

static ssize_t sn65dsi83_reg_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned val;
	int ret;
	struct panel_sn65dsi83 *sn = dev_to_panel_sn65(dev);
	char *endp;
	unsigned reg = simple_strtol(buf, &endp, 16);

	if (!sn->chip_enabled)
		return -EBUSY;
	if (reg > 0xe5)
		return count;

	sn->show_reg = reg;
	if (!endp)
		return count;
	if (*endp == 0x20)
		endp++;
	if (!*endp || *endp == 0x0a)
		return count;
	val = simple_strtol(endp, &endp, 16);
	if (val >= 0x100)
		return count;

	dev_err(dev, "%s:reg=0x%x, val=0x%x\n", __func__, reg, val);
	ret = sn_i2c_write_byte(sn, reg, val);
	if (ret < 0)
		return ret;
	return count;
}

static DEVICE_ATTR(sn65dsi83_reg, 0644, sn65dsi83_reg_show, sn65dsi83_reg_store);

static ssize_t sn65dsi83_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_sn65dsi83 *sn = dev_to_panel_sn65(dev);

	return sprintf(buf, "%d\n", sn->chip_enabled);
}

static ssize_t sn65dsi83_enable_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct panel_sn65dsi83 *sn = dev_to_panel_sn65(dev);
	char *endp;
	unsigned enable;

	enable = simple_strtol(buf, &endp, 16);
	if (enable) {
		sn65_enable(sn);
	} else {
		sn65_disable(sn);
	}
	return count;
}

static DEVICE_ATTR(sn65dsi83_enable, 0644, sn65dsi83_enable_show, sn65dsi83_enable_store);

/*
 * I2C init/probing/exit functions
 */
int sn65_init(struct device *dev, struct panel_sn65dsi83 *sn,
		struct device_node *disp_dsi, struct device_node *np)
{
	struct fwnode_handle *child = of_fwnode_handle(np);
	struct device_node *i2c_node;
	struct i2c_adapter *i2c;
	struct pinctrl_state *pins;
	struct pinctrl *pinctrl;
	struct gpio_desc *gp_en;
	struct gpio_desc *gp_irq;
	struct clk *pixel_clk;
	u32 sync_delay, hbp;
	const char *df;
	u32 dsi_lanes;
	int irq;
	int ret, i;

	dev_info(dev, "%s: start\n", __func__);
	i2c_node = of_parse_phandle(np, "i2c-bus", 0);
	if (!i2c_node)
		return 0;

	i2c = of_find_i2c_adapter_by_node(i2c_node);
	of_node_put(i2c_node);

	if (!i2c) {
		dev_dbg(sn->dev, "%s:i2c deferred\n", __func__);
		return -EPROBE_DEFER;
	}
	sn->i2c = i2c;
	of_property_read_u32(np, "i2c-address", &sn->i2c_address);
	of_property_read_u32(np, "i2c-max-frequency", &sn->i2c_max_frequency);

	ret = i2c_check_functionality(i2c,
					 I2C_FUNC_SMBUS_BYTE |
					 I2C_FUNC_SMBUS_BYTE_DATA);
	if (!ret) {
		dev_err(dev, "i2c_check_functionality failed\n");
		return -ENODEV;
	}

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		dev_dbg(sn->dev, "%s:devm_pinctrl_get %d\n", __func__, ret);
		return ret;
	}
	pins = pinctrl_lookup_state(pinctrl, "sn65dsi83");
	if (IS_ERR(pins)) {
		ret = PTR_ERR(pins);
		dev_dbg(sn->dev, "%s:pinctrl_lookup_state %d\n", __func__, ret);
		return ret;
	}
	ret = pinctrl_select_state(pinctrl, pins);
	if (ret) {
		dev_dbg(sn->dev, "%s:pinctrl_select_state %d\n", __func__, ret);
		return ret;
	}

	pixel_clk = devm_clk_get(dev, "pixel_clock");
	if (IS_ERR(pixel_clk)) {
		ret = PTR_ERR(pixel_clk);
		dev_dbg(sn->dev, "%s:devm_clk_get pixel_clk  %d\n", __func__, ret);
		pixel_clk = NULL;
	}

	for (i = 0; i < ARRAY_SIZE(sn->gp_en); i++) {
		gp_en = devm_fwnode_get_index_gpiod_from_child(dev, "enable", i,
				child, GPIOD_OUT_LOW, "sn65_en");
		if (IS_ERR(gp_en)) {
			ret = PTR_ERR(gp_en);
			if (i)
				break;
			dev_dbg(sn->dev,
				"%s:devm_fwnode_get_gpiod_from_child enable %d\n",
				__func__, ret);
			if (PTR_ERR(gp_en) != -EPROBE_DEFER)
				dev_err(dev,
					"Failed to get enable gpio: %d\n", ret);
			goto exit1;
		}
		if (!gp_en) {
			if (!i)
				dev_warn(dev, "no enable pin available");
			break;
		}
		sn->gp_en[i] = gp_en;
	}
	sn_enable_gp(sn);
	ret = sn_i2c_read_byte(sn, SN_CLK_SRC);
	if (ret < 0) {
		dev_info(dev, "i2c read failed\n");
		ret = -ENODEV;
		goto exit1;
	}

	sn->dev = dev;
	sn->pixel_clk = pixel_clk;
	mutex_init(&sn->power_mutex);

	sn->disp_dsi = disp_dsi;
	sn->sync_delay = 0x120;
	if (!of_property_read_u32(np, "sync-delay", &sync_delay)) {
		if (sync_delay > 0xfff) {
			ret = -EINVAL;
			goto exit1;
		}
		sn->sync_delay = sync_delay;
	}
	if (!of_property_read_u32(np, "hbp", &hbp)) {
		if (hbp > 0xff) {
			ret = -EINVAL;
			goto exit1;
		}
		sn->hbp = hbp;
	}
	if (of_property_read_u32(disp_dsi, "dsi-lanes", &dsi_lanes) < 0) {
		ret = -EINVAL;
		goto exit1;
	}
	if (dsi_lanes < 1 || dsi_lanes > 4) {
		ret = -EINVAL;
		goto exit1;
	}
	sn->dsi_lanes = dsi_lanes;
	sn->spwg = of_property_read_bool(np, "spwg");
	sn->jeida = of_property_read_bool(np, "jeida");
	if (!sn->spwg && !sn->jeida) {
		sn->spwg = of_property_read_bool(disp_dsi, "spwg");
		sn->jeida = of_property_read_bool(disp_dsi, "jeida");
	}
	sn->split_mode = of_property_read_bool(np, "split-mode");
	ret = of_property_read_string(disp_dsi, "dsi-format", &df);
	if (ret) {
		dev_err(dev, "dsi-format missing in display node%d\n", ret);
		goto exit1;
	}
	sn->dsi_bpp = !strcmp(df, "rgb666") ? 18 : 24;

	for (i = 0; i < 2; i++) {
		irq = of_irq_get(np, i);
		if (irq > 0) {
			gp_irq = devm_fwnode_get_index_gpiod_from_child(dev,
				"interrupts", i, child, GPIOD_IN, "sn65_irq");
			if (!IS_ERR(gp_en)) {
				if (gpiod_get_value(gp_irq))
					continue; /* active, wrong one */
			}
			sn->irq = irq;
			break;
		}
	}
	sn_disable_gp(sn);

	if (sn->irq) {
		irq_set_status_flags(sn->irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(dev, sn->irq,
				NULL, sn_irq_handler,
				IRQF_ONESHOT, client_name, sn);
		if (ret)
			pr_info("%s: request_irq failed, irq:%i\n", client_name, sn->irq);
	}

	ret = device_create_file(dev, &dev_attr_sn65dsi83_enable);
	ret = device_create_file(dev, &dev_attr_sn65dsi83_reg);
	if (ret < 0)
		pr_warn("failed to add sn65dsi83 sysfs files\n");
	dev_info(dev, "succeeded\n");
	return 0;
exit1:
	for (i = 0; i < ARRAY_SIZE(sn->gp_en); i++) {
		gp_en = sn->gp_en[i];
		if (!gp_en)
			break;
		/* enable might be used for something else, change to input */
		gpiod_direction_input(gp_en);
	}
	return ret;
}

int sn65_remove(struct panel_sn65dsi83 *sn)
{
	if (!sn->dev)
		return 0;
	device_remove_file(sn->dev, &dev_attr_sn65dsi83_reg);
	device_remove_file(sn->dev, &dev_attr_sn65dsi83_enable);
	sn65_disable(sn);
	return 0;
}
