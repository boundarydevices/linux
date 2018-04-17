/*
 * amlogic atv demod driver
 *
 * Author: nengwen.chen <nengwen.chen@amlogic.com>
 *
 *
 * Copyright (C) 2018 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define ATVDEMOD_DEVICE_NAME    "aml_atvdemod"
#define ATVDEMOD_DRIVER_NAME    "aml_atvdemod"
#define ATVDEMOD_MODULE_NAME    "aml_atvdemod"
#define ATVDEMOD_CLASS_NAME     "aml_atvdemod"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stddef.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

#include "drivers/media/dvb-core/dvb_frontend.h"

#include "atv_demod_debug.h"
#include "atv_demod_driver.h"
#include "atv_demod_v4l2.h"
#include "atv_demod_ops.h"

#include "atvdemod_func.h"
#include "atvauddemod_func.h"


#define AMLATVDEMOD_VER "Ref.2015/09/01a"

struct aml_atvdemod_device *amlatvdemod_devp;

static ssize_t aml_atvdemod_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int n = 0;
	unsigned int ret = 0;
	char *buf_orig = NULL, *ps = NULL, *token = NULL;
	char *parm[4] = { NULL };
	unsigned int data_snr[128] = { 0 };
	unsigned int data_snr_avg = 0;
	int data_afc = 0, block_addr = 0, block_reg = 0, block_val = 0;
	int i = 0, val = 0;
	unsigned long tmp = 0;
	struct aml_atvdemod_device *aml_atvdemod_dev = NULL;

	aml_atvdemod_dev = container_of(class, struct aml_atvdemod_device, cls);

	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;

	while (1) {
		token = strsep(&ps, "\n ");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strncmp(parm[0], "init", 4)) {
		ret = atv_demod_enter_mode();
		if (ret)
			pr_info("atv init error.\n");
	} else if (!strncmp(parm[0], "audout_mode", 11)) {
		if (get_atvdemod_state() == ATVDEMOD_STATE_WORK) {
			if (is_meson_txlx_cpu()) {
				atvauddemod_set_outputmode();
				pr_info("atvauddemod_set_outputmode done ....\n");
			}
		} else {
			pr_info("atvdemod_state not work  ....\n");
		}
	} else if (!strncmp(parm[0], "signal_audmode", 14)) {
		int stereo_flag, sap_flag;

		if (get_atvdemod_state() == ATVDEMOD_STATE_WORK) {
			if (is_meson_txlx_cpu()) {
				update_btsc_mode(1, &stereo_flag, &sap_flag);
				pr_info("get signal_audmode done ....\n");
			}
		} else {
			pr_info("atvdemod_state not work  ....\n");
		}
	} else if (!strncmp(parm[0], "clk", 3)) {
		adc_set_pll_cntl(1, 0x1, NULL);
		atvdemod_clk_init();
		if (is_meson_txlx_cpu() || is_meson_txhd_cpu())
			aud_demod_clk_gate(1);
		pr_info("atvdemod_clk_init done ....\n");
	} else if (!strncmp(parm[0], "tune", 4)) {
		/* val  = simple_strtol(parm[1], NULL, 10); */
	} else if (!strncmp(parm[0], "set", 3)) {
		if (!strncmp(parm[1], "avout_gain", 10)) {
			if (kstrtoul(buf + strlen("avout_offset") + 1,
					10, &tmp) == 0)
				val = tmp;
			atv_dmd_wr_byte(0x0c, 0x01, val & 0xff);
		} else if (!strncmp(parm[1], "avout_offset", 12)) {
			if (kstrtoul(buf + strlen("avout_offset") + 1,
					10, &tmp) == 0)
				val = tmp;
			atv_dmd_wr_byte(0x0c, 0x04, val & 0xff);
		} else if (!strncmp(parm[1], "atv_gain", 8)) {
			if (kstrtoul(buf + strlen("atv_gain") + 1,
					10, &tmp) == 0)
				val = tmp;
			atv_dmd_wr_byte(0x19, 0x01, val & 0xff);
		} else if (!strncmp(parm[1], "atv_offset", 10)) {
			if (kstrtoul(buf + strlen("atv_offset") + 1,
					10, &tmp) == 0)
				val = tmp;
			atv_dmd_wr_byte(0x19, 0x04, val & 0xff);
		}
	} else if (!strncmp(parm[0], "get", 3)) {
		if (!strncmp(parm[1], "avout_gain", 10)) {
			val = atv_dmd_rd_byte(0x0c, 0x01);
			pr_dbg("avout_gain:0x%x\n", val);
		} else if (!strncmp(parm[1], "avout_offset", 12)) {
			val = atv_dmd_rd_byte(0x0c, 0x04);
			pr_dbg("avout_offset:0x%x\n", val);
		} else if (!strncmp(parm[1], "atv_gain", 8)) {
			val = atv_dmd_rd_byte(0x19, 0x01);
			pr_dbg("atv_gain:0x%x\n", val);
		} else if (!strncmp(parm[1], "atv_offset", 10)) {
			val = atv_dmd_rd_byte(0x19, 0x04);
			pr_dbg("atv_offset:0x%x\n", val);
		}
	} else if (!strncmp(parm[0], "snr_hist", 8)) {
		data_snr_avg = 0;
		for (i = 0; i < 128; i++) {
			data_snr[i] = (atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC,
					0x50) >> 8);
			usleep_range(50 * 1000, 50 * 1000 + 100);
			data_snr_avg += data_snr[i];
		}
		data_snr_avg = data_snr_avg / 128;
		pr_dbg("**********snr_hist_128avg:0x%x(%d)*********\n",
				data_snr_avg,
				data_snr_avg);
	} else if (!strncmp(parm[0], "afc_info", 8)) {
		data_afc = retrieve_vpll_carrier_afc();
		pr_dbg("afc %d Khz.\n", data_afc);
	} else if (!strncmp(parm[0], "ver_info", 8)) {
		pr_dbg("aml_atvdemod_ver %s.\n",
				AMLATVDEMOD_VER);
	} else if (!strncmp(parm[0], "audio_autodet", 13)) {
		aml_audiomode_autodet(NULL);
	} else if (!strncmp(parm[0], "overmodule_det", 14)) {
		/* unsigned long over_threshold, */
		/* int det_mode = auto_det_mode; */
		aml_atvdemod_overmodule_det();
	} else if (!strncmp(parm[0], "audio_gain_set", 14)) {
		if (kstrtoul(buf + strlen("audio_gain_set") + 1, 16, &tmp) == 0)
			val = tmp;
		aml_audio_valume_gain_set(val);
		pr_dbg("audio_gain_set : %d\n", val);
	} else if (!strncmp(parm[0], "audio_gain_get", 14)) {
		val = aml_audio_valume_gain_get();
		pr_dbg("audio_gain_get : %d\n", val);
	} else if (!strncmp(parm[0], "fix_pwm_adj", 11)) {
		if (kstrtoul(parm[1], 10, &tmp) == 0) {
			val = tmp;
			aml_fix_PWM_adjust(val);
		}
	} else if (!strncmp(parm[0], "rs", 2)) {
		if (kstrtoul(parm[1], 16, &tmp) == 0)
			block_addr = tmp;
		if (kstrtoul(parm[2], 16, &tmp) == 0)
			block_reg = tmp;
		if (block_addr < APB_BLOCK_ADDR_TOP)
			block_val = atv_dmd_rd_long(block_addr, block_reg);
		pr_dbg("rs block_addr:0x%x,block_reg:0x%x,block_val:0x%x\n",
				block_addr,
				block_reg, block_val);
	} else if (!strncmp(parm[0], "ws", 2)) {
		if (kstrtoul(parm[1], 16, &tmp) == 0)
			block_addr = tmp;
		if (kstrtoul(parm[2], 16, &tmp) == 0)
			block_reg = tmp;
		if (kstrtoul(parm[3], 16, &tmp) == 0)
			block_val = tmp;
		if (block_addr < APB_BLOCK_ADDR_TOP)
			atv_dmd_wr_long(block_addr, block_reg, block_val);
		pr_dbg("ws block_addr:0x%x,block_reg:0x%x,block_val:0x%x\n",
				block_addr,
				block_reg, block_val);
		block_val = atv_dmd_rd_long(block_addr, block_reg);
		pr_dbg("readback_val:0x%x\n", block_val);
	} else if (!strncmp(parm[0], "pin_mux", 7)) {
		aml_atvdemod_dev->pin = devm_pinctrl_get_select(
				aml_atvdemod_dev->dev,
				aml_atvdemod_dev->pin_name);
		pr_dbg("atvdemod agc pinmux name:%s\n",
				aml_atvdemod_dev->pin_name);
	} else if (!strncmp(parm[0], "snr_cur", 7)) {
		data_snr_avg = aml_atvdemod_get_snr_ex();
		pr_dbg("**********snr_cur:%d*********\n", data_snr_avg);
	} else if (!strncmp(parm[0], "pll_status", 10)) {
		int vpll_lock;

		retrieve_vpll_carrier_lock(&vpll_lock);
		if ((vpll_lock & 0x1) == 0)
			pr_dbg("visual carrier lock:locked\n");
		else
			pr_dbg("visual carrier lock:unlocked\n");
	} else if (!strncmp(parm[0], "line_lock", 9)) {
		int line_lock;

		retrieve_vpll_carrier_line_lock(&line_lock);
		if (line_lock == 0)
			pr_dbg("line lock:locked\n");
		else
			pr_dbg("line lock:unlocked\n");
	} else if (!strncmp(parm[0], "audio_power", 11)) {
		int audio_power = 0;

		retrieve_vpll_carrier_audio_power(&audio_power);
	} else if (!strncmp(parm[0], "adc_power", 9)) {
		int adc_power = 0;

		retrieve_adc_power(&adc_power);
		pr_dbg("adc_power:%d\n", adc_power);
	} else if (!strncmp(parm[0], "search_atv", 10)) {
		unsigned int freq = 0;
		struct analog_parameters params;
		struct dvb_frontend *fe = NULL;

		fe = &aml_atvdemod_dev->v4l2_fe.v4l2_ad->fe;

		if (kstrtoul(parm[1], 10, &tmp) == 0)
			freq = tmp;

		params.frequency = freq;
		params.mode = 1;
		params.audmode = 0;
		params.std = V4L2_STD_PAL_I;

		fe->ops.analog_ops.set_params(fe, &params);
	} else if (!strncmp(parm[0], "mode_set", 8)) {
		int priv_cfg = AML_ATVDEMOD_INIT;
		struct dvb_frontend *fe = NULL;

		fe = &aml_atvdemod_dev->v4l2_fe.v4l2_ad->fe;

		if (kstrtoul(parm[1], 10, &tmp) == 0)
			priv_cfg = tmp;

		fe->ops.analog_ops.set_config(fe, &priv_cfg);
	} else
		pr_dbg("invalid command\n");

	kfree(buf_orig);

	return count;
}

