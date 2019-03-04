/*
 * drivers/amlogic/media/common/ge2d/ge2d_dmabuf.c
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

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/pagemap.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>

#include "ge2d_log.h"
#include "ge2d_dmabuf.h"

static void clear_dma_buffer(struct aml_dma_buffer *buffer, int index);

static void *aml_mm_vmap(phys_addr_t phys, unsigned long size)
{
	u32 offset, npages;
	struct page **pages = NULL;
	pgprot_t pgprot = PAGE_KERNEL;
	void *vaddr;
	int i;

	offset = offset_in_page(phys);
	npages = DIV_ROUND_UP(size + offset, PAGE_SIZE);

	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return NULL;
	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}
	/* pgprot = pgprot_writecombine(PAGE_KERNEL); */

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_err("vmaped fail, size: %d\n",
			npages << PAGE_SHIFT);
		vfree(pages);
		return NULL;
	}
	vfree(pages);
	ge2d_log_dbg("[HIGH-MEM-MAP] pa(%lx) to va(%p), size: %d\n",
		(unsigned long)phys, vaddr, npages << PAGE_SHIFT);
	return vaddr;
}

static void *aml_map_phyaddr_to_virt(dma_addr_t phys, unsigned long size)
{
	void *vaddr = NULL;

	if (!PageHighMem(phys_to_page(phys)))
		return phys_to_virt(phys);
	vaddr = aml_mm_vmap(phys, size);
	return vaddr;
}

/* dma free*/
static void aml_dma_put(void *buf_priv)
{
	struct aml_dma_buf *buf = buf_priv;
	struct page *cma_pages = NULL;
	void *vaddr = (void *)(PAGE_MASK & (ulong)buf->vaddr);

	if (!atomic_dec_and_test(&buf->refcount)) {
		ge2d_log_dbg("ge2d aml_dma_put, refcont=%d\n",
			atomic_read(&buf->refcount));
		return;
	}
	cma_pages = phys_to_page(buf->dma_addr);
	if (is_vmalloc_or_module_addr(vaddr))
		vunmap(vaddr);

	if (!dma_release_from_contiguous(buf->dev, cma_pages,
					 buf->size >> PAGE_SHIFT)) {
		pr_err("failed to release cma buffer\n");
	}
	buf->vaddr = NULL;
	clear_dma_buffer((struct aml_dma_buffer *)buf->priv, buf->index);
	put_device(buf->dev);
	kfree(buf);
	ge2d_log_dbg("ge2d free:aml_dma_buf=0x%p,buf->index=%d\n",
		buf, buf->index);
}

static void *aml_dma_alloc(struct device *dev, unsigned long attrs,
			  unsigned long size, enum dma_data_direction dma_dir,
			  gfp_t gfp_flags)
{
	struct aml_dma_buf *buf;
	struct page *cma_pages = NULL;
	dma_addr_t paddr = 0;

	if (WARN_ON(!dev))
		return (void *)(-EINVAL);

	buf = kzalloc(sizeof(struct aml_dma_buf), GFP_KERNEL | gfp_flags);
	if (!buf)
		return NULL;

	if (attrs)
		buf->attrs = attrs;
	cma_pages = dma_alloc_from_contiguous(dev,
		size >> PAGE_SHIFT, 0);
	if (cma_pages) {
		paddr = page_to_phys(cma_pages);
	} else {
		pr_err("failed to alloc cma pages.\n");
		return NULL;
	}
	buf->vaddr = aml_map_phyaddr_to_virt(paddr, size);
	buf->dev = get_device(dev);
	buf->size = size;
	buf->dma_dir = dma_dir;
	buf->dma_addr = paddr;
	atomic_inc(&buf->refcount);
	ge2d_log_dbg("aml_dma_buf=0x%p, refcont=%d\n",
		buf, atomic_read(&buf->refcount));

	return buf;
}

