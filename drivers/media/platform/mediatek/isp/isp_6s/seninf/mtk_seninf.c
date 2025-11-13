// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/of_graph.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/component.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/nvmem-consumer.h>
#include <linux/string.h>

#include <linux/ioport.h>
#include <linux/io.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>

#include "mtk_seninf.h"
#include "mtk_seninf_ops.h"
#include "mtk_seninf_route.h"

#define sd_to_ctx(__sd) container_of(__sd, struct seninf_ctx, subdev)
#define notifier_to_ctx(__n) container_of(__n, struct seninf_ctx, notifier)
#define ctrl_hdl_to_ctx(__h) container_of(__h, struct seninf_ctx, ctrl_handler)

struct platform_driver seninf_pdrv;

struct mtk_seninf_ops *g_seninf_ops;

static const char * const csi_port_names[] = {
	SENINF_CSI_PORT_NAMES
};

static const char * const clk_names[] = {
	SENINF_CLK_NAMES
};

static const char * const set_reg_names[] = {
	SET_REG_KEYS_NAMES
};

static const char * const engine_names[] = {
	ISP_ENGING_NAMES
};

struct mtk_seninf_plat_data {
	struct mtk_seninf_ops *seninf_ops;
};

static const struct mtk_seninf_plat_data mt8189_data = {
	.seninf_ops = &mtk_csi_phy_2_1,
};

static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	return g_seninf_ops->_show_status(dev, attr, buf);
}

static DEVICE_ATTR_RO(status);

static ssize_t err_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return g_seninf_ops->_show_err_status(dev, attr, buf);
}

static DEVICE_ATTR_RO(err_status);

static ssize_t debug_ops_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "This is debug ops message\n");

	return len;
}

enum REG_OPS_CMD {
	REG_OPS_CMD_ID,
	REG_OPS_CMD_CSI,
	REG_OPS_CMD_RG,
	REG_OPS_CMD_VAL,
	REG_OPS_CMD_MAX_NUM,
};

static ssize_t debug_ops_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	char delim[] = " ";
	char csi_names[20];
	char *token = NULL;
	char *sbuf = kcalloc(count + 1, sizeof(char), GFP_KERNEL);
	char *s = sbuf;
	int ret;
	char *arg[REG_OPS_CMD_MAX_NUM];
	struct seninf_core *core = dev_get_drvdata(dev);
	struct seninf_ctx *ctx;
	int csi_port = -1;
	int rg_idx = -1;
	u32 val, i, num_para = 0;

	if (!sbuf)
		goto ERR_DEBUG_OPS_STORE;

	memcpy(sbuf, buf, count);

	token = strsep(&s, delim);
	while (token && num_para < REG_OPS_CMD_MAX_NUM) {
		if (strlen(token)) {
			arg[num_para] = token;
			num_para++;
		}

		token = strsep(&s, delim);
	}

	if (num_para != REG_OPS_CMD_MAX_NUM) {
		dev_err(dev, "Wrong command parameter number\n");
		goto ERR_DEBUG_OPS_STORE;
	}

	if (strncmp("SET_REG", arg[REG_OPS_CMD_ID], sizeof("SET_REG")) == 0) {
		for (i = 0; i < REG_KEY_MAX_NUM; i++) {
			if (!strcasecmp(arg[REG_OPS_CMD_RG], set_reg_names[i]))
				rg_idx = i;
		}
		if (rg_idx < 0)
			goto ERR_DEBUG_OPS_STORE;

		ret = kstrtouint(arg[REG_OPS_CMD_VAL], 0, &val);
		if (ret)
			goto ERR_DEBUG_OPS_STORE;

		for (i = 0; i < CSI_PORT_MAX_NUM; i++) {
			memset(csi_names, 0, ARRAY_SIZE(csi_names));
			snprintf(csi_names, 10, "csi-%s", csi_port_names[i]);
			if (!strcasecmp(arg[REG_OPS_CMD_CSI], csi_names))
				csi_port = i;
		}

		if (csi_port < 0)
			goto ERR_DEBUG_OPS_STORE;

		// reg call
		mutex_lock(&core->mutex);

		list_for_each_entry(ctx, &core->list, list) {
			if (csi_port == ctx->port)
				g_seninf_ops->_set_reg(ctx, rg_idx, val);
		}

		mutex_unlock(&core->mutex);
	}

ERR_DEBUG_OPS_STORE:

	kfree(sbuf);

	return count;
}

static DEVICE_ATTR_RW(debug_ops);

static int seninf_dfs_exit(struct seninf_dfs *dfs)
{
	if (!dfs->cnt)
		return 0;

	dev_pm_opp_of_remove_table(dfs->dev);

	return 0;
}

static int __seninf_dfs_set(struct seninf_ctx *ctx, unsigned long freq)
{
	int i;
	struct seninf_ctx *tmp;
	struct seninf_core *core = ctx->core;
	struct seninf_dfs *dfs = &core->dfs;
	unsigned long require = 0, old;

	if (!dfs->cnt)
		return 0;

	old = ctx->isp_freq;
	ctx->isp_freq = freq;

	list_for_each_entry(tmp, &core->list, list) {
		if (tmp->isp_freq > require)
			require = tmp->isp_freq;
	}

	for (i = 0; i < dfs->cnt; i++) {
		if (dfs->freqs[i] >= require)
			break;
	}

	if (i == dfs->cnt) {
		ctx->isp_freq = old;
		return -EINVAL;
	}

	regulator_set_voltage(dfs->reg, dfs->volts[i],
			      dfs->volts[dfs->cnt - 1]);

	dev_info(ctx->dev, "freq %ld require %ld selected %ld\n",
		 freq, require, dfs->freqs[i]);

	return 0;
}

static int seninf_dfs_set(struct seninf_ctx *ctx, unsigned long freq)
{
	int ret;
	struct seninf_core *core = ctx->core;

	mutex_lock(&core->mutex);
	ret = __seninf_dfs_set(ctx, freq);
	mutex_unlock(&core->mutex);

	return ret;
}

static int seninf_core_pm_runtime_enable(struct seninf_core *core)
{
	int i, ret;

	core->pm_domain_cnt = of_count_phandle_with_args(core->dev->of_node,
							 "power-domains",
							 "#power-domain-cells");
	if (core->pm_domain_cnt == 1) {
		pm_runtime_enable(core->dev);
	} else if (core->pm_domain_cnt > 1) {
		core->pm_domain_devs =
			devm_kcalloc(core->dev, core->pm_domain_cnt,
				     sizeof(*core->pm_domain_devs), GFP_KERNEL);

		if (!core->pm_domain_devs)
			return -ENOMEM;

		for (i = 0; i < core->pm_domain_cnt; i++) {
			core->pm_domain_devs[i] =
				dev_pm_domain_attach_by_id(core->dev, i);
			dev_info(core->pm_domain_devs[i], "%s: attach pm id: %d\n", __func__, i);

			if (IS_ERR(core->pm_domain_devs[i])) {
				ret = PTR_ERR(core->pm_domain_devs[i]);

				dev_err(core->dev,
					 "%s: fail to probe pm id: %d (%d)\n",
					 __func__, i, ret);

				goto detach_pm;
			}
		}
	}

	return 0;

detach_pm:
	for (--i; i >= 0; i--)
		dev_pm_domain_detach(core->pm_domain_devs[i], true);

	return ret;
}

static int seninf_core_pm_runtime_disable(struct seninf_core *core)
{
	int i;

	if (core->pm_domain_cnt == 1) {
		pm_runtime_disable(core->dev);
	} else {
		if (!core->pm_domain_devs)
			return -EINVAL;

		for (i = 0; i < core->pm_domain_cnt; i++) {
			if (core->pm_domain_devs[i])
				dev_pm_domain_detach(core->pm_domain_devs[i], 1);
		}
	}

	return 0;
}

static int seninf_core_pm_runtime_get_sync(struct seninf_core *core)
{
	int ret, i;

	/* for one pm_domain */
	if (core->pm_domain_cnt == 1) {
		ret = pm_runtime_resume_and_get(core->dev);
		if (ret < 0) {
			dev_err(core->dev, "fail to resume seninf_core\n");
			return ret;
		}

		return 0;
	}

	if (!core->pm_domain_devs)
		return -EINVAL;

	/* more than one pm_domain */
	for (i = 0; i < core->pm_domain_cnt; i++) {
		if (core->pm_domain_devs[i]) {
			ret = pm_runtime_resume_and_get(core->pm_domain_devs[i]);
			if (ret < 0) {
				dev_info(core->dev,
					 "fail to resume pm_domain_devs(%d)\n", i);
				return ret;
			}
		}
	}

	return 0;
}

