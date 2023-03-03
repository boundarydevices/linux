/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * This header provides constants for i.MX8 PCIe.
 */

#ifndef _DT_BINDINGS_IMX8_PCIE_H
#define _DT_BINDINGS_IMX8_PCIE_H

/* Reference clock PAD mode */
#define IMX8_PCIE_REFCLK_PAD_UNUSED	0
#define IMX8_PCIE_REFCLK_PAD_INPUT	1
#define IMX8_PCIE_REFCLK_PAD_OUTPUT	2

/*
 * i.MX8QM HSIO(High Speed IO) module has three instances of single lane
 * SERDES PHY and an instance of two lanes PCIe GEN3 controller, an
 * instance of single lane PCIe GEN3 controller, as well as an instance
 * of SATA 3.0 controller.
 *
 * And HSIO module can be configured as the following different usecases.
 * 1 - A two lanes PCIea and a single lane SATA.
 * 2 - A single lane PCIea, a single lane PCIeb and a single lane SATA.
 * 3 - A two lanes PCIea, a single lane PCIeb.
 * Choose one mode, refer to the exact hardware board design.
 */
#define	PCIEAX2SATA		1
#define	PCIEAX1PCIEBX1SATA	2
#define	PCIEAX2PCIEBX1		3

#endif /* _DT_BINDINGS_IMX8_PCIE_H */
