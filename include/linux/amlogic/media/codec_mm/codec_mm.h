/*
 * include/linux/amlogic/media/codec_mm/codec_mm.h
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

#ifndef CODEC_MM_API_HEADER
#define CODEC_MM_API_HEADER
#include <linux/dma-direction.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/list.h>

/*
*memflags
*/
#define CODEC_MM_FLAGS_DMA 1
#define CODEC_MM_FLAGS_CPU 2
#define CODEC_MM_FLAGS_TVP 4

#define CODEC_MM_FLAGS_RESERVED 0x100
#define CODEC_MM_FLAGS_CMA		0x200

/*
*alloc from cma first,
*cma->then ..reserved.
*for less memory fragment;
*/
#define CODEC_MM_FLAGS_CMA_FIRST 0x400

/*
*flags can used for DRM:
*/
#define CODEC_MM_FLAGS_FOR_VDECODER 0x1000
#define CODEC_MM_FLAGS_FOR_ADECODER 0x2000
#define CODEC_MM_FLAGS_FOR_ENCODER  0x4000

/*
*if cma,
*clear thie buffer cache.
*/
#define CODEC_MM_FLAGS_CMA_CLEAR 0x10000

/*
*used in codec_mm owner.
*don't not set on others.
*/
#define CODEC_MM_FLAGS_FOR_LOCAL_MGR  0x8000000

/*used scatter manager*/
#define CODEC_MM_FLAGS_FOR_SCATTER  0x10000000

#define CODEC_MM_FLAGS_FROM_MASK \
	(CODEC_MM_FLAGS_DMA |\
	CODEC_MM_FLAGS_CPU |\
	CODEC_MM_FLAGS_TVP)

#define CODEC_MM_FLAGS_DMA_CPU  (CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CPU)
#define CODEC_MM_FLAGS_ANY	CODEC_MM_FLAGS_DMA_CPU

/*--------------------------------------------------*/
struct codec_mm_s {
	/*can be shared by many user */
	const char *owner[8];
	int ins_id;/*used by with channel?*/
	int ins_buffer_id;/*canbe for buffer id*/
	/*virtual buffer of this memory */
	char *vbuffer;
	void *mem_handle;	/*used for top level.alloc/free */
	void *from_ext;		/*alloced from pool*/
	ulong phy_addr;		/*if phy continue or one page only */
	int buffer_size;
	int page_count;
	int align2n;
	/*if vbuffer is no cache set*/
	   /*AMPORTS_MEM_FLAGS_NOCACHED  to flags */
#define AMPORTS_MEM_FLAGS_NOCACHED (1<<0)
	/*phy continue,need dma
	 */
#define AMPORTS_MEM_FLAGS_DMA (1<<1)
	int flags;
#define AMPORTS_MEM_FLAGS_FROM_SYS 1
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES 2
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED 3
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA 4
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP 5
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES 6
	int from_flags;
	/*may can be shared on many user..*/
	   /*decoder/di/ppmgr,*/
	atomic_t use_cnt;
	spinlock_t lock;
	char *pagemap;
	int pagemap_size;
	int alloced_page_num;
	u64 alloced_jiffies;
	int mem_id;
	int next_bit;
	struct list_head list;
};
struct codec_mm_s *codec_mm_alloc(const char *owner, int size,
		int align2n, int memflags);
unsigned long codec_mm_alloc_for_dma_ex(
		const char *owner,
		int page_cnt,
		int align2n,
		int memflags,
		int ins_id,
		int buffer_id);

void codec_mm_release(struct codec_mm_s *mem, const char *owner);
int codec_mm_request_shared_mem(struct codec_mm_s *mem, const char *owner);
/*call if not make sure valid data.*/
void codec_mm_release_with_check(struct codec_mm_s *mem, const char *owner);
unsigned long dma_get_cma_size_int_byte(struct device *dev);


/*---------------------------------------------------------------*/

unsigned long codec_mm_alloc_for_dma(const char *owner, int page_cnt,
	int align2n, int memflags);
int codec_mm_free_for_dma(const char *owner, unsigned long phy_addr);

void *codec_mm_phys_to_virt(unsigned long phy_addr);
unsigned long codec_mm_virt_to_phys(void *vaddr);

void codec_mm_dma_flush(void *vaddr,
	int size,
	enum dma_data_direction dir);

int codec_mm_get_total_size(void);
int codec_mm_get_free_size(void);
int codec_mm_get_reserved_size(void);
int codec_mm_enough_for_size(int size, int with_wait);
int codec_mm_video_tvp_enabled(void);
void *codec_mm_dma_alloc_coherent(const char *owner, int size,
			dma_addr_t *dma_handle, gfp_t flag, int memflags);
void codec_mm_dma_free_coherent(const char *owner, int size,
			void *cpu_addr, dma_addr_t dma_handle, int memflags);
#endif
