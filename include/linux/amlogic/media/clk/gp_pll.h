/*
 * include/linux/amlogic/media/clk/gp_pll.h
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

#ifndef GP_PLL_H
#define GP_PLL_H

#include <linux/kernel.h>
#include <linux/list.h>

#define GP_PLL_USER_FLAG_REQUEST 0x01
#define GP_PLL_USER_FLAG_GRANTED 0x02

#define GP_PLL_USER_EVENT_YIELD  0x01
#define GP_PLL_USER_EVENT_GRANT  0x02

struct gp_pll_user_handle_s {
	struct list_head list;
	const char *name;
	u32 priority;
	int (*callback)(struct gp_pll_user_handle_s *user, int event);
	u32 flag;
};

extern struct gp_pll_user_handle_s *gp_pll_user_register
	(const char *name, u32 priority,
	int (*callback)(struct gp_pll_user_handle_s *, int));

extern void gp_pll_user_unregister(struct gp_pll_user_handle_s *user);

extern void gp_pll_request(struct gp_pll_user_handle_s *user);

extern void gp_pll_release(struct gp_pll_user_handle_s *user);

#endif
