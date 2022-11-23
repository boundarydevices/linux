// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>

#include "mtk_hdmirx.h"
#include "hdmi_rx2_edid.h"
#include "hdmi_rx2_hal.h"
#include "hdmi_rx2_ctrl.h"

#define ELOG(fmt, arg...) pr_info(fmt, ##arg)
#define EDIDDBG hdmi2_is_debug(myhdmi,\
	HDMI_RX_DEBUG_EDID)

#define EDP myhdmi->_bEdidData
#define FVP myhdmi->First_16_VIC
#define DSP myhdmi->cDtsStr

#define DBGLOG(fmt, arg...) \
	do { \
		if (myhdmi->debugtype & HDMI_RX_DEBUG_EDID) \
			pr_info(fmt, ##arg); \
	} while (0)

#define DEBUG_EDID

u8 const aEDIDHeader[EDID_HEADER_LEN] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
u8 const aEVSDBHDR[EDID_VSDB_LEN] = {0x03, 0x0c, 0x00};
u8 const aEHFVSDBHDR[EDID_VSDB_LEN] = {0xd8, 0x5d, 0xc4};
u32 const aEDBVSVDBHDR = 0x00d046;
u32 const aEPLPVSVDBHDR = 0xea9fb1;

const char _cDtsFsStr[][7] = {
	{"32khz  "},
	{"44khz  "},
	{"48khz  "},
	{"88khz  "},
	{"96khz  "},
	{"176khz "},
	{"192khz "}
};

int
i4SI(struct MTK_HDMI *myhdmi, u32 u4Index)
{
	return myhdmi->i4HdmiShareInfo[u4Index];
}

void
vSetSI(struct MTK_HDMI *myhdmi, u32 u4Index, int i4Value)
{
	myhdmi->i4HdmiShareInfo[u4Index] = i4Value;
}

bool read_tx_edid(u8 *prData)
{
	return TRUE;
}

bool
fgReadEDID(struct MTK_HDMI *myhdmi)
{
	u8 bExtBlockNo;

	bExtBlockNo = 0x3;

	read_tx_edid(&EDP[0]);

	bExtBlockNo = EDP[EDID_ADDR_EXT_BLOCK_FLAG];
	if (bExtBlockNo > 3)
		bExtBlockNo = 3;
	vSetSI(myhdmi, SI_EDID_EXT_BLK_NO, bExtBlockNo);
	DBGLOG("[RX]ExtBlockNo: %d\n", bExtBlockNo);

	return TRUE;
}

bool
fgExtEdidChecksumOk(struct MTK_HDMI *myhdmi)
{
	u8 bCheckSum = 0;
	u32 u4Index;
	bool ret;

	for (u4Index = 128; u4Index < EDID_SIZE; u4Index++)
		bCheckSum = bCheckSum + EDP[u4Index];

	if (bCheckSum == 0) {
		ELOG("[RX]EXT EDID CheckSum Ok\n");
		ret = TRUE;
	} else {
		ELOG("[RX]EXT EDID CheckSum Fail\n");
		ret = FALSE;
	}
	return ret;
}

void
AnalyzeDTD(struct MTK_HDMI *myhdmi,
	u16 ui2Active,
	u16 HBlank,
	u8 bVfiq,
	u8 bFormat,
	bool fgFirstDTD)
{
	u32 ui4NTSC = SCP->s_dtd_ntsc_res;
	u32 ui4PAL = SCP->s_dtd_pal_res;
	u32 ui41stNTSC = SCP->s_1st_dtd_ntsc_res;
	u32 ui41stPAL = SCP->s_1st_dtd_pal_res;

	DBGLOG("[RX]EDID DTD\n");
	switch (ui2Active) {
	case 0x5a0: // 480i
		if (HBlank == 0x114) {
			// NTSC
			if (bFormat == 0) {
				// p-scan
				ui4NTSC |= SINK_480P_1440;
				if (fgFirstDTD)
					ui41stNTSC |= SINK_480P_1440;
				DBGLOG("[RX]Support 480p54\n");
			} else {
				ui4NTSC |= SINK_480I;
				if (fgFirstDTD)
					ui41stNTSC |= SINK_480I;
				DBGLOG("[RX]Support 480i\n");
			}
		} else if (HBlank == 0x120) {
			// PAL
			if (bFormat == 0) {
				ui4PAL |= SINK_576P_1440;
				if (fgFirstDTD)
					ui41stPAL |= SINK_576P_1440;
				DBGLOG("[RX]Support 576p54\n");
			} else {
				ui4PAL |= SINK_576I;
				if (fgFirstDTD)
					ui41stPAL |= SINK_576I;
				DBGLOG("[RX]Support 576i\n");
			}
		}
		break;
	case 0x2d0: // 480p
		if ((HBlank == 0x8a) && (bFormat == 0)) {
			ui4NTSC |= SINK_480P;
			if (fgFirstDTD)
				ui41stNTSC |= SINK_480P;
			DBGLOG("[RX]Support 480p27\n");
		} else if ((HBlank == 0x90) && (bFormat == 0)) {
			ui4PAL |= SINK_576P;
			if (fgFirstDTD)
				ui41stPAL |= SINK_576P;
			DBGLOG("[RX]Support 576p27\n");
		}
		break;
	case 0x500: // 720p
		if ((HBlank == 0x172) && (bFormat == 0)) {
			ui4NTSC |= SINK_720P60;
			if (fgFirstDTD)
				ui41stNTSC |= SINK_720P60;
			DBGLOG("[RX]Support 720p60\n");
		} else if ((HBlank == 0x2bc) && (bFormat == 0)) {
			ui4PAL |= SINK_720P50;
			if (fgFirstDTD)
				ui41stPAL |= SINK_720P50;
			DBGLOG("Support 720p50\n");
		}
		break;
	case 0x780: // 1080i, 1080P
		if ((HBlank == 0x118) && (bFormat == 1)) {
			ui4NTSC |= SINK_1080I60;
			if (fgFirstDTD)
				ui41stNTSC |= SINK_1080I60;
			DBGLOG("[RX]Support 1080i60\n");
		} else if ((HBlank == 0x118) && (bFormat == 0)) {
			if ((bVfiq >= 29) && (bVfiq <= 31)) {
				ui4NTSC |= SINK_1080P30;
				if (fgFirstDTD)
					ui41stNTSC |= SINK_1080P30;
				DBGLOG("[RX]Support 1080P30\n");
			} else {
				ui4NTSC |= SINK_1080P60;
				if (fgFirstDTD)
					ui41stNTSC |= SINK_1080P60;
				DBGLOG("[RX]Support 1080P60\n");
			}
		} else if ((HBlank == 0x2d0) && (bFormat == 1)) {
			ui4PAL |= SINK_1080I50;
			if (fgFirstDTD)
				ui41stPAL |= SINK_1080I50;
			DBGLOG("[RX]Support 1080i50\n");
		} else if ((HBlank == 0x2d0) && (bFormat == 0)) {
			if ((bVfiq >= 24) && (bVfiq <= 26)) {
				ui4PAL |= SINK_1080P25;
				if (fgFirstDTD)
					ui41stPAL |= SINK_1080P25;
				DBGLOG("[RX]Support 1080P25\n");
			} else {
				ui4PAL |= SINK_1080P50;
				if (fgFirstDTD)
					ui41stPAL |= SINK_1080P50;
				DBGLOG("[RX]Support 1080P50\n");
			}
		}
		break;
	default:
		break;
	}
	SCP->s_dtd_ntsc_res = ui4NTSC;
	SCP->s_dtd_pal_res = ui4PAL;
	SCP->s_1st_dtd_ntsc_res = ui41stNTSC;
	SCP->s_1st_dtd_pal_res = ui41stPAL;
}

void
vParserCEASVDVICBlock(struct MTK_HDMI *myhdmi,
	u8 bNo,
	u8 *prD)
{
	u32 ui4CEA_NTSC = 0, ui4CEA_PAL = 0, ui4OrgCEA_NTSC = 0;
	u32 ui4OrgCEA_PAL = 0, ui4NativeCEA_NTSC = 0, ui4NativeCEA_PAL = 0;
	bool b_Native_bit = FALSE;
	u8 bVic, bIdx;
	u32 ui4Temp = 0;

	for (bIdx = 0; bIdx < bNo; bIdx++) {
		if (*(prD + 1 + bIdx) & 0x80) {
			// Native bit
			ui4OrgCEA_NTSC = ui4CEA_NTSC;
			ui4OrgCEA_PAL = ui4CEA_PAL;
		}
		myhdmi->svd128VIC[bIdx] = (*(prD + 1 + bIdx) & 0x7f);
		b_Native_bit = (*(prD + 1 + bIdx) & 0x80);
		bVic = (*(prD + 1 + bIdx) & 0x7f);
		switch (bVic) {
		case 1:
			ui4CEA_NTSC |= SINK_VGA;
			ui4OrgCEA_NTSC |= SINK_VGA;
			if (b_Native_bit)
				ui4NativeCEA_NTSC |= SINK_VGA;
			break;
		case 6:
			ui4CEA_NTSC |= SINK_480I;
			ui4OrgCEA_NTSC |= SINK_480I_4_3;
			if (b_Native_bit) // Native bit
				ui4NativeCEA_NTSC |= SINK_480I_4_3;
			break;
		case 7:
			ui4CEA_NTSC |= SINK_480I;
			ui4OrgCEA_NTSC |= SINK_480I; // 16:9
			if (b_Native_bit)
				ui4NativeCEA_NTSC |= SINK_480I;
			break;
		case 2:
			ui4CEA_NTSC |= SINK_480P;
			ui4OrgCEA_NTSC |= SINK_480P_4_3;
			if (b_Native_bit)
				ui4NativeCEA_NTSC |= SINK_480P_4_3;
			break;
		case 3:
			ui4CEA_NTSC |= SINK_480P;
			ui4OrgCEA_NTSC |= SINK_480P;
			if (b_Native_bit)
				ui4NativeCEA_NTSC |= SINK_480P;
			break;
		case 14:
		case 15:
			ui4CEA_NTSC |= SINK_480P_1440;
			ui4OrgCEA_NTSC |= SINK_480P_1440;
			if (b_Native_bit)
				ui4NativeCEA_NTSC |= SINK_480P_1440;
			break;
		case 4:
			ui4CEA_NTSC |= SINK_720P60;
			ui4OrgCEA_NTSC |= SINK_720P60;
			if (b_Native_bit)
				ui4NativeCEA_NTSC |= SINK_720P60;
			break;
		case 5:
			ui4CEA_NTSC |= SINK_1080I60;
			ui4OrgCEA_NTSC |= SINK_1080I60;
			if (b_Native_bit)
				ui4NativeCEA_NTSC |= SINK_1080I60;
			break;
		case 21:
			ui4CEA_PAL |= SINK_576I;
			ui4OrgCEA_PAL |= SINK_576I_4_3;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_576I_4_3;
			break;
		case 22:
			ui4CEA_PAL |= SINK_576I;
			ui4OrgCEA_PAL |= SINK_576I;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_576I;
			break;
		case 16:
			ui4CEA_NTSC |= SINK_1080P60;
			ui4OrgCEA_NTSC |= SINK_1080P60;
			if (b_Native_bit)
				ui4NativeCEA_NTSC |= SINK_1080P60;
			break;
		case 17:
			ui4CEA_PAL |= SINK_576P;
			ui4OrgCEA_PAL |= SINK_576P_4_3;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_576P_4_3;
			break;
		case 18:
			ui4CEA_PAL |= SINK_576P;
			ui4OrgCEA_PAL |= SINK_576P;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_576P;
			break;
		case 29:
		case 30:
			ui4CEA_PAL |= SINK_576P_1440;
			ui4OrgCEA_PAL |= SINK_576P_1440;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_576P_1440;
			break;
		case 19:
			ui4CEA_PAL |= SINK_720P50;
			ui4OrgCEA_PAL |= SINK_720P50;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_720P50;
			break;
		case 20:
			ui4CEA_PAL |= SINK_1080I50;
			ui4OrgCEA_PAL |= SINK_1080I50;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_1080I50;
			break;
		case 31:
			ui4CEA_PAL |= SINK_1080P50;
			ui4OrgCEA_PAL |= SINK_1080P50;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_1080P50;
			break;
		case 32:
			ui4CEA_NTSC |= SINK_1080P24;
			ui4CEA_PAL |= SINK_1080P24;
			ui4CEA_NTSC |= SINK_1080P23976;
			ui4CEA_PAL |= SINK_1080P23976;
			ui4OrgCEA_PAL |= SINK_1080P24;
			ui4OrgCEA_NTSC |= SINK_1080P23976;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_1080P24;
			break;
		case 33:
			// ui4CEA_NTSC |= SINK_1080P25;
			ui4CEA_PAL |= SINK_1080P25;
			ui4OrgCEA_PAL |= SINK_1080P25;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_1080P25;
			break;
		case 34:
			ui4CEA_NTSC |= SINK_1080P30;
			ui4CEA_NTSC |= SINK_1080P2997;
			ui4OrgCEA_NTSC |= SINK_1080P2997;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_1080P30;
			break;
		case 35:
			ui4CEA_NTSC |= SINK_480P_2880;
			ui4OrgCEA_NTSC |= SINK_480P_2880_4_3;
			if (b_Native_bit)
				ui4NativeCEA_NTSC |= SINK_480P_2880_4_3;
			break;
		case 36:
			ui4CEA_NTSC |= SINK_480P_2880;
			ui4OrgCEA_NTSC |= SINK_480P_2880;
			if (b_Native_bit)
				ui4NativeCEA_NTSC |= SINK_480P_2880;
			break;
		case 37:
			ui4CEA_PAL |= SINK_576P_2880;
			ui4OrgCEA_PAL |= SINK_576P_2880_4_3;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_576P_2880_4_3;
			break;
		case 38:
			ui4CEA_PAL |= SINK_576P_2880;
			ui4OrgCEA_PAL |= SINK_576P_2880;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_576P_2880;
			break;
		case 93:
		case 103:
			SCP->s_hdmi_4k2kvic |=
				SINK_2160P_24HZ |
				SINK_2160P_23_976HZ;
			break;
		case 94:
		case 104:
			SCP->s_hdmi_4k2kvic |= SINK_2160P_25HZ;
			break;
		case 95:
		case 105:
			SCP->s_hdmi_4k2kvic |=
				SINK_2160P_29_97HZ |
				SINK_2160P_30HZ;
			break;
		case 98:
			SCP->s_hdmi_4k2kvic |=
				SINK_2161P_24HZ |
				SINK_2161P_23_976HZ;
			break;
		case 99:
			SCP->s_hdmi_4k2kvic |= SINK_2161P_25HZ;
			break;
		case 100:
			SCP->s_hdmi_4k2kvic |=
				SINK_2161P_29_97HZ |
				SINK_2161P_30HZ;
			break;
		case 96:
		case 106:
			SCP->s_hdmi_4k2kvic |= SINK_2160P_50HZ;
			break;
		case 97:
		case 107:
			SCP->s_hdmi_4k2kvic |= SINK_2160P_60HZ;
			break;
		case 101:
			SCP->s_hdmi_4k2kvic |= SINK_2161P_50HZ;
			break;
		case 102:
			SCP->s_hdmi_4k2kvic |= SINK_2161P_60HZ;
			break;
		case 60:
			ui4CEA_NTSC |= SINK_720P24;
			ui4CEA_PAL |= SINK_720P24;
			ui4CEA_NTSC |= SINK_720P23976;
			ui4CEA_PAL |= SINK_720P23976;
			ui4OrgCEA_PAL |= SINK_720P24;
			ui4OrgCEA_NTSC |= SINK_720P23976;
			if (b_Native_bit)
				ui4NativeCEA_PAL |= SINK_720P24;
			break;
		default:
			break;
		}

		if (bIdx < 0x10) {
			switch (*(prD + 1 + bIdx) & 0x7f) {
			case 6:
			case 7:
				ui4Temp = SINK_480I;
				break;
			case 2:
			case 3:
				ui4Temp = SINK_480P;
				break;
			case 14:
			case 15:
				ui4Temp = SINK_480P_1440;
				break;
			case 4:
				ui4Temp = SINK_720P60;
				break;
			case 5:
				ui4Temp = SINK_1080I60;
				break;
			case 21:
			case 22:
				ui4Temp = SINK_576I;
				break;
			case 16:
				ui4Temp = SINK_1080P60;
				break;
			case 17:
			case 18:
				ui4Temp = SINK_576P;
				break;
			case 29:
			case 30:
				ui4Temp = SINK_576P_1440;
				break;
			case 19:
				ui4Temp = SINK_720P50;
				break;
			case 20:
				ui4Temp = SINK_1080I50;
				break;
			case 31:
				ui4Temp = SINK_1080P50;
				break;
			case 32:
				ui4Temp = 0;
				ui4Temp |= SINK_1080P24;
				ui4Temp |= SINK_1080P23976;
				break;
			case 33:
				// ui4CEA_NTSC |= SINK_1080P25;
				ui4Temp = SINK_1080P25;
				break;
			case 34:
				ui4Temp = 0;
				ui4Temp |= SINK_1080P30;
				ui4Temp |= SINK_1080P2997;
				break;
			default:
				break;
			}
			myhdmi->First_16_N_VIC |= ui4CEA_NTSC;
			myhdmi->First_16_P_VIC |= ui4CEA_PAL;
			FVP[bIdx] = ui4Temp;
		}
		if (*(prD + 1 + bIdx) & 0x80) {
			ui4OrgCEA_NTSC = ui4CEA_NTSC & (~ui4OrgCEA_NTSC);
			ui4OrgCEA_PAL = ui4CEA_PAL & (~ui4OrgCEA_PAL);
			if (ui4OrgCEA_NTSC) {
				SCP->s_native_ntsc_res = ui4OrgCEA_NTSC;
			} else if (ui4OrgCEA_PAL) {
				SCP->s_native_pal_res = ui4OrgCEA_PAL;
			} else {
				SCP->s_native_ntsc_res = 0;
				SCP->s_native_pal_res = 0;
			}
		}
	}
	SCP->s_cea_ntsc_res |= ui4CEA_NTSC;
	SCP->s_cea_pal_res |= ui4CEA_PAL;
	SCP->s_org_cea_ntsc_res |= ui4OrgCEA_NTSC;
	SCP->s_org_cea_pal_res |= ui4OrgCEA_PAL;
	SCP->s_native_ntsc_res |= ui4NativeCEA_NTSC;
	SCP->s_native_pal_res |= ui4NativeCEA_PAL;
}

void
vParserDBVisionBlk(struct MTK_HDMI *myhdmi,
	u8 *prD)
{
	u8 bVersion;
	u8 bLength;

	bLength = (*prD) & 0x1F;
	bVersion = ((*(prD + 5)) >> 5) & 0x07;
	SCP->s_dbvision_vsvdb_len = bLength;
	SCP->s_dbvision_vsvdb_ver = bVersion;
	if (bLength <= 0x1A)
		memcpy(SCP->s_dbvision_blk, prD, bLength + 1);
	if ((bLength == 0x19) && (bVersion == 0)) {
		SCP->s_supp_dynamic_hdr |= EDID_SUPPORT_DV_HDR;
		if ((*(prD + 5)) & 0x01)
			SCP->s_supp_dynamic_hdr |= EDID_SUPPORT_YUV422_12BIT;
		if ((*(prD + 5)) & 0x02)
			SCP->s_supp_dynamic_hdr |=
				EDID_SUPPORT_DV_HDR_2160P60;
		SCP->s_dbvision_vsvdb_tminPQ =
			((*(prD + 19)) << 4) | (((*(prD + 18)) >> 4) & 0x0F);
		SCP->s_dbvision_vsvdb_tmaxPQ =
			((*(prD + 20)) << 4) | ((*(prD + 18)) & 0x0F);
		SCP->s_dbvision_vsvdb_Rx =
			((*(prD + 7)) << 4) | (((*(prD + 6)) >> 4) & 0x0F);
		SCP->s_dbvision_vsvdb_Ry =
			((*(prD + 8)) << 4) | ((*(prD + 6)) & 0x0F);
		SCP->s_dbvision_vsvdb_Gx =
			((*(prD + 10)) << 4) | (((*(prD + 9)) >> 4) & 0x0F);
		SCP->s_dbvision_vsvdb_Gy =
			((*(prD + 11)) << 4) | ((*(prD + 9)) & 0x0F);
		SCP->s_dbvision_vsvdb_Bx =
			((*(prD + 13)) << 4) | (((*(prD + 12)) >> 4) & 0x0F);
		SCP->s_dbvision_vsvdb_By =
			((*(prD + 14)) << 4) | ((*(prD + 12)) & 0x0F);
		SCP->s_dbvision_vsvdb_Wx =
			((*(prD + 16)) << 4) | (((*(prD + 15)) >> 4) & 0x0F);
		SCP->s_dbvision_vsvdb_Wy =
			((*(prD + 17)) << 4) | ((*(prD + 15)) & 0x0F);
	} else if ((bLength == 0x0E) && (bVersion == 1)) {
		SCP->s_supp_dynamic_hdr |= EDID_SUPPORT_DV_HDR;
		if ((*(prD + 5)) & 0x01)
			SCP->s_supp_dynamic_hdr |= EDID_SUPPORT_YUV422_12BIT;
		if ((*(prD + 5)) & 0x02)
			SCP->s_supp_dynamic_hdr |=
				EDID_SUPPORT_DV_HDR_2160P60;
		SCP->s_dbvision_vsvdb_tmin = (*(prD + 7)) >> 1;
		SCP->s_dbvision_vsvdb_tmax = (*(prD + 6)) >> 1;
		SCP->s_dbvision_vsvdb_Rx = *(prD + 9);
		SCP->s_dbvision_vsvdb_Ry = *(prD + 10);
		SCP->s_dbvision_vsvdb_Gx = *(prD + 11);
		SCP->s_dbvision_vsvdb_Gy = *(prD + 12);
		SCP->s_dbvision_vsvdb_Bx = *(prD + 13);
		SCP->s_dbvision_vsvdb_By = *(prD + 14);
	} else if ((bLength == 0x0B) && (bVersion == 1)) {
		SCP->s_supp_dynamic_hdr |= EDID_SUPPORT_DV_HDR;
		if ((*(prD + 5)) & 0x01)
			SCP->s_supp_dynamic_hdr |= EDID_SUPPORT_YUV422_12BIT;
		if ((*(prD + 5)) & 0x02)
			SCP->s_supp_dynamic_hdr |=
				EDID_SUPPORT_DV_HDR_2160P60;
		SCP->s_dbvision_vsvdb_v1_low_latency = (*(prD + 8)) & 0x03;
		if (SCP->s_dbvision_vsvdb_v1_low_latency == 1)
			SCP->s_dbvision_vsvdb_low_latency_supp = 1;
		SCP->s_dbvision_vsvdb_tmin = (*(prD + 7)) >> 1;
		SCP->s_dbvision_vsvdb_tmax = (*(prD + 6)) >> 1;
		SCP->s_dbvision_vsvdb_Rx = ((*(prD + 11)) >> 3) | 0xA0;
		SCP->s_dbvision_vsvdb_Ry = (((*(prD + 11)) & 0x07) << 2) |
			(((*(prD + 10)) & 0x01) << 1) |
			((*(prD + 9)) & 0x01) | 0x40;
		SCP->s_dbvision_vsvdb_Gx = (*(prD + 9)) >> 1;
		SCP->s_dbvision_vsvdb_Gy = ((*(prD + 10)) >> 1) | 0x80;
		SCP->s_dbvision_vsvdb_Bx = ((*(prD + 8)) >> 5) | 0x20;
		SCP->s_dbvision_vsvdb_By = (((*(prD + 8)) >> 2) & 0x07) | 0x08;
	} else if ((bLength == 0x0B) && (bVersion == 2)) {
		if ((*(prD + 5)) & 0x01)
			SCP->s_supp_dynamic_hdr |= EDID_SUPPORT_YUV422_12BIT;
		SCP->s_dbvision_vsvdb_v2_interface = (*(prD + 7)) & 0x03;
		if ((SCP->s_dbvision_vsvdb_v2_interface == 2) ||
			(SCP->s_dbvision_vsvdb_v2_interface == 3))
			SCP->s_supp_dynamic_hdr |= EDID_SUPPORT_DV_HDR;
		SCP->s_dbvision_vsvdb_low_latency_supp = 1;
		SCP->s_supp_dynamic_hdr |= EDID_SUPPORT_DV_HDR_2160P60;
		SCP->s_dbvision_vsvdb_v2_supp_10b12b_444 =
			(((*(prD + 8)) & 0x01) << 1) | ((*(prD + 9)) & 0x01);
		SCP->s_dbvision_vsvdb_supp_backlight_ctrl =
			((*(prD + 5)) >> 1) & 0x01;
		SCP->s_dbvision_vsvdb_backlt_min_luma = (*(prD + 6)) & 0x03;
		SCP->s_dbvision_vsvdb_tminPQ = ((*(prD + 6)) >> 3) * 20;
		SCP->s_dbvision_vsvdb_tmaxPQ = ((*(prD + 7)) >> 3) * 65 + 2055;
		SCP->s_dbvision_vsvdb_Rx = ((*(prD + 10)) >> 3) | 0xA0;
		SCP->s_dbvision_vsvdb_Ry = ((*(prD + 11)) >> 3) | 0x40;
		SCP->s_dbvision_vsvdb_Gx = (*(prD + 8)) >> 1;
		SCP->s_dbvision_vsvdb_Gy = ((*(prD + 9)) >> 1) | 0x80;
		SCP->s_dbvision_vsvdb_Bx = ((*(prD + 10)) & 0x07) | 0x20;
		SCP->s_dbvision_vsvdb_By = ((*(prD + 11)) & 0x07) | 0x08;
	}
}

void latency_data_8(struct MTK_HDMI *myhdmi,
	u8 *prD)
{
	u8 tmp;

	tmp = *(prD + 8);
	if (tmp & 0x20)
		SCP->s_vid_present = 1;
	else
		SCP->s_vid_present = 0;
	SCP->s_content_cnc = tmp & 0x0f;
	if (tmp & 0x80) {
		// Latency Present
		SCP->s_p_latency_present = TRUE;
		SCP->s_p_video_latency =
			*(prD + 9);
		SCP->s_p_audio_latency =
			*(prD + 10);
		if (tmp & 0x40) {
			// Interlace Latency present
			SCP->s_i_latency_present =
				TRUE;
			SCP->s_i_vid_latency =
				*(prD + 11);
			SCP->s_i_audio_latency =
				*(prD + 12);
		}
	}
	SCP->s_CNC = tmp & 0x0F;
}

void latency_data_13(struct MTK_HDMI *myhdmi,
	u8 LatOfs,
	u8 *prD)
{
	u8 Tmp13;

	Tmp13 = *(prD + 13 - LatOfs);
	if (Tmp13 & 0x80) {
		if (SCP->s_cea_pal_res &
			(SINK_576P |
				SINK_576P_4_3 |
				SINK_720P50 |
				SINK_1080I50 |
				SINK_1080P50))
			myhdmi->_u4i_3D_VIC |=
				SINK_720P50;
		if (SCP->s_cea_ntsc_res &
			(SINK_480P |
				SINK_480P_4_3 |
				SINK_720P60 |
				SINK_1080I60 |
				SINK_1080P60))
			myhdmi->_u4i_3D_VIC |=
				SINK_720P60;
		myhdmi->_u4i_3D_VIC |=
			SINK_1080P23976;
		myhdmi->_u4i_3D_VIC |=
			SINK_1080P24;
		DBGLOG("[RX]3D_present\n");
		SCP->s_3D_present = TRUE;
		SCP->s_3D_struct |=
			((0x1 << 0) |
				(0x1 << 6) |
				(0x1 << 8));
		if (myhdmi->First_16_N_VIC &
			(SINK_480P |
				SINK_480P_4_3 |
				SINK_720P60 |
				SINK_1080I60 |
				SINK_1080P60)) {
			SCP->s_cea_FP_3D |=
				SINK_720P60 |
				SINK_1080P23976 |
				SINK_1080P24;
			SCP->s_cea_TOB_3D |=
				SINK_720P60 |
				SINK_1080P23976 |
				SINK_1080P24;
			SCP->s_cea_SBS_3D |=
				SINK_1080I60;
		}
		if (myhdmi->First_16_P_VIC &
			(SINK_576P |
				SINK_576P_4_3 |
				SINK_720P50 |
				SINK_1080I50 |
				SINK_1080P50)) {
			SCP->s_cea_FP_3D |=
				SINK_720P50 |
				SINK_1080P23976 |
				SINK_1080P24;
			SCP->s_cea_TOB_3D |=
				SINK_720P50 |
				SINK_1080P23976 |
				SINK_1080P24;
			SCP->s_cea_SBS_3D |=
				SINK_1080I50;
		}
	} else
		SCP->s_3D_present = FALSE;
}

void philips_hdr(struct MTK_HDMI *myhdmi,
	u8 bNo,
	u8 *prD)
{
	u8 tmp;

	if (bNo >= 5) {
		tmp = *(prD + 5);
		if (tmp & 0x1)
			SCP->s_supp_dynamic_hdr |=
				EDID_SUPPORT_PHILIPS_HDR;
	}
}

void latency_data_14(struct MTK_HDMI *myhdmi,
	u8 LatOfs,
	u8 D3Multi_present,
	u8 D3Struct70,
	u8 D3Struct158,
	u8 *D2V_ord_Inx_p,
	u8 Tmp14,
	u8 *prD)
{
	u8 DataTmp, i;
	u8 D2V_ord_Inx = *D2V_ord_Inx_p;
	u16 D3MASKALL, D3MASK158, D3MASK70;

	// 3D_Structure_All
	if (D3Multi_present == 0x20) {
		if (((15 - LatOfs + ((Tmp14 & 0xE0) >> 5)) + (Tmp14 & 0x1F)) >=
			(15 - LatOfs +	((Tmp14 & 0xE0) >> 5) + 2)) {
#ifdef DEBUG_EDID
			DBGLOG("[RX]only 3D_Structure\n");
#endif
			D3Struct158 = *(prD + 15 -	LatOfs +
				((Tmp14 & 0xE0) >> 5));
			D3Struct70 = *(prD + 15 - LatOfs +
				((Tmp14 & 0xE0) >> 5) +	1);
			SCP->s_3D_struct |=
				(((u16)(D3Struct158)) << 8) | D3Struct70;
#ifdef DEBUG_EDID
			DBGLOG("[RX]3D_Structure_7_0 =0x%x\n",
				D3Struct70);
			DBGLOG("[RX]3D_Structure_15_8 =0x%x\n",
				D3Struct158);
#endif
		}
		if ((D3Struct70 & 0x01) == 0x01) {
			// support frame packet
			for (i = 0; i < 0x10; i++) {
				myhdmi->_u4i_3D_VIC |= FVP[i];
				SCP->s_cea_FP_3D |=
					FVP[i];
			}
#ifdef DEBUG_EDID
			DBGLOG("[RX]3D_Structure =FP\n");
#endif
		}
		if ((D3Struct70 & 0x40) == 0x40) {
			// support Top and Bottom
			for (i = 0; i < 0x10; i++)
				SCP->s_cea_TOB_3D |=
					FVP[i];
#ifdef DEBUG_EDID
			DBGLOG("[RX]3D_Structure = T&B\n");
#endif
		}
		if ((D3Struct158 & 0x01) ==	0x01) {
			// support Side by Side
			for (i = 0; i < 0x10; i++)
				SCP->s_cea_SBS_3D |= FVP[i];
#ifdef DEBUG_EDID
				DBGLOG("[RX]3D_Struct=Side by Side\n");
#endif
		}
		while (((15 - LatOfs + ((Tmp14 & 0xE0) >>	5)) +
			(Tmp14 & 0x1F)) >
			((15 - LatOfs + ((Tmp14 &  0xE0) >> 5)) + 2 +
			D2V_ord_Inx)) {
			// 2 is 3D_structure
			DataTmp = *(prD + 15 - LatOfs +
				((Tmp14 & 0xE0) >> 5) + 2 + D2V_ord_Inx);
			if ((DataTmp & 0x0F) < 0x08) {
				D2V_ord_Inx = D2V_ord_Inx + 1;
				if ((DataTmp & 0x0F) ==	0x00) {
					//3D_Structure=0,  support frame packet
					myhdmi->_u4i_3D_VIC |=
						FVP[((DataTmp & 0xF0) >> 4)];
					SCP->s_cea_FP_3D |=
						FVP[((DataTmp & 0xF0) >> 4)];
				}
				// 3D_Structure=6,  support Top and Bottom
				if ((DataTmp & 0x0F) == 0x06)
					SCP->s_cea_TOB_3D |=
						FVP[((DataTmp & 0xF0) >> 4)];
#ifdef DEBUG_EDID
ELOG("[RX]2D_VIC_order=%x\n",
((DataTmp &	 0xF0) >> 4));
ELOG("[RX]3D_Structure=%x\n",
(DataTmp &	0x0F));
#endif
			} else {
				D2V_ord_Inx = D2V_ord_Inx + 2;
				if ((DataTmp &  0x0F) == 0x08)
					// 3D_Structure=8,  support Side by side
					SCP->s_cea_SBS_3D |=
						FVP[((DataTmp & 0xF0) >> 4)];
#ifdef DEBUG_EDID
	if (EDIDDBG) {
		ELOG("[RX]2D_VIC_order=%x\n",
		((DataTmp & 0xF0) >> 4));
		ELOG("[HDMI TX2]3D_Structure=%x\n",
		(DataTmp &	0x0F));
		ELOG("[HDMI TX2]3D_Detail=%x\n",
			*(prD + 15 - LatOfs +	((Tmp14 & 0xE0) >>	5) + 2 +
			D2V_ord_Inx - 1));
	}
#endif
			}
		}
	} else if (D3Multi_present == 0x40) {
		if (((15 - LatOfs + ((Tmp14 & 0xE0) >> 5)) + (Tmp14 & 0x1F)) >=
			((15 - LatOfs + ((Tmp14 & 0xE0) >> 5)) + 4)) {
			// 4 is 3D_structure+3D_MASK
			D3Struct158 = *(prD + 15 - LatOfs +
				((Tmp14 & 0xE0) >> 5));
			D3Struct70 = *(prD + 15 - LatOfs +
				((Tmp14 & 0xE0) >> 5) + 1);
			D3MASK158 = *(prD + 15 - LatOfs +
				((Tmp14 & 0xE0) >> 5) + 2);
			D3MASK70 = *(prD + 15 - LatOfs +
				((Tmp14 & 0xE0) >> 5) + 3);
			if ((D3Struct70 & 0x01) == 0x01) {
				D3MASKALL =	(((u16)(D3MASK158))	<< 8) |
					((u16)(D3MASK70));
				for (i = 0;	i <	0x10; i++) {
					if (D3MASKALL &	0x0001) {
						myhdmi->_u4i_3D_VIC |= FVP[i];
						SCP->s_cea_FP_3D |= FVP[i];
					}
					D3MASKALL =	D3MASKALL >> 1;
				}
				SCP->s_3D_struct |=	(0x1 << 0);
			}
			if ((D3Struct70 & 0x40) == 0x40) {
			// support Top and Bottom
				D3MASKALL =	(((u16)(D3MASK158))	<< 8) |
					((u16)(D3MASK70));
				for (i = 0;	i <	0x10; i++) {
					if (D3MASKALL &	0x0001)
						SCP->s_cea_TOB_3D |=
							FVP[i];
					D3MASKALL =	D3MASKALL >> 1;
				}
				SCP->s_3D_struct |=	(0x1 << 6);
			}
			if ((D3Struct158 & 0x01) ==	0x01) {
				// support Side by Side
				D3MASKALL =	(((u16)(D3MASK158)) << 8) |
					((u16)(D3MASK70));
				for (i = 0;	i <	0x10; i++) {
					if (D3MASKALL &	0x0001)
						SCP->s_cea_SBS_3D |=
							FVP[i];
					D3MASKALL =	D3MASKALL >> 1;
				}
				SCP->s_3D_struct |=	(((u16)0x1) << 8);
			}
			DBGLOG("[RX]3D_Structure & 3D_Mask\n");
			DBGLOG("[RX]3D_Structure_7_0=%x\n",
				D3Struct70);
			DBGLOG("[RX]3D_MASK_15_8=%x\n",
				D3MASK158);
			DBGLOG("[RX]3D_MASK_7_0=%x\n",
				D3MASK70);
			DBGLOG("[RX]SBS_SUP_3D_RES = 0x%x\n",
				SCP->s_cea_SBS_3D);
			DBGLOG("[RX]SBS_FP_3D_RES = 0x%x\n",
				SCP->s_cea_FP_3D);
			DBGLOG("[RX]SBS_TOB_3D_RES = 0x%x\n",
				SCP->s_cea_TOB_3D);
			for (i = 0; i <	0x10; i++)
				DBGLOG("[RX]VIC[%d] list 0x%x\n",
					i, FVP[i]);
		}
		while (((15 - LatOfs + ((Tmp14 & 0xE0) >> 5)) +
			(Tmp14 & 0x1F)) >
			(15 - LatOfs + ((Tmp14 & 0xE0) >> 5) +
			4 +	D2V_ord_Inx)) {
			DataTmp = *(prD + 15 -	LatOfs +
				((Tmp14 & 0xE0) >> 5) + 4 + D2V_ord_Inx);
			if ((DataTmp & 0x0F) <	0x08) {
				D2V_ord_Inx = D2V_ord_Inx + 1;
				if ((DataTmp & 0x0F) ==	0x00) {
				// 3D_Structure=0,  supp frame packet
					myhdmi->_u4i_3D_VIC |=
						FVP[((DataTmp & 0xF0) >> 4)];
					SCP->s_cea_FP_3D |=
						FVP[((DataTmp & 0xF0) >> 4)];
				}
				if ((DataTmp & 0x0F) == 0x06) {
				// 3D_Structure=6,  supp Top and Bottom
					SCP->s_cea_TOB_3D |=
						FVP[((DataTmp & 0xF0) >> 4)];
				}
#ifdef DEBUG_EDID
	if (EDIDDBG) {
		ELOG("[RX]2D_VIC_order=%x\n",
		((DataTmp & 0xF0) >> 4));
		ELOG("[RX]3D_Structure=%x\n",
		(DataTmp & 0x0F));
	}
#endif
			} else {
				D2V_ord_Inx = D2V_ord_Inx + 2;
				if ((DataTmp & 0x0F) ==	0x08)
					// 3D_Structure=8,  support Side by side
					SCP->s_cea_SBS_3D |=
						FVP[((DataTmp & 0xF0) >> 4)];
#ifdef DEBUG_EDID
	if (EDIDDBG) {
		ELOG("[HDMI TX]2D_VIC_order=%x\n", ((DataTmp & 0xF0) >> 4));
		ELOG("[HDMI TX]3D_Structure=%x\n", (DataTmp & 0x0F));
		ELOG("[HDMI TX]3D_Detail=%x\n",
			*(prD + 15 - LatOfs + ((Tmp14 & 0xE0) >> 5) +
			4 + D2V_ord_Inx - 1));
	}
#endif
			}
		}
	} else {
#ifdef DEBUG_EDID
		if (EDIDDBG)
			ELOG("[RX]No 3D_Structure & 3D_Mask\n");
#endif
		D3Struct70 = 0;
		while (((15 - LatOfs + ((Tmp14 & 0xE0) >> 5)) +
			(Tmp14 & 0x1F)) >
			((15 - LatOfs + ((Tmp14 & 0xE0) >> 5)) +
			D2V_ord_Inx)) {
			DataTmp = *(prD + 15 - LatOfs + ((Tmp14 & 0xE0)
				>> 5) + D2V_ord_Inx);
			if ((DataTmp & 0x0F) < 0x08) {
				D2V_ord_Inx = D2V_ord_Inx + 1;
				if ((DataTmp & 0x0F) == 0x00) {
				// 3D_Structure=0,  support frame packet
					myhdmi->_u4i_3D_VIC |=
						FVP[((DataTmp & 0xF0) >> 4)];
					SCP->s_cea_FP_3D |=
						FVP[((DataTmp & 0xF0) >> 4)];
					SCP->s_3D_struct |= (1 << 0);
				}
				if ((DataTmp & 0x0F) ==	0x06) {
				// 3D_Structure=6,  support Top and Bottom
					SCP->s_cea_TOB_3D |=
						FVP[((DataTmp & 0xF0) >> 4)];
					SCP->s_3D_struct |= (1 << 6);
				}
#ifdef DEBUG_EDID
	if (EDIDDBG) {
		ELOG("[RX]2D_VIC_order=%x\n",
		((DataTmp & 0xF0) >> 4));
		ELOG("[RX]3D_Structure=%x\n",
		(DataTmp & 0x0F));
	}
#endif
			} else {
				D2V_ord_Inx = D2V_ord_Inx + 2;
				if ((DataTmp & 0x0F) ==	0x08) {
					// 3D_Structure=8,  support Side by side
					SCP->s_cea_SBS_3D |=
						FVP[((DataTmp & 0xF0) >> 4)];
					SCP->s_3D_struct |=	(0x0001 << 8);
				}
#ifdef DEBUG_EDID
	if (EDIDDBG) {
		ELOG("[HDMI TX]2D_VIC_order=%x\n", ((DataTmp & 0xF0) >> 4));
		ELOG("[HDMI TX]3D_Structure=%x\n", (DataTmp & 0x0F));
		ELOG("[HDMI TX]3D_Detail=%x\n",
			*(prD + 15 - LatOfs + ((Tmp14 & 0xE0) >> 5)
			+ D2V_ord_Inx - 1));
	}
#endif
			}
		}
	}

	*D2V_ord_Inx_p = D2V_ord_Inx;
}
void video_present(struct MTK_HDMI *myhdmi,
	u8 bNo,
	u8 Tmp14,
	u8 LatOfs,
	u8 *prD)
{
	u8 bIdx;

	if ((bNo > (14 - LatOfs)) && (((Tmp14 & 0xE0) >> 5) > 0)) {
		// hdmi_vic
		for (bIdx = 0; bIdx < ((Tmp14 & 0xE0) >> 5); bIdx++) {
			if ((*(prD + 15 - LatOfs + bIdx)) == 0x01)
				SCP->s_hdmi_4k2kvic |=
					SINK_2160P_29_97HZ +
					SINK_2160P_30HZ;
			if ((*(prD + 15 - LatOfs + bIdx)) == 0x02)
				SCP->s_hdmi_4k2kvic |=
					SINK_2160P_25HZ;
			if ((*(prD + 15 - LatOfs + bIdx)) == 0x03)
				SCP->s_hdmi_4k2kvic |=
					SINK_2160P_23_976HZ +
					SINK_2160P_24HZ;
			if ((*(prD + 15 - LatOfs + bIdx)) == 0x04)
				SCP->s_hdmi_4k2kvic |=
					SINK_2161P_24HZ;
		}
	}
}

void svd_vic(struct MTK_HDMI *myhdmi,
	u8 bNo,
	u8 *prD)
{
	u8 svd420cmdb, bIdx, i;

	for (bIdx = 0; bIdx < bNo - 1; bIdx++) {
		svd420cmdb = *(prD + 2 + bIdx);
		DBGLOG("[RX]vic=%d\n", svd420cmdb);
		for (i = 0; i < 8; i++) {
			if (svd420cmdb & 0x0001) {
				switch (myhdmi->svd128VIC[bIdx * 8 + i]
					& 0x7f) {
				case 96:
				case 106:
					SCP->s_4k2kvic_420_vdb |=
						SINK_2160P_50HZ;
					break;
				case 97:
				case 107:
					SCP->s_4k2kvic_420_vdb |=
						SINK_2160P_60HZ;
					break;
				case 101:
					SCP->s_4k2kvic_420_vdb |=
						SINK_2161P_50HZ;
					break;
				case 102:
					SCP->s_4k2kvic_420_vdb |=
						SINK_2161P_60HZ;
					break;
				default:
					break;
				}
			}
			svd420cmdb = svd420cmdb >> 1;
		}
	}
}

void
vParserCEADataBlk(struct MTK_HDMI *myhdmi,
	u8 *prD,
	u8 bLen)
{
	u8 tmp, bIdx;
	u8 LSum;
	u8 bType, bNo, AudC, PCN;
	u32 u4IEEE = 0;
#if defined(THREE_D_EDID)
	u8 Tmp13, Tmp8, LatOfs = 0;
#if defined(ADVANCE_THREE_D_EDID)
	u8 D3Multi_present = 0, D3Struct70 = 0, D3Struct158 = 0;
	u8 D2V_ord_Inx = 0;
	u8 i, Tmp14 = 0;
#endif
#endif

	while (bLen) {
		if (bLen > 0x80)
			break;

		// Step 1: get 1st data block type & total number of this data
		// type
		tmp = *prD;
		bType = tmp >> 5; // bit[7:5]
		bNo = tmp & 0x1F; // bit[4:0]
		if (bType == 0x02) {
			// Video data block
			DBGLOG("[RX]CEA video\n");
			vParserCEASVDVICBlock(myhdmi, bNo, prD);
		} else if (bType == 0x01) { // Audio data block
			DBGLOG("[RX]CEA short audio descriptor\n");
			SCP->edid_chksum_and_aud_sup &= ~(SINK_SAD_NO_EXIST);
			for (bIdx = 0; bIdx < (bNo / 3); bIdx++) {
				LSum = bIdx * 3;
				// get audio code
				AudC = (*(prD + LSum + 1) & 0x78) >> 3;
				if ((AudC >= AVD_LPCM) && AudC <= AVD_WMA)
					SCP->s_aud_dec |= (1 << (AudC - 1));
				if (AudC == AVD_LPCM) {
					// LPCM
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2) {
						SCP->s_pcm_ch_samp[PCN - 2]
							|= (*(prD + LSum + 2)
							& 0x7f);
						SCP->s_pcm_bit_size[PCN - 2]
							|= (*(prD + LSum + 3)
							& 0x07);
						SCP->s_org_pcm_ch_samp[PCN - 2]
							|= (*(prD + LSum + 2)
							& 0x7f);
						SCP->s_org_pcm_bit_size[PCN - 2]
							|= (*(prD + LSum + 3)
							& 0x07);
					}
				}
				if (AudC == AVD_DST) {
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2)
						SCP->s_dst_ch_samp[PCN - 2]
							= (*(prD + LSum + 2)
							& 0x7f);
				}
				if (AudC == AVD_DSD) {
					myhdmi->fgHDMIIsRpt = TRUE;
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2)
						SCP->s_dsd_ch_samp[PCN - 2]
							= (*(prD + LSum + 2)
							& 0x7f);
				}
				if (AudC == AVD_AC3) {
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2)
						SCP->s_ac3_ch_samp[PCN - 2]
							= (*(prD + LSum + 2)
							& 0x7f);
				}
				if (AudC == AVD_MPEG1_AUD) {
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2)
						SCP->s_mpeg1_ch_samp[PCN - 2]
							= (*(prD + LSum + 2)
							& 0x7f);
				}
				if (AudC == AVD_MP3) {
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2)
						SCP->s_mp3_ch_samp[PCN - 2]
							= (*(prD + LSum + 2)
							& 0x7f);
				}
				if (AudC == AVD_MPEG2_AUD) {
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2)
						SCP->s_mpeg2_ch_samp[PCN - 2]
							= (*(prD + LSum + 2)
							& 0x7f);
				}
				if (AudC == AVD_AAC) {
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2)
						SCP->s_aac_ch_samp[PCN - 2]
							= (*(prD + LSum + 2)
							& 0x7f);
				}
				if (AudC == AVD_DTS) {
					SCP->s_dts_fs =
						(*(prD + LSum + 2) & 0x7f);
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2)
						SCP->s_dts_ch_samp[PCN - 2]
							= (*(prD + LSum + 2)
							& 0x7f);
				}
				if (AudC == AVD_ATRAC) {
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2)
						SCP->s_atrac_ch_samp[PCN - 2]
							= (*(prD + LSum + 2)
							& 0x7f);
				}
				if (AudC == AVD_DOVI_PLUS) {
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2)
						SCP->s_dbplus_ch_samp[PCN - 2]
							= (*(prD + LSum + 2)
							& 0x7f);
				}
				if (AudC == AVD_DTS_HD) {
					myhdmi->fgHDMIIsRpt = TRUE;
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2)
						SCP->s_dtshd_ch_samp[PCN - 2]
							= (*(prD + LSum + 2)
							& 0x7f);
				}
				if (AudC == AVD_MAT_MLP) {
					myhdmi->fgHDMIIsRpt = TRUE;
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2)
						SCP->s_mat_mlp_ch_samp[PCN - 2]
							= (*(prD + LSum + 2)
							& 0x7f);
				}
				if (AudC == AVD_WMA) {
					PCN = (*(prD + LSum + 1) & 0x07) + 1;
					if (PCN >= 2)
						SCP->s_wma_ch_samp[PCN - 2]
							= (*(prD + LSum + 2)
							& 0x7f);
				}
			} // for(bIdx = 0; bIdx < bNo/3; bIdx++)
		} else if (bType == 0x04) {
			// speaker allocation tag code, 0x04
			DBGLOG("[RX]CEA speaker descriptor\n");
			SCP->s_spk_allocation = *(prD + 1) & 0x7f;
		} else if (bType == 0x03) {
			// VDSB exit
			DBGLOG("[RX]Find VSDB\n");
			if ((*(prD + 1) == aEHFVSDBHDR[0]) &&
				(*(prD + 2) == aEHFVSDBHDR[1]) &&
				(*(prD + 3) == aEHFVSDBHDR[2])) {
				DBGLOG("[RX]HFVSDB match\n");
				vSetSI(myhdmi, SI_VSDB_EXIST, TRUE);
				SCP->s_supp_hdmi_mode = TRUE;
				for (i = 0; i < 8; i++) {
					SCP->s_hfvsdb_blk[i] =
						*(prD + i);
				}
				SCP->s_max_tmds_char_rate = *(prD + 5);
				Tmp13 = *(prD + 6);
				if (Tmp13 & 0x80)
					SCP->s_hf_vsdb_info |=
						EDID_HF_VSDB_SCDC_PRESENT;
				if (Tmp13 & 0x40)
					SCP->s_hf_vsdb_info |=
						EDID_HF_VSDB_RR_CAPABLE;
				if (Tmp13 & 0x08)
					SCP->s_hf_vsdb_info |=
						EDID_HF_VSDB_LTE_340_SCR;
				if (Tmp13 & 0x04)
					SCP->s_hf_vsdb_info |=
						EDID_HF_VSDB_INDEPENDENT_VIEW;
				if (Tmp13 & 0x02)
					SCP->s_hf_vsdb_info |=
						EDID_HF_VSDB_DUAL_VIEW;
				if (Tmp13 & 0x01)
					SCP->s_hf_vsdb_info |=
						EDID_HF_VSDB_3D_OSD_DISPARITY;
				Tmp13 = *(prD + 7);
				SCP->s_dc420_color_bit = (Tmp13 & 0x07);
				if (*(prD + 0) >= 8) {
					SCP->s_allm_en = (*(prD + 8) >> 1) & 1;
					if (SCP->s_allm_en == 1)
						DBGLOG("[RX]allm en, 0x%02x\n",
							*(prD + 8));
				}
				SCP->s_vrr_en = 0;
				if (*(prD + 0) >= 10) {
					SCP->s_vrr_en = 1;
					SCP->s_cnmvrr = (*(prD + 8) >> 3) & 1;
				SCP->s_cinemavrr = (*(prD + 8) >> 4) & 1;
					SCP->s_mdelta = (*(prD + 8) >> 5) & 1;
					SCP->vrr_min = *(prD + 9)  & 0x3f;
				SCP->vrr_max = ((*(prD + 9) >> 6)  & 0x3)
					+ *(prD + 10);
			DBGLOG("[RX]VRR, %x, %x\n", *(prD + 0), *(prD + 8));
			DBGLOG("[RX]%s=%d,%s=%d,%s=%d,%s=%d,%s=%d,%s=%d\n",
						"s_vrr_en", SCP->s_vrr_en,
						"s_cnmvrr", SCP->s_cnmvrr,
						"s_cinemavrr", SCP->s_cinemavrr,
						"s_mdelta", SCP->s_mdelta,
						"vrr_min", SCP->vrr_min,
						"vrr_max", SCP->vrr_max);
				}
			}
			if ((*(prD + 1) == aEVSDBHDR[0]) &&
				(*(prD + 2) == aEVSDBHDR[1]) &&
				(*(prD + 3) == aEVSDBHDR[2])) {
				DBGLOG("[RX]VSDB match\n");
				vSetSI(myhdmi, SI_VSDB_EXIST, TRUE);
				SCP->s_supp_hdmi_mode = TRUE;

				// Read CEC physis address
				if (bNo >= 5) {
					SCP->s_cec_addr = (*(prD + 4) << 8) |
							  (*(prD + 5));
					if ((SCP->s_cec_addr & 0x0F00) != 0)
						myhdmi->fgHDMIIsRpt =
							TRUE;
				} else {
					SCP->s_cec_addr = 0xFFFF;
				}

				// Read Support AI
				if (bNo >= 6) {
					tmp = *(prD + 6);
					if (tmp & 0x80) {
						DBGLOG("[RX]SUPP_AI=1\n");
						SCP->s_supp_ai = 1;
						vSetSI(myhdmi, SI_SUPP_AI, 1);
					} else {
						DBGLOG("[RX]SUPP_AI=0\n");
						SCP->s_supp_ai = 0;
						vSetSI(myhdmi, SI_SUPP_AI, 0);
					}
					SCP->s_supp_ai =
						i4SI(myhdmi, SI_SUPP_AI);
					SCP->s_rgb_color_bit =
						((tmp >> 4) & 0x07);
					SCP->s_max_tmds = *(prD + 7);
					if (tmp & 0x08)
						// support YCbCr Deep Color
						SCP->s_ycc_color_bit =
							((tmp >> 4) & 0x07);
					DBGLOG("[RX]RGB DEEP bit=%x\n",
						SCP->s_rgb_color_bit);
					DBGLOG("[RX]YCbCr DEEP bit=%x\n",
						SCP->s_ycc_color_bit);
				} else {
					DBGLOG("[RX]SUPPORTS_AI = 0\n");
					SCP->s_supp_ai = 0;
					vSetSI(myhdmi, SI_SUPP_AI, 0);
				}

				// max tmds clock
				if (bNo >= 7) {
					tmp = *(prD + 7);
					SCP->s_max_tmds_clk = ((u16)tmp) * 5;
					DBGLOG("[RX]max tmds clk=%x\n",
						SCP->s_max_tmds_clk);
				} else {
					SCP->s_max_tmds_clk = 0;
					DBGLOG("[RX]no max tmds clk\n");
				}

				// Read Latency data
				if (bNo >= 8)
					latency_data_8(myhdmi, prD);

				if (bNo >= 8) {
					tmp = *(prD + 8);
					if (!(tmp & 0x80))
						// Latency Present
						LatOfs = LatOfs + 2;
					if (!(tmp & 0x40))
						// Interlace Latency present
						LatOfs = LatOfs + 2;
					DBGLOG("[RX]Latency_offset=%x\n",
						LatOfs);
				}
				if (bNo >= 13) {
					tmp = *(prD + 13);
					if (tmp & 0x80)
						SCP->s_3D_present = 1;
					else
						SCP->s_3D_present = 0;
				}
				if (bNo >= 8) {
					Tmp8 = *(prD + 8);
					if (Tmp8 & 0x20) {
						SCP->s_vid_present = TRUE;
						DBGLOG("[RX]add vid fmt\n");
					} else
						SCP->s_vid_present = FALSE;
				}
				if (bNo >= (13 - LatOfs))
					latency_data_13(myhdmi, LatOfs, prD);
				else
					SCP->s_3D_present = FALSE;

				if (bNo >= (13 - LatOfs)) {
					Tmp13 = *(prD + 13 - LatOfs);
					if ((Tmp13 & 0x60) == 0x20) {
						// 3D multi present = 01
#ifdef DEBUG_EDID
						DBGLOG("[RX]all VICs 3D\n");
#endif
						D3Multi_present = 0x20;
					} else if ((Tmp13 & 0x60) == 0x40) {
// 3D multi present = 10
#ifdef DEBUG_EDID
						DBGLOG("[RX]some VICs 3D\n");
#endif
						D3Multi_present = 0x40;
					} else {
#ifdef DEBUG_EDID
						DBGLOG("[RX]%s and %s\n",
							"3D_Structure_ALL_X",
							"3D_MASK_X are not pres");
#endif
						D3Multi_present = 0x00;
					}
				}
				if (bNo >= (14 - LatOfs)) {
					Tmp14 = *(prD + 14 - LatOfs);
#ifdef DEBUG_EDID
	DBGLOG("[RX]HDMI_3D_LEN=%x\n", (Tmp14 & 0x1F));
	DBGLOG("[RX]HDMI_VIC_LEN =%x\n", ((Tmp14 & 0xE0) >> 5));
#endif
				}
				if (SCP->s_vid_present == TRUE)
					video_present(myhdmi,
						bNo,
						Tmp14,
						LatOfs,
						prD);
				if (bNo > (14 - LatOfs + ((Tmp14 & 0xE0) >> 5)))
					latency_data_14(myhdmi,
						LatOfs,
						D3Multi_present,
						D3Struct70,
						D3Struct158,
						&D2V_ord_Inx,
						Tmp14,
						prD);
				SCP->s_cea_3D_res = myhdmi->_u4i_3D_VIC;
				DBGLOG("[RX]3D_resolution=%x\n",
					myhdmi->_u4i_3D_VIC);
			} else {
				if (SCP->s_supp_hdmi_mode == TRUE)
					vSetSI(myhdmi, SI_VSDB_EXIST, TRUE);
				else
					vSetSI(myhdmi, SI_VSDB_EXIST, FALSE);
			}
		} else if (bType == 0x07) {
			// Use Extended Tag
			if (*(prD + 1) == 0x05) {
				// Extend Tag code ==0x05
				if (*(prD + 2) & 0x1)
					SCP->s_colorimetry |= SINK_XV_YCC601;
				if (*(prD + 2) & 0x2)
					SCP->s_colorimetry |= SINK_XV_YCC709;
				if (*(prD + 2) & 0x4)
					SCP->s_colorimetry |= SINK_S_YCC601;
				if (*(prD + 2) & 0x8)
					SCP->s_colorimetry |= SINK_ADOBE_YCC601;
				if (*(prD + 2) & 0x10)
					SCP->s_colorimetry |= SINK_ADOBE_RGB;
				if (*(prD + 2) & 0x20)
					SCP->s_colorimetry |=
						SINK_COLOR_SPACE_BT2020_CYCC;
				if (*(prD + 2) & 0x40)
					SCP->s_colorimetry |=
						SINK_COLOR_SPACE_BT2020_YCC;
				if (*(prD + 2) & 0x80)
					SCP->s_colorimetry |=
						SINK_COLOR_SPACE_BT2020_RGB;
				if (*(prD + 3) & 0x1)
					SCP->s_colorimetry |= SINK_METADATA0;
				if (*(prD + 3) & 0x2)
					SCP->s_colorimetry |= SINK_METADATA1;
				if (*(prD + 3) & 0x4)
					SCP->s_colorimetry |= SINK_METADATA2;
			} else if (*(prD + 1) == 0x06) {
				// Extend Tag code ==0x06 sdr
				if (*(prD + 2) & 0x1)
					SCP->s_supp_static_hdr |=
						EDID_SUPPORT_SDR;
				if (*(prD + 2) & 0x2)
					SCP->s_supp_static_hdr |=
						EDID_SUPPORT_HDR;
				if (*(prD + 2) & 0x4)
					SCP->s_supp_static_hdr |=
						EDID_SUPPORT_SMPTE_ST_2084;
				if (*(prD + 2) & 0x8)
					SCP->s_supp_static_hdr |=
						EDID_SUPPORT_FUTURE_EOTF;
				if (*(prD + 2) & 0x10)
					SCP->s_supp_static_hdr |=
						EDID_SUPPORT_ET_4;
				if (*(prD + 2) & 0x20)
					SCP->s_supp_static_hdr |=
						EDID_SUPPORT_ET_5;
				if ((*prD & 0x1F) >= 4)
					SCP->s_hdr_content_max_luma =
						*(prD + 4);
				if ((*prD & 0x1F) >= 5)
					SCP->s_hdr_content_max_frame_avg_luma =
						*(prD + 5);
				if ((*prD & 0x1F) >= 6)
					SCP->s_hdr_content_min_luma =
						*(prD + 6);
				for (tmp = 0; tmp < ((*prD & 0x1F) + 1);
					tmp++) {
					if (tmp <= 6)
						SCP->s_hdr_block[tmp] =
							*(prD + tmp);
				}
			} else if (*(prD + 1) == 0x01) {
				u4IEEE = ((*(prD + 4)) << 16) |
					((*(prD + 3)) << 8) | (*(prD + 2));
				DBGLOG("[RX]u4IEEE = 0x%x\n", u4IEEE);
				if (u4IEEE == aEPLPVSVDBHDR)
					philips_hdr(myhdmi,
						bNo,
						prD);
				else if (u4IEEE == aEDBVSVDBHDR) {
					vParserDBVisionBlk(myhdmi, prD);
				}
			} else if (*(prD + 1) == 0xE) {
				// Extend Tag code ==0xE
				// support ycbcr420 Only SVDs
				DBGLOG("[RX]Support YCBCR 420 only, %d\n",
					bNo - 1);
				for (bIdx = 0; bIdx < bNo - 1; bIdx++) {
					SCP->s_colorimetry |= SINK_YCBCR_420;
					DBGLOG("[RX]vic = %d\n",
						(*(prD + 2 + bIdx) & 0x7f));
					switch (*(prD + 2 + bIdx) & 0x7f) {
					case 96:
					case 106:
						SCP->s_4k2kvic_420_vdb |=
							SINK_2160P_50HZ;
						break;
					case 97:
					case 107:
						SCP->s_4k2kvic_420_vdb |=
							SINK_2160P_60HZ;
						break;
					case 101:
						SCP->s_4k2kvic_420_vdb |=
							SINK_2161P_50HZ;
						break;
					case 102:
						SCP->s_4k2kvic_420_vdb |=
							SINK_2161P_60HZ;
						break;
					default:
						break;
					}
				}
			} else if (*(prD + 1) == 0xF) {
				// Extend Tag code ==0xF CMDB
				// support ycbcr420 capability Map data block
				DBGLOG("[RX]Supp YCC 420 Cap Map,%d\n",
					bNo - 1);
				SCP->s_colorimetry |= SINK_YCBCR_420_CAPABILITY;
				if (bNo == 1)
					SCP->s_4k2kvic_420_vdb =
						SINK_2160P_50HZ |
						SINK_2160P_60HZ |
						SINK_2161P_50HZ |
						SINK_2161P_60HZ;
				else {
					svd_vic(myhdmi,
						bNo,
						prD);
				}
			} else if (*(prD + 1) == 0x0) {
				// Extend Tag code ==0x0
				// support selectable, QS=1
				SCP->s_vcdb_data = *(prD + 2);
				DBGLOG("[RX]s_vcdb_data:%x\n",
					SCP->s_vcdb_data);
			}
		}

		// re-assign the next data block address
		prD += (bNo + 1); // '1' means the tag byte
		bLen -= (bNo + 1);
	}
	SCP->s_pcm_ch_samp[5] |= SCP->s_pcm_ch_samp[6];
	SCP->s_pcm_ch_samp[4] |= SCP->s_pcm_ch_samp[5];
	SCP->s_pcm_ch_samp[3] |= SCP->s_pcm_ch_samp[4];
	SCP->s_pcm_ch_samp[2] |= SCP->s_pcm_ch_samp[3];
	SCP->s_pcm_ch_samp[1] |= SCP->s_pcm_ch_samp[2];
	SCP->s_pcm_ch_samp[0] |= SCP->s_pcm_ch_samp[1];
	SCP->s_dts_ch_samp[5] |= SCP->s_dts_ch_samp[6];
	SCP->s_dts_ch_samp[4] |= SCP->s_dts_ch_samp[5];
	SCP->s_dts_ch_samp[3] |= SCP->s_dts_ch_samp[4];
	SCP->s_dts_ch_samp[2] |= SCP->s_dts_ch_samp[3];
	SCP->s_dts_ch_samp[1] |= SCP->s_dts_ch_samp[2];
	SCP->s_dts_ch_samp[0] |= SCP->s_dts_ch_samp[1];
	SCP->s_dsd_ch_samp[5] |= SCP->s_dsd_ch_samp[6];
	SCP->s_dsd_ch_samp[4] |= SCP->s_dsd_ch_samp[5];
	SCP->s_dsd_ch_samp[3] |= SCP->s_dsd_ch_samp[4];
	SCP->s_dsd_ch_samp[2] |= SCP->s_dsd_ch_samp[3];
	SCP->s_dsd_ch_samp[1] |= SCP->s_dsd_ch_samp[2];
	SCP->s_dsd_ch_samp[0] |= SCP->s_dsd_ch_samp[1];
	if (SCP->edid_chksum_and_aud_sup & SINK_EXT_BLK_CHKSUM_ERR) {
		vSetSI(myhdmi, SI_VSDB_EXIST, FALSE);
		SCP->s_supp_hdmi_mode = FALSE;
	}
	if (EDIDDBG) {
		ELOG("[RX]CEA Aud Support Aud Dec = %x\n",
			SCP->s_aud_dec);
		ELOG("[RX]CEA NTSC Support = %x\n",
			SCP->s_cea_ntsc_res);
		ELOG("[RX]CEA PAL Support = %x\n",
			SCP->s_cea_pal_res);
		ELOG("[RX]CEA ORG NTSC Support = %x\n",
			SCP->s_org_cea_ntsc_res);
		ELOG("[RX]CEA ORG PAL Support = %x\n",
			SCP->s_org_cea_pal_res);
		ELOG("[RX]CEA Native NTSC Support = %x\n",
			SCP->s_native_ntsc_res);
		ELOG("[RX]CEA Native PAL Support = %x\n",
			SCP->s_native_pal_res);
		ELOG("[RX]CEA Speaker Allocate = %x\n",
			SCP->s_spk_allocation);
		ELOG("[RX]CEA 2ch PCM fs = %x\n",
			SCP->s_pcm_ch_samp[0]);
		ELOG("[RX]CEA 6ch PCM fs = %x\n",
			SCP->s_pcm_ch_samp[4]);
		ELOG("[RX]CEA 8ch PCM fs = %x\n",
			SCP->s_pcm_ch_samp[6]);
		ELOG("[RX]CEA 2ch DTS fs = %x\n",
			SCP->s_dts_ch_samp[0]);
		ELOG("[RX]CEA 6ch DTS fs = %x\n",
			SCP->s_dts_ch_samp[4]);
		ELOG("[RX]CEA 8ch DTS fs = %x\n",
			SCP->s_dts_ch_samp[6]);
		ELOG("[RX]CEA 2ch DSD fs = %x\n",
			SCP->s_dsd_ch_samp[0]);
		ELOG("[RX]CEA 6ch DSD fs = %x\n",
			SCP->s_dsd_ch_samp[4]);
		ELOG("[RX]CEA 8ch DSD fs = %x\n",
			SCP->s_dsd_ch_samp[6]);
	}
}

