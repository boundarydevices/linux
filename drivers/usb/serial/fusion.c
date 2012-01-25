/*
  USB Driver for Fusion Wireless modems

  Copyright (C) 2011  Fusion Wireless <fusionwirelesscorp.com>

  This driver is free software; you can redistribute it and/or modify
  it under the terms of Version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  Portions copied from the Keyspan driver by Hugh Blemings <hugh@blemings.org>
*/

#define DRIVER_VERSION "v1.1.0-2.6.38-8-generic"
#define DRIVER_AUTHOR  "James Gibson <jgibson@fusionwirelesscorp.com>"
#define DRIVER_DESC "USB Driver for Fusion Wireless modems"

#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/errno.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include "usb-wwan.h"

/* Function prototypes */
static int  option_probe(struct usb_serial *serial,
			const struct usb_device_id *id);
static int option_send_setup(struct usb_serial_port *port);
static void option_instat_callback(struct urb *urb);

/* Vendor and product IDs */
#define VIATELECOM_VENDOR_ID			0x15EB
#define VIATELECOM_PRODUCT_SHAMU		0x0001

/* some devices interfaces need special handling due to a number of reasons */
enum option_blacklist_reason {
		OPTION_BLACKLIST_NONE = 0,
		OPTION_BLACKLIST_SENDSETUP = 1,
		OPTION_BLACKLIST_RESERVED_IF = 2
};

struct option_blacklist_info {
	const u32 infolen;	/* number of interface numbers on blacklist */
	const u8  *ifaceinfo;	/* pointer to the array holding the numbers */
	enum option_blacklist_reason reason;
};

static const u8 four_g_w14_no_sendsetup[] = { 0, 1 };
static const struct option_blacklist_info four_g_w14_blacklist = {
	.infolen = ARRAY_SIZE(four_g_w14_no_sendsetup),
	.ifaceinfo = four_g_w14_no_sendsetup,
	.reason = OPTION_BLACKLIST_SENDSETUP
};

static const struct usb_device_id option_ids[] = {
	{ USB_DEVICE(VIATELECOM_VENDOR_ID, VIATELECOM_PRODUCT_SHAMU) },
	{ } /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, option_ids);

static struct usb_driver option_driver = {
	.name       = "fusionwireless",
	.probe      = usb_serial_probe,
	.disconnect = usb_serial_disconnect,
#ifdef CONFIG_PM
	.suspend    = usb_serial_suspend,
	.resume     = usb_serial_resume,
	.supports_autosuspend =	1,
#endif
	.id_table   = option_ids,
	.no_dynamic_id = 	1,
};

/* The card has three separate interfaces, which the serial driver
 * recognizes separately, thus num_port=1.
 */

static struct usb_serial_driver option_1port_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"viatelecom-modem",
	},
	.description       = "CDMA EVDO modem (1-port)",
	.usb_driver        = &option_driver,
	.id_table          = option_ids,
	.num_ports         = 1,
	.probe             = option_probe,
	.open              = usb_wwan_open,
	.close             = usb_wwan_close,
	.dtr_rts	   = usb_wwan_dtr_rts,
	.write             = usb_wwan_write,
	.write_room        = usb_wwan_write_room,
	.chars_in_buffer   = usb_wwan_chars_in_buffer,
	.set_termios       = usb_wwan_set_termios,
	.tiocmget          = usb_wwan_tiocmget,
	.tiocmset          = usb_wwan_tiocmset,
	.attach            = usb_wwan_startup,
	.disconnect        = usb_wwan_disconnect,
	.release           = usb_wwan_release,
	.read_int_callback = option_instat_callback,
#ifdef CONFIG_PM
	.suspend           = usb_wwan_suspend,
	.resume            = usb_wwan_resume,
#endif
};

static int debug;

/* per port private data */

#define N_IN_URB 4
#define N_OUT_URB 4
#define IN_BUFLEN 4096
#define OUT_BUFLEN 4096

struct option_port_private {
	/* Input endpoints and buffer for this port */
	struct urb *in_urbs[N_IN_URB];
	u8 *in_buffer[N_IN_URB];
	/* Output endpoints and buffer for this port */
	struct urb *out_urbs[N_OUT_URB];
	u8 *out_buffer[N_OUT_URB];
	unsigned long out_busy;		/* Bit vector of URBs in use */
	int opened;
	struct usb_anchor delayed;

	/* Settings for the port */
	int rts_state;	/* Handshaking pins (outputs) */
	int dtr_state;
	int cts_state;	/* Handshaking pins (inputs) */
	int dsr_state;
	int dcd_state;
	int ri_state;

	unsigned long tx_start_time[N_OUT_URB];
};

