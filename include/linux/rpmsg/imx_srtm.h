/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2022-2023 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * @file linux/rpmsg/imx_srtm.h
 *
 * @brief Global header file for iMX SRTM (Simplified Real Time Message Application Protocol, base on rpmsg)
 *
 * @ingroup SRTM
 */
#ifndef __LINUX_RPMSG_IMX_SRTM_H__
#define __LINUX_RPMSG_IMX_SRTM_H__

/* Category define */
#define IMX_SRTM_CATEGORY_LIFECYCLE	(0x1)
#define IMX_SRTM_CATEGORY_PMIC		(0x2)
#define IMX_SRTM_CATEGORY_AUDIO		(0x3)
#define IMX_SRTM_CATEGORY_KEY		(0x4)
#define IMX_SRTM_CATEGORY_GPIO		(0x5)
#define IMX_SRTM_CATEGORY_RTC		(0x6)
#define IMX_SRTM_CATEGORY_SENSOR	(0x7)
#define IMX_SRTM_CATEGORY_AUTO		(0x8)
#define IMX_SRTM_CATEGORY_I2C		(0x9)
#define IMX_SRTM_CATEGORY_PWM		(0xA)
#define IMX_SRTM_CATEGORY_UART		(0xB)

/* srtm version */
#define IMX_SRTM_VER_UART (0x0001)

/* type */
#define IMX_SRTM_TYPE_REQUEST (0)
#define IMX_SRTM_TYPE_RESPONSE (1)
#define IMX_SRTM_TYPE_NOTIFY (2)

/* command */
#define IMX_SRTM_UART_COMMAND_SEND (1)
#define IMX_SRTM_UART_COMMAND_HELLO (2)

/* priority */
#define IMX_SRTM_UART_PRIORITY	(0x01)

/* flags */
#define IMX_SRTM_RPMSG_OVER_UART_FLAG (1 << 0)
#define IMX_SRTM_UART_SUPPORT_MULTI_UART_MSG_FLAG (1 << 1)
#define IMX_SRTM_UART_PORT_NUM_SHIFT (11U)
#define IMX_SRTM_UART_PORT_NUM_MASK (0x1F << 11U)
#define IMX_SRTM_UART_SPECIFY_PORT_NUM_SHIFT (10U)
#define IMX_SRTM_UART_SPECIFY_PORT_NUM_MASK (1 << IMX_SRTM_UART_SPECIFY_PORT_NUM_SHIFT)

struct imx_srtm_head {
	u8 cate;
	u8 major;
	u8 minor;
	u8 type;
	u8 cmd;
	u8 reserved[5];
} __packed;

#endif /* __LINUX_RPMSG_IMX_SRTM_H__ */
