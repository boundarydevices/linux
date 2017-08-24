/*
 * drivers/amlogic/media/vin/tvin/hdmirx_ext/hw_sii9135/sii9135_iface.c
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

/* implement of functions of hw_ops of hdmirx_ext_dev_s. */

#include "../hdmirx_ext_drv.h"
#include "../platform_iface.h"
#include "SiITypeDefs.h"
#include "SiIRXDefs.h"
#include "SiIGlob.h"
#include "SiIRXIO.h"
#include "SiIRX_API.h"
#include "sii9135_iface.h"

/* #include "io.h" */
/* #include "debug.h" */

#define SII9135_PULLUP_I2C_ADDR		0x31
#define SII9135_PULLDOWN_I2C_ADDR	0x30
#define SII9135_CHIP_NAME		"SII9135"

unsigned int i2c_write_byte(unsigned char address,
	unsigned char offset, unsigned char byteno,
	unsigned char *p_data, unsigned char device)
{
	int ret = 0;

	ret = __plat_i2c_write_block(address, offset, p_data, byteno);

	return (ret > 0) ? 0 : 1;
}

unsigned int i2c_read_byte(unsigned char address,
	unsigned char offset, unsigned char byteno,
	unsigned char *p_data, unsigned char device)
{
	int ret = 0;

	ret = __plat_i2c_read_block(address, offset, p_data, byteno);

	return (ret > 0) ? 0 : 1;
}


/* signal stauts */
static int __sii9135_get_cable_status(void)
{
	if (siiRX_CheckCableHPD())
		return 1;

	return 0;
}

static int __sii9135_get_signal_status(void)
{
	RXEXTPR("%s: sm_bVideo=%d\n", __func__, SiI_Ctrl.sm_bVideo);
#if 0
	if (SiI_Ctrl.sm_bVideo == SiI_RX_VS_VideoOn)
		return 1;
#endif
	if (siiIsVideoOn())
		return 1;

	return 0;
}

static int __sii9135_get_input_port(void)
{
	if (SiI_Ctrl.bVidInChannel == SiI_RX_VInCh1)
		return 1;
	else
		return 2;

	return 0;
}

static void __sii9135_set_input_port(int port)
{
	if (port == 2)
		SiI_RX_SetVideoInput(SiI_RX_VInCh2);
	else
		SiI_RX_SetVideoInput(SiI_RX_VInCh1);
	RXEXTPR("%s: %d\n", __func__, port);
}

/* signal timming */
static int __sii9135_get_video_timming(struct video_timming_s *ptimming)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();
	unsigned char tmp[4];
	unsigned char i2c_addr;
	int ret = 0;
/*
 *	CurTMDSCLK			= (int)ucTMDSClk;
 *	CurVTiming.PCLK			= (int)PCLK;
 *	CurVTiming.HActive		= HActive;
 *	CurVTiming.HTotal		= HTotal;
 *	CurVTiming.HFrontPorch		= HFP;
 *	CurVTiming.HSyncWidth		= HSYNCW;
 *	CurVTiming.HBackPorch		= HTotal - HActive - HFP - HSYNCW;
 *	CurVTiming.VActive		= VActive;
 *	CurVTiming.VTotal		= VTotal;
 *	CurVTiming.VFrontPorch		= VFP;
 *	CurVTiming.VSyncWidth		= VSYNCW;
 *	CurVTiming.VBackPorch		= VTotal - VActive - VFP - VSYNCW;
 *	CurVTiming.ScanMode		= (InterLaced)&0x01;
 *	CurVTiming.VPolarity		= (VSyncPol)&0x01;
 *	CurVTiming.HPolarity		= (HSyncPol)&0x01;
 */
