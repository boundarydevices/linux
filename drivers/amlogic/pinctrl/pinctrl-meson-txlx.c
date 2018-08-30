/*
 * drivers/amlogic/pinctrl/pinctrl-meson-txlx.c
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
#include <dt-bindings/gpio/meson-txlx-gpio.h>
#include "pinctrl-meson8-pmx.h"

static const struct pinctrl_pin_desc meson_txlx_periphs_pins[] = {
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
	MESON_PIN(GPIOZ_16),
	MESON_PIN(GPIOZ_17),
	MESON_PIN(GPIOZ_18),
	MESON_PIN(GPIOZ_19),
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
	MESON_PIN(GPIOH_10),
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
	MESON_PIN(GPIOC_0),
	MESON_PIN(GPIOC_1),
	MESON_PIN(GPIOC_2),
	MESON_PIN(GPIOC_3),
	MESON_PIN(GPIOC_4),
	MESON_PIN(GPIOC_5),
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
	MESON_PIN(GPIOW_0),
	MESON_PIN(GPIOW_1),
	MESON_PIN(GPIOW_2),
	MESON_PIN(GPIOW_3),
	MESON_PIN(GPIOW_4),
	MESON_PIN(GPIOW_5),
	MESON_PIN(GPIOW_6),
	MESON_PIN(GPIOW_7),
	MESON_PIN(GPIOW_8),
	MESON_PIN(GPIOW_9),
	MESON_PIN(GPIOW_10),
	MESON_PIN(GPIOW_11),
	MESON_PIN(GPIOW_12),
	MESON_PIN(GPIOW_13),
	MESON_PIN(GPIOW_14),
	MESON_PIN(GPIOW_15),
	MESON_PIN(GPIOY_0),
	MESON_PIN(GPIOY_1),
	MESON_PIN(GPIOY_2),
	MESON_PIN(GPIOY_3),
	MESON_PIN(GPIOY_4),
	MESON_PIN(GPIOY_5),
	MESON_PIN(GPIOY_6),
	MESON_PIN(GPIOY_7),
	MESON_PIN(GPIOY_8),
	MESON_PIN(GPIOY_9),
	MESON_PIN(GPIOY_10),
	MESON_PIN(GPIOY_11),
	MESON_PIN(GPIOY_12),
	MESON_PIN(GPIOY_13),
	MESON_PIN(GPIOY_14),
	MESON_PIN(GPIOY_15),
	MESON_PIN(GPIOY_16),
	MESON_PIN(GPIOY_17),
	MESON_PIN(GPIOY_18),
	MESON_PIN(GPIOY_19),
	MESON_PIN(GPIOY_20),
	MESON_PIN(GPIOY_21),
	MESON_PIN(GPIOY_22),
	MESON_PIN(GPIOY_23),
	MESON_PIN(GPIOY_24),
	MESON_PIN(GPIOY_25),
	MESON_PIN(GPIOY_26),
	MESON_PIN(GPIOY_27),
};

/* emmc & nand */
static const unsigned int emmc_nand_d07_pins[] = {
	BOOT_0, BOOT_1, BOOT_2,
	BOOT_3, BOOT_4, BOOT_5,
	BOOT_6, BOOT_7,
};

/* emmc */
static const unsigned int emmc_clk_pins[] = { BOOT_8 };
static const unsigned int emmc_cmd_pins[] = { BOOT_10 };
static const unsigned int emmc_ds_pins[] = { BOOT_11 };

/* nor */
static const unsigned int nor_d_pins[] = { BOOT_4 };
static const unsigned int nor_q_pins[] = { BOOT_5 };
static const unsigned int nor_c_pins[] = { BOOT_6 };
static const unsigned int nor_cs_pins[] = { BOOT_11 };

/* sacard */
static const unsigned int sdcard_d1_c_pins[] = { GPIOC_0 };
static const unsigned int sdcard_d0_c_pins[] = { GPIOC_1 };
static const unsigned int sdcard_clk_c_pins[] = { GPIOC_2 };
static const unsigned int sdcard_cmd_c_pins[] = { GPIOC_3 };
static const unsigned int sdcard_d3_c_pins[] = { GPIOC_4 };
static const unsigned int sdcard_d2_c_pins[] = { GPIOC_5 };

static const unsigned int sdcard_d1_z_pins[] = { GPIOZ_8 };
static const unsigned int sdcard_d0_z_pins[] = { GPIOZ_9 };
static const unsigned int sdcard_clk_z_pins[] = { GPIOZ_10 };
static const unsigned int sdcard_cmd_z_pins[] = { GPIOZ_11 };
static const unsigned int sdcard_d3_z_pins[] = { GPIOZ_12 };
static const unsigned int sdcard_d2_z_pins[] = { GPIOZ_13 };

/* i2c_a */
static const unsigned int i2c0_sda_pins[] = { GPIOZ_9 };
static const unsigned int i2c0_sck_pins[] = { GPIOZ_8 };

/* i2c_b */
static const unsigned int i2c1_sda_pins[] = { GPIODV_0 };
static const unsigned int i2c1_sck_pins[] = { GPIODV_1 };

/* i2c_c */
static const unsigned int i2c2_sda_pins[] = { GPIOH_3 };
static const unsigned int i2c2_sck_pins[] = { GPIOH_2 };

/* i2c_d */
static const unsigned int i2c3_sda_pins[] = { GPIOZ_2 };
static const unsigned int i2c3_sck_pins[] = { GPIOZ_3 };

/* i2c_slave_ao_ee */
static const unsigned int i2c_s_sda_ao_dv_pins[] = {
	GPIODV_0
};
static const unsigned int i2c_s_sck_ao_dv_pins[] = {
	GPIODV_1
};

static const unsigned int i2c_s_sda_ao_h_pins[] = {
	GPIOH_3
};
static const unsigned int i2c_s_sck_ao_h_pins[] = {
	GPIOH_2
};

/* pwm_a */
static const unsigned int pwm_a_pins[] = { GPIOZ_5 };

/* pwm_b */
static const unsigned int pwm_b_pins[] = { GPIOZ_6 };

/* pwm_c */
static const unsigned int pwm_c_z_pins[] = { GPIOZ_7 };
static const unsigned int pwm_c_h_pins[] = { GPIOH_6 };

/* pwm_d */
static const unsigned int pwm_d_dv_pins[] = { GPIODV_2 };
static const unsigned int pwm_d_z_pins[] = { GPIOZ_19 };

/* pwm_vs */
static const unsigned int pwm_vs_dv_pins[] = { GPIODV_3 };
static const unsigned int pwm_vs_z6_pins[] = { GPIOZ_6 };
static const unsigned int pwm_vs_z7_pins[] = { GPIOZ_7 };

/* pwm_e */
static const unsigned int pwm_e_dv_pins[] = { GPIODV_6 };
static const unsigned int pwm_e_h_pins[] = { GPIOH_4 };
static const unsigned int pwm_e_y_pins[] = { GPIOY_9 };
static const unsigned int pwm_e_z_pins[] = { GPIOZ_4 };

/* pwm_f */
static const unsigned int pwm_f_dv_pins[] = { GPIODV_10 };
static const unsigned int pwm_f_h_pins[] = { GPIOH_7 };

/* tsin_a */
static const unsigned int tsin_d0_a_dv_pins[] = { GPIODV_7 };
static const unsigned int tsin_clk_a_dv_pins[] = { GPIODV_8 };
static const unsigned int tsin_sop_a_dv_pins[] = { GPIODV_9 };
static const unsigned int tsin_valid_a_dv_pins[] = {
	GPIODV_10
};

static const unsigned int tsin_d0_a_y_pins[] = { GPIOY_1 };
static const unsigned int tsin_clk_a_y_pins[] = { GPIOY_0 };
static const unsigned int tsin_sop_a_y_pins[] = { GPIOY_2 };
static const unsigned int tsin_valid_a_y_pins[] = {
	GPIOY_3
};

static const unsigned int tsin_d0_a_z_pins[] = { GPIOZ_1 };
static const unsigned int tsin_clk_a_z_pins[] = { GPIOZ_0 };
static const unsigned int tsin_sop_a_z_pins[] = { GPIOZ_2 };
static const unsigned int tsin_valid_a_z_pins[] = {
	GPIOZ_3
};

static const unsigned int tsin_d1_7_a_pins[] = {
	GPIODV_0, GPIODV_1,
	GPIODV_2, GPIODV_3,
	GPIODV_4, GPIODV_5,
	GPIODV_6,
};

