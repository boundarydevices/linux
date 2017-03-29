/*
 * drivers/amlogic/audiodsp/dsp_microcode.h
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

#ifndef AUDIODSP_MICROCODE_HEADER
#define AUDIODSP_MICROCODE_HEADER

#include "audiodsp_module.h"
struct audiodsp_microcode {
	int id;
	/* support format; */
	int fmt;
	struct list_head list;
	unsigned long code_start_addr;
	unsigned long code_size;
	char file_name[64];
};

extern int audiodsp_microcode_register(struct audiodsp_priv *priv,
					      int fmt, char *filename);
extern struct audiodsp_microcode
	*audiodsp_find_supoort_mcode(struct audiodsp_priv *priv,
		int fmt);
extern int audiodsp_microcode_load(struct audiodsp_priv *priv,
				struct audiodsp_microcode *pmcode);
int audiodsp_microcode_free(struct audiodsp_priv *priv);

#endif /*  */
