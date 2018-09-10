/*
 * Pin controller and GPIO driver for Amlogic Meson tl1 SoC.
 *
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 * Author: Xingyu Chen <xingyu.chen@amlogic.com>
 *
 * SPDX-License-Identifier: (GPL-2.0+ or MIT)
 */

#include <dt-bindings/gpio/meson-tl1-gpio.h>
#include <linux/arm-smccc.h>
#include "pinctrl-meson.h"
#include "pinctrl-meson-axg-pmx.h"
#include "pinconf-meson-g12a.h"

static const struct pinctrl_pin_desc meson_tl1_periphs_pins[] = {
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
	MESON_PIN(GPIOH_11),
	MESON_PIN(GPIOH_12),
	MESON_PIN(GPIOH_13),
	MESON_PIN(GPIOH_14),
	MESON_PIN(GPIOH_15),
	MESON_PIN(GPIOH_16),
	MESON_PIN(GPIOH_17),
	MESON_PIN(GPIOH_18),
	MESON_PIN(GPIOH_19),
	MESON_PIN(GPIOH_20),
	MESON_PIN(GPIOH_21),
	MESON_PIN(GPIOH_22),
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
	MESON_PIN(GPIOC_0),
	MESON_PIN(GPIOC_1),
	MESON_PIN(GPIOC_2),
	MESON_PIN(GPIOC_3),
	MESON_PIN(GPIOC_4),
	MESON_PIN(GPIOC_5),
	MESON_PIN(GPIOC_6),
	MESON_PIN(GPIOC_7),
	MESON_PIN(GPIOC_8),
	MESON_PIN(GPIOC_9),
	MESON_PIN(GPIOC_10),
	MESON_PIN(GPIOC_11),
	MESON_PIN(GPIOC_12),
	MESON_PIN(GPIOC_13),
	MESON_PIN(GPIOC_14),
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
};

static const struct pinctrl_pin_desc meson_tl1_aobus_pins[] = {
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
	MESON_PIN(GPIOE_0),
	MESON_PIN(GPIOE_1),
	MESON_PIN(GPIOE_2),
	MESON_PIN(GPIO_TEST_N),
};

/* emmc */
static const unsigned int emmc_nand_d0_pins[] = {BOOT_0};
static const unsigned int emmc_nand_d1_pins[] = {BOOT_1};
static const unsigned int emmc_nand_d2_pins[] = {BOOT_2};
static const unsigned int emmc_nand_d3_pins[] = {BOOT_3};
static const unsigned int emmc_nand_d4_pins[] = {BOOT_4};
static const unsigned int emmc_nand_d5_pins[] = {BOOT_5};
static const unsigned int emmc_nand_d6_pins[] = {BOOT_6};
static const unsigned int emmc_nand_d7_pins[] = {BOOT_7};

static const unsigned int emmc_clk_pins[] = {BOOT_8};
static const unsigned int emmc_cmd_pins[] = {BOOT_10};
static const unsigned int emmc_nand_ds_pins[] = {BOOT_11};

/* nand */
static const unsigned int nand_wen_clk_pins[] = {BOOT_8};
static const unsigned int nand_ale_pins[] = {BOOT_9};
static const unsigned int nand_ren_wr_pins[] = {BOOT_10};
static const unsigned int nand_cle_pins[] = {BOOT_11};
static const unsigned int nand_ce0_pins[] = {BOOT_12};

/* nor */
static const unsigned int nor_hold_pins[] = {BOOT_3};
static const unsigned int nor_d_pins[] = {BOOT_4};
static const unsigned int nor_q_pins[] = {BOOT_5};
static const unsigned int nor_c_pins[] = {BOOT_6};
static const unsigned int nor_wp_pins[] = {BOOT_7};
static const unsigned int nor_cs_pins[] = {BOOT_13};

/* sdcard */
static const unsigned int sdcard_d0_pins[] = {GPIOC_0};
static const unsigned int sdcard_d1_pins[] = {GPIOC_1};
static const unsigned int sdcard_d2_pins[] = {GPIOC_2};
static const unsigned int sdcard_d3_pins[] = {GPIOC_3};
static const unsigned int sdcard_clk_pins[] = {GPIOC_4};
static const unsigned int sdcard_cmd_pins[] = {GPIOC_5};

/* spi0 */
static const unsigned int spi0_miso_c_pins[] = {GPIOC_11};
static const unsigned int spi0_mosi_c_pins[] = {GPIOC_12};
static const unsigned int spi0_clk_c_pins[] = {GPIOC_13};
static const unsigned int spi0_ss0_c_pins[] = {GPIOC_14};

static const unsigned int spi0_miso_h_pins[] = {GPIOH_17};
static const unsigned int spi0_mosi_h_pins[] = {GPIOH_18};
static const unsigned int spi0_clk_h_pins[] = {GPIOH_19};
static const unsigned int spi0_ss0_h_pins[] = {GPIOH_20};

static const unsigned int spi0_ss1_pins[] = {GPIOH_0};
static const unsigned int spi0_ss2_pins[] = {GPIOH_1};

/* spi1 */
static const unsigned int spi1_miso_c_pins[] = {GPIOC_0};
static const unsigned int spi1_mosi_c_pins[] = {GPIOC_1};
static const unsigned int spi1_clk_c_pins[] = {GPIOC_2};
static const unsigned int spi1_ss0_c_pins[] = {GPIOC_3};
static const unsigned int spi1_ss1_c_pins[] = {GPIOC_4};

static const unsigned int spi1_ss2_pins[] = {GPIOC_5};

static const unsigned int spi1_miso_dv_pins[] = {GPIODV_7};
static const unsigned int spi1_mosi_dv_pins[] = {GPIODV_8};
static const unsigned int spi1_clk_dv_pins[] = {GPIODV_9};
static const unsigned int spi1_ss0_dv_pins[] = {GPIODV_10};
static const unsigned int spi1_ss1_dv_pins[] = {GPIODV_11};

/* i2c0 */
static const unsigned int i2c0_sda_c_pins[] = {GPIOC_9};
static const unsigned int i2c0_sck_c_pins[] = {GPIOC_8};

static const unsigned int i2c0_sda_dv_pins[] = {GPIODV_0};
static const unsigned int i2c0_sck_dv_pins[] = {GPIODV_1};

/* i2c1 */
static const unsigned int i2c1_sda_z_pins[] = {GPIOZ_1};
static const unsigned int i2c1_sck_z_pins[] = {GPIOZ_2};

static const unsigned int i2c1_sda_h_pins[] = {GPIOH_22};
static const unsigned int i2c1_sck_h_pins[] = {GPIOH_21};

/* i2c2 */
static const unsigned int i2c2_sda_h_pins[] = {GPIOH_20};
static const unsigned int i2c2_sck_h_pins[] = {GPIOH_19};

static const unsigned int i2c2_sda_z_pins[] = {GPIOZ_10};
static const unsigned int i2c2_sck_z_pins[] = {GPIOZ_9};

/* i2c3 */
static const unsigned int i2c3_sda_dv_pins[] = {GPIODV_10};
static const unsigned int i2c3_sck_dv_pins[] = {GPIODV_9};

static const unsigned int i2c3_sda_h1_pins[] = {GPIOH_1};
static const unsigned int i2c3_sck_h0_pins[] = {GPIOH_0};

static const unsigned int i2c3_sda_h20_pins[] = {GPIOH_20};
static const unsigned int i2c3_sck_h19_pins[] = {GPIOH_19};

static const unsigned int i2c3_sda_c_pins[] = {GPIOC_14};
static const unsigned int i2c3_sck_c_pins[] = {GPIOC_13};

/* uart_a */
static const unsigned int uart_a_tx_pins[] = {GPIOC_6};
static const unsigned int uart_a_rx_pins[] = {GPIOC_7};
static const unsigned int uart_a_cts_pins[] = {GPIOC_8};
static const unsigned int uart_a_rts_pins[] = {GPIOC_9};

/* uart_b */
static const unsigned int uart_b_tx_pins[] = {GPIOH_17};
static const unsigned int uart_b_rx_pins[] = {GPIOH_18};
static const unsigned int uart_b_cts_pins[] = {GPIOH_19};
static const unsigned int uart_b_rts_pins[] = {GPIOH_20};

/* uart_c */
static const unsigned int uart_c_tx_pins[] = {GPIODV_7};
static const unsigned int uart_c_rx_pins[] = {GPIODV_8};
static const unsigned int uart_c_cts_pins[] = {GPIODV_9};
static const unsigned int uart_c_rts_pins[] = {GPIODV_10};

/* uart_ao_a_ee */
static const unsigned int uart_ao_a_tx_w2_pins[] = {GPIOW_2};
static const unsigned int uart_ao_a_rx_w3_pins[] = {GPIOW_3};

static const unsigned int uart_ao_a_tx_w6_pins[] = {GPIOW_6};
static const unsigned int uart_ao_a_rx_w7_pins[] = {GPIOW_7};

static const unsigned int uart_ao_a_tx_w10_pins[] = {GPIOW_10};
static const unsigned int uart_ao_a_rx_w11_pins[] = {GPIOW_11};

