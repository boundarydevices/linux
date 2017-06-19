/*
 * include/linux/amlogic/media/utils/amstream.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#ifndef AMSTREAM_H
#define AMSTREAM_H

/* #include <linux/interrupt.h> */
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/utils/aformat.h>

#ifdef __KERNEL__
#define PORT_FLAG_IN_USE    0x0001
#define PORT_FLAG_VFORMAT   0x0002
#define PORT_FLAG_AFORMAT   0x0004
#define PORT_FLAG_FORMAT    (PORT_FLAG_VFORMAT | PORT_FLAG_AFORMAT)
#define PORT_FLAG_VID       0x0008
#define PORT_FLAG_AID       0x0010
#define PORT_FLAG_SID       0x0020
#define PORT_FLAG_UD        0x0040
#define PORT_FLAG_DRM       0x0080
#define PORT_FLAG_ID        (PORT_FLAG_VID | \
		PORT_FLAG_AID | PORT_FLAG_SID | PORT_FLAG_UD)
#define PORT_FLAG_INITED    0x100

#define PORT_TYPE_VIDEO         0x01
#define PORT_TYPE_AUDIO         0x02
#define PORT_TYPE_MPTS          0x04
#define PORT_TYPE_MPPS          0x08
#define PORT_TYPE_ES            0x10
#define PORT_TYPE_RM            0x20
#define PORT_TYPE_SUB           0x40
#define PORT_TYPE_SUB_RD        0x80
#define PORT_TYPE_HEVC          0x100
#define PORT_TYPE_USERDATA      0x200
#define PORT_TYPE_FRAME         0x400
#define PORT_TYPE_DECODER_SCHED 0x800
#define PORT_TYPE_DUALDEC       0x1000
#endif

#define _A_M  'S'
#define AMSTREAM_IOC_VB_START   _IOW((_A_M), 0x00, int)
#define AMSTREAM_IOC_VB_SIZE    _IOW((_A_M), 0x01, int)
#define AMSTREAM_IOC_AB_START   _IOW((_A_M), 0x02, int)
#define AMSTREAM_IOC_AB_SIZE    _IOW((_A_M), 0x03, int)
#define AMSTREAM_IOC_VFORMAT    _IOW((_A_M), 0x04, int)
#define AMSTREAM_IOC_AFORMAT    _IOW((_A_M), 0x05, int)
#define AMSTREAM_IOC_VID        _IOW((_A_M), 0x06, int)
#define AMSTREAM_IOC_AID        _IOW((_A_M), 0x07, int)
#define AMSTREAM_IOC_VB_STATUS  _IOR((_A_M), 0x08, int)
#define AMSTREAM_IOC_AB_STATUS  _IOR((_A_M), 0x09, int)
#define AMSTREAM_IOC_SYSINFO    _IOW((_A_M), 0x0a, int)
#define AMSTREAM_IOC_ACHANNEL   _IOW((_A_M), 0x0b, int)
#define AMSTREAM_IOC_SAMPLERATE _IOW((_A_M), 0x0c, int)
#define AMSTREAM_IOC_DATAWIDTH  _IOW((_A_M), 0x0d, int)
#define AMSTREAM_IOC_TSTAMP     _IOW((_A_M), 0x0e, int)
#define AMSTREAM_IOC_VDECSTAT   _IOR((_A_M), 0x0f, int)
#define AMSTREAM_IOC_ADECSTAT   _IOR((_A_M), 0x10, int)

#define AMSTREAM_IOC_PORT_INIT   _IO((_A_M), 0x11)
#define AMSTREAM_IOC_TRICKMODE  _IOW((_A_M), 0x12, int)

