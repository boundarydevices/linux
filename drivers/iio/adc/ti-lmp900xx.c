/*
 * Texas Instruments LMP900xx SPI ADC driver
 *
 * Copyright 2017 Troy Kisky <troy.kisky@boundarydevices.com>
 *
 * Based on ti-ads7950.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#define LMP_RESETCN		0	/* Reset Control */
#define LMP_SPI_HANDSHAKECN	1	/* SPI Handshake Control */
#define LMP_SPI_RESET		2	/* SPI Reset Control */
#define LMP_SPI_STREAMCN	3	/* SPI Stream Control */
#define LMP_PWRCN		8	/* Power Mode Control and Status */
#define LMP_DATA_ONLY_1		9	/* Data Only Read Control 1 */
#define LMP_DATA_ONLY_2		0x0a	/* Data Only Read Control 2 */
#define LMP_ADC_RESTART		0x0b	/* ADC Restart Conversion */
#define LMP_GPIO_DIRCN		0x0e	/* GPIO Direction Control */
#define LMP_GPIO_DAT		0x0f	/* GPIO Data */
#define LMP_BGCALCN		0x10	/* Background Calibration Control */
#define LMP_SPI_DRDYBCN		0x11	/* SPI Data Ready Bar Control */
#define LMP_ADC_AUXCN		0x12	/* ADC Auxiliary Control */
#define LMP_SPI_CRC_CN		0x13	/* CRC Control */
#define LMP_SENDIAG_THLD	0x14	/* Sensor Diagnostic Threshold */
#define LMP_SCALCN		0x17	/* System Calibration Control */
#define LMP_ADC_DONE		0x18	/* ADC Data Available */
#define LMP_SENDIAG_FLAGS	0x19	/* Sensor Diagnostic Flags*/
#define LMP_ADC_DOUT		0x1a	/* Conversion Data 1 and 0. 2 bytes */
#define LMP_SPI_CRC_DAT		0x1d	/* CRC Data */
#define LMP_CH_STS		0x1e	/* Channel Status */
#define LMP_CH_SCAN		0x1f	/* Channel Scan Mode */
#define LMP_CHx_INPUTCN(chan)	(0x20 + (chan << 1))	/* CH0-6 Input Control */
#define LMP_CHx_CONFIG(chan)	(0x21 + (chan << 1))	/* CH0-6 Configuration */
#define LMP_CHx_SCAL_OFFSET(chan)		(0x30 + (chan << 3))	/* CH0-3 System Calibration Offset Coefficients, 2 bytes */
#define LMP_CHx_SCAL_GAIN(chan)			(0x33 + (chan << 3))	/* CH0-3 System Calibration Gain Coefficients, 2 bytes */
#define LMP_CHx_SCAL_SCALING(chan)		(0x36 + (chan << 3))	/* CH0-3 System Calibration Scaling Coefficients*/
#define LMP_CHx_SCAL_BITS_SELECTOR(chan)	(0x37 + (chan << 3))	/* CH0-3 System Calibration Bit Selector */

#define PWRCN_ACTIVE		0
#define PWRCN_POWER_DOWN	1
#define PWRCN_STANDBY		3

#define SCANMODE_SINGLE_CH_CONTINUOUS		0
#define SCANMODE_MULTI_CH_ONCE			1
#define SCANMODE_MULTI_CH_CONTINUOUS		2
#define SCANMODE_MULTI_CH_CONTINUOUS_BURN	3

struct lmp900xx_gpio {
	struct gpio_chip	chip;
	struct mutex		lock;
	unsigned char		out_state;
	unsigned char		unknown_state;
	unsigned char		dir;
};

struct lmp900xx_state {
	struct spi_device	*spi;
	struct completion	completion;
	struct spi_transfer	scan_xfer;
	struct spi_message	scan_msg;

	struct spi_transfer	reg_read_xfer;
	struct spi_message	reg_read_msg;

	struct spi_transfer	reg_write_xfer;
	struct spi_message	reg_write_msg;

	struct regulator	*reg;
	struct lmp900xx_gpio	gpio;
	struct iio_trigger	*trig;
	s64			timestamp;

	struct mutex		uar_lock;
	unsigned char		active_scan_mask;
	unsigned char		bytes_per_cycle;
	unsigned char		current_scan_mask;
	unsigned char		pending_chan;
	unsigned char		scanmode;
	unsigned char		uar;
	unsigned char		last_reg;
	unsigned char		result_index;
	unsigned char		get_index;
	unsigned char		trigger_on;
	unsigned char		adc_pending;
	/*
	 * DMA (thus cache coherency maintenance) requires the
	 * transfer buffers to live in their own cache lines.
	 */
	unsigned char adc_buf[32] ____cacheline_aligned;
	unsigned char result_buf[64];
	unsigned char start_scan[32];
	unsigned char continue_scan[32];
	unsigned char reg_read_tx[32];
	unsigned char reg_read_rx[32];
	unsigned char reg_write_tx[32];
	unsigned char push_buf[32];
};

