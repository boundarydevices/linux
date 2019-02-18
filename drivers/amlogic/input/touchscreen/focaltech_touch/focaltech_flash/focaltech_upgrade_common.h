/*
 * focaltech_upgrade_common.h
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

#ifndef __LINUX_FOCALTECH_UPGRADE_COMMON_H__
#define __LINUX_FOCALTECH_UPGRADE_COMMON_H__

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "../focaltech_flash.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/

/*****************************************************************************
* Static variables
*****************************************************************************/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
int fts_ctpm_erase_flash(struct i2c_client *client);
int fts_ctpm_pramboot_ecc(struct i2c_client *client);
bool fts_ctpm_check_run_state(struct i2c_client *client, int state);
void fts_ctpm_start_pramboot(struct i2c_client *client);
int fts_ctpm_start_fw_upgrade(struct i2c_client *client);
bool fts_ctpm_check_in_pramboot(struct i2c_client *client);
int fts_ctpm_upgrade_idc_init(struct i2c_client *client);
int fts_ctpm_write_app_for_idc(struct i2c_client *client, u32 length, u8 *readbuf);
int fts_ctpm_upgrade_ecc(struct i2c_client *client, u32 startaddr, u32 length);
int fts_ctpm_write_pramboot_for_idc(struct i2c_client *client, u32 length, u8 *readbuf);
int fts_writeflash(struct i2c_client *client, u32 writeaddr, u32 length, u8 *readbuf, u32 cnt);

int fts_ctpm_get_app_ver(void);
int fts_ctpm_fw_upgrade(struct i2c_client *client);
int fts_ctpm_lcd_cfg_upgrade(struct i2c_client *client);

#endif
