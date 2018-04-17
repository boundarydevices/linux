/*
 * amlogic atv demod driver
 *
 * Author: nengwen.chen <nengwen.chen@amlogic.com>
 *
 *
 * Copyright (C) 2018 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ATV_DEMOD_DEBUG_H__
#define __ATV_DEMOD_DEBUG_H__

extern int atvdemod_debug_en;

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#undef pr_info
#define pr_info(fmt, ...)\
	do {\
		if (1)\
			printk(fmt, ##__VA_ARGS__);\
	} while (0)

#undef pr_dbg
#define pr_dbg(fmt, ...) \
	do {\
		if (atvdemod_debug_en)\
			printk(fmt, ##__VA_ARGS__);\
	} while (0)

#undef pr_err
#define pr_err(fmt, ...) \
	do {\
		if (1)\
			printk(fmt, ##__VA_ARGS__);\
	} while (0)

#endif /* __ATV_DEMOD_DEBUG_H__ */
