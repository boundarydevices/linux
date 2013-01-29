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

#include <linux/usb/otg.h>
#include <linux/usb/gadget.h>
#include <linux/usb/chipidea.h>

#include "ci.h"
#include "bits.h"

void ci_clear_otg_interrupt(struct ci13xxx *ci, u32 bits)
{
	/* Only clear request bits */
	hw_write(ci, OP_OTGSC, OTGSC_INT_STATUS_BITS, bits);
}

void ci_enable_otg_interrupt(struct ci13xxx *ci, u32 bits)
{
	hw_write(ci, OP_OTGSC, bits, bits);
}

void ci_disable_otg_interrupt(struct ci13xxx *ci, u32 bits)
{
	hw_write(ci, OP_OTGSC, bits, 0);
}

static int ci_otg_set_peripheral(struct usb_otg *otg,
		struct usb_gadget *periph)
{
	otg->gadget = periph;

	return 0;
}

static int ci_otg_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	otg->host = host;

	return 0;
}

/**
 * ci_hdrc_otg_init - initialize device related bits
 * ci: the controller
 *
 * This function create otg struct, if the device can switch between
 * device and host.
 */
int ci_hdrc_otg_init(struct ci13xxx *ci)
{
	/* Useless at current */
	ci->otg.set_peripheral = ci_otg_set_peripheral;
	ci->otg.set_host = ci_otg_set_host;
	if (!IS_ERR_OR_NULL(ci->transceiver))
		ci->transceiver->otg = &ci->otg;
	ci_enable_otg_interrupt(ci, OTGSC_IDIE);

	return 0;
}
