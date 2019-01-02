/*
 * drivers/amlogic/media/camera/common/plat_ctrl.h
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

#ifndef _AMLOGIC_CAMERA_PLAT_CTRL_H
#define _AMLOGIC_CAMERA_PLAT_CTRL_H

#define ADDR8_DATA8		0
#define ADDR16_DATA8		1
#define ADDR16_DATA16		2
#define ADDR8_DATA16		3
#define TIME_DELAY		0xfe
#define END_OF_SCRIPT			0xff

struct cam_i2c_msg_s {
	unsigned char type;
	unsigned short addr;
	unsigned short data;
};

extern int i2c_get_byte(struct i2c_client *client, unsigned short addr);
extern int i2c_get_word(struct i2c_client *client, unsigned short addr);
extern int i2c_get_byte_add8(struct i2c_client *client, unsigned char addr);
extern int i2c_put_byte(struct i2c_client *client, unsigned short addr,
			unsigned char data);
extern int i2c_put_word(struct i2c_client *client, unsigned short addr,
			unsigned short data);
extern int i2c_put_byte_add8_new(struct i2c_client *client, unsigned char addr,
				 unsigned char data);
extern int i2c_put_byte_add8(struct i2c_client *client, char *buf, int len);
extern int cam_i2c_send_msg(struct i2c_client *client,
			struct cam_i2c_msg_s i2c_msg);

#endif /* _AMLOGIC_CAMERA_PLAT_CTRL_H. */
