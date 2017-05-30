/*
 * sound/soc/amlogic/auge/audio_io.h
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

#ifndef __AML_AUDIO_IO_H_
#define __AML_AUDIO_IO_H_

struct aml_audio_controller;

struct aml_audio_ctrl_ops {
	unsigned int (*read)(struct aml_audio_controller *actrlr,
			unsigned int reg);
	int (*write)(struct aml_audio_controller *actrlr,
			unsigned int reg, unsigned int value);
	int (*update_bits)(struct aml_audio_controller *actrlr,
		unsigned int reg, unsigned int mask, unsigned int value);
};

struct aml_audio_controller {
	struct regmap *regmap;
	const struct aml_audio_ctrl_ops *ops;
};

/* audio io controller */
int aml_init_audio_controller(struct aml_audio_controller *actrlr,
			struct regmap *regmap, struct aml_audio_ctrl_ops *ops);
int aml_audiobus_write(struct aml_audio_controller *actrlr,
			unsigned int reg, unsigned int value);
unsigned int aml_audiobus_read(struct aml_audio_controller *actrlr,
			unsigned int reg);
int aml_audiobus_update_bits(struct aml_audio_controller *actrlr,
		unsigned int reg, unsigned int mask, unsigned int value);

#endif
