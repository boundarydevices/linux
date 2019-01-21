/*
 * include/linux/amlogic/vad_api.h
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

#ifndef __VAD_API_H__
#define __VAD_API_H__

extern int register_vad_callback(int (*callback)(char *, int, int, int, int));
extern void unregister_vad_callback(void);

#endif /* __VAD_API_H__ */
