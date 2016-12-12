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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_data/i2c-imx.h>
#include "i2c-imx-hs.h"

/* Default value */
#define IMX_I2C_BIT_RATE	100000	/* 100kHz */

struct imx_i2c_hs_struct {
	struct device *dev;

	void __iomem		*base;
	struct gpio_desc	*scl_gpio;
	struct gpio_desc	*sda_gpio;
	unsigned int		speed;
	struct completion	cmd_complete;
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_scl_gpio;
	struct pinctrl_state	*pins_sda_gpio;
	unsigned		max_stop_delay;

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
	struct i2c_adapter	adapter;
};

#ifdef USE_POLL
void poll(struct imx_i2c_hs_struct *i2c_imx);
#endif

static const u16 i2c_clk_table[] = {
	 16,  18,  20,  22,  32,  36,  40,  44,   64,   72,   80,   88,  128,  144,  160,  176,
	256, 288, 320, 352, 512, 576, 640, 704, 1024, 1152, 1280, 1408, 2048, 2304, 2560, 2816,
	 24,  26,  28,  30,  48,  52,  56,  60,   96,  104,  112,  120,  192,  208,  224,  240,
	384, 416, 448, 480, 768, 832, 896, 960, 1536, 1664, 1792, 1920, 3072, 3328, 3584, 3840
};

static u16 reg_read(struct imx_i2c_hs_struct *i2c_imx, u32 reg_offset)
{
	return __raw_readw(i2c_imx->base + reg_offset);
}

static void reg_write(struct imx_i2c_hs_struct *i2c_imx, u32 reg_offset, u16 data)
{
	__raw_writew(data, i2c_imx->base + reg_offset);
}

static void reg_set_mask(struct imx_i2c_hs_struct *i2c_imx, u32 reg_offset, u16 mask)
{
	u16 value;

	value = reg_read(i2c_imx, reg_offset);
	value |= mask;
	reg_write(i2c_imx, reg_offset, value);
}
static void reg_clear_mask(struct imx_i2c_hs_struct *i2c_imx, u32 reg_offset, u16 mask)
{
	u16 value;

	value = reg_read(i2c_imx, reg_offset);
	value &= ~mask;
	reg_write(i2c_imx, reg_offset, value);
}

#if 0
static void reg_mod_mask(struct imx_i2c_hs_struct *i2c_imx, u32 reg_offset, u16 clear_mask, u16 set_mask)
{
	u16 value;
	value = reg_read(i2c_imx, reg_offset);
	value = (value & ~clear_mask) | set_mask;
	reg_write(i2c_imx, reg_offset, value);
}
#endif

static void i2c_imx_hs_set_div(struct imx_i2c_hs_struct *i2c_imx)
{
	unsigned long clk_freq;
	int i;
	int div, entry;
	unsigned best_index = ARRAY_SIZE(i2c_clk_table) - 1;
	int best_div = i2c_clk_table[best_index];

	clk_freq = clk_get_rate(i2c_imx->serial_clk);
	if (i2c_imx->speed) {
		div = (clk_freq + i2c_imx->speed - 1) / i2c_imx->speed;
		for (i = 0; i < ARRAY_SIZE(i2c_clk_table); i++) {
			entry = i2c_clk_table[i];
			if (div <= entry) {
				if (best_div > entry) {
					best_div = entry;
					best_index = i;
					if (best_div == div)
						break;
				}
			}
		}
	}
	pr_debug("%s: reg val=0x%x, div=%d, freq=%ld, desired=%d\n", __func__,
		best_index, best_div, clk_freq / best_div, i2c_imx->speed);
	reg_write(i2c_imx, HIFSFDR, best_index);
}

#define CLKS_ALWAYS_ON
static int i2c_imx_hs_enable(struct imx_i2c_hs_struct *i2c_imx)
{
#ifndef CLKS_ALWAYS_ON
	clk_prepare_enable(i2c_imx->ipg_clk);
	clk_prepare_enable(i2c_imx->serial_clk);
#endif
	i2c_imx_hs_set_div(i2c_imx);
	reg_write(i2c_imx, HICR, reg_read(i2c_imx, HICR) | HICR_HIEN);

	return 0;
}

static int i2c_imx_hs_disable(struct imx_i2c_hs_struct *i2c_imx)
{
	reg_write(i2c_imx, HICR, reg_read(i2c_imx, HICR) & (~HICR_HIEN));
#ifndef CLKS_ALWAYS_ON
	clk_disable_unprepare(i2c_imx->ipg_clk);
	clk_disable_unprepare(i2c_imx->serial_clk);
#endif
	return 0;
}

