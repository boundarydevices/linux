/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmi_rx_pktinfo.h
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
#ifndef __HDMIRX_PKT_INFO_H__
#define __HDMIRX_PKT_INFO_H__

#include <linux/workqueue.h>

#include <linux/amlogic/cpu_version.h>


#define K_ONEPKT_BUFF_SIZE		8
#define K_PKT_REREAD_SIZE		2

#define K_FLAG_TAB_END			0xa0a05f5f


enum dolbyvision_lenGth_e {
	E_DV_LENGTH_4 = 0x04,
	E_DV_LENGTH_5 = 0x05,
	E_DV_LENGTH_24 = 0x18,
	E_DV_LENGTH_27 = 0x1B
};

enum pkt_decode_type {
	PKT_BUFF_SET_FIFO = 0x01,
	PKT_BUFF_SET_GMD = 0x02,
	PKT_BUFF_SET_AIF = 0x04,
	PKT_BUFF_SET_AVI = 0x08,
	PKT_BUFF_SET_ACR = 0x10,
	PKT_BUFF_SET_GCP = 0x20,
	PKT_BUFF_SET_VSI = 0x40,
	PKT_BUFF_SET_AMP = 0x80,
	PKT_BUFF_SET_DRM   = 0x100,
	PKT_BUFF_SET_NVBI = 0x200,
	PKT_BUFF_SET_EMP = 0x400,

	PKT_BUFF_SET_UNKNOWN = 0xffff,
};

/* data island packet type define */
enum pkt_type_e {
	PKT_TYPE_NULL = 0x0,
	PKT_TYPE_ACR = 0x1,
	/*PKT_TYPE_AUD_SAMPLE = 0x2,*/
	PKT_TYPE_GCP = 0x3,
	PKT_TYPE_ACP = 0x4,
	PKT_TYPE_ISRC1 = 0x5,
	PKT_TYPE_ISRC2 = 0x6,
	/*PKT_TYPE_1BIT_AUD = 0x7,*/
	/*PKT_TYPE_DST_AUD = 0x8,*/
	/*PKT_TYPE_HBIT_AUD = 0x9,*/
	PKT_TYPE_GAMUT_META = 0xa,
	/*PKT_TYPE_3DAUD = 0xb,*/
	/*PKT_TYPE_1BIT3D_AUD = 0xc,*/
	PKT_TYPE_AUD_META = 0xd,
	/*PKT_TYPE_MUL_AUD = 0xe,*/
	/*PKT_TYPE_1BITMUL_AUD = 0xf,*/

	PKT_TYPE_INFOFRAME_VSI = 0x81,
	PKT_TYPE_INFOFRAME_AVI = 0x82,
	PKT_TYPE_INFOFRAME_SPD = 0x83,
	PKT_TYPE_INFOFRAME_AUD = 0x84,
	PKT_TYPE_INFOFRAME_MPEGSRC = 0x85,
	PKT_TYPE_INFOFRAME_NVBI = 0x86,
	PKT_TYPE_INFOFRAME_DRM = 0x87,
	PKT_TYPE_EMP = 0x7f,

	PKT_TYPE_UNKNOWN,
};

enum pkt_op_flag {
	/*infoframe type*/
	PKT_OP_VSI = 0x01,
	PKT_OP_AVI = 0x02,
	PKT_OP_SPD = 0x04,
	PKT_OP_AIF = 0x08,

	PKT_OP_MPEGS = 0x10,
	PKT_OP_NVBI = 0x20,
	PKT_OP_DRM = 0x40,
	PKT_OP_EMP = 0x80,

	PKT_OP_ACR = 0x100,
	PKT_OP_GCP = 0x200,
	PKT_OP_ACP = 0x400,
	PKT_OP_ISRC1 = 0x800,

	PKT_OP_ISRC2 = 0x1000,
	PKT_OP_GMD = 0x2000,
	PKT_OP_AMP = 0x4000,
};

struct pkt_typeregmap_st {
	uint32_t pkt_type;
	uint32_t reg_bit;
};