#define AMSTREAM_IOC_AUDIO_INFO	_IOW((_A_M), 0x13, int)
#define AMSTREAM_IOC_TRICK_STAT  _IOR((_A_M), 0x14, int)
#define AMSTREAM_IOC_AUDIO_RESET _IO((_A_M), 0x15)
#define AMSTREAM_IOC_SID         _IOW((_A_M), 0x16, int)
#define AMSTREAM_IOC_VPAUSE      _IOW((_A_M), 0x17, int)
#define AMSTREAM_IOC_AVTHRESH   _IOW((_A_M), 0x18, int)
#define AMSTREAM_IOC_SYNCTHRESH _IOW((_A_M), 0x19, int)
#define AMSTREAM_IOC_SUB_RESET   _IOW((_A_M), 0x1a, int)
#define AMSTREAM_IOC_SUB_LENGTH  _IOR((_A_M), 0x1b, int)
#define AMSTREAM_IOC_SET_DEC_RESET _IOW((_A_M), 0x1c, int)
#define AMSTREAM_IOC_TS_SKIPBYTE _IOW((_A_M), 0x1d, int)
#define AMSTREAM_IOC_SUB_TYPE    _IOW((_A_M), 0x1e, int)
#define AMSTREAM_IOC_CLEAR_VIDEO _IOW((_A_M), 0x1f, int)
#define AMSTREAM_IOC_VDECINFO _IOR((_A_M), 0x20, int)
#define AMSTREAM_IOC_GLOBAL_GET_VIDEO_OUTPUT  _IOR((_A_M), 0x21, int)
#define AMSTREAM_IOC_GLOBAL_SET_VIDEO_OUTPUT  _IOW((_A_M), 0x22, int)

#define AMSTREAM_IOC_APTS             _IOR((_A_M), 0x40, int)
#define AMSTREAM_IOC_VPTS             _IOR((_A_M), 0x41, int)
#define AMSTREAM_IOC_PCRSCR           _IOR((_A_M), 0x42, int)
#define AMSTREAM_IOC_SYNCENABLE      _IOW((_A_M), 0x43, int)
#define AMSTREAM_IOC_GET_SYNC_ADISCON  _IOR((_A_M), 0x44, int)
#define AMSTREAM_IOC_SET_SYNC_ADISCON  _IOW((_A_M), 0x45, int)
#define AMSTREAM_IOC_GET_SYNC_VDISCON  _IOR((_A_M), 0x46, int)
#define AMSTREAM_IOC_SET_SYNC_VDISCON  _IOW((_A_M), 0x47, int)
#define AMSTREAM_IOC_GET_VIDEO_DISABLE  _IOR((_A_M), 0x48, int)
#define AMSTREAM_IOC_SET_VIDEO_DISABLE  _IOW((_A_M), 0x49, int)
#define AMSTREAM_IOC_SET_PCRSCR       _IOW((_A_M), 0x4a, int)
#define AMSTREAM_IOC_GET_VIDEO_AXIS   _IOR((_A_M), 0x4b, int)
#define AMSTREAM_IOC_SET_VIDEO_AXIS   _IOW((_A_M), 0x4c, int)
#define AMSTREAM_IOC_GET_VIDEO_CROP   _IOR((_A_M), 0x4d, int)
#define AMSTREAM_IOC_SET_VIDEO_CROP   _IOW((_A_M), 0x4e, int)
#define AMSTREAM_IOC_PCRID        _IOW((_A_M), 0x4f, int)

/* VPP.3D IOCTL command list^M */
#define  AMSTREAM_IOC_SET_3D_TYPE  _IOW((_A_M), 0x3c, unsigned int)
#define  AMSTREAM_IOC_GET_3D_TYPE  _IOW((_A_M), 0x3d, unsigned int)
#define  AMSTREAM_IOC_GET_SOURCE_VIDEO_3D_TYPE  _IOW((_A_M), 0x3e, unsigned int)

#define AMSTREAM_IOC_SUB_NUM	_IOR((_A_M), 0x50, int)
#define AMSTREAM_IOC_SUB_INFO	_IOR((_A_M), 0x51, int)
#define AMSTREAM_IOC_GET_BLACKOUT_POLICY   _IOR((_A_M), 0x52, int)
#define AMSTREAM_IOC_SET_BLACKOUT_POLICY   _IOW((_A_M), 0x53, int)
#define AMSTREAM_IOC_UD_LENGTH _IOR((_A_M), 0x54, int)
#define AMSTREAM_IOC_UD_POC _IOR((_A_M), 0x55, int)
#define AMSTREAM_IOC_UD_FLUSH_USERDATA _IOR((_A_M), 0x56, int)
#define AMSTREAM_IOC_GET_SCREEN_MODE _IOR((_A_M), 0x58, int)
#define AMSTREAM_IOC_SET_SCREEN_MODE _IOW((_A_M), 0x59, int)
#define AMSTREAM_IOC_GET_VIDEO_DISCONTINUE_REPORT _IOR((_A_M), 0x5a, int)
#define AMSTREAM_IOC_SET_VIDEO_DISCONTINUE_REPORT _IOW((_A_M), 0x5b, int)
#define AMSTREAM_IOC_VF_STATUS  _IOR((_A_M), 0x60, int)
#define AMSTREAM_IOC_CLEAR_VBUF _IO((_A_M), 0x80)

