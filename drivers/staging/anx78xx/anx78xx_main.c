/*
 * Copyright(c) 2015, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/async.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/delay.h>

#include "anx78xx.h"
#include "slimport_tx_drv.h"

void anx78xx_poweron(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	struct anx78xx_platform_data *pdata = anx78xx->pdata;

	gpiod_set_value_cansleep(pdata->gpiod_reset, 1);
	usleep_range(1000, 2000);

	gpiod_set_value_cansleep(pdata->gpiod_pd, 0);
	usleep_range(1000, 2000);

	gpiod_set_value_cansleep(pdata->gpiod_reset, 0);

	dev_dbg(dev, "power on\n");
}

void anx78xx_poweroff(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	struct anx78xx_platform_data *pdata = anx78xx->pdata;

	gpiod_set_value_cansleep(pdata->gpiod_reset, 1);
	usleep_range(1000, 2000);

	gpiod_set_value_cansleep(pdata->gpiod_pd, 1);
	usleep_range(1000, 2000);

	dev_dbg(dev, "power down\n");
}

static int anx78xx_init_gpio(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	struct anx78xx_platform_data *pdata = anx78xx->pdata;
	int ret;

	/* gpio for chip power down */
	pdata->gpiod_pd = devm_gpiod_get(dev, "pd", GPIOD_OUT_HIGH);
	if (IS_ERR(pdata->gpiod_pd)) {
		dev_err(dev, "unable to claim pd gpio\n");
		ret = PTR_ERR(pdata->gpiod_pd);
		return ret;
	}

	/* gpio for chip reset */
	pdata->gpiod_reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(pdata->gpiod_reset)) {
		dev_err(dev, "unable to claim reset gpio\n");
		ret = PTR_ERR(pdata->gpiod_reset);
		return ret;
	}

	return 0;
}

static int anx78xx_system_init(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	int ret;

	ret = sp_chip_detect(anx78xx);
	if (ret == 0) {
		anx78xx_poweroff(anx78xx);
		dev_err(dev, "failed to detect anx78xx\n");
		return -ENODEV;
	}

	sp_tx_variable_init();
	return 0;
}

static void anx78xx_work_func(struct work_struct *work)
{
	struct anx78xx *anx78xx = container_of(work, struct anx78xx,
					       work.work);
	int workqueu_timer = 0;

	if (sp_tx_cur_states() >= STATE_PLAY_BACK)
		workqueu_timer = 500;
	else
		workqueu_timer = 100;
	mutex_lock(&anx78xx->lock);
	sp_main_process(anx78xx);
	mutex_unlock(&anx78xx->lock);
	queue_delayed_work(anx78xx->workqueue, &anx78xx->work,
			   msecs_to_jiffies(workqueu_timer));
}

static int anx78xx_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct anx78xx *anx78xx;
	int ret;

	if (!i2c_check_functionality(client->adapter,
		I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&client->dev, "i2c bus does not support the device\n");
		return -ENODEV;
	}

	anx78xx = devm_kzalloc(&client->dev,
			sizeof(struct anx78xx),
			GFP_KERNEL);
	if (!anx78xx)
		return -ENOMEM;

	anx78xx->pdata = devm_kzalloc(&client->dev,
			sizeof(struct anx78xx_platform_data),
			GFP_KERNEL);
	if (!anx78xx->pdata)
		return -ENOMEM;

	anx78xx->client = client;

	i2c_set_clientdata(client, anx78xx);

	mutex_init(&anx78xx->lock);

	ret = anx78xx_init_gpio(anx78xx);
	if (ret) {
		dev_err(&client->dev, "failed to initialize gpios\n");
		return ret;
	}

	INIT_DELAYED_WORK(&anx78xx->work, anx78xx_work_func);

	anx78xx->first_time = 1;
	anx78xx->workqueue = create_singlethread_workqueue("anx78xx_work");
	if (anx78xx->workqueue == NULL) {
		dev_err(&client->dev, "failed to create work queue\n");
		return -ENOMEM;
	}

	ret = anx78xx_system_init(anx78xx);
	if (ret) {
		dev_err(&client->dev, "failed to initialize anx78xx\n");
		goto cleanup;
	}

	/* enable driver */
	queue_delayed_work(anx78xx->workqueue, &anx78xx->work, 0);

	return 0;

cleanup:
	destroy_workqueue(anx78xx->workqueue);
	return ret;
}

static int anx78xx_i2c_remove(struct i2c_client *client)
{
	struct anx78xx *anx78xx = i2c_get_clientdata(client);

	destroy_workqueue(anx78xx->workqueue);

	return 0;
}

static int anx78xx_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct anx78xx *anx78xx = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&anx78xx->work);
	flush_workqueue(anx78xx->workqueue);
	anx78xx_poweroff(anx78xx);
	sp_tx_clean_state_machine();

	return 0;
}

static int anx78xx_i2c_resume(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct anx78xx *anx78xx = i2c_get_clientdata(client);

	queue_delayed_work(anx78xx->workqueue, &anx78xx->work, 0);

	return 0;
}

static SIMPLE_DEV_PM_OPS(anx78xx_i2c_pm_ops,
			anx78xx_i2c_suspend, anx78xx_i2c_resume);

static const struct i2c_device_id anx78xx_id[] = {
	{"anx7814", 0},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(i2c, anx78xx_id);

static const struct of_device_id anx78xx_match_table[] = {
	{.compatible = "analogix,anx7814",},
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, anx78xx_match_table);

static struct i2c_driver anx78xx_driver = {
	.driver = {
		   .name = "anx7814",
		   .pm = &anx78xx_i2c_pm_ops,
		   .of_match_table = of_match_ptr(anx78xx_match_table),
		   },
	.probe = anx78xx_i2c_probe,
	.remove = anx78xx_i2c_remove,
	.id_table = anx78xx_id,
};

module_i2c_driver(anx78xx_driver);

MODULE_DESCRIPTION("Slimport transmitter ANX78XX driver");
MODULE_AUTHOR("Junhua Xia <jxia@xxxxxxxxxxxxxxxx>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.1");
