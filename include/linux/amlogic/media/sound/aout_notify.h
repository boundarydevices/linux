/*
 * include/linux/amlogic/media/sound/aout_notify.h
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

#ifndef _AML_NOTIFY_H
#define _AML_NOTIFY_H
#include <linux/notifier.h>
int aout_notifier_call_chain(unsigned long val, void *v);
int aout_unregister_client(struct notifier_block *p);
int aout_register_client(struct notifier_block *p);
#endif /* _AML_NOTIFY_H */
