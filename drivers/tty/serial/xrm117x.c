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

//#define USE_SPI_MODE    0 
#define USE_OK335Dx_PLATFORM


/**
 * enum uart_pm_state - power states for UARTs
 * @UART_PM_STATE_ON: UART is powered, up and operational
 * @UART_PM_STATE_OFF: UART is powered off
 * @UART_PM_STATE_UNDEFINED: sentinel
 */
enum uart_pm_state {
	UART_PM_STATE_ON = 0,
	UART_PM_STATE_OFF = 3, /* number taken from ACPI */
	UART_PM_STATE_UNDEFINED,
};


#define XRM117XX_NAME			"xrm117x"
#define XRM117XX_NAME_SPI		"xrm117x_spi"


/* XRM117XX register definitions */
#define XRM117XX_RHR_REG		(0x00) /* RX FIFO */
#define XRM117XX_THR_REG		(0x00) /* TX FIFO */
#define XRM117XX_IER_REG		(0x01) /* Interrupt enable */
#define XRM117XX_IIR_REG		(0x02) /* Interrupt Identification */
#define XRM117XX_FCR_REG		(0x02) /* FIFO control */
#define XRM117XX_LCR_REG		(0x03) /* Line Control */
#define XRM117XX_MCR_REG		(0x04) /* Modem Control */
#define XRM117XX_LSR_REG		(0x05) /* Line Status */
#define XRM117XX_MSR_REG		(0x06) /* Modem Status */
#define XRM117XX_SPR_REG		(0x07) /* Scratch Pad */
#define XRM117XX_TXLVL_REG		(0x08) /* TX FIFO level */
#define XRM117XX_RXLVL_REG		(0x09) /* RX FIFO level */
#define XRM117XX_IODIR_REG		(0x0a) /* I/O Direction
						* - only on 75x/76x
						*/
#define XRM117XX_IOSTATE_REG		(0x0b) /* I/O State
						* - only on 75x/76x
						*/
#define XRM117XX_IOINTENA_REG		(0x0c) /* I/O Interrupt Enable
						* - only on 75x/76x
						*/
#define XRM117XX_IOCONTROL_REG		(0x0e) /* I/O Control
						* - only on 75x/76x
						*/
#define XRM117XX_EFCR_REG		(0x0f) /* Extra Features Control */

/* TCR/TLR Register set: Only if ((MCR[2] == 1) && (EFR[4] == 1)) */
#define XRM117XX_TCR_REG		(0x06) /* Transmit control */
#define XRM117XX_TLR_REG		(0x07) /* Trigger level */

/* Special Register set: Only if ((LCR[7] == 1) && (LCR != 0xBF)) */
#define XRM117XX_DLL_REG		(0x00) /* Divisor Latch Low */
#define XRM117XX_DLH_REG		(0x01) /* Divisor Latch High */

/* Enhanced Register set: Only if (LCR == 0xBF) */
#define XRM117XX_EFR_REG		(0x02) /* Enhanced Features */
#define XRM117XX_XON1_REG		(0x04) /* Xon1 word */
#define XRM117XX_XON2_REG		(0x05) /* Xon2 word */
#define XRM117XX_XOFF1_REG		(0x06) /* Xoff1 word */
#define XRM117XX_XOFF2_REG		(0x07) /* Xoff2 word */

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
#define XRM117XX_LCR_CONF_MODE_B	0xBF                   /* Enhanced
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

struct xrm117x_one {
	struct uart_port		port;
	struct work_struct		tx_work;
	struct work_struct		md_work;
    struct work_struct      stop_rx_work;
	struct work_struct      stop_tx_work;
	struct serial_rs485		rs485;
	unsigned char           msr_reg;
};

struct xrm117x_port {
	struct uart_driver		uart;
	struct xrm117x_devtype	*devtype;
	struct mutex			mutex;
	struct mutex			mutex_bus_access;
	struct clk			*clk;

#ifdef CONFIG_GPIOLIB
	struct gpio_chip		gpio;
#endif
	unsigned char			buf[XRM117XX_FIFO_SIZE];
	struct xrm117x_one		p[0];
};

#define to_xrm117x_one(p,e)	((container_of((p), struct xrm117x_one, e)))
#ifdef USE_SPI_MODE 
static struct spi_device *spi_dev = NULL; 
static u8 xrm117x_port_read(struct uart_port *port, u8 reg)
{
	struct xrm117x_port *s = dev_get_drvdata(port->dev);
	unsigned char cmd;
    ssize_t		status;
	u8			result; 
	mutex_lock(&s->mutex_bus_access);
	cmd = (0x80 | (reg<<3) | (port->line << 1)); 
	status = spi_write_then_read(spi_dev, &cmd, 1, &result, 1);
	mutex_unlock(&s->mutex_bus_access);
	if(status < 0)
	{
	    printk("Failed to xrm117x_port_read error code %d\n",status);
	}
	return result;
		
}

