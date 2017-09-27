/*
 * XRM117x tty serial driver - Copyright (C) 2015 Exar Corporation.
 * Author: Martin xu <martin.xu@exar.com>
 *
 *  Based on sc16is7xx.c, by Jon Ringle <jringle@gridpoint.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/uaccess.h>
#include <linux/irq.h>
#include "linux/version.h"


#ifdef CONFIG_GPIOLIB
#undef CONFIG_GPIOLIB
#endif

#ifndef PORT_SC16IS7XX   
#define PORT_SC16IS7XX   128
#endif

#define USE_SPI_MODE    1
//#define USE_OK335Dx_PLATFORM


#define XRM117XX_NAME			"xrm117x"
#define XRM117XX_NAME_SPI		"xrm117x_spi"

enum sc_reg {
	SC_LCR = 0,
	SC_RHR,
	SC_THR,
	SC_IER,
	SC_DLL,
	SC_DLH,
	SC_DLD,
	SC_IIR,
	SC_FCR,
	SC_MCR,
	SC_LSR,
	SC_TXLVL,
	SC_RXLVL,
	SC_EFCR,
	SC_MSR,
	SC_SPR,
	SC_TCR,
	SC_TLR,
	SC_EFR,
	SC_XON1,
	SC_XON2,
	SC_XOFF1,
	SC_XOFF2,
	SC_CHAN_REG_CNT,
/*
 *  keep IOSTATE before IODIR, so that it's write will occur before direction
 *  changes.
 */
	SC_IOSTATE_R = SC_CHAN_REG_CNT,
	SC_IOSTATE_W,
	SC_IODIR,
	SC_IOINTENA,
	SC_IOCONTROL,
	SC_REG_CNT,
};

/*
 * 7 6543  21 0
 * |   |   |  |
 * |   |   |  unused
 * |   |   00 channel A
 * |   |   01 channel B
 * |   |   1x reserved
 * | internal register select
 * unused
 */

#define MAKE_SUBADDRESS(port, reg) (unsigned char)(((reg & 0x0f) << 3) | (port << 1))

#define SP_0xBF		0		/* LCR==0xbf, LCR!=0xbf matters */
#define SP_DLAB		1		/* LCR[7] */
#define SP_MCR2		2		/* MCR[2] */

#define A_TYPEV(bit) (1 << ((bit) + 4))
#define A_TYPE(bit, val) ((0x10 | (val)) << ((bit) + 4))
#define _RO		0x1000		/* Read-only */
#define _WO		0x2000		/* Write-only */
#define _V		0x4000		/* Volatile */

static const unsigned short reg_map[] = {
/* always accessible */
[SC_LCR] =	3,
/* LCR != 0xBF, DLAB = 0 */
[SC_RHR] =	(0 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0) + _RO + _V),
[SC_THR] =	(0 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0) + _WO + _V),
[SC_IER] =	(1 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0)),
[SC_IIR] =	(2 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0) + _RO + _V),	/* ISR */
[SC_FCR] =	(2 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0) + _WO + _V),
[SC_TXLVL] =	(8 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0) + _RO + _V),
[SC_RXLVL] =	(9 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0) + _RO + _V),
[SC_IODIR] =	 (0x0A + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0)),
[SC_IOSTATE_R] = (0x0B + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0) + _RO + _V),
[SC_IOSTATE_W] = (0x0B + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0) + _WO),
[SC_IOINTENA] =  (0x0C + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0)),
[SC_IOCONTROL] = (0x0E + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0)),
[SC_EFCR] =	 (0x0F + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0)),

/* LCR != 0xBF, DLAB = 1 */
[SC_DLL] =	(0 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 1)),
[SC_DLH] =	(1 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 1)),
[SC_DLD] =	(2 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 1)),

/* LCR != 0xBF */
[SC_MCR] =	(4 + A_TYPE(SP_0xBF, 0)),
[SC_LSR] =	(5 + A_TYPE(SP_0xBF, 0) + _RO + _V),

/*  LCR != 0xBF,  EFR[4]=0 ||  MCR[2]=0 */
[SC_MSR] =	(6 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_MCR2, 0) + _RO + _V),
[SC_SPR] =	(7 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_MCR2, 0)),

/*  LCR != 0xBF,  EFR[4]=1 &&  MCR[2]=1 */
[SC_TCR] =	(6 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_MCR2, 1)),
[SC_TLR] =	(7 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_MCR2, 1)),

/* LCR == 0xBF */
[SC_EFR] =	(2 + A_TYPE(SP_0xBF, 1)),
[SC_XON1] =	(4 + A_TYPE(SP_0xBF, 1)),
[SC_XON2] =	(5 + A_TYPE(SP_0xBF, 1)),
[SC_XOFF1] =	(6 + A_TYPE(SP_0xBF, 1)),
[SC_XOFF2] =	(7 + A_TYPE(SP_0xBF, 1)),
};

/* XRM117XX register definitions */

/* IER register bits */
#define XRM117XX_IER_RDI_BIT		(1 << 0) /* Enable RX data interrupt */
#define XRM117XX_IER_THRI_BIT		(1 << 1) /* Enable TX holding register
						  * interrupt */
#define XRM117XX_IER_RLSI_BIT		(1 << 2) /* Enable RX line status
						  * interrupt */
#define XRM117XX_IER_MSI_BIT		(1 << 3) /* Enable Modem status
						  * interrupt */

/* IER register bits - write only if (EFR[4] == 1) */
#define XRM117XX_IER_SLEEP_BIT		(1 << 4) /* Enable Sleep mode */
#define XRM117XX_IER_XOFFI_BIT		(1 << 5) /* Enable Xoff interrupt */
#define XRM117XX_IER_RTSI_BIT		(1 << 6) /* Enable nRTS interrupt */
#define XRM117XX_IER_CTSI_BIT		(1 << 7) /* Enable nCTS interrupt */

/* FCR register bits */
#define XRM117XX_FCR_FIFO_BIT		(1 << 0) /* Enable FIFO */
#define XRM117XX_FCR_RXRESET_BIT	(1 << 1) /* Reset RX FIFO */
#define XRM117XX_FCR_TXRESET_BIT	(1 << 2) /* Reset TX FIFO */
#define XRM117XX_FCR_RXLVLL_BIT	(1 << 6) /* RX Trigger level LSB */
#define XRM117XX_FCR_RXLVLH_BIT	(1 << 7) /* RX Trigger level MSB */

/* FCR register bits - write only if (EFR[4] == 1) */
#define XRM117XX_FCR_TXLVLL_BIT	(1 << 4) /* TX Trigger level LSB */
#define XRM117XX_FCR_TXLVLH_BIT	(1 << 5) /* TX Trigger level MSB */

/* IIR register bits */
#define XRM117XX_IIR_NO_INT_BIT	(1 << 0) /* No interrupts pending */
#define XRM117XX_IIR_ID_MASK		0x3e     /* Mask for the interrupt ID */
#define XRM117XX_IIR_THRI_SRC		0x02     /* TX holding register empty */
#define XRM117XX_IIR_RDI_SRC		0x04     /* RX data interrupt */
#define XRM117XX_IIR_RLSE_SRC		0x06     /* RX line status error */
#define XRM117XX_IIR_RTOI_SRC		0x0c     /* RX time-out interrupt */
#define XRM117XX_IIR_MSI_SRC		0x00     /* Modem status interrupt
						  * - only on 75x/76x
						  */
#define XRM117XX_IIR_INPIN_SRC		0x30     /* Input pin change of state
						  * - only on 75x/76x
						  */
#define XRM117XX_IIR_XOFFI_SRC		0x10     /* Received Xoff */
#define XRM117XX_IIR_CTSRTS_SRC	0x20     /* nCTS,nRTS change of state
						  * from active (LOW)
						  * to inactive (HIGH)
						  */
/* LCR register bits */
#define XRM117XX_LCR_LENGTH0_BIT	(1 << 0) /* Word length bit 0 */
#define XRM117XX_LCR_LENGTH1_BIT	(1 << 1) /* Word length bit 1
						  *
						  * Word length bits table:
						  * 00 -> 5 bit words
						  * 01 -> 6 bit words
						  * 10 -> 7 bit words
						  * 11 -> 8 bit words
						  */
#define XRM117XX_LCR_STOPLEN_BIT	(1 << 2) /* STOP length bit
						  *
						  * STOP length bit table:
						  * 0 -> 1 stop bit
						  * 1 -> 1-1.5 stop bits if
						  *      word length is 5,
						  *      2 stop bits otherwise
						  */
