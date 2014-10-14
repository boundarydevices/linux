/*
 *  linux/arch/arm/common/gic.c
 *
 *  Copyright (C) 2002 ARM Limited, All Rights Reserved.
 *  Copyright (C) 2014 Stan Tomlinson, Persistent Systems <stomlinson@persistentsystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Interrupt architecture for the GIC:
 *
 * o There is one Interrupt Distributor, which receives interrupts
 *   from system devices and sends them to the Interrupt Controllers.
 *
 * o There is one CPU Interface per CPU, which sends interrupts sent
 *   by the Distributor, and interrupts generated locally, to the
 *   associated CPU. The base address of the CPU interface is usually
 *   aliased so that the same address points to different chips depending
 *   on the CPU it is accessed from.
 *
 * Note that IRQs 0-31 are special - they are local to each CPU.
 * As such, the enable set/clear, pending set/clear and active bit
 * registers are banked per-cpu for these sources.
 *
 * The GIC_FIQ sections enable multiple groups in the GIC hardware
 * so that a FIQ interrupt can be utilized.  The gic_enable_fiq
 * routine can be used to reassign one or more interrupts to be
 * handled by the FIQ interrupt routine.
 *
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/io.h>

#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/hardware/gic.h>

static DEFINE_SPINLOCK(irq_controller_lock);

/* Address of GIC 0 CPU interface */
void __iomem *gic_cpu_base_addr __read_mostly;

struct gic_chip_data {
	unsigned int irq_offset;
	void __iomem *dist_base;
	void __iomem *cpu_base;
};

/*
 * Supported arch specific GIC irq extension.
 * Default make them NULL.
 */
struct irq_chip gic_arch_extn = {
	.irq_eoi	= NULL,
	.irq_mask	= NULL,
	.irq_unmask	= NULL,
	.irq_retrigger	= NULL,
	.irq_set_type	= NULL,
	.irq_set_wake	= NULL,
};

#ifndef MAX_GIC_NR
#define MAX_GIC_NR	1
#endif

static struct gic_chip_data gic_data[MAX_GIC_NR] __read_mostly;

static inline void __iomem *gic_dist_base(struct irq_data *d)
{
	struct gic_chip_data *gic_data = irq_data_get_irq_chip_data(d);
	return gic_data->dist_base;
}

static inline void __iomem *gic_cpu_base(struct irq_data *d)
{
	struct gic_chip_data *gic_data = irq_data_get_irq_chip_data(d);
	return gic_data->cpu_base;
}

static inline unsigned int gic_irq(struct irq_data *d)
{
	struct gic_chip_data *gic_data = irq_data_get_irq_chip_data(d);
	return d->irq - gic_data->irq_offset;
}

/*
 * Routines to acknowledge, disable and enable interrupts
 */
static void gic_mask_irq(struct irq_data *d)
{
	u32 mask = 1 << (d->irq % 32);
	spin_lock(&irq_controller_lock);
	writel_relaxed(mask, gic_dist_base(d) + GIC_DIST_ENABLE_CLEAR + (gic_irq(d) / 32) * 4);
	if (gic_arch_extn.irq_mask)
		gic_arch_extn.irq_mask(d);
	spin_unlock(&irq_controller_lock);
}

static void gic_unmask_irq(struct irq_data *d)
{
	u32 mask = 1 << (d->irq % 32);
	spin_lock(&irq_controller_lock);
	if (gic_arch_extn.irq_unmask)
		gic_arch_extn.irq_unmask(d);
	writel_relaxed(mask, gic_dist_base(d) + GIC_DIST_ENABLE_SET + (gic_irq(d) / 32) * 4);
	spin_unlock(&irq_controller_lock);
}

static void gic_eoi_irq(struct irq_data *d)
{
	if (gic_arch_extn.irq_eoi) {
		spin_lock(&irq_controller_lock);
		gic_arch_extn.irq_eoi(d);
		spin_unlock(&irq_controller_lock);
	}

	writel_relaxed(gic_irq(d), gic_cpu_base(d) + GIC_CPU_EOI);
}

