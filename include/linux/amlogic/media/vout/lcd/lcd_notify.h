/*
 * include/linux/amlogic/media/vout/lcd/lcd_notify.h
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

#define LCD_PRIORITY_INIT_CONFIG       12
#define LCD_PRIORITY_INIT_VOUT         11

#define LCD_PRIORITY_SCREEN_BLACK      7
#define LCD_PRIORITY_POWER_BL_OFF      6
#define LCD_PRIORITY_POWER_IF_OFF      5
#define LCD_PRIORITY_POWER_ENCL_OFF    4
#define LCD_PRIORITY_POWER_ENCL_ON     3
#define LCD_PRIORITY_POWER_IF_ON       2
#define LCD_PRIORITY_POWER_BL_ON       1
#define LCD_PRIORITY_SCREEN_RESTORE    0

/* orignal event */
#define LCD_EVENT_SCREEN_BLACK      (1 << 0)
#define LCD_EVENT_BL_OFF            (1 << 1)
#define LCD_EVENT_IF_OFF            (1 << 2)
#define LCD_EVENT_ENCL_OFF          (1 << 3)
#define LCD_EVENT_ENCL_ON           (1 << 4)
#define LCD_EVENT_IF_ON             (1 << 5)
#define LCD_EVENT_BL_ON             (1 << 6)
#define LCD_EVENT_SCREEN_RESTORE    (1 << 7)

/* combined event */
#define LCD_EVENT_POWER_ON          (LCD_EVENT_BL_ON | LCD_EVENT_IF_ON | \
				LCD_EVENT_ENCL_ON | LCD_EVENT_SCREEN_RESTORE)
#define LCD_EVENT_IF_POWER_ON       (LCD_EVENT_IF_ON | LCD_EVENT_BL_ON | \
				LCD_EVENT_SCREEN_RESTORE)
#define LCD_EVENT_POWER_OFF         (LCD_EVENT_SCREEN_BLACK | \
				LCD_EVENT_BL_OFF | LCD_EVENT_IF_OFF | \
				LCD_EVENT_ENCL_OFF)
#define LCD_EVENT_IF_POWER_OFF      (LCD_EVENT_SCREEN_BLACK | \
				LCD_EVENT_BL_OFF | LCD_EVENT_IF_OFF)

#define LCD_EVENT_PREPARE           (LCD_EVENT_ENCL_ON)
#define LCD_EVENT_ENABLE            (LCD_EVENT_IF_ON | LCD_EVENT_BL_ON | \
				LCD_EVENT_SCREEN_RESTORE)
#define LCD_EVENT_DISABLE           (LCD_EVENT_SCREEN_BLACK | \
				LCD_EVENT_BL_OFF | LCD_EVENT_IF_OFF)
#define LCD_EVENT_UNPREPARE         (LCD_EVENT_ENCL_OFF)

/* lcd backlight index select */
#define LCD_EVENT_BACKLIGHT_SEL     (1 << 8)
/* lcd backlight pwm_vs vfreq change occurred */
#define LCD_EVENT_BACKLIGHT_UPDATE  (1 << 9)

#define LCD_EVENT_GAMMA_UPDATE      (1 << 10)
#define LCD_EVENT_EXTERN_SEL        (1 << 11)

/* lcd frame rate change occurred */
#define LCD_EVENT_FRAME_RATE_ADJUST (1 << 12)
/* lcd config change occurred */
#define LCD_EVENT_CONFIG_UPDATE     (1 << 13)
/* lcd bist pattern test occurred */
#define LCD_EVENT_TEST_PATTERN      (1 << 14)

#define LCD_VLOCK_PARAM_NUM         5
#define LCD_VLOCK_PARAM_BIT_UPDATE  (1 << 4)
#define LCD_VLOCK_PARAM_BIT_VALID   (1 << 0)
#define LCD_EVENT_VLOCK_PARAM       (1 << 16)


extern int aml_lcd_notifier_register(struct notifier_block *nb);
extern int aml_lcd_notifier_unregister(struct notifier_block *nb);
extern int aml_lcd_notifier_call_chain(unsigned long event, void *v);

#endif /* _INC_LCD_NOTIFY_H_ */