#define XRM117XX_LCR_PARITY_BIT	(1 << 3) /* Parity bit enable */
#define XRM117XX_LCR_EVENPARITY_BIT	(1 << 4) /* Even parity bit enable */
#define XRM117XX_LCR_FORCEPARITY_BIT	(1 << 5) /* 9-bit multidrop parity */
#define XRM117XX_LCR_TXBREAK_BIT	(1 << 6) /* TX break enable */
#define XRM117XX_LCR_DLAB_BIT		(1 << 7) /* Divisor Latch enable */
#define XRM117XX_LCR_WORD_LEN_5	(0x00)
#define XRM117XX_LCR_WORD_LEN_6	(0x01)
#define XRM117XX_LCR_WORD_LEN_7	(0x02)
#define XRM117XX_LCR_WORD_LEN_8	(0x03)
#define XRM117XX_LCR_CONF_MODE_A	XRM117XX_LCR_DLAB_BIT /* Special
								* reg set */

/* MCR register bits */
#define XRM117XX_MCR_DTR_BIT		(1 << 0) /* DTR complement
						  * - only on 75x/76x
						  */
#define XRM117XX_MCR_RTS_BIT		(1 << 1) /* RTS complement */
#define XRM117XX_MCR_TCRTLR_BIT	(1 << 2) /* TCR/TLR register enable */
#define XRM117XX_MCR_LOOP_BIT		(1 << 4) /* Enable loopback test mode */
#define XRM117XX_MCR_XONANY_BIT	(1 << 5) /* Enable Xon Any
						  * - write enabled
						  * if (EFR[4] == 1)
						  */
#define XRM117XX_MCR_IRDA_BIT		(1 << 6) /* Enable IrDA mode
						  * - write enabled
						  * if (EFR[4] == 1)
						  */
#define XRM117XX_MCR_CLKSEL_BIT	(1 << 7) /* Divide clock by 4
						  * - write enabled
						  * if (EFR[4] == 1)
						  */

/* LSR register bits */
#define XRM117XX_LSR_DR_BIT		(1 << 0) /* Receiver data ready */
#define XRM117XX_LSR_OE_BIT		(1 << 1) /* Overrun Error */
#define XRM117XX_LSR_PE_BIT		(1 << 2) /* Parity Error */
#define XRM117XX_LSR_FE_BIT		(1 << 3) /* Frame Error */
#define XRM117XX_LSR_BI_BIT		(1 << 4) /* Break Interrupt */
#define XRM117XX_LSR_BRK_ERROR_MASK	0x1E     /* BI, FE, PE, OE bits */
#define XRM117XX_LSR_THRE_BIT		(1 << 5) /* TX holding register empty */
#define XRM117XX_LSR_TEMT_BIT		(1 << 6) /* Transmitter empty */
#define XRM117XX_LSR_FIFOE_BIT		(1 << 7) /* Fifo Error */

/* MSR register bits */
#define XRM117XX_MSR_DCTS_BIT		(1 << 0) /* Delta CTS Clear To Send */
#define XRM117XX_MSR_DDSR_BIT		(1 << 1) /* Delta DSR Data Set Ready
						  * or (IO4)
						  * - only on 75x/76x
						  */
#define XRM117XX_MSR_DRI_BIT		(1 << 2) /* Delta RI Ring Indicator
						  * or (IO7)
						  * - only on 75x/76x
						  */
#define XRM117XX_MSR_DCD_BIT		(1 << 3) /* Delta CD Carrier Detect
						  * or (IO6)
						  * - only on 75x/76x
						  */
#define XRM117XX_MSR_CTS_BIT		(1 << 0) /* CTS */
#define XRM117XX_MSR_DSR_BIT		(1 << 1) /* DSR (IO4)
						  * - only on 75x/76x
						  */
#define XRM117XX_MSR_RI_BIT		(1 << 2) /* RI (IO7)
						  * - only on 75x/76x
						  */
#define XRM117XX_MSR_CD_BIT		(1 << 3) /* CD (IO6)
						  * - only on 75x/76x
						  */
#define XRM117XX_MSR_DELTA_MASK	0x0F     /* Any of the delta bits! */

/*
 * TCR register bits
 * TCR trigger levels are available from 0 to 60 characters with a granularity
 * of four.
 * The programmer must program the TCR such that TCR[3:0] > TCR[7:4]. There is
 * no built-in hardware check to make sure this condition is met. Also, the TCR
 * must be programmed with this condition before auto RTS or software flow
 * control is enabled to avoid spurious operation of the device.
 */
#define XRM117XX_TCR_RX_HALT(words)	((((words) / 4) & 0x0f) << 0)
#define XRM117XX_TCR_RX_RESUME(words)	((((words) / 4) & 0x0f) << 4)

/*
 * TLR register bits
 * If TLR[3:0] or TLR[7:4] are logical 0, the selectable trigger levels via the
 * FIFO Control Register (FCR) are used for the transmit and receive FIFO
 * trigger levels. Trigger levels from 4 characters to 60 characters are
 * available with a granularity of four.
 *
 * When the trigger level setting in TLR is zero, the SC16IS740/750/760 uses the
 * trigger level setting defined in FCR. If TLR has non-zero trigger level value
 * the trigger level defined in FCR is discarded. This applies to both transmit
 * FIFO and receive FIFO trigger level setting.
 *
 * When TLR is used for RX trigger level control, FCR[7:6] should be left at the
 * default state, that is, '00'.
 */
#define XRM117XX_TLR_TX_TRIGGER(words)	((((words) / 4) & 0x0f) << 0)
#define XRM117XX_TLR_RX_TRIGGER(words)	((((words) / 4) & 0x0f) << 4)

/* IOControl register bits (Only 750/760) */
#define XRM117XX_IOCONTROL_LATCH_BIT	(1 << 0) /* Enable input latching */
#define XRM117XX_IOCONTROL_GPIO_BIT	(1 << 1) /* Enable GPIO[7:4] */
#define XRM117XX_IOCONTROL_SRESET_BIT	(1 << 3) /* Software Reset */

/* EFCR register bits */
#define XRM117XX_EFCR_9BIT_MODE_BIT	(1 << 0) /* Enable 9-bit or Multidrop
						  * mode (RS485) */
#define XRM117XX_EFCR_RXDISABLE_BIT	(1 << 1) /* Disable receiver */
#define XRM117XX_EFCR_TXDISABLE_BIT	(1 << 2) /* Disable transmitter */
#define XRM117XX_EFCR_AUTO_RS485_BIT	(1 << 4) /* Auto RS485 RTS direction */
#define XRM117XX_EFCR_RTS_INVERT_BIT	(1 << 5) /* RTS output inversion */
#define XRM117XX_EFCR_IRDA_MODE_BIT	(1 << 7) /* IrDA mode
						  * 0 = rate upto 115.2 kbit/s
						  *   - Only 750/760
						  * 1 = rate upto 1.152 Mbit/s
						  *   - Only 760
						  */

/* EFR register bits */
#define XRM117XX_EFR_AUTORTS_BIT	(1 << 6) /* Auto RTS flow ctrl enable */
#define XRM117XX_EFR_AUTOCTS_BIT	(1 << 7) /* Auto CTS flow ctrl enable */
#define XRM117XX_EFR_XOFF2_DETECT_BIT	(1 << 5) /* Enable Xoff2 detection */
#define XRM117XX_EFR_ENABLE_BIT	(1 << 4) /* Enable enhanced functions
						  * and writing to IER[7:4],
						  * FCR[5:4], MCR[7:5]
						  */
#define XRM117XX_EFR_SWFLOW3_BIT	(1 << 3) /* SWFLOW bit 3 */
#define XRM117XX_EFR_SWFLOW2_BIT	(1 << 2) /* SWFLOW bit 2
						  *
						  * SWFLOW bits 3 & 2 table:
						  * 00 -> no transmitter flow
						  *       control
						  * 01 -> transmitter generates
						  *       XON2 and XOFF2
						  * 10 -> transmitter generates
						  *       XON1 and XOFF1
						  * 11 -> transmitter generates
						  *       XON1, XON2, XOFF1 and
						  *       XOFF2
						  */
#define XRM117XX_EFR_SWFLOW1_BIT	(1 << 1) /* SWFLOW bit 2 */
#define XRM117XX_EFR_SWFLOW0_BIT	(1 << 0) /* SWFLOW bit 3
						  *
						  * SWFLOW bits 3 & 2 table:
						  * 00 -> no received flow
						  *       control
						  * 01 -> receiver compares
						  *       XON2 and XOFF2
						  * 10 -> receiver compares
						  *       XON1 and XOFF1
						  * 11 -> receiver compares
						  *       XON1, XON2, XOFF1 and
						  *       XOFF2
						  */