struct lmp900xx_chip_info {
	const struct iio_chan_spec *channels;
	unsigned int num_channels;
};

enum lmp900xx_id {
	LMP90079,
};

#define LMP900XX_V_CHAN(index)					\
{								\
	.type = IIO_VOLTAGE,					\
	.indexed = 1,						\
	.channel = index,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),	\
	.address = index,					\
	.datasheet_name = "CH##index",				\
	.scan_index = index,					\
	.scan_type = {						\
		.sign = 's',					\
		.realbits = 16,					\
		.storagebits = 16,				\
		.shift = 0,					\
		.endianness = IIO_BE,				\
	},							\
}


#define DECLARE_LMP900XX_7_CHANNELS(name) \
const struct iio_chan_spec name ## _channels[] = { \
	LMP900XX_V_CHAN(0), \
	LMP900XX_V_CHAN(1), \
	LMP900XX_V_CHAN(2), \
	LMP900XX_V_CHAN(3), \
	LMP900XX_V_CHAN(4), \
	LMP900XX_V_CHAN(5), \
	LMP900XX_V_CHAN(6), \
	IIO_CHAN_SOFT_TIMESTAMP(16), \
}

static DECLARE_LMP900XX_7_CHANNELS(lmp900xx);

static const struct lmp900xx_chip_info lmp900xx_chip_info[] = {
	[LMP90079] = {
		.channels	= lmp900xx_channels,
		.num_channels	= ARRAY_SIZE(lmp900xx_channels),
	},
};

unsigned char reg_bytes[] = {
[0] = 1,
[1] = 1,
[2] = 1,
[3] = 1,
[8] = 1,
[9] = 1,
[0x0a] = 1,
[0x0b] = 1,
[0x0e] = 1,
[0x0f] = 1,
[0x10] = 1,
[0x11] = 1,
[0x12] = 1,
[0x13] = 1,
[0x14] = 1,
[0x17] = 1,
[0x18] = 1,
[0x19] = 1,
[0x1a] = 2,
[0x1d] = 1,
[0x1e] = 1,
[0x1f] = 1,
[0x20] = 1,
[0x21] = 1,
[0x22] = 1,
[0x23] = 1,
[0x24] = 1,
[0x25] = 1,
[0x26] = 1,
[0x27] = 1,
[0x28] = 1,
[0x29] = 1,
[0x2a] = 1,
[0x2b] = 1,
[0x2c] = 1,
[0x2d] = 1,
[0x30] = 2,
[0x33] = 2,
[0x36] = 1,
[0x37] = 1,
[0x38] = 2,
[0x3b] = 2,
[0x3e] = 1,
[0x3f] = 1,
[0x40] = 2,
[0x43] = 2,
[0x46] = 1,
[0x47] = 1,
[0x48] = 2,
[0x4b] = 2,
[0x4e] = 1,
[0x4f] = 1,
};


static int lmp900xx_spi_read_reg(struct lmp900xx_state *st, unsigned reg)
{
	unsigned char *p = &st->reg_read_tx[0];
	int len;
	unsigned bytes;
	int ret;
	int rx_index;

	if (reg >= ARRAY_SIZE(reg_bytes))
		return -EINVAL;
	bytes = reg_bytes[reg];
	if (!bytes || (bytes > 2))
		return -EINVAL;

	if (bytes == 1) {
		len = 4;
		p[2] = 0x80 | (0 << 5) | (reg & 0xf);
	} else {
		p[2] = 0x80 | (1 << 5) | (reg & 0xf);
		len = 5;
	}

	mutex_lock(&st->uar_lock);
	if (st->uar == (reg >> 4)) {
		p += 2;
		len -= 2;
		rx_index = 1;
	} else {
		st->uar = p[1] = (reg >> 4);
		rx_index = 3;
	}
	st->reg_read_xfer.tx_buf = p;
	st->reg_read_xfer.len = len;
	ret = spi_sync(st->spi, &st->reg_read_msg);
	if (ret >= 0) {
		if (bytes == 1)
			ret = st->reg_read_rx[rx_index];
		else
			ret = (st->reg_read_rx[rx_index] << 8) | st->reg_read_rx[rx_index + 1];
	}
	mutex_unlock(&st->uar_lock);
	pr_debug("%s:reg(%x)=%x\n", __func__, reg, ret);
	return ret;
}