static int gic_set_type(struct irq_data *d, unsigned int type)
{
	void __iomem *base = gic_dist_base(d);
	unsigned int gicirq = gic_irq(d);
	u32 enablemask = 1 << (gicirq % 32);
	u32 enableoff = (gicirq / 32) * 4;
	u32 confmask = 0x2 << ((gicirq % 16) * 2);
	u32 confoff = (gicirq / 16) * 4;
	bool enabled = false;
	u32 val;

	/* Interrupt configuration for SGIs can't be changed */
	if (gicirq < 16)
		return -EINVAL;

	if (type != IRQ_TYPE_LEVEL_HIGH && type != IRQ_TYPE_EDGE_RISING)
		return -EINVAL;

	spin_lock(&irq_controller_lock);

	if (gic_arch_extn.irq_set_type)
		gic_arch_extn.irq_set_type(d, type);

	val = readl_relaxed(base + GIC_DIST_CONFIG + confoff);
	if (type == IRQ_TYPE_LEVEL_HIGH)
		val &= ~confmask;
	else if (type == IRQ_TYPE_EDGE_RISING)
		val |= confmask;

	/*
	 * As recommended by the spec, disable the interrupt before changing
	 * the configuration
	 */
	if (readl_relaxed(base + GIC_DIST_ENABLE_SET + enableoff) & enablemask) {
		writel_relaxed(enablemask, base + GIC_DIST_ENABLE_CLEAR + enableoff);
		enabled = true;
	}

	writel_relaxed(val, base + GIC_DIST_CONFIG + confoff);

	if (enabled)
		writel_relaxed(enablemask, base + GIC_DIST_ENABLE_SET + enableoff);

	spin_unlock(&irq_controller_lock);

	return 0;
}

static int gic_retrigger(struct irq_data *d)
{
	if (gic_arch_extn.irq_retrigger)
		return gic_arch_extn.irq_retrigger(d);

	return -ENXIO;
}

#ifdef CONFIG_SMP
static int gic_set_affinity(struct irq_data *d, const struct cpumask *mask_val,
			    bool force)
{
	void __iomem *reg = gic_dist_base(d) + GIC_DIST_TARGET + (gic_irq(d) & ~3);
	unsigned int shift = (d->irq % 4) * 8;
	unsigned int cpu = cpumask_first(mask_val);
	u32 val, mask, bit;

	if (cpu >= 8)
		return -EINVAL;

	mask = 0xff << shift;
	bit = 1 << (cpu + shift);

	spin_lock(&irq_controller_lock);
	val = readl_relaxed(reg) & ~mask;
	writel_relaxed(val | bit, reg);
	spin_unlock(&irq_controller_lock);

	return 0;
}
#endif

#ifdef CONFIG_PM
static int gic_set_wake(struct irq_data *d, unsigned int on)
{
	int ret = -ENXIO;

	if (gic_arch_extn.irq_set_wake)
		ret = gic_arch_extn.irq_set_wake(d, on);

	return ret;
}

#else
#define gic_set_wake	NULL
#endif

static void gic_handle_cascade_irq(unsigned int irq, struct irq_desc *desc)
{
	struct gic_chip_data *chip_data = irq_get_handler_data(irq);
	struct irq_chip *chip = irq_get_chip(irq);
	unsigned int cascade_irq, gic_irq;
	unsigned long status;

	chained_irq_enter(chip, desc);

	spin_lock(&irq_controller_lock);
	status = readl_relaxed(chip_data->cpu_base + GIC_CPU_INTACK);
	spin_unlock(&irq_controller_lock);

	gic_irq = (status & 0x3ff);
	if (gic_irq == 1023)
		goto out;

	cascade_irq = gic_irq + chip_data->irq_offset;
	if (unlikely(gic_irq < 32 || gic_irq > 1020 || cascade_irq >= NR_IRQS))
		do_bad_IRQ(cascade_irq, desc);
	else
		generic_handle_irq(cascade_irq);

 out:
	chained_irq_exit(chip, desc);
}

