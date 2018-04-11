/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmi_rx_drv.h
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

#ifndef _TVHDMI_H
#define _TVHDMI_H

#include <linux/workqueue.h>
#include <linux/extcon.h>


#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/iomap.h>
#include <linux/cdev.h>
#include <linux/irqreturn.h>
#include "../tvin_global.h"
#include "../tvin_frontend.h"
//#include "hdmirx_repeater.h"
//#include "hdmi_rx_pktinfo.h"
//#include "hdmi_rx_edid.h"


#define RX_VER0 "ver.2018-04-11"
/*
 *
 *
 *
 *
 */
#define RX_VER1 "ver.2018/03/20"





/* 50ms timer for hdmirx main loop (HDMI_STATE_CHECK_FREQ is 20) */


#define ABS(x) ((x) < 0 ? -(x) : (x))



#define ESM_KILL_WAIT_TIMES 250
#define str_cmp(buff, str) ((strlen((str)) == strlen((buff))) &&	\
		(strncmp((buff), (str), strlen((str))) == 0))
#define pr_var(str, index) rx_pr("%5d %-30s = %#x\n", (index), #str, (str))
#define var_to_str(var) (#var)
#define var_str_cmp(str, var) str_cmp((str), (#var))
#define set_pr_var(a, b, c, d, e)  (comp_set_pr_var((a), \
	(var_to_str(b)), (&b), (c), (d), (e), (sizeof(b))))




#define PFIFO_SIZE 160

enum chip_id_e {
	CHIP_ID_GXTVBB,
	CHIP_ID_TXL,
	CHIP_ID_TXLX,
	CHIP_ID_TXHD = CHIP_ID_TXLX,
};

struct meson_hdmirx_data {
	enum chip_id_e chip_id;
};

struct hdmirx_dev_s {
	int                         index;
	dev_t                       devt;
	struct cdev                 cdev;
	struct device               *dev;
	struct tvin_parm_s          param;
	struct timer_list           timer;
	struct tvin_frontend_s		frontend;
	unsigned int			irq;
	char					irq_name[12];
	struct mutex			rx_lock;
	/*struct mutex			pd_fifo_lock;*/
	struct clk *modet_clk;
	struct clk *cfg_clk;
	struct clk *acr_ref_clk;
	struct clk *audmeas_clk;
	struct clk *aud_out_clk;
	struct clk *esm_clk;
	struct clk *skp_clk;
	const struct meson_hdmirx_data *data;
};

#define HDMI_IOC_MAGIC 'H'
#define HDMI_IOC_HDCP_ON	_IO(HDMI_IOC_MAGIC, 0x01)
#define HDMI_IOC_HDCP_OFF	_IO(HDMI_IOC_MAGIC, 0x02)
#define HDMI_IOC_EDID_UPDATE	_IO(HDMI_IOC_MAGIC, 0x03)
#define HDMI_IOC_PC_MODE_ON		_IO(HDMI_IOC_MAGIC, 0x04)
#define HDMI_IOC_PC_MODE_OFF	_IO(HDMI_IOC_MAGIC, 0x05)
#define HDMI_IOC_HDCP22_AUTO	_IO(HDMI_IOC_MAGIC, 0x06)
#define HDMI_IOC_HDCP22_FORCE14 _IO(HDMI_IOC_MAGIC, 0x07)
#define HDMI_IOC_HDMI_20_SET	_IO(HDMI_IOC_MAGIC, 0x08)
#define HDMI_IOC_HDCP_GET_KSV _IOR(HDMI_IOC_MAGIC, 0x09, struct _hdcp_ksv)
#define HDMI_IOC_PD_FIFO_PKTTYPE_EN _IOW(HDMI_IOC_MAGIC, 0x0a,\
	uint32_t)
#define HDMI_IOC_PD_FIFO_PKTTYPE_DIS _IOW(HDMI_IOC_MAGIC, 0x0b,\
		uint32_t)
#define HDMI_IOC_GET_PD_FIFO_PARAM _IOWR(HDMI_IOC_MAGIC, 0x0c,\
	struct pd_infoframe_s)

#define IOC_SPD_INFO  _BIT(0)
#define IOC_AUD_INFO  _BIT(1)
#define IOC_MPEGS_INFO _BIT(2)
#define IOC_AVI_INFO _BIT(3)

enum colorspace_e {
	E_COLOR_RGB,
	E_COLOR_YUV422,
	E_COLOR_YUV444,
	E_COLOR_YUV420,
};

enum colorrange_e {
	E_RANGE_DEFAULT,
	E_RANGE_LIMIT,
	E_RANGE_FULL,
};

enum colordepth_e {
	E_COLORDEPTH_8 = 8,
	E_COLORDEPTH_10 = 10,
	E_COLORDEPTH_12 = 12,
	E_COLORDEPTH_16 = 16,
};

enum port_sts_e {
	E_PORT0,
	E_PORT1,
	E_PORT2,
	E_PORT3,
	E_PORT_NUM,
	E_5V_LOST = 0xfd,
	E_INIT = 0xff,
};

