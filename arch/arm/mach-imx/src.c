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

#include <linux/export.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <asm/smp_plat.h>

#define SRC_SCR				0x000
#define SRC_GPR1			0x020
#define BP_SRC_SCR_WARM_RESET_ENABLE	0
#define BP_SRC_SCR_GPU3D_RST		1
#define BP_SRC_SCR_GPU2D_RST		4
#define BP_SRC_SCR_CORE1_RST		14
#define BP_SRC_SCR_CORE1_ENABLE		22

static void __iomem *src_base;

static DEFINE_SPINLOCK(scr_lock);

void imx_enable_cpu(int cpu, bool enable)
{
	u32 mask, val;
	unsigned long flags;

	cpu = cpu_logical_map(cpu);
	mask = 1 << (BP_SRC_SCR_CORE1_ENABLE + cpu - 1);
	spin_lock_irqsave(&scr_lock, flags);
	val = readl_relaxed(src_base + SRC_SCR);
	val = enable ? val | mask : val & ~mask;
	val |= 1 << (BP_SRC_SCR_CORE1_RST + cpu - 1);
	writel_relaxed(val, src_base + SRC_SCR);
	spin_unlock_irqrestore(&scr_lock, flags);

	val = jiffies;
	/* wait secondary cpu reset done, timeout is 10ms */
	while ((readl_relaxed(src_base + SRC_SCR) &
		(1 << (BP_SRC_SCR_CORE1_RST + cpu - 1))) != 0) {
		if (time_after(jiffies, (unsigned long)(val +
			msecs_to_jiffies(10)))) {
			printk(KERN_WARNING "cpu %d: cpu reset fail\n", cpu);
			break;
		}
	}
}

void imx_kill_cpu(unsigned int cpu)
{
	unsigned int val;

	val = jiffies;
	/* wait secondary cpu to die, timeout is 50ms */
	while (readl_relaxed(src_base + SRC_GPR1 + (8 * cpu) + 4) == 0) {
		if (time_after(jiffies, (unsigned long)(val +
			msecs_to_jiffies(50)))) {
			printk(KERN_WARNING "cpu %d: no handshake.\n", cpu);
			break;
		}
	}
	imx_enable_cpu(cpu, false);
}

void imx_cpu_handshake(unsigned int cpu, bool enable)
{
	writel_relaxed(!!enable, src_base + SRC_GPR1 + (8 * cpu) + 4);
}

void imx_set_cpu_jump(int cpu, void *jump_addr)
{
	cpu = cpu_logical_map(cpu);
	writel_relaxed(virt_to_phys(jump_addr),
		       src_base + SRC_GPR1 + cpu * 8);
}

void imx_src_prepare_restart(void)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&scr_lock, flags);
	/* clear enable bits of secondary cores */
	val = readl_relaxed(src_base + SRC_SCR);
	val &= ~(0x7 << BP_SRC_SCR_CORE1_ENABLE);
	writel_relaxed(val, src_base + SRC_SCR);
	spin_unlock_irqrestore(&scr_lock, flags);

	/* clear persistent entry register of primary core */
	writel_relaxed(0, src_base + SRC_GPR1);
}

int imx_src_reset_gpu(int gpucore_id)
{
	u32 bit_offset, val;
	unsigned long flags;

	/*
	 * gcvCORE_MAJOR    0x0
	 * gcvCORE_2D       0x1
	 * cvCORE_VG        0x2
	 */

	if (gpucore_id == 0x0)
		bit_offset = BP_SRC_SCR_GPU3D_RST;
	else if ((gpucore_id == 0x1) || (gpucore_id == 0x2))
		bit_offset = BP_SRC_SCR_GPU2D_RST;
	else
		return -1;

	spin_lock_irqsave(&scr_lock, flags);
	val = readl_relaxed(src_base + SRC_SCR);
	val |= (1 << bit_offset);
	writel_relaxed(val, src_base + SRC_SCR);
	spin_unlock_irqrestore(&scr_lock, flags);

	val = jiffies;
	while ((readl_relaxed(src_base + SRC_SCR) &
		(1 << bit_offset)) != 0) {
		if (time_after(jiffies, (unsigned long)(val +
			msecs_to_jiffies(10)))) {
			printk(KERN_WARNING "gpu %d: gpu hw reset timeout\n", gpucore_id);
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL(imx_src_reset_gpu);

void __init imx_src_init(void)
{
	struct device_node *np;
	u32 val;
	unsigned long flags;

	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-src");
	src_base = of_iomap(np, 0);
	WARN_ON(!src_base);

	/*
	 * force warm reset sources to generate cold reset
	 * for a more reliable restart
	 */
	spin_lock_irqsave(&scr_lock, flags);
	val = readl_relaxed(src_base + SRC_SCR);
	val &= ~(1 << BP_SRC_SCR_WARM_RESET_ENABLE);
	writel_relaxed(val, src_base + SRC_SCR);
	spin_unlock_irqrestore(&scr_lock, flags);
}
