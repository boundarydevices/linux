// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF secure heap exporter
 *
 * Copyright 2021 NXP.
 *
 */

#include <linux/genalloc.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_reserved_mem.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

#include "page_pool.h"
#include "deferred-free-helper.h"

static struct dma_heap *secure_heap;
static struct gen_pool *pool;

struct secure_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	int vmap_cnt;
	struct deferred_freelist_item deferred_free;
	void *vaddr;
	bool uncached;
};

struct dma_heap_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
	bool mapped;

	bool uncached;
};

struct rmem_secure {
	phys_addr_t base;
	phys_addr_t size;
};

static struct rmem_secure secure_data = {0};


static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sgtable_sg(table, sg, i) {
		sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static int secure_heap_attach(struct dma_buf *dmabuf,
			      struct dma_buf_attachment *attachment)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	struct sg_table *table;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table(&buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = attachment->dev;
	INIT_LIST_HEAD(&a->list);
	a->mapped = false;
	a->uncached = buffer->uncached;
	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void secure_heap_detach(struct dma_buf *dmabuf,
			       struct dma_buf_attachment *attachment)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a = attachment->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(a->table);
	kfree(a->table);
	kfree(a);
}

static struct sg_table *secure_heap_map_dma_buf(struct dma_buf_attachment *attachment,
						enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	int attr = 0;
	int ret;

	if (a->uncached)
		attr = DMA_ATTR_SKIP_CPU_SYNC;

	ret = dma_map_sgtable(attachment->dev, table, direction, attr);
	if (ret)
		return ERR_PTR(ret);

	a->mapped = true;
	return table;
}

static void secure_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
				      struct sg_table *table,
				      enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	int attr = 0;

	if (a->uncached)
		attr = DMA_ATTR_SKIP_CPU_SYNC;

	a->mapped = false;
	dma_unmap_sgtable(attachment->dev, table, direction, attr);
}

static int secure_heap_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
						enum dma_data_direction direction)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		invalidate_kernel_vmap_range(buffer->vaddr, buffer->len);

	if (!buffer->uncached) {
		list_for_each_entry(a, &buffer->attachments, list) {
			if (!a->mapped)
				continue;
			dma_sync_sgtable_for_cpu(a->dev, a->table, direction);
		}
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static int secure_heap_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					      enum dma_data_direction direction)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		flush_kernel_vmap_range(buffer->vaddr, buffer->len);

	if (!buffer->uncached) {
		list_for_each_entry(a, &buffer->attachments, list) {
			if (!a->mapped)
				continue;
			dma_sync_sgtable_for_device(a->dev, a->table, direction);
		}
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static int secure_heap_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;
	struct sg_table *table = &buffer->sg_table;
	unsigned long addr = vma->vm_start;
	struct sg_page_iter piter;
	int ret;

	if (buffer->uncached)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	for_each_sgtable_page(table, &piter, vma->vm_pgoff) {
		struct page *page = sg_page_iter_page(&piter);

		ret = remap_pfn_range(vma, addr, page_to_pfn(page), PAGE_SIZE,
				      vma->vm_page_prot);
		if (ret)
			return ret;
		addr += PAGE_SIZE;
		if (addr >= vma->vm_end)
			return 0;
	}
	return 0;
}

static void *secure_heap_do_vmap(struct secure_heap_buffer *buffer)
{
	struct sg_table *table = &buffer->sg_table;
	int npages = PAGE_ALIGN(buffer->len) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;
	struct sg_page_iter piter;
	pgprot_t pgprot = PAGE_KERNEL;
	void *vaddr;

	if (!pages)
		return ERR_PTR(-ENOMEM);

	if (buffer->uncached)
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sgtable_page(table, &piter, 0) {
		WARN_ON(tmp - pages >= npages);
		*tmp++ = sg_page_iter_page(&piter);
	}

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	vfree(pages);

	if (!vaddr)
		return ERR_PTR(-ENOMEM);

	return vaddr;
}

static void *secure_heap_vmap(struct dma_buf *dmabuf)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;
	void *vaddr;

	mutex_lock(&buffer->lock);
	if (buffer->vmap_cnt) {
		buffer->vmap_cnt++;
		vaddr = buffer->vaddr;
		goto out;
	}

	vaddr = secure_heap_do_vmap(buffer);
	if (IS_ERR(vaddr))
		goto out;

	buffer->vaddr = vaddr;
	buffer->vmap_cnt++;
out:
	mutex_unlock(&buffer->lock);

	return vaddr;
}

static void secure_heap_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	if (!--buffer->vmap_cnt) {
		vunmap(buffer->vaddr);
		buffer->vaddr = NULL;
	}
	mutex_unlock(&buffer->lock);
}

static int secure_heap_zero_buffer(struct secure_heap_buffer *buffer)
{
	struct sg_table *sgt = &buffer->sg_table;
	struct sg_page_iter piter;
	struct page *p;
	void *vaddr;
	int ret = 0;

	for_each_sgtable_page(sgt, &piter, 0) {
		p = sg_page_iter_page(&piter);
		vaddr = kmap_atomic(p);
		memset(vaddr, 0, PAGE_SIZE);
		kunmap_atomic(vaddr);
	}

	return ret;
}

