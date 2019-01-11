/*
 * drivers/amlogic/media/gdc/app/gdc_module.c
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

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uio_driver.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <meson_ion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-buf.h>

#include <linux/of_address.h>
#include <api/gdc_api.h>
#include "system_log.h"

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
//gdc configuration sequence
#include "gdc_config.h"
#include "gdc_dmabuf.h"

unsigned int gdc_log_level;
struct gdc_manager_s gdc_manager;

static const struct of_device_id gdc_dt_match[] = {
	{.compatible = "amlogic, g12b-gdc"},
	{} };

MODULE_DEVICE_TABLE(of, gdc_dt_match);

//////
static int meson_gdc_open(struct inode *inode, struct file *file)
{
	struct meson_gdc_dev_t *gdc_dev = gdc_manager.gdc_dev;
	struct mgdc_fh_s *fh = NULL;
	char ion_client_name[32];
	int rc = 0;

	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (fh == NULL) {
		gdc_log(LOG_DEBUG, "devm alloc failed\n");
		return -ENOMEM;
	}

	get_task_comm(fh->task_comm, current);
	gdc_log(LOG_DEBUG, "%s, %d, call from %s\n",
			__func__, __LINE__, fh->task_comm);

	file->private_data = fh;
	snprintf(ion_client_name, sizeof(fh->task_comm),
			"gdc-%s", fh->task_comm);
	if (!fh->ion_client)
		fh->ion_client = meson_ion_client_create(-1, ion_client_name);

	fh->gdev = gdc_dev;

	gdc_log(LOG_CRIT, "Success open\n");

	return rc;
}

static int meson_gdc_release(struct inode *inode, struct file *file)
{
	struct mgdc_fh_s *fh = file->private_data;
	struct page *cma_pages = NULL;
	bool rc = false;
	int ret = 0;

	if (fh->ion_client) {
		ion_client_destroy(fh->ion_client);
		fh->ion_client = NULL;
	}

	if (fh->i_kaddr != 0 && fh->i_len != 0) {
		cma_pages = virt_to_page(fh->i_kaddr);
		rc = dma_release_from_contiguous(&fh->gdev->pdev->dev,
						cma_pages,
						fh->i_len >> PAGE_SHIFT);
		if (rc == false) {
			ret = ret - 1;
			gdc_log(LOG_ERR, "Failed to release input buff\n");
		}

		fh->i_kaddr = NULL;
		fh->i_paddr = 0;
		fh->i_len = 0;
	}

	if (fh->o_kaddr != 0 && fh->o_len != 0) {
		cma_pages = virt_to_page(fh->o_kaddr);
		rc = dma_release_from_contiguous(&fh->gdev->pdev->dev,
						cma_pages,
						fh->o_len >> PAGE_SHIFT);
		if (rc == false) {
			ret = ret - 1;
			gdc_log(LOG_ERR, "Failed to release output buff\n");
		}

		fh->o_kaddr = NULL;
		fh->o_paddr = 0;
		fh->o_len = 0;
	}

	if (fh->c_kaddr != 0 && fh->c_len != 0) {
		cma_pages = virt_to_page(fh->c_kaddr);
		rc = dma_release_from_contiguous(&fh->gdev->pdev->dev,
						cma_pages,
						fh->c_len >> PAGE_SHIFT);
		if (rc == false) {
			ret = ret - 1;
			gdc_log(LOG_ERR, "Failed to release config buff\n");
		}

		fh->c_kaddr = NULL;
		fh->c_paddr = 0;
		fh->c_len = 0;
	}

	kfree(fh);
	fh = NULL;

	if (ret == 0)
		gdc_log(LOG_CRIT, "Success release\n");
	else
		gdc_log(LOG_ERR, "Error release\n");

	return ret;
}

static long meson_gdc_set_buff(void *f_fh,
					struct page *cma_pages,
					unsigned long len)
{
	int ret = 0;
	struct mgdc_fh_s *fh = NULL;

	if (f_fh == NULL || cma_pages == NULL || len == 0) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	fh = f_fh;

	switch (fh->mmap_type) {
	case INPUT_BUFF_TYPE:
		if (fh->i_paddr != 0 && fh->i_kaddr != NULL)
			return -EAGAIN;
		fh->i_paddr = page_to_phys(cma_pages);
		fh->i_kaddr = phys_to_virt(fh->i_paddr);
		fh->i_len = len;
	break;
	case OUTPUT_BUFF_TYPE:
		if (fh->o_paddr != 0 && fh->o_kaddr != NULL)
			return -EAGAIN;
		fh->o_paddr = page_to_phys(cma_pages);
		fh->o_kaddr = phys_to_virt(fh->o_paddr);
		fh->o_len = len;
	break;
	case CONFIG_BUFF_TYPE:
		if (fh->c_paddr != 0 && fh->c_kaddr != NULL)
			return -EAGAIN;
		fh->c_paddr = page_to_phys(cma_pages);
		fh->c_kaddr = phys_to_virt(fh->c_paddr);
		fh->c_len = len;
	break;
	default:
		gdc_log(LOG_ERR, "Error mmap type:0x%x\n", fh->mmap_type);
		ret = -EINVAL;
	break;
	}

	return ret;
}

static long meson_gdc_set_input_addr(uint32_t start_addr,
					struct gdc_cmd_s *gdc_cmd)
{
	struct gdc_config_s *gc = NULL;

	if (gdc_cmd == NULL || start_addr == 0) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	gc = &gdc_cmd->gdc_config;

	switch (gc->format) {
	case NV12:
		gdc_cmd->y_base_addr = start_addr;
		gdc_cmd->uv_base_addr = start_addr +
			gc->input_y_stride * gc->input_height;
	break;
	case YV12:
		gdc_cmd->y_base_addr = start_addr;
		gdc_cmd->u_base_addr = start_addr +
			gc->input_y_stride * gc->input_height;
		gdc_cmd->v_base_addr = gdc_cmd->u_base_addr +
			gc->input_c_stride * gc->input_height / 2;
	break;
	case Y_GREY:
		gdc_cmd->y_base_addr = start_addr;
		gdc_cmd->u_base_addr = 0;
		gdc_cmd->v_base_addr = 0;
	break;
	case YUV444_P:
		gdc_cmd->y_base_addr = start_addr;
		gdc_cmd->u_base_addr = start_addr +
			gc->input_y_stride * gc->input_height;
		gdc_cmd->v_base_addr = gdc_cmd->u_base_addr +
			gc->input_c_stride * gc->input_height;
	break;
	case RGB444_P:
		gdc_cmd->y_base_addr = start_addr;
		gdc_cmd->u_base_addr = start_addr +
			gc->input_y_stride * gc->input_height;
		gdc_cmd->v_base_addr = gdc_cmd->u_base_addr +
			gc->input_c_stride * gc->input_height;
	break;
	default:
		gdc_log(LOG_ERR, "Error config format\n");
		return -EINVAL;
	break;
	}

	return 0;
}

static void meson_gdc_dma_flush(struct device *dev,
					dma_addr_t addr,
					size_t size)
{
	if (dev == NULL) {
		gdc_log(LOG_ERR, "Error input param");
		return;
	}

	dma_sync_single_for_device(dev, addr, size, DMA_TO_DEVICE);
}

static void meson_gdc_cache_flush(struct device *dev,
					dma_addr_t addr,
					size_t size)
{
	if (dev == NULL) {
		gdc_log(LOG_ERR, "Error input param");
		return;
	}

	dma_sync_single_for_cpu(dev, addr, size, DMA_FROM_DEVICE);
}

static long meson_gdc_dma_map(struct gdc_dma_cfg *cfg)
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
		gdc_log(LOG_ERR, "Error input param");
		return -EINVAL;
	}

	fd = cfg->fd;
	dev = cfg->dev;
	dir = cfg->dir;

	dbuf = dma_buf_get(fd);
	if (dbuf == NULL) {
		gdc_log(LOG_ERR, "Failed to get dma buffer");
		return -EINVAL;
	}

	d_att = dma_buf_attach(dbuf, dev);
	if (d_att == NULL) {
		gdc_log(LOG_ERR, "Failed to set dma attach");
		goto attach_err;
	}

	sg = dma_buf_map_attachment(d_att, dir);
	if (sg == NULL) {
		gdc_log(LOG_ERR, "Failed to get dma sg");
		goto map_attach_err;
	}

	ret = dma_buf_begin_cpu_access(dbuf, dir);
	if (ret != 0) {
		gdc_log(LOG_ERR, "Failed to access dma buff");
		goto access_err;
	}

	vaddr = dma_buf_vmap(dbuf);
	if (vaddr == NULL) {
		gdc_log(LOG_ERR, "Failed to vmap dma buf");
		goto vmap_err;
	}

	cfg->dbuf = dbuf;
	cfg->attach = d_att;
	cfg->vaddr = vaddr;
	cfg->sg = sg;

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


static void meson_gdc_dma_unmap(struct gdc_dma_cfg *cfg)
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
		gdc_log(LOG_ERR, "Error input param");
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
}

static long meson_gdc_init_dma_addr(struct mgdc_fh_s *fh,
	struct gdc_settings *gs)
{
	long ret = -1;
	struct gdc_dma_cfg *dma_cfg = NULL;
	struct gdc_cmd_s *gdc_cmd = &fh->gdc_cmd;
	struct gdc_config_s *gc = &gdc_cmd->gdc_config;

	if (fh == NULL || gs == NULL) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	switch (gc->format) {
	case NV12:
		dma_cfg = &fh->y_dma_cfg;
		memset(dma_cfg, 0, sizeof(*dma_cfg));
		dma_cfg->dir = DMA_TO_DEVICE;
		dma_cfg->dev = &fh->gdev->pdev->dev;
		dma_cfg->fd = gs->y_base_fd;

		ret = meson_gdc_dma_map(dma_cfg);
		if (ret != 0) {
			gdc_log(LOG_ERR, "Failed to get map dma buff");
			return ret;
		}

		gdc_cmd->y_base_addr = virt_to_phys(dma_cfg->vaddr);

		dma_cfg = &fh->uv_dma_cfg;
		memset(dma_cfg, 0, sizeof(*dma_cfg));
		dma_cfg->dir = DMA_TO_DEVICE;
		dma_cfg->dev = &fh->gdev->pdev->dev;
		dma_cfg->fd = gs->uv_base_fd;

		ret = meson_gdc_dma_map(dma_cfg);
		if (ret != 0) {
			gdc_log(LOG_ERR, "Failed to get map dma buff");
			return ret;
		}

		gdc_cmd->uv_base_addr = virt_to_phys(dma_cfg->vaddr);
	break;
	case Y_GREY:
		dma_cfg = &fh->y_dma_cfg;
		memset(dma_cfg, 0, sizeof(*dma_cfg));
		dma_cfg->dir = DMA_TO_DEVICE;
		dma_cfg->dev = &fh->gdev->pdev->dev;
		dma_cfg->fd = gs->y_base_fd;

		ret = meson_gdc_dma_map(dma_cfg);
		if (ret != 0) {
			gdc_log(LOG_ERR, "Failed to get map dma buff");
			return ret;
		}
		gdc_cmd->y_base_addr = virt_to_phys(dma_cfg->vaddr);
		gdc_cmd->uv_base_addr = 0;
	break;
	default:
		gdc_log(LOG_ERR, "Error image format");
	break;
	}

	return ret;
}

static void meson_gdc_deinit_dma_addr(struct mgdc_fh_s *fh)
{
	struct gdc_dma_cfg *dma_cfg = NULL;
	struct gdc_cmd_s *gdc_cmd = &fh->gdc_cmd;
	struct gdc_config_s *gc = &gdc_cmd->gdc_config;

	if (fh == NULL) {
		gdc_log(LOG_ERR, "Error input param\n");
		return;
	}

	switch (gc->format) {
	case NV12:
		dma_cfg = &fh->y_dma_cfg;
		meson_gdc_dma_unmap(dma_cfg);

		dma_cfg = &fh->uv_dma_cfg;
		meson_gdc_dma_unmap(dma_cfg);
	break;
	case Y_GREY:
		dma_cfg = &fh->y_dma_cfg;
		meson_gdc_dma_unmap(dma_cfg);
	break;
	default:
		gdc_log(LOG_ERR, "Error image format");
	break;
	}
}

static int gdc_buffer_alloc(struct gdc_dmabuf_req_s *gdc_req_buf)
{
	struct device *dev;

	dev = &(gdc_manager.gdc_dev->pdev->dev);
	return gdc_dma_buffer_alloc(gdc_manager.buffer,
		dev, gdc_req_buf);
}

static int gdc_buffer_export(struct gdc_dmabuf_exp_s *gdc_exp_buf)
{
	return gdc_dma_buffer_export(gdc_manager.buffer, gdc_exp_buf);
}

static int gdc_buffer_free(int index)
{
	return gdc_dma_buffer_free(gdc_manager.buffer, index);

}

static void gdc_buffer_dma_flush(int dma_fd)
{
	struct device *dev;

	dev = &(gdc_manager.gdc_dev->pdev->dev);
	gdc_dma_buffer_dma_flush(dev, dma_fd);
}

static void gdc_buffer_cache_flush(int dma_fd)
{
	struct device *dev;

	dev = &(gdc_manager.gdc_dev->pdev->dev);
	gdc_dma_buffer_cache_flush(dev, dma_fd);
}

static long gdc_process_input_dma_info(struct mgdc_fh_s *fh,
	struct gdc_settings_ex *gs_ex)
{
	long ret = -1;
	unsigned long addr;
	struct aml_dma_cfg cfg;
	struct gdc_cmd_s *gdc_cmd = &fh->gdc_cmd;
	struct gdc_config_s *gc = &gdc_cmd->gdc_config;

	if (fh == NULL || gs_ex == NULL) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	switch (gc->format) {
	case NV12:
		if (gs_ex->input_buffer.plane_number == 1) {
			cfg.fd = gs_ex->input_buffer.y_base_fd;
			cfg.dev = &fh->gdev->pdev->dev;
			cfg.dir = DMA_TO_DEVICE;
			ret = gdc_dma_buffer_get_phys(&cfg, &addr);
			if (ret < 0) {
				gdc_log(LOG_ERR,
					"dma import input fd %d failed\n",
					gs_ex->input_buffer.y_base_fd);
				return -EINVAL;
			}
			ret = meson_gdc_set_input_addr(addr, gdc_cmd);
			if (ret != 0) {
				gdc_log(LOG_ERR, "set input addr failed\n");
				return -EINVAL;
			}
			gdc_log(LOG_INFO, "1 plane get input addr=%x\n",
				gdc_cmd->y_base_addr);
		} else if (gs_ex->input_buffer.plane_number == 2) {
			cfg.fd = gs_ex->input_buffer.y_base_fd;
			cfg.dev = &fh->gdev->pdev->dev;
			cfg.dir = DMA_TO_DEVICE;
			ret = gdc_dma_buffer_get_phys(&cfg, &addr);
			if (ret < 0) {
				gdc_log(LOG_ERR,
					"dma import input fd %d failed\n",
					gs_ex->input_buffer.y_base_fd);
				return -EINVAL;
			}
			gdc_cmd->y_base_addr = addr;

			cfg.fd = gs_ex->input_buffer.uv_base_fd;
			cfg.dev = &fh->gdev->pdev->dev;
			cfg.dir = DMA_TO_DEVICE;
			ret = gdc_dma_buffer_get_phys(&cfg, &addr);
			if (ret < 0) {
				gdc_log(LOG_ERR,
					"dma import input fd %d failed\n",
					gs_ex->input_buffer.uv_base_fd);
				return -EINVAL;
			}
			gdc_cmd->uv_base_addr = addr;
			gdc_log(LOG_INFO, "2 plane get input addr=%x\n",
				gdc_cmd->y_base_addr);
			gdc_log(LOG_INFO, "2 plane get input addr=%x\n",
				gdc_cmd->uv_base_addr);
		}
	break;
	case Y_GREY:
		cfg.fd = gs_ex->input_buffer.y_base_fd;
		cfg.dev = &(fh->gdev->pdev->dev);
		cfg.dir = DMA_TO_DEVICE;
		ret = gdc_dma_buffer_get_phys(&cfg, &addr);
		if (ret < 0) {
			gdc_log(LOG_ERR,
				"dma import input fd %d failed\n",
				gs_ex->input_buffer.shared_fd);
			return -EINVAL;
		}
		gdc_cmd->y_base_addr = addr;
		gdc_cmd->uv_base_addr = 0;
	break;
	default:
		gdc_log(LOG_ERR, "Error image format");
	break;
	}
	return 0;
}

static long gdc_process_ex_info(struct mgdc_fh_s *fh,
	struct gdc_settings_ex *gs_ex)
{
	long ret;
	unsigned long addr = 0;
	size_t len;
	struct aml_dma_cfg cfg;
	struct gdc_cmd_s *gdc_cmd = &fh->gdc_cmd;

	if (fh == NULL || gs_ex == NULL) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}
	memcpy(&(gdc_cmd->gdc_config), &(gs_ex->gdc_config),
		sizeof(struct gdc_config_s));
	gdc_cmd->fh = fh;
	if (gs_ex->output_buffer.mem_alloc_type == AML_GDC_MEM_ION) {
		/* ion alloc */
		ret = meson_ion_share_fd_to_phys(fh->ion_client,
			gs_ex->output_buffer.shared_fd,
			(ion_phys_addr_t *)&addr, &len);
		if (ret < 0) {
			gdc_log(LOG_ERR, "ion import out fd %d failed\n",
				gs_ex->output_buffer.shared_fd);
			return -EINVAL;
		}
	} else if (gs_ex->output_buffer.mem_alloc_type == AML_GDC_MEM_DMABUF) {
		/* dma alloc */
		cfg.fd = gs_ex->output_buffer.y_base_fd;
		cfg.dev = &(gdc_manager.gdc_dev->pdev->dev);
		cfg.dir = DMA_FROM_DEVICE;
		ret = gdc_dma_buffer_get_phys(&cfg, &addr);
		if (ret < 0) {
			gdc_log(LOG_ERR, "dma import out fd %d failed\n",
				gs_ex->output_buffer.y_base_fd);
			return -EINVAL;
		}

	}
	gdc_log(LOG_INFO, "%s, output addr=%lx\n", __func__, addr);
	gdc_cmd->base_gdc = 0;
	gdc_cmd->buffer_addr = addr;
	gdc_cmd->current_addr = gdc_cmd->buffer_addr;
	if (gs_ex->config_buffer.mem_alloc_type == AML_GDC_MEM_ION) {
		/* ion alloc */
		ret = meson_ion_share_fd_to_phys(fh->ion_client,
			gs_ex->config_buffer.shared_fd,
			(ion_phys_addr_t *)&addr, &len);
		if (ret < 0) {
			gdc_log(LOG_ERR, "ion import config fd %d failed\n",
				gs_ex->config_buffer.shared_fd);
			return -EINVAL;
		}
	} else if (gs_ex->config_buffer.mem_alloc_type == AML_GDC_MEM_DMABUF) {
		/* dma alloc */
		cfg.fd = gs_ex->config_buffer.y_base_fd;
		cfg.dev = &(gdc_manager.gdc_dev->pdev->dev);
		cfg.dir = DMA_TO_DEVICE;
		ret = gdc_dma_buffer_get_phys(&cfg, &addr);
		if (ret < 0) {
			gdc_log(LOG_ERR, "dma import config fd %d failed\n",
				gs_ex->config_buffer.shared_fd);
			return -EINVAL;
		}
	}
	gdc_cmd->gdc_config.config_addr = addr;
	gdc_log(LOG_INFO, "%s, config addr=%lx\n", __func__, addr);

	if (gs_ex->input_buffer.mem_alloc_type == AML_GDC_MEM_ION) {
		/* ion alloc */
		ret = meson_ion_share_fd_to_phys(fh->ion_client,
			gs_ex->input_buffer.shared_fd,
			(ion_phys_addr_t *)&addr, &len);
		if (ret < 0) {
			gdc_log(LOG_ERR, "ion import input fd %d failed\n",
				gs_ex->input_buffer.shared_fd);
			return -EINVAL;
		}
		ret = meson_gdc_set_input_addr(addr, gdc_cmd);
		if (ret != 0) {
			gdc_log(LOG_ERR, "set input addr failed\n");
			return -EINVAL;
		}
	} else if (gs_ex->input_buffer.mem_alloc_type == AML_GDC_MEM_DMABUF) {
		/* dma alloc */
		gdc_process_input_dma_info(fh, gs_ex);
	}
	gdc_log(LOG_INFO, "%s, input addr=%x\n",
		__func__, fh->gdc_cmd.y_base_addr);
	mutex_lock(&fh->gdev->d_mutext);
	#if 1
	if (gs_ex->config_buffer.mem_alloc_type == AML_GDC_MEM_DMABUF)
		gdc_buffer_dma_flush(gs_ex->config_buffer.shared_fd);
	#endif
	ret = gdc_run(gdc_cmd);
	if (ret < 0)
		gdc_log(LOG_ERR, "gdc process failed ret = %ld\n", ret);

	ret = wait_for_completion_timeout(&fh->gdev->d_com,
					msecs_to_jiffies(40));
	if (ret == 0)
		gdc_log(LOG_ERR, "gdc timeout\n");

	gdc_stop(gdc_cmd);
	#if 0
	if (gs_ex->output_buffer.mem_alloc_type == AML_GDC_MEM_DMABUF)
		gdc_buffer_cache_flush(gs_ex->output_buffer.shared_fd);
	#endif
	mutex_unlock(&fh->gdev->d_mutext);
	return 0;
}