static int i2c_imx_hs_wait_for_bus_idle(struct imx_i2c_hs_struct *i2c_imx)
{
	u16 value;
	unsigned long orig_jiffies = jiffies;

	while (1) {
		value = reg_read(i2c_imx, HISR);
		if (!(value & HISR_HIBB))
			break;
		if (time_after(jiffies, orig_jiffies + msecs_to_jiffies(500))) {
			dev_info(&i2c_imx->adapter.dev,
				"<%s> I2C bus is busy\n", __func__);
			return -ETIMEDOUT;
		}
		schedule();
	}
	return 0;	/* success */
}

static int i2c_imx_hs_stop(struct imx_i2c_hs_struct *i2c_imx)
{
#if 1
	unsigned tmp;
	unsigned long orig_jiffies = jiffies;
	unsigned diff = 0;

	/*
	 * Wait while clock is being stretched by device
	 * so that stop is not lost.
	 * We are currently driving SCL low because the controller
	 * does not know that we have done the last byte.
	 * So, change to gpio mode to keep SDA low until after clk is released.
	 */
	gpiod_direction_output(i2c_imx->sda_gpio, 0);
	pinctrl_select_state(i2c_imx->pinctrl, i2c_imx->pins_sda_gpio);
	reg_clear_mask(i2c_imx, HICR, HICR_MSTA | HICR_HIIEN);

	while (1) {
		tmp = gpiod_get_value(i2c_imx->scl_gpio);
		if (tmp)
			break;
		if (time_after(jiffies, orig_jiffies + msecs_to_jiffies(10))) {
			dev_info(&i2c_imx->adapter.dev,
				"<%s> I2C bus is busy\n", __func__);
			break;
		}
		schedule();
		diff = 1;
	}

	if (diff) {
		diff = jiffies - orig_jiffies;
		if (i2c_imx->max_stop_delay < diff) {
			i2c_imx->max_stop_delay = diff;
			pr_info("%s: new record for delay before stop %d\n", __func__, diff);
		}
	}

	udelay(100);	/* SDA guaranteed low for 100 us after SCL goes high*/
        gpiod_direction_input(i2c_imx->sda_gpio);
	pinctrl_select_state(i2c_imx->pinctrl, i2c_imx->pins_default);
#else
	reg_clear_mask(i2c_imx, HICR, HICR_MSTA | HICR_HIIEN);
#endif

	return 0;
}

static void reset_rx_fifo(struct imx_i2c_hs_struct *i2c_imx, unsigned wm);
static void reset_tx_fifo(struct imx_i2c_hs_struct *i2c_imx);

int force_idle_bus(struct imx_i2c_hs_struct *i2c_imx)
{
        int i;
        int sda, scl;
	unsigned long orig_jiffies;
        int ret = 0;

        gpiod_direction_input(i2c_imx->sda_gpio);
        gpiod_direction_input(i2c_imx->scl_gpio);

	ret = pinctrl_select_state(i2c_imx->pinctrl, i2c_imx->pins_scl_gpio);
	if (ret < 0)
		goto exit;

        sda = gpiod_get_value(i2c_imx->sda_gpio);
        scl = gpiod_get_value(i2c_imx->scl_gpio);
        if ((sda & scl) == 1)
                goto exit;              /* Bus is idle already */

	dev_info(&i2c_imx->adapter.dev,
		"sda=%d scl=%d\n", sda, scl);
        /* Send high and low on the SCL line */
        for (i = 0; i < 9; i++) {
                gpiod_direction_output(i2c_imx->scl_gpio, 0);
                udelay(50);
                gpiod_direction_input(i2c_imx->scl_gpio);
                udelay(50);
        }
	orig_jiffies = jiffies;
        while (1) {
                sda = gpiod_get_value(i2c_imx->sda_gpio);
                scl = gpiod_get_value(i2c_imx->scl_gpio);
                if ((sda & scl) == 1)
                        break;
		if (time_after(jiffies, orig_jiffies + msecs_to_jiffies(200))) {
                        ret = -EBUSY;
                        dev_err(&i2c_imx->adapter.dev,
                        	"failed to clear bus, sda=%d scl=%d\n",
                                sda, scl);
                        break;
                }
		schedule();
        }
exit:
	pinctrl_select_state(i2c_imx->pinctrl, i2c_imx->pins_default);
        return ret;
}

static void mxci2c_bus_recovery(struct imx_i2c_hs_struct *i2c_imx)
{
	dev_err(&i2c_imx->adapter.dev, "initiating i2c bus recovery\n");

	force_idle_bus(i2c_imx);

	/* Send STOP */
	i2c_imx_hs_stop(i2c_imx);
	reg_write(i2c_imx, HICR, 0);
	udelay(10);
	reg_write(i2c_imx, HICR, HICR_HIEN);
	reset_rx_fifo(i2c_imx, 1);
	reset_tx_fifo(i2c_imx);
	udelay(100);
}