/* audio clock regeneration pkt - 0x1 */
struct acr_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	uint8_t zero0;
	uint8_t zero1;
	uint8_t rsvd;
	/*sub packet 1*/
	struct sbpkt1_st {
		/*subpacket*/
		uint8_t SB0;
		uint8_t SB1_CTS_H:4;
		uint8_t SB1_rev:4;
		uint8_t SB2_CTS_M;
		uint8_t SB3_CTS_L;

		uint8_t SB4_N_H:4;
		uint8_t SB4_rev:4;
		uint8_t SB5_N_M;
		uint8_t SB6_N_L;
	} sbpkt1;
	/*sub packet 2*/
	struct sbpkt2_st {
		/*subpacket*/
		uint8_t SB0;
		uint8_t SB1_CTS_H:4;
		uint8_t SB1_rev:4;
		uint8_t SB2_CTS_M;
		uint8_t SB3_CTS_L;

		uint8_t SB4_N_H:4;
		uint8_t SB4_rev:4;
		uint8_t SB5_N_M;
		uint8_t SB6_N_L;
	} sbpkt2;
	/*sub packet 3*/
	struct sbpkt3_st {
		/*subpacket*/
		uint8_t SB0;
		uint8_t SB1_CTS_H:4;
		uint8_t SB1_rev:4;
		uint8_t SB2_CTS_M;
		uint8_t SB3_CTS_L;

		uint8_t SB4_N_H:4;
		uint8_t SB4_rev:4;
		uint8_t SB5_N_M;
		uint8_t SB6_N_L;
	} sbpkt3;
	/*sub packet 4*/
	struct sbpkt4_st {
		/*subpacket*/
		uint8_t SB0;
		uint8_t SB1_CTS_H:4;
		uint8_t SB1_rev:4;
		uint8_t SB2_CTS_M;
		uint8_t SB3_CTS_L;

		uint8_t SB4_N_H:4;
		uint8_t SB4_rev:4;
		uint8_t SB5_N_M;
		uint8_t SB6_N_L;
	} sbpkt4;
};

/* audio sample pkt - 0x2 */
struct aud_sample_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	/*HB1*/
	uint8_t sample_present:4;
	uint8_t layout:1;
	uint8_t hb1_rev:3;
	/*HB2*/
	uint8_t sample_flat:4;
	uint8_t b:4;
	uint8_t rsvd;
	struct aud_smpsbpkt_st {
		/*subpacket*/
		uint32_t left_27_4:24;
		uint32_t right_27_4:24;
		/*valid bit from first sub-frame*/
		uint32_t valid_l:1;
		/*user data bit from first sub-frame*/
		uint32_t user_l:1;
		/*channel status bit from first sub-frame*/
		uint32_t channel_l:1;
		/*parity bit from first sub-frame*/
		uint32_t parity_l:1;
		/*valid bit from second sub-frame*/
		uint32_t valid_r:1;
		/*user data bit from second sub-frame*/
		uint32_t user_r:1;
		/*channel status bit from second sub-frame*/
		uint32_t channel_r:1;
		/*parity bit from second sub-frame*/
		uint32_t parity_r:1;
	} sbpkt;
};

/* general control pkt - 0x3 */
struct gcp_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	uint8_t hb1_zero;
	uint8_t hb2_zero;
	uint8_t rsvd;
	/*sub packet*/
	struct gcp_sbpkt_st {
		/*SB0*/
		uint8_t set_avmute:1;
		uint8_t sb0_zero0:3;
		uint8_t clr_avmute:1;
		uint8_t sb0_zero1:3;
		/*SB1*/
		uint8_t colordepth:4;
		uint8_t pixelpkgphase:4;
		/*SB2*/
		uint8_t def_phase:1;
		uint8_t sb2_zero:7;
		/*SB3*/
		uint8_t sb3_zero;
		/*SB4*/
		uint8_t sb4_zero;
		/*SB5*/
		uint8_t sb5_zero;
		/*SB6*/
		uint8_t sb6_zero;
	} sbpkt;
};

/* acp control pkt - 0x4 */
struct acp_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	uint8_t acp_type;
	uint8_t rev;
	uint8_t rsvd;
	struct acp_sbpkt_st {
		/*depend on acp_type,section 9.3 for detail*/
		uint8_t pb[28];
	} sbpkt_st;
};

/* ISRC1 pkt - 0x50 and x06 */
struct isrc_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	uint8_t isrc_sts:3;
	uint8_t hb1_rev:3;
	uint8_t	isrc_valid:1;
	uint8_t	isrc_cont:1;
	uint8_t hb2_rev;
	uint8_t rsvd;
	/*sub-pkt section 8.2 for detail*/
	struct isrc_sbpkt_st {
		/*UPC_EAN_ISRC 0-15*/
		/*UPC_EAN_ISRC 16-32*/
		uint8_t upc_ean_isrc[16];
		uint8_t rev[12];
	} sbpkt;
};

