/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hdmi_tx_main.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/ctype.h>
#include <linux/extcon.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
/* #include <mach/am_regs.h> */

/* #include <linux/amlogic/osd/osd_dev.h> */
#include "hdmi_tx_hdcp.h"

#include <linux/input.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
#include <linux/extcon.h>

#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_SND_SOC
#include <linux/amlogic/media/sound/aout_notify.h>
#endif
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_ddc.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_config.h>
#include <linux/i2c.h>
#include "hw/tvenc_conf.h"
#ifdef CONFIG_INSTABOOT
#include <linux/amlogic/instaboot/instaboot.h>
#endif

#define DEVICE_NAME "amhdmitx"
#define HDMI_TX_COUNT 32
#define HDMI_TX_POOL_NUM  6
#define HDMI_TX_RESOURCE_NUM 4
#define HDMI_TX_PWR_CTRL_NUM	6

static dev_t hdmitx_id;
static struct class *hdmitx_class;
static int set_disp_mode_auto(void);
struct vinfo_s *hdmi_get_current_vinfo(void);
static void hdmitx_get_edid(struct hdmitx_dev *hdev);
static void hdmitx_set_drm_pkt(struct master_display_info_s *data);
static void hdmitx_set_vsif_pkt(enum eotf_type type, uint8_t tunnel_mode);
static int check_fbc_special(unsigned char *edid_dat);
static int hdcp_tst_sig;
static DEFINE_MUTEX(setclk_mutex);
static DEFINE_MUTEX(getedid_mutex);
static atomic_t kref_audio_mute;
static atomic_t kref_video_mute;
static char fmt_attr[16];

#ifndef CONFIG_AMLOGIC_VOUT
/* Fake vinfo */
struct vinfo_s vinfo_1080p60hz = {
	.name = "1080p60hz",
	.mode = VMODE_1080P,
	.width = 1920,
	.height = 1080,
	.field_height = 1080,
	.aspect_ratio_num = 16,
	.aspect_ratio_den  = 9,
	.sync_duration_num = 60,
	.sync_duration_den = 1,
	.video_clk		 = 148500000,
};
struct vinfo_s *get_current_vinfo(void)
{
	return &vinfo_1080p60hz;
}
#endif

struct hdmi_config_platform_data *hdmi_pdata;

static const unsigned int hdmi_cable[] = {
	EXTCON_DISP_HDMI,
	EXTCON_NONE,
};

static struct hdmitx_dev hdmitx_device;
struct extcon_dev *hdmitx_extcon_hdmi;
struct extcon_dev *hdmitx_excton_audio;
struct extcon_dev *hdmitx_excton_power;
struct extcon_dev *hdmitx_excton_hdr;
struct extcon_dev *hdmitx_excton_rxsense;
struct extcon_dev *hdmitx_excton_hdcp;
struct extcon_dev *hdmitx_excton_setmode;

static int hdmi_init;

static inline void hdmitx_notify_hpd(int hpd)
{
	if (hpd)
		hdmitx_event_notify(HDMITX_PLUG, NULL);
	else
		hdmitx_event_notify(HDMITX_UNPLUG, NULL);
}
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static void hdmitx_early_suspend(struct early_suspend *h)
{
	const struct vinfo_s *info = hdmi_get_current_vinfo();
	struct hdmitx_dev *phdmi = (struct hdmitx_dev *)h->param;

	if (info && (strncmp(info->name, "panel", 5) == 0
		|| strncmp(info->name, "null", 4) == 0))
		return;

	phdmi->hpd_lock = 1;
	msleep(20);
	phdmi->HWOp.CntlMisc(phdmi, MISC_AVMUTE_OP, SET_AVMUTE);
	mdelay(100);
	hdmi_print(IMP, SYS "HDMITX: early suspend\n");
	phdmi->HWOp.Cntl((struct hdmitx_dev *)h->param,
		HDMITX_EARLY_SUSPEND_RESUME_CNTL, HDMITX_EARLY_SUSPEND);
	phdmi->cur_VIC = HDMI_Unknown;
	phdmi->output_blank_flag = 0;
	phdmi->HWOp.CntlDDC(phdmi, DDC_HDCP_MUX_INIT, 1);
	phdmi->HWOp.CntlDDC(phdmi, DDC_HDCP_OP, HDCP14_OFF);
	extcon_set_state_sync(hdmitx_excton_power, EXTCON_DISP_HDMI, 0);
	phdmi->HWOp.CntlConfig(&hdmitx_device, CONF_CLR_AVI_PACKET, 0);
	phdmi->HWOp.CntlConfig(&hdmitx_device, CONF_CLR_VSDB_PACKET, 0);
}

static int hdmitx_is_hdmi_vmode(char *mode_name)
{
	enum hdmi_vic vic = hdmitx_edid_vic_tab_map_vic(mode_name);

	if (vic == HDMI_Unknown)
		return 0;

	return 1;
}

static void hdmitx_late_resume(struct early_suspend *h)
{
	const struct vinfo_s *info = hdmi_get_current_vinfo();
	struct hdmitx_dev *phdmi = (struct hdmitx_dev *)h->param;

	if (info && (strncmp(info->name, "panel", 5) == 0 ||
		strncmp(info->name, "null", 4) == 0)) {
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_VIDEO_BLANK_OP, VIDEO_UNBLANK);
		return;
	} else {
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_VIDEO_BLANK_OP, VIDEO_BLANK);
	}

	if (hdmitx_is_hdmi_vmode(info->name) == 1)
		phdmi->HWOp.CntlMisc(&hdmitx_device, MISC_HPLL_FAKE, 0);

	phdmi->hpd_lock = 0;

	/* update status for hpd and switch/state */
	hdmitx_device.hpd_state = !!(hdmitx_device.HWOp.CntlMisc(&hdmitx_device,
		MISC_HPD_GPI_ST, 0));
	hdmitx_notify_hpd(hdmitx_device.hpd_state);

	/*force to get EDID after resume for Amplifer Power case*/
	if (hdmitx_device.hpd_state)
		hdmitx_get_edid(phdmi);

	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
	set_disp_mode_auto();

	extcon_set_state_sync(hdmitx_extcon_hdmi, EXTCON_DISP_HDMI,
						hdmitx_device.hpd_state);
	extcon_set_state_sync(hdmitx_excton_power, EXTCON_DISP_HDMI,
						hdmitx_device.hpd_state);

	pr_info("amhdmitx: late resume module %d\n", __LINE__);
	phdmi->HWOp.Cntl((struct hdmitx_dev *)h->param,
		HDMITX_EARLY_SUSPEND_RESUME_CNTL, HDMITX_LATE_RESUME);
	hdmi_print(INF, SYS "late resume\n");
}

/* Set avmute_set signal to HDMIRX */
static int hdmitx_reboot_notifier(struct notifier_block *nb,
	unsigned long action, void *data)
{
	struct hdmitx_dev *hdev = container_of(nb, struct hdmitx_dev, nb);

	hdev->HWOp.CntlMisc(hdev, MISC_AVMUTE_OP, SET_AVMUTE);
	mdelay(100);
	hdev->HWOp.CntlMisc(hdev, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
	hdev->HWOp.CntlMisc(hdev, MISC_HPLL_OP, HPLL_DISABLE);
	return NOTIFY_OK;
}

static struct early_suspend hdmitx_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10,
	.suspend = hdmitx_early_suspend,
	.resume = hdmitx_late_resume,
	.param = &hdmitx_device,
};
#endif

/* static struct hdmitx_info hdmi_info; */
#define INIT_FLAG_VDACOFF		0x1
	/* unplug powerdown */
#define INIT_FLAG_POWERDOWN	  0x2

#define INIT_FLAG_NOT_LOAD 0x80

int hdmi_ch = 1;		/* 1: 2ch */

static unsigned char init_flag;
#undef DISABLE_AUDIO
/* if set to 1, then HDMI will output no audio */
/* In KTV case, HDMI output Picture only, and Audio is driven by other
 * sources.
 */
unsigned char hdmi_audio_off_flag;
/*
 * 0, do not unmux hpd when off or unplug ;
 * 1, unmux hpd when unplug;
 * 2, unmux hpd when unplug  or off;
 */
static int hpdmode = 1;
/* 0xffff=disable; 0=PRBS 11; 1=PRBS 15; 2=PRBS 7; 3=PRBS 31*/
static int hdmi_prbs_mode = 0xffff;
static int hdmi_detect_when_booting = 1;
/* 1: error  2: important  3: normal  4: detailed */
static int debug_level = INF;

int get_cur_vout_index(void)
/*
 * return value: 1, vout; 2, vout2;
 */
{
	int vout_index = 1;
	return vout_index;
}

struct vinfo_s *hdmi_get_current_vinfo(void)
{
	struct vinfo_s *info;

	info = get_current_vinfo();
	return info;
}

static  int  set_disp_mode(const char *mode)
{
	int ret =  -1;
	enum hdmi_vic vic;

	vic = hdmitx_edid_get_VIC(&hdmitx_device, mode, 1);
	if (strncmp(mode, "2160p30hz", strlen("2160p30hz")) == 0)
		vic = HDMI_4k2k_30;
	else if (strncmp(mode, "2160p25hz", strlen("2160p25hz")) == 0)
		vic = HDMI_4k2k_25;
	else if (strncmp(mode, "2160p24hz", strlen("2160p24hz")) == 0)
		vic = HDMI_4k2k_24;
	else if (strncmp(mode, "smpte24hz", strlen("smpte24hz")) == 0)
		vic = HDMI_4k2k_smpte_24;
	else
		;/* nothing */

	if (strncmp(mode, "1080p60hz", strlen("1080p60hz")) == 0)
		vic = HDMI_1080p60;
	if (strncmp(mode, "1080p50hz", strlen("1080p50hz")) == 0)
		vic = HDMI_1080p50;

	if (vic != HDMI_Unknown) {
		hdmitx_device.mux_hpd_if_pin_high_flag = 1;
		if (hdmitx_device.vic_count == 0) {
			if (hdmitx_device.unplug_powerdown)
				return 0;
		}
	}
#if 0
	set_vmode_enc_hw(vic);
#endif
	hdmitx_device.cur_VIC = HDMI_Unknown;
	ret = hdmitx_set_display(&hdmitx_device, vic);
	if (ret >= 0) {
		hdmitx_device.HWOp.Cntl(&hdmitx_device,
			HDMITX_AVMUTE_CNTL, AVMUTE_CLEAR);
		hdmitx_device.cur_VIC = vic;
		hdmitx_device.audio_param_update_flag = 1;
		hdmitx_device.auth_process_timer = AUTH_PROCESS_TIME;
	}

	if (hdmitx_device.cur_VIC == HDMI_Unknown) {
		if (hpdmode == 2) {
			/* edid will be read again when hpd is muxed and
			 * it is high
			 */
			hdmitx_edid_clear(&hdmitx_device);
			hdmitx_device.mux_hpd_if_pin_high_flag = 0;
		}
		if (hdmitx_device.HWOp.Cntl) {
			hdmitx_device.HWOp.Cntl(&hdmitx_device,
				HDMITX_HWCMD_TURNOFF_HDMIHW,
				(hpdmode == 2)?1:0);
		}
	}

	return ret;
}

static void hdmitx_pre_display_init(void)
{
	hdmitx_device.cur_VIC = HDMI_Unknown;
	hdmitx_device.auth_process_timer = AUTH_PROCESS_TIME;
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_VIDEO_BLANK_OP, VIDEO_BLANK);
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_HDCP_MUX_INIT, 1);
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_HDCP_OP, HDCP14_OFF);
	/* msleep(10); */
	hdmitx_device.HWOp.CntlMisc(&hdmitx_device, MISC_TMDS_PHY_OP,
		TMDS_PHY_DISABLE);
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_CLR_AVI_PACKET, 0);
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_CLR_VSDB_PACKET, 0);
}

/* fr_tab[]
 * 1080p24hz, 24:1
 * 1080p23.976hz, 2997:125
 * 25/50/100/200hz, no change
 */
static struct frac_rate_table fr_tab[] = {
	{"24hz", 24, 1, 2997, 125},
	{"30hz", 30, 1, 2997, 100},
	{"60hz", 60, 1, 2997, 50},
	{"120hz", 120, 1, 2997, 25},
	{"240hz", 120, 1, 5994, 25},
	{NULL},
};

static void recalc_vinfo_sync_duration(struct vinfo_s *info, unsigned int frac)
{
	struct frac_rate_table *fr = &fr_tab[0];

	while (fr->hz) {
		if (strstr(info->name, fr->hz)) {
			if (frac) {
				info->sync_duration_num = fr->sync_num_dec;
				info->sync_duration_den = fr->sync_den_dec;
			} else {
				info->sync_duration_num = fr->sync_num_int;
				info->sync_duration_den = fr->sync_den_int;
			}
			break;
		}
		fr++;
	}
}

static void hdmi_physcial_size_update(struct vinfo_s *info,
		struct hdmitx_dev *hdev)
{
	unsigned int width, height;

	if (info == NULL) {
		hdmi_print(ERR, VID "cann't get valid mode\n");
		return;
	}

	width = hdev->RXCap.physcial_weight;
	height = hdev->RXCap.physcial_height;
	if ((width == 0) || (height == 0)) {
		info->screen_real_width = info->aspect_ratio_num;
		info->screen_real_height = info->aspect_ratio_den;
	} else {
		info->screen_real_width = width * 10; /* transfer mm */
		info->screen_real_height = height * 10; /* transfer mm */
	}
	pr_info("hdmitx: update physcial size: %d %d\n",
		info->screen_real_width, info->screen_real_height);
}

