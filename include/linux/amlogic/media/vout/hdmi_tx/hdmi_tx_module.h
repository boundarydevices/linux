/*
 * include/linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h
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

#ifndef _HDMI_TX_MODULE_H
#define _HDMI_TX_MODULE_H
#include "hdmi_info_global.h"
#include "hdmi_config.h"
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
/* #include <linux/amlogic/aml_gpio_consumer.h> */

/*****************************
 *    hdmitx attr management
 ******************************/

/************************************
 *    hdmitx device structure
 *************************************/
/*  VIC_MAX_VALID_MODE and VIC_MAX_NUM are associated with
 *	HDMITX_VIC420_OFFSET and HDMITX_VIC_MASK in hdmi_common.h
 */
#define VIC_MAX_VALID_MODE	256 /* consider 4k2k */
/* half for valid vic, half for vic with y420*/
#define VIC_MAX_NUM 512
#define AUD_MAX_NUM 60
struct rx_audiocap {
	unsigned char audio_format_code;
	unsigned char channel_num_max;
	unsigned char freq_cc;
	unsigned char cc3;
};

enum hd_ctrl {
	VID_EN, VID_DIS, AUD_EN, AUD_DIS, EDID_EN, EDID_DIS, HDCP_EN, HDCP_DIS,
};

struct rx_cap {
	unsigned int native_Mode;
	/*video*/
	unsigned int VIC[VIC_MAX_NUM];
	unsigned int VIC_count;
	unsigned int native_VIC;
	/*audio*/
	struct rx_audiocap RxAudioCap[AUD_MAX_NUM];
	unsigned char AUD_count;
	unsigned char RxSpeakerAllocation;
	/*vendor*/
	unsigned int IEEEOUI;
	unsigned int Max_TMDS_Clock1; /* HDMI1.4b TMDS_CLK */
	unsigned int HF_IEEEOUI;	/* For HDMI Forum */
	unsigned int Max_TMDS_Clock2; /* HDMI2.0 TMDS_CLK */
	/* CEA861-F, Table 56, Colorimetry Data Block */
	unsigned int colorimetry_data;
	unsigned int scdc_present:1;
	unsigned int scdc_rr_capable:1; /* SCDC read request */
	unsigned int lte_340mcsc_scramble:1;
	unsigned int dc_y444:1;
	unsigned int dc_30bit:1;
	unsigned int dc_36bit:1;
	unsigned int dc_48bit:1;
	unsigned int dc_30bit_420:1;
	unsigned int dc_36bit_420:1;
	unsigned int dc_48bit_420:1;
	unsigned int hdr_sup_eotf_sdr:1;
	unsigned int hdr_sup_eotf_hdr:1;
	unsigned int hdr_sup_eotf_smpte_st_2084:1;
	unsigned int hdr_sup_eotf_hlg:1;
	unsigned int hdr_sup_SMD_type1:1;
	unsigned char hdr_lum_max;
	unsigned char hdr_lum_avg;
	unsigned char hdr_lum_min;
	unsigned char IDManufacturerName[4];
	unsigned char IDProductCode[2];
	unsigned char IDSerialNumber[4];
	unsigned char ReceiverProductName[16];
	unsigned char manufacture_week;
	unsigned char manufacture_year;
	unsigned char physcial_weight;
	unsigned char physcial_height;
	unsigned char edid_version;
	unsigned char edid_revision;
	unsigned int ColorDeepSupport;
	unsigned int Video_Latency;
	unsigned int Audio_Latency;
	unsigned int Interlaced_Video_Latency;
	unsigned int Interlaced_Audio_Latency;
	unsigned int threeD_present;
	unsigned int threeD_Multi_present;
	unsigned int hdmi_vic_LEN;
	unsigned int HDMI_3D_LEN;
	unsigned int threeD_Structure_ALL_15_0;
	unsigned int threeD_MASK_15_0;
	struct {
		unsigned char frame_packing;
		unsigned char top_and_bottom;
		unsigned char side_by_side;
	} support_3d_format[VIC_MAX_NUM];
	struct dv_info dv_info;
	enum hdmi_vic preferred_mode;
	struct dtd dtd[16];
	unsigned char dtd_idx;
	unsigned char flag_vfpdb;
	unsigned char number_of_dtd;
	/*blk0 check sum*/
	unsigned char blk0_chksum;
};

