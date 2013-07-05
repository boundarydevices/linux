/*
 * core.c - ChipIdea USB IP core family device controller
 *
 * Copyright (C) 2008 Chipidea - MIPS Technologies, Inc. All rights reserved.
 *
 * Author: David Lopo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * Description: ChipIdea USB IP core family device controller
 *
 * This driver is composed of several blocks:
 * - HW:     hardware interface
 * - DBG:    debug facilities (optional)
 * - UTIL:   utilities
 * - ISR:    interrupts handling
 * - ENDPT:  endpoint operations (Gadget API)
 * - GADGET: gadget operations (Gadget API)
 * - BUS:    bus glue code, bus abstraction layer
 *
 * Compile Options
 * - CONFIG_USB_GADGET_DEBUG_FILES: enable debug facilities
 * - STALL_IN:  non-empty bulk-in pipes cannot be halted
 *              if defined mass storage compliance succeeds but with warnings
 *              => case 4: Hi >  Dn
 *              => case 5: Hi >  Di
 *              => case 8: Hi <> Do
 *              if undefined usbtest 13 fails
 * - TRACE:     enable function tracing (depends on DEBUG)
 *
 * Main Features
 * - Chapter 9 & Mass Storage Compliance with Gadget File Storage
 * - Chapter 9 Compliance with Gadget Zero (STALL_IN undefined)
 * - Normal & LPM support
 *
 * USBTEST Report
 * - OK: 0-12, 13 (STALL_IN defined) & 14
 * - Not Supported: 15 & 16 (ISO)
 *
 * TODO List
 * - OTG
 * - Isochronous & Interrupt Traffic
 * - Handle requests which spawns into several TDs
 * - GET_STATUS(device) - always reports 0
 * - Gadget API (majority of optional features)
 * - Suspend & Remote Wakeup
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
#include <linux/usb/chipidea.h>
#include <linux/usb/of.h>
#include <linux/phy.h>

#include "ci.h"
#include "udc.h"
#include "bits.h"
#include "host.h"
#include "debug.h"
#include "otg.h"

/* Controller register map */
static uintptr_t ci_regs_nolpm[] = {
	[CAP_CAPLENGTH]		= 0x000UL,
	[CAP_HCCPARAMS]		= 0x008UL,
	[CAP_DCCPARAMS]		= 0x024UL,
	[CAP_TESTMODE]		= 0x038UL,
	[OP_USBCMD]		= 0x000UL,
	[OP_USBSTS]		= 0x004UL,
	[OP_USBINTR]		= 0x008UL,
	[OP_DEVICEADDR]		= 0x014UL,
	[OP_ENDPTLISTADDR]	= 0x018UL,
	[OP_PORTSC]		= 0x044UL,
	[OP_DEVLC]		= 0x084UL,
	[OP_OTGSC]		= 0x064UL,
	[OP_USBMODE]		= 0x068UL,
	[OP_ENDPTSETUPSTAT]	= 0x06CUL,
	[OP_ENDPTPRIME]		= 0x070UL,
	[OP_ENDPTFLUSH]		= 0x074UL,
	[OP_ENDPTSTAT]		= 0x078UL,
	[OP_ENDPTCOMPLETE]	= 0x07CUL,
	[OP_ENDPTCTRL]		= 0x080UL,
};

static uintptr_t ci_regs_lpm[] = {
	[CAP_CAPLENGTH]		= 0x000UL,
	[CAP_HCCPARAMS]		= 0x008UL,
	[CAP_DCCPARAMS]		= 0x024UL,
	[CAP_TESTMODE]		= 0x0FCUL,
	[OP_USBCMD]		= 0x000UL,
	[OP_USBSTS]		= 0x004UL,
	[OP_USBINTR]		= 0x008UL,
	[OP_DEVICEADDR]		= 0x014UL,
	[OP_ENDPTLISTADDR]	= 0x018UL,
	[OP_PORTSC]		= 0x044UL,
	[OP_DEVLC]		= 0x084UL,
	[OP_OTGSC]		= 0x0C4UL,
	[OP_USBMODE]		= 0x0C8UL,
	[OP_ENDPTSETUPSTAT]	= 0x0D8UL,
	[OP_ENDPTPRIME]		= 0x0DCUL,
	[OP_ENDPTFLUSH]		= 0x0E0UL,
	[OP_ENDPTSTAT]		= 0x0E4UL,
	[OP_ENDPTCOMPLETE]	= 0x0E8UL,
	[OP_ENDPTCTRL]		= 0x0ECUL,
};

