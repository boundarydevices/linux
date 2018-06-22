/*
 * drivers/amlogic/pinctrl/pinctrl_gxl.c
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

#include "pinctrl-meson.h"
#include <linux/arm-smccc.h>
#include <dt-bindings/gpio/gxl.h>
#include "pinctrl-meson8-pmx.h"

#define HHI_XTAL_DIVN_CNTL_GPIO (0xc883c000 + (0x2f << 2))

static const struct pinctrl_pin_desc meson_gxl_periphs_pins[] = {
	MESON_PIN(GPIOZ_0),
	MESON_PIN(GPIOZ_1),
	MESON_PIN(GPIOZ_2),
	MESON_PIN(GPIOZ_3),
	MESON_PIN(GPIOZ_4),
	MESON_PIN(GPIOZ_5),
	MESON_PIN(GPIOZ_6),
	MESON_PIN(GPIOZ_7),
	MESON_PIN(GPIOZ_8),
	MESON_PIN(GPIOZ_9),
	MESON_PIN(GPIOZ_10),
	MESON_PIN(GPIOZ_11),
	MESON_PIN(GPIOZ_12),
	MESON_PIN(GPIOZ_13),
	MESON_PIN(GPIOZ_14),
	MESON_PIN(GPIOZ_15),
	MESON_PIN(GPIOH_0),
	MESON_PIN(GPIOH_1),
	MESON_PIN(GPIOH_2),
	MESON_PIN(GPIOH_3),
	MESON_PIN(GPIOH_4),
	MESON_PIN(GPIOH_5),
	MESON_PIN(GPIOH_6),
	MESON_PIN(GPIOH_7),
	MESON_PIN(GPIOH_8),
	MESON_PIN(GPIOH_9),
	MESON_PIN(BOOT_0),
	MESON_PIN(BOOT_1),
	MESON_PIN(BOOT_2),
	MESON_PIN(BOOT_3),
	MESON_PIN(BOOT_4),
	MESON_PIN(BOOT_5),
	MESON_PIN(BOOT_6),
	MESON_PIN(BOOT_7),
	MESON_PIN(BOOT_8),
	MESON_PIN(BOOT_9),
	MESON_PIN(BOOT_10),
	MESON_PIN(BOOT_11),
	MESON_PIN(BOOT_12),
	MESON_PIN(BOOT_13),
	MESON_PIN(BOOT_14),
	MESON_PIN(BOOT_15),
	MESON_PIN(CARD_0),
	MESON_PIN(CARD_1),
	MESON_PIN(CARD_2),
	MESON_PIN(CARD_3),
	MESON_PIN(CARD_4),
	MESON_PIN(CARD_5),
	MESON_PIN(CARD_6),
	MESON_PIN(GPIODV_0),
	MESON_PIN(GPIODV_1),
	MESON_PIN(GPIODV_2),
	MESON_PIN(GPIODV_3),
	MESON_PIN(GPIODV_4),
	MESON_PIN(GPIODV_5),
	MESON_PIN(GPIODV_6),
	MESON_PIN(GPIODV_7),
	MESON_PIN(GPIODV_8),
	MESON_PIN(GPIODV_9),
	MESON_PIN(GPIODV_10),
	MESON_PIN(GPIODV_11),
	MESON_PIN(GPIODV_12),
	MESON_PIN(GPIODV_13),
	MESON_PIN(GPIODV_14),
	MESON_PIN(GPIODV_15),
	MESON_PIN(GPIODV_16),
	MESON_PIN(GPIODV_17),
	MESON_PIN(GPIODV_18),
	MESON_PIN(GPIODV_19),
	MESON_PIN(GPIODV_20),
	MESON_PIN(GPIODV_21),
	MESON_PIN(GPIODV_22),
	MESON_PIN(GPIODV_23),
	MESON_PIN(GPIODV_24),
	MESON_PIN(GPIODV_25),
	MESON_PIN(GPIODV_26),
	MESON_PIN(GPIODV_27),
	MESON_PIN(GPIODV_28),
	MESON_PIN(GPIODV_29),
	MESON_PIN(GPIOX_0),
	MESON_PIN(GPIOX_1),
	MESON_PIN(GPIOX_2),
	MESON_PIN(GPIOX_3),
	MESON_PIN(GPIOX_4),
	MESON_PIN(GPIOX_5),
	MESON_PIN(GPIOX_6),
	MESON_PIN(GPIOX_7),
	MESON_PIN(GPIOX_8),
	MESON_PIN(GPIOX_9),
	MESON_PIN(GPIOX_10),
	MESON_PIN(GPIOX_11),
	MESON_PIN(GPIOX_12),
	MESON_PIN(GPIOX_13),
	MESON_PIN(GPIOX_14),
	MESON_PIN(GPIOX_15),
	MESON_PIN(GPIOX_16),
	MESON_PIN(GPIOX_17),
	MESON_PIN(GPIOX_18),
	MESON_PIN(GPIOCLK_0),
	MESON_PIN(GPIOCLK_1),
};

/*emmc & nand*/
static const unsigned int emmc_nand_d07_pins[] = {
	BOOT_0, BOOT_1, BOOT_2,
	BOOT_3, BOOT_4, BOOT_5,
	BOOT_6, BOOT_7,
};

/*nand*/
static const unsigned int nand_ce0_pins[] = { BOOT_8 };
static const unsigned int nand_ce1_pins[] = { BOOT_9 };
static const unsigned int nand_rb0_pins[] = { BOOT_10 };
static const unsigned int nand_ale_pins[] = { BOOT_11 };
static const unsigned int nand_cle_pins[] = { BOOT_12 };
static const unsigned int nand_wen_clk_pins[] = { BOOT_13 };
static const unsigned int nand_ren_wr_pins[] = { BOOT_14 };
static const unsigned int nand_dqs_pins[] = { BOOT_15 };

/*emmc*/
static const unsigned int emmc_clk_pins[] = { BOOT_8 };
static const unsigned int emmc_cmd_pins[] = { BOOT_10 };
static const unsigned int emmc_ds_pins[] = { BOOT_15 };

/*nor*/
static const unsigned int nor_d_pins[] = { BOOT_11 };
static const unsigned int nor_q_pins[] = { BOOT_12 };
static const unsigned int nor_c_pins[] = { BOOT_13 };
static const unsigned int nor_cs_pins[] = { BOOT_15 };

/*sacard*/
static const unsigned int sdcard_d1_pins[] = { CARD_0 };
static const unsigned int sdcard_d0_pins[] = { CARD_1 };
static const unsigned int sdcard_clk_pins[] = { CARD_2 };
static const unsigned int sdcard_cmd_pins[] = { CARD_3 };
static const unsigned int sdcard_d3_pins[] = { CARD_4 };
static const unsigned int sdcard_d2_pins[] = { CARD_5 };

/*sdio*/
static const unsigned int sdio_d0_pins[] = { GPIOX_0 };
static const unsigned int sdio_d1_pins[] = { GPIOX_1 };
static const unsigned int sdio_d2_pins[] = { GPIOX_2 };
static const unsigned int sdio_d3_pins[] = { GPIOX_3 };
static const unsigned int sdio_clk_pins[] = { GPIOX_4 };
static const unsigned int sdio_cmd_pins[] = { GPIOX_5 };
static const unsigned int sdio_irq_pins[] = { GPIOX_7 };

/*jtag*/
static const unsigned int jtag_clk_0_pins[] = { GPIOH_6 };
static const unsigned int jtag_tms_0_pins[] = { GPIOH_7 };
static const unsigned int jtag_tdi_0_pins[] = { GPIOH_8 };
static const unsigned int jtag_tdo_0_pins[] = { GPIOH_9 };

