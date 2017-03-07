/*
 * drivers/amlogic/input/remote/remote_regmap.c
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

#include "remote_meson.h"

static struct remote_reg_map regs_legacy_nec[] = {
	{REG_LDR_ACTIVE,    (500 << 16) | (400 << 0)},
	{REG_LDR_IDLE,      300 << 16 | 200 << 0},
	{REG_LDR_REPEAT,    150 << 16 | 80 << 0},
	{REG_BIT_0,         72 << 16 | 40 << 0 },
	{REG_REG0,          7 << 28 | (0xFA0 << 12) | 0x13},
	{REG_STATUS,        (134 << 20) | (90 << 10)},
	{REG_REG1,          0xbe00},
};

static struct remote_reg_map regs_default_nec[] = {
	{ REG_LDR_ACTIVE,   (500 << 16) | (400 << 0)},
	{ REG_LDR_IDLE,     300 << 16 | 200 << 0},
	{ REG_LDR_REPEAT,   150 << 16 | 80 << 0},
	{ REG_BIT_0,        72 << 16 | 40 << 0},
	{ REG_REG0,         7 << 28 | (0xFA0 << 12) | 0x13},
	{ REG_STATUS,       (134 << 20) | (90 << 10)},
	{ REG_REG1,         0x9f00},
	{ REG_REG2,         0x00},
	{ REG_DURATN2,      0x00},
	{ REG_DURATN3,      0x00}
};

static struct remote_reg_map regs_default_duokan[] = {
	{ REG_LDR_ACTIVE,   ((70 << 16) | (30 << 0))},
	{ REG_LDR_IDLE,     ((50 << 16) | (15 << 0))},
	{ REG_LDR_REPEAT,   ((30 << 16) | (26 << 0))},
	{ REG_BIT_0,        ((66 << 16) | (40 << 0))},
	{ REG_REG0,         ((3 << 28) | (0x4e2 << 12) | (0x13))},
	{ REG_STATUS,       ((80 << 20) | (66 << 10))},
	{ REG_REG1,         0x9300},
	{ REG_REG2,         0xb90b},
	{ REG_DURATN2,      ((97 << 16) | (80 << 0))},
	{ REG_DURATN3,      ((120 << 16) | (97 << 0))},
	{ REG_REG3,         5000<<0}
};

static struct remote_reg_map regs_default_xmp_1_sw[] = {
	{ REG_LDR_ACTIVE,   0},
	{ REG_LDR_IDLE,     0},
	{ REG_LDR_REPEAT,   0},
	{ REG_BIT_0,        0},
	{ REG_REG0,        ((3 << 28) | (0xFA0 << 12) | (9))},
	{ REG_STATUS,       0},
	{ REG_REG1,         0x8574},
	{ REG_REG2,         0x02},
	{ REG_DURATN2,      0},
	{ REG_DURATN3,      0},
	{ REG_REG3,         0}
};

static struct remote_reg_map regs_default_xmp_1[] = {
	{ REG_LDR_ACTIVE,   0},
	{ REG_LDR_IDLE,     0},
	{ REG_LDR_REPEAT,   0},
	{ REG_BIT_0,        (52 << 16) | (45<<0)},
	{ REG_REG0,         ((7 << 28) | (0x5DC << 12) | (0x13))},
	{ REG_STATUS,       (87 << 20) | (80 << 10)},
	{ REG_REG1,         0x9f00},
	{ REG_REG2,         0xa90e},
	/*n=10,758+137*10=2128us,2128/20= 106*/
	{ REG_DURATN2,      (121<<16) | (114<<0)},
	{ REG_DURATN3,      (7<<16) | (7<<0)},
	{ REG_REG3,         0}
};

static struct remote_reg_map regs_default_nec_sw[] = {
	{ REG_LDR_ACTIVE,   0},
	{ REG_LDR_IDLE,     0},
	{ REG_LDR_REPEAT,   0},
	{ REG_BIT_0,        0},
	{ REG_REG0,        ((3 << 28) | (0xFA0 << 12) | (9))},
	{ REG_STATUS,       0},
	{ REG_REG1,         0x8574},
	{ REG_REG2,         0x02},
	{ REG_DURATN2,      0},
	{ REG_DURATN3,      0},
	{ REG_REG3,         0}
};

