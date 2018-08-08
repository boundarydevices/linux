/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmi_rx_edid.h
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
#ifndef _HDMI_RX_EDID_H_
#define _HDMI_RX_EDID_H_

#define EDID_SIZE			256
#define EDID_HDR_SIZE		7
#define EDID_HDR_HEAD_LEN	4
#define MAX_HDR_LUMI_LEN	3
#define MAX_EDID_BUF_SIZE	512

/* CEA861F Table 44~46 CEA data block tag code*/
/* tag code 0x0: reserved */
#define AUDIO_TAG 0x1
#define VIDEO_TAG 0x2
#define VENDOR_TAG 0x3
#define SPEAKER_TAG 0x4
/* VESA Display Transfer Characteristic Data Block */
#define VESA_TAG 0x5
/* tag code 0x6: reserved */
#define USE_EXTENDED_TAG 0x7

/* extended tag code */
#define VCDB_TAG 0 /* video capability data block */
#define VSVDB_TAG 1 /* Vendor-Specific Video Data Block */
#define VDDDB_TAG 2 /* VESA Display Device Data Block */
#define VVTDB_TAG 3 /* VESA Video Timing Block Extension */
/* extend tag code 0x4: Reserved for HDMI Video Data Block*/
#define CDB_TAG 5 /* Colorimetry Data Block */
#define HDR_TAG 6 /* HDR Static Metadata Data Block */
/* extend tag code 7-12: reserved */
#define VFPDB_TAG 13 /* Video Format Preference Data Block */
#define Y420VDB_TAG 14 /* YCBCR 4:2:0 Video Data Block */
#define Y420CMDB_TAG 15 /* YCBCR 4:2:0 Capability Map Data Block */
/* extend tag code 16: Reserved */
#define VSADB_TAG 17 /* Vendor-Specific Audio Data Block */
/* extend tag code 18~31: Reserved */
#define IFDB_TAG 32 /* infoframe data block */
#define HDMI_VIC420_OFFSET 0x100
#define HDMI_3D_OFFSET 0x180
#define HDMI_VESA_OFFSET 0x200


enum edid_audio_format_e {
	AUDIO_FORMAT_HEADER,
	AUDIO_FORMAT_LPCM,
	AUDIO_FORMAT_AC3,
	AUDIO_FORMAT_MPEG1,
	AUDIO_FORMAT_MP3,
	AUDIO_FORMAT_MPEG2,
	AUDIO_FORMAT_AAC,
	AUDIO_FORMAT_DTS,
	AUDIO_FORMAT_ATRAC,
	AUDIO_FORMAT_OBA,
	AUDIO_FORMAT_DDP,
	AUDIO_FORMAT_DTSHD,
	AUDIO_FORMAT_MAT,
	AUDIO_FORMAT_DST,
	AUDIO_FORMAT_WMAPRO,
};

enum edid_tag_e {
	EDID_TAG_NONE,
	EDID_TAG_AUDIO = 1,
	EDID_TAG_VIDEO = 2,
	EDID_TAG_VENDOR = 3,
	EDID_TAG_HDR = ((0x7<<8)|6),
};

struct edid_audio_block_t {
	unsigned char max_channel:3;
	unsigned char format_code:4;
	unsigned char fmt_code_resvrd:1;

	unsigned char freq_32khz:1;
	unsigned char freq_44_1khz:1;
	unsigned char freq_48khz:1;
	unsigned char freq_88_2khz:1;
	unsigned char freq_96khz:1;
	unsigned char freq_176_4khz:1;
	unsigned char freq_192khz:1;
	unsigned char freq_reserv:1;
	union bit_rate_u {
		struct pcm_t {
			unsigned char size_16bit:1;
			unsigned char size_20bit:1;
			unsigned char size_24bit:1;
			unsigned char size_reserv:5;
		} pcm;
		unsigned char others;
	} bit_rate;
};

