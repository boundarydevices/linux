/*
 * drivers/amlogic/iio/adc/meson_saradc.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The sar adc is work in polling mode for single sampling, or work in IRQ mode
 * for periodic sampling.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iio/iio.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/sysfs.h>
#include <linux/amlogic/iomap.h>
#include <dt-bindings/iio/adc/amlogic-saradc.h>
#include <asm/barrier.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/slab.h>

#define MESON_SAR_ADC_REG0					0x00
	#define MESON_SAR_ADC_REG0_PANEL_DETECT			BIT(31)
	#define MESON_SAR_ADC_REG0_BUSY_MASK			GENMASK(30, 28)
	#define MESON_SAR_ADC_REG0_DELTA_BUSY			BIT(30)
	#define MESON_SAR_ADC_REG0_AVG_BUSY			BIT(29)
	#define MESON_SAR_ADC_REG0_SAMPLE_BUSY			BIT(28)
	#define MESON_SAR_ADC_REG0_FIFO_FULL			BIT(27)
	#define MESON_SAR_ADC_REG0_FIFO_EMPTY			BIT(26)
	#define MESON_SAR_ADC_REG0_FIFO_COUNT_MASK		GENMASK(25, 21)
	#define MESON_SAR_ADC_REG0_ADC_BIAS_CTRL_MASK		GENMASK(20, 19)
	#define MESON_SAR_ADC_REG0_CURR_CHAN_ID_MASK		GENMASK(18, 16)
	#define MESON_SAR_ADC_REG0_ADC_TEMP_SEN_SEL		BIT(15)
	#define MESON_SAR_ADC_REG0_SAMPLING_STOP		BIT(14)
	#define MESON_SAR_ADC_REG0_CHAN_DELTA_EN_MASK		GENMASK(13, 12)
	#define MESON_SAR_ADC_REG0_DETECT_IRQ_POL		BIT(10)
	#define MESON_SAR_ADC_REG0_DETECT_IRQ_EN		BIT(9)
	#define MESON_SAR_ADC_REG0_FIFO_CNT_IRQ_MASK		GENMASK(8, 4)
	#define MESON_SAR_ADC_REG0_FIFO_IRQ_EN			BIT(3)
	#define MESON_SAR_ADC_REG0_SAMPLING_START		BIT(2)
	#define MESON_SAR_ADC_REG0_CONTINUOUS_EN		BIT(1)
	#define MESON_SAR_ADC_REG0_SAMPLE_ENGINE_ENABLE		BIT(0)

#define MESON_SAR_ADC_CHAN_LIST					0x04
	#define MESON_SAR_ADC_CHAN_LIST_MAX_INDEX_MASK		GENMASK(26, 24)
	#define MESON_SAR_ADC_CHAN_LIST_ENTRY_SHIFT(_chan)	(_chan * 3)
	#define MESON_SAR_ADC_CHAN_LIST_ENTRY_MASK(_chan)	\
					(GENMASK(2, 0) << ((_chan) * 3))

#define MESON_SAR_ADC_AVG_CNTL					0x08
	#define MESON_SAR_ADC_AVG_CNTL_AVG_MODE_SHIFT(_chan)	\
					(16 + ((_chan) * 2))
	#define MESON_SAR_ADC_AVG_CNTL_AVG_MODE_MASK(_chan)	\
					(GENMASK(17, 16) << ((_chan) * 2))
	#define MESON_SAR_ADC_AVG_CNTL_NUM_SAMPLES_SHIFT(_chan)	\
					(0 + ((_chan) * 2))
	#define MESON_SAR_ADC_AVG_CNTL_NUM_SAMPLES_MASK(_chan)	\
					(GENMASK(1, 0) << ((_chan) * 2))

#define MESON_SAR_ADC_REG3					0x0c
	#define MESON_SAR_ADC_REG3_CNTL_USE_SC_DLY		BIT(31)
	#define MESON_SAR_ADC_REG3_CLK_EN			BIT(30)
	#define MESON_SAR_ADC_REG3_BL30_INITIALIZED		BIT(28)
	#define MESON_SAR_ADC_REG3_CTRL_CONT_RING_COUNTER_EN	BIT(27)
	#define MESON_SAR_ADC_REG3_CTRL_SAMPLING_CLOCK_PHASE	BIT(26)
	#define MESON_SAR_ADC_REG3_CTRL_CHAN7_MUX_SEL_MASK	GENMASK(25, 23)
	#define MESON_SAR_ADC_REG3_DETECT_EN			BIT(22)
	#define MESON_SAR_ADC_REG3_ADC_EN			BIT(21)
	#define MESON_SAR_ADC_REG3_PANEL_DETECT_COUNT_MASK	GENMASK(20, 18)
	#define MESON_SAR_ADC_REG3_PANEL_DETECT_FILTER_TB_MASK	GENMASK(17, 16)
	#define MESON_SAR_ADC_REG3_ADC_CLK_DIV_SHIFT		10
	#define MESON_SAR_ADC_REG3_ADC_CLK_DIV_WIDTH		6
	#define MESON_SAR_ADC_REG3_BLOCK_DLY_SEL_MASK		GENMASK(9, 8)
	#define MESON_SAR_ADC_REG3_BLOCK_DLY_MASK		GENMASK(7, 0)

#define MESON_SAR_ADC_DELAY					0x10
	#define MESON_SAR_ADC_DELAY_INPUT_DLY_SEL_MASK		GENMASK(25, 24)
	#define MESON_SAR_ADC_DELAY_BL30_BUSY			BIT(15)
	#define MESON_SAR_ADC_DELAY_KERNEL_BUSY			BIT(14)
	#define MESON_SAR_ADC_DELAY_INPUT_DLY_CNT_MASK		GENMASK(23, 16)
	#define MESON_SAR_ADC_DELAY_SAMPLE_DLY_SEL_MASK		GENMASK(9, 8)
	#define MESON_SAR_ADC_DELAY_SAMPLE_DLY_CNT_MASK		GENMASK(7, 0)

#define MESON_SAR_ADC_LAST_RD					0x14
	#define MESON_SAR_ADC_LAST_RD_LAST_CHANNEL1_MASK	GENMASK(23, 16)
	#define MESON_SAR_ADC_LAST_RD_LAST_CHANNEL0_MASK	GENMASK(9, 0)

#define MESON_SAR_ADC_FIFO_RD					0x18
	#define MESON_SAR_ADC_FIFO_RD_CHAN_ID_MASK		GENMASK(14, 12)
	#define MESON_SAR_ADC_FIFO_RD_SAMPLE_VALUE_MASK		GENMASK(11, 0)

#define MESON_SAR_ADC_AUX_SW					0x1c
	#define MESON_SAR_ADC_AUX_SW_MUX_SEL_CHAN_MASK(_chan)	\
					(GENMASK(10, 8) << (((_chan) - 2) * 2))
	#define MESON_SAR_ADC_AUX_SW_VREF_P_MUX			BIT(6)
	#define MESON_SAR_ADC_AUX_SW_VREF_N_MUX			BIT(5)
	#define MESON_SAR_ADC_AUX_SW_MODE_SEL			BIT(4)
	#define MESON_SAR_ADC_AUX_SW_YP_DRIVE_SW		BIT(3)
	#define MESON_SAR_ADC_AUX_SW_XP_DRIVE_SW		BIT(2)
	#define MESON_SAR_ADC_AUX_SW_YM_DRIVE_SW		BIT(1)
	#define MESON_SAR_ADC_AUX_SW_XM_DRIVE_SW		BIT(0)

#define MESON_SAR_ADC_CHAN_10_SW				0x20
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_MUX_SEL_MASK	GENMASK(25, 23)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_VREF_P_MUX	BIT(22)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_VREF_N_MUX	BIT(21)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_MODE_SEL		BIT(20)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_YP_DRIVE_SW	BIT(19)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_XP_DRIVE_SW	BIT(18)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_YM_DRIVE_SW	BIT(17)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_XM_DRIVE_SW	BIT(16)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_MUX_SEL_MASK	GENMASK(9, 7)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_VREF_P_MUX	BIT(6)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_VREF_N_MUX	BIT(5)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_MODE_SEL		BIT(4)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_YP_DRIVE_SW	BIT(3)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_XP_DRIVE_SW	BIT(2)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_YM_DRIVE_SW	BIT(1)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_XM_DRIVE_SW	BIT(0)

#define MESON_SAR_ADC_DETECT_IDLE_SW				0x24
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_SW_EN	BIT(26)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_MUX_MASK	GENMASK(25, 23)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_VREF_P_MUX	BIT(22)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_VREF_N_MUX	BIT(21)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_MODE_SEL	BIT(20)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_YP_DRIVE_SW	BIT(19)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_XP_DRIVE_SW	BIT(18)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_YM_DRIVE_SW	BIT(17)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_XM_DRIVE_SW	BIT(16)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_MUX_SEL_MASK	GENMASK(9, 7)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_VREF_P_MUX	BIT(6)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_VREF_N_MUX	BIT(5)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_MODE_SEL	BIT(4)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_YP_DRIVE_SW	BIT(3)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_XP_DRIVE_SW	BIT(2)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_YM_DRIVE_SW	BIT(1)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_XM_DRIVE_SW	BIT(0)

#define MESON_SAR_ADC_DELTA_10					0x28
	#define MESON_SAR_ADC_DELTA_10_TEMP_SEL			BIT(27)
	#define MESON_SAR_ADC_DELTA_10_TS_REVE1			BIT(26)
	#define MESON_SAR_ADC_DELTA_10_CHAN1_DELTA_VALUE_MASK	GENMASK(25, 16)
	#define MESON_SAR_ADC_DELTA_10_TS_REVE0			BIT(15)
	#define MESON_SAR_ADC_DELTA_10_TS_C_SHIFT		11
	#define MESON_SAR_ADC_DELTA_10_TS_C_MASK		GENMASK(14, 11)
	#define MESON_SAR_ADC_DELTA_10_TS_VBG_EN		BIT(10)
	#define MESON_SAR_ADC_DELTA_10_CHAN0_DELTA_VALUE_MASK	GENMASK(9, 0)

/*
 * NOTE: registers from here are undocumented (the vendor Linux kernel driver
 * and u-boot source served as reference). These only seem to be relevant on
 * GXBB and newer.
 */