static struct remote_reg_map regs_default_rc5[] = {
	{ REG_LDR_ACTIVE,   0},
	{ REG_LDR_IDLE,     0},
	{ REG_LDR_REPEAT,   0},
	{ REG_BIT_0,        0},
	{ REG_REG0,         ((3 << 28) | (0x1644 << 12) | 0x13)},
	{ REG_STATUS,       (1 << 30)},
	{ REG_REG1,         ((1 << 15) | (13 << 8))},
	/*bit[0-3]: RC5; bit[8]: MSB first mode; bit[11]: compare frame method*/
	{ REG_REG2,         ((1 << 13) | (1 << 11) | (1 << 8) | 0x7)},
	/*Half bit for RC5 format: 888.89us*/
	{ REG_DURATN2,      ((49 << 16) | (40 << 0))},
	/*RC5 typically 1777.78us for whole bit*/
	{ REG_DURATN3,      ((94 << 16) | (83 << 0))},
	{ REG_REG3,         0}
};

static struct remote_reg_map regs_default_rc6[] = {
	{REG_LDR_ACTIVE,    (210 << 16) | (125 << 0)},
	/*rca leader 4000us,200* timebase*/
	{REG_LDR_IDLE,      50 << 16 | 38 << 0}, /* leader idle 400*/
	{REG_LDR_REPEAT,    145 << 16 | 125 << 0}, /* leader repeat*/
	/* logic '0' or '00' 1500us*/
	{REG_BIT_0,         51 << 16 | 38 << 0 },
	{REG_REG0,          (7 << 28)|(0xFA0 << 12)|0x13},
	/* sys clock boby time.base time = 20 body frame*/
	{REG_STATUS,       (94 << 20) | (82 << 10)},
	/*20bit:9440 32bit:9f40 36bit:a340 37bit:a440*/
	{REG_REG1,         0xa440},
	/*it may get the wrong customer value and key value from register if
	 *the value is set to 0x4,so the register value must set to 0x104
	 */
	{REG_REG2,         0x2909},
	{REG_DURATN2,      ((28 << 16) | (16 << 0))},
	{REG_DURATN3,      ((51 << 16) | (38 << 0))},
};

void set_hardcode(struct remote_chip *chip, int code)
{
	remote_dbg(chip->dev, "framecode=0x%x\n", code);
	chip->r_dev->cur_hardcode = code;
}

/**
 * legacy nec hardware interface
 * other interface share with the multi-format NEC
 */
static int ir_legacy_nec_get_scancode(struct remote_chip *chip)
{
	int  code = 0;
	int decode_status = 0;
	int status = 0;

	remote_reg_read(chip, LEGACY_IR_ID, REG_STATUS, &decode_status);
	if (decode_status & 0x01)
		status |= REMOTE_REPEAT;
	chip->decode_status = status; /*set decode status*/
	remote_reg_read(chip, LEGACY_IR_ID, REG_FRAME, &code);
	remote_dbg(chip->dev, "framecode=0x%x\n", code);
	chip->r_dev->cur_hardcode = code;
	code = (code >> 16) & 0xff;
	return code;
}

/*
 *nec hardware interface
 */
static int ir_nec_get_scancode(struct remote_chip *chip)
{
	int  code = 0;
	int decode_status = 0;
	int status = 0;

	remote_reg_read(chip, MULTI_IR_ID, REG_STATUS, &decode_status);
	if (decode_status & 0x01)
		status |= REMOTE_REPEAT;
	chip->decode_status = status; /*set decode status*/
	remote_reg_read(chip, MULTI_IR_ID, REG_FRAME, &code);
	remote_dbg(chip->dev, "framecode=0x%x\n", code);
	chip->r_dev->cur_hardcode = code;
	code = (code >> 16) & 0xff;
	return code;
}

static int ir_nec_get_decode_status(struct remote_chip *chip)
{
	int status = chip->decode_status;
	return status;
}

static u32 ir_nec_get_custom_code(struct remote_chip *chip)
{
	u32 custom_code;

	custom_code = chip->r_dev->cur_hardcode & 0xffff;
	return custom_code;
}

/*
 *	xmp-1 decode hardware interface
 */
static int xmp_decode_second;
static int ir_xmp_get_scancode(struct remote_chip *chip)
{
	int  code = 0;

	remote_reg_read(chip, MULTI_IR_ID, REG_FRAME, &code);
	remote_dbg(chip->dev, "framecode=0x%x\n", code);
	if (!xmp_decode_second) {
		chip->r_dev->cur_hardcode = 0;
		chip->r_dev->cur_customcode = code;
		xmp_decode_second = 1;
		return -1;
	}
	xmp_decode_second = 2;
	chip->r_dev->cur_hardcode = code;
	code = (code >> 8) & 0xff;
	return code;
}

