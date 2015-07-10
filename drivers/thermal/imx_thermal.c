/*
 * Copyright 2013-2015 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/busfreq-imx.h>
#include <linux/clk.h>
#include <linux/cpu_cooling.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/device_cooling.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/types.h>

#define REG_SET		0x4
#define REG_CLR		0x8
#define REG_TOG		0xc

#define MISC0				0x0150
#define MISC0_REFTOP_SELBIASOFF		(1 << 3)
#define MISC1				0x0160
#define MISC1_IRQ_TEMPHIGH		(1 << 29)
#define MISC1_IRQ_TEMPLOW		(1 << 28)
#define MISC1_IRQ_TEMPPANIC		(1 << 27)

#define IMX6_TEMPSENSE0			0x180
#define IMX6_TEMPSENSE1			0x190
#define IMX6_TEMPSENSE2			0x290

#define IMX7_TEMPSENSE0			0x300
#define IMX7_TEMPSENSE1			0x310

#define TEMPSENSE2			0x0290
#define TEMPSENSE2_LOW_VALUE_SHIFT	0
#define TEMPSENSE2_LOW_VALUE_MASK	0xfff
#define TEMPSENSE2_PANIC_VALUE_SHIFT	16
#define TEMPSENSE2_PANIC_VALUE_MASK	0xfff0000

#define IMX6_OCOTP_ANA1			0x04e0
#define IMX7_OCOTP_ANA1			0x04f0

/*
 * It defines the temperature in millicelsius for passive trip point
 * that will trigger cooling action when crossed.
 */
#define IMX_TEMP_PASSIVE		85000
#define IMX_TEMP_PASSIVE_COOL_DELTA     10000

#define IMX_POLLING_DELAY		2000 /* millisecond */
#define IMX_PASSIVE_DELAY		1000

#define FACTOR0				10000000
#define FACTOR1				15423
#define FACTOR2				4148468
#define OFFSET				3580661

#define TEMPMON_V1			1
#define TEMPMON_V2			2
#define TEMPMON_V3			3

#define POWERON				0
#define POWERDOWN			1

#define STOP				0
#define START				1

/* The driver supports 1 passive trip point and 1 critical trip point */
enum imx_thermal_trip {
	IMX_TRIP_PASSIVE,
	IMX_TRIP_CRITICAL,
	IMX_TRIP_NUM,
};

enum imx_reg_field_ids {
	TEMP_VALUE,
	POWER_DOWN,
	MEASURE_START,
	FINISHED,
	MEASURE_FREQ,
	PANIC_ALARM,
	HIGH_ALARM,
	MAX_REGFIELDS
};

struct thermal_soc_data {
	u32 version;
	const struct reg_field *reg_fields;
};

struct imx_thermal_data {
	struct thermal_zone_device *tz;
	struct thermal_cooling_device *cdev[2];
	enum thermal_device_mode mode;
	struct regmap *tempmon;
	struct regmap_field *temp_value;
	struct regmap_field *measure;
	struct regmap_field *finished;
	struct regmap_field *power_down;
	struct regmap_field *measure_freq;
	struct regmap_field *panic_alarm;
	struct regmap_field *high_alarm;
	u32 c1, c2; /* See formula in imx_get_sensor_data() */
	unsigned long temp_passive;
	unsigned long temp_critical;
	unsigned long alarm_temp;
	unsigned long last_temp;
	bool irq_enabled;
	int irq;
	struct clk *thermal_clk;
	struct mutex mutex;
	const struct thermal_soc_data *socdata;
};

static struct reg_field imx6q_regfields[MAX_REGFIELDS] = {
	[TEMP_VALUE] = REG_FIELD(IMX6_TEMPSENSE0, 8, 19),
	[POWER_DOWN] = REG_FIELD(IMX6_TEMPSENSE0, 0, 0),
	[MEASURE_START] = REG_FIELD(IMX6_TEMPSENSE0, 1, 1),
	[FINISHED] = REG_FIELD(IMX6_TEMPSENSE0, 2, 2),
	[MEASURE_FREQ] = REG_FIELD(IMX6_TEMPSENSE1, 0, 15),
	[HIGH_ALARM] = REG_FIELD(IMX6_TEMPSENSE0, 20, 31),
};

