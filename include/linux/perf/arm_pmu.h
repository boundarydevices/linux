/*
 *  linux/arch/arm/include/asm/pmu.h
 *
 *  Copyright (C) 2009 picoChip Designs Ltd, Jamie Iles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __ARM_PMU_H__
#define __ARM_PMU_H__

#include <linux/interrupt.h>
#include <linux/perf_event.h>
#include <linux/sysfs.h>
#include <asm/cputype.h>

/*
 * struct arm_pmu_platdata - ARM PMU platform data
 *
 * @handle_irq: an optional handler which will be called from the
 *	interrupt and passed the address of the low level handler,
 *	and can be used to implement any platform specific handling
 *	before or after calling it.
 */
struct arm_pmu_platdata {
	irqreturn_t (*handle_irq)(int irq, void *dev,
				  irq_handler_t pmu_handler);
};

#ifdef CONFIG_ARM_PMU

/*
 * The ARMv7 CPU PMU supports up to 32 event counters.
 */
#define ARMPMU_MAX_HWEVENTS		32

#define HW_OP_UNSUPPORTED		0xFFFF
#define C(_x)				PERF_COUNT_HW_CACHE_##_x
#define CACHE_OP_UNSUPPORTED		0xFFFF

#define PERF_MAP_ALL_UNSUPPORTED					\
	[0 ... PERF_COUNT_HW_MAX - 1] = HW_OP_UNSUPPORTED

#define PERF_CACHE_MAP_ALL_UNSUPPORTED					\
[0 ... C(MAX) - 1] = {							\
	[0 ... C(OP_MAX) - 1] = {					\
		[0 ... C(RESULT_MAX) - 1] = CACHE_OP_UNSUPPORTED,	\
	},								\
}

/* The events for a given PMU register set. */
struct pmu_hw_events {
	/*
	 * The events that are active on the PMU for the given index.
	 */
	struct perf_event	*events[ARMPMU_MAX_HWEVENTS];

	/*
	 * A 1 bit for an index indicates that the counter is being used for
	 * an event. A 0 means that the counter can be used.
	 */
	DECLARE_BITMAP(used_mask, ARMPMU_MAX_HWEVENTS);

	/*
	 * Hardware lock to serialize accesses to PMU registers. Needed for the
	 * read/modify/write sequences.
	 */
	raw_spinlock_t		pmu_lock;

	/*
	 * When using percpu IRQs, we need a percpu dev_id. Place it here as we
	 * already have to allocate this struct per cpu.
	 */
	struct arm_pmu		*percpu_pmu;
};

enum armpmu_attr_groups {
	ARMPMU_ATTR_GROUP_COMMON,
	ARMPMU_ATTR_GROUP_EVENTS,
	ARMPMU_ATTR_GROUP_FORMATS,
	ARMPMU_NR_ATTR_GROUPS
};

struct arm_pmu {
	struct pmu	pmu;
	cpumask_t	active_irqs;
	cpumask_t	supported_cpus;
	int		*irq_affinity;
	char		*name;
	irqreturn_t	(*handle_irq)(int irq_num, void *dev);
	void		(*enable)(struct perf_event *event);
	void		(*disable)(struct perf_event *event);
	int		(*get_event_idx)(struct pmu_hw_events *hw_events,
					 struct perf_event *event);
	void		(*clear_event_idx)(struct pmu_hw_events *hw_events,
					 struct perf_event *event);
	int		(*set_event_filter)(struct hw_perf_event *evt,
					    struct perf_event_attr *attr);
	u32		(*read_counter)(struct perf_event *event);
	void		(*write_counter)(struct perf_event *event, u32 val);
	void		(*start)(struct arm_pmu *);
	void		(*stop)(struct arm_pmu *);
	void		(*reset)(void *);
	int		(*request_irq)(struct arm_pmu *, irq_handler_t handler);
	void		(*free_irq)(struct arm_pmu *);
	int		(*map_event)(struct perf_event *event);
	int		num_events;
	atomic_t	active_events;
	struct mutex	reserve_mutex;
	u64		max_period;
	bool		secure_access; /* 32-bit ARM only */
#define ARMV8_PMUV3_MAX_COMMON_EVENTS 0x40
	DECLARE_BITMAP(pmceid_bitmap, ARMV8_PMUV3_MAX_COMMON_EVENTS);
	struct platform_device	*plat_device;
	struct pmu_hw_events	__percpu *hw_events;
	struct hlist_node	node;
	struct notifier_block	cpu_pm_nb;
	/* the attr_groups array must be NULL-terminated */
	const struct attribute_group *attr_groups[ARMPMU_NR_ATTR_GROUPS + 1];
};

#define to_arm_pmu(p) (container_of(p, struct arm_pmu, pmu))

u64 armpmu_event_update(struct perf_event *event);

int armpmu_event_set_period(struct perf_event *event);

