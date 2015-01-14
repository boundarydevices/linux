/*
 * Platform data for ADS1000 driver
 *
 * Copyright (C) Boundary Devices, 2013
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _ADS1000_H
#define _ADS1000_H

#define ADS1000_DEV_ID 0x49

/*
 * provide this structure to allow conversion
 * from A/D readings to mV
 */
struct ads1000_platform_data {
	int	 numerator;
	unsigned denominator;
};

#endif /* LINUX_ADS1015_H */
