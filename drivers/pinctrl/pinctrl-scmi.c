// SPDX-License-Identifier: GPL-2.0
/*
 * System Control and Power Interface (SCMI) Protocol based pinctrl driver
 *
 * Copyright (C) 2023 EPAM
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/seq_file.h>
#include <linux/scmi_protocol.h>
#include <linux/slab.h>

#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>

#include "pinctrl-utils.h"
#include "core.h"
#include "pinconf.h"

#define DRV_NAME "scmi-pinctrl"

static const struct scmi_pinctrl_proto_ops *pinctrl_ops;

struct scmi_pinctrl_funcs {
	unsigned int num_groups;
	const char **groups;
};

struct scmi_pinctrl {
	struct device *dev;
	struct scmi_protocol_handle *ph;
	struct pinctrl_dev *pctldev;
	struct pinctrl_desc pctl_desc;
	struct scmi_pinctrl_funcs *functions;
	unsigned int nr_functions;
	char **groups;
	unsigned int nr_groups;
	struct pinctrl_pin_desc *pins;
	unsigned int nr_pins;
};

static int pinctrl_scmi_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct scmi_pinctrl *pmx = pinctrl_dev_get_drvdata(pctldev);

	return pinctrl_ops->count_get(pmx->ph, GROUP_TYPE);
}

static const char *pinctrl_scmi_get_group_name(struct pinctrl_dev *pctldev, unsigned int selector)
{
	int ret;
	const char *name;
	struct scmi_pinctrl *pmx = pinctrl_dev_get_drvdata(pctldev);

	ret = pinctrl_ops->name_get(pmx->ph, selector, GROUP_TYPE, &name);
	if (ret) {
		dev_err(pmx->dev, "get name failed with err %d", ret);
		return NULL;
	}

	return name;
}

static int pinctrl_scmi_get_group_pins(struct pinctrl_dev *pctldev,
				       unsigned int selector,
				       const unsigned int **pins,
				       unsigned int *num_pins)
{
	struct scmi_pinctrl *pmx = pinctrl_dev_get_drvdata(pctldev);

	return pinctrl_ops->group_pins_get(pmx->ph, selector, pins, num_pins);
}

#ifdef CONFIG_OF
#if IS_ENABLED(CONFIG_PINCTRL_IMX)
extern int imx_scmi_dt_node_to_map(struct pinctrl_dev *pctldev, struct device_node *np,
				   struct pinctrl_map **map, unsigned int *num_maps);
#else
static inline int imx_scmi_dt_node_to_map(struct pinctrl_dev *pctldev,
					  struct device_node *np,
					  struct pinctrl_map **map, unsigned int *num_maps)
{
	return -EOPNOTSUPP;
}
#endif
static int pinctrl_scmi_dt_node_to_map(struct pinctrl_dev *pctldev,
				       struct device_node *np_config,
				       struct pinctrl_map **map,
				       u32 *num_maps)
{
	if ((of_machine_is_compatible("fsl,imx93") || of_machine_is_compatible("fsl,imx95")))
		return imx_scmi_dt_node_to_map(pctldev, np_config, map, num_maps);

	return pinconf_generic_dt_node_to_map(pctldev, np_config, map, num_maps,
					      PIN_MAP_TYPE_INVALID);

}
#endif /* CONFIG_OF */

static const struct pinctrl_ops pinctrl_scmi_pinctrl_ops = {
	.get_groups_count = pinctrl_scmi_get_groups_count,
	.get_group_name = pinctrl_scmi_get_group_name,
	.get_group_pins = pinctrl_scmi_get_group_pins,
#ifdef CONFIG_OF
	.dt_node_to_map = pinctrl_scmi_dt_node_to_map,
	.dt_free_map = pinconf_generic_dt_free_map,
#endif
};

static int pinctrl_scmi_get_functions_count(struct pinctrl_dev *pctldev)
{
	struct scmi_pinctrl *pmx = pinctrl_dev_get_drvdata(pctldev);

	return pinctrl_ops->count_get(pmx->ph, FUNCTION_TYPE);
}

static const char *pinctrl_scmi_get_function_name(struct pinctrl_dev *pctldev,
						  unsigned int selector)
{
	int ret;
	const char *name;
	struct scmi_pinctrl *pmx = pinctrl_dev_get_drvdata(pctldev);

	ret = pinctrl_ops->name_get(pmx->ph, selector, FUNCTION_TYPE, &name);
	if (ret) {
		dev_err(pmx->dev, "get name failed with err %d", ret);
		return NULL;
	}

	return name;
}

