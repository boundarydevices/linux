// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023 NXP.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/scmi_protocol.h>
#include <linux/scmi_nxp_protocol.h>
#include <linux/suspend.h>

static const struct scmi_imx_misc_proto_ops *imx_misc_ctrl_ops;
static struct scmi_protocol_handle *ph;
struct notifier_block scmi_imx_misc_ctrl_nb;

int scmi_imx_misc_ctrl_set(u32 id, u32 val)
{
	if (!ph)
		return -EPROBE_DEFER;

	return imx_misc_ctrl_ops->misc_ctrl_set(ph, id, 1, &val);
};
EXPORT_SYMBOL(scmi_imx_misc_ctrl_set);

int scmi_imx_misc_ctrl_get(u32 id, u32 *num, u32 *val)
{
	if (!ph)
		return -EPROBE_DEFER;

	return imx_misc_ctrl_ops->misc_ctrl_get(ph, id, num, val);
}
EXPORT_SYMBOL(scmi_imx_misc_ctrl_get);

static int scmi_imx_misc_ctrl_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	/* Placeholder */
	return 0;
}

static int scmi_imx_misc_ctrl_probe(struct scmi_device *sdev)
{
	const struct scmi_handle *handle = sdev->handle;
	struct device_node *np = sdev->dev.of_node;
	u32 src_id, flags;
	int ret, i, wu_num;

	if (!handle)
		return -ENODEV;

	imx_misc_ctrl_ops = handle->devm_protocol_get(sdev, SCMI_PROTOCOL_IMX_MISC, &ph);
	if (IS_ERR(imx_misc_ctrl_ops))
		return PTR_ERR(imx_misc_ctrl_ops);

	scmi_imx_misc_ctrl_nb.notifier_call = &scmi_imx_misc_ctrl_notifier;
	wu_num = of_property_count_u32_elems(np, "wakeup-sources");
	if (wu_num % 2) {
		dev_err(&sdev->dev, "Invalid wakeup-sources\n");
		return -EINVAL;
	}

	for (i = 0; i < wu_num; i += 2) {
		WARN_ON(of_property_read_u32_index(np, "wakeup-sources", i, &src_id));
		WARN_ON(of_property_read_u32_index(np, "wakeup-sources", i + 1, &flags));

		ret = handle->notify_ops->devm_event_notifier_register(sdev, SCMI_PROTOCOL_IMX_MISC,
								       SCMI_EVENT_IMX_MISC_CONTROL,
								       &src_id,
								       &scmi_imx_misc_ctrl_nb);
		if (ret)
			dev_err(&sdev->dev, "Failed to register scmi misc event: %d\n", src_id);
		else {
			ret = imx_misc_ctrl_ops->misc_ctrl_req_notify(ph, src_id,
								      SCMI_EVENT_IMX_MISC_CONTROL,
								      flags);
			if (ret)
				dev_err(&sdev->dev, "Failed to req notify: %d\n", src_id);
		}
	}

	return 0;
}

static const struct scmi_device_id scmi_id_table[] = {
	{ SCMI_PROTOCOL_IMX_MISC, "imx-misc-ctrl" },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static struct scmi_driver scmi_imx_misc_ctrl_driver = {
	.name = "scmi-imx-misc-ctrl",
	.probe = scmi_imx_misc_ctrl_probe,
	.id_table = scmi_id_table,
};
module_scmi_driver(scmi_imx_misc_ctrl_driver);

MODULE_AUTHOR("Peng Fan <peng.fan@nxp.com>");
MODULE_DESCRIPTION("IMX SM MISC driver");
MODULE_LICENSE("GPL");
