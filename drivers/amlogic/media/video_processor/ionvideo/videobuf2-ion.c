/*
 * drivers/amlogic/media/video_processor/ionvideo/videobuf2-ion.c
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

#include <linux/io.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-memops.h>

/*#include <asm/mach/map.h>*/
#include <ion.h>
#include "ion_priv.h"
#include "videobuf2-ion.h"

struct vb2_ion_buf {
	void *vaddr;
	struct page **pages;
	struct vm_area_struct *vma;
	int write;
	unsigned long size;
	unsigned int n_pages;
	atomic_t refcount;
	struct vb2_vmarea_handler handler;
	struct dma_buf *dbuf;
};

static void vb2_ion_put(void *buf_priv);

#ifdef IONVIDEO_DEBUG
static void *vb2_ion_alloc(void *alloc_ctx, unsigned long size, gfp_t gfp_flags)
{
	struct vb2_ion_buf *buf;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL | gfp_flags);
	if (!buf)
		return NULL;

	buf->size = size;
	buf->vaddr = vmalloc_user(buf->size);
	buf->handler.refcount = &buf->refcount;
	buf->handler.put = vb2_ion_put;
	buf->handler.arg = buf;

	if (!buf->vaddr) {
		pr_debug("ion of size %ld failed\n", buf->size);
		kfree(buf);
		return NULL;
	}

	atomic_inc(&buf->refcount);
	return buf;
}
#endif

static void vb2_ion_put(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	if (atomic_dec_and_test(&buf->refcount)) {
		vfree(buf->vaddr);
		kfree(buf);
	}
}

#ifdef IONVIDEO_DEBUG
static void *vb2_ion_get_userptr(void *alloc_ctx, unsigned long vaddr,
					unsigned long size, int write)
{
	struct vb2_ion_buf *buf;
	unsigned long first, last;
	int n_pages, offset;
	struct vm_area_struct *vma;
	dma_addr_t physp;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return NULL;

	buf->write = write;
	offset = vaddr & ~PAGE_MASK;
	buf->size = size;

	vma = find_vma(current->mm, vaddr);
	if (vma && (vma->vm_flags & VM_PFNMAP) && (vma->vm_pgoff)) {
#ifdef IONVIDEO_DEBUG
		if (vb2_get_contig_userptr(vaddr, size, &vma, &physp))
			goto fail_pages_array_alloc;
#endif
		buf->vma = vma;
		buf->vaddr = ioremap_nocache(physp, size);
		if (!buf->vaddr)
			goto fail_pages_array_alloc;
	} else {
		first = vaddr >> PAGE_SHIFT;
		last = (vaddr + size - 1) >> PAGE_SHIFT;
		buf->n_pages = last - first + 1;
		buf->pages = kcalloc(buf->n_pages, sizeof(struct page *),
					GFP_KERNEL);
		if (!buf->pages)
			goto fail_pages_array_alloc;

		/* current->mm->mmap_sem is taken by videobuf2 core */
#ifdef IONVIDEO_DEBUG
		n_pages = get_user_pages(current, current->mm,
						vaddr & PAGE_MASK, buf->n_pages,
						write, 1, /* force */
						buf->pages, NULL);
#endif
		if (n_pages != buf->n_pages)
			goto fail_get_user_pages;

		buf->vaddr = vm_map_ram(buf->pages, buf->n_pages, -1,
					PAGE_KERNEL);
		if (!buf->vaddr)
			goto fail_get_user_pages;
	}

	buf->vaddr += offset;
	return buf;

fail_get_user_pages:
	pr_debug("get_user_pages requested/got: %d/%d]\n", n_pages,
			buf->n_pages);
	while (--n_pages >= 0)
		put_page(buf->pages[n_pages]);
	kfree(buf->pages);

fail_pages_array_alloc: kfree(buf);

	return NULL;
}
#endif

static void vb2_ion_put_userptr(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;
	unsigned long vaddr = (unsigned long)buf->vaddr & PAGE_MASK;
	unsigned int i;

	if (buf->pages) {
		if (vaddr)
			vm_unmap_ram((void *)vaddr, buf->n_pages);
		for (i = 0; i < buf->n_pages; ++i) {
			if (buf->write)
				set_page_dirty_lock(buf->pages[i]);
			put_page(buf->pages[i]);
		}
		kfree(buf->pages);
	} else {
#ifdef IONVIDEO_DEBUG
		if (buf->vma)
			vb2_put_vma(buf->vma);
#endif
		iounmap(buf->vaddr);
	}
	kfree(buf);
}

