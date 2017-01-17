/*
 * drivers/amlogic/media/video_sink/videolog.h
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

#define LOG_LEVEL_VAR       amlog_level_video
#define LOG_MASK_VAR        amlog_mask_video

#define LOG_LEVEL_ERROR     0
#define LOG_MASK_TIMESTAMP  0x00000001UL
#define LOG_MASK_FRAMEINFO  0x00000002UL
#define LOG_MASK_FRAMESKIP  0x00000004UL
#define LOG_MASK_SLOWSYNC   0x00000008UL
#define LOG_MASK_KEEPBUF    0x00000010UL
#define LOG_MASK_SYSFS      0x00000020UL
#define LOG_MASK_VINFO      0x00000040UL
#define LOG_MASK_MODULE     0x00000080UL
#define LOG_MASK_VPP        0x00000100UL

#define LOG_MASK_DESC \
"[0x01]:TIMESTAMP,[0x02]:FRAMEINFO,[0x04]:FRAMESKIP,[0x08]:SLOWSYNC,[0x10]:KEEPBUF,[0x20]:SYSFS,[0x40]:VINFO,[0x80]:MODULE."
