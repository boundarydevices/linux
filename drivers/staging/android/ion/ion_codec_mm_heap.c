/*
 * drivers/staging/android/ion/ion_codec_mm_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion.h"
#include "ion_priv.h"
#include <linux/amlogic/media/codec_mm/codec_mm.h>

struct ion_codec_mm_heap {
	struct ion_heap heap;
	int max_can_alloc_size;
	int alloced_size;
};

#define CODEC_MM_ION "ION"

ion_phys_addr_t ion_codec_mm_allocate(struct ion_heap *heap,
				      unsigned long size,
				      unsigned long align)
{
	struct ion_codec_mm_heap *codec_heap =
		container_of(heap, struct ion_codec_mm_heap, heap);
	unsigned long offset;

	if (codec_heap->alloced_size + size > codec_heap->max_can_alloc_size) {
		pr_err(
			"ion_codec_mm_allocate failed out size %ld,alloced %d\n",
			size,
			codec_heap->alloced_size);
		return ION_CODEC_MM_ALLOCATE_FAIL;
	}

	offset = codec_mm_alloc_for_dma(
		CODEC_MM_ION,
		size / PAGE_SIZE,
		0,
		CODEC_MM_FLAGS_DMA);

	if (!offset) {
		pr_err("ion_codec_mm_allocate failed out size %d\n", (int)size);
		return ION_CODEC_MM_ALLOCATE_FAIL;
	}
	codec_heap->alloced_size += size;
	return offset;
}

void ion_codec_mm_free(struct ion_heap *heap, ion_phys_addr_t addr,
		       unsigned long size)
{
	struct ion_codec_mm_heap *codec_heap =
		container_of(heap, struct ion_codec_mm_heap, heap);

	if (addr == ION_CODEC_MM_ALLOCATE_FAIL)
		return;
	codec_mm_free_for_dma(CODEC_MM_ION, addr);
	codec_heap->alloced_size -= size;
}

static int ion_codec_mm_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	struct sg_table *table;
	ion_phys_addr_t paddr;
	int ret;

	if (align > PAGE_SIZE)
		return -EINVAL;

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	paddr = ion_codec_mm_allocate(heap, size, align);
	if (paddr == ION_CODEC_MM_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		goto err_free_table;
	}

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->priv_virt = table;
	buffer->sg_table = table;

	return 0;

err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	return ret;
}

static void ion_codec_mm_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	ion_phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	ion_heap_buffer_zero(buffer);

	if (ion_buffer_cached(buffer))
		dma_sync_sg_for_device(
			NULL,
			table->sgl,
			table->nents,
			DMA_BIDIRECTIONAL);

	ion_codec_mm_free(heap, paddr, buffer->size);
	sg_free_table(table);
	kfree(table);
}

static struct ion_heap_ops codec_mm_heap_ops = {
	.allocate = ion_codec_mm_heap_allocate,
	.free = ion_codec_mm_heap_free,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_codec_mm_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_codec_mm_heap *codec_heap;
	/* int ret; */
	codec_heap = kzalloc(sizeof(*codec_heap), GFP_KERNEL);
	if (!codec_heap)
		return ERR_PTR(-ENOMEM);

	codec_heap->max_can_alloc_size = heap_data->size;
	codec_heap->alloced_size = 0;
	codec_heap->heap.ops = &codec_mm_heap_ops;
	codec_heap->heap.type = ION_HEAP_TYPE_CUSTOM;
	codec_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	return &codec_heap->heap;
}

void ion_codec_mm_heap_destroy(struct ion_heap *heap)
{
	struct ion_codec_mm_heap *codec_heap =
	     container_of(heap, struct  ion_codec_mm_heap, heap);

	kfree(codec_heap);
	codec_heap = NULL;
}
