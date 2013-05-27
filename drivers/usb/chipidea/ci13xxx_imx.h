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

struct ci13xxx_imx_data;
/* Used to set SoC specific callbacks */
struct usbmisc_ops {
	/* It's called once when probe a usb device */
	int (*init)(struct ci13xxx_imx_data *data);
	/* It's called once after adding a usb device */
	int (*post)(struct ci13xxx_imx_data *data);
	/* set usb wakeup enable/disable */
	int (*set_wakeup)(struct ci13xxx_imx_data *data,
		enum ci_usb_wakeup_events wakeup_event);
	/* To know if current interrupt is wakeup interrupt */
	int (*is_wakeup_intr)(struct ci13xxx_imx_data *data);
	/* Call it before setting portsc.suspendM=1 */
	void (*hsic_set_connect)(struct ci13xxx_imx_data *data);
	int (*hsic_set_clk)(struct ci13xxx_imx_data *data, bool on);
};

struct ci13xxx_imx_data {
	struct device_node *phy_np;
	struct usb_phy *phy;
	struct platform_device *ci_pdev;
	struct clk *clk;
	struct usbmisc_ops *usbmisc_ops;
	/* non core register base address, mx23/mx28 has no non core register */
	struct regmap *non_core_base_addr;
	int index; /* controller's index */
	spinlock_t lock;
	int disable_oc:1; /* over current detect disabled */
	int evdo:1; /* set external vbus divider option */
	/* wakeup event for which operation mode, gadget/host/otg */
	enum ci_usb_wakeup_events wakeup_event;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_hsic_active;
};

#define IMX_WAKEUP_INTR_PENDING		1