#define AMSTREAM_IOC_APTS_LOOKUP    _IOR((_A_M), 0x81, int)
#define GET_FIRST_APTS_FLAG    _IOR((_A_M), 0x82, int)
#define AMSTREAM_IOC_GET_SYNC_ADISCON_DIFF  _IOR((_A_M), 0x83, int)
#define AMSTREAM_IOC_GET_SYNC_VDISCON_DIFF  _IOR((_A_M), 0x84, int)
#define AMSTREAM_IOC_SET_SYNC_ADISCON_DIFF  _IOW((_A_M), 0x85, int)
#define AMSTREAM_IOC_SET_SYNC_VDISCON_DIFF  _IOW((_A_M), 0x86, int)
#define AMSTREAM_IOC_GET_FREERUN_MODE  _IOR((_A_M), 0x87, int)
#define AMSTREAM_IOC_SET_FREERUN_MODE  _IOW((_A_M), 0x88, int)
#define AMSTREAM_IOC_SET_VSYNC_UPINT   _IOW((_A_M), 0x89, int)
#define AMSTREAM_IOC_GET_VSYNC_SLOW_FACTOR   _IOW((_A_M), 0x8a, int)
#define AMSTREAM_IOC_SET_VSYNC_SLOW_FACTOR   _IOW((_A_M), 0x8b, int)
#define AMSTREAM_IOC_SET_DEMUX  _IOW((_A_M), 0x90, int)
#define AMSTREAM_IOC_SET_DRMMODE _IOW((_A_M), 0x91, int)
#define AMSTREAM_IOC_TSTAMP_uS64 _IOW((_A_M), 0x95, int)

#define AMSTREAM_IOC_SET_VIDEO_DELAY_LIMIT_MS _IOW((_A_M), 0xa0, int)
#define AMSTREAM_IOC_GET_VIDEO_DELAY_LIMIT_MS _IOR((_A_M), 0xa1, int)
#define AMSTREAM_IOC_SET_AUDIO_DELAY_LIMIT_MS _IOW((_A_M), 0xa2, int)
#define AMSTREAM_IOC_GET_AUDIO_DELAY_LIMIT_MS _IOR((_A_M), 0xa3, int)
#define AMSTREAM_IOC_GET_AUDIO_CUR_DELAY_MS _IOR((_A_M), 0xa4, int)
#define AMSTREAM_IOC_GET_VIDEO_CUR_DELAY_MS _IOR((_A_M), 0xa5, int)
#define AMSTREAM_IOC_GET_AUDIO_AVG_BITRATE_BPS _IOR((_A_M), 0xa6, int)
#define AMSTREAM_IOC_GET_VIDEO_AVG_BITRATE_BPS _IOR((_A_M), 0xa7, int)
#define AMSTREAM_IOC_SET_APTS  _IOW((_A_M), 0xa8, int)
#define AMSTREAM_IOC_GET_LAST_CHECKIN_APTS _IOR((_A_M), 0xa9, int)
#define AMSTREAM_IOC_GET_LAST_CHECKIN_VPTS _IOR((_A_M), 0xaa, int)
#define AMSTREAM_IOC_GET_LAST_CHECKOUT_APTS _IOR((_A_M), 0xab, int)
#define AMSTREAM_IOC_GET_LAST_CHECKOUT_VPTS _IOR((_A_M), 0xac, int)

/* subtitle.c get/set subtitle info */
#define AMSTREAM_IOC_GET_SUBTITLE_INFO _IOR((_A_M), 0xad, int)
#define AMSTREAM_IOC_SET_SUBTITLE_INFO _IOW((_A_M), 0xae, int)
#define AMSTREAM_IOC_SET_OMX_VPTS _IOW((_A_M), 0xaf, int)
#define AMSTREAM_IOC_GET_OMX_VPTS _IOW((_A_M), 0xb0, int)

#define AMSTREAM_IOC_GET_TRICK_VPTS _IOR((_A_M), 0xf0, int)
#define AMSTREAM_IOC_DISABLE_SLOW_SYNC _IOW((_A_M), 0xf1, int)