static const unsigned int uart_ao_a_tx_c_pins[] = {GPIOC_3};
static const unsigned int uart_ao_a_rx_c_pins[] = {GPIOC_2};

/* iso7816 */
static const unsigned int iso7816_clk_pins[] = {GPIODV_4};
static const unsigned int iso7816_data_pins[] = {GPIODV_5};

/* eth */
static const unsigned int eth_mdio_pins[] = {GPIOH_2};
static const unsigned int eth_mdc_pins[] = {GPIOH_3};
static const unsigned int eth_rgmii_rx_clk_pins[] = {GPIOH_4};
static const unsigned int eth_rx_dv_pins[] = {GPIOH_5};
static const unsigned int eth_rxd0_pins[] = {GPIOH_6};
static const unsigned int eth_rxd1_pins[] = {GPIOH_7};
static const unsigned int eth_rxd2_rgmii_pins[] = {GPIOH_8};
static const unsigned int eth_rxd3_rgmii_pins[] = {GPIOH_9};
static const unsigned int eth_rgmii_tx_clk_pins[] = {GPIOH_10};
static const unsigned int eth_txen_pins[] = {GPIOH_11};
static const unsigned int eth_txd0_pins[] = {GPIOH_12};
static const unsigned int eth_txd1_pins[] = {GPIOH_13};
static const unsigned int eth_txd2_rgmii_pins[] = {GPIOH_14};
static const unsigned int eth_txd3_rgmii_pins[] = {GPIOH_15};
static const unsigned int eth_link_led_pins[] = {GPIOH_1};
static const unsigned int eth_act_led_pins[] = {GPIOH_0};

/* pwm_a */
static const unsigned int pwm_a_pins[] = {GPIOZ_3};

/* pwm_b */
static const unsigned int pwm_b_c_pins[] = {GPIOC_10};
static const unsigned int pwm_b_z_pins[] = {GPIOZ_4};

/* pwm_c */
static const unsigned int pwm_c_dv_pins[] = {GPIODV_2};
static const unsigned int pwm_c_h_pins[] = {GPIOH_13};
static const unsigned int pwm_c_z_pins[] = {GPIOZ_5};

/* pwm_d */
static const unsigned int pwm_d_dv_pins[] = {GPIODV_3};
static const unsigned int pwm_d_z_pins[] = {GPIOZ_6};

/* pwm_e */
static const unsigned int pwm_e_dv_pins[] = {GPIODV_6};
static const unsigned int pwm_e_z_pins[] = {GPIOZ_9};

/* pwm_f */
static const unsigned int pwm_f_dv_pins[] = {GPIODV_11};
static const unsigned int pwm_f_z_pins[] = {GPIOZ_10};

/* pwm_vs */
static const unsigned int pwm_vs_z5_pins[] = {GPIOZ_5};
static const unsigned int pwm_vs_z6_pins[] = {GPIOZ_6};

/* jtag_b */
static const unsigned int jtag_b_tdo_pins[] = {GPIOC_0};
static const unsigned int jtag_b_tdi_pins[] = {GPIOC_1};
static const unsigned int jtag_b_clk_pins[] = {GPIOC_4};
static const unsigned int jtag_b_tms_pins[] = {GPIOC_5};

/* bt656 */
static const unsigned int bt656_a_clk_pins[] = {GPIOH_2};
static const unsigned int bt656_a_vs_pins[] = {GPIOH_3};
static const unsigned int bt656_a_hs_pins[] = {GPIOH_4};
static const unsigned int bt656_a_din0_pins[] = {GPIOH_5};
static const unsigned int bt656_a_din1_pins[] = {GPIOH_6};
static const unsigned int bt656_a_din2_pins[] = {GPIOH_7};
static const unsigned int bt656_a_din3_pins[] = {GPIOH_8};
static const unsigned int bt656_a_din4_pins[] = {GPIOH_9};
static const unsigned int bt656_a_din5_pins[] = {GPIOH_10};
static const unsigned int bt656_a_din6_pins[] = {GPIOH_11};
static const unsigned int bt656_a_din7_pins[] = {GPIOH_12};

/* tsin_a */
static const unsigned int tsin_a_din7_pins[] = {GPIODV_0};
static const unsigned int tsin_a_din6_pins[] = {GPIODV_1};
static const unsigned int tsin_a_din5_pins[] = {GPIODV_2};
static const unsigned int tsin_a_din4_pins[] = {GPIODV_3};
static const unsigned int tsin_a_din3_pins[] = {GPIODV_4};
static const unsigned int tsin_a_din2_pins[] = {GPIODV_5};
static const unsigned int tsin_a_din1_pins[] = {GPIODV_6};

static const unsigned int tsin_a_din0_pins[] = {GPIODV_7};
static const unsigned int tsin_a_clk_pins[] = {GPIODV_8};
static const unsigned int tsin_a_sop_pins[] = {GPIODV_9};
static const unsigned int tsin_a_valid_pins[] = {GPIODV_10};

/* tsin_b */
static const unsigned int tsin_b_din0_pins[] = {GPIOH_4};
static const unsigned int tsin_b_clk_pins[] = {GPIOH_5};
static const unsigned int tsin_b_sop_pins[] = {GPIOH_6};
static const unsigned int tsin_b_valid_pins[] = {GPIOH_7};

/* tsin_c */
static const unsigned int tsin_c_din0_pins[] = {GPIOH_8};
static const unsigned int tsin_c_clk_pins[] = {GPIOH_9};
static const unsigned int tsin_c_sop_pins[] = {GPIOH_10};
static const unsigned int tsin_c_valid_pins[] = {GPIOH_11};

/* tsout */
static const unsigned int tsout_clk_pins[] = {GPIOC_0};
static const unsigned int tsout_sop_pins[] = {GPIOC_1};
static const unsigned int tsout_valid_pins[] = {GPIOC_2};
static const unsigned int tsout_dout0_pins[] = {GPIOC_3};
static const unsigned int tsout_dout1_pins[] = {GPIOC_4};
static const unsigned int tsout_dout2_pins[] = {GPIOC_5};
static const unsigned int tsout_dout3_pins[] = {GPIOC_6};
static const unsigned int tsout_dout4_pins[] = {GPIOC_7};
static const unsigned int tsout_dout5_pins[] = {GPIOC_8};
static const unsigned int tsout_dout6_pins[] = {GPIOC_9};
static const unsigned int tsout_dout7_pins[] = {GPIOC_10};

/* tcon */
static const unsigned int tcon_0_pins[] = {GPIOH_0};
static const unsigned int tcon_1_pins[] = {GPIOH_1};
static const unsigned int tcon_2_pins[] = {GPIOH_2};
static const unsigned int tcon_3_pins[] = {GPIOH_3};
static const unsigned int tcon_4_pins[] = {GPIOH_4};
static const unsigned int tcon_5_pins[] = {GPIOH_5};
static const unsigned int tcon_6_pins[] = {GPIOH_6};
static const unsigned int tcon_7_pins[] = {GPIOH_7};
static const unsigned int tcon_8_pins[] = {GPIOH_8};
static const unsigned int tcon_9_pins[] = {GPIOH_9};
static const unsigned int tcon_10_pins[] = {GPIOH_10};
static const unsigned int tcon_11_pins[] = {GPIOH_11};
static const unsigned int tcon_12_pins[] = {GPIOH_12};
static const unsigned int tcon_13_pins[] = {GPIOH_13};
static const unsigned int tcon_14_pins[] = {GPIOH_14};
static const unsigned int tcon_15_pins[] = {GPIOH_15};
static const unsigned int tcon_lock_pins[] = {GPIOH_16};
static const unsigned int tcon_spi_mo_pins[] = {GPIOH_17};
static const unsigned int tcon_spi_mi_pins[] = {GPIOH_18};
static const unsigned int tcon_spi_clk_pins[] = {GPIOH_19};
static const unsigned int tcon_spi_ss_pins[] = {GPIOH_20};
static const unsigned int tcon_slv_sda_pins[] = {GPIODV_0};
static const unsigned int tcon_slv_sck_pins[] = {GPIODV_1};
static const unsigned int tcon_aging_pins[] = {GPIODV_2};
static const unsigned int tcon_flicker_pins[] = {GPIODV_4};
static const unsigned int tcon_sfc_h2_pins[] = {GPIOH_2};
static const unsigned int tcon_sfc_h16_pins[] = {GPIOH_16};

/* hdmirx_a */
static const unsigned int hdmirx_a_hpd_pins[] = {GPIOW_0};
static const unsigned int hdmirx_a_det_pins[] = {GPIOW_1};
static const unsigned int hdmirx_a_sda_pins[] = {GPIOW_2};
static const unsigned int hdmirx_a_sck_pins[] = {GPIOW_3};

/* hdmirx_b */
static const unsigned int hdmirx_b_hpd_pins[] = {GPIOW_8};
static const unsigned int hdmirx_b_det_pins[] = {GPIOW_9};
static const unsigned int hdmirx_b_sda_pins[] = {GPIOW_10};
static const unsigned int hdmirx_b_sck_pins[] = {GPIOW_11};