struct cts_conftab {
	unsigned int fixed_n;
	unsigned int tmds_clk;
	unsigned int fixed_cts;
};

struct vic_attrmap {
	enum hdmi_vic VIC;
	unsigned int tmds_clk;
};

enum hdmi_event_t {
	HDMI_TX_NONE = 0,
	HDMI_TX_HPD_PLUGIN = 1,
	HDMI_TX_HPD_PLUGOUT = 2,
	HDMI_TX_INTERNAL_INTR = 4,
};

struct hdmi_phy_t {
	unsigned long reg;
	unsigned long val_sleep;
	unsigned long val_save;
};

struct audcts_log {
	unsigned int val:20;
	unsigned int stable:1;
};

struct frac_rate_table {
	char *hz;
	u32 sync_num_int;
	u32 sync_den_int;
	u32 sync_num_dec;
	u32 sync_den_dec;
};

enum hdmi_hdr_transfer {
	T_UNKNOWN = 0,
	T_BT709,
	T_UNDEF,
	T_BT601,
	T_BT470M,
	T_BT470BG,
	T_SMPTE170M,
	T_SMPTE240M,
	T_LINEAR,
	T_LOG100,
	T_LOG316,
	T_IEC61966_2_4,
	T_BT1361E,
	T_IEC61966_2_1,
	T_BT2020_10,
	T_BT2020_12,
	T_SMPTE_ST_2084,
	T_SMPTE_ST_28,
	T_HLG,
};

enum hdmi_hdr_color {
	C_UNKNOWN = 0,
	C_BT709,
	C_UNDEF,
	C_BT601,
	C_BT470M,
	C_BT470BG,
	C_SMPTE170M,
	C_SMPTE240M,
	C_FILM,
	C_BT2020,
};

