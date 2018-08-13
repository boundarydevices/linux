/*
 * drivers/amlogic/cec/m8_ao_cec.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/workqueue.h>
#include <linux/timer.h>

#include <linux/uaccess.h>
#include <linux/delay.h>

#include <linux/amlogic/pm.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_irq.h>
#include "hdmi_ao_cec.h"
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of_address.h>
#include <linux/random.h>
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
#endif
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/cec_common.h>
#include <linux/notifier.h>

#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>

#define DEFAULT_DEBUG_EN	0

#define DBG_BUF_SIZE		80
#define MAX_INT			0x7ffffff

/* global struct for tx and rx */
struct ao_cec_dev {
	unsigned char rx_msg[MAX_MSG];
	/* bit filed for flags */
	struct {
		unsigned char my_log_addr   : 4;
		unsigned char port_num      : 4; /* first bytes */
		unsigned char cec_tx_result : 2;
		unsigned char power_status  : 2;
		unsigned char rx_len        : 4; /* second bytes */
	#ifdef CONFIG_PM
		unsigned char cec_suspend   : 2;
	#endif
		unsigned char dev_type      : 3;
		unsigned char cec_version   : 3; /* third bytes */
		/* remaining bool flags */
		bool new_msg        : 1;
		bool phy_addr_test  : 1;
		bool wake_ok        : 1;
		bool cec_msg_dbg_en : 1;
		bool hal_control    : 1;
		/*
		 * pin_status: tv support cec check, for box
		 * hpd_state:  hdmi cable connected status
		 *
		 *  hpd_state  pin_status  | TV support cec
		 *      0          x       |      N/A
		 *      1          0       |       0
		 *      1          1       |       1
		 */
		bool pin_status     : 1;
		bool hpd_state      : 1;
	};

	/* hardware config */
	unsigned short arc_port;
	unsigned short phy_addr;
	unsigned short irq_cec;
	unsigned short cec_line_cnt;
	unsigned short hal_flag;
	unsigned int   port_seq;

	/* miscellaneous */
	unsigned int menu_lang;
	unsigned int open_count;
	dev_t dev_no;

	/* vendor related */
	unsigned int vendor_id;
	char *osd_name;
	char *product;
	const char *pin_name;

	/* resource of register */
	void __iomem *cec_reg;

	/* kernel resource */
	struct input_dev *remote_cec_dev;
	struct notifier_block hdmitx_nb;
	struct workqueue_struct *cec_thread;
	struct device *dbg_dev;
	struct delayed_work cec_work;
	struct completion rx_ok;
	struct completion tx_ok;
	spinlock_t cec_reg_lock;
	struct mutex cec_mutex;
	struct hrtimer start_bit_check;
};

static struct ao_cec_dev *cec_dev;

#define CEC_ERR(format, args...)				\
	{if (cec_dev->dbg_dev)					\
		dev_err(cec_dev->dbg_dev, format, ##args);	\
	}

#define CEC_INFO(format, args...)				\
	{if (cec_dev->cec_msg_dbg_en && cec_dev->dbg_dev)	\
		dev_info(cec_dev->dbg_dev, format, ##args);	\
	}


#define waiting_aocec_free() \
	do {\
		unsigned long cnt = 0;\
		while (readl(cec_dev->cec_reg + AO_CEC_RW_REG) & (1<<23)) {\
			if (cnt++ >= 3500) { \
				pr_info("waiting aocec free time out.\n");\
				break;\
			} \
		} \
	} while (0)

static void cec_set_reg_bits(unsigned int addr, unsigned int value,
	unsigned int offset, unsigned int len)
{
	unsigned int data32 = 0;

	data32 = readl(cec_dev->cec_reg + addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	writel(data32, cec_dev->cec_reg + addr);
}

static unsigned int aocec_rd_reg(unsigned long addr)
{
	unsigned int data32;
	unsigned long flags;

	waiting_aocec_free();
	spin_lock_irqsave(&cec_dev->cec_reg_lock, flags);
	data32 = 0;
	data32 |= 0 << 16; /* [16]	 cec_reg_wr */
	data32 |= 0 << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]	cec_reg_addr */
	writel(data32, cec_dev->cec_reg + AO_CEC_RW_REG);

	waiting_aocec_free();
	data32 = ((readl(cec_dev->cec_reg + AO_CEC_RW_REG)) >> 24) & 0xff;
	spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
	return data32;
} /* aocec_rd_reg */

static void aocec_wr_reg(unsigned long addr, unsigned long data)
{
	unsigned long data32;
	unsigned long flags;

	waiting_aocec_free();
	spin_lock_irqsave(&cec_dev->cec_reg_lock, flags);
	data32 = 0;
	data32 |= 1 << 16; /* [16]	 cec_reg_wr */
	data32 |= data << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]	cec_reg_addr */
	writel(data32, cec_dev->cec_reg + AO_CEC_RW_REG);
	spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
} /* aocec_wr_only_reg */

static void cec_hw_buf_clear(void)
{
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_DISABLE);
	aocec_wr_reg(CEC_TX_MSG_CMD, TX_ABORT);
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 1);
	aocec_wr_reg(CEC_TX_CLEAR_BUF, 1);
	udelay(100);
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 0);
	aocec_wr_reg(CEC_TX_CLEAR_BUF, 0);
	udelay(100);
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_NO_OP);
	aocec_wr_reg(CEC_TX_MSG_CMD, TX_NO_OP);
}

static void cec_logicaddr_set(int l_add)
{
	/* save logical address for suspend/wake up */
	cec_set_reg_bits(AO_DEBUG_REG1, l_add, 16, 4);
	aocec_wr_reg(CEC_LOGICAL_ADDR0, 0);
	cec_hw_buf_clear();
	aocec_wr_reg(CEC_LOGICAL_ADDR0, (l_add & 0xf));
	udelay(100);
	aocec_wr_reg(CEC_LOGICAL_ADDR0, (0x1 << 4) | (l_add & 0xf));
	if (cec_dev->cec_msg_dbg_en)
		CEC_INFO("set logical addr:0x%x\n",
			aocec_rd_reg(CEC_LOGICAL_ADDR0));
}

static void cec_enable_irq(void)
{
	cec_set_reg_bits(AO_CEC_INTR_MASKN, 0x6, 0, 3);
	CEC_INFO("enable:int mask:0x%x\n",
		 readl(cec_dev->cec_reg + AO_CEC_INTR_MASKN));
}

static void cec_arbit_bit_time_set(unsigned int bit_set,
				   unsigned int time_set, unsigned int flag)
{   /* 11bit:bit[10:0] */
	if (flag) {
		CEC_INFO("bit_set:0x%x;time_set:0x%x\n",
			bit_set, time_set);
	}

	switch (bit_set) {
	case 3:
		/* 3 bit */
		if (flag) {
			CEC_INFO("read 3 bit:0x%x%x\n",
				aocec_rd_reg(AO_CEC_TXTIME_4BIT_BIT10_8),
				aocec_rd_reg(AO_CEC_TXTIME_4BIT_BIT7_0));
		}
		aocec_wr_reg(AO_CEC_TXTIME_4BIT_BIT7_0, time_set & 0xff);
		aocec_wr_reg(AO_CEC_TXTIME_4BIT_BIT10_8, (time_set >> 8) & 0x7);
		if (flag) {
			CEC_INFO("write 3 bit:0x%x%x\n",
				aocec_rd_reg(AO_CEC_TXTIME_4BIT_BIT10_8),
				aocec_rd_reg(AO_CEC_TXTIME_4BIT_BIT7_0));
		}
		break;
		/* 5 bit */
	case 5:
		if (flag) {
			CEC_INFO("read 5 bit:0x%x%x\n",
				aocec_rd_reg(AO_CEC_TXTIME_2BIT_BIT10_8),
				aocec_rd_reg(AO_CEC_TXTIME_2BIT_BIT7_0));
		}
		aocec_wr_reg(AO_CEC_TXTIME_2BIT_BIT7_0, time_set & 0xff);
		aocec_wr_reg(AO_CEC_TXTIME_2BIT_BIT10_8, (time_set >> 8) & 0x7);
		if (flag) {
			CEC_INFO("write 5 bit:0x%x%x\n",
				aocec_rd_reg(AO_CEC_TXTIME_2BIT_BIT10_8),
				aocec_rd_reg(AO_CEC_TXTIME_2BIT_BIT7_0));
		}
		break;
		/* 7 bit */
	case 7:
		if (flag) {
			CEC_INFO("read 7 bit:0x%x%x\n",
				aocec_rd_reg(AO_CEC_TXTIME_17MS_BIT10_8),
				aocec_rd_reg(AO_CEC_TXTIME_17MS_BIT7_0));
		}
		aocec_wr_reg(AO_CEC_TXTIME_17MS_BIT7_0, time_set & 0xff);
		aocec_wr_reg(AO_CEC_TXTIME_17MS_BIT10_8, (time_set >> 8) & 0x7);
		if (flag) {
			CEC_INFO("write 7 bit:0x%x%x\n",
				aocec_rd_reg(AO_CEC_TXTIME_17MS_BIT10_8),
				aocec_rd_reg(AO_CEC_TXTIME_17MS_BIT7_0));
		}
		break;
	default:
		break;
	}
}

