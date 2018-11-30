/*
 * drivers/amlogic/cec/hdmi_ao_cec.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY orFITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/random.h>
#include <linux/pinctrl/consumer.h>

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_cec_20.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/pm.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/jtag.h>
#include <linux/amlogic/scpi_protocol.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend aocec_suspend_handler;
#endif
#include "hdmi_ao_cec.h"


#define CEC_FRAME_DELAY		msecs_to_jiffies(400)
#define CEC_DEV_NAME		"cec"

#define CEC_POWER_ON		(0 << 0)
#define CEC_EARLY_SUSPEND	(1 << 0)
#define CEC_DEEP_SUSPEND	(1 << 1)
#define CEC_POWER_RESUME	(1 << 2)

#define HR_DELAY(n)		(ktime_set(0, n * 1000 * 1000))
#define MAX_INT    0x7ffffff

struct cec_platform_data_s {
	/*unsigned int chip_id;*/
	unsigned char line_reg;/*cec gpio_i reg:0  ao;1 periph*/
	unsigned int line_bit;/*cec gpio position in reg*/
	bool ee_to_ao;/*ee cec hw module mv to ao;ao cec delete*/
	bool ceca_sts_reg;/*add new internal status register*/
	enum cecbver cecb_ver;/* detail discription ref enum cecbver */
};


struct cec_wakeup_t {
	unsigned int wk_logic_addr:8;
	unsigned int wk_phy_addr:16;
	unsigned int wk_port_id:8;
};

/* global struct for tx and rx */
struct ao_cec_dev {
	unsigned long dev_type;
	struct device_node *node;
	unsigned int port_num;	/*total input hdmi port number*/
	unsigned int cec_num;
	unsigned int arc_port;
	unsigned int output;
	unsigned int hal_flag;
	unsigned int phy_addr;
	unsigned int port_seq;
	unsigned int cpu_type;
	unsigned long irq_ceca;
	unsigned long irq_cecb;
	void __iomem *exit_reg;
	void __iomem *cec_reg;
	void __iomem *hdmi_rxreg;
	void __iomem *hhi_reg;
	void __iomem *periphs_reg;
	struct hdmitx_dev *tx_dev;
	struct workqueue_struct *cec_thread;
	struct device *dbg_dev;
	const char *pin_name;
	struct delayed_work cec_work;
	struct completion rx_ok;
	struct completion tx_ok;
	spinlock_t cec_reg_lock;
	struct mutex cec_mutex;
	struct mutex cec_ioctl_mutex;
	struct cec_wakeup_t wakup_data;
	unsigned int wakeup_reason;
#ifdef CONFIG_PM
	int cec_suspend;
#endif
	struct vendor_info_data v_data;
	struct cec_global_info_t cec_info;
	struct cec_platform_data_s *plat_data;
};

struct cec_msg_last {
	unsigned char msg[MAX_MSG];
	int len;
	int last_result;
	unsigned long last_jiffies;
};
static struct cec_msg_last *last_cec_msg;
static struct dbgflg stdbgflg;

static int phy_addr_test;

/* from android cec hal */
enum {
	HDMI_OPTION_WAKEUP = 1,
	HDMI_OPTION_ENABLE_CEC = 2,
	HDMI_OPTION_SYSTEM_CEC_CONTROL = 3,
	HDMI_OPTION_SET_LANG = 5,
	HDMI_OPTION_SERVICE_FLAG = 16,
};

static struct ao_cec_dev *cec_dev;
static int cec_tx_result;

static int cec_line_cnt;
static struct hrtimer start_bit_check;

static unsigned char rx_msg[MAX_MSG];
static unsigned char rx_len;
static unsigned int  new_msg;
static bool wake_ok = 1;
static bool ee_cec;
static bool pin_status;
static unsigned int cec_msg_dbg_en;