/* tsin_b */
static const unsigned int tsin_d0_b_pins[] = { GPIOY_11 };
static const unsigned int tsin_d1_b_pins[] = { GPIOY_12 };
static const unsigned int tsin_d2_b_pins[] = { GPIOY_13 };
static const unsigned int tsin_d3_b_pins[] = { GPIOY_14 };
static const unsigned int tsin_d4_b_pins[] = { GPIOY_15 };
static const unsigned int tsin_d5_b_pins[] = { GPIOY_17 };
static const unsigned int tsin_d6_b_pins[] = { GPIOY_19 };
static const unsigned int tsin_d7_b_pins[] = { GPIOY_20 };

static const unsigned int tsin_clk_b_pins[] = { GPIOY_18 };
static const unsigned int tsin_sop_b_pins[] = { GPIOY_10 };
static const unsigned int tsin_valid_b_pins[] = { GPIOY_16 };
static const unsigned int tsin_fail_b_pins[] = { GPIOY_22 };

/* atv */
static const unsigned int atv_if_agc_dv_pins[] = { GPIODV_2 };
static const unsigned int atv_if_agc_h_pins[] = { GPIOH_5 };

/* dtv */
static const unsigned int dtv_if_agc_dv2_pins[] = { GPIODV_2 };
static const unsigned int dtv_if_agc_dv3_pins[] = { GPIODV_3 };
static const unsigned int dtv_rf_agc_dv3_pins[] = { GPIODV_3 };

/* spi_a */
static const unsigned int spi_miso_a_pins[] = { GPIOZ_1 };
static const unsigned int spi_mosi_a_pins[] = { GPIOZ_0 };
static const unsigned int spi_clk_a_pins[] = { GPIOZ_2 };
static const unsigned int spi_ss0_a_pins[] = { GPIOZ_3 };
static const unsigned int spi_ss1_a_pins[] = { GPIOZ_4 };
static const unsigned int spi_ss2_a_pins[] = { GPIOZ_5 };

/* spi_b */
static const unsigned int spi_miso_b_dv_pins[] = { GPIODV_4 };
static const unsigned int spi_mosi_b_dv_pins[] = { GPIODV_3 };
static const unsigned int spi_clk_b_dv_pins[] = { GPIODV_5 };
static const unsigned int spi_ss0_b_dv_pins[] = { GPIODV_6 };
static const unsigned int spi_ss1_b_dv_pins[] = { GPIODV_7 };
static const unsigned int spi_ss2_b_dv_pins[] = { GPIODV_2 };

static const unsigned int spi_miso_b_c_pins[] = { GPIOC_1 };
static const unsigned int spi_mosi_b_c_pins[] = { GPIOC_3 };
static const unsigned int spi_clk_b_c_pins[] = { GPIOC_2 };
static const unsigned int spi_ss0_b_c_pins[] = { GPIOC_0 };
static const unsigned int spi_ss1_b_c_pins[] = { GPIOC_4 };
static const unsigned int spi_ss2_b_c_pins[] = { GPIOC_5 };

/* pcm_a */
static const unsigned int pcm_clk_a_dv_pins[] = { GPIODV_3 };
static const unsigned int pcm_fs_a_dv_pins[] = { GPIODV_4 };
static const unsigned int pcm_in_a_dv_pins[] = { GPIODV_5 };
static const unsigned int pcm_out_a_dv_pins[] = { GPIODV_6 };

static const unsigned int pcm_clk_a_z_pins[] = { GPIOZ_11 };
static const unsigned int pcm_fs_a_z_pins[] = { GPIOZ_12 };
static const unsigned int pcm_in_a_z_pins[] = { GPIOZ_13 };
static const unsigned int pcm_out_a_z_pins[] = { GPIOZ_14 };

/* uart_a */
static const unsigned int uart_tx_a_pins[] = { GPIODV_7 };
static const unsigned int uart_rx_a_pins[] = { GPIODV_8 };
static const unsigned int uart_cts_a_pins[] = { GPIODV_9 };
static const unsigned int uart_rts_a_pins[] = { GPIODV_10 };

/* uart_b */
static const unsigned int uart_tx_b_h_pins[] = { GPIOH_5 };
static const unsigned int uart_rx_b_h_pins[] = { GPIOH_6 };
static const unsigned int uart_tx_b_z_pins[] = { GPIOZ_15 };
static const unsigned int uart_rx_b_z_pins[] = { GPIOZ_16 };

static const unsigned int uart_cts_b_pins[] = { GPIOZ_18 };
static const unsigned int uart_rts_b_pins[] = { GPIOZ_19 };

/*uart_c*/
static const unsigned int uart_tx_c_pins[] = { GPIOZ_0 };
static const unsigned int uart_rx_c_pins[] = { GPIOZ_1 };
static const unsigned int uart_cts_c_pins[] = { GPIOZ_2 };
static const unsigned int uart_rts_c_pins[] = { GPIOZ_3 };

/* uart_ao_a_ee */
static const unsigned int uart_tx_ao_a_c4_pins[] = { GPIOC_4 };
static const unsigned int uart_rx_ao_a_c5_pins[] = { GPIOC_5 };

static const unsigned int uart_rx_ao_a_c4_pins[] = { GPIOC_4 };
static const unsigned int uart_tx_ao_a_c5_pins[] = { GPIOC_5 };

static const unsigned int uart_tx_ao_a_w2_pins[] = { GPIOW_2 };
static const unsigned int uart_rx_ao_a_w3_pins[] = { GPIOW_3 };

static const unsigned int uart_tx_ao_a_w6_pins[] = { GPIOW_6 };
static const unsigned int uart_rx_ao_a_w7_pins[] = { GPIOW_7 };

static const unsigned int uart_tx_ao_a_w10_pins[] = { GPIOW_10 };
static const unsigned int uart_rx_ao_a_w11_pins[] = { GPIOW_11 };

static const unsigned int uart_tx_ao_a_w14_pins[] = { GPIOW_14 };
static const unsigned int uart_rx_ao_a_w15_pins[] = { GPIOW_15 };

/* spdif_out */
static const unsigned int spdif_out_dv_pins[] = { GPIODV_6 };
static const unsigned int spdif_out_h_pins[] = { GPIOH_7 };
static const unsigned int spdif_out_z_pins[] = { GPIOZ_17 };

/*spdif_in*/
static const unsigned int spdif_in_h_pins[] = { GPIOH_0 };
static const unsigned int spdif_in_z16_pins[] = { GPIOZ_16 };
static const unsigned int spdif_in_z18_pins[] = { GPIOZ_18 };

/* hdmitx */
static const unsigned int hdmitx_hpd_pins[] = { GPIOH_1 };
static const unsigned int hdmitx_sck_pins[] = { GPIOH_2 };
static const unsigned int hdmitx_sda_pins[] = { GPIOH_3 };

/* hdmirx_a */
static const unsigned int hdmirx_hpd_a_pins[] = { GPIOW_4 };
static const unsigned int hdmirx_det_a_pins[] = { GPIOW_5 };
static const unsigned int hdmirx_sda_a_pins[] = { GPIOW_6 };
static const unsigned int hdmirx_sck_a_pins[] = { GPIOW_7 };

/* hdmirx_b */
static const unsigned int hdmirx_hpd_b_pins[] = { GPIOW_12 };
static const unsigned int hdmirx_det_b_pins[] = { GPIOW_13 };
static const unsigned int hdmirx_sda_b_pins[] = { GPIOW_14 };
static const unsigned int hdmirx_sck_b_pins[] = { GPIOW_15 };

/* hdmirx_c */
static const unsigned int hdmirx_hpd_c_pins[] = { GPIOW_8 };
static const unsigned int hdmirx_det_c_pins[] = { GPIOW_9 };
static const unsigned int hdmirx_sda_c_pins[] = { GPIOW_10 };
static const unsigned int hdmirx_sck_c_pins[] = { GPIOW_11 };

/* hdmirx_d */
static const unsigned int hdmirx_hpd_d_pins[] = { GPIOW_0 };
static const unsigned int hdmirx_det_d_pins[] = { GPIOW_1 };
static const unsigned int hdmirx_sda_d_pins[] = { GPIOW_2 };
static const unsigned int hdmirx_sck_d_pins[] = { GPIOW_3 };

/* audin */
static const unsigned int audin_lrclk_h7_pins[] = { GPIOH_7 };
static const unsigned int audin_sclk_h8_pins[] = { GPIOH_8 };
static const unsigned int audin_mclk_h_pins[] = { GPIOH_9 };
static const unsigned int audin_lrclkin_h7_pins[] = { GPIOH_7 };
static const unsigned int audin_sclkin_h8_pins[] = { GPIOH_8 };

