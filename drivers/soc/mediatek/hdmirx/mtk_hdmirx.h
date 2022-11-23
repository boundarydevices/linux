/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MTK_HDMIRX_H_
#define _MTK_HDMIRX_H_

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/tee_drv.h>

#include "hdmirx.h"
#include "mtk_hdmi_rpt.h"

#define HDMIRX_YOCTO 0

#define HDMIRX_BRINGUP
//#define HDMIRX_EFUSE

/* =1: 50ohm; =0: 45ohm */
#define HDMIRX_RTERM_50OHM 1

//CONFIG_MTK_ENG_BUILD
#define HDMIRX_ENG_BUILD

#if IS_ENABLED(CONFIG_OPTEE)
//#define HDMIRX_OPTEE_EN
#endif

#define CC_SUPPORT_HDR10Plus
#define VFY_MULTI_VSI 0

//#define HDMIRX_RPT_EN
#define HDMIRX_IS_RPT_MODE 0

#define HDMIRX_DEVNAME "hdmirx"

#define SRV_OK 0
#define SRV_FAIL (-1)
//#define TRUE 1
//#define FALSE 0

#define HDMIRX_IDLE 0
#define HDMIRX_INIT 1
#define HDMIRX_RUN 2
#define HDMIRX_EXIT 3

enum HDMI_SOURCE_VER {
	HDMI_SOURCE_VERSION_HDMI14 = 0,
	HDMI_SOURCE_VERSION_HDMI20,
	HDMI_SOURCE_VERSION_HDMI_NONE
};

enum HDMI_FRL_MODE_TYPE {
	HDMI_FRL_MODE_LEGACY = 0,
	HDMI_FRL_MODE_FRL_3G_3CH,
	HDMI_FRL_MODE_FRL_6G_3CH,
	HDMI_FRL_MODE_FRL_6G_4CH,
	HDMI_FRL_MODE_FRL_8G,
	HDMI_FRL_MODE_FRL_10G,
	HDMI_FRL_MODE_FRL_12G,
	HDMI_FRL_MODE_NONE,
	HDMI_FRL_MODE_LEGACY_14,
	HDMI_FRL_MODE_LEGACY_20,
};

struct ipi_cmd_s {
	unsigned int id;
	unsigned int data;
};

enum HDMI_TASK_COMMAND_TYPE_T {
	HDMI_RX_SERVICE_CMD = 0,
	HDMI_DISABLE_HDMI_RX_TASK_CMD,
	HDMI_RX_EG_WAKEUP_THREAD,
};

enum HDMI_RX_STATE_TYPE {
	RX_DETECT_STATE = 0,
	RX_CHANGE_E_STATE,
	RX_IDLE_STATE,
};

#define HDMI_RX_TIMER_TICKET 10 /* timer is 10 ticket */
#define HDMI_RX_TIMER_10MS (10 / HDMI_RX_TIMER_TICKET)
#define HDMI_RX_TIMER_15MS (15 / HDMI_RX_TIMER_TICKET)
#define HDMI_RX_TIMER_20MS (20 / HDMI_RX_TIMER_TICKET)
#define HDMI_RX_TIMER_100MS (100 / HDMI_RX_TIMER_TICKET)
#define HDMI_RX_TIMER_200MS (200 / HDMI_RX_TIMER_TICKET)
#define HDMI_RX_TIMER_400MS (400 / HDMI_RX_TIMER_TICKET)
#define HDMI_RX_TIMER_1S (1000 / HDMI_RX_TIMER_TICKET)

#define HDMI_STABLE_TIME 20
#define HDMI_RESET_TIMEOUT ((HDMI_STABLE_TIME * 10) + 300)

#define HDMI_RX_NEW_AVI (1 << 16)
#define HDMI_RX_NEW_SP (1 << 17)
#define HDMI_RX_NEW_AUD (1 << 18)
#define HDMI_RX_NEW_MPEG (1 << 19)
#define HDMI_RX_NEW_UNREC (1 << 20)
#define HDMI_RX_NEW_CP (1 << 23)

#define HDMI_RX_AAC_MUTE (1 << 6)
#define HDMI_RX_NEW_ACP (1 << 10)
#define HDMI_RX_NEW_VSIF (1 << 22)

#define HDMI_RX_NEW_ISRC1 (1 << 2)
#define HDMI_RX_NEW_ISRC2 (1 << 3)
#define HDMI_RX_NEW_GCP (1 << 4)
#define HDMI_RX_NEW_HF_VSIF (1 << 9)

#define HDMI_RX_86_NO (1 << 8)
#define HDMI_RX_86_NEW (1 << 9)
#define HDMI_RX_87_NO (1 << 10)
#define HDMI_RX_87_NEW (1 << 11)

#define HDMI_RST_ALL 1
#define HDMI_RST_EQ 2
#define HDMI_RST_DEEPCOLOR 3
#define HDMI_RST_FIXEQ 4
#define HDMI_RST_RTCK 5
#define HDMI_RST_DIGITALPHY 6

#define HDMI_AUD_Length_Unknown 0
#define HDMI_AUD_Length_16bits 1
#define HDMI_AUD_Length_17bits 2
#define HDMI_AUD_Length_18bits 3
#define HDMI_AUD_Length_19bits 4
#define HDMI_AUD_Length_20bits 5
#define HDMI_AUD_Length_21bits 6
#define HDMI_AUD_Length_22bits 7
#define HDMI_AUD_Length_23bits 8
#define HDMI_AUD_Length_24bits 9

enum { HDMI_3D_FramePacking = 0,
	HDMI_3D_FieldAlternative,
	HDMI_3D_LineAlternative,
	HDMI_3D_SideBySideFull,
	HDMI_3D_LDepth,
	HDMI_3D_LDepthGraph,
	HDMI_3D_TopBottom,
	HDMI_3D_SideBySideHalf,
	HDMI_3D_Unknown };

enum { rx_no_signal = 0,
	rx_detect_ckdt,
	rx_reset_analog,
	rx_reset_digital,
	rx_wait_timing_stable,
	rx_is_stable,
	rx_state_max };

enum HdmiRxMode {
	HDMI_RX_MODE_DVI,
	HDMI_RX_MODE_HDMI1X,
	HDMI_RX_MODE_HDMI2X,
	HDMI_RX_MODE_MHL
};

enum HdmiRxClrSpc {
	HDMI_RX_CLRSPC_UNKNOWN,
	HDMI_RX_CLRSPC_YC444_601,
	HDMI_RX_CLRSPC_YC422_601,
	HDMI_RX_CLRSPC_YC420_601,

	HDMI_RX_CLRSPC_YC444_709,
	HDMI_RX_CLRSPC_YC422_709,
	HDMI_RX_CLRSPC_YC420_709,

	HDMI_RX_CLRSPC_XVYC444_601,
	HDMI_RX_CLRSPC_XVYC422_601,
	HDMI_RX_CLRSPC_XVYC420_601,

	HDMI_RX_CLRSPC_XVYC444_709,
	HDMI_RX_CLRSPC_XVYC422_709,
	HDMI_RX_CLRSPC_XVYC420_709,

	HDMI_RX_CLRSPC_sYCC444_601,
	HDMI_RX_CLRSPC_sYCC422_601,
	HDMI_RX_CLRSPC_sYCC420_601,

	HDMI_RX_CLRSPC_Adobe_YCC444_601,
	HDMI_RX_CLRSPC_Adobe_YCC422_601,
	HDMI_RX_CLRSPC_Adobe_YCC420_601,

	HDMI_RX_CLRSPC_RGB,
	HDMI_RX_CLRSPC_Adobe_RGB,

	HDMI_RX_CLRSPC_BT_2020_RGB_non_const_luminous,
	HDMI_RX_CLRSPC_BT_2020_RGB_const_luminous,

	HDMI_RX_CLRSPC_BT_2020_YCC444_non_const_luminous,
	HDMI_RX_CLRSPC_BT_2020_YCC422_non_const_luminous,
	HDMI_RX_CLRSPC_BT_2020_YCC420_non_const_luminous,

	HDMI_RX_CLRSPC_BT_2020_YCC444_const_luminous,
	HDMI_RX_CLRSPC_BT_2020_YCC422_const_luminous,
	HDMI_RX_CLRSPC_BT_2020_YCC420_const_luminous
};

enum HdmiRxRange {
	HDMI_RX_RGB_FULL,
	HDMI_RX_RGB_LIMT,
	HDMI_RX_YCC_FULL,
	HDMI_RX_YCC_LIMT
};

enum HdmiRxScan {
	HDMI_RX_OVERSCAN = 1,
	HDMI_RX_UNDERSCAN };

enum HdmiRxAspect {
	HDMI_RX_ASPECT_RATIO_4_3 = 1,
	HDMI_RX_ASPECT_RATIO_16_9 };

enum HdmiRxAFD {
	HDMI_RX_AFD_SAME = 8,
	HDMI_RX_AFD_4_3,
	HDMI_RX_AFD_16_9,
	HDMI_RX_AFD_14_9
};
enum HdmiRxITCInfo_t {
	HDMI_RX_ITC_GRAPHICS = 0,
	HDMI_RX_ITC_PHOTO,
	HDMI_RX_ITC_CINEMA,
	HDMI_RX_ITC_GAME
};

#define HDCP2_RX_MAX_KSV_COUNT 31
#define HDCP2_RX_MAX_DEV_COUNT 4

enum HdmiRxHdcp2Status {
	HDMI_RX_HDCP2_STATUS_OFF = 0x0,
	HDMI_RX_HDCP2_STATUS_AUTH_DONE = 0x1,
	HDMI_RX_HDCP2_STATUS_AUTH_FAIL = 0x2,
	HDMI_RX_HDCP2_STATUS_CCHK_DONE = 0x4,
	HDMI_RX_HDCP2_STATUS_CCHK_FAIL = 0x8,
	HDMI_RX_HDCP2_STATUS_AKE_SENT_RCVD = 0x10,
	HDMI_RX_HDCP2_STATUS_SKE_SENT_RCVD = 0x20,
	HDMI_RX_HDCP2_STATUS_CERT_SENT_RCVD = 0x40,
	HDMI_RX_HDCP2_STATUS_DECRYPT_SUCCESS = 0x80,
	HDMI_RX_HDCP2_STATUS_DECRYPT_FAIL = 0x100
};

enum HdmiRxHdcp1Status {
	HDMI_RX_HDCP1_STATUS_OFF = 0x0,
	HDMI_RX_HDCP1_STATUS_AUTH_DONE = 0x1,
	HDMI_RX_HDCP1_STATUS_AUTH_FAIL = 0x2,
	HDMI_RX_HDCP1_STATUS_DECRYPT_SUCCESS = 0x4,
	HDMI_RX_HDCP1_STATUS_DECRYPT_FAIL = 0x8
};

enum RxHdcpState {
	UnAuthenticated = 0,
	Computations,
	WaitforDownstream,
	WaitVReady,
	Authenticated,
	RxHDCP_Max
};