static int aml_dma_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct aml_dma_buf *buf = buf_priv;
	unsigned long pfn = 0;
	unsigned long vsize = vma->vm_end - vma->vm_start;
	int ret = -1;

	if (!buf || !vma) {
		pr_err("No memory to map\n");
		return -EINVAL;
	}

	pfn = buf->dma_addr >> PAGE_SHIFT;
	ret = remap_pfn_range(vma, vma->vm_start, pfn,
		vsize, vma->vm_page_prot);
	if (ret) {
		pr_err("Remapping memory, error: %d\n", ret);
		return ret;
	}
	vma->vm_flags |= VM_DONTEXPAND;
	ge2d_log_dbg("mapped dma addr 0x%08lx at 0x%08lx, size %d\n",
		(unsigned long)buf->dma_addr, vma->vm_start,
		buf->size);
	return 0;
}

/*********************************************/
/*         DMABUF ops for exporters          */
/*********************************************/
struct aml_attachment {
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

static int aml_dmabuf_ops_attach(struct dma_buf *dbuf, struct device *dev,
	struct dma_buf_attachment *dbuf_attach)
{
	struct aml_attachment *attach;
	struct aml_dma_buf *buf = dbuf->priv;
	int num_pages = PAGE_ALIGN(buf->size) / PAGE_SIZE;
	struct sg_table *sgt;
	struct scatterlist *sg;
	phys_addr_t phys = buf->dma_addr;
	unsigned int i;
	int ret;

	attach = kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach)
		return -ENOMEM;

	sgt = &attach->sgt;
	/* Copy the buf->base_sgt scatter list to the attachment, as we can't
	 * map the same scatter list to multiple attachments at the same time.
	 */
	ret = sg_alloc_table(sgt, num_pages, GFP_KERNEL);
	if (ret) {
		kfree(attach);
		return -ENOMEM;
	}
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		struct page *page = phys_to_page(phys);

		if (!page) {
			sg_free_table(sgt);
			kfree(attach);
			return -ENOMEM;
		}
		sg_set_page(sg, page, PAGE_SIZE, 0);
		phys += PAGE_SIZE;
	}

	attach->dma_dir = DMA_NONE;
	dbuf_attach->priv = attach;

	return 0;
}

static void aml_dmabuf_ops_detach(struct dma_buf *dbuf,
	struct dma_buf_attachment *db_attach)
{
	struct aml_attachment *attach = db_attach->priv;
	struct sg_table *sgt;

	if (!attach)
		return;

	sgt = &attach->sgt;

	/* release the scatterlist cache */
	if (attach->dma_dir != DMA_NONE)
		dma_unmap_sg(db_attach->dev, sgt->sgl, sgt->orig_nents,
			attach->dma_dir);
	sg_free_table(sgt);
	kfree(attach);
	db_attach->priv = NULL;

}

static struct sg_table *aml_dmabuf_ops_map(
	struct dma_buf_attachment *db_attach, enum dma_data_direction dma_dir)
{
	struct aml_attachment *attach = db_attach->priv;
	/* stealing dmabuf mutex to serialize map/unmap operations */
	struct mutex *lock = &db_attach->dmabuf->lock;
	struct sg_table *sgt;

	mutex_lock(lock);

	sgt = &attach->sgt;
	/* return previously mapped sg table */
	if (attach->dma_dir == dma_dir) {
		mutex_unlock(lock);
		return sgt;
	}

	/* release any previous cache */
	if (attach->dma_dir != DMA_NONE) {
		dma_unmap_sg(db_attach->dev, sgt->sgl, sgt->orig_nents,
			attach->dma_dir);
		attach->dma_dir = DMA_NONE;
	}
	/* mapping to the client with new direction */
	sgt->nents = dma_map_sg(db_attach->dev, sgt->sgl, sgt->orig_nents,
				dma_dir);
	if (!sgt->nents) {
		pr_err("failed to map scatterlist\n");
		mutex_unlock(lock);
		return (void *)(-EIO);
	}

	attach->dma_dir = dma_dir;

	mutex_unlock(lock);
	return sgt;
}

static void aml_dmabuf_ops_unmap(struct dma_buf_attachment *db_attach,
	struct sg_table *sgt, enum dma_data_direction dma_dir)
{
	/* nothing to be done here */
}

