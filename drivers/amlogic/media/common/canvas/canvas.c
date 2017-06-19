/*
 * drivers/amlogic/media/common/canvas/canvas.c
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/major.h>
#include <linux/io.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/old_cpu_version.h>
#include <linux/kernel.h>

#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>

#include <linux/of_address.h>
#include "canvas_reg.h"
#include "canvas_priv.h"

#define DRIVER_NAME "amlogic-canvas"
#define MODULE_NAME "amlogic-canvas"
#define DEVICE_NAME "amcanvas"
#define CLASS_NAME  "amcanvas-class"

#define pr_dbg(fmt, args...) pr_info("Canvas: " fmt, ## args)
#define pr_error(fmt, args...) pr_err("Canvas: " fmt, ## args)

#define canvas_io_read(addr) readl(addr)
#define canvas_io_write(addr, val) writel((val), addr)

struct canvas_device_info {
	const char *device_name;
	spinlock_t lock;
	struct resource res;
	unsigned char __iomem *reg_base;
	struct canvas_s canvasPool[CANVAS_MAX_NUM];
	int max_canvas_num;
	struct platform_device *canvas_dev;
	ulong flags;
	ulong fiq_flag;
};
static struct canvas_device_info canvas_info;

#define CANVAS_VALID(n) ((n) >= 0 && (n) < canvas_pool_canvas_num())

static void
canvas_lut_data_build(ulong addr, u32 width, u32 height, u32 wrap, u32 blkmode,
	u32 endian, u32 *data_l, u32 *data_h)
{
	/*
	 *DMC_CAV_LUT_DATAL/DMC_CAV_LUT_DATAH
	 *high 32bits of cavnas data which need to be configured
	 *to canvas memory.
	 *64bits CANVAS look up table
	 *bit 61:58   Endian control.
	 *bit 61:  1 : switch 2 64bits data inside 128bits boundary.
	 *0 : no change.
	 *bit 60:  1 : switch 2 32bits data inside 64bits data boundary.
	 *0 : no change.
	 *bit 59:  1 : switch 2 16bits data inside 32bits data boundary.
	 *0 : no change.
	 *bit 58:  1 : switch 2 8bits data  inside 16bits data bournday.
	 *0 : no change.
	 *bit 57:56.   Canvas block mode.  2 : 64x32, 1: 32x32;
	 *0 : linear mode.
	 *bit 55:      canvas Y direction wrap control.
	 *1: wrap back in y.  0: not wrap back.
	 *bit 54:      canvas X direction wrap control.
	 *1: wrap back in X.  0: not wrap back.
	 *bit 53:41.   canvas Hight.
	 *bit 40:29.   canvas Width, unit: 8bytes. must in 32bytes boundary.
	 *that means last 2 bits must be 0.
	 *bit 28:0.    cavnas start address.   unit. 8 bytes. must be in
	 *32bytes boundary. that means last 2bits must be 0.
	 */

#define CANVAS_WADDR_LBIT 0
#define CANVAS_WIDTH_LBIT 29
#define CANVAS_WIDTH_HBIT 0
#define CANVAS_HEIGHT_HBIT (41 - 32)
#define CANVAS_WRAPX_HBIT  (54 - 32)
#define CANVAS_WRAPY_HBIT  (55 - 32)
#define CANVAS_BLKMODE_HBIT (56 - 32)
#define CANVAS_ENDIAN_HBIT (58 - 32)

	u32 addr_bits_l = ((addr + 7) >> 3 & CANVAS_ADDR_LMASK)
		<< CANVAS_WADDR_LBIT;

	u32 width_l = ((((width + 7) >> 3) & CANVAS_WIDTH_LMASK)
		<< CANVAS_WIDTH_LBIT);

	u32 width_h = ((((width + 7) >> 3) >> CANVAS_WIDTH_LWID)
		<< CANVAS_WIDTH_HBIT);

	u32 height_h = (height & CANVAS_HEIGHT_MASK)
		<< CANVAS_HEIGHT_BIT;

	u32 wrap_h = (wrap & (CANVAS_ADDR_WRAPX | CANVAS_ADDR_WRAPY));

	u32 blkmod_h = (blkmode & CANVAS_BLKMODE_MASK)
		<< CANVAS_BLKMODE_HBIT;

	u32 switch_bits_ctl = (endian & 0xf)
		<< CANVAS_ENDIAN_HBIT;

	*data_l = addr_bits_l | width_l;
	*data_h = width_h | height_h | wrap_h | blkmod_h | switch_bits_ctl;
}

