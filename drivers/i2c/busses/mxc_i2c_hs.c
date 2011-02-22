/*
 * Copyright (C) 2008-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
//#define USE_POLL
//#define DEBUG
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <mach/i2c.h>
#include "mxc_i2c_hs_reg.h"


typedef struct {
	struct device *dev;

	void __iomem		*reg_base_virt;
	unsigned long		reg_base_phy;
	int			irq;
	unsigned int		speed;
	struct completion	cmd_complete;

#define	CME_AL		0x1
#define	CME_AAS		0x2
#define	CME_NAK		0x4
#define CME_INCOMPLETE	0x08
	unsigned		cmd_err;
	unsigned		imr;
	struct clk		*ipg_clk;
	struct clk		*serial_clk;
	bool			low_power;

	struct i2c_msg		*msg;
	int			index;
	void (*i2c_clock_toggle)(void);
	struct i2c_adapter	adapter;
} mxc_i2c_hs;

#ifdef USE_POLL
void poll(mxc_i2c_hs *i2c_hs);
#endif

struct clk_div_table {
	int reg_value;
	int div;
};

static const struct clk_div_table i2c_clk_table[] = {
	{0x0, 16}, {0x1, 18}, {0x2, 20}, {0x3, 22},
	{0x20, 24}, {0x21, 26}, {0x22, 28}, {0x23, 30},
	{0x4, 32}, {0x5, 36}, {0x6, 40}, {0x7, 44},
	{0x24, 48}, {0x25, 52}, {0x26, 56}, {0x27, 60},
	{0x8, 64}, {0x9, 72}, {0xa, 80}, {0xb, 88},
	{0x28, 96}, {0x29, 104}, {0x2a, 112}, {0x2b, 120},
	{0xc, 128}, {0xd, 144}, {0xe, 160}, {0xf, 176},
	{0x2c, 192}, {0x2d, 208}, {0x2e, 224}, {0x2f, 240},
	{0x10, 256}, {0x11, 288}, {0x12, 320}, {0x13, 352},
	{0x30, 384}, {0x31, 416}, {0x32, 448}, {0x33, 480},
	{0x14, 512}, {0x15, 576}, {0x16, 640}, {0x17, 704},
	{0x34, 768}, {0x35, 832}, {0x36, 896}, {0x37, 960},
	{0x18, 1024}, {0x19, 1152}, {0x1a, 1280}, {0x1b, 1408},
	{0x38, 1536}, {0x39, 1664}, {0x3a, 1792}, {0x3b, 1920},
	{0x1c, 2048}, {0x1d, 2304}, {0x1e, 2560}, {0x1f, 2816},
	{0x3c, 3072}, {0x3d, 3328}, {0x3E, 3584}, {0x3F, 3840},
	{-1, -1}
};

extern void gpio_i2c_hs_inactive(void);
extern void gpio_i2c_hs_active(void);

static u16 reg_read(mxc_i2c_hs *i2c_hs, u32 reg_offset)
{
	return __raw_readw(i2c_hs->reg_base_virt + reg_offset);
}

static void reg_write(mxc_i2c_hs *i2c_hs, u32 reg_offset, u16 data)
{
	__raw_writew(data, i2c_hs->reg_base_virt + reg_offset);
}

static void reg_set_mask(mxc_i2c_hs *i2c_hs, u32 reg_offset, u16 mask)
{
	u16 value;

	value = reg_read(i2c_hs, reg_offset);
	value |= mask;
	reg_write(i2c_hs, reg_offset, value);
}
static void reg_clear_mask(mxc_i2c_hs *i2c_hs, u32 reg_offset, u16 mask)
{
	u16 value;

	value = reg_read(i2c_hs, reg_offset);
	value &= ~mask;
	reg_write(i2c_hs, reg_offset, value);
}

#if 0
static void reg_mod_mask(mxc_i2c_hs *i2c_hs, u32 reg_offset, u16 clear_mask, u16 set_mask)
{
	u16 value;
	value = reg_read(i2c_hs, reg_offset);
	value = (value & ~clear_mask) | set_mask;
	reg_write(i2c_hs, reg_offset, value);
}
#endif

static void mxci2c_hs_set_div(mxc_i2c_hs *i2c_hs)
{
	unsigned long clk_freq;
	int i;
	int div = -1;;

	clk_freq = clk_get_rate(i2c_hs->serial_clk);
	if (i2c_hs->speed) {
		div = (clk_freq + i2c_hs->speed - 1) / i2c_hs->speed;
		for (i = 0; i2c_clk_table[i].div >= 0; i++) {
			if (i2c_clk_table[i].div >= div) {
				div = i2c_clk_table[i].reg_value;
				reg_write(i2c_hs, HIFSFDR, div);
				break;
			}
		}
	}
}
#define CLKS_ALWAYS_ON
static int mxci2c_hs_enable(mxc_i2c_hs *i2c_hs)
{
	gpio_i2c_hs_active();
#ifndef CLKS_ALWAYS_ON
	clk_enable(i2c_hs->ipg_clk);
	clk_enable(i2c_hs->serial_clk);
#endif
	mxci2c_hs_set_div(i2c_hs);
	reg_write(i2c_hs, HICR, reg_read(i2c_hs, HICR) | HICR_HIEN);

	return 0;
}

static int mxci2c_hs_disable(mxc_i2c_hs *i2c_hs)
{
	reg_write(i2c_hs, HICR, reg_read(i2c_hs, HICR) & (~HICR_HIEN));
#ifndef CLKS_ALWAYS_ON
	clk_disable(i2c_hs->ipg_clk);
	clk_disable(i2c_hs->serial_clk);
#endif
	return 0;
}

static int mxci2c_hs_wait_for_bus_idle(mxc_i2c_hs *i2c_hs)
{
	u16 value;
	int retry = 1000;

	while (retry--) {
		value = reg_read(i2c_hs, HISR);
		if (value & HISR_HIBB) {
			udelay(1);
		} else {
			return 0;	/* success */
		}
	}
	dev_dbg(i2c_hs->dev, "%s: Bus Busy!\n", __func__);
	return -EREMOTEIO;
}

