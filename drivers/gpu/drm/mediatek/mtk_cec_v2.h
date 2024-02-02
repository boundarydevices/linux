/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_CEC_H
#define _MTK_CEC_H

#include <linux/cec.h>
#include <uapi/linux/cec.h>
#include <media/cec.h>
#include <media/cec-notifier.h>


/***** New CEC IP HW START*******/
#define CEC2_TR_CONFIG		0x00
#define CEC2_RX_CHK_DST			BIT(29)
#define CEC2_BYPASS			BIT(28)
#define CEC2_DEVICE_ADDR3		(0xf << 16)
#define CEC2_LA3_SHIFT			16
#define CEC2_DEVICE_ADDR2		(0xf << 20)
#define CEC2_LA2_SHIFT			20
#define CEC2_DEVICE_ADDR1		(0xf << 24)
#define CEC2_LA1_SHIFT			24
#define CEC2_RX_ACK_ERROR_BYPASS	BIT(13)
#define CEC2_RX_ACK_ERROR_HANDLE	BIT(12)
#define CEC2_RX_DATA_ACK		BIT(11)
#define CEC2_RX_HEADER_ACK		BIT(10)
#define CEC2_RX_HEADER_TRIG_INT_EN	BIT(9)
#define CEC2_RX_ACK_SEL			BIT(8)
#define CEC2_TX_RESET_WRITE		BIT(1)
#define CEC2_TX_RESET_READ		BIT(0)
#define CEC2_RX_RESET_WRITE		BIT(0)
#define CEC2_RX_RESET_READ		BIT(1)

#define CEC2_CKGEN		0x04
#define CEC2_CLK_TX_EN			BIT(20)
#define CEC2_CLK_32K_EN			BIT(19)
#define CEC2_CLK_27M_EN			BIT(18)
#define CEC2_CLK_SEL_DIV		BIT(17)
#define CEC2_CLK_PDN			BIT(16)
#define CEC2_CLK_DIV			(0xffff << 0)
#define CEC2_DIV_SEL_100K		0x82

#define CEC2_RX_TIMER_START_R	0x08
#define CEC2_RX_TIMER_START_R_MAX	(0x7ff << 16)
#define CEC2_RX_TIMER_START_R_MIN	(0x7ff)

#define CEC2_RX_TIMER_START_F	0x0c
#define CEC2_RX_TIMER_START_F_MAX	(0x7ff << 16)
#define CEC2_RX_TIMER_START_F_MIN	(0x7ff)

#define CEC2_RX_TIMER_DATA	0x10
#define CEC2_RX_TIMER_DATA_F_MAX	(0x7ff << 16)
#define CEC2_RX_TIMER_DATA_F_MIN	(0x7ff)

#define CEC2_RX_TIMER_ACK	0x14
#define CEC2_RX_TIMER_DATA_SAMPLE	(0x7ff << 16)
#define CEC2_RX_TIMER_ACK_R		0x7ff

#define CEC2_RX_TIMER_ERROR	0x18
#define CEC2_RX_TIMER_ERROR_D		(0x7ff << 16)

#define CEC2_TX_TIMER_START	0x1c
#define CEC2_TX_TIMER_START_F		(0x7ff << 16)
#define CEC2_TX_TIMER_START_F_SHIFT	16
#define CEC2_TX_TIMER_START_R		(0x7ff)

#define CEC2_TX_TIMER_DATA_R	0x20
#define CEC2_TX_TIMER_BIT1_R		(0x7ff << 16)
#define CEC2_TX_TIMER_BIT1_R_SHIFT	16
#define CEC2_TX_TIMER_BIT0_R		(0x7ff)

#define CEC2_TX_T_DATA_F	0x24
#define CEC2_TX_TIMER_DATA_BIT		(0x7ff << 16)
#define CEC2_TX_TIMER_DATA_BIT_SHIFT	16
#define CEC2_TX_TIMER_DATA_F		(0x7ff)

#define TX_TIMER_DATA_S		0x28
#define CEC2_TX_COMP_CNT		(0x7ff << 16)
#define CEC2_TX_TIMER_DATA_SAMPLE	(0x7ff)

#define CEC2_TX_ARB		0x2c
#define CEC2_TX_MAX_RETRANSMIT_NUM_ARB	GENMASK(29, 24)
#define CEC2_TX_MAX_RETRANSMIT_NUM_COL	GENMASK(23, 20)
#define CEC2_TX_MAX_RETRANSMIT_NUM_NAK	GENMASK(19, 16)
#define CEC2_TX_BCNT_RETRANSMIT		GENMASK(11, 8)
#define CEC2_TX_BCNT_NEW_MSG		GENMASK(7, 4)
#define CEC2_TX_BCNT_NEW_INIT		GENMASK(3, 0)