#define MESON_SAR_ADC_REG11					0x2c
	#define MESON_SAR_ADC_REG11_VREF_SEL			BIT(0)
	#define MESON_SAR_ADC_REG11_BANDGAP_EN			BIT(13)
	#define MESON_SAR_ADC_REG11_CHNL_REGS_EN		BIT(30)
	#define MESON_SAR_ADC_REG11_FIFO_EN			BIT(31)

#define MESON_SAR_ADC_REG13					0x34
	#define MESON_SAR_ADC_REG13_12BIT_CALIBRATION_MASK	GENMASK(13, 8)

/* NOTE: the registers below is introduced first on G12A platform */
#define MESON_SAR_ADC_CHNLX_BASE				0x38
#define MESON_SAR_ADC_CHNLX_SAMPLE_VALUE_SHIFT(_chan)		\
					((_chan) * 16)
#define MESON_SAR_ADC_CHNLX_ID_SHIFT(_chan)			\
					(12 + (_chan) * 16)
#define MESON_SAR_ADC_CHNLX_VALID_SHIFT(_chan)			\
					(15 + (_chan) * 16)
#define MESON_SAR_ADC_CHNL01					0x38
#define MESON_SAR_ADC_CHNL23					0x3c
#define MESON_SAR_ADC_CHNL45					0x40
#define MESON_SAR_ADC_CHNL67					0x44

#define MESON_SAR_ADC_MAX_FIFO_SIZE				32
#define MESON_SAR_ADC_TIMEOUT					100 /* ms */

#define P_HHI_DPLL_TOP_0	0x10c6

/* for use with IIO_VAL_INT_PLUS_MICRO */
#define MILLION							1000000

#define MESON_SAR_ADC_CHAN(_chan) {					\
	.type = IIO_VOLTAGE,						\
	.indexed = 1,							\
	.channel = _chan,						\
	.scan_index = _chan,						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |			\
				BIT(IIO_CHAN_INFO_AVERAGE_RAW) |	\
				BIT(IIO_CHAN_INFO_PROCESSED),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |		\
				BIT(IIO_CHAN_INFO_CALIBBIAS) |		\
				BIT(IIO_CHAN_INFO_CALIBSCALE),		\
	.scan_type = {							\
		.sign = 'u',						\
		.storagebits = 16,					\
		.shift = 0,						\
		.endianness = IIO_CPU,					\
	},								\
	.datasheet_name = "SAR_ADC_CH"#_chan,				\
}

#define IS_CHAN6(_chan) (_chan == 6)

/*unit: v*/
#define SAR_ADC_VREF  1.8

/*default clock for sar adc*/
#define SAR_ADC_CLOCK 1200000

static const char * const chan7_vol[] = {
	"gnd",
	"vdd/4",
	"vdd/2",
	"vdd*3/4",
	"vdd",
	"unused",
	"unused",
	"unused"
};

/*
 * TODO: the hardware supports IIO_TEMP for channel 6 as well which is
 * currently not supported by this driver.
 */
static const struct iio_chan_spec meson_sar_adc_iio_channels[] = {
	MESON_SAR_ADC_CHAN(SARADC_CH0),
	MESON_SAR_ADC_CHAN(SARADC_CH1),
	MESON_SAR_ADC_CHAN(SARADC_CH2),
	MESON_SAR_ADC_CHAN(SARADC_CH3),
	MESON_SAR_ADC_CHAN(SARADC_CH4),
	MESON_SAR_ADC_CHAN(SARADC_CH5),
	MESON_SAR_ADC_CHAN(SARADC_CH6),
	MESON_SAR_ADC_CHAN(SARADC_CH7),
	IIO_CHAN_SOFT_TIMESTAMP(8),
};

enum meson_sar_adc_sample_mode {
	SINGLE_MODE,
	PERIOD_MODE,
	MAX_MODE,
};

enum meson_sar_adc_avg_mode {
	NO_AVERAGING = 0x0,
	MEAN_AVERAGING = 0x1,
	MEDIAN_AVERAGING = 0x2,
};