static int mxci2c_hs_stop(mxc_i2c_hs *i2c_hs)
{
	reg_clear_mask(i2c_hs, HICR, HICR_MSTA | HICR_HIIEN);
	return 0;
}

static void reset_rx_fifo(mxc_i2c_hs *i2c_hs, unsigned wm);
static void reset_tx_fifo(mxc_i2c_hs *i2c_hs);

static void mxci2c_bus_recovery(mxc_i2c_hs *i2c_hs)
{
	dev_err(i2c_hs->dev, "initiating i2c bus recovery\n");
	if (i2c_hs->i2c_clock_toggle)
		i2c_hs->i2c_clock_toggle();
	/* Send STOP */
	mxci2c_hs_stop(i2c_hs);
	reg_write(i2c_hs, HICR, 0);
	udelay(10);
	reg_write(i2c_hs, HICR, HICR_HIEN);
	reset_rx_fifo(i2c_hs, 1);
	reset_tx_fifo(i2c_hs);
	udelay(100);
}

static void reset_rx_fifo(mxc_i2c_hs *i2c_hs, unsigned wm)
{
	unsigned val;
	reg_write(i2c_hs, HIRFR, HIRFR_RFWM((wm-1)) | HIRFR_RFLSH);
	do {
		val = reg_read(i2c_hs, HIRFR);
	} while (val & HIRFR_RFLSH);
}

static int mxci2c_hs_read(mxc_i2c_hs *i2c_hs, int repeat_start,
			  struct i2c_msg *msg)
{
	unsigned hirdcr;
	unsigned hirfr;
	u16 hicr;
	int ret = 0;

	/*receive mode */
	reg_clear_mask(i2c_hs, HICR, HICR_TXAK | HICR_MTX | HICR_HIIEN);
	hirfr = (msg->len > 8) ? HIRFR_RFEN | HIRFR_RFWM((8-1)) :
		((msg->len > 1) ? HIRFR_RFEN | HIRFR_RFWM((msg->len - 1 - 1)) : HIRFR_RFEN);