static struct reg_field imx6sx_regfields[MAX_REGFIELDS] = {
	[TEMP_VALUE] = REG_FIELD(IMX6_TEMPSENSE0, 8, 19),
	[POWER_DOWN] = REG_FIELD(IMX6_TEMPSENSE0, 0, 0),
	[MEASURE_START] = REG_FIELD(IMX6_TEMPSENSE0, 1, 1),
	[FINISHED] = REG_FIELD(IMX6_TEMPSENSE0, 2, 2),
	[MEASURE_FREQ] = REG_FIELD(IMX6_TEMPSENSE1, 0, 15),
	[HIGH_ALARM] = REG_FIELD(IMX6_TEMPSENSE0, 20, 31),
	[PANIC_ALARM] = REG_FIELD(IMX6_TEMPSENSE2, 16, 27),
};

static struct reg_field imx7d_regfields[MAX_REGFIELDS] = {
	[TEMP_VALUE] = REG_FIELD(IMX7_TEMPSENSE1, 0, 8),
	[POWER_DOWN] = REG_FIELD(IMX7_TEMPSENSE1, 9, 9),
	[MEASURE_START] = REG_FIELD(IMX7_TEMPSENSE1, 10, 10),
	[FINISHED] = REG_FIELD(IMX7_TEMPSENSE1, 11, 11),
	[MEASURE_FREQ] = REG_FIELD(IMX7_TEMPSENSE1, 16, 31),
	[HIGH_ALARM] = REG_FIELD(IMX7_TEMPSENSE0, 9, 17),
	[PANIC_ALARM] = REG_FIELD(IMX7_TEMPSENSE0, 18, 26),
};

static struct thermal_soc_data thermal_imx6q_data = {
	.version = TEMPMON_V1,
	.reg_fields = imx6q_regfields,
};

static struct thermal_soc_data thermal_imx6sx_data = {
	.version = TEMPMON_V2,
	.reg_fields = imx6sx_regfields,
};

static struct thermal_soc_data thermal_imx7d_data = {
	.version = TEMPMON_V3,
	.reg_fields = imx7d_regfields,
};

static struct imx_thermal_data *imx_thermal_data;

static int imx_thermal_regfield_alloc(struct platform_device *pdev)
{
	struct imx_thermal_data *data = platform_get_drvdata(pdev);
	struct regmap *regmap = data->tempmon;
	const struct reg_field *reg_fields = data->socdata->reg_fields;

	data->power_down = devm_regmap_field_alloc(&pdev->dev, regmap,
						 reg_fields[POWERDOWN]);
	data->measure = devm_regmap_field_alloc(&pdev->dev, regmap,
					     reg_fields[MEASURE_START]);
	data->finished = devm_regmap_field_alloc(&pdev->dev, regmap,
						  reg_fields[FINISHED]);
	data->temp_value = devm_regmap_field_alloc(&pdev->dev, regmap,
						reg_fields[TEMP_VALUE]);
	data->measure_freq = devm_regmap_field_alloc(&pdev->dev, regmap,
					      reg_fields[MEASURE_FREQ]);
	data->high_alarm = devm_regmap_field_alloc(&pdev->dev, regmap,
						reg_fields[HIGH_ALARM]);

	/* for imx6q tempmon, no panic_alarm available */
	if (data->socdata->version != TEMPMON_V1)
		data->panic_alarm = devm_regmap_field_alloc(&pdev->dev, regmap,
						   reg_fields[PANIC_ALARM]);
	return 0;
}

static void imx_set_panic_temp(struct imx_thermal_data *data,
			       signed long panic_temp)
{
	int critical_value;

	if (data->socdata->version == TEMPMON_V3)
		critical_value = panic_temp / 1000 + data->c1 - 25;
	else
		critical_value = (data->c2 - panic_temp) / data->c1;

	regmap_field_write(data->panic_alarm, critical_value);
}

static void imx_set_alarm_temp(struct imx_thermal_data *data,
			       signed long alarm_temp)
{
	int alarm_value;

	data->alarm_temp = alarm_temp;

	if (data->socdata->version == TEMPMON_V3)
		alarm_value = alarm_temp / 1000 + data->c1 - 25;
	else
		alarm_value = (data->c2 - alarm_temp) / data->c1;

	regmap_field_write(data->high_alarm, alarm_value);
}