struct RxHDCPRptData {
	bool is_hdcp2;
	u8 count;
	u8 depth;
	bool is_casc_exc;
	bool is_devs_exc;
	bool is_hdcp1_present;
	bool is_hdcp2_present;
	bool is_match;
	u8 list[(HDCP2_RX_MAX_KSV_COUNT + 1) * 5];
};

enum RX_MCLK_FREQUENCY_T {
	RX_MCLK_128FS,
	RX_MCLK_192FS,
	RX_MCLK_256FS,
	RX_MCLK_384FS,
	RX_MCLK_512FS,
	RX_MCLK_768FS
};

#define M_Fs 0x3F
#define B_Fs_UNKNOWN 1
#define B_Fs_44p1KHz 0
#define B_Fs_48KHz 2
#define B_Fs_32KHz 3
#define B_Fs_88p2KHz 8
#define B_Fs_96KHz 0xA
#define B_Fs_176p4KHz 0xC
#define B_Fs_192KHz 0xE
#define B_Fs_768KHz 0x9
#define B_Fs_HBR 0x9
#define B_fs_384KHz (0x5)
#define B_FS_1536KHz (0x15)
#define B_FS_1024KHz (0x35)
#define B_FS_3528KHz (0xd)
#define B_FS_7056KHz (0x2d)
#define B_FS_14112KHz (0x1d)
#define B_FS_64KHz (0xb)
#define B_FS_128KHz (0x2b)
#define B_FS_256KHz (0x1b)
#define B_FS_512KHz (0x3b)

struct AUDIO_CAPS {
	u8 AudioFlag;
	u8 AudSrcEnable;
	u8 SampleFreq; /* SampleFreq */
	u8 ChStat[5];
	union Audio_InfoFrame AudInf; /* refer to 861 */
	struct RX_REG_AUDIO_CHSTS AudChStat; /* refer to 60958 */
};

struct AUDIO_INFO {
	u8 u1HBRAudio;
	u8 u1DSDAudio;
	u8 u1RawSDAudio;
	u8 u1PCMMultiCh;
	u8 u1FsDec;
	u8 u1I2sChanValid;
	u8 u1I2sCh0Sel;
	u8 u1I2sCh1Sel;
	u8 u1I2sCh2Sel;
	u8 u1I2sCh3Sel;
	u8 u1I2sCh4Sel;
	enum RX_MCLK_FREQUENCY_T u1MCLKRatio;
};

enum HdmiRxAudioFs_t {
	HDMI_RX_AUDIO_FS_24KHZ,
	HDMI_RX_AUDIO_FS_32KHZ,
	HDMI_RX_AUDIO_FS_44_1KHZ,
	HDMI_RX_AUDIO_FS_48KHZ,
	HDMI_RX_AUDIO_FS_88_2KHZ,
	HDMI_RX_AUDIO_FS_96KHZ,
	HDMI_RX_AUDIO_FS_176_4KHZ,
	HDMI_RX_AUDIO_FS_192KHZ,
	HDMI_RX_AUDIO_FS_768KHZ
};

enum HdmiAudFifoStatus_t {
	HDMI_AUDIO_FIFO_NORMAL = 0x1,
	HDMI_AUDIO_FIFO_UNDERRUN = 0x2,
	HDMI_AUDIO_FIFO_OVERRUN = 0x4,
	HDMI_AUDIO_FS_CHG = 0x08
};

struct HdmiVideoInfo_t {
	enum HdmiRxClrSpc clrSpc;
	enum HdmiRxRange range;
};

struct HdmiRxITC {
	bool fgItc;
	enum HdmiRxITCInfo_t ItcContent;
};

enum { SV_HDMI_RANGE_FORCE_AUTO = 0,
	SV_HDMI_RANGE_FORCE_LIMIT,
	SV_HDMI_RANGE_FORCE_FULL };

struct HdmiRx3DInfo {
	u8 HDMI_3D_Enable;
	u8 HDMI_3D_Video_Format;
	u8 HDMI_3D_Str;
	u8 HDMI_3D_EXTDATA;
};

struct HdmiRxAudioFormat_t {
	u8 layout;
	u8 dsd;
	u8 hbrA;
	enum HdmiRxAudioFs_t audioFs;
};

struct HdmiRxAudioChannelStatus_t {
	u8 u1CategoryCode;
	u8 u1WordLength;
	u8 u1b1;
	u8 u1b2;
	u8 u1b3;
	bool fgCopyRight;
	bool fgIsPCM;
};

struct HdmiRxRes {
	u32 _u4HTotal;
	u32 _u4VTotal;
	u32 _u4Width;
	u32 _u4Height;
};

struct HdmiRxScdc {
	u8 u1ScramblingOn;
	u8 u1ClkRatio;
};

#define DEVICE_COUNT 0x7F
#define MAX_DEVS_EXCEEDED (0x01 << 7)
#define DEVICE_DEPTH (0x07 << 8)
#define MAX_CASCADE_EXCEEDED (0x1 << 11)
#define HDMI_MODE_BIT (0x1 << 12)

#define TX_MAX_KSV_COUNT 9
#define RX_MAX_KSV_COUNT 10
#define RX_MAX_DEV_COUNT 7

#define HDCP2_INCLUDE_HDCP1_DEV (0x1 << 0)
#define HDCP2_INCLUDE_HDCP2_DEV (0x1 << 1)
#define HDCP2_MAX_CASCADE_EXCEEDED (0x1 << 2)
#define HDCP2_MAX_DEVS_EXCEEDED (0x01 << 3)
#define HDCP2_DEVICE_COUNT (0x1F << 4)
#define HDCP2_DEVICE_DEPTH (0x07 << 9)

#define HDCP_RECEIVER 0
#define HDCP_REPEATER 1

#define HDMI_RX_PROB_LOG 0

#define HDMI2_RES_UNSTB_TO_STB_WAIT_COUNTER (300 / HDMI_RX_TIMER_TICKET)
#define HDMI2_RES_STB_TO_UNSTB_WAIT_COUNTER (60 / HDMI_RX_TIMER_TICKET)
#define HDMI2_RES_SCDT_WAIT_COUNTER (60 / HDMI_RX_TIMER_TICKET)
#define HDMI2_HDCP_STABLE (100 / HDMI_RX_TIMER_TICKET)
#define HDMI2_HPD_Period (300 / HDMI_RX_TIMER_TICKET)
#define HDMI2_Rsen_Period (HDMI2_HPD_Period - (250 / HDMI_RX_TIMER_TICKET))
#define HDMI2_CLK_DIFF 3
#define HDMI2_WAIT_DOWNSTREAM_TIME (5000 / HDMI_RX_TIMER_TICKET)