/* hdmirx_c */
static const unsigned int hdmirx_c_hpd_pins[] = {GPIOW_4};
static const unsigned int hdmirx_c_det_pins[] = {GPIOW_5};
static const unsigned int hdmirx_c_sda_pins[] = {GPIOW_6};
static const unsigned int hdmirx_c_sck_pins[] = {GPIOW_7};

/* pdm */
static const unsigned int pdm_dclk_c_pins[] = {GPIOC_6};
static const unsigned int pdm_din0_c_pins[] = {GPIOC_7};
static const unsigned int pdm_din1_c_pins[] = {GPIOC_8};
static const unsigned int pdm_din2_c_pins[] = {GPIOC_9};

static const unsigned int pdm_din3_pins[] = {GPIOC_10};

static const unsigned int pdm_dclk_z_pins[] = {GPIOZ_7};
static const unsigned int pdm_din0_z_pins[] = {GPIOZ_8};
static const unsigned int pdm_din1_z_pins[] = {GPIOZ_9};
static const unsigned int pdm_din2_z_pins[] = {GPIOZ_10};

/* spdif_in */
static const unsigned int spdif_in_pins[] = {GPIODV_5};

/* spdif_out */
static const unsigned int spdif_out_dv4_pins[] = {GPIODV_4};
static const unsigned int spdif_out_dv11_pins[] = {GPIODV_11};
static const unsigned int spdif_out_h_pins[] = {GPIOH_0};
static const unsigned int spdif_out_z_pins[] = {GPIOZ_7};

/* mclk0 */
static const unsigned int mclk0_h_pins[] = {GPIOH_4};
static const unsigned int mclk0_z_pins[] = {GPIOZ_0};

/* mclk1 */
static const unsigned int mclk1_c_pins[] = {GPIOC_10};
static const unsigned int mclk1_z_pins[] = {GPIOZ_8};

/* tdma_in */
static const unsigned int tdma_slv_sclk_pins[] = {GPIOZ_1};
static const unsigned int tdma_slv_fs_pins[] = {GPIOZ_2};
static const unsigned int tdma_din0_z_pins[] = {GPIOZ_7};
static const unsigned int tdma_din1_z_pins[] = {GPIOZ_8};
static const unsigned int tdma_din2_z_pins[] = {GPIOZ_9};
static const unsigned int tdma_din3_z_pins[] = {GPIOZ_10};

static const unsigned int tdma_din0_h_pins[] = {GPIOH_11};
static const unsigned int tdma_din1_h_pins[] = {GPIOH_12};
static const unsigned int tdma_din2_h_pins[] = {GPIOH_13};
static const unsigned int tdma_din3_h_pins[] = {GPIOH_14};

/* tdma_out */
static const unsigned int tdma_sclk_z_pins[] = {GPIOZ_1};
static const unsigned int tdma_fs_z_pins[] = {GPIOZ_2};
static const unsigned int tdma_dout0_z_pins[] = {GPIOZ_3};
static const unsigned int tdma_dout1_z_pins[] = {GPIOZ_4};
static const unsigned int tdma_dout2_z_pins[] = {GPIOZ_5};
static const unsigned int tdma_dout3_z_pins[] = {GPIOZ_6};

static const unsigned int tdma_sclk_h_pins[] = {GPIOH_5};
static const unsigned int tdma_fs_h_pins[] = {GPIOH_6};
static const unsigned int tdma_dout0_h_pins[] = {GPIOH_7};
static const unsigned int tdma_dout1_h_pins[] = {GPIOH_8};
static const unsigned int tdma_dout2_h_pins[] = {GPIOH_9};
static const unsigned int tdma_dout3_h_pins[] = {GPIOH_10};

/* tdmb_in */
static const unsigned int tdmb_slv_sclk_pins[] = {GPIOC_13};
static const unsigned int tdmb_slv_fs_pins[] = {GPIOC_14};
static const unsigned int tdmb_din0_c6_pins[] = {GPIOC_6};
static const unsigned int tdmb_din1_c7_pins[] = {GPIOC_7};
static const unsigned int tdmb_din2_pins[] = {GPIOC_8};
static const unsigned int tdmb_din3_pins[] = {GPIOC_9};

static const unsigned int tdmb_din0_c12_pins[] = {GPIOC_12};
static const unsigned int tdmb_din1_c11_pins[] = {GPIOC_11};

/* tdmb_out */
static const unsigned int tdmb_sclk_c4_pins[] = {GPIOC_4};
static const unsigned int tdmb_fs_c5_pins[] = {GPIOC_5};
static const unsigned int tdmb_dout0_c0_pins[] = {GPIOC_0};
static const unsigned int tdmb_dout1_c1_pins[] = {GPIOC_1};
static const unsigned int tdmb_dout2_pins[] = {GPIOC_2};
static const unsigned int tdmb_dout3_pins[] = {GPIOC_3};

static const unsigned int tdmb_sclk_c13_pins[] = {GPIOC_13};
static const unsigned int tdmb_fs_c14_pins[] = {GPIOC_14};
static const unsigned int tdmb_dout0_c12_pins[] = {GPIOC_12};
static const unsigned int tdmb_dout1_c11_pins[] = {GPIOC_11};

/* tdmc_in */
static const unsigned int tdmc_slv_sclk_pins[] = {GPIODV_7};
static const unsigned int tdmc_slv_fs_pins[] = {GPIODV_8};
static const unsigned int tdmc_din0_pins[] = {GPIODV_9};
static const unsigned int tdmc_din1_pins[] = {GPIODV_10};
static const unsigned int tdmc_din2_pins[] = {GPIODV_11};
static const unsigned int tdmc_din3_pins[] = {GPIODV_6};

/* tdmc_out */
static const unsigned int tdmc_sclk_pins[] = {GPIODV_7};
static const unsigned int tdmc_fs_pins[] = {GPIODV_8};
static const unsigned int tdmc_dout0_pins[] = {GPIODV_9};
static const unsigned int tdmc_dout1_pins[] = {GPIODV_10};
static const unsigned int tdmc_dout2_pins[] = {GPIODV_11};
static const unsigned int tdmc_dout3_pins[] = {GPIODV_6};

/* clk12_24_ee */
static const unsigned int clk12_24_pins[] = {GPIOH_13};

/* gen_clk_ee */
static const unsigned int gen_clk_ee_h17_pins[] = {GPIOH_17};
static const unsigned int gen_clk_ee_h21_pins[] = {GPIOH_21};

/* vx1 */
static const unsigned int vx1_htpdn_pins[] = {GPIOH_15};
static const unsigned int vx1_lockn_pins[] = {GPIOH_16};

/* atv */
static const unsigned int atv_if_agc_dv_pins[] = {GPIODV_2};
static const unsigned int atv_if_agc_c_pins[] = {GPIOC_6};

/* dtv */
static const unsigned int dtv_if_agc_dv2_pins[] = {GPIODV_2};
static const unsigned int dtv_if_agc_dv3_pins[] = {GPIODV_3};
static const unsigned int dtv_rf_agc_dv3_pins[] = {GPIODV_3};

static const unsigned int dtv_if_agc_c6_pins[] = {GPIOC_6};
static const unsigned int dtv_if_agc_c7_pins[] = {GPIOC_7};
static const unsigned int dtv_rf_agc_c7_pins[] = {GPIOC_7};

static struct meson_pmx_group meson_tl1_periphs_groups[] = {
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
	GPIO_GROUP(GPIOH_11),
	GPIO_GROUP(GPIOH_12),
	GPIO_GROUP(GPIOH_13),
	GPIO_GROUP(GPIOH_14),
	GPIO_GROUP(GPIOH_15),
	GPIO_GROUP(GPIOH_16),
	GPIO_GROUP(GPIOH_17),
	GPIO_GROUP(GPIOH_18),
	GPIO_GROUP(GPIOH_19),
	GPIO_GROUP(GPIOH_20),
	GPIO_GROUP(GPIOH_21),
	GPIO_GROUP(GPIOH_22),
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
	GPIO_GROUP(GPIOC_0),
	GPIO_GROUP(GPIOC_1),
	GPIO_GROUP(GPIOC_2),
	GPIO_GROUP(GPIOC_3),
	GPIO_GROUP(GPIOC_4),
	GPIO_GROUP(GPIOC_5),
	GPIO_GROUP(GPIOC_6),
	GPIO_GROUP(GPIOC_7),
	GPIO_GROUP(GPIOC_8),
	GPIO_GROUP(GPIOC_9),
	GPIO_GROUP(GPIOC_10),
	GPIO_GROUP(GPIOC_11),
	GPIO_GROUP(GPIOC_12),
	GPIO_GROUP(GPIOC_13),
	GPIO_GROUP(GPIOC_14),
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

	/* bank BOOT */
	GROUP(emmc_nand_d0,	1),
	GROUP(emmc_nand_d1,	1),
	GROUP(emmc_nand_d2,	1),
	GROUP(emmc_nand_d3,	1),
	GROUP(emmc_nand_d4,	1),
	GROUP(emmc_nand_d5,	1),
	GROUP(emmc_nand_d6,	1),
	GROUP(emmc_nand_d7,	1),
	GROUP(emmc_clk,		1),
	GROUP(emmc_cmd,		1),
	GROUP(emmc_nand_ds,	1),
	GROUP(nand_ce0,		2),
	GROUP(nand_ale,		2),
	GROUP(nand_cle,		2),
	GROUP(nand_wen_clk,	2),
	GROUP(nand_ren_wr,	2),
	GROUP(nor_hold,		3),
	GROUP(nor_d,		3),
	GROUP(nor_q,		3),
	GROUP(nor_c,		3),
	GROUP(nor_wp,		3),
	GROUP(nor_cs,		3),

