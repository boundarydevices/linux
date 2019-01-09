/*
 * drivers/amlogic/mailbox/scpi_protocol.c
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

#include <linux/err.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/mailbox_client.h>
#include <linux/amlogic/scpi_protocol.h>

#include <linux/slab.h>

#include "meson_mhu.h"


#define CMD_ID_SHIFT		0
#define CMD_ID_MASK		0xff
#define CMD_SENDER_ID_SHIFT	8
#define CMD_SENDER_ID_MASK	0xff
#define CMD_DATA_SIZE_SHIFT	20
#define CMD_DATA_SIZE_MASK	0x1ff
#define PACK_SCPI_CMD(cmd, sender, txsz)				\
	((((cmd) & CMD_ID_MASK) << CMD_ID_SHIFT) |			\
	(((sender) & CMD_SENDER_ID_MASK) << CMD_SENDER_ID_SHIFT) |	\
	(((txsz) & CMD_DATA_SIZE_MASK) << CMD_DATA_SIZE_SHIFT))

#define MAX_DVFS_DOMAINS	3
#define MAX_DVFS_OPPS		16
#define DVFS_LATENCY(hdr)	((hdr) >> 16)
#define DVFS_OPP_COUNT(hdr)	(((hdr) >> 8) & 0xff)

enum scpi_error_codes {
	SCPI_SUCCESS = 0, /* Success */
	SCPI_ERR_PARAM = 1, /* Invalid parameter(s) */
	SCPI_ERR_ALIGN = 2, /* Invalid alignment */
	SCPI_ERR_SIZE = 3, /* Invalid size */
	SCPI_ERR_HANDLER = 4, /* Invalid handler/callback */
	SCPI_ERR_ACCESS = 5, /* Invalid access/permission denied */
	SCPI_ERR_RANGE = 6, /* Value out of range */
	SCPI_ERR_TIMEOUT = 7, /* Timeout has occurred */
	SCPI_ERR_NOMEM = 8, /* Invalid memory area or pointer */
	SCPI_ERR_PWRSTATE = 9, /* Invalid power state */
	SCPI_ERR_SUPPORT = 10, /* Not supported or disabled */
	SCPI_ERR_DEVICE = 11, /* Device error */
	SCPI_ERR_MAX
};

static int scpi_freq_map_table[] = {
	0,
	0,
	1200,
	1300,
	1400,
	1500,
	1600,
	1700,
	1800,
	1900,
	2000,
	2100,
	2200,
	2300,
	2400,
	0
};
static int scpi_volt_map_table[] = {
	0,
	0,
	900,
	910,
	920,
	930,
	940,
	950,
	960,
	970,
	980,
	990,
	1000,
	1010,
	1020,
	0
};

struct scpi_data_buf {
	int client_id;
	struct mhu_data_buf *data;
	struct completion complete;
};

static int high_priority_cmds[] = {
	SCPI_CMD_GET_CSS_PWR_STATE,
	SCPI_CMD_CFG_PWR_STATE_STAT,
	SCPI_CMD_GET_PWR_STATE_STAT,
	SCPI_CMD_SET_DVFS,
	SCPI_CMD_GET_DVFS,
	SCPI_CMD_SET_RTC,
	SCPI_CMD_GET_RTC,
	SCPI_CMD_SET_CLOCK_INDEX,
	SCPI_CMD_SET_CLOCK_VALUE,
	SCPI_CMD_GET_CLOCK_VALUE,
	SCPI_CMD_SET_PSU,
	SCPI_CMD_GET_PSU,
	SCPI_CMD_SENSOR_CFG_PERIODIC,
	SCPI_CMD_SENSOR_CFG_BOUNDS,
	SCPI_CMD_WAKEUP_REASON_GET,
	SCPI_CMD_WAKEUP_REASON_CLR,
};

static struct scpi_dvfs_info *scpi_opps[MAX_DVFS_DOMAINS];

static int scpi_linux_errmap[SCPI_ERR_MAX] = {
	0, -EINVAL, -ENOEXEC, -EMSGSIZE,
	-EINVAL, -EACCES, -ERANGE, -ETIMEDOUT,
	-ENOMEM, -EINVAL, -EOPNOTSUPP, -EIO,
};

static inline int scpi_to_linux_errno(int errno)
{
	if (errno >= SCPI_SUCCESS && errno < SCPI_ERR_MAX)
		return scpi_linux_errmap[errno];
	return -EIO;
}

static bool high_priority_chan_supported(int cmd)
{
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(high_priority_cmds); idx++)
		if (cmd == high_priority_cmds[idx])
			return true;
	return false;
}

