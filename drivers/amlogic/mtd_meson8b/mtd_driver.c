/*
 * drivers/amlogic/mtd/mtd_driver.c
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
 */

#include "aml_mtd.h"

/* extern int nand_get_device( struct mtd_info *mtd, int new_state); */
/* extern void nand_release_device(struct mtd_info *mtd); */

#define nand_notifier_to_blk(l) container_of(l, struct aml_nand_device, nb)

static int mtd_nand_reboot_notifier(struct notifier_block *nb,
	unsigned long priority, void *arg)
{
	int error = 0;
	struct aml_nand_device *aml_nand_dev = nand_notifier_to_blk(nb);
	struct aml_nand_platform *plat = NULL;
	struct aml_nand_chip *aml_chip = NULL;
	struct mtd_info *mtd = NULL;
	int i;

	pr_info("%s %d\n", __func__, __LINE__);
	for (i = 1; i < aml_nand_dev->dev_num; i++) {
		plat = &aml_nand_dev->aml_nand_platform[i];
		aml_chip = plat->aml_chip;
		if (aml_chip) {
			mtd = aml_chip->mtd;
			/*fixit! nothing*/
		}
	}

	return error;
}

static const struct of_device_id amlogic_nand_dt_match[] = {
	{
		.compatible = "amlogic, aml_mtd_nand",
		.data = (void *)&aml_nand_mid_device,
	},
	{},
};

static inline struct aml_nand_device
	*aml_get_driver_data(struct platform_device *pdev)
{
	if (pdev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_node(amlogic_nand_dt_match, pdev->dev.of_node);
		return (struct aml_nand_device *)match->data;
	}
	return NULL;
}

static int amlogic_parse_nand_partion(struct device_node *np,
	struct aml_nand_platform *plat)
{
	struct mtd_partition *mtd_ptn = NULL;
	struct mtd_partition *p = NULL;
	struct device_node *child;
	unsigned int part_num = 0;

	child = of_get_next_child(np, NULL);
	if (!child) {
		pr_info("no partition\n");
		goto  err;
	}
	part_num = of_get_child_count(np);
	if (part_num > 0) {
		mtd_ptn = kcalloc(part_num, sizeof(struct mtd_partition),
			GFP_KERNEL);
		p = mtd_ptn;
		if (!mtd_ptn) {
			pr_info("malloc mtd_partition error\n");
			goto err;
		}
		memset(p, 0, sizeof(struct mtd_partition) * part_num);
		for_each_child_of_node(np, child) {
			p->name = (char *)child->name;
			if (of_property_read_u64(child, "offset", &p->offset))
				goto err;
			if (of_property_read_u64(child, "size", &p->size))
				goto err;
			p++;
		}
		plat->platform_nand_data.chip.partitions = mtd_ptn;
		plat->platform_nand_data.chip.nr_partitions = part_num;
		return 0;
	}
err:
	if (mtd_ptn)
		kzfree(mtd_ptn);
	return -1;
}

static int prase_get_dtb_nand_parameter(struct aml_nand_device *aml_nand_dev,
	struct platform_device *pdev)
{
	const char *propname;
	struct property *prop;
	const __be32 *list;
	int size, config, index;
	phandle phandle;
	struct device_node *np_config;
	struct device_node *np_part;
	struct device_node *np = pdev->dev.of_node;
	struct aml_nand_platform *plat = NULL;
	struct aml_nand_platform *plat_array = NULL;
	int val = 0, ret;

