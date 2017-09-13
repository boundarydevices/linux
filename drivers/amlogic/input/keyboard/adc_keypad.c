/*
 * drivers/amlogic/input/keyboard/adc_keypad.c
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/iio/consumer.h>
#include <linux/input-polldev.h>
#include <linux/amlogic/scpi_protocol.h>
#include "adc_keypad.h"

#define POLL_INTERVAL_DEFAULT 25
#define KEY_JITTER_COUNT  1
#define TMP_BUF_MAX 128

static char adc_key_mode_name[MAX_NAME_LEN] = "abcdef";
static char kernelkey_en_name[MAX_NAME_LEN] = "abcdef";
static bool keypad_enable_flag = true;

static int meson_adc_kp_search_key(struct meson_adc_kp *kp)
{
	struct adc_key *key;
	int value, i;

	mutex_lock(&kp->kp_lock);
	for (i = 0; i < kp->chan_num; i++) {
		if (iio_read_channel_processed(kp->pchan[kp->chan[i]],
				&value) >= 0) {
			if (value < 0)
				continue;
			list_for_each_entry(key, &kp->adckey_head, list) {
				if ((key->chan == kp->chan[i])
				&& (value >= key->value - key->tolerance)
				&& (value <= key->value + key->tolerance)) {
					mutex_unlock(&kp->kp_lock);
					return key->code;
				}
			}
		}
	}
	mutex_unlock(&kp->kp_lock);
	return KEY_RESERVED;
}

static void meson_adc_kp_poll(struct input_polled_dev *dev)
{
	struct meson_adc_kp *kp = dev->private;

	int code = meson_adc_kp_search_key(kp);

	if (kp->report_code && kp->report_code != code) {
		dev_info(&kp->poll_dev->input->dev,
				"key %d up\n", kp->report_code);
		input_report_key(kp->poll_dev->input, kp->report_code, 0);
		input_sync(kp->poll_dev->input);
		kp->report_code = 0;
	}

	if (code) {
		if (kp->count > 0 && code != kp->prev_code)
			kp->count = 0;

		if (kp->count < KEY_JITTER_COUNT)
			kp->count++;
		else {
			if (keypad_enable_flag && kp->report_code != code) {
				dev_info(&kp->poll_dev->input->dev,
					"key %d down\n", code);
				input_report_key(kp->poll_dev->input, code, 1);
				input_sync(kp->poll_dev->input);
				kp->report_code = code;
			}
			kp->count = 0;
		}
		kp->prev_code = code;
	}

}

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void meson_adc_kp_early_suspend(struct early_suspend *h)
{
	struct meson_adc_kp *kp = container_of(h,
			struct meson_adc_kp, early_suspend);

	cancel_delayed_work_sync(&kp->poll_dev->work);
}

static void meson_adc_kp_late_resume(struct early_suspend *h)
{
	struct meson_adc_kp *kp = container_of(h,
			struct meson_adc_kp, early_suspend);

	queue_delayed_work(system_freezable_wq, &kp->poll_dev->work,
		msecs_to_jiffies(kp->poll_dev->poll_interval));
}
#endif

static void send_data_to_bl301(void)
{
	u32 val;
	if (!strcmp(adc_key_mode_name, "POWER_WAKEUP_POWER")) {
		val = 0;  /*only power key resume*/
		scpi_send_usr_data(SCPI_CL_POWER, &val, sizeof(val));
	} else if (!strcmp(adc_key_mode_name, "POWER_WAKEUP_ANY")) {
		val = 1; /*any key resume*/
		scpi_send_usr_data(SCPI_CL_POWER, &val, sizeof(val));
	} else if (!strcmp(adc_key_mode_name, "POWER_WAKEUP_NONE")) {
		val = 2; /*no key can resume*/
		scpi_send_usr_data(SCPI_CL_POWER, &val, sizeof(val));
	}

}