	/* bank GPIOC */
	GROUP(sdcard_d0,	1),
	GROUP(sdcard_d1,	1),
	GROUP(sdcard_d2,	1),
	GROUP(sdcard_d3,	1),
	GROUP(sdcard_clk,	1),
	GROUP(sdcard_cmd,	1),
	GROUP(spi0_mosi_c,	1),
	GROUP(spi0_miso_c,	1),
	GROUP(spi0_ss0_c,	1),
	GROUP(spi0_clk_c,	1),
	GROUP(spi1_mosi_c,	5),
	GROUP(spi1_miso_c,	5),
	GROUP(spi1_clk_c,	5),
	GROUP(spi1_ss0_c,	5),
	GROUP(spi1_ss1_c,	5),
	GROUP(spi1_ss2,		5),
	GROUP(i2c0_sda_c,	5),
	GROUP(i2c0_sck_c,	5),
	GROUP(i2c3_sda_c,	4),
	GROUP(i2c3_sck_c,	4),
	GROUP(uart_a_tx,	1),
	GROUP(uart_a_rx,	1),
	GROUP(uart_a_cts,	1),
	GROUP(uart_a_rts,	1),
	GROUP(uart_ao_a_tx_c,	2),
	GROUP(uart_ao_a_rx_c,	2),
	GROUP(pwm_b_c,		1),
	GROUP(jtag_b_tdo,	2),
	GROUP(jtag_b_tdi,	2),
	GROUP(jtag_b_clk,	2),
	GROUP(jtag_b_tms,	2),
	GROUP(tsout_clk,	4),
	GROUP(tsout_sop,	4),
	GROUP(tsout_valid,	4),
	GROUP(tsout_dout0,	4),
	GROUP(tsout_dout1,	4),
	GROUP(tsout_dout2,	4),
	GROUP(tsout_dout3,	4),
	GROUP(tsout_dout4,	4),
	GROUP(tsout_dout5,	4),
	GROUP(tsout_dout6,	4),
	GROUP(tsout_dout7,	4),
	GROUP(pdm_dclk_c,	2),
	GROUP(pdm_din0_c,	2),
	GROUP(pdm_din1_c,	2),
	GROUP(pdm_din2_c,	2),
	GROUP(pdm_din3,		2),
	GROUP(mclk1_c,		3),
	GROUP(tdmb_slv_sclk,	3),
	GROUP(tdmb_slv_fs,	3),
	GROUP(tdmb_din0_c6,	3),
	GROUP(tdmb_din1_c7,	3),
	GROUP(tdmb_din2,	3),
	GROUP(tdmb_din3,	3),
	GROUP(tdmb_din0_c12,	3),
	GROUP(tdmb_din1_c11,	3),
	GROUP(tdmb_sclk_c4,	3),
	GROUP(tdmb_fs_c5,	3),
	GROUP(tdmb_dout0_c0,	3),
	GROUP(tdmb_dout1_c1,	3),
	GROUP(tdmb_dout2,	3),
	GROUP(tdmb_dout3,	3),
	GROUP(tdmb_sclk_c13,	2),
	GROUP(tdmb_fs_c14,	2),
	GROUP(tdmb_dout0_c12,	2),
	GROUP(tdmb_dout1_c11,	2),
	GROUP(atv_if_agc_c,	6),
	GROUP(dtv_if_agc_c6,	5),
	GROUP(dtv_if_agc_c7,	5),
	GROUP(dtv_rf_agc_c7,	6),

	/* bank GPIOH */
	GROUP(spi0_mosi_h,	2),
	GROUP(spi0_miso_h,	2),
	GROUP(spi0_ss0_h,	2),
	GROUP(spi0_clk_h,	2),
	GROUP(spi0_ss1,		2),
	GROUP(spi0_ss2,		2),
	GROUP(i2c1_sda_h,	1),
	GROUP(i2c1_sck_h,	1),
	GROUP(i2c2_sda_h,	3),
	GROUP(i2c2_sck_h,	3),
	GROUP(i2c3_sda_h1,	6),
	GROUP(i2c3_sck_h0,	6),
	GROUP(i2c3_sda_h20,	5),
	GROUP(i2c3_sck_h19,	5),
	GROUP(uart_b_tx,	4),
	GROUP(uart_b_rx,	4),
	GROUP(uart_b_cts,	4),
	GROUP(uart_b_rts,	4),
	GROUP(eth_mdio,		2),
	GROUP(eth_mdc,		2),
	GROUP(eth_rgmii_rx_clk,	2),
	GROUP(eth_rx_dv,	2),
	GROUP(eth_rxd0,		2),
	GROUP(eth_rxd1,		2),
	GROUP(eth_rxd2_rgmii,	2),
	GROUP(eth_rxd3_rgmii,	2),
	GROUP(eth_rgmii_tx_clk,	2),
	GROUP(eth_txen,		2),
	GROUP(eth_txd0,		2),
	GROUP(eth_txd1,		2),
	GROUP(eth_txd2_rgmii,	2),
	GROUP(eth_txd3_rgmii,	2),
	GROUP(eth_link_led,	4),
	GROUP(eth_act_led,	4),
	GROUP(pwm_c_h,		4),
	GROUP(bt656_a_vs,	2),
	GROUP(bt656_a_hs,	2),
	GROUP(bt656_a_clk,	2),
	GROUP(bt656_a_din0,	2),
	GROUP(bt656_a_din1,	2),
	GROUP(bt656_a_din2,	2),
	GROUP(bt656_a_din3,	2),
	GROUP(bt656_a_din4,	2),
	GROUP(bt656_a_din5,	2),
	GROUP(bt656_a_din6,	2),
	GROUP(bt656_a_din7,	2),
	GROUP(tsin_b_valid,	4),
	GROUP(tsin_b_sop,	4),
	GROUP(tsin_b_din0,	4),
	GROUP(tsin_b_clk,	4),
	GROUP(tsin_c_valid,	4),
	GROUP(tsin_c_sop,	4),
	GROUP(tsin_c_din0,	4),
	GROUP(tsin_c_clk,	4),
	GROUP(tcon_0,		1),
	GROUP(tcon_1,		1),
	GROUP(tcon_2,		1),
	GROUP(tcon_3,		1),
	GROUP(tcon_4,		1),
	GROUP(tcon_5,		1),
	GROUP(tcon_6,		1),
	GROUP(tcon_7,		1),
	GROUP(tcon_8,		1),
	GROUP(tcon_9,		1),
	GROUP(tcon_10,		1),
	GROUP(tcon_11,		1),
	GROUP(tcon_12,		1),
	GROUP(tcon_13,		1),
	GROUP(tcon_14,		1),
	GROUP(tcon_15,		1),
	GROUP(tcon_lock,	1),
	GROUP(tcon_spi_mo,	1),
	GROUP(tcon_spi_mi,	1),
	GROUP(tcon_spi_clk,	1),
	GROUP(tcon_spi_ss,	1),
	GROUP(tcon_sfc_h2,	4),
	GROUP(tcon_sfc_h16,	2),
	GROUP(spdif_out_h,	5),
	GROUP(mclk0_h,		5),
	GROUP(tdma_din0_h,	5),
	GROUP(tdma_din1_h,	5),
	GROUP(tdma_din2_h,	5),
	GROUP(tdma_din3_h,	5),
	GROUP(tdma_sclk_h,	5),
	GROUP(tdma_fs_h,	5),
	GROUP(tdma_dout0_h,	5),
	GROUP(tdma_dout1_h,	5),
	GROUP(tdma_dout2_h,	5),
	GROUP(tdma_dout3_h,	5),
	GROUP(clk12_24,		3),
	GROUP(gen_clk_ee_h17,	5),
	GROUP(gen_clk_ee_h21,	5),
	GROUP(vx1_htpdn,	3),
	GROUP(vx1_lockn,	3),

