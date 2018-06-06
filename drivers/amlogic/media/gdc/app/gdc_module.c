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

#include <linux/of_address.h>
#include <api/gdc_api.h>
#include "system_log.h"

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
//gdc configuration sequence
#include "gdc_config.h"

struct meson_gdc_dev_t *g_gdc_dev;

static const struct of_device_id gdc_dt_match[] = {
	{.compatible = "amlogic, g12b-gdc"},
	{} };

MODULE_DEVICE_TABLE(of, gdc_dt_match);

//////
static int meson_gdc_open(struct inode *inode, struct file *file)
{
	struct meson_gdc_dev_t *gdc_dev = g_gdc_dev;
	struct platform_device *pdev = gdc_dev->pdev;
	struct mgdc_fh_s *fh = NULL;
	char ion_client_name[32];
	int rc = 0;

	fh = devm_kzalloc(&pdev->dev, sizeof(*fh), GFP_KERNEL);
	if (fh == NULL) {
		LOG(LOG_DEBUG, "devm alloc failed\n");
		return -ENOMEM;
	}

	get_task_comm(fh->task_comm, current);
	LOG(LOG_DEBUG, "%s, %d, call from %s\n",
			__func__, __LINE__, fh->task_comm);

	file->private_data = fh;
	snprintf(ion_client_name, sizeof(fh->task_comm),
			"gdc-%s", fh->task_comm);
	if (!fh->ion_client)
		fh->ion_client = meson_ion_client_create(-1, ion_client_name);

	fh->gdev = gdc_dev;
	init_waitqueue_head(&fh->irq_queue);

	rc = devm_request_irq(&pdev->dev, gdc_dev->irq, interrupt_handler_next,
			IRQF_SHARED, "gdc", &fh->gs);
	if (rc)
		LOG(LOG_DEBUG, "cannot create irq func gdc\n");

	return rc;
}

static int meson_gdc_release(struct inode *inode, struct file *file)
{
	struct mgdc_fh_s *fh = file->private_data;
	struct meson_gdc_dev_t *gdc_dev = fh->gdev;
	struct platform_device *pdev = gdc_dev->pdev;

	LOG(LOG_DEBUG, "%s, %d\n", __func__, __LINE__);
	if (fh->ion_client) {
		ion_client_destroy(fh->ion_client);
		fh->ion_client = NULL;
	}

	devm_free_irq(&pdev->dev,  gdc_dev->irq, &fh->gs);

	return 0;
}

static long meson_gdc_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	size_t len;
	struct mgdc_fh_s *fh = file->private_data;
	struct gdc_settings  *gs = &fh->gs;
	struct gdc_config *gc = &gs->gdc_config;
	long ret = 0;
	ion_phys_addr_t addr;
	void __user *argp = (void __user *)arg;

	gs->fh = fh;

	switch (cmd) {
	case GDC_PROCESS:
		ret = copy_from_user(gs, argp, sizeof(*gs));
		if (ret < 0)
			LOG(LOG_DEBUG, "copy from user failed\n");

		LOG(LOG_DEBUG, "sizeof(gs)=%ld, magic=%d\n",
				sizeof(*gs), gs->magic);

		//configure gdc config, buffer address and resolution
		ret = meson_ion_share_fd_to_phys(fh->ion_client,
				gs->out_fd, &addr, &len);

		gs->buffer_addr = addr;
		gs->buffer_size = len;

		if (ret < 0)
			LOG(LOG_DEBUG, "import out fd %d failed\n", gs->out_fd);
		gs->base_gdc = 0;
		gs->current_addr = gs->buffer_addr;

		ret = meson_ion_share_fd_to_phys(fh->ion_client,
				gc->config_addr, &addr, &len);

		gc->config_addr = addr;

		ret = meson_ion_share_fd_to_phys(fh->ion_client,
				gs->in_fd, &addr, &len);
		if (gc->format == NV12) {
			gs->y_base_addr = addr;
			gs->uv_base_addr = addr +
				gc->input_y_stride * gc->input_height;
		} else if (gc->format == YV12) {
			gs->y_base_addr = addr;
			gs->u_base_addr = addr
				+ gc->input_y_stride * gc->input_height;

			gs->v_base_addr = gs->u_base_addr +
				gc->input_c_stride * gc->input_height / 2;
		}

		if (ret < 0)
			LOG(LOG_DEBUG, "import in fd %d failed\n", gs->in_fd);

		gs->fh = fh;
		ret = gdc_run(gs);
		if (ret < 0)
			LOG(LOG_DEBUG, "gdc process ret = %ld\n", ret);

		ret = wait_event_interruptible_timeout(fh->irq_queue,
				(gdc_busy_read() == 0),
				msecs_to_jiffies(30));
		break;

	default:
		pr_info("unsupported cmd 0x%x\n", cmd);
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
	LOG(LOG_DEBUG, "%s, %d\n", __func__, __LINE__);
	return len;
}
static DEVICE_ATTR(gdc_reg, 0554, gdc_reg_show, gdc_reg_store);

static ssize_t firmware1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	LOG(LOG_DEBUG, "%s, %d\n", __func__, __LINE__);
	return 1;
}

static ssize_t firmware1_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	LOG(LOG_DEBUG, "%s, %d\n", __func__, __LINE__);
	//gdc_fw_init();
	return 1;
}
static DEVICE_ATTR(firmware1, 0664, firmware1_show, firmware1_store);


static int gdc_platform_probe(struct platform_device *pdev)
{
	struct resource *gdc_res;
	struct meson_gdc_dev_t *gdc_dev = NULL;
	void *clk_cntl = NULL;
	void *pd_cntl = NULL;
	uint32_t reg_value = 0;

	// Initialize irq
	gdc_res = platform_get_resource(pdev,
			IORESOURCE_MEM, 0);
	if (!gdc_res) {
		LOG(LOG_ERR, "Error, no IORESOURCE_MEM DT!\n");
		return -ENOMEM;
	}

	if (init_gdc_io(pdev->dev.of_node) != 0) {
		LOG(LOG_ERR, "Error on mapping gdc memory!\n");
		return -ENOMEM;
	}

	device_create_file(&pdev->dev, &dev_attr_gdc_reg);
	device_create_file(&pdev->dev, &dev_attr_firmware1);

	gdc_dev = devm_kzalloc(&pdev->dev, sizeof(*gdc_dev),
			GFP_KERNEL);

	if (gdc_dev == NULL) {
		LOG(LOG_DEBUG, "devm alloc gdc dev failed\n");
		return -ENOMEM;
	}

	gdc_dev->pdev = pdev;
	spin_lock_init(&gdc_dev->slock);

	gdc_dev->irq = platform_get_irq(pdev, 0);
	if (gdc_dev->irq < 0) {
		LOG(LOG_DEBUG, "cannot find irq for gdc\n");
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
	LOG(LOG_DEBUG, "pd_cntl=%x\n", reg_value);
	reg_value = reg_value & (~(3<<18));
	LOG(LOG_DEBUG, "pd_cntl=%x\n", reg_value);
	iowrite32(reg_value, pd_cntl);
#endif

	g_gdc_dev = gdc_dev;

	return misc_register(&meson_gdc_dev);
}

static int gdc_platform_remove(struct platform_device *pdev)
{

	device_remove_file(&pdev->dev, &dev_attr_gdc_reg);
	device_remove_file(&pdev->dev, &dev_attr_firmware1);

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