static void reset_rx_fifo(struct imx_i2c_hs_struct *i2c_imx, unsigned wm)
{
	unsigned val;
	reg_write(i2c_imx, HIRFR, HIRFR_RFWM((wm-1)) | HIRFR_RFLSH);
	do {
		val = reg_read(i2c_imx, HIRFR);
	} while (val & HIRFR_RFLSH);
}

static int i2c_imx_hs_read(struct imx_i2c_hs_struct *i2c_imx, int repeat_start,
			  struct i2c_msg *msg)
{
	unsigned hirdcr;
	unsigned hirfr;
	u16 hicr;
	int ret = 0;

	/*receive mode */
	if (repeat_start)
		udelay(30);

	reg_clear_mask(i2c_imx, HICR, HICR_TXAK | HICR_MTX | HICR_HIIEN);
	hirfr = (msg->len > 8) ? HIRFR_RFEN | HIRFR_RFWM((8-1)) :
		((msg->len > 1) ? HIRFR_RFEN | HIRFR_RFWM((msg->len - 1 - 1)) : HIRFR_RFEN);

	 /*RDCR*/
	hirdcr = HIRDCR_RDC(msg->len);
	reg_write(i2c_imx, HIRDCR, hirdcr);

	 /*FIFO*/
	reg_write(i2c_imx, HIRFR, hirfr);

	pr_debug("%s: HICR=%x HISR=%x HIRFR=%x HIRDCR=%x hirdcr=%x receiving %i bytes\n", __func__,
			reg_read(i2c_imx, HICR), reg_read(i2c_imx, HISR), reg_read(i2c_imx, HIRFR), reg_read(i2c_imx, HIRDCR), hirdcr, msg->len);


	hicr = HICR_MSTA | HICR_HIIEN;
	if (repeat_start)
		hicr |= HICR_RSTA;

	if (msg->len <= 1)
		hicr |= HICR_TXAK;	/* No ack for last received byte */

	if (!repeat_start) {
		ret = i2c_imx_hs_wait_for_bus_idle(i2c_imx);
		if (ret)
			return ret;
	}
	reg_write(i2c_imx, HISR, HISR_HIAAS | HISR_HIAL | HISR_BTD | HISR_RDC_ZERO | HISR_TDC_ZERO | HISR_RXAK);
	i2c_imx->imr = HIIMR_RDF | HIIMR_HIAAS | HIIMR_HIAL | HIIMR_RXAK;
#ifdef USE_POLL
	reg_write(i2c_imx, HIIMR, 0xffff);
#else
	reg_write(i2c_imx, HIIMR, ~i2c_imx->imr);
#endif
	reg_set_mask(i2c_imx, HICR, hicr);	/*send start */
	return 0;
}

static void reset_tx_fifo(struct imx_i2c_hs_struct *i2c_imx)
{
	unsigned val;
	/* FIFO */
	reg_write(i2c_imx, HITFR, HITFR_TFWM(8 - HITFR_MAX_COUNT) | HITFR_TFLSH);
	do {
		val = reg_read(i2c_imx, HITFR);
	} while (val & HITFR_TFLSH);
}

static int i2c_imx_hs_write(struct imx_i2c_hs_struct *i2c_imx, int repeat_start,
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
	reg_clear_mask(i2c_imx, HICR, HICR_MTX | HICR_HIIEN);

	/* TDCR */
	hitdcr = HITDCR_TDC(msg->len);
	reg_write(i2c_imx, HITDCR, hitdcr);
	reg_write(i2c_imx, HITFR, HITFR_TFEN | HITFR_TFWM(0));

	pr_debug("%s: HICR=%x HISR=%x HITFR=%x HITDCR=%x hitdcr=%x sending %i bytes\n", __func__,
			reg_read(i2c_imx, HICR), reg_read(i2c_imx, HISR), reg_read(i2c_imx, HITFR), reg_read(i2c_imx, HITDCR), hitdcr, msg->len);

	i2c_imx->index = (msg->len > HITFR_MAX_COUNT) ? HITFR_MAX_COUNT : msg->len;

	for (i = 0; i < i2c_imx->index; i++) {
		reg_write(i2c_imx, HITDR, msg->buf[i]);
		pr_debug("%s:write %02x\n", __func__, msg->buf[i]);
	}

	if (!repeat_start) {
		ret = i2c_imx_hs_wait_for_bus_idle(i2c_imx);
		if (ret)
			return ret;
	}
	hicr = HICR_MSTA | HICR_HIIEN | HICR_MTX;
	if (repeat_start)
		hicr |= HICR_RSTA;

	reg_write(i2c_imx, HISR, HISR_HIAAS | HISR_HIAL | HISR_BTD | HISR_RDC_ZERO | HISR_TDC_ZERO | HISR_RXAK);
	i2c_imx->imr = HIIMR_TDE | HIIMR_HIAAS | HIIMR_HIAL | HIIMR_RXAK;
#ifdef USE_POLL
	reg_write(i2c_imx, HIIMR, 0xffff);
#else
	reg_write(i2c_imx, HIIMR, ~i2c_imx->imr);
#endif
	reg_set_mask(i2c_imx, HICR, hicr);	/*send start */
	return 0;
}