enum meson_sar_adc_num_samples {
	ONE_SAMPLE = 0x0,
	TWO_SAMPLES = 0x1,
	FOUR_SAMPLES = 0x2,
	EIGHT_SAMPLES = 0x3,
};

enum meson_sar_adc_chan7_mux_sel {
	CHAN7_MUX_VSS = 0x0,
	CHAN7_MUX_VDD_DIV4 = 0x1,
	CHAN7_MUX_VDD_DIV2 = 0x2,
	CHAN7_MUX_VDD_MUL3_DIV4 = 0x3,
	CHAN7_MUX_VDD = 0x4,
	CHAN7_MUX_CH7_INPUT = 0x7,
};

enum meson_sar_adc_resolution {
	SAR_ADC_10BIT = 10,
	SAR_ADC_12BIT = 12,
};

enum register_bit_state {
	BIT_LOW = 0,
	BIT_HIGH = 1,
};

enum vref_select {
	CALIB_VOL_AS_VREF = 0,
	VDDA_AS_VREF = 1,
};

/*
 * struct meson_sar_adc_reg_diff - various information relative to registers
 *
 * @reg3_ring_counter_disable: to disable continuous ring counter.
 * gxl and later: 1; others(gxtvbb etc): 0
 */
struct meson_sar_adc_reg_diff {
	bool		reg3_ring_counter_disable;
};

/*
 * struct meson_sar_adc_data - describe the differences of different platform.
 *
 * @obt_temp_chan6: whether to read data of temp sensor by channel 6
 * @has_bl30_integration:
 * @vref_sel: txlx and later: VDDA; others(txl etc): calibration voltage
 * @period_support: periodic sampling support
 * @has_chnl_regs: whether support for chnl[X] registers
 * @resolution: gxl and later: 12bit; others(gxtvbb etc): 10bit
 * @name:
 * @regs_diff: to describe the differences of the registers
 */
struct meson_sar_adc_data {
	bool			obt_temp_chan6;
	bool			has_bl30_integration;
	bool			vref_sel;
	bool			period_support;
	bool			has_chnl_regs;
	unsigned int		resolution;
	const char		*name;
	struct meson_sar_adc_reg_diff regs_diff;
};

struct meson_sar_adc_priv {
	struct regmap			*regmap;
	const struct meson_sar_adc_data	*data;
	struct clk			*clkin;
	struct clk			*clk81_gate;
	struct clk			*adc_clk;
	struct clk_gate			clk_gate;
	struct clk			*adc_div_clk;
	struct clk_divider		clk_div;
	int				calibbias;
	int				calibscale;
	int				chan7_mux_sel;
	int				delay_per_tick;
	int				ticks_per_period;
	int				active_channel_cnt;
	u8				*datum_buf;
};

static const struct regmap_config meson_sar_adc_regmap_config = {
	.reg_bits = 8,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = MESON_SAR_ADC_CHNL67,
};

static unsigned int meson_sar_adc_get_fifo_count(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	u32 regval;

	regmap_read(priv->regmap, MESON_SAR_ADC_REG0, &regval);

	return FIELD_GET(MESON_SAR_ADC_REG0_FIFO_COUNT_MASK, regval);
}

static int meson_sar_adc_calib_val(struct iio_dev *indio_dev, int val)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int tmp;

	/* use val_calib = scale * val_raw + offset calibration function */
	tmp = div_s64((s64)val * priv->calibscale, MILLION) + priv->calibbias;

	return clamp(tmp, 0, (1 << priv->data->resolution) - 1);
}

static int meson_sar_adc_wait_busy_clear(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int regval, timeout = 10000;

	/*
	 * NOTE: we need a small delay before reading the status, otherwise
	 * the sample engine may not have started internally (which would
	 * seem to us that sampling is already finished).
	 */
	do {
		udelay(1);
		regmap_read(priv->regmap, MESON_SAR_ADC_REG0, &regval);
	} while (FIELD_GET(MESON_SAR_ADC_REG0_BUSY_MASK, regval) && timeout--);

	if (timeout < 0)
		return -ETIMEDOUT;

	return 0;
}

static int meson_sar_adc_read_raw_sample(struct iio_dev *indio_dev,
					 const struct iio_chan_spec *chan,
					 int *val)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int regval, fifo_chan, fifo_val, count;

	if (meson_sar_adc_wait_busy_clear(indio_dev))
		return -ETIMEDOUT;

	count = meson_sar_adc_get_fifo_count(indio_dev);
	if (count != 1) {
		dev_err(&indio_dev->dev,
			"ADC FIFO has %d element(s) instead of one\n", count);
		return -EINVAL;
	}

	regmap_read(priv->regmap, MESON_SAR_ADC_FIFO_RD, &regval);
	fifo_chan = FIELD_GET(MESON_SAR_ADC_FIFO_RD_CHAN_ID_MASK, regval);
	if (fifo_chan != chan->channel) {
		dev_err(&indio_dev->dev,
			"ADC FIFO entry belongs to channel %d instead of %d\n",
			fifo_chan, chan->channel);
		return -EINVAL;
	}

	fifo_val = FIELD_GET(MESON_SAR_ADC_FIFO_RD_SAMPLE_VALUE_MASK, regval);
	fifo_val &= GENMASK(priv->data->resolution - 1, 0);

	/* to fix the sample value by software */
	*val = meson_sar_adc_calib_val(indio_dev, fifo_val);

	return 0;
}

static int meson_sar_adc_read_raw_sample_from_chnl(struct iio_dev *indio_dev,
					const struct iio_chan_spec *chan,
					int *val)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	unsigned int regval;
	int grp_off;
	int chan_off;
	int fifo_chan;
	int fifo_val;
	bool is_valid;

	grp_off = (chan->channel / 2) << 2;
	chan_off = chan->channel % 2;

	regmap_read(priv->regmap,
		MESON_SAR_ADC_CHNLX_BASE + grp_off, &regval);

	is_valid = (regval >> MESON_SAR_ADC_CHNLX_VALID_SHIFT(chan_off)) & 0x1;
	if (!is_valid) {
		dev_err(&indio_dev->dev,
			"ADC chnl reg have no valid sampling data\n");
		return -EINVAL;
	}

	fifo_chan = (regval >> MESON_SAR_ADC_CHNLX_ID_SHIFT(chan_off)) & 0x7;
	if (fifo_chan != chan->channel) {
		dev_err(&indio_dev->dev,
			"ADC Dout entry belongs to channel %d instead of %d\n",
			fifo_chan, chan->channel);
		return -EINVAL;
	}
	fifo_val = regval >> MESON_SAR_ADC_CHNLX_SAMPLE_VALUE_SHIFT(chan_off);
	fifo_val &= GENMASK(priv->data->resolution - 1, 0);

	/* to fix the sample value by software */
	*val = meson_sar_adc_calib_val(indio_dev, fifo_val);

	return 0;
}
static void meson_sar_adc_set_averaging(struct iio_dev *indio_dev,
					const struct iio_chan_spec *chan,
					enum meson_sar_adc_avg_mode mode,
					enum meson_sar_adc_num_samples samples)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int val, channel = chan->channel;

	val = samples << MESON_SAR_ADC_AVG_CNTL_NUM_SAMPLES_SHIFT(channel);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_AVG_CNTL,
			   MESON_SAR_ADC_AVG_CNTL_NUM_SAMPLES_MASK(channel),
			   val);

	val = mode << MESON_SAR_ADC_AVG_CNTL_AVG_MODE_SHIFT(channel);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_AVG_CNTL,
			   MESON_SAR_ADC_AVG_CNTL_AVG_MODE_MASK(channel), val);
}

