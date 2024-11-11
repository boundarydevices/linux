/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Unisoc (Shanghai) Technologies Co., Ltd
 */

#ifndef _ANDROID_DEBUG_SYMBOLS_H
#define _ANDROID_DEBUG_SYMBOLS_H

enum android_debug_symbol {
	ADS_PER_CPU_START = 0,
	ADS_PER_CPU_END,
	ADS_TEXT,
	ADS_SEND,
	ADS_MEM_BLOCK,
	ADS_INIT_MM,
	ADS_ITERATE_SUPERS,
	ADS_DROP_SLAB,
	ADS_FREE_PAGES,
	ADS_COMPACT_PAGES,
	ADS_SHOW_MEM, /* for debugging memory usage */
	ADS_TOTAL_CMA, /* for debugging total cma pages */
	ADS_SLAB_CACHES, /* for debugging slab */
	ADS_SLAB_MUTEX, /* for debugging slab */
#ifndef __GENKSYMS__
	ADS_START_RO_AFTER_INIT,
	ADS_END_RO_AFTER_INIT,
#endif
	ADS_END
};

enum android_debug_per_cpu_symbol {
	ADS_IRQ_STACK_PTR = 0,
	ADS_DEBUG_PER_CPU_END
};

#ifdef CONFIG_ANDROID_DEBUG_SYMBOLS

void *android_debug_symbol(enum android_debug_symbol symbol);
void *android_debug_per_cpu_symbol(enum android_debug_per_cpu_symbol symbol);

#else /* !CONFIG_ANDROID_DEBUG_SYMBOLS */

static inline void *android_debug_symbol(enum android_debug_symbol symbol)
{
	return NULL;
}
static inline void *android_debug_per_cpu_symbol(enum android_debug_per_cpu_symbol symbol)
{
	return NULL;
}

#endif /* CONFIG_ANDROID_DEBUG_SYMBOLS */

#endif /* _ANDROID_DEBUG_SYMBOLS_H */