#define CEC_ERR(format, args...)				\
	{if (cec_dev->dbg_dev)					\
		dev_err(cec_dev->dbg_dev, format, ##args);	\
	}

#define CEC_INFO(format, args...)				\
	{if (cec_msg_dbg_en && cec_dev->dbg_dev)		\
		dev_info(cec_dev->dbg_dev, format, ##args);	\
	}

#define CEC_INFO_L(level, format, args...)				\
	{if ((cec_msg_dbg_en >= level) && cec_dev->dbg_dev)		\
		dev_info(cec_dev->dbg_dev, format, ##args);	\
	}

static unsigned char msg_log_buf[128] = { 0 };

#define waiting_aocec_free(r) \
	do {\
		unsigned long cnt = 0;\
		while (readl(cec_dev->cec_reg + r) & (1<<23)) {\
			if (cnt++ == 3500) { \
				pr_info("waiting aocec %x free time out\n", r);\
				cec_hw_reset(CEC_A);\
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

unsigned int aocec_rd_reg(unsigned long addr)
{
	unsigned int data32;
	unsigned long flags;

	spin_lock_irqsave(&cec_dev->cec_reg_lock, flags);
	waiting_aocec_free(AO_CEC_RW_REG);
	data32 = 0;
	data32 |= 0 << 16; /* [16]	 cec_reg_wr */
	data32 |= 0 << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]	cec_reg_addr */
	writel(data32, cec_dev->cec_reg + AO_CEC_RW_REG);

	waiting_aocec_free(AO_CEC_RW_REG);
	data32 = ((readl(cec_dev->cec_reg + AO_CEC_RW_REG)) >> 24) & 0xff;
	spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
	return data32;
} /* aocec_rd_reg */

void aocec_wr_reg(unsigned long addr, unsigned long data)
{
	unsigned long data32;
	unsigned long flags;

	spin_lock_irqsave(&cec_dev->cec_reg_lock, flags);
	waiting_aocec_free(AO_CEC_RW_REG);
	data32 = 0;
	data32 |= 1 << 16; /* [16]	 cec_reg_wr */
	data32 |= data << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]	cec_reg_addr */
	writel(data32, cec_dev->cec_reg + AO_CEC_RW_REG);
	spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
} /* aocec_wr_only_reg */

/*------------for AO_CECB------------------*/
static unsigned int aocecb_rd_reg(unsigned long addr)
{
	unsigned int data32;
	unsigned long flags;
	unsigned int timeout = 0;

	spin_lock_irqsave(&cec_dev->cec_reg_lock, flags);
	data32 = 0;
	data32 |= 0 << 16; /* [16]	 cec_reg_wr */
	data32 |= 0 << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]	cec_reg_addr */
	writel(data32, cec_dev->cec_reg + AO_CECB_RW_REG);
	/* add for check access busy */
	data32 = readl(cec_dev->cec_reg + AO_CECB_RW_REG);
	while (data32 & (1 << 23)) {
		if (timeout++ > 200) {
			CEC_ERR("cecb access reg 0x%x fail\n",
				(unsigned int)addr);
			break;
		}
		data32 = readl(cec_dev->cec_reg + AO_CECB_RW_REG);
	}
	data32 = (data32 >> 24) & 0xff;
	spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
	return data32;
} /* aocecb_rd_reg */

static void aocecb_wr_reg(unsigned long addr, unsigned long data)
{
	unsigned long data32;
	unsigned long flags;

	spin_lock_irqsave(&cec_dev->cec_reg_lock, flags);
	data32 = 0;
	data32 |= 1 << 16; /* [16]	 cec_reg_wr */
	data32 |= data << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]	cec_reg_addr */
	writel(data32, cec_dev->cec_reg + AO_CECB_RW_REG);
	spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
} /* aocecb_wr_only_reg */

/*----------------- low level for EE cec rx/tx support ----------------*/
static inline void hdmirx_set_bits_top(uint32_t reg, uint32_t bits,
				       uint32_t start, uint32_t len)
{
	unsigned int tmp;

	tmp = hdmirx_rd_top(reg);
	tmp &= ~(((1 << len) - 1) << start);
	tmp |=  (bits << start);
	hdmirx_wr_top(reg, tmp);
}

static unsigned int hdmirx_cec_read(unsigned int reg)
{
	/*
	 * TXLX has moved ee cec to ao domain
	 */
	if (reg >= DWC_CEC_CTRL && cec_dev->plat_data->ee_to_ao)
		return aocecb_rd_reg((reg - DWC_CEC_CTRL) / 4);
	else
		return hdmirx_rd_dwc(reg);
}

/*only for ee cec*/
static void hdmirx_cec_write(unsigned int reg, unsigned int value)
{
	/*
	 * TXLX has moved ee cec to ao domain
	 */
	if (reg >= DWC_CEC_CTRL && cec_dev->plat_data->ee_to_ao)
		aocecb_wr_reg((reg - DWC_CEC_CTRL) / 4, value);
	else
		hdmirx_wr_dwc(reg, value);
}

static inline void hdmirx_set_bits_dwc(uint32_t reg, uint32_t bits,
				       uint32_t start, uint32_t len)
{
	unsigned int tmp;

	tmp = hdmirx_cec_read(reg);
	tmp &= ~(((1 << len) - 1) << start);
	tmp |=  (bits << start);
	hdmirx_cec_write(reg, tmp);
}

void cec_dbg_init(void)
{
	stdbgflg.hal_cmd_bypass = 0;
}

void cecb_hw_reset(void)
{
	/* cec disable */
	if (!cec_dev->plat_data->ee_to_ao)
		hdmirx_set_bits_dwc(DWC_DMI_DISABLE_IF, 0, 5, 1);
	else
		cec_set_reg_bits(AO_CECB_GEN_CNTL, 0, 0, 1);
	udelay(500);
}

static void cecrx_check_irq_enable(void)
{
	unsigned int reg32;

	/* irq on chip txlx has sperate from EE cec, no need check */
	if (cec_dev->plat_data->ee_to_ao)
		return;

	reg32 = hdmirx_cec_read(DWC_AUD_CEC_IEN);
	if ((reg32 & EE_CEC_IRQ_EN_MASK) != EE_CEC_IRQ_EN_MASK) {
		CEC_INFO("irq_en is wrong:%x, checker:%pf\n",
			 reg32, (void *)_RET_IP_);
		hdmirx_cec_write(DWC_AUD_CEC_IEN_SET, EE_CEC_IRQ_EN_MASK);
	}
}

static int cecb_trigle_tx(const unsigned char *msg, unsigned char len)
{
	int i = 0, size = 0;
	int lock;

	cecrx_check_irq_enable();
	while (1) {
		/* send is in process */
		lock = hdmirx_cec_read(DWC_CEC_LOCK);
		if (lock) {
			CEC_ERR("recevie msg in tx\n");
			cecb_irq_handle();
			return -1;
		}
		if (hdmirx_cec_read(DWC_CEC_CTRL) & 0x01)
			i++;
		else
			break;
		if (i > 25) {
			CEC_ERR("waiting busy timeout\n");
			return -1;
		}
		msleep(20);
	}
	size += sprintf(msg_log_buf + size, "CEC tx msg len %d:", len);
	for (i = 0; i < len; i++) {
		hdmirx_cec_write(DWC_CEC_TX_DATA0 + i * 4, msg[i]);
		size += sprintf(msg_log_buf + size, " %02x", msg[i]);
	}
	msg_log_buf[size] = '\0';
	CEC_INFO("%s\n", msg_log_buf);
	/* start send */
	hdmirx_cec_write(DWC_CEC_TX_CNT, len);
	hdmirx_set_bits_dwc(DWC_CEC_CTRL, 3, 0, 3);
	return 0;
}

int cec_has_irq(void)
{
	unsigned int intr_cec;

	if (!cec_dev->plat_data->ee_to_ao) {
		intr_cec = hdmirx_cec_read(DWC_AUD_CEC_ISTS);
		intr_cec &= EE_CEC_IRQ_EN_MASK;
	} else {
		intr_cec = readl(cec_dev->cec_reg + AO_CECB_INTR_STAT);
		intr_cec &= CECB_IRQ_EN_MASK;
	}
	return intr_cec;
}

static inline void cecrx_clear_irq(unsigned int flags)
{
	if (!cec_dev->plat_data->ee_to_ao)
		hdmirx_cec_write(DWC_AUD_CEC_ICLR, flags);
	else
		writel(flags, cec_dev->cec_reg + AO_CECB_INTR_CLR);
}

static int cecb_pick_msg(unsigned char *msg, unsigned char *out_len)
{
	int i, size;
	int len;

	len = hdmirx_cec_read(DWC_CEC_RX_CNT);
	size = sprintf(msg_log_buf, "CEC RX len %d:", len);
	for (i = 0; i < len; i++) {
		msg[i] = hdmirx_cec_read(DWC_CEC_RX_DATA0 + i * 4);
		size += sprintf(msg_log_buf + size, " %02x", msg[i]);
	}
	size += sprintf(msg_log_buf + size, "\n");
	msg_log_buf[size] = '\0';
	/* clr CEC lock bit */
	hdmirx_cec_write(DWC_CEC_LOCK, 0);
	CEC_INFO("%s", msg_log_buf);
	if (((msg[0] & 0xf0) >> 4) == cec_dev->cec_info.log_addr) {
		*out_len = 0;
		CEC_ERR("bad iniator with self:%s", msg_log_buf);
	} else
		*out_len = len;
	pin_status = 1;
	return 0;
}

void cecb_irq_handle(void)
{
	uint32_t intr_cec;
	uint32_t lock;
	int shift = 0;
	struct delayed_work *dwork;

	intr_cec = cec_has_irq();

	/* clear irq */
	if (intr_cec != 0)
		cecrx_clear_irq(intr_cec);

	if (cec_dev->plat_data->ee_to_ao)
		shift = 16;
	/* TX DONE irq, increase tx buffer pointer */
	if (intr_cec & CEC_IRQ_TX_DONE) {
		cec_tx_result = CEC_FAIL_NONE;
		complete(&cec_dev->tx_ok);
	}
	lock = hdmirx_cec_read(DWC_CEC_LOCK);
	/* EOM irq, message is coming */
	if ((intr_cec & CEC_IRQ_RX_EOM) || lock) {
		cecb_pick_msg(rx_msg, &rx_len);
		complete(&cec_dev->rx_ok);
		new_msg = 1;
		dwork = &cec_dev->cec_work;
		mod_delayed_work(cec_dev->cec_thread, dwork, 0);
	}

	/* TX error irq flags */
	if ((intr_cec & CEC_IRQ_TX_NACK)     ||
	    (intr_cec & CEC_IRQ_TX_ARB_LOST) ||
	    (intr_cec & CEC_IRQ_TX_ERR_INITIATOR)) {
		if (intr_cec & CEC_IRQ_TX_NACK) {
			cec_tx_result = CEC_FAIL_NACK;
			CEC_INFO_L(L_2, "warning:TX_NACK\n");
		} else if (intr_cec & CEC_IRQ_TX_ARB_LOST) {
			cec_tx_result = CEC_FAIL_BUSY;
			/* clear start */
			hdmirx_cec_write(DWC_CEC_TX_CNT, 0);
			hdmirx_set_bits_dwc(DWC_CEC_CTRL, 0, 0, 3);
			CEC_ERR("warning:ARB_LOST\n");
		} else if (intr_cec & CEC_IRQ_TX_ERR_INITIATOR) {
			CEC_ERR("warning:INITIATOR\n");
			cec_tx_result = CEC_FAIL_OTHER;
		} else
			cec_tx_result = CEC_FAIL_OTHER;
		complete(&cec_dev->tx_ok);
	}

	/* RX error irq flag */
	if (intr_cec & CEC_IRQ_RX_ERR_FOLLOWER) {
		CEC_ERR("warning:FOLLOWER\n");
		hdmirx_cec_write(DWC_CEC_LOCK, 0);
		/* TODO: need reset cec hw logic? */
	}

	/* wakeup op code will triger this int*/
	if (intr_cec & CEC_IRQ_RX_WAKEUP) {
		CEC_ERR("warning:RX_WAKEUP\n");
		hdmirx_cec_write(DWC_CEC_WKUPCTRL, WAKEUP_EN_MASK);
		/* TODO: wake up system if needed */
	}
}

static irqreturn_t cecb_isr(int irq, void *dev_instance)
{
	/*CEC_INFO("cecb_isr\n");*/
	cecb_irq_handle();
	return IRQ_HANDLED;
}

static void ao_cecb_init(void)
{
	unsigned long data32;
	unsigned int reg;

	cecb_hw_reset();

	if (!cec_dev->plat_data->ee_to_ao) {
		/* set cec clk 32768k */
		data32  = readl(cec_dev->hhi_reg + HHI_32K_CLK_CNTL);
		data32  = 0;
		/*
		 * [17:16] clk_sel: 0=oscin; 1=slow_oscin;
		 * 2=fclk_div3; 3=fclk_div5.
		 */
		data32 |= 0         << 16;
		/* [   15] clk_en */
		data32 |= 1         << 15;
		/* [13: 0] clk_div */
		data32 |= (732-1)   << 0;
		writel(data32, cec_dev->hhi_reg + HHI_32K_CLK_CNTL);
		hdmirx_wr_top(TOP_EDID_ADDR_CEC, EDID_CEC_ID_ADDR);

		/* hdmirx_cecclk_en */
		hdmirx_set_bits_top(TOP_CLK_CNTL, 1, 2, 1);
		hdmirx_set_bits_top(TOP_EDID_GEN_CNTL, EDID_AUTO_CEC_EN, 11, 1);

		/* enable all cec irq */
		/*cec_irq_enable(true);*/
		/* clear all wake up source */
		hdmirx_cec_write(DWC_CEC_WKUPCTRL, 0);
		/* cec enable */
		hdmirx_set_bits_dwc(DWC_DMI_DISABLE_IF, 1, 5, 1);
	} else {
		reg =   (0 << 31) |
			(0 << 30) |
			(1 << 28) |	/* clk_div0/clk_div1 in turn */
			((732-1) << 12) |/* Div_tcnt1 */
			((733-1) << 0);	/* Div_tcnt0 */
		writel(reg, cec_dev->cec_reg + AO_CECB_CLK_CNTL_REG0);
		reg =   (0 << 13) |
			((11-1)  << 12) |
			((8-1)  <<  0);
		writel(reg, cec_dev->cec_reg + AO_CECB_CLK_CNTL_REG1);

		reg = readl(cec_dev->cec_reg + AO_CECB_CLK_CNTL_REG0);
		reg |= (1 << 31);
		writel(reg, cec_dev->cec_reg + AO_CECB_CLK_CNTL_REG0);

		udelay(200);
		reg |= (1 << 30);
		writel(reg, cec_dev->cec_reg + AO_CECB_CLK_CNTL_REG0);

		reg = readl(cec_dev->cec_reg + AO_RTI_PWR_CNTL_REG0);
		reg |=  (0x01 << 14);	/* xtal gate */
		writel(reg, cec_dev->cec_reg + AO_RTI_PWR_CNTL_REG0);

		data32  = 0;
		data32 |= (7 << 12);	/* filter_del */
		data32 |= (1 <<  8);	/* filter_tick: 1us */
		data32 |= (1 <<  3);	/* enable system clock */
		data32 |= 0 << 1;	/* [2:1]	cntl_clk: */
			/* 0=Disable clk (Power-off mode); */
			/* 1=Enable gated clock (Normal mode); */
			/* 2=Enable free-run clk (Debug mode). */
		data32 |= 1 << 0;	/* [0]	  sw_reset: 1=Reset */
		writel(data32, cec_dev->cec_reg + AO_CECB_GEN_CNTL);
		/* Enable gated clock (Normal mode). */
		cec_set_reg_bits(AO_CECB_GEN_CNTL, 1, 1, 1);
		/* Release SW reset */
		cec_set_reg_bits(AO_CECB_GEN_CNTL, 0, 0, 1);

		if (cec_dev->plat_data->cecb_ver >= CECB_VER_2) {
			reg = 0;
			reg |= (0 << 6);/*curb_err_init*/
			reg |= (0 << 5);/*en_chk_sbitlow*/
			reg |= (2 << 0);/*rise_del_max*/
			hdmirx_cec_write(DWC_CEC_CTRL2, reg);
		}

		/* Enable all AO_CECB interrupt sources */
		/*cec_irq_enable(true);*/
		hdmirx_cec_write(DWC_CEC_WKUPCTRL, WAKEUP_EN_MASK);
	}
}

void eecec_irq_enable(bool enable)
{
	if (cec_dev->cpu_type < MESON_CPU_MAJOR_ID_TXLX) {
		if (enable)
			hdmirx_cec_write(DWC_AUD_CEC_IEN_SET,
				EE_CEC_IRQ_EN_MASK);
		else {
			hdmirx_cec_write(DWC_AUD_CEC_ICLR,
			(~(hdmirx_cec_read(DWC_AUD_CEC_IEN)) |
			EE_CEC_IRQ_EN_MASK));
			hdmirx_cec_write(DWC_AUD_CEC_IEN_SET,
			hdmirx_cec_read(DWC_AUD_CEC_IEN) &
			~EE_CEC_IRQ_EN_MASK);
			hdmirx_cec_write(DWC_AUD_CEC_IEN_CLR,
			(~(hdmirx_cec_read(DWC_AUD_CEC_IEN)) |
			EE_CEC_IRQ_EN_MASK));
		}
		CEC_INFO("cecb enable:int mask:0x%x\n",
		hdmirx_cec_read(DWC_AUD_CEC_IEN));
	} else {
		if (enable)
			writel(CECB_IRQ_EN_MASK,
			cec_dev->cec_reg + AO_CECB_INTR_MASKN);
		else
			writel(readl(cec_dev->cec_reg + AO_CECB_INTR_MASKN)
				& ~CECB_IRQ_EN_MASK,
				cec_dev->cec_reg + AO_CECB_INTR_MASKN);
		CEC_INFO("cecb enable:int mask:0x%x\n",
			 readl(cec_dev->cec_reg + AO_CECB_INTR_MASKN));
	}
}

void cec_irq_enable(bool enable)
{
	if (cec_dev->cec_num > 1) {
		eecec_irq_enable(enable);
		aocec_irq_enable(enable);
	} else {
		if (ee_cec == CEC_B)
			eecec_irq_enable(enable);
		else
			aocec_irq_enable(enable);
	}
}

/*
int cecrx_hw_init(void)
{
	unsigned int data32;

	if (ee_cec == CEC_A)
		return -1;

	cecb_hw_reset();

	ao_cecb_init();

	cec_logicaddr_set(cec_dev->cec_info.log_addr);
	return 0;
}
*/
static int dump_cecrx_reg(char *b)
{
	int i = 0, s = 0;
	unsigned char reg;
	unsigned int reg32;

	if (!cec_dev->plat_data->ee_to_ao) {
		reg32 = readl(cec_dev->hhi_reg + HHI_32K_CLK_CNTL);
		s += sprintf(b + s, "HHI_32K_CLK_CNTL:    0x%08x\n", reg32);
		reg32 = hdmirx_rd_top(TOP_EDID_ADDR_CEC);
		s += sprintf(b + s, "TOP_EDID_ADDR_CEC:   0x%08x\n", reg32);
		reg32 = hdmirx_rd_top(TOP_EDID_GEN_CNTL);
		s += sprintf(b + s, "TOP_EDID_GEN_CNTL:   0x%08x\n", reg32);
		reg32 = hdmirx_cec_read(DWC_AUD_CEC_IEN);
		s += sprintf(b + s, "DWC_AUD_CEC_IEN:     0x%08x\n", reg32);
		reg32 = hdmirx_cec_read(DWC_AUD_CEC_ISTS);
		s += sprintf(b + s, "DWC_AUD_CEC_ISTS:    0x%08x\n", reg32);
		reg32 = hdmirx_cec_read(DWC_DMI_DISABLE_IF);
		s += sprintf(b + s, "DWC_DMI_DISABLE_IF:  0x%08x\n", reg32);
		reg32 = hdmirx_rd_top(TOP_CLK_CNTL);
		s += sprintf(b + s, "TOP_CLK_CNTL:        0x%08x\n", reg32);
	} else {
		reg32 = readl(cec_dev->cec_reg + AO_CECB_CLK_CNTL_REG0);
		s += sprintf(b + s, "AO_CECB_CLK_CNTL_REG0:  0x%08x\n", reg32);
		reg32 = readl(cec_dev->cec_reg + AO_CECB_CLK_CNTL_REG1);
		s += sprintf(b + s, "AO_CECB_CLK_CNTL_REG1:  0x%08x\n", reg32);
		reg32 = readl(cec_dev->cec_reg + AO_CECB_GEN_CNTL);
		s += sprintf(b + s, "AO_CECB_GEN_CNTL:       0x%08x\n", reg32);
		reg32 = readl(cec_dev->cec_reg + AO_CECB_RW_REG);
		s += sprintf(b + s, "AO_CECB_RW_REG:         0x%08x\n", reg32);
		reg32 = readl(cec_dev->cec_reg + AO_CECB_INTR_MASKN);
		s += sprintf(b + s, "AO_CECB_INTR_MASKN:     0x%08x\n", reg32);
		reg32 = readl(cec_dev->cec_reg + AO_CECB_INTR_STAT);
		s += sprintf(b + s, "AO_CECB_INTR_STAT:      0x%08x\n", reg32);
	}

	s += sprintf(b + s, "CEC MODULE REGS:\n");
	s += sprintf(b + s, "CEC_CTRL     = 0x%02x\n", hdmirx_cec_read(0x1f00));
	if (cec_dev->plat_data->cecb_ver >= CECB_VER_2)
		s += sprintf(b + s, "CEC_CTRL2	  = 0x%02x\n",
			hdmirx_cec_read(0x1f04));
	s += sprintf(b + s, "CEC_MASK     = 0x%02x\n", hdmirx_cec_read(0x1f08));
	s += sprintf(b + s, "CEC_ADDR_L   = 0x%02x\n", hdmirx_cec_read(0x1f14));
	s += sprintf(b + s, "CEC_ADDR_H   = 0x%02x\n", hdmirx_cec_read(0x1f18));
	s += sprintf(b + s, "CEC_TX_CNT   = 0x%02x\n", hdmirx_cec_read(0x1f1c));
	s += sprintf(b + s, "CEC_RX_CNT   = 0x%02x\n", hdmirx_cec_read(0x1f20));
	if (cec_dev->plat_data->cecb_ver >= CECB_VER_2)
		s += sprintf(b + s, "CEC_STAT0   = 0x%02x\n",
			hdmirx_cec_read(0x1f24));
	s += sprintf(b + s, "CEC_LOCK     = 0x%02x\n", hdmirx_cec_read(0x1fc0));
	s += sprintf(b + s, "CEC_WKUPCTRL = 0x%02x\n", hdmirx_cec_read(0x1fc4));

	s += sprintf(b + s, "%s", "RX buffer:");
	for (i = 0; i < 16; i++) {
		reg = (hdmirx_cec_read(0x1f80 + i * 4) & 0xff);
		s += sprintf(b + s, " %02x", reg);
	}
	s += sprintf(b + s, "\n");

	s += sprintf(b + s, "%s", "TX buffer:");
	for (i = 0; i < 16; i++) {
		reg = (hdmirx_cec_read(0x1f40 + i * 4) & 0xff);
		s += sprintf(b + s, " %02x", reg);
	}
	s += sprintf(b + s, "\n");
	return s;
}

/*--------------------- END of EE CEC --------------------*/

void aocec_irq_enable(bool enable)
{
	if (enable)
		cec_set_reg_bits(AO_CEC_INTR_MASKN, 0x6, 0, 3);
	else
		cec_set_reg_bits(AO_CEC_INTR_MASKN, 0x0, 0, 3);
	CEC_INFO("ao enable:int mask:0x%x\n",
		 readl(cec_dev->cec_reg + AO_CEC_INTR_MASKN));
}

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

void cec_logicaddr_set(int l_add)
{
	/* save logical address for suspend/wake up */
	cec_set_reg_bits(AO_DEBUG_REG1, l_add, 16, 4);
	cec_dev->cec_info.addr_enable = (1 << l_add);
	if (ee_cec == CEC_B) {
		/* set ee_cec logical addr */
		if (l_add < 8)
			hdmirx_cec_write(DWC_CEC_ADDR_L, 1 << l_add);
		else
			hdmirx_cec_write(DWC_CEC_ADDR_H, 1 << (l_add - 8));

		CEC_INFO("set cecb logical addr:0x%x\n", l_add);
	} else {
		/*clear all logical address*/
		aocec_wr_reg(CEC_LOGICAL_ADDR0, 0);
		aocec_wr_reg(CEC_LOGICAL_ADDR1, 0);
		aocec_wr_reg(CEC_LOGICAL_ADDR2, 0);
		aocec_wr_reg(CEC_LOGICAL_ADDR3, 0);
		aocec_wr_reg(CEC_LOGICAL_ADDR4, 0);

		cec_hw_buf_clear();
		aocec_wr_reg(CEC_LOGICAL_ADDR0, (l_add & 0xf));
		udelay(100);
		aocec_wr_reg(CEC_LOGICAL_ADDR0, (0x1 << 4) | (l_add & 0xf));
		if (cec_msg_dbg_en)
			CEC_INFO("set cec alogical addr:0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR0));
	}
}

void ceca_addr_add(unsigned int l_add)
{
	unsigned int addr;
	unsigned int i;

	/* check if the logical addr is exist ? */
	for (i = CEC_LOGICAL_ADDR0; i <= CEC_LOGICAL_ADDR4; i++) {
		addr = aocec_rd_reg(i);
		if ((addr & 0x10) && ((addr & 0xf) == (l_add & 0xf))) {
			CEC_INFO("add 0x%x exist\n", l_add);
			return;
		}
	}

	/* find a empty place */
	for (i = CEC_LOGICAL_ADDR0; i <= CEC_LOGICAL_ADDR4; i++) {
		addr = aocec_rd_reg(i);
		if (addr & 0x10) {
			CEC_INFO(" skip 0x%x ,val=0x%x\n", i, addr);
			continue;
		} else {
			cec_hw_buf_clear();
			aocec_wr_reg(i, (l_add & 0xf));
			udelay(100);
			aocec_wr_reg(i, (l_add & 0xf)|0x10);
			CEC_INFO("cec a add addr %d at 0x%x\n",
				l_add, i);
			break;
		}
	}
}

void cecb_addr_add(unsigned int l_add)
{
	unsigned int addr;

	if (l_add < 8) {
		addr = hdmirx_cec_read(DWC_CEC_ADDR_L);
		addr |= (1 << l_add);
		hdmirx_cec_write(DWC_CEC_ADDR_L, addr);
	} else {
		addr = hdmirx_cec_read(DWC_CEC_ADDR_H);
		addr |= (1 << (l_add - 8));
		hdmirx_cec_write(DWC_CEC_ADDR_H, addr);
	}
	CEC_INFO("cec b add addr %d\n", l_add);
}

void cec_logicaddr_add(unsigned int cec_sel, unsigned int l_add)
{
	/* save logical address for suspend/wake up */
	cec_set_reg_bits(AO_DEBUG_REG1, l_add, 16, 4);

	if (cec_sel == CEC_B)
		cecb_addr_add(l_add);
	else
		ceca_addr_add(l_add);
}

void cec_logicaddr_remove(unsigned int cec_sel, unsigned int l_add)
{
	unsigned int addr;
	unsigned int i;

	if (cec_sel == CEC_B) {
		if (l_add < 8) {
			addr = hdmirx_cec_read(DWC_CEC_ADDR_L);
			addr &= ~(1 << l_add);
			hdmirx_cec_write(DWC_CEC_ADDR_L, addr);
		} else {
			addr = hdmirx_cec_read(DWC_CEC_ADDR_H);
			addr &= ~(1 << (l_add - 8));
			hdmirx_cec_write(DWC_CEC_ADDR_H, addr);
		}
		CEC_INFO("cec b remove addr %d\n", l_add);
	} else {
		for (i = CEC_LOGICAL_ADDR0; i <= CEC_LOGICAL_ADDR4; i++) {
			addr = aocec_rd_reg(i);
			if ((addr & 0xf) == (l_add & 0xf)) {
				aocec_wr_reg(i, (addr & 0xf));
				udelay(100);
				aocec_wr_reg(i, 0);
				cec_hw_buf_clear();
				CEC_INFO("cec a rm addr %d at 0x%x\n",
					l_add, i);
			}
		}
	}
}

void cec_restore_logical_addr(unsigned int cec_sel, unsigned int addr_en)
{
	unsigned int i;
	unsigned int addr_enable = addr_en;

	cec_clear_all_logical_addr(cec_sel);
	for (i = 0; i < 15; i++) {
		if (addr_enable & 0x1)
			cec_logicaddr_add(cec_sel, i);

		addr_enable = addr_enable >> 1;
	}
}

void ceca_hw_reset(void)
{
	writel(0x1, cec_dev->cec_reg + AO_CEC_GEN_CNTL);
	/* Enable gated clock (Normal mode). */
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 1, 1, 1);
	/* Release SW reset */
	udelay(100);
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 0, 0, 1);

	/* Enable all AO_CEC interrupt sources */
	/*cec_irq_enable(true);*/

	/* cec_logicaddr_set(cec_dev->cec_info.log_addr); */

	/* Cec arbitration 3/5/7 bit time set. */
	cec_arbit_bit_time_set(3, 0x118, 0);
	cec_arbit_bit_time_set(5, 0x000, 0);
	cec_arbit_bit_time_set(7, 0x2aa, 0);
}

void cec_hw_reset(unsigned int cec_sel)
{
	if (cec_sel == CEC_B) {
		ao_cecb_init();
		/* cec_logicaddr_set(cec_dev->cec_info.log_addr); */
	} else {
		ceca_hw_reset();
	}
	/* cec_logicaddr_set(cec_dev->cec_info.log_addr); */
	cec_restore_logical_addr(cec_sel, cec_dev->cec_info.addr_enable);
}

void cec_rx_buf_clear(void)
{
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 0x1);
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 0x0);
}

