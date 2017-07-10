/*
 * drivers/amlogic/audioinfo/audio_data.h
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

#ifndef _AUDIO_DATA_ANDROID_H_
#define _AUDIO_DATA_ANDROID_H_

#include <linux/cdev.h>
#include <linux/semaphore.h>

#define AUDIO_DATA_DEVICE_NODE_NAME "audio_data_debug"
#define AUDIO_DATA_DEVICE_FILE_NAME "audio_data_debug"
#define AUDIO_DATA_DEVICE_PROC_NAME "audio_data_debug"
#define AUDIO_DATA_DEVICE_CLASS_NAME "audio_data_debug"

#define EFUSE_DOLBY_AUDIO_EFFECT 1

struct audio_data_android_dev {
	unsigned int val;
	struct semaphore sem;
	struct cdev dev;
};
#endif
