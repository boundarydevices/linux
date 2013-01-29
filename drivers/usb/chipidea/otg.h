/*
 * Copyright (C) 2013 Freescale Semiconductor, Inc.
 *
 * Author: Peter Chen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DRIVERS_USB_CHIPIDEA_OTG_H
#define __DRIVERS_USB_CHIPIDEA_OTG_H

int ci_hdrc_otg_init(struct ci13xxx *ci);
void ci_clear_otg_interrupt(struct ci13xxx *ci, u32 bits);
void ci_enable_otg_interrupt(struct ci13xxx *ci, u32 bits);
void ci_disable_otg_interrupt(struct ci13xxx *ci, u32 bits);

#endif /* __DRIVERS_USB_CHIPIDEA_OTG_H */
