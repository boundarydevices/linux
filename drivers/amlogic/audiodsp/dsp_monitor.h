/*
 * drivers/amlogic/audiodsp/dsp_monitor.h
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

#ifndef DSP_MONITOR_HEADER_H
#define DSP_MONITOR_HEADER_H
#include "audiodsp_module.h"

void start_audiodsp_monitor(struct audiodsp_priv *priv);
void stop_audiodsp_monitor(struct audiodsp_priv *priv);
void init_audiodsp_monitor(struct audiodsp_priv *priv);
void release_audiodsp_monitor(struct audiodsp_priv *priv);

#endif	/*  */
