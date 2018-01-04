/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmi_rx_wrapper.c
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
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>

/* Local include */
#include "hdmirx_drv.h"
#include "hdmi_rx_reg.h"
#include "hdmi_rx_eq.h"

static int pll_unlock_cnt;
static int pll_unlock_max = 30;
MODULE_PARM_DESC(pll_unlock_max, "\n pll_unlock_max\n");
module_param(pll_unlock_max, int, 0664);

static int pll_lock_cnt;
static int pll_lock_max = 2;
MODULE_PARM_DESC(pll_lock_max, "\n pll_lock_max\n");
module_param(pll_lock_max, int, 0664);

static int dwc_rst_wait_cnt;
static int dwc_rst_wait_cnt_max = 1;
MODULE_PARM_DESC(dwc_rst_wait_cnt_max, "\n dwc_rst_wait_cnt_max\n");
module_param(dwc_rst_wait_cnt_max, int, 0664);

static int sig_stable_cnt;
static int sig_stable_max = 10;
MODULE_PARM_DESC(sig_stable_max, "\n sig_stable_max\n");
module_param(sig_stable_max, int, 0664);

static bool clk_debug;
MODULE_PARM_DESC(clk_debug, "\n clk_debug\n");
module_param(clk_debug, bool, 0664);

static int hpd_wait_cnt;
/* increase time of hpd low, to avoid some source like */
/* MTK box i2c communicate error */
static int hpd_wait_max = 20;
MODULE_PARM_DESC(hpd_wait_max, "\n hpd_wait_max\n");
module_param(hpd_wait_max, int, 0664);

static int sig_unstable_cnt;
static int sig_unstable_max = 80;
MODULE_PARM_DESC(sig_unstable_max, "\n sig_unstable_max\n");
module_param(sig_unstable_max, int, 0664);

static int sig_unready_cnt;
static int sig_unready_max = 5;/* 10; */
MODULE_PARM_DESC(sig_unready_max, "\n sig_unready_max\n");
module_param(sig_unready_max, int, 0664);

static int pow5v_max_cnt = 3;
MODULE_PARM_DESC(pow5v_max_cnt, "\n pow5v_max_cnt\n");
module_param(pow5v_max_cnt, int, 0664);

int rgb_quant_range;
MODULE_PARM_DESC(rgb_quant_range, "\n rgb_quant_range\n");
module_param(rgb_quant_range, int, 0664);

int yuv_quant_range;
MODULE_PARM_DESC(yuv_quant_range, "\n yuv_quant_range\n");
module_param(yuv_quant_range, int, 0664);

int it_content;
MODULE_PARM_DESC(it_content, "\n it_content\n");
module_param(it_content, int, 0664);

/* timing diff offset */
static int diff_pixel_th = 2;
static int diff_line_th = 10;
static int diff_frame_th = 40; /* (25hz-24hz)/2 = 50/100 */
MODULE_PARM_DESC(diff_pixel_th, "\n diff_pixel_th\n");
module_param(diff_pixel_th, int, 0664);
MODULE_PARM_DESC(diff_line_th, "\n diff_line_th\n");
module_param(diff_line_th, int, 0664);
MODULE_PARM_DESC(diff_frame_th, "\n diff_frame_th\n");
module_param(diff_frame_th, int, 0664);

static int port_map = 0x4231;
MODULE_PARM_DESC(port_map, "\n port_map\n");
module_param(port_map, int, 0664);

static int err_dbg_cnt;
static int err_dbg_cnt_max = 500;
MODULE_PARM_DESC(err_dbg_cnt_max, "\n err_dbg_cnt_max\n");
module_param(err_dbg_cnt_max, int, 0664);

static int edid_mode;
MODULE_PARM_DESC(edid_mode, "\n edid_mode\n");
module_param(edid_mode, int, 0664);

int force_vic;
MODULE_PARM_DESC(force_vic, "\n force_vic\n");
module_param(force_vic, int, 0664);

static int aud_sr_stb_max = 20;
MODULE_PARM_DESC(aud_sr_stb_max, "\n aud_sr_stb_max\n");
module_param(aud_sr_stb_max, int, 0664);

/* used in other module */
static int audio_sample_rate;
MODULE_PARM_DESC(audio_sample_rate, "\n audio_sample_rate\n");
module_param(audio_sample_rate, int, 0664);

static int auds_rcv_sts;
MODULE_PARM_DESC(auds_rcv_sts, "\n auds_rcv_sts\n");
module_param(auds_rcv_sts, int, 0664);

static int audio_coding_type;
MODULE_PARM_DESC(audio_coding_type, "\n audio_coding_type\n");
module_param(audio_coding_type, int, 0664);

static int audio_channel_count;
MODULE_PARM_DESC(audio_channel_count, "\n audio_channel_count\n");
module_param(audio_channel_count, int, 0664);
/* used in other module */

int log_level = LOG_EN;/*| HDCP_LOG;*/
MODULE_PARM_DESC(log_level, "\n log_level\n");
module_param(log_level, int, 0664);

static bool auto_switch_off;	/* only for hardware test */
MODULE_PARM_DESC(auto_switch_off, "\n auto_switch_off\n");
module_param(auto_switch_off, bool, 0664);

int clk_unstable_cnt;
static int clk_unstable_max = 100;
MODULE_PARM_DESC(clk_unstable_max, "\n clk_unstable_max\n");
module_param(clk_unstable_max, int, 0664);

int clk_stable_cnt;
static int clk_stable_max = 3;
MODULE_PARM_DESC(clk_stable_max, "\n clk_stable_max\n");
module_param(clk_stable_max, int, 0664);

static int unnormal_wait_max = 200;
MODULE_PARM_DESC(unnormal_wait_max, "\n unnormal_wait_max\n");
module_param(unnormal_wait_max, int, 0664);

static int wait_no_sig_max = 600;
MODULE_PARM_DESC(wait_no_sig_max, "\n wait_no_sig_max\n");
module_param(wait_no_sig_max, int, 0664);

/*edid original data from device*/
static unsigned char receive_hdr_lum[MAX_HDR_LUMI];
static int hdr_lum_len = MAX_HDR_LUMI;
MODULE_PARM_DESC(receive_hdr_lum, "\n receive_hdr_lum\n");
module_param_array(receive_hdr_lum, byte, &hdr_lum_len, 0664);

static bool new_hdr_lum;
MODULE_PARM_DESC(new_hdr_lum, "\n new_hdr_lum\n");
module_param(new_hdr_lum, bool, 0664);

/* to inform ESM whether the cable is connected or not */
bool hpd_to_esm;
MODULE_PARM_DESC(hpd_to_esm, "\n hpd_to_esm\n");
module_param(hpd_to_esm, bool, 0664);

bool hdcp22_kill_esm;
MODULE_PARM_DESC(hdcp22_kill_esm, "\n hdcp22_kill_esm\n");
module_param(hdcp22_kill_esm, bool, 0664);

static int hdcp22_capable_sts = 0xff;
MODULE_PARM_DESC(hdcp22_capable_sts, "\n hdcp22_capable_sts\n");
module_param(hdcp22_capable_sts, int, 0664);

static bool esm_auth_fail_en;
MODULE_PARM_DESC(esm_auth_fail_en, "\n esm_auth_fail_en\n");
module_param(esm_auth_fail_en, bool, 0664);

static int hdcp22_reset_max = 20;
MODULE_PARM_DESC(hdcp22_reset_max, "\n hdcp22_reset_max\n");
module_param(hdcp22_reset_max, int, 0664);

static int hdcp22_auth_sts = 0xff;
MODULE_PARM_DESC(hdcp22_auth_sts, "\n hdcp22_auth_sts\n");
module_param(hdcp22_auth_sts, int, 0664);

/*the esm reset flag for hdcp_rx22*/
bool esm_reset_flag;
MODULE_PARM_DESC(esm_reset_flag, "\n esm_reset_flag\n");
module_param(esm_reset_flag, bool, 0664);

/* to inform ESM whether the cable is connected or not */
bool video_stable_to_esm;
MODULE_PARM_DESC(video_stable_to_esm, "\n video_stable_to_esm\n");
module_param(video_stable_to_esm, bool, 0664);

static bool hdcp_mode_sel;
MODULE_PARM_DESC(hdcp_mode_sel, "\n hdcp_mode_sel\n");
module_param(hdcp_mode_sel, bool, 0664);

static bool hdcp_auth_status;
MODULE_PARM_DESC(hdcp_auth_status, "\n hdcp_auth_status\n");
module_param(hdcp_auth_status, bool, 0664);

static int loadkey_22_hpd_delay = 110;
MODULE_PARM_DESC(loadkey_22_hpd_delay, "\n loadkey_22_hpd_delay\n");
module_param(loadkey_22_hpd_delay, int, 0664);

static int wait_hdcp22_cnt = 900;
MODULE_PARM_DESC(wait_hdcp22_cnt, "\n wait_hdcp22_cnt\n");
module_param(wait_hdcp22_cnt, int, 0664);

static int wait_hdcp22_cnt1 = 200;
MODULE_PARM_DESC(wait_hdcp22_cnt1, "\n wait_hdcp22_cnt1\n");
module_param(wait_hdcp22_cnt1, int, 0664);

static int wait_hdcp22_cnt2 = 50;
MODULE_PARM_DESC(wait_hdcp22_cnt2, "\n wait_hdcp22_cnt2\n");
module_param(wait_hdcp22_cnt2, int, 0664);

static int wait_hdcp22_cnt3 = 900;
MODULE_PARM_DESC(wait_hdcp22_cnt3, "\n wait_hdcp22_cnt3\n");
module_param(wait_hdcp22_cnt3, int, 0664);

static int enable_hdcp22_loadkey = 1;
MODULE_PARM_DESC(enable_hdcp22_loadkey, "\n enable_hdcp22_loadkey\n");
module_param(enable_hdcp22_loadkey, int, 0664);

int do_esm_rst_flag;
MODULE_PARM_DESC(do_esm_rst_flag, "\n do_esm_rst_flag\n");
module_param(do_esm_rst_flag, int, 0664);

bool enable_hdcp22_esm_log;
MODULE_PARM_DESC(enable_hdcp22_esm_log, "\n enable_hdcp22_esm_log\n");
module_param(enable_hdcp22_esm_log, bool, 0664);

bool hdcp22_firmware_ok_flag = 1;
MODULE_PARM_DESC(hdcp22_firmware_ok_flag, "\n hdcp22_firmware_ok_flag\n");
module_param(hdcp22_firmware_ok_flag, bool, 0664);

int esm_err_force_14;
MODULE_PARM_DESC(esm_err_force_14, "\n esm_err_force_14\n");
module_param(esm_err_force_14, int, 0664);

static int reboot_esm_done;
MODULE_PARM_DESC(reboot_esm_done, "\n reboot_esm_done\n");
module_param(reboot_esm_done, int, 0664);

int esm_reboot_lvl = 1;
MODULE_PARM_DESC(esm_reboot_lvl, "\n esm_reboot_lvl\n");
module_param(esm_reboot_lvl, int, 0664);

int enable_esm_reboot;
MODULE_PARM_DESC(enable_esm_reboot, "\n enable_esm_reboot\n");
module_param(enable_esm_reboot, int, 0664);

bool esm_error_flag;
MODULE_PARM_DESC(esm_error_flag, "\n esm_error_flag\n");
module_param(esm_error_flag, bool, 0664);

unsigned int esm_data_base_addr;
/* MODULE_PARM_DESC(esm_data_base_addr,"\n esm_data_base_addr\n"); */
/* module_param(esm_data_base_addr, unsigned, 0664); */

bool hdcp22_esm_reset2;
module_param(hdcp22_esm_reset2, bool, 0664);
MODULE_PARM_DESC(hdcp22_esm_reset2, "hdcp22_esm_reset2");

bool hdcp22_stop_auth;
module_param(hdcp22_stop_auth, bool, 0664);
MODULE_PARM_DESC(hdcp22_stop_auth, "hdcp22_stop_auth");

/*esm recovery mode for changing resolution & hdmi2.0*/
int esm_recovery_mode = ESM_REC_MODE_TMDS;
module_param(esm_recovery_mode, int, 0664);
MODULE_PARM_DESC(esm_recovery_mode, "esm_recovery_mode");

int stable_check_lvl = 0x7ff;
module_param(stable_check_lvl, int, 0664);
MODULE_PARM_DESC(stable_check_lvl, "stable_check_lvl");

/* If dvd source received the frequent pulses on HPD line,
 *It will sent a length of dirty audio data sometimes.it's TX's issues.
 *Compared with other brands TV, delay 1.5S to avoid this noise.
 */
int edid_update_delay = 150;
module_param(edid_update_delay, int, 0664);
MODULE_PARM_DESC(edid_update_delay, "edid_update_delay");

bool hdcp22_reauth_enable;
int edid_update_flag;
bool hdcp22_stop_auth_enable;
bool hdcp22_esm_reset2_enable;
int sm_pause;
int pre_port = 0xff;
/*uint32_t irq_flag;*/
/*for some device pll unlock too long,send a hpd reset*/
bool hdmi5v_lost_flag;