static int pinctrl_scmi_get_function_groups(struct pinctrl_dev *pctldev,
					    unsigned int selector,
					    const char * const **groups,
					    unsigned int * const num_groups)
{
	const unsigned int *group_ids;
	int ret, i;
	struct scmi_pinctrl *pmx = pinctrl_dev_get_drvdata(pctldev);

	if (!groups || !num_groups)
		return -EINVAL;

	if (selector < pmx->nr_functions &&
	    pmx->functions[selector].num_groups) {
		*groups = (const char * const *)pmx->functions[selector].groups;
		*num_groups = pmx->functions[selector].num_groups;
		return 0;
	}

	ret = pinctrl_ops->function_groups_get(pmx->ph, selector,
					       &pmx->functions[selector].num_groups,
					       &group_ids);
	if (ret) {
		dev_err(pmx->dev, "Unable to get function groups, err %d", ret);
		return ret;
	}

	*num_groups = pmx->functions[selector].num_groups;
	if (!*num_groups)
		return -EINVAL;

	pmx->functions[selector].groups =
		devm_kcalloc(pmx->dev, *num_groups, sizeof(*pmx->functions[selector].groups),
			GFP_KERNEL);
	if (!pmx->functions[selector].groups)
		return -ENOMEM;

	for (i = 0; i < *num_groups; i++) {
		pmx->functions[selector].groups[i] =
			pinctrl_scmi_get_group_name(pmx->pctldev,
						    group_ids[i]);
		if (!pmx->functions[selector].groups[i]) {
			ret = -ENOMEM;
			goto err_free;
		}
	}

	*groups = (const char * const *)pmx->functions[selector].groups;

	return 0;

err_free:
	devm_kfree(pmx->dev, pmx->functions[selector].groups);

	return ret;
}

static int pinctrl_scmi_func_set_mux(struct pinctrl_dev *pctldev, unsigned int selector,
				     unsigned int group)
{
	struct scmi_pinctrl *pmx = pinctrl_dev_get_drvdata(pctldev);

	return pinctrl_ops->mux_set(pmx->ph, selector, group);
}

static int pinctrl_scmi_request(struct pinctrl_dev *pctldev, unsigned int offset)
{
	struct scmi_pinctrl *pmx = pinctrl_dev_get_drvdata(pctldev);

	return pinctrl_ops->pin_request(pmx->ph, offset);
}

static int pinctrl_scmi_free(struct pinctrl_dev *pctldev, unsigned int offset)
{
	struct scmi_pinctrl *pmx = pinctrl_dev_get_drvdata(pctldev);

	return pinctrl_ops->pin_free(pmx->ph, offset);
}

static const struct pinmux_ops pinctrl_scmi_pinmux_ops = {
	.request = pinctrl_scmi_request,
	.free = pinctrl_scmi_free,
	.get_functions_count = pinctrl_scmi_get_functions_count,
	.get_function_name = pinctrl_scmi_get_function_name,
	.get_function_groups = pinctrl_scmi_get_function_groups,
	.set_mux = pinctrl_scmi_func_set_mux,
};

static int pinctrl_scmi_pinconf_get(struct pinctrl_dev *pctldev, unsigned int _pin,
				    unsigned long *config)
{
	int ret;
	struct scmi_pinctrl *pmx = pinctrl_dev_get_drvdata(pctldev);
	enum pin_config_param config_type;
	unsigned long config_value;

	if (!config)
		return -EINVAL;

	config_type = pinconf_to_config_param(*config);

	ret = pinctrl_ops->config_get(pmx->ph, _pin, PIN_TYPE, config_type, &config_value);
	if (ret)
		return ret;

	*config = pinconf_to_config_packed(config_type, config_value);

	return 0;
}

static int pinctrl_scmi_pinconf_set(struct pinctrl_dev *pctldev,
				    unsigned int _pin,
				    unsigned long *configs,
				    unsigned int num_configs)
{
	int ret;
	struct scmi_pinctrl *pmx = pinctrl_dev_get_drvdata(pctldev);

	if (!configs || num_configs == 0)
		return -EINVAL;

	ret = pinctrl_ops->config_set(pmx->ph, _pin, PIN_TYPE, configs, num_configs);
	if (ret)
		dev_err(pmx->dev, "Error parsing config %d\n", ret);

	return ret;
}

static int pinctrl_scmi_pinconf_group_set(struct pinctrl_dev *pctldev,
					  unsigned int group,
					  unsigned long *configs,
					  unsigned int num_configs)
{
	int ret;
	struct scmi_pinctrl *pmx =  pinctrl_dev_get_drvdata(pctldev);

	if (!configs || num_configs == 0)
		return -EINVAL;

	ret = pinctrl_ops->config_set(pmx->ph, group, GROUP_TYPE, configs, num_configs);
	if (ret)
		dev_err(pmx->dev, "Error parsing config %d", ret);

	return ret;
};

static int pinctrl_scmi_pinconf_group_get(struct pinctrl_dev *pctldev,
					  unsigned int group,
					  unsigned long *config)
{
	int ret;
	struct scmi_pinctrl *pmx = pinctrl_dev_get_drvdata(pctldev);
	enum pin_config_param config_type;
	unsigned long config_value;

	if (!config)
		return -EINVAL;

	config_type = pinconf_to_config_param(*config);

	ret = pinctrl_ops->config_get(pmx->ph, group, GROUP_TYPE, config_type, &config_value);
	if (ret)
		return ret;

	*config = pinconf_to_config_packed(config_type, config_value);

	return 0;
}

