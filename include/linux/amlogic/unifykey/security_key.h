/*
 * include/linux/amlogic/unifykey/security_key.h
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

#ifndef _AML_SECURITY_KEY_H_
#define _AML_SECURITY_KEY_H_

/* internal return value*/
#define RET_OK		0
#define RET_EFAIL	1	/*not found*/
#define RET_EINVAL	2	/*name length*/
#define RET_EMEM	3	/*no enough memory*/
#define RET_EUND	0xff

#define SMC_UNK		0xffffffff

/* keyattr: 0: normal, 1: secure*/
int32_t secure_storage_write(uint8_t *keyname, uint8_t *keybuf,
			uint32_t keylen, uint32_t keyattr);
int32_t secure_storage_read(uint8_t *keyname, uint8_t *keybuf,
			uint32_t keylen, uint32_t *readlen);
int32_t secure_storage_verify(uint8_t *keyname, uint8_t *hashbuf);
int32_t secure_storage_query(uint8_t *keyname, uint32_t *retval);
int32_t secure_storage_tell(uint8_t *keyname, uint32_t *retval);
int32_t secure_storage_status(uint8_t *keyname, uint32_t *retval);
void *secure_storage_getbuffer(uint32_t *size);
void secure_storage_notifier_ex(uint32_t storagesize);
void secure_storage_type(uint32_t is_emmc);
int32_t secure_storage_set_enctype(uint32_t type);
int32_t secure_storage_get_enctype(void);
int32_t secure_storage_version(void);
#endif