static void scpi_rx_callback(struct mbox_client *cl, void *msg)
{
	struct mhu_data_buf *data = (struct mhu_data_buf *)msg;
	struct scpi_data_buf *scpi_buf = data->cl_data;

	complete(&scpi_buf->complete);
}

static int send_scpi_cmd(struct scpi_data_buf *scpi_buf, bool high_priority)
{
	struct mbox_chan *chan;
	struct mbox_client cl = {0};
	struct mhu_data_buf *data = scpi_buf->data;
	u32 status;

	cl.dev = the_scpi_device;
	cl.rx_callback = scpi_rx_callback;

	chan = mbox_request_channel(&cl, high_priority);
	if (IS_ERR(chan))
		return PTR_ERR(chan);

	init_completion(&scpi_buf->complete);
	if (mbox_send_message(chan, (void *)data) < 0) {
		status = SCPI_ERR_TIMEOUT;
		goto free_channel;
	}

	wait_for_completion(&scpi_buf->complete);
	status = *(u32 *)(data->rx_buf); /* read first word */

free_channel:
	mbox_free_channel(chan);

	return scpi_to_linux_errno(status);
}

#define SCPI_SETUP_DBUF(scpi_buf, mhu_buf, _client_id,\
			_cmd, _tx_buf, _rx_buf) \
do {						\
	struct mhu_data_buf *pdata = &mhu_buf;	\
	pdata->cmd = _cmd;			\
	pdata->tx_buf = &_tx_buf;		\
	pdata->tx_size = sizeof(_tx_buf);	\
	pdata->rx_buf = &_rx_buf;		\
	pdata->rx_size = sizeof(_rx_buf);	\
	scpi_buf.client_id = _client_id;	\
	scpi_buf.data = pdata;			\
} while (0)

#define SCPI_SETUP_DBUF_SIZE(scpi_buf, mhu_buf, _client_id,\
			_cmd, _tx_buf, _tx_size, _rx_buf, _rx_size) \
do {						\
	struct mhu_data_buf *pdata = &mhu_buf;	\
	pdata->cmd = _cmd;			\
	pdata->tx_buf = _tx_buf;		\
	pdata->tx_size = _tx_size;	\
	pdata->rx_buf = _rx_buf;		\
	pdata->rx_size = _rx_size;	\
	scpi_buf.client_id = _client_id;	\
	scpi_buf.data = pdata;			\
} while (0)

static int scpi_execute_cmd(struct scpi_data_buf *scpi_buf)
{
	struct mhu_data_buf *data;
	bool high_priority;

	if (!scpi_buf || !scpi_buf->data)
		return -EINVAL;
	data = scpi_buf->data;
	high_priority = high_priority_chan_supported(data->cmd);
	data->cmd = PACK_SCPI_CMD(data->cmd, scpi_buf->client_id,
				  data->tx_size);
	data->cl_data = scpi_buf;

	return send_scpi_cmd(scpi_buf, high_priority);
}

unsigned long scpi_clk_get_val(u16 clk_id)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	struct __packed {
		u32 status;
		u32 clk_rate;
	} buf;

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_CLOCKS,
			SCPI_CMD_GET_CLOCK_VALUE, clk_id, buf);
	if (scpi_execute_cmd(&sdata))
		return 0;

	return buf.clk_rate;
}
EXPORT_SYMBOL_GPL(scpi_clk_get_val);

int scpi_clk_set_val(u16 clk_id, unsigned long rate)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	int stat;
	struct __packed {
		u32 clk_rate;
		u16 clk_id;
	} buf;

	buf.clk_rate = (u32)rate;
	buf.clk_id = clk_id;

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_CLOCKS,
			SCPI_CMD_SET_CLOCK_VALUE, buf, stat);
	return scpi_execute_cmd(&sdata);
}
EXPORT_SYMBOL_GPL(scpi_clk_set_val);

struct scpi_dvfs_info *scpi_dvfs_get_opps(u8 domain)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	struct __packed {
		u32 status;
		u32 header;
		struct scpi_opp_entry opp[MAX_DVFS_OPPS];
	} buf;
	struct scpi_dvfs_info *opps;
	size_t opps_sz;
	int count, ret;

	if (domain >= MAX_DVFS_DOMAINS)
		return ERR_PTR(-EINVAL);

	if (scpi_opps[domain])	/* data already populated */
		return scpi_opps[domain];

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_DVFS,
			SCPI_CMD_GET_DVFS_INFO, domain, buf);
	ret = scpi_execute_cmd(&sdata);
	if (ret)
		return ERR_PTR(ret);

	opps = kmalloc(sizeof(*opps), GFP_KERNEL);
	if (!opps)
		return ERR_PTR(-ENOMEM);

	count = DVFS_OPP_COUNT(buf.header);
	opps_sz = count * sizeof(*(opps->opp));

	opps->count = count;
	opps->latency = DVFS_LATENCY(buf.header);
	opps->opp = kmalloc(opps_sz, GFP_KERNEL);
	if (!opps->opp) {
		kfree(opps);
		return ERR_PTR(-ENOMEM);
	}

	memcpy(opps->opp, &buf.opp[0], opps_sz);
	scpi_opps[domain] = opps;

	return opps;
}
EXPORT_SYMBOL_GPL(scpi_dvfs_get_opps);