/* one bit audio sample pkt - 0x7 */
struct obasmp_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	/*HB1*/
	uint8_t samples_presents_sp_x:4;
	uint8_t layout:1;
	uint8_t hb1_rev:3;
	/*HB2*/
	uint8_t samples_invalid_sp_x:4;
	uint8_t hb2_rev:4;
	uint8_t rsvd;
	/*subpacket*/
	struct oba_sbpkt_st {
		uint8_t chA_part0_7;
		uint8_t chA_part8_15;
		uint8_t chA_part16_23;
		uint8_t chB_part0_7;
		uint8_t chB_part8_15;
		uint8_t chB_part16_23;
		uint8_t chA_part24_27:4;
		uint8_t chB_part24_27:4;
	} sbpkt;
};

/* DST audio pkt - 0x8 */
struct dstaud_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	/*HB1*/
	uint8_t dst_normal_double:1;
	uint8_t hb1_rsvd:5;
	uint8_t sample_invalid:1;
	uint8_t frame_start:1;
	/*HB2*/
	uint8_t hb2_rsvd;
	uint8_t rsvd;
	struct dts_subpkt_st {
		uint8_t data[28];
	} sbpkt;
};

/* hbr audio pkt - 0x9 */
struct hbraud_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	/*HB1*/
	uint8_t hb1_rsvd;
	/*HB2*/
	uint8_t hb2_rsvd:4;
	uint8_t bx:4;

	uint8_t rsvd;
	/*subpacket*/
	/*null*/
};

/* gamut metadata pkt - 0xa */
struct gamutmeta_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	/*HB1*/
	uint8_t affect_seq_num:4;
	uint8_t gbd_profile:3;
	uint8_t next_feild:1;
	/*HB2*/
	uint8_t cur_seq_num:4;
	uint8_t pkt_seq:2;
	uint8_t hb2_rsvd:1;
	uint8_t no_cmt_gbd:1;
	uint8_t rsvd;
	/*subpacket*/
	union gamut_sbpkt_e {
		uint8_t p0_gbd_byte[28];
		struct p1_profile_st {
			uint8_t gbd_length_h;
			uint8_t gbd_length_l;
			uint8_t checksum;
			uint8_t gbd_byte_l[25];
		} p1_profile;
		uint8_t p1_gbd_byte_h[28];
	} sbpkt;
};

/* 3d audio sample pkt - 0xb */
struct a3dsmp_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	/*hb1*/
	uint8_t sample_presents:4;
	uint8_t sample_start:1;
	uint8_t hb1_rsvd:3;
	/*hb2*/
	uint8_t sample_flat_sp:4;
	uint8_t b_x:4;

	uint8_t rsvd;
	/*audio sub-packet*/
	struct aud3d_sbpkt_st {
		uint32_t left_27_4:24;
		uint32_t right_27_4:24;
		/*valid bit from first sub-frame*/
		uint8_t valid_l:1;
		/*user data bit from first sub-frame*/
		uint8_t user_l:1;
		/*channel status bit from first sub-frame*/
		uint8_t channel_l:1;
		/*parity bit from first sub-frame*/
		uint8_t parity_l:1;
		/*valid bit from second sub-frame*/
		uint8_t valid_r:1;
		/*user data bit from second sub-frame*/
		uint8_t user_r:1;
		/*channel status bit from second sub-frame*/
		uint8_t channel_r:1;
		/*parity bit from second sub-frame*/
		uint8_t parity_r:1;
	} sbpkt;
};

/* one bit 3d audio sample pkt - 0xc */
struct ob3d_smppkt_st {
	/*packet header*/
	uint8_t pkttype;
	/*hb1*/
	uint8_t samples_present_sp_x:4;
	uint8_t sample_start:1;
	uint8_t hb1_rsvd:3;
	/*hb2*/
	uint8_t samples_invalid_sp_x:4;
	uint8_t hb2_rsvd:4;

	uint8_t rsvd;
	/*subpacket*/
	struct ob_sbpkt {
		uint8_t chA_part0_7;
		uint8_t chA_part8_15;
		uint8_t chA_part16_23;
		uint8_t chB_part0_7;
		uint8_t chB_part8_15;
		uint8_t chB_part16_23;
		uint8_t chA_part24_27:4;
		uint8_t chB_part24_27:4;
	} sbpkt;
};

