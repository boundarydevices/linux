// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek HDMI CEC Driver
 *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/module.h>

#include "mtk_cec_v2.h"

#define cec_clk_27m 0x1
#define cec_clk_32k 0x2

unsigned char cec_clock = cec_clk_27m;

static const char * const mtk_cec_clk_names[MTK_CEC_CLK_COUNT] = {
	[MTK_CEC_66M_H] = "cec_66m_h",
	[MTK_CEC_66M_B] = "cec_66m_b",
	[MTK_HDMI_32K] = "hdmi_32k",
	[MTK_HDMI_26M] = "hdmi_26m",
};

static inline void mtk_cec_clear_bits(struct mtk_cec *cec,
	unsigned int offset, unsigned int bits)
{
	void __iomem *reg = cec->regs + offset;
	u32 tmp;

	tmp = readl(reg);
	tmp &= ~bits;
	writel(tmp, reg);
}

static inline void mtk_cec_set_bits(struct mtk_cec *cec,
	unsigned int offset, unsigned int bits)
{
	void __iomem *reg = cec->regs + offset;
	u32 tmp;

	tmp = readl(reg);
	tmp |= bits;
	writel(tmp, reg);
}

static inline void mtk_cec_mask(struct mtk_cec *cec, unsigned int offset,
			 unsigned int val, unsigned int mask)
{
	u32 tmp = readl(cec->regs + offset) & ~mask;

	tmp |= (val & mask);
	writel(tmp, cec->regs + offset);
}

static inline void mtk_cec_write(struct mtk_cec *cec,
	unsigned int offset, unsigned int val)
{
	writel(val, cec->regs + offset);
}


static inline unsigned int mtk_cec_read(struct mtk_cec *cec,
	unsigned int offset)
{
	return readl(cec->regs + offset);
}

static inline bool mtk_cec_read_bit(struct mtk_cec *cec,
	unsigned int offset, unsigned int bits)
{
	return (readl(cec->regs + offset) & bits) ? true : false;

}

inline void mtk_cec_rx_reset(struct mtk_cec *cec)
{
	unsigned int val;

	val = mtk_cec_read(cec, CEC2_TR_CONFIG);
	mtk_cec_write(cec, CEC2_TR_CONFIG, val & (~CEC2_RX_RESET_WRITE));
	udelay(1);
	val = mtk_cec_read(cec, CEC2_TR_CONFIG);
	mtk_cec_write(cec, CEC2_TR_CONFIG, val |
		(CEC2_TX_RESET_WRITE | CEC2_RX_RESET_WRITE));
}

inline void mtk_cec_tx_reset(struct mtk_cec *cec)
{
	unsigned int val;

	val = mtk_cec_read(cec, CEC2_TR_CONFIG);
	mtk_cec_write(cec, CEC2_TR_CONFIG, val & (~CEC2_TX_RESET_WRITE));
	udelay(1);
	val = mtk_cec_read(cec, CEC2_TR_CONFIG);
	mtk_cec_write(cec, CEC2_TR_CONFIG, val |
		(CEC2_TX_RESET_WRITE | CEC2_RX_RESET_WRITE));
}

static void mtk_cec_clk_div(struct mtk_cec *cec, unsigned int clk_src)
{
	if (clk_src == cec_clk_27m) {
		/* divide the clock of 26Mhz to 100khz */
		mtk_cec_mask(cec, CEC2_CKGEN, CEC2_CLK_27M_EN, CEC2_CLK_27M_EN);
		mtk_cec_mask(cec, CEC2_CKGEN, CEC2_DIV_SEL_100K, CEC2_CLK_DIV);
		mtk_cec_mask(cec, CEC2_CKGEN, 0x0, CEC2_CLK_SEL_DIV);
	} else {
		/* do not divide the clock of 32khz */
		mtk_cec_mask(cec, CEC2_CKGEN, 0x0, CEC2_CLK_27M_EN);
		mtk_cec_mask(cec, CEC2_CKGEN, CEC2_CLK_32K_EN, CEC2_CLK_32K_EN);
		mtk_cec_mask(cec, CEC2_CKGEN,
			CEC2_CLK_SEL_DIV, CEC2_CLK_SEL_DIV);
		return;
	}
}

static void mtk_cec_rx_check_addr(struct mtk_cec *cec, bool enable)
{
	if (enable) {
		mtk_cec_mask(cec, CEC2_TR_CONFIG,
			CEC2_RX_CHK_DST, CEC2_RX_CHK_DST);
		mtk_cec_mask(cec, CEC2_TR_CONFIG, 0, CEC2_BYPASS);
	} else {
		mtk_cec_mask(cec, CEC2_TR_CONFIG, 0, CEC2_RX_CHK_DST);
		mtk_cec_mask(cec, CEC2_TR_CONFIG, CEC2_BYPASS, CEC2_BYPASS);
	}
}

static void mtk_cec_rx_timing_config(struct mtk_cec *cec, unsigned int clk_src)
{
	if (clk_src == cec_clk_27m) {
		/* use default timing value */
		mtk_cec_write(cec, CEC2_RX_TIMER_START_R, 0x0186015e);
		mtk_cec_write(cec, CEC2_RX_TIMER_START_F, 0x01d601ae);
		mtk_cec_write(cec, CEC2_RX_TIMER_DATA, 0x011300cd);
		mtk_cec_write(cec, CEC2_RX_TIMER_ACK, 0x00690082);
		mtk_cec_write(cec, CEC2_RX_TIMER_ERROR, 0x01680000);
		mtk_cec_mask(cec, CEC2_CKGEN, CEC2_CLK_27M_EN, CEC2_CLK_27M_EN);
	} else {
		mtk_cec_write(cec, CEC2_RX_TIMER_START_R, 0x007d0070);
		mtk_cec_write(cec, CEC2_RX_TIMER_START_F, 0x0099008a);
		mtk_cec_write(cec, CEC2_RX_TIMER_DATA, 0x00230040);
		mtk_cec_write(cec, CEC2_RX_TIMER_ACK, 0x00000030);
		mtk_cec_write(cec, CEC2_RX_TIMER_ERROR, 0x007300aa);
		return;
	}

	mtk_cec_mask(cec, CEC2_TR_CONFIG, CEC2_RX_ACK_ERROR_BYPASS,
		CEC2_RX_ACK_ERROR_BYPASS);
	mtk_cec_rx_reset(cec);
}