static int hw_alloc_regmap(struct ci13xxx *ci, bool is_lpm)
{
	int i;

	kfree(ci->hw_bank.regmap);

	ci->hw_bank.regmap = kzalloc((OP_LAST + 1) * sizeof(void *),
				     GFP_KERNEL);
	if (!ci->hw_bank.regmap)
		return -ENOMEM;

	for (i = 0; i < OP_ENDPTCTRL; i++)
		ci->hw_bank.regmap[i] =
			(i <= CAP_LAST ? ci->hw_bank.cap : ci->hw_bank.op) +
			(is_lpm ? ci_regs_lpm[i] : ci_regs_nolpm[i]);

	for (; i <= OP_LAST; i++)
		ci->hw_bank.regmap[i] = ci->hw_bank.op +
			4 * (i - OP_ENDPTCTRL) +
			(is_lpm
			 ? ci_regs_lpm[OP_ENDPTCTRL]
			 : ci_regs_nolpm[OP_ENDPTCTRL]);

	return 0;
}

/**
 * hw_port_test_set: writes port test mode (execute without interruption)
 * @mode: new value
 *
 * This function returns an error code
 */
int hw_port_test_set(struct ci13xxx *ci, u8 mode)
{
	const u8 TEST_MODE_MAX = 7;

	if (mode > TEST_MODE_MAX)
		return -EINVAL;

	hw_write(ci, OP_PORTSC, PORTSC_PTC, mode << ffs_nr(PORTSC_PTC));
	return 0;
}

/**
 * hw_port_test_get: reads port test mode value
 *
 * This function returns port test mode value
 */
u8 hw_port_test_get(struct ci13xxx *ci)
{
	return hw_read(ci, OP_PORTSC, PORTSC_PTC) >> ffs_nr(PORTSC_PTC);
}

static int hw_device_init(struct ci13xxx *ci, void __iomem *base)
{
	u32 reg;

	/* bank is a module variable */
	ci->hw_bank.abs = base;

	ci->hw_bank.cap = ci->hw_bank.abs;
	ci->hw_bank.cap += ci->platdata->capoffset;
	ci->hw_bank.op = ci->hw_bank.cap + ioread8(ci->hw_bank.cap);

	hw_alloc_regmap(ci, false);
	reg = hw_read(ci, CAP_HCCPARAMS, HCCPARAMS_LEN) >>
		ffs_nr(HCCPARAMS_LEN);
	ci->hw_bank.lpm  = reg;
	hw_alloc_regmap(ci, !!reg);
	ci->hw_bank.size = ci->hw_bank.op - ci->hw_bank.abs;
	ci->hw_bank.size += OP_LAST;
	ci->hw_bank.size /= sizeof(u32);

	/* PHY Initialization */
	if (hw_read(ci, OP_PORTSC, PORTSC_PHCD)) {
		hw_write(ci, OP_PORTSC, PORTSC_PHCD, 0);
		/* Some clks sync between Controller and USB PHY */
		usleep_range(800, 1000);
	}
	if (ci->transceiver)
		usb_phy_init(ci->transceiver);

	reg = hw_read(ci, CAP_DCCPARAMS, DCCPARAMS_DEN) >>
		ffs_nr(DCCPARAMS_DEN);
	ci->hw_ep_max = reg * 2;   /* cache hw ENDPT_MAX */

	if (ci->hw_ep_max > ENDPT_MAX)
		return -ENODEV;

	/* Disable all interrupts bits */
	hw_write(ci, OP_USBINTR, 0xffffffff, 0);
	ci_disable_otg_interrupt(ci, OTGSC_INT_EN_BITS);

	/* Clear all interrupts status bits*/
	hw_write(ci, OP_USBSTS, 0xffffffff, 0xffffffff);
	ci_clear_otg_interrupt(ci, OTGSC_INT_STATUS_BITS);

	dev_dbg(ci->dev, "ChipIdea HDRC found, lpm: %d; cap: %p op: %p\n",
		ci->hw_bank.lpm, ci->hw_bank.cap, ci->hw_bank.op);

	/* setup lock mode ? */

	/* ENDPTSETUPSTAT is '0' by default */

	/* HCSPARAMS.bf.ppc SHOULD BE zero for device */

	return 0;
}