static int ir_xmp_get_decode_status(struct remote_chip *chip)
{
	int status = 0;

	switch (xmp_decode_second) {
	case 0:
	case 1:
		status = REMOTE_CUSTOM_DATA;
		break;
	case 2:
		if (chip->r_dev->cur_hardcode & (8<<20))
			status = REMOTE_REPEAT;
		else
			status = REMOTE_NORMAL;
		xmp_decode_second = 0;
		break;
	default:
		break;
	}
	return status;
}

static u32 ir_xmp_get_custom_code(struct remote_chip *chip)
{
	u32 custom_code;

	custom_code = chip->r_dev->cur_customcode & 0xffff;
	remote_dbg(chip->dev, "custom_code=0x%x\n", custom_code);
	return custom_code;
}

/*
 * duokan hardware interface
 */
static int duokan_parity_check(int code)
{
	unsigned int data;
	unsigned int c74, c30, d74, d30, p30;

	c74 = (code >> 16) & 0xF;
	c30 = (code >> 12) & 0xF;
	d74 = (code >> 8)  & 0xF;
	d30 = (code >> 4)  & 0xF;
	p30 = (code >> 0)  & 0xF;

	data = c74 ^ c30 ^ d74 ^ d30;

	if (p30 == data)
		return 0;

	pr_err("%s: parity check error code=0x%x\n", DRIVER_NAME, code);
	return -1;
}

static int ir_duokan_get_scancode(struct remote_chip *chip)
{
	int  code = 0;

	remote_reg_read(chip, MULTI_IR_ID, REG_FRAME, &code);
	if (duokan_parity_check(code) < 0) {
		set_hardcode(chip, 0);
		return 0;
	}
	set_hardcode(chip, code);
	code = (code >> 4) & 0xff;
	return code;
}


static int ir_duokan_get_decode_status(struct remote_chip *chip)
{
	int decode_status = 0;
	int status = 0;

	remote_reg_read(chip, MULTI_IR_ID, REG_STATUS, &decode_status);
	decode_status &= 0xf;
	if (decode_status & 0x01)
		status |= REMOTE_REPEAT;
	/*
	 *it is error,if the custom_code is not mask.
	 *if (decode_status & 0x02)
	 *	status |= REMOTE_CUSTOM_ERROR;
	 */
	if (decode_status & 0x04)
		status |= REMOTE_DATA_ERROR;
	return status;

}


static u32 ir_duokan_get_custom_code(struct remote_chip *chip)
{
	u32 custom_code;

	custom_code = (chip->r_dev->cur_hardcode >> 12) & 0xffff;
	chip->r_dev->cur_customcode = custom_code;
	remote_dbg(chip->dev, "custom_code=0x%x\n", custom_code);
	return custom_code;
}

/*
 *xmp-1 raw decoder interface
 */
static u32 ir_raw_xmp_get_custom_code(struct remote_chip *chip)
{
	u32 custom_code;

	custom_code = chip->r_dev->cur_customcode;
	remote_dbg(chip->dev, "custom_code=0x%x\n", custom_code);
	return custom_code;
}

static bool ir_raw_xmp_set_custom_code(struct remote_chip *chip, u32 code)
{
	chip->r_dev->cur_customcode = code;
	return 0;
}

/*
 * RC5 decoder interface
 * 14bit of one frame is stored in [13:0]:
 *      bit[13:11] is S1, S2, Toggle
 *      bit[10:6] is system_code/custom_code
 *      bit [5:0] is data_code/scan_code
 */

static int ir_rc5_get_scancode(struct remote_chip *chip)
{
	int code = 0;
	int status = 0;
	int decode_status = 0;

	remote_reg_read(chip, MULTI_IR_ID, REG_STATUS, &decode_status);
	decode_status &= 0xf;
	if (decode_status & 0x01)
		status |= REMOTE_REPEAT;
	chip->decode_status = status;
	remote_reg_read(chip, MULTI_IR_ID, REG_FRAME, &code);
	remote_dbg(chip->dev, "framecode=0x%x\n", code);
	chip->r_dev->cur_hardcode = code;
	code = code & 0x3f;
	return code;
}