	 /*RDCR*/
	hirdcr = HIRDCR_RDC(msg->len);
	reg_write(i2c_hs, HIRDCR, hirdcr);

	 /*FIFO*/
	reg_write(i2c_hs, HIRFR, hirfr);

	pr_debug("%s: HICR=%x HISR=%x HIRFR=%x HIRDCR=%x hirdcr=%x receiving %i bytes\n", __func__,
			reg_read(i2c_hs, HICR), reg_read(i2c_hs, HISR), reg_read(i2c_hs, HIRFR), reg_read(i2c_hs, HIRDCR), hirdcr, msg->len);


	hicr = HICR_MSTA | HICR_HIIEN;
	if (repeat_start)
		hicr |= HICR_RSTA;
	if (msg->len <= 1)
		hicr |= HICR_TXAK;	/* No ack for last received byte */

	if (!repeat_start) {
		ret = mxci2c_hs_wait_for_bus_idle(i2c_hs);
		if (ret)
			return ret;
	}
	reg_write(i2c_hs, HISR, HISR_HIAAS | HISR_HIAL | HISR_BTD | HISR_RDC_ZERO | HISR_TDC_ZERO | HISR_RXAK);
	i2c_hs->imr = HIIMR_RDF | HIIMR_HIAAS | HIIMR_HIAL | HIIMR_RXAK;
#ifdef USE_POLL
	reg_write(i2c_hs, HIIMR, 0xffff);
#else
	reg_write(i2c_hs, HIIMR, ~i2c_hs->imr);
#endif
	reg_set_mask(i2c_hs, HICR, hicr);	/*send start */
	return 0;
}

static void reset_tx_fifo(mxc_i2c_hs *i2c_hs)
{
	unsigned val;
	/* FIFO */
	reg_write(i2c_hs, HITFR, HITFR_TFWM(8 - HITFR_MAX_COUNT) | HITFR_TFLSH);
	do {
		val = reg_read(i2c_hs, HITFR);
	} while (val & HITFR_TFLSH);
}

static int mxci2c_hs_write(mxc_i2c_hs *i2c_hs, int repeat_start,
			   struct i2c_msg *msg)
{
	int i;
	unsigned hitdcr;
	u16 hicr;
	int ret = 0;

	/*
	 * clear transmit: if transmitting 2 messages are sent
	 * without stop, MTX needs cleared to show where 2nd starts
	 */
	reg_clear_mask(i2c_hs, HICR, HICR_MTX | HICR_HIIEN);

	/* TDCR */
	hitdcr = HITDCR_TDC(msg->len);
	reg_write(i2c_hs, HITDCR, hitdcr);
	reg_write(i2c_hs, HITFR, HITFR_TFEN | HITFR_TFWM(0));

	pr_debug("%s: HICR=%x HISR=%x HITFR=%x HITDCR=%x hitdcr=%x sending %i bytes\n", __func__,
			reg_read(i2c_hs, HICR), reg_read(i2c_hs, HISR), reg_read(i2c_hs, HITFR), reg_read(i2c_hs, HITDCR), hitdcr, msg->len);

	i2c_hs->index = (msg->len > HITFR_MAX_COUNT) ? HITFR_MAX_COUNT : msg->len;

	for (i = 0; i < i2c_hs->index; i++) {
		reg_write(i2c_hs, HITDR, msg->buf[i]);
	}

	if (!repeat_start) {
		ret = mxci2c_hs_wait_for_bus_idle(i2c_hs);
		if (ret)
			return ret;
	}
	hicr = HICR_MSTA | HICR_HIIEN | HICR_MTX;
	if (repeat_start)
		hicr |= HICR_RSTA;
	reg_write(i2c_hs, HISR, HISR_HIAAS | HISR_HIAL | HISR_BTD | HISR_RDC_ZERO | HISR_TDC_ZERO | HISR_RXAK);
	i2c_hs->imr = HIIMR_TDE | HIIMR_HIAAS | HIIMR_HIAL | HIIMR_RXAK;
#ifdef USE_POLL
	reg_write(i2c_hs, HIIMR, 0xffff);
#else
	reg_write(i2c_hs, HIIMR, ~i2c_hs->imr);
#endif
	reg_set_mask(i2c_hs, HICR, hicr);	/*send start */
	return 0;
}