static const unsigned int audin_lrclk_z9_pins[] = { GPIOZ_9 };
static const unsigned int audin_sclk_z8_pins[] = { GPIOZ_8 };
static const unsigned int audin_mclk_z_pins[] = { GPIOZ_4 };
static const unsigned int audin_lrclkin_z9_pins[] = { GPIOZ_9 };
static const unsigned int audin_sclkin_z8_pins[] = { GPIOZ_8 };

/* i2s */
static const unsigned int i2s_lrclk_h_pins[] = { GPIOH_7 };
static const unsigned int i2s_sclk_h_pins[] = { GPIOH_8 };
static const unsigned int i2s_mclk_h_pins[] = { GPIOH_9 };
static const unsigned int i2s_dout01_h6_pins[] = { GPIOH_6 };
static const unsigned int i2s_dout23_h5_pins[] = { GPIOH_5 };
static const unsigned int i2s_dout45_h4_pins[] = { GPIOH_4 };
static const unsigned int i2s_dout67_h0_pins[] = { GPIOH_0 };
static const unsigned int i2s_din01_h6_pins[] = { GPIOH_6 };
static const unsigned int i2s_din23_h5_pins[] = { GPIOH_5 };
static const unsigned int i2s_din45_h4_pins[] = { GPIOH_4 };
static const unsigned int i2s_din67_h0_pins[] = { GPIOH_0 };

static const unsigned int i2s_lrclk_z_pins[] = { GPIOZ_12 };
static const unsigned int i2s_sclk_z_pins[] = { GPIOZ_11 };
static const unsigned int i2s_mclk_z_pins[] = { GPIOZ_10 };
static const unsigned int i2s_dout01_z_pins[] = { GPIOZ_14 };
static const unsigned int i2s_dout23_z15_pins[] = { GPIOZ_15 };
static const unsigned int i2s_dout45_z_pins[] = { GPIOZ_16 };
static const unsigned int i2s_dout67_z19_pins[] = { GPIOZ_19 };
static const unsigned int i2s_din01_z_pins[] = { GPIOZ_13 };
static const unsigned int i2s_din23_z15_pins[] = { GPIOZ_15 };
static const unsigned int i2s_din45_z_pins[] = { GPIOZ_18 };
static const unsigned int i2s_din67_z19_pins[] = { GPIOZ_19 };

/* acodec_i2s */
static const unsigned int a_i2s_mclk_pins[] = { GPIOZ_10 };
static const unsigned int a_i2s_sclkin_pins[] = { GPIOZ_11 };
static const unsigned int a_i2s_lrclkin_pins[] = { GPIOZ_12 };
static const unsigned int a_i2s_din01_pins[] = { GPIOZ_13 };
static const unsigned int a_i2s_din23_pins[] = { GPIOZ_14 };

/* lcd */
static const unsigned int lcd_r0_1_pins[] = {
	GPIOY_0, GPIOY_1,
};

static const unsigned int lcd_r2_7_pins[] = {
	GPIOY_2, GPIOY_3,
	GPIOY_4, GPIOY_5,
	GPIOY_6, GPIOY_7,
};
static const unsigned int lcd_g0_1_pins[] = {
	GPIOY_8, GPIOY_9,
};
static const unsigned int lcd_g2_7_pins[] = {
	GPIOY_10, GPIOY_11,
	GPIOY_12, GPIOY_13,
	GPIOY_14, GPIOY_15,
};
static const unsigned int lcd_b0_1_pins[] = {
	GPIOY_16, GPIOY_17,
};
static const unsigned int lcd_b2_7_pins[] = {
	GPIOY_18, GPIOY_19,
	GPIOY_20, GPIOY_21,
	GPIOY_22, GPIOY_23,
};
static const unsigned int lcd_hs_pins[] = { GPIOY_26 };
static const unsigned int lcd_vs_pins[] = { GPIOY_27 };

/* tcon */
static const unsigned int tcon_oeh_pins[] = { GPIOY_25 };
static const unsigned int tcon_oev_pins[] = { GPIOY_0 };
static const unsigned int tcon_cpv_pins[] = { GPIOY_1 };
static const unsigned int tcon_cph_y24_pins[] = { GPIOY_24 };
static const unsigned int tcon_clko_y24_pins[] = { GPIOY_24 };
static const unsigned int tcon_vcom_y8_pins[] = { GPIOY_8 };
static const unsigned int tcon_vcom_y26_pins[] = { GPIOY_26 };

/* tsout */
static const unsigned int tsout_fail_pins[] = { GPIOY_4 };
static const unsigned int tsout_valid_pins[] = { GPIOY_23 };
static const unsigned int tsout_sop_pins[] = { GPIOY_24 };
static const unsigned int tsout_clk_pins[] = { GPIOY_21 };
static const unsigned int tsout_d0_pins[] = { GPIOY_25 };
static const unsigned int tsout_d1_pins[] = { GPIOY_26 };
static const unsigned int tsout_d2_pins[] = { GPIOY_27 };
static const unsigned int tsout_d3_pins[] = { GPIOY_5 };
static const unsigned int tsout_d4_pins[] = { GPIOY_6 };
static const unsigned int tsout_d5_pins[] = { GPIOY_7 };
static const unsigned int tsout_d6_pins[] = { GPIOY_8 };
static const unsigned int tsout_d7_pins[] = { GPIOY_9 };

/* eth */
static const unsigned int eth_mdio_pins[] = { GPIOY_4 };
static const unsigned int eth_mdc_pins[] = { GPIOY_5 };
static const unsigned int eth_rgmii_rx_clk_pins[] = { GPIOY_6 };
static const unsigned int eth_rx_dv_pins[] = { GPIOY_7 };
static const unsigned int eth_rxd0_pins[] = { GPIOY_8 };
static const unsigned int eth_rxd1_pins[] = { GPIOY_9 };
static const unsigned int eth_rxd2_pins[] = { GPIOY_10 };
static const unsigned int eth_rxd3_pins[] = { GPIOY_11 };
static const unsigned int eth_rgmii_tx_clk_pins[] = { GPIOY_12 };
static const unsigned int eth_tx_en_pins[] = { GPIOY_13 };
static const unsigned int eth_txd0_pins[] = { GPIOY_14 };
static const unsigned int eth_txd1_pins[] = { GPIOY_15 };
static const unsigned int eth_txd2_pins[] = { GPIOY_16 };
static const unsigned int eth_txd3_pins[] = { GPIOY_17 };
static const unsigned int eth_link_led_pins[] = { GPIOZ_18 };
static const unsigned int eth_act_led_pins[] = { GPIOZ_19 };

/* vbyone */
static const unsigned int vx1_lockn_pins[] = { GPIOH_0 };
static const unsigned int vx1_htpdn_pins[] = { GPIOH_1 };

static struct meson_pmx_group meson_txlx_periphs_groups[] = {
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
	GPIO_GROUP(GPIOZ_16),
	GPIO_GROUP(GPIOZ_17),
	GPIO_GROUP(GPIOZ_18),
	GPIO_GROUP(GPIOZ_19),
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
	GPIO_GROUP(GPIOH_10),
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
	GPIO_GROUP(GPIOC_0),
	GPIO_GROUP(GPIOC_1),
	GPIO_GROUP(GPIOC_2),
	GPIO_GROUP(GPIOC_3),
	GPIO_GROUP(GPIOC_4),
	GPIO_GROUP(GPIOC_5),
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
	GPIO_GROUP(GPIOW_0),
	GPIO_GROUP(GPIOW_1),
	GPIO_GROUP(GPIOW_2),
	GPIO_GROUP(GPIOW_3),
	GPIO_GROUP(GPIOW_4),
	GPIO_GROUP(GPIOW_5),
	GPIO_GROUP(GPIOW_6),
	GPIO_GROUP(GPIOW_7),
	GPIO_GROUP(GPIOW_8),
	GPIO_GROUP(GPIOW_9),
	GPIO_GROUP(GPIOW_10),
	GPIO_GROUP(GPIOW_11),
	GPIO_GROUP(GPIOW_12),
	GPIO_GROUP(GPIOW_13),
	GPIO_GROUP(GPIOW_14),
	GPIO_GROUP(GPIOW_15),
	GPIO_GROUP(GPIOY_0),
	GPIO_GROUP(GPIOY_1),
	GPIO_GROUP(GPIOY_2),
	GPIO_GROUP(GPIOY_3),
	GPIO_GROUP(GPIOY_4),
	GPIO_GROUP(GPIOY_5),
	GPIO_GROUP(GPIOY_6),
	GPIO_GROUP(GPIOY_7),
	GPIO_GROUP(GPIOY_8),
	GPIO_GROUP(GPIOY_9),
	GPIO_GROUP(GPIOY_10),
	GPIO_GROUP(GPIOY_11),
	GPIO_GROUP(GPIOY_12),
	GPIO_GROUP(GPIOY_13),
	GPIO_GROUP(GPIOY_14),
	GPIO_GROUP(GPIOY_15),
	GPIO_GROUP(GPIOY_16),
	GPIO_GROUP(GPIOY_17),
	GPIO_GROUP(GPIOY_18),
	GPIO_GROUP(GPIOY_19),
	GPIO_GROUP(GPIOY_20),
	GPIO_GROUP(GPIOY_21),
	GPIO_GROUP(GPIOY_22),
	GPIO_GROUP(GPIOY_23),
	GPIO_GROUP(GPIOY_24),
	GPIO_GROUP(GPIOY_25),
	GPIO_GROUP(GPIOY_26),
	GPIO_GROUP(GPIOY_27),

