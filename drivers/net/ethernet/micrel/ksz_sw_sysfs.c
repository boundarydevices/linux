/**
 * Microchip switch common sysfs code
 *
 * Copyright (c) 2015-2018 Microchip Technology Inc.
 *	Tristram Ha <Tristram.Ha@microchip.com>
 *
 * Copyright (c) 2011-2013 Micrel, Inc.
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


#include "ksz_sysfs.h"


static char *sw_name[TOTAL_PORT_NUM] = {
	"sw0",
	"sw1",
	"sw2",
};

static char *mac_name[SWITCH_MAC_TABLE_ENTRIES] = {
	"mac0",
	"mac1",
	"mac2",
	"mac3",
	"mac4",
	"mac5",
	"mac6",
	"mac7",
	"mac8",
	"mac9",
	"maca",
	"macb",
	"macc",
	"macd",
	"mace",
	"macf",
};

static char *vlan_name[VLAN_TABLE_ENTRIES] = {
	"vlan0",
	"vlan1",
	"vlan2",
	"vlan3",
	"vlan4",
	"vlan5",
	"vlan6",
	"vlan7",
	"vlan8",
	"vlan9",
	"vlana",
	"vlanb",
	"vlanc",
	"vland",
	"vlane",
	"vlanf",
};

static ssize_t netlan_show(struct device *d, struct device_attribute *attr,
	char *buf, unsigned long offset)
{
	struct ksz_port *uninitialized_var(port);
	struct ksz_sw *sw;
	struct semaphore *proc_sem;
	ssize_t len = -EINVAL;
	int proc_num;

	get_private_data(d, &proc_sem, &sw, &port);
	if (down_interruptible(proc_sem))
		return -ERESTARTSYS;

	len = 0;
	proc_num = offset / sizeof(int);
	len = sw->ops->sysfs_read(sw, proc_num, port, len, buf);
	if (len)
		goto netlan_show_done;

#ifdef CONFIG_KSZ_STP
	len = sw->ops->sysfs_stp_read(sw, proc_num, len, buf);
	if (len)
		goto netlan_show_done;
#endif

	/* Require hardware to be acquired first. */
	sw->ops->acquire(sw);
	len = sw->ops->sysfs_read_hw(sw, proc_num, len, buf);
	sw->ops->release(sw);

netlan_show_done:
	up(proc_sem);
	return len;
}

static ssize_t netlan_store(struct device *d, struct device_attribute *attr,
	const char *buf, size_t count, unsigned long offset)
{
	struct ksz_port *uninitialized_var(port);
	struct ksz_sw *sw;
	struct semaphore *proc_sem;
	ssize_t ret = -EINVAL;
	int num;
	int proc_num;

	num = get_num_val(buf);
	get_private_data(d, &proc_sem, &sw, &port);
	if (down_interruptible(proc_sem))
		return -ERESTARTSYS;

	proc_num = offset / sizeof(int);
	ret = count;

#ifdef CONFIG_KSZ_STP
	if (sw->ops->sysfs_stp_write(sw, proc_num, num, buf))
		goto netlan_store_done;
#endif

	sw->ops->acquire(sw);
	sw->ops->sysfs_write(sw, proc_num, port, num, buf);
	sw->ops->release(sw);

#ifdef CONFIG_KSZ_STP
netlan_store_done:
#endif
	up(proc_sem);
	return ret;
}

static ssize_t netsw_show(struct device *d, struct device_attribute *attr,
	char *buf, unsigned long offset)
{
	struct ksz_sw *sw;
	struct semaphore *proc_sem;
	ssize_t len = -EINVAL;
	int num;
	uint port;

	if (attr->attr.name[1] != '_')
		return len;
	port = attr->attr.name[0] - '0';
	if (port >= TOTAL_PORT_NUM)
		return len;

	get_private_data(d, &proc_sem, &sw, NULL);
	if (down_interruptible(proc_sem))
		return -ERESTARTSYS;

	len = 0;
	num = offset / sizeof(int);
	len = sw->ops->sysfs_port_read(sw, num, port, len, buf);
	if (len)
		goto netsw_show_done;

#ifdef CONFIG_KSZ_STP
	len = sw->ops->sysfs_stp_port_read(sw, num, port, len, buf);
	if (len)
		goto netsw_show_done;
#endif

	/* Require hardware to be acquired first. */
	sw->ops->acquire(sw);
	len = sw->ops->sysfs_port_read_hw(sw, num, port, len, buf);
	sw->ops->release(sw);

netsw_show_done:
	up(proc_sem);
	return len;
}