static const struct pinconf_ops pinctrl_scmi_pinconf_ops = {
	.is_generic = true,
	.pin_config_get = pinctrl_scmi_pinconf_get,
	.pin_config_set = pinctrl_scmi_pinconf_set,
	.pin_config_group_set = pinctrl_scmi_pinconf_group_set,
	.pin_config_group_get = pinctrl_scmi_pinconf_group_get,
	.pin_config_config_dbg_show = pinconf_generic_dump_config,
};

static int pinctrl_scmi_get_pins(struct scmi_pinctrl *pmx,
				 unsigned int *nr_pins,
				 const struct pinctrl_pin_desc **pins)
{
	int ret, i;

	if (!pins || !nr_pins)
		return -EINVAL;

	if (pmx->nr_pins) {
		*pins = pmx->pins;
		*nr_pins = pmx->nr_pins;
		return 0;
	}

	*nr_pins = pinctrl_ops->count_get(pmx->ph, PIN_TYPE);

	pmx->nr_pins = *nr_pins;
	pmx->pins = devm_kmalloc_array(pmx->dev, *nr_pins, sizeof(*pmx->pins), GFP_KERNEL);
	if (!pmx->pins)
		return -ENOMEM;

	for (i = 0; i < *nr_pins; i++) {
		pmx->pins[i].number = i;
		ret = pinctrl_ops->name_get(pmx->ph, i, PIN_TYPE, &pmx->pins[i].name);
		if (ret) {
			dev_err(pmx->dev, "Can't get name for pin %d: rc %d", i, ret);
			pmx->nr_pins = 0;
			return ret;
		}
	}

	*pins = pmx->pins;
	dev_dbg(pmx->dev, "got pins %d", *nr_pins);

	return 0;
}

static const struct scmi_device_id scmi_id_table[] = {
	{ SCMI_PROTOCOL_PINCTRL, "pinctrl" },
	{ }
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static int scmi_pinctrl_probe(struct scmi_device *sdev)
{
	int ret;
	struct device *dev = &sdev->dev;
	struct scmi_pinctrl *pmx;
	const struct scmi_handle *handle;
	struct scmi_protocol_handle *ph;

	if (!sdev || !sdev->handle)
		return -EINVAL;

	handle = sdev->handle;

	pinctrl_ops = handle->devm_protocol_get(sdev, SCMI_PROTOCOL_PINCTRL, &ph);
	if (IS_ERR(pinctrl_ops))
		return PTR_ERR(pinctrl_ops);

	pmx = devm_kzalloc(dev, sizeof(*pmx), GFP_KERNEL);
	if (!pmx)
		return -ENOMEM;

	pmx->ph = ph;

	pmx->dev = dev;
	pmx->pctl_desc.name = DRV_NAME;
	pmx->pctl_desc.owner = THIS_MODULE;
	pmx->pctl_desc.pctlops = &pinctrl_scmi_pinctrl_ops;
	pmx->pctl_desc.pmxops = &pinctrl_scmi_pinmux_ops;
	pmx->pctl_desc.confops = &pinctrl_scmi_pinconf_ops;

	ret = pinctrl_scmi_get_pins(pmx, &pmx->pctl_desc.npins,
				    &pmx->pctl_desc.pins);
	if (ret)
		return ret;

	ret = devm_pinctrl_register_and_init(dev, &pmx->pctl_desc, pmx, &pmx->pctldev);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to register pinctrl\n");

	pmx->nr_functions = pinctrl_scmi_get_functions_count(pmx->pctldev);
	pmx->nr_groups = pinctrl_scmi_get_groups_count(pmx->pctldev);

	if (pmx->nr_functions) {
		pmx->functions =
			devm_kcalloc(dev, pmx->nr_functions, sizeof(*pmx->functions),
				     GFP_KERNEL);
		if (!pmx->functions)
			return -ENOMEM;
	}

	if (pmx->nr_groups) {
		pmx->groups =
			devm_kcalloc(dev, pmx->nr_groups, sizeof(*pmx->groups), GFP_KERNEL);
		if (!pmx->groups)
			return -ENOMEM;
	}

	return pinctrl_enable(pmx->pctldev);
}

static struct scmi_driver scmi_pinctrl_driver = {
	.name = DRV_NAME,
	.probe = scmi_pinctrl_probe,
	.id_table = scmi_id_table,
};
module_scmi_driver(scmi_pinctrl_driver);

MODULE_AUTHOR("Oleksii Moisieiev <oleksii_moisieiev@epam.com>");
MODULE_DESCRIPTION("ARM SCMI pin controller driver");
MODULE_LICENSE("GPL");
