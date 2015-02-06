/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/regulator/consumer.h>
#include <linux/irqchip/arm-gic.h>
#include "common.h"
#include "hardware.h"

#define IMR_NUM			4
#define GPC_LPCR_A7_BSC		0x0
#define GPC_LPCR_A7_AD		0x4
#define GPC_LPCR_M4		0x8
#define GPC_SLPCR		0x14
#define GPC_PGC_ACK_SEL_A7	0x24
#define GPC_MISC_OFFSET		0x2c
#define GPC_IMR1_CORE0		0x30
#define GPC_IMR1_CORE1		0x40
#define GPC_SLOT0_CFG		0xb0
#define GPC_CPU_PGC_SW_PUP_REQ	0xf0
#define GPC_CPU_PGC_SW_PDN_REQ	0xfc
#define GPC_GTOR		0x124
#define GPC_PGC_C0		0x800
#define GPC_PGC_C1		0x840
#define GPC_PGC_SCU		0x880
#define GPC_PGC_FM		0xa00
#define GPC_PGC_MIPI_PHY	0xc00
#define GPC_PGC_PCIE_PHY	0xc40
#define GPC_PGC_USB_OTG1_PHY	0xc80
#define GPC_PGC_USB_OTG2_PHY	0xcc0
#define GPC_PGC_USB_HSIC_PHY	0xd00

#define BM_LPCR_A7_BSC_CPU_CLK_ON_LPM		0x4000
#define BM_LPCR_A7_BSC_LPM1			0xc
#define BM_LPCR_A7_BSC_LPM0			0x3
#define BP_LPCR_A7_BSC_LPM1			2
#define BP_LPCR_A7_BSC_LPM0			0
#define BM_LPCR_M4_MASK_DSM_TRIGGER		0x80000000
#define BM_SLPCR_EN_DSM				0x80000000
#define BM_SLPCR_VSTBY				0x4
#define BM_SLPCR_SBYOS				0x2
#define BM_SLPCR_BYPASS_PMIC_READY		0x1

#define BM_LPCR_A7_AD_EN_C1_PUP			0x800
#define BM_LPCR_A7_AD_EN_C0_PUP			0x200
#define BM_LPCR_A7_AD_EN_PLAT_PDN		0x10
#define BM_LPCR_A7_AD_EN_C1_PDN			0x8
#define BM_LPCR_A7_AD_EN_C0_PDN			0x2

#define BM_CPU_PGC_SW_PDN_PUP_REQ_CORE1_A7	0x2

enum imx_gpc_slot {
	CORE0_A7,
	CORE1_A7,
	SCU_A7,
	FAST_MEGA_MIX,
	MIPI_PHY,
	PCIE_PHY,
	USB_OTG1_PHY,
	USB_OTG2_PHY,
	USB_HSIC_PHY,
	CORE0_M4,
};

static void __iomem *gpc_base;
static u32 gpcv2_wake_irqs[IMR_NUM];
static u32 gpcv2_saved_imrs[IMR_NUM];
static DEFINE_SPINLOCK(gpcv2_lock);

void imx_gpcv2_set_slot_ack(u32 index, enum imx_gpc_slot m_core,
				bool mode, bool ack)
{
	u32 val;

	if (index > 9)
		pr_err("Invalid slot index!\n");
	/* set slot */
	writel_relaxed((mode + 1) << (m_core * 2), gpc_base +
		GPC_SLOT0_CFG + index * 4);

	if (ack) {
		/* set ack */
		val = readl_relaxed(gpc_base + GPC_PGC_ACK_SEL_A7);
		/* clear dummy ack */
		val &= ~(1 << (15 + (mode ? 16 : 0)));
		val |= 1 << (m_core + (mode ? 16 : 0));
		writel_relaxed(val, gpc_base + GPC_PGC_ACK_SEL_A7);
	}
}

