/*
 * sound/soc/amlogic/auge/vad_hw.h
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
#ifndef __VAD_HW_H__
#define __VAD_HW_H__
#include <linux/types.h>

#include "regs.h"
#include "iomap.h"

extern void vad_set_ram_coeff(int len, int *params);


extern void vad_set_de_params(int len, int *params);

extern void vad_set_pwd(void);

extern void vad_set_cep(void);

extern void vad_set_src(int src);

extern void vad_set_in(void);

extern void vad_set_enable(bool enable);
#endif
