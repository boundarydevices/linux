// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Huijuan Xie <huijuan.xie@mediatek.com>
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/component.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/of_irq.h>

bool it6122_enabled;
EXPORT_SYMBOL(it6122_enabled);

struct it6122_reg_cfg {
	u8 reg;
	u8 val;
	int sleep_in_ms;
};

struct it6122_data {
	struct device *dev;
	struct i2c_client *i2c_client;
	bool power_on;
	u32 reset_gpio;
	u32 vio33tp1_gpio;
};

struct it6122_data *it_pdata_ext;
static int it6122_parse_dt(struct device *dev,
	struct it6122_data *it_pdata)
{
	struct device_node *np = dev->of_node;

	it_pdata->reset_gpio =
		of_get_named_gpio(np, "ite,reset-gpio", 0);
	if (!gpio_is_valid(it_pdata->reset_gpio)) {
		pr_err("reset gpio not specified\n");
		return -EINVAL;
	}
	pr_debug("%s: reset_gpio = %d\n", __func__, it_pdata->reset_gpio);

	it_pdata->vio33tp1_gpio =
		of_get_named_gpio(np, "vio33tp1-gpio", 0);
	if (!gpio_is_valid(it_pdata->vio33tp1_gpio)) {
		pr_err("vio33tp1 gpio not specified\n");
		return -EINVAL;
	}
	pr_debug("%s: vio33tp1_gpio = %d\n", __func__, it_pdata->vio33tp1_gpio);

	return 0;
};

static struct it6122_reg_cfg it6122_init_setup[] = {
	{0x05, 0x17, 0},
	{0xC0, 0x01, 0},
	{0x05, 0x1F, 0},
	{0x05, 0x10, 0},
	{0xC0, 0x00, 0},
	{0xC0, 0x01, 0},
	{0xC5, 0xC0, 0},
	{0xC6, 0x20, 0},
	{0xC7, 0x20, 0},
	//Dual lvds link
	//{0xCB, 0x39, 0},
	//Single lvds link, 4data+1clock
	{0xCB, 0x31, 0},
	{0xC9, 0x00, 0},
	{0xC3, 0xE6, 0},
	{0xF5, 0x41, 0},
	{0xD8, 0x10, 0},
	{0xDA, 0x80, 0},
	{0x09, 0xFF, 0},
	{0x0A, 0xFF, 0},
	{0x0B, 0xFF, 0},
	{0xF8, 0x03, 0},
	{0x0D, 0x00, 0},
	{0x0C, 0x03, 0},
	{0x11, 0x03, 0},
	{0x12, 0x01, 0},
	{0x18, 0xC3, 0},
	{0x19, 0x03, 0},
	{0x20, 0x03, 0},
	{0x44, 0x30, 0},
	{0x4B, 0x01, 0},
	{0x4C, 0x04, 0},
	{0x4E, 0x00, 0},
	{0x4F, 0x01, 0},
	{0x27, 0x3E, 0},
	{0x70, 0x01, 0},
	{0x72, 0x07, 0},
	{0x73, 0x04, 0},
	{0x80, 0x03, 0},
	{0x84, 0xFF, 0},
	{0x85, 0x00, 0},
	{0xA1, 0x00, 0},
	{0xA2, 0x00, 0},
	{0xA3, 0x28, 0},
	{0xA5, 0x02, 0},
	{0xA0, 0x01, 0},
	{0x92, 0x1E, 0},
	{0x80, 0x02, 0},
	{0x70, 0x01, 0},
	{0x05, 0x14, 0},
	{0xC0, 0x03, 0},
	{0xC5, 0xC0, 0},
	{0x05, 0x00, 0},
	{0x91, 0xFB, 0},
	{0x31, 0x00, 0},
	{0x33, 0x3F, 0},
	{0x35, 0x3F, 0},
	{0x37, 0x00, 0},
	{0x39, 0x00, 0},
	{0x3A, 0x00, 0},
	{0x3C, 0x7F, 0},
	{0x3E, 0x7F, 0},
	{0x41, 0x00, 0},
	{0x43, 0x00, 0},
	{0xB0, 0x00, 0},
	{0xD0, 0x00, 0},
	{0x06, 0x11, 100},
	{0x05, 0x10, 0},
	{0x05, 0x00, 0},
	{0xC6, 0x00, 0},
	{0xC7, 0x00, 0},
};

