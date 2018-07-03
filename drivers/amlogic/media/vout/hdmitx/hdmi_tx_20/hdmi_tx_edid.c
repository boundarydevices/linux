/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hdmi_tx_edid.c
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
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <crypto/hash.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/delay.h>

#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include "hw/common.h"

#define CEA_DATA_BLOCK_COLLECTION_ADDR_1StP 0x04
#define VIDEO_TAG 0x40
#define AUDIO_TAG 0x20
#define VENDOR_TAG 0x60
#define SPEAKER_TAG 0x80

#define HDMI_EDID_BLOCK_TYPE_RESERVED	        0
#define HDMI_EDID_BLOCK_TYPE_AUDIO		1
#define HDMI_EDID_BLOCK_TYPE_VIDEO		2
#define HDMI_EDID_BLOCK_TYPE_VENDER	        3
#define HDMI_EDID_BLOCK_TYPE_SPEAKER	        4
#define HDMI_EDID_BLOCK_TYPE_VESA		5
#define HDMI_EDID_BLOCK_TYPE_RESERVED2	        6
#define HDMI_EDID_BLOCK_TYPE_EXTENDED_TAG       7

#define EXTENSION_VENDOR_SPECIFIC 0x1
#define EXTENSION_COLORMETRY_TAG 0x5
/* DRM stands for "Dynamic Range and Mastering " */
#define EXTENSION_DRM_TAG	0x6
/* Video Format Preference Data block */
#define EXTENSION_VFPDB_TAG	0xd
#define EXTENSION_Y420_VDB_TAG	0xe
#define EXTENSION_Y420_CMDB_TAG	0xf

#define EDID_DETAILED_TIMING_DES_BLOCK0_POS 0x36
#define EDID_DETAILED_TIMING_DES_BLOCK1_POS 0x48
#define EDID_DETAILED_TIMING_DES_BLOCK2_POS 0x5A
#define EDID_DETAILED_TIMING_DES_BLOCK3_POS 0x6C

/* EDID Descrptor Tag */
#define TAG_PRODUCT_SERIAL_NUMBER 0xFF
#define TAG_ALPHA_DATA_STRING 0xFE
#define TAG_RANGE_LIMITS 0xFD
#define TAG_DISPLAY_PRODUCT_NAME_STRING 0xFC /* MONITOR NAME */
#define TAG_COLOR_POINT_DATA 0xFB
#define TAG_STANDARD_TIMINGS 0xFA
#define TAG_DISPLAY_COLOR_MANAGEMENT 0xF9
#define TAG_CVT_TIMING_CODES 0xF8
#define TAG_ESTABLISHED_TIMING_III 0xF7
#define TAG_DUMMY_DES 0x10

static unsigned char __nosavedata edid_checkvalue[4] = {0};
static unsigned int hdmitx_edid_check_valid_blocks(unsigned char *buf);
static void Edid_DTD_parsing(struct rx_cap *pRXCap, unsigned char *data);
static void hdmitx_edid_set_default_aud(struct hdmitx_dev *hdev);

static void edid_save_checkvalue(unsigned char *buf, unsigned int block_cnt)
{
	unsigned int i, length, max;

	if (buf == NULL)
		return;

	length = sizeof(edid_checkvalue);
	memset(edid_checkvalue, 0x00, length);

	max = (block_cnt > length)?length:block_cnt;

	for (i = 0; i < max; i++)
		edid_checkvalue[i] = *(buf+(i+1)*128-1);
}

static int Edid_DecodeHeader(struct hdmitx_info *info, unsigned char *buff)
{
	int i, ret = 0;

	if (!(buff[0] | buff[7])) {
		for (i = 1; i < 7; i++) {
			if (buff[i] != 0xFF)
				ret = -1;
		}
	} else
		ret = -1;
	return ret;
}

static void Edid_ParsingIDManufacturerName(struct rx_cap *pRxCap,
		unsigned char *data)
{
	int i;
	unsigned char uppercase[26] = { 0 };
	unsigned char brand[3];

	/* Fill array uppercase with 'A' to 'Z' */
	for (i = 0; i < 26; i++)
		uppercase[i] = 'A' + i;

	brand[0] = data[0] >> 2;
	brand[1] = ((data[0] & 0x3) << 3) + (data[1] >> 5);
	brand[2] = data[1] & 0x1f;

	for (i = 0; i < 3; i++)
		pRxCap->IDManufacturerName[i] = uppercase[brand[i] - 1];
}

static void Edid_ParsingIDProductCode(struct rx_cap *pRXCap,
		unsigned char *data)
{
	if (data == NULL)
		return;
	pRXCap->IDProductCode[0] = data[1];
	pRXCap->IDProductCode[1] = data[0];
}

static void Edid_ParsingIDSerialNumber(struct rx_cap *pRXCap,
		unsigned char *data)
{
	int i;

	if (data != NULL)
		for (i = 0; i < 4; i++)
			pRXCap->IDSerialNumber[i] = data[3-i];
}

static int Edid_find_name_block(unsigned char *data)
{
	int ret = 0;
	int i;

	for (i = 0; i < 3; i++) {
		if (data[i])
			return ret;
	}
	if (data[3] == 0xfc)
		ret = 1;
	return ret;
}

static void Edid_ReceiverProductNameParse(struct rx_cap *pRxCap,
	unsigned char *data)
{
	int i = 0;
	/* some Display Product name end with 0x20, not 0x0a
	 */
	while ((data[i] != 0x0a) && (data[i] != 0x20) && (i < 13)) {
		pRxCap->ReceiverProductName[i] = data[i];
		i++;
	}
	pRxCap->ReceiverProductName[i] = '\0';
}

void Edid_DecodeStandardTiming(struct hdmitx_info *info,
	unsigned char *Data, unsigned char length)
{
	 unsigned char  i, TmpVal;
	 int hor_pixel, frame_rate;

	for (i = 0; i < length; i++) {
		if ((Data[i*2] != 0x01) && (Data[i*2 + 1] != 0x01)) {
			hor_pixel = (int)((Data[i*2]+31)*8);
			TmpVal = Data[i*2 + 1] & 0xC0;

			frame_rate = (int)((Data[i*2 + 1]) & 0x3F) + 60;

			if ((hor_pixel == 720) && (frame_rate == 60))
				info->hdmi_sup_480p  = 1;
			else if ((hor_pixel == 1280) && (frame_rate == 60))
				info->hdmi_sup_720p_60hz  = 1;
			else if ((hor_pixel == 1920) && (frame_rate == 60))
				info->hdmi_sup_1080p_60hz  = 1;
		}
	}
}

/* ----------------------------------------------------------- */
void Edid_ParseCEADetailedTimingDescriptors(struct hdmitx_info *info,
	unsigned char blk_mun, unsigned char BaseAddr,
	unsigned char *buff)
{
	unsigned char index_edid;

	for (index_edid = 0; index_edid < blk_mun; index_edid++) {
		BaseAddr += 18;
		/* there is not the TimingDescriptors */
		if ((BaseAddr + 18) > 0x7d)
			break;
	}
}

static struct vsdb_phyaddr vsdb_local = {0};
int get_vsdb_phy_addr(struct vsdb_phyaddr *vsdb)
{
	vsdb = &vsdb_local;
	return vsdb->valid;
}

static void set_vsdb_phy_addr(struct hdmitx_dev *hdev,
			      struct vsdb_phyaddr *vsdb,
			      unsigned char *edid_offset)
{
	int phy_addr;

	vsdb->a = (edid_offset[4] >> 4) & 0xf;
	vsdb->b = (edid_offset[4] >> 0) & 0xf;
	vsdb->c = (edid_offset[5] >> 4) & 0xf;
	vsdb->d = (edid_offset[5] >> 0) & 0xf;
	vsdb_local = *vsdb;
	vsdb->valid = 1;

	phy_addr = ((vsdb->a & 0xf) << 12) |
		   ((vsdb->b & 0xf) <<  8) |
		   ((vsdb->c & 0xf) <<  4) |
		   ((vsdb->d & 0xf) <<  0);
	hdev->physical_addr = phy_addr;
	hdmitx_event_notify(HDMITX_PHY_ADDR_VALID, &phy_addr);
}

static void set_vsdb_dc_cap(struct rx_cap *pRXCap)
{
	pRXCap->dc_y444 = !!(pRXCap->ColorDeepSupport & (1 << 3));
	pRXCap->dc_30bit = !!(pRXCap->ColorDeepSupport & (1 << 4));
	pRXCap->dc_36bit = !!(pRXCap->ColorDeepSupport & (1 << 5));
	pRXCap->dc_48bit = !!(pRXCap->ColorDeepSupport & (1 << 6));
}

static void set_vsdb_dc_420_cap(struct rx_cap *pRXCap,
	unsigned char *edid_offset)
{
	pRXCap->dc_30bit_420 = !!(edid_offset[6] & (1 << 0));
	pRXCap->dc_36bit_420 = !!(edid_offset[6] & (1 << 1));
	pRXCap->dc_48bit_420 = !!(edid_offset[6] & (1 << 2));
}

/* Special FBC check */
static int check_fbc_special(unsigned char *edid_dat)
{
	if ((edid_dat[250] == 0xfb) && (edid_dat[251] == 0x0c))
		return 1;
	else
		return 0;
}

int Edid_Parse_check_HDMI_VSDB(struct hdmitx_dev *hdev,
	unsigned char *buff)
{
	int ret = 0;
	struct hdmitx_info *info = &(hdev->hdmi_info);
	unsigned char  VSpecificBoundary, BlockAddr, len;
	int temp_addr = 0;

	VSpecificBoundary = buff[2];

	if (VSpecificBoundary < 4)
		ret = -1;
	BlockAddr = CEA_DATA_BLOCK_COLLECTION_ADDR_1StP;
	while (BlockAddr < VSpecificBoundary) {
		len = buff[BlockAddr] & 0x1F;
		if ((buff[BlockAddr] & 0xE0) == VENDOR_TAG)
			break;
		temp_addr = BlockAddr + len + 1;
		if (temp_addr >= VSpecificBoundary)
			break;
		BlockAddr = BlockAddr + len + 1;
	}

	set_vsdb_phy_addr(hdev, &info->vsdb_phy_addr, &buff[BlockAddr]);
	if (hdev->repeater_tx) {
		if ((check_fbc_special(&hdev->EDID_buf[0])) ||
		    (check_fbc_special(&hdev->EDID_buf1[0])))
			rx_edid_physical_addr(0, 0, 0, 0);
		else
			rx_edid_physical_addr(info->vsdb_phy_addr.a,
				info->vsdb_phy_addr.b,
				info->vsdb_phy_addr.c,
				info->vsdb_phy_addr.d);
	}

	if (temp_addr >= VSpecificBoundary)
		ret = -1;
	else {
		if ((buff[BlockAddr + 1] != 0x03) ||
			(buff[BlockAddr + 2] != 0x0C) ||
			(buff[BlockAddr + 3] != 0x0))
			ret = -1;
	}
	return ret;
}