/* Misc definitions */
#define XRM117XX_FIFO_SIZE		(64)
#define XRM117XX_REG_SHIFT		2

struct xrm117x_devtype {
	char	name[10];
	int	nr_gpio;
	int	nr_uart;
};

struct xrm117x_port;

struct xrm117x_ch {
	struct uart_port	port;
	struct xrm117x_port	*xr;
	unsigned char		channel_no;
	unsigned char		actual_lcr;
	unsigned char		actual_mcr;
	unsigned char		actual_efr;
	unsigned char		actual_ier;
	unsigned char		new_ier;
	unsigned char		tx_empty;
	struct work_struct	tx_work;
	struct work_struct	md_work;
	struct work_struct      stop_rx_work;
	struct work_struct      stop_tx_work;
	struct serial_rs485	rs485;
	unsigned char           msr_reg;
	unsigned		reg_valid;
	unsigned char		reg_cache[SC_CHAN_REG_CNT];
};

struct xrm117x_port {
	struct uart_driver		uart;
	const struct xrm117x_devtype	*devtype;
	struct mutex			mutex;
	struct mutex			mutex_bus_access;
	struct spi_device		*spi_dev;
	struct i2c_client 		*i2c_client;
	struct clk			*clk;
	unsigned			r_valid;
	unsigned char			dev_cache[SC_REG_CNT - SC_CHAN_REG_CNT];

#ifdef CONFIG_GPIOLIB
	struct gpio_chip		gpio;
#endif

	unsigned char			buf[XRM117XX_FIFO_SIZE];
	struct xrm117x_ch		p[0];
};

/*
 * ------------------------------------------------------------------
 */
int xrm_write_byte_data(struct xrm117x_port *xr, u8 reg, u8 val)
{
	int ret;

	if (xr->i2c_client) {
		ret = i2c_smbus_write_byte_data(xr->i2c_client, reg, val);
	} else {
		unsigned char spi_buf[2];

		spi_buf[0] = reg;
		spi_buf[1] =  val;
		ret = spi_write(xr->spi_dev, spi_buf, 2);
	}
	if (ret < 0)
		printk("%s: reg=%x, val=%x, Failed %d\n", __func__, reg, val, ret);
	pr_debug("%s: (%02x) = 0x%02x\n", __func__, reg, val);
	return ret;
}

int xrm_write_block_data(struct xrm117x_port *xr, u8 reg, int count, u8* buf)
{
	int ret;

	if (xr->i2c_client) {
		ret = i2c_smbus_write_i2c_block_data(xr->i2c_client,
				reg, count, buf);
	} else {
		struct spi_message m;
		struct spi_transfer t[2] = { { .tx_buf = &reg, .len = 1, },
					     { .tx_buf = buf, .len = count, }, };
		spi_message_init(&m);
		spi_message_add_tail(&t[0], &m);
		spi_message_add_tail(&t[1], &m);
		ret = spi_sync(xr->spi_dev, &m);
	}
	if (ret < 0)
		printk("%s: reg=%x, cnt=%x, Failed %d\n", __func__, reg, count, ret);
	return ret;
}

int xrm_read_block_data(struct xrm117x_port *xr, u8 reg, int count, u8* buf)
{
	int ret;

	if (xr->i2c_client) {
		ret = i2c_smbus_read_i2c_block_data(xr->i2c_client,
				reg, count, buf);
	} else {
		u8 reg1 = reg | 0x80;
		ret = spi_write_then_read(xr->spi_dev, &reg1, 1, buf, count);
		if (ret >= 0)
			ret = count;
	}
	if (ret < 0)
		printk("%s: reg=%x, cnt=%x, Failed %d\n", __func__, reg, count, ret);
	pr_debug("%s: (%02x) = 0x%02x\n", __func__, reg, buf[0]);
	return ret;
}
/*
 * ------------------------------------------------------------------
 */
static int _sc_select(struct xrm117x_ch *ch, enum sc_reg reg)
{
	int ret = 0;
	unsigned mreg;
	if (reg > ARRAY_SIZE(reg_map))
		return -EINVAL;
	mreg = reg_map[reg];
	if (mreg & A_TYPE(SP_0xBF, 1)) {
		/* LCR ==  or != 0xbf matters */
		unsigned char lcr;
		unsigned char mcr;
		unsigned char efr;
		if (mreg & A_TYPEV(SP_MCR2)) {
			/* efr[4] should be 1 as well */
			efr = ch->reg_cache[SC_EFR] | 0x10;
			if (ch->actual_efr != efr) {
				ret = _sc_select(ch, SC_EFR);
				if (ret)
					return ret;
				ret = xrm_write_byte_data(ch->xr,
					MAKE_SUBADDRESS(ch->channel_no, UART_EFR),
					efr);
				if (ret)
					return ret;
				ch->actual_efr = efr;
			}
		}
		if (mreg & A_TYPE(SP_MCR2, 1)) {
			/* mcr[2] matters */
			mcr = ch->reg_cache[SC_MCR] & ~0x4;
			if (mreg & A_TYPEV(SP_MCR2))
				mcr |= 4;
			if (ch->actual_mcr != mcr) {
				ret = _sc_select(ch, SC_MCR);
				if (ret)
					return ret;
				ret = xrm_write_byte_data(ch->xr,
					MAKE_SUBADDRESS(ch->channel_no, UART_MCR),
					mcr);
				if (ret)
					return ret;
				ch->actual_mcr = mcr;
			}
		}
		lcr = (mreg & A_TYPEV(SP_0xBF)) ? 0xbf : ch->reg_cache[SC_LCR];
		if (mreg & A_TYPE(SP_DLAB, 1)) {
			if (mreg & A_TYPEV(SP_DLAB))
				lcr |= 0x80;
			else
				lcr &= ~0x80;
		}
		if (ch->actual_lcr != lcr) {
			ret = xrm_write_byte_data(ch->xr,
				MAKE_SUBADDRESS(ch->channel_no, UART_LCR),
				lcr);
			if (ret)
				return ret;
			ch->actual_lcr = lcr;
		}
	}
	return ret;
}

static int _sc_serial_out_cnt(struct xrm117x_ch *ch, enum sc_reg reg, unsigned char* buf, int count)
{
	int ret;
	unsigned sub_address;
	unsigned char value = buf[count - 1];
	unsigned mreg;
	int retry = 0;
	if (reg > ARRAY_SIZE(reg_map))
		return -EINVAL;
	mreg = reg_map[reg];
	sub_address = MAKE_SUBADDRESS(ch->channel_no, mreg);
	do {
		ret = _sc_select(ch, reg);
		if (!ret) {
			ret = xrm_write_block_data(ch->xr,
					sub_address, count, buf);
			if (!ret)
				break;
		}
	} while (retry++ < 3);

	if (ret) {
		pr_info("%s: error %i\n", __func__, ret);
		return ret;
	}
	if (reg < SC_CHAN_REG_CNT) {
		ch->reg_cache[reg] = value;
		ch->reg_valid |= BIT(reg);
		if (reg == SC_LCR)
			ch->actual_lcr = value;
		if (reg == SC_MCR)
			ch->actual_mcr = value;
		if (reg == SC_EFR)
			ch->actual_efr = value;
	} else {
		ch->xr->dev_cache[reg - SC_CHAN_REG_CNT] = value;
		ch->xr->r_valid |= BIT(reg - SC_CHAN_REG_CNT);
	}
	return ret;
}

static int _sc_serial_in_cnt(struct xrm117x_ch *ch, enum sc_reg reg,
		unsigned char * buf, int count)
{
	unsigned sub_address;
	int val = 0;
	int ret = 0;
	int retry = 0;
	if (reg > ARRAY_SIZE(reg_map))
		return -EINVAL;
	sub_address = MAKE_SUBADDRESS(ch->channel_no, reg_map[reg]);
	do {
		ret = _sc_select(ch, reg);
		if (!ret) {
			ret = xrm_read_block_data(ch->xr,
					sub_address, count, buf);
			if (ret == count)
				break;
			if (ret > 0) {
				pr_info("%s: error %i of %i bytes read\n",
						__func__, ret, count);
				if (ret > count)
					ret = count;
				break;
			}
		}
	} while (retry++ < 3);

	if (ret <= 0) {
		pr_info("%s: error %i, count=%i\n", __func__, ret, count);
		return ret;
	}

	val = buf[ret - 1];
	if (reg == SC_LCR) {
		ch->actual_lcr = val;
		if (val == 0xbf)
			return ret;	/* don't update reg_cache */
	} else if (reg == SC_MCR)
		ch->actual_mcr = val;
	else if (reg == SC_EFR)
		ch->actual_efr = val;
	else if (reg == SC_LSR)
		ch->tx_empty = val & UART_LSR_TEMT;

	if (reg < SC_CHAN_REG_CNT) {
		ch->reg_cache[reg] = val;
		ch->reg_valid |= BIT(reg);
	} else {
		ch->xr->dev_cache[reg - SC_CHAN_REG_CNT] = val;
		ch->xr->r_valid |= BIT(reg - SC_CHAN_REG_CNT);
	}
	return ret;
}

