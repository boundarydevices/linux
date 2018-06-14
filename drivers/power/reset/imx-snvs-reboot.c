/*
 * Driver to write reboot parameter to the SNVS LPGPR reg.
 *
 * Copyright 2018 Boundary Devices, LLC.
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/regmap.h>

#define SNVS_LPGPR__REG	0x68	/* General Purpose Register 0 */

#define NORMAL_BOOT     6
#define RECOVERY_BOOT   7
#define FASTBOOT_BOOT   8

struct snvs_reboot_data {
	struct regmap *regmap;
	int offset;
};

static struct snvs_reboot_data snvs_pdata;

static void snvs_reboot_set_mode(char mode)
{
	struct snvs_reboot_data *pdata = &snvs_pdata;
	int val = 1 << mode;

	pr_debug("Setting %x to LPGPR0\n", val);

	regmap_update_bits(pdata->regmap, pdata->offset, val, val);
}

static int snvs_reboot_notifier_call(
		struct notifier_block *notifier,
		unsigned long what, void *data)
{
	int ret = NOTIFY_DONE;
	char *cmd = (char *)data;

	pr_debug("%s\n", cmd);

	if (cmd && strcmp(cmd, "recovery") == 0)
		snvs_reboot_set_mode(RECOVERY_BOOT);
	else if (cmd && strcmp(cmd, "bootloader") == 0)
		snvs_reboot_set_mode(FASTBOOT_BOOT);
	else if (cmd && strcmp(cmd, "") == 0)
		snvs_reboot_set_mode(NORMAL_BOOT);

	return ret;
}

static struct notifier_block snvs_reboot_notifier = {
	.notifier_call = snvs_reboot_notifier_call,
};

static int snvs_reboot_probe(struct platform_device *pdev)
{
	struct snvs_reboot_data *pdata = &snvs_pdata;
	struct device_node *np;
	int err;

	/* Get SNVS register Page */
	np = pdev->dev.of_node;
	if (!np)
		return -ENODEV;

	pdata->regmap = syscon_regmap_lookup_by_phandle(np, "regmap");
	if (IS_ERR(pdata->regmap)) {
		dev_err(&pdev->dev, "Can't get snvs syscon\n");
		return PTR_ERR(pdata->regmap);
	}

	/* Register this driver for reboot notifications */
	err = register_reboot_notifier(&snvs_reboot_notifier);
	if (err) {
		dev_err(&pdev->dev, "Failed to init reboot notifier\n");
		return err;
	}

	pdata->offset = SNVS_LPGPR__REG;
	of_property_read_u32(pdev->dev.of_node, "offset", &pdata->offset);

	dev_dbg(&pdev->dev, "Probed, LPGPR0 offset: %x\n", pdata->offset);

	return 0;
}

static int snvs_reboot_remove(struct platform_device *pdev)
{
	unregister_reboot_notifier(&snvs_reboot_notifier);

	return 0;
}

static const struct of_device_id snvs_reboot_ids[] = {
	{ .compatible = "fsl,sec-v4.0-reboot" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, snvs_reboot_ids);

static struct platform_driver snvs_reboot_driver = {
	.driver = {
		.name = "snvs_reboot",
		.of_match_table = snvs_reboot_ids,
	},
	.probe = snvs_reboot_probe,
	.remove = snvs_reboot_remove,
};
module_platform_driver(snvs_reboot_driver);

MODULE_AUTHOR("Boundary Devices");
MODULE_DESCRIPTION("i.MX SNVS reboot driver");
MODULE_LICENSE("GPL");