static int i2c_imx_hs_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
			  int num)
{
	int i;
	int ret = -EIO;
	struct imx_i2c_hs_struct *i2c_imx = (struct imx_i2c_hs_struct *) (i2c_get_adapdata(adap));

	if (i2c_imx->low_power) {
		dev_err(&adap->dev, "I2C Device in low power mode\n");
		return -EREMOTEIO;
	}

	if (num < 1) {
		return 0;
	}

	i2c_imx_hs_enable(i2c_imx);
	reset_rx_fifo(i2c_imx, 1);
	reset_tx_fifo(i2c_imx);

	i = 0;
	for (;;) {
		struct i2c_msg *msg = &msgs[i];
		int read;

		/* force write for a 0 length transaction*/
		if (!msg->len) {
			msg->flags &= ~I2C_M_RD;
			msg->buf = 0;
		}
		read = msg->flags & I2C_M_RD;

		i2c_imx->cmd_err = 0;
		init_completion(&i2c_imx->cmd_complete);
		i2c_imx->msg = msg;
		i2c_imx->index = 0;

		/*set address */
		reg_write(i2c_imx, HIMADR, HIMADR_LSB_ADR(msg->addr));

		if (read) {
			ret = i2c_imx_hs_read(i2c_imx, i ? 1 : 0, msg);
		} else {
			ret = i2c_imx_hs_write(i2c_imx, i ? 1 : 0, msg);
		}
		if (!ret) {
#ifdef USE_POLL
			unsigned retry = 10000;
			while (retry--) {
				udelay(10);
				poll(i2c_imx);
				pr_debug("%s: i2c_imx->msg=%p\n", __func__, i2c_imx->msg);
				if (!i2c_imx->msg) {
#ifdef DEBUG
					unsigned hisr;
					unsigned hirfr = reg_read(i2c_imx, HIRFR);

					if (hirfr & HIRFR_RFEN)
						reg_write(i2c_imx, HIRFR, HIRFR_RFEN);
					msleep(2);
					hisr = reg_read(i2c_imx, HISR);
					if (!(hisr & HISR_RDF))
						break;
					dev_err(&i2c_imx->adapter.dev, "read fifo not empty!! hisr=%x\n", hisr);
#else
					break;
#endif
				}
			}
#else
			int r = wait_for_completion_interruptible_timeout(&i2c_imx->cmd_complete,
				i2c_imx->adapter.timeout);
			if (r == 0) {
				int cnt = i2c_imx->msg ? i2c_imx->msg->len : 0;
				dev_err(&i2c_imx->adapter.dev, "controller timed out %d of %d, byte %d of %d\n",
						i, num, i2c_imx->index, cnt);
				ret = -ETIMEDOUT;
			} else {
				if (i2c_imx->msg) {
					/* signal stopped us, wait another jiffy */
					wait_for_completion_timeout(&i2c_imx->cmd_complete,1);
				}
				if (!read && !msg->len) {
					/*
					 *  NAK interrupts can be delayed
					 *  when sending 0 bytes
					 */
					msleep(1);
				}
			}
#endif
		}
		if (i2c_imx->msg) {
			i2c_imx->cmd_err |= CME_INCOMPLETE;
			i2c_imx->msg = NULL;
		}
		if (i2c_imx->cmd_err) {
			ret = (i2c_imx->cmd_err & CME_AL) ? -EREMOTEIO : -EIO;
		}
		if (ret) {
			if (0) printk(KERN_ERR "%s: ret=%i %s HICR=%x HISR=%x HITFR=%x HIRFR=%x cmd_err=%x sent %i of %i\n", __func__, ret, read ? "read" : "write",
					reg_read(i2c_imx, HICR), reg_read(i2c_imx, HISR), reg_read(i2c_imx, HITFR), reg_read(i2c_imx, HIRFR),
					i2c_imx->cmd_err, i2c_imx->index, msg->len);
			msleep(2);
			if (0) printk(KERN_ERR "%s: HISR=%x\n", __func__, reg_read(i2c_imx, HISR));
			if ((ret == -EREMOTEIO) || (ret == -ETIMEDOUT)) {
				/* This hardware is so buggy, let's reset the bus*/
				i2c_imx_hs_stop(i2c_imx);
				mxci2c_bus_recovery(i2c_imx);
			}
		} else {
			if (i2c_imx->index != msg->len) {
				printk(KERN_ERR "%s: %s byte cnt error %i != %i HICR=%x HISR=%x HITFR=%x HIRFR=%x sent %i of %i\n", __func__,
						read ? "read" : "write", i2c_imx->index, msg->len,
						reg_read(i2c_imx, HICR), reg_read(i2c_imx, HISR), reg_read(i2c_imx, HITFR), reg_read(i2c_imx, HIRFR),
						i2c_imx->index, msg->len);
				ret = -EIO;
			} else {
				pr_debug("%s: ret=%i %s HICR=%x HISR=%x HITFR=%x HIRFR=%x sent %i of %i\n", __func__, ret, read ? "read" : "write",
					reg_read(i2c_imx, HICR), reg_read(i2c_imx, HISR), reg_read(i2c_imx, HITFR), reg_read(i2c_imx, HIRFR),
					i2c_imx->index, msg->len);
			}
		}
		udelay(100);	/* 15 is too short */
		if (ret < 0)
			break;
		i++;
		if (i >= num)
			break;
	}
	i2c_imx_hs_stop(i2c_imx);
	msleep(1);
	i2c_imx_hs_disable(i2c_imx);

	if (ret < 0)
		return ret;

	return i;
}

