/*
 * drivers/amlogic/input/remote/remote_meson.c
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
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_platform.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/pm.h>
#include <linux/of_address.h>
#include "remote_meson.h"
#include <linux/amlogic/iomap.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>

static void amlremote_tasklet(unsigned long data);


DECLARE_TASKLET_DISABLED(tasklet, amlremote_tasklet, 0);

int remote_reg_read(struct remote_chip *chip, unsigned char id,
	unsigned int reg, unsigned int *val)
{
	if (id >= IR_ID_MAX) {
		dev_err(chip->dev, "invalid id:[%d] in %s\n", id, __func__);
		return -EINVAL;
	}

	*val = readl((chip->ir_contr[id].remote_regs+reg));

	return 0;
}

int remote_reg_write(struct remote_chip *chip, unsigned char id,
	unsigned int reg, unsigned int val)
{
	if (id >= IR_ID_MAX) {
		dev_err(chip->dev, "invalid id:[%d] in %s\n", id, __func__);
		return -EINVAL;
	}

	writel(val, (chip->ir_contr[id].remote_regs+reg));

	return 0;
}

int ir_scancode_sort(struct ir_map_tab *ir_map)
{
	bool is_sorted;
	u32 tmp;
	int i;
	int j;

	for (i = 0; i < ir_map->map_size - 1; i++) {
		is_sorted = true;
		for (j = 0; j < ir_map->map_size - i - 1; j++) {
			if (ir_map->codemap[j].map.scancode >
					ir_map->codemap[j+1].map.scancode) {
				is_sorted = false;
				tmp = ir_map->codemap[j].code;
				ir_map->codemap[j].code  =
						ir_map->codemap[j+1].code;
				ir_map->codemap[j+1].code = tmp;
			}
		}
		if (is_sorted)
			break;
	}

	return 0;
}

struct ir_map_tab_list *seek_map_tab(struct remote_chip *chip, int custom_code)
{
	struct ir_map_tab_list *ir_map = NULL;

	list_for_each_entry(ir_map, &chip->map_tab_head, list) {
		if (ir_map->tab.custom_code == custom_code)
			return ir_map;
	}
	return NULL;
}

void ir_tab_free(struct ir_map_tab_list *ir_map_list)
{
	kfree((void *)ir_map_list);
	ir_map_list = NULL;
}

static int ir_lookup_by_scancode(struct ir_map_tab *ir_map,
					  unsigned int scancode)
{
	int start = 0;
	int end = ir_map->map_size - 1;
	int mid;

	while (start <= end) {
		mid = (start + end) >> 1;
		if (ir_map->codemap[mid].map.scancode < scancode)
			start = mid + 1;
		else if (ir_map->codemap[mid].map.scancode > scancode)
			end = mid - 1;
		else
			return mid;
	}

	return -1;
}

static int ir_report_rel(struct remote_dev *dev, u32 scancode, int status)
{
	struct remote_chip *chip = (struct remote_chip *)dev->platform_data;
	struct ir_map_tab_list *ct = chip->cur_tab;
	static u32 repeat_count;
	s32 cursor_value = 0;
	u32 valid_scancode;
	u16 mouse_code;
	s32 move_accelerate[] = CURSOR_MOVE_ACCELERATE;

	/*nothing need to do in normal mode*/
	if (!ct || (ct->ir_dev_mode != MOUSE_MODE))
		return -EINVAL;

	if (status == REMOTE_REPEAT) {
		valid_scancode = dev->last_scancode;
		repeat_count++;
		if (repeat_count > ARRAY_SIZE(move_accelerate) - 1)
			repeat_count = ARRAY_SIZE(move_accelerate) - 1;
	} else {
		valid_scancode = scancode;
		dev->last_scancode = scancode;
		repeat_count = 0;
	}
	if (valid_scancode == ct->tab.cursor_code.cursor_left_scancode) {
		cursor_value = -(1 + move_accelerate[repeat_count]);
		mouse_code = REL_X;
	} else if (valid_scancode ==
			ct->tab.cursor_code.cursor_right_scancode) {
		cursor_value = 1 + move_accelerate[repeat_count];
		mouse_code = REL_X;
	} else if (valid_scancode ==
			ct->tab.cursor_code.cursor_up_scancode) {
		cursor_value = -(1 + move_accelerate[repeat_count]);
		mouse_code = REL_Y;
	} else if (valid_scancode ==
			ct->tab.cursor_code.cursor_down_scancode) {
		cursor_value = 1 + move_accelerate[repeat_count];
		mouse_code = REL_Y;
	} else {
		return -EINVAL;
	}
	input_event(chip->r_dev->input_device, EV_REL,
			mouse_code, cursor_value);
	input_sync(chip->r_dev->input_device);

	remote_dbg(chip->dev, "mouse cursor be %s moved %d.\n",
				mouse_code == REL_X ? "horizontal" :
					"vertical",
					cursor_value);

	return 0;
}

