/*
 * include/linux/amlogic/saradc.h
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

#ifndef __AML_SARADC_H__
#define __AML_SARADC_H__

#include <linux/io.h>

#define SARADC_DEV_NUM 1

enum {
	CHAN_0 = 0,
	CHAN_1,
	CHAN_2,
	CHAN_3,
	CHAN_4,
	CHAN_5,
	CHAN_6,
	CHAN_7,
	SARADC_CHAN_NUM,
};

extern int get_adc_sample(int dev_id, int ch);
extern int get_adc_sample_12bit(int dev_id, int ch);

#define bits_desc(reg_offset, bits_offset, bits_len) \
	(((bits_len)<<24)|((bits_offset)<<16)|(reg_offset))
#define of_mem_offset(bd) ((bd)&0xffff)
#define of_bits_offset(bd) (((bd)>>16)&0xff)
#define of_bits_len(bd) (((bd)>>24)&0xff)

extern void setb(void __iomem *mem_base, unsigned int bits_desc,
		unsigned int bits_val);
extern unsigned int getb(void __iomem *mem_base, unsigned int bits_desc);

#endif