/*
 *	ptimming->h_active		= CurVTiming.HActive;
 *	ptimming->h_total		= CurVTiming.HTotal;
 *	ptimming->hs_frontporch		= CurVTiming.HFrontPorch;
 *	ptimming->hs_width		= CurVTiming.HSyncWidth;
 *	ptimming->hs_backporch		= CurVTiming.HBackPorch;
 *	ptimming->v_active		= CurVTiming.VActive;
 *	ptimming->v_total		= CurVTiming.VTotal;
 *	ptimming->vs_frontporch		= CurVTiming.VFrontPorch;
 *	ptimming->vs_width		= CurVTiming.VSyncWidth;
 *	ptimming->vs_backporch		= CurVTiming.VBackPorch;
 */
	i2c_addr = hdrv->hw.i2c_addr;

	ret = i2c_read_byte(i2c_addr, 0x4e, 2, tmp, 0);
	if (ret)
		RXEXTERR("i2c read 0x4e error\n");
	ptimming->h_active = (((unsigned int)tmp[1]) << 8) + tmp[0];

	ret = i2c_read_byte(i2c_addr, 0x50, 2, tmp, 0);
	if (ret)
		RXEXTERR("i2c read 0x50 error\n");
	ptimming->v_active = (((unsigned int)tmp[1]) << 8) + tmp[0];

	ret = i2c_read_byte(i2c_addr, 0x3a, 2, tmp, 0);
	if (ret)
		RXEXTERR("i2c read 0x3a error\n");
	ptimming->h_total = (((unsigned int)tmp[1]) << 8) + tmp[0];

	ret = i2c_read_byte(i2c_addr, 0x3c, 2, tmp, 0);
	if (ret)
		RXEXTERR("i2c read 0x3c error\n");
	ptimming->v_total = (((unsigned int)tmp[1]) << 8) + tmp[0];

	ret = i2c_read_byte(i2c_addr, 0x5b, 2, tmp, 0);
	if (ret)
		RXEXTERR("i2c read 0x5b error\n");
	ptimming->hs_width = (((unsigned int)tmp[1]) << 8) + tmp[0];
	ret = i2c_read_byte(i2c_addr, 0x59, 2, tmp, 0);
	if (ret)
		RXEXTERR("i2c read 0x59 error\n");
	ptimming->hs_frontporch = (((unsigned int)tmp[1]) << 8) + tmp[0];
	ptimming->hs_backporch = ptimming->h_total - ptimming->hs_width -
			ptimming->hs_frontporch - ptimming->h_active;

	ret = i2c_read_byte(i2c_addr, 0x53, 1, tmp, 0);
	if (ret)
		RXEXTERR("i2c read 0x53 error\n");
	ptimming->vs_frontporch = tmp[0];
	ptimming->vs_width = 5;
	ret = i2c_read_byte(i2c_addr, 0x52, 1, tmp, 0);
	if (ret)
		RXEXTERR("i2c read 0x52 error\n");
	ptimming->vs_backporch = tmp[0] - ptimming->vs_width;

	ret = i2c_read_byte(i2c_addr, 0x55, 1, tmp, 0);
	if (ret)
		RXEXTERR("i2c read 0x52 error\n");
	ptimming->mode = (tmp[0] >> 2) & 1;
	ptimming->hs_pol = (tmp[0] >> 0) & 1;
	ptimming->vs_pol = (tmp[0] >> 1) & 1;
	return 0;
}

void __sii9135_dump_video_timming(void)
{
	int h_active, v_active, h_total, v_total;
	int hs_fp, hs_width, hs_bp;
	int vs_fp, vs_width, vs_bp;
	int hs_pol, vs_pol, mode;
	struct video_timming_s timming;

	if (__sii9135_get_video_timming(&timming) == -1) {
		RXEXTERR("%s: get video timming failed\n", __func__);
		return;
	}

	h_active  = timming.h_active;
	v_active  = timming.v_active;

	h_total   = timming.h_total;
	v_total   = timming.v_total;

	hs_fp     = timming.hs_frontporch;
	hs_width  = timming.hs_width;
	hs_bp     = timming.hs_backporch;

	vs_fp     = timming.vs_frontporch;
	vs_width  = timming.vs_width;
	vs_bp     = timming.vs_backporch;

	hs_pol    = timming.hs_pol;
	vs_pol    = timming.vs_pol;
	mode      = timming.mode;

	RXEXTPR("sii9135 video info:\n"
		"h * v active       = %4d x %4d\n"
		"h * v total        = %4d x %4d\n"
		"h_sync(fp, ws, bp) = %4d, %4d, %4d\n"
		"v_sync(fp, ws, bp) = %4d, %4d, %4d\n"
		"sync_pol(hs, vs)   = %s[%d], %s[%d]\n"
		"video_mode         = %s[%d]\n",
		h_active, v_active, h_total, v_total,
		hs_fp, hs_width, hs_bp,
		vs_fp, vs_width, vs_bp,
		((hs_pol) ? "positive" : "negative"), hs_pol,
		((vs_pol) ? "positive" : "negative"), vs_pol,
		((mode) ? "interlaced" : "progressive"), mode);
}