static int imx_get_temp(struct thermal_zone_device *tz, unsigned long *temp)
{
	struct imx_thermal_data *data = tz->devdata;
	unsigned int finished;
	unsigned int val;

	mutex_lock(&data->mutex);
	if (data->mode == THERMAL_DEVICE_ENABLED) {
		/* Check if a measurement is currently in progress */
		regmap_field_read(data->finished, &finished);
		regmap_field_read(data->temp_value, &val);
	} else {
		/*
		 * Every time we measure the temperature, we will power on the
		 * temperature sensor, enable measurements, take a reading,
		 * disable measurements, power off the temperature sensor.
		 */
		clk_prepare_enable(data->thermal_clk);
		regmap_field_write(data->power_down, POWERON);
		regmap_field_write(data->measure, START);

		finished = 0;
	}

	/*
	 * According to the temp sensor designers, it may require up to ~17us
	 * to complete a measurement.
	 */
	if (!finished) {

		/* On i.MX7, according to the design team, the finished bit can
		 * only keep 1us after the measured data available. It is hard
		 * for software to polling this bit. So wait for 20ms to make
		 * sure the measured data is valid.
		 */
		if (data->socdata->version == TEMPMON_V3)
			msleep(20);
		 else
			usleep_range(20, 50);

		regmap_field_read(data->finished, &finished);
		regmap_field_read(data->temp_value, &val);
	}

	if (data->mode != THERMAL_DEVICE_ENABLED) {
		regmap_field_write(data->measure, STOP);
		regmap_field_write(data->power_down, POWERDOWN);
		clk_disable_unprepare(data->thermal_clk);
	}

	/* The finished bit is not easy to poll on i.MX7, skip checking it */
	if (data->socdata->version != TEMPMON_V3 && !finished) {
		dev_dbg(&tz->device, "temp measurement never finished\n");
		mutex_unlock(&data->mutex);
		return -EAGAIN;
	}

	/* See imx_get_sensor_data() for formula derivation */
	if (data->socdata->version == TEMPMON_V3)
		*temp = (val - data->c1 + 25) * 1000;
	else
		*temp = data->c2 - val * data->c1;

	/* Update alarm value to next higher trip point */
	if (data->alarm_temp == data->temp_passive && *temp >= data->temp_passive)
		imx_set_alarm_temp(data, data->temp_critical);
	if (data->alarm_temp == data->temp_critical && *temp < data->temp_passive) {
		imx_set_alarm_temp(data, data->temp_passive);
		dev_dbg(&tz->device, "thermal alarm off: T < %lu\n",
			data->alarm_temp / 1000);
	}

	if (*temp != data->last_temp) {
		dev_dbg(&tz->device, "millicelsius: %ld\n", *temp);
		data->last_temp = *temp;
	}

	/* Reenable alarm IRQ if temperature below alarm temperature */
	if (!data->irq_enabled && *temp < data->alarm_temp) {
		data->irq_enabled = true;
		enable_irq(data->irq);
	}
	mutex_unlock(&data->mutex);

	return 0;
}

static int imx_get_mode(struct thermal_zone_device *tz,
			enum thermal_device_mode *mode)
{
	struct imx_thermal_data *data = tz->devdata;

	*mode = data->mode;

	return 0;
}

static int imx_set_mode(struct thermal_zone_device *tz,
			enum thermal_device_mode mode)
{
	struct imx_thermal_data *data = tz->devdata;

	if (mode == THERMAL_DEVICE_ENABLED) {
		tz->polling_delay = IMX_POLLING_DELAY;
		tz->passive_delay = IMX_PASSIVE_DELAY;

		regmap_field_write(data->power_down, POWERON);
		regmap_field_write(data->measure, START);

		if (!data->irq_enabled) {
			data->irq_enabled = true;
			enable_irq(data->irq);
		}
	} else {
		regmap_field_write(data->measure, STOP);
		regmap_field_write(data->power_down, POWERDOWN);

		tz->polling_delay = 0;
		tz->passive_delay = 0;

		if (data->irq_enabled) {
			disable_irq(data->irq);
			data->irq_enabled = false;
		}
	}

	data->mode = mode;
	thermal_zone_device_update(tz);

	return 0;
}