static int ir_rc5_get_decode_status(struct remote_chip *chip)
{
	int status = chip->decode_status;

	return status;
}

static u32 ir_rc5_get_custom_code(struct remote_chip *chip)
{
	u32 custom_code;

	custom_code = (chip->r_dev->cur_hardcode >> 6) & 0x1f;
	return custom_code;
}

/*RC6 decode interface*/
static int ir_rc6_get_scancode(struct remote_chip *chip)
{
	int code = 0;
	int code1 = 0;
	int status = 0;
	int decode_status = 0;

	remote_reg_read(chip, MULTI_IR_ID, REG_STATUS, &decode_status);
	decode_status &= 0xf;
	if (decode_status & 0x01)
		status |= REMOTE_REPEAT;
	chip->decode_status = status;
	remote_reg_read(chip, MULTI_IR_ID, REG_FRAME, &code);
	remote_dbg(chip->dev, "framecode=0x%x\n", code);
	/**
	 *if the frame length larger than 32Bit, we must read the REG_FRAME1.
	 *Otherwise it will affect the update of the 'frame1' and repeat frame
	 *detect
	 */
	remote_reg_read(chip, MULTI_IR_ID, REG_FRAME1, &code1);
	chip->r_dev->cur_hardcode = code;
	code = code & 0xff;
	return code;
}

static int ir_rc6_get_decode_status(struct remote_chip *chip)
{
	int status = chip->decode_status;

	return status;
}

static u32 ir_rc6_get_custom_code(struct remote_chip *chip)
{
	u32 custom_code;

	custom_code = (chip->r_dev->cur_hardcode >> 16) & 0xffff;
	return custom_code;
}

/*legacy IR controller support protocols*/
static struct aml_remote_reg_proto reg_legacy_nec = {
	.protocol = REMOTE_TYPE_LEGACY_NEC,
	.name     = "LEGACY_NEC",
	.reg_map      = regs_legacy_nec,
	.reg_map_size = ARRAY_SIZE(regs_legacy_nec),
	.get_scancode      = ir_legacy_nec_get_scancode,
	.get_decode_status = ir_nec_get_decode_status,
	.get_custom_code   = ir_nec_get_custom_code
};

static struct aml_remote_reg_proto reg_nec = {
	.protocol = REMOTE_TYPE_NEC,
	.name     = "NEC",
	.reg_map      = regs_default_nec,
	.reg_map_size = ARRAY_SIZE(regs_default_nec),
	.get_scancode      = ir_nec_get_scancode,
	.get_decode_status = ir_nec_get_decode_status,
	.get_custom_code   = ir_nec_get_custom_code
};

static struct aml_remote_reg_proto reg_duokan = {
	.protocol = REMOTE_TYPE_DUOKAN,
	.name	  = "DUOKAN",
	.reg_map      = regs_default_duokan,
	.reg_map_size = ARRAY_SIZE(regs_default_duokan),
	.get_scancode      = ir_duokan_get_scancode,
	.get_decode_status = ir_duokan_get_decode_status,
	.get_custom_code   = ir_duokan_get_custom_code
};

static struct aml_remote_reg_proto reg_xmp_1_sw = {
	.protocol = REMOTE_TYPE_RAW_XMP_1,
	.name	  = "XMP-1-RAW",
	.reg_map      = regs_default_xmp_1_sw,
	.reg_map_size = ARRAY_SIZE(regs_default_xmp_1_sw),
	.get_scancode      = NULL,
	.get_decode_status = NULL,
	.get_custom_code   = ir_raw_xmp_get_custom_code,
	.set_custom_code   = ir_raw_xmp_set_custom_code
};

static struct aml_remote_reg_proto reg_xmp_1 = {
	.protocol = REMOTE_TYPE_XMP_1,
	.name	  = "XMP-1",
	.reg_map      = regs_default_xmp_1,
	.reg_map_size = ARRAY_SIZE(regs_default_xmp_1),
	.get_scancode      = ir_xmp_get_scancode,
	.get_decode_status = ir_xmp_get_decode_status,
	.get_custom_code   = ir_xmp_get_custom_code,
};

static struct aml_remote_reg_proto reg_nec_sw = {
	.protocol = REMOTE_TYPE_RAW_NEC,
	.name	  = "NEC-SW",
	.reg_map      = regs_default_nec_sw,
	.reg_map_size = ARRAY_SIZE(regs_default_nec_sw),
	.get_scancode      = NULL,
	.get_decode_status = NULL,
	.get_custom_code   = NULL,
};

