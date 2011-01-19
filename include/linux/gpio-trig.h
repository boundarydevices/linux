/*
 * gpio-trig.h
 *
 * Copyright (c) 2007-2008  Boundary Devices
 *
 * Author: Eric Nelson <eric.nelson@boundarydevices.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __GPIO_TRIG_H
#define __GPIO_TRIG_H

#define MAX_PINS 7

struct trigger_t {
	unsigned char trigger_pin ;
	unsigned char rising_edge ;
	unsigned char num_pins ;
	unsigned char pins[MAX_PINS];
};

/*
 * Input: bytes to allocate
 * Output: offset into frame-buffer memory (or NULL)
 */
#define GPIO_TRIG_CFG  _IOWR('\xbd', 0x01, struct trigger_t)

#endif /* __GPIO_TRIG_H */
