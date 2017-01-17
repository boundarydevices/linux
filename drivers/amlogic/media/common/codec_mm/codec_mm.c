/*
 * drivers/amlogic/media/common/codec_mm/codec_mm.c
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
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt_env.h>
#include <linux/of_reserved_mem.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/genalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>

#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/codec_mm_scatter.h>

#include "codec_mm_priv.h"
#include "codec_mm_scatter_priv.h"
#include "codec_mm_keeper_priv.h"

#define TVP_POOL_NAME "TVP_POOL"
#define CMA_RES_POOL_NAME "CMA_RES"

#define RES_IS_MAPED

#define ALLOC_MAX_RETRY 1

#define CODEC_MM_FOR_DMA_ONLY(flags) \
	((flags & CODEC_MM_FLAGS_FROM_MASK) == CODEC_MM_FLAGS_DMA)

#define CODEC_MM_FOR_CPU_ONLY(flags) \
	((flags & CODEC_MM_FLAGS_FROM_MASK) == CODEC_MM_FLAGS_CPU)

#define CODEC_MM_FOR_DMACPU(flags) \
	((flags & CODEC_MM_FLAGS_FROM_MASK) == CODEC_MM_FLAGS_DMA_CPU)

#define RESERVE_MM_ALIGNED_2N	17

#define RES_MEM_FLAGS_HAVE_MAPED 0x4
static int dump_mem_infos(void *buf, int size);

/*
 *debug_mode:
 *
 *disable reserved:1
 *disable cma:2
 *disable sys memory:4
 *disable half memory:8,
 *	only used half memory,for debug.
 *	return nomem,if alloced > total/2;
 *dump memory info on failed:0x10,
 *trace memory alloc/free info:0x20,
 */
static u32 debug_mode;

static u32 debug_sc_mode;
u32 codec_mm_get_sc_debug_mode(void)
{
	return debug_sc_mode;
}
EXPORT_SYMBOL(codec_mm_get_sc_debug_mode);

static u32 debug_keep_mode;
u32 codec_mm_get_keep_debug_mode(void)
{
	return debug_keep_mode;
}
EXPORT_SYMBOL(codec_mm_get_keep_debug_mode);

#define TVP_MAX_SLOT 8
struct extpool_mgt_s {
	struct gen_pool *gen_pool[TVP_MAX_SLOT];
	struct codec_mm_s *mm[TVP_MAX_SLOT];
	int slot_num;
	int default_size;
	int default_4k_size;
	int alloced_size;
	int total_size;
	spinlock_t lock;
};

struct codec_mm_mgt_s {
	struct cma *cma;
	struct device *dev;
	struct list_head mem_list;
	struct gen_pool *res_pool;
	struct extpool_mgt_s tvp_pool;
	struct extpool_mgt_s cma_res_pool;
	struct reserved_mem rmem;
	int total_codec_mem_size;
	int total_alloced_size;
	int total_cma_size;
	int total_reserved_size;
	int max_used_mem_size;

	int alloced_res_size;
	int alloced_cma_size;
	int alloced_sys_size;
	int alloced_for_sc_size;
	int alloced_for_sc_cnt;

	int alloc_from_sys_pages_max;
	int enable_kmalloc_on_nomem;
	int res_mem_flags;

	/*1:for 1080p,2:for 4k */
	int tvp_enable;
	/*1:for 1080p,2:for 4k */
	int fastplay_enable;
	spinlock_t lock;
};

#define PHY_OFF() offsetof(struct codec_mm_s, phy_addr)
#define HANDLE_OFF() offsetof(struct codec_mm_s, mem_handle)
#define VADDR_OFF() offsetof(struct codec_mm_s, vbuffer)
#define VAL_OFF_VAL(mem, off) (*(unsigned long *)((unsigned long)(mem) + off))

static struct codec_mm_mgt_s *get_mem_mgt(void)
{
	static struct codec_mm_mgt_s mgt;
	static int inited;

	if (!inited) {
		memset(&mgt, 0, sizeof(struct codec_mm_mgt_s));
		inited++;
	}
	return &mgt;
};

static void *codec_mm_extpool_alloc(struct extpool_mgt_s *tvp_pool,
	void **from_pool, int size)
{
	int i = 0;
	void *handle = NULL;

	for (i = 0; i < tvp_pool->slot_num; i++) {
		if (!tvp_pool->gen_pool[i])
			return NULL;
		handle = (void *)gen_pool_alloc(tvp_pool->gen_pool[i], size);
		if (handle) {
			*from_pool = tvp_pool->gen_pool[i];
			return handle;
		}
	}
	return NULL;
}

static void *codec_mm_extpool_free(struct gen_pool *from_pool,
	void *handle, int size)
{
	gen_pool_free(from_pool, (unsigned long)handle, size);
	return 0;
}

/*
 *have_space:
 *1:	can alloced from reserved
 *2:	can alloced from cma
 *4:  can alloced from sys.
 *8:  can alloced from tvp.
 *
 *flags:
 *	is tvp = (flags & 1)
 */
static int codec_mm_alloc_pre_check_in(struct codec_mm_mgt_s *mgt,
	int need_size, int flags)
{
	int have_space = 0;
	int aligned_size = PAGE_ALIGN(need_size);

	if (aligned_size <= mgt->total_reserved_size - mgt->alloced_res_size)
		have_space |= 1;
	if (aligned_size <= mgt->cma_res_pool.total_size -
		mgt->cma_res_pool.alloced_size)
		have_space |= 1;

	if (aligned_size <= mgt->total_cma_size - mgt->alloced_cma_size)
		have_space |= 2;

	if (aligned_size / PAGE_SIZE <= mgt->alloc_from_sys_pages_max)
		have_space |= 4;
	if (aligned_size <= mgt->tvp_pool.total_size -
		mgt->tvp_pool.alloced_size)
		have_space |= 8;

	if (flags & 1)
		have_space = have_space & 8;

	if (debug_mode & 0xf) {
		have_space = have_space & (~(debug_mode & 1));
		have_space = have_space & (~(debug_mode & 2));
		have_space = have_space & (~(debug_mode & 4));
		if (debug_mode & 8) {
			if (mgt->total_alloced_size >
				mgt->total_codec_mem_size / 2) {
				pr_info("codec mm memory is limited by %d, (bit8 enable)\n",
					debug_mode);
				have_space = 0;
			}
		}
	}

	return have_space;
}

