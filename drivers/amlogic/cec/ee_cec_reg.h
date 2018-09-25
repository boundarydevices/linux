/*
 * drivers/amlogic/cec/ee_cec_reg.h
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

#ifndef __EE_CEC_REG_H__
#define __EE_CEC_REG_H__

#define TOP_CLK_CNTL			0x001
#define TOP_EDID_GEN_CNTL		0x004
#define TOP_EDID_ADDR_CEC		0x005
#define TOP_HPD_PWR5V			0x002
#define TOP_ARCTX_CNTL			0x010

/** Register address: audio clock interrupt clear enable */
#define DWC_AUD_CLK_IEN_CLR		(0xF90UL)
/** Register address: audio clock interrupt set enable */
#define DWC_AUD_CLK_IEN_SET		(0xF94UL)
/** Register address: audio clock interrupt status */
#define DWC_AUD_CLK_ISTS		(0xF98UL)
/** Register address: audio clock interrupt enable */
#define DWC_AUD_CLK_IEN			(0xF9CUL)
/** Register address: audio clock interrupt clear status */
#define DWC_AUD_CLK_ICLR		(0xFA0UL)
/** Register address: audio clock interrupt set status */
#define DWC_AUD_CLK_ISET		(0xFA4UL)
/** Register address: DMI disable interface */
#define DWC_DMI_DISABLE_IF		(0xFF4UL)

/*---- registers for EE CEC ----*/
#define DWC_CEC_CTRL                     0x1F00
#define DWC_CEC_CTRL2                    0x1F04/*tl1 later*/
#define DWC_CEC_MASK                     0x1F08
#define DWC_CEC_POLARITY                 0x1F0C
#define DWC_CEC_INT                      0x1F10
#define DWC_CEC_ADDR_L                   0x1F14
#define DWC_CEC_ADDR_H                   0x1F18
#define DWC_CEC_TX_CNT                   0x1F1C
#define DWC_CEC_RX_CNT                   0x1F20
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

/* cec ip irq flags bit discription */
#define CEC_IRQ_TX_DONE			(1 << 16)
#define CEC_IRQ_RX_EOM			(1 << 17)
#define CEC_IRQ_TX_NACK			(1 << 18)
#define CEC_IRQ_TX_ARB_LOST		(1 << 19)
#define CEC_IRQ_TX_ERR_INITIATOR	(1 << 20)
#define CEC_IRQ_RX_ERR_FOLLOWER		(1 << 21)
#define CEC_IRQ_RX_WAKEUP		(1 << 22)
#define EE_CEC_IRQ_EN_MASK		(0xf << 16)

#define EDID_CEC_ID_ADDR		0x00a100a0
#define EDID_AUTO_CEC_EN		0

#define HHI_32K_CLK_CNTL		(0x89 << 2)

#endif /* __EE_CEC_REG_H__ */