/* Functions used by new usb-serial code. */
static int __init option_init(void)
{
	int retval;

	retval = usb_serial_register(&option_1port_device);
	if (retval)
		{
		printk(KERN_INFO KBUILD_MODNAME ":"
		"in function: __init_option_init: "
		" usb_serial_register() failed.\n");
		goto failed_1port_device_register;
		}
	else
		{
		printk(KERN_INFO KBUILD_MODNAME ":"
		"in function: __init_option_init: "
		" usb_serial_register() succeeded.\n");
		}
	retval = usb_register(&option_driver);
	if (retval)
		{
		goto failed_driver_register;
		}

	printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
	       DRIVER_DESC "\n");

	return 0;

failed_driver_register:
	usb_serial_deregister(&option_1port_device);
failed_1port_device_register:
	return retval;
}

static void __exit option_exit(void)
{
	usb_deregister(&option_driver);
	usb_serial_deregister(&option_1port_device);
}

module_init(option_init);
module_exit(option_exit);

static int option_probe(struct usb_serial *serial,
			const struct usb_device_id *id)
{
	struct usb_wwan_intf_private *data;

	data = serial->private = kzalloc(sizeof(struct usb_wwan_intf_private), GFP_KERNEL);

	if (!data)
		return -ENOMEM;
	data->send_setup = option_send_setup;
	spin_lock_init(&data->susp_lock);
	data->private = (void *)id->driver_info;
	return 0;
}

static enum option_blacklist_reason is_blacklisted(const u8 ifnum,
				const struct option_blacklist_info *blacklist)
{
	const u8  *info;
	int i;

	if (blacklist) {
		info = blacklist->ifaceinfo;

		for (i = 0; i < blacklist->infolen; i++) {
			if (info[i] == ifnum)
				return blacklist->reason;
		}
	}
	return OPTION_BLACKLIST_NONE;
}

static void option_instat_callback(struct urb *urb)
{
	int err;
	int status = urb->status;
	struct usb_serial_port *port =  urb->context;
	struct option_port_private *portdata = usb_get_serial_port_data(port);

	dbg("%s", __func__);
	dbg("%s: urb %p port %p has data %p", __func__, urb, port, portdata);

	if (status == 0) {
		struct usb_ctrlrequest *req_pkt =
				(struct usb_ctrlrequest *)urb->transfer_buffer;

		if (!req_pkt) {
			dbg("%s: NULL req_pkt", __func__);
			return;
		}
		if ((req_pkt->bRequestType == 0xA1) &&
				(req_pkt->bRequest == 0x20)) {
			int old_dcd_state;
			unsigned char signals = *((unsigned char *)
					urb->transfer_buffer +
					sizeof(struct usb_ctrlrequest));

			dbg("%s: signal x%x", __func__, signals);

			old_dcd_state = portdata->dcd_state;
			portdata->cts_state = 1;
			portdata->dcd_state = ((signals & 0x01) ? 1 : 0);
			portdata->dsr_state = ((signals & 0x02) ? 1 : 0);
			portdata->ri_state = ((signals & 0x08) ? 1 : 0);

			if (old_dcd_state && !portdata->dcd_state) {
				struct tty_struct *tty =
						tty_port_tty_get(&port->port);
				if (tty && !C_CLOCAL(tty))
					tty_hangup(tty);
				tty_kref_put(tty);
			}
		} else {
			dbg("%s: type %x req %x", __func__,
				req_pkt->bRequestType, req_pkt->bRequest);
		}
	} else
		err("%s: error %d", __func__, status);

	/* Resubmit urb so we continue receiving IRQ data */
	if (status != -ESHUTDOWN && status != -ENOENT) {
		err = usb_submit_urb(urb, GFP_ATOMIC);
		if (err)
			dbg("%s: resubmit intr urb failed. (%d)",
				__func__, err);
	}
}

/** send RTS/DTR state to the port.
 *
 * This is exactly the same as SET_CONTROL_LINE_STATE from the PSTN
 * CDC.
*/
static int option_send_setup(struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;
	struct usb_wwan_intf_private *intfdata =
		(struct usb_wwan_intf_private *) serial->private;
	struct option_port_private *portdata;
	int ifNum = serial->interface->cur_altsetting->desc.bInterfaceNumber;
	int val = 0;

	dbg("%s", __func__);

	if (is_blacklisted(ifNum,
			   (struct option_blacklist_info *) intfdata->private)
	    == OPTION_BLACKLIST_SENDSETUP) {
		dbg("No send_setup on blacklisted interface #%d\n", ifNum);
		return -EIO;
	}

	portdata = usb_get_serial_port_data(port);

#if 1
	return usb_control_msg(serial->dev,
			       usb_rcvctrlpipe(serial->dev, 0),
			       0x01, 0x40, portdata->dtr_state? 1:0,
			       ifNum, NULL, 0, USB_CTRL_SET_TIMEOUT);
#else
	return usb_control_msg(serial->dev,
			       usb_rcvctrlpipe(serial->dev, 0),
			       0x22, 0x21, val, ifNum, NULL, 0, USB_CTRL_SET_TIMEOUT);
#endif
}

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug messages");
