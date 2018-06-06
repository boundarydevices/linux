/*
 * drivers/amlogic/media/gdc/inc/sys/system_gdc_io.h
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

#ifndef __SYSTEM_GDC_IO_H__
#define __SYSTEM_GDC_IO_H__

/**
 *   Read 32 bit word from gdc memory
 *
 *   This function returns a 32 bits word from GDC memory with a given offset.
 *
 *   @param addr - the offset in GDC memory to read 32 bits word.
 *
 *   @return 32 bits memory value
 */
uint32_t system_gdc_read_32(uint32_t addr);


/**
 *   Write 32 bits word to gdc memory
 *
 *   This function writes a 32 bits word to GDC memory with a given offset.
 *
 *   @param addr - the offset in GDC memory to write data.
 *   @param data - data to be written
 */
void system_gdc_write_32(uint32_t addr, uint32_t data);


#endif /* __SYSTEM_GDC_IO_H__ */