struct edid_hdr_block_t {
	unsigned char ext_tag_code;
	unsigned char sdr:1;
	unsigned char hdr:1;
	unsigned char smtpe_2048:1;
	unsigned char future:5;
	unsigned char meta_des_type1:1;
	unsigned char reserv:7;
	unsigned char max_lumi;
	unsigned char avg_lumi;
	unsigned char min_lumi;
};

enum edid_list_e {
	EDID_LIST_TOP,
	EDID_LIST_14,
	EDID_LIST_14_AUD,
	EDID_LIST_14_420C,
	EDID_LIST_14_420VD,
	EDID_LIST_20,
	EDID_LIST_NUM,
	EDID_LIST_NULL
};

struct detailed_timing_desc {
	unsigned int pixel_clk;
	unsigned int hactive;
	unsigned int vactive;
};
struct video_db_s {
	/* video data block: 31 bytes long maximum*/
	unsigned char svd_num;
	unsigned char hdmi_vic[31];
};
/* audio data block*/
struct audio_db_s {
	/* support for below audio format:
	 * 14 audio fmt + 1 header = 15
	 * uncompressed audio:lpcm
	 * compressed audio: others
	 */
	unsigned char aud_fmt_sup[15];
	/* short audio descriptor */
	struct edid_audio_block_t sad[15];
};

/* speaker allocation data block: 3 bytes*/
struct speaker_alloc_db_s {
	unsigned char flw_frw:1;
	unsigned char rlc_rrc:1;
	unsigned char flc_frc:1;
	unsigned char rc:1;
	unsigned char rl_rr:1;
	unsigned char fc:1;
	unsigned char lfe:1;
	unsigned char fl_fr:1;
	unsigned char resvd1:5;
	unsigned char fch:1;
	unsigned char tc:1;
	unsigned char flh_frh:1;
	unsigned char resvd2;
};

struct specific_vic_3d {
	unsigned char _2d_vic_order:4;
	unsigned char _3d_struct:4;
	unsigned char _3d_detail:4;
	unsigned char resvrd:4;
};

struct vsdb_s {
	unsigned int ieee_oui;
	/* phy addr 2 bytes */
	unsigned char a:4;
	unsigned char b:4;
	unsigned char c:4;
	unsigned char d:4;

	/* support_AI: Set to 1 if the Sink supports at least
	 * one function that uses information carried by the
	 * ACP, ISRC1, or ISRC2 packets.
	 */
	unsigned char support_AI:1;
	/* the three DC_XXbit bits above only indicate support
	 * for RGB4:4:4 at that pixel size.Support for YCbCb4:4:4
	 * in Deep Color modes is indicated with the DC_Y444 bit.
	 * If DC_Y444 is set, then YCbCr4:4:4 is supported for
	 * all modes indicated by the DC_XXbit flags.
	 */
	unsigned char DC_48bit:1;
	unsigned char DC_36bit:1;
	unsigned char DC_30bit:1;
	/* DC_Y444: Set if supports YCbCb4:4:4 Deep Color modes */
	unsigned char DC_y444:1;
	unsigned char resvd1:2;
	/* Set if Sink supports DVI dual-link operation */
	unsigned char dvi_dual:1;

	unsigned char max_tmds_clk;

	unsigned char latency_fields_present:1;
	unsigned char i_latency_fields_present:1;
	unsigned char hdmi_video_present:1;
	unsigned char resvd2:1;
	/* each bit indicates support for particular Content Type */
	unsigned char cnc3:1;/* game */
	unsigned char cnc2:1;/* cinema */
	unsigned char cnc1:1;/* photo*/
	unsigned char cnc0:1;/* graphics */

	unsigned char video_latency;
	unsigned char audio_latency;
	unsigned char interlaced_video_latency;
	unsigned char interlaced_audio_latency;

	unsigned char _3d_present:1;
	unsigned char _3d_multi_present:2;
	unsigned char image_size:2;
	unsigned char resvd3:3;

	unsigned char hdmi_vic_len:3;
	unsigned char hdmi_3d_len:5;