static void mtk_cec_tx_timing_config(struct mtk_cec *cec, unsigned int clk_src)
{
	if (clk_src == cec_clk_27m) {
		/* use default timing value */
		mtk_cec_write(cec, CEC2_TX_TIMER_START, 0x01c20172);
		mtk_cec_write(cec, CEC2_TX_TIMER_DATA_R, 0x003c0096);
		mtk_cec_write(cec, CEC2_TX_T_DATA_F, 0x00f000f0);
		mtk_cec_write(cec, TX_TIMER_DATA_S, 0x00040069);
		mtk_cec_write(cec, CEC2_LINE_DET, 0x1f4004b0);

		mtk_cec_mask(cec, CEC2_TX_TIMER_ACK, 0x00aa0028,
			CEC2_TX_TIMER_ACK_MAX | CEC2_TX_TIMER_ACK_MIN);
		mtk_cec_mask(cec, CEC2_CKGEN, CEC2_CLK_27M_EN, CEC2_CLK_27M_EN);
	} else {
		mtk_cec_write(cec, CEC2_TX_TIMER_START, 0x00920079);
		mtk_cec_write(cec, CEC2_TX_TIMER_DATA_R, 0x00140031);
		mtk_cec_write(cec, CEC2_TX_T_DATA_F, 0x004f004f);
		mtk_cec_write(cec, TX_TIMER_DATA_S, 0x00010022);
		mtk_cec_write(cec, CEC2_LINE_DET, 0x0a000180);

		mtk_cec_mask(cec, CEC2_TX_TIMER_ACK, 0x0036000d,
			CEC2_TX_TIMER_ACK_MAX | CEC2_TX_TIMER_ACK_MIN);
		return;
	}
	mtk_cec_mask(cec, CEC2_TX_ARB, 0x01 << 24,
		CEC2_TX_MAX_RETRANSMIT_NUM_ARB);
	mtk_cec_mask(cec, CEC2_TX_ARB, 0x01 << 20,
		CEC2_TX_MAX_RETRANSMIT_NUM_COL);
	mtk_cec_mask(cec, CEC2_TX_ARB, 0x01 << 16,
		CEC2_TX_MAX_RETRANSMIT_NUM_NAK);
	mtk_cec_mask(cec, CEC2_TX_ARB, 0x04 << 8,
		CEC2_TX_BCNT_RETRANSMIT);
	mtk_cec_mask(cec, CEC2_TX_ARB, 0x07 << 4,
		CEC2_TX_BCNT_NEW_MSG);
	mtk_cec_mask(cec, CEC2_TX_ARB, 0x05,
		CEC2_TX_BCNT_NEW_INIT);

	mtk_cec_mask(cec, CEC2_CKGEN, CEC2_CLK_TX_EN, CEC2_CLK_TX_EN);
	mtk_cec_tx_reset(cec);
}

inline void mtk_cec_txrx_interrupts_config(struct mtk_cec *cec)
{
	/*disable all irq config*/
	mtk_cec_write(cec, CEC2_INT_CLR, 0xffffffff);
	mtk_cec_write(cec, CEC2_INT_CLR, 0);

	/*enable all rx irq config except FSM_CHG_INT*/
	mtk_cec_mask(cec, CEC2_INT_EN, CEC2_RX_INT_ALL_EN, CEC2_RX_INT_ALL_EN);
	mtk_cec_mask(cec, CEC2_INT_EN, 0, CEC2_RX_FSM_CHG_INT_EN); //can.zeng add
}

static void mtk_cec_set_logic_addr(struct mtk_cec *cec,
	u8 la, enum cec_la_num num)
{
	if (num == DEVICE_ADDR_1)
		mtk_cec_mask(cec, CEC2_TR_CONFIG, la << CEC2_LA1_SHIFT,
			CEC2_DEVICE_ADDR1);
	else if (num == DEVICE_ADDR_2)
		mtk_cec_mask(cec, CEC2_TR_CONFIG, la << CEC2_LA2_SHIFT,
			CEC2_DEVICE_ADDR2);
	else if (num == DEVICE_ADDR_3)
		mtk_cec_mask(cec, CEC2_TR_CONFIG, la << CEC2_LA3_SHIFT,
			CEC2_DEVICE_ADDR3);
	else
		dev_dbg(cec->dev, "Cannot support more than 3 logic address");
}

static void mtk_cec_logic_addr_config(struct cec_adapter *adap)
{
	__u8 unreg_la_addr = 0xff;
	struct mtk_cec *cec = adap->priv;
	struct cec_log_addrs *cec_la = &(adap->log_addrs);

	memset((void *)cec_la, 0, sizeof(struct cec_log_addrs));
	cec_la->num_log_addrs = 0;
	cec_la->log_addr[0] = CEC_LOG_ADDR_UNREGISTERED;
	cec_la->cec_version = CEC_OP_CEC_VERSION_1_4;
	cec_la->log_addr_type[0] = CEC_LOG_ADDR_TYPE_UNREGISTERED;
	cec_la->primary_device_type[0] = CEC_LOG_ADDR_TYPE_UNREGISTERED;
	cec_la->log_addr_mask = 0;

	mtk_cec_set_logic_addr(cec, unreg_la_addr, DEVICE_ADDR_1);
	mtk_cec_set_logic_addr(cec, unreg_la_addr, DEVICE_ADDR_2);
	mtk_cec_set_logic_addr(cec, unreg_la_addr, DEVICE_ADDR_3);
}

