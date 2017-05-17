/*
 * drivers/amlogic/unifykey/v7/key_storage/crypto_api.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/crypto.h>
#include <crypto/internal/hash.h>
#include "crypto_api.h"

static u8 pkey[32] = {0xc1, 0xb2, 0x33, 0x34, 5, 6, 7, 8,
		      9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x11,
		      0xc3, 0xb4, 0x32, 0x34, 5, 6, 7, 8,
		      9, 0xa, 0xb, 0xc1, 0xd, 0xe1, 0xf2, 0x12};

int do_aes_internal(unsigned char enc_flag, unsigned char *in,
		int in_len, unsigned char *out, int *out_len)
{
	int                  ret = -1;
	int                  load_len = 0;
	unsigned char        blk_buf[64];
	struct crypto_cipher *tfm;

	if (!in || !out || !out_len)
		return ret;

	memset(out, 0x00, in_len);

	tfm = crypto_alloc_cipher("aes", 4, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm))
		return PTR_ERR(tfm);

	crypto_cipher_setkey(tfm, pkey, 32);

	load_len = AES_BLOCK_SIZE;
	*out_len = 0;

	do {
		memset(blk_buf, 0, sizeof(blk_buf));
		memcpy(blk_buf, in + *out_len, load_len);

		if (enc_flag)
			crypto_cipher_encrypt_one(tfm, out + *out_len, blk_buf);
		else
			crypto_cipher_decrypt_one(tfm, out + *out_len, blk_buf);
		*out_len += load_len;

		if (*out_len + load_len > in_len)
			load_len = in_len - *out_len;
	} while (*out_len != in_len);

	ret = 0;

	crypto_free_cipher(tfm);
	return ret;
}

int sha256(uint8_t *in, uint32_t len, unsigned char output[32])
{
	int                  ret = -1;
	struct crypto_shash  *tfm = NULL;
	struct shash_desc    *desc = NULL;


	if (!in)
		return ret;

	tfm = crypto_alloc_shash("sha256-generic", 0, 0);
	if (IS_ERR(tfm))
		return PTR_ERR(tfm);

	desc = kmalloc(sizeof(*desc) + crypto_shash_descsize(tfm), GFP_KERNEL);
	if (IS_ERR(desc)) {
		crypto_free_shash(tfm);
		return PTR_ERR(desc);
	}
	desc->tfm = tfm;
	desc->flags = crypto_shash_get_flags(tfm);

	ret = crypto_shash_digest(desc, in, len, output);
	crypto_free_shash(desc->tfm);
	kfree(desc);

	return ret;
}