static int imx_get_trip_type(struct thermal_zone_device *tz, int trip,
			     enum thermal_trip_type *type)
{
	*type = (trip == IMX_TRIP_PASSIVE) ? THERMAL_TRIP_PASSIVE :
					     THERMAL_TRIP_CRITICAL;
	return 0;
}

static int imx_get_crit_temp(struct thermal_zone_device *tz,
			     unsigned long *temp)
{
	struct imx_thermal_data *data = tz->devdata;

	*temp = data->temp_critical;

	return 0;
}

static int imx_get_trip_temp(struct thermal_zone_device *tz, int trip,
			     unsigned long *temp)
{
	struct imx_thermal_data *data = tz->devdata;

	*temp = (trip == IMX_TRIP_PASSIVE) ? data->temp_passive :
					     data->temp_critical;
	return 0;
}

static int imx_set_trip_temp(struct thermal_zone_device *tz, int trip,
			     unsigned long temp)
{
	struct imx_thermal_data *data = tz->devdata;

	if (trip == IMX_TRIP_CRITICAL) {
		data->temp_critical = temp;
		if (data->socdata->version == TEMPMON_V2)
			imx_set_panic_temp(data, temp);
	}

	if (trip == IMX_TRIP_PASSIVE) {
		if (temp > IMX_TEMP_PASSIVE)
			return -EINVAL;
		data->temp_passive = temp;
		imx_set_alarm_temp(data, temp);
	}

	return 0;
}

static int imx_get_trend(struct thermal_zone_device *tz,
	int trip, enum thermal_trend *trend)
{
	int ret;
	unsigned long trip_temp;

	ret = imx_get_trip_temp(tz, trip, &trip_temp);
	if (ret < 0)
		return ret;

	if (tz->temperature >= (trip_temp - IMX_TEMP_PASSIVE_COOL_DELTA))
		*trend = THERMAL_TREND_RAISE_FULL;
	else
		*trend = THERMAL_TREND_DROP_FULL;

	return 0;
}

static int imx_bind(struct thermal_zone_device *tz,
		    struct thermal_cooling_device *cdev)
{
	int ret;

	ret = thermal_zone_bind_cooling_device(tz, IMX_TRIP_PASSIVE, cdev,
					       THERMAL_NO_LIMIT,
					       THERMAL_NO_LIMIT);
	if (ret) {
		dev_err(&tz->device,
			"binding zone %s with cdev %s failed:%d\n",
			tz->type, cdev->type, ret);
		return ret;
	}

	return 0;
}

static int imx_unbind(struct thermal_zone_device *tz,
		      struct thermal_cooling_device *cdev)
{
	int ret;

	ret = thermal_zone_unbind_cooling_device(tz, IMX_TRIP_PASSIVE, cdev);
	if (ret) {
		dev_err(&tz->device,
			"unbinding zone %s with cdev %s failed:%d\n",
			tz->type, cdev->type, ret);
		return ret;
	}

	return 0;
}

static struct thermal_zone_device_ops imx_tz_ops = {
	.bind = imx_bind,
	.unbind = imx_unbind,
	.get_temp = imx_get_temp,
	.get_mode = imx_get_mode,
	.set_mode = imx_set_mode,
	.get_trip_type = imx_get_trip_type,
	.get_trip_temp = imx_get_trip_temp,
	.get_crit_temp = imx_get_crit_temp,
	.set_trip_temp = imx_set_trip_temp,
	.get_trend = imx_get_trend,
};