static int lmp900xx_spi_write_reg(struct lmp900xx_state *st, unsigned reg, unsigned value)
{
	unsigned char *p = &st->reg_write_tx[0];
	int len;
	unsigned bytes;
	int ret;

	pr_debug("%s:reg(%x)=%x\n", __func__, reg, value);
	if (reg >= ARRAY_SIZE(reg_bytes))
		return -EINVAL;
	bytes = reg_bytes[reg];
	if (!bytes || (bytes > 2))
		return -EINVAL;

	if (bytes == 1) {
		len = 4;
		p[2] = (0 << 5) | (reg & 0xf);
		p[3] = value;
	} else {
		p[2] = (1 << 5) | (reg & 0xf);
		p[3] = value >> 8;
		p[4] = value;
		len = 5;
	}

	mutex_lock(&st->uar_lock);
	if (st->uar == (reg >> 4)) {
		p += 2;
		len -= 2;
	} else {
		st->uar = p[1] = (reg >> 4);
	}
	st->reg_write_xfer.tx_buf = p;
	st->reg_write_xfer.len = len;
	ret = spi_sync(st->spi, &st->reg_write_msg);
	mutex_unlock(&st->uar_lock);
	return ret;
}

static void wait_adc_idle(struct lmp900xx_state *st)
{
	unsigned long timeout;

	if (!st->adc_pending)
		return;

	timeout = jiffies + msecs_to_jiffies(500);

	do {
		msleep(1);
		if (!st->adc_pending)
			return;
		if (time_after(jiffies, timeout)) {
			pr_err("%s: timed out\n",  __func__);
			break;
		}
	} while (1);
}

void setup_continue_scan(unsigned char *p)
{
	p[0] = 0x10;
	p[1] = LMP_SENDIAG_FLAGS >> 4;
	p[2] = 0x80 | (2 << 5) | (LMP_SENDIAG_FLAGS & 0xf);
#define CS_SENDIAG_FLAGS	3
	p[3] = 0xff;
#define CS_ADC_OUT		4
	p[4] = 0xff;
	p[5] = 0xff;
#define CS_LEN			6
}

void setup_start_scan(unsigned char *p)
{
	p[0] = 0x10;
	p[1] = LMP_CH_SCAN >> 4;
	p[2] = 0x00 | (0 << 5) | (LMP_CH_SCAN & 0xf);
#define SS_CHAN		3
	p[3] = 0;
	p[4] = 0x10;
	p[5] = LMP_PWRCN >> 4;
	p[6] = 0x00 | (0 << 5) | (LMP_PWRCN & 0xf);
	p[7] = PWRCN_ACTIVE;
#define SS_LEN		8
}

/*  **********************  trigger functions ********************** */
static int start_scan(struct lmp900xx_state *st, unsigned mask, unsigned scanmode)
{
	unsigned char *p = &st->start_scan[0];
	int len = SS_LEN;
	int ret;
	int first_chan, last_chan;

	if (!mask || (mask & 0xffffff80)) {
		pr_err("%s: mask=%x\n", __func__, mask);
		return -EINVAL;
	}
	if (st->current_scan_mask)
		return -EBUSY;

	first_chan = __ffs(mask);
	last_chan = __fls(mask);


	wait_adc_idle(st);
	mutex_lock(&st->uar_lock);
	st->scanmode = scanmode;
	p[SS_CHAN] = first_chan | (last_chan << 3) | (scanmode << 6);
	st->pending_chan = first_chan;
	if (st->uar == (LMP_CH_SCAN >> 4)) {
		p += 2;
		len -= 2;
	}
	st->uar = (LMP_PWRCN >> 4);
	st->scan_xfer.tx_buf = p;
	st->scan_xfer.len = len;
	st->result_index = 0;
	st->get_index = 0;
	st->current_scan_mask = mask;
	st->bytes_per_cycle = hweight8(mask) << 1;
	memset(st->push_buf, 0, sizeof(st->push_buf));
	st->adc_pending = 1;
	ret = spi_sync(st->spi, &st->scan_msg);
	mutex_unlock(&st->uar_lock);
	if (ret < 0) {
		pr_err("%s: ret=%d\n", __func__, ret);
		st->current_scan_mask = 0;
		return ret;
	}
	return 0;
}