static int mxci2c_hs_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
			  int num)
{
	int i;
	int ret = -EIO;
	mxc_i2c_hs *i2c_hs = (mxc_i2c_hs *) (i2c_get_adapdata(adap));

	if (i2c_hs->low_power) {
		dev_err(&adap->dev, "I2C Device in low power mode\n");
		return -EREMOTEIO;
	}

	if (num < 1) {
		return 0;
	}

	mxci2c_hs_enable(i2c_hs);
	reset_rx_fifo(i2c_hs, 1);
	reset_tx_fifo(i2c_hs);

	i = 0;
	for (;;) {
		struct i2c_msg *msg = &msgs[i];
		int read;
		/* force write for a 0 length transaction*/
		if (!msg->len)
			msg->flags &= ~I2C_M_RD;
		read = msg->flags & I2C_M_RD;

		i2c_hs->cmd_err = 0;
		INIT_COMPLETION(i2c_hs->cmd_complete);
		i2c_hs->msg = msg;
		i2c_hs->index = 0;

		/*set address */
		reg_write(i2c_hs, HIMADR, HIMADR_LSB_ADR(msg->addr));

		if (read) {
			ret = mxci2c_hs_read(i2c_hs, i ? 1 : 0, msg);
		} else {
			ret = mxci2c_hs_write(i2c_hs, i ? 1 : 0, msg);
		}
		if (!ret) {
#ifdef USE_POLL
			unsigned retry = 10000;
			while (retry--) {
				udelay(10);
				poll(i2c_hs);
				pr_debug("%s: i2c_hs->msg=%p\n", __func__, i2c_hs->msg);
				if (!i2c_hs->msg) {
#ifdef DEBUG
					unsigned hisr;
					unsigned hirfr = reg_read(i2c_hs, HIRFR);

					if (hirfr & HIRFR_RFEN)
						reg_write(i2c_hs, HIRFR, HIRFR_RFEN);
					msleep(2);
					hisr = reg_read(i2c_hs, HISR);
					if (!(hisr & HISR_RDF))
						break;
					dev_err(i2c_hs->dev, "read fifo not empty!! hisr=%x\n", hisr);
#else
					break;
#endif
				}
			}
#else
			int r = wait_for_completion_interruptible_timeout(&i2c_hs->cmd_complete,
				i2c_hs->adapter.timeout);
			if (r == 0) {
				dev_err(i2c_hs->dev, "controller timed out\n");
				ret = -ETIMEDOUT;
			} else {
				if (i2c_hs->msg) {
					/* signal stopped us, wait another jiffy */
					wait_for_completion_timeout(&i2c_hs->cmd_complete,1);
				}
			}
#endif
		}
		if (i2c_hs->msg) {
			i2c_hs->cmd_err |= CME_INCOMPLETE;
			i2c_hs->msg = NULL;
		}
		if (i2c_hs->cmd_err) {
			ret = (i2c_hs->cmd_err & CME_AL) ? -EREMOTEIO : -EIO;
		} else {
			ret = 0;
		}
		if (ret) {
			printk(KERN_ERR "%s: ret=%i %s HICR=%x HISR=%x HITFR=%x HIRFR=%x cmd_err=%x sent %i of %i\n", __func__, ret, read ? "read" : "write",
					reg_read(i2c_hs, HICR), reg_read(i2c_hs, HISR), reg_read(i2c_hs, HITFR), reg_read(i2c_hs, HIRFR),
					i2c_hs->cmd_err, i2c_hs->index, msg->len);
			msleep(2);
			printk(KERN_ERR "%s: HISR=%x\n", __func__, reg_read(i2c_hs, HISR));
			if ((ret == -EREMOTEIO) || (ret == -ETIMEDOUT)) {
				/* This hardware is so buggy, let's reset the bus*/
				mxci2c_hs_stop(i2c_hs);
				mxci2c_bus_recovery(i2c_hs);
			}
		} else {
			if (i2c_hs->index != msg->len) {
				printk(KERN_ERR "%s: %s byte cnt error %i != %i HICR=%x HISR=%x HITFR=%x HIRFR=%x sent %i of %i\n", __func__,
						read ? "read" : "write", i2c_hs->index, msg->len,
						reg_read(i2c_hs, HICR), reg_read(i2c_hs, HISR), reg_read(i2c_hs, HITFR), reg_read(i2c_hs, HIRFR),
						i2c_hs->index, msg->len);
				ret = -EIO;
			} else {
				pr_debug("%s: ret=%i %s HICR=%x HISR=%x HITFR=%x HIRFR=%x sent %i of %i\n", __func__, ret, read ? "read" : "write",
					reg_read(i2c_hs, HICR), reg_read(i2c_hs, HISR), reg_read(i2c_hs, HITFR), reg_read(i2c_hs, HIRFR),
					i2c_hs->index, msg->len);
			}
		}
		udelay(100);	/* 15 is too short */
		if (ret < 0)
			break;
		i++;
		if (i >= num)
			break;
	}
	mxci2c_hs_stop(i2c_hs);
	msleep(1);
	mxci2c_hs_disable(i2c_hs);

	if (ret < 0)
		return ret;

	return i;
}