	/* Bank DV */
	GROUP(i2c1_sda,		2,	25),
	GROUP(i2c1_sck,		2,	24),
	GROUP(i2c_s_sda_ao_dv,	9,	17),
	GROUP(i2c_s_sck_ao_dv,	9,	16),
	GROUP(pwm_d_dv,		2,	20),
	GROUP(pwm_vs_dv,	2,	17),
	GROUP(pwm_e_dv,		9,	24),
	GROUP(pwm_f_dv,		9,	15),
	GROUP(tsin_d0_a_dv,	2,	30),
	GROUP(tsin_clk_a_dv,	2,	29),
	GROUP(tsin_sop_a_dv,	2,	28),
	GROUP(tsin_valid_a_dv,	2,	27),
	GROUP(tsin_d1_7_a,	2,	31),
	GROUP(atv_if_agc_dv,	2,	23),
	GROUP(dtv_if_agc_dv2,	2,	22),
	GROUP(dtv_if_agc_dv3,	9,	31),
	GROUP(dtv_rf_agc_dv3,	2,	21),
	GROUP(spi_miso_b_dv,	9,	29),
	GROUP(spi_mosi_b_dv,	9,	30),
	GROUP(spi_clk_b_dv,	9,	28),
	GROUP(spi_ss0_b_dv,	9,	27),
	GROUP(spi_ss1_b_dv,	9,	26),
	GROUP(spi_ss2_b_dv,	9,	25),
	GROUP(pcm_clk_a_dv,	9,	23),
	GROUP(pcm_fs_a_dv,	9,	22),
	GROUP(pcm_in_a_dv,	9,	21),
	GROUP(pcm_out_a_dv,	9,	20),
	GROUP(uart_tx_a,	2,	14),
	GROUP(uart_rx_a,	2,	13),
	GROUP(uart_cts_a,	2,	12),
	GROUP(uart_rts_a,	2,	11),
	GROUP(spdif_out_dv,	9,	14),

	/* Bank C */
	GROUP(sdcard_d1_c,	6,	5),
	GROUP(sdcard_d0_c,	6,	4),
	GROUP(sdcard_clk_c,	6,	3),
	GROUP(sdcard_cmd_c,	6,	2),
	GROUP(sdcard_d3_c,	6,	0),
	GROUP(sdcard_d2_c,	6,	1),
	GROUP(spi_miso_b_c,	6,	20),
	GROUP(spi_mosi_b_c,	6,	18),
	GROUP(spi_clk_b_c,	6,	19),
	GROUP(spi_ss0_b_c,	6,	21),
	GROUP(spi_ss1_b_c,	6,	17),
	GROUP(spi_ss2_b_c,	6,	16),
	GROUP(uart_tx_ao_a_c4,	6,	9),
	GROUP(uart_rx_ao_a_c5,	6,	8),
	GROUP(uart_rx_ao_a_c4,	6,	11),
	GROUP(uart_tx_ao_a_c5,	6,	10),

	/* Bank H */
	GROUP(i2c_s_sda_ao_h,	0,	1),
	GROUP(i2c_s_sck_ao_h,	0,	2),
	GROUP(pwm_e_h,		0,	26),
	GROUP(pwm_f_h,		10,	1),
	GROUP(atv_if_agc_h,	10,	3),
	GROUP(spdif_out_h,	10,	0),
	GROUP(spdif_in_h,	0,	20),
	GROUP(i2c2_sck,		0,	29),
	GROUP(i2c2_sda,		0,	28),
	GROUP(hdmitx_hpd,	0,	23),
	GROUP(hdmitx_sck,	0,	22),
	GROUP(hdmitx_sda,	0,	21),
	GROUP(uart_tx_b_h,	0,	25),
	GROUP(uart_rx_b_h,	0,	24),
	GROUP(audin_lrclk_h7,	0,	5),
	GROUP(audin_sclk_h8,	0,	4),
	GROUP(audin_mclk_h,	0,	3),
	GROUP(audin_lrclkin_h7,	0,	7),
	GROUP(audin_sclkin_h8,	0,	6),
	GROUP(i2s_lrclk_h,	0,	14),
	GROUP(i2s_sclk_h,	0,	13),
	GROUP(i2s_mclk_h,	0,	12),
	GROUP(i2s_dout01_h6,	0,	15),
	GROUP(i2s_dout23_h5,	0,	16),
	GROUP(i2s_dout45_h4,	0,	17),
	GROUP(i2s_dout67_h0,	0,	18),
	GROUP(i2s_din01_h6,	0,	8),
	GROUP(i2s_din23_h5,	0,	9),
	GROUP(i2s_din45_h4,	0,	10),
	GROUP(i2s_din67_h0,	0,	11),
	GROUP(pwm_c_h,		10,	2),
	GROUP(vx1_lockn,	0,	31),
	GROUP(vx1_htpdn,	0,	30),

	/* Bank BOOT */
	GROUP(emmc_nand_d07,	7,	31),
	GROUP(emmc_clk,		7,	30),
	GROUP(emmc_cmd,		7,	29),
	GROUP(emmc_ds,		7,	28),
	GROUP(nor_d,		7,	13),
	GROUP(nor_q,		7,	12),
	GROUP(nor_c,		7,	11),
	GROUP(nor_cs,		7,	10),

	/* Bank Y */
	GROUP(pwm_e_y,		8,	13),
	GROUP(tsin_d0_a_y,	1,	17),
	GROUP(tsin_clk_a_y,	1,	18),
	GROUP(tsin_sop_a_y,	1,	16),
	GROUP(tsin_valid_a_y,	1,	15),
	GROUP(tsin_d0_b,	1,	26),
	GROUP(tsin_d1_b,	1,	25),
	GROUP(tsin_d2_b,	1,	24),
	GROUP(tsin_d3_b,	1,	23),
	GROUP(tsin_d4_b,	1,	22),
	GROUP(tsin_d5_b,	1,	21),
	GROUP(tsin_d6_b,	1,	20),
	GROUP(tsin_d7_b,	1,	19),
	GROUP(tsin_clk_b,	1,	28),
	GROUP(tsin_sop_b,	1,	20),
	GROUP(tsin_valid_b,	1,	29),
	GROUP(tsin_fail_b,	1,	27),
	GROUP(lcd_r0_1,		8,	29),
	GROUP(lcd_r2_7,		8,	28),
	GROUP(lcd_g0_1,		8,	27),
	GROUP(lcd_g2_7,		8,	26),
	GROUP(lcd_b0_1,		8,	25),
	GROUP(lcd_b2_7,		8,	24),
	GROUP(lcd_hs,		8,	30),
	GROUP(lcd_vs,		8,	31),
	GROUP(tcon_oeh,		8,	22),
	GROUP(tcon_oev,		8,	21),
	GROUP(tcon_cpv,		8,	20),
	GROUP(tcon_cph_y24,	8,	23),
	GROUP(tcon_clko_y24,	8,	18),
	GROUP(tcon_vcom_y8,	8,	19),
	GROUP(tcon_vcom_y26,	8,	14),
	GROUP(tsout_fail,	8,	12),
	GROUP(tsout_valid,	8,	11),
	GROUP(tsout_sop,	8,	10),
	GROUP(tsout_clk,	8,	9),
	GROUP(tsout_d0,		8,	8),
	GROUP(tsout_d1,		8,	7),
	GROUP(tsout_d2,		8,	6),
	GROUP(tsout_d3,		8,	5),
	GROUP(tsout_d4,		8,	4),
	GROUP(tsout_d5,		8,	3),
	GROUP(tsout_d6,		8,	2),
	GROUP(tsout_d7,		8,	1),
	GROUP(eth_mdio,		1,	8),
	GROUP(eth_mdc,		1,	7),
	GROUP(eth_rgmii_rx_clk,	1,	14),
	GROUP(eth_rx_dv,	1,	13),
	GROUP(eth_rxd0,		1,	12),
	GROUP(eth_rxd1,		1,	11),
	GROUP(eth_rxd2,		1,	10),
	GROUP(eth_rxd3,		1,	9),
	GROUP(eth_rgmii_tx_clk,	1,	6),
	GROUP(eth_tx_en,	1,	5),
	GROUP(eth_txd0,		1,	4),
	GROUP(eth_txd1,		1,	3),
	GROUP(eth_txd2,		1,	2),
	GROUP(eth_txd3,		1,	1),