static inline void imx6_calibrate_data(struct imx_thermal_data *data, u32 val)
{
	int t1, t2, n1, n2;
	u64 temp64;
	/*
	 * Sensor data layout:
	 *   [31:20] - sensor value @ 25C
	 *    [19:8] - sensor value of hot
	 *     [7:0] - hot temperature value
	 * Use universal formula now and only need sensor value @ 25C
	 * slope = 0.4297157 - (0.0015976 * 25C fuse)
	 */
	n1 = val >> 20;
	n2 = (val & 0xfff00) >> 8;
	t2 = val & 0xff;
	t1 = 25; /* t1 always 25C */

	/*
	 * Derived from linear interpolation:
	 * slope = 0.4297157 - (0.0015976 * 25C fuse)
	 * slope = (FACTOR2 - FACTOR1 * n1) / FACTOR0
	 * offset = OFFSET / 1000000
	 * (Nmeas - n1) / (Tmeas - t1) = slope
	 * We want to reduce this down to the minimum computation necessary
	 * for each temperature read.  Also, we want Tmeas in millicelsius
	 * and we don't want to lose precision from integer division. So...
	 * Tmeas = (Nmeas - n1) / slope + t1 + offset
	 * milli_Tmeas = 1000 * (Nmeas - n1) / slope + 1000 * t1 + OFFSET / 1000
	 * milli_Tmeas = -1000 * (n1 - Nmeas) / slope + 1000 * t1 + OFFSET /1000
	 * Let constant c1 = (-1000 / slope)
	 * milli_Tmeas = (n1 - Nmeas) * c1 + 1000 * t1 + OFFSET / 1000
	 * Let constant c2 = n1 *c1 + 1000 * t1 + OFFSET / 1000
	 * milli_Tmeas = c2 - Nmeas * c1
	 */
	temp64 = FACTOR0;
	temp64 *= 1000;
	do_div(temp64, FACTOR1 * n1 - FACTOR2);
	data->c1 = temp64;
	temp64 = OFFSET;
	do_div(temp64, 1000);
	data->c2 = n1 * data->c1 + 1000 * t1 + temp64;
}

/*
 * On i.MX7, we only use the calibration data at 25C to get the temp,
 * Tmeas = ( Nmeas - n1) + 25; n1 is the fuse value for 25C.
 */
static inline void imx7_calibrate_data(struct imx_thermal_data *data, u32 val)
{
	data->c1 = (val >> 9) & 0x1ff;
}

static int imx_get_sensor_data(struct platform_device *pdev)
{
	struct imx_thermal_data *data = platform_get_drvdata(pdev);
	struct regmap *map;
	int ret;
	u32 val;

	map = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
					      "fsl,tempmon-data");
	if (IS_ERR(map)) {
		ret = PTR_ERR(map);
		dev_err(&pdev->dev, "failed to get sensor regmap: %d\n", ret);
		return ret;
	}

	if (data->socdata->version == TEMPMON_V3)
		ret = regmap_read(map, IMX7_OCOTP_ANA1, &val);
	else
		ret = regmap_read(map, IMX6_OCOTP_ANA1, &val);

	if (ret) {
		dev_err(&pdev->dev, "failed to read sensor data: %d\n", ret);
		return ret;
	}

	if (val == 0 || val == ~0) {
		dev_err(&pdev->dev, "invalid sensor calibration data\n");
		return -EINVAL;
	}

	if (data->socdata->version == TEMPMON_V3)
		imx7_calibrate_data(data, val);
	else
		imx6_calibrate_data(data, val);

	/*
	 * Set the default passive cooling trip point to IMX_TEMP_PASSIVE.
	 * Can be changed from userspace.
	 */
	data->temp_passive = IMX_TEMP_PASSIVE;

	/*
	 * Set the default critical trip point to 20 C higher
	 * than passive trip point. Can be changed from userspace.
	 */
	data->temp_critical = IMX_TEMP_PASSIVE + 20 * 1000;

	return 0;
}

