/*
 * include/linux/amlogic/media/sound/audio_iomap.h
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

#ifndef AML_SND_IOMAP_H
#define AML_SND_IOMAP_H
#include <linux/platform_device.h>

enum{
	IO_AUDIN_BASE = 0,
	IO_AIU_BASE,
	IO_EQDRC_BASE,
	IO_HIU_RESET_BASE,
	IO_ISA_BASE,
	IO_BASE_MAX,
};

struct audio_iomap {
	bool use_iomap;
	void __iomem *reg_map[IO_BASE_MAX];
};

extern int aml_audin_read(unsigned int short_reg);
extern void aml_audin_write(unsigned int short_reg, unsigned int val);
extern void aml_audin_update_bits(unsigned int short_reg,
			unsigned int mask, unsigned int val);

extern int aml_aiu_read(unsigned int short_reg);
extern void aml_aiu_write(unsigned int short_reg, unsigned int val);
extern void aml_aiu_update_bits(unsigned int short_reg,
			unsigned int mask, unsigned int val);

extern int aml_eqdrc_read(unsigned int short_reg);
extern void aml_eqdrc_write(unsigned int short_reg, unsigned int val);
extern void aml_eqdrc_update_bits(unsigned int short_reg,
			unsigned int mask, unsigned int val);

extern int aml_hiu_reset_read(unsigned int short_reg);
extern void aml_hiu_reset_write(unsigned int short_reg, unsigned int val);
extern void aml_hiu_reset_update_bits(unsigned int short_reg,
			unsigned int mask, unsigned int val);

extern int aml_isa_read(unsigned int short_reg);
extern void aml_isa_write(unsigned int short_reg, unsigned int val);
extern void aml_isa_update_bits(unsigned int short_reg,
			unsigned int mask, unsigned int val);

#endif
