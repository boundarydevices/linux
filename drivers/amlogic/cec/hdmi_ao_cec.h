/*
 * drivers/amlogic/cec/hdmi_ao_cec.h
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

#ifndef __AO_CEC_H__
#define __AO_CEC_H__


#define CEC_DRIVER_VERSION	"Ver 2018/12/11\n"

#define CEC_FRAME_DELAY		msecs_to_jiffies(400)
#define CEC_DEV_NAME		"cec"

#define CEC_EARLY_SUSPEND	(1 << 0)
#define CEC_DEEP_SUSPEND	(1 << 1)
#define CEC_PHY_PORT_NUM		4
#define HR_DELAY(n)		(ktime_set(0, n * 1000 * 1000))

enum cecbver {
	/*first version*/
	CECB_VER_0 = 0,
	/*ee to ao */
	CECB_VER_1 = 1,
	/*
	 * 1.fix bug: cts 7-1
	 * 2.fix bug: Do not signal initiator error, when it's
	 *   myself who pulled down the line when functioning as a follower
	 * 3.fix bug: Receive messages are ignored and not acknowledge
	 * 4.add status reg
	 */
	CECB_VER_2 = 2,
};


#define L_1		1
#define L_2		2
#define L_3		3

#define CEC_A	0
#define CEC_B	1

/*
#define CEC_FUNC_MASK			0
#define ONE_TOUCH_PLAY_MASK		1
#define ONE_TOUCH_STANDBY_MASK	2
#define AUTO_POWER_ON_MASK		3
*/
#define CEC_FUNC_CFG_CEC_ON			0x01
#define CEC_FUNC_CFG_OTP_ON			0x02
#define CEC_FUNC_CFG_AUTO_STANDBY	0x04
#define CEC_FUNC_CFG_AUTO_POWER_ON	0x08
#define CEC_FUNC_CFG_ALL			0x2f
#define CEC_FUNC_CFG_NONE			0x0

/*#define AO_BASE				0xc8100000*/

#define AO_GPIO_I			((0x0A << 2))
#define PREG_PAD_GPIO3_I	(0x01b << 2)


#define AO_CEC_GEN_CNTL			((0x40 << 2))
#define AO_CEC_RW_REG			((0x41 << 2))
#define AO_CEC_INTR_MASKN		((0x42 << 2))
#define AO_CEC_INTR_CLR			((0x43 << 2))
#define AO_CEC_INTR_STAT		((0x44 << 2))

#define AO_RTI_PWR_CNTL_REG0		((0x04 << 2))
#define AO_CRT_CLK_CNTL1		((0x1a << 2))
#define AO_RTC_ALT_CLK_CNTL0		((0x25 << 2))
#define AO_RTC_ALT_CLK_CNTL1		((0x26 << 2))

/* for TXLX, same as AO_RTC_ALT_CLK_CNTLx */
#define AO_CEC_CLK_CNTL_REG0		((0x1d << 2))
#define AO_CEC_CLK_CNTL_REG1		((0x1e << 2))

#define AO_RTI_STATUS_REG1		((0x01 << 2))
#define AO_DEBUG_REG0			((0x28 << 2))
#define AO_DEBUG_REG1			((0x29 << 2))
#define AO_DEBUG_REG2			((0x2a << 2))
#define AO_DEBUG_REG3			((0x2b << 2))
/* for new add after g12a/b ...*/
#define AO_CEC_STICKY_DATA0			((0xca << 2))
#define AO_CEC_STICKY_DATA1			((0xcb << 2))
#define AO_CEC_STICKY_DATA2			((0xcc << 2))
#define AO_CEC_STICKY_DATA3			((0xcd << 2))
#define AO_CEC_STICKY_DATA4			((0xce << 2))
#define AO_CEC_STICKY_DATA5			((0xcf << 2))
#define AO_CEC_STICKY_DATA6			((0xd0 << 2))
#define AO_CEC_STICKY_DATA7			((0xd1 << 2))