static int _sc_serial_in_always(struct xrm117x_ch *ch, enum sc_reg reg)
{
	unsigned char buf[4];
	int ret = _sc_serial_in_cnt(ch, reg, buf, 1);
	if (ret <= 0)
		return ret;
	return buf[0];
}

static int _sc_serial_in(struct xrm117x_ch *ch, enum sc_reg reg)
{
	if (reg > ARRAY_SIZE(reg_map))
		return -EINVAL;
	if (reg_map[reg] & _V)
		return _sc_serial_in_always(ch, reg);

	if (reg < SC_CHAN_REG_CNT) {
		if (ch->reg_valid & BIT(reg))
			return _sc_serial_in_always(ch, reg);
		return ch->reg_cache[reg];
	}
	if (ch->xr->r_valid & BIT(reg - SC_CHAN_REG_CNT))
		return _sc_serial_in_always(ch, reg);
	return ch->xr->dev_cache[reg - SC_CHAN_REG_CNT];
}

static int _sc_serial_modify(struct xrm117x_ch *ch, enum sc_reg reg, u8 mask, u8 value)
{
	int ret = _sc_serial_in(ch, reg);

	if (ret < 0)
		return ret;
	value |= (u8)(ret & ~mask);
	return _sc_serial_out_cnt(ch, reg, &value, 1);
}

/*
 * ------------------------------------------------------------------
 */
static int sc_serial_out_cnt(struct xrm117x_ch *ch, enum sc_reg reg, unsigned char* buf, int count)
{
	int ret;

	mutex_lock(&ch->xr->mutex_bus_access);
	ret = _sc_serial_out_cnt(ch, reg, buf, count);
	mutex_unlock(&ch->xr->mutex_bus_access);
	return ret;
}

static int sc_serial_out(struct xrm117x_ch *ch, enum sc_reg reg, unsigned char value)
{
	return sc_serial_out_cnt(ch, reg, &value, 1);
}

static int sc_serial_in_cnt(struct xrm117x_ch *ch, enum sc_reg reg,
		unsigned char * buf, int count)
{
	int ret;

	mutex_lock(&ch->xr->mutex_bus_access);
	ret = _sc_serial_in_cnt(ch, reg, buf, count);
	mutex_unlock(&ch->xr->mutex_bus_access);
	return ret;
}

static int sc_serial_in(struct xrm117x_ch *ch, enum sc_reg reg)
{
	int ret;

	mutex_lock(&ch->xr->mutex_bus_access);
	ret = _sc_serial_in(ch, reg);
	mutex_unlock(&ch->xr->mutex_bus_access);
	return ret;
}

static int sc_serial_modify(struct xrm117x_ch *ch, enum sc_reg reg, u8 mask, u8 value)
{
	int ret;

	mutex_lock(&ch->xr->mutex_bus_access);
	ret = _sc_serial_modify(ch, reg, mask, value);
	mutex_unlock(&ch->xr->mutex_bus_access);
	return ret;
}

/*
 * ------------------------------------------------------------------
 */

static void xrm117x_power(struct xrm117x_ch *ch, int on)
{
	sc_serial_modify(ch, SC_IER, XRM117XX_IER_SLEEP_BIT,
			      on ? 0 : XRM117XX_IER_SLEEP_BIT);
}

static const struct xrm117x_devtype xrm117x_devtype = {
	.name		= "XRM117X",
	.nr_gpio	= 0,
	.nr_uart	= 2,
};

static const struct xrm117x_devtype xrm1170_devtype = {
	.name		= "XRM1170",
	.nr_gpio	= 0,
	.nr_uart	= 1,
};
static int xrm117x_set_baud(struct xrm117x_ch *ch, int baud)
{
	u8 lcr;
	u8 prescaler = 0;
	int shift = 0;
	unsigned long clk = ch->port.uartclk;
	unsigned long div = (clk << 1) / baud;

	if (div < 0x20) {
		shift = 1;
		if (div < 0x10)
			shift = 2;
		div = (clk << (shift + 1)) / baud;
	}
	if (div & 1)
		div++;
	div >>= 1;
	if (div > 0xfffff) {
		prescaler = XRM117XX_MCR_CLKSEL_BIT;
		div /= 4;
		if (div > 0xfffff)
			div = 0xfffff;
	}

	lcr = sc_serial_in(ch, SC_LCR);

	/* Enable enhanced features */
	sc_serial_modify(ch, SC_EFR,
			XRM117XX_EFR_ENABLE_BIT, XRM117XX_EFR_ENABLE_BIT);

	sc_serial_modify(ch, SC_MCR,
			      XRM117XX_MCR_CLKSEL_BIT,
			      prescaler);

	/* Write the new divisor */
	sc_serial_out(ch, SC_DLH, div >> 12);
	sc_serial_out(ch, SC_DLL, (div >> 4) & 0xff);
	sc_serial_out(ch, SC_DLD, (shift << 4) | (div & 0x0f));
	/* Put LCR back to the normal mode */
	sc_serial_out(ch, SC_LCR, lcr);

	pr_debug("%s: baud=%d, div = 0x%lx, %d\n", __func__, baud, div, shift);
	return DIV_ROUND_CLOSEST(clk << shift, div);
}

#if 0
static int xrm117x_dump_register(struct xrm117x_ch *ch)
{
	u8 lcr;
	u8 i,reg;
	lcr = sc_serial_in(ch, SC_LCR);
	sc_serial_out(ch, SC_LCR, 0x03);
	printk("******Dump register at LCR=0x03\n");
	for(i=0;i<16;i++)
	{
		reg = sc_serial_in(ch, i);
		printk("Reg[0x%02x] = 0x%02x\n",i,reg);
	}
	
	sc_serial_out(ch, SC_LCR, 0xBF);
	printk("******Dump register at LCR=0xBF\n");
	for(i=0;i<16;i++)
	{
		reg = sc_serial_in(ch, i);
		printk("Reg[0x%02x] = 0x%02x\n",i,reg);
	}
	
	/* Put LCR back to the normal mode */
	sc_serial_out(ch, SC_LCR, lcr);
	return 0;
}
#endif

static void xrm117x_handle_rx(struct xrm117x_ch *ch, unsigned int rxlen,
				unsigned int iir)
{
	struct xrm117x_port *xr = ch->xr;
	unsigned int lsr = 0, flag, bytes_read, i;
	bool read_lsr = (iir == XRM117XX_IIR_RLSE_SRC) ? true : false;
 
	if (unlikely(rxlen >= sizeof(xr->buf)))
	{
	    /*
		dev_warn(port->dev,
				     "Port %i: Possible RX FIFO overrun: %d\n",
				     port->line, rxlen);
		*/
		ch->port.icount.buf_overrun++;
		/* Ensure sanity of RX level */
		rxlen = sizeof(xr->buf);
	}
    
	while (rxlen)
	{
		/* Only read lsr if there are possible errors in FIFO */
		if (read_lsr)
		{
			lsr = sc_serial_in(ch, SC_LSR);
			if (!(lsr & XRM117XX_LSR_FIFOE_BIT))
				read_lsr = false; /* No errors left in FIFO */
		} 
		else
			lsr = 0;

		if (read_lsr) {
			xr->buf[0] = sc_serial_in(ch, SC_RHR);
			bytes_read = 1;
		} else {
			if(rxlen < 64) {
				sc_serial_in_cnt(ch, SC_RHR, xr->buf, rxlen);
			} else {
				rxlen = 64;//split into two spi transfer
				sc_serial_in_cnt(ch, SC_RHR, xr->buf, 32);
				sc_serial_in_cnt(ch, SC_RHR, xr->buf + 32, 32);
				//printk("xrm117x_handle_rx rxlen >= 64\n");
			}
			bytes_read = rxlen;
		}

		lsr &= XRM117XX_LSR_BRK_ERROR_MASK;

		ch->port.icount.rx++;
		flag = TTY_NORMAL;

		if (unlikely(lsr)) {
			if (lsr & XRM117XX_LSR_BI_BIT) {
				ch->port.icount.brk++;
				if (uart_handle_break(&ch->port))
					continue;
			} else if (lsr & XRM117XX_LSR_PE_BIT)
				ch->port.icount.parity++;
			else if (lsr & XRM117XX_LSR_FE_BIT)
				ch->port.icount.frame++;
			else if (lsr & XRM117XX_LSR_OE_BIT)
				ch->port.icount.overrun++;

			lsr &= ch->port.read_status_mask;
			if (lsr & XRM117XX_LSR_BI_BIT)
				flag = TTY_BREAK;
			else if (lsr & XRM117XX_LSR_PE_BIT)
				flag = TTY_PARITY;
			else if (lsr & XRM117XX_LSR_FE_BIT)
				flag = TTY_FRAME;
			else if (lsr & XRM117XX_LSR_OE_BIT)
				flag = TTY_OVERRUN;
		}

		for (i = 0; i < bytes_read; ++i) {
			if (uart_handle_sysrq_char(&ch->port, xr->buf[i]))
				continue;

			if (lsr & ch->port.ignore_status_mask)
				continue;

			uart_insert_char(&ch->port, lsr, XRM117XX_LSR_OE_BIT,
					 xr->buf[i], flag);
		}
		rxlen -= bytes_read;
	}
	
	//tty_flip_buffer_push(ch->port.state->port.tty);
	tty_flip_buffer_push(&ch->port.state->port);
}

