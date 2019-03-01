/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017-2018 NXP
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

/*!
 * Header file used to configure SoC pad list.
 */

#ifndef SC_PADS_H
#define SC_PADS_H

/* Includes */

/* Defines */

/*!
 * @name Pad Definitions
 */
/*@{*/
#define SC_P_M40_I2C0_SCL                        0	/* M40.I2C0.SCL, M40.UART0.RX, M40.GPIO0.IO02, LSIO.GPIO0.IO06, SCU.UART0.RX */
#define SC_P_M40_GPIO0_00                        1	/* M40.GPIO0.IO00, M40.TPM0.CH0, DMA.UART2.RX, LSIO.GPIO0.IO08 */
#define SC_P_M40_I2C0_SDA                        2	/* M40.I2C0.SDA, M40.UART0.TX, M40.GPIO0.IO03, LSIO.GPIO0.IO07, SCU.UART0.TX */
#define SC_P_M40_GPIO0_01                        3	/* M40.GPIO0.IO01, M40.TPM0.CH1, DMA.UART2.TX, LSIO.GPIO0.IO09 */
#define SC_P_M41_I2C0_SCL                        4	/* M41.I2C0.SCL, M41.UART0.RX, M41.GPIO0.IO02, LSIO.GPIO0.IO10 */
#define SC_P_M41_GPIO0_00                        5	/* M41.GPIO0.IO00, M41.TPM0.CH0, DMA.UART3.RX, LSIO.GPIO0.IO12 */
#define SC_P_M41_I2C0_SDA                        6	/* M41.I2C0.SDA, M41.UART0.TX, M41.GPIO0.IO03, LSIO.GPIO0.IO11 */
#define SC_P_M41_GPIO0_01                        7	/* M41.GPIO0.IO01, M41.TPM0.CH1, DMA.UART3.TX, LSIO.GPIO0.IO13 */
#define SC_P_GPT0_CLK                            8	/* LSIO.GPT0.CLK, DMA.I2C1.SCL, LSIO.KPP0.COL4, LSIO.GPIO0.IO14 */
#define SC_P_GPT0_COMPARE                        9	/* LSIO.GPT0.COMPARE, LSIO.PWM3.OUT, LSIO.KPP0.COL6, LSIO.GPIO0.IO16 */
#define SC_P_GPT0_CAPTURE                        10	/* LSIO.GPT0.CAPTURE, DMA.I2C1.SDA, LSIO.KPP0.COL5, LSIO.GPIO0.IO15 */
#define SC_P_UART0_RX                            11	/* DMA.UART0.RX, SCU.UART0.RX, LSIO.GPIO0.IO20 */
#define SC_P_UART0_TX                            12	/* DMA.UART0.TX, SCU.UART0.TX, LSIO.GPIO0.IO21 */
#define SC_P_UART0_RTS_B                         13	/* DMA.UART0.RTS_B, LSIO.PWM0.OUT, DMA.UART2.RX, LSIO.GPIO0.IO22 */
#define SC_P_UART0_CTS_B                         14	/* DMA.UART0.CTS_B, LSIO.PWM1.OUT, DMA.UART2.TX, LSIO.GPIO0.IO23 */
#define SC_P_UART1_TX                            15	/* DMA.UART1.TX, DMA.SPI3.SCK, LSIO.GPIO0.IO24 */
#define SC_P_UART1_RX                            16	/* DMA.UART1.RX, DMA.SPI3.SDO, LSIO.GPIO0.IO25 */
#define SC_P_UART1_RTS_B                         17	/* DMA.UART1.RTS_B, DMA.SPI3.SDI, DMA.UART1.CTS_B, LSIO.GPIO0.IO26 */
#define SC_P_UART1_CTS_B                         18	/* DMA.UART1.CTS_B, DMA.SPI3.CS0, DMA.UART1.RTS_B, LSIO.GPIO0.IO27 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_GPIOLH        19	/*  */
#define SC_P_SCU_PMIC_MEMC_ON                    20	/* SCU.GPIO0.IOXX_PMIC_MEMC_ON */
#define SC_P_SCU_WDOG_OUT                        21	/* SCU.WDOG0.WDOG_OUT */
#define SC_P_PMIC_I2C_SDA                        22	/* SCU.PMIC_I2C.SDA */
#define SC_P_PMIC_I2C_SCL                        23	/* SCU.PMIC_I2C.SCL */
#define SC_P_PMIC_INT_B                          24	/* SCU.DSC.PMIC_INT_B */
#define SC_P_SCU_GPIO0_00                        25	/* SCU.GPIO0.IO00, SCU.UART0.RX, LSIO.GPIO0.IO28 */
#define SC_P_SCU_GPIO0_01                        26	/* SCU.GPIO0.IO01, SCU.UART0.TX, LSIO.GPIO0.IO29 */
#define SC_P_SCU_GPIO0_02                        27	/* SCU.GPIO0.IO02, SCU.GPIO0.IOXX_PMIC_GPU0_ON, LSIO.GPIO0.IO30 */
#define SC_P_SCU_GPIO0_03                        28	/* SCU.GPIO0.IO03, LSIO.GPIO0.IO31 */
#define SC_P_SCU_GPIO0_04                        29	/* SCU.GPIO0.IO04, SCU.GPIO0.IOXX_PMIC_A72_ON, LSIO.GPIO1.IO00 */
#define SC_P_SCU_GPIO0_05                        30	/* SCU.GPIO0.IO05, LSIO.GPIO1.IO01 */
#define SC_P_SCU_GPIO0_06                        31	/* SCU.GPIO0.IO06, SCU.TPM0.CH0, LSIO.GPIO1.IO02 */
#define SC_P_SCU_GPIO0_07                        32	/* SCU.GPIO0.IO07, SCU.TPM0.CH1, SCU.DSC.RTC_CLOCK_OUTPUT_32K, LSIO.GPIO1.IO03 */
#define SC_P_SCU_BOOT_MODE1                      33	/* SCU.DSC.BOOT_MODE1 */
#define SC_P_SCU_BOOT_MODE2                      34	/* SCU.DSC.BOOT_MODE2 */
#define SC_P_SCU_BOOT_MODE3                      35	/* SCU.DSC.BOOT_MODE3 */
#define SC_P_SCU_BOOT_MODE4                      36	/* SCU.DSC.BOOT_MODE4 */
#define SC_P_LVDS0_GPIO00                        37	/* LVDS0.GPIO0.IO00, LVDS0.PWM0.OUT, LSIO.GPIO1.IO04 */
#define SC_P_LVDS0_GPIO01                        38	/* LVDS0.GPIO0.IO01, LSIO.GPIO1.IO05 */
#define SC_P_LVDS0_I2C0_SCL                      39	/* LVDS0.I2C0.SCL, LVDS0.GPIO0.IO02, LSIO.GPIO1.IO06 */
#define SC_P_LVDS0_I2C0_SDA                      40	/* LVDS0.I2C0.SDA, LVDS0.GPIO0.IO03, LSIO.GPIO1.IO07 */
#define SC_P_LVDS0_I2C1_SCL                      41	/* LVDS0.I2C1.SCL, DMA.UART2.TX, LSIO.GPIO1.IO08 */
#define SC_P_LVDS0_I2C1_SDA                      42	/* LVDS0.I2C1.SDA, DMA.UART2.RX, LSIO.GPIO1.IO09 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_LVDSGPIO      43	/*  */
#define SC_P_MIPI_DSI0_I2C0_SCL                  44	/* MIPI_DSI0.I2C0.SCL, LSIO.GPIO1.IO16 */
#define SC_P_MIPI_DSI0_I2C0_SDA                  45	/* MIPI_DSI0.I2C0.SDA, LSIO.GPIO1.IO17 */
#define SC_P_MIPI_DSI0_GPIO0_00                  46	/* MIPI_DSI0.GPIO0.IO00, MIPI_DSI0.PWM0.OUT, LSIO.GPIO1.IO18 */
#define SC_P_MIPI_DSI0_GPIO0_01                  47	/* MIPI_DSI0.GPIO0.IO01, LSIO.GPIO1.IO19 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_MIPIDSIGPIO   48	/*  */
#define SC_P_MIPI_CSI0_MCLK_OUT                  49	/* MIPI_CSI0.ACM.MCLK_OUT, LSIO.GPIO1.IO24 */
#define SC_P_MIPI_CSI0_I2C0_SCL                  50	/* MIPI_CSI0.I2C0.SCL, LSIO.GPIO1.IO25 */
#define SC_P_MIPI_CSI0_I2C0_SDA                  51	/* MIPI_CSI0.I2C0.SDA, LSIO.GPIO1.IO26 */
#define SC_P_MIPI_CSI0_GPIO0_00                  52	/* MIPI_CSI0.GPIO0.IO00, DMA.I2C0.SCL, MIPI_CSI1.I2C0.SCL, LSIO.GPIO1.IO27 */
#define SC_P_MIPI_CSI0_GPIO0_01                  53	/* MIPI_CSI0.GPIO0.IO01, DMA.I2C0.SDA, MIPI_CSI1.I2C0.SDA, LSIO.GPIO1.IO28 */
#define SC_P_MIPI_CSI1_MCLK_OUT                  54	/* MIPI_CSI1.ACM.MCLK_OUT, LSIO.GPIO1.IO29 */
#define SC_P_MIPI_CSI1_GPIO0_00                  55	/* MIPI_CSI1.GPIO0.IO00, LSIO.GPIO1.IO30 */
#define SC_P_MIPI_CSI1_GPIO0_01                  56	/* MIPI_CSI1.GPIO0.IO01, LSIO.GPIO1.IO31 */
#define SC_P_MIPI_CSI1_I2C0_SCL                  57	/* MIPI_CSI1.I2C0.SCL, LSIO.GPIO2.IO00 */
#define SC_P_MIPI_CSI1_I2C0_SDA                  58	/* MIPI_CSI1.I2C0.SDA, LSIO.GPIO2.IO01 */
#define SC_P_HDMI_TX0_TS_SCL                     59	/* HDMI_TX0.I2C0.SCL, DMA.I2C0.SCL, DMA.I2C2.SCL, LSIO.GPIO2.IO02 */
#define SC_P_HDMI_TX0_TS_SDA                     60	/* HDMI_TX0.I2C0.SDA, DMA.I2C0.SDA, DMA.I2C2.SDA, LSIO.GPIO2.IO03 */
#define SC_P_COMP_CTL_GPIO_3V3_HDMIGPIO          61	/*  */
#define SC_P_ESAI1_FSR                           62	/* AUD.ESAI1.FSR, ADMA.LCDIF.D00, LSIO.GPIO2.IO04 */
#define SC_P_ESAI1_FST                           63	/* AUD.ESAI1.FST, AUD.SPDIF0.EXT_CLK, ADMA.LCDIF.D16, LSIO.GPIO2.IO05 */
#define SC_P_ESAI1_SCKR                          64	/* AUD.ESAI1.SCKR, ADMA.LCDIF.D01, LSIO.GPIO2.IO06 */
#define SC_P_ESAI1_SCKT                          65	/* AUD.ESAI1.SCKT, AUD.SAI2.RXC, AUD.SPDIF0.EXT_CLK, LSIO.GPIO2.IO07 */
#define SC_P_ESAI1_TX0                           66	/* AUD.ESAI1.TX0, AUD.SAI2.RXD, AUD.SPDIF0.RX, LSIO.GPIO2.IO08 */
#define SC_P_ESAI1_TX1                           67	/* AUD.ESAI1.TX1, AUD.SAI2.RXFS, AUD.SPDIF0.TX, LSIO.GPIO2.IO09 */
#define SC_P_ESAI1_TX2_RX3                       68	/* AUD.ESAI1.TX2_RX3, AUD.SPDIF0.RX, ADMA.LCDIF.D17, LSIO.GPIO2.IO10 */
#define SC_P_ESAI1_TX3_RX2                       69	/* AUD.ESAI1.TX3_RX2, AUD.SPDIF0.TX, ADMA.LCDIF.RESET, LSIO.GPIO2.IO11 */
#define SC_P_ESAI1_TX4_RX1                       70	/* AUD.ESAI1.TX4_RX1, ADMA.LCDIF.D02, LSIO.GPIO2.IO12 */
#define SC_P_ESAI1_TX5_RX0                       71	/* AUD.ESAI1.TX5_RX0, ADMA.LCDIF.D03, LSIO.GPIO2.IO13 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_GPIORHB       72	/*  */
#define SC_P_MCLK_IN0                            73	/* AUD.ACM.MCLK_IN0, AUD.ESAI1.RX_HF_CLK, LSIO.GPIO3.IO00, ADMA.LCDIF.EN */
#define SC_P_MCLK_OUT0                           74	/* AUD.ACM.MCLK_OUT0, AUD.ESAI1.TX_HF_CLK, LSIO.GPIO3.IO01 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_GPIORHC       75	/*  */
#define SC_P_SPI0_SCK                            76	/* DMA.SPI0.SCK, AUD.SAI0.RXC, ADMA.LCDIF.D04, LSIO.GPIO3.IO02 */
#define SC_P_SPI0_SDO                            77	/* DMA.SPI0.SDO, AUD.SAI0.TXD, ADMA.LCDIF.D05, LSIO.GPIO3.IO03 */
#define SC_P_SPI0_SDI                            78	/* DMA.SPI0.SDI, AUD.SAI0.RXD, ADMA.LCDIF.D06, LSIO.GPIO3.IO04 */
#define SC_P_SPI0_CS0                            79	/* DMA.SPI0.CS0, AUD.SAI0.RXFS, ADMA.LCDIF.D07, LSIO.GPIO3.IO05 */
#define SC_P_SPI0_CS1                            80	/* DMA.SPI0.CS1, AUD.SAI0.TXC, ADMA.LCDIF.D08, LSIO.GPIO3.IO06 */
#define SC_P_SPI2_SCK                            81	/* DMA.SPI2.SCK, DMA.DMA0.REQ_IN0, ADMA.LCDIF.D09, LSIO.GPIO3.IO07 */
#define SC_P_SPI2_SDO                            82	/* DMA.SPI2.SDO, DMA.FTM.CH0, ADMA.LCDIF.D10, LSIO.GPIO3.IO08 */
#define SC_P_SPI2_SDI                            83	/* DMA.SPI2.SDI, DMA.FTM.CH1, ADMA.LCDIF.D11, LSIO.GPIO3.IO09 */
#define SC_P_SPI2_CS0                            84	/* DMA.SPI2.CS0, DMA.FTM.CH2, ADMA.LCDIF.D12, LSIO.GPIO3.IO10 */
#define SC_P_SPI2_CS1                            85	/* DMA.SPI2.CS1, AUD.SAI0.TXFS, ADMA.LCDIF.D13, LSIO.GPIO3.IO11 */
#define SC_P_SAI1_RXC                            86	/* AUD.SAI1.RXC, AUD.SAI0.TXD, ADMA.LCDIF.HSYNC, LSIO.GPIO3.IO12 */
#define SC_P_SAI1_RXD                            87	/* AUD.SAI1.RXD, AUD.SAI0.TXFS, ADMA.LCDIF.D14, LSIO.GPIO3.IO13 */
#define SC_P_SAI1_RXFS                           88	/* AUD.SAI1.RXFS, AUD.SAI0.RXD, ADMA.LCDIF.EN, LSIO.GPIO3.IO14 */
#define SC_P_SAI1_TXC                            89	/* AUD.SAI1.TXC, AUD.SAI0.TXC, ADMA.LCDIF.VSYNC, LSIO.GPIO3.IO15 */
#define SC_P_SAI1_TXD                            90	/* AUD.SAI1.TXD, AUD.SAI1.RXC, ADMA.LCDIF.CLK, LSIO.GPIO3.IO16 */
#define SC_P_SAI1_TXFS                           91	/* AUD.SAI1.TXFS, AUD.SAI1.RXFS, ADMA.LCDIF.D15, LSIO.GPIO3.IO17 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_GPIORHT       92	/*  */
#define SC_P_ADC_IN0                             93	/* DMA.ADC.IN0, ADMA.LCDIF.D08, LSIO.KPP0.COL0, LSIO.GPIO3.IO18 */
#define SC_P_ADC_IN1                             94	/* DMA.ADC.IN1, ADMA.LCDIF.D13, LSIO.KPP0.COL1, LSIO.GPIO3.IO19 */
#define SC_P_ADC_IN2                             95	/* DMA.ADC.IN2, LSIO.KPP0.COL2, LSIO.GPIO3.IO20 */
#define SC_P_ADC_IN3                             96	/* DMA.ADC.IN3, DMA.SPI1.SCK, LSIO.KPP0.COL3, LSIO.GPIO3.IO21, ADMA.LCDIF.D14 */
#define SC_P_ADC_IN4                             97	/* DMA.ADC.IN4, DMA.SPI1.SDO, LSIO.KPP0.ROW0, LSIO.GPIO3.IO22, ADMA.LCDIF.D15 */
#define SC_P_ADC_IN5                             98	/* DMA.ADC.IN5, DMA.SPI1.SDI, LSIO.KPP0.ROW1, LSIO.GPIO3.IO23, ADMA.LCDIF.CLK */
#define SC_P_LSIO_GPIO3_24                       99	/* DMA.ADC.IN6_dummy, DMA.SPI1.CS0, LSIO.KPP0.ROW2, LSIO.GPIO3.IO24, ADMA.LCDIF.HSYNC */
#define SC_P_LSIO_GPIO3_25                       100	/* DMA.ADC.IN7_dummy, DMA.SPI1.CS1, LSIO.KPP0.ROW3, LSIO.GPIO3.IO25, ADMA.LCDIF.VSYNC */
#define SC_P_FLEXCAN0_RX                         101	/* DMA.FLEXCAN0.RX, LSIO.GPIO3.IO29 */
#define SC_P_FLEXCAN0_TX                         102	/* DMA.FLEXCAN0.TX, LSIO.GPIO3.IO30 */
#define SC_P_FLEXCAN1_RX                         103	/* DMA.FLEXCAN1.RX, LSIO.GPIO3.IO31 */
#define SC_P_FLEXCAN1_TX                         104	/* DMA.FLEXCAN1.TX, LSIO.GPIO4.IO00 */
#define SC_P_FLEXCAN2_RX                         105	/* DMA.FLEXCAN2.RX, LSIO.GPIO4.IO01 */
#define SC_P_FLEXCAN2_TX                         106	/* DMA.FLEXCAN2.TX, LSIO.GPIO4.IO02 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_GPIOTHR       107	/*  */
#define SC_P_MLB_SIG                             108	/* CONN.MLB.SIG, AUD.SAI3.RXC, LSIO.GPIO3.IO26 */
#define SC_P_MLB_CLK                             109	/* CONN.MLB.CLK, AUD.SAI3.RXFS, LSIO.GPIO3.IO27 */
#define SC_P_MLB_DATA                            110	/* CONN.MLB.DATA, AUD.SAI3.RXD, LSIO.GPIO3.IO28 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_GPIOLHT       111	/*  */
#define SC_P_USB_SS3_TC0                         112	/* DMA.I2C1.SCL, CONN.USB_OTG1.PWR, LSIO.QSPI1A.DATA1, LSIO.GPIO4.IO03 */
#define SC_P_USB_SS3_TC1                         113	/* DMA.I2C1.SCL, CONN.USB_OTG2.PWR, LSIO.QSPI1A.DATA2, LSIO.GPIO4.IO04 */
#define SC_P_USB_SS3_TC2                         114	/* DMA.I2C1.SDA, CONN.USB_OTG1.OC, LSIO.QSPI1A.DQS, LSIO.GPIO4.IO05 */
#define SC_P_USB_SS3_TC3                         115	/* DMA.I2C1.SDA, CONN.USB_OTG2.OC, LSIO.QSPI1A.DATA3, LSIO.GPIO4.IO06 */
#define SC_P_COMP_CTL_GPIO_3V3_USB3IO            116	/*  */
#define SC_P_ENET0_MDIO                          117	/* CONN.ENET0.MDIO, DMA.I2C2.SDA, LSIO.GPIO4.IO13 */
#define SC_P_ENET0_MDC                           118	/* CONN.ENET0.MDC, DMA.I2C2.SCL, LSIO.GPIO4.IO14 */
#define SC_P_ENET0_REFCLK_125M_25M               119	/* CONN.ENET0.REFCLK_125M_25M, CONN.ENET0.PPS, LSIO.GPIO4.IO15 */
#define SC_P_ENET1_REFCLK_125M_25M               120	/* CONN.ENET1.REFCLK_125M_25M, CONN.ENET1.PPS, LSIO.GPIO4.IO16 */
#define SC_P_ENET1_MDIO                          121	/* CONN.ENET1.MDIO, DMA.I2C3.SDA, LSIO.GPIO4.IO17 */
#define SC_P_ENET1_MDC                           122	/* CONN.ENET1.MDC, DMA.I2C3.SCL, LSIO.GPIO4.IO18 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_GPIOCT        123	/*  */
#define SC_P_QSPI1A_SS0_B                        124	/* LSIO.QSPI1A.SS0_B, LSIO.GPIO4.IO19 */
#define SC_P_QSPI1A_SCLK                         125	/* LSIO.QSPI1A.SCLK, LSIO.GPIO4.IO21 */
#define SC_P_QSPI1A_DATA0                        126	/* LSIO.QSPI1A.DATA0, LSIO.GPIO4.IO26 */
#define SC_P_COMP_CTL_GPIO_3V3_QSPI1             127	/*  */
#define SC_P_QSPI0A_DATA0                        128	/* LSIO.QSPI0A.DATA0 */
#define SC_P_QSPI0A_DATA1                        129	/* LSIO.QSPI0A.DATA1 */
#define SC_P_QSPI0A_DATA2                        130	/* LSIO.QSPI0A.DATA2 */
#define SC_P_QSPI0A_DATA3                        131	/* LSIO.QSPI0A.DATA3 */
#define SC_P_QSPI0A_DQS                          132	/* LSIO.QSPI0A.DQS */
#define SC_P_QSPI0A_SS0_B                        133	/* LSIO.QSPI0A.SS0_B */
#define SC_P_QSPI0A_SCLK                         134	/* LSIO.QSPI0A.SCLK */
#define SC_P_QSPI0B_SCLK                         135	/* LSIO.QSPI0B.SCLK */
#define SC_P_QSPI0B_DATA0                        136	/* LSIO.QSPI0B.DATA0 */
#define SC_P_QSPI0B_DATA1                        137	/* LSIO.QSPI0B.DATA1 */
#define SC_P_QSPI0B_DATA2                        138	/* LSIO.QSPI0B.DATA2 */
#define SC_P_QSPI0B_DATA3                        139	/* LSIO.QSPI0B.DATA3 */
#define SC_P_QSPI0B_DQS                          140	/* LSIO.QSPI0B.DQS */
#define SC_P_QSPI0B_SS0_B                        141	/* LSIO.QSPI0B.SS0_B */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_QSPI0         142	/*  */
#define SC_P_PCIE_CTRL0_CLKREQ_B                 143	/* HSIO.PCIE0.CLKREQ_B, LSIO.GPIO4.IO27 */
#define SC_P_PCIE_CTRL0_WAKE_B                   144	/* HSIO.PCIE0.WAKE_B, LSIO.GPIO4.IO28 */
#define SC_P_PCIE_CTRL0_PERST_B                  145	/* HSIO.PCIE0.PERST_B, LSIO.GPIO4.IO29 */
#define SC_P_PCIE_CTRL1_CLKREQ_B                 146	/* HSIO.PCIE1.CLKREQ_B, DMA.I2C1.SDA, CONN.USB_OTG2.OC, LSIO.GPIO4.IO30 */
#define SC_P_PCIE_CTRL1_WAKE_B                   147	/* HSIO.PCIE1.WAKE_B, DMA.I2C1.SCL, CONN.USB_OTG2.PWR, LSIO.GPIO4.IO31 */
#define SC_P_PCIE_CTRL1_PERST_B                  148	/* HSIO.PCIE1.PERST_B, DMA.I2C1.SCL, CONN.USB_OTG1.PWR, LSIO.GPIO5.IO00 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_PCIESEP       149	/*  */
#define SC_P_EMMC0_CLK                           150	/* CONN.EMMC0.CLK, CONN.NAND.READY_B */
#define SC_P_EMMC0_CMD                           151	/* CONN.EMMC0.CMD, CONN.NAND.DQS, AUD.MQS.R, LSIO.GPIO5.IO03 */
#define SC_P_EMMC0_DATA0                         152	/* CONN.EMMC0.DATA0, CONN.NAND.DATA00, LSIO.GPIO5.IO04 */
#define SC_P_EMMC0_DATA1                         153	/* CONN.EMMC0.DATA1, CONN.NAND.DATA01, LSIO.GPIO5.IO05 */
#define SC_P_EMMC0_DATA2                         154	/* CONN.EMMC0.DATA2, CONN.NAND.DATA02, LSIO.GPIO5.IO06 */
#define SC_P_EMMC0_DATA3                         155	/* CONN.EMMC0.DATA3, CONN.NAND.DATA03, LSIO.GPIO5.IO07 */
#define SC_P_EMMC0_DATA4                         156	/* CONN.EMMC0.DATA4, CONN.NAND.DATA04, LSIO.GPIO5.IO08 */
#define SC_P_EMMC0_DATA5                         157	/* CONN.EMMC0.DATA5, CONN.NAND.DATA05, LSIO.GPIO5.IO09 */
#define SC_P_EMMC0_DATA6                         158	/* CONN.EMMC0.DATA6, CONN.NAND.DATA06, LSIO.GPIO5.IO10 */
#define SC_P_EMMC0_DATA7                         159	/* CONN.EMMC0.DATA7, CONN.NAND.DATA07, LSIO.GPIO5.IO11 */
#define SC_P_EMMC0_STROBE                        160	/* CONN.EMMC0.STROBE, CONN.NAND.CLE, LSIO.GPIO5.IO12 */
#define SC_P_EMMC0_RESET_B                       161	/* CONN.EMMC0.RESET_B, CONN.NAND.WP_B, CONN.USDHC1.VSELECT, LSIO.GPIO5.IO13 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_SD1FIX        162	/*  */
#define SC_P_USDHC1_CLK                          163	/* CONN.USDHC1.CLK, AUD.MQS.R */
#define SC_P_USDHC1_DATA1                        164	/* CONN.USDHC1.DATA1, CONN.NAND.RE_P, LSIO.GPIO5.IO16 */
#define SC_P_USDHC1_DATA0                        165	/* CONN.USDHC1.DATA0, CONN.NAND.RE_N, LSIO.GPIO5.IO15 */
#define SC_P_USDHC1_CMD                          166	/* CONN.USDHC1.CMD, AUD.MQS.L, LSIO.GPIO5.IO14 */
#define SC_P_CTL_NAND_RE_P_N                     167	/*  */
#define SC_P_USDHC1_DATA4                        168	/* CONN.USDHC1.DATA4, CONN.NAND.CE0_B, AUD.MQS.R, LSIO.GPIO5.IO19 */
#define SC_P_USDHC1_DATA3                        169	/* CONN.USDHC1.DATA3, CONN.NAND.DQS_P, LSIO.GPIO5.IO18 */
#define SC_P_CTL_NAND_DQS_P_N                    170	/*  */
#define SC_P_USDHC1_DATA2                        171	/* CONN.USDHC1.DATA2, CONN.NAND.DQS_N, LSIO.GPIO5.IO17 */
#define SC_P_USDHC1_DATA5                        172	/* CONN.USDHC1.DATA5, CONN.NAND.RE_B, AUD.MQS.L, LSIO.GPIO5.IO20 */
#define SC_P_USDHC1_DATA6                        173	/* CONN.USDHC1.DATA6, CONN.NAND.WE_B, CONN.USDHC1.WP, LSIO.GPIO5.IO21 */
#define SC_P_USDHC1_DATA7                        174	/* CONN.USDHC1.DATA7, CONN.NAND.ALE, CONN.USDHC1.CD_B, LSIO.GPIO5.IO22 */
#define SC_P_USDHC1_STROBE                       175	/* CONN.USDHC1.STROBE, CONN.NAND.CE1_B, CONN.USDHC1.RESET_B, LSIO.GPIO5.IO23 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_VSEL2         176	/*  */
#define SC_P_ENET0_RGMII_TXC                     177	/* CONN.ENET0.RGMII_TXC, CONN.ENET0.RCLK50M_OUT, CONN.ENET0.RCLK50M_IN, LSIO.GPIO5.IO30 */
#define SC_P_ENET0_RGMII_TX_CTL                  178	/* CONN.ENET0.RGMII_TX_CTL, LSIO.GPIO5.IO31 */
#define SC_P_ENET0_RGMII_TXD0                    179	/* CONN.ENET0.RGMII_TXD0, LSIO.GPIO6.IO00 */
#define SC_P_ENET0_RGMII_TXD1                    180	/* CONN.ENET0.RGMII_TXD1, LSIO.GPIO6.IO01 */
#define SC_P_ENET0_RGMII_TXD2                    181	/* CONN.ENET0.RGMII_TXD2, DMA.UART3.TX, LSIO.GPIO6.IO02 */
#define SC_P_ENET0_RGMII_TXD3                    182	/* CONN.ENET0.RGMII_TXD3, DMA.UART3.RTS_B, LSIO.GPIO6.IO03 */
#define SC_P_ENET0_RGMII_RXC                     183	/* CONN.ENET0.RGMII_RXC, DMA.UART3.CTS_B, LSIO.GPIO6.IO04 */
#define SC_P_ENET0_RGMII_RX_CTL                  184	/* CONN.ENET0.RGMII_RX_CTL, LSIO.GPIO6.IO05 */
#define SC_P_ENET0_RGMII_RXD0                    185	/* CONN.ENET0.RGMII_RXD0, LSIO.GPIO6.IO06 */
#define SC_P_ENET0_RGMII_RXD1                    186	/* CONN.ENET0.RGMII_RXD1, LSIO.GPIO6.IO07 */
#define SC_P_ENET0_RGMII_RXD2                    187	/* CONN.ENET0.RGMII_RXD2, CONN.ENET0.RMII_RX_ER, LSIO.GPIO6.IO08 */
#define SC_P_ENET0_RGMII_RXD3                    188	/* CONN.ENET0.RGMII_RXD3, DMA.UART3.RX, LSIO.GPIO6.IO09 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_ENETB         189	/*  */
#define SC_P_ENET1_RGMII_TXC                     190	/* CONN.ENET1.RGMII_TXC, CONN.ENET1.RCLK50M_OUT, CONN.ENET1.RCLK50M_IN, LSIO.GPIO6.IO10 */
#define SC_P_ENET1_RGMII_TX_CTL                  191	/* CONN.ENET1.RGMII_TX_CTL, LSIO.GPIO6.IO11 */
#define SC_P_ENET1_RGMII_TXD0                    192	/* CONN.ENET1.RGMII_TXD0, LSIO.GPIO6.IO12 */
#define SC_P_ENET1_RGMII_TXD1                    193	/* CONN.ENET1.RGMII_TXD1, LSIO.GPIO6.IO13 */
#define SC_P_ENET1_RGMII_TXD2                    194	/* CONN.ENET1.RGMII_TXD2, DMA.UART3.TX, LSIO.GPIO6.IO14 */
#define SC_P_ENET1_RGMII_TXD3                    195	/* CONN.ENET1.RGMII_TXD3, DMA.UART3.RTS_B, LSIO.GPIO6.IO15 */
#define SC_P_ENET1_RGMII_RXC                     196	/* CONN.ENET1.RGMII_RXC, DMA.UART3.CTS_B, LSIO.GPIO6.IO16 */
#define SC_P_ENET1_RGMII_RX_CTL                  197	/* CONN.ENET1.RGMII_RX_CTL, LSIO.GPIO6.IO17 */
#define SC_P_ENET1_RGMII_RXD0                    198	/* CONN.ENET1.RGMII_RXD0, LSIO.GPIO6.IO18 */
#define SC_P_ENET1_RGMII_RXD1                    199	/* CONN.ENET1.RGMII_RXD1, LSIO.GPIO6.IO19 */
#define SC_P_ENET1_RGMII_RXD3                    200	/* CONN.ENET1.RGMII_RXD3, DMA.UART3.RX, LSIO.GPIO6.IO21 */
#define SC_P_ENET1_RGMII_RXD2                    201	/* CONN.ENET1.RGMII_RXD2, CONN.ENET1.RMII_RX_ER, LSIO.GPIO6.IO20 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_ENETA         202	/*  */
/*@}*/

