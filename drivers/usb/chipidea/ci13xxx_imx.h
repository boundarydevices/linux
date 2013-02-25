/*
 * Copyright (C) 2013 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

enum ci_usb_wakeup_events{
	CI_USB_WAKEUP_EVENT_NONE,
	CI_USB_WAKEUP_EVENT_GADGET,
	CI_USB_WAKEUP_EVENT_HOST,
	CI_USB_WAKEUP_EVENT_OTG,
};

/* Used to set SoC specific callbacks */
struct usbmisc_ops {
	/* It's called once when probe a usb device */
	int (*init)(struct device *dev);
	/* It's called once after adding a usb device */
	int (*post)(struct device *dev);
	/* set usb wakeup enable/disable */
	int (*set_wakeup)(struct device *dev,
		enum ci_usb_wakeup_events wakeup_event);
	/* To know if current interrupt is wakeup interrupt */
	int (*is_wakeup_intr)(struct device *dev);
};

struct usbmisc_usb_device {
	struct device *dev; /* usb controller device */
	int index;

	int disable_oc:1; /* over current detect disabled */
	int evdo:1; /* set external vbus divider option */
	/* wakeup event for which operation mode, gadget/host/otg */
	enum ci_usb_wakeup_events wakeup_event;
};

int usbmisc_set_ops(const struct usbmisc_ops *ops);
void usbmisc_unset_ops(const struct usbmisc_ops *ops);
int
usbmisc_get_init_data(struct device *dev, struct usbmisc_usb_device *usbdev);

#define IMX_WAKEUP_INTR_PENDING		1