static irqreturn_t imx_thermal_alarm_irq(int irq, void *dev)
{
	struct imx_thermal_data *data = dev;

	disable_irq_nosync(irq);
	data->irq_enabled = false;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t imx_thermal_alarm_irq_thread(int irq, void *dev)
{
	struct imx_thermal_data *data = dev;

	dev_dbg(&data->tz->device, "THERMAL ALARM: T > %lu\n",
		data->alarm_temp / 1000);

	thermal_zone_device_update(data->tz);

	return IRQ_HANDLED;
}

static const struct of_device_id of_imx_thermal_match[] = {
	{ .compatible = "fsl,imx6q-tempmon", .data = &thermal_imx6q_data, },
	{ .compatible = "fsl,imx6sx-tempmon", .data = &thermal_imx6sx_data, },
	{ .compatible = "fsl,imx7d-tempmon", .data = &thermal_imx7d_data, },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, of_imx_thermal_match);

static int thermal_notifier_event(struct notifier_block *this,
					unsigned long event, void *ptr)
{
	mutex_lock(&imx_thermal_data->mutex);

	switch (event) {
	/*
	 * In low_bus_freq_mode, the thermal sensor auto measurement
	 * can be disabled to low the power consumption.
	 */
	case LOW_BUSFREQ_ENTER:
		regmap_field_write(imx_thermal_data->measure, STOP);
		regmap_field_write(imx_thermal_data->power_down, POWERDOWN);
		imx_thermal_data->mode = THERMAL_DEVICE_DISABLED;
		disable_irq(imx_thermal_data->irq);
		clk_disable_unprepare(imx_thermal_data->thermal_clk);
		break;

	/* Enabled thermal auto measurement when exiting low_bus_freq_mode */
	case LOW_BUSFREQ_EXIT:
		clk_prepare_enable(imx_thermal_data->thermal_clk);
		regmap_field_write(imx_thermal_data->power_down, POWERON);
		regmap_field_write(imx_thermal_data->measure, START);
		imx_thermal_data->mode = THERMAL_DEVICE_ENABLED;
		enable_irq(imx_thermal_data->irq);
		break;

	default:
		break;
	}
	mutex_unlock(&imx_thermal_data->mutex);

	return NOTIFY_OK;
}

static struct notifier_block thermal_notifier = {
	.notifier_call = thermal_notifier_event,
};

static int imx_thermal_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
		of_match_device(of_imx_thermal_match, &pdev->dev);
	struct imx_thermal_data *data;
	struct cpumask clip_cpus;
	struct regmap *map;
	int measure_freq;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	imx_thermal_data = data;

	map = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, "fsl,tempmon");
	if (IS_ERR(map)) {
		ret = PTR_ERR(map);
		dev_err(&pdev->dev, "failed to get tempmon regmap: %d\n", ret);
		return ret;
	}
	data->tempmon = map;

	data->socdata = of_id->data;

	data->thermal_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(data->thermal_clk)) {
		dev_warn(&pdev->dev, "failed to get thermal clk!\n");
	} else {
		/*
		 * Thermal sensor needs clk on to get correct value, normally
		 * we should enable its clk before taking measurement and disable
		 * clk after measurement is done, but if alarm function is enabled,
		 * hardware will auto measure the temperature periodically, so we
		 * need to keep the clk always on for alarm function.
		 */
		ret = clk_prepare_enable(data->thermal_clk);
		if (ret)
			dev_warn(&pdev->dev, "failed to enable thermal clk: %d\n", ret);
	}

	mutex_init(&data->mutex);
	/* make sure the IRQ flag is clear before enable irq */
	if (data->socdata->version != TEMPMON_V3)
		regmap_write(map, MISC1 + REG_CLR, MISC1_IRQ_TEMPHIGH);
	if (data->socdata->version == TEMPMON_V2) {
		/*
		 * reset value of LOW ALARM is incorrect, set it to lowest
		 * value to avoid false trigger of low alarm.
		 */
		regmap_write(map, TEMPSENSE2 + REG_SET,
			TEMPSENSE2_LOW_VALUE_MASK);
		regmap_write(map, MISC1 + REG_CLR, MISC1_IRQ_TEMPLOW |
			MISC1_IRQ_TEMPPANIC);
	}

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0) {
		ret = data->irq;
		goto out;
	}

	platform_set_drvdata(pdev, data);

	/* Allocate the regmap_field for the imx_thermal */
	imx_thermal_regfield_alloc(pdev);

	ret = imx_get_sensor_data(pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to get sensor data\n");
		goto out;
	}

	/* Make sure sensor is in known good state for measurements */
	regmap_field_write(data->power_down, POWERON);
	regmap_field_write(data->measure, STOP);
	regmap_field_write(data->measure_freq, 0);
	if (data->socdata->version != TEMPMON_V3)
		regmap_write(map, MISC0 + REG_SET, MISC0_REFTOP_SELBIASOFF);
	regmap_field_write(data->power_down, POWERDOWN);

	cpumask_set_cpu(0, &clip_cpus);
	data->cdev[0] = cpufreq_cooling_register(&clip_cpus);
	if (IS_ERR(data->cdev[0])) {
		ret = PTR_ERR(data->cdev[0]);
		dev_err(&pdev->dev,
			"failed to register cpufreq cooling device: %d\n", ret);
		goto out;
	}

	data->cdev[1] = devfreq_cooling_register();
	if (IS_ERR(data->cdev[1])) {
		ret = PTR_ERR(data->cdev[1]);
		dev_err(&pdev->dev,
			"failed to register devfreq cooling device: %d\n", ret);
		goto out;
	}

	data->tz = thermal_zone_device_register("imx_thermal_zone",
						IMX_TRIP_NUM,
						(1 << IMX_TRIP_NUM) - 1, data,
						&imx_tz_ops, NULL,
						IMX_PASSIVE_DELAY,
						IMX_POLLING_DELAY);
	if (IS_ERR(data->tz)) {
		ret = PTR_ERR(data->tz);
		dev_err(&pdev->dev,
			"failed to register thermal zone device %d\n", ret);
		cpufreq_cooling_unregister(data->cdev[0]);
		devfreq_cooling_unregister(data->cdev[1]);
		goto out;
	}

	/* Enable measurements at ~ 10 Hz */
	measure_freq = DIV_ROUND_UP(32768, 10); /* 10 Hz */
	regmap_field_write(data->measure_freq, measure_freq);
	imx_set_alarm_temp(data, data->temp_passive);

	if (data->socdata->version == TEMPMON_V2)
		imx_set_panic_temp(data, data->temp_critical);

	regmap_field_write(data->power_down, POWERON);
	regmap_field_write(data->measure, START);

	ret = devm_request_threaded_irq(&pdev->dev, data->irq,
			imx_thermal_alarm_irq, imx_thermal_alarm_irq_thread,
			0, "imx_thermal", data);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request alarm irq: %d\n", ret);
		goto out;
	}
	data->irq_enabled = true;

	data->mode = THERMAL_DEVICE_ENABLED;

	/* register the busfreq notifier called in low bus freq */
	if (data->socdata->version != TEMPMON_V3)
		register_busfreq_notifier(&thermal_notifier);

	return 0;