static inline bool is_poll_message(unsigned char header)
{
	unsigned char initiator, follower;

	initiator = (header >> 4) & 0xf;
	follower  = (header) & 0xf;
	return initiator == follower;
}

static inline bool is_feature_abort_msg(const unsigned char *msg, int len)
{
	if (!msg || len < 2)
		return false;
	if (msg[1] == CEC_OC_FEATURE_ABORT)
		return true;
	return false;
}

static inline bool is_report_phy_addr_msg(const unsigned char *msg, int len)
{
	if (!msg || len < 4)
		return false;
	if (msg[1] == CEC_OC_REPORT_PHYSICAL_ADDRESS)
		return true;
	return false;
}

static bool need_nack_repeat_msg(const unsigned char *msg, int len, int t)
{
	if (len == last_cec_msg->len &&
	    (is_poll_message(msg[0]) || is_feature_abort_msg(msg, len) ||
	      is_report_phy_addr_msg(msg, len)) &&
	    last_cec_msg->last_result == CEC_FAIL_NACK &&
	    jiffies - last_cec_msg->last_jiffies < t) {
		return true;
	}
	return false;
}

void cec_clear_all_logical_addr(unsigned int cec_sel)
{
	CEC_INFO("clear all logical addr\n");

	if (cec_sel == CEC_B) {
		hdmirx_cec_write(DWC_CEC_ADDR_L, 0);
		hdmirx_cec_write(DWC_CEC_ADDR_H, 0);
	} else {
		aocec_wr_reg(CEC_LOGICAL_ADDR0, 0);
		aocec_wr_reg(CEC_LOGICAL_ADDR1, 0);
		aocec_wr_reg(CEC_LOGICAL_ADDR2, 0);
		aocec_wr_reg(CEC_LOGICAL_ADDR3, 0);
		aocec_wr_reg(CEC_LOGICAL_ADDR4, 0);
	}
	/*udelay(100);*/
}

void cec_enable_arc_pin(bool enable)
{
	unsigned int data;

	if (cec_dev->plat_data->cecb_ver >= CECB_VER_2) {
		data = rd_reg_hhi(HHI_HDMIRX_ARC_CNTL);
		/* enable bit 1:1 bit 0: 0*/
		if (enable)
			data |= 0x02;
		else
			data &= 0xfffffffd;
		wr_reg_hhi(HHI_HDMIRX_ARC_CNTL, data);
		CEC_INFO("set arc en:%d, reg:%x\n", enable, data);
	} else {
		/* select arc according arg */
		if (enable)
			hdmirx_wr_top(TOP_ARCTX_CNTL, 0x01);
		else
			hdmirx_wr_top(TOP_ARCTX_CNTL, 0x00);
		CEC_INFO("set arc en:%d, reg:%lx\n",
			 enable, hdmirx_rd_top(TOP_ARCTX_CNTL));
	}
}
EXPORT_SYMBOL(cec_enable_arc_pin);

int cec_rx_buf_check(void)
{
	unsigned int rx_num_msg = 0;

	if (ee_cec == CEC_B) {
		cecrx_check_irq_enable();
		cecb_irq_handle();
	} else {
		rx_num_msg = aocec_rd_reg(CEC_RX_NUM_MSG);
		if (rx_num_msg)
			CEC_INFO("rx msg num:0x%02x\n", rx_num_msg);
	}
	return rx_num_msg;
}

int ceca_rx_irq_handle(unsigned char *msg, unsigned char *len)
{
	int i;
	int ret = -1;
	int pos;
	int rx_stat;

	rx_stat = aocec_rd_reg(CEC_RX_MSG_STATUS);
	if ((rx_stat != RX_DONE) || (aocec_rd_reg(CEC_RX_NUM_MSG) != 1)) {
		CEC_INFO("rx status:%x\n", rx_stat);
		writel((1 << 2), cec_dev->cec_reg + AO_CEC_INTR_CLR);
		aocec_wr_reg(CEC_RX_MSG_CMD,  RX_ACK_CURRENT);
		aocec_wr_reg(CEC_RX_MSG_CMD,  RX_NO_OP);
		cec_rx_buf_clear();
		return ret;
	}

	/* when use two cec ip, cec a only send msg, discard all rx msg */
	if (cec_dev->cec_num > 1) {
		writel((1 << 2), cec_dev->cec_reg + AO_CEC_INTR_CLR);
		aocec_wr_reg(CEC_RX_MSG_CMD,  RX_ACK_CURRENT);
		aocec_wr_reg(CEC_RX_MSG_CMD,  RX_NO_OP);
		cec_rx_buf_clear();
		CEC_INFO("discard msg\n");
		return ret;
	}

	*len = aocec_rd_reg(CEC_RX_MSG_LENGTH) + 1;

	for (i = 0; i < (*len) && i < MAX_MSG; i++)
		msg[i] = aocec_rd_reg(CEC_RX_MSG_0_HEADER + i);

	ret = rx_stat;

	/* ignore ping message */
	if (cec_msg_dbg_en && *len > 1) {
		pos = 0;
		pos += sprintf(msg_log_buf + pos,
			"cec: rx len: %d   dat: ", *len);
		for (i = 0; i < (*len); i++)
			pos += sprintf(msg_log_buf + pos, "%02x ", msg[i]);
		pos += sprintf(msg_log_buf + pos, "\n");
		msg_log_buf[pos] = '\0';
		CEC_INFO("%s", msg_log_buf);
	}
	last_cec_msg->len = 0;	/* invalid back up msg when rx */
	writel((1 << 2), cec_dev->cec_reg + AO_CEC_INTR_CLR);
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_ACK_CURRENT);
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_NO_OP);
	cec_rx_buf_clear();
	pin_status = 1;
	return ret;
}

/************************ cec arbitration cts code **************************/
/* using the cec pin as fiq gpi to assist the bus arbitration */