static void aml_dmabuf_ops_release(struct dma_buf *dbuf)
{
	/* drop reference obtained in vb2_dc_get_dmabuf */
	aml_dma_put(dbuf->priv);
}

static void *aml_dmabuf_ops_kmap(struct dma_buf *dbuf, unsigned long pgnum)
{
	struct aml_dma_buf *buf = dbuf->priv;

	return buf->vaddr ? buf->vaddr + pgnum * PAGE_SIZE : NULL;
}

static void *aml_dmabuf_ops_vmap(struct dma_buf *dbuf)
{
	struct aml_dma_buf *buf = dbuf->priv;

	return buf->vaddr;
}

static int aml_dmabuf_ops_mmap(struct dma_buf *dbuf,
	struct vm_area_struct *vma)
{
	return aml_dma_mmap(dbuf->priv, vma);
}

static struct dma_buf_ops ge2d_dmabuf_ops = {
	.attach = aml_dmabuf_ops_attach,
	.detach = aml_dmabuf_ops_detach,
	.map_dma_buf = aml_dmabuf_ops_map,
	.unmap_dma_buf = aml_dmabuf_ops_unmap,
	.kmap = aml_dmabuf_ops_kmap,
	.kmap_atomic = aml_dmabuf_ops_kmap,
	.vmap = aml_dmabuf_ops_vmap,
	.mmap = aml_dmabuf_ops_mmap,
	.release = aml_dmabuf_ops_release,
};

static struct dma_buf *get_dmabuf(void *buf_priv, unsigned long flags)
{
	struct aml_dma_buf *buf = buf_priv;
	struct dma_buf *dbuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.ops = &ge2d_dmabuf_ops;
	exp_info.size = buf->size;
	exp_info.flags = flags;
	exp_info.priv = buf;
	if (WARN_ON(!buf->vaddr))
		return NULL;

	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(dbuf))
		return NULL;

	/* dmabuf keeps reference to vb2 buffer */
	atomic_inc(&buf->refcount);
	ge2d_log_dbg("get_dmabuf, refcount=%d\n",
		atomic_read(&buf->refcount));
	return dbuf;
}

/* ge2d dma-buf api.h*/
static int find_empty_dma_buffer(struct aml_dma_buffer *buffer)
{
	int i;
	int found = 0;

	for (i = 0; i < AML_MAX_DMABUF; i++) {
		if (buffer->gd_buffer[i].alloc)
			continue;
		else {
			ge2d_log_dbg("find_empty_dma_buffer i=%d\n", i);
			found = 1;
			break;
		}
	}
	if (found)
		return i;
	else
		return -1;
}

static void clear_dma_buffer(struct aml_dma_buffer *buffer, int index)
{
	mutex_lock(&(buffer->lock));
	buffer->gd_buffer[index].mem_priv = NULL;
	buffer->gd_buffer[index].index = 0;
	buffer->gd_buffer[index].alloc = 0;
	mutex_unlock(&(buffer->lock));
}

void *ge2d_dma_buffer_create(void)
{
	int i;
	struct aml_dma_buffer *buffer;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return NULL;

	mutex_init(&buffer->lock);
	for (i = 0; i < AML_MAX_DMABUF; i++) {
		buffer->gd_buffer[i].mem_priv = NULL;
		buffer->gd_buffer[i].index = 0;
		buffer->gd_buffer[i].alloc = 0;
	}
	return buffer;
}

void ge2d_dma_buffer_destroy(struct aml_dma_buffer *buffer)
{
	kfree(buffer);
}

int ge2d_dma_buffer_alloc(struct aml_dma_buffer *buffer,
	struct device *dev,
	struct ge2d_dmabuf_req_s *ge2d_req_buf)
{
	void *buf;
	struct aml_dma_buf *dma_buf;
	unsigned int size;
	int index;

	if (WARN_ON(!dev))
		return (-EINVAL);
	if (!ge2d_req_buf)
		return (-EINVAL);
	if (!buffer)
		return (-EINVAL);