static int set_disp_mode_auto(void)
{
	int ret =  -1;
	struct vinfo_s *info = NULL;
	struct hdmitx_dev *hdev = &hdmitx_device;
	struct hdmi_format_para *para = NULL;
	unsigned char mode[32];
	enum hdmi_vic vic = HDMI_Unknown;
	/* vic_ready got from IP */
	enum hdmi_vic vic_ready = hdev->HWOp.GetState(
		hdev, STAT_VIDEO_VIC, 0);

	memset(mode, 0, sizeof(mode));

	/* get current vinfo */
	info = hdmi_get_current_vinfo();
	hdmi_print(IMP, VID "get current mode: %s\n",
		info ? info->name : "null");
	if (info == NULL)
		return -1;

	info->fresh_tx_hdr_pkt = hdmitx_set_drm_pkt;
	info->fresh_tx_vsif_pkt = hdmitx_set_vsif_pkt;
	info->dv_info = &hdev->RXCap.dv_info;
	if (!((strncmp(info->name, "480cvbs", 7) == 0) ||
		(strncmp(info->name, "576cvbs", 7) == 0) ||
		(strncmp(info->name, "null", 4) == 0))) {
		info->hdr_info.hdr_support = (hdev->RXCap.hdr_sup_eotf_sdr << 0)
				| (hdev->RXCap.hdr_sup_eotf_hdr << 1)
				| (hdev->RXCap.hdr_sup_eotf_smpte_st_2084 << 2);
		info->hdr_info.lumi_max = hdev->RXCap.hdr_lum_max;
		info->hdr_info.lumi_avg = hdev->RXCap.hdr_lum_avg;
		info->hdr_info.lumi_min = hdev->RXCap.hdr_lum_min;
		pr_info("hdmitx: update rx hdr info %x\n",
			info->hdr_info.hdr_support);
	}
	hdmi_physcial_size_update(info, hdev);

	/* If info->name equals to cvbs, then set mode to I mode to hdmi
	 */
	if ((strncmp(info->name, "480cvbs", 7) == 0) ||
		(strncmp(info->name, "576cvbs", 7) == 0) ||
		(strncmp(info->name, "ntsc_m", 6) == 0) ||
		(strncmp(info->name, "pal_m", 5) == 0) ||
		(strncmp(info->name, "pal_n", 5) == 0) ||
		(strncmp(info->name, "panel", 5) == 0) ||
		(strncmp(info->name, "null", 4) == 0)) {
		hdmi_print(ERR, VID "%s not valid hdmi mode\n", info->name);
		hdev->HWOp.CntlConfig(hdev, CONF_CLR_AVI_PACKET, 0);
		hdev->HWOp.CntlConfig(hdev, CONF_CLR_VSDB_PACKET, 0);
		hdev->HWOp.CntlMisc(hdev, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
		hdev->HWOp.CntlConfig(hdev, CONF_VIDEO_BLANK_OP, VIDEO_UNBLANK);
		hdev->para = para = hdmi_get_fmt_name("invalid", fmt_attr);
		return -1;
	}
	memcpy(mode, info->name, strlen(info->name));
	if (strstr(mode, "fp")) {
		int i = 0;

		for (; mode[i]; i++) {
			if ((mode[i] == 'f') && (mode[i + 1] == 'p')) {
				/* skip "f", 1080fp60hz -> 1080p60hz */
				do {
					mode[i] = mode[i + 1];
					i++;
				} while (mode[i]);
				break;
			}
		}
	}

	/* In the file hdmi_common/hdmi_parameters.c,
	 * the data array all_fmt_paras[] treat 2160p60hz and 2160p60hz420
	 * as two different modes, such Scrambler
	 * So if node "attr" contains 420, need append 420 to mode.
	 */
	if (strstr(fmt_attr, "420")) {
		if (!strstr(mode, "420"))
			strncat(mode, "420", 3);
	}
	extcon_set_state_sync(hdmitx_excton_setmode, EXTCON_DISP_HDMI, 1);
	para = hdmi_get_fmt_name(mode, fmt_attr);
	hdev->para = para;
	/* msleep(500); */
	vic = hdmitx_edid_get_VIC(hdev, mode, 1);
	if (strncmp(info->name, "2160p30hz", strlen("2160p30hz")) == 0) {
		vic = HDMI_4k2k_30;
	} else if (strncmp(info->name, "2160p25hz",
		strlen("2160p25hz")) == 0) {
		vic = HDMI_4k2k_25;
	} else if (strncmp(info->name, "2160p24hz",
		strlen("2160p24hz")) == 0) {
		vic = HDMI_4k2k_24;
	} else if (strncmp(info->name, "smpte24hz",
		strlen("smpte24hz")) == 0)
		vic = HDMI_4k2k_smpte_24;
	else {
	/* nothing */
	}
	if ((vic_ready != HDMI_Unknown) && (vic_ready == vic)) {
		hdmi_print(IMP, SYS "[%s] ALREADY init VIC = %d\n",
			__func__, vic);
		if (hdev->RXCap.IEEEOUI == 0) {
			/* DVI case judgement. In uboot, directly output HDMI
			 * mode
			 */
			hdev->HWOp.CntlConfig(hdev, CONF_HDMI_DVI_MODE,
				DVI_MODE);
			hdmi_print(IMP, SYS "change to DVI mode\n");
		} else if ((hdev->RXCap.IEEEOUI == 0xc03) &&
		(hdev->HWOp.CntlConfig(hdev, CONF_GET_HDMI_DVI_MODE, 0)
			== DVI_MODE)) {
			hdev->HWOp.CntlConfig(hdev, CONF_HDMI_DVI_MODE,
				HDMI_MODE);
			hdmi_print(IMP, SYS "change to HDMI mode\n");
		}
		hdev->cur_VIC = vic;
		hdev->output_blank_flag = 1;
		hdev->ready = 1;
		extcon_set_state_sync(hdmitx_excton_setmode,
			EXTCON_DISP_HDMI, 0);
		return 1;
	}
	hdmitx_pre_display_init();

	hdev->cur_VIC = HDMI_Unknown;
/* if vic is HDMI_Unknown, hdmitx_set_display will disable HDMI */
	ret = hdmitx_set_display(hdev, vic);
	pr_info("%s %d %d\n", info->name, info->sync_duration_num,
		info->sync_duration_den);
	recalc_vinfo_sync_duration(info, hdev->frac_rate_policy);
	pr_info("%s %d %d\n", info->name, info->sync_duration_num,
		info->sync_duration_den);
	if (ret >= 0) {
		hdev->HWOp.Cntl(hdev, HDMITX_AVMUTE_CNTL, AVMUTE_CLEAR);
		hdev->cur_VIC = vic;
		hdev->audio_param_update_flag = 1;
		hdev->auth_process_timer = AUTH_PROCESS_TIME;
	}
	if (hdev->cur_VIC == HDMI_Unknown) {
		if (hpdmode == 2) {
			/* edid will be read again when hpd is muxed
			 * and it is high
			 */
			hdmitx_edid_clear(hdev);
			hdev->mux_hpd_if_pin_high_flag = 0;
		}
		/* If current display is NOT panel, needn't TURNOFF_HDMIHW */
		if (strncmp(mode, "panel", 5) == 0) {
			hdev->HWOp.Cntl(hdev, HDMITX_HWCMD_TURNOFF_HDMIHW,
				(hpdmode == 2)?1:0);
		}
	}
	hdmitx_set_audio(hdev, &(hdev->cur_audio_param), hdmi_ch);
	hdev->output_blank_flag = 1;
	hdev->ready = 1;
	extcon_set_state_sync(hdmitx_excton_setmode, EXTCON_DISP_HDMI, 0);
	return ret;
}

/*disp_mode attr*/
static ssize_t show_disp_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "VIC:%d\n",
		hdmitx_device.cur_VIC);
	return pos;
}

static ssize_t store_disp_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	set_disp_mode(buf);
	return count;
}

static ssize_t show_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%s\n\r", fmt_attr);
	return pos;
}

static ssize_t store_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	memcpy(fmt_attr, buf, sizeof(fmt_attr));
	return count;
}

/*aud_mode attr*/
static ssize_t show_aud_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t store_aud_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* set_disp_mode(buf); */
	struct hdmitx_audpara *audio_param =
		&(hdmitx_device.cur_audio_param);
	if (strncmp(buf, "32k", 3) == 0)
		audio_param->sample_rate = FS_32K;
	else if (strncmp(buf, "44.1k", 5) == 0)
		audio_param->sample_rate = FS_44K1;
	else if (strncmp(buf, "48k", 3) == 0)
		audio_param->sample_rate = FS_48K;
	else {
		hdmitx_device.force_audio_flag = 0;
		return count;
	}
	audio_param->type = CT_PCM;
	audio_param->channel_num = CC_2CH;
	audio_param->sample_size = SS_16BITS;

	hdmitx_device.audio_param_update_flag = 1;
	hdmitx_device.force_audio_flag = 1;

	return count;
}

/*edid attr*/
static ssize_t show_edid(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return hdmitx_edid_dump(&hdmitx_device, buf, PAGE_SIZE);
}

static int dump_edid_data(unsigned int type, char *path)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	char line[128] = {0};
	mm_segment_t old_fs = get_fs();
	unsigned int i = 0, j = 0, k = 0, size = 0, block_cnt = 0;

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR|O_CREAT, 0666);
	if (IS_ERR(filp)) {
		pr_info("[%s] failed to open/create file: |%s|\n",
			__func__, path);
		goto PROCESS_END;
	}

	block_cnt = hdmitx_device.EDID_buf[0x7e] + 1;
	if (type == 1) {
		/* dump as bin file*/
		size = vfs_write(filp, hdmitx_device.EDID_buf,
							block_cnt*128, &pos);
	} else if (type == 2) {
		/* dump as txt file*/

		for (i = 0; i < block_cnt; i++) {
			for (j = 0; j < 8; j++) {
				for (k = 0; k < 16; k++) {
					snprintf((char *)&line[k*6], 7,
					"0x%02x, ",
					hdmitx_device.EDID_buf[i*128+j*16+k]);
				}
				line[16*6-1] = '\n';
				line[16*6] = 0x0;
				pos = (i*8+j)*16*6;
				size += vfs_write(filp, line, 16*6, &pos);
			}
		}
	}

	pr_info("[%s] write %d bytes to file %s\n", __func__, size, path);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);

PROCESS_END:
	set_fs(old_fs);
	return 0;
}

unsigned int use_loaded_edid;
static int load_edid_data(unsigned int type, char *path)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();

	struct kstat stat;
	unsigned int length = 0, max_len = EDID_MAX_BLOCK * 128;
	char *buf = NULL;

	set_fs(KERNEL_DS);

	filp = filp_open(path, O_RDONLY, 0444);
	if (IS_ERR(filp)) {
		pr_info("[%s] failed to open file: |%s|\n", __func__, path);
		goto PROCESS_END;
	}

	vfs_stat(path, &stat);

	length = (stat.size > max_len)?max_len:stat.size;

	buf = kmalloc(length, GFP_KERNEL);
	if (buf == NULL)
		goto PROCESS_END;

	vfs_read(filp, buf, length, &pos);

	memcpy(hdmitx_device.EDID_buf, buf, length);

	kfree(buf);
	filp_close(filp, NULL);

	pr_info("[%s] %d bytes loaded from file %s\n", __func__, length, path);

	hdmitx_edid_clear(&hdmitx_device);
	hdmitx_edid_parse(&hdmitx_device);
	pr_info("[%s] new edid loaded!\n", __func__);

PROCESS_END:
	set_fs(old_fs);
	return 0;
}

static ssize_t store_edid(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int argn = 0;
	char *p = NULL, *para = NULL, *argv[8] = {NULL};
	unsigned int path_length = 0;

	p = kstrdup(buf, GFP_KERNEL);
	if (p == NULL)
		return count;

	do {
		para = strsep(&p, " ");
		if (para != NULL) {
			argv[argn] = para;
			argn++;
			if (argn > 7)
				break;
		}
	} while (para != NULL);

	if (buf[0] == 'h') {
		int i;

		hdmi_print(INF, EDID "EDID hash value:\n");
		for (i = 0; i < 20; i++)
			pr_info("%02x", hdmitx_device.EDID_hash[i]);
		pr_info("\n");
	}
	if (buf[0] == 'd') {
		int ii, jj;
		unsigned long block_idx;
		int ret;

		ret = kstrtoul(buf+1, 16, &block_idx);
		if (block_idx < EDID_MAX_BLOCK) {
			for (ii = 0; ii < 8; ii++) {
				for (jj = 0; jj < 16; jj++) {
					hdmi_print(INF, "%02x ",
			hdmitx_device.EDID_buf[block_idx*128+ii*16+jj]);
				}
				hdmi_print(INF, "\n");
			}
		hdmi_print(INF, "\n");
	}
	}
	if (buf[0] == 'e') {
		int ii, jj;
		unsigned long block_idx;
		int ret;

		ret = kstrtoul(buf+1, 16, &block_idx);
		if (block_idx < EDID_MAX_BLOCK) {
			for (ii = 0; ii < 8; ii++) {
				for (jj = 0; jj < 16; jj++) {
					hdmi_print(INF, "%02x ",
		hdmitx_device.EDID_buf1[block_idx*128+ii*16+jj]);
				}
				hdmi_print(INF, "\n");
			}
			hdmi_print(INF, "\n");
		}
	}

	if (!strncmp(argv[0], "save", strlen("save"))) {
		unsigned int type = 0;

		if (argn != 3) {
			pr_info("[%s] cmd format: save bin/txt edid_file_path\n",
						__func__);
			goto PROCESS_END;
		}
		if (!strncmp(argv[1], "bin", strlen("bin")))
			type = 1;
		else if (!strncmp(argv[1], "txt", strlen("txt")))
			type = 2;

		if ((type == 1) || (type == 2)) {
			/* clean '\n' from file path*/
			path_length = strlen(argv[2]);
			if (argv[2][path_length-1] == '\n')
				argv[2][path_length-1] = 0x0;

			dump_edid_data(type, argv[2]);
		}
	} else if (!strncmp(argv[0], "load", strlen("load"))) {
		if (argn != 2) {
			pr_info("[%s] cmd format: load edid_file_path\n",
						__func__);
			goto PROCESS_END;
		}

		/* clean '\n' from file path*/
		path_length = strlen(argv[1]);
		if (argv[1][path_length-1] == '\n')
			argv[1][path_length-1] = 0x0;
		load_edid_data(0, argv[1]);
	}

PROCESS_END:
	kfree(p);
	return 16;
}

/* rawedid attr */
static ssize_t show_rawedid(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int pos = 0;
	int i;
	struct hdmitx_dev *hdev = &hdmitx_device;
	int num;

	/* prevent null prt */
	if (!hdev->edid_ptr)
		hdev->edid_ptr = hdev->EDID_buf;

	if (hdev->edid_ptr[0x7e] < 4)
		num = (hdev->edid_ptr[0x7e]+1)*0x80;
	else
		num = 0x100;

	for (i = 0; i < num; i++)
		pos += snprintf(buf+pos, PAGE_SIZE, "%02x", hdev->edid_ptr[i]);

	pos += snprintf(buf+pos, PAGE_SIZE, "\n");

	return pos;
}

/*
 * edid_parsing attr
 * If RX edid data are all correct, HEAD(00 ff ff ff ff ff ff 00), checksum,
 * version, etc), then return "ok". Otherwise, "ng"
 * Actually, in some old televisions, EDID is stored in EEPROM.
 * some bits in EEPROM may reverse with time.
 * But it does not affect  edid_parsing.
 * Therefore, we consider the RX edid data are all correct, return "OK"
 */
static ssize_t show_edid_parsing(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "ok\n");
	return pos;
}

/*
 * sink_type attr
 * sink, or repeater
 */
static ssize_t show_sink_type(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = &hdmitx_device;

	if (!hdev->hpd_state) {
		pos += snprintf(buf+pos, PAGE_SIZE, "none\n");
		return pos;
	}

	if (hdev->hdmi_info.vsdb_phy_addr.b)
		pos += snprintf(buf+pos, PAGE_SIZE, "repeater\n");
	else
		pos += snprintf(buf+pos, PAGE_SIZE, "sink\n");

	return pos;
}

/*
 * hdcp_repeater attr
 * For hdcp 22, hdcp_tx22 will write to store_hdcp_repeater
 * For hdcp 14, directly get bcaps bit
 */
static ssize_t show_hdcp_repeater(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = &hdmitx_device;

	if (hdev->hdcp_mode == 1)
		hdev->hdcp_bcaps_repeater = hdev->HWOp.CntlDDC(hdev,
			DDC_HDCP14_GET_BCAPS_RP, 0);

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n", hdev->hdcp_bcaps_repeater);

	return pos;
}

static ssize_t store_hdcp_repeater(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = &hdmitx_device;

	if (hdev->hdcp_mode == 2)
		hdev->hdcp_bcaps_repeater = (buf[0] == '1');

	return count;
}

/*
 * hdcp22_type attr
 */
static bool hdcp22_type;
static ssize_t show_hdcp22_type(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n", hdcp22_type);

	return pos;
}

static ssize_t store_hdcp22_type(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] == '0')
		hdcp22_type = 0;
	if (buf[0] == '1')
		hdcp22_type = 1;

	return count;
}

static ssize_t show_hdcp22_base(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "0x%x\n", get_hdcp22_base());

	return pos;
}

void hdmitx_audio_mute_op(unsigned int flag)
{
	hdmitx_device.tx_aud_cfg = flag;
	if (flag == 0)
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
	else
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AUDIO_MUTE_OP, AUDIO_UNMUTE);
}
EXPORT_SYMBOL(hdmitx_audio_mute_op);

