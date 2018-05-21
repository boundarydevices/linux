/*
 *  sn65dsi83.c - DVI output chip
 *
 *  Copyright (C) 2011 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/input-polldev.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/kthread.h>
#include <video/display_timing.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

/* register definitions according to the sn65dsi83 data sheet */
#define SN_SOFT_RESET		0x09
#define SN_CLK_SRC		0x0a
#define SN_CLK_DIV		0x0b

#define SN_PLL_EN		0x0d
#define SN_DSI_LANES		0x10
#define SN_DSI_EQ		0x11
#define SN_DSI_CLK		0x12
#define SN_FORMAT		0x18

#define SN_LVDS_VOLTAGE		0x19
#define SN_LVDS_TERM		0x1a
#define SN_LVDS_CM_VOLTAGE	0x1b
#define SN_HACTIVE_LOW		0x20
#define SN_HACTIVE_HIGH		0x21
#define SN_VACTIVE_LOW		0x24
#define SN_VACTIVE_HIGH		0x25
#define SN_SYNC_DELAY_LOW	0x28
#define SN_SYNC_DELAY_HIGH	0x29
#define SN_HSYNC_LOW		0x2c
#define SN_HSYNC_HIGH		0x2d
#define SN_VSYNC_LOW		0x30
#define SN_VSYNC_HIGH		0x31
#define SN_HBP			0x34
#define SN_VBP			0x36
#define SN_HFP			0x38
#define SN_VFP			0x3a
#define SN_TEST_PATTERN		0x3c
#define SN_IRQ_EN		0xe0
#define SN_IRQ_MASK		0xe1
#define SN_IRQ_STAT		0xe5

static const char *client_name = "sn65dsi83";

