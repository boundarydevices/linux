/*
 * host.c - ChipIdea USB host controller driver
 *
 * Copyright (c) 2012 Intel Corporation
 *
 * Author: Alexander Shishkin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/chipidea.h>

#define CHIPIDEA_EHCI
#include "../host/ehci-hcd.c"

#include "ci.h"
#include "bits.h"
#include "host.h"

static int ci_ehci_setup(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int ret;

	hcd->has_tt = 1;

	ret = ehci_setup(hcd);
	if (ret)
		return ret;

	ehci_port_power(ehci, 0);

	return ret;
}

static enum usb_device_speed ci_get_device_speed(struct ehci_hcd *ehci,
						u32 portsc)
{
	if (ehci_is_TDI(ehci)) {
		switch ((portsc >> (ehci->has_hostpc ? 25 : 26)) & 3) {
		case 0:
			return USB_SPEED_FULL;
		case 1:
			return USB_SPEED_LOW;
		case 2:
			return USB_SPEED_HIGH;
		default:
			return USB_SPEED_UNKNOWN;
		}
	} else {
		return USB_SPEED_HIGH;
	}
}

static void ci_ehci_bus_post_suspend(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int port;
	u32 tmp;

	port = HCS_N_PORTS(ehci->hcs_params);
	while (port--) {
		u32 __iomem *reg = &ehci->regs->port_status[port];
		u32 portsc = ehci_readl(ehci, reg);

		if (portsc & PORT_CONNECT) {
			/*
			 * For chipidea, the resume signal will be ended
			 * automatically, so for remote wakeup case, the
			 * usbcmd.rs may not be set before the resume has
			 * ended if other resume path consumes too much
			 * time (~23ms-24ms), in that case, the SOF will not
			 * send out within 3ms after resume ends, then the
			 * device will enter suspend again.
			 */
			if (ehci_to_hcd(ehci)->self.root_hub->do_remote_wakeup) {
				ehci_info(ehci,
					"Remote wakeup is enabled, "
					"and device is on the port\n");

				tmp = ehci_readl(ehci, &ehci->regs->command);
				tmp |= CMD_RUN;
				ehci_writel(ehci, tmp, &ehci->regs->command);
				/*
				 * It needs a short delay between set RUNSTOP
				 * and set PHCD.
				 */
				udelay(125);
			}
		}
	}

}

static void ci_ehci_bus_suspend_notify_phy(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int port;

	port = HCS_N_PORTS(ehci->hcs_params);
	while (port--) {
		u32 __iomem *reg = &ehci->regs->port_status[port];
		u32 portsc = ehci_readl(ehci, reg);

		if (portsc & PORT_CONNECT) {
			enum usb_device_speed speed;
			speed = ci_get_device_speed(ehci, portsc);
			/* notify the USB PHY */
			if (hcd->phy)
				usb_phy_notify_suspend(hcd->phy, speed);
		}
	}

}
static int ci_ehci_bus_suspend(struct usb_hcd *hcd)
{
	int ret = ehci_bus_suspend(hcd);

	if (ret)
		return ret;
	else if (!IS_ENABLED(CONFIG_USB_SUSPEND))
		ci_ehci_bus_suspend_notify_phy(hcd);

	ci_ehci_bus_post_suspend(hcd);

	return ret;
}

static void ci_ehci_bus_post_resume(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int port;

	port = HCS_N_PORTS(ehci->hcs_params);
	while (port--) {
		u32 __iomem *reg = &ehci->regs->port_status[port];
		u32 portsc = ehci_readl(ehci, reg);

		if (portsc & PORT_CONNECT) {
			enum usb_device_speed speed;
			speed = ci_get_device_speed(ehci, portsc);
			/* notify the USB PHY */
			if (hcd->phy)
				usb_phy_notify_resume(hcd->phy, speed);
		}
	}
}

static int ci_ehci_bus_resume(struct usb_hcd *hcd)
{
	int ret = ehci_bus_resume(hcd);

	if (ret)
		return ret;
	else if (!IS_ENABLED(CONFIG_USB_SUSPEND))
		ci_ehci_bus_post_resume(hcd);

	return ret;
}