#define AMSTREAM_IOC_GET_AUDIO_CHECKIN_BITRATE_BPS _IOR((_A_M), 0xf2, int)
#define AMSTREAM_IOC_GET_VIDEO_CHECKIN_BITRATE_BPS _IOR((_A_M), 0xf3, int)
#define AMSTREAM_IOC_VDEC_RESET _IO((_A_M), 0xf4)
#define AMSTREAM_IOC_GET_VERSION _IOR((_A_M), 0xc0, int)
#define AMSTREAM_IOC_GET _IOWR((_A_M), 0xc1, struct am_ioctl_parm)
#define AMSTREAM_IOC_SET _IOW((_A_M), 0xc2, struct am_ioctl_parm)
#define AMSTREAM_IOC_GET_EX _IOWR((_A_M), 0xc3, struct am_ioctl_parm_ex)
#define AMSTREAM_IOC_SET_EX _IOW((_A_M), 0xc4, struct am_ioctl_parm_ex)
#define AMSTREAM_IOC_GET_PTR _IOWR((_A_M), 0xc5, struct am_ioctl_parm_ptr)
#define AMSTREAM_IOC_SET_PTR _IOW((_A_M), 0xc6, struct am_ioctl_parm_ptr)

#define TRICKMODE_NONE       0x00
#define TRICKMODE_I          0x01
#define TRICKMODE_FFFB       0x02

#define TRICK_STAT_DONE     0x01
#define TRICK_STAT_WAIT     0x00

#define AUDIO_EXTRA_DATA_SIZE   (4096)
#define MAX_SUB_NUM		32

enum VIDEO_DEC_TYPE {
	VIDEO_DEC_FORMAT_UNKNOWN,
	VIDEO_DEC_FORMAT_MPEG4_3,

	VIDEO_DEC_FORMAT_MPEG4_4,
	VIDEO_DEC_FORMAT_MPEG4_5,

	VIDEO_DEC_FORMAT_H264,

	VIDEO_DEC_FORMAT_MJPEG,
	VIDEO_DEC_FORMAT_MP4,
	VIDEO_DEC_FORMAT_H263,

	VIDEO_DEC_FORMAT_REAL_8,
	VIDEO_DEC_FORMAT_REAL_9,

	VIDEO_DEC_FORMAT_WMV3,

	VIDEO_DEC_FORMAT_WVC1,
	VIDEO_DEC_FORMAT_SW,
	VIDEO_DEC_FORMAT_MAX
};

enum FRAME_BASE_VIDEO_PATH {
	FRAME_BASE_PATH_IONVIDEO = 0,
	FRAME_BASE_PATH_AMLVIDEO_AMVIDEO,
	FRAME_BASE_PATH_AMLVIDEO1_AMVIDEO2,
	FRAME_BASE_PATH_DI_AMVIDEO,
	FRAME_BASE_PATH_AMVIDEO,
	FRAME_BASE_PATH_AMVIDEO2,
	FRAME_BASE_PATH_MAX
};

struct buf_status {

	int size;

	int data_len;

	int free_len;

	unsigned int read_pointer;

	unsigned int write_pointer;
};

/*struct vdec_status.status*/
#define STAT_TIMER_INIT     0x01
#define STAT_MC_LOAD        0x02
#define STAT_ISR_REG        0x04
#define STAT_VF_HOOK        0x08
#define STAT_TIMER_ARM      0x10
#define STAT_VDEC_RUN       0x20

/*struct vdec_status.status on error*/

#define PARSER_FATAL_ERROR              (0x10<<16)
#define DECODER_ERROR_VLC_DECODE_TBL    (0x20<<16)
#define PARSER_ERROR_WRONG_HEAD_VER     (0x40<<16)
#define PARSER_ERROR_WRONG_PACKAGE_SIZE (0x80<<16)
#define DECODER_FATAL_ERROR_SIZE_OVERFLOW     (0x100<<16)
#define DECODER_FATAL_ERROR_UNKNOWN             (0x200<<16)
#define DECODER_FATAL_ERROR_NO_MEM		(0x400<<16)

#define DECODER_ERROR_MASK	(0xffff<<16)

struct vdec_status {
	unsigned int width;
	unsigned int height;
	unsigned int fps;
	unsigned int error_count;
	unsigned int status;
};

