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
#include <linux/err.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <mach/common.h>
#include <asm/hardware/gic.h>

#define GPC_CNTR		0x0
#define GPC_IMR1		0x008
#define GPC_PGC_GPU_PGCR	0x260
#define GPC_PGC_CPU_PDN		0x2a0
#define GPC_PGC_CPU_PUPSCR	0x2a4
#define GPC_PGC_CPU_PDNSCR	0x2a8

#define IMR_NUM			4

static void __iomem *gpc_base;
static u32 gpc_wake_irqs[IMR_NUM];
static u32 gpc_saved_imrs[IMR_NUM];
static u32 gpc_pu_count;
static bool pu_clk_get ;
static struct mutex pu_lock;
static struct clk *gpu3d_clk, *gpu3d_shader_clk, *gpu2d_clk, *gpu2d_axi_clk;
static struct clk *openvg_axi_clk, *vpu_clk;

static void imx_pu_clk(bool enable)
{
	if (enable) {
		clk_prepare(gpu3d_clk);
		clk_enable(gpu3d_clk);
		clk_prepare(gpu3d_shader_clk);
		clk_enable(gpu3d_shader_clk);
		clk_prepare(vpu_clk);
		clk_enable(vpu_clk);
		clk_prepare(gpu2d_clk);
		clk_enable(gpu2d_clk);
		clk_prepare(gpu2d_axi_clk);
		clk_enable(gpu2d_axi_clk);
		clk_prepare(openvg_axi_clk);
		clk_enable(openvg_axi_clk);
	} else {
		clk_disable(gpu3d_clk);
		clk_unprepare(gpu3d_clk);
		clk_disable(gpu3d_shader_clk);
		clk_unprepare(gpu3d_shader_clk);
		clk_disable(vpu_clk);
		clk_unprepare(vpu_clk);
		clk_disable(gpu2d_clk);
		clk_unprepare(gpu2d_clk);
		clk_disable(gpu2d_axi_clk);
		clk_unprepare(gpu2d_axi_clk);
		clk_disable(openvg_axi_clk);
		clk_unprepare(openvg_axi_clk);
	}
}

static void imx_gpc_pu_clk_init(void)
{
	/* Get gpu&vpu clk for power up PU by GPC */
	gpu3d_clk = clk_get(NULL, "gpu3d_core");
	if (IS_ERR(gpu3d_clk))
		printk(KERN_ERR "%s: failed to get gpu3d_clk!\n" , __func__);
	gpu3d_shader_clk = clk_get(NULL, "gpu3d_shader");
	if (IS_ERR(gpu3d_shader_clk))
		printk(KERN_ERR "%s: failed to get shade_clk!\n", __func__);
	vpu_clk = clk_get(NULL, "vpu_axi");
	if (IS_ERR(vpu_clk))
		printk(KERN_ERR "%s: failed to get vpu_clk!\n", __func__);

	gpu2d_clk = clk_get(NULL, "gpu2d_core");
	if (IS_ERR(gpu2d_clk))
		printk(KERN_ERR "%s: failed to get gpu2d_clk!\n" , __func__);
	gpu2d_axi_clk = clk_get(NULL, "gpu2d_axi");
	if (IS_ERR(gpu2d_axi_clk))
		printk(KERN_ERR "%s: failed to get gpu2d_axi_clk!\n", __func__);
	openvg_axi_clk = clk_get(NULL, "openvg_axi");
	if (IS_ERR(openvg_axi_clk))
		printk(KERN_ERR "%s: failed to get openvg_clk!\n", __func__);
}
void imx_gpc_power_up_pu(bool flag)
{
	unsigned int reg;

	mutex_lock(&pu_lock);

	if (!pu_clk_get) {
		imx_gpc_pu_clk_init();
		pu_clk_get = true;
	}

	if (gpc_pu_count && flag) {
		gpc_pu_count++;
		mutex_unlock(&pu_lock);
		return;
	}
	if ((gpc_pu_count > 1) && !flag) {
		gpc_pu_count--;
		mutex_unlock(&pu_lock);
		return;
	}

	if (!gpc_pu_count && flag) {
		/* turn on vddpu */
		imx_anatop_pu_vol(true);
		/* enable pu clock */
		imx_pu_clk(true);
		/* enable power up request */
		reg = __raw_readl(gpc_base + GPC_PGC_GPU_PGCR);
		__raw_writel(reg | 0x1, gpc_base + GPC_PGC_GPU_PGCR);
		/* power up request */
		reg = __raw_readl(gpc_base + GPC_CNTR);
		__raw_writel(reg | 0x2, gpc_base + GPC_CNTR);
		/* Wait for the power up bit to clear */
		while (__raw_readl(gpc_base + GPC_CNTR) & 0x2)
			;
		/* disable pu clock */
		imx_pu_clk(false);
		gpc_pu_count++;
	} else if ((gpc_pu_count == 1) && !flag) {
		/* enable power down request */
		reg = __raw_readl(gpc_base + GPC_PGC_GPU_PGCR);
		__raw_writel(reg | 0x1, gpc_base + GPC_PGC_GPU_PGCR);
		/* power down request */
		reg = __raw_readl(gpc_base + GPC_CNTR);
		__raw_writel(reg | 0x1, gpc_base + GPC_CNTR);
		/* Wait for power down to complete */
		while (__raw_readl(gpc_base + GPC_CNTR) & 0x1)
			;
		/* turn off vddpu */
		imx_anatop_pu_vol(false);
		gpc_pu_count--;
	}
	mutex_unlock(&pu_lock);
	return;
}