/*
 * AOCEC_B register
 */
#define AO_CECB_CLK_CNTL_REG0		((0xa0 << 2))
#define AO_CECB_CLK_CNTL_REG1		((0xa1 << 2))
#define AO_CECB_GEN_CNTL		((0xa2 << 2))
#define AO_CECB_RW_REG			((0xa3 << 2))
#define AO_CECB_INTR_MASKN		((0xa4 << 2))
#define AO_CECB_INTR_CLR		((0xa5 << 2))
#define AO_CECB_INTR_STAT		((0xa6 << 2))

/*
 * AOCEC_A internal register
 * read/write tx register list
 */
#define CEC_TX_MSG_0_HEADER        0x00
#define CEC_TX_MSG_1_OPCODE        0x01
#define CEC_TX_MSG_2_OP1           0x02
#define CEC_TX_MSG_3_OP2           0x03
#define CEC_TX_MSG_4_OP3           0x04
#define CEC_TX_MSG_5_OP4           0x05
#define CEC_TX_MSG_6_OP5           0x06
#define CEC_TX_MSG_7_OP6           0x07
#define CEC_TX_MSG_8_OP7           0x08
#define CEC_TX_MSG_9_OP8           0x09
#define CEC_TX_MSG_A_OP9           0x0A
#define CEC_TX_MSG_B_OP10          0x0B
#define CEC_TX_MSG_C_OP11          0x0C
#define CEC_TX_MSG_D_OP12          0x0D
#define CEC_TX_MSG_E_OP13          0x0E
#define CEC_TX_MSG_F_OP14          0x0F
#define CEC_TX_MSG_LENGTH          0x10
#define CEC_TX_MSG_CMD             0x11
#define CEC_TX_WRITE_BUF           0x12
#define CEC_TX_CLEAR_BUF           0x13
#define CEC_RX_MSG_CMD             0x14
#define CEC_RX_CLEAR_BUF           0x15
#define CEC_LOGICAL_ADDR0          0x16
#define CEC_LOGICAL_ADDR1          0x17
#define CEC_LOGICAL_ADDR2          0x18
#define CEC_LOGICAL_ADDR3          0x19
#define CEC_LOGICAL_ADDR4          0x1A
#define CEC_CLOCK_DIV_H            0x1B
#define CEC_CLOCK_DIV_L            0x1C

/*
 * AOCEC_A internal register
 * The following registers are for fine tuning CEC bit timing parameters.
 * They are only valid in AO CEC, NOT valid in HDMITX CEC.
 * The AO CEC's timing parameters are already set default to work with
 * 32768Hz clock, so hopefully SW never need to program these registers.
 * The timing registers are made programmable just in case.
 */