static struct irq_chip gic_chip = {
	.name			= "GIC",
	.irq_mask		= gic_mask_irq,
	.irq_unmask		= gic_unmask_irq,
	.irq_eoi		= gic_eoi_irq,
	.irq_set_type		= gic_set_type,
	.irq_retrigger		= gic_retrigger,
#ifdef CONFIG_SMP
	.irq_set_affinity	= gic_set_affinity,
#endif
	.irq_set_wake		= gic_set_wake,
};

void __cpuinit gic_cascade_irq(unsigned int gic_nr, unsigned int irq)
{
	if (gic_nr >= MAX_GIC_NR)
		BUG();
	if (irq_set_handler_data(irq, &gic_data[gic_nr]) != 0)
		BUG();
	irq_set_chained_handler(irq, gic_handle_cascade_irq);
}

static void __cpuinit gic_dist_init(struct gic_chip_data *gic,
	unsigned int irq_start)
{
	unsigned int gic_irqs, irq_limit, i;
	void __iomem *dist_base = gic->dist_base;
	u32 cpumask = 1 << smp_processor_id();

	cpumask |= cpumask << 8;
	cpumask |= cpumask << 16;

	writel_relaxed(0, dist_base + GIC_DIST_CTRL);

	/*
	 * Find out how many interrupts are supported.
	 * The GIC only supports up to 1020 interrupt sources.
	 */
	gic_irqs = readl_relaxed(dist_base + GIC_DIST_TYPER) & 0x1f;
	gic_irqs = (gic_irqs + 1) * 32;
	if (gic_irqs > 1020)
		gic_irqs = 1020;

#ifdef CONFIG_GIC_FIQ
	/*
	 * move interrupts into secure mode (Group 1 in v2)
	 */
	for (i = 32; i < gic_irqs; i += 32 )
		writel_relaxed(0xffffffff, dist_base + GIC_DIST_GROUP +  i * 4 / 32);
#endif

	/*
	 * Set all global interrupts to be level triggered, active low.
	 */
	for (i = 32; i < gic_irqs; i += 16)
		writel_relaxed(0, dist_base + GIC_DIST_CONFIG + i * 4 / 16);

	/*
	 * Set all global interrupts to this CPU only.
	 */
	for (i = 32; i < gic_irqs; i += 4)
		writel_relaxed(cpumask, dist_base + GIC_DIST_TARGET + i * 4 / 4);

	/*
	 * Set priority on all global interrupts.
	 */
	for (i = 32; i < gic_irqs; i += 4)
		writel_relaxed(0xa0a0a0a0, dist_base + GIC_DIST_PRI + i * 4 / 4);

	/*
	 * Disable all interrupts.  Leave the PPI and SGIs alone
	 * as these enables are banked registers.
	 */
	for (i = 32; i < gic_irqs; i += 32)
		writel_relaxed(0xffffffff, dist_base + GIC_DIST_ENABLE_CLEAR + i * 4 / 32);

	/*
	 * Limit number of interrupts registered to the platform maximum
	 */
	irq_limit = gic->irq_offset + gic_irqs;
	if (WARN_ON(irq_limit > NR_IRQS))
		irq_limit = NR_IRQS;

	/*
	 * Setup the Linux IRQ subsystem.
	 */
	for (i = irq_start; i < irq_limit; i++) {
		irq_set_chip_and_handler(i, &gic_chip, handle_fasteoi_irq);
		irq_set_chip_data(i, gic);
		set_irq_flags(i, IRQF_VALID | IRQF_PROBE);
	}

#ifdef CONFIG_GIC_FIQ
	writel_relaxed(3, dist_base + GIC_DIST_CTRL);
#else
	writel_relaxed(1, dist_base + GIC_DIST_CTRL);
#endif
}

