/*
 * include/linux/amlogic/usbtype.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __USB_CLK_HEADER_
#define __USB_CLK_HEADER_

#include <linux/platform_device.h>

#define MESON8		"meson8"
#define G9TV		"g9TV"
#define GXBABY		"gxbaby"
#define GXBABYTV	"gxtvbaby"
#define GXL		"gxl"
#define V2		"v2"

#define USB_NORMAL	0
#define USB_HOST_ONLY	1
#define USB_DEVICE_ONLY  2
#define	USB_OTG          3

#define USB_BC_MODE_DISCONNECT	0	/* Disconnected */
#define USB_BC_MODE_SDP		1	/* PC */
#define USB_BC_MODE_DCP		2	/* Charger */
#define USB_BC_MODE_CDP		3	/* PC + Charger */

#define USB_CORE_RESET_TIME	10
#define USB_ID_CHANGE_TIME	100

enum usb3_phy_func_e {
	AML_USB3_PHY_DISABLE = 0,
	AML_USB3_PHY_ENABLE,
};

enum usb_port_type_e {
	USB_PORT_TYPE_OTG = 0,
	USB_PORT_TYPE_HOST,
	USB_PORT_TYPE_SLAVE,
};

enum usb_port_speed_e {
	USB_PORT_SPEED_DEFAULT = 0,
	USB_PORT_SPEED_HIGH,
	USB_PORT_SPEED_FULL
};

enum usb_dma_config_e {
	USB_DMA_BURST_DEFAULT = 0,
	USB_DMA_BURST_SINGLE,
	USB_DMA_BURST_INCR,
	USB_DMA_BURST_INCR4,
	USB_DMA_BURST_INCR8,
	USB_DMA_BURST_INCR16,
	USB_DMA_DISABLE,
};

enum usb_phy_id_mode_e {
	USB_PHY_ID_MODE_HW = 0,
	USB_PHY_ID_MODE_SW_HOST,
	USB_PHY_ID_MODE_SW_SLAVE
};

enum usb_port_idx_e {
	USB_PORT_IDX_A = 0,
	USB_PORT_IDX_B,
	USB_PORT_IDX_C,
	USB_PORT_IDX_D
};

enum lm_device_type_e {
	LM_DEVICE_TYPE_USB = 0,
	LM_DEVICE_TYPE_SATA = 1,
};

int clk_enable_usb(struct platform_device *pdev, const char *s_clock_name,
	unsigned long usb_peri_reg, const char *cpu_type, int controller_type);

int clk_disable_usb(struct platform_device *pdev, const char *s_clock_name,
		unsigned long usb_peri_reg, const char *cpu_type);

int clk_resume_usb(struct platform_device *pdev, const char *s_clock_name,
		unsigned long usb_peri_reg, const char *cpu_type);

int clk_suspend_usb(struct platform_device *pdev, const char *s_clock_name,
		unsigned long usb_peri_reg, const char *cpu_type);

int device_status(unsigned long usb_peri_reg);
int device_status_v2(unsigned long usb_peri_reg);

extern int dwc_otg_power_register_notifier(struct notifier_block *nb);
extern int dwc_otg_power_unregister_notifier(struct notifier_block *nb);
extern int dwc_otg_charger_detect_register_notifier(struct notifier_block *nb);
#endif