static int codec_mm_alloc_in(struct codec_mm_mgt_s *mgt, struct codec_mm_s *mem)
{
	int try_alloced_from_sys = 0;
	int try_alloced_from_reserved = 0;
	int align_2n = mem->align2n < PAGE_SHIFT ? PAGE_SHIFT : mem->align2n;
	int try_cma_first = mem->flags & CODEC_MM_FLAGS_CMA_FIRST;
	int max_retry = ALLOC_MAX_RETRY;
	int have_space;

	int can_from_res = ((mgt->res_pool != NULL) &&	/*have res */
		!(mem->flags & CODEC_MM_FLAGS_CMA)) ||	/*must not CMA */
		((mem->flags & CODEC_MM_FLAGS_RESERVED) ||/*need RESERVED */
		CODEC_MM_FOR_DMA_ONLY(mem->flags) ||	/*NO CPU */
		((mem->flags & CODEC_MM_FLAGS_CPU) &&
			(mgt->res_mem_flags & RES_MEM_FLAGS_HAVE_MAPED)));
	 /*CPU*/ int can_from_cma = ((mgt->total_cma_size > 0) &&/*have cma */
		!(mem->flags & CODEC_MM_FLAGS_RESERVED)) ||
		(mem->flags & CODEC_MM_FLAGS_CMA);	/*can from CMA */
	/*not always reserved. */

	int can_from_sys = (mem->flags & CODEC_MM_FLAGS_DMA_CPU) &&
		(mem->page_count <= mgt->alloc_from_sys_pages_max);

	int can_from_tvp = (mem->flags & CODEC_MM_FLAGS_TVP);

	if (can_from_tvp) {
		can_from_sys = 0;
		can_from_res = 0;
		can_from_cma = 0;
	}

	have_space = codec_mm_alloc_pre_check_in(mgt, mem->buffer_size, 0);
	if (!have_space)
		return -10001;

	can_from_res = can_from_res && (have_space & 1);
	can_from_cma = can_from_cma && (have_space & 2);
	can_from_sys = can_from_sys && (have_space & 4);
	can_from_tvp = can_from_tvp && (have_space & 8);
	if (!can_from_res && !can_from_cma &&
		!can_from_sys && !can_from_tvp) {
		if (debug_mode & 0x10)
			pr_info("error, codec mm have space:%x\n",
				have_space);
		return -10002;
	}

	do {
		if ((mem->flags & CODEC_MM_FLAGS_DMA_CPU) &&
			mem->page_count <= mgt->alloc_from_sys_pages_max &&
			align_2n <= PAGE_SHIFT) {
			mem->mem_handle = (void *)__get_free_pages(GFP_KERNEL,
				get_order(mem->buffer_size));
			mem->from_flags = AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES;
			if (mem->mem_handle) {
				mem->vbuffer = mem->mem_handle;
				mem->phy_addr = virt_to_phys(mem->mem_handle);
				break;
			}
			try_alloced_from_sys = 1;
		}
		/*cma first. */
		if (try_cma_first && can_from_cma) {
			/*
			 *normal cma.
			 */
			mem->mem_handle = dma_alloc_from_contiguous(mgt->dev,
				mem->page_count, align_2n - PAGE_SHIFT);
			mem->from_flags = AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA;
			if (mem->mem_handle) {
				mem->vbuffer = mem->mem_handle;
				mem->phy_addr =
					page_to_phys((struct page *)
					mem->mem_handle);
#ifdef CONFIG_ARM64
				if (mem->flags & CODEC_MM_FLAGS_CMA_CLEAR) {
					/*dma_clear_buffer((struct page *)*/
					/*mem->vbuffer, mem->buffer_size);*/
				}
#endif
				break;
			}
		}
		/*reserved alloc.. */
		if (can_from_res && (align_2n <= RESERVE_MM_ALIGNED_2N)) {
			int aligned_buffer_size = ALIGN(mem->buffer_size,
				(1 << RESERVE_MM_ALIGNED_2N));
			mem->mem_handle =
				(void *)gen_pool_alloc(mgt->res_pool,
				aligned_buffer_size);
			mem->from_flags =
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED;
			if (mem->mem_handle) {
				/*default is no maped */
				mem->vbuffer = NULL;
				mem->phy_addr = (unsigned long)mem->mem_handle;
				mem->buffer_size = aligned_buffer_size;
				break;
			}
			try_alloced_from_reserved = 1;
		}
		/*can_from_res is reserved.. */
		if (can_from_res) {
			if (mgt->cma_res_pool.total_size > 0 &&
				(mgt->cma_res_pool.alloced_size +
					mem->buffer_size) <
				mgt->cma_res_pool.total_size) {
				/*
				 *from cma res first.
				 */
				int aligned_buffer_size =
					ALIGN(mem->buffer_size,
					(1 << RESERVE_MM_ALIGNED_2N));
				mem->mem_handle =
					(void *)
					codec_mm_extpool_alloc
					(&mgt->cma_res_pool, &mem->from_ext,
					aligned_buffer_size);
				mem->from_flags =
					AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES;
				if (mem->mem_handle) {
					/*no vaddr for TVP MEMORY */
					mem->vbuffer = NULL;
					mem->phy_addr =
						(unsigned long)mem->mem_handle;
					mem->buffer_size = aligned_buffer_size;
					break;
				}
			}

		}
		if (can_from_cma) {
			/*
			 *normal cma.
			 */
			mem->mem_handle = dma_alloc_from_contiguous(mgt->dev,
				mem->page_count, align_2n - PAGE_SHIFT);
			mem->from_flags = AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA;
			if (mem->mem_handle) {
				mem->vbuffer = mem->mem_handle;
				mem->phy_addr =
					page_to_phys((struct page *)
					mem->mem_handle);
#ifdef CONFIG_ARM64
				if (mem->flags & CODEC_MM_FLAGS_CMA_CLEAR) {
					/*dma_clear_buffer((struct page *)*/
					/*mem->vbuffer, mem->buffer_size);*/
				}
#endif
				break;
			}
		}
		if (can_from_tvp && align_2n <= RESERVE_MM_ALIGNED_2N) {
			/* 64k,aligend */
			int aligned_buffer_size = ALIGN(mem->buffer_size,
				(1 << RESERVE_MM_ALIGNED_2N));
			mem->mem_handle =
				(void *)codec_mm_extpool_alloc(&mgt->tvp_pool,
				&mem->from_ext, aligned_buffer_size);
			mem->from_flags = AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP;
			if (mem->mem_handle) {
				/*no vaddr for TVP MEMORY */
				mem->vbuffer = NULL;
				mem->phy_addr = (unsigned long)mem->mem_handle;
				mem->buffer_size = aligned_buffer_size;
				break;
			}
		}

		if ((mem->flags & CODEC_MM_FLAGS_DMA_CPU) &&
			mgt->enable_kmalloc_on_nomem && !try_alloced_from_sys) {
			mem->mem_handle = (void *)__get_free_pages(GFP_KERNEL,
				get_order(mem->buffer_size));
			mem->from_flags = AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES;
			if (mem->mem_handle) {
				mem->vbuffer = mem->mem_handle;
				mem->phy_addr =
					virt_to_phys((void *)mem->mem_handle);
				break;
			}
		}
	} while (--max_retry > 0);
	if (mem->mem_handle)
		return 0;
	else
		return -10003;
}

