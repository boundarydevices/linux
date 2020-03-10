/*
 * STMicroelectronics st_asm330lhh FIFO buffer library driver
 *
 * Copyright 2019 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/iio/iio.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/events.h>
#include <linux/iio/buffer.h>
#include <asm/unaligned.h>
#include <linux/iio/buffer.h>
#include "st_asm330lhh.h"

#define ST_ASM330LHH_REG_FIFO_STATUS1_ADDR	0x3a
#define ST_ASM330LHH_REG_TIMESTAMP0_ADDR	0x40
#define ST_ASM330LHH_REG_TIMESTAMP2_ADDR	0x42
#define ST_ASM330LHH_REG_FIFO_DATA_OUT_TAG_ADDR	0x78

#define ST_ASM330LHH_SAMPLE_DISCHARD		0x7ffd

/* Timestamp convergence filter parameter */
#define ST_ASM330LHH_EWMA_LEVEL			120
#define ST_ASM330LHH_EWMA_DIV			128

enum {
	ST_ASM330LHH_GYRO_TAG = 0x01,
	ST_ASM330LHH_ACC_TAG = 0x02,
#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
	ST_ASM330LHH_TEMP_TAG = 0x03,
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */
	ST_ASM330LHH_TS_TAG = 0x04,
};

/* Default timeout before to re-enable gyro */
int delay_gyro = 10;
module_param(delay_gyro, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(delay_gyro, "Delay for Gyro arming");
static bool delayed_enable_gyro;

static inline s64 st_asm330lhh_ewma(s64 old, s64 new, int weight)
{
	s64 diff, incr;

	diff = new - old;
	incr = div_s64((ST_ASM330LHH_EWMA_DIV - weight) * diff,
		       ST_ASM330LHH_EWMA_DIV);

	return old + incr;
}

inline int st_asm330lhh_reset_hwts(struct st_asm330lhh_hw *hw)
{
	u8 data = 0xaa;

	hw->ts = st_asm330lhh_get_time_ns();
	hw->ts_offset = hw->ts;
	hw->val_ts_old = 0;
	hw->hw_ts_high = 0;
	hw->tsample = 0ull;

	return st_asm330lhh_write_atomic(hw, ST_ASM330LHH_REG_TIMESTAMP2_ADDR,
				       sizeof(data), &data);
}

int st_asm330lhh_set_fifo_mode(struct st_asm330lhh_hw *hw,
			     enum st_asm330lhh_fifo_mode fifo_mode)
{
	int err;

	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_FIFO_CTRL4_ADDR,
					 ST_ASM330LHH_REG_FIFO_MODE_MASK,
					 fifo_mode);
	if (err < 0)
		return err;

	hw->fifo_mode = fifo_mode;

	if (fifo_mode == ST_ASM330LHH_FIFO_BYPASS)
		clear_bit(ST_ASM330LHH_HW_OPERATIONAL, &hw->state);
	else
		set_bit(ST_ASM330LHH_HW_OPERATIONAL, &hw->state);

	return 0;
}

int __st_asm330lhh_set_sensor_batching_odr(struct st_asm330lhh_sensor *s,
					 bool enable)
{
	enum st_asm330lhh_sensor_id id = s->id;
	struct st_asm330lhh_hw *hw = s->hw;
	u8 data = 0;
	int err;

	if (enable) {
		err = st_asm330lhh_get_batch_val(s, s->odr, s->uodr, &data);
		if (err < 0)
			return err;
	}

	return __st_asm330lhh_write_with_mask(hw,
					      hw->odr_table_entry[id].batching_reg.addr,
					      hw->odr_table_entry[id].batching_reg.mask,
					      data);
}

static inline int
st_asm330lhh_set_sensor_batching_odr(struct st_asm330lhh_sensor *sensor,
				   bool enable)
{
	struct st_asm330lhh_hw *hw = sensor->hw;
	int err;

	mutex_lock(&hw->page_lock);
	err = __st_asm330lhh_set_sensor_batching_odr(sensor, enable);
	mutex_unlock(&hw->page_lock);

	return err;
}