#define AO_CEC_QUIESCENT_25MS_BIT7_0            0x20
#define AO_CEC_QUIESCENT_25MS_BIT11_8           0x21
#define AO_CEC_STARTBITMINL2H_3MS5_BIT7_0       0x22
#define AO_CEC_STARTBITMINL2H_3MS5_BIT8         0x23
#define AO_CEC_STARTBITMAXL2H_3MS9_BIT7_0       0x24
#define AO_CEC_STARTBITMAXL2H_3MS9_BIT8         0x25
#define AO_CEC_STARTBITMINH_0MS6_BIT7_0         0x26
#define AO_CEC_STARTBITMINH_0MS6_BIT8           0x27
#define AO_CEC_STARTBITMAXH_1MS0_BIT7_0         0x28
#define AO_CEC_STARTBITMAXH_1MS0_BIT8           0x29
#define AO_CEC_STARTBITMINTOTAL_4MS3_BIT7_0     0x2A
#define AO_CEC_STARTBITMINTOTAL_4MS3_BIT9_8     0x2B
#define AO_CEC_STARTBITMAXTOTAL_4MS7_BIT7_0     0x2C
#define AO_CEC_STARTBITMAXTOTAL_4MS7_BIT9_8     0x2D
#define AO_CEC_LOGIC1MINL2H_0MS4_BIT7_0         0x2E
#define AO_CEC_LOGIC1MINL2H_0MS4_BIT8           0x2F
#define AO_CEC_LOGIC1MAXL2H_0MS8_BIT7_0         0x30
#define AO_CEC_LOGIC1MAXL2H_0MS8_BIT8           0x31
#define AO_CEC_LOGIC0MINL2H_1MS3_BIT7_0         0x32
#define AO_CEC_LOGIC0MINL2H_1MS3_BIT8           0x33
#define AO_CEC_LOGIC0MAXL2H_1MS7_BIT7_0         0x34
#define AO_CEC_LOGIC0MAXL2H_1MS7_BIT8           0x35
#define AO_CEC_LOGICMINTOTAL_2MS05_BIT7_0       0x36
#define AO_CEC_LOGICMINTOTAL_2MS05_BIT9_8       0x37
#define AO_CEC_LOGICMAXHIGH_2MS8_BIT7_0         0x38
#define AO_CEC_LOGICMAXHIGH_2MS8_BIT8           0x39
#define AO_CEC_LOGICERRLOW_3MS4_BIT7_0          0x3A
#define AO_CEC_LOGICERRLOW_3MS4_BIT8            0x3B
#define AO_CEC_NOMSMPPOINT_1MS05                0x3C
#define AO_CEC_DELCNTR_LOGICERR                 0x3E
#define AO_CEC_TXTIME_17MS_BIT7_0               0x40
#define AO_CEC_TXTIME_17MS_BIT10_8              0x41
#define AO_CEC_TXTIME_2BIT_BIT7_0               0x42
#define AO_CEC_TXTIME_2BIT_BIT10_8              0x43
#define AO_CEC_TXTIME_4BIT_BIT7_0               0x44
#define AO_CEC_TXTIME_4BIT_BIT10_8              0x45
#define AO_CEC_STARTBITNOML2H_3MS7_BIT7_0       0x46
#define AO_CEC_STARTBITNOML2H_3MS7_BIT8         0x47
#define AO_CEC_STARTBITNOMH_0MS8_BIT7_0         0x48
#define AO_CEC_STARTBITNOMH_0MS8_BIT8           0x49
#define AO_CEC_LOGIC1NOML2H_0MS6_BIT7_0         0x4A
#define AO_CEC_LOGIC1NOML2H_0MS6_BIT8           0x4B
#define AO_CEC_LOGIC0NOML2H_1MS5_BIT7_0         0x4C
#define AO_CEC_LOGIC0NOML2H_1MS5_BIT8           0x4D
#define AO_CEC_LOGIC1NOMH_1MS8_BIT7_0           0x4E
#define AO_CEC_LOGIC1NOMH_1MS8_BIT8             0x4F
#define AO_CEC_LOGIC0NOMH_0MS9_BIT7_0           0x50
#define AO_CEC_LOGIC0NOMH_0MS9_BIT8             0x51
#define AO_CEC_LOGICERRLOW_3MS6_BIT7_0          0x52
#define AO_CEC_LOGICERRLOW_3MS6_BIT8            0x53
#define AO_CEC_CHKCONTENTION_0MS1               0x54
#define AO_CEC_PREPARENXTBIT_0MS05_BIT7_0       0x56
#define AO_CEC_PREPARENXTBIT_0MS05_BIT8         0x57
#define AO_CEC_NOMSMPACKPOINT_0MS45             0x58
#define AO_CEC_ACK0NOML2H_1MS5_BIT7_0           0x5A
#define AO_CEC_ACK0NOML2H_1MS5_BIT8             0x5B
#define AO_CEC_BUGFIX_DISABLE_0                 0x60
#define AO_CEC_BUGFIX_DISABLE_1                 0x61

/*
 * AOCEC_A internal register
 * read only register list
 */
