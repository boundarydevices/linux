/*
 * drivers/amlogic/media/vout/cvbs/wss.h
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

#ifndef __WSS_H__
#define __WSS_H__

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* for 576i, according to <ETSI EN 300294 V1.4.1> */

#define WSS_576I_LINE							23

#define WSS_576I_AR_START						0x0
#define WSS_576I_AR_LENGTH						0x4
#define WSS_576I_AR_MASK						0x7
	#define WSS_576I_BITS_AR_FULL_43				0x1
	#define WSS_576I_BITS_AR_BOX_149_CENTRE		0x8
	#define WSS_576I_BITS_AR_BOX_149_TOP			0x4
	#define WSS_576I_BITS_AR_BOX_169_CENTRE		0xd
	#define WSS_576I_BITS_AR_BOX_169_TOP			0x2
	#define WSS_576I_BITS_AR_BOX_OVER_169_CENTRE	0xb
	#define WSS_576I_BITS_AR_FULL_43_SHOOT			0x7
	#define WSS_576I_BITS_AR_FULL_169				0xe


#define WSS_576I_MODE_START						0x4
#define WSS_576I_MODE_LENGTH					0x1
#define WSS_576I_MODE_MASK						0x1
	#define WSS_576I_BITS_MODE_CAMERA				0x0
	#define WSS_576I_BITS_MODE_FILM					0x1

#define WSS_576I_CODING_START					0x5
#define WSS_576I_CODING_LENGTH					0x1
#define WSS_576I_CODING_MASK					0x1
	#define WSS_576I_BITS_STANDARD_CODING			0x0
	#define WSS_576I_BITS_MOTION_COLOUR_PLUS		0x1

#define WSS_576I_HELPER_START					0x6
#define WSS_576I_HELPER_LENGTH					0x1
#define WSS_576I_HELPER_MASK					0x1
	#define WSS_576I_BITS_NO_HELPER					0x0
	#define WSS_576I_BITS_MODULATED_HELPER			0x1

#define WSS_576I_TTX_SUBT_START					0x8
#define WSS_576I_TTX_SUBT_LENGTH				0x1
#define WSS_576I_TTX_SUBT_MASK					0x1
	#define WSS_576I_BITS_TTX_NO_SUBT				0x0
	#define WSS_576I_BITS_TTX_SUBT					0x1

#define WSS_576I_OPEN_SUBT_START				0x9
#define WSS_576I_OPEN_SUBT_LENGTH				0x2
#define WSS_576I_OPEN_SUBT_MASK					0x3
	#define WSS_576I_BITS_NO_OPEN_SUBT				0x0
	#define WSS_576I_BITS_SUBT_OUT_AREA				0x1
	#define WSS_576I_BITS_SUBT_IN_AREA				0x2

#define WSS_576I_SURROUND_SND_START				0xb
#define WSS_576I_SURROUND_SND_LENGTH			0x1
#define WSS_576I_SURROUND_SND_MASK				0x1
	#define WSS_576I_BITS_NO_SURR_SOUND				0x0
	#define WSS_576I_BITS_SURR_SOUND				0x1

#define WSS_576I_CGMS_A_START					0xc
#define WSS_576I_CGMS_A_LENGTH					0x2
#define WSS_576I_CGMS_A_MASK					0x3
	#define WSS_576I_BITS_NO_COPYRIGHT				0x0
	#define WSS_576I_BITS_HAS_COPYRIGHT				0x1
	#define WSS_576I_BITS_COPYRIGHT_NOT_RESTRICTED	0x0
	#define WSS_576I_BITS_COPYRIGHT_RESTRICTED		0x1

#define WSS_576I_FULL_START						0x0
#define WSS_576I_FULL_LENGTH					0xe
#define WSS_576I_FULL_MASK						0x3fff

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* for 480i, according to <IEC 61880:1998> */

#define WSS_480I_LINE							20

#define WSS_480I_AR_START						0x0
#define WSS_480I_AR_LENGTH						0x2
#define WSS_480I_AR_MASK						0x3
#define WSS_480I_AR_BITS_43						0x0
#define WSS_480I_AR_BITS_43_LB					0x1
#define WSS_480I_AR_BITS_169					0x2
#define WSS_480I_AR_BITS_RESERVED				0x3

#define WSS_480I_CGMS_A_START					0x6
#define WSS_480I_CGMS_A_LENGTH					0x2
#define WSS_480I_CGMS_A_MASK					0x3
#define	WSS_480I_CGMS_A_PERMITTED				0x0
#define WSS_480I_CGMS_A_NOT_USED				0x1
#define WSS_480I_CGMS_A_ONE_COPY				0x2
#define WSS_480I_CGMS_A_NO_COPY					0x3

#define WSS_480I_PSP_START						0x9
#define WSS_480I_PSP_LENGTH						0x2
#define WSS_480I_PSP_MASK						0x3
#define WSS_480I_PSP_OFF						0x0
#define WSS_480I_PSP_SPLIT_OFF					0x1
#define WSS_480I_PSP_SPLIT_2LINE				0x2
#define WSS_480I_PSP_SPLIT_4LINE				0x3

#define WSS_480I_PRE_RECORDED_START				0xb
#define WSS_480I_PRE_RECORDED_LENGTH			0x1
#define WSS_480I_PRE_RECORDED_MASK				0x1
#define WSS_480I_PRE_RECORDED_NO				0x0
#define WSS_480I_PRE_RECORDED_YES				0x1

#define WSS_480I_FULL_START						0x0
#define WSS_480I_FULL_LENGTH					0xc
#define WSS_480I_FULL_MASK						0x7ff

#define WSS_576I_CC_START                       0x0
#define WSS_576I_CC_LINE						0x1615
/*line evn 21,odd 22*/
#define WSS_576I_CC_LENGTH						0x2
#define WSS_576I_CC_MASK                        0xffff

#define WSS_480I_CC_START                       0x0
#define WSS_480I_CC_LINE						0x1211
/*line evn 17,odd 18,reault all 21*/
#define WSS_480I_CC_LENGTH						0x2
#define WSS_480I_CC_MASK                        0xffff
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* common struct for 480i/576i wss */

enum WSS_576I_CMD {
	WSS_576I_CMD_AR = 0x0,
	WSS_576I_CMD_MODE = 0x1,
	WSS_576I_CMD_CODING = 0x2,
	WSS_576I_CMD_HELPER = 0x3,
	WSS_576I_CMD_TTX_SUBT = 0x4,
	WSS_576I_CMD_OPEN_SUBT = 0x5,
	WSS_576I_CMD_SURROUND_SND = 0x6,
	WSS_576I_CMD_CGMS_A = 0x7,
	WSS_576I_CMD_FULL = 0x8,
	WSS_576I_CMD_CC = 0x9,
	WSS_576I_CMD_OFF = 0xa,

	WSS_480I_CMD_AR = 0x10,
	WSS_480I_CMD_CGMS_A = 0x11,
	WSS_480I_CMD_PSP = 0x12,
	WSS_480I_CMD_PRE_RECORDED = 0x13,
	WSS_480I_CMD_CC = 0x14,
	WSS_480I_CMD_OFF = 0x15,

};

struct wss_info_t {
	unsigned int	wss_cmd;
	unsigned int	wss_line;
	unsigned int	start;
	unsigned int	length;
	unsigned int	mask;
	char			*description;
};

ssize_t aml_CVBS_attr_wss_show(struct class *class,
			struct class_attribute *attr, char *buf);
ssize_t  aml_CVBS_attr_wss_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#endif