static int seninf_core_pm_runtime_put(struct seninf_core *core)
{
	int i;

	if (core->pm_domain_cnt == 1) {
		pm_runtime_put_sync(core->dev);
	} else {
		if (!core->pm_domain_devs || core->pm_domain_cnt < 1)
			return -EINVAL;

		for (i = core->pm_domain_cnt - 1; i >= 0; i--) {
			if (core->pm_domain_devs[i])
				pm_runtime_put_sync(core->pm_domain_devs[i]);
		}
	}

	return 0;
}

static irqreturn_t mtk_irq_seninf(int irq, void *data)
{
	g_seninf_ops->_irq_handler(irq, data);
	return IRQ_HANDLED;
}

static int get_seninf_ops(struct device *dev, struct seninf_core *core)
{
	int i;

	for (i = 0; i < SENINF_PHY_VER_NUM; i++) {
		of_property_read_u32(dev->of_node, "seninf_num",
				     &g_seninf_ops->seninf_num);
		of_property_read_u32(dev->of_node, "mux_num",
				     &g_seninf_ops->mux_num);
		of_property_read_u32(dev->of_node, "cam_mux_num",
				     &g_seninf_ops->cam_mux_num);
		of_property_read_u32(dev->of_node, "pref_mux_num",
				     &g_seninf_ops->pref_mux_num);

		dev_info(dev,
			 "%s: seninf_num = %d, mux_num = %d, cam_mux_num = %d, pref_mux_num =%d\n",
			 __func__,
			 g_seninf_ops->seninf_num,
			 g_seninf_ops->mux_num,
			 g_seninf_ops->cam_mux_num,
			 g_seninf_ops->pref_mux_num);

		return 0;
	}

	dev_err(dev, "seninf config not correct.\n");
	return -1;
}

static int mtk_seninf_master_bind(struct device *dev)
{
	struct seninf_core *seninf_core_dev = dev_get_drvdata(dev);
	struct media_device *media_dev = &seninf_core_dev->media_dev;
	int ret;

	media_dev->dev = seninf_core_dev->dev;
	strscpy(media_dev->model, MEDIA_DRVNAME,
		sizeof(media_dev->model));
	strscpy(media_dev->driver_name, MEDIA_DRVNAME,
		sizeof(media_dev->driver_name));
	ret = snprintf(media_dev->bus_info, sizeof(media_dev->bus_info),
		       "platform:%s", dev_name(dev));
	if (ret >= sizeof(media_dev->bus_info))
		dev_dbg(dev, "%s: string truncated\n", __func__);
	media_dev->hw_revision = 0;
	// media_dev->ops = &mtk_seninf_dev_ops;
	media_device_init(media_dev);

	seninf_core_dev->v4l2_dev.mdev = media_dev;
	ret = v4l2_device_register(seninf_core_dev->dev, &seninf_core_dev->v4l2_dev);
	if (ret) {
		dev_err(dev, "Failed to register V4L2 device: %d\n", ret);
		goto fail_media_device_cleanup;
	}

	ret = media_device_register(media_dev);
	if (ret) {
		dev_err(dev, "Failed to register media device: %d\n",
			ret);
		goto fail_v4l2_device_unreg;
	}

	ret = component_bind_all(dev, seninf_core_dev);
	if (ret) {
		dev_err(dev, "Failed to bind all component: %d\n", ret);
		goto fail_media_device_unreg;
	}

	/* Expose all subdev's nodes */
	ret = v4l2_device_register_subdev_nodes(&seninf_core_dev->v4l2_dev);
	if (ret) {
		dev_err(dev, "Failed to register subdev nodes\n");
		goto fail_unreg_camsv_entities;
	}

	dev_info(dev, "%s success\n", __func__);
	return 0;

fail_unreg_camsv_entities:
	component_unbind_all(dev, seninf_core_dev);

fail_media_device_unreg:
	media_device_unregister(&seninf_core_dev->media_dev);

fail_v4l2_device_unreg:
	v4l2_device_unregister(&seninf_core_dev->v4l2_dev);

fail_media_device_cleanup:
	media_device_cleanup(&seninf_core_dev->media_dev);

	return ret;
}

static void mtk_seninf_master_unbind(struct device *dev)
{
	struct seninf_core *seninf_core_dev = dev_get_drvdata(dev);

	// mtk_seninf_dvfs_uninit(seninf_core_dev);
	component_unbind_all(dev, seninf_core_dev);

	media_device_unregister(&seninf_core_dev->media_dev);
	v4l2_device_unregister(&seninf_core_dev->v4l2_dev);
	media_device_cleanup(&seninf_core_dev->media_dev);
}

static const struct component_master_ops mtk_seninf_master_ops = {
	.bind = mtk_seninf_master_bind,
	.unbind = mtk_seninf_master_unbind,
};

static int compare_dev(struct device *dev, void *data)
{
	return dev == (struct device *)data;
}

static void mtk_seninf_match_remove(struct device *dev)
{
	(void) dev;
}

static int add_match_by_driver(struct device *dev,
			       struct component_match **match,
			       struct platform_driver *drv)
{
	struct device *p = NULL, *d;
	int num = 0;

	do {
		d = platform_find_device_by_driver(p, &drv->driver);
		put_device(p);
		p = d;
		if (!d)
			break;

		component_match_add(dev, match, compare_dev, d);
		num++;
	} while (true);

	return num;
}

static struct component_match *mtk_seninf_match_add(struct device *dev)
{
	struct seninf_core *seninf_core_ctx = dev_get_drvdata(dev);
	struct component_match *match = NULL;

	seninf_core_ctx->num_seninf_drivers =
		add_match_by_driver(dev, &match, &seninf_pdrv);

	if (IS_ERR(match))
		mtk_seninf_match_remove(dev);

	dev_info(dev, "#: seninf %d\n", seninf_core_ctx->num_seninf_drivers);

	return match ? match : ERR_PTR(-ENODEV);
}

static int register_sub_drivers(struct device *dev)
{
	struct component_match *match = NULL;
	int ret;

	ret = platform_driver_register(&seninf_pdrv);
	if (ret) {
		dev_err(dev, "%s seninf_pdrv fail\n", __func__);
		goto REGISTER_SENINF_FAIL;
	}

	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (ret) {
		dev_err(dev, "%s: failed to create sub devices\n", __func__);
		goto REGISTER_SENINF_FAIL;
	}

	match = mtk_seninf_match_add(dev);
	if (IS_ERR(match)) {
		ret = PTR_ERR(match);
		goto ADD_MATCH_FAIL;
	}

	ret = component_master_add_with_match(dev, &mtk_seninf_master_ops, match);
	if (ret < 0)
		goto MASTER_ADD_MATCH_FAIL;

	return 0;

MASTER_ADD_MATCH_FAIL:
	mtk_seninf_match_remove(dev);

ADD_MATCH_FAIL:
	platform_driver_unregister(&seninf_pdrv);

REGISTER_SENINF_FAIL:
	return ret;
}

