/*
 * include/linux/amlogic/media/amprime_sl/prime_sl.h
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

#ifndef _PRIMSE_SL_H_
#define _PRIMSE_SL_H_

#include <linux/types.h>

extern void prime_sl_process(struct vframe_s *vf);
extern bool is_dolby_vision_enable(void);

#endif