static u32 i2c_imx_hs_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

/*!
 * Stores the pointers for the i2c algorithm functions. The algorithm functions
 * is used by the i2c bus driver to talk to the i2c bus
 */
static struct i2c_algorithm i2c_imx_hs_algorithm = {
	.master_xfer = i2c_imx_hs_xfer,
	.functionality = i2c_imx_hs_func
};

static void isr_read_fifo_data(struct imx_i2c_hs_struct *i2c_imx)
{
	unsigned hirfr;
	unsigned rfc;
	unsigned i = i2c_imx->index;
	unsigned next = 0;
	unsigned len = (i2c_imx->msg) ? i2c_imx->msg->len : i;
	unsigned rem;

	reg_write(i2c_imx, HISR, HISR_BTD);
	hirfr = reg_read(i2c_imx, HIRFR);
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
		unsigned scl = 0;
		int loop = 0;

		rfc -= rem;
		if (next == 1) {
			/*
			 * A weird bug causes arbitration loss if TXAK
			 * is set while SCL is high.
			 * Wait for a high to low transition of SCL before
			 * setting TXAK.
			 */
			for (;;) {
				unsigned tmp = gpiod_get_value(i2c_imx->scl_gpio);

				scl = (scl << 1) | (tmp & 1);
				if ((scl & 3) == 2)
					break;

				if (++loop > 200) {
//					pr_debug("%s: timeout waiting for BTD, hisr=%x\n", __func__, hisr);
					break;
				}
				if ((scl==0) && (rem == 8))
					break;	/* fifo full, ack sent,  no more data coming */
//				udelay(1);
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
			i2c_imx->msg->buf[i++] = reg_read(i2c_imx, HIRDR);
			reg_set_mask(i2c_imx, HICR, HICR_TXAK);
			pr_debug("%s: HICR=%x HISR=%x HIRFR=%x HIRDCR=%x "
				"receiving data[%i] = %02x of %i\n", __func__,
				reg_read(i2c_imx, HICR), reg_read(i2c_imx, HISR),
				hirfr, reg_read(i2c_imx, HIRDCR), i-1,
				i2c_imx->msg->buf[i-1], len);
			rem--;
		}
		while (rem) {
			i2c_imx->msg->buf[i++] = reg_read(i2c_imx, HIRDR);
			pr_debug("%s: HICR=%x HISR=%x HIRFR=%x HIRDCR=%x "
				"receiving data[%i] = %02x of %i\n", __func__,
				reg_read(i2c_imx, HICR), reg_read(i2c_imx, HISR),
				hirfr, reg_read(i2c_imx, HIRDCR), i-1,
				i2c_imx->msg->buf[i-1], len);
			rem--;
		}
		i2c_imx->index = i;
		if (hirfr & HIRFR_RFEN)
			if (next && (next < 8))
				reg_write(i2c_imx, HIRFR, HIRFR_RFEN | HIRFR_RFWM((next-1)));
	}
	if (rfc) {
		/* unexpected data in fifo, hardware bug or message gone */
		while (rfc) {
			unsigned data = reg_read(i2c_imx, HIRDR);
			printk(KERN_ERR "%s: unexpected data %x\n", __func__, data);
			rfc--;
		}
		reg_set_mask(i2c_imx, HICR, HICR_TXAK);
	}
	if (!next) {
		i2c_imx->msg = NULL;
		complete(&i2c_imx->cmd_complete);
	}
}