/* ----------------------------------------------------------- */
void Edid_MonitorCapable861(struct hdmitx_info *info,
	unsigned char edid_flag)
{
	if (edid_flag & 0x80)
		info->support_underscan_flag = 1;
	if (edid_flag & 0x40) {
		struct hdmitx_dev *hdev =
			container_of(info, struct hdmitx_dev, hdmi_info);
		info->support_basic_audio_flag = 1;
		hdmitx_edid_set_default_aud(hdev);
	}
	if (edid_flag & 0x20)
		info->support_ycbcr444_flag = 1;
	if (edid_flag & 0x10)
		info->support_ycbcr422_flag = 1;
}


/* ----------------------------------------------------------- */
static void Edid_ParsingVideoDATABlock(struct hdmitx_info *info,
	unsigned char *buff, unsigned char BaseAddr,
	unsigned char NBytes)
{
	unsigned char i;

	NBytes &= 0x1F;
	for (i = 0; i < NBytes; i++) {
		switch (buff[i + BaseAddr]&0x7F) {
		case 6:
		case 7:
			info->hdmi_sup_480i  = 1;
			break;
		case 21:
		case 22:
			info->hdmi_sup_576i  = 1;
			break;
		case 2:
		case 3:
			info->hdmi_sup_480p  = 1;
			break;
		case 17:
		case 18:
			info->hdmi_sup_576p  = 1;
			break;
		case 4:
			info->hdmi_sup_720p_60hz  = 1;
			break;
		case 19:
			info->hdmi_sup_720p_50hz  = 1;
			break;
		case 5:
			info->hdmi_sup_1080i_60hz  = 1;
			break;
		case 20:
			info->hdmi_sup_1080i_50hz  = 1;
			break;
		case 16:
			info->hdmi_sup_1080p_60hz  = 1;
			break;
		case 31:
			info->hdmi_sup_1080p_50hz  = 1;
			break;
		case 32:
			info->hdmi_sup_1080p_24hz  = 1;
			break;
		case 33:
			info->hdmi_sup_1080p_25hz  = 1;
			break;
		case 34:
			info->hdmi_sup_1080p_30hz  = 1;
			break;
		default:
			break;
		}
	}
}

/* ----------------------------------------------------------- */
static void Edid_ParsingAudioDATABlock(struct hdmitx_info *info,
	unsigned char *Data, unsigned char BaseAddr,
	unsigned char NBytes)
{
	 unsigned char AudioFormatCode;
	 int i = BaseAddr;
	NBytes &= 0x1F;
	do {
		AudioFormatCode = (Data[i]&0xF8)>>3;
		switch (AudioFormatCode) {
		case 1:
			info->tv_audio_info._60958_PCM.support_flag = 1;
			info->tv_audio_info._60958_PCM.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._60958_PCM._192k =
					1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._60958_PCM._176k =
					1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._60958_PCM._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._60958_PCM._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._60958_PCM._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._60958_PCM._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._60958_PCM._32k = 1;
			if ((Data[i+2]&0x04))
				info->tv_audio_info._60958_PCM._24bit = 1;
			if ((Data[i+2]&0x02))
				info->tv_audio_info._60958_PCM._20bit = 1;
			if ((Data[i+2]&0x01))
				info->tv_audio_info._60958_PCM._16bit = 1;
			break;
		case 2:
			info->tv_audio_info._AC3.support_flag = 1;
			info->tv_audio_info._AC3.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._AC3._192k = 1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._AC3._176k = 1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._AC3._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._AC3._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._AC3._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._AC3._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._AC3._32k = 1;
			info->tv_audio_info._AC3._max_bit =
				Data[i+2];
			break;
		case 3:
			info->tv_audio_info._MPEG1.support_flag = 1;
			info->tv_audio_info._MPEG1.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._MPEG1._192k = 1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._MPEG1._176k = 1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._MPEG1._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._MPEG1._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._MPEG1._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._MPEG1._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._MPEG1._32k = 1;
			info->tv_audio_info._MPEG1._max_bit =
				Data[i+2];
			break;
		case 4:
			info->tv_audio_info._MP3.support_flag = 1;
			info->tv_audio_info._MP3.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._MP3._192k = 1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._MP3._176k = 1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._MP3._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._MP3._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._MP3._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._MP3._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._MP3._32k = 1;
			info->tv_audio_info._MP3._max_bit = Data[i+2];
			break;
		case 5:
			info->tv_audio_info._MPEG2.support_flag = 1;
			info->tv_audio_info._MPEG2.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._MPEG2._192k = 1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._MPEG2._176k = 1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._MPEG2._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._MPEG2._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._MPEG2._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._MPEG2._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._MPEG2._32k = 1;
			info->tv_audio_info._MPEG2._max_bit = Data[i+2];
			break;
		case 6:
			info->tv_audio_info._AAC.support_flag = 1;
			info->tv_audio_info._AAC.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._AAC._192k = 1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._AAC._176k = 1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._AAC._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._AAC._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._AAC._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._AAC._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._AAC._32k = 1;
			info->tv_audio_info._AAC._max_bit = Data[i+2];
			break;
		case 7:
			info->tv_audio_info._DTS.support_flag = 1;
			info->tv_audio_info._DTS.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._DTS._192k = 1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._DTS._176k = 1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._DTS._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._DTS._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._DTS._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._DTS._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._DTS._32k = 1;
			info->tv_audio_info._DTS._max_bit = Data[i+2];
			break;
		case 8:
			info->tv_audio_info._ATRAC.support_flag = 1;
			info->tv_audio_info._ATRAC.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._ATRAC._192k = 1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._ATRAC._176k = 1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._ATRAC._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._ATRAC._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._ATRAC._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._ATRAC._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._ATRAC._32k = 1;
			info->tv_audio_info._ATRAC._max_bit = Data[i+2];
			break;
		case 9:
			info->tv_audio_info._One_Bit_Audio.support_flag = 1;
			info->tv_audio_info._One_Bit_Audio.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._One_Bit_Audio._192k = 1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._One_Bit_Audio._176k = 1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._One_Bit_Audio._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._One_Bit_Audio._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._One_Bit_Audio._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._One_Bit_Audio._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._One_Bit_Audio._32k = 1;
			info->tv_audio_info._One_Bit_Audio._max_bit =
				Data[i+2];
			break;
		case 10:
			info->tv_audio_info._Dolby.support_flag = 1;
			info->tv_audio_info._Dolby.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._Dolby._192k = 1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._Dolby._176k = 1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._Dolby._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._Dolby._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._Dolby._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._Dolby._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._Dolby._32k = 1;
			info->tv_audio_info._Dolby._max_bit = Data[i+2];
			break;

		case 11:
			info->tv_audio_info._DTS_HD.support_flag = 1;
			info->tv_audio_info._DTS_HD.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._DTS_HD._192k = 1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._DTS_HD._176k = 1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._DTS_HD._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._DTS_HD._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._DTS_HD._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._DTS_HD._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._DTS_HD._32k = 1;
			info->tv_audio_info._DTS_HD._max_bit =
				Data[i+2];
			break;
		case 12:
			info->tv_audio_info._MAT.support_flag = 1;
			info->tv_audio_info._MAT.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._MAT._192k = 1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._MAT._176k = 1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._MAT._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._MAT._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._MAT._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._MAT._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._MAT._32k = 1;
			info->tv_audio_info._MAT._max_bit = Data[i+2];
			break;

		case 13:
			info->tv_audio_info._ATRAC.support_flag = 1;
			info->tv_audio_info._ATRAC.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._DST._192k = 1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._DST._176k = 1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._DST._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._DST._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._DST._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._DST._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._DST._32k = 1;
			info->tv_audio_info._DST._max_bit = Data[i+2];
			break;

		case 14:
			info->tv_audio_info._WMA.support_flag = 1;
			info->tv_audio_info._WMA.max_channel_num =
				(Data[i]&0x07);
			if ((Data[i+1]&0x40))
				info->tv_audio_info._WMA._192k = 1;
			if ((Data[i+1]&0x20))
				info->tv_audio_info._WMA._176k = 1;
			if ((Data[i+1]&0x10))
				info->tv_audio_info._WMA._96k = 1;
			if ((Data[i+1]&0x08))
				info->tv_audio_info._WMA._88k = 1;
			if ((Data[i+1]&0x04))
				info->tv_audio_info._WMA._48k = 1;
			if ((Data[i+1]&0x02))
				info->tv_audio_info._WMA._44k = 1;
			if ((Data[i+1]&0x01))
				info->tv_audio_info._WMA._32k = 1;
			info->tv_audio_info._WMA._max_bit = Data[i+2];
			break;

		default:
		break;
		}
	i += 3;
	} while (i < (NBytes + BaseAddr));
}

/* ----------------------------------------------------------- */
static void Edid_ParsingSpeakerDATABlock(struct hdmitx_info *info,
	unsigned char *buff, unsigned char BaseAddr)
{
	int ii;

	for (ii = 1; ii < 0x80; ) {
		switch (buff[BaseAddr] & ii) {
		case 0x40:
			info->tv_audio_info.speaker_allocation.rlc_rrc = 1;
			break;

		case 0x20:
			info->tv_audio_info.speaker_allocation.flc_frc = 1;
			break;

		case 0x10:
			info->tv_audio_info.speaker_allocation.rc = 1;
			break;

		case 0x08:
			info->tv_audio_info.speaker_allocation.rl_rr = 1;
			break;

		case 0x04:
			info->tv_audio_info.speaker_allocation.fc = 1;
			break;

		case 0x02:
			info->tv_audio_info.speaker_allocation.lfe = 1;
			break;

		case 0x01:
			info->tv_audio_info.speaker_allocation.fl_fr = 1;
			break;

		default:
		break;
		}
		ii = ii << 1;
	}
}