static u32 mxci2c_hs_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

/*!
 * Stores the pointers for the i2c algorithm functions. The algorithm functions
 * is used by the i2c bus driver to talk to the i2c bus
 */
static struct i2c_algorithm mxci2c_hs_algorithm = {
	.master_xfer = mxci2c_hs_xfer,
	.functionality = mxci2c_hs_func
};

static void isr_read_fifo_data(mxc_i2c_hs *i2c_hs)
{
	unsigned hirfr;
	unsigned rfc;
	unsigned i = i2c_hs->index;
	unsigned next = 0;
	unsigned len = (i2c_hs->msg) ? i2c_hs->msg->len : i;
	unsigned rem;
	reg_write(i2c_hs, HISR, HISR_BTD);
	hirfr = reg_read(i2c_hs, HIRFR);
	rfc = (hirfr & HIRFR_RFEN) ? (hirfr >> 8) & 0xf : 1;
	if (rfc > 8)
		rfc = 8;

	rem = len - i;
	if (rem > rfc) {
		/*
		 * More to be read next pass
		 */
		if (rem > 9) {
			if (rfc > (rem - 9))
				rfc = rem - 9;
		}
		next = rem - rfc;
		rem = rfc;
	} else {
		/*
		 * This fifo read will complete the transaction
		 * But due to a hardware bug, don't set TXAK yet.
		 * It should have been set already, but if interrupt
		 * latency causes it not to have been, then we
		 * will have to read extra bytes to ensure the last
		 * byte received was NAKed. If the fifo is
		 * full with the last ack bit already transmitted
		 * and TXAK is set before reading from the fifo,
		 * then NO more data will be received and the bus
		 * would need to be reset.
		 *
		 */
	}
	if (rem > 0) {
		rfc -= rem;
		if (next == 1) {
			int loop = 0;
			/*
			 * This fifo full interrupt can happen before the 9th
			 * ACK bit is sent. Delay up to 1 bit time waiting
			 * for BTD to be set, indicating ACK has been sent.
			 */
			for (;;) {
				unsigned hisr = reg_read(i2c_hs, HISR);
				if (hisr & HISR_BTD)
					break;
				if (loop++ > 10) {
					pr_debug("%s: timeout waiting for BTD, hisr=%x\n", __func__, hisr);
					break;
				}
				udelay(1);
			}
		}
		if (next <= 1) {
			/*
			 * The fifo is full with 1 byte left to read.
			 * Removing 1 from the fifo will allow the last byte to
			 * start reading. After it has started, we can set the
			 * TXAK bit. If TXAK is set before the byte has started
			 * being accepted, a hardware bug will cause the last
			 * byte to not be accepted, leaving the bus in a bad
			 * state.
			 */
			i2c_hs->msg->buf[i++] = reg_read(i2c_hs, HIRDR);
			reg_set_mask(i2c_hs, HICR, HICR_TXAK);
			pr_debug("%s: HICR=%x HISR=%x HIRFR=%x HIRDCR=%x "
				"receiving data[%i] = %02x of %i\n", __func__,
				reg_read(i2c_hs, HICR), reg_read(i2c_hs, HISR),
				hirfr, reg_read(i2c_hs, HIRDCR), i-1,
				i2c_hs->msg->buf[i-1], len);
			rem--;
		}
		while (rem) {
			i2c_hs->msg->buf[i++] = reg_read(i2c_hs, HIRDR);
			pr_debug("%s: HICR=%x HISR=%x HIRFR=%x HIRDCR=%x "
				"receiving data[%i] = %02x of %i\n", __func__,
				reg_read(i2c_hs, HICR), reg_read(i2c_hs, HISR),
				hirfr, reg_read(i2c_hs, HIRDCR), i-1,
				i2c_hs->msg->buf[i-1], len);
			rem--;
		}
		i2c_hs->index = i;
		if (hirfr & HIRFR_RFEN)
			if (next && (next < 8))
				reg_write(i2c_hs, HIRFR, HIRFR_RFEN | HIRFR_RFWM((next-1)));
	}
	if (rfc) {
		/* unexpected data in fifo, hardware bug or message gone */
		while (rfc) {
			unsigned data = reg_read(i2c_hs, HIRDR);
			printk(KERN_ERR "%s: unexpected data %x\n", __func__, data);
			rfc--;
		}
		reg_set_mask(i2c_hs, HICR, HICR_TXAK);
	}
	if (!next) {
		i2c_hs->msg = NULL;
		complete(&i2c_hs->cmd_complete);
	}
}

