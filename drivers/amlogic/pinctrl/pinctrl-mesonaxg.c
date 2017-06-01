/*
 * drivers/amlogic/pinctrl/pinctrl-mesonaxg.c
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

#include <dt-bindings/gpio/mesonaxg-gpio.h>
#include <linux/arm-smccc.h>
#include "pinctrl-meson.h"

#define EE_OFF	15

static const struct meson_desc_pin mesonaxg_periphs_pins[] = {
	MESON_PINCTRL_PIN(MESON_PIN(GPIOZ_0, EE_OFF), 0x2, 0,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "spi_a"),		/*SPI_CLK_A*/
		MESON_FUNCTION(0x2, "uart_b")),		/*UART_RTS_EE_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOZ_1, EE_OFF), 0x2, 4,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "spi_a"),		/*SPI_MOSI_A*/
		MESON_FUNCTION(0x2, "uart_b")),		/*UART_CTS_EE_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOZ_2, EE_OFF), 0x2, 8,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "spi_a"),		/*SPI_MISO_A*/
		MESON_FUNCTION(0x2, "uart_b")),		/*UART_TX_EE_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOZ_3, EE_OFF), 0x2, 12,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "spi_a"),		/*SPI_SS0_A*/
		MESON_FUNCTION(0x2, "uart_b")),		/*UART_RX_EE_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOZ_4, EE_OFF), 0x2, 16,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "spi_a"),		/*SPI_SS1_A*/
		MESON_FUNCTION(0x2, "pwm_b"),		/*PWM_B*/
		MESON_FUNCTION(0x3, "spdif_in")),	/*SPDIF_IN*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOZ_5, EE_OFF), 0x2, 20,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "spi_a"),		/*SPI_SS2_A*/
		MESON_FUNCTION(0x2, "pwm_a"),		/*PWM_A*/
		MESON_FUNCTION(0x3, "spdif_out")),	/*SPDIF_OUT*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOZ_6, EE_OFF), 0x2, 24,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "i2c_a"),		/*I2C_SCK_A*/
		MESON_FUNCTION(0x2, "uart_ao_b")),	/*UART_CTS_AO_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOZ_7, EE_OFF), 0x2, 28,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "i2c_a"),		/*I2C_SDA_A*/
		MESON_FUNCTION(0x2, "uart_ao_b")),	/*UART_RTS_AO_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOZ_8, EE_OFF), 0x3, 0,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "i2c_b"),		/*I2C_SCK_B*/
		MESON_FUNCTION(0x2, "uart_ao_b")),	/*UART_TX_AO_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOZ_9, EE_OFF), 0x3, 4,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "i2c_b"),		/*I2C_SDA_B*/
		MESON_FUNCTION(0x2, "uart_ao_b")),	/*UART_RX_AO_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOZ_10, EE_OFF), 0x3, 8,
		MESON_FUNCTION(0x0, "gpio")),
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_0, EE_OFF), 0x0, 0,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "emmc"),		/*EMMC_D0*/
		MESON_FUNCTION(0x1, "nandflash")),		/*EMMC_D0*/
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_1, EE_OFF), 0x0, 4,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "emmc"),
		MESON_FUNCTION(0x1, "nandflash")),		/*EMMC_D1*/
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_2, EE_OFF), 0x0, 8,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "emmc"),
		MESON_FUNCTION(0x1, "nandflash")),		/*EMMC_D2*/
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_3, EE_OFF), 0x0, 12,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "emmc"),		/*EMMC_D3*/
		MESON_FUNCTION(0x1, "nandflash"),
		MESON_FUNCTION(0x3, "norflash")),	/*NOR_HOLD*/
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_4, EE_OFF), 0x0, 16,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "emmc"),		/*EMMC_D4*/
		MESON_FUNCTION(0x1, "nandflash"),
		MESON_FUNCTION(0x3, "norflash")),	/*NOR_D*/
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_5, EE_OFF), 0x0, 20,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "emmc"),		/*EMMC_D5*/
		MESON_FUNCTION(0x1, "nandflash"),
		MESON_FUNCTION(0x3, "norflash")),	/*NOR_Q*/
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_6, EE_OFF), 0x0, 24,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "emmc"),		/*EMMC_D6*/
		MESON_FUNCTION(0x1, "nandflash"),
		MESON_FUNCTION(0x3, "norflash")),	/*NOR_C*/
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_7, EE_OFF), 0x0, 28,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "emmc"),
		MESON_FUNCTION(0x1, "nandflash")),		/*EMMC_D7*/
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_8, EE_OFF), 0x1, 0,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "emmc"),		/*EMMC_CLK*/
		MESON_FUNCTION(0x2, "nandflash")),	/*NAND_CE0 */
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_9, EE_OFF), 0x1, 4,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x2, "nandflash"),	/*NAND_ALE*/
		MESON_FUNCTION(0x3, "norflash")),	/*NOR_WP*/
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_10, EE_OFF), 0x1, 8,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "emmc"),		/*EMMC_CMD*/
		MESON_FUNCTION(0x2, "nandflash")),	/*NAND_CLE */
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_11, EE_OFF), 0x1, 12,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x2, "nandflash")),	/*NAND_WEN_CLK*/
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_12, EE_OFF), 0x1, 16,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x2, "nandflash")),	/*NAND_REN_WR */
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_13, EE_OFF), 0x1, 20,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "emmc"),		/*EMMC_DS*/
		MESON_FUNCTION(0x2, "nandflash")),	/*NAND_RB0*/
	MESON_PINCTRL_PIN(MESON_PIN(BOOT_14, EE_OFF), 0x1, 24,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x3, "norflash")),	/*NOR_CS */
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_0, EE_OFF), 0xb, 0,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "mclk"),		/*MCLK_A*/
		MESON_FUNCTION(0x2, "pwm_vs"),		/*PWM_VS*/
		MESON_FUNCTION(0x5, "wifi")),		/*WIFI_BEACON*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_1, EE_OFF), 0xb, 4,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "mclk"),		/*MCLK_B*/
		MESON_FUNCTION(0x3, "spdif_in"),	/*SPDIF_IN*/
		MESON_FUNCTION(0x4, "spdif_out"),	/*SPDIF_OUT*/
		MESON_FUNCTION(0x5, "wifi")),		/*WIFI_BEACON*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_2, EE_OFF), 0xb, 8,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdmc_out"),	/*TDMC_SCLK*/
		MESON_FUNCTION(0x2, "tdmc_in"),		/*TDMC_SLV_SCLK*/
		MESON_FUNCTION(0x3, "spi_b")),		/*SPI_MOSI_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_3, EE_OFF), 0xb, 12,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdmc_out"),	/*TDMC_FS*/
		MESON_FUNCTION(0x2, "tdmc_in"),     /*TDMC_SLV_FS*/
		MESON_FUNCTION(0x3, "spi_b")),		/*SPI_MISO_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_4, EE_OFF), 0xb, 16,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdmc_out"),	/*TDMC_DOUT0*/
		MESON_FUNCTION(0x2, "tdmc_in"),		/*TDMC_DIN0*/
		MESON_FUNCTION(0x3, "spi_b")),		/*SPI_CLK_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_5, EE_OFF), 0xb, 20,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdmc_out"),	/*TDMC_DOUT1*/
		MESON_FUNCTION(0x2, "tdmc_in"),		/*TDMC_DIN1*/
		MESON_FUNCTION(0x3, "spi_b")),		/*SPI_SS0_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_6, EE_OFF), 0xb, 24,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdmc_out"),	/*TDMC_DOUT2*/
		MESON_FUNCTION(0x2, "tdmc_in"),		/*TDMC_DIN2*/
		MESON_FUNCTION(0x3, "spi_b"),		/*SPI_SS1_B*/
		MESON_FUNCTION(0x4, "i2c_d")),		/*I2C_SDA_D*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_7, EE_OFF), 0xb, 28,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdmc_out"),	/*TDMC_DOUT3*/
		MESON_FUNCTION(0x2, "tdmc_in"),		/*TDMC_DIN3*/
		MESON_FUNCTION(0x3, "spdif_in"),	/*SPDIF_IN*/
		MESON_FUNCTION(0x4, "i2c_d")),		/*I2C_SCK_D*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_8, EE_OFF), 0xc, 0,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdmb_out"),	/*TDMB_SCLK*/
		MESON_FUNCTION(0x2, "tdmb_in")),	/*TDMB_SLV_SCLK*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_9, EE_OFF), 0xc, 4,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdmb_out"),	/*TDMB_FS*/
		MESON_FUNCTION(0x2, "tdmb_in")),	/*TDMB_SLV_FS*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_10, EE_OFF), 0xc, 8,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdmb_out"),	/*TDMB_DOUT0*/
		MESON_FUNCTION(0x2, "tdmb_in")),	/*TDMB_DIN0*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_11, EE_OFF), 0xc, 12,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdmb_out"),	/*TDMB_DOUT1*/
		MESON_FUNCTION(0x2, "tdmb_in"),		/*TDMB_DIN1*/
		MESON_FUNCTION(0x3, "spdif_out")),	/*SPDIF_OUT*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_12, EE_OFF), 0xc, 16,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdmb_out"),	/*TDMB_DOUT2*/
		MESON_FUNCTION(0x2, "tdmb_in"),		/*TDMB_DIN2*/
		MESON_FUNCTION(0x4, "i2c_d")),		/*I2C_SDA_D*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_13, EE_OFF), 0xc, 20,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdmb_out"),	/*TDMB_DOUT3*/
		MESON_FUNCTION(0x2, "tdmb_in"),		/*TDMB_DIN3*/
		MESON_FUNCTION(0x4, "i2c_d")),		/*I2C_SCK_D*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_14, EE_OFF), 0xc, 24,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "pdm"),			/*PDM_DCLK*/
		MESON_FUNCTION(0x3, "pwm_a"),		/*PWM_A*/
		MESON_FUNCTION(0x5, "wifi")),		/*WIFI_BEACON*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_15, EE_OFF), 0xc, 28,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "pdm"),			/*PDM_DIN0*/
		MESON_FUNCTION(0x2, "tdmc_out"),	/*TDMC_DOUT2*/
		MESON_FUNCTION(0x3, "pwm_b"),		/*PWM_B*/
		MESON_FUNCTION(0x4, "tdmc_in")),	/*TDMC_DIN2*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_16, EE_OFF), 0xd, 0,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "pdm"),			/*PDM_DIN1*/
		MESON_FUNCTION(0x2, "tdmc_out"),	/*TDMC_DOUT3*/
		MESON_FUNCTION(0x3, "pwm_c"),		/*PWM_C*/
		MESON_FUNCTION(0x4, "tdmc_in")),	/*TDMC_DIN3*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_17, EE_OFF), 0xd, 4,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "pdm"),			/*PDM_DIN2*/
		MESON_FUNCTION(0x2, "tdmb_out"),	/*TDMB_DOUT2*/
		MESON_FUNCTION(0x3, "i2c_c"),		/*I2C_SDA_C*/
		MESON_FUNCTION(0x4, "tdmb_in"),		/*TDMB_DIN2*/
		MESON_FUNCTION(0x5, "eth")),		/*ETH_MDIO*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_18, EE_OFF), 0xd, 8,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "pdm"),			/*PDM_DIN3*/
		MESON_FUNCTION(0x2, "tdmb_out"),	/*TDMB_DOUT3*/
		MESON_FUNCTION(0x3, "i2c_c"),		/*I2C_SCK_C*/
		MESON_FUNCTION(0x4, "tdmb_in"),		/*TDMB_DIN3*/
		MESON_FUNCTION(0x5, "eth")),		/*ETH_MDC*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_19, EE_OFF), 0xd, 12,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "spdif_in"),	/*SPDIF_IN*/
		MESON_FUNCTION(0x2, "spdif_out"),	/*SPDIF_OUT*/
		MESON_FUNCTION(0x3, "pdm"),			/*PDM_DCLK*/
		MESON_FUNCTION(0x4, "i2c_d")),		/*I2C_SDA_D*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOA_20, EE_OFF), 0xd, 16,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "spdif_out"),	/*SPDIF_OUT*/
		MESON_FUNCTION(0x2, "spdif_in"),	/*SPDIF_IN*/
		MESON_FUNCTION(0x4, "i2c_d"),		/*I2C_SCK_D*/
		MESON_FUNCTION(0x5, "wifi")),		/*WIFI_BEACON*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_0, EE_OFF), 0x4, 0,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "sdio"),		/*SDIO_D0*/
		MESON_FUNCTION(0x2, "jtag")),		/*JTAG_TDO*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_1, EE_OFF), 0x4, 4,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "sdio"),		/*SDIO_D1*/
		MESON_FUNCTION(0x2, "jtag")),		/*JTAG_TDI*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_2, EE_OFF), 0x4, 8,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "sdio"),		/*SDIO_D2*/
		MESON_FUNCTION(0x2, "uart_ao_a")),	/*UART_RX_AO_A*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_3, EE_OFF), 0x4, 12,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "sdio"),		/*SDIO_D3*/
		MESON_FUNCTION(0x2, "uart_ao_a")),	/*UART_TX_AO_A*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_4, EE_OFF), 0x4, 16,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "sdio"),		/*SDIO_CLK*/
		MESON_FUNCTION(0x2, "jtag")),		/*JTAG_CLK*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_5, EE_OFF), 0x4, 20,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "sdio"),		/*SDIO_CMD*/
		MESON_FUNCTION(0x2, "jtag")),		/*JTAG_TMS*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_6, EE_OFF), 0x4, 24,
		MESON_FUNCTION(0x0, "gpio")),
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_7, EE_OFF), 0x4, 28,
		MESON_FUNCTION(0x0, "gpio")),
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_8, EE_OFF), 0x5, 0,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "uart_a"),		/*UART_TX_EE_A*/
		MESON_FUNCTION(0x4, "eth")),		/*ETH_TXD0*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_9, EE_OFF), 0x5, 4,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "uart_a"),		/*UART_RX_EE_A*/
		MESON_FUNCTION(0x4, "eth")),		/*ETH_TXD1*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_10, EE_OFF), 0x5, 8,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "uart_a"),		/*UART_CTS_EE_A*/
		MESON_FUNCTION(0x3, "pwm_c"),		/*PWM_C*/
		MESON_FUNCTION(0x4, "eth")),		/*ETH_TXEN*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_11, EE_OFF), 0x5, 12,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "uart_a"),		/*UART_RTS_EE_A*/
		MESON_FUNCTION(0x3, "pwm_d")),		/*PWM_D*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_12, EE_OFF), 0x5, 16,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdma_out"),	/*TDMA_OUT*/
		MESON_FUNCTION(0x2, "tdma_in"),		/*TDMA_IN*/
		MESON_FUNCTION(0x4, "eth")),		/*ETH_RGMII_RX_CLK*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_13, EE_OFF), 0x5, 20,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdma_out"),	/*TDMA_OUT*/
		MESON_FUNCTION(0x2, "tdma_in"),		/*TDMA_IN*/
		MESON_FUNCTION(0x4, "eth")),		/*ETH_RXD0*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_14, EE_OFF), 0x5, 24,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdma_in"),		/*TDMA_IN*/
		MESON_FUNCTION(0x2, "tdma_out"),	/*TDMA_OUT*/
		MESON_FUNCTION(0x4, "eth")),		/*ETH_RXD1*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_15, EE_OFF), 0x5, 28,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "tdma_out"),	/*TDMA_OUT*/
		MESON_FUNCTION(0x2, "tdma_out1"),	/*TDMA_OUT1*/
		MESON_FUNCTION(0x3, "tdma_in"),		/*TDMA_IN*/
		MESON_FUNCTION(0x4, "eth")),		/*ETH_RX_DV*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_16, EE_OFF), 0x6, 0,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "i2c_b"),		/*I2C_SCK_B*/
		MESON_FUNCTION(0x2, "uart_b"),		/*UART_TX_EE_B*/
		MESON_FUNCTION(0x3, "pwm_d"),		/*PWM_D*/
		MESON_FUNCTION(0x4, "spi_b")),		/*SPI_SS0_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_17, EE_OFF), 0x6, 4,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "i2c_b"),		/*I2C_SDA_B*/
		MESON_FUNCTION(0x2, "uart_b"),		/*UART_RX_EE_B*/
		MESON_FUNCTION(0x3, "pwm_c"),		/*PWM_C*/
		MESON_FUNCTION(0x4, "spi_b")),		/*SPI_MOSI_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_18, EE_OFF), 0x6, 8,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "i2c_c"),		/*I2C_SCK_C*/
		MESON_FUNCTION(0x2, "uart_b"),		/*UART_RTS_EE_B*/
		MESON_FUNCTION(0x3, "pwm_a"),		/*PWM_A*/
		MESON_FUNCTION(0x4, "spi_b")),		/*SPI_MISO_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_19, EE_OFF), 0x6, 12,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "i2c_c"),		/*I2C_SDA_C*/
		MESON_FUNCTION(0x2, "uart_b"),		/*UART_CTS_EE_B*/
		MESON_FUNCTION(0x3, "pwm_b"),		/*PWM_B*/
		MESON_FUNCTION(0x4, "spi_b")),		/*SPI_CLK_B*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_20, EE_OFF), 0x6, 16,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "pwm_a")),		/*PWM_A*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_21, EE_OFF), 0x6, 20,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x4, "eth")),		/*ETH_MDIO*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOX_22, EE_OFF), 0x6, 24,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x4, "eth")),		/*ETH_MDC*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_0, EE_OFF), 0x8, 0,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_MDIO*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_1, EE_OFF), 0x8, 4,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_MDC*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_2, EE_OFF), 0x8, 8,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_RGMII_RX_CLK*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_3, EE_OFF), 0x8, 12,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_RX_DV*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_4, EE_OFF), 0x8, 16,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_RXD0*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_5, EE_OFF), 0x8, 20,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_RXD1*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_6, EE_OFF), 0x8, 24,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_RXD2_RGMII*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_7, EE_OFF), 0x8, 28,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_RXD3_RGMII*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_8, EE_OFF), 0x9, 0,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_RGMII_TX_CLK*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_9, EE_OFF), 0x9, 4,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_TXEN*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_10, EE_OFF), 0x9, 8,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_TXD0*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_11, EE_OFF), 0x9, 12,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_TXD1*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_12, EE_OFF), 0x9, 16,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_TXD2_RGMII*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_13, EE_OFF), 0x9, 20,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "eth")),		/*ETH_TXD3_RGMII*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_14, EE_OFF), 0x9, 24,
		MESON_FUNCTION(0x0, "gpio")),
	MESON_PINCTRL_PIN(MESON_PIN(GPIOY_15, EE_OFF), 0x9, 28,
		MESON_FUNCTION(0x0, "gpio")),
};

static const struct meson_desc_pin mesonaxg_aobus_pins[] = {
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_0, 0), 0x0, 0,
		MESON_FUNCTION(0x0, "gpio_ao"),
		MESON_FUNCTION(0x1, "uart_ao_a")),	/*UART_TX_AO_A*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_1, 0), 0x0, 4,
		MESON_FUNCTION(0x0, "gpio_ao"),
		MESON_FUNCTION(0x1, "uart_ao_a")),	/*UART_RX_AO_A*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_2, 0), 0x0, 8,
		MESON_FUNCTION(0x0, "gpio_ao"),
		MESON_FUNCTION(0x1, "uart_ao_b"),	/*UART_CTS_AO_B*/
		MESON_FUNCTION(0x2, "uart_ao_a"),	/*UART_CTS_AO_A*/
		MESON_FUNCTION(0x3, "pwm_ao_b")),	/* PWMAO_B */
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_3, 0), 0x0, 12,
		MESON_FUNCTION(0x0, "gpio_ao"),
		MESON_FUNCTION(0x1, "uart_ao_b"),	/*UART_RTS_AO_B*/
		MESON_FUNCTION(0x2, "uart_ao_a"),	/*UART_RTS_AO_A*/
		MESON_FUNCTION(0x3, "pwm_ao_a"),	/* PWMAO_A */
		MESON_FUNCTION(0x4, "jtag")),		/*JTAG_TDI */
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_4, 0), 0x0, 16,
		MESON_FUNCTION(0x0, "gpio_ao"),
		MESON_FUNCTION(0x1, "uart_ao_b"),	/*UART_TX_AO_B*/
		MESON_FUNCTION(0x2, "i2c_ao"),		/*I2C_SCK_AO*/
		MESON_FUNCTION(0x4, "jtag")),		/*JTAG_TDO */
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_5, 0), 0x0, 20,
		MESON_FUNCTION(0x0, "gpio_ao"),
		MESON_FUNCTION(0x1, "uart_ao_b"),	/*UART_RX_AO_B*/
		MESON_FUNCTION(0x2, "i2c_ao"),		/*I2C_SDA_AO*/
		MESON_FUNCTION(0x4, "jtag")),		/*JTAG_CLK */
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_6, 0), 0x0, 24,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "ir_in")),		/*IR_REMOTE_INPUT*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_7, 0), 0x0, 28,
		MESON_FUNCTION(0x0, "gpio"),
		MESON_FUNCTION(0x1, "ir_out"),		/*IR_REMOTE_OUT*/
		MESON_FUNCTION(0x4, "jtag")),		/*JTAG_TMS*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_8, 0), 0x1, 0,
		MESON_FUNCTION(0x0, "gpio_ao"),
		MESON_FUNCTION(0x1, "clk32"),		/*CLK_32K_IN*/
		MESON_FUNCTION(0x2, "i2c_ao"),		/*I2C_SCK_AO*/
		MESON_FUNCTION(0x3, "pwm_ao_c")),	/*PWMAO_C */
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_9, 0), 0x1, 4,
		MESON_FUNCTION(0x0, "gpio_ao"),
		MESON_FUNCTION(0x2, "i2c_ao"),		/*I2C_SDA_AO*/
		MESON_FUNCTION(0x3, "pwm_ao_d")),	/*PWMAO_D */
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_10, 0), 0x1, 8,
		MESON_FUNCTION(0x0, "gpio_ao"),
		MESON_FUNCTION(0x1, "i2c_slave_ao"),/*I2C_SLAVE_SCK_AO*/
		MESON_FUNCTION(0x2, "i2c_ao")),		/*I2C_SCK_AO */
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_11, 0), 0x1, 12,
		MESON_FUNCTION(0x0, "gpio_ao"),
		MESON_FUNCTION(0x1, "i2c_slave_ao"),/*I2C_SLAVE_SDA_AO*/
		MESON_FUNCTION(0x2, "i2c_ao")),		/*I2C_SDA_AO */
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_12, 0), 0x1, 16,
		MESON_FUNCTION(0x0, "gpio_ao"),
		MESON_FUNCTION(0x1, "clk12"),		/*CLK12_24*/
		MESON_FUNCTION(0x3, "pwm_ao_b")),	/*PWMAO_B */
	MESON_PINCTRL_PIN(MESON_PIN(GPIOAO_13, 0), 0x1, 20,
		MESON_FUNCTION(0x0, "gpio_ao"),
		MESON_FUNCTION(0x1, "clk25"),		/*CLK25_EE*/
		MESON_FUNCTION(0x2, "gen_clk_ao"),	/*GEN_CLK_AO */
		MESON_FUNCTION(0x3, "pwm_ao_c"),	/*PWMAO_C */
		MESON_FUNCTION(0x4, "gen_clk")),	/*GEN_CLK_EE*/
	MESON_PINCTRL_PIN(MESON_PIN(GPIO_TEST_N, 0), 0x1, 24,
		MESON_FUNCTION(0x0, "gpio_ao")),
};

