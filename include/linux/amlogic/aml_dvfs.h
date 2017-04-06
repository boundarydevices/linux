/*
 * include/linux/amlogic/aml_dvfs.h
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

#ifndef __AML_DVFS_H__
#define __AML_DVFS_H__
#include <linux/cpufreq.h>

/*
 * right now only support the flowing types of voltage change
 */
#define AML_DVFS_ID_VCCK		(1 << 0) /* voltage for CPU core */
#define AML_DVFS_ID_VDDEE		(1 << 1) /* voltage for VDDEE */
#define AML_DVFS_ID_DDR			(1 << 2) /* voltage for DDR */

#define AML_DVFS_FREQ_PRECHANGE		0
#define AML_DVFS_FREQ_POSTCHANGE	1

struct aml_dvfs {
	unsigned int freq;	/* frequent of clock source in KHz */
	unsigned int min_uV;	/* min target voltage of this frequent */
	unsigned int max_uV;	/* max target voltage of this frequent */
};

struct aml_dvfs_driver {
	char		*name;
	unsigned int	id_mask;   /* which types of voltage support */

	int (*set_voltage)(uint32_t id, uint32_t min_uV, uint32_t max_uV);
	int (*get_voltage)(uint32_t id, uint32_t *uV);
};

#ifdef CONFIG_AMLOGIC_M8B_DVFS
extern int aml_dvfs_register_driver(struct aml_dvfs_driver *driver);
extern int aml_dvfs_unregister_driver(struct aml_dvfs_driver *driver);
#else
inline int aml_dvfs_register_driver(struct aml_dvfs_driver *driver)
{
	return 0;
}

inline int aml_dvfs_unregister_driver(struct aml_dvfs_driver *driver)
{
	return 0;
}
#endif

#endif  /* __AML_DVFS_H__ */