static long meson_gdc_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	long ret = -1;
	size_t len;
	struct mgdc_fh_s *fh = file->private_data;
	struct gdc_settings gs;
	struct gdc_cmd_s *gdc_cmd = &fh->gdc_cmd;
	struct gdc_config_s *gc = &gdc_cmd->gdc_config;
	struct gdc_buf_cfg buf_cfg;
	struct page *cma_pages = NULL;
	struct gdc_settings_ex gs_ex;
	struct gdc_dmabuf_req_s gdc_req_buf;
	struct gdc_dmabuf_exp_s gdc_exp_buf;
	ion_phys_addr_t addr;
	int index, dma_fd;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case GDC_PROCESS:
		ret = copy_from_user(&gs, argp, sizeof(gs));
		if (ret < 0) {
			gdc_log(LOG_ERR, "copy from user failed\n");
			return -EINVAL;
		}

		gdc_log(LOG_DEBUG, "sizeof(gs)=%zu, magic=%d\n",
				sizeof(gs), gs.magic);

		//configure gdc config, buffer address and resolution
		ret = meson_ion_share_fd_to_phys(fh->ion_client,
				gs.out_fd, &addr, &len);
		if (ret < 0) {
			gdc_log(LOG_ERR,
				"import out fd %d failed\n", gs.out_fd);
			return -EINVAL;
		}
		memcpy(&gdc_cmd->gdc_config, &gs.gdc_config,
			sizeof(struct gdc_config_s));
		gdc_cmd->buffer_addr = addr;
		gdc_cmd->buffer_size = len;

		gdc_cmd->base_gdc = 0;
		gdc_cmd->current_addr = gdc_cmd->buffer_addr;

		ret = meson_ion_share_fd_to_phys(fh->ion_client,
				gc->config_addr, &addr, &len);
		if (ret < 0) {
			gdc_log(LOG_ERR, "import config fd failed\n");
			return -EINVAL;
		}

		gc->config_addr = addr;

		ret = meson_ion_share_fd_to_phys(fh->ion_client,
				gs.in_fd, &addr, &len);
		if (ret < 0) {
			gdc_log(LOG_ERR, "import in fd %d failed\n", gs.in_fd);
			return -EINVAL;
		}

		ret = meson_gdc_set_input_addr(addr, gdc_cmd);
		if (ret != 0) {
			gdc_log(LOG_ERR, "set input addr failed\n");
			return -EINVAL;
		}

		gdc_cmd->fh = fh;
		mutex_lock(&fh->gdev->d_mutext);
		ret = gdc_run(gdc_cmd);
		if (ret < 0)
			gdc_log(LOG_ERR, "gdc process ret = %ld\n", ret);

		ret = wait_for_completion_timeout(&fh->gdev->d_com,
						msecs_to_jiffies(40));
		if (ret == 0)
			gdc_log(LOG_ERR, "gdc timeout\n");

		gdc_stop(gdc_cmd);
		mutex_unlock(&fh->gdev->d_mutext);
	break;
	case GDC_RUN:
		ret = copy_from_user(&gs, argp, sizeof(gs));
		if (ret < 0)
			gdc_log(LOG_ERR, "copy from user failed\n");

		memcpy(&gdc_cmd->gdc_config, &gs.gdc_config,
			sizeof(struct gdc_config_s));
		gdc_cmd->buffer_addr = fh->o_paddr;
		gdc_cmd->buffer_size = fh->o_len;

		gdc_cmd->base_gdc = 0;
		gdc_cmd->current_addr = gdc_cmd->buffer_addr;

		gc->config_addr = fh->c_paddr;

		ret = meson_gdc_set_input_addr(fh->i_paddr, gdc_cmd);
		if (ret != 0) {
			gdc_log(LOG_ERR, "set input addr failed\n");
			return -EINVAL;
		}

		gdc_cmd->fh = fh;
		mutex_lock(&fh->gdev->d_mutext);
		meson_gdc_dma_flush(&fh->gdev->pdev->dev,
					fh->i_paddr, fh->i_len);
		meson_gdc_dma_flush(&fh->gdev->pdev->dev,
					fh->c_paddr, fh->c_len);
		ret = gdc_run(gdc_cmd);
		if (ret < 0)
			gdc_log(LOG_ERR, "gdc process failed ret = %ld\n", ret);

		ret = wait_for_completion_timeout(&fh->gdev->d_com,
						msecs_to_jiffies(40));
		if (ret == 0)
			gdc_log(LOG_ERR, "gdc timeout\n");

		gdc_stop(gdc_cmd);
		meson_gdc_cache_flush(&fh->gdev->pdev->dev,
					fh->o_paddr, fh->o_len);
		mutex_unlock(&fh->gdev->d_mutext);
	break;
	case GDC_HANDLE:
		ret = copy_from_user(&gs, argp, sizeof(gs));
		if (ret < 0)
			gdc_log(LOG_ERR, "copy from user failed\n");

		memcpy(&gdc_cmd->gdc_config, &gs.gdc_config,
			sizeof(struct gdc_config_s));
		gdc_cmd->buffer_addr = fh->o_paddr;
		gdc_cmd->buffer_size = fh->o_len;

		gdc_cmd->base_gdc = 0;
		gdc_cmd->current_addr = gdc_cmd->buffer_addr;

		gc->config_addr = fh->c_paddr;

		gdc_cmd->fh = fh;
		mutex_lock(&fh->gdev->d_mutext);
		ret = meson_gdc_init_dma_addr(fh, &gs);
		if (ret != 0) {
			mutex_unlock(&fh->gdev->d_mutext);
			gdc_log(LOG_ERR, "Failed to init dma addr");
			return ret;
		}
		meson_gdc_dma_flush(&fh->gdev->pdev->dev,
					fh->c_paddr, fh->c_len);
		ret = gdc_run(gdc_cmd);
		if (ret < 0)
			gdc_log(LOG_ERR, "gdc process failed ret = %ld\n", ret);

		ret = wait_for_completion_timeout(&fh->gdev->d_com,
						msecs_to_jiffies(40));
		if (ret == 0)
			gdc_log(LOG_ERR, "gdc timeout\n");

		gdc_stop(gdc_cmd);
		meson_gdc_cache_flush(&fh->gdev->pdev->dev,
					fh->o_paddr, fh->o_len);
		meson_gdc_deinit_dma_addr(fh);
		mutex_unlock(&fh->gdev->d_mutext);
	break;
	case GDC_REQUEST_BUFF:
		ret = copy_from_user(&buf_cfg, argp, sizeof(buf_cfg));
		if (ret < 0 || buf_cfg.type >= GDC_BUFF_TYPE_MAX) {
			gdc_log(LOG_ERR, "Error user param\n");
			return ret;
		}

		buf_cfg.len = PAGE_ALIGN(buf_cfg.len);

		cma_pages = dma_alloc_from_contiguous(&fh->gdev->pdev->dev,
						buf_cfg.len >> PAGE_SHIFT, 0);
		if (cma_pages != NULL) {
			fh->mmap_type = buf_cfg.type;
			ret = meson_gdc_set_buff(fh, cma_pages, buf_cfg.len);
			if (ret != 0) {
				dma_release_from_contiguous(
						&fh->gdev->pdev->dev,
						cma_pages,
						buf_cfg.len >> PAGE_SHIFT);
				gdc_log(LOG_ERR, "Failed to set buff\n");
				return ret;
			}
		} else {
			gdc_log(LOG_ERR, "Failed to alloc dma buff\n");
			return -ENOMEM;
		}

	break;
	case GDC_PROCESS_EX:
		ret = copy_from_user(&gs_ex, argp, sizeof(gs_ex));
		if (ret < 0)
			gdc_log(LOG_ERR, "copy from user failed\n");
		memcpy(&gdc_cmd->gdc_config, &gs_ex.gdc_config,
			sizeof(struct gdc_config_s));
		gdc_process_ex_info(fh, &gs_ex);
		break;
	case GDC_REQUEST_DMA_BUFF:
		ret = copy_from_user(&gdc_req_buf, argp,
			sizeof(struct gdc_dmabuf_req_s));
		if (ret < 0) {
			pr_err("Error user param\n");
			return -EINVAL;
		}
		ret = gdc_buffer_alloc(&gdc_req_buf);
		if (ret == 0)
			ret = copy_to_user(argp, &gdc_req_buf,
				sizeof(struct gdc_dmabuf_req_s));
		break;
	case GDC_EXP_DMA_BUFF:
		ret = copy_from_user(&gdc_exp_buf, argp,
			sizeof(struct gdc_dmabuf_exp_s));
		if (ret < 0) {
			pr_err("Error user param\n");
			return -EINVAL;
		}
		ret = gdc_buffer_export(&gdc_exp_buf);
		if (ret == 0)
			ret = copy_to_user(argp, &gdc_exp_buf,
				sizeof(struct gdc_dmabuf_exp_s));
		break;
	case GDC_FREE_DMA_BUFF:
		ret = copy_from_user(&index, argp,
			sizeof(int));
		if (ret < 0) {
			pr_err("Error user param\n");
			return -EINVAL;
		}
		ret = gdc_buffer_free(index);
		break;
	case GDC_SYNC_DEVICE:
		ret = copy_from_user(&dma_fd, argp,
			sizeof(int));
		if (ret < 0) {
			pr_err("Error user param\n");
			return -EINVAL;
		}
		gdc_buffer_dma_flush(dma_fd);
		break;
	case GDC_SYNC_CPU:
		ret = copy_from_user(&dma_fd, argp,
			sizeof(int));
		if (ret < 0) {
			pr_err("Error user param\n");
			return -EINVAL;
		}
		gdc_buffer_cache_flush(dma_fd);
		break;
	default:
		gdc_log(LOG_ERR, "unsupported cmd 0x%x\n", cmd);
		return -EINVAL;
	break;
	}

	return ret;
}

