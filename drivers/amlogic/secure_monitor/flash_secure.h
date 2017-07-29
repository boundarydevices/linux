/*
 * drivers/amlogic/secure_monitor/flash_secure.h
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

#ifndef _FLASH_SECURE_H_
#define _FLASH_SECURE_H_


void init_mutex(unsigned int *lock);
void lock_mutex(unsigned int *lock);
int lock_mutex_try(unsigned int *lock);
unsigned int unlock_mutex(unsigned int *lock);
void write_to_flash(unsigned char *psrc, unsigned  int size);

#ifdef CONFIG_AMLOGIC_M8B_NAND
int secure_storage_nand_write(char *buf, unsigned int len);
#endif

#ifdef CONFIG_SPI_NOR_SECURE_STORAGE
int secure_storage_spi_write(u8 *buf, u32 len);
#endif

#ifdef CONFIG_EMMC_SECURE_STORAGE
int mmc_secure_storage_ops(unsigned char *buf, unsigned int len, int wr_flag);
#endif

#endif