inline void mtk_cec_int_disable_all(struct mtk_cec *cec)
{
	mtk_cec_mask(cec, CEC2_INT_EN, 0x0, CEC2_INT_ALL_EN);
}

static void mtk_cec_init(struct mtk_cec *cec)
{
	mtk_cec_clk_div(cec, cec_clock);
	mtk_cec_rx_check_addr(cec, true);
	mtk_cec_rx_timing_config(cec, cec_clock);
	mtk_cec_tx_timing_config(cec, cec_clock);
	mtk_cec_txrx_interrupts_config(cec);
	mtk_cec_logic_addr_config(cec->adap);

}

static void mtk_cec_deinit(struct mtk_cec *cec)
{
	mtk_cec_int_disable_all(cec);
}

static int mtk_cec_clk_enable(struct mtk_cec *cec, bool enable)
{
	int i, ret;

	if (enable == true) {
		for (i = 0; i < ARRAY_SIZE(mtk_cec_clk_names); i++) {
			ret = clk_prepare_enable(cec->clk[i]);
			if (ret) {
				dev_err(cec->dev, "Failed to enable cec clk: %d\n", ret);
				return ret;
			}
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(mtk_cec_clk_names); i++)
			clk_disable_unprepare(cec->clk[i]);
	}

	return 0;
}

static int mtk_cec_power_enable(struct mtk_cec *cec, bool enable)
{
	dev_dbg(cec->dev, "enable=%d\n", enable);

	if (enable) {
		mtk_cec_clk_enable(cec, true);
		mtk_cec_mask(cec, CEC2_CKGEN, 0, CEC2_CLK_PDN);
		cec->cec_enabled = true;
	} else {
		mtk_cec_mask(cec, CEC2_CKGEN, CEC2_CLK_PDN, CEC2_CLK_PDN);
		mtk_cec_clk_enable(cec, false);
		cec->cec_enabled = false;
	}
	return 0;

}

static int mtk_cec_adap_enable(struct cec_adapter *adap, bool enable)
{
	struct mtk_cec *cec = adap->priv;

	dev_dbg(cec->dev, "enable=%d\n", enable);
	if (enable) {
		if (cec->cec_enabled)
			dev_dbg(cec->dev, "cec has already been enabled, return\n");
		else {
			mtk_cec_power_enable(cec, true);
			mtk_cec_init(cec);
		}
	} else {
		if (!cec->cec_enabled)
			dev_dbg(cec->dev, "cec has already been disabled, return\n");
		else {
			mtk_cec_deinit(cec);
			mtk_cec_power_enable(cec, false);
		}
	}
	return 0;
}

static int mtk_cec_adap_log_addr(struct cec_adapter *adap, u8 logical_addr)
{
	struct mtk_cec *cec = adap->priv;

	dev_dbg(cec->dev, "logical_address=%d\n", logical_addr);

	/* to do :mtk cec can accept 3 logic address, now just accept one */
	if (logical_addr != CEC_LOG_ADDR_INVALID)
		mtk_cec_set_logic_addr(cec, logical_addr, DEVICE_ADDR_1);

	return 0;
}

static void mtk_cec_set_msg_header(struct mtk_cec *cec, struct cec_msg *msg)
{
	struct cec_msg_header_block header = { };

	header.destination = (msg->msg[0]) & 0x0f;
	header.initiator = ((msg->msg[0]) & 0xf0) >> 4;
	mtk_cec_mask(cec, CEC2_TX_HEADER,
		header.initiator << 4, CEC2_TX_HEADER_SRC);
	mtk_cec_mask(cec, CEC2_TX_HEADER,
		header.destination, CEC2_TX_HEADER_DST);
}

static void mtk_cec_print_cec_frame(struct cec_frame *frame)
{
	struct cec_msg *msg = frame->msg;
	unsigned char header = msg->msg[0];

	pr_debug("message initiator:%d,follower:%d,length:%d,opcode:0x%x\n",
		(header & 0xf0) >> 4,
		header & 0x0f,
		msg->len,
		msg->msg[1]);
}

inline bool mtk_cec_tx_fsm_fail(struct mtk_cec *cec)
{
	return ((mtk_cec_read(cec, CEC2_TX_STATUS) &
		CEC2_TX_FSM) >> 16) == 0x10 ? true : false;
}


inline bool mtk_cec_tx_fail_int(struct mtk_cec *cec)
{
	return ((mtk_cec_read(cec, CEC2_INT_STA) &
		mtk_cec_read(cec, CEC2_INT_EN)) &
		CEC2_TX_INT_FAIL) ? true : false;
}


inline bool mtk_cec_input(struct mtk_cec *cec)
{
	return mtk_cec_read_bit(cec, CEC2_RX_STATUS, CEC2_CEC_INPUT);
}

inline bool mtk_cec_tx_fsm_idle(struct mtk_cec *cec)
{
	return ((mtk_cec_read(cec, CEC2_TX_STATUS) &
		CEC2_TX_FSM) >> 16) == 0x01 ? true : false;
}

inline void mtk_cec_tx_int_disable_all(struct mtk_cec *cec)
{
	mtk_cec_mask(cec, CEC2_INT_EN, 0x0, CEC2_TX_INT_ALL_EN);
}

inline void mtk_cec_tx_int_clear_all(struct mtk_cec *cec)
{
	mtk_cec_mask(cec, CEC2_INT_CLR,
		CEC2_TX_INT_ALL_CLR, CEC2_TX_INT_ALL_CLR);
	mtk_cec_mask(cec, CEC2_INT_CLR, 0, CEC2_TX_INT_ALL_CLR);
}

