/*
 * drivers/amlogic/media/common/codec_mm/codec_mm_priv.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#ifndef AML_MM_HEADER
#define AML_MM_HEADER
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#include<linux/printk.h>

#ifndef INFO_PREFIX
#define INFO_PREFIX "codec_mm"
#endif

/*#define codec_print(KERN_LEVEL, args...) \*/
		/*printk(KERN_LEVEL INFO_PREFIX ":" args)*/


#define codec_info(args...) pr_info(args)
#define codec_err(args...)  pr_err(args)
#define codec_waring(args...)  pr_warn(args)

/*
 *#ifdef pr_info
 *#undef pr_info
 *#undef pr_err
 *#undef pr_warn
 *#undef pr_warning
 *
 *#define pr_info(args...) codec_info(args)
 *#define pr_err(args...) codec_err(args)
 *#define pr_warn(args...) codec_waring(args)
 *#define pr_warning pr_warn
 *
 *#endif
*/


extern void dma_clear_buffer(struct page *page, size_t size);

u32 codec_mm_get_sc_debug_mode(void);
u32 codec_mm_get_keep_debug_mode(void);

#endif /**/