static void isr_write_fifo_data(struct imx_i2c_hs_struct *i2c_imx)
{
	unsigned rem = 0;
	int i = i2c_imx->index;
	unsigned hitfr = reg_read(i2c_imx, HITFR);
	unsigned tfc = (hitfr >> 8) & 0xf;
	int next = 0;
	struct i2c_msg *msg = i2c_imx->msg;
	pr_debug("%s: HITFR=%x HITDCR=%x\n", __func__, hitfr, reg_read(i2c_imx, HITDCR));
	if (tfc > 8)
		tfc = 8;
	tfc = 8 - tfc;
	if (msg)
		rem = msg->len - i;
	if (rem > tfc) {
		next = rem - tfc;
		rem = tfc;
	}
	if (rem > 0) {
		tfc -= rem;
		while (rem) {
			reg_write(i2c_imx, HITDR, msg->buf[i++]);
			pr_debug("%s:write %02x\n", __func__, msg->buf[i]);
			rem--;
		}
		i2c_imx->index = i;
	}
	if ((!next) && (tfc == 8)) {
		i2c_imx->msg = NULL;
		complete(&i2c_imx->cmd_complete);
		i2c_imx->imr &= ~HIIMR_TDE;
#ifndef USE_POLL
		reg_write(i2c_imx, HIIMR, ~i2c_imx->imr);
#endif
		pr_debug("%s: write complete, hiimr=%x\n", __func__, reg_read(i2c_imx, HIIMR));
	}
}

static int isr_do_work(struct imx_i2c_hs_struct *i2c_imx, unsigned hisr)
{
	unsigned hisr_m = hisr & i2c_imx->imr;

	dev_dbg(&i2c_imx->adapter.dev, "%s: hisr=0x%x 0x%x hiimr=0x%x add %x, %x bytes\n",
			__func__, hisr, hisr_m, reg_read(i2c_imx, HIIMR),
			i2c_imx->msg ? i2c_imx->msg->addr : 0xffff,
			i2c_imx->msg ? i2c_imx->msg->len : 0xffff);
	if (hisr_m & HISR_RDF) {
		isr_read_fifo_data(i2c_imx);
	} else if (hisr_m & HISR_TDE) {
		isr_write_fifo_data(i2c_imx);
	} else if (hisr_m & (HISR_HIAAS | HISR_HIAL | HISR_BTD |
			HISR_RDC_ZERO | HISR_TDC_ZERO | HISR_RXAK)) {
		reg_write(i2c_imx, HISR, hisr_m);
		/*
		 * Clear the status bits we've recognized in the imr,
		 * because they don't seem to be clearing in the status reg
		 */
		i2c_imx->imr &= ~(hisr & (HISR_HIAAS | HISR_HIAL | HISR_RXAK));
		reg_write(i2c_imx, HIIMR, ~i2c_imx->imr);

		if (hisr & (HISR_HIAAS | HISR_HIAL | HISR_RXAK)) {

			if (hisr & HISR_HIAL) {
				i2c_imx->cmd_err |= CME_AL;
			} else if (hisr & HISR_HIAAS) {
				i2c_imx->cmd_err |= CME_AAS;
			} else {
				i2c_imx->cmd_err |= CME_NAK;
				if (0) pr_info("%s:nak of %x\n", __func__, i2c_imx->msg ? i2c_imx->msg->addr : 0xffff);
			}
			if (0) printk(KERN_ERR "%s: HICR=%x HISR=%x HITFR=%x HIRFR=%x HIIMR=%x cmd_err=%x sent %i of %i\n", __func__,
					reg_read(i2c_imx, HICR), hisr, reg_read(i2c_imx, HITFR), reg_read(i2c_imx, HIRFR), reg_read(i2c_imx, HIIMR),
					i2c_imx->cmd_err, i2c_imx->index, (i2c_imx->msg) ? i2c_imx->msg->len : 0);
			if (i2c_imx->msg) {
				i2c_imx->msg = NULL;
				complete(&i2c_imx->cmd_complete);
			}
		}
	} else {
		if (hisr & HISR_RDF) {
			/* Something is wrong if we get here, try to correct */
			unsigned hiimr = reg_read(i2c_imx, HIIMR);
			if (!(hiimr & HIIMR_RDF)) {
				reg_write(i2c_imx, HIIMR, hiimr | HIIMR_RDF | 0xff00);
				pr_debug("%s: disabling RDF interrupt, hiimr=%x\n", __func__, hiimr);
				return -1;
			}
		}
		if (hisr & HISR_TDE) {
			/* Something is wrong if we get here, try to correct */
			unsigned hiimr = reg_read(i2c_imx, HIIMR);
			if (!(hiimr & HIIMR_TDE)) {
				reg_write(i2c_imx, HIIMR, hiimr | HIIMR_TDE | 0xff00);
				pr_debug("%s: disabling TDE interrupt, hiimr=%x\n", __func__, hiimr);
				return -1;
			}
		}
		return 1;
	}
	return 0;
}