	/* bank GPIODV */
	GROUP(spi1_mosi_dv,	4),
	GROUP(spi1_miso_dv,	4),
	GROUP(spi1_clk_dv,	4),
	GROUP(spi1_ss0_dv,	4),
	GROUP(spi1_ss1_dv,	4),
	GROUP(i2c0_sda_dv,	1),
	GROUP(i2c0_sck_dv,	1),
	GROUP(i2c3_sda_dv,	6),
	GROUP(i2c3_sck_dv,	6),
	GROUP(uart_c_tx,	5),
	GROUP(uart_c_rx,	5),
	GROUP(uart_c_cts,	5),
	GROUP(uart_c_rts,	5),
	GROUP(iso7816_clk,	4),
	GROUP(iso7816_data,	4),
	GROUP(pwm_c_dv,		5),
	GROUP(pwm_d_dv,		5),
	GROUP(pwm_e_dv,		4),
	GROUP(pwm_f_dv,		5),
	GROUP(tsin_a_valid,	3),
	GROUP(tsin_a_sop,	3),
	GROUP(tsin_a_din0,	3),
	GROUP(tsin_a_clk,	3),
	GROUP(tsin_a_din1,	3),
	GROUP(tsin_a_din2,	3),
	GROUP(tsin_a_din3,	3),
	GROUP(tsin_a_din4,	3),
	GROUP(tsin_a_din5,	3),
	GROUP(tsin_a_din6,	3),
	GROUP(tsin_a_din7,	3),
	GROUP(tcon_slv_sda,	2),
	GROUP(tcon_slv_sck,	2),
	GROUP(tcon_aging,	4),
	GROUP(tcon_flicker,	2),
	GROUP(spdif_in,		1),
	GROUP(spdif_out_dv4,	1),
	GROUP(spdif_out_dv11,	3),
	GROUP(tdmc_slv_sclk,	2),
	GROUP(tdmc_slv_fs,	2),
	GROUP(tdmc_din0,	2),
	GROUP(tdmc_din1,	2),
	GROUP(tdmc_din2,	2),
	GROUP(tdmc_din3,	2),
	GROUP(tdmc_sclk,	1),
	GROUP(tdmc_fs,		1),
	GROUP(tdmc_dout0,	1),
	GROUP(tdmc_dout1,	1),
	GROUP(tdmc_dout2,	1),
	GROUP(tdmc_dout3,	1),
	GROUP(atv_if_agc_dv,	2),
	GROUP(dtv_if_agc_dv2,	1),
	GROUP(dtv_if_agc_dv3,	1),
	GROUP(dtv_rf_agc_dv3,	2),

	/* bank GPIOZ */
	GROUP(i2c1_sda_z,	3),
	GROUP(i2c1_sck_z,	3),
	GROUP(i2c2_sda_z,	1),
	GROUP(i2c2_sck_z,	1),
	GROUP(pwm_a,		4),
	GROUP(pwm_b_z,		4),
	GROUP(pwm_c_z,		4),
	GROUP(pwm_d_z,		4),
	GROUP(pwm_e_z,		4),
	GROUP(pwm_f_z,		4),
	GROUP(pwm_vs_z5,	3),
	GROUP(pwm_vs_z6,	3),
	GROUP(pdm_dclk_z,	3),
	GROUP(pdm_din0_z,	3),
	GROUP(pdm_din1_z,	3),
	GROUP(pdm_din2_z,	3),
	GROUP(spdif_out_z,	1),
	GROUP(mclk0_z,		1),
	GROUP(mclk1_z,		1),
	GROUP(tdma_slv_sclk,	2),
	GROUP(tdma_slv_fs,	2),
	GROUP(tdma_din0_z,	2),
	GROUP(tdma_din1_z,	2),
	GROUP(tdma_din2_z,	2),
	GROUP(tdma_din3_z,	2),
	GROUP(tdma_sclk_z,	1),
	GROUP(tdma_fs_z,	1),
	GROUP(tdma_dout0_z,	1),
	GROUP(tdma_dout1_z,	1),
	GROUP(tdma_dout2_z,	1),
	GROUP(tdma_dout3_z,	1),

	/* bank GPIOW */
	GROUP(uart_ao_a_tx_w2,	2),
	GROUP(uart_ao_a_rx_w3,	2),
	GROUP(uart_ao_a_tx_w6,	2),
	GROUP(uart_ao_a_rx_w7,	2),
	GROUP(uart_ao_a_tx_w10,	2),
	GROUP(uart_ao_a_rx_w11,	2),
	GROUP(hdmirx_a_hpd,	1),
	GROUP(hdmirx_a_det,	1),
	GROUP(hdmirx_a_sda,	1),
	GROUP(hdmirx_a_sck,	1),
	GROUP(hdmirx_b_hpd,	1),
	GROUP(hdmirx_b_det,	1),
	GROUP(hdmirx_b_sda,	1),
	GROUP(hdmirx_b_sck,	1),
	GROUP(hdmirx_c_hpd,	1),
	GROUP(hdmirx_c_det,	1),
	GROUP(hdmirx_c_sda,	1),
	GROUP(hdmirx_c_sck,	1),
};

/* uart_ao_a */
static const unsigned int uart_ao_a_tx_pins[] = {GPIOAO_0};
static const unsigned int uart_ao_a_rx_pins[] = {GPIOAO_1};
static const unsigned int uart_ao_a_cts_pins[] = {GPIOE_0};
static const unsigned int uart_ao_a_rts_pins[] = {GPIOE_1};

/* uart_ao_b */
static const unsigned int uart_ao_b_tx_2_pins[] = {GPIOAO_2};
static const unsigned int uart_ao_b_rx_3_pins[] = {GPIOAO_3};

static const unsigned int uart_ao_b_tx_8_pins[] = {GPIOAO_8};
static const unsigned int uart_ao_b_rx_9_pins[] = {GPIOAO_9};

static const unsigned int uart_ao_b_cts_pins[] = {GPIOE_0};
static const unsigned int uart_ao_b_rts_pins[] = {GPIOE_1};

/* i2c_ao */
static const unsigned int i2c_ao_sck_2_pins[] = {GPIOAO_2};
static const unsigned int i2c_ao_sda_3_pins[] = {GPIOAO_3};

static const unsigned int i2c_ao_sck_e_pins[] = {GPIOE_0};
static const unsigned int i2c_ao_sda_e_pins[] = {GPIOE_1};

/* i2c_ao_slave */
static const unsigned int i2c_ao_slave_sck_pins[] = {GPIOAO_2};
static const unsigned int i2c_ao_slave_sda_pins[] = {GPIOAO_3};

/* ir_in */
static const unsigned int remote_input_ao_pins[] = {GPIOAO_5};

/* ir_out */
static const unsigned int remote_out_ao_pins[] = {GPIOAO_4};
static const unsigned int remote_out_ao9_pins[] = {GPIOAO_9};

/* pwm_ao_a */
static const unsigned int pwm_ao_a_pins[] = {GPIOAO_11};
static const unsigned int pwm_ao_a_hiz_pins[] = {GPIOAO_11};

/* pwm_ao_b */
static const unsigned int pwm_ao_b_pins[] = {GPIOE_0};

/* pwm_ao_c */
static const unsigned int pwm_ao_c_4_pins[] = {GPIOAO_4};
static const unsigned int pwm_ao_c_hiz_4_pins[] = {GPIOAO_4};
static const unsigned int pwm_ao_c_6_pins[] = {GPIOAO_6};
static const unsigned int pwm_ao_c_hiz_7_pins[] = {GPIOAO_7};

/* pwm_ao_d */
static const unsigned int pwm_ao_d_5_pins[] = {GPIOAO_5};
static const unsigned int pwm_ao_d_10_pins[] = {GPIOAO_10};
static const unsigned int pwm_ao_d_e_pins[] = {GPIOE_1};

/* jtag_a */
static const unsigned int jtag_a_tdi_pins[] = {GPIOAO_8};
static const unsigned int jtag_a_tdo_pins[] = {GPIOAO_9};
static const unsigned int jtag_a_clk_pins[] = {GPIOAO_6};
static const unsigned int jtag_a_tms_pins[] = {GPIOAO_7};

/* cec_ao */
static const unsigned int cec_ao_a_pins[] = {GPIOAO_10};
static const unsigned int cec_ao_b_pins[] = {GPIOAO_10};

/* spdif_out_ao */
static const unsigned int spdif_out_ao_pins[] = {GPIOAO_6};

/* spdif_in_ao */
static const unsigned int spdif_in_ao_pins[] = {GPIOAO_7};

/* pwm_a_e2 */
static const unsigned int pwm_a_e2_pins[] = {GPIOE_2};

/* clk25_ee */
static const unsigned int clk25_ee_pins[] = {GPIOE_2};

/* clk12_24_ao */
static const unsigned int clk12_24_ao_pins[] = {GPIOE_2};

/* gen_clk_ee_ao */
static const unsigned int gen_clk_ee_ao_pins[] = {GPIOAO_11};

/* gen_clk_ao */
static const unsigned int gen_clk_ao_pins[] = {GPIOAO_11};

static struct meson_pmx_group meson_tl1_aobus_groups[] = {
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
	GPIO_GROUP(GPIOE_0),
	GPIO_GROUP(GPIOE_1),
	GPIO_GROUP(GPIOE_2),
	GPIO_GROUP(GPIO_TEST_N),

