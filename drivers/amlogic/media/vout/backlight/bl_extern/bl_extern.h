/*
 * drivers/amlogic/media/vout/backlight/bl_extern/bl_extern.h
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

#ifndef _BL_EXTERN_H_
#define _BL_EXTERN_H_
#include <linux/i2c.h>
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>


extern unsigned int bl_debug_print_flag;
#define BLEX(fmt, args...)     pr_info("bl extern: "fmt"", ## args)
#define BLEXERR(fmt, args...)  pr_err("bl extern: error: "fmt"", ## args)

#define BL_EXTERN_DRIVER	"bl_extern"

struct aml_bl_extern_i2c_dev_s {
	char name[20];
	struct i2c_client *client;
};

extern struct aml_bl_extern_i2c_dev_s *aml_bl_extern_i2c_get_dev(void);

extern int bl_extern_i2c_write(struct i2c_client *i2client,
		unsigned char *buff, unsigned int len);
extern int bl_extern_i2c_read(struct i2c_client *i2client,
		unsigned char *buff, unsigned int len);


extern int bl_ext_default_probe(struct aml_bl_extern_driver_s *bl_ext);
extern int bl_ext_default_remove(struct aml_bl_extern_driver_s *bl_ext);

#ifdef CONFIG_AMLOGIC_BL_EXTERN_I2C_LP8556
extern int i2c_lp8556_probe(void);
extern int i2c_lp8556_remove(void);
#endif

#ifdef CONFIG_AMLOGIC_BL_EXTERN_MIPI_LT070ME05
extern int mipi_lt070me05_probe(void);
extern int mipi_lt070me05_remove(void);
#endif


#endif