int st_asm330lhh_update_watermark(struct st_asm330lhh_sensor *sensor,
				  u16 watermark)
{
	u16 fifo_watermark = ST_ASM330LHH_MAX_FIFO_DEPTH, cur_watermark = 0;
	struct st_asm330lhh_hw *hw = sensor->hw;
	struct st_asm330lhh_sensor *cur_sensor;
	__le16 wdata;
	int i, err;
	u8 data;

	for (i = ST_ASM330LHH_ID_GYRO; i < ST_ASM330LHH_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		cur_sensor = iio_priv(hw->iio_devs[i]);

		if (!(hw->enable_mask & BIT(cur_sensor->id)))
			continue;

		cur_watermark = (cur_sensor == sensor) ? watermark
						       : cur_sensor->watermark;

		fifo_watermark = min_t(u16, fifo_watermark, cur_watermark);
	}

	fifo_watermark = max_t(u16, fifo_watermark, 2);

	mutex_lock(&hw->lock);

	err = st_asm330lhh_read_atomic(hw, ST_ASM330LHH_REG_FIFO_CTRL1_ADDR + 1,
				     sizeof(data), &data);
	if (err < 0)
		goto out;

	fifo_watermark = ((data << 8) & ~ST_ASM330LHH_REG_FIFO_WTM_MASK) |
			 (fifo_watermark & ST_ASM330LHH_REG_FIFO_WTM_MASK);
	wdata = cpu_to_le16(fifo_watermark);
	err = st_asm330lhh_write_atomic(hw, ST_ASM330LHH_REG_FIFO_CTRL1_ADDR,
				      sizeof(wdata), (u8 *)&wdata);

out:
	mutex_unlock(&hw->lock);

	return err < 0 ? err : 0;
}

static struct iio_dev *st_asm330lhh_get_iiodev_from_tag(struct st_asm330lhh_hw *hw,
							u8 tag)
{
	struct iio_dev *iio_dev;

	switch (tag) {
	case ST_ASM330LHH_GYRO_TAG:
		iio_dev = hw->iio_devs[ST_ASM330LHH_ID_GYRO];
		break;
	case ST_ASM330LHH_ACC_TAG:
		iio_dev = hw->iio_devs[ST_ASM330LHH_ID_ACC];
		break;
#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
	case ST_ASM330LHH_TEMP_TAG:
		iio_dev = hw->iio_devs[ST_ASM330LHH_ID_TEMP];
		break;
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */
	default:
		iio_dev = NULL;
		break;
	}

	return iio_dev;
}

static inline void st_asm330lhh_sync_hw_ts(struct st_asm330lhh_hw *hw, s64 ts)
{
	s64 delta = ts - hw->hw_ts;

	hw->ts_offset = st_asm330lhh_ewma(hw->ts_offset, delta,
					  ST_ASM330LHH_EWMA_LEVEL);
}

