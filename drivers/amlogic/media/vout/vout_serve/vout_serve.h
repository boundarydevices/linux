/*
 * drivers/amlogic/media/vout/vout_serve/vout_serve.h
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

#ifndef _VOUT_SERVE_H_
#define _VOUT_SERVE_H_

#include <stdarg.h>
#include <linux/printk.h>

#define VOUTPR(fmt, args...)     pr_info("vout: "fmt"", ## args)
#define VOUTERR(fmt, args...)    pr_err("vout: error: "fmt"", ## args)

#endif