static int seninf_core_probe(struct platform_device *pdev)
{
	int i, ret, irq;
	struct resource *res;
	struct seninf_core *core;
	struct device *dev = &pdev->dev;
	const struct mtk_seninf_plat_data *plat_data;

	dev_dbg(dev, "seninf_core | start %s\n", __func__);

	core = devm_kzalloc(&pdev->dev, sizeof(*core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	plat_data = of_device_get_match_data(dev);
	if (!plat_data)
		return -ENODEV;

	g_seninf_ops = plat_data->seninf_ops;
	if (!g_seninf_ops)
		return -ENODEV;

	ret = get_seninf_ops(dev, core);
	if (ret) {
		dev_err(dev, "failed to get seninf ops\n");
		return ret;
	}

	dev_set_drvdata(dev, core);
	core->dev = dev;
	mutex_init(&core->mutex);
	INIT_LIST_HEAD(&core->list);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	core->reg_if = devm_ioremap_resource(dev, res);
	if (IS_ERR(core->reg_if))
		return PTR_ERR(core->reg_if);

	spin_lock_init(&core->spinlock_irq);
	irq = platform_get_irq_optional(pdev, 0);
	if (irq < 0) {
		dev_dbg(dev, "No irq resource\n");
	} else {
		ret = devm_request_irq(dev, irq, mtk_irq_seninf, 0,
				       dev_name(dev), core);
		if (ret) {
			dev_err(dev, "failed to request irq=%d\n", irq);
			return ret;
		}
		dev_dbg(dev, "registered irq=%d\n", irq);
	}

	for (i = 0; i < CLK_MAXCNT; i++) {
		core->clk[i] = devm_clk_get(dev, clk_names[i]);
		if (IS_ERR(core->clk[i])) {
			dev_info(dev, "failed to get %s\n", clk_names[i]);
			core->clk[i] = NULL;
			/* ignore not define seninf */
		}
	}

	mtk_seninf_init_res(core);

	ret = register_sub_drivers(dev);
	if (ret) {
		dev_err(dev, "fail to register sub drivers\n");
		goto REGISTER_SUB_DRIVERS_ERR;
	}

	ret = device_create_file(dev, &dev_attr_status);
	if (ret)
		dev_err(dev, "failed to create sysfs status\n");

	ret = device_create_file(dev, &dev_attr_debug_ops);
	if (ret)
		dev_err(dev, "failed to create sysfs debug ops\n");

	ret = device_create_file(dev, &dev_attr_err_status);
	if (ret)
		dev_err(dev, "failed to create sysfs status\n");

	ret = seninf_core_pm_runtime_enable(core);
	if (ret) {
		dev_err(dev, "failed to enable seninf_core_pm_runtime\n");
		goto PM_RUNTIME_ENABLE_ERR;
	}

	dev_dbg(dev, "seninf_core | stop %s\n", __func__);

	return 0;

PM_RUNTIME_ENABLE_ERR:
	device_remove_file(dev, &dev_attr_status);
	device_remove_file(dev, &dev_attr_debug_ops);
	device_remove_file(dev, &dev_attr_err_status);

REGISTER_SUB_DRIVERS_ERR:
	mtk_seninf_release_res(core);

	return ret;
}

static int seninf_core_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct seninf_core *core = dev_get_drvdata(dev);

	device_remove_file(dev, &dev_attr_status);
	device_remove_file(dev, &dev_attr_debug_ops);
	device_remove_file(dev, &dev_attr_err_status);
	of_platform_depopulate(dev);
	seninf_core_pm_runtime_disable(core);

	component_master_del(dev, &mtk_seninf_master_ops);
	mtk_seninf_match_remove(dev);
	platform_driver_unregister(&seninf_pdrv);

	dev_dbg(dev, "camsys | start %s\n", __func__);

	return 0;
}

static int get_csi_port(struct device *dev, int *port)
{
	int i, ret;
	const char *name;

	ret = of_property_read_string(dev->of_node, "mediatek,csi-port", &name);
	if (ret)
		return ret;

	for (i = 0; i < CSI_PORT_MAX_NUM; i++) {
		if (!strcasecmp(name, csi_port_names[i])) {
			*port = i;
			return 0;
		}
	}

	return -1;
}

static int seninf_subscribe_event(struct v4l2_subdev *sd,
				  struct v4l2_fh *fh,
				  struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subdev_subscribe_event(sd, fh, sub);
	default:
		return -EINVAL;
	}
}

static void init_fmt(struct seninf_ctx *ctx)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ctx->fmt); i++) {
		ctx->fmt[i].format.code = MEDIA_BUS_FMT_SBGGR10_1X10;
		ctx->fmt[i].format.width = DEFAULT_WIDTH;
		ctx->fmt[i].format.height = DEFAULT_HEIGHT;
		ctx->fmt[i].format.field = V4L2_FIELD_NONE;
		ctx->fmt[i].format.colorspace = V4L2_COLORSPACE_SRGB;
		ctx->fmt[i].format.xfer_func = V4L2_XFER_FUNC_DEFAULT;
		ctx->fmt[i].format.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
		ctx->fmt[i].format.quantization = V4L2_QUANTIZATION_DEFAULT;
	}

	for (i = 0; i < ARRAY_SIZE(ctx->vcinfo.vc); i++)
		ctx->vcinfo.vc[i].pixel_mode = SENINF_DEF_PIXEL_MODE;
}

static int dev_read_csi_efuse(struct seninf_ctx *ctx)
{
	struct nvmem_cell *cell;
	size_t len = 0;
	u32 *buf;

	ctx->m_csi_efuse = 0x00000000;

	cell = nvmem_cell_get(ctx->dev, "rg_csi");
	dev_info(ctx->dev, "ctx->port = %d\n", ctx->port);
	if (IS_ERR(cell)) {
		if (PTR_ERR(cell) == -EPROBE_DEFER) {
			dev_err(ctx->dev,
				 "read csi efuse returned with error cell %d\n",
				 -EPROBE_DEFER - EPROBE_DEFER);
			return PTR_ERR(cell);
		}
		dev_info(ctx->dev,
			 "read csi efuse returned with error cell %d\n", -1);
		return -1;
	}
	buf = (u32 *)nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);
	if (IS_ERR(buf)) {
		dev_err(ctx->dev, "read csi efuse returned with error buf\n");
		return PTR_ERR(buf);
	}
	ctx->m_csi_efuse = *buf;
	kfree(buf);
	dev_info(ctx->dev, "Efuse Data: 0x%08x\n", ctx->m_csi_efuse);

	return 0;
}

static const struct v4l2_mbus_framefmt fmt_default = {
	.code = MEDIA_BUS_FMT_SBGGR10_1X10,
	.width = DEFAULT_WIDTH,
	.height = DEFAULT_HEIGHT,
	.field = V4L2_FIELD_NONE,
	.colorspace = V4L2_COLORSPACE_SRGB,
	.xfer_func = V4L2_XFER_FUNC_DEFAULT,
	.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
	.quantization = V4L2_QUANTIZATION_DEFAULT,
};

static int __seninf_set_routing(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_krouting *routing)
{
	int ret;
	struct seninf_ctx *ctx = sd_to_ctx(sd);

	ret = v4l2_subdev_routing_validate(sd, routing, V4L2_SUBDEV_ROUTING_NO_SOURCE_STREAM_MIX);
	if (ret)
		return ret;

	ret = v4l2_subdev_set_routing_with_fmt(sd, state, routing,
					       &fmt_default);
	if (ret)
		return ret;
	return 0;
}

int mtk_seninf_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt)
{
	int ret = 0;

	/* The width, height and code must match. */
	if (source_fmt->format.width != sink_fmt->format.width) {
		dev_err(sd->entity.graph_obj.mdev->dev,
			"%s: width does not match (source %u, sink %u)\n",
			__func__,
			source_fmt->format.width, sink_fmt->format.width);
		ret = -EPIPE;
	}

	if (source_fmt->format.height != sink_fmt->format.height) {
		dev_err(sd->entity.graph_obj.mdev->dev,
			"%s: height does not match (source %u, sink %u)\n",
			__func__,
			source_fmt->format.height, sink_fmt->format.height);
		ret = -EPIPE;
	}

	if (source_fmt->format.code != sink_fmt->format.code) {
		dev_err(sd->entity.graph_obj.mdev->dev,
			"%s: warn: media bus code does not match (source 0x%8.8x, sink 0x%8.8x)\n",
			__func__,
			source_fmt->format.code, sink_fmt->format.code);
		ret = -EPIPE;
	}

	dev_dbg(sd->entity.graph_obj.mdev->dev,
		"%s: link was \"%s\":%u -> \"%s\":%u\n", __func__,
		link->source->entity->name, link->source->index,
		link->sink->entity->name, link->sink->index);

	if (ret)
		dev_err(sd->v4l2_dev->dev, "%s: link validate failed pad/code/w/h: SRC(%d/0x%x/%d/%d), SINK(%d:0x%x/%d/%d)\n",
			 sd->name, source_fmt->pad, source_fmt->format.code,
			 source_fmt->format.width, source_fmt->format.height,
			 sink_fmt->pad, sink_fmt->format.code,
			 sink_fmt->format.width, sink_fmt->format.height);

	return ret;
}

static int mtk_seninf_init_cfg(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state)
{
	struct v4l2_subdev_route routes[PAD_MAXCNT - 1] = { };
	struct v4l2_subdev_krouting routing = {
		.num_routes = PAD_MAXCNT - 1,
		.routes = routes,
	};
	unsigned int i;

	/*
	 * A seninf entity has one sink pad and multiple source pads.
	 * The stream from the sink pad can be routed to any source
	 * pad, so the routes count is the same as the source pad count.
	 * By default, we enable the 1st route in the seninf entity.
	 */
	for (i = 0; i < routing.num_routes; i++) {
		struct v4l2_subdev_route *route = &routes[i];

		route->sink_pad = 0;
		route->sink_stream = 0;
		route->source_pad = 1 + i;
		route->source_stream = 0;
		routes[i].flags = V4L2_SUBDEV_ROUTE_FL_ACTIVE;
	}

	return __seninf_set_routing(sd, sd_state, &routing);
}

static int seninf_set_routing(struct v4l2_subdev *sd,
			      struct v4l2_subdev_state *state,
			      enum v4l2_subdev_format_whence which,
			      struct v4l2_subdev_krouting *routing)
{
	return __seninf_set_routing(sd, state, routing);
}

