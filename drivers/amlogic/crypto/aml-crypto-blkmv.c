/*
 * drivers/amlogic/crypto/aml-crypto-blkmv.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/hw_random.h>
#include <linux/platform_device.h>

#include <linux/device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/crypto.h>
#include <linux/cryptohash.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/internal/hash.h>
#include "aml-crypto-blkmv.h"

// ---------------------------------------------------------------------
// DMA descriptor table support
// ---------------------------------------------------------------------
unsigned long swap_ulong32(unsigned long val)
{
	unsigned char *p = (unsigned char *)&val;

	return ((*p)<<24)+((*(p+1))<<16)+((*(p+2))<<8)+((*(p+3))<<0);
}

// --------------------------------------------
//          NDMA_set_table_position
// --------------------------------------------
void ndma_set_table_position(unsigned long thread_num,
		unsigned long table_start, unsigned long size)
{
	unsigned long table_end;

	table_end = table_start + size;
	switch (thread_num) {
	case 3:
		cbus_wr(NDMA_THREAD_TABLE_START3, table_start);
		cbus_wr(NDMA_THREAD_TABLE_END3, table_end);
		/* Pulse thread init to register the new start/end locations */
		cbus_wr(NDMA_THREAD_REG,
				cbus_rd(NDMA_THREAD_REG) | (1 << 27));
		break;
	case 2:
		cbus_wr(NDMA_THREAD_TABLE_START2, table_start);
		cbus_wr(NDMA_THREAD_TABLE_END2, table_end);
		/* Pulse thread init to register the new start/end locations */
		cbus_wr(NDMA_THREAD_REG,
				cbus_rd(NDMA_THREAD_REG) | (1 << 26));
		break;
	case 1:
		cbus_wr(NDMA_THREAD_TABLE_START1, table_start);
		cbus_wr(NDMA_THREAD_TABLE_END1, table_end);
		/* Pulse thread init to register the new start/end locations */
		cbus_wr(NDMA_THREAD_REG,
				cbus_rd(NDMA_THREAD_REG) | (1 << 25));
		break;
	case 0:
		cbus_wr(NDMA_THREAD_TABLE_START0, table_start);
		cbus_wr(NDMA_THREAD_TABLE_END0, table_end);
		/* Pulse thread init to register the new start/end locations */
		cbus_wr(NDMA_THREAD_REG,
				cbus_rd(NDMA_THREAD_REG) | (1 << 24));
		break;
	}
}

/* --------------------------------------------
 *  ndma_add_descriptor_1d
 *  -------------------------------------------
 */
void  ndma_add_descriptor_1d(
		unsigned long   table,
		unsigned long   irq,
		unsigned long   restart,
		unsigned long   pre_endian,
		unsigned long   post_endian,
		unsigned long   type,
		unsigned long   bytes_to_move,
		unsigned long   inlinetype,
		unsigned long   src_addr,
		unsigned long   dest_addr)
{
	unsigned long *p = (unsigned long *) table;
	unsigned long curr_key;
	unsigned long keytype;
	unsigned long cryptodir;
	unsigned long mode;

	(*p++) =  (0x01 << 30)        | /* owned by CPU */
		(pre_endian << 27)    |
		(0 << 26)             |
		(0 << 25)             |
		(inlinetype << 22)    | /* TDES in-place processing */
		(irq  << 21)          |
		(0 << 0);                     /* thread slice */

	(*p++) =  src_addr;

	(*p++) =  dest_addr;

	(*p++) =  bytes_to_move & 0x01FFFFFF;

	(*p++) =  0x00000000;       /* 2D entry */

	(*p++) =  0x00000000;       /* 2D entry */

	/* Prepare the pointer for the next descriptor boundary
	 * inline processing + bytes to move extension
	 */
	switch (inlinetype) {
	case INLINE_TYPE_TDES:
		curr_key = (type & 0x3);
		(*p++) =  (restart << 6)      |
			(curr_key << 3)     |
			(post_endian << 0);
		break;
	case INLINE_TYPE_DIVX:
		(*p++) = post_endian & 0x7;
		break;
	case INLINE_TYPE_CRC:
		(*p++) = post_endian & 0x7;
		break;
	case INLINE_TYPE_AES:
		keytype = (type & 0x3);
		cryptodir = ((type >> 2) & 1);
		mode = ((type >> 3) & 3);
		(*p++) = (pre_endian << 0) |
			(post_endian << 4)  |
			(keytype << 8)  |
			(cryptodir << 10)|
			(((restart) ? 1 : 0) << 11)|
			(mode << 12) |
			(0 << 14) | /* 128 bit ctr */
			(0xf << 16); /* big endian */
		break;
	default:
		*p++ = 0;
		break;
	}
	*p = 0;

}

void ndma_set_table_count(unsigned long thread_num, unsigned int cnt)
{
	cbus_wr(NDMA_TABLE_ADD_REG, (thread_num << 8) | (cnt << 0));
}

/* --------------------------------------------
 *  ndma_start()
 *  --------------------------------------------
 *  Start the block move procedure
 */
void ndma_start(unsigned long thread_num)
{
	/* Enable */
	cbus_wr(NDMA_CNTL_REG0,
			(cbus_rd(NDMA_CNTL_REG0)  | (1 << NDMA_ENABLE)));
	cbus_wr(NDMA_THREAD_REG,
			(cbus_rd(NDMA_THREAD_REG)
			 | (1 << (thread_num + 8))));
}

void ndma_stop(unsigned long thread_num)
{
	cbus_wr(NDMA_THREAD_REG,
			cbus_rd(NDMA_THREAD_REG)
			& ~(1 << (thread_num + 8)));
	/* If no threads enabled, then shut down the DMA engine completely */
	if (!(cbus_rd(NDMA_THREAD_REG) & (0xF << 8)))
		cbus_wr(NDMA_CNTL_REG0,
				cbus_rd(NDMA_CNTL_REG0)
				& ~(1 << NDMA_ENABLE));
}

/* --------------------------------------------
 *  ndma_wait_for_completion()
 *  --------------------------------------------
 *  Wait for all block moves to complete
 */

void ndma_wait_for_completion(unsigned long thread_num)
{
	while ((cbus_rd(NDMA_TABLE_ADD_REG) & (0xF << (thread_num*8))))
		;
}