	/* bank AO */
	GROUP(uart_ao_a_tx,	1),
	GROUP(uart_ao_a_rx,	1),
	GROUP(uart_ao_a_cts,	1),
	GROUP(uart_ao_a_rts,	1),
	GROUP(uart_ao_b_tx_2,	2),
	GROUP(uart_ao_b_rx_3,	2),
	GROUP(uart_ao_b_tx_8,	3),
	GROUP(uart_ao_b_rx_9,	3),
	GROUP(uart_ao_b_cts,	2),
	GROUP(uart_ao_b_rts,	2),
	GROUP(i2c_ao_sck_2,	1),
	GROUP(i2c_ao_sda_3,	1),
	GROUP(i2c_ao_sck_e,	4),
	GROUP(i2c_ao_sda_e,	4),
	GROUP(i2c_ao_slave_sck,	3),
	GROUP(i2c_ao_slave_sda,	3),
	GROUP(remote_input_ao,	1),
	GROUP(remote_out_ao,	1),
	GROUP(remote_out_ao9,	2),
	GROUP(pwm_ao_a,		3),
	GROUP(pwm_ao_a_hiz,	2),
	GROUP(pwm_ao_b,		3),
	GROUP(pwm_ao_c_4,	3),
	GROUP(pwm_ao_c_hiz_4,	4),
	GROUP(pwm_ao_c_6,	3),
	GROUP(pwm_ao_c_hiz_7,	3),
	GROUP(pwm_ao_d_5,	3),
	GROUP(pwm_ao_d_10,	3),
	GROUP(pwm_ao_d_e,	3),
	GROUP(jtag_a_tdi,	1),
	GROUP(jtag_a_tdo,	1),
	GROUP(jtag_a_clk,	1),
	GROUP(jtag_a_tms,	1),
	GROUP(cec_ao_a,		1),
	GROUP(cec_ao_b,		2),
	GROUP(spdif_out_ao,	2),
	GROUP(spdif_in_ao,	2),
	GROUP(pwm_a_e2,		3),
	GROUP(clk12_24_ao,	1),
	GROUP(clk25_ee,		2),
	GROUP(gen_clk_ee_ao,	4),
	GROUP(gen_clk_ao,	5),
};

static const char * const gpio_periphs_groups[] = {
	"GPIOZ_0", "GPIOZ_1", "GPIOZ_2", "GPIOZ_3", "GPIOZ_4",
	"GPIOZ_5", "GPIOZ_6", "GPIOZ_7", "GPIOZ_8", "GPIOZ_9",
	"GPIOZ_10",

	"GPIOH_0", "GPIOH_1", "GPIOH_2", "GPIOH_3", "GPIOH_4",
	"GPIOH_5", "GPIOH_6", "GPIOH_7", "GPIOH_8", "GPIOH_9",
	"GPIOH_10", "GPIOH_11", "GPIOH_12", "GPIOH_13", "GPIOH_14",
	"GPIOH_15", "GPIOH_16", "GPIOH_17", "GPIOH_18", "GPIOH_19",
	"GPIOH_20", "GPIOH_21", "GPIOH_22",

	"BOOT_0", "BOOT_1", "BOOT_2", "BOOT_3", "BOOT_4",
	"BOOT_5", "BOOT_6", "BOOT_7", "BOOT_8", "BOOT_9",
	"BOOT_10", "BOOT_11", "BOOT_12", "BOOT_13",

	"GPIOC_0", "GPIOC_1", "GPIOC_2", "GPIOC_3", "GPIOC_4",
	"GPIOC_5", "GPIOC_6", "GPIOC_7", "GPIOC_8", "GPIOC_9",
	"GPIOC_10", "GPIOC_11", "GPIOC_12", "GPIOC_13", "GPIOC_14",

	"GPIOW_0", "GPIOW_1", "GPIOW_2", "GPIOW_3", "GPIOW_4",
	"GPIOW_5", "GPIOW_6", "GPIOW_7", "GPIOW_8", "GPIOW_9",
	"GPIOW_10", "GPIOW_11",

	"GPIODV_0", "GPIODV_1", "GPIODV_2", "GPIODV_3", "GPIODV_4",
	"GPIODV_5", "GPIODV_6", "GPIODV_7", "GPIODV_8", "GPIODV_9",
	"GPIODV_10", "GPIODV_11",

};

static const char * const emmc_groups[] = {
	"emmc_nand_d0", "emmc_nand_d1", "emmc_nand_d2",
	"emmc_nand_d3", "emmc_nand_d4", "emmc_nand_d5",
	"emmc_nand_d6", "emmc_nand_d7",
	"emmc_clk", "emmc_cmd", "emmc_nand_ds",
};

static const char * const nand_groups[] = {
	"emmc_nand_d0", "emmc_nand_d1", "emmc_nand_d2",
	"emmc_nand_d3", "emmc_nand_d4", "emmc_nand_d5",
	"emmc_nand_d6", "emmc_nand_d7",
	"nand_ce0", "nand_ale", "nand_cle",
	"nand_wen_clk", "nand_ren_wr", "emmc_nand_ds",
};

static const char * const nor_groups[] = {
	"nor_d", "nor_q", "nor_c", "nor_cs",
	"nor_hold", "nor_wp",
};

static const char * const sdcard_groups[] = {
	"sdcard_d0", "sdcard_d1", "sdcard_d2", "sdcard_d3",
	"sdcard_clk", "sdcard_cmd",
};

static const char * const spi0_groups[] = {
	"spi0_mosi_c", "spi0_miso_c", "spi0_ss0_c", "spi0_clk_c",
	"spi0_mosi_h", "spi0_miso_h", "spi0_ss0_h", "spi0_clk_h",
	"spi0_ss1", "spi0_ss2",
};

static const char * const spi1_groups[] = {
	"spi1_mosi_dv", "spi1_miso_dv", "spi1_ss0_dv", "spi1_clk_dv",
	"spi1_ss1_dv",
	"spi1_mosi_c", "spi1_miso_c", "spi1_ss0_c", "spi1_clk_c",
	"spi1_ss1_c", "spi1_ss2",
};

static const char * const i2c0_groups[] = {
	"i2c0_sda_c", "i2c0_sck_c",
	"i2c0_sda_dv", "i2c0_sck_dv",
};

static const char * const i2c1_groups[] = {
	"i2c1_sda_z", "i2c1_sck_z",
	"i2c1_sda_h", "i2c1_sck_h",
};

static const char * const i2c2_groups[] = {
	"i2c2_sda_h", "i2c2_sck_h",
	"i2c2_sda_z", "i2c2_sck_z",
};

static const char * const i2c3_groups[] = {
	"i2c3_sda_h1", "i2c3_sck_h0",
	"i2c3_sda_h20", "i2c3_sck_h19",
	"i2c3_sda_dv", "i2c3_sck_dv",
	"i2c3_sda_c", "i2c3_sck_c",
};

static const char * const uart_a_groups[] = {
	"uart_a_tx", "uart_a_rx", "uart_a_cts", "uart_a_rts",
};

static const char * const uart_b_groups[] = {
	"uart_b_tx", "uart_b_rx", "uart_b_cts", "uart_b_rts",
};

static const char * const uart_c_groups[] = {
	"uart_c_tx", "uart_c_rx", "uart_c_cts", "uart_c_rts",
};

static const char * const uart_ao_a_ee_groups[] = {
	"uart_ao_a_rx_c", "uart_ao_a_tx_c",
	"uart_ao_a_rx_w3", "uart_ao_a_tx_w2",
	"uart_ao_a_rx_w7", "uart_ao_a_tx_w6",
	"uart_ao_a_rx_w11", "uart_ao_a_tx_w10",
};

static const char * const iso7816_groups[] = {
	"iso7816_clk", "iso7816_data",
};

static const char * const eth_groups[] = {
	"eth_rxd2_rgmii", "eth_rxd3_rgmii", "eth_rgmii_tx_clk",
	"eth_txd2_rgmii", "eth_txd3_rgmii", "eth_rgmii_rx_clk",
	"eth_txd0", "eth_txd1", "eth_txen", "eth_mdc",
	"eth_rxd0", "eth_rxd1", "eth_rx_dv", "eth_mdio",
	"eth_link_led", "eth_act_led",
};

static const char * const pwm_a_groups[] = {
	"pwm_a",
};

static const char * const pwm_b_groups[] = {
	"pwm_b_c", "pwm_b_z",
};

static const char * const pwm_c_groups[] = {
	"pwm_c_dv", "pwm_c_h", "pwm_c_z",
};

static const char * const pwm_d_groups[] = {
	"pwm_d_dv", "pwm_d_z",
};

static const char * const pwm_e_groups[] = {
	"pwm_e_dv", "pwm_e_z",
};

static const char * const pwm_f_groups[] = {
	"pwm_f_dv", "pwm_f_z",
};

static const char * const pwm_vs_groups[] = {
	"pwm_vs_z5", "pwm_vs_z6",
};

static const char * const jtag_b_groups[] = {
	"jtag_b_tdi", "jtag_b_tdo", "jtag_b_clk", "jtag_b_tms",
};

static const char * const tsout_groups[] = {
	"tsout_clk", "tsout_sop", "tsout_valid",
	"tsout_dout0", "tsout_dout1", "tsout_dout2",
	"tsout_dout3", "tsout_dout4", "tsout_dout5",
	"tsout_dout6", "tsout_dout7",
};

static const char * const tcon_groups[] = {
	"tcon_0", "tcon_1", "tcon_2", "tcon_3",
	"tcon_4", "tcon_5", "tcon_6", "tcon_7",
	"tcon_8", "tcon_9", "tcon_10", "tcon_11",
	"tcon_12", "tcon_13", "tcon_14", "tcon_15",
	"tcon_lock", "tcon_spi_mo", "tcon_spi_mi",
	"tcon_spi_clk", "tcon_spi_ss", "tcon_slv_sda",
	"tcon_slv_sck", "tcon_aging", "tcon_flicker",
	"tcon_sfc_h2", "tcon_sfc_h16",
};

