/*
 * include/linux/amlogic/media/sound/auge_utils.h
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

#ifndef __AUGE_UTILS_H__
#define __AUGE_UTILS_H__

void auge_acodec_reset(void);
void auge_toacodec_ctrl(int tdmout_id);
void auge_toacodec_ctrl_ext(int tdmout_id, int ch0_sel, int ch1_sel);
#endif
