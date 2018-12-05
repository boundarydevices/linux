/*
 * Pin controller and GPIO driver for Amlogic Meson G12A SoC.
 *
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 * Author: Xingyu Chen <xingyu.chen@amlogic.com>
 *
 * SPDX-License-Identifier: (GPL-2.0+ or MIT)
 */

#include <dt-bindings/gpio/meson-g12a-gpio.h>
#include <linux/arm-smccc.h>
#include "pinctrl-meson.h"
#include "pinctrl-meson-axg-pmx.h"
#include "pinconf-meson-g12a.h"

static const struct pinctrl_pin_desc meson_g12a_periphs_pins[] = {
	MESON_PIN(GPIOV_0),
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
	MESON_PIN(GPIOC_0),
	MESON_PIN(GPIOC_1),
	MESON_PIN(GPIOC_2),
	MESON_PIN(GPIOC_3),
	MESON_PIN(GPIOC_4),
	MESON_PIN(GPIOC_5),
	MESON_PIN(GPIOC_6),
	MESON_PIN(GPIOC_7),
	MESON_PIN(GPIOA_0),
	MESON_PIN(GPIOA_1),
	MESON_PIN(GPIOA_2),
	MESON_PIN(GPIOA_3),
	MESON_PIN(GPIOA_4),
	MESON_PIN(GPIOA_5),
	MESON_PIN(GPIOA_6),
	MESON_PIN(GPIOA_7),
	MESON_PIN(GPIOA_8),
	MESON_PIN(GPIOA_9),
	MESON_PIN(GPIOA_10),
	MESON_PIN(GPIOA_11),
	MESON_PIN(GPIOA_12),
	MESON_PIN(GPIOA_13),
	MESON_PIN(GPIOA_14),
	MESON_PIN(GPIOA_15),
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
	MESON_PIN(GPIOX_19),
};