static void Edid_ParsingVendSpec(struct rx_cap *pRXCap,
	unsigned char *buf)
{
	struct dv_info *dv = &pRXCap->dv_info;
	unsigned char *dat = buf;
	unsigned char pos = 0;

	memset(dv, 0, sizeof(struct dv_info));
	dv->block_flag = CORRECT;
	dv->length = dat[pos] & 0x1f;
	memcpy(dv->rawdata, dat, dv->length + 1);
	pos++;

	if (dat[pos] != 1) {
		pr_info("hdmitx: edid: parsing fail %s[%d]\n", __func__,
			__LINE__);
		return;
	}

	pos++;
	dv->ieeeoui = dat[pos++];
	dv->ieeeoui += dat[pos++] << 8;
	dv->ieeeoui += dat[pos++] << 16;
	if (dv->ieeeoui != DV_IEEE_OUI) {
		dv->block_flag = ERROR_LENGTH;
		return;
	}

	dv->ver = (dat[pos] >> 5) & 0x7;
	/* Refer to DV 2.9 Page 27 */
	if (dv->ver == 0) {
		if (dv->length == 0x19) {
			dv->sup_yuv422_12bit = dat[pos] & 0x1;
			dv->sup_2160p60hz = (dat[pos] >> 1) & 0x1;
			dv->sup_global_dimming = (dat[pos] >> 2) & 0x1;
			pos++;
			dv->Rx =
				(dat[pos+1] << 4) | (dat[pos] >> 4);
			dv->Ry =
				(dat[pos+2] << 4) | (dat[pos] & 0xf);
			pos += 3;
			dv->Gx =
				(dat[pos+1] << 4) | (dat[pos] >> 4);
			dv->Gy =
				(dat[pos+2] << 4) | (dat[pos] & 0xf);
			pos += 3;
			dv->Bx =
				(dat[pos+1] << 4) | (dat[pos] >> 4);
			dv->By =
				(dat[pos+2] << 4) | (dat[pos] & 0xf);
			pos += 3;
			dv->Wx =
				(dat[pos+1] << 4) | (dat[pos] >> 4);
			dv->Wy =
				(dat[pos+2] << 4) | (dat[pos] & 0xf);
			pos += 3;
			dv->tminPQ =
				(dat[pos+1] << 4) | (dat[pos] >> 4);
			dv->tmaxPQ =
				(dat[pos+2] << 4) | (dat[pos] & 0xf);
			pos += 3;
			dv->dm_major_ver = dat[pos] >> 4;
			dv->dm_minor_ver = dat[pos] & 0xf;
			pos++;
		} else
			dv->block_flag = ERROR_LENGTH;
	}

	if (dv->ver == 1) {
		if (dv->length == 0x0B) {/* Refer to DV 2.9 Page 33 */
			dv->dm_version = (dat[pos] >> 2) & 0x7;
			dv->sup_yuv422_12bit = dat[pos] & 0x1;
			dv->sup_2160p60hz = (dat[pos] >> 1) & 0x1;
			pos++;
			dv->sup_global_dimming = dat[pos] & 0x1;
			dv->tmaxLUM = dat[pos] >> 1;
			pos++;
			dv->colorimetry = dat[pos] & 0x1;
			dv->tminLUM = dat[pos] >> 1;
			pos++;
			dv->low_latency = dat[pos] & 0x3;
			dv->Bx = 0x20 | ((dat[pos] >> 5) & 0x7);
			dv->By = 0x08 | ((dat[pos] >> 2) & 0x7);
			pos++;
			dv->Gx = 0x00 | (dat[pos] >> 1);
			dv->Ry = 0x40 | ((dat[pos] & 0x1) |
				((dat[pos + 1] & 0x1) << 1) |
				((dat[pos + 2] & 0x3) << 2));
			pos++;
			dv->Gy = 0x80 | (dat[pos] >> 1);
			pos++;
			dv->Rx = 0xA0 | (dat[pos] >> 3);
			pos++;
		} else if (dv->length == 0x0E) {
			dv->dm_version = (dat[pos] >> 2) & 0x7;
			dv->sup_yuv422_12bit = dat[pos] & 0x1;
			dv->sup_2160p60hz = (dat[pos] >> 1) & 0x1;
			pos++;
			dv->sup_global_dimming = dat[pos] & 0x1;
			dv->tmaxLUM = dat[pos] >> 1;
			pos++;
			dv->colorimetry = dat[pos] & 0x1;
			dv->tminLUM = dat[pos] >> 1;
			pos += 2; /* byte8 is reserved as 0 */
			dv->Rx = dat[pos++];
			dv->Ry = dat[pos++];
			dv->Gx = dat[pos++];
			dv->Gy = dat[pos++];
			dv->Bx = dat[pos++];
			dv->By = dat[pos++];
		} else
			dv->block_flag = ERROR_LENGTH;
	}
	if (dv->ver == 2) {
		if (dv->length == 0x0B) {
			dv->sup_2160p60hz = 0x1;/*default*/
			dv->dm_version = (dat[pos] >> 2) & 0x7;
			dv->sup_yuv422_12bit = dat[pos] & 0x1;
			dv->sup_backlight_control = (dat[pos] >> 1) & 0x1;
			pos++;
			dv->sup_global_dimming = (dat[pos] >> 2) & 0x1;
			dv->backlt_min_luma = dat[pos] & 0x3;
			dv->tminPQ = dat[pos] >> 3;
			pos++;
			dv->Interface = dat[pos] & 0x3;
			dv->tmaxPQ = dat[pos] >> 3;
			pos++;
			dv->sup_10b_12b_444 = ((dat[pos] & 0x1) << 1) |
				(dat[pos + 1] & 0x1);
			dv->Gx = 0x00 | (dat[pos] >> 1);
			pos++;
			dv->Gy = 0x80 | (dat[pos] >> 1);
			pos++;
			dv->Rx = 0xA0 | (dat[pos] >> 3);
			dv->Bx = 0x20 | (dat[pos] & 0x7);
			pos++;
			dv->Ry = 0x40  | (dat[pos] >> 3);
			dv->By = 0x08  | (dat[pos] & 0x7);
			pos++;
		} else
			dv->block_flag = ERROR_LENGTH;
	}

	if (pos > dv->length)
		pr_info("hdmitx: edid: maybe invalid dv%d data\n", dv->ver);
}

/* ----------------------------------------------------------- */
static int Edid_ParsingY420VDBBlock(struct rx_cap *pRXCap,
	unsigned char *buf)
{
	unsigned char tag = 0, ext_tag = 0, data_end = 0;
	unsigned int pos = 0;
	int i = 0, found = 0;

	tag = (buf[pos] >> 5) & 0x7;
	data_end = (buf[pos] & 0x1f)+1;
	pos++;
	ext_tag = buf[pos];

	if ((tag != 0x7) || (ext_tag != 0xe))
		goto INVALID_Y420VDB;

	pos++;
	while (pos < data_end) {
		if (pRXCap->VIC_count < VIC_MAX_NUM) {
			for (i = 0; i < pRXCap->VIC_count; i++) {
				if (pRXCap->VIC[i] == buf[pos]) {
					pRXCap->VIC[i] =
					HDMITX_VIC420_OFFSET + buf[pos];
					found = 1;
					/* Here we do not break,because
					 *	some EDID may have the same
					 *	repeated VICs
					 */
				}
			}
			if (found == 0) {
				pRXCap->VIC[pRXCap->VIC_count] =
				HDMITX_VIC420_OFFSET + buf[pos];
				pRXCap->VIC_count++;
			}
		}
		pos++;
	}

	return 0;

INVALID_Y420VDB:
	pr_info("[%s] it's not a valid y420vdb!\n", __func__);
	return -1;
}

static int Edid_ParsingDRMBlock(struct rx_cap *pRXCap,
	unsigned char *buf)
{
	unsigned char tag = 0, ext_tag = 0, data_end = 0;
	unsigned int pos = 0;

	tag = (buf[pos] >> 5) & 0x7;
	data_end = (buf[pos] & 0x1f);
	pos++;
	ext_tag = buf[pos];
	if ((tag != HDMI_EDID_BLOCK_TYPE_EXTENDED_TAG)
		|| (ext_tag != EXTENSION_DRM_TAG))
		goto INVALID_DRM;
	pos++;
	pRXCap->hdr_sup_eotf_sdr = !!(buf[pos] & (0x1 << 0));
	pRXCap->hdr_sup_eotf_hdr = !!(buf[pos] & (0x1 << 1));
	pRXCap->hdr_sup_eotf_smpte_st_2084 = !!(buf[pos] & (0x1 << 2));
	pRXCap->hdr_sup_eotf_hlg = !!(buf[pos] & (0x1 << 3));
	pos++;
	pRXCap->hdr_sup_SMD_type1 = !!(buf[pos] & (0x1 << 0));
	pos++;
	if (data_end == 3)
		return 0;
	if (data_end == 4) {
		pRXCap->hdr_lum_max = buf[pos];
		return 0;
	}
	if (data_end == 5) {
		pRXCap->hdr_lum_max = buf[pos];
		pRXCap->hdr_lum_avg = buf[pos + 1];
		return 0;
	}
	if (data_end == 6) {
		pRXCap->hdr_lum_max = buf[pos];
		pRXCap->hdr_lum_avg = buf[pos + 1];
		pRXCap->hdr_lum_min = buf[pos + 2];
		return 0;
	}
	return 0;
INVALID_DRM:
	pr_info("[%s] it's not a valid DRM\n", __func__);
	return -1;
}

static int Edid_ParsingVFPDB(struct rx_cap *pRXCap, unsigned char *buf)
{
	unsigned int len = buf[0] & 0x1f;
	enum hdmi_vic svr = HDMI_Unknown;

	if (buf[1] != EXTENSION_VFPDB_TAG)
		return 0;
	if (len < 2)
		return 0;

	svr = buf[2];
	if (((svr >= 1) && (svr <= 127)) ||
		((svr >= 193) && (svr <= 253))) {
		pRXCap->flag_vfpdb = 1;
		pRXCap->preferred_mode = svr;
		pr_info("preferred mode 0 srv %d\n", pRXCap->preferred_mode);
		return 1;
	}
	if ((svr >= 129) && (svr <= 144)) {
		pRXCap->flag_vfpdb = 1;
		pRXCap->preferred_mode = pRXCap->dtd[svr - 129].vic;
		pr_info("preferred mode 0 dtd %d\n", pRXCap->preferred_mode);
		return 1;
	}
	return 0;
}

/* ----------------------------------------------------------- */
static int Edid_ParsingY420CMDBBlock(struct hdmitx_info *info,
	unsigned char *buf)
{
	unsigned char tag = 0, ext_tag = 0, length = 0, data_end = 0;
	unsigned int pos = 0, i = 0;

	tag = (buf[pos] >> 5) & 0x7;
	length = buf[pos] & 0x1f;
	data_end = length + 1;
	pos++;
	ext_tag = buf[pos];

	if ((tag != 0x7) || (ext_tag != 0xf))
		goto INVALID_Y420CMDB;

	if (length == 1) {
		info->y420_all_vic = 1;
		return 0;
	}

	info->bitmap_length = 0;
	info->bitmap_valid = 0;
	memset(info->y420cmdb_bitmap, 0x00, Y420CMDB_MAX);

	pos++;
	if (pos < data_end) {
		info->bitmap_length = data_end - pos;
		info->bitmap_valid = 1;
	}
	while (pos < data_end) {
		if (i < Y420CMDB_MAX)
			info->y420cmdb_bitmap[i] = buf[pos];
		pos++;
		i++;
	}

	return 0;

INVALID_Y420CMDB:
	pr_info("[%s] it's not a valid y420cmdb!\n", __func__);
	return -1;

}

static int Edid_Y420CMDB_fill_all_vic(struct hdmitx_dev *hdmitx_device)
{
	struct rx_cap *rxcap = &(hdmitx_device->RXCap);
	struct hdmitx_info *info = &(hdmitx_device->hdmi_info);
	unsigned int count = rxcap->VIC_count;
	unsigned int a, b;

	if (info->y420_all_vic != 1)
		return 1;

	a = count/8;
	a = (a >= Y420CMDB_MAX)?Y420CMDB_MAX:a;
	b = count%8;

	if (a > 0)
		memset(&(info->y420cmdb_bitmap[0]), 0xff, a);

	if ((b != 0) && (a < Y420CMDB_MAX))
		info->y420cmdb_bitmap[a] = (((1 << b) - 1) << (8-b));

	info->bitmap_length = (b == 0) ? a : (a + 1);
	info->bitmap_valid = (info->bitmap_length != 0)?1:0;

	return 0;
}

static int Edid_Y420CMDB_PostProcess(struct hdmitx_dev *hdmitx_device)
{
	unsigned int i = 0, j = 0, valid = 0;
	struct rx_cap *rxcap = &(hdmitx_device->RXCap);
	struct hdmitx_info *info = &(hdmitx_device->hdmi_info);
	unsigned char *p = NULL;

	if (info->y420_all_vic == 1)
		Edid_Y420CMDB_fill_all_vic(hdmitx_device);

	if (info->bitmap_valid == 0)
		goto PROCESS_END;

	for (i = 0; i < info->bitmap_length; i++) {
		p = &(info->y420cmdb_bitmap[i]);
		for (j = 0; j < 8; j++) {
			valid = ((*p >> j) & 0x1);
			if (valid != 0) {
				rxcap->VIC[rxcap->VIC_count] =
				HDMITX_VIC420_OFFSET + rxcap->VIC[i*8+j];
				rxcap->VIC_count++;
			}
		}
	}

PROCESS_END:
	return 0;
}

