/*
 * sound/soc/amlogic/auge/iomap.h
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

#ifndef __AML_SND_IOMAP_H__
#define __AML_SND_IOMAP_H__

enum{
	IO_PDM_BUS = 0,
	IO_AUDIO_BUS,

	IO_MAX,
};

extern int aml_pdm_read(unsigned int reg);

extern void aml_pdm_write(unsigned int reg, unsigned int val);

extern void aml_pdm_update_bits(unsigned int reg,
	unsigned int mask, unsigned int val);

extern int audiobus_read(unsigned int reg);

extern void audiobus_write(unsigned int reg, unsigned int val);

extern void audiobus_update_bits(unsigned int reg,
	unsigned int mask, unsigned int val);

#endif