bool
ParserExtEDID(struct MTK_HDMI *myhdmi,
	u8 *prD)
{
	u8 bIdx;
	u8 tmp = 0;
	u16 HAct, HBlank, ui2VActive, ui2VBlanking;
	u8 bOfst, *prCEAaddr;
	u8 Vfiq = 1;
	u32 dHtotal = 1, dVtotal = 1, ui2PixClk = 1;

	DBGLOG("[RX]EDID ext block\n");
	DBGLOG("[RX]Tag:%d Rev:%d Ofst:%d\n",
		*(prD + EXTEDID_ADDR_TAG),
		*(prD + EXTEDID_ADDR_REVISION),
		*(prD + EXTEDID_ADDR_OFST_TIME_DSPR));

	SCP->s_ExtEdid_Rev = *(prD + EXTEDID_ADDR_REVISION);
	for (bIdx = 0; bIdx < E_BLK_LEN; bIdx++)
		tmp += *(prD + bIdx);
	DBGLOG("[RX]Extend CheckSum =%d\n", tmp);
	tmp = 0;

	// check if EDID checksume pass
	if (tmp) {
		DBGLOG("[RX]Extend checksum fail\n");
		return FALSE;
	}

	DBGLOG("[RX]Extend checksum pass\n");
	SCP->edid_chksum_and_aud_sup &= ~SINK_EXT_BLK_CHKSUM_ERR;

	// Step 1: get the offset value of 1st detail timing description within
	// extension block
	bOfst = *(prD + EXTEDID_ADDR_OFST_TIME_DSPR);
	if (*(prD + EDID_ADDR_EXTEND_BYTE3) & 0x40)
		// Support basic audio
		SCP->edid_chksum_and_aud_sup &= ~SINK_BASIC_AUDIO_NO_SUP;

	// Max'0528'04, move to here, after read 0x80 ~ 0xFF because it is
	// 0x83...
	if (*(prD + EDID_ADDR_EXTEND_BYTE3) & 0x20)
		// receiver support YCbCr 4:4:4
		SCP->s_colorimetry |= SINK_YCBCR_444;
	if (*(prD + EDID_ADDR_EXTEND_BYTE3) & 0x10)
		// receiver support YCbCr 4:2:2
		SCP->s_colorimetry |= SINK_YCBCR_422;
	SCP->s_colorimetry |= SINK_RGB;

	// Step 3: read-back the pixel clock of each timing descriptor

	// Step 4: read-back V active line to define EDID resolution
	for (bIdx = 0; bIdx < 6; bIdx++) {
		if (((bOfst + 18 * bIdx) > 109) ||
			(*(prD + bOfst + 18 * bIdx) == 0))
			break;
		HAct =
			(u16)(*(prD + bOfst + 18 * bIdx + OFST_H_ACT_BLA_HI)
			& 0xf0) << 4;
		HAct |= *(prD + bOfst + 18 * bIdx + OFST_H_ACTIVE_LO);
		HBlank =
			(u16)(*(prD + bOfst + 18 * bIdx + OFST_H_ACT_BLA_HI)
			& 0x0f) << 8;
		HBlank |= *(prD + bOfst + 18 * bIdx + OFST_H_BLANKING_LO);
		ui2VBlanking =
			(u16)(*(prD + bOfst + 18 * bIdx + OFST_V_ACTIVE_HI)
			& 0x0f) << 8;
		ui2VBlanking |= *(prD + bOfst + 18 * bIdx + OFST_V_BLANKING_LO);
		tmp = (*(prD + bOfst + 18 * bIdx + OFST_FLAGS) & 0x80) >> 7;
		ui2PixClk = (u16) *(prD + bOfst + 18 * bIdx + OFST_PXL_CLK_LO);
		ui2PixClk |=
			((u16) *(prD + bOfst + 18 * bIdx + OFST_PXL_CLK_HI))
			<< 8;
		ui2VActive =
			(u16) *(prD + bOfst + 18 * bIdx + OFST_V_ACTIVE_LO);
		ui2VActive |=
			(u16)(*(prD + bOfst + 18 * bIdx + OFST_V_ACTIVE_HI)
			& 0xf0) << 4;
		ui2PixClk = ui2PixClk * 10000;
		dHtotal = (HAct + HBlank);
		dVtotal = (ui2VActive + ui2VBlanking);
		Vfiq = 1;
		if (((dHtotal * dVtotal) != 0) && (ui2PixClk != 0))
			Vfiq = ui2PixClk / (dHtotal * dVtotal);
		DBGLOG("[RX]clk=%d,h=%d,v=%d,vfir=%d\n",
			ui2PixClk, dHtotal, dVtotal, Vfiq);
		AnalyzeDTD(myhdmi, HAct, HBlank, Vfiq, tmp, FALSE);
	}
	if (EDIDDBG) {
		ELOG("[RX]EDID ext block DTD\n");
		ELOG("[RX]s_dtd_ntsc_res=%x\n",
			SCP->s_dtd_ntsc_res);
		ELOG("[RX]s_dtd_pal_res=%x\n",
			SCP->s_dtd_pal_res);
	}
	if (*(prD + EXTEDID_ADDR_REVISION) >= 0x03) {
		DBGLOG("[RX]check CEA Data block\n");
		prCEAaddr = prD + 4;
		vParserCEADataBlk(myhdmi, prCEAaddr, bOfst - 4);
	}
	return TRUE;
}

