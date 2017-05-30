/*
 * sound/soc/amlogic/auge/audio_io.c
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

#include <linux/module.h>
#include "audio_io.h"

int aml_init_audio_controller(struct aml_audio_controller *actrlr,
			struct regmap *regmap, struct aml_audio_ctrl_ops *ops)
{
	actrlr->regmap = regmap;
	actrlr->ops = ops;

	return 0;
}

int aml_audiobus_write(struct aml_audio_controller *actrlr,
			unsigned int reg, unsigned int value)
{
	if (actrlr->ops->write)
		return actrlr->ops->write(actrlr, reg, value);

	return -1;
}

unsigned int aml_audiobus_read(struct aml_audio_controller *actrlr,
		unsigned int reg)
{
	if (actrlr->ops->read)
		return actrlr->ops->read(actrlr, reg);

	return 0;
}

int aml_audiobus_update_bits(struct aml_audio_controller *actrlr,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	if (actrlr->ops->update_bits)
		return actrlr->ops->update_bits(actrlr, reg, mask, value);

	return -1;
}

/* Module information */
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("ALSA Soc Aml Audio Utils");
MODULE_LICENSE("GPL v2");