static const struct pinctrl_pin_desc meson_g12a_aobus_pins[] = {
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
static const unsigned int emmc_nand_ds_pins[] = {BOOT_13};

/* nand */
static const unsigned int nand_wen_clk_pins[] = {BOOT_8};
static const unsigned int nand_ale_pins[] = {BOOT_9};
static const unsigned int nand_cle_pins[] = {BOOT_10};
static const unsigned int nand_ce0_pins[] = {BOOT_11};
static const unsigned int nand_ren_wr_pins[] = {BOOT_12};
static const unsigned int nand_rb0_pins[] = {BOOT_14};
static const unsigned int nand_ce1_pins[] = {BOOT_15};

/* nor */
static const unsigned int nor_hold_pins[] = {BOOT_3};
static const unsigned int nor_d_pins[] = {BOOT_4};
static const unsigned int nor_q_pins[] = {BOOT_5};
static const unsigned int nor_c_pins[] = {BOOT_6};
static const unsigned int nor_wp_pins[] = {BOOT_7};
static const unsigned int nor_cs_pins[] = {BOOT_14};

/* sdio */
static const unsigned int sdio_d0_pins[] = {GPIOX_0};
static const unsigned int sdio_d1_pins[] = {GPIOX_1};
static const unsigned int sdio_d2_pins[] = {GPIOX_2};
static const unsigned int sdio_d3_pins[] = {GPIOX_3};
static const unsigned int sdio_clk_pins[] = {GPIOX_4};
static const unsigned int sdio_cmd_pins[] = {GPIOX_5};
static const unsigned int sdio_dummy_pins[] = {GPIOV_0};

/* sdcard */
static const unsigned int sdcard_d0_c_pins[] = {GPIOC_0};
static const unsigned int sdcard_d1_c_pins[] = {GPIOC_1};
static const unsigned int sdcard_d2_c_pins[] = {GPIOC_2};
static const unsigned int sdcard_d3_c_pins[] = {GPIOC_3};
static const unsigned int sdcard_clk_c_pins[] = {GPIOC_4};
static const unsigned int sdcard_cmd_c_pins[] = {GPIOC_5};

static const unsigned int sdcard_d0_z_pins[] = {GPIOZ_2};
static const unsigned int sdcard_d1_z_pins[] = {GPIOZ_3};
static const unsigned int sdcard_d2_z_pins[] = {GPIOZ_4};
static const unsigned int sdcard_d3_z_pins[] = {GPIOZ_5};
static const unsigned int sdcard_clk_z_pins[] = {GPIOZ_6};
static const unsigned int sdcard_cmd_z_pins[] = {GPIOZ_7};

/* spi0 */
static const unsigned int spi0_mosi_c_pins[] = {GPIOC_0};
static const unsigned int spi0_miso_c_pins[] = {GPIOC_1};
static const unsigned int spi0_ss0_c_pins[] = {GPIOC_2};
static const unsigned int spi0_clk_c_pins[] = {GPIOC_3};

static const unsigned int spi0_mosi_x_pins[] = {GPIOX_8};
static const unsigned int spi0_miso_x_pins[] = {GPIOX_9};
static const unsigned int spi0_ss0_x_pins[] = {GPIOX_10};
static const unsigned int spi0_clk_x_pins[] = {GPIOX_11};

/* spi1 */
static const unsigned int spi1_mosi_pins[] = {GPIOH_4};
static const unsigned int spi1_miso_pins[] = {GPIOH_5};
static const unsigned int spi1_ss0_pins[] = {GPIOH_6};
static const unsigned int spi1_clk_pins[] = {GPIOH_7};

/* i2c0 */
static const unsigned int i2c0_sda_c_pins[] = {GPIOC_5};
static const unsigned int i2c0_sck_c_pins[] = {GPIOC_6};

static const unsigned int i2c0_sda_z0_pins[] = {GPIOZ_0};
static const unsigned int i2c0_sck_z1_pins[] = {GPIOZ_1};

static const unsigned int i2c0_sda_z7_pins[] = {GPIOZ_7};
static const unsigned int i2c0_sck_z8_pins[] = {GPIOZ_8};

/* i2c1 */
static const unsigned int i2c1_sda_x_pins[] = {GPIOX_10};
static const unsigned int i2c1_sck_x_pins[] = {GPIOX_11};

static const unsigned int i2c1_sda_h2_pins[] = {GPIOH_2};
static const unsigned int i2c1_sck_h3_pins[] = {GPIOH_3};

static const unsigned int i2c1_sda_h6_pins[] = {GPIOH_6};
static const unsigned int i2c1_sck_h7_pins[] = {GPIOH_7};

/* i2c2 */
static const unsigned int i2c2_sda_x_pins[] = {GPIOX_17};
static const unsigned int i2c2_sck_x_pins[] = {GPIOX_18};

static const unsigned int i2c2_sda_z_pins[] = {GPIOZ_14};
static const unsigned int i2c2_sck_z_pins[] = {GPIOZ_15};

/* i2c3 */
static const unsigned int i2c3_sda_h_pins[] = {GPIOH_0};
static const unsigned int i2c3_sck_h_pins[] = {GPIOH_1};

static const unsigned int i2c3_sda_a_pins[] = {GPIOA_14};
static const unsigned int i2c3_sck_a_pins[] = {GPIOA_15};

/* uart_a */
static const unsigned int uart_tx_a_pins[] = {GPIOX_12};
static const unsigned int uart_rx_a_pins[] = {GPIOX_13};
static const unsigned int uart_cts_a_pins[] = {GPIOX_14};
static const unsigned int uart_rts_a_pins[] = {GPIOX_15};

/* uart_b */
static const unsigned int uart_tx_b_pins[] = {GPIOX_6};
static const unsigned int uart_rx_b_pins[] = {GPIOX_7};

/* uart_c */
static const unsigned int uart_rts_c_pins[] = {GPIOH_4};
static const unsigned int uart_cts_c_pins[] = {GPIOH_5};
static const unsigned int uart_rx_c_pins[] = {GPIOH_6};
static const unsigned int uart_tx_c_pins[] = {GPIOH_7};

/* uart_ao_a_ee */
static const unsigned int uart_ao_rx_a_c2_pins[] = {GPIOC_2};
static const unsigned int uart_ao_tx_a_c3_pins[] = {GPIOC_3};

/* iso7816 */
static const unsigned int iso7816_clk_c_pins[] = {GPIOC_5};
static const unsigned int iso7816_data_c_pins[] = {GPIOC_6};

static const unsigned int iso7816_clk_x_pins[] = {GPIOX_8};
static const unsigned int iso7816_data_x_pins[] = {GPIOX_9};

static const unsigned int iso7816_clk_h_pins[] = {GPIOH_6};
static const unsigned int iso7816_data_h_pins[] = {GPIOH_7};

static const unsigned int iso7816_clk_z_pins[] = {GPIOZ_0};
static const unsigned int iso7816_data_z_pins[] = {GPIOZ_1};

/* eth */
static const unsigned int eth_mdio_pins[] = {GPIOZ_0};
static const unsigned int eth_mdc_pins[] = {GPIOZ_1};
static const unsigned int eth_rgmii_rx_clk_pins[] = {GPIOZ_2};
static const unsigned int eth_rx_dv_pins[] = {GPIOZ_3};
static const unsigned int eth_rxd0_pins[] = {GPIOZ_4};
static const unsigned int eth_rxd1_pins[] = {GPIOZ_5};
static const unsigned int eth_rxd2_rgmii_pins[] = {GPIOZ_6};
static const unsigned int eth_rxd3_rgmii_pins[] = {GPIOZ_7};
static const unsigned int eth_rgmii_tx_clk_pins[] = {GPIOZ_8};
static const unsigned int eth_txen_pins[] = {GPIOZ_9};
static const unsigned int eth_txd0_pins[] = {GPIOZ_10};
static const unsigned int eth_txd1_pins[] = {GPIOZ_11};
static const unsigned int eth_txd2_rgmii_pins[] = {GPIOZ_12};
static const unsigned int eth_txd3_rgmii_pins[] = {GPIOZ_13};
static const unsigned int eth_link_led_pins[] = {GPIOZ_14};
static const unsigned int eth_act_led_pins[] = {GPIOZ_15};

/* pwm_a */
static const unsigned int pwm_a_pins[] = {GPIOX_6};

/* pwm_b */
static const unsigned int pwm_b_x7_pins[] = {GPIOX_7};
static const unsigned int pwm_b_x19_pins[] = {GPIOX_19};

/* pwm_c */
static const unsigned int pwm_c_c4_pins[] = {GPIOC_4};
static const unsigned int pwm_c_x5_pins[] = {GPIOX_5};
static const unsigned int pwm_c_x8_pins[] = {GPIOX_8};

/* pwm_d */
static const unsigned int pwm_d_x3_pins[] = {GPIOX_3};
static const unsigned int pwm_d_x6_pins[] = {GPIOX_6};

/* pwm_e */
static const unsigned int pwm_e_pins[] = {GPIOX_16};

/* pwm_f */
static const unsigned int pwm_f_x_pins[] = {GPIOX_7};
static const unsigned int pwm_f_h_pins[] = {GPIOH_5};

/* cec_ao_ee */
static const unsigned int cec_ao_a_ee_pins[] = {GPIOH_3};
static const unsigned int cec_ao_b_ee_pins[] = {GPIOH_3};

/* jtag_b */
static const unsigned int jtag_b_tdo_pins[] = {GPIOC_0};
static const unsigned int jtag_b_tdi_pins[] = {GPIOC_1};
static const unsigned int jtag_b_clk_pins[] = {GPIOC_4};
static const unsigned int jtag_b_tms_pins[] = {GPIOC_5};

/* bt656 */
static const unsigned int bt656_a_vs_pins[] = {GPIOZ_0};
static const unsigned int bt656_a_hs_pins[] = {GPIOZ_1};
static const unsigned int bt656_a_clk_pins[] = {GPIOZ_3};
static const unsigned int bt656_a_din0_pins[] = {GPIOZ_4};
static const unsigned int bt656_a_din1_pins[] = {GPIOZ_5};
static const unsigned int bt656_a_din2_pins[] = {GPIOZ_6};
static const unsigned int bt656_a_din3_pins[] = {GPIOZ_7};
static const unsigned int bt656_a_din4_pins[] = {GPIOZ_8};
static const unsigned int bt656_a_din5_pins[] = {GPIOZ_9};
static const unsigned int bt656_a_din6_pins[] = {GPIOZ_10};
static const unsigned int bt656_a_din7_pins[] = {GPIOZ_11};

/* tsin_a */
static const unsigned int tsin_a_valid_pins[] = {GPIOX_2};
static const unsigned int tsin_a_sop_pins[] = {GPIOX_1};
static const unsigned int tsin_a_din0_pins[] = {GPIOX_0};
static const unsigned int tsin_a_clk_pins[] = {GPIOX_3};

/* tsin_b */
static const unsigned int tsin_b_valid_x_pins[] = {GPIOX_9};
static const unsigned int tsin_b_sop_x_pins[] = {GPIOX_8};
static const unsigned int tsin_b_din0_x_pins[] = {GPIOX_10};
static const unsigned int tsin_b_clk_x_pins[] = {GPIOX_11};

static const unsigned int tsin_b_valid_z_pins[] = {GPIOZ_2};
static const unsigned int tsin_b_sop_z_pins[] = {GPIOZ_3};
static const unsigned int tsin_b_din0_z_pins[] = {GPIOZ_4};
static const unsigned int tsin_b_clk_z_pins[] = {GPIOZ_5};

static const unsigned int tsin_b_fail_pins[] = {GPIOZ_6};
static const unsigned int tsin_b_din1_pins[] = {GPIOZ_7};
static const unsigned int tsin_b_din2_pins[] = {GPIOZ_8};
static const unsigned int tsin_b_din3_pins[] = {GPIOZ_9};
static const unsigned int tsin_b_din4_pins[] = {GPIOZ_10};
static const unsigned int tsin_b_din5_pins[] = {GPIOZ_11};
static const unsigned int tsin_b_din6_pins[] = {GPIOZ_12};
static const unsigned int tsin_b_din7_pins[] = {GPIOZ_13};

/* hdmitx */
static const unsigned int hdmitx_sda_pins[] = {GPIOH_0};
static const unsigned int hdmitx_sck_pins[] = {GPIOH_1};
static const unsigned int hdmitx_hpd_in_pins[] = {GPIOH_2};

/* pdm */
static const unsigned int pdm_din0_c_pins[] = {GPIOC_0};
static const unsigned int pdm_din1_c_pins[] = {GPIOC_1};
static const unsigned int pdm_din2_c_pins[] = {GPIOC_2};
static const unsigned int pdm_din3_c_pins[] = {GPIOC_3};
static const unsigned int pdm_dclk_c_pins[] = {GPIOC_4};

static const unsigned int pdm_din0_x_pins[] = {GPIOX_0};
static const unsigned int pdm_din1_x_pins[] = {GPIOX_1};
static const unsigned int pdm_din2_x_pins[] = {GPIOX_2};
static const unsigned int pdm_din3_x_pins[] = {GPIOX_3};
static const unsigned int pdm_dclk_x_pins[] = {GPIOX_4};

static const unsigned int pdm_din0_z_pins[] = {GPIOZ_2};
static const unsigned int pdm_din1_z_pins[] = {GPIOZ_3};
static const unsigned int pdm_din2_z_pins[] = {GPIOZ_4};
static const unsigned int pdm_din3_z_pins[] = {GPIOZ_5};
static const unsigned int pdm_dclk_z_pins[] = {GPIOZ_6};

static const unsigned int pdm_din0_a_pins[] = {GPIOA_8};
static const unsigned int pdm_din1_a_pins[] = {GPIOA_9};
static const unsigned int pdm_din2_a_pins[] = {GPIOA_6};
static const unsigned int pdm_din3_a_pins[] = {GPIOA_5};
static const unsigned int pdm_dclk_a_pins[] = {GPIOA_7};

/* spdif_in */
static const unsigned int spdif_in_h_pins[] = {GPIOH_5};
static const unsigned int spdif_in_a10_pins[] = {GPIOA_10};
static const unsigned int spdif_in_a12_pins[] = {GPIOA_12};

/* spdif_out */
static const unsigned int spdif_out_h_pins[] = {GPIOH_4};
static const unsigned int spdif_out_a11_pins[] = {GPIOA_11};
static const unsigned int spdif_out_a13_pins[] = {GPIOA_13};

/* mclk0 */
static const unsigned int mclk0_a_pins[] = {GPIOA_0};

/* mclk1 */
static const unsigned int mclk1_x_pins[] = {GPIOX_5};
static const unsigned int mclk1_z_pins[] = {GPIOZ_8};
static const unsigned int mclk1_a_pins[] = {GPIOA_11};

/* tdma_in */
static const unsigned int tdma_slv_sclk_pins[] = {GPIOX_11};
static const unsigned int tdma_slv_fs_pins[] = {GPIOX_10};
static const unsigned int tdma_din0_pins[] = {GPIOX_9};
static const unsigned int tdma_din1_pins[] = {GPIOX_8};

/* tdma_out */
static const unsigned int tdma_sclk_pins[] = {GPIOX_11};
static const unsigned int tdma_fs_pins[] = {GPIOX_10};
static const unsigned int tdma_dout0_pins[] = {GPIOX_9};
static const unsigned int tdma_dout1_pins[] = {GPIOX_8};

/* tdmb_in */
static const unsigned int tdmb_slv_sclk_pins[] = {GPIOA_1};
static const unsigned int tdmb_slv_fs_pins[] = {GPIOA_2};
static const unsigned int tdmb_din0_pins[] = {GPIOA_3};
static const unsigned int tdmb_din1_pins[] = {GPIOA_4};
static const unsigned int tdmb_din2_pins[] = {GPIOA_5};
static const unsigned int tdmb_din3_a_pins[] = {GPIOA_6};
static const unsigned int tdmb_din3_h_pins[] = {GPIOH_5};

/* tdmb_out */
static const unsigned int tdmb_sclk_pins[] = {GPIOA_1};
static const unsigned int tdmb_fs_pins[] = {GPIOA_2};
static const unsigned int tdmb_dout0_pins[] = {GPIOA_3};
static const unsigned int tdmb_dout1_pins[] = {GPIOA_4};
static const unsigned int tdmb_dout2_pins[] = {GPIOA_5};
static const unsigned int tdmb_dout3_a_pins[] = {GPIOA_6};
static const unsigned int tdmb_dout3_h_pins[] = {GPIOH_5};

/* tdmc_in */
static const unsigned int tdmc_slv_sclk_a_pins[] = {GPIOA_12};
static const unsigned int tdmc_slv_fs_a_pins[] = {GPIOA_13};
static const unsigned int tdmc_din0_a_pins[] = {GPIOA_10};
static const unsigned int tdmc_din1_a_pins[] = {GPIOA_9};
static const unsigned int tdmc_din2_a_pins[] = {GPIOA_8};
static const unsigned int tdmc_din3_a_pins[] = {GPIOA_7};

static const unsigned int tdmc_slv_sclk_z_pins[] = {GPIOZ_7};
static const unsigned int tdmc_slv_fs_z_pins[] = {GPIOZ_6};
static const unsigned int tdmc_din0_z_pins[] = {GPIOZ_2};
static const unsigned int tdmc_din1_z_pins[] = {GPIOZ_3};
static const unsigned int tdmc_din2_z_pins[] = {GPIOZ_4};
static const unsigned int tdmc_din3_z_pins[] = {GPIOZ_5};

/* tdmc_out */
static const unsigned int tdmc_sclk_a_pins[] = {GPIOA_12};
static const unsigned int tdmc_fs_a_pins[] = {GPIOA_13};
static const unsigned int tdmc_dout0_a_pins[] = {GPIOA_10};
static const unsigned int tdmc_dout1_a_pins[] = {GPIOA_9};
static const unsigned int tdmc_dout2_a_pins[] = {GPIOA_8};
static const unsigned int tdmc_dout3_a_pins[] = {GPIOA_7};

static const unsigned int tdmc_sclk_z_pins[] = {GPIOZ_7};
static const unsigned int tdmc_fs_z_pins[] = {GPIOZ_6};
static const unsigned int tdmc_dout0_z_pins[] = {GPIOZ_2};
static const unsigned int tdmc_dout1_z_pins[] = {GPIOZ_3};
static const unsigned int tdmc_dout2_z_pins[] = {GPIOZ_4};
static const unsigned int tdmc_dout3_z_pins[] = {GPIOZ_5};

/* gen_clk_ee */
static const unsigned int gen_clk_ee_x_pins[] = {GPIOX_19};
static const unsigned int gen_clk_ee_z_pins[] = {GPIOZ_13};

/* introduce extra ee pin-groups for G12B based on G12A */

/* ir_out */
static const unsigned int remote_out_h_pins[] = {GPIOH_6};
static const unsigned int remote_out_z_pins[] = {GPIOZ_10};

/* pwm_b */
static const unsigned int pwm_b_h_pins[] = {GPIOH_7};
static const unsigned int pwm_b_z0_pins[] = {GPIOZ_0};
static const unsigned int pwm_b_z13_pins[] = {GPIOZ_13};

/* pwm_c */
static const unsigned int pwm_c_z_pins[] = {GPIOZ_1};

/* pwm_d */
static const unsigned int pwm_d_z_pins[] = {GPIOZ_2};
static const unsigned int pwm_d_a4_pins[] = {GPIOA_4};

/* pwm_f */
static const unsigned int pwm_f_z_pins[] = {GPIOZ_12};
static const unsigned int pwm_f_a11_pins[] = {GPIOA_11};

/* gpio 12m/24m */
static const unsigned int clk12_24_z_pins[] = {GPIOZ_13};
static const unsigned int clk12_24_ao_pins[] = {GPIOAO_10};
static const unsigned int clk12_24_e_pins[] = {GPIOE_2};

static struct meson_pmx_group meson_g12a_periphs_groups[] = {
	GPIO_GROUP(GPIOV_0),
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
	GPIO_GROUP(GPIOC_0),
	GPIO_GROUP(GPIOC_1),
	GPIO_GROUP(GPIOC_2),
	GPIO_GROUP(GPIOC_3),
	GPIO_GROUP(GPIOC_4),
	GPIO_GROUP(GPIOC_5),
	GPIO_GROUP(GPIOC_6),
	GPIO_GROUP(GPIOC_7),
	GPIO_GROUP(GPIOA_0),
	GPIO_GROUP(GPIOA_1),
	GPIO_GROUP(GPIOA_2),
	GPIO_GROUP(GPIOA_3),
	GPIO_GROUP(GPIOA_4),
	GPIO_GROUP(GPIOA_5),
	GPIO_GROUP(GPIOA_6),
	GPIO_GROUP(GPIOA_7),
	GPIO_GROUP(GPIOA_8),
	GPIO_GROUP(GPIOA_9),
	GPIO_GROUP(GPIOA_10),
	GPIO_GROUP(GPIOA_11),
	GPIO_GROUP(GPIOA_12),
	GPIO_GROUP(GPIOA_13),
	GPIO_GROUP(GPIOA_14),
	GPIO_GROUP(GPIOA_15),
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
	GPIO_GROUP(GPIOX_19),

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
	GROUP(nand_rb0,		2),
	GROUP(nand_ce1,		2),
	GROUP(nor_hold,		3),
	GROUP(nor_d,		3),
	GROUP(nor_q,		3),
	GROUP(nor_c,		3),
	GROUP(nor_wp,		3),
	GROUP(nor_cs,		3),

	/* bank GPIOZ */
	GROUP(sdcard_d0_z,	5),
	GROUP(sdcard_d1_z,	5),
	GROUP(sdcard_d2_z,	5),
	GROUP(sdcard_d3_z,	5),
	GROUP(sdcard_clk_z,	5),
	GROUP(sdcard_cmd_z,	5),
	GROUP(i2c0_sda_z0,	4),
	GROUP(i2c0_sck_z1,	4),
	GROUP(i2c0_sda_z7,	7),
	GROUP(i2c0_sck_z8,	7),
	GROUP(i2c2_sda_z,	3),
	GROUP(i2c2_sck_z,	3),
	GROUP(iso7816_clk_z,	3),
	GROUP(iso7816_data_z,	3),
	GROUP(eth_mdio,		1),
	GROUP(eth_mdc,		1),
	GROUP(eth_rgmii_rx_clk,	1),
	GROUP(eth_rx_dv,	1),
	GROUP(eth_rxd0,		1),
	GROUP(eth_rxd1,		1),
	GROUP(eth_rxd2_rgmii,	1),
	GROUP(eth_rxd3_rgmii,	1),
	GROUP(eth_rgmii_tx_clk,	1),
	GROUP(eth_txen,		1),
	GROUP(eth_txd0,		1),
	GROUP(eth_txd1,		1),
	GROUP(eth_txd2_rgmii,	1),
	GROUP(eth_txd3_rgmii,	1),
	GROUP(eth_link_led,	1),
	GROUP(eth_act_led,	1),
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
	GROUP(tsin_b_valid_z,	3),
	GROUP(tsin_b_sop_z,	3),
	GROUP(tsin_b_din0_z,	3),
	GROUP(tsin_b_clk_z,	3),
	GROUP(tsin_b_fail,	3),
	GROUP(tsin_b_din1,	3),
	GROUP(tsin_b_din2,	3),
	GROUP(tsin_b_din3,	3),
	GROUP(tsin_b_din4,	3),
	GROUP(tsin_b_din5,	3),
	GROUP(tsin_b_din6,	3),
	GROUP(tsin_b_din7,	3),
	GROUP(pdm_din0_z,	7),
	GROUP(pdm_din1_z,	7),
	GROUP(pdm_din2_z,	7),
	GROUP(pdm_din3_z,	7),
	GROUP(pdm_dclk_z,	7),
	GROUP(tdmc_slv_sclk_z,	6),
	GROUP(tdmc_slv_fs_z,	6),
	GROUP(tdmc_din0_z,	6),
	GROUP(tdmc_din1_z,	6),
	GROUP(tdmc_din2_z,	6),
	GROUP(tdmc_din3_z,	6),
	GROUP(tdmc_sclk_z,	4),
	GROUP(tdmc_fs_z,	4),
	GROUP(tdmc_dout0_z,	4),
	GROUP(tdmc_dout1_z,	4),
	GROUP(tdmc_dout2_z,	4),
	GROUP(tdmc_dout3_z,	4),
	GROUP(mclk1_z,		4),
	GROUP(remote_out_z,	5),
	GROUP(pwm_b_z0,		5),
	GROUP(pwm_b_z13,	5),
	GROUP(pwm_c_z,		5),
	GROUP(pwm_d_z,		2),
	GROUP(pwm_f_z,		5),
	GROUP(clk12_24_z,	2),
	GROUP(gen_clk_ee_z,	7),

	/* bank GPIOX */
	GROUP(sdio_d0,		1),
	GROUP(sdio_d1,		1),
	GROUP(sdio_d2,		1),
	GROUP(sdio_d3,		1),
	GROUP(sdio_clk,		1),
	GROUP(sdio_cmd,		1),
	GROUP(spi0_mosi_x,	4),
	GROUP(spi0_miso_x,	4),
	GROUP(spi0_ss0_x,	4),
	GROUP(spi0_clk_x,	4),
	GROUP(i2c1_sda_x,	5),
	GROUP(i2c1_sck_x,	5),
	GROUP(i2c2_sda_x,	1),
	GROUP(i2c2_sck_x,	1),
	GROUP(uart_tx_a,	1),
	GROUP(uart_rx_a,	1),
	GROUP(uart_cts_a,	1),
	GROUP(uart_rts_a,	1),
	GROUP(uart_tx_b,	2),
	GROUP(uart_rx_b,	2),
	GROUP(iso7816_clk_x,	6),
	GROUP(iso7816_data_x,	6),
	GROUP(pwm_a,		1),
	GROUP(pwm_b_x7,		4),
	GROUP(pwm_b_x19,	1),
	GROUP(pwm_c_x5,		4),
	GROUP(pwm_c_x8,		5),
	GROUP(pwm_d_x3,		4),
	GROUP(pwm_d_x6,		4),
	GROUP(pwm_e,		1),
	GROUP(pwm_f_x,		1),
	GROUP(tsin_a_valid,	3),
	GROUP(tsin_a_sop,	3),
	GROUP(tsin_a_din0,	3),
	GROUP(tsin_a_clk,	3),
	GROUP(tsin_b_valid_x,	3),
	GROUP(tsin_b_sop_x,	3),
	GROUP(tsin_b_din0_x,	3),
	GROUP(tsin_b_clk_x,	3),
	GROUP(pdm_din0_x,	2),
	GROUP(pdm_din1_x,	2),
	GROUP(pdm_din2_x,	2),
	GROUP(pdm_din3_x,	2),
	GROUP(pdm_dclk_x,	2),
	GROUP(tdma_slv_sclk,	2),
	GROUP(tdma_slv_fs,	2),
	GROUP(tdma_din0,	2),
	GROUP(tdma_din1,	2),
	GROUP(tdma_sclk,	1),
	GROUP(tdma_fs,		1),
	GROUP(tdma_dout0,	1),
	GROUP(tdma_dout1,	1),
	GROUP(mclk1_x,		2),
	GROUP(gen_clk_ee_x,	7),

	/* bank GPIOC */
	GROUP(sdcard_d0_c,	1),
	GROUP(sdcard_d1_c,	1),
	GROUP(sdcard_d2_c,	1),
	GROUP(sdcard_d3_c,	1),
	GROUP(sdcard_clk_c,	1),
	GROUP(sdcard_cmd_c,	1),
	GROUP(spi0_mosi_c,	5),
	GROUP(spi0_miso_c,	5),
	GROUP(spi0_ss0_c,	5),
	GROUP(spi0_clk_c,	5),
	GROUP(i2c0_sda_c,	3),
	GROUP(i2c0_sck_c,	3),
	GROUP(uart_ao_rx_a_c2,	2),
	GROUP(uart_ao_tx_a_c3,	2),
	GROUP(iso7816_clk_c,	5),
	GROUP(iso7816_data_c,	5),
	GROUP(pwm_c_c4,		5),
	GROUP(jtag_b_tdo,	2),
	GROUP(jtag_b_tdi,	2),
	GROUP(jtag_b_clk,	2),
	GROUP(jtag_b_tms,	2),
	GROUP(pdm_din0_c,	4),
	GROUP(pdm_din1_c,	4),
	GROUP(pdm_din2_c,	4),
	GROUP(pdm_din3_c,	4),
	GROUP(pdm_dclk_c,	4),

	/* bank GPIOH */
	GROUP(spi1_mosi,	3),
	GROUP(spi1_miso,	3),
	GROUP(spi1_ss0,		3),
	GROUP(spi1_clk,		3),
	GROUP(i2c1_sda_h2,	2),
	GROUP(i2c1_sck_h3,	2),
	GROUP(i2c1_sda_h6,	4),
	GROUP(i2c1_sck_h7,	4),
	GROUP(i2c3_sda_h,	2),
	GROUP(i2c3_sck_h,	2),
	GROUP(uart_tx_c,	2),
	GROUP(uart_rx_c,	2),
	GROUP(uart_cts_c,	2),
	GROUP(uart_rts_c,	2),
	GROUP(iso7816_clk_h,	1),
	GROUP(iso7816_data_h,	1),
	GROUP(pwm_f_h,		4),
	GROUP(cec_ao_a_ee,	4),
	GROUP(cec_ao_b_ee,	5),
	GROUP(hdmitx_sda,	1),
	GROUP(hdmitx_sck,	1),
	GROUP(hdmitx_hpd_in,	1),
	GROUP(spdif_out_h,	1),
	GROUP(spdif_in_h,	1),
	GROUP(tdmb_din3_h,	6),
	GROUP(tdmb_dout3_h,	5),
	GROUP(remote_out_h,	5),
	GROUP(pwm_b_h,		5),

	/* bank GPIOA */
	GROUP(i2c3_sda_a,	2),
	GROUP(i2c3_sck_a,	2),
	GROUP(pdm_din0_a,	1),
	GROUP(pdm_din1_a,	1),
	GROUP(pdm_din2_a,	1),
	GROUP(pdm_din3_a,	1),
	GROUP(pdm_dclk_a,	1),
	GROUP(spdif_in_a10,	1),
	GROUP(spdif_in_a12,	1),
	GROUP(spdif_out_a11,	1),
	GROUP(spdif_out_a13,	1),
	GROUP(tdmb_slv_sclk,	2),
	GROUP(tdmb_slv_fs,	2),
	GROUP(tdmb_din0,	2),
	GROUP(tdmb_din1,	2),
	GROUP(tdmb_din2,	2),
	GROUP(tdmb_din3_a,	2),
	GROUP(tdmb_sclk,	1),
	GROUP(tdmb_fs,		1),
	GROUP(tdmb_dout0,	1),
	GROUP(tdmb_dout1,	1),
	GROUP(tdmb_dout2,	3),
	GROUP(tdmb_dout3_a,	3),
	GROUP(tdmc_slv_sclk_a,	3),
	GROUP(tdmc_slv_fs_a,	3),
	GROUP(tdmc_din0_a,	3),
	GROUP(tdmc_din1_a,	3),
	GROUP(tdmc_din2_a,	3),
	GROUP(tdmc_din3_a,	3),
	GROUP(tdmc_sclk_a,	2),
	GROUP(tdmc_fs_a,	2),
	GROUP(tdmc_dout0_a,	2),
	GROUP(tdmc_dout1_a,	2),
	GROUP(tdmc_dout2_a,	2),
	GROUP(tdmc_dout3_a,	2),
	GROUP(mclk0_a,		1),
	GROUP(mclk1_a,		2),
	GROUP(pwm_d_a4,		3),
	GROUP(pwm_f_a11,	3),

	/* bank GPIOV */
	GROUP(sdio_dummy,	1),
};

/* uart_ao_a */
static const unsigned int uart_ao_tx_a_pins[] = {GPIOAO_0};
static const unsigned int uart_ao_rx_a_pins[] = {GPIOAO_1};
static const unsigned int uart_ao_cts_a_pins[] = {GPIOE_0};
static const unsigned int uart_ao_rts_a_pins[] = {GPIOE_1};

/* uart_ao_b */
static const unsigned int uart_ao_tx_b_2_pins[] = {GPIOAO_2};
static const unsigned int uart_ao_rx_b_3_pins[] = {GPIOAO_3};

static const unsigned int uart_ao_tx_b_8_pins[] = {GPIOAO_8};
static const unsigned int uart_ao_rx_b_9_pins[] = {GPIOAO_9};

static const unsigned int uart_ao_cts_b_pins[] = {GPIOE_0};
static const unsigned int uart_ao_rts_b_pins[] = {GPIOE_1};

/* i2c_ao */
static const unsigned int i2c_ao_sck_pins[] = {GPIOAO_2};
static const unsigned int i2c_ao_sda_pins[] = {GPIOAO_3};

static const unsigned int i2c_ao_sck_e_pins[] = {GPIOE_0};
static const unsigned int i2c_ao_sda_e_pins[] = {GPIOE_1};

/* i2c_ao_slave */
static const unsigned int i2c_ao_slave_sck_pins[] = {GPIOAO_2};
static const unsigned int i2c_ao_slave_sda_pins[] = {GPIOAO_3};

/* ir_in */
static const unsigned int remote_input_ao_pins[] = {GPIOAO_5};

/* ir_out */
static const unsigned int remote_out_ao_pins[] = {GPIOAO_4};

/* pwm_ao_a */
static const unsigned int pwm_ao_a_pins[] = {GPIOAO_11};
static const unsigned int pwm_ao_a_hiz_pins[] = {GPIOAO_11};

/* pwm_ao_b */
static const unsigned int pwm_ao_b_pins[] = {GPIOE_0};

/* pwm_ao_c */
static const unsigned int pwm_ao_c_4_pins[] = {GPIOAO_4};
static const unsigned int pwm_ao_c_hiz_4_pins[] = {GPIOAO_4};
static const unsigned int pwm_ao_c_6_pins[] = {GPIOAO_6};

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

/* tsin_a_ao */
static const unsigned int tsin_a_sop_ao_pins[] = {GPIOAO_6};
static const unsigned int tsin_a_din0_ao_pins[] = {GPIOAO_7};
static const unsigned int tsin_a_clk_ao_pins[] = {GPIOAO_8};
static const unsigned int tsin_a_valid_ao_pins[] = {GPIOAO_9};

/* spdif_out_ao */
static const unsigned int spdif_out_ao_pins[] = {GPIOAO_10};

/* tdmb_out_ao */
static const unsigned int tdmb_dout0_ao_pins[] = {GPIOAO_4};
static const unsigned int tdmb_dout1_ao_pins[] = {GPIOAO_10};
static const unsigned int tdmb_dout2_ao_pins[] = {GPIOAO_6};
static const unsigned int tdmb_fs_ao_pins[] = {GPIOAO_7};
static const unsigned int tdmb_sclk_ao_pins[] = {GPIOAO_8};

/* tdmb_in_ao */
static const unsigned int tdmb_din0_ao_pins[] = {GPIOAO_4};
static const unsigned int tdmb_din1_ao_pins[] = {GPIOAO_10};
static const unsigned int tdmb_din2_ao_pins[] = {GPIOAO_6};
static const unsigned int tdmb_slv_fs_ao_pins[] = {GPIOAO_7};
static const unsigned int tdmb_slv_sclk_ao_pins[] = {GPIOAO_8};

/* mclk0_ao */
static const unsigned int mclk0_ao_pins[] = {GPIOAO_9};

/* gen_clk_ee_ao */
static const unsigned int gen_clk_ee_ao_pins[] = {GPIOAO_11};

/* gen_clk_ao */
static const unsigned int gen_clk_ao_pins[] = {GPIOAO_11};

/* introduce extra ao pin-groups for G12B based on G12A */

/* ir_out */
static const unsigned int remote_out_ao9_pins[] = {GPIOAO_9};

/* pwm_a_gpioe */
static const unsigned int pwm_a_e2_pins[] = {GPIOE_2};

static struct meson_pmx_group meson_g12a_aobus_groups[] = {
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
	GROUP(uart_ao_tx_a,	1),
	GROUP(uart_ao_rx_a,	1),
	GROUP(uart_ao_cts_a,	1),
	GROUP(uart_ao_rts_a,	1),
	GROUP(uart_ao_tx_b_2,	2),
	GROUP(uart_ao_rx_b_3,	2),
	GROUP(uart_ao_tx_b_8,	3),
	GROUP(uart_ao_rx_b_9,	3),
	GROUP(uart_ao_cts_b,	2),
	GROUP(uart_ao_rts_b,	2),
	GROUP(i2c_ao_sck,	1),
	GROUP(i2c_ao_sda,	1),
	GROUP(i2c_ao_sck_e,	4),
	GROUP(i2c_ao_sda_e,	4),
	GROUP(i2c_ao_slave_sck,	3),
	GROUP(i2c_ao_slave_sda,	3),
	GROUP(remote_input_ao,	1),
	GROUP(remote_out_ao,	1),
	GROUP(pwm_ao_a,		3),
	GROUP(pwm_ao_a_hiz,	2),
	GROUP(pwm_ao_b,		3),
	GROUP(pwm_ao_c_4,	3),
	GROUP(pwm_ao_c_hiz_4,	4),
	GROUP(pwm_ao_c_6,	3),
	GROUP(pwm_ao_d_5,	3),
	GROUP(pwm_ao_d_10,	3),
	GROUP(pwm_ao_d_e,	3),
	GROUP(jtag_a_tdi,	1),
	GROUP(jtag_a_tdo,	1),
	GROUP(jtag_a_clk,	1),
	GROUP(jtag_a_tms,	1),
	GROUP(cec_ao_a,		1),
	GROUP(cec_ao_b,		2),
	GROUP(tsin_a_sop_ao,	4),
	GROUP(tsin_a_din0_ao,	4),
	GROUP(tsin_a_clk_ao,	4),
	GROUP(tsin_a_valid_ao,	4),
	GROUP(spdif_out_ao,	4),
	GROUP(tdmb_dout0_ao,	5),
	GROUP(tdmb_dout1_ao,	5),
	GROUP(tdmb_dout2_ao,	5),
	GROUP(tdmb_fs_ao,	5),
	GROUP(tdmb_sclk_ao,	5),
	GROUP(tdmb_din0_ao,	6),
	GROUP(tdmb_din1_ao,	6),
	GROUP(tdmb_din2_ao,	6),
	GROUP(tdmb_slv_fs_ao,	6),
	GROUP(tdmb_slv_sclk_ao,	6),
	GROUP(mclk0_ao,		5),
	GROUP(remote_out_ao9,	2),
	GROUP(pwm_a_e2,		3),
	GROUP(clk12_24_ao,	7),
	GROUP(clk12_24_e,	2),
	GROUP(gen_clk_ee_ao,	4),
	GROUP(gen_clk_ao,	5),
};

static const char * const gpio_periphs_groups[] = {
	"GPIOV_0",

	"GPIOZ_0", "GPIOZ_1", "GPIOZ_2", "GPIOZ_3", "GPIOZ_4",
	"GPIOZ_5", "GPIOZ_6", "GPIOZ_7", "GPIOZ_8", "GPIOZ_9",
	"GPIOZ_10", "GPIOZ_11", "GPIOZ_12", "GPIOZ_13", "GPIOZ_14",
	"GPIOZ_15",

	"GPIOH_0", "GPIOH_1", "GPIOH_2", "GPIOH_3", "GPIOH_4",
	"GPIOH_5", "GPIOH_6", "GPIOH_7", "GPIOH_8",

	"BOOT_0", "BOOT_1", "BOOT_2", "BOOT_3", "BOOT_4",
	"BOOT_5", "BOOT_6", "BOOT_7", "BOOT_8", "BOOT_9",
	"BOOT_10", "BOOT_11", "BOOT_12", "BOOT_13", "BOOT_14",
	"BOOT_15",

	"GPIOC_0", "GPIOC_1", "GPIOC_2", "GPIOC_3", "GPIOC_4",
	"GPIOC_5", "GPIOC_6", "GPIOC_7",

	"GPIOA_0", "GPIOA_1", "GPIOA_2", "GPIOA_3", "GPIOA_4",
	"GPIOA_5", "GPIOA_6", "GPIOA_7", "GPIOA_8", "GPIOA_9",
	"GPIOA_10", "GPIOA_11", "GPIOA_12", "GPIOA_13", "GPIOA_14",
	"GPIOA_15",

	"GPIOX_0", "GPIOX_1", "GPIOX_2", "GPIOX_3", "GPIOX_4",
	"GPIOX_5", "GPIOX_6", "GPIOX_7", "GPIOX_8", "GPIOX_9",
	"GPIOX_10", "GPIOX_11", "GPIOX_12", "GPIOX_13", "GPIOX_14",
	"GPIOX_15", "GPIOX_16", "GPIOX_17", "GPIOX_18", "GPIOX_19",
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
	"nand_wen_clk", "nand_ren_wr", "nand_rb0",
	"emmc_nand_ds", "nand_ce1",
};

static const char * const nor_groups[] = {
	"nor_d", "nor_q", "nor_c", "nor_cs",
	"nor_hold", "nor_wp",
};

static const char * const sdio_groups[] = {
	"sdio_d0", "sdio_d1", "sdio_d2", "sdio_d3",
	"sdio_cmd", "sdio_clk", "sdio_dummy",
};

static const char * const sdcard_groups[] = {
	"sdcard_d0_c", "sdcard_d1_c", "sdcard_d2_c", "sdcard_d3_c",
	"sdcard_clk_c", "sdcard_cmd_c",
	"sdcard_d0_z", "sdcard_d1_z", "sdcard_d2_z", "sdcard_d3_z",
	"sdcard_clk_z", "sdcard_cmd_z",
};

static const char * const spi0_groups[] = {
	"spi0_mosi_c", "spi0_miso_c", "spi0_ss0_c", "spi0_clk_c",
	"spi0_mosi_x", "spi0_miso_x", "spi0_ss0_x", "spi0_clk_x",
};

static const char * const spi1_groups[] = {
	"spi1_mosi", "spi1_miso", "spi1_ss0", "spi1_clk",
};

static const char * const i2c0_groups[] = {
	"i2c0_sda_c", "i2c0_sck_c",
	"i2c0_sda_z0", "i2c0_sck_z1",
	"i2c0_sda_z7", "i2c0_sck_z8",
};

static const char * const i2c1_groups[] = {
	"i2c1_sda_x", "i2c1_sck_x",
	"i2c1_sda_h2", "i2c1_sck_h3",
	"i2c1_sda_h6", "i2c1_sck_h7",
};

static const char * const i2c2_groups[] = {
	"i2c2_sda_x", "i2c2_sck_x",
	"i2c2_sda_z", "i2c2_sck_z",
};

static const char * const i2c3_groups[] = {
	"i2c3_sda_h", "i2c3_sck_h",
	"i2c3_sda_a", "i2c3_sck_a",
};

static const char * const uart_a_groups[] = {
	"uart_tx_a", "uart_rx_a", "uart_cts_a", "uart_rts_a",
};

static const char * const uart_b_groups[] = {
	"uart_tx_b", "uart_rx_b",
};

static const char * const uart_c_groups[] = {
	"uart_tx_c", "uart_rx_c", "uart_cts_c", "uart_rts_c",
};

static const char * const uart_ao_a_ee_groups[] = {
	"uart_ao_rx_a_c2", "uart_ao_tx_a_c3",
};

static const char * const iso7816_groups[] = {
	"iso7816_clk_c", "iso7816_data_c",
	"iso7816_clk_x", "iso7816_data_x",
	"iso7816_clk_h", "iso7816_data_h",
	"iso7816_clk_z", "iso7816_data_z",
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
	"pwm_b_x7", "pwm_b_x19",
	"pwm_b_h", "pwm_b_z0", "pwm_b_z13",
};

static const char * const pwm_c_groups[] = {
	"pwm_c_c4", "pwm_c_x5", "pwm_c_x8",
	"pwm_c_z",
};

static const char * const pwm_d_groups[] = {
	"pwm_d_x3", "pwm_d_x6",
	"pwm_d_z", "pwm_d_a4",
};

static const char * const pwm_e_groups[] = {
	"pwm_e",
};

static const char * const pwm_f_groups[] = {
	"pwm_f_x", "pwm_f_h",
	"pwm_f_z", "pwm_f_a11",
};

static const char * const cec_ao_ee_groups[] = {
	"cec_ao_a_ee", "cec_ao_b_ee",
};

static const char * const jtag_b_groups[] = {
	"jtag_b_tdi", "jtag_b_tdo", "jtag_b_clk", "jtag_b_tms",
};

static const char * const bt656_groups[] = {
	"bt656_a_vs", "bt656_a_hs", "bt656_a_clk",
	"bt656_a_din0", "bt656_a_din1", "bt656_a_din2",
	"bt656_a_din3", "bt656_a_din4", "bt656_a_din5",
	"bt656_a_din6", "bt656_a_din7",
};

static const char * const tsin_a_groups[] = {
	"tsin_a_valid", "tsin_a_sop", "tsin_a_din0",
	"tsin_a_clk",
};

static const char * const tsin_b_groups[] = {
	"tsin_b_valid_x", "tsin_b_sop_x", "tsin_b_din0_x", "tsin_b_clk_x",
	"tsin_b_valid_z", "tsin_b_sop_z", "tsin_b_din0_z", "tsin_b_clk_z",
	"tsin_b_fail", "tsin_b_din1", "tsin_b_din2", "tsin_b_din3",
	"tsin_b_din4", "tsin_b_din5", "tsin_b_din6", "tsin_b_din7",
};

static const char * const hdmitx_groups[] = {
	"hdmitx_sda", "hdmitx_sck", "hdmitx_hpd_in",
};

static const char * const pdm_groups[] = {
	"pdm_din0_c", "pdm_din1_c", "pdm_din2_c", "pdm_din3_c",
	"pdm_dclk_c",
	"pdm_din0_x", "pdm_din1_x", "pdm_din2_x", "pdm_din3_x",
	"pdm_dclk_x",
	"pdm_din0_z", "pdm_din1_z", "pdm_din2_z", "pdm_din3_z",
	"pdm_dclk_z",
	"pdm_din0_a", "pdm_din1_a", "pdm_din2_a", "pdm_din3_a",
	"pdm_dclk_a",
};

static const char * const spdif_in_groups[] = {
	"spdif_in_h", "spdif_in_a10", "spdif_in_a12",
};

static const char * const spdif_out_groups[] = {
	"spdif_out_h", "spdif_out_a11", "spdif_out_a13",
};

static const char * const mclk0_groups[] = {
	"mclk0_a",
};

static const char * const mclk1_groups[] = {
	"mclk1_x", "mclk1_z", "mclk1_a",
};

static const char * const tdma_in_groups[] = {
	"tdma_slv_sclk", "tdma_slv_fs", "tdma_din0", "tdma_din1",
	"tdma_sclk", "tdma_fs",
};

static const char * const tdma_out_groups[] = {
	"tdma_sclk", "tdma_fs", "tdma_dout0", "tdma_dout1",
	"tdma_slv_sclk", "tdma_slv_fs",
};

static const char * const tdmb_in_groups[] = {
	"tdmb_slv_sclk", "tdmb_slv_fs", "tdmb_din0", "tdmb_din1",
	"tdmb_din2", "tdmb_din3_a", "tdmb_din3_h",
	"tdmb_sclk", "tdmb_fs",
};

static const char * const tdmb_out_groups[] = {
	"tdmb_sclk", "tdmb_fs", "tdmb_dout0", "tdmb_dout1",
	"tdmb_dout2", "tdmb_dout3_a", "tdmb_dout3_h",
	"tdmb_slv_sclk", "tdmb_slv_fs",
};

static const char * const tdmc_in_groups[] = {
	"tdmc_slv_sclk_a", "tdmc_slv_fs_a", "tdmc_din0_a", "tdmc_din1_a",
	"tdmc_din2_a", "tdmc_din3_a", "tdmc_sclk_a", "tdmc_fs_a",
	"tdmc_slv_sclk_z", "tdmc_slv_fs_z", "tdmc_din0_z", "tdmc_din1_z",
	"tdmc_din2_z", "tdmc_din3_z", "tdmc_sclk_z", "tdmc_fs_z",
};

static const char * const tdmc_out_groups[] = {
	"tdmc_sclk_a", "tdmc_fs_a", "tdmc_dout0_a", "tdmc_dout1_a",
	"tdmc_dout2_a", "tdmc_dout3_a", "tdmc_slv_sclk_a", "tdmc_slv_fs_a",
	"tdmc_sclk_z", "tdmc_fs_z", "tdmc_dout0_z", "tdmc_dout1_z",
	"tdmc_dout2_z", "tdmc_dout3_z", "tdmc_slv_sclk_z", "tdmc_slv_fs_z",
};

static const char * const remote_out_groups[] = {
	"remote_out_h", "remote_out_z",
};

static const char * const gen_clk_ee_groups[] = {
	"gen_clk_ee_x", "gen_clk_ee_z",
};

static const char * const gpio_aobus_groups[] = {
	"GPIOAO_0", "GPIOAO_1", "GPIOAO_2", "GPIOAO_3", "GPIOAO_4",
	"GPIOAO_5", "GPIOAO_6", "GPIOAO_7", "GPIOAO_8", "GPIOAO_9",
	"GPIOAO_10", "GPIOAO_11", "GPIOE_0", "GPIOE_1", "GPIOE_2",
	"GPIO_TEST_N",
};

static const char * const uart_ao_a_groups[] = {
	"uart_ao_tx_a", "uart_ao_rx_a",
	"uart_ao_cts_a", "uart_ao_rts_a",
};

static const char * const uart_ao_b_groups[] = {
	"uart_ao_tx_b_2", "uart_ao_rx_b_3",
	"uart_ao_tx_b_8", "uart_ao_rx_b_9",
	"uart_ao_cts_b", "uart_ao_rts_b",
};

static const char * const i2c_ao_groups[] = {
	"i2c_ao_sck", "i2c_ao_sda",
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
	"pwm_ao_c_6",
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

static const char * const tsin_a_ao_groups[] = {
	"tsin_a_sop_ao", "tsin_a_din0_ao", "tsin_a_clk_ao", "tsin_a_valid_ao",
};

static const char * const spdif_out_ao_groups[] = {
	"spdif_out_ao",
};

static const char * const tdmb_out_ao_groups[] = {
	"tdmb_dout0_ao", "tdmb_dout1_ao", "tdmb_dout2_ao",
	"tdmb_fs_ao", "tdmb_sclk_ao",
};

static const char * const tdmb_in_ao_groups[] = {
	"tdmb_din0_ao", "tdmb_din1_ao", "tdmb_din2_ao",
	"tdmb_slv_fs_ao", "tdmb_slv_sclk_ao",
};

static const char * const mclk0_ao_groups[] = {
	"mclk0_ao",
};

static const char * const pwm_a_gpioe_groups[] = {
	"pwm_a_e2",
};

static const char * const clk12_24_ee_groups[] = {
	"clk12_24_z",
};

static const char * const clk12_24_ao_groups[] = {
	"clk12_24_ao", "clk12_24_e",
};

static const char * const gen_clk_ee_ao_groups[] = {
	"gen_clk_ee_ao",
};

static const char * const gen_clk_ao_groups[] = {
	"gen_clk_ao",
};

static struct meson_pmx_func meson_g12a_periphs_functions[] = {
	FUNCTION(gpio_periphs),
	FUNCTION(emmc),
	FUNCTION(nor),
	FUNCTION(spi0),
	FUNCTION(spi1),
	FUNCTION(sdio),
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
	FUNCTION(cec_ao_ee),
	FUNCTION(jtag_b),
	FUNCTION(bt656),
	FUNCTION(tsin_a),
	FUNCTION(tsin_b),
	FUNCTION(hdmitx),
	FUNCTION(pdm),
	FUNCTION(spdif_out),
	FUNCTION(spdif_in),
	FUNCTION(mclk0),
	FUNCTION(mclk1),
	FUNCTION(tdma_in),
	FUNCTION(tdma_out),
	FUNCTION(tdmb_in),
	FUNCTION(tdmb_out),
	FUNCTION(tdmc_in),
	FUNCTION(tdmc_out),
	FUNCTION(remote_out),
	FUNCTION(clk12_24_ee),
	FUNCTION(gen_clk_ee),
};

static struct meson_pmx_func meson_g12a_aobus_functions[] = {
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
	FUNCTION(tsin_a_ao),
	FUNCTION(spdif_out_ao),
	FUNCTION(tdmb_out_ao),
	FUNCTION(tdmb_in_ao),
	FUNCTION(mclk0_ao),
	FUNCTION(pwm_a_gpioe),
	FUNCTION(clk12_24_ao),
	FUNCTION(gen_clk_ee_ao),
	FUNCTION(gen_clk_ao),
};

static struct meson_bank meson_g12a_periphs_banks[] = {
	/* name  first  last  irq  pullen  pull  dir  out  in */
	BANK("V",    GPIOV_0,    GPIOV_0,   -1,
	5,  17,  5,  17,  16,  17,  17,  17,  18,  17),
	BANK("Z",    GPIOZ_0,    GPIOZ_15, 12,
	4,  0,  4,  0,  12,  0,  13, 0,  14, 0),
	BANK("H",    GPIOH_0,    GPIOH_8, 28,
	3,  0,  3,  0,  9,  0,  10,  0,  11,  0),
	BANK("BOOT", BOOT_0,     BOOT_15,  37,
	0,  0,  0,  0,  0, 0,  1, 0,  2, 0),
	BANK("C",    GPIOC_0,    GPIOC_7,  53,
	1,  0,  1,  0,  3, 0,  4, 0,  5, 0),
	BANK("A",    GPIOA_0,    GPIOA_15,  61,
	5,  0,  5,  0,  16,  0,  17,  0,  18,  0),
	BANK("X",    GPIOX_0,    GPIOX_19,   77,
	2,  0,  2,  0,  6,  0,  7,  0,  8,  0),
};

static struct meson_bank meson_g12a_aobus_banks[] = {
	/* name  first  last  irq  pullen  pull  dir  out  in  */
	BANK("AO",   GPIOAO_0,  GPIOAO_11,  0,
	3,  0,  2, 0,  0,  0,  4, 0,  1,  0),
	BANK("E",   GPIOE_0,  GPIOE_2,   97,
	3,  16,  2, 16,  0,  16,  4, 16,  1,  16),
	BANK("TESTN",  GPIO_TEST_N,  GPIO_TEST_N,  -1,
	3,  31,  2, 31,  0,  31,  4, 31,  1,  31),
};

static struct meson_pmx_bank meson_g12a_periphs_pmx_banks[] = {
	/*	 name	 first		lask	   reg	offset  */
	BANK_PMX("V",    GPIOV_0, GPIOV_0, 0x2, 24),
	BANK_PMX("Z",    GPIOZ_0, GPIOZ_15, 0x6, 0),
	BANK_PMX("H",    GPIOH_0, GPIOH_8, 0xb, 0),
	BANK_PMX("BOOT", BOOT_0,  BOOT_15,  0x0, 0),
	BANK_PMX("C",    GPIOC_0, GPIOC_7, 0x9, 0),
	BANK_PMX("A",    GPIOA_0, GPIOA_15, 0xd, 0),
	BANK_PMX("X",    GPIOX_0, GPIOX_19, 0x3, 0),
};

static struct meson_axg_pmx_data meson_g12a_periphs_pmx_banks_data = {
	.pmx_banks	= meson_g12a_periphs_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_g12a_periphs_pmx_banks),
};

static struct meson_drive_bank meson_g12a_periphs_drive_banks[] = {
	/*	 name	 first		lask	   reg	offset  */
	BANK_DRIVE("Z",    GPIOZ_0, GPIOZ_15, 0x5, 0),
	BANK_DRIVE("H",    GPIOH_0, GPIOH_8, 0x4, 0),
	BANK_DRIVE("BOOT", BOOT_0,  BOOT_15,  0x0, 0),
	BANK_DRIVE("C",    GPIOC_0, GPIOC_7, 0x1, 0),
	BANK_DRIVE("A",    GPIOA_0, GPIOA_15, 0x6, 0),
	BANK_DRIVE("X",    GPIOX_0, GPIOX_19, 0x2, 0),
};

static struct meson_drive_data meson_g12a_periphs_drive_data = {
	.drive_banks	= meson_g12a_periphs_drive_banks,
	.num_drive_banks = ARRAY_SIZE(meson_g12a_periphs_drive_banks),
};

static struct meson_pmx_bank meson_g12a_aobus_pmx_banks[] = {
	BANK_PMX("AO", GPIOAO_0, GPIOAO_11, 0x0, 0),
	BANK_PMX("E", GPIOE_0, GPIOE_2, 0x1, 16),
	BANK_PMX("TESTN", GPIO_TEST_N, GPIO_TEST_N, 0x1, 28),
};

static struct meson_axg_pmx_data meson_g12a_aobus_pmx_banks_data = {
	.pmx_banks	= meson_g12a_aobus_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_g12a_aobus_pmx_banks),
};