static u32 getkeycode(struct remote_dev *dev, u32 scancode)
{
	struct remote_chip *chip = (struct remote_chip *)dev->platform_data;
	struct ir_map_tab_list *ct = chip->cur_tab;
	int index;

	if (!ct) {
		dev_err(chip->dev, "cur_custom is nulll\n");
		return KEY_RESERVED;
	}
	/*return BTN_LEFT in mouse mode*/
	if (ct->ir_dev_mode == MOUSE_MODE &&
			scancode == ct->tab.cursor_code.cursor_ok_scancode) {
		remote_dbg(chip->dev, "mouse left button scancode: 0x%x",
					BTN_LEFT);
		return BTN_LEFT;
	}

	index = ir_lookup_by_scancode(&ct->tab, scancode);
	if (index < 0) {
		dev_err(chip->dev, "scancode %d undefined\n", scancode);
		return KEY_RESERVED;
	}

	/*save remote-control work mode*/
	if (dev->keypressed == false &&
			scancode == ct->tab.cursor_code.fn_key_scancode) {
		if (ct->ir_dev_mode == NORMAL_MODE)
			ct->ir_dev_mode = MOUSE_MODE;
		else
			ct->ir_dev_mode = NORMAL_MODE;
		dev_info(chip->dev, "remote control[ID: 0x%x] switch to %s\n",
					ct->tab.custom_code,
					ct->ir_dev_mode ?
					"mouse mode":"normal mode");
	}

	return ct->tab.codemap[index].map.keycode;
}

static bool is_valid_custom(struct remote_dev *dev)
{
	struct remote_chip *chip = (struct remote_chip *)dev->platform_data;
	int custom_code;

	if (!chip->ir_contr[chip->ir_work].get_custom_code)
		return true;
	custom_code = chip->ir_contr[chip->ir_work].get_custom_code(chip);
	chip->cur_tab = seek_map_tab(chip, custom_code);
	if (chip->cur_tab) {
		dev->keyup_delay = chip->cur_tab->tab.release_delay;
		return true;
	}
	return false;
}

static bool is_next_repeat(struct remote_dev *dev)
{
	unsigned int val = 0;
	unsigned char fbusy = 0;
	unsigned char cnt;

	struct remote_chip *chip = (struct remote_chip *)dev->platform_data;

	for (cnt = 0; cnt < (ENABLE_LEGACY_IR(chip->protocol) ? 2:1); cnt++) {
		remote_reg_read(chip, cnt, REG_STATUS, &val);
		fbusy |= IR_CONTROLLER_BUSY(val);
	}
	remote_dbg(chip->dev, "ir controller busy flag = %d\n", fbusy);
	if (!dev->wait_next_repeat && fbusy)
		return true;
	else
		return false;
}

static bool set_custom_code(struct remote_dev *dev, u32 code)
{
	struct remote_chip *chip = (struct remote_chip *)dev->platform_data;

	return chip->ir_contr[chip->ir_work].set_custom_code(chip, code);
}

