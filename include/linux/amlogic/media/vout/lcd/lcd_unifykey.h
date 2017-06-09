/*
 * include/linux/amlogic/media/vout/lcd/lcd_unifykey.h
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

#ifndef _INC_AML_LCD_UNIFYKEY_H__
#define _INC_AML_LCD_UNIFYKEY_H__

#define LCD_UNIFYKEY_WAIT_TIMEOUT    300

/* declare external unifykey function */
extern int key_unify_read(char *keyname, unsigned char *keydata,
	unsigned int datalen, unsigned int *reallen);
extern int key_unify_size(char *keyname, unsigned int *reallen);
extern int key_unify_query(char *keyname, unsigned int *keystate,
	unsigned int *keypermit);
extern int key_unify_get_init_flag(void);


#define LCD_UKEY_RETRY_CNT_MAX   5
/*
 *  lcd unifykey data struct: little-endian, for example:
 *    4byte: d[0]=0x01, d[1]=0x02, d[2] = 0x03, d[3]= 0x04,
 *	   data = 0x04030201
 */

/* define lcd unifykey length */

#define LCD_UKEY_HEAD_SIZE        10
#define LCD_UKEY_HEAD_CRC32       4
#define LCD_UKEY_HEAD_DATA_LEN    2
#define LCD_UKEY_HEAD_VERSION     2
#define LCD_UKEY_HEAD_RESERVED    2

struct aml_lcd_unifykey_header_s {
	unsigned int crc32;
	unsigned short data_len;
	unsigned short version;
	unsigned short reserved;
};

/*  lcd */
#define LCD_UKEY_LCD_SIZE          265

/* header (10Byte) */
/* LCD_UKEY_HEAD_SIZE */
/* basic (36Byte) */
#define LCD_UKEY_MODEL_NAME        30
#define LCD_UKEY_INTERFACE         1
#define LCD_UKEY_LCD_BITS          1
#define LCD_UKEY_SCREEN_WIDTH      2
#define LCD_UKEY_SCREEN_HEIGHT     2
/* timing (18Byte) */
#define LCD_UKEY_H_ACTIVE          2
#define LCD_UKEY_V_ACTIVE          2
#define LCD_UKEY_H_PERIOD          2
#define LCD_UKEY_V_PERIOD          2
#define LCD_UKEY_HS_WIDTH          2
#define LCD_UKEY_HS_BP             2
#define LCD_UKEY_HS_POL            1
#define LCD_UKEY_VS_WIDTH          2
#define LCD_UKEY_VS_BP             2
#define LCD_UKEY_VS_POL            1
/* customer (31Byte) */
#define LCD_UKEY_FR_ADJ_TYPE       1
#define LCD_UKEY_SS_LEVEL          1
#define LCD_UKEY_CLK_AUTO_GEN      1
#define LCD_UKEY_PCLK              4
#define LCD_UKEY_H_PERIOD_MIN      2
#define LCD_UKEY_H_PERIOD_MAX      2
#define LCD_UKEY_V_PERIOD_MIN      2
#define LCD_UKEY_V_PERIOD_MAX      2
#define LCD_UKEY_PCLK_MIN          4
#define LCD_UKEY_PCLK_MAX          4
#define LCD_UKEY_CUST_VAL_8        4
#define LCD_UKEY_CUST_VAL_9        4
/* interface (20Byte) */
#define LCD_UKEY_IF_ATTR_0         2
#define LCD_UKEY_IF_ATTR_1         2
#define LCD_UKEY_IF_ATTR_2         2
#define LCD_UKEY_IF_ATTR_3         2
#define LCD_UKEY_IF_ATTR_4         2
#define LCD_UKEY_IF_ATTR_5         2
#define LCD_UKEY_IF_ATTR_6         2
#define LCD_UKEY_IF_ATTR_7         2
#define LCD_UKEY_IF_ATTR_8         2
#define LCD_UKEY_IF_ATTR_9         2
/* power (5Byte * n) */
#define LCD_UKEY_PWR_TYPE          1
#define LCD_UKEY_PWR_INDEX         1
#define LCD_UKEY_PWR_VAL           1
#define LCD_UKEY_PWR_DELAY         2

/* lcd extern */
#define LCD_UKEY_LCD_EXT_SIZE       550