/*------------------------external function------------------------------*/
/*------------------------external function end------------------------------*/
struct rx_s rx;

static unsigned char edid_temp[EDID_SIZE];
static char edid_buf[MAX_EDID_BUF_SIZE];
static int edid_size;

/* hdmi1.4 edid */
static unsigned char edid_14[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x52, 0x74, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x19, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x54,
0x45, 0x53, 0x54, 0x0A, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xEE,
0x02, 0x03, 0x34, 0xF0, 0x55, 0x5F, 0x10, 0x1F,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x5D, 0x26, 0x09, 0x07, 0x03, 0x15, 0x07,
0x50, 0x83, 0x01, 0x00, 0x00, 0x6E, 0x03, 0x0C,
0x00, 0x10, 0x00, 0x98, 0x3C, 0x20, 0x80, 0x80,
0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x96,
};
/* 1.4 edid,support dobly,MAT,DTS,ATMOS*/
static unsigned char edid_14_aud[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x52, 0x74, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x19, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x54,
0x45, 0x53, 0x54, 0x0A, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xEE,
0x02, 0x03, 0x40, 0xF0, 0x55, 0x5F, 0x10, 0x1F,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x5D, 0x32, 0x09, 0x07, 0x03, 0x15, 0x07,
0x50, 0x55, 0x07, 0x01, 0x67, 0x7E, 0x01, 0x0F,
0x7F, 0x07, 0x3D, 0x07, 0x50, 0x83, 0x01, 0x00,
0x00, 0x6E, 0x03, 0x0C, 0x00, 0x10, 0x00, 0x98,
0x3C, 0x20, 0x80, 0x80, 0x01, 0x02, 0x03, 0x04,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12,
};
/* 1.4 edid,support 420 capability map block*/
static unsigned char edid_14_420c[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x52, 0x74, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x19, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x54,
0x45, 0x53, 0x54, 0x0A, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xEE,
0x02, 0x03, 0x3E, 0xF0, 0x59, 0x5F, 0x10, 0x1F,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x5D, 0x61, 0x60, 0x65, 0x66, 0x26, 0x09,
0x07, 0x03, 0x15, 0x07, 0x50, 0x83, 0x01, 0x00,
0x00, 0x6E, 0x03, 0x0C, 0x00, 0x10, 0x00, 0x98,
0x3C, 0x20, 0x80, 0x80, 0x01, 0x02, 0x03, 0x04,
0xE5, 0x0F, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x27,
};

/* 1.4 edid,support 420  video data block*/
static unsigned char edid_14_420vd[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x52, 0x74, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x19, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x54,
0x45, 0x53, 0x54, 0x0A, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xEE,
0x02, 0x03, 0x3A, 0xF0, 0x55, 0x5F, 0x10, 0x1F,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x5D, 0x26, 0x09, 0x07, 0x03, 0x15, 0x07,
0x50, 0x83, 0x01, 0x00, 0x00, 0x6E, 0x03, 0x0C,
0x00, 0x10, 0x00, 0x98, 0x3C, 0x20, 0x80, 0x80,
0x01, 0x02, 0x03, 0x04, 0xE5, 0x0E, 0x61, 0x60,
0x65, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
};

/* 2.0 edid,support HDR,DOLBYVISION */
static unsigned char edid_20[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x52, 0x74, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x19, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x54,
0x45, 0x53, 0x54, 0x0A, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xEE,
0x02, 0x03, 0x60, 0xF0, 0x5C, 0x5F, 0x10, 0x1F,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x5D, 0x60, 0x61, 0x65, 0x66, 0x62, 0x63,
0x64, 0x26, 0x09, 0x07, 0x03, 0x15, 0x07, 0x50,
0x83, 0x01, 0x00, 0x00, 0x6E, 0x03, 0x0C, 0x00,
0x10, 0x00, 0x98, 0x3C, 0x20, 0x80, 0x80, 0x01,
0x02, 0x03, 0x04, 0x67, 0xD8, 0x5D, 0xC4, 0x01,
0x78, 0x88, 0x03, 0xE3, 0x05, 0x40, 0x01, 0xE5,
0x0F, 0x00, 0x00, 0xE0, 0x01, 0xE3, 0x06, 0x05,
0x01, 0xEE, 0x01, 0x46, 0xD0, 0x00, 0x26, 0x0F,
0x8B, 0x00, 0xA8, 0x53, 0x4B, 0x9D, 0x27, 0x0B,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83,
};

unsigned char *edid_list[] = {
	edid_buf,
	edid_14,
	edid_14_aud,
	edid_14_420c,
	edid_14_420vd,
	edid_20,
};

int rx_get_edid_index(void)
{
	if ((edid_mode == 0) &&
		edid_size > 4 &&
		edid_buf[0] == 'E' &&
		edid_buf[1] == 'D' &&
		edid_buf[2] == 'I' &&
		edid_buf[3] == 'D') {
		rx_pr("edid: use Top edid\n");
		return EDID_LIST_BUFF;
	}
	if (edid_mode == 0)
		return EDID_LIST_14;
	else if (edid_mode < EDID_LIST_NUM)
		return edid_mode;
	else
		return EDID_LIST_14;
}

unsigned char *rx_get_edid_buffer(int index)
{
	if (index == EDID_LIST_BUFF)
		return edid_list[index] + 4;
	else
		return edid_list[index];
}

int rx_get_edid_size(int index)
{
	if (index == EDID_LIST_BUFF) {
		if (edid_size > EDID_SIZE + 4)
			edid_size = EDID_SIZE + 4;
		return edid_size - 4;
	} else
		return EDID_SIZE;
}

int rx_set_hdr_lumi(unsigned char *data, int len)
{
	if ((data == NULL) || (len == 0) || (len > MAX_HDR_LUMI))
		return false;

	memcpy(receive_hdr_lum, data, len);
	new_hdr_lum = true;
	return true;
}
EXPORT_SYMBOL(rx_set_hdr_lumi);


unsigned char *rx_get_edid(int edid_index)
{
	/*int edid_index = rx_get_edid_index();*/

	memcpy(edid_temp, rx_get_edid_buffer(edid_index),
				rx_get_edid_size(edid_index));
	/*
	 *rx_pr("index=%d,get size=%d,edid_size=%d,rept=%d\n",
	 *			edid_index,
	 *			rx_get_edid_size(edid_index),
	 *			edid_size, hdmirx_repeat_support());
	 *rx_pr("edid update port map:%#x,up_phy_addr=%#x\n",
	 *			port_map, up_phy_addr);
	 */
	return edid_temp;
}

void rx_edid_update_hdr_info(
							unsigned char *p_edid,
							u_int idx)
{
	u_char hdr_edid[EDID_HDR_SIZE];

	if (p_edid == NULL)
		return;

	/* update hdr info */
	hdr_edid[0] = ((EDID_TAG_HDR >> 3)&0xE0) + (6 & 0x1f);
	hdr_edid[1] = EDID_TAG_HDR & 0xFF;
	memcpy(hdr_edid + 4, receive_hdr_lum,
				sizeof(unsigned char)*3);
	rx_modify_edid(p_edid, rx_get_edid_size(idx),
						hdr_edid);
}

unsigned int rx_edid_cal_phy_addr(
					u_int brepeat,
					u_int up_addr,
					u_int portmap,
					u_char *pedid,
					u_int *phy_offset,
					u_int *phy_addr)
{
	u_int root_offset = 0;
	u_int i;
	u_int flag = 0;

	if (!(pedid && phy_offset && phy_addr))
		return -1;

	for (i = 0; i <= 255; i++) {
		/*find phy_addr_offset*/
		if (pedid[i] == 0x03) {
			if ((pedid[i+1] == 0x0c) &&
				(pedid[i+2] == 0x00) &&
				(pedid[i+4]) == 0x0c) {
				if (brepeat)
					pedid[i+3] = 0x00;
				else
					pedid[i+3] = 0x10;
				*phy_offset = i+3;
				flag = 1;
				break;
			}
		}
	}

	if (brepeat) {
		/*get the root index*/
		i = 0;
		while (i < 4) {
			if (((up_addr << (i*4)) & 0xf000) != 0) {
				root_offset = i;
				break;
			}
			i++;
		}
		if (i == 4)
			root_offset = 4;

		//rx_pr("portmap:%#x,rootoffset=%d,upaddr=%#x\n",
					//portmap,
					///root_offset,
					//up_addr);

		for (i = 0; i < E_PORT_NUM; i++) {
			if (root_offset == 0)
				phy_addr[i] = 0xFFFF;
			else
				phy_addr[i] = (up_addr |
				((((portmap >> i*4) & 0xf) << 12) >>
				(root_offset - 1)*4));

			phy_addr[i] = rx_exchange_bits(phy_addr[i]);
			//rx_pr("port %d phy:%d\n", i, phy_addr[i]);
		}
	} else {
		for (i = 0; i < E_PORT_NUM; i++)
			phy_addr[i] = ((portmap >> i*4) & 0xf) << 4;
	}

	return flag;
}

void rx_edid_fill_to_register(
						u_char *pedid,
						u_int brepeat,
						u_int *pphy_addr,
						u_char *pchecksum)
{
	u_int i;
	u_int checksum = 0;
	u_int value = 0;

	if (!(pedid && pphy_addr && pchecksum))
		return;

	/* clear all and disable all overlay register 0~7*/
	for (i = TOP_EDID_RAM_OVR0; i <= TOP_EDID_RAM_OVR7; ) {
		hdmirx_wr_top(i, 0);
		i += 2;
	}

	/* physical address info at second block */
	for (i = 128; i <= 255; i++) {
		value = pedid[i];
		if (i < 255) {
			checksum += pedid[i];
			checksum &= 0xff;
		} else if (i == 255) {
			value = (0x100 - checksum)&0xff;
		}
	}

	/* physical address info at second block */
	for (i = 0; i <= 255; i++) {
		/* fill first edid buffer */
		hdmirx_wr_top(TOP_EDID_OFFSET + i, pedid[i]);
		/* fill second edid buffer */
		hdmirx_wr_top(0x100+TOP_EDID_OFFSET + i, pedid[i]);
	}
	/* caculate 4 port check sum */
	if (brepeat) {
		for (i = 0; i < E_PORT_NUM; i++) {
			pchecksum[i] = (0x100 + value - (pphy_addr[i] & 0xFF) -
			((pphy_addr[i] >> 8) & 0xFF)) & 0xff;
			/*rx_pr("port %d phy:%d\n", i, pphy_addr[i]);*/
		}
	} else {
		for (i = 0; i < E_PORT_NUM; i++) {
			pchecksum[i] = (0x100 - (checksum +
				(pphy_addr[i] - 0x10))) & 0xff;
			/*rx_pr("port %d phy:%d\n", i, pphy_addr[i]);*/
		}
	}
}

void rx_edid_update_overlay(
						u_int phy_addr_offset,
						u_int *pphy_addr,
						u_char *pchecksum)
{
	//u_int i;

	if (!(pphy_addr && pchecksum))
		return;

	/* replace the first edid ram data */
	/* physical address byte 1 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR2,
		(phy_addr_offset + 1) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR2_DATA,
		((pphy_addr[E_PORT0] >> 8) & 0xFF) |
		 (((pphy_addr[E_PORT1] >> 8) & 0xFF)<<8)
			| (((pphy_addr[E_PORT2] >> 8) & 0xFF)<<16)
			| (((pphy_addr[E_PORT3] >> 8) & 0xFF)<<24));
	/* physical address byte 0 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR1,
		phy_addr_offset | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR1_DATA,
		(pphy_addr[E_PORT0] & 0xFF) | ((pphy_addr[E_PORT1] & 0xFF)<<8) |
			((pphy_addr[E_PORT2] & 0xFF)<<16) |
			((pphy_addr[E_PORT3] & 0xFF) << 24));

	/* checksum */
	hdmirx_wr_top(TOP_EDID_RAM_OVR0,
		0xff | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR0_DATA,
			pchecksum[E_PORT0]|(pchecksum[E_PORT1]<<8)|
			(pchecksum[E_PORT2]<<16) | (pchecksum[E_PORT3] << 24));


	/* replace the second edid ram data */
	/* physical address byte 1 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR5,
		(phy_addr_offset + 0x101) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR5_DATA,
		((pphy_addr[E_PORT0] >> 8) & 0xFF) |
		 (((pphy_addr[E_PORT1] >> 8) & 0xFF)<<8)
			| (((pphy_addr[E_PORT2] >> 8) & 0xFF)<<16)
			| (((pphy_addr[E_PORT3] >> 8) & 0xFF)<<24));
	/* physical address byte 0 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR4,
		(phy_addr_offset + 0x100) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR4_DATA,
		(pphy_addr[E_PORT0] & 0xFF) | ((pphy_addr[E_PORT1] & 0xFF)<<8) |
			((pphy_addr[E_PORT2] & 0xFF)<<16) |
			((pphy_addr[E_PORT3] & 0xFF) << 24));
	/* checksum */
	hdmirx_wr_top(TOP_EDID_RAM_OVR3,
		(0xff + 0x100) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR3_DATA,
			pchecksum[E_PORT0]|(pchecksum[E_PORT1]<<8)|
			(pchecksum[E_PORT2]<<16) | (pchecksum[E_PORT3] << 24));

	//for (i = 0; i < E_PORT_NUM; i++) {
		//rx_pr(">port %d,addr 0x%x,checksum 0x%x\n",
					//i, pphy_addr[i], pchecksum[i]);
	//}
}