static const unsigned int jtag_tdi_1_pins[] = { CARD_0 };
static const unsigned int jtag_tdo_1_pins[] = { CARD_1 };
static const unsigned int jtag_clk_1_pins[] = { CARD_2 };
static const unsigned int jtag_tms_1_pins[] = { CARD_3 };

/*pwm_a*/
static const unsigned int pwm_a_pins[] = { GPIOX_6 };

/*pwm_b*/
static const unsigned int pwm_b_pins[] = { GPIODV_29 };

/*pwm_c*/
static const unsigned int pwm_c_pins[] = { GPIOZ_15 };

/*pwm_d*/
static const unsigned int pwm_d_pins[] = { GPIODV_28 };

/*pwm_e*/
static const unsigned int pwm_e_pins[] = { GPIOX_16 };

/*pwm_f*/
static const unsigned int pwm_f_x7_pins[] = { GPIOX_7 };
static const unsigned int pwm_f_clk1_pins[] = { GPIOCLK_1 };

/*pcm*/
static const unsigned int pcm_out_a_pins[] = { GPIOX_8 };
static const unsigned int pcm_in_a_pins[] = { GPIOX_9 };
static const unsigned int pcm_fs_a_pins[] = { GPIOX_10 };
static const unsigned int pcm_clk_a_pins[] = { GPIOX_11 };

/*spi*/
static const unsigned int spi_sclk_0_pins[] = { GPIOZ_11 };
static const unsigned int spi_miso_0_pins[] = { GPIOZ_12 };
static const unsigned int spi_mosi_0_pins[] = { GPIOZ_13 };

static const unsigned int spi_mosi_1_pins[] = { GPIOX_8 };
static const unsigned int spi_miso_1_pins[] = { GPIOX_9 };
static const unsigned int spi_sclk_1_pins[] = { GPIOX_11 };

/*hdmi_hpd*/
static const unsigned int hdmi_hpd_pins[] = { GPIOH_0 };

/*hdmi_ddc*/
static const unsigned int hdmi_sda_pins[] = { GPIOH_1 };
static const unsigned int hdmi_scl_pins[] = { GPIOH_2 };

/*spdif_out*/
static const unsigned int spdif_out_pins[] = { GPIOH_4 };

/*spdif_in*/
static const unsigned int spdif_in_h4_pins[] = { GPIOH_4 };
static const unsigned int spdif_in_z14_pins[] = { GPIOZ_14 };

/*i2s*/
static const unsigned int i2s_am_clk_pins[] = { GPIOH_6 };
static const unsigned int i2s_ao_clk_out_pins[] = { GPIOH_7 };
static const unsigned int i2s_lr_clk_out_pins[] = { GPIOH_8 };
static const unsigned int i2sout_ch01_pins[] = { GPIOH_9 };
static const unsigned int i2sout_ch23_z5_pins[] = { GPIOZ_5 };
static const unsigned int i2sout_ch45_z6_pins[] = { GPIOZ_6 };
static const unsigned int i2sout_ch67_z7_pins[] = { GPIOZ_7 };

static const unsigned int i2sin_ch23_pins[] = { GPIOZ_2 };
static const unsigned int i2sin_ch45_pins[] = { GPIOZ_3 };
static const unsigned int i2sin_ch67_pins[] = { GPIOZ_4 };

/*tsin_a*/
static const unsigned int tsin_sop_a_x8_pins[] = { GPIOX_8 };
static const unsigned int tsin_d_valid_a_x9_pins[] = { GPIOX_9 };
static const unsigned int tsin_d0_a_x10_pins[] = { GPIOX_10 };
static const unsigned int tsin_clk_a_x11_pins[] = { GPIOX_11 };

static const unsigned int tsin_d0_a_dv0_pins[] = { GPIODV_0 };
static const unsigned int tsin_clk_a_dv8_pins[] = { GPIODV_8 };
static const unsigned int tsin_sop_a_dv9_pins[] = { GPIODV_9 };
static const unsigned int tsin_d_valid_a_dv10_pins[] = {
	GPIODV_10
};

static const unsigned int tsin_fail_a_dv11_pins[] = { GPIODV_11 };
static const unsigned int tsin_d1_7_a_dv1_7_pins[] = {
	GPIODV_1, GPIODV_2,
	GPIODV_3, GPIODV_4,
	GPIODV_5, GPIODV_6,
	GPIODV_7
};

/*tsin_b*/
static const unsigned int tsin_clk_b_h6_pins[] = { GPIOH_6 };
static const unsigned int tsin_d0_b_h7_pins[] = { GPIOH_7 };
static const unsigned int tsin_sop_b_h8_pins[] = { GPIOH_8 };
static const unsigned int tsin_d_valid_b_h9_pins[] = { GPIOH_9 };

static const unsigned int tsin_d_valid_b_z0_pins[] = { GPIOZ_0 };
static const unsigned int tsin_sop_b_z1_pins[] = { GPIOZ_1 };
static const unsigned int tsin_d0_b_z2_pins[] = { GPIOZ_2 };
static const unsigned int tsin_clk_b_z3_pins[] = { GPIOZ_3 };

static const unsigned int tsin_fail_b_z4_pins[] = { GPIOZ_4 };

/*lcd*/
static const unsigned int lcd_r0_1_pins[] = {
	GPIODV_0, GPIODV_1,
};

static const unsigned int lcd_r2_7_pins[] = {
	GPIODV_2, GPIODV_3,
	GPIODV_4, GPIODV_5,
	GPIODV_6, GPIODV_7,
};
static const unsigned int lcd_g0_1_pins[] = {
	GPIODV_8, GPIODV_9,
};
static const unsigned int lcd_g2_7_pins[] = {
	GPIODV_10, GPIODV_11,
	GPIODV_12, GPIODV_13,
	GPIODV_14, GPIODV_15,
};
static const unsigned int lcd_b0_1_pins[] = {
	GPIODV_16, GPIODV_17,
};
static const unsigned int lcd_b2_7_pins[] = {
	GPIODV_18, GPIODV_19,
	GPIODV_20, GPIODV_21,
	GPIODV_22, GPIODV_23,
};
static const unsigned int lcd_vs_pins[] = { GPIODV_24 };
static const unsigned int lcd_hs_pins[] = { GPIODV_25 };

/*tsout*/
static const unsigned int tsout_fail_pins[] = { GPIODV_12 };
static const unsigned int tsout_d_valid_pins[] = { GPIODV_13 };
static const unsigned int tsout_sop_pins[] = { GPIODV_14 };
static const unsigned int tsout_clk_pins[] = { GPIODV_15 };
static const unsigned int tsout_d0_pins[] = { GPIODV_16 };
static const unsigned int tsout_d1_7_pins[] = {
	GPIODV_17, GPIODV_18,
	GPIODV_19, GPIODV_20,
	GPIODV_21, GPIODV_22,
	GPIODV_23
};

/*uart_ao_a*/
static const unsigned int uart_tx_ao_a_card4_pins[] = { CARD_4 };
static const unsigned int uart_rx_ao_a_card4_pins[] = { CARD_4 };
static const unsigned int uart_rx_ao_a_card5_pins[] = { CARD_5 };
static const unsigned int uart_tx_ao_a_card5_pins[] = { CARD_5 };

/*uart_a*/
static const unsigned int uart_tx_a_pins[] = { GPIOX_12 };
static const unsigned int uart_rx_a_pins[] = { GPIOX_13 };
static const unsigned int uart_cts_a_pins[] = { GPIOX_14 };
static const unsigned int uart_rts_a_pins[] = { GPIOX_15 };