/* return value: 1: successful	  0: error */
static int ceca_trigle_tx(const unsigned char *msg, int len)
{
	int i;
	unsigned int n;
	int pos;
	int reg;
	unsigned int j = 40;
	unsigned int tx_stat;
	static int cec_timeout_cnt = 1;

	while (1) {
		tx_stat = aocec_rd_reg(CEC_TX_MSG_STATUS);
		if (tx_stat != TX_BUSY)
			break;

		if (!(j--)) {
			CEC_INFO("ceca waiting busy timeout\n");
			aocec_wr_reg(CEC_TX_MSG_CMD, TX_ABORT);
			cec_timeout_cnt++;
			if (cec_timeout_cnt > 0x08)
				cec_hw_reset(CEC_A);
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

		if (cec_msg_dbg_en) {
			pos = 0;
			pos += sprintf(msg_log_buf + pos,
				       "cec: tx len: %d dat: ", len);
			for (n = 0; n < len; n++) {
				pos += sprintf(msg_log_buf + pos,
					       "%02x ", msg[n]);
			}

			pos += sprintf(msg_log_buf + pos, "\n");

			msg_log_buf[pos] = '\0';
			pr_info("%s", msg_log_buf);
		}
		cec_timeout_cnt = 0;
		return 0;
	}
	CEC_ERR("error msg sts:0x%x\n", reg);
	return -1;
}

void ceca_tx_irq_handle(void)
{
	unsigned int tx_status = aocec_rd_reg(CEC_TX_MSG_STATUS);

	cec_tx_result = -1;
	switch (tx_status) {
	case TX_DONE:
		aocec_wr_reg(CEC_TX_MSG_CMD, TX_NO_OP);
		cec_tx_result = CEC_FAIL_NONE;
		break;

	case TX_BUSY:
		CEC_ERR("TX_BUSY\n");
		cec_tx_result = CEC_FAIL_BUSY;
		break;

	case TX_ERROR:
		if (cec_msg_dbg_en)
			CEC_ERR("TX ERROR!\n");
		aocec_wr_reg(CEC_TX_MSG_CMD, TX_ABORT);
		ceca_hw_reset();
		if (cec_dev->cec_num <= 1)
			cec_restore_logical_addr(CEC_A,
				cec_dev->cec_info.addr_enable);
		cec_tx_result = CEC_FAIL_NACK;
		break;

	case TX_IDLE:
		CEC_ERR("TX_IDLE\n");
		cec_tx_result = CEC_FAIL_OTHER;
		break;
	default:
		break;
	}
	writel((1 << 1), cec_dev->cec_reg + AO_CEC_INTR_CLR);
	complete(&cec_dev->tx_ok);
}

static int get_line(void)
{
	int reg, ret = -EINVAL;

	if (cec_dev->plat_data->line_reg == 1)
		reg = readl(cec_dev->periphs_reg + PREG_PAD_GPIO3_I);
	else
		reg = readl(cec_dev->cec_reg + AO_GPIO_I);
	ret = (reg & (1 << cec_dev->plat_data->line_bit));

	return ret;
}

static enum hrtimer_restart cec_line_check(struct hrtimer *timer)
{
	if (get_line() == 0)
		cec_line_cnt++;
	hrtimer_forward_now(timer, HR_DELAY(1));
	return HRTIMER_RESTART;
}

static int check_confilct(void)
{
	int i;

	for (i = 0; i < 200; i++) {
		/*
		 * sleep 20ms and using hrtimer to check cec line every 1ms
		 */
		cec_line_cnt = 0;
		hrtimer_start(&start_bit_check, HR_DELAY(1), HRTIMER_MODE_REL);
		msleep(20);
		hrtimer_cancel(&start_bit_check);
		if (cec_line_cnt == 0)
			break;
		CEC_INFO("line busy:%d\n", cec_line_cnt);
	}
	if (i >= 200)
		return -EBUSY;
	else
		return 0;
}

static bool check_physical_addr_valid(int timeout)
{
	while (timeout > 0) {
		if (cec_dev->dev_type == CEC_TV_ADDR)
			break;
		if (phy_addr_test)
			break;
		/* physical address for box */
		if (cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.valid == 0) {
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
	int t;
	int retry = 2;
	unsigned int cec_sel;

	/* only use cec a send msg */
	if (cec_dev->cec_num > 1)
		cec_sel = CEC_A;
	else
		cec_sel = ee_cec;

	t = msecs_to_jiffies((cec_sel == CEC_B) ? 2000 : 5000);

	if (len == 0)
		return CEC_FAIL_NONE;

	/*
	 * AO CEC controller will ack poll message itself if logical
	 *	address already set. Must clear it before poll again
	 */
	if (is_poll_message(msg[0]))
		cec_clear_all_logical_addr(cec_sel);

	/*
	 * for CEC CTS 9.3. Android will try 3 poll message if got NACK
	 * but AOCEC will retry 4 tx for each poll message. Framework
	 * repeat this poll message so quick makes 12 sequential poll
	 * waveform seen on CEC bus. And did not pass CTS
	 * specification of 9.3
	 */
	if ((cec_sel == CEC_A) && need_nack_repeat_msg(msg, len, t)) {
		if (!memcmp(msg, last_cec_msg->msg, len)) {
			CEC_INFO("NACK repeat message:%x\n", len);
			return CEC_FAIL_NACK;
		}
	}

	mutex_lock(&cec_dev->cec_mutex);
	/* make sure we got valid physical address */
	if (len >= 2 && msg[1] == CEC_OC_REPORT_PHYSICAL_ADDRESS)
		check_physical_addr_valid(3);

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

	if (cec_sel == CEC_B)
		ret = cecb_trigle_tx(msg, len);
	else
		ret = ceca_trigle_tx(msg, len);
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
	cec_tx_result = -1;
	ret = wait_for_completion_timeout(&cec_dev->tx_ok, t);
	if (ret <= 0) {
		/* timeout or interrupt */
		if (ret == 0) {
			CEC_ERR("tx timeout\n");
			cec_hw_reset(cec_sel);
		}
		ret = CEC_FAIL_OTHER;
	} else {
		ret = cec_tx_result;
	}
	if (ret != CEC_FAIL_NONE && ret != CEC_FAIL_NACK) {
		if (retry > 0) {
			retry--;
			msleep(100 + (prandom_u32() & 0x07) * 10);
			goto try_again;
		}
	}
	mutex_unlock(&cec_dev->cec_mutex);

	if (cec_sel == CEC_A) {
		last_cec_msg->last_result = ret;
		if (ret == CEC_FAIL_NACK) {
			memcpy(last_cec_msg->msg, msg, len);
			last_cec_msg->len = len;
			last_cec_msg->last_jiffies = jiffies;
		}
	}
	return ret;
}

/* -------------------------------------------------------------------------- */
/* AO CEC0 config */
/* -------------------------------------------------------------------------- */
static void ao_ceca_init(void)
{
	unsigned long data32;
	unsigned int reg;
	unsigned int chiptype;

	chiptype = get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR);
	/*CEC_INFO("chiptype=0x%x\n", chiptype);*/
	if (chiptype >= MESON_CPU_MAJOR_ID_GXBB) {
		if (cec_dev->plat_data->ee_to_ao) {
			reg = (0 << 31) |
				(0 << 30) |
				(1 << 28) | /* clk_div0/clk_div1 in turn */
				((732-1) << 12) |/* Div_tcnt1 */
				((733-1) << 0); /* Div_tcnt0 */
			writel(reg, cec_dev->cec_reg + AO_CEC_CLK_CNTL_REG0);
			reg = (0 << 13) |
				((11-1)  << 12) |
				((8-1)	<<	0);
			writel(reg, cec_dev->cec_reg + AO_CEC_CLK_CNTL_REG1);
			/*enable clk in*/
			reg = readl(cec_dev->cec_reg + AO_CEC_CLK_CNTL_REG0);
			reg |= (1 << 31);
			writel(reg, cec_dev->cec_reg + AO_CEC_CLK_CNTL_REG0);
			/*enable clk out*/
			udelay(200);
			reg |= (1 << 30);
			writel(reg, cec_dev->cec_reg + AO_CEC_CLK_CNTL_REG0);
		} else {
			reg =	(0 << 31) |
				(0 << 30) |
				(1 << 28) | /* clk_div0/clk_div1 in turn */
				((732-1) << 12) |/* Div_tcnt1 */
				((733-1) << 0); /* Div_tcnt0 */
			writel(reg, cec_dev->cec_reg + AO_RTC_ALT_CLK_CNTL0);
			reg =	(0 << 13) |
				((11-1)  << 12) |
				((8-1)	<<	0);
			writel(reg, cec_dev->cec_reg + AO_RTC_ALT_CLK_CNTL1);

			/*enable clk in*/
			reg = readl(cec_dev->cec_reg + AO_RTC_ALT_CLK_CNTL0);
			reg |= (1 << 31);
			writel(reg, cec_dev->cec_reg + AO_RTC_ALT_CLK_CNTL0);
			/*enable clk out*/
			udelay(200);
			reg |= (1 << 30);
			writel(reg, cec_dev->cec_reg + AO_RTC_ALT_CLK_CNTL0);
		}

		if (cec_dev->plat_data->ee_to_ao) {
			reg = readl(cec_dev->cec_reg + AO_RTI_PWR_CNTL_REG0);
			reg |=	(0x01 << 14);/* enable the crystal clock*/
			writel(reg, cec_dev->cec_reg + AO_RTI_PWR_CNTL_REG0);
		} else {
			reg = readl(cec_dev->cec_reg + AO_CRT_CLK_CNTL1);
			reg |= (0x800 << 16);/* select cts_rtc_oscin_clk */
			writel(reg, cec_dev->cec_reg + AO_CRT_CLK_CNTL1);

			reg = readl(cec_dev->cec_reg + AO_RTI_PWR_CNTL_REG0);
			reg &= ~(0x07 << 10);
			reg |=	(0x04 << 10);/* XTAL generate 32k */
			writel(reg, cec_dev->cec_reg + AO_RTI_PWR_CNTL_REG0);
		}
	}

	if (cec_dev->plat_data->ee_to_ao) {
		data32	= 0;
		data32 |= (7 << 12);	/* filter_del */
		data32 |= (1 <<  8);	/* filter_tick: 1us */
		data32 |= (1 <<  3);	/* enable system clock*/
		data32 |= 0 << 1;	/* [2:1]	cntl_clk: */
				/* 0=Disable clk (Power-off mode); */
				/* 1=Enable gated clock (Normal mode);*/
				/* 2=Enable free-run clk (Debug mode).*/
		data32 |= 1 << 0;	/* [0]	  sw_reset: 1=Reset*/
		writel(data32, cec_dev->cec_reg + AO_CEC_GEN_CNTL);
	} else {
		data32	= 0;
		data32 |= 0 << 1;	/* [2:1]	cntl_clk:*/
				/* 0=Disable clk (Power-off mode);*/
				/* 1=Enable gated clock (Normal mode);*/
				/* 2=Enable free-run clk (Debug mode).*/
		data32 |= 1 << 0;	/* [0]	  sw_reset: 1=Reset */
		writel(data32, cec_dev->cec_reg + AO_CEC_GEN_CNTL);
	}
	/* Enable gated clock (Normal mode). */
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 1, 1, 1);
	/* Release SW reset */
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 0, 0, 1);

	/* Enable all AO_CEC interrupt sources */
	/*cec_irq_enable(true);*/

	cec_arbit_bit_time_set(3, 0x118, 0);
	cec_arbit_bit_time_set(5, 0x000, 0);
	cec_arbit_bit_time_set(7, 0x2aa, 0);
}

void cec_arbit_bit_time_set(unsigned int bit_set,
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

static unsigned int ao_cec_intr_stat(void)
{
	return readl(cec_dev->cec_reg + AO_CEC_INTR_STAT);
}

unsigned int cec_intr_stat(void)
{
	return ao_cec_intr_stat();
}

/*
 *wr_flag: 1 write; value valid
 *		 0 read;  value invalid
 */
unsigned int cec_config(unsigned int value, bool wr_flag)
{
	if (wr_flag)
		cec_set_reg_bits(AO_DEBUG_REG0, value, 0, 8);

	return readl(cec_dev->cec_reg + AO_DEBUG_REG0) & 0xff;
}

/*
 *wr_flag:1 write; value valid
 *		0 read;  value invalid
 */
unsigned int cec_phyaddr_config(unsigned int value, bool wr_flag)
{
	if (wr_flag)
		cec_set_reg_bits(AO_DEBUG_REG1, value, 0, 16);

	return readl(cec_dev->cec_reg + AO_DEBUG_REG1);
}

/*
void cec_keep_reset(void)
{
	if (ee_cec == CEC_B)
		cecb_hw_reset();
	else
		writel(0x1, cec_dev->cec_reg + AO_CEC_GEN_CNTL);
}
*/

/*
 * cec hw module init before allocate logical address
 */
static void cec_pre_init(void)
{
	unsigned int reg = readl(cec_dev->cec_reg + AO_RTI_STATUS_REG1);

	reg &= 0xfffff;
	if ((reg & 0xffff) == 0xffff)
		wake_ok = 0;
	pr_info("cec: wake up flag:%x\n", reg);

	if (cec_dev->cec_num > 1) {
		ao_ceca_init();
		ao_cecb_init();
	} else {
		if (ee_cec == CEC_B)
			ao_cecb_init();
		else
			ao_ceca_init();
	}

	//need restore all logical address
	if (cec_dev->cec_num > 1)
		cec_restore_logical_addr(CEC_B, cec_dev->cec_info.addr_enable);
	else
		cec_restore_logical_addr(ee_cec, cec_dev->cec_info.addr_enable);
}

static int cec_late_check_rx_buffer(void)
{
	int ret;
	/*struct delayed_work *dwork = &cec_dev->cec_work;*/

	ret = cec_rx_buf_check();
	if (!ret)
		return 0;
	/*
	 * start another check if rx buffer is full
	 */
	if ((-1) == ceca_rx_irq_handle(rx_msg, &rx_len)) {
		CEC_INFO("buffer got unrecorgnized msg\n");
		cec_rx_buf_clear();
		return 0;
	}
	return 1;
}

void cec_key_report(int suspend)
{
	input_event(cec_dev->cec_info.remote_cec_dev, EV_KEY, KEY_POWER, 1);
	input_sync(cec_dev->cec_info.remote_cec_dev);
	input_event(cec_dev->cec_info.remote_cec_dev, EV_KEY, KEY_POWER, 0);
	input_sync(cec_dev->cec_info.remote_cec_dev);
	if (!suspend)
		CEC_INFO("== WAKE UP BY CEC ==\n")
	else
		CEC_INFO("== SLEEP by CEC==\n")
}

void cec_give_version(unsigned int dest)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char msg[3];

	if (dest != 0xf) {
		msg[0] = ((index & 0xf) << 4) | dest;
		msg[1] = CEC_OC_CEC_VERSION;
		msg[2] = cec_dev->cec_info.cec_version;
		cec_ll_tx(msg, 3);
	}
}