void imx_gpcv2_set_lpm_mode(u32 cpu, enum mxc_cpu_pwr_mode mode)
{
	u32 val1 = readl_relaxed(gpc_base + GPC_LPCR_A7_BSC);
	u32 val2 = readl_relaxed(gpc_base + GPC_SLPCR);

	if (cpu == 0)
		val1 &= ~BM_LPCR_A7_BSC_LPM0;
	if (cpu == 1)
		val1 &= ~BM_LPCR_A7_BSC_LPM1;

	val2 &= ~(BM_SLPCR_EN_DSM | BM_SLPCR_VSTBY |
		BM_SLPCR_SBYOS | BM_SLPCR_BYPASS_PMIC_READY);

	switch (mode) {
	case WAIT_CLOCKED:
		break;
	case WAIT_UNCLOCKED:
		val1 |= 0x1 << (BP_LPCR_A7_BSC_LPM0 + cpu * 2);
		val1 &= ~BM_LPCR_A7_BSC_CPU_CLK_ON_LPM;
		break;
	case STOP_POWER_OFF:
		val1 |= 0x2 << (BP_LPCR_A7_BSC_LPM0 + cpu * 2);
		val1 &= ~BM_LPCR_A7_BSC_CPU_CLK_ON_LPM;
		val2 |= BM_SLPCR_EN_DSM;
		val2 |= BM_SLPCR_SBYOS;
		val2 |= BM_SLPCR_BYPASS_PMIC_READY;
		break;
	default:
		return;
	}

	writel_relaxed(val1, gpc_base + GPC_LPCR_A7_BSC);
	writel_relaxed(val2, gpc_base + GPC_SLPCR);
}

void imx_gpcv2_set_plat_power_gate_by_lpm(void)
{
	u32 val = readl_relaxed(gpc_base + GPC_LPCR_A7_AD);

	val |= BM_LPCR_A7_AD_EN_PLAT_PDN;

	writel_relaxed(val, gpc_base + GPC_LPCR_A7_AD);
}

void imx_gpcv2_set_m_core_pgc(bool enable, u32 offset)
{
	writel_relaxed(enable, gpc_base + offset);
}

void imx_gpcv2_set_core1_pdn_pup_by_software(bool pdn)
{
	u32 val = readl_relaxed(gpc_base + (pdn ?
		GPC_CPU_PGC_SW_PDN_REQ : GPC_CPU_PGC_SW_PUP_REQ));

	imx_gpcv2_set_m_core_pgc(true, GPC_PGC_C1);
	val |= BM_CPU_PGC_SW_PDN_PUP_REQ_CORE1_A7;
	writel_relaxed(val, gpc_base + (pdn ?
		GPC_CPU_PGC_SW_PDN_REQ : GPC_CPU_PGC_SW_PUP_REQ));

	while ((readl_relaxed(gpc_base + (pdn ?
		GPC_CPU_PGC_SW_PDN_REQ : GPC_CPU_PGC_SW_PUP_REQ)) &
		BM_CPU_PGC_SW_PDN_PUP_REQ_CORE1_A7) != 0)
		;
	imx_gpcv2_set_m_core_pgc(false, GPC_PGC_C1);
}

void imx_gpcv2_set_cpu_power_gate_by_lpm(u32 cpu)
{
	u32 val = readl_relaxed(gpc_base + GPC_LPCR_A7_AD);

	if (cpu == 0)
		val |= BM_LPCR_A7_AD_EN_C0_PDN | BM_LPCR_A7_AD_EN_C0_PUP;
	if (cpu == 1)
		val |= BM_LPCR_A7_AD_EN_C1_PDN | BM_LPCR_A7_AD_EN_C1_PUP;

	writel_relaxed(val, gpc_base + GPC_LPCR_A7_AD);
}

void imx_gpcv2_pre_suspend(bool arm_power_off)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1_CORE0;
	int i;

	if (arm_power_off) {
		/* both core0 and core1's lpm need to be set */
		imx_gpcv2_set_lpm_mode(0, STOP_POWER_OFF);
		imx_gpcv2_set_lpm_mode(1, STOP_POWER_OFF);
		/* enable core0 power down/up with low power mode */
		imx_gpcv2_set_cpu_power_gate_by_lpm(0);
		/* enable plat power down with low power mode */
		imx_gpcv2_set_plat_power_gate_by_lpm();

		/* enable cpu0 power down in slot0 */
		imx_gpcv2_set_slot_ack(0, CORE0_A7, false, false);
		/* enable scu power down in slot1, ack for last one */
		imx_gpcv2_set_slot_ack(1, SCU_A7, false, true);


		/* enable scu power up in slot5 */
		imx_gpcv2_set_slot_ack(5, SCU_A7, true, false);
		/* enable core0 power up in slot5, ack for last one */
		imx_gpcv2_set_slot_ack(6, CORE0_A7, true, true);

		/* enable core0, scu */
		writel_relaxed(0x1, gpc_base + GPC_PGC_C0);
		writel_relaxed(0x1, gpc_base + GPC_PGC_SCU);
	}

	for (i = 0; i < IMR_NUM; i++) {
		gpcv2_saved_imrs[i] = readl_relaxed(reg_imr1 + i * 4);
		writel_relaxed(~gpcv2_wake_irqs[i], reg_imr1 + i * 4);
	}
}