/* audio metadata pkt - 0xd */
struct audmtdata_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	/*hb1*/
	uint8_t audio_3d:1;
	uint8_t hb1_rsvd:7;
	/*hb2*/
	uint8_t num_view:2;
	uint8_t num_audio_str:2;
	uint8_t hb2_rsvd:4;

	uint8_t rsvd;
	/*sub-packet*/
	union aud_mdata_subpkt_u {
		struct aud_mtsbpkt_3d_1_st {
			uint8_t threeD_cc:5;
			uint8_t rsvd2:3;

			uint8_t acat:4;
			uint8_t rsvd3:4;

			uint8_t threeD_ca;
			uint8_t rsvd4[25];
		} subpkt_3d_1;

		struct aud_mtsbpkt_3d_0_st {
			uint8_t descriptor0[5];
			uint8_t descriptor1[5];
			uint8_t descriptor2[5];
			uint8_t descriptor3[5];
			uint8_t rsvd4[8];
		} subpkt_3d_0;
	} sbpkt;
};

/* multi-stream audio sample pkt - 0xe */
struct msaudsmp_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	/*hb1*/
	uint8_t stream_present_sp_x:4;
	uint8_t hb1_rsvd:4;
	/*hb2*/
	uint8_t stream_flat_sp:4;
	uint8_t b_x:4;

	uint8_t rsvd1;
	/*audio sub-packet*/
	struct audmul_sbpkt_st {
		/*subpacket*/
		uint32_t left_27_4:24;
		uint32_t right_27_4:24;
		/*valid bit from first sub-frame*/
		uint32_t valid_l:1;
		/*user data bit from first sub-frame*/
		uint32_t user_l:1;
		/*channel status bit from first sub-frame*/
		uint32_t channel_l:1;
		/*parity bit from first sub-frame*/
		uint32_t parity_l:1;
		/*valid bit from second sub-frame*/
		uint32_t valid_r:1;
		/*user data bit from second sub-frame*/
		uint32_t user_r:1;
		/*channel status bit from second sub-frame*/
		uint32_t channel_r:1;
		/*parity bit from second sub-frame*/
		uint32_t parity_r:1;
	} sbpkt;
};

/* one bit multi-stream audio sample pkt - 0xf */
struct obmaudsmp_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	/*hb1*/
	uint8_t stream_present_sp_x:4;
	uint8_t hb1_rsvd:4;
	/*hb2*/
	uint8_t stream_invalid_sp_x:4;
	uint8_t hb2_rsvd:4;

	uint8_t rsvd;
	/*audio sub-packet*/
	struct onebmtstr_smaud_sbpkt_st {
		uint8_t chA_part0_7;
		uint8_t chA_part8_15;
		uint8_t chA_part16_23;
		uint8_t chB_part0_7;
		uint8_t chB_part8_15;
		uint8_t chB_part16_23;
		uint8_t chA_part24_27:4;
		uint8_t chB_part24_27:4;
	} __packed sbpkt;
} __packed;


/* EMP pkt - 0x7f */
struct emp_pkt_st {
	/*packet header*/
	uint8_t pkttype;
	/*hb1*/
	uint8_t first:1;
	uint8_t last:1;
	uint8_t hb1_rsvd:6;
	/*hb2*/
	uint8_t sequence_idx;

	uint8_t rsvd;
	/*content*/
	struct content_st {
		uint8_t new:1;
		uint8_t end:1;
		uint8_t ds_type:2;
		uint8_t afr:1;
		uint8_t vfr:1;
		uint8_t sync:1;
		uint8_t rev_0:1;
		uint8_t rev_1;
		uint8_t organization_id;
		uint16_t data_set_tag;
		uint16_t data_set_length;
		uint8_t md[21];
	} __packed cnt;
} __packed;


/* fifo raw data type - 0x8x */
struct fifo_rawdata_st {
	/*packet header*/
	uint8_t pkttype;
	uint8_t version;
	uint8_t length;
	uint8_t rsd;
	/*packet body*/
	uint8_t PB[28];
} __packed;

/* vendor specific infoFrame packet - 0x81 */
struct vsi_infoframe_st {
	uint8_t pkttype;
	struct vsi_ver_st {
		uint8_t version:7;
		uint8_t chgbit:1;
	} __packed ver_st;
	uint8_t length;
	uint8_t rsd;