static int meson_sar_adc_temp_sensor_init(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELTA_10,
				MESON_SAR_ADC_DELTA_10_TEMP_SEL,
				FIELD_PREP(MESON_SAR_ADC_DELTA_10_TEMP_SEL, 1));

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELTA_10,
				MESON_SAR_ADC_DELTA_10_TS_REVE0,
				FIELD_PREP(MESON_SAR_ADC_DELTA_10_TS_REVE0, 1));

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELTA_10,
				MESON_SAR_ADC_DELTA_10_TS_REVE1,
				FIELD_PREP(MESON_SAR_ADC_DELTA_10_TS_REVE1, 1));
	return 0;
}

static void meson_sar_adc_enable_channel(struct iio_dev *indio_dev,
					const struct iio_chan_spec *chan,
					unsigned char idx)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	u32 regval;

	/*
	 * the SAR ADC engine allows sampling multiple channels at the same
	 * time. to keep it simple we're only working with one *internal*
	 * channel, which starts counting at index 0 (which means: count = 1).
	 */

	regval = FIELD_PREP(MESON_SAR_ADC_CHAN_LIST_MAX_INDEX_MASK, idx);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_CHAN_LIST,
			   MESON_SAR_ADC_CHAN_LIST_MAX_INDEX_MASK, regval);

	/* map channel index 0 to the channel which we want to read */
	regval = chan->channel << MESON_SAR_ADC_CHAN_LIST_ENTRY_SHIFT(idx),
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_CHAN_LIST,
			   MESON_SAR_ADC_CHAN_LIST_ENTRY_MASK(idx), regval);

	if (IS_CHAN6(chan->channel)) {
		if (priv->data->obt_temp_chan6)
			meson_sar_adc_temp_sensor_init(indio_dev);
		else
			regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELTA_10,
					   MESON_SAR_ADC_DELTA_10_TEMP_SEL, 0);
	}
}

static void meson_sar_adc_set_chan7_mux(struct iio_dev *indio_dev, int sel)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	u32 regval;

	regval = FIELD_PREP(MESON_SAR_ADC_REG3_CTRL_CHAN7_MUX_SEL_MASK, sel);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_CTRL_CHAN7_MUX_SEL_MASK, regval);

	priv->chan7_mux_sel = sel;

	usleep_range(10, 20);
}

static void meson_sar_adc_start_sample_engine(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_SAMPLE_ENGINE_ENABLE,
			   MESON_SAR_ADC_REG0_SAMPLE_ENGINE_ENABLE);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_SAMPLING_START,
			   MESON_SAR_ADC_REG0_SAMPLING_START);
}

static void meson_sar_adc_stop_sample_engine(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_SAMPLING_STOP,
			   MESON_SAR_ADC_REG0_SAMPLING_STOP);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_SAMPLE_ENGINE_ENABLE, 0);
}

static int meson_sar_adc_lock(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int val, timeout = 10000;

	mutex_lock(&indio_dev->mlock);

	if (priv->data->has_bl30_integration) {
		/* wait until BL30 releases it's lock (so we can use
		 * the SAR ADC)
		 */
		do {
			udelay(1);
			regmap_read(priv->regmap, MESON_SAR_ADC_DELAY, &val);
		} while (val & MESON_SAR_ADC_DELAY_BL30_BUSY && timeout--);

		if (timeout < 0)
			return -ETIMEDOUT;

		/* prevent BL30 from using the SAR ADC while we are using it */
		regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELAY,
				   MESON_SAR_ADC_DELAY_KERNEL_BUSY,
				   MESON_SAR_ADC_DELAY_KERNEL_BUSY);
		isb();
		dsb(sy);
		udelay(1);
		regmap_read(priv->regmap, MESON_SAR_ADC_DELAY, &val);
		if (val & MESON_SAR_ADC_DELAY_BL30_BUSY)
			return -ETIMEDOUT;
	}

	return 0;
}

static void meson_sar_adc_unlock(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	if (priv->data->has_bl30_integration)
		/* allow BL30 to use the SAR ADC again */
		regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELAY,
				   MESON_SAR_ADC_DELAY_KERNEL_BUSY, 0);

	mutex_unlock(&indio_dev->mlock);
}

static void meson_sar_adc_clear_fifo(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	unsigned int count, tmp;

	for (count = 0; count < MESON_SAR_ADC_MAX_FIFO_SIZE; count++) {
		if (!meson_sar_adc_get_fifo_count(indio_dev))
			break;

		regmap_read(priv->regmap, MESON_SAR_ADC_FIFO_RD, &tmp);
	}
}

static int meson_sar_adc_get_sample(struct iio_dev *indio_dev,
				    const struct iio_chan_spec *chan,
				    enum meson_sar_adc_avg_mode avg_mode,
				    enum meson_sar_adc_num_samples avg_samples,
				    int *val)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int ret;

	ret = meson_sar_adc_lock(indio_dev);
	if (ret) {
		meson_sar_adc_unlock(indio_dev);
		return ret;
	}

	if (iio_buffer_enabled(indio_dev)) {
		if (priv->data->has_chnl_regs) {
			ret = meson_sar_adc_read_raw_sample_from_chnl(indio_dev,
				chan, val);
			meson_sar_adc_unlock(indio_dev);

			return  (ret == 0) ? IIO_VAL_INT : ret;
		}
		meson_sar_adc_unlock(indio_dev);
		return -EBUSY;
	}

	/* clear the FIFO to make sure we're not reading old values */
	meson_sar_adc_clear_fifo(indio_dev);

	meson_sar_adc_set_averaging(indio_dev, chan, avg_mode, avg_samples);

	meson_sar_adc_enable_channel(indio_dev, chan, 0);

	meson_sar_adc_start_sample_engine(indio_dev);
	ret = meson_sar_adc_read_raw_sample(indio_dev, chan, val);
	meson_sar_adc_stop_sample_engine(indio_dev);

	meson_sar_adc_unlock(indio_dev);

	if (ret) {
		dev_warn(indio_dev->dev.parent,
			 "failed to read sample for channel %d: %d\n",
			 chan->channel, ret);
		return ret;
	}
	return IIO_VAL_INT;
}

