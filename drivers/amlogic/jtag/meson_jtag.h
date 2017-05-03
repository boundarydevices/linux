/*
 * drivers/amlogic/jtag/meson_jtag.h
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

#ifndef __AML_JTAG_H
#define __AML_JTAG_H

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>

#define CLUSTER_BIT		2
#define	CLUSTER_DISABLE		(-1)

struct aml_jtag_dev {
	struct platform_device *pdev;
	struct class cls;

#ifdef CONFIG_MACH_MESON8B
	void __iomem *base;
#endif

	struct notifier_block notifier;

	unsigned int old_select;
	unsigned int select;
	unsigned int cluster;

	const unsigned int *ao_gpios;
	const unsigned int *ee_gpios;

	int ao_ngpios;
	int ee_ngpios;

};

#endif