	/* Bank Z */
	GROUP(sdcard_d1_z,	10,	22),
	GROUP(sdcard_d0_z,	10,	21),
	GROUP(sdcard_clk_z,	10,	20),
	GROUP(sdcard_cmd_z,	10,	19),
	GROUP(sdcard_d3_z,	10,	18),
	GROUP(sdcard_d2_z,	10,	17),
	GROUP(pwm_d_z,		3,	27),
	GROUP(pwm_vs_z6,	4,	15),
	GROUP(pwm_vs_z7,	4,	13),
	GROUP(pwm_e_z,		4,	19),
	GROUP(tsin_d0_a_z,	3,	15),
	GROUP(tsin_clk_a_z,	3,	16),
	GROUP(tsin_sop_a_z,	3,	14),
	GROUP(tsin_valid_a_z,	3,	13),
	GROUP(pcm_clk_a_z,	3,	6),
	GROUP(pcm_fs_a_z,	3,	5),
	GROUP(pcm_in_a_z,	3,	4),
	GROUP(pcm_out_a_z,	3,	3),
	GROUP(spdif_out_z,	3,	30),
	GROUP(spdif_in_z16,	3,	31),
	GROUP(spdif_in_z18,	10,	23),
	GROUP(uart_tx_b_z,	3,	25),
	GROUP(uart_rx_b_z,	3,	24),
	GROUP(uart_cts_b,	3,	23),
	GROUP(uart_rts_b,	3,	22),
	GROUP(audin_lrclk_z9,	10,	26),
	GROUP(audin_sclk_z8,	10,	27),
	GROUP(audin_mclk_z,	10,	28),
	GROUP(audin_lrclkin_z9,	10,	24),
	GROUP(audin_sclkin_z8,	10,	25),
	GROUP(i2s_lrclk_z,	4,	8),
	GROUP(i2s_sclk_z,	4,	9),
	GROUP(i2s_mclk_z,	4,	10),
	GROUP(i2s_dout01_z,	4,	7),
	GROUP(i2s_dout23_z15,	4,	6),
	GROUP(i2s_dout45_z,	4,	5),
	GROUP(i2s_dout67_z19,	4,	4),
	GROUP(i2s_din01_z,	4,	1),
	GROUP(i2s_din23_z15,	3,	0),
	GROUP(i2s_din45_z,	3,	2),
	GROUP(i2s_din67_z19,	3,	1),
	GROUP(eth_link_led,	3,	29),
	GROUP(eth_act_led,	3,	28),
	GROUP(spi_miso_a,	4,	30),
	GROUP(spi_mosi_a,	4,	31),
	GROUP(spi_clk_a,	4,	29),
	GROUP(spi_ss0_a,	4,	28),
	GROUP(spi_ss1_a,	4,	27),
	GROUP(spi_ss2_a,	4,	26),
	GROUP(i2c0_sda,		4,	11),
	GROUP(i2c0_sck,		4,	12),
	GROUP(uart_tx_c,	4,	25),
	GROUP(uart_rx_c,	4,	24),
	GROUP(uart_cts_c,	4,	23),
	GROUP(uart_rts_c,	4,	22),
	GROUP(pwm_a,		4,	17),
	GROUP(pwm_b,		4,	16),
	GROUP(pwm_c_z,		4,	14),
	GROUP(i2c3_sda,		4,	21),
	GROUP(i2c3_sck,		4,	20),
	GROUP(a_i2s_mclk,	3,	20),
	GROUP(a_i2s_sclkin,	3,	19),
	GROUP(a_i2s_lrclkin,	3,	18),
	GROUP(a_i2s_din01,	3,	17),
	GROUP(a_i2s_din23,	4,	0),

	/* Bank W */
	GROUP(uart_tx_ao_a_w2,	5,	15),
	GROUP(uart_rx_ao_a_w3,	5,	14),
	GROUP(uart_tx_ao_a_w6,	5,	13),
	GROUP(uart_rx_ao_a_w7,	5,	12),
	GROUP(uart_tx_ao_a_w10,	5,	11),
	GROUP(uart_rx_ao_a_w11,	5,	10),
	GROUP(uart_tx_ao_a_w14,	5,	9),
	GROUP(uart_rx_ao_a_w15,	5,	8),
	GROUP(hdmirx_hpd_a,	5,	27),
	GROUP(hdmirx_det_a,	5,	26),
	GROUP(hdmirx_sda_a,	5,	25),
	GROUP(hdmirx_sck_a,	5,	24),
	GROUP(hdmirx_hpd_b,	5,	19),
	GROUP(hdmirx_det_b,	5,	18),
	GROUP(hdmirx_sda_b,	5,	17),
	GROUP(hdmirx_sck_b,	5,	16),
	GROUP(hdmirx_hpd_c,	5,	23),
	GROUP(hdmirx_det_c,	5,	22),
	GROUP(hdmirx_sda_c,	5,	21),
	GROUP(hdmirx_sck_c,	5,	20),
	GROUP(hdmirx_hpd_d,	5,	31),
	GROUP(hdmirx_det_d,	5,	30),
	GROUP(hdmirx_sda_d,	5,	29),
	GROUP(hdmirx_sck_d,	5,	28),
};

static const struct pinctrl_pin_desc meson_txlx_aobus_pins[] = {
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
	MESON_PIN(GPIOAO_10),
	MESON_PIN(GPIOAO_11),
	MESON_PIN(GPIOAO_12),
	MESON_PIN(GPIOAO_13),
	MESON_PIN(GPIO_TEST_N),
};

/* uart_ao_a */
static const unsigned int uart_tx_ao_a_pins[] = { GPIOAO_0 };
static const unsigned int uart_rx_ao_a_pins[] = { GPIOAO_1 };
static const unsigned int uart_cts_ao_a_pins[] = { GPIOAO_2 };
static const unsigned int uart_rts_ao_a_pins[] = { GPIOAO_3 };

/* uart_ao_b */
static const unsigned int uart_tx_ao_b_ao0_pins[] = { GPIOAO_0 };
static const unsigned int uart_rx_ao_b_ao1_pins[] = { GPIOAO_1 };
static const unsigned int uart_cts_ao_b_pins[] = { GPIOAO_2 };
static const unsigned int uart_rts_ao_b_pins[] = { GPIOAO_3 };

static const unsigned int uart_tx_ao_b_ao4_pins[] = { GPIOAO_4 };
static const unsigned int uart_rx_ao_b_ao5_pins[] = { GPIOAO_5 };

static const unsigned int uart_tx_ao_b_ao10_pins[] = { GPIOAO_10 };
static const unsigned int uart_rx_ao_b_ao11_pins[] = { GPIOAO_11 };

/* ir_out */
static const unsigned int remote_out_ao2_pins[] = { GPIOAO_2 };
static const unsigned int remote_out_ao6_pins[] = { GPIOAO_6 };

/* ir_in */
static const unsigned int remote_in_pins[] = { GPIOAO_6 };

/* pwm_ao_a */
static const unsigned int pwm_ao_a_ao3_pins[] = { GPIOAO_3 };
static const unsigned int pwm_ao_a_ao7_pins[] = { GPIOAO_7 };

/* pwm_ao_b */
static const unsigned int pwm_ao_b_ao8_pins[] = { GPIOAO_8 };
static const unsigned int pwm_ao_b_ao9_pins[] = { GPIOAO_9 };

/* pwm_ao_c */
static const unsigned int pwm_ao_c_ao10_pins[] = { GPIOAO_10 };
static const unsigned int pwm_ao_c_ao12_pins[] = { GPIOAO_12 };

