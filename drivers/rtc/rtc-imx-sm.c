// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023 NXP.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/scmi_protocol.h>
#include <linux/scmi_nxp_protocol.h>
#include <linux/suspend.h>

static struct scmi_imx_bbm_rtc {
	struct rtc_device *rtc_dev;
	struct scmi_protocol_handle *ph;
} *scmi_imx_bbm_rtc;

static const struct scmi_imx_bbm_proto_ops *imx_bbm_ops;
struct notifier_block scmi_imx_bbm_rtc_nb;

static int scmi_imx_bbm_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct scmi_protocol_handle *ph = scmi_imx_bbm_rtc->ph;
	u64 val;
	int ret;

	ret = imx_bbm_ops->rtc_time_get(ph, 0, &val);
	if (ret)
		dev_err(dev, "%s: %d\n", __func__, ret);

	rtc_time64_to_tm(val, tm);

	return 0;
}

static int scmi_imx_bbm_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct scmi_protocol_handle *ph = scmi_imx_bbm_rtc->ph;
	u64 val;
	int ret;

	val = rtc_tm_to_time64(tm);

	ret = imx_bbm_ops->rtc_time_set(ph, 0, val);
	if (ret)
		dev_err(dev, "%s: %d\n", __func__, ret);

	return 0;
}

static int scmi_imx_bbm_alarm_irq_enable(struct device *dev, unsigned int enable)
{
	return 0;
}

static int scmi_imx_bbm_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct scmi_protocol_handle *ph = scmi_imx_bbm_rtc->ph;
	struct rtc_time *alrm_tm = &alrm->time;
	u64 val;
	int ret;

	val = rtc_tm_to_time64(alrm_tm);

	ret = imx_bbm_ops->rtc_alarm_set(ph, 0, val);
	if (ret)
		dev_err(dev, "%s: %d\n", __func__, ret);

	return 0;
}

static const struct rtc_class_ops smci_imx_bbm_rtc_ops = {
	.read_time = scmi_imx_bbm_rtc_read_time,
	.set_time = scmi_imx_bbm_rtc_set_time,
	.set_alarm = scmi_imx_bbm_set_alarm,
	.alarm_irq_enable = scmi_imx_bbm_alarm_irq_enable,
};

static int scmi_imx_bbm_rtc_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	rtc_update_irq(scmi_imx_bbm_rtc->rtc_dev, 1, RTC_AF | RTC_IRQF);

	return 0;
}

static int scmi_imx_bbm_rtc_probe(struct scmi_device *sdev)
{
	const struct scmi_handle *handle = sdev->handle;
	struct device *dev = &sdev->dev;
	struct scmi_protocol_handle *ph;
	int ret;

	if (!handle)
		return -ENODEV;

	scmi_imx_bbm_rtc = devm_kzalloc(dev, sizeof(struct scmi_imx_bbm_rtc), GFP_KERNEL);
	if (!scmi_imx_bbm_rtc)
		return -ENOMEM;

	imx_bbm_ops = handle->devm_protocol_get(sdev, SCMI_PROTOCOL_IMX_BBM, &ph);
	if (IS_ERR(imx_bbm_ops))
		return PTR_ERR(imx_bbm_ops);

	device_init_wakeup(dev, true);

	scmi_imx_bbm_rtc->rtc_dev = devm_rtc_allocate_device(dev);
	if (IS_ERR(scmi_imx_bbm_rtc->rtc_dev))
		return PTR_ERR(scmi_imx_bbm_rtc->rtc_dev);

	scmi_imx_bbm_rtc->ph = ph;
	scmi_imx_bbm_rtc->rtc_dev->ops = &smci_imx_bbm_rtc_ops;
	scmi_imx_bbm_rtc->rtc_dev->range_min = 0;
	scmi_imx_bbm_rtc->rtc_dev->range_max = U32_MAX;

	ret = devm_rtc_register_device(scmi_imx_bbm_rtc->rtc_dev);
	if (ret)
		return ret;

	/* TODO pm notifier */

	scmi_imx_bbm_rtc_nb.notifier_call = &scmi_imx_bbm_rtc_notifier;
	return handle->notify_ops->devm_event_notifier_register(sdev, SCMI_PROTOCOL_IMX_BBM, 0,
								NULL, &scmi_imx_bbm_rtc_nb);
}

static const struct scmi_device_id scmi_id_table[] = {
	{ SCMI_PROTOCOL_IMX_BBM, "imx-bbm" },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static struct scmi_driver scmi_imx_bbm_rtc_driver = {
	.name = "scmi-imx-bbm-rtc",
	.probe = scmi_imx_bbm_rtc_probe,
	.id_table = scmi_id_table,
};
module_scmi_driver(scmi_imx_bbm_rtc_driver);

MODULE_AUTHOR("Peng Fan <peng.fan@nxp.com>");
MODULE_DESCRIPTION("IMX SM BBM RTC driver");
MODULE_LICENSE("GPL");