static int lmp900xx_set_trigger_state(struct iio_trigger *trig, bool state)
{
	struct iio_dev *iio = iio_trigger_get_drvdata(trig);
	struct lmp900xx_state *st = iio_priv(iio);
	int ret;

	st->trigger_on = state;
	if (state) {
		ret = start_scan(st, st->active_scan_mask, SCANMODE_MULTI_CH_CONTINUOUS);
		if (ret < 0)
			st->trigger_on = 0;
	} else {
		st->current_scan_mask = 0;
		/*
		 * Wait for last adc to finish so that interrupts don't
		 * get stuck low
		 */
		wait_adc_idle(st);
		ret = lmp900xx_spi_write_reg(st, LMP_PWRCN, PWRCN_POWER_DOWN);
	}
	return ret;
}

static int lmp900xx_validate_device(struct iio_trigger *trig,
				   struct iio_dev *iio)
{
	struct iio_dev *indio = iio_trigger_get_drvdata(trig);

	if (indio != iio)
		return -EINVAL;

	return 0;
}

static const struct iio_trigger_ops lmp900xx_trigger_ops = {
	.owner = THIS_MODULE,
	.validate_device = &lmp900xx_validate_device,
	.set_trigger_state = &lmp900xx_set_trigger_state,
};

/*  **********************  GPIO functions ********************** */
static int lmp900xx_gpio_get_direction(struct gpio_chip *chip,
	unsigned int offset)
{
	struct lmp900xx_gpio *const d = container_of(chip, struct lmp900xx_gpio, chip);

	if (offset > 5)
		return -EINVAL;

	return ((d->dir >> offset) & 1) ^ 1;
}

static int lmp900xx_gpio_direction_input(struct gpio_chip *chip,
	unsigned int offset)
{
	struct lmp900xx_gpio *const d = container_of(chip, struct lmp900xx_gpio, chip);
	struct lmp900xx_state *st;
	unsigned char new_val;

	if (offset > 5)
		return -EINVAL;

	st = container_of(d, struct lmp900xx_state, gpio);

	mutex_lock(&d->lock);
	new_val = d->dir & ~(1 << offset);
	if (d->dir != new_val) {
		d->dir = new_val;
		lmp900xx_spi_write_reg(st, LMP_GPIO_DIRCN, new_val);
	}
	mutex_unlock(&d->lock);
	return 0;
}

static int lmp900xx_gpio_direction_output(struct gpio_chip *chip,
	unsigned int offset, int value)
{
	struct lmp900xx_gpio *const d = container_of(chip, struct lmp900xx_gpio, chip);
	struct lmp900xx_state *st;
	unsigned char new_val;

	if (offset > 5)
		return -EINVAL;

	st = container_of(d, struct lmp900xx_state, gpio);
	chip->set(chip, offset, value);

	mutex_lock(&d->lock);
	new_val = d->dir | (1 << offset);
	if (d->dir != new_val) {
		d->dir = new_val;
		lmp900xx_spi_write_reg(st, LMP_GPIO_DIRCN, new_val);
		d->unknown_state |= (1 << offset);
		mutex_unlock(&d->lock);
		chip->set(chip, offset, value);
	} else {
		mutex_unlock(&d->lock);
	}
	return 0;
}

static int lmp900xx_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct lmp900xx_gpio *const d = container_of(chip, struct lmp900xx_gpio, chip);
	struct lmp900xx_state *st;
	int ret;

	if (offset > 5)
		return -EINVAL;

	st = container_of(d, struct lmp900xx_state, gpio);
	ret = lmp900xx_spi_read_reg(st, LMP_GPIO_DAT);
	if (ret < 0)
		return ret;
	return (ret >> offset) & 1;
}

static void lmp900xx_gpio_set(struct gpio_chip *chip, unsigned int offset,
	int value)
{
	struct lmp900xx_gpio *const d = container_of(chip, struct lmp900xx_gpio, chip);
	struct lmp900xx_state *st;
	unsigned new_val;

	if (offset > 5)
		return;

	st = container_of(d, struct lmp900xx_state, gpio);
	mutex_lock(&d->lock);
	new_val = d->out_state;
	if (value)
		new_val |= (1 << offset);
	else
		new_val &= ~(1 << offset);

	if ((d->out_state != new_val) || (d->unknown_state & (1 << offset))) {
		d->unknown_state &= ~(1 << offset);
		d->out_state = new_val;
		lmp900xx_spi_write_reg(st, LMP_GPIO_DAT, new_val);
	}
	mutex_unlock(&d->lock);
}

