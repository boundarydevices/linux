/**
 * Microchip KSZ8863 definition file
 *
 * Copyright (c) 2015-2018 Microchip Technology Inc.
 *	Tristram Ha <Tristram.Ha@microchip.com>
 *
 * Copyright (c) 2012-2014 Micrel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


/* -------------------------------------------------------------------------- */

#ifndef __KSZ8863_H
#define __KSZ8863_H


#define REG_CHIP_ID0			0x00

#define FAMILY_ID			0x88

#define REG_CHIP_ID1			0x01

#define SWITCH_CHIP_ID_MASK		0xF0
#define SWITCH_CHIP_ID_SHIFT		4
#define SWITCH_REVISION_MASK		0x0E
#define SWITCH_REVISION_SHIFT		1
#define SWITCH_START			0x01

#define CHIP_ID_63			0x30

#define REG_SWITCH_CTRL_0		0x02

#define SWITCH_NEW_BACKOFF		(1 << 7)
#define SWITCH_FLUSH_DYN_MAC_TABLE	(1 << 5)
#define SWITCH_FLUSH_STA_MAC_TABLE	(1 << 4)
#define SWITCH_PASS_PAUSE		(1 << 3)
#define SWITCH_LINK_AUTO_AGING		(1 << 0)

#define REG_SWITCH_CTRL_1		0x03

#define SWITCH_PASS_ALL			(1 << 7)
#define SWITCH_TAIL_TAG_ENABLE		(1 << 6)
#define SWITCH_TX_FLOW_CTRL		(1 << 5)
#define SWITCH_RX_FLOW_CTRL		(1 << 4)
#define SWITCH_CHECK_LENGTH		(1 << 3)
#define SWITCH_AGING_ENABLE		(1 << 2)
#define SWITCH_FAST_AGING		(1 << 1)
#define SWITCH_AGGR_BACKOFF		(1 << 0)

#define REG_SWITCH_CTRL_2		0x04

#define UNICAST_VLAN_BOUNDARY		(1 << 7)
#define MULTICAST_STORM_DISABLE		(1 << 6)
#define SWITCH_BACK_PRESSURE		(1 << 5)
#define FAIR_FLOW_CTRL			(1 << 4)
#define NO_EXC_COLLISION_DROP		(1 << 3)
#define SWITCH_HUGE_PACKET		(1 << 2)
#define SWITCH_LEGAL_PACKET		(1 << 1)

#define REG_SWITCH_CTRL_3		0x05

#define SWITCH_VLAN_ENABLE		(1 << 7)
#define SWITCH_IGMP_SNOOP		(1 << 6)
#define WEIGHTED_FAIR_QUEUE_ENABLE	(1 << 3)
#define SWITCH_MIRROR_RX_TX		(1 << 0)

#define REG_SWITCH_CTRL_4		0x06

#define SWITCH_HALF_DUPLEX		(1 << 6)
#define SWITCH_FLOW_CTRL		(1 << 5)
#define SWITCH_10_MBIT			(1 << 4)
#define SWITCH_REPLACE_VID		(1 << 3)
#define BROADCAST_STORM_RATE_HI		0x07

#define REG_SWITCH_CTRL_5		0x07

#define BROADCAST_STORM_RATE_LO		0xFF
#define BROADCAST_STORM_RATE		0x07FF

#define REG_SWITCH_CTRL_9		0x0B

#define SPI_CLK_125_MHZ			0x80
#define SPI_CLK_62_5_MHZ		0x40
#define SPI_CLK_31_25_MHZ		0x00

#define REG_SWITCH_CTRL_10		0x0C
#define REG_SWITCH_CTRL_11		0x0D

#define SWITCH_802_1P_MASK		3
#define SWITCH_802_1P_BASE		3
#define SWITCH_802_1P_SHIFT		2

#define SWITCH_802_1P_MAP_MASK		3
#define SWITCH_802_1P_MAP_SHIFT		2

#define REG_SWITCH_CTRL_12		0x0E

#define SWITCH_UNKNOWN_DA_ENABLE	(1 << 7)
#define SWITCH_DRIVER_16MA		(1 << 6)
#define SWITCH_UNKNOWN_DA_2_PORT3	(1 << 2)
#define SWITCH_UNKNOWN_DA_2_PORT2	(1 << 1)
#define SWITCH_UNKNOWN_DA_2_PORT1	(1 << 0)

#define REG_SWITCH_CTRL_13		0x0F

