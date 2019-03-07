/*
 * drivers/amlogic/media/enhancement/amprime_sl/amprime_sl.c
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

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/registers/regs/viu_regs.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "amprime_sl.h"
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/amprime_sl/prime_sl.h>

/*======================================*/

static const struct hdr_prime_sl_func_s *p_funcs;
struct prime_sl_device_data_s prime_sl_meson_dev;
#define AMPRIME_SL_NAME               "amprime_sl"
#define AMPRIME_SL_CLASS_NAME         "amprime_sl"

struct amprime_sl_dev_s {
	dev_t                       devt;
	struct cdev                 cdev;
	dev_t                       devno;
	struct device               *dev;
	struct class                *clsp;
};
static struct amprime_sl_dev_s amprime_sl_dev;
static struct prime_t prime_sl_setting;

static uint prime_sl_enable = DISABLE;
module_param(prime_sl_enable, uint, 0664);
MODULE_PARM_DESC(prime_sl_enable, "\n prime_sl_enable\n");

static uint prime_sl_display_Brightness = 150;
module_param(prime_sl_display_Brightness, uint, 0664);
MODULE_PARM_DESC(prime_sl_display_Brightness, "\n prime_sl_display_Brightness\n");

bool is_prime_sl_enable(void)
{
	return prime_sl_enable;
}
EXPORT_SYMBOL(is_prime_sl_enable);

static int parse_para(char *p)
{
	int data;

	data = cpu_to_le32((unsigned int)(*p));
	*p += 4;
	return data;
}

static int prime_sl_parse_sei_and_meta(char *p, unsigned int size)
{
	unsigned int i;
	struct sl_hdr_metadata *sl_hdr_metadata
		= &prime_sl_setting.prime_metadata;

	if (size != sizeof(struct sl_hdr_metadata)) {
		pr_info("error metadata size\n");
		return 0;
	}

	sl_hdr_metadata->partID = parse_para(p);
	sl_hdr_metadata->majorSpecVersionID = parse_para(p);
	sl_hdr_metadata->minorSpecVersionID = parse_para(p);
	sl_hdr_metadata->payloadMode = parse_para(p);
	sl_hdr_metadata->hdrPicColourSpace = parse_para(p);
	sl_hdr_metadata->hdrDisplayColourSpace = parse_para(p);
	sl_hdr_metadata->hdrDisplayMaxLuminance = parse_para(p);
	sl_hdr_metadata->hdrDisplayMinLuminance = parse_para(p);
	sl_hdr_metadata->sdrPicColourSpace = parse_para(p);
	sl_hdr_metadata->sdrDisplayColourSpace = parse_para(p);
	sl_hdr_metadata->sdrDisplayMaxLuminance = parse_para(p);
	sl_hdr_metadata->sdrDisplayMinLuminance = parse_para(p);
	for (i = 0; i < 4; i++)
		sl_hdr_metadata->matrixCoefficient[i] = parse_para(p);
	for (i = 0; i < 2; i++)
		sl_hdr_metadata->chromaToLumaInjection[i] = parse_para(p);
	for (i = 0; i < 3; i++)
		sl_hdr_metadata->kCoefficient[i] = parse_para(p);
	if (!sl_hdr_metadata->payloadMode) {
		sl_hdr_metadata->u.variables.tmInputSignalBlackLevelOffset
			= parse_para(p);
		sl_hdr_metadata->u.variables.tmInputSignalWhiteLevelOffset
			= parse_para(p);
		sl_hdr_metadata->u.variables.shadowGain = parse_para(p);
		sl_hdr_metadata->u.variables.highlightGain = parse_para(p);
		sl_hdr_metadata->u.variables.midToneWidthAdjFactor
			= parse_para(p);
		sl_hdr_metadata->u.variables.tmOutputFineTuningNumVal
			= parse_para(p);
		for (i = 0; i < sl_hdr_metadata->
			u.variables.tmOutputFineTuningNumVal; i++) {
			sl_hdr_metadata->u.variables.tmOutputFineTuningX[i]
				= parse_para(p);
			sl_hdr_metadata->u.variables.tmOutputFineTuningX[i]
				= parse_para(p);
		}
		sl_hdr_metadata->u.variables.saturationGainNumVal
			= parse_para(p);
		for (i = 0; i < sl_hdr_metadata->
			u.variables.saturationGainNumVal; i++) {
			sl_hdr_metadata->u.variables.saturationGainX[i]
				= parse_para(p);
			sl_hdr_metadata->u.variables.saturationGainY[i]
				= parse_para(p);
		}
	} else {
		sl_hdr_metadata->u.tables.luminanceMappingNumVal
			= parse_para(p);
		for (i = 0; i < sl_hdr_metadata->
			u.tables.luminanceMappingNumVal; i++) {
			sl_hdr_metadata->u.tables.luminanceMappingX[i]
				= parse_para(p);
			sl_hdr_metadata->u.tables.luminanceMappingY[i]
				= parse_para(p);
		}
		sl_hdr_metadata->u.tables.colourCorrectionNumVal
			= parse_para(p);
		for (i = 0; i < sl_hdr_metadata->
			u.tables.colourCorrectionNumVal; i++) {
			sl_hdr_metadata->u.tables.colourCorrectionX[i]
				= parse_para(p);
			sl_hdr_metadata->u.tables.colourCorrectionY[i]
				= parse_para(p);
		}
	}
	return 0;
}