#define EDID_MAX_BLOCK              4
#define HDMI_TMP_BUF_SIZE           1024
struct hdmitx_dev {
	struct cdev cdev; /* The cdev structure */
	struct proc_dir_entry *proc_file;
	struct task_struct *task;
	struct task_struct *task_monitor;
	struct task_struct *task_hdcp;
	struct notifier_block nb;
	struct workqueue_struct *hdmi_wq;
	struct workqueue_struct *rxsense_wq;
	struct device *hdtx_dev;
	struct delayed_work work_hpd_plugin;
	struct delayed_work work_hpd_plugout;
	struct delayed_work work_rxsense;
	struct work_struct work_internal_intr;
	struct work_struct work_hdr;
	struct delayed_work work_do_hdcp;
#ifdef CONFIG_AML_HDMI_TX_14
	struct delayed_work cec_work;
#endif
	struct timer_list hdcp_timer;
	const char *hpd_pin;
	const char *ddc_pin;
	int chip_type;
	int hdcp_try_times;
	/* -1, no hdcp; 0, NULL; 1, 1.4; 2, 2.2 */
	int hdcp_mode;
	int hdcp_bcaps_repeater;
	int ready;	/* 1, hdmi stable output, others are 0 */
	int hdcp_hpd_stick;	/* 1 not init & reset at plugout */
	struct {
		void (*SetPacket)(int type, unsigned char *DB,
			unsigned char *HB);
		void (*SetAudioInfoFrame)(unsigned char *AUD_DB,
			unsigned char *CHAN_STAT_BUF);
		int (*SetDispMode)(struct hdmitx_dev *hdmitx_device);
		int (*SetAudMode)(struct hdmitx_dev *hdmitx_device,
			struct hdmitx_audpara *audio_param);
		void (*SetupIRQ)(struct hdmitx_dev *hdmitx_device);
		void (*DebugFun)(struct hdmitx_dev *hdmitx_device,
			const char *buf);
		void (*UnInit)(struct hdmitx_dev *hdmitx_device);
		int (*CntlPower)(struct hdmitx_dev *hdmitx_device,
			unsigned int cmd, unsigned int arg); /* Power control */
		/* edid/hdcp control */
		int (*CntlDDC)(struct hdmitx_dev *hdmitx_device,
			unsigned int cmd, unsigned long arg);
		/* Audio/Video/System Status */
		int (*GetState)(struct hdmitx_dev *hdmitx_device,
			unsigned int cmd, unsigned int arg);
		int (*CntlPacket)(struct hdmitx_dev *hdmitx_device,
			unsigned int cmd,
			unsigned int arg); /* Packet control */
		int (*CntlConfig)(struct hdmitx_dev *hdmitx_device,
			unsigned int cmd,
			unsigned int arg); /* Configure control */
		int (*CntlMisc)(struct hdmitx_dev *hdmitx_device,
			unsigned int cmd, unsigned int arg); /* Other control */
		int (*Cntl)(struct hdmitx_dev *hdmitx_device, unsigned int cmd,
			unsigned int arg); /* Other control */
	} HWOp;
	struct {
		unsigned int hdcp14_en;
		unsigned int hdcp14_rslt;
	} hdcpop;
	struct hdmi_config_platform_data config_data;
	enum hdmi_event_t hdmitx_event;
	unsigned int irq_hpd;
	/* wait_queue_head_t   wait_queue;*/
	/*EDID*/
	unsigned int cur_edid_block;
	unsigned int cur_phy_block_ptr;
	unsigned char EDID_buf[EDID_MAX_BLOCK * 128];
	unsigned char EDID_buf1[EDID_MAX_BLOCK*128]; /* for second read */
	unsigned char *edid_ptr;
	unsigned int edid_parsing; /* Indicator that RX edid data integrated */
	unsigned char EDID_hash[20];
	struct rx_cap RXCap;
	struct hdmitx_vidpara *cur_video_param;
	int vic_count;
	/*audio*/
	struct hdmitx_audpara cur_audio_param;
	int audio_param_update_flag;
	/*status*/
#define DISP_SWITCH_FORCE       0
#define DISP_SWITCH_EDID        1
	unsigned char disp_switch_config; /* 0, force; 1,edid */
	unsigned char unplug_powerdown;
	unsigned short physical_addr;
	unsigned int cur_VIC;
	/**/
	unsigned char hpd_event; /* 1, plugin; 2, plugout */
	unsigned char hpd_state; /* 1, connect; 0, disconnect */
	unsigned char force_audio_flag;
	unsigned char mux_hpd_if_pin_high_flag;
	int auth_process_timer;
	struct hdmitx_info hdmi_info;
	unsigned char tmp_buf[HDMI_TMP_BUF_SIZE];
	unsigned int log;
	unsigned int tx_aud_cfg; /* 0, off; 1, on */
	/* For some un-well-known TVs, no edid at all */
	unsigned int tv_no_edid;
	unsigned int hpd_lock;
	struct hdmi_format_para *para;
	/* 0: RGB444  1: Y444  2: Y422  3: Y420 */
	/* 4: 24bit  5: 30bit  6: 36bit  7: 48bit */
	/* if equals to 1, means current video & audio output are blank */
	unsigned int output_blank_flag;
	unsigned int audio_notify_flag;
	unsigned int audio_step;
	unsigned int repeater_tx;
	/* 0.1% clock shift, 1080p60hz->59.94hz */
	unsigned int frac_rate_policy;
	unsigned int rxsense_policy;
	/* configure for I2S: 8ch in, 2ch out */
	/* 0: default setting  1:ch0/1  2:ch2/3  3:ch4/5  4:ch6/7 */
	unsigned int aud_output_ch;
	enum hdmi_hdr_transfer hdr_transfer_feature;
	enum hdmi_hdr_color hdr_color_feature;
	unsigned int sdr_hdr_feature;
	unsigned int flag_3dfp:1;
	unsigned int flag_3dtb:1;
	unsigned int flag_3dss:1;
};

#define CMD_DDC_OFFSET          (0x10 << 24)
#define CMD_STATUS_OFFSET       (0x11 << 24)
#define CMD_PACKET_OFFSET       (0x12 << 24)
#define CMD_MISC_OFFSET         (0x13 << 24)
#define CMD_CONF_OFFSET         (0x14 << 24)
#define CMD_STAT_OFFSET         (0x15 << 24)

/***********************************************************************
 *             DDC CONTROL //CntlDDC
 **********************************************************************/
