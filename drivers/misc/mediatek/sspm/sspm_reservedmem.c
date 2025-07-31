// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by kmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>
#include "sspm_define.h"
#include "sspm_helper.h"
#include "sspm_reservedmem.h"
#define _SSPM_INTERNAL_
#include "sspm_reservedmem_define.h"

#define SSPM_MEM_TBL_UNIT 2

static phys_addr_t sspm_mem_base_phys;
static phys_addr_t sspm_mem_base_virt;
static phys_addr_t sspm_mem_size;

static void sspm_reserve_memory_ioremap(struct platform_device *pdev)
{
	struct device_node *mem_region;
	struct resource res;
	unsigned int res_ram_start = 0;
	unsigned int res_ram_size = 0;
	int ret;

	/* Get reserved memory from dts if have */
	of_property_read_u32(pdev->dev.of_node, "sspm-res-ram-start",
						 &res_ram_start);

	if (res_ram_start) {
		of_property_read_u32(pdev->dev.of_node, "sspm-res-ram-size",
							 &res_ram_size);

		sspm_mem_base_phys = (phys_addr_t)res_ram_start;
		sspm_mem_size = (phys_addr_t)res_ram_size;
		return;
	}

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret)
		dev_err(&pdev->dev, "of_reserved_mem_device_init failed\n");

	mem_region = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!mem_region) {
		dev_err(&pdev->dev, "no memory-region sysmem phandle\n");
		return;
	}

	ret = of_address_to_resource(mem_region, 0, &res);
	of_node_put(mem_region);
	if (ret) {
		dev_err(&pdev->dev, "of_address_to_resource sysmem failed\n");
		return;
	}

	sspm_mem_base_phys = (phys_addr_t)res.start;
	sspm_mem_size = resource_size(&res);
}