static void ceca_hw_reset(void)
{
	writel(0x1, cec_dev->cec_reg + AO_CEC_GEN_CNTL);
	/* Enable gated clock (Normal mode). */
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 1, 1, 1);
	/* Release SW reset */
	udelay(100);
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 0, 0, 1);

	/* Enable all AO_CEC interrupt sources */
	cec_set_reg_bits(AO_CEC_INTR_MASKN, 0x6, 0, 3);

	cec_logicaddr_set(cec_dev->my_log_addr);

	/* Cec arbitration 3/5/7 bit time set. */
	cec_arbit_bit_time_set(3, 0x118, 0);
	cec_arbit_bit_time_set(5, 0x000, 0);
	cec_arbit_bit_time_set(7, 0x2aa, 0);

	CEC_INFO("hw reset :logical addr:0x%x\n",
		aocec_rd_reg(CEC_LOGICAL_ADDR0));
}

static void cec_rx_buf_clear(void)
{
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 0x1);
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 0x0);
}

static int cec_rx_buf_check(void)
{
	unsigned int rx_num_msg;

	rx_num_msg = aocec_rd_reg(CEC_RX_NUM_MSG);
	if (rx_num_msg)
		CEC_INFO("rx msg num:0x%02x\n", rx_num_msg);

	return rx_num_msg;
}

static void format_msg_str(const char *msg, char len, char *prefix, char *buf)
{
	int size = 0, i;

	size = sprintf(buf + size, "%s %2d:", prefix, len);
	for (i = 0; i < len; i++)
		size += sprintf(buf + size, " %02x", msg[i]);
	buf[size] = '\0';
	WARN_ON(size >= DBG_BUF_SIZE);
}

int cec_ll_rx(unsigned char *msg)
{
	int i;
	int ret = -1;
	int rx_stat;
	int len;

	rx_stat = aocec_rd_reg(CEC_RX_MSG_STATUS);
	if ((rx_stat != RX_DONE) || (aocec_rd_reg(CEC_RX_NUM_MSG) != 1)) {
		CEC_INFO("rx status:%x\n", rx_stat);
		writel((1 << 2), cec_dev->cec_reg + AO_CEC_INTR_CLR);
		aocec_wr_reg(CEC_RX_MSG_CMD,  RX_ACK_CURRENT);
		aocec_wr_reg(CEC_RX_MSG_CMD,  RX_NO_OP);
		cec_rx_buf_clear();
		return ret;
	}

	len = aocec_rd_reg(CEC_RX_MSG_LENGTH) + 1;
	cec_dev->rx_len = len;

	for (i = 0; i < len && i < MAX_MSG; i++)
		msg[i] = aocec_rd_reg(CEC_RX_MSG_0_HEADER + i);

	ret = rx_stat;

	if (cec_dev->cec_msg_dbg_en) {
		char buf[DBG_BUF_SIZE] = {};

		format_msg_str(msg, len, "CEC rx msg", buf);
		pr_info("%s\n", buf);
	}

	writel((1 << 2), cec_dev->cec_reg + AO_CEC_INTR_CLR);
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_ACK_CURRENT);
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_NO_OP);
	cec_rx_buf_clear();
	cec_dev->pin_status = 1;
	return ret;
}

/************************ cec arbitration cts code **************************/
/* using the cec pin as fiq gpi to assist the bus arbitration */

/* return value: 1: successful	  0: error */
static int cec_ll_trigle_tx(const unsigned char *msg, int len)
{
	int i;
	int reg;
	unsigned int j = 40;
	unsigned int tx_stat;
	int cec_timeout_cnt = 1;

	while (1) {
		tx_stat = aocec_rd_reg(CEC_TX_MSG_STATUS);
		if (tx_stat != TX_BUSY)
			break;

		if (!(j--)) {
			CEC_INFO("waiting busy timeout\n");
			aocec_wr_reg(CEC_TX_MSG_CMD, TX_ABORT);
			cec_timeout_cnt++;
			if (cec_timeout_cnt > 0x08)
				ceca_hw_reset();
			break;
		}
		msleep(20);
	}

	reg = aocec_rd_reg(CEC_TX_MSG_STATUS);
	if (reg == TX_IDLE || reg == TX_DONE) {
		for (i = 0; i < len; i++)
			aocec_wr_reg(CEC_TX_MSG_0_HEADER + i, msg[i]);

		aocec_wr_reg(CEC_TX_MSG_LENGTH, len-1);
		aocec_wr_reg(CEC_TX_MSG_CMD, TX_REQ_CURRENT);
		if (cec_dev->cec_msg_dbg_en) {
			char buf[DBG_BUF_SIZE] = {};

			format_msg_str(msg, len, "CEC tx msg", buf);
			pr_info("%s\n", buf);
		}
		return 0;
	}
	return -1;
}

static void tx_irq_handle(void)
{
	unsigned int tx_status = aocec_rd_reg(CEC_TX_MSG_STATUS);

	switch (tx_status) {
	case TX_DONE:
		aocec_wr_reg(CEC_TX_MSG_CMD, TX_NO_OP);
		cec_dev->cec_tx_result = CEC_FAIL_NONE;
		break;

	case TX_BUSY:
		CEC_ERR("TX_BUSY\n");
		cec_dev->cec_tx_result = CEC_FAIL_BUSY;
		break;

	case TX_ERROR:
		if (cec_dev->cec_msg_dbg_en  == 1)
			CEC_ERR("TX ERROR!!!\n");
		if (aocec_rd_reg(CEC_RX_MSG_STATUS) == RX_ERROR)
			ceca_hw_reset();
		else
			aocec_wr_reg(CEC_TX_MSG_CMD, TX_NO_OP);
		cec_dev->cec_tx_result = CEC_FAIL_NACK;
		break;

	case TX_IDLE:
		CEC_ERR("TX_IDLE\n");
		cec_dev->cec_tx_result = CEC_FAIL_OTHER;
		break;
	default:
		cec_dev->cec_tx_result = CEC_FAIL_OTHER;
		break;
	}
	writel((1 << 1), cec_dev->cec_reg + AO_CEC_INTR_CLR);
	complete(&cec_dev->tx_ok);
}

static int get_line(void)
{
	int reg, cpu_type, ret = -EINVAL;

	reg = readl(cec_dev->cec_reg + AO_GPIO_I);
	cpu_type = get_cpu_type();
	if (cpu_type <= MESON_CPU_MAJOR_ID_GXBB)
		ret = (reg & (1 << 12));
	else
		ret = 1;
	return ret;
}

