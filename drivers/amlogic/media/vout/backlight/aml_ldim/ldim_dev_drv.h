/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ldim_dev_drv.h
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

#ifndef __LDIM_DEV_DRV_H
#define __LDIM_DEV_DRV_H

extern void ldim_gpio_set(int index, int value);
extern unsigned int ldim_gpio_get(int index);

extern int ldim_dev_iw7027_probe(void);
extern int ldim_dev_iw7027_remove(void);
extern int ldim_dev_ob3350_probe(void);
extern int ldim_dev_ob3350_remove(void);
extern void ldim_set_duty_pwm(struct bl_pwm_config_s *ld_pwm);
extern int ldim_dev_global_probe(void);
extern int ldim_dev_global_remove(void);


#endif /* __LDIM_DEV_DRV_H */