	/*PB0*/
	uint32_t checksum:8;
	/*PB1-3*/
	uint32_t ieee:24;/* first two hex digits*/

	/*body by different format*/
	union vsi_sbpkt_u {
		struct payload_st {
			uint32_t data[6];
		} __packed payload;

		/* video format 0x01*/
		struct vsi_st {
			/*pb4*/
			uint8_t rsvd0:5;
			uint8_t vdfmt:3;
			/*pb5*/
			uint8_t hdmi_vic;
			/*pb6*/
			uint8_t data[22];
		} __packed vsi;

		/* 3D: video format(0x2) */
		struct vsi_3Dext_st {
			/*pb4*/
			uint8_t rsvd0:5;
			uint8_t vdfmt:3;
			/*pb5*/
			uint8_t rsvd2:3;
			uint8_t threeD_meta_pre:1;
			uint8_t threeD_st:4;
			/*pb6*/
			uint8_t rsvd3:4;
			uint8_t threeD_ex:4;
			/*pb7*/
			uint8_t threeD_meta_type:3;
			uint8_t threeD_meta_length:5;
			uint8_t threeD_meta_data[20];
		} __packed vsi_3Dext;

		/*dolby vision , length 0x18*/
		struct vsi_DobV_st {
			/*pb4*/
			uint8_t rsvd0:5;
			uint8_t vdfmt:3;
			/*pb5*/
			uint8_t hdmi_vic;
			/*pb6*/
			uint8_t data[22];/* val=0 */
		} __packed vsi_DobV;
		/*TODO:hdmi2.1 spec vsi packet*/
		struct vsi_st_21 {
			/*pb4*/
			uint8_t ver:8;
			/*pb5*/
			uint8_t threeD_valid:1;
			uint8_t allm_mode:1;
			uint8_t rsvd1:2;
			uint8_t ccbpc:4;
			/*pb6*/
			/*todo*/
		} __packed vsi_st_21;
	} __packed sbpkt;
} __packed;

/* AVI infoFrame packet - 0x82 */
struct avi_infoframe_st {
	uint8_t pkttype;
	uint8_t version;
	uint8_t length;
	uint8_t rsd;
	/*PB0*/
	uint8_t checksum;
	union cont_u {
		struct v1_st {
			/*byte 1*/
			uint8_t scaninfo:2;			/* S1,S0 */
			uint8_t barinfo:2;			/* B1,B0 */
			uint8_t activeinfo:1;		/* A0 */
			uint8_t colorindicator:2;	/* Y1,Y0 */
			uint8_t rev0:1;
			/*byte 2*/
			uint8_t fmt_ration:4;		/* R3-R0 */
			uint8_t pic_ration:2;		/* M1-M0 */
			uint8_t colorimetry:2;		/* C1-C0 */
			/*byte 3*/
			uint8_t pic_scaling:2;		/* SC1-SC0 */
			uint8_t rev1:6;
			/*byte 4*/
			uint8_t rev2:8;
			/*byte 5*/
			uint8_t rev3:8;
		} __packed v1;
		struct v2v3_st {
			/*byte 1*/
			uint8_t scaninfo:2;			/* S1,S0 */
			uint8_t barinfo:2;			/* B1,B0 */
			uint8_t activeinfo:1;		/* A0 1:R3-R0*/
			uint8_t colorindicator:3;	/* Y2-Y0 */
			/*byte 2*/
			uint8_t fmt_ration:4;		/* R3-R0 */
			uint8_t pic_ration:2;		/* M1-M0 */
			uint8_t colorimetry:2;		/* C1-C0 */
			/*byte 3*/
			uint8_t pic_scaling:2;		/* SC1-SC0 */
			uint8_t qt_range:2;			/* Q1-Q0 */
			uint8_t ext_color:3;		/* EC2-EC0 */
			uint8_t it_content:1;		/* ITC */
			/*byte 4*/
			uint8_t vic:8;				/* VIC7-VIC0 */
			/*byte 5*/
			uint8_t pix_repeat:4;		/* PR3-PR0 */
			uint8_t content_type:2;		/* CN1-CN0 */
			uint8_t ycc_range:2;		/* YQ1-YQ0 */
		} __packed v2v3;
	} cont;
	/*byte 6,7*/
	uint16_t line_num_end_topbar:16;	/*littel endian can use*/
	/*byte 8,9*/
	uint16_t line_num_start_btmbar:16;
	/*byte 10,11*/
	uint16_t pix_num_left_bar:16;
	/*byte 12,13*/
	uint16_t pix_num_right_bar:16;
} __packed;

