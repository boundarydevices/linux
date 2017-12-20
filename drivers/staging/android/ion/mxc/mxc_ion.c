/*
 * drivers/gpu/mxc/mxc_ion.c
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2012-2016 Freescale Semiconductor, Inc.
 * Copyright 2017 NXP.
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

#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/uaccess.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/dma-buf.h>

#include "../ion_priv.h"
#include "../ion.h"
#include "../ion_of.h"
#include "../../uapi/mxc_ion.h"

struct fsl_ion_dev {
	struct ion_heap **heaps;
	struct ion_device *idev;
	struct ion_platform_data *data;
};

static struct ion_of_heap fsl_heaps[] = {
	PLATFORM_HEAP("fsl,cma", 0, ION_HEAP_TYPE_DMA, "cma"),
	PLATFORM_HEAP("fsl,sys-contig", 1,
			ION_HEAP_TYPE_SYSTEM_CONTIG, "sys-contig"),
	PLATFORM_HEAP("fsl,system", 2,
			ION_HEAP_TYPE_SYSTEM, "system"),
    {}
};

static long
mxc_custom_ioctl(struct ion_client *client, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case ION_IOC_PHYS:
		{
			struct ion_handle *handle;
			struct ion_phys_data data;
			struct device *dev;
			void *vaddr;

			if (copy_from_user(&data, (void __user *) arg,
					   sizeof(struct ion_phys_data)))
				return -EFAULT;
			handle = ion_handle_get_by_id_wrap(client, data.handle);
			if (IS_ERR(handle))
				return PTR_ERR(handle);

			vaddr = ion_map_kernel(client, handle);
			ion_handle_put_wrap(handle);

			if (IS_ERR(vaddr))
				return PTR_ERR(vaddr);

			dev = ion_device_get_by_client(client);
			data.phys = virt_to_dma(dev, vaddr);

			if (copy_to_user((void __user *) arg, &data,
					 sizeof(struct ion_phys_data)))
				return -EFAULT;
			return 0;
		}
	case ION_IOC_PHYS_DMA:
		{
			int ret = 0;
			struct device *dev = NULL;
			struct dma_buf *dmabuf = NULL;
			unsigned long phys = 0;
			u64 dma_mask = DMA_BIT_MASK(32);
			size_t len = 0;
			struct ion_phys_dma_data data;
			const struct vb2_mem_ops *mem_ops =
			    &vb2_dma_contig_memops;
			dma_addr_t *addr;
			void *mem_priv;

			if (copy_from_user(&data, (void __user *) arg,
					   sizeof(struct ion_phys_dma_data)))
				return -EFAULT;

			/* Get physical address from dmafd */
			dmabuf = dma_buf_get(data.dmafd);
			if (!dmabuf)
				return -1;

			dev = ion_device_get_by_client(client);
			dev->dma_mask = &dma_mask;

			mem_priv = mem_ops->attach_dmabuf(dev,
							  dmabuf, dmabuf->size,
							  DMA_BIDIRECTIONAL);
			if (IS_ERR(mem_priv))
				goto err1;
			ret = mem_ops->map_dmabuf(mem_priv);
			if (ret)
				goto err0;

			addr = mem_ops->cookie(mem_priv);
			phys = *addr;
			len = dmabuf->size;

			data.phys = phys;
			data.size = len;
			if (copy_to_user((void __user *) arg, &data,
					 sizeof(struct ion_phys_dma_data))) {
				ret = -EFAULT;
			}

			/* unmap and detach */
			mem_ops->unmap_dmabuf(mem_priv);
err0:
			mem_ops->detach_dmabuf(mem_priv);
err1:
			dma_buf_put(dmabuf);

			if (ret < 0)
				return ret;
			return 0;
		}
	case ION_IOC_PHYS_VIRT:
		{
			struct device *dev = NULL;
			u64 dma_mask = DMA_BIT_MASK(32);
			struct ion_phys_virt_data data;
			const struct vb2_mem_ops *mem_ops =
			    &vb2_dma_contig_memops;
			dma_addr_t *addr;
			void *mem_priv;

			if (copy_from_user(&data, (void __user *) arg,
					   sizeof(struct ion_phys_virt_data)))
				return -EFAULT;

			/* Get physical address from virtual address */
			if (!data.virt)
				return -1;

			dev = ion_device_get_by_client(client);
			dev->dma_mask = &dma_mask;

			mem_priv = mem_ops->get_userptr(dev,
							data.virt, data.size,
							DMA_BIDIRECTIONAL);
			if (IS_ERR(mem_priv))
				return -1;

			addr = mem_ops->cookie(mem_priv);
			mem_ops->put_userptr(mem_priv);

			data.phys = *addr;
			if (copy_to_user((void __user *) arg, &data,
					 sizeof(struct ion_phys_virt_data)))
				return -EFAULT;
			return 0;
		}
	default:
		return -ENOTTY;
	}

	return 0;
}

int
mxc_ion_probe(struct platform_device *pdev)
{
	int i;
	struct fsl_ion_dev *fdev = NULL;

	fdev = devm_kzalloc(&pdev->dev, sizeof(*fdev), GFP_KERNEL);
	if (!fdev)
		return -ENODEV;

	platform_set_drvdata(pdev, fdev);
	fdev->idev = ion_device_create(mxc_custom_ioctl);
	if (IS_ERR(fdev->idev))
		return PTR_ERR(fdev->idev);

	fdev->data = ion_parse_dt(pdev, fsl_heaps);
	if (IS_ERR(fdev->data))
		return PTR_ERR(fdev->data);

	fdev->heaps = devm_kzalloc(&pdev->dev,
			sizeof(struct ion_heap *) * fdev->data->nr, GFP_KERNEL);
	if (!fdev->heaps) {
		ion_destroy_platform_data(fdev->data);
		return -ENOMEM;
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < fdev->data->nr; i++) {
		fdev->heaps[i] = ion_heap_create(&fdev->data->heaps[i]);
		if (!fdev->heaps[i]) {
			ion_destroy_platform_data(fdev->data);
			return -ENOMEM;
		}
		ion_device_add_heap(fdev->idev, fdev->heaps[i]);
	}

	return 0;
}

int
mxc_ion_remove(struct platform_device *pdev)
{
	struct fsl_ion_dev *fdev = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < fdev->data->nr; i++)
		ion_heap_destroy(fdev->heaps[i]);
	ion_destroy_platform_data(fdev->data);
	ion_device_destroy(fdev->idev);

	return 0;
}

static struct of_device_id ion_match_table[] = {
	{.compatible = "fsl,mxc-ion"},
	{},
};

static struct platform_driver ion_driver = {
	.probe = mxc_ion_probe,
	.remove = mxc_ion_remove,
	.driver = {
		   .name = "ion-mxc",
		   .of_match_table = ion_match_table,
		   },
};

static int __init
ion_init(void)
{
	return platform_driver_register(&ion_driver);
}

static void __exit
ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
}

module_init(ion_init);
module_exit(ion_exit);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MXC ion allocator driver");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("fb");