static void Edid_Y420CMDB_Reset(struct hdmitx_info *info)
{
	info->bitmap_valid = 0;
	info->bitmap_length = 0;
	info->y420_all_vic = 0;
	memset(info->y420cmdb_bitmap, 0x00, Y420CMDB_MAX);
}

static char *rptx_edid_aud;
static char rptx_edid_buf[512];
MODULE_PARM_DESC(rptx_edid_aud, "\n receive_edid\n");
module_param(rptx_edid_aud, charp, 0444);

/* ----------------------------------------------------------- */
int Edid_ParsingCEADataBlockCollection(struct hdmitx_info *info,
	unsigned char *buff)
{
	unsigned char AddrTag, D, Addr, Data;
	int temp_addr, i, len, pos;

	/* Byte number offset d where Detailed Timing data begins */
	D = buff[2];
	Addr = 4;

	AddrTag = Addr;
	do {
		Data = buff[AddrTag];
		switch (Data&0xE0) {
		case VIDEO_TAG:
			if ((Addr + (Data&0x1f)) < D)
				Edid_ParsingVideoDATABlock(info, buff,
					Addr + 1, (Data & 0x1F));
			break;

		case AUDIO_TAG:
			len = (Data & 0x1f) + 1;
			rx_set_receiver_edid(&buff[AddrTag], len);
			for (pos = 0, i = 0; i < len; i++)
				pos += sprintf(rptx_edid_buf+pos, "%02x",
					buff[AddrTag + i]);
			rptx_edid_buf[pos + 1] = 0;
			if ((Addr + (Data&0x1f)) < D)
				Edid_ParsingAudioDATABlock(info, buff,
					Addr + 1, (Data & 0x1F));
			break;

		case SPEAKER_TAG:
			if ((Addr + (Data&0x1f)) < D)
				Edid_ParsingSpeakerDATABlock(info, buff,
					Addr + 1);
			break;

		case VENDOR_TAG:
			if ((Addr + (Data&0x1f)) < D) {
				if ((buff[Addr + 1] != 0x03) ||
					(buff[Addr + 2] != 0x0c) ||
					(buff[Addr + 3] != 0x00)) {
					info->auth_state = HDCP_NO_AUTH;
				}
				if ((Data&0x1f) > 5) {
					if (buff[Addr + 6] & 0x80)
						info->support_ai_flag = 1;
				}
			}
			break;

		default:
		break;
		}
		Addr += (Data & 0x1F);   /* next Tag Address */
		AddrTag = ++Addr;
		Data = buff[Addr];
		temp_addr =   Addr + (Data & 0x1F);
		if (temp_addr >= D)	/* force to break; */
			break;
	} while (Addr < D);

	return 0;
}

/* ----------------------------------------------------------- */

/* parse Sink 3D information */
static int hdmitx_edid_3d_parse(struct rx_cap *pRXCap, unsigned char *dat,
	unsigned int size)
{
	int j = 0;
	unsigned int base = 0;
	unsigned int pos = base + 1;

	if (dat[base] & (1 << 7))
		pos += 2;
	if (dat[base] & (1 << 6))
		pos += 2;
	if (dat[base] & (1 << 5)) {
		pRXCap->threeD_present = dat[pos] >> 7;
		pRXCap->threeD_Multi_present = (dat[pos] >> 5) & 0x3;
		pos += 1;
		pRXCap->hdmi_vic_LEN = dat[pos] >> 5;
		pRXCap->HDMI_3D_LEN = dat[pos] & 0x1f;
		pos += pRXCap->hdmi_vic_LEN + 1;

		if (pRXCap->threeD_Multi_present == 0x01) {
			pRXCap->threeD_Structure_ALL_15_0 =
				(dat[pos] << 8) + dat[pos+1];
			pRXCap->threeD_MASK_15_0 = 0;
			pos += 2;
		}
		if (pRXCap->threeD_Multi_present == 0x02) {
			pRXCap->threeD_Structure_ALL_15_0 =
				(dat[pos] << 8) + dat[pos+1];
			pos += 2;
			pRXCap->threeD_MASK_15_0 = (dat[pos] << 8) + dat[pos+1];
			pos += 2;
		}
	}
	while (pos < size) {
		if ((dat[pos] & 0xf) < 0x8) {
			/* frame packing */
			if ((dat[pos] & 0xf) == T3D_FRAME_PACKING)
				pRXCap->support_3d_format[pRXCap->VIC[((dat[pos]
					& 0xf0) >> 4)]].frame_packing = 1;
			/* top and bottom */
			if ((dat[pos] & 0xf) == T3D_TAB)
				pRXCap->support_3d_format[pRXCap->VIC[((dat[pos]
					& 0xf0) >> 4)]].top_and_bottom = 1;
			pos += 1;
		} else {
			/* SidebySide */
			if ((dat[pos] & 0xf) == T3D_SBS_HALF)
				if ((dat[pos+1] >> 4) < 0xb)
					pRXCap->support_3d_format[pRXCap->VIC[
						((dat[pos] & 0xf0) >> 4)]]
						.side_by_side = 1;
			pos += 2;
		}
	}
	if (pRXCap->threeD_MASK_15_0 == 0) {
		for (j = 0; (j < 16) && (j < pRXCap->VIC_count); j++) {
			pRXCap->support_3d_format[pRXCap->VIC[j]].frame_packing
				= 1;
			pRXCap->support_3d_format[pRXCap->VIC[j]].top_and_bottom
				= 1;
			pRXCap->support_3d_format[pRXCap->VIC[j]].side_by_side
				= 1;
		}
	} else {
		for (j = 0; j < 16; j++) {
			if (((pRXCap->threeD_MASK_15_0) >> j) & 0x1) {
				/* frame packing */
				if (pRXCap->threeD_Structure_ALL_15_0
					& (1 << 0))
					pRXCap->support_3d_format[pRXCap->
						VIC[j]].frame_packing = 1;
				/* top and bottom */
				if (pRXCap->threeD_Structure_ALL_15_0
					& (1 << 6))
					pRXCap->support_3d_format[pRXCap->
						VIC[j]].top_and_bottom = 1;
				/* top and bottom */
				if (pRXCap->threeD_Structure_ALL_15_0
					& (1 << 8))
					pRXCap->support_3d_format[pRXCap->
						VIC[j]].side_by_side = 1;
			}
		}
	}
	return 1;
}

/* parse Sink 4k2k information */
static void hdmitx_edid_4k2k_parse(struct rx_cap *pRXCap, unsigned char *dat,
	unsigned int size)
{
	if ((size > 4) || (size == 0)) {
		pr_info(EDID
			"4k2k in edid out of range, SIZE = %d\n",
			size);
		return;
	}
	while (size--) {
		if (*dat == 1)
			pRXCap->VIC[pRXCap->VIC_count] = HDMI_4k2k_30;
		else if (*dat == 2)
			pRXCap->VIC[pRXCap->VIC_count] = HDMI_4k2k_25;
		else if (*dat == 3)
			pRXCap->VIC[pRXCap->VIC_count] = HDMI_4k2k_24;
		else if (*dat == 4)
			pRXCap->VIC[pRXCap->VIC_count] = HDMI_4k2k_smpte_24;
		else
			;
		dat++;
		pRXCap->VIC_count++;
	}
}

static int hdmitx_edid_block_parse(struct hdmitx_dev *hdmitx_device,
	unsigned char *BlockBuf)
{
	unsigned char offset, End;
	unsigned char count;
	unsigned char tag;
	int i, tmp, idx;
	unsigned char *vfpdb_offset = NULL;
	struct rx_cap *pRXCap = &(hdmitx_device->RXCap);
	unsigned int aud_flag = 0;

	if (BlockBuf[0] != 0x02)
		return -1; /* not a CEA BLOCK. */
	End = BlockBuf[2]; /* CEA description. */
	pRXCap->native_Mode = BlockBuf[3];
	pRXCap->number_of_dtd += BlockBuf[3] & 0xf;

	pRXCap->native_VIC = 0xff;
	pRXCap->AUD_count = 0;

	Edid_Y420CMDB_Reset(&(hdmitx_device->hdmi_info));

	for (offset = 4 ; offset < End ; ) {
		tag = BlockBuf[offset] >> 5;
		count = BlockBuf[offset] & 0x1f;
		switch (tag) {
		case HDMI_EDID_BLOCK_TYPE_AUDIO:
			aud_flag = 1;
			tmp = count / 3;
			idx = pRXCap->AUD_count;
			pRXCap->AUD_count += tmp;
			offset++;
			for (i = 0 ; i < tmp; i++) {
				pRXCap->RxAudioCap[idx + i].audio_format_code =
					(BlockBuf[offset + i * 3]>>3)&0xf;
				pRXCap->RxAudioCap[idx + i].channel_num_max =
					BlockBuf[offset + i * 3]&0x7;
				pRXCap->RxAudioCap[idx + i].freq_cc =
					BlockBuf[offset + i * 3 + 1]&0x7f;
				pRXCap->RxAudioCap[idx + i].cc3 =
					BlockBuf[offset + i * 3 + 2]&0x7;
			}
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_VIDEO:
			offset++;
			for (i = 0 ; i < count ; i++) {
				unsigned char VIC;

				VIC = BlockBuf[offset + i] & (~0x80);
				pRXCap->VIC[pRXCap->VIC_count] = VIC;
				if (BlockBuf[offset + i] & 0x80)
					pRXCap->native_VIC = VIC;
				pRXCap->VIC_count++;
			}
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_VENDER:
			offset++;
			if ((BlockBuf[offset] == 0x03) &&
				(BlockBuf[offset+1] == 0x0c) &&
				(BlockBuf[offset+2] == 0x00)) {
				pRXCap->IEEEOUI = 0x000c03;
				pRXCap->ColorDeepSupport =
					(count > 5) ? BlockBuf[offset+5] : 0;
				set_vsdb_dc_cap(pRXCap);
				pRXCap->Max_TMDS_Clock1 =
					(count > 6) ? BlockBuf[offset+6] : 0;

				if (count > 7) {
					tmp = BlockBuf[offset+7];
					idx = offset + 8;
					if (tmp & (1<<6))
						idx += 2;
					if (tmp & (1<<7))
						idx += 2;
					if (tmp & (1<<5)) {
						idx += 1;
						/* valid 4k */
					if (BlockBuf[idx] & 0xe0) {
						hdmitx_edid_4k2k_parse(
							pRXCap,
							&BlockBuf[idx + 1],
							BlockBuf[idx] >> 5);
					}
					/* valid 3D */
					if (BlockBuf[idx-1] & 0xe0) {
						hdmitx_edid_3d_parse(
							pRXCap,
							&BlockBuf[offset+7],
							count - 7);
					}
					}
				}
			} else if ((BlockBuf[offset] == 0xd8) &&
				(BlockBuf[offset+1] == 0x5d) &&
				(BlockBuf[offset+2] == 0xc4)) {
				pRXCap->HF_IEEEOUI = 0xd85dc4;
				pRXCap->Max_TMDS_Clock2 = BlockBuf[offset+4];
				pRXCap->scdc_present =
					!!(BlockBuf[offset+5] & (1 << 7));
				pRXCap->scdc_rr_capable =
					!!(BlockBuf[offset+5] & (1 << 6));
				pRXCap->lte_340mcsc_scramble =
					!!(BlockBuf[offset+5] & (1 << 3));
				set_vsdb_dc_420_cap(&hdmitx_device->RXCap,
					&BlockBuf[offset]);
			}

			offset += count; /* ignore the remaind. */
			break;

		case HDMI_EDID_BLOCK_TYPE_SPEAKER:
			offset++;
			pRXCap->RxSpeakerAllocation = BlockBuf[offset];
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_VESA:
			offset++;
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_EXTENDED_TAG:
			{
				unsigned char ext_tag = 0;

				ext_tag = BlockBuf[offset+1];
				switch (ext_tag) {
				case EXTENSION_VENDOR_SPECIFIC:
					Edid_ParsingVendSpec(pRXCap,
						&BlockBuf[offset]);
					break;
				case EXTENSION_COLORMETRY_TAG:
					pRXCap->colorimetry_data =
						BlockBuf[offset + 2];
					break;
				case EXTENSION_DRM_TAG:
					Edid_ParsingDRMBlock(pRXCap,
						&BlockBuf[offset]);
					rx_set_hdr_lumi(&BlockBuf[offset],
						(BlockBuf[offset] & 0x1f) + 1);
					break;
				case EXTENSION_VFPDB_TAG:
/* Just record VFPDB offset address, call Edid_ParsingVFPDB() after DTD
 * parsing, in case that
 * SVR >=129 and SVR <=144, Interpret as the Kth DTD in the EDID,
 * where K = SVR – 128 (for K=1 to 16)
 */
					vfpdb_offset = &BlockBuf[offset];
					break;
				case EXTENSION_Y420_VDB_TAG:
					Edid_ParsingY420VDBBlock(pRXCap,
						&BlockBuf[offset]);
					break;
				case EXTENSION_Y420_CMDB_TAG:
					Edid_ParsingY420CMDBBlock(
						&(hdmitx_device->hdmi_info),
						&BlockBuf[offset]);
					break;
				default:
					break;
				}
			}
			offset += count+1;
			break;

		case HDMI_EDID_BLOCK_TYPE_RESERVED:
			offset++;
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_RESERVED2:
			offset++;
			offset += count;
			break;

		default:
			break;
		}
	}

	if (aud_flag == 0)
		hdmitx_edid_set_default_aud(hdmitx_device);

	Edid_Y420CMDB_PostProcess(hdmitx_device);
	hdmitx_device->vic_count = pRXCap->VIC_count;

	idx = BlockBuf[3] & 0xf;
	for (i = 0; i < idx; i++)
		Edid_DTD_parsing(pRXCap, &BlockBuf[BlockBuf[2] + i * 18]);
	if (vfpdb_offset)
		Edid_ParsingVFPDB(pRXCap, vfpdb_offset);

	return 0;
}