static void kernel_keypad_enable_mode_enable(void)
{
	if (!strcmp(kernelkey_en_name, "KEYPAD_UNLOCK"))
		keypad_enable_flag = 1;  /*unlock, normal mode*/
	else if (!strcmp(kernelkey_en_name, "KEYPAD_LOCK"))
		keypad_enable_flag = 0;  /*lock, press key will be not useful*/
	else
		keypad_enable_flag = 1;
}

/*meson_adc_kp_get_valid_chan() - used to get valid adc channel
 *
 *@kp: to save number of channel in
 *
 */
static void meson_adc_kp_get_valid_chan(struct meson_adc_kp *kp)
{
	unsigned char incr;
	struct adc_key *key;

	mutex_lock(&kp->kp_lock);
	kp->chan_num = 0; /*recalculate*/
	list_for_each_entry(key, &kp->adckey_head, list) {
		if (kp->chan_num == 0) {
			kp->chan[kp->chan_num++] = key->chan;
		} else {
			for (incr = 0; incr < kp->chan_num; incr++) {
				if (key->chan == kp->chan[incr])
					break;
				if (incr == (kp->chan_num - 1))
					kp->chan[kp->chan_num++] = key->chan;
			}
		}
	}
	mutex_unlock(&kp->kp_lock);
}

static int meson_adc_kp_get_devtree_pdata(struct platform_device *pdev,
			struct meson_adc_kp *kp)
{
	int ret;
	int count;
	int value;
	int state = 0;
	unsigned char cnt;
	const char *uname;
	unsigned int key_num;
	struct adc_key *key;
	struct of_phandle_args chanspec;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "failed to get device node\n");
		return -EINVAL;
	}

	count = of_property_count_strings(pdev->dev.of_node,
		"io-channel-names");
	if (count < 0) {
		dev_err(&pdev->dev, "failed to get io-channel-names");
		return -ENODATA;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "poll-interval", &value);
	if (ret)
		kp->poll_period = POLL_INTERVAL_DEFAULT;
	else
		kp->poll_period = value;

	for (cnt = 0; cnt < count; cnt++) {
		ret = of_parse_phandle_with_args(pdev->dev.of_node,
			"io-channels", "#io-channel-cells", cnt, &chanspec);
		if (ret)
			return ret;

		if (!chanspec.args_count)
			return -EINVAL;

		if (chanspec.args[0] >= SARADC_CH_NUM) {
			dev_err(&pdev->dev, "invalid channel index[%u]\n",
					chanspec.args[0]);
			return -EINVAL;
		}

		ret = of_property_read_string_index(pdev->dev.of_node,
				"io-channel-names", cnt, &uname);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid channel name index[%d]\n",
					cnt);
			return -EINVAL;
		}

		kp->pchan[chanspec.args[0]] = devm_iio_channel_get(&pdev->dev,
				uname);
		if (IS_ERR(kp->pchan[chanspec.args[0]]))
			return PTR_ERR(kp->pchan[chanspec.args[0]]);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "key_num", &key_num);
	if (ret) {
		dev_err(&pdev->dev, "failed to get key_num!\n");
		return -EINVAL;
	}

	for (cnt = 0; cnt < key_num; cnt++) {
		key = kzalloc(sizeof(struct adc_key), GFP_KERNEL);
		if (!key) {
			dev_err(&pdev->dev, "alloc mem failed!\n");
			return -ENOMEM;
		}

		ret = of_property_read_string_index(pdev->dev.of_node,
			 "key_name", cnt, &uname);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid key name index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}
		strncpy(key->name, uname, MAX_NAME_LEN);

		ret = of_property_read_u32_index(pdev->dev.of_node,
			"key_code", cnt, &key->code);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid key code index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
			"key_chan", cnt, &key->chan);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid key chan index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}

		if (!kp->pchan[key->chan]) {
			dev_err(&pdev->dev, "invalid channel[%u], please enable it first by DTS\n",
					key->chan);
			state = -EINVAL;
			goto err;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
			"key_val", cnt, &key->value);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid key value index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
			"key_tolerance", cnt, &key->tolerance);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid key tolerance index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}
		list_add_tail(&key->list, &kp->adckey_head);
	}
	meson_adc_kp_get_valid_chan(kp);
	return 0;
