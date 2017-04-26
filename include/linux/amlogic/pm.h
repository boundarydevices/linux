/*
 * include/linux/amlogic/pm.h
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

#ifndef __AML_PM_H__
#define __AML_PM_H__
#include <linux/notifier.h>

/* wake up reason*/
#define	UDEFINED_WAKEUP	0
#define	CHARGING_WAKEUP	1
#define	REMOTE_WAKEUP		2
#define	RTC_WAKEUP			3
#define	BT_WAKEUP			4
#define	WIFI_WAKEUP			5
#define	POWER_KEY_WAKEUP	6
#define	AUTO_WAKEUP			7
#define	CEC_WAKEUP			8
#define	REMOTE_CUS_WAKEUP		9
#define ETH_PHY_WAKEUP      10
extern unsigned int get_resume_method(void);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
enum {
	EARLY_SUSPEND_LEVEL_BLANK_SCREEN = 50,
	EARLY_SUSPEND_LEVEL_STOP_DRAWING = 100,
	EARLY_SUSPEND_LEVEL_DISABLE_FB = 150,
};

struct early_suspend {
	struct list_head link;
	int level;
	void (*suspend)(struct early_suspend *h);
	void (*resume)(struct early_suspend *h);
	void *param;
};
extern void register_early_suspend(struct early_suspend *handler);
extern void unregister_early_suspend(struct early_suspend *handler);

#endif //CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#endif