/* source product descriptor infoFrame  - 0x83 */
struct spd_infoframe_st {
	uint8_t pkttype;
	uint8_t version;
	uint8_t length;				/*length=25*/
	uint8_t rsd;
	uint8_t checksum;
	/*Vendor Name Character*/
	uint8_t vendor_name[8];
	/*Product Description Character*/
	uint8_t product_des[16];
	/*byte 25*/
	uint8_t source_info;
} __packed;

/* audio infoFrame packet - 0x84 */
struct aud_infoframe_st {
	uint8_t pkttype;
	uint8_t version;
	uint8_t length;
	uint8_t rsd;
	uint8_t checksum;
	/*byte 1*/
	uint8_t ch_count:3;		/*CC2-CC0*/
	uint8_t rev0:1;
	uint8_t coding_type:4;	/*CT3-CT0*/
	/*byte 2*/
	uint8_t sample_size:2;	/*SS1-SS0*/
	uint8_t sample_frq:3;	/*SF2-SF0*/
	uint8_t rev1:3;
	/*byte 3*/
	uint8_t fromat;		/*fmt according to CT3-CT0*/
	/*byte 4*/
	uint8_t ca;		/*CA7-CA0*/
	/*byte 5*/
	uint8_t lfep:2; /*BL1-BL0*/
	uint8_t rev2:1;
	uint8_t level_shift_value:4;/*LSV3-LSV0*/
	uint8_t down_mix:1;/*DM_INH*/
	/*byte 6-10*/
	uint8_t rev[5];
} __packed;

/* mpeg source infoframe packet - 0x85 */
struct ms_infoframe_st {
	uint8_t pkttype;
	uint8_t version;
	uint8_t length;
	uint8_t rsd;
	uint8_t checksum;
	/*byte 1-4*/
	/*little endian mode*/
	uint32_t bitrate;	/*byte MB0(low)-MB3(upper)*/

	/*byte 5*/
	struct ms_byte5_st {
		uint8_t mpeg_frame:2;/*MF1-MF0*/
		uint8_t rev0:2;
		uint8_t field_rpt:1;/*FR0*/
		uint8_t rev1:3;
	} __packed b5_st;
	/*byte 6-10*/
	uint8_t rev[5];
} __packed;

/* ntsc vbi infoframe packet - 0x86 */
struct vbi_infoframe_st {
	uint8_t pkttype;
	uint8_t version;
	uint8_t length;
	uint8_t rsd;
	uint8_t checksum;
	/*packet content*/
	uint8_t data_identifier;
	uint8_t data_unit_id;
	uint8_t data_unit_length;
	uint8_t data_field[24];
} __packed;

/* dynamic range and mastering infoframe packet - 0x87 */
struct drm_infoframe_st {
	uint8_t pkttype;
	uint8_t version;
	uint8_t length;
	uint8_t rsd;

	/*static metadata descriptor*/
	union meta_des_u {
		struct des_type1_st {
			/*PB0*/
			uint8_t checksum;
			/*PB1*/
			/*electrico-optinal transfer function*/
			uint8_t eotf:3;
			uint8_t rev0:5;
			/*PB2*/
			/*static metadata descriptor id*/
			uint8_t meta_des_id:3;
			uint8_t rev1:5;

			/*little endian use*/
			/*display primaries*/
			uint16_t dis_pri_x0;
			uint16_t dis_pri_y0;
			uint16_t dis_pri_x1;
			uint16_t dis_pri_y1;
			uint16_t dis_pri_x2;
			uint16_t dis_pri_y2;
			uint16_t white_points_x;
			uint16_t white_points_y;
			/*max display mastering luminance*/
			uint16_t max_dislum;
			/*min display mastering luminance*/
			uint16_t min_dislum;
			/*maximum content light level*/
			uint16_t max_light_lvl;
			/*maximum frame-average light level*/
			uint16_t max_fa_light_lvl;
		} __packed tp1;
		uint32_t payload[7];
	} __packed des_u;
} __packed;