static int __sii9135_is_hdmi_mode(void)
{
	if (siiIsHDMI_Mode())
		return 1;

	return 0;
}

static int __sii9135_get_audio_sample_rate(void)
{
	BYTE bRegVal;

	bRegVal = siiReadByteHDMIRXP1(RX_FREQ_SVAL_ADDR) & 0x0F;  /*reg 102 */

	return (int)bRegVal;
}

static int __print_buf(char *buf, int length)
{
	int i = 0;

	if (buf == NULL)
		return -1;

	for (i = 0; i < length; i++)
		pr_info("[0x%.2x] = %.2X\n", i, buf[i]);

	return 0;
}

static int __sii9135_debug(char *buf)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();
	int argn = 0;
	char *p = NULL, *para = NULL, *argv[4] = {NULL, NULL, NULL, NULL};
	char str[3] = {' ', '\n', '\0'};
	unsigned long cmd = 0, dev_addr = 0, reg_start = 0, reg_end = 0;
	unsigned long length = 0, value = 0xff;
	char i2c_buf[2] = {0, 0};
	char *tmp = NULL;
	unsigned long type = 255, count = 0;
	int ret = 0;
	int i;

	p = kstrdup(buf, GFP_KERNEL);
	while (1) {
		para = strsep(&p, str);
		if (para == NULL)
			break;
		if (*para == '\0')
			continue;
		argv[argn++] = para;
	}