static int st_asm330lhh_read_fifo(struct st_asm330lhh_hw *hw)
{
	u8 iio_buf[ALIGN(ST_ASM330LHH_SAMPLE_SIZE, sizeof(s64)) + sizeof(s64)];
	u8 buf[6 * ST_ASM330LHH_FIFO_SAMPLE_SIZE], tag, *ptr;
	int i, err, word_len, fifo_len, read_len;
	struct iio_dev *iio_dev;
	s64 ts_irq, hw_ts_old;
	__le16 fifo_status;
	u16 fifo_depth;
	s16 drdymask;
	u32 val;

	/* return if FIFO is already disabled */
	if (!test_bit(ST_ASM330LHH_HW_OPERATIONAL, &hw->state)) {
		dev_warn(hw->dev, "%s: FIFO in bypass mode\n", __func__);

		return 0;
	}

	ts_irq = hw->ts - hw->delta_ts;

	err = st_asm330lhh_read_atomic(hw, ST_ASM330LHH_REG_FIFO_STATUS1_ADDR,
				     sizeof(fifo_status), (u8 *)&fifo_status);
	if (err < 0)
		return err;

	fifo_depth = le16_to_cpu(fifo_status) & ST_ASM330LHH_REG_FIFO_STATUS_DIFF;
	if (!fifo_depth)
		return 0;

	fifo_len = fifo_depth * ST_ASM330LHH_FIFO_SAMPLE_SIZE;
	read_len = 0;
	while (read_len < fifo_len) {
		word_len = min_t(int, fifo_len - read_len, sizeof(buf));
		err = st_asm330lhh_read_atomic(hw,
					     ST_ASM330LHH_REG_FIFO_DATA_OUT_TAG_ADDR,
					     word_len, buf);
		if (err < 0)
			return err;

		for (i = 0; i < word_len; i += ST_ASM330LHH_FIFO_SAMPLE_SIZE) {
			ptr = &buf[i + ST_ASM330LHH_TAG_SIZE];
			tag = buf[i] >> 3;

			if (tag == ST_ASM330LHH_TS_TAG) {
				val = get_unaligned_le32(ptr);

				if (hw->val_ts_old > val)
					hw->hw_ts_high++;

				hw_ts_old = hw->hw_ts;

				/* check hw rollover */
				hw->val_ts_old = val;
				hw->hw_ts = (val + ((s64)hw->hw_ts_high << 32)) *
					    hw->ts_delta_ns;
				hw->ts_offset = st_asm330lhh_ewma(hw->ts_offset,
						ts_irq - hw->hw_ts,
						ST_ASM330LHH_EWMA_LEVEL);

				if (!test_bit(ST_ASM330LHH_HW_FLUSH, &hw->state))
					/* sync ap timestamp and sensor one */
					st_asm330lhh_sync_hw_ts(hw, ts_irq);

				ts_irq += hw->hw_ts;

				if (!hw->tsample)
					hw->tsample = hw->ts_offset + hw->hw_ts;
				else
					hw->tsample = hw->tsample + hw->hw_ts - hw_ts_old;
			} else {
				iio_dev = st_asm330lhh_get_iiodev_from_tag(hw, tag);
				if (!iio_dev)
					continue;

				/* skip samples if not ready */
				drdymask = (s16)le16_to_cpu(get_unaligned_le16(ptr));
				if (unlikely(drdymask >= ST_ASM330LHH_SAMPLE_DISCHARD)) {
#ifdef ST_ASM330LHH_DEBUG_DISCHARGE
					struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);

					sensor->discharged_samples++;
#endif /* ST_ASM330LHH_DEBUG_DISCHARGE */
					continue;
				}

				memcpy(iio_buf, ptr, ST_ASM330LHH_SAMPLE_SIZE);

				hw->tsample = min_t(s64,
						    st_asm330lhh_get_time_ns(),
						    hw->tsample);

				iio_push_to_buffers_with_timestamp(iio_dev,
								   iio_buf,
								   hw->tsample);
			}
		}
		read_len += word_len;
	}

	return read_len;
}

ssize_t st_asm330lhh_get_max_watermark(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(dev_get_drvdata(dev));

	return sprintf(buf, "%d\n", sensor->max_watermark);
}

ssize_t st_asm330lhh_get_watermark(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(dev_get_drvdata(dev));

	return sprintf(buf, "%d\n", sensor->watermark);
}

ssize_t st_asm330lhh_set_watermark(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);
	int err, val;

	err = iio_device_claim_direct_mode(iio_dev);
	if (err)
		return err;

	err = kstrtoint(buf, 10, &val);
	if (err < 0)
		goto out;

	err = st_asm330lhh_update_watermark(sensor, val);
	if (err < 0)
		goto out;

	sensor->watermark = val;

out:
	iio_device_release_direct_mode(iio_dev);

	return err < 0 ? err : size;
}