err:
	kfree(key);
	return state;

}

static void meson_adc_kp_list_free(struct meson_adc_kp *kp)
{
	struct adc_key *key;
	struct adc_key *key_tmp;

	mutex_lock(&kp->kp_lock);
	list_for_each_entry_safe(key, key_tmp, &kp->adckey_head, list) {
		list_del(&key->list);
		kfree(key);
	}
	mutex_unlock(&kp->kp_lock);
}

static ssize_t table_show(struct class *cls, struct class_attribute *attr,
			char *buf)
{
	struct meson_adc_kp *kp = container_of(cls,
					struct meson_adc_kp, kp_class);
	struct adc_key *key;
	unsigned char key_num = 1;
	int len = 0;

	mutex_lock(&kp->kp_lock);
	list_for_each_entry(key, &kp->adckey_head, list) {
		len += sprintf(buf+len,
			"[%d]: name=%-21s code=%-5d channel=%-3d value=%-5d tolerance=%-5d\n",
			key_num,
			key->name,
			key->code,
			key->chan,
			key->value,
			key->tolerance);
		key_num++;
	}
	mutex_unlock(&kp->kp_lock);

	return len;
}

static ssize_t table_store(struct class *cls, struct class_attribute *attr,
			 const char *buf, size_t count)
{
	struct meson_adc_kp *kp = container_of(cls,
					struct meson_adc_kp, kp_class);
	struct device *dev = kp->poll_dev->input->dev.parent;
	struct adc_key *dkey;
	struct adc_key *key;
	struct adc_key *key_tmp;
	char nbuf[TMP_BUF_MAX];
	char *pbuf = nbuf;
	unsigned char colon_num = 0;
	int nsize = 0;
	int state = 0;
	char *pval;

	/*count inclued '\0'*/
	if (count > TMP_BUF_MAX) {
		dev_err(dev, "write data is too long[max:%d]: %ld\n",
			TMP_BUF_MAX, count);
		return -EINVAL;
	}

	/*trim all invisible characters include '\0', tab, space etc*/
	while (*buf) {
		if (*buf > ' ')
			nbuf[nsize++] = *buf;
		if (*buf == ':')
			colon_num++;
		buf++;
	}
	nbuf[nsize] = '\0';

	/*write "null" or "NULL" to clean up all key table*/
	if (strcasecmp("null", nbuf) == 0) {
		meson_adc_kp_list_free(kp);
		return count;
	}

	/*to judge write data format whether valid or not*/
	if (colon_num != 4) {
		dev_err(dev, "write data invalid: %s\n", nbuf);
		dev_err(dev, "=> [name]:[code]:[channel]:[value]:[tolerance]\n");
		return -EINVAL;
	}

	dkey = kzalloc(sizeof(struct adc_key), GFP_KERNEL);
	if (!dkey)
		return -ENOMEM;

	/*save the key data in order*/
	pval = strsep(&pbuf, ":"); /*name*/
	if (pval)
		strncpy(dkey->name, pval, MAX_NAME_LEN);

	pval = strsep(&pbuf, ":"); /*code*/
	if (pval)
		if (kstrtoint(pval, 0, &dkey->code) < 0) {
			state = -EINVAL;
			goto err;
		}
	pval = strsep(&pbuf, ":"); /*channel*/
	if (pval)
		if (kstrtoint(pval, 0, &dkey->chan) < 0) {
			state = -EINVAL;
			goto err;
		}
	if (!kp->pchan[dkey->chan]) {
		dev_err(dev, "invalid channel[%u], please enable it first by DTS\n",
				dkey->chan);
		state = -EINVAL;
		goto err;
	}
	pval = strsep(&pbuf, ":"); /*value*/
	if (pval)
		if (kstrtoint(pval, 0, &dkey->value) < 0) {
			state = -EINVAL;
			goto err;
		}
	pval = strsep(&pbuf, ":"); /*tolerance*/
	if (pval)
		if (kstrtoint(pval, 0, &dkey->tolerance) < 0) {
			state = -EINVAL;
			goto err;
		}

	/*check channel data whether valid or not*/
	if (dkey->chan >= SARADC_CH_NUM) {
		dev_err(dev, "invalid channel[%d-%d]: %d\n", 0,
			SARADC_CH_NUM-1, dkey->chan);
		state = -EINVAL;
		goto err;
	}

	/*check sample data whether valid or not*/
	if (dkey->value > SAM_MAX) {
		dev_err(dev, "invalid sample value[%d-%d]: %d\n",
			SAM_MIN, SAM_MAX, dkey->value);
		state = -EINVAL;
		goto err;
	}

	/*check tolerance data whether valid or not*/
	if (dkey->tolerance > TOL_MAX) {
		dev_err(dev, "invalid tolerance[%d-%d]: %d\n",
			TOL_MIN, TOL_MAX, dkey->tolerance);
		state = -EINVAL;
		goto err;
	}

	mutex_lock(&kp->kp_lock);
	list_for_each_entry_safe(key, key_tmp, &kp->adckey_head, list) {
		if ((key->code == dkey->code) ||
			((key->chan == dkey->chan) &&
			(key->value == dkey->value))) {
			dev_info(dev, "del older key => %s:%d:%d:%d:%d\n",
				key->name, key->code, key->chan,
				key->value, key->tolerance);
			clear_bit(key->code,  kp->poll_dev->input->keybit);
			list_del(&key->list);
			kfree(key);
		}
	}
	set_bit(dkey->code,  kp->poll_dev->input->keybit);
	list_add_tail(&dkey->list, &kp->adckey_head);
	dev_info(dev, "add newer key => %s:%d:%d:%d:%d\n", dkey->name,
		dkey->code, dkey->chan, dkey->value, dkey->tolerance);
	mutex_unlock(&kp->kp_lock);

	meson_adc_kp_get_valid_chan(kp);

	return count;
err:
	kfree(dkey);
	return state;
}