void cec_report_physical_address_smp(void)
{
	unsigned char msg[5];
	unsigned char index = cec_dev->cec_info.log_addr;
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

void cec_device_vendor_id(void)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char msg[5];
	unsigned int vendor_id;

	vendor_id = cec_dev->v_data.vendor_id;
	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_DEVICE_VENDOR_ID;
	msg[2] = (vendor_id >> 16) & 0xff;
	msg[3] = (vendor_id >> 8) & 0xff;
	msg[4] = (vendor_id >> 0) & 0xff;

	cec_ll_tx(msg, 5);
}

void cec_give_deck_status(unsigned int dest)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char msg[3];

	msg[0] = ((index & 0xf) << 4) | dest;
	msg[1] = CEC_OC_DECK_STATUS;
	msg[2] = 0x1a;
	cec_ll_tx(msg, 3);
}

void cec_menu_status_smp(int dest, int status)
{
	unsigned char msg[3];
	unsigned char index = cec_dev->cec_info.log_addr;

	msg[0] = ((index & 0xf) << 4) | dest;
	msg[1] = CEC_OC_MENU_STATUS;
	if (status == DEVICE_MENU_ACTIVE)
		msg[2] = DEVICE_MENU_ACTIVE;
	else
		msg[2] = DEVICE_MENU_INACTIVE;
	cec_ll_tx(msg, 3);
}

void cec_inactive_source(int dest)
{
	unsigned char index = cec_dev->cec_info.log_addr;
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

void cec_set_osd_name(int dest)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char osd_len = strlen(cec_dev->cec_info.osd_name);
	unsigned char msg[16];

	if (dest != 0xf) {
		msg[0] = ((index & 0xf) << 4) | dest;
		msg[1] = CEC_OC_SET_OSD_NAME;
		memcpy(&msg[2], cec_dev->cec_info.osd_name, osd_len);

		cec_ll_tx(msg, 2 + osd_len);
	}
}

void cec_active_source_smp(void)
{
	unsigned char msg[4];
	unsigned char index = cec_dev->cec_info.log_addr;
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

void cec_request_active_source(void)
{
	unsigned char msg[2];
	unsigned char index = cec_dev->cec_info.log_addr;

	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_REQUEST_ACTIVE_SOURCE;
	cec_ll_tx(msg, 2);
}

void cec_set_stream_path(unsigned char *msg)
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

void cec_report_power_status(int dest, int status)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char msg[3];

	msg[0] = ((index & 0xf) << 4) | dest;
	msg[1] = CEC_OC_REPORT_POWER_STATUS;
	msg[2] = status;
	cec_ll_tx(msg, 3);
}

static void cec_rx_process(void)
{
	int len = rx_len;
	int initiator, follower;
	int opcode;
	unsigned char msg[MAX_MSG] = {};
	int dest_phy_addr;

	if (len < 2 || !new_msg)		/* ignore ping message */
		return;

	memcpy(msg, rx_msg, len);
	initiator = ((msg[0] >> 4) & 0xf);
	follower  = msg[0] & 0xf;
	if (follower != 0xf && follower != cec_dev->cec_info.log_addr) {
		CEC_ERR("wrong rx message of bad follower:%x", follower);
		return;
	}
	opcode = msg[1];
	switch (opcode) {
	case CEC_OC_ACTIVE_SOURCE:
		if (wake_ok == 0) {
			int phy_addr = msg[2] << 8 | msg[3];

			if (phy_addr == 0xffff)
				break;
			wake_ok = 1;
			phy_addr |= (initiator << 16);
			writel(phy_addr, cec_dev->cec_reg + AO_RTI_STATUS_REG1);
			CEC_INFO("found wake up source:%x", phy_addr);
		}
		break;

	case CEC_OC_ROUTING_CHANGE:
		dest_phy_addr = msg[4] << 8 | msg[5];
		if ((dest_phy_addr == cec_dev->phy_addr) &&
			(cec_dev->cec_suspend == CEC_EARLY_SUSPEND)) {
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
		if (cec_dev->cec_suspend != CEC_POWER_ON)
			cec_key_report(0);
		break;

	case CEC_OC_REQUEST_ACTIVE_SOURCE:
		if (cec_dev->cec_suspend == CEC_POWER_ON)
			cec_active_source_smp();
		break;

	case CEC_OC_GIVE_DEVICE_POWER_STATUS:
		if (cec_dev->cec_suspend == CEC_DEEP_SUSPEND)
			cec_report_power_status(initiator, POWER_STANDBY);
		else if (cec_dev->cec_suspend == CEC_EARLY_SUSPEND)
			cec_report_power_status(initiator, TRANS_ON_TO_STANDBY);
		else if (cec_dev->cec_suspend == CEC_POWER_RESUME)
			cec_report_power_status(initiator, TRANS_STANDBY_TO_ON);
		else
			cec_report_power_status(initiator, POWER_ON);
		break;

	case CEC_OC_USER_CONTROL_PRESSED:
		/* wake up by key function */
		if (cec_dev->cec_suspend != CEC_POWER_ON) {
			if (msg[2] == 0x40 || msg[2] == 0x6d)
				cec_key_report(0);
		}
		break;

	case CEC_OC_MENU_REQUEST:
		if (cec_dev->cec_suspend != CEC_POWER_ON)
			cec_menu_status_smp(initiator, DEVICE_MENU_INACTIVE);
		else
			cec_menu_status_smp(initiator, DEVICE_MENU_ACTIVE);
		break;

	case CEC_OC_IMAGE_VIEW_ON:
	case CEC_OC_TEXT_VIEW_ON:
		/* request active source needed */
		dest_phy_addr = 0xffff;
		dest_phy_addr =	(dest_phy_addr << 0) | (initiator << 16);
		writel(dest_phy_addr, cec_dev->cec_reg + AO_RTI_STATUS_REG1);
		CEC_INFO("weak up by otp\n");
		cec_key_report(0);
		break;

	default:
		CEC_ERR("unsupported command:%x\n", opcode);
		CEC_ERR("wake_ok=%d,hal_flag=0x%x\n",
			wake_ok, cec_dev->hal_flag);
		break;
	}
	new_msg = 0;
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
	struct delayed_work *dwork = &cec_dev->cec_work;
	unsigned int cec_cfg;

	cec_cfg = cec_config(0, 0);
	if (cec_cfg & CEC_FUNC_CFG_CEC_ON) {
		/*cec module on*/
		if (cec_dev && (!wake_ok || cec_service_suspended()))
			cec_rx_process();

		/*for check rx buffer for old chip version, cec rx irq process*/
		/*in internal hdmi rx, for avoid msg lose*/
		if ((cec_dev->cpu_type < MESON_CPU_MAJOR_ID_TXLX) &&
			(cec_cfg == CEC_FUNC_CFG_ALL)) {
			if (cec_late_check_rx_buffer()) {
				/*msg in*/
				mod_delayed_work(cec_dev->cec_thread, dwork, 0);
				return;
			}
		}
	}
	/*triger next process*/
	queue_delayed_work(cec_dev->cec_thread, dwork, CEC_FRAME_DELAY);
}

static irqreturn_t ceca_isr(int irq, void *dev_instance)
{
	unsigned int intr_stat = 0;
	struct delayed_work *dwork;

	/*CEC_INFO("ceca_isr\n");*/
	dwork = &cec_dev->cec_work;
	intr_stat = cec_intr_stat();
	if (intr_stat & (1<<1)) {   /* aocec tx intr */
		ceca_tx_irq_handle();
		return IRQ_HANDLED;
	}
	if ((-1) == ceca_rx_irq_handle(rx_msg, &rx_len))
		return IRQ_HANDLED;

	complete(&cec_dev->rx_ok);
	/* check rx buffer is full */
	new_msg = 1;
	mod_delayed_work(cec_dev->cec_thread, dwork, 0);
	return IRQ_HANDLED;
}
/*
static void check_wake_up(void)
{
	if (wake_ok == 0)
		cec_request_active_source();
}
*/
/******************** cec class interface *************************/
static ssize_t device_type_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%ld\n", cec_dev->dev_type);
}

static ssize_t device_type_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int type;

	if (kstrtouint(buf, 10, &type) != 0)
		return -EINVAL;

	cec_dev->dev_type = type;
	CEC_ERR("set dev_type to %d\n", type);
	return count;
}

static ssize_t menu_language_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	char a, b, c;

	a = ((cec_dev->cec_info.menu_lang >> 16) & 0xff);
	b = ((cec_dev->cec_info.menu_lang >>  8) & 0xff);
	c = ((cec_dev->cec_info.menu_lang >>  0) & 0xff);
	return sprintf(buf, "%c%c%c\n", a, b, c);
}

static ssize_t menu_language_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	char a, b, c;

	if (sscanf(buf, "%c%c%c", &a, &b, &c) != 3)
		return -EINVAL;

	cec_dev->cec_info.menu_lang = (a << 16) | (b << 8) | c;
	CEC_ERR("set menu_language to %s\n", buf);
	return count;
}

static ssize_t vendor_id_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", cec_dev->cec_info.vendor_id);
}