static void codec_mm_free_in(struct codec_mm_mgt_s *mgt, struct codec_mm_s *mem)
{
	if (!(mem->flags & CODEC_MM_FLAGS_FOR_LOCAL_MGR))
		mgt->total_alloced_size -= mem->buffer_size;
	if (mem->flags & CODEC_MM_FLAGS_FOR_SCATTER) {
		mgt->alloced_for_sc_size -= mem->buffer_size;
		mgt->alloced_for_sc_cnt--;
	}

	if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA) {
		dma_release_from_contiguous(mgt->dev,
			mem->mem_handle, mem->page_count);
		mgt->alloced_cma_size -= mem->buffer_size;
	} else if (mem->from_flags ==
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED) {
		gen_pool_free(mgt->res_pool,
			(unsigned long)mem->mem_handle, mem->buffer_size);
		mgt->alloced_res_size -= mem->buffer_size;
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP) {
		codec_mm_extpool_free((struct gen_pool *)mem->from_ext,
			mem->mem_handle, mem->buffer_size);
		mgt->tvp_pool.alloced_size -= mem->buffer_size;
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES) {
		free_pages((unsigned long)mem->mem_handle,
			get_order(mem->buffer_size));
		mgt->alloced_sys_size -= mem->buffer_size;
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES) {
		codec_mm_extpool_free((struct gen_pool *)mem->from_ext,
			mem->mem_handle, mem->buffer_size);
		mgt->cma_res_pool.alloced_size -= mem->buffer_size;
	}

}

struct codec_mm_s *codec_mm_alloc(const char *owner, int size,
	int align2n, int memflags)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = kmalloc(sizeof(struct codec_mm_s),
		GFP_KERNEL);
	int count;
	int ret;
	unsigned long flags;

	if (!mem) {
		pr_err("not enough mem for struct codec_mm_s\n");
		return NULL;
	}

	if ((mgt->tvp_enable & 3) && (memflags & CODEC_MM_FLAGS_FOR_VDECODER)) {
		/*
		 *if tvp & video decoder used tvp memory.
		 *Audio don't protect for default now.
		 */
		memflags = memflags & (~CODEC_MM_FLAGS_FROM_MASK);
		memflags |= CODEC_MM_FLAGS_TVP;
	}
	if ((memflags & CODEC_MM_FLAGS_FROM_MASK) == 0)
		memflags |= CODEC_MM_FLAGS_DMA;

	memset(mem, 0, sizeof(struct codec_mm_s));
	mem->buffer_size = PAGE_ALIGN(size);
	count = mem->buffer_size / PAGE_SIZE;
	mem->page_count = count;
	mem->align2n = align2n;
	mem->flags = memflags;
	ret = codec_mm_alloc_in(mgt, mem);

	/*have used for scatter. */
	if (ret == -10003 && mgt->alloced_for_sc_cnt > 0 &&
		!(memflags & CODEC_MM_FLAGS_FOR_SCATTER)) {
		/*if not scatter, free scatter caches. */
		pr_err(" No mem ret=%d, clear scatter cache!!\n", ret);
		codec_mm_scatter_free_all_ignorecache();
		ret = codec_mm_alloc_in(mgt, mem);
	}
	if (ret < 0) {
		pr_err("not enough mem for %s size %d, ret=%d\n",
			owner, size, ret);
		kfree(mem);
		if (debug_mode & 0x10)
			dump_mem_infos(NULL, 0);
		return NULL;
	}

	atomic_set(&mem->use_cnt, 1);
	mem->owner[0] = owner;
	spin_lock_init(&mem->lock);
	spin_lock_irqsave(&mgt->lock, flags);
	list_add_tail(&mem->list, &mgt->mem_list);
	switch (mem->from_flags) {
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES:
		mgt->alloced_sys_size += mem->buffer_size;
		break;
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA:
		mgt->alloced_cma_size += mem->buffer_size;
		break;
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP:
		mgt->tvp_pool.alloced_size += mem->buffer_size;
		break;
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED:
		mgt->alloced_res_size += mem->buffer_size;
		break;
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES:
		mgt->cma_res_pool.alloced_size += mem->buffer_size;
		break;
	default:
		pr_err("error alloc flags %d\n", mem->from_flags);
	}
	if (!(mem->flags & CODEC_MM_FLAGS_FOR_LOCAL_MGR)) {
		mgt->total_alloced_size += mem->buffer_size;
		if (mgt->total_alloced_size > mgt->max_used_mem_size)
			mgt->max_used_mem_size = mgt->total_alloced_size;
	}
	if ((mem->flags & CODEC_MM_FLAGS_FOR_SCATTER)) {
		mgt->alloced_for_sc_size += mem->buffer_size;
		mgt->alloced_for_sc_cnt++;
	}
	spin_unlock_irqrestore(&mgt->lock, flags);
	if (debug_mode & 0x20)
		pr_err("%s alloc mem size %d at %lx from %d\n",
			owner, size, mem->phy_addr, mem->from_flags);
	return mem;
}
EXPORT_SYMBOL(codec_mm_alloc);

void codec_mm_release(struct codec_mm_s *mem, const char *owner)
{

	int index;
	unsigned long flags;
	int i;
	const char *max_owner;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (!mem)
		return;

	spin_lock_irqsave(&mem->lock, flags);
	index = atomic_dec_return(&mem->use_cnt);
	max_owner = mem->owner[index];
	for (i = 0; i < index; i++) {
		if (mem->owner[i] && strcmp(owner, mem->owner[i]) == 0)
			mem->owner[i] = max_owner;
	}
	if (debug_mode & 0x20)
		pr_err("%s free mem size %d at %lx from %d,index =%d\n",
			owner, mem->buffer_size, mem->phy_addr,
			mem->from_flags, index);
	mem->owner[index] = NULL;
	if (index == 0) {
		spin_unlock_irqrestore(&mem->lock, flags);
		codec_mm_free_in(mgt, mem);
		list_del(&mem->list);
		kfree(mem);
		return;
	}
	spin_unlock_irqrestore(&mem->lock, flags);
}
EXPORT_SYMBOL(codec_mm_release);