static void isr_write_fifo_data(mxc_i2c_hs *i2c_hs)
{
	unsigned rem = 0;
	int i = i2c_hs->index;
	unsigned hitfr = reg_read(i2c_hs, HITFR);
	unsigned tfc = (hitfr >> 8) & 0xf;
	int next = 0;
	struct i2c_msg *msg = i2c_hs->msg;
	pr_debug("%s: HITFR=%x HITDCR=%x\n", __func__, hitfr, reg_read(i2c_hs, HITDCR));
	if (tfc > 8)
		tfc = 8;
	tfc = 8 - tfc;
	if (msg)
		rem = i2c_hs->msg->len - i;
	if (rem > tfc) {
		next = rem - tfc;
		rem = tfc;
	}
	if (rem > 0) {
		tfc -= rem;
		while (rem) {
			reg_write(i2c_hs, HITDR, msg->buf[i++]);
			rem--;
		}
		i2c_hs->index = i;
	}
	if ((!next) && (tfc == 8)) {
		i2c_hs->msg = NULL;
		complete(&i2c_hs->cmd_complete);
		i2c_hs->imr &= ~HIIMR_TDE;
		pr_debug("%s: write complete, hiimr=%x\n", __func__, reg_read(i2c_hs, HIIMR));
#ifndef USE_POLL
		reg_write(i2c_hs, HIIMR, ~i2c_hs->imr);
#endif
	}
}

