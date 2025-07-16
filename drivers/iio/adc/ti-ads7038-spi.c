// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * This driver supports TI 12Bit ADC devices
 *
 *	 - ADS7038 with SPI interface
 *
 * Copyright (C) 2023 SYS TEC electronic AG
 * Author: Andre Werner <andre.werner@systec-electronic.com>
 */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/iio/iio.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/types.h>

#include "ti-ads7038.h"

#define ADS7038_OPCODE_NOOP		0x00
#define ADS7038_OPCODE_REGREAD		0x10
#define ADS7038_OPCODE_REGWRITE		0x08
#define ADS7038_OPCODE_SETBIT		0x18
#define ADS7038_OPCODE_CLEATBIT		0x20

/*
 * The bitwidth for ADC channel results differ
 * by setting average and status
 * in the dedicated configuration registers.
 */
#define ADS7038_NO_AVG_NO_STAT		12	/* bits */
#define ADS7038_NO_AVG_STAT		16	/* bits */
#define ADS7038_AVG_NO_STAT		16	/* bits */
#define ADS7038_AVG_STAT		20	/* bits */

#define ADS7038_SPI_FRAME_SIZE_REG		3	/* bytes */
#define ADS7038_SPI_FRAME_SIZE_CHAN_MAX		2	/* elements */

static int ads7038_regmap_spi_read(void *context, unsigned int reg,
				   unsigned int *val)
{
	struct device *const dev = (struct device *)context;
	struct spi_device *const spi = to_spi_device(dev);
	int ret;
	const u8 tx_buf[ADS7038_SPI_FRAME_SIZE_REG] = {
		[0] = ADS7038_OPCODE_REGREAD,
		[1] = (u8)(reg & GENMASK(7, 0)),
		[2] = 0,	/* dummy data */
	};
	u8 rx_buf[ADS7038_SPI_FRAME_SIZE_REG] = { 0 };

	/* Data contains 8bit address and 8bit register data */
	struct spi_transfer xfer[] = {
		{
		 .tx_buf = tx_buf,
		 .rx_buf = NULL,
		 .len = ADS7038_SPI_FRAME_SIZE_REG,
		 .bits_per_word = ADS7038_REGISTER_SIZE,
		 .cs_change = 1,
		  },
		{
		 .tx_buf = NULL,
		 .rx_buf = rx_buf,
		 .len = ADS7038_SPI_FRAME_SIZE_REG,
		 .bits_per_word = ADS7038_REGISTER_SIZE,
		  },
	};

	if (!val)
		return -EINVAL;

	ret = spi_sync_transfer(spi, xfer, ARRAY_SIZE(xfer));

	if (ret != 0)
		return ret;

	*val = (unsigned int)rx_buf[0];

	return ret;
}

static int ads7038_regmap_spi_write(void *context, unsigned int reg,
				    unsigned int val)
{
	unsigned int ret;
	unsigned int regval;
	struct device *const dev = (struct device *)context;
	struct spi_device *const spi = to_spi_device(dev);
	struct iio_dev *const indio_dev =
	    (struct iio_dev *)dev_get_drvdata(dev);
	struct ads7038_data *const data =
	    (struct ads7038_data *)iio_priv(indio_dev);
	const u8 buf[ADS7038_SPI_FRAME_SIZE_REG] = {
		[0] = ADS7038_OPCODE_REGWRITE,
		[1] = (u8)(reg & GENMASK(7, 0)),
		[2] = (u8)(val & GENMASK(7, 0)),
	};

	struct spi_transfer xfer[] = {
		{.tx_buf = buf,
		 .rx_buf = NULL,
		 .len = ARRAY_SIZE(buf) },
	};

	ret = spi_sync_transfer(spi, xfer, ARRAY_SIZE(xfer));

	if (ret != 0)
		goto error;

	/* If status or avaraging is used, the length for the spi output frame changes. */
	if (reg == ADS7038_DATA_CFG_REG || reg == ADS7038_OSR_CFG_REG) {
		ret = ads7038_regmap_spi_read(context, ADS7038_DATA_CFG_REG, &regval);
		if (ret != 0)
			goto error;

		data->faverage = (regval & ADS7038_DATA_CFG_APPEND_STATUS) ? 1 : 0;

		ret = ads7038_regmap_spi_read(context, ADS7038_OSR_CFG_REG, &regval);
		if (ret != 0)
			goto error;

		data->fstatus = (regval & ADS7038_OSR_CFG_OSR) ? 1 : 0;
	}

error:
	return ret;
}