#define SWITCH_PORT_PHY_ADDR_MASK	0x1F
#define SWITCH_PORT_PHY_ADDR_SHIFT	3


#define REG_PORT_1_CTRL_0		0x10
#define REG_PORT_2_CTRL_0		0x20
#define REG_PORT_3_CTRL_0		0x30

#define PORT_BROADCAST_STORM		(1 << 7)
#define PORT_DIFFSERV_ENABLE		(1 << 6)
#define PORT_802_1P_ENABLE		(1 << 5)
#define PORT_BASED_PRIORITY_MASK	0x18
#define PORT_BASED_PRIORITY_BASE	0x03
#define PORT_BASED_PRIORITY_SHIFT	3
#define PORT_PORT_PRIO_0		0x00
#define PORT_PORT_PRIO_1		0x08
#define PORT_PORT_PRIO_2		0x10
#define PORT_PORT_PRIO_3		0x18
#define PORT_INSERT_TAG			(1 << 2)
#define PORT_REMOVE_TAG			(1 << 1)
#define PORT_4_QUEUES_ENABLE		(1 << 0)

#define REG_PORT_1_CTRL_1		0x11
#define REG_PORT_2_CTRL_1		0x21
#define REG_PORT_3_CTRL_1		0x31

#define PORT_MIRROR_SNIFFER		(1 << 7)
#define PORT_MIRROR_RX			(1 << 6)
#define PORT_MIRROR_TX			(1 << 5)
#define PORT_DOUBLE_TAG			(1 << 4)
#define PORT_802_1P_REMAPPING		(1 << 3)
#define PORT_VLAN_MEMBERSHIP		0x07

#define REG_PORT_1_CTRL_2		0x12
#define REG_PORT_2_CTRL_2		0x22
#define REG_PORT_3_CTRL_2		0x32

#define PORT_2_QUEUES_ENABLE		(1 << 7)
#define PORT_INGRESS_FILTER		(1 << 6)
#define PORT_DISCARD_NON_VID		(1 << 5)
#define PORT_FORCE_FLOW_CTRL		(1 << 4)
#define PORT_BACK_PRESSURE		(1 << 3)
#define PORT_TX_ENABLE			(1 << 2)
#define PORT_RX_ENABLE			(1 << 1)
#define PORT_LEARN_DISABLE		(1 << 0)

#define REG_PORT_1_CTRL_3		0x13
#define REG_PORT_2_CTRL_3		0x23
#define REG_PORT_3_CTRL_3		0x33
#define REG_PORT_1_CTRL_4		0x14
#define REG_PORT_2_CTRL_4		0x24
#define REG_PORT_3_CTRL_4		0x34

#define PORT_DEFAULT_VID		0x0001

#define REG_PORT_1_CTRL_5		0x15
#define REG_PORT_2_CTRL_5		0x25
#define REG_PORT_3_CTRL_5		0x35

#define PORT_3_MII_MAC_MODE		(1 << 7)
#define PORT_SA_MAC2			(1 << 6)
#define PORT_SA_MAC1			(1 << 5)
#define PORT_DROP_TAG			(1 << 4)
#define PORT_INGRESS_LIMIT_MODE		0x0C
#define PORT_INGRESS_ALL		0x00
#define PORT_INGRESS_UNICAST		0x04
#define PORT_INGRESS_MULTICAST		0x08
#define PORT_INGRESS_BROADCAST		0x0C
#define PORT_COUNT_IFG			(1 << 1)
#define PORT_COUNT_PREAMBLE		(1 << 0)

#define REG_PORT_1_IN_RATE_0		0x16
#define REG_PORT_2_IN_RATE_0		0x26
#define REG_PORT_3_IN_RATE_0		0x36

#define PORT_3_INVERTED_REFCLK		(1 << 7)

#define REG_PORT_1_IN_RATE_1		0x17
#define REG_PORT_2_IN_RATE_1		0x27
#define REG_PORT_3_IN_RATE_1		0x37
#define REG_PORT_1_IN_RATE_2		0x18
#define REG_PORT_2_IN_RATE_2		0x28
#define REG_PORT_3_IN_RATE_2		0x38
#define REG_PORT_1_IN_RATE_3		0x19
#define REG_PORT_2_IN_RATE_3		0x29
#define REG_PORT_3_IN_RATE_3		0x39

#define PORT_PRIO_RATE			0x0F
#define PORT_PRIO_RATE_SHIFT		4