#define DDC_RESET_EDID          (CMD_DDC_OFFSET + 0x00)
#define DDC_RESET_HDCP          (CMD_DDC_OFFSET + 0x01)
#define DDC_HDCP_OP             (CMD_DDC_OFFSET + 0x02)
	#define HDCP14_ON	0x1
	#define HDCP14_OFF	0x2
	#define HDCP22_ON	0x3
	#define HDCP22_OFF	0x4
#define DDC_IS_HDCP_ON          (CMD_DDC_OFFSET + 0x04)
#define DDC_HDCP_GET_AKSV       (CMD_DDC_OFFSET + 0x05)
#define DDC_HDCP_GET_BKSV       (CMD_DDC_OFFSET + 0x06)
#define DDC_HDCP_GET_AUTH       (CMD_DDC_OFFSET + 0x07)
#define DDC_PIN_MUX_OP          (CMD_DDC_OFFSET + 0x08)
#define PIN_MUX             0x1
#define PIN_UNMUX           0x2
#define DDC_EDID_READ_DATA      (CMD_DDC_OFFSET + 0x0a)
#define DDC_IS_EDID_DATA_READY  (CMD_DDC_OFFSET + 0x0b)
#define DDC_EDID_GET_DATA       (CMD_DDC_OFFSET + 0x0c)
#define DDC_EDID_CLEAR_RAM      (CMD_DDC_OFFSET + 0x0d)
#define DDC_HDCP_MUX_INIT	(CMD_DDC_OFFSET + 0x0e)
#define DDC_HDCP_14_LSTORE	(CMD_DDC_OFFSET + 0x0f)
#define DDC_HDCP_22_LSTORE	(CMD_DDC_OFFSET + 0x10)
#define DDC_SCDC_DIV40_SCRAMB	(CMD_DDC_OFFSET + 0x20)
#define DDC_HDCP14_GET_BCAPS_RP	(CMD_DDC_OFFSET + 0x30)

/***********************************************************************
 *             CONFIG CONTROL //CntlConfig
 **********************************************************************/
/* Video part */
#define CONF_VIDEO_BLANK_OP     (CMD_CONF_OFFSET + 0x00)
#define VIDEO_BLANK         0x1
#define VIDEO_UNBLANK       0x2
#define CONF_HDMI_DVI_MODE      (CMD_CONF_OFFSET + 0x02)
#define HDMI_MODE           0x1
#define DVI_MODE            0x2
#define CONF_SYSTEM_ST          (CMD_CONF_OFFSET + 0x03)
/* Audio part */
#define CONF_CLR_AVI_PACKET     (CMD_CONF_OFFSET + 0x04)
#define CONF_CLR_VSDB_PACKET    (CMD_CONF_OFFSET + 0x05)
#define CONF_VIDEO_MAPPING	(CMD_CONF_OFFSET + 0x06)
#define CONF_GET_HDMI_DVI_MODE	(CMD_CONF_OFFSET + 0x07)

#define CONF_AUDIO_MUTE_OP      (CMD_CONF_OFFSET + 0x1000 + 0x00)
#define AUDIO_MUTE          0x1
#define AUDIO_UNMUTE        0x2
#define CONF_CLR_AUDINFO_PACKET (CMD_CONF_OFFSET + 0x1000 + 0x01)
#define CONF_AVI_BT2020		(CMD_CONF_OFFSET + 0X2000 + 0x00)
	#define CLR_AVI_BT2020	0x0
	#define SET_AVI_BT2020	0x1
/* set value as COLORSPACE_RGB444, YUV422, YUV444, YUV420 */
#define CONF_AVI_RGBYCC_INDIC	(CMD_CONF_OFFSET + 0X2000 + 0x01)
#define CONF_AVI_Q01		(CMD_CONF_OFFSET + 0X2000 + 0x02)
	#define RGB_RANGE_DEFAULT	0
	#define RGB_RANGE_LIM		1
	#define RGB_RANGE_FUL		2
	#define RGB_RANGE_RSVD		3
#define CONF_AVI_YQ01		(CMD_CONF_OFFSET + 0X2000 + 0x03)
	#define YCC_RANGE_LIM		0
	#define YCC_RANGE_FUL		1
	#define YCC_RANGE_RSVD		2