void imx_gpcv2_post_resume(void)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1_CORE0;
	int i;

	for (i = 0; i < IMR_NUM; i++)
		writel_relaxed(gpcv2_saved_imrs[i], reg_imr1 + i * 4);

	/* both core0 and core1's lpm need to be set */
	imx_gpcv2_set_lpm_mode(0, WAIT_CLOCKED);
	imx_gpcv2_set_lpm_mode(1, WAIT_CLOCKED);
}

static int imx_gpcv2_irq_set_wake(struct irq_data *d, unsigned int on)
{
	unsigned int idx = d->irq / 32 - 1;
	unsigned long flags;
	u32 mask;

	/* Sanity check for SPI irq */
	if (d->irq < 32)
		return -EINVAL;

	mask = 1 << d->irq % 32;
	spin_lock_irqsave(&gpcv2_lock, flags);
	gpcv2_wake_irqs[idx] = on ? gpcv2_wake_irqs[idx] | mask :
				  gpcv2_wake_irqs[idx] & ~mask;
	spin_unlock_irqrestore(&gpcv2_lock, flags);

	return 0;
}

void imx_gpcv2_mask_all(void)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1_CORE0;
	int i;

	for (i = 0; i < IMR_NUM; i++) {
		gpcv2_saved_imrs[i] = readl_relaxed(reg_imr1 + i * 4);
		writel_relaxed(~0, reg_imr1 + i * 4);
	}
}

void imx_gpcv2_restore_all(void)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1_CORE0;
	int i;

	for (i = 0; i < IMR_NUM; i++)
		writel_relaxed(gpcv2_saved_imrs[i], reg_imr1 + i * 4);
}

void imx_gpcv2_irq_unmask(struct irq_data *d)
{
	void __iomem *reg;
	u32 val;

	/* Sanity check for SPI irq */
	if (d->irq < 32)
		return;

	reg = gpc_base + GPC_IMR1_CORE0 + (d->irq / 32 - 1) * 4;
	val = readl_relaxed(reg);
	val &= ~(1 << d->irq % 32);
	writel_relaxed(val, reg);
}

void imx_gpcv2_irq_mask(struct irq_data *d)
{
	void __iomem *reg;
	u32 val;

	/* Sanity check for SPI irq */
	if (d->irq < 32)
		return;

	reg = gpc_base + GPC_IMR1_CORE0 + (d->irq / 32 - 1) * 4;
	val = readl_relaxed(reg);
	val |= 1 << (d->irq % 32);
	writel_relaxed(val, reg);
}

void __init imx_gpcv2_init(void)
{
	struct device_node *np;
	int i;

	np = of_find_compatible_node(NULL, NULL, "fsl,imx7d-gpc");
	gpc_base = of_iomap(np, 0);
	WARN_ON(!gpc_base);

	/* Initially mask all interrupts */
	for (i = 0; i < IMR_NUM; i++) {
		writel_relaxed(~0, gpc_base + GPC_IMR1_CORE0 + i * 4);
		writel_relaxed(~0, gpc_base + GPC_IMR1_CORE1 + i * 4);
	}

	writel_relaxed(0x0, gpc_base + GPC_LPCR_A7_BSC);
	/* mask m4 dsm trigger */
	writel_relaxed(readl_relaxed(gpc_base + GPC_LPCR_M4) |
		BM_LPCR_M4_MASK_DSM_TRIGGER, gpc_base + GPC_LPCR_M4);

	/* Register GPC as the secondary interrupt controller behind GIC */
	gic_arch_extn.irq_mask = imx_gpcv2_irq_mask;
	gic_arch_extn.irq_unmask = imx_gpcv2_irq_unmask;
	gic_arch_extn.irq_set_wake = imx_gpcv2_irq_set_wake;
}