static const char * const bt656_groups[] = {
	"bt656_a_vs", "bt656_a_hs", "bt656_a_clk",
	"bt656_a_din0", "bt656_a_din1", "bt656_a_din2",
	"bt656_a_din3", "bt656_a_din4", "bt656_a_din5",
	"bt656_a_din6", "bt656_a_din7",
};

static const char * const tsin_a_groups[] = {
	"tsin_a_valid", "tsin_a_sop", "tsin_a_clk",
	"tsin_a_din0", "tsin_a_din1", "tsin_a_din2", "tsin_a_din3",
	"tsin_a_din4", "tsin_a_din5", "tsin_a_din6", "tsin_a_din7",
};

static const char * const tsin_b_groups[] = {
	"tsin_b_valid", "tsin_b_sop", "tsin_b_din0",
	"tsin_b_clk",
};

static const char * const tsin_c_groups[] = {
	"tsin_c_valid", "tsin_c_sop", "tsin_c_din0",
	"tsin_c_clk",
};

static const char * const hdmirx_a_groups[] = {
	"hdmirx_a_hpd", "hdmirx_a_det",
	"hdmirx_a_sda", "hdmirx_a_sck",
};

static const char * const hdmirx_b_groups[] = {
	"hdmirx_b_hpd", "hdmirx_b_det",
	"hdmirx_b_sda", "hdmirx_b_sck",
};

static const char * const hdmirx_c_groups[] = {
	"hdmirx_c_hpd", "hdmirx_c_det",
	"hdmirx_c_sda", "hdmirx_c_sck",
};

static const char * const pdm_groups[] = {
	"pdm_din0_c", "pdm_din1_c", "pdm_din2_c", "pdm_dclk_c",
	"pdm_din3",
	"pdm_din0_z", "pdm_din1_z", "pdm_din2_z", "pdm_dclk_z",
};

static const char * const spdif_in_groups[] = {
	"spdif_in",
};

static const char * const spdif_out_groups[] = {
	"spdif_out_h", "spdif_out_z", "spdif_out_dv4",
	"spdif_out_dv11",
};

static const char * const mclk0_groups[] = {
	"mclk0_h", "mclk0_z",
};

static const char * const mclk1_groups[] = {
	"mclk1_c", "mclk1_z",
};

static const char * const tdma_in_groups[] = {
	"tdma_slv_sclk", "tdma_slv_fs",
	"tdma_din0_z", "tdma_din1_z", "tdma_din2_z", "tdma_din3_z",
	"tdma_din0_h", "tdma_din1_h", "tdma_din2_h", "tdma_din3_h",
};

static const char * const tdma_out_groups[] = {
	"tdma_sclk_z", "tdma_fs_z",
	"tdma_dout0_z", "tdma_dout1_z", "tdma_dout2_z", "tdma_dout3_z",
	"tdma_sclk_h", "tdma_fs_h",
	"tdma_dout0_h", "tdma_dout1_h", "tdma_dout2_h", "tdma_dout3_h",
};

static const char * const tdmb_in_groups[] = {
	"tdmb_slv_sclk", "tdmb_slv_fs",
	"tdmb_din0_c6", "tdmb_din1_c7", "tdmb_din2", "tdmb_din3",
	"tdmb_din0_c12", "tdmb_din1_c11",
};

static const char * const tdmb_out_groups[] = {
	"tdmb_sclk_c4", "tdmb_fs_c5",
	"tdmb_dout0_c0", "tdmb_dout1_c1", "tdmb_dout2", "tdmb_dout3",
	"tdmb_sclk_c13", "tdmb_fs_c14",
	"tdmb_dout0_c12", "tdmb_dout1_c11",
};

static const char * const tdmc_in_groups[] = {
	"tdmc_slv_sclk", "tdmc_slv_fs",
	"tdmc_din0", "tdmc_din1", "tdmc_din2", "tdmc_din3",
};

static const char * const tdmc_out_groups[] = {
	"tdmc_sclk", "tdmc_fs",
	"tdmc_dout0", "tdmc_dout1", "tdmc_dout2", "tdmc_dout3",
};

static const char * const clk12_24_ee_groups[] = {
	"clk12_24",
};

static const char * const gen_clk_ee_groups[] = {
	"gen_clk_ee_h17", "gen_clk_ee_h21",
};

static const char * const vx1_groups[] = {
	"vx1_htpdn", "vx1_lockn",
};

static const char * const atv_groups[] = {
	"atv_if_agc_dv", "atv_if_agc_c",
};

static const char * const dtv_groups[] = {
	"dtv_if_agc_dv2", "dtv_if_agc_dv3", "dtv_rf_agc_dv3",
	"dtv_if_agc_c6", "dtv_if_agc_c7", "dtv_rf_agc_c7",
};

static const char * const gpio_aobus_groups[] = {
	"GPIOAO_0", "GPIOAO_1", "GPIOAO_2", "GPIOAO_3", "GPIOAO_4",
	"GPIOAO_5", "GPIOAO_6", "GPIOAO_7", "GPIOAO_8", "GPIOAO_9",
	"GPIOAO_10", "GPIOAO_11", "GPIOE_0", "GPIOE_1", "GPIOE_2",
	"GPIO_TEST_N",
};

static const char * const uart_ao_a_groups[] = {
	"uart_ao_a_tx", "uart_ao_a_rx",
	"uart_ao_a_cts", "uart_ao_a_rts",
};

static const char * const uart_ao_b_groups[] = {
	"uart_ao_b_tx_2", "uart_ao_b_rx_3",
	"uart_ao_b_tx_8", "uart_ao_b_rx_9",
	"uart_ao_b_cts", "uart_ao_b_rts",
};

static const char * const i2c_ao_groups[] = {
	"i2c_ao_sck_2", "i2c_ao_sda_3",
	"i2c_ao_sck_e", "i2c_ao_sda_e",
};

static const char * const i2c_ao_slave_groups[] = {
	"i2c_ao_slave_sck", "i2c_ao_slave_sda",
};

static const char * const remote_input_ao_groups[] = {
	"remote_input_ao",
};

static const char * const remote_out_ao_groups[] = {
	"remote_out_ao",
	"remote_out_ao9",
};

static const char * const pwm_ao_a_groups[] = {
	"pwm_ao_a", "pwm_ao_a_hiz",
};

static const char * const pwm_ao_b_groups[] = {
	"pwm_ao_b",
};

static const char * const pwm_ao_c_groups[] = {
	"pwm_ao_c_4", "pwm_ao_c_hiz_4",
	"pwm_ao_c_6", "pwm_ao_c_hiz_7",
};

static const char * const pwm_ao_d_groups[] = {
	"pwm_ao_d_5", "pwm_ao_d_10", "pwm_ao_d_e",
};

static const char * const jtag_a_groups[] = {
	"jtag_a_tdi", "jtag_a_tdo", "jtag_a_clk", "jtag_a_tms",
};

static const char * const cec_ao_groups[] = {
	"cec_ao_a", "cec_ao_b",
};

static const char * const spdif_out_ao_groups[] = {
	"spdif_out_ao",
};

static const char * const spdif_in_ao_groups[] = {
	"spdif_in_ao",
};

static const char * const pwm_a_e2_groups[] = {
	"pwm_a_e2",
};

static const char * const clk25_ee_groups[] = {
	"clk25_ee",
};

static const char * const clk12_24_ao_groups[] = {
	"clk12_24_ao",
};

static const char * const gen_clk_ee_ao_groups[] = {
	"gen_clk_ee_ao",
};

static const char * const gen_clk_ao_groups[] = {
	"gen_clk_ao",
};

static struct meson_pmx_func meson_tl1_periphs_functions[] = {
	FUNCTION(gpio_periphs),
	FUNCTION(emmc),
	FUNCTION(nor),
	FUNCTION(spi0),
	FUNCTION(spi1),
	FUNCTION(nand),
	FUNCTION(sdcard),
	FUNCTION(i2c0),
	FUNCTION(i2c1),
	FUNCTION(i2c2),
	FUNCTION(i2c3),
	FUNCTION(uart_a),
	FUNCTION(uart_b),
	FUNCTION(uart_c),
	FUNCTION(uart_ao_a_ee),
	FUNCTION(iso7816),
	FUNCTION(eth),
	FUNCTION(pwm_a),
	FUNCTION(pwm_b),
	FUNCTION(pwm_c),
	FUNCTION(pwm_d),
	FUNCTION(pwm_e),
	FUNCTION(pwm_f),
	FUNCTION(pwm_vs),
	FUNCTION(jtag_b),
	FUNCTION(tsout),
	FUNCTION(tcon),
	FUNCTION(bt656),
	FUNCTION(tsin_a),
	FUNCTION(tsin_b),
	FUNCTION(tsin_c),
	FUNCTION(hdmirx_a),
	FUNCTION(hdmirx_b),
	FUNCTION(hdmirx_c),
	FUNCTION(pdm),
	FUNCTION(spdif_in),
	FUNCTION(spdif_out),
	FUNCTION(mclk0),
	FUNCTION(mclk1),
	FUNCTION(tdma_in),
	FUNCTION(tdma_out),
	FUNCTION(tdmb_in),
	FUNCTION(tdmb_out),
	FUNCTION(tdmc_in),
	FUNCTION(tdmc_out),
	FUNCTION(clk12_24_ee),
	FUNCTION(gen_clk_ee),
	FUNCTION(vx1),
	FUNCTION(atv),
	FUNCTION(dtv),
};