/* pwm_ao_d */
static const unsigned int pwm_ao_d_ao2_pins[] = { GPIOAO_2 };
static const unsigned int pwm_ao_d_ao11_pins[] = { GPIOAO_11 };
static const unsigned int pwm_ao_d_ao13_pins[] = { GPIOAO_13 };

/* i2c_ao */
static const unsigned int i2c_sck_ao4_pins[] = {GPIOAO_4 };
static const unsigned int i2c_sda_ao5_pins[] = {GPIOAO_5 };

static const unsigned int i2c_sck_ao10_pins[] = {GPIOAO_10 };
static const unsigned int i2c_sda_ao11_pins[] = {GPIOAO_11 };

/* i2c_slave_ao */
static const unsigned int i2c_slave_sck_ao4_pins[] = {GPIOAO_4 };
static const unsigned int i2c_slave_sda_ao5_pins[] = {GPIOAO_5 };

static const unsigned int i2c_slave_sck_ao10_pins[] = {GPIOAO_10 };
static const unsigned int i2c_slave_sda_ao11_pins[] = {GPIOAO_11 };

/* ao_cec */
static const unsigned int ao_cec_ao7_pins[] = { GPIOAO_7 };
static const unsigned int ao_cec_ao8_pins[] = { GPIOAO_8 };

/* ao_cec_b */
static const unsigned int ao_cec_b_ao7_pins[] = { GPIOAO_7 };
static const unsigned int ao_cec_b_ao8_pins[] = { GPIOAO_8 };

/*
 * Note: The offset of AO_RTI_PIN_MUX_REG register is (0x05 << 2), and
 * next offset address (0x06 << 2) is corresponding to AO_RTI_PIN_MUX_REG2
 * register.
 */
static struct meson_pmx_group meson_txlx_aobus_groups[] = {
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
	GPIO_GROUP(GPIOAO_10),
	GPIO_GROUP(GPIOAO_11),
	GPIO_GROUP(GPIOAO_12),
	GPIO_GROUP(GPIOAO_13),
	GPIO_GROUP(GPIO_TEST_N),

	/* bank AO */
	GROUP(uart_tx_ao_a,		0,	12),
	GROUP(uart_rx_ao_a,		0,	11),
	GROUP(uart_cts_ao_a,		0,	10),
	GROUP(uart_rts_ao_a,		0,	9),
	GROUP(uart_tx_ao_b_ao0,		0,	26),
	GROUP(uart_rx_ao_b_ao1,		0,	25),
	GROUP(uart_cts_ao_b,		0,	8),
	GROUP(uart_rts_ao_b,		0,	7),
	GROUP(uart_tx_ao_b_ao4,		0,	24),
	GROUP(uart_rx_ao_b_ao5,		0,	23),
	GROUP(uart_tx_ao_b_ao10,	1,	9),
	GROUP(uart_rx_ao_b_ao11,	1,	8),
	GROUP(remote_out_ao2,		0,	28),
	GROUP(remote_out_ao6,		0,	21),
	GROUP(remote_in,		0,	0),
	GROUP(pwm_ao_a_ao3,		0,	22),
	GROUP(pwm_ao_a_ao7,		0,	17),
	GROUP(pwm_ao_b_ao8,		0,	27),
	GROUP(pwm_ao_b_ao9,		0,	3),
	GROUP(pwm_ao_c_ao10,		1,	5),
	GROUP(pwm_ao_c_ao12,		1,	2),
	GROUP(pwm_ao_d_ao2,		1,	10),
	GROUP(pwm_ao_d_ao11,		1,	18),
	GROUP(pwm_ao_d_ao13,		1,	1),
	GROUP(i2c_sck_ao4,		0,	6),
	GROUP(i2c_sda_ao5,		0,	5),
	GROUP(i2c_sck_ao10,		1,	7),
	GROUP(i2c_sda_ao11,		1,	4),
	GROUP(i2c_slave_sck_ao4,	0,	2),
	GROUP(i2c_slave_sda_ao5,	0,	1),
	GROUP(i2c_slave_sck_ao10,	1,	6),
	GROUP(i2c_slave_sda_ao11,	1,	3),
	GROUP(ao_cec_ao7,		0,	15),
	GROUP(ao_cec_ao8,		1,	16),
	GROUP(ao_cec_b_ao7,		1,	13),
	GROUP(ao_cec_b_ao8,		1,	17),
};

static const char * const gpio_periphs_groups[] = {
	"GPIOZ_0", "GPIOZ_1", "GPIOZ_2", "GPIOZ_3", "GPIOZ_4",
	"GPIOZ_5", "GPIOZ_6", "GPIOZ_7", "GPIOZ_8", "GPIOZ_9",
	"GPIOZ_10", "GPIOZ_11", "GPIOZ_12", "GPIOZ_13", "GPIOZ_14",
	"GPIOZ_15", "GPIOZ_16", "GPIOZ_17", "GPIOZ_18", "GPIOZ_19",

	"GPIOH_0", "GPIOH_1", "GPIOH_2", "GPIOH_3", "GPIOH_4",
	"GPIOH_5", "GPIOH_6", "GPIOH_7", "GPIOH_8", "GPIOH_9",
	"GPIOH_10",

	"BOOT_0", "BOOT_1", "BOOT_2", "BOOT_3", "BOOT_4",
	"BOOT_5", "BOOT_6", "BOOT_7", "BOOT_8", "BOOT_9",
	"BOOT_10", "BOOT_11",

	"GPIOC_0", "GPIOC_1", "GPIOC_2", "GPIOC_3", "GPIOC_4",
	"GPIOC_5",

	"GPIODV_0", "GPIODV_1", "GPIODV_2", "GPIODV_3", "GPIODV_4",
	"GPIODV_5", "GPIODV_6", "GPIODV_7", "GPIODV_8", "GPIODV_9",
	"GPIODV_10",

	"GPIOW_0", "GPIOW_1", "GPIOW_2", "GPIOW_3", "GPIOW_4",
	"GPIOW_5", "GPIOW_6", "GPIOW_7", "GPIOW_8", "GPIOW_9",
	"GPIOW_10", "GPIOW_11", "GPIOW_12", "GPIOW_13", "GPIOW_14",
	"GPIOW_15",

	"GPIOY_0", "GPIOY_1", "GPIOY_2", "GPIOY_3", "GPIOY_4",
	"GPIOY_5", "GPIOY_6", "GPIOY_7", "GPIOY_8", "GPIOY_9",
	"GPIOY_10", "GPIOY_11", "GPIOY_12", "GPIOY_13", "GPIOY_14",
	"GPIOY_15", "GPIOY_16", "GPIOY_17", "GPIOY_18", "GPIOY_19",
	"GPIOY_20", "GPIOY_21", "GPIOY_22", "GPIOY_23", "GPIOY_24",
	"GPIOY_25", "GPIOY_26", "GPIOY_27",

};

static const char * const emmc_groups[] = {
	"emmc_nand_d07", "emmc_clk", "emmc_cmd", "emmc_ds",
};

static const char * const sdcard_groups[] = {
	"sdcard_d0_c", "sdcard_d1_c", "sdcard_d2_c", "sdcard_d3_c",
	"sdcard_cmd_c", "sdcard_clk_c",
	"sdcard_d0_z", "sdcard_d1_z", "sdcard_d2_z", "sdcard_d3_z",
	"sdcard_cmd_z", "sdcard_clk_z",
};

static const char *const nor_groups[] = {
	"nor_d", "nor_q", "nor_c", "nor_cs",
};

static const char * const i2c0_groups[] = {
	"i2c0_sda", "i2c0_sck",
};

static const char * const i2c1_groups[] = {
	"i2c1_sda", "i2c1_sck",
};

static const char * const i2c2_groups[] = {
	"i2c2_sda", "i2c2_sck",
};

static const char * const i2c3_groups[] = {
	"i2c3_sda", "i2c3_sck",
};

static const char * const i2c_slave_ao_ee_groups[] = {
	"i2c_s_sda_ao_dv", "i2c_s_sck_ao_dv",
	"i2c_s_sda_ao_h", "i2c_s_sck_ao_h",
};

static const char * const pwm_a_groups[] = {
	"pwm_a",
};

static const char * const pwm_b_groups[] = {
	"pwm_b",
};

static const char * const pwm_c_groups[] = {
	"pwm_c_z", "pwm_c_h",
};

static const char * const pwm_d_groups[] = {
	"pwm_d_dv", "pwm_d_z",
};

static const char * const pwm_vs_groups[] = {
	"pwm_vs_dv", "pwm_vs_z6", "pwm_vs_z7",
};

