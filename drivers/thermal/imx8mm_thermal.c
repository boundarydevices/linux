// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 NXP.
 *
 * Author: Anson Huang <Anson.Huang@nxp.com>
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device_cooling.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include "thermal_hwmon.h"

#include "thermal_core.h"

#define TER			0x0	/* TMU enable */
#define TPS			0x4
#define TRITSR			0x20	/* TMU immediate temp */

#define TER_ADC_PD		BIT(30)
#define TER_EN			BIT(31)
#define TRITSR_TEMP0_VAL_MASK	0xff
#define TRITSR_TEMP1_VAL_MASK	0xff0000

#define PROBE_SEL_ALL		GENMASK(31, 30)

#define probe_status_offset(x)	(30 + x)
#define SIGN_BIT		BIT(7)
#define TEMP_VAL_MASK		GENMASK(6, 0)

#define VER1_TEMP_LOW_LIMIT	10000
#define VER2_TEMP_LOW_LIMIT	-40000
#define VER2_TEMP_HIGH_LIMIT	125000
#define IMX_TEMP_PASSIVE_COOL_DELTA 10000

#define TMU_VER1		0x1
#define TMU_VER2		0x2

#define MAX_SENSOR_NUMBER	2

struct thermal_soc_data {
	u32 num_sensors;
	u32 version;
	int (*get_temp)(void *, int *);
};

struct tmu_sensor {
	struct imx8mm_tmu *priv;
	u32 hw_id;
	struct thermal_zone_device *tzd;
	struct thermal_cooling_device *cdev;
	int temp_passive;
	int temp_critical;
};

struct imx8mm_tmu {
	void __iomem *base;
	struct clk *clk;
	const struct thermal_soc_data *socdata;
	struct tmu_sensor sensors[];
};

/* The driver support 1 passive trip point and 1 critical trip point */
enum imx_thermal_trip {
	IMX_TRIP_PASSIVE,
	IMX_TRIP_CRITICAL,
	IMX_TRIP_NUM,
};

static int imx8mm_tmu_get_temp(void *data, int *temp)
{
	struct tmu_sensor *sensor = data;
	struct imx8mm_tmu *tmu = sensor->priv;
	u32 val;

	val = readl_relaxed(tmu->base + TRITSR) & TRITSR_TEMP0_VAL_MASK;
	*temp = val * 1000;
	if (*temp < VER1_TEMP_LOW_LIMIT)
		return -EAGAIN;

	return 0;
}

static int imx8mp_tmu_get_temp(void *data, int *temp)
{
	struct tmu_sensor *sensor = data;
	struct imx8mm_tmu *tmu = sensor->priv;
	unsigned long val;
	bool ready;
	int loop_cnt = 0;

	if (sensor->hw_id > (MAX_SENSOR_NUMBER - 1))
		return -EINVAL;

	while (1) {
		/* the temp sensor need about 1ms to finish the measurement */
		if (loop_cnt)
			usleep_range(1000, 2000);
		/* read the calibrated temp value */
		val = readl_relaxed(tmu->base + TRITSR);
		ready = test_bit(probe_status_offset(sensor->hw_id), &val);
		if (ready)
			break;
		if (loop_cnt > 1)
			return -EAGAIN;
		loop_cnt++;
	}

	val = sensor->hw_id ? FIELD_GET(TRITSR_TEMP1_VAL_MASK, val) :
	      FIELD_GET(TRITSR_TEMP0_VAL_MASK, val);
	if (val & SIGN_BIT) /* negative */
		val = (~(val & TEMP_VAL_MASK) + 1);

	*temp = val * 1000;
	if (*temp < VER2_TEMP_LOW_LIMIT || *temp > VER2_TEMP_HIGH_LIMIT)
		return -EAGAIN;

	return 0;
}

static int tmu_get_temp(struct thermal_zone_device *tz, int *temp)
{
	struct tmu_sensor *sensor = tz->devdata;
	struct imx8mm_tmu *tmu = sensor->priv;

	return tmu->socdata->get_temp(sensor, temp);
}

static int tmu_get_trend(struct thermal_zone_device *tz, int trip,
			 enum thermal_trend *trend)
{
	struct tmu_sensor *sensor = tz->devdata;
	int trip_temp;

	if (!sensor->tzd)
		return 0;

	trip_temp = (trip == IMX_TRIP_PASSIVE) ? sensor->temp_passive : sensor->temp_critical;

	if (sensor->tzd->temperature >= (trip_temp - IMX_TEMP_PASSIVE_COOL_DELTA))
		*trend = THERMAL_TREND_RAISING;
	else
		*trend = THERMAL_TREND_DROPPING;

	return 0;
}

static int tmu_set_trip_temp(struct thermal_zone_device *tz, int trip, int temp)

{
	struct tmu_sensor *sensor = tz->devdata;

	if (trip == IMX_TRIP_CRITICAL)
		sensor->temp_critical = temp;

	if (trip == IMX_TRIP_PASSIVE)
		sensor->temp_passive = temp;

	return 0;
}

static const struct thermal_zone_device_ops tmu_tz_ops = {
	.get_temp = tmu_get_temp,
	.get_trend = tmu_get_trend,
	.set_trip_temp = tmu_set_trip_temp,
};