inline void mtk_cec_tx_finish_int_en(struct mtk_cec *cec, unsigned int en)
{
	mtk_cec_mask(cec, CEC2_INT_EN, en << 8, CEC2_TX_DATA_FINISH_INT_EN);
}

inline void mtk_cec_tx_fail_longlow_int_en(struct mtk_cec *cec, unsigned int en)
{
	mtk_cec_mask(cec, CEC2_INT_EN, en << 7, CEC2_LINE_lOW_LONG_INT_EN);
}

inline void mtk_cec_tx_fail_retransmit_int_en(struct mtk_cec *cec,
	unsigned int en)
{
	mtk_cec_mask(cec, CEC2_INT_EN, en << 12,
		CEC2_TX_FAIL_RETRANSMIT_INT_EN);
}

inline void mtk_cec_tx_trigger_send(struct mtk_cec *cec)
{
	dev_dbg(cec->dev, "[CEC] %s,%d\n", __func__, __LINE__);
	mtk_cec_mask(cec, CEC2_TX_HEADER, CEC2_TX_READY, CEC2_TX_READY);
}

inline void mtk_cec_tx_set_h_eom(struct mtk_cec *cec, unsigned int heom)
{
	mtk_cec_mask(cec, CEC2_TX_HEADER, heom << 8, CEC2_TX_HEADER_EOM);
}

inline void mtk_cec_tx_set_data_len(struct mtk_cec *cec, unsigned int len)
{
	mtk_cec_mask(cec, CEC2_TX_HEADER, len << 12, CEC2_TX_SEND_CNT);
}

inline void mtk_cec_tx_detect_startbit(struct mtk_cec *cec)
{
	static unsigned int tx_fsm, pre_tx_fsm;

	tx_fsm = ((mtk_cec_read(cec, CEC2_TX_STATUS)) & CEC2_TX_FSM) >> 16;
	if ((tx_fsm == 0x08) && (pre_tx_fsm == 0x20)) {
		mtk_cec_tx_reset(cec);
		dev_dbg(cec->dev, "\nDetect Start bit Err\n\n");
		mtk_cec_tx_int_clear_all(cec);
		cec->transmitting.status.tx_status = CEC_TX_FAIL;
		cec_transmit_attempt_done(cec->adap, CEC_TX_STATUS_ERROR);
	}
	pre_tx_fsm = tx_fsm;
}

inline bool mtk_cec_tx_fail_longlow(struct mtk_cec *cec)
{
	return mtk_cec_read_bit(cec, CEC2_INT_STA, CEC2_LINE_lOW_LONG_INT_STA);
}

inline void mtk_cec_tx_fail_longlow_intclr(struct mtk_cec *cec)
{
	mtk_cec_mask(cec, CEC2_INT_CLR,
		CEC2_LINE_lOW_LONG_INT_CLR, CEC2_LINE_lOW_LONG_INT_CLR);
	mtk_cec_mask(cec, CEC2_INT_CLR, 0, CEC2_LINE_lOW_LONG_INT_CLR);
}

inline bool mtk_cec_tx_fail_retransmit_intsta(struct mtk_cec *cec)
{
	return mtk_cec_read_bit(cec, CEC2_INT_STA,
		CEC2_TX_FAIL_RETRANSMIT_INT_STA);
}

inline void mtk_cec_tx_fail_retransmit_intclr(struct mtk_cec *cec)
{
	mtk_cec_mask(cec, CEC2_INT_CLR,
		CEC2_TX_FAIL_RETRANSMIT_INT_CLR,
		CEC2_TX_FAIL_RETRANSMIT_INT_CLR);
	mtk_cec_mask(cec, CEC2_INT_CLR, 0, CEC2_TX_FAIL_RETRANSMIT_INT_CLR);
}

inline bool mtk_cec_tx_fail_h_ack(struct mtk_cec *cec)
{
	return mtk_cec_read_bit(cec, CEC2_INT_STA,
		CEC2_TX_FAIL_HEADER_ACK_INT_STA);
}

inline void mtk_cec_tx_fail_h_ack_intclr(struct mtk_cec *cec)
{
	mtk_cec_mask(cec, CEC2_INT_CLR,
		CEC2_TX_FAIL_HEADER_ACK_INT_CLR,
		CEC2_TX_FAIL_HEADER_ACK_INT_CLR);
	mtk_cec_mask(cec, CEC2_INT_CLR, 0, CEC2_TX_FAIL_HEADER_ACK_INT_CLR);
}

inline bool mtk_cec_tx_fail_data_ack(struct mtk_cec *cec)
{
	return mtk_cec_read_bit(cec, CEC2_INT_STA,
		CEC2_TX_FAIL_DATA_ACK_INT_STA);
}

inline void mtk_cec_tx_fail_data_ack_intclr(struct mtk_cec *cec)
{
	mtk_cec_mask(cec, CEC2_INT_CLR,
		CEC2_TX_FAIL_DATA_ACK_INT_CLR, CEC2_TX_FAIL_DATA_ACK_INT_CLR);
	mtk_cec_mask(cec, CEC2_INT_CLR, 0, CEC2_TX_FAIL_DATA_ACK_INT_CLR);
}

inline bool mtk_cec_tx_fail_data(struct mtk_cec *cec)
{
	return mtk_cec_read_bit(cec, CEC2_INT_STA, CEC2_TX_FAIL_DATA_INT_STA);
}

inline void mtk_cec_tx_fail_data_intclr(struct mtk_cec *cec)
{
	mtk_cec_mask(cec, CEC2_INT_CLR,
		CEC2_TX_FAIL_DATA_INT_CLR, CEC2_TX_FAIL_DATA_INT_CLR);
	mtk_cec_mask(cec, CEC2_INT_CLR, 0, CEC2_TX_FAIL_DATA_INT_CLR);
}