static enum hrtimer_restart cec_line_check(struct hrtimer *timer)
{
	if (get_line() == 0)
		cec_dev->cec_line_cnt++;
	hrtimer_forward_now(timer, HR_DELAY(1));
	return HRTIMER_RESTART;
}

static int check_confilct(void)
{
	int i;

	for (i = 0; i < 50; i++) {
		/*
		 * sleep 20ms and using hrtimer to check cec line every 1ms
		 */
		cec_dev->cec_line_cnt = 0;
		hrtimer_start(&cec_dev->start_bit_check,
			      HR_DELAY(1), HRTIMER_MODE_REL);
		msleep(20);
		hrtimer_cancel(&cec_dev->start_bit_check);
		if (cec_dev->cec_line_cnt == 0)
			break;
		CEC_INFO("line busy:%d\n", cec_dev->cec_line_cnt);
	}
	if (i >= 50)
		return -EBUSY;
	else
		return 0;
}

static inline bool physical_addr_valid(void)
{
	if (cec_dev->dev_type == DEV_TYPE_TV)
		return 1;
	if (cec_dev->phy_addr_test)
		return 1;
	if (cec_dev->phy_addr == INVALID_PHY_ADDR)
		return 0;
	return 1;
}

static int cec_hdmi_tx_notify_handler(struct notifier_block *nb,
				      unsigned long value, void *p)
{
	int ret = 0;
	int phy_addr = 0;

	switch (value) {
	case HDMITX_PLUG:
		cec_dev->hpd_state = 1;
		CEC_INFO("%s, HDMITX_PLUG\n", __func__);
		break;

	case HDMITX_UNPLUG:
		cec_dev->hpd_state = 0;
		CEC_INFO("%s, HDMITX_UNPLUG\n", __func__);
		break;

	case HDMITX_PHY_ADDR_VALID:
		phy_addr = *((int *)p);
		cec_dev->phy_addr = phy_addr & 0xffff;
		cec_dev->hpd_state = 1;
		CEC_INFO("%s, phy_addr %x ok\n", __func__, cec_dev->phy_addr);
		break;

	default:
		CEC_INFO("unsupported hdmitx notify:%ld, arg:%p\n", value, p);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static bool check_physical_addr_valid(int timeout)
{
	while (timeout > 0) {
		if (!physical_addr_valid()) {
			msleep(100);
			timeout--;
		} else
			break;
	}
	if (timeout <= 0)
		return false;
	return true;
}

/* Return value: < 0: fail, > 0: success */
int cec_ll_tx(const unsigned char *msg, unsigned char len)
{
	int ret = -1;
	int t = msecs_to_jiffies(2000);
	int retry = 2;

	if (len == 0)
		return CEC_FAIL_NONE;

	mutex_lock(&cec_dev->cec_mutex);
	/* make sure physical address is valid before send */
	if (len >= 2 && msg[1] == CEC_OC_REPORT_PHYSICAL_ADDRESS)
		check_physical_addr_valid(20);

try_again:
	reinit_completion(&cec_dev->tx_ok);
	/*
	 * CEC controller won't ack message if it is going to send
	 * state. If we detect cec line is low during waiting signal
	 * free time, that means a send is already started by other
	 * device, we should wait it finished.
	 */
	if (check_confilct()) {
		CEC_ERR("bus confilct too long\n");
		mutex_unlock(&cec_dev->cec_mutex);
		return CEC_FAIL_BUSY;
	}

	ret = cec_ll_trigle_tx(msg, len);
	if (ret < 0) {
		/* we should increase send idx if busy */
		CEC_INFO("tx busy\n");
		if (retry > 0) {
			retry--;
			msleep(100 + (prandom_u32() & 0x07) * 10);
			goto try_again;
		}
		mutex_unlock(&cec_dev->cec_mutex);
		return CEC_FAIL_BUSY;
	}
	cec_dev->cec_tx_result = CEC_FAIL_OTHER;
	ret = wait_for_completion_timeout(&cec_dev->tx_ok, t);
	if (ret <= 0) {
		/* timeout or interrupt */
		if (ret == 0) {
			CEC_ERR("tx timeout\n");
			ceca_hw_reset();
		}
		ret = CEC_FAIL_OTHER;
	} else {
		ret = cec_dev->cec_tx_result;
	}
	if (ret != CEC_FAIL_NONE && ret != CEC_FAIL_NACK) {
		if (retry > 0) {
			retry--;
			msleep(100 + (prandom_u32() & 0x07) * 10);
			goto try_again;
		}
	}
	mutex_unlock(&cec_dev->cec_mutex);

	return ret;
}

/* -------------------------------------------------------------------------- */
/* AO CEC0 config */
/* -------------------------------------------------------------------------- */
static void ao_cec_init(void)
{
	unsigned long data32;

	data32  = 0;
	data32 |= 0 << 1;	/* [2:1]	cntl_clk: */
				/* 0=Disable clk (Power-off mode); */
				/* 1=Enable gated clock (Normal mode); */
				/* 2=Enable free-run clk (Debug mode). */
	data32 |= 1 << 0;	/* [0]	  sw_reset: 1=Reset */
	writel(data32, cec_dev->cec_reg + AO_CEC_GEN_CNTL);
	/* Enable gated clock (Normal mode). */
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 1, 1, 1);
	/* Release SW reset */
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 0, 0, 1);

	/* Enable all AO_CEC interrupt sources */
	cec_enable_irq();
}

static unsigned int ao_cec_intr_stat(void)
{
	return readl(cec_dev->cec_reg + AO_CEC_INTR_STAT);
}

static unsigned int cec_intr_stat(void)
{
	return ao_cec_intr_stat();
}

/*
 *wr_flag: 1 write; value valid
 *		 0 read;  value invalid
 */
static unsigned int cec_config(unsigned int value, bool wr_flag)
{
	if (wr_flag)
		cec_set_reg_bits(AO_DEBUG_REG0, value, 0, 8);

	return readl(cec_dev->cec_reg + AO_DEBUG_REG0) & 0xff;
}

/*
 *wr_flag:1 write; value valid
 *		0 read;  value invalid
 */
static unsigned int cec_phyaddr_config(unsigned int value, bool wr_flag)
{
	if (wr_flag)
		cec_set_reg_bits(AO_DEBUG_REG1, value, 0, 16);

	return readl(cec_dev->cec_reg + AO_DEBUG_REG1);
}

static void cec_keep_reset(void)
{
	writel(0x1, cec_dev->cec_reg + AO_CEC_GEN_CNTL);
}
/*
 * cec hw module init before allocate logical address
 */
static void cec_pre_init(void)
{
	unsigned int reg = readl(cec_dev->cec_reg + AO_RTI_STATUS_REG1);

	ao_cec_init();

	cec_arbit_bit_time_set(3, 0x118, 0);
	cec_arbit_bit_time_set(5, 0x000, 0);
	cec_arbit_bit_time_set(7, 0x2aa, 0);
	reg &= 0xfffff;
	if ((reg & 0xffff) == 0xffff)
		cec_dev->wake_ok = 0;
	pr_info("cec: wake up flag:%x\n", reg);
}

static int cec_late_check_rx_buffer(void)
{
	int ret;
	struct delayed_work *dwork = &cec_dev->cec_work;

	ret = cec_rx_buf_check();
	if (!ret)
		return 0;
	/*
	 * start another check if rx buffer is full
	 */
	if ((-1) == cec_ll_rx(cec_dev->rx_msg)) {
		CEC_INFO("buffer got unrecorgnized msg\n");
		cec_rx_buf_clear();
		ret = 0;
	} else {
		mod_delayed_work(cec_dev->cec_thread, dwork, 0);
		ret = 1;
	}
	return ret;
}