void hdmitx_video_mute_op(unsigned int flag)
{
	if (flag == 0)
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_VIDEO_MUTE_OP, VIDEO_MUTE);
	else
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_VIDEO_MUTE_OP, VIDEO_UNMUTE);
}
EXPORT_SYMBOL(hdmitx_video_mute_op);

static void hdr_work_func(struct work_struct *work)
{
	struct hdmitx_dev *hdev =
		container_of(work, struct hdmitx_dev, work_hdr);

	unsigned char DRM_HB[3] = {0x87, 0x1, 26};
	unsigned char DRM_DB[26] = {0x0};

	hdev->HWOp.SetPacket(HDMI_PACKET_DRM, DRM_DB, DRM_HB);
	hdev->HWOp.CntlConfig(hdev, CONF_AVI_BT2020, CLR_AVI_BT2020);
	msleep(1500);/*delay 1.5s*/

	if (hdev->hdr_transfer_feature == T_BT709 &&
		hdev->hdr_color_feature == C_BT709)
		hdev->HWOp.SetPacket(HDMI_PACKET_DRM, NULL, NULL);
	/* switch_set_state(&hdmi_hdr, hdev->hdr_src_feature); */
}

#define GET_LOW8BIT(a)	((a) & 0xff)
#define GET_HIGH8BIT(a)	(((a) >> 8) & 0xff)
static void hdmitx_set_drm_pkt(struct master_display_info_s *data)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	unsigned char DRM_HB[3] = {0x87, 0x1, 26};
	unsigned char DRM_DB[26] = {0x0};

	/*
	 *hdr_color_feature: bit 23-16: color_primaries
	 *	1:bt709  0x9:bt2020
	 *hdr_transfer_feature: bit 15-8: transfer_characteristic
	 *	1:bt709 0xe:bt2020-10 0x10:smpte-st-2084 0x12:hlg(todo)
	 */
	if (data) {
		hdev->hdr_transfer_feature = (data->features >> 8) & 0xff;
		hdev->hdr_color_feature = (data->features >> 16) & 0xff;
	}
	if ((!data) || (!(hdev->RXCap.hdr_sup_eotf_smpte_st_2084) &&
		!(hdev->RXCap.hdr_sup_eotf_hdr) &&
		!(hdev->RXCap.hdr_sup_eotf_sdr) &&
		!(hdev->RXCap.hdr_sup_eotf_hlg))) {
		DRM_HB[1] = 0;
		DRM_HB[2] = 0;
		hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM, NULL, NULL);
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device, CONF_AVI_BT2020,
			CLR_AVI_BT2020);
		return;
	}

	/*SDR*/
	if (hdev->hdr_transfer_feature == T_BT709 &&
		hdev->hdr_color_feature == C_BT709) {
		schedule_work(&hdev->work_hdr);
		return;
	}

	DRM_DB[1] = 0x0;
	DRM_DB[2] = GET_LOW8BIT(data->primaries[0][0]);
	DRM_DB[3] = GET_HIGH8BIT(data->primaries[0][0]);
	DRM_DB[4] = GET_LOW8BIT(data->primaries[0][1]);
	DRM_DB[5] = GET_HIGH8BIT(data->primaries[0][1]);
	DRM_DB[6] = GET_LOW8BIT(data->primaries[1][0]);
	DRM_DB[7] = GET_HIGH8BIT(data->primaries[1][0]);
	DRM_DB[8] = GET_LOW8BIT(data->primaries[1][1]);
	DRM_DB[9] = GET_HIGH8BIT(data->primaries[1][1]);
	DRM_DB[10] = GET_LOW8BIT(data->primaries[2][0]);
	DRM_DB[11] = GET_HIGH8BIT(data->primaries[2][0]);
	DRM_DB[12] = GET_LOW8BIT(data->primaries[2][1]);
	DRM_DB[13] = GET_HIGH8BIT(data->primaries[2][1]);
	DRM_DB[14] = GET_LOW8BIT(data->white_point[0]);
	DRM_DB[15] = GET_HIGH8BIT(data->white_point[0]);
	DRM_DB[16] = GET_LOW8BIT(data->white_point[1]);
	DRM_DB[17] = GET_HIGH8BIT(data->white_point[1]);
	DRM_DB[18] = GET_LOW8BIT(data->luminance[0]);
	DRM_DB[19] = GET_HIGH8BIT(data->luminance[0]);
	DRM_DB[20] = GET_LOW8BIT(data->luminance[1]);
	DRM_DB[21] = GET_HIGH8BIT(data->luminance[1]);
	DRM_DB[22] = GET_LOW8BIT(data->max_content);
	DRM_DB[23] = GET_HIGH8BIT(data->max_content);
	DRM_DB[24] = GET_LOW8BIT(data->max_frame_average);
	DRM_DB[25] = GET_HIGH8BIT(data->max_frame_average);

	/* bt2020 + gamma transfer */
	if (hdev->hdr_transfer_feature == T_BT709 &&
		hdev->hdr_color_feature == C_BT2020) {
		if (hdev->sdr_hdr_feature == 0) {
			hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
				NULL, NULL);
			hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
				CONF_AVI_BT2020, SET_AVI_BT2020);
		} else if (hdev->sdr_hdr_feature == 1) {
			memset(DRM_DB, 0, sizeof(DRM_DB));
			hdev->HWOp.SetPacket(HDMI_PACKET_DRM,
				DRM_DB, DRM_HB);
			hdev->HWOp.CntlConfig(&hdmitx_device,
				CONF_AVI_BT2020, SET_AVI_BT2020);
		} else {
			DRM_DB[0] = 0x02; /* SMPTE ST 2084 */
			hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
				DRM_DB, DRM_HB);
			hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
				CONF_AVI_BT2020, SET_AVI_BT2020);
		}
		return;
	}

	/* SMPTE ST 2084 and (BT2020 or NON_STANDARD) */
	if (hdev->RXCap.hdr_sup_eotf_smpte_st_2084) {
		if (hdev->hdr_transfer_feature == T_SMPTE_ST_2084 &&
			hdev->hdr_color_feature == C_BT2020) {
			DRM_DB[0] = 0x02; /* SMPTE ST 2084 */
			hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
				DRM_DB, DRM_HB);
			hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
				CONF_AVI_BT2020, SET_AVI_BT2020);
			return;
		} else if (hdev->hdr_transfer_feature == T_SMPTE_ST_2084 &&
			hdev->hdr_color_feature != C_BT2020) {
			DRM_DB[0] = 0x02; /* no standard SMPTE ST 2084 */
			hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
				DRM_DB, DRM_HB);
			hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
				CONF_AVI_BT2020, CLR_AVI_BT2020);
			return;
		}
	}

	/*HLG and BT2020*/
	if (hdev->RXCap.hdr_sup_eotf_hlg) {
		if (hdev->hdr_color_feature == C_BT2020 &&
			(hdev->hdr_transfer_feature == T_BT2020_10 ||
			hdev->hdr_transfer_feature == T_HLG)) {
			DRM_DB[0] = 0x03;/* HLG is 0x03 */
			hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
				DRM_DB, DRM_HB);
			hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
				CONF_AVI_BT2020, SET_AVI_BT2020);
			return;
		}
	}

	/*other case*/
	hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM, NULL, NULL);
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device, CONF_AVI_BT2020,
		CLR_AVI_BT2020);
	return;
}

static void hdmitx_set_vsif_pkt(enum eotf_type type, uint8_t tunnel_mode)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	unsigned char VEN_HB[3] = {0x81, 0x01};
	unsigned char VEN_DB[24] = {0x00};
	unsigned char len = 0;
	unsigned int vic = hdev->cur_VIC;
	unsigned int hdmi_vic_4k_flag = 0;

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_GXL) {
		pr_info("hdmitx: not support DolbyVision\n");
		return;
	}

	if ((vic == HDMI_3840x2160p30_16x9) ||
	    (vic == HDMI_3840x2160p25_16x9) ||
	    (vic == HDMI_3840x2160p24_16x9) ||
	    (vic == HDMI_4096x2160p24_256x135))
		hdmi_vic_4k_flag = 1;

	switch (type) {
	case EOTF_T_DOLBYVISION:
		len = 0x18;
		break;
	case EOTF_T_HDR10:
		len = 0x05;
		break;
	case EOTF_T_SDR:
		len = 0x05;
		break;
	case EOTF_T_NULL:
	default:
		len = 0x05;
		break;
	}

	VEN_HB[2] = len;
	VEN_DB[0] = 0x03;
	VEN_DB[1] = 0x0c;
	VEN_DB[2] = 0x00;
	VEN_DB[3] = 0x00;

	if (hdmi_vic_4k_flag) {
		VEN_DB[3] = 0x20;
		if (vic == HDMI_3840x2160p30_16x9)
			VEN_DB[4] = 0x1;
		else if (vic == HDMI_3840x2160p25_16x9)
			VEN_DB[4] = 0x2;
		else if (vic == HDMI_3840x2160p24_16x9)
			VEN_DB[4] = 0x3;
		else if (vic == HDMI_4096x2160p24_256x135)
			VEN_DB[4] = 0x4;
		else
			VEN_DB[4] = 0x0;
	}

	if (type == EOTF_T_DOLBYVISION) {
		hdev->HWOp.SetPacket(HDMI_PACKET_VEND, VEN_DB, VEN_HB);
		if (tunnel_mode == 1) {
			hdev->HWOp.CntlConfig(hdev, CONF_AVI_RGBYCC_INDIC,
				COLORSPACE_RGB444);
			hdev->HWOp.CntlConfig(hdev, CONF_AVI_Q01,
				RGB_RANGE_FUL);
		} else {
			hdev->HWOp.CntlConfig(hdev, CONF_AVI_RGBYCC_INDIC,
				COLORSPACE_YUV422);
			hdev->HWOp.CntlConfig(hdev, CONF_AVI_YQ01,
				YCC_RANGE_FUL);
		}
	} else {
		if (hdmi_vic_4k_flag)
			hdev->HWOp.SetPacket(HDMI_PACKET_VEND, VEN_DB, VEN_HB);
		else
			hdev->HWOp.SetPacket(HDMI_PACKET_VEND, NULL, NULL);
		hdev->HWOp.CntlConfig(hdev, CONF_AVI_RGBYCC_INDIC,
			hdev->para->cs);
		hdev->HWOp.CntlConfig(hdev, CONF_AVI_Q01, RGB_RANGE_LIM);
		hdev->HWOp.CntlConfig(hdev, CONF_AVI_YQ01, YCC_RANGE_LIM);
	}
}

/*config attr*/
static ssize_t show_config(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	unsigned char *conf;
	struct hdmitx_dev *hdev = &hdmitx_device;

	pos += snprintf(buf+pos, PAGE_SIZE, "cur_VIC: %d\n", hdev->cur_VIC);
	if (hdev->cur_video_param)
		pos += snprintf(buf+pos, PAGE_SIZE,
			"cur_video_param->VIC=%d\n",
			hdev->cur_video_param->VIC);
	if (hdev->para) {
		pos += snprintf(buf+pos, PAGE_SIZE, "cd = %d\n",
			hdev->para->cd);
		pos += snprintf(buf+pos, PAGE_SIZE, "cs = %d\n",
			hdev->para->cs);
	}

	switch (hdev->tx_aud_cfg) {
	case 0:
		conf = "off";
		break;
	case 1:
		conf = "on";
		break;
	case 2:
		conf = "auto";
		break;
	default:
		conf = "none";
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "audio config: %s\n", conf);

	if (hdev->flag_3dfp)
		conf = "FramePacking";
	else if (hdev->flag_3dss)
		conf = "SidebySide";
	else if (hdev->flag_3dtb)
		conf = "TopButtom";
	else
		conf = "off";
	pos += snprintf(buf+pos, PAGE_SIZE, "3D config: %s\n", conf);
	return pos;
}

static ssize_t store_config(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	pr_info("hdmitx: config: %s\n", buf);
	if (strncmp(buf, "force", 5) == 0)
		hdmitx_device.disp_switch_config = DISP_SWITCH_FORCE;
	else if (strncmp(buf, "edid", 4) == 0)
		hdmitx_device.disp_switch_config = DISP_SWITCH_EDID;
	else if (strncmp(buf, "unplug_powerdown", 16) == 0) {
		if (buf[16] == '0')
			hdmitx_device.unplug_powerdown = 0;
		else
			hdmitx_device.unplug_powerdown = 1;
	} else if (strncmp(buf, "3d", 2) == 0) {
		/* Second, set 3D parameters */
		if (strncmp(buf+2, "tb", 2) == 0) {
			hdmitx_device.flag_3dtb = 1;
			hdmitx_device.flag_3dss = 0;
			hdmitx_device.flag_3dfp = 0;
			hdmi_set_3d(&hdmitx_device, T3D_TAB, 0);
		} else if ((strncmp(buf+2, "lr", 2) == 0) ||
			(strncmp(buf+2, "ss", 2) == 0)) {
			unsigned long sub_sample_mode = 0;

			hdmitx_device.flag_3dtb = 0;
			hdmitx_device.flag_3dss = 1;
			hdmitx_device.flag_3dfp = 0;
			if (buf[2])
				ret = kstrtoul(buf+2, 10,
					&sub_sample_mode);
			/* side by side */
			hdmi_set_3d(&hdmitx_device, T3D_SBS_HALF,
				sub_sample_mode);
		} else if (strncmp(buf+2, "fp", 2) == 0) {
			hdmitx_device.flag_3dtb = 0;
			hdmitx_device.flag_3dss = 0;
			hdmitx_device.flag_3dfp = 1;
			hdmi_set_3d(&hdmitx_device, T3D_FRAME_PACKING, 0);
		} else if (strncmp(buf+2, "off", 3) == 0) {
			hdmitx_device.flag_3dfp = 0;
			hdmitx_device.flag_3dtb = 0;
			hdmitx_device.flag_3dss = 0;
			hdmi_set_3d(&hdmitx_device, T3D_DISABLE, 0);
		}
	} else if (strncmp(buf, "audio_", 6) == 0) {
		if (strncmp(buf+6, "off", 3) == 0) {
			hdmitx_audio_mute_op(0);
			hdmi_print(IMP, AUD "configure off\n");
		} else if (strncmp(buf+6, "on", 2) == 0) {
			hdmitx_audio_mute_op(1);
			hdmi_print(IMP, AUD "configure on\n");
		} else if (strncmp(buf+6, "auto", 4) == 0) {
			/* auto mode. if sink doesn't support current
			 * audio format, then no audio output
			 */
			hdmitx_device.tx_aud_cfg = 2;
			hdmi_print(IMP, AUD "configure auto\n");
		} else
			hdmi_print(ERR, AUD "configure error\n");
	} else if (strncmp(buf, "drm", 3) == 0) {
		unsigned char DRM_HB[3] = {0x87, 0x1, 26};
		unsigned char DRM_DB[26] = {
			0x00, 0x00, 0xc2, 0x33, 0xc4, 0x86, 0x4c, 0x1d,
			0xb8, 0x0b, 0xd0, 0x84, 0x80, 0x3e, 0x13, 0x3d,
			0x42, 0x40, 0x4c, 0x04, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00,
		};

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB)
			hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
				DRM_DB, DRM_HB);
	} else if (strncmp(buf, "vsif", 4) == 0)
		hdmitx_set_vsif_pkt(buf[4] - '0', buf[5] == '1');

	return 16;
}

static ssize_t show_aud_mute(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n",
		atomic_read(&kref_audio_mute));
	return pos;
}