#define CEC2_TX_HEADER		0x30
#define CEC2_TX_READY			BIT(16)
#define CEC2_TX_SEND_CNT		(0xf << 12)
#define CEC2_TX_HEADER_EOM		BIT(8)
#define CEC2_TX_HEADER_DST		(0x0f)
#define CEC2_TX_HEADER_SRC		(0xf0)

#define CEC2_TX_DATA0		0x34
#define CEC2_TX_DATA_B3			(0xff << 24)
#define CEC2_TX_DATA_B2			(0xff << 16)
#define CEC2_TX_DATA_B1			(0xff << 8)
#define CEC2_TX_DATA_B0			(0xff)

#define CEC2_TX_DATA1		0x38
#define CEC2_TX_DATA_B7			(0xff << 24)
#define CEC2_TX_DATA_B6			(0xff << 16)
#define CEC2_TX_DATA_B5			(0xff << 8)
#define CEC2_TX_DATA_B4			(0xff)

#define CEC2_TX_DATA2		0x3c
#define CEC2_TX_DATA_B11		(0xff << 24)
#define CEC2_TX_DATA_B10		(0xff << 16)
#define CEC2_TX_DATA_B9			(0xff << 8)
#define CEC2_TX_DATA_B8			(0xff)

#define CEC2_TX_DATA3		0x40
#define CEC2_TX_DATA_B15		(0xff << 24)
#define CEC2_TX_DATA_B14		(0xff << 16)
#define CEC2_TX_DATA_B13		(0xff << 8)
#define CEC2_TX_DATA_B12		(0xff)

#define CEC2_TX_TIMER_ACK	0x44
#define CEC2_TX_TIMER_ACK_MAX		GENMASK(26, 16)
#define CEC2_TX_TIMER_ACK_MIN		GENMASK(15, 0)

#define CEC2_LINE_DET		0x48
#define CEC2_TIMER_LOW_LONG		GENMASK(31, 16)
#define CEC2_TIMER_HIGH_LONG		GENMASK(15, 0)

#define CEC2_RX_BUF_HEADER	0x4c
#define CEC2_RX_BUF_RISC_ACK		BIT(16)
#define CEC2_RX_BUF_CNT			(0xf << 12)
#define CEC2_RX_HEADER_EOM		BIT(8)
#define CEC2_RX_HEADER_SRC		(0xf << 4)
#define CEC2_RX_HEADER_DST		(0xf)

#define CEC2_RX_DATA0		0x50
#define CEC2_RX_DATA_B3			(0xff << 24)
#define CEC2_RX_DATA_B2			(0xff << 16)
#define CEC2_RX_DATA_B1			(0xff << 8)
#define CEC2_RX_DATA_B0			(0xff)

#define CEC2_RX_DATA1		0x54
#define CEC2_RX_DATA_B7			(0xff << 24)
#define CEC2_RX_DATA_B6			(0xff << 16)
#define CEC2_RX_DATA_B5			(0xff << 8)
#define CEC2_RX_DATA_B4			(0xff)

#define CEC2_RX_DATA2		0x58
#define CEC2_RX_DATA_B11		(0xff << 24)
#define CEC2_RX_DATA_B10		(0xff << 16)
#define CEC2_RX_DATA_B9			(0xff << 8)
#define CEC2_RX_DATA_B8			(0xff)

#define CEC2_RX_DATA3		0x5c
#define CEC2_RX_DATA_B15		(0xff << 24)
#define CEC2_RX_DATA_B14		(0xff << 16)
#define CEC2_RX_DATA_B13		(0xff << 8)
#define CEC2_RX_DATA_B12		(0xff)

#define CEC2_RX_STATUS		0x60
#define CEC2_CEC_INPUT			BIT(31)
#define CEC2_RX_FSM			GENMASK(22, 16)
#define CEC2_RX_BIT_COUNTER		(0xf << 12)
#define CEC2_RX_TIMER			(0x7ff)

#define CEC2_TX_STATUS		0x64
#define CEC2_TX_NUM_RETRANSMIT		GENMASK(31, 26)
#define CEC2_TX_FSM			GENMASK(24, 16)
#define CEC2_TX_BIT_COUNTER		(0xf << 12)
#define CEC2_TX_TIMER			(0x7ff)