#define CEC_RX_MSG_0_HEADER        0x80
#define CEC_RX_MSG_1_OPCODE        0x81
#define CEC_RX_MSG_2_OP1           0x82
#define CEC_RX_MSG_3_OP2           0x83
#define CEC_RX_MSG_4_OP3           0x84
#define CEC_RX_MSG_5_OP4           0x85
#define CEC_RX_MSG_6_OP5           0x86
#define CEC_RX_MSG_7_OP6           0x87
#define CEC_RX_MSG_8_OP7           0x88
#define CEC_RX_MSG_9_OP8           0x89
#define CEC_RX_MSG_A_OP9           0x8A
#define CEC_RX_MSG_B_OP10          0x8B
#define CEC_RX_MSG_C_OP11          0x8C
#define CEC_RX_MSG_D_OP12          0x8D
#define CEC_RX_MSG_E_OP13          0x8E
#define CEC_RX_MSG_F_OP14          0x8F
#define CEC_RX_MSG_LENGTH          0x90
#define CEC_RX_MSG_STATUS          0x91
#define CEC_RX_NUM_MSG             0x92
#define CEC_TX_MSG_STATUS          0x93
#define CEC_TX_NUM_MSG             0x94
/*
 * AOCEC_A internal register
 * read only (tl1 later)
 */
#define CEC_STAT_0_0				0xA0
#define CEC_STAT_0_1				0xA1
#define CEC_STAT_0_2				0xA2
#define CEC_STAT_0_3				0xA3
#define CEC_STAT_1_0				0xA4
#define CEC_STAT_1_1				0xA5
#define CEC_STAT_1_2				0xA6

/* tx_msg_cmd definition */
#define TX_NO_OP                0  /* No transaction */
#define TX_REQ_CURRENT          1  /* Transmit earliest message in buffer */
#define TX_ABORT                2  /* Abort transmitting earliest message */
/* Overwrite earliest message in buffer and transmit next message */
#define TX_REQ_NEXT             3

/* tx_msg_status definition */
#define TX_IDLE                 0  /* No transaction */
#define TX_BUSY                 1  /* Transmitter is busy */
/* Message has been successfully transmitted */
#define TX_DONE                 2
#define TX_ERROR                3  /* Message has been transmitted with error */

/* rx_msg_cmd */
#define RX_NO_OP                0  /* No transaction */
#define RX_ACK_CURRENT          1  /* Read earliest message in buffer */
#define RX_DISABLE              2  /* Disable receiving latest message */
/* Clear earliest message from buffer and read next message */
#define RX_ACK_NEXT             3

/* rx_msg_status */
#define RX_IDLE                 0  /* No transaction */
#define RX_BUSY                 1  /* Receiver is busy */
#define RX_DONE                 2  /* Message has been received successfully */
#define RX_ERROR                3  /* Message has been received with error */

#define TOP_HPD_PWR5V           0x002
#define TOP_ARCTX_CNTL          0x010
#define TOP_CLK_CNTL			0x001
#define TOP_EDID_GEN_CNTL		0x004
#define TOP_EDID_ADDR_CEC		0x005

/** Register address: audio clock interrupt clear enable */
#define DWC_AUD_CEC_IEN_CLR		(0xF90UL)
/** Register address: audio clock interrupt set enable */
#define DWC_AUD_CEC_IEN_SET		(0xF94UL)
/** Register address: audio clock interrupt status */
#define DWC_AUD_CEC_ISTS		(0xF98UL)
/** Register address: audio clock interrupt enable */
#define DWC_AUD_CEC_IEN			(0xF9CUL)
/** Register address: audio clock interrupt clear status */
#define DWC_AUD_CEC_ICLR		(0xFA0UL)
/** Register address: audio clock interrupt set status */
#define DWC_AUD_CEC_ISET		(0xFA4UL)
/** Register address: DMI disable interface */
#define DWC_DMI_DISABLE_IF		(0xFF4UL)

/*
 * AOCEC_B internal register
 * for EE CEC
 */
