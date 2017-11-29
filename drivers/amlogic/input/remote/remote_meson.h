/*
 * drivers/amlogic/input/remote/remote_meson.h
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

#ifndef _REMOTE_MESON_H
#define _REMOTE_MESON_H
#include <linux/cdev.h>
#include "rc_common.h"
#include "remote_core.h"

#define DRIVER_NAME "meson-remote"

#define IR_DATA_IS_VALID(data) (data & 0x8)
#define IR_CONTROLLER_BUSY(x) ((x >> 7) & 0x1)

#define CURSOR_MOVE_ACCELERATE {0, 2, 2, 4, 4, 6, 8, 10, 12, 14, 16, 18}

enum IR_CONTR_NUMBER {
	MULTI_IR_ID = 0,
	LEGACY_IR_ID,
	IR_ID_MAX
};

enum IR_WORK_MODE {
	NORMAL_MODE = 0,
	MOUSE_MODE = 1
};

struct remote_range {
	struct range active;
	struct range idle;
	struct range repeat;
	struct range bit_zero_zero;
	struct range bit_zero_one;
	struct range bit_one_zero;
	struct range bit_one_one;
};

struct remote_reg_map {
	unsigned int reg;
	unsigned int val;
};

/*
 *struct ir_map_tab_list
 *
 *@ir_dev_mode: 0: normal mode; 1: mouse mode
 *@list:
 *@tab:
 */
struct ir_map_tab_list {
	bool ir_dev_mode;
	struct list_head list;
	struct ir_map_tab tab;
};

struct cdev;
struct remote_chip;

/**
 *struct key_number - to save the number of key for map table
 *
 *@update_flag: to ensure get key number before map table
 *@value:
 */
struct key_number {
	bool update_flag;
	int  value;
};

/**
 *struct remote_contr_desc - describe the different properties and methods
 *for the Legacy IR controller and multi-format IR controller.
 *TODO: compatible with the "struct aml_remote_reg_proto"
 */
struct remote_contr_desc {
	void __iomem *remote_regs;
	char *proto_name;
	int (*get_scancode)(struct remote_chip *chip);
	int (*get_decode_status)(struct remote_chip *chip);
	u32 (*get_custom_code)(struct remote_chip *chip);
	bool (*set_custom_code)(struct remote_chip *chip, u32 code);
};
/**
 *struct remote_chip - describe the common properties and methods
 * for the Legacy IR controller and multi-format IR controller.
 */
struct remote_chip {
	struct device *dev;
	struct remote_dev *r_dev;
	struct remote_range reg_duration;
	char *dev_name;
	int protocol;

	dev_t chr_devno;
	struct class  *chr_class;
	struct cdev chrdev;
	struct mutex  file_lock;
	spinlock_t slock;

	bool debug_enable;
	bool repeat_enable;
#define CUSTOM_TABLES_SIZE 20
#define CUSTOM_NUM_MAX 20
	struct list_head map_tab_head;
	struct ir_map_tab_list *cur_tab;
	int custom_num;
	struct key_number key_num;
	int decode_status;
	int sys_custom_code;
	const char *keymap_name;
	int	irqno;       /*irq number*/
	int	irq_cpumask;
	/**
	 *indicate which ir controller working.
	 *0: multi format IR
	 *1: legacy IR
	 */
	unsigned char ir_work;
	/**
	 *multi_format IR controller register saved to ir_contr[0]
	 *legacy IR controller register saved to ir_contr[1]
	 */
	struct remote_contr_desc ir_contr[2];

	/*software decode*/
	unsigned char bit_count;
	unsigned short time_window[18];

	int (*report_key)(struct remote_chip *chip);
	int (*release_key)(struct remote_chip *chip);
	int (*set_register_config)(struct remote_chip *chip, int type);
	int (*debug_printk)(const char *, ...);
};

struct aml_remote_reg_proto {
	int protocol;
	char *name;
	struct aml_remote_reg *reg;
	struct remote_reg_map *reg_map;
	int reg_map_size;
	int (*get_scancode)(struct remote_chip *chip);
	int (*get_decode_status)(struct remote_chip *chip);
	u32 (*get_custom_code)(struct remote_chip *chip);
	bool (*set_custom_code)(struct remote_chip *chip, u32 code);
};

enum {
	DECODE_MODE_NEC			= 0x00,
	DECODE_MODE_SKIP_LEADER = 0x01,
	DECODE_MODE_SOFTWARE    = 0x02,
	DECODE_MODE_MITSUBISHI_OR_50560 = 0x03,
	DECODE_MODE_DUOKAN = 0x0B
};

enum remote_reg {
	REG_LDR_ACTIVE = 0x00<<2,
	REG_LDR_IDLE   = 0x01<<2,
	REG_LDR_REPEAT = 0x02<<2,
	REG_BIT_0      = 0x03<<2,
	REG_REG0       = 0x04<<2,
	REG_FRAME      = 0x05<<2,
	REG_STATUS     = 0x06<<2,
	REG_REG1       = 0x07<<2,
	REG_REG2       = 0x08<<2,
	REG_DURATN2    = 0x09<<2,
	REG_DURATN3    = 0x0a<<2,
	REG_FRAME1     = 0x0b<<2,
	REG_STATUS1    = 0x0c<<2,
	REG_STATUS2    = 0x0d<<2,
	REG_REG3       = 0x0e<<2,
	REG_FRAME_RSV0 = 0x0f<<2,
	REG_FRAME_RSV1 = 0x10<<2
};

int ir_register_default_config(struct remote_chip *chip, int type);
int ir_cdev_init(struct remote_chip *chip);
void ir_cdev_free(struct remote_chip *chip);


int remote_reg_read(struct remote_chip *chip, unsigned char id,
	unsigned int reg, unsigned int *val);
int remote_reg_write(struct remote_chip *chip, unsigned char id,
	unsigned int reg, unsigned int val);
int ir_scancode_sort(struct ir_map_tab *ir_map);
struct ir_map_tab_list *seek_map_tab(struct remote_chip *chip, int custom_code);
void ir_tab_free(struct ir_map_tab_list *ir_map_list);

#endif
