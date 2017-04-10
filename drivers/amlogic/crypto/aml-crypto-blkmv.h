/*
 * drivers/amlogic/crypto/aml-crypto-blkmv.h
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

#ifndef _AML_CRYPTO_H_
#define _AML_CRYPTO_H_

#include <linux/amlogic/iomap.h>
#define AML_CRYPTO_DEBUG    0

#define AM_RING_OSC_REG0 0x207f

#define cbus_wr(addr, data)  \
	aml_write_cbus(addr, data)
#define cbus_rd(addr)        \
	aml_read_cbus(addr)

#define NDMA_TABLE_SIZE     (32)
/* driver logic flags */
#define TDES_KEY_LENGTH 32
#define TDES_MIN_BLOCK_SIZE 8

#define MODE_ECB 0
#define MODE_CBC 1
#define MODE_CTR 2

#define DIR_ENCRYPT 0
#define DIR_DECRYPT 1

#define INLINE_TYPE_NORMAL       0
#define INLINE_TYPE_TDES            1
#define INLINE_TYPE_DIVX              2
#define INLINE_TYPE_CRC              3
#define INLINE_TYPE_AES              4

#define NDMA_CNTL_REG0                             0x2270
    #define NDMA_AES_STATUS             12
    #define NDMA_ENABLE                 14
    #define NDMA_STATUS                 26
#define NDMA_TABLE_ADD_REG                         0x2272
#define NDMA_TDES_KEY_LO                           0x2273
#define NDMA_TDES_KEY_HI                           0x2274
#define NDMA_TDES_CONTROL                          0x2275
#define NDMA_AES_CONTROL                           0x2276
#define NDMA_AES_RK_FIFO                           0x2277
#define NDMA_CRC_OUT                               0x2278
#define NDMA_THREAD_REG                            0x2279
#define NDMA_THREAD_TABLE_START0                   0x2280
#define NDMA_THREAD_TABLE_CURR0                    0x2281
#define NDMA_THREAD_TABLE_END0                     0x2282
#define NDMA_THREAD_TABLE_START1                   0x2283
#define NDMA_THREAD_TABLE_CURR1                    0x2284
#define NDMA_THREAD_TABLE_END1                     0x2285
#define NDMA_THREAD_TABLE_START2                   0x2286
#define NDMA_THREAD_TABLE_CURR2                    0x2287
#define NDMA_THREAD_TABLE_END2                     0x2288
#define NDMA_THREAD_TABLE_START3                   0x2289
#define NDMA_THREAD_TABLE_CURR3                    0x228a
#define NDMA_THREAD_TABLE_END3                     0x228b
#define NDMA_CNTL_REG1                             0x228c
#define NDMA_AES_KEY_0                             0x2290
#define NDMA_AES_KEY_1                             0x2291
#define NDMA_AES_KEY_2                             0x2292
#define NDMA_AES_KEY_3                             0x2293
#define NDMA_AES_KEY_4                             0x2294
#define NDMA_AES_KEY_5                             0x2295
#define NDMA_AES_KEY_6                             0x2296
#define NDMA_AES_KEY_7                             0x2297
#define NDMA_AES_IV_0                              0x2298
#define NDMA_AES_IV_1                              0x2299
#define NDMA_AES_IV_2                              0x229a
#define NDMA_AES_IV_3                              0x229b
#define NDMA_AES_REG0                             0x229c

#define TDES_THREAD_INDEX 2
#define AES_THREAD_INDEX 3
#define SEC_ALLOWED_MASK 0xc /* thread 0 and 1 are secure*/

unsigned long swap_ulong32(unsigned long val);
void ndma_set_table_position(unsigned long thread_num,
		unsigned long table_start, unsigned long size);
void ndma_set_table_position_secure(unsigned long thread_num,
		unsigned long table_start, unsigned long size);
void ndma_add_descriptor_1d(
		unsigned long   table,
		unsigned long   irq,
		unsigned long   restart,
		unsigned long   pre_endian,
		unsigned long   post_endian,
		unsigned long   type,
		unsigned long   bytes_to_move,
		unsigned long   inlinetype,
		unsigned long   src_addr,
		unsigned long   dest_addr);

void ndma_set_table_count(unsigned long thread_num, unsigned int cnt);
void ndma_start(unsigned long thread_num);
void ndma_stop(unsigned long thread_num);
void ndma_wait_for_completion(unsigned long thread_num);
#endif