static ssize_t aml_atvdemod_show(struct class *cls,
		struct class_attribute *attr, char *buff)
{
	pr_dbg("\n usage:\n");
	pr_dbg("[get soft version] echo ver_info > /sys/class/amlatvdemod/atvdemod_debug\n");
	pr_dbg("[get afc value] echo afc_info > /sys/class/amlatvdemod/atvdemod_debug\n");
	pr_dbg("[reinit atvdemod] echo init > /sys/class/amlatvdemod/atvdemod_debug\n");
	pr_dbg("[get av-out-gain/av-out-offset/atv-gain/atv-offset]:\n"
				"echo get av_gain/av_offset/atv_gain/atv_offset > /sys/class/amlatvdemod/atvdemod_debug\n");
	pr_dbg("[set av-out-gain/av-out-offset/atv-gain/atv-offset]:\n"
				"echo set av_gain/av_offset/atv_gain/atv_offset val(0~255) > /sys/class/amlatvdemod/atvdemod_debug\n");
	return 0;
}

static struct class_attribute atvdemod_debug_class_attrs[] = {
	__ATTR(atvdemod_debug, 0644, aml_atvdemod_show, aml_atvdemod_store),
	__ATTR_NULL
};

static void aml_atvdemod_dt_parse(struct aml_atvdemod_device *pdev)
{
	struct device_node *node = NULL;
	struct device_node *node_i2c = NULL;
	unsigned int val = 0;
	const char *str = NULL;
	int ret = 0;

	node = pdev->dev->of_node;
	if (node) {
		ret = of_property_read_u32(node, "reg_23cf", &val);
		if (ret)
			pr_err("can't find reg_23cf.\n");
		else
			pdev->reg_23cf = val;

		ret = of_property_read_u32(node, "audio_gain_val", &val);
		if (ret)
			pr_err("can't find audio_gain_val.\n");
		else
			set_audio_gain_val(val);

		ret = of_property_read_u32(node, "video_gain_val", &val);
		if (ret)
			pr_err("can't find video_gain_val.\n");
		else
			set_video_gain_val(val);

		/* agc pin mux */
		ret = of_property_read_string(node, "pinctrl-names",
				&pdev->pin_name);
		if (!ret) {
#if 0
			amlatvdemod_devp->pin = devm_pinctrl_get_select(
				&pdev->dev, pdev->pin_name);
#endif
			pr_err("atvdemod agc pinmux name: %s\n",
					pdev->pin_name);
		}

		ret = of_property_read_u32(node, "btsc_sap_mode", &val);
		if (ret)
			pr_err("can't find btsc_sap_mode.\n");
		else
			pdev->btsc_sap_mode = val;

		ret = of_property_read_string(node, "tuner", &str);
		if (ret)
			pr_err("can't find tuner.\n");
		else {
			if (!strncmp(str, "mxl661_tuner", 12))
				pdev->tuner_id = AM_TUNER_MXL661;
			else if (!strncmp(str, "si2151_tuner", 12))
				pdev->tuner_id = AM_TUNER_SI2151;
			else if (!strncmp(str, "r840_tuner", 10))
				pdev->tuner_id = AM_TUNER_R840;
			else
				pr_err("can't find tuner: %s.\n", str);
		}

		node_i2c = of_parse_phandle(node, "tuner_i2c_ada_id", 0);
		if (node_i2c) {
			pdev->i2c_adp = of_find_i2c_adapter_by_node(node_i2c);
			of_node_put(node_i2c);

			if (!pdev->i2c_adp)
				pr_err("can't find tuner_i2c_adap.\n");
		}
#if 0
		ret = of_property_read_u32(node, "tuner_i2c_ada_id", &val);
		if (ret)
			pr_err("can't find tuner_i2c_ada_id.\n");
		else
			pdev->i2c_adapter_id = val;
#endif
		ret = of_property_read_u32(node, "tuner_i2c_addr", &val);
		if (ret)
			pr_err("can't find tuner_i2c_addr.\n");
		else
			pdev->i2c_addr = val;
	}
}