void
ParserExtEDIDState(struct MTK_HDMI *myhdmi,
	u8 *prEdid)
{
	u8 tmp;
	u8 *prD;

	if (i4SI(myhdmi, SI_EDID_RESULT) == TRUE) {
		// parsing EDID extension block if it exist
		for (tmp = 0; tmp < i4SI(myhdmi, SI_EDID_EXT_BLK_NO); tmp++) {
			if ((E_BLK_LEN + tmp * E_BLK_LEN) < EDID_SIZE) {
				if (*(prEdid + E_BLK_LEN + tmp * E_BLK_LEN)
					== 0x02) {
					prD = (prEdid + E_BLK_LEN +
						tmp * E_BLK_LEN);
					ParserExtEDID(myhdmi, prD);
				} else if (*(prEdid + E_BLK_LEN +
					tmp * E_BLK_LEN) == 0xF0) {
					DBGLOG("[RX]BlockMap Detect\n");
				}
			} else {
				ELOG("[RX]Big Err! %s Buf Over\n", __func__);
			}
		}
	}
}

bool
ParserEDID(struct MTK_HDMI *myhdmi,
	u8 *prbData)
{
	u8 bIdx;
	u8 bIndex = 0;
	u8 tmp = 0;
	u16 HAct, HBlank;
	u8 Vfiq = 1;
	u32 dHtotal = 1, dVtotal = 1, ui2PixClk = 1;
	u16 ui2VActive, ui2VBlanking;

	DBGLOG("[RX]EDID base block\n");
	DBGLOG("[RX]EDID ver:%d/rev:%d\n",
		*(prbData + EDID_ADDR_VERSION),
		*(prbData + EDID_ADDR_REVISION));

	SCP->s_edid_ver = *(prbData + EDID_ADDR_VERSION);
	SCP->s_edid_rev = *(prbData + EDID_ADDR_REVISION);
	SCP->s_Disp_H_Size = *(prbData + EDID_IMAGE_HORIZONTAL_SIZE);
	SCP->s_Disp_V_Size = *(prbData + EDID_IMAGE_VERTICAL_SIZE);

	// Step 1: check if EDID header pass
	// ie. EDID[0] ~ EDID[7] = specify header pattern
	for (bIdx = EDID_ADDR_HEADER;
		bIdx < (EDID_ADDR_HEADER + EDID_HEADER_LEN); bIdx++) {
		if (*(prbData + bIdx) != aEDIDHeader[bIdx]) {
			if (EDIDDBG) {
				ELOG("[RX]EDID header ERR\n");
				ELOG("[RX]index=%d, value=%d\n", bIdx,
					*(prbData + bIdx));
			}
			return FALSE;
		}
	}

	// Step 2: Check if EDID checksume pass
	// ie. value of EDID[0] + ... + [0x7F] = 256*n
	for (bIdx = 0; bIdx < E_BLK_LEN; bIdx++) {
		// add the value into checksum
		tmp += *(prbData + bIdx);
	}
	DBGLOG("[RX]Basic CheckSum = %d\n", tmp);

	// check if EDID checksume pass
	if (tmp) {
		DBGLOG("[RX]EDID Base C-Sum Ng\n");
	} else {
		DBGLOG("[RX]EDID Base C-Sum Ok\n");
		SCP->edid_chksum_and_aud_sup &= ~SINK_BASE_BLK_CHKSUM_ERR;
	}

	// [3.3] read-back H active line to define EDID resolution
	for (bIdx = 0; bIdx < 2; bIdx++) {
		HAct = (u16)(*(prbData + E_ADR_T_DSPR_1 +
			18 * bIdx + OFST_H_ACT_BLA_HI) & 0xf0) << 4;
		HAct |= *(prbData + E_ADR_T_DSPR_1 + 18 * bIdx +
			OFST_H_ACTIVE_LO);
		HBlank = (u16)(*(prbData + E_ADR_T_DSPR_1 +
			18 * bIdx + OFST_H_ACT_BLA_HI) & 0x0f) << 8;
		HBlank |= *(prbData + E_ADR_T_DSPR_1 +
			18 * bIdx + OFST_H_BLANKING_LO);
		tmp = (*(prbData + E_ADR_T_DSPR_1 + 18 * bIdx +
			OFST_FLAGS) & 0x80) >> 7;
		ui2VActive = (u16) *(prbData + E_ADR_T_DSPR_1 +
			18 * bIdx + OFST_V_ACTIVE_LO);
		ui2VActive |= (u16)(*(prbData + E_ADR_T_DSPR_1 +
			18 * bIdx + OFST_V_ACTIVE_HI) &	0xf0) << 4;
		ui2VBlanking = (u16)(*(prbData + E_ADR_T_DSPR_1 +
			18 * bIdx + OFST_V_ACTIVE_HI) & 0x0f) << 8;
		ui2VBlanking |= *(prbData + E_ADR_T_DSPR_1 +
			18 * bIdx + OFST_V_BLANKING_LO);
		ui2PixClk = (u16) *(prbData + E_ADR_T_DSPR_1 +
			18 * bIdx + OFST_PXL_CLK_LO);
		ui2PixClk |= ((u16) *(prbData + E_ADR_T_DSPR_1 +
			18 * bIdx + OFST_PXL_CLK_HI)) << 8;
		ui2PixClk = ui2PixClk * 10000;
		dHtotal = (HAct + HBlank);
		dVtotal = (ui2VActive + ui2VBlanking);
		Vfiq = 1;
		if (((dHtotal * dVtotal) != 0) && (ui2PixClk != 0))
			Vfiq = ui2PixClk / (dHtotal * dVtotal);
		DBGLOG("[RX]base,clk=%d,h=%d,v=%d,vfir=%d\n", ui2PixClk,
			dHtotal, dVtotal, Vfiq);
		if (bIdx == 0)
			AnalyzeDTD(myhdmi, HAct, HBlank, Vfiq, tmp, TRUE);
		else
			AnalyzeDTD(myhdmi, HAct, HBlank, Vfiq, tmp, FALSE);

		// add by mingming.fu for monitor NAME
		DBGLOG("[RX]TYPE tag1  = 0x%x, TYPE tag2 = 0x%x\n",
			*(prbData + E_ADR_MTR_DSPR_1 + 3),
			*(prbData + E_ADR_MTR_DSPR_2 + 3));
		if ((*(prbData + E_ADR_MTR_DSPR_1 + 3)) ==
			MONITOR_NAME_DESCRIPTOR) {
			for (bIndex = 0; bIndex < 13; bIndex++) {
				SCP->_bMonitorName[bIndex] =
					*(prbData + E_ADR_MTR_DSPR_1 +
					5 + bIndex);
				DBGLOG("[RX]_bMonitorName[%d]=0x%x\n",
					bIndex,
					SCP->_bMonitorName[bIndex]);
			}
		} else if ((*(prbData + E_ADR_MTR_DSPR_2 + 3)) ==
			MONITOR_NAME_DESCRIPTOR) {
			for (bIndex = 0; bIndex < 13; bIndex++) {
				SCP->_bMonitorName[bIndex] =
					*(prbData + E_ADR_MTR_DSPR_2 +
					5 + bIndex);
				DBGLOG("[RX]_bMonitorName[%d]=0x%x\n",
					bIndex,
					SCP->_bMonitorName[bIndex]);
			}
		}
	}
	if (EDIDDBG) {
		ELOG("[RX]EDID base block OK\n");
		ELOG("[RX]SCP->s_dtd_ntsc_res=%x\n",
			SCP->s_dtd_ntsc_res);
		ELOG("[RX]SCP->s_dtd_pal_res=%x\n",
			SCP->s_dtd_pal_res);
	}
	return TRUE;
}

