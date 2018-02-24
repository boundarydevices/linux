/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmi_rx_wrapper.h
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

#ifndef HDMI_RX_WRAPPER_H_
#define HDMI_RX_WRAPPER_H_


#define	LOG_EN		0x01
#define VIDEO_LOG	0x02
#define AUDIO_LOG	0x04
#define HDCP_LOG	0x08
#define PACKET_LOG	0x10
#define EQ_LOG		0x20
#define REG_LOG		0x40
#define ERR_LOG		0x80
#define VSI_LOG		0x800

/* stable_check_lvl */
#define HACTIVE_EN		0x01
#define VACTIVE_EN		0x02
#define HTOTAL_EN		0x04
#define VTOTAL_EN		0x08
#define COLSPACE_EN		0x10
#define REFRESH_RATE_EN 0x20
#define REPEAT_EN		0x40
#define DVI_EN			0x80
#define INTERLACED_EN	0x100
#define HDCP_ENC_EN		0x200
#define COLOR_DEP_EN	0x400

/* aud sample rate stable range */
#define AUD_SR_RANGE 2000
#define PHY_REQUEST_CLK_MIN		170000000
#define PHY_REQUEST_CLK_MAX		370000000
#define TIMER_STATE_CHECK		(1*HZ/100)
#define USE_NEW_FSM_METHODE

struct freq_ref_s {
	bool interlace;
	uint8_t cd420;
	uint8_t type_3d;
	unsigned int hactive;
	unsigned int vactive;
	uint8_t vic;
};

enum video_format_e {

	HDMI_UNKNOWN = 0,
	HDMI_720x480i,
	HDMI_1440x480i,
	HDMI_720x576i,
	HDMI_1440x576i,

	HDMI_720x480p = 5,
	HDMI_1440x480p,
	HDMI_720x576p,
	HDMI_1440x576p,

	HDMI_1440x240p = 9,
	HDMI_2880x240p,
	HDMI_1440x288p,
	HDMI_2880x288p,

	HDMI_720p = 13,
	HDMI_1080i,
	HDMI_1080p,

	HDMI_3840x2160 = 16,
	HDMI_4096x2160,
	HDMI_3840x2160_420,
	HDMI_4096x2160_420,
	HDMI_1080p_420,

	HDMI_640_480 = 21,
	HDMI_800_600,
	HDMI_1024_768,
	HDMI_720_400,
	HDMI_1280_768,
	HDMI_1280_800 = 26,
	HDMI_1280_960,
	HDMI_1280_1024,
	HDMI_1360_768,
	HDMI_1366_768,
	HDMI_1600_900 = 31,
	HDMI_1600_1200,
	HDMI_1920_1200,
	HDMI_1440_900,
	HDMI_1400_1050,
	HDMI_1680_1050 = 36,
	HDMI_2560_1440,

	HDMI_480p_FRAMEPACKING,
	HDMI_576p_FRAMEPACKING,
	HDMI_720p_FRAMEPACKING,
	HDMI_1080i_FRAMEPACKING,
	HDMI_1080i_ALTERNATIVE,
	HDMI_1080p_ALTERNATIVE,
	HDMI_1080p_FRAMEPACKING,

	HDMI_UNSUPPORT
};

enum fsm_states_e {
	FSM_5V_LOST,
	FSM_INIT,
	FSM_HPD_LOW,
	FSM_HPD_HIGH,
	FSM_WAIT_CLK_STABLE,
	FSM_EQ_START,
	FSM_WAIT_EQ_DONE,
	FSM_SIG_UNSTABLE,
	FSM_SIG_WAIT_STABLE,
	FSM_SIG_STABLE,
	FSM_SIG_READY,
};

enum err_code_e {
	ERR_NONE,
	ERR_5V_LOST,
	ERR_CLK_UNSTABLE,
	ERR_PHY_UNLOCK,
	ERR_DE_UNSTABLE,
	ERR_NO_HDCP14_KEY,
	ERR_TIMECHANGE,
	ERR_UNKONW
};

enum irq_flag_e {
	IRQ_AUD_FLAG = 0x01,
	IRQ_PACKET_FLAG = 0x02,
};

enum hdcp22_auth_state_e {
	HDCP22_AUTH_STATE_NOT_CAPABLE,
	HDCP22_AUTH_STATE_CAPABLE,
	HDCP22_AUTH_STATE_LOST,
	HDCP22_AUTH_STATE_SUCCESS,
	HDCP22_AUTH_STATE_FAILED,
};

enum esm_recovery_mode_e {
	ESM_REC_MODE_RESET = 1,
	ESM_REC_MODE_TMDS,
};

enum err_recovery_mode_e {
	ERR_REC_EQ_RETRY,
	ERR_REC_HPD_RST,
	ERR_REC_END
};
enum aud_clk_err_e {
	E_AUDPLL_OK,
	E_REQUESTCLK_ERR,
	E_PLLRATE_CHG,
};

/* signal */
extern enum tvin_sig_fmt_e hdmirx_hw_get_fmt(void);
extern void rx_main_state_machine(void);
extern void rx_err_monitor(void);
extern void rx_nosig_monitor(void);
extern bool rx_is_nosig(void);
extern bool rx_is_sig_ready(void);
extern void hdmirx_open_port(enum tvin_port_e port);
extern void hdmirx_close_port(void);
extern bool is_clk_stable(void);
extern unsigned int rx_get_pll_lock_sts(void);
extern unsigned int rx_get_scdc_clkrate_sts(void);
extern void set_scdc_cfg(int hpdlow, int pwrprovided);
extern void fsm_restart(void);
extern void rx_5v_monitor(void);
extern void rx_audio_pll_sw_update(void);
extern void rx_acr_info_sw_update(void);
extern void rx_sw_reset(int level);
extern void rx_aud_pll_ctl(bool en);
extern void hdmirx_timer_handler(unsigned long arg);



#endif

