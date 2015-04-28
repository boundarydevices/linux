/*
 * otg.c - ChipIdea USB IP core OTG driver
 *
 * Copyright (C) 2013 Freescale Semiconductor, Inc.
 *
 * Author: Peter Chen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * This file mainly handles otgsc register, it may include OTG operation
 * in the future.
 */

#include <linux/extcon.h>
#include <linux/usb/otg.h>
#include <linux/usb/gadget.h>
#include <linux/usb/chipidea.h>
#include <linux/kthread.h>
#include <linux/freezer.h>

#include "ci.h"
#include "bits.h"
#include "otg.h"

/**
 * hw_read_otgsc returns otgsc register bits value.
 * @mask: bitfield mask
 */
u32 hw_read_otgsc(struct ci_hdrc *ci, u32 mask)
{
	return hw_read(ci, OP_OTGSC, mask);
}

/**
 * hw_write_otgsc updates target bits of OTGSC register.
 * @mask: bitfield mask
 * @data: to be written
 */
void hw_write_otgsc(struct ci_hdrc *ci, u32 mask, u32 data)
{
	hw_write(ci, OP_OTGSC, mask | OTGSC_INT_STATUS_BITS, data);
}

/**
 * ci_otg_role - pick role based on ID pin state
 * @ci: the controller
 */
enum ci_role ci_otg_role(struct ci_hdrc *ci)
{
	u32 sts = hw_read(ci, OP_OTGSC, ~0);
	enum ci_role role = sts & OTGSC_ID
		? CI_ROLE_GADGET
		: CI_ROLE_HOST;

	return role;
}

/*
 * Handling vbus glitch
 * We only need to consider glitch for without usb connection,
 * With usb connection, we consider it as real disconnection.
 *
 * If the vbus can't be kept above B session valid for timeout value,
 * we think it is a vbus glitch, otherwise it's a valid vbus.
 */
#define CI_VBUS_CONNECT_TIMEOUT_MS 300
static int ci_is_vbus_glitch(struct ci_hdrc *ci)
{
	int i;
	for (i = 0; i < CI_VBUS_CONNECT_TIMEOUT_MS/20; i++) {
		if (hw_read_otgsc(ci, OTGSC_AVV)) {
			return 0;
		} else if (!hw_read_otgsc(ci, OTGSC_BSV)) {
			dev_warn(ci->dev, "there is a vbus glitch\n");
			return 1;
		}
		msleep(20);
	}

	return 0;
}

void ci_handle_vbus_connected(struct ci_hdrc *ci)
{
	int bsv;

	/*
	 * TODO: if the platform does not supply 5v to udc, or use other way
	 * to supply 5v, it needs to use other conditions to call
	 * usb_gadget_vbus_connect.
	 */
	if (!ci->is_otg)
		return;

	bsv = hw_read_otgsc(ci, OTGSC_BSV) ? 1 : 0;

	if (bsv && !ci_is_vbus_glitch(ci))
		usb_gadget_vbus_connect(&ci->gadget);

	extcon_set_cable_state_(&ci->extcon, 2, bsv);
}

void ci_handle_vbus_change(struct ci_hdrc *ci)
{
	int bsv;

	if (!ci->is_otg)
		return;

	bsv = hw_read_otgsc(ci, OTGSC_BSV) ? 1 : 0;
	if (bsv)
		usb_gadget_vbus_connect(&ci->gadget);
	else
		usb_gadget_vbus_disconnect(&ci->gadget);

	extcon_set_cable_state_(&ci->extcon, 2, bsv);
}