static struct meson_bank mesonaxg_periphs_banks[] = {
	/* name   first   last   irq  pullen  pull    dir  out  in */
	BANK("A", PIN(GPIOA_0, EE_OFF), PIN(GPIOA_20, EE_OFF), 40,
	0, 0,  0,  0,  0,  0,  1, 0,  2, 0),
	BANK("Y", PIN(GPIOY_0, EE_OFF), PIN(GPIOY_15, EE_OFF), 84,
	1, 0,  1,  0,  3,  0,  4, 0,  5, 0),
	BANK("X", PIN(GPIOX_0, EE_OFF), PIN(GPIOX_22, EE_OFF), 61,
	2, 0,  2,  0,  6,  0,  7, 0,  8, 0),
	BANK("Z", PIN(GPIOZ_0, EE_OFF), PIN(GPIOZ_10, EE_OFF), 14,
	3, 0,  3,  0,  9,  0, 10, 0, 11, 0),
	BANK("BOOT", PIN(BOOT_0, EE_OFF), PIN(BOOT_14, EE_OFF), 25,
	4, 0,  4,  0,  12, 0, 13, 0, 14, 0),
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
static struct meson_bank mesonaxg_aobus_banks[] = {
	/*   name  first  last  irq  pullen  pull    dir     out     in  */
	BANK("AO", PIN(GPIOAO_0, 0), PIN(GPIOAO_13, 0), 0,
	0,  16,  0,  0,  0,  0,  0, 16,  1,  0),
	BANK("TEST", PIN(GPIO_TEST_N, 0), PIN(GPIO_TEST_N, 0), -1,
	0, 30, 0, 14, 0, 14, 0, 31, 1, 31),
};

static struct meson_domain_data mesonaxg_periphs_domain_data = {
	.name		= "periphs-banks",
	.banks		= mesonaxg_periphs_banks,
	.num_banks	= ARRAY_SIZE(mesonaxg_periphs_banks),
	.pin_base	= 15,
	.num_pins	= 85,
};

static struct meson_domain_data mesonaxg_aobus_domain_data = {
	.name		= "aobus-banks",
	.banks		= mesonaxg_aobus_banks,
	.num_banks	= ARRAY_SIZE(mesonaxg_aobus_banks),
	.pin_base	= 0,
	.num_pins	= 15,
};

struct meson_pinctrl_data  meson_axg_periphs_pinctrl_data = {
	.meson_pins  = mesonaxg_periphs_pins,
	.domain_data = &mesonaxg_periphs_domain_data,
	.num_pins = ARRAY_SIZE(mesonaxg_periphs_pins),
};

struct meson_pinctrl_data  meson_axg_aobus_pinctrl_data = {
	.meson_pins  = mesonaxg_aobus_pins,
	.domain_data = &mesonaxg_aobus_domain_data,
	.num_pins = ARRAY_SIZE(mesonaxg_aobus_pins),
};

int meson_axg_aobus_init(struct meson_pinctrl *pc)
{
	struct arm_smccc_res res;
	/*set TEST_N to output*/
	arm_smccc_smc(CMD_TEST_N_DIR, TEST_N_OUTPUT, 0, 0, 0, 0, 0, 0, &res);

	return 0;
}
