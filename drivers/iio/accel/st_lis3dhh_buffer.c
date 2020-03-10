/*
 * STMicroelectronics st_lis3dhh sensor driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/iio/events.h>
#include <linux/iio/iio.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include "st_lis3dhh.h"

#define REG_CTRL3_ADDR			0x23
#define REG_CTRL5_ACC_FIFO_EN_MASK	BIT(1)

#define REG_FIFO_CTRL_REG		0x2e
#define REG_FIFO_CTRL_REG_WTM_MASK	GENMASK(4, 0)
#define REG_FIFO_CTRL_MODE_MASK		GENMASK(7, 5)

#define REG_FIFO_SRC_ADDR		0x2f
#define REG_FIFO_SRC_OVR_MASK		BIT(6)
#define REG_FIFO_SRC_FSS_MASK		GENMASK(5, 0)

#define ST_LIS3DHH_MAX_WATERMARK	28

enum st_lis3dhh_fifo_mode {
	ST_LIS3DHH_FIFO_BYPASS = 0x0,
	ST_LIS3DHH_FIFO_STREAM = 0x6,
};

static inline s64 st_lis3dhh_get_timestamp(void)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);

	return timespec_to_ns(&ts);
}

#define ST_LIS3DHH_EWMA_LEVEL			120
#define ST_LIS3DHH_EWMA_DIV			128
static inline s64 st_lis3dhh_ewma(s64 old, s64 new, int weight)
{
	s64 diff, incr;

	diff = new - old;
	incr = div_s64((ST_LIS3DHH_EWMA_DIV - weight) * diff,
		       ST_LIS3DHH_EWMA_DIV);

	return old + incr;
}

static int st_lis3dhh_set_fifo_mode(struct st_lis3dhh_hw *hw,
				    enum st_lis3dhh_fifo_mode mode)
{
	return st_lis3dhh_write_with_mask(hw, REG_FIFO_CTRL_REG,
					  REG_FIFO_CTRL_MODE_MASK, mode);
}

static int st_lis3dhh_read_fifo(struct st_lis3dhh_hw *hw)
{
	u8 iio_buff[ALIGN(ST_LIS3DHH_DATA_SIZE, sizeof(s64)) + sizeof(s64)];
	u8 buff[ST_LIS3DHH_RX_MAX_LENGTH], data, nsamples;
	struct iio_dev *iio_dev = iio_priv_to_dev(hw);
	struct iio_chan_spec const *ch = iio_dev->channels;
	int i, err, word_len, fifo_len, read_len = 0;

	err = st_lis3dhh_spi_read(hw, REG_FIFO_SRC_ADDR, sizeof(data), &data);
	if (err < 0)
		return err;

	nsamples = data & REG_FIFO_SRC_FSS_MASK;
	fifo_len = nsamples * ST_LIS3DHH_DATA_SIZE;

	while (read_len < fifo_len) {
		word_len = min_t(int, fifo_len - read_len, sizeof(buff));
		err = st_lis3dhh_spi_read(hw, ch[0].address, word_len, buff);
		if (err < 0)
			return err;

		for (i = 0; i < word_len; i += ST_LIS3DHH_DATA_SIZE) {
			memcpy(iio_buff, &buff[i], ST_LIS3DHH_DATA_SIZE);
			iio_push_to_buffers_with_timestamp(iio_dev, iio_buff,
							   hw->ts);
			hw->ts += hw->delta_ts;
		}
		read_len += word_len;
	}

	return read_len;
}

ssize_t st_lis3dhh_flush_hwfifo(struct device *device,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(device);
	struct st_lis3dhh_hw *hw = iio_priv(iio_dev);
	s64 code;
	int err;

	mutex_lock(&hw->fifo_lock);

	err = st_lis3dhh_read_fifo(hw);
	hw->ts_irq = st_lis3dhh_get_timestamp();

	mutex_unlock(&hw->fifo_lock);

	code = IIO_UNMOD_EVENT_CODE(IIO_ACCEL, -1,
				    IIO_EV_TYPE_FIFO_FLUSH,
				    IIO_EV_DIR_EITHER);
	iio_push_event(iio_dev, code, hw->ts_irq);

	return err < 0 ? err : size;
}

ssize_t st_lis3dhh_get_hwfifo_watermark(struct device *device,
					struct device_attribute *attr,
					char *buf)
{
	struct iio_dev *iio_dev = dev_get_drvdata(device);
	struct st_lis3dhh_hw *hw = iio_priv(iio_dev);

	return sprintf(buf, "%d\n", hw->watermark);
}

int st_lis3dhh_update_watermark(struct st_lis3dhh_hw *hw, u8 watermark)
{
	return st_lis3dhh_write_with_mask(hw, REG_FIFO_CTRL_REG,
					  REG_FIFO_CTRL_REG_WTM_MASK,
					  watermark);
}

ssize_t st_lis3dhh_set_hwfifo_watermark(struct device *device,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(device);
	struct st_lis3dhh_hw *hw = iio_priv(iio_dev);
	int err, val;

	mutex_lock(&iio_dev->mlock);
	if (iio_buffer_enabled(iio_dev)) {
		err = -EBUSY;
		goto unlock;
	}

	err = kstrtoint(buf, 10, &val);
	if (err < 0)
		goto unlock;

	if (val < 1 || val > ST_LIS3DHH_MAX_WATERMARK) {
		err = -EINVAL;
		goto unlock;
	}

	err = st_lis3dhh_update_watermark(hw, val);
	if (err < 0)
		goto unlock;

	hw->watermark = val;

unlock:
	mutex_unlock(&iio_dev->mlock);

	return err < 0 ? err : size;
}

ssize_t st_lis3dhh_get_max_hwfifo_watermark(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	return sprintf(buf, "%d\n", ST_LIS3DHH_MAX_WATERMARK);
}

static irqreturn_t st_lis3dhh_buffer_handler_irq(int irq, void *private)
{
	struct st_lis3dhh_hw *hw = private;
	s64 ts, delta_ts;

	ts = st_lis3dhh_get_timestamp();
	delta_ts = div_s64(ts - hw->ts_irq, hw->watermark);
	hw->delta_ts = st_lis3dhh_ewma(hw->delta_ts, delta_ts,
				       ST_LIS3DHH_EWMA_LEVEL);
	hw->ts_irq = ts;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t st_lis3dhh_buffer_handler_thread(int irq, void *private)
{
	struct st_lis3dhh_hw *hw = private;

	mutex_lock(&hw->fifo_lock);
	st_lis3dhh_read_fifo(hw);
	mutex_unlock(&hw->fifo_lock);

	return IRQ_HANDLED;
}

static int st_lis3dhh_update_fifo(struct st_lis3dhh_hw *hw, bool enable)
{
	enum st_lis3dhh_fifo_mode mode;
	int err;

	if (enable) {
		hw->ts_irq = hw->ts = st_lis3dhh_get_timestamp();
		hw->delta_ts = div_s64(1000000000LL, ST_LIS3DHH_ODR);
	}

	err = st_lis3dhh_write_with_mask(hw, REG_CTRL3_ADDR,
					 REG_CTRL5_ACC_FIFO_EN_MASK, enable);
	if (err < 0)
		return err;

	mode = enable ? ST_LIS3DHH_FIFO_STREAM : ST_LIS3DHH_FIFO_BYPASS;
	err = st_lis3dhh_set_fifo_mode(hw, mode);
	if (err < 0)
		return err;

	return st_lis3dhh_set_enable(hw, enable);
}

static int st_lis3dhh_buffer_preenable(struct iio_dev *iio_dev)
{
	return st_lis3dhh_update_fifo(iio_priv(iio_dev), true);
}

static int st_lis3dhh_buffer_postdisable(struct iio_dev *iio_dev)
{
	return st_lis3dhh_update_fifo(iio_priv(iio_dev), false);
}

static const struct iio_buffer_setup_ops st_lis3dhh_buffer_ops = {
	.preenable = st_lis3dhh_buffer_preenable,
	.postdisable = st_lis3dhh_buffer_postdisable,
};

int st_lis3dhh_fifo_setup(struct st_lis3dhh_hw *hw)
{
	struct iio_dev *iio_dev = iio_priv_to_dev(hw);
	struct iio_buffer *buffer;
	int ret;

	ret = devm_request_threaded_irq(hw->dev, hw->irq,
					st_lis3dhh_buffer_handler_irq,
					st_lis3dhh_buffer_handler_thread,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					hw->name, hw);
	if (ret) {
		dev_err(hw->dev, "failed to request trigger irq %d\n",
			hw->irq);
		return ret;
	}

	buffer = devm_iio_kfifo_allocate(hw->dev);
	if (!buffer)
		return -ENOMEM;

	iio_device_attach_buffer(iio_dev, buffer);
	iio_dev->setup_ops = &st_lis3dhh_buffer_ops;
	iio_dev->modes |= INDIO_BUFFER_SOFTWARE;

	return 0;
}

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics st_lis3dhh buffer driver");
MODULE_LICENSE("GPL v2");