void *codec_mm_dma_alloc_coherent(const char *owner, int size,
	dma_addr_t *dma_handle, gfp_t flag, int memflags)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	void *addr = NULL;

	addr = dma_alloc_coherent(mgt->dev, size, dma_handle, flag);
	return addr;
}
EXPORT_SYMBOL(codec_mm_dma_alloc_coherent);

void codec_mm_dma_free_coherent(const char *owner, int size,
	void *cpu_addr, dma_addr_t dma_handle, int memflags)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	dma_free_coherent(mgt->dev, size, cpu_addr, dma_handle);
}
EXPORT_SYMBOL(codec_mm_dma_free_coherent);

void codec_mm_dma_flush(void *vaddr, int size, enum dma_data_direction dir)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	dma_addr_t dma_addr;

	dma_addr = dma_map_single(mgt->dev, vaddr, size, dir);
	if (dma_addr)
		dma_unmap_single(mgt->dev, dma_addr, size, dir);
}
EXPORT_SYMBOL(codec_mm_dma_flush);

int codec_mm_request_shared_mem(struct codec_mm_s *mem, const char *owner)
{
	if (!mem || atomic_read(&mem->use_cnt) > 7)
		return -1;
	mem->owner[atomic_inc_return(&mem->use_cnt) - 1] = owner;
	return 0;
}

static struct codec_mm_s *codec_mm_get_by_val_off(unsigned long val, int off)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem, *want_mem;
	unsigned long flags;

	want_mem = NULL;
	spin_lock_irqsave(&mgt->lock, flags);
	if (!list_empty(&mgt->mem_list)) {
		list_for_each_entry(mem, &mgt->mem_list, list) {
			if (mem && VAL_OFF_VAL(mem, off) == val)
				want_mem = mem;
		}
	}
	spin_unlock_irqrestore(&mgt->lock, flags);
	return want_mem;
}

unsigned long codec_mm_alloc_for_dma(const char *owner, int page_cnt,
	int align2n, int memflags)
{
	struct codec_mm_s *mem;

	mem = codec_mm_alloc(owner, page_cnt << PAGE_SHIFT, align2n, memflags);

	if (!mem)
		return 0;
	return mem->phy_addr;
}
EXPORT_SYMBOL(codec_mm_alloc_for_dma);

int codec_mm_free_for_dma(const char *owner, unsigned long phy_addr)
{
	struct codec_mm_s *mem;

	mem = codec_mm_get_by_val_off(phy_addr, PHY_OFF());

	if (mem)
		codec_mm_release(mem, owner);
	else
		return -1;
	return 0;
}
EXPORT_SYMBOL(codec_mm_free_for_dma);

static int codec_mm_init_tvp_pool(struct extpool_mgt_s *tvp_pool,
	struct codec_mm_s *mm)
{
	struct gen_pool *pool;
	int ret;

	pool = gen_pool_create(RESERVE_MM_ALIGNED_2N, -1);
	if (!pool)
		return -ENOMEM;
	ret = gen_pool_add(pool, mm->phy_addr, mm->buffer_size, -1);
	if (ret < 0) {
		gen_pool_destroy(pool);
		return -1;
	}
	tvp_pool->gen_pool[tvp_pool->slot_num] = pool;
	tvp_pool->mm[tvp_pool->slot_num] = mm;
	return 0;
}

int codec_mm_extpool_pool_alloc(struct extpool_mgt_s *tvp_pool,
	int size, int memflags, int for_tvp)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem;
	int alloced_size = tvp_pool->alloced_size;
	int try_alloced_size = size;
	int ret;

/*alloced from reserved*/
	try_alloced_size = mgt->total_reserved_size - mgt->alloced_res_size;
	if (try_alloced_size > 0 && for_tvp) {
		try_alloced_size = min_t(int,
			size - alloced_size, try_alloced_size);
		mem = codec_mm_alloc(TVP_POOL_NAME,
			try_alloced_size,
			0,
			CODEC_MM_FLAGS_FOR_LOCAL_MGR | CODEC_MM_FLAGS_RESERVED);

		if (mem) {
			ret = codec_mm_init_tvp_pool(tvp_pool, mem);
			if (ret < 0) {
				codec_mm_release(mem, TVP_POOL_NAME);
			} else {
				alloced_size += try_alloced_size;
				tvp_pool->slot_num++;

			}
		}
	}
	if (alloced_size >= size) {
		/*alloc finished. */
		goto alloced_finished;
	}

/*alloced from cma:*/
	try_alloced_size = mgt->total_cma_size - mgt->alloced_cma_size;
	if (try_alloced_size > 0) {
		try_alloced_size = min_t(int, size -
			alloced_size, try_alloced_size);
		mem = codec_mm_alloc(for_tvp ?
			TVP_POOL_NAME :
			CMA_RES_POOL_NAME,
			try_alloced_size,
			0, CODEC_MM_FLAGS_FOR_LOCAL_MGR | CODEC_MM_FLAGS_CMA);

		if (mem) {
			ret = codec_mm_init_tvp_pool(tvp_pool, mem);
			if (ret < 0) {
				codec_mm_release(mem, TVP_POOL_NAME);
			} else {
				alloced_size += try_alloced_size;
				tvp_pool->slot_num++;
			}
		}
	}

alloced_finished:
	if (alloced_size > 0) {
		tvp_pool->total_size = alloced_size;
		return alloced_size;
	}
	return -1;
}
EXPORT_SYMBOL(codec_mm_extpool_pool_alloc);

/*
 *call this before free all
 *alloced TVP memory;
 *it not free,some memory may free ignore.
 *return :
 */