static ssize_t vendor_id_store(struct class *cla, struct class_attribute *attr,
	const char *buf, size_t count)
{
	unsigned int id;

	if (kstrtouint(buf, 16, &id) != 0)
		return -EINVAL;
	cec_dev->cec_info.vendor_id = id;
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

static const char * const ceca_reg_name3[] = {
	"STAT_0_0",
	"STAT_0_1",
	"STAT_0_2",
	"STAT_0_3",
	"STAT_1_0",
	"STAT_1_1",
	"STAT_1_2"
};


static ssize_t dump_reg_show(struct class *cla,
	struct class_attribute *attr, char *b)
{
	int i, s = 0;

	if (ee_cec == CEC_B)
		return dump_cecrx_reg(b);

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

	if (cec_dev->plat_data->ceca_sts_reg) {
		for (i = 0; i < ARRAY_SIZE(ceca_reg_name3); i++) {
			s += sprintf(b + s, "%s:%2x\n",
			 ceca_reg_name3[i], aocec_rd_reg(i + 0xA0));
		}
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
	return sprintf(buf, "%s\n", cec_dev->cec_info.osd_name);
}

static ssize_t port_seq_store(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	unsigned int seq;

	if (kstrtouint(buf, 16, &seq) != 0)
		return -EINVAL;

	CEC_ERR("port_seq:%x\n", seq);
	cec_dev->port_seq = seq;
	return count;
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

	tx_hpd = cec_dev->tx_dev->hpd_state;
	if (cec_dev->dev_type != CEC_TV_ADDR) {
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
	unsigned int tx_hpd;
	char p;

	tx_hpd = cec_dev->tx_dev->hpd_state;
	if (cec_dev->dev_type != CEC_TV_ADDR) {
		if (!tx_hpd) {
			pin_status = 0;
			return sprintf(buf, "%s\n", "disconnected");
		}
		if (pin_status == 0) {
			p = (cec_dev->cec_info.log_addr << 4) | CEC_TV_ADDR;
			if (cec_ll_tx(&p, 1) == CEC_FAIL_NONE)
				return sprintf(buf, "%s\n", "ok");
			else
				return sprintf(buf, "%s\n", "fail");
		} else
			return sprintf(buf, "%s\n", "ok");
	} else {
		return sprintf(buf, "%s\n", pin_status ? "ok" : "fail");
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

	if (kstrtouint(buf, 16, &addr) != 0)
		return -EINVAL;

	if (addr > 0xffff || addr < 0) {
		CEC_ERR("invalid input:%s\n", buf);
		phy_addr_test = 0;
		return -EINVAL;
	}
	cec_dev->phy_addr = addr;
	phy_addr_test = 1;
	return count;
}

static ssize_t dbg_en_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", cec_msg_dbg_en);
}

static ssize_t dbg_en_store(struct class *cla, struct class_attribute *attr,
	const char *buf, size_t count)
{
	int en;

	if (kstrtouint(buf, 16, &en) != 0)
		return -EINVAL;

	cec_msg_dbg_en = en;
	return count;
}

static ssize_t cmd_store(struct class *cla, struct class_attribute *attr,
	const char *bu, size_t count)
{
	char buf[20] = {};
	int tmpbuf[20] = {};
	int i;
	int cnt;

	cnt = sscanf(bu, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
		    &tmpbuf[0], &tmpbuf[1], &tmpbuf[2], &tmpbuf[3],
		    &tmpbuf[4], &tmpbuf[5], &tmpbuf[6], &tmpbuf[7],
		    &tmpbuf[8], &tmpbuf[9], &tmpbuf[10], &tmpbuf[11],
		    &tmpbuf[12], &tmpbuf[13], &tmpbuf[14], &tmpbuf[15]);
	if (cnt < 0)
		return -EINVAL;
	if (cnt > 16)
		cnt = 16;

	for (i = 0; i < cnt; i++)
		buf[i] = (char)tmpbuf[i];

	/*CEC_ERR("cnt=%d\n", cnt);*/
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

	cnt = kstrtouint(bu, 16, &val);
	if (cnt < 0 || val > 0xff)
		return -EINVAL;
	cec_config(val, 1);
	if (val == 0)
		cec_clear_all_logical_addr(ee_cec);/*cec_keep_reset();*/
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

static ssize_t cec_version_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	CEC_INFO("driver date:%s\n", CEC_DRIVER_VERSION);
	return sprintf(buf, "%d\n", cec_dev->cec_info.cec_version);
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
	cec_dev->cec_info.log_addr = val;
	cec_dev->cec_info.power_status = POWER_ON;

	return count;
}

static ssize_t log_addr_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", cec_dev->cec_info.log_addr);
}

static ssize_t dbg_store(struct class *cla, struct class_attribute *attr,
	const char *bu, size_t count)
{
	const char *delim = " ";
	char *token;
	char *cur = (char *)bu;
	struct dbgflg *dbg = &stdbgflg;
	unsigned int addr, val;

	token = strsep(&cur, delim);
	if (token && strncmp(token, "bypass", 6) == 0) {
		/*get the second param*/
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		dbg->hal_cmd_bypass = val ? 1 : 0;
		CEC_ERR("cmdbypass:%d\n", val);
	} else if (token && strncmp(token, "dbgen", 5) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		cec_msg_dbg_en = val;
		CEC_ERR("msg_dbg_en:%d\n", val);
	} else if (token && strncmp(token, "ra", 2) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		CEC_ERR("rd ceca reg:0x%x val:0x%x\n", addr,
				aocec_rd_reg(addr));
	} else if (token && strncmp(token, "wa", 2) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		CEC_ERR("wa ceca reg:0x%x val:0x%x\n", addr, val);
		aocec_wr_reg(addr, val);
	} else if (token && strncmp(token, "rb", 2) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		CEC_ERR("rd cecb reg:0x%x val:0x%x\n", addr,
				hdmirx_cec_read(addr));
	} else if (token && strncmp(token, "wb", 2) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		CEC_ERR("wb cecb reg:0x%x val:0x%x\n", addr, val);
		hdmirx_cec_write(addr, val);
	} else if (token && strncmp(token, "dump", 4) == 0) {
		dump_reg();
	} else if (token && strncmp(token, "status", 6) == 0) {
		cec_dump_info();
	} else if (token && strncmp(token, "rao", 3) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		val = readl(cec_dev->cec_reg + addr);
		CEC_ERR("rao addr:0x%x, val:0x%x", val, addr);
	} else if (token && strncmp(token, "wao", 3) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		writel(val, cec_dev->cec_reg + addr);
		CEC_ERR("wao addr:0x%x, val:0x%x", val, addr);
	} else if (token && strncmp(token, "preinit", 7) == 0) {
		cec_pre_init();
	} else if (token && strncmp(token, "setaddr", 7) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		cec_logicaddr_set(addr);
	} else if (token && strncmp(token, "clraddr", 7) == 0) {
		cec_dev->cec_info.addr_enable = 0;
		cec_clear_all_logical_addr(ee_cec);
	} else if (token && strncmp(token, "clralladdr", 10) == 0) {
		cec_dev->cec_info.addr_enable = 0;
		cec_clear_all_logical_addr(0);
		cec_clear_all_logical_addr(1);
	} else if (token && strncmp(token, "addaddr", 7) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;
		cec_dev->cec_info.addr_enable |= (1 << (addr & 0xf));
		if (cec_dev->cec_num > 1)
			cec_logicaddr_add(CEC_B, addr);
		else
			cec_logicaddr_add(ee_cec, addr);
	} else if (token && strncmp(token, "rmaddr", 6) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;
		cec_dev->cec_info.addr_enable &= ~(1 << (addr & 0xf));
		if (cec_dev->cec_num > 1)
			cec_logicaddr_remove(CEC_B, addr);
		else
			cec_logicaddr_remove(ee_cec, addr);
	} else {
		if (token)
			CEC_ERR("no cmd:%s\n", token);
	}

	return count;
}

static ssize_t dbg_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	CEC_INFO("dbg_show\n");
	return 0;
}


static struct class_attribute aocec_class_attr[] = {
	__ATTR_WO(cmd),
	__ATTR_RO(port_num),
	__ATTR_RO(osd_name),
	__ATTR_RO(dump_reg),
	__ATTR_RO(port_status),
	__ATTR_RO(pin_status),
	__ATTR_RO(cec_version),
	__ATTR_RO(arc_port),
	__ATTR_RO(wake_up),
	__ATTR(port_seq, 0664, port_seq_show, port_seq_store),
	__ATTR(physical_addr, 0664, physical_addr_show, physical_addr_store),
	__ATTR(vendor_id, 0664, vendor_id_show, vendor_id_store),
	__ATTR(menu_language, 0664, menu_language_show, menu_language_store),
	__ATTR(device_type, 0664, device_type_show, device_type_store),
	__ATTR(dbg_en, 0664, dbg_en_show, dbg_en_store),
	__ATTR(log_addr, 0664, log_addr_show, log_addr_store),
	__ATTR(fun_cfg, 0664, fun_cfg_show, fun_cfg_store),
	__ATTR(dbg, 0664, dbg_show, dbg_store),
	__ATTR_NULL
};

/******************** cec hal interface ***************************/
static int hdmitx_cec_open(struct inode *inode, struct file *file)
{
	if (atomic_add_return(1, &cec_dev->cec_info.open_count)) {
		cec_dev->cec_info.hal_ctl = 1;
		/* set default logical addr flag for uboot */
		cec_set_reg_bits(AO_DEBUG_REG1, 0xf, 16, 4);
	}
	return 0;
}

static int hdmitx_cec_release(struct inode *inode, struct file *file)
{
	if (!atomic_sub_return(1, &cec_dev->cec_info.open_count))
		cec_dev->cec_info.hal_ctl = 0;
	return 0;
}

static ssize_t hdmitx_cec_read(struct file *f, char __user *buf,
			   size_t size, loff_t *p)
{
	int ret;

	if ((cec_dev->hal_flag & (1 << HDMI_OPTION_SYSTEM_CEC_CONTROL)))
		rx_len = 0;
	/*CEC_ERR("read msg start\n");*/
	ret = wait_for_completion_timeout(&cec_dev->rx_ok, CEC_FRAME_DELAY);
	if (ret <= 0) {
		/*CEC_ERR("read msg ret=0\n");*/
		return ret;
	}

	if (rx_len == 0) {
		/*CEC_ERR("read msg rx_len=0\n");*/
		return 0;
	}

	/*CEC_ERR("read msg end\n");*/
	if (copy_to_user(buf, rx_msg, rx_len))
		return -EINVAL;
	return rx_len;
}

static ssize_t hdmitx_cec_write(struct file *f, const char __user *buf,
			    size_t size, loff_t *p)
{
	unsigned char tempbuf[16] = {};
	int ret = CEC_FAIL_OTHER;
	unsigned int cec_cfg;

	if (stdbgflg.hal_cmd_bypass)
		return -EINVAL;

	if (size > 16)
		size = 16;
	if (size <= 0)
		return -EINVAL;

	if (copy_from_user(tempbuf, buf, size))
		return -EINVAL;

	cec_cfg = cec_config(0, 0);
	if (cec_cfg & CEC_FUNC_CFG_CEC_ON) {
		/*cec module on*/
		ret = cec_ll_tx(tempbuf, size);
	} else {
		CEC_ERR("err:cec module disabled\n");
	}

	return ret;
}

static void init_cec_port_info(struct hdmi_port_info *port,
			       struct ao_cec_dev *cec_dev)
{
	unsigned int a, b, c = 0, d, e = 0;
	unsigned int phy_head = 0xf000, phy_app = 0x1000, phy_addr;
	struct hdmitx_dev *tx_dev;

	/* physical address for TV or repeator */
	tx_dev = cec_dev->tx_dev;
	if (tx_dev == NULL || cec_dev->dev_type == CEC_TV_ADDR) {
		phy_addr = 0;
	} else if (tx_dev->hdmi_info.vsdb_phy_addr.valid == 1) {
		/* get phy address from tx module */
		a = tx_dev->hdmi_info.vsdb_phy_addr.a;
		b = tx_dev->hdmi_info.vsdb_phy_addr.b;
		c = tx_dev->hdmi_info.vsdb_phy_addr.c;
		d = tx_dev->hdmi_info.vsdb_phy_addr.d;
		phy_addr = ((a << 12) | (b << 8) | (c << 4) | (d));
	} else
		phy_addr = 0;

	/* found physical address append for repeator */
	for (a = 0; a < 4; a++) {
		if (phy_addr & phy_head) {
			phy_head >>= 4;
			phy_app  >>= 4;
		} else
			break;
	}

	CEC_ERR("%s phy_addr:%x, port num:%x\n", __func__, phy_addr,
				cec_dev->port_num);
	CEC_ERR("port_seq=0x%x\n", cec_dev->port_seq);
	/* init for port info */
	for (a = 0; a < sizeof(cec_dev->port_seq) * 2; a++) {
		/* set port physical address according port sequence */
		if (cec_dev->port_seq) {
			c = (cec_dev->port_seq >> (4 * a)) & 0xf;
			if (c == 0xf) { /* not used */
				CEC_INFO("port %d is not used\n", a);
				continue;
			}
			port[e].physical_address = (c) * phy_app + phy_addr;
		} else {
			/* asending order if port_seq is not set */
			port[e].physical_address = (a + 1) * phy_app + phy_addr;
		}

		/* select input / output port*/
		if ((e + cec_dev->output) == cec_dev->port_num) {
			port[e].physical_address = phy_addr;
			port[e].port_id = 0;
			port[e].type = HDMI_OUTPUT;
		} else {
			port[e].type = HDMI_INPUT;
			port[e].port_id = c;/*a + 1; phy port - ui id*/
		}
		port[e].cec_supported = 1;
		/* set ARC feature according mask */
		if (cec_dev->arc_port & (1 << e))
			port[e].arc_supported = 1;
		else
			port[e].arc_supported = 0;
		CEC_ERR("portinfo id:%d arc:%d phy:%x,type:%d\n",
				port[e].port_id, port[e].arc_supported,
				port[e].physical_address,
				port[e].type);
		e++;
		if (e >= cec_dev->port_num)
			break;
	}
}


void cec_dump_info(void)
{
	struct hdmi_port_info *port;

	CEC_ERR("driver date:%s\n", CEC_DRIVER_VERSION);
	CEC_ERR("cec sel:%d\n", ee_cec);
	CEC_ERR("cec_num:%d\n", cec_dev->cec_num);
	CEC_ERR("dev_type:%d\n", (unsigned int)cec_dev->dev_type);
	CEC_ERR("wk_logic_addr:0x%x\n", cec_dev->wakup_data.wk_logic_addr);
	CEC_ERR("wk_phy_addr:0x%x\n", cec_dev->wakup_data.wk_phy_addr);
	CEC_ERR("wk_port_id:0x%x\n", cec_dev->wakup_data.wk_port_id);
	CEC_ERR("wakeup_reason:0x%x\n", cec_dev->wakeup_reason);
	CEC_ERR("phy_addr:0x%x\n", cec_dev->phy_addr);
	CEC_ERR("cec_version:0x%x\n", cec_dev->cec_info.cec_version);
	CEC_ERR("hal_ctl:0x%x\n", cec_dev->cec_info.hal_ctl);
	CEC_ERR("menu_lang:0x%x\n", cec_dev->cec_info.menu_lang);
	CEC_ERR("menu_status:0x%x\n", cec_dev->cec_info.menu_status);
	CEC_ERR("open_count:%d\n", cec_dev->cec_info.open_count.counter);
	CEC_ERR("vendor_id:0x%x\n", cec_dev->v_data.vendor_id);
	CEC_ERR("port_num:0x%x\n", cec_dev->port_num);
	CEC_ERR("output:0x%x\n", cec_dev->output);
	CEC_ERR("arc_port:0x%x\n", cec_dev->arc_port);
	CEC_ERR("hal_flag:0x%x\n", cec_dev->hal_flag);
	CEC_ERR("hpd_state:0x%x\n", cec_dev->tx_dev->hpd_state);
	CEC_ERR("cec_config:0x%x\n", cec_config(0, 0));
	CEC_ERR("log_addr:0x%x\n", cec_dev->cec_info.log_addr);
	port = kcalloc(cec_dev->port_num, sizeof(*port), GFP_KERNEL);
	if (port) {
		init_cec_port_info(port, cec_dev);
		kfree(port);
	}

	if (cec_dev->cec_num > 1) {
		CEC_ERR("addrL 0x%x\n", hdmirx_cec_read(DWC_CEC_ADDR_L));
		CEC_ERR("addrH 0x%x\n", hdmirx_cec_read(DWC_CEC_ADDR_H));

		CEC_ERR("addr0 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR0));
		CEC_ERR("addr1 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR1));
		CEC_ERR("addr2 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR2));
		CEC_ERR("addr3 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR3));
		CEC_ERR("addr4 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR4));
	} else {
		if (ee_cec == CEC_B) {
			CEC_ERR("addrL 0x%x\n",
				hdmirx_cec_read(DWC_CEC_ADDR_L));
			CEC_ERR("addrH 0x%x\n",
				hdmirx_cec_read(DWC_CEC_ADDR_H));
		} else {
			CEC_ERR("addr0 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR0));
			CEC_ERR("addr1 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR1));
			CEC_ERR("addr2 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR2));
			CEC_ERR("addr3 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR3));
			CEC_ERR("addr4 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR4));
		}
	}
	CEC_ERR("addr_enable:0x%x\n", cec_dev->cec_info.addr_enable);
}

static long hdmitx_cec_ioctl(struct file *f,
			     unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	unsigned int tmp;
	struct hdmi_port_info *port;
	unsigned int a, b, c, d;
	struct hdmitx_dev *tx_dev;
	/*unsigned int tx_hpd;*/

	mutex_lock(&cec_dev->cec_ioctl_mutex);
	switch (cmd) {
	case CEC_IOC_GET_PHYSICAL_ADDR:
		check_physical_addr_valid(20);
		/* physical address for TV or repeator */
		tx_dev = cec_dev->tx_dev;
		if (!tx_dev || cec_dev->dev_type == CEC_TV_ADDR) {
			tmp = 0;
		} else if (tx_dev->hdmi_info.vsdb_phy_addr.valid == 1) {
			/*hpd attach and wait read edid*/
			a = tx_dev->hdmi_info.vsdb_phy_addr.a;
			b = tx_dev->hdmi_info.vsdb_phy_addr.b;
			c = tx_dev->hdmi_info.vsdb_phy_addr.c;
			d = tx_dev->hdmi_info.vsdb_phy_addr.d;
			tmp = ((a << 12) | (b << 8) | (c << 4) | (d));
		} else
			tmp = 0;

		if (!phy_addr_test) {
			cec_dev->phy_addr = tmp;
			cec_phyaddr_config(tmp, 1);
		} else
			tmp = cec_dev->phy_addr;

		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	case CEC_IOC_GET_VERSION:
		tmp = cec_dev->cec_info.cec_version;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	case CEC_IOC_GET_VENDOR_ID:
		tmp = cec_dev->v_data.vendor_id;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	case CEC_IOC_GET_PORT_NUM:
		tmp = cec_dev->port_num;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	case CEC_IOC_GET_PORT_INFO:
		port = kcalloc(cec_dev->port_num, sizeof(*port), GFP_KERNEL);
		if (!port) {
			CEC_ERR("no memory\n");
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		check_physical_addr_valid(20);	/*delay time:20 x 100ms*/
		init_cec_port_info(port, cec_dev);
		if (copy_to_user(argp, port, sizeof(*port) *
				cec_dev->port_num))
			CEC_ERR("err get port info\n");

		kfree(port);
		break;

	case CEC_IOC_SET_OPTION_WAKEUP:
		tmp = cec_config(0, 0);
		if (arg)
			tmp |= CEC_FUNC_CFG_AUTO_POWER_ON;
		else
			tmp &= ~(CEC_FUNC_CFG_AUTO_POWER_ON);
		cec_config(tmp, 1);
		break;

	case CEC_IOC_SET_AUTO_DEVICE_OFF:
		tmp = cec_config(0, 0);
		if (arg)
			tmp |= CEC_FUNC_CFG_AUTO_STANDBY;
		else
			tmp &= ~(CEC_FUNC_CFG_AUTO_STANDBY);
		cec_config(tmp, 1);
		break;

	case CEC_IOC_SET_OPTION_ENALBE_CEC:
		a = cec_config(0, 0);
		if (arg)
			a |= CEC_FUNC_CFG_CEC_ON;
		else
			a &= ~(CEC_FUNC_CFG_CEC_ON);
		cec_config(a, 1);

		tmp = (1 << HDMI_OPTION_ENABLE_CEC);
		if (arg) {
			cec_dev->hal_flag |= tmp;
			cec_pre_init();
		} else {
			cec_dev->hal_flag &= ~(tmp);
			CEC_INFO("disable CEC\n");
			/*cec_keep_reset();*/
			cec_clear_all_logical_addr(ee_cec);
		}
		break;

	case CEC_IOC_SET_OPTION_SYS_CTRL:
		tmp = (1 << HDMI_OPTION_SYSTEM_CEC_CONTROL);
		if (arg) {
			cec_dev->hal_flag |= tmp;
			/*cec_config(CEC_FUNC_CFG_ALL, 1);*/
		} else
			cec_dev->hal_flag &= ~(tmp);
		cec_dev->hal_flag |= (1 << HDMI_OPTION_SERVICE_FLAG);
		break;

	case CEC_IOC_SET_OPTION_SET_LANG:
		cec_dev->cec_info.menu_lang = arg;
		break;

	case CEC_IOC_GET_CONNECT_STATUS:
		if (copy_from_user(&a, argp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}

		/* mixed for rx & tx */
		/* a is current port idx, 0: tx device */
		if (a != 0) {
			tmp = hdmirx_get_connect_info();
			if (tmp & (1 << (a - 1)))
				tmp = 1;
			else
				tmp = 0;
		} else {
			tmp = cec_dev->tx_dev->hpd_state;
		}
		/*CEC_ERR("port id:%d, sts:%d\n", a, tmp);*/
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	case CEC_IOC_ADD_LOGICAL_ADDR:
		tmp = arg & 0xf;
		/*cec_logicaddr_set(tmp);*/
		/*cec_logicaddr_add(ee_cec, tmp);*/
		if (cec_dev->cec_num > 1)
			cec_logicaddr_add(CEC_B, tmp);
		else
			cec_logicaddr_add(ee_cec, tmp);
		cec_dev->cec_info.addr_enable |= (1 << tmp);

		/* add by hal, to init some data structure */
		cec_dev->cec_info.log_addr = tmp;
		cec_dev->cec_info.power_status = POWER_ON;
		cec_dev->cec_info.vendor_id = cec_dev->v_data.vendor_id;
		strncpy(cec_dev->cec_info.osd_name,
		       cec_dev->v_data.cec_osd_string, 14);
		break;

	case CEC_IOC_CLR_LOGICAL_ADDR:
		if (cec_dev->cec_num > 1)
			cec_clear_all_logical_addr(CEC_B);
		else
			cec_clear_all_logical_addr(ee_cec);
		cec_dev->cec_info.addr_enable = 0;
		break;

	case CEC_IOC_SET_DEV_TYPE:
		cec_dev->dev_type = arg;
		break;

	case CEC_IOC_SET_ARC_ENABLE:
		CEC_INFO("Ioc set arc pin\n");
		cec_enable_arc_pin(arg);
		break;

	case CEC_IOC_GET_BOOT_ADDR:
		tmp = (cec_dev->wakup_data.wk_logic_addr << 16) |
				cec_dev->wakup_data.wk_phy_addr;
		CEC_ERR("Boot addr:%#x\n", (unsigned int)tmp);
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	case CEC_IOC_GET_BOOT_REASON:
		tmp = cec_dev->wakeup_reason;
		CEC_ERR("Boot reason:%#x\n", (unsigned int)tmp);
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	default:
		CEC_ERR("error ioctrl\n");
		break;
	}
	mutex_unlock(&cec_dev->cec_ioctl_mutex);
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
	if (mode) {
		*mode = 0666;
		CEC_INFO("mode is %x\n", *mode);
	} else
		CEC_INFO("mode is null\n");
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
#ifdef CONFIG_HAS_EARLYSUSPEND
static void aocec_early_suspend(struct early_suspend *h)
{
	cec_dev->cec_suspend = CEC_EARLY_SUSPEND;
	CEC_INFO("%s, suspend:%d\n", __func__, cec_dev->cec_suspend);
}

static void aocec_late_resume(struct early_suspend *h)
{
	cec_dev->cec_suspend = CEC_POWER_ON;
	CEC_INFO("%s, suspend:%d\n", __func__, cec_dev->cec_suspend);

}
#endif

#ifdef CONFIG_OF
static const struct cec_platform_data_s cec_gxl_data = {
	.line_reg = 0,
	.line_bit = 8,
	.ee_to_ao = 0,
	.ceca_sts_reg = 0,
	.cecb_ver = CECB_VER_0,
};

static const struct cec_platform_data_s cec_txlx_data = {
	.line_reg = 0,
	.line_bit = 7,
	.ee_to_ao = 1,
	.ceca_sts_reg = 0,
	.cecb_ver = CECB_VER_1,
};

static const struct cec_platform_data_s cec_g12a_data = {
	.line_reg = 1,
	.line_bit = 3,
	.ee_to_ao = 1,
	.ceca_sts_reg = 0,
	.cecb_ver = CECB_VER_1,
};

static const struct cec_platform_data_s cec_txl_data = {
	.line_reg = 0,
	.line_bit = 7,
	.ee_to_ao = 0,
	.ceca_sts_reg = 0,
	.cecb_ver = CECB_VER_0,
};

static const struct cec_platform_data_s cec_tl1_data = {
	.line_reg = 0,
	.line_bit = 10,
	.ee_to_ao = 1,
	.ceca_sts_reg = 1,
	.cecb_ver = CECB_VER_2,
};


static const struct of_device_id aml_cec_dt_match[] = {
	{
		.compatible = "amlogic, amlogic-aocec",
		.data = &cec_gxl_data,
	},
	{
		.compatible = "amlogic, aocec-txlx",
		.data = &cec_txlx_data,
	},
	{
		.compatible = "amlogic, aocec-g12a",
		.data = &cec_g12a_data,
	},
	{
		.compatible = "amlogic, aocec-txl",
		.data = &cec_txl_data,
	},
	{
		.compatible = "amlogic, aocec-tl1",
		.data = &cec_tl1_data,
	},
	{}
};
#endif

static void cec_node_val_init(void)
{
	/* initial main logical address */
	cec_dev->cec_info.log_addr = 0;
	/* all logical address disable */
	cec_dev->cec_info.addr_enable = 0;
	cec_dev->cec_info.open_count.counter = 0;
}

static int aml_cec_probe(struct platform_device *pdev)
{
	struct device *cdev;
	int ret = 0;
	const struct of_device_id *of_id;
#ifdef CONFIG_OF
	struct device_node *node = pdev->dev.of_node;
	int r;
	const char *irq_name = NULL;
	struct pinctrl *pin;
	struct vendor_info_data *vend;
	struct resource *res;
	resource_size_t *base;
#endif

	cec_dev = devm_kzalloc(&pdev->dev, sizeof(struct ao_cec_dev),
			GFP_KERNEL);
	if (IS_ERR(cec_dev)) {
		dev_err(&pdev->dev, "device malloc err!\n");
		ret = -ENOMEM;
		goto tag_cec_devm_err;
	}

	/*will replace by CEC_IOC_SET_DEV_TYPE*/
	cec_dev->dev_type = CEC_PLAYBACK_DEVICE_1_ADDR;
	cec_dev->dbg_dev  = &pdev->dev;
	cec_dev->tx_dev   = get_hdmitx_device();
	cec_dev->cpu_type = get_cpu_type();
	cec_dev->node = pdev->dev.of_node;
	phy_addr_test = 0;
	CEC_ERR("cec driver date:%s\n", CEC_DRIVER_VERSION);
	cec_dbg_init();
	/* cdev registe */
	r = class_register(&aocec_class);
	if (r) {
		CEC_ERR("regist class failed\n");
		ret = -EINVAL;
		goto tag_cec_class_reg;
	}
	pdev->dev.class = &aocec_class;
	r = register_chrdev(0, CEC_DEV_NAME,
					  &hdmitx_cec_fops);
	if (r < 0) {
		CEC_ERR("alloc chrdev failed\n");
		ret = -EINVAL;
		goto tag_cec_chr_reg_err;
	}
	cec_dev->cec_info.dev_no = r;
	CEC_INFO("alloc chrdev %x\n", cec_dev->cec_info.dev_no);
	cdev = device_create(&aocec_class, &pdev->dev,
			     MKDEV(cec_dev->cec_info.dev_no, 0),
			     NULL, CEC_DEV_NAME);
	if (IS_ERR(cdev)) {
		CEC_ERR("create chrdev failed, dev:%p\n", cdev);
		ret = -EINVAL;
		goto tag_cec_device_create_err;
	}

	/*get compatible matched device, to get chip related data*/
	of_id = of_match_device(aml_cec_dt_match, &pdev->dev);
	if (of_id != NULL)
		cec_dev->plat_data = (struct cec_platform_data_s *)of_id->data;
	else
		CEC_ERR("unable to get matched device\n");

	cec_node_val_init();
	init_completion(&cec_dev->rx_ok);
	init_completion(&cec_dev->tx_ok);
	mutex_init(&cec_dev->cec_mutex);
	mutex_init(&cec_dev->cec_ioctl_mutex);
	spin_lock_init(&cec_dev->cec_reg_lock);
	cec_dev->cec_info.remote_cec_dev = input_allocate_device();
	if (!cec_dev->cec_info.remote_cec_dev) {
		CEC_INFO("No enough memory\n");
		ret = -ENOMEM;
		goto tag_cec_alloc_input_err;
	}

	cec_dev->cec_info.remote_cec_dev->name = "cec_input";

	cec_dev->cec_info.remote_cec_dev->evbit[0] = BIT_MASK(EV_KEY);
	cec_dev->cec_info.remote_cec_dev->keybit[BIT_WORD(BTN_0)] =
		BIT_MASK(BTN_0);
	cec_dev->cec_info.remote_cec_dev->id.bustype = BUS_ISA;
	cec_dev->cec_info.remote_cec_dev->id.vendor = 0x1b8e;
	cec_dev->cec_info.remote_cec_dev->id.product = 0x0cec;
	cec_dev->cec_info.remote_cec_dev->id.version = 0x0001;

	set_bit(KEY_POWER, cec_dev->cec_info.remote_cec_dev->keybit);

	if (input_register_device(cec_dev->cec_info.remote_cec_dev)) {
		CEC_INFO("Failed to register device\n");
		input_free_device(cec_dev->cec_info.remote_cec_dev);
	}

	/* config: read from dts */
	r = of_property_read_u32(node, "cec_sel", &(cec_dev->cec_num));
	if (r) {
		CEC_ERR("not find 'port_num'\n");
		cec_dev->cec_num = 0;
	} else {
		CEC_ERR("use two cec ip\n");
	}

	/* if using EE CEC */
	if (of_property_read_bool(node, "ee_cec"))
		ee_cec = CEC_B;
	else
		ee_cec = CEC_A;
	CEC_ERR("using cec:%d\n", ee_cec);
	/* pinmux set */
	if (of_get_property(node, "pinctrl-names", NULL)) {
		pin = devm_pinctrl_get(&pdev->dev);
		/*get sleep state*/
		cec_dev->dbg_dev->pins->sleep_state =
			pinctrl_lookup_state(pin, "cec_pin_sleep");
		if (IS_ERR(cec_dev->dbg_dev->pins->sleep_state))
			CEC_ERR("get sleep state error!\n");
		/*get active state*/
		if (ee_cec == CEC_B) {
			cec_dev->dbg_dev->pins->default_state =
				pinctrl_lookup_state(pin, "hdmitx_aocecb");
			if (IS_ERR(cec_dev->dbg_dev->pins->default_state)) {
				CEC_ERR("get aocecb error!\n");
				cec_dev->dbg_dev->pins->default_state =
					pinctrl_lookup_state(pin, "default");
				if (IS_ERR(
					cec_dev->dbg_dev->pins->default_state))
					CEC_ERR("get default error0\n");
				CEC_ERR("use default cec\n");
				/*force use default*/
				ee_cec = CEC_A;
			}
		} else {
			cec_dev->dbg_dev->pins->default_state =
				pinctrl_lookup_state(pin, "default");
			if (IS_ERR(cec_dev->dbg_dev->pins->default_state))
				CEC_ERR("get default error1!\n");
		}
		/*select pin state*/
		ret = pinctrl_pm_select_default_state(&pdev->dev);
		if (ret > 0)
			CEC_ERR("select state error:0x%x\n", ret);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ao_exit");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start,
						res->end - res->start);
		if (!base) {
			CEC_ERR("Unable to map ao_exit base\n");
			goto tag_cec_reg_map_err;
		}
		cec_dev->exit_reg = (void *)base;
	} else
		CEC_ERR("no ao_exit regs\n");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ao");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start,
						res->end - res->start);
		if (!base) {
			CEC_ERR("Unable to map ao base\n");
			goto tag_cec_reg_map_err;
		}
		cec_dev->cec_reg = (void *)base;
	} else {
		CEC_ERR("no ao regs\n");
		goto tag_cec_reg_map_err;
	}
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hdmirx");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start,
						res->end - res->start);
		if (!base) {
			CEC_ERR("Unable to map hdmirx base\n");
			goto tag_cec_reg_map_err;
		}
		cec_dev->hdmi_rxreg = (void *)base;
	} else
		CEC_ERR("no hdmirx regs\n");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hhi");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start,
						res->end - res->start);
		if (!base) {
			CEC_ERR("Unable to map hhi base\n");
			goto tag_cec_reg_map_err;
		}
		cec_dev->hhi_reg = (void *)base;
	} else
		CEC_ERR("no hhi regs\n");
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "periphs");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start,
						res->end - res->start);
		if (!base) {
			CEC_ERR("Unable to map periphs base\n");
			goto tag_cec_reg_map_err;
		}
		cec_dev->periphs_reg = (void *)base;
	} else
		CEC_ERR("no periphs regs\n");

	r = of_property_read_u32(node, "port_num", &(cec_dev->port_num));
	if (r) {
		CEC_ERR("not find 'port_num'\n");
		cec_dev->port_num = 1;
	}
	r = of_property_read_u32(node, "arc_port_mask", &(cec_dev->arc_port));
	if (r) {
		CEC_ERR("not find 'arc_port_mask'\n");
		cec_dev->arc_port = 0;
	}
	r = of_property_read_u32(node, "output", &(cec_dev->output));
	if (r) {
		CEC_ERR("not find 'output'\n");
		cec_dev->output = 0;
	}
	vend = &cec_dev->v_data;
	r = of_property_read_string(node, "vendor_name",
		(const char **)&(vend->vendor_name));
	if (r)
		CEC_INFO("not find vendor name\n");

	r = of_property_read_u32(node, "vendor_id", &(vend->vendor_id));
	if (r)
		CEC_INFO("not find vendor id\n");

	r = of_property_read_string(node, "product_desc",
		(const char **)&(vend->product_desc));
	if (r)
		CEC_INFO("not find product desc\n");

	r = of_property_read_string(node, "cec_osd_string",
		(const char **)&(vend->cec_osd_string));
	if (r) {
		CEC_INFO("not find cec osd string\n");
		strcpy(vend->cec_osd_string, "AML TV/BOX");
	}
	r = of_property_read_u32(node, "cec_version",
				 &(cec_dev->cec_info.cec_version));
	if (r) {
		/* default set to 2.0 */
		CEC_INFO("not find cec_version\n");
		cec_dev->cec_info.cec_version = CEC_VERSION_14A;
	}

	/* irq set */
	cec_irq_enable(false);
	/* default enable all function*/
	cec_config(CEC_FUNC_CFG_ALL, 1);
	/* for init */
	cec_pre_init();
	/* cec hw module reset */
	if (cec_dev->cec_num > 1) {
		cec_hw_reset(CEC_A);
		cec_hw_reset(CEC_B);
	} else {
		cec_hw_reset(ee_cec);
	}

	if (of_irq_count(node) > 1) {
		/* need enable two irq */
		cec_dev->irq_cecb = of_irq_get(node, 0);/*cecb int*/
		cec_dev->irq_ceca = of_irq_get(node, 1);/*ceca int*/
	} else {
		cec_dev->irq_cecb = of_irq_get(node, 0);
		cec_dev->irq_ceca = cec_dev->irq_cecb;
	}

	CEC_ERR("irq cnt:%d\n", of_irq_count(node));
	if (of_get_property(node, "interrupt-names", NULL)) {
		r = of_property_read_string(node, "interrupt-names", &irq_name);
		if (cec_dev->cec_num > 1) {
			/* request two int source */
			CEC_ERR("request_irq two irq src\n");
			r = request_irq(cec_dev->irq_ceca, &ceca_isr,
				IRQF_SHARED, irq_name, (void *)cec_dev);
			if (r < 0)
				CEC_INFO("aocec irq request fail\n");

			r = request_irq(cec_dev->irq_cecb, &cecb_isr,
				IRQF_SHARED, irq_name, (void *)cec_dev);
			if (r < 0)
				CEC_INFO("cecb irq request fail\n");
		} else {
			if (!r && (ee_cec == CEC_A)) {
				r = request_irq(cec_dev->irq_ceca, &ceca_isr,
					IRQF_SHARED, irq_name, (void *)cec_dev);
				if (r < 0)
					CEC_INFO("aocec irq request fail\n");
			}

			if (!r && (ee_cec == CEC_B)) {
				r = request_irq(cec_dev->irq_cecb, &cecb_isr,
					IRQF_SHARED, irq_name, (void *)cec_dev);
				if (r < 0)
					CEC_INFO("cecb irq request fail\n");
			}
		}
	}

	last_cec_msg = devm_kzalloc(&pdev->dev,
		sizeof(*last_cec_msg), GFP_KERNEL);
	if (!last_cec_msg) {
		CEC_ERR("allocate last_cec_msg failed\n");
		ret = -ENOMEM;
		goto tag_cec_msg_alloc_err;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	aocec_suspend_handler.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 20;
	aocec_suspend_handler.suspend = aocec_early_suspend;
	aocec_suspend_handler.resume  = aocec_late_resume;
	aocec_suspend_handler.param   = cec_dev;
	register_early_suspend(&aocec_suspend_handler);
#endif
	hrtimer_init(&start_bit_check, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	start_bit_check.function = cec_line_check;
	cec_dev->cec_thread = create_workqueue("cec_work");
	if (cec_dev->cec_thread == NULL) {
		CEC_INFO("create work queue failed\n");
		ret = -EFAULT;
		goto tag_cec_threat_err;
	}
	INIT_DELAYED_WORK(&cec_dev->cec_work, cec_task);
	queue_delayed_work(cec_dev->cec_thread, &cec_dev->cec_work, 0);

	scpi_get_wakeup_reason(&cec_dev->wakeup_reason);
	CEC_ERR("wakeup_reason:0x%x\n", cec_dev->wakeup_reason);
	scpi_get_cec_val(SCPI_CMD_GET_CEC1,
				(unsigned int *)&cec_dev->wakup_data);
	scpi_get_cec_val(SCPI_CMD_GET_CEC2, &r);
	CEC_ERR("cev val1: %#x;val2: %#x\n",
			*((unsigned int *)&cec_dev->wakup_data), r);

	cec_irq_enable(true);
	CEC_ERR("%s success end\n", __func__);
	return 0;

tag_cec_msg_alloc_err:
	if (cec_dev->cec_num > 1) {
		free_irq(cec_dev->irq_ceca, (void *)cec_dev);
		free_irq(cec_dev->irq_cecb, (void *)cec_dev);
	} else {
		if (ee_cec == CEC_B)
			free_irq(cec_dev->irq_cecb, (void *)cec_dev);
		else
			free_irq(cec_dev->irq_ceca, (void *)cec_dev);
	}
tag_cec_reg_map_err:
	input_free_device(cec_dev->cec_info.remote_cec_dev);
tag_cec_alloc_input_err:
	destroy_workqueue(cec_dev->cec_thread);
tag_cec_threat_err:
	device_destroy(&aocec_class,
		MKDEV(cec_dev->cec_info.dev_no, 0));
tag_cec_device_create_err:
	unregister_chrdev(cec_dev->cec_info.dev_no, CEC_DEV_NAME);
tag_cec_chr_reg_err:
	class_unregister(&aocec_class);
tag_cec_class_reg:
	devm_kfree(&pdev->dev, cec_dev);
tag_cec_devm_err:
	return ret;
}

static int aml_cec_remove(struct platform_device *pdev)
{
	CEC_INFO("cec uninit!\n");
	if (cec_dev->cec_num > 1) {
		free_irq(cec_dev->irq_ceca, (void *)cec_dev);
		free_irq(cec_dev->irq_cecb, (void *)cec_dev);
	} else {
		if (ee_cec == CEC_B)
			free_irq(cec_dev->irq_cecb, (void *)cec_dev);
		else
			free_irq(cec_dev->irq_ceca, (void *)cec_dev);
	}
	kfree(last_cec_msg);

	if (cec_dev->cec_thread) {
		cancel_delayed_work_sync(&cec_dev->cec_work);
		destroy_workqueue(cec_dev->cec_thread);
	}
	input_unregister_device(cec_dev->cec_info.remote_cec_dev);
	unregister_chrdev(cec_dev->cec_info.dev_no, CEC_DEV_NAME);
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

static void aml_cec_pm_complete(struct device *dev)
{
	int exit = 0;

	if (cec_dev->exit_reg) {
		exit = readl(cec_dev->exit_reg);
		CEC_INFO("wake up flag:%x\n", exit);
	}
	if (((exit >> 28) & 0xf) == CEC_WAKEUP)
		cec_key_report(0);
}

static int aml_cec_suspend_noirq(struct device *dev)
{
	int ret = 0;

	CEC_INFO("cec suspend noirq\n");
	if (cec_dev->cec_num > 1)
		cec_clear_all_logical_addr(CEC_B);
	else
		cec_clear_all_logical_addr(ee_cec);

	if (!IS_ERR(cec_dev->dbg_dev->pins->sleep_state))
		ret = pinctrl_pm_select_sleep_state(cec_dev->dbg_dev);
	else
		CEC_ERR("pinctrl sleep_state error\n");
	return 0;
}

static int aml_cec_resume_noirq(struct device *dev)
{
	int ret = 0;
	unsigned int temp;

	CEC_INFO("cec resume noirq!\n");

	cec_dev->cec_info.power_status = TRANS_STANDBY_TO_ON;

	scpi_get_wakeup_reason(&cec_dev->wakeup_reason);
	CEC_ERR("wakeup_reason:0x%x\n", cec_dev->wakeup_reason);

	scpi_get_cec_val(SCPI_CMD_GET_CEC1,
				(unsigned int *)&cec_dev->wakup_data);
	scpi_get_cec_val(SCPI_CMD_GET_CEC2, &temp);
	CEC_ERR("cev val1: %#x;val2: %#x\n",
					*((unsigned int *)&cec_dev->wakup_data),
						temp);

	cec_dev->cec_info.power_status = TRANS_STANDBY_TO_ON;
	cec_dev->cec_suspend = CEC_POWER_RESUME;
	if (!IS_ERR(cec_dev->dbg_dev->pins->default_state))
		ret = pinctrl_pm_select_default_state(cec_dev->dbg_dev);
	else
		CEC_ERR("pinctrl default_state error\n");
	return 0;
}

static const struct dev_pm_ops aml_cec_pm = {
	.prepare  = aml_cec_pm_prepare,
	.complete = aml_cec_pm_complete,
	.suspend_noirq = aml_cec_suspend_noirq,
	.resume_noirq = aml_cec_resume_noirq,
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
