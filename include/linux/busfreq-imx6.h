/*
 * Copyright 2012-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_MXC_BUSFREQ_H__
#define __ASM_ARCH_MXC_BUSFREQ_H__

/*
 * This enumerates busfreq mode.
 */
enum bus_freq_mode {
	BUS_FREQ_HIGH,
	BUS_FREQ_MED,
	BUS_FREQ_AUDIO,
	BUS_FREQ_LOW,
};
void request_bus_freq(enum bus_freq_mode mode);
void release_bus_freq(enum bus_freq_mode mode);
#endif