union pktinfo {
	/*normal packet 0x0-0xf*/
	struct acr_pkt_st audclkgen_ptk;
	struct aud_sample_pkt_st audsmp_pkt;
	struct gcp_pkt_st gcp_pkt;
	struct acp_pkt_st acp_pkt;
	struct isrc_pkt_st isrc_pkt;
	struct obasmp_pkt_st onebitaud_pkt;
	struct dstaud_pkt_st dstaud_pkt;
	struct hbraud_pkt_st hbraud_pkt;
	struct gamutmeta_pkt_st gamutmeta_pkt;
	struct a3dsmp_pkt_st aud3dsmp_pkt;
	struct ob3d_smppkt_st oneb3dsmp_pkt;
	struct audmtdata_pkt_st audmeta_pkt;
	struct msaudsmp_pkt_st mulstraudsamp_pkt;
	struct obmaudsmp_pkt_st obmasmpaud_pkt;
	struct emp_pkt_st emp_pkt;
};

union infoframe_u {
	/*info frame 0x81 - 0x87*/
	/* struct pd_infoframe_s word_md_infoframe; */
	struct fifo_rawdata_st raw_infoframe;
	struct vsi_infoframe_st vsi_infoframe;
	struct avi_infoframe_st avi_infoframe;
	struct spd_infoframe_st spd_infoframe;
	struct aud_infoframe_st aud_infoframe;
	struct ms_infoframe_st ms_infoframe;
	struct vbi_infoframe_st vbi_infoframe;
	struct drm_infoframe_st drm_infoframe;
};

enum vsi_vid_format_e {
	VSI_FORMAT_NO_DATA,
	VSI_FORMAT_EXT_RESOLUTION,
	VSI_FORMAT_3D_FORMAT,
	VSI_FORMAT_FUTURE,
};

struct rxpkt_st {
	uint32_t pkt_cnt_avi;
	uint32_t pkt_cnt_vsi;
	uint32_t pkt_cnt_drm;
	uint32_t pkt_cnt_spd;
	uint32_t pkt_cnt_audif;
	uint32_t pkt_cnt_mpeg;
	uint32_t pkt_cnt_nvbi;

	uint32_t pkt_cnt_acr;
	uint32_t pkt_cnt_gcp;
	uint32_t pkt_cnt_acp;
	uint32_t pkt_cnt_isrc1;
	uint32_t pkt_cnt_isrc2;
	uint32_t pkt_cnt_gameta;
	uint32_t pkt_cnt_amp;
	uint32_t pkt_cnt_emp;

	uint32_t pkt_cnt_vsi_ex;
	uint32_t pkt_cnt_drm_ex;
	uint32_t pkt_cnt_gmd_ex;
	uint32_t pkt_cnt_aif_ex;
	uint32_t pkt_cnt_avi_ex;
	uint32_t pkt_cnt_acr_ex;
	uint32_t pkt_cnt_gcp_ex;
	uint32_t pkt_cnt_amp_ex;
	uint32_t pkt_cnt_nvbi_ex;
	uint32_t pkt_cnt_emp_ex;

	uint32_t pkt_op_flag;

	uint32_t fifo_Int_cnt;
	uint32_t fifo_pkt_num;

	uint32_t pkt_chk_flg;

	uint32_t pkt_attach_vsi;
	uint32_t pkt_attach_drm;
};

struct pd_infoframe_s {
	uint32_t HB;
	uint32_t PB0;
	uint32_t PB1;
	uint32_t PB2;
	uint32_t PB3;
	uint32_t PB4;
	uint32_t PB5;
	uint32_t PB6;
};

struct packet_info_s {
	/* packet type 0x81 vendor-specific */
	struct pd_infoframe_s vs_info;
	/* packet type 0x82 AVI */
	struct pd_infoframe_s avi_info;
	/* packet type 0x83 source product description */
	struct pd_infoframe_s spd_info;
	/* packet type 0x84 Audio */
	struct pd_infoframe_s aud_pktinfo;
	/* packet type 0x85 Mpeg source */
	struct pd_infoframe_s mpegs_info;
	/* packet type 0x86 NTSCVBI */
	struct pd_infoframe_s ntscvbi_info;
	/* packet type 0x87 DRM */
	struct pd_infoframe_s drm_info;

