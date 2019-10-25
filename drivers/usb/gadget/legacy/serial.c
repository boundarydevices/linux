// SPDX-License-Identifier: GPL-2.0+
/*
 * serial.c -- USB gadget serial driver
 *
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 by David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>

#include "u_serial.h"


/* Defines */

#define GS_VERSION_STR			"v2.4"
#define GS_VERSION_NUM			0x2400

#define GS_LONG_NAME			"Gadget Serial"
#define GS_VERSION_NAME			GS_LONG_NAME " " GS_VERSION_STR

/*-------------------------------------------------------------------------*/
USB_GADGET_COMPOSITE_OPTIONS();

/* Thanks to NetChip Technologies for donating this product ID.
*
* DO NOT REUSE THESE IDs with a protocol-incompatible driver!!  Ever!!
* Instead:  allocate your own, using normal USB-IF procedures.
*/
#define GS_VENDOR_ID			0x0525	/* NetChip */
#define GS_PRODUCT_ID			0xa4a6	/* Linux-USB Serial Gadget */
#define GS_CDC_PRODUCT_ID		0xa4a7	/* ... as CDC-ACM */
#define GS_CDC_OBEX_PRODUCT_ID		0xa4a9	/* ... as CDC-OBEX */

/* string IDs are assigned dynamically */

#define STRING_DESCRIPTION_IDX		USB_GADGET_FIRST_AVAIL_IDX

struct s_driver_desc {
	struct usb_composite_driver driver;
	struct usb_device_descriptor dev_desc;
	struct usb_gadget_strings *dev_strings[2];
	struct usb_gadget_strings stringtab_dev;
	struct usb_string strings[STRING_DESCRIPTION_IDX + 2];
	bool acm;
	bool obex;
	unsigned ports;
};

struct gadget_serial {
	struct usb_configuration config;
	struct usb_function_instance *fi_serial[MAX_U_SERIAL_PORTS];
	struct usb_function *f_serial[MAX_U_SERIAL_PORTS];
	const struct usb_descriptor_header *otg_desc[2];
};

/*-------------------------------------------------------------------------*/

/* Module */
MODULE_DESCRIPTION(GS_VERSION_NAME);
MODULE_AUTHOR("Al Borchers");
MODULE_AUTHOR("David Brownell");
MODULE_LICENSE("GPL");

static bool use_acm[4] = {true, true, true, true};
static unsigned num_acm;
module_param_array(use_acm, bool, &num_acm, 0);
MODULE_PARM_DESC(use_acm, "Use CDC ACM, default=yes");

static bool use_obex[4];
static unsigned num_obex;
module_param_array(use_obex, bool, &num_obex, 0);
MODULE_PARM_DESC(use_obex, "Use CDC OBEX, default=no");

static unsigned n_ports[4] = {1, 1, 1 ,1};
static unsigned num_n_ports;
module_param_array(n_ports, uint, &num_n_ports, 0);
MODULE_PARM_DESC(n_ports, "number of ports to create, default=1");

static bool enable = true;

static int switch_gserial_enable(bool do_enable);

static int enable_set(const char *s, const struct kernel_param *kp)
{
	bool do_enable;
	int ret;

	if (!s)	/* called for no-arg enable == default */
		return 0;

	ret = strtobool(s, &do_enable);
	if (ret || enable == do_enable)
		return ret;

	ret = switch_gserial_enable(do_enable);
	if (!ret)
		enable = do_enable;

	return ret;
}

static const struct kernel_param_ops enable_ops = {
	.set = enable_set,
	.get = param_get_bool,
};

module_param_cb(enable, &enable_ops, &enable, 0644);

static char *udc_name[4];
static unsigned num_udc;
module_param_array(udc_name, charp, &num_udc, 0);
MODULE_PARM_DESC(udc_name, "udc to bind, default is 1st found");

/*-------------------------------------------------------------------------*/

static int serial_register_ports(struct usb_composite_dev *cdev,
		struct usb_configuration *c, const char *f_name, unsigned ports)
{
	struct gadget_serial *gs = cdev->private_data;
	int i;
	int ret;

	ret = usb_add_config_only(cdev, c);
	if (ret)
		goto out;

	for (i = 0; i < ports; i++) {

		gs->fi_serial[i] = usb_get_function_instance(f_name);
		if (IS_ERR(gs->fi_serial[i])) {
			ret = PTR_ERR(gs->fi_serial[i]);
			goto fail;
		}

		gs->f_serial[i] = usb_get_function(gs->fi_serial[i]);
		if (IS_ERR(gs->f_serial[i])) {
			ret = PTR_ERR(gs->f_serial[i]);
			goto err_get_func;
		}

		ret = usb_add_function(c, gs->f_serial[i]);
		if (ret)
			goto err_add_func;
	}