static void cec_key_report(int suspend)
{
	input_event(cec_dev->remote_cec_dev, EV_KEY, KEY_POWER, 1);
	input_sync(cec_dev->remote_cec_dev);
	input_event(cec_dev->remote_cec_dev, EV_KEY, KEY_POWER, 0);
	input_sync(cec_dev->remote_cec_dev);
	pm_wakeup_event(cec_dev->dbg_dev, 2000);
	if (!suspend)
		CEC_INFO("== WAKE UP BY CEC ==\n")
	else
		CEC_INFO("== SLEEP by CEC==\n")
}

static void cec_give_version(unsigned int dest)
{
	unsigned char index = cec_dev->my_log_addr;
	unsigned char msg[3];

	if (dest != 0xf) {
		msg[0] = ((index & 0xf) << 4) | dest;
		msg[1] = CEC_OC_CEC_VERSION;
		msg[2] = cec_dev->cec_version;
		cec_ll_tx(msg, 3);
	}
}

static void cec_report_physical_address_smp(void)
{
	unsigned char msg[5];
	unsigned char index = cec_dev->my_log_addr;
	unsigned char phy_addr_ab, phy_addr_cd;

	phy_addr_ab = (cec_dev->phy_addr >> 8) & 0xff;
	phy_addr_cd = (cec_dev->phy_addr >> 0) & 0xff;
	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_REPORT_PHYSICAL_ADDRESS;
	msg[2] = phy_addr_ab;
	msg[3] = phy_addr_cd;
	msg[4] = cec_dev->dev_type;

	cec_ll_tx(msg, 5);
}

static void cec_device_vendor_id(void)
{
	unsigned char index = cec_dev->my_log_addr;
	unsigned char msg[5];
	unsigned int vendor_id;

	vendor_id = cec_dev->vendor_id;
	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_DEVICE_VENDOR_ID;
	msg[2] = (vendor_id >> 16) & 0xff;
	msg[3] = (vendor_id >> 8) & 0xff;
	msg[4] = (vendor_id >> 0) & 0xff;

	cec_ll_tx(msg, 5);
}

static void cec_give_deck_status(unsigned int dest)
{
	unsigned char index = cec_dev->my_log_addr;
	unsigned char msg[3];

	msg[0] = ((index & 0xf) << 4) | dest;
	msg[1] = CEC_OC_DECK_STATUS;
	msg[2] = 0x1a;
	cec_ll_tx(msg, 3);
}

static void cec_menu_status_smp(int dest, int status)
{
	unsigned char msg[3];
	unsigned char index = cec_dev->my_log_addr;

	msg[0] = ((index & 0xf) << 4) | dest;
	msg[1] = CEC_OC_MENU_STATUS;
	if (status == DEVICE_MENU_ACTIVE)
		msg[2] = DEVICE_MENU_ACTIVE;
	else
		msg[2] = DEVICE_MENU_INACTIVE;
	cec_ll_tx(msg, 3);
}

static void cec_inactive_source(int dest)
{
	unsigned char index = cec_dev->my_log_addr;
	unsigned char msg[4];
	unsigned char phy_addr_ab, phy_addr_cd;

	phy_addr_ab = (cec_dev->phy_addr >> 8) & 0xff;
	phy_addr_cd = (cec_dev->phy_addr >> 0) & 0xff;
	msg[0] = ((index & 0xf) << 4) | dest;
	msg[1] = CEC_OC_INACTIVE_SOURCE;
	msg[2] = phy_addr_ab;
	msg[3] = phy_addr_cd;

	cec_ll_tx(msg, 4);
}

static void cec_set_osd_name(int dest)
{
	unsigned char index = cec_dev->my_log_addr;
	unsigned char osd_len = strlen(cec_dev->osd_name);
	unsigned char msg[16];

	if (dest != 0xf) {
		msg[0] = ((index & 0xf) << 4) | dest;
		msg[1] = CEC_OC_SET_OSD_NAME;
		memcpy(&msg[2], cec_dev->osd_name, osd_len);

		cec_ll_tx(msg, 2 + osd_len);
	}
}

static void cec_active_source_smp(void)
{
	unsigned char msg[4];
	unsigned char index = cec_dev->my_log_addr;
	unsigned char phy_addr_ab;
	unsigned char phy_addr_cd;

	phy_addr_ab = (cec_dev->phy_addr >> 8) & 0xff;
	phy_addr_cd = (cec_dev->phy_addr >> 0) & 0xff;
	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_ACTIVE_SOURCE;
	msg[2] = phy_addr_ab;
	msg[3] = phy_addr_cd;
	cec_ll_tx(msg, 4);
}

static void cec_request_active_source(void)
{
	unsigned char msg[2];
	unsigned char index = cec_dev->my_log_addr;

	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_REQUEST_ACTIVE_SOURCE;
	cec_ll_tx(msg, 2);
}

static void cec_set_stream_path(unsigned char *msg)
{
	unsigned int phy_addr_active;

	phy_addr_active = (unsigned int)(msg[2] << 8 | msg[3]);
	if (phy_addr_active == cec_dev->phy_addr) {
		cec_active_source_smp();
		/*
		 * some types of TV such as panasonic need to send menu status,
		 * otherwise it will not send remote key event to control
		 * device's menu
		 */
		cec_menu_status_smp(msg[0] >> 4, DEVICE_MENU_ACTIVE);
	}
}

static void cec_report_power_status(int dest, int status)
{
	unsigned char index = cec_dev->my_log_addr;
	unsigned char msg[3];

	msg[0] = ((index & 0xf) << 4) | dest;
	msg[1] = CEC_OC_REPORT_POWER_STATUS;
	msg[2] = status;
	cec_ll_tx(msg, 3);
}

static void cec_rx_process(void)
{
	int len = cec_dev->rx_len;
	int initiator, follower;
	int opcode;
	unsigned char msg[MAX_MSG] = {};
	int dest_phy_addr;

	if (len < 2 || !cec_dev->new_msg)	/* ignore ping message */
		return;

	memcpy(msg, cec_dev->rx_msg, len);
	initiator = ((msg[0] >> 4) & 0xf);
	follower  = msg[0] & 0xf;
	if (follower != 0xf && follower != cec_dev->my_log_addr) {
		CEC_ERR("wrong rx message of bad follower:%x", follower);
		return;
	}
	opcode = msg[1];
	switch (opcode) {
	case CEC_OC_ACTIVE_SOURCE:
		if (cec_dev->wake_ok == 0) {
			int phy_addr = msg[2] << 8 | msg[3];

			if (phy_addr == INVALID_PHY_ADDR)
				break;
			cec_dev->wake_ok = 1;
			phy_addr |= (initiator << 16);
			writel(phy_addr, cec_dev->cec_reg + AO_RTI_STATUS_REG1);
			CEC_INFO("found wake up source:%x", phy_addr);
		}
		break;

	case CEC_OC_ROUTING_CHANGE:
		dest_phy_addr = msg[4] << 8 | msg[5];
		if (dest_phy_addr == cec_dev->phy_addr) {
			CEC_INFO("wake up by ROUTING_CHANGE\n");
			cec_key_report(0);
		}
		break;

	case CEC_OC_GET_CEC_VERSION:
		cec_give_version(initiator);
		break;

	case CEC_OC_GIVE_DECK_STATUS:
		cec_give_deck_status(initiator);
		break;

	case CEC_OC_GIVE_PHYSICAL_ADDRESS:
		cec_report_physical_address_smp();
		break;

	case CEC_OC_GIVE_DEVICE_VENDOR_ID:
		cec_device_vendor_id();
		break;

	case CEC_OC_GIVE_OSD_NAME:
		cec_set_osd_name(initiator);
		break;

	case CEC_OC_STANDBY:
		cec_inactive_source(initiator);
		cec_menu_status_smp(initiator, DEVICE_MENU_INACTIVE);
		break;

	case CEC_OC_SET_STREAM_PATH:
		cec_set_stream_path(msg);
		/* wake up if in early suspend */
		if (cec_dev->cec_suspend == CEC_EARLY_SUSPEND)
			cec_key_report(0);
		break;

	case CEC_OC_REQUEST_ACTIVE_SOURCE:
		if (!cec_dev->cec_suspend)
			cec_active_source_smp();
		break;

	case CEC_OC_GIVE_DEVICE_POWER_STATUS:
		if (cec_dev->cec_suspend)
			cec_report_power_status(initiator, POWER_STANDBY);
		else
			cec_report_power_status(initiator, POWER_ON);
		break;

	case CEC_OC_USER_CONTROL_PRESSED:
		/* wake up by key function */
		if (cec_dev->cec_suspend == CEC_EARLY_SUSPEND) {
			if (msg[2] == 0x40 || msg[2] == 0x6d)
				cec_key_report(0);
		}
		break;

	case CEC_OC_MENU_REQUEST:
		if (cec_dev->cec_suspend)
			cec_menu_status_smp(initiator, DEVICE_MENU_INACTIVE);
		else
			cec_menu_status_smp(initiator, DEVICE_MENU_ACTIVE);
		break;

	default:
		CEC_ERR("unsupported command:%x\n", opcode);
		break;
	}
	cec_dev->new_msg = 0;
}