static void hdmitx_edid_set_default_aud(struct hdmitx_dev *hdev)
{
	struct rx_cap *pRXCap = &(hdev->RXCap);

	/* if AUD_count not equal to 0, no need default value */
	if (pRXCap->AUD_count)
		return;

	pRXCap->AUD_count = 1;
	pRXCap->RxAudioCap[0].audio_format_code = 1; /* PCM */
	pRXCap->RxAudioCap[0].channel_num_max = 1; /* 2ch */
	pRXCap->RxAudioCap[0].freq_cc = 7; /* 32/44.1/48 kHz */
	pRXCap->RxAudioCap[0].cc3 = 7; /* 16/20/24 bit */
}

/* add default VICs for DVI case */
static void hdmitx_edid_set_default_vic(struct hdmitx_dev *hdmitx_device)
{
	struct rx_cap *pRXCap = &(hdmitx_device->RXCap);

	pRXCap->VIC_count = 0x3;
	pRXCap->VIC[0] = HDMI_720x480p60_16x9;
	pRXCap->VIC[1] = HDMI_1280x720p60_16x9;
	pRXCap->VIC[2] = HDMI_1920x1080p60_16x9;
	pRXCap->native_VIC = HDMI_720x480p60_16x9;
	hdmitx_device->vic_count = pRXCap->VIC_count;
	pr_info(EDID "set default vic\n");
}

#if 0
#define PRINT_HASH(hash)	\
	{			\
		pr_info("%s:%d ", __func__, __LINE__); int __i;	\
		for (__i = 0; __i < 20; __i++)	\
			pr_info("%02x,", hash[__i]);	\
			pr_info("\n");\
	}
#else
#define PRINT_HASH(hash)
#endif

static int edid_hash_calc(unsigned char *hash, const char *data,
	unsigned int len)
{
#if 0
	struct scatterlist sg;

	struct crypto_hash *tfm;
	struct hash_desc desc;

	tfm = crypto_alloc_hash("sha1", 0, CRYPTO_ALG_ASYNC);
	PRINT_HASH(hash);
	if (IS_ERR(tfm))
		return -EINVAL;

	PRINT_HASH(hash);
	/* ... set up the scatterlists ... */
	sg_init_one(&sg, (u8 *) data, len);
	desc.tfm = tfm;
	desc.flags = 0;



	if (crypto_hash_digest(&desc, &sg, len, hash))
		return -EINVAL;

	err = crypto_ahash_digest(req);
	ahash_request_zero(req);

	PRINT_HASH(hash);
	crypto_free_hash(tfm);
#endif
	return 1;
}

static int hdmitx_edid_search_IEEEOUI(char *buf)
{
	int i;

	for (i = 0; i < 0x180; i++) {
		if ((buf[i] == 0x03) && (buf[i+1] == 0x0c) &&
			(buf[i+2] == 0x00))
			return 1;
	}
	return 0;
}

/* check EDID strictly */
static int edid_check_valid(unsigned char *buf)
{
	unsigned int chksum = 0;
	unsigned int i = 0;

	/* check block 0 first 8 bytes */
	if ((buf[0] != 0) && (buf[7] != 0))
		return 0;
	for (i = 1; i < 7; i++) {
		if (buf[i] != 0xff)
			return 0;
	}

	/* check block 0 checksum */
	for (chksum = 0, i = 0; i < 0x80; i++)
		chksum += buf[i];

	if ((chksum & 0xff) != 0)
		return 0;

	/* check Extension flag at block 0 */
	if (buf[0x7e] == 0)
		return 0;

	/* check block 1 extension tag */
	if (!((buf[0x80] == 0x2) || (buf[0x80] == 0xf0)))
		return 0;

	/* check block 1 checksum */
	for (chksum = 0, i = 0x80; i < 0x100; i++)
		chksum += buf[i];

	if ((chksum & 0xff) != 0)
		return 0;

	return 1;
}

/* retrun 1 valid edid */
int check_dvi_hdmi_edid_valid(unsigned char *buf)
{
	unsigned int chksum = 0;
	unsigned int i = 0;

	/* check block 0 first 8 bytes */
	if ((buf[0] != 0) && (buf[7] != 0))
		return 0;
	for (i = 1; i < 7; i++) {
		if (buf[i] != 0xff)
			return 0;
	}

	/* check block 0 checksum */
	for (chksum = 0, i = 0; i < 0x80; i++)
		chksum += buf[i];
	if ((chksum & 0xff) != 0)
		return 0;

	if (buf[0x7e] == 0)/* check Extension flag at block 0 */
		return 1;
	/* check block 1 extension tag */
	else if (!((buf[0x80] == 0x2) || (buf[0x80] == 0xf0)))
		return 0;

	/* check block 1 checksum */
	for (chksum = 0, i = 0x80; i < 0x100; i++)
		chksum += buf[i];
	if ((chksum & 0xff) != 0)
		return 0;

	/* check block 2 checksum */
	if (buf[0x7e] > 1) {
		for (chksum = 0, i = 0x100; i < 0x180; i++)
			chksum += buf[i];
		if ((chksum & 0xff) != 0)
			return 0;
	}

	/* check block 3 checksum */
	if (buf[0x7e] > 2) {
		for (chksum = 0, i = 0x180; i < 0x200; i++)
			chksum += buf[i];
		if ((chksum & 0xff) != 0)
			return 0;
	}

	return 1;
}

static void Edid_ManufactureDateParse(struct rx_cap *pRxCap,
		unsigned char *data)
{
	if (data == NULL)
		return;

	/* week:
	 *	0: not specified
	 *	0x1~0x36: valid week
	 *	0x37~0xfe: reserved
	 *	0xff: model year is specified
	 */
	if ((data[0] == 0) || ((data[0] >= 0x37) && (data[0] <= 0xfe)))
		pRxCap->manufacture_week = 0;
	else
		pRxCap->manufacture_week = data[0];

	/* year:
	 *	0x0~0xf: reserved
	 *	0x10~0xff: year of manufacture,
	 *		or model year(if specified by week=0xff)
	 */
	pRxCap->manufacture_year =
		(data[1] <= 0xf)?0:data[1];
}

static void Edid_VersionParse(struct rx_cap *pRxCap,
		unsigned char *data)
{
	if (data == NULL)
		return;

	/*
	 *	0x1: edid version 1
	 *	0x0,0x2~0xff: reserved
	 */
	pRxCap->edid_version = (data[0] == 0x1)?1:0;

	/*
	 *	0x0~0x4: revision number
	 *	0x5~0xff: reserved
	 */
	pRxCap->edid_revision = (data[1] < 0x5)?data[1]:0;
}

static void Edid_PhyscialSizeParse(struct rx_cap *pRxCap,
		unsigned char *data)
{
	if ((data[0] != 0) && (data[1] != 0)) {
		pRxCap->physcial_weight = data[0];
		pRxCap->physcial_height = data[1];
	}
}

/* if edid block 0 are all zeros, then consider RX as HDMI device */
static int edid_zero_data(unsigned char *buf)
{
	int sum = 0;
	int i = 0;

	for (i = 0; i < 128; i++)
		sum += buf[i];

	if (sum == 0)
		return 1;
	else
		return 0;
}

static void dump_dtd_info(struct dtd *t)
{
	pr_info(EDID "%s[%d]\n", __func__, __LINE__);
#define PR(a) pr_info(EDID "%s: %d\n", #a, t->a)
	PR(pixel_clock);
	PR(h_active);
	PR(h_blank);
	PR(v_active);
	PR(v_blank);
	PR(h_sync_offset);
	PR(h_sync);
	PR(v_sync_offset);
	PR(v_sync);
}

