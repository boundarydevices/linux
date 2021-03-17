/**
 * Microchip Ethernet driver common header
 *
 * Copyright (c) 2015-2020 Microchip Technology Inc.
 *	Tristram Ha <Tristram.Ha@microchip.com>
 *
 * Copyright (c) 2009-2011 Micrel, Inc.
 *
 * This file contains shared structure definitions to be used between network
 * and switch drivers.
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


#ifndef KSZ_COMMON_H
#define KSZ_COMMON_H


#define NL  "\n"
#define PER_CHAR  "%%"


/* Used to indicate type of flow control support. */
enum {
	PHY_NO_FLOW_CTRL,
	PHY_FLOW_CTRL,
	PHY_TX_ONLY,
	PHY_RX_ONLY
};

/* Used to indicate link connection state. */
enum {
	media_connected,
	media_disconnected,
	media_unknown
};

/* -------------------------------------------------------------------------- */

/**
 * struct ksz_timer_info - Timer information data structure
 * @timer:	Kernel timer.
 * @cnt:	Running timer counter.
 * @max:	Number of times to run timer; -1 for infinity.
 * @period:	Timer period in jiffies.
 */
struct ksz_timer_info {
	struct timer_list timer;
	int cnt;
	int max;
	int period;
};

/**
 * struct ksz_counter_info - OS dependent counter information data structure
 * @counter:	Wait queue to wakeup after counters are read.
 * @time:	Next time in jiffies to read counter.
 * @read:	Indication of counters read in full or not.
 */
struct ksz_counter_info {
	wait_queue_head_t counter;
	unsigned long time;
	int read;
};

#define DEV_NAME_SIZE			32

/**
 * struct ksz_dev_attr - Sysfs data structure
 * @dev_attr:	Device attribute.
 * @dev_name:	Attribute name.
 */
struct ksz_dev_attr {
	struct device_attribute dev_attr;
	char dev_name[DEV_NAME_SIZE];
};

struct ksz_dev_major {
	void *dev;
	int major;
};

struct file_dev_info {
	void *dev;
	uint minor;
	u8 *read_buf;
	u8 *read_in;
	u8 *write_buf;
	size_t read_max;
	size_t read_tmp;
	size_t read_len;
	size_t write_len;
	struct semaphore sem;
	struct mutex lock;
	wait_queue_head_t wait_msg;
	uint notifications[8];
	struct file_dev_info *next;

	void (*dev_free)(struct file_dev_info *info);
	int (*dev_ioctl)(struct file_dev_info *info, void *arg);
	ssize_t (*dev_read)(struct file_dev_info *info, u8 *buf, size_t count,
			    size_t *offp);
	ssize_t (*dev_write)(struct file_dev_info *info, u8 *buf, size_t count,
			     size_t *offp);
};

#endif