ssize_t st_asm330lhh_flush_fifo(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);
	struct st_asm330lhh_hw *hw = sensor->hw;
	s64 event;
	int count;
	s64 type;
	s64 ts;

	mutex_lock(&hw->fifo_lock);
	ts = st_asm330lhh_get_time_ns();
	hw->delta_ts = ts - hw->ts;
	hw->ts = ts;
	set_bit(ST_ASM330LHH_HW_FLUSH, &hw->state);
	count = st_asm330lhh_read_fifo(hw);
	mutex_unlock(&hw->fifo_lock);

	type = count > 0 ? IIO_EV_DIR_FIFO_DATA : IIO_EV_DIR_FIFO_EMPTY;
	event = IIO_UNMOD_EVENT_CODE(iio_dev->channels[0].type, -1,
				     IIO_EV_TYPE_FIFO_FLUSH, type);
	iio_push_event(iio_dev, event, st_asm330lhh_get_time_ns());

	return size;
}

int st_asm330lhh_suspend_fifo(struct st_asm330lhh_hw *hw)
{
	int err;

	mutex_lock(&hw->fifo_lock);
	st_asm330lhh_read_fifo(hw);
	err = st_asm330lhh_set_fifo_mode(hw, ST_ASM330LHH_FIFO_BYPASS);
	mutex_unlock(&hw->fifo_lock);

	return err;
}

int st_asm330lhh_update_batching(struct iio_dev *iio_dev, bool enable)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);
	struct st_asm330lhh_hw *hw = sensor->hw;
	int err;

	disable_irq(hw->irq);

	err = st_asm330lhh_set_sensor_batching_odr(sensor, enable);
	enable_irq(hw->irq);

	return err;
}

static int st_asm330lhh_update_fifo(struct iio_dev *iio_dev, bool enable)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);
	struct st_asm330lhh_hw *hw = sensor->hw;
	int err;

	if (sensor->id == ST_ASM330LHH_ID_GYRO && !enable)
		delayed_enable_gyro = true;

	if (sensor->id == ST_ASM330LHH_ID_GYRO &&
	    enable && delayed_enable_gyro) {
		delayed_enable_gyro = false;
		msleep(delay_gyro);
	}

	disable_irq(hw->irq);

	err = st_asm330lhh_sensor_set_enable(sensor, enable);
	if (err < 0)
		goto out;

	err = st_asm330lhh_set_sensor_batching_odr(sensor, enable);
	if (err < 0)
		goto out;

#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
	/*
	 * This is an auxiliary sensor, it need to get batched
	 * toghether at least with a primary sensor (Acc/Gyro).
	 */
	if (sensor->id == ST_ASM330LHH_ID_TEMP) {
		if (!(hw->enable_mask & (BIT(ST_ASM330LHH_ID_ACC) |
					 BIT(ST_ASM330LHH_ID_GYRO)))) {
			struct st_asm330lhh_sensor *acc_sensor;
			u8 data = 0;

			acc_sensor = iio_priv(hw->iio_devs[ST_ASM330LHH_ID_ACC]);
			if (enable) {
				err = st_asm330lhh_get_batch_val(acc_sensor,
						sensor->odr, sensor->uodr,
						&data);
				if (err < 0)
					goto out;
			}

			err = st_asm330lhh_write_with_mask(hw,
					hw->odr_table_entry[ST_ASM330LHH_ID_ACC].batching_reg.addr,
					hw->odr_table_entry[ST_ASM330LHH_ID_ACC].batching_reg.mask,
					data);
			if (err < 0)
				goto out;
		}
	}
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */

	err = st_asm330lhh_update_watermark(sensor, sensor->watermark);
	if (err < 0)
		goto out;

	if (enable && hw->fifo_mode == ST_ASM330LHH_FIFO_BYPASS) {
		st_asm330lhh_reset_hwts(hw);
		err = st_asm330lhh_set_fifo_mode(hw, ST_ASM330LHH_FIFO_CONT);
	} else if (!hw->enable_mask) {
		err = st_asm330lhh_set_fifo_mode(hw, ST_ASM330LHH_FIFO_BYPASS);
	}

out:
	enable_irq(hw->irq);

	return err;
}