static void xrm117x_handle_tx(struct xrm117x_ch *ch)
{
	struct xrm117x_port *xr = ch->xr;
	struct circ_buf *xmit = &ch->port.state->xmit;
	unsigned int txlen, to_send, i;

	if (unlikely(ch->port.x_char))
	{
		sc_serial_out_cnt(ch, SC_THR, &ch->port.x_char, 1);
		ch->port.icount.tx++;
		ch->port.x_char = 0;
		return;
	}
	if (uart_circ_empty(xmit)|| uart_tx_stopped(&ch->port))
	{
	    //printk("xrm117x_handle_tx stopped\n");
	    return;
  	}
	
	/* Get length of data pending in circular buffer */
	to_send = uart_circ_chars_pending(xmit);
	
	if (likely(to_send)) 
	{
		/* Limit to size of TX FIFO */
		txlen = sc_serial_in(ch, SC_TXLVL);
		to_send = (to_send > txlen) ? txlen : to_send;

		/* Add data to send */
		ch->port.icount.tx += to_send;

		/* Convert to linear buffer */
		for (i = 0; i < to_send; ++i) 
		{
			xr->buf[i] = xmit->buf[xmit->tail];
			xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		}
		sc_serial_out_cnt(ch, SC_THR, xr->buf, to_send);

	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&ch->port);

}

static void xrm117x_port_irq(struct xrm117x_ch *ch)
{
	do {
		unsigned int iir, msr, rxlen;
		unsigned char lsr = sc_serial_in(ch, SC_LSR);

		if (lsr & 0x02) {
			/*dev_err(ch->port.dev,
					    "Port %i:Rx Overrun lsr =0x%02x",ch->port.line, lsr);*/
			dev_err(ch->port.dev,"Rx Overrun channel_no=%d\n", ch->channel_no);
		   
		}
		iir = sc_serial_in(ch, SC_IIR);
				
		if (iir & XRM117XX_IIR_NO_INT_BIT)
			break;
        
		iir &= XRM117XX_IIR_ID_MASK;

		switch (iir) {
		case XRM117XX_IIR_RDI_SRC:
		case XRM117XX_IIR_RLSE_SRC:
		case XRM117XX_IIR_RTOI_SRC:
		case XRM117XX_IIR_XOFFI_SRC:
			rxlen = sc_serial_in(ch, SC_RXLVL);
			if (rxlen)
				xrm117x_handle_rx(ch, rxlen, iir);
			break;

		case XRM117XX_IIR_CTSRTS_SRC:
			msr = sc_serial_in(ch, SC_MSR);
			uart_handle_cts_change(&ch->port,
					       !!(msr & XRM117XX_MSR_CTS_BIT));
			ch->msr_reg = msr;
			//printk("uart_handle_cts_change =0x%02x\n",msr);
			break;
		case XRM117XX_IIR_THRI_SRC:
			mutex_lock(&ch->xr->mutex);
			xrm117x_handle_tx(ch);
			mutex_unlock(&ch->xr->mutex);
			break;
		#if 1	
		case XRM117XX_IIR_MSI_SRC:
			msr = sc_serial_in(ch, SC_MSR);
			break; 
		#endif	
		default:
			dev_err(ch->port.dev,
					    "Port %i: Unexpected interrupt: %x",
					    ch->port.line, iir);
			break;
		}
	} while (1);
}

static irqreturn_t xrm117x_ist(int irq, void *dev_id)
{
	struct xrm117x_port *xr = (struct xrm117x_port *)dev_id;
	int i;

	for (i = 0; i < xr->uart.nr; ++i)
		xrm117x_port_irq(&xr->p[i]);

	return IRQ_HANDLED;
}

#define to_xrm117x_ch(p,e)	((container_of((p), struct xrm117x_ch, e)))

static void xrm117x_wq_proc(struct work_struct *ws)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(ws, tx_work);
	struct xrm117x_port *xr = ch->xr;
	mutex_lock(&xr->mutex);
	xrm117x_handle_tx(ch);
	mutex_unlock(&xr->mutex);
}

static void xrm117x_stop_tx(struct uart_port* port)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(port, port);

	schedule_work(&ch->stop_tx_work);
}

static void xrm117x_stop_rx(struct uart_port* port)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(port, port);

	schedule_work(&ch->stop_rx_work);
}

static void xrm117x_start_tx(struct uart_port *port)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(port, port);
	/* handle rs485 */
	if ((ch->rs485.flags & SER_RS485_ENABLED) &&
	    (ch->rs485.delay_rts_before_send > 0)) {
		mdelay(ch->rs485.delay_rts_before_send);
	}
	if (!work_pending(&ch->tx_work))
		schedule_work(&ch->tx_work);
}

static void xrm117x_stop_rx_work_proc(struct work_struct *ws)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(ws, stop_rx_work);

	ch->port.read_status_mask &= ~XRM117XX_LSR_DR_BIT;
	sc_serial_modify(ch, SC_LSR, XRM117XX_LSR_DR_BIT, 0);
}

static void xrm117x_stop_tx_work_proc(struct work_struct *ws)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(ws, stop_tx_work);
	struct xrm117x_port *xr = ch->xr;
	struct circ_buf *xmit = &ch->port.state->xmit;
	
	mutex_lock(&xr->mutex);
	/* handle rs485 */
	if (ch->rs485.flags & SER_RS485_ENABLED)
	{
		/* do nothing if current tx not yet completed */
		int lsr = sc_serial_in(ch, SC_LSR);
		if (!(lsr & XRM117XX_LSR_TEMT_BIT))
			return;

		if (uart_circ_empty(xmit) &&
			(ch->rs485.delay_rts_after_send > 0))
			mdelay(ch->rs485.delay_rts_after_send);
	}

	sc_serial_modify(ch, SC_IER, XRM117XX_IER_THRI_BIT, 0);
	mutex_unlock(&xr->mutex);
}

static unsigned int xrm117x_tx_empty(struct uart_port *port)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(port, port);
	unsigned int lsr;
	unsigned int result;

	lsr = sc_serial_in(ch, SC_LSR);
	result = (lsr & XRM117XX_LSR_THRE_BIT)  ? TIOCSER_TEMT : 0;
	return result;
}

static unsigned int xrm117x_get_mctrl(struct uart_port *port)
{
	unsigned int status,ret;
	struct xrm117x_ch *ch = to_xrm117x_ch(port, port);

	//status = sc_serial_in(ch, SC_MSR);
	status = ch->msr_reg;
	ret = 0;
	if (status & UART_MSR_DCD)
		ret |= TIOCM_CAR;
	if (status & UART_MSR_RI)
		ret |= TIOCM_RNG;
	if (status & UART_MSR_DSR)
		ret |= TIOCM_DSR;
	//if (status & UART_MSR_CTS)
		ret |= TIOCM_CTS;
	
	return ret;
}

static void xrm117x_md_proc(struct work_struct *ws)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(ws, md_work);
	unsigned int mctrl = ch->port.mctrl;
	unsigned char mcr = 0;
	if (mctrl & TIOCM_RTS)
		mcr |= UART_MCR_RTS;
	if (mctrl & TIOCM_DTR)
		mcr |= UART_MCR_DTR;
	if (mctrl & TIOCM_OUT1)
		mcr |= UART_MCR_OUT1;
	if (mctrl & TIOCM_OUT2)
		mcr |= UART_MCR_OUT2;
	if (mctrl & TIOCM_LOOP)
		mcr |= UART_MCR_LOOP;
	sc_serial_out(ch, SC_MCR, mcr);
	//sc_serial_modify(ch, SC_MCR,XRM117XX_MCR_LOOP_BIT,(ch->port.mctrl & TIOCM_LOOP) ?XRM117XX_MCR_LOOP_BIT : 0);
}