#define CONF_VIDEO_MUTE_OP      (CMD_CONF_OFFSET + 0x1000 + 0x04)
#define VIDEO_MUTE          0x1
#define VIDEO_UNMUTE        0x2

/***********************************************************************
 *             MISC control, hpd, hpll //CntlMisc
 **********************************************************************/
#define MISC_HPD_MUX_OP         (CMD_MISC_OFFSET + 0x00)
#define MISC_HPD_GPI_ST         (CMD_MISC_OFFSET + 0x02)
#define MISC_HPLL_OP            (CMD_MISC_OFFSET + 0x03)
#define		HPLL_ENABLE         0x1
#define		HPLL_DISABLE        0x2
#define		HPLL_SET	    0x3
#define MISC_TMDS_PHY_OP        (CMD_MISC_OFFSET + 0x04)
#define TMDS_PHY_ENABLE     0x1
#define TMDS_PHY_DISABLE    0x2
#define MISC_VIID_IS_USING      (CMD_MISC_OFFSET + 0x05)
#define MISC_CONF_MODE420       (CMD_MISC_OFFSET + 0x06)
#define MISC_TMDS_CLK_DIV40     (CMD_MISC_OFFSET + 0x07)
#define MISC_COMP_HPLL         (CMD_MISC_OFFSET + 0x08)
#define COMP_HPLL_SET_OPTIMISE_HPLL1    0x1
#define COMP_HPLL_SET_OPTIMISE_HPLL2    0x2
#define MISC_COMP_AUDIO         (CMD_MISC_OFFSET + 0x09)
#define COMP_AUDIO_SET_N_6144x2          0x1
#define COMP_AUDIO_SET_N_6144x3          0x2
#define MISC_AVMUTE_OP          (CMD_MISC_OFFSET + 0x0a)
#define MISC_FINE_TUNE_HPLL     (CMD_MISC_OFFSET + 0x0b)
	#define OFF_AVMUTE	0x0
	#define CLR_AVMUTE	0x1
	#define SET_AVMUTE	0x2
#define MISC_HPLL_FAKE			(CMD_MISC_OFFSET + 0x0c)
#define MISC_ESM_RESET		(CMD_MISC_OFFSET + 0x0d)
#define MISC_HDCP_CLKDIS	(CMD_MISC_OFFSET + 0x0e)
#define MISC_TMDS_RXSENSE	(CMD_MISC_OFFSET + 0x0f)
#define MISC_I2C_REACTIVE       (CMD_MISC_OFFSET + 0x10)

/***********************************************************************
 *                          Get State //GetState
 **********************************************************************/
#define STAT_VIDEO_VIC          (CMD_STAT_OFFSET + 0x00)
#define STAT_VIDEO_CLK          (CMD_STAT_OFFSET + 0x01)
#define STAT_AUDIO_FORMAT       (CMD_STAT_OFFSET + 0x10)
#define STAT_AUDIO_CHANNEL      (CMD_STAT_OFFSET + 0x11)
#define STAT_AUDIO_CLK_STABLE   (CMD_STAT_OFFSET + 0x12)
#define STAT_AUDIO_PACK         (CMD_STAT_OFFSET + 0x13)

/* HDMI LOG */
#define HDMI_LOG_HDCP           (1 << 0)

#define HDMI_SOURCE_DESCRIPTION 0
#define HDMI_PACKET_VEND        1
#define HDMI_MPEG_SOURCE_INFO   2
#define HDMI_PACKET_AVI         3
#define HDMI_AUDIO_INFO         4
#define HDMI_AUDIO_CONTENT_PROTECTION   5
#define HDMI_PACKET_HBR         6
#define HDMI_PACKET_DRM		0x86

#define HDMI_PROCESS_DELAY  msleep(10)
/* reduce a little time, previous setting is 4000/10 */
#define AUTH_PROCESS_TIME   (1000/100)

#define HDMITX_VER "20170622"

/***********************************************************************
 *    hdmitx protocol level interface
 **********************************************************************/
extern void hdmitx_init_parameters(struct hdmitx_info *info);
extern enum hdmi_vic hdmitx_edid_vic_tab_map_vic(const char *disp_mode);