static int meson_sar_adc_temp_sensor_calib(struct iio_dev *indio_dev,
				int factor)
{
		struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
		int tmp;

		regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELTA_10,
			MESON_SAR_ADC_DELTA_10_TS_C_MASK,
			FIELD_PREP(MESON_SAR_ADC_DELTA_10_TS_C_MASK,
				(factor & 0xf)));

		tmp = aml_read_cbus(P_HHI_DPLL_TOP_0);
		tmp = (tmp & (~(1 << 9))) | (((factor >> 4) & 0x1) << 9);
		aml_write_cbus(P_HHI_DPLL_TOP_0, tmp);

		return 0;
}
static int meson_sar_adc_iio_info_read_raw(struct iio_dev *indio_dev,
					   const struct iio_chan_spec *chan,
					   int *val, int *val2, long mask)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		return meson_sar_adc_get_sample(indio_dev, chan, NO_AVERAGING,
						ONE_SAMPLE, val);

	case IIO_CHAN_INFO_AVERAGE_RAW:
		return meson_sar_adc_get_sample(indio_dev, chan,
						MEAN_AVERAGING, EIGHT_SAMPLES,
						val);

	case IIO_CHAN_INFO_PROCESSED: /* return the 10bit sample value */
		ret = meson_sar_adc_get_sample(indio_dev, chan, NO_AVERAGING,
						ONE_SAMPLE, val);
		if (priv->data->resolution == SAR_ADC_12BIT)
			*val = *val >> 2;

		return ret;

	case IIO_CHAN_INFO_SCALE:
		*val = SAR_ADC_VREF * MILLION;
		*val2 = priv->data->resolution;
		return IIO_VAL_FRACTIONAL_LOG2;

	case IIO_CHAN_INFO_CALIBBIAS:
		*val = priv->calibbias;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_CALIBSCALE:
		*val = priv->calibscale / MILLION;
		*val2 = priv->calibscale % MILLION;
		return IIO_VAL_INT_PLUS_MICRO;

	default:
		return -EINVAL;
	}
}

static int meson_sar_adc_iio_info_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val, int val2, long mask)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	switch (mask) {
	/* to set temperature calibration factor */
	/* TODO: there is no better way to set the temperature calibration
	 * factor currently by the consumer API of IIO. use the
	 * mask "IIO_CHAN_INFO_RAW" to pass the parameter temporarily.
	 */
	case IIO_CHAN_INFO_RAW:
		if (priv->data->obt_temp_chan6 && IS_CHAN6(chan->channel)) {
			meson_sar_adc_temp_sensor_calib(indio_dev, val);
			return 0;
		} else
			return -EINVAL;
	default:
		return -EINVAL;
	}
}

static int meson_sar_adc_update_scan_mode(struct iio_dev *indio_dev,
				    const unsigned long *scan_mask)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	kfree(priv->datum_buf);
	priv->datum_buf = kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
	if (!priv->datum_buf)
		return -ENOMEM;

	return 0;
}

static int meson_sar_adc_clk_init(struct iio_dev *indio_dev,

		void __iomem *base)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	struct clk_init_data init;
	const char *clk_parents[1];

	init.name = devm_kasprintf(&indio_dev->dev, GFP_KERNEL, "%s#adc_div",
				   of_node_full_name(indio_dev->dev.of_node));
	init.flags = 0;
	init.ops = &clk_divider_ops;
	clk_parents[0] = __clk_get_name(priv->clkin);
	init.parent_names = clk_parents;
	init.num_parents = 1;

	priv->clk_div.reg = base + MESON_SAR_ADC_REG3;
	priv->clk_div.shift = MESON_SAR_ADC_REG3_ADC_CLK_DIV_SHIFT;
	priv->clk_div.width = MESON_SAR_ADC_REG3_ADC_CLK_DIV_WIDTH;
	priv->clk_div.hw.init = &init;
	priv->clk_div.flags = 0;

	priv->adc_div_clk = devm_clk_register(&indio_dev->dev,
					      &priv->clk_div.hw);
	if (WARN_ON(IS_ERR(priv->adc_div_clk)))
		return PTR_ERR(priv->adc_div_clk);

	init.name = devm_kasprintf(&indio_dev->dev, GFP_KERNEL, "%s#adc_en",
				   of_node_full_name(indio_dev->dev.of_node));
	init.flags = CLK_SET_RATE_PARENT;
	init.ops = &clk_gate_ops;
	clk_parents[0] = __clk_get_name(priv->adc_div_clk);
	init.parent_names = clk_parents;
	init.num_parents = 1;

	priv->clk_gate.reg = base + MESON_SAR_ADC_REG3;
	priv->clk_gate.bit_idx = fls(MESON_SAR_ADC_REG3_CLK_EN);
	priv->clk_gate.hw.init = &init;

	priv->adc_clk = devm_clk_register(&indio_dev->dev, &priv->clk_gate.hw);
	if (WARN_ON(IS_ERR(priv->adc_clk)))
		return PTR_ERR(priv->adc_clk);

	return 0;
}

static int meson_sar_adc_init(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int regval, ret;

	/*
	 * make sure we start at CH7 input since the other muxes are only used
	 * for internal calibration.
	 */
	meson_sar_adc_set_chan7_mux(indio_dev, CHAN7_MUX_CH7_INPUT);

	if (priv->data->has_bl30_integration) {
		/*
		 * leave sampling delay and the input clocks as configured by
		 * BL30 to make sure BL30 gets the values it expects when
		 * reading the temperature sensor.
		 */
		regmap_read(priv->regmap, MESON_SAR_ADC_REG3, &regval);
		if (regval & MESON_SAR_ADC_REG3_BL30_INITIALIZED)
			return 0;
	}

	meson_sar_adc_stop_sample_engine(indio_dev);

	/* update the channel 6 MUX to select the temperature sensor */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			MESON_SAR_ADC_REG0_ADC_TEMP_SEN_SEL,
			MESON_SAR_ADC_REG0_ADC_TEMP_SEN_SEL);

	/* disable all channels by default */
	ret = regmap_write(priv->regmap, MESON_SAR_ADC_CHAN_LIST, 0x0);
	if (ret)
		return ret;

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_CTRL_SAMPLING_CLOCK_PHASE, 0);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_CNTL_USE_SC_DLY,
			   MESON_SAR_ADC_REG3_CNTL_USE_SC_DLY);

	/* delay between two samples = (10+1) * 1uS */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELAY,
			   MESON_SAR_ADC_DELAY_INPUT_DLY_CNT_MASK,
			   FIELD_PREP(MESON_SAR_ADC_DELAY_SAMPLE_DLY_CNT_MASK,
				      10));
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELAY,
			   MESON_SAR_ADC_DELAY_SAMPLE_DLY_SEL_MASK,
			   FIELD_PREP(MESON_SAR_ADC_DELAY_SAMPLE_DLY_SEL_MASK,
				      0));

	/* delay between two samples = (10+1) * 1uS */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELAY,
			   MESON_SAR_ADC_DELAY_INPUT_DLY_CNT_MASK,
			   FIELD_PREP(MESON_SAR_ADC_DELAY_INPUT_DLY_CNT_MASK,
				      10));
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELAY,
			   MESON_SAR_ADC_DELAY_INPUT_DLY_SEL_MASK,
			   FIELD_PREP(MESON_SAR_ADC_DELAY_INPUT_DLY_SEL_MASK,
				      1));
	/* disable internal ring counter */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			MESON_SAR_ADC_REG3_CTRL_CONT_RING_COUNTER_EN,
			FIELD_PREP(MESON_SAR_ADC_REG3_CTRL_CONT_RING_COUNTER_EN,
			priv->data->regs_diff.reg3_ring_counter_disable));
	/* select the vref */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG11,
				MESON_SAR_ADC_REG11_VREF_SEL,
				FIELD_PREP(MESON_SAR_ADC_REG11_VREF_SEL,
					priv->data->vref_sel));

	ret = clk_set_rate(priv->adc_clk, SAR_ADC_CLOCK);
	if (ret) {
		dev_err(indio_dev->dev.parent,
			"failed to set adc clock rate\n");
		return ret;
	}

	return 0;
}