	pr_info("%s:%d,parse dts start\n", __func__, __LINE__);
	if (pdev->dev.of_node) {
		of_node_get(np);
		ret = of_property_read_u32(np,
			"plat-num", (u32 *)&aml_nand_dev->dev_num);
		if (ret) {
			pr_info("%s:%d,please config plat-num item\n",
				__func__, __LINE__);
			return -1;
		}
		if (aml_nand_dev->dev_num > 0) {
			plat_array =
			kzalloc(sizeof(*plat_array) * aml_nand_dev->dev_num,
				GFP_KERNEL);
			if (!plat_array)
				return -1;
			aml_nand_dev->aml_nand_platform = plat_array;
		}

		for (index = 0; index < aml_nand_dev->dev_num; index++) {
			plat = &aml_nand_dev->aml_nand_platform[index];
			if (plat == NULL) {
				pr_info("%s:%d, kzalloc mem fail\n",
					__func__, __LINE__);
				continue;
			}
			propname = kasprintf(GFP_KERNEL, "plat-part-%d", index);
			prop = of_find_property(np, propname, &size);
			kfree(propname);
			if (!prop)
				break;
			list = prop->value;
			size /= sizeof(*list);
			ret = of_property_read_string_index(np,
				"plat-names",
				index, (const char **)&plat->name);
			if (ret < 0)
				plat->name = prop->name + 8;

			for (config = 0; config < size; config++) {
				phandle = be32_to_cpup(list++);
			np_config = of_find_node_by_phandle(phandle);
			if (!np_config) {
				pr_info("%s:%d,not find device node\n",
					__func__, __LINE__);
				goto err;
			}
			/*
			 *ret = of_property_read_u32(np_config,
			 *		"part_num",
			 *	&plat->platform_nand_data.chip.nr_partitions);
			 **/
			ret = of_property_read_u32(np_config,
				"partition", &val);
			if (ret == 0) {
				phandle = val;
				np_part =
				of_find_node_by_phandle(phandle);
				if (!np_config) {
					pr_info("%s:%d not find device node\n",
						__func__, __LINE__);
					goto err;
				}
				ret = amlogic_parse_nand_partion(np_part, plat);
				if (ret) {
					pr_info("%s:%d,prase part table fail\n",
						__func__, __LINE__);
					goto err;
				}
			}
			of_node_put(np_config);
			}
		}
	};
	pr_info("%s:%d,parse dts end\n", __func__, __LINE__);
	return 0;
err:
	return -1;
}

static int mtd_nand_probe(struct platform_device *pdev)
{
	struct aml_nand_device *aml_nand_dev = NULL;
	int err = 0;

	aml_nand_dev = aml_get_driver_data(pdev);
	pdev->dev.platform_data = aml_nand_dev;

	if (!aml_nand_dev) {
		dev_err(&pdev->dev, "no platform specific information\n");
		err = -ENOMEM;
		goto exit_error;
	}

	platform_set_drvdata(pdev, aml_nand_dev);

	spin_lock_init(&upper_controller.lock);
	init_waitqueue_head(&upper_controller.wq);

	aml_nand_dev->nb.notifier_call = mtd_nand_reboot_notifier;
	register_reboot_notifier(&aml_nand_dev->nb);
	atomic_notifier_chain_register(&panic_notifier_list, &aml_nand_dev->nb);

	/*prase dtb and get device(part) information*/
	prase_get_dtb_nand_parameter(aml_nand_dev, pdev);

	nand_init(pdev);

exit_error:
	return err;
}

static int mtd_nand_remove(struct platform_device *pdev)
{
	struct aml_nand_device *aml_nand_dev = aml_get_driver_data(pdev);
	struct aml_nand_platform *plat = NULL;
	struct aml_nand_chip *aml_chip = NULL;
	struct mtd_info *mtd = NULL;
	int i;

	platform_set_drvdata(pdev, NULL);
	for (i = 0; i < aml_nand_dev->dev_num; i++) {
		plat = &aml_nand_dev->aml_nand_platform[i];
		aml_chip = plat->aml_chip;
		if (aml_chip) {
			mtd = aml_chip->mtd;
			if (mtd) {
				nand_release(mtd);
				kfree(mtd);
			}
			kfree(aml_chip);
		}
	}
	if (pdev->dev.of_node) {
		if (aml_nand_dev->aml_nand_platform) {
			for (i = 0; i < aml_nand_dev->dev_num; i++)
				plat = &aml_nand_dev->aml_nand_platform[i];
			kfree(aml_nand_dev->aml_nand_platform);
		}
		of_node_put(pdev->dev.of_node);
	}
	return 0;
}

static void mtd_nand_shutdown(struct platform_device *pdev)
{
	/*NULL*/
}

/* driver device registration */
static struct platform_driver mtd_nand_driver = {
	.probe		= mtd_nand_probe,
	.remove		= mtd_nand_remove,
	.shutdown	= mtd_nand_shutdown,
	.driver		= {
		.name	= "aml_mtd_nand",
		.owner	= THIS_MODULE,
		.of_match_table = amlogic_nand_dt_match,
	},
};

static int __init mtd_nand_init(void)
{
	pr_info("amlogic mtd driver init\n");
	return platform_driver_register(&mtd_nand_driver);
}

static void __exit mtd_nand_exit(void)
{
	pr_info("amlogic mtd driver exit\n");
	platform_driver_unregister(&mtd_nand_driver);
}

module_init(mtd_nand_init);
module_exit(mtd_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AML NAND TEAM");
MODULE_DESCRIPTION("aml mtd nand flash driver");

