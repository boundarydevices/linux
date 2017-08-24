/*
 * drivers/amlogic/media/vin/tvin/hdmirx_ext/vdin_iface.h
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

#ifndef __VDIN_INTERFACE_H__
#define __VDIN_INTERFACE_H__

#include "../tvin_frontend.h"
#include "hdmirx_ext_drv.h"

extern void set_invert_top_bot(bool invert_flag);

extern int hdmirx_ext_register_tvin_frontend(struct tvin_frontend_s *frontend);
extern void hdmirx_ext_stop_vdin(void);
extern void hdmirx_ext_start_vdin(int width, int height, int frame_rate,
		int field_flag);
extern void hdmirx_ext_start_vdin_mode(unsigned int mode);

#endif