static void xrm117x_port_write(struct uart_port *port, u8 reg, u8 val)
{
	struct xrm117x_port *s = dev_get_drvdata(port->dev);
	unsigned char spi_buf[2];
	mutex_lock(&s->mutex_bus_access);
	spi_buf[0] = ((reg<<3) | (port->line << 1));
	spi_buf[1] =  val;
    ssize_t		status;
	status = spi_write(spi_dev, spi_buf, 2);
	if(status < 0)
	{
	    printk("Failed to xrm117x_port_write Err_code %d\n",status);
	}
	mutex_unlock(&s->mutex_bus_access);
	   
}
static void xrm117x_port_update(struct uart_port *port, u8 reg,
				  u8 mask, u8 val)
{
    unsigned int tmp;
	tmp = xrm117x_port_read(port,reg);
	tmp &= ~mask;
    tmp |= val & mask;
	xrm117x_port_write(port,reg,tmp); 
}


void xrm117x_raw_write(struct uart_port *port,const void *reg,unsigned char *buf,int len)
{
    struct xrm117x_port *s = dev_get_drvdata(port->dev);
	ssize_t		status;
  	struct spi_message m;
	struct spi_transfer t[2] = { { .tx_buf = reg, .len = 1, },
				     { .tx_buf = buf, .len = len, }, };
	mutex_lock(&s->mutex_bus_access);		
	spi_message_init(&m);
	spi_message_add_tail(&t[0], &m);
	spi_message_add_tail(&t[1], &m);
	status = spi_sync(spi_dev, &m);
	if(status < 0)
	{
	    printk("Failed to xrm117x_raw_write Err_code %d\n",status);
	}
	mutex_unlock(&s->mutex_bus_access);

}
void xrm117x_raw_read(struct uart_port *port,unsigned char *buf,int len)
{
    struct xrm117x_port *s = dev_get_drvdata(port->dev);
  	unsigned char cmd;
    ssize_t		status;
	mutex_lock(&s->mutex_bus_access);	
	cmd = 0x80 | (XRM117XX_RHR_REG << 3) | (port->line << 1) ; 
	status = spi_write_then_read(spi_dev, &cmd, 1, buf, len);
	mutex_unlock(&s->mutex_bus_access);
	if(status < 0)
	{
	    printk("Failed to xrm117x_raw_read Err_code %d\n",status);
		
	}
 
}
#else
static struct i2c_client *xrm117x_i2c_client = NULL;

static u8 xrm117x_port_read(struct uart_port *port, u8 reg)
{
	struct xrm117x_port *s = dev_get_drvdata(port->dev);
    struct i2c_msg xfer[2];
	u8 reg_value = 0;
	u8 cmd = (reg <<3)|(port->line << 1);
	int ret; 
	
	mutex_lock(&s->mutex_bus_access);
	xfer[0].addr = xrm117x_i2c_client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 1;
	xfer[0].buf = (void *)&cmd;

	xfer[1].addr = xrm117x_i2c_client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = 1;
	xfer[1].buf = &reg_value;
   	
	ret = i2c_transfer(xrm117x_i2c_client->adapter, xfer, 2);
	mutex_unlock(&s->mutex_bus_access);
	if (ret == 2)
	{
	  return reg_value;
	}
	else if (ret < 0)
	{
	    printk("Failed to xrm117x_port_read <%d>\n",ret);
		return ret;
	}
	else
	{
		return -EIO;
	}	
		
		
}

static int xrm117x_port_write(struct uart_port *port, u8 reg, u8 val)
{
	struct xrm117x_port *s = dev_get_drvdata(port->dev);
	unsigned char i2c_buf[2];
	int		ret;
	i2c_buf[0] = ((reg<<3) | (port->line << 1));
	i2c_buf[1] = val;
	mutex_lock(&s->mutex_bus_access);
	ret = i2c_master_send(xrm117x_i2c_client, i2c_buf, 2);
	mutex_unlock(&s->mutex_bus_access);
	if (ret == 2)
		return 0;
	else if (ret < 0)
	{
	    printk("Failed to xrm117x_port_write <%d>\n",ret);
		return ret;
	}
	else
	{
		return -EIO;
	}	
	   
}

static int xrm117x_port_fifo_write(struct uart_port *port, const void *val, size_t val_len)
{
	unsigned char *buf;
	int		ret;
	buf = (unsigned char *)kmalloc(val_len + 1, GFP_KERNEL);
	if (!buf)
			return -ENOMEM;
	buf[0] = ((port->line) << 1);
	memcpy(buf + 1, val, val_len);
	ret = i2c_master_send(xrm117x_i2c_client, buf, val_len + 1);
	kfree(buf);
	if (ret == val_len + 1)
		return 0;
	else if (ret < 0)
	{
	    printk("Failed to xrm117x_port_fifo_write <%d>\n",ret);
		return ret;
	}
	else
	{
		return -EIO;
	}	
	   
}

static void xrm117x_port_update(struct uart_port *port, u8 reg,
				  u8 mask, u8 val)
{
    unsigned int tmp;
	tmp = xrm117x_port_read(port,reg);
	tmp &= ~mask;
    tmp |= val & mask;
	xrm117x_port_write(port,reg,tmp); 
}