	unsigned char hdmi_4k2k_30hz_sup;
	unsigned char hdmi_4k2k_25hz_sup;
	unsigned char hdmi_4k2k_24hz_sup;
	unsigned char hdmi_smpte_sup;

	/*3D*/
	uint16_t _3d_struct_all;
	uint16_t _3d_mask_15_0;
	struct specific_vic_3d _2d_vic[16];
	unsigned char _2d_vic_num;
};

struct hf_vsdb_s {
	unsigned int ieee_oui;
	unsigned char version;
	unsigned char max_tmds_rate;

	unsigned char scdc_present:1;
	/* if set, the sink is capable of initiating an SCDC read request */
	unsigned char rr_cap:1;
	unsigned char resvd1:2;
	unsigned char lte_340m_scramble:1;
	unsigned char independ_view:1;
	unsigned char dual_view:1;
	unsigned char _3d_osd_disparity:1;

	unsigned char resvrd2:5;
	unsigned char dc_48bit_420:1;
	unsigned char dc_36bit_420:1;
	unsigned char dc_30bit_420:1;
};

struct colorimetry_db_s {
	unsigned char BT2020_RGB:1;
	unsigned char BT2020_YCC:1;
	unsigned char BT2020_cYCC:1;
	unsigned char Adobe_RGB:1;
	unsigned char Adobe_YCC601:1;
	unsigned char sYCC601:1;
	unsigned char xvYCC709:1;
	unsigned char xvYCC601:1;

	unsigned char resvd:4;
	/* MDX: designated for future gamut-related metadata. As yet undefined,
	 * this metadata is carried in an interface-specific way.
	 */
	unsigned char MD3:1;
	unsigned char MD2:1;
	unsigned char MD1:1;
	unsigned char MD0:1;
};

struct hdr_db_s {
	/* CEA861.3 table7-9 */
	/* ET_4 to ET_5 shall be set to 0. Future
	 * Specifications may define other EOTFs
	 */
	unsigned char resvd1: 4;
	unsigned char eotf_hlg:1;
	/* SMPTE ST 2084[2] */
	unsigned char eotf_smpte_st_2084:1;
	/* Traditional gamma - HDR Luminance Range */
	unsigned char eotf_hdr:1;
	/* Traditional gamma - SDR Luminance Range */
	unsigned char eotf_sdr:1;

	/* Static Metadata Descriptors:
	 * SM_1 to SM_7  Reserved for future use
	 */
	unsigned char hdr_SMD_other_type:7;
	unsigned char hdr_SMD_type1:1;

	unsigned char hdr_lum_max;
	unsigned char hdr_lum_avg;
	unsigned char hdr_lum_min;
};

struct video_cap_db_s {
	/* quantization range:
	 * 0: no data
	 * 1: selectable via AVI YQ/Q
	 */
	unsigned char quanti_range_ycc:1;
	unsigned char quanti_range_rgb:1;
	/* scan behaviour */
	/* s_PT
	 * 0: No Data (refer to S_CE or S_IT fields)
	 * 1: always overscanned
	 * 2: always underscanned
	 * 3: support both
	 */
	unsigned char s_PT:2;
	/* s_IT
	 * 0: IT Video Formats not supported
	 * 1: always overscanned
	 * 2: always underscanned
	 * 3: support both
	 */
	unsigned char s_IT:2;
	unsigned char s_CE:2;
};

struct dv_vsvdb_s {
	unsigned int ieee_oui;
	unsigned char version:3;
	unsigned char DM_version:3;
	unsigned char sup_2160p60hz:1;
	unsigned char sup_yuv422_12bit:1;

	unsigned char target_max_lum:7;
	unsigned char sup_global_dimming:1;

	unsigned char target_min_lum:7;
	unsigned char colormetry:1;