int scpi_dvfs_get_idx(u8 domain)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	struct __packed {
		u32 status;
		u8 dvfs_idx;
	} buf;
	int ret;

	if (domain >= MAX_DVFS_DOMAINS)
		return -EINVAL;

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_DVFS,
			SCPI_CMD_GET_DVFS, domain, buf);
	ret = scpi_execute_cmd(&sdata);

	if (!ret)
		ret = buf.dvfs_idx;
	return ret;
}
EXPORT_SYMBOL_GPL(scpi_dvfs_get_idx);

int scpi_dvfs_set_idx(u8 domain, u8 idx)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	struct __packed {
		u8 dvfs_domain;
		u8 dvfs_idx;
	} buf;
	int stat;

	buf.dvfs_idx = idx;
	buf.dvfs_domain = domain;

	if (domain >= MAX_DVFS_DOMAINS)
		return -EINVAL;

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_DVFS,
			SCPI_CMD_SET_DVFS, buf, stat);
	return scpi_execute_cmd(&sdata);
}
EXPORT_SYMBOL_GPL(scpi_dvfs_set_idx);

int scpi_get_sensor(char *name)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	struct __packed {
		u32 status;
		u16 sensors;
	} cap_buf;
	struct __packed {
		u32 status;
		u16 sensor;
		u8 class;
		u8 trigger;
		char name[20];
	} info_buf;
	int ret;
	u16 sensor_id;

	/* This should be handled by a generic macro */
	do {
		struct mhu_data_buf *pdata = &mdata;

		pdata->cmd = SCPI_CMD_SENSOR_CAPABILITIES;
		pdata->tx_size = 0;
		pdata->rx_buf = &cap_buf;
		pdata->rx_size = sizeof(cap_buf);
		sdata.client_id = SCPI_CL_THERMAL;
		sdata.data = pdata;
	} while (0);
	ret = scpi_execute_cmd(&sdata);
	if (ret)
		goto out;

	ret = -ENODEV;
	for (sensor_id = 0; sensor_id < cap_buf.sensors; sensor_id++) {
		SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_THERMAL,
				SCPI_CMD_SENSOR_INFO, sensor_id, info_buf);
		ret = scpi_execute_cmd(&sdata);
		if (ret)
			break;

		if (!strcmp(name, info_buf.name)) {
			ret = sensor_id;
			break;
		}
	}
out:
	return ret;
}
EXPORT_SYMBOL_GPL(scpi_get_sensor);

int scpi_get_sensor_value(u16 sensor, u32 *val)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	struct __packed {
		u32 status;
		u32 val;
	} buf;
	int ret;

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_THERMAL, SCPI_CMD_SENSOR_VALUE,
			sensor, buf);
	ret = scpi_execute_cmd(&sdata);
	if (ret == 0)
		*val = buf.val;

	return ret;
}
EXPORT_SYMBOL_GPL(scpi_get_sensor_value);

/****Send fail when data size > 0x1fd.      ***
 * Because of USER_LOW_TASK_SHARE_MEM_BASE ***
 * size limitation.
 * You can call scpi_send_usr_data()
 * multi-times when your data is bigger
 * than 0x1fe
 */
int scpi_send_usr_data(u32 client_id, u32 *val, u32 size)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	struct __packed {
		u32 status;
		u32 val;
	} buf;
	int ret;

	/*client_id bit map should locates @ 0xff.
	 * bl30 will send client_id via half-Word
	 */
	if (client_id & ~0xff)
		return -E2BIG;

	/*Check size here because of USER_LOW_TASK_SHARE_MEM_BASE
	 * size limitation, and first Word is used as command,
	 * second word is used as tx_size.
	 */
	if (size > 0x1fd)
		return -EPERM;

	SCPI_SETUP_DBUF_SIZE(sdata, mdata, client_id, SCPI_CMD_SET_USR_DATA,
			val, size, &buf, sizeof(buf));
	ret = scpi_execute_cmd(&sdata);

	return ret;
}
EXPORT_SYMBOL_GPL(scpi_send_usr_data);

