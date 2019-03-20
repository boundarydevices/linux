/*
 * include/linux/amlogic/usb-v2.h
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

#ifndef __USB_V2_HEADER_
#define __USB_V2_HEADER_

#include <linux/usb/phy.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/clk.h>

#define PHY_REGISTER_SIZE	0x20
/* Register definitions */

int aml_new_usb_v2_register_notifier(struct notifier_block *nb);
int aml_new_usb_v2_unregister_notifier(struct notifier_block *nb);

struct u2p_aml_regs_v2 {
	void __iomem	*u2p_r_v2[2];
};

union u2p_r0_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned host_device:1;
		unsigned power_ok:1;
		unsigned hast_mode:1;
		unsigned POR:1;
		unsigned IDPULLUP0:1;
		unsigned DRVVBUS0:1;
		unsigned reserved:26;
	} b;
};

union u2p_r1_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_rdy:1;
		unsigned IDDIG0:1;
		unsigned OTGSESSVLD0:1;
		unsigned VBUSVALID0:1;
		unsigned reserved:28;
	} b;
};

struct usb_aml_regs_v2 {
	void __iomem	*usb_r_v2[6];
};

union usb_r0_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned reserved:17;
		unsigned p30_lane0_tx2rx_loopback:1;
		unsigned p30_lane0_ext_pclk_reg:1;
		unsigned p30_pcs_rx_los_mask_val:10;
		unsigned u2d_ss_scaledown_mode:2;
		unsigned u2d_act:1;
	} b;
};

union usb_r1_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned u3h_bigendian_gs:1;
		unsigned u3h_pme_en:1;
		unsigned u3h_hub_port_overcurrent:3;
		unsigned reserved_1:2;
		unsigned u3h_hub_port_perm_attach:3;
		unsigned reserved_2:2;
		unsigned u3h_host_u2_port_disable:2;
		unsigned reserved_3:2;
		unsigned u3h_host_u3_port_disable:1;
		unsigned u3h_host_port_power_control_present:1;
		unsigned u3h_host_msi_enable:1;
		unsigned u3h_fladj_30mhz_reg:6;
		unsigned p30_pcs_tx_swing_full:7;
	} b;
};

union usb_r2_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned reserved:20;
		unsigned p30_pcs_tx_deemph_3p5db:6;
		unsigned p30_pcs_tx_deemph_6db:6;
	} b;
};

union usb_r3_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned p30_ssc_en:1;
		unsigned p30_ssc_range:3;
		unsigned p30_ssc_ref_clk_sel:9;
		unsigned p30_ref_ssp_en:1;
		unsigned reserved:18;
	} b;
};

union usb_r4_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned p21_PORTRESET0:1;
		unsigned p21_SLEEPM0:1;
		unsigned mem_pd:2;
		unsigned p21_only:1;
		unsigned reserved:27;
	} b;
};

union usb_r5_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned iddig_sync:1;
		unsigned iddig_reg:1;
		unsigned iddig_cfg:2;
		unsigned iddig_en0:1;
		unsigned iddig_en1:1;
		unsigned iddig_curr:1;
		unsigned usb_iddig_irq:1;
		unsigned iddig_th:8;
		unsigned iddig_cnt:8;
		unsigned reserved:8;
	} b;
};

struct amlogic_usb_v2 {
	struct usb_phy		phy;
	struct device		*dev;
	void __iomem	*regs;
	void __iomem	*reset_regs;
	void __iomem	*phy_cfg[4];
	void __iomem	*phy3_cfg;
	void __iomem	*phy3_cfg_r1;
	void __iomem	*phy3_cfg_r2;
	void __iomem	*phy3_cfg_r4;
	void __iomem	*phy3_cfg_r5;
	void __iomem	*usb2_phy_cfg;
	void __iomem	*power_base;
	void __iomem	*hhi_mem_pd_base;
	u32 pll_setting[8];
	int phy_cfg_state[4];
	/* Set VBus Power though GPIO */
	int vbus_power_pin;
	int vbus_power_pin_work_mask;
	struct delayed_work	work;
	struct gpio_desc *usb_gpio_desc;

	int portnum;
	int suspend_flag;
	int phy_version;
	int pwr_ctl;
	struct clk		*clk;
};

union phy3_r1 {
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


union phy3_r2 {
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


union phy3_r4 {
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

union phy3_r5 {
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


#endif