	size = PAGE_ALIGN(ge2d_req_buf->len);
	if (size == 0)
		return (-EINVAL);
	buf = aml_dma_alloc(dev, 0, size, ge2d_req_buf->dma_dir,
		GFP_HIGHUSER | __GFP_ZERO);
	if (!buf)
		return (-ENOMEM);
	mutex_lock(&(buffer->lock));
	index = find_empty_dma_buffer(buffer);
	if ((index < 0) || (index >= AML_MAX_DMABUF)) {
		pr_err("no empty buffer found\n");
		mutex_unlock(&(buffer->lock));
		aml_dma_put(buf);
		return (-ENOMEM);
	}
	((struct aml_dma_buf *)buf)->priv = buffer;
	((struct aml_dma_buf *)buf)->index = index;
	buffer->gd_buffer[index].mem_priv = buf;
	buffer->gd_buffer[index].index = index;
	buffer->gd_buffer[index].alloc = 1;
	mutex_unlock(&(buffer->lock));
	ge2d_req_buf->index = index;
	dma_buf = (struct aml_dma_buf *)buf;
	if (dma_buf->dma_dir == DMA_FROM_DEVICE)
		dma_sync_single_for_cpu(dma_buf->dev,
			dma_buf->dma_addr,
			dma_buf->size, DMA_FROM_DEVICE);
	return 0;
}

int ge2d_dma_buffer_free(struct aml_dma_buffer *buffer, int index)
{
	struct aml_dma_buf *buf;

	if (!buffer)
		return (-EINVAL);
	if ((index < 0) || (index >= AML_MAX_DMABUF))
		return (-EINVAL);

	buf = buffer->gd_buffer[index].mem_priv;
	if (!buf) {
		pr_err("aml_dma_buf is null\n");
		return (-EINVAL);
	}
	aml_dma_put(buf);
	return 0;
}

int ge2d_dma_buffer_export(struct aml_dma_buffer *buffer,
	struct ge2d_dmabuf_exp_s *ge2d_exp_buf)
{
	struct aml_dma_buf *buf;
	struct dma_buf *dbuf;
	int ret, index;
	unsigned int flags;

	if (!ge2d_exp_buf)
		return (-EINVAL);
	if (!buffer)
		return (-EINVAL);

	index = ge2d_exp_buf->index;
	if ((index < 0) || (index >= AML_MAX_DMABUF))
		return (-EINVAL);

	flags = ge2d_exp_buf->flags;
	buf = buffer->gd_buffer[index].mem_priv;
	if (!buf) {
		pr_err("aml_dma_buf is null\n");
		return (-EINVAL);
	}

	dbuf = get_dmabuf(buf, flags & O_ACCMODE);
	if (IS_ERR_OR_NULL(dbuf)) {
		pr_err("failed to export buffer %d\n", index);
		return -EINVAL;
	}
	ret = dma_buf_fd(dbuf, flags & ~O_ACCMODE);
	if (ret < 0) {
		pr_err("buffer %d, failed to export (%d)\n",
			index, ret);
		dma_buf_put(dbuf);
		return ret;
	}

	ge2d_log_dbg("buffer %d,exported as %d descriptor\n",
		index, ret);
	ge2d_exp_buf->fd = ret;
	return 0;
}

int ge2d_dma_buffer_map(struct aml_dma_cfg *cfg)
{
	long ret = -1;
	int fd = -1;
	struct dma_buf *dbuf = NULL;
	struct dma_buf_attachment *d_att = NULL;
	struct sg_table *sg = NULL;
	void *vaddr = NULL;
	struct device *dev = NULL;
	enum dma_data_direction dir;

	if (cfg == NULL || (cfg->fd < 0) || cfg->dev == NULL) {
		pr_err("error input param");
		return -EINVAL;
	}
	fd = cfg->fd;
	dev = cfg->dev;
	dir = cfg->dir;

	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		pr_err("failed to get dma buffer");
		return -EINVAL;
	}

	d_att = dma_buf_attach(dbuf, dev);
	if (IS_ERR(d_att)) {
		pr_err("failed to set dma attach");
		goto attach_err;
	}

	sg = dma_buf_map_attachment(d_att, dir);
	if (IS_ERR(sg)) {
		pr_err("failed to get dma sg");
		goto map_attach_err;
	}

	ret = dma_buf_begin_cpu_access(dbuf, dir);
	if (ret != 0) {
		pr_err("failed to access dma buff");
		goto access_err;
	}

	vaddr = dma_buf_vmap(dbuf);
	if (vaddr == NULL) {
		pr_err("failed to vmap dma buf");
		goto vmap_err;
	}
	cfg->dbuf = dbuf;
	cfg->attach = d_att;
	cfg->vaddr = vaddr;
	cfg->sg = sg;
	ge2d_log_dbg("%s, dbuf=0x%p\n", __func__, dbuf);
	return ret;

