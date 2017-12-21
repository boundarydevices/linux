/*
 * drivers/amlogic/amaudio/amaudio.h
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

#ifndef _AMAUDIO_H_
#define _AMAUDIO_H_

#define AMAUDIO_MODULE_NAME "amaudio"
#define AMAUDIO_DRIVER_NAME "amaudio"
#define AMAUDIO_DEVICE_NAME "amaudio-dev"
#define AMAUDIO_CLASS_NAME "amaudio"

#define AMAUDIO_IOC_SET_LEFT_MONO	_IOW('A', 0x0e, int)
#define AMAUDIO_IOC_SET_RIGHT_MONO	_IOW('A', 0x0f, int)
#define AMAUDIO_IOC_SET_STEREO	_IOW('A', 0x10, int)
#define AMAUDIO_IOC_SET_CHANNEL_SWAP	_IOW('A', 0x11, int)
#define AMAUDIO_IOC_MUTE_LEFT_RIGHT_CHANNEL _IOW('A', 0x1f, unsigned long)
#define AMAUDIO_IOC_MUTE_UNMUTE _IOW('A', 0x20, unsigned long)

extern int if_audio_out_enable(void);
extern int if_958_audio_out_enable(void);
extern unsigned int read_i2s_mute_swap_reg(void);
extern void audio_i2s_swap_left_right(unsigned int flag);
extern void audio_mute_left_right(unsigned int flag);
extern void audio_i2s_unmute(void);
extern void audio_i2s_mute(void);

extern unsigned int audioin_mode;
extern unsigned int dac_mute_const;

#endif
