/*
 * Copyright 2011-2013 Freescale Semiconductor, Inc.
 * Copyright 2011 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/suspend.h>
#include <linux/genalloc.h>
#include <asm/cacheflush.h>
#include <asm/fncpy.h>
#include <asm/proc-fns.h>
#include <asm/suspend.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/mach/map.h>

#include "common.h"
#include "hardware.h"

static struct gen_pool *iram_pool;
static void *suspend_iram_base;
static unsigned long iram_size, iram_paddr;
static int (*suspend_in_iram_fn)(void *iram_vbase,
	unsigned long iram_pbase, unsigned int cpu_type);
static unsigned int cpu_type;

static int imx6q_suspend_finish(unsigned long val)
{
	/*
	 * call low level suspend function in iram,
	 * as we need to float DDR IO.
	 */
	suspend_in_iram_fn(suspend_iram_base, iram_paddr, cpu_type);
	return 0;
}

static int imx6q_pm_enter(suspend_state_t state)
{
	switch (state) {
	case PM_SUSPEND_MEM:
		imx6q_set_lpm(STOP_POWER_OFF);
		imx_gpc_pre_suspend();
		imx_anatop_pre_suspend();
		imx_set_cpu_jump(0, v7_cpu_resume);
		/* Zzz ... */
		cpu_suspend(0, imx6q_suspend_finish);
		imx_smp_prepare();
		imx_anatop_post_resume();
		imx_gpc_post_resume();
		imx6q_set_lpm(WAIT_CLOCKED);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct map_desc imx6_pm_io_desc[] __initdata = {
	imx_map_entry(MX6Q, MMDC_P0, MT_DEVICE),
	imx_map_entry(MX6Q, MMDC_P1, MT_DEVICE),
	imx_map_entry(MX6Q, SRC, MT_DEVICE),
	imx_map_entry(MX6Q, IOMUXC, MT_DEVICE),
	imx_map_entry(MX6Q, CCM, MT_DEVICE),
	imx_map_entry(MX6Q, ANATOP, MT_DEVICE),
	imx_map_entry(MX6Q, GPC, MT_DEVICE),
	imx_map_entry(MX6Q, L2, MT_DEVICE),
};

void __init imx6_pm_map_io(void)
{
	iotable_init(imx6_pm_io_desc, ARRAY_SIZE(imx6_pm_io_desc));
}

static const struct platform_suspend_ops imx6q_pm_ops = {
	.enter = imx6q_pm_enter,
	.valid = suspend_valid_only_mem,
};

void __init imx6q_pm_init(void)
{
	struct device_node *node;
	unsigned long iram_base;
	struct platform_device *pdev;

	node = of_find_compatible_node(NULL, NULL, "mmio-sram");
	if (!node) {
		pr_err("failed to find ocram node!\n");
		return;
	}

	pdev = of_find_device_by_node(node);
	if (!pdev) {
		pr_err("failed to find ocram device!\n");
		return;
	}

	iram_pool = dev_get_gen_pool(&pdev->dev);
	if (!iram_pool) {
		pr_err("iram pool unavailable!\n");
		return;
	}

	iram_size = MX6_SUSPEND_IRAM_SIZE;

	iram_base = gen_pool_alloc(iram_pool, iram_size);
	if (!iram_base) {
		pr_err("unable to alloc iram!\n");
		return;
	}

	iram_paddr = gen_pool_virt_to_phys(iram_pool, iram_base);

	suspend_iram_base = __arm_ioremap(iram_paddr, iram_size,
			MT_MEMORY_NONCACHED);

	suspend_in_iram_fn = (void *)fncpy(suspend_iram_base,
		&imx6_suspend, iram_size);

	suspend_set_ops(&imx6q_pm_ops);

	/* Set cpu_type for DSM */
	if (cpu_is_imx6q())
		cpu_type = MXC_CPU_IMX6Q;
	else if (cpu_is_imx6dl())
		cpu_type = MXC_CPU_IMX6DL;
}