static void hw_phymode_configure(struct ci13xxx *ci)
{
	u32 portsc, lpm;

	switch (ci->platdata->phy_mode) {
	case USBPHY_INTERFACE_MODE_UTMI:
		portsc = PORTSC_PTS(PTS_UTMI);
		lpm = DEVLC_PTS(PTS_UTMI);
		break;
	case USBPHY_INTERFACE_MODE_UTMIW:
		portsc = PORTSC_PTS(PTS_UTMI) | PORTSC_PTW;
		lpm = DEVLC_PTS(PTS_UTMI) | DEVLC_PTW;
		break;
	case USBPHY_INTERFACE_MODE_ULPI:
		portsc = PORTSC_PTS(PTS_ULPI);
		lpm = DEVLC_PTS(PTS_ULPI);
		break;
	case USBPHY_INTERFACE_MODE_SERIAL:
		portsc = PORTSC_PTS(PTS_SERIAL);
		lpm = DEVLC_PTS(PTS_SERIAL);
		break;
	case USBPHY_INTERFACE_MODE_HSIC:
		portsc = PORTSC_PTS(PTS_HSIC);
		lpm = DEVLC_PTS(PTS_HSIC);
		break;
	default:
		return;
	}

	if (ci->hw_bank.lpm)
		hw_write(ci, OP_DEVLC, DEVLC_PTS(7) | DEVLC_PTW, lpm);
	else
		hw_write(ci, OP_PORTSC, PORTSC_PTS(7) | PORTSC_PTW, portsc);
}

/**
 * hw_device_reset: resets chip (execute without interruption)
 * @ci: the controller
  *
 * This function returns an error code
 */
int hw_device_reset(struct ci13xxx *ci, u32 mode)
{
	/* should flush & stop before reset */
	hw_write(ci, OP_ENDPTFLUSH, ~0, ~0);
	hw_write(ci, OP_USBCMD, USBCMD_RS, 0);

	hw_write(ci, OP_USBCMD, USBCMD_RST, USBCMD_RST);
	while (hw_read(ci, OP_USBCMD, USBCMD_RST))
		udelay(10);		/* not RTOS friendly */


	if (ci->platdata->notify_event)
		ci->platdata->notify_event(ci,
			CI13XXX_CONTROLLER_RESET_EVENT);

	if (ci->platdata->flags & CI13XXX_DISABLE_STREAMING)
		hw_write(ci, OP_USBMODE, USBMODE_CI_SDIS, USBMODE_CI_SDIS);

	/* USBMODE should be configured step by step */
	hw_write(ci, OP_USBMODE, USBMODE_CM, USBMODE_CM_IDLE);
	hw_write(ci, OP_USBMODE, USBMODE_CM, mode);
	/* HW >= 2.3 */
	hw_write(ci, OP_USBMODE, USBMODE_SLOM, USBMODE_SLOM);

	if (hw_read(ci, OP_USBMODE, USBMODE_CM) != mode) {
		pr_err("cannot enter in %s mode", ci_role(ci)->name);
		pr_err("lpm = %i", ci->hw_bank.lpm);
		return -ENODEV;
	}

	return 0;
}

/**
 * ci_otg_role - pick role based on ID pin state
 * @ci: the controller
 */
static enum ci_role ci_otg_role(struct ci13xxx *ci)
{
	u32 sts = hw_read(ci, OP_OTGSC, ~0);
	enum ci_role role = sts & OTGSC_ID
		? CI_ROLE_GADGET
		: CI_ROLE_HOST;

	return role;
}

/**
 * hw_wait_reg: wait the register value
 *
 * Sometimes, it needs to wait register value before going on.
 * Eg, when switch to device mode, the vbus value should be lower
 * than OTGSC_BSV before connects to host.
 *
 * @ci: the controller
 * @reg: register index
 * @mask: mast bit
 * @value: the bit value to wait
 * @timeout: timeout to indicate an error
 *
 * This function returns an error code if timeout
 */
static int hw_wait_reg(struct ci13xxx *ci, enum ci13xxx_regs reg, u32 mask,
				u32 value, unsigned long timeout)
{
	unsigned long elapse = jiffies + timeout;

	while (hw_read(ci, reg, mask) != value) {
		if (time_after(jiffies, elapse)) {
			dev_err(ci->dev, "timeout waiting for %08x in %d\n",
					mask, reg);
			return -ETIMEDOUT;
		}
		msleep(20);
	}

	return 0;
}