#define RX_HDCP_LOG(fmt, arg...)                                               \
	do {                                                                   \
		if (myhdmi->debugtype & HDMI_RX_DEBUG_HDCP)                    \
			pr_info(fmt, ##arg);                                    \
	} while (0)

#define RX_AUDIO_LOG(fmt, arg...)                                              \
	do {                                                                   \
		if (myhdmi->debugtype & HDMI_RX_DEBUG_AUDIO)                   \
			pr_info(fmt, ##arg);                                    \
	} while (0)

#define RX_INFO_LOG(fmt, arg...)                                               \
	do {                                                                   \
		if (myhdmi->debugtype & HDMI_RX_DEBUG_INFOFRAME)               \
			pr_info(fmt, ##arg);                                    \
	} while (0)

#define RX_DET_LOG(fmt, arg...)                                                \
	do {                                                                   \
		if (myhdmi->debugtype & HDMI_RX_DEBUG_SYNC_DET)                \
			pr_info(fmt, ##arg);                                    \
	} while (0)

#define RX_STB_LOG(fmt, arg...)                                                \
	do {                                                                   \
		if (myhdmi->debugtype & HDMI_RX_DEBUG_STB)                     \
			pr_info(fmt, ##arg);                                    \
	} while (0)

#define RX_PROB_LOG(fmt, arg...)                                               \
	do {                                                                   \
		if (HDMI_RX_PROB_LOG)                                          \
			pr_info(fmt, ##arg);                                    \
	} while (0)

#define RX_DEF_LOG(fmt, arg...) pr_info(fmt, ##arg)

#define AUDFS_24KHz 6
#define AUDFS_48KHz 2
#define AUDFS_96KHz 10
#define AUDFS_192KHz 14

enum AUDIO_STATE {
	ASTATE_AudioOff = 0,
	ASTATE_RequestAudio,
	ASTATE_ResetAudio,
	ASTATE_WaitForReady,
	ASTATE_AudioUnMute,
	ASTATE_AudioOn,
	ASTATE_Reserved
};

enum AUDIO_EVENT_T { AUDIO_LOCK = 0, AUDIO_UNLOCK };

enum SYS_STATUS { ER_SUCCESS = 0, ER_FAIL, ER_RESERVED };

#define HDMI2_COM_INVALID_FRAME_RATE 1

#define HDMI2_HDCP2_CCHK_DONE_CHECK_COUNT 20

enum HdmiTmdsDataChannel_t {
	HDMI_TMDS_0,
	HDMI_TMDS_1,
	HDMI_TMDS_2,
	HDMI_TMDS_3_CLK
};

enum RX_DEEP_COLOR_MODE {
	RX_NON_DEEP = 0x00,
	RX_30BITS_DEEP_COLOR = 0x01,
	RX_36BITS_DEEP_COLOR = 0x02,
	RX_48BITS_DEEP_COLOR = 0x03
};

enum HDMI_RX_AUDIO_FS {
	SW_44p1K,
	SW_88p2K,
	SW_176p4K,
	SW_48K,
	SW_96K,
	SW_192K,
	SW_32K,
	HW_FS
};

#define HDMI_RX_CKDT_INT (1 << 0)
#define HDMI_RX_SCDT_INT (1 << 1)
#define HDMI_RX_VSYNC_INT (1 << 2)
#define HDMI_RX_CDRADIO_INT (1 << 3)
#define HDMI_RX_SCRAMBLING_INT (1 << 4)
#define HDMI_RX_FSCHG_INT (1 << 5)
#define HDMI_RX_DSDPKT_INT (1 << 7)
#define HDMI_RX_HBRPKT_INT (1 << 8)
#define HDMI_RX_CHCHG_INT (1 << 9)
#define HDMI_RX_AVI_INT (1 << 10)
#define HDMI_RX_HDMI_MODE_CHG (1 << 13)
#define HDMI_RX_HDMI_NEW_GCP (1 << 14)
#define HDMI_RX_PDTCLK_CHG (1 << 15)
#define HDMI_RX_H_UNSTABLE (1 << 16)
#define HDMI_RX_V_UNSTABLE (1 << 17)
#define HDMI_RX_SET_MUTE (1 << 18)
#define HDMI_RX_CLEAR_MUTE (1 << 19)
#define HDMI_RX_EMP_INT (1 << 20)
#define HDMI_RX_VSI_INT (1 << 21)
#define HDMI_RX_VRR_INT (1 << 22)
#define HDMI_RX_IRQ_ALL (0xFFFFFFFF)

#define HAL_Delay_us(delay) udelay(delay)
#define hal_delay_2us(delay) udelay(delay << 1)

#define HDCP2_2_KEY_SIZE 880
#ifdef CC_HDCP2_2_AES_ENCRYPTED
#define ENC_HDCP2_2_KEY_SIZE HDCP2_2_KEY_SIZE
#else
#define ENC_HDCP2_2_KEY_SIZE 916
#endif
#define PRIV_KEY_SIZE 16

struct HDCP2_ENC_INFO_T {
	u8 keytype;
	u8 au1key[1024];
	u16 u2Len;
};

struct HDCP2_KEY_INFO_T {
	u8 u1P[64];
	u8 u1Q[64];
	u8 u1DP[64];
	u8 u1DQ[64];
	u8 u1QINV[64];
	u8 u1ReceiverID[5];
	u8 u1RxPub[131];
	u8 u1Rsv[2];
	u8 u1NotUse[54];

	u8 u1PubSignature[384];
};

struct HDCP2_PRIMI_KEY_INFO_T {
	u8 u1LC[16];
	u8 u1ReceiverID[5];
	u8 u1RxPub[131];
	u8 u1Rsv[2];
	u8 u1PubSignature[384];
	u8 u1P[64];
	u8 u1Q[64];
	u8 u1DP[64];
	u8 u1DQ[64];
	u8 u1QINV[64];
	u8 u1SHA[20];
	u8 u1pad[2];
} __packed;

struct HDCP1_KEY_INFO_T {
	u8 u1BKSV[5];
	u8 u1CRCValue[2];
	u8 u1PrivateKey[280];
};

#define FLASH_HDCP2_2_KEY_SIZE 880
#define FLASH_HDCP1_4_KEY_SIZE 320

#define ENC_LC128_SIZE (64)
#define DEC_CERT_SIZE (522)
#define ENC_PRI_KEY_SIZE (368)

#define ENC_HDCP1_KEY_SIZE (336) /* Asset key320+ 16Byte) */

#define CERT_STRUCT_RECEIVER_ID (40 - 40)  /* 5 */
#define CERT_STRUCT_N (45 - 40)		   /* 128 */
#define CERT_STRUCT_E (173 - 40)	   /* 3 */
#define CERT_STRUCT_RESERVED (176 - 40)    /* 2 */
#define CERT_STRUCT_SIGNATURE (178 - 40)   /* 384 */
#define PRI_KEY_STRUCT_P (562 - 562)       /* 64 */
#define PRI_KEY_STRUCT_Q (626 - 562)       /* 64 */
#define PRI_KEY_STRUCT_MOD_P_1 (690 - 562) /* 64 */
#define PRI_KEY_STRUCT_MOD_Q_1 (754 - 562) /* 64 */
#define PRI_KEY_STRUCT_MOD_Q (818 - 562)   /* 64 */

#define CERT_STRUCT_RECEIVER_ID_SIZE 5
#define CERT_STRUCT_N_SIZE 128
#define CERT_STRUCT_E_SIZE 3
#define CERT_STRUCT_RESERVED_SIZE 2
#define CERT_STRUCT_SIGNATURE_SIZE 384
#define PRI_KEY_STRUCT_P_SIZE 64
#define PRI_KEY_STRUCT_Q_SIZE 64
#define PRI_KEY_STRUCT_MOD_P_1_SIZE 64
#define PRI_KEY_STRUCT_MOD_Q_1_SIZE 64
#define PRI_KEY_STRUCT_MOD_Q_SIZE 64

struct HDMIRX_ENC_LC128_T {
	u8 enc_lc128[ENC_LC128_SIZE];
};

struct HDMIRX_CERT_T {
	u8 dec_cert[DEC_CERT_SIZE];
};

struct HDMIRX_PRI_KEY_T {
	u8 enc_pri_key[ENC_PRI_KEY_SIZE];
};

struct HDMIRX_HDCP1_ENC_KEY_T {
	u8 enc_key[ENC_HDCP1_KEY_SIZE];
};

/**
 *0x00,	(1)
 *ksv,		(5)
 *0xff,0xff,0x00,	(3)
 *key,				(280)
 *0x00,0x00,0x00	(3)
 */
#define FLASH_HDCP_KEY_RESERVE (1 + 5 + 3 + 280 + 3)

#define HDMI_RX_DVI_WAIT_SPPORT_TIMING_COUNT (200 / HDMI_RX_TIMER_TICKET)

#define HDR10P_MAX_METADATA_BUF 10 //define max metadat buffer for debug purpose
#define DYNAMIC_METADATA_PKT_LEN 28
#define VSI_METADATA_LEN 28

#define DYNAMIC_METADATA_LEN (28*80) //for 10+ and DV EMP array length

#ifdef CC_SUPPORT_HDR10Plus
enum E_DVI_HDR10P_STATUS {
	DVI_HDR10P_NONE = 0,
	DVI_HDR10P_VSI,
	DVI_HDR10P_EMP
};

struct HDMI_HDR10PLUS_EMP_MATADATA_PACKETS_T {
	u8 au1DyMdPKT[DYNAMIC_METADATA_LEN];
};

struct HDMI_HDR10PLUS_EMP_METADATA_T {
	u8   uCountryCode;
	u16  u2ProviderCode;
	u16  u2OrientedCode;
	u8   uIdentifier;
	u8   uVersion;
	u8   uNumWindows;
	u16  u2WindowUpperLeftCornerX[3];
	u16  u2WindowUpperLeftCornerY[3];
	u16  u2WindowLowerRightCornerX[3];
	u16  u2WindowLowerRightCornerY[3];
	u16  u2CenterOfEllipseX[3];
	u16  u2CenterOfEllipseY[3];
	u8   uRotationAngle[3];
	u16  u2SemiMajorAxisInternalEllipse[3];
	u16  u2SemiMajorAxisExternalEllipse[3];
	u16  u2SemiMinorAxisExternalEllipse[3];
	u8   uOverlapProcessOption[3];

	u32  u4TargetSystemDisplayMaximumLuminance;
	bool    bTargetSystemDisplayActualPeakLuminanceFlag;

	u8   NumRowTargetSysDispActualPeakLum;
	u8   NumColTargetSysDispActualPeakLum;
	u8   TargetSysDispActualPeakLum[25][25];

	u32  u4Maxscl[3][3];
	u32  u4AverageMaxrgb[3];
	u8   uNumDistributionMaxrgbPercentiles[3];
	u8   uDistributionMaxrgbPercentages[3][15];
	u32  u4DistributionMaxrgbPercentiles[3][15];
	u16  u2FractionBrightPixels[3];

	bool    bMasteringDisplayActualPeakLuminanceFlag;

	u8   NumRowMasterDispActualPeakLum;
	u8   NumColMasterDispActualPeakLum;
	u8   MasterDispyActualPeakLum[25][25];

	bool    bToneMappingFlag[3];
	u16  u2KneePointX[3];
	u16  u2KneePointY[3];
	u8   uNumBezierCurveAnchors[3];
	u16  u2BezierCurveAnchors[3][15];
	bool    bColorSaturationMappingFlag[3];
	u8   uColorSaturationWeight[3];
};

struct HDR10Plus_EMP_T {
	bool fgHdr10PlusEMPExist;
	u16 u2CurrEMPMetaIndex;
	u16 u2Length;
	struct HDMI_HDR10PLUS_EMP_METADATA_T rHdr10PlusEmpMetadata;
};

struct HDR10Plus_VSI_METADATA_T {
	u8 u1ApplicationVersion;
	u8 u1TargetSystemDisplayMaximumLuminance;
	u8 u1AverageMaxrgb;
	u8 au1DistributionValue[9];
	u8 u1NumBezierCurveAnchrs;
	u16 u2KneePointX;
	u16 u2KneePointY;
	u8 u1BezierCurveAnchors[9];
	u8 u1GraphicsOverlayFlag;
	u8 u1NoDelayFlag;
};

struct HDR10Plus_VSI_T {
	bool fgHdr10PlusVsiExist;
	u16 u2CurrVSIMetadataIndex;
	u16 u2Length;
	struct HDR10Plus_VSI_METADATA_T rHdr10PlusVsiMetadata;
};
#endif

enum { DVI_NO_SIGNAL, DVI_CHK_MODECHG, DVI_WAIT_STABLE, DVI_SATE_MAX };

enum { DVI_3D_NONE,
	DVI_FP_progressive,
	DVI_FP_interlace,
	DVI_SBS_full_progressive,
	DVI_SBS_full_interlace,
	DVI_Field_Alternative };

struct DVI_STATUS {
	u8 DviChkState;
	u8 bDviModeChgFlg;
	u8 DviVclk;
	u32 DviHclk;
	u32 DviPixClk;
	u32 DviWidth;
	u32 DviHeight;
	u32 DviHtotal;
	u32 DviVtotal;
	u8 Interlace;
	u8 DviTiming;
	u8 HdmiMD; /* 1: hdmi, 0: dvi */
	u8 HDMICS; /* intput colorspace */

	u8 ModeChgCnt;
	u8 DeChgCnt;
	u8 PixelClkChgCnt;
	u8 ColorChgCnt;
	u8 NoSigalCnt;
	u8 NoConnCnt;
	u8 InfoChgCnt;
#ifdef CC_SUPPORT_HDR10Plus
	enum E_DVI_HDR10P_STATUS _rHdr10PStatus;
#endif
};

#define ACR_LOCK_TIMEOUT (400 / HDMI_RX_TIMER_TICKET)

#define HDMIRX_INT_STATUS_CHK                                                  \
	(HDMI_AUDIO_FIFO_UNDERRUN | HDMI_AUDIO_FIFO_OVERRUN | HDMI_AUDIO_FS_CHG)

#define B_CAP_AUDIO_ON (1 << 7)
#define B_CAP_HBR_AUDIO (1 << 6)
#define B_CAP_DSD_AUDIO (1 << 5)
#define B_LAYOUT (1 << 4)
#define B_MULTICH (1 << 4)
#define B_HBR_BY_SPDIF (1 << 3)

#define B_CAP_AUDIO_ON_SFT 7
#define B_CAP_HBR_AUDIO_SFT 6
#define B_CAP_DSD_AUDIO_SFT 5
#define B_MULTICH_SFT 4
#define B_HBR_BY_SPDIF_SFT 3

#define B_AUDIO_ON (1 << 7)
#define B_HBRAUDIO (1 << 6)
#define B_DSDAUDIO (1 << 5)
#define B_AUDIO_LAYOUT (1 << 4)
#define M_AUDIO_CH 0xF
#define B_AUDIO_SRC_VALID_3 (1 << 3)
#define B_AUDIO_SRC_VALID_2 (1 << 2)
#define B_AUDIO_SRC_VALID_1 (1 << 1)
#define B_AUDIO_SRC_VALID_0 (1 << 0)

#define F_AUDIO_ON (1 << 7)
#define F_AUDIO_HBR (1 << 6)
#define F_AUDIO_DSD (1 << 5)
#define F_AUDIO_NLPCM (1 << 4)
#define F_AUDIO_LAYOUT_1 (1 << 3)
#define F_AUDIO_LAYOUT_0 (0 << 3)

#define T_AUDIO_MASK 0xF0
#define T_AUDIO_OFF 0
#define T_AUDIO_HBR (F_AUDIO_ON | F_AUDIO_HBR)
#define T_AUDIO_DSD (F_AUDIO_ON | F_AUDIO_DSD)
#define T_AUDIO_NLPCM (F_AUDIO_ON | F_AUDIO_NLPCM)
#define T_AUDIO_LPCM (F_AUDIO_ON)

enum RX_SW_MCLK_FREQUNECY_T {
	RX_SW_MCLK_128FS,
	RX_SW_MCLK_256FS,
	RX_SW_MCLK_384FS,
	RX_SW_MCLK_512FS
};

enum SAMPLE_FREQUENCY_T {
	MCLK_128FS,
	MCLK_192FS,
	MCLK_256FS,
	MCLK_384FS,
	MCLK_512FS,
	MCLK_768FS,
	MCLK_1152FS,
};

enum AUDIO_FORMAT { FORMAT_RJ = 0, FORMAT_LJ = 1, FORMAT_I2S = 2 };

enum LRCK_CYC_T { LRCK_CYC_16, LRCK_CYC_24, LRCK_CYC_32 };

enum HDMI_AUDIO_I2S_FMT_T {
	HDMI_RJT_24BIT = 0,
	HDMI_RJT_16BIT,
	HDMI_LJT_24BIT,
	HDMI_LJT_16BIT,
	HDMI_I2S_24BIT,
	HDMI_I2S_16BIT
};

enum HDMI_Rx_DEBUG_MESSAGE_T {
	HDMI_RX_DEBUG_EDID = (1 << 0),
	HDMI_RX_DEBUG_HOT_PLUG = (1 << 1),
	HDMI_RX_DEBUG_HDCP = (1 << 2),
	HDMI_RX_DEBUG_HDCP_RI = (1 << 3),
	HDMI_RX_DEBUG_HV_TOTAL = (1 << 4),
	HDMI_RX_DEBUG_AUDIO = (1 << 5),
	HDMI_RX_DEBUG_INFOFRAME = (1 << 6),
	HDMI_RX_DEBUG_DEEPCOLOR = (1 << 7),
	HDMI_RX_DEBUG_3D = (1 << 8),
	HDMI_RX_DEBUG_XVYCC = (1 << 9),
	HDMI_RX_DEBUG_SYNC_DET = (1 << 10),
	HDMI_RX_DEBUG_MHL = (1 << 11),
	HDMI_RX_DEBUG_STB = (1 << 12),
	HDMI_RX_DEF_LOG = (1 << 31),
};

enum VSW_NFY_COND_T {
	VSW_NFY_ERROR = -1,
	VSW_NFY_LOCK = 1,
	VSW_NFY_UNLOCK,
	VSW_NFY_RES_CHGING,
	VSW_NFY_RES_CHG_DONE,
	VSW_NFY_ASPECT_CHG,
	VSW_NFY_CS_CHG,
	VSW_NFY_JPEG_CHG,
	VSW_NFY_CINEMA_CHG
};

#define ACP_TYPE_GENERAL_AUDIO 0x00
#define ACP_TYPE_IEC60958 0x01
#define ACP_TYPE_DVD_AUDIO 0x02
#define ACP_TYPE_SACD 0x03
#define ACP_LOST_DISABLE 0xFF

struct HDMI_RX_E_T {
	u8 u1HdmiRxEDID[512];
};

struct HDMIRX_TIMING_T {
	u32 h_total;
	u32 v_total;
	u32 h_active;
	u32 v_active;
	u32 h_frontporch;
	u32 h_backporch;
	u32 v_frontporch_even;
	u32 v_backporch_even;
	u32 v_frontporch_odd;
	u32 v_backporch_odd;
	bool is_interlace;
	u32 pixel_clk;
	u32 vs_clk;
	u32 hs_clk;
	u32 mode_id;
};

struct HDMIRX_INFO_T {
	bool is_5v_on;
	bool is_stable;
	bool is_timing_lock;
	bool is_hdmi_mode;
	bool is_rgb;
	u8 deep_color;
	struct HDMIRX_TIMING_T timing;
};

struct HDMIRX_PKT_T {
	u8 buf[32];
};

struct HDMIRX_AUD_INFO_T {
	bool is_aud_lock;
	struct AUDIO_INFO aud;
};

struct HDMIRX_HDCP_KEY14_T {
	u8 u1Hdcprxkey[384];
};

struct HDMIRX_HDCP_KEY22_T {
	u8 u1Hdcprxkey[384];
};

struct VGAMODE {
	u16 IHF;       /* Horizontal Frequency for timing search */
	u8 IVF;	/* Vertical Frequency for timing search */
	u16 ICLK;      /* Pixel Frequency */
	u16 IHTOTAL;   /* H Total */
	u16 IVTOTAL;   /* V Total */
	u16 IPH_SYNCW; /* H Sync Width */
	u16 IPH_WID;   /* H Resolution */
	u16 IPH_BP;    /* H Back Porch */
	u16 IPV_STA;   /* V Back Porch + Sync Width */
	u16 IPV_LEN;   /* V Resolution */
	u16 COMBINE;   /* ?? */
	u8 timing_id;
};

enum HDMITX_PORT {
	HDMI_PLUG_IN_ONLY = 0,
	HDMI_PLUG_IN_AND_SINK_POWER_ON,
	HDMI_PLUG_OUT,
	HDMI_PLUG_IDLE
};

enum HDMI_CTRL_STATE_T {
	HDMI_STATE_IDLE = 0,
	HDMI_STATE_HOT_PLUG_OUT,
	HDMI_STATE_HOT_PLUGIN_AND_POWER_ON,
	HDMI_STATE_HOT_PLUG_IN_ONLY,
	HDMI_STATE_READ_EDID,
	HDMI_STATE_POWER_ON_READ_EDID,
	HDMI_STATE_POWER_OFF_HOT_PLUG_OUT,
};

#define E_SUPPORT_HD_AUD

#ifdef E_SUPPORT_HD_AUD
#define E_DEF_BL1_ADDR_PHY 0x29
#else
#define E_DEF_BL1_ADDR_PHY 0x20
#endif

//#define HDMI_INPUT_COUNT 3
#define HDMI_INPUT_COUNT 1

#define EEPROM_ID 0xA0
#define E_BLOCK_SIZE 128
#define E_ADR_CHK_SUM 0x7F
#define E_BL0_ADR_EXT_NMB 0x7E

#define E_BL0_ADR_HEADER 0x00
#define E_BL0_LEN_HEADER 8

#define E_BL0_ADR_VER 0x12
#define E_BL0_ADR_REV 0x13

#define E_MON_NAME_DTD 0xFC
#define E_MON_RANGE_DTD 0xFD

#define E_BL1_ADR_DTD_OFFSET 0x02
#define E_BL0_ADR_DTDs 0x36
#define END_1stPAGE_DESCR_ADDR 0x7E

#define E_WEEK 0x00
#define E_YEAR 0x15
// Min. Vertical rate (for interlace this refers to field rate), in Hz.
#define DEF_MIN_V_HZ 48
// Max. Vertical rate (for interlace this refers to field rate), in Hz.
#define DEF_MAX_V_HZ 62
// Min. Horizontal in kHz
#define DEF_MIN_H_KHZ 23
// Max. Horizontal in kHz,
#define DEF_MAX_H_KHZ 47
// Max. Supported Pixel Clock, in MHz/10, rounded
#define DEF_MAX_PIXCLK_10MHZ 8

#define E_VID_BLK_LEN (13) // total of 4k res is 10
#define E_VID_BLK_LEN_1080P (14)
#define E_420_BIT_MAP_LEN ((E_VID_BLK_LEN / 8) + 1)
#define E_420_CMDB_LEN (E_420_BIT_MAP_LEN + 2)
#define E_420_VDB_LEN 6
#define E_HF_VEND_BLK_LEN 11
#define E_HDR_SMDB_LEN 7
#define E_HF_VD_BLK_LEN 8
#define E_DOVIVISION_DB_LEN 32

#ifdef E_SUPPORT_HD_AUD
#define E_AUD_BLK_LEN 28
#define E_AUD2CHPCM_ONLY_LEN 4 //22
#else
#define E_AUD_BLK_LEN 16
#define E_AUD2CHPCM_ONLY_LEN 4 //13
#endif

#define E_SPK_BLK_LEN 4
#define E_VEND_FULL_L (14)
#define E_VEND_BLK_4K2K_FULL_LEN 19

#define E_VEND_BLK_LEN 7

#define E_VEND_BLK_MINI_LEN 7
#define E_COLORMETRY_BLK_LEN 4
#define E_VCDB_BLOCK_LEN 3
#define E_DTD_BLOCK_LEN (2 * 18)
#define E_DTD_BLOCK_1080P_LEN (18)

#define AUDIO_TAG 0x20
#define VIDEO_TAG 0x40
#define VENDOR_TAG 0x60
#define SPEAKER_TAG 0x80

#define EEPROM0 0
#define EEPROM1 1
#define EEPROM2 2

#define EDIDDEV0 0
#define EDIDDEV1 1
#define EDIDDEV2 2

struct PHY_ADDR_PARA_T {
	u16 Org;
	u16 Dvd;
	u16 Sat;
	u16 Tv;
	u16 Else;
};

struct E_PARA_T {
	u8 PHYLevel;
	u8 bBlock0Err;
	u8 bBlock1Err;
	u8 PHYPoint;
	u8 bCopyDone;
	u8 bDownDvi;
	u8 Number;
};

enum E_CEA_TYPE {
	E_CEA_TAG = 0,
	E_CEA_VERSION,
	E_CEA_D,
	E_CEA_3,
	E_CEA_VID_DB,
	E_CEA_AUD_DB,
	E_CEA_SPK_DB,
	E_CEA_VS_DB,
	E_CEA_EXT_VC_DB,
	E_CEA_EXT_VSV_DB,
	E_CEA_EXT_VESA_DISP_DB,
	E_CEA_EXT_VESA_VID_DB,
	E_CEA_EXT_CM_DB,
	E_CEA_EXT_HF_VS_DB,
	E_CEA_EXT_420_VID_DB,
	E_CEA_EXT_420_CM_DB,
	E_CEA_EXT_VS_AUD_DB,
	E_CEA_EXT_INFO_DB,
};

#define F_PIX_MAX 22275 // 1080P 12bits

#define THREE_D_EDID
#define MAX_HDMI_SHAREINFO 64
#define EDID_SIZE 512

#define SINK_480P (1 << 0)
#define SINK_720P60 (1 << 1)
#define SINK_1080I60 (1 << 2)
#define SINK_1080P60 (1 << 3)
#define SINK_480P_1440 (1 << 4)
#define SINK_480P_2880 (1 << 5)
#define SINK_480I (1 << 6)      // actuall 480Ix1440
#define SINK_480I_1440 (1 << 7) // actuall 480Ix2880
#define SINK_480I_2880 (1 << 8) // No this type for 861D
#define SINK_1080P30 (1 << 9)
#define SINK_576P (1 << 10)
#define SINK_720P50 (1 << 11)
#define SINK_1080I50 (1 << 12)
#define SINK_1080P50 (1 << 13)
#define SINK_576P_1440 (1 << 14)
#define SINK_576P_2880 (1 << 15)
#define SINK_576I (1 << 16)
#define SINK_576I_1440 (1 << 17)
#define SINK_576I_2880 (1 << 18)
#define SINK_1080P25 (1 << 19)
#define SINK_1080P24 (1 << 20)
#define SINK_1080P23976 (1 << 21)
#define SINK_1080P2997 (1 << 22)
#define SINK_VGA (1 << 23)      // 640x480P
#define SINK_480I_4_3 (1 << 24) // 720x480I 4:3
#define SINK_480P_4_3 (1 << 25) // 720x480P 4:3
#define SINK_480P_2880_4_3 (1 << 26)
#define SINK_576I_4_3 (1 << 27) // 720x480I 4:3
#define SINK_576P_4_3 (1 << 28) // 720x480P 4:3
#define SINK_576P_2880_4_3 (1 << 29)
#define SINK_720P24 (1 << 30)
#define SINK_720P23976 (1 << 31)

#define SINK_2160P_23_976HZ (1 << 0)
#define SINK_2160P_24HZ (1 << 1)
#define SINK_2160P_25HZ (1 << 2)
#define SINK_2160P_29_97HZ (1 << 3)
#define SINK_2160P_30HZ (1 << 4)

#define SINK_2161P_23_976HZ (1 << 5)
#define SINK_2161P_24HZ (1 << 6)
#define SINK_2161P_25HZ (1 << 7)
#define SINK_2161P_29_97HZ (1 << 8)
#define SINK_2161P_30HZ (1 << 9)

#define SINK_2160P_50HZ (1 << 10)
#define SINK_2160P_60HZ (1 << 11)
#define SINK_2161P_50HZ (1 << 12)
#define SINK_2161P_60HZ (1 << 13)

#define HDMI_SINK_AUDIO_DEC_LPCM (1 << 0)
#define HDMI_SINK_AUDIO_DEC_AC3 (1 << 1)
#define HDMI_SINK_AUDIO_DEC_MPEG1 (1 << 2)
#define HDMI_SINK_AUDIO_DEC_MP3 (1 << 3)
#define HDMI_SINK_AUDIO_DEC_MPEG2 (1 << 4)
#define HDMI_SINK_AUDIO_DEC_AAC (1 << 5)
#define HDMI_SINK_AUDIO_DEC_DTS (1 << 6)
#define HDMI_SINK_AUDIO_DEC_ATRAC (1 << 7)
#define HDMI_SINK_AUDIO_DEC_DSD (1 << 8)
#define HDMI_SINK_AUDIO_DEC_DOVI_PLUS (1 << 9)
#define HDMI_SINK_AUDIO_DEC_DTS_HD (1 << 10)
#define HDMI_SINK_AUDIO_DEC_MAT_MLP (1 << 11)
#define HDMI_SINK_AUDIO_DEC_DST (1 << 12)
#define HDMI_SINK_AUDIO_DEC_WMA (1 << 13)

#define SINK_YCBCR_444 (1 << 0)
#define SINK_YCBCR_422 (1 << 1)
#define SINK_XV_YCC709 (1 << 2)
#define SINK_XV_YCC601 (1 << 3)
#define SINK_METADATA0 (1 << 4)
#define SINK_METADATA1 (1 << 5)
#define SINK_METADATA2 (1 << 6)
#define SINK_RGB (1 << 7)
#define SINK_COLOR_SPACE_BT2020_CYCC (1 << 8)
#define SINK_COLOR_SPACE_BT2020_YCC (1 << 9)
#define SINK_COLOR_SPACE_BT2020_RGB (1 << 10)
#define SINK_YCBCR_420 (1 << 11)
#define SINK_YCBCR_420_CAPABILITY (1 << 12)

#define SINK_METADATA3 (1 << 13)
#define SINK_S_YCC601 (1 << 14)
#define SINK_ADOBE_YCC601 (1 << 15)
#define SINK_ADOBE_RGB (1 << 16)

// (5) Define the EDID relative information
// (5.1) Define one EDID block length
#define E_BLK_LEN 128
// (5.2) Define EDID header length
#define EDID_HEADER_LEN 8
// (5.3) Define the address for EDID info. (ref. EDID Recommended Practive for
// EIA/CEA-861)
// Base Block 0
#define EDID_ADDR_HEADER 0x00
#define EDID_ADDR_VERSION 0x12
#define EDID_ADDR_REVISION 0x13
#define EDID_IMAGE_HORIZONTAL_SIZE 0x15
#define EDID_IMAGE_VERTICAL_SIZE 0x16
#define EDID_ADDR_FEATURE_SUPPORT 0x18
#define E_ADR_T_DSPR_1 0x36
#define E_ADR_T_DSPR_2 0x48
#define E_ADR_MTR_DSPR_1 0x5A
#define E_ADR_MTR_DSPR_2 0x6C
#define EDID_ADDR_EXT_BLOCK_FLAG 0x7E
#define EDID_ADDR_EXTEND_BYTE3 0x03 // EDID address: 0x83
// Extension Block 1:
#define EXTEDID_ADDR_TAG 0x00
#define EXTEDID_ADDR_REVISION 0x01
#define EXTEDID_ADDR_OFST_TIME_DSPR 0x02
// (5.4) Define the ID for descriptor block type
// Notice: reference Table 11 ~ 14 of "EDID Recommended Practive for
// EIA/CEA-861"
#define DETAIL_TIMING_DESCRIPTOR -1
#define UNKNOWN_DESCRIPTOR -255
#define MONITOR_NAME_DESCRIPTOR 0xFC
#define MONITOR_RANGE_LIMITS_DESCRIPTOR 0xFD

// (5.5) Define the offset address of info. within detail timing descriptor
// block
#define OFST_PXL_CLK_LO 0
#define OFST_PXL_CLK_HI 1
#define OFST_H_ACTIVE_LO 2
#define OFST_H_BLANKING_LO 3
#define OFST_H_ACT_BLA_HI 4
#define OFST_V_ACTIVE_LO 5
#define OFST_V_BLANKING_LO 6
#define OFST_V_ACTIVE_HI 7
#define OFST_FLAGS 17
// (5.6) Define the ID for EDID extension type
#define LCD_TIMING 0x1
#define CEA_TIMING_EXTENSION 0x01
#define EDID_20_EXTENSION 0x20
#define COLOR_INFO_TYPE0 0x30
#define DVI_FEATURE_DATA 0x40
#define TOUCH_SCREEN_MAP 0x50
#define BLOCK_MAP 0xF0
#define EXTENSION_DEFINITION 0xFF
// (5.7) Define EDID VSDB header length
#define EDID_VSDB_LEN 0x03
#define EDID_VSVDB_LEN 0x03

#define HDR_PACKET_DISABLE 0x00
#define HDR_PACKET_ACTIVE 0x01
#define HDR_PACKET_ZERO 0x02

#define HDR_DEBUG_DISABLE_METADATA (1 << 0)
#define HDR_DEBUG_DISABLE_HDR (1 << 1)
#define HDR_DEBUG_DISABLE_BT2020 (1 << 2)
#define HDR_DEBUG_DISABLE_DV_HDR (1 << 3)
#define HDR_DEBUG_DISABLE_PHI_HDR (1 << 4)

// HDMI_SINK_VCDB_T Each bit defines the VIDEO Capability Data Block of EDID.
#define SINK_CE_ALWAYS_OVERSCANNED (1 << 0)
#define SINK_CE_ALWAYS_UNDERSCANNED (1 << 1)
#define SINK_CE_BOTH_OVER_AND_UNDER_SCAN (1 << 2)
#define SINK_IT_ALWAYS_OVERSCANNED (1 << 3)
#define SINK_IT_ALWAYS_UNDERSCANNED (1 << 4)
#define SINK_IT_BOTH_OVER_AND_UNDER_SCAN (1 << 5)
#define SINK_PT_ALWAYS_OVERSCANNED (1 << 6)
#define SINK_PT_ALWAYS_UNDERSCANNED (1 << 7)
#define SINK_PT_BOTH_OVER_AND_UNDER_SCAN (1 << 8)
#define SINK_RGB_SELECTABLE (1 << 9)

// Sink audio channel ability for a fixed Fs
#define SINK_AUDIO_2CH (1 << 0)
#define SINK_AUDIO_3CH (1 << 1)
#define SINK_AUDIO_4CH (1 << 2)
#define SINK_AUDIO_5CH (1 << 3)
#define SINK_AUDIO_6CH (1 << 4)
#define SINK_AUDIO_7CH (1 << 5)
#define SINK_AUDIO_8CH (1 << 6)

// Sink supported sampling rate for a fixed channel number
#define SINK_AUDIO_32k (1 << 0)
#define SINK_AUDIO_44k (1 << 1)
#define SINK_AUDIO_48k (1 << 2)
#define SINK_AUDIO_88k (1 << 3)
#define SINK_AUDIO_96k (1 << 4)
#define SINK_AUDIO_176k (1 << 5)
#define SINK_AUDIO_192k (1 << 6)

// The following definition is for Sink speaker allocation data block .
#define SINK_AUDIO_FL_FR (1 << 0)
#define SINK_AUDIO_LFE (1 << 1)
#define SINK_AUDIO_FC (1 << 2)
#define SINK_AUDIO_RL_RR (1 << 3)
#define SINK_AUDIO_RC (1 << 4)
#define SINK_AUDIO_FLC_FRC (1 << 5)
#define SINK_AUDIO_RLC_RRC (1 << 6)

// The following definition is
// For EDID Audio Support, //HDMI_EDID_CHKSUM_AND_AUDIO_SUP_T
#define SINK_BASIC_AUDIO_NO_SUP (1 << 0)
#define SINK_SAD_NO_EXIST (1 << 1) // short audio descriptor
#define SINK_BASE_BLK_CHKSUM_ERR (1 << 2)
#define SINK_EXT_BLK_CHKSUM_ERR (1 << 3)

#define SINK_CNC0_GRAPHICS (1 << 0)
#define SINK_CNC1_PHOTO (1 << 1) // short audio descriptor
#define SINK_CNC2_CINEMA (1 << 2)
#define SINK_CNC3_GAME (1 << 3)

enum AUDIO_BITSTREAM_TYPE_T {
	AVD_BITS_NONE = 0x00,
	AVD_LPCM = 0x01,
	AVD_AC3 = 0x02,
	AVD_MPEG1_AUD = 0x03,
	AVD_MP3 = 0x04,
	AVD_MPEG2_AUD = 0x05,
	AVD_AAC = 0x06,
	AVD_DTS = 0x07,
	AVD_ATRAC = 0x08,
	AVD_DSD = 0x09,
	AVD_DOVI_PLUS = 0x0A,
	AVD_DTS_HD = 0x0B,
	AVD_MAT_MLP = 0x0C,
	AVD_DST = 0x0D,
	AVD_WMA = 0x0E,
	AVD_CDDA = 0x0F,
	AVD_SACD_PCM = 0x10,
	AVD_APE = 0x11,
	AVD_FLAC = 0x12,
	AVD_VOBIS = 0x13,
	AVD_AMR = 0x14,
	AVD_AWB = 0x15,
	AVD_ALAC = 0x16,
	AVD_OPUS = 0x17,
	AVD_WAV_MQA = 0x18,
	AVD_FLAC_MQA = 0x19,
	AVD_ALAC_MQA = 0x1A,
	AVD_ATMOS = 0x1B,
	AVD_DTSX = 0x1C,
	AVD_HDCD = 0xfe,
	AVD_BITS_OTHERS = 0xff
};

enum EDID_DYNAMIC_HDR_T {
	EDID_SUPPORT_PHILIPS_HDR = (1 << 0),
	EDID_SUPPORT_DV_HDR = (1 << 1),
	EDID_SUPPORT_YUV422_12BIT = (1 << 2),
	EDID_SUPPORT_DV_HDR_2160P60 = (1 << 3),
};

enum EDID_STATIC_HDR_T {
	EDID_SUPPORT_SDR = (1 << 0),
	EDID_SUPPORT_HDR = (1 << 1),
	EDID_SUPPORT_SMPTE_ST_2084 = (1 << 2),
	EDID_SUPPORT_FUTURE_EOTF = (1 << 3),
	EDID_SUPPORT_ET_4 = (1 << 4),
	EDID_SUPPORT_ET_5 = (1 << 5),
};

enum EDID_HF_VSDB_INFO_T {
	EDID_HF_VSDB_3D_OSD_DISPARITY = (1 << 0),
	EDID_HF_VSDB_DUAL_VIEW = (1 << 1),
	EDID_HF_VSDB_INDEPENDENT_VIEW = (1 << 2),
	EDID_HF_VSDB_LTE_340_SCR = (1 << 3),
	EDID_HF_VSDB_RR_CAPABLE = (1 << 6),
	EDID_HF_VSDB_SCDC_PRESENT = (1 << 7),
};

enum HDMI_SHARE_INFO_TYPE_T {
	SI_VSDB_EXIST = 0,
	SI_HDMI_RECEIVER_STATUS,
	SI_HDMI_PORD_OFF_PLUG_ONLY,
	SI_EDID_EXT_BLK_NO,
	SI_EDID_RESULT,
	SI_SUPP_AI,
	SI_HDMI_HDCP_RESULT,
	SI_REPEATER_DEVICE_COUNT,
	SI_HDMI_CEC_LA,
	SI_HDMI_CEC_ACTIVE_SOURCE,
	SI_HDMI_CEC_PROCESS,
	SI_HDMI_CEC_PARA0,
	SI_HDMI_CEC_PARA1,
	SI_HDMI_CEC_PARA2,
	SI_HDMI_NO_HDCP_TEST,
	SI_HDMI_SRC_CONTROL,
	SI_A_CODE_MODE,
	SI_EDID_AUDIO_CAPABILITY,
	SI_HDMI_AUDIO_INPUT_SOURCE,
	SI_HDMI_AUDIO_CH_NUM,
	SI_HDMI_DVD_AUDIO_PROHIBIT,
	SI_DVD_HDCP_REVOCATION_RESULT,
};

enum HDMI_SINK_DEEP_COLOR_T {
	HDMI_SINK_NO_DEEP_COLOR = 0,
	HDMI_SINK_DEEP_COLOR_10_BIT = (1 << 0),
	HDMI_SINK_DEEP_COLOR_12_BIT = (1 << 1),
	HDMI_SINK_DEEP_COLOR_16_BIT = (1 << 2)
};

enum HDMI_DEEP_COLOR_T {
	HDMI_DEEP_COLOR_AUTO = 0,
	HDMI_NO_DEEP_COLOR,
	HDMI_DEEP_COLOR_10_BIT,
	HDMI_DEEP_COLOR_12_BIT,
	HDMI_DEEP_COLOR_16_BIT,
	HDMI_DEEP_COLOR_10_BIT_DITHER,
	HDMI_DEEP_COLOR_8_BIT_DITHER
};

struct HDMI_SINK_AV_CAP_T {
	u32 s_cea_ntsc_res;     // use HDMI_SINK_VIDEO_RES_T
	u32 s_cea_pal_res;      // use HDMI_SINK_VIDEO_RES_T
	u32 s_org_cea_ntsc_res; // use HDMI_SINK_VIDEO_RES_T
	u32 s_org_cea_pal_res;  // use HDMI_SINK_VIDEO_RES_T
	u32 s_dtd_ntsc_res;     // use HDMI_SINK_VIDEO_RES_T
	u32 s_dtd_pal_res;      // use HDMI_SINK_VIDEO_RES_T
	u32 s_1st_dtd_ntsc_res; // use HDMI_SINK_VIDEO_RES_T
	u32 s_1st_dtd_pal_res;  // use HDMI_SINK_VIDEO_RES_T
	u32 s_native_ntsc_res;  // use HDMI_SINK_VIDEO_RES_T
	u32 s_native_pal_res;   // use HDMI_SINK_VIDEO_RES_T
	u32 s_colorimetry;      // use HDMI_SINK_VIDEO_COLORIMETRY_T
	u16 s_vcdb_data;	// use HDMI_SINK_VCDB_T
	u16 s_aud_dec;		// HDMI_SINK_AUDIO_DECODER_T
	u8 s_dsd_ch_num;
	u32 s_dts_fs;
	u8 s_dts_ch_samp[7]; // n: channel number index, value: each bit means
			     // sampling rate for this channel number
			     // (SINK_AUDIO_32k..)
	u8 s_pcm_ch_samp[7]; // n: channel number index, value: each bit means
			     // sampling rate for this channel number
			     // (SINK_AUDIO_32k..)
	u8 s_pcm_bit_size[7]; ////n: channel number index, value: each bit means
			      ///bit size for this channel number
	u8 s_dst_ch_samp[7];  // n: channel number index, value: each bit means
			     // sampling rate for this channel number
			     // (SINK_AUDIO_32k..)
	u8 s_dsd_ch_samp[7]; // n: channel number index, value: each bit means
			     // sampling rate for this channel number
			     // (SINK_AUDIO_32k..)
	u8 s_org_pcm_ch_samp[7];  // original PCM sampling data
	u8 s_org_pcm_bit_size[7]; // original PCM bit Size
	u8 s_ac3_ch_samp[7];
	u8 s_mpeg1_ch_samp[7];
	u8 s_mp3_ch_samp[7];
	u8 s_mpeg2_ch_samp[7];
	u8 s_aac_ch_samp[7];
	u8 s_atrac_ch_samp[7];
	u8 s_dbplus_ch_samp[7];
	u8 s_dtshd_ch_samp[7];
	u8 s_mat_mlp_ch_samp[7];
	u8 s_wma_ch_samp[7];
	u16 s_max_tmds_clk;
	u8 s_spk_allocation;
	u8 s_content_cnc;
	u8 s_p_latency_present;
	u8 s_i_latency_present;
	u8 s_p_audio_latency;
	u8 s_p_video_latency;
	u8 s_i_audio_latency;
	u8 s_i_vid_latency;
	u8 s_rgb_color_bit;
	u8 s_ycc_color_bit;
	u8 s_max_tmds;		     // kenny add 2010/4/25
	u16 edid_chksum_and_aud_sup; // HDMI_EDID_CHKSUM_AND_AUDIO_SUP_T
	u16 s_cec_addr;
	bool s_edid_ready;
	bool s_supp_hdmi_mode;
	u8 s_ExtEdid_Rev;
	u8 s_edid_ver;
	u8 s_edid_rev;
	u8 s_supp_ai;
	u8 s_Disp_H_Size;
	u8 s_Disp_V_Size;
	u32 s_hdmi_4k2kvic;
	u32 s_4k2kvic_420_vdb;
	bool s_vid_present;
	u8 s_CNC;
	bool s_3D_present;
	u32 s_cea_3D_res;
	u16 s_3D_struct;
	u32 s_cea_FP_3D;
	u32 s_cea_TOB_3D;
	u32 s_cea_SBS_3D;
	u8 s_SCDC_present;
	u8 s_LTE_340M_sramble;
	u8 _bMonitorName[13];
	// add for BDP00062297 2D/3D issue with OTP
	u16 s_ID_manu_name;    //(08H~09H)
	u16 s_ID_product_code; //(0aH~0bH)
	u32 s_ID_serial_num;   //(0cH~0fH)
	u8 s_week_of_manu;     //(10H)
	u8 s_year_of_manu;     //(11H)  base on year 1990
	// add end
	u32 s_id_serial_num;
	u8 s_supp_static_hdr;
	u8 s_supp_dynamic_hdr;
	u8 s_hf_vsdb_exist;
	u8 s_max_tmds_char_rate;
	u8 s_hf_vsdb_info;
	u8 s_dc420_color_bit;
	/* allm */
	u8 s_allm_en;
	/* vrr */
	u8 s_vrr_en;
	u8 s_cnmvrr;
	u8 s_cinemavrr;
	u8 s_mdelta;
	u32 vrr_min;
	u32 vrr_max;
	u8 s_hdr_content_max_luma;
	u8 s_hdr_content_max_frame_avg_luma;
	u8 s_hdr_content_min_luma;
	u8 s_hdr_block[7];     // add for hdmi rx merge EDID
	u8 s_hfvsdb_blk[8];    // add for hdmi rx merge EDID
	u8 s_dbvision_blk[32]; // add for DV
	u32 s_dbvision_vsvdb_len;
	u32 s_dbvision_vsvdb_ver;
	u32 s_dbvision_vsvdb_v1_low_latency;
	u32 s_dbvision_vsvdb_v2_interface;
	u32 s_dbvision_vsvdb_low_latency_supp;
	u32 s_dbvision_vsvdb_v2_supp_10b12b_444;
	u32 s_dbvision_vsvdb_supp_backlight_ctrl;
	u32 s_dbvision_vsvdb_backlt_min_luma;
	u32 s_dbvision_vsvdb_tmin;
	u32 s_dbvision_vsvdb_tmax;
	u32 s_dbvision_vsvdb_tminPQ;
	u32 s_dbvision_vsvdb_tmaxPQ;
	u32 s_dbvision_vsvdb_Rx;
	u32 s_dbvision_vsvdb_Ry;
	u32 s_dbvision_vsvdb_Gx;
	u32 s_dbvision_vsvdb_Gy;
	u32 s_dbvision_vsvdb_Bx;
	u32 s_dbvision_vsvdb_By;
	u32 s_dbvision_vsvdb_Wx;
	u32 s_dbvision_vsvdb_Wy;
};

enum HdmiTmdsClkRate_t {
	HDMI_TMDS_CLOCK_UNKNOWN,
	HDMI_TMDS_CLOCK_27, //freq <=  27.000
	HDMI_TMDS_CLOCK_27_33P75, //27.000 < freq <=  33.750
	HDMI_TMDS_CLOCK_33P75_40P5, //33.750 < freq <=  40.500
	HDMI_TMDS_CLOCK_40P5_54, //40.500 < freq <=  54.000
	HDMI_TMDS_CLOCK_54_75, //54.000 < freq <=  75.000
	HDMI_TMDS_CLOCK_75_93P75, //75.000 < freq <=  97.750
	HDMI_TMDS_CLOCK_93P75_112P5, //93.750 < freq <= 112.500
	HDMI_TMDS_CLOCK_112P5_148P5, //112.500 < freq <= 148.500
	HDMI_TMDS_CLOCK_148P5_150, //148.500 < freq <= 150.000
	HDMI_TMDS_CLOCK_150_185P625, //150.000 < freq <= 185.625
	HDMI_TMDS_CLOCK_185P625_227P75, //185.625 < freq <= 227.750
	HDMI_TMDS_CLOCK_227P75_290, //227.750 < freq <= 290.000
	HDMI_TMDS_CLOCK_297_594, //290.000 < freq <= 594.000
	HDMI20_TMDS_CLOCK_290_340, //290.000 < freq <= 340.000 // 20190816
	HDMI20_TMDS_CLOCK_340_425P5, //340.000 < freq <= 425.500 // 20190816
	HDMI20_TMDS_CLOCK_425P5_510, //425.500 < freq <= 510.000 // 20190816
	HDMI20_TMDS_CLOCK_510_594, //510.000 < freq <= 594.000 // 20190816
	HDMI_FRL_DATA_RATE_3G, //3GB/s
	HDMI_FRL_DATA_RATE_6G, //6GB/s
	HDMI_FRL_DATA_RATE_8G, //8GB/s
	HDMI_FRL_DATA_RATE_10G, //10GB/s
	HDMI_FRL_DATA_RATE_12G, //12GB/s
};

struct HDMI21PHYWriteLatencyRG_t {
	u32 u4Address;
	u32 u4Mask;
	bool fgNotRGWO;
};

enum HDMI2ANA_DELAY_ITEM {
	HDMI2ANA_DELAY_SAOSC,
	HDMI2ANA_DELAY_EQLOCK_HALF,
	HDMI2ANA_DELAY_EQLOCK_FULL,
	HDMI2ANA_DELAY_EQINCREASE,
	HDMI2ANA_DELAY_MAX
};

struct HDMI2ANA_LEQ_ITEM {
	bool fgLeak;
	bool fgAvg;
	bool fgLeak_Increase;
	bool fgLeak_Decrease;
	bool fgLEQ_Reduce;
};

struct HDMI2ANA_CONFIG {
	u16 au2DelayRatio[HDMI2ANA_DELAY_MAX]; //ratio*10%
	struct HDMI2ANA_LEQ_ITEM tLEQItem;
	u8 u1SAOSC_LFSEL;
	u8 u1EQOSC_LFSEL;
	bool fgSupport3GHDMI20;
	bool fgForce40xMode;
};

enum HdmiRxFreq_t {
	HDMI_RX_FREQ_DVI,
	HDMI_RX_FREQ_MON_CK,
	HDMI_RX_FREQ_MON_CK2,
	HDMI_RX_FREQ_DEEPCLRCK
};

enum HdmiRxFreqMeter_t {
	HDMI_RX_FREQ_METER_LP,
	HDMI_RX_FREQ_METER_HP
};

enum HDMI2ANA_CDR_RST_DELAY_ITEM {
	HDMI21ANA_CDRST_DELAY_CDRDONE,
	HDMI21ANA_CDRST_DELAY_100us,
	HDMI21ANA_CDRST_DELAY_2ms
};

enum HdmiRxSettingState_t {
	HDMI21_SETTING_NONE = 0,
	HDMI21_SETTING_STATE00 = 1,
	HDMI21_SETTING_STATE01 = 2,
	HDMI21_SETTING_STATE02 = 3,
	HDMI21_SETTING_STATE03 = 4,
	HDMI21_SETTING_STATE04 = 5,
	HDMI21_SETTING_STATE05 = 6,
	HDMI21_SETTING_STATE05_1 = 7,
	HDMI21_SETTING_STATE06 = 8,
	HDMI21_SETTING_STATE06_1 = 9,
	HDMI21_SETTING_STATE07 = 10,
	HDMI21_SETTING_STATE08 = 11,
	HDMI21_SETTING_STATE08_1 = 12,
	HDMI21_SETTING_STATE08_2 = 13,
	HDMI21_SETTING_STATE09_0 = 14,
	HDMI21_SETTING_STATE09 = 15,
	HDMI21_SETTING_STATE09_1 = 16,
	HDMI21_SETTING_STATE10 = 17,
	HDMI21_SETTING_STATE10_1 = 18,
	HDMI21_SETTING_STATE10_2 = 19,
	HDMI21_SETTING_STATE11 = 20,
	HDMI21_SETTING_STATE11_1 = 21,
	HDMI21_SETTING_STATE11_2 = 22,
	HDMI21_SETTING_STATE12 = 23,
	HDMI21_SETTING_STATE12_1 = 24,
	HDMI21_SETTING_STATE13 = 25,
};

enum HdmiRxCDRMode_t {
	HDMI21_RXCDRMODE_NONE = 0,
	HDMI21_RXCDRMODE_OVERSAMPLE_8X,
	HDMI21_RXCDRMODE_OVERSAMPLE_4X,
	HDMI21_RXCDRMODE_OVERSAMPLE_2X,
	HDMI21_RXCDRMODE_OVERSAMPLE_HALF_RATE,
};

enum HdmiRxPLLource_t {
	HDMI21_HDMIRXPLL_SOURCE_TMDSCK = 0,
	HDMI21_HDMIRXPLL_SOURCE_MDPLL = 1,
};

struct HDMI21PHY_DEBUG_PARA {
	u32 u4CntTarget;
	u16 u2PatMask;
	u16 u2TargetPat;
	u32 *apu4DebugBufWP[8];
	short s2Par1;
	short s2Par2;
	short s2Par1Min;
	short s2Par1Max;
	short s2Par2Min;
	short s2Par2Max;
};

#define u2AgingCnt_time 100 // Verification time=100
#define HDMI21PHY_VERIFICATION_LEQOS_TEST 1

struct HDMI21PHY_CALI_DATA {
	u32 u4TAPN1[u2AgingCnt_time];
	u32 u4TAP0[u2AgingCnt_time];
	u32 u4TAP1[u2AgingCnt_time];
	u32 u4TAP2[u2AgingCnt_time];
	u32 u4TAP3[u2AgingCnt_time];
	u32 u4TAP4[u2AgingCnt_time];
	u32 u4TAP5[u2AgingCnt_time];
	u32 u4LEQ[u2AgingCnt_time];
	u32 u4VGA[u2AgingCnt_time];
	u32 u4XDBG[u2AgingCnt_time];
	u32 u4XDBGD[u2AgingCnt_time];
	u32 u4PIEDGE[u2AgingCnt_time];
	u32 u4PIEDBG[u2AgingCnt_time];
	u32 u4D0L_OS[u2AgingCnt_time];
	u32 u4DLEV1_OS[u2AgingCnt_time];
	u32 u4DLEV0_OS[u2AgingCnt_time];
	u32 u4EDBG_OS[u2AgingCnt_time];
	u32 u4E0_OS[u2AgingCnt_time];
	u32 u4D1H_OS[u2AgingCnt_time];
	u32 u4D1L_OS[u2AgingCnt_time];
	u32 u4D0H_OS[u2AgingCnt_time];
	u32 u4E1_OS[u2AgingCnt_time];
#if HDMI21PHY_VERIFICATION_LEQOS_TEST
	u32 u4LEQ_OS_Pre[u2AgingCnt_time];
#endif
	u32 u4LEQ_OS[u2AgingCnt_time];
	u32 u4HTOTAL[u2AgingCnt_time];
	u32 u4HACTIVE[u2AgingCnt_time];
	u32 u4VTOTAL[u2AgingCnt_time];
	u32 u4VACTIVE[u2AgingCnt_time];
	u32 u4CRCValue[u2AgingCnt_time];
	bool fgCRCFAIL[u2AgingCnt_time];
	bool fgCRCREADY[u2AgingCnt_time];
	u32 u4DCRCValue[u2AgingCnt_time];
	bool fgDCRCFAIL[u2AgingCnt_time];
	bool fgDCRCREADY[u2AgingCnt_time];
};

struct HDMI21PHY_STRESSTEST {
	u32 u4Times;
	u32 u4HTOTAL;
	u32 u4HACTIVE;
	u32 u4VTOTAL;
	u32 u4VACTIVE;
	u32 u4DCRCValue;
	bool fgDCRCFAIL;
	bool fgDCRCREADY;
};

struct HDMI21PHY_CALSTATUS {
	bool fgSigDetCalTimeout;
};

struct HDMI_HDR_EMP_MD_T {
	//hdmi spec,CTA-861-G-Final-HDMI forum.pdf,
	//The HDR Dynamic Metadata Extended inforFrame HEAD
	u8 u1EMPHead;
	u8 u1ByteIndex; //value is 0~DYNAMIC_METADATA_LEN-1,
	u8 u1BitIndex; //value is 1~8
	u16 u2Length; //value is 0~DYNAMIC_METADATA_LEN-1,
	u8 pkt_len;
	u8 au1DyMdPKT[DYNAMIC_METADATA_LEN];
};

struct notify_device {
	const char *name;
	struct device *dev;
	int index;
	int state;
	void *myhdmi;

	ssize_t (*print_name)(struct notify_device *sdev, char *buf);
	ssize_t (*print_state)(struct notify_device *sdev, char *buf);
};

struct aud_notify_device {
	const char *name;
	struct device *dev;
	int index;
	int state;
	int aud_fs;
	int aud_ch;
	int aud_bit;

	ssize_t (*print_name)(struct aud_notify_device *sdev, char *buf);
	ssize_t (*print_state)(struct aud_notify_device *sdev, char *buf);
};

struct MTK_HDMI {
	struct device *dev;
	struct device *phydev;
	struct device *txdev;

	dev_t rx_devno;
	struct cdev rx_cdev;
	struct class *rx_class;

	struct notify_device switch_data;
	struct aud_notify_device switch_aud_data;
	struct class *switch_class;
	struct class *switch_aud_class;
	atomic_t dev_cnt;
	atomic_t aud_dev_cnt;
	enum HDMIRX_NOTIFY_T video_notify;
	enum HDMIRX_NOTIFY_T audio_notify;

	/* callback function */
	struct device *ap_dev;
	int (*callback)(struct device *dev, enum HDMIRX_NOTIFY_T event);

	/* hdmi tx device */
	struct MTK_HDMITX *tx;
	struct MTK_HDMIRX *rx;

	enum hdmi_mode_setting hdmi_mode_setting;

	/* task */
	struct timer_list hdmi_timer;
	struct task_struct *main_task;
	wait_queue_head_t main_wq;
	atomic_t main_event;
	u32 main_cmd;
	u32 main_state;
	bool is_rx_init;
	bool is_wait_finish;
	bool is_rx_task_disable;
	u32 task_count;
	u32 timer_count;
	u32 power_on;
	u32 efuse;
	u32 rterm;
	u8 vid_locked;
	u8 aud_locked;

	/* clk */
	struct clk *hdmiclksel;
	struct clk *hdmiclk208m;
	struct clk *hdcpclksel;
	struct clk *hdcpclk104m;
	struct clk *hdmisel;

	/* gpio */
	u32 gpio_hpd;
	u32 gpio_5v;

	/* power */
	struct regulator *hdmi_v33;
	struct regulator *hdmi_v08;
	struct regulator *reg_vcore;

	/* irq */
	int rx_irq;
	int rx_phy_irq;

	/* regs */
	void __iomem *dig_addr;
	void __iomem *tx_addr;
	void __iomem *phy_addr;
	void __iomem *rx_edid_addr;
	void __iomem *scdc_addr;
	void __iomem *apmixedsys_addr;
	void __iomem *topckgen_addr;
	void __iomem *vdout_addr;
	void __iomem *rgu_addr;

	/* hdmi2 */
	u8 HDMIState;
	u8 HDMIPWR5V;
	u8 UnplugFlag;
	u8 HDMIAvMute;
	u8 HDMIScdtLose;
	u8 HDMIHPDTimerCnt;
	u8 Is422;
	u8 force_40x;
	u32 crc0;
	u32 crc1;
	u32 slt_crc;
	bool scdt;
	u32 scdt_clr_cnt;
	bool vrr_en;
	bool fgCKDTDeteck;

	u8 u1TxEdidReady;
	u8 u1TxEdidReadyOld;
	u8 bHdmiRepeaterMode;
	u8 bAppHdmiRepeaterMode;
	u8 bHDMICurrSwitch;
	u8 bAppHDMICurrSwitch;

	enum HdmiRxMode HdmiMode;
	enum HdmiRxClrSpc HdmiClrSpc;
	enum HdmiRxDepth HdmiDepth;
	enum HdmiRxRange HdmiRange;
	enum HdmiRxScan HdmiScan;
	enum HdmiRxAspect HdmiAspect;
	enum HdmiRxAFD HdmiAFD;
	struct HdmiRxITC HdmiITC;
	struct HdmiRx3DInfo Hdmi3DInfo;
	enum HdmiRxHdcp1Status Hdcp1xState;
	enum HdmiRxHdcp2Status Hdcp2xState;
	struct HdmiRxRes ResInfo;
	struct HdmiRxScdc ScdcInfo;
	struct HdmiInfo AviInfo;
	struct HdmiInfo SpdInfo;
	struct HdmiInfo AudInfo;
	struct HdmiInfo MpegInfo;
	struct HdmiInfo UnrecInfo;
	struct HdmiInfo AcpInfo;
	struct HdmiInfo VsiInfo;
	struct HdmiInfo MetaInfo;
	struct HdmiInfo Isrc0Info;
	struct HdmiInfo Isrc1Info;
	struct HdmiInfo GcpInfo;
	struct HdmiInfo HfVsiInfo;
	struct HdmiInfo Info86;
	struct HdmiInfo Info87;
	struct HdmiInfo DVVSIInfo;
#ifdef CC_SUPPORT_HDR10Plus
	struct HDR10Plus_EMP_T _rHdr10PlusEmpCtx;
	struct HDR10Plus_VSI_T _rHdr10PlusVsiCtx;
	struct HDR10Plus_EMP_T _rHdr10PlusEmpCtxBuff[HDR10P_MAX_METADATA_BUF];
	struct HDR10Plus_VSI_T _rHdr10PlusVsiCtxBuff[HDR10P_MAX_METADATA_BUF];
#endif
	struct HDMI_HDR_EMP_MD_T emp;
	struct HDMI_HDR_EMP_MD_T emp_test;
struct HdmiVRR vrr_emp;
	struct hdr10InfoPkt hdr10info;

	u8 HDMIRangeMode;
	u8 HDMIRange;

	u8 u1CchkCheckCount;
	/* various */
	u8 hdcpinit;
	u32 debugtype;
	u8 portswitch;

	/* phy */
	struct HDMI21PHY_CALSTATUS Hdmi2RxPHY;
	struct HDMI2ANA_CONFIG tAnaConfig;
	bool fgHdmi21PhyFirstInit;
	u8 _bPLLBandSel;
	u8 _bCDRMode;
	u32 _dwHDMIClkKhz;
	bool _fgTopLEQInit;
	bool fgFixLEQ;
	u8 u1LEQSetting;
	u32 u4PreLEQOS;
	u16 u2VOLTAGE[18];
	struct HDMI21PHY_CALI_DATA tCalData;
	u16 CaliCnt;
	bool _fgHDMI40xMode;

	enum RxHdcpState hdcp_state;
	u8 hdcp_mode;
	bool auth_done;
	u8 force_hdcp_mode;
	u8 hdcpstatus;
	u8 AKSVShadow[5];
	u8 BKSVShadow[5];
	u8 AnShadow[8];
	u16 RiShadow;
	u16 RiShadowOld;
	struct RxHDCPRptData rpt_data;
	bool is_stream_mute;
	u32 WaitTxKsvListCount;

	u8 hdcp_version;
	u8 hdcp_cnt;
	bool hdcp_flag;

	u8 ITCFlag;
	u8 ITCContent;

	u32 pdtclk_count;
	u8 scdc_reset;
	u32 reset_wait_time;
	u32 timing_unstable_time;
	u32 wait_data_stable_count;
	u32 timing_stable_count;
	unsigned long reset_timeout_start;
	unsigned long wait_scdt_timeout_start;
	unsigned long timeout_start;
	unsigned long unstb_start;

	u8 my_debug;
	u8 my_debug1;
	u8 my_debug2;

	u32 h_total;
	u32 v_total;
	u32 h_unstb_count;
	u8 is_420_cfg;
	u8 is_dp_info;
	bool is_40x_mode;
	u8 dp_cfg;
	bool is_av_mute;
	bool is_timing_chg;
	u32 stable_count;
	u32 hdcp_stable_count;
	u32 p5v_status;

	/* dvi */
	struct DVI_STATUS dvi_s;
	struct MTK_VIDEO_PARA vid_para;
	bool is_detected;
	u8 dvi_state_old;
	u8 wait_support_timing;

	/* audio */
	u8 aud_enable;
	struct AUDIO_INFO aud_s;
	enum AUDIO_STATE aud_state;
	struct AUDIO_CAPS aud_caps;
	enum AUDIO_FORMAT aud_format;
	u32 acr_lock_timer;
	u32 acr_lock_count;
	u8 acp_type;
	u8 fifo_err_conut;
	u8 audio_on_delay;
	unsigned long wait_audio_stable_start;

	u32 isr_mark;

	/* key */
	u32 hdcp2_key_id_count;
	u8 key1x[FLASH_HDCP_KEY_RESERVE];

	u8 _bHdmiAudioOutputMode;
	u8 _bHdmiRepeaterModeDelayCount;

	/* repeater */
	bool _fgHDMIRxBypassMode;
	struct HDMI_SINK_AV_CAP_T *SinkCap;
	u8 oBLK[128];
	u8 rEBuf[128];
	u8 oEBuf[128];

	u8 sel_edid;
	bool chg_edid;

	u8 CP_VidDataBLK[E_VID_BLK_LEN];
	// add PCM 3,4,5,6,7 channel, each 3 bytes
	/* CP: compose */
	u8 CP_AudDataBLK[E_AUD_BLK_LEN + 15];
	u8 CP_SPKDataBLK[E_SPK_BLK_LEN];
	u8 CP_VSDBDataBLK[E_VEND_FULL_L];
	u8 CP_VSDBData4K2KBLK[E_VEND_BLK_4K2K_FULL_LEN];
	u8 CP_ColorimetryDataBLK[E_COLORMETRY_BLK_LEN];
	u8 CP_VcdbDataBLK[E_VCDB_BLOCK_LEN];
	u8 CP_420cmdbDataBLK[E_420_CMDB_LEN];
	u8 CP_420VicDataBLK[E_420_VDB_LEN];
	u8 CP_HFVSDBDataBLK[E_HF_VEND_BLK_LEN];
	u8 CP_HDRSMDataBLK[E_HDR_SMDB_LEN];
	u8 CP_DoviVision_Data_Block[E_DOVIVISION_DB_LEN];

	u8 u1HDMIIN1EDID[256];
	u8 u1HDMIIN2EDID[256];
	u8 u1HDMIINEDID[512];

	struct PHY_ADDR_PARA_T PHYAdr;
	struct E_PARA_T Edid;

	bool fgVidUseAnd;
	bool AudUseAnd;
	bool _fg2chPcmOnly;
	u8 _u1EDID0CHKSUM;
	u8 _u1EDID1CHKSUM;
	u16 _u2EDID0PA;
	u8 _u1EDID0PAOFF; // PA remap offset
	u16 _u2EDID1PA;
	u8 _u1EDID1PAOFF; // PA remap offset
	u8 _u1EDID2CHKSUM;
	u16 _u2EDID2PA;
	u8 _u1EDID2PAOFF; // PA remap offset

	u8 PHY_ADR_ST[5]; // max 5 VSDB PHY

	u16 _u2Port1Addr;
	u16 _u2Port2Addr;
	u16 _u2Port3Addr;
	bool fgUseMdyPA;
	u16 u2MdyPA;

	u32 EFirst16VIC[16];
	u8 EDID420BitMap[E_420_BIT_MAP_LEN];

	/* edid parser */
	u32 _u4SinkProductID;
	bool _fgHdmiNoEdidCheck;
	bool fgHDMIIsRpt;
	int i4HdmiShareInfo[MAX_HDMI_SHAREINFO];
	u8 _bEdidData[EDID_SIZE];
	u8 _bHdmiTestEdidData[256];
	u8 _u1DbgEdidPcmChFs[7];
	u8 _u1DbgEdidPcmChBitSize[7];
	u8 _u1DbgEdidSpkAllocation;
	u16 _u2DbgEdidAudDec;
#if defined(THREE_D_EDID)
	u32 _u4i_3D_VIC;
#define ADVANCE_THREE_D_EDID
#if defined(ADVANCE_THREE_D_EDID)
	u32 First_16_N_VIC;
	u32 First_16_P_VIC;
	u32 First_16_VIC[16];
#endif
	u32 svd128VIC[128];
	u32 _u4i_svd_420_CMDB;
#endif
	char cDtsStr[7];

#ifdef HDMIRX_OPTEE_EN
	struct tee_context *hdcp_ctx;
	struct tee_ioctl_open_session_arg hdcp_osarg;
#endif

	u8 key_present;

	u32 temp;
};

#define RX_ADR myhdmi->dig_addr
#define TX_ADR myhdmi->tx_addr
#define A_ADR myhdmi->phy_addr
#define E_ADR myhdmi->rx_edid_addr
#define S_ADR myhdmi->scdc_addr
#define AP_ADR myhdmi->apmixedsys_addr
#define CK_ADR myhdmi->topckgen_addr
#define V_ADR myhdmi->vdout_addr
#define R_ADR myhdmi->rgu_addr

#define TXP myhdmi->tx
#define SCP myhdmi->SinkCap

#endif