static int aml_atvdemod_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res = NULL;
	int size_io_reg = 0;
	struct aml_atvdemod_device *dev = NULL;

	dev = devm_kmalloc(&pdev->dev, sizeof(struct aml_atvdemod_device),
			GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->name = ATVDEMOD_DEVICE_NAME;
	dev->cls.name = ATVDEMOD_DEVICE_NAME;
#if 0
	dev->cls.name = devm_kmalloc(&pdev->dev, sizeof(ATVDEMOD_DEVICE_NAME),
			GFP_KERNEL);
	if (!dev->cls.name)
		return -ENOMEM;

	snprintf((char *)dev->cls.name, sizeof(ATVDEMOD_DEVICE_NAME), "%s",
			ATVDEMOD_DEVICE_NAME);
#endif

	dev->cls.class_attrs = atvdemod_debug_class_attrs;
	ret = class_register(&dev->cls);
	if (ret) {
		pr_err("class_create fail.\n");
		goto fail_create_class;
	}

	dev->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("get demod memory resource fail.\n");
		goto fail_get_resource;
	}

	size_io_reg = resource_size(res);
	dev->demod_reg_base = devm_ioremap_nocache(&pdev->dev,
			res->start, size_io_reg);
	if (!dev->demod_reg_base) {
		pr_err("demod ioremap failed.\n");
		goto fail_get_resource;
	}

	pr_info("demod start = 0x%p, size = 0x%x, demod_reg_base = 0x%p.\n",
			(void *) res->start, size_io_reg,
			dev->demod_reg_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		pr_err("no audio demod memory resource.\n");
		dev->audio_reg_base = NULL;
	} else {
		size_io_reg = resource_size(res);
		dev->audio_reg_base = devm_ioremap_nocache(
				&pdev->dev, res->start, size_io_reg);
		if (!dev->audio_reg_base) {
			pr_err("audio ioremap failed.\n");
			goto fail_get_resource;
		}

		pr_info("audio start = 0x%p, size = 0x%x, audio_reg_base = 0x%p.\n",
					(void *) res->start, size_io_reg,
					dev->audio_reg_base);
	}

	if (is_meson_txlx_cpu() || is_meson_txhd_cpu())
		dev->hiu_reg_base = ioremap(0xff63c000, 0x2000);
	else
		dev->hiu_reg_base = ioremap(0xc883c000, 0x2000);

	pr_info("hiu_reg_base = 0x%p.\n", dev->hiu_reg_base);

	if (is_meson_txlx_cpu() || is_meson_txhd_cpu())
		dev->periphs_reg_base = ioremap(0xff634000, 0x2000);
	else
		dev->periphs_reg_base = ioremap(0xc8834000, 0x2000);

	pr_info("periphs_reg_base = 0x%p.\n", dev->periphs_reg_base);

	aml_atvdemod_dt_parse(dev);

	dev->v4l2_ad.dev = dev->dev;
	dev->v4l2_ad.i2c.addr = dev->i2c_addr;
	dev->v4l2_ad.i2c.adapter = dev->i2c_adp;
	/* i2c_get_adapter(dev->i2c_adapter_id); */
	dev->v4l2_ad.tuner_id = dev->tuner_id;
	ret = v4l2_resister_frontend(&dev->v4l2_ad, &dev->v4l2_fe);
	if (ret) {
		pr_err("resister v4l2 fail.\n");
		goto fail_register_v4l2;
	}

	platform_set_drvdata(pdev, dev);

	amlatvdemod_devp = dev;

	pr_info("%s: OK.\n", __func__);

	return 0;