static void Edid_DTD_parsing(struct rx_cap *pRXCap, unsigned char *data)
{
	struct hdmi_format_para *para = NULL;
	struct dtd *t = &pRXCap->dtd[pRXCap->dtd_idx];

	memset(t, 0, sizeof(struct dtd));
	t->pixel_clock = data[0] + (data[1] << 8);
	t->h_active = (((data[4] >> 4) & 0xf) << 8) + data[2];
	t->h_blank = ((data[4] & 0xf) << 8) + data[3];
	t->v_active = (((data[7] >> 4) & 0xf) << 8) + data[5];
	t->v_blank = ((data[7] & 0xf) << 8) + data[6];
	t->h_sync_offset = (((data[11] >> 6) & 0x3) << 8) + data[8];
	t->h_sync = (((data[11] >> 4) & 0x3) << 8) + data[9];
	t->v_sync_offset = (((data[11] >> 2) & 0x3) << 4) +
		((data[10] >> 4) & 0xf);
	t->v_sync = (((data[11] >> 0) & 0x3) << 4) + ((data[10] >> 0) & 0xf);
/*
 * Special handling of 1080i60hz, 1080i50hz
 */
	if ((t->pixel_clock == 7425) && (t->h_active == 1920) &&
		(t->v_active == 1080)) {
		t->v_active = t->v_active / 2;
		t->v_blank = t->v_blank / 2;
	}
/*
 * Special handling of 480i60hz, 576i50hz
 */
	if (((((t->flags) >> 1) & 0x3) == 0) && (t->h_active == 1440)) {
		if (t->pixel_clock == 2700) /* 576i50hz */
			goto next;
		if ((t->pixel_clock - 2700) < 10) /* 480i60hz */
			t->pixel_clock = 2702;
next:
		t->v_active = t->v_active / 2;
		t->v_blank = t->v_blank / 2;
	}
/*
 * call hdmi_match_dtd_paras() to check t is matched with VIC
 */
	para = hdmi_match_dtd_paras(t);
	if (para) {
		t->vic = para->vic;
		pRXCap->preferred_mode = pRXCap->dtd[0].vic; /* Select dtd0 */
		pr_info(EDID "get dtd%d vic: %d\n",
			pRXCap->dtd_idx, para->vic);
		pRXCap->dtd_idx++;
	} else
		dump_dtd_info(t);
}

int hdmitx_edid_parse(struct hdmitx_dev *hdmitx_device)
{
	unsigned char CheckSum;
	unsigned char zero_numbers;
	unsigned char BlockCount;
	unsigned char *EDID_buf;
	int i, j, ret_val;
	int idx[4];
	struct rx_cap *pRXCap = &(hdmitx_device->RXCap);
	struct vinfo_s *info = NULL;

	if (check_dvi_hdmi_edid_valid(hdmitx_device->EDID_buf)) {
		EDID_buf = hdmitx_device->EDID_buf;
		hdmitx_device->edid_parsing = 1;
		memcpy(hdmitx_device->EDID_buf1, hdmitx_device->EDID_buf,
			EDID_MAX_BLOCK * 128);
	} else
		EDID_buf = hdmitx_device->EDID_buf1;
	hdmitx_device->edid_ptr = EDID_buf;
	pr_info(EDID "EDID Parser:\n");
	memset(rptx_edid_buf, 0, sizeof(rptx_edid_buf));
	rptx_edid_aud = &rptx_edid_buf[0];
	/* Calculate the EDID hash for special use */
	memset(hdmitx_device->EDID_hash, 0,
		ARRAY_SIZE(hdmitx_device->EDID_hash));
	edid_hash_calc(hdmitx_device->EDID_hash, hdmitx_device->EDID_buf, 256);

	ret_val = Edid_DecodeHeader(&hdmitx_device->hdmi_info, &EDID_buf[0]);

	for (i = 0, CheckSum = 0 ; i < 128 ; i++) {
		CheckSum += EDID_buf[i];
		CheckSum &= 0xFF;
	}

	if (CheckSum != 0)
		pr_info(EDID "PLUGIN_DVI_OUT\n");

	Edid_ParsingIDManufacturerName(&hdmitx_device->RXCap, &EDID_buf[8]);
	Edid_ParsingIDProductCode(&hdmitx_device->RXCap, &EDID_buf[0x0A]);
	Edid_ParsingIDSerialNumber(&hdmitx_device->RXCap, &EDID_buf[0x0C]);
	idx[0] = EDID_DETAILED_TIMING_DES_BLOCK0_POS;
	idx[1] = EDID_DETAILED_TIMING_DES_BLOCK1_POS;
	idx[2] = EDID_DETAILED_TIMING_DES_BLOCK2_POS;
	idx[3] = EDID_DETAILED_TIMING_DES_BLOCK3_POS;
	for (i = 0; i < 4; i++) {
		if ((EDID_buf[idx[i]]) && (EDID_buf[idx[i] + 1]))
			Edid_DTD_parsing(pRXCap, &EDID_buf[idx[i]]);

		if (Edid_find_name_block(&EDID_buf[idx[i]]))
			Edid_ReceiverProductNameParse(&hdmitx_device->RXCap,
				&EDID_buf[idx[i]+5]);
	}

	Edid_ManufactureDateParse(&hdmitx_device->RXCap, &EDID_buf[16]);

	Edid_VersionParse(&hdmitx_device->RXCap, &EDID_buf[18]);

	Edid_PhyscialSizeParse(&hdmitx_device->RXCap, &EDID_buf[21]);

	Edid_DecodeStandardTiming(&hdmitx_device->hdmi_info, &EDID_buf[26], 8);
	Edid_ParseCEADetailedTimingDescriptors(&hdmitx_device->hdmi_info,
		4, 0x36, &EDID_buf[0]);

	BlockCount = EDID_buf[0x7E];
	hdmitx_device->RXCap.blk0_chksum = EDID_buf[0x7F];

	if (BlockCount == 0) {
		pr_info(EDID "EDID BlockCount=0\n");
		hdmitx_edid_set_default_vic(hdmitx_device);

		/* DVI case judgement: only contains one block and
		 * checksum valid
		 */
		CheckSum = 0;
		zero_numbers = 0;
		for (i = 0; i < 128; i++) {
			CheckSum += EDID_buf[i];
			if (EDID_buf[i] == 0)
				zero_numbers++;
		}
		pr_info(EDID "edid blk0 checksum:%d ext_flag:%d\n",
			CheckSum, EDID_buf[0x7e]);
		if ((CheckSum & 0xff) == 0)
			hdmitx_device->RXCap.IEEEOUI = 0;
		else
			hdmitx_device->RXCap.IEEEOUI = 0x0c03;
		if (zero_numbers > 120)
			hdmitx_device->RXCap.IEEEOUI = 0x0c03;

		return 0; /* do nothing. */
	}

	/* Note: some DVI monitor have more than 1 block */
	if ((BlockCount == 1) && (EDID_buf[0x81] == 1)) {
		hdmitx_device->RXCap.IEEEOUI = 0;
		hdmitx_device->RXCap.VIC_count = 0x3;
		hdmitx_device->RXCap.VIC[0] = HDMI_720x480p60_16x9;
		hdmitx_device->RXCap.VIC[1] = HDMI_1280x720p60_16x9;
		hdmitx_device->RXCap.VIC[2] = HDMI_1920x1080p60_16x9;
		hdmitx_device->RXCap.native_VIC = HDMI_720x480p60_16x9;
		hdmitx_device->vic_count = hdmitx_device->RXCap.VIC_count;
		pr_info(EDID "set default vic\n");
		return 0;
	} else if (BlockCount > EDID_MAX_BLOCK) {
		BlockCount = EDID_MAX_BLOCK;
	}

	for (i = 1 ; i <= BlockCount ; i++) {
		if ((BlockCount > 1) && (i == 1))
			CheckSum = 0;	   /* ignore the block1 data */
		else {
			if (((BlockCount == 1) && (i == 1)) ||
				((BlockCount > 1) && (i == 2)))
				Edid_Parse_check_HDMI_VSDB(
					hdmitx_device,
					&EDID_buf[i * 128]);

			for (j = 0, CheckSum = 0 ; j < 128 ; j++) {
				CheckSum += EDID_buf[i*128 + j];
				CheckSum &= 0xFF;
			}
			if (CheckSum == 0) {
				Edid_MonitorCapable861(
					&hdmitx_device->hdmi_info,
					EDID_buf[i * 128 + 3]);
				ret_val = Edid_ParsingCEADataBlockCollection(
					&hdmitx_device->hdmi_info,
					&EDID_buf[i * 128]);
				Edid_ParseCEADetailedTimingDescriptors(
					&hdmitx_device->hdmi_info, 5,
					EDID_buf[i * 128 + 2],
					&EDID_buf[i * 128]);
			}
		}

		hdmitx_edid_block_parse(hdmitx_device, &(EDID_buf[i*128]));
	}

/*
 * Because DTDs are not able to represent some Video Formats, which can be
 * represented as SVDs and might be preferred by Sinks, the first DTD in the
 * base EDID data structure and the first SVD in the first CEA Extension can
 * differ. When the first DTD and SVD do not match and the total number of
 * DTDs defining Native Video Formats in the whole EDID is zero, the first
 * SVD shall take precedence.
 */
	if (!pRXCap->flag_vfpdb && (pRXCap->preferred_mode != pRXCap->VIC[0]) &&
		(pRXCap->number_of_dtd == 0)) {
		pr_info(EDID "change preferred_mode from %d to %d\n",
			pRXCap->preferred_mode,	pRXCap->VIC[0]);
		pRXCap->preferred_mode = pRXCap->VIC[0];
	}

	if (hdmitx_edid_search_IEEEOUI(&EDID_buf[128])) {
		pRXCap->IEEEOUI = 0x0c03;
		pr_info(EDID "find IEEEOUT\n");
	} else {
		pRXCap->IEEEOUI = 0x0;
		pr_info(EDID "not find IEEEOUT\n");
	}

	if ((pRXCap->IEEEOUI != 0x0c03) || (pRXCap->IEEEOUI == 0x0) ||
		(pRXCap->VIC_count == 0))
		hdmitx_edid_set_default_vic(hdmitx_device);

	/* strictly DVI device judgement */
	/* valid EDID & no audio tag & no IEEEOUI */
	if (edid_check_valid(&EDID_buf[0]) &&
		!hdmitx_edid_search_IEEEOUI(&EDID_buf[128])) {
		pRXCap->IEEEOUI = 0x0;
		pr_info(EDID "sink is DVI device\n");
	} else
		pRXCap->IEEEOUI = 0x0c03;

	if (edid_zero_data(EDID_buf))
		pRXCap->IEEEOUI = 0x0c03;

	if ((!pRXCap->AUD_count) && (!pRXCap->IEEEOUI))
		hdmitx_edid_set_default_aud(hdmitx_device);

	edid_save_checkvalue(EDID_buf, BlockCount+1);

	i = hdmitx_edid_dump(hdmitx_device, (char *)(hdmitx_device->tmp_buf),
		HDMI_TMP_BUF_SIZE);
	hdmitx_device->tmp_buf[i] = 0;

	if (!hdmitx_edid_check_valid_blocks(&EDID_buf[0])) {
		pRXCap->IEEEOUI = 0x0c03;
		pr_info(EDID "Invalid edid, consider RX as HDMI device\n");
	}
	/* update RX HDR information */
	info = get_current_vinfo();
	if (info) {
		if (!((strncmp(info->name, "480cvbs", 7) == 0) ||
		(strncmp(info->name, "576cvbs", 7) == 0) ||
		(strncmp(info->name, "null", 4) == 0))) {
			info->hdr_info.hdr_support =
				(pRXCap->hdr_sup_eotf_sdr << 0) |
				(pRXCap->hdr_sup_eotf_hdr << 1) |
				(pRXCap->hdr_sup_eotf_smpte_st_2084 << 2) |
				(pRXCap->hdr_sup_eotf_hlg << 3);
			info->hdr_info.colorimetry_support =
				pRXCap->colorimetry_data;
			info->hdr_info.lumi_max = pRXCap->hdr_lum_max;
			info->hdr_info.lumi_avg = pRXCap->hdr_lum_avg;
			info->hdr_info.lumi_min = pRXCap->hdr_lum_min;
			pr_info(EDID "update rx hdr info %x at edid parsing\n",
				info->hdr_info.hdr_support);
		}
	}
	return 0;

}