static int xrm117x_raw_write(struct uart_port *port,const void *reg,unsigned char *buf,int len)
{
    struct xrm117x_port *s = dev_get_drvdata(port->dev);
	struct i2c_msg xfer[2];
	int ret;
  	mutex_lock(&s->mutex_bus_access);
  
    /* If the I2C controller can't do a gather tell the core, it
	 * will substitute in a linear write for us.
	 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)	 
	if (!i2c_check_functionality(xrm117x_i2c_client->adapter, I2C_FUNC_PROTOCOL_MANGLING))
#else		
	if (!i2c_check_functionality(xrm117x_i2c_client->adapter, I2C_FUNC_NOSTART))
#endif		
	{/* If that didn't work fall back on linearising by hand. */
	 	ret = xrm117x_port_fifo_write(port,buf,len);
		mutex_unlock(&s->mutex_bus_access);
		return ret;
	}
	
	xfer[0].addr = xrm117x_i2c_client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 1;
	xfer[0].buf = (void *)reg;

	xfer[1].addr = xrm117x_i2c_client->addr;
	xfer[1].flags = I2C_M_NOSTART;
	xfer[1].len = len;
	xfer[1].buf = (void *)buf;
	ret = i2c_transfer(xrm117x_i2c_client->adapter, xfer, 2);
	mutex_unlock(&s->mutex_bus_access);
	if (ret == 2)
		return 0;
	else if (ret < 0)
	{
	    printk("Failed to xrm117x_raw_write <%d>\n",ret);
		return ret;
	}
	else
		return -EIO;

}
static int xrm117x_raw_read(struct uart_port *port,unsigned char *buf,int len)
{
    struct xrm117x_port *s = dev_get_drvdata(port->dev);
  	struct i2c_msg xfer[2];
	unsigned char cmd = (XRM117XX_RHR_REG << 3) | (port->line << 1);
	int ret;
	mutex_lock(&s->mutex_bus_access);
    xfer[0].addr = xrm117x_i2c_client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 1;
	xfer[0].buf = (void *)&cmd;

	xfer[1].addr = xrm117x_i2c_client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = buf;

	ret = i2c_transfer(xrm117x_i2c_client->adapter, xfer, 2);
	mutex_unlock(&s->mutex_bus_access);
	if (ret == 2)
	{
	  return 0;
	}
	else if (ret < 0)
	{
	    printk("Failed to xrm117x_raw_read <%d>\n",ret);
		return ret;
	}
	else
		return -EIO;
	
	
 
}

#endif
static void xrm117x_power(struct uart_port *port, int on)
{
	xrm117x_port_update(port, XRM117XX_IER_REG,
			      XRM117XX_IER_SLEEP_BIT,
			      on ? 0 : XRM117XX_IER_SLEEP_BIT);
}

static const struct xrm117x_devtype xrm117x_devtype = {
	.name		= "XRM117X",
	.nr_gpio	= 0,
	.nr_uart	= 2,
};
static int xrm117x_set_baud(struct uart_port *port, int baud)
{
	u8 lcr,tmp;
	u8 prescaler = 0;
	unsigned long clk = port->uartclk, div = clk / 16 / baud;
 	if (div > 0xffff) {
		prescaler = XRM117XX_MCR_CLKSEL_BIT;
		div /= 4;
	}

	lcr = xrm117x_port_read(port, XRM117XX_LCR_REG);

	/* Open the LCR divisors for configuration */
	xrm117x_port_write(port, XRM117XX_LCR_REG,
			     XRM117XX_LCR_CONF_MODE_B);

	/* Enable enhanced features */
	tmp = xrm117x_port_read(port, XRM117XX_EFR_REG);
	
	xrm117x_port_write(port, XRM117XX_EFR_REG,
			     tmp | XRM117XX_EFR_ENABLE_BIT);

	/* Put LCR back to the normal mode */
	xrm117x_port_write(port, XRM117XX_LCR_REG, lcr);

	xrm117x_port_update(port, XRM117XX_MCR_REG,
			      XRM117XX_MCR_CLKSEL_BIT,
			      prescaler);

	/* Open the LCR divisors for configuration */
	xrm117x_port_write(port, XRM117XX_LCR_REG,
			     XRM117XX_LCR_CONF_MODE_A);

	/* Write the new divisor */
	xrm117x_port_write(port, XRM117XX_DLH_REG, div / 256);
	xrm117x_port_write(port, XRM117XX_DLL_REG, div % 256);
	
	/* Put LCR back to the normal mode */
	xrm117x_port_write(port, XRM117XX_LCR_REG, lcr);

	return DIV_ROUND_CLOSEST(clk / 16, div);
}

static int xrm117x_dump_register(struct uart_port *port)
{
	u8 lcr;
	u8 i,reg;
	lcr = xrm117x_port_read(port, XRM117XX_LCR_REG);
    xrm117x_port_write(port, XRM117XX_LCR_REG, 0x03);
	printk("******Dump register at LCR=0x03\n");
	for(i=0;i<16;i++)
	{
		reg = xrm117x_port_read(port, i);
		printk("Reg[0x%02x] = 0x%02x\n",i,reg);
	}
	
	xrm117x_port_write(port, XRM117XX_LCR_REG, 0xBF);
	printk("******Dump register at LCR=0xBF\n");
	for(i=0;i<16;i++)
	{
		reg = xrm117x_port_read(port, i);
		printk("Reg[0x%02x] = 0x%02x\n",i,reg);
	}
	
	/* Put LCR back to the normal mode */
	xrm117x_port_write(port, XRM117XX_LCR_REG, lcr);
	return 0;
}