/*uart_b*/
static const unsigned int uart_tx_b_pins[] = { GPIODV_24 };
static const unsigned int uart_rx_b_pins[] = { GPIODV_25 };
static const unsigned int uart_cts_b_pins[] = { GPIODV_26 };
static const unsigned int uart_rts_b_pins[] = { GPIODV_27 };

/*uart_c*/
static const unsigned int uart_tx_c_pins[] = { GPIOX_8 };
static const unsigned int uart_rx_c_pins[] = { GPIOX_9 };
static const unsigned int uart_cts_c_pins[] = { GPIOX_10 };
static const unsigned int uart_rts_c_pins[] = { GPIOX_11 };

/*dmic*/
static const unsigned int dmic_in_dv24_pins[] = { GPIODV_24 };
static const unsigned int dmic_clk_dv25_pins[] = { GPIODV_25 };

static const unsigned int dmic_in_z8_pins[] = { GPIOZ_8 };
static const unsigned int dmic_clk_z9_pins[] = { GPIOZ_9};

/*tcon*/
static const unsigned int tcon_stv1_pins[] = { GPIODV_24 };
static const unsigned int tcon_sth1_pins[] = { GPIODV_25 };
static const unsigned int tcon_cph_pins[] = { GPIODV_26 };
static const unsigned int tcon_vcom_pins[] = { GPIODV_27 };
static const unsigned int tcon_oeh_pins[] = { GPIODV_27 };

/*i2c_a*/
static const unsigned int i2c_sda_a_pins[] = { GPIODV_24 };
static const unsigned int i2c_scl_a_pins[] = { GPIODV_25 };

/*i2c_b*/
static const unsigned int i2c_sda_b_pins[] = { GPIODV_26 };
static const unsigned int i2c_scl_b_pins[] = { GPIODV_27 };

/*i2c_c*/
static const unsigned int i2c_sda_c_dv28_pins[] = { GPIODV_28 };
static const unsigned int i2c_scl_c_dv29_pins[] = { GPIODV_29 };

static const unsigned int i2c_sda_c_dv18_pins[] = { GPIODV_18 };
static const unsigned int i2c_scl_c_dv19_pins[] = { GPIODV_19 };

/*i2c_d*/
static const unsigned int i2c_sda_d_pins[] = { GPIOX_10 };
static const unsigned int i2c_scl_d_pins[] = { GPIOX_11 };

/*eth*/
static const unsigned int eth_mdio_pins[] = { GPIOZ_0 };
static const unsigned int eth_mdc_pins[] = { GPIOZ_1 };
static const unsigned int eth_clk_rx_clk_pins[] = { GPIOZ_2 };
static const unsigned int eth_rx_dv_pins[] = { GPIOZ_3 };
static const unsigned int eth_rxd0_pins[] = { GPIOZ_4 };
static const unsigned int eth_rxd1_pins[] = { GPIOZ_5 };
static const unsigned int eth_rxd2_pins[] = { GPIOZ_6 };
static const unsigned int eth_rxd3_pins[] = { GPIOZ_7 };
static const unsigned int eth_rgmii_tx_clk_pins[] = { GPIOZ_8 };
static const unsigned int eth_tx_en_pins[] = { GPIOZ_9 };
static const unsigned int eth_txd0_pins[] = { GPIOZ_10 };
static const unsigned int eth_txd1_pins[] = { GPIOZ_11 };
static const unsigned int eth_txd2_pins[] = { GPIOZ_12 };
static const unsigned int eth_txd3_pins[] = { GPIOZ_13 };
static const unsigned int eth_link_led_pins[] = { GPIOZ_14 };
static const unsigned int eth_act_led_pins[] = { GPIOZ_15 };

/*dvp*/
static const unsigned int dvp_vs_pins[] = { GPIOZ_0 };
static const unsigned int dvp_hs_pins[] = { GPIOZ_1 };
static const unsigned int dvp_clk_pins[] = { GPIOZ_3 };
static const unsigned int dvp_d2_9_pins[] = {
	GPIOZ_4, GPIOZ_5,
	GPIOZ_6, GPIOZ_7,
	GPIOZ_8, GPIOZ_9,
	GPIOZ_10, GPIOZ_11
};

/*iso7816*/
static const unsigned int iso7816_clk_dv_pins[] = { GPIODV_22 };
static const unsigned int iso7816_data_dv_pins[] = { GPIODV_23 };

static struct meson_pmx_group meson_gxl_periphs_groups[] = {
	GPIO_GROUP(GPIOZ_0),
	GPIO_GROUP(GPIOZ_1),
	GPIO_GROUP(GPIOZ_2),
	GPIO_GROUP(GPIOZ_3),
	GPIO_GROUP(GPIOZ_4),
	GPIO_GROUP(GPIOZ_5),
	GPIO_GROUP(GPIOZ_6),
	GPIO_GROUP(GPIOZ_7),
	GPIO_GROUP(GPIOZ_8),
	GPIO_GROUP(GPIOZ_9),
	GPIO_GROUP(GPIOZ_10),
	GPIO_GROUP(GPIOZ_11),
	GPIO_GROUP(GPIOZ_12),
	GPIO_GROUP(GPIOZ_13),
	GPIO_GROUP(GPIOZ_14),
	GPIO_GROUP(GPIOZ_15),
	GPIO_GROUP(GPIOH_0),
	GPIO_GROUP(GPIOH_1),
	GPIO_GROUP(GPIOH_2),
	GPIO_GROUP(GPIOH_3),
	GPIO_GROUP(GPIOH_4),
	GPIO_GROUP(GPIOH_5),
	GPIO_GROUP(GPIOH_6),
	GPIO_GROUP(GPIOH_7),
	GPIO_GROUP(GPIOH_8),
	GPIO_GROUP(GPIOH_9),
	GPIO_GROUP(BOOT_0),
	GPIO_GROUP(BOOT_1),
	GPIO_GROUP(BOOT_2),
	GPIO_GROUP(BOOT_3),
	GPIO_GROUP(BOOT_4),
	GPIO_GROUP(BOOT_5),
	GPIO_GROUP(BOOT_6),
	GPIO_GROUP(BOOT_7),
	GPIO_GROUP(BOOT_8),
	GPIO_GROUP(BOOT_9),
	GPIO_GROUP(BOOT_10),
	GPIO_GROUP(BOOT_11),
	GPIO_GROUP(BOOT_12),
	GPIO_GROUP(BOOT_13),
	GPIO_GROUP(BOOT_14),
	GPIO_GROUP(BOOT_15),
	GPIO_GROUP(CARD_0),
	GPIO_GROUP(CARD_1),
	GPIO_GROUP(CARD_2),
	GPIO_GROUP(CARD_3),
	GPIO_GROUP(CARD_4),
	GPIO_GROUP(CARD_5),
	GPIO_GROUP(CARD_6),
	GPIO_GROUP(GPIODV_0),
	GPIO_GROUP(GPIODV_1),
	GPIO_GROUP(GPIODV_2),
	GPIO_GROUP(GPIODV_3),
	GPIO_GROUP(GPIODV_4),
	GPIO_GROUP(GPIODV_5),
	GPIO_GROUP(GPIODV_6),
	GPIO_GROUP(GPIODV_7),
	GPIO_GROUP(GPIODV_8),
	GPIO_GROUP(GPIODV_9),
	GPIO_GROUP(GPIODV_10),
	GPIO_GROUP(GPIODV_11),
	GPIO_GROUP(GPIODV_12),
	GPIO_GROUP(GPIODV_13),
	GPIO_GROUP(GPIODV_14),
	GPIO_GROUP(GPIODV_15),
	GPIO_GROUP(GPIODV_16),
	GPIO_GROUP(GPIODV_17),
	GPIO_GROUP(GPIODV_18),
	GPIO_GROUP(GPIODV_19),
	GPIO_GROUP(GPIODV_20),
	GPIO_GROUP(GPIODV_21),
	GPIO_GROUP(GPIODV_22),
	GPIO_GROUP(GPIODV_23),
	GPIO_GROUP(GPIODV_24),
	GPIO_GROUP(GPIODV_25),
	GPIO_GROUP(GPIODV_26),
	GPIO_GROUP(GPIODV_27),
	GPIO_GROUP(GPIODV_28),
	GPIO_GROUP(GPIODV_29),
	GPIO_GROUP(GPIOX_0),
	GPIO_GROUP(GPIOX_1),
	GPIO_GROUP(GPIOX_2),
	GPIO_GROUP(GPIOX_3),
	GPIO_GROUP(GPIOX_4),
	GPIO_GROUP(GPIOX_5),
	GPIO_GROUP(GPIOX_6),
	GPIO_GROUP(GPIOX_7),
	GPIO_GROUP(GPIOX_8),
	GPIO_GROUP(GPIOX_9),
	GPIO_GROUP(GPIOX_10),
	GPIO_GROUP(GPIOX_11),
	GPIO_GROUP(GPIOX_12),
	GPIO_GROUP(GPIOX_13),
	GPIO_GROUP(GPIOX_14),
	GPIO_GROUP(GPIOX_15),
	GPIO_GROUP(GPIOX_16),
	GPIO_GROUP(GPIOX_17),
	GPIO_GROUP(GPIOX_18),
	GPIO_GROUP(GPIOCLK_0),
	GPIO_GROUP(GPIOCLK_1),