static int meson_gdc_mmap(struct file *file_p,
				struct vm_area_struct *vma)
{
	int ret = -1;
	unsigned long buf_len = 0;
	struct mgdc_fh_s *fh = file_p->private_data;

	buf_len = vma->vm_end - vma->vm_start;

	switch (fh->mmap_type) {
	case INPUT_BUFF_TYPE:
		ret = remap_pfn_range(vma, vma->vm_start,
			fh->i_paddr >> PAGE_SHIFT,
			buf_len, vma->vm_page_prot);
		if (ret != 0)
			gdc_log(LOG_ERR, "Failed to mmap input buffer\n");
	break;
	case OUTPUT_BUFF_TYPE:
		ret = remap_pfn_range(vma, vma->vm_start,
			fh->o_paddr >> PAGE_SHIFT,
			buf_len, vma->vm_page_prot);
		if (ret != 0)
			gdc_log(LOG_ERR, "Failed to mmap input buffer\n");

	break;
	case CONFIG_BUFF_TYPE:
		ret = remap_pfn_range(vma, vma->vm_start,
			fh->c_paddr >> PAGE_SHIFT,
			buf_len, vma->vm_page_prot);
		if (ret != 0)
			gdc_log(LOG_ERR, "Failed to mmap input buffer\n");
	break;
	default:
		gdc_log(LOG_ERR, "Error mmap type:0x%x\n", fh->mmap_type);
	break;
	}