extern int hdmitx_edid_parse(struct hdmitx_dev *hdmitx_device);
extern int check_dvi_hdmi_edid_valid(unsigned char *buf);

enum hdmi_vic hdmitx_edid_get_VIC(struct hdmitx_dev *hdmitx_device,
	const char *disp_mode, char force_flag);

extern int hdmitx_edid_VIC_support(enum hdmi_vic vic);

extern int hdmitx_edid_dump(struct hdmitx_dev *hdmitx_device, char *buffer,
	int buffer_len);
bool hdmitx_edid_check_valid_mode(struct hdmitx_dev *hdev,
	struct hdmi_format_para *para);
extern const char *hdmitx_edid_vic_to_string(enum hdmi_vic vic);
extern void hdmitx_edid_clear(struct hdmitx_dev *hdmitx_device);

extern void hdmitx_edid_ram_buffer_clear(struct hdmitx_dev *hdmitx_device);

extern void hdmitx_edid_buf_compare_print(struct hdmitx_dev *hdmitx_device);

extern const char *hdmitx_edid_get_native_VIC(struct hdmitx_dev *hdmitx_device);

/*
 * HDMI Repeater TX I/F
 * RX downstream Information from rptx to rprx
 */
/* send part raw edid from TX to RX */
extern void rx_repeat_hpd_state(unsigned int st);
/* prevent compile error in no HDMIRX case */
void __attribute__((weak))rx_repeat_hpd_state(unsigned int st)
{
}

extern void rx_edid_physical_addr(unsigned char a, unsigned char b,
	unsigned char c, unsigned char d);
void __attribute__((weak))rx_edid_physical_addr(unsigned char a,
	unsigned char b, unsigned char c, unsigned char d)
{
}

extern void rx_set_repeater_support(bool enable);
void __attribute__((weak))rx_set_repeater_support(bool enable)
{
}

extern void rx_set_receiver_edid(unsigned char *data, int len);
void __attribute__((weak))rx_set_receiver_edid(unsigned char *data, int len)
{
}

/*
 * ver = 22 means downstream supports HDCP22
 * ver = 14 means support HDCP14
 * ver = 0 means support NO HDCP
 */
extern void rx_repeat_hdcp_ver(unsigned int ver);
void __attribute__((weak))rx_repeat_hdcp_ver(unsigned int ver)
{
}

extern void rx_set_receive_hdcp(unsigned char *data, int len, int depth,
	bool max_cascade, bool max_devs);
void __attribute__((weak))rx_set_receive_hdcp(unsigned char *data, int len,
	int depth, bool max_cascade, bool max_devs)
{
}

extern int hdmitx_set_display(struct hdmitx_dev *hdmitx_device,
	enum hdmi_vic VideoCode);

extern int hdmi_set_3d(struct hdmitx_dev *hdmitx_device, int type,
	unsigned int param);

extern int hdmitx_set_audio(struct hdmitx_dev *hdmitx_device,
	struct hdmitx_audpara *audio_param, int hdmi_ch);

/* for notify to cec */
#define HDMITX_PLUG			1
#define HDMITX_UNPLUG			2
#define HDMITX_PHY_ADDR_VALID		3

#ifdef CONFIG_AMLOGIC_HDMITX
extern struct hdmitx_dev *get_hdmitx_device(void);
extern int get_hpd_state(void);
extern int hdmitx_event_notifier_regist(struct notifier_block *nb);
extern int hdmitx_event_notifier_unregist(struct notifier_block *nb);
extern void hdmitx_event_notify(unsigned long state, void *arg);
extern void hdmitx_hdcp_status(int hdmi_authenticated);
#else
static inline struct hdmitx_dev *get_hdmitx_device(void)
{
	return NULL;
}
static inline int get_hpd_state(void)
{
	return -1;
}
static inline int hdmitx_event_notifier_regist(struct notifier_block *nb)
{
	return -EINVAL;
}

static inline int hdmitx_event_notifier_unregist(struct notifier_block *nb)
{
	return -EINVAL;
}
#endif

extern int hdmi_print_buf(char *buf, int len);

extern void hdmi_set_audio_para(int para);

extern void hdmitx_output_rgb(void);

