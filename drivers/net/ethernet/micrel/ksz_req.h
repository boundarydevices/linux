/**
 * Microchip driver request common header
 *
 * Copyright (c) 2015-2018 Microchip Technology Inc.
 *	Tristram Ha <Tristram.Ha@microchip.com>
 *
 * Copyright (c) 2009-2014 Micrel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef KSZ_REQ_H
#define KSZ_REQ_H

enum {
	DEV_IOC_OK,
	DEV_IOC_INVALID_SIZE,
	DEV_IOC_INVALID_CMD,
	DEV_IOC_INVALID_LEN,

	DEV_IOC_LAST
};

enum {
	DEV_CMD_INFO,
	DEV_CMD_GET,
	DEV_CMD_PUT,

	DEV_CMD_LAST
};

enum {
	DEV_INFO_INIT,
	DEV_INFO_EXIT,
	DEV_INFO_QUIT,
	DEV_INFO_NOTIFY,
	DEV_INFO_PORT,

	DEV_INFO_LAST
};

struct ksz_request {
	int size;
	int cmd;
	int subcmd;
	int output;
	int result;
	union {
		u8 data[1];
		int num[1];
	} param;
};

/* Some compilers in different OS cannot have zero number in array. */
#define SIZEOF_ksz_request	(sizeof(struct ksz_request) - sizeof(int))

/* Not used in the driver. */

#ifndef MAX_REQUEST_SIZE
#define MAX_REQUEST_SIZE	20
#endif

struct ksz_request_actual {
	int size;
	int cmd;
	int subcmd;
	int output;
	int result;
	union {
		u8 data[MAX_REQUEST_SIZE];
		int num[MAX_REQUEST_SIZE / sizeof(int)];
	} param;
};

#define DEV_IOC_MAGIC			0x92

#define DEV_IOC_MAX			1


struct ksz_read_msg {
	u16 len;
	u8 data[0];
} __packed;


enum {
	DEV_MOD_BASE,
	DEV_MOD_PTP,
	DEV_MOD_MRP,
	DEV_MOD_DLR,
	DEV_MOD_HSR,
};

struct ksz_resp_msg {
	u16 module;
	u16 cmd;
	union {
		u32 data[1];
	} resp;
} __packed;

#endif