int hdmi_rx_ctrl_edid_update(void)
{
	int edid_index = rx_get_edid_index();
	bool brepeat = hdmirx_repeat_support();
	u_char *pedid_data;
	u_int sts;
	u_int phy_addr_offset;
	u_int phy_addr[E_PORT_NUM] = {0, 0, 0};
	u_char checksum[E_PORT_NUM] = {0, 0, 0};

	/* get edid from buffer, return buffer addr */
	pedid_data = rx_get_edid(edid_index);

	/* update hdr info to edid buffer */
	rx_edid_update_hdr_info(pedid_data, edid_index);

	if (brepeat) {
		/* repeater mode */
		rx_edid_update_audio_info(pedid_data,
				rx_get_edid_size(edid_index));
	}

	/* caculate physical address and checksum */
	sts = rx_edid_cal_phy_addr(brepeat,
					up_phy_addr, port_map,
					pedid_data, &phy_addr_offset,
					phy_addr);
	if (!sts) {
		/* not find physical address info */
		rx_pr("err: not finded phy addr info\n");
	}

	/* write edid to edid register */
	rx_edid_fill_to_register(pedid_data, brepeat,
							phy_addr, checksum);
	if (sts) {
		/* update physical and checksum */
		rx_edid_update_overlay(phy_addr_offset, phy_addr, checksum);
	}
	return true;
}

void rx_hpd_to_esm_handle(struct work_struct *work)
{
	cancel_delayed_work(&esm_dwork);
	/* switch_set_state(&rx.hpd_sdev, 0x0); */
	extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 0);
	rx_pr("esm_hpd-0\n");
	mdelay(80);
	/* switch_set_state(&rx.hpd_sdev, 0x01); */
	extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 1);
	rx_pr("esm_hpd-1\n");
}

void hdmirx_dv_packet_stop(void)
{
	if (rx.vsi_info.dolby_vision_sts == DOLBY_VERSION_START) {
		if (((hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD0) & 0xFF) != 0)
		|| ((hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD0) & 0xFF00) != 0))
			return;
		if (rx.vsi_info.dolby_vision) {
			rx.vsi_info.dolby_vision = FALSE;
			rx.vsi_info.packet_stop = 0;
		} else
			rx.vsi_info.packet_stop++;
		if (rx.vsi_info.packet_stop > DV_STOP_PACKET_MAX) {
			rx.vsi_info.dolby_vision_sts =
							DOLBY_VERSION_STOP;
				rx_pr("no dv packet receive stop\n");
			}
	}
}

/*
 *func: irq tasklet
 *param: flag:
 *	0x01:	audio irq
 *	0x02:	packet irq
 */
void rx_tasklet_handler(unsigned long arg)
{
	struct rx_s *prx = (struct rx_s *)arg;
	uint8_t irq_flag = prx->irq_flag;

	prx->irq_flag = 0;

	/*rx_pr("irq_flag = 0x%x\n", irq_flag);*/
	if (irq_flag & IRQ_PACKET_FLAG)
		rx_pkt_handler(PKT_BUFF_SET_FIFO);

	if ((irq_flag & IRQ_AUD_FLAG) &&
		(rx.aud_info.real_sr != 0))
		hdmirx_audio_fifo_rst();

	/*irq_flag = 0;*/
}

static int hdmi_rx_ctrl_irq_handler(void)
{
	int error = 0;
	/* unsigned i = 0; */
	uint32_t intr_hdmi = 0;
	uint32_t intr_md = 0;
	uint32_t intr_pedc = 0;
	/* uint32_t intr_aud_clk = 0; */
	uint32_t intr_aud_fifo = 0;
	uint32_t intr_hdcp22 = 0;
	uint32_t intr_aud_cec = 0;
	bool vsi_handle_flag = false;
	bool drm_handle_flag = false;

	/* clear interrupt quickly */
	intr_hdmi =
	    hdmirx_rd_dwc(DWC_HDMI_ISTS) &
	    hdmirx_rd_dwc(DWC_HDMI_IEN);
	if (intr_hdmi != 0)
		hdmirx_wr_dwc(DWC_HDMI_ICLR, intr_hdmi);

	intr_md =
	    hdmirx_rd_dwc(DWC_MD_ISTS) &
	    hdmirx_rd_dwc(DWC_MD_IEN);
	if (intr_md != 0)
		hdmirx_wr_dwc(DWC_MD_ICLR, intr_md);

	intr_pedc =
	    hdmirx_rd_dwc(DWC_PDEC_ISTS) &
	    hdmirx_rd_dwc(DWC_PDEC_IEN);
	if (intr_pedc != 0)
		hdmirx_wr_dwc(DWC_PDEC_ICLR, intr_pedc);

	intr_aud_fifo =
	    hdmirx_rd_dwc(DWC_AUD_FIFO_ISTS) &
	    hdmirx_rd_dwc(DWC_AUD_FIFO_IEN);
	if (intr_aud_fifo != 0)
		hdmirx_wr_dwc(DWC_AUD_FIFO_ICLR, intr_aud_fifo);

	if (rx.chip_id == CHIP_ID_TXL) {
		intr_aud_cec =
				hdmirx_rd_dwc(DWC_AUD_CEC_ISTS) &
				hdmirx_rd_dwc(DWC_AUD_CEC_IEN);
		if (intr_aud_cec != 0) {
			/*cecrx_irq_handle();*/
			hdmirx_wr_dwc(DWC_AUD_CEC_ICLR, intr_aud_cec);
		}
	}

	if (hdcp22_on) {
		intr_hdcp22 =
			hdmirx_rd_dwc(DWC_HDMI2_ISTS) &
		    hdmirx_rd_dwc(DWC_HDMI2_IEN);
	}

	if (intr_hdcp22 != 0) {
		hdmirx_wr_dwc(DWC_HDMI2_ICLR, intr_hdcp22);
		if (log_level & HDCP_LOG)
			rx_pr("intr=%#x\n", intr_hdcp22);
		switch (intr_hdcp22) {
		case _BIT(0):
			hdcp22_capable_sts = HDCP22_AUTH_STATE_CAPABLE;
			rx.hdcp.hdcp_version = HDCP_VER_22;
			break;
		case _BIT(1):
			hdcp22_capable_sts = HDCP22_AUTH_STATE_NOT_CAPABLE;
			break;
		case _BIT(2):
			hdcp22_auth_sts = HDCP22_AUTH_STATE_LOST;
			break;
		case _BIT(3):
			hdcp22_auth_sts = HDCP22_AUTH_STATE_SUCCESS;
			break;
		case _BIT(4):
			hdcp22_auth_sts = HDCP22_AUTH_STATE_FAILED;
			break;
		default:
			break;
		}
	}

	/* check hdmi open status before dwc isr */
	if (!rx.open_fg) {
		if (log_level & 0x1000)
			rx_pr("[isr] ignore dwc isr ---\n");
		return error;
	}

	if (intr_hdmi != 0) {
		if (get(intr_hdmi, AKSV_RCV) != 0) {
			/*if (log_level & HDCP_LOG)*/
			rx_pr("[*aksv*\n");
			rx.hdcp.hdcp_version = HDCP_VER_14;
			if (hdmirx_repeat_support()) {
				queue_delayed_work(repeater_wq, &repeater_dwork,
						msecs_to_jiffies(5));
				rx_start_repeater_auth();
			}
		}
	}

	if (intr_md != 0) {
		if (get(intr_md, md_ists_en) != 0) {
			if (log_level & 0x100)
				rx_pr("md_ists:%x\n", intr_md);
		}
	}

	if (intr_pedc != 0) {
		if (get(intr_pedc, DVIDET | AVI_CKS_CHG) != 0) {
			if (log_level & 0x400)
				rx_pr("[irq] AVI_CKS_CHG\n");
		}
		if (get(intr_pedc, VSI_RCV) != 0) {
			if (log_level & 0x400)
				rx_pr("[irq] VSI_RCV\n");
			vsi_handle_flag = true;
		}
		if (get(intr_pedc, VSI_CKS_CHG) != 0) {
			if (log_level & 0x400)
				rx_pr("[irq] VSI_CKS_CHG\n");
		}
		if (get(intr_pedc, PD_FIFO_START_PASS) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] FIFO START\n");
			rx.irq_flag |= IRQ_PACKET_FLAG;
		}
		if (get(intr_pedc, _BIT(1)) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] FIFO MAX\n");
		}
		if (get(intr_pedc, _BIT(0)) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] FIFO MIN\n");
		}
		if (!is_meson_txlx_cpu()) {
			if (get(intr_pedc,
				DRM_RCV_EN) != 0) {
				if (log_level & 0x400)
					rx_pr("[irq] DRM_RCV_EN %#x\n",
					intr_pedc);
				drm_handle_flag = true;
			}
		} else {
			if (get(intr_pedc,
				DRM_RCV_EN_TXLX) != 0) {
				if (log_level & 0x400)
					rx_pr("[irq] DRM_RCV_EN %#x\n",
					intr_pedc);
				drm_handle_flag = true;
			}
		}
		if (get(intr_pedc, PD_FIFO_OVERFL) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] PD_FIFO_OVERFL\n");
			rx.irq_flag |= IRQ_PACKET_FLAG;
		}
		if (get(intr_pedc, PD_FIFO_UNDERFL) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] PD_FIFO_UNDFLOW\n");
			rx.irq_flag |= IRQ_PACKET_FLAG;
		}
	}

	if (intr_aud_fifo != 0) {
		if (get(intr_aud_fifo, OVERFL) != 0) {
			if (log_level & 0x100)
				rx_pr("[irq] OVERFL\n");
			//if (rx.aud_info.real_sr != 0)
				//error |= hdmirx_audio_fifo_rst();
		}
		if (get(intr_aud_fifo, UNDERFL) != 0) {
			if (log_level & 0x100)
				rx_pr("[irq] UNDERFL\n");
			//if (rx.aud_info.real_sr != 0)
				//error |= hdmirx_audio_fifo_rst();
		}
	}
	if (vsi_handle_flag)
		rx_pkt_handler(PKT_BUFF_SET_VSI);

	if (drm_handle_flag)
		rx_pkt_handler(PKT_BUFF_SET_DRM);

	if (rx.irq_flag)
		tasklet_schedule(&rx_tasklet);

	return error;
}

irqreturn_t irq_handler(int irq, void *params)
{
	int error = 0;
	unsigned long hdmirx_top_intr_stat;

	if (params == 0) {
		rx_pr("%s: %s\n",
			__func__,
			"RX IRQ invalid parameter");
		return IRQ_HANDLED;
	}

	hdmirx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT);
reisr:hdmirx_wr_top(TOP_INTR_STAT_CLR, hdmirx_top_intr_stat);
	/* modify interrupt flow for isr loading */
	/* top interrupt handler */
	if (hdmirx_top_intr_stat & (1 << 13))
		rx_pr("[isr] auth rise\n");
	if (hdmirx_top_intr_stat & (1 << 14))
		rx_pr("[isr] auth fall\n");
	if (hdmirx_top_intr_stat & (1 << 15))
		rx_pr("[isr] enc rise\n");
	if (hdmirx_top_intr_stat & (1 << 16))
		rx_pr("[isr] enc fall\n");

	/* must clear ip interrupt quickly */
	if (hdmirx_top_intr_stat & (~(1 << 30))) {
		error = hdmi_rx_ctrl_irq_handler();
		if (error < 0) {
			if (error != -EPERM) {
				rx_pr("%s: RX IRQ handler %d\n",
					__func__,
					error);
			}
		}
	}

	/* check the ip interrupt again */
	hdmirx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT);
	if (hdmirx_top_intr_stat & (1 << 31)) {
		if (log_level & 0x100)
			rx_pr("[isr] need clear ip irq---\n");
		goto reisr;

	}
	return IRQ_HANDLED;
}

static const uint32_t sr_tbl[] = {
	32000,
	44100,
	48000,
	88200,
	96000,
	176400,
	192000,
	0
};

static bool check_real_sr_change(void)
{
	uint8_t i;
	bool ret = false;
	/* note: if arc is missmatch with LUT, then return 0 */
	uint32_t ret_sr = 0;

	for (i = 0; sr_tbl[i] != 0; i++) {
		if (abs(rx.aud_info.arc - sr_tbl[i]) < AUD_SR_RANGE) {
			ret_sr = sr_tbl[i];
			break;
		}
	}
	if (ret_sr != rx.aud_info.real_sr) {
		rx.aud_info.real_sr = ret_sr;
		ret = true;
		if (log_level & AUDIO_LOG)
			dump_state(2);
	}
	return ret;
}

#if 0
static unsigned char is_sample_rate_stable(int sample_rate_pre,
					   int sample_rate_cur)
{
	unsigned char ret = 0;

	if (ABS(sample_rate_pre - sample_rate_cur) <
		AUD_SR_RANGE)
		ret = 1;

	return ret;
}
#endif