static int ads7038_read_reg(struct device *dev, const unsigned int reg,
			    unsigned int *val)
{
	int ret;
	struct iio_dev *const indio_dev = (struct iio_dev *)dev_get_drvdata(dev);
	struct ads7038_data *const data = (struct ads7038_data *)iio_priv(indio_dev);
	struct regmap *const map = data->regmap;

	ret = regmap_read(map, reg, val);

	return ret;
}

static int ads7038_write_reg(struct device *dev, const unsigned int reg,
			     unsigned int val)
{
	struct iio_dev *const indio_dev = (struct iio_dev *)dev_get_drvdata(dev);
	struct ads7038_data *const data = (struct ads7038_data *)iio_priv(indio_dev);
	struct regmap *const map = data->regmap;
	int ret;

	ret = regmap_write(map, reg, val);

	return ret;
}

static int ads7038_set_mode_manual(struct spi_device *dev,
				   struct ads7038_data *const data)
{
	int ret;
	struct regmap *const map = data->regmap;
	const unsigned int regs[] = { ADS7038_OPMODE_CFG_REG,
		ADS7038_SEQUENCE_CFG_REG
	};
	unsigned int reg_values[2] = { 0 };
	unsigned int idx;

	/* Registers need to be read first to adapt configuration bits. */
	for (idx = 0; idx < ARRAY_SIZE(regs); ++idx) {
		ret = regmap_read(map, regs[idx], &reg_values[idx]);
		if (ret != 0) {
			dev_dbg(&dev->dev,
				"Cannot read value from register %02X.\n",
				regs[idx]);
			break;
		}
	}

	if (ret != 0)
		goto out;

	reg_values[0] &= ~ADS7038_OPMODE_CFG_CONV_MODE;
	reg_values[0] |= ADS7038_OPMODE_CFG_CONV_MODE_MANUAL;
	reg_values[0] &= ~ADS7038_SEQUENCE_CFG_SEQ_MODE;
	reg_values[0] |= ADS7038_SEQUENCE_CFG_SEQ_MODE_MANUAL;

	for (idx = 0; idx < ARRAY_SIZE(regs); ++idx) {
		ret = regmap_write(map, regs[idx], reg_values[idx]);
		if (ret != 0) {
			dev_dbg(&dev->dev,
				"Cannot write value to register %02X.\n",
				regs[idx]);
			break;
		}
	}

out:
	return ret;
}

static int ads7038_read_channel(struct iio_dev *indio_dev,
				const enum MANUAL_CHID chan,
				struct ads7038_ch_meas_result *const res)
{
	int ret;
	struct ads7038_data *const data = (struct ads7038_data *)iio_priv(indio_dev);
	struct spi_device *const spi_dev = to_spi_device(data->dev);
	enum FUNC_MODE *const current_mode = &data->func_mode;
	const u8 chan_buf[ADS7038_SPI_FRAME_SIZE_REG] = {
		[0] = ADS7038_OPCODE_REGWRITE,
		[1] = ADS7038_CHANNEL_SEL_REG,
		[2] = chan,
	};
	u16 rx_buf[ADS7038_SPI_FRAME_SIZE_CHAN_MAX] = { 0 };

	/*
	 * Fixup: If the channel not changes between sequential reads, the first
	 * SPI frame is not necessary. This can be handled separately.
	 */
	struct spi_transfer xfer[] = {
		{
		 .tx_buf = chan_buf,
		 .rx_buf = NULL,
		 .len = ARRAY_SIZE(chan_buf),
		 .bits_per_word = ADS7038_REGISTER_SIZE,
		 .tx_nbits = SPI_NBITS_SINGLE,
		 .cs_change = 1,
		  },
		{
		 .tx_buf = NULL,
		 .rx_buf = rx_buf,
		 .len = sizeof(rx_buf),
		 .tx_nbits = SPI_NBITS_SINGLE,
		 .cs_change = 1,
		  },
		{.tx_buf = NULL,
		 .rx_buf = rx_buf,
		 .len = sizeof(rx_buf),
		 .tx_nbits = SPI_NBITS_SINGLE },
	};