	/* packet type 0x01 info */
	struct pd_infoframe_s acr_info;
	/* packet type 0x03 info */
	struct pd_infoframe_s gcp_info;
	/* packet type 0x04 info */
	struct pd_infoframe_s acp_info;
	/* packet type 0x05 info */
	struct pd_infoframe_s isrc1_info;
	/* packet type 0x06 info */
	struct pd_infoframe_s isrc2_info;
	/* packet type 0x0a info */
	struct pd_infoframe_s gameta_info;
	/* packet type 0x0d audio metadata data */
	struct pd_infoframe_s amp_info;

	/* packet type 0x7f emp */
	struct pd_infoframe_s emp_info;
};

struct st_pkt_test_buff {
	/* packet type 0x81 vendor-specific */
	struct pd_infoframe_s vs_info;
	/* packet type 0x82 AVI */
	struct pd_infoframe_s avi_info;
	/* packet type 0x83 source product description */
	struct pd_infoframe_s spd_info;
	/* packet type 0x84 Audio */
	struct pd_infoframe_s aud_pktinfo;
	/* packet type 0x85 Mpeg source */
	struct pd_infoframe_s mpegs_info;
	/* packet type 0x86 NTSCVBI */
	struct pd_infoframe_s ntscvbi_info;
	/* packet type 0x87 DRM */
	struct pd_infoframe_s drm_info;

	/* packet type 0x01 info */
	struct pd_infoframe_s acr_info;
	/* packet type 0x03 info */
	struct pd_infoframe_s gcp_info;
	/* packet type 0x04 info */
	struct pd_infoframe_s acp_info;
	/* packet type 0x05 info */
	struct pd_infoframe_s isrc1_info;
	/* packet type 0x06 info */
	struct pd_infoframe_s isrc2_info;
	/* packet type 0x0a info */
	struct pd_infoframe_s gameta_info;
	/* packet type 0x0d audio metadata data */
	struct pd_infoframe_s amp_info;

	/* packet type 0x7f EMP */
	struct pd_infoframe_s emp_info;

	/*externl set*/
	struct pd_infoframe_s ex_vsi;
	struct pd_infoframe_s ex_avi;
	struct pd_infoframe_s ex_audif;
	struct pd_infoframe_s ex_drm;
	struct pd_infoframe_s ex_nvbi;
	struct pd_infoframe_s ex_acr;
	struct pd_infoframe_s ex_gcp;
	struct pd_infoframe_s ex_gmd;
	struct pd_infoframe_s ex_amp;
};



extern struct packet_info_s rx_pkt;
/*extern bool hdr_enable;*/
extern void rx_pkt_status(void);
extern void rx_pkt_debug(void);
extern void rx_debug_pktinfo(char input[][20]);
extern void rx_pkt_dump(enum pkt_type_e typeID);
extern void rx_pkt_initial(void);
extern int rx_pkt_handler(enum pkt_decode_type pkt_int_src);
extern uint32_t rx_pkt_type_mapping(enum pkt_type_e pkt_type);
extern void rx_pkt_buffclear(enum pkt_type_e pkt_type);
extern void rx_pkt_content_chk_en(uint32_t enable);
extern void rx_pkt_check_content(void);
extern void rx_pkt_set_fifo_pri(uint32_t pri);
extern uint32_t rx_pkt_get_fifo_pri(void);

extern void rx_get_vsi_info(void);

/*please ignore checksum byte*/
extern void rx_pkt_get_audif_ex(void *pktinfo);
/*please ignore checksum byte*/
extern void rx_pkt_get_avi_ex(void *pktinfo);
extern void rx_pkt_get_drm_ex(void *pktinfo);
extern void rx_pkt_get_acr_ex(void *pktinfo);
extern void rx_pkt_get_gmd_ex(void *pktinfo);
extern void rx_pkt_get_ntscvbi_ex(void *pktinfo);
extern void rx_pkt_get_amp_ex(void *pktinfo);
extern void rx_pkt_get_vsi_ex(void *pktinfo);
extern void rx_pkt_get_gcp_ex(void *pktinfo);

extern uint32_t rx_pkt_chk_attach_vsi(void);
extern void rx_pkt_clr_attach_vsi(void);
extern uint32_t rx_pkt_chk_attach_drm(void);
extern void rx_pkt_clr_attach_drm(void);
extern uint32_t rx_pkt_chk_busy_vsi(void);
extern uint32_t rx_pkt_chk_busy_drm(void);
extern void rx_get_pd_fifo_param(enum pkt_type_e pkt_type,
		struct pd_infoframe_s *pkt_info);

#endif