#define REG_PORT_1_LINK_MD_CTRL		0x1A
#define REG_PORT_2_LINK_MD_CTRL		0x2A

#define PORT_CABLE_10M_SHORT		(1 << 7)
#define PORT_CABLE_DIAG_RESULT		0x60
#define PORT_CABLE_STAT_NORMAL		0x00
#define PORT_CABLE_STAT_OPEN		0x20
#define PORT_CABLE_STAT_SHORT		0x40
#define PORT_CABLE_STAT_FAILED		0x60
#define PORT_START_CABLE_DIAG		(1 << 4)
#define PORT_FORCE_LINK			(1 << 3)
#define PORT_POWER_SAVING		0x04
#define PORT_PHY_REMOTE_LOOPBACK	(1 << 1)
#define PORT_CABLE_FAULT_COUNTER_H	0x01

#define REG_PORT_1_LINK_MD_RESULT	0x1B
#define REG_PORT_2_LINK_MD_RESULT	0x2B

#define PORT_CABLE_FAULT_COUNTER_L	0xFF
#define PORT_CABLE_FAULT_COUNTER	0x1FF

#define REG_PORT_1_CTRL_12		0x1C
#define REG_PORT_2_CTRL_12		0x2C

#define PORT_AUTO_NEG_ENABLE		(1 << 7)
#define PORT_FORCE_100_MBIT		(1 << 6)
#define PORT_FORCE_FULL_DUPLEX		(1 << 5)
#define PORT_AUTO_NEG_SYM_PAUSE		(1 << 4)
#define PORT_AUTO_NEG_100BTX_FD		(1 << 3)
#define PORT_AUTO_NEG_100BTX		(1 << 2)
#define PORT_AUTO_NEG_10BT_FD		(1 << 1)
#define PORT_AUTO_NEG_10BT		(1 << 0)

#define REG_PORT_1_CTRL_13		0x1D
#define REG_PORT_2_CTRL_13		0x2D

#define PORT_LED_OFF			(1 << 7)
#define PORT_TX_DISABLE			(1 << 6)
#define PORT_AUTO_NEG_RESTART		(1 << 5)
#define PORT_REMOTE_FAULT_DISABLE	(1 << 4)
#define PORT_POWER_DOWN			(1 << 3)
#define PORT_AUTO_MDIX_DISABLE		(1 << 2)
#define PORT_FORCE_MDIX			(1 << 1)
#define PORT_LOOPBACK			(1 << 0)

#define REG_PORT_1_STATUS_0		0x1E
#define REG_PORT_2_STATUS_0		0x2E

#define PORT_MDIX_STATUS		(1 << 7)
#define PORT_AUTO_NEG_COMPLETE		(1 << 6)
#define PORT_STATUS_LINK_GOOD		(1 << 5)
#define PORT_REMOTE_SYM_PAUSE		(1 << 4)
#define PORT_REMOTE_100BTX_FD		(1 << 3)
#define PORT_REMOTE_100BTX		(1 << 2)
#define PORT_REMOTE_10BT_FD		(1 << 1)
#define PORT_REMOTE_10BT		(1 << 0)

#define REG_PORT_1_STATUS_1		0x1F
#define REG_PORT_2_STATUS_1		0x2F
#define REG_PORT_3_STATUS_1		0x3F

#define PORT_HP_MDIX			(1 << 7)
#define PORT_REVERSED_POLARITY		(1 << 5)
#define PORT_TX_FLOW_CTRL		(1 << 4)
#define PORT_RX_FLOW_CTRL		(1 << 3)
#define PORT_STAT_SPEED_100MBIT		(1 << 2)
#define PORT_STAT_FULL_DUPLEX		(1 << 1)
#define PORT_REMOTE_FAULT		(1 << 0)

#define REG_PORT_CTRL_0			0x00
#define REG_PORT_CTRL_1			0x01
#define REG_PORT_CTRL_2			0x02
#define REG_PORT_CTRL_VID		0x03
#define REG_PORT_CTRL_5			0x05
#define REG_PORT_IN_RATE_0		0x06
#define REG_PORT_IN_RATE_1		0x07
#define REG_PORT_IN_RATE_2		0x08
#define REG_PORT_IN_RATE_3		0x09
#define REG_PORT_LINK_MD_CTRL		0x0A
#define REG_PORT_LINK_MD_RESULT		0x0B
#define REG_PORT_CTRL_12		0x0C
#define REG_PORT_CTRL_13		0x0D
#define REG_PORT_STATUS_0		0x0E
#define REG_PORT_STATUS_1		0x0F