static void *vb2_ion_vaddr(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	if (!buf->vaddr) {
		pr_err("Address of an unallocated plane requested");
		pr_err("or cannot map user pointer\n");
		return NULL;
	}

	return buf->vaddr;
}

static unsigned int vb2_ion_num_users(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	return atomic_read(&buf->refcount);
}

static int vb2_ion_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct vb2_ion_buf *buf = buf_priv;
	int ret;

	pr_info("11vb2_ion_mmap\n");
	if (!buf) {
		pr_err("No memory to map\n");
		return -EINVAL;
	}

	ret = remap_vmalloc_range(vma, buf->vaddr, 0);
	if (ret) {
		pr_err("Remapping ion memory, error: %d\n", ret);
		return ret;
	}

	/*
	 * Make sure that vm_areas for 2 buffers won't be merged together
	 */
	vma->vm_flags |= VM_DONTEXPAND;

	/*
	 * Use common vm_area operations to track buffer refcount.
	 */
	vma->vm_private_data = &buf->handler;
	vma->vm_ops = &vb2_common_vm_ops;

	vma->vm_ops->open(vma);
	pr_info("22vb2_ion_mmap\n");
	return 0;
}

/*********************************************/
/*       callbacks for DMABUF buffers        */
/*********************************************/

static int vb2_ion_map_dmabuf(void *mem_priv)
{
	/* struct vb2_ion_buf *buf = mem_priv; */
	/*  */
	/* struct ion_buffer *buffer = buf->dbuf->priv; */
	/* int mtype = MT_MEMORY_NONCACHED; */
	/*  */
	/* if (buffer->flags & ION_FLAG_CACHED) */
	/* mtype = MT_MEMORY; */
#if 0
	buf->vaddr = __arm_ioremap(buffer->priv_phys, buffer->size, mtype);

	return buf->vaddr ? 0 : -EFAULT;
#else
	return 0;
#endif
}

static void vb2_ion_unmap_dmabuf(void *mem_priv)
{
	/* struct vb2_ion_buf *buf = mem_priv; */

#if 0
	__arm_iounmap(buf->vaddr);

	buf->vaddr = NULL;
#endif
}

static void vb2_ion_detach_dmabuf(void *mem_priv)
{
	struct vb2_ion_buf *buf = mem_priv;

	if (buf->vaddr)
		dma_buf_vunmap(buf->dbuf, buf->vaddr);

	kfree(buf);
}

#ifdef IONVIDEO_DEBUG
static void *vb2_ion_attach_dmabuf(void *alloc_ctx, struct dma_buf *dbuf,
					unsigned long size, int write)
{
	struct vb2_ion_buf *buf;

	if (dbuf->size < size)
		return ERR_PTR(-EFAULT);

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dbuf = dbuf;
	buf->write = write;
	buf->size = size;

	return buf;
}
#endif

static void *vb2_ion_cookie(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	struct ion_buffer *buffer = buf->dbuf->priv;
#if 0
	return (void *)buffer->priv_phys;
#endif
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);

	ion_phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	return (void *)paddr;
}

const struct vb2_mem_ops vb2_ion_memops = {
#ifdef IONVIDEO_DEBUG
	.alloc = vb2_ion_alloc,
#endif
	.put = vb2_ion_put,
#ifdef IONVIDEO_DEBUG
	.get_userptr = vb2_ion_get_userptr,
#endif
	.put_userptr = vb2_ion_put_userptr,
	.map_dmabuf = vb2_ion_map_dmabuf,
	.unmap_dmabuf = vb2_ion_unmap_dmabuf,
#ifdef IONVIDEO_DEBUG
	.attach_dmabuf = vb2_ion_attach_dmabuf,
#endif
	.detach_dmabuf = vb2_ion_detach_dmabuf,
	.vaddr = vb2_ion_vaddr,
	.mmap = vb2_ion_mmap,
	.num_users = vb2_ion_num_users,
	.cookie = vb2_ion_cookie,
};
EXPORT_SYMBOL_GPL(vb2_ion_memops);

MODULE_DESCRIPTION("ion memory handling routines for videobuf2");
MODULE_AUTHOR("Shuai Cao <shuai.cao@amlogic.com>");
MODULE_LICENSE("GPL");