static int codec_mm_extpool_pool_release(struct extpool_mgt_s *tvp_pool)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	int i;
	int ignored = 0;

	for (i = 0; i < tvp_pool->slot_num; i++) {
		struct gen_pool *gpool = tvp_pool->gen_pool[i];
		int slot_mem_size = 0;

		if (gpool) {
			if (gen_pool_avail(gpool) != gen_pool_size(gpool)) {
				pr_err("ERROR: TVP pool is not free.\n");
				ignored++;
				continue;	/*ignore this free now, */
			}
			slot_mem_size = gen_pool_size(gpool);
			gen_pool_destroy(tvp_pool->gen_pool[i]);
			if (tvp_pool->mm[i]) {
				codec_mm_release(tvp_pool->mm[i],
					TVP_POOL_NAME);
			}
		}
		mgt->tvp_pool.total_size -= slot_mem_size;
		tvp_pool->gen_pool[i] = NULL;
		tvp_pool->mm[i] = NULL;
	}
	if (ignored > 0) {
		int before_free_slot = tvp_pool->slot_num + 1;

		for (i = 0; i < tvp_pool->slot_num; i++) {
			if (tvp_pool->gen_pool[i] && before_free_slot < i) {
				tvp_pool->gen_pool[before_free_slot] =
					tvp_pool->gen_pool[i];
				tvp_pool->mm[before_free_slot] =
					tvp_pool->mm[i];
				tvp_pool->gen_pool[i] = NULL;
				tvp_pool->mm[i] = NULL;
				before_free_slot++;
			}
			if (!tvp_pool->gen_pool[i] && before_free_slot > i) {
				before_free_slot = i;
				/**/
			}
		}

	}
	tvp_pool->slot_num = ignored;
	return ignored;
}

/*
 *victor_size
 *=sizeof(res)/sizeof(ulong)
 *res[0]=addr1_start;
 *res[1]=addr1_end;
 *res[2]=addr2_start;
 *res[3]=addr2_end;
 *..
 *res[2*n]=addrx_start;
 *res[2*n+1]=addrx_end;
 *
 *return n;
 */
static int codec_mm_tvp_get_mem_resource(ulong *res, int victor_size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct extpool_mgt_s *tvp_pool = &mgt->tvp_pool;
	int i;

	for (i = 0; i < tvp_pool->slot_num && i < victor_size / 2; i++) {
		if (tvp_pool->mm[i]) {
			res[2 * i] = tvp_pool->mm[i]->phy_addr;
			res[2 * i + 1] = tvp_pool->mm[i]->phy_addr +
				tvp_pool->mm[i]->buffer_size - 1;
		}
	}
	return i;
}

static int codec_mm_is_in_tvp_region(ulong phy_addr)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct extpool_mgt_s *tvp_pool = &mgt->tvp_pool;
	int i;
	int in = 0, in2 = 0;

	for (i = 0; i < tvp_pool->slot_num; i++) {
		if (tvp_pool->mm[i]) {
			in = tvp_pool->mm[i]->phy_addr <= phy_addr;
			in2 = (tvp_pool->mm[i]->phy_addr +
				tvp_pool->mm[i]->buffer_size - 1) >= phy_addr;
			in = in && in2;
			if (in)
				break;
			in = 0;
		}
	}
	return in;
}

void *codec_mm_phys_to_virt(unsigned long phy_addr)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (codec_mm_is_in_tvp_region(phy_addr))
		return NULL;	/* no virt for tvp memory; */

	if (phy_addr >= mgt->rmem.base &&
		phy_addr < mgt->rmem.base + mgt->rmem.size) {
		if (mgt->res_mem_flags & RES_MEM_FLAGS_HAVE_MAPED)
			return phys_to_virt(phy_addr);
		return NULL;	/* no virt for reserved memory; */
	}

	return phys_to_virt(phy_addr);
}
EXPORT_SYMBOL(codec_mm_phys_to_virt);

unsigned long codec_mm_virt_to_phys(void *vaddr)
{
	return page_to_phys((struct page *)vaddr);
}
EXPORT_SYMBOL(codec_mm_virt_to_phys);

static unsigned long dma_get_cma_size_int_byte(struct device *dev)
{
	unsigned long size = 0;
	struct cma *cma = NULL;

	if (!dev) {
		pr_err("CMA: NULL DEV\n");
		return 0;
	}

	cma = dev_get_cma_area(dev);
	if (!cma) {
		pr_err("CMA:  NO CMA region\n");
		return 0;
	}
	size = cma_get_size(cma);

	return size;
}

static int dump_mem_infos(void *buf, int size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem;
	unsigned long flags;

	char *pbuf = buf;
	char sbuf[512];
	int tsize = 0;
	int s;

	if (!pbuf)
		pbuf = sbuf;
	s = sprintf(pbuf, "codec mem info:\n\ttotal codec mem size:%d MB\n",
		mgt->total_codec_mem_size / SZ_1M);
	tsize += s;
	pbuf += s;

	s = sprintf(pbuf, "\talloced size= %d MB\n\tmax alloced: %d MB\n",
		mgt->total_alloced_size / SZ_1M,
		mgt->max_used_mem_size / SZ_1M);
	tsize += s;
	pbuf += s;

	s = sprintf(pbuf, "\tCMA:%d,RES:%d,TVP:%d,SYS:%d MB\n",
		mgt->alloced_cma_size / SZ_1M,
		mgt->alloced_res_size / SZ_1M,
		mgt->tvp_pool.alloced_size / SZ_1M,
		mgt->alloced_sys_size / SZ_1M);
	tsize += s;
	pbuf += s;

	if (mgt->res_pool) {
		s = sprintf(pbuf,
			"\t[%d]RES size:%d MB,alloced:%d MB free:%d MB\n",
			AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED,
			(int)(gen_pool_size(mgt->res_pool) / SZ_1M),
			(int)(mgt->alloced_res_size / SZ_1M),
			(int)(gen_pool_avail(mgt->res_pool) / SZ_1M));
		tsize += s;
		pbuf += s;
	}

	s = sprintf(pbuf, "\t[%d]CMA size:%d MB:alloced: %d MB,free:%d MB\n",
		AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA,
		(int)(dma_get_cma_size_int_byte(mgt->dev) / SZ_1M),
		(int)(mgt->alloced_cma_size / SZ_1M),
		(int)((dma_get_cma_size_int_byte(mgt->dev) -
				mgt->alloced_cma_size) / SZ_1M));
	tsize += s;
	pbuf += s;

	if (mgt->tvp_pool.slot_num > 0) {
		s = sprintf(pbuf,
			"\t[%d]TVP size:%d MB,alloced:%d MB free:%d MB\n",
			AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP,
			(int)(mgt->tvp_pool.total_size / SZ_1M),
			(int)(mgt->tvp_pool.alloced_size / SZ_1M),
			(int)((mgt->tvp_pool.total_size -
					mgt->tvp_pool.alloced_size) / SZ_1M));
		tsize += s;
		pbuf += s;
	}
	if (mgt->cma_res_pool.slot_num > 0) {
		s = sprintf(pbuf,
			"\t[%d]CMA_RES size:%d MB,alloced:%d MB free:%d MB\n",
			AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES,
			(int)(mgt->cma_res_pool.total_size / SZ_1M),
			(int)(mgt->cma_res_pool.alloced_size / SZ_1M),
			(int)((mgt->cma_res_pool.total_size -
					mgt->cma_res_pool.alloced_size) /
				SZ_1M));
		tsize += s;
		pbuf += s;
	}

	if (!buf && tsize > 0) {
		pr_info("%s", sbuf);
		pbuf = sbuf;
		sbuf[0] = '\0';
		tsize = 0;
	}
	spin_lock_irqsave(&mgt->lock, flags);
	if (list_empty(&mgt->mem_list)) {
		spin_unlock_irqrestore(&mgt->lock, flags);
		return tsize;
	}
	list_for_each_entry(mem, &mgt->mem_list, list) {
		s = sprintf(pbuf,
			"\towner: %s:%s,addr=%p,s=%d,from=%d,cnt=%d\n",
			mem->owner[0] ? mem->owner[0] : "no",
			mem->owner[1] ? mem->owner[1] : "no",
			(void *)mem->phy_addr,
			mem->buffer_size, mem->from_flags,
			atomic_read(&mem->use_cnt));

		if (buf) {
			pbuf += s;
			if (tsize + s > size - 128)
				break;	/*no memory for dump now. */
		} else {
			pr_info("%s", sbuf);
		}
		tsize += s;
	}
	spin_unlock_irqrestore(&mgt->lock, flags);

	return tsize;
}