	return 0;

err_add_func:
	usb_put_function(gs->f_serial[i]);
err_get_func:
	usb_put_function_instance(gs->fi_serial[i]);

fail:
	i--;
	while (i >= 0) {
		usb_remove_function(c, gs->f_serial[i]);
		usb_put_function(gs->f_serial[i]);
		usb_put_function_instance(gs->fi_serial[i]);
		i--;
	}
out:
	return ret;
}

static int gs_bind(struct usb_composite_dev *cdev)
{
	struct usb_composite_driver *driver = cdev->driver;
	struct s_driver_desc *sdd = container_of(driver, struct s_driver_desc, driver);
	struct usb_string *s = sdd->strings;
	struct gadget_serial *gs = cdev->private_data;
	int status;

	if (!gs) {
		gs = kzalloc(sizeof(struct gadget_serial), GFP_KERNEL);
		cdev->private_data = gs;
		gs->config.bmAttributes	= USB_CONFIG_ATT_SELFPOWER;
		if (sdd->acm) {
			gs->config.label = "CDC ACM config";
			gs->config.bConfigurationValue = 2;
		} else if (sdd->obex) {
			gs->config.label = "CDC OBEX config";
			gs->config.bConfigurationValue = 3;
		} else {
			gs->config.label = "Generic Serial config";
			gs->config.bConfigurationValue = 1;
		}
		s[STRING_DESCRIPTION_IDX].s = gs->config.label;
	}
	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */

	status = usb_string_ids_tab(cdev, s);
	if (status < 0)
		goto fail;
	sdd->dev_desc.iManufacturer = s[USB_GADGET_MANUFACTURER_IDX].id;
	sdd->dev_desc.iProduct = s[USB_GADGET_PRODUCT_IDX].id;
	status = s[STRING_DESCRIPTION_IDX].id;
	gs->config.iConfiguration = status;

	if (gadget_is_otg(cdev->gadget)) {
		if (!gs->otg_desc[0]) {
			struct usb_descriptor_header *usb_desc;

			usb_desc = usb_otg_descriptor_alloc(cdev->gadget);
			if (!usb_desc) {
				status = -ENOMEM;
				goto fail;
			}
			usb_otg_descriptor_init(cdev->gadget, usb_desc);
			gs->otg_desc[0] = usb_desc;
			gs->otg_desc[1] = NULL;
		}
		gs->config.descriptors = gs->otg_desc;
		gs->config.bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	/* register our configuration */
	if (sdd->acm) {
		status  = serial_register_ports(cdev, &gs->config,
				"acm", sdd->ports);
		usb_ep_autoconfig_reset(cdev->gadget);
	} else if (sdd->obex)
		status = serial_register_ports(cdev, &gs->config,
				"obex", sdd->ports);
	else {
		status = serial_register_ports(cdev, &gs->config,
				"gser", sdd->ports);
	}
	if (status < 0)
		goto fail1;

	usb_composite_overwrite_options(cdev, &coverwrite);
	INFO(cdev, "%s\n", GS_VERSION_NAME);

	return 0;
fail1:
	kfree(gs->otg_desc[0]);
	gs->otg_desc[0] = NULL;
fail:
	return status;
}

static int gs_unbind(struct usb_composite_dev *cdev)
{
	struct usb_composite_driver *driver = cdev->driver;
	struct s_driver_desc *sdd = container_of(driver, struct s_driver_desc, driver);
	struct gadget_serial *gs = cdev->private_data;
	int i;

	if (!gs)
		return 0;
	for (i = 0; i < sdd->ports; i++) {
		usb_put_function(gs->f_serial[i]);
		usb_put_function_instance(gs->fi_serial[i]);
	}
	cdev->private_data = NULL;

	kfree(gs->otg_desc[0]);
	kfree(gs);
	return 0;
}

static struct s_driver_desc gserial_driver = {
	.driver = {
		.name		= "g_serial",
		.max_speed	= USB_SPEED_SUPER,
		.bind		= gs_bind,
		.unbind		= gs_unbind,
	},
	.dev_desc = {
		.bLength =		USB_DT_DEVICE_SIZE,
		.bDescriptorType =	USB_DT_DEVICE,
		/* .bcdUSB = DYNAMIC */
		/* .bDeviceClass = f(use_acm) */
		.bDeviceSubClass =	0,
		.bDeviceProtocol =	0,
		/* .bMaxPacketSize0 = f(hardware) */
		.idVendor =		cpu_to_le16(GS_VENDOR_ID),
		/* .idProduct =	f(use_acm) */
		.bcdDevice = cpu_to_le16(GS_VERSION_NUM),
		/* .iManufacturer = DYNAMIC */
		/* .iProduct = DYNAMIC */
		.bNumConfigurations =	1,
	},
};

static struct s_driver_desc *g_sdd;

static int switch_gserial_enable(bool do_enable)
{
	int i;
	struct s_driver_desc *sdd;
	struct s_driver_desc *base = g_sdd;
	int cnt = num_udc;

	if (!enable)
		return 0;

	if (!cnt || !base)
		cnt = 1;
	for (i = cnt - 1; i >= 0; i--) {
		sdd = i ? &base[i - 1] : &gserial_driver;
		if (do_enable)
			usb_composite_probe(&sdd->driver);
		else
			usb_composite_unregister(&sdd->driver);
	}
	return 0;
}

void setup_strings(struct s_driver_desc *s)
{
	s->strings[USB_GADGET_MANUFACTURER_IDX].s = "";
	s->strings[USB_GADGET_PRODUCT_IDX].s = GS_VERSION_NAME;
	s->strings[USB_GADGET_SERIAL_IDX].s = "";

	s->stringtab_dev.language = 0x0409;	/* en-us */
	s->stringtab_dev.strings = s->strings;
	s->dev_strings[0] = &s->stringtab_dev;
	s->dev_strings[1] = NULL;
	s->driver.strings = s->dev_strings;
}

void setup_desc(struct usb_device_descriptor *dev_desc, int acm, int obex)
{
	if (acm) {
		dev_desc->bDeviceClass = USB_CLASS_COMM;
		dev_desc->idProduct = cpu_to_le16(GS_CDC_PRODUCT_ID);
	} else if (obex) {
		dev_desc->bDeviceClass = USB_CLASS_COMM;
		dev_desc->idProduct = cpu_to_le16(GS_CDC_OBEX_PRODUCT_ID);
	} else {
		dev_desc->bDeviceClass = USB_CLASS_VENDOR_SPEC;
		dev_desc->idProduct = cpu_to_le16(GS_PRODUCT_ID);
	}
}

void setup_driver_desc(struct s_driver_desc *sdd, int acm, int obex, unsigned ports)
{
	sdd->acm = acm;
	sdd->obex = obex;
	sdd->ports = ports;
	sdd->driver.dev = &sdd->dev_desc;
	setup_strings(sdd);
	setup_desc(&sdd->dev_desc, acm, obex);
}

static int __init gserial_init(void)
{
	struct s_driver_desc *sdd = &gserial_driver;
	static struct s_driver_desc *base = NULL;
	bool acm, obex;
	unsigned ports;
	int ret = 0;
	unsigned i;


	if (!enable)
		return 0;
	/* We *could* export two configs; that'd be much cleaner...
	 * but neither of these product IDs was defined that way.
	 */

	if (num_udc > ARRAY_SIZE(udc_name))
		return -EINVAL;
	if (num_udc) {
		if (num_udc > 1) {
			g_sdd = base = kmalloc(sizeof(*sdd) * (num_udc - 1), GFP_KERNEL);
			for (i = 1; i < num_udc; i++) {
				base[i - 1] = *sdd;
			}
		}
		for (i = 0; i < num_udc; i++) {
			acm = use_acm[num_acm ? ((i < num_acm) ? i : num_acm - 1) : 0];
			obex = use_obex[num_obex ? ((i < num_obex) ? i : num_obex - 1) : 0];
			ports = n_ports[num_n_ports ? ((i < num_n_ports) ? i : num_n_ports - 1) : 0];
			setup_driver_desc(sdd, acm, obex, ports);
			sdd->driver.gadget_driver.match_existing_only = 1;
			ret = usb_composite_probe_udc_name(&sdd->driver, udc_name[i]);
			sdd = &base[i];
		}
	} else {
		acm = use_acm[0];
		obex = use_obex[0];
		ports = n_ports[0];
		setup_driver_desc(sdd, acm, obex, ports);
		ret = usb_composite_probe(&sdd->driver);
	}
	return ret;
}
module_init(gserial_init);

static void __exit gserial_cleanup(void)
{
	int i;
	struct s_driver_desc *sdd;
	struct s_driver_desc *base = g_sdd;
	int cnt = num_udc;

	if (!enable)
		return;
	g_sdd = NULL;
	if (!cnt || !base)
		cnt = 1;
	for (i = cnt - 1; i >= 0; i--) {
		sdd = i ? &base[i - 1] : &gserial_driver;
		usb_composite_unregister(&sdd->driver);
	}
	kfree(base);
}
module_exit(gserial_cleanup);