int armpmu_map_event(struct perf_event *event,
		     const unsigned (*event_map)[PERF_COUNT_HW_MAX],
		     const unsigned (*cache_map)[PERF_COUNT_HW_CACHE_MAX]
						[PERF_COUNT_HW_CACHE_OP_MAX]
						[PERF_COUNT_HW_CACHE_RESULT_MAX],
		     u32 raw_event_mask);

struct pmu_probe_info {
	unsigned int cpuid;
	unsigned int mask;
	int (*init)(struct arm_pmu *);
};

#define PMU_PROBE(_cpuid, _mask, _fn)	\
{					\
	.cpuid = (_cpuid),		\
	.mask = (_mask),		\
	.init = (_fn),			\
}

#define ARM_PMU_PROBE(_cpuid, _fn) \
	PMU_PROBE(_cpuid, ARM_CPU_PART_MASK, _fn)

#define ARM_PMU_XSCALE_MASK	((0xff << 24) | ARM_CPU_XSCALE_ARCH_MASK)

#define XSCALE_PMU_PROBE(_version, _fn) \
	PMU_PROBE(ARM_CPU_IMP_INTEL << 24 | _version, ARM_PMU_XSCALE_MASK, _fn)

int arm_pmu_device_probe(struct platform_device *pdev,
			 const struct of_device_id *of_table,
			 const struct pmu_probe_info *probe_table);

#define ARMV8_PMU_PDEV_NAME "armv8-pmu"

#ifdef CONFIG_AMLOGIC_MODIFY
#define MAX_DELTA_CNT 4
struct amlpmu_cpuinfo {
	int irq_num;

	/*
	 * In interrupt generated cpu(affinity cpu)
	 * If pmu no overflowed, then we need to send IPI to some other cpus to
	 * fix it. And before send IPI, set corresponding cpu's fix_done and
	 * fix_overflowed to zero, in corresponding cpu's IPI interrupt will set
	 * fix_done to inform source cpu and if indeed pmu overflowed then also
	 * set fix_overflowed to 1, then inerrupt generated cpu can feel that.
	 */
	int fix_done;
	int fix_overflowed;

	/* for interrupt affinity prediction */
	ktime_t last_stamp;
	s64 stamp_deltas[MAX_DELTA_CNT];
	s64 avg_delta;
	ktime_t next_predicted_stamp;

	/*
	 * irq state account of this cpu
	 *
	 * - valid_irq_cnt:
	 *   valid irq cnt.(pmu overflow happened)
	 * - fix_irq_cnt:
	 *   when this cpu is pmu interrupt generated affinity cpu, a pmu
	 *   interrupt if cpu affinity predict failed so no pmu overflow
	 *   happened and succeeded send IPI to other cpu, then it's a send
	 *   fix irq. So the lower is better.
	 * - empty_irq_cnt:
	 *   when this cpu is pmu interrupt generated affinity cpu, a pmu
	 *   interrupt that no overflow happened and also no fix IPI sended to
	 *   other cpus, then it's a empty irq.
	 *   when this cpu is not affinity cpu, a IPI interrupt(pmu fix from
	 *   affinity cpu) that no pmu overflow happened, it's a empty irq.
	 *
	 *   attention:
	 *   A interrupt can be a valid_irq and also a fix_irq.
	 */
	unsigned long valid_irq_cnt;
	unsigned long fix_irq_cnt;
	unsigned long empty_irq_cnt;

	unsigned long valid_irq_time;
	unsigned long fix_irq_time;
	unsigned long empty_irq_time;

	unsigned long last_valid_irq_cnt;
	unsigned long last_fix_irq_cnt;
	unsigned long last_empty_irq_cnt;

	unsigned long last_valid_irq_time;
	unsigned long last_fix_irq_time;
	unsigned long last_empty_irq_time;
};


#define MAX_CLUSTER_NR 2
struct amlpmu_context {
	struct amlpmu_cpuinfo __percpu *cpuinfo;

	/* struct arm_pmu */
	struct arm_pmu *pmu;

	int clusterb_enabled;

	unsigned int __iomem *regs[MAX_CLUSTER_NR];
	int irqs[MAX_CLUSTER_NR];
	struct cpumask cpumasks[MAX_CLUSTER_NR];
	int first_cpus[MAX_CLUSTER_NR];

	/*
	 * In main pmu irq route wait for other cpu fix done may cause lockup,
	 * when lockup we disable main irq for a while.
	 * relax_timer will enable main irq again.
	 */
	struct hrtimer relax_timer;

	unsigned int relax_timer_ns;
	unsigned int max_wait_cnt;
};

extern struct amlpmu_context amlpmu_ctx;

int amlpmu_handle_irq(struct arm_pmu *cpu_pmu, int irq_num, int has_overflowed);

/* defined int arch/arm(64)/kernel/perf_event(_v7).c */
void amlpmu_handle_irq_ipi(void *arg);
#endif

#endif /* CONFIG_ARM_PMU */

#endif /* __ARM_PMU_H__ */
