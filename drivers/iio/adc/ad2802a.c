/*
 * Freescale ADC driver
 *
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/regulator/consumer.h>
#include <linux/of_platform.h>
#include <linux/err.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/driver.h>

/* This will be the driver name the kernel reports */
#define DRIVER_NAME "ad2802-adc"

/* ADC register */
#define REG_ADC_CH_A_CFG1		0x00
#define REG_ADC_CH_A_CFG2		0x10
#define REG_ADC_CH_B_CFG1		0x20
#define REG_ADC_CH_B_CFG2		0x30
#define REG_ADC_CH_C_CFG1		0x40
#define REG_ADC_CH_C_CFG2		0x50
#define REG_ADC_CH_D_CFG1		0x60
#define REG_ADC_CH_D_CFG2		0x70
#define REG_ADC_CH_SW_CFG		0x80
#define REG_ADC_TIMER_UNIT		0x90
#define REG_ADC_DMA_FIFO		0xa0
#define REG_ADC_FIFO_STATUS		0xb0
#define REG_ADC_INT_SIG_EN		0xc0
#define REG_ADC_INT_EN			0xd0
#define REG_ADC_INT_STATUS		0xe0
#define REG_ADC_CHA_B_CNV_RSLT		0xf0
#define REG_ADC_CHC_D_CNV_RSLT		0x100
#define REG_ADC_CH_SW_CNV_RSLT		0x110
#define REG_ADC_DMA_FIFO_DAT		0x120
#define REG_ADC_ADC_CFG			0x130

#define CHANNEL_REG_SHIF		0x20

#define CHANNEL_EN			(0x1 << 31)
#define CHANNEL_DISABLE			(0x0 << 31)
#define CHANNEL_SINGLE			(0x1 << 30)
#define CHANNEL_AVG_EN			(0x1 << 29)
#define CHANNEL_SEL_SHIF		24

#define PRE_DIV_4			(0x0 << 29)
#define PRE_DIV_8			(0x1 << 29)
#define PRE_DIV_16			(0x2 << 29)
#define PRE_DIV_32			(0x3 << 29)
#define PRE_DIV_64			(0x4 << 29)
#define PRE_DIV_128			(0x5 << 29)

#define ADC_CLK_DOWN			(0x1 << 31)
#define ADC_POWER_DOWN			(0x1 << 1)
#define ADC_EN				0x1

#define AVG_NUM_4			(0x0 << 12)
#define AVG_NUM_8			(0x1 << 12)
#define AVG_NUM_16			(0x2 << 12)
#define AVG_NUM_32			(0x3 << 12)

#define ad2802_ADC_TIMEOUT		msecs_to_jiffies(100)

#define ad2802_ADC_CHAN(_idx, _chan_type) {			\
	.type = (_chan_type),					\
	.indexed = 1,						\
	.channel = (_idx),					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |	\
				BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
}

enum clk_pre_div {
	CLK_PRE_DIV_4,
	CLK_PRE_DIV_8,
	CLK_PRE_DIV_16,
	CLK_PRE_DIV_32,
	CLK_PRE_DIV_64,
	CLK_PRE_DIV_128,
};

enum average_num {
	AVERAGE_NUM_4,
	AVERAGE_NUM_8,
	AVERAGE_NUM_16,
	AVERAGE_NUM_32,
};

struct ad2802_adc_feature {
	enum clk_pre_div clk_pre_div;
	enum average_num avg_num;

	u32 core_time_unit;	/* define the sample rate */

	bool dma_en;
	bool average_en;
};

struct ad2802_adc {
	struct device *dev;
	void __iomem *regs;
	struct clk *clk;

	u32 vref_uv;
	u32 value;
	u32 channel;
	u32 pre_div_num;

	struct regulator *vref;
	struct ad2802_adc_feature adc_feature;

	struct completion completion;
};

