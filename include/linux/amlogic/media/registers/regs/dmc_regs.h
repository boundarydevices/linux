/*
 * include/linux/amlogic/media/registers/regs/dmc_regs.h
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

#ifndef DMC_REGS_HEADERS__
#define DMC_REGS_HEADERS__

#define DMC_REQ_CTRL 0x00
#define DMC_SOFT_RST 0x01
#define DMC_SOFT_RST1 0x02
#define DMC_RST_STS 0x03
#define DMC_RST_STS1 0x04
#define DMC_VERSION 0x05
#define DMC_RAM_PD 0x11
#define DC_CAV_LUT_DATAL 0x12
#define DC_CAV_LUT_DATAH 0x13
#define DC_CAV_LUT_ADDR 0x14
#define DC_CAV_LUT_RDATAL 0x15
#define DC_CAV_LUT_RDATAH 0x16
#define DMC_2ARB_CTRL 0x20
#define DMC_REFR_CTRL1 0x23
#define DMC_REFR_CTRL2 0x24
#define DMC_PARB_CTRL 0x25
#define DMC_MON_CTRL2 0x26
#define DMC_MON_CTRL3 0x27
#define DMC_MON_ALL_REQ_CNT 0x28
#define DMC_MON_ALL_GRANT_CNT 0x29
#define DMC_MON_ONE_GRANT_CNT 0x2a
#define DMC_CLKG_CTRL0 0x30
#define DMC_CLKG_CTRL1 0x31
#define DMC_CHAN_STS 0x32
#define DMC_CMD_FILTER_CTRL1 0x40
#define DMC_CMD_FILTER_CTRL2 0x41
#define DMC_CMD_FILTER_CTRL3 0x42
#define DMC_CMD_FILTER_CTRL4 0x43
#define DMC_CMD_BUFFER_CTRL 0x44
#define DMC_AM0_CHAN_CTRL 0x60
#define DMC_AM0_HOLD_CTRL 0x61
#define DMC_AM0_QOS_INC 0x62
#define DMC_AM0_QOS_INCBK 0x63
#define DMC_AM0_QOS_DEC 0x64
#define DMC_AM0_QOS_DECBK 0x65
#define DMC_AM0_QOS_DIS 0x66
#define DMC_AM0_QOS_DISBK 0x67
#define DMC_AM0_QOS_CTRL0 0x68
#define DMC_AM0_QOS_CTRL1 0x69
#define DMC_AM1_CHAN_CTRL 0x6a
#define DMC_AM1_HOLD_CTRL 0x6b
#define DMC_AM1_QOS_INC 0x6c
#define DMC_AM1_QOS_INCBK 0x6d
#define DMC_AM1_QOS_DEC 0x6e
#define DMC_AM1_QOS_DECBK 0x6f
#define DMC_AM1_QOS_DIS 0x70
#define DMC_AM1_QOS_DISBK 0x71
#define DMC_AM1_QOS_CTRL0 0x72
#define DMC_AM1_QOS_CTRL1 0x73
#define DMC_AM2_CHAN_CTRL 0x74
#define DMC_AM2_HOLD_CTRL 0x75
#define DMC_AM2_QOS_INC 0x76
#define DMC_AM2_QOS_INCBK 0x77
#define DMC_AM2_QOS_DEC 0x78
#define DMC_AM2_QOS_DECBK 0x79
#define DMC_AM2_QOS_DIS 0x7a
#define DMC_AM2_QOS_DISBK 0x7b
#define DMC_AM2_QOS_CTRL0 0x7c
#define DMC_AM2_QOS_CTRL1 0x7d
#define DMC_AM3_CHAN_CTRL 0x7e
#define DMC_AM3_HOLD_CTRL 0x7f
#define DMC_AM3_QOS_INC 0x80
#define DMC_AM3_QOS_INCBK 0x81
#define DMC_AM3_QOS_DEC 0x82
#define DMC_AM3_QOS_DECBK 0x83
#define DMC_AM3_QOS_DIS 0x84
#define DMC_AM3_QOS_DISBK 0x85
#define DMC_AM3_QOS_CTRL0 0x86
#define DMC_AM3_QOS_CTRL1 0x87
#define DMC_AM4_CHAN_CTRL 0x88
#define DMC_AM4_HOLD_CTRL 0x89
#define DMC_AM4_QOS_INC 0x8a
#define DMC_AM4_QOS_INCBK 0x8b
#define DMC_AM4_QOS_DEC 0x8c
#define DMC_AM4_QOS_DECBK 0x8d
#define DMC_AM4_QOS_DIS 0x8e
#define DMC_AM4_QOS_DISBK 0x8f
#define DMC_AM4_QOS_CTRL0 0x90
#define DMC_AM4_QOS_CTRL1 0x91
#define DMC_AM5_CHAN_CTRL 0x92
#define DMC_AM5_HOLD_CTRL 0x93
#define DMC_AM5_QOS_INC 0x94
#define DMC_AM5_QOS_INCBK 0x95
#define DMC_AM5_QOS_DEC 0x96
#define DMC_AM5_QOS_DECBK 0x97
#define DMC_AM5_QOS_DIS 0x98
#define DMC_AM5_QOS_DISBK 0x99
#define DMC_AM5_QOS_CTRL0 0x9a
#define DMC_AM5_QOS_CTRL1 0x9b
#define DMC_AM6_CHAN_CTRL 0x9c
#define DMC_AM6_HOLD_CTRL 0x9d
#define DMC_AM6_QOS_INC 0x9e
#define DMC_AM6_QOS_INCBK 0x9f
#define DMC_AM6_QOS_DEC 0xa0
#define DMC_AM6_QOS_DECBK 0xa1
#define DMC_AM6_QOS_DIS 0xa2
#define DMC_AM6_QOS_DISBK 0xa3
#define DMC_AM6_QOS_CTRL0 0xa4
#define DMC_AM6_QOS_CTRL1 0xa5
#define DMC_AM7_CHAN_CTRL 0xa6
#define DMC_AM7_HOLD_CTRL 0xa7
#define DMC_AM7_QOS_INC 0xa8
#define DMC_AM7_QOS_INCBK 0xa9
#define DMC_AM7_QOS_DEC 0xaa
#define DMC_AM7_QOS_DECBK 0xab
#define DMC_AM7_QOS_DIS 0xac
#define DMC_AM7_QOS_DISBK 0xad
#define DMC_AM7_QOS_CTRL0 0xae
#define DMC_AM7_QOS_CTRL1 0xaf
#define DMC_AXI0_CHAN_CTRL 0xb0
#define DMC_AXI0_HOLD_CTRL 0xb1
#define DMC_AXI0_QOS_INC 0xb2
#define DMC_AXI0_QOS_INCBK 0xb3
#define DMC_AXI0_QOS_DEC 0xb4
#define DMC_AXI0_QOS_DECBK 0xb5
#define DMC_AXI0_QOS_DIS 0xb6
#define DMC_AXI0_QOS_DISBK 0xb7
#define DMC_AXI0_QOS_CTRL0 0xb8
#define DMC_AXI0_QOS_CTRL1 0xb9
#define DMC_AXI1_CHAN_CTRL 0xba
#define DMC_AXI1_HOLD_CTRL 0xbb
#define DMC_AXI1_QOS_INC 0xbc
#define DMC_AXI1_QOS_INCBK 0xbd
#define DMC_AXI1_QOS_DEC 0xbe
#define DMC_AXI1_QOS_DECBK 0xbf
#define DMC_AXI1_QOS_DIS 0xc0
#define DMC_AXI1_QOS_DISBK 0xc1
#define DMC_AXI1_QOS_CTRL0 0xc2
#define DMC_AXI1_QOS_CTRL1 0xc3
#define DMC_AXI2_CHAN_CTRL 0xc4
#define DMC_AXI2_HOLD_CTRL 0xc5
#define DMC_AXI2_QOS_INC 0xc6
#define DMC_AXI2_QOS_INCBK 0xc7
#define DMC_AXI2_QOS_DEC 0xc8
#define DMC_AXI2_QOS_DECBK 0xc9
#define DMC_AXI2_QOS_DIS 0xca
#define DMC_AXI2_QOS_DISBK 0xcb
#define DMC_AXI2_QOS_CTRL0 0xcc
#define DMC_AXI2_QOS_CTRL1 0xcd
#define DMC_AXI3_CHAN_CTRL 0xce
#define DMC_AXI3_HOLD_CTRL 0xcf
#define DMC_AXI3_QOS_INC 0xd0
#define DMC_AXI3_QOS_INCBK 0xd1
#define DMC_AXI3_QOS_DEC 0xd2
#define DMC_AXI3_QOS_DECBK 0xd3
#define DMC_AXI3_QOS_DIS 0xd4
#define DMC_AXI3_QOS_DISBK 0xd5
#define DMC_AXI3_QOS_CTRL0 0xd6
#define DMC_AXI3_QOS_CTRL1 0xd7
#define DMC_AXI4_CHAN_CTRL 0xd8
#define DMC_AXI4_HOLD_CTRL 0xd9
#define DMC_AXI4_QOS_INC 0xda
#define DMC_AXI4_QOS_INCBK 0xdb
#define DMC_AXI4_QOS_DEC 0xdc
#define DMC_AXI4_QOS_DECBK 0xdd
#define DMC_AXI4_QOS_DIS 0xde
#define DMC_AXI4_QOS_DISBK 0xdf
#define DMC_AXI4_QOS_CTRL0 0xe0
#define DMC_AXI4_QOS_CTRL1 0xe1
#define DMC_AXI5_CHAN_CTRL 0xe2
#define DMC_AXI5_HOLD_CTRL 0xe3
#define DMC_AXI5_QOS_INC 0xe4
#define DMC_AXI5_QOS_INCBK 0xe5
#define DMC_AXI5_QOS_DEC 0xe6
#define DMC_AXI5_QOS_DECBK 0xe7
#define DMC_AXI5_QOS_DIS 0xe8
#define DMC_AXI5_QOS_DISBK 0xe9
#define DMC_AXI5_QOS_CTRL0 0xea
#define DMC_AXI5_QOS_CTRL1 0xeb
#define DMC_AXI6_CHAN_CTRL 0xec
#define DMC_AXI6_HOLD_CTRL 0xed
#define DMC_AXI6_QOS_INC 0xee
#define DMC_AXI6_QOS_INCBK 0xef
#define DMC_AXI6_QOS_DEC 0xf0
#define DMC_AXI6_QOS_DECBK 0xf1
#define DMC_AXI6_QOS_DIS 0xf2
#define DMC_AXI6_QOS_DISBK 0xf3
#define DMC_AXI6_QOS_CTRL0 0xf4
#define DMC_AXI6_QOS_CTRL1 0xf5
#define DMC_AXI7_CHAN_CTRL 0xf6
#define DMC_AXI7_HOLD_CTRL 0xf7
#define DMC_AXI7_QOS_INC 0xf8
#define DMC_AXI7_QOS_INCBK 0xf9
#define DMC_AXI7_QOS_DEC 0xfa
#define DMC_AXI7_QOS_DECBK 0xfb
#define DMC_AXI7_QOS_DIS 0xfc
#define DMC_AXI7_QOS_DISBK 0xfd
#define DMC_AXI7_QOS_CTRL0 0xfe
#define DMC_AXI7_QOS_CTRL1 0xff

#endif