static void imx8mm_tmu_enable(struct imx8mm_tmu *tmu, bool enable)
{
	u32 val;

	val = readl_relaxed(tmu->base + TER);
	val = enable ? (val | TER_EN) : (val & ~TER_EN);
	if (tmu->socdata->version == TMU_VER2)
		val = enable ? (val & ~TER_ADC_PD) : (val | TER_ADC_PD);
	writel_relaxed(val, tmu->base + TER);
}

static void imx8mm_tmu_probe_sel_all(struct imx8mm_tmu *tmu)
{
	u32 val;

	val = readl_relaxed(tmu->base + TPS);
	val |= PROBE_SEL_ALL;
	writel_relaxed(val, tmu->base + TPS);
}

static int imx8mm_tmu_probe(struct platform_device *pdev)
{
	const struct thermal_soc_data *data;
	const struct thermal_trip *trips;
	struct imx8mm_tmu *tmu;
	int ret;
	int i;

	data = of_device_get_match_data(&pdev->dev);

	tmu = devm_kzalloc(&pdev->dev, struct_size(tmu, sensors,
			   data->num_sensors), GFP_KERNEL);
	if (!tmu)
		return -ENOMEM;

	tmu->socdata = data;

	tmu->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(tmu->base))
		return PTR_ERR(tmu->base);

	tmu->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(tmu->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(tmu->clk),
				     "failed to get tmu clock\n");

	ret = clk_prepare_enable(tmu->clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable tmu clock: %d\n", ret);
		return ret;
	}

	/* disable the monitor during initialization */
	imx8mm_tmu_enable(tmu, false);

	for (i = 0; i < data->num_sensors; i++) {
		tmu->sensors[i].priv = tmu;
		tmu->sensors[i].tzd =
			devm_thermal_of_zone_register(&pdev->dev, i,
						      &tmu->sensors[i],
						      &tmu_tz_ops);
		if (IS_ERR(tmu->sensors[i].tzd)) {
			ret = PTR_ERR(tmu->sensors[i].tzd);
			dev_err(&pdev->dev,
				"failed to register thermal zone sensor[%d]: %d\n",
				i, ret);
			goto disable_clk;
		}
		tmu->sensors[i].hw_id = i;

		tmu->sensors[i].cdev = devfreq_cooling_register();
		if (IS_ERR(tmu->sensors[i].cdev)) {
			ret = PTR_ERR(tmu->sensors[i].cdev);
			if (ret != -EPROBE_DEFER)
				dev_err(&pdev->dev,
					"failed to register devfreq cooling device %d\n", ret);
			return ret;
		}

		ret = thermal_zone_bind_cooling_device(tmu->sensors[i].tzd,
			IMX_TRIP_PASSIVE,
			tmu->sensors[i].cdev,
			THERMAL_NO_LIMIT,
			THERMAL_NO_LIMIT,
			THERMAL_WEIGHT_DEFAULT);
		if (ret) {
			dev_err(&pdev->dev,
				"binding zone %s with cdev %s failed:%d\n",
				tmu->sensors[i].tzd->type, tmu->sensors[i].cdev->type, ret);
			devfreq_cooling_unregister(tmu->sensors[i].cdev);
			return ret;
		}

		trips = of_thermal_get_trip_points(tmu->sensors[i].tzd);

		/* get the thermal trip temp */
		tmu->sensors[i].temp_passive = trips[0].temperature;
		tmu->sensors[i].temp_critical = trips[1].temperature;
	}

	platform_set_drvdata(pdev, tmu);

	/* enable all the probes for V2 TMU */
	if (tmu->socdata->version == TMU_VER2)
		imx8mm_tmu_probe_sel_all(tmu);

	/* enable the monitor */
	imx8mm_tmu_enable(tmu, true);

	for (i = 0; i < data->num_sensors; i++) {
		tmu->sensors[0].tzd->tzp->no_hwmon = false;
		ret = thermal_add_hwmon_sysfs(tmu->sensors[0].tzd);
		if (ret < 0)
			break;
	}
	return ret;

disable_clk:
	clk_disable_unprepare(tmu->clk);
	return ret;
}

static int imx8mm_tmu_remove(struct platform_device *pdev)
{
	struct imx8mm_tmu *tmu = platform_get_drvdata(pdev);

	/* disable TMU */
	imx8mm_tmu_enable(tmu, false);

	clk_disable_unprepare(tmu->clk);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct thermal_soc_data imx8mm_tmu_data = {
	.num_sensors = 1,
	.version = TMU_VER1,
	.get_temp = imx8mm_tmu_get_temp,
};

static struct thermal_soc_data imx8mp_tmu_data = {
	.num_sensors = 2,
	.version = TMU_VER2,
	.get_temp = imx8mp_tmu_get_temp,
};

static const struct of_device_id imx8mm_tmu_table[] = {
	{ .compatible = "fsl,imx8mm-tmu", .data = &imx8mm_tmu_data, },
	{ .compatible = "fsl,imx8mp-tmu", .data = &imx8mp_tmu_data, },
	{ },
};
MODULE_DEVICE_TABLE(of, imx8mm_tmu_table);

static struct platform_driver imx8mm_tmu = {
	.driver = {
		.name	= "i.mx8mm_thermal",
		.of_match_table = imx8mm_tmu_table,
	},
	.probe = imx8mm_tmu_probe,
	.remove = imx8mm_tmu_remove,
};
module_platform_driver(imx8mm_tmu);

MODULE_AUTHOR("Anson Huang <Anson.Huang@nxp.com>");
MODULE_DESCRIPTION("i.MX8MM Thermal Monitor Unit driver");
MODULE_LICENSE("GPL v2");