bool
fgIsHdmiNoEDIDChk(struct MTK_HDMI *myhdmi)
{
	return myhdmi->_fgHdmiNoEdidCheck;
}

void
ShowEdidRawData(struct MTK_HDMI *myhdmi)
{
	u16 tmp, i, j;
	u8 *p;

	for (tmp = 0; tmp <= i4SI(myhdmi, SI_EDID_EXT_BLK_NO); tmp++) {
		ELOG("\n");
		ELOG("===============================\n");
		ELOG("   EDID Block Number=#%d\n", tmp);
		ELOG("===============================\n");
		j = tmp * E_BLK_LEN;
		for (i = 0; i < 8; i++) {
			p = &EDP[j + i * 16];
			ELOG("%02x: %02x %02x %02x %02x %02x %02x %02x %02x\n",
				i * 16 + j,
				p[0], p[1], p[2], p[3],
				p[4], p[5],	p[6], p[7]);
			ELOG("%02x %02x %02x  %02x  %02x  %02x  %02x  %02x\n",
				 p[8], p[9], p[10],	p[11],
				 p[12], p[13], p[14], p[15]);
		}
	}
	ELOG("===============================\n");
	ELOG("\n");
}

void
SetNoEdidChkInfo(struct MTK_HDMI *myhdmi)
{
	u8 bInx;

	ELOG("[RX]%s !!!\n", __func__);
	vSetSI(myhdmi, SI_EDID_RESULT, TRUE);
	vSetSI(myhdmi, SI_VSDB_EXIST, TRUE);
	SCP->s_supp_hdmi_mode = TRUE;
	SCP->s_dtd_ntsc_res = 0xffffffff;
	SCP->s_dtd_pal_res = 0xffffffff;
	SCP->s_1st_dtd_ntsc_res = 0xffffffff;
	SCP->s_1st_dtd_pal_res = 0xffffffff;
	// 420 only don't set to 1
	SCP->s_colorimetry = 0xffff & (~SINK_YCBCR_420);
	SCP->s_cea_ntsc_res = 0xffffffff;
	SCP->s_cea_pal_res = 0xffffffff;

	// SCP->s_native_ntsc_res = 0;
	// SCP->s_native_pal_res = 0;
	// SCP->s_vcdb_data = 0;
	SCP->s_aud_dec = 0xffff;
	SCP->s_dsd_ch_num = 5;
	SCP->s_dts_fs = 0xff;
	for (bInx = 0; bInx < 7; bInx++) {
		SCP->s_pcm_ch_samp[bInx] = 0xff;
		SCP->s_dsd_ch_samp[bInx] = 0xff;
		SCP->s_dst_ch_samp[bInx] = 0xff;
		SCP->s_dts_ch_samp[bInx] = 0xff;
	}
	for (bInx = 0; bInx < 7; bInx++)
		SCP->s_pcm_bit_size[bInx] = 0xff;
	SCP->s_spk_allocation = 0xff;
	SCP->s_rgb_color_bit =
		(HDMI_SINK_DEEP_COLOR_10_BIT |
		HDMI_SINK_DEEP_COLOR_12_BIT |
		HDMI_SINK_DEEP_COLOR_16_BIT);
	SCP->s_ycc_color_bit =
		(HDMI_SINK_DEEP_COLOR_10_BIT |
		HDMI_SINK_DEEP_COLOR_12_BIT |
		HDMI_SINK_DEEP_COLOR_16_BIT);
	SCP->edid_chksum_and_aud_sup = 0;
	SCP->s_supp_ai = 1;
	SCP->s_edid_ready = TRUE;
	SCP->s_3D_present = TRUE;
	SCP->s_cea_3D_res = 0xFFFFFFFF;
	SCP->s_3D_struct = ((0x1 << 0) | (0x1 << 6) | (0x1 << 8));
	SCP->s_cea_FP_3D = 0xFFFFFFFF;
	SCP->s_cea_TOB_3D = 0xFFFFFFFF;
	SCP->s_cea_SBS_3D = 0xFFFFFFFF;
	SCP->s_max_tmds_clk = 0xFFFF;
	SCP->s_hdmi_4k2kvic = 0x3fff;
	SCP->s_4k2kvic_420_vdb = 0x3fff;
	SCP->s_SCDC_present = 0;
	// set 0 because some tv can't support this function
	SCP->s_LTE_340M_sramble = 0;
	SCP->s_max_tmds_char_rate = 0xFF;
	SCP->s_supp_static_hdr = 0xFF;
	SCP->s_supp_dynamic_hdr = 0xFF;
	SCP->s_dbvision_vsvdb_len = 0x0B;
	SCP->s_dbvision_vsvdb_ver = 0x02;
	SCP->s_dbvision_vsvdb_v1_low_latency = 0x01;
	SCP->s_dbvision_vsvdb_v2_interface = 0x03;
	SCP->s_dbvision_vsvdb_low_latency_supp = 1;
	SCP->s_dbvision_vsvdb_v2_supp_10b12b_444 = 0x02;
	SCP->s_dbvision_vsvdb_supp_backlight_ctrl = 1;
}