static ssize_t netsw_store(struct device *d, struct device_attribute *attr,
	const char *buf, size_t count, unsigned long offset)
{
	struct ksz_sw *sw;
	struct semaphore *proc_sem;
	ssize_t ret = -EINVAL;
	int num;
	uint port;
	int proc_num;

	if (attr->attr.name[1] != '_')
		return ret;
	port = attr->attr.name[0] - '0';
	if (port >= TOTAL_PORT_NUM)
		return ret;
	num = get_num_val(buf);
	get_private_data(d, &proc_sem, &sw, NULL);
	if (down_interruptible(proc_sem))
		return -ERESTARTSYS;

	proc_num = offset / sizeof(int);
	ret = count;

#ifdef CONFIG_KSZ_STP
	if (sw->ops->sysfs_stp_port_write(sw, proc_num, port, num, buf))
		goto netsw_store_done;
#endif

	sw->ops->acquire(sw);
	sw->ops->sysfs_port_write(sw, proc_num, port, num, buf);
	sw->ops->release(sw);

#ifdef CONFIG_KSZ_STP
netsw_store_done:
#endif
	up(proc_sem);
	return ret;
}

static ssize_t netmac_show(struct device *d, struct device_attribute *attr,
	char *buf, unsigned long offset)
{
	struct ksz_sw *sw;
	struct semaphore *proc_sem;
	ssize_t len = -EINVAL;
	int num;
	int index;

	if (attr->attr.name[1] != '_')
		return len;
	if ('a' <= attr->attr.name[0] && attr->attr.name[0] <= 'f')
		index = attr->attr.name[0] - 'a' + 10;
	else
		index = attr->attr.name[0] - '0';
	if (index >= SWITCH_MAC_TABLE_ENTRIES)
		return len;

	get_private_data(d, &proc_sem, &sw, NULL);
	if (down_interruptible(proc_sem))
		return -ERESTARTSYS;

	len = 0;
	num = offset / sizeof(int);
	len = sw->ops->sysfs_mac_read(sw, num, index, len, buf);
	up(proc_sem);
	return len;
}

static ssize_t netmac_store(struct device *d, struct device_attribute *attr,
	const char *buf, size_t count, unsigned long offset)
{
	struct ksz_sw *sw;
	struct semaphore *proc_sem;
	ssize_t ret = -EINVAL;
	int num;
	int index;
	int proc_num;

	if (attr->attr.name[1] != '_')
		return ret;
	if ('a' <= attr->attr.name[0] && attr->attr.name[0] <= 'f')
		index = attr->attr.name[0] - 'a' + 10;
	else
		index = attr->attr.name[0] - '0';
	if (index >= SWITCH_MAC_TABLE_ENTRIES)
		return ret;
	num = get_num_val(buf);
	get_private_data(d, &proc_sem, &sw, NULL);
	if (down_interruptible(proc_sem))
		return -ERESTARTSYS;

	proc_num = offset / sizeof(int);
	ret = count;
	sw->ops->acquire(sw);
	sw->ops->sysfs_mac_write(sw, proc_num, index, num, buf);
	sw->ops->release(sw);
	up(proc_sem);
	return ret;
}

static ssize_t netvlan_show(struct device *d, struct device_attribute *attr,
	char *buf, unsigned long offset)
{
	struct ksz_sw *sw;
	struct semaphore *proc_sem;
	ssize_t len = -EINVAL;
	int num;
	int index;

	if (attr->attr.name[1] != '_')
		return len;
	if ('a' <= attr->attr.name[0] && attr->attr.name[0] <= 'f')
		index = attr->attr.name[0] - 'a' + 10;
	else
		index = attr->attr.name[0] - '0';
	if (index >= VLAN_TABLE_ENTRIES)
		return len;

	get_private_data(d, &proc_sem, &sw, NULL);
	if (down_interruptible(proc_sem))
		return -ERESTARTSYS;

	len = 0;
	num = offset / sizeof(int);
	len = sw->ops->sysfs_vlan_read(sw, num, index, len, buf);
	up(proc_sem);
	return len;
}