/* header (10Byte) */
/* LCD_UKEY_HEAD_SIZE */
/* basic (33Byte) */
#define LCD_UKEY_EXT_NAME           30
#define LCD_UKEY_EXT_INDEX          1
#define LCD_UKEY_EXT_TYPE           1
#define LCD_UKEY_EXT_STATUS         1
/* type (10Byte) */
#define LCD_UKEY_EXT_TYPE_VAL_0     1
#define LCD_UKEY_EXT_TYPE_VAL_1     1
#define LCD_UKEY_EXT_TYPE_VAL_2     1
#define LCD_UKEY_EXT_TYPE_VAL_3     1
#define LCD_UKEY_EXT_TYPE_VAL_4     1
#define LCD_UKEY_EXT_TYPE_VAL_5     1
#define LCD_UKEY_EXT_TYPE_VAL_6     1
#define LCD_UKEY_EXT_TYPE_VAL_7     1
#define LCD_UKEY_EXT_TYPE_VAL_8     1
#define LCD_UKEY_EXT_TYPE_VAL_9     1
/* init (cmd_size) */
#define LCD_UKEY_EXT_INIT_TYPE       1
/*#define LCD_UKEY_EXT_INIT_VAL        1   //not defined */
#define LCD_UKEY_EXT_INIT_DELAY      1

/* backlight */
#define LCD_UKEY_BL_SIZE            92

/* header (10Byte) */
/* LCD_UKEY_HEAD_SIZE */
/* basic (30Byte) */
#define LCD_UKEY_BL_NAME            30
/* level (12Byte) */
#define LCD_UKEY_BL_LEVEL_UBOOT     2
#define LCD_UKEY_BL_LEVEL_KERNEL    2
#define LCD_UKEY_BL_LEVEL_MAX       2
#define LCD_UKEY_BL_LEVEL_MIN       2
#define LCD_UKEY_BL_LEVEL_MID       2
#define LCD_UKEY_BL_LEVEL_MID_MAP   2
/* method (8Byte) */
#define LCD_UKEY_BL_METHOD          1
#define LCD_UKEY_BL_EN_GPIO         1
#define LCD_UKEY_BL_EN_GPIO_ON      1
#define LCD_UKEY_BL_EN_GPIO_OFF     1
#define LCD_UKEY_BL_ON_DELAY        2
#define LCD_UKEY_BL_OFF_DELAY       2
/* pwm (32Byte) */
#define LCD_UKEY_BL_PWM_ON_DELAY    2
#define LCD_UKEY_BL_PWM_OFF_DELAY   2
#define LCD_UKEY_BL_PWM_METHOD      1
#define LCD_UKEY_BL_PWM_PORT        1
#define LCD_UKEY_BL_PWM_FREQ        4
#define LCD_UKEY_BL_PWM_DUTY_MAX    1
#define LCD_UKEY_BL_PWM_DUTY_MIN    1
#define LCD_UKEY_BL_PWM_GPIO        1
#define LCD_UKEY_BL_PWM_GPIO_OFF    1
#define LCD_UKEY_BL_PWM2_METHOD     1
#define LCD_UKEY_BL_PWM2_PORT       1
#define LCD_UKEY_BL_PWM2_FREQ       4
#define LCD_UKEY_BL_PWM2_DUTY_MAX   1
#define LCD_UKEY_BL_PWM2_DUTY_MIN   1
#define LCD_UKEY_BL_PWM2_GPIO       1
#define LCD_UKEY_BL_PWM2_GPIO_OFF   1
#define LCD_UKEY_BL_PWM_LEVEL_MAX   2
#define LCD_UKEY_BL_PWM_LEVEL_MIN   2
#define LCD_UKEY_BL_PWM2_LEVEL_MAX  2
#define LCD_UKEY_BL_PWM2_LEVEL_MIN  2


/* API */
extern int lcd_unifykey_len_check(int key_len, int len);
extern int lcd_unifykey_check(char *key_name);
extern int lcd_unifykey_header_check(unsigned char *buf,
		struct aml_lcd_unifykey_header_s *header);
extern int lcd_unifykey_get(char *key_name,
		unsigned char *buf, int *len);
extern void lcd_unifykey_print(void);

#endif
