/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _HDMIRX_H_
#define  _HDMIRX_H_

#define HDMIRX_DEVNAME "hdmirx"

#define HDMIRX_DRV_VER_MAJOR 1
#define HDMIRX_DRV_VER_MINOR 0
#define HDMIRX_DRV_VER ((HDMIRX_DRV_VER_MAJOR << 8) | HDMIRX_DRV_VER_MINOR)

#define VENDORSPEC_INFOFRAME_TYPE 0x01
#define AVI_INFOFRAME_TYPE 0x02
#define SPD_INFOFRAME_TYPE 0x03
#define AUDIO_INFOFRAME_TYPE 0x04
#define MPEG_INFOFRAME_TYPE 0x05

#define VENDORSPEC_INFOFRAME_VER 0x01
#define AVI_INFOFRAME_VER 0x02
#define SPD_INFOFRAME_VER 0x01
#define AUDIO_INFOFRAME_VER 0x01
#define MPEG_INFOFRAME_VER 0x01

#define VENDORSPEC_INFOFRAME_LEN 8
#define AVI_INFOFRAME_LEN 13
#define SPD_INFOFRAME_LEN 25
#define AUDIO_INFOFRAME_LEN 10
#define MPEG_INFOFRAME_LEN 10

#define ACP_PKT_LEN 9
#define ISRC1_PKT_LEN 16
#define ISRC2_PKT_LEN 16

/* SampleFreq */
#define HDMI2_AUD_FS_44K (0x0)
#define HDMI2_AUD_FS_88K (0x8)
#define HDMI2_AUD_FS_176K (0xc)
#define HDMI2_AUD_FS_48K (0x2)
#define HDMI2_AUD_FS_96K (0xa)
#define HDMI2_AUD_FS_192K (0xe)
#define HDMI2_AUD_FS_32K (0x3)
#define HDMI2_AUD_FS_768K (0x9)
#define HDMI2_AUD_FS_384K (0x5)
#define HDMI2_AUD_FS_1536K (0x15)
#define HDMI2_AUD_FS_1024K (0x35)
#define HDMI2_AUD_FS_3528K (0xd)
#define HDMI2_AUD_FS_7056K (0x2d)
#define HDMI2_AUD_FS_14112K (0x1d)
#define HDMI2_AUD_FS_64K (0xb)
#define HDMI2_AUD_FS_128K (0x2b)
#define HDMI2_AUD_FS_256K (0x1b)
#define HDMI2_AUD_FS_512K (0x3b)
#define HDMI2_AUD_FS_UNKNOWN (0x1)

/* color space */
enum HDMIRX_CS {
	HDMI_CS_RGB = 0,	/* RGB */
	HDMI_CS_YUV444,		/* YUV444 */
	HDMI_CS_YUV422,		/* YUV422 */
	HDMI_CS_YUV420		/* YUV420 */
};

/* deep color */
enum HdmiRxDP {
	HDMIRX_BIT_DEPTH_8_BIT = 0,	/* 24bits */
	HDMIRX_BIT_DEPTH_10_BIT,	/* 30bits */
	HDMIRX_BIT_DEPTH_12_BIT,	/* 36bits */
	HDMIRX_BIT_DEPTH_16_BIT		/* 48bits */
};

struct HDMIRX_DEV_INFO {
	u8 hdmirx5v;		/* HDMI source 5v level, 1: 5v high; 0: 5v low */
	bool hpd;		/* hpd status, true: HPD high; false: HPD low */
	u32 power_on;		/* hdmirx power on, 1: power on; 0: power off */
	u8 vid_locked;		/* 1: video is stable; 0: video is unstable */
	u8 aud_locked;		/* 1: audio is stable; 0: audio is unstable */
	u8 hdcp_version;	/* HDMI source HDCP version, 22: hdcp2.2, others: HDCP1.4 */
};

struct HDMIRX_VID_PARA {
	enum HDMIRX_CS cs;	/* color space */
	enum HdmiRxDP dp;	/* deep color */
	u32 htotal;		/* total pixels number of one line */
	u32 vtotal;		/* total lines number of one frame */
	u32 hactive;		/* video active pixels number of one line */
	u32 vactive;		/* video active lines number of one frame */
	u32 is_pscan;		/* 1: pscan; 0: interlace */
	bool hdmi_mode;		/* true: HDMI mode; false: DVI mode */
	u32 frame_rate;		/* frame rate */
	u32 pixclk;		/* pixel clock,  the unit is kHZ*/
};

struct hdr10InfoPkt {
	u8 type;		/* infoframe type */
	bool fgValid;		/* true: the packet is valid; false:  the packet is invalid*/
	u8 u1InfoData[32];	/* packet data, refer to HDMI SPEC */
};

/* audio infoframe packet, refer to CTA-861 */
union Audio_InfoFrame {
	struct {
		u8 Type;
		u8 Ver;
		u8 Len;

		u8 AudioChannelCount : 3;
		u8 RSVD1 : 1;
		u8 AudioCodingType : 4;

		u8 SampleSize : 2;
		u8 SampleFreq : 3;
		u8 Rsvd2 : 3;

		u8 FmtCoding;

		u8 SpeakerPlacement;

		u8 Rsvd3 : 3;
		u8 LevelShiftValue : 4;
		u8 DM_INH : 1;
	} info;
	struct {
		u8 AUD_HB[3];
		u8 AUD_DB[AUDIO_INFOFRAME_LEN];
	} pktbyte;
};