static int it6122_write(struct it6122_data *it_pdata, u8 reg, u8 val)
{
	struct i2c_client *client = it_pdata->i2c_client;
	int ret;
	u8 buf[2] = {reg, val};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret != 1) {
		pr_err("%s i2c write failed", __func__);
		return ret;
	}

	return (ret == 1 ? 0 : -EIO);
}

static int it6122_read_u8(struct it6122_data *it_pdata, u8 reg, u8 *buf)
{

	struct i2c_client *client = it_pdata->i2c_client;
	int ret;

	ret = i2c_master_send(client, &reg, sizeof(reg));
	if (ret != sizeof(reg)) {
		pr_err("%s Failed to send i2c command, reg=0x%02x, ret=%d\n", __func__, reg, ret);
		return ret;
	}

	ret = i2c_master_recv(client, buf, sizeof(*buf));
	if (ret != 1) {
		pr_err("%s Failed to recv i2c command, reg=0x%02x, buf=0x%02x, ret=%d\n",
			__func__, reg, *buf, ret);
		return ret;
	}
	pr_debug("%s reg=0x%02x, buf=0x%02x\n", __func__, reg, *buf);
	return 0;
}

static int it6122_write_array(struct it6122_data *it_pdata,
	struct it6122_reg_cfg *cfg, size_t size)
{
	int ret = 0;
	int i;
	const size_t size_len = size / sizeof(struct it6122_reg_cfg);

	for (i = 0; i < size_len; i++) {
		ret = it6122_write(it_pdata, cfg[i].reg, cfg[i].val);

		if (ret != 0) {
			pr_err("reg writes failed. Last write %02X to %02X\n",
				cfg[i].val, cfg[i].reg);
			goto w_regs_fail;
		}

		if (cfg[i].sleep_in_ms)
			msleep(cfg[i].sleep_in_ms);
	}

w_regs_fail:
	if (ret != 0)
		pr_err("exiting with ret = %d after %d writes\n", ret, i);

	return ret;
}