static bool cec_service_suspended(void)
{
	/* service is not enabled */
	if (!(cec_dev->hal_flag & (1 << HDMI_OPTION_SERVICE_FLAG)))
		return false;
	if (!(cec_dev->hal_flag & (1 << HDMI_OPTION_SYSTEM_CEC_CONTROL)))
		return true;
	return false;
}

static void cec_task(struct work_struct *work)
{
	struct delayed_work *dwork;

	dwork = &cec_dev->cec_work;
	if (cec_dev && (!cec_dev->wake_ok || cec_service_suspended()))
		cec_rx_process();

	if (!cec_late_check_rx_buffer())
		queue_delayed_work(cec_dev->cec_thread, dwork, CEC_FRAME_DELAY);
}

static irqreturn_t cec_isr_handler(int irq, void *dev_instance)
{
	unsigned int intr_stat = 0;
	struct delayed_work *dwork;

	dwork = &cec_dev->cec_work;
	intr_stat = cec_intr_stat();
	if (intr_stat & (1<<1)) {   /* aocec tx intr */
		tx_irq_handle();
		return IRQ_HANDLED;
	}
	if ((-1) == cec_ll_rx(cec_dev->rx_msg))
		return IRQ_HANDLED;

	complete(&cec_dev->rx_ok);
	/* check rx buffer is full */
	cec_dev->new_msg = 1;
	mod_delayed_work(cec_dev->cec_thread, dwork, 0);
	return IRQ_HANDLED;
}

static void check_wake_up(void)
{
	if (cec_dev->wake_ok == 0)
		cec_request_active_source();
}

/******************** cec class interface *************************/
static ssize_t device_type_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cec_dev->dev_type);
}

static ssize_t device_type_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long type;

	if (kstrtol(buf, 10, &type))
		return -EINVAL;

	cec_dev->dev_type = type;
	CEC_ERR("set dev_type to %ld\n", type);
	return count;
}

static ssize_t menu_language_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	char a, b, c;

	a = ((cec_dev->menu_lang >> 16) & 0xff);
	b = ((cec_dev->menu_lang >>  8) & 0xff);
	c = ((cec_dev->menu_lang >>  0) & 0xff);
	return sprintf(buf, "%c%c%c\n", a, b, c);
}

static ssize_t menu_language_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	char a, b, c;

	if (sscanf(buf, "%c%c%c", &a, &b, &c) != 3)
		return -EINVAL;

	cec_dev->menu_lang = (a << 16) | (b << 8) | c;
	CEC_ERR("set menu_language to %s\n", buf);
	return count;
}

static ssize_t vendor_id_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", cec_dev->vendor_id);
}

static ssize_t vendor_id_store(struct class *cla, struct class_attribute *attr,
	const char *buf, size_t count)
{
	int id;

	if (kstrtoint(buf, 16, &id))
		return -EINVAL;
	cec_dev->vendor_id = id;
	return count;
}

static ssize_t port_num_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cec_dev->port_num);
}

static const char * const cec_reg_name1[] = {
	"CEC_TX_MSG_LENGTH",
	"CEC_TX_MSG_CMD",
	"CEC_TX_WRITE_BUF",
	"CEC_TX_CLEAR_BUF",
	"CEC_RX_MSG_CMD",
	"CEC_RX_CLEAR_BUF",
	"CEC_LOGICAL_ADDR0",
	"CEC_LOGICAL_ADDR1",
	"CEC_LOGICAL_ADDR2",
	"CEC_LOGICAL_ADDR3",
	"CEC_LOGICAL_ADDR4",
	"CEC_CLOCK_DIV_H",
	"CEC_CLOCK_DIV_L"
};

static const char * const cec_reg_name2[] = {
	"CEC_RX_MSG_LENGTH",
	"CEC_RX_MSG_STATUS",
	"CEC_RX_NUM_MSG",
	"CEC_TX_MSG_STATUS",
	"CEC_TX_NUM_MSG"
};

static ssize_t dump_reg_show(struct class *cla,
	struct class_attribute *attr, char *b)
{
	int i, s = 0;

	s += sprintf(b + s, "TX buffer:\n");
	for (i = 0; i <= CEC_TX_MSG_F_OP14; i++)
		s += sprintf(b + s, "%2d:%2x\n", i, aocec_rd_reg(i));

	for (i = 0; i < ARRAY_SIZE(cec_reg_name1); i++) {
		s += sprintf(b + s, "%s:%2x\n",
			     cec_reg_name1[i], aocec_rd_reg(i + 0x10));
	}

	s += sprintf(b + s, "RX buffer:\n");
	for (i = 0; i <= CEC_TX_MSG_F_OP14; i++)
		s += sprintf(b + s, "%2d:%2x\n", i, aocec_rd_reg(i + 0x80));

	for (i = 0; i < ARRAY_SIZE(cec_reg_name2); i++) {
		s += sprintf(b + s, "%s:%2x\n",
			     cec_reg_name2[i], aocec_rd_reg(i + 0x90));
	}
	return s;
}

static ssize_t arc_port_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", cec_dev->arc_port);
}

static ssize_t osd_name_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", cec_dev->osd_name);
}

static ssize_t port_seq_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", cec_dev->port_seq);
}

static ssize_t port_status_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	unsigned int tmp;
	unsigned int tx_hpd;

	tx_hpd = cec_dev->hpd_state;
	if (cec_dev->dev_type == DEV_TYPE_PLAYBACK) {
		tmp = tx_hpd;
		return sprintf(buf, "%x\n", tmp);
	}

	tmp = hdmirx_rd_top(TOP_HPD_PWR5V);
	CEC_INFO("TOP_HPD_PWR5V:%x\n", tmp);
	tmp >>= 20;
	tmp &= 0xf;
	tmp |= (tx_hpd << 16);
	return sprintf(buf, "%x\n", tmp);
}

static ssize_t pin_status_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	unsigned int tx_hpd = 0;
	char p;

	tx_hpd = cec_dev->hpd_state;
	if (cec_dev->dev_type == DEV_TYPE_PLAYBACK) {
		if (!tx_hpd) {
			cec_dev->pin_status = 0;
			return sprintf(buf, "%s\n", "disconnected");
		}
		if (cec_dev->pin_status == 0) {
			p = (cec_dev->my_log_addr << 4) | CEC_TV_ADDR;
			if (cec_ll_tx(&p, 1) == CEC_FAIL_NONE)
				return sprintf(buf, "%s\n", "ok");
			else
				return sprintf(buf, "%s\n", "fail");
		} else
			return sprintf(buf, "%s\n", "ok");
	} else {
		return sprintf(buf, "%s\n",
			       cec_dev->pin_status ? "ok" : "fail");
	}
}