struct vdec_info {
	char vdec_name[16];
	unsigned int ver;
	unsigned int frame_width;
	unsigned int frame_height;
	unsigned int frame_rate;
	unsigned int bit_rate;
	unsigned int frame_dur;
	unsigned int frame_data;
	unsigned int error_count;
	unsigned int status;
	unsigned int frame_count;
	unsigned int error_frame_count;
	unsigned int drop_frame_count;
	unsigned long long total_data;
	unsigned int samp_cnt;
	unsigned int offset;
	char reserved[32];
};

struct adec_status {
	unsigned int channels;
	unsigned int sample_rate;
	unsigned int resolution;
	unsigned int error_count;
	unsigned int status;
};

struct am_io_param {
	union {
		int data;
		int id;		/* get bufstatus? or others */
	};
	int len;		/* buffer size; */

	union {

		char buf[1];

		struct buf_status status;

		struct vdec_status vstatus;
		struct adec_status astatus;
	};
};

struct am_io_info {
	union {
		int data;
		int id;
	};
	int len;
	union {
		char buf[1];
		struct vdec_info vinfo;
	};
};

struct audio_info {

	int valid;

	int sample_rate;

	int channels;

	int bitrate;

	int codec_id;

	int block_align;

	int extradata_size;

	char extradata[AUDIO_EXTRA_DATA_SIZE];
};

struct dec_sysinfo {

	unsigned int format;

	unsigned int width;

	unsigned int height;

	unsigned int rate;

	unsigned int extra;

	unsigned int status;

	unsigned int ratio;

	void *param;

	unsigned long long ratio64;
};

struct subtitle_info {

	unsigned char id;

	unsigned char width;

	unsigned char height;

	unsigned char type;
};

struct codec_profile_t {

	char *name;		/* video codec short name */
	char *profile;		/* Attributes,separated by commas */
};

struct userdata_poc_info_t {

	unsigned int poc_info;

	unsigned int poc_number;
};

/*
******************************************************************
* 0x100~~0x1FF : set cmd
* 0x200~~0x2FF : set ex cmd
* 0x300~~0x3FF : set ptr cmd
* 0x400~~0x7FF : set reserved cmd
* 0x800~~0x8FF : get cmd
* 0x900~~0x9FF : get ex cmd
* 0xA00~~0xAFF : get ptr cmd
* 0xBFF~~0xFFF : get reserved cmd
* 0xX00~~0xX5F : amstream cmd(X: cmd type)
* 0xX60~~0xXAF : video cmd(X: cmd type)
* 0xXAF~~0xXFF : reserved cmd(X: cmd type)
******************************************************************
*/

/*  amstream set cmd */
#define AMSTREAM_SET_VB_START 0x101
#define AMSTREAM_SET_VB_SIZE 0x102
#define AMSTREAM_SET_AB_START 0x103
#define AMSTREAM_SET_AB_SIZE 0x104
#define AMSTREAM_SET_VFORMAT 0x105
#define AMSTREAM_SET_AFORMAT 0x106
#define AMSTREAM_SET_VID 0x107
#define AMSTREAM_SET_AID 0x108
#define AMSTREAM_SET_SID 0x109
#define AMSTREAM_SET_PCRID 0x10A
#define AMSTREAM_SET_ACHANNEL 0x10B
#define AMSTREAM_SET_SAMPLERATE 0x10C
#define AMSTREAM_SET_DATAWIDTH 0x10D
#define AMSTREAM_SET_TSTAMP 0x10E
#define AMSTREAM_SET_TSTAMP_US64 0x10F
#define AMSTREAM_SET_APTS 0x110
#define AMSTREAM_PORT_INIT 0x111
#define AMSTREAM_SET_TRICKMODE 0x112	/* amstream,video */
#define AMSTREAM_AUDIO_RESET 0x013
#define AMSTREAM_SUB_RESET 0x114
#define AMSTREAM_DEC_RESET 0x115
#define AMSTREAM_SET_TS_SKIPBYTE 0x116
#define AMSTREAM_SET_SUB_TYPE 0x117
#define AMSTREAM_SET_PCRSCR 0x118
#define AMSTREAM_SET_DEMUX 0x119
#define AMSTREAM_SET_VIDEO_DELAY_LIMIT_MS 0x11A
#define AMSTREAM_SET_AUDIO_DELAY_LIMIT_MS 0x11B
#define AMSTREAM_SET_DRMMODE 0x11C

