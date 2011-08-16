/*
 * Gadget Driver for emulating a USB Keyboard and Mouse
 *
 * Copyright (C) 2011 Boundary Devices
 * Author: Eric Nelson <eric.nelson@boundarydevices.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/usb/g_hid.h>
#include <linux/platform_device.h>

static struct hidg_func_descriptor kbd_data = {
	.subclass		= 0, /* No subclass */
	.protocol		= 1, /* Keyboard */
	.report_length		= 8,
	.report_desc_length	= 53,
	.report_desc		= {
		0x05, 0x01,	/* USAGE_PAGE (Generic Desktop)	          */
		0x09, 0x06,	/* USAGE (Keyboard)                       */
		0xa1, 0x01,	/* COLLECTION (Application)               */
		0x05, 0x07,	/*   USAGE_PAGE (Keyboard)                */
		0x19, 0xe0,	/*   USAGE_MINIMUM (Keyboard LeftControl) */
		0x29, 0xe7,	/*   USAGE_MAXIMUM (Keyboard Right GUI)   */
		0x15, 0x00,	/*   LOGICAL_MINIMUM (0)                  */
		0x25, 0x01,	/*   LOGICAL_MAXIMUM (1)                  */
		0x75, 0x01,	/*   REPORT_SIZE (1)                      */
		0x95, 0x08,	/*   REPORT_COUNT (8)                     */
		0x81, 0x02,	/*   INPUT (Data,Var,Abs)                 */

		0x95, 0x06,	/*   REPORT_COUNT (6)                     */
		0x75, 0x08,	/*   REPORT_SIZE (8)                      */
		0x15, 0x00,	/*   LOGICAL_MINIMUM (0)                  */
		0x26, 0xff, 00,	/*   LOGICAL_MAXIMUM (101)                */
		0x05, 0x07,	/*   USAGE_PAGE (Keyboard)                */
		0x19, 0x00,	/*   USAGE_MINIMUM (Reserved)             */
		0x2a, 0xff, 00,	/*   USAGE_MAXIMUM (Keyboard Application) */
		0x81, 0x00,	/*   INPUT (Data,Ary,Abs)                 */

		0x95, 0x01,	/*   REPORT_COUNT (1)                     */
		0x75, 0x08,	/*   REPORT_SIZE (8)                      */
		0x81, 0x01,	/*   INPUT (Cnst,Var,Abs)                 */
		0x95, 0x08,	/*   REPORT_COUNT (8)                     */
		0x75, 0x01,	/*   REPORT_SIZE (1)                      */
		0x91, 0x01,	/*   OUTPUT (Cnst,Var,Abs)                */
		0xc0		/* END_COLLECTION                         */
	}
};

static struct platform_device kbd_gadget = {
	.name			= "hidg",
	.id			= 0,
	.num_resources		= 0,
	.resource		= 0,
	.dev.platform_data	= &kbd_data,
};

static struct hidg_func_descriptor mouse_data = {
	.subclass		= 0, /* No subclass */
	.protocol		= 2, /* Mouse */
	.report_length		= 8,
	.report_desc_length	= 76,
	.report_desc		= {
		  0x05, 0x01, 0x09, 0x02,  0xa1, 0x01, 0x09, 0x01,
		  0xa1, 0x00, 0x09, 0x00,  0x15, 0x00, 0x25, 0x01,
		  0x95, 0x06, 0x75, 0x01,  0x81, 0x01, 0x05, 0x09,
		  0x19, 0x01, 0x29, 0x03,  0x15, 0x00, 0x25, 0x01,
		  0x95, 0x02, 0x75, 0x01,  0x81, 0x02, 0x05, 0x01,
		  0x09, 0x30, 0x09, 0x31,  0x15, 0x00, 0x26, 0xff,
		  0x03, 0x36, 0x80, 0x00,  0x46, 0xff, 0x7f, 0x75,
		  0x10, 0x95, 0x02, 0x81,  0x02, 0x09, 0x00, 0x15,
		  0x00, 0x26, 0xff, 0x00,  0x75, 0x08, 0x95, 0x05,
		  0xb1, 0x02, 0xc0, 0xc0
	}
};

static struct platform_device mouse_gadget = {
	.name			= "hidg",
	.id			= 1,
	.num_resources		= 0,
	.resource		= 0,
	.dev.platform_data	= &mouse_data,
};

static int __init init(void)
{
	int rval ;
	pr_info("USB driver initialize\n");
	rval = platform_device_register(&kbd_gadget);
	if (0 == rval) {
		rval = platform_device_register(&mouse_gadget);
		if (0 == rval) {
			printk (KERN_ERR "%s: registered keyboard and mouse gadgets\n", __FILE__ );
		} else
			pr_err("%s/%s: error %d registering mouse data\n", __FILE__, __func__, rval );
	} else
		pr_err("%s/%s: error %d registering keyboard data\n", __FILE__, __func__, rval );
	return rval ;
}

static void __exit cleanup(void)
{
	pr_info("USB driver cleanup\n");
        platform_device_unregister(&mouse_gadget);
	platform_device_unregister(&kbd_gadget);
}

module_init(init);
module_exit(cleanup);

MODULE_AUTHOR("Eric Nelson");
MODULE_DESCRIPTION("Gadget USB Keyboard & Mouse");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