#define PORT_CTRL_ADDR(port, addr)		\
	(addr = REG_PORT_1_CTRL_0 + (port) *	\
		(REG_PORT_2_CTRL_0 - REG_PORT_1_CTRL_0))


#define REG_SWITCH_RESET		0x43

#define GLOBAL_SOFTWARE_RESET		(1 << 4)
#define PCS_RESET			(1 << 0)


#define REG_TOS_PRIO_CTRL_0		0x60
#define REG_TOS_PRIO_CTRL_1		0x61
#define REG_TOS_PRIO_CTRL_2		0x62
#define REG_TOS_PRIO_CTRL_3		0x63
#define REG_TOS_PRIO_CTRL_4		0x64
#define REG_TOS_PRIO_CTRL_5		0x65
#define REG_TOS_PRIO_CTRL_6		0x66
#define REG_TOS_PRIO_CTRL_7		0x67
#define REG_TOS_PRIO_CTRL_8		0x68
#define REG_TOS_PRIO_CTRL_9		0x69
#define REG_TOS_PRIO_CTRL_10		0x6A
#define REG_TOS_PRIO_CTRL_11		0x6B
#define REG_TOS_PRIO_CTRL_12		0x6C
#define REG_TOS_PRIO_CTRL_13		0x6D
#define REG_TOS_PRIO_CTRL_14		0x6E
#define REG_TOS_PRIO_CTRL_15		0x6F

#define TOS_PRIO_M			3
#define TOS_PRIO_S			2


#define REG_SWITCH_MAC_ADDR_0		0x70
#define REG_SWITCH_MAC_ADDR_1		0x71
#define REG_SWITCH_MAC_ADDR_2		0x72
#define REG_SWITCH_MAC_ADDR_3		0x73
#define REG_SWITCH_MAC_ADDR_4		0x74
#define REG_SWITCH_MAC_ADDR_5		0x75


#define REG_USER_DEFINED_1		0x76
#define REG_USER_DEFINED_2		0x77
#define REG_USER_DEFINED_3		0x78


#define REG_IND_CTRL_0			0x79

#define TABLE_READ			(1 << 4)
#define TABLE_STATIC_MAC		(0 << 2)
#define TABLE_VLAN			(1 << 2)
#define TABLE_DYNAMIC_MAC		(2 << 2)
#define TABLE_MIB			(3 << 2)

#define REG_IND_CTRL_1			0x7A

#define TABLE_ENTRY_MASK		0x03FF

#define REG_IND_DATA_8			0x7B
#define REG_IND_DATA_7			0x7C
#define REG_IND_DATA_6			0x7D
#define REG_IND_DATA_5			0x7E
#define REG_IND_DATA_4			0x7F
#define REG_IND_DATA_3			0x80
#define REG_IND_DATA_2			0x81
#define REG_IND_DATA_1			0x82
#define REG_IND_DATA_0			0x83

#define REG_IND_DATA_CHECK		REG_IND_DATA_8
#define REG_IND_DATA_HI			REG_IND_DATA_7
#define REG_IND_DATA_LO			REG_IND_DATA_3

#define REG_PORT_0_MAC_ADDR_0		0x8E
#define REG_PORT_0_MAC_ADDR_1		0x8F
#define REG_PORT_0_MAC_ADDR_2		0x90
#define REG_PORT_0_MAC_ADDR_3		0x91
#define REG_PORT_0_MAC_ADDR_4		0x92
#define REG_PORT_0_MAC_ADDR_5		0x93
#define REG_PORT_1_MAC_ADDR_0		0x94
#define REG_PORT_1_MAC_ADDR_1		0x95
#define REG_PORT_1_MAC_ADDR_2		0x96
#define REG_PORT_1_MAC_ADDR_3		0x97
#define REG_PORT_1_MAC_ADDR_4		0x98
#define REG_PORT_1_MAC_ADDR_5		0x99

#define REG_PORT_1_OUT_RATE_0		0x9A
#define REG_PORT_2_OUT_RATE_0		0x9E
#define REG_PORT_3_OUT_RATE_0		0xA2

#define SWITCH_OUT_RATE_ENABLE		(1 << 7)

