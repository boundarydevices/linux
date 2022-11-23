/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MTK_HDMI_RPT_H_
#define _MTK_HDMI_RPT_H_

/** @ingroup type_group_mtk_hdmi_enum
 * @brief timing format ID.
 */
enum { MODE_NOSIGNAL = 0,
	MODE_525I_OVERSAMPLE,
	MODE_625I_OVERSAMPLE,
	MODE_480P_OVERSAMPLE,
	MODE_576P_OVERSAMPLE,
	MODE_720p_50,
	MODE_720p_60,
	MODE_1080i_48,
	MODE_1080i_50,
	MODE_1080i,
	MODE_1080p_24,
	MODE_1080p_25,
	MODE_1080p_30,
	MODE_1080p_50,
	MODE_1080p_60,
	MODE_525I,
	MODE_625I,
	MODE_480P,
	MODE_576P,
	MODE_720p_24,
	MODE_720p_25,
	MODE_720p_30,
	MODE_240P,
	MODE_540P,
	MODE_288P,
	MODE_480P_24,
	MODE_480P_30,
	MODE_576P_25,
	MODE_HDMI_640_480P,
	MODE_HDMI_720p_24,
	MODE_3D_720p_50_FP,
	MODE_3D_720p_60_FP,
	MODE_3D_1080p_24_FP,
	MODE_3D_1080I_60_FP,
	MODE_3D_480p_60_FP,
	MODE_3D_576p_50_FP,
	MODE_3D_720p_24_FP,
	MODE_3D_720p_30_FP,
	MODE_3D_1080p_30_FP,
	MODE_3D_480I_60_FP,
	MODE_3D_576I_60_FP,
	MODE_3D_1080I_50_FP,
	MODE_3D_1080p_50_FP,
	MODE_3D_1080p_60_FP,
	MODE_3D_1650_750_60_FP,
	MODE_3D_1650_1500_30_FP,
	MODE_3D_640_480p_60_FP,
	MODE_3D_1440_240p_60_FP,
	MODE_3D_1440_288p_50_FP,
	MODE_3D_1440_576p_50_FP,
	MODE_3D_720p_25_FP,
	MODE_3D_1080p_25_FP,
	MODE_3D_1080I_1250TOTAL_50_FP,
	MODE_3D_1080p_24_SBS_FULL,
	MODE_3D_1080p_25_SBS_FULL,
	MODE_3D_1080p_30_SBS_FULL,
	MODE_3D_1080I_50_SBS_FULL,
	MODE_3D_1080I_60_SBS_FULL,
	MODE_3D_720p_24_SBS_FULL,
	MODE_3D_720p_30_SBS_FULL,
	MODE_3D_720p_50_SBS_FULL,
	MODE_3D_720p_60_SBS_FULL,
	MODE_3D_480p_60_SBS_FULL,
	MODE_3D_576p_50_SBS_FULL,
	MODE_3D_480I_60_SBS_FULL,
	MODE_3D_576I_50_SBS_FULL,
	MODE_3D_640_480p_60_SBS_FULL,
	MODE_3D_640_480p_60_LA,
	MODE_3D_240p_60_LA,
	MODE_3D_288p_50_LA,
	MODE_3D_480p_60_LA,
	MODE_3D_576p_50_LA,
	MODE_3D_720p_24_LA,
	MODE_3D_720p_60_LA,
	MODE_3D_720p_50_LA,
	MODE_3D_1080p_24_LA,
	MODE_3D_1080p_25_LA,
	MODE_3D_1080p_30_LA,
	MODE_3D_480I_60_FA,
	MODE_3D_576I_50_FA,
	MODE_3D_1080I_60_FA,
	MODE_3D_1080I_50_FA,
	MODE_3D_MASTER_1080I_60_FA,
	MODE_3D_MASTER_1080I_50_FA,
	MODE_3D_480I_60_SBS_HALF,
	MODE_3D_576I_50_SBS_HALF,
	MODE_3D_1080I_60_SBS_HALF,
	MODE_3D_1080I_50_SBS_HALF,
	MODE_1080i_50_VID39,
	MODE_1080P_30_2640H,
	MODE_240P_60_3432H,
	MODE_576i_50_3456H_FP,
	MODE_576P_50_1728H_FP,
	MODE_480P_60_3432H,
	MODE_2576P_60_3456H,
	MODE_3D_1440_480p_60_FP,
	MODE_3D_240p_263LINES,
	MODE_3840_1080P_24,
	MODE_3840_1080P_25,
	MODE_3840_1080P_30,
	MODE_3840_2160P_15,
	MODE_3840_2160P_24,
	MODE_3840_2160P_25,
	MODE_3840_2160P_30,
	MODE_4096_2160P_24,
	MODE_4096_2160P_25,
	MODE_4096_2160P_30,
	MODE_4096_2160P_50,
	MODE_4096_2160P_60,
	MODE_3840_2160P_50,
	MODE_3840_2160P_60,
	MODE_4096_2160P_50_420,
	MODE_4096_2160P_60_420,
	MODE_3840_2160P_50_420,
	MODE_3840_2160P_60_420,

	MODE_MAX,
	MODE_DE_MODE,
	MODE_NODISPLAY,
	MODE_NOSUPPORT,
	MODE_WAIT
};