out:
	clk_disable_unprepare(data->thermal_clk);
	return ret;
}

static int imx_thermal_remove(struct platform_device *pdev)
{
	struct imx_thermal_data *data = platform_get_drvdata(pdev);

	if (data->socdata->version != TEMPMON_V3)
		unregister_busfreq_notifier(&thermal_notifier);

	/* Disable measurements */
	regmap_field_write(data->power_down, POWERDOWN);
	if (!IS_ERR(data->thermal_clk))
		clk_disable_unprepare(data->thermal_clk);

	thermal_zone_device_unregister(data->tz);
	cpufreq_cooling_unregister(data->cdev[0]);
	devfreq_cooling_unregister(data->cdev[1]);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int imx_thermal_suspend(struct device *dev)
{
	struct imx_thermal_data *data = dev_get_drvdata(dev);

	/*
	 * Need to disable thermal sensor, otherwise, when thermal core
	 * try to get temperature before thermal sensor resume, a wrong
	 * temperature will be read as the thermal sensor is powered
	 * down.
	 */
	regmap_field_write(data->measure, STOP);
	regmap_field_write(data->power_down, POWERDOWN);

	data->mode = THERMAL_DEVICE_DISABLED;
	disable_irq(data->irq);
	clk_disable_unprepare(data->thermal_clk);

	return 0;
}

static int imx_thermal_resume(struct device *dev)
{
	struct imx_thermal_data *data = dev_get_drvdata(dev);

	/* Enabled thermal sensor after resume */
	clk_prepare_enable(data->thermal_clk);
	regmap_field_write(data->power_down, POWERON);
	regmap_field_write(data->measure, START);
	data->mode = THERMAL_DEVICE_ENABLED;
	enable_irq(data->irq);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(imx_thermal_pm_ops,
			 imx_thermal_suspend, imx_thermal_resume);

static struct platform_driver imx_thermal = {
	.driver = {
		.name	= "imx_thermal",
		.owner  = THIS_MODULE,
		.pm	= &imx_thermal_pm_ops,
		.of_match_table = of_imx_thermal_match,
	},
	.probe		= imx_thermal_probe,
	.remove		= imx_thermal_remove,
};

static int __init imx_thermal_init(void)
{
	return platform_driver_register(&imx_thermal);
}

late_initcall(imx_thermal_init);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Thermal driver for Freescale i.MX SoCs");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-thermal");