	/* Bank X */
	GROUP(uart_tx_a,		5,	19),
	GROUP(uart_rx_a,		5,	18),
	GROUP(uart_cts_a,		5,	17),
	GROUP(uart_rts_a,		5,	16),
	GROUP(uart_tx_c,		5,	13),
	GROUP(uart_rx_c,		5,	12),
	GROUP(uart_cts_c,		5,	11),
	GROUP(uart_rts_c,		5,	10),
	GROUP(pcm_out_a,		5,	23),
	GROUP(pcm_in_a,			5,	22),
	GROUP(pcm_fs_a,			5,	21),
	GROUP(pcm_clk_a,		5,	20),
	GROUP(pwm_e,			5,	15),
	GROUP(sdio_d0,			5,	31),	/*x0*/
	GROUP(sdio_d1,			5,	30),	/*x1*/
	GROUP(sdio_d2,			5,	29),	/*x2*/
	GROUP(sdio_d3,			5,	28),	/*x3*/
	GROUP(sdio_clk,			5,	27),	/*x4*/
	GROUP(sdio_cmd,			5,	26),	/*x5*/
	GROUP(pwm_a,			5,	25),	/*x6*/
	GROUP(sdio_irq,			5,	24),	/*x7*/
	GROUP(pwm_f_x7,			5,	14),	/*x7*/
	GROUP(tsin_sop_a_x8,		6,	3),	/*x8*/
	GROUP(tsin_d_valid_a_x9,	6,	2),	/*x9*/
	GROUP(tsin_d0_a_x10,		6,	1),	/*x10*/
	GROUP(tsin_clk_a_x11,		6,	0),	/*x11*/
	GROUP(i2c_sda_d,		5,	5),	/*x10*/
	GROUP(i2c_scl_d,		5,	4),	/*x11*/
	GROUP(spi_sclk_1,		5,	3),	/*x8*/
	GROUP(spi_miso_1,		5,	2),	/*x9*/
	GROUP(spi_mosi_1,		5,	0),	/*x11*/


	/* Bank H */
	GROUP(hdmi_hpd,			6,	31),	/*H0*/
	GROUP(hdmi_sda,			6,	30),	/*H1*/
	GROUP(hdmi_scl,			6,	29),	/*H2*/
	GROUP(spdif_out,		6,	28),	/*H4*/
	GROUP(spdif_in_h4,		6,	27),	/*H4*/
	GROUP(i2s_am_clk,		6,	26),	/*H6*/
	GROUP(i2s_ao_clk_out,		6,	25),	/*H7*/
	GROUP(i2s_lr_clk_out,		6,	24),	/*H8*/
	GROUP(i2sout_ch01,		6,	23),	/*H9*/
	GROUP(tsin_clk_b_h6,		6,	20),	/*H6*/
	GROUP(tsin_d0_b_h7,		6,	19),	/*H7*/
	GROUP(tsin_sop_b_h8,		6,	18),	/*H8*/
	GROUP(tsin_d_valid_b_h9,	6,	17),	/*H9*/


	/* Bank Z */
	GROUP(eth_mdio,			4,	23),	/*z0*/
	GROUP(eth_mdc,			4,	22),	/*z1*/
	GROUP(eth_clk_rx_clk,		4,	21),	/*z2*/
	GROUP(eth_rx_dv,		4,	20),	/*z3*/
	GROUP(eth_rxd0,			4,	19),	/*z4*/
	GROUP(eth_rxd1,			4,	18),	/*z5*/
	GROUP(eth_rxd2,			4,	17),	/*z6*/
	GROUP(eth_rxd3,			4,	16),	/*z7*/
	GROUP(eth_rgmii_tx_clk,		4,	15),	/*z8*/
	GROUP(eth_tx_en,		4,	14),	/*z9*/
	GROUP(eth_txd0,			4,	13),	/*z10*/
	GROUP(eth_txd1,			4,	12),	/*z11*/
	GROUP(eth_txd2,			4,	11),	/*z12*/
	GROUP(eth_txd3,			4,	10),	/*z13*/
	GROUP(tsin_d_valid_b_z0,	3,	19),	/*z0*/
	GROUP(dvp_vs,			3,	14),	/*z0*/
	GROUP(tsin_sop_b_z1,		3,	18),	/*z1*/
	GROUP(dvp_hs,			3,	13),	/*z1*/
	GROUP(i2sin_ch23,		3,	29),	/*z2*/
	GROUP(tsin_d0_b_z2,		3,	17),	/*z2*/
	GROUP(i2sin_ch45,		3,	28),	/*z3*/
	GROUP(tsin_clk_b_z3,		3,	16),	/*z3*/
	GROUP(dvp_clk,			3,	12),	/*z3*/
	GROUP(i2sin_ch67,		3,	27),	/*z4*/
	GROUP(tsin_fail_b_z4,		3,	15),	/*z4*/
	GROUP(dvp_d2_9,			3,	11),	/*z4*/
	GROUP(i2sout_ch23_z5,		3,	26),	/*z5*/
	GROUP(i2sout_ch45_z6,		3,	25),	/*z6*/
	GROUP(i2sout_ch67_z7,		3,	24),	/*z7*/
	GROUP(spi_sclk_0,		4,	4),	/*z11*/
	GROUP(spi_miso_0,		4,	3),	/*z12*/
	GROUP(spi_mosi_0,		4,	2),	/*z13*/
	GROUP(spdif_in_z14,		3,	21),
	GROUP(eth_link_led,		4,	25),
	GROUP(eth_act_led,		4,	24),
	GROUP(pwm_c,			3,	20),	/*z15*/
	GROUP(dmic_in_z8,		3,	23),	/*z8*/
	GROUP(dmic_clk_z9,		3,	22),	/*z9*/

