/*
 * drivers/amlogic/media/vout/lcd/lcd_notify.c
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
#include <linux/notifier.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>

static BLOCKING_NOTIFIER_HEAD(lcd_notifier_list);

/**
 * aml_lcd_notifier_register - register a client notifier
 * @nb: notifier block to callback on events
 */
int aml_lcd_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&lcd_notifier_list, nb);
}
EXPORT_SYMBOL(aml_lcd_notifier_register);

/**
 * aml_lcd_notifier_unregister - unregister a client notifier
 * @nb: notifier block to callback on events
 */
int aml_lcd_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&lcd_notifier_list, nb);
}
EXPORT_SYMBOL(aml_lcd_notifier_unregister);

/**
 * aml_lcd_notifier_call_chain - notify clients of lcd events
 *
 */
int aml_lcd_notifier_call_chain(unsigned long event, void *v)
{
	return blocking_notifier_call_chain(&lcd_notifier_list, event, v);
}
EXPORT_SYMBOL_GPL(aml_lcd_notifier_call_chain);
