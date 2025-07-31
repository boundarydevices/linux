// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/scmi_protocol.h>

#include "tinysys-scmi.h"

static struct scmi_tinysys_info_st *t_info;
static const struct scmi_tinysys_proto_ops *tinysys_ops;

static f_handler_t cb_array[SCMI_TINYSYS_CB_MAX];

struct scmi_tinysys_info_st *get_scmi_tinysys_info(void)
{
	return t_info;
}
EXPORT_SYMBOL(get_scmi_tinysys_info);

int scmi_tinysys_common_set(const struct scmi_protocol_handle *ph, u32 feature_id,
	u32 p1, u32 p2, u32 p3, u32 p4, u32 p5)
{
	int ret;

	ret = tinysys_ops->common_set(ph, feature_id, p1, p2, p3, p4, p5);

	if (ret)
		dev_info(&(t_info->sdev->dev), "[scmi][tinysys_set] fid:%u ret:%d p1:%u p2:%u p3:%u p4:%u p5:%u\n",
			feature_id, ret, p1, p2, p3, p4, p5);

	return ret;
}
EXPORT_SYMBOL(scmi_tinysys_common_set);

int scmi_tinysys_common_get(const struct scmi_protocol_handle *ph, u32 feature_id,
	u32 p1, struct scmi_tinysys_status *rvalue)
{
	int ret;

	ret = tinysys_ops->common_get(ph, feature_id, p1, rvalue);

	if (ret)
		dev_info(&(t_info->sdev->dev), "[scmi][tinysys_get] fid:%u ret:%d p1:%u r1:%u r2:%u r3:%u\n",
			feature_id, ret, p1, rvalue->r1, rvalue->r2, rvalue->r3);

	return ret;
}
EXPORT_SYMBOL(scmi_tinysys_common_get);

int scmi_tinysys_slbc_ctrl(const struct scmi_protocol_handle *ph,
	u32 cmd, u32 slbc_resv1, u32 slbc_resv2, u32 slbc_resv3, u32 slbc_resv4,
	struct scmi_tinysys_slbc_ctrl_status *rvalue)
{
	int ret;

	ret = tinysys_ops->slbc_ctrl(
		ph, cmd, slbc_resv1, slbc_resv2, slbc_resv3, slbc_resv4, rvalue);

	if (ret) {
		dev_info(&(t_info->sdev->dev), "[scmi][tinysys_slbc_ctrl dump] ret:%d cmd:%u\n",
				ret, cmd);

		dev_info(&(t_info->sdev->dev), "slbc_in: resv1:%u resv2:%u resv3:%u resv4:%u\n",
				slbc_resv1, slbc_resv2, slbc_resv3, slbc_resv4);

		dev_info(&(t_info->sdev->dev), "slbc_out: resv1:%u, resv2:%u, resv3:%u, resv4:%u\n",
				rvalue->slbc_resv1, rvalue->slbc_resv2,
				rvalue->slbc_resv3, rvalue->slbc_resv4);
	}

	return ret;
}
EXPORT_SYMBOL(scmi_tinysys_slbc_ctrl);

static int scmi_tinysys_notifier_fn(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct scmi_tinysys_notifier_report *r = data;
	struct scmi_tinysys_report_st *report = (struct scmi_tinysys_report_st *)&(r->f_id);
	f_handler_t func;

	dev_dbg(&(t_info->sdev->dev), "scmi notify report ktime:%lld f_id:%d p1:%d %d %d %d\n",
		r->timestamp, r->f_id, r->p1_status, r->p2_status, r->p3_status,
		r->p4_status);

	if (r->f_id < SCMI_TINYSYS_CB_MAX) {

		func = cb_array[r->f_id];
		if (func)
			func(r->f_id, report);
	}
	return NOTIFY_OK;
}

static struct notifier_block tinysys_nb = {
	.notifier_call = scmi_tinysys_notifier_fn,
};

int scmi_tinysys_event_notify(u32 feature_id, u32 notify_enable)
{

	int ret = 0;
	int f_id = feature_id;
	struct scmi_device *sdev = t_info->sdev;

	if (notify_enable) {
		ret = sdev->handle->notify_ops->devm_event_notifier_register(sdev,
			 SCMI_PROTOCOL_TINYSYS, SCMI_EVENT_TINYSYS_NOTIFIER, &f_id, &tinysys_nb);
		if (ret)
			dev_info(&(t_info->sdev->dev),
				"scmi register_event_notifier f_id:%d ret:%d\n", f_id, ret);

	} else {
		ret = sdev->handle->notify_ops->devm_event_notifier_unregister(sdev,
			SCMI_PROTOCOL_TINYSYS, SCMI_EVENT_TINYSYS_NOTIFIER, &f_id, &tinysys_nb);
		if (ret)
			dev_info(&(t_info->sdev->dev),
				"scmi unregister_event_notifier f_id:%d ret:%d\n", f_id, ret);
	}

	return ret;
}
EXPORT_SYMBOL(scmi_tinysys_event_notify);

void scmi_tinysys_register_event_notifier(u32 feature_id, f_handler_t hand)
{
	if (feature_id < SCMI_TINYSYS_CB_MAX)
		cb_array[feature_id] = hand;
	else
		dev_info(&(t_info->sdev->dev),
			"feature_id %d >= SCMI_TINYSYS_CB_MAX\n", feature_id);
}
EXPORT_SYMBOL(scmi_tinysys_register_event_notifier);

static int scmi_tinysys_probe(struct scmi_device *sdev)
{
	struct device *dev = &sdev->dev;

	const struct scmi_handle *handle = sdev->handle;
	struct scmi_protocol_handle *ph;

	if (!handle)
		return -ENODEV;

	scmi_tinysys_register();

	tinysys_ops = handle->devm_protocol_get(sdev, SCMI_PROTOCOL_TINYSYS, &ph);
	if (IS_ERR(tinysys_ops))
		return PTR_ERR(tinysys_ops);

	t_info = devm_kzalloc(dev, sizeof(*t_info), GFP_KERNEL);
	if (!t_info)
		return -ENOMEM;

	t_info->sdev = sdev;

	t_info->ph = ph;

	return 0;
}

static const struct scmi_device_id scmi_id_table[] = {
	{ SCMI_PROTOCOL_TINYSYS, "tinysys" },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static struct scmi_driver scmi_tinysys_driver = {
	.name = "scmi-tinysys",
	.probe = scmi_tinysys_probe,
	.id_table = scmi_id_table,
};
module_scmi_driver(scmi_tinysys_driver);

MODULE_DESCRIPTION("SCMI tinysys driver");
MODULE_LICENSE("GPL");