void IT6122_ShowPParam(struct it6122_data *it_pdata)
{
	u8 u8PRec_FIFOStg;
	u8 data = 0;
	u8 data1 = 0;
	u32 g_u16PHFP = 0, g_u16PHSW = 0, g_u16PHBP = 0;
	u32	g_u16PHDEW = 0, g_u16PHVR2nd = 0, g_u16PHTotal = 0;
	u32 g_u16PVFP = 0, g_u16PVSW = 0, g_u16PVBP = 0;
	u32 g_u16PVDEW = 0, g_u16PVFP2nd = 0, g_u16PVTotal = 0;

	pr_info("%s+\n", __func__);

	it6122_read_u8(it_pdata, 0x30, &data);
	it6122_read_u8(it_pdata, 0x31, &data1);
	g_u16PHFP = data + ((data1 & 0x3F) << 8);
	pr_info("0x30=0x%x; 0x31=0x%x, g_u16PHFP=0x%x\n", data, data1, g_u16PHFP);
	usleep_range(1000, 1100);

	it6122_read_u8(it_pdata, 0x32, &data);
	it6122_read_u8(it_pdata, 0x33, &data1);
	g_u16PHSW = data + ((data1 & 0x3F) << 8);
	pr_info("0x32=0x%x; 0x33=0x%x, g_u16PHSW=0x%x\n", data, data1, g_u16PHSW);
	usleep_range(1000, 1100);

	it6122_read_u8(it_pdata, 0x34, &data);
	it6122_read_u8(it_pdata, 0x35, &data1);
	g_u16PHBP = data + ((data1 & 0x3F) << 8);
	pr_info("0x34=0x%x; 0x35=0x%x, g_u16PHBP=0x%x\n", data, data1, g_u16PHBP);
	usleep_range(1000, 1100);

	it6122_read_u8(it_pdata, 0x36, &data);
	it6122_read_u8(it_pdata, 0x37, &data1);
	g_u16PHDEW = data + ((data1 & 0x3F) << 8);
	pr_info("0x36=0x%x; 0x37=0x%x, g_u16PHDEW=0x%x\n", data, data1, g_u16PHDEW);
	usleep_range(1000, 1100);

	it6122_read_u8(it_pdata, 0x38, &data);
	it6122_read_u8(it_pdata, 0x39, &data1);
	g_u16PHVR2nd = data + ((data1 & 0x3F) << 8);
	pr_info("0x38=0x%x; 0x39=0x%x, g_u16PHVR2nd=0x%x\n", data, data1, g_u16PHVR2nd);
	usleep_range(1000, 1100);

	g_u16PHTotal = g_u16PHFP + g_u16PHSW + g_u16PHBP + g_u16PHDEW;
	pr_info("g_u16PHTotal=%02x\n", g_u16PHTotal);

	it6122_read_u8(it_pdata, 0x3A, &data);
	it6122_read_u8(it_pdata, 0x3B, &data1);
	g_u16PVFP = data + ((data1 & 0x3F) << 8);
	pr_info("0x3A=0x%x; 0x3B=0x%x, g_u16PVFP=0x%x\n", data, data1, g_u16PVFP);
	usleep_range(1000, 1100);

	it6122_read_u8(it_pdata, 0x3C, &data);
	it6122_read_u8(it_pdata, 0x3D, &data1);
	g_u16PVSW = data + ((data1 & 0x3F) << 8);
	pr_info("0x3C=0x%x; 0x3D=0x%x, g_u16PVSW=0x%x\n", data, data1, g_u16PVSW);
	usleep_range(1000, 1100);

	it6122_read_u8(it_pdata, 0x3E, &data);
	it6122_read_u8(it_pdata, 0x3F, &data1);
	g_u16PVBP = data + ((data1 & 0x3F) << 8);
	pr_info("0x3E=0x%x; 0x3F=0x%x, g_u16PVBP=0x%x\n", data, data1, g_u16PVBP);
	usleep_range(1000, 1100);

	it6122_read_u8(it_pdata, 0x40, &data);
	it6122_read_u8(it_pdata, 0x41, &data1);
	g_u16PVDEW = data + ((data1 & 0x3F) << 8);
	pr_info("0x40=0x%x; 0x41=0x%x, g_u16PVDEW=0x%x\n", data, data1, g_u16PVDEW);
	usleep_range(1000, 1100);

	it6122_read_u8(it_pdata, 0x42, &data);
	it6122_read_u8(it_pdata, 0x43, &data1);
	g_u16PVFP2nd = data + ((data1 & 0x3F) << 8);
	pr_info("0x42=0x%x; 0x43=0x%x, g_u16PVFP2nd=0x%x\n", data, data1, g_u16PVFP2nd);
	usleep_range(1000, 1100);

	g_u16PVTotal = g_u16PVFP + g_u16PVSW + g_u16PVBP + g_u16PVDEW;
	pr_info("g_u16PVTotal=0x%x\n", g_u16PVTotal);

	it6122_read_u8(it_pdata, 0x6C, &u8PRec_FIFOStg);
	pr_info("0x6c=0x%x, PRec_FIFOStg = %d\r\n ", u8PRec_FIFOStg, u8PRec_FIFOStg & 0x07);
	usleep_range(1000, 1100);

	pr_info("%s-\n", __func__);
}

void IT6122_DumpReg(struct it6122_data *it_pdata)
{
	int i = 0;
	u8 data = 0;

	pr_info("%s+\n", __func__);
	for (i = 0 ; i < 0xFF; i++) {

		if ((i % 0x10) == 0x00)
			pr_info("i=%02x\n", i);

		it6122_read_u8(it_pdata, i, &data);
		pr_info("Read reg(0x%x)=0x%02x", i, data);
		usleep_range(1000, 1100);
	}
	it6122_read_u8(it_pdata, 0xFF, &data);
	pr_info("Read reg(0xFF)=0x%02x", data);

	IT6122_ShowPParam(it_pdata);
	pr_info("%s-\n", __func__);
}