static void xrm117x_handle_rx(struct uart_port *port, unsigned int rxlen,
				unsigned int iir)
{
	struct xrm117x_port *s = dev_get_drvdata(port->dev);
	unsigned int lsr = 0, ch, flag, bytes_read, i;
	bool read_lsr = (iir == XRM117XX_IIR_RLSE_SRC) ? true : false;
 
	if (unlikely(rxlen >= sizeof(s->buf)))
	{
	    /*
		dev_warn(port->dev,
				     "Port %i: Possible RX FIFO overrun: %d\n",
				     port->line, rxlen);
		*/
		port->icount.buf_overrun++;
		/* Ensure sanity of RX level */
		rxlen = sizeof(s->buf);
	}
    
	while (rxlen)
	{
		/* Only read lsr if there are possible errors in FIFO */
		if (read_lsr)
		{
			lsr = xrm117x_port_read(port, XRM117XX_LSR_REG);
			if (!(lsr & XRM117XX_LSR_FIFOE_BIT))
				read_lsr = false; /* No errors left in FIFO */
		} 
		else
			lsr = 0;

		if (read_lsr) 
		{
			s->buf[0] = xrm117x_port_read(port, XRM117XX_RHR_REG);
			bytes_read = 1;
		} 
		else
		{
		    if(rxlen < 64)
		    {
		      //printk("xrm117x_handle_rx rxlen:%d\n",rxlen);
	    	  xrm117x_raw_read(port,s->buf,rxlen);
		    }
			else
			{
			  rxlen = 64;//split into two spi transfer
			  xrm117x_raw_read(port,s->buf,32);
			  xrm117x_raw_read(port,s->buf+32,32);
			  //printk("xrm117x_handle_rx rxlen >= 64\n");
			}
			bytes_read = rxlen;
		}

		lsr &= XRM117XX_LSR_BRK_ERROR_MASK;

		port->icount.rx++;
		flag = TTY_NORMAL;

		if (unlikely(lsr)) {
			if (lsr & XRM117XX_LSR_BI_BIT) {
				port->icount.brk++;
				if (uart_handle_break(port))
					continue;
			} else if (lsr & XRM117XX_LSR_PE_BIT)
				port->icount.parity++;
			else if (lsr & XRM117XX_LSR_FE_BIT)
				port->icount.frame++;
			else if (lsr & XRM117XX_LSR_OE_BIT)
				port->icount.overrun++;

			lsr &= port->read_status_mask;
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
			ch = s->buf[i];
			if (uart_handle_sysrq_char(port, ch))
				continue;

			if (lsr & port->ignore_status_mask)
				continue;

			uart_insert_char(port, lsr, XRM117XX_LSR_OE_BIT, ch,
					 flag);
		}
		rxlen -= bytes_read;
	}
	
      tty_flip_buffer_push(port->state->port.tty);
	  //tty_flip_buffer_push(&port->state->port);
}

