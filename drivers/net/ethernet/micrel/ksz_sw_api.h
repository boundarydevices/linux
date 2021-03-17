/**
 * Microchip switch driver API header
 *
 * Copyright (c) 2016 Microchip Technology Inc.
 *	Tristram Ha <Tristram.Ha@microchip.com>
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


#ifndef KSZ_SW_API_H
#define KSZ_SW_API_H


enum {
	DEV_INFO_SW_LINK = DEV_INFO_LAST,
	DEV_INFO_SW_ACL,
};

enum {
	DEV_SW_CFG,
};


#define SW_INFO_LINK_CHANGE		(1 << 0)
#define SW_INFO_ACL_INTR		(1 << 1)


#define SP_RX				(1 << 0)
#define SP_TX				(1 << 1)
#define SP_LEARN			(1 << 2)
#define SP_MIRROR_RX			(1 << 3)
#define SP_MIRROR_TX			(1 << 4)
#define SP_MIRROR_SNIFFER		(1 << 5)
#define SP_PHY_POWER			(1 << 6)

#define SP_BCAST_STORM			(1 << 16)
#define SP_DIFFSERV			(1 << 17)
#define SP_802_1P			(1 << 18)

struct ksz_info_cfg {
	uint set;
	uint on_off;
} __packed;

#ifndef TX_RATE_UNIT
#define TX_RATE_UNIT			10000
#endif

struct ksz_info_speed {
	uint tx_rate;
	u8 duplex;
	u8 flow_ctrl;
} __packed;

union ksz_info_data {
	struct ksz_info_cfg cfg;
	struct ksz_info_speed speed;
} __packed;

struct ksz_info_opt {
	u8 num;
	u8 port;
	union ksz_info_data data;
} __packed;

#endif