extern int get_cur_vout_index(void);
extern struct vinfo_s *hdmi_get_current_vinfo(void);
void phy_pll_off(void);

extern int get_hpd_state(void);
void hdmitx_hdcp_do_work(struct hdmitx_dev *hdev);

/***********************************************************************
 *    hdmitx hardware level interface
 ***********************************************************************/
/* #define DOUBLE_CLK_720P_1080I */
extern unsigned char hdmi_pll_mode; /* 1, use external clk as hdmi pll source */

extern void HDMITX_Meson_Init(struct hdmitx_dev *hdmitx_device);

extern unsigned char hdmi_audio_off_flag;
extern unsigned int get_hdcp22_base(void);
/*
 * hdmitx_audio_mute_op() is used by external driver call
 * flag: 0: audio off   1: audio_on
 *       2: for EDID auto mode
 */
extern void hdmitx_audio_mute_op(unsigned int flag);
extern void hdmitx_video_mute_op(unsigned int flag);

#define HDMITX_HWCMD_MUX_HPD_IF_PIN_HIGH       0x3
#define HDMITX_HWCMD_TURNOFF_HDMIHW           0x4
#define HDMITX_HWCMD_MUX_HPD                0x5
#define HDMITX_HWCMD_PLL_MODE                0x6
#define HDMITX_HWCMD_TURN_ON_PRBS           0x7
#define HDMITX_FORCE_480P_CLK                0x8
#define HDMITX_GET_AUTHENTICATE_STATE        0xa
#define HDMITX_SW_INTERNAL_HPD_TRIG          0xb
#define HDMITX_HWCMD_OSD_ENABLE              0xf

#define HDMITX_HDCP_MONITOR                  0x11
#define HDMITX_IP_INTR_MASN_RST              0x12
#define HDMITX_EARLY_SUSPEND_RESUME_CNTL     0x14
#define HDMITX_EARLY_SUSPEND             0x1
#define HDMITX_LATE_RESUME               0x2
/* Refer to HDMI_OTHER_CTRL0 in hdmi_tx_reg.h */
#define HDMITX_IP_SW_RST                     0x15
#define TX_CREG_SW_RST      (1<<5)
#define TX_SYS_SW_RST       (1<<4)
#define CEC_CREG_SW_RST     (1<<3)
#define CEC_SYS_SW_RST      (1<<2)
#define HDMITX_AVMUTE_CNTL                   0x19
#define AVMUTE_SET          0   /* set AVMUTE to 1 */
#define AVMUTE_CLEAR        1   /* set AVunMUTE to 1 */
#define AVMUTE_OFF          2   /* set both AVMUTE and AVunMUTE to 0 */
#define HDMITX_CBUS_RST                      0x1A
#define HDMITX_INTR_MASKN_CNTL               0x1B
#define INTR_MASKN_ENABLE   0
#define INTR_MASKN_DISABLE  1
#define INTR_CLEAR          2

#define HDMI_HDCP_DELAYTIME_AFTER_DISPLAY    20      /* unit: ms */

#define HDMITX_HDCP_MONITOR_BUF_SIZE         1024
struct Hdcp_Sub {
	char *hdcp_sub_name;
	unsigned int hdcp_sub_addr_start;
	unsigned int hdcp_sub_len;
};

/***********************************************************************
 *                   hdmi debug printk
 * level: 0 ~ 4     Default is 2
 *      0: ERRor  1: IMPortant  2: INFormative  3: DETtal  4: LOW
 * hdmi_print(ERR, EDID "edid bad\");
 * hdmi_print(IMP, AUD "set audio format: AC-3\n");
 * hdmi_print(DET)
 **********************************************************************/
#define HD          "hdmitx: "
#define VID         HD "video: "
#define AUD         HD "audio: "
#define CEC         HD "cec: "
#define EDID        HD "edid: "
#define HDCP        HD "hdcp: "
#define SYS         HD "system: "
#define HPD         HD "hpd: "

#define ERR         1
#define IMP         2
#define INF         3
#define LOW         4
#define DET         (5, "%s[%d]", __func__, __LINE__)

extern void hdmi_print(int level, const char *fmt, ...);

#define dd()
#ifndef dd
#error delete debug information
#endif
#endif