static void xrm117x_handle_tx(struct uart_port *port)
{
	struct xrm117x_port *s = dev_get_drvdata(port->dev);
	struct circ_buf *xmit = &port->state->xmit;
	unsigned int txlen, to_send, i;
	unsigned char thr_reg;
 	if (unlikely(port->x_char))
	{
		xrm117x_port_write(port, XRM117XX_THR_REG, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}
  	if (uart_circ_empty(xmit)|| uart_tx_stopped(port)) 
	{
	    //printk("xrm117x_handle_tx stopped\n");
	    return;
  	}
	
	/* Get length of data pending in circular buffer */
	to_send = uart_circ_chars_pending(xmit);
	
	if (likely(to_send)) 
	{
		/* Limit to size of TX FIFO */
		txlen = xrm117x_port_read(port, XRM117XX_TXLVL_REG);
		to_send = (to_send > txlen) ? txlen : to_send;

		/* Add data to send */
		port->icount.tx += to_send;

		/* Convert to linear buffer */
		for (i = 0; i < to_send; ++i) 
		{
			s->buf[i] = xmit->buf[xmit->tail];
			xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		}
		//printk("xrm117x_handle_tx %d bytes\n",to_send);
		thr_reg = (XRM117XX_THR_REG | (port->line << 1));
		xrm117x_raw_write(port,&thr_reg,s->buf,to_send);

	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

}

static void xrm117x_port_irq(struct xrm117x_port *s, int portno)
{
	struct uart_port *port = &s->p[portno].port;
	do {
		unsigned int iir, msr, rxlen;
		unsigned char lsr;
        lsr = xrm117x_port_read(port, XRM117XX_LSR_REG);
		if(lsr & 0x02)
		{
		
		    /*dev_err(port->dev,
					    "Port %i:Rx Overrun lsr =0x%02x",port->line, lsr);*/
		    dev_err(port->dev,"Rx Overrun portno=%d\n",portno);
		   
		}
		iir = xrm117x_port_read(port, XRM117XX_IIR_REG);
				
		if (iir & XRM117XX_IIR_NO_INT_BIT)
			break;
        
		iir &= XRM117XX_IIR_ID_MASK;

		switch (iir) {
		case XRM117XX_IIR_RDI_SRC:
		case XRM117XX_IIR_RLSE_SRC:
		case XRM117XX_IIR_RTOI_SRC:
		case XRM117XX_IIR_XOFFI_SRC:
			rxlen = xrm117x_port_read(port, XRM117XX_RXLVL_REG);
			if (rxlen)
				xrm117x_handle_rx(port, rxlen, iir);
			break;

		case XRM117XX_IIR_CTSRTS_SRC:
			msr = xrm117x_port_read(port, XRM117XX_MSR_REG);
			uart_handle_cts_change(port,
					       !!(msr & XRM117XX_MSR_CTS_BIT));
			s->p[portno].msr_reg = msr; 
			//printk("uart_handle_cts_change =0x%02x\n",msr);
			break;
		case XRM117XX_IIR_THRI_SRC:
			mutex_lock(&s->mutex);
			xrm117x_handle_tx(port);
			mutex_unlock(&s->mutex);
			break;
		#if 1	
		case XRM117XX_IIR_MSI_SRC:
			msr = xrm117x_port_read(port, XRM117XX_MSR_REG);
			break; 
		#endif	
		default:
			dev_err(port->dev,
					    "Port %i: Unexpected interrupt: %x",
					    port->line, iir);
			break;
		}
	} while (1);
}

static irqreturn_t xrm117x_ist(int irq, void *dev_id)
{
	struct xrm117x_port *s = (struct xrm117x_port *)dev_id;
	int i;

	for (i = 0; i < s->uart.nr; ++i)
		xrm117x_port_irq(s, i);

	return IRQ_HANDLED;
}

static void xrm117x_wq_proc(struct work_struct *ws)
{
	struct xrm117x_one *one = to_xrm117x_one(ws, tx_work);
	struct xrm117x_port *s = dev_get_drvdata(one->port.dev);
	mutex_lock(&s->mutex);
	xrm117x_handle_tx(&one->port);
	mutex_unlock(&s->mutex);
}

static void xrm117x_stop_tx(struct uart_port* port)
{
    struct xrm117x_one *one = to_xrm117x_one(port, port);
	schedule_work(&one->stop_tx_work);
	
	
}

static void xrm117x_stop_rx(struct uart_port* port)
{
    struct xrm117x_one *one = to_xrm117x_one(port, port);
	schedule_work(&one->stop_rx_work);

	
}

static void xrm117x_start_tx(struct uart_port *port)
{
	struct xrm117x_one *one = to_xrm117x_one(port, port);
	/* handle rs485 */
	if ((one->rs485.flags & SER_RS485_ENABLED) &&
	    (one->rs485.delay_rts_before_send > 0)) {
		mdelay(one->rs485.delay_rts_before_send);
	}
    if (!work_pending(&one->tx_work))
    {
		schedule_work(&one->tx_work);
    }
	
}
static void xrm117x_stop_rx_work_proc(struct work_struct *ws)
{
    struct xrm117x_one *one = to_xrm117x_one(ws, stop_rx_work);
	struct xrm117x_port *s = dev_get_drvdata(one->port.dev);
	mutex_lock(&s->mutex);
    one->port.read_status_mask &= ~XRM117XX_LSR_DR_BIT;
		xrm117x_port_update(&one->port, XRM117XX_IER_REG,
					  XRM117XX_LSR_DR_BIT,
					  0);
	mutex_unlock(&s->mutex);
	
}
static void xrm117x_stop_tx_work_proc(struct work_struct *ws)
{
	struct xrm117x_one *one = to_xrm117x_one(ws, stop_tx_work);
	struct xrm117x_port *s = dev_get_drvdata(one->port.dev);
	struct circ_buf *xmit = &one->port.state->xmit;
	
	mutex_lock(&s->mutex);
	/* handle rs485 */
	if (one->rs485.flags & SER_RS485_ENABLED)
	{
		/* do nothing if current tx not yet completed */
		int lsr = xrm117x_port_read(&one->port, XRM117XX_LSR_REG);
		if (!(lsr & XRM117XX_LSR_TEMT_BIT))
			return;

		if (uart_circ_empty(xmit) &&
			(one->rs485.delay_rts_after_send > 0))
			mdelay(one->rs485.delay_rts_after_send);
	}

	xrm117x_port_update(&one->port, XRM117XX_IER_REG,
				  XRM117XX_IER_THRI_BIT,
				  0);
	mutex_unlock(&s->mutex);
	

}

static unsigned int xrm117x_tx_empty(struct uart_port *port)
{
	unsigned int lsr;
    unsigned int result;
	lsr = xrm117x_port_read(port, XRM117XX_LSR_REG);
    result = (lsr & XRM117XX_LSR_THRE_BIT)  ? TIOCSER_TEMT : 0; 
	return result;
}

static unsigned int xrm117x_get_mctrl(struct uart_port *port)
{
    unsigned int status,ret;
	struct xrm117x_port *s = dev_get_drvdata(port->dev);
	//status = xrm117x_port_read(port, XRM117XX_MSR_REG);
	status = s->p[port->line].msr_reg;
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
	struct xrm117x_one *one = to_xrm117x_one(ws, md_work);
	unsigned int mctrl = one->port.mctrl;
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
	xrm117x_port_write(&one->port, XRM117XX_MCR_REG, mcr);
	//xrm117x_port_update(&one->port, XRM117XX_MCR_REG,XRM117XX_MCR_LOOP_BIT,(one->port.mctrl & TIOCM_LOOP) ?XRM117XX_MCR_LOOP_BIT : 0);
}

static void xrm117x_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct xrm117x_one *one = to_xrm117x_one(port, port);
	schedule_work(&one->md_work);
}

static void xrm117x_break_ctl(struct uart_port *port, int break_state)
{
	xrm117x_port_update(port, XRM117XX_LCR_REG,
			      XRM117XX_LCR_TXBREAK_BIT,
			      break_state ? XRM117XX_LCR_TXBREAK_BIT : 0);
}

static void xrm117x_set_termios(struct uart_port *port,
				  struct ktermios *termios,
				  struct ktermios *old)
{
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

	xrm117x_port_write(port, XRM117XX_LCR_REG,
			     XRM117XX_LCR_CONF_MODE_B);

	/* Configure flow control */
	xrm117x_port_write(port, XRM117XX_XON1_REG, termios->c_cc[VSTART]);
	xrm117x_port_write(port, XRM117XX_XOFF1_REG, termios->c_cc[VSTOP]);
	if (termios->c_cflag & CRTSCTS)
	{
		flow |= XRM117XX_EFR_AUTOCTS_BIT | XRM117XX_EFR_AUTORTS_BIT;
	}
	
	if (termios->c_iflag & IXON)
		flow |= XRM117XX_EFR_SWFLOW3_BIT;
	if (termios->c_iflag & IXOFF)
		flow |= XRM117XX_EFR_SWFLOW1_BIT;
	
	xrm117x_port_write(port, XRM117XX_EFR_REG, flow);
	//printk("xrm117x_set_termios write EFR = 0x%02x\n",flow);
   	
	/* Update LCR register */
	xrm117x_port_write(port, XRM117XX_LCR_REG, lcr);
	
	if (termios->c_cflag & CRTSCTS)
	{
	    //printk("xrm117x_set_termios enable rts/cts\n");
		xrm117x_port_update(port, XRM117XX_MCR_REG,XRM117XX_MCR_RTS_BIT,XRM117XX_MCR_RTS_BIT);
	}
    else
    {
        //printk("xrm117x_set_termios disable rts/cts\n");
        xrm117x_port_update(port, XRM117XX_MCR_REG,XRM117XX_MCR_RTS_BIT,0);
    }
	//xrm117x_port_write(port, XRM117XX_IOCONTROL_REG, io_control);//configure GPIO  to GPIO or Modem control mode
	
	/* Get baud rate generator configuration */
	baud = uart_get_baud_rate(port, termios, old,
				  port->uartclk / 16 / 4 / 0xffff,
				  port->uartclk / 16);
    /* Setup baudrate generator */
	baud = xrm117x_set_baud(port, baud);
	/* Update timeout according to new baud rate */
	uart_update_timeout(port, termios->c_cflag, baud);
	//xrm117x_dump_register(port);
	
	
}

#if defined(TIOCSRS485) && defined(TIOCGRS485)
static void xrm117x_config_rs485(struct uart_port *port,
				   struct serial_rs485 *rs485)
{
	struct xrm117x_one *one = to_xrm117x_one(port, port);
	one->rs485 = *rs485;

	if (one->rs485.flags & SER_RS485_ENABLED) {
		xrm117x_port_update(port, XRM117XX_EFCR_REG,
				      XRM117XX_EFCR_AUTO_RS485_BIT,
				      XRM117XX_EFCR_AUTO_RS485_BIT);
	} else {
		xrm117x_port_update(port, XRM117XX_EFCR_REG,
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
				 &(to_xrm117x_one(port, port)->rs485),
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
	unsigned int val;
   	xrm117x_power(port, 1);
	/* Reset FIFOs*/
	val =  XRM117XX_FCR_RXRESET_BIT | XRM117XX_FCR_TXRESET_BIT;
	xrm117x_port_write(port, XRM117XX_FCR_REG, val);
	udelay(5);
	xrm117x_port_write(port, XRM117XX_FCR_REG,
			     XRM117XX_FCR_FIFO_BIT);

	/* Enable EFR */
	xrm117x_port_write(port, XRM117XX_LCR_REG,
			     XRM117XX_LCR_CONF_MODE_B);

	/* Enable write access to enhanced features and internal clock div */
	xrm117x_port_write(port, XRM117XX_EFR_REG,
			     XRM117XX_EFR_ENABLE_BIT);

	/* Enable TCR/TLR */
	xrm117x_port_update(port, XRM117XX_MCR_REG,
			      XRM117XX_MCR_TCRTLR_BIT,
			      XRM117XX_MCR_TCRTLR_BIT);

	/* Configure flow control levels */
	/* Flow control halt level 48, resume level 24 */
	xrm117x_port_write(port, XRM117XX_TCR_REG,
			     XRM117XX_TCR_RX_RESUME(24) |
			     XRM117XX_TCR_RX_HALT(48));


	/* Now, initialize the UART */
	xrm117x_port_write(port, XRM117XX_LCR_REG, XRM117XX_LCR_WORD_LEN_8);

	/* Enable the Rx and Tx FIFO */
	xrm117x_port_update(port, XRM117XX_EFCR_REG,
			      XRM117XX_EFCR_RXDISABLE_BIT |
			      XRM117XX_EFCR_TXDISABLE_BIT,
			      0);

	/* Enable RX, TX, CTS change interrupts */
	//val = XRM117XX_IER_RDI_BIT | XRM117XX_IER_THRI_BIT | XRM117XX_IER_CTSI_BIT;
	val = XRM117XX_IER_RDI_BIT | XRM117XX_IER_THRI_BIT | XRM117XX_IER_CTSI_BIT;	
	xrm117x_port_write(port, XRM117XX_IER_REG, val);

	
	return 0;
}

static void xrm117x_shutdown(struct uart_port *port)
{
    //printk("xrm117x_shutdown...\n");
	/* Disable all interrupts */
	xrm117x_port_write(port, XRM117XX_IER_REG, 0);
	
	/* Disable TX/RX */
	xrm117x_port_write(port, XRM117XX_EFCR_REG,
			     XRM117XX_EFCR_RXDISABLE_BIT |
			     XRM117XX_EFCR_TXDISABLE_BIT);

	xrm117x_power(port, 0);
	
		
}

static const char *xrm117x_type(struct uart_port *port)
{
	struct xrm117x_port *s = dev_get_drvdata(port->dev);
	return (port->type == PORT_SC16IS7XX) ? s->devtype->name : NULL;
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
	xrm117x_power(port, (state == UART_PM_STATE_ON) ? 1 : 0);
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
	struct xrm117x_port *s = container_of(chip, struct xrm117x_port,
						gpio);
	struct uart_port *port = &s->p[0].port;

	val = xrm117x_port_read(port, XRM117XX_IOSTATE_REG);

	return !!(val & BIT(offset));
}

static void xrm117x_gpio_set(struct gpio_chip *chip, unsigned offset, int val)
{
	struct xrm117x_port *s = container_of(chip, struct xrm117x_port,
						gpio);
	struct uart_port *port = &s->p[0].port;

	xrm117x_port_update(port, XRM117XX_IOSTATE_REG, BIT(offset),
			      val ? BIT(offset) : 0);
}

static int xrm117x_gpio_direction_input(struct gpio_chip *chip,
					  unsigned offset)
{
	struct xrm117x_port *s = container_of(chip, struct xrm117x_port,
						gpio);
	struct uart_port *port = &s->p[0].port;

	xrm117x_port_update(port, XRM117XX_IODIR_REG, BIT(offset), 0);

	return 0;
}

static int xrm117x_gpio_direction_output(struct gpio_chip *chip,
					   unsigned offset, int val)
{
	struct xrm117x_port *s = container_of(chip, struct xrm117x_port,
						gpio);
	struct uart_port *port = &s->p[0].port;

	xrm117x_port_update(port, XRM117XX_IOSTATE_REG, BIT(offset),
			      val ? BIT(offset) : 0);
	xrm117x_port_update(port, XRM117XX_IODIR_REG, BIT(offset),
			      BIT(offset));

	return 0;
}
#endif

#ifdef USE_OK335Dx_PLATFORM
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))
#define	GPIO_XR117X_IRQ	GPIO_TO_PIN(0,19)
//#define	GPIO_XR117X_IRQ	GPIO_TO_PIN(2,1)
#endif
static int xrm117x_probe(struct device *dev,
			   struct xrm117x_devtype *devtype,
			   int irq, unsigned long flags)
{
	unsigned long freq;
	int i, ret;
	struct xrm117x_port *s;
	
	/* Alloc port structure */
	s = devm_kzalloc(dev, sizeof(*s) +
			 sizeof(struct xrm117x_one) * devtype->nr_uart,
			 GFP_KERNEL);
	if (!s) {
		dev_err(dev, "Error allocating port structure\n");
		return -ENOMEM;
	}
    #if 0
	s->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(s->clk)) {
		if (pfreq)
			freq = *pfreq;
		else
			return PTR_ERR(s->clk);
	} else {
		freq = clk_get_rate(s->clk);
	}
	#else
    freq = 14745600;//Fixed the clk freq 
	#endif
	s->devtype = devtype;
	dev_set_drvdata(dev, s);
	/* Register UART driver */
	s->uart.owner		= THIS_MODULE;
	s->uart.dev_name	= "ttyXRM";
	s->uart.nr		= devtype->nr_uart;
	ret = uart_register_driver(&s->uart);
	if (ret) {
		dev_err(dev, "Registering UART driver failed\n");
		goto out_clk;
	}

#ifdef CONFIG_GPIOLIB
	if (devtype->nr_gpio) {
		/* Setup GPIO cotroller */
		s->gpio.owner		 = THIS_MODULE;
		s->gpio.dev		 = dev;
		s->gpio.label		 = dev_name(dev);
		s->gpio.direction_input	 = xrm117x_gpio_direction_input;
		s->gpio.get		 = xrm117x_gpio_get;
		s->gpio.direction_output = xrm117x_gpio_direction_output;
		s->gpio.set		 = xrm117x_gpio_set;
		s->gpio.base		 = -1;
		s->gpio.ngpio		 = devtype->nr_gpio;
		s->gpio.can_sleep	 = 1;
		ret = gpiochip_add(&s->gpio);
		if (ret)
			goto out_uart;
	}
#endif

	mutex_init(&s->mutex);
	mutex_init(&s->mutex_bus_access);
	//I2C_Bus_init();
	for (i = 0; i < devtype->nr_uart; ++i) {
		/* Initialize port data */
		s->p[i].port.line	= i;
		s->p[i].port.dev	= dev;
		s->p[i].port.irq	= irq;
		s->p[i].port.type	= PORT_SC16IS7XX;
		s->p[i].port.fifosize	= XRM117XX_FIFO_SIZE;
		s->p[i].port.flags	= UPF_FIXED_TYPE | UPF_LOW_LATENCY;
		s->p[i].port.iotype	= UPIO_PORT;
		s->p[i].port.uartclk	= freq;
		s->p[i].port.ops	= &xrm117x_ops;
		/* Disable all interrupts */
		xrm117x_port_write(&s->p[i].port, XRM117XX_IER_REG, 0);
		/* Disable TX/RX */
		xrm117x_port_write(&s->p[i].port, XRM117XX_EFCR_REG,
				     XRM117XX_EFCR_RXDISABLE_BIT |
				     XRM117XX_EFCR_TXDISABLE_BIT);
		
		s->p[i].msr_reg = xrm117x_port_read(&s->p[i].port, XRM117XX_MSR_REG);
		
		/* Initialize queue for start TX */
		INIT_WORK(&s->p[i].tx_work, xrm117x_wq_proc);
		/* Initialize queue for changing mode */
		INIT_WORK(&s->p[i].md_work, xrm117x_md_proc);

        INIT_WORK(&s->p[i].stop_rx_work, xrm117x_stop_rx_work_proc);
		INIT_WORK(&s->p[i].stop_tx_work, xrm117x_stop_tx_work_proc);
				
		/* Register port */
		uart_add_one_port(&s->uart, &s->p[i].port);
		/* Go to suspend mode */
		xrm117x_power(&s->p[i].port, 0);
	}

#ifdef USE_OK335Dx_PLATFORM	
	/* Setup interrupt */
	irq_set_irq_type(gpio_to_irq(GPIO_XR117X_IRQ), IRQF_TRIGGER_FALLING);
	
	irq = gpio_to_irq(GPIO_XR117X_IRQ);
#endif	
	ret = devm_request_threaded_irq(dev, irq, NULL, xrm117x_ist,
					IRQF_ONESHOT | IRQF_TRIGGER_FALLING | IRQF_SHARED | flags, dev_name(dev), s);
		
	printk("xrm117x_probe~ devm_request_threaded_irq =%d result:%d\n",irq, ret);
	
	if (!ret)
		return 0;

	mutex_destroy(&s->mutex);

#ifdef CONFIG_GPIOLIB
	if (devtype->nr_gpio)
		gpiochip_remove(&s->gpio);

out_uart:
#endif
	uart_unregister_driver(&s->uart);

out_clk:
	if (!IS_ERR(s->clk))
		/*clk_disable_unprepare(s->clk)*/;

	return ret;
}



