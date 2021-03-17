/**
 * Microchip switch common code
 *
 * Copyright (c) 2015-2020 Microchip Technology Inc.
 *	Tristram Ha <Tristram.Ha@microchip.com>
 *
 * Copyright (c) 2010-2015 Micrel, Inc.
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

#define MAX_SYSFS_BUF_SIZE		(4080 - 80)

enum {
	PROC_SW_INFO,
	PROC_SW_VERSION,

	PROC_SET_SW_DUPLEX,
	PROC_SET_SW_SPEED,
	PROC_SET_SW_FORCE,
	PROC_SET_SW_FLOW_CTRL,

	PROC_SET_SW_FEATURES,
	PROC_SET_SW_OVERRIDES,
	PROC_SET_SW_MIB,

	PROC_SET_SW_REG,
	PROC_SET_SW_VID,

	PROC_DYNAMIC,
	PROC_STATIC,
	PROC_VLAN,

	PROC_SET_AGING,
	PROC_SET_FAST_AGING,
	PROC_SET_LINK_AGING,

	PROC_SET_BROADCAST_STORM,
	PROC_SET_MULTICAST_STORM,
	PROC_SET_DIFFSERV,
	PROC_SET_802_1P,

	PROC_ENABLE_VLAN,
	PROC_SET_REPLACE_NULL_VID,
	PROC_SET_MAC_ADDR,
	PROC_SET_MIRROR_MODE,
	PROC_SET_TAIL_TAG,

	PROC_SET_IGMP_SNOOP,
	PROC_SET_IPV6_MLD_SNOOP,
	PROC_SET_IPV6_MLD_OPTION,

	PROC_SET_AGGR_BACKOFF,
	PROC_SET_NO_EXC_DROP,
	PROC_SET_BUFFER_RESERVE,

	PROC_SET_HUGE_PACKET,
	PROC_SET_LEGAL_PACKET,
	PROC_SET_LENGTH_CHECK,

	PROC_SET_BACK_PRESSURE_MODE,
	PROC_SET_SWITCH_FLOW_CTRL,
	PROC_SET_SWITCH_HALF_DUPLEX,
	PROC_SET_SWITCH_10_MBIT,

	PROC_SET_RX_FLOW_CTRL,
	PROC_SET_TX_FLOW_CTRL,
	PROC_SET_FAIR_FLOW_CTRL,
	PROC_SET_VLAN_BOUNDARY,

	PROC_SET_FORWARD_UNKNOWN_DEST,
	PROC_SET_INS_TAG_0_1,
	PROC_SET_INS_TAG_0_2,
	PROC_SET_INS_TAG_1_0,
	PROC_SET_INS_TAG_1_2,
	PROC_SET_INS_TAG_2_0,
	PROC_SET_INS_TAG_2_1,

	PROC_SET_PASS_ALL,
	PROC_SET_PASS_PAUSE,

	PROC_SET_PHY_ADDR,

	PROC_GET_PORTS,
	PROC_GET_DEV_START,
	PROC_GET_VLAN_START,
	PROC_GET_STP,

#ifdef CONFIG_KSZ_STP
	PROC_GET_STP_BR_INFO,
	PROC_SET_STP_BR_ON,
	PROC_SET_STP_BR_PRIO,
	PROC_SET_STP_BR_FWD_DELAY,
	PROC_SET_STP_BR_MAX_AGE,
	PROC_SET_STP_BR_HELLO_TIME,
	PROC_SET_STP_BR_TX_HOLD,
	PROC_SET_STP_VERSION,
#endif
};

enum {
	PROC_SET_PORT_MIB,

	PROC_SET_DEF_VID,
	PROC_SET_MEMBER,

	PROC_ENABLE_BROADCAST_STORM,
	PROC_ENABLE_DIFFSERV,
	PROC_ENABLE_802_1P,

	PROC_SET_PORT_BASED,

	PROC_SET_DIS_NON_VID,
	PROC_SET_INGRESS,
	PROC_SET_INSERT_TAG,
	PROC_SET_REMOVE_TAG,
	PROC_SET_DOUBLE_TAG,
	PROC_SET_DROP_TAG,
	PROC_SET_REPLACE_PRIO,

	PROC_SET_RX,
	PROC_SET_TX,
	PROC_SET_LEARN,

	PROC_ENABLE_PRIO_QUEUE,
	PROC_SET_TX_P0_CTRL,
	PROC_SET_TX_P1_CTRL,
	PROC_SET_TX_P2_CTRL,
	PROC_SET_TX_P3_CTRL,
	PROC_SET_TX_P0_RATIO,
	PROC_SET_TX_P1_RATIO,
	PROC_SET_TX_P2_RATIO,
	PROC_SET_TX_P3_RATIO,

	PROC_ENABLE_RX_PRIO_RATE,
	PROC_ENABLE_TX_PRIO_RATE,
	PROC_SET_RX_LIMIT,
	PROC_SET_CNT_IFG,
	PROC_SET_CNT_PRE,
	PROC_SET_RX_P0_RATE,
	PROC_SET_RX_P1_RATE,
	PROC_SET_RX_P2_RATE,
	PROC_SET_RX_P3_RATE,
	PROC_SET_TX_P0_RATE,
	PROC_SET_TX_P1_RATE,
	PROC_SET_TX_P2_RATE,
	PROC_SET_TX_P3_RATE,

	PROC_SET_MIRROR_PORT,
	PROC_SET_MIRROR_RX,
	PROC_SET_MIRROR_TX,

	PROC_SET_BACK_PRESSURE,
	PROC_SET_FORCE_FLOW_CTRL,

	PROC_SET_UNKNOWN_DEF_PORT,
	PROC_SET_FORWARD_INVALID_VID,

	PROC_SET_PORT_DUPLEX,
	PROC_SET_PORT_SPEED,
	PROC_SET_LINK_MD,

	PROC_SET_PORT_MAC_ADDR,
	PROC_SET_SRC_FILTER_0,
	PROC_SET_SRC_FILTER_1,

#ifdef CONFIG_KSZ_STP
	PROC_GET_STP_INFO,
	PROC_SET_STP_ON,
	PROC_SET_STP_PRIO,
	PROC_SET_STP_ADMIN_PATH_COST,
	PROC_SET_STP_PATH_COST,
	PROC_SET_STP_ADMIN_EDGE,
	PROC_SET_STP_AUTO_EDGE,
	PROC_SET_STP_MCHECK,
	PROC_SET_STP_ADMIN_P2P,
#endif
};

enum {
	PROC_SET_STATIC_FID,
	PROC_SET_STATIC_USE_FID,
	PROC_SET_STATIC_OVERRIDE,
	PROC_SET_STATIC_VALID,
	PROC_SET_STATIC_PORTS,
	PROC_SET_STATIC_MAC_ADDR,
};

enum {
	PROC_SET_VLAN_VALID,
	PROC_SET_VLAN_MEMBER,
	PROC_SET_VLAN_FID,
	PROC_SET_VLAN_VID,
};

/* -------------------------------------------------------------------------- */

static uint get_phy_port(struct ksz_sw *sw, uint n)
{
if (n > sw->mib_port_cnt + 1)
dbg_msg("  !!! %s %d\n", __func__, n);
	if (n >= sw->mib_port_cnt + 1)
		n = 0;
	return sw->port_info[n].phy_p;
}

static uint get_log_port(struct ksz_sw *sw, uint p)
{
if (p >= sw->port_cnt)
dbg_msg("  !!! %s %d\n", __func__, p);
if (!sw->port_info[p].log_m)
dbg_msg("  ??? %s %d\n", __func__, p);
	return sw->port_info[p].log_p;
}

#if 0
static uint get_log_port_zero(struct ksz_sw *sw, uint p)
{
	uint n;

	n = get_log_port(sw, p);
	if (n)
		n--;
	else
		n = sw->mib_port_cnt;
	return n;
}
#endif

#if 0
static uint get_phy_mask_from_log(struct ksz_sw *sw, uint log_m)
{
	struct ksz_port_info *info;
	uint n;
	uint p;
	uint phy_m = 0;

	for (n = 0; n <= sw->mib_port_cnt; n++) {
		info = &sw->port_info[n];
		p = info->phy_p;
		if (log_m & sw->port_info[p].log_m)
			phy_m |= info->phy_m;
	}
	return phy_m;
}

static uint get_log_mask_from_phy(struct ksz_sw *sw, uint phy_m)
{
	struct ksz_port_info *info;
	uint n;
	uint p;
	uint log_m = 0;

	for (n = 0; n <= sw->mib_port_cnt; n++) {
		info = &sw->port_info[n];
		p = info->phy_p;
		if (phy_m & info->phy_m)
			log_m |= sw->port_info[p].log_m;
	}
	return log_m;
}
#endif

static uint get_sysfs_port(struct ksz_sw *sw, uint n)
{
	return n;
}

static inline struct ksz_port_cfg *get_port_cfg(struct ksz_sw *sw, uint p)
{
	return &sw->info->port_cfg[p];
}

static inline struct ksz_port_info *get_port_info(struct ksz_sw *sw, uint p)
{
	return &sw->port_info[p];
}

static inline struct ksz_port_mib *get_port_mib(struct ksz_sw *sw, uint p)
{
	return &sw->port_mib[p];
}

static void sw_acquire(struct ksz_sw *sw)
{
	mutex_lock(&sw->lock);
	mutex_lock(sw->reglock);
}  /* sw_acquire */

static void sw_release(struct ksz_sw *sw)
{
	mutex_unlock(sw->reglock);
	mutex_unlock(&sw->lock);
}  /* sw_release */

/* -------------------------------------------------------------------------- */

/*
#define STATIC_MAC_TABLE_ADDR		00-0000FFFF-FFFFFFFF
#define STATIC_MAC_TABLE_FWD_PORTS	00-00070000-00000000
#define STATIC_MAC_TABLE_VALID		00-00080000-00000000
#define STATIC_MAC_TABLE_OVERRIDE	00-00100000-00000000
#define STATIC_MAC_TABLE_USE_FID	00-00200000-00000000
#define STATIC_MAC_TABLE_FID		00-03C00000-00000000
*/

#define STATIC_MAC_TABLE_ADDR		0x0000FFFF
#define STATIC_MAC_TABLE_FWD_PORTS	0x00070000
#define STATIC_MAC_TABLE_VALID		0x00080000
#define STATIC_MAC_TABLE_OVERRIDE	0x00100000
#define STATIC_MAC_TABLE_USE_FID	0x00200000
#define STATIC_MAC_TABLE_FID		0x03C00000

#define STATIC_MAC_FWD_PORTS_SHIFT	16
#define STATIC_MAC_FID_SHIFT		22

/*
#define VLAN_TABLE_VID			00-00000000-00000FFF
#define VLAN_TABLE_FID			00-00000000-0000F000
#define VLAN_TABLE_MEMBERSHIP		00-00000000-00070000
#define VLAN_TABLE_VALID		00-00000000-00080000
*/

#define VLAN_TABLE_VID			0x00000FFF
#define VLAN_TABLE_FID			0x0000F000
#define VLAN_TABLE_MEMBERSHIP		0x00070000
#define VLAN_TABLE_VALID		0x00080000

#define VLAN_TABLE_FID_SHIFT		12
#define VLAN_TABLE_MEMBERSHIP_SHIFT	16

/*
#define DYNAMIC_MAC_TABLE_ADDR		00-0000FFFF-FFFFFFFF
#define DYNAMIC_MAC_TABLE_FID		00-000F0000-00000000
#define DYNAMIC_MAC_TABLE_SRC_PORT	00-00300000-00000000
#define DYNAMIC_MAC_TABLE_TIMESTAMP	00-00C00000-00000000
#define DYNAMIC_MAC_TABLE_ENTRIES	03-FF000000-00000000
#define DYNAMIC_MAC_TABLE_MAC_EMPTY	04-00000000-00000000
#define DYNAMIC_MAC_TABLE_RESERVED	78-00000000-00000000
#define DYNAMIC_MAC_TABLE_NOT_READY	80-00000000-00000000
*/

#define DYNAMIC_MAC_TABLE_ADDR		0x0000FFFF
#define DYNAMIC_MAC_TABLE_FID		0x000F0000
#define DYNAMIC_MAC_TABLE_SRC_PORT	0x00300000
#define DYNAMIC_MAC_TABLE_TIMESTAMP	0x00C00000
#define DYNAMIC_MAC_TABLE_ENTRIES	0xFF000000

#define DYNAMIC_MAC_TABLE_ENTRIES_H	0x03
#define DYNAMIC_MAC_TABLE_MAC_EMPTY	0x04
#define DYNAMIC_MAC_TABLE_RESERVED	0x78
#define DYNAMIC_MAC_TABLE_NOT_READY	0x80

#define DYNAMIC_MAC_FID_SHIFT		16
#define DYNAMIC_MAC_SRC_PORT_SHIFT	20
#define DYNAMIC_MAC_TIMESTAMP_SHIFT	22
#define DYNAMIC_MAC_ENTRIES_SHIFT	24
#define DYNAMIC_MAC_ENTRIES_H_SHIFT	8

/*
#define MIB_COUNTER_VALUE		00-00000000-3FFFFFFF
#define MIB_COUNTER_VALID		00-00000000-40000000
#define MIB_COUNTER_OVERFLOW		00-00000000-80000000
*/

#ifndef MIB_COUNTER_OVERFLOW
#define MIB_COUNTER_OVERFLOW		(1 << 31)
#define MIB_COUNTER_VALID		(1 << 30)
#define MIB_COUNTER_VALUE		0x3FFFFFFF
#endif

#define KS_MIB_PACKET_DROPPED_TX_0	0x100
#define KS_MIB_PACKET_DROPPED_TX_1	0x101
#define KS_MIB_PACKET_DROPPED_TX	0x102
#define KS_MIB_PACKET_DROPPED_RX_0	0x103
#define KS_MIB_PACKET_DROPPED_RX_1	0x104
#define KS_MIB_PACKET_DROPPED_RX	0x105

#define MIB_PACKET_DROPPED		0x0000FFFF

#ifdef CONFIG_1588_PTP
#include "ksz_ptp.c"
#endif

/* -------------------------------------------------------------------------- */

/* Switch functions */

#define HW_DELAY(sw, reg)			\
	do {					\
		u16 dummy;			\
		dummy = SW_R(sw, reg);		\
	} while (0)

/**
 * sw_r_table - read 4 bytes of data from switch table
 * @sw:		The switch instance.
 * @table:	The table selector.
 * @addr:	The address of the table entry.
 * @data:	Buffer to store the read data.
 *
 * This routine reads 4 bytes of data from the table of the switch.
 * Hardware is locked to minimize corruption of read data.
 */
static void sw_r_table(struct ksz_sw *sw, int table, u16 addr, u32 *data)
{
	u16 ctrl_addr;

	ctrl_addr = IND_ACC_TABLE(table | TABLE_READ) | addr;

	mutex_lock(sw->reglock);
	sw->reg->w16(sw, REG_IND_CTRL_0, ctrl_addr);
	HW_DELAY(sw, REG_IND_CTRL_0);
	*data = sw->reg->r32(sw, REG_IND_DATA_LO);
	mutex_unlock(sw->reglock);
}

/**
 * sw_w_table - write 4 bytes of data to the switch table
 * @sw:		The switch instance.
 * @table:	The table selector.
 * @addr:	The address of the table entry.
 * @data:	Data to be written.
 *
 * This routine writes 4 bytes of data to the table of the switch.
 * Hardware is locked to minimize corruption of written data.
 */
static void sw_w_table(struct ksz_sw *sw, int table, u16 addr, u32 data)
{
	u16 ctrl_addr;

	ctrl_addr = IND_ACC_TABLE(table) | addr;

	mutex_lock(sw->reglock);
	sw->reg->w32(sw, REG_IND_DATA_LO, data);
	sw->reg->w16(sw, REG_IND_CTRL_0, ctrl_addr);
	HW_DELAY(sw, REG_IND_CTRL_0);
	mutex_unlock(sw->reglock);
}

/**
 * sw_r_table_64 - read 8 bytes of data from the switch table
 * @sw:		The switch instance.
 * @table:	The table selector.
 * @addr:	The address of the table entry.
 * @data_hi:	Buffer to store the high part of read data (bit63 ~ bit32).
 * @data_lo:	Buffer to store the low part of read data (bit31 ~ bit0).
 *
 * This routine reads 8 bytes of data from the table of the switch.
 * Hardware is locked to minimize corruption of read data.
 */
static void sw_r_table_64(struct ksz_sw *sw, int table, u16 addr, u32 *data_hi,
	u32 *data_lo)
{
	u16 ctrl_addr;

	ctrl_addr = IND_ACC_TABLE(table | TABLE_READ) | addr;

	mutex_lock(sw->reglock);
	sw->reg->w16(sw, REG_IND_CTRL_0, ctrl_addr);
	HW_DELAY(sw, REG_IND_CTRL_0);
	*data_hi = sw->reg->r32(sw, REG_IND_DATA_HI);
	*data_lo = sw->reg->r32(sw, REG_IND_DATA_LO);
	mutex_unlock(sw->reglock);
}

/**
 * sw_w_table_64 - write 8 bytes of data to the switch table
 * @sw:		The switch instance.
 * @table:	The table selector.
 * @addr:	The address of the table entry.
 * @data_hi:	The high part of data to be written (bit63 ~ bit32).
 * @data_lo:	The low part of data to be written (bit31 ~ bit0).
 *
 * This routine writes 8 bytes of data to the table of the switch.
 * Hardware is locked to minimize corruption of written data.
 */
static void sw_w_table_64(struct ksz_sw *sw, int table, u16 addr, u32 data_hi,
	u32 data_lo)
{
	u16 ctrl_addr;

	ctrl_addr = IND_ACC_TABLE(table) | addr;

	mutex_lock(sw->reglock);
	sw->reg->w32(sw, REG_IND_DATA_HI, data_hi);
	sw->reg->w32(sw, REG_IND_DATA_LO, data_lo);
	sw->reg->w16(sw, REG_IND_CTRL_0, ctrl_addr);
	HW_DELAY(sw, REG_IND_CTRL_0);
	mutex_unlock(sw->reglock);
}

static inline int valid_dyn_entry(struct ksz_sw *sw, u8 *data)
{
	int timeout = 100;

	do {
		*data = sw->reg->r8(sw, REG_IND_DATA_CHECK);
		timeout--;
	} while ((*data & DYNAMIC_MAC_TABLE_NOT_READY) && timeout);

	/* Entry is not ready for accessing. */
	if (*data & DYNAMIC_MAC_TABLE_NOT_READY)
		return 1;

	/* Entry is ready for accessing. */
	else {
		/* There is no valid entry in the table. */
		if (*data & DYNAMIC_MAC_TABLE_MAC_EMPTY)
			return 2;
	}
	return 0;
}

/**
 * sw_r_dyn_mac_table - read from dynamic MAC table
 * @sw:		The switch instance.
 * @addr:	The address of the table entry.
 * @mac_addr:	Buffer to store the MAC address.
 * @fid:	Buffer to store the FID.
 * @src_port:	Buffer to store the source port number.
 * @timestamp:	Buffer to store the timestamp.
 * @entries:	Buffer to store the number of entries.  If this is zero, the
 *		table is empty and so this function should not be called again
 *		until later.
 *
 * This function reads an entry of the dynamic MAC table of the switch.
 * Hardware is locked to minimize corruption of read data.
 *
 * Return 0 if the entry is successfully read; otherwise -1.
 */
static int sw_r_dyn_mac_table(struct ksz_sw *sw, u16 addr, u8 *mac_addr,
	u8 *fid, u8 *src_port, u8 *timestamp, u16 *entries)
{
	u32 data_hi;
	u32 data_lo;
	u16 ctrl_addr;
	int rc;
	u8 data;

	ctrl_addr = IND_ACC_TABLE(TABLE_DYNAMIC_MAC | TABLE_READ) | addr;

	mutex_lock(sw->reglock);
	sw->reg->w16(sw, REG_IND_CTRL_0, ctrl_addr);
	HW_DELAY(sw, REG_IND_CTRL_0);

	rc = valid_dyn_entry(sw, &data);
	if (1 == rc) {
		if (0 == addr)
			*entries = 0;
	} else if (2 == rc)
		*entries = 0;
	/* At least one valid entry in the table. */
	else {
		data_hi = sw->reg->r32(sw, REG_IND_DATA_HI);

		/* Check out how many valid entry in the table. */
		*entries = (u16)(((((u16)
			data & DYNAMIC_MAC_TABLE_ENTRIES_H) <<
			DYNAMIC_MAC_ENTRIES_H_SHIFT) |
			(((data_hi & DYNAMIC_MAC_TABLE_ENTRIES) >>
			DYNAMIC_MAC_ENTRIES_SHIFT))) + 1);

		*fid = (u8)((data_hi & DYNAMIC_MAC_TABLE_FID) >>
			DYNAMIC_MAC_FID_SHIFT);
		*src_port = (u8)((data_hi & DYNAMIC_MAC_TABLE_SRC_PORT) >>
			DYNAMIC_MAC_SRC_PORT_SHIFT);
		*timestamp = (u8)((
			data_hi & DYNAMIC_MAC_TABLE_TIMESTAMP) >>
			DYNAMIC_MAC_TIMESTAMP_SHIFT);

		data_lo = sw->reg->r32(sw, REG_IND_DATA_LO);

		mac_addr[5] = (u8) data_lo;
		mac_addr[4] = (u8)(data_lo >> 8);
		mac_addr[3] = (u8)(data_lo >> 16);
		mac_addr[2] = (u8)(data_lo >> 24);

		mac_addr[1] = (u8) data_hi;
		mac_addr[0] = (u8)(data_hi >> 8);
		rc = 0;
	}
	mutex_unlock(sw->reglock);

	return rc;
}

static ssize_t sw_d_dyn_mac_table(struct ksz_sw *sw, char *buf, ssize_t len)
{
	u16 entries = 0;
	u16 i;
	u8 mac_addr[ETH_ALEN];
	char str[80];
	u8 port = 0;
	u8 timestamp = 0;
	u8 fid = 0;
	int locked = mutex_is_locked(&sw->lock);
	int first_break = true;

	if (locked)
		mutex_unlock(sw->reglock);
	memset(mac_addr, 0, ETH_ALEN);
	i = 0;
	do {
		if (!sw_r_dyn_mac_table(sw, i, mac_addr, &fid, &port,
				&timestamp, &entries)) {
			if (len >= MAX_SYSFS_BUF_SIZE && first_break) {
				first_break = false;
				len += sprintf(buf + len, "...\n");
			}
			sprintf(str,
				"%02X:%02X:%02X:%02X:%02X:%02X"
				"  f:%x  p:%x  t:%x  %03x\n",
				mac_addr[0], mac_addr[1], mac_addr[2],
				mac_addr[3], mac_addr[4], mac_addr[5],
				fid, port, timestamp, entries);
			if (len < MAX_SYSFS_BUF_SIZE)
				len += sprintf(buf + len, "%s", str);
			else
				printk(KERN_INFO "%s", str);
		}
		i++;
	} while (i < entries);
	if (locked)
		mutex_lock(sw->reglock);
	return len;
}

/**
 * sw_r_sta_mac_table - read from static MAC table
 * @sw:		The switch instance.
 * @addr:	The address of the table entry.
 * @mac_addr:	Buffer to store the MAC address.
 * @ports:	Buffer to store the port members.
 * @override:	Buffer to store the override flag.
 * @use_fid:	Buffer to store the use FID flag which indicates the FID is
 *		valid.
 * @fid:	Buffer to store the FID.
 *
 * This function reads an entry of the static MAC table of the switch.  It
 * calls sw_r_table_64() to get the data.
 *
 * Return 0 if the entry is valid; otherwise -1.
 */
static int sw_r_sta_mac_table(struct ksz_sw *sw, u16 addr, u8 *mac_addr,
	u8 *ports, int *override, int *use_fid, u8 *fid)
{
	u32 data_hi;
	u32 data_lo;
	int locked = mutex_is_locked(&sw->lock);

	if (locked)
		mutex_unlock(sw->reglock);
	sw_r_table_64(sw, TABLE_STATIC_MAC, addr, &data_hi, &data_lo);
	if (locked)
		mutex_lock(sw->reglock);
	if (data_hi & (STATIC_MAC_TABLE_VALID | STATIC_MAC_TABLE_OVERRIDE)) {
		mac_addr[5] = (u8) data_lo;
		mac_addr[4] = (u8)(data_lo >> 8);
		mac_addr[3] = (u8)(data_lo >> 16);
		mac_addr[2] = (u8)(data_lo >> 24);
		mac_addr[1] = (u8) data_hi;
		mac_addr[0] = (u8)(data_hi >> 8);
		*ports = (u8)((data_hi & STATIC_MAC_TABLE_FWD_PORTS) >>
			STATIC_MAC_FWD_PORTS_SHIFT);
		*override = (data_hi & STATIC_MAC_TABLE_OVERRIDE) ? 1 : 0;
		*use_fid = (data_hi & STATIC_MAC_TABLE_USE_FID) ? 1 : 0;
		*fid = (u8)((data_hi & STATIC_MAC_TABLE_FID) >>
			STATIC_MAC_FID_SHIFT);
		return 0;
	}
	return -1;
}

/**
 * sw_w_sta_mac_table - write to the static MAC table
 * @sw:		The switch instance.
 * @addr:	The address of the table entry.
 * @mac_addr:	The MAC address.
 * @ports:	The port members.
 * @override:	The flag to override the port receive/transmit settings.
 * @valid:	The flag to indicate entry is valid.
 * @use_fid:	The flag to indicate the FID is valid.
 * @fid:	The FID value.
 *
 * This routine writes an entry of the static MAC table of the switch.  It
 * calls sw_w_table_64() to write the data.
 */
static void sw_w_sta_mac_table(struct ksz_sw *sw, u16 addr, u8 *mac_addr,
	u8 ports, int override, int valid, int use_fid, u8 fid)
{
	u32 data_hi;
	u32 data_lo;
	int locked = mutex_is_locked(&sw->lock);

	data_lo = ((u32) mac_addr[2] << 24) |
		((u32) mac_addr[3] << 16) |
		((u32) mac_addr[4] << 8) | mac_addr[5];
	data_hi = ((u32) mac_addr[0] << 8) | mac_addr[1];
	data_hi |= (u32) ports << STATIC_MAC_FWD_PORTS_SHIFT;

	if (override)
		data_hi |= STATIC_MAC_TABLE_OVERRIDE;
	if (use_fid) {
		data_hi |= STATIC_MAC_TABLE_USE_FID;
		data_hi |= (u32) fid << STATIC_MAC_FID_SHIFT;
	}
	if (valid)
		data_hi |= STATIC_MAC_TABLE_VALID;
	else
		data_hi &= ~STATIC_MAC_TABLE_OVERRIDE;

	if (locked)
		mutex_unlock(sw->reglock);
	sw_w_table_64(sw, TABLE_STATIC_MAC, addr, data_hi, data_lo);
	if (locked)
		mutex_lock(sw->reglock);
}

static ssize_t sw_d_sta_mac_table(struct ksz_sw *sw, char *buf, ssize_t len)
{
	u16 i;
	u8 mac_addr[ETH_ALEN];
	u8 ports;
	int override;
	int use_fid;
	u8 fid;

	i = 0;
	do {
		if (!sw_r_sta_mac_table(sw, i, mac_addr, &ports, &override,
				&use_fid, &fid)) {
			len += sprintf(buf + len,
				"%d: %02X:%02X:%02X:%02X:%02X:%02X "
				"%x %u %u:%x\n",
				i, mac_addr[0], mac_addr[1], mac_addr[2],
				mac_addr[3], mac_addr[4], mac_addr[5],
				ports, override, use_fid, fid);
		}
		i++;
	} while (i < STATIC_MAC_TABLE_ENTRIES);
	return len;
}

static ssize_t sw_d_mac_table(struct ksz_sw *sw, char *buf, ssize_t len)
{
	struct ksz_mac_table *entry;
	struct ksz_alu_table *alu;
	int i;
	int first_static = true;

	i = STATIC_MAC_TABLE_ENTRIES;
	do {
		entry = &sw->info->mac_table[i];
		if (entry->valid) {
			if (first_static) {
				first_static = false;
				len += sprintf(buf + len, "\n");
			}
			alu = &sw->info->alu_table[i];
			len += sprintf(buf + len,
				"%2x: %02X:%02X:%02X:%02X:%02X:%02X "
				"%x %u %u:%x  %02x:%x\n",
				i, entry->mac_addr[0], entry->mac_addr[1],
				entry->mac_addr[2], entry->mac_addr[3],
				entry->mac_addr[4], entry->mac_addr[5],
				entry->ports, entry->override, entry->use_fid,
				entry->fid,
				alu->forward, alu->owner);
		}
		i++;
		if (SWITCH_MAC_TABLE_ENTRIES == i)
			first_static = true;
	} while (i < MULTI_MAC_TABLE_ENTRIES);
	return len;
}

/* -------------------------------------------------------------------------- */

/**
 * sw_r_vlan_table - read from the VLAN table
 * @sw:		The switch instance.
 * @addr:	The address of the table entry.
 * @vid:	Buffer to store the VID.
 * @fid:	Buffer to store the VID.
 * @member:	Buffer to store the port membership.
 *
 * This function reads an entry of the VLAN table of the switch.  It calls
 * sw_r_table() to get the data.
 *
 * Return 0 if the entry is valid; otherwise -1.
 */
static int sw_r_vlan_table(struct ksz_sw *sw, u16 addr, u16 *vid, u8 *fid,
	u8 *member)
{
	u32 data;
	int locked = mutex_is_locked(&sw->lock);

	if (locked)
		mutex_unlock(sw->reglock);
	sw_r_table(sw, TABLE_VLAN, addr, &data);
	if (locked)
		mutex_lock(sw->reglock);
	if (data & VLAN_TABLE_VALID) {
		*vid = (u16)(data & VLAN_TABLE_VID);
		*fid = (u8)((data & VLAN_TABLE_FID) >> VLAN_TABLE_FID_SHIFT);
		*member = (u8)((data & VLAN_TABLE_MEMBERSHIP) >>
			VLAN_TABLE_MEMBERSHIP_SHIFT);
		return 0;
	}
	return -1;
}

/**
 * sw_w_vlan_table - write to the VLAN table
 * @sw:		The switch instance.
 * @addr:	The address of the table entry.
 * @vid:	The VID value.
 * @fid:	The FID value.
 * @member:	The port membership.
 * @valid:	The flag to indicate entry is valid.
 *
 * This routine writes an entry of the VLAN table of the switch.  It calls
 * sw_w_table() to write the data.
 */
static void sw_w_vlan_table(struct ksz_sw *sw, u16 addr, u16 vid, u8 fid,
	u8 member, int valid)
{
	u32 data;
	int entry;
	struct ksz_sw_info *info = sw->info;
	int locked = mutex_is_locked(&sw->lock);

	data = vid;
	data |= (u32) fid << VLAN_TABLE_FID_SHIFT;
	data |= (u32) member << VLAN_TABLE_MEMBERSHIP_SHIFT;
	if (valid)
		data |= VLAN_TABLE_VALID;

	if (locked)
		mutex_unlock(sw->reglock);
	sw_w_table(sw, TABLE_VLAN, addr, data);
	if (locked)
		mutex_lock(sw->reglock);

	entry = addr;
	if (entry >= VLAN_TABLE_ENTRIES)
		return;
	info->vlan_table[entry].vid = vid;
	info->vlan_table[entry].fid = fid;
	info->vlan_table[entry].member = member;
	info->vlan_table[entry].valid = valid;
}

static ssize_t sw_d_vlan_table(struct ksz_sw *sw, char *buf, ssize_t len)
{
	u16 i;
	u16 vid;
	u8 fid;
	u8 member;
	struct ksz_sw_info *info = sw->info;

	i = 0;
	do {
		if (!sw_r_vlan_table(sw, i, &vid, &fid, &member)) {
			info->vlan_table[i].vid = vid;
			info->vlan_table[i].fid = fid;
			info->vlan_table[i].member = member;
			info->vlan_table[i].valid = 1;
			len += sprintf(buf + len,
				"%x: 0x%03x %x %x\n", i, vid, fid, member);
		} else
			info->vlan_table[i].valid = 0;
		i++;
	} while (i < VLAN_TABLE_ENTRIES);
	return len;
}

#ifdef CONFIG_MRP
#include "ksz_mrp.c"
#endif

/* -------------------------------------------------------------------------- */

/*
 * Some counters do not need to be read too often because they are less likely
 * to increase much.
 */
static u8 mib_read_max[SWITCH_COUNTER_NUM] = {
	1,
	1,
	4,
	4,
	4,
	4,
	4,
	4,
	4,
	4,
	1,
	1,
	1,
	1,
	2,
	2,
	2,
	2,
	2,
	2,

	1,
	1,
	4,
	1,
	1,
	1,
	1,
	4,
	4,
	4,
	4,
	4,
};

/**
 * port_r_mib_cnt - read MIB counter
 * @sw:		The switch instance.
 * @port:	The port index.
 * @addr:	The address of the counter.
 * @cnt:	Buffer to store the counter.
 *
 * This routine reads a MIB counter of the port.
 * Hardware is locked to minimize corruption of read data.
 */
static void port_r_mib_cnt(struct ksz_sw *sw, uint port, u16 addr, u64 *cnt)
{
	u32 data;
	u16 ctrl_addr;
	int timeout;

	ctrl_addr = addr + SWITCH_COUNTER_NUM * port;

	mutex_lock(sw->reglock);

	ctrl_addr |= IND_ACC_TABLE(TABLE_MIB | TABLE_READ);
	sw->reg->w16(sw, REG_IND_CTRL_0, ctrl_addr);
	HW_DELAY(sw, REG_IND_CTRL_0);

	for (timeout = 2; timeout > 0; timeout--) {
		data = sw->reg->r32(sw, REG_IND_DATA_LO);

if (data & 0x3f000000)
dbg_msg(" mib: %d=%04x %08x\n", port, ctrl_addr, data);
		if (data & MIB_COUNTER_VALID) {
			if (data & MIB_COUNTER_OVERFLOW)
dbg_msg(" overflow: %d %x\n", addr, ctrl_addr);
			if (data & MIB_COUNTER_OVERFLOW)
				*cnt += MIB_COUNTER_VALUE + 1;
			*cnt += data & MIB_COUNTER_VALUE;
			break;
		}
	}

	mutex_unlock(sw->reglock);
}  /* port_r_mib_cnt */

/**
 * port_r_mib_pkt - read dropped packet counts
 * @sw:		The switch instance.
 * @port:	The port index.
 * @cnt:	Buffer to store the receive and transmit dropped packet counts.
 *
 * This routine reads the dropped packet counts of the port.
 * Hardware is locked to minimize corruption of read data.
 */
static void port_r_mib_pkt(struct ksz_sw *sw, uint port, u32 *last, u64 *cnt)
{
	u32 cur;
	u32 data;
	u16 ctrl_addr;
	int index;

	index = KS_MIB_PACKET_DROPPED_RX_0 + port;
	do {
		ctrl_addr = index;

		mutex_lock(sw->reglock);

		ctrl_addr |= IND_ACC_TABLE(TABLE_MIB | TABLE_READ);
		sw->reg->w16(sw, REG_IND_CTRL_0, ctrl_addr);
		HW_DELAY(sw, REG_IND_CTRL_0);
		data = sw->reg->r32(sw, REG_IND_DATA_LO);

		mutex_unlock(sw->reglock);

		data &= MIB_PACKET_DROPPED;
		cur = *last;
		if (data != cur) {
			*last = data;
			if (data < cur)
				data += MIB_PACKET_DROPPED + 1;
			data -= cur;
			*cnt += data;
		}
		++last;
		++cnt;
		index -= KS_MIB_PACKET_DROPPED_TX -
			KS_MIB_PACKET_DROPPED_TX_0 + 1;
	} while (index >= KS_MIB_PACKET_DROPPED_TX_0 + port);
}  /* port_r_mib_pkt */

static int exit_mib_read(struct ksz_sw *sw)
{
	if (sw->intr_using)
		return true;
	return false;
}  /* exit_mib_read */

/**
 * port_r_cnt - read MIB counters periodically
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine is used to read the counters of the port periodically to avoid
 * counter overflow.  The hardware should be acquired first before calling this
 * routine.
 *
 * Return non-zero when not all counters not read.
 */
static int port_r_cnt(struct ksz_sw *sw, uint port)
{
	struct ksz_port_mib *mib = get_port_mib(sw, port);

	if (mib->mib_start < SWITCH_COUNTER_NUM)
		while (mib->cnt_ptr < SWITCH_COUNTER_NUM) {
			if (exit_mib_read(sw))
				return mib->cnt_ptr;
			++mib->read_cnt[mib->cnt_ptr];
			if (mib->read_cnt[mib->cnt_ptr] >=
					mib->read_max[mib->cnt_ptr]) {
				mib->read_cnt[mib->cnt_ptr] = 0;
				port_r_mib_cnt(sw, port, mib->cnt_ptr,
					&mib->counter[mib->cnt_ptr]);
			}
			++mib->cnt_ptr;
		}
	if (sw->mib_cnt > SWITCH_COUNTER_NUM)
		port_r_mib_pkt(sw, port, mib->dropped,
			&mib->counter[SWITCH_COUNTER_NUM]);
	mib->cnt_ptr = 0;
	return 0;
}  /* port_r_cnt */

/**
 * port_init_cnt - initialize MIB counter values
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine is used to initialize all counters to zero if the hardware
 * cannot do it after reset.
 */
static inline void port_init_cnt(struct ksz_sw *sw, uint port)
{
	struct ksz_port_mib *mib = get_port_mib(sw, port);

	mutex_lock(&sw->lock);
	mib->cnt_ptr = 0;
	if (mib->mib_start < SWITCH_COUNTER_NUM)
		do {
			mib->read_cnt[mib->cnt_ptr] = 0;
			mib->read_max[mib->cnt_ptr] =
				mib_read_max[mib->cnt_ptr];
			port_r_mib_cnt(sw, port, mib->cnt_ptr,
				&mib->counter[mib->cnt_ptr]);
			++mib->cnt_ptr;
		} while (mib->cnt_ptr < SWITCH_COUNTER_NUM);
	if (sw->mib_cnt > SWITCH_COUNTER_NUM)
		port_r_mib_pkt(sw, port, mib->dropped,
			&mib->counter[SWITCH_COUNTER_NUM]);
	memset((void *) mib->counter, 0, sizeof(u64) *
		TOTAL_SWITCH_COUNTER_NUM);
	mib->cnt_ptr = 0;
	mib->rate[0].last = mib->rate[1].last = 0;
	mib->rate[0].last_cnt = mib->rate[1].last_cnt = 0;
	mib->rate[0].peak = mib->rate[1].peak = 0;
	mutex_unlock(&sw->lock);
}  /* port_init_cnt */

/* -------------------------------------------------------------------------- */

/*
 * Port functions
 */

/**
 * port_chk - check port register bits
 * @sw:		The switch instance.
 * @port:	The port index.
 * @offset:	The offset of the port register.
 * @bits:	The data bits to check.
 *
 * This function checks whether the specified bits of the port register are set
 * or not.
 *
 * Return 0 if the bits are not set.
 */
static int port_chk(struct ksz_sw *sw, uint port, int offset, SW_D bits)
{
	u32 addr;
	SW_D data;

	PORT_CTRL_ADDR(port, addr);
	addr += offset;
	data = SW_R(sw, addr);
	return (data & bits) == bits;
}

/**
 * port_cfg - set port register bits
 * @sw:		The switch instance.
 * @port:	The port index.
 * @offset:	The offset of the port register.
 * @bits:	The data bits to set.
 * @set:	The flag indicating whether the bits are to be set or not.
 *
 * This routine sets or resets the specified bits of the port register.
 */
static void port_cfg(struct ksz_sw *sw, uint port, int offset, SW_D bits,
	bool set)
{
	u32 addr;
	SW_D data;

	PORT_CTRL_ADDR(port, addr);
	addr += offset;
	data = SW_R(sw, addr);
	if (set)
		data |= bits;
	else
		data &= ~bits;
	SW_W(sw, addr, data);
}

/**
 * port_chk_shift - check port bit
 * @sw:		The switch instance.
 * @port:	The port index.
 * @offset:	The offset of the register.
 * @shift:	Number of bits to shift.
 *
 * This function checks whether the specified port is set in the register or
 * not.
 *
 * Return 0 if the port is not set.
 */
static int port_chk_shift(struct ksz_sw *sw, uint port, u32 addr, int shift)
{
	SW_D data;
	SW_D bit = 1 << port;

	data = SW_R(sw, addr);
	data >>= shift;
	return (data & bit) == bit;
}

/**
 * port_cfg_shift - set port bit
 * @sw:		The switch instance.
 * @port:	The port index.
 * @offset:	The offset of the register.
 * @shift:	Number of bits to shift.
 * @set:	The flag indicating whether the port is to be set or not.
 *
 * This routine sets or resets the specified port in the register.
 */
static void port_cfg_shift(struct ksz_sw *sw, uint port, u32 addr, int shift,
	bool set)
{
	SW_D data;
	SW_D bits = 1 << port;

	data = SW_R(sw, addr);
	bits <<= shift;
	if (set)
		data |= bits;
	else
		data &= ~bits;
	SW_W(sw, addr, data);
}

/**
 * port_r8 - read byte from port register
 * @sw:		The switch instance.
 * @port:	The port index.
 * @offset:	The offset of the port register.
 * @data:	Buffer to store the data.
 *
 * This routine reads a byte from the port register.
 */
static void port_r8(struct ksz_sw *sw, uint port, int offset, u8 *data)
{
	u32 addr;

	PORT_CTRL_ADDR(port, addr);
	addr += offset;
	*data = sw->reg->r8(sw, addr);
}

/**
 * port_w8 - write byte to port register
 * @sw:		The switch instance.
 * @port:	The port index.
 * @offset:	The offset of the port register.
 * @data:	Data to write.
 *
 * This routine writes a byte to the port register.
 */
static void port_w8(struct ksz_sw *sw, uint port, int offset, u8 data)
{
	u32 addr;

	PORT_CTRL_ADDR(port, addr);
	addr += offset;
	sw->reg->w8(sw, addr, data);
}

/**
 * port_r16 - read word from port register.
 * @sw:		The switch instance.
 * @port:	The port index.
 * @offset:	The offset of the port register.
 * @data:	Buffer to store the data.
 *
 * This routine reads a word from the port register.
 */
static void port_r16(struct ksz_sw *sw, uint port, int offset, u16 *data)
{
	u32 addr;

	PORT_CTRL_ADDR(port, addr);
	addr += offset;
	*data = sw->reg->r16(sw, addr);
}

/**
 * port_w16 - write word to port register.
 * @sw:		The switch instance.
 * @port:	The port index.
 * @offset:	The offset of the port register.
 * @data:	Data to write.
 *
 * This routine writes a word to the port register.
 */
static void port_w16(struct ksz_sw *sw, uint port, int offset, u16 data)
{
	u32 addr;

	PORT_CTRL_ADDR(port, addr);
	addr += offset;
	sw->reg->w16(sw, addr, data);
}

/**
 * sw_chk - check switch register bits
 * @sw:		The switch instance.
 * @addr:	The address of the switch register.
 * @bits:	The data bits to check.
 *
 * This function checks whether the specified bits of the switch register are
 * set or not.
 *
 * Return 0 if the bits are not set.
 */
static int sw_chk(struct ksz_sw *sw, u32 addr, SW_D bits)
{
	SW_D data;

	data = SW_R(sw, addr);
	return (data & bits) == bits;
}

/**
 * sw_cfg - set switch register bits
 * @sw:		The switch instance.
 * @addr:	The address of the switch register.
 * @bits:	The data bits to set.
 * @set:	The flag indicating whether the bits are to be set or not.
 *
 * This function sets or resets the specified bits of the switch register.
 */
static void sw_cfg(struct ksz_sw *sw, u32 addr, SW_D bits, bool set)
{
	SW_D data;

	data = SW_R(sw, addr);
	if (set)
		data |= bits;
	else
		data &= ~bits;
	SW_W(sw, addr, data);
}

#ifdef PORT_OUT_RATE_ADDR
/**
 * port_out_rate_r8 - read byte from port register
 * @sw:		The switch instance.
 * @port:	The port index.
 * @offset:	The offset of the port register.
 * @data:	Buffer to store the data.
 *
 * This routine reads a byte from the port register.
 */
static void port_out_rate_r8(struct ksz_sw *sw, uint port, int offset, u8 *data)
{
	u32 addr;

	PORT_OUT_RATE_ADDR(port, addr);
	addr += offset;
	*data = sw->reg->r8(sw, addr);
}

/**
 * port_out_rate_w8 - write byte to port register
 * @sw:		The switch instance.
 * @port:	The port index.
 * @offset:	The offset of the port register.
 * @data:	Data to write.
 *
 * This routine writes a byte to the port register.
 */
static void port_out_rate_w8(struct ksz_sw *sw, uint port, int offset, u8 data)
{
	u32 addr;

	PORT_OUT_RATE_ADDR(port, addr);
	addr += offset;
	sw->reg->w8(sw, addr, data);
}
#endif

/* -------------------------------------------------------------------------- */

/* Bandwidth */

static inline void port_cfg_broad_storm(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_BCAST_STORM_CTRL, PORT_BROADCAST_STORM, set);
}

static inline int port_chk_broad_storm(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_BCAST_STORM_CTRL, PORT_BROADCAST_STORM);
}

/* Driver set switch broadcast storm protection at 10% rate. */
#define BROADCAST_STORM_PROTECTION_RATE	10

/* 148,800 frames * 67 ms / 100 */
#define BROADCAST_STORM_VALUE		9969

/**
 * sw_cfg_broad_storm - configure broadcast storm threshold
 * @sw:		The switch instance.
 * @percent:	Broadcast storm threshold in percent of transmit rate.
 *
 * This routine configures the broadcast storm threshold of the switch.
 */
static void sw_cfg_broad_storm(struct ksz_sw *sw, u8 percent)
{
	u16 data;
	u32 value = ((u32) BROADCAST_STORM_VALUE * (u32) percent / 100);

	if (value > BROADCAST_STORM_RATE)
		value = BROADCAST_STORM_RATE;

	data = sw->reg->r16(sw, S_REPLACE_VID_CTRL);
#if (SW_SIZE == (2))
	data = ntohs(data);
#endif
	data &= ~BROADCAST_STORM_RATE;
	data |= value;
#if (SW_SIZE == (2))
	data = ntohs(data);
#endif
	sw->reg->w16(sw, S_REPLACE_VID_CTRL, data);
}

/**
 * sw_get_board_storm - get broadcast storm threshold
 * @sw:		The switch instance.
 * @percent:	Buffer to store the broadcast storm threshold percentage.
 *
 * This routine retrieves the broadcast storm threshold of the switch.
 */
static void sw_get_broad_storm(struct ksz_sw *sw, u8 *percent)
{
	int num;
	u16 data;

	data = sw->reg->r16(sw, S_REPLACE_VID_CTRL);
#if (SW_SIZE == (2))
	data = ntohs(data);
#endif
	num = (data & BROADCAST_STORM_RATE);
	num = (num * 100 + BROADCAST_STORM_VALUE / 2) / BROADCAST_STORM_VALUE;
	*percent = (u8) num;
}

/**
 * sw_dis_broad_storm - disable broadcast storm protection
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine disables the broadcast storm limit function of the switch.
 */
static void sw_dis_broad_storm(struct ksz_sw *sw, uint port)
{
	port_cfg_broad_storm(sw, port, 0);
}

/**
 * sw_ena_broad_storm - enable broadcast storm protection
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine enables the broadcast storm limit function of the switch.
 */
static void sw_ena_broad_storm(struct ksz_sw *sw, uint port)
{
	sw_cfg_broad_storm(sw, sw->info->broad_per);
	port_cfg_broad_storm(sw, port, 1);
}

/**
 * sw_init_broad_storm - initialize broadcast storm
 * @sw:		The switch instance.
 *
 * This routine initializes the broadcast storm limit function of the switch.
 */
static void sw_init_broad_storm(struct ksz_sw *sw)
{
	u8 percent;

	sw_get_broad_storm(sw, &percent);
	sw->info->broad_per = percent;
}

/**
 * hw_cfg_broad_storm - configure broadcast storm
 * @sw:		The switch instance.
 * @percent:	Broadcast storm threshold in percent of transmit rate.
 *
 * This routine configures the broadcast storm threshold of the switch.
 * It is called by user functions.  The hardware should be acquired first.
 */
static void hw_cfg_broad_storm(struct ksz_sw *sw, u8 percent)
{
	if (percent > 100)
		percent = 100;

	sw_cfg_broad_storm(sw, percent);
	sw_init_broad_storm(sw);
}

/**
 * sw_setup_broad_storm - setup broadcast storm
 * @sw:		The switch instance.
 *
 * This routine setup the broadcast storm limit function of the switch.
 */
static void sw_setup_broad_storm(struct ksz_sw *sw)
{
	uint port;

	/* Enable switch broadcast storm protection at 10% percent rate. */
	hw_cfg_broad_storm(sw, BROADCAST_STORM_PROTECTION_RATE);
	for (port = 0; port < SWITCH_PORT_NUM; port++)
		sw_ena_broad_storm(sw, port);
	sw_cfg(sw, REG_SWITCH_CTRL_2, MULTICAST_STORM_DISABLE, 1);
}

/* -------------------------------------------------------------------------- */

/* Rate Control */

static inline int get_rate_ctrl_offset(int port, int prio)
{
	int offset;

#if (SW_SIZE == (2))
	offset = REG_PORT1_TXQ_RATE_CTRL1 + port * 4;
	offset += ((3 - prio) / 2) * 2;
	offset++;
	offset -= (prio & 1);
#else
	offset = REG_PORT1_TXQ3_RATE_CTRL + port * 4;
	offset += (3 - prio);
#endif
	return offset;
}

/**
 * hw_cfg_rate_ctrl - configure port rate control
 * @sw:		The switch instance.
 * @port:	The port index.
 * @prio:	The priority index to configure.
 * @ctrl:	The flag indicating whether the rate control bit is set or not.
 *
 * This routine configures the priority rate control of the port.
 */
static void hw_cfg_rate_ctrl(struct ksz_sw *sw, uint port, int prio, int ctrl)
{
	int offset;
	u8 data;
	u8 saved;

	offset = get_rate_ctrl_offset(port, prio);

	data = sw->reg->r8(sw, offset);
	saved = data;
	data &= ~RATE_CTRL_ENABLE;
	if (ctrl)
		data |= RATE_CTRL_ENABLE;
	if (data != saved)
		sw->reg->w8(sw, offset, data);
	sw->info->port_cfg[port].rate_ctrl[prio] = data;
}

#ifdef RATE_RATIO_MASK
/**
 * hw_cfg_rate_ratio - configure port rate ratio
 * @sw:		The switch instance.
 * @port:	The port index.
 * @prio:	The priority index to configure.
 * @ratio:	The rate ratio.
 *
 * This routine configures the priority rate ratio of the port.
 */
static void hw_cfg_rate_ratio(struct ksz_sw *sw, uint port, int prio, u8 ratio)
{
	int offset;
	u8 data;
	u8 saved;

	if (ratio >= RATE_CTRL_ENABLE)
		return;

	offset = get_rate_ctrl_offset(port, prio);

	data = sw->reg->r8(sw, offset);
	saved = data;
	data &= RATE_CTRL_ENABLE;
	data |= ratio;
	if (data != saved)
		sw->reg->w8(sw, offset, data);
	sw->info->port_cfg[port].rate_ctrl[prio] = data;
}
#endif

/**
 * hw_get_rate_ctrl - get port rate control
 * @sw:		The switch instance.
 * @port:	The port index.
 * @prio:	The priority index to retrieve.
 *
 * This routine retrieves the priority rate control of the port.
 */
static void hw_get_rate_ctrl(struct ksz_sw *sw, uint port, int prio)
{
	int offset;
	u8 data;

	offset = get_rate_ctrl_offset(port, prio);

	data = sw->reg->r8(sw, offset);
	sw->info->port_cfg[port].rate_ctrl[prio] = data;
}

/* -------------------------------------------------------------------------- */

/* Rate Limit */

/**
 * hw_cfg_rate_limit - configure port rate limit modes
 * @sw:		The switch instance.
 * @port:	The port index.
 * @mask:	The mask value.
 * @shift:	The shift position.
 * @mode:	The rate limit mode.
 *
 * This helper routine configures the rate limit modes of the port.
 */
static void hw_cfg_rate_limit(struct ksz_sw *sw, uint port, u8 mask, u8 shift,
	u8 mode)
{
	u8 data;
	u8 saved;

	port_r8(sw, port, P_RATE_LIMIT_CTRL, &data);
	saved = data;
	data &= ~(mask << shift);
	data |= mode << shift;
	if (data != saved)
		port_w8(sw, port, P_RATE_LIMIT_CTRL, data);
	sw->info->port_cfg[port].rate_limit = data;
}

/**
 * hw_cfg_cnt_ifg - configure port rate limit count IFG control
 * @sw:		The switch instance.
 * @port:	The port index.
 * @set:	The flag indicating whether the count control is set or not.
 *
 * This routine configures the rate limit count IFG control of the port.
 */
static void hw_cfg_cnt_ifg(struct ksz_sw *sw, uint port, bool set)
{
	hw_cfg_rate_limit(sw, port, 1, 1, set);
}

/**
 * hw_cfg_cnt_pre - configure port rate limit count preamble control
 * @sw:		The switch instance.
 * @port:	The port index.
 * @set:	The flag indicating whether the count control is set or not.
 *
 * This routine configures the rate limit count preamble control of the port.
 */
static void hw_cfg_cnt_pre(struct ksz_sw *sw, uint port, bool set)
{
	hw_cfg_rate_limit(sw, port, 1, 0, set);
}

/**
 * hw_cfg_rx_limit - configure port rate limit mode
 * @sw:		The switch instance.
 * @port:	The port index.
 * @mode:	The rate limit mode.
 *
 * This routine configures the rate limit mode of the port.
 */
static void hw_cfg_rx_limit(struct ksz_sw *sw, uint port, u8 mode)
{
	if (mode >= 4)
		return;

	hw_cfg_rate_limit(sw, port, 3, 2, mode);
}

/**
 * hw_get_rate_limit - get port rate limit control
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine retrieves the rate limit of the port.
 */
static void hw_get_rate_limit(struct ksz_sw *sw, uint port)
{
	u8 data;

	port_r8(sw, port, P_RATE_LIMIT_CTRL, &data);
	sw->info->port_cfg[port].rate_limit = data;
}

/* -------------------------------------------------------------------------- */

static uint get_rate_from_val(u8 val)
{
	uint i;

	if (0 == val)
		i = 0;
	else if (val <= 0x64)
		i = 1000 * val;
	else
		i = 64 * (val - 0x64);
	return i;
}

static int get_rate_to_val(uint rate)
{
	int i;

	if (rate >= 1000) {
		i = (rate + 500) / 1000;
		if (i > 0x64)
			i = 0x64;
	} else if (0 == rate)
		i = 0;
	else {
		i = (rate + 32) / 64;
		if (0 == i)
			i = 1;
		else if (i > 15)
			i = 15;
		i += 0x64;
	}
	return i;
}

/**
 * port_cfg_rate - configure port priority rate
 * @sw:		The switch instance.
 * @port:	The port index.
 * @prio:	The priority index to configure.
 * @offset:	The receive or transmit rate offset.
 * @shift:	The shift position to set the value.
 * @rate:	The rate limit in number of Kbps.
 *
 * This helper routine configures the priority rate of the port.
 */
static void port_cfg_rate(struct ksz_sw *sw, uint port, int prio, int offset,
	uint rate)
{
	u8 data;
	u8 factor;

	factor = (u8) get_rate_to_val(rate);

#ifndef PORT_OUT_RATE_ADDR
	offset += (prio / 2) * 2;
	offset += (prio & 1);

	/* First rate limit register may have special function bit. */
	if (0 == prio) {
		port_r8(sw, port, offset, &data);
		data &= 0x80;
		factor |= data;
	}
	port_w8(sw, port, offset, factor);
#else

	/* First rate limit register may have special function bit. */
	if (0 == prio) {
		if (REG_PORT_1_OUT_RATE_0 == offset)
			port_out_rate_r8(sw, port, REG_PORT_OUT_RATE_0 + prio,
				&data);
		else
			port_r8(sw, port, REG_PORT_IN_RATE_0 + prio, &data);
		data &= 0x80;
		factor |= data;
	}
	if (REG_PORT_1_OUT_RATE_0 == offset)
		port_out_rate_w8(sw, port, REG_PORT_OUT_RATE_0 + prio, factor);
	else
		port_w8(sw, port, REG_PORT_IN_RATE_0 + prio, factor);
#endif
}

/**
 * port_get_rate - get port priority rate
 * @sw:		The switch instance.
 * @port:	The port index.
 * @prio:	The priority index to configure.
 * @offset:	The receive or transmit rate offset.
 * @shift:	The shift position to get the value.
 * @rate:	Buffer to store the data rate in number of Kbps.
 *
 * This helper routine retrieves the priority rate of the port.
 */
static void port_get_rate(struct ksz_sw *sw, uint port, int prio, int offset,
	uint *rate)
{
	u8 data;

#ifndef PORT_OUT_RATE_ADDR
	offset += (prio / 2) * 2;
	offset += (prio & 1);
	port_r8(sw, port, offset, &data);
#else
	if (REG_PORT_1_OUT_RATE_0 == offset)
		port_out_rate_r8(sw, port, REG_PORT_OUT_RATE_0 + prio, &data);
	else
		port_r8(sw, port, REG_PORT_IN_RATE_0 + prio, &data);
#endif
	data &= 0x7F;
	*rate = get_rate_from_val(data);
}

/**
 * hw_cfg_prio_rate - configure port priority
 * @sw:		The switch instance.
 * @port:	The port index.
 * @prio:	The priority index to configure.
 * @rate:	The rate limit in number of Kbps.
 * @offset:	The receive or transmit rate offset.
 * @result:	Buffer to store the data rate in number of Kbps.
 *
 * This helper routine configures the priority rate of the port and retrieves
 * the actual rate number.
 */
static void hw_cfg_prio_rate(struct ksz_sw *sw, uint port, int prio, uint rate,
	int offset, uint *result)
{
	port_cfg_rate(sw, port, prio, offset, rate);
	port_get_rate(sw, port, prio, offset, result);
}

/**
 * hw_cfg_rx_prio_rate - configure port receive priority
 * @sw:		The switch instance.
 * @port:	The port index.
 * @prio:	The priority index to configure.
 * @rate:	The rate limit in number of Kbps.
 *
 * This routine configures the receive priority rate of the port.
 * It is called by user functions.  The hardware should be acquired first.
 */
static void hw_cfg_rx_prio_rate(struct ksz_sw *sw, uint port, int prio,
	uint rate)
{
	hw_cfg_prio_rate(sw, port, prio, rate,
#ifdef PORT_OUT_RATE_ADDR
		REG_PORT_1_IN_RATE_0,
#else
		REG_PORT_IN_RATE_0,
#endif
		&sw->info->port_cfg[port].rx_rate[prio]);
}

/**
 * hw_cfg_tx_prio_rate - configure port transmit priority
 * @sw:		The switch instance.
 * @port:	The port index.
 * @prio:	The priority index to configure.
 * @rate:	The rate limit in number of Kbps.
 *
 * This routine configures the transmit priority rate of the port.
 * It is called by user functions.  The hardware should be acquired first.
 */
static void hw_cfg_tx_prio_rate(struct ksz_sw *sw, uint port, int prio,
	uint rate)
{
	hw_cfg_prio_rate(sw, port, prio, rate,
#ifdef PORT_OUT_RATE_ADDR
		REG_PORT_1_OUT_RATE_0,
#else
		REG_PORT_OUT_RATE_0,
#endif
		&sw->info->port_cfg[port].tx_rate[prio]);
}

/**
 * sw_chk_rx_prio_rate - check switch rx priority rate
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This function checks whether the rx priority rate function of the switch is
 * enabled.
 *
 * Return 0 if not enabled.
 */
static int sw_chk_rx_prio_rate(struct ksz_sw *sw, uint port)
{
	u32 rate_addr;
	u32 in_rate;

	PORT_CTRL_ADDR(port, rate_addr);
	rate_addr += REG_PORT_IN_RATE_0;
	in_rate = sw->reg->r32(sw, rate_addr);
	return (in_rate) != 0;
}  /* sw_chk_rx_prio_rate */

/**
 * sw_chk_prio_rate - check switch tx priority rate
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This function checks whether the tx priority rate function of the switch is
 * enabled.
 *
 * Return 0 if not enabled.
 */
static int sw_chk_tx_prio_rate(struct ksz_sw *sw, uint port)
{
#ifdef PORT_OUT_RATE_ADDR
	u32 addr;
	u32 rate_addr;
	u32 out_rate;

	PORT_OUT_RATE_ADDR(port, addr);
	rate_addr = addr + REG_PORT_OUT_RATE_0;
	out_rate = sw->reg->r32(sw, rate_addr);
	return (out_rate) != 0;
#else
	return port_chk(sw, port, REG_PORT_OUT_RATE_0, 0x80);
#endif
}  /* sw_chk_tx_prio_rate */

/**
 * sw_dis_rx_prio_rate - disable switch rx priority rate
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine disables the rx priority rate function of the switch.
 */
static void sw_dis_rx_prio_rate(struct ksz_sw *sw, uint port)
{
	u32 rate_addr;

	PORT_CTRL_ADDR(port, rate_addr);
	rate_addr += REG_PORT_IN_RATE_0;
	sw->reg->w32(sw, rate_addr, 0);
}  /* sw_dis_rx_prio_rate */

/**
 * sw_dis_tx_prio_rate - disable tx switch priority rate
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine disables the tx priority rate function of the switch.
 */
static void sw_dis_tx_prio_rate(struct ksz_sw *sw, uint port)
{
#ifdef PORT_OUT_RATE_ADDR
	u32 addr;
	u32 rate_addr;

	PORT_OUT_RATE_ADDR(port, addr);
	rate_addr = addr + REG_PORT_OUT_RATE_0;
	sw->reg->w32(sw, rate_addr, 0);
#else
	u8 data;

	port_r8(sw, port, REG_PORT_OUT_RATE_0, &data);
	data &= ~0x80;
	port_w8(sw, port, REG_PORT_OUT_RATE_0, data);
#endif
}  /* sw_dis_tx_prio_rate */

/**
 * sw_ena_rx_prio_rate - enable switch rx priority rate
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine enables the rx priority rate function of the switch.
 */
static void sw_ena_rx_prio_rate(struct ksz_sw *sw, uint port)
{
	int prio;

	for (prio = 0; prio < PRIO_QUEUES; prio++)
		hw_cfg_rx_prio_rate(sw, port, prio,
			sw->info->port_cfg[port].rx_rate[prio]);
}  /* sw_ena_rx_prio_rate */

/**
 * sw_ena_tx_prio_rate - enable switch tx priority rate
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine enables the tx priority rate function of the switch.
 */
static void sw_ena_tx_prio_rate(struct ksz_sw *sw, uint port)
{
	int prio;

	for (prio = 0; prio < PRIO_QUEUES; prio++)
		hw_cfg_tx_prio_rate(sw, port, prio,
			sw->info->port_cfg[port].tx_rate[prio]);
#ifndef PORT_OUT_RATE_ADDR
	do {
		u8 data;

		port_r8(sw, port, REG_PORT_OUT_RATE_0, &data);
		data |= 0x80;
		port_w8(sw, port, REG_PORT_OUT_RATE_0, data);
	} while (0);
#endif
}  /* sw_ena_tx_prio_rate */

/**
 * sw_init_prio_rate - initialize switch prioirty rate
 * @sw:		The switch instance.
 *
 * This routine initializes the priority rate function of the switch.
 */
static void sw_init_prio_rate(struct ksz_sw *sw)
{
	int offset;
	uint port;
	int prio;

	for (port = 0; port < TOTAL_PORT_NUM; port++) {
		hw_get_rate_limit(sw, port);
		for (prio = 0; prio < PRIO_QUEUES; prio++) {
			hw_get_rate_ctrl(sw, port, prio);
#ifdef PORT_OUT_RATE_ADDR
			offset = REG_PORT_1_IN_RATE_0;
#else
			offset = REG_PORT_IN_RATE_0;
#endif
			port_get_rate(sw, port, prio, offset,
				&sw->info->port_cfg[port].rx_rate[prio]);
#ifdef PORT_OUT_RATE_ADDR
			offset = REG_PORT_1_OUT_RATE_0;
#else
			offset = REG_PORT_OUT_RATE_0;
#endif
			port_get_rate(sw, port, prio, offset,
				&sw->info->port_cfg[port].tx_rate[prio]);
		}
	}
}  /* sw_init_prio_rate */

/* -------------------------------------------------------------------------- */

/* Communication */

static inline void port_cfg_back_pressure(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_STP_CTRL, PORT_BACK_PRESSURE, set);
}

static inline void port_cfg_force_flow_ctrl(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_STP_CTRL, PORT_FORCE_FLOW_CTRL, set);
}

static inline int port_chk_back_pressure(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_STP_CTRL, PORT_BACK_PRESSURE);
}

static inline int port_chk_force_flow_ctrl(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_STP_CTRL, PORT_FORCE_FLOW_CTRL);
}

/* -------------------------------------------------------------------------- */

/* Spanning Tree */

static inline void port_cfg_dis_learn(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_STP_CTRL, PORT_LEARN_DISABLE, set);
}

static inline void port_cfg_rx(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_STP_CTRL, PORT_RX_ENABLE, set);
	if (set)
		sw->rx_ports |= (1 << p);
	else
		sw->rx_ports &= ~(1 << p);
}

static inline void port_cfg_tx(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_STP_CTRL, PORT_TX_ENABLE, set);
	if (set)
		sw->tx_ports |= (1 << p);
	else
		sw->tx_ports &= ~(1 << p);
}

static inline int port_chk_dis_learn(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_STP_CTRL, PORT_LEARN_DISABLE);
}

static inline int port_chk_rx(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_STP_CTRL, PORT_RX_ENABLE);
}

static inline int port_chk_tx(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_STP_CTRL, PORT_TX_ENABLE);
}

static inline void sw_cfg_fast_aging(struct ksz_sw *sw, bool set)
{
	sw_cfg(sw, REG_SWITCH_CTRL_1, SWITCH_FAST_AGING, set);
}

static void sw_flush_dyn_mac_table(struct ksz_sw *sw, uint port)
{
	uint cnt;
	uint first;
	uint index;
	int learn_disable[TOTAL_PORT_NUM];

	if ((uint) port < TOTAL_PORT_NUM) {
		first = port;
		cnt = first + 1;
	} else {
		first = 0;
		cnt = TOTAL_PORT_NUM;
	}
	for (index = first; index < cnt; index++) {
		port = get_phy_port(sw, index);
		learn_disable[port] = port_chk_dis_learn(sw, port);
		if (!learn_disable[port])
			port_cfg_dis_learn(sw, port, 1);
	}
	sw_cfg(sw, S_FLUSH_TABLE_CTRL, SWITCH_FLUSH_DYN_MAC_TABLE, 1);
	for (index = first; index < cnt; index++) {
		port = get_phy_port(sw, index);
		if (!learn_disable[port])
			port_cfg_dis_learn(sw, port, 0);
	}
}

/* -------------------------------------------------------------------------- */

/* VLAN */

static inline void port_cfg_ins_tag(struct ksz_sw *sw, uint p, bool insert)
{
	port_cfg(sw, p,
		P_TAG_CTRL, PORT_INSERT_TAG, insert);
}

static inline void port_cfg_rmv_tag(struct ksz_sw *sw, uint p, bool remove)
{
	port_cfg(sw, p,
		P_TAG_CTRL, PORT_REMOVE_TAG, remove);
}

static inline int port_chk_ins_tag(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_TAG_CTRL, PORT_INSERT_TAG);
}

static inline int port_chk_rmv_tag(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_TAG_CTRL, PORT_REMOVE_TAG);
}

#ifdef PORT_DOUBLE_TAG
static inline void port_cfg_double_tag(struct ksz_sw *sw, uint p, int remove)
{
	port_cfg(sw, p,
		P_MIRROR_CTRL, PORT_DOUBLE_TAG, remove);
}

static inline int port_chk_double_tag(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_MIRROR_CTRL, PORT_DOUBLE_TAG);
}
#endif

static inline void port_cfg_dis_non_vid(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_STP_CTRL, PORT_DISCARD_NON_VID, set);
}

static inline void port_cfg_drop_tag(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_SA_MAC_CTRL, PORT_DROP_TAG, set);
}

static inline void port_cfg_in_filter(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_STP_CTRL, PORT_INGRESS_FILTER, set);
}

static inline int port_chk_dis_non_vid(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_STP_CTRL, PORT_DISCARD_NON_VID);
}

static inline int port_chk_drop_tag(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_SA_MAC_CTRL, PORT_DROP_TAG);
}

static inline int port_chk_in_filter(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_STP_CTRL, PORT_INGRESS_FILTER);
}

/* -------------------------------------------------------------------------- */

/* Mirroring */

static inline void port_cfg_mirror_sniffer(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_MIRROR_CTRL, PORT_MIRROR_SNIFFER, set);
}

static inline void port_cfg_mirror_rx(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_MIRROR_CTRL, PORT_MIRROR_RX, set);
}

static inline void port_cfg_mirror_tx(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_MIRROR_CTRL, PORT_MIRROR_TX, set);
}

static inline void sw_cfg_mirror_rx_tx(struct ksz_sw *sw, bool set)
{
	sw_cfg(sw, S_MIRROR_CTRL, SWITCH_MIRROR_RX_TX, set);
}

static inline int port_chk_mirror_sniffer(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_MIRROR_CTRL, PORT_MIRROR_SNIFFER);
}

static inline int port_chk_mirror_rx(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_MIRROR_CTRL, PORT_MIRROR_RX);
}

static inline int port_chk_mirror_tx(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_MIRROR_CTRL, PORT_MIRROR_TX);
}

static inline int sw_chk_mirror_rx_tx(struct ksz_sw *sw)
{
	return sw_chk(sw, S_MIRROR_CTRL, SWITCH_MIRROR_RX_TX);
}

static void sw_setup_mirror(struct ksz_sw *sw)
{
	uint port;

	for (port = 0; port < TOTAL_PORT_NUM; port++) {
		port_cfg_mirror_sniffer(sw, port, 0);
		port_cfg_mirror_rx(sw, port, 0);
		port_cfg_mirror_tx(sw, port, 0);
	}
	sw_cfg_mirror_rx_tx(sw, 0);
}

/* -------------------------------------------------------------------------- */

static void sw_cfg_unk_dest(struct ksz_sw *sw, bool set)
{
	u8 data;

	data = sw->reg->r8(sw, S_UNKNOWN_DA_CTRL);
	if (set)
		data |= SWITCH_UNKNOWN_DA_ENABLE;
	else
		data &= ~SWITCH_UNKNOWN_DA_ENABLE;
	sw->reg->w8(sw, S_UNKNOWN_DA_CTRL, data);
}

static int sw_chk_unk_dest(struct ksz_sw *sw)
{
	u8 data;

	data = sw->reg->r8(sw, S_UNKNOWN_DA_CTRL);
	return (data & SWITCH_UNKNOWN_DA_ENABLE) == SWITCH_UNKNOWN_DA_ENABLE;
}

static void sw_cfg_unk_def_port(struct ksz_sw *sw, uint port, bool set)
{
	u8 data;
	u8 bits = 1 << port;

	data = sw->reg->r8(sw, S_UNKNOWN_DA_CTRL);
	if (set)
		data |= bits;
	else
		data &= ~bits;
	sw->reg->w8(sw, S_UNKNOWN_DA_CTRL, data);
}

static int sw_chk_unk_def_port(struct ksz_sw *sw, uint port)
{
	u8 data;
	u8 bit = 1 << port;

	data = sw->reg->r8(sw, S_UNKNOWN_DA_CTRL);
	return (data & bit) == bit;
}

static inline void sw_cfg_for_inv_vid(struct ksz_sw *sw, uint port, bool set)
{
	port_cfg_shift(sw, port, S_FORWARD_INVALID_VID_CTRL,
		FORWARD_INVALID_PORT_SHIFT, set);
}

static inline int sw_chk_for_inv_vid(struct ksz_sw *sw, uint port)
{
	return port_chk_shift(sw, port, S_FORWARD_INVALID_VID_CTRL,
		FORWARD_INVALID_PORT_SHIFT);
}

/* -------------------------------------------------------------------------- */

/* Priority */

static inline void port_cfg_diffserv(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_PRIO_CTRL, PORT_DIFFSERV_ENABLE, set);
}

static inline void port_cfg_802_1p(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_PRIO_CTRL, PORT_802_1P_ENABLE, set);
}

static inline void port_cfg_replace_prio(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_MIRROR_CTRL, PORT_802_1P_REMAPPING, set);
}

static inline void port_cfg_2_queue(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_2_QUEUE_CTRL, PORT_2_QUEUES_ENABLE, set);
}

static inline void port_cfg_4_queue(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_4_QUEUE_CTRL, PORT_4_QUEUES_ENABLE, set);
}

static inline int port_chk_diffserv(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_PRIO_CTRL, PORT_DIFFSERV_ENABLE);
}

static inline int port_chk_802_1p(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_PRIO_CTRL, PORT_802_1P_ENABLE);
}

static inline int port_chk_replace_prio(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_MIRROR_CTRL, PORT_802_1P_REMAPPING);
}

static inline int port_chk_2_queue(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_2_QUEUE_CTRL, PORT_2_QUEUES_ENABLE);
}

static inline int port_chk_4_queue(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_4_QUEUE_CTRL, PORT_4_QUEUES_ENABLE);
}

/* -------------------------------------------------------------------------- */

static inline void port_cfg_src_filter_0(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_SA_MAC_CTRL, PORT_SA_MAC1, set);
}

static inline void port_cfg_src_filter_1(struct ksz_sw *sw, uint p, bool set)
{
	port_cfg(sw, p,
		P_SA_MAC_CTRL, PORT_SA_MAC2, set);
}

static inline int port_chk_src_filter_0(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_SA_MAC_CTRL, PORT_SA_MAC1);
}

static inline int port_chk_src_filter_1(struct ksz_sw *sw, uint p)
{
	return port_chk(sw, p,
		P_SA_MAC_CTRL, PORT_SA_MAC2);
}

/**
 * port_get_addr - get the port MAC address.
 * @sw:		The switch instance.
 * @port:	The port index.
 * @mac_addr:	Buffer to store the MAC address.
 *
 * This function retrieves the MAC address of the port.
 */
static inline void port_get_addr(struct ksz_sw *sw, uint port, u8 *mac_addr)
{
	int i;
	SW_D reg;
#if (SW_SIZE == (2))
	u16 *addr = (u16 *) mac_addr;
#endif

	if (0 == port)
		reg = REG_PORT_0_MAC_ADDR_0;
	else
		reg = REG_PORT_1_MAC_ADDR_0;
#if (SW_SIZE == (2))
	for (i = 0; i < 6; i += 2) {
		*addr = sw->reg->r16(sw, reg - i);
		*addr = ntohs(*addr);
		addr++;
	}
#else
	for (i = 0; i < 6; i++)
		mac_addr[i] = sw->reg->r8(sw, reg + i);
#endif
	memcpy(sw->port_info[port].mac_addr, mac_addr, 6);
}

/**
 * port_set_addr - configure port MAC address
 * @sw:		The switch instance.
 * @port:	The port index.
 * @mac_addr:	The MAC address.
 *
 * This function configures the MAC address of the port.
 */
static void port_set_addr(struct ksz_sw *sw, uint port, u8 *mac_addr)
{
	int i;
	SW_D reg;
#if (SW_SIZE == (2))
	u16 *addr = (u16 *) mac_addr;
#endif

	if (0 == port)
		reg = REG_PORT_0_MAC_ADDR_0;
	else
		reg = REG_PORT_1_MAC_ADDR_0;
#if (SW_SIZE == (2))
	for (i = 0; i < 6; i += 2) {
		sw->reg->w16(sw, reg - i, htons(*addr));
		addr++;
	}
#else
	for (i = 0; i < 6; i++)
		sw->reg->w8(sw, reg + i, mac_addr[i]);
#endif
	memcpy(sw->port_info[port].mac_addr, mac_addr, 6);
}

static inline void sw_setup_src_filter(struct ksz_sw *sw, u8 *mac_addr)
{
	uint port;

	for (port = 0; port < SWITCH_PORT_NUM; port++) {
		port_set_addr(sw, port, mac_addr);
		port_cfg_src_filter_0(sw, port, 1);
		port_cfg_src_filter_1(sw, port, 1);
	}
}

static void sw_cfg_src_filter(struct ksz_sw *sw, bool set)
{
	uint port;

	for (port = 0; port < SWITCH_PORT_NUM; port++) {
		port_cfg_src_filter_0(sw, port, set);
		port_cfg_src_filter_1(sw, port, set);
	}
}  /* sw_cfg_src_filter */

/* -------------------------------------------------------------------------- */

/**
 * sw_set_tos_prio - program switch TOS priority
 * @sw:		The switch instance.
 * @tos:	ToS value from 6-bit (bit7 ~ bit2) of ToS field, ranging from 0
 *		to 63.
 * @prio:	Priority to be assigned.
 *
 * This routine programs the TOS priority into the switch registers.
 */
static void sw_set_tos_prio(struct ksz_sw *sw, u8 tos, SW_D prio)
{
	SW_W(sw, S_TOS_PRIO_CTRL + tos * SW_SIZE, prio);
}

/**
 * sw_dis_diffserv - disable switch DiffServ priority
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine disables the DiffServ priority function of the switch.
 */
static void sw_dis_diffserv(struct ksz_sw *sw, uint port)
{
	port_cfg_diffserv(sw, port, 0);
}

/**
 * sw_ena_diffserv - enable switch DiffServ priority
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine enables the DiffServ priority function of the switch.
 */
static void sw_ena_diffserv(struct ksz_sw *sw, uint port)
{
	port_cfg_diffserv(sw, port, 1);
}

/**
 * hw_cfg_tos_prio - configure TOS priority
 * @sw:		The switch instance.
 * @tos:	ToS value from 6-bit (bit7 ~ bit2) of ToS field, ranging from 0
 *		to 63.
 * @prio:	Priority to be assigned.
 *
 * This routine configures the TOS priority in the hardware.
 * DiffServ Value 0 ~ 63 is mapped to Priority Queue Number 0 ~ 3.
 * It is called by user functions.  The hardware should be acquired first.
 */
static void hw_cfg_tos_prio(struct ksz_sw *sw, u8 tos, SW_D prio)
{
	int shift;
	SW_D data = prio;
	SW_D mask = KS_PRIO_M;

	if (tos >= DIFFSERV_ENTRIES)
		return;

#if (SW_SIZE == (2))
	if (prio >= 0x100)
		mask = 0xffff;
	else
#endif
	if (prio >= 0x10)
		mask = 0xff;
	else if (prio > KS_PRIO_M)
		mask = 0xf;
	shift = (tos & (KS_PRIO_IN_REG - 1)) * KS_PRIO_S;
	prio = prio << shift;
	if (prio >> shift != data)
		return;
	mask <<= shift;
	tos /= KS_PRIO_IN_REG;

	sw->info->diffserv[tos] &= ~mask;
	sw->info->diffserv[tos] |= prio;

	sw_set_tos_prio(sw, tos, sw->info->diffserv[tos]);
}

/* -------------------------------------------------------------------------- */

/**
 * sw_set_802_1p_prio - program switch 802.1p priority
 * @sw:		The switch instance.
 * @prio:	Priority to be assigned.
 *
 * This routine programs the 802.1p priority into the switch register.
 */
static void sw_set_802_1p_prio(struct ksz_sw *sw, u8 tag, SW_D prio)
{
	SW_W(sw, S_802_1P_PRIO_CTRL + tag / SW_SIZE, prio);
}

/**
 * sw_dis_802_1p - disable switch 802.1p priority
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine disables the 802.1p priority function of the switch.
 */
static void sw_dis_802_1p(struct ksz_sw *sw, uint port)
{
	port_cfg_802_1p(sw, port, 0);
}

/**
 * sw_ena_802_1p - enable switch 802.1p priority
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine enables the 802.1p priority function of the switch.
 */
static void sw_ena_802_1p(struct ksz_sw *sw, uint port)
{
	port_cfg_802_1p(sw, port, 1);
}

/**
 * hw_cfg_802_1p_prio - configure 802.1p priority
 * @sw:		The switch instance.
 * @tag:	The 802.1p tag priority value, ranging from 0 to 7.
 * @prio:	Priority to be assigned.
 *
 * This routine configures the 802.1p priority in the hardware.
 * 802.1p Tag priority value 0 ~ 7 is mapped to Priority Queue Number 0 ~ 3.
 * It is called by user functions.  The hardware should be acquired first.
 */
static void hw_cfg_802_1p_prio(struct ksz_sw *sw, u8 tag, SW_D prio)
{
	int shift;
	SW_D data = prio;
	SW_D mask = KS_PRIO_M;

	if (tag >= PRIO_802_1P_ENTRIES)
		return;

#if (SW_SIZE == (2))
	if (prio >= 0x100)
		mask = 0xffff;
	else
#endif
	if (prio >= 0x10)
		mask = 0xff;
	else if (prio > KS_PRIO_M)
		mask = 0xf;
	shift = (tag & (KS_PRIO_IN_REG - 1)) * KS_PRIO_S;
	prio = prio << shift;
	if (prio >> shift != data)
		return;
	mask <<= shift;
	tag /= KS_PRIO_IN_REG;

	sw->info->p_802_1p[tag] &= ~mask;
	sw->info->p_802_1p[tag] |= prio;

	sw_set_802_1p_prio(sw, tag, sw->info->p_802_1p[tag]);
}

/**
 * sw_cfg_replace_null_vid -
 * @sw:		The switch instance.
 * @set:	The flag to disable or enable.
 *
 */
static void sw_cfg_replace_null_vid(struct ksz_sw *sw, bool set)
{
	sw_cfg(sw, S_REPLACE_VID_CTRL, SWITCH_REPLACE_VID, set);
}

/**
 * sw_cfg_replace_prio - enable switch 802.10 priority re-mapping
 * @sw:		The switch instance.
 * @port:	The port index.
 * @set:	The flag to disable or enable.
 *
 * This routine enables the 802.1p priority re-mapping function of the switch.
 * That allows 802.1p priority field to be replaced with the port's default
 * tag's priority value if the ingress packet's 802.1p priority has a higher
 * priority than port's default tag's priority.
 */
static void sw_cfg_replace_prio(struct ksz_sw *sw, uint port, bool set)
{
	port_cfg_replace_prio(sw, port, set);
}

/* -------------------------------------------------------------------------- */

/**
 * sw_cfg_port_based - configure switch port based priority
 * @sw:		The switch instance.
 * @port:	The port index.
 * @prio:	The priority to set.
 *
 * This routine configures the port based priority of the switch.
 */
static void sw_cfg_port_based(struct ksz_sw *sw, uint port, u8 prio)
{
	SW_D data;

	if (prio > PORT_BASED_PRIORITY_BASE)
		prio = PORT_BASED_PRIORITY_BASE;

	port_r(sw, port, P_PRIO_CTRL, &data);
	data &= ~PORT_BASED_PRIORITY_MASK;
	data |= prio << PORT_BASED_PRIORITY_SHIFT;
	port_w(sw, port, P_PRIO_CTRL, data);

	sw->info->port_cfg[port].port_prio = prio;
}

/* -------------------------------------------------------------------------- */

/**
 * sw_set_multi_queue - enable transmit multiple queues
 * @sw:		The switch instance.
 * @port:	The port index.
 * @queue:	The number of queues.
 *
 * This routine enables the transmit multiple queues selection of the switch
 * port.  The port transmit queue is split into two or four priority queues.
 */
static void sw_set_multi_queue(struct ksz_sw *sw, uint port, int queue)
{
	switch (queue) {
	case 4:
	case 3:
		port_cfg_2_queue(sw, port, 0);
		port_cfg_4_queue(sw, port, 1);
		break;
	case 2:
		port_cfg_2_queue(sw, port, 1);
		port_cfg_4_queue(sw, port, 0);
		break;
	default:
		port_cfg_2_queue(sw, port, 0);
		port_cfg_4_queue(sw, port, 0);
	}
}  /* sw_set_multi_queue */

/**
 * sw_init_prio - initialize switch priority
 * @sw:		The switch instance.
 *
 * This routine initializes the switch QoS priority functions.
 */
static void sw_init_prio(struct ksz_sw *sw)
{
	uint port;
	int tos;
	SW_D data;

	for (tos = 0; tos < PRIO_802_1P_ENTRIES / KS_PRIO_IN_REG; tos++)
		sw->info->p_802_1p[tos] =
			SW_R(sw, S_802_1P_PRIO_CTRL + tos * SW_SIZE);

	for (tos = 0; tos < DIFFSERV_ENTRIES / KS_PRIO_IN_REG; tos++)
		sw->info->diffserv[tos] =
			SW_R(sw, S_TOS_PRIO_CTRL + tos * SW_SIZE);

	for (port = 0; port < TOTAL_PORT_NUM; port++) {
		port_r(sw, port, P_PRIO_CTRL, &data);
		data &= PORT_BASED_PRIORITY_MASK;
		data >>= PORT_BASED_PRIORITY_SHIFT;
		sw->info->port_cfg[port].port_prio = data;
	}

#ifdef USE_DIFF_PORT_PRIORITY
/*
 * THa  2012/02/01
 * Port 3 sometimes cannot send frames out through the port where 100%
 * wire-rate 64 bytes traffic also goes through.  Solution is to assign
 * different port priorities.  It does not matter port 3 has higher priority,
 * just different from the port where heavy traffic comes in.
 */
	for (port = 0; port < TOTAL_PORT_NUM; port++)
		sw->info->port_cfg[port].port_prio = port;
#endif
#ifdef USE_DIFF_PORT_PRIORITY
	for (port = 0; port < SWITCH_PORT_NUM; port++) {
		int prio;

		for (prio = 0; prio < PRIO_QUEUES; prio++) {
			sw->info->port_cfg[port].rx_rate[prio] = 95000;
			sw->info->port_cfg[port].tx_rate[prio] = 0;
		}
		sw_ena_rx_prio_rate(sw, port);
	}
#endif
}

/**
 * sw_setup_prio - setup switch priority
 * @sw:		The switch instance.
 *
 * This routine setup the switch QoS priority functions.
 */
static void sw_setup_prio(struct ksz_sw *sw)
{
	uint port;

	/* All QoS functions disabled. */
	for (port = 0; port < TOTAL_PORT_NUM; port++) {
		sw_set_multi_queue(sw, port, 4);
		sw_dis_diffserv(sw, port);
		sw_cfg_replace_prio(sw, port, 0);
		sw_cfg_port_based(sw, port, sw->info->port_cfg[port].port_prio);

		sw_ena_802_1p(sw, port);
	}
	sw_cfg_replace_null_vid(sw, 0);
}

/* -------------------------------------------------------------------------- */

/**
 * port_cfg_def_vid - configure port default VID
 * @sw:		The switch instance.
 * @port:	The port index.
 * @vid:	The VID value.
 *
 * This routine configures the default VID of the port.
 */
static void port_cfg_def_vid(struct ksz_sw *sw, uint port, u16 vid)
{
	port_w16(sw, port, REG_PORT_CTRL_VID, vid);
}

/**
 * port_get_def_vid - get port default VID.
 * @sw:		The switch instance.
 * @port:	The port index.
 * @vid:	Buffer to store the VID.
 *
 * This routine retrieves the default VID of the port.
 */
static void port_get_def_vid(struct ksz_sw *sw, uint port, u16 *vid)
{
	port_r16(sw, port, REG_PORT_CTRL_VID, vid);
}

/**
 * sw_cfg_def_vid - configure switch port default VID
 * @sw:		The switch instance.
 * @port:	The port index.
 * @vid:	The VID value.
 *
 * This routine configures the default VID of the port.
 */
static void sw_cfg_def_vid(struct ksz_sw *sw, uint port, u16 vid)
{
	sw->info->port_cfg[port].vid = vid;
	port_cfg_def_vid(sw, port, vid);
}

/**
 * sw_cfg_port_base_vlan - configure port-based VLAN membership
 * @sw:		The switch instance.
 * @port:	The port index.
 * @member:	The port-based VLAN membership.
 *
 * This routine configures the port-based VLAN membership of the port.
 */
static void sw_cfg_port_base_vlan(struct ksz_sw *sw, uint port, u8 member)
{
	SW_D data;

	port_r(sw, port, P_MIRROR_CTRL, &data);
	data &= ~PORT_VLAN_MEMBERSHIP;
	data |= (member & sw->PORT_MASK);
	port_w(sw, port, P_MIRROR_CTRL, data);

	sw->info->port_cfg[port].member = member;
}

/**
 * sw_vlan_cfg_dis_non_vid - configure discard non VID
 * @sw:		The switch instance.
 * @port:	The port index.
 * @set:	The flag to disable or enable.
 *
 * This routine configures Discard Non VID packets of the switch port.
 * If enabled, the device will discard packets whose VLAN id does not match
 * ingress port-based default VLAN id.
 */
static void sw_vlan_cfg_dis_non_vid(struct ksz_sw *sw, uint port, bool set)
{
	port_cfg_dis_non_vid(sw, port, set);
}

/**
 * sw_vlan_cfg_drop_tag - configure 802.1q tagged packet drop
 * @sw:		The switch instance.
 * @port:	The port index.
 * @set:	The flag to disable or enable.
 *
 */
static void sw_vlan_cfg_drop_tag(struct ksz_sw *sw, uint port, bool set)
{
	port_cfg_drop_tag(sw, port, set);
}

/**
 * sw_vlan_cfg_in_filter - configure ingress VLAN filtering
 * @sw:		The switch instance.
 * @port:	The port index.
 * @set:	The flag to disable or enable.
 *
 * This routine configures Ingress VLAN filtering of the switch port.
 * If enabled, the device will discard packets whose VLAN id membership	in the
 * VLAN table bits [18:16] does not include the ingress port that received this
 * packet.
 */
static void sw_vlan_cfg_in_filter(struct ksz_sw *sw, uint port, bool set)
{
	port_cfg_in_filter(sw, port, set);
}

/**
 * sw_vlan_cfg_ins_tag - configure 802.1q tag insertion
 * @sw:		The switch instance.
 * @port:	The port index.
 * @set:	The flag to disable or enable.
 *
 * This routine configures 802.1q Tag insertion to the switch port.
 * If enabled, the device will insert 802.1q tag to the transmit packet on this
 * port if received packet is an untagged packet.  The device will not insert
 * 802.1q tag if received packet is tagged packet.
 */
static void sw_vlan_cfg_ins_tag(struct ksz_sw *sw, uint port, bool set)
{
	port_cfg_ins_tag(sw, port, set);
}

/**
 * sw_vlan_cfg_rmv_tag - configure 802.1q tag removal
 * @sw:		The switch instance.
 * @port:	The port index.
 * @set:	The flag to disable or enable.
 *
 * This routine configures 802.1q Tag removal to the switch port.
 * If enabled, the device will removed 802.1q tag to the transmit packet on
 * this port if received packet is a tagged packet.  The device will not remove
 * 802.1q tag if received packet is untagged packet.
 */
static void sw_vlan_cfg_rmv_tag(struct ksz_sw *sw, uint port, bool set)
{
	port_cfg_rmv_tag(sw, port, set);
}

#ifdef PORT_DOUBLE_TAG
/**
 * sw_vlan_cfg_double_tag - configure 802.1q double tag
 * @sw:		The switch instance.
 * @port:	The port index.
 * @set:	The flag to disable or enable.
 *
 */
static void sw_vlan_cfg_double_tag(struct ksz_sw *sw, uint port, bool set)
{
	port_cfg_double_tag(sw, port, set);
}
#endif

/**
 * sw_dis_vlan - disable switch VLAN
 * @sw:		The switch instance.
 *
 * This routine disables the VLAN function of the switch.
 */
static void sw_dis_vlan(struct ksz_sw *sw)
{
	sw_cfg(sw, S_MIRROR_CTRL, SWITCH_VLAN_ENABLE, 0);
}

/**
 * sw_ena_vlan - enable switch VLAN
 * @sw:		The switch instance.
 *
 * This routine enables the VLAN function of the switch.
 */
static void sw_ena_vlan(struct ksz_sw *sw)
{
	int entry;
	struct ksz_sw_info *info = sw->info;

	/* Create 16 VLAN entries in the VLAN table. */
	for (entry = 0; entry < VLAN_TABLE_ENTRIES; entry++) {
		sw_w_vlan_table(sw, entry,
			info->vlan_table[entry].vid,
			info->vlan_table[entry].fid,
			info->vlan_table[entry].member,
			info->vlan_table[entry].valid);
	}

	/* Enable 802.1q VLAN mode. */
	sw_cfg(sw, REG_SWITCH_CTRL_2, UNICAST_VLAN_BOUNDARY, 1);
	sw_cfg(sw, S_MIRROR_CTRL, SWITCH_VLAN_ENABLE, 1);
}

/**
 * sw_init_vlan - initialize switch VLAN
 * @sw:		The switch instance.
 *
 * This routine initializes the VLAN function of the switch.
 */
static void sw_init_vlan(struct ksz_sw *sw)
{
	uint port;
	int entry;
	SW_D data;
	struct ksz_sw_info *info = sw->info;

	/* Read 16 VLAN entries from device's VLAN table. */
	for (entry = 0; entry < VLAN_TABLE_ENTRIES; entry++) {
		if (!sw_r_vlan_table(sw, entry,
				&info->vlan_table[entry].vid,
				&info->vlan_table[entry].fid,
				&info->vlan_table[entry].member))
			info->vlan_table[entry].valid = 1;
		else
			info->vlan_table[entry].valid = 0;
	}

	for (port = 0; port < TOTAL_PORT_NUM; port++) {
		port_get_def_vid(sw, port, &info->port_cfg[port].vid);
		port_r(sw, port, P_MIRROR_CTRL, &data);
		data &= PORT_VLAN_MEMBERSHIP;
		info->port_cfg[port].member = data;
		info->port_cfg[port].vid_member = data;
	}
	sw_cfg(sw, S_INS_SRC_PVID_CTRL,
		(SWITCH_INS_TAG_1_PORT_2 | SWITCH_INS_TAG_1_PORT_3 |
		SWITCH_INS_TAG_2_PORT_1 | SWITCH_INS_TAG_2_PORT_3 |
		SWITCH_INS_TAG_3_PORT_1 | SWITCH_INS_TAG_3_PORT_2),
		true);
	info->vlan_table_used = 1;
	info->fid_used = 1;
}

/**
 * sw_get_addr - get the switch MAC address.
 * @sw:		The switch instance.
 * @mac_addr:	Buffer to store the MAC address.
 *
 * This function retrieves the MAC address of the switch.
 */
static inline void sw_get_addr(struct ksz_sw *sw, u8 *mac_addr)
{
	int i;
#if (SW_SIZE == (2))
	u16 *addr = (u16 *) mac_addr;

	for (i = 0; i < 6; i += 2) {
		*addr = sw->reg->r16(sw, REG_SWITCH_MAC_ADDR_0 + i);
		*addr = ntohs(*addr);
		addr++;
	}
#else

	for (i = 0; i < 6; i++)
		mac_addr[i] = sw->reg->r8(sw, REG_SWITCH_MAC_ADDR_0 + i);
#endif
	memcpy(sw->info->mac_addr, mac_addr, 6);
}

/**
 * sw_set_addr - configure switch MAC address
 * @sw:		The switch instance.
 * @mac_addr:	The MAC address.
 *
 * This function configures the MAC address of the switch.
 */
static void sw_set_addr(struct ksz_sw *sw, u8 *mac_addr)
{
	int i;
#if (SW_SIZE == (2))
	u16 *addr = (u16 *) mac_addr;

	for (i = 0; i < 6; i += 2) {
		sw->reg->w16(sw, REG_SWITCH_MAC_ADDR_0 + i, htons(*addr));
		addr++;
	}
#else

	for (i = 0; i < 6; i++)
		sw->reg->w8(sw, REG_SWITCH_MAC_ADDR_0 + i, mac_addr[i]);
#endif
	memcpy(sw->info->mac_addr, mac_addr, 6);
}

#ifdef SWITCH_PORT_PHY_ADDR_MASK
/**
 * sw_init_phy_addr - initialize switch PHY address
 * @sw:		The switch instance.
 *
 * This function initializes the PHY address of the switch.
 */
static void sw_init_phy_addr(struct ksz_sw *sw)
{
	u8 addr;

	addr = sw->reg->r8(sw, REG_SWITCH_CTRL_13);
	addr >>= SWITCH_PORT_PHY_ADDR_SHIFT;
	addr &= SWITCH_PORT_PHY_ADDR_MASK;
	sw->info->phy_addr = addr;
}

/**
 * sw_set_phy_addr - configure switch PHY address
 * @sw:		The switch instance.
 * @addr:	The PHY address.
 *
 * This function configures the PHY address of the switch.
 */
static void sw_set_phy_addr(struct ksz_sw *sw, u8 addr)
{
	sw->info->phy_addr = addr;
	addr &= SWITCH_PORT_PHY_ADDR_MASK;
	addr <<= SWITCH_PORT_PHY_ADDR_SHIFT;
	sw->reg->w8(sw, REG_SWITCH_CTRL_13, addr);
}
#endif

#define STP_ENTRY			0
#define BROADCAST_ENTRY			1
#define BRIDGE_ADDR_ENTRY		2
#define IPV6_ADDR_ENTRY			3

/**
 * sw_set_global_ctrl - set switch global control
 * @sw:		The switch instance.
 *
 * This routine sets the global control of the switch function.
 */
static void sw_set_global_ctrl(struct ksz_sw *sw)
{
	SW_D data;

	/* Enable switch MII flow control. */
	data = SW_R(sw, S_REPLACE_VID_CTRL);
	data |= SWITCH_FLOW_CTRL;
	SW_W(sw, S_REPLACE_VID_CTRL, data);

	data = SW_R(sw, S_LINK_AGING_CTRL);
	data |= SWITCH_LINK_AUTO_AGING;
	SW_W(sw, S_LINK_AGING_CTRL, data);

	data = SW_R(sw, REG_SWITCH_CTRL_1);

	/* Enable aggressive back off algorithm in half duplex mode. */
	data |= SWITCH_AGGR_BACKOFF;

	/* Enable automatic fast aging when link changed detected. */
	data |= SWITCH_AGING_ENABLE;

	if (sw->overrides & FAST_AGING)
		data |= SWITCH_FAST_AGING;
	else
		data &= ~SWITCH_FAST_AGING;

	SW_W(sw, REG_SWITCH_CTRL_1, data);

	data = SW_R(sw, REG_SWITCH_CTRL_2);

	/* Make sure unicast VLAN boundary is set as default. */
	if (sw->dev_count > 1)
		data |= UNICAST_VLAN_BOUNDARY;

	/* Enable no excessive collision drop. */
	data |= NO_EXC_COLLISION_DROP;
	SW_W(sw, REG_SWITCH_CTRL_2, data);

#ifdef REG_ANA_CTRL_3
	/* Enable LinkMD function. */
	sw_w16(sw, REG_ANA_CTRL_3, 0x8008);
#endif

#ifdef CONFIG_KSZ_HSR
	if (sw->features & HSR_HW) {
		int n;

		for (n = 0; n < sw->eth_cnt; n++) {
			if (sw->eth_maps[n].proto & HSR_HW) {
				if (sw->netdev[n]) {
					sw->ops->release(sw);
					sw->ops->cfg_mac(sw, BRIDGE_ADDR_ENTRY,
						sw->netdev[n]->dev_addr,
						sw->HOST_MASK, false, false, 0);
					sw->ops->acquire(sw);
				}
				break;
			}
		}
	}
#endif
}  /* sw_set_global_ctrl */

/* -------------------------------------------------------------------------- */

/**
 * port_set_stp_state - configure port spanning tree state
 * @sw:		The switch instance.
 * @port:	The port index.
 * @state:	The spanning tree state.
 *
 * This routine configures the spanning tree state of the port.
 */
static void port_set_stp_state(struct ksz_sw *sw, uint port, int state)
{
	SW_D data;
	struct ksz_port_cfg *port_cfg;
	int member = -1;

	port_cfg = get_port_cfg(sw, port);
	port_r(sw, port, P_STP_CTRL, &data);
	switch (state) {
	case STP_STATE_DISABLED:
		data &= ~(PORT_TX_ENABLE | PORT_RX_ENABLE);
		data |= PORT_LEARN_DISABLE;
		if (port < SWITCH_PORT_NUM)
			member = 0;
		break;
	case STP_STATE_LISTENING:
/*
 * No need to turn on transmit because of port direct mode.
 * Turning on receive is required if static MAC table is not setup.
 */
		data &= ~PORT_TX_ENABLE;
		data |= PORT_RX_ENABLE;
		data |= PORT_LEARN_DISABLE;
		if (port < SWITCH_PORT_NUM &&
		    STP_STATE_DISABLED == port_cfg->stp_state)
			member = sw->HOST_MASK | port_cfg->vid_member;
		break;
	case STP_STATE_LEARNING:
		data &= ~PORT_TX_ENABLE;
		data |= PORT_RX_ENABLE;
		data &= ~PORT_LEARN_DISABLE;
		break;
	case STP_STATE_FORWARDING:
		data |= (PORT_TX_ENABLE | PORT_RX_ENABLE);
		data &= ~PORT_LEARN_DISABLE;

#ifdef CONFIG_KSZ_STP
		/* Actual port membership setting is done in another RSTP
		 * processing routine.
		 */
		if (sw->stp == 1 || (sw->stp && (sw->stp & (1 << port)))) {
			struct ksz_stp_info *info = &sw->info->rstp;

			if (info->br.bridgeEnabled)
				break;
		}
#endif
		if (((sw->features & SW_VLAN_DEV) || sw->dev_offset) &&
		    port != sw->HOST_PORT)
			/* Set port-base vlan membership with host port. */
			member = sw->HOST_MASK | port_cfg->vid_member;
		break;
	case STP_STATE_BLOCKED:
/*
 * Need to setup static MAC table with override to keep receiving BPDU
 * messages.  See sw_setup_stp routine.
 */
		data &= ~(PORT_TX_ENABLE | PORT_RX_ENABLE);
		data |= PORT_LEARN_DISABLE;
		if (port < SWITCH_PORT_NUM &&
		    STP_STATE_DISABLED == port_cfg->stp_state)
			member = sw->HOST_MASK | port_cfg->vid_member;
		break;
	case STP_STATE_SIMPLE:
		data |= (PORT_TX_ENABLE | PORT_RX_ENABLE);
		data |= PORT_LEARN_DISABLE;
		if (port < SWITCH_PORT_NUM)
			/* Set port-base vlan membership with host port. */
			member = sw->HOST_MASK | port_cfg->vid_member;
		break;
	}
	port_w(sw, port, P_STP_CTRL, data);
	port_cfg->stp_state = state;
	if (data & PORT_RX_ENABLE)
		sw->rx_ports |= (1 << port);
	else
		sw->rx_ports &= ~(1 << port);
	if (data & PORT_TX_ENABLE)
		sw->tx_ports |= (1 << port);
	else
		sw->tx_ports &= ~(1 << port);

	/* Port membership may share register with STP state. */
	if (member >= 0)
		sw_cfg_port_base_vlan(sw, port, (u8) member);
}

/**
 * sw_clr_sta_mac_table - clear static MAC table
 * @sw:		The switch instance.
 *
 * This routine clears the static MAC table.
 */
static void sw_clr_sta_mac_table(struct ksz_sw *sw)
{
	struct ksz_mac_table *entry;
	int i;

	for (i = 0; i < STATIC_MAC_TABLE_ENTRIES; i++) {
		entry = &sw->info->mac_table[i];
		sw_w_sta_mac_table(sw, i,
			entry->mac_addr, entry->ports,
			0, 0,
			entry->use_fid, entry->fid);
	}
}

/**
 * sw_setup_stp - setup switch spanning tree support
 * @sw:		The switch instance.
 *
 * This routine initializes the spanning tree support of the switch.
 */
static void sw_setup_stp(struct ksz_sw *sw)
{
	struct ksz_mac_table *entry;
	struct ksz_alu_table *alu;
	struct ksz_sw_info *info = sw->info;

	entry = &info->mac_table[STP_ENTRY];
	entry->mac_addr[0] = 0x01;
	entry->mac_addr[1] = 0x80;
	entry->mac_addr[2] = 0xC2;
	entry->mac_addr[3] = 0x00;
	entry->mac_addr[4] = 0x00;
	entry->mac_addr[5] = 0x00;
	entry->ports = sw->HOST_MASK;
	entry->override = 1;
	entry->valid = 1;
	alu = &info->alu_table[STP_ENTRY];
	alu->forward = FWD_STP_DEV | FWD_HOST | FWD_HOST_OVERRIDE;
	alu->owner = sw->PORT_MASK;
	alu->valid = 1;
	sw_w_sta_mac_table(sw, STP_ENTRY,
		entry->mac_addr, entry->ports,
		entry->override, entry->valid,
		entry->use_fid, entry->fid);

	entry = &info->mac_table[BROADCAST_ENTRY];
	memset(entry->mac_addr, 0xFF, ETH_ALEN);
	entry->ports = sw->HOST_MASK;
	entry->override = 0;
	entry->valid = 1;
	alu = &sw->info->alu_table[BROADCAST_ENTRY];
	alu->forward = FWD_MAIN_DEV | FWD_STP_DEV | FWD_HOST;
	alu->owner = sw->PORT_MASK;
	alu->valid = 1;

	entry = &info->mac_table[BRIDGE_ADDR_ENTRY];
	memcpy(entry->mac_addr, info->mac_addr, ETH_ALEN);
	entry->ports = sw->HOST_MASK;
	entry->override = 0;
	entry->valid = 1;
	alu = &sw->info->alu_table[BRIDGE_ADDR_ENTRY];
	alu->forward = FWD_MAIN_DEV | FWD_HOST;
	alu->owner = sw->PORT_MASK;
	alu->valid = 1;

	entry = &info->mac_table[IPV6_ADDR_ENTRY];
	memcpy(entry->mac_addr, info->mac_addr, ETH_ALEN);
	entry->mac_addr[0] = 0x33;
	entry->mac_addr[1] = 0x33;
	entry->mac_addr[2] = 0xFF;
	entry->ports = sw->HOST_MASK;
	entry->override = 0;
	entry->valid = 1;
	alu = &sw->info->alu_table[IPV6_ADDR_ENTRY];
	alu->forward = FWD_MAIN_DEV | FWD_STP_DEV | FWD_HOST;
	alu->owner = sw->PORT_MASK;
	alu->valid = 1;
}

#ifdef CONFIG_KSZ_STP
static void bridge_change(struct ksz_sw *sw)
{
	uint port;
	u8 member;
	struct ksz_sw_info *info = sw->info;

	for (port = 0; port < SWITCH_PORT_NUM; port++) {
		if (STP_STATE_FORWARDING == info->port_cfg[port].stp_state)
			member = sw->HOST_MASK | info->member[0];
		else if (STP_STATE_DISABLED == info->port_cfg[port].stp_state)
			member = 0;
		else
			member = sw->HOST_MASK | (1 << port);
		if (member != info->port_cfg[port].member)
			sw_cfg_port_base_vlan(sw, port, member);
	}
}  /* bridge_change */
#endif

#define MAX_SW_LEN			1500

#ifdef CONFIG_KSZ_STP
#include "ksz_stp.c"
#endif
#ifdef CONFIG_KSZ_DLR
#include "ksz_dlr.c"
#endif
#ifdef CONFIG_KSZ_HSR
#include "ksz_hsr.c"
#endif

/*
 * Link detection routines
 */

static inline void dbp_link(struct ksz_port *port, struct ksz_sw *sw,
	int change)
{
	struct ksz_port_info *info;
	uint i;
	uint n;
	uint p;

	for (i = 0, n = port->first_port; i < port->port_cnt; i++, n++) {
		p = get_phy_port(sw, n);
		info = get_port_info(sw, p);
		if (media_connected == info->state) {
			if (change & (1 << i)) {
				printk(KERN_INFO "link %d-%d: %d, %d\n",
					sw->id, i + port->first_port,
					info->tx_rate / TX_RATE_UNIT,
					info->duplex);
			}
		} else {
			if (change & (1 << i))
				printk(KERN_INFO "link %d-%d disconnected\n",
					sw->id, i + port->first_port);
		}
	}
}

static SW_D port_advertised_flow_ctrl(struct ksz_port *port, SW_D ctrl)
{
	ctrl &= ~PORT_AUTO_NEG_SYM_PAUSE;
	switch (port->flow_ctrl) {
	case PHY_FLOW_CTRL:
		ctrl |= PORT_AUTO_NEG_SYM_PAUSE;
		break;
	/* Not supported. */
	case PHY_TX_ONLY:
	case PHY_RX_ONLY:
	default:
		break;
	}
	return ctrl;
}

static u8 sw_determine_flow_ctrl(struct ksz_sw *sw, struct ksz_port *port,
	u8 local, u8 remote)
{
	int rx;
	int tx;
	u8 flow = 0;

	if (sw->overrides & PAUSE_FLOW_CTRL)
		return flow;

	rx = tx = 0;
	if (port->force_link)
		rx = tx = 1;
	if (remote & PORT_REMOTE_SYM_PAUSE) {
		if (local & PORT_AUTO_NEG_SYM_PAUSE)
			rx = tx = 1;
	}
	if (rx)
		flow |= 0x01;
	if (tx)
		flow |= 0x02;
#ifdef DEBUG
	printk(KERN_INFO "pause: %d, %d; %02x %02x\n",
		rx, tx, local, remote);
#endif
	return flow;
}

static void sw_notify_link_change(struct ksz_sw *sw, uint ports)
{
	static u8 link_buf[sizeof(struct ksz_info_opt) +
		sizeof(struct ksz_info_speed) * TOTAL_PORT_NUM +
		sizeof(struct ksz_resp_msg)];

	if ((sw->notifications & SW_INFO_LINK_CHANGE)) {
		struct ksz_resp_msg *msg = (struct ksz_resp_msg *) link_buf;
		struct ksz_info_opt *opt = (struct ksz_info_opt *)
			&msg->resp.data;
		struct ksz_port_info *info;
		struct ksz_info_speed *speed;
		struct file_dev_info *dev_info;
		int c;
		int n;
		int p;
		int q;

		/* Check whether only 1 port has change. */
		c = 0;
		q = 0;
		for (n = 1; n <= sw->mib_port_cnt; n++) {
			p = get_phy_port(sw, n);
			if (ports & (1 << p)) {
				q = n;
				c++;
			}
		}
		if (c > 1) {
			c = sw->mib_port_cnt;
			q = 1;
		}
		msg->module = DEV_MOD_BASE;
		msg->cmd = DEV_INFO_SW_LINK;
		opt->num = (u8) c;
		opt->port = (u8) q;
		speed = &opt->data.speed;
		for (n = 1; n <= opt->num; n++, q++) {
			p = get_phy_port(sw, q);
			info = get_port_info(sw, p);
			if (info->state == media_connected) {
				speed->tx_rate = info->tx_rate;
				speed->duplex = info->duplex;
				speed->flow_ctrl = info->flow_ctrl;
			} else {
				speed->tx_rate = 0;
				speed->duplex = 0;
				speed->flow_ctrl = 0;
			}
			++speed;
		}
		n = opt->num * sizeof(struct ksz_info_speed);
		n += 2;
		n += sizeof(struct ksz_resp_msg);
		n -= 4;
		dev_info = sw->dev_list[0];
		while (dev_info) {
			if ((dev_info->notifications[DEV_MOD_BASE] &
			    SW_INFO_LINK_CHANGE))
				file_dev_setup_msg(dev_info, msg, n, NULL,
						   NULL);
			dev_info = dev_info->next;
		}
	}
}  /* sw_notify_link_change */

static int port_chk_force_link(struct ksz_sw *sw, uint p, SW_D data,
	SW_D status)
{
#define PORT_REMOTE_STATUS				\
	(PORT_REMOTE_100BTX_FD | PORT_REMOTE_100BTX |	\
	PORT_REMOTE_10BT_FD | PORT_REMOTE_10BT)

	static SW_D saved_ctrl;
	static SW_D saved_status;
	static int test_stage;

	if (!(data & PORT_AUTO_NEG_ENABLE))
		return 0;
	if (!(status & PORT_REMOTE_SYM_PAUSE) &&
	    (status & PORT_REMOTE_STATUS) != PORT_REMOTE_STATUS) {
		if (saved_ctrl) {
			if ((status & PORT_STAT_FULL_DUPLEX) !=
			    (saved_status & PORT_STAT_FULL_DUPLEX)) {
				printk(KERN_INFO
					"%d-%d: duplex is defaulted to %s\n",
					sw->id, p,
					(saved_ctrl & PORT_FORCE_FULL_DUPLEX) ?
					"full" : "half");
			}
			if (data != saved_ctrl) {
				saved_ctrl |= PORT_AUTO_NEG_RESTART;
				port_w(sw, p, P_PHY_CTRL, saved_ctrl);
				test_stage = 2;
			} else
				test_stage = 0;
			saved_ctrl = 0;
			if (test_stage)
				return 1;
		} else if (!test_stage) {
			saved_ctrl = data;
			saved_status = status;
			if (status & PORT_STAT_FULL_DUPLEX)
				data &= ~PORT_FORCE_FULL_DUPLEX;
			else
				data |= PORT_FORCE_FULL_DUPLEX;
			data |= PORT_AUTO_NEG_RESTART;
			port_w(sw, p, P_PHY_CTRL, data);
			test_stage = 1;
			return 1;
		}
		test_stage = 0;
	}
	return 0;
}

/**
 * port_get_link_speed - get current link status
 * @port:	The port instance.
 *
 * This routine reads PHY registers to determine the current link status of the
 * switch ports.
 */
static int port_get_link_speed(struct ksz_port *port)
{
	struct ksz_port_info *info;
	struct ksz_port_info *linked = NULL;
	struct ksz_port_state *state;
	struct ksz_sw *sw = port->sw;
	SW_D data;
	SW_D status;
	u8 local;
	u8 remote;
	uint i;
	uint n;
	uint p;
	int change = 0;

	for (i = 0, n = port->first_port; i < port->port_cnt; i++, n++) {
		p = get_phy_port(sw, n);
		info = get_port_info(sw, p);
		state = &sw->port_state[p];
		port_r(sw, p, P_PHY_CTRL, &data);
		port_r(sw, p, P_SPEED_STATUS, &status);

#if (SW_SIZE == (1))
		port_r(sw, p, P_LINK_STATUS, &remote);
#else
		remote = status;
#endif
		remote &= ~PORT_MDIX_STATUS;
		local = (u8) data;

		if (remote & PORT_STATUS_LINK_GOOD) {

			/* Remember the first linked port. */
			if (!linked)
				linked = info;
		}

		/* No change to status. */
		if (local == info->advertised && remote == info->partner)
			continue;

#ifdef DEBUG
		printk(KERN_INFO "advertised: %02X-%02X; partner: %02X-%02X\n",
			local, info->advertised, remote, info->partner);
#endif
		if (remote & PORT_STATUS_LINK_GOOD) {
			if (port_chk_force_link(sw, p, data, status)) {
				if (linked == info)
					linked = NULL;
				continue;
			}
			info->tx_rate = 10 * TX_RATE_UNIT;
			if (status & PORT_STAT_SPEED_100MBIT)
				info->tx_rate = 100 * TX_RATE_UNIT;

			info->duplex = 1;
			if (status & PORT_STAT_FULL_DUPLEX)
				info->duplex = 2;

#ifdef DEBUG
			printk(KERN_INFO "flow_ctrl: "SW_SIZE_STR"\n", status &
				(PORT_RX_FLOW_CTRL | PORT_TX_FLOW_CTRL));
#endif
			if (media_connected != info->state) {
				info->flow_ctrl = sw_determine_flow_ctrl(sw,
					port, local, remote);
				if (status & PORT_RX_FLOW_CTRL)
					info->flow_ctrl |= 0x10;
				if (status & PORT_TX_FLOW_CTRL)
					info->flow_ctrl |= 0x20;
				if (sw->info)
					port_cfg_back_pressure(sw, p,
						(1 == info->duplex));
				change |= 1 << i;
			}
			info->state = media_connected;
		} else {
			if (media_disconnected != info->state) {
				change |= 1 << i;

				/* Indicate the link just goes down. */
				state->link_down = 1;
			}
			info->state = media_disconnected;
		}
		info->advertised = local;
		info->partner = remote;
		state->state = info->state;
	}

	if (linked && media_disconnected == port->linked->state)
		port->linked = linked;

#ifdef DEBUG
	if (change)
		dbp_link(port, sw, change);
#endif
	if (change) {
		port->link_ports |= change;
		schedule_work(&port->link_update);
	}
	return change;
}

/**
 * port_set_link_speed - set port speed
 * @port:	The port instance.
 *
 * This routine sets the link speed of the switch ports.
 */
static void port_set_link_speed(struct ksz_port *port)
{
	struct ksz_sw *sw = port->sw;
	struct ksz_port_info *info;
	SW_D data;
	SW_D cfg;
	SW_D status;
	uint i;
	uint n;
	uint p;

	for (i = 0, n = port->first_port; i < port->port_cnt; i++, n++) {
		p = get_phy_port(sw, n);
		info = get_port_info(sw, p);
		if (info->fiber)
			continue;

		info->own_flow_ctrl = port->flow_ctrl;
		info->own_duplex = port->duplex;
		info->own_speed = port->speed;

		port_r(sw, p, P_PHY_CTRL, &data);
		port_r(sw, p, P_LINK_STATUS, &status);

		cfg = 0;
		if (status & PORT_STATUS_LINK_GOOD)
			cfg = data;

		data |= PORT_AUTO_NEG_ENABLE;
		data = port_advertised_flow_ctrl(port, data);

		data |= PORT_AUTO_NEG_100BTX_FD | PORT_AUTO_NEG_100BTX |
			PORT_AUTO_NEG_10BT_FD | PORT_AUTO_NEG_10BT;

		/* Check if manual configuration is specified by the user. */
		if (port->speed || port->duplex) {
			if (10 == port->speed)
				data &= ~(PORT_AUTO_NEG_100BTX_FD |
					PORT_AUTO_NEG_100BTX);
			else if (100 == port->speed)
				data &= ~(PORT_AUTO_NEG_10BT_FD |
					PORT_AUTO_NEG_10BT);
			if (1 == port->duplex)
				data &= ~(PORT_AUTO_NEG_100BTX_FD |
					PORT_AUTO_NEG_10BT_FD);
			else if (2 == port->duplex)
				data &= ~(PORT_AUTO_NEG_100BTX |
					PORT_AUTO_NEG_10BT);
		}
		if (data != cfg) {
#if (SW_SIZE == (1))
			port_w(sw, p, P_PHY_CTRL, data);
			port_r(sw, p, P_NEG_RESTART_CTRL, &data);
#endif
			data |= PORT_AUTO_NEG_RESTART;
			port_w(sw, p, P_NEG_RESTART_CTRL, data);

			/* Link is going down. */
			sw->port_state[p].state = media_disconnected;
		}
	}
}

/**
 * port_force_link_speed - force port speed
 * @port:	The port instance.
 *
 * This routine forces the link speed of the switch ports.
 */
static void port_force_link_speed(struct ksz_port *port)
{
	struct ksz_sw *sw = port->sw;
	struct ksz_port_info *info;
	SW_D data;
	uint i;
	uint n;
	uint p;

	for (i = 0, n = port->first_port; i < port->port_cnt; i++, n++) {
		p = get_phy_port(sw, n);
		info = get_port_info(sw, p);
		info->own_flow_ctrl = port->flow_ctrl;
		info->own_duplex = port->duplex;
		info->own_speed = port->speed;

		port_r(sw, p, P_PHY_CTRL, &data);
		data &= ~PORT_AUTO_NEG_ENABLE;
		if (10 == port->speed)
			data &= ~PORT_FORCE_100_MBIT;
		else if (100 == port->speed)
			data |= PORT_FORCE_100_MBIT;
		if (1 == port->duplex)
			data &= ~PORT_FORCE_FULL_DUPLEX;
		else if (2 == port->duplex)
			data |= PORT_FORCE_FULL_DUPLEX;
		port_w(sw, p, P_PHY_CTRL, data);
	}
}

/**
 * sw_enable - enable the switch
 * @sw:		The switch instance.
 *
 * This routine enables the switch with specific configuration.
 */
static void sw_enable(struct ksz_sw *sw)
{
	uint n;
	uint port;
	int state = STP_STATE_FORWARDING;

	if ((sw->dev_count > 1 && !sw->dev_offset) ||
	    (sw->features & STP_SUPPORT)) {
		u8 member;

		for (n = 1; n <= SWITCH_PORT_NUM; n++) {
			port = get_phy_port(sw, n);
			member = (1 << port);
			if (sw->features & SW_VLAN_DEV) {
				struct ksz_dev_map *map;
				int q;

				for (q = 0; q < sw->eth_cnt; q++) {
					map = &sw->eth_maps[q];
					if (map->first <= n &&
					    n <= map->first + map->cnt - 1) {
						member = map->mask;
						break;
					}
				}
			}
			sw->info->port_cfg[port].vid_member = member;
		}
	} else if (1 == sw->eth_cnt) {
		u8 member;

		for (n = 1; n <= sw->mib_port_cnt; n++) {
			port = get_phy_port(sw, n);
			member = 0;
			if (sw->eth_maps[0].mask & (1 << port))
				member = sw->eth_maps[0].mask;
			sw->info->port_cfg[port].vid_member = member;
		}
	}
	for (port = 0; port < SWITCH_PORT_NUM; port++) {
		if (sw->dev_count > 1 ||
		    (sw->eth_maps[0].mask &&
		    !(sw->eth_maps[0].mask & (1 << port))))
			port_set_stp_state(sw, port, STP_STATE_DISABLED);
		else
			port_set_stp_state(sw, port, state);
	}
	if (sw->dev_count > 1 && !sw->dev_offset && sw->eth_cnt < 2)
		port_set_stp_state(sw, SWITCH_PORT_NUM, STP_STATE_SIMPLE);
	else
		port_set_stp_state(sw, SWITCH_PORT_NUM, state);

	/*
	 * There may be some entries in the dynamic MAC table before the
	 * the learning is turned off.  Once the entries are in the table the
	 * switch may keep updating them even learning is off.
	 */
	if (sw->dev_count > 1)
		sw_flush_dyn_mac_table(sw, TOTAL_PORT_NUM);
}  /* sw_enable */

/**
 * sw_init - initialize the switch
 * @sw:		The switch instance.
 *
 * This routine initializes the hardware switch engine for default operation.
 */
static void sw_init(struct ksz_sw *sw)
{
	memset(sw->tx_pad, 0, 60);
	sw->tx_start = 0;

#ifdef REG_DSP_CTRL_6
	do {
		SW_D data;
		int i;
		int fiber = 0;

		for (i = 0; i < SWITCH_PORT_NUM; i++) {
			if (sw->port_info[i].fiber)
				fiber |= (1 << i);
		}
		if (fiber) {
			data = SW_R(sw, REG_CFG_CTRL);
			data &= ~(fiber << PORT_COPPER_MODE_S);
			SW_W(sw, REG_CFG_CTRL, data);
			data = SW_R(sw, REG_DSP_CTRL_6);
			data &= ~COPPER_RECEIVE_ADJUSTMENT;
			SW_W(sw, REG_DSP_CTRL_6, data);
		}
	} while (0);
#endif

#ifdef SWITCH_PORT_PHY_ADDR_MASK
	sw_init_phy_addr(sw);
#endif

	sw_init_broad_storm(sw);

	sw_init_prio(sw);

	sw_init_prio_rate(sw);

	sw_init_vlan(sw);

	if (!sw_chk(sw, REG_SWITCH_CTRL_1,
			SWITCH_TX_FLOW_CTRL | SWITCH_RX_FLOW_CTRL))
		sw->overrides |= PAUSE_FLOW_CTRL;
}  /* sw_init */

/**
 * sw_setup - setup the switch
 * @sw:		The switch instance.
 *
 * This routine setup the hardware switch engine for default operation.
 */
static void sw_setup(struct ksz_sw *sw)
{
	uint port;

	sw_set_global_ctrl(sw);
	for (port = 0; port < SWITCH_PORT_NUM; port++) {
		SW_D data;

		port_cfg_back_pressure(sw, port, 1);

		/*
		 * Switch actually cannot do auto-negotiation with old 10Mbit
		 * hub.
		 */
		port_r(sw, port, P_FORCE_CTRL, &data);
		if (sw->port_info[port].fiber) {
			port_cfg_force_flow_ctrl(sw, port, 1);
			data &= ~PORT_AUTO_NEG_ENABLE;
			data |= PORT_FORCE_FULL_DUPLEX;
		} else {
			port_cfg_force_flow_ctrl(sw, port, 0);
			data &= ~PORT_FORCE_FULL_DUPLEX;
		}
		port_w(sw, port, P_FORCE_CTRL, data);
	}

	sw_setup_broad_storm(sw);

	sw_setup_prio(sw);

	sw_setup_mirror(sw);

	sw->info->multi_sys = MULTI_MAC_TABLE_ENTRIES;
	sw->info->multi_net = SWITCH_MAC_TABLE_ENTRIES;
	if (sw->features & STP_SUPPORT) {
		sw_setup_stp(sw);
	}
#ifdef CONFIG_KSZ_DLR
	if (sw->features & DLR_HW)
		sw_setup_dlr(sw);
#endif
}  /* sw_setup */

static inline void sw_reset(struct ksz_sw *sw)
{
	u8 data;

	data = sw->reg->r8(sw, REG_SWITCH_RESET);
	data |= GLOBAL_SOFTWARE_RESET;
	sw->reg->w8(sw, REG_SWITCH_RESET, data);
	udelay(1);
	data &= ~GLOBAL_SOFTWARE_RESET;
	sw->reg->w8(sw, REG_SWITCH_RESET, data);
}  /* sw_reset */

/* -------------------------------------------------------------------------- */

#ifdef CONFIG_HAVE_KSZ8463
#define KSZSW_REGS_SIZE			0x800

static struct sw_regs {
	int start;
	int end;
} sw_regs_range[] = {
	{ 0x000, 0x1BA },
	{ 0x200, 0x398 },
	{ 0x400, 0x690 },
	{ 0x734, 0x736 },
	{ 0x748, 0x74E },
	{ 0, 0 }
};
#else
#define KSZSW_REGS_SIZE			0x100

static struct sw_regs {
	int start;
	int end;
} sw_regs_range[] = {
	{ 0x000, 0x100 },
	{ 0, 0 }
};
#endif

static int check_sw_reg_range(unsigned addr)
{
	struct sw_regs *range = sw_regs_range;

	while (range->end > range->start) {
		if (range->start <= addr && addr < range->end)
			return true;
		range++;
	}
	return false;
}

/* -------------------------------------------------------------------------- */

#ifdef KSZSW_REGS_SIZE
static struct ksz_sw *get_sw_data(struct device *d)
{
	struct sw_priv *hw_priv = dev_get_drvdata(d);

	return &hw_priv->sw;
}

static ssize_t kszsw_registers_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	size_t i;
	SW_D reg;
	struct device *dev;
	struct ksz_sw *sw;

	if (unlikely(off > KSZSW_REGS_SIZE))
		return 0;

	if ((off + count) > KSZSW_REGS_SIZE)
		count = KSZSW_REGS_SIZE - off;

	if (unlikely(!count))
		return count;

	dev = container_of(kobj, struct device, kobj);
	sw = get_sw_data(dev);

	reg = off;
	sw->ops->acquire(sw);
	i = sw->reg->get(sw, reg, count, buf);
	sw->ops->release(sw);
	return i;
}

static ssize_t kszsw_registers_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	size_t i;
	SW_D reg;
	struct device *dev;
	struct ksz_sw *sw;

	if (unlikely(off >= KSZSW_REGS_SIZE))
		return -EFBIG;

	if ((off + count) > KSZSW_REGS_SIZE)
		count = KSZSW_REGS_SIZE - off;

	if (unlikely(!count))
		return count;

	dev = container_of(kobj, struct device, kobj);
	sw = get_sw_data(dev);

	reg = off;
	sw->ops->acquire(sw);
	i = sw->reg->set(sw, reg, count, buf);
	sw->ops->release(sw);
	return i;
}

static struct bin_attribute kszsw_registers_attr = {
	.attr = {
		.name	= "registers",
		.mode	= S_IRUSR | S_IWUSR,
	},
	.size	= KSZSW_REGS_SIZE,
	.read	= kszsw_registers_read,
	.write	= kszsw_registers_write,
};
#endif

static int sw_reg_get(struct ksz_sw *sw, u32 reg, size_t count, char *buf)
{
	size_t i;
	SW_D *addr;

	addr = (SW_D *) buf;
	for (i = 0; i < count; i += SW_SIZE, reg += SW_SIZE, addr++) {
		*addr = 0;
		if (check_sw_reg_range(reg))
			*addr = SW_R(sw, reg);
	}
	return i;
}

static int sw_reg_set(struct ksz_sw *sw, u32 reg, size_t count, char *buf)
{
	size_t i;
	SW_D *addr;

	addr = (SW_D *) buf;
	for (i = 0; i < count; i += SW_SIZE, reg += SW_SIZE, addr++) {
		if (check_sw_reg_range(reg))
			SW_W(sw, reg, *addr);
	}
	return i;
}

/* -------------------------------------------------------------------------- */

/*
 * Microchip LinkMD routines
 */

enum {
	CABLE_UNKNOWN,
	CABLE_GOOD,
	CABLE_CROSSED,
	CABLE_REVERSED,
	CABLE_CROSSED_REVERSED,
	CABLE_OPEN,
	CABLE_SHORT
};

#define STATUS_FULL_DUPLEX		0x01
#define STATUS_CROSSOVER		0x02
#define STATUS_REVERSED			0x04

#define LINK_10MBPS_FULL		0x00000001
#define LINK_10MBPS_HALF		0x00000002
#define LINK_100MBPS_FULL		0x00000004
#define LINK_100MBPS_HALF		0x00000008
#define LINK_1GBPS_FULL			0x00000010
#define LINK_1GBPS_HALF			0x00000020
#define LINK_10GBPS_FULL		0x00000040
#define LINK_10GBPS_HALF		0x00000080
#define LINK_SYM_PAUSE			0x00000100
#define LINK_ASYM_PAUSE			0x00000200

#define LINK_AUTO_MDIX			0x00010000
#define LINK_MDIX			0x00020000
#define LINK_AUTO_POLARITY		0x00040000

#define CABLE_LEN_MAXIMUM		15000
#define CABLE_LEN_MULTIPLIER		41

#define PHY_RESET_TIMEOUT		10

/**
 * sw_get_link_md - get LinkMD status
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine is used to get the LinkMD status.
 */
static void sw_get_link_md(struct ksz_sw *sw, uint port)
{
	SW_D crossover;
	SW_D ctrl;
	SW_D data;
	SW_D link;
	u16 len;
	int i;
	int timeout;
	struct ksz_port_info *port_info = &sw->port_info[port];

	if (port_info->fiber)
		return;
	port_r(sw, port, P_SPEED_STATUS, &data);
#if (SW_SIZE == (1))
	port_r(sw, port, P_LINK_STATUS, &link);
#else
	link = data;
#endif
	port_info->status[0] = CABLE_UNKNOWN;
	if (link & PORT_STATUS_LINK_GOOD) {
		int stat = 0;

		port_info->status[0] = CABLE_GOOD;
		port_info->length[0] = 1;
		port_info->status[1] = CABLE_GOOD;
		port_info->length[1] = 1;
		port_info->status[2] = CABLE_GOOD;
		port_info->length[2] = 1;

		if (link & PORT_MDIX_STATUS)
			stat |= STATUS_CROSSOVER;
		if (data & PORT_REVERSED_POLARITY)
			stat |= STATUS_REVERSED;
		if ((stat & (STATUS_CROSSOVER | STATUS_REVERSED)) ==
				(STATUS_CROSSOVER | STATUS_REVERSED))
			port_info->status[0] = CABLE_CROSSED_REVERSED;
		else if ((stat & STATUS_CROSSOVER) == STATUS_CROSSOVER)
			port_info->status[0] = CABLE_CROSSED;
		else if ((stat & STATUS_REVERSED) == STATUS_REVERSED)
			port_info->status[0] = CABLE_REVERSED;
		return;
	}

	/* Put in 10 Mbps mode. */
	port_r(sw, port, P_PHY_CTRL, &ctrl);
	data = ctrl;
	data &= ~(PORT_AUTO_NEG_ENABLE | PORT_FORCE_100_MBIT |
		PORT_FORCE_FULL_DUPLEX);
#if (SW_SIZE == (1))
	port_w(sw, port, P_PHY_CTRL, data);

	port_r(sw, port, P_NEG_RESTART_CTRL, &data);
#endif
	crossover = data;

	for (i = 1; i <= 2; i++) {
		data = crossover;

		/* Disable auto MDIX. */
		data |= PORT_AUTO_MDIX_DISABLE;
		if (0 == i)
			data &= ~PORT_FORCE_MDIX;
		else
			data |= PORT_FORCE_MDIX;

		/* Disable transmitter. */
		data |= PORT_TX_DISABLE;
		port_w(sw, port, P_NEG_RESTART_CTRL, data);

		/* Wait at most 1 second.*/
		delay_milli(100);

		/* Enable transmitter. */
		data &= ~PORT_TX_DISABLE;
		port_w(sw, port, P_NEG_RESTART_CTRL, data);

		/* Start cable diagnostic test. */
		port_r(sw, port, REG_PORT_LINK_MD_CTRL, &data);
		data |= PORT_START_CABLE_DIAG;
		port_w(sw, port, REG_PORT_LINK_MD_CTRL, data);
		timeout = PHY_RESET_TIMEOUT;
		do {
			if (!--timeout)
				break;
			delay_milli(10);
			port_r(sw, port, REG_PORT_LINK_MD_CTRL, &data);
		} while ((data & PORT_START_CABLE_DIAG));

		port_info->length[i] = 0;
		port_info->status[i] = CABLE_UNKNOWN;

		if (!(data & PORT_START_CABLE_DIAG)) {
#if (SW_SIZE == (1))
			port_r8(sw, port, REG_PORT_LINK_MD_RESULT, &link);
			len = data & PORT_CABLE_FAULT_COUNTER_H;
			len <<= 16;
			len |= link;
#else
			len = data & PORT_CABLE_FAULT_COUNTER;
#endif
			port_info->length[i] = len *
				CABLE_LEN_MULTIPLIER;
			if (data & PORT_CABLE_10M_SHORT)
				port_info->length[i] = 1;
			data &= PORT_CABLE_DIAG_RESULT;
			switch (data) {
			case PORT_CABLE_STAT_NORMAL:
				port_info->status[i] = CABLE_GOOD;
				port_info->length[i] = 1;
				break;
			case PORT_CABLE_STAT_OPEN:
				port_info->status[i] = CABLE_OPEN;
				break;
			case PORT_CABLE_STAT_SHORT:
				port_info->status[i] = CABLE_SHORT;
				break;
			}
		}
	}

	port_w(sw, port, P_PHY_CTRL, ctrl);
	if (ctrl & PORT_AUTO_NEG_ENABLE) {
		crossover |= PORT_AUTO_NEG_RESTART;
		port_w(sw, port, P_NEG_RESTART_CTRL, crossover);
	}

	port_info->length[0] = port_info->length[1];
	port_info->status[0] = port_info->status[1];
	for (i = 2; i < 3; i++) {
		if (CABLE_GOOD == port_info->status[0]) {
			if (port_info->status[i] != CABLE_GOOD) {
				port_info->status[0] = port_info->status[i];
				port_info->length[0] = port_info->length[i];
				break;
			}
		}
	}
}

/* -------------------------------------------------------------------------- */

static void get_sw_mib_counters(struct ksz_sw *sw, int first, int cnt,
	u64 *counter)
{
	int i;
	int mib;
	uint n;
	uint p;
	struct ksz_port_mib *port_mib;

	memset(counter, 0, sizeof(u64) * TOTAL_SWITCH_COUNTER_NUM);
	for (i = 0, n = first; i < cnt; i++, n++) {
		p = get_phy_port(sw, n);
		port_mib = get_port_mib(sw, p);
		for (mib = port_mib->mib_start; mib < sw->mib_cnt; mib++)
			counter[mib] += port_mib->counter[mib];
	}
}

#define MIB_RX_LO_PRIO			0x00
#define MIB_RX_HI_PRIO			0x01
#define MIB_RX_UNDERSIZE		0x02
#define MIB_RX_FRAGMENT			0x03
#define MIB_RX_OVERSIZE			0x04
#define MIB_RX_JABBER			0x05
#define MIB_RX_SYMBOL_ERR		0x06
#define MIB_RX_CRC_ERR			0x07
#define MIB_RX_ALIGNMENT_ERR		0x08
#define MIB_RX_CTRL_8808		0x09
#define MIB_RX_PAUSE			0x0A
#define MIB_RX_BROADCAST		0x0B
#define MIB_RX_MULTICAST		0x0C
#define MIB_RX_UNICAST			0x0D
#define MIB_RX_OCTET_64			0x0E
#define MIB_RX_OCTET_65_127		0x0F
#define MIB_RX_OCTET_128_255		0x10
#define MIB_RX_OCTET_256_511		0x11
#define MIB_RX_OCTET_512_1023		0x12
#define MIB_RX_OCTET_1024_1522		0x13
#define MIB_TX_LO_PRIO			0x14
#define MIB_TX_HI_PRIO			0x15
#define MIB_TX_LATE_COLLISION		0x16
#define MIB_TX_PAUSE			0x17
#define MIB_TX_BROADCAST		0x18
#define MIB_TX_MULTICAST		0x19
#define MIB_TX_UNICAST			0x1A
#define MIB_TX_DEFERRED			0x1B
#define MIB_TX_TOTAL_COLLISION		0x1C
#define MIB_TX_EXCESS_COLLISION		0x1D
#define MIB_TX_SINGLE_COLLISION		0x1E
#define MIB_TX_MULTI_COLLISION		0x1F

#define MIB_RX_DROPS			0x20
#define MIB_TX_DROPS			0x21

static struct {
	char string[20];
} mib_names[TOTAL_SWITCH_COUNTER_NUM] = {
	{ "rx           " },
	{ "rx_hi        " },
	{ "rx_undersize" },
	{ "rx_fragments" },
	{ "rx_oversize" },
	{ "rx_jabbers" },
	{ "rx_symbol_err" },
	{ "rx_crc_err" },
	{ "rx_align_err" },
	{ "rx_mac_ctrl" },
	{ "rx_pause" },
	{ "rx_bcast" },
	{ "rx_mcast" },
	{ "rx_ucast" },
	{ "rx_64_or_less" },
	{ "rx_65_127" },
	{ "rx_128_255" },
	{ "rx_256_511" },
	{ "rx_512_1023" },
	{ "rx_1024_1522" },

	{ "tx           " },
	{ "tx_hi        " },
	{ "tx_late_col" },
	{ "tx_pause" },
	{ "tx_bcast" },
	{ "tx_mcast" },
	{ "tx_ucast" },
	{ "tx_deferred" },
	{ "tx_total_col" },
	{ "tx_exc_col" },
	{ "tx_single_col" },
	{ "tx_mult_col" },

	{ "rx_discards" },
	{ "tx_discards" },
};

static struct {
	int rx;
	int tx;
} mib_display[TOTAL_SWITCH_COUNTER_NUM / 2] = {
	{ MIB_RX_LO_PRIO, MIB_TX_LO_PRIO },
	{ MIB_RX_HI_PRIO, MIB_TX_HI_PRIO },
	{ MIB_RX_PAUSE, MIB_TX_PAUSE },
	{ MIB_RX_BROADCAST, MIB_TX_BROADCAST },
	{ MIB_RX_MULTICAST, MIB_TX_MULTICAST },
	{ MIB_RX_UNICAST, MIB_TX_UNICAST },
	{ MIB_RX_OCTET_64, MIB_RX_OCTET_65_127 },
	{ MIB_RX_OCTET_128_255, MIB_RX_OCTET_256_511 },
	{ MIB_RX_OCTET_512_1023, MIB_RX_OCTET_1024_1522 },
	{ MIB_RX_UNDERSIZE, MIB_RX_OVERSIZE },
	{ MIB_RX_FRAGMENT, MIB_RX_JABBER },
	{ MIB_RX_SYMBOL_ERR, MIB_RX_CRC_ERR },
	{ MIB_RX_ALIGNMENT_ERR, MIB_RX_CTRL_8808 },
	{ MIB_TX_LATE_COLLISION, MIB_TX_DEFERRED },
	{ MIB_TX_TOTAL_COLLISION, MIB_TX_EXCESS_COLLISION },
	{ MIB_TX_SINGLE_COLLISION, MIB_TX_MULTI_COLLISION },
	{ MIB_RX_DROPS, MIB_TX_DROPS },
};

static int display_sw_mib_counters(struct ksz_sw *sw, int first, int cnt,
	char *buf)
{
	int mib;
	int n;
	int len = 0;
	u64 counter[TOTAL_SWITCH_COUNTER_NUM];

	get_sw_mib_counters(sw, first, cnt, counter);
	for (mib = 0; mib < TOTAL_SWITCH_COUNTER_NUM / 2; mib++) {
		int rx = mib_display[mib].rx;
		int tx = mib_display[mib].tx;
		if (buf)
			len += sprintf(buf + len,
				"%s\t= %-20llu\t%s\t= %-20llu\n",
				mib_names[rx].string,
				counter[rx],
				mib_names[tx].string,
				counter[tx]);
		else
			printk(KERN_INFO "%s\t= %-20llu\t%s\t= %-20llu\n",
				mib_names[rx].string,
				counter[rx],
				mib_names[tx].string,
				counter[tx]);
	}
	for (n = 0, mib = first; n < cnt; n++, mib++) {
		int j;
		uint p = get_phy_port(sw, mib);
		struct ksz_port_mib *port_mib = get_port_mib(sw, p);

		for (j = 0; j < 2; j++) {
			if (port_mib->rate[j].peak) {
				u32 num;
				u32 frac;

				num = port_mib->rate[j].peak / 10;
				frac = port_mib->rate[j].peak % 10;
				if (buf)
					len += sprintf(buf + len,
						"%d:%d=%u.%u\n", mib, j,
						num, frac);
				else
					printk(KERN_INFO
						"%d:%d=%u.%u\n", mib, j,
						num, frac);
				port_mib->rate[j].peak = 0;
			}
		}
	}
	return len;
}  /* display_sw_mib_counters */

/* -------------------------------------------------------------------------- */

static ssize_t display_sw_info(int cnt, char *buf, ssize_t len)
{
	len += sprintf(buf + len, "duplex:\t\t");
	len += sprintf(buf + len, "0 for auto; ");
	len += sprintf(buf + len,
		"set to 1 for half-duplex; 2, full-duplex\n");
	len += sprintf(buf + len, "speed:\t\t");
	len += sprintf(buf + len,
		"0 for auto; set to 10 or 100\n");
	len += sprintf(buf + len, "force:\t\t");
	len += sprintf(buf + len,
		"set to 1 to force link to specific speed setting\n");
	len += sprintf(buf + len, "flow_ctrl:\t");
	len += sprintf(buf + len,
		"set to 0 to disable flow control\n");
	len += sprintf(buf + len, "mib:\t\t");
	len += sprintf(buf + len,
		"display the MIB table\n");
	printk(KERN_INFO "%s", buf);
	len = 0;

	if (TOTAL_PORT_NUM != cnt)
		return len;

	len += sprintf(buf + len, "\ndynamic_table:\t");
	len += sprintf(buf + len,
		"display the switch's dynamic MAC table\n");
	len += sprintf(buf + len, "static_table:\t");
	len += sprintf(buf + len,
		"display the switch's static MAC table\n");
	len += sprintf(buf + len, "vlan_table:\t");
	len += sprintf(buf + len,
		"display the switch's VLAN table\n");

	len += sprintf(buf + len, "\naging:\t\t");
	len += sprintf(buf + len,
		"disable/enable aging\n");
	len += sprintf(buf + len, "fast_aging:\t");
	len += sprintf(buf + len,
		"disable/enable fast aging\n");
	len += sprintf(buf + len, "link_aging:\t");
	len += sprintf(buf + len,
		"disable/enable link change auto aging\n");

	len += sprintf(buf + len, "\nbcast_per:\t");
	len += sprintf(buf + len,
		"set broadcast storm percentage\n");
	len += sprintf(buf + len, "mcast_storm:\t");
	len += sprintf(buf + len,
		"disable multicast storm protection\n");
	len += sprintf(buf + len, "diffserv_map:\t");
	len += sprintf(buf + len,
		"set DiffServ value.  Use \"decimal=hexadecimal\" format\n");
	len += sprintf(buf + len, "p_802_1p_map:\t");
	len += sprintf(buf + len,
		"set 802.1p value.  Use \"decimal=hexadecimal\" format\n");
	printk(KERN_INFO "%s", buf);
	len = 0;

	len += sprintf(buf + len, "\nvlan:\t\t");
	len += sprintf(buf + len,
		"disable/enable 802.1Q VLAN\n");
	len += sprintf(buf + len, "null_vid:\t");
	len += sprintf(buf + len,
		"set to 1 to replace null vid\n");
	len += sprintf(buf + len, "macaddr:\t");
	len += sprintf(buf + len,
		"set switch MAC address\n");
	len += sprintf(buf + len, "mirror_mode:\t");
	len += sprintf(buf + len,
		"set to 1 to use mirror rx AND tx mode\n");
	len += sprintf(buf + len, "tail_tag:\t");
	len += sprintf(buf + len,
		"disable/enable tail tagging\n");

	len += sprintf(buf + len, "\nigmp_snoop:\t");
	len += sprintf(buf + len,
		"disable/enable IGMP snooping\n");
	len += sprintf(buf + len, "ipv6_mld_snoop:\t");
	len += sprintf(buf + len,
		"disable/enable IPv6 MLD snooping\n");
	len += sprintf(buf + len, "ipv6_mld_option:");
	len += sprintf(buf + len,
		"disable/enable IPv6 MLD option snooping\n");

	len += sprintf(buf + len, "\naggr_backoff:\t");
	len += sprintf(buf + len,
		"disable/enable aggressive backoff in half-duplex mode\n");
	len += sprintf(buf + len, "no_exc_drop:\t");
	len += sprintf(buf + len,
		"disable/enable no excessive collision drop\n");
	len += sprintf(buf + len, "buf_reserve:\t");
	len += sprintf(buf + len,
		"disable/enable buffer reserve\n");
	printk(KERN_INFO "%s", buf);
	len = 0;

	len += sprintf(buf + len, "\nhuge_packet:\t");
	len += sprintf(buf + len,
		"disable/enable huge packet support\n");
	len += sprintf(buf + len, "legal_packet:\t");
	len += sprintf(buf + len,
		"disable/enable legal packet\n");
	len += sprintf(buf + len, "length_check:\t");
	len += sprintf(buf + len,
		"disable/enable legal packet length check\n");

	len += sprintf(buf + len, "\nback_pressure:\t");
	len += sprintf(buf + len,
		"set back pressure mode\n");
	len += sprintf(buf + len, "sw_flow_ctrl:\t");
	len += sprintf(buf + len,
		"disable/enable switch host port flow control\n");
	len += sprintf(buf + len, "sw_half_duplex:\t");
	len += sprintf(buf + len,
		"disable/enable switch host port half-duplex mode\n");
#ifdef SWITCH_10_MBIT
	len += sprintf(buf + len, "sw_10_mbit:\t");
	len += sprintf(buf + len,
		"disable/enable switch host port 10Mbit mode\n");
#endif
	len += sprintf(buf + len, "rx_flow_ctrl:\t");
	len += sprintf(buf + len,
		"disable/enable receive flow control\n");
	len += sprintf(buf + len, "tx_flow_ctrl:\t");
	len += sprintf(buf + len,
		"disable/enable transmit flow control\n");
	len += sprintf(buf + len, "fair_flow_ctrl:\t");
	len += sprintf(buf + len,
		"disable/enable fair flow control mode\n");
	printk(KERN_INFO "%s", buf);
	len = 0;

	len += sprintf(buf + len, "vlan_bound:\t");
	len += sprintf(buf + len,
		"disable/enable unicast VLAN boundary\n");
	len += sprintf(buf + len, "fw_unk_dest:\t");
	len += sprintf(buf + len,
		"disable/enable unknown destination address forwarding\n");

	len += sprintf(buf + len, "\nins_tag_0_1:\t");
	len += sprintf(buf + len,
		"set to 1 to insert tag from port 1 to 2\n");
	len += sprintf(buf + len, "ins_tag_0_2:\t");
	len += sprintf(buf + len,
		"set to 1 to insert tag from port 1 to 3\n");
	len += sprintf(buf + len, "ins_tag_1_0:\t");
	len += sprintf(buf + len,
		"set to 1 to insert tag from port 2 to 1\n");
	len += sprintf(buf + len, "ins_tag_1_2:\t");
	len += sprintf(buf + len,
		"set to 1 to insert tag from port 2 to 3\n");
	len += sprintf(buf + len, "ins_tag_2_0:\t");
	len += sprintf(buf + len,
		"set to 1 to insert tag from port 3 to 1\n");
	len += sprintf(buf + len, "ins_tag_2_1:\t");
	len += sprintf(buf + len,
		"set to 1 to insert tag from port 3 to 2\n");

	len += sprintf(buf + len, "\npass_all:\t");
	len += sprintf(buf + len,
		"set to 1 to pass all frames for debugging\n");
	len += sprintf(buf + len, "pass_pause:\t");
	len += sprintf(buf + len,
		"set to 1 to pass PAUSE frames for debugging\n");
	printk(KERN_INFO "%s", buf);
	len = 0;

	len += sprintf(buf + len, "\nswitch port settings:\n");
	len += sprintf(buf + len, "duplex:\t\t");
	len += sprintf(buf + len,
		"display the port's duplex setting\n");
	len += sprintf(buf + len, "speed:\t\t");
	len += sprintf(buf + len,
		"display the port's link speed\n");
	len += sprintf(buf + len, "linkmd:\t\t");
	len += sprintf(buf + len,
		"write to start LinkMD test.  read for result\n");
	len += sprintf(buf + len, "mib:\t\t");
	len += sprintf(buf + len,
		"display the port's MIB table\n");
	printk(KERN_INFO "%s", buf);
	len = 0;

	len += sprintf(buf + len, "vid:\t\t");
	len += sprintf(buf + len,
		"set default VID value\n");
	len += sprintf(buf + len, "member:\t\t");
	len += sprintf(buf + len,
		"set VLAN membership\n");

	len += sprintf(buf + len, "bcast_storm:\t");
	len += sprintf(buf + len,
		"disable/enable broadcast storm protection\n");
	len += sprintf(buf + len, "diffserv:\t");
	len += sprintf(buf + len,
		"disable/enable DiffServ priority\n");
	len += sprintf(buf + len, "p_802_1p:\t");
	len += sprintf(buf + len,
		"disable/enable 802.1p priority\n");

	len += sprintf(buf + len, "port_based:\t");
	len += sprintf(buf + len,
		"disable/enable port-based priority\n");
	printk(KERN_INFO "%s", buf);
	len = 0;

	len += sprintf(buf + len, "prio_queue:\t");
	len += sprintf(buf + len,
		"disable/enable priority queue\n");
	len += sprintf(buf + len, "tx_p0_ctrl:\t");
	len += sprintf(buf + len,
		"set priority queue 0 control\n");
	len += sprintf(buf + len, "tx_p1_ctrl:\t");
	len += sprintf(buf + len,
		"set priority queue 1 control\n");
	len += sprintf(buf + len, "tx_p2_ctrl:\t");
	len += sprintf(buf + len,
		"set priority queue 2 control\n");
	len += sprintf(buf + len, "tx_p3_ctrl:\t");
	len += sprintf(buf + len,
		"set priority queue 3 control\n");
	len += sprintf(buf + len, "tx_p0_ratio:\t");
	len += sprintf(buf + len,
		"set priority queue 0 ratio\n");
	len += sprintf(buf + len, "tx_p1_ratio:\t");
	len += sprintf(buf + len,
		"set priority queue 1 ratio\n");
	len += sprintf(buf + len, "tx_p2_ratio:\t");
	len += sprintf(buf + len,
		"set priority queue 2 ratio\n");
	len += sprintf(buf + len, "tx_p3_ratio:\t");
	len += sprintf(buf + len,
		"set priority queue 3 ratio\n");
	len += sprintf(buf + len, "prio_rate:\t");
	len += sprintf(buf + len,
		"disable/enable priority queue rate limiting\n");
	len += sprintf(buf + len, "rx_limit:\t");
	len += sprintf(buf + len,
		"set rx rate limiting mode\n");
	len += sprintf(buf + len, "cnt_ifg:\t");
	len += sprintf(buf + len,
		"set to 1 to count IPG\n");
	len += sprintf(buf + len, "cnt_pre:\t");
	len += sprintf(buf + len,
		"set to 1 to count preamble\n");
	len += sprintf(buf + len, "rx_p0_rate:\t");
	len += sprintf(buf + len,
		"set rx priority queue 0 rate in 64Kbps unit\n");
	len += sprintf(buf + len, "rx_p1_rate:\t");
	len += sprintf(buf + len,
		"set rx priority queue 1 rate in 64Kbps unit\n");
	len += sprintf(buf + len, "rx_p2_rate:\t");
	len += sprintf(buf + len,
		"set rx priority queue 2 rate in 64Kbps unit\n");
	len += sprintf(buf + len, "rx_p3_rate:\t");
	len += sprintf(buf + len,
		"set rx priority queue 3 rate in 64Kbps unit\n");
	len += sprintf(buf + len, "tx_p0_rate:\t");
	len += sprintf(buf + len,
		"set tx priority queue 0 rate in 64Kbps unit\n");
	len += sprintf(buf + len, "tx_p1_rate:\t");
	len += sprintf(buf + len,
		"set tx priority queue 1 rate in 64Kbps unit\n");
	len += sprintf(buf + len, "tx_p2_rate:\t");
	len += sprintf(buf + len,
		"set tx priority queue 2 rate in 64Kbps unit\n");
	len += sprintf(buf + len, "tx_p3_rate:\t");
	len += sprintf(buf + len,
		"set tx priority queue 3 rate in 64Kbps unit\n");
	printk(KERN_INFO "%s", buf);
	len = 0;

	len += sprintf(buf + len, "rx:\t\t");
	len += sprintf(buf + len,
		"disable/enable rx\n");
	len += sprintf(buf + len, "tx:\t\t");
	len += sprintf(buf + len,
		"disable/enable tx\n");
	len += sprintf(buf + len, "learn:\t\t");
	len += sprintf(buf + len,
		"disable/enable learning\n");

	len += sprintf(buf + len, "mirror_port:\t");
	len += sprintf(buf + len,
		"disable/enable mirror port\n");
	len += sprintf(buf + len, "mirror_rx:\t");
	len += sprintf(buf + len,
		"disable/enable mirror receive\n");
	len += sprintf(buf + len, "mirror_tx:\t");
	len += sprintf(buf + len,
		"disable/enable mirror transmit\n");
	printk(KERN_INFO "%s", buf);
	len = 0;

	len += sprintf(buf + len, "\nnon_vid:\t");
	len += sprintf(buf + len,
		"set to 1 to discard non-VID packets\n");
	len += sprintf(buf + len, "ingress:\t");
	len += sprintf(buf + len,
		"disable/enable ingress VLAN filtering\n");
	len += sprintf(buf + len, "ins_tag:\t");
	len += sprintf(buf + len,
		"disable/enable insert VLAN tag feature\n");
	len += sprintf(buf + len, "rmv_tag:\t");
	len += sprintf(buf + len,
		"disable/enable remove VLAN tag feature\n");
	len += sprintf(buf + len, "drop_tagged:\t");
	len += sprintf(buf + len,
		"disable/enable drop tagged packet feature\n");
	len += sprintf(buf + len, "replace_prio:\t");
	len += sprintf(buf + len,
		"disable/enable replace 802.1p priority feature\n");
	len += sprintf(buf + len, "back_pressure:\t");
	len += sprintf(buf + len,
		"disable/enable back pressure in half-duplex mode\n");
	len += sprintf(buf + len, "force_flow_ctrl:");
	len += sprintf(buf + len,
		"set to 1 to force flow control\n");
	len += sprintf(buf + len, "fw_unk_dest:\t");
	len += sprintf(buf + len,
		"set to 1 to forward unknown destination address packets\n");
	len += sprintf(buf + len, "fw_inv_vid:\t");
	len += sprintf(buf + len,
		"set to 1 to forward invalid VID packets\n");
	len += sprintf(buf + len, "\nmacaddr:\t");
	len += sprintf(buf + len,
		"set port MAC address\n");
	len += sprintf(buf + len, "src_filter_0:\t");
	len += sprintf(buf + len,
		"disable/enable source filtering port 1 MAC address\n");
	len += sprintf(buf + len, "src_filter_1:\t");
	len += sprintf(buf + len,
		"disable/enable source filtering port 2 MAC address\n");
	printk(KERN_INFO "%s", buf);
	len = 0;

	len += sprintf(buf + len, "\nstatic MAC table:\n");
	len += sprintf(buf + len, "addr:\t\t");
	len += sprintf(buf + len,
		"set MAC address\n");
	len += sprintf(buf + len, "ports:\t\t");
	len += sprintf(buf + len,
		"set destination ports\n");
	len += sprintf(buf + len, "override:\t");
	len += sprintf(buf + len,
		"set override bit\n");
	len += sprintf(buf + len, "use_fid:\t");
	len += sprintf(buf + len,
		"set use FID bit\n");
	len += sprintf(buf + len, "fid:\t\t");
	len += sprintf(buf + len,
		"set FID\n");
	len += sprintf(buf + len, "valid:\t\t");
	len += sprintf(buf + len,
		"set valid bit and write to table\n");
	len += sprintf(buf + len, "\nVLAN table:\n");
	len += sprintf(buf + len, "vid:\t\t");
	len += sprintf(buf + len,
		"set VID\n");
	len += sprintf(buf + len, "fid:\t\t");
	len += sprintf(buf + len,
		"set FID\n");
	len += sprintf(buf + len, "member:\t\t");
	len += sprintf(buf + len,
		"set membership\n");
	len += sprintf(buf + len, "valid:\t\t");
	len += sprintf(buf + len,
		"set valid bit and write to table\n");
	printk(KERN_INFO "%s", buf);
	len = 0;

	return len;
}

static ssize_t sysfs_sw_read(struct ksz_sw *sw, int proc_num,
	struct ksz_port *port, ssize_t len, char *buf)
{
	int i;
	int j;
	u16 map;
	struct ksz_sw_info *info = sw->info;

	switch (proc_num) {
	case PROC_SW_INFO:
		len = display_sw_info(TOTAL_PORT_NUM, buf, len);
		break;
	case PROC_SW_VERSION:
		len += sprintf(buf + len, "%s  %s\n",
			SW_DRV_VERSION, SW_DRV_RELDATE);
		break;
	case PROC_SET_SW_DUPLEX:
		if (!port)
			break;
		len += sprintf(buf + len, "%u; ", port->duplex);
		if (media_connected == port->linked->state) {
			if (1 == port->linked->duplex)
				len += sprintf(buf + len, "half-duplex\n");
			else if (2 == port->linked->duplex)
				len += sprintf(buf + len, "full-duplex\n");
		} else
			len += sprintf(buf + len, "unlinked\n");
		break;
	case PROC_SET_SW_SPEED:
		if (!port)
			break;
		len += sprintf(buf + len, "%u; ", port->speed);
		if (media_connected == port->linked->state)
			len += sprintf(buf + len, "%u\n",
				port->linked->tx_rate / TX_RATE_UNIT);
		else
			len += sprintf(buf + len, "unlinked\n");
		break;
	case PROC_SET_SW_FORCE:
		if (!port)
			break;
		len += sprintf(buf + len, "%u\n", port->force_link);
		break;
	case PROC_SET_SW_FLOW_CTRL:
		if (!port)
			break;
		len += sprintf(buf + len, "%u; ", port->flow_ctrl);
		switch (port->flow_ctrl) {
		case PHY_FLOW_CTRL:
			len += sprintf(buf + len, "flow control\n");
			break;
		case PHY_TX_ONLY:
			len += sprintf(buf + len, "tx only\n");
			break;
		case PHY_RX_ONLY:
			len += sprintf(buf + len, "rx only\n");
			break;
		default:
			len += sprintf(buf + len, "no flow control\n");
			break;
		}
		break;
	case PROC_SET_SW_MIB:
		if (!port)
			break;
		len += display_sw_mib_counters(sw, port->first_port,
			port->mib_port_cnt, buf + len);
		break;
	case PROC_SET_BROADCAST_STORM:
		len += sprintf(buf + len, "%u%%\n", info->broad_per);
		break;
	case PROC_SET_DIFFSERV:
		for (i = 0; i < DIFFSERV_ENTRIES / KS_PRIO_IN_REG; i++) {
			len += sprintf(buf + len, "%2u=",
				i * KS_PRIO_IN_REG);
			map = info->diffserv[i];
			for (j = 0; j < KS_PRIO_IN_REG; j++) {
				len += sprintf(buf + len, "%u ",
					map & KS_PRIO_M);
				map >>= KS_PRIO_S;
			}
			len += sprintf(buf + len, "\t"SW_SIZE_STR"\n",
				info->diffserv[i]);
		}
		break;
	case PROC_SET_802_1P:
		for (i = 0; i < PRIO_802_1P_ENTRIES / KS_PRIO_IN_REG; i++) {
			len += sprintf(buf + len, "%u=",
				i * KS_PRIO_IN_REG);
			map = info->p_802_1p[i];
			for (j = 0; j < KS_PRIO_IN_REG; j++) {
				len += sprintf(buf + len, "%u ",
					map & KS_PRIO_M);
				map >>= KS_PRIO_S;
			}
			len += sprintf(buf + len, "\t"SW_SIZE_STR"\n",
				info->p_802_1p[i]);
		}
		break;
#ifdef SWITCH_PORT_PHY_ADDR_MASK
	case PROC_SET_PHY_ADDR:
		len += sprintf(buf + len, "%u\n",
			info->phy_addr);
		break;
#endif
	case PROC_SET_SW_VID:
		len += sprintf(buf + len, "0x%04x\n", sw->vid);
		break;
	case PROC_GET_PORTS:
	{
		uint ports = sw->mib_port_cnt;

		if (sw->eth_cnt)
			ports = sw->eth_maps[0].cnt;
		len += sprintf(buf + len, "%u\n", ports);
		break;
	}
	case PROC_GET_DEV_START:
	{
		int start = 0;

		if (sw->dev_offset)
			start = 100;
		len += sprintf(buf + len, "%u\n", start);
		break;
	}
	case PROC_GET_VLAN_START:
	{
		int start = 0;

		if (sw->features & VLAN_PORT)
			start = VLAN_PORT_START;
		len += sprintf(buf + len, "%u\n", start);
		break;
	}
	case PROC_GET_STP:
		len += sprintf(buf + len, "0\n");
		break;
	case PROC_SET_SW_FEATURES:
		len += sprintf(buf + len, "%08x:\n", sw->features);
		len += sprintf(buf + len, "\t%08x = STP support\n",
			STP_SUPPORT);
		len += sprintf(buf + len, "\t%08x = VLAN port forwarding\n",
			VLAN_PORT);
		len += sprintf(buf + len, "\t%08x = VLAN port remove tag\n",
			VLAN_PORT_REMOVE_TAG);
		len += sprintf(buf + len, "\t%08x = VLAN port tag tailing\n",
			VLAN_PORT_TAGGING);
#ifdef CONFIG_KSZ_DLR
		len += sprintf(buf + len, "\t%08x = DLR support\n",
			DLR_HW);
#endif
#ifdef CONFIG_KSZ_HSR
		len += sprintf(buf + len, "\t%08x = HSR support\n",
			HSR_HW);
#endif
		len += sprintf(buf + len, "\t%08x = different MAC addresses\n",
			DIFF_MAC_ADDR);
#ifdef CONFIG_1588_PTP
		len += sprintf(buf + len, "\t%08x = 1588 PTP\n",
			PTP_HW);
#endif
		break;
	case PROC_SET_SW_OVERRIDES:
		len += sprintf(buf + len, "%08x:\n", sw->overrides);
		len += sprintf(buf + len, "\t%08x = flow control\n",
			PAUSE_FLOW_CTRL);
		len += sprintf(buf + len, "\t%08x = fast aging\n",
			FAST_AGING);
		len += sprintf(buf + len, "\t%08x = tag is removed\n",
			TAG_REMOVE);
		len += sprintf(buf + len, "\t%08x = tail tagging\n",
			TAIL_TAGGING);
		break;
	}
	return len;
}

static ssize_t sysfs_sw_read_hw(struct ksz_sw *sw, int proc_num, ssize_t len,
	char *buf)
{
	u8 data[8];
	int chk = 0;
	int type = SHOW_HELP_ON_OFF;
	char note[40];

	note[0] = '\0';
	switch (proc_num) {
	case PROC_SET_AGING:
		chk = sw_chk(sw, REG_SWITCH_CTRL_1, SWITCH_AGING_ENABLE);
		break;
	case PROC_SET_FAST_AGING:
		chk = sw_chk(sw, REG_SWITCH_CTRL_1, SWITCH_FAST_AGING);
		break;
	case PROC_SET_LINK_AGING:
		chk = sw_chk(sw, S_LINK_AGING_CTRL, SWITCH_LINK_AUTO_AGING);
		break;
	case PROC_SET_MULTICAST_STORM:
		chk = !sw_chk(sw, REG_SWITCH_CTRL_2, MULTICAST_STORM_DISABLE);
		break;
	case PROC_ENABLE_VLAN:
		chk = sw_chk(sw, S_MIRROR_CTRL, SWITCH_VLAN_ENABLE);
		break;
	case PROC_SET_REPLACE_NULL_VID:
		chk = sw_chk(sw, S_REPLACE_VID_CTRL, SWITCH_REPLACE_VID);
		break;
	case PROC_SET_MAC_ADDR:
		sw_get_addr(sw, data);
		len += sprintf(buf + len, "%02X:%02X:%02X:%02X:%02X:%02X\n",
			data[0], data[1], data[2], data[3], data[4], data[5]);
		type = SHOW_HELP_NONE;
		break;
	case PROC_SET_MIRROR_MODE:
		chk = sw_chk_mirror_rx_tx(sw);
		if (sw->verbose) {
			if (chk)
				strcpy(note, " (rx and tx)");
			else
				strcpy(note, " (rx or tx)");
		}
		type = SHOW_HELP_SPECIAL;
		break;
	case PROC_SET_IGMP_SNOOP:
		chk = sw_chk(sw, S_MIRROR_CTRL, SWITCH_IGMP_SNOOP);
		break;
#ifdef SWITCH_IPV6_MLD_SNOOP
	case PROC_SET_IPV6_MLD_SNOOP:
		chk = sw_chk(sw, S_MIRROR_CTRL, SWITCH_IPV6_MLD_SNOOP);
		break;
	case PROC_SET_IPV6_MLD_OPTION:
		chk = sw_chk(sw, S_MIRROR_CTRL, SWITCH_IPV6_MLD_OPTION);
		break;
#endif
	case PROC_SET_TAIL_TAG:
		chk = sw_chk(sw, S_TAIL_TAG_CTRL, SWITCH_TAIL_TAG_ENABLE);
		break;
	case PROC_SET_AGGR_BACKOFF:
		chk = sw_chk(sw, REG_SWITCH_CTRL_1, SWITCH_AGGR_BACKOFF);
		break;
	case PROC_SET_NO_EXC_DROP:
		chk = sw_chk(sw, REG_SWITCH_CTRL_2, NO_EXC_COLLISION_DROP);
		break;
#ifdef SWITCH_BUF_RESERVE
	case PROC_SET_BUFFER_RESERVE:
		chk = sw_chk(sw, REG_SWITCH_CTRL_2, SWITCH_BUF_RESERVE);
		break;
#endif
	case PROC_SET_VLAN_BOUNDARY:
		chk = sw_chk(sw, REG_SWITCH_CTRL_2, UNICAST_VLAN_BOUNDARY);
		break;
	case PROC_SET_HUGE_PACKET:
		chk = sw_chk(sw, REG_SWITCH_CTRL_2, SWITCH_HUGE_PACKET);
		break;
	case PROC_SET_LEGAL_PACKET:
		chk = sw_chk(sw, REG_SWITCH_CTRL_2, SWITCH_LEGAL_PACKET);
		break;
	case PROC_SET_LENGTH_CHECK:
		chk = sw_chk(sw, REG_SWITCH_CTRL_1, SWITCH_CHECK_LENGTH);
		break;
	case PROC_SET_BACK_PRESSURE_MODE:
		chk = sw_chk(sw, REG_SWITCH_CTRL_2, SWITCH_BACK_PRESSURE);
		break;
	case PROC_SET_SWITCH_FLOW_CTRL:
		chk = sw_chk(sw, S_REPLACE_VID_CTRL, SWITCH_FLOW_CTRL);
		break;
	case PROC_SET_SWITCH_HALF_DUPLEX:
		chk = sw_chk(sw, S_REPLACE_VID_CTRL, SWITCH_HALF_DUPLEX);
		break;
#ifdef SWITCH_10_MBIT
	case PROC_SET_SWITCH_10_MBIT:
		chk = sw_chk(sw, S_REPLACE_VID_CTRL, SWITCH_10_MBIT);
		break;
#endif
	case PROC_SET_RX_FLOW_CTRL:
		chk = sw_chk(sw, REG_SWITCH_CTRL_1, SWITCH_RX_FLOW_CTRL);
		break;
	case PROC_SET_TX_FLOW_CTRL:
		chk = sw_chk(sw, REG_SWITCH_CTRL_1, SWITCH_TX_FLOW_CTRL);
		break;
	case PROC_SET_FAIR_FLOW_CTRL:
		chk = sw_chk(sw, REG_SWITCH_CTRL_2, FAIR_FLOW_CTRL);
		break;
	case PROC_SET_FORWARD_UNKNOWN_DEST:
		chk = sw_chk_unk_dest(sw);
		break;
	case PROC_SET_INS_TAG_0_1:
		chk = sw_chk(sw, S_INS_SRC_PVID_CTRL, SWITCH_INS_TAG_1_PORT_2);
		break;
	case PROC_SET_INS_TAG_0_2:
		chk = sw_chk(sw, S_INS_SRC_PVID_CTRL, SWITCH_INS_TAG_1_PORT_3);
		break;
	case PROC_SET_INS_TAG_1_0:
		chk = sw_chk(sw, S_INS_SRC_PVID_CTRL, SWITCH_INS_TAG_2_PORT_1);
		break;
	case PROC_SET_INS_TAG_1_2:
		chk = sw_chk(sw, S_INS_SRC_PVID_CTRL, SWITCH_INS_TAG_2_PORT_3);
		break;
	case PROC_SET_INS_TAG_2_0:
		chk = sw_chk(sw, S_INS_SRC_PVID_CTRL, SWITCH_INS_TAG_3_PORT_1);
		break;
	case PROC_SET_INS_TAG_2_1:
		chk = sw_chk(sw, S_INS_SRC_PVID_CTRL, SWITCH_INS_TAG_3_PORT_2);
		break;
	case PROC_SET_PASS_ALL:
		chk = sw_chk(sw, REG_SWITCH_CTRL_1, SWITCH_PASS_ALL);
		break;
	case PROC_SET_PASS_PAUSE:
		chk = sw_chk(sw, S_LINK_AGING_CTRL, SWITCH_PASS_PAUSE);
		break;
	case PROC_DYNAMIC:
		len = sw_d_dyn_mac_table(sw, buf, len);
		type = SHOW_HELP_NONE;
		break;
	case PROC_STATIC:
		len = sw_d_sta_mac_table(sw, buf, len);
		len = sw_d_mac_table(sw, buf, len);
		type = SHOW_HELP_NONE;
		break;
	case PROC_VLAN:
		len = sw_d_vlan_table(sw, buf, len);
		type = SHOW_HELP_NONE;
		break;
	default:
		type = SHOW_HELP_NONE;
		break;
	}
	return sysfs_show(len, buf, type, chk, note, sw->verbose);
}

static int sysfs_sw_write(struct ksz_sw *sw, int proc_num,
	struct ksz_port *port, int num, const char *buf)
{
	int changes;
	int count;
	unsigned int val;
	u8 data[8];
	int processed = true;

	switch (proc_num) {
	case PROC_SW_INFO:
		sw_init(sw);
		sw->verbose = !!num;
		break;
	case PROC_SET_SW_DUPLEX:
		if (!port)
			break;
		if (num <= 2)
			port->duplex = (u8) num;
		break;
	case PROC_SET_SW_SPEED:
		if (!port)
			break;
		if (0 == num || 10 == num || 100 == num)
			port->speed = (u8) num;
		break;
	case PROC_SET_SW_FORCE:
		if (!port)
			break;
		port->force_link = (u8) num;
		if (port->force_link)
			port_force_link_speed(port);
		else
			port_set_link_speed(port);
		break;
	case PROC_SET_SW_FLOW_CTRL:
		if (!port)
			break;
		if (num <= PHY_FLOW_CTRL)
			port->flow_ctrl = (u8) num;
		break;
	case PROC_SET_SW_MIB:
		for (count = 0; count < sw->port_cnt; count++) {
			struct ksz_port_mib *mib = get_port_mib(sw, count);

			memset((void *) mib->counter, 0, sizeof(u64) *
				TOTAL_SWITCH_COUNTER_NUM);
		}
		break;
	case PROC_SET_SW_REG:
		count = sscanf(buf, "%x=%x", (unsigned int *) &num, &val);
		if (1 == count)
			printk(KERN_INFO SW_SIZE_STR"\n",
				SW_R(sw, num));
		else if (2 == count)
			SW_W(sw, num, val);
		break;
	case PROC_SET_SW_VID:
		sw->vid = num;
		break;
	case PROC_SET_SW_FEATURES:
		if ('0' != buf[0] || 'x' != buf[1])
			sscanf(buf, "%x", &num);
		changes = sw->features ^ num;
		sw->features = num;
		if (changes & VLAN_PORT_REMOVE_TAG) {
			int i;
			bool enabled;

			if (num & VLAN_PORT_REMOVE_TAG) {
				enabled = true;
				sw->overrides |= TAG_REMOVE;
			} else {
				enabled = false;
				sw->overrides &= ~TAG_REMOVE;
			}
			for (i = 0; i < SWITCH_PORT_NUM; i++)
				port_cfg_rmv_tag(sw, i, enabled);
		}
		break;
	case PROC_SET_SW_OVERRIDES:
		if ('0' != buf[0] || 'x' != buf[1])
			sscanf(buf, "%x", &num);
		sw->overrides = num;
		break;
	case PROC_DYNAMIC:
		sw_flush_dyn_mac_table(sw, TOTAL_PORT_NUM);
		break;
	case PROC_STATIC:
		sw_clr_sta_mac_table(sw);
		break;
	case PROC_SET_AGING:
		sw_cfg(sw, REG_SWITCH_CTRL_1, SWITCH_AGING_ENABLE, num);
		break;
	case PROC_SET_FAST_AGING:
		sw_cfg(sw, REG_SWITCH_CTRL_1, SWITCH_FAST_AGING, num);
		break;
	case PROC_SET_LINK_AGING:
		sw_cfg(sw, S_LINK_AGING_CTRL, SWITCH_LINK_AUTO_AGING, num);
		break;
	case PROC_SET_BROADCAST_STORM:
		hw_cfg_broad_storm(sw, num);
		break;
	case PROC_SET_MULTICAST_STORM:
		sw_cfg(sw, REG_SWITCH_CTRL_2, MULTICAST_STORM_DISABLE, !num);
		break;
	case PROC_SET_DIFFSERV:
		count = sscanf(buf, "%d=%x", (unsigned int *) &num, &val);
		if (2 == count)
			hw_cfg_tos_prio(sw, (u8) num, (u16) val);
		break;
	case PROC_SET_802_1P:
		count = sscanf(buf, "%d=%x", (unsigned int *) &num, &val);
		if (2 == count)
			hw_cfg_802_1p_prio(sw, (u8) num, (u16) val);
		break;
	case PROC_ENABLE_VLAN:
		if (!num)
			sw_dis_vlan(sw);
		else
			sw_ena_vlan(sw);
		break;
	case PROC_SET_REPLACE_NULL_VID:
		sw_cfg_replace_null_vid(sw, num);
		break;
	case PROC_SET_MAC_ADDR:
	{
		int i;
		int n[6];

		i = sscanf(buf, "%x:%x:%x:%x:%x:%x",
			&n[0], &n[1], &n[2], &n[3], &n[4], &n[5]);
		if (6 == i) {
			for (i = 0; i < 6; i++)
				data[i] = (u8) n[i];
			sw_set_addr(sw, data);
		}
		break;
	}
	case PROC_SET_MIRROR_MODE:
		sw_cfg_mirror_rx_tx(sw, num);
		break;
	case PROC_SET_IGMP_SNOOP:
		sw_cfg(sw, S_MIRROR_CTRL, SWITCH_IGMP_SNOOP, num);
		break;
#ifdef SWITCH_IPV6_MLD_SNOOP
	case PROC_SET_IPV6_MLD_SNOOP:
		sw_cfg(sw, S_MIRROR_CTRL, SWITCH_IPV6_MLD_SNOOP, num);
		break;
	case PROC_SET_IPV6_MLD_OPTION:
		sw_cfg(sw, S_MIRROR_CTRL, SWITCH_IPV6_MLD_OPTION, num);
		break;
#endif
	case PROC_SET_TAIL_TAG:
		sw_cfg(sw, S_TAIL_TAG_CTRL, SWITCH_TAIL_TAG_ENABLE, num);
		if (num)
			sw->overrides |= TAIL_TAGGING;
		else
			sw->overrides &= ~TAIL_TAGGING;
		break;
	case PROC_SET_AGGR_BACKOFF:
		sw_cfg(sw, REG_SWITCH_CTRL_1, SWITCH_AGGR_BACKOFF, num);
		break;
	case PROC_SET_NO_EXC_DROP:
		sw_cfg(sw, REG_SWITCH_CTRL_2, NO_EXC_COLLISION_DROP, num);
		break;
#ifdef SWITCH_BUF_RESERVE
	case PROC_SET_BUFFER_RESERVE:
		sw_cfg(sw, REG_SWITCH_CTRL_2, SWITCH_BUF_RESERVE, num);
		break;
#endif
	case PROC_SET_VLAN_BOUNDARY:
		sw_cfg(sw, REG_SWITCH_CTRL_2, UNICAST_VLAN_BOUNDARY, num);
		break;
	case PROC_SET_HUGE_PACKET:
		sw_cfg(sw, REG_SWITCH_CTRL_2, SWITCH_HUGE_PACKET, num);
		break;
	case PROC_SET_LEGAL_PACKET:
		sw_cfg(sw, REG_SWITCH_CTRL_2, SWITCH_LEGAL_PACKET, num);
		break;
	case PROC_SET_LENGTH_CHECK:
		sw_cfg(sw, REG_SWITCH_CTRL_1, SWITCH_CHECK_LENGTH, num);
		break;
	case PROC_SET_BACK_PRESSURE_MODE:
		sw_cfg(sw, REG_SWITCH_CTRL_2, SWITCH_BACK_PRESSURE, num);
		break;
	case PROC_SET_SWITCH_FLOW_CTRL:
		sw_cfg(sw, S_REPLACE_VID_CTRL, SWITCH_FLOW_CTRL, num);
		break;
	case PROC_SET_SWITCH_HALF_DUPLEX:
		sw_cfg(sw, S_REPLACE_VID_CTRL, SWITCH_HALF_DUPLEX, num);
		break;
#ifdef SWITCH_10_MBIT
	case PROC_SET_SWITCH_10_MBIT:
		sw_cfg(sw, S_REPLACE_VID_CTRL, SWITCH_10_MBIT, num);
		break;
#endif
	case PROC_SET_RX_FLOW_CTRL:
		sw_cfg(sw, REG_SWITCH_CTRL_1, SWITCH_RX_FLOW_CTRL, num);
		break;
	case PROC_SET_TX_FLOW_CTRL:
		sw_cfg(sw, REG_SWITCH_CTRL_1, SWITCH_TX_FLOW_CTRL, num);
		break;
	case PROC_SET_FAIR_FLOW_CTRL:
		sw_cfg(sw, REG_SWITCH_CTRL_2, FAIR_FLOW_CTRL, num);
		break;
	case PROC_SET_FORWARD_UNKNOWN_DEST:
		sw_cfg_unk_dest(sw, num);
		break;
	case PROC_SET_INS_TAG_0_1:
		sw_cfg(sw, S_INS_SRC_PVID_CTRL, SWITCH_INS_TAG_1_PORT_2, num);
		break;
	case PROC_SET_INS_TAG_0_2:
		sw_cfg(sw, S_INS_SRC_PVID_CTRL, SWITCH_INS_TAG_1_PORT_3, num);
		break;
	case PROC_SET_INS_TAG_1_0:
		sw_cfg(sw, S_INS_SRC_PVID_CTRL, SWITCH_INS_TAG_2_PORT_1, num);
		break;
	case PROC_SET_INS_TAG_1_2:
		sw_cfg(sw, S_INS_SRC_PVID_CTRL, SWITCH_INS_TAG_2_PORT_3, num);
		break;
	case PROC_SET_INS_TAG_2_0:
		sw_cfg(sw, S_INS_SRC_PVID_CTRL, SWITCH_INS_TAG_3_PORT_1, num);
		break;
	case PROC_SET_INS_TAG_2_1:
		sw_cfg(sw, S_INS_SRC_PVID_CTRL, SWITCH_INS_TAG_3_PORT_2, num);
		break;
	case PROC_SET_PASS_ALL:
		sw_cfg(sw, REG_SWITCH_CTRL_1, SWITCH_PASS_ALL, num);
		break;
	case PROC_SET_PASS_PAUSE:
		sw_cfg(sw, S_LINK_AGING_CTRL, SWITCH_PASS_PAUSE, num);
		break;
#ifdef SWITCH_PORT_PHY_ADDR_MASK
	case PROC_SET_PHY_ADDR:
		sw_set_phy_addr(sw, num);
		break;
#endif
	default:
		processed = false;
		break;
	}
	return processed;
}

static ssize_t sysfs_port_read(struct ksz_sw *sw, int proc_num, uint port,
	ssize_t len, char *buf)
{
	struct ksz_port_cfg *port_cfg;
	struct ksz_port_info *port_info;
	int chk = 0;
	int type = SHOW_HELP_NONE;
	char note[40];

	note[0] = '\0';
	port = get_sysfs_port(sw, port);
	port_cfg = get_port_cfg(sw, port);
	port_info = get_port_info(sw, port);
	switch (proc_num) {
	case PROC_SET_PORT_DUPLEX:
		if (media_connected == port_info->state) {
			if (1 == port_info->duplex)
				len += sprintf(buf + len, "half-duplex\n");
			else if (2 == port_info->duplex)
				len += sprintf(buf + len, "full-duplex\n");
		} else
			len += sprintf(buf + len, "unlinked\n");
		break;
	case PROC_SET_PORT_SPEED:
		if (media_connected == port_info->state)
			len += sprintf(buf + len, "%u\n",
				port_info->tx_rate / TX_RATE_UNIT);
		else
			len += sprintf(buf + len, "unlinked\n");
		break;
	case PROC_SET_PORT_MIB:
		port = get_log_port(sw, port);
		len += display_sw_mib_counters(sw, port, 1, buf + len);
		break;
	case PROC_SET_LINK_MD:
		len += sprintf(buf + len, "%u:%u %u:%u %u:%u\n",
			port_info->length[0], port_info->status[0],
			port_info->length[1], port_info->status[1],
			port_info->length[2], port_info->status[2]);
		if (sw->verbose)
			len += sprintf(buf + len,
				"(%d=unknown; %d=normal; %d=open; %d=short)\n",
				CABLE_UNKNOWN, CABLE_GOOD, CABLE_OPEN,
				CABLE_SHORT);
		break;
	case PROC_SET_PORT_BASED:
		chk = port_cfg->port_prio;
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_DEF_VID:
		chk = port_cfg->vid;
		type = SHOW_HELP_HEX_4;
		break;
	case PROC_SET_MEMBER:
		chk = port_cfg->member;
		type = SHOW_HELP_HEX_2;
		break;
	case PROC_SET_TX_P0_CTRL:
		chk = (port_cfg->rate_ctrl[0] & RATE_CTRL_ENABLE) != 0;
		type = SHOW_HELP_ON_OFF;
		break;
	case PROC_SET_TX_P1_CTRL:
		chk = (port_cfg->rate_ctrl[1] & RATE_CTRL_ENABLE) != 0;
		type = SHOW_HELP_ON_OFF;
		break;
	case PROC_SET_TX_P2_CTRL:
		chk = (port_cfg->rate_ctrl[2] & RATE_CTRL_ENABLE) != 0;
		type = SHOW_HELP_ON_OFF;
		break;
	case PROC_SET_TX_P3_CTRL:
		chk = (port_cfg->rate_ctrl[3] & RATE_CTRL_ENABLE) != 0;
		type = SHOW_HELP_ON_OFF;
		break;
#ifdef RATE_RATIO_MASK
	case PROC_SET_TX_P0_RATIO:
		chk = port_cfg->rate_ctrl[0] & RATE_RATIO_MASK;
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_TX_P1_RATIO:
		chk = port_cfg->rate_ctrl[1] & RATE_RATIO_MASK;
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_TX_P2_RATIO:
		chk = port_cfg->rate_ctrl[2] & RATE_RATIO_MASK;
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_TX_P3_RATIO:
		chk = port_cfg->rate_ctrl[3] & RATE_RATIO_MASK;
		type = SHOW_HELP_NUM;
		break;
#endif
	case PROC_SET_RX_LIMIT:
		chk = (port_cfg->rate_limit >> 2) & 3;
		if (sw->verbose) {
			switch (chk) {
			case 1:
				strcpy(note, " (flooded unicast)");
				break;
			case 2:
				strcpy(note, " (multicast)");
				break;
			case 3:
				strcpy(note, " (broadcast)");
				break;
			default:
				strcpy(note, " (all)");
			}
		}
		type = SHOW_HELP_SPECIAL;
		break;
	case PROC_SET_CNT_IFG:
		chk = (port_cfg->rate_limit & 2) != 0;
		type = SHOW_HELP_ON_OFF;
		break;
	case PROC_SET_CNT_PRE:
		chk = (port_cfg->rate_limit & 1) != 0;
		type = SHOW_HELP_ON_OFF;
		break;
	case PROC_SET_RX_P0_RATE:
		chk = port_cfg->rx_rate[0];
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_RX_P1_RATE:
		chk = port_cfg->rx_rate[1];
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_RX_P2_RATE:
		chk = port_cfg->rx_rate[2];
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_RX_P3_RATE:
		chk = port_cfg->rx_rate[3];
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_TX_P0_RATE:
		chk = port_cfg->tx_rate[0];
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_TX_P1_RATE:
		chk = port_cfg->tx_rate[1];
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_TX_P2_RATE:
		chk = port_cfg->tx_rate[2];
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_TX_P3_RATE:
		chk = port_cfg->tx_rate[3];
		type = SHOW_HELP_NUM;
		break;
	}
	return sysfs_show(len, buf, type, chk, note, sw->verbose);
}

static ssize_t sysfs_port_read_hw(struct ksz_sw *sw, int proc_num, uint port,
	ssize_t len, char *buf)
{
	u8 data[8];
	int chk = 0;
	int type = SHOW_HELP_ON_OFF;
	char note[40];

	note[0] = '\0';
	port = get_sysfs_port(sw, port);
	switch (proc_num) {
	case PROC_ENABLE_BROADCAST_STORM:
		chk = port_chk_broad_storm(sw, port);
		break;
	case PROC_ENABLE_DIFFSERV:
		chk = port_chk_diffserv(sw, port);
		break;
	case PROC_ENABLE_802_1P:
		chk = port_chk_802_1p(sw, port);
		break;
	case PROC_ENABLE_PRIO_QUEUE:
		chk = port_chk_4_queue(sw, port);
		chk <<= 1;
		chk |= port_chk_2_queue(sw, port);
		chk = (1 << chk);
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_MIRROR_PORT:
		chk = port_chk_mirror_sniffer(sw, port);
		break;
	case PROC_SET_MIRROR_RX:
		chk = port_chk_mirror_rx(sw, port);
		break;
	case PROC_SET_MIRROR_TX:
		chk = port_chk_mirror_tx(sw, port);
		break;
	case PROC_SET_LEARN:
		chk = !port_chk_dis_learn(sw, port);
		break;
	case PROC_SET_RX:
		chk = port_chk_rx(sw, port);
		break;
	case PROC_SET_TX:
		chk = port_chk_tx(sw, port);
		break;
	case PROC_SET_INSERT_TAG:
		chk = port_chk_ins_tag(sw, port);
		break;
	case PROC_SET_REMOVE_TAG:
		chk = port_chk_rmv_tag(sw, port);
		break;
#ifdef PORT_DOUBLE_TAG
	case PROC_SET_DOUBLE_TAG:
		chk = port_chk_double_tag(sw, port);
		break;
#endif
	case PROC_SET_DROP_TAG:
		chk = port_chk_drop_tag(sw, port);
		break;
	case PROC_SET_REPLACE_PRIO:
		chk = port_chk_replace_prio(sw, port);
		break;
	case PROC_ENABLE_RX_PRIO_RATE:
		chk = sw_chk_rx_prio_rate(sw, port);
		break;
	case PROC_ENABLE_TX_PRIO_RATE:
		chk = sw_chk_tx_prio_rate(sw, port);
		break;
	case PROC_SET_DIS_NON_VID:
		chk = port_chk_dis_non_vid(sw, port);
		break;
	case PROC_SET_INGRESS:
		chk = port_chk_in_filter(sw, port);
		break;
	case PROC_SET_BACK_PRESSURE:
		chk = port_chk_back_pressure(sw, port);
		break;
	case PROC_SET_FORCE_FLOW_CTRL:
		chk = port_chk_force_flow_ctrl(sw, port);
		break;
	case PROC_SET_UNKNOWN_DEF_PORT:
		chk = sw_chk_unk_def_port(sw, port);
		break;
	case PROC_SET_FORWARD_INVALID_VID:
		chk = sw_chk_for_inv_vid(sw, port);
		break;
	case PROC_SET_PORT_MAC_ADDR:
		port_get_addr(sw, port, data);
		len += sprintf(buf + len, "%02X:%02X:%02X:%02X:%02X:%02X\n",
			data[0], data[1], data[2], data[3], data[4], data[5]);
		type = SHOW_HELP_NONE;
		break;
	case PROC_SET_SRC_FILTER_0:
		chk = port_chk_src_filter_0(sw, port);
		break;
	case PROC_SET_SRC_FILTER_1:
		chk = port_chk_src_filter_1(sw, port);
		break;
	default:
		type = SHOW_HELP_NONE;
		break;
	}
	return sysfs_show(len, buf, type, chk, note, sw->verbose);
}

static int sysfs_port_write(struct ksz_sw *sw, int proc_num, uint port,
	int num, const char *buf)
{
	u8 data[8];
	int processed = true;

	port = get_sysfs_port(sw, port);
	switch (proc_num) {
	case PROC_SET_PORT_DUPLEX:
	case PROC_SET_PORT_SPEED:
	{
		struct ksz_port phy_port;
		struct ksz_port_info *port_info = get_port_info(sw, port);

		if ((PROC_SET_PORT_DUPLEX == proc_num && num > 2) ||
		    (PROC_SET_PORT_SPEED == proc_num &&
		    num != 0 && num != 10 && num != 100))
			break;

		phy_port.sw = sw;
		phy_port.port_cnt = 1;
		phy_port.first_port = get_log_port(sw, port);
		phy_port.flow_ctrl = port_info->own_flow_ctrl;
		phy_port.duplex = port_info->own_duplex;
		phy_port.speed = port_info->own_speed;
		if (PROC_SET_PORT_DUPLEX == proc_num)
			phy_port.duplex = (u8) num;
		else
			phy_port.speed = (u16) num;
		port_set_link_speed(&phy_port);
		break;
	}
	case PROC_SET_PORT_MIB:
	{
		struct ksz_port_mib *mib = get_port_mib(sw, port);

		memset((void *) mib->counter, 0, sizeof(u64) *
			TOTAL_SWITCH_COUNTER_NUM);
		break;
	}
	case PROC_ENABLE_BROADCAST_STORM:
		if (!num)
			sw_dis_broad_storm(sw, port);
		else
			sw_ena_broad_storm(sw, port);
		break;
	case PROC_ENABLE_DIFFSERV:
		if (!num)
			sw_dis_diffserv(sw, port);
		else
			sw_ena_diffserv(sw, port);
		break;
	case PROC_ENABLE_802_1P:
		if (!num)
			sw_dis_802_1p(sw, port);
		else
			sw_ena_802_1p(sw, port);
		break;
	case PROC_SET_PORT_BASED:
		sw_cfg_port_based(sw, port, num);
		break;
	case PROC_SET_DEF_VID:
		sw_cfg_def_vid(sw, port, num);
		break;
	case PROC_SET_MEMBER:
		sw_cfg_port_base_vlan(sw, port, (u8) num);
		break;
	case PROC_ENABLE_PRIO_QUEUE:
		if (0 <= num && num <= 4)
			sw_set_multi_queue(sw, port, num);
		break;
	case PROC_SET_TX_P0_CTRL:
		hw_cfg_rate_ctrl(sw, port, 0, num);
		break;
	case PROC_SET_TX_P1_CTRL:
		hw_cfg_rate_ctrl(sw, port, 1, num);
		break;
	case PROC_SET_TX_P2_CTRL:
		hw_cfg_rate_ctrl(sw, port, 2, num);
		break;
	case PROC_SET_TX_P3_CTRL:
		hw_cfg_rate_ctrl(sw, port, 3, num);
		break;
#ifdef RATE_RATIO_MASK
	case PROC_SET_TX_P0_RATIO:
		hw_cfg_rate_ratio(sw, port, 0, (u8) num);
		break;
	case PROC_SET_TX_P1_RATIO:
		hw_cfg_rate_ratio(sw, port, 1, (u8) num);
		break;
	case PROC_SET_TX_P2_RATIO:
		hw_cfg_rate_ratio(sw, port, 2, (u8) num);
		break;
	case PROC_SET_TX_P3_RATIO:
		hw_cfg_rate_ratio(sw, port, 3, (u8) num);
		break;
#endif
	case PROC_SET_RX_LIMIT:
		hw_cfg_rx_limit(sw, port, (u8) num);
		break;
	case PROC_SET_CNT_IFG:
		hw_cfg_cnt_ifg(sw, port, num);
		break;
	case PROC_SET_CNT_PRE:
		hw_cfg_cnt_pre(sw, port, num);
		break;
	case PROC_SET_RX_P0_RATE:
		hw_cfg_rx_prio_rate(sw, port, 0, num);
		break;
	case PROC_SET_RX_P1_RATE:
		hw_cfg_rx_prio_rate(sw, port, 1, num);
		break;
	case PROC_SET_RX_P2_RATE:
		hw_cfg_rx_prio_rate(sw, port, 2, num);
		break;
	case PROC_SET_RX_P3_RATE:
		hw_cfg_rx_prio_rate(sw, port, 3, num);
		break;
	case PROC_SET_TX_P0_RATE:
		hw_cfg_tx_prio_rate(sw, port, 0, num);
		break;
	case PROC_SET_TX_P1_RATE:
		hw_cfg_tx_prio_rate(sw, port, 1, num);
		break;
	case PROC_SET_TX_P2_RATE:
		hw_cfg_tx_prio_rate(sw, port, 2, num);
		break;
	case PROC_SET_TX_P3_RATE:
		hw_cfg_tx_prio_rate(sw, port, 3, num);
		break;
	case PROC_SET_MIRROR_PORT:
		port_cfg_mirror_sniffer(sw, port, num);
		break;
	case PROC_SET_MIRROR_RX:
		port_cfg_mirror_rx(sw, port, num);
		break;
	case PROC_SET_MIRROR_TX:
		port_cfg_mirror_tx(sw, port, num);
		break;
	case PROC_SET_LEARN:
		port_cfg_dis_learn(sw, port, !num);
		if (!num)
			sw_cfg(sw, S_FLUSH_TABLE_CTRL,
				SWITCH_FLUSH_DYN_MAC_TABLE, 1);
		break;
	case PROC_SET_RX:
		port_cfg_rx(sw, port, num);
		break;
	case PROC_SET_TX:
		port_cfg_tx(sw, port, num);
		break;
	case PROC_SET_INSERT_TAG:
		sw_vlan_cfg_ins_tag(sw, port, num);
		break;
	case PROC_SET_REMOVE_TAG:
		sw_vlan_cfg_rmv_tag(sw, port, num);
		break;
#ifdef PORT_DOUBLE_TAG
	case PROC_SET_DOUBLE_TAG:
		sw_vlan_cfg_double_tag(sw, port, num);
		break;
#endif
	case PROC_SET_DROP_TAG:
		sw_vlan_cfg_drop_tag(sw, port, num);
		break;
	case PROC_SET_REPLACE_PRIO:
		sw_cfg_replace_prio(sw, port, num);
		break;
	case PROC_ENABLE_RX_PRIO_RATE:
		if (!num)
			sw_dis_rx_prio_rate(sw, port);
		else
			sw_ena_rx_prio_rate(sw, port);
		break;
	case PROC_ENABLE_TX_PRIO_RATE:
		if (!num)
			sw_dis_tx_prio_rate(sw, port);
		else
			sw_ena_tx_prio_rate(sw, port);
		break;
	case PROC_SET_DIS_NON_VID:
		sw_vlan_cfg_dis_non_vid(sw, port, num);
		break;
	case PROC_SET_INGRESS:
		sw_vlan_cfg_in_filter(sw, port, num);
		break;
	case PROC_SET_BACK_PRESSURE:
		port_cfg_back_pressure(sw, port, num);
		break;
	case PROC_SET_FORCE_FLOW_CTRL:
		port_cfg_force_flow_ctrl(sw, port, num);
		break;
	case PROC_SET_UNKNOWN_DEF_PORT:
		sw_cfg_unk_def_port(sw, port, num);
		break;
	case PROC_SET_FORWARD_INVALID_VID:
		sw_cfg_for_inv_vid(sw, port, num);
		break;
	case PROC_SET_LINK_MD:
		sw_get_link_md(sw, port);
		break;
	case PROC_SET_PORT_MAC_ADDR:
	{
		int i;
		int n[6];

		i = sscanf(buf, "%x:%x:%x:%x:%x:%x",
			&n[0], &n[1], &n[2], &n[3], &n[4], &n[5]);
		if (6 == i) {
			for (i = 0; i < 6; i++)
				data[i] = (u8) n[i];
			port_set_addr(sw, port, data);
		}
		break;
	}
	case PROC_SET_SRC_FILTER_0:
		port_cfg_src_filter_0(sw, port, num);
		break;
	case PROC_SET_SRC_FILTER_1:
		port_cfg_src_filter_1(sw, port, num);
		break;
	default:
		processed = false;
		break;
	}
	return processed;
}

static ssize_t sysfs_mac_read(struct ksz_sw *sw, int proc_num, int index,
	ssize_t len, char *buf)
{
	struct ksz_mac_table *entry;

	entry = &sw->info->mac_table[index];
	switch (proc_num) {
	case PROC_SET_STATIC_FID:
		len += sprintf(buf + len, "%u\n", entry->fid);
		break;
	case PROC_SET_STATIC_USE_FID:
		len += sprintf(buf + len, "%u\n", entry->use_fid);
		break;
	case PROC_SET_STATIC_OVERRIDE:
		len += sprintf(buf + len, "%u\n", entry->override);
		break;
	case PROC_SET_STATIC_VALID:
		len += sprintf(buf + len, "%u\n", entry->valid);
		break;
	case PROC_SET_STATIC_PORTS:
		len += sprintf(buf + len, "%u\n", entry->ports);
		break;
	case PROC_SET_STATIC_MAC_ADDR:
		len += sprintf(buf + len, "%02x:%02x:%02x:%02x:%02x:%02x\n",
			entry->mac_addr[0], entry->mac_addr[1],
			entry->mac_addr[2], entry->mac_addr[3],
			entry->mac_addr[4], entry->mac_addr[5]);
		break;
	}
	return len;
}

static int sysfs_mac_write(struct ksz_sw *sw, int proc_num, int index,
	int num, const char *buf)
{
	struct ksz_mac_table *entry;
	int processed = true;

	entry = &sw->info->mac_table[index];
	switch (proc_num) {
	case PROC_SET_STATIC_FID:
		if (0 <= num && num < 16)
			entry->fid = num;
		break;
	case PROC_SET_STATIC_USE_FID:
		if (num)
			entry->use_fid = 1;
		else
			entry->use_fid = 0;
		break;
	case PROC_SET_STATIC_OVERRIDE:
		if (num)
			entry->override = 1;
		else
			entry->override = 0;
		break;
	case PROC_SET_STATIC_VALID:
		if (num)
			entry->valid = 1;
		else
			entry->valid = 0;

		/* Not hardware entry. */
		if (index >= STATIC_MAC_TABLE_ENTRIES)
			break;
		sw_w_sta_mac_table(sw, index,
			entry->mac_addr, entry->ports,
			entry->override, entry->valid,
			entry->use_fid, entry->fid);
		break;
	case PROC_SET_STATIC_PORTS:
		if (0 <= num && num <= sw->PORT_MASK)
			entry->ports = num;
		break;
	case PROC_SET_STATIC_MAC_ADDR:
	{
		int i;
		int n[6];

		i = sscanf(buf, "%x:%x:%x:%x:%x:%x",
			&n[0], &n[1], &n[2], &n[3], &n[4], &n[5]);
		if (6 == i) {
			for (i = 0; i < 6; i++)
				entry->mac_addr[i] = (u8) n[i];
		}
		break;
	}
	default:
		processed = false;
		break;
	}
	return processed;
}

static ssize_t sysfs_vlan_read(struct ksz_sw *sw, int proc_num, int index,
	ssize_t len, char *buf)
{
	struct ksz_vlan_table *entry;

	entry = &sw->info->vlan_table[index];
	switch (proc_num) {
	case PROC_SET_VLAN_VALID:
		len += sprintf(buf + len, "%u\n", entry->valid);
		break;
	case PROC_SET_VLAN_MEMBER:
		len += sprintf(buf + len, "%u\n", entry->member);
		break;
	case PROC_SET_VLAN_FID:
		len += sprintf(buf + len, "%u\n", entry->fid);
		break;
	case PROC_SET_VLAN_VID:
		len += sprintf(buf + len, "0x%04x\n", entry->vid);
		break;
	}
	return len;
}

static int sysfs_vlan_write(struct ksz_sw *sw, int proc_num, int index,
	int num)
{
	struct ksz_vlan_table *entry;
	int processed = true;

	entry = &sw->info->vlan_table[index];
	switch (proc_num) {
	case PROC_SET_VLAN_VALID:
		if (num)
			entry->valid = 1;
		else
			entry->valid = 0;
		sw_w_vlan_table(sw, index, entry->vid, entry->fid,
			entry->member, entry->valid);
		break;
	case PROC_SET_VLAN_MEMBER:
		if (0 <= num && num <= sw->PORT_MASK)
			entry->member = num;
		break;
	case PROC_SET_VLAN_FID:
		if (0 <= num && num < 16)
			entry->fid = num;
		break;
	case PROC_SET_VLAN_VID:
		if (0 <= num && num < 0x1000)
			entry->vid = num;
		break;
	default:
		processed = false;
		break;
	}
	return processed;
}

/* -------------------------------------------------------------------------- */

static void sw_cfg_mac(struct ksz_sw *sw, u8 index, u8 *dest, u32 ports,
	int override, int use_fid, u16 fid)
{
	struct ksz_mac_table *mac;

	if (index >= STATIC_MAC_TABLE_ENTRIES)
		return;
	mac = &sw->info->mac_table[index];
	memset(mac, 0, sizeof(struct ksz_mac_table));
	memcpy(mac->mac_addr, dest, ETH_ALEN);
	mac->ports = (u8) ports;
	mac->override = override;
	mac->use_fid = use_fid;
	mac->fid = (u8) fid;
	mac->valid = mac->ports != 0;
	if (!mac->valid && mac->override) {
		mac->override = 0;
		mac->valid = 1;
	}
	sw_w_sta_mac_table(sw, index, mac->mac_addr, mac->ports, mac->override,
		mac->valid, mac->use_fid, mac->fid);
}  /* sw_cfg_mac */

static void sw_cfg_vlan(struct ksz_sw *sw, u8 index, u16 vid, u16 fid,
	u32 ports)
{
	struct ksz_vlan_table vlan;

	memset(&vlan, 0, sizeof(struct ksz_vlan_table));
	vlan.vid = vid;
	vlan.fid = (u8) fid;
	vlan.member = (u8) ports;
	vlan.valid = vlan.member != 0;
	sw_w_vlan_table(sw, index, vlan.vid, vlan.fid, vlan.member, vlan.valid);
}  /* sw_cfg_vlan */

static u8 sw_alloc_mac(struct ksz_sw *sw)
{
	int i;

	for (i = 1; i < STATIC_MAC_TABLE_ENTRIES; i++) {
		if (!(sw->info->mac_table_used & (1 << i))) {
			sw->info->mac_table_used |= (1 << i);
			return i;
		}
	}
	return 0;
}  /* sw_alloc_mac */

static void sw_free_mac(struct ksz_sw *sw, u8 index)
{
	sw->info->mac_table_used &= ~(1 << index);
}  /* sw_free_mac */

static u8 sw_alloc_vlan(struct ksz_sw *sw)
{
	int i;

	/* First one is reserved. */
	for (i = 1; i < VLAN_TABLE_ENTRIES - 1; i++) {
		if (!(sw->info->vlan_table_used & (1 << i))) {
			sw->info->vlan_table_used |= (1 << i);
			return i;
		}
	}

	/* Reject request. */
	return 0;
}  /* sw_alloc_vlan */

static void sw_free_vlan(struct ksz_sw *sw, u8 index)
{
	if (index)
		sw->info->vlan_table_used &= ~(1 << index);
}  /* sw_free_vlan */

static u16 sw_alloc_fid(struct ksz_sw *sw, u16 vid)
{
	int i;

	/* First one is reserved. */
	for (i = 1; i < VLAN_TABLE_ENTRIES; i++) {
		if (!(sw->info->fid_used & (1 << i))) {
			sw->info->fid_used |= (1 << i);
			return i;
		}
	}
	return 0;
}  /* sw_alloc_fid */

static void sw_free_fid(struct ksz_sw *sw, u16 fid)
{
	if (fid)
		sw->info->fid_used &= ~(1 << fid);
}  /* sw_free_fid */

static const u8 *sw_get_br_id(struct ksz_sw *sw)
{
	static u8 id[8];
	const u8* ret = id;

	memcpy(&id[2], sw->info->mac_addr, ETH_ALEN);
	id[0] = 0x80;
	id[1] = 0x00;

#ifdef CONFIG_KSZ_STP
	ret = stp_br_id(&sw->info->rstp);
#endif
	return ret;
}  /* sw_get_br_id */

static void sw_from_backup(struct ksz_sw *sw, uint p)
{
#ifdef CONFIG_KSZ_MRP
	if (sw->features & MRP_SUPPORT) {
		struct mrp_info *mrp = &sw->mrp;

		mrp->ops->from_backup(mrp, p);
	}
#endif
}  /* sw_from_backup */

static void sw_to_backup(struct ksz_sw *sw, uint p)
{
#ifdef CONFIG_KSZ_MRP
	if (sw->features & MRP_SUPPORT) {
		struct mrp_info *mrp = &sw->mrp;

		mrp->ops->to_backup(mrp, p);
	}
#endif
}  /* sw_to_backup */

static void sw_from_designated(struct ksz_sw *sw, uint p, bool alt)
{
#ifdef CONFIG_KSZ_MRP
	if (sw->features & MRP_SUPPORT) {
		struct mrp_info *mrp = &sw->mrp;

		mrp->ops->from_designated(mrp, p, alt);
	}
#endif
}  /* sw_from_designated */

static void sw_to_designated(struct ksz_sw *sw, uint p)
{
#ifdef CONFIG_KSZ_MRP
	if (sw->features & MRP_SUPPORT) {
		struct mrp_info *mrp = &sw->mrp;

		mrp->ops->to_designated(mrp, p);
	}
#endif
}  /* sw_to_designated */

static void sw_tc_detected(struct ksz_sw *sw, uint p)
{
#ifdef CONFIG_KSZ_MRP
	if (sw->features & MRP_SUPPORT) {
		struct mrp_info *mrp = &sw->mrp;

		mrp->ops->tc_detected(mrp, p);
	}
#endif
}  /* sw_tc_detected */

static int sw_get_tcDetected(struct ksz_sw *sw, uint p)
{
	int ret = false;

#ifdef CONFIG_KSZ_STP
	if (sw->features & STP_SUPPORT) {
		struct ksz_stp_info *info = &sw->info->rstp;

		ret = info->ops->get_tcDetected(info, p);
	}
#endif
	return ret;
}  /* sw_get_tcDetected */

#ifdef CONFIG_HAVE_KSZ8463
enum {
	KSZ8463_SW_CHIP,
};

#ifndef NO_PHYDEV
static void sw_dis_intr(struct ksz_sw *sw)
{
	sw->reg->w16(sw, TS_INT_ENABLE, 0);
	sw->reg->w16(sw, TRIG_INT_ENABLE, 0);
	sw->reg->w16(sw, REG_INT_MASK, 0);
}  /* sw_dis_intr */

static void sw_ena_intr(struct ksz_sw *sw)
{
	struct sw_priv *ks = container_of(sw, struct sw_priv, sw);

	sw->reg->w16(sw, REG_INT_MASK, ks->intr_mask);
}  /* sw_ena_intr */

static SW_D sw_proc_intr(struct ksz_sw *sw)
{
	SW_D status;
	struct sw_priv *ks = container_of(sw, struct sw_priv, sw);

	status = sw->reg->r16(sw, REG_INT_STATUS);
	status &= ks->intr_mask;
	if (status & INT_PHY) {
		sw->reg->w16(sw, REG_INT_STATUS, INT_PHY);
		status &= ~INT_PHY;
		schedule_delayed_work(&ks->link_read, 0);
	}
#ifdef CONFIG_1588_PTP
	do {
		struct ptp_info *ptp = &sw->ptp_hw;

		if (ptp->ops->proc_intr) {
			ptp->ops->proc_intr(ptp);
			status = 0;
		}
	} while (0);
#endif
	return status;
}  /* sw_proc_intr */

static void sw_setup_intr(struct ksz_sw *sw)
{
	struct sw_priv *ks = container_of(sw, struct sw_priv, sw);

	ks->intr_mask = INT_PHY | INT_TIMESTAMP | INT_TRIG_OUTPUT;
	sw->reg->w16(sw, TS_INT_ENABLE, 0);
	sw->reg->w16(sw, TS_INT_STATUS, 0xffff);
}  /* sw_setup_intr */

static int sw_chk_id(struct ksz_sw *sw, u16 *id)
{
	*id = sw->reg->r16(sw, REG_SWITCH_SIDER);
	if ((*id & CIDER_ID_MASK) != CIDER_ID_8463 &&
	    (*id & CIDER_ID_MASK) != CIDER_ID_8463_RLI)
		return -ENODEV;
	sw->chip_id = KSZ8463_SW_CHIP;
	return 2;
}
#endif
#endif

#ifdef CONFIG_HAVE_KSZ8863
enum {
	KSZ8863_SW_CHIP,
	KSZ8873_SW_CHIP,
};

static void sw_dis_intr(struct ksz_sw *sw)
{
	sw->reg->w8(sw, REG_INT_ENABLE, 0);
}  /* sw_dis_intr */

static void sw_ena_intr(struct ksz_sw *sw)
{
	struct sw_priv *ks = container_of(sw, struct sw_priv, sw);

	sw->reg->w8(sw, REG_INT_ENABLE, ks->intr_mask);
}  /* sw_ena_intr */

static SW_D sw_proc_intr(struct ksz_sw *sw)
{
	SW_D status;
	struct sw_priv *ks = container_of(sw, struct sw_priv, sw);

	status = sw->reg->r8(sw, REG_INT_STATUS);
	status &= ks->intr_mask;
	if (status & ks->intr_mask) {
		sw->reg->w8(sw, REG_INT_STATUS, status);
		status &= ~ks->intr_mask;
		schedule_delayed_work(&ks->link_read, 0);
	}
	return status;
}  /* sw_proc_intr */

static void sw_setup_intr(struct ksz_sw *sw)
{
	struct sw_priv *ks = container_of(sw, struct sw_priv, sw);

	ks->intr_mask = INT_PORT_1_LINK_CHANGE | INT_PORT_2_LINK_CHANGE |
		INT_PORT_3_LINK_CHANGE | INT_PORT_1_2_LINK_CHANGE;
	sw->reg->w8(sw, REG_INT_ENABLE, 0);
	sw->reg->w8(sw, REG_INT_STATUS, ks->intr_mask);
}  /* sw_setup_intr */

static int sw_chk_id(struct ksz_sw *sw, u16 *id)
{
	u8 mode;

	*id = sw->reg->r16(sw, REG_CHIP_ID0);
	if ((*id & SWITCH_CHIP_ID_MASK) != CHIP_ID_63)
		return -ENODEV;
	sw->chip_id = KSZ8863_SW_CHIP;
	mode = sw->reg->r8(sw, REG_MODE_INDICATOR);
	if (!(mode & (PORT_1_COPPER | PORT_2_COPPER)) ||
	    !(mode & MODE_2_PHY))
		sw->chip_id = KSZ8873_SW_CHIP;
	return 2;
}
#endif

static void sw_cfg_tail_tag(struct ksz_sw *sw, bool enable)
{
	sw_cfg(sw, S_TAIL_TAG_CTRL, SWITCH_TAIL_TAG_ENABLE, enable);
}

static void sw_set_multi(struct ksz_sw *sw, struct net_device *dev,
	struct ksz_port *priv)
{
	struct ksz_mac_table *entry;
	struct ksz_alu_table *alu;
	struct netdev_hw_addr *ha;
	int i;
	int found;
	int owner;
	int port = 0;
	struct ksz_sw_info *info = sw->info;

	/* This device is associated with a switch port. */
	if (1 == priv->port_cnt)
		port = priv->first_port;
	owner = 1 << port;

	/* Remove old multicast entries. */
	for (i = SWITCH_MAC_TABLE_ENTRIES; i < info->multi_net; i++) {
		entry = &info->mac_table[i];
		alu = &info->alu_table[i];
		if (alu->valid && (alu->owner & owner)) {
			/* Remove device ownership. */
			alu->owner &= ~owner;
			if (!port)
				alu->forward &= ~FWD_MAIN_DEV;
			else if (alu->owner <= 1)
				alu->forward &= ~FWD_STP_DEV;
			if (!alu->owner) {
				alu->valid = 0;
				entry->ports = 0;
				entry->valid = 0;
			}
		}
	}
	netdev_for_each_mc_addr(ha, dev) {
		if (!(*ha->addr & 1))
			continue;
		if (info->multi_net == info->multi_sys)
			break;
		found = 0;
		for (i = 0; i < MULTI_MAC_TABLE_ENTRIES; i++) {
			entry = &info->mac_table[i];
			alu = &info->alu_table[i];
			if (alu->valid &&
			    !memcmp(entry->mac_addr, ha->addr, ETH_ALEN)) {
				found = i + 1;
				break;
			}
			if (!alu->valid && !found &&
			    i >= SWITCH_MAC_TABLE_ENTRIES &&
			    i < info->multi_net)
				found = i + 1;
		}
		if (!found) {
			info->multi_net++;
			found = info->multi_net;
		}
		found--;
		if (found >= SWITCH_MAC_TABLE_ENTRIES &&
		    found < info->multi_net) {
			entry = &info->mac_table[found];
			alu = &info->alu_table[found];
			if (port)
				alu->forward |= FWD_STP_DEV;
			else
				alu->forward |= FWD_MAIN_DEV;
			alu->owner |= owner;
			alu->valid = 1;
			memcpy(entry->mac_addr, ha->addr, ETH_ALEN);
			entry->ports = sw->PORT_MASK;
			entry->valid = 1;
		}
	}
}  /* sw_set_multi */

static void sw_forward(struct ksz_sw *sw, u8 *addr, u8 *self, u16 proto,
	int tag)
{
	int forward = 0;

	/* Already set for PTP message. */
	if (sw->info->forward)
		return;

	/* Check for multicast addresses that are not forwarding. */
	if (addr[0] & 0x01) {
		int i;
		struct ksz_mac_table *entry;
		struct ksz_alu_table *alu;

		for (i = MULTI_MAC_TABLE_ENTRIES - 1; i >= 0; i--) {
			alu = &sw->info->alu_table[i];
			entry = &sw->info->mac_table[i];
			if (alu->valid &&
			    !memcmp(addr, entry->mac_addr, ETH_ALEN)) {
				forward = alu->forward;
				if (!(forward & FWD_VLAN_DEV)) {
					if (proto == 0x888E)
						forward = FWD_STP_DEV |
							  FWD_VLAN_DEV;
				}
				break;
			}
		}
		if (!forward)
			forward = FWD_MAIN_DEV/* | FWD_MCAST*/;

	/* Check unicast address to host. */
	} else if (!memcmp(addr, self, ETH_ALEN))
		forward = FWD_HOST;
	else
		forward = FWD_MAIN_DEV/* | FWD_UCAST*/;
	if (!tag)
		forward &= ~FWD_VLAN_DEV;
	sw->info->forward = forward;
#if 0
dbg_msg(" forward: %d %x\n", tag, forward);
#endif
}  /* sw_forward */

static struct net_device *sw_rx_dev(struct ksz_sw *sw, u8 *data, u32 *len,
	int *tag, int *port)
{
	u16 proto;
	u16* proto_loc;
	struct net_device *dev;
	struct vlan_ethhdr *vlan = (struct vlan_ethhdr *) data;
	int index = -1;
	int vid = 0;

	proto_loc = &vlan->h_vlan_proto;
	proto = htons(*proto_loc);
	sw->info->forward = 0;

	/* Get received port number. */
	if (sw->overrides & TAIL_TAGGING) {
		(*len)--;
		*tag = data[*len];

		/* In case tagging is not working right. */
		if (*tag >= SWITCH_PORT_NUM)
			*tag = 0;

		/* Save receiving port. */
		*port = *tag;
		index = sw->info->port_cfg[*tag].index;
	}

	/* Determine network device from VLAN id. */
	if (index < 0) {
		index = 0;
		if (vlan->h_vlan_proto == htons(ETH_P_8021Q)) {
			u16 vlan_tci = ntohs(vlan->h_vlan_TCI);

			vid = vlan_tci & VLAN_VID_MASK;
			proto_loc = &vlan->h_vlan_encapsulated_proto;
			proto = htons(*proto_loc);
		}
		if (vid && (sw->features & SW_VLAN_DEV)) {
			struct ksz_dev_map *map;
			int p;

			for (p = 0; p < sw->eth_cnt; p++) {
				map = &sw->eth_maps[p];
				if (vid == map->vlan) {
					*port = map->first;
					p = get_phy_port(sw, *port);
					*port = p;
					index = sw->info->port_cfg[p].index;
					break;
				}
			}
		}
	}
	if (index >= sw->dev_count + sw->dev_offset) {
		printk(KERN_INFO "  [%s] netdev not correct\n", __func__);
		BUG();
	}
	dev = sw->netdev[index];
#ifdef CONFIG_KSZ_DLR
	if (proto == DLR_TAG_TYPE)
		return dev;
#endif
	if (!(sw->features & VLAN_PORT_TAGGING) ||
	    !(sw->vlan_id & (1 << *tag))) {
		*tag = 0;
	}
	sw_forward(sw, data, dev->dev_addr, proto, *tag);
	return dev;
}  /* sw_rx_dev */

static int pkt_matched(struct sk_buff *skb, struct net_device *dev, void *ptr,
	int (*match_multi)(void *ptr, u8 *addr), u8 h_promiscuous)
{
	int drop = false;
	u8 bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (skb->data[0] & 0x01) {
		if (memcmp(skb->data, bcast_addr, ETH_ALEN))
			drop = match_multi(ptr, skb->data);
	} else if (h_promiscuous && memcmp(skb->data, dev->dev_addr, ETH_ALEN))
		drop = true;
	if (drop)
		return 0;
	return skb->len;
}  /* pkt_matched */

static int sw_match_pkt(struct ksz_sw *sw, struct net_device **dev,
	void **priv, int (*get_promiscuous)(void *ptr),
	int (*match_multi)(void *ptr, u8 *data), struct sk_buff *skb,
	u8 h_promiscuous)
{
	int s_promiscuous;

	if (sw->dev_count <= 1)
		return true;
	s_promiscuous = get_promiscuous(*priv);
	if (!s_promiscuous && !pkt_matched(skb, *dev, *priv, match_multi,
			h_promiscuous)) {
		int matched = false;

		/* There is a parent network device. */
		if (sw->dev_offset) {
			matched = true;
			*dev = sw->netdev[0];
			*priv = netdev_priv(*dev);
			s_promiscuous = get_promiscuous(*priv);
			if (!s_promiscuous && !pkt_matched(skb, *dev, *priv,
					match_multi, h_promiscuous))
				matched = false;
		}
		return matched;
	}
	return true;
}  /* sw_match_pkt */

static struct net_device *sw_parent_rx(struct ksz_sw *sw,
	struct net_device *dev, struct sk_buff *skb, int *forward,
	struct net_device **parent_dev, struct sk_buff **parent_skb)
{
	if (sw->dev_offset && dev != sw->netdev[0]) {
		*parent_dev = sw->netdev[0];
		if (!*forward)
			*forward = FWD_MAIN_DEV | FWD_STP_DEV;
		if ((*forward & (FWD_MAIN_DEV | FWD_STP_DEV)) ==
		    (FWD_MAIN_DEV | FWD_STP_DEV))
			*parent_skb = skb_clone(skb, GFP_ATOMIC);
		else if (!(*forward & FWD_STP_DEV))
			dev = *parent_dev;
		else
			*forward &= ~FWD_VLAN_DEV;
	}
	return dev;
}  /* sw_parent_rx */

static int sw_port_vlan_rx(struct ksz_sw *sw, struct net_device *dev,
	struct net_device *parent_dev, struct sk_buff *skb, int forward,
	int tag, void *ptr, void (*rx_tstamp)(void *ptr, struct sk_buff *skb))
{
	struct sk_buff *vlan_skb;
	struct net_device *vlan_dev = dev;

	/* Add VLAN tag manually. */
	if (!(forward & FWD_VLAN_DEV))
		return false;

	if (!tag || !(sw->features & VLAN_PORT))
		return false;
	tag += VLAN_PORT_START;

	/* Only forward to one network device. */
	if (!(forward & FWD_MAIN_DEV)) {
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), tag);
		return true;
	}
	vlan_skb = skb_clone(skb, GFP_ATOMIC);
	if (!vlan_skb)
		return false;
	skb_reset_mac_header(vlan_skb);
	__vlan_hwaccel_put_tag(vlan_skb, htons(ETH_P_8021Q), tag);
#ifdef CONFIG_1588_PTP
	do {
		struct ptp_info *ptp = ptr;

		if (rx_tstamp && (ptp->rx_en & 1))
			rx_tstamp(ptp, vlan_skb);
	} while (0);
#endif
	if (parent_dev && dev != parent_dev) {
		vlan_dev = parent_dev;
		vlan_skb->dev = vlan_dev;
	}
	vlan_skb->protocol = eth_type_trans(vlan_skb, vlan_dev);
	netif_rx(vlan_skb);
	return true;
}  /* sw_port_vlan_rx */

static int sw_drv_rx(struct ksz_sw *sw, struct sk_buff *skb, uint port)
{
	int ret = 1;

#ifdef CONFIG_KSZ_STP
	if (sw->features & STP_SUPPORT) {
		ret = stp_rcv(&sw->info->rstp, skb, port);
		if (!ret)
			return ret;
	}
#endif
#ifdef CONFIG_KSZ_DLR
	if (sw->features & DLR_HW) {
		ret = dlr_rcv(&sw->info->dlr, skb, port);
		if (!ret)
			return ret;
	}
#endif
#ifdef CONFIG_KSZ_HSR
	if (sw->features & HSR_HW) {
		ret = hsr_rcv(&sw->info->hsr, skb, port);

		/* It is an HSR frame or consumed. */
		if (ret < 2)
			return ret;
	}
#endif

	/* Need to remove VLAN tag if not using tail tag. */
	if (sw->dev_count > 1 && (sw->features & SW_VLAN_DEV) &&
	    !(sw->overrides & TAIL_TAGGING)) {
		struct vlan_ethhdr *vlan = (struct vlan_ethhdr *) skb->data;

		if (vlan->h_vlan_proto == htons(ETH_P_8021Q)) {
			int p;
			int vid;
			struct ethhdr *eth;
			u16 vlan_tci = ntohs(vlan->h_vlan_TCI);

			vid = vlan_tci & VLAN_VID_MASK;
			for (p = 0; p < sw->eth_cnt; p++) {
				if (vid == sw->eth_maps[p].vlan) {
					eth = (struct ethhdr *)
						skb_pull(skb, VLAN_HLEN);
					memmove(eth, vlan, 12);
					break;
				}
			}
		}
	}
	return ret;
}  /* sw_drv_rx */

static int sw_get_mtu(struct ksz_sw *sw)
{
	int need_tail_tag = false;
	int header = 0;
	int mtu = 0;

#ifdef CONFIG_KSZ_DLR
	if (sw->features & DLR_HW)
		need_tail_tag = true;
#endif
#ifdef CONFIG_KSZ_HSR
	if (sw->features & HSR_HW)
		need_tail_tag = true;
#endif
	if (sw->dev_count > 1 && !(sw->features & SW_VLAN_DEV))
		need_tail_tag = true;
	if (sw->features & VLAN_PORT_TAGGING)
		need_tail_tag = true;
	if (sw->features & STP_SUPPORT)
		need_tail_tag = true;
	if (need_tail_tag)
		mtu += 1;
#ifdef CONFIG_KSZ_HSR
	if (sw->features & HSR_HW)
		header = HSR_HLEN;
#endif
	if (sw->features & SW_VLAN_DEV)
		if (header < VLAN_HLEN)
			header = VLAN_HLEN;
	mtu += header;
dbg_msg("mtu: %d\n", mtu);
	return mtu;
}  /* sw_get_mtu */

static int sw_get_tx_len(struct ksz_sw *sw, struct sk_buff *skb, uint port,
	int *header)
{
	int len = skb->len;
	int hlen = 0;

	if (sw->features & SW_VLAN_DEV)
		hlen = VLAN_HLEN;
#ifdef CONFIG_KSZ_HSR
	do {
		int i;

		i = sw->info->port_cfg[port].index;
		if (sw->eth_cnt && (sw->eth_maps[i].proto & HSR_HW))
			hlen = HSR_HLEN;
	} while (0);
#endif
	*header += hlen;
	if (!(sw->overrides & TAIL_TAGGING))
		return len;
	if (len < 60)
		len = 60;
	len += 1;
	return len;
}  /* sw_get_tx_len */

static void sw_add_tail_tag(struct ksz_sw *sw, struct sk_buff *skb, uint ports)
{
	u8 *trailer;
	int len = 1;

	trailer = skb_put(skb, len);
	trailer[0] = (u8) ports & sw->PORT_MASK;
}  /* sw_add_tail_tag */

static int sw_get_tail_tag(u8 *trailer, int *port)
{
	int len = 1;

	*port = *trailer;
	return len;
}  /* sw_get_tail_tag */

static void sw_add_vid(struct ksz_sw *sw, u16 vid)
{
	if ((sw->features & VLAN_PORT) && vid >= VLAN_PORT_START) {
		vid -= VLAN_PORT_START;
		if (vid <= SWITCH_PORT_NUM)
			sw->vlan_id |= (1 << vid);
	}
}  /* sw_add_vid */

static void sw_kill_vid(struct ksz_sw *sw, u16 vid)
{
	if ((sw->features & VLAN_PORT) && vid >= VLAN_PORT_START) {
		vid -= VLAN_PORT_START;
		if (vid <= SWITCH_PORT_NUM)
			sw->vlan_id &= ~(1 << vid);
	}
}  /* sw_kill_vid */

static int add_frag(void *from, char *to, int offset, int len, int odd,
	struct sk_buff *skb)
{
	memcpy(to + offset, from, len);
	return 0;
}

static struct sk_buff *sw_ins_vlan(struct ksz_sw *sw, uint port,
	struct sk_buff *skb)
{
	port = get_phy_port(sw, port);

#ifdef CONFIG_KSZ_HSR
	do {
		int i = sw->info->port_cfg[port].index;

		if (sw->eth_cnt && (sw->eth_maps[i].proto & HSR_HW))
			return skb;
	} while (0);
#endif

	/* Need to insert VLAN tag. */
	if (sw->dev_count > 1 && (sw->features & SW_VLAN_DEV)) {
		u16 vid;
		struct vlan_ethhdr *vlan;
		struct ethhdr *eth = (struct ethhdr *) skb->data;

		vid = sw->info->port_cfg[port].vid;
		vlan = (struct vlan_ethhdr *) skb_push(skb, VLAN_HLEN);
		memmove(vlan, eth, 12);
		vlan->h_vlan_TCI = htons(vid);
		vlan->h_vlan_proto = htons(ETH_P_8021Q);
	}
	return skb;
}  /* sw_ins_vlan */

#ifdef CONFIG_KSZ_HSR
static struct sk_buff *sw_ins_hsr(struct ksz_sw *sw, uint port,
	struct sk_buff *skb, u8 *ports)
{
	int i;
	uint p;

	p = get_phy_port(sw, n);
	i = sw->info->port_cfg[port].index;
	if (sw->eth_cnt && (sw->eth_maps[i].proto & HSR_HW)) {
		struct ksz_hsr_info *info = &sw->info->hsr;
		struct hsr_port *from =
			hsr_port_get_hsr(&info->hsr, HSR_PT_MASTER);

		if (!hsr_forward_skb(skb, from))
			return NULL;
		memcpy(&skb->data[6], info->src_addr, ETH_ALEN);
		*ports = info->member;
	}
	return skb;
}  /* sw_ins_hsr */
#endif

static struct sk_buff *sw_check_skb(struct ksz_sw *sw, struct sk_buff *skb,
	struct ksz_port *priv, void *ptr,
	int (*update_msg)(u8 *data, u32 port, u32 overrides))
{
	int len;
	uint port;
	u8 dest;
	struct sk_buff *org_skb;
	int update_dst = (sw->overrides & TAIL_TAGGING);

#ifdef CONFIG_1588_PTP
	struct ptp_info *ptp = ptr;

	if (ptp->overrides & (PTP_PORT_FORWARD | PTP_PORT_TX_FORWARD))
		update_dst |= 2;
#endif
	if (!update_dst)
		return sw_ins_vlan(sw, priv->first_port, skb);

#ifdef CONFIG_KSZ_STP
	if (skb->protocol == htons(STP_TAG_TYPE))
		return skb;
#endif
#ifdef CONFIG_KSZ_DLR
	if (skb->protocol == htons(DLR_TAG_TYPE))
		return skb;
#endif

	org_skb = skb;
	port = 0;

	/* This device is associated with a switch port. */
	if (1 == priv->port_cnt)
		port = priv->first_port;

	do {
		u16 prio;
		u16 vid;
		uint i;
		uint p;
		u32 features = sw->features;

		p = get_phy_port(sw, priv->first_port);
		i = sw->info->port_cfg[p].index;
		if (sw->features & SW_VLAN_DEV)
			features = sw->eth_maps[i].proto;
		if (!(features & VLAN_PORT) || port || vlan_get_tag(skb, &vid))
			break;
		prio = vid & VLAN_PRIO_MASK;
		vid &= VLAN_VID_MASK;
		if (vid < VLAN_PORT_START)
			break;
		vid -= VLAN_PORT_START;
		if (!vid || vid > SWITCH_PORT_NUM)
			break;
		port = vid;

		if (sw->vid || prio) {
			struct vlan_ethhdr *vlan =
				(struct vlan_ethhdr *) skb->data;
			u16 vlan_tci = ntohs(vlan->h_vlan_TCI);

			vlan_tci &= ~VLAN_VID_MASK;
			vlan_tci |= sw->vid;
			vlan->h_vlan_TCI = htons(vlan_tci);

		/* Need to remove VLAN tag manually. */
		} else if (!(sw->overrides & TAG_REMOVE)) {
			u8 *data;

			len = VLAN_ETH_HLEN - 2;
			data = &skb->data[len];
			memmove(data - VLAN_HLEN, data, skb->len - len);
			skb->len -= VLAN_HLEN;
		}
	} while (0);
#ifdef CONFIG_1588_PTP
	if (ptp) {
		int blocked;
		u32 dst = port;
		u32 overrides = ptp->overrides;

		if (!dst && ptp->version < 1)
			dst = 3;
		if (ptp->features & PTP_PDELAY_HACK) {
			dst |= (u32) sw->tx_ports << 16;
			overrides |= PTP_UPDATE_DST_PORT;
		}
		blocked = update_msg(skb->data, dst, overrides);
		if (blocked) {
			dev_kfree_skb_irq(skb);
			return NULL;
		}
	}
#endif
	if (!(sw->overrides & TAIL_TAGGING))
		return skb;

	dest = 0;
	if (port) {
		port = get_phy_port(sw, port);
		dest = 1 << port;
	}

	/* Socket buffer has no fragments. */
	if (!skb_shinfo(skb)->nr_frags) {
#ifdef NET_SKBUFF_DATA_USES_OFFSET
		len = skb_end_pointer(skb) - skb->data;
#else
		len = skb->end - skb->data;
#endif
		if (skb->len + 1 > len || len < 60 + 1) {
			len = (skb->len + 7) & ~3;
			if (len < 64)
				len = 64;
			skb = dev_alloc_skb(len);
			if (!skb)
				return NULL;
			memcpy(skb->data, org_skb->data, org_skb->len);
			skb->len = org_skb->len;
			copy_old_skb(org_skb, skb);
		}
		if (skb->len < 60) {
			memset(&skb->data[skb->len], 0, 60 - skb->len);
			skb->len = 60;
		}
		skb_set_tail_pointer(skb, skb->len);
		len = skb->len;
	}
	if (!dest) {
#ifdef CONFIG_KSZ_HSR
		skb = sw_ins_hsr(sw, priv->first_port, skb, &dest);
		if (!skb)
			return NULL;
		len = skb->len;
#endif
	}	
	if (!dest) {
		/* Use VLAN for port forwarding if not specified directly. */
		skb = sw_ins_vlan(sw, priv->first_port, skb);
		if (len != skb->len)
			len = skb->len;
	}
	if (!skb_shinfo(skb)->nr_frags) {
		skb->data[len] = dest;
		skb_put(skb, 1);
	} else {
		struct sock dummy;
		struct sock *sk;

		sk = skb->sk;
		if (!sk) {
			sk = &dummy;
			sk->sk_allocation = GFP_KERNEL;
			refcount_set(&sk->sk_wmem_alloc, 1);
		}

		/* Clear last tag. */
		sw->tx_pad[sw->tx_start] = 0;
		sw->tx_start = 0;
		len = 1;
		if (skb->len < 60) {
			sw->tx_start = 60 - skb->len;
			len += sw->tx_start;
		}
		sw->tx_pad[sw->tx_start] = dest;
		skb_append_datato_frags(sk, skb, add_frag, sw->tx_pad, len);
	}

	/* Need to compensate checksum for some devices. */
	if (skb->ip_summed != CHECKSUM_PARTIAL)
		dest = 0;
	if (dest && (sw->overrides & UPDATE_CSUM)) {
		__sum16 *csum_loc = (__sum16 *)
			(skb->head + skb->csum_start + skb->csum_offset);

		/* Checksum is cleared by driver to be filled by hardware. */
		if (!*csum_loc) {
			__sum16 new_csum;

			new_csum = dest << 8;
			*csum_loc = ~htons(new_csum);
		}
	}
	return skb;
}  /* sw_check_skb */

static struct sk_buff *sw_check_tx(struct ksz_sw *sw, struct net_device *dev,
	struct sk_buff *skb, struct ksz_port *priv)
{
	void *ptr = NULL;
	int (*update_msg)(u8 *data, u32 port, u32 overrides) = NULL;

#ifdef CONFIG_1588_PTP
	if (sw->features & PTP_HW) {
		struct ptp_info *ptp = &sw->ptp_hw;

		ptr = ptp;
		update_msg = ptp->ops->update_msg;
	}
#endif

	return sw_check_skb(sw, skb, priv, ptr, update_msg);
}  /* sw_check_tx */

static struct sk_buff *sw_final_skb(struct ksz_sw *sw, struct sk_buff *skb,
	struct net_device *dev, struct ksz_port *port)
{
	skb = sw->net_ops->check_tx(sw, dev, skb, port);
	if (!skb)
		return NULL;

#ifdef CONFIG_1588_PTP
	if (sw->features & PTP_HW) {
		struct ptp_info *ptp = &sw->ptp_hw;

		if (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP)
			ptp->ops->get_tx_tstamp(ptp, skb);
	}
#endif
	return skb;
}  /* sw_final_skb */

static void sw_start(struct ksz_sw *sw, u8 *addr)
{
	int need_tail_tag = false;
	int need_vlan = false;

	sw->ops->acquire(sw);
	sw_setup(sw);
	sw_enable(sw);
	sw_set_addr(sw, addr);
	if (1 == sw->dev_count)
		sw_setup_src_filter(sw, addr);
#ifdef CONFIG_KSZ_DLR
	if (sw->features & DLR_HW)
		need_tail_tag = true;
#endif
#ifdef CONFIG_KSZ_HSR
	if (sw->features & HSR_HW)
		need_tail_tag = true;
#endif
	if (sw->dev_count > 1 && !(sw->features & SW_VLAN_DEV))
		need_tail_tag = true;
	if (sw->features & VLAN_PORT) {
		if (sw->features & VLAN_PORT_REMOVE_TAG) {
			uint n;
			uint p;

			for (n = 0; n < SWITCH_PORT_NUM; n++) {
				p = n;
				port_cfg_rmv_tag(sw, p, true);
			}
			sw->overrides |= TAG_REMOVE;
		}
		if (sw->features & VLAN_PORT_TAGGING)
			need_tail_tag = true;
	} else if (sw->features & SW_VLAN_DEV) {
		struct ksz_dev_map *map;
		int i;
		int p;
		uint port;
		uint q;

		for (p = 0; p < sw->eth_cnt; p++) {

			/* Not really using VLAN. */
			if (1 == sw->eth_maps[p].vlan)
				continue;
			sw->ops->release(sw);

			map = &sw->eth_maps[p];

			/*
			 * Setting FID allows same MAC address in different
			 * VLANs.
			 */
			sw_w_vlan_table(sw, p + 1,
				map->vlan,
				map->vlan & (FID_ENTRIES - 1),
				sw->HOST_MASK | map->mask, true);
			sw->ops->acquire(sw);
			for (i = 0, q = map->first;
			     i < map->cnt; i++, q++) {
				port = get_phy_port(sw, q);
				sw_cfg_def_vid(sw, port, map->vlan);
				port_cfg_rmv_tag(sw, port, true);
			}
		}

		/* Use tail tag to determine the network device. */
		if (need_tail_tag)
			port_cfg_rmv_tag(sw, sw->HOST_PORT, true);
		else
			port_cfg_ins_tag(sw, sw->HOST_PORT, true);
		need_vlan = true;
	}
	if (sw->features & STP_SUPPORT)
		need_tail_tag = true;
	if (need_tail_tag) {
		sw_cfg_tail_tag(sw, true);
		sw->overrides |= TAIL_TAGGING;
	}
	if (need_vlan) {
		sw_ena_vlan(sw);
	}
	sw_ena_intr(sw);
	sw->ops->release(sw);

#ifdef CONFIG_KSZ_STP
	if (sw->features & STP_SUPPORT) {
		struct ksz_stp_info *stp = &sw->info->rstp;

		if (stp->br.bridgeEnabled)
			stp_start(stp);
	} else
		stp_set_addr(&sw->info->rstp, sw->info->mac_addr);
#endif

#ifdef CONFIG_1588_PTP
	if (sw->features & PTP_HW) {
		struct ptp_info *ptp = &sw->ptp_hw;

		ptp->reg->start(ptp, true);
	}
#endif
}  /* sw_start */

static int sw_stop(struct ksz_sw *sw, int complete)
{
	int reset = false;

#ifdef CONFIG_KSZ_STP
	if (sw->features & STP_SUPPORT) {
		struct ksz_stp_info *stp = &sw->info->rstp;

		if (stp->br.bridgeEnabled)
			stp_stop(stp, true);
	}
#endif

#ifdef CONFIG_1588_PTP
	if (sw->features & PTP_HW) {
		struct ptp_info *ptp = &sw->ptp_hw;

		reset = ptp->ops->stop(ptp, true);
	}
#endif
	sw->ops->acquire(sw);
	if (!reset)
		sw_reset(sw);
	reset = true;
	sw_init(sw);

	/* Clean out static MAC table when the switch shutdown. */
	if ((sw->features & STP_SUPPORT) && complete)
		sw_clr_sta_mac_table(sw);
	sw->ops->release(sw);
	return reset;
}  /* sw_stop */

static void sw_init_mib(struct ksz_sw *sw)
{
	unsigned long interval;
	uint n;
	uint p;

	interval = MIB_READ_INTERVAL * 2 / (sw->mib_port_cnt + 1);
	for (n = 0; n <= sw->mib_port_cnt; n++) {
		p = get_phy_port(sw, n);
		sw->port_mib[p].mib_start = 0;
		if (sw->next_jiffies < jiffies)
			sw->next_jiffies = jiffies + HZ * 2;
		else
			sw->next_jiffies += interval;
		sw->counter[p].time = sw->next_jiffies;
		sw->port_state[p].state = media_disconnected;
		port_init_cnt(sw, p);
	}
	sw->port_state[sw->HOST_PORT].state = media_connected;
}  /* sw_init_mib */

static int sw_open_dev(struct ksz_sw *sw, struct net_device *dev, u8 *addr)
{
	int mode = 0;

	sw_init_mib(sw);

	sw->main_dev = dev;
	sw->net_ops->start(sw, addr);
	if (sw->dev_count > 1)
		mode |= 1;
	if (sw->features & DIFF_MAC_ADDR)
		mode |= 2;
	return mode;
}  /* sw_open_dev */

static void sw_open_port(struct ksz_sw *sw, struct net_device *dev,
	struct ksz_port *port, u8 *state)
{
	uint i;
	uint n;
	uint p;
	struct ksz_port_info *info;

	/* Update in case it is changed. */
	if (dev->phydev)
		port->phydev = dev->phydev;
	for (i = 0, n = port->first_port; i < port->port_cnt; i++, n++) {
		p = get_phy_port(sw, n);
		info = get_port_info(sw, p);
		/*
		 * Initialize to invalid value so that link detection
		 * is done.
		 */
		info->partner = 0xFF;
		info->state = media_unknown;
		if (port->port_cnt == 1) {
			if (sw->netdev[0]) {
				struct ksz_port *sw_port = sw->netport[0];

				port->speed = sw_port->speed;
				port->duplex = sw_port->duplex;
				port->flow_ctrl = sw_port->flow_ctrl;
			}
			if (info->own_speed != port->speed ||
			    info->own_duplex != port->duplex) {
				if (info->own_speed)
					port->speed = info->own_speed;
				if (info->own_duplex)
					port->duplex = info->own_duplex;
			}
		}
	}
	info = get_port_info(sw, sw->HOST_PORT);
	info->report = true;

	sw->ops->acquire(sw);

	/* Need to open the port in multiple device interfaces mode. */
	if (sw->dev_count > 1 && (!sw->dev_offset || dev != sw->netdev[0])) {
		*state = STP_STATE_SIMPLE;
		if (sw->dev_offset && !(sw->features & STP_SUPPORT)) {
			*state = STP_STATE_FORWARDING;
		}
		if (sw->features & SW_VLAN_DEV) {
			p = get_phy_port(sw, port->first_port);
			i = sw->info->port_cfg[p].index;
			if (!(sw->eth_maps[i].proto & HSR_HW))
				*state = STP_STATE_FORWARDING;
		}
		for (i = 0, n = port->first_port; i < port->port_cnt;
		     i++, n++) {
			p = get_phy_port(sw, n);
			sw->dev_ports |= (1 << p);
#ifdef CONFIG_KSZ_STP
			if (sw->features & STP_SUPPORT) {
				stp_enable_port(&sw->info->rstp, p, state);
			}
#endif
			port_set_stp_state(sw, p, *state);
		}
		port_set_addr(sw, port->first_port, dev->dev_addr);
		port_cfg_src_filter_0(sw, port->first_port, 1);
		port_cfg_src_filter_1(sw, port->first_port, 1);
	} else if (sw->dev_count == 1) {
		sw->dev_ports = sw->PORT_MASK;
	}

	if (port->force_link)
		port_force_link_speed(port);
	else
		port_set_link_speed(port);
	port_get_link_speed(port);
	sw->ops->release(sw);

#ifdef CONFIG_KSZ_DLR
	if (sw->features & DLR_HW) {
		struct ksz_dlr_info *info = &sw->info->dlr;

		p = get_phy_port(sw, port->first_port);
		if (info->ports[0] == p)
			prep_dlr(info, sw->main_dev,
					sw->main_dev->dev_addr);
	}
#endif
#ifdef CONFIG_KSZ_HSR
	if (sw->features & HSR_HW) {
		struct ksz_hsr_info *info = &sw->info->hsr;

		p = get_phy_port(sw, port->first_port);
		if (info->ports[0] == p)
			prep_hsr(info, sw->main_dev,
					sw->main_dev->dev_addr);
	}
#endif
}  /* sw_open_port */

static void sw_close_port(struct ksz_sw *sw, struct net_device *dev,
	struct ksz_port *port)
{
	int i;
	uint p;

	/* Need to shut the port manually in multiple device interfaces mode. */
	if (sw->dev_count > 1 && (!sw->dev_offset || dev != sw->netdev[0])) {
		uint n;

		sw->ops->acquire(sw);
		for (i = 0, n = port->first_port; i < port->port_cnt;
		     i++, n++) {
			p = get_phy_port(sw, n);
#ifdef CONFIG_KSZ_STP
			if (sw->features & STP_SUPPORT)
				stp_disable_port(&sw->info->rstp, p);
#endif
#ifdef CONFIG_KSZ_MRP
			if (sw->features & MRP_SUPPORT) {
				mrp_close_port(mrp, p);
			}
#endif
			sw->dev_ports &= ~(1 << p);
			port_set_stp_state(sw, p, STP_STATE_DISABLED);
		}
		sw->ops->release(sw);
	} else if (sw->dev_count == 1) {
#ifdef CONFIG_KSZ_MRP
		uint n;

		if (sw->features & MRP_SUPPORT) {
			for (n = 0; n <= sw->mib_port_cnt; n++) {
				p = get_phy_port(sw, n);
				mrp_close_port(mrp, p);
			}
		}
#endif
		sw->dev_ports = 0;
	}
#ifdef CONFIG_KSZ_HSR
	if (sw->features & HSR_HW) {
		struct ksz_hsr_info *info = &sw->info->hsr;

		p = get_phy_port(sw, port->first_port);
		if (info->ports[0] == p)
			stop_hsr(info);
	}
#endif
}  /* sw_close_port */

static void sw_open(struct ksz_sw *sw)
{
#ifdef CONFIG_KSZ_DLR
	prep_dlr(&sw->info->dlr, sw->main_dev, sw->main_dev->dev_addr);
#endif
#ifdef CONFIG_1588_PTP_
	if (sw->features & PTP_HW) {
		int i;
		struct ptp_info *ptp = &sw->ptp_hw;

		for (i = 0; i < MAX_PTP_PORT; i++)
			ptp->linked[i] = (sw->port_info[i].state ==
				media_connected);
	}
#endif
	/* Timer may already be started by the SPI device. */
	if (!sw->monitor_timer_info->max)
		ksz_start_timer(sw->monitor_timer_info,
			sw->monitor_timer_info->period);
}  /* sw_open */

static void sw_close(struct ksz_sw *sw)
{
	ksz_stop_timer(sw->monitor_timer_info);
	cancel_delayed_work_sync(sw->link_read);
}  /* sw_close */

static u8 sw_set_mac_addr(struct ksz_sw *sw, struct net_device *dev,
	u8 promiscuous, uint port)
{
	sw->ops->acquire(sw);
	if (sw->dev_count > 1 && (!sw->dev_offset || dev != sw->netdev[0])) {
		port_set_addr(sw, port, dev->dev_addr);
		if (sw->features & DIFF_MAC_ADDR) {
			sw->features &= ~DIFF_MAC_ADDR;
			--promiscuous;
		}
		for (port = 0; port < SWITCH_PORT_NUM; port++)
			if (memcmp(sw->port_info[port].mac_addr,
					dev->dev_addr, ETH_ALEN)) {
				sw->features |= DIFF_MAC_ADDR;
				++promiscuous;
				break;
			}
	} else {
		int i;

		sw_setup_src_filter(sw, dev->dev_addr);

		/* Make MAC address the same in all the ports. */
		if (sw->dev_count > 1) {
			for (i = 0; i < SWITCH_PORT_NUM; i++)
				memcpy(sw->netdev[i + 1]->dev_addr,
					dev->dev_addr, ETH_ALEN);
			if (sw->features & DIFF_MAC_ADDR) {
				sw->features &= ~DIFF_MAC_ADDR;
				--promiscuous;
			}
		}
		if (dev == sw->netdev[0])
			sw_set_addr(sw, dev->dev_addr);
	}
	sw->ops->release(sw);
#ifdef CONFIG_KSZ_DLR
	dlr_change_addr(&sw->info->dlr, dev->dev_addr);
#endif
	return promiscuous;
}  /* sw_set_mac_addr */

static struct ksz_dev_major sw_majors[MAX_SW_DEVICES];

static struct file_dev_info *alloc_sw_dev_info(struct ksz_sw *sw, uint minor)
{
	struct file_dev_info *info;

	info = kzalloc(sizeof(struct file_dev_info), GFP_KERNEL);
	if (info) {
		info->dev = sw;
		sema_init(&info->sem, 1);
		mutex_init(&info->lock);
		init_waitqueue_head(&info->wait_msg);
		info->read_max = 60000;
		info->read_tmp = MAX_SW_LEN;
		info->read_buf = kzalloc(info->read_max + info->read_tmp,
			GFP_KERNEL);
		info->read_in = &info->read_buf[info->read_max];
		info->write_len = 1000;
		info->write_buf = kzalloc(info->write_len, GFP_KERNEL);

		info->minor = minor;
		info->next = sw->dev_list[minor];
		sw->dev_list[minor] = info;
	}
	return info;
}  /* alloc_sw_dev_info */

static void free_sw_dev_info(struct file_dev_info *info)
{
	if (info) {
		struct ksz_sw *sw = info->dev;
		uint minor = info->minor;

		file_dev_clear_notify(sw->dev_list[minor], info, DEV_MOD_BASE,
				      &sw->notifications);
		file_gen_dev_release(info, &sw->dev_list[minor]);
	}
}  /* free_sw_dev_info */

static int sw_dev_open(struct inode *inode, struct file *filp)
{
	struct file_dev_info *info = (struct file_dev_info *)
		filp->private_data;
	uint minor = MINOR(inode->i_rdev);
	uint major = MAJOR(inode->i_rdev);
	struct ksz_sw *sw = NULL;
	int i;

	if (minor > 1)
		return -ENODEV;
	for (i = 0; i < MAX_SW_DEVICES; i++) {
		if (sw_majors[i].major == major) {
			sw = sw_majors[i].dev;
			break;
		}
	}
	if (!sw)
		return -ENODEV;
	if (!info) {
		info = alloc_sw_dev_info(sw, minor);
		if (info)
			filp->private_data = info;
		else
			return -ENOMEM;
	}
	return 0;
}  /* sw_dev_open */

static int sw_dev_release(struct inode *inode, struct file *filp)
{
	struct file_dev_info *info = (struct file_dev_info *)
		filp->private_data;

	free_sw_dev_info(info);
	filp->private_data = NULL;
	return 0;
}  /* sw_dev_release */

static int sw_get_attrib(struct ksz_sw *sw, int subcmd, int size,
	int *req_size, size_t *len, u8 *data, int *output)
{
	struct ksz_info_opt *opt = (struct ksz_info_opt *) data;
	struct ksz_info_cfg *cfg = &opt->data.cfg;
	uint i;
	uint n;
	uint p;

	*len = 0;
	*output = 0;
	switch (subcmd) {
	case DEV_SW_CFG:
		n = opt->num;
		p = opt->port;
		if (!n)
			n = 1;
		*len = 2 + n * sizeof(struct ksz_info_cfg);
		if (n > sw->mib_port_cnt ||
		    p > sw->mib_port_cnt)
			return DEV_IOC_INVALID_CMD;
		break;
	}
	if (!*len)
		return DEV_IOC_INVALID_CMD;
	if (size < *len) {
		*req_size = *len + SIZEOF_ksz_request;
		return DEV_IOC_INVALID_LEN;
	}
	switch (subcmd) {
	case DEV_SW_CFG:
		n = opt->port;
		sw->ops->acquire(sw);
		for (i = 0; i < opt->num; i++, n++) {
			p = get_phy_port(sw, n);
			cfg->on_off = 0;
			if (cfg->set & SP_LEARN) {
				if (!port_chk_dis_learn(sw, p))
					cfg->on_off |= SP_LEARN;
			}
			if (cfg->set & SP_RX) {
				if (port_chk_rx(sw, p))
					cfg->on_off |= SP_RX;
			}
			if (cfg->set & SP_TX) {
				if (port_chk_tx(sw, p))
					cfg->on_off |= SP_TX;
			}
			if (p == sw->HOST_PORT)
				continue;
#if 0
			if (cfg->set & SP_PHY_POWER) {
				if (port_chk_power(sw, p))
					cfg->on_off |= SP_PHY_POWER;
			}
#endif
			cfg++;
		}
		sw->ops->release(sw);
		break;
	}
	return DEV_IOC_OK;
}  /* sw_get_attrib */

static int sw_set_attrib(struct ksz_sw *sw, int subcmd, int size,
	int *req_size, u8 *data, int *output)
{
	struct ksz_info_opt *opt = (struct ksz_info_opt *) data;
	struct ksz_info_cfg *cfg = &opt->data.cfg;
	int len;
	uint i;
	uint n;
	uint p;

	*output = 0;
	switch (subcmd) {
	case DEV_SW_CFG:
		n = opt->num;
		p = opt->port;
		if (!n)
			n = 1;
		len = 2 + n * sizeof(struct ksz_info_cfg);
		if (size < len)
			goto not_enough;
		if (n > sw->mib_port_cnt ||
		    p > sw->mib_port_cnt)
			return DEV_IOC_INVALID_CMD;
		n = opt->port;
		sw->ops->acquire(sw);
		for (i = 0; i < opt->num; i++, n++) {
			p = get_phy_port(sw, n);
			if (cfg->set & SP_LEARN)
				port_cfg_dis_learn(sw, p,
					!(cfg->on_off & SP_LEARN));
			if (cfg->set & SP_RX)
				port_cfg_rx(sw, p,
					!!(cfg->on_off & SP_RX));
			if (cfg->set & SP_TX)
				port_cfg_tx(sw, p,
					!!(cfg->on_off & SP_TX));
			if (p == sw->HOST_PORT)
				continue;
#if 0
			if (cfg->set & SP_PHY_POWER)
				port_cfg_power(sw, p,
					!!(cfg->on_off & SP_PHY_POWER));
#endif
			cfg++;
		}
		sw->ops->release(sw);
		break;
	default:
		return DEV_IOC_INVALID_CMD;
	}
	return DEV_IOC_OK;

not_enough:
	*req_size = len + SIZEOF_ksz_request;
	return DEV_IOC_INVALID_LEN;
}  /* sw_set_attrib */

static int sw_get_info(struct ksz_sw *sw, int subcmd, int size,
	int *req_size, size_t *len, u8 *data)
{
	struct ksz_info_opt *opt = (struct ksz_info_opt *) data;
	struct ksz_info_speed *speed = &opt->data.speed;
	struct ksz_port_info *info;
	uint i;
	uint n;
	uint p;

	*len = 0;
	switch (subcmd) {
	case DEV_INFO_SW_LINK:
		n = opt->num;
		p = opt->port;
		if (!n)
			n = 1;
		*len = 2 + n * sizeof(struct ksz_info_speed);
		if (n > sw->mib_port_cnt ||
		    p > sw->mib_port_cnt)
			return DEV_IOC_INVALID_CMD;
		break;
	}
	if (!*len)
		return DEV_IOC_INVALID_CMD;
	if (size < *len) {
		*req_size = *len + SIZEOF_ksz_request;
		return DEV_IOC_INVALID_LEN;
	}
	switch (subcmd) {
	case DEV_INFO_SW_LINK:
		n = p;
		for (i = 0; i < opt->num; i++, n++) {
			p = get_phy_port(sw, n);
			info = get_port_info(sw, p);
			if (info->state == media_connected) {
				speed->tx_rate = info->tx_rate;
				speed->duplex = info->duplex;
				speed->flow_ctrl = info->flow_ctrl;
			} else {
				memset(speed, 0, sizeof(struct ksz_info_speed));
			}
			++speed;
		}
		break;
	}
	return DEV_IOC_OK;
}  /* sw_get_info */

static int base_dev_req(struct ksz_sw *sw, char *arg, void *info)
{
	struct ksz_request *req = (struct ksz_request *) arg;
	int len;
	int maincmd;
	int req_size;
	int subcmd;
	int output;
	size_t param_size;
	u8 data[PARAM_DATA_SIZE];
	struct ksz_resp_msg *msg = (struct ksz_resp_msg *) data;
	int err = 0;
	int result = 0;

	get_user_data(&req_size, &req->size, info);
	get_user_data(&maincmd, &req->cmd, info);
	get_user_data(&subcmd, &req->subcmd, info);
	get_user_data(&output, &req->output, info);
	len = req_size - SIZEOF_ksz_request;

	maincmd &= 0xffff;
	switch (maincmd) {
	case DEV_CMD_INFO:
		switch (subcmd) {
		case DEV_INFO_INIT:
			req_size = SIZEOF_ksz_request + 4;
			if (len >= 4) {
				data[0] = 'M';
				data[1] = 'i';
				data[2] = 'c';
				data[3] = 'r';
				data[4] = 1;
				data[5] = sw->mib_port_cnt;
				err = write_user_data(data, req->param.data,
					6, info);
				if (err)
					goto dev_ioctl_done;
			} else
				result = DEV_IOC_INVALID_LEN;
			break;
		case DEV_INFO_EXIT:

		/* fall through */
		case DEV_INFO_QUIT:

			/* Not called through char device. */
			if (!info)
				break;
			file_dev_clear_notify(sw->dev_list[0], info,
					      DEV_MOD_BASE,
					      &sw->notifications);
			msg->module = DEV_MOD_BASE;
			msg->cmd = DEV_INFO_QUIT;
			msg->resp.data[0] = 0;
			file_dev_setup_msg(info, msg, 8, NULL, NULL);
			sw->notifications = 0;
			break;
		case DEV_INFO_NOTIFY:
			if (len >= 4) {
				struct file_dev_info *dev_info = info;
				uint *notify = (uint *) data;

				_chk_ioctl_size(len, 4, 0, &req_size, &result,
					&req->param, data, info);
				dev_info->notifications[DEV_MOD_BASE] =
					*notify;
				sw->notifications |= *notify;
			}
			break;
		case DEV_INFO_SW_LINK:
			if (_chk_ioctl_size(len, len, 0, &req_size, &result,
			    &req->param, data, info))
				goto dev_ioctl_resp;
			result = sw_get_info(sw, subcmd, len, &req_size,
					     &param_size, data);
			if (result)
				goto dev_ioctl_resp;
			err = write_user_data(data, req->param.data,
					      param_size, info);
			if (err)
				goto dev_ioctl_done;
			req_size = param_size + SIZEOF_ksz_request;
			break;
		default:
			result = DEV_IOC_INVALID_CMD;
			break;
		}
		break;
	case DEV_CMD_PUT:
		if (_chk_ioctl_size(len, len, 0, &req_size, &result,
		    &req->param, data, info))
			goto dev_ioctl_resp;
		result = sw_set_attrib(sw, subcmd, len, &req_size, data,
			&output);
		if (result)
			goto dev_ioctl_resp;
		put_user_data(&output, &req->output, info);
		break;
	case DEV_CMD_GET:
		if (_chk_ioctl_size(len, len, 0, &req_size, &result,
		    &req->param, data, info))
			goto dev_ioctl_resp;
		result = sw_get_attrib(sw, subcmd, len, &req_size,
			&param_size, data, &output);
		if (result)
			goto dev_ioctl_resp;
		err = write_user_data(data, req->param.data, param_size, info);
		if (err)
			goto dev_ioctl_done;
		req_size = param_size + SIZEOF_ksz_request;
		put_user_data(&output, &req->output, info);
		break;
	default:
		result = DEV_IOC_INVALID_CMD;
		break;
	}

dev_ioctl_resp:
	put_user_data(&req_size, &req->size, info);
	put_user_data(&result, &req->result, info);

	/* Return ERESTARTSYS so that the system call is called again. */
	if (result < 0)
		err = result;

dev_ioctl_done:
	return err;
}  /* base_dev_req */

static int sw_dev_req(struct ksz_sw *sw, char *arg,
	struct file_dev_info *info)
{
	struct ksz_request *req = (struct ksz_request *) arg;
	int maincmd;
	int req_size;
	int err = 0;
	int result = DEV_IOC_OK;

	/* Check request size. */
	get_user_data(&req_size, &req->size, info);
	if (chk_ioctl_size(req_size, SIZEOF_ksz_request, 0, &req_size,
	    &result, NULL, NULL))
		goto dev_ioctl_resp;

	result = -EOPNOTSUPP;
	get_user_data(&maincmd, &req->cmd, info);
	maincmd >>= 16;
	switch (maincmd) {
	case DEV_MOD_BASE:
		err = base_dev_req(sw, arg, info);
		result = 0;
		break;
#ifdef CONFIG_KSZ_DLR
	case DEV_MOD_DLR:
		if (sw->features & DLR_HW) {
			struct ksz_dlr_info *dlr = &sw->info->dlr;

			err = dlr->ops->dev_req(dlr, arg, info);
			result = 0;
		}
		break;
#endif
	default:
		break;
	}

	/* Processed by specific module. */
	if (!result)
		return err;
	if (result < 0)
		goto dev_ioctl_done;

dev_ioctl_resp:
	put_user_data(&req_size, &req->size, info);
	put_user_data(&result, &req->result, info);

dev_ioctl_done:

	/* Return ERESTARTSYS so that the system call is called again. */
	if (result < 0)
		err = result;

	return err;
}  /* sw_dev_req */

static ssize_t sw_dev_read(struct file *filp, char *buf, size_t count,
	loff_t *offp)
{
	struct file_dev_info *info = (struct file_dev_info *)
		filp->private_data;
	ssize_t result = 0;
	int rc;

	if (!info->read_len) {
		*offp = 0;
		rc = wait_event_interruptible(info->wait_msg,
			0 != info->read_len);

		/* Cannot continue if ERESTARTSYS. */
		if (rc < 0)
			return 0;
	}

	if (down_interruptible(&info->sem))
		return -ERESTARTSYS;

	mutex_lock(&info->lock);
	if (*offp >= info->read_len) {
		info->read_len = 0;
		count = 0;
		*offp = 0;
		goto dev_read_done;
	}

	if (*offp + count > info->read_len) {
		count = info->read_len - *offp;
		info->read_len = 0;
	}

	if (copy_to_user(buf, &info->read_buf[*offp], count)) {
		result = -EFAULT;
		goto dev_read_done;
	}
	if (info->read_len)
		*offp += count;
	else
		*offp = 0;
	result = count;

dev_read_done:
	mutex_unlock(&info->lock);
	up(&info->sem);
	return result;
}  /* sw_dev_read */

#ifdef HAVE_UNLOCKED_IOCTL
static long sw_dev_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
#else
static int sw_dev_ioctl(struct inode *inode, struct file *filp,
	unsigned int cmd, unsigned long arg)
#endif
{
	struct file_dev_info *info = (struct file_dev_info *)
		filp->private_data;
	struct ksz_sw *sw = info->dev;
	int err = 0;

	if (_IOC_TYPE(cmd) != DEV_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > DEV_IOC_MAX)
		return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void *) arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void *) arg, _IOC_SIZE(cmd));
	if (err) {
		printk(KERN_ALERT "err fault\n");
		return -EFAULT;
	}
	if (down_interruptible(&info->sem))
		return -ERESTARTSYS;

	err = sw_dev_req(sw, (char *) arg, info);
	up(&info->sem);
	return err;
}  /* sw_dev_ioctl */

static ssize_t sw_dev_write(struct file *filp, const char *buf, size_t count,
	loff_t *offp)
{
	struct file_dev_info *info = (struct file_dev_info *)
		filp->private_data;
	ssize_t result = 0;
	size_t size;
	int rc;

	if (!count)
		return result;

	if (down_interruptible(&info->sem))
		return -ERESTARTSYS;

	if (*offp >= info->write_len) {
		result = -ENOSPC;
		goto dev_write_done;
	}
	if (*offp + count > info->write_len)
		count = info->write_len - *offp;
	if (copy_from_user(info->write_buf, buf, count)) {
		result = -EFAULT;
		goto dev_write_done;
	}
	size = 0;
	result = size;
	rc = 0;
	if (rc)
		result = rc;

dev_write_done:
	up(&info->sem);
	return result;
}  /* sw_dev_write */

static const struct file_operations sw_dev_fops = {
	.read		= sw_dev_read,
	.write		= sw_dev_write,
#ifdef HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl	= sw_dev_ioctl,
#else
	.ioctl		= sw_dev_ioctl,
#endif
	.open		= sw_dev_open,
	.release	= sw_dev_release,
};

static struct class *sw_class[MAX_SW_DEVICES];

static int init_sw_dev(int id, int dev_major, char *dev_name)
{
	int result;

	result = register_chrdev(dev_major, dev_name, &sw_dev_fops);
	if (result < 0) {
		printk(KERN_WARNING "%s: can't get major %d\n", dev_name,
			dev_major);
		return result;
	}
	if (0 == dev_major)
		dev_major = result;
	sw_class[id] = class_create(THIS_MODULE, dev_name);
	if (IS_ERR(sw_class[id])) {
		unregister_chrdev(dev_major, dev_name);
		return -ENODEV;
	}
	device_create(sw_class[id], NULL, MKDEV(dev_major, 0), NULL, dev_name);
	return dev_major;
}  /* init_sw_dev */

static void exit_sw_dev(int id, int dev_major, char *dev_name)
{
	device_destroy(sw_class[id], MKDEV(dev_major, 0));
	class_destroy(sw_class[id]);
	unregister_chrdev(dev_major, dev_name);
}  /* exit_sw_dev */

static void sw_init_dev(struct ksz_sw *sw)
{
	sprintf(sw->dev_name, "sw_dev");
	if (sw->id)
		sprintf(sw->dev_name, "sw_dev_%u", sw->id);
	sw->dev_major = init_sw_dev(sw->id, 0, sw->dev_name);
	sw->msg_buf = kzalloc(MAX_SW_LEN, GFP_KERNEL);
	sw_majors[sw->id].dev = sw;
	sw_majors[sw->id].major = sw->dev_major;
}  /* sw_init_dev */

static void sw_exit_dev(struct ksz_sw *sw)
{
	kfree(sw->msg_buf);
	if (sw->dev_major >= 0)
		exit_sw_dev(sw->id, sw->dev_major, sw->dev_name);
}  /* sw_exit_dev */

static void link_update_work(struct work_struct *work)
{
	struct ksz_port *port =
		container_of(work, struct ksz_port, link_update);
	struct ksz_sw *sw = port->sw;
	struct net_device *dev;
	struct phy_device *phydev;
	struct ksz_port_info *info;
	uint i;
	uint p;
	int link;

	sw_notify_link_change(sw, port->link_ports);

	for (p = 0; p < SWITCH_PORT_NUM; p++) {
		i = p + 1;
		info = get_port_info(sw, p);

		if (!info->report)
			continue;
		info->report = false;
		phydev = sw->phy[i];
		phydev->link = (info->state == media_connected);
		phydev->speed = info->tx_rate / TX_RATE_UNIT;
		phydev->duplex = (info->duplex == 2);
	}

	info = port->linked;
	phydev = port->phydev;
	phydev->link = (info->state == media_connected);
	phydev->speed = info->tx_rate / TX_RATE_UNIT;
	phydev->duplex = (info->duplex == 2);
	dev = port->netdev;
	if (dev) {
		link = netif_carrier_ok(dev);
		if (link != phydev->link) {
			if (phydev->link)
				netif_carrier_on(dev);
			else
				netif_carrier_off(dev);
			if (netif_msg_link(sw))
				pr_info("%s link %s\n",
					dev->name,
					phydev->link ? "on" : "off");
		}
	}

#ifdef CONFIG_KSZ_STP
	if (sw->features & STP_SUPPORT) {
		struct ksz_stp_info *stp = &sw->info->rstp;

		stp->ops->link_change(stp, true);
	}
#endif
	port->link_ports = 0;

	/* The switch is always linked; speed and duplex are also fixed. */
	phydev = NULL;
	dev = sw->netdev[0];
	if (dev)
		phydev = dev->phydev;
	if (phydev) {
		int phy_link = 0;

		port = sw->netport[0];

		/* phydev settings may be changed by ethtool. */
		info = get_port_info(sw, sw->HOST_PORT);
		phydev->link = 1;
		phydev->speed = SPEED_100;
		phydev->duplex = 1;
		phydev->pause = 1;
		phy_link = (port->linked->state == media_connected);
		link = netif_carrier_ok(dev);
		if (link != phy_link) {
			if (netif_msg_link(sw))
				pr_info("%s link %s\n",
					dev->name,
					phy_link ? "on" : "off");
		}
		if (phydev->adjust_link && phydev->attached_dev &&
		    info->report) {
			phydev->adjust_link(phydev->attached_dev);
			info->report = false;
		}
		link = netif_carrier_ok(dev);
		if (link != phy_link) {
			if (phy_link)
				netif_carrier_on(dev);
			else
				netif_carrier_off(dev);
		}
	}

#ifdef CONFIG_KSZ_HSR
	if (sw->features & HSR_HW) {
		struct ksz_hsr_info *hsr = &sw->info->hsr;

		p = get_phy_port(sw, port->first_port);
		if (hsr->ports[0] <= port->first_port &&
		    port->first_port <= hsr->ports[1])
			hsr->ops->check_announce(hsr);
	}
#endif
}  /* link_update_work */

/*
 * This enables multiple network device mode for the switch, which contains at
 * least two physical ports.  Some users like to take control of the ports for
 * running Spanning Tree Protocol.  The driver will create an additional eth?
 * device for each port depending on the mode.
 *
 * Some limitations are the network devices cannot have different MTU and
 * multicast hash tables.
 */
static int multi_dev;

static int stp;

/*
 * This enables fast aging in the switch.  Not sure what situation requires
 * that.  However, fast aging is used to flush the dynamic MAC table when STP
 * support is enabled.
 */
static int fast_aging;

static void sw_setup_zone(struct ksz_sw *sw)
{
#ifdef CONFIG_KSZ_DLR
	sw->eth_maps[0].cnt = 2;
	sw->eth_maps[0].mask = 3;
	sw->eth_maps[0].port = 0;
	sw->eth_maps[0].phy_id = 1;
	sw->eth_maps[0].vlan = 1;
	sw->eth_maps[0].proto = DLR_HW;
	sw->eth_cnt = 1;
	sw->features |= DLR_HW;
#endif
#ifdef CONFIG_KSZ_HSR
	sw->eth_maps[0].cnt = 2;
	sw->eth_maps[0].mask = 3;
	sw->eth_maps[0].port = 0;
	sw->eth_maps[0].phy_id = 1;
	sw->eth_maps[0].vlan = 1;
	sw->eth_maps[0].proto = HSR_HW;
	sw->eth_cnt = 1;
	sw->features |= HSR_HW;
#endif
}  /* sw_setup_zone */

static void sw_setup_logical_ports(struct ksz_sw *sw)
{
	struct ksz_port_info *info;
	struct ksz_port_info *pinfo;
	uint l;
	uint m;
	uint n;
	uint p;

	for (n = 0; n < TOTAL_PORT_NUM; n++) {
		info = &sw->port_info[n];
		if (n) {
			p = n - 1;
			l = n;
			m = BIT(p);
		} else {
			p = sw->HOST_PORT;
			l = 0;
			m = BIT(p);
		}
		info->phy_p = p;
		info->phy_m = BIT(p);
		pinfo = &sw->port_info[p];
		pinfo->log_p = l;
		pinfo->log_m = m;
		pinfo->phy_id = l;
	}
	for (n = 0; n <= SWITCH_PORT_NUM; n++) {
		info = &sw->port_info[n];
		pinfo = &sw->port_info[info->phy_p];
dbg_msg("%d= %d:%02x %d:%02x %d:%02x %d %d\n", n,
info->phy_p, info->phy_m, info->log_p, info->log_m,
pinfo->log_p, pinfo->log_m, pinfo->phy_id, pinfo->fiber);
	}
}  /* sw_setup_logical_ports */

static int phy_offset;

static void sw_setup_special(struct ksz_sw *sw, int *port_cnt,
	int *mib_port_cnt, int *dev_cnt)
{
	phy_offset = 0;
	sw->dev_offset = 0;
	sw->phy_offset = 0;

	/* Multiple network device interfaces are required. */
	if (1 == sw->multi_dev) {
		sw->dev_count = SWITCH_PORT_NUM;
		sw->phy_offset = 1;
	} else if (2 == sw->multi_dev)
		sw->features |= VLAN_PORT | VLAN_PORT_TAGGING;
	else if (3 == sw->multi_dev) {
		sw->dev_count = SWITCH_PORT_NUM;
		sw->dev_offset = 1;
	} else if (4 == sw->multi_dev)
		sw->features |= VLAN_PORT;
	else if (5 == sw->multi_dev) {
		sw->dev_count = SWITCH_PORT_NUM;
		sw->dev_offset = 1;
		sw->features |= VLAN_PORT | VLAN_PORT_TAGGING;
	}

	/* Single network device has multiple ports. */
	if (1 == sw->dev_count) {
		*port_cnt = SWITCH_PORT_NUM;
		*mib_port_cnt = SWITCH_PORT_NUM;
	}
	if (1 == sw->multi_dev && sw->eth_cnt)
		sw->dev_count = sw->eth_cnt;
	*dev_cnt = sw->dev_count;
	if (3 == sw->multi_dev || 5 == sw->multi_dev)
		(*dev_cnt)++;
#ifdef CONFIG_1588_PTP
	if (sw->features & VLAN_PORT) {
		struct ptp_info *ptp = &sw->ptp_hw;

		ptp->overrides |= PTP_PORT_FORWARD;
	}
#endif
}  /* sw_setup_special */

static void sw_leave_dev(struct ksz_sw *sw)
{
	int i;

#ifdef CONFIG_KSZ_STP
	if (sw->features & STP_SUPPORT)
		leave_stp(&sw->info->rstp);
#endif
	for (i = 0; i < sw->dev_count; i++) {
		sw->netdev[i] = NULL;
		sw->netport[i] = NULL;
	}
	sw->eth_cnt = 0;
	sw->dev_count = 0;
	sw->dev_offset = 0;
	sw->phy_offset = 0;
}  /* sw_leave_dev */

static int sw_setup_dev(struct ksz_sw *sw, struct net_device *dev,
	char *dev_name, struct ksz_port *port, int i, int port_cnt,
	int mib_port_cnt)
{
	struct ksz_port_info *info;
	int cnt;
	uint n;
	uint p;
	uint pi;
	int phy_id;
	struct ksz_dev_map *map;

	if (!phy_offset)
		phy_offset = sw->phy_offset;

	/* dev_offset is ether 0 or 1. */
	p = i;
	if (p)
		p -= sw->dev_offset;

	if (sw->dev_offset) {
		/*
		 * First device associated with switch has been
		 * created.
		 */
		if (i)
			snprintf(dev->name, IFNAMSIZ, "%s.10%%d", dev_name);
		else {
			port_cnt = SWITCH_PORT_NUM;
			mib_port_cnt = SWITCH_PORT_NUM;
		}
	}

	map = &sw->eth_maps[i];
	if (1 == sw->multi_dev && sw->eth_cnt) {
		port_cnt = map->cnt;
		p = map->first - 1;
		mib_port_cnt = port_cnt;
	}

#ifdef CONFIG_KSZ_HSR
	if (sw->eth_cnt && (sw->eth_maps[i].proto & HSR_HW)) {
		port_cnt = sw->eth_maps[i].cnt;
		p = sw->eth_maps[i].port;
		mib_port_cnt = port_cnt;
		setup_hsr(&sw->info->hsr, dev);
		dev->hard_header_len += HSR_HLEN;
	}
#endif

	port->port_cnt = port_cnt;
	port->mib_port_cnt = mib_port_cnt;
	port->first_port = p + 1;
	port->flow_ctrl = PHY_FLOW_CTRL;

#ifdef CONFIG_KSZ_STP
	if (!i && sw->features & STP_SUPPORT)
		prep_stp_mcast(dev);
#endif

	/* Point to port under netdev. */
	if (phy_offset)
		phy_id = port->first_port + phy_offset - 1;
	else
		phy_id = 0;

#ifndef NO_PHYDEV
	/* Replace virtual port with one from network device. */
	do {
		struct phy_device *phydev;
		struct phy_priv *priv;
		struct sw_priv *hw_priv = container_of(sw, struct sw_priv, sw);

		if (hw_priv->bus)
			phydev = mdiobus_get_phy(hw_priv->bus, phy_id);
		else
			phydev = &sw->phy_map[phy_id];
		priv = phydev->priv;
		priv->port = port;
	} while (0);
#endif
	if (!phy_offset)
		phy_offset = 1;

	p = get_phy_port(sw, port->first_port);
	port->sw = sw;
	port->linked = get_port_info(sw, p);

	for (cnt = 0, n = port->first_port; cnt < port_cnt; cnt++, n++) {
		pi = get_phy_port(sw, n);
		info = get_port_info(sw, pi);
		info->port_id = pi;
		info->state = media_disconnected;
		sw->info->port_cfg[pi].index = i;
	}
	sw->netdev[i] = dev;
	sw->netport[i] = port;
	port->netdev = dev;
	port->phydev = sw->phy[phy_id];

	INIT_WORK(&port->link_update, link_update_work);

	if (sw->features & VLAN_PORT)
		dev->features |= NETIF_F_HW_VLAN_CTAG_FILTER;

	/* Needed for inserting VLAN tag. */
	if (sw->features & SW_VLAN_DEV)
		dev->hard_header_len += VLAN_HLEN;
dbg_msg("%s %d:%d phy:%d\n", __func__, port->first_port, port->port_cnt, phy_id);

	return phy_id;
}  /* sw_setup_dev */

static u8 sw_get_priv_state(struct net_device *dev)
{
	return STP_STATE_SIMPLE;
}

static void sw_set_priv_state(struct net_device *dev, u8 state)
{
}

static int netdev_chk_running(struct net_device *dev)
{
	return netif_running(dev);
}

static int netdev_chk_stopped(struct net_device *dev)
{
	return netif_running(dev) && netif_queue_stopped(dev);
}

static void netdev_start_queue(struct net_device *dev)
{
	netif_start_queue(dev);
}

static void netdev_stop_queue(struct net_device *dev)
{
	netif_stop_queue(dev);
}

static void netdev_wake_queue(struct net_device *dev)
{
	netif_wake_queue(dev);
}

static void sw_netdev_oper(struct ksz_sw *sw, struct net_device *dev,
	int (*netdev_chk)(struct net_device *dev),
	void (*netdev_oper)(struct net_device *dev))
{
	uint port;
	int dev_count = 1;

	dev_count = sw->dev_count + sw->dev_offset;
	if (dev_count <= 1) {
		netdev_oper(dev);
		return;
	}
	for (port = 0; port < dev_count; port++) {
		dev = sw->netdev[port];
		if (!dev)
			continue;
		if (!netdev_chk || netdev_chk(dev))
			netdev_oper(dev);
	}
}  /* sw_netdev_oper */

static void sw_netdev_open_port(struct ksz_sw *sw, struct net_device *dev)
{
	struct ksz_port *port;
	u8 state;
	int p;
	int dev_count = 1;

	dev_count = sw->dev_count + sw->dev_offset;
	if (dev_count <= 1) {
		port = sw->net_ops->get_priv_port(dev);
		state = sw->net_ops->get_state(dev);
		sw->net_ops->open_port(sw, dev, port, &state);
		sw->net_ops->set_state(dev, state);
		return;
	}
	for (p = 0; p < dev_count; p++) {
		dev = sw->netdev[p];
		if (!dev)
			continue;
		port = sw->net_ops->get_priv_port(dev);
		state = sw->net_ops->get_state(dev);
		sw->net_ops->open_port(sw, dev, port, &state);
		sw->net_ops->set_state(dev, state);
	}
}  /* sw_netdev_open_port */

static void sw_netdev_start_queue(struct ksz_sw *sw, struct net_device *dev)
{
	sw_netdev_oper(sw, dev, netdev_chk_running, netdev_start_queue);
}

static void sw_netdev_stop_queue(struct ksz_sw *sw, struct net_device *dev)
{
	sw_netdev_oper(sw, dev, netdev_chk_running, netdev_stop_queue);
}

static void sw_netdev_wake_queue(struct ksz_sw *sw, struct net_device *dev)
{
	sw_netdev_oper(sw, dev, netdev_chk_stopped, netdev_wake_queue);
}

static struct ksz_sw_net_ops sw_net_ops = {
	.setup_special		= sw_setup_special,
	.setup_dev		= sw_setup_dev,
	.leave_dev		= sw_leave_dev,
	.get_state		= sw_get_priv_state,
	.set_state		= sw_set_priv_state,

	.start			= sw_start,
	.stop			= sw_stop,
	.open_dev		= sw_open_dev,
	.open_port		= sw_open_port,
	.close_port		= sw_close_port,
	.open			= sw_open,
	.close			= sw_close,

	.netdev_start_queue	= sw_netdev_start_queue,
	.netdev_stop_queue	= sw_netdev_stop_queue,
	.netdev_wake_queue	= sw_netdev_wake_queue,
	.netdev_open_port	= sw_netdev_open_port,

	.set_mac_addr		= sw_set_mac_addr,

	.get_mtu		= sw_get_mtu,
	.get_tx_len		= sw_get_tx_len,
	.add_tail_tag		= sw_add_tail_tag,
	.get_tail_tag		= sw_get_tail_tag,
	.add_vid		= sw_add_vid,
	.kill_vid		= sw_kill_vid,
	.check_tx		= sw_check_tx,
	.rx_dev			= sw_rx_dev,
	.match_pkt		= sw_match_pkt,
	.parent_rx		= sw_parent_rx,
	.port_vlan_rx		= sw_port_vlan_rx,
	.final_skb		= sw_final_skb,
	.drv_rx			= sw_drv_rx,
	.set_multi		= sw_set_multi,

};

static struct ksz_sw_ops sw_ops = {
	.init			= sw_init_dev,
	.exit			= sw_exit_dev,
	.dev_req		= sw_dev_req,

	.get_phy_port		= get_phy_port,
	.get_log_port		= get_log_port,

	.acquire		= sw_acquire,
	.release		= sw_release,

	.chk			= sw_chk,
	.cfg			= sw_cfg,

	.port_get_link_speed	= port_get_link_speed,
	.port_set_link_speed	= port_set_link_speed,
	.port_force_link_speed	= port_force_link_speed,

	.port_r_cnt		= port_r_cnt,
	.get_mib_counters	= get_sw_mib_counters,

	.sysfs_read		= sysfs_sw_read,
	.sysfs_read_hw		= sysfs_sw_read_hw,
	.sysfs_write		= sysfs_sw_write,
	.sysfs_port_read	= sysfs_port_read,
	.sysfs_port_read_hw	= sysfs_port_read_hw,
	.sysfs_port_write	= sysfs_port_write,
	.sysfs_mac_read		= sysfs_mac_read,
	.sysfs_mac_write	= sysfs_mac_write,
	.sysfs_vlan_read	= sysfs_vlan_read,
	.sysfs_vlan_write	= sysfs_vlan_write,

#ifdef CONFIG_KSZ_STP
	.sysfs_stp_read		= sysfs_stp_read,
	.sysfs_stp_write	= sysfs_stp_write,
	.sysfs_stp_port_read	= sysfs_stp_port_read,
	.sysfs_stp_port_write	= sysfs_stp_port_write,
#endif

	.cfg_mac		= sw_cfg_mac,
	.cfg_vlan		= sw_cfg_vlan,
	.alloc_mac		= sw_alloc_mac,
	.free_mac		= sw_free_mac,
	.alloc_vlan		= sw_alloc_vlan,
	.free_vlan		= sw_free_vlan,
	.alloc_fid		= sw_alloc_fid,
	.free_fid		= sw_free_fid,

	.get_br_id		= sw_get_br_id,
	.from_backup		= sw_from_backup,
	.to_backup		= sw_to_backup,
	.from_designated	= sw_from_designated,
	.to_designated		= sw_to_designated,
	.tc_detected		= sw_tc_detected,
	.get_tcDetected		= sw_get_tcDetected,

	.cfg_src_filter		= sw_cfg_src_filter,
	.flush_table		= sw_flush_dyn_mac_table,

};
/* -------------------------------------------------------------------------- */

/* debugfs code */
static int state_show(struct seq_file *seq, void *v)
{
	int i;
	int j;
	SW_D data[16 / SW_SIZE];
	struct sw_priv *priv = seq->private;
	struct ksz_sw *sw = &priv->sw;

	for (i = 0; i < 0x100; i += 16) {
		seq_printf(seq, SW_SIZE_STR":\t", i);
		mutex_lock(&priv->lock);
		for (j = 0; j < 16 / SW_SIZE; j++)
			data[j] = sw->reg->r(sw, i + j * SW_SIZE);
		mutex_unlock(&priv->lock);
		for (j = 0; j < 16 / SW_SIZE; j++)
			seq_printf(seq, SW_SIZE_STR" ", data[j]);
		seq_printf(seq, "\n");
	}
	return 0;
}

static int state_open(struct inode *inode, struct file *file)
{
	return single_open(file, state_show, inode->i_private);
}

static const struct file_operations state_fops = {
	.owner	= THIS_MODULE,
	.open	= state_open,
	.read	= seq_read,
	.llseek	= seq_lseek,
	.release = single_release,
};

/**
 * create_debugfs - create debugfs directory and files
 * @priv:	The switch device structure.
 *
 * Create the debugfs entries for the specific device.
 */
static void create_debugfs(struct sw_priv *priv)
{
	struct dentry *root;
	char root_name[32];

	snprintf(root_name, sizeof(root_name), "%s",
		 dev_name(priv->dev));

	root = debugfs_create_dir(root_name, NULL);
	if (IS_ERR(root)) {
		pr_err("cannot create debugfs root\n");
		return;
	}

	priv->debug_root = root;
	priv->debug_file = debugfs_create_file("state", 0444, root,
		priv, &state_fops);
	if (IS_ERR(priv->debug_file))
		pr_err("cannot create debugfs state file\n");
}

static void delete_debugfs(struct sw_priv *priv)
{
	debugfs_remove(priv->debug_file);
	debugfs_remove(priv->debug_root);
}

/* -------------------------------------------------------------------------- */

#define USE_SPEED_LINK
#define USE_MIB
#include "ksz_sw_sysfs.c"

#ifdef CONFIG_1588_PTP
#include "ksz_ptp_sysfs.c"
#endif

#ifndef NO_PHYDEV
static irqreturn_t sw_interrupt(int irq, void *phy_dat)
{
	struct sw_priv *ks = phy_dat;

	ks->sw.intr_using += 1;
	ks->irq_work.func(&ks->irq_work);

	return IRQ_HANDLED;
}  /* sw_interrupt */

static void sw_change(struct work_struct *work)
{
	struct sw_priv *ks =
		container_of(work, struct sw_priv, irq_work);
	struct ksz_sw *sw = &ks->sw;
	SW_D status;

	ks->intr_working = true;
	mutex_lock(&ks->hwlock);
	mutex_lock(&ks->lock);
	sw->intr_using++;
	status = sw_proc_intr(sw);
	sw->intr_using--;
	mutex_unlock(&ks->lock);
	if (status) {
		mutex_lock(&ks->lock);
		sw->reg->w(sw, REG_INT_STATUS, status);
		mutex_unlock(&ks->lock);
	}
	mutex_unlock(&ks->hwlock);
	sw->intr_using = 0;
}  /* sw_change */

static int sw_start_interrupt(struct sw_priv *ks, const char *name)
{
	int err = 0;

	INIT_WORK(&ks->irq_work, sw_change);

	err = request_threaded_irq(ks->irq, NULL, sw_interrupt,
		ks->intr_mode, name, ks);
	if (err < 0) {
		printk(KERN_WARNING "%s: Can't get IRQ %d (PHY)\n",
			name,
			ks->irq);
		ks->irq = 0;
	}

	return err;
}  /* sw_start_interrupt */

static void sw_stop_interrupt(struct sw_priv *ks)
{
	free_irq(ks->irq, ks);
	cancel_work_sync(&ks->irq_work);
}  /* sw_stop_interrupt */
#endif

/* -------------------------------------------------------------------------- */

static void sw_init_phy_priv(struct sw_priv *ks)
{
	struct phy_priv *phydata;
	struct ksz_port *port;
	struct ksz_sw *sw = &ks->sw;
	uint n;
	uint p;

	for (n = 0; n <= sw->mib_port_cnt + 1; n++) {
		phydata = &sw->phydata[n];
		port = &ks->ports[n];
		phydata->port = port;
		port->sw = sw;
		port->phydev = &sw->phy_map[n];
		port->flow_ctrl = PHY_FLOW_CTRL;
		port->port_cnt = 1;
		port->mib_port_cnt = 1;
		p = n;
		if (!n) {
			port->port_cnt = sw->mib_port_cnt;
			port->mib_port_cnt = sw->mib_port_cnt;
			p = 1;
		}
		port->first_port = p;
		p = get_phy_port(sw, p);
		port->linked = get_port_info(sw, p);
dbg_msg(" %s %d=p:%d; f:%d c:%d i:%d\n", __func__, n, p,
port->first_port, port->port_cnt, port->linked->phy_id);
		INIT_WORK(&port->link_update, link_update_work);
		sw->phy_map[n].priv = phydata;
	}
}  /* sw_init_phy_priv */

#ifndef NO_PHYDEV
static void sw_init_phydev(struct ksz_sw *sw, struct phy_device *phydev)
{
	struct ksz_port_info *info = get_port_info(sw, sw->HOST_PORT);

	phydev->interface = sw->interface;
	phydev->link = 1;
	phydev->speed = info->tx_rate / TX_RATE_UNIT;
	phydev->duplex = (info->duplex == 2);
	phydev->pause = 1;
}  /* sw_init_phydev */

#ifdef CONFIG_HAVE_KSZ8463
#define KSZ8463_ID_HI		0x0022

#define KSZ8463_SW_ID		0x8463
#define PHY_ID_KSZ_SW		((KSZ8463_ID_HI << 16) | KSZ8463_SW_ID)

static char *kszsw_phy_driver_names[] = {
	"Microchip KSZ8463 Switch",
};
#endif

#ifdef CONFIG_HAVE_KSZ8863
#define KSZ8863_SW_ID		0x8863
#define PHY_ID_KSZ_SW		((KSZ8863_ID_HI << 16) | KSZ8863_SW_ID)

static char *kszsw_phy_driver_names[] = {
	"Microchip KSZ8863 Switch",
	"Microchip KSZ8873 Switch",
};
#endif

static int kszphy_config_init(struct phy_device *phydev)
{
	return 0;
}

static struct phy_driver kszsw_phy_driver = {
	.phy_id		= PHY_ID_KSZ_SW,
	.phy_id_mask	= 0x00ffffff,
	.name		= "Microchip KSZ8463 Switch",
	.features	= (PHY_BASIC_FEATURES |	SUPPORTED_Pause),
	.flags		= PHY_HAS_INTERRUPT,
	.config_init	= kszphy_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
};

#ifdef CONFIG_HAVE_KSZ8463
/*
 * Tha  2011/03/11
 * The hardware register reads low word first of PHY id instead of high word.
 */
static inline int actual_reg(int regnum)
{
	if (2 == regnum)
		regnum = 3;
	else if (3 == regnum)
		regnum = 2;
	return regnum;
}

/**
 * sw_r_phy - read data from PHY register
 * @sw:		The switch instance.
 * @phy:	PHY address to read.
 * @reg:	PHY register to read.
 * @val:	Buffer to store the read data.
 *
 * This routine reads data from the PHY register.
 */
static void sw_r_phy(struct ksz_sw *sw, u16 phy, u16 reg, u16 *val)
{
	u16 base;
	u16 data = 0;

	reg = actual_reg(reg);
	if (2 == phy)
		base = PHY2_REG_CTRL;
	else
		base = PHY1_REG_CTRL;
	data = sw->reg->r16(sw, base + reg * 2);
	if (2 == reg)
		data = KSZ8463_SW_ID;
	*val = data;
}  /* sw_r_phy */

/**
 * sw_w_phy - write data to PHY register
 * @hw:		The switch instance.
 * @phy:	PHY address to write.
 * @reg:	PHY register to write.
 * @val:	Word data to write.
 *
 * This routine writes data to the PHY register.
 */
static void sw_w_phy(struct ksz_sw *sw, u16 phy, u16 reg, u16 val)
{
	u16 base;

	if (2 == phy)
		base = PHY2_REG_CTRL;
	else
		base = PHY1_REG_CTRL;
	sw->reg->w16(sw, base + reg * 2, val);
}  /* sw_w_phy */
#endif

#ifdef CONFIG_HAVE_KSZ8863
/**
 * sw_r_phy - read data from PHY register
 * @sw:		The switch instance.
 * @phy:	PHY address to read.
 * @reg:	PHY register to read.
 * @val:	Buffer to store the read data.
 *
 * This routine reads data from the PHY register.
 */
static void sw_r_phy(struct ksz_sw *sw, u16 phy, u16 reg, u16 *val)
{
	u8 ctrl;
	u8 restart;
	u8 link;
	u8 speed;
	u8 p = phy - 1;
	u16 data = 0;

	switch (reg) {
	case PHY_REG_CTRL:
		port_r(sw, p, P_PHY_CTRL, &ctrl);
		port_r(sw, p, P_NEG_RESTART_CTRL, &restart);
		port_r(sw, p, P_SPEED_STATUS, &speed);
		if (restart & PORT_LOOPBACK)
			data |= PHY_LOOPBACK;
		if (ctrl & PORT_FORCE_100_MBIT)
			data |= PHY_SPEED_100MBIT;
		if (ctrl & PORT_AUTO_NEG_ENABLE)
			data |= PHY_AUTO_NEG_ENABLE;
		if (restart & PORT_POWER_DOWN)
			data |= PHY_POWER_DOWN;
		if (restart & PORT_AUTO_NEG_RESTART)
			data |= PHY_AUTO_NEG_RESTART;
		if (ctrl & PORT_FORCE_FULL_DUPLEX)
			data |= PHY_FULL_DUPLEX;
		if (speed & PORT_HP_MDIX)
			data |= PHY_HP_MDIX;
		if (restart & PORT_FORCE_MDIX)
			data |= PHY_FORCE_MDIX;
		if (restart & PORT_AUTO_MDIX_DISABLE)
			data |= PHY_AUTO_MDIX_DISABLE;
		if (restart & PORT_REMOTE_FAULT_DISABLE)
			data |= PHY_REMOTE_FAULT_DISABLE;
		if (restart & PORT_TX_DISABLE)
			data |= PHY_TRANSMIT_DISABLE;
		if (restart & PORT_LED_OFF)
			data |= PHY_LED_DISABLE;
		break;
	case PHY_REG_STATUS:
		port_r(sw, p, P_LINK_STATUS, &link);
		port_r(sw, p, P_SPEED_STATUS, &speed);
		data = PHY_100BTX_FD_CAPABLE |
			PHY_100BTX_CAPABLE |
			PHY_10BT_FD_CAPABLE |
			PHY_10BT_CAPABLE |
			PHY_AUTO_NEG_CAPABLE;
		if (link & PORT_AUTO_NEG_COMPLETE)
			data |= PHY_AUTO_NEG_ACKNOWLEDGE;
		if (link & PORT_STATUS_LINK_GOOD)
			data |= PHY_LINK_STATUS;
		if (speed & PORT_REMOTE_FAULT)
			data |= PHY_REMOTE_FAULT;
		break;
	case PHY_REG_ID_1:
		data = 0x0022;
		break;
	case PHY_REG_ID_2:
		/* Use unique switch id to differentiate from regular PHY. */
		data = KSZ8863_SW_ID;
		break;
	case PHY_REG_AUTO_NEGOTIATION:
		port_r(sw, p, P_PHY_CTRL, &ctrl);
		data = PHY_AUTO_NEG_802_3;
		if (ctrl & PORT_AUTO_NEG_SYM_PAUSE)
			data |= PHY_AUTO_NEG_SYM_PAUSE;
		if (ctrl & PORT_AUTO_NEG_100BTX_FD)
			data |= PHY_AUTO_NEG_100BTX_FD;
		if (ctrl & PORT_AUTO_NEG_100BTX)
			data |= PHY_AUTO_NEG_100BTX;
		if (ctrl & PORT_AUTO_NEG_10BT_FD)
			data |= PHY_AUTO_NEG_10BT_FD;
		if (ctrl & PORT_AUTO_NEG_10BT)
			data |= PHY_AUTO_NEG_10BT;
		break;
	case PHY_REG_REMOTE_CAPABILITY:
		port_r(sw, p, P_LINK_STATUS, &link);
		data = PHY_AUTO_NEG_802_3;
		if (link & PORT_REMOTE_SYM_PAUSE)
			data |= PHY_AUTO_NEG_SYM_PAUSE;
		if (link & PORT_REMOTE_100BTX_FD)
			data |= PHY_AUTO_NEG_100BTX_FD;
		if (link & PORT_REMOTE_100BTX)
			data |= PHY_AUTO_NEG_100BTX;
		if (link & PORT_REMOTE_10BT_FD)
			data |= PHY_AUTO_NEG_10BT_FD;
		if (link & PORT_REMOTE_10BT)
			data |= PHY_AUTO_NEG_10BT;
		break;
	default:
		break;
	}
	*val = data;
}  /* sw_r_phy */

/**
 * sw_w_phy - write data to PHY register
 * @hw:		The switch instance.
 * @phy:	PHY address to write.
 * @reg:	PHY register to write.
 * @val:	Word data to write.
 *
 * This routine writes data to the PHY register.
 */
static void sw_w_phy(struct ksz_sw *sw, u16 phy, u16 reg, u16 val)
{
	u8 ctrl;
	u8 restart;
	u8 speed;
	u8 data;
	u8 p = phy - 1;

	switch (reg) {
	case PHY_REG_CTRL:
		port_r(sw, p, P_SPEED_STATUS, &speed);
		data = speed;
		if (val & PHY_HP_MDIX)
			data |= PORT_HP_MDIX;
		else
			data &= ~PORT_HP_MDIX;
		if (data != speed)
			port_w(sw, p, P_SPEED_STATUS, data);
		port_r(sw, p, P_PHY_CTRL, &ctrl);
		data = ctrl;
		if (val & PHY_AUTO_NEG_ENABLE)
			data |= PORT_AUTO_NEG_ENABLE;
		else
			data &= ~PORT_AUTO_NEG_ENABLE;
		if (val & PHY_SPEED_100MBIT)
			data |= PORT_FORCE_100_MBIT;
		else
			data &= ~PORT_FORCE_100_MBIT;
		if (val & PHY_FULL_DUPLEX)
			data |= PORT_FORCE_FULL_DUPLEX;
		else
			data &= ~PORT_FORCE_FULL_DUPLEX;
		if (data != ctrl)
			port_w(sw, p, P_PHY_CTRL, data);
		port_r(sw, p, P_NEG_RESTART_CTRL, &restart);
		data = restart;
		if (val & PHY_LED_DISABLE)
			data |= PORT_LED_OFF;
		else
			data &= ~PORT_LED_OFF;
		if (val & PHY_TRANSMIT_DISABLE)
			data |= PORT_TX_DISABLE;
		else
			data &= ~PORT_TX_DISABLE;
		if (val & PHY_AUTO_NEG_RESTART)
			data |= PORT_AUTO_NEG_RESTART;
		else
			data &= ~(PORT_AUTO_NEG_RESTART);
		if (val & PHY_REMOTE_FAULT_DISABLE)
			data |= PORT_REMOTE_FAULT_DISABLE;
		else
			data &= ~PORT_REMOTE_FAULT_DISABLE;
		if (val & PHY_POWER_DOWN)
			data |= PORT_POWER_DOWN;
		else
			data &= ~PORT_POWER_DOWN;
		if (val & PHY_AUTO_MDIX_DISABLE)
			data |= PORT_AUTO_MDIX_DISABLE;
		else
			data &= ~PORT_AUTO_MDIX_DISABLE;
		if (val & PHY_FORCE_MDIX)
			data |= PORT_FORCE_MDIX;
		else
			data &= ~PORT_FORCE_MDIX;
		if (val & PHY_LOOPBACK)
			data |= PORT_LOOPBACK;
		else
			data &= ~PORT_LOOPBACK;
		if (data != restart)
			port_w(sw, p, P_NEG_RESTART_CTRL, data);
		break;
	case PHY_REG_AUTO_NEGOTIATION:
		port_r(sw, p, P_PHY_CTRL, &ctrl);
		data = ctrl;
		data &= ~(PORT_AUTO_NEG_SYM_PAUSE |
			PORT_AUTO_NEG_100BTX_FD |
			PORT_AUTO_NEG_100BTX |
			PORT_AUTO_NEG_10BT_FD |
			PORT_AUTO_NEG_10BT);
		if (val & PHY_AUTO_NEG_SYM_PAUSE)
			data |= PORT_AUTO_NEG_SYM_PAUSE;
		if (val & PHY_AUTO_NEG_100BTX_FD)
			data |= PORT_AUTO_NEG_100BTX_FD;
		if (val & PHY_AUTO_NEG_100BTX)
			data |= PORT_AUTO_NEG_100BTX;
		if (val & PHY_AUTO_NEG_10BT_FD)
			data |= PORT_AUTO_NEG_10BT_FD;
		if (val & PHY_AUTO_NEG_10BT)
			data |= PORT_AUTO_NEG_10BT;
		if (data != ctrl)
			port_w(sw, p, P_PHY_CTRL, data);
		break;
	default:
		break;
	}
}  /* sw_w_phy */
#endif

static int ksz_mii_read(struct mii_bus *bus, int phy_id, int regnum)
{
	struct sw_priv *ks = bus->priv;
	struct ksz_sw *sw = &ks->sw;
	int ret = 0;

	if (phy_id > SWITCH_PORT_NUM)
		return 0xffff;

	mutex_lock(&ks->lock);
	if (regnum < 6) {
		u16 data;
		struct ksz_port *port;

		port = &ks->ports[phy_id];

		/* Not initialized during registration. */
		if (sw->phy[phy_id]) {
			struct phy_priv *phydata;

			phydata = sw->phy[phy_id]->priv;
			port = phydata->port;
		}
		phy_id = port->linked->phy_id;
		sw_r_phy(&ks->sw, phy_id, regnum, &data);
		ret = data;
	}
	mutex_unlock(&ks->lock);
	return ret;
}  /* ksz_mii_read */

static int ksz_mii_write(struct mii_bus *bus, int phy_id, int regnum, u16 val)
{
	struct sw_priv *ks = bus->priv;

	if (phy_id > SWITCH_PORT_NUM)
		return -EINVAL;

	mutex_lock(&ks->lock);
	if (regnum < 6) {
		uint i;
		uint p;
		int first;
		int last;
		struct ksz_sw *sw = &ks->sw;

		if (0 == phy_id) {
			first = 1;
			last = SWITCH_PORT_NUM;
		} else {
			int n;
			int f;
			int l;
			struct ksz_dev_map *map;

			first = phy_id;
			last = phy_id;
			for (n = 0; n < sw->eth_cnt; n++) {
				map = &sw->eth_maps[n];
				f = map->first;
				l = f + map->cnt - 1;
				if (f <= phy_id && phy_id < l) {
					first = map->first;
					last = first + map->cnt + 1;
					break;
				}
			}
dbg_msg(" %d f:%d l:%d\n", phy_id, first, last);
		}

		/* PHY device driver resets or powers down the PHY. */
		if (0 == regnum &&
#ifdef CONFIG_HAVE_KSZ8463
		    (val & (PHY_RESET | PHY_POWER_DOWN)))
#else
		    (val & (PHY_RESET_NOT | PHY_POWER_DOWN)))
#endif
			goto done;
		for (i = first; i <= last; i++) {
			p = i;
			sw_w_phy(sw, p, regnum, val);
		}
		if (PHY_REG_CTRL == regnum &&
		    !(val & PHY_AUTO_NEG_ENABLE))
			schedule_delayed_work(&ks->link_read, 1);
	}
done:
	mutex_unlock(&ks->lock);
	return 0;
}  /* ksz_mii_write */

static int driver_installed;

static int ksz_mii_init(struct sw_priv *ks)
{
	struct platform_device *pdev;
	struct mii_bus *bus;
	struct phy_device *phydev;
	int err;
	int i;

	pdev = platform_device_register_simple("Switch MII bus", ks->sw.id,
		NULL, 0);
	if (!pdev)
		return -ENOMEM;

	bus = mdiobus_alloc();
	if (bus == NULL) {
		err = -ENOMEM;
		goto mii_init_reg;
	}

	if (!driver_installed) {
		kszsw_phy_driver.name =
			kszsw_phy_driver_names[ks->sw.chip_id];
		err = phy_driver_register(&kszsw_phy_driver, THIS_MODULE);
		if (err)
			goto mii_init_free_mii_bus;
		driver_installed = true;
	}

	bus->name = "Switch MII bus",
	bus->read = ksz_mii_read;
	bus->write = ksz_mii_write;
	snprintf(bus->id, MII_BUS_ID_SIZE, "sw.%d", ks->sw.id);
	bus->parent = &pdev->dev;
	bus->phy_mask = ~((1 << (ks->sw.mib_port_cnt + 2)) - 1);
	bus->priv = ks;

	for (i = 0; i < PHY_MAX_ADDR; i++)
		bus->irq[i] = ks->irq;

	err = mdiobus_register(bus);
	if (err < 0)
		goto mii_init_free_mii_bus;

	for (i = 0; i < PHY_MAX_ADDR; i++) {
		phydev = mdiobus_get_phy(bus, i);
		if (phydev) {
			struct phy_priv *priv = &ks->sw.phydata[i];

			priv->state = phydev->state;
			phydev->priv = priv;
		}
	}

	ks->bus = bus;
	ks->pdev = pdev;
	phydev = mdiobus_get_phy(bus, 0);
	ks->phydev = phydev;
	sw_init_phydev(&ks->sw, phydev);

	return 0;

mii_init_free_mii_bus:
	if (driver_installed) {
		phy_driver_unregister(&kszsw_phy_driver);
		driver_installed = false;
	}
	mdiobus_free(bus);

mii_init_reg:
	platform_device_unregister(pdev);

	return err;
}  /* ksz_mii_init */

static void ksz_mii_exit(struct sw_priv *ks)
{
	int i;
	struct phy_device *phydev;
	struct platform_device *pdev = ks->pdev;
	struct mii_bus *bus = ks->bus;

	if (ks->irq > 0) {
		struct ksz_sw *sw = &ks->sw;

		mutex_lock(&ks->lock);
		sw_dis_intr(sw);
		mutex_unlock(&ks->lock);
		sw_stop_interrupt(ks);
	}
	for (i = 0; i < PHY_MAX_ADDR; i++) {
		phydev = mdiobus_get_phy(bus, i);
		if (phydev) {
			struct ksz_port *port;

			port = &ks->ports[i];
			flush_work(&port->link_update);
		}
	}
	mdiobus_unregister(bus);
	if (driver_installed) {
		phy_driver_unregister(&kszsw_phy_driver);
		driver_installed = false;
	}
	mdiobus_free(bus);
	platform_device_unregister(pdev);
}  /* ksz_mii_exit */
#endif

/* driver bus management functions */

static void determine_rate(struct ksz_sw *sw, struct ksz_port_mib *mib)
{
	int j;

	for (j = 0; j < 2; j++) {
		if (mib->rate[j].last) {
			int offset;
			u64 cnt;
			u64 last_cnt;
			unsigned long diff = jiffies - mib->rate[j].last;

			if (0 == j)
				offset = MIB_RX_LO_PRIO;
			else
				offset = MIB_TX_LO_PRIO;
			cnt = mib->counter[offset] + mib->counter[offset + 1];
			last_cnt = cnt;
			cnt -= mib->rate[j].last_cnt;
			if (cnt > 1000000 && diff >= 100) {
				u32 rem;
				u64 rate = cnt;

				rate *= 8;
				diff *= 10 * 100;
				rate = div_u64_rem(rate, diff, &rem);
				mib->rate[j].last = jiffies;
				mib->rate[j].last_cnt = last_cnt;
				if (mib->rate[j].peak < (u32) rate)
					mib->rate[j].peak = (u32) rate;
			}
		} else
			mib->rate[j].last = jiffies;
	}
}  /* determine_rate */

static void ksz_mib_read_work(struct work_struct *work)
{
	struct sw_priv *hw_priv =
		container_of(work, struct sw_priv, mib_read);
	struct ksz_sw *sw = &hw_priv->sw;
	struct ksz_port_mib *mib;
	unsigned long interval;
	uint n;
	uint p;
	int cnt = 0;

	/* Find out how many ports are connected. */
	for (n = 0; n <= sw->mib_port_cnt; n++) {
		p = get_phy_port(sw, n);
		if (media_connected == sw->port_state[p].state)
			++cnt;
	}
	if (!cnt)
		cnt++;
	interval = MIB_READ_INTERVAL * 2 / cnt;
	if (time_before(sw->next_jiffies, jiffies)) {
		sw->next_jiffies = jiffies;
		sw->next_jiffies += interval;
	}
	for (n = 0; n <= sw->mib_port_cnt; n++) {
		p = get_phy_port(sw, n);
		mib = get_port_mib(sw, p);

		/* Reading MIB counters or requested to read. */
		if (mib->cnt_ptr || 1 == hw_priv->counter[p].read) {

			/* Need to process interrupt. */
			if (port_r_cnt(sw, p))
				return;
			hw_priv->counter[p].read = 0;

			/* Finish reading counters. */
			if (0 == mib->cnt_ptr) {
				hw_priv->counter[p].read = 2;
				wake_up_interruptible(
					&hw_priv->counter[p].counter);
				if (p != sw->HOST_PORT)
					determine_rate(sw, mib);
			}
		} else if (time_after_eq(jiffies, hw_priv->counter[p].time)) {
			hw_priv->counter[p].time = sw->next_jiffies;
			/* Only read MIB counters when the port is connected. */
			if (media_connected == sw->port_state[p].state)
				hw_priv->counter[p].read = 1;

			/* Read dropped counters. */
			else
				mib->cnt_ptr = SWITCH_COUNTER_NUM;
			sw->next_jiffies += interval;

		/* Port is just disconnected. */
		} else if (sw->port_state[p].link_down) {
			int j;

			for (j = 0; j < SWITCH_COUNTER_NUM; j++)
				mib->read_cnt[j] += mib->read_max[j];
			sw->port_state[p].link_down = 0;

			/* Read counters one last time after link is lost. */
			hw_priv->counter[p].read = 1;
		}
	}
}  /* ksz_mib_read_work */

static void copy_port_status(struct ksz_port *src, struct ksz_port *dst)
{
	dst->duplex = src->duplex;
	dst->speed = src->speed;
	dst->force_link = src->force_link;
	dst->linked = src->linked;
}

static void link_read_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sw_priv *hw_priv =
		container_of(dwork, struct sw_priv, link_read);
	struct ksz_sw *sw = &hw_priv->sw;
	struct ksz_port *port = NULL;
	struct ksz_port *sw_port = NULL;
	int i;
	int changes = 0;
	int s = 1;
	int dev_cnt = sw->dev_count + sw->dev_offset;

	if (1 == sw->dev_count || 1 == sw->dev_offset)
		s = 0;
	/* Check main device when child devices are used. */
	if (sw->dev_offset)
		sw_port = sw->netport[0];
	sw->ops->acquire(sw);
	for (i = sw->dev_offset; i < dev_cnt; i++) {
		struct phy_priv *phydata;
		int n = i + s;

		port = sw->netport[i];
		phydata = &sw->phydata[n];
		if (!port)
			port = phydata->port;
		changes |= port_get_link_speed(port);

		/* Copy all port information for user access. */
		if (port != phydata->port)
			copy_port_status(port, phydata->port);
	}
	sw->ops->release(sw);

	/* Not to read PHY registers unnecessarily if no link change. */
	if (!changes)
		return;

#ifdef CONFIG_1588_PTP_
	if (sw->features & PTP_HW) {
		struct ptp_info *ptp = &sw->ptp_hw;

		for (i = 0; i < ptp->ports; i++)
			ptp->linked[i] = (sw->port_info[i].state ==
				media_connected);
	}
#endif
	/* Change linked port for main device if it is not connected. */
	if (!sw_port || (media_connected == sw_port->linked->state))
		return;

	for (i = sw->dev_offset; i < dev_cnt; i++) {
		struct phy_priv *phydata;

		port = sw->netport[i];
		if (!port) {
			phydata = &sw->phydata[i];
			port = phydata->port;
		}
		if (media_connected == port->linked->state) {
			sw_port->linked = port->linked;
			break;
		}
	}
}  /* link_read_work */

/*
 * Hardware monitoring
 */

static void ksz_mib_monitor(unsigned long ptr)
{
	struct sw_priv *hw_priv = (struct sw_priv *) ptr;

	schedule_work(&hw_priv->mib_read);

	ksz_update_timer(&hw_priv->mib_timer_info);
}  /* ksz_mib_monitor */

static void ksz_dev_monitor(unsigned long ptr)
{
	struct sw_priv *hw_priv = (struct sw_priv *) ptr;

#ifndef NO_PHYDEV
	struct phy_device *phydev;
	struct phy_priv *priv;
	int i;

	for (i = 0; i < TOTAL_PORT_NUM; i++) {
		phydev = mdiobus_get_phy(hw_priv->bus, i);
		if (!phydev)
			continue;
		priv = phydev->priv;
		if (priv->state != phydev->state) {
			priv->state = phydev->state;
			if (PHY_UP == phydev->state ||
			    PHY_RESUMING == phydev->state)
				schedule_work(&priv->port->link_update);
		}
	}
	if (!hw_priv->intr_working)
		schedule_delayed_work(&hw_priv->link_read, 0);
#endif

	ksz_update_timer(&hw_priv->monitor_timer_info);
}  /* ksz_dev_monitor */

static int fiber;
static int intr_mode;

static int sw_device_present;

static void ksz_probe_last(struct sw_priv *ks)
{
	struct ksz_sw *sw = &ks->sw;
	int ret;

	dev_set_drvdata(ks->dev, ks);

#ifndef CONFIG_KSZ_SWITCH_EMBEDDED
	init_sw_sysfs(sw, &ks->sysfs, ks->dev);
#endif
#ifdef KSZSW_REGS_SIZE
	ret = sysfs_create_bin_file(&ks->dev->kobj,
		&kszsw_registers_attr);
#endif
	sema_init(&ks->proc_sem, 1);

	INIT_WORK(&ks->mib_read, ksz_mib_read_work);

	/* 500 ms timeout */
	ksz_init_timer(&ks->mib_timer_info, 500 * HZ / 1000,
		ksz_mib_monitor, ks);
	ksz_init_timer(&ks->monitor_timer_info, 100 * HZ / 1000,
		ksz_dev_monitor, ks);

	ksz_start_timer(&ks->mib_timer_info, ks->mib_timer_info.period);
	if (!(sw->multi_dev & 1) && !sw->stp)
		ksz_start_timer(&ks->monitor_timer_info,
			ks->monitor_timer_info.period * 10);

	sw_device_present++;

#ifdef CONFIG_1588_PTP
	if (sw->features & PTP_HW) {
		struct ptp_info *ptp = &sw->ptp_hw;

		ptp->ops->init(ptp);
		init_ptp_sysfs(&ks->ptp_sysfs, ks->dev);
	}
#endif

#ifndef NO_PHYDEV
	if (ks->irq <= 0)
		return;
	mutex_lock(&ks->lock);
	sw_setup_intr(sw);
	mutex_unlock(&ks->lock);
	ret = sw_start_interrupt(ks, dev_name(ks->dev));
	if (ret < 0)
		printk(KERN_WARNING "No switch interrupt\n");
	else {
		mutex_lock(&ks->lock);
		sw_ena_intr(sw);
		mutex_unlock(&ks->lock);
	}
#endif
}

static int ksz_probe(struct sw_priv *ks)
{
	struct ksz_sw *sw;
	struct ksz_port_info *info;
	u16 id;
	int i;
	uint mib_port_count;
	uint pi;
	uint port_count;
	int ret;

	if (sw_device_present >= MAX_SW_DEVICES)
		return -ENODEV;

	ks->intr_mode = intr_mode ? IRQF_TRIGGER_FALLING :
		IRQF_TRIGGER_LOW;

	mutex_init(&ks->hwlock);
	mutex_init(&ks->lock);

	sw = &ks->sw;
	mutex_init(&sw->lock);
	sw->hwlock = &ks->hwlock;
	sw->reglock = &ks->lock;
	sw->dev = ks;

	sw->net_ops = &sw_net_ops;
	sw->ops = &sw_ops;

	/* simple check for a valid chip being connected to the bus */
	mutex_lock(&ks->lock);
	ret = sw_chk_id(sw, &id);
	mutex_unlock(&ks->lock);
	if (ret < 0) {
		dev_err(ks->dev, "failed to read device ID(0x%x)\n", id);
		goto err_sw;
	}
	dev_info(ks->dev, "chip id 0x%04x\n", id);

	if (ret > 1) {
		sw->info = kzalloc(sizeof(struct ksz_sw_info), GFP_KERNEL);
		if (!sw->info) {
			ret = -ENOMEM;
			goto err_sw;
		}

		port_count = TOTAL_PORT_NUM;
		mib_port_count = SWITCH_PORT_NUM;
	} else {
		port_count = 1;
		mib_port_count = 1;
		multi_dev = stp = 0;
	}

	sw->PORT_MASK = (1 << mib_port_count) - 1;

	sw->HOST_PORT = port_count - 1;
	sw->HOST_MASK = (1 << sw->HOST_PORT);

	sw->mib_cnt = TOTAL_SWITCH_COUNTER_NUM;
	sw->mib_port_cnt = mib_port_count;
	sw->port_cnt = port_count;

	sw->id = sw_device_present;

	sw->dev_count = 1;

#ifdef DEBUG
	sw->verbose = 1;
#endif
	if (multi_dev < 0)
		multi_dev = 0;
	if (stp < 0)
		stp = 0;

	sw_setup_zone(sw);

	sw_setup_logical_ports(sw);

	sw->PORT_MASK |= sw->HOST_MASK;
dbg_msg("mask: %x %x\n", sw->HOST_MASK, sw->PORT_MASK);

#ifndef NO_PHYDEV
	dbg_msg("%s\n", kszsw_phy_driver_names[ks->sw.chip_id]);
#endif

#ifdef CONFIG_1588_PTP
	sw->features |= PTP_HW;
	if (sw->features & PTP_HW) {
		struct ptp_info *ptp = &sw->ptp_hw;

		ptp->ports = ret;
		ptp->reg = &ptp_reg_ops;
		ptp->ops = &ptp_ops;
		ptp->dev_parent = ks->dev;
	}
#endif

	INIT_DELAYED_WORK(&ks->link_read, link_read_work);

	for (pi = 0; pi < SWITCH_PORT_NUM; pi++) {
		/*
		 * Initialize to invalid value so that link detection
		 * is done.
		 */
		info = get_port_info(sw, pi);
		info->partner = 0xFF;
		info->state = media_disconnected;
	}
	sw->interface = PHY_INTERFACE_MODE_MII;
	info = get_port_info(sw, pi);
	info->state = media_connected;

	sw_init_phy_priv(ks);

#ifndef NO_PHYDEV
	ret = ksz_mii_init(ks);
	if (ret)
		goto err_mii;
#endif

	if (ks->bus) {
		for (i = 0; i <= sw->port_cnt; i++)
			sw->phy[i] = mdiobus_get_phy(ks->bus, i);
	} else {
		for (i = 0; i <= sw->port_cnt; i++)
			sw->phy[i] = &sw->phy_map[i];
	}

	sw->multi_dev |= multi_dev;
	sw->stp |= stp;
	sw->fast_aging |= fast_aging;
	if (sw->stp)
		sw->features |= STP_SUPPORT;
	if (sw->fast_aging)
		sw->overrides |= FAST_AGING;

	sw->counter = ks->counter;
	sw->monitor_timer_info = &ks->monitor_timer_info;
	sw->link_read = &ks->link_read;

	sw_init_mib(sw);

	init_waitqueue_head(&sw->queue);
	for (i = 0; i < TOTAL_PORT_NUM; i++)
		init_waitqueue_head(&ks->counter[i].counter);

	create_debugfs(ks);

#ifdef CONFIG_HAVE_KSZ8863
	sw->ops->acquire(sw);
	id = sw->reg->r8(sw, REG_MODE_INDICATOR);
	fiber = 0;
	if (!(id & PORT_1_COPPER))
		fiber |= 1;
	if (!(id & PORT_2_COPPER))
		fiber |= 2;
	sw->ops->release(sw);
#endif
	for (i = 0; i < SWITCH_PORT_NUM; i++) {
		if (fiber & (1 << i))
			sw->port_info[i].fiber = true;
	}
	if (sw->info) {
#ifdef CONFIG_KSZ_STP
		ksz_stp_init(&sw->info->rstp, sw);
#endif
#ifdef CONFIG_KSZ_HSR
		if (sw->features & HSR_HW)
			ksz_hsr_init(&sw->info->hsr, sw);
#endif
		sw->ops->acquire(sw);
		sw_init(sw);
		sw_setup(sw);
		sw_enable(sw);
		sw->ops->release(sw);
		sw->ops->init(sw);
	}

#ifndef NO_PHYDEV
	ksz_probe_last(ks);
#endif

	return 0;

#ifndef NO_PHYDEV
err_mii:
	kfree(sw->info);
#endif

err_sw:
	kfree(ks->hw_dev);
	kfree(ks);

	return ret;
}

static void ksz_remove_first(struct sw_priv *ks)
{
	struct ksz_sw *sw = &ks->sw;

#ifdef CONFIG_1588_PTP
	if (sw->features & PTP_HW) {
		struct ptp_info *ptp = &sw->ptp_hw;

		exit_ptp_sysfs(&ks->ptp_sysfs, ks->dev);
		ptp->ops->exit(ptp);
	}
#endif
#ifndef NO_PHYDEV
	ksz_mii_exit(ks);
#endif
	ksz_stop_timer(&ks->monitor_timer_info);
	ksz_stop_timer(&ks->mib_timer_info);
	flush_work(&ks->mib_read);
	cancel_delayed_work_sync(&ks->link_read);

#ifdef KSZSW_REGS_SIZE
	sysfs_remove_bin_file(&ks->dev->kobj, &kszsw_registers_attr);
#endif

	if (sw->info) {
		sw->ops->exit(sw);

#ifndef CONFIG_KSZ_SWITCH_EMBEDDED
		exit_sw_sysfs(sw, &ks->sysfs, ks->dev);
#endif

#ifdef CONFIG_KSZ_STP
		ksz_stp_exit(&sw->info->rstp);
#endif
	}

	delete_debugfs(ks);

	/* Indicate this function has been called before. */
	dev_set_drvdata(ks->dev, NULL);
}

static int ksz_remove(struct sw_priv *ks)
{
	struct sw_priv *hw_priv = dev_get_drvdata(ks->dev);
	struct ksz_sw *sw = &ks->sw;

	if (hw_priv)
		ksz_remove_first(hw_priv);
	kfree(sw->info);
	kfree(ks->hw_dev);
	kfree(ks);

	return 0;
}

module_param(fiber, int, 0);
MODULE_PARM_DESC(fiber, "Use fiber in ports");

module_param(intr_mode, int, 0);
MODULE_PARM_DESC(intr_mode,
	"Configure which interrupt mode to use(0=level low, 1=falling)");

module_param(fast_aging, int, 0);
module_param(multi_dev, int, 0);
module_param(stp, int, 0);
MODULE_PARM_DESC(fast_aging, "Fast aging");
MODULE_PARM_DESC(multi_dev, "Multiple device interfaces");
MODULE_PARM_DESC(stp, "STP support");