static ssize_t store_aud_mute(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] == '1') {
		atomic_inc(&kref_audio_mute);
		if (atomic_read(&kref_audio_mute) == 1)
			hdmitx_audio_mute_op(0);
	}
	if (buf[0] == '0') {
		if (!(atomic_sub_and_test(0, &kref_audio_mute))) {
			atomic_dec(&kref_audio_mute);
			if (atomic_sub_and_test(0, &kref_audio_mute))
				hdmitx_audio_mute_op(1);
		}
	}

	return count;
}

static ssize_t show_vid_mute(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n",
		atomic_read(&kref_video_mute));
	return pos;
}

static ssize_t store_vid_mute(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] == '1') {
		atomic_inc(&kref_video_mute);
		if (atomic_read(&kref_video_mute) == 1)
			hdmitx_video_mute_op(0);
	}
	if (buf[0] == '0') {
		if (!(atomic_sub_and_test(0, &kref_video_mute))) {
			atomic_dec(&kref_video_mute);
			if (atomic_sub_and_test(0, &kref_video_mute))
				hdmitx_video_mute_op(1);
		}
	}

	return count;
}

static ssize_t store_debug(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	hdmitx_device.HWOp.DebugFun(&hdmitx_device, buf);
	return count;
}

/* support format lists */
const char *disp_mode_t[] = {
#if 0
	"480i60hz",
	"576i50hz",
#endif
	"480p60hz",
	"576p50hz",
	"720p60hz",
	"1080i60hz",
	"1080p60hz",
	"720p50hz",
	"1080i50hz",
	"1080p30hz",
	"1080p50hz",
	"1080p25hz",
	"1080p24hz",
	"2160p30hz",
	"2160p25hz",
	"2160p24hz",
	"smpte24hz",
	"smpte25hz",
	"smpte30hz",
	"smpte50hz",
	"smpte60hz",
	"smpte50hz420",
	"smpte60hz420",
	"2160p50hz",
	"2160p60hz",
	"2160p50hz420",
	"2160p60hz420",
	NULL
};

/**/
static ssize_t show_disp_cap(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, pos = 0;
	const char *native_disp_mode =
		hdmitx_edid_get_native_VIC(&hdmitx_device);
	enum hdmi_vic vic;

	if (hdmitx_device.tv_no_edid) {
		pos += snprintf(buf+pos, PAGE_SIZE, "null edid\n");
	} else {
		for (i = 0; disp_mode_t[i]; i++) {
			vic = hdmitx_edid_get_VIC(&hdmitx_device,
				disp_mode_t[i], 0);
		if (vic != HDMI_Unknown) {
			pos += snprintf(buf+pos, PAGE_SIZE, "%s",
				disp_mode_t[i]);
			if (native_disp_mode && (strcmp(
				native_disp_mode,
				disp_mode_t[i]) == 0)) {
				pos += snprintf(buf+pos, PAGE_SIZE,
					"*\n");
			} else
			pos += snprintf(buf+pos, PAGE_SIZE, "\n");
		}
		}
	}
	return pos;
}

static ssize_t show_preferred_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct rx_cap *pRXCap = &hdmitx_device.RXCap;

	pos += snprintf(buf+pos, PAGE_SIZE, "%s\n",
		hdmitx_edid_vic_to_string(pRXCap->preferred_mode));

	return pos;
}

/**/
static int local_support_3dfp(enum hdmi_vic vic)
{
	switch (vic) {
	case HDMI_1280x720p50_16x9:
	case HDMI_1280x720p60_16x9:
	case HDMI_1920x1080p24_16x9:
	case HDMI_1920x1080p25_16x9:
	case HDMI_1920x1080p30_16x9:
	case HDMI_1920x1080p50_16x9:
	case HDMI_1920x1080p60_16x9:
		return 1;
	default:
		return 0;
	}
}
static ssize_t show_disp_cap_3d(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, pos = 0;
	int j = 0;
	enum hdmi_vic vic;

	pos += snprintf(buf+pos, PAGE_SIZE, "3D support lists:\n");
	for (i = 0; disp_mode_t[i]; i++) {
		/* 3D is not supported under 4k modes */
		if (strstr(disp_mode_t[i], "2160p") ||
			strstr(disp_mode_t[i], "smpte"))
			continue;
		vic = hdmitx_edid_get_VIC(&hdmitx_device,
			disp_mode_t[i], 0);
		for (j = 0; j < hdmitx_device.RXCap.VIC_count; j++) {
			if (vic == hdmitx_device.RXCap.VIC[j])
				break;
		}
		pos += snprintf(buf+pos, PAGE_SIZE, "\n%s ",
			disp_mode_t[i]);
		if (local_support_3dfp(vic)
			&& (hdmitx_device.RXCap.support_3d_format[
			hdmitx_device.RXCap.VIC[j]].frame_packing == 1)) {
			pos += snprintf(buf+pos, PAGE_SIZE,
				"FramePacking ");
		}
		if (hdmitx_device.RXCap.support_3d_format[
			hdmitx_device.RXCap.VIC[j]].top_and_bottom == 1) {
			pos += snprintf(buf+pos, PAGE_SIZE,
				"TopBottom ");
		}
		if (hdmitx_device.RXCap.support_3d_format[
			hdmitx_device.RXCap.VIC[j]].side_by_side == 1) {
			pos += snprintf(buf+pos, PAGE_SIZE,
				"SidebySide ");
		}
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "\n");

	return pos;
}

/**/
static ssize_t show_aud_cap(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, pos = 0, j;
	static const char * const aud_coding_type[] =  {
		"ReferToStreamHeader", "PCM", "AC-3", "MPEG1", "MP3",
		"MPEG2", "AAC", "DTS", "ATRAC",	"OneBitAudio",
		"Dobly_Digital+", "DTS-HD", "MAT", "DST", "WMA_Pro",
		"Reserved", NULL};
	static const char * const aud_sampling_frequency[] = {
		"ReferToStreamHeader", "32", "44.1", "48", "88.2", "96",
		"176.4", "192", NULL};
	static const char * const aud_sample_size[] = {"ReferToStreamHeader",
		"16", "20", "24", NULL};

	struct rx_cap *pRXCap = &(hdmitx_device.RXCap);

	pos += snprintf(buf + pos, PAGE_SIZE,
		"CodingType MaxChannels SamplingFreq SampleSize\n");
	for (i = 0; i < pRXCap->AUD_count; i++) {
		pos += snprintf(buf + pos, PAGE_SIZE, "%s, %d ch, ",
			aud_coding_type[pRXCap->RxAudioCap[i].
				audio_format_code],
			pRXCap->RxAudioCap[i].channel_num_max + 1);
		for (j = 0; j < 7; j++) {
			if (pRXCap->RxAudioCap[i].freq_cc & (1 << j))
				pos += snprintf(buf + pos, PAGE_SIZE, "%s/",
					aud_sampling_frequency[j+1]);
		}
		pos += snprintf(buf + pos - 1, PAGE_SIZE, " kHz, ");
		for (j = 0; j < 3; j++) {
			if (pRXCap->RxAudioCap[i].cc3 & (1 << j))
				pos += snprintf(buf + pos, PAGE_SIZE, "%s/",
					aud_sample_size[j+1]);
		}
		pos += snprintf(buf + pos - 1, PAGE_SIZE, " bit\n");
	}

	return pos;
}

/**/
static ssize_t show_dc_cap(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	enum hdmi_vic vic = HDMI_Unknown;
	int pos = 0;
	struct rx_cap *pRXCap = &(hdmitx_device.RXCap);

#if 0
	if (pRXCap->dc_48bit_420)
		pos += snprintf(buf + pos, PAGE_SIZE, "420,16bit\n");
	if (pRXCap->dc_36bit_420)
		pos += snprintf(buf + pos, PAGE_SIZE, "420,12bit\n");
#endif
	if (pRXCap->dc_30bit_420) {
		pos += snprintf(buf + pos, PAGE_SIZE, "420,10bit\n");
		pos += snprintf(buf + pos, PAGE_SIZE, "420,8bit\n");
	} else {
		vic = hdmitx_edid_get_VIC(&hdmitx_device, "2160p60hz420", 0);
		if (vic != HDMI_Unknown) {
			pos += snprintf(buf + pos, PAGE_SIZE, "420,8bit\n");
			goto next444;
		}
		vic = hdmitx_edid_get_VIC(&hdmitx_device, "2160p50hz420", 0);
		if (vic != HDMI_Unknown) {
			pos += snprintf(buf + pos, PAGE_SIZE, "420,8bit\n");
			goto next444;
		}
	}
next444:
	if (pRXCap->dc_y444) {
#if 0
		if (pRXCap->dc_36bit)
			pos += snprintf(buf + pos, PAGE_SIZE, "444,12bit\n");
		if (pRXCap->dc_36bit)
			pos += snprintf(buf + pos, PAGE_SIZE, "422,12bit\n");
#endif
		if (pRXCap->dc_30bit) {
			pos += snprintf(buf + pos, PAGE_SIZE, "444,10bit\n");
			pos += snprintf(buf + pos, PAGE_SIZE, "444,8bit\n");
		}
#if 0
		if (pRXCap->dc_48bit)
			pos += snprintf(buf + pos, PAGE_SIZE, "444,16bit\n");
#endif
		if (pRXCap->dc_30bit) {
			pos += snprintf(buf + pos, PAGE_SIZE, "422,10bit\n");
			pos += snprintf(buf + pos, PAGE_SIZE, "422,8bit\n");
			goto nextrgb;
		}
	} else {
		if (pRXCap->native_Mode & (1 << 5))
			pos += snprintf(buf + pos, PAGE_SIZE, "444,8bit\n");
		if (pRXCap->native_Mode & (1 << 4))
			pos += snprintf(buf + pos, PAGE_SIZE, "422,8bit\n");
	}
nextrgb:
#if 0
	if (pRXCap->dc_48bit)
		pos += snprintf(buf + pos, PAGE_SIZE, "rgb,16bit\n");
	if (pRXCap->dc_36bit)
		pos += snprintf(buf + pos, PAGE_SIZE, "rgb,12bit\n");
#endif
	if (pRXCap->dc_30bit)
		pos += snprintf(buf + pos, PAGE_SIZE, "rgb,10bit\n");
	pos += snprintf(buf + pos, PAGE_SIZE, "rgb,8bit\n");
	return pos;
}

static bool valid_mode;
static char cvalid_mode[32];

static bool pre_process_str(char *name)
{
	int i;
	unsigned int flag = 0;
	char *color_format[4] = {"444", "422", "420", "rgb"};

	for (i = 0 ; i < 4 ; i++) {
		if (strstr(name, color_format[i]) != NULL)
			flag++;
	}
	if (flag >= 2)
		return 0;
	else
		return 1;
}

static ssize_t show_valid_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmi_format_para *para = NULL;

	if (cvalid_mode[0]) {
		valid_mode = pre_process_str(cvalid_mode);
		if (valid_mode == 0) {
			pos += snprintf(buf + pos, PAGE_SIZE, "%d\n\r",
				valid_mode);
			return pos;
		}
		para = hdmi_get_fmt_name(cvalid_mode, cvalid_mode);
	}
	if (para) {
		pr_info("sname = %s\n", para->sname);
		pr_info("char_clk = %d\n", para->tmds_clk);
		pr_info("cd = %d\n", para->cd);
		pr_info("cs = %d\n", para->cs);
	}

	valid_mode = hdmitx_edid_check_valid_mode(&hdmitx_device, para);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n\r", valid_mode);

	return pos;
}

static ssize_t store_valid_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	memset(cvalid_mode, 0, sizeof(cvalid_mode));
	memcpy(cvalid_mode, buf, sizeof(cvalid_mode));
	cvalid_mode[31] = '\0';
	return count;
}


/**/
static ssize_t show_hdr_cap(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct rx_cap *pRXCap = &(hdmitx_device.RXCap);

	pos += snprintf(buf + pos, PAGE_SIZE, "Supported EOTF:\n");
	pos += snprintf(buf + pos, PAGE_SIZE, "    Traditional SDR: %d\n",
		pRXCap->hdr_sup_eotf_sdr);
	pos += snprintf(buf + pos, PAGE_SIZE, "    Traditional HDR: %d\n",
		pRXCap->hdr_sup_eotf_hdr);
	pos += snprintf(buf + pos, PAGE_SIZE, "    SMPTE ST 2084: %d\n",
		pRXCap->hdr_sup_eotf_smpte_st_2084);
	pos += snprintf(buf + pos, PAGE_SIZE, "    Hybrif Log-Gamma: %d\n",
		pRXCap->hdr_sup_eotf_hlg);
	pos += snprintf(buf + pos, PAGE_SIZE, "Supported SMD type1: %d\n",
		pRXCap->hdr_sup_SMD_type1);
	pos += snprintf(buf + pos, PAGE_SIZE, "Luminance Data\n");
	pos += snprintf(buf + pos, PAGE_SIZE, "    Max: %d\n",
		pRXCap->hdr_lum_max);
	pos += snprintf(buf + pos, PAGE_SIZE, "    Avg: %d\n",
		pRXCap->hdr_lum_avg);
	pos += snprintf(buf + pos, PAGE_SIZE, "    Min: %d\n",
		pRXCap->hdr_lum_min);

	return pos;
}

static ssize_t show_dv_cap(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	const struct dv_info *dv = &(hdmitx_device.RXCap.dv_info);

	if (dv->ieeeoui != 0x00d046)
		return pos;
	pos += snprintf(buf + pos, PAGE_SIZE,
		"DolbyVision%d RX support list:\n", dv->ver);
	if (dv->sup_yuv422_12bit)
		pos += snprintf(buf + pos, PAGE_SIZE, "    yuv422_12bit\n");
	pos += snprintf(buf + pos, PAGE_SIZE,
		"    2160p%shz: 1\n", dv->sup_2160p60hz ? "60" : "30");
	if (dv->sup_global_dimming)
		pos += snprintf(buf + pos, PAGE_SIZE, "    global dimming\n");
	if (dv->colorimetry)
		pos += snprintf(buf + pos, PAGE_SIZE, "    colorimetry\n");
	pos += snprintf(buf + pos, PAGE_SIZE,
		"    IEEEOUI: 0x%06x\n", dv->ieeeoui);
	if (dv->ver == 0)
		pos += snprintf(buf + pos, PAGE_SIZE, "    DM Ver: %x:%x\n",
			dv->vers.ver0.dm_major_ver, dv->vers.ver0.dm_minor_ver);
	if (dv->ver == 1)
		pos += snprintf(buf + pos, PAGE_SIZE, "    DM Ver: %x\n",
			dv->vers.ver1.dm_version);

	return pos;
}

static ssize_t show_aud_ch(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	   int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE,
		"hdmi_channel = %d ch\n", hdmi_ch ? hdmi_ch + 1 : 0);
	return pos;
}

static ssize_t store_aud_ch(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (strncmp(buf, "6ch", 3) == 0)
		hdmi_ch = 5;
	else if (strncmp(buf, "8ch", 3) == 0)
		hdmi_ch = 7;
	else if (strncmp(buf, "2ch", 3) == 0)
		hdmi_ch = 1;
	else
		return count;

	hdmitx_device.audio_param_update_flag = 1;
	hdmitx_device.force_audio_flag = 1;

	return count;
}

/* hdmitx audio output channel */
static ssize_t show_aud_output_chs(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	int pos = 0;

	if (hdev->aud_output_ch)
		pos += snprintf(buf + pos, PAGE_SIZE,
			"Audio Output Channels: %x:%x\n",
			(hdev->aud_output_ch >> 4) & 0xf,
			(hdev->aud_output_ch & 0xf));

	return pos;
}