static void amlremote_tasklet(unsigned long data)
{
	struct remote_chip *chip = (struct remote_chip *)data;
	unsigned long flags;
	int status = -1;
	int scancode = -1;

	/**
	 *need first get_scancode, then get_decode_status, the status
	 *may was set flag from get_scancode function
	 */
	spin_lock_irqsave(&chip->slock, flags);
	if (chip->ir_contr[chip->ir_work].get_scancode)
		scancode = chip->ir_contr[chip->ir_work].get_scancode(chip);
	if (chip->ir_contr[chip->ir_work].get_decode_status)
		status = chip->ir_contr[chip->ir_work].get_decode_status(chip);
	if (status == REMOTE_NORMAL) {
		remote_dbg(chip->dev, "receive scancode=0x%x\n", scancode);
		remote_keydown(chip->r_dev, scancode, status);
	} else if (status & REMOTE_REPEAT) {
		remote_dbg(chip->dev, "receive repeat\n");
		remote_keydown(chip->r_dev, scancode, status);
	} else
		dev_err(chip->dev, "receive error %d\n", status);
	spin_unlock_irqrestore(&chip->slock, flags);

}

static irqreturn_t ir_interrupt(int irq, void *dev_id)
{
	struct remote_chip *rc = (struct remote_chip *)dev_id;
	int contr_status = 0;
	int val = 0;
	u32 duration;
	char buf[50];
	unsigned char cnt;
	enum raw_event_type type = RAW_SPACE;

	remote_reg_read(rc, MULTI_IR_ID, REG_REG1, &val);
	val = (val & 0x1FFF0000) >> 16;
	sprintf(buf, "d:%d\n", val);
	debug_log_printk(rc->r_dev, buf);
	/**
	 *software decode multiple protocols by using Time Measurement of
	 *multif-format IR controller
	 */
	if (MULTI_IR_SOFTWARE_DECODE(rc->protocol)) {
		rc->ir_work = MULTI_IR_ID;
		duration = val*10*1000;
		type    = RAW_PULSE;
		sprintf(buf, "------\n");
		debug_log_printk(rc->r_dev, buf);
		remote_raw_event_store_edge(rc->r_dev, type, duration);
		remote_raw_event_handle(rc->r_dev);
	} else {
		for (cnt = 0; cnt < (ENABLE_LEGACY_IR(rc->protocol)
				? 2:1); cnt++) {
			remote_reg_read(rc, cnt, REG_STATUS, &contr_status);
			if (IR_DATA_IS_VALID(contr_status)) {
				rc->ir_work = cnt;
				break;
			}
		}

		if (cnt == IR_ID_MAX) {
			dev_err(rc->dev, "invalid interrupt.\n");
			return IRQ_HANDLED;
		}

		tasklet_schedule(&tasklet);
	}
	return IRQ_HANDLED;
}

static int get_custom_tables(struct device_node *node,
	struct remote_chip *chip)
{
	const phandle *phandle;
	struct device_node *custom_maps, *map;
	u32 value;
	int ret = -1;
	int index;
	char *propname;
	const char *uname;
	unsigned long flags;
	struct ir_map_tab_list *ptable;

	phandle = of_get_property(node, "map", NULL);
	if (!phandle) {
		dev_err(chip->dev, "%s:don't find match custom\n", __func__);
		return -1;
	}

	custom_maps = of_find_node_by_phandle(be32_to_cpup(phandle));
	if (!custom_maps) {
		dev_err(chip->dev, "can't find device node key\n");
		return -1;
	}

	ret = of_property_read_u32(custom_maps, "mapnum", &value);
	if (ret) {
		dev_err(chip->dev, "please config correct mapnum item\n");
		return -1;
	}
	chip->custom_num = value;
	if (chip->custom_num > CUSTOM_NUM_MAX)
		chip->custom_num = CUSTOM_NUM_MAX;

	dev_info(chip->dev, "custom_number = %d\n", chip->custom_num);