	/* Bank DV */
	GROUP(tsin_d0_a_dv0,		2,	4),	/*dv0*/
	GROUP(tsin_d1_7_a_dv1_7,	2,	3),	/*dv1-7*/
	GROUP(tsin_clk_a_dv8,		2,	2),	/*dv8*/
	GROUP(tsin_sop_a_dv9,		2,	1),	/*dv9*/
	GROUP(tsin_d_valid_a_dv10,	2,	0),	/*dv10*/
	GROUP(tsin_fail_a_dv11,		1,	31),	/*dv11*/
	GROUP(tsout_fail,		1,	30),	/*dv12*/
	GROUP(tsout_d_valid,		1,	29),	/*dv13*/
	GROUP(tsout_sop,		1,	28),	/*dv14*/
	GROUP(tsout_clk,		1,	27),	/*dv15*/
	GROUP(tsout_d0,			1,	26),	/*dv16*/
	GROUP(tsout_d1_7,		1,	25),	/*dv17-23*/
	GROUP(lcd_r0_1,			3,	10),	/*dv0-1*/
	GROUP(lcd_r2_7,			3,	9),	/*dv2-7*/
	GROUP(lcd_g0_1,			3,	8),	/*dv8-9*/
	GROUP(lcd_g2_7,			3,	7),	/*dv10-15*/
	GROUP(lcd_b0_1,			3,	6),	/*dv16-17*/
	GROUP(lcd_b2_7,			3,	5),	/*dv18-23*/
	GROUP(lcd_vs,			3,	4),	/*dv24*/
	GROUP(lcd_hs,			3,	3),	/*dv25*/
	GROUP(tcon_stv1,		1,	22),	/*dv24*/
	GROUP(tcon_sth1,		1,	21),	/*dv25*/
	GROUP(tcon_cph,			1,	20),	/*dv26*/
	GROUP(tcon_vcom,		1,	19),	/*dv27*/
	GROUP(tcon_oeh,			1,	18),	/*dv27*/
	GROUP(uart_tx_b,		2,	16),
	GROUP(uart_rx_b,		2,	15),
	GROUP(uart_cts_b,		2,	14),
	GROUP(uart_rts_b,		2,	13),
	GROUP(i2c_sda_a,		1,	15),	/*dv24*/
	GROUP(i2c_scl_a,		1,	14),	/*dv25*/
	GROUP(dmic_in_dv24,		2,	7),	/*dv24*/
	GROUP(dmic_clk_dv25,		2,	6),	/* dv25 */
	GROUP(i2c_sda_b,		1,	13),	/*dv26*/
	GROUP(i2c_scl_b,		1,	12),	/*dv27*/
	GROUP(i2c_sda_c_dv28,		1,	11),	/*dv28*/
	GROUP(i2c_scl_c_dv29,		1,	10),	/*dv29*/
	GROUP(i2c_sda_c_dv18,		1,	17),	/*dv18*/
	GROUP(i2c_scl_c_dv19,		1,	16),	/*dv19*/
	GROUP(pwm_b,			2,	11),	/*dv29*/
	GROUP(pwm_d,			2,	12),	/*dv28*/
	GROUP(iso7816_clk_dv,		2,	18), /*dv22*/
	GROUP(iso7816_data_dv,		2,	17), /*dv23*/

	/* Bank BOOT */
	GROUP(emmc_nand_d07,		7,	31),
	GROUP(emmc_clk,			7,	30),
	GROUP(emmc_cmd,			7,	29),
	GROUP(emmc_ds,			7,	28),
	GROUP(nor_d,			7,	13),
	GROUP(nor_q,			7,	12),
	GROUP(nor_c,			7,	11),
	GROUP(nor_cs,			7,	10),
	GROUP(nand_ce0,			7,	7),	/*boot8*/
	GROUP(nand_ce1,			7,	6),	/*boot9*/
	GROUP(nand_rb0,			7,	5),	/*boot10*/
	GROUP(nand_ale,			7,	4),	/*boot11*/
	GROUP(nand_cle,			7,	3),	/*boot12*/
	GROUP(nand_wen_clk,		7,	2),	/*boot13*/
	GROUP(nand_ren_wr,		7,	1),	/*boot14*/
	GROUP(nand_dqs,			7,	0),	/*boot15*/

	/* Bank CARD */
	GROUP(sdcard_d1,		6,	5),	/*card0*/
	GROUP(sdcard_d0,		6,	4),	/*card1*/
	GROUP(sdcard_clk,		6,	3),	/*card2*/
	GROUP(sdcard_cmd,		6,	2),	/*card3*/
	GROUP(sdcard_d3,		6,	1),	/*card4*/
	GROUP(sdcard_d2,		6,	0),	/*card5*/
	GROUP(uart_tx_ao_a_card4,	6,	9),	/*card4*/
	GROUP(uart_rx_ao_a_card5,	6,	8),	/*card5*/
	GROUP(uart_rx_ao_a_card4,	6,	11),	/*card4*/
	GROUP(uart_tx_ao_a_card5,	6,	10),	/*card5*/

#if 0
	GROUP(jtag_tdi_0,		0,	0xff),
	GROUP(jtag_tdo_0,		0,	0xff),
	GROUP(jtag_clk_0,		0,	0xff),
	GROUP(jtag_tms_0,		0,	0xff),
	GROUP(jtag_tdi_1,		0,	0xff),
	GROUP(jtag_tdo_1,		0,	0xff),
	GROUP(jtag_clk_1,		0,	0xff),
	GROUP(jtag_tms_1,		0,	0xff),
#endif

	/*Bank CLK*/
	GROUP(pwm_f_clk1,		8,	30),
};

static const struct pinctrl_pin_desc meson_gxl_aobus_pins[] = {
	MESON_PIN(GPIOAO_0),
	MESON_PIN(GPIOAO_1),
	MESON_PIN(GPIOAO_2),
	MESON_PIN(GPIOAO_3),
	MESON_PIN(GPIOAO_4),
	MESON_PIN(GPIOAO_5),
	MESON_PIN(GPIOAO_6),
	MESON_PIN(GPIOAO_7),
	MESON_PIN(GPIOAO_8),
	MESON_PIN(GPIOAO_9),
	MESON_PIN(GPIO_TEST_N),
};

/*uart_ao_a*/
static const unsigned int uart_tx_ao_a_0_pins[] = { GPIOAO_0 };
static const unsigned int uart_rx_ao_a_0_pins[] = { GPIOAO_1 };
static const unsigned int uart_cts_ao_a_pins[] = { GPIOAO_2 };
static const unsigned int uart_rts_ao_a_pins[] = { GPIOAO_3 };

/*uart_ao_b*/
static const unsigned int uart_tx_ao_b_0_pins[] = { GPIOAO_4 };
static const unsigned int uart_rx_ao_b_0_pins[] = { GPIOAO_5 };

static const unsigned int uart_tx_ao_b_1_pins[] = { GPIOAO_0 };
static const unsigned int uart_rx_ao_b_1_pins[] = { GPIOAO_1 };

static const unsigned int uart_cts_ao_b_pins[] = { GPIOAO_2 };
static const unsigned int uart_rts_ao_b_pins[] = { GPIOAO_3 };

/*pwm_ao_a*/
static const unsigned int pwm_ao_a_ao3_pins[] = { GPIOAO_3 };
static const unsigned int pwm_ao_a_ao8_pins[] = { GPIOAO_8 };

