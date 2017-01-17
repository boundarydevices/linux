/*
 * include/linux/amlogic/media/vout/lcd_notify.h
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

#ifndef _INC_LCD_NOTIFY_H_
#define _INC_LCD_NOTIFY_H_

#include <linux/notifier.h>

#define LCD_PRIORITY_INIT_CONFIG       4
#define LCD_PRIORITY_INIT_VOUT         3

#define LCD_PRIORITY_POWER_BL_OFF      2
#define LCD_PRIORITY_POWER_LCD         1
#define LCD_PRIORITY_POWER_BL_ON       0

#define LCD_EVENT_BL_ON             (1 << 0)
#define LCD_EVENT_LCD_ON            (1 << 1)
#define LCD_EVENT_POWER_ON          (LCD_EVENT_BL_ON | LCD_EVENT_LCD_ON)
#define LCD_EVENT_BL_OFF            (1 << 2)
#define LCD_EVENT_LCD_OFF           (1 << 3)
#define LCD_EVENT_POWER_OFF         (LCD_EVENT_BL_OFF | LCD_EVENT_LCD_OFF)

/* lcd backlight index select */
#define LCD_EVENT_BACKLIGHT_SEL     (1 << 4)

/* lcd backlight pwm_vs vfreq change occurred */
#define LCD_EVENT_BACKLIGHT_UPDATE  (1 << 5)

#define LCD_EVENT_GAMMA_UPDATE  (1 << 6)

/* lcd frame rate change occurred */
#define LCD_EVENT_FRAME_RATE_ADJUST (1 << 8)

/* lcd config change occurred */
#define LCD_EVENT_CONFIG_UPDATE     (1 << 9)

/* lcd bist pattern test occurred */
#define LCD_EVENT_TEST_PATTERN      (1 << 10)

extern int aml_lcd_notifier_register(struct notifier_block *nb);
extern int aml_lcd_notifier_unregister(struct notifier_block *nb);
extern int aml_lcd_notifier_call_chain(unsigned long event, void *v);

#endif /* _INC_LCD_NOTIFY_H_ */