static void __cpuinit gic_cpu_init(struct gic_chip_data *gic)
{
	void __iomem *dist_base = gic->dist_base;
	void __iomem *cpu_base = gic->cpu_base;
	int i;

	/*
	 * move interrupts into secure mode (Group 1 in v2)
	 */
	for (i = 0; i < 32; i += 32 )
		writel_relaxed(0xffffffff, dist_base + GIC_DIST_GROUP +  i * 4 / 32);

	/*
	 * Deal with the banked PPI and SGI interrupts - disable all
	 * PPI interrupts, ensure all SGI interrupts are enabled.
	 */
	writel_relaxed(0xffff0000, dist_base + GIC_DIST_ENABLE_CLEAR);
	writel_relaxed(0x0000ffff, dist_base + GIC_DIST_ENABLE_SET);

	/*
	 * Set priority on PPI and SGI interrupts
	 */
	for (i = 0; i < 32; i += 4)
		writel_relaxed(0xa0a0a0a0, dist_base + GIC_DIST_PRI + i * 4 / 4);
	writel_relaxed(0xf0, cpu_base + GIC_CPU_PRIMASK);

#define SBPR   0x10
#define FIQEN  0x08
#define AckCtl 0x04
#define GRP1   0x02
#define GRP0   0x01

#ifdef CONFIG_GIC_FIQ
	writel_relaxed(GRP0|GRP1|AckCtl|FIQEN|SBPR, cpu_base + GIC_CPU_CTRL);
#else
	writel_relaxed(GRP0, cpu_base + GIC_CPU_CTRL);	// enable Group 0
#endif

#undef SBPR
#undef FIQEN
#undef AckCtl
#undef GRP1
#undef GRP0
}

void __cpuinit gic_init(unsigned int gic_nr, unsigned int irq_start,
	void __iomem *dist_base, void __iomem *cpu_base)
{
	struct gic_chip_data *gic;
#ifdef CONFIG_GIC_FIQ
	int icpid2;
	int gic_dist_typer;
	int gicV1;
	int gicV2;
	int gicSE;
#endif

	BUG_ON(gic_nr >= MAX_GIC_NR);

	gic = &gic_data[gic_nr];
	gic->dist_base = dist_base;
	gic->cpu_base = cpu_base;
	gic->irq_offset = (irq_start - 1) & ~31;

	if (gic_nr == 0)
		gic_cpu_base_addr = cpu_base;

#ifdef CONFIG_GIC_FIQ
	icpid2 = __raw_readl(dist_base + GIC_ICPIDR2);
	gic_dist_typer = __raw_readl(dist_base + GIC_DIST_TYPER);
	gicV1 = (icpid2 & GIC_ICPIDR2_ARCHREV_MASK) == GIC_ICPIDR2_ARCHREV_V1;
	gicV2 = (icpid2 & GIC_ICPIDR2_ARCHREV_MASK) == GIC_ICPIDR2_ARCHREV_V2;
	gicSE = (gic_dist_typer & GIC_DIST_TYPER_SECURITY_EXTN) == GIC_DIST_TYPER_SECURITY_EXTN;
	if ( !gicV1 && !gicV2 )	{
		// hardware sanity check -- cannot use if not a known GIC
		printk( "GIC_ICPIDR2_ARCHREV is: %08x\n", icpid2 & GIC_ICPIDR2_ARCHREV_MASK );
		printk( "   must be either %08x or %08x\n", GIC_ICPIDR2_ARCHREV_V1, GIC_ICPIDR2_ARCHREV_V2 );
		printk( "   error: this means GIC is neither version 1 or version 2\n" );
		BUG();
	}
	if ( gicV1 && !gicSE )	{
		// hardware sanity check -- cannot use if not a known GIC
		printk( "GIC_ICPIDR2: %08x\n", icpid2 );
		printk( "GIC_DIST_TYPER: %08x\n", gicSE );
		printk( "   error: GIC is V1 and does not have Security Extensions (so no FIQ available)\n" );
		BUG();
	}
#endif

	gic_dist_init(gic, irq_start);
	gic_cpu_init(gic);
}

void __cpuinit gic_secondary_init(unsigned int gic_nr)
{
	BUG_ON(gic_nr >= MAX_GIC_NR);

	gic_cpu_init(&gic_data[gic_nr]);
}

#ifdef CONFIG_GIC_FIQ
//
// gic_enable_fiq should probably have a generic
// name or be a part of an abstraction structure
// for ARM chips, so that different irq_chips
// might support the same functionality.
//