/* audio channel status, refer to IEC-60958 */
struct RX_REG_AUDIO_CHSTS {
	u8 rev : 1;
	u8 IsLPCM : 1;
	u8 CopyRight : 1;
	u8 AdditionFormatInfo : 3;
	u8 ChannelStatusMode : 2;
	u8 CategoryCode;
	u8 SourceNumber : 4;
	u8 ChannelNumber : 4;
	u8 SamplingFreq : 4;
	u8 ClockAccuary : 2;
	u8 rev2 : 2;
	u8 WordLen : 4;
	u8 OriginalSamplingFreq : 4;
};

struct IO_AUDIO_CAPS {
	u8 SampleFreq;			/* SampleFreq */
	union Audio_InfoFrame AudInf;	/* refer to CTA-861 */
	u8 CHStatusData[5];		/* audio channel status data, refer to IEC-60958 */
	struct RX_REG_AUDIO_CHSTS AudChStat;	/* audio channel status, refer to IEC-60958 */
};

struct IO_AUDIO_INFO {
	bool is_HBRAudio;	/* audio is HBR */
	bool is_DSDAudio;	/* audio is DSD */
	bool is_RawSDAudio;	/* audio is raw data */
	bool is_PCMMultiCh;	/* audio is PCM */
};

struct HDMIRX_AUD_INFO {
	struct IO_AUDIO_CAPS caps;
	struct IO_AUDIO_INFO info;
};

/* notify event */
enum HDMIRX_NOTIFY_T {
	HDMI_RX_PWR_5V_CHANGE = 0,	/* HDMI 5v voltage level is changed */
	HDMI_RX_TIMING_LOCK,		/* locked stable video */
	HDMI_RX_TIMING_UNLOCK,		/* unlocked stable video */
	HDMI_RX_AUD_LOCK,		/* locked stable audio */
	HDMI_RX_AUD_UNLOCK,		/* unlocked stable audio */
	HDMI_RX_ACP_PKT_CHANGE,		/* ACP packet is changed */
	HDMI_RX_AVI_INFO_CHANGE,	/* AVI infoframe is changed */
	HDMI_RX_AUD_INFO_CHANGE,	/* audio infoframe is changed */
	HDMI_RX_HDR_INFO_CHANGE,	/* HDR10 infoframe is changed */
	HDMI_RX_EDID_CHANGE,		/* update EDID */
	HDMI_RX_HDCP_VERSION,		/* HDMI source HDCP version is changed */
	HDMI_RX_PLUG_IN,		/* HDMI cable plug in */
	HDMI_RX_PLUG_OUT,		/* HDMI cable plug out */
};

/* HDMI input port */
enum HDMI_SWITCH_NO {
	HDMI_SWITCH_INIT = 0,	/* close HDMI input */
	HDMI_SWITCH_1,		/* open HDMI input port */
	HDMI_SWITCH_2,		/* invalid port */
	HDMI_SWITCH_3,		/* invalid port */
	HDMI_SWITCH_4		/* invalid port */
};

#define HDMI_IOW(num, dtype)		_IOW('H', num, dtype)
#define HDMI_IOR(num, dtype)		_IOR('H', num, dtype)
#define HDMI_IOWR(num, dtype)		_IOWR('H', num, dtype)
#define HDMI_IO(num)			_IO('H', num)

/* get HDMIRX video information */
#define MTK_HDMIRX_VID_INFO		HDMI_IOWR(1, struct HDMIRX_VID_PARA)
/* get HDMIRX audio information */
#define MTK_HDMIRX_AUD_INFO		HDMI_IOWR(2, struct HDMIRX_AUD_INFO)
/*
 * enable HDMIRX function, default is enable.
 * 1: enable
 * 0: disable
 */
#define MTK_HDMIRX_ENABLE		HDMI_IOW(3, unsigned int)
/* get HDMIRX device information */
#define MTK_HDMIRX_DEV_INFO		HDMI_IOWR(4, struct HDMIRX_DEV_INFO)
/*
 * set HDMI input port, default is HDMI_SWITCH_1.
 * HDMI_SWITCH_INIT: close.
 * HDMI_SWITCH_1: open HDMI input port.
 */
#define MTK_HDMIRX_SWITCH		HDMI_IOW(5, unsigned int)
/* get HDR10 packet */
#define MTK_HDMIRX_PKT			HDMI_IOWR(6, struct hdr10InfoPkt)
#define MTK_HDMIRX_DRV_VER		HDMI_IOWR(7, unsigned int)

#define CP_MTK_HDMIRX_VID_INFO		HDMI_IOWR(1, struct HDMIRX_VID_PARA)
#define CP_MTK_HDMIRX_AUD_INFO		HDMI_IOWR(2, struct HDMIRX_AUD_INFO)
#define CP_MTK_HDMIRX_ENABLE		HDMI_IOW(3, unsigned int)
#define CP_MTK_HDMIRX_DEV_INFO		HDMI_IOWR(4, struct HDMIRX_DEV_INFO)
#define CP_MTK_HDMIRX_SWITCH		HDMI_IOW(5, unsigned int)
#define CP_MTK_HDMIRX_PKT		HDMI_IOWR(6, struct hdr10InfoPkt)
#define CP_MTK_HDMIRX_DRV_VER		HDMI_IOWR(7, unsigned int)

#endif