static const char * const pwm_e_groups[] = {
	"pwm_e_dv", "pwm_e_h", "pwm_e_y", "pwm_e_z",
};

static const char * const pwm_f_groups[] = {
	"pwm_f_dv", "pwm_f_h",
};

static const char * const tsin_a_groups[] = {
	"tsin_d0_a_dv", "tsin_clk_a_dv", "tsin_sop_a_dv",
	"tsin_valid_a_dv",
	"tsin_d0_a_y", "tsin_clk_a_y", "tsin_sop_a_y",
	"tsin_valid_a_y",
	"tsin_d0_a_z", "tsin_clk_a_z", "tsin_sop_a_z",
	"tsin_valid_a_z",
	"tsin_d1_7_a",
};

static const char * const tsin_b_groups[] = {
	"tsin_d0_b", "tsin_d1_b", "tsin_d2_b", "tsin_d3_b",
	"tsin_d4_b", "tsin_d5_b", "tsin_d6_b", "tsin_d7_b",
	"tsin_clk_b", "tsin_sop_b", "tsin_valid_b",
	"tsin_fail_b",
};

static const char * const dtv_groups[] = {
	"dtv_if_agc_dv2", "dtv_if_agc_dv3", "dtv_rf_agc_dv3",
};

static const char * const atv_groups[] = {
	"atv_if_agc_dv", "atv_if_agc_h",
};

static const char * const spi_a_groups[] = {
	"spi_miso_a", "spi_mosi_a", "spi_clk_a",
	"spi_ss0_a", "spi_ss1_a", "spi_ss2_a",
};

static const char * const spi_b_groups[] = {
	"spi_miso_b_dv", "spi_mosi_b_dv", "spi_clk_b_dv",
	"spi_ss0_b_dv", "spi_ss1_b_dv", "spi_ss2_b_dv",
	"spi_miso_b_c", "spi_mosi_b_c", "spi_clk_b_c",
	"spi_ss0_b_c", "spi_ss1_b_c", "spi_ss2_b_c",
};

static const char * const pcm_a_groups[] = {
	"pcm_clk_a_dv", "pcm_fs_a_dv", "pcm_in_a_dv", "pcm_out_a_dv",
	"pcm_clk_a_z", "pcm_fs_a_z", "pcm_in_a_z", "pcm_out_a_z",
};

static const char * const uart_a_groups[] = {
	"uart_tx_a", "uart_rx_a", "uart_cts_a", "uart_rts_a",
};

static const char * const uart_b_groups[] = {
	"uart_tx_b_h", "uart_rx_b_h",
	"uart_tx_b_z", "uart_rx_b_z",
	"uart_cts_b", "uart_rts_b",
};

static const char * const uart_c_groups[] = {
	"uart_tx_c", "uart_rx_c", "uart_cts_c", "uart_rts_c",
};

static const char * const uart_ao_a_ee_groups[] = {
	"uart_tx_ao_a_c4", "uart_rx_ao_a_c5",
	"uart_rx_ao_a_c4", "uart_tx_ao_a_c5",
	"uart_tx_ao_a_w2", "uart_rx_ao_a_w3",
	"uart_tx_ao_a_w6", "uart_rx_ao_a_w7",
	"uart_tx_ao_a_w10", "uart_rx_ao_a_w11",
	"uart_tx_ao_a_w14", "uart_rx_ao_a_w15",
};

static const char * const spdif_out_groups[] = {
	"spdif_out_dv", "spdif_out_h", "spdif_out_z",
};

static const char * const spdif_in_groups[] = {
	"spdif_in_h", "spdif_in_z16", "spdif_in_z18",
};

static const char * const hdmitx_groups[] = {
	"hdmitx_hpd", "hdmitx_sck", "hdmitx_sda",
};

static const char * const hdmirx_a_groups[] = {
	"hdmirx_hpd_a", "hdmirx_det_a", "hdmirx_sda_a", "hdmirx_sck_a",
};

static const char * const hdmirx_b_groups[] = {
	"hdmirx_hpd_b", "hdmirx_det_b", "hdmirx_sda_b", "hdmirx_sck_b",
};

static const char * const hdmirx_c_groups[] = {
	"hdmirx_hpd_c", "hdmirx_det_c", "hdmirx_sda_c", "hdmirx_sck_c",
};

static const char * const hdmirx_d_groups[] = {
	"hdmirx_hpd_d", "hdmirx_det_d", "hdmirx_sda_d", "hdmirx_sck_d",
};

static const char * const audin_groups[] = {
	"audin_lrclk_h7", "audin_sclk_h8", "audin_mclk_h",
	"audin_lrclkin_h7", "audin_sclkin_h8",
	"audin_lrclk_z9", "audin_sclk_z8", "audin_mclk_z",
	"audin_lrclkin_z9", "audin_sclkin_z8",
};

static const char * const i2s_groups[] = {
	"i2s_lrclk_h", "i2s_sclk_h", "i2s_mclk_h",
	"i2s_dout01_h6", "i2s_dout23_h5", "i2s_dout45_h4", "i2s_dout67_h0",
	"i2s_din01_h6", "i2s_din23_h5", "i2s_din45_h4", "i2s_din67_h0",
	"i2s_lrclk_z", "i2s_sclk_z", "i2s_mclk_z",
	"i2s_dout01_z", "i2s_dout23_z15", "i2s_dout45_z", "i2s_dout67_z19",
	"i2s_din01_z", "i2s_din23_z15", "i2s_din45_z", "i2s_din67_z19",
};

static const char * const acodec_i2s_groups[] = {
	"a_i2s_mclk", "a_i2s_sclkin", "a_i2s_lrclkin",
	"a_i2s_din01", "a_i2s_din23",
};

static const char * const lcd_groups[] = {
	"lcd_r0_1", "lcd_r2_7",
	"lcd_g0_1", "lcd_g2_7",
	"lcd_b0_1", "lcd_b2_7",
	"lcd_hs", "lcd_vs",
};

static const char * const tcon_groups[] = {
	"tcon_oeh", "tcon_oev", "tcon_cpv", "tcon_cph_y24",
	"tcon_clko_y24", "tcon_vcom_y8", "tcon_vcom_y26",
};

static const char * const tsout_groups[] = {
	"tsout_fail", "tsout_valid", "tsout_sop", "tsout_clk",
	"tsout_d0", "tsout_d1", "tsout_d2", "tsout_d3",
	"tsout_d4", "tsout_d5", "tsout_d6", "tsout_d7",
};

static const char * const eth_groups[] = {
	"eth_mdio", "eth_mdc", "eth_rgmii_rx_clk", "eth_rx_dv",
	"eth_rxd0", "eth_rxd1", "eth_rxd2", "eth_rxd3",
	"eth_rgmii_tx_clk", "eth_tx_en",
	"eth_txd0", "eth_txd1", "eth_txd2", "eth_txd3",
	"eth_link_led", "eth_act_led",
};

static const char * const vbyone_groups[] = {
	"vx1_lockn", "vx1_htpdn",
};

static struct meson_pmx_func meson_txlx_periphs_functions[] = {
	FUNCTION(gpio_periphs),
	FUNCTION(emmc),
	FUNCTION(nor),
	FUNCTION(sdcard),
	FUNCTION(i2c0),
	FUNCTION(i2c1),
	FUNCTION(i2c2),
	FUNCTION(i2c3),
	FUNCTION(i2c_slave_ao_ee),
	FUNCTION(pwm_a),
	FUNCTION(pwm_b),
	FUNCTION(pwm_c),
	FUNCTION(pwm_d),
	FUNCTION(pwm_vs),
	FUNCTION(pwm_e),
	FUNCTION(pwm_f),
	FUNCTION(tsin_a),
	FUNCTION(tsin_b),
	FUNCTION(atv),
	FUNCTION(dtv),
	FUNCTION(spi_a),
	FUNCTION(spi_b),
	FUNCTION(pcm_a),
	FUNCTION(uart_a),
	FUNCTION(uart_b),
	FUNCTION(uart_c),
	FUNCTION(uart_ao_a_ee),
	FUNCTION(spdif_out),
	FUNCTION(spdif_in),
	FUNCTION(hdmitx),
	FUNCTION(hdmirx_a),
	FUNCTION(hdmirx_b),
	FUNCTION(hdmirx_c),
	FUNCTION(hdmirx_d),
	FUNCTION(audin),
	FUNCTION(i2s),
	FUNCTION(acodec_i2s),
	FUNCTION(lcd),
	FUNCTION(tcon),
	FUNCTION(tsout),
	FUNCTION(eth),
	FUNCTION(vbyone),
};