static void prime_sl_parser_metadata(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	struct prime_cfg_t *Cfg = &prime_sl_setting.Cfg;
	char *p;
	unsigned int size = 0;
	unsigned int type = 0;

	if (vf && (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS)) {
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		req.low_latency = 0;

		Cfg->width = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		Cfg->height = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;
		Cfg->bit_depth = vf->bitdepth;
		Cfg->yuv_range = (vf->signal_type >> 25) & 0x1;
		Cfg->display_Brightness = prime_sl_display_Brightness;

		vf_notify_provider_by_name("prime_sl_dec",
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);

		if (req.aux_buf && req.aux_size) {
			p = req.aux_buf;
			while (p < req.aux_buf
				+ req.aux_size - 8) {
				size = cpu_to_le32((unsigned int)(*p));
				p += 4;
				type = cpu_to_le32((unsigned int)(*p));
				p += 4;
				if (type == 0x04000000)/*need to double check*/
					prime_sl_parse_sei_and_meta(p, size);
				p += size;
			}
		}
	}
}

void prime_sl_process(struct vframe_s *vf)
{
	prime_sl_parser_metadata(vf);
	if (!p_funcs)
		p_funcs->prime_metadata_parser_process(&prime_sl_setting);
	prime_sl_set_reg(&prime_sl_setting.prime_sl);
	prime_sl_close();
}
EXPORT_SYMBOL(prime_sl_process);

static void dbg_setting(struct prime_sl_t *prime_sl)
{
	unsigned int i;

	pr_info("%s\n", __func__);
	pr_info("\t legacy_mode_en\t%d\n", prime_sl->legacy_mode_en);
	pr_info("\t clip_en\t%d\n", prime_sl->clip_en);
	pr_info("\t reg_gclk_ctrl\t%d\n", prime_sl->reg_gclk_ctrl);
	pr_info("\t gclk_ctrl\t%d\n", prime_sl->gclk_ctrl);
	pr_info("\t primesl_en\t%d\n",
		prime_sl->primesl_en);
	pr_info("\t inv_chroma_ratio\t%d\n",
		prime_sl->inv_chroma_ratio);
	pr_info("\t inv_y_ratio\t%d\n", prime_sl->inv_y_ratio);
	pr_info("\t l_headroom\t%d\n", prime_sl->l_headroom);
	pr_info("\t footroom\t%d\n", prime_sl->footroom);
	pr_info("\t c_headroom\t%d\n", prime_sl->c_headroom);
	pr_info("\t mub\t%d\n",
		prime_sl->mub);
	pr_info("\t mua\t%d\n", prime_sl->mua);

	for (i = 0; i < 7; i++)
		pr_info("\t oct[%d]\t%d\n", i, prime_sl->oct[i]);
	for (i = 0; i < 3; i++)
		pr_info("\t d_lut_threshold[%d]\t%d\n", i,
		prime_sl->d_lut_threshold[i]);
	for (i = 0; i < 4; i++)
		pr_info("\t d_lut_step[%d]\t%d\n", i, prime_sl->d_lut_step[i]);
	for (i = 0; i < 9; i++)
		pr_info("\t rgb2yuv[%d]\t%d\n", i, prime_sl->rgb2yuv[i]);
	for (i = 0; i < 65; i++)
		pr_info("\t lut_c[%d]\t%d\n", i, prime_sl->lut_c[i]);
	for (i = 0; i < 65; i++)
		pr_info("\t lut_p[%d]\t%d\n", i, prime_sl->lut_p[i]);
	for (i = 0; i < 65; i++)
		pr_info("\t lut_d[%d]\t%d\n", i, prime_sl->lut_d[i]);
}

