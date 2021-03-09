/*
* Copyright (C) 2012-2019 InvenSense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#define pr_fmt(fmt) "inv_mpu: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/of_device.h>

#include "inv_mpu_iio.h"
#include "inv_mpu_dts.h"

#define INV_SPI_READ 0x80

static int inv_spi_single_write(struct inv_mpu_state *st, u8 reg, u8 data)
{
	struct spi_message msg;
	int res;
	u8 d[2];
	struct spi_transfer xfers = {
		.tx_buf = d,
		.bits_per_word = 8,
		.len = 2,
	};

	pr_debug("reg_write: reg=0x%x data=0x%x\n", reg, data);
	d[0] = reg;
	d[1] = data;
	spi_message_init(&msg);
	spi_message_add_tail(&xfers, &msg);
	res = spi_sync(to_spi_device(st->dev), &msg);

	return res;
}

static int inv_spi_read(struct inv_mpu_state *st, u8 reg, int len, u8 *data)
{
	struct spi_message msg;
	int res;
	u8 d[1];
	struct spi_transfer xfers[] = {
		{
		 .tx_buf = d,
		 .bits_per_word = 8,
		 .len = 1,
		 },
		{
		 .rx_buf = data,
		 .bits_per_word = 8,
		 .len = len,
		 }
	};

	if (!data)
		return -EINVAL;

	d[0] = (reg | INV_SPI_READ);

	spi_message_init(&msg);
	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);
	res = spi_sync(to_spi_device(st->dev), &msg);

	if (len ==1)
		pr_debug("reg_read: reg=0x%x length=%d data=0x%x\n",
							reg, len, data[0]);
	else
		pr_debug("reg_read: reg=0x%x length=%d d0=0x%x d1=0x%x\n",
					reg, len, data[0], data[1]);

	return res;

}

#if defined(CONFIG_INV_MPU_IIO_ICM20648) || \
	defined(CONFIG_INV_MPU_IIO_ICM20608D)
static int inv_spi_mem_write(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		     u32 len, u8 const *data)
{
	struct spi_message msg;
	u8 buf[258];
	int res;

	struct spi_transfer xfers = {
		.tx_buf = buf,
		.bits_per_word = 8,
		.len = len + 1,
	};

	if (!data || !st)
		return -EINVAL;

	if (len > (sizeof(buf) - 1))
		return -ENOMEM;

	inv_plat_single_write(st, REG_MEM_BANK_SEL, mem_addr >> 8);
	inv_plat_single_write(st, REG_MEM_START_ADDR, mem_addr & 0xFF);

	buf[0] = REG_MEM_R_W;
	memcpy(buf + 1, data, len);
	spi_message_init(&msg);
	spi_message_add_tail(&xfers, &msg);
	res = spi_sync(to_spi_device(st->dev), &msg);

	return res;
}

static int inv_spi_mem_read(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		    u32 len, u8 *data)
{
	int res;

	if (!data || !st)
		return -EINVAL;

	if (len > 256)
		return -EINVAL;

	res = inv_plat_single_write(st, REG_MEM_BANK_SEL, mem_addr >> 8);
	res = inv_plat_single_write(st, REG_MEM_START_ADDR, mem_addr & 0xFF);
	res = inv_plat_read(st, REG_MEM_R_W, len, data);

	return res;
}
#endif

/*
 *  inv_mpu_probe() - probe function.
 */
static int inv_mpu_probe(struct spi_device *spi)
{
	const struct spi_device_id *id = spi_get_device_id(spi);
	struct inv_mpu_state *st;
	struct iio_dev *indio_dev;
	int result;

	indio_dev = iio_device_alloc(sizeof(*st));
	if (indio_dev == NULL) {
		pr_err("memory allocation failed\n");
		result = -ENOMEM;
		goto out_no_free;
	}

	st = iio_priv(indio_dev);
	st->write = inv_spi_single_write;
	st->read = inv_spi_read;
#if defined(CONFIG_INV_MPU_IIO_ICM20648) || \
	defined(CONFIG_INV_MPU_IIO_ICM20608D)
	st->mem_write = inv_spi_mem_write;
	st->mem_read = inv_spi_mem_read;
#endif
	st->dev = &spi->dev;
	st->irq = spi->irq;
#if defined(CONFIG_INV_MPU_IIO_ICM42600)
	st->i2c_dis = BIT_UI_SIFS_DISABLE_I2C;
#elif !defined(CONFIG_INV_MPU_IIO_ICM20602) \
	&& !defined(CONFIG_INV_MPU_IIO_IAM20680)
	st->i2c_dis = BIT_I2C_IF_DIS;
#endif
	st->bus_type = BUS_SPI;
	spi_set_drvdata(spi, indio_dev);
	indio_dev->dev.parent = &spi->dev;
	indio_dev->name = id->name;

#ifdef CONFIG_OF
	result = invensense_mpu_parse_dt(st->dev, &st->plat_data);
	if (result)
		goto out_free;
#else
	if (dev_get_platdata(st->dev) == NULL) {
		result = -ENODEV;
		goto out_free;
	}
	st->plat_data = *(struct mpu_platform_data *)dev_get_platdata(st->dev);
#endif
	/* Power on device */
	if (st->plat_data.power_on) {
		result = st->plat_data.power_on(&st->plat_data);
		if (result < 0) {
			dev_err(st->dev, "power_on failed: %d\n", result);
			goto out_free;
		}
		pr_info("%s: power on here.\n", __func__);
	}
	pr_info("%s: power on.\n", __func__);

	msleep(100);

	/* power is turned on inside check chip type */
	result = inv_check_chip_type(indio_dev, id->name);
	if (result)
		goto out_free;

	result = inv_mpu_configure_ring(indio_dev);
	if (result) {
		pr_err("configure ring buffer fail\n");
		goto out_free;
	}

	result = iio_device_register(indio_dev);
	if (result) {
		pr_err("IIO device register fail\n");
		goto out_unreg_ring;
	}

	result = inv_create_dmp_sysfs(indio_dev);
	if (result) {
		pr_err("create dmp sysfs failed\n");
		goto out_unreg_iio;
	}
	init_waitqueue_head(&st->wait_queue);
	st->resume_state = true;
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&st->wake_lock, WAKE_LOCK_SUSPEND, "inv_mpu");
#else
	st->wake_lock = wakeup_source_create("inv_mpu");
	wakeup_source_add(st->wake_lock);
	if (st->wake_lock)
		pr_info("wakeup_source is created successfully\n");
	else
		pr_info("failed to create wakeup_source\n");