static const struct freq_ref_s freq_ref[] = {
	/* interlace 420 3d hac vac index */
	/* 420mode */
	{0,	3,	0,	1920,	2160,	HDMI_3840x2160_420},
	{0, 3,	0,	2048,	2160,	HDMI_4096x2160_420},
	{0, 3,	0,	960,	1080,	HDMI_1080p_420},
	/* interlace */
	{1,	0,	0,	720,	240,	HDMI_720x480i},
	{1,	0,	0,	1440,	240,	HDMI_1440x480i},
	{1, 0,	0,	720,	288,	HDMI_720x576i},
	{1, 0,	0,	1440,	288,	HDMI_1440x576i},
	{1, 0,	0,	1920,	540,	HDMI_1080i},
	{1, 0,	2,	1920,	1103,	HDMI_1080i_ALTERNATIVE},
	{1, 0,	1,	1920,	2228,	HDMI_1080i_FRAMEPACKING},

	{0, 0,	0,	1440,	240,	HDMI_1440x240p},
	{0, 0,	0,	2880,	240,	HDMI_2880x240p},
	{0, 0,	0,	1440,	288,	HDMI_1440x288p},
	{0, 0,	0,	2880,	288,	HDMI_2880x288p},

	{0, 0,	0,	720,	480,	HDMI_720x480p},
	{0, 0,	0,	1440,	480,	HDMI_1440x480p},
	{0, 0,	1,	720,	1005,	HDMI_480p_FRAMEPACKING},

	{0, 0,	0,	720,	576,	HDMI_720x576p},
	{0, 0,	0,	1440,	576,	HDMI_1440x576p},
	{0, 0,	1,	720,	1201,	HDMI_576p_FRAMEPACKING},

	{0, 0,	0,	1280,	720,	HDMI_720p},
	{0, 0,	1,	1280,	1470,	HDMI_720p_FRAMEPACKING},

	{0, 0,	0,	1920,	1080,	HDMI_1080p},
	{0, 0,	2,	1920,	2160,	HDMI_1080p_ALTERNATIVE},
	{0, 0,	1,	1920,	2205,	HDMI_1080p_FRAMEPACKING},

	/* vesa format*/
	{0, 0,	0,	640,	480,	HDMI_640_480},
	{0, 0,	0,	720,	400,	HDMI_720_400},
	{0, 0,	0,	800,	600,	HDMI_800_600},
	{0, 0,	0,	1024,	768,	HDMI_1024_768},
	{0, 0,	0,	1280,	768,	HDMI_1280_768},
	{0, 0,	0,	1280,	800,	HDMI_1280_800},
	{0, 0,	0,	1280,	960,	HDMI_1280_960},
	{0, 0,	0,	1280,	1024,	HDMI_1280_1024},
	{0, 0,	0,	1360,	768,	HDMI_1360_768},
	{0, 0,	0,	1366,	768,	HDMI_1366_768},
	{0, 0,	0,	1440,	900,	HDMI_1440_900},
	{0, 0,	0,	1400,	1050,	HDMI_1400_1050},
	{0, 0,	0,	1600,	900,	HDMI_1600_900},
	{0, 0,	0,	1600,	1200,	HDMI_1600_1200},
	{0, 0,	0,	1680,	1050,	HDMI_1680_1050},
	{0, 0,	0,	1920,	1200,	HDMI_1920_1200},

	/* 4k2k mode */
	{0, 0,	0,	3840,	2160,	HDMI_3840x2160},
	{0, 0,	0,	4096,	2160,	HDMI_4096x2160},
	{0, 0,	0,	2560,	1440,	HDMI_2560_1440},
	{0, 0,	1,	2560,	3488,	HDMI_2560_1440},
	{0, 0,	2,	2560,	2986,	HDMI_2560_1440},

	/* for AG-506 */
	{0, 0,	0,	720,	483,	HDMI_720x480p},
	{0, 0,	0,	0,		0,		HDMI_UNKNOWN}
};

enum tvin_sig_fmt_e hdmirx_hw_get_fmt(void)
{
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
	unsigned int vic = rx.pre.sw_vic;

	if (force_vic)
		vic = force_vic;

	switch (vic) {
	case HDMI_640_480:
		fmt = TVIN_SIG_FMT_HDMI_640X480P_60HZ;
		break;
	case HDMI_720x480p:
		fmt = TVIN_SIG_FMT_HDMI_720X480P_60HZ;
		break;
	case HDMI_1440x480p:
		fmt = TVIN_SIG_FMT_HDMI_1440X480P_60HZ;
		break;
	case HDMI_480p_FRAMEPACKING:
		fmt = TVIN_SIG_FMT_HDMI_720X480P_60HZ_FRAME_PACKING;
		break;
	case HDMI_720p:
		fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ;
		break;
	case HDMI_720p_FRAMEPACKING:
		fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ_FRAME_PACKING;
		break;
	case HDMI_1080i:
		fmt = TVIN_SIG_FMT_HDMI_1920X1080I_60HZ;
		break;
	case HDMI_1080i_FRAMEPACKING:
		fmt = TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_FRAME_PACKING;
		break;
	case HDMI_1080i_ALTERNATIVE:
		fmt = TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_ALTERNATIVE;
		break;
	case HDMI_720x480i:
	case HDMI_1440x480i:
		fmt = TVIN_SIG_FMT_HDMI_1440X480I_60HZ;
		break;
	case HDMI_1080p:
	case HDMI_1080p_420:
		fmt = TVIN_SIG_FMT_HDMI_1920X1080P_60HZ;
		break;
	case HDMI_1080p_FRAMEPACKING:
		fmt = TVIN_SIG_FMT_HDMI_1920X1080P_24HZ_FRAME_PACKING;
		break;
	case HDMI_1080p_ALTERNATIVE:
		fmt = TVIN_SIG_FMT_HDMI_1920X1080P_24HZ_ALTERNATIVE;
		break;
	case HDMI_720x576p:
	case HDMI_1440x576p:
		fmt = TVIN_SIG_FMT_HDMI_720X576P_50HZ;
		break;
	case HDMI_576p_FRAMEPACKING:
		fmt = TVIN_SIG_FMT_HDMI_720X576P_50HZ_FRAME_PACKING;
		break;
	case HDMI_720x576i:
	case HDMI_1440x576i:
		fmt = TVIN_SIG_FMT_HDMI_1440X576I_50HZ;
		break;
	case HDMI_1440x240p:
		fmt = TVIN_SIG_FMT_HDMI_1440X240P_60HZ;
		break;
	case HDMI_2880x240p:
		fmt = TVIN_SIG_FMT_HDMI_2880X240P_60HZ;
		break;
	case HDMI_1440x288p:
		fmt = TVIN_SIG_FMT_HDMI_1440X288P_50HZ;
		break;
	case HDMI_2880x288p:
		fmt = TVIN_SIG_FMT_HDMI_2880X288P_50HZ;
		break;
	case HDMI_800_600:
		fmt = TVIN_SIG_FMT_HDMI_800X600_00HZ;
		break;
	case HDMI_1024_768:
		fmt = TVIN_SIG_FMT_HDMI_1024X768_00HZ;
		break;
	case HDMI_720_400:
		fmt = TVIN_SIG_FMT_HDMI_720X400_00HZ;
		break;
	case HDMI_1280_768:
		fmt = TVIN_SIG_FMT_HDMI_1280X768_00HZ;
		break;
	case HDMI_1280_800:
		fmt = TVIN_SIG_FMT_HDMI_1280X800_00HZ;
		break;
	case HDMI_1280_960:
		fmt = TVIN_SIG_FMT_HDMI_1280X960_00HZ;
		break;
	case HDMI_1280_1024:
		fmt = TVIN_SIG_FMT_HDMI_1280X1024_00HZ;
		break;
	case HDMI_1360_768:
		fmt = TVIN_SIG_FMT_HDMI_1360X768_00HZ;
		break;
	case HDMI_1366_768:
		fmt = TVIN_SIG_FMT_HDMI_1366X768_00HZ;
		break;
	case HDMI_1600_1200:
		fmt = TVIN_SIG_FMT_HDMI_1600X1200_00HZ;
		break;
	case HDMI_1600_900:
		fmt = TVIN_SIG_FMT_HDMI_1600X900_60HZ;
		break;
	case HDMI_1920_1200:
		fmt = TVIN_SIG_FMT_HDMI_1920X1200_00HZ;
		break;
	case HDMI_1440_900:
		fmt = TVIN_SIG_FMT_HDMI_1440X900_00HZ;
		break;
	case HDMI_1400_1050:
		fmt = TVIN_SIG_FMT_HDMI_1400X1050_00HZ;
		break;
	case HDMI_1680_1050:
		fmt = TVIN_SIG_FMT_HDMI_1680X1050_00HZ;
		break;
	case HDMI_3840x2160:
	case HDMI_3840x2160_420:
		if (en_4k_timing)
			fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
		else
			fmt = TVIN_SIG_FMT_NULL;
		break;
	case HDMI_4096x2160:
	case HDMI_4096x2160_420:
		if (en_4k_timing)
			fmt = TVIN_SIG_FMT_HDMI_4096_2160_00HZ;
		else
			fmt = TVIN_SIG_FMT_NULL;
		break;
	case HDMI_2560_1440:
		fmt = TVIN_SIG_FMT_HDMI_1920X1200_00HZ;
		break;
	default:
		break;
	}
	return fmt;
}

bool rx_is_sig_ready(void)
{
	if ((rx.state == FSM_SIG_READY) ||
		(force_vic))
		return true;
	else
		return false;
}

bool rx_is_nosig(void)
{
	if (force_vic)
		return false;
	return rx.no_signal;
}

/*
 * check timing info
 */