	if (chan > AIN_MAX)
		return -EINVAL;

	mutex_lock(&data->lock);

	/* Configure read manual mode for single read of channel value */
	if (*current_mode != MAN) {
		dev_dbg(&spi_dev->dev, "Set manual mode for reading data.\n");
		ret = ads7038_set_mode_manual(spi_dev, data);
		if (ret != 0)
			goto out;

		*current_mode = MAN;
	}

	ret = spi_sync_transfer(spi_dev, xfer, ARRAY_SIZE(xfer));

	if (ret != 0)
		goto out;

	if (!!data->faverage) {
		res->raw = be16_to_cpup((__be16 *)&rx_buf[0]);
		res->faverage = 1;
		if (!!data->fstatus) {
			res->status = (be16_to_cpup((__be16 *)&rx_buf[1]) & GENMASK(15, 11)) >> 11;
			res->fstatus = 1;
		}
	} else {
		res->raw = be16_to_cpup((__be16 *)&rx_buf[0]);
		res->raw >>= 4;
		if (!!data->fstatus) {
			res->status = (be16_to_cpup((__be16 *)&rx_buf[0]) & GENMASK(3, 0));
			res->fstatus = 1;
		}
	}

out:
	mutex_unlock(&data->lock);
	return ret;
}

static struct regmap_bus ads7038_regmap_bus = {
	.reg_write = ads7038_regmap_spi_write,
	.reg_read = ads7038_regmap_spi_read,
	.reg_format_endian_default = REGMAP_ENDIAN_LITTLE,
	.val_format_endian_default = REGMAP_ENDIAN_LITTLE,
};

static const struct ads7038_info ads7038_info = {
	.read_reg = ads7038_read_reg,
	.write_reg = ads7038_write_reg,
	.read_channel = ads7038_read_channel,
};

static int ads7038_spi_probe(struct spi_device *spi)
{
	const struct spi_device_id *id = spi_get_device_id(spi);
	struct regmap *regmap;
	const struct regmap_config *regmap_config = &ads7038_regmap_config;
	int ret;
	struct regulator *reg;

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_dbg(&spi->dev, "Spi_setup failed!\n");
		goto error_spi;
	}

	regmap = devm_regmap_init(&spi->dev, &ads7038_regmap_bus, &spi->dev,
				  regmap_config);
	if (IS_ERR(regmap)) {
		dev_dbg(&spi->dev, "Failed to allocate register map\n");
		ret = PTR_ERR(regmap);
		goto error_spi;
	}

	reg = devm_regulator_get(&spi->dev, "vref");
	if (IS_ERR(reg)) {
		dev_dbg(&spi->dev, "Failed to get regulator \"vref\"\n");
		ret = PTR_ERR(reg);
		goto error_spi;
	}

	ret = regulator_enable(reg);
	if (ret) {
		dev_dbg(&spi->dev, "Failed to enable regulator \"vref\"\n");
		goto error_spi;
	}

	return ads7038_common_probe(&spi->dev, &ads7038_info, regmap,
				    reg, id->name, spi->irq);

error_spi:
	dev_err(&spi->dev, "Probe failed\n");
	return ret;
}

static void ads7038_spi_remove(struct spi_device *spi)
{
	int ret;

	ret = ads7038_common_remove(&spi->dev);
	if (ret != 0)
		dev_err(&spi->dev, "Error while removing general driver\n.");
}

static const struct of_device_id ads7038_of_spi_match[] = {
	{.compatible = "ti,ads7038" },
	{ },
};

MODULE_DEVICE_TABLE(of, ads7038_of_spi_match);

const struct spi_device_id ads7038_spi_id[] = {
	{.name = "ads7038" },
	{ },
};

MODULE_DEVICE_TABLE(spi, ads7038_spi_id);

static struct spi_driver ads7038_spi_driver = {
	.driver = {
		   .name = "ads7038-spi",
		   .of_match_table = ads7038_of_spi_match,
		    },
	.id_table = ads7038_spi_id,
	.probe = ads7038_spi_probe,
	.remove = ads7038_spi_remove,
};

module_spi_driver(ads7038_spi_driver);

MODULE_AUTHOR("Andre Werner <andre.werner@systec-electronic.com>");
MODULE_DESCRIPTION("ADS7038 SPI bus driver");
MODULE_LICENSE("GPL");
