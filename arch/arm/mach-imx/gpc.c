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

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/irqchip/arm-gic.h>
#include "common.h"

#define GPC_IMR1		0x008
#define GPC_PGC_CPU_PDN		0x2a0
#define GPC_PGC_GPU_PDN		0x260
#define GPC_CNTR		0x0
#define GPC_CNTR_xPU_UP_REQ_SHIFT	0x1

#define IMR_NUM			4

static void __iomem *gpc_base;
static u32 gpc_wake_irqs[IMR_NUM];
static u32 gpc_saved_imrs[IMR_NUM];
static struct clk *gpu3d_clk, *gpu3d_shader_clk, *gpu2d_clk, *gpu2d_axi_clk;
static struct clk *openvg_axi_clk, *vpu_clk;

void imx_gpc_pre_suspend(void)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1;
	int i;

	/* Tell GPC to power off ARM core when suspend */
	writel_relaxed(0x1, gpc_base + GPC_PGC_CPU_PDN);

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

void imx_gpc_mask_all(void)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1;
	int i;

	for (i = 0; i < IMR_NUM; i++) {
		gpc_saved_imrs[i] = readl_relaxed(reg_imr1 + i * 4);
		writel_relaxed(~0, reg_imr1 + i * 4);
	}

}

void imx_gpc_restore_all(void)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1;
	int i;

	for (i = 0; i < IMR_NUM; i++)
		writel_relaxed(gpc_saved_imrs[i], reg_imr1 + i * 4);
}

void imx_gpc_irq_unmask(struct irq_data *d)
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

void imx_gpc_irq_mask(struct irq_data *d)
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

static void imx_gpc_pu_clk_init(void)
{
	if (gpu3d_clk != NULL &&  gpu3d_shader_clk != NULL
		&& gpu2d_clk != NULL && gpu2d_axi_clk != NULL
		&& openvg_axi_clk != NULL && vpu_clk != NULL)
		return;

	/* Get gpu&vpu clk for power up PU by GPC */
	gpu3d_clk = clk_get(NULL, "gpu3d_core");
	gpu3d_shader_clk = clk_get(NULL, "gpu3d_shader");
	gpu2d_clk = clk_get(NULL, "gpu2d_core");
	gpu2d_axi_clk = clk_get(NULL, "gpu2d_axi");
	openvg_axi_clk = clk_get(NULL, "openvg_axi");
	vpu_clk = clk_get(NULL, "vpu_axi");
	if (IS_ERR(gpu3d_clk) || IS_ERR(gpu3d_shader_clk)
		|| IS_ERR(gpu2d_clk) || IS_ERR(gpu2d_axi_clk)
		|| IS_ERR(openvg_axi_clk) || IS_ERR(vpu_clk))
		printk(KERN_ERR "%s: failed to get clk!\n", __func__);
}

static void imx_pu_clk(bool enable)
{
	if (enable) {
		clk_prepare_enable(gpu3d_clk);
		clk_prepare_enable(gpu3d_shader_clk);
		clk_prepare_enable(vpu_clk);
		clk_prepare_enable(gpu2d_clk);
		clk_prepare_enable(gpu2d_axi_clk);
		clk_prepare_enable(openvg_axi_clk);
	} else {
		clk_disable_unprepare(gpu3d_clk);
		clk_disable_unprepare(gpu3d_shader_clk);
		clk_disable_unprepare(vpu_clk);
		clk_disable_unprepare(gpu2d_clk);
		clk_disable_unprepare(gpu2d_axi_clk);
		clk_disable_unprepare(openvg_axi_clk);
	}
}

void imx_gpc_xpu_enable(void)
{
	/*
	 * PU is turned off in uboot, so we need to turn it on here
	 * to avoid kernel hang during GPU init, will remove
	 * this code after PU power management done.
	 */
	imx_gpc_pu_clk_init();
	imx_pu_clk(true);
	writel_relaxed(1, gpc_base + GPC_PGC_GPU_PDN);
	writel_relaxed(1 << GPC_CNTR_xPU_UP_REQ_SHIFT, gpc_base + GPC_CNTR);
	while (readl_relaxed(gpc_base + GPC_CNTR) & 0x2)
		;
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
}
