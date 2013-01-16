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
#include <linux/suspend.h>
#include <asm/cacheflush.h>
#include <asm/proc-fns.h>
#include <asm/suspend.h>
#include <asm/hardware/cache-l2x0.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <asm/mach/map.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/delay.h>

extern unsigned long phys_l2x0_saved_regs;
static void *suspend_iram_base;
static struct clk *ocram_clk;
static void __iomem *iram_base;
static int (*suspend_in_iram_fn)(unsigned int *iram_vbase,
	unsigned int *iram_pbase, unsigned int cpu_type);

static int imx6q_suspend_finish(unsigned long val)
{
	/* call low level suspend function in iram,
	 * as we need to float DDR IO */
	suspend_in_iram_fn((unsigned int *)iram_base,
		(unsigned int *)(MX6Q_IRAM_BASE_ADDR +
		MX6Q_IRAM_SIZE), 0);
	return 0;
}

static int imx6q_pm_enter(suspend_state_t state)
{
	switch (state) {
	case PM_SUSPEND_MEM:
		imx6q_set_lpm(STOP_POWER_OFF);
		imx_gpc_pre_suspend();
		imx_set_cpu_jump(0, v7_cpu_resume);
		/* Zzz ... */
		cpu_suspend(0, imx6q_suspend_finish);
		imx_smp_prepare();
		imx_gpc_post_resume();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct map_desc mx6_pm_io_desc[] __initdata = {
	{
	.virtual = IMX_IO_P2V(MX6Q_MMDC_P0_BASE_ADDR),
	.pfn = __phys_to_pfn(MX6Q_MMDC_P0_BASE_ADDR),
	.length = SZ_4K,
	.type = MT_DEVICE},
	{
	.virtual = IMX_IO_P2V(MX6Q_SRC_BASE_ADDR),
	.pfn = __phys_to_pfn(MX6Q_SRC_BASE_ADDR),
	.length = SZ_4K,
	.type = MT_DEVICE},
	{
	.virtual = IMX_IO_P2V(MX6Q_IOMUXC_BASE_ADDR),
	.pfn = __phys_to_pfn(MX6Q_IOMUXC_BASE_ADDR),
	.length = SZ_4K,
	.type = MT_DEVICE},
	{
	.virtual = IMX_IO_P2V(MX6Q_CCM_BASE_ADDR),
	.pfn = __phys_to_pfn(MX6Q_CCM_BASE_ADDR),
	.length = SZ_4K,
	.type = MT_DEVICE},
	{
	.virtual = IMX_IO_P2V(MX6Q_ANATOP_BASE_ADDR),
	.pfn = __phys_to_pfn(MX6Q_ANATOP_BASE_ADDR),
	.length = SZ_4K,
	.type = MT_DEVICE},
};

void __init imx_pm_map_io(void)
{
	iotable_init(mx6_pm_io_desc, ARRAY_SIZE(mx6_pm_io_desc));
}

static const struct platform_suspend_ops imx6q_pm_ops = {
	.enter = imx6q_pm_enter,
	.valid = suspend_valid_only_mem,
};

void __init imx6q_pm_init(void)
{
	ocram_clk = clk_get(NULL, "ocram");
	if (IS_ERR(ocram_clk)) {
		printk(KERN_ERR "%s: failed to get ocram clk\n", __func__);
		return;
	}
	clk_prepare(ocram_clk);
	clk_enable(ocram_clk);
	iram_base = ioremap(MX6Q_IRAM_BASE_ADDR + MX6Q_IRAM_SIZE, SZ_4K);
	/* last 4K of IRAM is reserved for suspend/resume */
	suspend_iram_base = __arm_ioremap(MX6Q_IRAM_BASE_ADDR +
		MX6Q_IRAM_SIZE, SZ_4K, MT_MEMORY_NONCACHED);
	memcpy((void *)suspend_iram_base, imx_suspend, SZ_4K);
	suspend_in_iram_fn = (void *)suspend_iram_base;
	/*
	 * The l2x0 core code provides an infrastucture to save and restore
	 * l2x0 registers across suspend/resume cycle.  But because imx6q
	 * retains L2 content during suspend and needs to resume L2 before
	 * MMU is enabled, it can only utilize register saving support and
	 * have to take care of restoring on its own.  So we save physical
	 * address of the data structure used by l2x0 core to save registers,
	 * and later restore the necessary ones in imx6q resume entry.
	 */
#ifdef CONFIG_CACHE_L2X0
	phys_l2x0_saved_regs = __pa(&l2x0_saved_regs);
#endif

	suspend_set_ops(&imx6q_pm_ops);
}