static int mtk_seninf_set_fmt(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_format *fmt)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	struct v4l2_mbus_framefmt *format, *stream_format;
	char sink_format_changed = 0;
	u32 other_pad, other_stream;
	int ret;

	dev_info(ctx->dev, "%s: pad %d, which %d\n", __func__, fmt->pad, fmt->which);

	if (fmt->pad < PAD_SINK || fmt->pad >= PAD_MAXCNT)
		return -EINVAL;

	format = &ctx->fmt[fmt->pad].format;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		stream_format = v4l2_subdev_state_get_stream_format(sd_state,
								    fmt->pad, fmt->stream);
		if (!stream_format) {
			dev_warn(ctx->dev, "Cannot find stream format for pad %d", fmt->pad);
			return -EINVAL;
		}
		*stream_format = fmt->format;
		dev_dbg(ctx->dev,
			"s_fmt pad %d code/res 0x%x/%dx%d which %d=> 0x%x/%dx%d\n",
			fmt->pad,
			fmt->format.code,
			fmt->format.width,
			fmt->format.height,
			fmt->which,
			format->code,
			format->width,
			format->height);

		/* Don't propagate the format from source pad to sink pad. */
		if (fmt->pad != PAD_SINK)
			goto set_fmt_end;

		/* Propagate the format to the corresponding source pad. */
		format = v4l2_subdev_state_get_opposite_stream_format(sd_state, fmt->pad,
								      fmt->stream);
		if (!format) {
			dev_warn(ctx->dev, "Cannot find the opposite end of pad %d\n",
				 fmt->pad);
		} else {
			*format = fmt->format;
		}
	} else {
		/* NOTE: update vcinfo once the SINK format changed */
		if (fmt->pad == PAD_SINK)
			sink_format_changed = 1;

		format->code = fmt->format.code;
		format->width = fmt->format.width;
		format->height = fmt->format.height;

		if (sink_format_changed && !ctx->is_test_model)
			mtk_seninf_get_vcinfo(ctx);

		dev_dbg(ctx->dev,
			 "s_fmt pad %d code/res 0x%x/%dx%d which %d=> 0x%x/%dx%d\n",
			 fmt->pad,
			 fmt->format.code,
			 fmt->format.width,
			 fmt->format.height,
			 fmt->which,
			 format->code,
			 format->width,
			 format->height);

		/* Don't propagate the format from source pad to sink pad. */
		if (fmt->pad != PAD_SINK)
			goto set_fmt_end;

		/* Propagate the format to the corresponding source pad. */
		ret = v4l2_subdev_routing_find_opposite_end(&sd_state->routing,
							    fmt->pad, fmt->stream,
							    &other_pad, &other_stream);
		if (ret) {
			dev_warn(ctx->dev, "Cannot find the opposite end of pad %d ret=%d\n",
				 fmt->pad, ret);
		} else {
			dev_dbg(ctx->dev, "Propagate the format from pad %d to pad %d\n",
				fmt->pad, other_pad);
			ctx->fmt[other_pad].format = fmt->format;
		}
	}

set_fmt_end:
	return 0;
}

static int mtk_seninf_get_fmt(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_format *fmt)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	struct v4l2_mbus_framefmt *format, *stream_format;

	if (fmt->pad < PAD_SINK || fmt->pad >= PAD_MAXCNT)
		return -EINVAL;

	format = &ctx->fmt[fmt->pad].format;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		stream_format = v4l2_subdev_state_get_stream_format(sd_state,
								    fmt->pad, fmt->stream);
		if (!stream_format) {
			dev_warn(ctx->dev, "Cannot find stream format for pad %d", fmt->pad);
			return -EINVAL;
		}
		fmt->format = *stream_format;
	} else {
		fmt->format.code = format->code;
		fmt->format.width = format->width;
		fmt->format.height = format->height;
		fmt->format.field = format->field;
		fmt->format.colorspace = format->colorspace;
		fmt->format.xfer_func = format->xfer_func;
		fmt->format.ycbcr_enc = format->ycbcr_enc;
		fmt->format.quantization = format->quantization;
	}

	return 0;
}

static int set_test_model(struct seninf_ctx *ctx, char enable)
{
	struct seninf_vc *vc[] = { NULL, NULL, NULL, NULL, NULL };
	int i = 0, ret = 0, vc_used = 0;
	struct seninf_mux *mux;
	int pref_idx[] = { 0, 1, 2, 3, 4 };

	if (ctx->is_test_model == 1) {
		vc[vc_used++] = mtk_seninf_get_vc_by_pad(ctx, PAD_SRC_GENERAL0);
	} else {
		dev_err(ctx->dev, "testmodel%d invalid\n", ctx->is_test_model);
		return -1;
	}

	for (; i < vc_used; ++i) {
		if (!vc[i]) {
			dev_warn(ctx->dev, "vc not found\n");
			return -1;
		}
	}

	if (enable) {
		ret = pm_runtime_resume_and_get(ctx->dev);
		if (ret < 0) {
			dev_err(ctx->dev, "failed at pm_runtime_resume_and_get\n");
			return ret;
		}

		if (ctx->core->clk[CLK_TOP_CAMTM])
			ret = clk_prepare_enable(ctx->core->clk[CLK_TOP_CAMTM]);
		if (ret)
			return ret;

		for (i = 0; i < vc_used; ++i) {
			mux = mtk_seninf_mux_get_pref(ctx,
							  pref_idx,
							  ARRAY_SIZE(pref_idx));
			if (!mux)
				return -EBUSY;
			vc[i]->mux = mux->idx;
			vc[i]->cam = ctx->pad2cam[vc[i]->out_pad];
			vc[i]->enable = 1;

			dev_info(ctx->dev,
				 "test mode mux %d, cam %d, pixel mode %d\n",
				 vc[i]->mux, vc[i]->cam, vc[i]->pixel_mode);

			g_seninf_ops->_set_test_model(ctx,
					vc[i]->mux, vc[i]->cam, vc[i]->pixel_mode);

			usleep_range(40, 60);
		}
	} else {
		g_seninf_ops->_set_idle(ctx);
		mtk_seninf_release_mux(ctx);

		if (ctx->core->clk[CLK_TOP_CAMTM])
			clk_disable_unprepare(ctx->core->clk[CLK_TOP_CAMTM]);

		pm_runtime_put_sync(ctx->dev);
	}

	ctx->streaming = enable;

	return 0;
}

static int config_hw(struct seninf_ctx *ctx)
{
	int i, intf, skip_mux_ctrl;
	int hs_pol, vs_pol, vc_sel, dt_sel, dt_en;
	struct seninf_vcinfo *vcinfo;
	struct seninf_vc *vc;
	struct seninf_mux *mux, *mux_by_grp[SENINF_VC_MAXCNT] = { 0 };

	intf = ctx->seninf_idx;
	vcinfo = &ctx->vcinfo;

	g_seninf_ops->_reset(ctx, intf);

	g_seninf_ops->_set_vc(ctx, intf, vcinfo);

	g_seninf_ops->_set_csi_mipi(ctx);

	/* should set false */
	hs_pol = 0;
	vs_pol = 0;

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];

		/* alloc mux by group */
		if (mux_by_grp[vc->group]) {
			mux = mux_by_grp[vc->group];
			skip_mux_ctrl = 1;
		} else {
			int pref_idx[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
					10, 11, 12, 13, 14, 15,	16, 17, 18, 19, 20, 21 };
			mux_by_grp[vc->group] =
				mtk_seninf_mux_get_pref(ctx,
							    pref_idx,
							    g_seninf_ops->pref_mux_num);
			mux = mux_by_grp[vc->group];
			skip_mux_ctrl = 0;
		}

		if (!mux) {
			mtk_seninf_release_mux(ctx);
			return -EBUSY;
		}

		vc->mux = mux->idx;
		vc->cam = ctx->pad2cam[vc->out_pad];

		if (!skip_mux_ctrl) {
			g_seninf_ops->_mux(ctx, vc->mux);
			g_seninf_ops->_set_mux_ctrl(ctx, vc->mux,
						    hs_pol, vs_pol,
						    MIPI_SENSOR + vc->group,
						    vc->pixel_mode);
			g_seninf_ops->_set_mux_exp(ctx, vc->mux,
						   vc->exp_hsize, vc->exp_vsize);

			g_seninf_ops->_set_top_mux_ctrl(ctx, vc->mux, intf);

			/* disable mtk_seninf_set_mux_crop length limit */
		}
		dev_info(ctx->dev, "ctx->pad2cam[%d] %d vc->out_pad %d vc->cam %d, i %d pixel_mode %d",
			 vc->out_pad, ctx->pad2cam[vc->out_pad],
			 vc->out_pad, vc->cam, i, vc->pixel_mode);

		if (vc->cam != 0xff) {
			vc_sel = vc->vc;
			dt_sel = vc->dt;
			dt_en = !!dt_sel;

			/* CMD_SENINF_FINALIZE_CAM_MUX */
			g_seninf_ops->_set_cammux_vc(ctx, vc->cam,
						     vc_sel, dt_sel, dt_en, dt_en);
			g_seninf_ops->_set_cammux_src(ctx, vc->mux, vc->cam,
						      vc->exp_hsize, vc->exp_vsize);
			g_seninf_ops->_set_cammux_chk_pixel_mode(ctx,
								 vc->cam,
								 vc->pixel_mode);
			g_seninf_ops->_cammux(ctx, vc->cam);

			dev_info(ctx->dev, "vc[%d] pad %d intf %d mux %d cam %d\n",
				 i, vc->out_pad, intf, vc->mux, vc->cam);
		} else {
			dev_info(ctx->dev,
				 "not set camtg yet, vc[%d] pad %d intf %d mux %d cam %d\n",
				 i, vc->out_pad, intf, vc->mux, vc->cam);
		}
	}
	return 0;
}