	return ret;
}

static const struct file_operations meson_gdc_fops = {
	.owner = THIS_MODULE,
	.open = meson_gdc_open,
	.release = meson_gdc_release,
	.unlocked_ioctl = meson_gdc_ioctl,
	.compat_ioctl = meson_gdc_ioctl,
	.mmap = meson_gdc_mmap,
};

static struct miscdevice meson_gdc_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "gdc",
	.fops	= &meson_gdc_fops,
};

static ssize_t gdc_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i;

	len += sprintf(buf+len, "gdc adapter register below\n");
	for (i = 0; i <= 0xff; i += 4) {
		len += sprintf(buf+len, "\t[0xff950000 + 0x%08x, 0x%-8x\n",
						i, system_gdc_read_32(i));
	}

	return len;
}

static ssize_t gdc_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	gdc_log(LOG_DEBUG, "%s, %d\n", __func__, __LINE__);
	return len;
}
static DEVICE_ATTR(gdc_reg, 0554, gdc_reg_show, gdc_reg_store);

static ssize_t firmware1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	gdc_log(LOG_DEBUG, "%s, %d\n", __func__, __LINE__);
	return 1;
}

static ssize_t firmware1_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	gdc_log(LOG_DEBUG, "%s, %d\n", __func__, __LINE__);
	//gdc_fw_init();
	return 1;
}
static DEVICE_ATTR(firmware1, 0664, firmware1_show, firmware1_store);