static const char * const gpio_aobus_groups[] = {
	"GPIOAO_0", "GPIOAO_1", "GPIOAO_2", "GPIOAO_3", "GPIOAO_4",
	"GPIOAO_5", "GPIOAO_6", "GPIOAO_7", "GPIOAO_8", "GPIOAO_9",
	"GPIOAO_10", "GPIOAO_11", "GPIOAO_12", "GPIOAO_13",
	"GPIO_TEST_N",
};

static const char * const uart_ao_a_groups[] = {
	"uart_tx_ao_a", "uart_rx_ao_a", "uart_cts_ao_a",
	"uart_rts_ao_a",
};

static const char * const uart_ao_b_groups[] = {
	"uart_tx_ao_b_ao0", "uart_rx_ao_b_ao1",
	"uart_tx_ao_b_ao4", "uart_rx_ao_b_ao5",
	"uart_tx_ao_b_ao10", "uart_rx_ao_b_ao11",
	"uart_cts_ao_b", "uart_rts_ao_b",
};

static const char * const ir_in_groups[] = {
	"remote_in",
};

static const char *const ir_out_groups[] = {
	"remote_out_ao2", "remote_out_ao6",
};

static const char * const pwm_ao_a_groups[] = {
	"pwm_ao_a_ao3", "pwm_ao_a_ao7",
};

static const char * const pwm_ao_b_groups[] = {
	"pwm_ao_b_ao8", "pwm_ao_b_ao9",
};

static const char * const pwm_ao_c_groups[] = {
	"pwm_ao_c_ao10", "pwm_ao_c_ao12",
};

static const char * const pwm_ao_d_groups[] = {
	"pwm_ao_d_ao2", "pwm_ao_d_ao11", "pwm_ao_d_ao13",
};

static const char * const i2c_ao_groups[] = {
	"i2c_sck_ao4", "i2c_sda_ao5",
	"i2c_sck_ao10", "i2c_sda_ao11",
};

static const char * const i2c_slave_ao_groups[] = {
	"i2c_slave_sck_ao4", "i2c_slave_sda_ao5",
	"i2c_slave_sck_ao10", "i2c_slave_sda_ao11",
};

static const char * const ao_cec_groups[] = {
	"ao_cec_ao7", "ao_cec_ao8",
};

static const char * const ao_cec_b_groups[] = {
	"ao_cec_b_ao7", "ao_cec_b_ao8",
};

static struct meson_pmx_func meson_txlx_aobus_functions[] = {
	FUNCTION(gpio_aobus),
	FUNCTION(uart_ao_a),
	FUNCTION(uart_ao_b),
	FUNCTION(ir_in),
	FUNCTION(ir_out),
	FUNCTION(pwm_ao_a),
	FUNCTION(pwm_ao_b),
	FUNCTION(pwm_ao_c),
	FUNCTION(pwm_ao_d),
	FUNCTION(i2c_ao),
	FUNCTION(i2c_slave_ao),
	FUNCTION(ao_cec),
	FUNCTION(ao_cec_b),
};

/*
 * GPIOW is OD pin, it don't support pull-up/down. In order to compatible
 * with the current software, we assume it located in reserved bit field
 * PULL_UP_EN_REG1 bit[0-15] and PULL_UP_REG1 bit[0-15].
 */
static struct meson_bank meson_txlx_periphs_banks[] = {
	/*   name    first   last  irq  pullen  pull    dir     out     in  */
	BANK("Z", GPIOZ_0, GPIOZ_19, 14,
	3,  0,  3,  0,  9,  0,  10, 0,  11, 0),
	BANK("H", GPIOH_0, GPIOH_10, 34,
	1,  20, 1,  20, 3,  20, 4,  20, 5,  20),
	BANK("BOOT", BOOT_0, BOOT_11, 45,
	2,  0,  2,  0,  6,  0,  7,  0,  8,  0),
	BANK("C", GPIOC_0, GPIOC_5, 58,
	2,  20, 2,  20, 6,  20, 7,  20, 8,  20),
	BANK("DV", GPIODV_0, GPIODV_10, 64,
	0,  0,  0,  0,  0,  0,  1,  0,  2,  0),
	BANK("W", GPIOW_0, GPIOW_15, 75,
	1,  0,  1,  0,  3,  0,  4,  0,  5,  0),
	BANK("Y", GPIOY_0, GPIOY_27, 91,
	4,  0,  4,  0,  12, 0,  13, 0,  14, 0),
};

/*
 * TEST_N is special pin, only used as gpio output at present.
 * the direction control bit from AO_SEC_REG0 bit[0], it
 * configured to output when pinctrl driver is initialized.
 * to make the api of gpiolib work well, the reserved bit(bit[14])
 * seen as direction control bit.
 *
 * AO_GPIO_O_EN_N       0x09<<2=0x24     bit[31]     output level
 * AO_GPIO_I            0x0a<<2=0x28     bit[31]     input level
 * AO_SEC_REG0          0x50<<2=0x140    bit[0]      input enable
 * AO_RTI_PULL_UP_REG   0x0b<<2=0x2c     bit[14]     pull-up/down
 * AO_RTI_PULL_UP_REG   0x0b<<2=0x2c     bit[30]     pull-up enable
 */
static struct meson_bank meson_txlx_aobus_banks[] = {
	/* name    first   last  irq  pullen  pull    dir     out     in  */
	BANK("AO", GPIOAO_0, GPIOAO_13,	0,
	0,  16, 0,  0,  0,  0,  0,  16, 1,  0),
	BANK("TEST", GPIO_TEST_N, GPIO_TEST_N,	-1,
	0,  30, 0,  14, 0,  14, 0,  31, 1,  31),
};

int meson_txlx_aobus_init(struct meson_pinctrl *pc)
{
	struct arm_smccc_res res;
	/*set TEST_N to output*/
	arm_smccc_smc(CMD_TEST_N_DIR, TEST_N_OUTPUT, 0, 0, 0, 0, 0, 0, &res);

	return 0;
}

struct meson_pinctrl_data meson_txlx_periphs_pinctrl_data = {
	.name		= "periphs-banks",
	.init		= NULL,
	.pins		= meson_txlx_periphs_pins,
	.groups		= meson_txlx_periphs_groups,
	.funcs		= meson_txlx_periphs_functions,
	.banks		= meson_txlx_periphs_banks,
	.num_pins	= ARRAY_SIZE(meson_txlx_periphs_pins),
	.num_groups	= ARRAY_SIZE(meson_txlx_periphs_groups),
	.num_funcs	= ARRAY_SIZE(meson_txlx_periphs_functions),
	.num_banks	= ARRAY_SIZE(meson_txlx_periphs_banks),
	.pmx_ops	= &meson8_pmx_ops,
};

struct meson_pinctrl_data meson_txlx_aobus_pinctrl_data = {
	.name		= "aobus-banks",
	.init		= meson_txlx_aobus_init,
	.pins		= meson_txlx_aobus_pins,
	.groups		= meson_txlx_aobus_groups,
	.funcs		= meson_txlx_aobus_functions,
	.banks		= meson_txlx_aobus_banks,
	.num_pins	= ARRAY_SIZE(meson_txlx_aobus_pins),
	.num_groups	= ARRAY_SIZE(meson_txlx_aobus_groups),
	.num_funcs	= ARRAY_SIZE(meson_txlx_aobus_functions),
	.num_banks	= ARRAY_SIZE(meson_txlx_aobus_banks),
	.pmx_ops	= &meson8_pmx_ops,
};

static const struct of_device_id meson_txlx_pinctrl_dt_match[] = {
	{
		.compatible = "amlogic,meson-txlx-periphs-pinctrl",
		.data = &meson_txlx_periphs_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-txlx-aobus-pinctrl",
		.data = &meson_txlx_aobus_pinctrl_data,
	},
	{}	/* Let KASAN shut up */
};

static struct platform_driver meson_txlx_pinctrl_driver = {
	.probe		= meson_pinctrl_probe,
	.driver = {
		.name	= "meson-txlx-pinctrl",
		.of_match_table = meson_txlx_pinctrl_dt_match,
	},
};

static int __init txlx_pmx_init(void)
{
	return platform_driver_register(&meson_txlx_pinctrl_driver);
}

static void __exit txlx_pmx_exit(void)
{
	platform_driver_unregister(&meson_txlx_pinctrl_driver);
}

arch_initcall(txlx_pmx_init);
module_exit(txlx_pmx_exit);
MODULE_DESCRIPTION("txlx pin control driver");
MODULE_LICENSE("GPL v2");