static int calc_buffered_pixel_rate(struct device *dev,
				    s64 width, s64 height,
				    s64 hblank, s64 vblank,
				    int fps_n, int fps_d, s64 *result)
{
	s64 orig_pixel_rate = *result;
	u64 buffered_pixel_rate, pclk, k;

	if (fps_d == 0 || width == 0 || hblank == 0 || ISP_CLK_LOW == 0) {
		dev_info(dev,
			 "Prevent divided by 0, fps_d= %d, w= %llu, h= %llu, ISP_CLK= %d\n",
			 fps_d, width, hblank, ISP_CLK_LOW);
		return 0;
	}

	/* calculate pclk */
	pclk = (width + hblank) * (height + vblank) * fps_n;
	do_div(pclk, fps_d);

	/* calculate buffered pixel_rate */
	buffered_pixel_rate = orig_pixel_rate * width;
	k = HW_BUF_EFFECT * orig_pixel_rate;
	do_div(k, ISP_CLK_LOW);
	do_div(buffered_pixel_rate, (width + hblank - k));
	*result = buffered_pixel_rate;

	dev_info(dev,
		 "%s: w %lld h %lld hb %lld vb %lld fps %d/%d pclk %lld->%lld orig %lld k %lld hbe %d\n",
		 __func__, width, height, hblank, vblank,
		 fps_n, fps_d, pclk, buffered_pixel_rate, orig_pixel_rate, k, HW_BUF_EFFECT);

	return 0;
}

static int get_buffered_pixel_rate(struct seninf_ctx *ctx,
				   struct v4l2_subdev *sd, int sd_pad_idx,
				   s64 *result)
{
	int ret;
	struct v4l2_ctrl *ctrl;
	struct v4l2_subdev_format fmt;
	struct v4l2_subdev_frame_interval fi;
	s64 width, height, hblank, vblank;

	fmt.pad = sd_pad_idx;
	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = v4l2_subdev_call(sd, pad, get_fmt, NULL, &fmt);
	if (ret) {
		dev_info(ctx->dev, "no get_fmt in %s\n", sd->name);
		return ret;
	}

	width = fmt.format.width;
	height = fmt.format.height;

	memset(&fi, 0, sizeof(fi));
	fi.pad = sd_pad_idx;
	ret = v4l2_subdev_call(sd, video, g_frame_interval, &fi);
	if (ret) {
		dev_info(ctx->dev, "no g_frame_interval in %s\n", sd->name);
		return ret;
	}

	ctrl = v4l2_ctrl_find(sd->ctrl_handler, V4L2_CID_HBLANK);
	if (!ctrl) {
		dev_info(ctx->dev, "no hblank in %s\n", sd->name);
		return -EINVAL;
	}

	hblank = v4l2_ctrl_g_ctrl(ctrl);

	ctrl = v4l2_ctrl_find(sd->ctrl_handler, V4L2_CID_VBLANK);
	if (!ctrl) {
		dev_info(ctx->dev, "no vblank in %s\n", sd->name);
		return -EINVAL;
	}

	vblank = v4l2_ctrl_g_ctrl(ctrl);

	/* update fps */
	ctx->fps_n = fi.interval.denominator;
	ctx->fps_d = fi.interval.numerator;

	return calc_buffered_pixel_rate(ctx->dev, width, height, hblank, vblank,
					ctx->fps_n, ctx->fps_d, result);
}

static int get_pixel_rate(struct seninf_ctx *ctx, struct v4l2_subdev *sd,
			  s64 *result)
{
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sd->ctrl_handler, V4L2_CID_PIXEL_RATE);
	if (!ctrl) {
		dev_info(ctx->dev, "no pixel rate in subdev %s\n", sd->name);
		return -EINVAL;
	}

	*result = v4l2_ctrl_g_ctrl_int64(ctrl);

	return 0;
}

static int get_mbus_config(struct seninf_ctx *ctx, struct v4l2_subdev *sd)
{
	struct v4l2_mbus_config cfg;
	int ret;

	ret = v4l2_subdev_call(sd, pad, get_mbus_config, ctx->sensor_pad_idx, &cfg);
	if (ret) {
		dev_info(ctx->dev, "no g_mbus_config in %s\n",
			 sd->entity.name);
		return -1;
	}

	ctx->is_cphy = cfg.type == V4L2_MBUS_CSI2_CPHY;

	ctx->num_data_lanes = cfg.bus.mipi_csi2.num_data_lanes;

	return 0;
}

static int get_mbus_config_by_endpoint(struct seninf_ctx *ctx)
{
	struct fwnode_handle *ep, *fwnode;
	struct v4l2_fwnode_endpoint vep = {};
	int ret = 0;

	ep = fwnode_graph_get_endpoint_by_id(dev_fwnode(ctx->dev), 0, 0, 0);

	if (!ep) {
		dev_err(ctx->dev, "Failed to get endpoint\n");
		return -EINVAL;
	}

	fwnode = fwnode_graph_get_remote_endpoint(ep);
	ret = v4l2_fwnode_endpoint_parse(ep, &vep);
	fwnode_handle_put(ep);

	if (ret) {
		dev_err(ctx->dev, "Failed to parse %pOF ret=%d\n", to_of_node(fwnode), ret);
		return -EINVAL;
	}

	if (vep.bus_type != V4L2_MBUS_CSI2_CPHY &&
	    vep.bus_type != V4L2_MBUS_CSI2_DPHY) {
		dev_dbg(ctx->dev, "CSI2 bus is neither CPHY nor DPHY, set to DPHY by default\n");
		vep.bus_type = V4L2_MBUS_CSI2_DPHY;
	}

	ctx->is_cphy = (vep.bus_type == V4L2_MBUS_CSI2_CPHY);
	ctx->num_data_lanes = vep.bus.mipi_csi2.num_data_lanes;

	dev_dbg(ctx->dev, "ctx->is_cphy=%u ctx->num_data_lanes=%d\n",
		ctx->is_cphy, ctx->num_data_lanes);

	return 0;
}

int update_isp_clk(struct seninf_ctx *ctx)
{
	int i, pixelmode;
	struct seninf_dfs *dfs = &ctx->core->dfs;
	s64 pixel_rate = -1;
	u64 dfs_freq;
	struct seninf_vc *vc;
	int ret = 0;

	if (!dfs->cnt) {
		dev_info(ctx->dev, "dfs not ready.\n");
		return ret;
	}

	vc = mtk_seninf_get_vc_by_pad(ctx, PAD_SRC_GENERAL0);
	if (!vc) {
		dev_info(ctx->dev, "failed to get vc\n");
		return -1;
	}
	dev_info(ctx->dev,
		 "%s dfs->cnt %d pixel mode %d customized_pixel_rate %lld, buffered_pixel_rate %lld mipi_pixel_rate %lld\n",
		 __func__, dfs->cnt, vc->pixel_mode, ctx->customized_pixel_rate,
		 ctx->buffered_pixel_rate, ctx->mipi_pixel_rate);

	/* Use SensorPixelrate */
	if (ctx->customized_pixel_rate) {
		pixel_rate = ctx->customized_pixel_rate;
	} else if (ctx->buffered_pixel_rate) {
		pixel_rate = ctx->buffered_pixel_rate;
	} else if (ctx->mipi_pixel_rate) {
		pixel_rate = ctx->mipi_pixel_rate;
	} else {
		dev_err(ctx->dev, "failed to get pixel_rate\n");
		return -EINVAL;
	}

	pixelmode = vc->pixel_mode;
	for (i = 0; i < dfs->cnt; i++) {
		dfs_freq = dfs->freqs[i];
		dfs_freq = dfs_freq * (100 - SENINF_CLK_MARGIN_IN_PERCENT);
		do_div(dfs_freq, 100);
		if ((dfs_freq << pixelmode) >= pixel_rate)
			break;
	}

	if (i == dfs->cnt) {
		dev_info(ctx->dev, "mux is overrun. please adjust pixelmode\n");
		return -EINVAL;
	}

	return 0;
}