/*	pr_info("get para: %s %s %s %s!\n",argv[0],argv[1],argv[2],argv[3]);*/

	if (!strcmp(argv[0], "r"))
		cmd = 0;
	else if (!strcmp(argv[0], "w"))
		cmd = 1;
	else if (!strcmp(argv[0], "dump"))
		cmd = 2;
	else if (!strcmp(argv[0], "vinfo"))
		cmd = 3;
	else if (!strcmp(argv[0], "help"))
		cmd = 4;
	else if (!strncmp(argv[0], "reset", strlen("reset")))
		cmd = 5;
	else if (!strncmp(argv[0], "tt", strlen("tt")))
		cmd = 6;
	else {/* invalid command format, take it as "help" */
		pr_info("invalid cmd = %s\n", argv[0]);
		cmd = 4;
	}

	pr_info(" cmd = %ld - \"%s\"\n", cmd, argv[0]);
	if ((argn < 1) || ((cmd == 0) && (argn != 3)) ||
		((cmd == 1) && (argn != 4)) || ((cmd == 2) && (argn != 4))) {
		pr_info("invalid command format!\n");
		cmd = 4;
	}

	switch (cmd) {
	case 0: /* r device_address register_address*/
		ret = kstrtoul(argv[1], 16, &dev_addr);
		ret = kstrtoul(argv[2], 16, &reg_start);

		ret = i2c_read_byte(dev_addr, reg_start, 1, i2c_buf, 0);
		pr_info("reg read dev:0x%x[0x%x] = 0x%x, ret = %d\n",
			(unsigned int)dev_addr, (unsigned int)reg_start,
			i2c_buf[0], ret);
		break;
	case 1: /* w device_address register_address value */
		ret = kstrtoul(argv[1], 16, &dev_addr);
		ret = kstrtoul(argv[2], 16, &reg_start);
		ret = kstrtoul(argv[3], 16, &value);

		i2c_buf[0] = value;

		ret = i2c_write_byte(dev_addr, reg_start, 1, i2c_buf, 0);
		pr_info("i2c write dev: 0x%x[0x%x] = 0x%x, ret = %d\n",
			(unsigned int)dev_addr, (unsigned int)reg_start,
			(unsigned int)value, ret);
		ret = i2c_read_byte(dev_addr, reg_start, 1, i2c_buf, 0);
		pr_info("i2c read back ret = %d, value = 0x%x\n",
			ret, i2c_buf[0]);
		break;
	case 2: /* dump device_address register */
		ret = kstrtoul(argv[1], 16, &dev_addr);
		ret = kstrtoul(argv[2], 16, &reg_start);
		ret = kstrtoul(argv[3], 16, &reg_end);
		length = reg_end - reg_start + 1;

		pr_info("reg dump dev = 0x%x, start = 0x%x, end = 0x%x\n",
			(unsigned int)dev_addr, (unsigned int)reg_start,
			(unsigned int)reg_end);

		tmp = kzalloc((sizeof(char) * length), GFP_KERNEL);
		if (tmp != NULL) {
			ret = i2c_read_byte(dev_addr, reg_start,
				length, tmp, 0);
			pr_info("sii9135 dev=0x%.2x regs=0x%.2x:0x%.2x:\n",
				(unsigned int)dev_addr, (unsigned int)reg_start,
				(unsigned int)(reg_start+length-1));
			__print_buf(tmp, length);

			kfree(tmp);
		}
		break;
	case 3: /* vinfo */
		pr_info("begin dump hdmi-in video info:\n");
		__sii9135_dump_video_timming();
		break;
	case 5: /* reset, for test reset_gpio */
		ret = kstrtoul(argv[1], 10, &value);
		value = (value == 0) ? 0 : 1;

		__plat_gpio_output(&hdrv->hw.reset_gpio, value);
		pr_info("reset_gpio is set to %d\n", (unsigned int)value);
		break;
	case 6: /* tt, for loop test of hdmirx_ext i2c */
		ret = kstrtoul(argv[1], 10, &type);
		ret = kstrtoul(argv[2], 10, &count);
		pr_info("i2c stability test: type = %ld, count = %ld\n",
			type, count);

		if (type == 0) { /*write*/
			dev_addr = 0x60;
			reg_start = 0x08;
			value = 0x05;
			i2c_buf[0] = value;
			for (i = 0; i < count; i++) {
				ret = i2c_write_byte(dev_addr, reg_start,
					1, i2c_buf, 0);
				pr_info("i2c_w: 0x%x[0x%x]=0x%x, ret=%d\n",
					(unsigned int)dev_addr,
					(unsigned int)reg_start,
					(unsigned int)value, ret);
				udelay(50);
			}
		} else { /*read*/
			for (i = 0; i < count; i++) {
				dev_addr = 0x60;
				reg_start = 0x08;
				ret = i2c_read_byte(dev_addr, reg_start, 1,
					i2c_buf, 0);
				pr_info("i2c_r: 0x%x[0x%x]=0x%x, ret=%d\n",
					(unsigned int)dev_addr,
					(unsigned int)reg_start,
					i2c_buf[0], ret);
				udelay(50);
			}
		}
		break;
	case 4: /* help */
	default:
		pr_info("sii9135 debug command format:\n");
		pr_info("	read register: r device_address register_address\n");
		pr_info("	write register: w device_address register_address value\n");
		pr_info("	dump register: dump device_address register_address_start register_address_end\n");
		pr_info("	video timming: vinfo\n");
		break;
	}

	kfree(p);
	return 0;
}

