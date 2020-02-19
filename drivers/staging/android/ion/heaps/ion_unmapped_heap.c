/*
 * drivers/staging/android/ion/ion_unmapped_heap.c
 * 
 * Copyright 2021 NXP
 * Copyright (C) 2016-2017 Linaro, Inc.
 * Copyright (C) Allwinner 2014
 * Author: <sunny@allwinnertech.com> for Allwinner.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * ION heap type for handling physical memory heap not mapped
 * in the linux-based OS.
 *
 * "unmapped heap" buffers are default not mapped but buffer owner
 * can explicitly request mapping for some specific purpose.
 *
 * Based on Allwinner work (allocation thru gen_pool) and
 * HiSilicon work (create ION heaps from DT nodes,
 * Author: Chen Feng <puck.chen@hisilicon.com>).
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/ion.h>

#define MAX_UNMAPPED_AREA 2

struct rmem_unmapped {
	phys_addr_t base;
	phys_addr_t size;

	char name[32];
};
static struct rmem_unmapped unmapped_data[MAX_UNMAPPED_AREA] = {0};
uint32_t unmapped_count = 0;

struct ion_unmapped_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	phys_addr_t base;
	size_t          size;
};

struct unmapped_buffer_priv {
	phys_addr_t base;
};

static phys_addr_t get_buffer_base(struct unmapped_buffer_priv *priv)
{
	return priv->base;
}

static phys_addr_t ion_unmapped_allocate(struct ion_heap *heap,
					   unsigned long size,
					   phys_addr_t *addr)
{
	struct ion_unmapped_heap *umh =
		container_of(heap, struct ion_unmapped_heap, heap);
	unsigned long offset = gen_pool_alloc(umh->pool, size);

	if (!offset) {
		return false;
	}

	*addr = offset;
	return true;
}

static void ion_unmapped_free(struct ion_heap *heap, phys_addr_t addr,
			    unsigned long size)
{
	struct ion_unmapped_heap *umh =
		container_of(heap, struct ion_unmapped_heap, heap);

	gen_pool_free(umh->pool, addr, size);
}

static struct sg_table *ion_unmapped_heap_map_dma(struct ion_heap *heap,
						struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}
	sg_set_page(table->sgl,
		    phys_to_page(get_buffer_base(buffer->priv_virt)),
		    buffer->size, 0);

	return table;
}

void ion_unmapped_heap_unmap_dma(struct ion_heap *heap,
				struct ion_buffer *buffer)
{
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
}

static int ion_unmapped_heap_allocate(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    unsigned long size,
				    unsigned long flags)
{
	struct unmapped_buffer_priv *priv;
	phys_addr_t base;
	int rc = -EINVAL;

	if (!ion_unmapped_allocate(heap, size, &base))
		return -ENOMEM;

	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (IS_ERR_OR_NULL(priv)) {
		rc = -ENOMEM;
		goto err;
	}

	priv->base = base;
	buffer->size = roundup(size, PAGE_SIZE);
	buffer->priv_virt = priv;

	buffer->sg_table = ion_unmapped_heap_map_dma(heap, buffer);
	if (!buffer->sg_table) {
		rc = -ENOMEM;
		goto err;
	}

	sg_dma_address(buffer->sg_table->sgl) = priv->base;
	sg_dma_len(buffer->sg_table->sgl) = size;
	return 0;
err:
	ion_unmapped_free(heap, base, size);
	kfree(priv);
	buffer->priv_virt = NULL;
	return rc;
}

static void ion_unmapped_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;


	ion_unmapped_heap_unmap_dma(heap, buffer);
	ion_unmapped_free(heap, get_buffer_base(buffer->priv_virt),
			 buffer->size);
	kfree(buffer->priv_virt);
	buffer->priv_virt = NULL;
}

static struct ion_heap_ops unmapped_heap_ops = {
	.allocate = ion_unmapped_heap_allocate,
	.free = ion_unmapped_heap_free,
};

struct ion_heap *ion_unmapped_heap_create(struct rmem_unmapped *heap_data)
{
	struct ion_unmapped_heap *unmapped_heap;
	int ret;

	struct page *page;
	size_t size;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

	unmapped_heap = kzalloc(sizeof(*unmapped_heap), GFP_KERNEL);
	if (!unmapped_heap)
		return ERR_PTR(-ENOMEM);

	// ensure memory address align to 64K which can meet VPU requirement.
	unmapped_heap->pool = gen_pool_create(PAGE_SHIFT+4, -1);
	if (!unmapped_heap->pool) {
		kfree(unmapped_heap);
		return ERR_PTR(-ENOMEM);
	}
	unmapped_heap->base = heap_data->base;
	unmapped_heap->size = size;
	gen_pool_add(unmapped_heap->pool, unmapped_heap->base, heap_data->size,
		     -1);
	unmapped_heap->heap.ops = &unmapped_heap_ops;
	unmapped_heap->heap.type = ION_HEAP_TYPE_UNMAPPED;
	unmapped_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	unmapped_heap->heap.name = heap_data->name;

	return &unmapped_heap->heap;
}
static int rmem_unmapped_device_init(struct reserved_mem *rmem,
					 struct device *dev)
{
	dev_set_drvdata(dev, rmem);
	return 0;
}

static void rmem_unmapped_device_release(struct reserved_mem *rmem,
					 struct device *dev)
{
	dev_set_drvdata(dev, NULL);
}


static const struct reserved_mem_ops rmem_dma_ops = {
	.device_init    = rmem_unmapped_device_init,
	.device_release = rmem_unmapped_device_release,
};

static void rmem_unmapped_setup(struct reserved_mem *rmem)
{
    if (rmem == NULL) {
        pr_info("Invalid reserved mem info\n");
        return;
    }
	if (unmapped_count < MAX_UNMAPPED_AREA)
	{
		unmapped_data[unmapped_count].base = rmem->base;
		unmapped_data[unmapped_count].size = rmem->size;
		memcpy(unmapped_data[unmapped_count].name,rmem->name,32);
		rmem->ops = &rmem_dma_ops;
		pr_info("Reserved memory: ION unmapped pool %s at %pa, size %ld MiB\n",
				rmem->name,&rmem->base, (unsigned long)rmem->size / SZ_1M);

		unmapped_count++;
	}
}

static int ion_add_unmapped_heap(void)
{
	struct ion_heap *heap;
	uint32_t i;

    /*
     * To support GKI, move this to module.
     * Then we have to check the devicetree by driver itself.
     */
    struct device_node np;
    np.full_name = "unmapped";
    np.name = "unmapped";
    struct reserved_mem *rmem = of_reserved_mem_lookup(&np);
    rmem_unmapped_setup(rmem);

	for (i=0;i<unmapped_count;i++)
	{
		if (unmapped_data[i].base == 0 || unmapped_data[i].size == 0)
			return -EINVAL;

		heap = ion_unmapped_heap_create(&unmapped_data[i]);
		if (IS_ERR(heap))
			return PTR_ERR(heap);

		ion_device_add_heap(heap);
	}
	return 0;
}

void ion_unmapped_heap_destroy(struct ion_heap *heap)
{
	struct ion_unmapped_heap *umh =
	     container_of(heap, struct  ion_unmapped_heap, heap);

	gen_pool_destroy(umh->pool);
	kfree(umh);
	umh = NULL;
}

device_initcall(ion_add_unmapped_heap);
MODULE_LICENSE("GPL v2");
