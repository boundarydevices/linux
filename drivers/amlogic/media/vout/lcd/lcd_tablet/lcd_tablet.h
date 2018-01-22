/*
 * drivers/amlogic/media/vout/lcd/lcd_tablet/lcd_tablet.h
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

#ifndef __AML_LCD_TABLET_H__
#define __AML_LCD_TABLET_H__

extern void lcd_tablet_config_update(struct lcd_config_s *pconf);
extern void lcd_tablet_config_post_update(struct lcd_config_s *pconf);
extern void lcd_tablet_driver_init_pre(void);
extern void lcd_tablet_driver_disable_post(void);
extern int lcd_tablet_driver_init(void);
extern void lcd_tablet_driver_disable(void);

#endif