static bool rx_is_timing_stable(void)
{
	bool ret = true;

	if ((abs(rx.cur.hactive - rx.pre.hactive) > diff_pixel_th) &&
		(stable_check_lvl & HACTIVE_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("hactive(%d=>%d),",
				rx.pre.hactive,
				rx.cur.hactive);
	}
	if ((abs(rx.cur.vactive - rx.pre.vactive) > diff_line_th) &&
		(stable_check_lvl & VACTIVE_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("vactive(%d=>%d),",
				rx.pre.hactive,
				rx.cur.hactive);
	}
	if ((abs(rx.cur.htotal - rx.pre.htotal) > diff_pixel_th) &&
		(stable_check_lvl & HTOTAL_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("htotal(%d=>%d),",
				rx.pre.htotal,
				rx.cur.htotal);
	}
	if ((abs(rx.cur.vtotal - rx.pre.vtotal) > diff_line_th) &&
		(stable_check_lvl & VTOTAL_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("vtotal(%d=>%d),",
				rx.pre.vtotal,
				rx.cur.vtotal);
	}
	if ((rx.pre.colorspace != rx.cur.colorspace) &&
		(stable_check_lvl & COLSPACE_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("colorspace(%d=>%d),",
				rx.pre.colorspace,
				rx.cur.colorspace);
	}
	if ((abs(rx.pre.frame_rate - rx.cur.frame_rate) > diff_frame_th) &&
		(stable_check_lvl & REFRESH_RATE_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("frame_rate(%d=>%d),",
				rx.pre.frame_rate,
				rx.cur.frame_rate);
	}
	if ((rx.pre.repeat != rx.cur.repeat) &&
		(stable_check_lvl & REPEAT_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("repeat(%d=>%d),",
				rx.pre.repeat,
				rx.cur.repeat);
	}
	if ((rx.pre.hw_dvi != rx.cur.hw_dvi) &&
		(stable_check_lvl & DVI_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("dvi(%d=>%d),",
				rx.pre.hw_dvi,
				rx.cur.hw_dvi);
	}
	if ((rx.pre.interlaced != rx.cur.interlaced) &&
		(stable_check_lvl & INTERLACED_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("interlaced(%d=>%d),",
				rx.pre.interlaced,
				rx.cur.interlaced);
	}

	if ((rx.pre.colordepth != rx.cur.colordepth) &&
		(stable_check_lvl & COLOR_DEP_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("colordepth(%d=>%d),",
				rx.pre.colordepth,
				rx.cur.colordepth);
	}
	if ((ret == false) && (log_level & VIDEO_LOG))
		rx_pr("\n");

	if (force_vic)
		ret = false;
	return ret;
}

static int get_timing_fmt(void)
{
	int i;
	int size = sizeof(freq_ref)/sizeof(struct freq_ref_s);

	rx.pre.sw_vic = 0;
	rx.pre.sw_dvi = 0;
	rx.pre.sw_fp = 0;
	rx.pre.sw_alternative = 0;

	for (i = 0; i < size; i++) {
		if (abs(rx.pre.hactive - freq_ref[i].hactive) > diff_pixel_th)
			continue;
		if ((abs(rx.pre.vactive - freq_ref[i].vactive)) > diff_line_th)
			continue;
		if ((rx.pre.colorspace != freq_ref[i].cd420) &&
			(freq_ref[i].cd420 != 0))
			continue;
		if (freq_ref[i].interlace != rx.pre.interlaced)
			continue;
		break;
	}
	if (i == size)
		return i;

	rx.pre.sw_vic = freq_ref[i].vic;
	rx.pre.sw_dvi = rx.pre.hw_dvi;

	return i;
}

static void set_fsm_state(enum fsm_states_e sts)
{
	rx.state = sts;
}

static void signal_status_init(void)
{
	hpd_wait_cnt = 0;
	pll_unlock_cnt = 0;
	pll_lock_cnt = 0;
	sig_unstable_cnt = 0;
	sig_stable_cnt = 0;
	sig_stable_cnt = 0;
	sig_unstable_cnt = 0;
	sig_unready_cnt = 0;
	/*rx.wait_no_sig_cnt = 0;*/
	/* rx.no_signal = false; */
	rx.pre_state = 0;
	/* audio */
	audio_sample_rate = 0;
	audio_coding_type = 0;
	audio_channel_count = 0;
	auds_rcv_sts = 0;
	rx.aud_sr_stable_cnt = 0;
	rx.aud_sr_unstable_cnt = 0;
	rx_aud_pll_ctl(0);
	rx_set_eq_run_state(E_EQ_START);
	rx.err_rec_mode = ERR_REC_EQ_RETRY;
	rx_irq_en(FALSE);
	if (hdcp22_on) {
		if (esm_recovery_mode == ESM_REC_MODE_TMDS)
			rx_esm_tmdsclk_en(FALSE);
		esm_set_stable(FALSE);
	}
	set_scdc_cfg(1, 0);
	rx.state = FSM_INIT;
	rx.err_code = ERR_NONE;
	/*if (hdcp22_on)*/
		/*esm_set_stable(0);*/
	rx.hdcp.hdcp_version = HDCP_VER_NONE;
	rx.skip = 0;
}

void packet_update(void)
{
	/*rx_getaudinfo(&rx.aud_info);*/

	rgb_quant_range = rx.cur.rgb_quant_range;
	yuv_quant_range = rx.cur.yuv_quant_range;
	it_content = rx.cur.it_content;

	auds_rcv_sts = rx.aud_info.aud_packet_received;
	audio_sample_rate = rx.aud_info.real_sr;
	audio_coding_type = rx.aud_info.coding_type;
	audio_channel_count = rx.aud_info.channel_count;

	hdmirx_dv_packet_stop();
}

void hdmirx_hdcp22_reauth(void)
{
	if (hdcp22_reauth_enable) {
		esm_auth_fail_en = 1;
		hdcp22_auth_sts = 0xff;
	}
}

void monitor_hdcp22_sts(void)
{
	/*if the auth lost after the success of authentication*/
	if ((hdcp22_capable_sts == HDCP22_AUTH_STATE_CAPABLE) &&
		((hdcp22_auth_sts == HDCP22_AUTH_STATE_LOST) ||
		(hdcp22_auth_sts == HDCP22_AUTH_STATE_FAILED))) {
		hdmirx_hdcp22_reauth();
		/*rx_pr("\n auth lost force hpd rst\n");*/
	}
}

void rx_dwc_reset(void)
{
	if (log_level & VIDEO_LOG)
		rx_pr("rx_dwc_reset\n");
	/*
	 * hdcp14 sts only be cleared by
	 * 1. hdmi swreset
	 * 2. new AKSV is received
	 */
	if (rx.hdcp.hdcp_version == HDCP_VER_14)
		rx_sw_reset(1);
	else
		rx_sw_reset(2);
	rx_irq_en(TRUE);
	if (hdcp22_on) {
		if (esm_recovery_mode == ESM_REC_MODE_TMDS)
			rx_esm_tmdsclk_en(TRUE);
		esm_set_stable(TRUE);
		if (rx.hdcp.hdcp_version == HDCP_VER_22)
			hdmirx_hdcp22_reauth();
	}
	hdmirx_audio_fifo_rst();
	hdmirx_packet_fifo_rst();
}


int rx_get_cur_hpd_sts(void)
{
	return rx_get_hpd_sts() & (1 << rx.port);
}

bool is_tmds_valid(void)
{
	if (force_vic)
		return true;

	return (rx_get_pll_lock_sts() == 1) ? true : false;
}

void esm_set_reset(bool reset)
{
	if (log_level & HDCP_LOG)
		rx_pr("esm set reset:%d\n", reset);

	esm_reset_flag = reset;
}

void esm_set_stable(bool stable)
{
	if (log_level & HDCP_LOG)
		rx_pr("esm set stable:%d\n", stable);
	video_stable_to_esm = stable;
}

void esm_recovery(void)
{
	if (hdcp22_stop_auth_enable)
		hdcp22_stop_auth = 1;
	if (hdcp22_esm_reset2_enable)
		hdcp22_esm_reset2 = 1;
}

void rx_esm_exception_monitor(void)
{
	int irq_status, exception;

	irq_status = rx_hdcp22_rd_reg(HPI_REG_IRQ_STATUS);
	/*rx_pr("++++++hdcp22 state irq status:%#x\n", reg);*/
	if (irq_status & IRQ_STATUS_UPDATE_BIT) {
		exception =
		rx_hdcp22_rd_reg_bits(HPI_REG_EXCEPTION_STATUS,
						EXCEPTION_CODE);
		if (exception != rx.hdcp.hdcp22_exception) {
			rx_pr("++++++hdcp22 state:%#x,vec:%#x\n",
				rx_hdcp22_rd_reg(HPI_REG_EXCEPTION_STATUS),
				exception);
			rx.hdcp.hdcp22_exception = exception;
		}
	}
}

void esm_rst_monitor(void)
{
	static int esm_rst_cnt;

	if (video_stable_to_esm == 0) {
		if (esm_rst_cnt++ > hdcp22_reset_max) {
			if (log_level & HDCP_LOG)
				rx_pr("esm=1\n");
			esm_set_stable(TRUE);
			esm_rst_cnt = 0;
		}
	}
}

bool is_unnormal_format(uint8_t wait_cnt)
{
	bool ret = false;

	if ((rx.pre.sw_vic == HDMI_UNSUPPORT) ||
		(rx.pre.sw_vic == HDMI_UNKNOWN)) {
		ret = true;
		if (wait_cnt == sig_stable_max)
			rx_pr("*unsupport*\n");
		if (unnormal_wait_max == wait_cnt) {
			dump_state(1);
			ret = false;
		}
	}
	if (rx.pre.sw_dvi == 1) {
		ret = true;
		if (wait_cnt == sig_stable_max)
			rx_pr("*DVI*\n");
		if (unnormal_wait_max == wait_cnt) {
			dump_state(1);
			ret = false;
		}
	}
	if ((rx.pre.hdcp14_state != 3) &&
		(rx.pre.hdcp14_state != 0) &&
		(rx.hdcp.hdcp_version == HDCP_VER_14)) {
		ret = true;
		if (sig_stable_max == wait_cnt)
			rx_pr("hdcp14 unfinished\n");
		if (unnormal_wait_max == wait_cnt) {
			if ((rx.hdcp.bksv[0] == 0) &&
				(rx.hdcp.bksv[1] == 0))
				rx.err_code = ERR_NO_HDCP14_KEY;
			ret = false;
		}
	}
	if ((ret == false) && (wait_cnt != sig_stable_max))
		rx_pr("unnormal_format wait cnt = %d\n",
			wait_cnt-sig_stable_max);
	return ret;
}

void fsm_restart(void)
{
	if (hdcp22_on) {
		if (esm_recovery_mode == ESM_REC_MODE_TMDS)
			rx_esm_tmdsclk_en(FALSE);
		esm_set_stable(FALSE);
	}
	set_scdc_cfg(1, 0);
	rx.state = FSM_INIT;
	rx_pr("force_fsm_init\n");
}

void dump_unnormal_info(void)
{
	if (rx.pre.colorspace != rx.cur.colorspace)
		rx_pr("colorspace:%d-%d\n",
			rx.pre.colorspace,
			rx.cur.colorspace);
	if (rx.pre.colordepth != rx.cur.colordepth)
		rx_pr("colordepth:%d-%d\n",
			rx.pre.colordepth,
			rx.cur.colordepth);
	if (rx.pre.interlaced != rx.cur.interlaced)
		rx_pr("interlace:%d-%d\n",
			rx.pre.interlaced,
			rx.cur.interlaced);
	if (rx.pre.htotal != rx.cur.htotal)
		rx_pr("htotal:%d-%d\n",
			rx.pre.htotal,
			rx.cur.htotal);
	if (rx.pre.hactive != rx.cur.hactive)
		rx_pr("hactive:%d-%d\n",
			rx.pre.hactive,
			rx.cur.hactive);
	if (rx.pre.vtotal != rx.cur.vtotal)
		rx_pr("vtotal:%d-%d\n",
			rx.pre.vtotal,
			rx.cur.vtotal);
	if (rx.pre.vactive != rx.cur.vactive)
		rx_pr("vactive:%d-%d\n",
			rx.pre.vactive,
			rx.cur.vactive);
	if (rx.pre.repeat != rx.cur.repeat)
		rx_pr("repetition:%d-%d\n",
			rx.pre.repeat,
			rx.cur.repeat);
	if (rx.pre.frame_rate != rx.cur.frame_rate)
		rx_pr("frame_rate:%d-%d\n",
			rx.pre.frame_rate,
			rx.cur.frame_rate);
	if (rx.pre.sw_vic != rx.cur.sw_vic)
		rx_pr("sw_vic:%d-%d\n",
			rx.pre.sw_vic,
			rx.cur.sw_vic);
	if (rx.pre.hw_dvi != rx.cur.hw_dvi)
		rx_pr("dvi:%d-%d\n,",
			rx.pre.hw_dvi,
			rx.cur.hw_dvi);
	if (rx.pre.hdcp14_state != rx.cur.hdcp14_state)
		rx_pr("HDCP14 state:%d-%d\n",
			rx.pre.hdcp14_state,
			rx.cur.hdcp14_state);
	if (rx.pre.hdcp22_state != rx.cur.hdcp22_state)
		rx_pr("HDCP22 state:%d-%d\n",
			rx.pre.hdcp22_state,
			rx.cur.hdcp22_state);
}

void rx_send_hpd_pulse(void)
{
	rx_set_hpd(0);
	fsm_restart();
}

static void set_hdcp(struct hdmi_rx_ctrl_hdcp *hdcp, const unsigned char *b_key)
{
	int i, j;
	/*memset(&init_hdcp_data, 0, sizeof(struct hdmi_rx_ctrl_hdcp));*/
	for (i = 0, j = 0; i < 80; i += 2, j += 7) {
		hdcp->keys[i + 1] =
		    b_key[j] | (b_key[j + 1] << 8) | (b_key[j + 2] << 16) |
		    (b_key[j + 3] << 24);
		hdcp->keys[i + 0] =
		    b_key[j + 4] | (b_key[j + 5] << 8) | (b_key[j + 6] << 16);
	}
	hdcp->bksv[1] =
	    b_key[j] | (b_key[j + 1] << 8) | (b_key[j + 2] << 16) |
	    (b_key[j + 3] << 24);
	hdcp->bksv[0] = b_key[j + 4];

}

#if 0
int hdmirx_read_key_buf(char *buf, int max_size)
{
	if (key_size > max_size) {
		rx_pr("Error: %s,key size %d",
				__func__, key_size);
		rx_pr("is larger than the buf size of %d\n", max_size);
		return 0;
	}
	memcpy(buf, key_buf, key_size);
	rx_pr("HDMIRX: read key buf\n");
	return key_size;
}
#endif

/*
 * func: hdmirx_fill_key_buf
 */
void hdmirx_fill_key_buf(const char *buf, int size)
{
	if (size > MAX_KEY_BUF_SIZE) {
		rx_pr("Error: %s,key size %d",
				__func__,
				size);
		rx_pr("is larger than the max size of %d\n",
			MAX_KEY_BUF_SIZE);
		return;
	}
	if (buf[0] == 'k' && buf[1] == 'e' && buf[2] == 'y') {
		set_hdcp(&rx.hdcp, buf + 3);
	} else {
		//memcpy(key_buf, buf, size);
		//key_size = size;
		//rx_pr("HDMIRX: fill key buf, size %d\n", size);
	}
}

int hdmirx_read_edid_buf(char *buf, int max_size)
{
	if (edid_size > max_size) {
		rx_pr("Error: %s,edid size %d",
				__func__,
				edid_size);
		rx_pr(" is larger than the buf size of %d\n",
			max_size);
		return 0;
	}
	memcpy(buf, edid_buf, edid_size);
	rx_pr("HDMIRX: read edid buf\n");
	return edid_size;
}

void hdmirx_fill_edid_buf(const char *buf, int size)
{
	if (size > MAX_EDID_BUF_SIZE) {
		rx_pr("Error: %s,edid size %d",
				__func__,
				size);
		rx_pr(" is larger than the max size of %d\n",
			MAX_EDID_BUF_SIZE);
		return;
	}
	memcpy(edid_buf, buf, size);

	edid_size = size;
	rx_pr("HDMIRX: fill edid buf, size %d\n",
		size);
}

/*
 *debug functions
 */
int hdmirx_hw_dump_reg(unsigned char *buf, int size)
{
	return 0;
}
#if 0
void set_pr_var(charbuff, str, val, index)
{
	char index_c[5] = {'\0'};

	sprintf(index_c, "%d", (index));
	if (str_cmp((buff), (str)) ||
		str_cmp((buff), (index_c))) {
		if (ret == 0)
			(str) = (val);
		pr_var(str, (index));
		return;
	}
	(index)++;
}

void rx_set_global_varaible(const char *buf, int size)
{
	char tmpbuf[60], varbuff[60];
	int i = 0;
	long value = 0;
	int ret = 0;
	int index = 1;

	/* rx_pr("buf: %s size: %#x\n", buf, size); */

	if ((buf == 0) || (size == 0) || (size > 60))
		return;

	memset(tmpbuf, 0, sizeof(tmpbuf));
	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ') &&
					(buf[i] != '\n') && (i < size)) {
		tmpbuf[i] = buf[i];
		i++;
	}
	/*skip the space*/
	while (++i < size) {
		if ((buf[i] != ' ') && (buf[i] != ','))
			break;
	}
	if ((buf[i] == '0') && ((buf[i + 1] == 'x') || (buf[i + 1] == 'X')))
		ret = kstrtol(buf + i + 2, 16, &value);
	else
		ret = kstrtol(buf + i, 10, &value);
	/*rx_pr("tmpbuf: %s value: %#x index:%#x\n", tmpbuf, value, index);*/

	if (ret != 0)
		rx_pr("No value set:%d\n", ret);
	cpy_str(varbuff, dwc_rst_wait_cnt_max);
	set_pr_var(tmpbuf, cpy(dwc_rst_wait_cnt_max), value, index);
	set_pr_var(tmpbuf, dwc_rst_wait_cnt_max, value, index);
	set_pr_var(tmpbuf, sig_stable_max, value, index);
	set_pr_var(tmpbuf, clk_debug, value, index);
	set_pr_var(tmpbuf, hpd_wait_max, value, index);
	set_pr_var(tmpbuf, sig_unstable_max, value, index);
	set_pr_var(tmpbuf, sig_unready_max, value, index);
	set_pr_var(tmpbuf, pow5v_max_cnt, value, index);
	set_pr_var(tmpbuf, rgb_quant_range, value, index);
	set_pr_var(tmpbuf, yuv_quant_range, value, index);
	set_pr_var(tmpbuf, it_content, value, index);
	set_pr_var(tmpbuf, diff_pixel_th, value, index);
	set_pr_var(tmpbuf, diff_line_th, value, index);
	set_pr_var(tmpbuf, diff_frame_th, value, index);
	set_pr_var(tmpbuf, port_map, value, index);
	set_pr_var(tmpbuf, edid_mode, value, index);
	set_pr_var(tmpbuf, force_vic, value, index);
	/* set_pr_var(tmpbuf, force_ready, value, index); */
	set_pr_var(tmpbuf, hdcp22_kill_esm, value, index);
	set_pr_var(tmpbuf, audio_sample_rate, value, index);
	set_pr_var(tmpbuf, auds_rcv_sts, value, index);
	set_pr_var(tmpbuf, audio_coding_type, value, index);
	set_pr_var(tmpbuf, audio_channel_count, value, index);
	set_pr_var(tmpbuf, hdcp22_capable_sts, value, index);
	set_pr_var(tmpbuf, esm_auth_fail_en, value, index);
	set_pr_var(tmpbuf, hdcp22_auth_sts, value, index);
	set_pr_var(tmpbuf, log_level, value, index);
	/* set_pr_var(tmpbuf, frame_skip_en, value, index); */
	set_pr_var(tmpbuf, auto_switch_off, value, index);
	set_pr_var(tmpbuf, clk_unstable_cnt, value, index);
	set_pr_var(tmpbuf, clk_unstable_max, value, index);
	set_pr_var(tmpbuf, clk_stable_cnt, value, index);
	set_pr_var(tmpbuf, clk_stable_max, value, index);
	set_pr_var(tmpbuf, wait_no_sig_max, value, index);
	/*set_pr_var(tmpbuf, hdr_enable, value, index);*/
	set_pr_var(tmpbuf, receive_edid_len, value, index);
	set_pr_var(tmpbuf, new_edid, value, index);
	set_pr_var(tmpbuf, hdr_lum_len, value, index);
	set_pr_var(tmpbuf, new_hdr_lum, value, index);
	set_pr_var(tmpbuf, hdcp_array_len, value, index);
	set_pr_var(tmpbuf, hdcp_len, value, index);
	set_pr_var(tmpbuf, hdcp_repeat_depth, value, index);
	set_pr_var(tmpbuf, new_hdcp, value, index);
	set_pr_var(tmpbuf, repeat_plug, value, index);
	set_pr_var(tmpbuf, up_phy_addr, value, index);
	set_pr_var(tmpbuf, hdcp22_reset_max, value, index);
	set_pr_var(tmpbuf, hpd_to_esm, value, index);
	set_pr_var(tmpbuf, esm_reset_flag, value, index);
	set_pr_var(tmpbuf, video_stable_to_esm, value, index);
	//set_pr_var(tmpbuf, hdcp_mode_sel, value, index);
	//set_pr_var(tmpbuf, hdcp_auth_status, value, index);
	set_pr_var(tmpbuf, loadkey_22_hpd_delay, value, index);
	set_pr_var(tmpbuf, wait_hdcp22_cnt, value, index);
	set_pr_var(tmpbuf, wait_hdcp22_cnt1, value, index);
	set_pr_var(tmpbuf, wait_hdcp22_cnt2, value, index);
	set_pr_var(tmpbuf, wait_hdcp22_cnt3, value, index);
	set_pr_var(tmpbuf, enable_hdcp22_loadkey, value, index);
	set_pr_var(tmpbuf, do_esm_rst_flag, value, index);
	set_pr_var(tmpbuf, enable_hdcp22_esm_log, value, index);
	set_pr_var(tmpbuf, hdcp22_firmware_ok_flag, value, index);
	set_pr_var(tmpbuf, esm_err_force_14, value, index);
	set_pr_var(tmpbuf, reboot_esm_done, value, index);
	set_pr_var(tmpbuf, esm_reboot_lvl, value, index);
	set_pr_var(tmpbuf, enable_esm_reboot, value, index);
	set_pr_var(tmpbuf, esm_error_flag, value, index);
	set_pr_var(tmpbuf, esm_data_base_addr, value, index);
	set_pr_var(tmpbuf, stable_check_lvl, value, index);
	set_pr_var(tmpbuf, hdcp22_reauth_enable, value, index);
	set_pr_var(tmpbuf, hdcp22_stop_auth_enable, value, index);
	set_pr_var(tmpbuf, hdcp22_stop_auth, value, index);
	set_pr_var(tmpbuf, hdcp22_esm_reset2_enable, value, index);
	set_pr_var(tmpbuf, hdcp22_esm_reset2, value, index);
	set_pr_var(tmpbuf, esm_recovery_mode, value, index);
}
#endif
void rx_get_global_varaible(const char *buf)
{
	int i = 1;

	rx_pr("index %-30s   value\n", "varaible");
	/* pr_var( sig_pll_unlock_cnt ); */
	/* pr_var( sig_pll_unlock_max ); */
	/* pr_var( sig_pll_lock_max ); */
	pr_var(dwc_rst_wait_cnt_max, i++);
	pr_var(sig_stable_max, i++);
	pr_var(clk_debug, i++);
	pr_var(hpd_wait_max, i++);
	pr_var(sig_unstable_max, i++);
	pr_var(sig_unready_max, i++);
	pr_var(pow5v_max_cnt, i++);
	pr_var(rgb_quant_range, i++);
	pr_var(yuv_quant_range, i++);
	pr_var(it_content, i++);
	pr_var(diff_pixel_th, i++);
	pr_var(diff_line_th, i++);
	pr_var(diff_frame_th, i++);
	pr_var(port_map, i++);
	pr_var(edid_mode, i++);
	pr_var(force_vic, i++);
	pr_var(hdcp22_kill_esm, i++);
	pr_var(audio_sample_rate, i++);
	pr_var(auds_rcv_sts, i++);
	pr_var(audio_coding_type, i++);
	pr_var(audio_channel_count, i++);
	pr_var(hdcp22_capable_sts, i++);
	pr_var(esm_auth_fail_en, i++);
	pr_var(hdcp22_auth_sts, i++);
	pr_var(log_level, i++);
	/* pr_var( frame_skip_en , i++); */
	pr_var(auto_switch_off, i++);
	pr_var(clk_unstable_cnt, i++);
	pr_var(clk_unstable_max, i++);
	pr_var(clk_stable_cnt, i++);
	pr_var(clk_stable_max, i++);
	pr_var(wait_no_sig_max, i++);
	/*pr_var(hdr_enable, i++);*/
	pr_var(receive_edid_len, i++);
	pr_var(new_edid, i++);
	pr_var(hdr_lum_len, i++);
	pr_var(new_hdr_lum, i++);
	pr_var(hdcp_array_len, i++);
	pr_var(hdcp_len, i++);
	pr_var(hdcp_repeat_depth, i++);
	pr_var(new_hdcp, i++);
	pr_var(repeat_plug, i++);
	pr_var(up_phy_addr, i++);
	pr_var(hdcp22_reset_max, i++);
	pr_var(hpd_to_esm, i++);
	pr_var(esm_reset_flag, i++);
	pr_var(video_stable_to_esm, i++);
	//pr_var(hdcp_mode_sel, i++);
	//pr_var(hdcp_auth_status, i++);
	pr_var(loadkey_22_hpd_delay, i++);
	pr_var(wait_hdcp22_cnt, i++);
	pr_var(wait_hdcp22_cnt1, i++);
	pr_var(wait_hdcp22_cnt2, i++);
	pr_var(wait_hdcp22_cnt3, i++);
	pr_var(enable_hdcp22_loadkey, i++);
	pr_var(do_esm_rst_flag, i++);
	pr_var(enable_hdcp22_esm_log, i++);
	pr_var(hdcp22_firmware_ok_flag, i++);
	pr_var(esm_err_force_14, i++);
	pr_var(reboot_esm_done, i++);
	pr_var(esm_reboot_lvl, i++);
	pr_var(enable_esm_reboot, i++);
	pr_var(esm_error_flag, i++);
	pr_var(esm_data_base_addr, i++);
	pr_var(stable_check_lvl, i++);
	pr_var(hdcp22_reauth_enable, i++);
	pr_var(hdcp22_stop_auth_enable, i++);
	pr_var(hdcp22_stop_auth, i++);
	pr_var(hdcp22_esm_reset2_enable, i++);
	pr_var(hdcp22_esm_reset2, i++);
	pr_var(esm_recovery_mode, i++);
}


void skip_frame(void)
{
	if (rx.state == FSM_SIG_READY)
		rx.skip = (1000 * 100 / rx.pre.frame_rate / 10) + 1;
	if (log_level & VIDEO_LOG)
		rx_pr("rx.skip = %d\n", rx.skip);
}

/***********************
 *hdmirx_hw_init
 *hdmirx_hw_uninit
 ***********************/
void hdmirx_hw_init(enum tvin_port_e port)
{
	rx.port = (port - TVIN_PORT_HDMI0) & 0xf;
	//rx.no_signal = false;
	//rx.wait_no_sig_cnt = 0;
	if (hdmirx_repeat_support())
		rx.hdcp.repeat = repeat_plug;
	else
		rx.hdcp.repeat = 0;

	if ((pre_port != rx.port) ||
		(rx_get_cur_hpd_sts()) == 0) {
		if (hdcp22_on) {
			esm_set_stable(FALSE);
			esm_set_reset(TRUE);
			if (esm_recovery_mode == ESM_REC_MODE_TMDS)
				rx_esm_tmdsclk_en(FALSE);
			if (log_level & VIDEO_LOG)
				rx_pr("switch_set_state:%d\n", pwr_sts);
		}
		rx.state = FSM_HPD_LOW;
		rx_set_hpd(0);
		/* need reset the whole module when switch port */
		hdmirx_hw_config();
	} else {
		if (rx.state >= FSM_SIG_STABLE)
			rx.state = FSM_SIG_STABLE;
		else
			rx.state = FSM_WAIT_CLK_STABLE;
	}
	edid_update_flag = 0;
	rx_pkt_initial();

	rx_pr("%s:%d\n", __func__, rx.port);
}

void hdmirx_hw_uninit(void)
{
	if (sm_pause)
		return;
}

void rx_nosig_monitor(void)
{
	if (rx.cur_5v_sts == 0)
		rx.no_signal = true;

	else if (rx.state != FSM_SIG_READY) {
		if (rx.wait_no_sig_cnt >= wait_no_sig_max)
			rx.no_signal = true;
		else {
			rx.wait_no_sig_cnt++;
			if (rx.no_signal)
				rx.no_signal = false;
		}
	} else {
		rx.wait_no_sig_cnt = 0;
		rx.no_signal = false;
	}
}

static void rx_cable_clk_monitor(void)
{
	static bool pre_sts;

	bool sts = is_clk_stable();

	if (pre_sts != sts) {
		rx_pr("\nclk stable = %d\n", sts);
		pre_sts = sts;
	}
}
/* ---------------------------------------------------------- */
/* func:         port A,B,C,D  hdmitx-5v monitor & HPD control */
/* note:         G9TV portD no used */
/* ---------------------------------------------------------- */
void rx_5v_monitor(void)
{
	static uint8_t check_cnt;
	uint8_t tmp_5v = rx_get_hdmi5v_sts();

	if (auto_switch_off)
		tmp_5v = 0x0f;

	if (tmp_5v != pwr_sts)
		check_cnt++;

	if (check_cnt >= pow5v_max_cnt) {
		check_cnt = 0;
		pwr_sts = tmp_5v;
		rx.cur_5v_sts = (pwr_sts >> rx.port) & 1;
		hotplug_wait_query();
		if (rx.cur_5v_sts == 0) {
			set_fsm_state(FSM_5V_LOST);
			rx.err_code = ERR_5V_LOST;
		}
	}
	rx.cur_5v_sts = (pwr_sts >> rx.port) & 1;
}

void rx_err_monitor(void)
{
	//static bool hdcp14_sts;
	if (clk_debug)
		rx_cable_clk_monitor();

	rx.timestamp++;
	if (rx.err_code == ERR_NONE)
		return;

	if (err_dbg_cnt++ > err_dbg_cnt_max)
		//return;
		err_dbg_cnt = 0;
	//rx_pr("err_code = %d\n", rx.err_code);
	switch (rx.err_code) {
	case ERR_5V_LOST:
		if (err_dbg_cnt == 0)
			rx_pr("hdmi-5v-state = %x\n", pwr_sts);
		break;
	case ERR_CLK_UNSTABLE:
		if (err_dbg_cnt == 0)
			rx_pr("PHY_MAINFSM_STATUS1 = %x\n",
				hdmirx_rd_phy(PHY_MAINFSM_STATUS1));
		break;
	case ERR_PHY_UNLOCK:
		if (err_dbg_cnt == 0)
			rx_pr("EQ = %d-%d-%d\n",
				eq_ch0.bestsetting,
				eq_ch1.bestsetting,
				eq_ch2.bestsetting);
		break;
	case ERR_DE_UNSTABLE:
		if (err_dbg_cnt == 0)
			dump_unnormal_info();
		break;
	case ERR_NO_HDCP14_KEY:
		if (err_dbg_cnt == 0) {
			rx_pr("NO HDCP1.4 KEY\n");
			rx_pr("bksv = %d,%d", rx.hdcp.bksv[0], rx.hdcp.bksv[1]);
		}
		break;
	case ERR_TIMECHANGE:
		//rx_pr("ready time = %d", rx.unready_timestamp);
		//rx_pr("stable time = %d", rx.stable_timestamp);
		if (((rx.unready_timestamp - rx.stable_timestamp) < 30) &&
			(rx_get_eq_run_state() == E_EQ_SAME)) {
			rx.err_cnt++;
			rx_pr("timingchange warning cnt = %d\n", rx.err_cnt);
		}
		if (rx.err_cnt > 3)
			fsm_restart();
		rx.err_code = ERR_NONE;
		break;
	default:
		break;
	}
}

/*
 * FUNC: rx_main_state_machine
 * signal detection main process
 */
void rx_main_state_machine(void)
{
	switch (rx.state) {
	case FSM_5V_LOST:
		if (rx.cur_5v_sts)
			rx.state = FSM_INIT;
		break;
	case FSM_HPD_LOW:
		rx_set_hpd(0);
		set_scdc_cfg(1, 0);
		rx.state = FSM_INIT;
		break;
	case FSM_INIT:
		signal_status_init();
		rx.state = FSM_HPD_HIGH;
		break;
	case FSM_HPD_HIGH:
		hpd_wait_cnt++;
		if (rx_get_cur_hpd_sts() == 0) {
			if ((edid_update_flag) || (rx.boot_flag)) {
				if (hpd_wait_cnt <= hpd_wait_max*10)
					break;
			} else {
				if (hpd_wait_cnt <= hpd_wait_max)
					break;
			}
			rx.boot_flag = FALSE;
		}
		hpd_wait_cnt = 0;
		clk_unstable_cnt = 0;
		pre_port = rx.port;
		rx_set_hpd(1);
		set_scdc_cfg(0, 1);
		//rx.hdcp.hdcp_version = HDCP_VER_NONE;
		rx.state = FSM_WAIT_CLK_STABLE;
		break;
	case FSM_WAIT_CLK_STABLE:
		if (is_clk_stable()) {
			clk_unstable_cnt = 0;
			if (++clk_stable_cnt > clk_stable_max) {
				rx.state = FSM_EQ_START;
				clk_stable_cnt = 0;
				rx.err_code = ERR_NONE;
			}
		} else {
			clk_stable_cnt = 0;
			if (clk_unstable_cnt < clk_unstable_max) {
				clk_unstable_cnt++;
				break;
			}
			if (rx.err_rec_mode != ERR_REC_END) {
				rx_set_hpd(0);
				rx.state = FSM_HPD_HIGH;
				rx.err_rec_mode = ERR_REC_END;
			} else
				rx.err_code = ERR_CLK_UNSTABLE;
		}
		break;
	case FSM_EQ_START:
		rx_eq_algorithm();
		rx.state = FSM_WAIT_EQ_DONE;
		break;
	case FSM_WAIT_EQ_DONE:
		if ((rx_get_eq_run_state() == E_EQ_FINISH) ||
			(rx_get_eq_run_state() == E_EQ_SAME)) {
			rx.state = FSM_SIG_UNSTABLE;
			pll_lock_cnt = 0;
			pll_unlock_cnt = 0;
		}
		break;
	case FSM_SIG_UNSTABLE:
		if (is_tmds_valid()) {
			pll_unlock_cnt = 0;
			if (++pll_lock_cnt < pll_lock_max)
				break;
			rx_dwc_reset();
			rx.state = FSM_SIG_WAIT_STABLE;
		} else {
			pll_lock_cnt = 0;
			if (pll_unlock_cnt < pll_unlock_max) {
				pll_unlock_cnt++;
				break;
			}
			if (rx.err_rec_mode == ERR_REC_EQ_RETRY) {
				rx.state = FSM_WAIT_CLK_STABLE;
				rx.err_rec_mode = ERR_REC_HPD_RST;
				rx_set_eq_run_state(E_EQ_START);
			} else if (rx.err_rec_mode == ERR_REC_HPD_RST) {
				rx_set_hpd(0);
				rx.state = FSM_HPD_HIGH;
				rx.err_rec_mode = ERR_REC_END;
			} else
				rx.err_code = ERR_PHY_UNLOCK;
		}
		break;
	case FSM_SIG_WAIT_STABLE:
		dwc_rst_wait_cnt++;
		if (dwc_rst_wait_cnt < dwc_rst_wait_cnt_max)
			break;
		if ((edid_update_flag) &&
			(dwc_rst_wait_cnt < edid_update_delay))
			break;
		edid_update_flag = 0;
		dwc_rst_wait_cnt = 0;
		sig_stable_cnt = 0;
		sig_unstable_cnt = 0;
		rx.state = FSM_SIG_STABLE;
		break;
	case FSM_SIG_STABLE:
		memcpy(&rx.pre, &rx.cur,
			sizeof(struct rx_video_info));
		rx_get_video_info();
		if (rx_is_timing_stable()) {
			if (++sig_stable_cnt >= sig_stable_max) {
				get_timing_fmt();
				if (is_unnormal_format(sig_stable_cnt))
					break;
				sig_unready_cnt = 0;
				rx.skip = 0;
				rx.state = FSM_SIG_READY;
				rx.aud_sr_stable_cnt = 0;
				rx.aud_sr_unstable_cnt = 0;
				rx.no_signal = false;
				//memset(&rx.aud_info, 0,
					//sizeof(struct aud_info_s));
				//rx_set_eq_run_state(E_EQ_PASS);
				hdmirx_config_video();
				rx_aud_pll_ctl(1);
				rx.stable_timestamp = rx.timestamp;
				rx_pr("Sig ready\n");
				dump_state(1);
			}
		} else {
			sig_stable_cnt = 0;
			if (sig_unstable_cnt < sig_unstable_max) {
				sig_unstable_cnt++;
				break;
			}
			if (rx.err_rec_mode == ERR_REC_EQ_RETRY) {
				rx.state = FSM_WAIT_CLK_STABLE;
				rx.err_rec_mode = ERR_REC_HPD_RST;
				rx_set_eq_run_state(E_EQ_START);
			} else if (rx.err_rec_mode == ERR_REC_HPD_RST) {
				rx_set_hpd(0);
				rx.state = FSM_HPD_HIGH;
				rx.err_rec_mode = ERR_REC_END;
			} else
				rx.err_code = ERR_DE_UNSTABLE;
		}
		break;
	case FSM_SIG_READY:
		rx_get_video_info();
		/* video info change */
		if ((!is_tmds_valid()) ||
			(!rx_is_timing_stable())) {
			skip_frame();
			if (++sig_unready_cnt >= sig_unready_max) {
				/*sig_lost_lock_cnt = 0;*/
				rx.unready_timestamp = rx.timestamp;
				rx.err_code = ERR_TIMECHANGE;
				dump_unnormal_info();
				rx_pr("sig ready exit: ");
				rx_pr("tmdsvalid:%d, unready:%d\n",
					hdmirx_rd_dwc(DWC_HDMI_PLL_LCK_STS),
					sig_unready_cnt);
				sig_unready_cnt = 0;
				audio_sample_rate = 0;
				rx_aud_pll_ctl(0);
				rx.hdcp.hdcp_version = HDCP_VER_NONE;
				rx.state = FSM_WAIT_CLK_STABLE;
				rx.pre_state = FSM_SIG_READY;
				rx.skip = 0;
				rx.aud_sr_stable_cnt = 0;
				rx.aud_sr_unstable_cnt = 0;
				if (hdcp22_on) {
					esm_set_stable(FALSE);
					if (esm_recovery_mode
						== ESM_REC_MODE_RESET)
						esm_set_reset(TRUE);
					else
						rx_esm_tmdsclk_en(FALSE);
				}
				break;
			}
		} else {
			sig_unready_cnt = 0;
			if (rx.skip > 0)
				rx.skip--;
		}
		if (rx.pre.sw_dvi == 1)
			break;


		rx_get_audinfo(&rx.aud_info);

		if (check_real_sr_change())
			hdmirx_audio_pll_sw_update();

		if (is_aud_pll_error()) {
			rx.aud_sr_unstable_cnt++;
			if (rx.aud_sr_unstable_cnt > aud_sr_stb_max) {
				hdmirx_audio_pll_sw_update();
				if (log_level & AUDIO_LOG)
					rx_pr("update audio-err\n");
				rx.aud_sr_unstable_cnt = 0;
			}
		}
		packet_update();
		break;
	default:
		break;
	}
}

int hdmirx_show_info(unsigned char *buf, int size)
{
	int pos = 0;
	struct drm_infoframe_st *drmpkt;

	drmpkt = (struct drm_infoframe_st *)&(rx.drm_info);

	pos += snprintf(buf+pos, size-pos,
		"HDMI info\n\n");
	if (rx.cur.colorspace == E_COLOR_RGB)
		pos += snprintf(buf+pos, size-pos,
			"Color Space: %s\n", "0-RGB");
	else if (rx.cur.colorspace == E_COLOR_YUV422)
		pos += snprintf(buf+pos, size-pos,
			"Color Space: %s\n", "1-YUV422");
	else if (rx.cur.colorspace == E_COLOR_YUV444)
		pos += snprintf(buf+pos, size-pos,
			"Color Space: %s\n", "1-YUV444");
	else if (rx.cur.colorspace == E_COLOR_YUV420)
		pos += snprintf(buf+pos, size-pos,
			"Color Space: %s\n", "1-YUV420");
	pos += snprintf(buf+pos, size-pos,
		"Dvi: %d\n", rx.cur.hw_dvi);
	pos += snprintf(buf+pos, size-pos,
		"Interlace: %d\n", rx.cur.interlaced);
	pos += snprintf(buf+pos, size-pos,
		"Htotal: %d\n", rx.cur.htotal);
	pos += snprintf(buf+pos, size-pos,
		"Hactive: %d\n", rx.cur.hactive);
	pos += snprintf(buf+pos, size-pos,
		"Vtotal: %d\n", rx.cur.vtotal);
	pos += snprintf(buf+pos, size-pos,
		"Vactive: %d\n", rx.cur.vactive);
	pos += snprintf(buf+pos, size-pos,
		"Repetition: %d\n", rx.cur.repeat);
	pos += snprintf(buf+pos, size-pos,
		"Color Depth: %d\n", rx.cur.colordepth);
	pos += snprintf(buf+pos, size-pos,
		"Frame Rate: %d\n", rx.cur.frame_rate);
	pos += snprintf(buf+pos, size-pos,
		"Skip frame: %d\n", rx.skip);
	pos += snprintf(buf+pos, size-pos,
		"avmute skip: %d\n", rx.avmute_skip);
	pos += snprintf(buf+pos, size-pos,
		"TMDS clock: %d\n", hdmirx_get_tmds_clock());
	pos += snprintf(buf+pos, size-pos,
		"Pixel clock: %d\n", hdmirx_get_pixel_clock());
	if (drmpkt->des_u.tp1.eotf == EOTF_SDR)
		pos += snprintf(buf+pos, size-pos,
			"HDR EOTF: %s\n", "SDR");
	else if (drmpkt->des_u.tp1.eotf == EOTF_HDR)
		pos += snprintf(buf+pos, size-pos,
		"HDR EOTF: %s\n", "HDR");
	else if (drmpkt->des_u.tp1.eotf == EOTF_SMPTE_ST_2048)
		pos += snprintf(buf+pos, size-pos,
		"HDR EOTF: %s\n", "SMPTE_ST_2048");
	pos += snprintf(buf+pos, size-pos,
		"Dolby Vision: %s\n", (rx.vsi_info.dolby_vision?"on":"off"));

	pos += snprintf(buf+pos, size-pos,
		"\n\nAudio info\n\n");
	pos += snprintf(buf+pos, size-pos,
		"CTS: %d\n", rx.aud_info.cts);
	pos += snprintf(buf+pos, size-pos,
		"N: %d\n", rx.aud_info.n);
	pos += snprintf(buf+pos, size-pos,
		"Recovery clock: %d\n", rx.aud_info.arc);
	pos += snprintf(buf+pos, size-pos,
		"audio receive data: %d\n", auds_rcv_sts);
	pos += snprintf(buf+pos, size-pos,
		"Audio PLL clock: %d\n", hdmirx_get_audio_clock());
	pos += snprintf(buf+pos, size-pos,
		"mpll_div_clk: %d\n", hdmirx_get_mpll_div_clk());

	pos += snprintf(buf+pos, size-pos,
		"\n\nHDCP info\n\n");
	pos += snprintf(buf+pos, size-pos,
		"HDCP Debug Value: 0x%x\n", hdmirx_rd_dwc(DWC_HDCP_DBG));
	pos += snprintf(buf+pos, size-pos,
		"HDCP14 state: %d\n", rx.cur.hdcp14_state);
	pos += snprintf(buf+pos, size-pos,
		"HDCP22 state: %d\n", rx.cur.hdcp22_state);
	if (rx.port == E_PORT0)
		pos += snprintf(buf+pos, size-pos,
			"Source Physical address: %d.0.0.0\n", 1);
	else if (rx.port == E_PORT1)
		pos += snprintf(buf+pos, size-pos,
			"Source Physical address: %d.0.0.0\n", 3);
	else if (rx.port == E_PORT2)
		pos += snprintf(buf+pos, size-pos,
			"Source Physical address: %d.0.0.0\n", 2);
	else if (rx.port == E_PORT3)
		pos += snprintf(buf+pos, size-pos,
			"Source Physical address: %d.0.0.0\n", 4);
	pos += snprintf(buf+pos, size-pos,
		"HDCP22_ON: %d\n", hdcp22_on);
	pos += snprintf(buf+pos, size-pos,
		"HDCP22 sts: 0x%x\n", rx_hdcp22_rd_reg(0x60));
	pos += snprintf(buf+pos, size-pos,
		"HDCP22_capable_sts: %d\n", hdcp22_capable_sts);
	pos += snprintf(buf+pos, size-pos,
		"sts0x8fc: 0x%x\n", hdmirx_rd_dwc(DWC_HDCP22_STATUS));
	pos += snprintf(buf+pos, size-pos,
		"sts0x81c: 0x%x\n", hdmirx_rd_dwc(DWC_HDCP22_CONTROL));

	return pos;
}

static void dump_hdcp_data(void)
{
	rx_pr("\n*************HDCP");
	rx_pr("***************");
	rx_pr("\n hdcp-seed = %d ",
		rx.hdcp.seed);
	/* KSV CONFIDENTIAL */
	rx_pr("\n hdcp-ksv = %x---%x",
		rx.hdcp.bksv[0],
		rx.hdcp.bksv[1]);
	rx_pr("\n*************HDCP end**********\n");
}

void dump_state(unsigned char enable)
{
	/*int error = 0;*/
	/* int i = 0; */
	static struct aud_info_s a;

	rx_get_video_info();
	if (enable & 1) {
		rx_pr("[HDMI info]");
		rx_pr("colorspace %d,", rx.cur.colorspace);
		rx_pr("hw_vic %d,", rx.cur.hw_vic);
		rx_pr("dvi %d,", rx.cur.hw_dvi);
		rx_pr("interlace %d\n", rx.cur.interlaced);
		rx_pr("htotal %d", rx.cur.htotal);
		rx_pr("hactive %d", rx.cur.hactive);
		rx_pr("vtotal %d", rx.cur.vtotal);
		rx_pr("vactive %d", rx.cur.vactive);
		rx_pr("repetition %d\n", rx.cur.repeat);
		rx_pr("colordepth %d", rx.cur.colordepth);
		rx_pr("frame_rate %d\n", rx.cur.frame_rate);
		rx_pr("TMDS clock = %d,",
			hdmirx_get_tmds_clock());
		rx_pr("Pixel clock = %d\n",
			hdmirx_get_pixel_clock());
		rx_pr("Audio PLL clock = %d",
			hdmirx_get_audio_clock());
		rx_pr("ESM clock = %d",
			hdmirx_get_esm_clock());
		rx_pr("avmute_skip:0x%x\n", rx.avmute_skip);
		rx_pr("rx.no_signal=%d,rx.state=%d,",
				rx.no_signal,
				rx.state);
		rx_pr("skip frame=%d\n", rx.skip);
		rx_pr("fmt=0x%x,sw_vic:%d,",
				hdmirx_hw_get_fmt(),
				rx.cur.sw_vic);
		rx_pr("sw_dvi:%d,sw_fp:%d,",
				rx.cur.sw_dvi,
				rx.cur.sw_fp);
		rx_pr("sw_alternative:%d\n",
			rx.cur.sw_alternative);
		rx_pr("HDCP debug value=0x%x\n",
			hdmirx_rd_dwc(DWC_HDCP_DBG));
		rx_pr("HDCP14 state:%d\n",
			rx.cur.hdcp14_state);
		rx_pr("HDCP22 state:%d\n",
			rx.cur.hdcp22_state);
		rx_pr("audio receive data:%d\n",
			auds_rcv_sts);
		rx_pr("avmute_skip:0x%x\n", rx.avmute_skip);
	}
	if (enable & 2) {
		rx_get_audinfo(&a);
		rx_pr("AudioInfo:");
		rx_pr(" CT=%u CC=%u",
				a.coding_type,
				a.channel_count);
		rx_pr(" SF=%u SS=%u",
				a.sample_frequency,
				a.sample_size);
		rx_pr(" CA=%u",
			a.channel_allocation);
		rx_pr(" CTS=%d, N=%d,",
				a.cts, a.n);
		rx_pr("recovery clock is %d\n",
			a.arc);
	}
	if (enable & 4) {
		/***************hdcp*****************/
		rx_pr("HDCP version:%d\n", rx.hdcp.hdcp_version);
		if (hdcp22_on) {
			rx_pr("HDCP22 sts = %x\n",
				rx_hdcp22_rd_reg(0x60));
			rx_pr("HDCP22_on = %d\n",
				hdcp22_on);
			rx_pr("HDCP22_auth_sts = %d\n",
				hdcp22_auth_sts);
			rx_pr("HDCP22_capable_sts = %d\n",
				hdcp22_capable_sts);
			rx_pr("video_stable_to_esm = %d\n",
				video_stable_to_esm);
			rx_pr("hpd_to_esm = %d\n",
				hpd_to_esm);
			rx_pr("sts8fc = %x",
				hdmirx_rd_dwc(DWC_HDCP22_STATUS));
			rx_pr("sts81c = %x",
				hdmirx_rd_dwc(DWC_HDCP22_CONTROL));

			/*
			 *if (!esm_print_device_info())
			 *	rx_pr("\n !!No esm rx opened\n");
			 */
		}
		dump_hdcp_data();
		/*--------------edid-------------------*/
		rx_pr("edid index: %d\n", edid_mode);
		rx_pr("phy addr: %#x,%#x,port: %d, up phy addr:%#x\n",
			hdmirx_rd_top(TOP_EDID_RAM_OVR1_DATA),
			hdmirx_rd_top(TOP_EDID_RAM_OVR2_DATA),
				rx.port, up_phy_addr);
		rx_pr("downstream come: %d hpd:%d hdr lume:%d\n",
			new_edid, repeat_plug, new_hdr_lum);
	}
}

int hdmirx_debug(const char *buf, int size)
{
	char tmpbuf[128];
	int i = 0;
	long value = 0;

	char input[5][20];
	char *const delim = " ";
	char *token;
	char *cur;
	int cnt = 0;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;

	for (cnt = 0; cnt < 5; cnt++)
		input[cnt][0] = '\0';

	cur = (char *)buf;
	cnt = 0;
	while ((token = strsep(&cur, delim)) && (cnt < 5)) {
		if (strlen((char *)token) < 20)
			strcpy(&input[cnt][0], (char *)token);
		else
			rx_pr("err input\n");
		cnt++;
	}

	if (strncmp(tmpbuf, "help", 4) == 0) {
		rx_pr("*****************\n");
		rx_pr("reset0--hw_config\n");
		rx_pr("reset1--8bit phy rst\n");
		rx_pr("reset3--irq open\n");
		rx_pr("reset4--edid_update\n");
		rx_pr("reset5--esm rst\n");
		rx_pr("database--esm data addr\n");
		rx_pr("duk--dump duk\n");
		rx_pr("suspend_pddq--pddqdown\n");
		rx_pr("*****************\n");
	} else if (strncmp(tmpbuf, "hpd", 3) == 0)
		rx_set_hpd(tmpbuf[3] == '0' ? 0 : 1);
	else if (strncmp(tmpbuf, "cable_status", 12) == 0) {
		size = hdmirx_rd_top(TOP_HPD_PWR5V) >> 20;
		rx_pr("cable_status = %x\n", size);
	} else if (strncmp(tmpbuf, "signal_status", 13) == 0) {
		size = rx.no_signal;
		rx_pr("signal_status = %d\n", size);
	} else if (strncmp(tmpbuf, "reset", 5) == 0) {
		if (tmpbuf[5] == '0') {
			rx_pr(" hdmirx hw config\n");
			hdmirx_hw_config();
		} else if (tmpbuf[5] == '1') {
			rx_pr(" hdmirx phy init 8bit\n");
			hdmirx_phy_init();
		} else if (tmpbuf[5] == '3') {
			rx_pr(" irq open\n");
			rx_irq_en(TRUE);
		} else if (tmpbuf[5] == '4') {
			rx_pr(" edid update\n");
			hdmi_rx_ctrl_edid_update();
		} else if (tmpbuf[5] == '5') {
			hdmirx_hdcp22_esm_rst();
		}
	} else if (strncmp(tmpbuf, "state", 5) == 0) {
		if (tmpbuf[5] == '1')
			dump_state(0xff);
		else
			dump_state(1);
	} else if (strncmp(tmpbuf, "database", 5) == 0) {
		rx_pr("data base = 0x%x\n", esm_data_base_addr);
	} else if (strncmp(tmpbuf, "pause", 5) == 0) {
		if (kstrtol(tmpbuf + 5, 10, &value) < 0)
			return -EINVAL;
		rx_pr("%s\n", value ? "pause" : "enable");
		sm_pause = value;
	} else if (strncmp(tmpbuf, "reg", 3) == 0) {
		dump_reg();
	}  else if (strncmp(tmpbuf, "duk", 3) == 0) {
		rx_pr("hdcp22=%d\n", rx_sec_set_duk());
	} else if (strncmp(tmpbuf, "edid", 4) == 0) {
		dump_edid_reg();
	} else if (strncmp(tmpbuf, "load14key", 7) == 0) {
		rx_debug_loadkey();
	} else if (strncmp(tmpbuf, "load22key", 9) == 0) {
		rx_debug_load22key();
	} else if (strncmp(tmpbuf, "esm0", 4) == 0) {
		/* switch_set_state(&rx.hpd_sdev, 0x0); */
		extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 0);
	} else if (strncmp(tmpbuf, "esm1", 4) == 0) {
		/*switch_set_state(&rx.hpd_sdev, 0x01);*/
		extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 1);
	} else if (strncmp(input[0], "pktinfo", 7) == 0) {
		rx_debug_pktinfo(input);
	} else if (tmpbuf[0] == 'w') {
		rx_debug_wr_reg(buf, tmpbuf, i);
	} else if (tmpbuf[0] == 'r') {
		rx_debug_rd_reg(buf, tmpbuf);
	} else if (tmpbuf[0] == 'v') {
		rx_pr("------------------\n");
		rx_pr("Hdmirx version0: %s\n", RX_VER0);
		rx_pr("Hdmirx version1: %s\n", RX_VER1);
		rx_pr("Hdmirx version2: %s\n", RX_VER2);
		rx_pr("Hdmirx version3: %s\n", RX_VER3);
		rx_pr("Hdmirx version4: %s\n", RX_VER4);
		rx_pr("------------------\n");
	}
	return 0;
}