#define DWC_CEC_CTRL                     0x1F00
#define DWC_CEC_CTRL2                    0x1F04/*tl1 later*/
#define DWC_CEC_MASK                     0x1F08
#define DWC_CEC_POLARITY                 0x1F0C
#define DWC_CEC_INT                      0x1F10
#define DWC_CEC_ADDR_L                   0x1F14
#define DWC_CEC_ADDR_H                   0x1F18
#define DWC_CEC_TX_CNT                   0x1F1C
#define DWC_CEC_RX_CNT                   0x1F20
#define DWC_CEC_STAT0                    0x1F24/*tl1 later*/
#define DWC_CEC_TX_DATA0                 0x1F40
#define DWC_CEC_TX_DATA1                 0x1F44
#define DWC_CEC_TX_DATA2                 0x1F48
#define DWC_CEC_TX_DATA3                 0x1F4C
#define DWC_CEC_TX_DATA4                 0x1F50
#define DWC_CEC_TX_DATA5                 0x1F54
#define DWC_CEC_TX_DATA6                 0x1F58
#define DWC_CEC_TX_DATA7                 0x1F5C
#define DWC_CEC_TX_DATA8                 0x1F60
#define DWC_CEC_TX_DATA9                 0x1F64
#define DWC_CEC_TX_DATA10                0x1F68
#define DWC_CEC_TX_DATA11                0x1F6C
#define DWC_CEC_TX_DATA12                0x1F70
#define DWC_CEC_TX_DATA13                0x1F74
#define DWC_CEC_TX_DATA14                0x1F78
#define DWC_CEC_TX_DATA15                0x1F7C
#define DWC_CEC_RX_DATA0                 0x1F80
#define DWC_CEC_RX_DATA1                 0x1F84
#define DWC_CEC_RX_DATA2                 0x1F88
#define DWC_CEC_RX_DATA3                 0x1F8C
#define DWC_CEC_RX_DATA4                 0x1F90
#define DWC_CEC_RX_DATA5                 0x1F94
#define DWC_CEC_RX_DATA6                 0x1F98
#define DWC_CEC_RX_DATA7                 0x1F9C
#define DWC_CEC_RX_DATA8                 0x1FA0
#define DWC_CEC_RX_DATA9                 0x1FA4
#define DWC_CEC_RX_DATA10                0x1FA8
#define DWC_CEC_RX_DATA11                0x1FAC
#define DWC_CEC_RX_DATA12                0x1FB0
#define DWC_CEC_RX_DATA13                0x1FB4
#define DWC_CEC_RX_DATA14                0x1FB8
#define DWC_CEC_RX_DATA15                0x1FBC
#define DWC_CEC_LOCK                     0x1FC0
#define DWC_CEC_WKUPCTRL                 0x1FC4

/*
 * AOCEC_B internal register
 * for EE CEC
 */