inline bool mtk_cec_tx_fail_header(struct mtk_cec *cec)
{
	return mtk_cec_read_bit(cec, CEC2_INT_STA, CEC2_TX_FAIL_HEADER_INT_STA);
}

inline void mtk_cec_tx_fail_header_intclr(struct mtk_cec *cec)
{
	mtk_cec_mask(cec, CEC2_INT_CLR,
		CEC2_TX_FAIL_HEADER_INT_CLR, CEC2_TX_FAIL_HEADER_INT_CLR);
	mtk_cec_mask(cec, CEC2_INT_CLR, 0, CEC2_TX_FAIL_HEADER_INT_CLR);
}

inline bool mtk_cec_tx_fail_src(struct mtk_cec *cec)
{
	return mtk_cec_read_bit(cec, CEC2_INT_STA, CEC2_TX_FAIL_SRC_INT_STA);
}

inline void mtk_cec_tx_fail_src_intclr(struct mtk_cec *cec)
{
	mtk_cec_mask(cec, CEC2_INT_CLR,
		CEC2_TX_FAIL_SRC_INT_CLR, CEC2_TX_FAIL_SRC_INT_CLR);
	mtk_cec_mask(cec, CEC2_INT_CLR, 0, CEC2_TX_FAIL_SRC_INT_CLR);
}

inline bool mtk_cec_tx_finish(struct mtk_cec *cec)
{
	return mtk_cec_read_bit(cec, CEC2_INT_STA, CEC2_TX_DATA_FINISH_INT_STA);
}

inline void mtk_cec_tx_finish_clr(struct mtk_cec *cec)
{
	mtk_cec_mask(cec, CEC2_INT_CLR,
		CEC2_TX_DATA_FINISH_INT_CLR, CEC2_TX_DATA_FINISH_INT_CLR);
	mtk_cec_mask(cec, CEC2_INT_CLR,
		0, CEC2_TX_DATA_FINISH_INT_CLR);
}

static void mtk_cec_tx_set_data(struct mtk_cec *cec,
	struct cec_msg *cec_msg, unsigned char data_len)
{
	unsigned char i, addr, byte_shift;

	for (i = 0; i < data_len; i++) {
		addr = (i / 4) * 4 + CEC2_TX_DATA0;
		byte_shift = (i % 4 * 8);
		mtk_cec_mask(cec, addr, cec_msg->msg[i+1] << byte_shift,
			0xff << byte_shift);
	}
}

inline bool mtk_cec_rx_header_arrived(struct mtk_cec *cec)
{
	bool is_rx_fsm_irq;
	bool is_rx_fsm_header;

	is_rx_fsm_irq = mtk_cec_read_bit(cec, CEC2_INT_STA,
		CEC2_RX_FSM_CHG_INT_STA);
	is_rx_fsm_header = ((mtk_cec_read(cec,
		CEC2_RX_STATUS) & CEC2_RX_FSM) >> 16) == 0x08;
	if (is_rx_fsm_irq && is_rx_fsm_header)
		return true;
	else
		return false;
}

inline unsigned int mtk_cec_rx_get_dst(struct mtk_cec *cec)
{
	return mtk_cec_read(cec, CEC2_RX_BUF_HEADER) & CEC2_RX_HEADER_DST;
}

inline unsigned int mtk_cec_rx_get_src(struct mtk_cec *cec)
{
	return (mtk_cec_read(cec, CEC2_RX_BUF_HEADER) &
		CEC2_RX_HEADER_SRC) >> 4;
}

inline bool mtk_cec_rx_data_arrived(struct mtk_cec *cec)
{
	return mtk_cec_read_bit(cec, CEC2_INT_STA, CEC2_RX_DATA_RCVD_INT_STA);
}

inline bool mtk_cec_rx_buffer_ready(struct mtk_cec *cec)
{
	return mtk_cec_read_bit(cec, CEC2_INT_STA, CEC2_RX_BUF_READY_INT_STA);
}

inline bool mtk_cec_rx_buffer_full(struct mtk_cec *cec)
{
	return mtk_cec_read_bit(cec, CEC2_INT_STA, CEC2_RX_BUF_FULL_INT_STA);
}


inline void mtk_cec_rx_int_all_clr(struct mtk_cec *cec)
{
	mtk_cec_mask(cec, CEC2_INT_CLR, CEC2_RX_INT_ALL_CLR, CEC2_RX_INT_ALL_CLR);
	mtk_cec_mask(cec, CEC2_INT_CLR, 0, CEC2_RX_INT_ALL_CLR);
}

static int mtk_cec_send_msg(struct mtk_cec *cec)
{
	unsigned char msg_data_size;
	unsigned char size;
	struct cec_frame *frame = &cec->transmitting;
	unsigned int max_retries = 2;

	size = frame->msg->len;
	msg_data_size = size - CEC_HEADER_BLOCK_SIZE;

	mtk_cec_print_cec_frame(frame);

	if (frame->status.tx_status == CEC_TX_START) {

		while (mtk_cec_tx_fsm_fail(cec) || mtk_cec_tx_fail_int(cec)) {
			mtk_cec_tx_reset(cec);
			if (mtk_cec_input(cec))
				mtk_cec_tx_fail_longlow_intclr(cec);
			if (max_retries-- == 0)
				return -EIO;
			/* wait cec isr thread handle */
			mdelay(100);
		}

		if (!mtk_cec_tx_fsm_idle(cec))
			mtk_cec_tx_reset(cec);

		/* fill header block*/
		mtk_cec_set_msg_header(cec, frame->msg);
		dev_dbg(cec->dev, "tx header=0x%02x,seq=%d,op=0x%x,l=%d\n",
			frame->msg->msg[0], frame->msg->sequence,
			frame->msg->msg[1], frame->msg->len);

		/*disable and clear interrupts*/
		mtk_cec_tx_int_disable_all(cec);
		mtk_cec_tx_int_clear_all(cec);

		/* fill data block*/
		if (msg_data_size == 0)
			mtk_cec_tx_set_h_eom(cec, 1);
		else {
			mtk_cec_tx_set_h_eom(cec, 0);
			mtk_cec_tx_set_data_len(cec, msg_data_size);
			mtk_cec_tx_set_data(cec, frame->msg, msg_data_size);
		}
		cec->transmitting.status.tx_status = CEC_TX_Transmitting;
	}

	mtk_cec_tx_finish_int_en(cec, 1);
	mtk_cec_tx_fail_longlow_int_en(cec, 1);
	mtk_cec_tx_fail_retransmit_int_en(cec, 1);
	mtk_cec_tx_trigger_send(cec);

	return 0;
}