enum hdcp_version_e {
	HDCP_VER_NONE,
	HDCP_VER_14,
	HDCP_VER_22,
};

enum hdmirx_port_e {
	HDMIRX_PORT0,
	HDMIRX_PORT1,
	HDMIRX_PORT2,
	HDMIRX_PORT3,
};

enum map_addr_module_e {
	MAP_ADDR_MODULE_CBUS,
	MAP_ADDR_MODULE_HIU,
	MAP_ADDR_MODULE_HDMIRX_CAPB3,
	MAP_ADDR_MODULE_SEC_AHB,
	MAP_ADDR_MODULE_SEC_AHB2,
	MAP_ADDR_MODULE_APB4,
	MAP_ADDR_MODULE_TOP,
	MAP_ADDR_MODULE_NUM
};

/**
 * @short HDMI RX controller video parameters
 *
 * For Auxiliary Video InfoFrame (AVI) details see HDMI 1.4a section 8.2.2
 */
struct rx_video_info {
	/** DVI detection status: DVI (true) or HDMI (false) */
	bool hw_dvi;

	uint8_t hdcp_type;
	/** bit'0:auth start  bit'1:enc state(0:not endrypted 1:encrypted) **/
	uint8_t hdcp14_state;
	/** 1:decrypted 0:encrypted **/
	uint8_t hdcp22_state;
	/** Deep color mode: 8, 10, 12 or 16 [bits per pixel] */
	uint8_t colordepth;
	/** Pixel clock frequency [kHz] */
	uint32_t pixel_clk;
	/** Refresh rate [0.01Hz] */
	uint32_t frame_rate;
	/** Interlaced */
	bool interlaced;
	/** Vertical offset */
	uint32_t voffset;
	/** Vertical active */
	uint32_t vactive;
	/** Vertical total */
	uint32_t vtotal;
	/** Horizontal offset */
	uint32_t hoffset;
	/** Horizontal active */
	uint32_t hactive;
	/** Horizontal total */
	uint32_t htotal;
	/** AVI Y1-0, video format */
	uint8_t colorspace;
	/** AVI VIC6-0, video identification code */
	uint8_t hw_vic;
	/** AVI PR3-0, pixel repetition factor */
	uint8_t repeat;
	/* for sw info */
	uint8_t sw_vic;
	uint8_t sw_dvi;
	unsigned int it_content;
	/** AVI Q1-0, RGB quantization range */
	unsigned int rgb_quant_range;
	/** AVI Q1-0, YUV quantization range */
	unsigned int yuv_quant_range;
	bool sw_fp;
	bool sw_alternative;
};

/** Receiver key selection size - 40 bits */
#define HDCP_BKSV_SIZE	(2 *  1)
/** Encrypted keys size - 40 bits x 40 keys */
#define HDCP_KEYS_SIZE	(2 * 40)

/**
 * @short HDMI RX controller HDCP configuration
 */
struct hdmi_rx_hdcp {
	/** Repeater mode else receiver only */
	bool repeat;
	bool cascade_exceed;
	bool dev_exceed;
	/*downstream depth*/
	unsigned char depth;
	/*downstream count*/
	uint32_t count;
	/** Key description seed */
	uint32_t seed;
	uint32_t delay;/*according to the timer,5s*/
	/**
	 * Receiver key selection
	 * @note 0: high order, 1: low order
	 */
	uint32_t bksv[HDCP_BKSV_SIZE];
	/**
	 * Encrypted keys
	 * @note 0: high order, 1: low order
	 */
	uint32_t keys[HDCP_KEYS_SIZE];
	struct extcon_dev *rx_excton_auth;
	enum hdcp_version_e hdcp_version;/* 0 no hdcp;1 hdcp14;2 hdcp22 */
	unsigned char hdcp22_exception;/*esm exception code,reg addr :0x60*/
};

static const unsigned int rx22_ext[] = {
	EXTCON_DISP_HDMI,
	EXTCON_NONE,
};
struct vsi_info_s {
	unsigned int identifier;
	unsigned char vd_fmt;
	unsigned char _3d_structure;
	unsigned char _3d_ext_data;
	bool dolby_vision;
	bool low_latency;
	bool backlt_md_bit;
	unsigned int dolby_timeout;
	unsigned int eff_tmax_pq;
};

#define CHANNEL_STATUS_SIZE   24

struct aud_info_s {
    /* info frame*/
	/*
	 *unsigned char cc;
	 *unsigned char ct;
	 *unsigned char ss;
	 *unsigned char sf;
	 */

	int coding_type;
	int channel_count;
	int sample_frequency;
	int sample_size;
	int coding_extension;
	int channel_allocation;
	/*
	 *int down_mix_inhibit;
	 *int level_shift_value;
	 */

	int aud_packet_received;

	/* channel status */
	unsigned char channel_status[CHANNEL_STATUS_SIZE];
	unsigned char channel_status_bak[CHANNEL_STATUS_SIZE];
    /**/
	unsigned int cts;
	unsigned int n;
	unsigned int arc;
	/**/
	int real_channel_num;
	int real_sample_size;
	int real_sr;
};