#define CI_VBUS_STABLE_TIMEOUT 500
static void ci_handle_id_switch(struct ci13xxx *ci)
{
	enum ci_role role = ci_otg_role(ci);

	if (role != ci->role) {
		dev_dbg(ci->dev, "switching from %s to %s\n",
			ci_role(ci)->name, ci->roles[role]->name);

		/* 1. Finish the current role */
		ci_role_stop(ci);
		hw_device_reset(ci, USBMODE_CM_IDLE);

		/* 2. Turn on/off vbus according to coming role */
		if (role == CI_ROLE_GADGET)
			/* wait vbus lower than OTGSC_BSV */
			hw_wait_reg(ci, OP_OTGSC, OTGSC_BSV, 0,
					CI_VBUS_STABLE_TIMEOUT);

		/* 3. Begin the new role */
		ci_role_start(ci, role);

	}
}

void ci_handle_vbus_change(struct ci13xxx *ci)
{
	u32 otgsc = hw_read(ci, OP_OTGSC, ~0);

	if (otgsc & OTGSC_BSV)
		usb_gadget_vbus_connect(&ci->gadget);
	else
		usb_gadget_vbus_disconnect(&ci->gadget);
}

/**
 * ci_otg_work - perform otg (vbus/id) event handle
 * @work: work struct
 */
static void ci_otg_work(struct work_struct *work)
{
	struct ci13xxx *ci = container_of(work, struct ci13xxx, work);

	if (ci->id_event) {
		ci->id_event = false;
		/* Keep controller active during id switch */
		pm_runtime_get_sync(ci->dev);
		ci_handle_id_switch(ci);
		pm_runtime_put_sync(ci->dev);
	} else if (ci->b_sess_valid_event) {
		ci->b_sess_valid_event = false;
		ci_handle_vbus_change(ci);
	} else
		dev_err(ci->dev, "unexpected event occurs at %s\n", __func__);

	enable_irq(ci->irq);
}

static void ci_delayed_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ci13xxx *ci = container_of(dwork, struct ci13xxx, dwork);

	otg_set_vbus(&ci->otg, true);

}

static ssize_t show_role(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct ci13xxx *ci = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", ci_role(ci)->name);
}

static ssize_t store_role(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct ci13xxx *ci = dev_get_drvdata(dev);
	enum ci_role role;
	int ret;

	for (role = CI_ROLE_HOST; role < CI_ROLE_END; role++)
		if (ci->roles[role] && !strcmp(buf, ci->roles[role]->name))
			break;

	if (role == CI_ROLE_END || role == ci->role)
		return -EINVAL;

	ci_role_stop(ci);
	ret = ci_role_start(ci, role);
	if (ret)
		return ret;

	return count;
}

static DEVICE_ATTR(role, S_IRUSR | S_IWUSR, show_role, store_role);

static irqreturn_t ci_irq(int irq, void *data)
{
	struct ci13xxx *ci = data;
	irqreturn_t ret = IRQ_NONE;
	u32 otgsc = 0;

	if (atomic_read(&ci->in_lpm)) {
		disable_irq_nosync(irq);
		ci->wakeup_int = true;
		pm_runtime_get(ci->dev);
		return IRQ_HANDLED;
	}

	otgsc = hw_read(ci, OP_OTGSC, ~0);

	/*
	 * Handle id change interrupt, it indicates device/host function
	 * switch.
	 */
	if (ci->is_otg && (otgsc & OTGSC_IDIE) && (otgsc & OTGSC_IDIS)) {
		ci->id_event = true;
		ci_clear_otg_interrupt(ci, OTGSC_IDIS);
		disable_irq_nosync(ci->irq);
		queue_work(ci->wq, &ci->work);
		return IRQ_HANDLED;
	}

	/*
	 * Handle vbus change interrupt, it indicates device connection
	 * and disconnection events.
	 */
	if ((otgsc & OTGSC_BSVIE) && (otgsc & OTGSC_BSVIS)) {
		ci->b_sess_valid_event = true;
		ci_clear_otg_interrupt(ci, OTGSC_BSVIS);
		disable_irq_nosync(ci->irq);
		queue_work(ci->wq, &ci->work);
		return IRQ_HANDLED;
	}

	/* Handle device/host interrupt */
	if (ci->role != CI_ROLE_END)
		ret = ci_role(ci)->irq(ci);

	return ret;
}

