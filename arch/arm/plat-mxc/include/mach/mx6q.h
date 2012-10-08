/*
 * Copyright 2011 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2011 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef __MACH_MX6Q_H__
#define __MACH_MX6Q_H__

#define MX6Q_IO_P2V(x)			IMX_IO_P2V(x)
#define MX6Q_IO_ADDRESS(x)		IOMEM(MX6Q_IO_P2V(x))

/*
 * The following are the blocks that need to be statically mapped.
 * For other blocks, the base address really should be retrieved from
 * device tree.
 */
#define MX6Q_SCU_BASE_ADDR		0x00a00000
#define MX6Q_SCU_SIZE			0x1000
#define MX6Q_CCM_BASE_ADDR		0x020c4000
#define MX6Q_CCM_SIZE			0x4000
#define MX6Q_ANATOP_BASE_ADDR		0x020c8000
#define MX6Q_ANATOP_SIZE		0x1000

#define MX6Q_UART1_BASE_ADDR		0x02020000
#define MX6Q_UART2_BASE_ADDR		0x021e8000
#define MX6Q_UART3_BASE_ADDR		0x021ec000
#define MX6Q_UART4_BASE_ADDR		0x021f0000
#define MX6Q_UART5_BASE_ADDR		0x021f4000

/*
 * MX6Q_UART_BASE_ADDR is put in the middle to force the expansion
 * of MX6Q_UART##n##_BASE_ADDR.
 */
#define MX6Q_UART_BASE_ADDR(n)	MX6Q_UART##n##_BASE_ADDR
#define MX6Q_UART_BASE(n)	MX6Q_UART_BASE_ADDR(n)
#define MX6Q_DEBUG_UART_BASE	MX6Q_UART_BASE(CONFIG_DEBUG_IMX6Q_UART_PORT)

#endif	/* __MACH_MX6Q_H__ */