struct class_attribute meson_adckey_attrs[] = {
	__ATTR_RW(table),
	__ATTR_NULL
};

static void meson_adc_kp_init_keybit(struct meson_adc_kp *kp)

{
	struct adc_key *key;

	list_for_each_entry(key, &kp->adckey_head, list)
		set_bit(key->code,
			kp->poll_dev->input->keybit); /*set event code*/
}

static int meson_adc_kp_probe(struct platform_device *pdev)
{
	struct meson_adc_kp *kp;
	struct input_dev *input;
	int ret = 0;

	send_data_to_bl301();
	kernel_keypad_enable_mode_enable();

	kp = kzalloc(sizeof(struct meson_adc_kp), GFP_KERNEL);
	if (!kp)
		return -ENOMEM;

	platform_set_drvdata(pdev, kp);
	mutex_init(&kp->kp_lock);
	INIT_LIST_HEAD(&kp->adckey_head);
	kp->report_code = 0;
	kp->prev_code = 0;
	kp->count = 0;
	ret = meson_adc_kp_get_devtree_pdata(pdev, kp);
	if (ret)
		goto err;

	/*alloc input poll device*/
	kp->poll_dev = devm_input_allocate_polled_device(&pdev->dev);
	if (!kp->poll_dev) {
		dev_err(&pdev->dev, "alloc input poll device failed!\n");
		ret = -ENOMEM;
		goto err;
	}

	kp->poll_dev->poll = meson_adc_kp_poll;
	kp->poll_dev->poll_interval = kp->poll_period;
	kp->poll_dev->private = kp;
	input = kp->poll_dev->input;

	set_bit(EV_KEY, input->evbit);
	set_bit(EV_REP, input->evbit);
	meson_adc_kp_init_keybit(kp);

	input->name = "adc_keypad";
	input->phys = "adc_keypad/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_ISA;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	input->rep[REP_DELAY] = 0xffffffff;
	input->rep[REP_PERIOD] = 0xffffffff;

	input->keycodesize = sizeof(unsigned short);
	input->keycodemax = 0x1ff;

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	kp->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	kp->early_suspend.suspend = meson_adc_kp_early_suspend;
	kp->early_suspend.resume = meson_adc_kp_late_resume;
	register_early_suspend(&kp->early_suspend);
#endif

	/*init class*/
	kp->kp_class.name = DRIVE_NAME;
	kp->kp_class.owner = THIS_MODULE;
	kp->kp_class.class_attrs = meson_adckey_attrs;
	ret = class_register(&kp->kp_class);
	if (ret) {
		dev_err(&pdev->dev, "fail to create adc keypad class.\n");
		goto err;
	}

	/*register input poll device*/
	ret = input_register_polled_device(kp->poll_dev);
	if (ret) {
		dev_err(&pdev->dev,
			 "unable to register keypad input poll device.\n");
		goto err1;
	}

	return ret;

err1:
	class_unregister(&kp->kp_class);
err:
	meson_adc_kp_list_free(kp);
	kfree(kp);
	return ret;
}

