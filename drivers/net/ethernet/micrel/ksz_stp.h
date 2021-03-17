/**
 * Microchip STP driver header
 *
 * Copyright (c) 2016-2017 Microchip Technology Inc.
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


#ifndef KSZ_STP_H
#define KSZ_STP_H


#define STP_TAG_TYPE			0x1080


struct llc {
	u16 len;
	u8 dsap;
	u8 ssap;
	u8 ctrl;
} __packed;

struct _bridge_id {
	u16 prio;
	u8 addr[6];
} __packed;

struct _port_id {
	u8 prio;
	u8 num;
} __packed;

struct bpdu {
	u16 protocol;
	u8 version;
	u8 type;
	u8 flags;
	struct _bridge_id root;
	u32 root_path_cost;
	struct _bridge_id bridge_id;
	struct _port_id port_id;
	u16 message_age;
	u16 max_age;
	u16 hello_time;
	u16 forward_delay;
	u8 version_1_length;
} __packed;


struct stp_prio {
	struct _bridge_id root;
	u32 root_path_cost;
	struct _bridge_id bridge_id;
	struct _port_id port_id;
} __packed;

struct stp_vector {
	struct stp_prio prio;
	struct _port_id port_id;
} __packed;

struct stp_times {
	u16 message_age;
	u16 max_age;
	u16 hello_time;
	u16 forward_delay;
} __packed;


enum {
	ADMIN_P2P_FORCE_FALSE,
	ADMIN_P2P_FORCE_TRUE,
	ADMIN_P2P_AUTO,
};

enum {
	ROLE_UNKNOWN,
	ROLE_ALT_BACKUP,
	ROLE_ROOT,
	ROLE_DESIGNATED,
	ROLE_ALTERNATE,
	ROLE_BACKUP,
	ROLE_DISABLED,
};

enum {
	INFO_SUPERIOR_DESIGNATED,
	INFO_REPEATED_DESIGNATED,
	INFO_INFERIOR_DESIGNATED,
	INFO_INFERIOR_ROOT_ALT,
	INFO_OTHER,
};

enum {
	INFO_TYPE_UNKNOWN,
	INFO_TYPE_MINE,
	INFO_TYPE_AGED,
	INFO_TYPE_RECEIVED,
	INFO_TYPE_DISABLED,
};

struct stp_br_vars {
	struct _bridge_id br_id_;
	struct stp_vector bridgePrio_;
	struct stp_prio rootPrio_;
	struct stp_times bridgeTimes_;
	struct stp_times rootTimes_;
	struct _port_id rootPortId_;
	u32 TC_sec_;
	u32 TC_cnt_;
	u32 TC_set_;

	uint MigrateTime_;
	uint TxHoldCount_;
	u8 ForceProtocolVersion_;
};

struct ksz_stp_timers {
	/*
	 * Edge Delay timer is time remaining before port is identified as an
	 * operEdgePort.
	 */
	uint edgeDelayWhile_;
	/*
	 * Forward Delay timer is used to delay Port State transitions until
	 * other Bridges have received spanning tree information.
	 */
	uint fdWhile_;
	/*
	 * Hello timer is used to ensure that at least one BPDU is sent by a
	 * Desginated Port in each HelloTime period.
	 */
	uint helloWhen_;
	/*
	 * Migration Delay timer is used to allow time for another RSTP Bridge
	 * to synchronize its migration state before the receipt of a BPDU can
	 * cause this Port to change the BPDU type it sends.
	 */
	uint mdelayWhile_;
	/*
	 * Recent Backup timer is maintained at twice HelloTime while the Port
	 * is a Backup Port.
	 */
	uint rbWhile_;
	/*
	 * Received Info timer is time remaining before spanning tree
	 * inforamtion is aged out.
	 */
	uint rcvdInfoWhile_;
	/*
	 * Recent Root timer.
	 */
	uint rrWhile_;
	/*
	 * Topology Change timer is used to send TCN Messages.
	 */
	uint tcWhile_;
	uint tcDetected_;
};

struct stp_admin_vars {
	u32 AdminEdgePort_:1;
	u32 AutoEdgePort_:1;
	u32 operPointToPointMAC_:1;

	u8 adminPointToPointMAC_;
	uint adminPortPathCost_;

};