static int debug_err_detect_initialize(struct seninf_ctx *ctx)
{
	struct seninf_core *core;
	struct seninf_ctx *ctx_;

	core = dev_get_drvdata(ctx->dev->parent);

	core->csi_irq_en_flag = 0;

	list_for_each_entry(ctx_, &core->list, list) {
		ctx_->data_not_enough_flag = 0;
		ctx_->err_lane_resync_flag = 0;
		ctx_->crc_err_flag = 0;
		ctx_->ecc_err_double_flag = 0;
		ctx_->ecc_err_corrected_flag = 0;
		ctx_->fifo_overrun_flag = 0;
		ctx_->size_err_flag = 0;
		ctx_->data_not_enough_cnt = 0;
		ctx_->err_lane_resync_cnt = 0;
		ctx_->crc_err_cnt = 0;
		ctx_->ecc_err_double_cnt = 0;
		ctx_->ecc_err_corrected_cnt = 0;
		ctx_->fifo_overrun_cnt = 0;
		ctx_->size_err_cnt = 0;
	}

	return 0;
}

static int seninf_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	struct mtk_mipi_cdphy_port *port_ctx;

	if (ctx->streaming == enable)
		return 0;

	if (ctx->is_test_model)
		return set_test_model(ctx, enable);

	if (!ctx->sensor_sd) {
		dev_info(ctx->dev, "no sensor\n");
		return -EFAULT;
	}

	if (enable) {
		debug_err_detect_initialize(ctx);

		ret = get_mbus_config(ctx, ctx->sensor_sd);
		if (ret < 0) {
			dev_dbg(ctx->dev, "get_mbus_config() fails, try endpoint\n");
			ret = get_mbus_config_by_endpoint(ctx);

			if (ret < 0) {
				dev_dbg(ctx->dev, "trying endpoint fails\n");
				return ret;
			}
		}

		get_pixel_rate(ctx, ctx->sensor_sd, &ctx->mipi_pixel_rate);

		ctx->buffered_pixel_rate = ctx->mipi_pixel_rate;
		get_buffered_pixel_rate(ctx, ctx->sensor_sd,
					ctx->sensor_pad_idx,
					&ctx->buffered_pixel_rate);

		ret = pm_runtime_get_sync(ctx->dev);
		if (ret < 0) {
			dev_info(ctx->dev, "%s pm_runtime_get_sync ret %d\n", __func__, ret);
			pm_runtime_put_noidle(ctx->dev);
			return ret;
		}

		update_isp_clk(ctx);

		port_ctx = phy_get_drvdata(ctx->phy);

		dev_info(ctx->dev, "phy rx: cphy_settle_delay_dt %d dphy_settle_delay_dt %d settle_delay_ck %d hs_trail_parameter %d",
			 port_ctx->cphy_settle_delay_dt,
			 port_ctx->dphy_settle_delay_dt,
			 port_ctx->settle_delay_ck,
			 port_ctx->hs_trail_parameter);

		port_ctx->customized_pixel_rate = ctx->customized_pixel_rate;
		port_ctx->mipi_pixel_rate = ctx->mipi_pixel_rate;
		port_ctx->buffered_pixel_rate = ctx->buffered_pixel_rate;

		if (port_ctx->num_data_lanes != ctx->num_data_lanes) {
			dev_err(ctx->dev, "phy rx lane numbers(%d) mismatch phy tx lane numbers(%d)\n",
				port_ctx->num_data_lanes,
				ctx->num_data_lanes);
			return ret;
		}

		phy_power_on(ctx->phy);

		ret = config_hw(ctx);
		if (ret) {
			dev_info(ctx->dev, "config_seninf_hw ret %d\n", ret);
			return ret;
		}

		ret = v4l2_subdev_call(ctx->sensor_sd, video, s_stream, 1);
		if (ret) {
			dev_info(ctx->dev, "sensor stream-on ret %d\n", ret);
			g_seninf_ops->_set_idle(ctx);
			mtk_seninf_release_mux(ctx);
			seninf_dfs_set(ctx, 0);
			g_seninf_ops->_poweroff(ctx);
			pm_runtime_put_sync(ctx->dev);
			ctx->streaming = 0;
			return  ret;
		}

		// g_seninf_ops->_debug(ctx);
	} else {
		phy_power_off(ctx->phy);
		ret = v4l2_subdev_call(ctx->sensor_sd, video, s_stream, 0);
		if (ret) {
			dev_info(ctx->dev, "sensor stream-off ret %d\n", ret);
			return ret;
		}

		g_seninf_ops->_set_idle(ctx);
		mtk_seninf_release_mux(ctx);

		g_seninf_ops->_poweroff(ctx);
		pm_runtime_put_sync(ctx->dev);
	}

	ctx->streaming = enable;
	return 0;
}

static int seninf_enable_streams(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state, u32 pad,
				 u64 streams_mask)
{
	return seninf_s_stream(sd, 1);
}

static int seninf_disable_streams(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state, u32 pad,
				  u64 streams_mask)
{
	return seninf_s_stream(sd, 0);
}

static const struct v4l2_subdev_pad_ops seninf_subdev_pad_ops = {
	.link_validate = mtk_seninf_link_validate,
	.init_cfg = mtk_seninf_init_cfg,
	.set_fmt = mtk_seninf_set_fmt,
	.get_fmt = mtk_seninf_get_fmt,
	.set_routing = seninf_set_routing,
	.enable_streams = seninf_enable_streams,
	.disable_streams = seninf_disable_streams,
};

static const struct v4l2_subdev_video_ops seninf_subdev_video_ops = {
	.s_stream = seninf_s_stream,
};

static const struct v4l2_subdev_core_ops seninf_subdev_core_ops = {
	.subscribe_event	= seninf_subscribe_event,
	.unsubscribe_event	= v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_ops seninf_subdev_ops = {
	.core	= &seninf_subdev_core_ops,
	.video	= &seninf_subdev_video_ops,
	.pad	= &seninf_subdev_pad_ops,
};

static unsigned int get_engine_id(struct seninf_ctx *ctx, const char *subdev_name)
{
	int i, engine_idx;

	for (i = 0; i < ISP_ENGINE_COUNT; i++) {
		dev_dbg(ctx->dev, "%s subdev name %s, engine name %s",
			 __func__, subdev_name, engine_names[i]);
		if (strstr(subdev_name, engine_names[i])) {
			engine_idx = i + 2;
			return engine_idx;
		}
	}

	return 255;
}

static int seninf_link_setup(struct media_entity *entity,
			     const struct media_pad *local,
			     const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd;
	struct seninf_ctx *ctx;
	unsigned int camtg;
	int ret = 0;

	sd = media_entity_to_v4l2_subdev(entity);
	if (!sd)
		return -EINVAL;
	ctx = v4l2_get_subdevdata(sd);
	if (!ctx)
		return -EINVAL;

	if (local->flags & MEDIA_PAD_FL_SOURCE) {
		if (flags & MEDIA_LNK_FL_ENABLED) {
			ctx->engine_sd =
				media_entity_to_v4l2_subdev(remote->entity);
			ctx->engine_pad_idx = remote->index;
			if (!mtk_seninf_get_vc_by_pad(ctx, local->index)) {
				dev_dbg(ctx->dev,
					 "%s enable link w/o vc_info pad idex %d\n",
					 __func__, local->index);
			}

			/*
			 * Set seninf cam mux(camtg) for seninf
			 * seninf_cam_mux2 => camsv2
			 * seninf_cam_mux3 => camsv3
			 * seninf_cam_mux4 => camsv4
			 * seninf_cam_mux5 => camsv5
			 * seninf_cam_mux6 => camsv6
			 * seninf_cam_mux7 => camsv7
			 */
			camtg = get_engine_id(ctx, ctx->engine_sd->entity.name);
			if (camtg == 255) {
				dev_err(ctx->dev, "%s get_engine_id failed\n", __func__);
				return -EINVAL;
			}
			dev_info(ctx->dev, "%s current camtg %d", __func__, camtg);

			ret = mtk_cam_seninf_set_camtg(ctx, local->index, camtg);
			if (ret) {
				dev_err(ctx->dev, "%s set camtg fail\n", __func__);
				return ret;
			}

			dev_dbg(ctx->dev, "%s link setup: %s -> %s  engine_pad_idx %d\n",
				 __func__,
				 sd->entity.name,
				 ctx->engine_sd->entity.name,
				 ctx->engine_pad_idx);
		} else {
			ctx->engine_sd = NULL;
		}
	} else {
		/* NOTE: update vcinfo once the link becomes enabled */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			ctx->sensor_sd =
				media_entity_to_v4l2_subdev(remote->entity);
			ctx->sensor_pad_idx = remote->index;
			mtk_seninf_get_vcinfo(ctx);
			dev_dbg(ctx->dev, "%s link setup: %s -> %s  sensor_pad_idx %d\n",
				 __func__,
				 sd->entity.name,
				 ctx->sensor_sd->entity.name,
				 ctx->sensor_pad_idx);
		} else {
			ctx->sensor_sd = NULL;
		}
	}

	dev_dbg(ctx->dev, "%s link setup flags: %x local_pad_idx %d remote_pad_idx %d\n",
		__func__,
		flags,
		local->index,
		remote->index);

	return 0;
}