/*
 * aud_output_chs CONFIGURE:
 *     [7:4] -- Output Channel Numbers, must be 2/4/6/8
 *     [3:0] -- Output Channel Mask, matched as CH3/2/1/0 R/L
 */
static ssize_t store_aud_output_chs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	int tmp = -1;
	int ret = 0;
	unsigned long msk;
	static unsigned int update_flag = -1;

	if (isdigit(buf[0]))
		tmp = buf[0] - '0';

	if (!((tmp == 2) || (tmp == 4) || (tmp == 6) || (tmp == 8))) {
		pr_info("err chn setting, must be 2, 4, 6 or 8, Rst as def\n");
		hdev->aud_output_ch = 0;
		if (update_flag != hdev->aud_output_ch) {
			update_flag = hdev->aud_output_ch;
			hdmitx_set_audio(hdev, &(hdev->cur_audio_param), 0);
		}
		return count;
	}

	/* get channel no. For I2S, there are 4 I2S_in[3:0] */
	if ((buf[1] == ':') && (isxdigit(buf[2])))
		ret = kstrtoul(&buf[2], 16, &msk);
	else
		msk = 0;
	if (ret || (msk == 0)) {
		pr_info("err chn msk, must larger than 0\n");
		return count;
	}

	hdev->aud_output_ch = (tmp << 4) + msk;

	if (update_flag != hdev->aud_output_ch) {
		update_flag = hdev->aud_output_ch;
		hdmitx_set_audio(hdev, &(hdev->cur_audio_param), 0);
	}
	return count;
}

/*
 *  1: set avmute
 * -1: clear avmute
 *  0: off avmute
 */
static ssize_t store_avmute(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int cmd = OFF_AVMUTE;

	if (strncmp(buf, "-1", 2) == 0)
		cmd = CLR_AVMUTE;
	else if (strncmp(buf, "0", 1) == 0)
		cmd = OFF_AVMUTE;
	else if (strncmp(buf, "1", 1) == 0)
		cmd = SET_AVMUTE;
	else
		pr_info("set avmute wrong: %s\n", buf);

	hdmitx_device.HWOp.CntlMisc(&hdmitx_device, MISC_AVMUTE_OP, cmd);
	return count;
}

static ssize_t show_avmute(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

/*
 * 0: clear vic
 */
static ssize_t store_vic(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = &hdmitx_device;

	if (strncmp(buf, "0", 1) == 0) {
		hdev->HWOp.CntlConfig(hdev, CONF_CLR_AVI_PACKET, 0);
		hdev->HWOp.CntlConfig(hdev, CONF_CLR_VSDB_PACKET, 0);
	}

	return count;
}

static ssize_t show_vic(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	enum hdmi_vic vic = HDMI_Unknown;
	int pos = 0;

	vic = hdev->HWOp.GetState(hdev, STAT_VIDEO_VIC, 0);
	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n", vic);

	return pos;
}

/*
 *  1: enable hdmitx phy
 *  0: disable hdmitx phy
 */
static ssize_t store_phy(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int cmd = TMDS_PHY_ENABLE;

	if (strncmp(buf, "0", 1) == 0)
		cmd = TMDS_PHY_DISABLE;
	else if (strncmp(buf, "1", 1) == 0)
		cmd = TMDS_PHY_ENABLE;
	else
		pr_info("hdmitx: set phy wrong: %s\n", buf);

	hdmitx_device.HWOp.CntlMisc(&hdmitx_device, MISC_TMDS_PHY_OP, cmd);
	return count;
}

static ssize_t show_phy(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t store_rxsense_policy(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		pr_info("hdmitx: set rxsense_policy as %d\n", val);
		if ((val == 0) || (val == 1))
			hdmitx_device.rxsense_policy = val;
		else
			pr_info("only accept as 0 or 1\n");
	}
	if (hdmitx_device.rxsense_policy)
		queue_delayed_work(hdmitx_device.rxsense_wq,
			&hdmitx_device.work_rxsense, 0);
	else
		cancel_delayed_work(&hdmitx_device.work_rxsense);


	return count;
}

static ssize_t show_rxsense_policy(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		hdmitx_device.rxsense_policy);

	return pos;
}

static ssize_t store_frac_rate(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		pr_info("hdmitx: set frac_rate_policy as %d\n", val);
		if ((val == 0) || (val == 1))
			hdmitx_device.frac_rate_policy = val;
		else
			pr_info("only accept as 0 or 1\n");
	}

	return count;
}

static ssize_t show_frac_rate(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		hdmitx_device.frac_rate_policy);

	return pos;
}

static ssize_t store_hdcp_clkdis(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	hdmitx_device.HWOp.CntlMisc(&hdmitx_device, MISC_HDCP_CLKDIS,
		buf[0] == '1' ? 1 : 0);
	return count;
}

static ssize_t show_hdcp_clkdis(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t store_hdcp_pwr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] == '1') {
		hdcp_tst_sig = 1;
		pr_info("set hdcp_pwr 1\n");
	}

	return count;
}

static ssize_t show_hdcp_pwr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	if (hdcp_tst_sig == 1) {
		pos += snprintf(buf + pos, PAGE_SIZE, "%d\n", hdcp_tst_sig);
		hdcp_tst_sig = 0;
		pr_info("restore hdcp_pwr 0\n");
	}

	return pos;
}

static ssize_t store_hdcp_byp(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	hdmitx_device.HWOp.CntlMisc(&hdmitx_device, MISC_HDCP_CLKDIS,
		buf[0] == '1' ? 1 : 0);

	return count;
}

static int lstore;
static ssize_t show_hdcp_lstore(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	if (lstore < 0x10) {
		lstore = 0;
		if (hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
			DDC_HDCP_14_LSTORE, 0))
			lstore += 1;
		if (hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
			DDC_HDCP_22_LSTORE, 0))
			lstore += 2;
	}
	if (lstore & 0x1)
		pos += snprintf(buf + pos, PAGE_SIZE, "14\n");
	if (lstore & 0x2)
		pos += snprintf(buf + pos, PAGE_SIZE, "22\n");
	if ((lstore & 0xf) == 0)
		pos += snprintf(buf + pos, PAGE_SIZE, "00\n");
	return pos;
}

static ssize_t store_hdcp_lstore(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	pr_info("hdcp: set lstore as %s\n", buf);
	if (strncmp(buf, "0", 1) == 0)
		lstore = 0x10;
	if (strncmp(buf, "11", 2) == 0)
		lstore = 0x11;
	if (strncmp(buf, "12", 2) == 0)
		lstore = 0x12;
	if (strncmp(buf, "13", 2) == 0)
		lstore = 0x13;

	return count;
}

static unsigned int div40 = -1;
static ssize_t show_div40(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	if (div40 != -1)
		pos += snprintf(buf + pos, PAGE_SIZE, "%d\n", div40);

	return pos;
}

static ssize_t store_div40(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = &hdmitx_device;

	hdev->HWOp.CntlDDC(hdev, DDC_SCDC_DIV40_SCRAMB, buf[0] == '1');
	div40 = (buf[0] == '1');

	return count;
}

static ssize_t show_hdcp_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	switch (hdmitx_device.hdcp_mode) {
	case 1:
		pos += snprintf(buf + pos, PAGE_SIZE, "14");
		break;
	case 2:
		pos += snprintf(buf + pos, PAGE_SIZE, "22");
		break;
	default:
		pos += snprintf(buf + pos, PAGE_SIZE, "off");
		break;
	}

	return pos;
}

static ssize_t store_hdcp_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
/* temporarily mark, no use SW I2C any more */
#if 0
	/* Issue SW I2C transaction to take advantage of SDA recovery logic */
	char tmp[8];

	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_PIN_MUX_OP, PIN_UNMUX);
	edid_rx_data(0x0, tmp, sizeof(tmp));
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_PIN_MUX_OP, PIN_MUX);
#endif

	pr_info("hdcp: set mode as %s\n", buf);
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_HDCP_MUX_INIT, 1);
	if (strncmp(buf, "0", 1) == 0) {
		hdmitx_device.hdcp_mode = 0;
		hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
			DDC_HDCP_OP, HDCP14_OFF);
		hdmitx_hdcp_do_work(&hdmitx_device);
	}
	if (strncmp(buf, "1", 1) == 0) {
		hdmitx_device.hdcp_mode = 1;
		hdmitx_hdcp_do_work(&hdmitx_device);
		hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
			DDC_HDCP_OP, HDCP14_ON);
	}
	if (strncmp(buf, "2", 1) == 0) {
		hdmitx_device.hdcp_mode = 2;
		hdmitx_hdcp_do_work(&hdmitx_device);
		hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
			DDC_HDCP_MUX_INIT, 2);
	}

	return count;
}

void direct_hdcptx14_start(void)
{
	pr_info("%s[%d]", __func__, __LINE__);
	hdmitx_device.hdcp_mode = 1;
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
			DDC_HDCP_OP, HDCP14_ON);
}
EXPORT_SYMBOL(direct_hdcptx14_start);

void direct_hdcptx14_stop(void)
{
	pr_info("%s[%d]", __func__, __LINE__);
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
		DDC_HDCP_OP, HDCP14_OFF);
}
EXPORT_SYMBOL(direct_hdcptx14_stop);

static ssize_t store_hdcp_ctrl(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_HDCP_14_LSTORE,
		0) == 0)
		return count;
	dev_warn(dev, "hdmitx20: %s\n", buf);
	if (strncmp(buf, "stop", 4) == 0) {
		if (strncmp(buf+4, "14", 2) == 0)
			hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
				DDC_HDCP_OP, HDCP14_OFF);
		if (strncmp(buf+4, "22", 2) == 0)
			hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
				DDC_HDCP_OP, HDCP22_OFF);
		hdmitx_device.hdcp_mode = 0;
		hdmitx_hdcp_do_work(&hdmitx_device);
	}

	return count;
}

static ssize_t show_hdcp_ctrl(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t show_hdcp_ksv_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0, i;
	char bksv_buf[5];

	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_HDCP_GET_BKSV,
		(unsigned long int)bksv_buf);

	pos += snprintf(buf+pos, PAGE_SIZE, "HDCP14 BKSV: ");
	for (i = 0; i < 5; i++) {
		pos += snprintf(buf+pos, PAGE_SIZE, "%02x",
			bksv_buf[i]);
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "  %s\n",
		hdcp_ksv_valid(bksv_buf) ? "Valid" : "Invalid");

	return pos;
}

/* Special FBC check */
static int check_fbc_special(unsigned char *edid_dat)
{
	if ((edid_dat[250] == 0xfb) && (edid_dat[251] == 0x0c))
		return 1;
	else
		return 0;
}

static ssize_t show_hdcp_ver(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	uint32_t ver = 0U;

	if (check_fbc_special(&hdmitx_device.EDID_buf[0])
	    || check_fbc_special(&hdmitx_device.EDID_buf1[0])) {
		pos += snprintf(buf+pos, PAGE_SIZE, "00\n\r");
		return pos;
	}

	/* if TX don't have HDCP22 key, skip RX hdcp22 ver */
	if (hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
		DDC_HDCP_22_LSTORE, 0) == 0)
		goto next;

	/* Detect RX support HDCP22 */
	mutex_lock(&getedid_mutex);
	ver = hdcp_rd_hdcp22_ver();
	mutex_unlock(&getedid_mutex);
	if (ver) {
		pos += snprintf(buf+pos, PAGE_SIZE, "22\n\r");
		pos += snprintf(buf+pos, PAGE_SIZE, "14\n\r");
		return pos;
	}
next:	/* Detect RX support HDCP14 */
	/* Here, must assume RX support HDCP14, otherwise affect 1A-03 */
	if (ver == 0U) {
		pos += snprintf(buf+pos, PAGE_SIZE, "14\n\r");
		return pos;
	}
	return pos;
}

static ssize_t show_hpd_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d",
		hdmitx_device.hpd_state);
	return pos;
}

static ssize_t show_hdmi_init(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n\r", hdmi_init);
	return pos;
}

static ssize_t show_ready(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\r\n",
		hdmitx_device.ready);
	return pos;
}

static ssize_t store_ready(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (strncmp(buf, "0", 1) == 0)
		hdmitx_device.ready = 0;
	if (strncmp(buf, "1", 1) == 0)
		hdmitx_device.ready = 1;
	return count;
}

static ssize_t show_support_3d(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n",
		hdmitx_device.RXCap.threeD_present);
	return pos;
}

void hdmi_print(int dbg_lvl, const char *fmt, ...)
{
	va_list args;
#if 0
	if (dbg_lvl == OFF)
		return;
#endif
	if (dbg_lvl <= debug_level) {
		va_start(args, fmt);
		vprintk(fmt, args);
		va_end(args);
	}
}

static DEVICE_ATTR(disp_mode, 0664, show_disp_mode, store_disp_mode);
static DEVICE_ATTR(attr, 0664, show_attr, store_attr);
static DEVICE_ATTR(aud_mode, 0644, show_aud_mode, store_aud_mode);
static DEVICE_ATTR(aud_mute, 0644, show_aud_mute, store_aud_mute);
static DEVICE_ATTR(vid_mute, 0644, show_vid_mute, store_vid_mute);
static DEVICE_ATTR(edid, 0644, show_edid, store_edid);
static DEVICE_ATTR(rawedid, 0444, show_rawedid, NULL);
static DEVICE_ATTR(sink_type, 0444, show_sink_type, NULL);
static DEVICE_ATTR(edid_parsing, 0444, show_edid_parsing, NULL);
static DEVICE_ATTR(config, 0664, show_config, store_config);
static DEVICE_ATTR(debug, 0200, NULL, store_debug);
static DEVICE_ATTR(disp_cap, 0444, show_disp_cap, NULL);
static DEVICE_ATTR(preferred_mode, 0444, show_preferred_mode, NULL);
static DEVICE_ATTR(aud_cap, 0444, show_aud_cap, NULL);
static DEVICE_ATTR(hdr_cap, 0444, show_hdr_cap, NULL);
static DEVICE_ATTR(dv_cap, 0444, show_dv_cap, NULL);
static DEVICE_ATTR(dc_cap, 0444, show_dc_cap, NULL);
static DEVICE_ATTR(valid_mode, 0664, show_valid_mode, store_valid_mode);
static DEVICE_ATTR(aud_ch, 0664, show_aud_ch, store_aud_ch);
static DEVICE_ATTR(aud_output_chs, 0664, show_aud_output_chs,
	store_aud_output_chs);
static DEVICE_ATTR(avmute, 0664, show_avmute, store_avmute);
static DEVICE_ATTR(vic, 0664, show_vic, store_vic);
static DEVICE_ATTR(phy, 0664, show_phy, store_phy);
static DEVICE_ATTR(frac_rate_policy, 0664, show_frac_rate, store_frac_rate);
static DEVICE_ATTR(rxsense_policy, 0644, show_rxsense_policy,
	store_rxsense_policy);
static DEVICE_ATTR(hdcp_clkdis, 0664, show_hdcp_clkdis, store_hdcp_clkdis);
static DEVICE_ATTR(hdcp_pwr, 0664, show_hdcp_pwr, store_hdcp_pwr);
static DEVICE_ATTR(hdcp_byp, 0200, NULL, store_hdcp_byp);
static DEVICE_ATTR(hdcp_mode, 0664, show_hdcp_mode, store_hdcp_mode);
static DEVICE_ATTR(hdcp_lstore, 0664, show_hdcp_lstore, store_hdcp_lstore);
static DEVICE_ATTR(hdcp_repeater, 0644, show_hdcp_repeater,
	store_hdcp_repeater);