struct rx_s {
	enum chip_id_e chip_id;
	/** HDMI RX received signal changed */
	uint8_t skip;
	/*avmute*/
	uint32_t avmute_skip;
	/** HDMI RX input port 0 (A) or 1 (B) (or 2(C) or 3 (D)) */
	uint8_t port;
	/* first boot flag */
	/* workaround for xiaomi-MTK box: */
	/* if box is under suspend and it worked at hdcp2.2 mode before, */
	/* must do hpd reset and keep hpd low at least 2S to ensure hdcp2.2 */
	/* work normally, otherwise mibox's hdcp22 module will pull down SDA */
	/* and stop EDID communication.*/
	/* compare with LG & LETV, the result is the same. */
	bool boot_flag;
	bool open_fg;
	uint8_t irq_flag;
	/** HDMI RX controller HDCP configuration */
	struct hdmi_rx_hdcp hdcp;
	/*report hpd status to app*/
	struct extcon_dev *rx_excton_rx22;

	/* wrapper */
	unsigned int state;
	unsigned int pre_state;
	/* recovery mode */
	unsigned char err_rec_mode;
	unsigned char err_code;
	unsigned char pre_5v_sts;
	unsigned char cur_5v_sts;
	bool no_signal;
	unsigned char err_cnt;
	uint16_t wait_no_sig_cnt;
	int aud_sr_stable_cnt;
	int aud_sr_unstable_cnt;
	unsigned long int timestamp;
	unsigned long int stable_timestamp;
	unsigned long int unready_timestamp;
	/* info */
	struct rx_video_info pre;
	struct rx_video_info cur;
	struct aud_info_s aud_info;
	struct vsi_info_s vs_info_details;
	struct tvin_hdr_info_s hdr_info;
	unsigned int pwr_sts;
	/* for debug */
	/*struct pd_infoframe_s dbg_info;*/
};

struct _hdcp_ksv {
	uint32_t bksv0;
	uint32_t bksv1;
};

struct reg_map {
	unsigned int phy_addr;
	unsigned int size;
	void __iomem *p;
	int flag;
};

/* system */
extern struct delayed_work	eq_dwork;
extern struct workqueue_struct	*eq_wq;
extern struct delayed_work	esm_dwork;
extern struct workqueue_struct	*esm_wq;
extern struct delayed_work	repeater_dwork;
extern struct workqueue_struct	*repeater_wq;
extern struct tasklet_struct rx_tasklet;
extern struct device *hdmirx_dev;
extern struct rx_s rx;
extern struct reg_map reg_maps[MAP_ADDR_MODULE_NUM];
extern void rx_tasklet_handler(unsigned long arg);


/* reg */


/* hotplug */
extern unsigned int pwr_sts;
extern int pre_port;
extern void rx_set_hpd(bool en);
extern unsigned int rx_get_hdmi5v_sts(void);
extern unsigned int rx_get_hpd_sts(void);
extern void hotplug_wait_query(void);
extern void rx_send_hpd_pulse(void);

/* irq */
extern void rx_irq_en(bool enable);
extern irqreturn_t irq_handler(int irq, void *params);

/* user interface */
extern int pc_mode_en;
extern int it_content;
extern int rgb_quant_range;
extern int yuv_quant_range;
extern int en_4k_timing;
extern bool en_4096_2_3840;
extern int en_4k_2_2k;
extern bool hdmi_cec_en;
extern int hdmi_yuv444_enable;
extern int skip_frame_cnt;
/* debug */
extern bool hdcp_enable;
extern int log_level;
extern int sm_pause;
extern int rx_set_global_variable(const char *buf, int size);
extern void rx_get_global_variable(const char *buf);
extern int rx_pr(const char *fmt, ...);
extern unsigned int hdmirx_hw_dump_reg(unsigned char *buf, int size);
extern unsigned int hdmirx_show_info(unsigned char *buf, int size);
extern bool is_afifo_error(void);
extern bool is_aud_pll_error(void);
extern int hdmirx_debug(const char *buf, int size);
extern void dump_reg(void);
extern void dump_edid_reg(void);
extern void dump_state(unsigned char enable);
extern void rx_debug_loadkey(void);
extern void rx_debug_load22key(void);
extern int rx_debug_wr_reg(const char *buf, char *tmpbuf, int i);
extern int rx_debug_rd_reg(const char *buf, char *tmpbuf);

/* repeater */
bool hdmirx_repeat_support(void);

/* edid-hdcp14 */
extern unsigned int edid_update_flag;

extern void hdmirx_fill_edid_buf(const char *buf, int size);
extern unsigned int hdmirx_read_edid_buf(char *buf, int max_size);
extern void hdmirx_fill_key_buf(const char *buf, int size);

/* packets */
extern int dv_nopacket_timeout;
extern unsigned int packet_fifo_cfg;
extern unsigned int *pd_fifo_buf;



/* for other modules */
extern int External_Mute(int mute_flag);
extern void vdac_enable(bool on, unsigned int module_sel);
extern int rx_is_hdcp22_support(void);
#endif
