/*
 * STMicroelectronics st_asm330lhh sensor driver
 *
 * Copyright 2018 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef ST_ASM330LHH_H
#define ST_ASM330LHH_H

#include <linux/device.h>
#include <linux/iio/iio.h>
#include <linux/delay.h>

#define ST_ASM330LHH_MAX_ODR 		833
#define ST_ASM330LHH_ODR_LIST_SIZE	8

#define ST_ASM330LHH_DEV_NAME		"asm330lhh"

#define ST_ASM330LHH_SAMPLE_SIZE	6
#define ST_ASM330LHH_TS_SAMPLE_SIZE	4
#define ST_ASM330LHH_TAG_SIZE		1
#define ST_ASM330LHH_FIFO_SAMPLE_SIZE	(ST_ASM330LHH_SAMPLE_SIZE + \
					 ST_ASM330LHH_TAG_SIZE)
#define ST_ASM330LHH_MAX_FIFO_DEPTH	416

#define ST_ASM330LHH_DATA_CHANNEL(chan_type, addr, mod, ch2, scan_idx,	\
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

static const struct iio_event_spec st_asm330lhh_flush_event = {
	.type = IIO_EV_TYPE_FIFO_FLUSH,
	.dir = IIO_EV_DIR_EITHER,
};

static const struct iio_event_spec st_asm330lhh_thr_event = {
	.type = IIO_EV_TYPE_THRESH,
	.dir = IIO_EV_DIR_RISING,
	.mask_separate = BIT(IIO_EV_INFO_ENABLE),
};

#define ST_ASM330LHH_EVENT_CHANNEL(ctype, etype)	\
{							\
	.type = ctype,					\
	.modified = 0,					\
	.scan_index = -1,				\
	.indexed = -1,					\
	.event_spec = &st_asm330lhh_##etype##_event,	\
	.num_event_specs = 1,				\
}

#define ST_ASM330LHH_RX_MAX_LENGTH	64
#define ST_ASM330LHH_TX_MAX_LENGTH	16

struct st_asm330lhh_transfer_buffer {
	u8 rx_buf[ST_ASM330LHH_RX_MAX_LENGTH];
	u8 tx_buf[ST_ASM330LHH_TX_MAX_LENGTH] ____cacheline_aligned;
};

struct st_asm330lhh_transfer_function {
	int (*read)(struct device *dev, u8 addr, int len, u8 *data);
	int (*write)(struct device *dev, u8 addr, int len, const u8 *data);
};

struct st_asm330lhh_reg {
	u8 addr;
	u8 mask;
};

struct st_asm330lhh_odr {
	u16 hz;
	u8 val;
};

struct st_asm330lhh_odr_table_entry {
	struct st_asm330lhh_reg reg;
	struct st_asm330lhh_odr odr_avl[ST_ASM330LHH_ODR_LIST_SIZE];
};

struct st_asm330lhh_fs {
	struct st_asm330lhh_reg reg;
	u32 gain;
	u8 val;
};

#define ST_ASM330LHH_FS_LIST_SIZE		5
#define ST_ASM330LHH_FS_ACC_LIST_SIZE		4
#define ST_ASM330LHH_FS_GYRO_LIST_SIZE		5
#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
#define ST_ASM330LHH_FS_TEMP_LIST_SIZE		1
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */
struct st_asm330lhh_fs_table_entry {
	u8 size;
	struct st_asm330lhh_fs fs_avl[ST_ASM330LHH_FS_LIST_SIZE];
};

#define ST_ASM330LHH_ACC_FS_2G_GAIN	IIO_G_TO_M_S_2(61)
#define ST_ASM330LHH_ACC_FS_4G_GAIN	IIO_G_TO_M_S_2(122)
#define ST_ASM330LHH_ACC_FS_8G_GAIN	IIO_G_TO_M_S_2(244)
#define ST_ASM330LHH_ACC_FS_16G_GAIN	IIO_G_TO_M_S_2(488)

#define ST_ASM330LHH_GYRO_FS_250_GAIN	IIO_DEGREE_TO_RAD(8750)
#define ST_ASM330LHH_GYRO_FS_500_GAIN	IIO_DEGREE_TO_RAD(17500)
#define ST_ASM330LHH_GYRO_FS_1000_GAIN	IIO_DEGREE_TO_RAD(35000)
#define ST_ASM330LHH_GYRO_FS_2000_GAIN	IIO_DEGREE_TO_RAD(70000)
#define ST_ASM330LHH_GYRO_FS_4000_GAIN	IIO_DEGREE_TO_RAD(140000)

struct st_asm330lhh_ext_dev_info {
	const struct st_asm330lhh_ext_dev_settings *ext_dev_settings;
	u8 ext_dev_i2c_addr;
};

enum st_asm330lhh_sensor_id {
	ST_ASM330LHH_ID_GYRO,
	ST_ASM330LHH_ID_ACC,
#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
	ST_ASM330LHH_ID_TEMP,
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */
	ST_ASM330LHH_ID_MAX
};

enum st_asm330lhh_fifo_mode {
	ST_ASM330LHH_FIFO_BYPASS = 0x0,
	ST_ASM330LHH_FIFO_CONT = 0x6,
};

