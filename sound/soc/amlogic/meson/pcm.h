/*
 * sound/soc/amlogic/meson/pcm.h
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

#ifndef __AML_PCM_H__
#define __AML_PCM_H__

struct aml_pcm_runtime_data {
	spinlock_t	lock;
	dma_addr_t	buffer_start;
	unsigned int	buffer_size;
	unsigned int	buffer_offset;
	unsigned int	data_size;
	unsigned int	running;
	unsigned int	timer_period;
	unsigned int	period_elapsed;

	struct timer_list	timer;
	struct snd_pcm_substream *substream;
};

#endif