int scpi_get_vrtc(u32 *p_vrtc)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	u32 temp = 0;
	struct __packed {
		u32 status;
		u32 vrtc;
	} buf;

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_NONE,
			SCPI_CMD_GET_RTC, temp, buf);
	if (scpi_execute_cmd(&sdata))
		return -EPERM;

	*p_vrtc = buf.vrtc;

	return 0;
}
EXPORT_SYMBOL_GPL(scpi_get_vrtc);

int scpi_set_vrtc(u32 vrtc_val)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	int state;

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_NONE,
			SCPI_CMD_SET_RTC, vrtc_val, state);
	if (scpi_execute_cmd(&sdata))
		return -EPERM;

	return 0;
}
EXPORT_SYMBOL_GPL(scpi_set_vrtc);

int scpi_get_ring_value(unsigned char *val)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	struct __packed {
		unsigned int status;
		unsigned char ringinfo[8];
	} buf;
	int ret;
	u32 temp = 0;

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_NONE, SCPI_CMD_OSCRING_VALUE,
			temp, buf);
	ret = scpi_execute_cmd(&sdata);
	if (ret == 0)
		memcpy(val, &buf.ringinfo, sizeof(buf.ringinfo));
	return ret;
}
EXPORT_SYMBOL_GPL(scpi_get_ring_value);

int scpi_get_wakeup_reason(u32 *wakeup_reason)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	u32 temp = 0;
	struct __packed {
		u32 status;
		u32 reason;
	} buf;

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_NONE,
			SCPI_CMD_WAKEUP_REASON_GET, temp, buf);
	if (scpi_execute_cmd(&sdata))
		return -EPERM;

	*wakeup_reason = buf.reason;

	return 0;
}
EXPORT_SYMBOL_GPL(scpi_get_wakeup_reason);

int scpi_clr_wakeup_reason(void)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	u32 temp = 0, state;

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_NONE,
			SCPI_CMD_WAKEUP_REASON_CLR, temp, state);
	if (scpi_execute_cmd(&sdata))
		return -EPERM;

	return 0;
}
EXPORT_SYMBOL_GPL(scpi_clr_wakeup_reason);

int scpi_get_cec_val(enum scpi_std_cmd index, u32 *p_cec)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	u32 temp = 0;
	struct __packed {
		u32 status;
		u32 cec_val;
	} buf;

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_NONE,
			index, temp, buf);
	if (scpi_execute_cmd(&sdata))
		return -EPERM;

	*p_cec = buf.cec_val;
	return 0;

}
EXPORT_SYMBOL_GPL(scpi_get_cec_val);

u8 scpi_get_ethernet_calc(void)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	u8 temp = 0;

	struct __packed {
		u32 status;
		u8 eth_calc;
	} buf;

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_NONE,
		SCPI_CMD_GET_ETHERNET_CALC, temp, buf);
	if (scpi_execute_cmd(&sdata))
		return -EPERM;
	return buf.eth_calc;
}
EXPORT_SYMBOL_GPL(scpi_get_ethernet_calc);

int scpi_get_cpuinfo(enum scpi_get_pfm_type type, u32 *freq, u32 *vol)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	u8 index = 0;
	int ret;

	struct __packed {
		u32 status;
		u8 pfm_info[4];
	} buf;

	SCPI_SETUP_DBUF(sdata, mdata, SCPI_CL_NONE,
		SCPI_CMD_GET_CPUINFO, index, buf);
	if (scpi_execute_cmd(&sdata))
		return -EPERM;

	switch (type) {
	case SCPI_CPUINFO_VERSION:
		ret = buf.pfm_info[0];
		break;
	case SCPI_CPUINFO_CLUSTER0:
		index = (buf.pfm_info[1] >> 4) & 0xf;
		*freq = scpi_freq_map_table[index];
		index = buf.pfm_info[1] & 0xf;
		*vol = scpi_volt_map_table[index];
		ret = 0;
		break;
	case SCPI_CPUINFO_CLUSTER1:
		index = (buf.pfm_info[2] >> 4) & 0xf;
		*freq = scpi_freq_map_table[index];
		index = buf.pfm_info[2] & 0xf;
		*vol = scpi_volt_map_table[index];
		ret = 0;
		break;
	case SCPI_CPUINFO_SLT:
		index = (buf.pfm_info[3] >> 4) & 0xf;
		*freq = scpi_freq_map_table[index];
		index = buf.pfm_info[3] & 0xf;
		*vol = scpi_volt_map_table[index];
		ret = 0;
		break;
	default:
		*freq = 0;
		*vol = 0;
		ret = -1;
		break;
	};
	return ret;
}
EXPORT_SYMBOL_GPL(scpi_get_cpuinfo);