	for (index = 0; index < chip->custom_num; index++) {
		propname = kasprintf(GFP_KERNEL, "map%d", index);
		phandle = of_get_property(custom_maps, propname, NULL);
		if (!phandle) {
			dev_err(chip->dev, "%s:don't find match map%d\n",
					__func__, index);
			return -1;
		}
		map = of_find_node_by_phandle(be32_to_cpup(phandle));
		if (!map) {
			dev_err(chip->dev, "can't find device node key\n");
			return -1;
		}

		ret = of_property_read_u32(map, "size", &value);
		if (ret || value > MAX_KEYMAP_SIZE) {
			dev_err(chip->dev, "no config size item or err\n");
			return -1;
		}

		/*alloc memory*/
		ptable = kzalloc(sizeof(struct ir_map_tab_list) +
				    value * sizeof(union _codemap), GFP_KERNEL);
		if (!ptable)
			return -ENOMEM;

		ptable->tab.map_size = value;
		dev_info(chip->dev, "ptable->map_size = %d\n",
							ptable->tab.map_size);

		ret = of_property_read_string(map, "mapname", &uname);
		if (ret) {
			dev_err(chip->dev, "please config mapname item\n");
			goto err;
		}
		strncpy(ptable->tab.custom_name, uname, CUSTOM_NAME_LEN);

		dev_info(chip->dev, "ptable->custom_name = %s\n",
						ptable->tab.custom_name);

		ret = of_property_read_u32(map, "customcode", &value);
		if (ret) {
			dev_err(chip->dev, "please config customcode item\n");
			goto err;
		}
		ptable->tab.custom_code = value;
		dev_info(chip->dev, "ptable->custom_code = 0x%x\n",
						ptable->tab.custom_code);

		ret = of_property_read_u32(map, "release_delay", &value);
		if (ret) {
			dev_err(chip->dev, "remote:don't find the node <release_delay>\n");
			goto err;
		}
		ptable->tab.release_delay = value;
		dev_info(chip->dev, "ptable->release_delay = %d\n",
						ptable->tab.release_delay);

		ret = of_property_read_u32_array(map,
				"keymap", (u32 *)&ptable->tab.codemap[0],
					ptable->tab.map_size);
		if (ret) {
			dev_err(chip->dev, "please config keymap item\n");
			goto err;
		}

		memset(&ptable->tab.cursor_code, 0xff,
					sizeof(struct cursor_codemap));
		ir_scancode_sort(&ptable->tab);
		/*insert list*/
		spin_lock_irqsave(&chip->slock, flags);
		list_add_tail(&ptable->list, &chip->map_tab_head);
		spin_unlock_irqrestore(&chip->slock, flags);

	}
	return 0;
err:
	ir_tab_free(ptable);
	return -1;
}


static int ir_get_devtree_pdata(struct platform_device *pdev)
{
	struct resource *res_irq;
	struct resource *res_mem;
	resource_size_t *res_start[2];
	struct pinctrl *p;
	int ret;
	int value;
	unsigned char i;


	struct remote_chip *chip = platform_get_drvdata(pdev);

	ret = of_property_read_u32(pdev->dev.of_node,
			"protocol", &chip->protocol);
	if (ret) {
		dev_err(chip->dev, "don't find the node <protocol>\n");
		chip->protocol = 1;
	}
	dev_info(chip->dev, "protocol = 0x%x\n", chip->protocol);

	p = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(p)) {
		dev_err(chip->dev, "pinctrl error, %ld\n", PTR_ERR(p));
		return -1;
	}

	for (i = 0; i < 2; i++) {
		res_mem = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (IS_ERR_OR_NULL(res_mem)) {
			dev_err(chip->dev, "get IORESOURCE_MEM error, %ld\n",
					PTR_ERR(p));
			return PTR_ERR(res_mem);
		}
		res_start[i] = devm_ioremap_resource(&pdev->dev, res_mem);
		chip->ir_contr[i].remote_regs = (void __iomem *)res_start[i];
	}

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (IS_ERR_OR_NULL(res_irq)) {
		dev_err(chip->dev, "get IORESOURCE_IRQ error, %ld\n",
				PTR_ERR(p));
		return PTR_ERR(res_irq);
	}

	chip->irqno = res_irq->start;

	dev_info(chip->dev, "platform_data irq =%d\n", chip->irqno);

	ret = of_property_read_u32(pdev->dev.of_node,
				"max_frame_time", &value);
	if (ret) {
		dev_err(chip->dev, "don't find the node <max_frame_time>\n");
		value = 200; /*default value*/
	}

	chip->r_dev->max_frame_time = value;


	/*create map table */
	ret = get_custom_tables(pdev->dev.of_node, chip);
	if (ret < 0)
		return -1;

	return 0;
}