static ssize_t loglevel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf+len, "%d\n", gdc_log_level);
	return len;
}

static ssize_t loglevel_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("log_level: %d->%d\n", gdc_log_level, res);
	gdc_log_level = res;

	return len;
}

static DEVICE_ATTR(loglevel, 0664, loglevel_show, loglevel_store);

irqreturn_t gdc_interrupt_handler(int irq, void *param)
{
	struct meson_gdc_dev_t *gdc_dev = param;

	complete(&gdc_dev->d_com);

	return IRQ_HANDLED;
}

static int gdc_platform_probe(struct platform_device *pdev)
{
	int rc = -1;
	struct resource *gdc_res;
	struct meson_gdc_dev_t *gdc_dev = NULL;
	void *clk_cntl = NULL;
	void *pd_cntl = NULL;
	uint32_t reg_value = 0;

	// Initialize irq
	gdc_res = platform_get_resource(pdev,
			IORESOURCE_MEM, 0);
	if (!gdc_res) {
		gdc_log(LOG_ERR, "Error, no IORESOURCE_MEM DT!\n");
		return -ENOMEM;
	}

	if (init_gdc_io(pdev->dev.of_node) != 0) {
		gdc_log(LOG_ERR, "Error on mapping gdc memory!\n");
		return -ENOMEM;
	}

	of_reserved_mem_device_init(&(pdev->dev));

	gdc_dev = devm_kzalloc(&pdev->dev, sizeof(*gdc_dev),
			GFP_KERNEL);

	if (gdc_dev == NULL) {
		gdc_log(LOG_DEBUG, "devm alloc gdc dev failed\n");
		return -ENOMEM;
	}

	gdc_dev->pdev = pdev;
	gdc_dev->misc_dev.minor = meson_gdc_dev.minor;
	gdc_dev->misc_dev.name = meson_gdc_dev.name;
	gdc_dev->misc_dev.fops = meson_gdc_dev.fops;
	spin_lock_init(&gdc_dev->slock);

	gdc_dev->irq = platform_get_irq(pdev, 0);
	if (gdc_dev->irq < 0) {
		gdc_log(LOG_DEBUG, "cannot find irq for gdc\n");
		return -EINVAL;
	}

#if 0
	gdc_dev->clk_core = devm_clk_get(&pdev->dev, "core");
	rc = clk_set_rate(gdc_dev->clk_core, 800000000);

	gdc_dev->clk_axi = devm_clk_get(&pdev->dev, "axi");
	rc = clk_set_rate(gdc_dev->clk_axi, 800000000);
#else
	clk_cntl = of_iomap(pdev->dev.of_node, 1);
	iowrite32((3<<25)|(1<<24)|(0<<16)|(3<<9)|(1<<8)|(0<<0), clk_cntl);
	pd_cntl = of_iomap(pdev->dev.of_node, 2);
	reg_value = ioread32(pd_cntl);
	gdc_log(LOG_DEBUG, "pd_cntl=%x\n", reg_value);
	reg_value = reg_value & (~(3<<18));
	gdc_log(LOG_DEBUG, "pd_cntl=%x\n", reg_value);
	iowrite32(reg_value, pd_cntl);
#endif

	mutex_init(&gdc_dev->d_mutext);
	init_completion(&gdc_dev->d_com);

	rc = devm_request_irq(&pdev->dev, gdc_dev->irq,
						gdc_interrupt_handler,
						IRQF_SHARED, "gdc", gdc_dev);
	if (rc != 0)
		gdc_log(LOG_ERR, "cannot create irq func gdc\n");

	gdc_manager.buffer = gdc_dma_buffer_create();
	gdc_manager.gdc_dev = gdc_dev;
	rc = misc_register(&gdc_dev->misc_dev);
	if (rc < 0) {
		dev_err(&pdev->dev,
			"misc_register() for minor %d failed\n",
			gdc_dev->misc_dev.minor);
	}
	device_create_file(gdc_dev->misc_dev.this_device,
		&dev_attr_gdc_reg);
	device_create_file(gdc_dev->misc_dev.this_device,
		&dev_attr_firmware1);
	device_create_file(gdc_dev->misc_dev.this_device,
		&dev_attr_loglevel);

	platform_set_drvdata(pdev, gdc_dev);
	return rc;
}

static int gdc_platform_remove(struct platform_device *pdev)
{

	device_remove_file(meson_gdc_dev.this_device,
		&dev_attr_gdc_reg);
	device_remove_file(meson_gdc_dev.this_device,
		&dev_attr_firmware1);
	device_remove_file(meson_gdc_dev.this_device,
		&dev_attr_loglevel);

	gdc_dma_buffer_destroy(gdc_manager.buffer);
	gdc_manager.gdc_dev = NULL;

	misc_deregister(&meson_gdc_dev);
	return 0;
}

static struct platform_driver gdc_platform_driver = {
	.driver = {
		.name = "gdc",
		.owner = THIS_MODULE,
		.of_match_table = gdc_dt_match,
	},
	.probe	= gdc_platform_probe,
	.remove	= gdc_platform_remove,
};

module_platform_driver(gdc_platform_driver);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Amlogic Multimedia");