phys_addr_t sspm_reserve_mem_get_phys(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[SSPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return sspm_reserve_mblock[id].start_phys;
}
EXPORT_SYMBOL_GPL(sspm_reserve_mem_get_phys);

phys_addr_t sspm_reserve_mem_get_virt(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[SSPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return sspm_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL_GPL(sspm_reserve_mem_get_virt);

phys_addr_t sspm_reserve_mem_get_size(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[SSPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return sspm_reserve_mblock[id].size;
}
EXPORT_SYMBOL_GPL(sspm_reserve_mem_get_size);

int sspm_reserve_memory_init(void)
{
	unsigned int id;
	phys_addr_t accumlate_memory_size;
	int ret;
	unsigned int sspm_mem_num, m_idx, m_size;

	if (NUMS_MEM_ID == 0)
		return 0;

	sspm_reserve_memory_ioremap(sspm_pdev);

	if (!sspm_mem_base_phys)
		return -1;

	/* Get reserved memory table from dts */
	ret = of_property_count_u32_elems(sspm_pdev->dev.of_node,
					"sspm-mem-tbl");

	if (ret <= 0) {
		pr_info("[SSPM] sspm-mem-tbl is not defined, skip read\n");
		ret = 0;
		/* ret = 0, skip read reserved mem tbl from dts */
	}
	sspm_mem_num = ret / SSPM_MEM_TBL_UNIT;

	for (id = 0; id < sspm_mem_num; id++) {
		ret = of_property_read_u32_index(sspm_pdev->dev.of_node,
				"sspm-mem-tbl",
				id * SSPM_MEM_TBL_UNIT,
				&m_idx);
		if (ret) {
			pr_err("[SSPM] cannot get memory index(%d)\n", id);
			return -1;
		}

		if (m_idx >= NUMS_MEM_ID) {
			pr_err("[SSPM] unexpected index: %d\n", m_idx);
			return -1;
		}

		ret = of_property_read_u32_index(sspm_pdev->dev.of_node,
				"sspm-mem-tbl",
				(id * SSPM_MEM_TBL_UNIT) + 1,
				&m_size);
		if (ret) {
			pr_err("[SSPM] cannot get memory size index(%d)\n", id);
			return -1;
		}

		sspm_reserve_mblock[m_idx].num = m_idx;
		sspm_reserve_mblock[m_idx].size = m_size;
#ifdef DEBUG
		pr_info("[SSPM] reserved: <%d  0x%x>\n", m_idx, m_size);
#endif
	}

    /* Phy memory */
	accumlate_memory_size = 0;
	for (id = 0; id < NUMS_MEM_ID; id++) {
		sspm_reserve_mblock[id].start_phys = sspm_mem_base_phys +
							accumlate_memory_size;

		accumlate_memory_size += sspm_reserve_mblock[id].size;
	}

    /* Virt memory */
	accumlate_memory_size = 0;
	sspm_mem_base_virt = (phys_addr_t)(uintptr_t)
			ioremap_wc(sspm_mem_base_phys, sspm_mem_size);

#ifdef DEBUG
	pr_info("[SSPM]reserve mem: virt:0x%llx - 0x%llx (0x%llx)\n",
			sspm_mem_base_virt,
			sspm_mem_base_virt + sspm_mem_size,
			sspm_mem_size);
#endif

	for (id = 0; id < NUMS_MEM_ID; id++) {
		sspm_reserve_mblock[id].start_virt = sspm_mem_base_virt +
							accumlate_memory_size;
		accumlate_memory_size += sspm_reserve_mblock[id].size;
	}
	/* the reserved memory should be larger then expected memory
	 * or sspm_reserve_mblock does not match dts
	 */

	WARN_ON_ONCE(accumlate_memory_size > sspm_mem_size);

#ifdef DEBUG
	for (id = 0; id < NUMS_MEM_ID; id++) {
		pr_info("[SSPM][mem_reserve-%d] ", id);
		pr_info("phys:0x%llx, virt:0x%llx, size:0x%llx\n",
			(unsigned long long)sspm_reserve_mem_get_phys(id),
			(unsigned long long)sspm_reserve_mem_get_virt(id),
			(unsigned long long)sspm_reserve_mem_get_size(id));
	}
#endif

	return 0;
}

#ifdef SSPM_SHARE_BUFFER_SUPPORT
void __iomem *sspm_base;
unsigned int sspm_share_region_base, sspm_share_region_size;
phys_addr_t sspm_sbuf_get(unsigned int offset)
{
	if (!is_sspm_ready()) {
		pr_notice("[SSPM] device resource is not ready\n");
		return 0;
	}

	if (offset < sspm_share_region_base ||
		offset > sspm_share_region_base + sspm_share_region_size) {
		pr_notice("[SSPM] illegal sbuf request: 0x%x\n", offset);
		return 0;
	} else {
		return (phys_addr_t)(sspm_base + offset);
	}
}
EXPORT_SYMBOL_GPL(sspm_sbuf_get);

int sspm_sbuf_init(void)
{
	struct device *dev = &sspm_pdev->dev;
	struct resource *res;
	u32 ret;

	if (sspm_pdev) {
		res = platform_get_resource_byname(sspm_pdev,
			IORESOURCE_MEM, "sspm_base");
		sspm_base = devm_ioremap_resource(dev, res);

		if (IS_ERR((void const *) sspm_base))
			return -1;
	} else {
		return -1;
	}

	ret = of_property_read_u32(sspm_pdev->dev.of_node, "sspm-share-region-base",
						&sspm_share_region_base);
	if (ret) {
		pr_notice("[SSPM] sspm_share_region_base is not defined.\n");
		return -1;
	}

	ret = of_property_read_u32(sspm_pdev->dev.of_node, "sspm-share-region-size",
						 &sspm_share_region_size);
	if (ret) {
		pr_notice("[SSPM] sspm_share_region_size is not defined.\n");
		return -1;
	}

	return 0;
}
#endif