static DEVICE_ATTR(hdcp22_type, 0644, show_hdcp22_type, store_hdcp22_type);
static DEVICE_ATTR(hdcp22_base, 0444, show_hdcp22_base, NULL);
static DEVICE_ATTR(div40, 0664, show_div40, store_div40);
static DEVICE_ATTR(hdcp_ctrl, 0664, show_hdcp_ctrl, store_hdcp_ctrl);
static DEVICE_ATTR(disp_cap_3d, 0444, show_disp_cap_3d, NULL);
static DEVICE_ATTR(hdcp_ksv_info, 0444, show_hdcp_ksv_info, NULL);
static DEVICE_ATTR(hdcp_ver, 0444, show_hdcp_ver, NULL);
static DEVICE_ATTR(hpd_state, 0444, show_hpd_state, NULL);
static DEVICE_ATTR(hdmi_init, 0444, show_hdmi_init, NULL);
static DEVICE_ATTR(ready, 0664, show_ready, store_ready);
static DEVICE_ATTR(support_3d, 0444, show_support_3d, NULL);

struct vinfo_s *hdmi_info;
static struct vinfo_s *hdmitx_get_current_info(void)
{
	return hdmi_info;
}

static int hdmitx_set_current_vmode(enum vmode_e mode)
{
	pr_info("%s[%d]\n", __func__, __LINE__);
	if (!(mode & VMODE_INIT_BIT_MASK))
		set_disp_mode_auto();
	else
		pr_info("hdmitx: alread display in uboot\n");

	return 0;
}

static enum vmode_e hdmitx_validate_vmode(char *mode)
{
	struct vinfo_s *info = hdmi_get_valid_vinfo(mode);

	if (info) {
		hdmi_info = info;
		return VMODE_HDMI;
	}
	return VMODE_MAX;
}

static int hdmitx_vmode_is_supported(enum vmode_e mode)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_HDMI)
		return true;
	else
		return false;
}

static int hdmitx_module_disable(enum vmode_e cur_vmod)
{ /* TODO */
	return 0;
}


static struct vout_server_s hdmitx_server = {
	.name = "vout_hdmitx_server",
	.op = {
		.get_vinfo = hdmitx_get_current_info,
		.set_vmode = hdmitx_set_current_vmode,
		.validate_vmode = hdmitx_validate_vmode,
		.vmode_is_supported = hdmitx_vmode_is_supported,
		.disable = hdmitx_module_disable,
#ifdef CONFIG_PM
		.vout_suspend = NULL,
		.vout_resume = NULL,
#endif
	},
};


#include <linux/soundcard.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>

static struct rate_map_fs map_fs[] = {
	{0,	  FS_REFER_TO_STREAM},
	{32000,  FS_32K},
	{44100,  FS_44K1},
	{48000,  FS_48K},
	{88200,  FS_88K2},
	{96000,  FS_96K},
	{176400, FS_176K4},
	{192000, FS_192K},
};

static enum hdmi_audio_fs aud_samp_rate_map(unsigned int rate)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(map_fs); i++) {
		if (map_fs[i].rate == rate) {
			hdmi_print(IMP, AUD "aout notify rate %d\n",
				rate);
			return map_fs[i].fs;
		}
	}
	hdmi_print(IMP, AUD "get FS_MAX\n");
	return FS_MAX;
}

static unsigned char *aud_type_string[] = {
	"CT_REFER_TO_STREAM",
	"CT_PCM",
	"CT_AC_3",
	"CT_MPEG1",
	"CT_MP3",
	"CT_MPEG2",
	"CT_AAC",
	"CT_DTS",
	"CT_ATRAC",
	"CT_ONE_BIT_AUDIO",
	"CT_DOLBY_D",
	"CT_DTS_HD",
	"CT_MAT",
	"CT_DST",
	"CT_WMA",
	"CT_MAX",
};

static struct size_map aud_size_map_ss[] = {
	{0,	 SS_REFER_TO_STREAM},
	{16,	SS_16BITS},
	{20,	SS_20BITS},
	{24,	SS_24BITS},
	{32,	SS_MAX},
};

static enum hdmi_audio_sampsize aud_size_map(unsigned int bits)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aud_size_map_ss); i++) {
		if (bits == aud_size_map_ss[i].sample_bits) {
			hdmi_print(IMP, AUD "aout notify size %d\n",
				bits);
			return aud_size_map_ss[i].ss;
		}
	}
	hdmi_print(IMP, AUD "get SS_MAX\n");
	return SS_MAX;
}

static int hdmitx_notify_callback_a(struct notifier_block *block,
	unsigned long cmd, void *para);
static struct notifier_block hdmitx_notifier_nb_a = {
	.notifier_call	= hdmitx_notify_callback_a,
};
static int hdmitx_notify_callback_a(struct notifier_block *block,
	unsigned long cmd, void *para)
{
	int i, audio_check = 0;
	struct rx_cap *pRXCap = &(hdmitx_device.RXCap);
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)para;
	struct hdmitx_audpara *audio_param =
		&(hdmitx_device.cur_audio_param);
	enum hdmi_audio_fs n_rate = aud_samp_rate_map(substream->runtime->rate);
	enum hdmi_audio_sampsize n_size =
		aud_size_map(substream->runtime->sample_bits);

	hdmitx_device.audio_param_update_flag = 0;
	hdmitx_device.audio_notify_flag = 0;

	if (audio_param->sample_rate != n_rate) {
		audio_param->sample_rate = n_rate;
		hdmitx_device.audio_param_update_flag = 1;
	}

	if (audio_param->type != cmd) {
		audio_param->type = cmd;
	hdmi_print(INF, AUD "aout notify format %s\n",
		aud_type_string[audio_param->type & 0xff]);
	hdmitx_device.audio_param_update_flag = 1;
	}

	if (audio_param->sample_size != n_size) {
		audio_param->sample_size = n_size;
		hdmitx_device.audio_param_update_flag = 1;
	}

	if (audio_param->channel_num !=
		(substream->runtime->channels - 1)) {
		audio_param->channel_num =
		substream->runtime->channels - 1;
		hdmitx_device.audio_param_update_flag = 1;
	}
	if (hdmitx_device.tx_aud_cfg == 2) {
		hdmi_print(INF, AUD "auto mode\n");
	/* Detect whether Rx is support current audio format */
	for (i = 0; i < pRXCap->AUD_count; i++) {
		if (pRXCap->RxAudioCap[i].audio_format_code == cmd)
			audio_check = 1;
	}
	/* sink don't support current audio mode */
	if ((!audio_check) && (cmd != CT_PCM)) {
		pr_info("Sink not support this audio format %lu\n",
			cmd);
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
		hdmitx_device.audio_param_update_flag = 0;
	}
	}
	if (hdmitx_device.audio_param_update_flag == 0)
		hdmi_print(INF, AUD "no update\n");
	else
		hdmitx_device.audio_notify_flag = 1;


	if ((!hdmi_audio_off_flag) &&
		(hdmitx_device.audio_param_update_flag)) {
		/* plug-in & update audio param */
		if (hdmitx_device.hpd_state == 1) {
			hdmitx_set_audio(&hdmitx_device,
			&(hdmitx_device.cur_audio_param), hdmi_ch);
		if ((hdmitx_device.audio_notify_flag == 1) ||
			(hdmitx_device.audio_step == 1)) {
			hdmitx_device.audio_notify_flag = 0;
			hdmitx_device.audio_step = 0;
		}
		hdmitx_device.audio_param_update_flag = 0;
		hdmi_print(INF, AUD "set audio param\n");
	}
	}


	return 0;
}

struct i2c_client *i2c_edid_client;

static void hdmitx_get_edid(struct hdmitx_dev *hdev)
{
	mutex_lock(&getedid_mutex);
	/* TODO hdmitx_edid_ram_buffer_clear(hdev); */
	hdev->HWOp.CntlDDC(hdev, DDC_RESET_EDID, 0);
	hdev->HWOp.CntlDDC(hdev, DDC_PIN_MUX_OP, PIN_MUX);
	/* start reading edid frist time */
	hdev->HWOp.CntlDDC(hdev, DDC_EDID_READ_DATA, 0);
	hdev->HWOp.CntlDDC(hdev, DDC_EDID_GET_DATA, 0);
	/* If EDID is not correct at first time, then retry */
	if (!check_dvi_hdmi_edid_valid(hdev->EDID_buf)) {
		msleep(100);
		/* start reading edid second time */
		hdev->HWOp.CntlDDC(hdev, DDC_EDID_READ_DATA, 0);
		hdev->HWOp.CntlDDC(hdev, DDC_EDID_GET_DATA, 1);
	}
	hdmitx_edid_clear(hdev);
	hdmitx_edid_parse(hdev);
	hdmitx_edid_buf_compare_print(hdev);
	mutex_unlock(&getedid_mutex);
}

static int get_downstream_hdcp_ver(void)
{
	/* if TX don't have HDCP22 key, skip RX hdcp22 ver */
	if (hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
		DDC_HDCP_22_LSTORE, 0) == 0)
		goto next;
	if (hdcp_rd_hdcp22_ver())
		return 22;
next:
	/* if (hdcp_rd_hdcp14_ver()) */
	return 14;
}


static void hdmitx_rxsense_process(struct work_struct *work)
{
	int sense;
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_rxsense);

	sense = hdev->HWOp.CntlMisc(hdev, MISC_TMDS_RXSENSE, 0);
	extcon_set_state_sync(hdmitx_excton_rxsense, EXTCON_DISP_HDMI, sense);
	queue_delayed_work(hdev->rxsense_wq, &hdev->work_rxsense, HZ);
}

static void hdmitx_hpd_plugin_handler(struct work_struct *work)
{
	char bksv_buf[5];
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_hpd_plugin);

	if (!(hdev->hdmitx_event & (HDMI_TX_HPD_PLUGIN)))
		return;
	if (hdev->rxsense_policy) {
		cancel_delayed_work(&hdev->work_rxsense);
		queue_delayed_work(hdev->rxsense_wq, &hdev->work_rxsense, 0);
		while (!(hdmitx_excton_rxsense->state))
			msleep_interruptible(1000);
	}
	mutex_lock(&setclk_mutex);
	pr_info("hdmitx: plugin\n");
	hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGIN;
	/* start reading E-EDID */
	if (hdev->repeater_tx)
		rx_repeat_hpd_state(1);
	hdmitx_get_edid(hdev);
	mutex_lock(&getedid_mutex);
	hdev->HWOp.CntlMisc(hdev, MISC_I2C_REACTIVE, 0);
	mutex_unlock(&getedid_mutex);
	if (hdev->repeater_tx) {
		if (check_fbc_special(&hdev->EDID_buf[0])
			|| check_fbc_special(&hdev->EDID_buf1[0]))
			rx_set_repeater_support(0);
		else
			rx_set_repeater_support(1);
		rx_repeat_hdcp_ver(get_downstream_hdcp_ver());
		hdev->HWOp.CntlDDC(hdev, DDC_HDCP_GET_BKSV,
			(unsigned long int)bksv_buf);
		rx_set_receive_hdcp(bksv_buf, 1, 1, 0, 0);
	}
	set_disp_mode_auto();
	hdmitx_set_audio(hdev, &(hdev->cur_audio_param), hdmi_ch);
	hdev->hpd_state = 1;
	hdmitx_notify_hpd(hdev->hpd_state);

	extcon_set_state_sync(hdmitx_extcon_hdmi, EXTCON_DISP_HDMI, 1);
	extcon_set_state_sync(hdmitx_excton_audio, EXTCON_DISP_HDMI, 1);

	mutex_unlock(&setclk_mutex);
}

static void clear_hdr_info(struct hdmitx_dev *hdev)
{
	struct vinfo_s *info = get_current_vinfo();

	if (info) {
		info->hdr_info.hdr_support = 0;
		info->hdr_info.lumi_max = 0;
		info->hdr_info.lumi_avg = 0;
		info->hdr_info.lumi_min = 0;
		pr_info("hdmitx: clear RX hdr info\n");
	}
}

static void hdmitx_hpd_plugout_handler(struct work_struct *work)
{
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_hpd_plugout);

	if (!(hdev->hdmitx_event & (HDMI_TX_HPD_PLUGOUT)))
		return;
	hdev->hdcp_mode = 0;
	hdev->hdcp_bcaps_repeater = 0;
	hdev->HWOp.CntlDDC(hdev, DDC_HDCP_MUX_INIT, 1);
	hdev->HWOp.CntlDDC(hdev, DDC_HDCP_OP, HDCP14_OFF);
	mutex_lock(&setclk_mutex);
	pr_info("hdmitx: plugout\n");
	if (!!(hdev->HWOp.CntlMisc(hdev, MISC_HPD_GPI_ST, 0))) {
		pr_info("hdmitx: hpd gpi high\n");
		hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGOUT;
		mutex_unlock(&setclk_mutex);
		return;
	}
	hdev->ready = 0;
	if (hdev->repeater_tx)
		rx_repeat_hpd_state(0);
	hdev->HWOp.CntlConfig(hdev, CONF_CLR_AVI_PACKET, 0);
	hdev->HWOp.CntlDDC(hdev, DDC_HDCP_MUX_INIT, 1);
	hdev->HWOp.CntlDDC(hdev, DDC_HDCP_OP, HDCP14_OFF);
	hdev->HWOp.CntlMisc(hdev, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
	hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGOUT;
	hdev->HWOp.CntlMisc(hdev, MISC_ESM_RESET, 0);
	clear_hdr_info(hdev);
	hdmitx_edid_clear(hdev);
	hdmitx_edid_ram_buffer_clear(hdev);
	hdev->hpd_state = 0;
	hdmitx_notify_hpd(hdev->hpd_state);

	extcon_set_state_sync(hdmitx_extcon_hdmi, EXTCON_DISP_HDMI, 0);
	extcon_set_state_sync(hdmitx_excton_audio, EXTCON_DISP_HDMI, 0);
	mutex_unlock(&setclk_mutex);
}

static void hdmitx_internal_intr_handler(struct work_struct *work)
{
	struct hdmitx_dev *hdev = container_of((struct work_struct *)work,
		struct hdmitx_dev, work_internal_intr);

	hdev->HWOp.DebugFun(hdev, "dumpintr");
}

int get_hpd_state(void)
{
	int ret;

	mutex_lock(&setclk_mutex);
	ret = hdmitx_device.hpd_state;
	mutex_unlock(&setclk_mutex);

	return ret;
}
EXPORT_SYMBOL(get_hpd_state);

/******************************
 *  hdmitx kernel task
 *******************************/
int tv_audio_support(int type, struct rx_cap *pRXCap)
{
	int i, audio_check = 0;

	for (i = 0; i < pRXCap->AUD_count; i++) {
		if (pRXCap->RxAudioCap[i].audio_format_code == type)
			audio_check = 1;
	}
	return audio_check;
}