static void mtk_cec_msg_init(struct mtk_cec *cec, struct cec_msg *msg)
{
	cec->transmitting.msg = msg;
	cec->transmitting.retry_count = 0;
	cec->transmitting.status.tx_status = CEC_TX_START;
}

static int mtk_cec_adap_transmit(struct cec_adapter *adap, u8 attempts,
				 u32 signal_free_time, struct cec_msg *msg)
{
	struct mtk_cec *cec = adap->priv;

	dev_dbg(cec->dev, "[CEC] %s,%d\n", __func__, __LINE__);
	mtk_cec_msg_init(cec, msg);
	mtk_cec_send_msg(cec);

	return 0;
}

inline unsigned int mtk_cec_rx_data_len(struct mtk_cec *cec)
{
	unsigned int val;

	val = mtk_cec_read(cec, CEC2_RX_BUF_HEADER);
	if (val & CEC2_RX_HEADER_EOM)
		return 0;
	if (val & CEC2_RX_BUF_CNT)
		return ((val & CEC2_RX_BUF_CNT) >> 12);
	else
		return 0;
}

inline void mtk_cec_rx_notify_data_taken(struct mtk_cec *cec)
{
	mtk_cec_mask(cec, CEC2_RX_BUF_HEADER,
		CEC2_RX_BUF_RISC_ACK, CEC2_RX_BUF_RISC_ACK);
}

void mtk_cec_tx_work_handle(struct work_struct *work)
{
	struct mtk_cec *cec = container_of(work, struct mtk_cec, cec_tx_work);

	if (cec->transmitting.status.tx_status == CEC_TX_COMPLETE) {
		dev_dbg(cec->dev, "notify CEC CORE adapter transmit done\n");
		cec_transmit_attempt_done(cec->adap, CEC_TX_STATUS_OK);

	} else if ((cec->transmitting.status.tx_status == CEC_TX_FAIL_HNAK) ||
		(cec->transmitting.status.tx_status == CEC_TX_FAIL_DNAK)) {
		dev_dbg(cec->dev, "notify CEC CORE adapter transmit NACK\n");
		cec_transmit_attempt_done(cec->adap, CEC_TX_STATUS_NACK);

	} else if ((cec->transmitting.status.tx_status == CEC_TX_FAIL) ||
		(cec->transmitting.status.tx_status == CEC_TX_FAIL_HEAD) ||
		(cec->transmitting.status.tx_status == CEC_TX_FAIL_DATA) ||
		(cec->transmitting.status.tx_status == CEC_TX_FAIL_SRC) ||
		(cec->transmitting.status.tx_status == CEC_TX_FAIL_LOW) ||
		(cec->transmitting.status.tx_status == CEC_TX_FAIL_RETR)) {
		dev_dbg(cec->dev, "notify CEC CORE adapter transmit error\n");
		cec_transmit_attempt_done(cec->adap, CEC_TX_STATUS_ERROR);

	} else {
		dev_dbg(cec->dev, "WARNING: no corresponding case\n");
		cec_transmit_attempt_done(cec->adap, CEC_TX_STATUS_ERROR);
	}
}

void mtk_cec_rx_work_handle(struct work_struct *work)
{
	struct mtk_cec *cec = container_of(work, struct mtk_cec, cec_rx_work);
	struct cec_frame *received_msg = &cec->received;

	if (received_msg->status.rx_status == CEC_RX_COMPLETE) {
		dev_dbg(cec->dev, "notify CEC CORE adapter receive msg\n");
		cec_received_msg(cec->adap, received_msg->msg);
	}
}

static void mtk_cec_receiving_msg(struct mtk_cec *cec)
{
	unsigned char rxlen;
	unsigned char i, addr, byte_shift;

	struct cec_msg_header_block header = { };
	struct cec_frame *received_msg = &cec->received;

	received_msg->status.rx_status = CEC_RX_Receiving;
	if (!mtk_cec_rx_buffer_ready(cec)) {
		received_msg->status.rx_status = CEC_RX_FAIL;
		dev_dbg(cec->dev, "%s,cec buffer not ready\n", __func__);
		return;
	}

	/* <polling message> only */
	if (mtk_cec_rx_data_len(cec) == 0) {
		dev_dbg(cec->dev, "polling message\n");
		mtk_cec_rx_notify_data_taken(cec);
		received_msg->status.rx_status = CEC_RX_COMPLETE;
		return;
	}

	header.initiator = mtk_cec_rx_get_src(cec);
	header.destination = mtk_cec_rx_get_dst(cec);
	received_msg->msg->msg[0] =
		(header.initiator << 4 | header.destination);

	rxlen = mtk_cec_rx_data_len(cec);
	received_msg->msg->len = rxlen + CEC_HEADER_BLOCK_SIZE;

	for (i = 0; i < rxlen; i++) {
		addr = (i / 4) * 4 + CEC2_RX_DATA0;
		byte_shift = (i % 4 * 8);
		received_msg->msg->msg[i+1] =
		(mtk_cec_read(cec, addr) & 0xff << byte_shift) >> byte_shift;
	}
	dev_dbg(cec->dev, "RECEIVED MESSAGE:\n");
	mtk_cec_print_cec_frame(received_msg);
	received_msg->status.rx_status = CEC_RX_COMPLETE;
	mtk_cec_rx_notify_data_taken(cec);
	schedule_work(&cec->cec_rx_work);

}