fail_register_v4l2:
fail_get_resource:
	class_unregister(&dev->cls);
fail_create_class:
	/*devm_kfree(&pdev->dev, dev->cls.name);*/
	devm_kfree(&pdev->dev, dev);

	pr_info("%s: fail.\n", __func__);

	return ret;
}

static int aml_atvdemod_remove(struct platform_device *pdev)
{
	struct aml_atvdemod_device *dev = platform_get_drvdata(pdev);

	if (dev == NULL)
		return -1;

	class_unregister(&dev->cls);

	v4l2_unresister_frontend(&dev->v4l2_fe);

	amlatvdemod_devp = NULL;

	/*devm_kfree(&pdev->dev, dev->cls.name);*/
	devm_kfree(&pdev->dev, dev);

	pr_info("%s: OK.\n", __func__);

	return 0;
}

static void aml_atvdemod_shutdown(struct platform_device *pdev)
{
	pr_info("%s: OK.\n", __func__);
}

static int aml_atvdemod_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct aml_atvdemod_device *dev = platform_get_drvdata(pdev);

	v4l2_frontend_suspend(&dev->v4l2_fe);

	pr_info("%s: OK.\n", __func__);

	return 0;
}

int aml_atvdemod_resume(struct platform_device *pdev)
{
	struct aml_atvdemod_device *dev = platform_get_drvdata(pdev);

	v4l2_frontend_resume(&dev->v4l2_fe);

	pr_info("%s: OK.\n", __func__);

	return 0;
}