/*
#define AO_CECB_CTRL_ADDR                0x00
#define AO_CECB_CTRL2_ADDR               0x01
#define AO_CECB_INTR_MASK_ADDR           0x02
#define AO_CECB_LADD_LOW_ADDR            0x05
#define AO_CECB_LADD_HIGH_ADDR           0x06
#define AO_CECB_TX_CNT_ADDR              0x07
#define AO_CECB_RX_CNT_ADDR              0x08
#define AO_CECB_STAT0_ADDR               0x09
#define AO_CECB_TX_DATA00_ADDR           0x10
#define AO_CECB_TX_DATA01_ADDR           0x11
#define AO_CECB_TX_DATA02_ADDR           0x12
#define AO_CECB_TX_DATA03_ADDR           0x13
#define AO_CECB_TX_DATA04_ADDR           0x14
#define AO_CECB_TX_DATA05_ADDR           0x15
#define AO_CECB_TX_DATA06_ADDR           0x16
#define AO_CECB_TX_DATA07_ADDR           0x17
#define AO_CECB_TX_DATA08_ADDR           0x18
#define AO_CECB_TX_DATA09_ADDR           0x19
#define AO_CECB_TX_DATA10_ADDR           0x1A
#define AO_CECB_TX_DATA11_ADDR           0x1B
#define AO_CECB_TX_DATA12_ADDR           0x1C
#define AO_CECB_TX_DATA13_ADDR           0x1D
#define AO_CECB_TX_DATA14_ADDR           0x1E
#define AO_CECB_TX_DATA15_ADDR           0x1F
#define AO_CECB_RX_DATA00_ADDR           0x20
#define AO_CECB_RX_DATA01_ADDR           0x21
#define AO_CECB_RX_DATA02_ADDR           0x22
#define AO_CECB_RX_DATA03_ADDR           0x23
#define AO_CECB_RX_DATA04_ADDR           0x24
#define AO_CECB_RX_DATA05_ADDR           0x25
#define AO_CECB_RX_DATA06_ADDR           0x26
#define AO_CECB_RX_DATA07_ADDR           0x27
#define AO_CECB_RX_DATA08_ADDR           0x28
#define AO_CECB_RX_DATA09_ADDR           0x29
#define AO_CECB_RX_DATA10_ADDR           0x2A
#define AO_CECB_RX_DATA11_ADDR           0x2B
#define AO_CECB_RX_DATA12_ADDR           0x2C
#define AO_CECB_RX_DATA13_ADDR           0x2D
#define AO_CECB_RX_DATA14_ADDR           0x2E
#define AO_CECB_RX_DATA15_ADDR           0x2F
#define AO_CECB_LOCK_BUF_ADDR            0x30
#define AO_CECB_WAKEUPCTRL_ADDR          0x31
*/


/*
 * AOCEC B CEC_STAT0
 */
enum {
	CECB_STAT0_S2P_IDLE = 0,
	CECB_STAT0_S2P_SBITLOWER = 1,
	CECB_STAT0_S2P_SBH = 2,
	CECB_STAT0_S2P_L1LOWER = 5,
	CECB_STAT0_S2P_SMP1 = 6,
	CECB_STAT0_S2P_SMP0 = 7,
	CECB_STAT0_S2P_L0H = 8,
	CECB_STAT0_S2P_ERRLMIN = 9,
	CECB_STAT0_S2P_ERRLMAX = 0xe,
};

enum {
	CECB_STAT0_P2S_TIDLE = 0,
	CECB_STAT0_P2S_SEND_SBIT = 1,
	CECB_STAT0_P2S_SEND_DBIT = 2,
	CECB_STAT0_P2S_SEND_EOM = 3,
	CECB_STAT0_P2S_SEND_ACK = 4,
	CECB_STAT0_P2S_FBACK_ACK = 5,
	CECB_STAT0_P2S_FBACK_RX_ERR = 6,
};


/* cec ip irq flags bit discription */
#define EECEC_IRQ_TX_DONE		(1 << 16)
#define EECEC_IRQ_RX_EOM		(1 << 17)
#define EECEC_IRQ_TX_NACK		(1 << 18)
#define EECEC_IRQ_TX_ARB_LOST		(1 << 19)
#define EECEC_IRQ_TX_ERR_INITIATOR	(1 << 20)
#define EECEC_IRQ_RX_ERR_FOLLOWER	(1 << 21)
#define EECEC_IRQ_RX_WAKEUP		(1 << 22)
#define EE_CEC_IRQ_EN_MASK		(0x3f << 16)

/* cec irq bit flags for AO_CEC_B */
#define CECB_IRQ_TX_DONE		(1 << 0)
#define CECB_IRQ_RX_EOM			(1 << 1)
#define CECB_IRQ_TX_NACK		(1 << 2)
#define CECB_IRQ_TX_ARB_LOST		(1 << 3)
#define CECB_IRQ_TX_ERR_INITIATOR	(1 << 4)
#define CECB_IRQ_RX_ERR_FOLLOWER	(1 << 5)
#define CECB_IRQ_RX_WAKEUP		(1 << 6)
#define CECB_IRQ_EN_MASK		(0x3f << 0)

