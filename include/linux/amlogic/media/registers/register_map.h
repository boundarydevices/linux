/*
 * include/linux/amlogic/media/registers/register_map.h
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

#ifndef CODECIO_REGISTER_MAP_H
#define CODECIO_REGISTER_MAP_H
int codecio_read_cbus(unsigned int reg);
void codecio_write_cbus(unsigned int reg, unsigned int val);
int codecio_read_dosbus(unsigned int reg);
void codecio_write_dosbus(unsigned int reg, unsigned int val);
int codecio_read_hiubus(unsigned int reg);
void codecio_write_hiubus(unsigned int reg, unsigned int val);
int codecio_read_aobus(unsigned int reg);
void codecio_write_aobus(unsigned int reg, unsigned int val);
int codecio_read_vcbus(unsigned int reg);
void codecio_write_vcbus(unsigned int reg, unsigned int val);
int codecio_read_dmcbus(unsigned int reg);
void codecio_write_dmcbus(unsigned int reg, unsigned int val);
int codecio_read_parsbus(unsigned int reg);
void codecio_write_parsbus(unsigned int reg, unsigned int val);
int codecio_read_aiubus(unsigned int reg);
void codecio_write_aiubus(unsigned int reg, unsigned int val);
int codecio_read_demuxbus(unsigned int reg);
void codecio_write_demuxbus(unsigned int reg, unsigned int val);
int codecio_read_resetbus(unsigned int reg);
void codecio_write_resetbus(unsigned int reg, unsigned int val);
#endif