/*  **********************  DRDY irq function ********************** */
static irqreturn_t lmp900xx_irq_handler(int irq, void *data)
{
	struct iio_dev *iio = data;
	struct lmp900xx_state *st = iio_priv(iio);
	unsigned char *p = &st->continue_scan[0];
	int ret;
	int i;
	int len = CS_LEN;
	int pending_chan = st->pending_chan;
	int next_pending_chan = pending_chan;
	int current_scan_mask = st->current_scan_mask;
	int rx_index;
	int cycle_complete = 0;
	int repeat = 0;
	unsigned char *pdiag;
	int diag;

	int next = st->result_index + 2;

	if (next >= ARRAY_SIZE(st->result_buf))
		next = 0;
	if (st->get_index == next) {
		/* no space to store current result, repeat read */
		repeat = 1;
	}

	if (!repeat) {
		current_scan_mask >>= (pending_chan + 1);
		if (!current_scan_mask) {
			current_scan_mask = st->current_scan_mask;
			next_pending_chan = -1;
			if (current_scan_mask) {
				st->timestamp = iio_get_time_ns(iio);
				cycle_complete = 1;
			}
		}
		if (current_scan_mask) {
			if ((next_pending_chan >= 0) || st->trigger_on)
				next_pending_chan += ffs(current_scan_mask);
		}
	}
	mutex_lock(&st->uar_lock);
	if (st->uar == (LMP_SENDIAG_FLAGS >> 4)) {
		p += 2;
		len -= 2;
		rx_index = CS_SENDIAG_FLAGS - 2;
	} else {
		st->uar = (LMP_SENDIAG_FLAGS >> 4);
		rx_index = CS_SENDIAG_FLAGS;
	}

	st->scan_xfer.tx_buf = p;
	st->scan_xfer.len = len;
	ret = spi_sync(st->spi, &st->scan_msg);
	i = st->result_index;

	if (ret < 0 || repeat)
		goto exit1;

	if (!current_scan_mask) {
		mutex_unlock(&st->uar_lock);
		if (st->scanmode != SCANMODE_MULTI_CH_ONCE) {
			st->scanmode = SCANMODE_MULTI_CH_ONCE;
			lmp900xx_spi_write_reg(st, LMP_CH_SCAN, pending_chan |
					(pending_chan << 3) | (SCANMODE_MULTI_CH_ONCE << 6));
		}
		st->adc_pending = 0;
		/*
		 * Unexpected interrupt, let us hope reading adc
		 * clears it, but sleep to slow them down in case
		 * it doesn't
		 */
		msleep(10);
		return IRQ_HANDLED;
	}

	pdiag = &st->adc_buf[rx_index];
	diag = pdiag[0];
	pr_debug("%s: i=%x %x diag=%x %x\n", __func__,
			i, st->bytes_per_cycle,
			diag, st->current_scan_mask);
	if ((diag & 7) != pending_chan) {
		/*
		 * either a conversion was missed,
		 * or we don't care about this channel
		 */
		goto exit1;
	}
	st->result_buf[i++] = pdiag[LMP_ADC_DOUT - LMP_SENDIAG_FLAGS];
	st->result_buf[i++] = pdiag[LMP_ADC_DOUT + 1 - LMP_SENDIAG_FLAGS];
	if (i >= ARRAY_SIZE(st->result_buf))
		i = 0;
	st->result_index = i;
	mutex_unlock(&st->uar_lock);

	if (next_pending_chan >= 0) {
		st->pending_chan = next_pending_chan;
	} else {
		/* We can stop the conversions */
		if (st->scanmode != SCANMODE_MULTI_CH_ONCE) {
			st->scanmode = SCANMODE_MULTI_CH_ONCE;
			lmp900xx_spi_write_reg(st, LMP_CH_SCAN, pending_chan |
				(pending_chan << 3) | (SCANMODE_MULTI_CH_ONCE << 6));
			msleep(2);
			lmp900xx_spi_read_reg(st, LMP_ADC_DOUT);
			lmp900xx_spi_read_reg(st, LMP_ADC_DOUT);
		}
		st->adc_pending = 0;
	}

	if (cycle_complete) {
		pr_debug("%s: result_index=%x %x, %d %x\n", __func__,
				st->result_index, st->bytes_per_cycle,
				pending_chan, st->current_scan_mask);
		if (iio_buffer_enabled(iio)) {
			if (st->trigger_on)
				iio_trigger_poll_chained(iio->trig);
			else
				pr_err("%s:trigger off, %x\n", __func__,
						st->result_index);
		} else {
			complete(&st->completion);
		}
	}
	return IRQ_HANDLED;
exit1:
	mutex_unlock(&st->uar_lock);
	if (st->scanmode == SCANMODE_MULTI_CH_ONCE) {
		if (st->adc_pending) {
			lmp900xx_spi_write_reg(st, LMP_CH_SCAN, pending_chan |
				(pending_chan << 3) | (SCANMODE_MULTI_CH_ONCE << 6));
			lmp900xx_spi_write_reg(st, LMP_PWRCN, PWRCN_ACTIVE);
		}
	}
	return IRQ_HANDLED;
}