static int hdmi_task_handle(void *data)
{
	struct hdmitx_dev *hdmitx_device = (struct hdmitx_dev *)data;

	hdmitx_extcon_hdmi->state = !!(hdmitx_device->HWOp.CntlMisc(
		hdmitx_device, MISC_HPD_GPI_ST, 0));
	hdmitx_device->hpd_state = hdmitx_extcon_hdmi->state;
	hdmitx_notify_hpd(hdmitx_device->hpd_state);
	extcon_set_state_sync(hdmitx_excton_power, EXTCON_DISP_HDMI,
						hdmitx_device->hpd_state);
	INIT_WORK(&hdmitx_device->work_hdr, hdr_work_func);

/* When init hdmi, clear the hdmitx module edid ram and edid buffer. */
	hdmitx_edid_ram_buffer_clear(hdmitx_device);

	hdmitx_device->hdmi_wq = alloc_workqueue(DEVICE_NAME,
		WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
	INIT_DELAYED_WORK(&hdmitx_device->work_hpd_plugin,
		hdmitx_hpd_plugin_handler);
	INIT_DELAYED_WORK(&hdmitx_device->work_hpd_plugout,
		hdmitx_hpd_plugout_handler);
	INIT_WORK(&hdmitx_device->work_internal_intr,
		hdmitx_internal_intr_handler);

	/* for rx sense feature */
	hdmitx_device->rxsense_wq = alloc_workqueue(hdmitx_excton_rxsense->name,
		WQ_SYSFS | WQ_FREEZABLE, 0);
	INIT_DELAYED_WORK(&hdmitx_device->work_rxsense, hdmitx_rxsense_process);

	hdmitx_device->tx_aud_cfg = 1; /* default audio configure is on */

	hdmitx_device->HWOp.SetupIRQ(hdmitx_device);

	/* Trigger */
	hdmitx_device->HWOp.CntlMisc(hdmitx_device, MISC_HPD_MUX_OP, PIN_UNMUX);
	mdelay(20);
	hdmitx_device->HWOp.CntlMisc(hdmitx_device, MISC_HPD_MUX_OP, PIN_MUX);

	hdmi_init = 1;
	return 0;
}

/* Linux */
/*****************************
 *	hdmitx driver file_operations
 *
 ******************************/
static int amhdmitx_open(struct inode *node, struct file *file)
{
	struct hdmitx_dev *hdmitx_in_devp;

	/* Get the per-device structure that contains this cdev */
	hdmitx_in_devp = container_of(node->i_cdev, struct hdmitx_dev, cdev);
	file->private_data = hdmitx_in_devp;

	return 0;

}


static int amhdmitx_release(struct inode *node, struct file *file)
{
	/* struct hdmitx_dev *hdmitx_in_devp = file->private_data; */

	/* Reset file pointer */

	/* Release some other fields */
	/* ... */
	return 0;
}

static const struct file_operations amhdmitx_fops = {
	.owner	= THIS_MODULE,
	.open	 = amhdmitx_open,
	.release  = amhdmitx_release,
/* .ioctl	= amhdmitx_ioctl, */
};

struct hdmitx_dev *get_hdmitx_device(void)
{
	return &hdmitx_device;
}
EXPORT_SYMBOL(get_hdmitx_device);

static int get_dt_vend_init_data(struct device_node *np,
	struct vendor_info_data *vend)
{
	int ret;

	ret = of_property_read_string(np, "vendor_name",
		(const char **)&(vend->vendor_name));
	if (ret)
		hdmi_print(INF, SYS "not find vendor name\n");

	ret = of_property_read_u32(np, "vendor_id", &(vend->vendor_id));
	if (ret)
		hdmi_print(INF, SYS "not find vendor id\n");

	ret = of_property_read_string(np, "product_desc",
		(const char **)&(vend->product_desc));
	if (ret)
		hdmi_print(INF, SYS "not find product desc\n");
	return 0;
}

static void hdmitx_init_fmt_attr(struct hdmitx_dev *hdev, char *attr)
{
	memset(attr, 0, sizeof(fmt_attr));
	if ((hdev->para->cd == COLORDEPTH_RESERVED) &&
	    (hdev->para->cs == COLORSPACE_RESERVED)) {
		strcpy(fmt_attr, "default");
	} else {
		switch (hdev->para->cs) {
		case COLORSPACE_RGB444:
			memcpy(fmt_attr, "rgb,", 4);
			break;
		case COLORSPACE_YUV422:
			memcpy(fmt_attr, "422,", 4);
			break;
		case COLORSPACE_YUV444:
			memcpy(fmt_attr, "444,", 4);
			break;
		case COLORSPACE_YUV420:
			memcpy(fmt_attr, "420,", 4);
			break;
		default:
			break;
		}
		switch (hdev->para->cd) {
		case COLORDEPTH_24B:
			strcat(fmt_attr, "8bit");
			break;
		case COLORDEPTH_30B:
			strcat(fmt_attr, "10bit");
			break;
		case COLORDEPTH_36B:
			strcat(fmt_attr, "12bit");
			break;
		case COLORDEPTH_48B:
			strcat(fmt_attr, "16bit");
			break;
		default:
			break;
		}
	}
}

static void hdmi_init_chip_type(void)
{
	/* auto detect chip_type for registers ioremap */
	switch (get_cpu_type()) {
	case MESON_CPU_MAJOR_ID_TXLX:
		hdmitx_device.chip_type = 1;
		break;
	default:
		break;
	}

	pr_info("hdmitx: %s: %d\n", __func__, hdmitx_device.chip_type);
}

/* for notify to cec */
static BLOCKING_NOTIFIER_HEAD(hdmitx_event_notify_list);
int hdmitx_event_notifier_regist(struct notifier_block *nb)
{
	int ret;

	ret = blocking_notifier_chain_register(&hdmitx_event_notify_list, nb);
	/* update status when register */
	if (!ret && nb && nb->notifier_call) {
		hdmitx_notify_hpd(hdmitx_device.hpd_state);
		if (hdmitx_device.physical_addr != 0xffff)
			hdmitx_event_notify(HDMITX_PHY_ADDR_VALID,
					    &hdmitx_device.physical_addr);
	}

	return ret;
}
EXPORT_SYMBOL(hdmitx_event_notifier_regist);

int hdmitx_event_notifier_unregist(struct notifier_block *nb)
{
	int ret;

	ret = blocking_notifier_chain_unregister(&hdmitx_event_notify_list, nb);

	return ret;
}
EXPORT_SYMBOL(hdmitx_event_notifier_unregist);

void hdmitx_event_notify(unsigned long state, void *arg)
{
	blocking_notifier_call_chain(&hdmitx_event_notify_list, state, arg);
}

void hdmitx_hdcp_status(int hdmi_authenticated)
{
	extcon_set_state_sync(hdmitx_excton_hdcp, EXTCON_DISP_HDMI,
							hdmi_authenticated);
}

void hdmitx_extcon_register(struct platform_device *pdev, struct device *dev)
{
	struct extcon_dev *edev;
	int ret;

	/*hdmitx extcon hdmi*/
	edev = extcon_dev_allocate(hdmi_cable);
	if (IS_ERR(edev)) {
		hdmi_print(IMP, SYS "failed to allocate hdmitx extcon hdmi\n");
		return;
	}
	edev->dev.parent = dev;
	edev->name = "hdmitx_extcon_hdmi";
	dev_set_name(&edev->dev, "hdmi");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		hdmi_print(IMP, SYS "failed to register hdmitx extcon hdmi\n");
		return;
	}
	hdmitx_extcon_hdmi = edev;

	/*hdmitx extcon audio*/
	edev = extcon_dev_allocate(hdmi_cable);
	if (IS_ERR(edev)) {
		hdmi_print(IMP, SYS "failed to allocate hdmitx extcon audio\n");
		return;
	}

	edev->dev.parent = dev;
	edev->name = "hdmitx_excton_audio";
	dev_set_name(&edev->dev, "hdmi_audio");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		hdmi_print(IMP, SYS "failed to register hdmitx extcon audio\n");
		return;
	}
	hdmitx_excton_audio = edev;

	/*hdmitx extcon power*/
	edev = extcon_dev_allocate(hdmi_cable);
	if (IS_ERR(edev)) {
		hdmi_print(IMP, SYS "failed to allocate hdmitx extcon power\n");
		return;
	}

	edev->dev.parent = dev;
	edev->name = "hdmitx_excton_power";
	dev_set_name(&edev->dev, "hdmi_power");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		hdmi_print(IMP, SYS "failed to register extcon power\n");
		return;
	}
	hdmitx_excton_power = edev;

	/*hdmitx extcon hdr*/
	edev = extcon_dev_allocate(hdmi_cable);
	if (IS_ERR(edev)) {
		hdmi_print(IMP, SYS "failed to allocate hdmitx extcon hdr\n");
		return;
	}

	edev->dev.parent = dev;
	edev->name = "hdmitx_excton_hdr";
	dev_set_name(&edev->dev, "hdmi_hdr");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		hdmi_print(IMP, SYS "failed to register hdmitx extcon hdr\n");
		return;
	}
	hdmitx_excton_hdr = edev;

	/*hdmitx extcon rxsense*/
	edev = extcon_dev_allocate(hdmi_cable);
	if (IS_ERR(edev)) {
		hdmi_print(IMP, SYS "failed to allocate extcon rxsense\n");
		return;
	}

	edev->dev.parent = dev;
	edev->name = "hdmitx_excton_rxsense";
	dev_set_name(&edev->dev, "hdmi_rxsense");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		hdmi_print(IMP, SYS "failed to register extcon rxsense\n");
		return;
	}
	hdmitx_excton_rxsense = edev;

	/*hdmitx extcon hdcp*/
	edev = extcon_dev_allocate(hdmi_cable);
	if (IS_ERR(edev)) {
		hdmi_print(IMP, SYS "failed to allocate extcon hdcp\n");
		return;
	}

	edev->dev.parent = dev;
	edev->name = "hdmitx_excton_hdcp";
	dev_set_name(&edev->dev, "hdcp");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		hdmi_print(IMP, SYS "failed to register extcon hdcp\n");
		return;
	}
	hdmitx_excton_hdcp = edev;

	/*hdmitx extcon setmode*/
	edev = extcon_dev_allocate(hdmi_cable);
	if (IS_ERR(edev)) {
		hdmi_print(IMP, SYS "failed to allocate extcon setmode\n");
		return;
	}

	edev->dev.parent = dev;
	edev->name = "hdmitx_excton_setmode";
	dev_set_name(&edev->dev, "setmode");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		hdmi_print(IMP, SYS "failed to register extcon setmode\n");
		return;
	}
	hdmitx_excton_setmode = edev;

}

static int amhdmitx_device_init(struct hdmitx_dev *hdmi_dev)
{
	if (hdmi_dev == NULL)
		return 1;

	hdmi_dev->hdtx_dev = NULL;

	return 0;
}

static int amhdmitx_probe(struct platform_device *pdev)
{
	int r, ret = 0;
	struct pinctrl *p;
	struct device *dev;

#ifdef CONFIG_OF
	int val;
	phandle phandle;
	struct device_node *init_data;
#endif

	hdmi_print(IMP, SYS "amhdmitx_init\n");
	hdmi_print(IMP, SYS "Ver: %s\n", HDMITX_VER);

	amhdmitx_device_init(&hdmitx_device);

	hdmitx_device.hdtx_dev = &pdev->dev;
	hdmitx_device.physical_addr = 0xffff;
	/* init para for NULL protection */
	hdmitx_device.para = hdmi_get_fmt_name("invalid", fmt_attr);
	hdmi_print(IMP, SYS "amhdmitx_probe\n");

	r = alloc_chrdev_region(&hdmitx_id, 0, HDMI_TX_COUNT,
		DEVICE_NAME);

	hdmitx_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(hdmitx_class)) {
		unregister_chrdev_region(hdmitx_id, HDMI_TX_COUNT);
		return -1;
		/* return PTR_ERR(aoe_class); */
	}

	hdmitx_device.unplug_powerdown = 0;
	hdmitx_device.vic_count = 0;
	hdmitx_device.auth_process_timer = 0;
	hdmitx_device.force_audio_flag = 0;
	hdmitx_device.hdcp_mode = 0;
	hdmitx_device.ready = 0;
	/* no 1.000/1.001 modes by default */
	hdmitx_device.frac_rate_policy = 0;
	hdmitx_device.rxsense_policy = 0; /* no RxSense by default */

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	register_early_suspend(&hdmitx_early_suspend_handler);
#endif
	hdmitx_device.nb.notifier_call = hdmitx_reboot_notifier;
	register_reboot_notifier(&hdmitx_device.nb);
	if ((init_flag&INIT_FLAG_POWERDOWN) && (hpdmode == 2))
		hdmitx_device.mux_hpd_if_pin_high_flag = 0;
	else
		hdmitx_device.mux_hpd_if_pin_high_flag = 1;
	hdmitx_device.audio_param_update_flag = 0;
	cdev_init(&(hdmitx_device.cdev), &amhdmitx_fops);
	hdmitx_device.cdev.owner = THIS_MODULE;
	cdev_add(&(hdmitx_device.cdev), hdmitx_id, HDMI_TX_COUNT);

	dev = device_create(hdmitx_class, NULL, hdmitx_id, NULL,
		"amhdmitx%d", 0); /* kernel>=2.6.27 */

	if (dev == NULL) {
		hdmi_print(ERR, SYS "device_create create error\n");
		class_destroy(hdmitx_class);
		r = -EEXIST;
		return r;
	}
	hdmitx_device.hdtx_dev = dev;
	ret = device_create_file(dev, &dev_attr_disp_mode);
	ret = device_create_file(dev, &dev_attr_attr);
	ret = device_create_file(dev, &dev_attr_aud_mode);
	ret = device_create_file(dev, &dev_attr_aud_mute);
	ret = device_create_file(dev, &dev_attr_vid_mute);
	ret = device_create_file(dev, &dev_attr_edid);
	ret = device_create_file(dev, &dev_attr_rawedid);
	ret = device_create_file(dev, &dev_attr_sink_type);
	ret = device_create_file(dev, &dev_attr_edid_parsing);
	ret = device_create_file(dev, &dev_attr_config);
	ret = device_create_file(dev, &dev_attr_debug);
	ret = device_create_file(dev, &dev_attr_disp_cap);
	ret = device_create_file(dev, &dev_attr_preferred_mode);
	ret = device_create_file(dev, &dev_attr_disp_cap_3d);
	ret = device_create_file(dev, &dev_attr_aud_cap);
	ret = device_create_file(dev, &dev_attr_hdr_cap);
	ret = device_create_file(dev, &dev_attr_dv_cap);
	ret = device_create_file(dev, &dev_attr_aud_ch);
	ret = device_create_file(dev, &dev_attr_aud_output_chs);
	ret = device_create_file(dev, &dev_attr_avmute);
	ret = device_create_file(dev, &dev_attr_vic);
	ret = device_create_file(dev, &dev_attr_phy);
	ret = device_create_file(dev, &dev_attr_frac_rate_policy);
	ret = device_create_file(dev, &dev_attr_rxsense_policy);
	ret = device_create_file(dev, &dev_attr_hdcp_clkdis);
	ret = device_create_file(dev, &dev_attr_hdcp_pwr);
	ret = device_create_file(dev, &dev_attr_hdcp_ksv_info);
	ret = device_create_file(dev, &dev_attr_hdcp_ver);
	ret = device_create_file(dev, &dev_attr_hdcp_byp);
	ret = device_create_file(dev, &dev_attr_hdcp_mode);
	ret = device_create_file(dev, &dev_attr_hdcp_repeater);
	ret = device_create_file(dev, &dev_attr_hdcp22_type);
	ret = device_create_file(dev, &dev_attr_hdcp22_base);
	ret = device_create_file(dev, &dev_attr_hdcp_lstore);
	ret = device_create_file(dev, &dev_attr_div40);
	ret = device_create_file(dev, &dev_attr_hdcp_ctrl);
	ret = device_create_file(dev, &dev_attr_hpd_state);
	ret = device_create_file(dev, &dev_attr_hdmi_init);
	ret = device_create_file(dev, &dev_attr_ready);
	ret = device_create_file(dev, &dev_attr_support_3d);
	ret = device_create_file(dev, &dev_attr_dc_cap);
	ret = device_create_file(dev, &dev_attr_valid_mode);

	vout_register_server(&hdmitx_server);
