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
#include <linux/scatterlist.h>
#include <crypto/internal/hash.h>
#include "crypto_api.h"



static u8 const fixed_iv[16] = {0xAD, 0x93, 0x00, 0xC4,
				0x8E, 0x50, 0x20, 0xC5,
				0x3F, 0xBF, 0x23, 0x32,
				0x80, 0x5A, 0xC6, 0xDF};
static u8 const fixed_key[32] = {0x2F, 0x7D, 0x49, 0xD9,
				 0x15, 0x8B, 0x7F, 0x04,
				 0x2C, 0x80, 0xB0, 0x62,
				 0x78, 0x25, 0x8D, 0x9C,
				 0x13, 0x22, 0x02, 0x4A,
				 0x55, 0x23, 0xBB, 0xCB,
				 0xF1, 0xFB, 0x2A, 0xCC,
				 0xBB, 0x95, 0xF4, 0x50};



/* AES-256-CBC */
int do_aes_internal(unsigned char enc_flag, unsigned char *in,
		int in_len, unsigned char *out, int *out_len)
{
	int                     ret = -1;
	struct crypto_blkcipher *tfm;
	struct blkcipher_desc   desc;
	struct scatterlist	sg_in;
	struct scatterlist	sg_out;

	if (!in || !out || !out_len)
		return ret;

	tfm = crypto_alloc_blkcipher("cbc(aes)", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm))
		return PTR_ERR(tfm);
	desc.tfm = tfm;
	desc.flags = 0x00;

	sg_init_one(&sg_in, in, in_len);
	sg_init_one(&sg_out, out, in_len);
	crypto_blkcipher_setkey(tfm, fixed_key, 32);
	crypto_blkcipher_set_iv(tfm, fixed_iv, 16);

	if (enc_flag)
		crypto_blkcipher_encrypt(&desc, &sg_out, &sg_in, sg_in.length);
	else
		crypto_blkcipher_decrypt(&desc, &sg_out, &sg_in, sg_in.length);

	*out_len = sg_out.length;
	crypto_free_blkcipher(tfm);

	ret = 0;
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