/*
 * lmp900xx_update_scan_mode() setup the spi transfer buffer for the new
 * scan mask
 */
static int lmp900xx_update_scan_mode(struct iio_dev *iio,
				     const unsigned long *active_scan_mask)
{
	struct lmp900xx_state *st = iio_priv(iio);
	unsigned long mask = *active_scan_mask;
	unsigned char next = mask & 0x7f;

	if (next != mask)
		return -EINVAL;

	st->active_scan_mask = next;
	return 0;
}

static irqreturn_t lmp900xx_trigger_handler(int irq, void *dat)
{
	struct iio_poll_func *pf = dat;
	struct iio_dev *iio = pf->indio_dev;
	struct lmp900xx_state *st = iio_priv(iio);
	unsigned get_index = st->get_index;
	unsigned char *p = &st->result_buf[get_index];
	int cnt;
	int bytes_per_cycle = st->bytes_per_cycle;
	int r = st->result_index;

	if (r < get_index)
		r += ARRAY_SIZE(st->result_buf);
	get_index += bytes_per_cycle;
	if (get_index > r) {
		pr_err("%s: sample not ready, %x %x %x\n", __func__,
			get_index, r, bytes_per_cycle);
		goto out;
	}
	if (get_index >= ARRAY_SIZE(st->result_buf)) {
		get_index -= ARRAY_SIZE(st->result_buf);
		cnt = bytes_per_cycle - get_index;
		memcpy(st->push_buf, p, cnt);
		if (get_index) {
			memcpy(&st->push_buf[cnt], st->result_buf, get_index);
		}
	} else {
		memcpy(st->push_buf, p, bytes_per_cycle);
	}
	p = st->push_buf;
	st->get_index = get_index;
	iio_push_to_buffers_with_timestamp(iio, p, st->timestamp);
out:
	iio_trigger_notify_done(iio->trig);
	return IRQ_HANDLED;
}

static int lmp900xx_scan_direct(struct lmp900xx_state *st, unsigned int chan)
{
	int ret = start_scan(st, 1 << chan, SCANMODE_MULTI_CH_ONCE);

	if (ret < 0)
		return ret;

	ret = wait_for_completion_interruptible_timeout(
		&st->completion, HZ);
	if (ret == 0)
		ret = -ETIMEDOUT;
	if (ret < 0) {
		pr_err("%s:ret=%d\n", __func__, ret);
	} else {
		ret = (st->result_buf[0] << 8) | st->result_buf[1];
	}
	st->current_scan_mask = 0;
	return ret;
}

static int lmp900xx_get_vref(struct lmp900xx_state *st)
{
	int vref;

	vref = regulator_get_voltage(st->reg);
	if (vref < 0)
		return vref;

	vref /= 1000;
	return vref;
}

static int lmp900xx_read_raw(struct iio_dev *iio,
			       struct iio_chan_spec const *chan,
			       int *val, int *val2, long m)
{
	struct lmp900xx_state *st = iio_priv(iio);
	int ret;

	switch (m) {
	case IIO_CHAN_INFO_RAW:
#if 0
		ret = iio_device_claim_direct_mode(iio);
		if (ret < 0)
			return ret;
#endif
		ret = lmp900xx_scan_direct(st, chan->address);
#if 0
		iio_device_release_direct_mode(iio);
#endif
		if (ret < 0)
			return ret;
		*val = ret;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		ret = lmp900xx_get_vref(st);
		if (ret < 0)
			return ret;

		*val = ret;
		*val2 = chan->scan_type.realbits - 1;
		return IIO_VAL_FRACTIONAL_LOG2;
	}
	return -EINVAL;
}

static const struct iio_info lmp900xx_info = {
	.read_raw		= &lmp900xx_read_raw,
	.update_scan_mode	= lmp900xx_update_scan_mode,
	.driver_module		= THIS_MODULE,
};

static ssize_t show_reg(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct iio_dev *iio = container_of(dev, struct iio_dev, dev);
	struct lmp900xx_state *st = iio_priv(iio);
	s32 rval;

	pr_debug("%s:last_reg=%x\n", __func__, st->last_reg);
	if (!st->last_reg) {
		int reg;
		int cnt = 0;
		int n;

		for (reg = 1; reg < ARRAY_SIZE(reg_bytes); reg++) {
			if (!reg_bytes[reg])
				continue;
			rval = lmp900xx_spi_read_reg(st, reg);
			if (rval >= 0) {
				n = sprintf(buf, "0x%02x:0x%x\n", reg, rval);
				if (n > 0) {
					cnt += n;
					buf += n;
				}
			}
		}
		return cnt;
	}
	rval = lmp900xx_spi_read_reg(st, st->last_reg);
	if (rval < 0)
		return rval;
	return sprintf(buf, "0x%x", rval);
}