#ifdef USE_POLL
void poll(struct imx_i2c_hs_struct *i2c_imx)
{
	unsigned hisr;
#ifdef DEBUG
	msleep(5);
#endif
	hisr = reg_read(i2c_imx, HISR);
#ifdef DEBUG
	msleep(5);
#endif
	isr_do_work(i2c_imx, hisr);
}
#endif

/*
 * Interrupt service routine. This gets called whenever an I2C interrupt
 * occurs.
 */
static irqreturn_t i2c_imx_hs_isr(int this_irq, void *dev_id)
{
	struct imx_i2c_hs_struct *i2c_imx = dev_id;
	int count = 0;
	unsigned hisr;

	for (;;) {
		hisr = reg_read(i2c_imx, HISR);
		if (isr_do_work(i2c_imx, hisr))
			break;
		if (count++ == 100) {
			dev_warn(&i2c_imx->adapter.dev, "Too much work in one IRQ\n");
			break;
		}
	}
	return IRQ_HANDLED;
}

static int i2c_imx_hs_probe(struct platform_device *pdev)
{
	struct imx_i2c_hs_struct *i2c_imx;
	struct resource *res;
	struct imxi2c_platform_data *pdata = dev_get_platdata(&pdev->dev);
	void __iomem *base;
	int irq, ret;
	u32 bitrate;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "can't get irq number\n");
		return irq;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	i2c_imx = devm_kzalloc(&pdev->dev, sizeof(struct imx_i2c_hs_struct),
				GFP_KERNEL);
	if (!i2c_imx) {
		dev_err(&pdev->dev, "can't allocate interface\n");
		return -ENOMEM;
	}
	i2c_imx->scl_gpio = devm_gpiod_get_index(&pdev->dev, "scl", 0, GPIOD_IN);
	if (IS_ERR(i2c_imx->scl_gpio))
		return -ENODEV;

	i2c_imx->sda_gpio = devm_gpiod_get_index(&pdev->dev, "sda", 0, GPIOD_IN);
	if (IS_ERR(i2c_imx->sda_gpio))
		return -ENODEV;

	i2c_imx->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(i2c_imx->pinctrl)) {
		ret = PTR_ERR(i2c_imx->pinctrl);
		return -ENODEV;
	}


	i2c_imx->pins_default = pinctrl_lookup_state(i2c_imx->pinctrl, "default");
	if (IS_ERR(i2c_imx->pins_default)) {
		ret = PTR_ERR(i2c_imx->pins_default);
		return -ENODEV;
	}

	i2c_imx->pins_scl_gpio = pinctrl_lookup_state(i2c_imx->pinctrl, "scl-gpio");
	if (IS_ERR(i2c_imx->pins_scl_gpio)) {
		ret = PTR_ERR(i2c_imx->pins_scl_gpio);
		return -ENODEV;
	}

	i2c_imx->pins_sda_gpio = pinctrl_lookup_state(i2c_imx->pinctrl, "sda-gpio");
	if (IS_ERR(i2c_imx->pins_sda_gpio)) {
		ret = PTR_ERR(i2c_imx->pins_sda_gpio);
		return -ENODEV;
	}

	/* Setup i2c_imx driver structure */
	strlcpy(i2c_imx->adapter.name, pdev->name, sizeof(i2c_imx->adapter.name));
	i2c_imx->adapter.owner		= THIS_MODULE;
	i2c_imx->adapter.algo		= &i2c_imx_hs_algorithm;
	i2c_imx->adapter.dev.parent	= &pdev->dev;
	i2c_imx->adapter.nr 		= pdev->id;
	i2c_imx->adapter.dev.of_node	= pdev->dev.of_node;
	i2c_imx->adapter.timeout	= msecs_to_jiffies(1000);
	i2c_imx->base			= base;

	/* Get I2C ipg clock */
	i2c_imx->ipg_clk = devm_clk_get(&pdev->dev, "ipg");
	if (IS_ERR(i2c_imx->ipg_clk)) {
		dev_err(&pdev->dev, "can't get I2C ipg clock\n");
		return PTR_ERR(i2c_imx->ipg_clk);
	}

	ret = clk_prepare_enable(i2c_imx->ipg_clk);
	if (ret) {
		dev_err(&pdev->dev, "can't enable I2C ipg clock\n");
		return ret;
	}

	/* Get I2C serial clock */
	i2c_imx->serial_clk = devm_clk_get(&pdev->dev, "serial");
	if (IS_ERR(i2c_imx->serial_clk)) {
		dev_err(&pdev->dev, "can't get I2C serial clock\n");
		return PTR_ERR(i2c_imx->serial_clk);
	}

	ret = clk_prepare_enable(i2c_imx->serial_clk);
	if (ret) {
		dev_err(&pdev->dev, "can't enable I2C serial clock\n");
		return ret;
	}
	/* Request IRQ */
	ret = devm_request_irq(&pdev->dev, irq, i2c_imx_hs_isr,
			       IRQF_NO_SUSPEND, pdev->name, i2c_imx);
	if (ret) {
		dev_err(&pdev->dev, "failure requesting irq %d\n", irq);
		return ret;
	}

	init_completion(&i2c_imx->cmd_complete);

	/* Set up adapter data */
	i2c_set_adapdata(&i2c_imx->adapter, i2c_imx);

	/* Set up clock divider */
	bitrate = IMX_I2C_BIT_RATE;
	ret = of_property_read_u32(pdev->dev.of_node,
				   "clock-frequency", &bitrate);
	if (ret < 0 && pdata && pdata->bitrate)
		bitrate = pdata->bitrate;
	i2c_imx->speed = bitrate;
	i2c_imx_hs_set_div(i2c_imx);

	i2c_imx->low_power = false;

	/* Add I2C adapter */
	ret = i2c_add_numbered_adapter(&i2c_imx->adapter);
	if (ret < 0) {
		dev_err(&pdev->dev, "registration failed\n");
		return ret;
	}

	/* Set up platform driver data */
	platform_set_drvdata(pdev, i2c_imx);
