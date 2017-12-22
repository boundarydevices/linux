/*
 * drivers/amlogic/unifykey/amlkey_if.h
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

#ifndef __AMLKEY_IF_H__
#define __AMLKEY_IF_H__

/*
 * init
 */
int32_t amlkey_init_gen(uint8_t *seed, uint32_t len, int encrypt_type);
int32_t amlkey_init_m8b(uint8_t *seed, uint32_t len, int encrypt_type);

/*
 * query if the key already programmed
 * exsit 1, non 0
 */
int32_t amlkey_isexsit(const uint8_t *name);
/*
 * query if the prgrammed key is secure
 * secure 1, non 0
 */
int32_t amlkey_issecure(const uint8_t *name);
/*
 * query if the prgrammed key is encrypt
 * return encrypt 1, non 0;
 */
int32_t amlkey_isencrypt(const uint8_t *name);
/*
 * actual bytes of key value
 */
ssize_t amlkey_size(const uint8_t *name);
/*
 * read non-secure key in bytes, return byets readback actully.
 */
ssize_t amlkey_read(const uint8_t *name, uint8_t *buffer, uint32_t len);

/*
 * write key with attr in bytes , return bytes readback actully
 *	attr: bit0, secure/non-secure
 *		  bit8, encrypt/non-encrypt
 */
ssize_t amlkey_write(const uint8_t *name,
		uint8_t *buffer,
		uint32_t len,
		uint32_t attr);

/*
 * get the hash value of programmed secure key | 32bytes length, sha256
 */
int32_t amlkey_hash_4_secure(const uint8_t *name, uint8_t *hash);

extern int32_t nand_key_read(uint8_t *buf,
			uint32_t len, uint32_t *actual_length);

extern int32_t nand_key_write(uint8_t *buf,
			uint32_t len, uint32_t *actual_length);

extern int32_t emmc_key_read(uint8_t *buf,
			uint32_t len, uint32_t *actual_length);

extern int32_t emmc_key_write(uint8_t *buf,
			uint32_t len, uint32_t *actual_length);

#endif

