/*
 * STMicroelectronics st_lsm6dso sensor driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef ST_LSM6DSO_H
#define ST_LSM6DSO_H

#include <linux/device.h>
#include <linux/iio/iio.h>
#include <linux/delay.h>

#define ST_LSM6DSO_MAX_ODR 833
#define ST_LSM6DSO_ODR_LIST_SIZE	8

#define ST_LSM6DSO_DEV_NAME		"lsm6dso"

#define ST_LSM6DSO_SAMPLE_SIZE		6
#define ST_LSM6DSO_TS_SAMPLE_SIZE	4
#define ST_LSM6DSO_TAG_SIZE		1
#define ST_LSM6DSO_FIFO_SAMPLE_SIZE	(ST_LSM6DSO_SAMPLE_SIZE + \
					 ST_LSM6DSO_TAG_SIZE)
#define ST_LSM6DSO_MAX_FIFO_DEPTH	416

#define ST_LSM6DSO_REG_FUNC_CFG_ACCESS_ADDR	0x01
#define ST_LSM6DSO_REG_SHUB_REG_MASK		BIT(6)
#define ST_LSM6DSO_REG_FUNC_CFG_MASK		BIT(7)

#define ST_LSM6DSO_DATA_CHANNEL(chan_type, addr, mod, ch2, scan_idx,	\
				rb, sb, sg)				\
{									\
	.type = chan_type,						\
	.address = addr,						\
	.modified = mod,						\
	.channel2 = ch2,						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |			\
			      BIT(IIO_CHAN_INFO_SCALE),			\
	.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
	.scan_index = scan_idx,						\
	.scan_type = {							\
		.sign = sg,						\
		.realbits = rb,						\
		.storagebits = sb,					\
		.endianness = IIO_LE,					\
	},								\
}

static const struct iio_event_spec st_lsm6dso_flush_event = {
	.type = IIO_EV_TYPE_FIFO_FLUSH,
	.dir = IIO_EV_DIR_EITHER,
};

static const struct iio_event_spec st_lsm6dso_thr_event = {
	.type = IIO_EV_TYPE_THRESH,
	.dir = IIO_EV_DIR_RISING,
	.mask_separate = BIT(IIO_EV_INFO_ENABLE),
};

#define ST_LSM6DSO_EVENT_CHANNEL(ctype, etype)		\
{							\
	.type = ctype,					\
	.modified = 0,					\
	.scan_index = -1,				\
	.indexed = -1,					\
	.event_spec = &st_lsm6dso_##etype##_event,	\
	.num_event_specs = 1,				\
}

#define ST_LSM6DSO_RX_MAX_LENGTH	64
#define ST_LSM6DSO_TX_MAX_LENGTH	16

struct st_lsm6dso_transfer_buffer {
	u8 rx_buf[ST_LSM6DSO_RX_MAX_LENGTH];
	u8 tx_buf[ST_LSM6DSO_TX_MAX_LENGTH] ____cacheline_aligned;
};

struct st_lsm6dso_transfer_function {
	int (*read)(struct device *dev, u8 addr, int len, u8 *data);
	int (*write)(struct device *dev, u8 addr, int len, const u8 *data);
};

struct st_lsm6dso_reg {
	u8 addr;
	u8 mask;
};

struct st_lsm6dso_odr {
	u16 hz;
	u8 val;
};

struct st_lsm6dso_odr_table_entry {
	struct st_lsm6dso_reg reg;
	struct st_lsm6dso_odr odr_avl[ST_LSM6DSO_ODR_LIST_SIZE];
};

struct st_lsm6dso_fs {
	struct st_lsm6dso_reg reg;
	u32 gain;
	u8 val;
};

#define ST_LSM6DSO_FS_LIST_SIZE		4
#define ST_LSM6DSO_FS_ACC_LIST_SIZE	4
#define ST_LSM6DSO_FS_GYRO_LIST_SIZE	4
struct st_lsm6dso_fs_table_entry {
	u8 size;
	struct st_lsm6dso_fs fs_avl[ST_LSM6DSO_FS_LIST_SIZE];
};

#define ST_LSM6DSO_ACC_FS_2G_GAIN	IIO_G_TO_M_S_2(61)
#define ST_LSM6DSO_ACC_FS_4G_GAIN	IIO_G_TO_M_S_2(122)
#define ST_LSM6DSO_ACC_FS_8G_GAIN	IIO_G_TO_M_S_2(244)
#define ST_LSM6DSO_ACC_FS_16G_GAIN	IIO_G_TO_M_S_2(488)

#define ST_LSM6DSO_GYRO_FS_250_GAIN	IIO_DEGREE_TO_RAD(8750)
#define ST_LSM6DSO_GYRO_FS_500_GAIN	IIO_DEGREE_TO_RAD(17500)
#define ST_LSM6DSO_GYRO_FS_1000_GAIN	IIO_DEGREE_TO_RAD(35000)
#define ST_LSM6DSO_GYRO_FS_2000_GAIN	IIO_DEGREE_TO_RAD(70000)

struct st_lsm6dso_ext_dev_info {
	const struct st_lsm6dso_ext_dev_settings *ext_dev_settings;
	u8 ext_dev_i2c_addr;
};

enum st_lsm6dso_sensor_id {
	ST_LSM6DSO_ID_GYRO,
	ST_LSM6DSO_ID_ACC,
	ST_LSM6DSO_ID_EXT0,
	ST_LSM6DSO_ID_EXT1,
	ST_LSM6DSO_ID_STEP_COUNTER,
	ST_LSM6DSO_ID_STEP_DETECTOR,
	ST_LSM6DSO_ID_SIGN_MOTION,
	ST_LSM6DSO_ID_GLANCE,
	ST_LSM6DSO_ID_MOTION,
	ST_LSM6DSO_ID_NO_MOTION,
	ST_LSM6DSO_ID_WAKEUP,
	ST_LSM6DSO_ID_PICKUP,
	ST_LSM6DSO_ID_ORIENTATION,
	ST_LSM6DSO_ID_WRIST_TILT,
	ST_LSM6DSO_ID_TILT,
	ST_LSM6DSO_ID_MAX,
};

static const enum st_lsm6dso_sensor_id st_lsm6dso_main_sensor_list[] = {
	 [0] = ST_LSM6DSO_ID_GYRO,
	 [1] = ST_LSM6DSO_ID_ACC,
	 [2] = ST_LSM6DSO_ID_STEP_COUNTER,
	 [3] = ST_LSM6DSO_ID_STEP_DETECTOR,
	 [4] = ST_LSM6DSO_ID_SIGN_MOTION,
	 [5] = ST_LSM6DSO_ID_GLANCE,
	 [6] = ST_LSM6DSO_ID_MOTION,
	 [7] = ST_LSM6DSO_ID_NO_MOTION,
	 [8] = ST_LSM6DSO_ID_WAKEUP,
	 [9] = ST_LSM6DSO_ID_PICKUP,
	[10] = ST_LSM6DSO_ID_ORIENTATION,
	[11] = ST_LSM6DSO_ID_WRIST_TILT,
	[12] = ST_LSM6DSO_ID_TILT,
};

enum st_lsm6dso_fifo_mode {
	ST_LSM6DSO_FIFO_BYPASS = 0x0,
	ST_LSM6DSO_FIFO_CONT = 0x6,
};

enum {
	ST_LSM6DSO_HW_FLUSH,
	ST_LSM6DSO_HW_OPERATIONAL,
};

/**
 * struct st_lsm6dso_sensor - ST IMU sensor instance
 * @id: Sensor identifier.
 * @hw: Pointer to instance of struct st_lsm6dso_hw.
 * @ext_dev_info: Sensor hub i2c slave settings.
 * @trig: Sensor iio trigger.
 * @gain: Configured sensor sensitivity.
 * @odr: Output data rate of the sensor [Hz].
 * @std_samples: Counter of samples to discard during sensor bootstrap.
 * @std_level: Samples to discard threshold.
 * @max_watermark: Max supported watermark level.
 * @watermark: Sensor watermark level.
 * @batch_mask: Sensor mask for FIFO batching register
 * @batch_reg: Batching register info (addr + mask).
 */