static struct meson_pmx_func meson_tl1_aobus_functions[] = {
	FUNCTION(gpio_aobus),
	FUNCTION(uart_ao_a),
	FUNCTION(uart_ao_b),
	FUNCTION(i2c_ao),
	FUNCTION(i2c_ao_slave),
	FUNCTION(remote_input_ao),
	FUNCTION(remote_out_ao),
	FUNCTION(pwm_ao_a),
	FUNCTION(pwm_ao_b),
	FUNCTION(pwm_ao_c),
	FUNCTION(pwm_ao_d),
	FUNCTION(jtag_a),
	FUNCTION(cec_ao),
	FUNCTION(spdif_out_ao),
	FUNCTION(spdif_in_ao),
	FUNCTION(pwm_a_e2),
	FUNCTION(clk12_24_ao),
	FUNCTION(clk25_ee),
	FUNCTION(gen_clk_ee_ao),
	FUNCTION(gen_clk_ao),
};

static struct meson_bank meson_tl1_periphs_banks[] = {
	/* name  first  last  irq  pullen  pull  dir  out  in */
	BANK("Z",    GPIOZ_0,    GPIOZ_10, 12,
	1,  0,  1,  0,  3,  0,  4, 0,  5, 0),
	BANK("H",    GPIOH_0,    GPIOH_22, 23,
	2,  0,  2,  0,  6,  0,  7,  0,  8,  0),
	BANK("BOOT", BOOT_0,     BOOT_13,  46,
	0,  0,  0,  0,  0,  0,  1, 0,  2, 0),
	BANK("C",    GPIOC_0,    GPIOC_14,  60,
	3,  0,  3,  0,  9,  0,  10, 0,  11, 0),
	BANK("W",    GPIOW_0,    GPIOW_11,  75,
	4,  0,  4,  0,  12,  0,  13,  0,  14,  0),
	BANK("DV",    GPIODV_0,    GPIODV_11,   87,
	5,  0,  5,  0,  16,  0,  17,  0,  18,  0),
};

static struct meson_bank meson_tl1_aobus_banks[] = {
	/* name  first  last  irq  pullen  pull  dir  out  in  */
	BANK("AO",   GPIOAO_0,  GPIOAO_11,  0,
	3,  0,  2, 0,  0,  0,  4, 0,  1,  0),
	BANK("E",   GPIOE_0,  GPIOE_2,   99,
	3,  16,  2, 16,  0,  16,  4, 16,  1,  16),
	BANK("TESTN",  GPIO_TEST_N,  GPIO_TEST_N,  -1,
	3,  31,  2, 31,  0,  31,  4, 31,  1,  31),
};

static struct meson_pmx_bank meson_tl1_periphs_pmx_banks[] = {
	/*	 name	 first		lask	   reg	offset  */
	BANK_PMX("Z",    GPIOZ_0, GPIOZ_10, 0x2, 0),
	BANK_PMX("H",    GPIOH_0, GPIOH_22, 0x7, 0),
	BANK_PMX("BOOT", BOOT_0,  BOOT_13,  0x0, 0),
	BANK_PMX("C",    GPIOC_0, GPIOC_14, 0x4, 0),
	BANK_PMX("W",    GPIOW_0, GPIOW_11, 0xa, 0),
	BANK_PMX("DV",    GPIODV_0, GPIODV_11, 0xc, 0),
};

static struct meson_axg_pmx_data meson_tl1_periphs_pmx_banks_data = {
	.pmx_banks	= meson_tl1_periphs_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_tl1_periphs_pmx_banks),
};

static struct meson_drive_bank meson_tl1_periphs_drive_banks[] = {
	/*	 name	 first		lask	   reg	offset  */
	BANK_DRIVE("Z",    GPIOZ_0, GPIOZ_10, 0x1, 0),
	BANK_DRIVE("H",    GPIOH_0, GPIOH_22, 0x2, 0),
	BANK_DRIVE("BOOT", BOOT_0,  BOOT_13,  0x0, 0),
	BANK_DRIVE("C",    GPIOC_0, GPIOC_14, 0x4, 0),
	BANK_DRIVE("W",    GPIOW_0, GPIOW_11, 0x5, 0),
	BANK_DRIVE("DV",    GPIODV_0, GPIODV_11, 0x6, 0),
};

static struct meson_drive_data meson_tl1_periphs_drive_data = {
	.drive_banks	= meson_tl1_periphs_drive_banks,
	.num_drive_banks = ARRAY_SIZE(meson_tl1_periphs_drive_banks),
};

static struct meson_pmx_bank meson_tl1_aobus_pmx_banks[] = {
	BANK_PMX("AO", GPIOAO_0, GPIOAO_11, 0x0, 0),
	BANK_PMX("E", GPIOE_0, GPIOE_2, 0x1, 16),
	BANK_PMX("TESTN", GPIO_TEST_N, GPIO_TEST_N, 0x1, 28),
};

static struct meson_axg_pmx_data meson_tl1_aobus_pmx_banks_data = {
	.pmx_banks	= meson_tl1_aobus_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_tl1_aobus_pmx_banks),
};

static struct meson_drive_bank meson_tl1_aobus_drive_banks[] = {
	BANK_DRIVE("AO", GPIOAO_0, GPIOAO_11, 0x0, 0),
	BANK_DRIVE("E", GPIOE_0, GPIOE_2, 0x1, 0),
	BANK_DRIVE("TESTN", GPIO_TEST_N, GPIO_TEST_N, 0x1, 28),
};

static struct meson_drive_data meson_tl1_aobus_drive_data = {
	.drive_banks     = meson_tl1_aobus_drive_banks,
	.num_drive_banks = ARRAY_SIZE(meson_tl1_aobus_drive_banks),
};

static int meson_tl1_aobus_init(struct meson_pinctrl *pc)
{
	struct arm_smccc_res res;
	/*set TEST_N to output*/
	arm_smccc_smc(CMD_TEST_N_DIR, TEST_N_OUTPUT, 0, 0, 0, 0, 0, 0, &res);

	return 0;
}

static struct meson_pinctrl_data meson_tl1_periphs_pinctrl_data = {
	.name		= "periphs-banks",
	.init		= NULL,
	.pins		= meson_tl1_periphs_pins,
	.groups		= meson_tl1_periphs_groups,
	.funcs		= meson_tl1_periphs_functions,
	.banks		= meson_tl1_periphs_banks,
	.num_pins	= ARRAY_SIZE(meson_tl1_periphs_pins),
	.num_groups	= ARRAY_SIZE(meson_tl1_periphs_groups),
	.num_funcs	= ARRAY_SIZE(meson_tl1_periphs_functions),
	.num_banks	= ARRAY_SIZE(meson_tl1_periphs_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_tl1_periphs_pmx_banks_data,
	.pcf_ops	= &meson_g12a_pinconf_ops,
	.pcf_drv_data	= &meson_tl1_periphs_drive_data,
};

static struct meson_pinctrl_data meson_tl1_aobus_pinctrl_data = {
	.name		= "aobus-banks",
	.init		= meson_tl1_aobus_init,
	.pins		= meson_tl1_aobus_pins,
	.groups		= meson_tl1_aobus_groups,
	.funcs		= meson_tl1_aobus_functions,
	.banks		= meson_tl1_aobus_banks,
	.num_pins	= ARRAY_SIZE(meson_tl1_aobus_pins),
	.num_groups	= ARRAY_SIZE(meson_tl1_aobus_groups),
	.num_funcs	= ARRAY_SIZE(meson_tl1_aobus_functions),
	.num_banks	= ARRAY_SIZE(meson_tl1_aobus_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_tl1_aobus_pmx_banks_data,
	.pcf_ops	= &meson_g12a_pinconf_ops,
	.pcf_drv_data	= &meson_tl1_aobus_drive_data,
};

static const struct of_device_id meson_tl1_pinctrl_dt_match[] = {
	{
		.compatible = "amlogic,meson-tl1-periphs-pinctrl",
		.data = &meson_tl1_periphs_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-tl1-aobus-pinctrl",
		.data = &meson_tl1_aobus_pinctrl_data,
	},
	{ },
};

static struct platform_driver meson_tl1_pinctrl_driver = {
	.probe  = meson_pinctrl_probe,
	.driver = {
		.name	= "meson-tl1-pinctrl",
		.of_match_table = meson_tl1_pinctrl_dt_match,
	},
};

static int __init tl1_pmx_init(void)
{
	return platform_driver_register(&meson_tl1_pinctrl_driver);
}

static void __exit tl1_pmx_exit(void)
{
	platform_driver_unregister(&meson_tl1_pinctrl_driver);
}

arch_initcall(tl1_pmx_init);
module_exit(tl1_pmx_exit);
MODULE_DESCRIPTION("tl1 pin control driver");
MODULE_LICENSE("GPL v2");