static const struct iio_chan_spec ad2802_adc_iio_channels[] = {
	ad2802_ADC_CHAN(0, IIO_VOLTAGE),
	ad2802_ADC_CHAN(1, IIO_VOLTAGE),
	ad2802_ADC_CHAN(2, IIO_VOLTAGE),
	ad2802_ADC_CHAN(3, IIO_VOLTAGE),
	ad2802_ADC_CHAN(4, IIO_VOLTAGE),
	ad2802_ADC_CHAN(5, IIO_VOLTAGE),
	ad2802_ADC_CHAN(6, IIO_VOLTAGE),
	ad2802_ADC_CHAN(7, IIO_VOLTAGE),
	ad2802_ADC_CHAN(8, IIO_VOLTAGE),
	ad2802_ADC_CHAN(9, IIO_VOLTAGE),
	ad2802_ADC_CHAN(10, IIO_VOLTAGE),
	ad2802_ADC_CHAN(11, IIO_VOLTAGE),
	ad2802_ADC_CHAN(12, IIO_VOLTAGE),
	ad2802_ADC_CHAN(13, IIO_VOLTAGE),
	ad2802_ADC_CHAN(14, IIO_VOLTAGE),
	ad2802_ADC_CHAN(15, IIO_VOLTAGE),
	/* sentinel */
};

static void ad2802_feature_config(struct ad2802_adc *info)
{
	info->adc_feature.clk_pre_div = CLK_PRE_DIV_4;
	info->adc_feature.avg_num = AVERAGE_NUM_32;

	info->adc_feature.core_time_unit = 1;

	info->adc_feature.dma_en = false;
	info->adc_feature.average_en = true;
}

static void ad2802_adc_sample_set(struct ad2802_adc *info)
{
	struct ad2802_adc_feature *adc_feature = &info->adc_feature;
	u32 i;
	u32 time_cfg = 0;
	/* before sample set, disable channel A,B,C,D */
	for (i = 0; i < 4; i++)
		writel(CHANNEL_DISABLE, info->regs + i * CHANNEL_REG_SHIF);
	switch (adc_feature->clk_pre_div) {
	case CLK_PRE_DIV_4:
		time_cfg |= PRE_DIV_4;
		info->pre_div_num = 4;
		break;
	case CLK_PRE_DIV_8:
		time_cfg |= PRE_DIV_8;
		info->pre_div_num = 8;
		break;
	case CLK_PRE_DIV_16:
		time_cfg |= PRE_DIV_16;
		info->pre_div_num = 16;
		break;
	case CLK_PRE_DIV_32:
		time_cfg |= PRE_DIV_32;
		info->pre_div_num = 32;
		break;
	case CLK_PRE_DIV_64:
		time_cfg |= PRE_DIV_64;
		info->pre_div_num = 64;
		break;
	case CLK_PRE_DIV_128:
		time_cfg |= PRE_DIV_128;
		info->pre_div_num = 128;
		break;
	default:
		dev_err(info->dev, "error pre div!\n");
	}

	time_cfg |= adc_feature->core_time_unit;
	writel(time_cfg, info->regs + REG_ADC_TIMER_UNIT);
}

static void ad2802_hw_init(struct ad2802_adc *info)
{
	u32 cfg;
	/* power up and enable adc analogue core */
	cfg = readl(info->regs + REG_ADC_ADC_CFG);
	cfg &= ~(ADC_CLK_DOWN | ADC_POWER_DOWN);
	cfg |= ADC_EN;
	writel(cfg, info->regs + REG_ADC_ADC_CFG);

	/* enable channel A,B,C,D interrupt */
	writel(0xf00, info->regs + REG_ADC_INT_SIG_EN);
	writel(0xf00, info->regs + REG_ADC_INT_EN);

	ad2802_adc_sample_set(info);
}