void imx_gpc_pre_suspend(bool arm_power_off)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1;
	int i;

	if (arm_power_off) {
		/* Tell GPC to power off ARM core when suspend */
		writel_relaxed(0x1, gpc_base + GPC_PGC_CPU_PDN);

		/*
		 * The PUPSCR is a counter that counts in CKIL(32K) cycles.
		 * Should include the time it takes for the ARM LDO to ramp up.
		 */
		writel_relaxed(0xf0f, gpc_base + GPC_PGC_CPU_PUPSCR);

		/*
		 * The PDNSCR is a counter that counts in IPG_CLK cycles.
		 * This counter can be set to minimum values to power
		 * down faster.
		 */
		writel_relaxed(0x101, gpc_base + GPC_PGC_CPU_PDNSCR);
	}

	for (i = 0; i < IMR_NUM; i++) {
		gpc_saved_imrs[i] = readl_relaxed(reg_imr1 + i * 4);
		writel_relaxed(~gpc_wake_irqs[i], reg_imr1 + i * 4);
	}
}

void imx_gpc_post_resume(void)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1;
	int i;

	/* Keep ARM core powered on for other low-power modes */
	writel_relaxed(0x0, gpc_base + GPC_PGC_CPU_PDN);

	for (i = 0; i < IMR_NUM; i++)
		writel_relaxed(gpc_saved_imrs[i], reg_imr1 + i * 4);
}

void imx_gpc_mask_all(void)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1;
	int i;

	for (i = 0; i < IMR_NUM; i++)
		writel_relaxed(0xffffffff, reg_imr1 + i * 4);
}

static int imx_gpc_irq_set_wake(struct irq_data *d, unsigned int on)
{
	unsigned int idx = d->irq / 32 - 1;
	u32 mask;

	/* Sanity check for SPI irq */
	if (d->irq < 32)
		return -EINVAL;

	mask = 1 << d->irq % 32;
	gpc_wake_irqs[idx] = on ? gpc_wake_irqs[idx] | mask :
				  gpc_wake_irqs[idx] & ~mask;

	return 0;
}

static void imx_gpc_irq_unmask(struct irq_data *d)
{
	void __iomem *reg;
	u32 val;

	/* Sanity check for SPI irq */
	if (d->irq < 32)
		return;

	reg = gpc_base + GPC_IMR1 + (d->irq / 32 - 1) * 4;
	val = readl_relaxed(reg);
	val &= ~(1 << d->irq % 32);
	writel_relaxed(val, reg);
}

static void imx_gpc_irq_mask(struct irq_data *d)
{
	void __iomem *reg;
	u32 val;

	/* Sanity check for SPI irq */
	if (d->irq < 32)
		return;

	reg = gpc_base + GPC_IMR1 + (d->irq / 32 - 1) * 4;
	val = readl_relaxed(reg);
	val |= 1 << (d->irq % 32);
	writel_relaxed(val, reg);
}

void __init imx_gpc_init(void)
{
	struct device_node *np;
	int i;

	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-gpc");
	gpc_base = of_iomap(np, 0);
	WARN_ON(!gpc_base);

	/* Initially mask all interrupts */
	for (i = 0; i < IMR_NUM; i++)
		writel_relaxed(~0, gpc_base + GPC_IMR1 + i * 4);

	/* Register GPC as the secondary interrupt controller behind GIC */
	gic_arch_extn.irq_mask = imx_gpc_irq_mask;
	gic_arch_extn.irq_unmask = imx_gpc_irq_unmask;
	gic_arch_extn.irq_set_wake = imx_gpc_irq_set_wake;

	mutex_init(&pu_lock);
}