#ifndef CLKS_ALWAYS_ON
	clk_disable_unprepare(i2c_imx->serial_clk);
	clk_disable_unprepare(i2c_imx->ipg_clk);
#endif
	dev_dbg(&i2c_imx->adapter.dev, "claimed irq %d\n", irq);
	dev_dbg(&i2c_imx->adapter.dev, "device resources from 0x%x to 0x%x\n",
		res->start, res->end);
	dev_dbg(&i2c_imx->adapter.dev, "allocated %d bytes at 0x%x\n",
		resource_size(res), res->start);
	dev_dbg(&i2c_imx->adapter.dev, "adapter name: \"%s\"\n",
		i2c_imx->adapter.name);
	dev_info(&i2c_imx->adapter.dev, "IMX I2C adapter registered, %d hz\n", i2c_imx->speed);

	return 0;   /* Return OK */
}

static int i2c_imx_hs_remove(struct platform_device *pdev)
{
	struct imx_i2c_hs_struct *i2c_imx = platform_get_drvdata(pdev);

#ifdef CLKS_ALWAYS_ON
	clk_disable_unprepare(i2c_imx->serial_clk);
	clk_disable_unprepare(i2c_imx->ipg_clk);
#endif
	i2c_del_adapter(&i2c_imx->adapter);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int i2c_imx_hs_suspend(struct device *dev)
{
	pinctrl_pm_select_sleep_state(dev);
	return 0;
}

static int i2c_imx_hs_resume(struct device *dev)
{
	pinctrl_pm_select_default_state(dev);
	return 0;
}

static SIMPLE_DEV_PM_OPS(imx_i2c_hs_pm, i2c_imx_hs_suspend, i2c_imx_hs_resume);
#define IMX_I2C_HS_PM	(&imx_i2c_hs_pm)
#else
#define IMX_I2C_HS_PM	NULL
#endif

static struct platform_device_id i2c_imx_hs_devtype[] = {
	{
		.name = "imx51-hsi2c",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(platform, i2c_imx_hs_devtype);

static const struct of_device_id i2c_imx_hs_dt_ids[] = {
	{ .compatible = "fsl,imx51-hsi2c", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, i2c_imx_hs_dt_ids);

static struct platform_driver i2c_imx_hs_driver = {
	.probe = i2c_imx_hs_probe,
	.remove = i2c_imx_hs_remove,
	.driver = {
		.name = "i2c_imx_hs",
		.owner = THIS_MODULE,
		.of_match_table = i2c_imx_hs_dt_ids,
		.pm = IMX_I2C_HS_PM,
	},
	.id_table	= i2c_imx_hs_devtype,
};

static int __init i2c_imx_hs_init(void)
{
	return platform_driver_register(&i2c_imx_hs_driver);
}
subsys_initcall(i2c_imx_hs_init);

static void __exit i2c_imx_hs_exit(void)
{
	platform_driver_unregister(&i2c_imx_hs_driver);
}
module_exit(i2c_imx_hs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("I2C adapter driver for IMX51 highspeed I2C bus");
MODULE_ALIAS("platform:imx-i2c-hs");