/*!
 * @name Pad Mux Definitions
 * format: name padid padmux
 */
/*@{*/
#define SC_P_M40_I2C0_SCL_M40_I2C0_SCL                          SC_P_M40_I2C0_SCL                  0
#define SC_P_M40_I2C0_SCL_M40_UART0_RX                          SC_P_M40_I2C0_SCL                  1
#define SC_P_M40_I2C0_SCL_M40_GPIO0_IO02                        SC_P_M40_I2C0_SCL                  2
#define SC_P_M40_I2C0_SCL_LSIO_GPIO0_IO06                       SC_P_M40_I2C0_SCL                  3
#define SC_P_M40_I2C0_SCL_SCU_UART0_RX                          SC_P_M40_I2C0_SCL                  4
#define SC_P_M40_GPIO0_00_M40_GPIO0_IO00                        SC_P_M40_GPIO0_00                  0
#define SC_P_M40_GPIO0_00_M40_TPM0_CH0                          SC_P_M40_GPIO0_00                  1
#define SC_P_M40_GPIO0_00_DMA_UART2_RX                          SC_P_M40_GPIO0_00                  2
#define SC_P_M40_GPIO0_00_LSIO_GPIO0_IO08                       SC_P_M40_GPIO0_00                  3
#define SC_P_M40_I2C0_SDA_M40_I2C0_SDA                          SC_P_M40_I2C0_SDA                  0
#define SC_P_M40_I2C0_SDA_M40_UART0_TX                          SC_P_M40_I2C0_SDA                  1
#define SC_P_M40_I2C0_SDA_M40_GPIO0_IO03                        SC_P_M40_I2C0_SDA                  2
#define SC_P_M40_I2C0_SDA_LSIO_GPIO0_IO07                       SC_P_M40_I2C0_SDA                  3
#define SC_P_M40_I2C0_SDA_SCU_UART0_TX                          SC_P_M40_I2C0_SDA                  4
#define SC_P_M40_GPIO0_01_M40_GPIO0_IO01                        SC_P_M40_GPIO0_01                  0
#define SC_P_M40_GPIO0_01_M40_TPM0_CH1                          SC_P_M40_GPIO0_01                  1
#define SC_P_M40_GPIO0_01_DMA_UART2_TX                          SC_P_M40_GPIO0_01                  2
#define SC_P_M40_GPIO0_01_LSIO_GPIO0_IO09                       SC_P_M40_GPIO0_01                  3
#define SC_P_M41_I2C0_SCL_M41_I2C0_SCL                          SC_P_M41_I2C0_SCL                  0
#define SC_P_M41_I2C0_SCL_M41_UART0_RX                          SC_P_M41_I2C0_SCL                  1
#define SC_P_M41_I2C0_SCL_M41_GPIO0_IO02                        SC_P_M41_I2C0_SCL                  2
#define SC_P_M41_I2C0_SCL_LSIO_GPIO0_IO10                       SC_P_M41_I2C0_SCL                  3
#define SC_P_M41_GPIO0_00_M41_GPIO0_IO00                        SC_P_M41_GPIO0_00                  0
#define SC_P_M41_GPIO0_00_M41_TPM0_CH0                          SC_P_M41_GPIO0_00                  1
#define SC_P_M41_GPIO0_00_DMA_UART3_RX                          SC_P_M41_GPIO0_00                  2
#define SC_P_M41_GPIO0_00_LSIO_GPIO0_IO12                       SC_P_M41_GPIO0_00                  3
#define SC_P_M41_I2C0_SDA_M41_I2C0_SDA                          SC_P_M41_I2C0_SDA                  0
#define SC_P_M41_I2C0_SDA_M41_UART0_TX                          SC_P_M41_I2C0_SDA                  1
#define SC_P_M41_I2C0_SDA_M41_GPIO0_IO03                        SC_P_M41_I2C0_SDA                  2
#define SC_P_M41_I2C0_SDA_LSIO_GPIO0_IO11                       SC_P_M41_I2C0_SDA                  3
#define SC_P_M41_GPIO0_01_M41_GPIO0_IO01                        SC_P_M41_GPIO0_01                  0
#define SC_P_M41_GPIO0_01_M41_TPM0_CH1                          SC_P_M41_GPIO0_01                  1
#define SC_P_M41_GPIO0_01_DMA_UART3_TX                          SC_P_M41_GPIO0_01                  2
#define SC_P_M41_GPIO0_01_LSIO_GPIO0_IO13                       SC_P_M41_GPIO0_01                  3
#define SC_P_GPT0_CLK_LSIO_GPT0_CLK                             SC_P_GPT0_CLK                      0
#define SC_P_GPT0_CLK_DMA_I2C1_SCL                              SC_P_GPT0_CLK                      1
#define SC_P_GPT0_CLK_LSIO_KPP0_COL4                            SC_P_GPT0_CLK                      2
#define SC_P_GPT0_CLK_LSIO_GPIO0_IO14                           SC_P_GPT0_CLK                      3
#define SC_P_GPT0_COMPARE_LSIO_GPT0_COMPARE                     SC_P_GPT0_COMPARE                  0
#define SC_P_GPT0_COMPARE_LSIO_PWM3_OUT                         SC_P_GPT0_COMPARE                  1
#define SC_P_GPT0_COMPARE_LSIO_KPP0_COL6                        SC_P_GPT0_COMPARE                  2
#define SC_P_GPT0_COMPARE_LSIO_GPIO0_IO16                       SC_P_GPT0_COMPARE                  3
#define SC_P_GPT0_CAPTURE_LSIO_GPT0_CAPTURE                     SC_P_GPT0_CAPTURE                  0
#define SC_P_GPT0_CAPTURE_DMA_I2C1_SDA                          SC_P_GPT0_CAPTURE                  1
#define SC_P_GPT0_CAPTURE_LSIO_KPP0_COL5                        SC_P_GPT0_CAPTURE                  2
#define SC_P_GPT0_CAPTURE_LSIO_GPIO0_IO15                       SC_P_GPT0_CAPTURE                  3
#define SC_P_UART0_RX_DMA_UART0_RX                              SC_P_UART0_RX                      0
#define SC_P_UART0_RX_SCU_UART0_RX                              SC_P_UART0_RX                      1
#define SC_P_UART0_RX_LSIO_GPIO0_IO20                           SC_P_UART0_RX                      3
#define SC_P_UART0_TX_DMA_UART0_TX                              SC_P_UART0_TX                      0
#define SC_P_UART0_TX_SCU_UART0_TX                              SC_P_UART0_TX                      1
#define SC_P_UART0_TX_LSIO_GPIO0_IO21                           SC_P_UART0_TX                      3
#define SC_P_UART0_RTS_B_DMA_UART0_RTS_B                        SC_P_UART0_RTS_B                   0
#define SC_P_UART0_RTS_B_LSIO_PWM0_OUT                          SC_P_UART0_RTS_B                   1
#define SC_P_UART0_RTS_B_DMA_UART2_RX                           SC_P_UART0_RTS_B                   2
#define SC_P_UART0_RTS_B_LSIO_GPIO0_IO22                        SC_P_UART0_RTS_B                   3
#define SC_P_UART0_CTS_B_DMA_UART0_CTS_B                        SC_P_UART0_CTS_B                   0
#define SC_P_UART0_CTS_B_LSIO_PWM1_OUT                          SC_P_UART0_CTS_B                   1
#define SC_P_UART0_CTS_B_DMA_UART2_TX                           SC_P_UART0_CTS_B                   2
#define SC_P_UART0_CTS_B_LSIO_GPIO0_IO23                        SC_P_UART0_CTS_B                   3
#define SC_P_UART1_TX_DMA_UART1_TX                              SC_P_UART1_TX                      0
#define SC_P_UART1_TX_DMA_SPI3_SCK                              SC_P_UART1_TX                      1
#define SC_P_UART1_TX_LSIO_GPIO0_IO24                           SC_P_UART1_TX                      3
#define SC_P_UART1_RX_DMA_UART1_RX                              SC_P_UART1_RX                      0
#define SC_P_UART1_RX_DMA_SPI3_SDO                              SC_P_UART1_RX                      1
#define SC_P_UART1_RX_LSIO_GPIO0_IO25                           SC_P_UART1_RX                      3
#define SC_P_UART1_RTS_B_DMA_UART1_RTS_B                        SC_P_UART1_RTS_B                   0
#define SC_P_UART1_RTS_B_DMA_SPI3_SDI                           SC_P_UART1_RTS_B                   1
#define SC_P_UART1_RTS_B_DMA_UART1_CTS_B                        SC_P_UART1_RTS_B                   2
#define SC_P_UART1_RTS_B_LSIO_GPIO0_IO26                        SC_P_UART1_RTS_B                   3
#define SC_P_UART1_CTS_B_DMA_UART1_CTS_B                        SC_P_UART1_CTS_B                   0
#define SC_P_UART1_CTS_B_DMA_SPI3_CS0                           SC_P_UART1_CTS_B                   1
#define SC_P_UART1_CTS_B_DMA_UART1_RTS_B                        SC_P_UART1_CTS_B                   2
#define SC_P_UART1_CTS_B_LSIO_GPIO0_IO27                        SC_P_UART1_CTS_B                   3
#define SC_P_SCU_PMIC_MEMC_ON_SCU_GPIO0_IOXX_PMIC_MEMC_ON       SC_P_SCU_PMIC_MEMC_ON              0
#define SC_P_SCU_WDOG_OUT_SCU_WDOG0_WDOG_OUT                    SC_P_SCU_WDOG_OUT                  0
#define SC_P_PMIC_I2C_SDA_SCU_PMIC_I2C_SDA                      SC_P_PMIC_I2C_SDA                  0
#define SC_P_PMIC_I2C_SCL_SCU_PMIC_I2C_SCL                      SC_P_PMIC_I2C_SCL                  0
#define SC_P_PMIC_INT_B_SCU_DSC_PMIC_INT_B                      SC_P_PMIC_INT_B                    0
#define SC_P_SCU_GPIO0_00_SCU_GPIO0_IO00                        SC_P_SCU_GPIO0_00                  0
#define SC_P_SCU_GPIO0_00_SCU_UART0_RX                          SC_P_SCU_GPIO0_00                  1
#define SC_P_SCU_GPIO0_00_LSIO_GPIO0_IO28                       SC_P_SCU_GPIO0_00                  3
#define SC_P_SCU_GPIO0_01_SCU_GPIO0_IO01                        SC_P_SCU_GPIO0_01                  0
#define SC_P_SCU_GPIO0_01_SCU_UART0_TX                          SC_P_SCU_GPIO0_01                  1
#define SC_P_SCU_GPIO0_01_LSIO_GPIO0_IO29                       SC_P_SCU_GPIO0_01                  3
#define SC_P_SCU_GPIO0_02_SCU_GPIO0_IO02                        SC_P_SCU_GPIO0_02                  0
#define SC_P_SCU_GPIO0_02_SCU_GPIO0_IOXX_PMIC_GPU0_ON           SC_P_SCU_GPIO0_02                  1
#define SC_P_SCU_GPIO0_02_LSIO_GPIO0_IO30                       SC_P_SCU_GPIO0_02                  3
#define SC_P_SCU_GPIO0_03_SCU_GPIO0_IO03                        SC_P_SCU_GPIO0_03                  0
#define SC_P_SCU_GPIO0_03_LSIO_GPIO0_IO31                       SC_P_SCU_GPIO0_03                  3
#define SC_P_SCU_GPIO0_04_SCU_GPIO0_IO04                        SC_P_SCU_GPIO0_04                  0
#define SC_P_SCU_GPIO0_04_SCU_GPIO0_IOXX_PMIC_A72_ON            SC_P_SCU_GPIO0_04                  1
#define SC_P_SCU_GPIO0_04_LSIO_GPIO1_IO00                       SC_P_SCU_GPIO0_04                  3
#define SC_P_SCU_GPIO0_05_SCU_GPIO0_IO05                        SC_P_SCU_GPIO0_05                  0
#define SC_P_SCU_GPIO0_05_LSIO_GPIO1_IO01                       SC_P_SCU_GPIO0_05                  3
#define SC_P_SCU_GPIO0_06_SCU_GPIO0_IO06                        SC_P_SCU_GPIO0_06                  0
#define SC_P_SCU_GPIO0_06_SCU_TPM0_CH0                          SC_P_SCU_GPIO0_06                  1
#define SC_P_SCU_GPIO0_06_LSIO_GPIO1_IO02                       SC_P_SCU_GPIO0_06                  3
#define SC_P_SCU_GPIO0_07_SCU_GPIO0_IO07                        SC_P_SCU_GPIO0_07                  0
#define SC_P_SCU_GPIO0_07_SCU_TPM0_CH1                          SC_P_SCU_GPIO0_07                  1
#define SC_P_SCU_GPIO0_07_SCU_DSC_RTC_CLOCK_OUTPUT_32K          SC_P_SCU_GPIO0_07                  2
#define SC_P_SCU_GPIO0_07_LSIO_GPIO1_IO03                       SC_P_SCU_GPIO0_07                  3
#define SC_P_SCU_BOOT_MODE1_SCU_DSC_BOOT_MODE1                  SC_P_SCU_BOOT_MODE1                0
#define SC_P_SCU_BOOT_MODE2_SCU_DSC_BOOT_MODE2                  SC_P_SCU_BOOT_MODE2                0
#define SC_P_SCU_BOOT_MODE3_SCU_DSC_BOOT_MODE3                  SC_P_SCU_BOOT_MODE3                0
#define SC_P_SCU_BOOT_MODE4_SCU_DSC_BOOT_MODE4                  SC_P_SCU_BOOT_MODE4                0
#define SC_P_LVDS0_GPIO00_LVDS0_GPIO0_IO00                      SC_P_LVDS0_GPIO00                  0
#define SC_P_LVDS0_GPIO00_LVDS0_PWM0_OUT                        SC_P_LVDS0_GPIO00                  1
#define SC_P_LVDS0_GPIO00_LSIO_GPIO1_IO04                       SC_P_LVDS0_GPIO00                  3
#define SC_P_LVDS0_GPIO01_LVDS0_GPIO0_IO01                      SC_P_LVDS0_GPIO01                  0
#define SC_P_LVDS0_GPIO01_LSIO_GPIO1_IO05                       SC_P_LVDS0_GPIO01                  3
#define SC_P_LVDS0_I2C0_SCL_LVDS0_I2C0_SCL                      SC_P_LVDS0_I2C0_SCL                0
#define SC_P_LVDS0_I2C0_SCL_LVDS0_GPIO0_IO02                    SC_P_LVDS0_I2C0_SCL                1
#define SC_P_LVDS0_I2C0_SCL_LSIO_GPIO1_IO06                     SC_P_LVDS0_I2C0_SCL                3
#define SC_P_LVDS0_I2C0_SDA_LVDS0_I2C0_SDA                      SC_P_LVDS0_I2C0_SDA                0
#define SC_P_LVDS0_I2C0_SDA_LVDS0_GPIO0_IO03                    SC_P_LVDS0_I2C0_SDA                1
#define SC_P_LVDS0_I2C0_SDA_LSIO_GPIO1_IO07                     SC_P_LVDS0_I2C0_SDA                3
#define SC_P_LVDS0_I2C1_SCL_LVDS0_I2C1_SCL                      SC_P_LVDS0_I2C1_SCL                0
#define SC_P_LVDS0_I2C1_SCL_DMA_UART2_TX                        SC_P_LVDS0_I2C1_SCL                1
#define SC_P_LVDS0_I2C1_SCL_LSIO_GPIO1_IO08                     SC_P_LVDS0_I2C1_SCL                3
#define SC_P_LVDS0_I2C1_SDA_LVDS0_I2C1_SDA                      SC_P_LVDS0_I2C1_SDA                0
#define SC_P_LVDS0_I2C1_SDA_DMA_UART2_RX                        SC_P_LVDS0_I2C1_SDA                1
#define SC_P_LVDS0_I2C1_SDA_LSIO_GPIO1_IO09                     SC_P_LVDS0_I2C1_SDA                3
#define SC_P_MIPI_DSI0_I2C0_SCL_MIPI_DSI0_I2C0_SCL              SC_P_MIPI_DSI0_I2C0_SCL            0
#define SC_P_MIPI_DSI0_I2C0_SCL_LSIO_GPIO1_IO16                 SC_P_MIPI_DSI0_I2C0_SCL            3
#define SC_P_MIPI_DSI0_I2C0_SDA_MIPI_DSI0_I2C0_SDA              SC_P_MIPI_DSI0_I2C0_SDA            0
#define SC_P_MIPI_DSI0_I2C0_SDA_LSIO_GPIO1_IO17                 SC_P_MIPI_DSI0_I2C0_SDA            3
#define SC_P_MIPI_DSI0_GPIO0_00_MIPI_DSI0_GPIO0_IO00            SC_P_MIPI_DSI0_GPIO0_00            0
#define SC_P_MIPI_DSI0_GPIO0_00_MIPI_DSI0_PWM0_OUT              SC_P_MIPI_DSI0_GPIO0_00            1
#define SC_P_MIPI_DSI0_GPIO0_00_LSIO_GPIO1_IO18                 SC_P_MIPI_DSI0_GPIO0_00            3
#define SC_P_MIPI_DSI0_GPIO0_01_MIPI_DSI0_GPIO0_IO01            SC_P_MIPI_DSI0_GPIO0_01            0
#define SC_P_MIPI_DSI0_GPIO0_01_LSIO_GPIO1_IO19                 SC_P_MIPI_DSI0_GPIO0_01            3
#define SC_P_MIPI_CSI0_MCLK_OUT_MIPI_CSI0_ACM_MCLK_OUT          SC_P_MIPI_CSI0_MCLK_OUT            0
#define SC_P_MIPI_CSI0_MCLK_OUT_LSIO_GPIO1_IO24                 SC_P_MIPI_CSI0_MCLK_OUT            3
#define SC_P_MIPI_CSI0_I2C0_SCL_MIPI_CSI0_I2C0_SCL              SC_P_MIPI_CSI0_I2C0_SCL            0
#define SC_P_MIPI_CSI0_I2C0_SCL_LSIO_GPIO1_IO25                 SC_P_MIPI_CSI0_I2C0_SCL            3
#define SC_P_MIPI_CSI0_I2C0_SDA_MIPI_CSI0_I2C0_SDA              SC_P_MIPI_CSI0_I2C0_SDA            0
#define SC_P_MIPI_CSI0_I2C0_SDA_LSIO_GPIO1_IO26                 SC_P_MIPI_CSI0_I2C0_SDA            3
#define SC_P_MIPI_CSI0_GPIO0_00_MIPI_CSI0_GPIO0_IO00            SC_P_MIPI_CSI0_GPIO0_00            0
#define SC_P_MIPI_CSI0_GPIO0_00_DMA_I2C0_SCL                    SC_P_MIPI_CSI0_GPIO0_00            1
#define SC_P_MIPI_CSI0_GPIO0_00_MIPI_CSI1_I2C0_SCL              SC_P_MIPI_CSI0_GPIO0_00            2
#define SC_P_MIPI_CSI0_GPIO0_00_LSIO_GPIO1_IO27                 SC_P_MIPI_CSI0_GPIO0_00            3
#define SC_P_MIPI_CSI0_GPIO0_01_MIPI_CSI0_GPIO0_IO01            SC_P_MIPI_CSI0_GPIO0_01            0
#define SC_P_MIPI_CSI0_GPIO0_01_DMA_I2C0_SDA                    SC_P_MIPI_CSI0_GPIO0_01            1
#define SC_P_MIPI_CSI0_GPIO0_01_MIPI_CSI1_I2C0_SDA              SC_P_MIPI_CSI0_GPIO0_01            2
#define SC_P_MIPI_CSI0_GPIO0_01_LSIO_GPIO1_IO28                 SC_P_MIPI_CSI0_GPIO0_01            3
#define SC_P_MIPI_CSI1_MCLK_OUT_MIPI_CSI1_ACM_MCLK_OUT          SC_P_MIPI_CSI1_MCLK_OUT            0
#define SC_P_MIPI_CSI1_MCLK_OUT_LSIO_GPIO1_IO29                 SC_P_MIPI_CSI1_MCLK_OUT            3
#define SC_P_MIPI_CSI1_GPIO0_00_MIPI_CSI1_GPIO0_IO00            SC_P_MIPI_CSI1_GPIO0_00            0
#define SC_P_MIPI_CSI1_GPIO0_00_LSIO_GPIO1_IO30                 SC_P_MIPI_CSI1_GPIO0_00            3
#define SC_P_MIPI_CSI1_GPIO0_01_MIPI_CSI1_GPIO0_IO01            SC_P_MIPI_CSI1_GPIO0_01            0
#define SC_P_MIPI_CSI1_GPIO0_01_LSIO_GPIO1_IO31                 SC_P_MIPI_CSI1_GPIO0_01            3
#define SC_P_MIPI_CSI1_I2C0_SCL_MIPI_CSI1_I2C0_SCL              SC_P_MIPI_CSI1_I2C0_SCL            0
#define SC_P_MIPI_CSI1_I2C0_SCL_LSIO_GPIO2_IO00                 SC_P_MIPI_CSI1_I2C0_SCL            3
#define SC_P_MIPI_CSI1_I2C0_SDA_MIPI_CSI1_I2C0_SDA              SC_P_MIPI_CSI1_I2C0_SDA            0
#define SC_P_MIPI_CSI1_I2C0_SDA_LSIO_GPIO2_IO01                 SC_P_MIPI_CSI1_I2C0_SDA            3
#define SC_P_HDMI_TX0_TS_SCL_HDMI_TX0_I2C0_SCL                  SC_P_HDMI_TX0_TS_SCL               0
#define SC_P_HDMI_TX0_TS_SCL_DMA_I2C0_SCL                       SC_P_HDMI_TX0_TS_SCL               1
#define SC_P_HDMI_TX0_TS_SCL_DMA_I2C2_SCL                       SC_P_HDMI_TX0_TS_SCL               2
#define SC_P_HDMI_TX0_TS_SCL_LSIO_GPIO2_IO02                    SC_P_HDMI_TX0_TS_SCL               3
#define SC_P_HDMI_TX0_TS_SDA_HDMI_TX0_I2C0_SDA                  SC_P_HDMI_TX0_TS_SDA               0
#define SC_P_HDMI_TX0_TS_SDA_DMA_I2C0_SDA                       SC_P_HDMI_TX0_TS_SDA               1
#define SC_P_HDMI_TX0_TS_SDA_DMA_I2C2_SDA                       SC_P_HDMI_TX0_TS_SDA               2
#define SC_P_HDMI_TX0_TS_SDA_LSIO_GPIO2_IO03                    SC_P_HDMI_TX0_TS_SDA               3
#define SC_P_ESAI1_FSR_AUD_ESAI1_FSR                            SC_P_ESAI1_FSR                     0
#define SC_P_ESAI1_FSR_ADMA_LCDIF_D00                           SC_P_ESAI1_FSR                     2
#define SC_P_ESAI1_FSR_LSIO_GPIO2_IO04                          SC_P_ESAI1_FSR                     3
#define SC_P_ESAI1_FST_AUD_ESAI1_FST                            SC_P_ESAI1_FST                     0
#define SC_P_ESAI1_FST_AUD_SPDIF0_EXT_CLK                       SC_P_ESAI1_FST                     1
#define SC_P_ESAI1_FST_ADMA_LCDIF_D16                           SC_P_ESAI1_FST                     2
#define SC_P_ESAI1_FST_LSIO_GPIO2_IO05                          SC_P_ESAI1_FST                     3
#define SC_P_ESAI1_SCKR_AUD_ESAI1_SCKR                          SC_P_ESAI1_SCKR                    0
#define SC_P_ESAI1_SCKR_ADMA_LCDIF_D01                          SC_P_ESAI1_SCKR                    2
#define SC_P_ESAI1_SCKR_LSIO_GPIO2_IO06                         SC_P_ESAI1_SCKR                    3
#define SC_P_ESAI1_SCKT_AUD_ESAI1_SCKT                          SC_P_ESAI1_SCKT                    0
#define SC_P_ESAI1_SCKT_AUD_SAI2_RXC                            SC_P_ESAI1_SCKT                    1
#define SC_P_ESAI1_SCKT_AUD_SPDIF0_EXT_CLK                      SC_P_ESAI1_SCKT                    2
#define SC_P_ESAI1_SCKT_LSIO_GPIO2_IO07                         SC_P_ESAI1_SCKT                    3
#define SC_P_ESAI1_TX0_AUD_ESAI1_TX0                            SC_P_ESAI1_TX0                     0
#define SC_P_ESAI1_TX0_AUD_SAI2_RXD                             SC_P_ESAI1_TX0                     1
#define SC_P_ESAI1_TX0_AUD_SPDIF0_RX                            SC_P_ESAI1_TX0                     2
#define SC_P_ESAI1_TX0_LSIO_GPIO2_IO08                          SC_P_ESAI1_TX0                     3
#define SC_P_ESAI1_TX1_AUD_ESAI1_TX1                            SC_P_ESAI1_TX1                     0
#define SC_P_ESAI1_TX1_AUD_SAI2_RXFS                            SC_P_ESAI1_TX1                     1
#define SC_P_ESAI1_TX1_AUD_SPDIF0_TX                            SC_P_ESAI1_TX1                     2
#define SC_P_ESAI1_TX1_LSIO_GPIO2_IO09                          SC_P_ESAI1_TX1                     3
#define SC_P_ESAI1_TX2_RX3_AUD_ESAI1_TX2_RX3                    SC_P_ESAI1_TX2_RX3                 0
#define SC_P_ESAI1_TX2_RX3_AUD_SPDIF0_RX                        SC_P_ESAI1_TX2_RX3                 1
#define SC_P_ESAI1_TX2_RX3_ADMA_LCDIF_D17                       SC_P_ESAI1_TX2_RX3                 2
#define SC_P_ESAI1_TX2_RX3_LSIO_GPIO2_IO10                      SC_P_ESAI1_TX2_RX3                 3
#define SC_P_ESAI1_TX3_RX2_AUD_ESAI1_TX3_RX2                    SC_P_ESAI1_TX3_RX2                 0
#define SC_P_ESAI1_TX3_RX2_AUD_SPDIF0_TX                        SC_P_ESAI1_TX3_RX2                 1
#define SC_P_ESAI1_TX3_RX2_ADMA_LCDIF_RESET                     SC_P_ESAI1_TX3_RX2                 2
#define SC_P_ESAI1_TX3_RX2_LSIO_GPIO2_IO11                      SC_P_ESAI1_TX3_RX2                 3
#define SC_P_ESAI1_TX4_RX1_AUD_ESAI1_TX4_RX1                    SC_P_ESAI1_TX4_RX1                 0
#define SC_P_ESAI1_TX4_RX1_ADMA_LCDIF_D02                       SC_P_ESAI1_TX4_RX1                 2
#define SC_P_ESAI1_TX4_RX1_LSIO_GPIO2_IO12                      SC_P_ESAI1_TX4_RX1                 3
#define SC_P_ESAI1_TX5_RX0_AUD_ESAI1_TX5_RX0                    SC_P_ESAI1_TX5_RX0                 0
#define SC_P_ESAI1_TX5_RX0_ADMA_LCDIF_D03                       SC_P_ESAI1_TX5_RX0                 2
#define SC_P_ESAI1_TX5_RX0_LSIO_GPIO2_IO13                      SC_P_ESAI1_TX5_RX0                 3
#define SC_P_MCLK_IN0_AUD_ACM_MCLK_IN0                          SC_P_MCLK_IN0                      0
#define SC_P_MCLK_IN0_AUD_ESAI1_RX_HF_CLK                       SC_P_MCLK_IN0                      2
#define SC_P_MCLK_IN0_LSIO_GPIO3_IO00                           SC_P_MCLK_IN0                      3
#define SC_P_MCLK_IN0_ADMA_LCDIF_EN                             SC_P_MCLK_IN0                      4
#define SC_P_MCLK_OUT0_AUD_ACM_MCLK_OUT0                        SC_P_MCLK_OUT0                     0
#define SC_P_MCLK_OUT0_AUD_ESAI1_TX_HF_CLK                      SC_P_MCLK_OUT0                     2
#define SC_P_MCLK_OUT0_LSIO_GPIO3_IO01                          SC_P_MCLK_OUT0                     3
#define SC_P_SPI0_SCK_DMA_SPI0_SCK                              SC_P_SPI0_SCK                      0
#define SC_P_SPI0_SCK_AUD_SAI0_RXC                              SC_P_SPI0_SCK                      1
#define SC_P_SPI0_SCK_ADMA_LCDIF_D04                            SC_P_SPI0_SCK                      2
#define SC_P_SPI0_SCK_LSIO_GPIO3_IO02                           SC_P_SPI0_SCK                      3
#define SC_P_SPI0_SDO_DMA_SPI0_SDO                              SC_P_SPI0_SDO                      0
#define SC_P_SPI0_SDO_AUD_SAI0_TXD                              SC_P_SPI0_SDO                      1
#define SC_P_SPI0_SDO_ADMA_LCDIF_D05                            SC_P_SPI0_SDO                      2
#define SC_P_SPI0_SDO_LSIO_GPIO3_IO03                           SC_P_SPI0_SDO                      3
#define SC_P_SPI0_SDI_DMA_SPI0_SDI                              SC_P_SPI0_SDI                      0
#define SC_P_SPI0_SDI_AUD_SAI0_RXD                              SC_P_SPI0_SDI                      1
#define SC_P_SPI0_SDI_ADMA_LCDIF_D06                            SC_P_SPI0_SDI                      2
#define SC_P_SPI0_SDI_LSIO_GPIO3_IO04                           SC_P_SPI0_SDI                      3
#define SC_P_SPI0_CS0_DMA_SPI0_CS0                              SC_P_SPI0_CS0                      0
#define SC_P_SPI0_CS0_AUD_SAI0_RXFS                             SC_P_SPI0_CS0                      1
#define SC_P_SPI0_CS0_ADMA_LCDIF_D07                            SC_P_SPI0_CS0                      2
#define SC_P_SPI0_CS0_LSIO_GPIO3_IO05                           SC_P_SPI0_CS0                      3
#define SC_P_SPI0_CS1_DMA_SPI0_CS1                              SC_P_SPI0_CS1                      0
#define SC_P_SPI0_CS1_AUD_SAI0_TXC                              SC_P_SPI0_CS1                      1
#define SC_P_SPI0_CS1_ADMA_LCDIF_D08                            SC_P_SPI0_CS1                      2
#define SC_P_SPI0_CS1_LSIO_GPIO3_IO06                           SC_P_SPI0_CS1                      3
#define SC_P_SPI2_SCK_DMA_SPI2_SCK                              SC_P_SPI2_SCK                      0
#define SC_P_SPI2_SCK_DMA_DMA0_REQ_IN0                          SC_P_SPI2_SCK                      1
#define SC_P_SPI2_SCK_ADMA_LCDIF_D09                            SC_P_SPI2_SCK                      2
#define SC_P_SPI2_SCK_LSIO_GPIO3_IO07                           SC_P_SPI2_SCK                      3
#define SC_P_SPI2_SDO_DMA_SPI2_SDO                              SC_P_SPI2_SDO                      0
#define SC_P_SPI2_SDO_DMA_FTM_CH0                               SC_P_SPI2_SDO                      1
#define SC_P_SPI2_SDO_ADMA_LCDIF_D10                            SC_P_SPI2_SDO                      2
#define SC_P_SPI2_SDO_LSIO_GPIO3_IO08                           SC_P_SPI2_SDO                      3
#define SC_P_SPI2_SDI_DMA_SPI2_SDI                              SC_P_SPI2_SDI                      0
#define SC_P_SPI2_SDI_DMA_FTM_CH1                               SC_P_SPI2_SDI                      1
#define SC_P_SPI2_SDI_ADMA_LCDIF_D11                            SC_P_SPI2_SDI                      2
#define SC_P_SPI2_SDI_LSIO_GPIO3_IO09                           SC_P_SPI2_SDI                      3
#define SC_P_SPI2_CS0_DMA_SPI2_CS0                              SC_P_SPI2_CS0                      0
#define SC_P_SPI2_CS0_DMA_FTM_CH2                               SC_P_SPI2_CS0                      1
#define SC_P_SPI2_CS0_ADMA_LCDIF_D12                            SC_P_SPI2_CS0                      2
#define SC_P_SPI2_CS0_LSIO_GPIO3_IO10                           SC_P_SPI2_CS0                      3
#define SC_P_SPI2_CS1_DMA_SPI2_CS1                              SC_P_SPI2_CS1                      0
#define SC_P_SPI2_CS1_AUD_SAI0_TXFS                             SC_P_SPI2_CS1                      1
#define SC_P_SPI2_CS1_ADMA_LCDIF_D13                            SC_P_SPI2_CS1                      2
#define SC_P_SPI2_CS1_LSIO_GPIO3_IO11                           SC_P_SPI2_CS1                      3
#define SC_P_SAI1_RXC_AUD_SAI1_RXC                              SC_P_SAI1_RXC                      0
#define SC_P_SAI1_RXC_AUD_SAI0_TXD                              SC_P_SAI1_RXC                      1
#define SC_P_SAI1_RXC_ADMA_LCDIF_HSYNC                          SC_P_SAI1_RXC                      2
#define SC_P_SAI1_RXC_LSIO_GPIO3_IO12                           SC_P_SAI1_RXC                      3
#define SC_P_SAI1_RXD_AUD_SAI1_RXD                              SC_P_SAI1_RXD                      0
#define SC_P_SAI1_RXD_AUD_SAI0_TXFS                             SC_P_SAI1_RXD                      1
#define SC_P_SAI1_RXD_ADMA_LCDIF_D14                            SC_P_SAI1_RXD                      2
#define SC_P_SAI1_RXD_LSIO_GPIO3_IO13                           SC_P_SAI1_RXD                      3
#define SC_P_SAI1_RXFS_AUD_SAI1_RXFS                            SC_P_SAI1_RXFS                     0
#define SC_P_SAI1_RXFS_AUD_SAI0_RXD                             SC_P_SAI1_RXFS                     1
#define SC_P_SAI1_RXFS_ADMA_LCDIF_EN                            SC_P_SAI1_RXFS                     2
#define SC_P_SAI1_RXFS_LSIO_GPIO3_IO14                          SC_P_SAI1_RXFS                     3
#define SC_P_SAI1_TXC_AUD_SAI1_TXC                              SC_P_SAI1_TXC                      0
#define SC_P_SAI1_TXC_AUD_SAI0_TXC                              SC_P_SAI1_TXC                      1
#define SC_P_SAI1_TXC_ADMA_LCDIF_VSYNC                          SC_P_SAI1_TXC                      2
#define SC_P_SAI1_TXC_LSIO_GPIO3_IO15                           SC_P_SAI1_TXC                      3
#define SC_P_SAI1_TXD_AUD_SAI1_TXD                              SC_P_SAI1_TXD                      0
#define SC_P_SAI1_TXD_AUD_SAI1_RXC                              SC_P_SAI1_TXD                      1
#define SC_P_SAI1_TXD_ADMA_LCDIF_CLK                            SC_P_SAI1_TXD                      2
#define SC_P_SAI1_TXD_LSIO_GPIO3_IO16                           SC_P_SAI1_TXD                      3
#define SC_P_SAI1_TXFS_AUD_SAI1_TXFS                            SC_P_SAI1_TXFS                     0
#define SC_P_SAI1_TXFS_AUD_SAI1_RXFS                            SC_P_SAI1_TXFS                     1
#define SC_P_SAI1_TXFS_ADMA_LCDIF_D15                           SC_P_SAI1_TXFS                     2
#define SC_P_SAI1_TXFS_LSIO_GPIO3_IO17                          SC_P_SAI1_TXFS                     3
#define SC_P_ADC_IN0_DMA_ADC_IN0                                SC_P_ADC_IN0                       0
#define SC_P_ADC_IN0_ADMA_LCDIF_D08                             SC_P_ADC_IN0                       1
#define SC_P_ADC_IN0_LSIO_KPP0_COL0                             SC_P_ADC_IN0                       2
#define SC_P_ADC_IN0_LSIO_GPIO3_IO18                            SC_P_ADC_IN0                       3
#define SC_P_ADC_IN1_DMA_ADC_IN1                                SC_P_ADC_IN1                       0
#define SC_P_ADC_IN1_ADMA_LCDIF_D13                             SC_P_ADC_IN1                       1
#define SC_P_ADC_IN1_LSIO_KPP0_COL1                             SC_P_ADC_IN1                       2
#define SC_P_ADC_IN1_LSIO_GPIO3_IO19                            SC_P_ADC_IN1                       3
#define SC_P_ADC_IN2_DMA_ADC_IN2                                SC_P_ADC_IN2                       0
#define SC_P_ADC_IN2_LSIO_KPP0_COL2                             SC_P_ADC_IN2                       2
#define SC_P_ADC_IN2_LSIO_GPIO3_IO20                            SC_P_ADC_IN2                       3
#define SC_P_ADC_IN3_DMA_ADC_IN3                                SC_P_ADC_IN3                       0
#define SC_P_ADC_IN3_DMA_SPI1_SCK                               SC_P_ADC_IN3                       1
#define SC_P_ADC_IN3_LSIO_KPP0_COL3                             SC_P_ADC_IN3                       2
#define SC_P_ADC_IN3_LSIO_GPIO3_IO21                            SC_P_ADC_IN3                       3
#define SC_P_ADC_IN3_ADMA_LCDIF_D14                             SC_P_ADC_IN3                       4
#define SC_P_ADC_IN4_DMA_ADC_IN4                                SC_P_ADC_IN4                       0
#define SC_P_ADC_IN4_DMA_SPI1_SDO                               SC_P_ADC_IN4                       1
#define SC_P_ADC_IN4_LSIO_KPP0_ROW0                             SC_P_ADC_IN4                       2
#define SC_P_ADC_IN4_LSIO_GPIO3_IO22                            SC_P_ADC_IN4                       3
#define SC_P_ADC_IN4_ADMA_LCDIF_D15                             SC_P_ADC_IN4                       4
#define SC_P_ADC_IN5_DMA_ADC_IN5                                SC_P_ADC_IN5                       0
#define SC_P_ADC_IN5_DMA_SPI1_SDI                               SC_P_ADC_IN5                       1
#define SC_P_ADC_IN5_LSIO_KPP0_ROW1                             SC_P_ADC_IN5                       2
#define SC_P_ADC_IN5_LSIO_GPIO3_IO23                            SC_P_ADC_IN5                       3
#define SC_P_ADC_IN5_ADMA_LCDIF_CLK                             SC_P_ADC_IN5                       4
#define SC_P_LSIO_GPIO3_24_DMA_ADC_IN6_dummy                    SC_P_LSIO_GPIO3_24                 0
#define SC_P_LSIO_GPIO3_24_DMA_SPI1_CS0                         SC_P_LSIO_GPIO3_24                 1
#define SC_P_LSIO_GPIO3_24_LSIO_KPP0_ROW2                       SC_P_LSIO_GPIO3_24                 2
#define SC_P_LSIO_GPIO3_24_LSIO_GPIO3_IO24                      SC_P_LSIO_GPIO3_24                 3
#define SC_P_LSIO_GPIO3_24_ADMA_LCDIF_HSYNC                     SC_P_LSIO_GPIO3_24                 4
#define SC_P_LSIO_GPIO3_25_DMA_ADC_IN7_dummy                    SC_P_LSIO_GPIO3_25                 0
#define SC_P_LSIO_GPIO3_25_DMA_SPI1_CS1                         SC_P_LSIO_GPIO3_25                 1
#define SC_P_LSIO_GPIO3_25_LSIO_KPP0_ROW3                       SC_P_LSIO_GPIO3_25                 2
#define SC_P_LSIO_GPIO3_25_LSIO_GPIO3_IO25                      SC_P_LSIO_GPIO3_25                 3
#define SC_P_LSIO_GPIO3_25_ADMA_LCDIF_VSYNC                     SC_P_LSIO_GPIO3_25                 4
#define SC_P_FLEXCAN0_RX_DMA_FLEXCAN0_RX                        SC_P_FLEXCAN0_RX                   0
#define SC_P_FLEXCAN0_RX_LSIO_GPIO3_IO29                        SC_P_FLEXCAN0_RX                   3
#define SC_P_FLEXCAN0_TX_DMA_FLEXCAN0_TX                        SC_P_FLEXCAN0_TX                   0
#define SC_P_FLEXCAN0_TX_LSIO_GPIO3_IO30                        SC_P_FLEXCAN0_TX                   3
#define SC_P_FLEXCAN1_RX_DMA_FLEXCAN1_RX                        SC_P_FLEXCAN1_RX                   0
#define SC_P_FLEXCAN1_RX_LSIO_GPIO3_IO31                        SC_P_FLEXCAN1_RX                   3
#define SC_P_FLEXCAN1_TX_DMA_FLEXCAN1_TX                        SC_P_FLEXCAN1_TX                   0
#define SC_P_FLEXCAN1_TX_LSIO_GPIO4_IO00                        SC_P_FLEXCAN1_TX                   3
#define SC_P_FLEXCAN2_RX_DMA_FLEXCAN2_RX                        SC_P_FLEXCAN2_RX                   0
#define SC_P_FLEXCAN2_RX_LSIO_GPIO4_IO01                        SC_P_FLEXCAN2_RX                   3
#define SC_P_FLEXCAN2_TX_DMA_FLEXCAN2_TX                        SC_P_FLEXCAN2_TX                   0
#define SC_P_FLEXCAN2_TX_LSIO_GPIO4_IO02                        SC_P_FLEXCAN2_TX                   3
#define SC_P_MLB_SIG_CONN_MLB_SIG                               SC_P_MLB_SIG                       0
#define SC_P_MLB_SIG_AUD_SAI3_RXC                               SC_P_MLB_SIG                       1
#define SC_P_MLB_SIG_LSIO_GPIO3_IO26                            SC_P_MLB_SIG                       3
#define SC_P_MLB_CLK_CONN_MLB_CLK                               SC_P_MLB_CLK                       0
#define SC_P_MLB_CLK_AUD_SAI3_RXFS                              SC_P_MLB_CLK                       1
#define SC_P_MLB_CLK_LSIO_GPIO3_IO27                            SC_P_MLB_CLK                       3
#define SC_P_MLB_DATA_CONN_MLB_DATA                             SC_P_MLB_DATA                      0
#define SC_P_MLB_DATA_AUD_SAI3_RXD                              SC_P_MLB_DATA                      1
#define SC_P_MLB_DATA_LSIO_GPIO3_IO28                           SC_P_MLB_DATA                      3
#define SC_P_USB_SS3_TC0_DMA_I2C1_SCL                           SC_P_USB_SS3_TC0                   0
#define SC_P_USB_SS3_TC0_CONN_USB_OTG1_PWR                      SC_P_USB_SS3_TC0                   1
#define SC_P_USB_SS3_TC0_LSIO_QSPI1A_DATA1                      SC_P_USB_SS3_TC0                   2
#define SC_P_USB_SS3_TC0_LSIO_GPIO4_IO03                        SC_P_USB_SS3_TC0                   3
#define SC_P_USB_SS3_TC1_DMA_I2C1_SCL                           SC_P_USB_SS3_TC1                   0
#define SC_P_USB_SS3_TC1_CONN_USB_OTG2_PWR                      SC_P_USB_SS3_TC1                   1
#define SC_P_USB_SS3_TC1_LSIO_QSPI1A_DATA2                      SC_P_USB_SS3_TC1                   2
#define SC_P_USB_SS3_TC1_LSIO_GPIO4_IO04                        SC_P_USB_SS3_TC1                   3
#define SC_P_USB_SS3_TC2_DMA_I2C1_SDA                           SC_P_USB_SS3_TC2                   0
#define SC_P_USB_SS3_TC2_CONN_USB_OTG1_OC                       SC_P_USB_SS3_TC2                   1
#define SC_P_USB_SS3_TC2_LSIO_QSPI1A_DQS                        SC_P_USB_SS3_TC2                   2
#define SC_P_USB_SS3_TC2_LSIO_GPIO4_IO05                        SC_P_USB_SS3_TC2                   3
#define SC_P_USB_SS3_TC3_DMA_I2C1_SDA                           SC_P_USB_SS3_TC3                   0
#define SC_P_USB_SS3_TC3_CONN_USB_OTG2_OC                       SC_P_USB_SS3_TC3                   1
#define SC_P_USB_SS3_TC3_LSIO_QSPI1A_DATA3                      SC_P_USB_SS3_TC3                   2
#define SC_P_USB_SS3_TC3_LSIO_GPIO4_IO06                        SC_P_USB_SS3_TC3                   3
#define SC_P_ENET0_MDIO_CONN_ENET0_MDIO                         SC_P_ENET0_MDIO                    0
#define SC_P_ENET0_MDIO_DMA_I2C2_SDA                            SC_P_ENET0_MDIO                    1
#define SC_P_ENET0_MDIO_LSIO_GPIO4_IO13                         SC_P_ENET0_MDIO                    3
#define SC_P_ENET0_MDC_CONN_ENET0_MDC                           SC_P_ENET0_MDC                     0
#define SC_P_ENET0_MDC_DMA_I2C2_SCL                             SC_P_ENET0_MDC                     1
#define SC_P_ENET0_MDC_LSIO_GPIO4_IO14                          SC_P_ENET0_MDC                     3
#define SC_P_ENET0_REFCLK_125M_25M_CONN_ENET0_REFCLK_125M_25M   SC_P_ENET0_REFCLK_125M_25M         0
#define SC_P_ENET0_REFCLK_125M_25M_CONN_ENET0_PPS               SC_P_ENET0_REFCLK_125M_25M         1
#define SC_P_ENET0_REFCLK_125M_25M_LSIO_GPIO4_IO15              SC_P_ENET0_REFCLK_125M_25M         3
#define SC_P_ENET1_REFCLK_125M_25M_CONN_ENET1_REFCLK_125M_25M   SC_P_ENET1_REFCLK_125M_25M         0
#define SC_P_ENET1_REFCLK_125M_25M_CONN_ENET1_PPS               SC_P_ENET1_REFCLK_125M_25M         1
#define SC_P_ENET1_REFCLK_125M_25M_LSIO_GPIO4_IO16              SC_P_ENET1_REFCLK_125M_25M         3
#define SC_P_ENET1_MDIO_CONN_ENET1_MDIO                         SC_P_ENET1_MDIO                    0
#define SC_P_ENET1_MDIO_DMA_I2C3_SDA                            SC_P_ENET1_MDIO                    1
#define SC_P_ENET1_MDIO_LSIO_GPIO4_IO17                         SC_P_ENET1_MDIO                    3
#define SC_P_ENET1_MDC_CONN_ENET1_MDC                           SC_P_ENET1_MDC                     0
#define SC_P_ENET1_MDC_DMA_I2C3_SCL                             SC_P_ENET1_MDC                     1
#define SC_P_ENET1_MDC_LSIO_GPIO4_IO18                          SC_P_ENET1_MDC                     3
#define SC_P_QSPI1A_SS0_B_LSIO_QSPI1A_SS0_B                     SC_P_QSPI1A_SS0_B                  0
#define SC_P_QSPI1A_SS0_B_LSIO_GPIO4_IO19                       SC_P_QSPI1A_SS0_B                  3
#define SC_P_QSPI1A_SCLK_LSIO_QSPI1A_SCLK                       SC_P_QSPI1A_SCLK                   0
#define SC_P_QSPI1A_SCLK_LSIO_GPIO4_IO21                        SC_P_QSPI1A_SCLK                   3
#define SC_P_QSPI1A_DATA0_LSIO_QSPI1A_DATA0                     SC_P_QSPI1A_DATA0                  0
#define SC_P_QSPI1A_DATA0_LSIO_GPIO4_IO26                       SC_P_QSPI1A_DATA0                  3
#define SC_P_QSPI0A_DATA0_LSIO_QSPI0A_DATA0                     SC_P_QSPI0A_DATA0                  0
#define SC_P_QSPI0A_DATA1_LSIO_QSPI0A_DATA1                     SC_P_QSPI0A_DATA1                  0
#define SC_P_QSPI0A_DATA2_LSIO_QSPI0A_DATA2                     SC_P_QSPI0A_DATA2                  0
#define SC_P_QSPI0A_DATA3_LSIO_QSPI0A_DATA3                     SC_P_QSPI0A_DATA3                  0
#define SC_P_QSPI0A_DQS_LSIO_QSPI0A_DQS                         SC_P_QSPI0A_DQS                    0
#define SC_P_QSPI0A_SS0_B_LSIO_QSPI0A_SS0_B                     SC_P_QSPI0A_SS0_B                  0
#define SC_P_QSPI0A_SCLK_LSIO_QSPI0A_SCLK                       SC_P_QSPI0A_SCLK                   0
#define SC_P_QSPI0B_SCLK_LSIO_QSPI0B_SCLK                       SC_P_QSPI0B_SCLK                   0
#define SC_P_QSPI0B_DATA0_LSIO_QSPI0B_DATA0                     SC_P_QSPI0B_DATA0                  0
#define SC_P_QSPI0B_DATA1_LSIO_QSPI0B_DATA1                     SC_P_QSPI0B_DATA1                  0
#define SC_P_QSPI0B_DATA2_LSIO_QSPI0B_DATA2                     SC_P_QSPI0B_DATA2                  0
#define SC_P_QSPI0B_DATA3_LSIO_QSPI0B_DATA3                     SC_P_QSPI0B_DATA3                  0
#define SC_P_QSPI0B_DQS_LSIO_QSPI0B_DQS                         SC_P_QSPI0B_DQS                    0
#define SC_P_QSPI0B_SS0_B_LSIO_QSPI0B_SS0_B                     SC_P_QSPI0B_SS0_B                  0
#define SC_P_PCIE_CTRL0_CLKREQ_B_HSIO_PCIE0_CLKREQ_B            SC_P_PCIE_CTRL0_CLKREQ_B           0
#define SC_P_PCIE_CTRL0_CLKREQ_B_LSIO_GPIO4_IO27                SC_P_PCIE_CTRL0_CLKREQ_B           3
#define SC_P_PCIE_CTRL0_WAKE_B_HSIO_PCIE0_WAKE_B                SC_P_PCIE_CTRL0_WAKE_B             0
#define SC_P_PCIE_CTRL0_WAKE_B_LSIO_GPIO4_IO28                  SC_P_PCIE_CTRL0_WAKE_B             3
#define SC_P_PCIE_CTRL0_PERST_B_HSIO_PCIE0_PERST_B              SC_P_PCIE_CTRL0_PERST_B            0
#define SC_P_PCIE_CTRL0_PERST_B_LSIO_GPIO4_IO29                 SC_P_PCIE_CTRL0_PERST_B            3
#define SC_P_PCIE_CTRL1_CLKREQ_B_HSIO_PCIE1_CLKREQ_B            SC_P_PCIE_CTRL1_CLKREQ_B           0
#define SC_P_PCIE_CTRL1_CLKREQ_B_DMA_I2C1_SDA                   SC_P_PCIE_CTRL1_CLKREQ_B           1
#define SC_P_PCIE_CTRL1_CLKREQ_B_CONN_USB_OTG2_OC               SC_P_PCIE_CTRL1_CLKREQ_B           2
#define SC_P_PCIE_CTRL1_CLKREQ_B_LSIO_GPIO4_IO30                SC_P_PCIE_CTRL1_CLKREQ_B           3
#define SC_P_PCIE_CTRL1_WAKE_B_HSIO_PCIE1_WAKE_B                SC_P_PCIE_CTRL1_WAKE_B             0
#define SC_P_PCIE_CTRL1_WAKE_B_DMA_I2C1_SCL                     SC_P_PCIE_CTRL1_WAKE_B             1
#define SC_P_PCIE_CTRL1_WAKE_B_CONN_USB_OTG2_PWR                SC_P_PCIE_CTRL1_WAKE_B             2
#define SC_P_PCIE_CTRL1_WAKE_B_LSIO_GPIO4_IO31                  SC_P_PCIE_CTRL1_WAKE_B             3
#define SC_P_PCIE_CTRL1_PERST_B_HSIO_PCIE1_PERST_B              SC_P_PCIE_CTRL1_PERST_B            0
#define SC_P_PCIE_CTRL1_PERST_B_DMA_I2C1_SCL                    SC_P_PCIE_CTRL1_PERST_B            1
#define SC_P_PCIE_CTRL1_PERST_B_CONN_USB_OTG1_PWR               SC_P_PCIE_CTRL1_PERST_B            2
#define SC_P_PCIE_CTRL1_PERST_B_LSIO_GPIO5_IO00                 SC_P_PCIE_CTRL1_PERST_B            3
#define SC_P_EMMC0_CLK_CONN_EMMC0_CLK                           SC_P_EMMC0_CLK                     0
#define SC_P_EMMC0_CLK_CONN_NAND_READY_B                        SC_P_EMMC0_CLK                     1
#define SC_P_EMMC0_CMD_CONN_EMMC0_CMD                           SC_P_EMMC0_CMD                     0
#define SC_P_EMMC0_CMD_CONN_NAND_DQS                            SC_P_EMMC0_CMD                     1
#define SC_P_EMMC0_CMD_AUD_MQS_R                                SC_P_EMMC0_CMD                     2
#define SC_P_EMMC0_CMD_LSIO_GPIO5_IO03                          SC_P_EMMC0_CMD                     3
#define SC_P_EMMC0_DATA0_CONN_EMMC0_DATA0                       SC_P_EMMC0_DATA0                   0
#define SC_P_EMMC0_DATA0_CONN_NAND_DATA00                       SC_P_EMMC0_DATA0                   1
#define SC_P_EMMC0_DATA0_LSIO_GPIO5_IO04                        SC_P_EMMC0_DATA0                   3
#define SC_P_EMMC0_DATA1_CONN_EMMC0_DATA1                       SC_P_EMMC0_DATA1                   0
#define SC_P_EMMC0_DATA1_CONN_NAND_DATA01                       SC_P_EMMC0_DATA1                   1
#define SC_P_EMMC0_DATA1_LSIO_GPIO5_IO05                        SC_P_EMMC0_DATA1                   3
#define SC_P_EMMC0_DATA2_CONN_EMMC0_DATA2                       SC_P_EMMC0_DATA2                   0
#define SC_P_EMMC0_DATA2_CONN_NAND_DATA02                       SC_P_EMMC0_DATA2                   1
#define SC_P_EMMC0_DATA2_LSIO_GPIO5_IO06                        SC_P_EMMC0_DATA2                   3
#define SC_P_EMMC0_DATA3_CONN_EMMC0_DATA3                       SC_P_EMMC0_DATA3                   0
#define SC_P_EMMC0_DATA3_CONN_NAND_DATA03                       SC_P_EMMC0_DATA3                   1
#define SC_P_EMMC0_DATA3_LSIO_GPIO5_IO07                        SC_P_EMMC0_DATA3                   3
#define SC_P_EMMC0_DATA4_CONN_EMMC0_DATA4                       SC_P_EMMC0_DATA4                   0
#define SC_P_EMMC0_DATA4_CONN_NAND_DATA04                       SC_P_EMMC0_DATA4                   1
#define SC_P_EMMC0_DATA4_LSIO_GPIO5_IO08                        SC_P_EMMC0_DATA4                   3
#define SC_P_EMMC0_DATA5_CONN_EMMC0_DATA5                       SC_P_EMMC0_DATA5                   0
#define SC_P_EMMC0_DATA5_CONN_NAND_DATA05                       SC_P_EMMC0_DATA5                   1
#define SC_P_EMMC0_DATA5_LSIO_GPIO5_IO09                        SC_P_EMMC0_DATA5                   3
#define SC_P_EMMC0_DATA6_CONN_EMMC0_DATA6                       SC_P_EMMC0_DATA6                   0
#define SC_P_EMMC0_DATA6_CONN_NAND_DATA06                       SC_P_EMMC0_DATA6                   1
#define SC_P_EMMC0_DATA6_LSIO_GPIO5_IO10                        SC_P_EMMC0_DATA6                   3
#define SC_P_EMMC0_DATA7_CONN_EMMC0_DATA7                       SC_P_EMMC0_DATA7                   0
#define SC_P_EMMC0_DATA7_CONN_NAND_DATA07                       SC_P_EMMC0_DATA7                   1
#define SC_P_EMMC0_DATA7_LSIO_GPIO5_IO11                        SC_P_EMMC0_DATA7                   3
#define SC_P_EMMC0_STROBE_CONN_EMMC0_STROBE                     SC_P_EMMC0_STROBE                  0
#define SC_P_EMMC0_STROBE_CONN_NAND_CLE                         SC_P_EMMC0_STROBE                  1
#define SC_P_EMMC0_STROBE_LSIO_GPIO5_IO12                       SC_P_EMMC0_STROBE                  3
#define SC_P_EMMC0_RESET_B_CONN_EMMC0_RESET_B                   SC_P_EMMC0_RESET_B                 0
#define SC_P_EMMC0_RESET_B_CONN_NAND_WP_B                       SC_P_EMMC0_RESET_B                 1
#define SC_P_EMMC0_RESET_B_CONN_USDHC1_VSELECT                  SC_P_EMMC0_RESET_B                 2
#define SC_P_EMMC0_RESET_B_LSIO_GPIO5_IO13                      SC_P_EMMC0_RESET_B                 3
#define SC_P_USDHC1_CLK_CONN_USDHC1_CLK                         SC_P_USDHC1_CLK                    0
#define SC_P_USDHC1_CLK_AUD_MQS_R                               SC_P_USDHC1_CLK                    1
#define SC_P_USDHC1_DATA1_CONN_USDHC1_DATA1                     SC_P_USDHC1_DATA1                  0
#define SC_P_USDHC1_DATA1_CONN_NAND_RE_P                        SC_P_USDHC1_DATA1                  1
#define SC_P_USDHC1_DATA1_LSIO_GPIO5_IO16                       SC_P_USDHC1_DATA1                  3
#define SC_P_USDHC1_DATA0_CONN_USDHC1_DATA0                     SC_P_USDHC1_DATA0                  0
#define SC_P_USDHC1_DATA0_CONN_NAND_RE_N                        SC_P_USDHC1_DATA0                  1
#define SC_P_USDHC1_DATA0_LSIO_GPIO5_IO15                       SC_P_USDHC1_DATA0                  3
#define SC_P_USDHC1_CMD_CONN_USDHC1_CMD                         SC_P_USDHC1_CMD                    0
#define SC_P_USDHC1_CMD_AUD_MQS_L                               SC_P_USDHC1_CMD                    1
#define SC_P_USDHC1_CMD_LSIO_GPIO5_IO14                         SC_P_USDHC1_CMD                    3
#define SC_P_USDHC1_DATA4_CONN_USDHC1_DATA4                     SC_P_USDHC1_DATA4                  0
#define SC_P_USDHC1_DATA4_CONN_NAND_CE0_B                       SC_P_USDHC1_DATA4                  1
#define SC_P_USDHC1_DATA4_AUD_MQS_R                             SC_P_USDHC1_DATA4                  2
#define SC_P_USDHC1_DATA4_LSIO_GPIO5_IO19                       SC_P_USDHC1_DATA4                  3
#define SC_P_USDHC1_DATA3_CONN_USDHC1_DATA3                     SC_P_USDHC1_DATA3                  0
#define SC_P_USDHC1_DATA3_CONN_NAND_DQS_P                       SC_P_USDHC1_DATA3                  1
#define SC_P_USDHC1_DATA3_LSIO_GPIO5_IO18                       SC_P_USDHC1_DATA3                  3
#define SC_P_USDHC1_DATA2_CONN_USDHC1_DATA2                     SC_P_USDHC1_DATA2                  0
#define SC_P_USDHC1_DATA2_CONN_NAND_DQS_N                       SC_P_USDHC1_DATA2                  1
#define SC_P_USDHC1_DATA2_LSIO_GPIO5_IO17                       SC_P_USDHC1_DATA2                  3
#define SC_P_USDHC1_DATA5_CONN_USDHC1_DATA5                     SC_P_USDHC1_DATA5                  0
#define SC_P_USDHC1_DATA5_CONN_NAND_RE_B                        SC_P_USDHC1_DATA5                  1
#define SC_P_USDHC1_DATA5_AUD_MQS_L                             SC_P_USDHC1_DATA5                  2
#define SC_P_USDHC1_DATA5_LSIO_GPIO5_IO20                       SC_P_USDHC1_DATA5                  3
#define SC_P_USDHC1_DATA6_CONN_USDHC1_DATA6                     SC_P_USDHC1_DATA6                  0
#define SC_P_USDHC1_DATA6_CONN_NAND_WE_B                        SC_P_USDHC1_DATA6                  1
#define SC_P_USDHC1_DATA6_CONN_USDHC1_WP                        SC_P_USDHC1_DATA6                  2
#define SC_P_USDHC1_DATA6_LSIO_GPIO5_IO21                       SC_P_USDHC1_DATA6                  3
#define SC_P_USDHC1_DATA7_CONN_USDHC1_DATA7                     SC_P_USDHC1_DATA7                  0
#define SC_P_USDHC1_DATA7_CONN_NAND_ALE                         SC_P_USDHC1_DATA7                  1
#define SC_P_USDHC1_DATA7_CONN_USDHC1_CD_B                      SC_P_USDHC1_DATA7                  2
#define SC_P_USDHC1_DATA7_LSIO_GPIO5_IO22                       SC_P_USDHC1_DATA7                  3
#define SC_P_USDHC1_STROBE_CONN_USDHC1_STROBE                   SC_P_USDHC1_STROBE                 0
#define SC_P_USDHC1_STROBE_CONN_NAND_CE1_B                      SC_P_USDHC1_STROBE                 1
#define SC_P_USDHC1_STROBE_CONN_USDHC1_RESET_B                  SC_P_USDHC1_STROBE                 2
#define SC_P_USDHC1_STROBE_LSIO_GPIO5_IO23                      SC_P_USDHC1_STROBE                 3
#define SC_P_ENET0_RGMII_TXC_CONN_ENET0_RGMII_TXC               SC_P_ENET0_RGMII_TXC               0
#define SC_P_ENET0_RGMII_TXC_CONN_ENET0_RCLK50M_OUT             SC_P_ENET0_RGMII_TXC               1
#define SC_P_ENET0_RGMII_TXC_CONN_ENET0_RCLK50M_IN              SC_P_ENET0_RGMII_TXC               2
#define SC_P_ENET0_RGMII_TXC_LSIO_GPIO5_IO30                    SC_P_ENET0_RGMII_TXC               3
#define SC_P_ENET0_RGMII_TX_CTL_CONN_ENET0_RGMII_TX_CTL         SC_P_ENET0_RGMII_TX_CTL            0
#define SC_P_ENET0_RGMII_TX_CTL_LSIO_GPIO5_IO31                 SC_P_ENET0_RGMII_TX_CTL            3
#define SC_P_ENET0_RGMII_TXD0_CONN_ENET0_RGMII_TXD0             SC_P_ENET0_RGMII_TXD0              0
#define SC_P_ENET0_RGMII_TXD0_LSIO_GPIO6_IO00                   SC_P_ENET0_RGMII_TXD0              3
#define SC_P_ENET0_RGMII_TXD1_CONN_ENET0_RGMII_TXD1             SC_P_ENET0_RGMII_TXD1              0
#define SC_P_ENET0_RGMII_TXD1_LSIO_GPIO6_IO01                   SC_P_ENET0_RGMII_TXD1              3
#define SC_P_ENET0_RGMII_TXD2_CONN_ENET0_RGMII_TXD2             SC_P_ENET0_RGMII_TXD2              0
#define SC_P_ENET0_RGMII_TXD2_DMA_UART3_TX                      SC_P_ENET0_RGMII_TXD2              1
#define SC_P_ENET0_RGMII_TXD2_LSIO_GPIO6_IO02                   SC_P_ENET0_RGMII_TXD2              3
#define SC_P_ENET0_RGMII_TXD3_CONN_ENET0_RGMII_TXD3             SC_P_ENET0_RGMII_TXD3              0
#define SC_P_ENET0_RGMII_TXD3_DMA_UART3_RTS_B                   SC_P_ENET0_RGMII_TXD3              1
#define SC_P_ENET0_RGMII_TXD3_LSIO_GPIO6_IO03                   SC_P_ENET0_RGMII_TXD3              3
#define SC_P_ENET0_RGMII_RXC_CONN_ENET0_RGMII_RXC               SC_P_ENET0_RGMII_RXC               0
#define SC_P_ENET0_RGMII_RXC_DMA_UART3_CTS_B                    SC_P_ENET0_RGMII_RXC               1
#define SC_P_ENET0_RGMII_RXC_LSIO_GPIO6_IO04                    SC_P_ENET0_RGMII_RXC               3
#define SC_P_ENET0_RGMII_RX_CTL_CONN_ENET0_RGMII_RX_CTL         SC_P_ENET0_RGMII_RX_CTL            0
#define SC_P_ENET0_RGMII_RX_CTL_LSIO_GPIO6_IO05                 SC_P_ENET0_RGMII_RX_CTL            3
#define SC_P_ENET0_RGMII_RXD0_CONN_ENET0_RGMII_RXD0             SC_P_ENET0_RGMII_RXD0              0
#define SC_P_ENET0_RGMII_RXD0_LSIO_GPIO6_IO06                   SC_P_ENET0_RGMII_RXD0              3
#define SC_P_ENET0_RGMII_RXD1_CONN_ENET0_RGMII_RXD1             SC_P_ENET0_RGMII_RXD1              0
#define SC_P_ENET0_RGMII_RXD1_LSIO_GPIO6_IO07                   SC_P_ENET0_RGMII_RXD1              3
#define SC_P_ENET0_RGMII_RXD2_CONN_ENET0_RGMII_RXD2             SC_P_ENET0_RGMII_RXD2              0
#define SC_P_ENET0_RGMII_RXD2_CONN_ENET0_RMII_RX_ER             SC_P_ENET0_RGMII_RXD2              1
#define SC_P_ENET0_RGMII_RXD2_LSIO_GPIO6_IO08                   SC_P_ENET0_RGMII_RXD2              3
#define SC_P_ENET0_RGMII_RXD3_CONN_ENET0_RGMII_RXD3             SC_P_ENET0_RGMII_RXD3              0
#define SC_P_ENET0_RGMII_RXD3_DMA_UART3_RX                      SC_P_ENET0_RGMII_RXD3              1
#define SC_P_ENET0_RGMII_RXD3_LSIO_GPIO6_IO09                   SC_P_ENET0_RGMII_RXD3              3
#define SC_P_ENET1_RGMII_TXC_CONN_ENET1_RGMII_TXC               SC_P_ENET1_RGMII_TXC               0
#define SC_P_ENET1_RGMII_TXC_CONN_ENET1_RCLK50M_OUT             SC_P_ENET1_RGMII_TXC               1
#define SC_P_ENET1_RGMII_TXC_CONN_ENET1_RCLK50M_IN              SC_P_ENET1_RGMII_TXC               2
#define SC_P_ENET1_RGMII_TXC_LSIO_GPIO6_IO10                    SC_P_ENET1_RGMII_TXC               3
#define SC_P_ENET1_RGMII_TX_CTL_CONN_ENET1_RGMII_TX_CTL         SC_P_ENET1_RGMII_TX_CTL            0
#define SC_P_ENET1_RGMII_TX_CTL_LSIO_GPIO6_IO11                 SC_P_ENET1_RGMII_TX_CTL            3
#define SC_P_ENET1_RGMII_TXD0_CONN_ENET1_RGMII_TXD0             SC_P_ENET1_RGMII_TXD0              0
#define SC_P_ENET1_RGMII_TXD0_LSIO_GPIO6_IO12                   SC_P_ENET1_RGMII_TXD0              3
#define SC_P_ENET1_RGMII_TXD1_CONN_ENET1_RGMII_TXD1             SC_P_ENET1_RGMII_TXD1              0
#define SC_P_ENET1_RGMII_TXD1_LSIO_GPIO6_IO13                   SC_P_ENET1_RGMII_TXD1              3
#define SC_P_ENET1_RGMII_TXD2_CONN_ENET1_RGMII_TXD2             SC_P_ENET1_RGMII_TXD2              0
#define SC_P_ENET1_RGMII_TXD2_DMA_UART3_TX                      SC_P_ENET1_RGMII_TXD2              1
#define SC_P_ENET1_RGMII_TXD2_LSIO_GPIO6_IO14                   SC_P_ENET1_RGMII_TXD2              3
#define SC_P_ENET1_RGMII_TXD3_CONN_ENET1_RGMII_TXD3             SC_P_ENET1_RGMII_TXD3              0
#define SC_P_ENET1_RGMII_TXD3_DMA_UART3_RTS_B                   SC_P_ENET1_RGMII_TXD3              1
#define SC_P_ENET1_RGMII_TXD3_LSIO_GPIO6_IO15                   SC_P_ENET1_RGMII_TXD3              3
#define SC_P_ENET1_RGMII_RXC_CONN_ENET1_RGMII_RXC               SC_P_ENET1_RGMII_RXC               0
#define SC_P_ENET1_RGMII_RXC_DMA_UART3_CTS_B                    SC_P_ENET1_RGMII_RXC               1
#define SC_P_ENET1_RGMII_RXC_LSIO_GPIO6_IO16                    SC_P_ENET1_RGMII_RXC               3
#define SC_P_ENET1_RGMII_RX_CTL_CONN_ENET1_RGMII_RX_CTL         SC_P_ENET1_RGMII_RX_CTL            0
#define SC_P_ENET1_RGMII_RX_CTL_LSIO_GPIO6_IO17                 SC_P_ENET1_RGMII_RX_CTL            3
#define SC_P_ENET1_RGMII_RXD0_CONN_ENET1_RGMII_RXD0             SC_P_ENET1_RGMII_RXD0              0
#define SC_P_ENET1_RGMII_RXD0_LSIO_GPIO6_IO18                   SC_P_ENET1_RGMII_RXD0              3
#define SC_P_ENET1_RGMII_RXD1_CONN_ENET1_RGMII_RXD1             SC_P_ENET1_RGMII_RXD1              0
#define SC_P_ENET1_RGMII_RXD1_LSIO_GPIO6_IO19                   SC_P_ENET1_RGMII_RXD1              3
#define SC_P_ENET1_RGMII_RXD3_CONN_ENET1_RGMII_RXD3             SC_P_ENET1_RGMII_RXD3              0
#define SC_P_ENET1_RGMII_RXD3_DMA_UART3_RX                      SC_P_ENET1_RGMII_RXD3              1
#define SC_P_ENET1_RGMII_RXD3_LSIO_GPIO6_IO21                   SC_P_ENET1_RGMII_RXD3              3
#define SC_P_ENET1_RGMII_RXD2_CONN_ENET1_RGMII_RXD2             SC_P_ENET1_RGMII_RXD2              0
#define SC_P_ENET1_RGMII_RXD2_CONN_ENET1_RMII_RX_ER             SC_P_ENET1_RGMII_RXD2              1
#define SC_P_ENET1_RGMII_RXD2_LSIO_GPIO6_IO20                   SC_P_ENET1_RGMII_RXD2              3
/*@}*/

#endif				/* SC_PADS_H */