static void canvas_lut_data_parser(struct canvas_s *canvas, u32 data_l,
	u32 data_h)
{
	ulong addr;
	u32 width;
	u32 height;
	u32 wrap;
	u32 blkmode;
	u32 endian;

	addr = (data_l & CANVAS_ADDR_LMASK) << 3;
	width = (data_l >> 29) & 0x7;
	width |= (data_h & 0x1ff) << 3;
	width = width << 3;

	height = (data_h >> (41 - 32)) & 0xfff;

	wrap = (data_h >> (54 - 32)) & 0x3;
	blkmode = (data_h >> (56 - 32)) & 0x3;
	endian = (data_h >> (58 - 32)) & 0xf;
	canvas->addr = addr;
	canvas->width = width;
	canvas->height = height;
	canvas->wrap = wrap;
	canvas->blkmode = blkmode;
	canvas->endian = endian;
}

static void canvas_config_locked(u32 index, struct canvas_s *p)
{
	struct canvas_device_info *info = &canvas_info;
	u32 datal, datah;
	u32 reg_add = 0;

	canvas_lut_data_build(p->addr,
			p->width,
			p->height,
			p->wrap,
			p->blkmode,
			p->endian, &datal, &datah);

	if ((get_cpu_type() == MESON_CPU_MAJOR_ID_M8M2) ||
		(get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB))
		reg_add = DC_CAV_LUT_DATAL_M8M2 - DC_CAV_LUT_DATAL;

	canvas_io_write(info->reg_base + reg_add + DC_CAV_LUT_DATAL, datal);

	canvas_io_write(info->reg_base + reg_add + DC_CAV_LUT_DATAH, datah);

	canvas_io_write(info->reg_base + reg_add + DC_CAV_LUT_ADDR,
		CANVAS_LUT_WR_EN | index);

	canvas_io_read(info->reg_base + reg_add + DC_CAV_LUT_DATAH);

	p->dataL = datal;
	p->dataH = datah;

}

int canvas_read_hw(u32 index, struct canvas_s *canvas)
{
	struct canvas_device_info *info = &canvas_info;
	u32 datal, datah;
	int reg_add = 0;

	if (!CANVAS_VALID(index))
		return -1;
	datal = datah = 0;
	if ((get_cpu_type() == MESON_CPU_MAJOR_ID_M8M2) ||
		(get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB))
		reg_add = DC_CAV_LUT_DATAL_M8M2 - DC_CAV_LUT_DATAL;
	canvas_io_write(info->reg_base + reg_add + DC_CAV_LUT_ADDR,
		CANVAS_LUT_RD_EN | (index & 0xff));
	datal = canvas_io_read(info->reg_base + reg_add + DC_CAV_LUT_RDATAL);
	datah = canvas_io_read(info->reg_base + reg_add + DC_CAV_LUT_RDATAH);
	canvas->dataL = datal;
	canvas->dataH = datah;
	canvas_lut_data_parser(canvas, datal, datah);
	return 0;
}
EXPORT_SYMBOL(canvas_read_hw);

#define canvas_lock(info, f, f2) do {\
		spin_lock_irqsave(&info->lock, f);\
		raw_local_save_flags(f2);\
		local_fiq_disable();\
	} while (0)

#define canvas_unlock(info, f, f2) do {\
		raw_local_irq_restore(f2);\
		spin_unlock_irqrestore(&info->lock, f);\
	} while (0)