static void secure_heap_buf_free(struct deferred_freelist_item *item,
				 enum df_reason reason)
{
	struct secure_heap_buffer *buffer;
	struct sg_table *table;
	struct scatterlist *sg;
	int i;

	buffer = container_of(item, struct secure_heap_buffer, deferred_free);
	/* Zero the buffer pages before adding back to the pool */
	if (reason == DF_NORMAL)
		if (secure_heap_zero_buffer(buffer))
			reason = DF_UNDER_PRESSURE; // On failure, just free

	table = &buffer->sg_table;
	for_each_sg(table->sgl, sg, table->nents, i) {
		gen_pool_free(pool,
						sg_dma_address(sg),
						sg_dma_len(sg));
	}

	sg_free_table(table);
	kfree(buffer);
}

static void secure_heap_dma_buf_release(struct dma_buf *dmabuf)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;
	int npages = PAGE_ALIGN(buffer->len) / PAGE_SIZE;

	deferred_free(&buffer->deferred_free, secure_heap_buf_free, npages);
}

static const struct dma_buf_ops secure_heap_buf_ops = {
	.attach = secure_heap_attach,
	.detach = secure_heap_detach,
	.map_dma_buf = secure_heap_map_dma_buf,
	.unmap_dma_buf = secure_heap_unmap_dma_buf,
	.begin_cpu_access = secure_heap_dma_buf_begin_cpu_access,
	.end_cpu_access = secure_heap_dma_buf_end_cpu_access,
	.mmap = secure_heap_mmap,
	.vmap = secure_heap_vmap,
	.vunmap = secure_heap_vunmap,
	.release = secure_heap_dma_buf_release,
};


static struct dma_buf *secure_heap_do_allocate(struct dma_heap *heap,
					       unsigned long len,
					       unsigned long fd_flags,
					       unsigned long heap_flags,
					       bool uncached)
{
	struct secure_heap_buffer *buffer;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	unsigned long size = roundup(len,  PAGE_SIZE);
	struct dma_buf *dmabuf;
	struct sg_table *table;
	struct list_head pages;
	struct page *page;
	int ret = -ENOMEM;
	unsigned long phy_addr;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->heap = heap;
	buffer->len = size;
	buffer->uncached = uncached;

	phy_addr = gen_pool_alloc(pool, size);
	if (!phy_addr)
		goto free_buffer;

	table = &buffer->sg_table;
	if (sg_alloc_table(table, 1, GFP_KERNEL))
		goto free_buffer;

	sg_set_page(table->sgl,
			phys_to_page(phy_addr),
			size, 0);
	sg_dma_address(table->sgl) = phy_addr;
	sg_dma_len(table->sgl) = size;

	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = &secure_heap_buf_ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_pages;
	}

	return dmabuf;

free_pages:
	sg_free_table(table);

free_buffer:
	kfree(buffer);

	return ERR_PTR(ret);
}

static struct dma_buf *secure_heap_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	// use uncache buffer here by default
	return secure_heap_do_allocate(heap, len, fd_flags, heap_flags, true);
	// use cache buffer
	// return secure_heap_do_allocate(heap, len, fd_flags, heap_flags, false);
}

static const struct dma_heap_ops secure_heap_ops = {
	.allocate = secure_heap_allocate,
};

static int secure_heap_create(void)
{
	struct dma_heap_export_info exp_info;
	int i;
	int ret;

	pool = gen_pool_create(PAGE_SHIFT+4, -1);
	if (!pool) {
		pr_info("can't creat gen poool");
		return -EINVAL;
	}

	/*add secure memory which reserved in dts into pool of genallocater*/
	struct device_node np;
	np.full_name = "secure";
	np.name = "secure";
	struct reserved_mem *rmem = of_reserved_mem_lookup(&np);
	if (!rmem) {
		pr_err("of_reserved_mem_lookup() returned NULL\n");
		gen_pool_destroy(pool);
		return 0;
	}

	secure_data.base = rmem->base;
	secure_data.size = rmem->size;

	if (secure_data.base == 0 || secure_data.size == 0) {
		pr_err("secure_data base or size is not correct\n");
		gen_pool_destroy(pool);
		return -EINVAL;
	}

	ret = gen_pool_add(pool, secure_data.base, secure_data.size, -1);
	if (ret < 0) {
		pr_err("failed to add memory into pool");
		gen_pool_destroy(pool);
		return -EINVAL;
	}

	exp_info.name = "secure";
	exp_info.ops = &secure_heap_ops;
	exp_info.priv = NULL;

	secure_heap = dma_heap_add(&exp_info);
	if (IS_ERR(secure_heap))
		return PTR_ERR(secure_heap);

	return 0;
}
module_init(secure_heap_create);
MODULE_LICENSE("GPL v2");