static ssize_t netvlan_store(struct device *d, struct device_attribute *attr,
	const char *buf, size_t count, unsigned long offset)
{
	struct ksz_sw *sw;
	struct semaphore *proc_sem;
	ssize_t ret = -EINVAL;
	int num;
	int index;
	int proc_num;

	if (attr->attr.name[1] != '_')
		return ret;
	if ('a' <= attr->attr.name[0] && attr->attr.name[0] <= 'f')
		index = attr->attr.name[0] - 'a' + 10;
	else
		index = attr->attr.name[0] - '0';
	if (index >= VLAN_TABLE_ENTRIES)
		return ret;
	num = get_num_val(buf);
	get_private_data(d, &proc_sem, &sw, NULL);
	if (down_interruptible(proc_sem))
		return -ERESTARTSYS;

	proc_num = offset / sizeof(int);
	ret = count;
	sw->ops->acquire(sw);
	sw->ops->sysfs_vlan_write(sw, proc_num, index, num);
	sw->ops->release(sw);
	up(proc_sem);
	return ret;
}

#define LAN_ATTR(_name, _mode, _show, _store) \
struct device_attribute lan_attr_##_name = \
	__ATTR(_name, _mode, _show, _store)

/* generate a read-only attribute */
#define NETLAN_RD_ENTRY(name)						\
static ssize_t show_lan_##name(struct device *d,			\
	struct device_attribute *attr, char *buf)			\
{									\
	return netlan_show(d, attr, buf,				\
		offsetof(struct lan_attributes, name));			\
}									\
static LAN_ATTR(name, S_IRUGO, show_lan_##name, NULL)

/* generate a write-able attribute */
#define NETLAN_WR_ENTRY(name)						\
static ssize_t show_lan_##name(struct device *d,			\
	struct device_attribute *attr, char *buf)			\
{									\
	return netlan_show(d, attr, buf,				\
		offsetof(struct lan_attributes, name));			\
}									\
static ssize_t store_lan_##name(struct device *d,			\
	struct device_attribute *attr, const char *buf, size_t count)	\
{									\
	return netlan_store(d, attr, buf, count,			\
		offsetof(struct lan_attributes, name));			\
}									\
static LAN_ATTR(name, S_IRUGO | S_IWUSR, show_lan_##name, store_lan_##name)

#define SW_ATTR(_name, _mode, _show, _store) \
struct device_attribute sw_attr_##_name = \
	__ATTR(0_##_name, _mode, _show, _store)

/* generate a read-only attribute */
#define NETSW_RD_ENTRY(name)						\
static ssize_t show_sw_##name(struct device *d,				\
	struct device_attribute *attr, char *buf)			\
{									\
	return netsw_show(d, attr, buf,					\
		offsetof(struct sw_attributes, name));			\
}									\
static SW_ATTR(name, S_IRUGO, show_sw_##name, NULL)

/* generate a write-able attribute */
#define NETSW_WR_ENTRY(name)						\
static ssize_t show_sw_##name(struct device *d,				\
	struct device_attribute *attr, char *buf)			\
{									\
	return netsw_show(d, attr, buf,					\
		offsetof(struct sw_attributes, name));			\
}									\
static ssize_t store_sw_##name(struct device *d,			\
	struct device_attribute *attr, const char *buf, size_t count)	\
{									\
	return netsw_store(d, attr, buf, count,				\
		offsetof(struct sw_attributes, name));			\
}									\
static SW_ATTR(name, S_IRUGO | S_IWUSR, show_sw_##name, store_sw_##name)

#define MAC_ATTR(_name, _mode, _show, _store) \
struct device_attribute mac_attr_##_name = \
	__ATTR(0_##_name, _mode, _show, _store)

/* generate a read-only attribute */
#define NETMAC_RD_ENTRY(name)						\
static ssize_t show_mac_##name(struct device *d,			\
	struct device_attribute *attr, char *buf)			\
{									\
	return netmac_show(d, attr, buf,				\
		offsetof(struct static_mac_attributes, name));		\
}									\
static MAC_ATTR(name, S_IRUGO, show_mac_##name, NULL)

/* generate a write-able attribute */
#define NETMAC_WR_ENTRY(name)						\
static ssize_t show_mac_##name(struct device *d,			\
	struct device_attribute *attr, char *buf)			\
{									\
	return netmac_show(d, attr, buf,				\
		offsetof(struct static_mac_attributes, name));		\
}									\
static ssize_t store_mac_##name(struct device *d,			\
	struct device_attribute *attr, const char *buf, size_t count)	\
{									\
	return netmac_store(d, attr, buf, count,			\
		offsetof(struct static_mac_attributes, name));		\
}									\
static MAC_ATTR(name, S_IRUGO | S_IWUSR, show_mac_##name, store_mac_##name)

#define VLAN_ATTR(_name, _mode, _show, _store) \
struct device_attribute vlan_attr_##_name = \
	__ATTR(0_##_name, _mode, _show, _store)

/* generate a read-only attribute */
#define NETVLAN_RD_ENTRY(name)						\
static ssize_t show_vlan_##name(struct device *d,			\
	struct device_attribute *attr, char *buf)			\
{									\
	return netvlan_show(d, attr, buf,				\
		offsetof(struct vlan_attributes, name));		\
}									\
static VLAN_ATTR(name, S_IRUGO, show_vlan_##name, NULL)

/* generate a write-able attribute */
#define NETVLAN_WR_ENTRY(name)						\
static ssize_t show_vlan_##name(struct device *d,			\
	struct device_attribute *attr, char *buf)			\
{									\
	return netvlan_show(d, attr, buf,				\
		offsetof(struct vlan_attributes, name));		\
}									\
static ssize_t store_vlan_##name(struct device *d,			\
	struct device_attribute *attr, const char *buf, size_t count)	\
{									\
	return netvlan_store(d, attr, buf, count,			\
		offsetof(struct vlan_attributes, name));		\
}									\
static VLAN_ATTR(name, S_IRUGO | S_IWUSR, show_vlan_##name, store_vlan_##name)

NETLAN_WR_ENTRY(info);
NETLAN_RD_ENTRY(version);
NETLAN_WR_ENTRY(duplex);
NETLAN_WR_ENTRY(speed);
NETLAN_WR_ENTRY(force);
NETLAN_WR_ENTRY(flow_ctrl);
NETLAN_WR_ENTRY(mib);
NETLAN_WR_ENTRY(reg);
NETLAN_WR_ENTRY(vid);
NETLAN_WR_ENTRY(features);
NETLAN_WR_ENTRY(overrides);

NETLAN_WR_ENTRY(dynamic_table);
NETLAN_WR_ENTRY(static_table);
NETLAN_RD_ENTRY(vlan_table);
NETLAN_WR_ENTRY(aging);
NETLAN_WR_ENTRY(fast_aging);
NETLAN_WR_ENTRY(link_aging);
NETLAN_WR_ENTRY(bcast_per);
NETLAN_WR_ENTRY(mcast_storm);
NETLAN_WR_ENTRY(diffserv_map);
NETLAN_WR_ENTRY(p_802_1p_map);
NETLAN_WR_ENTRY(vlan);
NETLAN_WR_ENTRY(null_vid);
NETLAN_WR_ENTRY(macaddr);
NETLAN_WR_ENTRY(mirror_mode);
NETLAN_WR_ENTRY(tail_tag);
NETLAN_WR_ENTRY(igmp_snoop);
NETLAN_WR_ENTRY(ipv6_mld_snoop);
NETLAN_WR_ENTRY(ipv6_mld_option);
NETLAN_WR_ENTRY(aggr_backoff);
NETLAN_WR_ENTRY(no_exc_drop);
#ifdef SWITCH_BUF_RESERVE
NETLAN_WR_ENTRY(buf_reserve);
#endif
NETLAN_WR_ENTRY(huge_packet);
NETLAN_WR_ENTRY(legal_packet);
NETLAN_WR_ENTRY(length_check);
NETLAN_WR_ENTRY(back_pressure);
NETLAN_WR_ENTRY(sw_flow_ctrl);
NETLAN_WR_ENTRY(sw_half_duplex);
#ifdef SWITCH_10_MBIT
NETLAN_WR_ENTRY(sw_10_mbit);
#endif
NETLAN_WR_ENTRY(rx_flow_ctrl);
NETLAN_WR_ENTRY(tx_flow_ctrl);
NETLAN_WR_ENTRY(fair_flow_ctrl);
NETLAN_WR_ENTRY(vlan_bound);
NETLAN_WR_ENTRY(fw_unk_dest);
NETLAN_WR_ENTRY(ins_tag_0_1);
NETLAN_WR_ENTRY(ins_tag_0_2);
NETLAN_WR_ENTRY(ins_tag_1_0);
NETLAN_WR_ENTRY(ins_tag_1_2);
NETLAN_WR_ENTRY(ins_tag_2_0);
NETLAN_WR_ENTRY(ins_tag_2_1);
NETLAN_WR_ENTRY(pass_all);
NETLAN_WR_ENTRY(pass_pause);
#ifdef SWITCH_PORT_PHY_ADDR_MASK
NETLAN_WR_ENTRY(phy_addr);
#endif
NETLAN_RD_ENTRY(ports);
NETLAN_RD_ENTRY(dev_start);
NETLAN_RD_ENTRY(vlan_start);
NETLAN_RD_ENTRY(stp);

#ifdef CONFIG_KSZ_STP
NETLAN_RD_ENTRY(stp_br_info);
NETLAN_WR_ENTRY(stp_br_on);
NETLAN_WR_ENTRY(stp_br_prio);
NETLAN_WR_ENTRY(stp_br_fwd_delay);
NETLAN_WR_ENTRY(stp_br_hello_time);
NETLAN_WR_ENTRY(stp_br_max_age);
NETLAN_WR_ENTRY(stp_br_tx_hold);
NETLAN_WR_ENTRY(stp_version);
#endif

NETSW_WR_ENTRY(mib);
NETSW_WR_ENTRY(vid);
NETSW_WR_ENTRY(member);
NETSW_WR_ENTRY(bcast_storm);
NETSW_WR_ENTRY(rx);
NETSW_WR_ENTRY(tx);
NETSW_WR_ENTRY(learn);
NETSW_WR_ENTRY(mirror_port);
NETSW_WR_ENTRY(mirror_rx);
NETSW_WR_ENTRY(mirror_tx);
NETSW_WR_ENTRY(diffserv);
NETSW_WR_ENTRY(p_802_1p);
NETSW_WR_ENTRY(port_based);
NETSW_WR_ENTRY(non_vid);
NETSW_WR_ENTRY(drop_tagged);
NETSW_WR_ENTRY(ingress);
NETSW_WR_ENTRY(ins_tag);
NETSW_WR_ENTRY(rmv_tag);
#ifdef PORT_DOUBLE_TAG
NETSW_WR_ENTRY(double_tag);
#endif
NETSW_WR_ENTRY(replace_prio);
NETSW_WR_ENTRY(prio_queue);
NETSW_WR_ENTRY(tx_p0_ctrl);
NETSW_WR_ENTRY(tx_p1_ctrl);
NETSW_WR_ENTRY(tx_p2_ctrl);
NETSW_WR_ENTRY(tx_p3_ctrl);
#ifdef RATE_RATIO_MASK
NETSW_WR_ENTRY(tx_p0_ratio);
NETSW_WR_ENTRY(tx_p1_ratio);
NETSW_WR_ENTRY(tx_p2_ratio);
NETSW_WR_ENTRY(tx_p3_ratio);
#endif
NETSW_WR_ENTRY(rx_prio_rate);
NETSW_WR_ENTRY(tx_prio_rate);
NETSW_WR_ENTRY(rx_limit);
NETSW_WR_ENTRY(cnt_ifg);
NETSW_WR_ENTRY(cnt_pre);
NETSW_WR_ENTRY(rx_p0_rate);
NETSW_WR_ENTRY(rx_p1_rate);
NETSW_WR_ENTRY(rx_p2_rate);
NETSW_WR_ENTRY(rx_p3_rate);
NETSW_WR_ENTRY(tx_p0_rate);
NETSW_WR_ENTRY(tx_p1_rate);
NETSW_WR_ENTRY(tx_p2_rate);
NETSW_WR_ENTRY(tx_p3_rate);
NETSW_WR_ENTRY(back_pressure);
NETSW_WR_ENTRY(force_flow_ctrl);
NETSW_WR_ENTRY(fw_unk_dest);
NETSW_WR_ENTRY(fw_inv_vid);

NETSW_RD_ENTRY(duplex);
NETSW_RD_ENTRY(speed);
NETSW_WR_ENTRY(linkmd);
NETSW_WR_ENTRY(macaddr);
NETSW_WR_ENTRY(src_filter_0);
NETSW_WR_ENTRY(src_filter_1);

#ifdef CONFIG_KSZ_STP
NETSW_RD_ENTRY(stp_info);
NETSW_WR_ENTRY(stp_on);
NETSW_WR_ENTRY(stp_prio);
NETSW_WR_ENTRY(stp_admin_path_cost);
NETSW_WR_ENTRY(stp_path_cost);
NETSW_WR_ENTRY(stp_admin_edge);
NETSW_WR_ENTRY(stp_auto_edge);
NETSW_WR_ENTRY(stp_mcheck);
NETSW_WR_ENTRY(stp_admin_p2p);
#endif

NETMAC_WR_ENTRY(fid);
NETMAC_WR_ENTRY(use_fid);
NETMAC_WR_ENTRY(override);
NETMAC_WR_ENTRY(valid);
NETMAC_WR_ENTRY(ports);
NETMAC_WR_ENTRY(addr);

NETVLAN_WR_ENTRY(valid);
NETVLAN_WR_ENTRY(member);
NETVLAN_WR_ENTRY(fid);
NETVLAN_WR_ENTRY(vid);

static struct attribute *lan_attrs[] = {
	&lan_attr_info.attr,
	&lan_attr_version.attr,
#ifdef USE_SPEED_LINK
	&lan_attr_duplex.attr,
	&lan_attr_speed.attr,
	&lan_attr_force.attr,
	&lan_attr_flow_ctrl.attr,
#endif
#ifdef USE_MIB
	&lan_attr_mib.attr,
#endif
	&lan_attr_reg.attr,
	&lan_attr_vid.attr,
	&lan_attr_features.attr,
	&lan_attr_overrides.attr,

	&lan_attr_dynamic_table.attr,
	&lan_attr_static_table.attr,
	&lan_attr_vlan_table.attr,
	&lan_attr_aging.attr,
	&lan_attr_fast_aging.attr,
	&lan_attr_link_aging.attr,
	&lan_attr_bcast_per.attr,
	&lan_attr_mcast_storm.attr,
	&lan_attr_diffserv_map.attr,
	&lan_attr_p_802_1p_map.attr,
	&lan_attr_vlan.attr,
	&lan_attr_null_vid.attr,
	&lan_attr_macaddr.attr,
	&lan_attr_mirror_mode.attr,
	&lan_attr_tail_tag.attr,
	&lan_attr_igmp_snoop.attr,
	&lan_attr_ipv6_mld_snoop.attr,
	&lan_attr_ipv6_mld_option.attr,
	&lan_attr_aggr_backoff.attr,
	&lan_attr_no_exc_drop.attr,
#ifdef SWITCH_BUF_RESERVE
	&lan_attr_buf_reserve.attr,
#endif
	&lan_attr_huge_packet.attr,
	&lan_attr_legal_packet.attr,
	&lan_attr_length_check.attr,
	&lan_attr_back_pressure.attr,
	&lan_attr_sw_flow_ctrl.attr,
	&lan_attr_sw_half_duplex.attr,
#ifdef SWITCH_10_MBIT
	&lan_attr_sw_10_mbit.attr,
#endif
	&lan_attr_rx_flow_ctrl.attr,
	&lan_attr_tx_flow_ctrl.attr,
	&lan_attr_fair_flow_ctrl.attr,
	&lan_attr_vlan_bound.attr,
	&lan_attr_fw_unk_dest.attr,
	&lan_attr_ins_tag_0_1.attr,
	&lan_attr_ins_tag_0_2.attr,
	&lan_attr_ins_tag_1_0.attr,
	&lan_attr_ins_tag_1_2.attr,
	&lan_attr_ins_tag_2_0.attr,
	&lan_attr_ins_tag_2_1.attr,
	&lan_attr_pass_all.attr,
	&lan_attr_pass_pause.attr,
#ifdef SWITCH_PORT_PHY_ADDR_MASK
	&lan_attr_phy_addr.attr,
#endif
	&lan_attr_ports.attr,
	&lan_attr_dev_start.attr,
	&lan_attr_vlan_start.attr,
	&lan_attr_stp.attr,

#ifdef CONFIG_KSZ_STP
	&lan_attr_stp_br_info.attr,
	&lan_attr_stp_br_on.attr,
	&lan_attr_stp_br_prio.attr,
	&lan_attr_stp_br_fwd_delay.attr,
	&lan_attr_stp_br_hello_time.attr,
	&lan_attr_stp_br_max_age.attr,
	&lan_attr_stp_br_tx_hold.attr,
	&lan_attr_stp_version.attr,
#endif

	NULL
};

static struct attribute *sw_attrs[] = {
	&sw_attr_vid.attr,
	&sw_attr_member.attr,
	&sw_attr_bcast_storm.attr,
	&sw_attr_rx.attr,
	&sw_attr_tx.attr,
	&sw_attr_learn.attr,
	&sw_attr_mirror_port.attr,
	&sw_attr_mirror_rx.attr,
	&sw_attr_mirror_tx.attr,
	&sw_attr_diffserv.attr,
	&sw_attr_p_802_1p.attr,
	&sw_attr_port_based.attr,
	&sw_attr_non_vid.attr,
	&sw_attr_drop_tagged.attr,
	&sw_attr_ingress.attr,
	&sw_attr_ins_tag.attr,
	&sw_attr_rmv_tag.attr,
#ifdef PORT_DOUBLE_TAG
	&sw_attr_double_tag.attr,
#endif
	&sw_attr_replace_prio.attr,
	&sw_attr_prio_queue.attr,
	&sw_attr_tx_p0_ctrl.attr,
	&sw_attr_tx_p1_ctrl.attr,
	&sw_attr_tx_p2_ctrl.attr,
	&sw_attr_tx_p3_ctrl.attr,
#ifdef RATE_RATIO_MASK
	&sw_attr_tx_p0_ratio.attr,
	&sw_attr_tx_p1_ratio.attr,
	&sw_attr_tx_p2_ratio.attr,
	&sw_attr_tx_p3_ratio.attr,
#endif
	&sw_attr_rx_prio_rate.attr,
	&sw_attr_tx_prio_rate.attr,
	&sw_attr_rx_limit.attr,
	&sw_attr_cnt_ifg.attr,
	&sw_attr_cnt_pre.attr,
	&sw_attr_rx_p0_rate.attr,
	&sw_attr_rx_p1_rate.attr,
	&sw_attr_rx_p2_rate.attr,
	&sw_attr_rx_p3_rate.attr,
	&sw_attr_tx_p0_rate.attr,
	&sw_attr_tx_p1_rate.attr,
	&sw_attr_tx_p2_rate.attr,
	&sw_attr_tx_p3_rate.attr,
	&sw_attr_back_pressure.attr,
	&sw_attr_force_flow_ctrl.attr,
	&sw_attr_fw_unk_dest.attr,
	&sw_attr_fw_inv_vid.attr,
	&sw_attr_mib.attr,

	&sw_attr_duplex.attr,
	&sw_attr_speed.attr,
	&sw_attr_linkmd.attr,
	&sw_attr_macaddr.attr,
	&sw_attr_src_filter_0.attr,
	&sw_attr_src_filter_1.attr,

#ifdef CONFIG_KSZ_STP
	&sw_attr_stp_info.attr,
	&sw_attr_stp_on.attr,
	&sw_attr_stp_prio.attr,
	&sw_attr_stp_admin_path_cost.attr,
	&sw_attr_stp_path_cost.attr,
	&sw_attr_stp_admin_edge.attr,
	&sw_attr_stp_auto_edge.attr,
	&sw_attr_stp_mcheck.attr,
	&sw_attr_stp_admin_p2p.attr,
#endif

	NULL
};

static struct attribute *mac_attrs[] = {
	&mac_attr_fid.attr,
	&mac_attr_use_fid.attr,
	&mac_attr_override.attr,
	&mac_attr_valid.attr,
	&mac_attr_ports.attr,
	&mac_attr_addr.attr,
	NULL
};

static struct attribute *vlan_attrs[] = {
	&vlan_attr_valid.attr,
	&vlan_attr_member.attr,
	&vlan_attr_fid.attr,
	&vlan_attr_vid.attr,
	NULL
};

static struct attribute_group lan_group = {
	.name  = "sw",
	.attrs  = lan_attrs,
};

static struct attribute_group sw_group = {
	.name  = "sw0",
	.attrs  = sw_attrs,
};

static struct attribute_group mac_group = {
	.name  = "mac0",
	.attrs  = mac_attrs,
};

static struct attribute_group vlan_group = {
	.name  = "vlan0",
	.attrs  = vlan_attrs,
};

/* Kernel checking requires the attributes are in data segment. */
#define MAC_ATTRS_SIZE		(sizeof(mac_attrs) / sizeof(void *) - 1)
#define VLAN_ATTRS_SIZE		(sizeof(vlan_attrs) / sizeof(void *) - 1)
#define SW_ATTRS_SIZE		(sizeof(sw_attrs) / sizeof(void *) - 1)

#define MAX_SWITCHES		2

static struct ksz_dev_attr ksz_sw_dev_attrs[(
	MAC_ATTRS_SIZE * SWITCH_MAC_TABLE_ENTRIES +
	VLAN_ATTRS_SIZE * VLAN_TABLE_ENTRIES +
	SW_ATTRS_SIZE * TOTAL_PORT_NUM) * MAX_SWITCHES];
static struct ksz_dev_attr *ksz_sw_dev_attrs_ptr = ksz_sw_dev_attrs;

static void exit_sw_sysfs(struct ksz_sw *sw, struct ksz_sw_sysfs *info,
	struct device *dev)
{
	int i;

	for (i = 0; i < VLAN_TABLE_ENTRIES; i++) {
		vlan_group.name = vlan_name[i];
		vlan_group.attrs = info->vlan_attrs[i];
		sysfs_remove_group(&dev->kobj, &vlan_group);
		kfree(info->vlan_attrs[i]);
		info->vlan_attrs[i] = NULL;
		info->ksz_vlan_attrs[i] = NULL;
	}
	for (i = 0; i < SWITCH_MAC_TABLE_ENTRIES; i++) {
		mac_group.name = mac_name[i];
		mac_group.attrs = info->mac_attrs[i];
		sysfs_remove_group(&dev->kobj, &mac_group);
		kfree(info->mac_attrs[i]);
		info->mac_attrs[i] = NULL;
		info->ksz_mac_attrs[i] = NULL;
	}
	for (i = 0; i < TOTAL_PORT_NUM; i++) {
		sw_group.name = sw_name[i];
		sw_group.attrs = info->port_attrs[i];
		sysfs_remove_group(&dev->kobj, &sw_group);
		kfree(info->port_attrs[i]);
		info->port_attrs[i] = NULL;
		info->ksz_port_attrs[i] = NULL;
	}
	ksz_sw_dev_attrs_ptr = ksz_sw_dev_attrs;

	sysfs_remove_group(&dev->kobj, &lan_group);
}

static int init_sw_sysfs(struct ksz_sw *sw, struct ksz_sw_sysfs *info,
	struct device *dev)
{
	int err;
	int i;
	char *file;

	err = sysfs_create_group(&dev->kobj, &lan_group);
	if (err)
		return err;
	for (i = 0; i < SWITCH_MAC_TABLE_ENTRIES; i++) {
		err = alloc_dev_attr(mac_attrs,
			sizeof(mac_attrs) / sizeof(void *), i,
			&info->ksz_mac_attrs[i],
			&info->mac_attrs[i], NULL, &ksz_sw_dev_attrs_ptr);
		if (err)
			return err;
		mac_group.name = mac_name[i];
		mac_group.attrs = info->mac_attrs[i];
		err = sysfs_create_group(&dev->kobj, &mac_group);
		if (err)
			return err;
	}
	for (i = 0; i < VLAN_TABLE_ENTRIES; i++) {
		err = alloc_dev_attr(vlan_attrs,
			sizeof(vlan_attrs) / sizeof(void *), i,
			&info->ksz_vlan_attrs[i],
			&info->vlan_attrs[i], NULL, &ksz_sw_dev_attrs_ptr);
		if (err)
			return err;
		vlan_group.name = vlan_name[i];
		vlan_group.attrs = info->vlan_attrs[i];
		err = sysfs_create_group(&dev->kobj, &vlan_group);
		if (err)
			return err;
	}
	for (i = 0; i < TOTAL_PORT_NUM; i++) {
		file = NULL;
		if (i == sw->HOST_PORT)
			file = "0_duplex";
		err = alloc_dev_attr(sw_attrs,
			sizeof(sw_attrs) / sizeof(void *), i,
			&info->ksz_port_attrs[i], &info->port_attrs[i],
			file, &ksz_sw_dev_attrs_ptr);
		if (err)
			return err;
		sw_group.name = sw_name[i];
		sw_group.attrs = info->port_attrs[i];
		err = sysfs_create_group(&dev->kobj, &sw_group);
		if (err)
			return err;
	}
	return err;
}

