/*
 * include/linux/amlogic/wifi_dt.h
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

#ifndef _wifi_dt_h_
#define _wifi_dt_h_

extern void sdio_reinit(void);
extern char *get_wifi_inf(void);

void extern_wifi_set_enable(int is_on);
int wifi_irq_num(void);

/*amlogic 4.9 kernel support pci interface wifi*/
extern void pci_lock_rescan_remove(void);
extern struct pci_bus *pci_find_next_bus(const struct pci_bus *from);
extern unsigned int pci_rescan_bus(struct pci_bus *bus);
extern void pci_unlock_rescan_remove(void);
extern struct pci_dev *pci_get_device(unsigned int vendor, unsigned int device,
			       struct pci_dev *from);
extern void pci_stop_and_remove_bus_device_locked(struct pci_dev *dev);

#endif /* _wifi_dt_h_ */
