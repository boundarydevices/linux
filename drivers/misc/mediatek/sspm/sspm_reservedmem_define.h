/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */
#ifndef _SSPM_RESERVEDMEM_DEFINE_H_
#define _SSPM_RESERVEDMEM_DEFINE_H_
#include <sspm_reservedmem.h>

enum sspm_reserve_mem_id {
	SSPM_MEM_ID   = 0,
	PWRAP_MEM_ID  = 1,
	PMIC_MEM_ID   = 2,
	UPD_MEM_ID    = 3,
	QOS_MEM_ID    = 4,
	SWPM_MEM_ID   = 5,
	SMI_MEM_ID    = 6,
	GPU_MEM_ID    = 7,
	SLBC_MEM_ID   = 8,
	NUMS_MEM_ID,
};

#define SSPM_PLT_LOGGER_BUF_LEN 0x100000

#ifdef _SSPM_INTERNAL_
/* The total size of sspm_reserve_mblock should less equal than
 * reserve-memory-sspm_share of device tree
 */
static struct sspm_reserve_mblock sspm_reserve_mblock[NUMS_MEM_ID] = {
	{
		.num = SSPM_MEM_ID,
		.size = 0x100 + SSPM_PLT_LOGGER_BUF_LEN,
		/* logger header + 1M log buffer */
	},
	{
		.num = PWRAP_MEM_ID,
		.size = 0x000,  /* 0k */
	},
	{
		.num = PMIC_MEM_ID,
		.size = 0x000,  /* 0K */
	},
	{
		.num = UPD_MEM_ID,
		.size = 0x1800, /* 6K */
	},
	{
		.num = QOS_MEM_ID,
		.size = 0x1000, /* 4K */
	},
	{
		.num = SWPM_MEM_ID,
		.size = 0x2000,  /* 8K */
	},
	{
		.num = SMI_MEM_ID,
		.size = 0x9000, /* 36K */
	},
	{
		.num = GPU_MEM_ID,
		.size = 0x1000,  /* 4K */
	},
	{
		.num = SLBC_MEM_ID,
		.size = 0x0,  /* 0K */
	},
	/* TO align 64K, total is 1M+64K.  */
};
#endif
#endif