static ssize_t physical_addr_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	unsigned int tmp = cec_dev->phy_addr;

	return sprintf(buf, "%04x\n", tmp);
}

static ssize_t physical_addr_store(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	int addr;

	if (kstrtoint(buf, 16, &addr))
		return -EINVAL;

	if (addr > INVALID_PHY_ADDR || addr < 0) {
		CEC_ERR("invalid input:%s\n", buf);
		cec_dev->phy_addr_test = 0;
		return -EINVAL;
	}
	cec_dev->phy_addr = addr;
	cec_dev->phy_addr_test = 1;
	return count;
}

static ssize_t dbg_en_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", cec_dev->cec_msg_dbg_en);
}

static ssize_t dbg_en_store(struct class *cla, struct class_attribute *attr,
	const char *buf, size_t count)
{
	int en;

	if (kstrtoint(buf, 10, &en))
		return -EINVAL;

	cec_dev->cec_msg_dbg_en = en ? 1 : 0;
	return count;
}

static ssize_t cmd_store(struct class *cla, struct class_attribute *attr,
	const char *bu, size_t count)
{
	char buf[16] = {};
	int cnt;

	cnt = sscanf(bu, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
		    (int *)&buf[0], (int *)&buf[1], (int *)&buf[2],
		    (int *)&buf[3], (int *)&buf[4], (int *)&buf[5],
		    (int *)&buf[6], (int *)&buf[7], (int *)&buf[8],
		    (int *)&buf[9], (int *)&buf[10], (int *)&buf[11],
		    (int *)&buf[12], (int *)&buf[13], (int *)&buf[14],
		    (int *)&buf[15]);
	if (cnt < 0)
		return -EINVAL;
	cec_ll_tx(buf, cnt);
	return count;
}

static ssize_t wake_up_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	unsigned int reg = readl(cec_dev->cec_reg + AO_RTI_STATUS_REG1);

	return sprintf(buf, "%x\n", reg & 0xfffff);
}

static ssize_t fun_cfg_store(struct class *cla, struct class_attribute *attr,
	const char *bu, size_t count)
{
	int cnt, val;

	cnt = kstrtoint(bu, 16, &val);
	if (cnt < 0 || val > 0xff)
		return -EINVAL;
	cec_config(val, 1);
	if (val == 0)
		cec_keep_reset();
	else
		cec_pre_init();
	return count;
}

static ssize_t fun_cfg_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	unsigned int reg = cec_config(0, 0);

	return sprintf(buf, "0x%x\n", reg & 0xff);
}

static ssize_t log_addr_store(struct class *cla, struct class_attribute *attr,
	const char *bu, size_t count)
{
	int cnt, val;

	cnt = kstrtoint(bu, 16, &val);
	if (cnt < 0 || val > 0xf)
		return -EINVAL;
	cec_logicaddr_set(val);
	/* add by hal, to init some data structure */
	cec_dev->my_log_addr = val;
	cec_dev->power_status = POWER_ON;

	return count;
}

static ssize_t log_addr_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", cec_dev->my_log_addr);
}

static struct class_attribute aocec_class_attr[] = {
	__ATTR_WO(cmd),
	__ATTR_RO(port_num),
	__ATTR_RO(port_seq),
	__ATTR_RO(osd_name),
	__ATTR_RO(dump_reg),
	__ATTR_RO(port_status),
	__ATTR_RO(pin_status),
	__ATTR_RO(arc_port),
	__ATTR_RO(wake_up),
	__ATTR(physical_addr, 0664, physical_addr_show, physical_addr_store),
	__ATTR(vendor_id, 0664, vendor_id_show, vendor_id_store),
	__ATTR(menu_language, 0664, menu_language_show, menu_language_store),
	__ATTR(device_type, 0664, device_type_show, device_type_store),
	__ATTR(dbg_en, 0664, dbg_en_show, dbg_en_store),
	__ATTR(fun_cfg, 0664, fun_cfg_show, fun_cfg_store),
	__ATTR(log_addr, 0664, log_addr_show, log_addr_store),
	__ATTR_NULL
};

/******************** cec hal interface ***************************/
static int hdmitx_cec_open(struct inode *inode, struct file *file)
{
	cec_dev->open_count++;
	if (cec_dev->open_count) {
		cec_dev->hal_control = 1;
		/* set default logical addr flag for uboot */
		cec_set_reg_bits(AO_DEBUG_REG1, 0xf, 16, 4);
	}
	return 0;
}

static int hdmitx_cec_release(struct inode *inode, struct file *file)
{
	cec_dev->open_count--;
	if (!cec_dev->open_count)
		cec_dev->hal_control = 0;
	return 0;
}

static ssize_t hdmitx_cec_read(struct file *f, char __user *buf,
			   size_t size, loff_t *p)
{
	int ret;

	if ((cec_dev->hal_flag & (1 << HDMI_OPTION_SYSTEM_CEC_CONTROL)))
		cec_dev->rx_len = 0;
	ret = wait_for_completion_timeout(&cec_dev->rx_ok, CEC_FRAME_DELAY);
	if (ret <= 0)
		return ret;
	if (cec_dev->rx_len == 0)
		return 0;

	if (copy_to_user(buf, cec_dev->rx_msg, cec_dev->rx_len))
		return -EINVAL;
	return cec_dev->rx_len;
}

static ssize_t hdmitx_cec_write(struct file *f, const char __user *buf,
			    size_t size, loff_t *p)
{
	unsigned char tempbuf[16] = {};
	int ret;

	if (size > 16)
		size = 16;
	if (size <= 0)
		return -EINVAL;

	if (copy_from_user(tempbuf, buf, size))
		return -EINVAL;

	ret = cec_ll_tx(tempbuf, size);
	return ret;
}

static void init_cec_port_info(struct hdmi_port_info *port,
			       struct ao_cec_dev *cec_dev)
{
	unsigned int a, b, c;
	unsigned int phy_head = 0xf000, phy_app = 0x1000, phy_addr;

	/* physical address for TV or repeator */
	if (cec_dev->dev_type == DEV_TYPE_TV)
		phy_addr = 0;
	else if (cec_dev->phy_addr != INVALID_PHY_ADDR)
		phy_addr = cec_dev->phy_addr;
	else
		phy_addr = 0;

	/* found physical address append for repeator */
	for (a = 0; a < 4; a++) {
		if (phy_addr & phy_head) {
			phy_head >>= 4;
			phy_app  >>= 4;
		} else
			break;
	}
	if (cec_dev->dev_type == DEV_TYPE_TUNER)
		b = cec_dev->port_num - 1;
	else
		b = cec_dev->port_num;

	/* init for port info */
	for (a = 0; a < b; a++) {
		port[a].type = HDMI_INPUT;
		port[a].port_id = a + 1;
		port[a].cec_supported = 1;
		/* set ARC feature according mask */
		if (cec_dev->arc_port & (1 << a))
			port[a].arc_supported = 1;
		else
			port[a].arc_supported = 0;

		/* set port physical address according port sequence */
		if (cec_dev->port_seq) {
			c = (cec_dev->port_seq >> (4 * a)) & 0xf;
			port[a].physical_address = (c + 1) * phy_app + phy_addr;
		} else {
			/* asending order if port_seq is not set */
			port[a].physical_address = (a + 1) * phy_app + phy_addr;
		}
	}

	if (cec_dev->dev_type == DEV_TYPE_TUNER) {
		/* last port is for tx in mixed tx/rx */
		port[a].type = HDMI_OUTPUT;
		port[a].port_id = a + 1;
		port[a].cec_supported = 1;
		port[a].arc_supported = 0;
		port[a].physical_address = phy_addr;
	}
}