#define REG_PORT_1_OUT_RATE_1		0x9B
#define REG_PORT_2_OUT_RATE_1		0x9F
#define REG_PORT_3_OUT_RATE_1		0xA3
#define REG_PORT_1_OUT_RATE_2		0x9C
#define REG_PORT_2_OUT_RATE_2		0xA0
#define REG_PORT_3_OUT_RATE_2		0xA4
#define REG_PORT_1_OUT_RATE_3		0x9D
#define REG_PORT_2_OUT_RATE_3		0xA1
#define REG_PORT_3_OUT_RATE_3		0xA5

#define REG_PORT_OUT_RATE_0		0x00
#define REG_PORT_OUT_RATE_1		0x01
#define REG_PORT_OUT_RATE_2		0x02
#define REG_PORT_OUT_RATE_3		0x03

#define PORT_OUT_RATE_ADDR(port, addr)			\
	(addr = REG_PORT_1_OUT_RATE_0 + (port) *	\
		(REG_PORT_2_OUT_RATE_0 - REG_PORT_1_OUT_RATE_0))


#define REG_MODE_INDICATOR		0xA6

#define MODE_2_MII			(1 << 7)
#define MODE_2_PHY			(1 << 6)
#define PORT_1_RMII			(1 << 5)
#define PORT_3_RMII			(1 << 4)
#define PORT_1_MAC_MII			(1 << 3)
#define PORT_3_MAC_MII			(1 << 2)
#define PORT_1_COPPER			(1 << 1)
#define PORT_2_COPPER			(1 << 0)

#define MODE_MLL			0x03
#define MODE_RLL			0x13
#define MODE_FLL			0x01

#define REG_BUF_RESERVED_Q3		0xA7
#define REG_BUF_RESERVED_Q2		0xA8
#define REG_BUF_RESERVED_Q1		0xA9
#define REG_BUF_RESERVED_Q0		0xAA
#define REG_PM_FLOW_CTRL_SELECT_1	0xAB
#define REG_PM_FLOW_CTRL_SELECT_2	0xAC
#define REG_PM_FLOW_CTRL_SELECT_3	0xAD
#define REG_PM_FLOW_CTRL_SELECT_4	0xAE

#define REG_PORT1_TXQ3_RATE_CTRL	0xAF
#define REG_PORT1_TXQ2_RATE_CTRL	0xB0
#define REG_PORT1_TXQ1_RATE_CTRL	0xB1
#define REG_PORT1_TXQ0_RATE_CTRL	0xB2
#define REG_PORT2_TXQ3_RATE_CTRL	0xB3
#define REG_PORT2_TXQ2_RATE_CTRL	0xB4
#define REG_PORT2_TXQ1_RATE_CTRL	0xB5
#define REG_PORT2_TXQ0_RATE_CTRL	0xB6
#define REG_PORT3_TXQ3_RATE_CTRL	0xB7
#define REG_PORT3_TXQ2_RATE_CTRL	0xB8
#define REG_PORT3_TXQ1_RATE_CTRL	0xB9
#define REG_PORT3_TXQ0_RATE_CTRL	0xBA

#define RATE_CTRL_ENABLE		(1 << 7)


#define REG_INT_ENABLE			0xBB

#define REG_INT_STATUS			0xBC

#define INT_PORT_1_2_LINK_CHANGE	(1 << 7)
#define INT_PORT_3_LINK_CHANGE		(1 << 2)
#define INT_PORT_2_LINK_CHANGE		(1 << 1)
#define INT_PORT_1_LINK_CHANGE		(1 << 0)

#define REG_PAUSE_ITERATION_LIMIT	0xBD

#define REG_INSERT_SRC_PVID		0xC2

#define SWITCH_INS_TAG_1_PORT_2		(1 << 5)
#define SWITCH_INS_TAG_1_PORT_3		(1 << 4)
#define SWITCH_INS_TAG_2_PORT_1		(1 << 3)
#define SWITCH_INS_TAG_2_PORT_3		(1 << 2)
#define SWITCH_INS_TAG_3_PORT_1		(1 << 1)
#define SWITCH_INS_TAG_3_PORT_2		(1 << 0)


#define REG_POWER_MANAGEMENT		0xC3