static int it6122_power_on_off(struct it6122_data *it_pdata, bool on)
{
	int ret;
	u8 data = 0;

	pr_debug("%s (set to %d)\n", __func__, on);

	if (on && !it_pdata->power_on) {
		gpio_direction_output(it_pdata->vio33tp1_gpio, 1);
		usleep_range(20000, 20100);
		gpio_direction_output(it_pdata->reset_gpio, 0);
		usleep_range(20000, 20100);
		gpio_direction_output(it_pdata->reset_gpio, 1);
		usleep_range(20000, 20100);

		ret = it6122_write(it_pdata, 0x11, 0x03); // normal mode
		if (ret != 0) {
			pr_err("it6122_write 0x11 to 0x03 failed\n");
			return ret;
		}

		ret = it6122_write_array(it_pdata, it6122_init_setup, sizeof(it6122_init_setup));
		if (ret != 0) {
			pr_err("it6122_write_array with it6122_init_setup failed\n");
			return ret;
		}
		msleep(130);

		it_pdata->power_on = true;
		pr_debug("it6122 power on(reset gpio = %d)\n", it_pdata->reset_gpio);
	} else if (!on) {
		ret = it6122_write(it_pdata, 0x05, 0x10);
		if (ret != 0) {
			pr_err("it6122_write 0x05 to 0x10 failed\n");
			return ret;
		}
		ret = it6122_write(it_pdata, 0x11, 0x07); // standby mode
		if (ret != 0) {
			pr_err("it6122_write 0x11 to 0x07 failed\n");
			return ret;
		}
		usleep_range(10000, 10100);
		gpio_direction_output(it_pdata->reset_gpio, 0);
		msleep(1000);
		gpio_direction_output(it_pdata->vio33tp1_gpio, 0);
		msleep(1000);
		it_pdata->power_on = false;
		pr_debug("it6122 power off(reset gpio = %d)\n", it_pdata->reset_gpio);
	}

	return 0;
};

int it6122_bridge_power_on_off(bool on)
{
	struct it6122_data *it_pdata;
	int ret;

	it_pdata = it_pdata_ext;
	if (it_pdata) {
		ret = it6122_power_on_off(it_pdata, on);
		if (ret != 0) {
			pr_err("it6122_power_on_off failed, on = %d\n", on);
			return ret;
		}
	} else {
		pr_err("%s failed, it_pdata is NULL\n", __func__);
		return -EIO;
	}
	return 0;
};
EXPORT_SYMBOL(it6122_bridge_power_on_off);

bool it6122_bridge_enabled(void)
{
	return it6122_enabled;
};
EXPORT_SYMBOL(it6122_bridge_enabled);

static int it6122_probe(struct i2c_client *client,
	 const struct i2c_device_id *id)
{
	struct it6122_data *it_pdata;
	int ret = 0;
	u8 chip_id = 0;

	it_pdata_ext = NULL;
	pr_debug("%s: entry wayT\n", __func__);
	if (!client || !client->dev.of_node) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("device doesn't support I2C\n");
		return -ENODEV;
	}

	it_pdata = devm_kzalloc(&client->dev,
		sizeof(struct it6122_data), GFP_KERNEL);
	if (!it_pdata)
		return -ENOMEM;

	ret = it6122_parse_dt(&client->dev, it_pdata);
	if (ret) {
		pr_err("failed to parse device tree\n");
		goto err_dt_parse;
	}

	it_pdata->dev = &client->dev;
	it_pdata->i2c_client = client;
	pr_debug("%s: I2C address is 0x%X\n", __func__, client->addr);

	i2c_set_clientdata(client, it_pdata);
	dev_set_drvdata(&client->dev, it_pdata);

	gpio_direction_output(it_pdata->reset_gpio, 1);
	usleep_range(20000, 20100);
	it6122_read_u8(it_pdata, 0x00, &chip_id);
	pr_debug("it6122 Chip ID:0x%x\n", chip_id);
	if (chip_id != 0x54) {
		pr_err("%s error reading config register\n", __func__);
		ret = -ENODEV;
		goto err_config;
	}
	pr_debug("%s: Probe success!\n", __func__);

	it6122_enabled = true;
	it_pdata_ext = it_pdata;

	return ret;

err_config:
err_dt_parse:
	devm_kfree(&client->dev, it_pdata);
	return ret;
};

static int it6122_remove(struct i2c_client *client)
{
	struct it6122_data *it_pdata = i2c_get_clientdata(client);

	devm_kfree(&client->dev, it_pdata);
	it_pdata_ext = NULL;

	return 0;
};

static struct i2c_device_id it6122_id[] = {
	{ "ite,it6122", 0},
	{}
};

static const struct of_device_id it6122_match_table[] = {
	{.compatible = "ite,it6122"},
	{}
};
MODULE_DEVICE_TABLE(of, it6122_match_table);

static struct i2c_driver it6122_driver = {
	.driver = {
		.name = "it6122",
		.owner = THIS_MODULE,
		.of_match_table = it6122_match_table,
	},
	.probe = it6122_probe,
	.remove = it6122_remove,
	.id_table = it6122_id,
};

static int __init it6122_init(void)
{
	return i2c_add_driver(&it6122_driver);
}

static void __exit it6122_exit(void)
{
	i2c_del_driver(&it6122_driver);
}

module_init(it6122_init);
module_exit(it6122_exit);