static long hdmitx_cec_ioctl(struct file *f,
			     unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	unsigned long tmp = 0, i;
	struct hdmi_port_info *port;
	int tx_hpd = 0;

	switch (cmd) {
	case CEC_IOC_GET_PHYSICAL_ADDR:
		check_physical_addr_valid(20);
		if (cec_dev->dev_type ==  DEV_TYPE_PLAYBACK &&
		    !cec_dev->phy_addr_test) {
			/* physical address for mbox */
			if (cec_dev->phy_addr == INVALID_PHY_ADDR)
				return -EINVAL;
			tmp = cec_dev->phy_addr;
		} else {
			if (cec_dev->dev_type == DEV_TYPE_TV)
				tmp = 0;
			/* for repeator */
			else if (cec_dev->phy_addr != INVALID_PHY_ADDR)
				tmp = cec_dev->phy_addr;
			else
				tmp = 0;
		}
		if (!cec_dev->phy_addr_test) {
			cec_dev->phy_addr = tmp;
			cec_phyaddr_config(tmp, 1);
		} else
			tmp = cec_dev->phy_addr;

		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_VERSION:
		tmp = cec_dev->cec_version;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_VENDOR_ID:
		tmp = cec_dev->vendor_id;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_PORT_NUM:
		tmp = cec_dev->port_num;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_PORT_INFO:
		port = kcalloc(cec_dev->port_num, sizeof(*port), GFP_KERNEL);
		if (!port) {
			CEC_ERR("no memory\n");
			return -EINVAL;
		}
		if (cec_dev->dev_type == DEV_TYPE_PLAYBACK) {
			/* for tx only 1 port */
			tmp = cec_dev->phy_addr;
			port->type = HDMI_OUTPUT;
			port->port_id = 1;
			port->cec_supported = 1;
			/* not support arc for tx */
			port->arc_supported = 0;
			port->physical_address = tmp & INVALID_PHY_ADDR;
			if (copy_to_user(argp, port, sizeof(*port))) {
				kfree(port);
				return -EINVAL;
			}
		} else {
			tmp = cec_dev->port_num;
			init_cec_port_info(port, cec_dev);
			if (copy_to_user(argp, port, sizeof(*port) * tmp)) {
				kfree(port);
				return -EINVAL;
			}
		}
		kfree(port);
		break;

	case CEC_IOC_SET_OPTION_WAKEUP:
		tmp = cec_config(0, 0);
		tmp &= ~(1 << AUTO_POWER_ON_MASK);
		tmp |=  (arg << AUTO_POWER_ON_MASK);
		cec_config(tmp, 1);
		break;

	case CEC_IOC_SET_AUTO_DEVICE_OFF:
		tmp = cec_config(0, 0);
		tmp &= ~(1 << ONE_TOUCH_STANDBY_MASK);
		tmp |=  (arg << ONE_TOUCH_STANDBY_MASK);
		cec_config(tmp, 1);
		break;

	case CEC_IOC_SET_OPTION_ENALBE_CEC:
		tmp = (1 << HDMI_OPTION_ENABLE_CEC);
		if (arg) {
			cec_dev->hal_flag |= tmp;
			cec_config(0x2f, 1);
			cec_pre_init();
		} else {
			cec_dev->hal_flag &= ~(tmp);
			CEC_INFO("disable CEC\n");
			cec_config(0x0, 1);
			cec_keep_reset();
		}
		break;

	case CEC_IOC_SET_OPTION_SYS_CTRL:
		tmp = (1 << HDMI_OPTION_SYSTEM_CEC_CONTROL);
		if (arg) {
			cec_dev->hal_flag |= tmp;
			cec_config(0x2f, 1);
		} else
			cec_dev->hal_flag &= ~(tmp);
		cec_dev->hal_flag |= (1 << HDMI_OPTION_SERVICE_FLAG);
		break;

	case CEC_IOC_SET_OPTION_SET_LANG:
		cec_dev->menu_lang = arg;
		break;

	case CEC_IOC_GET_CONNECT_STATUS:
		tx_hpd = cec_dev->hpd_state;
		if (cec_dev->dev_type == DEV_TYPE_PLAYBACK)
			tmp = tx_hpd;
		else {
			if (copy_from_user(&i, argp, _IOC_SIZE(cmd)))
				return -EINVAL;
			if (cec_dev->port_num == i &&
			    cec_dev->dev_type == DEV_TYPE_TUNER)
				tmp = tx_hpd;
			else {	/* mixed for rx */
				tmp = (hdmirx_rd_top(TOP_HPD_PWR5V) >> 20);
				if (tmp & (1 << (i - 1)))
					tmp = 1;
				else
					tmp = 0;
			}
		}
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_ADD_LOGICAL_ADDR:
		tmp = arg & 0xf;
		cec_logicaddr_set(tmp);
		/* add by hal, to init some data structure */
		cec_dev->my_log_addr = tmp;
		if (cec_dev->dev_type != DEV_TYPE_PLAYBACK)
			check_wake_up();
		break;

	case CEC_IOC_CLR_LOGICAL_ADDR:
		/* TODO: clear global info */
		break;

	case CEC_IOC_SET_DEV_TYPE:
		if (arg < DEV_TYPE_TV && arg > DEV_TYPE_VIDEO_PROCESSOR)
			return -EINVAL;
		cec_dev->dev_type = arg;
		break;

	case CEC_IOC_SET_ARC_ENABLE:
		/* select arc according arg */
		if (arg)
			hdmirx_wr_top(TOP_ARCTX_CNTL, 0x03);
		else
			hdmirx_wr_top(TOP_ARCTX_CNTL, 0x00);
		CEC_INFO("set arc en:%ld, reg:%lx\n",
			 arg, hdmirx_rd_top(TOP_ARCTX_CNTL));
		break;

	default:
		break;
	}
	return 0;
}

#ifdef CONFIG_COMPAT
static long hdmitx_cec_compat_ioctl(struct file *f,
				    unsigned int cmd, unsigned long arg)
{
	arg = (unsigned long)compat_ptr(arg);
	return hdmitx_cec_ioctl(f, cmd, arg);
}
#endif

/* for improve rw permission */
static char *aml_cec_class_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0666;
	CEC_INFO("mode is %x\n", *mode);
	return NULL;
}

static struct class aocec_class = {
	.name = CEC_DEV_NAME,
	.class_attrs = aocec_class_attr,
	.devnode = aml_cec_class_devnode,
};


static const struct file_operations hdmitx_cec_fops = {
	.owner          = THIS_MODULE,
	.open           = hdmitx_cec_open,
	.read           = hdmitx_cec_read,
	.write          = hdmitx_cec_write,
	.release        = hdmitx_cec_release,
	.unlocked_ioctl = hdmitx_cec_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = hdmitx_cec_compat_ioctl,
#endif
};

/************************ cec high level code *****************************/
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
struct early_suspend aocec_suspend_handler;
static void aocec_early_suspend(struct early_suspend *h)
{
	cec_dev->cec_suspend = CEC_EARLY_SUSPEND;
	CEC_INFO("%s, suspend:%d\n", __func__, cec_dev->cec_suspend);
}

static void aocec_late_resume(struct early_suspend *h)
{
	cec_dev->cec_suspend = 0;
	CEC_INFO("%s, suspend:%d\n", __func__, cec_dev->cec_suspend);

}
#endif