#define SWITCH_CPU_CLK_POWER_DOWN	(1 << 7)
#define SWITCH_CLK_POWER_DOWN		(1 << 6)
#define SWITCH_LED_SELECTION		0x30
#define SWITCH_LED_LINK_ACT_SPEED	0x00
#define SWITCH_LED_LINK_ACT		0x20
#define SWITCH_LED_LINK_ACT_DUPLEX	0x10
#define SWITCH_LED_LINK_DUPLEX		0x30
#define SWITCH_LED_OUTPUT_MODE		(1 << 3)
#define SWITCH_PLL_POWER_DOWN		(1 << 2)
#define SWITCH_POWER_MANAGEMENT_MODE	0x03
#define SWITCH_NORMAL			0x00
#define SWITCH_ENERGY_DETECTION		0x01
#define SWITCH_SOFTWARE_POWER_DOWN	0x02
#define SWITCH_POWER_SAVING		0x03


#define REG_FORWARD_INVALID_VID		0xC6

#define SWITCH_FORWARD_INVALID_PORTS	0x70
#define FORWARD_INVALID_PORT_SHIFT	4
#define PORT_3_RMII_CLOCK_SELECTION	(1 << 3)
#define PORT_1_RMII_CLOCK_SELECTION	(1 << 2)
#define SWITCH_HOST_INTERFACE_MODE	0x03
#define SWITCH_I2C_MASTER		0x00
#define SWITCH_I2C_SLAVE		0x01
#define SWITCH_SPI_SLAVE		0x02
#define SWITCH_SMII			0x03


#ifndef PHY_REG_CTRL
#define PHY_REG_CTRL			0

#define PHY_RESET_NOT			(1 << 15)
#define PHY_LOOPBACK			(1 << 14)
#define PHY_SPEED_100MBIT		(1 << 13)
#define PHY_AUTO_NEG_ENABLE		(1 << 12)
#define PHY_POWER_DOWN			(1 << 11)
#define PHY_MII_DISABLE_NOT		(1 << 10)
#define PHY_AUTO_NEG_RESTART		(1 << 9)
#define PHY_FULL_DUPLEX			(1 << 8)
#define PHY_COLLISION_TEST_NOT		(1 << 7)
#define PHY_HP_MDIX			(1 << 5)
#define PHY_FORCE_MDIX			(1 << 4)
#define PHY_AUTO_MDIX_DISABLE		(1 << 3)
#define PHY_REMOTE_FAULT_DISABLE	(1 << 2)
#define PHY_TRANSMIT_DISABLE		(1 << 1)
#define PHY_LED_DISABLE			(1 << 0)

#define PHY_REG_STATUS			1

#define PHY_100BT4_CAPABLE		(1 << 15)
#define PHY_100BTX_FD_CAPABLE		(1 << 14)
#define PHY_100BTX_CAPABLE		(1 << 13)
#define PHY_10BT_FD_CAPABLE		(1 << 12)
#define PHY_10BT_CAPABLE		(1 << 11)
#define PHY_MII_SUPPRESS_CAPABLE_NOT	(1 << 6)
#define PHY_AUTO_NEG_ACKNOWLEDGE	(1 << 5)
#define PHY_REMOTE_FAULT		(1 << 4)
#define PHY_AUTO_NEG_CAPABLE		(1 << 3)
#define PHY_LINK_STATUS			(1 << 2)
#define PHY_JABBER_DETECT_NOT		(1 << 1)
#define PHY_EXTENDED_CAPABILITY		(1 << 0)

#define PHY_REG_ID_1			2
#define PHY_REG_ID_2			3

#define PHY_REG_AUTO_NEGOTIATION	4

#define PHY_AUTO_NEG_NEXT_PAGE_NOT	(1 << 15)
#define PHY_AUTO_NEG_REMOTE_FAULT_NOT	(1 << 13)
#define PHY_AUTO_NEG_SYM_PAUSE		(1 << 10)
#define PHY_AUTO_NEG_100BT4		(1 << 9)
#define PHY_AUTO_NEG_100BTX_FD		(1 << 8)
#define PHY_AUTO_NEG_100BTX		(1 << 7)
#define PHY_AUTO_NEG_10BT_FD		(1 << 6)
#define PHY_AUTO_NEG_10BT		(1 << 5)
#define PHY_AUTO_NEG_SELECTOR		0x001F
#define PHY_AUTO_NEG_802_3		0x0001

#define PHY_REG_REMOTE_CAPABILITY	5