static void xrm117x_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(port, port);
	schedule_work(&ch->md_work);
}

static void xrm117x_break_ctl(struct uart_port *port, int break_state)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(port, port);

	sc_serial_modify(ch, SC_LCR,
			      XRM117XX_LCR_TXBREAK_BIT,
			      break_state ? XRM117XX_LCR_TXBREAK_BIT : 0);
}

static void xrm117x_set_termios(struct uart_port *port,
				  struct ktermios *termios,
				  struct ktermios *old)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(port, port);
	unsigned int lcr, flow = 0;
	int baud;

	/* Mask termios capabilities we don't support */
	termios->c_cflag &= ~CMSPAR;

	/* Word size */
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		lcr = XRM117XX_LCR_WORD_LEN_5;
		break;
	case CS6:
		lcr = XRM117XX_LCR_WORD_LEN_6;
		break;
	case CS7:
		lcr = XRM117XX_LCR_WORD_LEN_7;
		break;
	case CS8:
		lcr = XRM117XX_LCR_WORD_LEN_8;
		break;
	default:
		lcr = XRM117XX_LCR_WORD_LEN_8;
		termios->c_cflag &= ~CSIZE;
		termios->c_cflag |= CS8;
		break;
	}

	/* Parity */
	if (termios->c_cflag & PARENB) {
		lcr |= XRM117XX_LCR_PARITY_BIT;
		if (!(termios->c_cflag & PARODD))
			lcr |= XRM117XX_LCR_EVENPARITY_BIT;
	}

	/* Stop bits */
	if (termios->c_cflag & CSTOPB)
		lcr |= XRM117XX_LCR_STOPLEN_BIT; /* 2 stops */

	/* Set read status mask */
	port->read_status_mask = XRM117XX_LSR_OE_BIT;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= XRM117XX_LSR_PE_BIT |
					  XRM117XX_LSR_FE_BIT;
	if (termios->c_iflag & (BRKINT | PARMRK))
		port->read_status_mask |= XRM117XX_LSR_BI_BIT;

	/* Set status ignore mask */
	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNBRK)
		port->ignore_status_mask |= XRM117XX_LSR_BI_BIT;
	if (!(termios->c_cflag & CREAD))
		port->ignore_status_mask |= XRM117XX_LSR_BRK_ERROR_MASK;

	/* Configure flow control */
	sc_serial_out(ch, SC_XON1, termios->c_cc[VSTART]);
	sc_serial_out(ch, SC_XOFF1, termios->c_cc[VSTOP]);

	if (termios->c_cflag & CRTSCTS)
	{
		flow |= XRM117XX_EFR_AUTOCTS_BIT | XRM117XX_EFR_AUTORTS_BIT;
	}

	if (termios->c_iflag & IXON)
		flow |= XRM117XX_EFR_SWFLOW3_BIT;
	if (termios->c_iflag & IXOFF)
		flow |= XRM117XX_EFR_SWFLOW1_BIT;

	sc_serial_out(ch, SC_EFR, flow);
	//printk("xrm117x_set_termios write EFR = 0x%02x\n",flow);
   	
	/* Update LCR register */
	sc_serial_out(ch, SC_LCR, lcr);
	
	if (termios->c_cflag & CRTSCTS)
	{
		//printk("xrm117x_set_termios enable rts/cts\n");
		sc_serial_modify(ch, SC_MCR, XRM117XX_MCR_RTS_BIT, XRM117XX_MCR_RTS_BIT);
	} else {
		//printk("xrm117x_set_termios disable rts/cts\n");
		sc_serial_modify(ch, SC_MCR, XRM117XX_MCR_RTS_BIT,0);
	}
	//sc_serial_out(ch, SC_IOCONTROL, io_control);//configure GPIO  to GPIO or Modem control mode

	/* Get baud rate generator configuration */
	baud = uart_get_baud_rate(port, termios, old,
				  port->uartclk / 16 / 4 / 0xffff,
				  port->uartclk / 4);
	/* Setup baudrate generator */
	baud = xrm117x_set_baud(ch, baud);
	/* Update timeout according to new baud rate */
	uart_update_timeout(port, termios->c_cflag, baud);
	//xrm117x_dump_register(ch);
}

#if defined(TIOCSRS485) && defined(TIOCGRS485)
static void xrm117x_config_rs485(struct uart_port *port,
				   struct serial_rs485 *rs485)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(port, port);

	ch->rs485 = *rs485;

	if (ch->rs485.flags & SER_RS485_ENABLED) {
		sc_serial_modify(ch, SC_EFCR,
				      XRM117XX_EFCR_AUTO_RS485_BIT,
				      XRM117XX_EFCR_AUTO_RS485_BIT);
	} else {
		sc_serial_modify(ch, SC_EFCR,
				      XRM117XX_EFCR_AUTO_RS485_BIT,
				      0);
	}
}
#endif

static int xrm117x_ioctl(struct uart_port *port, unsigned int cmd,
			   unsigned long arg)
{
	
#if defined(TIOCSRS485) && defined(TIOCGRS485)
	struct serial_rs485 rs485;
	switch (cmd) 
	{
	case TIOCSRS485:
		if (copy_from_user(&rs485, (void __user *)arg, sizeof(rs485)))
			return -EFAULT;

		xrm117x_config_rs485(port, &rs485);
		return 0;
	case TIOCGRS485:
		if (copy_to_user((void __user *)arg,
				 &(to_xrm117x_ch(port, port)->rs485),
				 sizeof(rs485)))
			return -EFAULT;
		return 0;
	default:
		break;
	}
#endif

	return -ENOIOCTLCMD;
}

static int xrm117x_startup(struct uart_port *port)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(port, port);
	unsigned int val;

	if (!IS_ERR(ch->xr->clk)) {
		int ret = clk_prepare_enable(ch->xr->clk);
		if (ret)
			pr_info("%s: clk prepare failed %d\n", __func__, ret);
	}
	xrm117x_power(ch, 1);
	/* Reset FIFOs*/
	val =  XRM117XX_FCR_RXRESET_BIT | XRM117XX_FCR_TXRESET_BIT;
	sc_serial_out(ch, SC_FCR, val);
	udelay(5);
	sc_serial_out(ch, SC_FCR, XRM117XX_FCR_FIFO_BIT);

	/* Enable write access to enhanced features and internal clock div */
	sc_serial_out(ch, SC_EFR, XRM117XX_EFR_ENABLE_BIT);

	/* Enable TCR/TLR */
	sc_serial_modify(ch, SC_MCR,
			      XRM117XX_MCR_TCRTLR_BIT,
			      XRM117XX_MCR_TCRTLR_BIT);

	/* Configure flow control levels */
	/* Flow control halt level 48, resume level 24 */
	sc_serial_out(ch, SC_TCR,
			     XRM117XX_TCR_RX_RESUME(24) |
			     XRM117XX_TCR_RX_HALT(48));


	/* Now, initialize the UART */
	sc_serial_out(ch, SC_LCR, ch->reg_cache[SC_LCR]);

	/* Enable the Rx and Tx FIFO */
	sc_serial_modify(ch, SC_EFCR,
			      XRM117XX_EFCR_RXDISABLE_BIT |
			      XRM117XX_EFCR_TXDISABLE_BIT,
			      0);

	/* Enable RX, TX, CTS change interrupts */
	//val = XRM117XX_IER_RDI_BIT | XRM117XX_IER_THRI_BIT | XRM117XX_IER_CTSI_BIT;
	val = XRM117XX_IER_RDI_BIT | XRM117XX_IER_THRI_BIT | XRM117XX_IER_CTSI_BIT;	
	sc_serial_out(ch, SC_IER, val);

	
	return 0;
}

static void xrm117x_shutdown(struct uart_port *port)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(port, port);

	//printk("xrm117x_shutdown...\n");
	/* Disable all interrupts */
	sc_serial_out(ch, SC_IER, 0);
	
	/* Disable TX/RX */
	sc_serial_out(ch, SC_EFCR,
			     XRM117XX_EFCR_RXDISABLE_BIT |
			     XRM117XX_EFCR_TXDISABLE_BIT);

	xrm117x_power(ch, 0);
	if (!IS_ERR(ch->xr->clk))
		clk_disable_unprepare(ch->xr->clk);
}