static const struct of_device_id aml_atvdemod_dt_match[] = {
	{
		.compatible = "amlogic, aml_atvdemod",
	},
	{
	},
};

static struct platform_driver aml_atvdemod_driver = {
	.driver = {
		.name = "aml_atvdemod",
		.owner = THIS_MODULE,
		.of_match_table = aml_atvdemod_dt_match,
	},
	.probe    = aml_atvdemod_probe,
	.remove   = aml_atvdemod_remove,
	.shutdown = aml_atvdemod_shutdown,
	.suspend  = aml_atvdemod_suspend,
	.resume   = aml_atvdemod_resume,
};

static int __init aml_atvdemod_init(void)
{
	if (platform_driver_register(&aml_atvdemod_driver)) {
		pr_err("%s: failed to register driver.\n", __func__);
		return -ENODEV;
	}

	pr_info("%s: OK.\n", __func__);

	return 0;
}

static void __exit aml_atvdemod_exit(void)
{
	platform_driver_unregister(&aml_atvdemod_driver);
	pr_info("%s: OK.\n", __func__);
}

MODULE_AUTHOR("nengwen.chen <nengwen.chen@amlogic.com>");
MODULE_DESCRIPTION("aml atv demod device driver");
MODULE_LICENSE("GPL");

module_init(aml_atvdemod_init);
module_exit(aml_atvdemod_exit);