void canvas_config_ex(u32 index, ulong addr, u32 width, u32 height, u32 wrap,
	u32 blkmode, u32 endian)
{
	struct canvas_device_info *info = &canvas_info;
	struct canvas_s *canvas;
	unsigned long flags, fiqflags;
	if (!CANVAS_VALID(index))
		return;

	if (!canvas_pool_canvas_alloced(index)) {
		pr_info("Try config not allocked canvas[%d]\n", index);
		dump_stack();
		return;
	}
	canvas_lock(info, flags, fiqflags);
	canvas = &info->canvasPool[index];
	canvas->addr = addr;
	canvas->width = width;
	canvas->height = height;
	canvas->wrap = wrap;
	canvas->blkmode = blkmode;
	canvas->endian = endian;
	canvas_config_locked(index, canvas);
	canvas_unlock(info, flags, fiqflags);
}
EXPORT_SYMBOL(canvas_config_ex);

void canvas_config_config(u32 index, struct canvas_config_s *cfg)
{
	canvas_config_ex(index, cfg->phy_addr,
		cfg->width, cfg->height, CANVAS_ADDR_NOWRAP,
		cfg->block_mode, cfg->endian);
}
EXPORT_SYMBOL(canvas_config_config);

void canvas_config(u32 index, ulong addr, u32 width, u32 height, u32 wrap,
	u32 blkmode)
{
	return canvas_config_ex(index, addr, width, height, wrap, blkmode, 0);
}
EXPORT_SYMBOL(canvas_config);

void canvas_read(u32 index, struct canvas_s *p)
{
	struct canvas_device_info *info = &canvas_info;

	if (CANVAS_VALID(index))
		*p = info->canvasPool[index];
}
EXPORT_SYMBOL(canvas_read);

void canvas_copy(u32 src, u32 dst)
{
	struct canvas_device_info *info = &canvas_info;
	struct canvas_s *canvas_src = &info->canvasPool[src];
	struct canvas_s *canvas_dst = &info->canvasPool[dst];
	unsigned long flags, fiqflags;

	if (!CANVAS_VALID(src) || !CANVAS_VALID(dst))
		return;

	if (!canvas_pool_canvas_alloced(src)
		|| !canvas_pool_canvas_alloced(dst)) {
		pr_info("!canvas_copy  without alloc,src=%d,dst=%d\n",
			src, dst);
		dump_stack();
		return;
	}

	canvas_lock(info, flags, fiqflags);
	canvas_dst->addr = canvas_src->addr;
	canvas_dst->width = canvas_src->width;
	canvas_dst->height = canvas_src->height;
	canvas_dst->wrap = canvas_src->wrap;
	canvas_dst->blkmode = canvas_src->blkmode;
	canvas_dst->endian = canvas_src->endian;
	canvas_dst->dataH = canvas_src->dataH;
	canvas_dst->dataL = canvas_src->dataL;
	canvas_config_locked(dst, canvas_dst);
	canvas_unlock(info, flags, fiqflags);
}
EXPORT_SYMBOL(canvas_copy);

void canvas_update_addr(u32 index, u32 addr)
{
	struct canvas_device_info *info = &canvas_info;
	struct canvas_s *canvas;
	unsigned long flags, fiqflags;

	if (!CANVAS_VALID(index))
		return;
	canvas = &info->canvasPool[index];
	if (!canvas_pool_canvas_alloced(index)) {
		pr_info("canvas_update_addrwithout alloc,index=%d\n",
			index);
		dump_stack();
		return;
	}
	canvas_lock(info, flags, fiqflags);
	canvas->addr = addr;
	canvas_config_locked(index, canvas);
	canvas_unlock(info, flags, fiqflags);

	return;
}
EXPORT_SYMBOL(canvas_update_addr);

unsigned int canvas_get_addr(u32 index)
{
	struct canvas_device_info *info = &canvas_info;

	return info->canvasPool[index].addr;
}
EXPORT_SYMBOL(canvas_get_addr);

/*********************************************************/
#define to_canvas(kobj) container_of(kobj, struct canvas_s, kobj)
static ssize_t addr_show(struct canvas_s *canvas, char *buf)
{
	return sprintf(buf, "0x%lx\n", canvas->addr);
}