static int meson_adc_kp_remove(struct platform_device *pdev)
{
	struct meson_adc_kp *kp = platform_get_drvdata(pdev);

	class_unregister(&kp->kp_class);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	unregister_early_suspend(&kp->early_suspend);
#endif
	meson_adc_kp_list_free(kp);
	kfree(kp);
	return 0;
}

static int meson_adc_kp_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	return 0;
}

static int meson_adc_kp_resume(struct platform_device *pdev)
{
	struct meson_adc_kp *kp = platform_get_drvdata(pdev);

	if (get_resume_method() == POWER_KEY_WAKEUP) {
		dev_info(&pdev->dev, "adc keypad wakeup\n");
		input_report_key(kp->poll_dev->input,  KEY_POWER,  1);
		input_sync(kp->poll_dev->input);
		input_report_key(kp->poll_dev->input,  KEY_POWER,  0);
		input_sync(kp->poll_dev->input);
	}
	return 0;
}

static const struct of_device_id key_dt_match[] = {
	{.compatible = "amlogic, adc_keypad",},
	{},
};

static struct platform_driver kp_driver = {
	.probe      = meson_adc_kp_probe,
	.remove     = meson_adc_kp_remove,
	.suspend    = meson_adc_kp_suspend,
	.resume     = meson_adc_kp_resume,
	.driver     = {
		.name   = DRIVE_NAME,
		.of_match_table = key_dt_match,
	},
};

static int __init meson_adc_kp_init(void)
{
	return platform_driver_register(&kp_driver);
}

static void __exit meson_adc_kp_exit(void)
{
	platform_driver_unregister(&kp_driver);
}

/*
 * Note: If the driver is compiled as a module, the __setup() do nothing
 *       __setup() is defined in include/linux/init.h
 */
#ifndef MODULE

static int __init adc_key_mode_para_setup(char *s)
{
	if (s)
		sprintf(adc_key_mode_name, "%s", s);

	return 0;
}
__setup("adckeyswitch=", adc_key_mode_para_setup);

static int __init kernel_keypad_enable_setup(char *s)
{
	if (s)
		sprintf(kernelkey_en_name, "%s", s);

	return 0;
}
__setup("kernelkey_enable=", kernel_keypad_enable_setup);

#endif

late_initcall(meson_adc_kp_init);
module_exit(meson_adc_kp_exit);
MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("ADC Keypad Driver");
MODULE_LICENSE("GPL");