static ssize_t store_reg(struct device *dev, struct device_attribute *attr,
	 const char *buf, size_t count)
{
	struct iio_dev *iio = container_of(dev, struct iio_dev, dev);
	struct lmp900xx_state *st = iio_priv(iio);
	unsigned reg, value, bytes;
	int numscanned = sscanf(buf,"%x %x\n", &reg, &value);

	if (reg >= ARRAY_SIZE(reg_bytes)) {
		dev_err(dev,"%s: invalid register: use form 0xREG [0xVAL]\n",
			__func__);
		return -EINVAL;
	}
	bytes = reg_bytes[reg];
	if (!bytes || (bytes > 2)) {
		dev_err(dev,"%s: invalid register: use form 0xREG [0xVAL]\n",
			__func__);
		return -EINVAL;
	}

	if (numscanned == 2) {
		int rval = lmp900xx_spi_write_reg(st, reg, value);
		if (rval)
			dev_err(dev, "%s: error %d setting reg 0x%04x to 0x%02x\n",
				__func__, rval, reg, value);
	} else if (numscanned == 1) {
		st->last_reg = reg;
	} else {
		dev_err(dev,"%s: invalid register: use form 0xREG [0xVAL]\n",
			__func__);
	}
	return count;
}
static DEVICE_ATTR(lmp900xx_reg, S_IRUGO|S_IWUSR|S_IWGRP, show_reg, store_reg);