/*  video set   cmd */
#define AMSTREAM_SET_OMX_VPTS 0x160
#define AMSTREAM_SET_VPAUSE 0x161
#define AMSTREAM_SET_AVTHRESH 0x162
#define AMSTREAM_SET_SYNCTHRESH 0x163
#define AMSTREAM_SET_SYNCENABLE 0x164
#define AMSTREAM_SET_SYNC_ADISCON 0x165
#define AMSTREAM_SET_SYNC_VDISCON 0x166
#define AMSTREAM_SET_SYNC_ADISCON_DIFF 0x167
#define AMSTREAM_SET_SYNC_VDISCON_DIFF 0x168
#define AMSTREAM_SET_VIDEO_DISABLE 0x169
#define AMSTREAM_SET_VIDEO_DISCONTINUE_REPORT 0x16A
#define AMSTREAM_SET_SCREEN_MODE 0x16B
#define AMSTREAM_SET_BLACKOUT_POLICY 0x16C
#define AMSTREAM_CLEAR_VBUF 0x16D
#define AMSTREAM_SET_CLEAR_VIDEO 0x16E
#define AMSTREAM_SET_FREERUN_MODE 0x16F
#define AMSTREAM_SET_DISABLE_SLOW_SYNC 0x170
#define AMSTREAM_SET_3D_TYPE 0x171
#define AMSTREAM_SET_VSYNC_UPINT 0x172
#define AMSTREAM_SET_VSYNC_SLOW_FACTOR 0x173
#define AMSTREAM_SET_FRAME_BASE_PATH 0x174
#define AMSTREAM_SET_EOS 0x175
#define AMSTREAM_SET_RECEIVE_ID 0x176

/*  video set ex cmd */
#define AMSTREAM_SET_EX_VIDEO_AXIS 0x260
#define AMSTREAM_SET_EX_VIDEO_CROP 0x261

/*  amstream set ptr cmd */
#define AMSTREAM_SET_PTR_AUDIO_INFO 0x300
#define AMSTREAM_SET_PTR_CONFIGS 0x301

/*  amstream get cmd */
#define AMSTREAM_GET_SUB_LENGTH 0x800
#define AMSTREAM_GET_UD_LENGTH 0x801
#define AMSTREAM_GET_APTS_LOOKUP 0x802
#define AMSTREAM_GET_FIRST_APTS_FLAG 0x803
#define AMSTREAM_GET_APTS 0x804
#define AMSTREAM_GET_VPTS 0x805
#define AMSTREAM_GET_PCRSCR 0x806
#define AMSTREAM_GET_LAST_CHECKIN_APTS 0x807
#define AMSTREAM_GET_LAST_CHECKIN_VPTS 0x808
#define AMSTREAM_GET_LAST_CHECKOUT_APTS 0x809
#define AMSTREAM_GET_LAST_CHECKOUT_VPTS 0x80A
#define AMSTREAM_GET_SUB_NUM 0x80B
#define AMSTREAM_GET_VIDEO_DELAY_LIMIT_MS 0x80C
#define AMSTREAM_GET_AUDIO_DELAY_LIMIT_MS 0x80D
#define AMSTREAM_GET_AUDIO_CUR_DELAY_MS 0x80E
#define AMSTREAM_GET_VIDEO_CUR_DELAY_MS 0x80F
#define AMSTREAM_GET_AUDIO_AVG_BITRATE_BPS 0x810
#define AMSTREAM_GET_VIDEO_AVG_BITRATE_BPS 0x811
#define AMSTREAM_GET_ION_ID 0x812
#define AMSTREAM_GET_NEED_MORE_DATA 0x813
/*  video get cmd */
#define AMSTREAM_GET_OMX_VPTS 0x860
#define AMSTREAM_GET_TRICK_STAT 0x861
#define AMSTREAM_GET_TRICK_VPTS 0x862
#define AMSTREAM_GET_SYNC_ADISCON 0x863
#define AMSTREAM_GET_SYNC_VDISCON 0x864
#define AMSTREAM_GET_SYNC_ADISCON_DIFF 0x865
#define AMSTREAM_GET_SYNC_VDISCON_DIFF 0x866
#define AMSTREAM_GET_VIDEO_DISABLE 0x867
#define AMSTREAM_GET_VIDEO_DISCONTINUE_REPORT 0x868
#define AMSTREAM_GET_SCREEN_MODE 0x869
#define AMSTREAM_GET_BLACKOUT_POLICY 0x86A
#define AMSTREAM_GET_FREERUN_MODE 0x86B
#define AMSTREAM_GET_3D_TYPE 0x86C
#define AMSTREAM_GET_VSYNC_SLOW_FACTOR 0x86D