/*pwm_ao_b*/
static const unsigned int pwm_ao_b_ao6_pins[] = { GPIOAO_6 };
static const unsigned int pwm_ao_b_ao9_pins[] = { GPIOAO_9 };

/*i2c_ao*/
static const unsigned int i2c_sck_ao_pins[] = {GPIOAO_4 };
static const unsigned int i2c_sda_ao_pins[] = {GPIOAO_5 };

/*i2c_slave_ao*/
static const unsigned int i2c_slave_sck_ao_pins[] = {GPIOAO_4 };
static const unsigned int i2c_slave_sda_ao_pins[] = {GPIOAO_5 };

/*spdif_out_ao*/
static const unsigned int spdid_out_ao6_pins[] = { GPIOAO_6 };

/*remote input*/
static const unsigned int remote_input_pins[] = { GPIOAO_7 };

/*ir out*/
static const unsigned int ir_out_ao7_pins[] = { GPIOAO_7 };
static const unsigned int ir_out_ao9_pins[] = { GPIOAO_9 };

/*ao_cec*/
static const unsigned int ao_cec_pins[] = { GPIOAO_8 };

/*ee_cec*/
static const unsigned int ee_cec_pins[] = { GPIOAO_8 };

/*i2s_out_ao*/
static const unsigned int i2sout_ch23_ao8_pins[] = { GPIOAO_8 };
static const unsigned int i2sout_ch45_ao9_pins[] = { GPIOAO_9 };

/*spdif_out_ao*/
static const unsigned int spdid_out_ao9_pins[] = { GPIOAO_9 };

static struct meson_pmx_group meson_gxl_aobus_groups[] = {
	GPIO_GROUP(GPIOAO_0),
	GPIO_GROUP(GPIOAO_1),
	GPIO_GROUP(GPIOAO_2),
	GPIO_GROUP(GPIOAO_3),
	GPIO_GROUP(GPIOAO_4),
	GPIO_GROUP(GPIOAO_5),
	GPIO_GROUP(GPIOAO_6),
	GPIO_GROUP(GPIOAO_7),
	GPIO_GROUP(GPIOAO_8),
	GPIO_GROUP(GPIOAO_9),
	GPIO_GROUP(GPIO_TEST_N),

	/* bank AO */
	GROUP(uart_tx_ao_b_1,	0,	26),
	GROUP(uart_rx_ao_b_1,	0,	25),
	GROUP(uart_tx_ao_a_0,	0,	12),
	GROUP(uart_rx_ao_a_0,	0,	11),
	GROUP(uart_cts_ao_a,	0,	10),
	GROUP(uart_rts_ao_a,	0,	9),
	GROUP(uart_cts_ao_b,	0,	8),
	GROUP(uart_rts_ao_b,	0,	7),
	GROUP(i2c_sck_ao,	0,	6),
	GROUP(i2c_sda_ao,	0,	5),
	GROUP(i2c_slave_sck_ao,	0,	2),
	GROUP(i2c_slave_sda_ao,	0,	1),
	GROUP(pwm_ao_a_ao3,	0,	22),
	GROUP(uart_tx_ao_b_0,	0,	24),
	GROUP(uart_rx_ao_b_0,	0,	23),
	GROUP(pwm_ao_b_ao6,	0,	18),
	GROUP(remote_input,	0,	0),
	GROUP(ir_out_ao7,	0,	21),
	GROUP(ao_cec,		0,	15),
	GROUP(ee_cec,		0,	14),
	GROUP(i2sout_ch23_ao8,	2,	0),
	GROUP(pwm_ao_a_ao8,	0,	17),
	GROUP(ir_out_ao9,	0,	31),
	GROUP(spdid_out_ao6,	0,	16),
	GROUP(spdid_out_ao9,	0,	4),
	GROUP(i2sout_ch45_ao9,	2,	1),
	GROUP(pwm_ao_b_ao9,	0,	3),
};

static const char * const gpio_periphs_groups[] = {
	"GPIOZ_0", "GPIOZ_1", "GPIOZ_2", "GPIOZ_3", "GPIOZ_4",
	"GPIOZ_5", "GPIOZ_6", "GPIOZ_7", "GPIOZ_8", "GPIOZ_9",
	"GPIOZ_10", "GPIOZ_11", "GPIOZ_12", "GPIOZ_13", "GPIOZ_14",
	"GPIOZ_15",

	"GPIOH_0", "GPIOH_1", "GPIOH_2", "GPIOH_3", "GPIOH_4",
	"GPIOH_5", "GPIOH_6", "GPIOH_7", "GPIOH_8", "GPIOH_9",

	"BOOT_0", "BOOT_1", "BOOT_2", "BOOT_3", "BOOT_4",
	"BOOT_5", "BOOT_6", "BOOT_7", "BOOT_8", "BOOT_9",
	"BOOT_10", "BOOT_11", "BOOT_12", "BOOT_13", "BOOT_14", "BOOT_15",

	"CARD_0", "CARD_1", "CARD_2", "CARD_3", "CARD_4",
	"CARD_5", "CARD_6",

	"GPIODV_0", "GPIODV_1", "GPIODV_2", "GPIODV_3", "GPIODV_4",
	"GPIODV_5", "GPIODV_6", "GPIODV_7", "GPIODV_8", "GPIODV_9",
	"GPIODV_10", "GPIODV_11", "GPIODV_12", "GPIODV_13", "GPIODV_14",
	"GPIODV_15", "GPIODV_16", "GPIODV_17", "GPIODV_18", "GPIODV_19",
	"GPIODV_20", "GPIODV_21", "GPIODV_22", "GPIODV_23", "GPIODV_24",
	"GPIODV_25", "GPIODV_26", "GPIODV_27", "GPIODV_28", "GPIODV_29",

	"GPIOX_0", "GPIOX_1", "GPIOX_2", "GPIOX_3", "GPIOX_4",
	"GPIOX_5", "GPIOX_6", "GPIOX_7", "GPIOX_8", "GPIOX_9",
	"GPIOX_10", "GPIOX_11", "GPIOX_12", "GPIOX_13", "GPIOX_14",
	"GPIOX_15", "GPIOX_16", "GPIOX_17", "GPIOX_18",

	"GPIOCLK_0", "GPIOCLK_1",
};

static const char * const emmc_groups[] = {
	"emmc_nand_d07", "emmc_clk", "emmc_cmd", "emmc_ds",
};

static const char * const sdcard_groups[] = {
	"sdcard_d0", "sdcard_d1", "sdcard_d2", "sdcard_d3",
	"sdcard_cmd", "sdcard_clk",
};

static const char * const uart_a_groups[] = {
	"uart_tx_a", "uart_rx_a", "uart_cts_a", "uart_rts_a",
};

static const char * const uart_b_groups[] = {
	"uart_tx_b", "uart_rx_b", "uart_cts_b", "uart_rts_b",
};

static const char * const uart_c_groups[] = {
	"uart_tx_c", "uart_rx_c", "uart_cts_c", "uart_rts_c",
};

static const char * const eth_groups[] = {
	"eth_mdio", "eth_mdc", "eth_clk_rx_clk", "eth_rx_dv",
	"eth_rxd0", "eth_rxd1", "eth_rxd2", "eth_rxd3",
	"eth_rgmii_tx_clk", "eth_tx_en",
	"eth_txd0", "eth_txd1", "eth_txd2", "eth_txd3",
	"eth_link_led", "eth_act_led",
};

static const char * const jtag_groups[] = {
	"jtag_tdi_0", "jtag_tdo_0", "jtag_clk_0", "jtag_tms_0",
	"jtag_tdi_1", "jtag_tdo_1", "jtag_clk_1", "jtag_tms_1",
};