static void dbg_config(struct prime_cfg_t *Cfg)
{
	pr_info("%s\n", __func__);

	pr_info("\t width\t%d\n", Cfg->width);
	pr_info("\t height\t%d\n", Cfg->height);
	pr_info("\t bit_depth\t%d\n", Cfg->bit_depth);
	pr_info("\t display_OETF\t%d\n", Cfg->display_OETF);
	pr_info("\t display_Brightness\t%d\n",
		Cfg->display_Brightness);
	pr_info("\t yuv_range\t%d\n", Cfg->yuv_range);
}

static void dbg_metadata(struct sl_hdr_metadata *pmetadata)
{
	pr_info("%s\n", __func__);

	pr_info("\t partID\t%d\n", pmetadata->partID);
	pr_info("\t majorSpecVersionID\t%d\n",
		pmetadata->majorSpecVersionID);
	pr_info("\t minorSpecVersionID\t%d\n",
		pmetadata->minorSpecVersionID);
	pr_info("\t payloadMode\t%d\n",
		pmetadata->payloadMode);
	pr_info("\t hdrPicColourSpace\t%d\n",
		pmetadata->hdrPicColourSpace);
	pr_info("\t hdrDisplayColourSpace\t%d\n",
		pmetadata->hdrDisplayColourSpace);
	pr_info("\t hdrDisplayMaxLuminance\t%d\n",
		pmetadata->hdrDisplayMaxLuminance);

	pr_info("\t hdrDisplayMinLuminance\t%d\n",
		pmetadata->hdrDisplayMinLuminance);
	pr_info("\t sdrPicColourSpace\t%d\n",
		pmetadata->sdrPicColourSpace);
	pr_info("\t sdrDisplayColourSpace\t%d\n",
		pmetadata->sdrDisplayColourSpace);
	pr_info("\t sdrDisplayMaxLuminance\t%d\n",
		pmetadata->sdrDisplayMaxLuminance);
	pr_info("\t sdrDisplayMinLuminance\t%d\n",
		pmetadata->sdrDisplayMinLuminance);

}

static ssize_t amprime_sl_debug_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{

	if (!buf)
		return count;

	if (!strcmp(buf, "metadata")) {
		dbg_metadata(&prime_sl_setting.prime_metadata);
		dbg_config(&prime_sl_setting.Cfg);
	} else if (!strcmp(buf, "setting"))
		dbg_setting(&prime_sl_setting.prime_sl);
	else
		pr_info("unsupport commands\n");
	return count;

}

static struct class_attribute amprime_sl_class_attrs[] = {
	__ATTR(debug, 0644, NULL, amprime_sl_debug_store),
	__ATTR_NULL
};


int register_prime_functions(const struct hdr_prime_sl_func_s *func)
{
	int ret = -1;

	if (!p_funcs && func) {
		pr_info("*** register_prime_functions. version %s ***\n",
			func->version_info);
		ret = 0;
		p_funcs = func;
		p_funcs->prime_api_init();
	}
	return ret;
}
EXPORT_SYMBOL(register_prime_functions);

int unregister_prime_functions(void)
{
	int ret = -1;

	if (p_funcs) {
		pr_info("*** unregister_prime_functions ***\n");
		p_funcs->prime_api_exit();
		p_funcs = NULL;
		ret = 0;
	}
	return ret;
}
EXPORT_SYMBOL(unregister_prime_functions);

static struct prime_sl_device_data_s prime_sl_g12 = {
	.cpu_id = _CPU_MAJOR_ID_G12,
};

static struct prime_sl_device_data_s prime_sl_tl1 = {
	.cpu_id = _CPU_MAJOR_ID_TL1,
};