static int meson_sar_adc_hw_enable(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int ret;

	ret = meson_sar_adc_lock(indio_dev);
	if (ret) {
		meson_sar_adc_unlock(indio_dev);
		return ret;
	}

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG11,
			   MESON_SAR_ADC_REG11_BANDGAP_EN,
			   MESON_SAR_ADC_REG11_BANDGAP_EN);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_ADC_EN,
			   MESON_SAR_ADC_REG3_ADC_EN);

	udelay(5);

	if (priv->clk81_gate) {
		ret = clk_prepare_enable(priv->clk81_gate);
		if (ret) {
			dev_err(indio_dev->dev.parent, "failed to enable clk81 gate\n");
			goto err_adc_clk;
		}
	}

	ret = clk_prepare_enable(priv->adc_clk);
	if (ret) {
		dev_err(indio_dev->dev.parent, "failed to enable adc clk\n");
		goto err_adc_clk;
	}

	meson_sar_adc_unlock(indio_dev);

	return 0;

err_adc_clk:
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_ADC_EN, 0);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG11,
			   MESON_SAR_ADC_REG11_BANDGAP_EN, 0);
	meson_sar_adc_unlock(indio_dev);

	return ret;
}

static int meson_sar_adc_hw_disable(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int ret;

	ret = meson_sar_adc_lock(indio_dev);
	if (ret) {
		meson_sar_adc_unlock(indio_dev);
		return ret;
	}
	clk_disable_unprepare(priv->adc_clk);

	if (priv->clk81_gate)
		clk_disable_unprepare(priv->clk81_gate);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_ADC_EN, 0);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG11,
			   MESON_SAR_ADC_REG11_BANDGAP_EN, 0);

	meson_sar_adc_unlock(indio_dev);

	return 0;
}

static irqreturn_t meson_sar_adc_irq(int irq, void *data)
{
	struct iio_dev *indio_dev = data;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	unsigned int cnt, threshold;
	u32 regval;

	regmap_read(priv->regmap, MESON_SAR_ADC_REG0, &regval);
	cnt = FIELD_GET(MESON_SAR_ADC_REG0_FIFO_COUNT_MASK, regval);
	threshold = FIELD_GET(MESON_SAR_ADC_REG0_FIFO_CNT_IRQ_MASK, regval);

	if (cnt < threshold)
		return IRQ_NONE;

	disable_irq_nosync(irq);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t meson_sar_adc_worker(int irq, void *data)
{
	struct iio_dev *indio_dev = data;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	u16 fifo_cnt;
	u16 fifo_val;
	u32 regval;
	u32 i = 0;
	u32 j = 0;

	fifo_cnt = meson_sar_adc_get_fifo_count(indio_dev);

	for (j = 0; j < fifo_cnt; j = j + i) {
		for (i = 0; i < priv->active_channel_cnt; i++) {
			regmap_read(priv->regmap,
				MESON_SAR_ADC_FIFO_RD, &regval);

			fifo_val = FIELD_GET(
				MESON_SAR_ADC_FIFO_RD_SAMPLE_VALUE_MASK,
				regval);
			fifo_val &= GENMASK(priv->data->resolution - 1, 0);

			priv->datum_buf[i*2] = fifo_val & 0xff;
			priv->datum_buf[i*2+1] = (fifo_val >> 8) & 0xff;
		}
		iio_push_to_buffers_with_timestamp(indio_dev, priv->datum_buf,
			iio_get_time_ns(indio_dev));
	}

	meson_sar_adc_clear_fifo(indio_dev);
	enable_irq(irq);

	return IRQ_HANDLED;
}

static int meson_sar_adc_calib(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int ret, nominal0, nominal1, value0, value1;

	/* use points 25% and 75% for calibration */
	nominal0 = (1 << priv->data->resolution) >> 2;
	nominal1 = ((1 << priv->data->resolution) * 3) >> 2;

	meson_sar_adc_set_chan7_mux(indio_dev, CHAN7_MUX_VDD_DIV4);
	usleep_range(10, 20);
	ret = meson_sar_adc_get_sample(indio_dev,
				       &meson_sar_adc_iio_channels[7],
				       MEAN_AVERAGING, EIGHT_SAMPLES, &value0);
	if (ret < 0)
		goto out;

	meson_sar_adc_set_chan7_mux(indio_dev, CHAN7_MUX_VDD_MUL3_DIV4);
	usleep_range(10, 20);
	ret = meson_sar_adc_get_sample(indio_dev,
				       &meson_sar_adc_iio_channels[7],
				       MEAN_AVERAGING, EIGHT_SAMPLES, &value1);
	if (ret < 0)
		goto out;

	if (value1 <= value0) {
		ret = -EINVAL;
		goto out;
	}

	priv->calibscale = div_s64((nominal1 - nominal0) * (s64)MILLION,
				   value1 - value0);
	priv->calibbias = nominal0 - div_s64((s64)value0 * priv->calibscale,
					     MILLION);
	ret = 0;
out:
	meson_sar_adc_set_chan7_mux(indio_dev, CHAN7_MUX_CH7_INPUT);

	return ret;
}

static int meson_sar_adc_sample_mode_set(struct iio_dev *indio_dev,
		enum meson_sar_adc_sample_mode mode)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	if (mode != SINGLE_MODE && mode != PERIOD_MODE)
		return -EINVAL;

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			MESON_SAR_ADC_REG0_SAMPLING_STOP,
			(mode == SINGLE_MODE) ?
			MESON_SAR_ADC_REG0_SAMPLING_STOP : 0);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			MESON_SAR_ADC_REG0_CONTINUOUS_EN,
			(mode == PERIOD_MODE) ?
			MESON_SAR_ADC_REG0_CONTINUOUS_EN : 0);

	return 0;
}

static void meson_sar_adc_chan_spec_update(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	struct iio_chan_spec *chan;
	int i;

	for (i = 0; i < indio_dev->num_channels; i++) {
		chan = (struct iio_chan_spec *)indio_dev->channels + i;
		if (chan->channel < 0)
			continue;
		chan->scan_type.realbits = priv->data->resolution;
	}
}

static int meson_sar_adc_iio_buffer_setup(struct iio_dev *indio_dev,
		irqreturn_t (*pollfunc_bh)(int irq, void *p),
		irqreturn_t (*pollfunc_th)(int irq, void *p),
		int irq, unsigned long flags,
		const struct iio_buffer_setup_ops *setup_ops) {

	struct iio_buffer *buffer;
	int ret;

	buffer = iio_kfifo_allocate();
	if (!buffer)
		return -ENOMEM;

	iio_device_attach_buffer(indio_dev, buffer);

	ret = devm_request_threaded_irq(indio_dev->dev.parent, irq,
			pollfunc_th,
			pollfunc_bh,
			flags,
			indio_dev->name,
			indio_dev);
	if (ret)
		goto error_kfifo_free;

	indio_dev->setup_ops = setup_ops;
	indio_dev->modes |= INDIO_BUFFER_SOFTWARE;

	return 0;

error_kfifo_free:
	iio_kfifo_free(indio_dev->buffer);

	return ret;
}