void
ShowEdidInfo(struct MTK_HDMI *myhdmi)
{
	u32 u4Res = 0;
	char *pMonitorName;
	char string[4] = {0};
	u8 bInx = 0;

	ELOG("[RX]EDID ver:%d/rev:%d\n",
		SCP->s_edid_ver, SCP->s_edid_rev);
	ELOG("[RX]EDID Extend Rev:%d\n",
		SCP->s_ExtEdid_Rev);
	if (SCP->s_supp_hdmi_mode)
		ELOG("[RX]SINK Device is HDMI\n");
	else
		ELOG("[RX]SINK Device is DVI\n");
	if (SCP->s_supp_hdmi_mode)
		ELOG("[RX]CEC ADDRESS:%x\n", SCP->s_cec_addr);
	ELOG("[RX]max clock limit : %d\n",
		SCP->s_max_tmds_clk);
	u4Res = (SCP->s_cea_ntsc_res |
		SCP->s_dtd_ntsc_res |
		SCP->s_cea_pal_res |
		SCP->s_dtd_pal_res);
	if (u4Res & SINK_480I)
		ELOG("[RX]SUPPORT 1440x480I 59.94hz\n");
	if (u4Res & SINK_480I_1440)
		ELOG("[RX]SUPPORT 2880x480I 59.94hz\n");
	if (u4Res & SINK_480P)
		ELOG("[RX]SUPPORT 720x480P 59.94hz\n");
	if (u4Res & SINK_480P_1440)
		ELOG("[RX]SUPPORT 1440x480P 59.94hz\n");
	if (u4Res & SINK_480P_2880)
		ELOG("[RX]SUPPORT 2880x480P 59.94hz\n");
	if (u4Res & SINK_720P60)
		ELOG("[RX]SUPPORT 1280x720P 59.94hz\n");
	if (u4Res & SINK_1080I60)
		ELOG("[RX]SUPPORT 1920x1080I 59.94hz\n");
	if (u4Res & SINK_1080P60)
		ELOG("[RX]SUPPORT 1920x1080P 59.94hz\n");
	if (u4Res & SINK_576I)
		ELOG("[RX]SUPPORT 1440x576I 50hz\n");
	if (u4Res & SINK_576I_1440)
		ELOG("[RX]SUPPORT 2880x576I 50hz\n");
	if (u4Res & SINK_576P)
		ELOG("[RX]SUPPORT 720x576P 50hz\n");
	if (u4Res & SINK_576P_1440)
		ELOG("[RX]SUPPORT 1440x576P 50hz\n");
	if (u4Res & SINK_576P_2880)
		ELOG("[RX]SUPPORT 2880x576P 50hz\n");
	if (u4Res & SINK_720P50)
		ELOG("[RX]SUPPORT 1280x720P 50hz\n");
	if (u4Res & SINK_1080I50)
		ELOG("[RX]SUPPORT 1920x1080I 50hz\n");
	if (u4Res & SINK_1080P50)
		ELOG("[RX]SUPPORT 1920x1080P 50hz\n");
	if (u4Res & SINK_1080P30)
		ELOG("[RX]SUPPORT 1920x1080P 30hz\n");
	if (u4Res & SINK_1080P24)
		ELOG("[RX]SUPPORT 1920x1080P 24hz\n");
	if (u4Res & SINK_1080P25)
		ELOG("[RX]SUPPORT 1920x1080P 25hz\n");
	u4Res = (SCP->s_native_ntsc_res | SCP->s_native_ntsc_res);
	ELOG("[RX]NTSC Native =%x\n", SCP->s_native_ntsc_res);
	ELOG("[RX]PAL Native =%x\n", SCP->s_native_pal_res);
	if (u4Res & SINK_480I)
		ELOG("[RX]Native resolution is 1440x480I 59.94hz\n");
	if (u4Res & SINK_480I_1440)
		ELOG("[RX]Native resolution is 2880x480I 59.94hz\n");
	if (u4Res & SINK_480P)
		ELOG("[RX]Native resolution is 720x480P 59.94hz\n");
	if (u4Res & SINK_480P_1440)
		ELOG("[RX]Native resolution is 1440x480P 59.94hz\n");
	if (u4Res & SINK_480P_2880)
		ELOG("[RX]Native resolution is 2880x480P 59.94hz\n");
	if (u4Res & SINK_720P60)
		ELOG("[RX]Native resolution is 1280x720P 59.94hz\n");
	if (u4Res & SINK_1080I60)
		ELOG("[RX]Native resolution is 1920x1080I 59.94hz\n");
	if (u4Res & SINK_1080P60)
		ELOG("[RX]Native resolution is 1920x1080P 59.94hz\n");
	if (u4Res & SINK_576I)
		ELOG("[RX]Native resolution is 1440x576I 50hz\n");
	if (u4Res & SINK_576I_1440)
		ELOG("[RX]Native resolution is 2880x576I 50hz\n");
	if (u4Res & SINK_576P)
		ELOG("[RX]Native resolution is 720x576P 50hz\n");
	if (u4Res & SINK_576P_1440)
		ELOG("[RX]Native resolution is 1440x576P 50hz\n");
	if (u4Res & SINK_576P_2880)
		ELOG("[RX]Native resolution is 2880x576P 50hz\n");
	if (u4Res & SINK_720P50)
		ELOG("[RX]Native resolution is 1280x720P 50hz\n");
	if (u4Res & SINK_1080I50)
		ELOG("[RX]Native resolution is 1920x1080I 50hz\n");
	if (u4Res & SINK_1080P50)
		ELOG("[RX]Native resolution is 1920x1080P 50hz\n");
	if (u4Res & SINK_1080P30)
		ELOG("[RX]Native resolution is 1920x1080P 30hz\n");
	if (u4Res & SINK_1080P24)
		ELOG("[RX]Native resolution is 1920x1080P 24hz\n");
	if (u4Res & SINK_1080P25)
		ELOG("[RX]Native resolution is 1920x1080P 25hz\n");
	u4Res = SCP->s_hdmi_4k2kvic;
	if (u4Res & SINK_2160P_23_976HZ)
		ELOG("[RX]4k2k support to SINK_2160P_23_976HZ\n");
	if (u4Res & SINK_2160P_24HZ)
		ELOG("[RX]4k2k support to SINK_2160P_24HZ\n");
	if (u4Res & SINK_2160P_25HZ)
		ELOG("[RX]4k2k support to SINK_2160P_25HZ\n");
	if (u4Res & SINK_2160P_29_97HZ)
		ELOG("[RX]4k2k support to SINK_2160P_29_97HZ\n");
	if (u4Res & SINK_2160P_30HZ)
		ELOG("[RX]4k2k support to SINK_2160P_30HZ\n");
	if (u4Res & SINK_2161P_23_976HZ)
		ELOG("[RX]4k2k support to SINK_2161P_23_976HZ\n");
	if (u4Res & SINK_2161P_24HZ)
		ELOG("[RX]4k2k support to SINK_2161P_24HZ\n");
	if (u4Res & SINK_2161P_25HZ)
		ELOG("[RX]4k2k support to SINK_2161P_25HZ\n");
	if (u4Res & SINK_2161P_29_97HZ)
		ELOG("[RX]4k2k support to SINK_2161P_29_97HZ\n");
	if (u4Res & SINK_2161P_30HZ)
		ELOG("[RX]4k2k support to SINK_2161P_30HZ\n");
	if (u4Res & SINK_2160P_50HZ)
		ELOG("[RX]4k2k support to SINK_2160P_50HZ\n");
	if (u4Res & SINK_2160P_60HZ)
		ELOG("[RX]4k2k support to SINK_2160P_60HZ\n");
	if (u4Res & SINK_2161P_50HZ)
		ELOG("[RX]4k2k support to SINK_2161P_50HZ\n");
	if (u4Res & SINK_2161P_60HZ)
		ELOG("[RX]4k2k support to SINK_2161P_60HZ\n");
	ELOG("[RX]SUPPORT RGB\n");
	if (SCP->s_colorimetry & SINK_YCBCR_444)
		ELOG("[RX]SUPPORT YCBCR 444\n");
	if (SCP->s_colorimetry & SINK_YCBCR_422)
		ELOG("[RX]SUPPORT YCBCR 422\n");
	if (SCP->s_colorimetry & SINK_YCBCR_420)
		ELOG("[RX]SUPPORT YCBCR ONLY 420, 0x%x\n",
			SCP->s_4k2kvic_420_vdb);
	if (SCP->s_colorimetry & SINK_YCBCR_420_CAPABILITY)
		ELOG("[RX]SUPPORT YCBCR 420 and other colorspace, 0x%x\n",
			SCP->s_4k2kvic_420_vdb);
	if (SCP->s_colorimetry & SINK_XV_YCC709)
		ELOG("[RX]SUPPORT xvYCC 709\n");
	if (SCP->s_colorimetry & SINK_XV_YCC601)
		ELOG("[RX]SUPPORT xvYCC 601\n");
	if (SCP->s_colorimetry & SINK_S_YCC601)
		ELOG("[RX]SINK_S_YCC601\n");
	if (SCP->s_colorimetry & SINK_ADOBE_YCC601)
		ELOG("[RX]SINK_ADOBE_YCC601\n");
	if (SCP->s_colorimetry & SINK_ADOBE_RGB)
		ELOG("[RX]SINK_ADOBE_RGB\n");
	if (SCP->s_colorimetry & SINK_COLOR_SPACE_BT2020_CYCC)
		ELOG("[RX]SINK_COLOR_SPACE_BT2020_CYCC\n");
	if (SCP->s_colorimetry & SINK_COLOR_SPACE_BT2020_YCC)
		ELOG("[RX]SINK_COLOR_SPACE_BT2020_YCC\n");
	if (SCP->s_colorimetry & SINK_COLOR_SPACE_BT2020_RGB)
		ELOG("[RX]SINK_COLOR_SPACE_BT2020_RGB\n");
	if (SCP->s_colorimetry & SINK_METADATA0)
		ELOG("[RX]SUPPORT metadata P0\n");
	if (SCP->s_colorimetry & SINK_METADATA1)
		ELOG("[RX]SUPPORT metadata P1\n");
	if (SCP->s_colorimetry & SINK_METADATA2)
		ELOG("[RX]SUPPORT metadata P2\n");
	if (SCP->s_ycc_color_bit & HDMI_SINK_DEEP_COLOR_10_BIT)
		ELOG("[RX]SUPPORT YCBCR 30 Bits Deep Color\n");
	if (SCP->s_ycc_color_bit & HDMI_SINK_DEEP_COLOR_12_BIT)
		ELOG("[RX]SUPPORT YCBCR 36 Bits Deep Color\n");
	if (SCP->s_ycc_color_bit & HDMI_SINK_DEEP_COLOR_16_BIT)
		ELOG("[RX]SUPPORT YCBCR 48 Bits Deep Color\n");
	if (SCP->s_ycc_color_bit == HDMI_SINK_NO_DEEP_COLOR)
		ELOG("[RX]Not SUPPORT YCBCR Deep Color\n");
	if (SCP->s_dc420_color_bit & HDMI_SINK_DEEP_COLOR_16_BIT)
		ELOG("[RX]SUPPORT YCBCR420 48 Bits Deep Color\n");
	if (SCP->s_dc420_color_bit & HDMI_SINK_DEEP_COLOR_12_BIT)
		ELOG("[RX]SUPPORT YCBCR420 36 Bits Deep Color\n");
	if (SCP->s_dc420_color_bit & HDMI_SINK_DEEP_COLOR_10_BIT)
		ELOG("[RX]SUPPORT YCBCR420 30 Bits Deep Color\n");
	if (SCP->s_dc420_color_bit == HDMI_SINK_NO_DEEP_COLOR)
		ELOG("[RX]Not SUPPORT YCBCR420 Deep Color\n");
	if (SCP->s_rgb_color_bit & HDMI_SINK_DEEP_COLOR_10_BIT)
		ELOG("[RX]SUPPORT RGB 30 Bits Deep Color\n");
	if (SCP->s_rgb_color_bit & HDMI_SINK_DEEP_COLOR_12_BIT)
		ELOG("[RX]SUPPORT RGB 36 Bits Deep Color\n");
	if (SCP->s_rgb_color_bit & HDMI_SINK_DEEP_COLOR_16_BIT)
		ELOG("[RX]SUPPORT RGB 48 Bits Deep Color\n");
	if (SCP->s_rgb_color_bit == HDMI_SINK_NO_DEEP_COLOR)
		ELOG("[RX]Not SUPPORT RGB Deep Color\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_LPCM)
		ELOG("[RX]SUPPORT LPCM\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_AC3)
		ELOG("[RX]SUPPORT AC3 Decode\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_MPEG1)
		ELOG("[RX]SUPPORT MPEG1 Decode\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_MP3)
		ELOG("[RX]SUPPORT AC3 Decode\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_MPEG2)
		ELOG("[RX]SUPPORT MPEG2 Decode\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_AAC)
		ELOG("[RX]SUPPORT AAC Decode\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_DTS)
		ELOG("[RX]SUPPORT DTS Decode\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_ATRAC)
		ELOG("[RX]SUPPORT ATRAC Decode\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_DSD)
		ELOG("[RX]SUPPORT SACD DSD Decode\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_DOVI_PLUS)
		ELOG("[RX]SUPPORT Dovi Plus Decode\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_DTS_HD)
		ELOG("[RX]SUPPORT DTS HD Decode\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_MAT_MLP)
		ELOG("[RX]SUPPORT MAT MLP Decode\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_DST)
		ELOG("[RX]SUPPORT SACD DST Decode\n");
	if (SCP->s_aud_dec & HDMI_SINK_AUDIO_DEC_WMA)
		ELOG("[RX]SUPPORT  WMA Decode\n");

	if (SCP->s_dts_ch_samp[0] != 0) {
		memcpy(&DSP[0], "       ", 7);
		ELOG("[RX]SUPPORT DTS Max 2CH, Fs is:");
		for (bInx = 0; bInx < 7; bInx++) {
			if ((SCP->s_dts_ch_samp[0] >> bInx) & 0x01) {
				memcpy(&DSP[0], &_cDtsFsStr[bInx][0], 7);
				ELOG("%s,", &DSP[0]);
			}
		}
		ELOG("\n");
	}
	if (SCP->s_dts_ch_samp[4] != 0) {
		memcpy(&DSP[0], "       ", 7);
		ELOG("[RX]SUPPORT DTS Max 6CH, Fs is:");
		for (bInx = 0; bInx < 7; bInx++) {
			if ((SCP->s_dts_ch_samp[4] >> bInx) & 0x01) {
				memcpy(&DSP[0], &_cDtsFsStr[bInx][0], 7);
				ELOG("%s,", &DSP[0]);
			}
		}
		ELOG("\n");
	}
	if (SCP->s_dts_ch_samp[5] != 0) {
		memcpy(&DSP[0], "       ", 7);
		ELOG("[RX]SUPPORT DTS Max 7CH, Fs is:");
		for (bInx = 0; bInx < 7; bInx++) {
			if ((SCP->s_dts_ch_samp[5] >> bInx) & 0x01) {
				memcpy(&DSP[0], &_cDtsFsStr[bInx][0], 7);
				ELOG("%s,", &DSP[0]);
			}
		}
		ELOG("\n");
	}
	if (SCP->s_dts_ch_samp[6] != 0) {
		memcpy(&DSP[0], "       ", 7);
		ELOG("[RX]SUPPORT DTS Max 8CH, Fs is:");
		for (bInx = 0; bInx < 7; bInx++) {
			if ((SCP->s_dts_ch_samp[6] >> bInx) & 0x01) {
				memcpy(&DSP[0], &_cDtsFsStr[bInx][0], 7);
				ELOG("%s,", &DSP[0]);
			}
		}
		ELOG("\n");
	}
	if (SCP->s_pcm_ch_samp[0] != 0) {
		memcpy(&DSP[0], "       ", 7);
		ELOG("[RX]SUPPORT PCM Max 2CH, Fs is:");
		for (bInx = 0; bInx < 7; bInx++) {
			if ((SCP->s_pcm_ch_samp[0] >> bInx) & 0x01) {
				memcpy(&DSP[0], &_cDtsFsStr[bInx][0], 7);
				ELOG("%s,", &DSP[0]);
			}
		}
		ELOG("\n");
	}
	if (SCP->s_pcm_ch_samp[4] != 0) {
		memcpy(&DSP[0], "       ", 7);
		ELOG("[RX]SUPPORT PCM Max 6CH, Fs is:");
		for (bInx = 0; bInx < 7; bInx++) {
			if ((SCP->s_pcm_ch_samp[4] >> bInx) & 0x01) {
				memcpy(&DSP[0], &_cDtsFsStr[bInx][0], 7);
				ELOG("%s,", &DSP[0]);
			}
		}
		ELOG("\n");
	}
	if (SCP->s_pcm_ch_samp[5] != 0) {
		memcpy(&DSP[0], "       ", 7);
		ELOG("[RX]SUPPORT PCM Max 7CH, Fs is:");
		for (bInx = 0; bInx < 7; bInx++) {
			if ((SCP->s_pcm_ch_samp[5] >> bInx) & 0x01) {
				memcpy(&DSP[0], &_cDtsFsStr[bInx][0], 7);
				ELOG("%s,", &DSP[0]);
			}
		}
		ELOG("\n");
	}
	if (SCP->s_pcm_ch_samp[6] != 0) {
		memcpy(&DSP[0], "       ", 7);
		ELOG("[RX]SUPPORT PCM Max 8CH, Fs is:");
		for (bInx = 0; bInx < 7; bInx++) {
			if ((SCP->s_pcm_ch_samp[6] >> bInx) & 0x01) {
				memcpy(&DSP[0], &_cDtsFsStr[bInx][0], 7);
				ELOG("%s,", &DSP[0]);
			}
		}
		ELOG("\n");
	}

	if (SCP->s_spk_allocation & SINK_AUDIO_FL_FR)
		ELOG("[RX]Speaker FL/FR allocated\n");
	if (SCP->s_spk_allocation & SINK_AUDIO_LFE)
		ELOG("[RX]Speaker LFE allocated\n");
	if (SCP->s_spk_allocation & SINK_AUDIO_FC)
		ELOG("[RX]Speaker FC allocated\n");
	if (SCP->s_spk_allocation & SINK_AUDIO_RL_RR)
		ELOG("[RX]Speaker RL/RR allocated\n");
	if (SCP->s_spk_allocation & SINK_AUDIO_RC)
		ELOG("[RX]Speaker RC allocated\n");
	if (SCP->s_spk_allocation & SINK_AUDIO_FLC_FRC)
		ELOG("[RX]Speaker FLC/FRC allocated\n");
	if (SCP->s_spk_allocation & SINK_AUDIO_RLC_RRC)
		ELOG("[RX]Speaker RLC/RRC allocated\n");
	ELOG("[RX]HDMI edid support content type =%x\n",
		SCP->s_content_cnc);
	ELOG("[RX]Lip Sync Progressive audio latency = %d\n",
		SCP->s_p_audio_latency);
	ELOG("[RX]Lip Sync Progressive video latency = %d\n",
		SCP->s_p_video_latency);
	if (SCP->s_i_latency_present) {
		ELOG("[RX]Lip Sync Interlace audio latency = %d\n",
			SCP->s_i_audio_latency);
		ELOG("[RX]Lip Sync Interlace video latency = %d\n",
			SCP->s_i_vid_latency);
	}
	if (SCP->s_supp_ai == 1)
		ELOG("[RX]Support AI\n");
	else
		ELOG("[RX]Not Support AI\n");
	ELOG("[RX]Monitor Max horizontal size = %d\n",
		SCP->s_Disp_H_Size);
	ELOG("[RX]Monitor Max vertical size = %d\n",
		SCP->s_Disp_V_Size);
	if (SCP->s_vid_present == TRUE)
		ELOG("[RX]HDMI_Video_Present\n");
	else
		ELOG("[RX]No HDMI_Video_Present\n");
	if (SCP->s_3D_present == TRUE) {
		ELOG("[RX]3D_present\n");
		if (((SCP->s_3D_struct) & (0x01 << 0)) == (0x01 << 0)) {
			/*support Frame Packet */
			ELOG("[RX]3D_Structure = Frame Packet\n");
			u4Res = SCP->s_cea_FP_3D;
			if (u4Res & SINK_480I)
				ELOG("[RX]SUPPORT 1440x480I 59.94hz\n");
			if (u4Res & SINK_480I_1440)
				ELOG("[RX]SUPPORT 2880x480I 59.94hz\n");
			if (u4Res & SINK_480P)
				ELOG("[RX]SUPPORT 720x480P 59.94hz\n");
			if (u4Res & SINK_480P_1440)
				ELOG("[RX]SUPPORT 1440x480P 59.94hz\n");
			if (u4Res & SINK_480P_2880)
				ELOG("[RX]SUPPORT 2880x480P 59.94hz\n");
			if (u4Res & SINK_720P60)
				ELOG("[RX]SUPPORT 1280x720P 59.94hz\n");
			if (u4Res & SINK_1080I60)
				ELOG("[RX]SUPPORT 1920x1080I 59.94hz\n");
			if (u4Res & SINK_1080P60)
				ELOG("[RX]SUPPORT 1920x1080P 59.94hz\n");
			if (u4Res & SINK_1080P23976)
				ELOG("[RX]SUPPORT 1920x1080P 23.97hz\n");
			if (u4Res & SINK_1080P2997)
				ELOG("[RX]SUPPORT 1920x1080P 29.97hz\n");
			if (u4Res & SINK_576I)
				ELOG("[RX]SUPPORT 1440x576I 50hz\n");
			if (u4Res & SINK_576I_1440)
				ELOG("[RX]SUPPORT 2880x576I 50hz\n");
			if (u4Res & SINK_576P)
				ELOG("[RX]SUPPORT 720x576P 50hz\n");
			if (u4Res & SINK_576P_1440)
				ELOG("[RX]SUPPORT 1440x576P 50hz\n");
			if (u4Res & SINK_576P_2880)
				ELOG("[RX]SUPPORT 2880x576P 50hz\n");
			if (u4Res & SINK_720P50)
				ELOG("[RX]SUPPORT 1280x720P 50hz\n");
			if (u4Res & SINK_1080I50)
				ELOG("[RX]SUPPORT 1920x1080I 50hz\n");
			if (u4Res & SINK_1080P50)
				ELOG("[RX]SUPPORT 1920x1080P 50hz\n");
			if (u4Res & SINK_1080P30)
				ELOG("[RX]SUPPORT 1920x1080P 30hz\n");
			if (u4Res & SINK_1080P24)
				ELOG("[RX]SUPPORT 1920x1080P 24hz\n");
			if (u4Res & SINK_1080P25)
				ELOG("[RX]SUPPORT 1920x1080P 25hz\n");
		}
		if (((SCP->s_3D_struct) & (0x01 << 6)) == (0x01 << 6)) {
			// support Top and Bottom
			ELOG("[RX]3D_Structure = T&B\n");
			u4Res = SCP->s_cea_TOB_3D;
			if (u4Res & SINK_480I)
				ELOG("[RX]SUPPORT 1440x480I 59.94hz\n");
			if (u4Res & SINK_480I_1440)
				ELOG("[RX]SUPPORT 2880x480I 59.94hz\n");
			if (u4Res & SINK_480P)
				ELOG("[RX]SUPPORT 720x480P 59.94hz\n");
			if (u4Res & SINK_480P_1440)
				ELOG("[RX]SUPPORT 1440x480P 59.94hz\n");
			if (u4Res & SINK_480P_2880)
				ELOG("[RX]SUPPORT 2880x480P 59.94hz\n");
			if (u4Res & SINK_720P60)
				ELOG("[RX]SUPPORT 1280x720P 59.94hz\n");
			if (u4Res & SINK_1080I60)
				ELOG("[RX]SUPPORT 1920x1080I 59.94hz\n");
			if (u4Res & SINK_1080P60)
				ELOG("[RX]SUPPORT 1920x1080P 59.94hz\n");
			if (u4Res & SINK_1080P23976)
				ELOG("[RX]SUPPORT 1920x1080P 23.97hz\n");
			if (u4Res & SINK_1080P2997)
				ELOG("[RX]SUPPORT 1920x1080P 29.97hz\n");
			if (u4Res & SINK_576I)
				ELOG("[RX]SUPPORT 1440x576I 50hz\n");
			if (u4Res & SINK_576I_1440)
				ELOG("[RX]SUPPORT 2880x576I 50hz\n");
			if (u4Res & SINK_576P)
				ELOG("[RX]SUPPORT 720x576P 50hz\n");
			if (u4Res & SINK_576P_1440)
				ELOG("[RX]SUPPORT 1440x576P 50hz\n");
			if (u4Res & SINK_576P_2880)
				ELOG("[RX]SUPPORT 2880x576P 50hz\n");
			if (u4Res & SINK_720P50)
				ELOG("[RX]SUPPORT 1280x720P 50hz\n");
			if (u4Res & SINK_1080I50)
				ELOG("[RX]SUPPORT 1920x1080I 50hz\n");
			if (u4Res & SINK_1080P50)
				ELOG("[RX]SUPPORT 1920x1080P 50hz\n");
			if (u4Res & SINK_1080P30)
				ELOG("[RX]SUPPORT 1920x1080P 30hz\n");
			if (u4Res & SINK_1080P24)
				ELOG("[RX]SUPPORT 1920x1080P 24hz\n");
			if (u4Res & SINK_1080P25)
				ELOG("[RX]SUPPORT 1920x1080P 25hz\n");
		}
		if (((SCP->s_3D_struct) & (0x0001 << 8)) == (0x0001 << 8)) {
			ELOG("[RX]3D_Structure = Side by Side\n");
			u4Res = SCP->s_cea_SBS_3D;
			if (u4Res & SINK_480I)
				ELOG("[RX]SUPPORT 1440x480I 59.94hz\n");
			if (u4Res & SINK_480I_1440)
				ELOG("[RX]SUPPORT 2880x480I 59.94hz\n");
			if (u4Res & SINK_480P)
				ELOG("[RX]SUPPORT 720x480P 59.94hz\n");
			if (u4Res & SINK_480P_1440)
				ELOG("[RX]SUPPORT 1440x480P 59.94hz\n");
			if (u4Res & SINK_480P_2880)
				ELOG("[RX]SUPPORT 2880x480P 59.94hz\n");
			if (u4Res & SINK_720P60)
				ELOG("[RX]SUPPORT 1280x720P 59.94hz\n");
			if (u4Res & SINK_1080I60)
				ELOG("[RX]SUPPORT 1920x1080I 59.94hz\n");
			if (u4Res & SINK_1080P60)
				ELOG("[RX]SUPPORT 1920x1080P 59.94hz\n");
			if (u4Res & SINK_1080P23976)
				ELOG("[RX]SUPPORT 1920x1080P 23.97hz\n");
			if (u4Res & SINK_1080P2997)
				ELOG("[RX]SUPPORT 1920x1080P 29.97hz\n");
			if (u4Res & SINK_576I)
				ELOG("[RX]SUPPORT 1440x576I 50hz\n");
			if (u4Res & SINK_576I_1440)
				ELOG("[RX]SUPPORT 2880x576I 50hz\n");
			if (u4Res & SINK_576P)
				ELOG("[RX]SUPPORT 720x576P 50hz\n");
			if (u4Res & SINK_576P_1440)
				ELOG("[RX]SUPPORT 1440x576P 50hz\n");
			if (u4Res & SINK_576P_2880)
				ELOG("[RX]SUPPORT 2880x576P 50hz\n");
			if (u4Res & SINK_720P50)
				ELOG("[RX]SUPPORT 1280x720P 50hz\n");
			if (u4Res & SINK_1080I50)
				ELOG("[RX]SUPPORT 1920x1080I 50hz\n");
			if (u4Res & SINK_1080P50)
				ELOG("[RX]SUPPORT 1920x1080P 50hz\n");
			if (u4Res & SINK_1080P30)
				ELOG("[RX]SUPPORT 1920x1080P 30hz\n");
			if (u4Res & SINK_1080P24)
				ELOG("[RX]SUPPORT 1920x1080P 24hz\n");
			if (u4Res & SINK_1080P25)
				ELOG("[RX]SUPPORT 1920x1080P 25hz\n");
		}
	} else {
		ELOG("[RX]No 3D_present\n");
	}

	if (SCP->s_hf_vsdb_info & EDID_HF_VSDB_SCDC_PRESENT)
		ELOG("[RX]EDID_HF_VSDB_SCDC_PRESENT EXIST\n");
	if (SCP->s_hf_vsdb_info & EDID_HF_VSDB_RR_CAPABLE)
		ELOG("[RX]EDID_HF_VSDB_RR_CAPABLE EXIST\n");
	if (SCP->s_hf_vsdb_info & EDID_HF_VSDB_LTE_340_SCR)
		ELOG("[RX]EDID_HF_VSDB_LTE_340_SCR EXIST\n");
	if (SCP->s_hf_vsdb_info & EDID_HF_VSDB_INDEPENDENT_VIEW)
		ELOG("[RX]EDID_HF_VSDB_INDEPENDENT_VIEW EXIST\n");
	if (SCP->s_hf_vsdb_info & EDID_HF_VSDB_DUAL_VIEW)
		ELOG("[RX]EDID_HF_VSDB_DUAL_VIEW EXIST\n");
	if (SCP->s_hf_vsdb_info & EDID_HF_VSDB_3D_OSD_DISPARITY)
		ELOG("[RX]EDID_HF_VSDB_3D_OSD_DISPARITY EXIST\n");
	if (SCP->s_supp_dynamic_hdr & EDID_SUPPORT_PHILIPS_HDR)
		ELOG("[RX]EDID_SUPPORT_PHILIPS_HDR\n");
	if (SCP->s_supp_dynamic_hdr & EDID_SUPPORT_DV_HDR)
		ELOG("[RX]EDID_SUPPORT_DV_HDR\n");
	if (SCP->s_supp_dynamic_hdr & EDID_SUPPORT_YUV422_12BIT)
		ELOG("[RX]EDID_SUPPORT_YUV422_12BIT\n");
	if (SCP->s_supp_dynamic_hdr & EDID_SUPPORT_DV_HDR_2160P60)
		ELOG("[RX]EDID_SUPPORT_DV_HDR_2160P60\n");
	if (SCP->s_supp_static_hdr & EDID_SUPPORT_SDR)
		ELOG("[RX]EDID_SUPPORT_SDR\n");
	if (SCP->s_supp_static_hdr & EDID_SUPPORT_HDR)
		ELOG("[RX]EDID_SUPPORT_HDR\n");
	if (SCP->s_supp_static_hdr & EDID_SUPPORT_SMPTE_ST_2084)
		ELOG("[RX]EDID_SUPPORT_SMPTE_ST_2084\n");
	if (SCP->s_supp_static_hdr & EDID_SUPPORT_FUTURE_EOTF)
		ELOG("[RX]EDID_SUPPORT_FUTURE_EOTF\n");
	if (SCP->s_supp_static_hdr & EDID_SUPPORT_ET_4)
		ELOG("[RX]EDID_SUPPORT_ET_4\n");
	if (SCP->s_supp_static_hdr & EDID_SUPPORT_ET_5)
		ELOG("[RX]EDID_SUPPORT_ET_5\n");
	if (SCP->s_dbvision_vsvdb_low_latency_supp)
		ELOG("[RX]EDID_SUPPORT_LOWLATENCY\n");
	ELOG("[RX]VSVDB version=0x%x\n", SCP->s_dbvision_vsvdb_ver);
	ELOG("[RX]VSVDB vsvdb v2 interface=0x%x\n",
		SCP->s_dbvision_vsvdb_v2_interface);
	ELOG("[RX]VSVDB vsvdb_v1_low_latency=0x%x\n",
		SCP->s_dbvision_vsvdb_v1_low_latency);
	ELOG("[RX]VSVDB low_latency_support=0x%x\n",
		SCP->s_dbvision_vsvdb_low_latency_supp);
	ELOG("[RX]VSVDB vsvdb_v2_supports_10b_12b_444;=0x%x\n",
		SCP->s_dbvision_vsvdb_v2_supp_10b12b_444);
	ELOG("[RX]VSVDB vsvdb support backlight control=0x%x\n",
		SCP->s_dbvision_vsvdb_supp_backlight_ctrl);
	ELOG("[RX]s_hdr_content_max_luma = 0x%x\n",
		SCP->s_hdr_content_max_luma);
	ELOG("[RX]s_hdr_content_max_frame_avg_luma = 0x%x\n",
		SCP->s_hdr_content_max_frame_avg_luma);
	ELOG("[RX]s_hdr_content_min_luma = 0x%x\n",
		SCP->s_hdr_content_min_luma);
	ELOG("[RX]HDR block=0x%x;0x%x;0x%x;0x%x;0x%x;0x%x;0x%x\n",
		SCP->s_hdr_block[0],
		SCP->s_hdr_block[1],
		SCP->s_hdr_block[2],
		SCP->s_hdr_block[3],
		SCP->s_hdr_block[4],
		SCP->s_hdr_block[5],
		SCP->s_hdr_block[6]);
	ELOG("[RX]HFVSDB block=0x%x;0x%x;0x%x;0x%x;0x%x;0x%x;0x%x;0x%x\n",
		SCP->s_hfvsdb_blk[0],
		SCP->s_hfvsdb_blk[1],
		SCP->s_hfvsdb_blk[2],
		SCP->s_hfvsdb_blk[3],
		SCP->s_hfvsdb_blk[4],
		SCP->s_hfvsdb_blk[5],
		SCP->s_hfvsdb_blk[6],
		SCP->s_hfvsdb_blk[7]);
	pMonitorName = (char *)(SCP->_bMonitorName);
	ELOG("[RX]_bMonitorName = %s\n", pMonitorName);
	string[2] = (char)((SCP->s_ID_manu_name & 0x1f) + 0x40);
	string[1] = (char)((SCP->s_ID_manu_name >> 5 & 0x1f) + 0x40);
	string[0] = (char)((SCP->s_ID_manu_name >> 10 & 0x1f) + 0x40);
	ELOG("[RX]ID_manufacturer_name = 0x%x : %s maker\n",
		SCP->s_ID_manu_name, string);
	ELOG("[RX]s_max_tmds_char_rate = 0x%x\n",
		SCP->s_max_tmds_char_rate);
	ELOG("[RX]s_hf_vsdb_info = 0x%x\n",
		SCP->s_hf_vsdb_info);
}

void
ClearEdidInfo(struct MTK_HDMI *myhdmi)
{
	u8 bInx;

	memset(EDP, 0, EDID_SIZE);
	vSetSI(myhdmi, SI_EDID_RESULT, FALSE);
	vSetSI(myhdmi, SI_VSDB_EXIST, FALSE);
	SCP->s_supp_hdmi_mode = FALSE;
	SCP->s_dtd_ntsc_res = 0;
	SCP->s_dtd_pal_res = 0;
	SCP->s_1st_dtd_ntsc_res = 0;
	SCP->s_1st_dtd_pal_res = 0;
	SCP->s_colorimetry = 0;
	SCP->s_cea_ntsc_res = 0;
	SCP->s_cea_pal_res = 0;
	SCP->s_org_cea_ntsc_res = 0;
	SCP->s_org_cea_pal_res = 0;
	SCP->s_native_ntsc_res = 0;
	SCP->s_native_pal_res = 0;
	SCP->s_vcdb_data = 0;
	SCP->s_aud_dec = 0;
	SCP->s_dsd_ch_num = 0;
	SCP->s_dts_fs = 0;
	for (bInx = 0; bInx < 7; bInx++) {
		if (bInx == 0)
			SCP->s_pcm_ch_samp[bInx] = 0x07;
		else
			SCP->s_pcm_ch_samp[bInx] = 0;
		SCP->s_dsd_ch_samp[bInx] = 0;
		SCP->s_dst_ch_samp[bInx] = 0;
		SCP->s_org_pcm_ch_samp[bInx] = 0;
		SCP->s_org_pcm_bit_size[bInx] = 0;
		SCP->s_dts_ch_samp[bInx] = 0;
	}
	for (bInx = 0; bInx < 7; bInx++) {
		if (bInx == 0)
			SCP->s_pcm_bit_size[bInx] = 0x07;
		else
			SCP->s_pcm_bit_size[bInx] = 0;
	}
	SCP->s_spk_allocation = 0;
	SCP->s_p_latency_present = 0;
	SCP->s_i_latency_present = 0;
	SCP->s_p_latency_present = 0;
	SCP->s_p_audio_latency = 0;
	SCP->s_p_video_latency = 0;
	SCP->s_i_audio_latency = 0;
	SCP->s_i_vid_latency = 0;
	SCP->s_vid_present = 0;
	SCP->s_rgb_color_bit = HDMI_SINK_NO_DEEP_COLOR;
	SCP->s_ycc_color_bit = HDMI_SINK_NO_DEEP_COLOR;
	SCP->edid_chksum_and_aud_sup =
		(SINK_BASIC_AUDIO_NO_SUP |
		SINK_SAD_NO_EXIST |
		SINK_BASE_BLK_CHKSUM_ERR |
		SINK_EXT_BLK_CHKSUM_ERR);
	SCP->s_cec_addr = 0xffff;
	SCP->s_edid_ready = FALSE;
	myhdmi->_u4SinkProductID = 0;
	SCP->s_supp_ai = 0;
	SCP->s_Disp_H_Size = 0;
	SCP->s_Disp_V_Size = 0;
	SCP->s_CNC = 0;

	SCP->s_ID_manu_name = 0;
	SCP->s_ID_product_code = 0;
	SCP->s_ID_serial_num = 0;
	SCP->s_week_of_manu = 0;
	SCP->s_year_of_manu = 0;

	// add end
	SCP->s_3D_present = FALSE;
	SCP->s_cea_3D_res = 0;
	SCP->s_3D_struct = 0;
	SCP->s_cea_FP_3D = 0;
	SCP->s_cea_TOB_3D = 0;
	SCP->s_cea_SBS_3D = 0;
	SCP->s_hdmi_4k2kvic = 0;
	SCP->s_4k2kvic_420_vdb = 0;
	SCP->s_SCDC_present = 0;
	SCP->s_LTE_340M_sramble = 0;
	SCP->s_supp_static_hdr = 0;
	SCP->s_supp_dynamic_hdr = 0;
	SCP->s_hf_vsdb_exist = 0;
	SCP->s_max_tmds_char_rate = 0;
	SCP->s_hf_vsdb_info = 0;
	SCP->s_dc420_color_bit = 0;
	SCP->s_hdr_content_max_luma = 0;
	SCP->s_hdr_content_max_frame_avg_luma = 0;
	SCP->s_hdr_content_min_luma = 0;
	SCP->s_dbvision_vsvdb_len = 0;
	SCP->s_dbvision_vsvdb_ver = 0;
	SCP->s_dbvision_vsvdb_v1_low_latency = 0;
	SCP->s_dbvision_vsvdb_v2_interface = 0;
	SCP->s_dbvision_vsvdb_low_latency_supp = 0;
	SCP->s_dbvision_vsvdb_v2_supp_10b12b_444 = 0;
	SCP->s_dbvision_vsvdb_supp_backlight_ctrl = 0;
	SCP->s_dbvision_vsvdb_backlt_min_luma = 0;
	SCP->s_dbvision_vsvdb_tmin = 0;
	SCP->s_dbvision_vsvdb_tmax = 0;
	SCP->s_dbvision_vsvdb_tminPQ = 0;
	SCP->s_dbvision_vsvdb_tmaxPQ = 0;
	SCP->s_dbvision_vsvdb_Rx = 0;
	SCP->s_dbvision_vsvdb_Ry = 0;
	SCP->s_dbvision_vsvdb_Gx = 0;
	SCP->s_dbvision_vsvdb_Gy = 0;
	SCP->s_dbvision_vsvdb_Bx = 0;
	SCP->s_dbvision_vsvdb_By = 0;
	SCP->s_dbvision_vsvdb_Wx = 0;
	SCP->s_dbvision_vsvdb_Wy = 0;
	for (bInx = 0; bInx < 32; bInx++)
		SCP->s_dbvision_blk[bInx] = 0;
	for (bInx = 0; bInx < 7; bInx++)
		SCP->s_hdr_block[bInx] = 0;
	for (bInx = 0; bInx < 8; bInx++)
		SCP->s_hfvsdb_blk[bInx] = 0;
	for (bInx = 0; bInx < 13; bInx++)
		SCP->_bMonitorName[bInx] = 0;
	for (bInx = 0; bInx < 7; bInx++) {
		myhdmi->_u1DbgEdidPcmChFs[bInx] = 0x7f;
		myhdmi->_u1DbgEdidPcmChBitSize[bInx] = 0x07;
	}
	myhdmi->_u1DbgEdidSpkAllocation = 0x7f;
	myhdmi->_u2DbgEdidAudDec = 0x1 << 1;
	if (fgIsHdmiNoEDIDChk(myhdmi))
		SetNoEdidChkInfo(myhdmi);
}

void
vSetDebugEdidAudioPcm(struct MTK_HDMI *myhdmi,
	u8 u1ChNum,
	u8 u1Fs,
	u8 u1BitSize)
{
	if ((u1ChNum >= 2) && (u1ChNum <= 8)) {
		myhdmi->_u1DbgEdidPcmChFs[u1ChNum - 2] = u1Fs;
		myhdmi->_u1DbgEdidPcmChBitSize[u1ChNum - 2] = u1BitSize;
	}
}

void
vSetDebugEdidSpkAllocation(struct MTK_HDMI *myhdmi,
	u8 u1SpkAllocation)
{
	myhdmi->_u1DbgEdidSpkAllocation = u1SpkAllocation;
	ELOG("[RX]Change Speak allocation EDID=%x\n",
		myhdmi->_u1DbgEdidSpkAllocation);
}

void
vSetDebugEdidAudDecoder(struct MTK_HDMI *myhdmi,
	u16 u2AudDec)
{
	myhdmi->_u2DbgEdidAudDec = u2AudDec;
	ELOG("[RX]Change Audio Decoder EDID=%x\n",
		myhdmi->_u2DbgEdidAudDec);
}

void
vForceUpdateAudioPcmEdid(struct MTK_HDMI *myhdmi,
	bool fgUpdateOn)
{
	u8 u1Inx;

	if (fgUpdateOn) {
		for (u1Inx = 0; u1Inx < 7; u1Inx++) {
			SCP->s_pcm_ch_samp[u1Inx] =
				myhdmi->_u1DbgEdidPcmChFs[u1Inx];
			SCP->s_pcm_bit_size[u1Inx] =
				myhdmi->_u1DbgEdidPcmChBitSize[u1Inx];
		}
		SCP->s_spk_allocation =
			myhdmi->_u1DbgEdidSpkAllocation;
		SCP->s_aud_dec =
			myhdmi->_u2DbgEdidAudDec;
	}

	else {
		for (u1Inx = 0; u1Inx < 7; u1Inx++) {
			myhdmi->_u1DbgEdidPcmChFs[u1Inx] =
				SCP->s_pcm_ch_samp[u1Inx];
			myhdmi->_u1DbgEdidPcmChBitSize[u1Inx] =
				SCP->s_pcm_bit_size[u1Inx];
			myhdmi->_u1DbgEdidSpkAllocation =
				SCP->s_spk_allocation;
			myhdmi->_u2DbgEdidAudDec =
				SCP->s_aud_dec;
		}
	}
}

void
SetEdidChkErr(struct MTK_HDMI *myhdmi)
{
	u8 bInx;

	vSetSI(myhdmi, SI_EDID_RESULT, TRUE);
	vSetSI(myhdmi, SI_VSDB_EXIST, FALSE);
	SCP->s_supp_hdmi_mode = TRUE;
	SCP->s_dtd_ntsc_res = SINK_480P; // 0x1fffff;
	SCP->s_dtd_pal_res = SINK_576P;  // 0x1fffff;
	SCP->s_colorimetry = 0;
	SCP->s_cea_ntsc_res = 0;
	SCP->s_cea_pal_res = 0;
	SCP->s_org_cea_ntsc_res = 0;
	SCP->s_org_cea_pal_res = 0;
	SCP->s_native_ntsc_res = 0;
	SCP->s_native_pal_res = 0;
	SCP->s_vcdb_data = 0;
	SCP->s_aud_dec = 1; // PCM only
	SCP->s_dsd_ch_num = 0;
	SCP->s_dts_fs = 0;
	for (bInx = 0; bInx < 7; bInx++) {
		if (bInx == 0)
			SCP->s_pcm_ch_samp[bInx] = 0x07; // 2ch max 48khz
		else
			SCP->s_pcm_ch_samp[bInx] = 0x0;
		SCP->s_dsd_ch_samp[bInx] = 0;
		SCP->s_dst_ch_samp[bInx] = 0;
		SCP->s_dts_ch_samp[bInx] = 0;
	}
	for (bInx = 0; bInx < 7; bInx++) {
		if (bInx == 0)
			SCP->s_pcm_bit_size[bInx] = 0x07; // 2ch 24 bits
		else
			SCP->s_pcm_bit_size[bInx] = 0;
	}
	SCP->s_spk_allocation = 0;
	SCP->s_p_latency_present = 0;
	SCP->s_i_latency_present = 0;
	SCP->s_p_audio_latency = 0;
	SCP->s_p_video_latency = 0;
	SCP->s_i_audio_latency = 0;
	SCP->s_i_vid_latency = 0;
	SCP->s_vid_present = 0;
	SCP->s_rgb_color_bit = HDMI_SINK_NO_DEEP_COLOR;
	SCP->s_ycc_color_bit = HDMI_SINK_NO_DEEP_COLOR;
	SCP->edid_chksum_and_aud_sup =
		(SINK_BASIC_AUDIO_NO_SUP |
		SINK_SAD_NO_EXIST |
		SINK_BASE_BLK_CHKSUM_ERR |
		SINK_EXT_BLK_CHKSUM_ERR);
	SCP->s_cec_addr = 0xffff;
	SCP->s_edid_ready = FALSE;
	myhdmi->_u4SinkProductID = 0;
	SCP->s_supp_ai = 0;
	SCP->s_ID_manu_name = 0;
	SCP->s_ID_product_code = 0;
	SCP->s_ID_serial_num = 0;
	SCP->s_week_of_manu = 0;
	SCP->s_year_of_manu = 0;
	SCP->s_3D_present = FALSE;
	SCP->s_cea_3D_res = 0;
	SCP->s_3D_struct = 0;
	SCP->s_cea_FP_3D = 0;
	SCP->s_cea_TOB_3D = 0;
	SCP->s_cea_SBS_3D = 0;
	SCP->s_hdmi_4k2kvic = 0;
	SCP->s_4k2kvic_420_vdb = 0;
	SCP->s_SCDC_present = 0;
	SCP->s_LTE_340M_sramble = 0;
	for (bInx = 0; bInx < 13; bInx++)
		SCP->_bMonitorName[bInx] = 0;
}

bool
fgHdmiEdidModify(struct MTK_HDMI *myhdmi)
{
	u32 i = 0, k = 0;
	u8 HEADER[8] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};

	for (i = 0; i < 8; i++) {
		if (myhdmi->_bHdmiTestEdidData[i] == HEADER[i])
			k++;
	}
	if (k == 8)
		return TRUE;
	else
		return FALSE;
}

void
vCheckEDID(struct MTK_HDMI *myhdmi)
{
	u8 i;

	DBGLOG("[RX]%s\n", __func__);

	ClearEdidInfo(myhdmi);
	for (i = 0; i < 0x10; i++)
		FVP[i] = 0;
	myhdmi->First_16_N_VIC = 0;
	myhdmi->First_16_P_VIC = 0;
	myhdmi->_u4i_3D_VIC = 0;
	SCP->s_vid_present = FALSE;
	SCP->s_3D_present = FALSE;
	SCP->s_cea_3D_res = 0;
	SCP->s_3D_struct = 0;
	SCP->s_cea_FP_3D = 0;
	SCP->s_cea_TOB_3D = 0;
	SCP->s_cea_SBS_3D = 0;
	SCP->s_hdmi_4k2kvic = 0;
	SCP->s_4k2kvic_420_vdb = 0;

		if (fgReadEDID(myhdmi) == TRUE) {
			if (ParserEDID(myhdmi, &EDP[0]) == TRUE) {
				DBGLOG("[RX]EDID BASE OK\n");
				vSetSI(myhdmi, SI_EDID_RESULT, TRUE);
				SCP->s_edid_ready = TRUE;
		}
	}

	if ((i4SI(myhdmi, SI_EDID_EXT_BLK_NO) * E_BLK_LEN) < EDID_SIZE)
		// for Buffer Overflow error
		ParserExtEDIDState(myhdmi, &EDP[0]);
	myhdmi->_u4SinkProductID =
		((EDP[0x08]) |
		(EDP[0x09] << 8) |
		(EDP[0x0a] << 16) |
		(EDP[0x0b] << 24));
	SCP->s_ID_manu_name = ((EDP[0x09]) |
		(EDP[0x08] << 8));
	SCP->s_ID_product_code = ((EDP[0x0b]) |
		(EDP[0x0a] << 8));
	SCP->s_ID_serial_num =
		((EDP[0x0f]) |
		(EDP[0x0e] << 8) |
		(EDP[0x0d] << 16) |
		(EDP[0x0c] << 24));
	SCP->s_week_of_manu = EDP[0x10];
	SCP->s_year_of_manu = EDP[0x11];
	if (EDIDDBG)
		ShowEdidRawData(myhdmi);
	if (EDIDDBG)
		ShowEdidInfo(myhdmi);
	if (fgIsHdmiNoEDIDChk(myhdmi))
		SetNoEdidChkInfo(myhdmi);
	vForceUpdateAudioPcmEdid(myhdmi, FALSE);
	if (((SCP->s_dtd_ntsc_res & SINK_720P60) |
		(SCP->s_cea_ntsc_res & SINK_720P60)) == 0)
		SCP->s_cea_3D_res &= ~SINK_720P60;
	if (((SCP->s_dtd_pal_res & SINK_720P50) |
		(SCP->s_cea_pal_res & SINK_720P50)) == 0)
		SCP->s_cea_3D_res &= ~SINK_720P50;
}

u32
u4GetSinkProductId(struct MTK_HDMI *myhdmi)
{
	return myhdmi->_u4SinkProductID;
}