/* common mask */
#define CEC_IRQ_TX_DONE			(1 << (16 - shift))
#define CEC_IRQ_RX_EOM			(1 << (17 - shift))
#define CEC_IRQ_TX_NACK			(1 << (18 - shift))
#define CEC_IRQ_TX_ARB_LOST		(1 << (19 - shift))
#define CEC_IRQ_TX_ERR_INITIATOR	(1 << (20 - shift))
#define CEC_IRQ_RX_ERR_FOLLOWER		(1 << (21 - shift))
#define CEC_IRQ_RX_WAKEUP		(1 << (22 - shift))

/* wakeup mask */
#define WAKEUP_OP_86_EN			(1 << 7)
#define WAKEUP_OP_82_EN			(1 << 6)
#define WAKEUP_OP_70_EN			(1 << 5)
#define WAKEUP_OP_44_EN			(1 << 4)
#define WAKEUP_OP_42_EN			(1 << 3)
#define WAKEUP_OP_41_EN			(1 << 2)
#define WAKEUP_OP_0D_EN			(1 << 1)
#define WAKEUP_OP_04_EN			(1 << 0)
#define WAKEUP_DIS_MASK			0
#define WAKEUP_EN_MASK		(WAKEUP_OP_86_EN | \
							WAKEUP_OP_0D_EN | \
							WAKEUP_OP_04_EN)

#define EDID_CEC_ID_ADDR		0x00a100a0
#define EDID_AUTO_CEC_EN		0

#define HHI_32K_CLK_CNTL		(0x89 << 2)
#define HHI_HDMIRX_ARC_CNTL		(0xe8  << 2)


struct dbgflg {
	unsigned int hal_cmd_bypass:1;

};

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
extern unsigned long hdmirx_rd_top(unsigned long addr);
extern void hdmirx_wr_top(unsigned long addr, unsigned long data);
extern uint32_t hdmirx_rd_dwc(uint16_t addr);
extern void hdmirx_wr_dwc(uint16_t addr, uint32_t data);
extern unsigned int rd_reg_hhi(unsigned int offset);
extern void wr_reg_hhi(unsigned int offset, unsigned int val);

#else
static inline unsigned long hdmirx_rd_top(unsigned long addr)
{
	return 0;
}

static inline void hdmirx_wr_top(unsigned long addr, unsigned long data)
{
}

static inline uint32_t hdmirx_rd_dwc(uint16_t addr)
{
	return 0;
}
static inline void hdmirx_wr_dwc(uint16_t addr, uint32_t data)
{
}

unsigned int rd_reg_hhi(unsigned int offset)
{
	return 0;
}

void wr_reg_hhi(unsigned int offset, unsigned int val)
{
}

#endif

extern int hdmirx_get_connect_info(void);
int __attribute__((weak))hdmirx_get_connect_info(void)
{
	return 0;
}

#ifdef CONFIG_AMLOGIC_AO_CEC
unsigned int aocec_rd_reg(unsigned long addr);
void aocec_wr_reg(unsigned long addr, unsigned long data);
void cecb_irq_handle(void);
void cec_logicaddr_set(int l_add);
void cec_arbit_bit_time_set(unsigned int bit_set,
				unsigned int time_set, unsigned int flag);
void cec_irq_enable(bool enable);
void aocec_irq_enable(bool enable);
extern void dump_reg(void);
#endif
extern void cec_dump_info(void);
extern void cec_hw_reset(unsigned int cec_sel);
extern void cec_restore_logical_addr(unsigned int cec_sel,
	unsigned int addr_en);
extern void cec_logicaddr_add(unsigned int cec_sel, unsigned int l_add);
extern void cec_clear_all_logical_addr(unsigned int cec_sel);
#endif	/* __AO_CEC_H__ */