#ifdef CONFIG_AMLOGIC_SND_SOC
	aout_register_client(&hdmitx_notifier_nb_a);
#else
	r = r ? (long int)&hdmitx_notifier_nb_a :
		(long int)&hdmitx_notifier_nb_a;
#endif

	hdmi_init_chip_type();
#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		memset(&hdmitx_device.config_data, 0,
			sizeof(struct hdmi_config_platform_data));
/* HPD pinctrl */
	if (of_get_property(pdev->dev.of_node, "pinctrl-names", NULL)) {
		ret = of_property_read_string_index(pdev->dev.of_node,
			"pinctrl-names", 0, &hdmitx_device.hpd_pin);
		if (!ret)
			p = devm_pinctrl_get_select(&pdev->dev,
				hdmitx_device.hpd_pin);
	}
/* DDC pinctrl */
	if (of_get_property(pdev->dev.of_node, "pinctrl-names", NULL)) {
		ret = of_property_read_string_index(pdev->dev.of_node,
			"pinctrl-names", 1, &hdmitx_device.ddc_pin);
		if (!ret)
			p = devm_pinctrl_get_select(&pdev->dev,
				hdmitx_device.ddc_pin);
	}

/* Get vendor information */
	ret = of_property_read_u32(pdev->dev.of_node,
			"vend-data", &val);
	if (ret)
		hdmi_print(INF, SYS "not find match init-data\n");
	if (ret == 0) {
		phandle = val;
		init_data = of_find_node_by_phandle(phandle);
		if (!init_data)
			hdmi_print(INF, SYS "not find device node\n");
		hdmitx_device.config_data.vend_data = kzalloc(
			sizeof(struct vendor_info_data), GFP_KERNEL);
		if (!hdmitx_device.config_data.vend_data)
			hdmi_print(INF, SYS
				"can not get vend_data dat\n");
		ret = get_dt_vend_init_data(init_data,
			hdmitx_device.config_data.vend_data);
		if (ret)
			hdmi_print(INF, SYS "not find vend_init_data\n");
	}
/* Get power control */
		ret = of_property_read_u32(pdev->dev.of_node,
			"pwr-ctrl", &val);
	if (ret)
		hdmi_print(INF, SYS "not find match pwr-ctl\n");
	if (ret == 0) {
		phandle = val;
		init_data = of_find_node_by_phandle(phandle);
		if (!init_data)
			hdmi_print(INF, SYS "not find device node\n");
		hdmitx_device.config_data.pwr_ctl = kzalloc((sizeof(
			struct hdmi_pwr_ctl)) * HDMI_TX_PWR_CTRL_NUM,
			GFP_KERNEL);
		if (!hdmitx_device.config_data.pwr_ctl)
			hdmi_print(INF, SYS"can not get pwr_ctl mem\n");
		memset(hdmitx_device.config_data.pwr_ctl, 0,
			sizeof(struct hdmi_pwr_ctl));
		if (ret)
			hdmi_print(INF, SYS "not find pwr_ctl\n");
	}
	}

#else
	hdmi_pdata = pdev->dev.platform_data;
	if (!hdmi_pdata) {
		hdmi_print(INF, SYS "not get platform data\n");
		r = -ENOENT;
	} else {
		hdmi_print(INF, SYS "get hdmi platform data\n");
	}
#endif
	hdmitx_device.irq_hpd = platform_get_irq_byname(pdev, "hdmitx_hpd");
	if (hdmitx_device.irq_hpd == -ENXIO) {
		pr_err("%s: ERROR: hdmitx hpd irq No not found\n",
			__func__);
		return -ENXIO;
	}
	pr_info("hdmitx hpd irq = %d\n", hdmitx_device.irq_hpd);

	hdmitx_extcon_register(pdev, dev);

	hdmitx_init_parameters(&hdmitx_device.hdmi_info);
	HDMITX_Meson_Init(&hdmitx_device);
	hdmitx_init_fmt_attr(&hdmitx_device, fmt_attr);
	pr_info("hdmitx: attr %s\n", fmt_attr);
	hdmitx_device.task = kthread_run(hdmi_task_handle,
		&hdmitx_device, "kthread_hdmi");
	return r;
}

static int amhdmitx_remove(struct platform_device *pdev)
{
	struct device *dev = hdmitx_device.hdtx_dev;

	cancel_work_sync(&hdmitx_device.work_hdr);

	if (hdmitx_device.HWOp.UnInit)
		hdmitx_device.HWOp.UnInit(&hdmitx_device);
	hdmitx_device.hpd_event = 0xff;
	kthread_stop(hdmitx_device.task);
	vout_unregister_server(&hdmitx_server);
#ifdef CONFIG_AMLOGIC_SND_SOC
	aout_unregister_client(&hdmitx_notifier_nb_a);
#endif

	/* Remove the cdev */
	device_remove_file(dev, &dev_attr_disp_mode);
	device_remove_file(dev, &dev_attr_attr);
	device_remove_file(dev, &dev_attr_aud_mode);
	device_remove_file(dev, &dev_attr_aud_mute);
	device_remove_file(dev, &dev_attr_vid_mute);
	device_remove_file(dev, &dev_attr_edid);
	device_remove_file(dev, &dev_attr_rawedid);
	device_remove_file(dev, &dev_attr_sink_type);
	device_remove_file(dev, &dev_attr_edid_parsing);
	device_remove_file(dev, &dev_attr_config);
	device_remove_file(dev, &dev_attr_debug);
	device_remove_file(dev, &dev_attr_disp_cap);
	device_remove_file(dev, &dev_attr_preferred_mode);
	device_remove_file(dev, &dev_attr_disp_cap_3d);
	device_remove_file(dev, &dev_attr_hdr_cap);
	device_remove_file(dev, &dev_attr_dv_cap);
	device_remove_file(dev, &dev_attr_dc_cap);
	device_remove_file(dev, &dev_attr_valid_mode);
	device_remove_file(dev, &dev_attr_hpd_state);
	device_remove_file(dev, &dev_attr_hdmi_init);
	device_remove_file(dev, &dev_attr_ready);
	device_remove_file(dev, &dev_attr_support_3d);
	device_remove_file(dev, &dev_attr_avmute);
	device_remove_file(dev, &dev_attr_vic);
	device_remove_file(dev, &dev_attr_frac_rate_policy);
	device_remove_file(dev, &dev_attr_rxsense_policy);
	device_remove_file(dev, &dev_attr_hdcp_pwr);
	device_remove_file(dev, &dev_attr_aud_output_chs);
	device_remove_file(dev, &dev_attr_div40);
	device_remove_file(dev, &dev_attr_hdcp_repeater);
	device_remove_file(dev, &dev_attr_hdcp22_type);
	device_remove_file(dev, &dev_attr_hdcp22_base);

	cdev_del(&hdmitx_device.cdev);

	device_destroy(hdmitx_class, hdmitx_id);

	class_destroy(hdmitx_class);

/* TODO */
/* kfree(hdmi_pdata->phy_data); */
/* kfree(hdmi_pdata); */

	unregister_chrdev_region(hdmitx_id, HDMI_TX_COUNT);
	return 0;
}

#ifdef CONFIG_PM
static int amhdmitx_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	return 0;
}

static int amhdmitx_resume(struct platform_device *pdev)
{
	pr_info("amhdmitx: resume module %d\n", __LINE__);
	return 0;
}
#endif

#ifdef CONFIG_INSTABOOT
static unsigned char __nosavedata EDID_buf_save[EDID_MAX_BLOCK * 128];
static unsigned char __nosavedata EDID_buf1_save[EDID_MAX_BLOCK * 128];
static unsigned char __nosavedata EDID_hash_save[20];
static struct rx_cap __nosavedata RXCap_save;
static struct hdmitx_info __nosavedata hdmi_info_save;

static void save_device_param(void)
{
	memcpy(EDID_buf_save, hdmitx_device.EDID_buf, EDID_MAX_BLOCK * 128);
	memcpy(EDID_buf1_save, hdmitx_device.EDID_buf1, EDID_MAX_BLOCK * 128);
	memcpy(EDID_hash_save, hdmitx_device.EDID_hash, 20);
	memcpy(&RXCap_save, &hdmitx_device.RXCap, sizeof(struct rx_cap));
	memcpy(&hdmi_info_save, &hdmitx_device.hdmi_info,
		sizeof(struct hdmitx_info));
}

static void restore_device_param(void)
{
	memcpy(hdmitx_device.EDID_buf, EDID_buf_save, EDID_MAX_BLOCK * 128);
	memcpy(hdmitx_device.EDID_buf1, EDID_buf1_save, EDID_MAX_BLOCK * 128);
	memcpy(hdmitx_device.EDID_hash, EDID_hash_save, 20);
	memcpy(&hdmitx_device.RXCap, &RXCap_save, sizeof(struct rx_cap));
	memcpy(&hdmitx_device.hdmi_info, &hdmi_info_save,
		sizeof(struct hdmitx_info));
}

static int amhdmitx_realdata_save(void)
{
	save_device_param();
	return 0;
}

static void amhdmitx_realdata_restore(void)
{
	restore_device_param();
}

static struct instaboot_realdata_ops amhdmitx_realdata_ops = {
	.save		= amhdmitx_realdata_save,
	.restore	= amhdmitx_realdata_restore,
};

static int amhdmitx_restore(struct device *dev)
{
	int current_hdmi_state = !!(hdmitx_device.HWOp.CntlMisc(&hdmitx_device,
			MISC_HPD_GPI_ST, 0));
	char *vout_mode = get_vout_mode_internal();

	if (strstr(vout_mode, "cvbs") && current_hdmi_state == 1) {
		mutex_lock(&setclk_mutex);
		hdmitx_extcon_hdmi->state = 0;
		hdmitx_device.hpd_state = hdmitx_extcon_hdmi->state;
		hdmitx_notify_hpd(hdmitx_device.hpd_state);
		mutex_unlock(&setclk_mutex);
		pr_info("resend hdmi plug in event\n");
		hdmitx_device.hdmitx_event |= HDMI_TX_HPD_PLUGIN;
		hdmitx_device.hdmitx_event &= ~HDMI_TX_HPD_PLUGOUT;
		PREPARE_DELAYED_WORK(&hdmitx_device.work_hpd_plugin,
			hdmitx_hpd_plugin_handler);
		queue_delayed_work(hdmitx_device.hdmi_wq,
			&hdmitx_device.work_hpd_plugin, 2 * HZ);
	} else {
		mutex_lock(&setclk_mutex);
		hdmitx_extcon_hdmi->state = current_hdmi_state;
		hdmitx_device.hpd_state = hdmitx_extcon_hdmi->state;
		hdmitx_notify_hpd(hdmitx_device.hpd_state);
		mutex_unlock(&setclk_mutex);
	}
	return 0;
}
static int amhdmitx_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return amhdmitx_suspend(pdev, PMSG_SUSPEND);
}
static int amhdmitx_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return amhdmitx_resume(pdev);
}
static const struct dev_pm_ops amhdmitx_pm = {
	.restore	= amhdmitx_restore,
	.suspend	= amhdmitx_pm_suspend,
	.resume		= amhdmitx_pm_resume,
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id meson_amhdmitx_dt_match[] = {
	{
	.compatible	 = "amlogic, amhdmitx",
	},
	{},
};
#else
#define meson_amhdmitx_dt_match NULL
#endif
static struct platform_driver amhdmitx_driver = {
	.probe	  = amhdmitx_probe,
	.remove	 = amhdmitx_remove,
#ifdef CONFIG_PM
	.suspend	= amhdmitx_suspend,
	.resume	 = amhdmitx_resume,
#endif
	.driver	 = {
		.name   = DEVICE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = meson_amhdmitx_dt_match,
#ifdef CONFIG_HIBERNATION
		.pm	= &amhdmitx_pm,
#endif
	}
};

static int  __init amhdmitx_init(void)
{
	if (init_flag&INIT_FLAG_NOT_LOAD)
		return 0;

	if (platform_driver_register(&amhdmitx_driver)) {
		hdmi_print(ERR, SYS
			"failed to register amhdmitx module\n");
#if 0
	platform_device_del(amhdmi_tx_device);
	platform_device_put(amhdmi_tx_device);
#endif
	return -ENODEV;
	}
#ifdef CONFIG_INSTABOOT
	INIT_LIST_HEAD(&amhdmitx_realdata_ops.node);
	register_instaboot_realdata_ops(&amhdmitx_realdata_ops);
#endif
	return 0;
}




static void __exit amhdmitx_exit(void)
{
	hdmi_print(INF, SYS "amhdmitx_exit\n");
	platform_driver_unregister(&amhdmitx_driver);
/* \\	platform_device_unregister(amhdmi_tx_device); */
/* \\	amhdmi_tx_device = NULL; */
#ifdef CONFIG_INSTABOOT
	unregister_instaboot_realdata_ops(&amhdmitx_realdata_ops);
#endif
}

/* module_init(amhdmitx_init); */
subsys_initcall(amhdmitx_init);
module_exit(amhdmitx_exit);

MODULE_DESCRIPTION("AMLOGIC HDMI TX driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");

/* besides characters defined in separator, '\"' are used as separator;
 * and any characters in '\"' will not act as separator
 */
static char *next_token_ex(char *separator, char *buf, unsigned int size,
	unsigned int offset, unsigned int *token_len,
	unsigned int *token_offset)
{
	char *pToken = NULL;
	char last_separator = 0;
	char trans_char_flag = 0;

	if (buf) {
		for (; offset < size; offset++) {
			int ii = 0;
		char ch;

		if (buf[offset] == '\\') {
			trans_char_flag = 1;
			continue;
		}
		while (((ch = separator[ii++]) != buf[offset]) && (ch))
			;
		if (ch) {
			if (!pToken) {
				continue;
		} else {
			if (last_separator != '"') {
				*token_len = (unsigned int)
					(buf + offset - pToken);
				*token_offset = offset;
				return pToken;
			}
		}
		} else if (!pToken) {
			if (trans_char_flag && (buf[offset] == '"'))
				last_separator = buf[offset];
			pToken = &buf[offset];
		} else if ((trans_char_flag && (buf[offset] == '"'))
				&& (last_separator == '"')) {
			*token_len = (unsigned int)(buf + offset - pToken - 2);
			*token_offset = offset + 1;
			return pToken + 1;
		}
		trans_char_flag = 0;
	}
	if (pToken) {
		*token_len = (unsigned int)(buf + offset - pToken);
		*token_offset = offset;
	}
	}
	return pToken;
}

static  int __init hdmitx_boot_para_setup(char *s)
{
	char separator[] = {' ', ',', ';', 0x0};
	char *token;
	unsigned int token_len, token_offset, offset = 0;
	int size = strlen(s);

	do {
		token = next_token_ex(separator, s, size, offset,
				&token_len, &token_offset);
	if (token) {
		if ((token_len == 3)
			&& (strncmp(token, "off", token_len) == 0)) {
			init_flag |= INIT_FLAG_NOT_LOAD;
		}
	}
		offset = token_offset;
	} while (token);
	return 0;
}

__setup("hdmitx=", hdmitx_boot_para_setup);

MODULE_PARM_DESC(hdmi_detect_when_booting, "\n hdmi_detect_when_booting\n");
module_param(hdmi_detect_when_booting, int, 0664);

MODULE_PARM_DESC(hdmi_prbs_mode, "\n hdmi_prbs_mode\n");
module_param(hdmi_prbs_mode, int, 0664);

MODULE_PARM_DESC(debug_level, "\n debug_level\n");
module_param(debug_level, int, 0664);