static int isr_do_work(mxc_i2c_hs *i2c_hs, unsigned hisr)
{
	dev_dbg(i2c_hs->dev, "%s: hisr=0x%x hiimr=0x%x\n", __func__, hisr, reg_read(i2c_hs, HIIMR));
	if (i2c_hs->imr & hisr & HISR_RDF) {
		isr_read_fifo_data(i2c_hs);
	} else if (i2c_hs->imr & hisr & HISR_TDE) {
		isr_write_fifo_data(i2c_hs);
	} else if (hisr & (HISR_HIAAS | HISR_HIAL | HISR_BTD |
			HISR_RDC_ZERO | HISR_TDC_ZERO | HISR_RXAK)) {
		reg_write(i2c_hs, HISR, hisr);
		if (hisr & (HISR_HIAAS | HISR_HIAL | HISR_RXAK)) {
			if (hisr & HISR_HIAL) {
				i2c_hs->cmd_err |= CME_AL;
			} else if (hisr & HISR_HIAAS) {
				i2c_hs->cmd_err |= CME_AAS;
			} else {
				i2c_hs->cmd_err |= CME_NAK;
			}
			printk(KERN_ERR "%s: HICR=%x HISR=%x HITFR=%x HIRFR=%x HIIMR=%x cmd_err=%x sent %i of %i\n", __func__,
					reg_read(i2c_hs, HICR), hisr, reg_read(i2c_hs, HITFR), reg_read(i2c_hs, HIRFR), reg_read(i2c_hs, HIIMR),
					i2c_hs->cmd_err, i2c_hs->index, (i2c_hs->msg) ? i2c_hs->msg->len : 0);
			i2c_hs->msg = NULL;
			complete(&i2c_hs->cmd_complete);
		}
	} else {
		if (hisr & HISR_RDF) {
			unsigned hiimr = reg_read(i2c_hs, HIIMR);
			if (!(hiimr & HIIMR_RDF)) {
				reg_write(i2c_hs, HIIMR, hiimr | HIIMR_RDF | 0xff00);
				pr_debug("%s: disabling RDF interrupt, hiimr=%x\n", __func__, hiimr);
				return -1;
			}
		}
		if (hisr & HISR_TDE) {
			unsigned hiimr = reg_read(i2c_hs, HIIMR);
			if (!(hiimr & HIIMR_TDE)) {
				reg_write(i2c_hs, HIIMR, hiimr | HIIMR_TDE | 0xff00);
				pr_debug("%s: disabling TDE interrupt, hiimr=%x\n", __func__, hiimr);
				return -1;
			}
		}
		return 1;
	}
	return 0;
}

#ifdef USE_POLL
void poll(mxc_i2c_hs *i2c_hs)
{
	unsigned hisr;
#ifdef DEBUG
	msleep(5);
#endif
	hisr = reg_read(i2c_hs, HISR);
#ifdef DEBUG
	msleep(5);
#endif
	isr_do_work(i2c_hs, hisr);
}
#endif

/*
 * Interrupt service routine. This gets called whenever an I2C interrupt
 * occurs.
 */
static irqreturn_t mxci2c_hs_isr(int this_irq, void *dev_id)
{
	mxc_i2c_hs *i2c_hs = dev_id;
	int count = 0;
	unsigned hisr;

	for (;;) {
		hisr = reg_read(i2c_hs, HISR);
		if (isr_do_work(i2c_hs, hisr))
			break;
		if (count++ == 100) {
			dev_warn(i2c_hs->dev, "Too much work in one IRQ\n");
			break;
		}
	}
	return IRQ_HANDLED;
}