static irqreturn_t st_asm330lhh_handler_irq(int irq, void *private)
{
	struct st_asm330lhh_hw *hw = (struct st_asm330lhh_hw *)private;
	s64 ts = st_asm330lhh_get_time_ns();

	hw->delta_ts = ts - hw->ts;
	hw->ts = ts;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t st_asm330lhh_handler_thread(int irq, void *private)
{
	struct st_asm330lhh_hw *hw = (struct st_asm330lhh_hw *)private;

	mutex_lock(&hw->fifo_lock);
	st_asm330lhh_read_fifo(hw);
	clear_bit(ST_ASM330LHH_HW_FLUSH, &hw->state);
	mutex_unlock(&hw->fifo_lock);

#ifdef CONFIG_IIO_ST_ASM330LHH_EN_BASIC_FEATURES
	return st_asm330lhh_event_handler(hw);
#else /* CONFIG_IIO_ST_ASM330LHH_EN_BASIC_FEATURES */
	return IRQ_HANDLED;
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_BASIC_FEATURES */
}

static int st_asm330lhh_fifo_preenable(struct iio_dev *iio_dev)
{
	return st_asm330lhh_update_fifo(iio_dev, true);
}

static int st_asm330lhh_fifo_postdisable(struct iio_dev *iio_dev)
{
	return st_asm330lhh_update_fifo(iio_dev, false);
}

static const struct iio_buffer_setup_ops st_asm330lhh_fifo_ops = {
	.preenable = st_asm330lhh_fifo_preenable,
	.postdisable = st_asm330lhh_fifo_postdisable,
};

static int st_asm330lhh_enable_timestamp(struct st_asm330lhh_hw *hw,
					 bool enable)
{
	return st_asm330lhh_write_with_mask(hw,
					    ST_ASM330LHH_REG_FIFO_CTRL4_ADDR,
					    ST_ASM330LHH_REG_DEC_TS_MASK,
					    enable == true ? 1 : 0);
}

static int st_asm330lhh_fifo_init(struct st_asm330lhh_hw *hw)
{
	return st_asm330lhh_enable_timestamp(hw, true);
}

int st_asm330lhh_buffers_setup(struct st_asm330lhh_hw *hw)
{
	struct iio_buffer *buffer;
	unsigned long irq_type;
	bool irq_active_low;
	int i, err;

	irq_type = irqd_get_trigger_type(irq_get_irq_data(hw->irq));
	if (irq_type == IRQF_TRIGGER_NONE)
		irq_type = IRQF_TRIGGER_HIGH;

	switch (irq_type) {
	case IRQF_TRIGGER_HIGH:
	case IRQF_TRIGGER_RISING:
		irq_active_low = false;
		break;
	case IRQF_TRIGGER_LOW:
	case IRQF_TRIGGER_FALLING:
		irq_active_low = true;
		break;
	default:
		dev_info(hw->dev, "mode %lx unsupported\n", irq_type);
		return -EINVAL;
	}

	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL3_C_ADDR,
					 ST_ASM330LHH_REG_H_LACTIVE_MASK,
					 irq_active_low);
	if (err < 0)
		return err;

	if (device_property_read_bool(hw->dev, "drive-open-drain")) {
		err = st_asm330lhh_write_with_mask(hw,
					ST_ASM330LHH_REG_CTRL3_C_ADDR,
					ST_ASM330LHH_REG_PP_OD_MASK,
					1);
		if (err < 0)
			return err;

		irq_type |= IRQF_SHARED;
	}

	err = devm_request_threaded_irq(hw->dev, hw->irq,
					st_asm330lhh_handler_irq,
					st_asm330lhh_handler_thread,
					irq_type | IRQF_ONESHOT,
					"asm330lhh", hw);
	if (err) {
		dev_err(hw->dev, "failed to request trigger irq %d\n",
			hw->irq);
		return err;
	}

	for (i = 0; i < ST_ASM330LHH_ID_EVENT; i++) {
		if (!hw->iio_devs[i])
			continue;

		buffer = devm_iio_kfifo_allocate(hw->dev);
		if (!buffer)
			return -ENOMEM;

		iio_device_attach_buffer(hw->iio_devs[i], buffer);
		hw->iio_devs[i]->modes |= INDIO_BUFFER_SOFTWARE;
		hw->iio_devs[i]->setup_ops = &st_asm330lhh_fifo_ops;
	}

	return st_asm330lhh_fifo_init(hw);
}