static void mtk_cec_rx_event_handler(struct mtk_cec *cec)
{
	struct cec_frame *received_msg = &cec->received;

	received_msg->msg = &cec->rx_msg;

	if (mtk_cec_rx_data_arrived(cec) || mtk_cec_rx_buffer_ready(cec)) {
		dev_dbg(cec->dev, "RX:D\n");
		mtk_cec_receiving_msg(cec);
	}
	if (mtk_cec_rx_buffer_full(cec)) {
		received_msg->status.rx_status = CEC_RX_FAIL;
		dev_dbg(cec->dev, "[mtk_cec]Overflow\n");
	}

	mtk_cec_rx_int_all_clr(cec);
}

static void mtk_cec_tx_event_handler(struct mtk_cec *cec)
{
	mtk_cec_tx_detect_startbit(cec);

	if (mtk_cec_tx_fail_longlow(cec)) {
		mtk_cec_tx_fail_longlow_int_en(cec, 0);
		mtk_cec_tx_fail_longlow_intclr(cec);
		mtk_cec_tx_reset(cec);
		cec->transmitting.status.tx_status = CEC_TX_FAIL_LOW;
		schedule_work(&cec->cec_tx_work);
		dev_dbg(cec->dev, "CEC Direct/Brdcst msg fail:long low");
	}

	if (mtk_cec_tx_fail_retransmit_intsta(cec)) {
		cec->transmitting.status.tx_status = CEC_TX_FAIL_RETR;

		/*broadcast msg*/
		if ((cec->transmitting.msg->msg[0] & 0x0f) ==
			CEC_LOG_ADDR_BROADCAST) {
			if (mtk_cec_tx_fail_h_ack(cec) ||
				mtk_cec_tx_fail_data_ack(cec)) {
				mtk_cec_tx_fail_h_ack_intclr(cec);
				mtk_cec_tx_fail_data_ack_intclr(cec);
				dev_dbg(cec->dev, "broardcast NACK:done\n");
				cec->transmitting.status.tx_status = CEC_TX_COMPLETE;
			} else {
				dev_dbg(cec->dev, "broardcast ACK:fail\n");
				cec->transmitting.status.tx_status = CEC_TX_FAIL;
			}
			schedule_work(&cec->cec_tx_work);
		} else { /*direct msg*/
			if (mtk_cec_tx_fail_h_ack(cec)) {
				mtk_cec_tx_fail_h_ack_intclr(cec);
				cec->transmitting.status.tx_status = CEC_TX_FAIL_HNAK;
				dev_dbg(cec->dev, "err:Header NACK\n");
			}
			if (mtk_cec_tx_fail_data_ack(cec)) {
				mtk_cec_tx_fail_data_ack_intclr(cec);
				cec->transmitting.status.tx_status = CEC_TX_FAIL_DNAK;
				dev_dbg(cec->dev, "err:Data NACK\n");
			}
			if (mtk_cec_tx_fail_data(cec)) {
				mtk_cec_tx_fail_data_intclr(cec);
				cec->transmitting.status.tx_status = CEC_TX_FAIL_DATA;
				dev_dbg(cec->dev, "err:Data\n");
			}
			if (mtk_cec_tx_fail_header(cec)) {
				mtk_cec_tx_fail_header_intclr(cec);
				cec->transmitting.status.tx_status = CEC_TX_FAIL_HEAD;
				dev_dbg(cec->dev, "err:Header\n");
			}
			if (mtk_cec_tx_fail_src(cec)) {
				mtk_cec_tx_fail_src_intclr(cec);
				cec->transmitting.status.tx_status = CEC_TX_FAIL_SRC;
				dev_dbg(cec->dev, "err:Source\n");
			}
			schedule_work(&cec->cec_tx_work);
		}

		dev_dbg(cec->dev, "err:Retransmit\n");
		mtk_cec_tx_fail_retransmit_intclr(cec);
	}

	if (mtk_cec_tx_finish(cec)) {
		mtk_cec_tx_finish_clr(cec);
		mtk_cec_tx_int_disable_all(cec);
		mtk_cec_tx_int_clear_all(cec);
		if ((cec->transmitting.msg->msg[0] & 0x0f) ==
			CEC_LOG_ADDR_BROADCAST)
			dev_dbg(cec->dev, "CEC Brdcst message success\n");
		else
			dev_dbg(cec->dev, "CEC Direct message success\n");

		cec->transmitting.status.tx_status = CEC_TX_COMPLETE;
		schedule_work(&cec->cec_tx_work);
	}
}

static irqreturn_t mtk_cec_isr_thread(int irq, void *arg)
{
	struct mtk_cec *cec = arg;

	dev_dbg(cec->dev, "status=0x%08x,en=0x%08x,irq=0x%08x\n",
		mtk_cec_read(cec, CEC2_INT_STA),
		mtk_cec_read(cec, CEC2_INT_EN),
		(mtk_cec_read(cec, CEC2_INT_STA) &
		mtk_cec_read(cec, CEC2_INT_EN)));

	mtk_cec_rx_event_handler(cec);
	mtk_cec_tx_event_handler(cec);

	return IRQ_HANDLED;
}

