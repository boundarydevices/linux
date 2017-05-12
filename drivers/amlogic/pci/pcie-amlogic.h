/*
 * drivers/amlogic/pci/pcie-amlogic.h
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

#ifndef __AMLOGIC_PCI_H__
#define __AMLOGIC_PCI_H__

/* PCIe Port Logic registers */
#define PLR_OFFSET			0x700
#define PCIE_PORT_LINK_CTRL_OFF	(PLR_OFFSET + 0x10)
#define PCIE_GEN2_CTRL_OFF	(PLR_OFFSET + 0x10c)

#define TYPE1_HDR_OFFSET	0X0
#define PCIE_BASE_ADDR0		(TYPE1_HDR_OFFSET + 0X10)
#define PCIE_BASE_ADDR1		(TYPE1_HDR_OFFSET + 0x14)

#define PCIE_CFG0			0x0

#define APP_LTSSM_ENABLE	(1<<7)
#define PCIE_CFG_STATUS12	0x30
#define PCIE_CFG_STATUS17	0x44

#define    WAIT_LINKUP_TIMEOUT         5000

enum pcie_data_rate {
	PCIE_GEN1,
	PCIE_GEN2,
	PCIE_GEN3,
	PCIE_GEN4
};

#define PCIE_CAP_OFFSET	0x70
#define PCIE_DEV_CTRL_DEV_STUS	(PCIE_CAP_OFFSET + 0x08)

#endif