static int meson_sar_adc_iio_buffer_cleanup(struct iio_dev *indio_dev)
{
	iio_kfifo_free(indio_dev->buffer);

	return 0;
}

static int meson_sar_adc_buffer_postenable(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	const struct iio_chan_spec *chan;
	unsigned char idx = 0;
	unsigned char bit;

	meson_sar_adc_sample_mode_set(indio_dev, PERIOD_MODE);

	/* set sampling period time */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_DELAY_SAMPLE_DLY_SEL_MASK,
			   FIELD_PREP(MESON_SAR_ADC_DELAY_SAMPLE_DLY_SEL_MASK,
				      priv->delay_per_tick));

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_BLOCK_DLY_MASK,
			   FIELD_PREP(MESON_SAR_ADC_REG3_BLOCK_DLY_MASK,
				      priv->ticks_per_period));

	meson_sar_adc_clear_fifo(indio_dev);

	for_each_set_bit(bit, indio_dev->active_scan_mask,
					indio_dev->num_channels) {
		chan = indio_dev->channels + bit;

		if (chan->channel < 0)
			continue;
		meson_sar_adc_enable_channel(indio_dev, chan, idx);
		idx++;
	}

	if (!idx)
		return -EINVAL;
	priv->active_channel_cnt = idx;

	/*
	 * generate interrupt when fifo contains N samples, and the N
	 * is required to align base on the number of active scan channel
	 */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			MESON_SAR_ADC_REG0_FIFO_CNT_IRQ_MASK,
			FIELD_PREP(MESON_SAR_ADC_REG0_FIFO_CNT_IRQ_MASK,
			16 - (16 % idx)));

	/* enable irq */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			MESON_SAR_ADC_REG0_FIFO_IRQ_EN,
			MESON_SAR_ADC_REG0_FIFO_IRQ_EN);

	/*
	 * enable chnl regs which save the sampling value for
	 * individual channel
	 */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG11,
			MESON_SAR_ADC_REG11_CHNL_REGS_EN,
			MESON_SAR_ADC_REG11_CHNL_REGS_EN);

	meson_sar_adc_start_sample_engine(indio_dev);

	return 0;
}

static int meson_sar_adc_buffer_predisable(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	meson_sar_adc_stop_sample_engine(indio_dev);

	meson_sar_adc_sample_mode_set(indio_dev, SINGLE_MODE);

	/* disable irq */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			MESON_SAR_ADC_REG0_FIFO_IRQ_EN, 0);

	/* disable chnl regs */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG11,
			MESON_SAR_ADC_REG11_CHNL_REGS_EN, 0);
	return 0;
}

static const struct iio_buffer_setup_ops meson_sar_adc_buffer_setup_ops = {
	.postenable  = meson_sar_adc_buffer_postenable,
	.predisable = meson_sar_adc_buffer_predisable,
};

static ssize_t chan7_mux_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int len = 0;
	int i;

	len = sprintf(buf, "current: [%d]%s\n\n",
			priv->chan7_mux_sel, chan7_vol[priv->chan7_mux_sel]);
	for (i = 0; i < ARRAY_SIZE(chan7_vol); i++)
		len += sprintf(buf+len, "%d: %s\n", i, chan7_vol[i]);

	return len;
}

static ssize_t chan7_mux_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	int val;

	if (kstrtoint(buf, 0, &val) != 0)
		return -EINVAL;
	if (val >= ARRAY_SIZE(chan7_vol))
		return -EINVAL;
	meson_sar_adc_set_chan7_mux(indio_dev, val);

	return count;
}

static IIO_DEVICE_ATTR(chan7_mux, 0644,
		chan7_mux_show, chan7_mux_store, -1);

static struct attribute *attrs[] = {
	&iio_dev_attr_chan7_mux.dev_attr.attr,
	NULL, /*need to terminate the list of attributes by NULL*/
};

static const struct attribute_group meson_sar_adc_attr_group = {
	.attrs = attrs,
};

static const struct iio_info meson_sar_adc_iio_info = {
	.read_raw = meson_sar_adc_iio_info_read_raw,
	.write_raw = meson_sar_adc_iio_info_write_raw,
	.update_scan_mode = meson_sar_adc_update_scan_mode,
	.attrs = &meson_sar_adc_attr_group,
	.driver_module = THIS_MODULE,
};

struct meson_sar_adc_data meson_sar_adc_g12a_data = {
	.obt_temp_chan6 = false,
	.has_bl30_integration = false,
	.period_support = true,
	.has_chnl_regs = true,
	.vref_sel = VDDA_AS_VREF,
	.resolution = SAR_ADC_12BIT,
	.name = "meson-g12a-saradc",
	.regs_diff = {
		.reg3_ring_counter_disable = BIT_HIGH,
	},
};

struct meson_sar_adc_data meson_sar_adc_txlx_data = {
	.obt_temp_chan6 = false,
	.has_bl30_integration = true,
	.vref_sel = VDDA_AS_VREF,
	.resolution = SAR_ADC_12BIT,
	.name = "meson-txlx-saradc",
	.regs_diff = {
		.reg3_ring_counter_disable = BIT_HIGH,
	},
};

struct meson_sar_adc_data meson_sar_adc_axg_data = {
	.obt_temp_chan6 = false,
	.has_bl30_integration = true,
	.vref_sel = VDDA_AS_VREF,
	.resolution = SAR_ADC_12BIT,
	.name = "meson-axg-saradc",
	.regs_diff = {
		.reg3_ring_counter_disable = BIT_HIGH,
	},
};

struct meson_sar_adc_data meson_sar_adc_txl_data = {
	.obt_temp_chan6 = false,
	.has_bl30_integration = true,
	.vref_sel = CALIB_VOL_AS_VREF,
	.resolution = SAR_ADC_12BIT,
	.name = "meson-txl-saradc",
	.regs_diff = {
		.reg3_ring_counter_disable = BIT_HIGH,
	},
};

struct meson_sar_adc_data meson_sar_adc_gxl_data = {
	.obt_temp_chan6 = false,
	.has_bl30_integration = true,
	.vref_sel = CALIB_VOL_AS_VREF,
	.resolution = SAR_ADC_12BIT,
	.name = "meson-gxl-saradc",
	.regs_diff = {
		.reg3_ring_counter_disable = BIT_HIGH,
	},
};

struct meson_sar_adc_data meson_sar_adc_gxm_data = {
	.obt_temp_chan6 = false,
	.has_bl30_integration = true,
	.vref_sel = CALIB_VOL_AS_VREF,
	.resolution = SAR_ADC_12BIT,
	.name = "meson-gxm-saradc",
	.regs_diff = {
		.reg3_ring_counter_disable = BIT_HIGH,
	},
};

