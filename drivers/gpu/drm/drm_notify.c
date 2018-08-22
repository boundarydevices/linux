// SPDX-License-Identifier: (GPL-2.0+)
/*
 *  linux/drivers/gpu/drm/drm_notify.c
 *
 *  Based on:
 *  linux/drivers/video/fb_notify.c
 *
 *  Copyright (C) 2018 Boundary Devices LLC
 */
#include <linux/notifier.h>
#include <linux/export.h>

static BLOCKING_NOTIFIER_HEAD(drm_notifier_list);

/**
 *	drm_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int drm_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&drm_notifier_list, nb);
}
EXPORT_SYMBOL(drm_register_client);

/**
 *	drm_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int drm_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&drm_notifier_list, nb);
}
EXPORT_SYMBOL(drm_unregister_client);

/**
 * drm_notifier_call_chain - notify clients of events
 *
 */
int drm_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&drm_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(drm_notifier_call_chain);