static void ad2802_channel_set(struct ad2802_adc *info)
{
	u32 cfg1 = 0;
	u32 cfg2;
	u32 channel;

	channel = info->channel;

	/*
	 * physical channel 0 chose logical channel A
	 * physical channel 1 chose logical channel B
	 * physical channel 2 chose logical channel C
	 * physical channel 3 chose logical channel D
	 */
	cfg1 |= (CHANNEL_EN | CHANNEL_SINGLE);
	if (info->adc_feature.average_en)
		cfg1 |= CHANNEL_AVG_EN;
	cfg1 |= (channel << CHANNEL_SEL_SHIF);

	cfg2 = readl(info->regs + CHANNEL_REG_SHIF * channel);

	switch (info->adc_feature.avg_num) {
	case AVERAGE_NUM_4:
		cfg2 |= AVG_NUM_4;
		break;
	case AVERAGE_NUM_8:
		cfg2 |= AVG_NUM_8;
		break;
	case AVERAGE_NUM_16:
		cfg2 |= AVG_NUM_16;
		break;
	case AVERAGE_NUM_32:
		cfg2 |= AVG_NUM_32;
		break;
	default:
		dev_err(info->dev, "error average number!\n");
		break;
	}

	writel(cfg2, info->regs + CHANNEL_REG_SHIF * channel + 0x10);
	writel(cfg1, info->regs + CHANNEL_REG_SHIF * channel);
}

static u32 ad2802_get_sample_rate(struct ad2802_adc *info)
{
	/* input clock is always 24MHz */
	u32 input_clk = 24000000;
	u32 analogue_core_clk;
	u32 core_time_unit = info->adc_feature.core_time_unit;
	u32 sample_clk;
	u32 tmp;

	analogue_core_clk = input_clk / info->pre_div_num;
	tmp = (core_time_unit + 1) * 6;
	sample_clk = analogue_core_clk / tmp;

	return sample_clk;
}

static int ad2802_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val,
			int *val2,
			long mask)
{
	struct ad2802_adc *info = iio_priv(indio_dev);

	u32 channel;
	unsigned long ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&indio_dev->mlock);
		reinit_completion(&info->completion);

		channel = (chan->channel) & 0x0f;
		info->channel = channel;
		ad2802_channel_set(info);

		ret = wait_for_completion_interruptible_timeout
				(&info->completion, ad2802_ADC_TIMEOUT);
		if (ret == 0) {
			mutex_unlock(&indio_dev->mlock);
			return -ETIMEDOUT;
		}
		if (ret < 0) {
			mutex_unlock(&indio_dev->mlock);
			return ret;
		}

		*val = info->value;
		mutex_unlock(&indio_dev->mlock);
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		*val = info->vref_uv / 1000;
		*val2 = 12;
		return IIO_VAL_FRACTIONAL_LOG2;

	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = ad2802_get_sample_rate(info);
		*val2 = 0;
		return IIO_VAL_INT;

	default:
		break;
	}

	return -EINVAL;
}

static int ad2802_adc_read_data(struct ad2802_adc *info)
{
	u32 channel;
	u32 value;
	channel = info->channel;
	if (channel < 2)
		value = readl(info->regs + REG_ADC_CHA_B_CNV_RSLT);
	else
		value = readl(info->regs + REG_ADC_CHC_D_CNV_RSLT);
	if (channel & 0x1)
		value = (value >> 16) & 0xFFF;
	else
		value &= 0xFFF;

	return value;
}

static irqreturn_t ad2802_adc_isr(int irq, void *dev_id)
{
	struct ad2802_adc *info = (struct ad2802_adc *)dev_id;
	int status;

	status = readl(info->regs + REG_ADC_INT_STATUS);
	if (status & 0xf00) {
		info->value = ad2802_adc_read_data(info);
		complete(&info->completion);
	}
	writel(0, info->regs + REG_ADC_INT_STATUS);
	return IRQ_HANDLED;
}

static int ad2802_adc_reg_access(struct iio_dev *indio_dev,
			unsigned reg, unsigned writeval,
			unsigned *readval)
{
	struct ad2802_adc *info = iio_priv(indio_dev);
	if ((readval == NULL) ||
		((reg % 4) || (reg > REG_ADC_ADC_CFG)))
		return -EINVAL;

	*readval = readl(info->regs + reg);
	return 0;
}

static const struct iio_info ad2802_adc_iio_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &ad2802_read_raw,
	.debugfs_reg_access = &ad2802_adc_reg_access,
};