static DEFINE_IDA(ci_ida);

struct platform_device *ci13xxx_add_device(struct device *dev,
			struct resource *res, int nres,
			struct ci13xxx_platform_data *platdata)
{
	struct platform_device *pdev;
	int id, ret;

	id = ida_simple_get(&ci_ida, 0, 0, GFP_KERNEL);
	if (id < 0)
		return ERR_PTR(id);

	pdev = platform_device_alloc("ci_hdrc", id);
	if (!pdev) {
		ret = -ENOMEM;
		goto put_id;
	}

	pdev->dev.parent = dev;
	pdev->dev.dma_mask = dev->dma_mask;
	pdev->dev.dma_parms = dev->dma_parms;
	dma_set_coherent_mask(&pdev->dev, dev->coherent_dma_mask);

	ret = platform_device_add_resources(pdev, res, nres);
	if (ret)
		goto err;

	ret = platform_device_add_data(pdev, platdata, sizeof(*platdata));
	if (ret)
		goto err;

	ret = platform_device_add(pdev);
	if (ret)
		goto err;

	return pdev;

err:
	platform_device_put(pdev);
put_id:
	ida_simple_remove(&ci_ida, id);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(ci13xxx_add_device);

void ci13xxx_remove_device(struct platform_device *pdev)
{
	int id = pdev->id;
	platform_device_unregister(pdev);
	ida_simple_remove(&ci_ida, id);
}
EXPORT_SYMBOL_GPL(ci13xxx_remove_device);

static int __devinit ci_hdrc_probe(struct platform_device *pdev)
{
	struct device	*dev = &pdev->dev;
	struct ci13xxx	*ci;
	struct resource	*res;
	void __iomem	*base;
	int		ret;
	u32		otgsc;
	enum usb_dr_mode dr_mode;

	if (!dev->platform_data) {
		dev_err(dev, "platform data missing\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "missing resource\n");
		return -ENODEV;
	}

	base = devm_request_and_ioremap(dev, res);
	if (!base) {
		dev_err(dev, "can't request and ioremap resource\n");
		return -ENOMEM;
	}

	ci = devm_kzalloc(dev, sizeof(*ci), GFP_KERNEL);
	if (!ci) {
		dev_err(dev, "can't allocate device\n");
		return -ENOMEM;
	}

	ci->dev = dev;
	ci->platdata = dev->platform_data;
	ci->reg_vbus = ci->platdata->reg_vbus;
	if (ci->platdata->phy)
		ci->transceiver = ci->platdata->phy;
	else
		ci->global_phy = true;

	ret = hw_device_init(ci, base);
	if (ret < 0) {
		dev_err(dev, "can't initialize hardware\n");
		return -ENODEV;
	}

	ci->hw_bank.phys = res->start;

	ci->irq = platform_get_irq(pdev, 0);
	if (ci->irq < 0) {
		dev_err(dev, "missing IRQ\n");
		return -ENODEV;
	}

	INIT_WORK(&ci->work, ci_otg_work);
	INIT_DELAYED_WORK(&ci->dwork, ci_delayed_work);
	ci->wq = create_singlethread_workqueue("ci_otg");
	if (!ci->wq) {
		dev_err(dev, "can't create workqueue\n");
		return -ENODEV;
	}

	/*
	 * Set wakeup capable before the child device is created.
	 * As at child device (roothub)'s init, it will set controller
	 * device as wakeup enabled. If we does not put it before creating
	 * roothub, the wakeup will not be enabled by default.
	 */
	device_set_wakeup_capable(&pdev->dev, true);

	dr_mode = ci->platdata->dr_mode;
	if (dr_mode == USB_DR_MODE_UNKNOWN)
		dr_mode = USB_DR_MODE_OTG;

	/* initialize role(s) before the interrupt is requested */
	if (dr_mode == USB_DR_MODE_OTG || dr_mode == USB_DR_MODE_HOST) {
		ret = ci_hdrc_host_init(ci);
		if (ret)
			dev_info(dev, "doesn't support host\n");
	}

	if (dr_mode == USB_DR_MODE_OTG || dr_mode == USB_DR_MODE_PERIPHERAL) {
		ret = ci_hdrc_gadget_init(ci);
		if (ret)
			dev_info(dev, "doesn't support gadget, ret=%d\n", ret);
	}

	if (!ci->roles[CI_ROLE_HOST] && !ci->roles[CI_ROLE_GADGET]) {
		dev_err(dev, "no supported roles\n");
		ret = -ENODEV;
		goto rm_wq;
	}

	if (ci->roles[CI_ROLE_HOST] && ci->roles[CI_ROLE_GADGET]) {
		dev_info(dev, "support otg\n");
		ci->is_otg = true;
		/* if otg is supported, create struct usb_otg */
		ci_hdrc_otg_init(ci);
		/* ID pin needs 1ms debouce time, we delay 2ms for safe */
		mdelay(2);
		ci->role = ci_otg_role(ci);
	} else {
		ci->role = ci->roles[CI_ROLE_HOST]
			? CI_ROLE_HOST
			: CI_ROLE_GADGET;
	}

	hw_phymode_configure(ci);

	otgsc = hw_read(ci, OP_OTGSC, ~0);

	/*
	 * If the gadget is supported, call its init unconditionally,
	 * We need to support load gadget module at init.rc.
	 */
	if (ci->roles[CI_ROLE_GADGET]) {
		ret = ci->roles[CI_ROLE_GADGET]->init(ci);
		if (ret) {
			dev_err(dev, "can't init %s role, ret=%d\n",
					ci_role(ci)->name, ret);
			ret = -ENODEV;
			goto rm_wq;
		}
	}

	if (ci->role == CI_ROLE_HOST) {
		ret = ci->roles[CI_ROLE_HOST]->init(ci);
		if (ret) {
			dev_err(dev, "can't init %s role, ret=%d\n",
					ci_role(ci)->name, ret);
			ret = -ENODEV;
			goto rm_wq;
		}
	}

	platform_set_drvdata(pdev, ci);
	ret = request_irq(ci->irq, ci_irq, IRQF_SHARED, ci->platdata->name,
			  ci);
	if (ret)
		goto stop;

	ret = device_create_file(dev, &dev_attr_role);
	if (ret)
		goto rm_attr;

	/* Handle the situation that usb device at the MicroB to A cable */
	if (ci->is_otg && !(otgsc & OTGSC_ID))
		queue_delayed_work(ci->wq, &ci->dwork, msecs_to_jiffies(500));

	/*
	 * Its parent controller driver will take pm responsibility
	 */
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_no_callbacks(&pdev->dev);

	return ret;

rm_attr:
	device_remove_file(dev, &dev_attr_role);
stop:
	ci_role_stop(ci);
rm_wq:
	flush_workqueue(ci->wq);
	destroy_workqueue(ci->wq);

	return ret;
}

static int ci_hdrc_remove(struct platform_device *pdev)
{
	struct ci13xxx *ci = platform_get_drvdata(pdev);

	flush_workqueue(ci->wq);
	destroy_workqueue(ci->wq);

	device_set_wakeup_capable(&pdev->dev, false);
	pm_runtime_get_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);

	device_remove_file(ci->dev, &dev_attr_role);
	free_irq(ci->irq, ci);
	ci_role_destroy(ci);

	/* Put the PHY into low power mode */
	hw_write(ci, OP_PORTSC, PORTSC_PHCD, PORTSC_PHCD);

	return 0;
}

static int ci_core_suspend(struct device *dev)
{
	return 0;
}

static int ci_core_resume(struct device *dev)
{
	int ret;

	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
}


static const struct dev_pm_ops ci_core_pm_ops = {
	.suspend		= ci_core_suspend,
	.resume			= ci_core_resume,
};
static struct platform_driver ci_hdrc_driver = {
	.probe	= ci_hdrc_probe,
	.remove	= __devexit_p(ci_hdrc_remove),
	.driver	= {
		.name	= "ci_hdrc",
		.pm	= &ci_core_pm_ops,
	},
#ifdef CONFIG_PM
#endif
};

module_platform_driver(ci_hdrc_driver);

MODULE_ALIAS("platform:ci_hdrc");
MODULE_ALIAS("platform:ci13xxx");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("David Lopo <dlopo@chipidea.mips.com>");
MODULE_DESCRIPTION("ChipIdea HDRC Driver");