static ssize_t width_show(struct canvas_s *canvas, char *buf)
{
	return sprintf(buf, "%d\n", canvas->width);
}

static ssize_t height_show(struct canvas_s *canvas, char *buf)
{
	return sprintf(buf, "%d\n", canvas->height);
}

static ssize_t show_canvas(struct canvas_s *canvas, char *buf)
{
	int l = 0;

	l = sprintf(buf + l, "index:0x%x\n", (unsigned int)canvas->index);
	l += sprintf(buf + l, "addr:0x%x\n", (unsigned int)canvas->addr);
	l += sprintf(buf + l, "height:%d\n", canvas->height);
	l += sprintf(buf + l, "width:%d\n", canvas->width);
	l += sprintf(buf + l, "wrap:%d\n", canvas->wrap);
	l += sprintf(buf + l, "blkmode:%d\n", canvas->blkmode);
	l += sprintf(buf + l, "endian:%d\n", canvas->endian);
	l += sprintf(buf + l, "datal:%x\n", canvas->dataL);
	l += sprintf(buf + l, "datah:%x\n", canvas->dataH);
	return l;
}

static ssize_t config_show(struct canvas_s *canvas, char *buf)
{
	return show_canvas(canvas, buf);
}

static ssize_t confighw_show(struct canvas_s *canvas, char *buf)
{
	struct canvas_s hwcanvas;

	memset(&hwcanvas, 0, sizeof(hwcanvas));
	hwcanvas.index = canvas->index;
	canvas_read_hw(canvas->index, &hwcanvas);
	return show_canvas(&hwcanvas, buf);
}

static ssize_t confighw_store(struct canvas_s *canvas,
	const char *buf, size_t size)
{
	/*TODO FOR DEBUG
	 *
	 *ulong addr;
	 *u32 width;
	 *u32 height;
	 *u32 wrap;
	 *u32 blkmode;
	 *u32 endian;
	 *int ret;
	 *
	 *ret = sscanf(buf, "0x%x %d %d %d %d %d\n",
	 *(unsigned int *)&addr, &width,
	 *&height, &wrap, &blkmode, &endian);
	 *if (ret != 6) {
	 *pr_err("get parameters %d\n", ret);
	 *pr_err("usage: echo 0xaddr width height wrap blk end >\n");
	 *return -EIO;
	 *}
	 *canvas->addr = addr;
	 *canvas->width = width;
	 *canvas->height = height;
	 *canvas->wrap = wrap;
	 *canvas->blkmode = blkmode;
	 *canvas->endian = endian;
	 *
	 *canvas_config_locked(canvas->index, canvas);
	 */
	return 0;
}

struct canvas_sysfs_entry {
	struct attribute attr;

	 ssize_t (*show)(struct canvas_s *, char *);
	 ssize_t (*store)(struct canvas_s *, const char *, size_t);
};

static struct canvas_sysfs_entry addr_attribute = __ATTR_RO(addr);
static struct canvas_sysfs_entry width_attribute = __ATTR_RO(width);
static struct canvas_sysfs_entry height_attribute = __ATTR_RO(height);
static struct canvas_sysfs_entry config_attribute = __ATTR_RO(config);
static struct canvas_sysfs_entry confighw_attribute = __ATTR_RW(confighw);

static void canvas_release(struct kobject *kobj)
{
}

static ssize_t canvas_type_show(struct kobject *kobj, struct attribute *attr,
	char *buf)
{
	struct canvas_s *canvas = to_canvas(kobj);
	struct canvas_sysfs_entry *entry;

	entry = container_of(attr, struct canvas_sysfs_entry, attr);

	if (!entry->show)
		return -EIO;

	return entry->show(canvas, buf);
}

static ssize_t canvas_type_store(struct kobject *kobj, struct attribute *attr,
	const char *buf, size_t size)
{
	struct canvas_s *canvas = to_canvas(kobj);
	struct canvas_sysfs_entry *entry;

	entry = container_of(attr, struct canvas_sysfs_entry, attr);

	if (!entry->store)
		return -EIO;

	return entry->store(canvas, buf, size);
}