static struct meson_drive_bank meson_g12a_aobus_drive_banks[] = {
	BANK_DRIVE("AO", GPIOAO_0, GPIOAO_11, 0x0, 0),
	BANK_DRIVE("E", GPIOE_0, GPIOE_2, 0x1, 0),
	BANK_DRIVE("TESTN", GPIO_TEST_N, GPIO_TEST_N, 0x1, 28),
};

static struct meson_drive_data meson_g12a_aobus_drive_data = {
	.drive_banks     = meson_g12a_aobus_drive_banks,
	.num_drive_banks = ARRAY_SIZE(meson_g12a_aobus_drive_banks),
};

static int meson_g12a_aobus_init(struct meson_pinctrl *pc)
{
	struct arm_smccc_res res;
	/*set TEST_N to output*/
	arm_smccc_smc(CMD_TEST_N_DIR, TEST_N_OUTPUT, 0, 0, 0, 0, 0, 0, &res);

	return 0;
}

static struct meson_pinctrl_data meson_g12a_periphs_pinctrl_data = {
	.name		= "periphs-banks",
	.init		= NULL,
	.pins		= meson_g12a_periphs_pins,
	.groups		= meson_g12a_periphs_groups,
	.funcs		= meson_g12a_periphs_functions,
	.banks		= meson_g12a_periphs_banks,
	.num_pins	= ARRAY_SIZE(meson_g12a_periphs_pins),
	.num_groups	= ARRAY_SIZE(meson_g12a_periphs_groups),
	.num_funcs	= ARRAY_SIZE(meson_g12a_periphs_functions),
	.num_banks	= ARRAY_SIZE(meson_g12a_periphs_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_g12a_periphs_pmx_banks_data,
	.pcf_ops	= &meson_g12a_pinconf_ops,
	.pcf_drv_data	= &meson_g12a_periphs_drive_data,
};

static struct meson_pinctrl_data meson_g12a_aobus_pinctrl_data = {
	.name		= "aobus-banks",
	.init		= meson_g12a_aobus_init,
	.pins		= meson_g12a_aobus_pins,
	.groups		= meson_g12a_aobus_groups,
	.funcs		= meson_g12a_aobus_functions,
	.banks		= meson_g12a_aobus_banks,
	.num_pins	= ARRAY_SIZE(meson_g12a_aobus_pins),
	.num_groups	= ARRAY_SIZE(meson_g12a_aobus_groups),
	.num_funcs	= ARRAY_SIZE(meson_g12a_aobus_functions),
	.num_banks	= ARRAY_SIZE(meson_g12a_aobus_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_g12a_aobus_pmx_banks_data,
	.pcf_ops	= &meson_g12a_pinconf_ops,
	.pcf_drv_data	= &meson_g12a_aobus_drive_data,
};

static const struct of_device_id meson_g12a_pinctrl_dt_match[] = {
	{
		.compatible = "amlogic,meson-g12a-periphs-pinctrl",
		.data = &meson_g12a_periphs_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-g12a-aobus-pinctrl",
		.data = &meson_g12a_aobus_pinctrl_data,
	},
	{ },
};

static struct platform_driver meson_g12a_pinctrl_driver = {
	.probe  = meson_pinctrl_probe,
	.driver = {
		.name	= "meson-g12a-pinctrl",
		.of_match_table = meson_g12a_pinctrl_dt_match,
	},
};

static int __init g12a_pmx_init(void)
{
	return platform_driver_register(&meson_g12a_pinctrl_driver);
}

static void __exit g12a_pmx_exit(void)
{
	platform_driver_unregister(&meson_g12a_pinctrl_driver);
}

arch_initcall(g12a_pmx_init);
module_exit(g12a_pmx_exit);
MODULE_DESCRIPTION("g12a pin control driver");
MODULE_LICENSE("GPL v2");