static int lmp900xx_probe(struct spi_device *spi)
{
	struct lmp900xx_state *st;
	struct lmp900xx_gpio * d;
	struct iio_dev *iio;
	const struct lmp900xx_chip_info *info;
	int ret;
	int chan;
	int loop = 0;

	iio = devm_iio_device_alloc(&spi->dev, sizeof(*st));
	if (!iio)
		return -ENOMEM;

	st = iio_priv(iio);
	init_completion(&st->completion);

	spi_set_drvdata(spi, iio);

	st->spi = spi;
	setup_continue_scan(st->continue_scan);
	setup_start_scan(st->start_scan);
	st->scan_xfer.rx_buf = &st->adc_buf[0];
	spi_message_init_with_transfers(&st->scan_msg,
					&st->scan_xfer, 1);

	st->reg_write_tx[0] = 0x10;
	st->reg_write_tx[1] = 0;
	st->reg_write_tx[2] = 0;
	st->reg_write_tx[3] = 0xff;
	st->reg_write_tx[4] = 0xff;
	spi_message_init_with_transfers(&st->reg_write_msg,
					&st->reg_write_xfer, 1);

	st->reg_read_tx[0] = 0x10;
	st->reg_read_tx[1] = 0;
	st->reg_read_tx[2] = 0;
	st->reg_read_tx[3] = 0xff;
	st->reg_read_tx[4] = 0xff;
	st->reg_read_xfer.rx_buf = &st->reg_read_rx[0];
	spi_message_init_with_transfers(&st->reg_read_msg,
					&st->reg_read_xfer, 1);

	info = &lmp900xx_chip_info[spi_get_device_id(spi)->driver_data];

	iio->name = spi_get_device_id(spi)->name;
	iio->dev.parent = &spi->dev;
	iio->modes = INDIO_DIRECT_MODE;
	iio->channels = info->channels;
	iio->num_channels = info->num_channels;
	iio->info = &lmp900xx_info;

	st->reg = devm_regulator_get(&spi->dev, "vref");
	if (IS_ERR(st->reg)) {
		dev_err(&spi->dev, "Failed get get regulator \"vref\"\n");
		return PTR_ERR(st->reg);
	}

	ret = regulator_enable(st->reg);
	if (ret) {
		dev_err(&spi->dev, "Failed to enable regulator \"vref\"\n");
		return ret;
	}

	d = &st->gpio;
	d->chip.label = "lmp900xx";
	d->chip.owner = THIS_MODULE;
	d->chip.base = -1;
	d->chip.ngpio = 5;
	d->chip.get_direction = lmp900xx_gpio_get_direction;
	d->chip.direction_input = lmp900xx_gpio_direction_input;
	d->chip.direction_output = lmp900xx_gpio_direction_output;
	d->chip.get = lmp900xx_gpio_get;
	d->chip.set = lmp900xx_gpio_set;
	d->out_state = 0x0;
	mutex_init(&d->lock);
	mutex_init(&st->uar_lock);

	lmp900xx_spi_write_reg(st, LMP_RESETCN, 0xc3);
	msleep(2);
	/* Take out of continuous scan mode */
	lmp900xx_spi_read_reg(st, LMP_ADC_DOUT);
	lmp900xx_spi_write_reg(st, LMP_CH_SCAN, 0 | (0 << 3) |
			(SCANMODE_MULTI_CH_ONCE << 6));
	st->scanmode = SCANMODE_MULTI_CH_ONCE;

	lmp900xx_spi_write_reg(st, LMP_SPI_CRC_CN, 0);
	lmp900xx_spi_write_reg(st, LMP_GPIO_DIRCN, 0);
	for (chan = 0; chan <= 6; chan++) {
		lmp900xx_spi_write_reg(st, LMP_CHx_INPUTCN(chan), (chan << 3) | 7);
		lmp900xx_spi_write_reg(st, LMP_CHx_CONFIG(chan), 0x70);
	}
	lmp900xx_spi_write_reg(st, LMP_SPI_HANDSHAKECN, 4);
	lmp900xx_spi_write_reg(st, LMP_SPI_DRDYBCN, 0x80);

	/* flush adc data caused by continuous mode */
	lmp900xx_spi_read_reg(st, LMP_ADC_DOUT);
	lmp900xx_spi_write_reg(st, LMP_PWRCN, PWRCN_ACTIVE);
	while (1) {
		msleep(10);
		lmp900xx_spi_read_reg(st, LMP_ADC_DOUT);
		/* read just finished conversion */
		ret = lmp900xx_spi_read_reg(st, LMP_CH_STS);
		if (!(ret & 2))
			if (ret & 1)
				break;
		if (loop++ > 10)
			break;
	}
	lmp900xx_spi_write_reg(st, LMP_PWRCN, PWRCN_POWER_DOWN);

	ret = gpiochip_add(&d->chip);
	if (ret) {
		dev_err(&spi->dev, "GPIO registering failed (%d)\n", ret);
		goto error1;
	}

	st->trig = devm_iio_trigger_alloc(&spi->dev, "%s-trigger",
							iio->name);
	if (!st->trig) {
		ret = -ENOMEM;
		dev_err(&iio->dev, "Failed to allocate iio trigger\n");
		goto error2;
	}

	st->trig->ops = &lmp900xx_trigger_ops;
	st->trig->dev.parent = &spi->dev;
	iio_trigger_set_drvdata(st->trig, iio);
	ret = iio_trigger_register(st->trig);
	if (ret)
		goto error2;

	ret = devm_request_threaded_irq(&spi->dev,
			spi->irq, NULL, lmp900xx_irq_handler,
			IRQF_ONESHOT, "lmp900xx", iio);
	if (ret) {
		dev_err(&spi->dev, "Failed request irq\n");
		goto error3;
	}

	ret = iio_triggered_buffer_setup(iio, NULL,
					 &lmp900xx_trigger_handler, NULL);
	if (ret) {
		dev_err(&spi->dev, "Failed to setup triggered buffer\n");
		goto error3;
	}

	ret = iio_device_register(iio);
	if (ret) {
		dev_err(&spi->dev, "Failed to register iio device\n");
		goto error4;
	}

	if (device_create_file(&iio->dev, &dev_attr_lmp900xx_reg))
		dev_err(&iio->dev, "Error on creating sysfs file for lmp900xx_reg\n");
	else
		dev_err(&iio->dev, "created sysfs entry for reading regs\n");
	return 0;
error4:
	iio_triggered_buffer_cleanup(iio);
error3:
	iio_trigger_unregister(st->trig);
error2:
	gpiochip_remove(&d->chip);
error1:
	regulator_disable(st->reg);
	return ret;
}

static int lmp900xx_remove(struct spi_device *spi)
{
	struct iio_dev *iio = spi_get_drvdata(spi);
	struct lmp900xx_state *st = iio_priv(iio);

	iio_device_unregister(iio);
	iio_triggered_buffer_cleanup(iio);
	iio_trigger_unregister(st->trig);
	gpiochip_remove(&st->gpio.chip);
	regulator_disable(st->reg);
	return 0;
}

static const struct spi_device_id lmp900xx_id[] = {
	{ "lmp90079", LMP90079 },
	{ }
};
MODULE_DEVICE_TABLE(spi, lmp900xx_id);

static struct spi_driver lmp900xx_driver = {
	.driver = {
		.name	= "lmp900xx",
	},
	.probe		= lmp900xx_probe,
	.remove		= lmp900xx_remove,
	.id_table	= lmp900xx_id,
};
module_spi_driver(lmp900xx_driver);

MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("TI LMP900XX ADC");
MODULE_LICENSE("GPL v2");