#define CEC2_BACK		0x70
#define CEC2_RGS_BACK			GENMASK(31, 21)
#define CEC2_TX_LAST_ERR_STA		GENMASK(24, 16)
#define CEC2_REG_BACK			GENMASK(15, 0)

#define CEC2_INT_CLR		0x74
#define CEC2_INT_CLR_ALL		GENMASK(23, 0)
#define CEC2_RX_INT_ALL_CLR		GENMASK(23, 16)
#define CEC2_TX_INT_FAIL_CLR		(GENMASK(14, 9) | BIT(7))
#define CEC2_TX_INT_ALL_CLR		(CEC2_TX_INT_FAIL_CLR | BIT(15) | BIT(8))
#define CEC2_RX_FSM_CHG_INT_CLR		BIT(23)
#define CEC2_RX_ACK_FAIL_INT_CLR	BIT(22)
#define CEC2_RX_ERROR_HANDLING_INT_CLR	BIT(21)
#define CEC2_RX_BUF_FULL_INT_CLR	BIT(20)
#define CEC2_RX_STAGE_READY_INT_CLR	BIT(19)
#define CEC2_RX_DATA_RCVD_INT_CLR	BIT(18)
#define CEC2_RX_HEADER_RCVD_INT_CLR	BIT(17)
#define CEC2_TX_FSM_CHG_INT_CLR		BIT(15)
#define CEC2_TX_FAIL_DATA_ACK_INT_CLR	BIT(14)
#define CEC2_TX_FAIL_HEADER_ACK_INT_CLR	BIT(13)
#define CEC2_TX_FAIL_RETRANSMIT_INT_CLR	BIT(12)
#define CEC2_TX_FAIL_DATA_INT_CLR	BIT(11)
#define CEC2_TX_FAIL_HEADER_INT_CLR	BIT(10)
#define CEC2_TX_FAIL_SRC_INT_CLR	BIT(9)
#define CEC2_TX_DATA_FINISH_INT_CLR	BIT(8)
#define CEC2_LINE_lOW_LONG_INT_CLR	BIT(7)
#define CEC2_LINE_HIGH_LONG_INT_CLR	BIT(6)
#define CEC2_PORD_FAIL_INT_CLR		BIT(5)
#define CEC2_PORD_RISE_INT_CLR		BIT(4)
#define CEC2_HTPLG_FAIL_INT_CLR		BIT(3)
#define CEC2_HTPLG_RISE_INT_CLR		BIT(2)
#define CEC2_FAIL_INT_CLR		BIT(1)
#define CEC2_RISE_INT_CLR		BIT(0)

#define CEC2_INT_EN		0x78
#define CEC2_INT_ALL_EN			GENMASK(31, 0)
#define CEC2_RX_INT_ALL_EN		GENMASK(23, 16)
#define CEC2_TX_INT_FAIL_EN		(GENMASK(14, 9) | BIT(7))
#define CEC2_TX_INT_ALL_EN		(CEC2_TX_INT_FAIL_EN | BIT(15) | BIT(8))
#define CEC2_RX_FSM_CHG_INT_EN		BIT(23)
#define CEC2_RX_ACK_FAIL_INT_EN		BIT(22)
#define CEC2_RX_ERROR_HANDLING_INT_EN	BIT(21)
#define CEC2_RX_BUF_FULL_INT_EN		BIT(20)
#define CEC2_RX_STAGE_READY_INT_EN	BIT(19)
#define CEC2_RX_DATA_RCVD_INT_EN	BIT(18)
#define CEC2_RX_HEADER_RCVD_INT_EN	BIT(17)
#define CEC2_RX_BUF_READY_INT_EN	BIT(16)
#define CEC2_TX_FSM_CHG_INT_EN		BIT(15)
#define CEC2_TX_FAIL_DATA_ACK_INT_EN	BIT(14)
#define CEC2_TX_FAIL_HEADER_ACK_INT_EN	BIT(13)
#define CEC2_TX_FAIL_RETRANSMIT_INT_EN	BIT(12)
#define CEC2_TX_FAIL_DATA_INT_EN	BIT(11)
#define CEC2_TX_FAIL_HEADER_INT_EN	BIT(10)
#define CEC2_TX_FAIL_SRC_INT_EN		BIT(9)
#define CEC2_TX_DATA_FINISH_INT_EN	BIT(8)
#define CEC2_LINE_lOW_LONG_INT_EN	BIT(7)
#define CEC2_LINE_HIGH_LONG_INT_EN	BIT(6)
#define CEC2_PORD_FAIL_INT_EN		BIT(5)
#define CEC2_PORD_RISE_INT_EN		BIT(4)
#define CEC2_HTPLG_FAIL_INT_EN		BIT(3)
#define CEC2_HTPLG_RISE_INT_EN		BIT(2)
#define CEC2_FALL_INT_EN		BIT(1)
#define CEC2_RISE_INT_EN		BIT(0)