static const struct of_device_id ad2802_adc_match[] = {
	{ .compatible = "fsl,imx7d-adc", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ad2802_adc_match);

static int ad2802_adc_probe(struct platform_device *pdev)
{
	struct ad2802_adc *info;
	struct iio_dev *indio_dev;
	struct resource *mem;
	int irq;
	int ret;
	u32 channels;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(struct ad2802_adc));
	if (!indio_dev) {
		dev_err(&pdev->dev, "Failed allocating iio device\n");
		return -ENOMEM;
	}

	info = iio_priv(indio_dev);
	info->dev = &pdev->dev;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	info->regs = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(info->regs))
		return PTR_ERR(info->regs);

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		return -EINVAL;
	}

	ret = devm_request_irq(info->dev, irq,
				ad2802_adc_isr, 0,
				dev_name(&pdev->dev), info);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed requesting irq, irq = %d\n", irq);
		return ret;
	}

	info->clk = devm_clk_get(&pdev->dev, "adc");
	if (IS_ERR(info->clk)) {
		dev_err(&pdev->dev, "failed getting clock, err = %ld\n",
						PTR_ERR(info->clk));
		ret = PTR_ERR(info->clk);
		return ret;
	}

	info->vref = devm_regulator_get(&pdev->dev, "vref");
	if (IS_ERR(info->vref))
		return PTR_ERR(info->vref);

	ret = regulator_enable(info->vref);
	if (ret)
		return ret;

	info->vref_uv = regulator_get_voltage(info->vref);

	platform_set_drvdata(pdev, indio_dev);

	init_completion(&info->completion);

	ret  = of_property_read_u32(pdev->dev.of_node,
					"num-channels", &channels);
	if (ret)
		channels = ARRAY_SIZE(ad2802_adc_iio_channels);

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->dev.of_node = pdev->dev.of_node;
	indio_dev->info = &ad2802_adc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = ad2802_adc_iio_channels;
	indio_dev->num_channels = (int)channels;

	ret = clk_prepare_enable(info->clk);
	if (ret) {
		dev_err(&pdev->dev,
			"Could not prepare or enable the clock.\n");
		goto error_adc_clk_enable;
	}

	ad2802_feature_config(info);
	ad2802_hw_init(info);

	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't register the device.\n");
		goto error_iio_device_register;
	}

	return 0;

error_iio_device_register:
	clk_disable_unprepare(info->clk);
error_adc_clk_enable:
	regulator_disable(info->vref);

	return ret;
}

static int ad2802_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct ad2802_adc *info = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	regulator_disable(info->vref);
	clk_disable_unprepare(info->clk);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ad2802_adc_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ad2802_adc *info = iio_priv(indio_dev);
	u32 adc_cfg;

	adc_cfg = readl(info->regs + REG_ADC_ADC_CFG);
	adc_cfg |= ADC_CLK_DOWN | ADC_POWER_DOWN;
	adc_cfg &= ~ADC_EN;
	writel(adc_cfg, info->regs + REG_ADC_ADC_CFG);

	clk_disable_unprepare(info->clk);
	regulator_disable(info->vref);

	return 0;
}

static int ad2802_adc_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ad2802_adc *info = iio_priv(indio_dev);
	int ret;

	ret = regulator_enable(info->vref);
	if (ret)
		return ret;

	ret = clk_prepare_enable(info->clk);
	if (ret)
		return ret;

	ad2802_hw_init(info);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(ad2802_adc_pm_ops,
			 ad2802_adc_suspend,
			 ad2802_adc_resume);

static struct platform_driver ad2802_driver = {
	.probe		= ad2802_adc_probe,
	.remove		= ad2802_adc_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = ad2802_adc_match,
		.pm	= &ad2802_adc_pm_ops,
	},
};

module_platform_driver(ad2802_driver);

MODULE_AUTHOR("Haibo Chen <b51421@freescale.com>");
MODULE_DESCRIPTION("Freeacale ad2802 ADC driver");
MODULE_LICENSE("GPL v2");