static const struct of_device_id amprime_sl_match[] = {
	{
		.compatible = "amlogic, prime_sl_g12",
		.data = &prime_sl_g12,
	},
	{
		.compatible = "amlogic, prime_sl_tl1",
		.data = &prime_sl_tl1,
	},
	{},
};

static int amprime_sl_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int i = 0;
	struct amprime_sl_dev_s *devp = &amprime_sl_dev;

	pr_info("amprime_sl probe start & ver: %s\n", DRIVER_VER);
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct prime_sl_device_data_s *prime_sl_meson;
		struct device_node *of_node = pdev->dev.of_node;

		match = of_match_node(amprime_sl_match, of_node);
		if (match) {
			prime_sl_meson =
				(struct prime_sl_device_data_s *)match->data;
			if (prime_sl_meson)
				memcpy(&amprime_sl_dev, prime_sl_meson,
					sizeof(struct prime_sl_device_data_s));
			else {
				pr_err("%s data NOT match\n", __func__);
				return -ENODEV;
			}
		} else {
				pr_err("%s NOT match\n", __func__);
				return -ENODEV;
		}

	}
	pr_info("\n cpu_id=%d\n", prime_sl_meson_dev.cpu_id);
	memset(devp, 0, (sizeof(struct amprime_sl_dev_s)));
	ret = alloc_chrdev_region(&devp->devno, 0, 1, AMPRIME_SL_NAME);
	if (ret < 0)
		goto fail_alloc_region;
	devp->clsp = class_create(THIS_MODULE,
		AMPRIME_SL_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}

	for (i = 0;  amprime_sl_class_attrs[i].attr.name; i++) {
		if (class_create_file(devp->clsp,
			&amprime_sl_class_attrs[i]) < 0)
			goto fail_class_create_file;
	}
/*	cdev_init(&devp->cdev, &amprime_sl_fops);
 *	devp->cdev.owner = THIS_MODULE;
 *	ret = cdev_add(&devp->cdev, devp->devno, 1);
 *	if (ret)
 *		goto fail_add_cdev;
 *
 *	devp->dev = device_create(devp->clsp, NULL, devp->devno,
 *			NULL, AMPRIME_SL_NAME);
 *	if (IS_ERR(devp->dev)) {
 *		ret = PTR_ERR(devp->dev);
 *		goto fail_create_device;
 *	}
 */
	pr_info("%s: probe ok\n", __func__);
	return 0;
/*
 *
 *fail_create_device:
 *	pr_info("[amprime_sl.] : amprime_sl device create error.\n");
 *	cdev_del(&devp->cdev);
 *
 *fail_add_cdev:
 *	pr_info("[amprime_sl.] : amprime_sl add device error.\n");
 */
fail_class_create_file:
	pr_info("[amprime_sl.] : amprime_sl class create file error.\n");
	for (i = 0; amprime_sl_class_attrs[i].attr.name; i++)
		class_remove_file(devp->clsp,
			&amprime_sl_class_attrs[i]);
	class_destroy(devp->clsp);
fail_create_class:
	pr_info("[amdolby_vision.] : amdolby_vision class create error.\n");
	unregister_chrdev_region(devp->devno, 1);
fail_alloc_region:
	pr_info("[amprime_sl.] : amprime_sl alloc error.\n");
	pr_info("[amprime_sl.] : amprime_sl.\n");
	return ret;
}

static int __exit amprime_sl_remove(struct platform_device *pdev)
{
	struct amprime_sl_dev_s *devp = &amprime_sl_dev;

	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
	pr_info("[ amprime_sl.] :  amprime_sl.\n");
	return 0;
}

static struct platform_driver amprime_sl_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "amprime_sl",
		.of_match_table = amprime_sl_match,
	},
	.probe = amprime_sl_probe,
	.remove = __exit_p(amprime_sl_remove),
};

static int __init amprime_sl_init(void)
{
	pr_info("prime_sl module init\n");
	if (platform_driver_register(&amprime_sl_driver)) {
		pr_info("failed to register amprime_sl module\n");
		return -ENODEV;
	}
	return 0;
}

static void __exit amprime_sl_exit(void)
{
	pr_info("prime_sl module exit\n");
}

module_init(amprime_sl_init);
module_exit(amprime_sl_exit);

MODULE_DESCRIPTION("Amlogic HDR Prime SL driver");
MODULE_LICENSE("GPL");