enum {
	ST_ASM330LHH_HW_FLUSH,
	ST_ASM330LHH_HW_OPERATIONAL,
};

/**
 * struct st_asm330lhh_sensor - ST IMU sensor instance
 * @id: Sensor identifier.
 * @hw: Pointer to instance of struct st_asm330lhh_hw.
 * @gain: Configured sensor sensitivity.
 * @odr: Output data rate of the sensor [Hz].
 * @std_samples: Counter of samples to discard during sensor bootstrap.
 * @std_level: Samples to discard threshold.
 * @max_watermark: Max supported watermark level.
 * @watermark: Sensor watermark level.
 * @batch_mask: Sensor mask for FIFO batching register
 * @batch_reg: Batching register info (addr + mask).
 */
struct st_asm330lhh_sensor {
	enum st_asm330lhh_sensor_id id;
	struct st_asm330lhh_hw *hw;

	u32 gain;
	u16 odr;
	u32 offset;

#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
	__le16 old_data;
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */

	u16 max_watermark;
	u16 watermark;

	struct st_asm330lhh_reg batch_reg;
};

/**
 * struct st_asm330lhh_hw - ST IMU MEMS hw instance
 * @dev: Pointer to instance of struct device (I2C or SPI).
 * @irq: Device interrupt line (I2C or SPI).
 * @lock: Mutex to protect read and write operations.
 * @fifo_lock: Mutex to prevent concurrent access to the hw FIFO.
 * @page_lock: Mutex to prevent concurrent memory page configuration.
 * @fifo_mode: FIFO operating mode supported by the device.
 * @state: hw operational state.
 * @enable_mask: Enabled sensor bitmask.
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
struct st_asm330lhh_hw {
	struct device *dev;
	int irq;

	struct mutex lock;
	struct mutex fifo_lock;
	struct mutex page_lock;

	enum st_asm330lhh_fifo_mode fifo_mode;
	unsigned long state;
	u32 enable_mask;
	u32 requested_mask;

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

	struct iio_dev *iio_devs[ST_ASM330LHH_ID_MAX];

	const struct st_asm330lhh_transfer_function *tf;
	struct st_asm330lhh_transfer_buffer tb;
};

extern const struct dev_pm_ops st_asm330lhh_pm_ops;

static inline int st_asm330lhh_read_atomic(struct st_asm330lhh_hw *hw, u8 addr,
					 int len, u8 *data)
{
	int err;

	mutex_lock(&hw->page_lock);
	err = hw->tf->read(hw->dev, addr, len, data);
	mutex_unlock(&hw->page_lock);

	return err;
}

static inline int st_asm330lhh_write_atomic(struct st_asm330lhh_hw *hw, u8 addr,
					  int len, u8 *data)
{
	int err;

	mutex_lock(&hw->page_lock);
	err = hw->tf->write(hw->dev, addr, len, data);
	mutex_unlock(&hw->page_lock);

	return err;
}

int __st_asm330lhh_write_with_mask(struct st_asm330lhh_hw *hw, u8 addr, u8 mask,
				 u8 val);
static inline int st_asm330lhh_write_with_mask(struct st_asm330lhh_hw *hw, u8 addr,
					     u8 mask, u8 val)
{
	int err;

	mutex_lock(&hw->page_lock);
	err = __st_asm330lhh_write_with_mask(hw, addr, mask, val);
	mutex_unlock(&hw->page_lock);

	return err;
}

static inline bool st_asm330lhh_is_fifo_enabled(struct st_asm330lhh_hw *hw)
{
	return hw->enable_mask & (BIT(ST_ASM330LHH_ID_GYRO) |
				  BIT(ST_ASM330LHH_ID_ACC));
}

int st_asm330lhh_probe(struct device *dev, int irq,
		     const struct st_asm330lhh_transfer_function *tf_ops);
int st_asm330lhh_sensor_set_enable(struct st_asm330lhh_sensor *sensor,
				 bool enable);
int st_asm330lhh_buffers_setup(struct st_asm330lhh_hw *hw);
int st_asm330lhh_get_odr_val(enum st_asm330lhh_sensor_id id, u16 odr, u8 *val);
int st_asm330lhh_update_watermark(struct st_asm330lhh_sensor *sensor,
				u16 watermark);
ssize_t st_asm330lhh_flush_fifo(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size);
ssize_t st_asm330lhh_get_max_watermark(struct device *dev,
				     struct device_attribute *attr, char *buf);
ssize_t st_asm330lhh_get_watermark(struct device *dev,
				 struct device_attribute *attr, char *buf);
ssize_t st_asm330lhh_set_watermark(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size);
int st_asm330lhh_suspend_fifo(struct st_asm330lhh_hw *hw);
int st_asm330lhh_set_fifo_mode(struct st_asm330lhh_hw *hw,
			     enum st_asm330lhh_fifo_mode fifo_mode);
int __st_asm330lhh_set_sensor_batching_odr(struct st_asm330lhh_sensor *sensor,
					 bool enable);
int st_asm330lhh_update_batching(struct iio_dev *iio_dev, bool enable);
#endif /* ST_ASM330LHH_H */