	unsigned char resvrd;
	uint16_t Rx;
	uint16_t Ry;
	uint16_t Gx;
	uint16_t Gy;
	uint16_t Bx;
	uint16_t By;
	uint16_t Wx;
	uint16_t Wy;
	uint16_t tminPQ;
	uint16_t tmaxPQ;
	uint8_t dm_major_ver;
	uint8_t dm_minor_ver;
};

struct edid_info_s {
	/* 8 */
	unsigned char manufacturer_name[3];
	/* 10 */
	unsigned int product_code;
	/* 12 */
	unsigned int serial_number;
	/* 16 */
	unsigned char product_week;
	unsigned int product_year;
	unsigned char edid_version;
	unsigned char edid_revision;
	/* 54 + 18 * x */
	struct detailed_timing_desc descriptor1;
	struct detailed_timing_desc descriptor2;
	/* 54 + 18 * x */
	unsigned char monitor_name[13];
	unsigned int max_sup_pixel_clk;
	uint8_t extension_flag;
	/* 127 */
	unsigned char block0_chk_sum;

	/* CEA extension */
	/* CEA header */
	unsigned char cea_tag;
	unsigned char cea_revision;
	unsigned char dtd_offset;
	unsigned char underscan_sup:1;
	unsigned char basic_aud_sup:1;
	unsigned char ycc444_sup:1;
	unsigned char ycc422_sup:1;
	unsigned char native_dtd_num:4;
	/* audio data block */
	struct video_db_s video_db;
	/* audio data block */
	struct audio_db_s audio_db;
	/* speaker allocation data block */
	struct speaker_alloc_db_s speaker_alloc;
	/* vendor specific data block */
	struct vsdb_s vsdb;
	/* hdmi forum vendor specific data block */
	bool contain_hf_vsdb;
	struct hf_vsdb_s hf_vsdb;
	/* video capability data block */
	bool contain_vcdb;
	struct video_cap_db_s vcdb;
	/* vendor specific video data block - dolby vision */
	bool contain_vsvdb;
	struct dv_vsvdb_s dv_vsvdb;
	/* colorimetry data block */
	bool contain_cdb;
	struct colorimetry_db_s color_db;
	/* HDR Static Metadata Data Block */
	bool contain_hdr_db;
	struct hdr_db_s hdr_db;
	/* Y420 video data block: 6 SVD maximum:
	 * y420vdb support only 4K50/60hz, smpte50/60hz
	 * 4K50/60hz format aspect ratio: 16:9, 64:27
	 * smpte50/60hz 256:135
	 */
	bool contain_y420_vdb;
	unsigned char y420_vic_len;
	unsigned char y420_vdb_vic[6];
	/* Y420 Capability Map Data Block: 31 SVD maxmium */
	bool contain_y420_cmdb;
	unsigned char y420_all_vic;
	unsigned char y420_cmdb_vic[31];
};

struct edid_data_s {
	unsigned char edid[256];
	unsigned int physical[4];
	unsigned char phy_offset;
	unsigned int checksum;
};