//
// set irq to use the FIQ interrupt handler
//
void gic_enable_fiq(unsigned int irq, int enable)
{
	unsigned int cpu;
	unsigned int group_offset, group_bit;
	unsigned int pri_offset, pri_word;
	unsigned int V;
	struct gic_chip_data *gic;
	void __iomem *dist_base;

	static unsigned int pri_words[4] = {
		0xa0a0a090,
		0xa0a090a0,
		0xa090a0a0,
		0x90a0a0a0
		};

	local_fiq_enable();

	group_offset = (irq >> 5) * 4;
	group_bit = 1 << (irq & 31);

	pri_offset = (irq >> 2) * 4;
	pri_word = enable ? pri_words[irq & 3] : 0xa0a0a0a0;

	for_each_possible_cpu(cpu)
	{
		gic = &gic_data[cpu];
		dist_base = gic->dist_base;

		V = __raw_readl(dist_base + GIC_DIST_GROUP + group_offset);
		if ( enable )
			V &= ~group_bit;
		else
			V |= group_bit;
		writel_relaxed(V, dist_base + GIC_DIST_GROUP + group_offset );

		writel_relaxed(pri_word, dist_base + GIC_DIST_PRI + pri_offset);	// set the priority
	}
}
EXPORT_SYMBOL(gic_enable_fiq);
#endif

void __cpuinit gic_enable_ppi(unsigned int irq)
{
	unsigned long flags;

	local_irq_save(flags);
	irq_set_status_flags(irq, IRQ_NOPROBE);
	gic_unmask_irq(irq_get_irq_data(irq));
	local_irq_restore(flags);
}
void __cpuinit gic_disable_ppi(unsigned int irq)
{
	unsigned long flags;

	local_irq_save(flags);
	irq_set_status_flags(irq, IRQ_NOPROBE);
	gic_mask_irq(irq_get_irq_data(irq));
	local_irq_restore(flags);
}

void save_gic_cpu_state(unsigned int gic_nr, struct gic_cpu_state *gcs)
{
	BUG_ON(gic_nr >= MAX_GIC_NR);

	gcs->iccicr = __raw_readl(gic_data[gic_nr].cpu_base + GIC_CPU_CTRL);
	gcs->iccpmr = __raw_readl(gic_data[gic_nr].cpu_base + GIC_CPU_PRIMASK);
	gcs->iccbpr = __raw_readl(gic_data[gic_nr].cpu_base + GIC_CPU_BINPOINT);
	gcs->iccabpr = __raw_readl(gic_data[gic_nr].cpu_base + GIC_CPU_ABINPOINT);
}

void restore_gic_cpu_state(unsigned int gic_nr, struct gic_cpu_state *gcs)
{
	BUG_ON(gic_nr >= MAX_GIC_NR);

	__raw_writel(gcs->iccpmr, gic_data[gic_nr].cpu_base + GIC_CPU_PRIMASK);
	__raw_writel(gcs->iccbpr, gic_data[gic_nr].cpu_base + GIC_CPU_BINPOINT);
	__raw_writel(gcs->iccabpr, gic_data[gic_nr].cpu_base + GIC_CPU_ABINPOINT);

	/* at last, restore ctrl register */
	__raw_writel(gcs->iccicr, gic_data[gic_nr].cpu_base + GIC_CPU_CTRL);
}