static const struct sysfs_ops canvas_sysfs_ops = {
	.show = canvas_type_show,
	.store = canvas_type_store,
};

static struct attribute *canvas_attrs[] = {
	&addr_attribute.attr,
	&width_attribute.attr,
	&height_attribute.attr,
	&config_attribute.attr,
	&confighw_attribute.attr,
	NULL,
};

static struct kobj_type canvas_attr_type = {
	.release = canvas_release,
	.sysfs_ops = &canvas_sysfs_ops,
	.default_attrs = canvas_attrs,
};

/* static int __devinit canvas_probe(struct platform_device *pdev) */
static int canvas_probe(struct platform_device *pdev)
{

	int i, r;
	struct canvas_device_info *info = &canvas_info;
	struct resource *res;
	int size;

	r = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "cannot obtain I/O memory region");
		return -ENODEV;
	}
	info->res = *res;
	size = (int)resource_size(res);
	pr_info("canvas_probe reg=%p,size=%x\n",
			(void *)res->start, size);
	if (!devm_request_mem_region(&pdev->dev,
		res->start, size, pdev->name)) {
		dev_err(&pdev->dev, "Memory region busy\n");
		return -EBUSY;
	}
	info->reg_base = devm_ioremap_nocache(&pdev->dev, res->start, size);
	if (info->reg_base == NULL) {
		dev_err(&pdev->dev, "devm_ioremap_nocache canvas failed!\n");
		goto err1;
	}
	pr_info("canvas maped reg_base =%p\n", info->reg_base);
	amcanvas_manager_init();
	memset(info->canvasPool, 0, CANVAS_MAX_NUM * sizeof(struct canvas_s));
	info->max_canvas_num = canvas_pool_canvas_num();
	spin_lock_init(&info->lock);
	for (i = 0; i < info->max_canvas_num; i++) {
		info->canvasPool[i].index = i;
		r = kobject_init_and_add(&info->canvasPool[i].kobj,
				&canvas_attr_type,
				&pdev->dev.kobj, "%d", i);
		if (r) {
			pr_error("Unable to create canvas objects %d\n", i);
			goto err2;
		}
	}
	info->canvas_dev = pdev;
	return 0;

err2:
	for (i--; i >= 0; i--)
		kobject_put(&info->canvasPool[i].kobj);
	amcanvas_manager_exit();
	devm_iounmap(&pdev->dev, info->reg_base);
err1:
	devm_release_mem_region(&pdev->dev, res->start, size);
	pr_error("Canvas driver probe failed\n");
	return r;
}

/* static int __devexit canvas_remove(struct platform_device *pdev) */
static int canvas_remove(struct platform_device *pdev)
{
	int i;
	struct canvas_device_info *info = &canvas_info;

	for (i = 0; i < info->max_canvas_num; i++)
		kobject_put(&info->canvasPool[i].kobj);
	amcanvas_manager_exit();
	devm_iounmap(&pdev->dev, info->reg_base);
	devm_release_mem_region(&pdev->dev,
				info->res.start,
				resource_size(&info->res));
	pr_error("Canvas driver removed.\n");

	return 0;
}

static const struct of_device_id canvas_dt_match[] = {
	{
			.compatible = "amlogic, meson, canvas",
		},
	{},
};

static struct platform_driver canvas_driver = {
	.probe = canvas_probe,
	.remove = canvas_remove,
	.driver = {
			.name = "amlogic-canvas",
			.of_match_table = canvas_dt_match,
		},
};

static int __init amcanvas_init(void)
{
	int r;

	r = platform_driver_register(&canvas_driver);
	if (r) {
		pr_error("Unable to register canvas driver\n");
		return r;
	}
	pr_info("register canvas platform driver\n");

	return 0;
}

static void __exit amcanvas_exit(void)
{

	platform_driver_unregister(&canvas_driver);
}

postcore_initcall(amcanvas_init);
module_exit(amcanvas_exit);

MODULE_DESCRIPTION("AMLOGIC Canvas management driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