enum hdmi_vic_e {
	/* Refer to CEA 861-D */
	HDMI_UNKNOWN = 0,
	HDMI_640x480p60 = 1,
	/* for video format which have two different
	 * aspect ratios, VICs list below that don't
	 * indicate aspect ratio, its aspect ratio
	 * is default. e.g:
	 * HDMI_480p60, means 480p60_4x3
	 * HDMI_720p60, means 720p60_16x9
	 * HDMI_1080p50, means 1080p50_16x9
	 */
	HDMI_480p60 = 2,
	HDMI_480p60_16x9 = 3,
	HDMI_720p60 = 4,
	HDMI_1080i60 = 5,
	HDMI_480i60 = 6,
	HDMI_480i60_16x9 = 7,
	HDMI_1440x240p60 = 8,
	HDMI_1440x240p60_16x9 = 9,
	HDMI_2880x480i60 = 10,
	HDMI_2880x480i60_16x9 = 11,
	HDMI_2880x240p60 = 12,
	HDMI_2880x240p60_16x9 = 13,
	HDMI_1440x480p60 = 14,
	HDMI_1440x480p60_16x9 = 15,
	HDMI_1080p60 = 16,
	HDMI_576p50 = 17,
	HDMI_576p50_16x9 = 18,
	HDMI_720p50 = 19,
	HDMI_1080i50 = 20,
	HDMI_576i50 = 21,
	HDMI_576i50_16x9 = 22,
	HDMI_1440x288p50 = 23,
	HDMI_1440x288p50_16x9 = 24,
	HDMI_2880x576i50 = 25,
	HDMI_2880x576i50_16x9 = 26,
	HDMI_2880x288p50 = 27,
	HDMI_2880x288p50_16x9 = 28,
	HDMI_1440x576p50 = 29,
	HDMI_1440x576p50_16x9 = 30,
	HDMI_1080p50 = 31,
	HDMI_1080p24 = 32,
	HDMI_1080p25 = 33,
	HDMI_1080p30 = 34,
	HDMI_2880x480p60 = 35,
	HDMI_2880x480p60_16x9 = 36,
	HDMI_2880x576p50 = 37,
	HDMI_2880x576p50_16x9 = 38,
	HDMI_1080i50_1250 = 39,
	HDMI_1080i100 = 40,
	HDMI_720p100 = 41,
	HDMI_576p100 = 42,
	HDMI_576p100_16x9 = 43,
	HDMI_576i100 = 44,
	HDMI_576i100_16x9 = 45,
	HDMI_1080i120 = 46,
	HDMI_720p120 = 47,
	HDMI_480p120 = 48,
	HDMI_480p120_16x9 = 49,
	HDMI_480i120 = 50,
	HDMI_480i120_16x9 = 51,
	HDMI_576p200 = 52,
	HDMI_576p200_16x9 = 53,
	HDMI_576i200 = 54,
	HDMI_576i200_16x9 = 55,
	HDMI_480p240 = 56,
	HDMI_480p240_16x9 = 57,
	HDMI_480i240 = 58,
	HDMI_480i240_16x9 = 59,
	/* Refet to CEA 861-F */
	HDMI_720p24 = 60,
	HDMI_720p25 = 61,
	HDMI_720p30 = 62,
	HDMI_1080p120 = 63,
	HDMI_1080p100 = 64,
	HDMI_720p24_64x27 = 65,
	HDMI_720p25_64x27 = 66,
	HDMI_720p30_64x27 = 67,
	HDMI_720p50_64x27 = 68,
	HDMI_720p60_64x27 = 69,
	HDMI_720p100_64x27 = 70,
	HDMI_720p120_64x27 = 71,
	HDMI_1080p24_64x27 = 72,
	HDMI_1080p25_64x27 = 73,
	HDMI_1080p30_64x27 = 74,
	HDMI_1080p50_64x27 = 75,
	HDMI_1080p60_64x27 = 76,
	HDMI_1080p100_64x27 = 77,
	HDMI_1080p120_64x27 = 78,
	HDMI_1680x720p24_64x27 = 79,
	HDMI_1680x720p25_64x27 = 80,
	HDMI_1680x720p30_64x27 = 81,
	HDMI_1680x720p50_64x27 = 82,
	HDMI_1680x720p60_64x27 = 83,
	HDMI_1680x720p100_64x27 = 84,
	HDMI_1680x720p120_64x27 = 85,
	HDMI_2560x1080p24_64x27 = 86,
	HDMI_2560x1080p25_64x27 = 87,
	HDMI_2560x1080p30_64x27 = 88,
	HDMI_2560x1080p50_64x27 = 89,
	HDMI_2560x1080p60_64x27 = 90,
	HDMI_2560x1080p100_64x27 = 91,
	HDMI_2560x1080p120_64x27 = 92,
	/* 3840*2160 */
	HDMI_2160p24_16x9 = 93,
	HDMI_2160p25_16x9 = 94,
	HDMI_2160p30_16x9 = 95,
	HDMI_2160p50_16x9 = 96,
	HDMI_2160p60_16x9 = 97,
	/* 4096*2160 */
	HDMI_4096p24_256x135 = 98,
	HDMI_4096p25_256x135 = 99,
	HDMI_4096p30_256x135 = 100,
	HDMI_4096p50_256x135 = 101,
	HDMI_4096p60_256x135 = 102,
	/* 3840*2160 */
	HDMI_2160p24_64x27 = 103,
	HDMI_2160p25_64x27 = 104,
	HDMI_2160p30_64x27 = 105,
	HDMI_2160p50_64x27 = 106,
	HDMI_2160p60_64x27 = 107,
	HDMI_RESERVED = 108,
	/* VIC 108~255: Reserved for the Future */