static int xrm117x_remove(struct device *dev)
{
	struct xrm117x_port *s = dev_get_drvdata(dev);
	int i;
#ifdef CONFIG_GPIOLIB
	if (s->devtype->nr_gpio)
		gpiochip_remove(&s->gpio);
#endif

	for (i = 0; i < s->uart.nr; i++) {
		cancel_work_sync(&s->p[i].tx_work);
		cancel_work_sync(&s->p[i].md_work);
		cancel_work_sync(&s->p[i].stop_rx_work);
		cancel_work_sync(&s->p[i].stop_tx_work);
		uart_remove_one_port(&s->uart, &s->p[i].port);
		xrm117x_power(&s->p[i].port, 0);
	}

	mutex_destroy(&s->mutex);
	mutex_destroy(&s->mutex_bus_access);
	uart_unregister_driver(&s->uart);
	if (!IS_ERR(s->clk))
		/*clk_disable_unprepare(s->clk)*/;

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
	struct xrm117x_devtype *devtype = &xrm117x_devtype;
	unsigned long flags = 0;

	int ret;
	
	spi_dev =spi;
		
	ret = xrm117x_probe(&spi->dev, devtype,spi->irq, flags);
	
	return ret;
}


static int xrm117x_spi_remove(struct spi_device *spi)
{

      return xrm117x_remove(&spi->dev);
}

static struct spi_driver xrm117x_spi_uart_driver = {
       .driver = {
               .name   = XRM117XX_NAME_SPI,
               .bus    = &spi_bus_type,
               .owner  = THIS_MODULE,
       },
       .probe          = xrm117x_spi_probe,
       .remove         = __devexit_p(xrm117x_spi_remove),
     
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
	xrm117x_i2c_client = i2c; 
	ret = xrm117x_probe(&i2c->dev, devtype,i2c->irq, flags);
	return ret;
}

static int xrm117x_i2c_remove(struct i2c_client *client)
{
	return xrm117x_remove(&client->dev);
}


static const struct i2c_device_id xrm117x_i2c_id_table[] = {
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


#ifdef USE_SPI_MODE
static int __init xrm117x_init(void)
{
	return spi_register_driver(&xrm117x_spi_uart_driver);
}
module_init(xrm117x_init);

static void __exit xrm117x_exit(void)
{
	spi_unregister_driver(&xrm117x_spi_uart_driver);
}
module_exit(xrm117x_exit);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin xu <martin.xu@exar.com>");
MODULE_DESCRIPTION("XRM117XX serial driver");