vmap_err:
	dma_buf_end_cpu_access(dbuf, dir);

access_err:
	dma_buf_unmap_attachment(d_att, sg, dir);

map_attach_err:
	dma_buf_detach(dbuf, d_att);

attach_err:
	dma_buf_put(dbuf);

	return ret;
}

int ge2d_dma_buffer_get_phys(struct aml_dma_cfg *cfg, unsigned long *addr)
{
	struct sg_table *sg_table;
	struct page *page;
	int ret;

	ret = ge2d_dma_buffer_map(cfg);
	if (ret < 0) {
		pr_err("gdc_dma_buffer_map failed\n");
		return ret;
	}
	if (cfg->sg) {
		sg_table = cfg->sg;
		page = sg_page(sg_table->sgl);
		*addr = PFN_PHYS(page_to_pfn(page));
		ret = 0;
	}
	return ret;
}

void ge2d_dma_buffer_unmap(struct aml_dma_cfg *cfg)
{
	int fd = -1;
	struct dma_buf *dbuf = NULL;
	struct dma_buf_attachment *d_att = NULL;
	struct sg_table *sg = NULL;
	void *vaddr = NULL;
	struct device *dev = NULL;
	enum dma_data_direction dir;

	if (cfg == NULL || (cfg->fd < 0) || cfg->dev == NULL
			|| cfg->dbuf == NULL || cfg->vaddr == NULL
			|| cfg->attach == NULL || cfg->sg == NULL) {
		pr_err("Error input param");
		return;
	}
	fd = cfg->fd;
	dev = cfg->dev;
	dir = cfg->dir;
	dbuf = cfg->dbuf;
	vaddr = cfg->vaddr;
	d_att = cfg->attach;
	sg = cfg->sg;

	dma_buf_vunmap(dbuf, vaddr);

	dma_buf_end_cpu_access(dbuf, dir);

	dma_buf_unmap_attachment(d_att, sg, dir);

	dma_buf_detach(dbuf, d_att);

	dma_buf_put(dbuf);

	ge2d_log_dbg("%s, dbuf=0x%p\n", __func__, dbuf);
}

void ge2d_dma_buffer_dma_flush(struct device *dev, int fd)
{
	struct dma_buf *dmabuf;
	struct aml_dma_buf *buf;

	ge2d_log_dbg("ge2d_dma_buffer_dma_flush fd=%d\n", fd);
	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		pr_err("dma_buf_get failed\n");
		return;
	}
	buf = dmabuf->priv;
	if (!buf) {
		pr_err("error input param");
		return;
	}
	if ((buf->size > 0) && (buf->dev == dev))
		dma_sync_single_for_device(buf->dev, buf->dma_addr,
			buf->size, DMA_TO_DEVICE);
	dma_buf_put(dmabuf);
}

void ge2d_dma_buffer_cache_flush(struct device *dev, int fd)
{
	struct dma_buf *dmabuf;
	struct aml_dma_buf *buf;

	ge2d_log_dbg("ge2d_dma_buffer_cache_flush fd=%d\n", fd);
	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		pr_err("dma_buf_get failed\n");
		return;
	}
	buf = dmabuf->priv;
	if (!buf) {
		pr_err("error input param");
		return;
	}
	if ((buf->size > 0) && (buf->dev == dev))
		dma_sync_single_for_cpu(buf->dev, buf->dma_addr,
			buf->size, DMA_FROM_DEVICE);
	dma_buf_put(dmabuf);
}