static const char *xrm117x_type(struct uart_port *port)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(port, port);
	struct xrm117x_port *xr = ch->xr;
	return (port->type == PORT_SC16IS7XX) ? xr->devtype->name : NULL;
}


static int xrm117x_request_port(struct uart_port *port)
{
	/* Do nothing */
	return 0;
}

static void xrm117x_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE)
		port->type = PORT_SC16IS7XX;
}

static int xrm117x_verify_port(struct uart_port *port,
				 struct serial_struct *s)
{
	if ((s->type != PORT_UNKNOWN) && (s->type != PORT_SC16IS7XX))
		return -EINVAL;
	if (s->irq != port->irq)
		return -EINVAL;

	return 0;
}

static void xrm117x_pm(struct uart_port *port, unsigned int state,
			 unsigned int oldstate)
{
	struct xrm117x_ch *ch = to_xrm117x_ch(port, port);

	xrm117x_power(ch, (state == UART_PM_STATE_ON) ? 1 : 0);
}

static void xrm117x_null_void(struct uart_port *port)
{
	/* Do nothing */
}
static void xrm117x_enable_ms(struct uart_port *port)
{
	/* Do nothing */
}

static const struct uart_ops xrm117x_ops = {
	.tx_empty	= xrm117x_tx_empty,
	.set_mctrl	= xrm117x_set_mctrl,
	.get_mctrl	= xrm117x_get_mctrl,
	.stop_tx	= xrm117x_stop_tx,
	.start_tx	= xrm117x_start_tx,
	.stop_rx	= xrm117x_stop_rx,
	.break_ctl	= xrm117x_break_ctl,
	.startup	= xrm117x_startup,
	.shutdown	= xrm117x_shutdown,
	.set_termios	= xrm117x_set_termios,
	.type		= xrm117x_type,
	.request_port	= xrm117x_request_port,
	.release_port	= xrm117x_null_void,
	.config_port	= xrm117x_config_port,
	.verify_port	= xrm117x_verify_port,
	.ioctl		= xrm117x_ioctl,
	.enable_ms	= xrm117x_enable_ms,
	.pm		= xrm117x_pm,
};

#ifdef CONFIG_GPIOLIB
static int xrm117x_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	unsigned int val;
	struct xrm117x_port *xr = container_of(chip, struct xrm117x_port, gpio);
	struct xrm117x_ch *ch = &xr->p[0];

	val = sc_serial_in(ch, SC_IOSTATE_R);

	return !!(val & BIT(offset));
}

static void xrm117x_gpio_set(struct gpio_chip *chip, unsigned offset, int val)
{
	struct xrm117x_port *xr = container_of(chip, struct xrm117x_port,
						gpio);
	struct xrm117x_ch *ch = &xr->p[0];

	sc_serial_modify(ch, SC_IOSTATE_W, BIT(offset),
			      val ? BIT(offset) : 0);
}

static int xrm117x_gpio_direction_input(struct gpio_chip *chip,
					  unsigned offset)
{
	struct xrm117x_port *xr = container_of(chip, struct xrm117x_port, gpio);
	struct xrm117x_ch *ch = &xr->p[0];

	sc_serial_modify(ch, SC_IODIR, BIT(offset), 0);

	return 0;
}

static int xrm117x_gpio_direction_output(struct gpio_chip *chip,
					   unsigned offset, int val)
{
	struct xrm117x_port *xr = container_of(chip, struct xrm117x_port, gpio);
	struct xrm117x_ch *ch = &xr->p[0];

	sc_serial_modify(ch, SC_IOSTATE_W, BIT(offset),
			      val ? BIT(offset) : 0);
	sc_serial_modify(ch, SC_IODIR, BIT(offset),
			      BIT(offset));

	return 0;
}
#endif

#ifdef USE_OK335Dx_PLATFORM
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))
#define	GPIO_XR117X_IRQ	GPIO_TO_PIN(0,19)
//#define	GPIO_XR117X_IRQ	GPIO_TO_PIN(2,1)
#endif


static const char *reg_names[] = {
[SC_LCR] = "LCR",
[SC_RHR] = "RHR",
[SC_THR] = "THR",
[SC_IER] = "IER",
[SC_DLL] = "DLL",
[SC_DLH] = "DLH",
[SC_DLD] = "DLD",
[SC_IIR] = "IIR",
[SC_FCR] = "FCR",
[SC_MCR] = "MCR",
[SC_LSR] = "LSR",
[SC_TXLVL] = "TXLVL",
[SC_RXLVL] = "RXLVL",
[SC_EFCR] = "EFCR",
[SC_MSR] = "MSR",
[SC_SPR] = "SPR",
[SC_TCR] = "TCR",
[SC_TLR] = "TLR",
[SC_EFR] = "EFR",
[SC_XON1] = "XON1",
[SC_XON2] = "XON2",
[SC_XOFF1] = "XOFF1",
[SC_XOFF2] = "XOFF2",
[SC_IOSTATE_R] = "IOSTATE_R",
[SC_IOSTATE_W] = "IOSTATE_W",
[SC_IODIR] = "IODIR",
[SC_IOINTENA] = "IOINTENA",
[SC_IOCONTROL] = "IOCONTROL",
};

static struct xrm117x_port *serial_xrm117x_xr;

static ssize_t xrm117x_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xrm117x_port *xr = serial_xrm117x_xr;
	struct xrm117x_ch *ch;
	int total = 0;
	int cnt;
	int reg;
	int i;
	if (!xr)
		return -ENODEV;

	for (i = 0; i < xr->uart.nr; i++) {
		ch = &xr->p[i];
		cnt = sprintf(buf, "Channel %i:\n", i);
		total += cnt;
		buf += cnt;
		for (reg = 0; reg < SC_CHAN_REG_CNT; reg++) {
			if (ch->reg_valid & BIT(reg))
				cnt = sprintf(buf, "%s=%02x\n", reg_names[reg], ch->reg_cache[reg]);
			else
				cnt = sprintf(buf, "%s=??\n", reg_names[reg]);
			total += cnt;
			buf += cnt;
		}
		cnt = sprintf(buf, "\n");
		total += cnt;
		buf += cnt;
	}
	cnt = sprintf(buf, "IO_Regs:\n");
	total += cnt;
	buf += cnt;
	for (reg = SC_CHAN_REG_CNT; reg < SC_REG_CNT; reg++) {
		if (xr->r_valid & BIT(reg - SC_CHAN_REG_CNT))
			cnt = sprintf(buf, "%s=%02x\n", reg_names[reg], xr->dev_cache[reg - SC_CHAN_REG_CNT]);
		else
			cnt = sprintf(buf, "%s=??\n", reg_names[reg]);
		total += cnt;
		buf += cnt;
	}
	cnt = sprintf(buf, "\n");
	total += cnt;
	return total;
}

static DEVICE_ATTR(xrm117x_reg, 0444, xrm117x_reg_show, NULL);

static int xrm117x_probe_common(struct device *dev,
			   const struct xrm117x_devtype *devtype,
			   int irq, unsigned long flags,
			   struct spi_device *spi,
			   struct i2c_client *i2c)
{
	unsigned long freq;
	int i, ret;
	struct xrm117x_port *xr;
	struct gpio_desc *reset_gpio = NULL;
	
	/* Alloc port structure */
	xr = devm_kzalloc(dev, sizeof(*xr) +
			 sizeof(struct xrm117x_ch) * devtype->nr_uart,
			 GFP_KERNEL);
	if (!xr) {
		dev_err(dev, "Error allocating port structure\n");
		return -ENOMEM;
	}
	xr->spi_dev = spi;
	xr->i2c_client = i2c;
	xr->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(xr->clk)) {
		freq = 14745600;	/* Fixed clk freq */
	} else {
		freq = clk_get_rate(xr->clk);
		pr_info("%s: %ld\n", __func__, freq);
	}
	xr->devtype = devtype;

	dev_set_drvdata(dev, xr);
	/* Register UART driver */
	xr->uart.owner		= THIS_MODULE;
	xr->uart.dev_name	= "ttyXRM";
	xr->uart.nr		= devtype->nr_uart;
	ret = uart_register_driver(&xr->uart);
	if (ret) {
		dev_err(dev, "Registering UART driver failed\n");
		goto out_clk;
	}

#ifdef CONFIG_GPIOLIB
	if (devtype->nr_gpio) {
		/* Setup GPIO cotroller */
		xr->gpio.owner		 = THIS_MODULE;
		xr->gpio.dev		 = dev;
		xr->gpio.label		 = dev_name(dev);
		xr->gpio.direction_input = xrm117x_gpio_direction_input;
		xr->gpio.get		 = xrm117x_gpio_get;
		xr->gpio.direction_output = xrm117x_gpio_direction_output;
		xr->gpio.set		 = xrm117x_gpio_set;
		xr->gpio.base		 = -1;
		xr->gpio.ngpio		 = devtype->nr_gpio;
		xr->gpio.can_sleep	 = 1;
		ret = gpiochip_add(&xr->gpio);
		if (ret)
			goto out_uart;
	}