static int mxci2c_hs_probe(struct platform_device *pdev)
{
	mxc_i2c_hs *i2c_hs;
	struct imxi2c_platform_data *i2c_plat_data = pdev->dev.platform_data;
	struct resource *res;
	int id = pdev->id;
	int ret = 0;
	struct i2c_adapter *adap;

	i2c_hs = kzalloc(sizeof(mxc_i2c_hs), GFP_KERNEL);
	if (!i2c_hs) {
		return -ENOMEM;
	}

	init_completion(&i2c_hs->cmd_complete);
	i2c_hs->dev = get_device(&pdev->dev);

	i2c_hs->speed = i2c_plat_data->bitrate;
	i2c_hs->i2c_clock_toggle = i2c_plat_data->i2c_clock_toggle;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		ret = -ENODEV;
		goto err1;
	}
	i2c_hs->reg_base_virt = ioremap(res->start, res->end - res->start + 1);
	i2c_hs->reg_base_phy = res->start;

	i2c_hs->ipg_clk = clk_get(&pdev->dev, "hsi2c_clk");
	i2c_hs->serial_clk = clk_get(&pdev->dev, "hsi2c_serial_clk");

	/*
	 * Request the I2C interrupt
	 */
	i2c_hs->irq = platform_get_irq(pdev, 0);
	if (i2c_hs->irq < 0) {
		ret = i2c_hs->irq;
		goto err1;
	}
	ret = request_irq(i2c_hs->irq, mxci2c_hs_isr, 0, pdev->name, i2c_hs);
	if (ret) {
		dev_err(&pdev->dev, "failure requesting irq %i\n", i2c_hs->irq);
		goto err1;
	}

	i2c_hs->low_power = false;

	/*
	 * Set the adapter information
	 */
	adap = &i2c_hs->adapter;
	strlcpy(adap->name, pdev->name, 48);
	adap->id = adap->nr = id;
	adap->algo = &mxci2c_hs_algorithm;
	adap->timeout = (1*HZ);
	platform_set_drvdata(pdev, i2c_hs);
	i2c_set_adapdata(adap, i2c_hs);
	ret = i2c_add_numbered_adapter(adap);
	if (ret < 0) {
		goto err2;
	}

#ifdef CLKS_ALWAYS_ON
	clk_enable(i2c_hs->ipg_clk);
	clk_enable(i2c_hs->serial_clk);
#endif
	printk(KERN_INFO "MXC HS I2C driver\n");
	return 0;

      err2:
	platform_set_drvdata(pdev, NULL);
	free_irq(i2c_hs->irq, i2c_hs);
      err1:
	dev_err(&pdev->dev, "failed to probe high speed i2c adapter\n");
	iounmap(i2c_hs->reg_base_virt);
	put_device(&pdev->dev);
	kfree(i2c_hs);
	return ret;
}

static int mxci2c_hs_suspend(struct platform_device *pdev, pm_message_t state)
{
	mxc_i2c_hs *i2c_hs = platform_get_drvdata(pdev);

	if (i2c_hs == NULL) {
		return -1;
	}

	/* Prevent further calls to be processed */
	i2c_hs->low_power = true;

	gpio_i2c_hs_inactive();

	return 0;
}

static int mxci2c_hs_resume(struct platform_device *pdev)
{
	mxc_i2c_hs *i2c_hs = platform_get_drvdata(pdev);

	if (i2c_hs == NULL)
		return -1;

	i2c_hs->low_power = false;
	gpio_i2c_hs_active();

	return 0;
}

static int mxci2c_hs_remove(struct platform_device *pdev)
{
	mxc_i2c_hs *i2c_hs = platform_get_drvdata(pdev);

	i2c_del_adapter(&i2c_hs->adapter);
	gpio_i2c_hs_inactive();
	platform_set_drvdata(pdev, NULL);
	iounmap(i2c_hs->reg_base_virt);
#ifdef CLKS_ALWAYS_ON
	clk_disable(i2c_hs->ipg_clk);
	clk_disable(i2c_hs->serial_clk);
#endif
	kfree(i2c_hs);
	return 0;
}

static struct platform_driver mxci2c_hs_driver = {
	.driver = {
		   .name = "mxc_i2c_hs",
		   .owner = THIS_MODULE,
		   },
	.probe = mxci2c_hs_probe,
	.remove = mxci2c_hs_remove,
	.suspend = mxci2c_hs_suspend,
	.resume = mxci2c_hs_resume,
};

/*!
 * Function requests the interrupts and registers the i2c adapter structures.
 *
 * @return The function returns 0 on success and a non-zero value on failure.
 */
static int __init mxci2c_hs_init(void)
{
	/* Register the device driver structure. */
	return platform_driver_register(&mxci2c_hs_driver);
}

/*!
 * This function is used to cleanup all resources before the driver exits.
 */
static void __exit mxci2c_hs_exit(void)
{
	platform_driver_unregister(&mxci2c_hs_driver);
}

subsys_initcall(mxci2c_hs_init);
module_exit(mxci2c_hs_exit);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MXC HIGH SPEED I2C driver");
MODULE_LICENSE("GPL");