struct st_lsm6dso_sensor {
	enum st_lsm6dso_sensor_id id;
	struct st_lsm6dso_hw *hw;

	struct st_lsm6dso_ext_dev_info ext_dev_info;

	struct iio_trigger *trig;

	u32 gain;
	u16 odr;

	u8 std_samples;
	u8 std_level;

	u16 max_watermark;
	u16 watermark;

	struct st_lsm6dso_reg batch_reg;
};

/**
 * struct st_lsm6dso_hw - ST IMU MEMS hw instance
 * @dev: Pointer to instance of struct device (I2C or SPI).
 * @irq: Device interrupt line (I2C or SPI).
 * @lock: Mutex to protect read and write operations.
 * @fifo_lock: Mutex to prevent concurrent access to the hw FIFO.
 * @page_lock: Mutex to prevent concurrent memory page configuration.
 * @fifo_mode: FIFO operating mode supported by the device.
 * @state: hw operational state.
 * @enable_mask: Enabled sensor bitmask.
 * @fsm_enable_mask: FSM Enabled sensor bitmask.
 * @embfunc_irq_reg: Embedded function irq configutation register.
 * @ext_data_len: Number of i2c slave devices connected to I2C master.
 * @ts_offset: Hw timestamp offset.
 * @hw_ts: Latest hw timestamp from the sensor.
 * @hw_ts_high: Manage timestamp rollover
 * @tsample:
 * @hw_ts_old:
 * @delta_ts: Delta time between two consecutive interrupts.
 * @delta_hw_ts:
 * @ts: Latest timestamp from irq handler.
 * @iio_devs: Pointers to acc/gyro iio_dev instances.
 * @tf: Transfer function structure used by I/O operations.
 * @tb: Transfer buffers used by SPI I/O operations.
 */
struct st_lsm6dso_hw {
	struct device *dev;
	int irq;

	struct mutex lock;
	struct mutex fifo_lock;
	struct mutex page_lock;