static const char * const pwm_a_groups[] = {
	"pwm_a",
};

static const char * const pwm_b_groups[] = {
	"pwm_b",
};

static const char * const pwm_c_groups[] = {
	"pwm_c",
};

static const char * const pwm_d_groups[] = {
	"pwm_d",
};

static const char * const pwm_e_groups[] = {
	"pwm_e",
};

static const char * const pwm_f_groups[] = {
	"pwm_f_clk1", "pwm_f_x7"
};

static const char * const pcm_a_groups[] = {
	"pcm_out_a", "pcm_in_a", "pcm_fs_a", "pcm_clk_a",
};

static const char * const spdif_out_groups[] = {
	"spdif_out",
};

static const char * const spdif_in_groups[] = {
	"spdif_in_h4", "spdif_in_z14",
};

static const char * const spi_groups[] = {
	"spi_sclk_0", "spi_miso_0", "spi_mosi_0",
	"spi_sclk_1", "spi_miso_1", "spi_mosi_1",
};

static const char * const nand_groups[] = {
	"emmc_nand_d07", "nand_ce0", "nand_ce1", "nand_rb0", "nand_ale",
	"nand_cle", "nand_wen_clk", "nand_ren_wr", "nand_dqs",
};

static const char * const hdmi_hpd_groups[] = {
	"hdmi_hpd",
};

static const char * const hdmi_ddc_groups[] = {
	"hdmi_sda", "hdmi_scl",
};

static const char * const i2c_a_groups[] = {
	"i2c_sda_a", "i2c_scl_a",
};

static const char * const i2c_b_groups[] = {
	"i2c_sda_b", "i2c_scl_b",
};

static const char * const i2c_c_groups[] = {
	"i2c_sda_c_dv28", "i2c_scl_c_dv29",
	"i2c_sda_c_dv18", "i2c_scl_c_dv19",
};

static const char * const i2c_d_groups[] = {
	"i2c_sda_d", "i2c_scl_d",
};

static const char * const i2s_groups[] = {
	"i2s_am_clk", "i2s_ao_clk_out", "i2s_lr_clk_out", "i2sout_ch01",
	"i2sout_ch23_z5", "i2sout_ch45_z6", "i2sout_ch67_z7",
	"i2sin_ch23", "i2sin_ch45", "i2sin_ch67",
};

static const char * const pdm_groups[] = {
	"pdm_in", "pdm_clk",
};

static const char * const sdio_groups[] = {
	"sdio_d0", "sdio_d1", "sdio_d2", "sdio_d3",
	"sdio_clk", "sdio_cmd", "sdio_irq",
};

static const char * const uart_ao_a_card_groups[] = {
	"uart_tx_ao_a_card4", "uart_rx_ao_a_card5",
	"uart_rx_ao_a_card4", "uart_tx_ao_a_card5",
};

static const char * const tsin_a_groups[] = {
	"tsin_sop_a_x8", "tsin_d_valid_a_x9", "tsin_d0_a_x10",
	"tsin_clk_a_x11",
	"tsin_sop_a_dv9", "tsin_d_valid_a_dv10", "tsin_d0_a_dv0",
	"tsin_clk_a_dv8",
	"tsin_fail_a_dv11", "tsin_d1_7_a_dv1_7",
};

static const char * const tsin_b_groups[] = {
	"tsin_clk_b_h6", "tsin_d0_b_h7", "tsin_sop_b_h8", "tsin_d_valid_b_h9",
	"tsin_clk_b_z3", "tsin_d0_b_z2", "tsin_sop_b_z1", "tsin_d_valid_b_z0",
	"tsin_fail_b_z4",
};

static const char * const tsout_a_groups[] = {
	"tsout_fail", "tsout_d_valid", "tsout_sop", "tsout_clk",
	"tsout_d0", "tsout_d1_7",
};

static const char * const lcd_ttl_groups[] = {
	"lcd_r0_1", "lcd_r2_7",
	"lcd_g0_1", "lcd_g2_7",
	"lcd_b0_1", "lcd_b2_7",
	"tcon_stv1", "tcon_sth1",
	"tcon_cph", "tcon_oeh",
};

static const char *const nor_groups[] = {
	"nor_d", "nor_q", "nor_c", "nor_cs",
};

static const char *const dvp_groups[] = {
	"dvp_vs", "dvp_hs", "dvp_clk", "dvp_d2_9"
};

static const char *const dmic_groups[] = {
	"dmic_in_dv24", "dmic_clk_dv25",
	"dmic_in_z8", "dmic_clk_z9",
};

static const char * const iso7816_groups[] = {
	"iso7816_clk_dv", "iso7816_data_dv",
};


static struct meson_pmx_func meson_gxl_periphs_functions[] = {
	FUNCTION(gpio_periphs),
	FUNCTION(emmc),
	FUNCTION(sdcard),
	FUNCTION(uart_a),
	FUNCTION(uart_b),
	FUNCTION(uart_c),
	FUNCTION(iso7816),
	FUNCTION(eth),
	FUNCTION(jtag),
	FUNCTION(pwm_a),
	FUNCTION(pwm_b),
	FUNCTION(pwm_c),
	FUNCTION(pwm_d),
	FUNCTION(pwm_e),
	FUNCTION(pwm_f),
	FUNCTION(pcm_a),
	FUNCTION(spdif_out),
	FUNCTION(spdif_in),
	FUNCTION(i2s),
	FUNCTION(pdm),
	FUNCTION(spi),
	FUNCTION(i2c_a),
	FUNCTION(i2c_b),
	FUNCTION(i2c_c),
	FUNCTION(i2c_d),
	FUNCTION(hdmi_ddc),
	FUNCTION(hdmi_hpd),
	FUNCTION(nand),
	FUNCTION(sdio),
	FUNCTION(uart_ao_a_card),
	FUNCTION(tsin_a),
	FUNCTION(tsin_b),
	FUNCTION(tsout_a),
	FUNCTION(lcd_ttl),
	FUNCTION(nor),
	FUNCTION(dvp),
	FUNCTION(dmic),
};

static const char * const gpio_aobus_groups[] = {
	"GPIOAO_0", "GPIOAO_1", "GPIOAO_2", "GPIOAO_3", "GPIOAO_4",
	"GPIOAO_5", "GPIOAO_6", "GPIOAO_7", "GPIOAO_8", "GPIOAO_9",
	"GPIO_TEST_N",
};

static const char * const uart_ao_groups[] = {
	"uart_tx_ao_a_0", "uart_rx_ao_a_0", "uart_cts_ao_a",
	"uart_rts_ao_a",
};

static const char * const uart_ao_b_groups[] = {
	"uart_tx_ao_b_0", "uart_rx_ao_b_0", "uart_cts_ao_b",
	"uart_rts_ao_b",	"uart_tx_ao_b_1", "uart_rx_ao_b_1",
};

static const char * const i2c_ao_groups[] = {
	"i2c_sck_ao", "i2c_sda_ao",
};

static const char * const i2c_slave_ao_groups[] = {
	"i2c_slave_sck_ao", "i2c_slave_sda_ao",
};

static const char * const remote_groups[] = {
	"remote_input",
};

static const char *const ir_out_ao_groups[] = {
	"ir_out_ao9", "ir_out_ao7",
};

static const char * const ee_cec_groups[] = {
	"ee_cec",
};

static const char * const ao_cec_groups[] = {
	"ao_cec",
};