struct stp_vars {
	u32 agree_:1;
	u32 agreed_:1;
	u32 disputed_:1;
	u32 fdbFlush_:1;
	u32 forward_:1;
	u32 forwarding_:1;
	u32 learn_:1;
	u32 learning_:1;
	u32 mcheck_:1;
	u32 newInfo_:1;
	u32 operEdge_:1;
	u32 portEnabled_:1;
	u32 proposed_:1;
	u32 proposing_:1;
	u32 rcvdBPDU_:1;
	u32 rcvdMsg_:1;
	u32 rcvdRSTP_:1;
	u32 rcvdSTP_:1;
	u32 rcvdTc_:1;
	u32 rcvdTcAck_:1;
	u32 rcvdTcn_:1;
	u32 reRoot_:1;
	u32 reselect_:1;
	u32 selected_:1;
	u32 sendRSTP_:1;
	u32 sync_:1;
	u32 synced_:1;
	u32 tcAck_:1;
	u32 tcProp_:1;
	u32 tick_:1;
	u32 updtInfo_:1;

	uint ageingTime_;

	u8 infoIs_;
	u8 rcvdInfo_;
	u8 role_;
	u8 selectedRole_;

	uint txCount_;
	struct _port_id portId_;
	uint PortPathCost_;

	struct stp_prio desgPrio_;
	struct stp_prio msgPrio_;
	struct stp_prio portPrio_;
	struct stp_times desgTimes_;
	struct stp_times msgTimes_;
	struct stp_times portTimes_;
};

#define NUM_OF_PORT_TIMERS		(9 + 1)

struct stp_port_vars {
	uint timers[NUM_OF_PORT_TIMERS];
	struct stp_admin_vars admin_var;
	struct stp_vars stp_var;

	u8 bpduVersion_;
	u8 bpduType_;
	u8 bpduFlags_;
	u8 bpduRole_;
	struct stp_prio bpduPrio_;
	struct stp_times bpduTimes_;
};

struct ksz_stp_bridge;

#define NUM_OF_PORT_STATE_MACHINES	9

struct ksz_stp_dbg_times {
	struct stp_prio downPriority;
	struct stp_prio lastPriority;
	unsigned long alt_jiffies;
	unsigned long learn_jiffies;
	unsigned long block_jiffies;
	u8 role_;
};

struct ksz_stp_port {
	struct stp_port_vars vars;

	u8 states[NUM_OF_PORT_STATE_MACHINES];
	u8 state_index;
	u8 port_index;
	int off;
	int link;
	int duplex;
	int speed;
	struct sk_buff_head rxq;

#if 0
	struct bpdu own_bpdu;
#endif
	struct bpdu rx_bpdu0;
	struct bpdu tx_bpdu0;
#if 0
	struct bpdu tx_bpdu1;
#endif
	int dbg_rx;
	int dbg_tx;
#if 0
	int dbg_tx_bpdu;
#endif

	struct ksz_stp_dbg_times dbg_times[1];

	struct ksz_stp_bridge *br;
};

#define NUM_OF_BRIDGE_STATE_MACHINES	1

struct ksz_stp_bridge {
	struct stp_br_vars vars;

	u8 bridgeEnabled;

	struct ksz_stp_port ports[SWITCH_PORT_NUM];
	u8 port_cnt;
	u8 skip_tx;
	u16 port_rx;

	u8 states[NUM_OF_BRIDGE_STATE_MACHINES];
	u8 state_index;

	void *parent;
	struct mutex lock;

	u32 hack_4_1:1;
	u32 hack_4_2:1;
	u32 hack_5_2:1;
};


struct ksz_stp_info;

struct stp_ops {
	int (*change_addr)(struct ksz_stp_info *stp, u8 *addr);
	void (*link_change)(struct ksz_stp_info *stp, int update);

	int (*dev_req)(struct ksz_stp_info *stp, char *arg, void *info);

	int (*get_tcDetected)(struct ksz_stp_info *info, int p);
};

struct ksz_stp_info {
	struct ksz_stp_bridge br;

	void *sw_dev;
	struct net_device *dev;
	struct ksz_timer_info port_timer_info;
	uint timer_tick;
	struct work_struct state_machine;
	bool machine_running;
	struct work_struct rx_proc;

	u8 tx_frame[60];
	struct bpdu *bpdu;
	int len;

	struct sw_dev_info *dev_info;
	uint notifications;

	const struct stp_ops *ops;
};

#endif
