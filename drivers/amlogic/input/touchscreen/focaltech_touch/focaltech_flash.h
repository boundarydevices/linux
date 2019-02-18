/*
 * drivers/amlogic/input/touchscreen/focaltech_touch/focaltech_flash.h
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

#ifndef __LINUX_FOCALTECH_FLASH_H__
#define __LINUX_FOCALTECH_FLASH_H__

/*****************************************************************************
* 1.Included header files
*****************************************************************************/

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_REG_ECC                                  0xCC
#define FTS_RST_CMD_REG2                             0xBC
#define FTS_READ_ID_REG                              0x90
#define FTS_ERASE_APP_REG                            0x61
#define FTS_ERASE_PARAMS_CMD                         0x63
#define FTS_FW_WRITE_CMD                             0xBF
#define FTS_REG_RESET_FW                             0x07
#define FTS_RST_CMD_REG1                             0xFC
#define LEN_FLASH_ECC_MAX                            0xFFFE

#define FTS_PACKET_LENGTH                            128
#define FTS_SETTING_BUF_LEN                          128

#define FTS_UPGRADE_LOOP                             30
#define AUTO_CLB_NEED                                1
#define AUTO_CLB_NONEED                              0
#define FTS_UPGRADE_AA                               0xAA
#define FTS_UPGRADE_55                               0x55
#define FTXXXX_INI_FILEPATH_CONFIG                   "/sdcard/"

enum FW_STATUS
{
    FTS_RUN_IN_ERROR,
    FTS_RUN_IN_APP,
    FTS_RUN_IN_ROM,
    FTS_RUN_IN_PRAM,
    FTS_RUN_IN_BOOTLOADER
};

enum FILE_SIZE_TYPE
{
    FW_SIZE,
    FW2_SIZE,
    FW3_SIZE,
    PRAMBOOT_SIZE,
};

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
/* IC info */

struct fts_upgrade_fun
{
    int (*get_i_file)(struct i2c_client *, int);
    int (*get_app_bin_file_ver)(char *);
    int (*get_app_i_file_ver)(void);
    int (*upgrade_with_app_i_file)(struct i2c_client *);
    int (*upgrade_with_app_bin_file)(struct i2c_client *, char *);
    int (*get_hlic_ver)(void);
    int (*upgrade_with_lcd_cfg_i_file)(struct i2c_client *);
    int (*upgrade_with_lcd_cfg_bin_file)(struct i2c_client *, char *);
};
extern struct fts_upgrade_fun fts_updatefun;

/*****************************************************************************
* Static variables
*****************************************************************************/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
extern u8 CTPM_FW[];
extern u8 CTPM_FW2[];
extern u8 CTPM_FW3[];
extern u8 aucFW_PRAM_BOOT[];
extern u8 CTPM_LCD_CFG[];
extern u8 *g_fw_file;
extern int g_fw_len;
extern struct fts_upgrade_fun  fts_updatefun_curr;
extern struct ft_chip_t chip_types;

#if FTS_AUTO_UPGRADE_EN
extern struct workqueue_struct *touch_wq;
extern struct work_struct fw_update_work;
#endif

void fts_ctpm_upgrade_init(void);
void fts_ctpm_upgrade_exit(void);
void fts_ctpm_upgrade_delay(u32 i);
void fts_ctpm_get_upgrade_array(void);
int fts_ctpm_auto_upgrade(struct i2c_client *client);
int fts_fw_upgrade(struct device *dev, bool force);
int fts_ctpm_auto_clb(struct i2c_client *client);

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
u32 fts_getsize(u8 fw_type);
int fts_GetFirmwareSize(char *firmware_name);
int fts_ctpm_i2c_hid2std(struct i2c_client *client);
int fts_ReadFirmware(char *firmware_name,u8 *firmware_buf);
void fts_ctpm_rom_or_pram_reset(struct i2c_client *client);
enum FW_STATUS fts_ctpm_get_pram_or_rom_id(struct i2c_client *client);

int fts_ctpm_erase_flash(struct i2c_client *client);
int fts_ctpm_pramboot_ecc(struct i2c_client *client);
bool fts_ctpm_check_run_state(struct i2c_client *client, int state);
void fts_ctpm_start_pramboot(struct i2c_client *client);
int fts_ctpm_start_fw_upgrade(struct i2c_client *client);
bool fts_ctpm_check_in_pramboot(struct i2c_client *client);
int fts_ctpm_upgrade_idc_init(struct i2c_client *client);
int fts_ctpm_write_app_for_idc(struct i2c_client *client,
	u32 length, u8 *readbuf);
int fts_ctpm_upgrade_ecc(struct i2c_client *client, u32 startaddr, u32 length);
int fts_ctpm_write_pramboot_for_idc(struct i2c_client *client,
	u32 length, u8 *readbuf);
int fts_writeflash(struct i2c_client *client, u32 writeaddr, u32 length,
	u8 *readbuf, u32 cnt);

int fts_ctpm_get_app_ver(void);
int fts_ctpm_fw_upgrade(struct i2c_client *client);
int fts_ctpm_lcd_cfg_upgrade(struct i2c_client *client);

#endif