#define CI_VBUS_STABLE_TIMEOUT_MS 5000
static void ci_handle_id_switch(struct ci_hdrc *ci)
{
	enum ci_role role = ci_otg_role(ci);

	if (role != ci->role) {
		if (ci->is_otg) {
			dev_info(ci->dev, "role %d to %d\n", ci->role, role);
			extcon_set_cable_state_(&ci->extcon, role, 1);
		}
		dev_dbg(ci->dev, "switching from %s to %s\n",
			ci_role(ci)->name, ci->roles[role]->name);

		ci_role_stop(ci);
		/* wait vbus lower than OTGSC_BSV */
		hw_wait_reg(ci, OP_OTGSC, OTGSC_BSV, 0,
				CI_VBUS_STABLE_TIMEOUT_MS);
		ci_role_start(ci, role);
		if (ci->is_otg) {
			role = ci->role;
			if (role < CI_ROLE_END) {
				extcon_set_cable_state_(&ci->extcon, role, 1);
			} else {
				role = 0;
				extcon_set_cable_state_(&ci->extcon, role, 0);
			}
			extcon_set_cable_state_(&ci->extcon, role ^ 1, 0);
		}
	}
}

/* If there is pending otg event */
static inline bool ci_otg_event_is_pending(struct ci_hdrc *ci)
{
	return ci->id_event || ci->b_sess_valid_event || ci->vbus_glitch_check_event;
}

static void ci_handle_vbus_glitch(struct ci_hdrc *ci)
{
	bool valid_vbus_change = false;

	if (hw_read_otgsc(ci, OTGSC_BSV)) {
		if (!ci_is_vbus_glitch(ci)) {
			valid_vbus_change = true;
		}
	} else {
		if (ci->vbus_active)
			valid_vbus_change = true;
	}

	if (valid_vbus_change)
		ci->b_sess_valid_event = true;
}

/**
 * ci_otg_event - perform otg (vbus/id) event handle
 * @ci: ci_hdrc struct
 */
static void ci_otg_event(struct ci_hdrc *ci)
{
	disable_irq(ci->irq);
	if (ci->vbus_glitch_check_event) {
		ci->vbus_glitch_check_event = false;
		pm_runtime_get_sync(ci->dev);
		ci_handle_vbus_glitch(ci);
		pm_runtime_put_sync(ci->dev);
	}

	if (ci->id_event) {
		ci->id_event = false;
		/* Keep controller active during id switch */
		pm_runtime_get_sync(ci->dev);
		ci_handle_id_switch(ci);
		pm_runtime_put_sync(ci->dev);
	}
	if (ci->b_sess_valid_event) {
		ci->b_sess_valid_event = false;
		pm_runtime_get_sync(ci->dev);
		ci_handle_vbus_change(ci);
		pm_runtime_put_sync(ci->dev);
	}
	enable_irq(ci->irq);
}

static int ci_otg_thread(void *ptr)
{
	struct ci_hdrc *ci = ptr;

	set_freezable();

	do {
		wait_event_freezable(ci->otg_wait,
				ci_otg_event_is_pending(ci) ||
				kthread_should_stop());
		ci_otg_event(ci);
	} while (!kthread_should_stop());

	dev_warn(ci->dev, "ci_otg_thread quits\n");

	return 0;
}

/**
 * ci_hdrc_otg_init - initialize otg struct
 * ci: the controller
 */
int ci_hdrc_otg_init(struct ci_hdrc *ci)
{
	init_waitqueue_head(&ci->otg_wait);
	ci->otg_task = kthread_run(ci_otg_thread, ci, "ci otg thread");
	if (IS_ERR(ci->otg_task)) {
		dev_err(ci->dev, "error to create otg thread\n");
		return PTR_ERR(ci->otg_task);
	}

	return 0;
}

/**
 * ci_hdrc_otg_destroy - destroy otg struct
 * ci: the controller
 */
void ci_hdrc_otg_destroy(struct ci_hdrc *ci)
{
	kthread_stop(ci->otg_task);
	ci_disable_otg_interrupt(ci, OTGSC_INT_EN_BITS);
	ci_clear_otg_interrupt(ci, OTGSC_INT_STATUS_BITS);
}