static const struct media_entity_operations seninf_media_ops = {
	.link_setup = seninf_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static int seninf_notifier_bound(struct v4l2_async_notifier *notifier,
				 struct v4l2_subdev *sd,
				 struct v4l2_async_connection *asd)
{
	struct seninf_ctx *ctx = notifier_to_ctx(notifier);
	struct media_entity *sd_entity = &sd->entity;
	int ret;
	unsigned int i, j;

	dev_info(ctx->dev, "%s start bound %s\n", __func__, sd->entity.name);

	if (strstr(sd->entity.name, "camsv")) {
		for (i = 0; i < sd_entity->num_pads; i++) {
			if (sd_entity->pads[i].flags & MEDIA_PAD_FL_SINK)
				break;
		}

		if (i == sd_entity->num_pads) {
			dev_err(ctx->dev,
				"No sink pad in external entity\n");
			return -EINVAL;
		}

		for (j = PAD_SRC_GENERAL0; j < PAD_MAXCNT; j++) {
			ret = media_create_pad_link(&ctx->subdev.entity, j,
						    sd_entity, i, 0);
			if (ret) {
				dev_err(ctx->dev,
					"failed to create link for %s\n",
					sd->entity.name);
				return ret;
			}
		}

		ret = v4l2_device_register_subdev_nodes(ctx->subdev.v4l2_dev);
		if (ret) {
			dev_err(ctx->dev, "failed to create subdev nodes\n");
			return ret;
		}
	} else {
		for (i = 0; i < sd_entity->num_pads; i++) {
			if (sd_entity->pads[i].flags & MEDIA_PAD_FL_SOURCE)
				break;
		}

		if (i == sd_entity->num_pads) {
			dev_err(ctx->dev,
				"No source pad in external entity\n");
			return -EINVAL;
		}

		ret = media_create_pad_link(sd_entity, i,
					&ctx->subdev.entity, 0, 0);
		if (ret) {
			dev_err(ctx->dev,
				"failed to create link for %s\n",
				sd->entity.name);
			return ret;
		}

		ret = v4l2_device_register_subdev_nodes(ctx->subdev.v4l2_dev);
		if (ret) {
			dev_err(ctx->dev, "failed to create subdev nodes\n");
			return ret;
		}
	}

	dev_info(ctx->dev, "%s is bounded\n", sd->entity.name);

	return 0;
}

static void seninf_notifier_unbind(struct v4l2_async_notifier *notifier,
				   struct v4l2_subdev *sd,
				   struct v4l2_async_connection *asd)
{
	struct seninf_ctx *ctx = notifier_to_ctx(notifier);

	dev_info(ctx->dev, "%s is unbounded\n", sd->entity.name);
}

static const struct v4l2_async_notifier_operations seninf_async_ops = {
	.bound = seninf_notifier_bound,
	.unbind = seninf_notifier_unbind,
};

/* NOTE: update vcinfo once test_model switches */
static int seninf_test_pattern(struct seninf_ctx *ctx, u32 pattern)
{
	switch (pattern) {
	case 0:
		if (ctx->streaming)
			return -EBUSY;
		ctx->is_test_model = 0;
		mtk_seninf_get_vcinfo(ctx);
		dev_info(ctx->dev, "test pattern off\n");
		break;
#ifdef SENINF_DEBUG
	case 1: // seninf stream on
		dev_info(ctx->dev, "seninf test stream on\n");
		mtk_seninf_alloc_cam_mux(ctx);
		seninf_s_stream(&ctx->subdev, 1);
		break;
	case 2: // seninf stream off
		dev_info(ctx->dev, "seninf test stream off\n");
		seninf_s_stream(&ctx->subdev, 0);
		mtk_seninf_release_cam_mux(ctx);
		break;
#endif
	case 3: // development
		if (ctx->streaming)
			return -EBUSY;
		ctx->is_test_model = pattern;
		mtk_seninf_get_vcinfo_test(ctx);
		dev_info(ctx->dev, "test pattern on\n");
		break;
	default:
		break;
	}

	return 0;
}

static int mtk_seninf_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct seninf_ctx *ctx = ctrl_hdl_to_ctx(ctrl->handler);
	int ret = -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_TEST_PATTERN:
		ret = seninf_test_pattern(ctx, ctrl->val);
		break;
	default:
		ret = 0;
		dev_info(ctx->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops seninf_ctrl_ops = {
	.s_ctrl = mtk_seninf_set_ctrl,
};

static int seninf_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);

	mutex_lock(&ctx->mutex);
	ctx->open_refcnt++;

	if (ctx->open_refcnt == 1)
		dev_info(ctx->dev, "%s open_refcnt %d\n",
			 __func__, ctx->open_refcnt);

	mutex_unlock(&ctx->mutex);

	return 0;
}

static int seninf_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);

	mutex_lock(&ctx->mutex);
	ctx->open_refcnt--;

	if (!ctx->open_refcnt) {
		dev_info(ctx->dev, "%s open_refcnt %d\n",
			 __func__, ctx->open_refcnt);
		if (ctx->streaming)
			seninf_s_stream(&ctx->subdev, 0);
	}

	mutex_unlock(&ctx->mutex);

	return 0;
}

static const struct v4l2_subdev_internal_ops seninf_internal_ops = {
	.open = seninf_open,
	.close = seninf_close,
};

static const char *const seninf_test_pattern_menu[] = {
	"test pattern off",
	"seninf test stream on",
	"seninf test stream off",
};