static int aml_cec_probe(struct platform_device *pdev)
{
	struct device *cdev;
#ifdef CONFIG_OF
	struct device_node *node = pdev->dev.of_node;
	unsigned int tmp;
	int irq_idx = 0, r, num;
	const char *irq_name = NULL;
	struct pinctrl *p;
	struct resource *res;
	resource_size_t *base;
#endif

	cec_dev = kzalloc(sizeof(struct ao_cec_dev), GFP_KERNEL);
	if (!cec_dev)
		return -ENOMEM;
	cec_dev->cec_msg_dbg_en = DEFAULT_DEBUG_EN;
	cec_dev->dev_type = DEV_TYPE_PLAYBACK;
	cec_dev->dbg_dev  = &pdev->dev;
	cec_dev->wake_ok  = 1;
	cec_dev->phy_addr = INVALID_PHY_ADDR;

	/* cdev registe */
	r = class_register(&aocec_class);
	if (r) {
		CEC_ERR("regist class failed\n");
		return -EINVAL;
	}
	pdev->dev.class = &aocec_class;
	cec_dev->dev_no = register_chrdev(0, CEC_DEV_NAME,
					  &hdmitx_cec_fops);
	if (cec_dev->dev_no < 0) {
		CEC_ERR("alloc chrdev failed\n");
		return -EINVAL;
	}
	CEC_INFO("alloc chrdev %x\n", cec_dev->dev_no);
	cdev = device_create(&aocec_class, &pdev->dev,
			     MKDEV(cec_dev->dev_no, 0),
			     NULL, CEC_DEV_NAME);
	if (IS_ERR(cdev)) {
		CEC_ERR("create chrdev failed, dev:%p\n", cdev);
		unregister_chrdev(cec_dev->dev_no,
				  CEC_DEV_NAME);
		return -EINVAL;
	}

	init_completion(&cec_dev->rx_ok);
	init_completion(&cec_dev->tx_ok);
	mutex_init(&cec_dev->cec_mutex);
	spin_lock_init(&cec_dev->cec_reg_lock);
	cec_dev->cec_thread = create_workqueue("cec_work");
	if (cec_dev->cec_thread == NULL) {
		CEC_INFO("create work queue failed\n");
		return -EFAULT;
	}
	INIT_DELAYED_WORK(&cec_dev->cec_work, cec_task);
	cec_dev->remote_cec_dev = input_allocate_device();
	if (!cec_dev->remote_cec_dev)
		CEC_INFO("No enough memory\n");

	cec_dev->remote_cec_dev->name = "cec_input";

	cec_dev->remote_cec_dev->evbit[0] = BIT_MASK(EV_KEY);
	cec_dev->remote_cec_dev->keybit[BIT_WORD(BTN_0)] =
		BIT_MASK(BTN_0);
	cec_dev->remote_cec_dev->id.bustype = BUS_ISA;
	cec_dev->remote_cec_dev->id.vendor = 0x1b8e;
	cec_dev->remote_cec_dev->id.product = 0x0cec;
	cec_dev->remote_cec_dev->id.version = 0x0001;

	set_bit(KEY_POWER, cec_dev->remote_cec_dev->keybit);

	if (input_register_device(cec_dev->remote_cec_dev)) {
		CEC_INFO("Failed to register device\n");
		input_free_device(cec_dev->remote_cec_dev);
	}

#ifdef CONFIG_OF
	/* pinmux set */
	if (of_get_property(node, "pinctrl-names", NULL)) {
		r = of_property_read_string(node,
					    "pinctrl-names",
					    &cec_dev->pin_name);
		if (!r)
			p = devm_pinctrl_get_select(&pdev->dev,
						    cec_dev->pin_name);
	}

	/* irq set */
	irq_idx = of_irq_get(node, 0);
	cec_dev->irq_cec = irq_idx;
	if (of_get_property(node, "interrupt-names", NULL)) {
		r = of_property_read_string(node, "interrupt-names", &irq_name);
		if (!r) {
			r = request_irq(irq_idx, &cec_isr_handler, IRQF_SHARED,
					irq_name, (void *)cec_dev);
		}
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		base = ioremap(res->start, res->end - res->start);
		cec_dev->cec_reg = (void *)base;
	} else {
		CEC_ERR("no CEC reg resource\n");
		cec_dev->cec_reg = NULL;
	}

	r = of_property_read_u32(node, "port_num", &num);
	if (r) {
		CEC_ERR("not find 'port_num'\n");
		cec_dev->port_num = 1;
	} else
		cec_dev->port_num = num;
	r = of_property_read_u32(node, "arc_port_mask", &num);
	if (r) {
		CEC_ERR("not find 'arc_port_mask'\n");
		cec_dev->arc_port = 0;
	} else
		cec_dev->arc_port = num;

	r = of_property_read_u32(node, "vendor_id", &(cec_dev->vendor_id));
	if (r)
		CEC_INFO("not find vendor id\n");

	r = of_property_read_string(node, "cec_osd_string",
		(const char **)&(cec_dev->osd_name));
	if (r) {
		CEC_INFO("not find cec osd string\n");
		cec_dev->osd_name = "Amlogic";
	}

	r = of_property_read_u32(node, "cec_version",
		&(tmp));
	if (r) {
		/* default set to 2.0 */
		CEC_INFO("not find cec_version\n");
		cec_dev->cec_version = CEC_VERSION_20;
	} else
		cec_dev->cec_version = tmp;

	/* get port sequence */
	node = of_find_node_by_path("/hdmirx");
	if (node == NULL) {
		CEC_ERR("can't find hdmirx\n");
		cec_dev->port_seq = 0;
	} else {
		r = of_property_read_u32(node, "rx_port_maps",
					 &(cec_dev->port_seq));
		if (r)
			CEC_INFO("not find rx_port_maps\n");
	}
#endif

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	aocec_suspend_handler.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 20;
	aocec_suspend_handler.suspend = aocec_early_suspend;
	aocec_suspend_handler.resume  = aocec_late_resume;
	aocec_suspend_handler.param   = cec_dev;
	register_early_suspend(&aocec_suspend_handler);
#endif
	device_init_wakeup(cec_dev->dbg_dev, 1);

	hrtimer_init(&cec_dev->start_bit_check,
		     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cec_dev->start_bit_check.function = cec_line_check;
	cec_dev->hdmitx_nb.notifier_call = cec_hdmi_tx_notify_handler;
	hdmitx_event_notifier_regist(&cec_dev->hdmitx_nb);
	/* for init */
	cec_pre_init();
	queue_delayed_work(cec_dev->cec_thread, &cec_dev->cec_work, 0);
	return 0;
}

static __exit int aml_cec_remove(struct platform_device *pdev)
{
	CEC_INFO("cec uninit!\n");
	free_irq(cec_dev->irq_cec, (void *)cec_dev);

	hdmitx_event_notifier_unregist(&cec_dev->hdmitx_nb);
	if (cec_dev->cec_thread) {
		cancel_delayed_work_sync(&cec_dev->cec_work);
		destroy_workqueue(cec_dev->cec_thread);
	}
	input_unregister_device(cec_dev->remote_cec_dev);
	unregister_chrdev(cec_dev->dev_no, CEC_DEV_NAME);
	class_unregister(&aocec_class);
	kfree(cec_dev);
	return 0;
}

#ifdef CONFIG_PM
static int aml_cec_pm_prepare(struct device *dev)
{
	cec_dev->cec_suspend = CEC_DEEP_SUSPEND;
	CEC_INFO("%s, cec_suspend:%d\n", __func__, cec_dev->cec_suspend);
	return 0;
}
static const struct dev_pm_ops aml_cec_pm = {
	.prepare  = aml_cec_pm_prepare,
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id aml_cec_dt_match[] = {
	{
		.compatible = "amlogic, amlogic-aocec",
	},
	{},
};
#endif

static struct platform_driver aml_cec_driver = {
	.driver = {
		.name  = "cectx",
		.owner = THIS_MODULE,
	#ifdef CONFIG_PM
		.pm     = &aml_cec_pm,
	#endif
	#ifdef CONFIG_OF
		.of_match_table = aml_cec_dt_match,
	#endif
	},
	.probe  = aml_cec_probe,
	.remove = aml_cec_remove,
};

static int __init cec_init(void)
{
	int ret;

	ret = platform_driver_register(&aml_cec_driver);
	return ret;
}

static void __exit cec_uninit(void)
{
	platform_driver_unregister(&aml_cec_driver);
}


module_init(cec_init);
module_exit(cec_uninit);
MODULE_DESCRIPTION("AMLOGIC HDMI TX CEC driver");
MODULE_LICENSE("GPL");