#define PHY_REMOTE_NEXT_PAGE_NOT	(1 << 15)
#define PHY_REMOTE_ACKNOWLEDGE_NOT	(1 << 14)
#define PHY_REMOTE_REMOTE_FAULT_NOT	(1 << 13)
#define PHY_REMOTE_SYM_PAUSE		(1 << 10)
#define PHY_REMOTE_100BTX_FD		(1 << 8)
#define PHY_REMOTE_100BTX		(1 << 7)
#define PHY_REMOTE_10BT_FD		(1 << 6)
#define PHY_REMOTE_10BT			(1 << 5)
#endif

#define KSZ8863_ID_HI			0x0022
#define KSZ8863_ID_LO			0x1430

#define PHY_REG_LINK_MD			29

#define PHY_START_CABLE_DIAG		(1 << 15)
#define PHY_CABLE_DIAG_RESULT		0x6000
#define PHY_CABLE_STAT_NORMAL		0x0000
#define PHY_CABLE_STAT_OPEN		0x2000
#define PHY_CABLE_STAT_SHORT		0x4000
#define PHY_CABLE_STAT_FAILED		0x6000
#define PHY_CABLE_10M_SHORT		(1 << 12)
#define PHY_CABLE_FAULT_COUNTER		0x01FF

#define PHY_REG_PHY_CTRL		30

#define PHY_STAT_REVERSED_POLARITY	(1 << 5)
#define PHY_STAT_MDIX			(1 << 4)
#define PHY_FORCE_LINK			(1 << 3)
#define PHY_POWER_SAVING_DISABLE	(1 << 2)
#define PHY_REMOTE_LOOPBACK		(1 << 1)


/* Default values are used in ksz_sw.h if these are not defined. */
#define PRIO_QUEUES			4

#define KS_PRIO_IN_REG			4

#define TOTAL_PORT_NUM			3

#define KSZ8863_COUNTER_NUM		0x20
#define TOTAL_KSZ8863_COUNTER_NUM	(KSZ8863_COUNTER_NUM + 2)

#define SWITCH_COUNTER_NUM		KSZ8863_COUNTER_NUM
#define TOTAL_SWITCH_COUNTER_NUM	TOTAL_KSZ8863_COUNTER_NUM

/* Required for common switch control in ksz_sw.c */
#define SW_D				u8
#define SW_R(sw, addr)			(sw)->reg->r8(sw, addr)
#define SW_W(sw, addr, val)		(sw)->reg->w8(sw, addr, val)
#define SW_SIZE				(1)
#define SW_SIZE_STR			"%02x"
#define port_r				port_r8
#define port_w				port_w8

#define P_BCAST_STORM_CTRL		REG_PORT_CTRL_0
#define P_PRIO_CTRL			REG_PORT_CTRL_0
#define P_TAG_CTRL			REG_PORT_CTRL_0
#define P_MIRROR_CTRL			REG_PORT_CTRL_1
#define P_STP_CTRL			REG_PORT_CTRL_2
#define P_PHY_CTRL			REG_PORT_CTRL_12
#define P_FORCE_CTRL			REG_PORT_CTRL_12
#define P_NEG_RESTART_CTRL		REG_PORT_CTRL_13
#define P_LINK_STATUS			REG_PORT_STATUS_0
#define P_SPEED_STATUS			REG_PORT_STATUS_1
#define P_RATE_LIMIT_CTRL		REG_PORT_CTRL_5
#define P_SA_MAC_CTRL			REG_PORT_CTRL_5
#define P_4_QUEUE_CTRL			REG_PORT_CTRL_0
#define P_2_QUEUE_CTRL			REG_PORT_CTRL_2

#define S_LINK_AGING_CTRL		REG_SWITCH_CTRL_0
#define S_MIRROR_CTRL			REG_SWITCH_CTRL_3
#define S_REPLACE_VID_CTRL		REG_SWITCH_CTRL_4
#define S_802_1P_PRIO_CTRL		REG_SWITCH_CTRL_10
#define S_UNKNOWN_DA_CTRL		REG_SWITCH_CTRL_12
#define S_TOS_PRIO_CTRL			REG_TOS_PRIO_CTRL_0
#define S_FLUSH_TABLE_CTRL		REG_SWITCH_CTRL_0
#define S_TAIL_TAG_CTRL			REG_SWITCH_CTRL_1
#define S_FORWARD_INVALID_VID_CTRL	REG_FORWARD_INVALID_VID
#define S_INS_SRC_PVID_CTRL		REG_INSERT_SRC_PVID

#define IND_ACC_TABLE(table)		((table) << 8)

#endif
