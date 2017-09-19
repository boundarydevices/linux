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
extern unsigned int is_pm_freeze_mode(void);

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
extern unsigned int lgcy_early_suspend_init(void);

#endif //CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND

#ifdef CONFIG_AMLOGIC_M8B_SUSPEND
/*l2c virtual addr*/
#define IO_PL310_BASE 0xfe000000

/*IR, power key, low power,
 *adapter plug in/out and so on,
 *are all use this flag.
 */
#define FLAG_WAKEUP_PWRKEY		0x1234abcd
#define FLAG_WAKEUP_ALARM		0x12345678
#define FLAG_WAKEUP_WIFI		0x12340001
#define FLAG_WAKEUP_BT			0x12340002
#define FLAG_WAKEUP_PWROFF		0x12340003

/*AOBUS*/
#define AO_RTI_STATUS_REG2 0x0008
#define AO_RTC_ADDR0 0x0740
#define AO_RTC_ADDR1 0x0744
#define AO_RTC_ADDR2 0x0748
#define AO_RTC_ADDR3 0x074c
#define AO_UART_STATUS 0x04cc
#define AO_UART_REG5   0x04d4

/*CBUS*/
#define HHI_SYS_PLL_CNTL  0x10c0
#define HHI_MPEG_CLK_CNTL 0x105d

#endif //CONFIG_AMLOGIC_M8B_SUSPEND

#endif //__AML_PM_H__