int codec_mm_video_tvp_enabled(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->tvp_enable;
}
EXPORT_SYMBOL(codec_mm_video_tvp_enabled);

int codec_mm_get_total_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	int total_size = mgt->total_codec_mem_size;

	if ((debug_mode & 0xf) == 0) {	/*no debug memory mode. */
		return total_size;
	}
	/*
	 *disable reserved:1
	 *disable cma:2
	 *disable sys memory:4
	 *disable half memory:8,
	 */
	if (debug_mode & 0x8)
		total_size -= mgt->total_codec_mem_size / 2;
	if (debug_mode & 0x1) {
		total_size -= mgt->total_reserved_size;
		total_size -= mgt->cma_res_pool.total_size;
	}
	if (debug_mode & 0x2)
		total_size -= mgt->total_cma_size;
	if (total_size < 0)
		total_size = 0;
	return total_size;
}
EXPORT_SYMBOL(codec_mm_get_total_size);

int codec_mm_get_free_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return codec_mm_get_total_size() - mgt->total_alloced_size;
}
EXPORT_SYMBOL(codec_mm_get_free_size);

int codec_mm_get_reserved_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->total_reserved_size;
}
EXPORT_SYMBOL(codec_mm_get_reserved_size);

/*
 *with_wait:
 *1: if no mem, do wait and free some cache.
 *0: do not wait.
 */
int codec_mm_enough_for_size(int size, int with_wait)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	int have_mem = codec_mm_alloc_pre_check_in(mgt, size, 0);

	if (!have_mem && with_wait && mgt->alloced_for_sc_cnt > 0) {
		pr_err(" No mem, clear scatter cache!!\n");
		codec_mm_scatter_free_all_ignorecache();
		have_mem = codec_mm_alloc_pre_check_in(mgt, size, 0);
		if (have_mem)
			return 1;
		if (debug_mode & 0x20)
			dump_mem_infos(NULL, 0);
		return 0;
	}
	return 1;
}
EXPORT_SYMBOL(codec_mm_enough_for_size);

int codec_mm_mgt_init(struct device *dev)
{

	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	INIT_LIST_HEAD(&mgt->mem_list);
	mgt->dev = dev;
	mgt->alloc_from_sys_pages_max = 4;
	if (mgt->rmem.size > 0) {
		unsigned long aligned_addr;
		int aligned_size;
		/*mem aligned, */
		mgt->res_pool = gen_pool_create(RESERVE_MM_ALIGNED_2N, -1);
		if (!mgt->res_pool)
			return -ENOMEM;
		aligned_addr = ((unsigned long)mgt->rmem.base +
			((1 << RESERVE_MM_ALIGNED_2N) - 1)) &
			(~((1 << RESERVE_MM_ALIGNED_2N) - 1));
		aligned_size = mgt->rmem.size -
			(int)(aligned_addr - (unsigned long)mgt->rmem.base);
		gen_pool_add(mgt->res_pool, aligned_addr, aligned_size, -1);
		pr_debug("add reserve memory %p(aligned %p) size=%x(aligned %x)\n",
			(void *)mgt->rmem.base, (void *)aligned_addr,
			(int)mgt->rmem.size, (int)aligned_size);
		mgt->total_reserved_size = aligned_size;
		mgt->total_codec_mem_size = aligned_size;
#ifdef RES_IS_MAPED
		mgt->res_mem_flags |= RES_MEM_FLAGS_HAVE_MAPED;
#endif
	}
	mgt->total_cma_size = dma_get_cma_size_int_byte(mgt->dev);
	mgt->total_codec_mem_size += mgt->total_cma_size;
	mgt->tvp_pool.default_size = mgt->total_reserved_size + SZ_1M * 32;
	/*2M for audio not protect. */
	mgt->tvp_pool.default_4k_size = mgt->total_codec_mem_size - SZ_1M * 2;

	mgt->cma_res_pool.default_size = mgt->total_cma_size;
	mgt->cma_res_pool.default_4k_size = mgt->total_cma_size;
	spin_lock_init(&mgt->lock);
	return 0;
}
EXPORT_SYMBOL(codec_mm_mgt_init);

