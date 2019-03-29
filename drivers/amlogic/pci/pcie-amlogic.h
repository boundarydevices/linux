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

/* PCIe Port Logic registers */
#define PLR_OFFSET			0x700
#define PCIE_PHY_DEBUG_R1		(PLR_OFFSET + 0x2c)
#define PCIE_PHY_DEBUG_R1_LINK_UP	(0x1 << 4)
#define PCIE_PHY_DEBUG_R1_LINK_IN_TRAINING	(0x1 << 29)

#define    WAIT_LINKUP_TIMEOUT         2000

enum pcie_data_rate {
	PCIE_GEN1,
	PCIE_GEN2,
	PCIE_GEN3,
	PCIE_GEN4
};

#define PCIE_CAP_OFFSET	0x70
#define PCIE_DEV_CTRL_DEV_STUS	(PCIE_CAP_OFFSET + 0x08)

union phy_r0 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_test_powerdown:1;
		unsigned phy_ref_use_pad:1;
		unsigned pipe_port_sel:2;
		unsigned pcs_common_clocks:1;
		unsigned reserved:27;
	} b;
};

union phy_r1 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_tx1_term_offset:5;
		unsigned phy_tx0_term_offset:5;
		unsigned phy_rx1_eq:3;
		unsigned phy_rx0_eq:3;
		unsigned phy_los_level:5;
		unsigned phy_los_bias:3;
		unsigned phy_ref_clkdiv2:1;
		unsigned phy_mpll_multiplier:7;
	} b;
};

union phy_r2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned pcs_tx_deemph_gen2_6db:6;
		unsigned pcs_tx_deemph_gen2_3p5db:6;
		unsigned pcs_tx_deemph_gen1:6;
		unsigned phy_tx_vboost_lvl:3;
		unsigned reserved:11;
	} b;
};

union phy_r3 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned pcs_tx_swing_low:7;
		unsigned pcs_tx_swing_full:7;
		unsigned reserved:18;
	} b;
};

union phy_r4 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_cr_write:1;
		unsigned phy_cr_read:1;
		unsigned phy_cr_data_in:16;
		unsigned phy_cr_cap_data:1;
		unsigned phy_cr_cap_addr:1;
		unsigned reserved:12;
	} b;
};

union phy_r5 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_cr_data_out:16;
		unsigned phy_cr_ack:1;
		unsigned phy_bs_out:1;
		unsigned reserved:14;
	} b;
};

union phy_r6 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_bs_update_dr:1;
		unsigned phy_bs_shift_dr:1;
		unsigned phy_bs_preload:1;
		unsigned phy_bs_invert:1;
		unsigned phy_bs_init:1;
		unsigned phy_bs_in:1;
		unsigned phy_bs_highz:1;
		unsigned phy_bs_extest_ac:1;
		unsigned phy_bs_extest:1;
		unsigned phy_bs_clk:1;
		unsigned phy_bs_clamp:1;
		unsigned phy_bs_capture_dr:1;
		unsigned phy_acjt_level:5;
		unsigned reserved:15;
	} b;
};

struct pcie_phy_aml_regs {
	void __iomem    *pcie_phy_r[7];
};

struct pcie_phy {
	u32 power_state;
	u32 device_attch;
	u32 reset_state;
	void __iomem		*phy_base;	/* DT 1st resource */
	void __iomem		*reset_base;/* DT 3nd resource */
	u32 pcie_ctrl_sleep_shift;
	u32 pcie_hhi_mem_pd_mask;
	u32 pcie_ctrl_iso_shift;
	u32 pcie_hhi_mem_pd_shift;
};


#endif
