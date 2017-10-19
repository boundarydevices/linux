/*
 * include/linux/amlogic/usb-gxl.h
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

#ifndef __USB_GXL_HEADER_
#define __USB_GXL_HEADER_

#include <linux/usb/phy.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>

#define PHY_REGISTER_SIZE	0x20
/* Register definitions */

int aml_new_usb_register_notifier(struct notifier_block *nb);
int aml_new_usb_unregister_notifier(struct notifier_block *nb);

struct u2p_aml_regs_t {
	void __iomem	*u2p_r[3];
};

union u2p_r0_t {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned bypass_sel:1;
		unsigned bypass_dm_en:1;
		unsigned bypass_dp_en:1;
		unsigned txbitstuffenh:1;
		unsigned txbitstuffen:1;
		unsigned dmpulldown:1;
		unsigned dppulldown:1;
		unsigned vbusvldextsel:1;
		unsigned vbusvldext:1;
		unsigned adp_prb_en:1;
		unsigned adp_dischrg:1;
		unsigned adp_chrg:1;
		unsigned drvvbus:1;
		unsigned idpullup:1;
		unsigned loopbackenb:1;
		unsigned otgdisable:1;
		unsigned commononn:1;
		unsigned fsel:3;
		unsigned refclksel:2;
		unsigned por:1;
		unsigned vatestenb:2;
		unsigned set_iddq:1;
		unsigned ate_reset:1;
		unsigned fsv_minus:1;
		unsigned fsv_plus:1;
		unsigned bypass_dm_data:1;
		unsigned bypass_dp_data:1;
		unsigned not_used:1;
	} b;
};

union u2p_r1_t {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned burn_in_test:1;
		unsigned aca_enable:1;
		unsigned dcd_enable:1;
		unsigned vdatsrcenb:1;
		unsigned vdatdetenb:1;
		unsigned chrgsel:1;
		unsigned tx_preemp_pulse_tune:1;
		unsigned tx_preemp_amp_tune:2;
		unsigned tx_res_tune:2;
		unsigned tx_rise_tune:2;
		unsigned tx_vref_tune:4;
		unsigned tx_fsls_tune:4;
		unsigned tx_hsxv_tune:2;
		unsigned otg_tune:3;
		unsigned sqrx_tune:3;
		unsigned comp_dis_tune:3;
	} b;
};

union u2p_r2_t {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned data_in:4;
		unsigned data_in_en:4;
		unsigned addr:4;
		unsigned data_out_sel:1;
		unsigned clk:1;
		unsigned data_out:4;
		unsigned aca_pin_range_c:1;
		unsigned aca_pin_range_b:1;
		unsigned aca_pin_range_a:1;
		unsigned aca_pin_gnd:1;
		unsigned aca_pin_float:1;
		unsigned chg_det:1;
		unsigned device_sess_vld:1;
		unsigned adp_probe:1;
		unsigned adp_sense:1;
		unsigned sessend:1;
		unsigned vbusvalid:1;
		unsigned bvalid:1;
		unsigned avalid:1;
		unsigned iddig:1;
	} b;
};

struct usb_aml_regs_t {
	void __iomem	*usb_r[7];
};

union usb_r0_t {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned p30_fsel:6;
		unsigned p30_phy_reset:1;
		unsigned p30_test_powerdown_hsp:1;
		unsigned p30_test_powerdown_ssp:1;
		unsigned p30_acjt_level:5;
		unsigned p30_tx_vboost_lvl:3;
		unsigned p30_lane0_tx2rx_loopbk:1;
		unsigned p30_lane0_ext_pclk_req:1;
		unsigned p30_pcs_rx_los_mask_val:10;
		unsigned u2d_ss_scaledown_mode:2;
		unsigned u2d_act:1;
	} b;
};

union usb_r1_t {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned u3h_bigendian_gs:1;
		unsigned u3h_pme_en:1;
		unsigned u3h_hub_port_overcurrent:5;
		unsigned u3h_hub_port_perm_attach:5;
		unsigned u3h_host_u2_port_disable:4;
		unsigned u3h_host_u3_port_disable:1;
		unsigned u3h_host_port_power_control_present:1;
		unsigned u3h_host_msi_enable:1;
		unsigned u3h_fladj_30mhz_reg:6;
		unsigned p30_pcs_tx_swing_full:7;
	} b;
};

union usb_r2_t {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned p30_cr_data_in:16;
		unsigned p30_cr_read:1;
		unsigned p30_cr_write:1;
		unsigned p30_cr_cap_addr:1;
		unsigned p30_cr_cap_data:1;
		unsigned p30_pcs_tx_deemph_3p5db:6;
		unsigned p30_pcs_tx_deemph_6db:6;
	} b;
};

union usb_r3_t {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned p30_ssc_en:1;
		unsigned p30_ssc_range:3;
		unsigned p30_ssc_ref_clk_sel:9;
		unsigned p30_ref_ssp_en:1;
		unsigned reserved14:2;
		unsigned p30_los_bias:3;
		unsigned p30_los_level:5;
		unsigned p30_mpll_multiplier:7;
		unsigned reserved31:1;
	} b;
};

union usb_r4_t {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned p21_PORTRESET0:1;
		unsigned p21_SLEEPM0:1;
		unsigned mem_pd:2;
		unsigned p21_only:1;
		unsigned reserved4:27;
	} b;
};

union usb_r5_t {
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
		unsigned iddig_irq:1;
		unsigned iddig_th:8;
		unsigned iddig_cnt:8;
		unsigned reserved5:8;
	} b;
};

union usb_r6_t {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned p30_cr_data_out:16;
		unsigned p30_cr_ack:1;
		unsigned not_used:15;
	} b;
};

struct amlogic_usb {
	struct usb_phy		phy;
	struct device		*dev;
	void __iomem	*regs;
	void __iomem	*reset_regs;
	/* Set VBus Power though GPIO */
	int vbus_power_pin;
	int vbus_power_pin_work_mask;
	struct delayed_work	work;
	struct gpio_desc *usb_gpio_desc;

	int portnum;
	int suspend_flag;
};

#endif