#endif
	reset_gpio = devm_gpiod_get_index(dev, "reset", 0, GPIOD_OUT_HIGH);
	if (!IS_ERR(reset_gpio)) {
		/* release reset */
		gpiod_set_value(reset_gpio, 1);        /* assert */
		udelay(1);
		gpiod_set_value(reset_gpio, 0);        /* release */
		msleep(2);
	} else {
		pr_warn("%s: could not find reset\n", __func__);
	}

	mutex_init(&xr->mutex);
	mutex_init(&xr->mutex_bus_access);
	//I2C_Bus_init();
	for (i = 0; i < devtype->nr_uart; ++i) {
		/* Initialize port data */
		xr->p[i].port.line	= i;
		xr->p[i].port.dev	= dev;
		xr->p[i].port.irq	= irq;
		xr->p[i].port.type	= PORT_SC16IS7XX;
		xr->p[i].port.fifosize	= XRM117XX_FIFO_SIZE;
		xr->p[i].port.flags	= UPF_FIXED_TYPE | UPF_LOW_LATENCY;
		xr->p[i].port.iotype	= UPIO_PORT;
		xr->p[i].port.uartclk	= freq;
		xr->p[i].port.ops	= &xrm117x_ops;
		xr->p[i].xr		= xr;
		/* Disable all interrupts */
		sc_serial_out(&xr->p[i], SC_IER, 0);
		/* Disable TX/RX */
		sc_serial_out(&xr->p[i], SC_EFCR,
				     XRM117XX_EFCR_RXDISABLE_BIT |
				     XRM117XX_EFCR_TXDISABLE_BIT);
		
		xr->p[i].msr_reg = sc_serial_in(&xr->p[i], SC_MSR);
		
		/* Initialize queue for start TX */
		INIT_WORK(&xr->p[i].tx_work, xrm117x_wq_proc);
		/* Initialize queue for changing mode */
		INIT_WORK(&xr->p[i].md_work, xrm117x_md_proc);

		INIT_WORK(&xr->p[i].stop_rx_work, xrm117x_stop_rx_work_proc);
		INIT_WORK(&xr->p[i].stop_tx_work, xrm117x_stop_tx_work_proc);
				
		/* Register port */
		uart_add_one_port(&xr->uart, &xr->p[i].port);
		/* Go to suspend mode */
		xrm117x_power(&xr->p[i], 0);
	}

#ifdef USE_OK335Dx_PLATFORM	
	/* Setup interrupt */
	irq_set_irq_type(gpio_to_irq(GPIO_XR117X_IRQ), IRQF_TRIGGER_FALLING);
	
	irq = gpio_to_irq(GPIO_XR117X_IRQ);
#endif	
	ret = devm_request_threaded_irq(dev, irq, NULL, xrm117x_ist,
					IRQF_ONESHOT | IRQF_TRIGGER_FALLING | IRQF_SHARED | flags, dev_name(dev), xr);
	if (ret < 0)
		goto irq_fail;

	serial_xrm117x_xr = xr;
	ret = device_create_file(dev, &dev_attr_xrm117x_reg);
	if (ret < 0)
		printk(KERN_WARNING "failed to add xrm117x sysfs files\n");

	return 0;

irq_fail:
	mutex_destroy(&xr->mutex);

#ifdef CONFIG_GPIOLIB
	if (devtype->nr_gpio)
		gpiochip_remove(&xr->gpio);

out_uart:
#endif
	uart_unregister_driver(&xr->uart);

out_clk:
	if (!IS_ERR(xr->clk))
		/*clk_disable_unprepare(xr->clk)*/;

	return ret;
}



static int xrm117x_remove(struct device *dev)
{
	struct xrm117x_port *xr = dev_get_drvdata(dev);
	int i;
#ifdef CONFIG_GPIOLIB
	if (xr->devtype->nr_gpio)
		gpiochip_remove(&xr->gpio);
#endif

	for (i = 0; i < xr->uart.nr; i++) {
		cancel_work_sync(&xr->p[i].tx_work);
		cancel_work_sync(&xr->p[i].md_work);
		cancel_work_sync(&xr->p[i].stop_rx_work);
		cancel_work_sync(&xr->p[i].stop_tx_work);
		uart_remove_one_port(&xr->uart, &xr->p[i].port);
		xrm117x_power(&xr->p[i], 0);
	}

	mutex_destroy(&xr->mutex);
	mutex_destroy(&xr->mutex_bus_access);
	uart_unregister_driver(&xr->uart);
	if (!IS_ERR(xr->clk))
		/*clk_disable_unprepare(xr->clk)*/;

	return 0;
}

static const struct of_device_id __maybe_unused xrm117x_dt_ids[] = {
	{ .compatible = "exar,xrm117x",	.data = &xrm117x_devtype, },
	{ }
};
MODULE_DEVICE_TABLE(of, xrm117x_dt_ids);


#ifdef USE_SPI_MODE
static int xrm117x_spi_probe(struct spi_device *spi)
{
	const struct spi_device_id *id = spi_get_device_id(spi);
	const struct xrm117x_devtype *devtype = (const struct xrm117x_devtype *)id->driver_data;
	unsigned long flags = 0;
	int ret;
	
		
	ret = xrm117x_probe_common(&spi->dev, devtype,spi->irq, flags, spi, NULL);
	return ret;
}


static int xrm117x_spi_remove(struct spi_device *spi)
{

      return xrm117x_remove(&spi->dev);
}

static const struct spi_device_id xrm117x_spi_ids[] = {
	{ "xrm1170",	(kernel_ulong_t)&xrm1170_devtype, },
	{ "xrm117x",	(kernel_ulong_t)&xrm117x_devtype, },
	{ },
};

MODULE_DEVICE_TABLE(spi, xrm117x_spi_ids);

static struct spi_driver xrm117x_spi_uart_driver = {
       .driver = {
               .name   = XRM117XX_NAME_SPI,
               .bus    = &spi_bus_type,
               .owner  = THIS_MODULE,
               .of_match_table	= of_match_ptr(xrm117x_dt_ids),
       },
       .probe          = xrm117x_spi_probe,
       .remove         = xrm117x_spi_remove,
       .id_table	= xrm117x_spi_ids,
};

module_spi_driver(xrm117x_spi_uart_driver);

MODULE_ALIAS("spi:xrm117x");
#else
static int xrm117x_i2c_probe(struct i2c_client *i2c,
			       const struct i2c_device_id *id)
{
	struct xrm117x_devtype *devtype;
	unsigned long flags = 0;
	int ret;
	if (i2c->dev.of_node) {
		const struct of_device_id *of_id =
				of_match_device(xrm117x_dt_ids, &i2c->dev);

		devtype = (struct xrm117x_devtype *)of_id->data;
	} else {
		devtype = (struct xrm117x_devtype *)id->driver_data;
		flags = IRQF_TRIGGER_FALLING;
	}
	ret = xrm117x_probe_common(&i2c->dev, devtype,i2c->irq, flags, NULL, i2c);
	return ret;
}

static int xrm117x_i2c_remove(struct i2c_client *client)
{
	return xrm117x_remove(&client->dev);
}


static const struct i2c_device_id xrm117x_i2c_id_table[] = {
	{ "xrm1170",	(kernel_ulong_t)&xrm1170_devtype, },
	{ "xrm117x",	(kernel_ulong_t)&xrm117x_devtype, },
	{ }
};

MODULE_DEVICE_TABLE(i2c, xrm117x_i2c_id_table);

static struct i2c_driver xrm117x_i2c_uart_driver = {
	.driver = {
		.name		= XRM117XX_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(xrm117x_dt_ids),
	},
	.probe		= xrm117x_i2c_probe,
	.remove		= xrm117x_i2c_remove,
	.id_table	= xrm117x_i2c_id_table,
};
/*
module_i2c_driver(xrm117x_i2c_uart_driver);

MODULE_ALIAS("i2c:xrm117x");
*/
static int __init xrm117x_init(void)
{
	return i2c_add_driver(&xrm117x_i2c_uart_driver);
}
module_init(xrm117x_init);

static void __exit xrm117x_exit(void)
{
	i2c_del_driver(&xrm117x_i2c_uart_driver);
}
module_exit(xrm117x_exit);

#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin xu <martin.xu@exar.com>");
MODULE_DESCRIPTION("XRM117XX serial driver");