/*  amstream get ex cmd */
#define AMSTREAM_GET_EX_VB_STATUS 0x900
#define AMSTREAM_GET_EX_AB_STATUS 0x901
#define AMSTREAM_GET_EX_VDECSTAT 0x902
#define AMSTREAM_GET_EX_ADECSTAT 0x903
#define AMSTREAM_GET_EX_UD_POC 0x904

/*  video get ex cmd */
#define AMSTREAM_GET_EX_VF_STATUS 0x960
#define AMSTREAM_GET_EX_VIDEO_AXIS 0x961
#define AMSTREAM_GET_EX_VIDEO_CROP 0x962

/*  amstream get ptr cmd */
#define AMSTREAM_GET_PTR_SUB_INFO 0xA00

#define AMSTREAM_IOC_VERSION_FIRST 2
#define AMSTREAM_IOC_VERSION_SECOND 0

struct am_ioctl_parm {
	union {
		u32 data_32;
		u64 data_64;
		enum vformat_e data_vformat;
		enum aformat_e data_aformat;
		enum FRAME_BASE_VIDEO_PATH frame_base_video_path;
		char data[8];
	};
	u32 cmd;
	char reserved[4];
};

struct am_ioctl_parm_ex {
	union {
		struct buf_status status;
		struct vdec_status vstatus;
		struct adec_status astatus;

		struct userdata_poc_info_t data_userdata_info;
		char data[24];

	};
	u32 cmd;
	char reserved[4];
};

struct am_ioctl_parm_ptr {
	union {
		struct audio_info *pdata_audio_info;
		struct subtitle_info *pdata_sub_info;
		void *pointer;
		char data[8];
	};
	u32 cmd;
	u32 len; /*char reserved[4]; */
};

#define SUPPORT_VDEC_NUM	(20)
int vcodec_profile_register(const struct codec_profile_t *vdec_profile);
ssize_t vcodec_profile_read(char *buf);

#ifdef __KERNEL__
#include <linux/interrupt.h>

/*TS demux operation interface*/
struct tsdemux_ops {

	int (*reset)(void);

	int (*set_reset_flag)(void);

	int (*request_irq)(irq_handler_t handler, void *data);

	int (*free_irq)(void);

	int (*set_vid)(int vpid);

	int (*set_aid)(int apid);

	int (*set_sid)(int spid);

	int (*set_pcrid)(int pcrpid);

	int (*set_skipbyte)(int skipbyte);

	int (*set_demux)(int dev);
};

void tsdemux_set_ops(struct tsdemux_ops *ops);
int tsdemux_set_reset_flag(void);

void set_adec_func(int (*adec_func)(struct adec_status *));
void wakeup_sub_poll(void);
void init_userdata_fifo(void);
void reset_userdata_fifo(int bInit);
int wakeup_userdata_poll(struct userdata_poc_info_t poc,
			int wp, unsigned long start_phyaddr,
			int buf_size, int data_length);
int get_sub_type(void);

#endif				/**/

struct tcon_gamma_table_s {

	u16 data[256];
} /*tcon_gamma_table_t */;

struct tcon_rgb_ogo_s {

	unsigned int en;

	int r_pre_offset;	/* s11.0, range -1024~+1023, default is 0 */
	int g_pre_offset;	/* s11.0, range -1024~+1023, default is 0 */
	int b_pre_offset;	/* s11.0, range -1024~+1023, default is 0 */
	unsigned int r_gain;   /* u1.10, range 0~2047, default is 1024 (1.0x) */
	unsigned int g_gain;   /* u1.10, range 0~2047, default is 1024 (1.0x) */
	unsigned int b_gain;   /* u1.10, range 0~2047, default is 1024 (1.0x) */
	int r_post_offset;	/* s11.0, range -1024~+1023, default is 0 */
	int g_post_offset;	/* s11.0, range -1024~+1023, default is 0 */
	int b_post_offset;	/* s11.0, range -1024~+1023, default is 0 */
} /*tcon_rgb_ogo_t */;

#endif /* AMSTREAM_H */