static const struct cec_adap_ops mtk_hdmi_cec_adap_ops = {
	.adap_enable = mtk_cec_adap_enable,
	.adap_log_addr = mtk_cec_adap_log_addr,
	.adap_transmit = mtk_cec_adap_transmit,
};

static int mtk_get_cec_clk(struct mtk_cec *cec,
				struct device_node *np)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_cec_clk_names); i++) {
		cec->clk[i] = of_clk_get_by_name(np,
			mtk_cec_clk_names[i]);

		if (IS_ERR(cec->clk[i]))
			return PTR_ERR(cec->clk[i]);
	}

	return 0;
}

static int mtk_cec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_cec *cec;
	struct resource *res;
	struct device_node *np;
	struct platform_device *hdmi_dev;
	int ret;

	cec = devm_kzalloc(dev, sizeof(*cec), GFP_KERNEL);
	if (!cec)
		return -ENOMEM;

	cec->dev = dev;

	np = of_parse_phandle(pdev->dev.of_node, "hdmi", 0);
	if (!np) {
		dev_err(dev, "Failed to find hdmi node in CEC node\n");
		return -ENODEV;
	}
	hdmi_dev = of_find_device_by_node(np);
	if (!hdmi_dev) {
		dev_err(dev, "Failed to find hdmi platform device\n");
		return -EPROBE_DEFER;
	}

	platform_set_drvdata(pdev, cec);
	spin_lock_init(&cec->lock);

	ret = mtk_get_cec_clk(cec, dev->of_node);
	if (ret) {
		dev_err(dev, "Failed to get clocks: %d\n", ret);
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cec->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(cec->regs)) {
		ret = PTR_ERR(cec->regs);
		dev_err(dev, "Failed to ioremap cec: %d\n", ret);
		return ret;
	}

	cec->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);

	if (request_irq(cec->irq, mtk_cec_isr_thread,
		IRQF_TRIGGER_HIGH, "cecirq", cec) < 0)
		dev_err(dev, "request cec interrupt failed.\n");
	else
		dev_info(dev, "request cec interrupt success\n");

	cec->adap = cec_allocate_adapter(&mtk_hdmi_cec_adap_ops,
					cec, "mtk-hdmi-cec",
					CEC_CAP_TRANSMIT | CEC_CAP_PASSTHROUGH |
					CEC_CAP_LOG_ADDRS | CEC_CAP_PHYS_ADDR, 1);

	ret = PTR_ERR_OR_ZERO(cec->adap);
	if (ret < 0) {
		dev_err(dev, "Failed to allocate cec adapter %d\n", ret);
		return ret;
	}

	ret = cec_register_adapter(cec->adap, dev);
	if (ret) {
		dev_err(dev, "Fail to register cec adapter\n");
		cec_delete_adapter(cec->adap);
		return ret;
	}

	cec->notifier = cec_notifier_cec_adap_register(&hdmi_dev->dev, NULL, cec->adap);
	if (!cec->notifier)
		return -ENOMEM;

	INIT_WORK(&cec->cec_tx_work, mtk_cec_tx_work_handle);
	INIT_WORK(&cec->cec_rx_work, mtk_cec_rx_work_handle);

	cec->cec_enabled = false;

	return 0;
}

static int mtk_cec_remove(struct platform_device *pdev)
{
	int i;
	struct mtk_cec *cec = platform_get_drvdata(pdev);

	cec_unregister_adapter(cec->adap);
	cec_notifier_cec_adap_unregister(cec->notifier, cec->adap);

	dev_dbg(cec->dev, "cec free irq\n");
	free_irq(cec->irq, NULL);

	for (i = 0; i < ARRAY_SIZE(mtk_cec_clk_names); i++)
		clk_disable_unprepare(cec->clk[i]);
	return 0;
}

struct mtk_hdmi_cec_data {
	bool support_cec;
};

static const struct mtk_hdmi_cec_data cec2_data = {
	.support_cec = true,
};

static const struct of_device_id mtk_cec_of_ids[] = {
	{ .compatible = "mediatek,cec2",
	  .data = &cec2_data},
	{},
};

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int mtk_cec_suspend(struct device *dev)
{
	struct mtk_cec *cec = dev_get_drvdata(dev);
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_CEC_SUSPEND_LOW_POWER)
	if (cec->cec_enabled == true) {
		mtk_cec_clk_enable(cec, false);
		cec->cec_enabled = false;
	}
	dev_dbg(cec->dev, "cec low power mode\n");
#else
	dev_dbg(cec->dev, "cec no low power mode\n");
#endif
	dev_dbg(cec->dev, "cec suspend success!\n");
	return 0;
}

static int mtk_cec_resume(struct device *dev)
{
	struct mtk_cec *cec = dev_get_drvdata(dev);
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_CEC_SUSPEND_LOW_POWER)
	if (cec->cec_enabled == false) {
		mtk_cec_clk_enable(cec, true);
		cec->cec_enabled = true;
	}
	dev_dbg(cec->dev, "cec enable clock\n");
#endif
	dev_dbg(cec->dev, "cec resume success!\n");
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_cec_pm_ops, mtk_cec_suspend, mtk_cec_resume);

struct platform_driver mtk_cec2_driver = {
	.probe = mtk_cec_probe,
	.remove = mtk_cec_remove,
	.driver = {
		.name = "mediatek-cec2",
		.of_match_table = mtk_cec_of_ids,
		.pm = &mtk_cec_pm_ops,
	},
};
MODULE_DEVICE_TABLE(of, mtk_cec_of_ids);

module_platform_driver(mtk_cec2_driver);
MODULE_AUTHOR("Tommy Chen <tommyyl.chen@mediatek.com>");
MODULE_AUTHOR("Can Zeng <can.zeng@mediatek.com>");
MODULE_DESCRIPTION("MediaTek HDMI CEC Driver");
MODULE_LICENSE("GPL v2");