struct meson_sar_adc_data meson_sar_adc_m8b_data = {
	.obt_temp_chan6 = true,
	.has_bl30_integration = true,
	.vref_sel = CALIB_VOL_AS_VREF,
	.resolution = SAR_ADC_10BIT,
	.name = "meson-m8b-saradc",
	.regs_diff = {
		.reg3_ring_counter_disable = BIT_LOW,
	},
};

static const struct of_device_id meson_sar_adc_of_match[] = {
	{
		.compatible = "amlogic,meson-g12a-saradc",
		.data = &meson_sar_adc_g12a_data,
	}, {
		.compatible = "amlogic,meson-txlx-saradc",
		.data = &meson_sar_adc_txlx_data,
	}, {
		.compatible = "amlogic,meson-axg-saradc",
		.data = &meson_sar_adc_axg_data,
	}, {
		.compatible = "amlogic,meson-txl-saradc",
		.data = &meson_sar_adc_txl_data,
	}, {
		.compatible = "amlogic,meson-gxl-saradc",
		.data = &meson_sar_adc_gxl_data,
	}, {
		.compatible = "amlogic,meson-gxm-saradc",
		.data = &meson_sar_adc_gxm_data,
	}, {
		.compatible = "amlogic,meson-m8b-saradc",
		.data = &meson_sar_adc_m8b_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, meson_sar_adc_of_match);

static int meson_sar_adc_probe(struct platform_device *pdev)
{
	struct meson_sar_adc_priv *priv;
	struct iio_dev *indio_dev;
	struct resource *res;
	void __iomem *base;
	const struct of_device_id *match;
	int ret;
	int irq;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*priv));
	if (!indio_dev) {
		dev_err(&pdev->dev, "failed allocating iio device\n");
		return -ENOMEM;
	}

	priv = iio_priv(indio_dev);
	match = of_match_device(meson_sar_adc_of_match, &pdev->dev);
	if (!match)
		return -EINVAL;

	priv->data = match->data;
	indio_dev->name = priv->data->name;
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->dev.of_node = pdev->dev.of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &meson_sar_adc_iio_info;

	indio_dev->channels = meson_sar_adc_iio_channels;
	indio_dev->num_channels = ARRAY_SIZE(meson_sar_adc_iio_channels);

	meson_sar_adc_chan_spec_update(indio_dev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (!irq)
		return -EINVAL;

	priv->regmap = devm_regmap_init_mmio(&pdev->dev, base,
					     &meson_sar_adc_regmap_config);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	priv->clkin = devm_clk_get(&pdev->dev, "xtal");
	if (IS_ERR(priv->clkin)) {
		dev_err(&pdev->dev, "failed to get xtal\n");
		return PTR_ERR(priv->clkin);
	}

	priv->clk81_gate = devm_clk_get(&pdev->dev, "clk81_gate");
	if (IS_ERR(priv->clk81_gate)) {
		if (PTR_ERR(priv->clk81_gate) == -ENOENT) {
			priv->clk81_gate = NULL;
		} else {
			dev_err(&pdev->dev, "failed to get clk81 gate\n");
			return PTR_ERR(priv->clk81_gate);
		}
	}

	priv->adc_clk = devm_clk_get(&pdev->dev, "saradc_clk");
	if (IS_ERR(priv->adc_clk)) {
		if (PTR_ERR(priv->adc_clk) == -ENOENT) {
			priv->adc_clk = NULL;
		} else {
			dev_err(&pdev->dev, "failed to get adc clk\n");
			return PTR_ERR(priv->adc_clk);
		}
	}

	/* on pre-GXBB SoCs the SAR ADC itself provides the ADC clock: */
	if (!priv->adc_clk) {
		ret = meson_sar_adc_clk_init(indio_dev, base);
		if (ret)
			return ret;
	}

	priv->calibscale = MILLION;

	if (priv->data->period_support) {
		ret = of_property_read_u32(pdev->dev.of_node,
			"amlogic,delay-per-tick", &priv->delay_per_tick);
		if (ret) {
			dev_info(&pdev->dev,
				"set delay per tick to <1ms> by default.");
			/* 1ms per tick */
			priv->delay_per_tick = 3;
		}

		ret = of_property_read_u32(pdev->dev.of_node,
			"amlogic,ticks-per-period", &priv->ticks_per_period);
		if (ret) {
			dev_info(&pdev->dev,
				"set ticks per period to <1> by default.");
			/* 1 ticks per sampling period */
			priv->ticks_per_period = 1;
		}

		ret = meson_sar_adc_iio_buffer_setup(indio_dev,
			&meson_sar_adc_worker,
			&meson_sar_adc_irq,
			irq,
			IRQF_SHARED,
			&meson_sar_adc_buffer_setup_ops);

		if (ret)
			return ret;
	}

	ret = meson_sar_adc_init(indio_dev);
	if (ret)
		goto err;

	ret = meson_sar_adc_hw_enable(indio_dev);
	if (ret)
		goto err;

	ret = meson_sar_adc_calib(indio_dev);
	if (ret)
		dev_warn(&pdev->dev, "calibration failed\n");

	platform_set_drvdata(pdev, indio_dev);

	ret = iio_device_register(indio_dev);
	if (ret)
		goto err_hw;

	return 0;

err_hw:
	meson_sar_adc_hw_disable(indio_dev);
err:
	meson_sar_adc_iio_buffer_cleanup(indio_dev);

	return ret;
}

static int meson_sar_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	meson_sar_adc_iio_buffer_cleanup(indio_dev);
	kfree(priv->datum_buf);

	return meson_sar_adc_hw_disable(indio_dev);
}

static int __maybe_unused meson_sar_adc_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int ret;

	if (iio_buffer_enabled(indio_dev)) {
		ret = meson_sar_adc_buffer_predisable(indio_dev);
		if (ret)
			return ret;
	}

	ret = meson_sar_adc_hw_disable(indio_dev);
	if (ret)
		return ret;

	return 0;
}

static int __maybe_unused meson_sar_adc_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int ret;

	ret = meson_sar_adc_hw_enable(indio_dev);
	if (ret)
		return ret;

	if (iio_buffer_enabled(indio_dev)) {
		ret = meson_sar_adc_buffer_postenable(indio_dev);
		if (ret)
			return ret;
	}
	return 0;
}

static void meson_sar_adc_shutdown(struct platform_device *pdev)
{
	meson_sar_adc_suspend(&pdev->dev);
}

static SIMPLE_DEV_PM_OPS(meson_sar_adc_pm_ops,
			 meson_sar_adc_suspend, meson_sar_adc_resume);

static struct platform_driver meson_sar_adc_driver = {
	.probe		= meson_sar_adc_probe,
	.remove		= meson_sar_adc_remove,
	.shutdown	= meson_sar_adc_shutdown,
	.driver		= {
		.name	= "meson-saradc",
		.of_match_table = meson_sar_adc_of_match,
		.pm = &meson_sar_adc_pm_ops,
	},
};

module_platform_driver(meson_sar_adc_driver);

MODULE_AUTHOR("Martin Blumenstingl <martin.blumenstingl@googlemail.com>");
MODULE_DESCRIPTION("Amlogic Meson SAR ADC driver");
MODULE_LICENSE("GPL v2");