static int __init amstream_test_init(void)
{
#if 0
	unsigned long buf[4];
	struct codec_mm_s *mem[20];

	mem[0] = codec_mm_alloc("test0", 1024 * 1024, 0, 0);
	mem[1] = codec_mm_alloc("test1", 1024 * 1024, 0, 0);
	mem[2] = codec_mm_alloc("test2", 1024 * 1024, 0, 0);
	mem[3] = codec_mm_alloc("test32", 1024 * 1024, 0, 0);
	mem[4] = codec_mm_alloc("test3", 400 * 1024, 0, 0);
	mem[5] = codec_mm_alloc("test3", 400 * 1024, 0, 0);
	mem[6] = codec_mm_alloc("test666", 400 * 1024, 0, 0);
	mem[7] = codec_mm_alloc("test3", 8 * 1024 * 1024, 0, 0);
	mem[8] = codec_mm_alloc("test4", 3 * 1024 * 1024, 0, 0);
	pr_info("TT:%p,%p,%p,%p\n", mem[0], mem[1], mem[2], mem[3]);
	dump_mem_infos(NULL, 0);
	codec_mm_release(mem[1], "test1");
	codec_mm_release(mem[7], "test3");
	codec_mm_request_shared_mem(mem[3], "test55");
	codec_mm_request_shared_mem(mem[6], "test667");
	dump_mem_infos(NULL, 0);
	codec_mm_release(mem[3], "test32");
	codec_mm_release(mem[0], "test1");
	codec_mm_request_shared_mem(mem[3], "test57");
	codec_mm_request_shared_mem(mem[3], "test58");
	codec_mm_release(mem[3], "test55");
	dump_mem_infos(NULL, 0);
	mem[8] = codec_mm_alloc("test4", 8 * 1024 * 1024, 0,
		CODEC_MM_FLAGS_DMA);
	mem[9] = codec_mm_alloc("test5", 4 * 1024 * 1024, 0,
		CODEC_MM_FLAGS_TVP);
	mem[10] = codec_mm_alloc("test6", 10 * 1024 * 1024, 0,
		CODEC_MM_FLAGS_DMA_CPU);
	dump_mem_infos(NULL, 0);

	buf[0] = codec_mm_alloc_for_dma("streambuf1",
		8 * 1024 * 1024 / PAGE_SIZE, 0, 0);
	buf[1] = codec_mm_alloc_for_dma("streambuf2",
		2 * 1024 * 1024 / PAGE_SIZE, 0, 0);
	buf[2] = codec_mm_alloc_for_dma("streambuf2",
		118 * 1024 / PAGE_SIZE, 0, 0);
	buf[3] = codec_mm_alloc_for_dma("streambuf2",
		(jiffies & 0x7f) * 1024 / PAGE_SIZE, 0, 0);
	dump_mem_infos(NULL, 0);
	codec_mm_free_for_dma("streambuf2", buf[3]);
	codec_mm_free_for_dma("streambuf2", buf[1]);
	codec_mm_free_for_dma("streambuf2", buf[2]);
	codec_mm_free_for_dma("streambuf1", buf[0]);
	dump_mem_infos(NULL, 0);
#endif

	return 0;
}

static ssize_t codec_mm_dump_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	size_t ret;

	ret = dump_mem_infos(buf, PAGE_SIZE);
	return ret;
}

static ssize_t codec_mm_scatter_dump_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	size_t ret;

	ret = codec_mm_scatter_info_dump(buf, PAGE_SIZE);
	return ret;
}

static ssize_t codec_mm_keeper_dump_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	size_t ret;

	ret = codec_mm_keeper_dump_info(buf, PAGE_SIZE);
	return ret;
}

static ssize_t tvp_enable_help_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t size = 0;

	size += sprintf(buf, "tvp enable help:\n");
	size += sprintf(buf + size, "echo n > tvp_enable\n");
	size += sprintf(buf + size, "0: disable tvp(tvp size to 0)\n");
	size += sprintf(buf + size,
		"1: enable tvp for 1080p playing(use default size)\n");
	size += sprintf(buf + size,
		"2: enable tvp for 4k playing(use default 4k size)\n");
	return size;
}

static ssize_t tvp_enable_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned int val;
	ssize_t ret;

	val = -1;
	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	/*
	 *always free all scatter cache for
	 *tvp changes.
	 */
	codec_mm_scatter_free_all_ignorecache();
	switch (val) {
	case 0:
		ret = codec_mm_extpool_pool_release(&mgt->tvp_pool);
		mgt->tvp_enable = 0;
		pr_info("disalbe tvp\n");
		break;
	case 1:
		codec_mm_extpool_pool_alloc(&mgt->tvp_pool,
			mgt->tvp_pool.default_size, 0, 1);
		mgt->tvp_enable = 1;
		pr_info("enable tvp for 1080p\n");
		break;
	case 2:
		codec_mm_extpool_pool_alloc(&mgt->tvp_pool,
			mgt->tvp_pool.default_4k_size, 0, 1);
		mgt->tvp_enable = 2;
		pr_info("enable tvp for 4k\n");
		break;
	default:
		pr_err("unknown cmd! %d\n", val);
	}
	return size;
}

static ssize_t fastplay_enable_help_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t size = 0;

	size += sprintf(buf, "fastplay enable help:\n");
	size += sprintf(buf + size, "echo n > tvp_enable\n");
	size += sprintf(buf + size, "0: disable fastplay\n");
	size += sprintf(buf + size,
		"1: enable fastplay for 1080p playing(use default size)\n");
	return size;
}

static ssize_t fastplay_enable_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned int val;
	ssize_t ret;

	val = -1;
	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	switch (val) {
	case 0:
		ret = codec_mm_extpool_pool_release(&mgt->cma_res_pool);
		mgt->fastplay_enable = 0;
		pr_err("disalbe fastplay\n");
		break;
	case 1:
		codec_mm_extpool_pool_alloc(&mgt->cma_res_pool,
			mgt->cma_res_pool.default_4k_size, 0, 0);
		mgt->fastplay_enable = 1;
		pr_err("enable fastplay\n");
		break;
	default:
		pr_err("unknown cmd! %d\n", val);
	}
	return size;
}

static ssize_t codec_mm_config_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	size_t ret;

	ret = sprintf(buf,
		"default_tvp_size:0x%x(%d MB)\n",
		mgt->tvp_pool.default_size, mgt->tvp_pool.default_size / SZ_1M);
	ret += sprintf(buf + ret,
		"default_tvp_4k_size:0x%x(%d MB)\n",
		mgt->tvp_pool.default_4k_size,
		mgt->tvp_pool.default_4k_size / SZ_1M);
	ret += codec_mm_scatter_mgt_get_config(buf + ret);
	return ret;
}

static ssize_t codec_mm_config_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned int val;
	ssize_t ret;

	ret = sscanf(buf, "default_tvp_size:0x%x", &val);
	if (ret == 1) {
		mgt->tvp_pool.default_size = val;
		return size;
	}
	ret = sscanf(buf, "default_tvp_4k_size:0x%x", &val);
	if (ret == 1) {
		mgt->tvp_pool.default_4k_size = val;
		return size;
	}
	return codec_mm_scatter_mgt_set_config(buf, size);
}

static ssize_t tvp_region_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	size_t ret;
	ulong res_victor[16];
	int i;
	int off = 0;

	ret = codec_mm_tvp_get_mem_resource(res_victor, 8);
	for (i = 0; i < ret; i++) {
		off += sprintf(buf + off,
			"segment%d:0x%p - 0x%p (size:0x%x)\n",
			i,
			(void *)res_victor[2 * i],
			(void *)res_victor[2 * i + 1],
			(int)(res_victor[2 * i + 1] - res_victor[2 * i] + 1));
	}
	return off;
}