#endif
	dev_info(st->dev, "%s ma-kernel-%s is ready to go!\n",
	         indio_dev->name, INVENSENSE_DRIVER_VERSION);

#ifdef SENSOR_DATA_FROM_REGISTERS
	pr_info("Data read from registers\n");
#else
	pr_info("Data read from FIFO\n");
#endif
#ifdef TIMER_BASED_BATCHING
	pr_info("Timer based batching\n");
#endif

	return 0;

out_unreg_iio:
	iio_device_unregister(indio_dev);
out_unreg_ring:
	inv_mpu_unconfigure_ring(indio_dev);
out_free:
	iio_device_free(indio_dev);
out_no_free:
	dev_err(&spi->dev, "%s failed %d\n", __func__, result);
	return result;
}

static void inv_mpu_shutdown(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	inv_switch_power_in_lp(st, true);
	dev_dbg(st->dev, "Shutting down %s...\n", st->hw->name);

	/* reset to make sure previous state are not there */
#if defined(CONFIG_INV_MPU_IIO_ICM42600)
	result = inv_plat_single_write(st, REG_CHIP_CONFIG_REG, BIT_SOFT_RESET);
#else
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_H_RESET);
#endif
	if (result)
		dev_err(st->dev, "Failed to reset %s\n",
			st->hw->name);
	msleep(POWER_UP_TIME);
	/* turn off power to ensure gyro engine is off */
	result = inv_set_power(st, false);
	if (result)
		dev_err(st->dev, "Failed to turn off %s\n",
			st->hw->name);
	inv_switch_power_in_lp(st, false);
	mutex_unlock(&indio_dev->mlock);
}

/*
 *  inv_mpu_remove() - remove function.
 */
static int inv_mpu_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
	struct inv_mpu_state *st = iio_priv(indio_dev);

#ifndef CONFIG_HAS_WAKELOCK
	if (st->wake_lock)
		wakeup_source_destroy(st->wake_lock);
#endif
	iio_device_unregister(indio_dev);
	inv_mpu_unconfigure_ring(indio_dev);
	iio_device_free(indio_dev);
	dev_info(st->dev, "inv-mpu-iio module removed.\n");

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int inv_mpu_spi_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = spi_get_drvdata(to_spi_device(dev));

	return inv_mpu_suspend(indio_dev);
}

static void inv_mpu_spi_complete(struct device *dev)
{
	struct iio_dev *indio_dev = spi_get_drvdata(to_spi_device(dev));

	inv_mpu_complete(indio_dev);
}
#endif

static const struct dev_pm_ops inv_mpu_spi_pmops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = inv_mpu_spi_suspend,
	.complete = inv_mpu_spi_complete,
#endif
};

/* device id table is used to identify what device can be
 * supported by this driver
 */
static const struct spi_device_id inv_mpu_id[] = {
#ifdef CONFIG_INV_MPU_IIO_ICM20648
	{"icm20648", ICM20648},
#else
	{"icm20608d", ICM20608D},
	{"icm20690", ICM20690},
	{"icm20602", ICM20602},
	{"iam20680", IAM20680},
	{"icm42600", ICM42600},
	{"icm42686", ICM42686},
#endif
	{}
};
MODULE_DEVICE_TABLE(spi, inv_mpu_id);

static const struct of_device_id inv_mpu_of_match[] = {
#ifdef CONFIG_INV_MPU_IIO_ICM20648
	{
		.compatible = "invensense,icm20648",
		.data = (void *)ICM20648,
	},
#else
	{
		.compatible = "invensense,icm20608d",
		.data = (void *)ICM20608D,
	}, {
		.compatible = "invensense,icm20690",
		.data = (void *)ICM20690,
	}, {
		.compatible = "invensense,icm20602",
		.data = (void *)ICM20602,
	}, {
		.compatible = "invensense,iam20680",
		.data = (void *)IAM20680,
	}, {
		.compatible = "invensense,icm42600",
		.data = (void *)ICM42600,
	}, {
		.compatible = "invensense,icm42686",
		.data = (void *)ICM42686,
	},
#endif
	{ }
};
MODULE_DEVICE_TABLE(of, inv_mpu_of_match);

static struct spi_driver inv_mpu_driver = {
	.probe = inv_mpu_probe,
	.remove = inv_mpu_remove,
	.shutdown = inv_mpu_shutdown,
	.id_table = inv_mpu_id,
	.driver = {
		.owner = THIS_MODULE,
		.of_match_table = inv_mpu_of_match,
		.name = "inv-mpu-iio-spi",
		.pm = &inv_mpu_spi_pmops,
	},
};
module_spi_driver(inv_mpu_driver);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense SPI device driver");
MODULE_LICENSE("GPL");