static int ir_hardware_init(struct platform_device *pdev)
{
	int ret;

	struct remote_chip *chip = platform_get_drvdata(pdev);

	if (!pdev->dev.of_node) {
		dev_err(chip->dev, "pdev->dev.of_node == NULL!\n");
		return -1;
	}

	ret = ir_get_devtree_pdata(pdev);
	if (ret < 0)
		return ret;
	chip->set_register_config(chip, chip->protocol);
	ret = request_irq(chip->irqno, ir_interrupt, IRQF_SHARED,
				"keypad", (void *)chip);
	if (ret < 0)
		goto error_irq;

	chip->irq_cpumask = 1;
	irq_set_affinity(chip->irqno, cpumask_of(chip->irq_cpumask));

	tasklet_enable(&tasklet);
	tasklet.data = (unsigned long)chip;

	return 0;

error_irq:
	dev_err(chip->dev, "request_irq error %d\n", ret);

	return ret;

}

static int ir_hardware_free(struct platform_device *pdev)
{
	struct remote_chip *chip = platform_get_drvdata(pdev);

	free_irq(chip->irqno, chip);
	return 0;
}

static void ir_input_device_init(struct input_dev *dev,
	struct device *parent, const char *name)
{
	dev->name = name;
	dev->phys = "keypad/input0";
	dev->dev.parent = parent;
	dev->id.bustype = BUS_ISA;
	dev->id.vendor  = 0x0001;
	dev->id.product = 0x0001;
	dev->id.version = 0x0100;
	dev->rep[REP_DELAY] = 0xffffffff;  /*close input repeat*/
	dev->rep[REP_PERIOD] = 0xffffffff; /*close input repeat*/
}

