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
#include <linux/spi/spi.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>

/* ldim spi api*/
extern int ldim_spi_write(struct spi_device *spi, unsigned char *tbuf,
		int wlen);
extern int ldim_spi_read(struct spi_device *spi, unsigned char *tbuf, int wlen,
		unsigned char *rbuf, int rlen);
extern int ldim_spi_driver_add(struct aml_ldim_driver_s *ldim_drv);
extern int ldim_spi_driver_remove(struct aml_ldim_driver_s *ldim_drv);

/* ldim global api */
extern void ldim_gpio_set(int index, int value);
extern unsigned int ldim_gpio_get(int index);
extern void ldim_set_duty_pwm(struct bl_pwm_config_s *ld_pwm);
extern void ldim_pwm_off(struct bl_pwm_config_s *ld_pwm);

/* ldim dev api */
extern int ldim_dev_iw7027_probe(struct aml_ldim_driver_s *ldim_drv);
extern int ldim_dev_iw7027_remove(struct aml_ldim_driver_s *ldim_drv);

extern int ldim_dev_ob3350_probe(struct aml_ldim_driver_s *ldim_drv);
extern int ldim_dev_ob3350_remove(struct aml_ldim_driver_s *ldim_drv);

extern int ldim_dev_global_probe(struct aml_ldim_driver_s *ldim_drv);
extern int ldim_dev_global_remove(struct aml_ldim_driver_s *ldim_drv);


#endif /* __LDIM_DEV_DRV_H */