	enum st_lsm6dso_fifo_mode fifo_mode;
	unsigned long state;
	u32 enable_mask;
	u32 requested_mask;

	u16 fsm_enable_mask;
	u8 embfunc_irq_reg;

	u8 ext_data_len;
	s32 odr_calib;

	/* Timestamp sample ODR */
	u16 odr;

	s64 ts_offset;
	s64 hw_ts;
	s64 hw_ts_high;
	s64 tsample;
	s64 hw_ts_old;
	s64 delta_ts;
	s64 delta_hw_ts;
	s64 ts;

	struct iio_dev *iio_devs[ST_LSM6DSO_ID_MAX];

	const struct st_lsm6dso_transfer_function *tf;
	struct st_lsm6dso_transfer_buffer tb;
};

extern const struct dev_pm_ops st_lsm6dso_pm_ops;

static inline int st_lsm6dso_read_atomic(struct st_lsm6dso_hw *hw, u8 addr,
					 int len, u8 *data)
{
	int err;

	mutex_lock(&hw->page_lock);
	err = hw->tf->read(hw->dev, addr, len, data);
	mutex_unlock(&hw->page_lock);

	return err;
}

static inline int st_lsm6dso_write_atomic(struct st_lsm6dso_hw *hw, u8 addr,
					  int len, u8 *data)
{
	int err;

	mutex_lock(&hw->page_lock);
	err = hw->tf->write(hw->dev, addr, len, data);
	mutex_unlock(&hw->page_lock);

	return err;
}

int __st_lsm6dso_write_with_mask(struct st_lsm6dso_hw *hw, u8 addr, u8 mask,
				 u8 val);
static inline int st_lsm6dso_write_with_mask(struct st_lsm6dso_hw *hw, u8 addr,
					     u8 mask, u8 val)
{
	int err;

	mutex_lock(&hw->page_lock);
	err = __st_lsm6dso_write_with_mask(hw, addr, mask, val);
	mutex_unlock(&hw->page_lock);

	return err;
}

static inline int st_lsm6dso_set_page_access(struct st_lsm6dso_hw *hw,
						u8 mask, u8 data)
{
	int err;

	err = __st_lsm6dso_write_with_mask(hw, ST_LSM6DSO_REG_FUNC_CFG_ACCESS_ADDR,
					   mask, data);
	usleep_range(100, 150);

	return err;
}

static inline bool st_lsm6dso_is_fifo_enabled(struct st_lsm6dso_hw *hw)
{
	return hw->enable_mask & (BIT(ST_LSM6DSO_ID_STEP_COUNTER) |
				  BIT(ST_LSM6DSO_ID_GYRO) |
				  BIT(ST_LSM6DSO_ID_EXT0) |
				  BIT(ST_LSM6DSO_ID_EXT1) |
				  BIT(ST_LSM6DSO_ID_ACC));
}

int st_lsm6dso_probe(struct device *dev, int irq,
		     const struct st_lsm6dso_transfer_function *tf_ops);
int st_lsm6dso_sensor_set_enable(struct st_lsm6dso_sensor *sensor,
				 bool enable);
int st_lsm6dso_shub_set_enable(struct st_lsm6dso_sensor *sensor, bool enable);
int st_lsm6dso_buffers_setup(struct st_lsm6dso_hw *hw);
int st_lsm6dso_get_odr_val(enum st_lsm6dso_sensor_id id, u16 odr, u8 *val);
int st_lsm6dso_update_watermark(struct st_lsm6dso_sensor *sensor,
				u16 watermark);
int st_lsm6dso_shub_probe(struct st_lsm6dso_hw *hw);
ssize_t st_lsm6dso_flush_fifo(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size);
ssize_t st_lsm6dso_get_max_watermark(struct device *dev,
				     struct device_attribute *attr, char *buf);
ssize_t st_lsm6dso_get_watermark(struct device *dev,
				 struct device_attribute *attr, char *buf);
ssize_t st_lsm6dso_set_watermark(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size);
int st_lsm6dso_set_page_access(struct st_lsm6dso_hw *hw, u8 mask, u8 data);
int st_lsm6dso_suspend_fifo(struct st_lsm6dso_hw *hw);
int st_lsm6dso_set_fifo_mode(struct st_lsm6dso_hw *hw,
			     enum st_lsm6dso_fifo_mode fifo_mode);
int __st_lsm6dso_set_sensor_batching_odr(struct st_lsm6dso_sensor *sensor,
					 bool enable);
int st_lsm6dso_fsm_init(struct st_lsm6dso_hw *hw);
int st_lsm6dso_fsm_get_orientation(struct st_lsm6dso_hw *hw, u8 *data);
int st_lsm6dso_embfunc_sensor_set_enable(struct st_lsm6dso_sensor *sensor,
					 bool enable);
int st_lsm6dso_step_counter_set_enable(struct st_lsm6dso_sensor *sensor,
				       bool enable);
int st_lsm6dso_reset_step_counter(struct iio_dev *iio_dev);
#endif /* ST_LSM6DSO_H */