static const unsigned char registers_to_show[] = {
	SN_SOFT_RESET, 	SN_CLK_SRC, SN_CLK_DIV, SN_PLL_EN,
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

struct sn65dsi83_priv
{
	struct i2c_client	*client;
	struct device_node	*disp_node;
	struct device_node	*disp_dsi;
	struct gpio_desc	*gp_en;
	struct clk		*mipi_clk;
	struct notifier_block	nb;
	u32			int_cnt;
	u32			pixelclock;
	u8			chip_enabled;
	u8			show_reg;
	u8			dsi_lanes;
	u8			spwg;	/* lvds lane 3 has MSBs of color */
	u8			jeida;	/* lvds lane 3 has LSBs of color */
	u8			dsi_bpp;
	u16			sync_delay;

	u8			dsi_clk_divider;
	u8			mipi_clk_index;
};

/**
 * sn_i2c_read_reg - read data from a register of the i2c slave device.
 *
 * @client: i2c device.
 * @reg: the register to read from.
 * @buf: raw write data buffer.
 * @len: length of the buffer to write
 */
static int sn_i2c_read_byte(struct sn65dsi83_priv *sn, u8 reg)
{
	struct i2c_client *client = sn->client;
	int ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s failed(%i)\n", __func__, ret);
	return ret;
}

/**
 * sn_i2c_write - write data to a register of the i2c slave device.
 *
 * @client: i2c device.
 * @reg: the register to write to.
 * @buf: raw data buffer to write.
 * @len: length of the buffer to write
 */
static int sn_i2c_write_byte(struct sn65dsi83_priv *sn, u8 reg, u8 val)
{
	struct i2c_client *client = sn->client;
	int ret = i2c_smbus_write_byte_data(sn->client, reg, val);

	if (ret < 0)
		dev_err(&client->dev, "%s failed(%i)\n", __func__, ret);
	return ret;
}

static void sn_disable(struct sn65dsi83_priv *sn)
{
	if (sn->chip_enabled) {
		disable_irq(sn->client->irq);
		sn->chip_enabled = 0;
	}
	gpiod_set_value(sn->gp_en, 0);
}

static void sn_enable_gp(struct gpio_desc *gp_en)
{
	msleep(15);	/* disabled for at least 10 ms */
	gpiod_set_value(gp_en, 1);
	msleep(1);
}

static void sn_enable_irq(struct sn65dsi83_priv *sn)
{
	sn_i2c_write_byte(sn, SN_IRQ_STAT, 0xff);
	sn_i2c_write_byte(sn, SN_IRQ_MASK, 0x7f);
	sn_i2c_write_byte(sn, SN_IRQ_EN, 1);
}

static int sn_get_dsi_clk_divider(struct sn65dsi83_priv *sn)
{
	u32 dsi_clk_divider = 25;
	u32 mipi_clk_rate;
	u8 mipi_clk_index;
	int ret;
	u32 pixelclock = sn->pixelclock;

	mipi_clk_rate = clk_get_rate(sn->mipi_clk);
	if (!mipi_clk_rate) {
		pr_err("mipi clock is off\n");
		mipi_clk_rate = pixelclock * 3;
	}
	if (mipi_clk_rate > 500000000) {
		pr_err("mipi clock(%d) is too high\n", mipi_clk_rate);
		mipi_clk_rate = 500000000;
	}
	if (pixelclock)
		dsi_clk_divider = mipi_clk_rate / pixelclock;

	if (dsi_clk_divider > 25)
		dsi_clk_divider = 25;
	else if (!dsi_clk_divider)
		dsi_clk_divider = 1;
	mipi_clk_index = mipi_clk_rate / 5000000;
	if (mipi_clk_index < 8)
		mipi_clk_index = 8;
	ret = (sn->dsi_clk_divider == dsi_clk_divider) &&
		(sn->mipi_clk_index == mipi_clk_index);
	if (!ret)
		pr_info("dsi_clk_divider = %d, mipi_clk_index=%d, mipi_clk_rate=%d\n",
			dsi_clk_divider, mipi_clk_index, mipi_clk_rate);
	sn->dsi_clk_divider = dsi_clk_divider;
	sn->mipi_clk_index = mipi_clk_index;
	return ret;
}

static int sn_setup_regs(struct sn65dsi83_priv *sn)
{
	unsigned i = 5;
	int format = 0x10;
	u32 pixelclock;
	struct videomode vm;
	int ret;

	ret = of_get_videomode(sn->disp_dsi, &vm, 0);
	if (ret < 0)
		return ret;

	pixelclock = vm.pixelclock;
	if (pixelclock) {
		if (pixelclock > 37500000) {
			i = (pixelclock - 12500000) / 25000000;
			if (i > 5)
				i = 5;
		}
	}
	sn->pixelclock = pixelclock;
	pr_info("pixelclock=%d %dx%d, margins=%d,%d %d,%d  syncs=%d %d\n",
		pixelclock, vm.hactive, vm.vactive,
		vm.hback_porch, vm.hfront_porch,
		vm.vback_porch, vm.vfront_porch,
		vm.hsync_len, vm.vsync_len);
	sn_i2c_write_byte(sn, SN_CLK_SRC, (i << 1) | 1);
	sn_get_dsi_clk_divider(sn);
	sn_i2c_write_byte(sn, SN_CLK_DIV, (sn->dsi_clk_divider - 1) << 3);

	sn_i2c_write_byte(sn, SN_DSI_LANES, ((4 - sn->dsi_lanes) << 3) | 0x20);
	sn_i2c_write_byte(sn, SN_DSI_EQ, 0);
	sn_i2c_write_byte(sn, SN_DSI_CLK, sn->mipi_clk_index);
	if (vm.flags & DISPLAY_FLAGS_DE_LOW)
		format |= BIT(7);
	if (!(vm.flags & DISPLAY_FLAGS_HSYNC_HIGH))
		format |= BIT(6);
	if (!(vm.flags & DISPLAY_FLAGS_VSYNC_HIGH))
		format |= BIT(5);
	if (sn->dsi_bpp == 24) {
		if (sn->spwg) {
			/* lvds lane 3 has MSBs of color */
			format |= BIT(3);
		} else if (sn->jeida) {
			/* lvds lane 3 has LSBs of color */
			format |= BIT(3) | BIT(1);
		} else {
			/* unused lvds lane 3 has LSBs of color */
			format |= BIT(1);
		}
	}
	sn_i2c_write_byte(sn, SN_FORMAT, format);

	sn_i2c_write_byte(sn, SN_LVDS_VOLTAGE, 4);
	sn_i2c_write_byte(sn, SN_LVDS_TERM, 2);
	sn_i2c_write_byte(sn, SN_LVDS_CM_VOLTAGE, 0);
	sn_i2c_write_byte(sn, SN_HACTIVE_LOW, (u8)vm.hactive);
	sn_i2c_write_byte(sn, SN_HACTIVE_HIGH, (u8)(vm.hactive >> 8));
	sn_i2c_write_byte(sn, SN_VACTIVE_LOW, (u8)vm.vactive);
	sn_i2c_write_byte(sn, SN_VACTIVE_HIGH, (u8)(vm.vactive >> 8));
	sn_i2c_write_byte(sn, SN_SYNC_DELAY_LOW, (u8)sn->sync_delay);
	sn_i2c_write_byte(sn, SN_SYNC_DELAY_HIGH, (u8)(sn->sync_delay >> 8));
	sn_i2c_write_byte(sn, SN_HSYNC_LOW, (u8)vm.hsync_len);
	sn_i2c_write_byte(sn, SN_HSYNC_HIGH, (u8)(vm.hsync_len >> 8));
	sn_i2c_write_byte(sn, SN_VSYNC_LOW, (u8)vm.vsync_len);
	sn_i2c_write_byte(sn, SN_VSYNC_HIGH, (u8)(vm.vsync_len >> 8));
	sn_i2c_write_byte(sn, SN_HBP, (u8)vm.hback_porch);
	sn_i2c_write_byte(sn, SN_VBP, (u8)vm.vback_porch);
	sn_i2c_write_byte(sn, SN_HFP, (u8)vm.hfront_porch);
	sn_i2c_write_byte(sn, SN_VFP, (u8)vm.vfront_porch);
	sn_i2c_write_byte(sn, SN_TEST_PATTERN, 0);
	return 0;
}

static void sn_enable_pll(struct sn65dsi83_priv *sn)
{
	if (!sn_get_dsi_clk_divider(sn)) {
		sn_i2c_write_byte(sn, SN_CLK_DIV, (sn->dsi_clk_divider - 1) << 3);
		sn_i2c_write_byte(sn, SN_DSI_CLK, sn->mipi_clk_index);
	}
	sn_i2c_write_byte(sn, SN_PLL_EN, 1);
	msleep(5);
	sn_i2c_write_byte(sn, SN_SOFT_RESET, 1);
	sn_enable_irq(sn);
}

static void sn_disable_pll(struct sn65dsi83_priv *sn)
{
	if (sn->chip_enabled)
		sn_i2c_write_byte(sn, SN_PLL_EN, 0);
}

static void sn_init(struct sn65dsi83_priv *sn)
{
	sn_i2c_write_byte(sn, SN_SOFT_RESET, 1);
	sn_i2c_write_byte(sn, SN_PLL_EN, 0);
	sn_i2c_write_byte(sn, SN_IRQ_MASK, 0x0);
	sn_i2c_write_byte(sn, SN_IRQ_EN, 1);
}

static void sn_prepare(struct sn65dsi83_priv *sn)
{
	sn_enable_gp(sn->gp_en);
	sn_init(sn);
	if (!sn->chip_enabled) {
		sn->chip_enabled = 1;
		enable_irq(sn->client->irq);
	}
	sn_setup_regs(sn);
}

static int sn_fb_event(struct notifier_block *nb, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct fb_info *info = evdata->info;
	struct device_node *node = info->device->of_node;
	struct sn65dsi83_priv *sn = container_of(nb, struct sn65dsi83_priv, nb);
	struct device *dev;
	int blank_type;

	dev = &sn->client->dev;
	if (0) dev_info(dev, "%s: event %lx)\n", __func__, event);
	if (node != sn->disp_node)
		return 0;

	switch (event) {
	case FB_R_EARLY_EVENT_BLANK:
		blank_type = *((int *)evdata->data);
		if (blank_type == FB_BLANK_UNBLANK) {
			sn_disable(sn);
		} else {
			sn_enable_pll(sn);
		}
		break;
	case FB_EARLY_EVENT_BLANK:
		blank_type = *((int *)evdata->data);
		if (blank_type == FB_BLANK_UNBLANK) {
			sn_prepare(sn);
		} else {
			sn_disable_pll(sn);
		}
		break;
	case FB_EVENT_BLANK: {
		blank_type = *((int *)evdata->data);
		if (blank_type == FB_BLANK_UNBLANK) {
			sn_enable_pll(sn);
		} else {
			sn_disable(sn);
		}
		dev_info(dev, "%s: blank type 0x%x\n", __func__, blank_type );
		break;
	}
	case FB_EVENT_SUSPEND : {
		dev_info(dev, "%s: suspend\n", __func__ );
		sn_disable(sn);
		break;
	}
	case FB_EVENT_RESUME : {
		dev_info(dev, "%s: resume\n", __func__ );
		break;
	}
	case FB_EVENT_FB_REGISTERED : {
		if (clk_get_rate(sn->mipi_clk)) {
			sn_prepare(sn);
			sn_enable_pll(sn);
		}
		break;
	}
	default:
		dev_info(dev, "%s: unknown event %lx\n", __func__, event);
	}
	return 0;
}



/*
 * We only report errors in this handler
 */
static irqreturn_t sn_irq_handler(int irq, void *id)
{
	struct sn65dsi83_priv *sn = id;
	int status = sn_i2c_read_byte(sn, SN_IRQ_STAT);

	if (status > 0) {
		sn_i2c_write_byte(sn, SN_IRQ_STAT, status);
		dev_info(&sn->client->dev, "%s: status %x %x %x\n", __func__,
			status, sn_i2c_read_byte(sn, SN_CLK_SRC),
			sn_i2c_read_byte(sn, SN_IRQ_MASK));
//		if (status & 1)
//			sn_i2c_write_byte(sn, SN_SOFT_RESET, 1);
		if (sn->int_cnt++ > 10) {
			disable_irq_nosync(sn->client->irq);
		} else {
			msleep(100);
		}
		return IRQ_HANDLED;
	} else {
		dev_err(&sn->client->dev, "%s: read error %d\n", __func__, status);
	}
	return IRQ_NONE;
}


static ssize_t sn65dsi83_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sn65dsi83_priv *sn = dev_get_drvdata(dev);
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
	struct sn65dsi83_priv *sn = dev_get_drvdata(dev);
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

/*
 * I2C init/probing/exit functions
 */
static int sn65dsi83_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	int ret;
	struct sn65dsi83_priv *sn;
	struct i2c_adapter *adapter;
        struct device_node *np = client->dev.of_node;
	struct gpio_desc *gp_en;
	const char *df;
	u32 sync_delay;
	u32 dsi_lanes;

	adapter = to_i2c_adapter(client->dev.parent);

	ret = i2c_check_functionality(adapter,
					 I2C_FUNC_SMBUS_BYTE |
					 I2C_FUNC_SMBUS_BYTE_DATA);
	if (!ret) {
		dev_err(&client->dev, "i2c_check_functionality failed\n");
		return -ENODEV;
	}

	gp_en = devm_gpiod_get_optional(&client->dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(gp_en)) {
		if (PTR_ERR(gp_en) != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get enable gpio: %ld\n",
				PTR_ERR(gp_en));
		return PTR_ERR(gp_en);
	}
	if (gp_en) {
		sn_enable_gp(gp_en);
	} else {
		dev_warn(&client->dev, "no enable pin available");
	}
	ret = i2c_smbus_read_byte_data(client, SN_CLK_SRC);
	if (ret < 0) {
		/* enable might be used for something else, change to input */
		gpiod_direction_input(gp_en);
		dev_info(&client->dev, "i2c read failed\n");
		return -ENODEV;
	}
//	gpiod_set_value(gp_en, 0);

	sn = devm_kzalloc(&client->dev, sizeof(*sn), GFP_KERNEL);
	if (!sn)
		return -ENOMEM;
	sn->client = client;
	sn->gp_en = gp_en;
	sn_init(sn);

	sn->disp_dsi = of_parse_phandle(np, "display-dsi", 0);
	if (!sn->disp_dsi)
		return -ENODEV;
	sn->disp_node = of_parse_phandle(np, "display", 0);
	if (!sn->disp_node)
		return -ENODEV;
	sn->sync_delay = 0x120;
	if (!of_property_read_u32(np, "sync-delay", &sync_delay)) {
		if (sync_delay > 0xfff)
			return -EINVAL;
		sn->sync_delay = sync_delay;
	}
	if (of_property_read_u32(sn->disp_dsi, "dsi-lanes", &dsi_lanes) < 0)
		return -EINVAL;
	if (dsi_lanes < 1 || dsi_lanes > 4)
		return -EINVAL;
	sn->dsi_lanes = dsi_lanes;
	sn->spwg = of_property_read_bool(sn->disp_dsi, "spwg");
	sn->jeida = of_property_read_bool(sn->disp_dsi, "jeida");
	ret = of_property_read_string(sn->disp_dsi, "dsi-format", &df);
	if (ret) {
		dev_err(&client->dev, "dsi-format missing in display node%d\n", ret);
		return ret;
	}
	sn->dsi_bpp = !strcmp(df, "rgb666") ? 18 : 24;

	sn->mipi_clk = devm_clk_get(&client->dev, "mipi_clk");
	if (IS_ERR(sn->mipi_clk))
		return PTR_ERR(sn->mipi_clk);

	ret = devm_request_threaded_irq(&client->dev, client->irq,
			NULL, sn_irq_handler,
			IRQF_ONESHOT, client->name, sn);
	if (ret)
		pr_info("%s: request_irq failed, irq:%i\n", client_name, client->irq);
	disable_irq(client->irq);

	i2c_set_clientdata(client, sn);
	sn->nb.notifier_call = sn_fb_event;
	ret = fb_register_client(&sn->nb);
	if (ret < 0) {
		dev_err(&client->dev, "fb_register_client failed(%d)\n", ret);
		return ret;
	}
	ret = device_create_file(&client->dev, &dev_attr_sn65dsi83_reg);
	if (ret < 0)
		pr_warn("failed to add sn65dsi83 sysfs files\n");

	sn_prepare(sn);
	dev_info(&client->dev, "succeeded\n");
	return 0;
}

static int sn65dsi83_remove(struct i2c_client *client)
{
	struct sn65dsi83_priv *sn = i2c_get_clientdata(client);

	device_remove_file(&client->dev, &dev_attr_sn65dsi83_reg);
	fb_unregister_client(&sn->nb);
	sn_disable(sn);
	return 0;
}

static const struct i2c_device_id sn65dsi83_id[] = {
	{"sn65dsi83", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, sn65dsi83_id);

static struct i2c_driver sn65dsi83_driver = {
	.driver = {
		   .name = "sn65dsi83",
		   .owner = THIS_MODULE,
		   },
	.probe = sn65dsi83_probe,
	.remove = sn65dsi83_remove,
	.id_table = sn65dsi83_id,
};

module_i2c_driver(sn65dsi83_driver);

MODULE_AUTHOR("Boundary Devices, Inc.");
MODULE_DESCRIPTION("sn65dsi83 mipi to lvds bridge");
MODULE_LICENSE("GPL");