static struct dispmode_vic dispmode_vic_tab[] = {
	{"480i60hz", HDMI_480i60_16x9},
	{"480p60hz", HDMI_480p60_16x9},
	{"576i50hz", HDMI_576i50_16x9},
	{"576p50hz", HDMI_576p50_16x9},
	{"720p50hz", HDMI_720p50},
	{"720p60hz", HDMI_720p60},
	{"1080i50hz", HDMI_1080i50},
	{"1080i60hz", HDMI_1080i60},
	{"1080p50hz", HDMI_1080p50},
	{"1080p30hz", HDMI_1080p30},
	{"1080p25hz", HDMI_1080p25},
	{"1080p24hz", HDMI_1080p24},
	{"1080p60hz", HDMI_1080p60},
	{"2160p30hz", HDMI_4k2k_30},
	{"2160p25hz", HDMI_4k2k_25},
	{"2160p24hz", HDMI_4k2k_24},
	{"smpte24hz", HDMI_4k2k_smpte_24},
	{"smpte25hz", HDMI_4096x2160p25_256x135},
	{"smpte30hz", HDMI_4096x2160p30_256x135},
	{"smpte50hz420", HDMI_4096x2160p50_256x135_Y420},
	{"smpte60hz420", HDMI_4096x2160p60_256x135_Y420},
	{"2160p60hz420", HDMI_3840x2160p60_16x9_Y420},
	{"2160p50hz420", HDMI_3840x2160p50_16x9_Y420},
	{"smpte50hz", HDMI_4096x2160p50_256x135},
	{"smpte60hz", HDMI_4096x2160p60_256x135},
	{"2160p60hz", HDMI_4k2k_60},
	{"2160p50hz", HDMI_4k2k_50},

};

int hdmitx_edid_VIC_support(enum hdmi_vic vic)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dispmode_vic_tab); i++) {
		if (vic == dispmode_vic_tab[i].VIC)
			return 1;
	}

	return 0;
}

enum hdmi_vic hdmitx_edid_vic_tab_map_vic(const char *disp_mode)
{
	enum hdmi_vic vic = HDMI_Unknown;
	int i;

	for (i = 0; i < ARRAY_SIZE(dispmode_vic_tab); i++) {
		if (strncmp(disp_mode, dispmode_vic_tab[i].disp_mode,
			strlen(dispmode_vic_tab[i].disp_mode)) == 0) {
			vic = dispmode_vic_tab[i].VIC;
			break;
		}
	}

	if (vic == HDMI_Unknown)
		pr_info(EDID "not find mapped vic\n");

	return vic;
}

const char *hdmitx_edid_vic_tab_map_string(enum hdmi_vic vic)
{
	int i;
	const char *disp_str = NULL;

	for (i = 0; i < ARRAY_SIZE(dispmode_vic_tab); i++) {
		if (vic == dispmode_vic_tab[i].VIC) {
			disp_str = dispmode_vic_tab[i].disp_mode;
			break;
		}
	}

	return disp_str;
}

const char *hdmitx_edid_vic_to_string(enum hdmi_vic vic)
{
	int i;
	const char *disp_str = NULL;

	for (i = 0; i < ARRAY_SIZE(dispmode_vic_tab); i++) {
		if (vic == dispmode_vic_tab[i].VIC) {
			disp_str = dispmode_vic_tab[i].disp_mode;
			break;
		}
	}

	return disp_str;
}

/* For some TV's EDID, there maybe exist some information ambiguous.
 * Such as EDID declears support 2160p60hz(Y444 8bit), but no valid
 * Max_TMDS_Clock2 to indicate that it can support 5.94G signal.
 */
bool hdmitx_edid_check_valid_mode(struct hdmitx_dev *hdev,
	struct hdmi_format_para *para)
{
	bool valid = 0;
	struct rx_cap *pRXCap = NULL;
	unsigned int rx_max_tmds_clk = 0;
	unsigned int calc_tmds_clk = 0;
	int i = 0;
	int svd_flag = 0;
	/* Default max color depth is 24 bit */
	enum hdmi_color_depth rx_y444_max_dc = COLORDEPTH_24B;
	enum hdmi_color_depth rx_y422_max_dc = COLORDEPTH_24B;
	enum hdmi_color_depth rx_y420_max_dc = COLORDEPTH_24B;
	enum hdmi_color_depth rx_rgb_max_dc = COLORDEPTH_24B;

	if (!hdev || !para)
		return 0;

	if (strcmp(para->sname, "invalid") == 0)
		return 0;
	/* exclude such as: 2160p60hz YCbCr444 10bit */
	switch (para->vic) {
	case HDMI_3840x2160p50_16x9:
	case HDMI_3840x2160p60_16x9:
	case HDMI_4096x2160p50_256x135:
	case HDMI_4096x2160p60_256x135:
	case HDMI_3840x2160p50_64x27:
	case HDMI_3840x2160p60_64x27:
		if ((para->cs == COLORSPACE_RGB444) ||
			(para->cs == COLORSPACE_YUV444))
			if (para->cd != COLORDEPTH_24B)
				return 0;
		if (para->cs == COLORSPACE_YUV422)
			if (para->cd != COLORDEPTH_48B)
				return 0;
		break;
	case HDMI_720x480i60_16x9:
	case HDMI_720x576i50_16x9:
		if (para->cs == COLORSPACE_YUV422)
			return 0;
	default:
		break;
	}

	pRXCap = &(hdev->RXCap);

	/* DVI case, only 8bit */
	if (pRXCap->IEEEOUI != 0x0c03) {
		if (para->cd != COLORDEPTH_24B)
			return 0;
	}

	/* target mode is not contained at RX SVD */
	for (i = 0; (i < pRXCap->VIC_count) && (i < VIC_MAX_NUM); i++) {
		if ((para->vic & 0xff) == (pRXCap->VIC[i] & 0xff))
			svd_flag = 1;
	}
	if (svd_flag == 0)
		return 0;

	/* Get RX Max_TMDS_Clock */
	if (pRXCap->Max_TMDS_Clock2)
		rx_max_tmds_clk = pRXCap->Max_TMDS_Clock2 * 5;
	else {
		/* Default min is 74.25 / 5 */
		if (pRXCap->Max_TMDS_Clock1 < 0xf)
			pRXCap->Max_TMDS_Clock1 = 0x1e;
		rx_max_tmds_clk = pRXCap->Max_TMDS_Clock1 * 5;
	}

	calc_tmds_clk = para->tmds_clk;
	if (para->cs == COLORSPACE_YUV420)
		calc_tmds_clk = calc_tmds_clk / 2;
	if (para->cs != COLORSPACE_YUV422) {
		switch (para->cd) {
		case COLORDEPTH_30B:
			calc_tmds_clk = calc_tmds_clk * 5 / 4;
			break;
		case COLORDEPTH_36B:
			calc_tmds_clk = calc_tmds_clk * 3 / 2;
			break;
		case COLORDEPTH_48B:
			calc_tmds_clk = calc_tmds_clk * 2;
			break;
		case COLORDEPTH_24B:
		default:
			calc_tmds_clk = calc_tmds_clk * 1;
			break;
		}
	}
	calc_tmds_clk = calc_tmds_clk / 1000;
	pr_info("RX tmds clk: %d   Calc clk: %d\n", rx_max_tmds_clk,
		calc_tmds_clk);
	if (calc_tmds_clk < rx_max_tmds_clk)
		valid = 1;
	else
		return 0;

	if (para->cs == COLORSPACE_YUV444) {
		/* Rx may not support Y444 */
		if (!(pRXCap->native_Mode & (1 << 5)))
			return 0;
		if (pRXCap->dc_y444 && pRXCap->dc_30bit)
			rx_y444_max_dc = COLORDEPTH_30B;
		if (pRXCap->dc_y444 && pRXCap->dc_36bit)
			rx_y444_max_dc = COLORDEPTH_36B;
		if (para->cd <= rx_y444_max_dc)
			valid = 1;
		else
			valid = 0;
		return valid;
	}
	if (para->cs == COLORSPACE_YUV422) {
		/* Rx may not support Y422 */
		if (!(pRXCap->native_Mode & (1 << 4)))
			return 0;
		if (pRXCap->dc_y444 && pRXCap->dc_30bit)
			rx_y422_max_dc = COLORDEPTH_30B;
		if (pRXCap->dc_y444 && pRXCap->dc_36bit)
			rx_y422_max_dc = COLORDEPTH_36B;
		if (para->cd <= rx_y422_max_dc)
			valid = 1;
		else
			valid = 0;
		return valid;
	}
	if (para->cs == COLORSPACE_RGB444) {
		/* Always assume RX supports RGB444 */
		if (pRXCap->dc_30bit)
			rx_rgb_max_dc = COLORDEPTH_30B;
		if (pRXCap->dc_36bit)
			rx_rgb_max_dc = COLORDEPTH_36B;
		if (para->cd <= rx_rgb_max_dc)
			valid = 1;
		else
			valid = 0;
		return valid;
	}
	if (para->cs == COLORSPACE_YUV420) {
		if (pRXCap->dc_30bit_420)
			rx_y420_max_dc = COLORDEPTH_30B;
		if (pRXCap->dc_36bit_420)
			rx_y420_max_dc = COLORDEPTH_36B;
		if (para->cd <= rx_y420_max_dc)
			valid = 1;
		else
			valid = 0;
		return valid;
	}

	return valid;
}

/* force_flag: 0 means check with RX's edid */
/* 1 means no check wich RX's edid */
enum hdmi_vic hdmitx_edid_get_VIC(struct hdmitx_dev *hdev,
	const char *disp_mode, char force_flag)
{
	struct rx_cap *pRXCap = &(hdev->RXCap);
	int  j;
	enum hdmi_vic vic = hdmitx_edid_vic_tab_map_vic(disp_mode);

	if (vic != HDMI_Unknown) {
		if (force_flag == 0) {
			for (j = 0 ; j < pRXCap->VIC_count ; j++) {
				if (pRXCap->VIC[j] == vic)
					break;
			}
			if (j >= pRXCap->VIC_count)
				vic = HDMI_Unknown;
		}
	}
	return vic;
}

const char *hdmitx_edid_get_native_VIC(struct hdmitx_dev *hdmitx_device)
{
	struct rx_cap *pRXCap = &(hdmitx_device->RXCap);

	return hdmitx_edid_vic_to_string(pRXCap->native_VIC);
}