extern struct drm_display_mode mode_640x480_60hz_4v3;
extern struct drm_display_mode mode_720x480_60hz_4v3;
extern struct drm_display_mode mode_720x576_50hz_16v9;
extern struct drm_display_mode mode_1280x720_50hz_16v9;
extern struct drm_display_mode mode_1280x720_60hz_16v9;
extern struct drm_display_mode mode_1920x1080_60hz_16v9;
extern struct drm_display_mode mode_1920x1080_50hz_16v9;
extern struct drm_display_mode mode_1920x1080_24hz_16v9;
extern struct drm_display_mode mode_1920x1080_25hz_16v9;
extern struct drm_display_mode mode_1920x1080_30hz_16v9;
extern struct drm_display_mode mode_3840x2160_24hz_16v9;
extern struct drm_display_mode mode_3840x2160_25hz_16v9;
extern struct drm_display_mode mode_3840x2160_30hz_16v9;
extern struct drm_display_mode mode_3840x2160_50hz_16v9;
extern struct drm_display_mode mode_3840x2160_60hz_16v9;
extern struct drm_display_mode mode_4096x2160_24hz;
extern struct drm_display_mode mode_4096x2160_25hz;
extern struct drm_display_mode mode_4096x2160_30hz;
extern struct drm_display_mode mode_4096x2160_50hz;
extern struct drm_display_mode mode_4096x2160_60hz;

enum HdmiInfoFrameID_t {
	HDMI_INFO_FRAME_UNKNOWN,
	HDMI_INFO_FRAME_ID_AVI,
	HDMI_INFO_FRAME_ID_SPD,
	HDMI_INFO_FRAME_ID_AUD,
	HDMI_INFO_FRAME_ID_MPEG,
	HDMI_INFO_FRAME_ID_UNREC,
	HDMI_INFO_FRAME_ID_ACP,
	HDMI_INFO_FRAME_ID_VSI,
	HDMI_INFO_FRAME_ID_METADATA,
	HDMI_INFO_FRAME_ID_ISRC0,
	HDMI_INFO_FRAME_ID_ISRC1,
	HDMI_INFO_FRAME_ID_GCP,
	HDMI_INFO_FRAME_ID_HFVSI,
	HDMI_INFO_FRAME_ID_86,
	HDMI_INFO_FRAME_ID_87,
	HDMI_INFO_FRAME_ID_DVVSI,
};

struct HdmiInfo {
	enum HdmiInfoFrameID_t u1InfoID;
	bool fgValid;
	bool fgChanged;
	u8 bCount;
	u8 bLength;
	unsigned long u4LastReceivedTime;
	unsigned long u4timeout;
	u8 u1InfoData[32];
};

struct HdmiVRR {
	bool fgValid;
	u32 cnt;
	u32 EmpData[8];
};

enum packet_type {
	AVI_INFOFRAME = 0,
	AUDIO_INFOFRAME,
	ACP_PACKET,
	ISRC1_PACKET,
	ISRC2_PACKET,
	GAMUT_PACKET,
	VENDOR_INFOFRAME,
	SPD_INFOFRAME,
	MPEG_INFOFRAME,
	GEN_INFOFRAME,
	HDR87_PACKET,
	DVVSI_PACKET,
	HFVSI_PACKET,
	MAX_PACKET,
};

enum MTK_HDMI_CS {
	CS_RGB = 0,
	CS_YUV444,
	CS_YUV422,
	CS_YUV420
};

enum HdmiRxDepth {
	HDMI_RX_BIT_DEPTH_8_BIT = 0,
	HDMI_RX_BIT_DEPTH_10_BIT,
	HDMI_RX_BIT_DEPTH_12_BIT,
	HDMI_RX_BIT_DEPTH_16_BIT
};

struct MTK_VIDEO_PARA {
	enum MTK_HDMI_CS cs;
	enum HdmiRxDepth dp;
	u32 htotal;
	u32 vtotal;
	u32 hactive;
	u32 vactive;
	u32 is_pscan;
	bool hdmi_mode;
	u32 frame_rate;
	u32 pixclk;
	u32 tmdsclk;
	bool is_40x;
	u32 id;
	bool vrr_en;
};

enum hdmi_mode_setting {
	HDMI_SOURCE_MODE = 0,
	HDMI_RPT_PIXEL_MODE,
	HDMI_RPT_TMDS_MODE,
	HDMI_RPT_DGI_MODE,
	HDMI_RPT_DRAM_MODE,
};

struct MTK_HDMITX {
	struct device *dev;
	void (*rpt_mode)(struct device *dev,
		enum hdmi_mode_setting mode);
	bool (*get_edid)(struct device *dev,
		u8 *p,
		u32 num,
		u32 len);
	void (*hdcp_start)(struct device *dev,
		bool hdcp_on);
	u8 (*hdcp_ver)(struct device *dev);
	void (*hdmi_enable)(struct device *dev,
		bool en);
	bool (*is_plugin)(struct device *dev);
	u32 (*get_port)(struct device *dev);
	void (*signal_off)(struct device *dev);
	void (*send_pkt)(struct device *dev,
		enum packet_type type,
		bool enable,
		u8 *pkt_data,
		struct HdmiInfo *pkt_p);
	void (*send_vrr)(struct device *dev,
		struct HdmiVRR *pkt_p);
	bool (*cfg_video)(struct device *dev,
		struct MTK_VIDEO_PARA *vid_para);
	bool (*cfg_audio)(struct device *dev, u8 aud);
};

struct MTK_HDMIRX {
	struct device *dev;
	void (*issue_edid)(struct device *dev,
		u8 u1EdidReady);
	void (*set_ksv)(struct device *dev,
		bool fgIsHdcp2,
		u16 u2TxBStatus,
		u8 *prbTxBksv,
		u8 *prbTxKsvlist,
		bool fgTxVMatch);
	bool (*hdcp_is_rpt)(struct device *dev);
	bool (*hdcp_is_doing_auth)(struct device *dev);
	bool (*hdcp_reauth)(struct device *dev);
};

#endif