static const char * const pwm_ao_a_groups[] = {
	"pwm_ao_a_ao3", "pwm_ao_a_ao8",
};

static const char * const pwm_ao_b_groups[] = {
	"pwm_ao_b_ao6", "pwm_ao_b_ao9",
};

static const char * const i2s_out_ao_groups[] = {
	"i2sout_ch23_ao8", "i2sout_ch45_ao9",
};

static const char * const spdif_out_ao_groups[] = {
	"spdid_out_ao6", "spdid_out_ao9",
};

static struct meson_pmx_func meson_gxl_aobus_functions[] = {
	FUNCTION(gpio_aobus),
	FUNCTION(uart_ao),
	FUNCTION(uart_ao_b),
	FUNCTION(i2c_ao),
	FUNCTION(i2c_slave_ao),
	FUNCTION(remote),
	FUNCTION(ir_out_ao),
	FUNCTION(ee_cec),
	FUNCTION(ao_cec),
	FUNCTION(pwm_ao_a),
	FUNCTION(pwm_ao_b),
	FUNCTION(i2s_out_ao),
	FUNCTION(spdif_out_ao),
};

/*To use Bank CLK as normal pin, and have to set the register
 *HHI_XTAL_DIVN_CNTL[0xc883c000 + (0x2f << 2)], as follows:
 *
 *bit[10] = 0
 *bit[11] = 0
 *bit[12] = 0
 *
 */
static struct meson_bank meson_gxl_periphs_banks[] = {
	/*   name    first   last  irq  pullen  pull    dir     out     in  */
	BANK("X", GPIOX_0, GPIOX_18, 89,
		4,  0,  4,  0,  12, 0,  13, 0,  14, 0),
	BANK("DV", GPIODV_0, GPIODV_29, 59,
		0,  0,  0,  0,  0,  0,  1,  0,  2,  0),
	BANK("H", GPIOH_0, GPIOH_9, 26,
		1, 20,  1, 20,  3, 20,  4, 20,  5, 20),
	BANK("Z", GPIOZ_0, GPIOZ_15, 10,
		3,  0,  3,  0,  9,  0,  10, 0, 11,  0),
	BANK("CARD", CARD_0, CARD_6, 52,
		2, 20,  2, 20,  6, 20,  7, 20,  8, 20),
	BANK("BOOT", BOOT_0, BOOT_15, 36,
		2,  0,  2,  0,  6,  0,  7,  0,  8,  0),
	BANK("CLK", GPIOCLK_0, GPIOCLK_1, 108,
		3, 28,  3, 28,  9, 28, 10, 28, 11, 28),
};

/*  TEST_N is special pin, only used as gpio output at present.
 *  the direction control bit from AO_SEC_REG0 bit[0], it
 *  configured to output when pinctrl driver is initialized.
 *  to make the api of gpiolib work well, the reserved bit(bit[14])
 *  seen as direction control bit.
 *
 * AO_GPIO_O_EN_N       0x09<<2=0x24     bit[31]     output level
 * AO_GPIO_I            0x0a<<2=0x28     bit[31]     input level
 * AO_SEC_REG0          0x50<<2=0x140    bit[0]      input enable
 * AO_RTI_PULL_UP_REG   0x0b<<2=0x2c     bit[14]     pull-up/down
 * AO_RTI_PULL_UP_REG   0x0b<<2=0x2c     bit[30]     pull-up enable
 */
static struct meson_bank meson_gxl_aobus_banks[] = {
	/* name    first   last  irq  pullen  pull    dir     out     in  */
	BANK("AO", GPIOAO_0, GPIOAO_9, 0,
		0,  16,  0,  0,  0,  0,  0, 16,  1,  0),
	BANK("TEST", GPIO_TEST_N, GPIO_TEST_N, -1,
		0, 30, 0, 14, 0, 14, 0, 31, 1, 31),
};

static int meson_gxl_periphs_init(struct meson_pinctrl *pc)
{
	void __iomem *reg;

	if (!request_mem_region(HHI_XTAL_DIVN_CNTL_GPIO, 4, "gpioclk")) {
		dev_err(pc->dev, "could not get region 0x%x - 0x%x\n",
			HHI_XTAL_DIVN_CNTL_GPIO,
			HHI_XTAL_DIVN_CNTL_GPIO + 4);
		return -EBUSY;
	}

	reg = ioremap(HHI_XTAL_DIVN_CNTL_GPIO, 4);
	if (!reg) {
		dev_err(pc->dev, "could not remap register memory\n");
		return -ENOMEM;
	}
	writel(readl(reg) & (~(7 << 10)), reg);

	iounmap(reg);
	release_mem_region(HHI_XTAL_DIVN_CNTL_GPIO, 4);

	return 0;
}

static int meson_gxl_aobus_init(struct meson_pinctrl *pc)
{
	struct arm_smccc_res res;
	/*set TEST_N to output*/
	arm_smccc_smc(CMD_TEST_N_DIR, TEST_N_OUTPUT, 0, 0, 0, 0, 0, 0, &res);

	return 0;
}

static struct meson_pinctrl_data meson_gxl_periphs_pinctrl_data = {
	.name		= "periphs-banks",
	.init		= meson_gxl_periphs_init,
	.pins		= meson_gxl_periphs_pins,
	.groups		= meson_gxl_periphs_groups,
	.funcs		= meson_gxl_periphs_functions,
	.banks		= meson_gxl_periphs_banks,
	.num_pins	= ARRAY_SIZE(meson_gxl_periphs_pins),
	.num_groups	= ARRAY_SIZE(meson_gxl_periphs_groups),
	.num_funcs	= ARRAY_SIZE(meson_gxl_periphs_functions),
	.num_banks	= ARRAY_SIZE(meson_gxl_periphs_banks),
	.pmx_ops	= &meson8_pmx_ops,
};

static struct meson_pinctrl_data meson_gxl_aobus_pinctrl_data = {
	.name		= "aobus-banks",
	.init		= meson_gxl_aobus_init,
	.pins		= meson_gxl_aobus_pins,
	.groups		= meson_gxl_aobus_groups,
	.funcs		= meson_gxl_aobus_functions,
	.banks		= meson_gxl_aobus_banks,
	.num_pins	= ARRAY_SIZE(meson_gxl_aobus_pins),
	.num_groups	= ARRAY_SIZE(meson_gxl_aobus_groups),
	.num_funcs	= ARRAY_SIZE(meson_gxl_aobus_functions),
	.num_banks	= ARRAY_SIZE(meson_gxl_aobus_banks),
	.pmx_ops	= &meson8_pmx_ops,
};

static const struct of_device_id meson_gxl_pinctrl_dt_match[] = {
	{
		.compatible = "amlogic,meson-gxl-periphs-pinctrl",
		.data = &meson_gxl_periphs_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-gxl-aobus-pinctrl",
		.data = &meson_gxl_aobus_pinctrl_data,
	},

};

static struct platform_driver meson_gxl_pinctrl_driver = {
	.probe		= meson_pinctrl_probe,
	.driver = {
		.name	= "meson-gxl-pinctrl",
		.of_match_table = meson_gxl_pinctrl_dt_match,
	},
};

static int __init gxl_pmx_init(void)
{
	return platform_driver_register(&meson_gxl_pinctrl_driver);
}

static void __exit gxl_pmx_exit(void)
{
	platform_driver_unregister(&meson_gxl_pinctrl_driver);
}

arch_initcall(gxl_pmx_init);
module_exit(gxl_pmx_exit);
MODULE_DESCRIPTION("gxl pin control driver");
MODULE_LICENSE("GPL v2");