static int seninf_initialize_controls(struct seninf_ctx *ctx)
{
	struct v4l2_ctrl_handler *handler;
	int ret;

	handler = &ctx->ctrl_handler;
	ret = v4l2_ctrl_handler_init(handler, 2);
	if (ret)
		return ret;
	v4l2_ctrl_new_std_menu_items(handler, &seninf_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(seninf_test_pattern_menu) - 1,
				     0, 0, seninf_test_pattern_menu);

	if (handler->error) {
		ret = handler->error;
		dev_info(ctx->dev, "Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	ctx->subdev.ctrl_handler = handler;
	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int seninf_parse_fwnode(struct device *dev, struct v4l2_async_notifier *notifier)
{
	struct fwnode_handle *fwnode = NULL;
	int ret = 0;

	fwnode_graph_for_each_endpoint(dev_fwnode(dev), fwnode) {
		struct v4l2_async_connection *s_asc;
		struct fwnode_handle *dev_fwnode;
		bool is_available;

		dev_fwnode = fwnode_graph_get_port_parent(fwnode);
		if (!dev_fwnode)
			continue;

		is_available = fwnode_device_is_available(dev_fwnode);
		fwnode_handle_put(dev_fwnode);
		if (!is_available)
			continue;

		s_asc = v4l2_async_nf_add_fwnode_remote(notifier,
							fwnode,
							struct v4l2_async_connection);
		if (IS_ERR(s_asc)) {
			ret = PTR_ERR(s_asc);
			break;
		}
	}

	if (fwnode)
		fwnode_handle_put(fwnode);

	return ret;
}

static int register_subdev(struct seninf_ctx *ctx, struct v4l2_device *v4l2_dev)
{
	int i, ret;
	struct v4l2_subdev *sd = &ctx->subdev;
	struct device *dev = ctx->dev;
	struct media_pad *pads = ctx->pads;
	struct v4l2_async_notifier *notifier = &ctx->notifier;

	v4l2_subdev_init(sd, &seninf_subdev_ops);

	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->flags |= V4L2_SUBDEV_FL_HAS_EVENTS;
	sd->flags |= V4L2_SUBDEV_FL_STREAMS;
	sd->dev = dev;

	if (strlen(dev->of_node->name) > 16) {
		ret = snprintf(sd->name, sizeof(sd->name), "%s-%s",
			       dev_driver_string(dev), &dev->of_node->name[16]);
		if (ret >= sizeof(sd->name))
			dev_dbg(dev, "%s: string truncated\n", __func__);
	} else {
		ret = snprintf(sd->name, sizeof(sd->name), "%s-%s",
			       dev_driver_string(dev), csi_port_names[ctx->port]);
		if (ret >= sizeof(sd->name))
			dev_dbg(dev, "%s: string truncated\n", __func__);
	}

	v4l2_set_subdevdata(sd, ctx);

	sd->entity.function = MEDIA_ENT_F_VID_IF_BRIDGE;
	sd->entity.ops = &seninf_media_ops;
	// sd->internal_ops = &seninf_internal_ops;

	pads[PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	for (i = PAD_SRC_GENERAL0; i < PAD_MAXCNT; i++)
		pads[i].flags = MEDIA_PAD_FL_SOURCE;

	for (i = 0; i < PAD_MAXCNT; i++)
		ctx->pad2cam[i] = 0xff;

	ret = media_entity_pads_init(&sd->entity, PAD_MAXCNT, pads);
	if (ret < 0) {
		dev_err(dev, "failed to init pads\n");
		return ret;
	}

	ret = v4l2_subdev_init_finalize(sd);
	if (ret < 0) {
		dev_err(dev, "failed to init finalize subdev ret=%d\n", ret);
		return ret;
	}

	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret < 0) {
		dev_err(dev, "failed to register subdev\n");
		return ret;
	}

	ctx->phy = devm_phy_get(dev, ctx->phy_name);
	if (IS_ERR(ctx->phy)) {
		dev_err(dev, "failed to get phy:%ld phy name:%s\n",
			PTR_ERR(ctx->phy), ctx->phy_name);
		ret = PTR_ERR(ctx->phy);
	}

	v4l2_async_nf_init(notifier, v4l2_dev);

	ret = seninf_parse_fwnode(dev, notifier);
	if (ret < 0)
		dev_err(dev, "no endpoint\n");

	notifier->ops = &seninf_async_ops;
	ret = v4l2_async_nf_register(notifier);
	if (ret < 0) {
		dev_err(dev, "failed to register notifier\n");
		goto err_unregister_subdev;
	}

	return 0;

err_unregister_subdev:
	v4l2_device_unregister_subdev(sd);
	v4l2_async_nf_cleanup(notifier);

	return ret;
}

static void unregister_subdev(struct seninf_ctx *ctx)
{
	struct v4l2_subdev *sd = &ctx->subdev;

	v4l2_async_nf_unregister(&ctx->notifier);
	v4l2_async_nf_cleanup(&ctx->notifier);
	v4l2_subdev_cleanup(sd);
	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
}

static int seninf_comp_bind(struct device *dev, struct device *master,
			    void *data)
{
	struct seninf_core *seninf_core_ctx = data;
	struct v4l2_device *v4l2_dev = &seninf_core_ctx->v4l2_dev;
	struct seninf_ctx *ctx = dev_get_drvdata(dev);

	return register_subdev(ctx, v4l2_dev);
}

static void seninf_comp_unbind(struct device *dev, struct device *master,
			       void *data)
{
	struct seninf_ctx *ctx = dev_get_drvdata(dev);

	unregister_subdev(ctx);
}

static const struct component_ops seninf_comp_ops = {
	.bind = seninf_comp_bind,
	.unbind = seninf_comp_unbind,
};

static int seninf_probe(struct platform_device *pdev)
{
	int ret, port;
	struct seninf_ctx *ctx;
	struct device *dev = &pdev->dev;
	struct seninf_core *core;
	const char *phy_name;

	dev_dbg(dev, "seninf | start %s\n", __func__);
	if (!dev->parent)
		return -EPROBE_DEFER;

	/* get mtk seninf_core */
	core = dev_get_drvdata(dev->parent);
	if (!core)
		return -EPROBE_DEFER;

	/* init seninf_csi ctx */
	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	dev_set_drvdata(dev, ctx);
	ctx->dev = dev;
	ctx->core = core;
	list_add(&ctx->list, &core->list);
	INIT_LIST_HEAD(&ctx->list_mux);
	INIT_LIST_HEAD(&ctx->list_cam_mux);

	ctx->open_refcnt = 0;
	ctx->streaming_counter = 0;
	mutex_init(&ctx->mutex);
	mutex_init(&ctx->streaming_protect);

	ret = get_csi_port(dev, &port);
	if (ret) {
		dev_err(dev, "get_csi_port ret %d\n", ret);
		return ret;
	}

	g_seninf_ops->_init_iomem(ctx, core->reg_if);
	g_seninf_ops->_init_port(ctx, port);
	init_fmt(ctx);

	ret = of_property_read_string(dev->of_node, "phy-names", &phy_name);
	if (ret) {
		dev_err(dev, "Failed to read phy-names property: %i\n", ret);
		return ret;
	}

	strscpy(ctx->phy_name, phy_name, PHY_NAME_LEN);

	dev_info(dev, "%s phy-name: dts %s seninf_ctx %s\n", __func__, phy_name, ctx->phy_name);

	ret = dev_read_csi_efuse(ctx);
	if (ret < 0)
		dev_info(dev, "Failed to read efuse data\n");

	ret = seninf_initialize_controls(ctx);
	if (ret) {
		dev_info(dev, "Failed to initialize controls\n");
		return ret;
	}

	ret = component_add(dev, &seninf_comp_ops);
	if (ret < 0) {
		dev_info(dev, "component_add failed\n");
		goto err_free_handler;
	}

	pm_runtime_enable(dev);

	dev_info(dev, "%s: port=%d\n", __func__, ctx->port);

	dev_info(dev, "seninf | %s success\n", __func__);

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);

	return ret;
}

static int seninf_pm_suspend(struct device *dev)
{
	int i;
	struct seninf_ctx *ctx = dev_get_drvdata(dev);
	struct seninf_core *core = ctx->core;

	mutex_lock(&core->mutex);

	core->refcnt--;
	if (core->refcnt == 0) {
		i = CLK_MAXCNT;
		do {
			i--;
			if (ctx->core->clk[i])
				clk_disable_unprepare(ctx->core->clk[i]);
		} while (i);
		seninf_core_pm_runtime_put(core);
		if (ctx->core->dfs.reg && regulator_is_enabled(ctx->core->dfs.reg))
			regulator_disable(ctx->core->dfs.reg);
	}

	mutex_unlock(&core->mutex);

	return 0;
}

static int seninf_pm_resume(struct device *dev)
{
	u32 i;
	int ret;

	struct seninf_ctx *ctx = dev_get_drvdata(dev);
	struct seninf_core *core = ctx->core;

	mutex_lock(&core->mutex);

	core->refcnt++;

	if (core->refcnt == 1) {
		ret = seninf_core_pm_runtime_get_sync(core);
		if (ret < 0) {
			dev_err(dev, "seninf_core_pm_runtime_get_sync failed\n");
			return ret;
		}

		for (i = 0; i < CLK_MAXCNT; i++) {
			if (core->clk[i])
				ret = clk_prepare_enable(core->clk[i]);
			if (ret)
				dev_dbg(dev, "%s: clk seninf%d is empty\n",
					__func__, i);
		}
		g_seninf_ops->_disable_all_mux(ctx);
		g_seninf_ops->_disable_all_cammux(ctx);
	}

	mutex_unlock(&core->mutex);

	return 0;
}

static const struct dev_pm_ops pm_ops = {
	SET_RUNTIME_PM_OPS(seninf_pm_suspend, seninf_pm_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
};

static int seninf_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct seninf_ctx *ctx = dev_get_drvdata(dev);

	if (ctx->streaming) {
		g_seninf_ops->_set_idle(ctx);
		mtk_seninf_release_mux(ctx);
	}

	pm_runtime_disable(ctx->dev);

	unregister_subdev(ctx);

	v4l2_ctrl_handler_free(&ctx->ctrl_handler);

	mutex_destroy(&ctx->mutex);

	dev_dbg(dev, "camsys | start %s\n", __func__);

	return 0;
}

static const struct of_device_id seninf_of_match[] = {
	{ .compatible = "mediatek,mt8189-seninf" },
	{},
};
MODULE_DEVICE_TABLE(of, seninf_of_match);

static int seninf_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int seninf_resume(struct platform_device *pdev)
{
	return 0;
}

struct platform_driver seninf_pdrv = {
	.probe	= seninf_probe,
	.remove	= seninf_remove,
	.suspend = seninf_suspend,
	.resume = seninf_resume,
	.driver	= {
		.name = "seninf",
		.of_match_table = seninf_of_match,
		.pm  = &pm_ops,
	},
};

static const struct of_device_id mtk_seninf_of_match[] = {
	{ .compatible = "mediatek,mt8189-seninf-core", .data = &mt8189_data },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_seninf_of_match);

static struct platform_driver mtk_seninf_driver = {
	.probe	= seninf_core_probe,
	.remove	= seninf_core_remove,
	.driver	= {
		.name = "mtk-seninf-core",
		.of_match_table = mtk_seninf_of_match,
	},
};

module_platform_driver(mtk_seninf_driver);

MODULE_DESCRIPTION("MTK sensor interface driver");
MODULE_AUTHOR("Rongdong Wang <rongdong.wang@mediatek.com>");
MODULE_LICENSE("GPL");