/* Clear HDMI Hardware Module EDID RAM and EDID Buffer */
void hdmitx_edid_ram_buffer_clear(struct hdmitx_dev *hdmitx_device)
{
	unsigned int i = 0;

	/* Clear HDMI Hardware Module EDID RAM */
	hdmitx_device->HWOp.CntlDDC(hdmitx_device, DDC_EDID_CLEAR_RAM, 0);

	/* Clear EDID Buffer */
	for (i = 0; i < EDID_MAX_BLOCK*128; i++)
		hdmitx_device->EDID_buf[i] = 0;
	for (i = 0; i < EDID_MAX_BLOCK*128; i++)
		hdmitx_device->EDID_buf1[i] = 0;
}

/* Clear the Parse result of HDMI Sink's EDID. */
void hdmitx_edid_clear(struct hdmitx_dev *hdmitx_device)
{
	char tmp[2] = {0};
	struct rx_cap *pRXCap = &(hdmitx_device->RXCap);

	memset(pRXCap, 0, sizeof(struct rx_cap));

	/* Note: in most cases, we think that rx is tv and the default
	 * IEEEOUI is HDMI Identifier
	 */
	pRXCap->IEEEOUI = 0x000c03;

	hdmitx_device->vic_count = 0;
	hdmitx_device->hdmi_info.vsdb_phy_addr.a = 0;
	hdmitx_device->hdmi_info.vsdb_phy_addr.b = 0;
	hdmitx_device->hdmi_info.vsdb_phy_addr.c = 0;
	hdmitx_device->hdmi_info.vsdb_phy_addr.d = 0;
	hdmitx_device->hdmi_info.vsdb_phy_addr.valid = 0;
	memset(&vsdb_local, 0, sizeof(struct vsdb_phyaddr));
	memset(&hdmitx_device->EDID_hash[0], 0,
		sizeof(hdmitx_device->EDID_hash));
	hdmitx_device->edid_parsing = 0;
	hdmitx_edid_set_default_aud(hdmitx_device);
	rx_set_hdr_lumi(&tmp[0], 2);
	rx_set_receiver_edid(&tmp[0], 2);
}

/*
 * print one block data of edid
 */
#define TMP_EDID_BUF_SIZE	(256+8)
static void hdmitx_edid_blk_print(unsigned char *blk, unsigned int blk_idx)
{
	unsigned int i, pos;
	unsigned char *tmp_buf = NULL;

	tmp_buf = kmalloc(TMP_EDID_BUF_SIZE, GFP_KERNEL);
	if (!tmp_buf)
		return;

	memset(tmp_buf, 0, TMP_EDID_BUF_SIZE);
	pr_info(EDID "blk%d raw data\n", blk_idx);
	for (i = 0, pos = 0; i < 128; i++) {
		pos += sprintf(tmp_buf + pos, "%02x", blk[i]);
		if (((i+1) & 0x1f) == 0)    /* print 32bytes a line */
			pos += sprintf(tmp_buf + pos, "\n");
	}
	pos += sprintf(tmp_buf + pos, "\n");
	pr_info(EDID "\n%s\n", tmp_buf);
	kfree(tmp_buf);
}

/*
 * check EDID buf contains valid block numbers
 */
static unsigned int hdmitx_edid_check_valid_blocks(unsigned char *buf)
{
	unsigned int valid_blk_no = 0;
	unsigned int i = 0, j = 0;
	unsigned int tmp_chksum = 0;

	for (j = 0; j < EDID_MAX_BLOCK; j++) {
		for (i = 0; i < 128; i++)
			tmp_chksum += buf[i + j*128];
		if (tmp_chksum != 0) {
			valid_blk_no++;
			if ((tmp_chksum & 0xff) == 0)
				pr_info(EDID "check sum valid\n");
			else
				pr_info(EDID "check sum invalid\n");
		}
		tmp_chksum = 0;
	}
	return valid_blk_no;
}

/*
 * suppose DDC read EDID two times successfully,
 * then compare EDID_buf and EDID_buf1.
 * if same, just print out EDID_buf raw data, else print out 2 buffers
 */
void hdmitx_edid_buf_compare_print(struct hdmitx_dev *hdmitx_device)
{
	unsigned int i = 0;
	unsigned int err_no = 0;
	unsigned char *buf0 = hdmitx_device->EDID_buf;
	unsigned char *buf1 = hdmitx_device->EDID_buf1;
	unsigned int valid_blk_no = 0;
	unsigned int blk_idx = 0;

	for (i = 0; i < EDID_MAX_BLOCK*128; i++) {
		if (buf0[i] != buf1[i])
			err_no++;
	}

	if (err_no == 0) {
		/* calculate valid edid block numbers */
		valid_blk_no = hdmitx_edid_check_valid_blocks(buf0);

		if (valid_blk_no == 0)
			pr_info(EDID "raw data are all zeroes\n");
		else {
			for (blk_idx = 0; blk_idx < valid_blk_no; blk_idx++)
				hdmitx_edid_blk_print(&buf0[blk_idx*128],
					blk_idx);
		}
	} else {
		pr_info(EDID "%d errors between two reading\n", err_no);
		valid_blk_no = hdmitx_edid_check_valid_blocks(buf0);
		for (blk_idx = 0; blk_idx < valid_blk_no; blk_idx++)
			hdmitx_edid_blk_print(&buf0[blk_idx*128], blk_idx);

		valid_blk_no = hdmitx_edid_check_valid_blocks(buf1);
		for (blk_idx = 0; blk_idx < valid_blk_no; blk_idx++)
			hdmitx_edid_blk_print(&buf1[blk_idx*128], blk_idx);
	}
}

int hdmitx_edid_dump(struct hdmitx_dev *hdmitx_device, char *buffer,
	int buffer_len)
{
	int i, pos = 0;
	struct rx_cap *pRXCap = &(hdmitx_device->RXCap);

	pos += snprintf(buffer+pos, buffer_len-pos,
		"Rx Manufacturer Name: %s\n", pRXCap->IDManufacturerName);
	pos += snprintf(buffer+pos, buffer_len-pos,
		"Rx Product Code: %02x%02x\n",
		pRXCap->IDProductCode[0],
		pRXCap->IDProductCode[1]);
	pos += snprintf(buffer+pos, buffer_len-pos,
		"Rx Serial Number: %02x%02x%02x%02x\n",
		pRXCap->IDSerialNumber[0],
		pRXCap->IDSerialNumber[1],
		pRXCap->IDSerialNumber[2],
		pRXCap->IDSerialNumber[3]);
	pos += snprintf(buffer+pos, buffer_len-pos,
		"Rx Product Name: %s\n", pRXCap->ReceiverProductName);

	pos += snprintf(buffer+pos, buffer_len-pos,
		"Manufacture Week: %d\n", pRXCap->manufacture_week);
	pos += snprintf(buffer+pos, buffer_len-pos,
		"Manufacture Year: %d\n", pRXCap->manufacture_year+1990);

	pos += snprintf(buffer+pos, buffer_len-pos,
		"Physcial size(cm): %d x %d\n",
		pRXCap->physcial_weight, pRXCap->physcial_height);

	pos += snprintf(buffer+pos, buffer_len-pos,
		"EDID Version: %d.%d\n",
		pRXCap->edid_version, pRXCap->edid_revision);

	pos += snprintf(buffer+pos, buffer_len-pos,
		"EDID block number: 0x%x\n", hdmitx_device->EDID_buf[0x7e]);
	pos += snprintf(buffer+pos, buffer_len-pos,
		"blk0 chksum: 0x%02x\n", pRXCap->blk0_chksum);

	pos += snprintf(buffer+pos, buffer_len-pos,
		"Source Physical Address[a.b.c.d]: %x.%x.%x.%x\n",
		hdmitx_device->hdmi_info.vsdb_phy_addr.a,
		hdmitx_device->hdmi_info.vsdb_phy_addr.b,
		hdmitx_device->hdmi_info.vsdb_phy_addr.c,
		hdmitx_device->hdmi_info.vsdb_phy_addr.d);

	pos += snprintf(buffer+pos, buffer_len-pos,
		"native Mode %x, VIC (native %d):\n",
		pRXCap->native_Mode, pRXCap->native_VIC);

	pos += snprintf(buffer+pos, buffer_len-pos,
		"ColorDeepSupport %x\n", pRXCap->ColorDeepSupport);

	for (i = 0 ; i < pRXCap->VIC_count ; i++) {
		pos += snprintf(buffer+pos, buffer_len-pos, "%d ",
		pRXCap->VIC[i]);
	}
	pos += snprintf(buffer+pos, buffer_len-pos, "\n");
	pos += snprintf(buffer+pos, buffer_len-pos,
		"Audio {format, channel, freq, cce}\n");
	for (i = 0; i < pRXCap->AUD_count; i++) {
		pos += snprintf(buffer+pos, buffer_len-pos,
			"{%d, %d, %x, %x}\n",
			pRXCap->RxAudioCap[i].audio_format_code,
			pRXCap->RxAudioCap[i].channel_num_max,
			pRXCap->RxAudioCap[i].freq_cc,
			pRXCap->RxAudioCap[i].cc3);
	}
	pos += snprintf(buffer+pos, buffer_len-pos,
		"Speaker Allocation: %x\n", pRXCap->RxSpeakerAllocation);
	pos += snprintf(buffer+pos, buffer_len-pos,
		"Vendor: 0x%x ( %s device)\n",
		pRXCap->IEEEOUI, (pRXCap->IEEEOUI)?"HDMI":"DVI");

	pos += snprintf(buffer+pos, buffer_len-pos,
		"MaxTMDSClock1 %d MHz\n", pRXCap->Max_TMDS_Clock1 * 5);

	if (pRXCap->HF_IEEEOUI) {
		pos += snprintf(buffer+pos, buffer_len-pos, "Vendor2: 0x%x\n",
			pRXCap->HF_IEEEOUI);
		pos += snprintf(buffer+pos, buffer_len-pos,
			"MaxTMDSClock2 %d MHz\n", pRXCap->Max_TMDS_Clock2 * 5);
	}
	if (pRXCap->colorimetry_data)
		pos += snprintf(buffer+pos, buffer_len-pos,
			"ColorMetry: 0x%x\n", pRXCap->colorimetry_data);
	pos += snprintf(buffer+pos, buffer_len-pos, "SCDC: %x\n",
		pRXCap->scdc_present);
	pos += snprintf(buffer+pos, buffer_len-pos, "RR_Cap: %x\n",
		pRXCap->scdc_rr_capable);
	pos += snprintf(buffer+pos, buffer_len-pos, "LTE_340M_Scramble: %x\n",
		pRXCap->lte_340mcsc_scramble);

	if (pRXCap->dv_info.ieeeoui == 0x00d046)
		pos += snprintf(buffer+pos, buffer_len-pos,
			"  DolbyVision%d", pRXCap->dv_info.ver);
	if (pRXCap->hdr_sup_eotf_smpte_st_2084)
		pos += snprintf(buffer+pos, buffer_len-pos, "  HDR");
	if (pRXCap->dc_y444 || pRXCap->dc_30bit || pRXCap->dc_30bit_420)
		pos += snprintf(buffer+pos, buffer_len-pos, "  DeepColor");
	pos += snprintf(buffer+pos, buffer_len-pos, "\n");

	/* for checkvalue which maybe used by application to adjust
	 * whether edid is changed
	 */
	pos += snprintf(buffer+pos, buffer_len-pos,
		"checkvalue: 0x%02x%02x%02x%02x\n",
			edid_checkvalue[0],
			edid_checkvalue[1],
			edid_checkvalue[2],
			edid_checkvalue[3]);

	return pos;
}