void save_gic_dist_state(unsigned int gic_nr, struct gic_dist_state *gds)
{
	unsigned int gic_irqs, i;

	BUG_ON(gic_nr >= MAX_GIC_NR);

	gic_irqs = readl(gic_data[gic_nr].dist_base + GIC_DIST_TYPER) & 0x1f;
	gic_irqs = (gic_irqs + 1) * 32;
	if (gic_irqs > 1020)
		gic_irqs = 1020;

	gds->icddcr = __raw_readl(gic_data[gic_nr].dist_base + GIC_DIST_CTRL);

	/* save interrupt enable status */
	for (i = 0; i < gic_irqs; i += 32)
		gds->icdisern[i / 32] = __raw_readl(gic_data[gic_nr].dist_base
					+ GIC_DIST_ENABLE_SET + i * 4 / 32);

	/* save interrupt pending status */
	for (i = 0; i < gic_irqs; i += 32)
		gds->icdisprn[i / 32] = __raw_readl(gic_data[gic_nr].dist_base
					+ GIC_DIST_PENDING_SET + i * 4 / 32);

	/* save active bit status */
	for (i = 0; i < gic_irqs; i += 32)
		gds->icdabrn[i / 32] = __raw_readl(gic_data[gic_nr].dist_base
					+ GIC_DIST_ACTIVE_BIT + i * 4 / 32);

	/* interrupt priority registers */
	for (i = 0; i < gic_irqs; i += 4)
		gds->icdiprn[i / 4] = __raw_readl(gic_data[gic_nr].dist_base
					+ GIC_DIST_PRI + i * 4 / 4);

	/* interrupt processor targets registers */
	for (i = 0; i < gic_irqs; i += 4)
		gds->icdiptrn[i / 4] = __raw_readl(gic_data[gic_nr].dist_base
					+ GIC_DIST_TARGET + i * 4 / 4);

	/* interrupt configuration registers */
	for (i = 0; i < gic_irqs; i += 16)
		gds->icdicfrn[i / 16] = __raw_readl(gic_data[gic_nr].dist_base
					+ GIC_DIST_CONFIG + i * 4 / 16);
}

void restore_gic_dist_state(unsigned int gic_nr, struct gic_dist_state *gds)
{
	unsigned int gic_irqs, i;

	BUG_ON(gic_nr >= MAX_GIC_NR);

	gic_irqs = readl(gic_data[gic_nr].dist_base + GIC_DIST_TYPER) & 0x1f;
	gic_irqs = (gic_irqs + 1) * 32;
	if (gic_irqs > 1020)
		gic_irqs = 1020;

	__raw_writel(0, gic_data[gic_nr].dist_base + GIC_DIST_CTRL);

	/* interrupt configuration registers */
	for (i = 0; i < gic_irqs; i += 16)
		 __raw_writel(gds->icdicfrn[i / 16], gic_data[gic_nr].dist_base
					+ GIC_DIST_CONFIG + i * 4 / 16);

	/* interrupt priority registers */
	for (i = 0; i < gic_irqs; i += 4)
		 __raw_writel(gds->icdiprn[i / 4], gic_data[gic_nr].dist_base
					+ GIC_DIST_PRI + i * 4 / 4);

	/* interrupt processor targets registers */
	for (i = 0; i < gic_irqs; i += 4)
		 __raw_writel(gds->icdiptrn[i / 4], gic_data[gic_nr].dist_base
					+ GIC_DIST_TARGET + i * 4 / 4);

	/* restore active bits */
	for (i = 0; i < gic_irqs; i += 32)
		__raw_writel(gds->icdabrn[i / 32], gic_data[gic_nr].dist_base
					+ GIC_DIST_ACTIVE_BIT + i * 4 / 32);

	/* restore pending bits */
	for (i = 0; i < gic_irqs; i += 32)
		__raw_writel(gds->icdisprn[i / 32], gic_data[gic_nr].dist_base
					+ GIC_DIST_PENDING_SET + i * 4 / 32);

	/* restore interrupt enable status */
	for (i = 0; i < gic_irqs; i += 32)
		__raw_writel(gds->icdisern[i / 32], gic_data[gic_nr].dist_base
					+ GIC_DIST_ENABLE_SET + i * 4 / 32);

	/* at last restore ctrl register */
	__raw_writel(gds->icddcr, gic_data[gic_nr].dist_base + GIC_DIST_CTRL);
}

#ifdef CONFIG_SMP
void gic_raise_softirq(const struct cpumask *mask, unsigned int irq)
{
	unsigned long map = *cpus_addr(*mask);

	/*
	 * Ensure that stores to Normal memory are visible to the
	 * other CPUs before issuing the IPI.
	 */
	dsb();

	/* this always happens on GIC0 */
	writel_relaxed(map << 16 | irq, gic_data[0].dist_base + GIC_DIST_SOFTINT);
}
#endif