static struct aml_remote_reg_proto reg_rc5 = {
	.protocol = REMOTE_TYPE_RC5,
	.name	  = "RC5",
	.reg_map      = regs_default_rc5,
	.reg_map_size = ARRAY_SIZE(regs_default_rc5),
	.get_scancode      = ir_rc5_get_scancode,
	.get_decode_status = ir_rc5_get_decode_status,
	.get_custom_code   = ir_rc5_get_custom_code,
};

static struct aml_remote_reg_proto reg_rc6 = {
	.protocol = REMOTE_TYPE_RC6,
	.name	  = "RC6",
	.reg_map      = regs_default_rc6,
	.reg_map_size = ARRAY_SIZE(regs_default_rc6),
	.get_scancode      = ir_rc6_get_scancode,
	.get_decode_status = ir_rc6_get_decode_status,
	.get_custom_code   = ir_rc6_get_custom_code,
};


const struct aml_remote_reg_proto *remote_reg_proto[] = {
	&reg_nec,
	&reg_duokan,
	&reg_xmp_1,
	&reg_xmp_1_sw,
	&reg_nec_sw,
	&reg_rc5,
	&reg_rc6,
	&reg_legacy_nec,
	NULL
};

static int ir_contr_init(struct remote_chip *chip, int type, unsigned char id)
{
	const struct aml_remote_reg_proto **reg_proto = remote_reg_proto;
	struct remote_reg_map *reg_map;
	int size;
	int status;

	for ( ; (*reg_proto) != NULL ; ) {
		if ((*reg_proto)->protocol == type)
			break;
		reg_proto++;
	}
	if (!*reg_proto) {
		dev_err(chip->dev, "%s, protocol set err %d\n", __func__, type);
		return -EINVAL;
	}

	remote_reg_read(chip, id, REG_STATUS, &status);
	remote_reg_read(chip, id, REG_FRAME,  &status);
	/*
	 * reset ir decoder and disable the state machine
	 * of IR decoder.
	 * [15] = 0 ,disable the machine of IR decoder
	 * [0] = 0x01,set to 1 to reset the IR decoder
	 */
	remote_reg_write(chip, id, REG_REG1, 0x01);
	dev_info(chip->dev, "default protocol = 0x%x and id = %d\n",
				(*reg_proto)->protocol, id);
	reg_map = (*reg_proto)->reg_map;
	size    = (*reg_proto)->reg_map_size;

	for (  ; size > 0;  ) {
		remote_reg_write(chip, id, reg_map->reg, reg_map->val);
		dev_info(chip->dev, "reg=0x%x, val=0x%x\n",
				reg_map->reg, reg_map->val);
		reg_map++;
		size--;
	}
	/*
	 * when we reinstall remote controller register,
	 * we need reset IR decoder, set 1 to REG_REG1 bit0,
	 * after IR decoder reset, we need to clear the bit0
	 */
	remote_reg_read(chip, id, REG_REG1, &status);
	status |= 1;
	remote_reg_write(chip, id, REG_REG1, status);
	status &= ~0x01;
	remote_reg_write(chip, id, REG_REG1, status);
	chip->ir_contr[id].get_scancode      = (*reg_proto)->get_scancode;
	chip->ir_contr[id].get_decode_status = (*reg_proto)->get_decode_status;
	chip->ir_contr[id].proto_name        = (*reg_proto)->name;
	chip->ir_contr[id].get_custom_code   = (*reg_proto)->get_custom_code;
	chip->ir_contr[id].set_custom_code   = (*reg_proto)->set_custom_code;

	return 0;
}

int ir_register_default_config(struct remote_chip *chip, int type)
{
	if (ENABLE_LEGACY_IR(type)) {
		/*initialize registers for legacy IR controller*/
		ir_contr_init(chip, LEGACY_IR_TYPE_MASK(type), LEGACY_IR_ID);
	} else {
		/*disable legacy IR controller: REG_REG1[15]*/
		remote_reg_write(chip, LEGACY_IR_ID, REG_REG1, 0x0);
	}
	/*initialize registers for Multi-format IR controller*/
	ir_contr_init(chip, MULTI_IR_TYPE_MASK(type), MULTI_IR_ID);

	return 0;

}
EXPORT_SYMBOL(ir_register_default_config);