	/* the following VICs are for y420 mode,
	 * they are fake VICs that used to diff
	 * from non-y420 mode, and have same VIC
	 * with normal VIC in the lower bytes.
	 */
	HDMI_VIC_Y420 = HDMI_VIC420_OFFSET,
	HDMI_2160p50_16x9_Y420	=
		HDMI_VIC420_OFFSET + HDMI_2160p50_16x9,
	HDMI_2160p60_16x9_Y420	=
		HDMI_VIC420_OFFSET + HDMI_2160p60_16x9,
	HDMI_4096p50_256x135_Y420 =
		HDMI_VIC420_OFFSET + HDMI_4096p50_256x135,
	HDMI_4096p60_256x135_Y420 =
		HDMI_VIC420_OFFSET + HDMI_4096p60_256x135,
	HDMI_2160p50_64x27_Y420 =
		HDMI_VIC420_OFFSET + HDMI_2160p50_64x27,
	HDMI_2160p60_64x27_Y420 =
		HDMI_VIC420_OFFSET + HDMI_2160p60_64x27,
	HDMI_1080p_420,
	HDMI_VIC_Y420_MAX,

	HDMI_VIC_3D = HDMI_3D_OFFSET,
	HDMI_480p_FRAMEPACKING,
	HDMI_576p_FRAMEPACKING,
	HDMI_720p_FRAMEPACKING,
	HDMI_1080i_ALTERNATIVE,
	HDMI_1080i_FRAMEPACKING,
	HDMI_1080p_ALTERNATIVE,
	HDMI_1080p_FRAMEPACKING,

	HDMI_800_600 = HDMI_VESA_OFFSET,
	HDMI_1024_768,
	HDMI_720_400,
	HDMI_1280_768,
	HDMI_1280_800,
	HDMI_1280_960,
	HDMI_1280_1024,
	HDMI_1360_768,
	HDMI_1366_768,
	HDMI_1600_900,
	HDMI_1600_1200,
	HDMI_1920_1200,
	HDMI_1440_900,
	HDMI_1400_1050,
	HDMI_1680_1050,
	HDMI_1152_864,
	HDMI_2560_1440,
	HDMI_UNSUPPORT,
};

extern int edid_mode;
extern int port_map;
extern bool new_hdr_lum;
extern bool atmos_edid_update_hpd_en;
int rx_set_hdr_lumi(unsigned char *data, int len);
void rx_edid_physical_addr(int a, int b, int c, int d);
unsigned char rx_parse_arc_aud_type(const unsigned char *buff);
extern unsigned int hdmi_rx_top_edid_update(void);
unsigned char rx_get_edid_index(void);
unsigned char *rx_get_edid(int edid_index);
void edid_parse_block0(uint8_t *p_edid, struct edid_info_s *edid_info);
void edid_parse_cea_block(uint8_t *p_edid, struct edid_info_s *edid_info);
void rx_edid_parse_print(struct edid_info_s *edid_info);
void rx_modify_edid(unsigned char *buffer,
				int len, unsigned char *addition);
void rx_edid_update_audio_info(unsigned char *p_edid,
						unsigned int len);
extern bool is_ddc_idle(unsigned char port_id);
#endif