/* The below code is based on tegra ehci driver */
static int ci_ehci_hub_control(
	struct usb_hcd	*hcd,
	u16		typeReq,
	u16		wValue,
	u16		wIndex,
	char		*buf,
	u16		wLength
)
{
	struct ehci_hcd	*ehci = hcd_to_ehci(hcd);
	u32 __iomem	*status_reg;
	u32		temp;
	unsigned long	flags;
	int		retval = 0;

	status_reg = &ehci->regs->port_status[(wIndex & 0xff) - 1];

	spin_lock_irqsave(&ehci->lock, flags);

	if (typeReq == SetPortFeature && wValue == USB_PORT_FEAT_SUSPEND) {
		temp = ehci_readl(ehci, status_reg);
		if ((temp & PORT_PE) == 0 || (temp & PORT_RESET) != 0) {
			retval = -EPIPE;
			goto done;
		}

		temp &= ~(PORT_RWC_BITS | PORT_WKCONN_E);
		temp |= PORT_WKDISC_E | PORT_WKOC_E;
		ehci_writel(ehci, temp | PORT_SUSPEND, status_reg);

		/*
		 * If a transaction is in progress, there may be a delay in
		 * suspending the port. Poll until the port is suspended.
		 */
		if (handshake(ehci, status_reg, PORT_SUSPEND,
						PORT_SUSPEND, 5000))
			ehci_err(ehci, "timeout waiting for SUSPEND\n");

		spin_unlock_irqrestore(&ehci->lock, flags);
		ci_ehci_bus_suspend_notify_phy(hcd);
		spin_lock_irqsave(&ehci->lock, flags);

		set_bit((wIndex & 0xff) - 1, &ehci->suspended_ports);
		goto done;
	}

	/*
	 * After resume has finished, it needs do some post resume
	 * operation for some SoCs.
	 */
	else if (typeReq == ClearPortFeature &&
					wValue == USB_PORT_FEAT_C_SUSPEND) {

		/* Make sure the resume has finished, it should be finished */
		if (handshake(ehci, status_reg, PORT_RESUME, 0, 25000))
			ehci_err(ehci, "timeout waiting for resume\n");

		temp = ehci_readl(ehci, status_reg);

		ci_ehci_bus_post_resume(hcd);
	}

	spin_unlock_irqrestore(&ehci->lock, flags);

	/* Handle the hub control events here */
	return ehci_hub_control(hcd, typeReq, wValue, wIndex, buf, wLength);
done:
	spin_unlock_irqrestore(&ehci->lock, flags);
	return retval;
}

static const struct hc_driver ci_ehci_hc_driver = {
	.description	= "ehci_hcd",
	.product_desc	= "ChipIdea HDRC EHCI",
	.hcd_priv_size	= sizeof(struct ehci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq	= ehci_irq,
	.flags	= HCD_MEMORY | HCD_USB2,

	/*
	 * basic lifecycle operations
	 */
	.reset		= ci_ehci_setup,
	.start		= ehci_run,
	.stop		= ehci_stop,
	.shutdown	= ehci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue		= ehci_urb_enqueue,
	.urb_dequeue		= ehci_urb_dequeue,
	.endpoint_disable	= ehci_endpoint_disable,
	.endpoint_reset		= ehci_endpoint_reset,

	/*
	 * scheduling support
	 */
	.get_frame_number = ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data	= ehci_hub_status_data,
	.hub_control		= ci_ehci_hub_control,
	.bus_suspend		= ci_ehci_bus_suspend,
	.bus_resume		= ci_ehci_bus_resume,
	.relinquish_port	= ehci_relinquish_port,
	.port_handed_over	= ehci_port_handed_over,

	.clear_tt_buffer_complete = ehci_clear_tt_buffer_complete,
};

static irqreturn_t host_irq(struct ci13xxx *ci)
{
	return usb_hcd_irq(ci->irq, ci->hcd);
}

static int host_start(struct ci13xxx *ci)
{
	struct usb_hcd *hcd;
	struct ehci_hcd *ehci;
	int ret;

	if (usb_disabled())
		return -ENODEV;

	hcd = usb_create_hcd(&ci_ehci_hc_driver, ci->dev, dev_name(ci->dev));
	if (!hcd)
		return -ENOMEM;

	dev_set_drvdata(ci->dev, ci);
	hcd->rsrc_start = ci->hw_bank.phys;
	hcd->rsrc_len = ci->hw_bank.size;
	hcd->regs = ci->hw_bank.abs;
	hcd->has_tt = 1;

	hcd->power_budget = ci->platdata->power_budget;
	hcd->phy = ci->transceiver;

	ehci = hcd_to_ehci(hcd);
	ehci->caps = ci->hw_bank.cap;
	ehci->has_hostpc = ci->hw_bank.lpm;

	if (ci->reg_vbus) {
		ret = regulator_enable(ci->reg_vbus);
		if (ret) {
			dev_err(ci->dev,
				"Failed to enable vbus regulator, ret=%d\n",
				ret);
			goto put_hcd;
		}
	}

	ret = usb_add_hcd(hcd, 0, 0);
	if (ret)
		goto disable_reg;
	else
		ci->hcd = hcd;

	if (ci->platdata->flags & CI13XXX_DISABLE_STREAMING)
		hw_write(ci, OP_USBMODE, USBMODE_CI_SDIS, USBMODE_CI_SDIS);

	return ret;

disable_reg:
	if (ci->reg_vbus)
		regulator_disable(ci->reg_vbus);
put_hcd:
	usb_put_hcd(hcd);

	return ret;
}

static void host_stop(struct ci13xxx *ci)
{
	struct usb_hcd *hcd = ci->hcd;

	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);
	if (ci->reg_vbus)
		regulator_disable(ci->reg_vbus);
}

int ci_hdrc_host_init(struct ci13xxx *ci)
{
	struct ci_role_driver *rdrv;

	if (!hw_read(ci, CAP_DCCPARAMS, DCCPARAMS_HC))
		return -ENXIO;

	rdrv = devm_kzalloc(ci->dev, sizeof(struct ci_role_driver), GFP_KERNEL);
	if (!rdrv)
		return -ENOMEM;

	rdrv->init	= host_start;
	rdrv->start	= host_start;
	rdrv->stop	= host_stop;
	rdrv->destroy	= host_stop;
	rdrv->irq	= host_irq;
	rdrv->name	= "host";
	ci->roles[CI_ROLE_HOST] = rdrv;

	return 0;
}