#define CEC2_INT_STA		0x7c
#define CEC2_TRX_INT_STA		GENMASK(23, 0)
#define CEC2_TX_INT_FAIL		(GENMASK(14, 9) | BIT(7))
#define CEC2_RX_FSM_CHG_INT_STA		BIT(23)
#define CEC2_RX_ACK_FAIL_INT_STA	BIT(22)
#define CEC2_RX_ERROR_HANDLING_INT_STA	BIT(21)
#define CEC2_RX_BUF_FULL_INT_STA	BIT(20)
#define CEC2_RX_STAGE_READY_INT_STA	BIT(19)
#define CEC2_RX_DATA_RCVD_INT_STA	BIT(18)
#define CEC2_RX_HEADER_RCVD_INT_STA	BIT(17)
#define CEC2_RX_BUF_READY_INT_STA	BIT(16)
#define CEC2_TX_FSM_CHG_INT_STA		BIT(15)
#define CEC2_TX_FAIL_DATA_ACK_INT_STA	BIT(14)
#define CEC2_TX_FAIL_HEADER_ACK_INT_STA	BIT(13)
#define CEC2_TX_FAIL_RETRANSMIT_INT_STA	BIT(12)
#define CEC2_TX_FAIL_DATA_INT_STA	BIT(11)
#define CEC2_TX_FAIL_HEADER_INT_STA	BIT(10)
#define CEC2_TX_FAIL_SRC_INT_STA	BIT(9)
#define CEC2_TX_DATA_FINISH_INT_STA	BIT(8)
#define CEC2_LINE_lOW_LONG_INT_STA	BIT(7)
#define CEC2_LINE_HIGH_LONG_INT_STA	BIT(6)
#define CEC2_PORD_FAIL_INT_STA		BIT(5)
#define CEC2_PORD_RISE_INT_STA		BIT(4)
#define CEC2_HTPLG_FAIL_INT_STA		BIT(3)
#define CEC2_HTPLG_RISE_INT_STA		BIT(2)
#define CEC2_FAlL_INT_STA		BIT(1)
#define CEC2_RISE_INT_STA		BIT(0)
/***** register map end*******/

#define CEC_HEADER_BLOCK_SIZE 1

enum mtk_cec_clk_id {
	MTK_CEC_66M_H,
	MTK_CEC_66M_B,
	MTK_HDMI_32K,
	MTK_HDMI_26M,
	MTK_CEC_CLK_COUNT,
};

enum cec_tx_status {
	CEC_TX_START,
	CEC_TX_Transmitting,
	CEC_TX_COMPLETE,
	CEC_TX_FAIL,
	CEC_TX_FAIL_DNAK,
	CEC_TX_FAIL_HNAK,
	CEC_TX_FAIL_RETR,
	CEC_TX_FAIL_DATA,
	CEC_TX_FAIL_HEAD,
	CEC_TX_FAIL_SRC,
	CEC_TX_FAIL_LOW,
	CEC_TX_STATUS_NUM,
};

enum cec_rx_status {
	CEC_RX_START,
	CEC_RX_Receiving,
	CEC_RX_COMPLETE,
	CEC_RX_FAIL,
	CEC_RX_STATUS_NUM,
};

struct cec_frame {
	struct cec_msg *msg;
	unsigned char retry_count;
	union {
		enum cec_tx_status tx_status;
		enum cec_rx_status rx_status;
	} status;
};

struct mtk_cec {
	void __iomem *regs;
	struct clk *clk[MTK_CEC_CLK_COUNT];
	int irq;
	struct device *hdmi_dev;
	struct device *dev;
	spinlock_t lock;
	struct cec_adapter *adap;
	struct cec_notifier	*notifier;
	struct cec_frame transmitting;
	struct cec_frame received;
	struct cec_msg rx_msg;	/* dynamic alloc or fixed memory?? */
	bool cec_enabled;
	struct work_struct cec_tx_work;
	struct work_struct cec_rx_work;
};

enum cec_inner_clock {
	CLK_27M_SRC = 0,
};

enum cec_la_num {
	DEVICE_ADDR_1 = 1,
	DEVICE_ADDR_2 = 2,
	DEVICE_ADDR_3 = 3,
};

struct cec_msg_header_block {
	unsigned char destination:4;
	unsigned char initiator:4;
};

#endif /* _MTK_CEC_H */