static char *__sii9135_get_chip_version(void)
{
#if 0
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();
	unsigned char tmp[3];
	unsigned char i2c_addr;
	int ret;

	i2c_addr = hdrv->hw.i2c_addr;
	ret = i2c_read_byte(i2c_addr, RX_DEV_IDL_ADDR, 3, tmp, 0);
	siiReadBlockHDMIRXP0(RX_DEV_IDL_ADDR, 3, pbChipVer);
#endif
	return SII9135_CHIP_NAME;
}

static void __sii9135_hw_reset(int flag)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();
	int val_on, val_off;

	if (hdrv->hw.reset_gpio.flag == 0) {
		RXEXTPR("%s: invalid reset_gpio\n", __func__);
		return;
	}
	RXEXTPR("%s: %d\n", __func__, flag);

	if (hdrv->hw.reset_gpio.val_on != -1)
		val_on = hdrv->hw.reset_gpio.val_on;
	else
		val_on = 0;
	if (hdrv->hw.reset_gpio.val_off != -1)
		val_off = hdrv->hw.reset_gpio.val_off;
	else
		val_off = 1;
	if (flag)
		__plat_gpio_output(&hdrv->hw.reset_gpio, val_on);
	else
		__plat_gpio_output(&hdrv->hw.reset_gpio, val_off);
}

static void __sii9135_hw_en(int flag)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();
	int val_on, val_off;

	if (hdrv->hw.en_gpio.flag == 0) {
		RXEXTPR("%s: invalid en_gpio\n", __func__);
		return;
	}
	RXEXTPR("%s: %d\n", __func__, flag);

	if (hdrv->hw.en_gpio.val_on != -1)
		val_on = hdrv->hw.en_gpio.val_on;
	else
		val_on = 1;
	if (hdrv->hw.en_gpio.val_off != -1)
		val_off = hdrv->hw.en_gpio.val_off;
	else
		val_off = 0;
	if (flag)
		__plat_gpio_output(&hdrv->hw.en_gpio, val_on);
	else
		__plat_gpio_output(&hdrv->hw.en_gpio, val_off);
}

static int __sii9135_task_handler(void *data)
{
	sii9135_main();

	return 0;
}

static int __sii9135_init(void)
{
	RXEXTPR("%s\n", __func__);

	return 0;
}

static int __sii9135_enable(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();
	unsigned char i2c_addr = SII9135_PULLDOWN_I2C_ADDR;

	if (!hdrv)
		return -1;

	RXEXTPR("%s\n", __func__);

	__sii9135_hw_en(1);
	__plat_msleep(100);

	i2c_addr = hdrv->hw.i2c_addr;

	__plat_i2c_init(&(hdrv->hw), i2c_addr);

	__sii9135_hw_reset(1);
	__plat_msleep(200);
	__sii9135_hw_reset(0);
	__plat_msleep(200);

	__plat_create_thread(__sii9135_task_handler, hdrv, HDMIRX_EXT_DRV_NAME);

	return 0;
}

static void __sii9135_disable(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if (!hdrv)
		return;

	RXEXTPR("%s\n", __func__);

	__sii9135_hw_reset(1);
	__sii9135_hw_en(0);
}

void hdmirx_ext_register_hw_sii9135(struct hdmirx_ext_drv_s *hdrv)
{
	if (!hdrv)
		return;

	RXEXTPR("%s\n", __func__);
	hdrv->hw.get_cable_status      = __sii9135_get_cable_status;
	hdrv->hw.get_signal_status     = __sii9135_get_signal_status;
	hdrv->hw.get_input_port        = __sii9135_get_input_port;
	hdrv->hw.set_input_port        = __sii9135_set_input_port;
	hdrv->hw.get_video_timming     = __sii9135_get_video_timming;
	hdrv->hw.is_hdmi_mode          = __sii9135_is_hdmi_mode;
	hdrv->hw.get_audio_sample_rate = __sii9135_get_audio_sample_rate;
	hdrv->hw.debug                 = __sii9135_debug;
	hdrv->hw.get_chip_version      = __sii9135_get_chip_version;
	hdrv->hw.init                  = __sii9135_init;
	hdrv->hw.enable                = __sii9135_enable;
	hdrv->hw.disable               = __sii9135_disable;
}