static ssize_t codec_mm_debug_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t size = 0;

	size += sprintf(buf, "mm_scatter help:\n");
	size += sprintf(buf + size, "echo n > mm_scatter_debug\n");
	size += sprintf(buf + size, "n==0: clear all debugs)\n");
	size += sprintf(buf + size, "n=1: dump all alloced scatters\n");
	size += sprintf(buf + size, "n=2: dump all slots\n");

	size += sprintf(buf + size, "n=3: dump all free slots\n");

	size += sprintf(buf + size, "n=4: dump all sid hash table\n");

	size += sprintf(buf + size, "n=5: free all free slot now!\n");

	size += sprintf(buf + size, "n=10: force free all keeper\n");

	size += sprintf(buf + size,
		"n==100: cmd mode p1 p ##mode:0,dump, 1,alloc 2,more,3,free some,4,free all\n");
	return size;
}

static ssize_t codec_mm_debug_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	unsigned int val;
	ssize_t ret;

	val = -1;
	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	switch (val) {
	case 0:
		pr_info("clear debug!\n");
		break;
	case 1:
		codec_mm_dump_all_scatters();
		break;
	case 2:
		codec_mm_dump_all_slots();
		break;
	case 3:
		codec_mm_dump_free_slots();
		break;
	case 4:
		codec_mm_dump_all_hash_table();
		break;
	case 5:
		codec_mm_free_all_free_slots();
		break;
	case 10:
		codec_mm_keeper_free_all_keep(1);
		break;
	case 100:{
			int cmd, mode, p1, p2;

			mode = p1 = p2 = 0;
			ret = sscanf(buf, "%d %d %d %d", &cmd, &mode, &p1, &p2);
			codec_mm_scatter_test(mode, p1, p2);
		}
		break;
	default:
		pr_err("unknown cmd! %d\n", val);
	}
	return size;

}

static struct class_attribute codec_mm_class_attrs[] = {
	__ATTR_RO(codec_mm_dump),
	__ATTR_RO(codec_mm_scatter_dump),
	__ATTR_RO(codec_mm_keeper_dump),
	__ATTR_RO(tvp_region),
	__ATTR(tvp_enable, 0664,
		tvp_enable_help_show, tvp_enable_store),
	__ATTR(fastplay_enable, 0664,
		fastplay_enable_help_show, fastplay_enable_store),
	__ATTR(config, 0664,
		codec_mm_config_show, codec_mm_config_store),
	__ATTR(debug, 0664,
		codec_mm_debug_show, codec_mm_debug_store),
	__ATTR_NULL
};

static struct class codec_mm_class = {
	.name = "codec_mm",
	.class_attrs = codec_mm_class_attrs,
};

static int codec_mm_probe(struct platform_device *pdev)
{

	int r;

	pdev->dev.platform_data = get_mem_mgt();
	r = of_reserved_mem_device_init(&pdev->dev);
	if (r == 0)
		pr_debug("codec_mm_probe mem init done\n");

	codec_mm_mgt_init(&pdev->dev);
	r = class_register(&codec_mm_class);
	if (r) {
		pr_err("vdec class create fail.\n");
		return r;
	}
	r = of_reserved_mem_device_init(&pdev->dev);
	if (r == 0)
		pr_debug("codec_mm reserved memory probed done\n");

	pr_debug("codec_mm_probe ok\n");

	codec_mm_scatter_mgt_init();
	codec_mm_keeper_mgr_init();
	amstream_test_init();
	codec_mm_scatter_mgt_test();
	return 0;
}

static const struct of_device_id amlogic_mem_dt_match[] = {
	{
			.compatible = "amlogic, codec, mm",
		},
	{},
};

static struct platform_driver codec_mm_driver = {
	.probe = codec_mm_probe,
	.remove = NULL,
	.driver = {
			.owner = THIS_MODULE,
			.name = "codec_mm",
			.of_match_table = amlogic_mem_dt_match,

		}
};

static int __init codec_mm_module_init(void)
{

	pr_err("codec_mm_module_init\n");

	if (platform_driver_register(&codec_mm_driver)) {
		pr_err("failed to register amports mem module\n");
		return -ENODEV;

	}

	return 0;
}

arch_initcall(codec_mm_module_init);
MODULE_DESCRIPTION("AMLOGIC amports mem  driver");
MODULE_LICENSE("GPL");

#if 0
static int __init codec_mm_cma_setup(struct reserved_mem *rmem)
{
	int ret;
	phys_addr_t base, size, align;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	align = (phys_addr_t) PAGE_SIZE << max(MAX_ORDER - 1, pageblock_order);
	base = ALIGN(rmem->base, align);
	size = round_down(rmem->size - (base - rmem->base), align);
	ret = cma_init_reserved_mem(base, size, 0, &mgt->cma);
	if (ret) {
		pr_info("TT:vdec: cma init reserve area failed.\n");
		mgt->cma = NULL;
		return ret;
	}
#ifndef CONFIG_ARM64
	dma_contiguous_early_fixup(base, size);
#endif
	pr_info("TT:vdec: cma setup\n");
	return 0;
}

RESERVEDMEM_OF_DECLARE(codec_mm_cma, "amlogic, codec-mm-cma",
	codec_mm_cma_setup);

#endif
static int codec_mm_reserved_init(struct reserved_mem *rmem, struct device *dev)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	mgt->rmem = *rmem;
	pr_debug("codec_mm_reserved_init %p->%p\n",
		(void *)mgt->rmem.base,
		(void *)mgt->rmem.base + mgt->rmem.size);
	return 0;
}

static const struct reserved_mem_ops codec_mm_rmem_vdec_ops = {
	.device_init = codec_mm_reserved_init,
};

static int __init codec_mm_res_setup(struct reserved_mem *rmem)
{
	rmem->ops = &codec_mm_rmem_vdec_ops;
	pr_debug("vdec: reserved mem setup\n");

	return 0;
}

RESERVEDMEM_OF_DECLARE(codec_mm_reserved, "amlogic, codec-mm-reserved",
	codec_mm_res_setup);

module_param(debug_mode, uint, 0664);
MODULE_PARM_DESC(debug_mode, "\n debug module\n");
module_param(debug_sc_mode, uint, 0664);
MODULE_PARM_DESC(debug_sc_mode, "\n debug scatter module\n");
module_param(debug_keep_mode, uint, 0664);
MODULE_PARM_DESC(debug_keep_mode, "\n debug keep module\n");