static int remote_probe(struct platform_device *pdev)
{
	struct remote_dev *dev;
	int ret;
	struct remote_chip *chip;

	pr_info("%s: remote_probe\n", DRIVER_NAME);
	chip = kzalloc(sizeof(struct remote_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("%s: kzalloc remote_chip error!\n", DRIVER_NAME);
		ret = -ENOMEM;
		goto err_end;
	}

	dev = remote_allocate_device();
	if (!dev) {
		pr_err("%s: kzalloc remote_dev error!\n", DRIVER_NAME);
		ret = -ENOMEM;
		goto err_alloc_remote_dev;
	}

	mutex_init(&chip->file_lock);
	spin_lock_init(&chip->slock);
	INIT_LIST_HEAD(&chip->map_tab_head);

	chip->r_dev = dev;
	chip->dev = &pdev->dev;

	chip->r_dev->dev = &pdev->dev;
	chip->r_dev->platform_data = (void *)chip;
	chip->r_dev->getkeycode    = getkeycode;
	chip->r_dev->ir_report_rel = ir_report_rel;
	chip->r_dev->set_custom_code = set_custom_code;
	chip->r_dev->is_valid_custom = is_valid_custom;
	chip->r_dev->is_next_repeat  = is_next_repeat;
	chip->set_register_config = ir_register_default_config;
	platform_set_drvdata(pdev, chip);

	ir_input_device_init(dev->input_device, &pdev->dev, "aml_keypad");

	ret = ir_hardware_init(pdev);
	if (ret < 0)
		goto err_hard_init;

	ret = ir_cdev_init(chip);
	if (ret < 0)
		goto err_cdev_init;

	dev->rc_type = chip->protocol;
	ret = remote_register_device(dev);
	if (ret)
		goto error_register_remote;

	device_init_wakeup(&pdev->dev, 1);
	dev_pm_set_wake_irq(&pdev->dev, chip->irqno);

	return 0;

error_register_remote:
	ir_hardware_free(pdev);
err_cdev_init:
	remote_free_device(dev);
err_hard_init:
	ir_cdev_free(chip);
err_alloc_remote_dev:
	kfree(chip);
err_end:
	return ret;
}

static int remote_remove(struct platform_device *pdev)
{
	struct remote_chip *chip = platform_get_drvdata(pdev);

	tasklet_disable(&tasklet);
	tasklet_kill(&tasklet);

	free_irq(chip->irqno, chip); /*irq dev_id is chip address*/
	ir_cdev_free(chip);
	remote_unregister_device(chip->r_dev);
	remote_free_device(chip->r_dev);

	kfree(chip);
	return 0;
}

static int remote_resume(struct device *dev)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	unsigned int val;
	unsigned long flags;
	unsigned char cnt;

	dev_info(dev, "remote resume\n");
	/*resume register config*/
	spin_lock_irqsave(&chip->slock, flags);
	chip->set_register_config(chip, chip->protocol);
	/* read REG_STATUS and REG_FRAME to clear status */
	for (cnt = 0; cnt < (ENABLE_LEGACY_IR(chip->protocol) ? 2:1); cnt++) {
		remote_reg_read(chip, cnt, REG_STATUS, &val);
		remote_reg_read(chip, cnt, REG_FRAME, &val);
	}
	spin_unlock_irqrestore(&chip->slock, flags);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	if (get_resume_method() == REMOTE_WAKEUP) {
		input_event(chip->r_dev->input_device,
		    EV_KEY, KEY_POWER, 1);
		input_sync(chip->r_dev->input_device);
		input_event(chip->r_dev->input_device,
		    EV_KEY, KEY_POWER, 0);
		input_sync(chip->r_dev->input_device);
	}

	if (get_resume_method() == REMOTE_CUS_WAKEUP) {
		input_event(chip->r_dev->input_device, EV_KEY, 133, 1);
		input_sync(chip->r_dev->input_device);
		input_event(chip->r_dev->input_device, EV_KEY, 133, 0);
		input_sync(chip->r_dev->input_device);
	}
#endif

	irq_set_affinity(chip->irqno, cpumask_of(chip->irq_cpumask));
	enable_irq(chip->irqno);
	return 0;
}

static int remote_suspend(struct device *dev)
{
	struct remote_chip *chip = dev_get_drvdata(dev);

	dev_info(dev, "remote suspend\n");
	disable_irq(chip->irqno);
	return 0;
}

static const struct of_device_id remote_dt_match[] = {
	{
		.compatible     = "amlogic, aml_remote",
	},
	{},
};

#ifdef CONFIG_PM
static const struct dev_pm_ops remote_pm_ops = {
	.suspend_late = remote_suspend,
	.resume_early = remote_resume,
};
#endif

static struct platform_driver remote_driver = {
	.probe = remote_probe,
	.remove = remote_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = remote_dt_match,
#ifdef CONFIG_PM
		.pm = &remote_pm_ops,
#endif
	},
};

static int __init remote_init(void)
{
	pr_info("%s: Driver init\n", DRIVER_NAME);
	return platform_driver_register(&remote_driver);
}

static void __exit remote_exit(void)
{
	pr_info("%s: Driver exit\n", DRIVER_NAME);
	platform_driver_unregister(&remote_driver);
}

module_init(remote_init);
module_exit(remote_exit);


MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("AMLOGIC REMOTE PROTOCOL");
MODULE_LICENSE("GPL");
