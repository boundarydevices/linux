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

#define DEF_CEC_PA

#define ELOG(fmt, arg...) pr_info(fmt, ##arg)
#define EDIDDBG hdmi2_is_debug(myhdmi,\
	HDMI_RX_DEBUG_EDID)

#define DBGLOG(fmt, arg...) \
	do { \
		if (myhdmi->debugtype & HDMI_RX_DEBUG_EDID) \
			pr_info(fmt, ##arg); \
	} while (0)

u8 const Def_Edid_Blk0[128] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
	/* E_MANUFACTURER_ID */ 0x36, 0x8B,
	/* E_PRODUCT_ID */ 0x00, 0x00,
	/* E_SERIAL_NUMBER */ 0x00, 0x00, 0x00, 0x00,
	E_WEEK,
	E_YEAR,
	0x01, 0x03,
	0x80, 0x80, 0x50, 0x78, 0x0A,
	0xEE, 0x91, 0xA3, 0x54, 0x4C, 0x99, 0x26, 0x0F, 0x50, 0x54,
	0x20,			// 640 x 480 @ 60Hz (IBM,VGA)
	0x00,			// None Specified
	0x00,			// None Specified
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	//(36H-47H) Detailed Timing / Descriptor Block 1:
	// Table 69> 1280x720p (60Hz 16:9)
	0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20, 0x6E,
	0x28, 0x55, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E,
	//(48H-59H) Detailed Timing / Descriptor Block 2:
	// Table 78> 720x480p (60Hz 16:9)
	0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10,
	0x3E, 0x96, 0x00, 0x58, 0xC2, 0x21, 0x00, 0x00, 0x18,
	//(5AH-6BH) Detailed Timing / Descriptor Block 3:
	// Descriptor: Monitor Name string
	0x00, 0x00, 0x00, 0xFC, 0x00,	// header
	'B', 'D', '-', 'H', 'T', 'S', 0x0A, ' ', ' ', ' ', ' ', ' ', ' ',
	//(6CH-7DH) Detailed Timing / Descriptor Block 4:
	//         Monitor Range Limits:
	0x00, 0x00, 0x00, 0xFD, 0x00,	// header
	DEF_MIN_V_HZ,	// Min Vertical Freq
	DEF_MAX_V_HZ,	// Max Vertical Freq
	DEF_MIN_H_KHZ,	// Min Horiz. Freq
	DEF_MAX_H_KHZ,	// Max Horiz. Freq
	DEF_MAX_PIXCLK_10MHZ,	// Pixel Clock in 10MHz
	0x00,			// GTF - Not Used
	0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,	// padding
	//(7EH)    Number Of Extension Blocks
	//         Extended Flag = 1    //No Extension EDID Block(s)
	0x01,
	//(7FH)    Check Sum
	0x00			// not correct, but it is not important here
};

u8 const Def_Edid_Blk1_Header[4] = {
	0x02, 0x03, 0x25, 0x71
};

u8 Speaker_Data_Block[E_SPK_BLK_LEN] = {
	0x83,
	0x4F, 0x00, 0x00
};

u8 SpkDataBlk_2CH_ONLY[E_SPK_BLK_LEN] = {
	0x83,
	0x01, 0x00, 0x00
};

u8 const Def_420cmdb[5] = {
	0xe4, 0x0f, 0, 0, 0x03 //svd 17/18, 3840p50/60
};

u8 const Vid_Data_BLK[E_VID_BLK_LEN] = {
	0x4C,	// 12
	0x04, 0x90, /*native 0x10 */
	0x13, 0x1f,
	0x22,
	0x01, 0x02, 0x03,	// SD 60Hz timing
	0x11, 0x12,	// SD 50Hz timing
	95,
	97
};

u8 const Vid_Data_BLK_1080P[E_VID_BLK_LEN_1080P] = {
	0x4d,			// 13
	0x04, 0x90, /*native 0x10 */
	0x13, 0x1f,
	0x20, 0x22, 0x21, 0x3C,
	0x01, 0x02, 0x03, // SD 60Hz timing
	0x11, 0x12 // SD 50Hz timing
};

#ifdef E_SUPPORT_HD_AUDIO
u8 const
Audio_Data_Block[E_AUD_BLK_LEN] = {
	0x3B,
	0x09, 0x7f, 0x07,	//PCM 2CH, 32~192KHz, 16/20/24Bit
	0x0F, 0x7f, 0x07,	// PCM 8CH, 32~192KHz, 16/20/24Bit
	0x0D, 0x7f, 0x07,	// PCM 6CH, 32~192KHz, 16/20/24Bit
	0x15, 0x07, 0x50,	// AC3 5CH, 32~48KHz,  640Kbps
	0x3d, 0x07, 0xc0,	// DTS 5CH, 32~48KHz,  1536Kbps
	0x57, 0x04, 0x03, //DD+ 8ch, 48khz
	0x5f, 0x7e, 0x03,	// DTS-HD, 44.1~ 192 khz,
	0x67, 0x54, 0x03, //MAT 8ch, 48/96/192 khz, (DD TRUE HD)
	0x35, 0x07, 0x50,	// AAC 5CH, 32~48KHz,  640Kbps
};

u8 const
AudDataBlk_2CH_ONLY[E_AUD2CHPCM_ONLY_LEN] = {
	0x23,
	0x09, 0x7f, 0x07,	//PCM 2CH, 32~192KHz, 16/20/24Bit
	// 0x15, 0x07, 0x50,  //AC3 5CH, 32~48KHz,  640Kbps
	// 0x3d, 0x07, 0xc0,  //DTS 5CH, 32~48KHz,  1536Kbps
	// 0x4d, 0x02, 0x00,  //DSD 6ch, 44.1khz,
	// 0x57, 0x06, 0x00,   //DD+ 8ch, 44.1 ~ 48khz
	// 0x5f, 0x7e, 0x01,   //DTS-HD, 44.1~ 192 khz,
	// 0x67, 0x7e, 0x00,    //MAT 8ch, 44.1~ 192 khz,
	// 0x35, 0x07, 0x50,         //AAC 5CH, 32~48KHz,  640Kbps
};
#else
u8 const
Audio_Data_Block[E_AUD_BLK_LEN] = {
	0x2F,
	0x09, 0x7F, 0x07,	// PCM 2CH, 32~192KHz, 16/20/24Bit
	0x0D, 0x1F, 0x07,	// PCM 6CH, 32~96KHz, 16/20/24Bit
	0x15, 0x07, 0x50,	// AC3 6CH, 32~ 48K, 640Kbps
	0x3D, 0x07, 0xC0,	// DTS 6CH, 32~ 48K, 1536Kbps
	0x35, 0x07, 0x50,	// AAC 5CH, 32~48KHz,  640Kbps
};

u8 const
AudDataBlk_2CH_ONLY[E_AUD2CHPCM_ONLY_LEN] = {
	0x23,
	//0x09, 0x7F, 0x07,             //PCM 2CH, 32~192KHz, 16/20/24Bit
	0x09, 0x07, 0x07,	//PCM 2CH, 32~48KHz, 16/20/24Bit
	// 0x15, 0x07, 0x50,             //AC3 6CH, 32~ 48K, 640Kbps
	// 0x3D, 0x07, 0xC0,             //DTS 6CH, 32~ 48K, 1536Kbps
	// 0x35, 0x07, 0x50,         //AAC 5CH, 32~48KHz,  640Kbps
};
#endif

u8
VendDataBLK_Full[E_VEND_FULL_L] = {
	0x6d,
	0x03, 0x0c, 0x00,
	0x10, 0x00,
	0xb8, 0x3c, 0x2b, 0x00, 0x60, 0x01, 0x02, 0x03
};

u8 const
VendDataBLK_Mini[E_VEND_BLK_MINI_LEN] = {
	0x66,
	0x03, 0x0C, 0x00,	//HDMI VSDB (0x000C03)
	0x10, 0x00,		// PHY Address (0x26..0x27)
	0x80,			// support AI = 1

};

u8 const
Colorimetry_Data_Block[E_COLORMETRY_BLK_LEN] = {
	0xE3,
	0x05,			// Extended Tag code=0x05
	0xe3, 0x01
};

u8 const
Vcdb_Data_Block[E_VCDB_BLOCK_LEN] = {
	0xE2,
	0x00,			// Extended Tag code=0x00
	0x01			// Always OverScane
};
/* detail timing block */
u8 const
Dtl_T_Blk[E_DTD_BLOCK_LEN] = {
	//2560*1440p * 60 (2720 * 1525)
	0x38, 0x61, 0x00, 0xA0, 0xA0, 0xA0, 0x55, 0x50, 0x30,
	0x20, 0x35, 0x00, 0x58, 0x54, 0x21, 0x00, 0x00, 0x1A,
	// Detailed Timing : Table 72> 720x480p (59.94Hz 16:9)
	// 0x25
	0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10,
	0x3E, 0x96, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x18,
};

u8 const
Dtl_T_Blk_1080p[E_DTD_BLOCK_LEN] = {
	// Detailed Timing : Table 72> 720x480p (59.94Hz 16:9)
	// 0x25
	0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10,
	0x3E, 0x96, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x18,
};

u8 const
HF_VD_Block[E_HF_VD_BLK_LEN] = {
	0x67,
	0xd8, 0x5d, 0xc4,	//HDMI HF VSDB (0xc45dd8)
	0x01,
	0x78, 0x80, 0x00,
};

void vShowEDIDAudioDB(struct MTK_HDMI *myhdmi,
	u8 *pbuf,
	u8 *inx)
{
	u8 len, i;

	ELOG("[RX]audio DB addr=0x%x\n", *inx);
	len = *(pbuf + *inx) & 0x1f;
	for (i = 0; i < (len + 1); i++) {
		ELOG("[RX]audio DB[%d/%d]=0x%x\n",
			i, len + 1, *(pbuf + *inx));
		*inx = *inx + 1;
	}
}

void vShowEDIDBLKData(bool en, u8 *pbuf)
{
	u32 j;

	if (en != TRUE)
		return;

	for (j = 0; j < 127; j += 16) {
		ELOG("%02x: %02x %02x %02x %02x %02x %02x %02x %02x\n",
		   j,
		   pbuf[j],
		   pbuf[j + 1],
		   pbuf[j + 2],
		   pbuf[j + 3],
		   pbuf[j + 4],
		   pbuf[j + 5],
		   pbuf[j + 6],
		   pbuf[j + 7]);
		ELOG(" %02x %02x %02x %02x %02x %02x %02x %02x\n",
		   pbuf[j + 8],
		   pbuf[j + 9],
		   pbuf[j + 10],
		   pbuf[j + 11],
		   pbuf[j + 12],
		   pbuf[j + 13],
		   pbuf[j + 14],
		   pbuf[j + 15]);
	}
}

void vShowEDIDVideoDB(struct MTK_HDMI *myhdmi,
	u8 *pbuf,
	u8 *inx)
{
	u8 len, i;

	ELOG("[RX]video DB addr=0x%x\n", *inx);
	len = *(pbuf + *inx) & 0x1f;
	for (i = 0; i < (len + 1); i++) {
		ELOG("[RX]video DB[%d/%d]=0x%x/%d\n",
			i, len + 1,
			*(pbuf + *inx),
			*(pbuf + *inx));
		*inx = *inx + 1;
	}
}

void vShowEDIDVSDB(struct MTK_HDMI *myhdmi,
	u8 *pbuf,
	u8 *inx)
{
	u8 len, i;

	ELOG("[RX]VS DB addr=0x%x\n", *inx);
	len = *(pbuf + *inx) & 0x1f;
	for (i = 0; i < (len + 1); i++) {
		ELOG("[RX]VS DB[%d/%d]=0x%x/%d\n",
			i, len + 1,
			*(pbuf + *inx),
			*(pbuf + *inx));
		*inx = *inx + 1;
	}
}

void vShowEDIDSpeakerDB(struct MTK_HDMI *myhdmi,
	u8 *pbuf,
	u8 *inx)
{
	u8 len, i;

	ELOG("[RX]speaker DB addr=0x%x\n", *inx);
	len = *(pbuf + *inx) & 0x1f;
	for (i = 0; i < (len + 1); i++) {
		ELOG("[RX]speaker DB[%d/%d]=0x%x\n",
			i, len + 1,
			*(pbuf + *inx));
		*inx = *inx + 1;
	}
}

void vShowEDIDVESADB(struct MTK_HDMI *myhdmi,
	u8 *pbuf,
	u8 *inx)
{
	u8 len, i;

	ELOG("[RX]VESA DB addr=0x%x\n", *inx);
	len = *(pbuf + *inx) & 0x1f;
	for (i = 0; i < (len + 1); i++) {
		ELOG("[RX]VESA DB[%d/%d]=0x%x\n",
			i, len + 1,
			*(pbuf + *inx));
		*inx = *inx + 1;
	}
}

void vShowEDIDExtendedDB(struct MTK_HDMI *myhdmi,
	u8 *pbuf,
	u8 *inx)
{
	u8 len, i, ext_tag;

	ELOG("[RX]extended DB addr=0x%x\n", *inx);
	len = *(pbuf + *inx) & 0x1f;
	ext_tag = *(pbuf + *inx + 1);
	for (i = 0; i < (len + 1); i++) {
		ELOG("[RX]ext #%d DB[%d/%d]=0x%x\n",
			ext_tag, i, len + 1,
			*(pbuf + *inx));
		*inx = *inx + 1;
	}
}

void vShowEDIDCEABlock(struct MTK_HDMI *myhdmi,
	u8 *pbuf)
{
	u8 inx, i, dtd_d, version;

	inx = 0;

	ELOG("[RX]show edid cea block\n");
	ELOG("[RX]tag=0x%x\n", *(pbuf + inx));
	inx++;
	ELOG("[RX]version=0x%x\n", *(pbuf + inx));
	version = *(pbuf + inx);
	inx++;
	ELOG("[RX]dtd_d=0x%x\n", *(pbuf + inx));
	dtd_d = *(pbuf + inx);
	inx++;
	ELOG("[RX]#3=0x%x\n", *(pbuf + inx));
	inx++;

	if (version < 3)
		goto vShowEDIDCEABlock_1;

	ELOG("[RX]show data block\n");
	while (inx < dtd_d) {
		switch ((*(pbuf + inx)) >> 5) {
		case 1:	// audio data block
			vShowEDIDAudioDB(myhdmi, pbuf, &inx);
			break;
		case 2:	// video data block
			vShowEDIDVideoDB(myhdmi, pbuf, &inx);
			break;
		case 3:	// vendor-specific data block
			vShowEDIDVSDB(myhdmi, pbuf, &inx);
			break;
		case 4:	// speaker allocation data block
			vShowEDIDSpeakerDB(myhdmi, pbuf, &inx);
			break;
		case 5:	// VESA display transfer characteristic data
			// block
			vShowEDIDVESADB(myhdmi, pbuf, &inx);
			break;
		case 7:	// use extended tag
			vShowEDIDExtendedDB(myhdmi, pbuf, &inx);
			break;
		default:
			if ((*(pbuf + inx) & 0x1f) > 0)
				inx = inx + (*(pbuf + inx) & 0x1f);
			else
				inx++;
			break;
		}
	}

vShowEDIDCEABlock_1:

	while ((inx + 18) < 128) {
		ELOG("[RX]DTD #0x%x\n", inx);
		for (i = 0; i < 18; i++) {
			ELOG("[RX]DTD[%d/18]=0x%x\n",
				i, *(pbuf + inx));
			inx++;
		}
	}
}

void
vEDIDFindCEADataBlock(struct MTK_HDMI *myhdmi,
	u8 in_type,
	u8 *start_index,
	u8 *in_pbuf,
	u8 *out_pbuf)
{
	u8 inx, type, i, dtd_d, version, len, ext_tag;

	inx = 0;

	inx++;
	version = *(in_pbuf + inx);
	inx++;
	dtd_d = *(in_pbuf + inx);
	inx++;
	inx++;

	*out_pbuf = 0;

	if (version < 3)
		return;

	while (inx < dtd_d) {
		len = (*(in_pbuf + inx) & 0x1f) + 1;
		type = (*(in_pbuf + inx)) >> 5;

		if (inx < *start_index) {

		} else if (((type == 1) && (in_type == E_CEA_AUD_DB)) ||
			   ((type == 2) && (in_type == E_CEA_VID_DB)) ||
			   ((type == 3) && (in_type == E_CEA_VS_DB)) ||
			   ((type == 4) && (in_type == E_CEA_SPK_DB))) {
			for (i = 0; i < len; i++)
				*(out_pbuf + i) = *(in_pbuf + inx + i);
			*start_index = inx;
			return;
		} else if (type == 7) {
			ext_tag = *(in_pbuf + inx + 1);
			// extended DB
			if (((ext_tag == 0) &&
			     (in_type == E_CEA_EXT_VC_DB)) ||
			    ((ext_tag == 5) &&
			     (in_type == E_CEA_EXT_CM_DB)) ||
			    ((ext_tag == 6) &&
			     (in_type == E_CEA_EXT_HF_VS_DB)) ||
			    ((ext_tag == 14) &&
			     (in_type == E_CEA_EXT_420_VID_DB)) ||
			    ((ext_tag == 15) &&
			     (in_type == E_CEA_EXT_420_CM_DB)) ||
			    ((ext_tag == 32) &&
			    (in_type == E_CEA_EXT_INFO_DB))) {
				for (i = 0; i < len; i++)
					*(out_pbuf + i) =
						*(in_pbuf + inx + i);
				*start_index = inx;
				return;
			}
		}
		inx = inx + len;
	}
}

u8 Edid_Chk_Sum(struct MTK_HDMI *myhdmi,
	u8 *p_block)
{
	u8 check_sum = 0;
	u16 i;

	for (i = 0; i < (E_BLOCK_SIZE - 1); i++)
		check_sum += *(p_block + i);
	check_sum = 0x100 - check_sum;
	return check_sum;
}

bool Edid_CheckSum_Check(struct MTK_HDMI *myhdmi,
	u8 *p_block)
{
	u8 check_sum = 0;
	u16 i;

	for (i = 0; i < (E_BLOCK_SIZE - 1); i++)
		check_sum += *(p_block + i);
	check_sum = 0x100 - check_sum;
	if (*(p_block + (E_BLOCK_SIZE - 1)) != check_sum)
		return FALSE;
	return TRUE;
}

bool Edid_BL0Header_Check(struct MTK_HDMI *myhdmi,
	u8 *p_block)
{
	u16 i;

	for (i = 0; i < E_BL0_LEN_HEADER; i++) {
		if (*(p_block + E_BL0_ADR_HEADER + i) !=
		    Def_Edid_Blk0[E_BL0_ADR_HEADER + i])
			return FALSE;
	}
	return TRUE;
}

bool Edid_BL0Version_Check(struct MTK_HDMI *myhdmi,
	u8 *p_block)
{
	/* only 1.x versions are allowed (not 2.0) */
	if (*(p_block + E_BL0_ADR_VER) != 1)
		return FALSE;
	return TRUE;
}

bool fgDTDSupp(struct MTK_HDMI *myhdmi,
	u8 *prbData)
{
	u16 ui2HActive, ui2VActive;

	ui2HActive = (u16)(*(prbData + 4) & 0xf0) << 4;
	ui2HActive |= *(prbData + 2);
	ui2VActive = (u16)(*(prbData + 7) & 0xf0) << 4;
	ui2VActive |= *(prbData + 5);

	DBGLOG("[RX]%s Check\n", __func__);
	DBGLOG("[RX]ui2HActive = 0x%x, ui2VActive = 0x%x\n",
		ui2HActive, ui2VActive);

	// 1080I60  1080P60
	if ((ui2HActive == 0x780) &&
		((ui2VActive == 0x21c) ||
		(ui2VActive == 0x438)))
		return TRUE;
	// 480P60, 576P
	if ((ui2HActive == 0x2D0) &&
		((ui2VActive == 0x1E0) ||
		(ui2VActive == 0x240)))
		return TRUE;
	// 720P60, 720P50
	if ((ui2HActive == 0x500) &&
		(ui2VActive == 0x2D0))
		return TRUE;
	// 640x480
	if ((ui2HActive == 0x280) &&
		(ui2VActive == 0x1E0))
		return TRUE;

	return FALSE;
}

void ComposeEdidBlock0(struct MTK_HDMI *myhdmi,
	u8 *pr_rBlk,
	u8 *pr_oBlk)
{
	u8 j, i;
	u8 rEdidAddr, oEdidAddr;
	u16 PixClk;
	u32 sink_vid;

	sink_vid = (SCP->s_cea_ntsc_res |
		    SCP->s_cea_pal_res);

	for (i = 0x00; i <= 0x13; i++)
		*(pr_oBlk + i) = Def_Edid_Blk0[i];
	for (i = 0x14; i <= 0x18; i++)	// basic Display Prams
		*(pr_oBlk + i) = *(pr_rBlk + i);
	for (i = 0x19; i <= 0x22; i++)	// Chromaticity
		*(pr_oBlk + i) = *(pr_rBlk + i);
	for (i = 0x23; i <= 0x35; i++)
		*(pr_oBlk + i) = Def_Edid_Blk0[i];

	rEdidAddr = E_BL0_ADR_DTDs;	// 0x36
	oEdidAddr = rEdidAddr;

	for (j = 0; j < 4; j++) {
		PixClk = (*(pr_rBlk + rEdidAddr + 1) << 8) |
			(*(pr_rBlk + rEdidAddr));
		/* 16500 */
		if ((PixClk <= F_PIX_MAX) && PixClk) {
			if (fgDTDSupp(myhdmi, pr_rBlk + rEdidAddr) == TRUE) {
				for (i = 0; i <= 17; i++)
					*(pr_oBlk + oEdidAddr + i) =
						*(pr_rBlk + rEdidAddr + i);
			} else {
				for (i = 0; i <= 17; i++)
					*(pr_oBlk + oEdidAddr + i) = 0;
			}
		} else if (PixClk == 0x0000) {
			if (*(pr_rBlk + rEdidAddr + 3) == E_MON_RANGE_DTD) {
				/* 00 00 00 FD */
				for (i = 0; i <= 17; i++)
					*(pr_oBlk + oEdidAddr + i) =
						*(pr_rBlk + rEdidAddr + i);

			} else if (*(pr_rBlk + rEdidAddr + 3) ==
				E_MON_NAME_DTD) {
				/* 00 00 00 FC */
				for (i = 0; i <= 4; i++)
					*(pr_oBlk + oEdidAddr + i) =
					    Def_Edid_Blk0[0x5a + i];
				for (i = 5; i <= 17; i++)
					*(pr_oBlk + oEdidAddr + i) =
						*(pr_rBlk + rEdidAddr + i);
			} else {
				for (i = 0; i <= 17; i++)
					*(pr_oBlk + oEdidAddr + i) =
						*(pr_rBlk + rEdidAddr + i);
			}
		} else {
			for (i = 0; i <= 17; i++)
				*(pr_oBlk + oEdidAddr + i) = 0;
		}

		oEdidAddr += 18;	/* Increase address for one DTD */
		rEdidAddr += 18;

		if (rEdidAddr >= END_1stPAGE_DESCR_ADDR)	/* 0x7e */
			break;
		if (oEdidAddr >= (END_1stPAGE_DESCR_ADDR))	/* 0x7e */
			break;
	}

	if (myhdmi->AudUseAnd == TRUE)
		*(pr_oBlk + E_BL0_ADR_EXT_NMB) =
		    *(pr_rBlk + E_BL0_ADR_EXT_NMB);
	else
		*(pr_oBlk + E_BL0_ADR_EXT_NMB) = 0x01;

	*(pr_oBlk + E_ADR_CHK_SUM) =
		Edid_Chk_Sum(myhdmi, pr_oBlk);
}

void Default_Edid_BL0_Write(struct MTK_HDMI *myhdmi)
{
	u8 Addr;

	DBGLOG("[RX]%s\n", __func__);

	for (Addr = 0; Addr < 127; Addr++)
		myhdmi->oBLK[Addr] = Def_Edid_Blk0[Addr];

	myhdmi->oBLK[E_ADR_CHK_SUM] =
		Edid_Chk_Sum(myhdmi, &myhdmi->oBLK[0]);

#if (HDMI_INPUT_COUNT == 1)
	vWriteEDIDBlk0(myhdmi, EEPROM0, myhdmi->oBLK);
#else
	vWriteEDIDBlk0(myhdmi, EEPROM0, myhdmi->oBLK);
	vWriteEDIDBlk0(myhdmi, EEPROM1, myhdmi->oBLK);
	vWriteEDIDBlk0(myhdmi, EEPROM2, myhdmi->oBLK);
#endif
}

void modify_pa(u8 *p,
	bool fgUseMdyPA,
	u16 u2MdyPA,
	u8 Addr,
	u8 i)
{
	if ((i == 4) || (i == 5)) {
		if (fgUseMdyPA == TRUE) {
			if (i == 4)
				p[Addr] = (u2MdyPA >> 8) & 0xff;
			if (i == 5)
				p[Addr] = u2MdyPA & 0xff;
		} else
			p[Addr] = VendDataBLK_Full[i];
	} else
		p[Addr] = VendDataBLK_Full[i];
}

void Default_Edid_BL1_Write(struct MTK_HDMI *myhdmi)
{
	u8 Addr, oriAddr, i, j;
	u8 u1EdidNum, PhyInxTmp, u1PAHigh, u1PALow;
	u8 PhyInx, u1DownSTRM;
	u16 ui2_pa, P1Addr, P2Addr, P3Addr;

	/*** Block 1 ***/
	DBGLOG("[RX]%s\n", __func__);

	for (Addr = 0; Addr < 4; Addr++)
		myhdmi->oBLK[Addr] = Def_Edid_Blk1_Header[Addr];

	DBGLOG("[RX]blk1 of video data addr=0x%x\n", Addr);

	if (myhdmi->sel_edid == 1) {
		RX_DEF_LOG("[rx]SVD 1080p only\n");
		for (i = 0; i < E_VID_BLK_LEN_1080P; i++, Addr++)
			myhdmi->oBLK[Addr] = Vid_Data_BLK_1080P[i];
	} else
		for (i = 0; i < E_VID_BLK_LEN; i++, Addr++)
			myhdmi->oBLK[Addr] = Vid_Data_BLK[i];

	DBGLOG("[RX]blk1 of audio data addr=0x%x\n", Addr);

	if (fgOnlySupportPcm2ch(myhdmi) == TRUE) {
		for (i = 0; (i < E_AUD2CHPCM_ONLY_LEN) && (Addr < 128);
			i++, Addr++)
			myhdmi->oBLK[Addr] = AudDataBlk_2CH_ONLY[i];
	} else {
		for (i = 0; (i < E_AUD_BLK_LEN) && (Addr < 128);
			i++, Addr++)
			myhdmi->oBLK[Addr] = Audio_Data_Block[i];
	}

	DBGLOG("[RX]blk1 of spk data addr=0x%x\n", Addr);

	if (fgOnlySupportPcm2ch(myhdmi) == TRUE) {
		for (i = 0; (i < E_SPK_BLK_LEN) && (Addr < 128); i++, Addr++)
			myhdmi->oBLK[Addr] = SpkDataBlk_2CH_ONLY[i];
	} else {
		for (i = 0; (i < E_SPK_BLK_LEN) && (Addr < 128); i++, Addr++)
			myhdmi->oBLK[Addr] = Speaker_Data_Block[i];
	}

	oriAddr = Addr;

	PhyInx = Addr + 4;

	for (u1EdidNum = 0; u1EdidNum <= EEPROM2; u1EdidNum++) {
		switch (u1EdidNum) {
		case EEPROM0:
			for (i = 0; i < E_VEND_FULL_L; i++, Addr++) {
				/* PA start address */
				if (Addr < 128)
					modify_pa(&myhdmi->oBLK[0],
						myhdmi->fgUseMdyPA,
						myhdmi->u2MdyPA,
						Addr,
						i);
			}
			if (myhdmi->sel_edid != 1) {
				// HF-VSDB
				for (i = 0; (i < E_HF_VD_BLK_LEN) && (Addr < 128); i++, Addr++)
					myhdmi->oBLK[Addr] = HF_VD_Block[i];
				// 420 cmdb
				for (i = 0; (i < 5) && (Addr < 128); i++, Addr++)
					myhdmi->oBLK[Addr] = Def_420cmdb[i];
			}

			myhdmi->oBLK[2] = Addr;	/* Update data offset */

			if (myhdmi->sel_edid == 1) {
				RX_DEF_LOG("[rx]DTD 1080p only\n");
				for (i = 0; (Addr + 18) < 128; i += 18) {
					if ((i + 18) > E_DTD_BLOCK_1080P_LEN)
						break;
					for (j = 0; (j < 18) && (Addr < 128);
						j++, Addr++)
						myhdmi->oBLK[Addr] = Dtl_T_Blk_1080p[i + j];
				}
			} else {
				for (i = 0; (Addr + 18) < 128; i += 18) {
					if ((i + 18) > E_DTD_BLOCK_LEN)
						break;
					for (j = 0; (j < 18) && (Addr < 128);
						j++, Addr++)
						myhdmi->oBLK[Addr] = Dtl_T_Blk[i + j];
				}
			}
			for (; Addr < 128; Addr++)
				myhdmi->oBLK[Addr] = 0;

			u1DownSTRM = 0;
			ui2_pa = 0;
#ifdef DEF_CEC_PA
			PhyInxTmp = 16;
			if (PhyInxTmp >= 4)
				PhyInxTmp -= 4;
			P1Addr = (ui2_pa | (1 << PhyInxTmp));
			P2Addr = (ui2_pa | (2 << PhyInxTmp));
			P3Addr = (ui2_pa | (3 << PhyInxTmp));
#else
/*
 *			for (PhyInxTmp = 0; PhyInxTmp < 16; PhyInxTmp += 4) {
 *				if (((ui2_pa >> PhyInxTmp) & 0x0f) != 0)
 *					break;
 *			}
 *
 *			if (PhyInxTmp != 0) {
 *				if (PhyInxTmp >= 4)
 *					PhyInxTmp -= 4;
 *				P1Addr = (ui2_pa | (1 << PhyInxTmp));
 *				P2Addr = (ui2_pa | (2 << PhyInxTmp));
 *				P3Addr = (ui2_pa | (3 << PhyInxTmp));
 *			} else {
 *				if ((u1DownSTRM == 0) && (ui2_pa == 0xFFFF)) {
 *					P1Addr = 0x1100;
 *					P2Addr = 0x1200;
 *					P3Addr = 0x1400;
 *				} else {
 *					P1Addr = 0xFFFF;
 *					P2Addr = 0xFFFF;
 *					P3Addr = 0xFFFF;
 *				}
 *			}
 */
#endif

			if (myhdmi->_u2Port1Addr != 0)
				P1Addr = myhdmi->_u2Port1Addr;

			u1PAHigh = (u8)(P1Addr >> 8);
			u1PALow = (u8)(P1Addr & 0xFF);
			if ((PhyInx + 1) < sizeof(myhdmi->oBLK)) {
				myhdmi->oBLK[PhyInx] = u1PAHigh;
				myhdmi->oBLK[PhyInx + 1] = u1PALow;
			}
			myhdmi->_u2Port1Addr = P1Addr;
			if (txapi_is_plugin(myhdmi) == TRUE)
				if ((PhyInx + 2) < sizeof(myhdmi->oBLK))
					myhdmi->oBLK[PhyInx + 2] &= 0x80;

			myhdmi->oBLK[E_ADR_CHK_SUM] =
			    Edid_Chk_Sum(myhdmi, &myhdmi->oBLK[0]);
			myhdmi->_u1EDID0CHKSUM = myhdmi->oBLK[E_ADR_CHK_SUM];
			myhdmi->_u2EDID0PA = P1Addr;
			myhdmi->_u1EDID0PAOFF = PhyInx;
			vWriteEDIDBlk1(myhdmi, EEPROM0, myhdmi->oBLK);
			break;

#if (HDMI_INPUT_COUNT != 1)
		case EEPROM1:
			Addr = oriAddr;

			for (i = 0; (i < E_VEND_FULL_L) && (Addr < 128);
				i++, Addr++) {
				if (Addr < 128)
					modify_pa(&myhdmi->oBLK[0],
						myhdmi->fgUseMdyPA,
						myhdmi->u2MdyPA,
						Addr,
						i);
			}

			myhdmi->oBLK[2] = Addr;

			for (i = 0; (Addr + 18) < 128; i += 18) {
				if ((i + 18) >= E_DTD_BLOCK_LEN)
					break;
				for (j = 0; (j < 18) && (Addr < 128);
					j++, Addr++)
					myhdmi->oBLK[Addr] = Dtl_T_Blk[i + j];
			}

			for (; Addr < 128; Addr++)
				myhdmi->oBLK[Addr] = 0;

			u1DownSTRM = 0;
			ui2_pa = 0;
#ifdef DEF_CEC_PA
			PhyInxTmp = 16;
			if (PhyInxTmp >= 4)
				PhyInxTmp -= 4;
			P1Addr = (ui2_pa | (1 << PhyInxTmp));
			P2Addr = (ui2_pa | (2 << PhyInxTmp));
			P3Addr = (ui2_pa | (3 << PhyInxTmp));
#else
/*
 *			for (PhyInxTmp = 0; PhyInxTmp < 16; PhyInxTmp += 4)
 *				if (((ui2_pa >> PhyInxTmp) & 0x0f) != 0)
 *					break;
 *			if (PhyInxTmp != 0) {
 *				if (PhyInxTmp >= 4)
 *					PhyInxTmp -= 4;
 *
 *				P1Addr = (ui2_pa | (1 << PhyInxTmp));
 *				P2Addr = (ui2_pa | (2 << PhyInxTmp));
 *				P3Addr = (ui2_pa | (3 << PhyInxTmp));
 *			} else {
 *				if ((u1DownSTRM == 0) && (ui2_pa == 0xFFFF)) {
 *					P1Addr = 0x1100;
 *					P2Addr = 0x1200;
 *					P3Addr = 0x1400;
 *				} else {
 *					P1Addr = 0xFFFF;
 *					P2Addr = 0xFFFF;
 *					P3Addr = 0xFFFF;
 *				}
 *			}
 */
#endif

			u1PAHigh = (u8)(P2Addr >> 8);
			u1PALow = (u8)(P2Addr & 0xFF);
			if ((PhyInx + 1) < sizeof(myhdmi->oBLK)) {
				myhdmi->oBLK[PhyInx] = u1PAHigh;
				myhdmi->oBLK[PhyInx + 1] = u1PALow;
			}
			myhdmi->_u2Port2Addr = P2Addr;
			if (txapi_is_plugin(myhdmi) == TRUE)
				if ((PhyInx + 2) < sizeof(myhdmi->oBLK))
					myhdmi->oBLK[PhyInx + 2] &= 0x80;

			myhdmi->oBLK[E_ADR_CHK_SUM] =
			    Edid_Chk_Sum(myhdmi, &myhdmi->oBLK[0]);

			myhdmi->_u1EDID1CHKSUM = myhdmi->oBLK[E_ADR_CHK_SUM];
			myhdmi->_u2EDID1PA = P2Addr;
			myhdmi->_u1EDID1PAOFF = PhyInx;

			vWriteEDIDBlk1(myhdmi, EEPROM1, myhdmi->oBLK);
			break;

		case EEPROM2:
			Addr = oriAddr;

			for (i = 0; (i < E_VEND_FULL_L) && (Addr < 128);
				i++, Addr++) {
				if (Addr < 128)
					modify_pa(&myhdmi->oBLK[0],
						myhdmi->fgUseMdyPA,
						myhdmi->u2MdyPA,
						Addr,
						i);
			}

			myhdmi->oBLK[2] = Addr;

			for (i = 0; (Addr + 18) < 128; i += 18) {
				if ((i + 18) >= E_DTD_BLOCK_LEN)
					break;
				for (j = 0; (j < 18) && (Addr < 128);
					j++, Addr++)
					myhdmi->oBLK[Addr] = Dtl_T_Blk[i + j];
			}

			for (; Addr < 128; Addr++)
				myhdmi->oBLK[Addr] = 0;

			u1DownSTRM = 0;
			ui2_pa = 0;
#ifdef DEF_CEC_PA
			PhyInxTmp = 16;
			if (PhyInxTmp >= 4)
				PhyInxTmp -= 4;
			P1Addr = (ui2_pa | (1 << PhyInxTmp));
			P2Addr = (ui2_pa | (2 << PhyInxTmp));
			P3Addr = (ui2_pa | (3 << PhyInxTmp));
#else
/*
 *			for (PhyInxTmp = 0; PhyInxTmp < 16;
 *				PhyInxTmp += 4)
 *				if (((ui2_pa >> PhyInxTmp) & 0x0f) != 0)
 *					break;
 *			if (PhyInxTmp != 0) {
 *
 *			} else {
 *				if ((u1DownSTRM == 0) && (ui2_pa == 0xFFFF)) {
 *					P1Addr = 0x1100;
 *					P2Addr = 0x1200;
 *					P3Addr = 0x1400;
 *				} else {
 *					P1Addr = 0xFFFF;
 *					P2Addr = 0xFFFF;
 *					P3Addr = 0xFFFF;
 *				}
 *			}
 */
#endif

			u1PAHigh = (u8)(P3Addr >> 8);
			u1PALow = (u8)(P3Addr & 0xFF);
			if ((PhyInx + 1) < sizeof(myhdmi->oBLK)) {
				myhdmi->oBLK[PhyInx] = u1PAHigh;
				myhdmi->oBLK[PhyInx + 1] = u1PALow;
			}
			myhdmi->_u2Port3Addr = P3Addr;
			if (txapi_is_plugin(myhdmi) == TRUE) {
				if ((PhyInx + 2) < sizeof(myhdmi->oBLK))
					myhdmi->oBLK[PhyInx + 2] &= 0x80;
			}

			myhdmi->oBLK[E_ADR_CHK_SUM] =
			    Edid_Chk_Sum(myhdmi, &myhdmi->oBLK[0]);

			myhdmi->_u1EDID2CHKSUM = myhdmi->oBLK[E_ADR_CHK_SUM];
			myhdmi->_u2EDID2PA = P3Addr;
			myhdmi->_u1EDID2PAOFF = PhyInx;

			vWriteEDIDBlk1(myhdmi, EEPROM2, myhdmi->oBLK);
			break;
#endif
		default:
			break;
		}

		ELOG("[RX]def EBlk1 u1ENum%d Page1 EDID\r\n",
			   u1EdidNum);
		vShowEDIDBLKData(EDIDDBG, &myhdmi->oBLK[0]);
	}

	myhdmi->Edid.bBlock1Err = TRUE;
	myhdmi->Edid.bDownDvi = TRUE;
	myhdmi->PHYAdr.Org = 0x0000;
	myhdmi->PHYAdr.Dvd = 0x1000;
	myhdmi->PHYAdr.Sat = 0x2000;
	myhdmi->PHYAdr.Tv = 0x3000;
	myhdmi->PHYAdr.Else = 0x4000;
	myhdmi->Edid.PHYLevel = 1;
}

bool ParseHDMIEDID(struct MTK_HDMI *myhdmi,
	u8 *prBlk)
{
	if (*(prBlk + 0) != 2)
		return FALSE;
	if (*(prBlk + 1) < 3)
		return FALSE;
	return TRUE;
}

void edid420bitmap(struct MTK_HDMI *myhdmi,
	u8 u1VidAndOP,
	u32 u4_4k_420,
	u8 i,
	u8 j)
{
	if (j < 1)
		return;

	if (u1VidAndOP) {
		if ((((myhdmi->CP_VidDataBLK[i] & 0x7f) == 96) &&
			 (u4_4k_420 & SINK_2160P_50HZ)) ||
			(((myhdmi->CP_VidDataBLK[i] & 0x7f) == 97) &&
			 (u4_4k_420 & SINK_2160P_60HZ)) ||
			(((myhdmi->CP_VidDataBLK[i] & 0x7f) == 101) &&
			 (u4_4k_420 & SINK_2161P_50HZ)) ||
			(((myhdmi->CP_VidDataBLK[i] & 0x7f) == 102) &&
			   (u4_4k_420 & SINK_2161P_60HZ))) {
			myhdmi->EDID420BitMap[(j - 1) / 8] |=
				1 << ((j - 1) % 8);
		}
	} else {
		if (((myhdmi->CP_VidDataBLK[i] &  0x7f) == 96) ||
			((myhdmi->CP_VidDataBLK[i] &  0x7f) == 97) ||
			((myhdmi->CP_VidDataBLK[i] &  0x7f) == 101) ||
			((myhdmi->CP_VidDataBLK[i] & 0x7f) == 102)) {
			myhdmi->EDID420BitMap[(j - 1) / 8] |=
				1 << ((j - 1) % 8);
		}
	}
}

void
ComposeVideoBlock(struct MTK_HDMI *myhdmi,
	u8 u1VidAndOP,
	u8 u1AudAndOP)
{
	u32 sink_vid, sink_native_vid, ui4Temp = 0;
	u8 TotalLen, i, j;
	u8 temp[E_VID_BLK_LEN];
	u32 sink_4k_vid, u4_4k_420;

	DBGLOG("[RX]%s\n", __func__);

	sink_vid = (SCP->s_dtd_ntsc_res |
		    SCP->s_org_cea_ntsc_res |
		    SCP->s_dtd_pal_res |
		    SCP->s_org_cea_pal_res);
	sink_native_vid =
	    (SCP->s_native_ntsc_res |
	     SCP->s_native_pal_res);
	sink_4k_vid = SCP->s_hdmi_4k2kvic;

	DBGLOG("[RX]sink_vid=0x%x, u1VidAndOP =%d\n",
		sink_vid, u1VidAndOP);
	DBGLOG("[RX]s_supp_hdmi_mode=%d\n",
		SCP->s_supp_hdmi_mode);
	DBGLOG("[RX]s_native_ntsc_res= 0x%x\n",
		SCP->s_native_ntsc_res);
	DBGLOG("[RX]s_native_pal_res= 0x%x\n",
		SCP->s_native_pal_res);

	TotalLen = E_VID_BLK_LEN - 1;
	for (i = 0; i < TotalLen + 1; i++)
		myhdmi->CP_VidDataBLK[i] = 0;

	for (i = 0; i < 0x10; i++)
		myhdmi->EFirst16VIC[i] = 0;

	i = 1;
	while (TotalLen) {
		switch (Vid_Data_BLK[i] & 0x7f) {
		case 1:
			if (u1VidAndOP) {
				if (sink_vid & SINK_VGA) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_VGA)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else {
					if ((SCP->s_supp_hdmi_mode ==
					     FALSE) && (u1AudAndOP == FALSE))
						myhdmi->CP_VidDataBLK[i] = 1;
					else
						myhdmi->CP_VidDataBLK[i] = 0;
				}
			} else {
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			}
			break;

		case 2:	/* 720x480P 4:3 */
			if (u1VidAndOP) {
				if (sink_vid & SINK_480P_4_3) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_480P_4_3)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];

			break;

		case 3:
			if (u1VidAndOP) {
				if (sink_vid & SINK_480P) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_480P)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];

			break;

		case 4:	// 720P60
			if (u1VidAndOP) {
				if (sink_vid & SINK_720P60) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_720P60)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 5:	// 1080i60
			if (u1VidAndOP) {
				if (sink_vid & SINK_1080I60) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_1080I60)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 6:	// 480i
			if (u1VidAndOP) {
				if (sink_vid & SINK_480I_4_3) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_480I_4_3)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 7:	// 480i
			if (u1VidAndOP) {
				if (sink_vid & SINK_480I) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_480I)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 16:	// 1080P60
			if (u1VidAndOP) {
				if (sink_vid & SINK_1080P60) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_1080P60)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 17:	// 576P
			if (u1VidAndOP) {
				if (sink_vid & SINK_576P_4_3) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_576P_4_3)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 18:
			if (u1VidAndOP) {
				if (sink_vid & SINK_576P) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_576P)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 19:	// 720P50
			if (u1VidAndOP) {
				if (sink_vid & SINK_720P50) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_720P50)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 20:	// 1080I50
			if (u1VidAndOP) {
				if (sink_vid & SINK_1080I50) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_1080I50)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 21:	// 576i
			if (u1VidAndOP) {
				if (sink_vid & SINK_576I_4_3) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_576I_4_3)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 22:
			if (u1VidAndOP) {
				if (sink_vid & SINK_576I) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_576I)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 31:	// 1080P50
			if (u1VidAndOP) {
				if (sink_vid & SINK_1080P50) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_1080P50)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 32:	// 1080P24
			if (u1VidAndOP) {
				if ((sink_vid & SINK_1080P24) ||
					(sink_vid & SINK_1080P23976)) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if ((sink_native_vid & SINK_1080P24) ||
					    (sink_native_vid & SINK_1080P23976))
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 60:	// 720P24
			if (u1VidAndOP) {
				if ((sink_vid & SINK_720P24) ||
					(sink_vid & SINK_720P23976)) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if ((sink_native_vid & SINK_720P24) ||
					    (sink_native_vid & SINK_720P23976))
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 34:	// 1080P30
			if (u1VidAndOP) {
				if ((sink_vid & SINK_1080P30) ||
					(sink_vid & SINK_1080P2997)) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if ((sink_native_vid & SINK_1080P30) ||
					    (sink_native_vid & SINK_1080P2997))
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 33:	// 1080P25
			if (u1VidAndOP) {
				if (sink_vid & SINK_1080P25) {
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
					if (sink_native_vid & SINK_1080P25)
						myhdmi->CP_VidDataBLK[i] |=
							0x80;
				} else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 93:	// 3840*2160p24
			if (u1VidAndOP) {
				if ((sink_4k_vid & SINK_2160P_24HZ) ||
				    (sink_4k_vid & SINK_2160P_23_976HZ))
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
				else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 94:	// 3840*2160p25
			if (u1VidAndOP) {
				if (sink_4k_vid & SINK_2160P_25HZ)
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
				else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 95:	// 3840*2160p30
			if (u1VidAndOP) {
				if ((sink_4k_vid & SINK_2160P_30HZ) ||
				    (sink_4k_vid & SINK_2160P_29_97HZ))
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
				else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 96:	// 3840*2160p50
			if (u1VidAndOP) {
				if (sink_4k_vid & SINK_2160P_50HZ)
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
				else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 97:	// 3840*2160p60
			if (u1VidAndOP) {
				if (sink_4k_vid & SINK_2160P_60HZ)
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
				else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 98:	// 4096*2160p24
			if (u1VidAndOP) {
				if (sink_4k_vid & SINK_2161P_24HZ)
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
				else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 99:	// 4096*2160p25
			if (u1VidAndOP) {
				if (sink_4k_vid & SINK_2161P_25HZ)
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
				else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 100:	// 4096*2160p30
			if (u1VidAndOP) {
				if (sink_4k_vid & SINK_2161P_30HZ)
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
				else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 101:	// 4096*2160p50
			if (u1VidAndOP) {
				if (sink_4k_vid & SINK_2161P_50HZ)
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
				else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		case 102:	// 4096*2160p60
			if (u1VidAndOP) {
				if (sink_4k_vid & SINK_2161P_60HZ)
					myhdmi->CP_VidDataBLK[i] =
					    Vid_Data_BLK[i] & 0x7f;
				else
					myhdmi->CP_VidDataBLK[i] = 0;
			} else
				myhdmi->CP_VidDataBLK[i] = Vid_Data_BLK[i];
			break;

		default:
			myhdmi->CP_VidDataBLK[i] = 0;
			break;
		}

		i++;
		TotalLen--;
	}

	for (i = 0; i < E_VID_BLK_LEN; i++)
		temp[i] = 0;	// clean

	// clear 420 bit map
	for (i = 0; i < E_420_BIT_MAP_LEN; i++)
		myhdmi->EDID420BitMap[i] = 0;

	// for 420cmdb
	sink_4k_vid = SCP->s_hdmi_4k2kvic & (
		SINK_2160P_50HZ |
		SINK_2160P_60HZ |
		SINK_2161P_50HZ |
		SINK_2161P_60HZ);
	u4_4k_420 = SCP->s_4k2kvic_420_vdb & (
		SINK_2160P_50HZ |
		SINK_2160P_60HZ |
		SINK_2161P_50HZ |
		SINK_2161P_60HZ);
	u4_4k_420 = u4_4k_420 & sink_4k_vid;

	j = 1;
	TotalLen = 0;
	for (i = 1; i < E_VID_BLK_LEN; i++) {
		if (myhdmi->CP_VidDataBLK[i] != 0) {
			// 420 bit map (only 4k50/60 resolution)
			edid420bitmap(myhdmi, u1VidAndOP, u4_4k_420, i, j);

			temp[j] = myhdmi->CP_VidDataBLK[i];
			TotalLen++;
			j++;

			// get rx video block VIC order,  used for compose vsdb
			// 3D info
			if (i <= 0x10) {
				switch (myhdmi->CP_VidDataBLK[i] & 0x7f) {
				case 1:
					ui4Temp = 0;
					ui4Temp |= SINK_VGA;
					break;
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
					ui4Temp = SINK_1080P25;
					break;

				case 34:
					ui4Temp = 0;
					ui4Temp |= SINK_1080P30;
					ui4Temp |= SINK_1080P2997;
					break;

				case 60:
					ui4Temp = 0;
					ui4Temp |= SINK_720P24;
					ui4Temp |= SINK_720P23976;
					break;

				default:
					break;
				}
				myhdmi->EFirst16VIC[i - 1] = ui4Temp;
				DBGLOG("[RX]First16VIC[%d]=0x%x\n",
					i - 1, ui4Temp);
			}
		}
	}

	temp[0] = ((0x02 << 5) | TotalLen);	// Update Header

	for (i = 0; i < (TotalLen + 1); i++) {
		myhdmi->CP_VidDataBLK[i] = temp[i];
		DBGLOG("[RX]myhdmi->CP_VidDataBLK[%d/%d]=%x\n",
			i, (TotalLen + 1), myhdmi->CP_VidDataBLK[i]);
	}

	if (EDIDDBG) {
		for (i = 0; i < E_420_BIT_MAP_LEN; i++)
			ELOG("[RX]myhdmi->EDID420BitMap[%d]=%x\n",
				   i, myhdmi->EDID420BitMap[i]);
	}
}

void audio_data_blk(struct MTK_HDMI *myhdmi,
	u8 *prData,
	u8 *poBLK,
	u8 *poCnt,
	u8 *Aud_Blk,
	u8 *bNo_p)
{
	u8 bNo, i;
	u8 bIdx;
	u8 LenSum;
	u8 TotalLen = 0, dec = 0, chan = 0, sink_dec = 0;
	u8 sink_chan = 0, dec_support = 0, head_add = 0;
	u8 data_byte2 = 0, data_byte3 = 0;
	u8 sink_byte2 = 0, sink_byte3 = 0;
	u8 head_start = 0, sad_len = 0;
	u8 PCM2CHADD = 0;
	u8 uTemp6ChByte2 = 0, uTemp2ChByte2 = 0, uTempByte2 = 0;

	bNo = *bNo_p;

	dec = 0;
	chan = 0;
	sink_dec = 0;
	sink_chan = 0;

	head_add = 0;
	sad_len = 0;

	for (bIdx = 0; bIdx < (bNo / 3); bIdx++) {
		LenSum = bIdx * 3;
		dec_support = 0;

		if (fgOnlySupportPcm2ch(myhdmi) == TRUE)
			TotalLen = E_AUD2CHPCM_ONLY_LEN - 1;
		else
			TotalLen = E_AUD_BLK_LEN - 1;
		i = 1;

		while (TotalLen) {
			/* if (i < sizeof(Aud_Blk)){} */
			if (i >= sizeof(Aud_Blk))
				goto audio_data_blk_1;

			dec = (Aud_Blk[i] >> 3) & 0x0f;
			chan = (Aud_Blk[i] & 0x07);

			sink_dec = (*(prData + LenSum + 1) >> 3) & 0x0f;
			sink_chan = (*(prData + LenSum + 1) & 0x07);

			ELOG("sink_chan =%d, chan =%d\n",
				   sink_chan, chan);

			if (dec == sink_dec) {
				if (dec != 1) {
					dec_support = 1;
					if ((i + 2) < sizeof(Aud_Blk)) {
						data_byte2 = Aud_Blk[i + 1];
						data_byte3 = Aud_Blk[i + 2];
					}
					break;
				} else if ((chan >= sink_chan) ||
					((chan >= 5) && (chan <= sink_chan))) {
					dec_support = 1;
					if ((i + 2) < sizeof(Aud_Blk)) {
						data_byte2 = Aud_Blk[i + 1];
						data_byte3 = Aud_Blk[i + 2];
					}
					break;
				}
			}

audio_data_blk_1:
			i += 3;
			if (TotalLen >= 3)
				TotalLen -= 3;
			else
				TotalLen = 0;
		}

		if (dec_support) {
			if (head_add == 0) {
				head_start = *poCnt;
				*(poBLK + *poCnt) = *prData;
				*poCnt = (*poCnt + 1);
				head_add = 1;
			}

			sink_byte2 = *(prData + LenSum + 2);
			sink_byte3 = *(prData + LenSum + 3);

			if (chan >= sink_chan)
				*(poBLK + *poCnt) = (dec << 3) | sink_chan;
			else
				*(poBLK + *poCnt) = (dec << 3) | chan;

			*poCnt = (*poCnt + 1);

			*(poBLK + *poCnt) = (sink_byte2 & data_byte2);
			*poCnt = (*poCnt + 1);

			if (sink_dec == 0x01)	// PCM
				*(poBLK + *poCnt) =
					(sink_byte3 & data_byte3);
			else if ((sink_dec >= 0x02) && (sink_dec >= 0x08))
				*(poBLK + *poCnt) = sink_byte3;
			else
				*(poBLK + *poCnt) = sink_byte3;

			*poCnt = (*poCnt + 1);

			uTemp6ChByte2 = sink_byte2 & 0x7F;
			// sample rates.same as
			// the PCM6Ch below
			uTemp2ChByte2 = sink_byte2 & 0x1F;
			// sample rates.same as
			// the PCM2Ch below
			uTempByte2 = sink_byte2 & data_byte2;
			// Add 6Ch
			if ((SCP->s_org_pcm_ch_samp[4] == 0) &&
				(sink_dec == 0x01)) {
				if (uTemp6ChByte2 > uTempByte2) {
					// except when 6ch
					// Sample < 8ch sample
					// after composed
					DBGLOG("[RX]ADD SAD PCM6CH\n");
					*(poBLK + *poCnt) = (dec << 3) | 5;
					*poCnt = (*poCnt + 1);

					*(poBLK + *poCnt) =
						(sink_byte2 & 0x7F);
					*poCnt = (*poCnt + 1);

					*(poBLK + *poCnt) =
						(sink_byte3 & data_byte3);
					*poCnt = (*poCnt + 1);

					sad_len += 3;
				}
			}
			// Add 2Ch
			if ((SCP->s_org_pcm_ch_samp[0] == 0) &&
				(sink_dec == 0x01) && (PCM2CHADD == 0)) {
				if ((uTemp2ChByte2 > uTempByte2) ||
					(uTemp2ChByte2 > uTemp6ChByte2)) {
					DBGLOG("[RX]ADD SAD PCM2CH\n");
					*(poBLK + *poCnt) =
						(dec << 3) | 1;
					*poCnt = (*poCnt + 1);

					*(poBLK + *poCnt) =
						(sink_byte2 & 0x1F);
					*poCnt = (*poCnt + 1);

					*(poBLK + *poCnt) =
						(sink_byte3 & data_byte3);
					*poCnt = (*poCnt + 1);

					sad_len += 3;
				}
				PCM2CHADD = 1;
			}
			sad_len += 3;
		}
	}

	if (head_add)
		*(poBLK + head_start) = (0x01 << 5) | sad_len;

	 *bNo_p = bNo;
}


void
vCEAAudioDataAndOperation(struct MTK_HDMI *myhdmi,
	u8 *prData,
	u8 bCEALen,
	u8 *poBLK,
	u8 *poCnt)
{
	u8 bTemp;
	u8 bType, bNo, i;
	u8 pcm_max_chan = 1;
	u8 TotalLen = 0, dec = 0, chan = 0;
	u8 Aud_Blk[E_AUD_BLK_LEN];

	DBGLOG("[RX]%s\n", __func__);

	for (i = 0; i < sizeof(Aud_Blk); i++)
		Aud_Blk[i] = 0;

	if (fgOnlySupportPcm2ch(myhdmi) == TRUE) {
		// 2ch only, update audio block
		for (i = 0; i < sizeof(AudDataBlk_2CH_ONLY); i++)
			Aud_Blk[i] = AudDataBlk_2CH_ONLY[i];
	} else {
		for (i = 0; i < sizeof(Audio_Data_Block); i++)
			Aud_Blk[i] = Audio_Data_Block[i];
	}

	// TotalLen=    Audio_Data_Block[0]&0x1f;
	if (fgOnlySupportPcm2ch(myhdmi) == TRUE)
		TotalLen = E_AUD2CHPCM_ONLY_LEN - 1;
	else
		TotalLen = E_AUD_BLK_LEN - 1;
	i = 1;
	while (TotalLen) {
		// check default PCM how many channels are supportted
		if (i < sizeof(Aud_Blk)) {
			dec = (Aud_Blk[i] >> 3) & 0x0f;
			if (dec == 0x01) {
				chan = (Aud_Blk[i] & 0x07);
				if (chan > pcm_max_chan)
					pcm_max_chan = chan;
			}
		}

		i += 3;
		if (TotalLen >= 3)	// for reduce warning
			TotalLen -= 3;
		else
			TotalLen = 0;
	}

	DBGLOG("[RX]pcm_max_chan =%d\n", pcm_max_chan);

	while (bCEALen > 0) {
		// Step 1: get 1st data block type & total number of this data
		// type
		bTemp = *prData;
		bType = bTemp >> 5;	// bit[7:5]
		bNo = bTemp & 0x1F;	// bit[4:0]
		// Audio data block
		if (bType == 0x01)
			audio_data_blk(myhdmi,
				prData,
				poBLK,
				poCnt,
				&Aud_Blk[0],
				&bNo);

		// re-assign the next data block address
		// '1' means the tag byte
		prData += (bNo + 1);

		if (bCEALen >= (bNo + 1))
			bCEALen -= (bNo + 1);
		else
			bCEALen = 0;
	}
}

void aud_2ch_to_8ch(struct MTK_HDMI *myhdmi,
	u8 *k_p,
	u8 *idx_p,
	u8 *temp)
{
	u8 k = *k_p;
	u8 idx = *idx_p;

	for (idx = 0; idx < 7; idx++) {
		// 2ch to 8ch
		if (SCP->s_org_pcm_ch_samp[idx] != 0) {
			if ((k + 2) < sizeof(temp)) {
				temp[k] = ((0x1 << 3) | (idx + 1));
				temp[k + 1] = SCP->s_org_pcm_ch_samp[idx];
				temp[k + 2] = SCP->s_org_pcm_bit_size[idx];
			}
			k += 3;
		}
	}
}

void ComposeAudioBlock(struct MTK_HDMI *myhdmi,
	u8 u1AndOperation)
{
	u8 TotalLen, i, j, k, idx = 0, dec, chan;
	u8 temp[E_AUD_BLK_LEN + 15];
	u8 Aud_Blk[E_AUD_BLK_LEN];
	u16 sink_dec;

	dec = 0;
	chan = 0;
	for (i = 0; i < (E_AUD_BLK_LEN + 15); i++) {
		myhdmi->CP_AudDataBLK[i] = 0;
		temp[i] = 0;
	}

	for (i = 0; i < E_AUD_BLK_LEN; i++)
		Aud_Blk[i] = 0;

	if (fgOnlySupportPcm2ch(myhdmi) == TRUE) {
		// 2ch only, update audio block
		for (i = 0; i < sizeof(AudDataBlk_2CH_ONLY); i++)
			Aud_Blk[i] = AudDataBlk_2CH_ONLY[i];
	} else {
		for (i = 0; i < sizeof(Audio_Data_Block); i++)
			Aud_Blk[i] = Audio_Data_Block[i];
	}

	sink_dec = SCP->s_aud_dec;
	if (fgOnlySupportPcm2ch(myhdmi) == TRUE)
		TotalLen = E_AUD2CHPCM_ONLY_LEN - 1;
	else
		TotalLen = E_AUD_BLK_LEN - 1;
	temp[0] = Aud_Blk[0];

	i = 1;
	k = 1;
	while (TotalLen) {
		if (i < sizeof(Aud_Blk)) {
			dec = (Aud_Blk[i] >> 3) & 0x0f;	// Default Support
			chan = (Aud_Blk[i] & 0x07) + 1;
		}
		if ((k + 2) >= (E_AUD_BLK_LEN + 15)) {
			RX_DEF_LOG("[RX] aud blk too long\n");
			return;
		}
		switch (dec) {
		case 0x01:	// PCM
			// Default And read EDID
			if (u1AndOperation) {
				// Here, we need add UI check
				if (chan == 8) {
					// skip 2ch
					// Deal with PCM
					aud_2ch_to_8ch(myhdmi,
						&k, &idx, &temp[0]);
				}
			} else {
				if (((k + 2) < sizeof(temp)) &&
				    ((i + 2) < sizeof(Aud_Blk))) {
					temp[k] = Aud_Blk[i];
					temp[k + 1] = Aud_Blk[i + 1];
					temp[k + 2] = Aud_Blk[i + 2];
				}
				k += 3;
			}
			break;

		case 0x02:	// Ac3
		case 0x06:	// AAC
		case 0x07:	// DTS
		case 0x09:	// DSD
		case 0x0A:	// DD+
		case 0x0B:	// DTS-HD
		case 0x0C:	// MAT
			// Default And read EDID
			if (u1AndOperation) {
				if (((sink_dec &
					HDMI_SINK_AUDIO_DEC_AC3) &&
					(dec == 0x02)) ||
					((sink_dec &
					HDMI_SINK_AUDIO_DEC_AAC) &&
					(dec == 0x06)) ||
					((sink_dec &
					HDMI_SINK_AUDIO_DEC_DTS) &&
					(dec == 0x07)) ||
					((sink_dec &
					HDMI_SINK_AUDIO_DEC_DSD) &&
					(dec == 0x09)) ||
					((sink_dec &
					HDMI_SINK_AUDIO_DEC_DOVI_PLUS) &&
					(dec == 0x0A)) ||
					((sink_dec &
					HDMI_SINK_AUDIO_DEC_MAT_MLP) &&
					(dec == 0x0C))) {
					if (((k + 2) < sizeof(temp)) &&
					    ((i + 2) < sizeof(Aud_Blk))) {
						temp[k] = Aud_Blk[i];
						temp[k + 1] = Aud_Blk[i + 1];
						temp[k + 2] = Aud_Blk[i + 2];
					}
					k += 3;
				} else {
					if ((k + 2) < sizeof(temp)) {
						temp[k] = 0;
						temp[k + 1] = 0;
						temp[k + 2] = 0;
					}
					k += 3;
				}
			} else {
				if (((k + 2) < sizeof(temp)) &&
				    ((i + 2) < sizeof(Aud_Blk))) {
					temp[k] = Aud_Blk[i];
					temp[k + 1] = Aud_Blk[i + 1];
					temp[k + 2] = Aud_Blk[i + 2];
				}
				k += 3;
			}
			break;
		default:
			break;
		}

		i += 3;
		if (TotalLen >= 3)	// for reduce warning
			TotalLen -= 3;
		else
			TotalLen = 0;
	}

	TotalLen = 0;
	j = 1;
	for (i = 1; (i + 2) < (E_AUD_BLK_LEN + 15); i += 3) {
		if (temp[i] != 0) {
			myhdmi->CP_AudDataBLK[j] = temp[i];
			myhdmi->CP_AudDataBLK[j + 1] = temp[i + 1];
			myhdmi->CP_AudDataBLK[j + 2] = temp[i + 2];
			TotalLen += 3;
			j += 3;
		}
	}

	myhdmi->CP_AudDataBLK[0] = (0x01 << 5) | TotalLen;

	if (EDIDDBG) {
		for (i = 0; i < (TotalLen + 1); i++)
			ELOG("[RX]CP_AudDataBLK[%d/%d]=0x%x\n",
				   i, TotalLen, myhdmi->CP_AudDataBLK[i]);
	}
}

void
vCEASpkAndOperation(struct MTK_HDMI *myhdmi,
	u8 *prData,
	u8 bCEALen,
	u8 *poBLK,
	u8 *poCnt)
{
	u8 bTemp;
	u8 bType, bNo, sink_speak;

	DBGLOG("[RX]%s\n", __func__);

	while (bCEALen > 0) {
		// Step 1: get 1st data block type & total number of this data
		// type
		bTemp = *prData;
		bType = bTemp >> 5;	// bit[7:5]
		bNo = bTemp & 0x1F;	// bit[4:0]

		if ((bType == 0x04) && (bNo == 3)) {
			// speaker allocation tag code, 0x04
			*(poBLK + *poCnt) = *prData;	// header
			*poCnt = *poCnt + 1;
			sink_speak = *(prData + 1);

			if (fgOnlySupportPcm2ch(myhdmi) == TRUE)
				*(poBLK + *poCnt) =
					SpkDataBlk_2CH_ONLY[1] &
					sink_speak;
			else
				*(poBLK + *poCnt) =
					Speaker_Data_Block[1] &
					sink_speak;

			*poCnt = *poCnt + 1;
			*(poBLK + *poCnt) = *(prData + 2);
			*poCnt = *poCnt + 1;

			*(poBLK + *poCnt) = *(prData + 3);
			*poCnt = *poCnt + 1;
		}
		// re-assign the next data block address
		prData += (bNo + 1);	// '1' means the tag byte

		if (bCEALen >= (bNo + 1))
			bCEALen -= (bNo + 1);
		else
			bCEALen = 0;
	}			// while(bCEALen)
}

void ComposeSpeakerAllocate(struct MTK_HDMI *myhdmi,
	u8 u1AndOperation)
{
	u8 sink_speak, i;
	u8 u1speakalltemp[E_SPK_BLK_LEN];

	sink_speak = SCP->s_spk_allocation;

	if (fgOnlySupportPcm2ch(myhdmi) == TRUE) {
		for (i = 0; i < E_SPK_BLK_LEN; i++)
			u1speakalltemp[i] = SpkDataBlk_2CH_ONLY[i];
	} else {
		for (i = 0; i < E_SPK_BLK_LEN; i++)
			u1speakalltemp[i] = Speaker_Data_Block[i];
	}

	DBGLOG("[RX]sink_speak = 0x%x\n", sink_speak);

	if (u1AndOperation) {
		if (sink_speak != 0)
			myhdmi->CP_SPKDataBLK[0] = u1speakalltemp[0];
		else
			myhdmi->CP_SPKDataBLK[0] = 0;

		if (sink_speak != 0)
			myhdmi->CP_SPKDataBLK[1] =
				u1speakalltemp[1] & sink_speak;
		else
			myhdmi->CP_SPKDataBLK[1] = 0;

		if (sink_speak != 0) {
			myhdmi->CP_SPKDataBLK[2] = u1speakalltemp[2];
			myhdmi->CP_SPKDataBLK[3] = u1speakalltemp[3];
		} else {
			myhdmi->CP_SPKDataBLK[2] = 0;
			myhdmi->CP_SPKDataBLK[3] = 0;
		}
	} else {
		myhdmi->CP_SPKDataBLK[0] = u1speakalltemp[0];
		myhdmi->CP_SPKDataBLK[1] = u1speakalltemp[1];
		myhdmi->CP_SPKDataBLK[2] = u1speakalltemp[2];
		myhdmi->CP_SPKDataBLK[3] = u1speakalltemp[3];
	}
}

u8 const aTxEDIDVSDBHeader[3] = { 0x03, 0x0c, 0x00 };

u8 u1BitCnt(u32 u4Data)
{
	u8 u1Conter = 0, u1Tmp;

	for (u1Tmp = 0; u1Tmp < 32; u1Tmp++)
		if (u4Data & (1 << u1Tmp))
			u1Conter++;
	return u1Conter;
}

bool fgSupport4K2K(struct MTK_HDMI *myhdmi)
{
	bool fgReturn = FALSE;
	u32 u4CEASinkCap;

	u4CEASinkCap = SCP->s_hdmi_4k2kvic;
	DBGLOG("[RX] u4CEASinkCap = 0x%x,myhdmi->Edid.Number = 0x%x\n",
		u4CEASinkCap, myhdmi->Edid.Number);

	if ((u4CEASinkCap & (SINK_2160P_23_976HZ | SINK_2160P_24HZ |
			     SINK_2160P_25HZ | SINK_2160P_29_97HZ |
			     SINK_2160P_30HZ | SINK_2161P_24HZ)))
		fgReturn = TRUE;

	DBGLOG("[RX] fgReturn = %d\n", fgReturn);

	return fgReturn;
}

void
ComposeVSDB(struct MTK_HDMI *myhdmi,
	u8 u1VidAndOP,
	u8 u1AudAndOP,
	u8 *prData,
	u8 bCEALen,
	u8 *poBLK,
	u8 *poCnt)
{
	u8 i, sink_deep = 0, sink_latency = 0;
	u8 bVsdbLen = 0, b4kLen = 0;
	u8 bNo, bTemp, bType, bVsdbNo = 0;
	u8 head_start = 0, max_tmds_clk = 0;
	u8 data_temp = 0, phy_addr_no, latency_offset;
	u8 u13DPresent = 0, u13DMulitipresent = 0, u13DLenOff = 0;
	u32 E3DFPRes, u4RxEDID3DSBSRes, E3DTBRes;
	u32 EStructAllRes;
	u16 u2RxEDIDStructAll = 0, EDIDMASK = 0;
	u8 u13DLen = 0;

	DBGLOG("[RX]%s, u1VidAndOP =%d\n",
		__func__, u1VidAndOP);
	DBGLOG("[RX]%s, u1AudAndOP =%d\n",
		__func__, u1AudAndOP);
	DBGLOG("[RX]%s, poCnt =0x%x,poBLK = 0x%x\n",
		__func__, *poCnt, *poBLK);
	DBGLOG("[RX]%s, *prData =0x%x, bCEALen = %d\n",
		__func__, *prData, bCEALen);

	for (i = 0; i < 5; i++)
		myhdmi->PHY_ADR_ST[i] = 0;

	phy_addr_no = 0;

	while (bCEALen > 0) {
		// Step 1: get 1st data block type & total number of this data
		// type
		bTemp = *prData;
		bType = bTemp >> 5;	// bit[7:5]
		bNo = bTemp & 0x1F;	// bit[4:0]

		if (bType != 0x03)
			goto ComposeVSDB_10;

		// VDSB exist
		for (bTemp = 0; bTemp < 3; bTemp++) {
			if (*(prData + bTemp + 1) !=
				aTxEDIDVSDBHeader[bTemp]) {
				break;
			}
		}

		// for loop to end, is. VSDB header match
		if (bTemp != 3)
			goto ComposeVSDB_10;

		if (EDIDDBG) {
			for (i = 0; i < (*prData & 0x1f) + 1; i++)
				ELOG("[RX]tx vsdb[%d]=%x\n",
					i, prData[i]);
		}

		latency_offset = 0;
		head_start = *poCnt;

		*(poBLK + *poCnt) = ((0x03 << 5) | bNo);
		*poCnt = *poCnt + 1;
		for (i = 1; i < 4; i++) {
			*(poBLK + *poCnt) = VendDataBLK_Full[i];
			*poCnt = *poCnt + 1;
		}

		*(poBLK + *poCnt) = *(prData + 4);
		if (phy_addr_no < 5)
			myhdmi->PHY_ADR_ST[phy_addr_no] = *poCnt;
		myhdmi->Edid.PHYPoint = myhdmi->PHY_ADR_ST[0];

		phy_addr_no++;
		*poCnt = *poCnt + 1;
		*(poBLK + *poCnt) = *(prData + 5);
		*poCnt = *poCnt + 1;
		myhdmi->PHYAdr.Org = (*(prData + 4) << 8) | *(prData + 5);

		DBGLOG("[RX]Vsdb#%d Addr.Org=%x\n",
			phy_addr_no,
			myhdmi->PHYAdr.Org);

		bVsdbNo = bNo;
		bVsdbLen = 5;

		if (u1VidAndOP == 0) {
			*(poBLK + *poCnt) = VendDataBLK_Full[6];
			*poCnt = *poCnt + 1;
			*(poBLK + *poCnt) = VendDataBLK_Full[7];
			*poCnt = *poCnt + 1;
			*(poBLK + *poCnt) = VendDataBLK_Full[8];
			*poCnt = *poCnt + 1;
			*(poBLK + *poCnt) = VendDataBLK_Full[9];
			*poCnt = *poCnt + 1;
			*(poBLK + *poCnt) = VendDataBLK_Full[10];
			*poCnt = *poCnt + 1;
			*(poBLK + head_start) =
			    (0x03 << 5) | (VendDataBLK_Full[0] & 0x1f);
			goto ComposeVSDB_9;
		}

		if (bVsdbNo >= 6)
			sink_deep = *(prData + 6);
		sink_latency = *(prData + 8);
		if (u1AudAndOP == FALSE) {
		/* Speaker,  speaker+hdmi */
			if (bVsdbNo >= 6)
				/* AI bit should be default */
				*(poBLK + *poCnt) =
					(sink_deep & VendDataBLK_Full[6]) |
					(VendDataBLK_Full[6] & 0x80);
			else
				*(poBLK + *poCnt) =
				    VendDataBLK_Full[6] & 0x80;
			*poCnt = *poCnt + 1;
			bVsdbLen += 1;
		} else {
			if (bVsdbNo >= 6) {
				*(poBLK + *poCnt) =
				    sink_deep & VendDataBLK_Full[6];
				*poCnt = *poCnt + 1;
				bVsdbLen += 1;
			}
		}
		if (bVsdbNo >= 7) {
			max_tmds_clk = *(prData + 7);
			if (max_tmds_clk < VendDataBLK_Full[7])
				*(poBLK + *poCnt) = max_tmds_clk;
			else
				*(poBLK + *poCnt) =
				    VendDataBLK_Full[7];
			*poCnt = *poCnt + 1;
			bVsdbLen += 1;
		}
		if (bVsdbNo >= 8) {
			data_temp = *(prData + 8);
			if (0)
				// bypass consider sink latency, CNC copy
				*(poBLK + *poCnt) =
					(data_temp & VendDataBLK_Full[8]) |
					(data_temp & 0xc0) | (data_temp & 0x0f);
			else
				*(poBLK + *poCnt) =
					(data_temp & VendDataBLK_Full[8]) |
					(data_temp & 0x0f);
			*poCnt = *poCnt + 1;
			bVsdbLen += 1;
		}
		if (bVsdbNo >= 9) {
			if (*(prData + 8) & 0x80) {
				/* sink_p_latency_presen temply Using OR */
				if (*(poBLK + 8) & 0x80) {
					*(poBLK + *poCnt) =
						*(prData + 9);
					*poCnt = *poCnt + 1;
				}
				latency_offset += 1;
				if (*(poBLK + 8) & 0x80) {
					*(poBLK + *poCnt) =
						*(prData + 10);
					*poCnt = *poCnt + 1;
				}
				latency_offset += 1;
				if (*(poBLK + 8) & 0x80)
					bVsdbLen += 2;
			}
			if (*(prData + 8) & 0x40) {
				/* sink_i_latency_present */
				if (bVsdbNo >= 11) {
					if (*(poBLK + 8) & 0x40) {
						*(poBLK + *poCnt) =
							*(prData + 11);
						*poCnt = *poCnt + 1;
					}
					latency_offset += 1;
					if (*(poBLK + 8) & 0x40) {
						*(poBLK + *poCnt) =
							*(prData + 12);
						*poCnt = *poCnt + 1;
					}
					latency_offset += 1;
					if (*(poBLK + 8) & 0x40)
						bVsdbLen += 2;
				}
			}
			if (bVsdbNo >= (9 + latency_offset)) {
				u13DPresent =
				    *(prData + 9 + latency_offset) & 0x80;
				u13DMulitipresent =
				    *(prData + 9 + latency_offset) & 0x60;
				if (u13DMulitipresent == 0 ||
					u13DMulitipresent == 0x60)
					// not support 3D Muliti Present
					*(poBLK + *poCnt) =
						*(prData + 9 + latency_offset) &
						((VendDataBLK_Full[9] & 0x9F) |
						u13DMulitipresent);
				else // 3D Muliti present = 0x40
					*(poBLK + *poCnt) =
						VendDataBLK_Full[9];
				*poCnt = *poCnt + 1;
				bVsdbLen += 1;
			}
			if (bVsdbNo >= (10 + latency_offset)) {
				*(poBLK + *poCnt) =
					VendDataBLK_Full[10] & 0xE0;
				// HDMI_3D_LEN
				u13DLenOff = *poCnt;
				// the 3D len address
				*poCnt = *poCnt + 1;
				bVsdbLen += 1;
				if (fgSupport4K2K(myhdmi)) {
					b4kLen = 0;
					if ((SCP->s_hdmi_4k2kvic &
					    SINK_2160P_30HZ) ||
					    (SCP->s_hdmi_4k2kvic &
						SINK_2160P_29_97HZ)) {
						b4kLen++;
						*(poBLK + *poCnt) = 1;
						*poCnt = *poCnt + 1;
						bVsdbLen += 1;
					}
					if (SCP->s_hdmi_4k2kvic &
					    SINK_2160P_25HZ) {
						b4kLen++;
						*(poBLK + *poCnt) = 2;
						*poCnt = *poCnt + 1;
						bVsdbLen += 1;
					}
					if ((SCP->s_hdmi_4k2kvic &
					     SINK_2160P_23_976HZ)
					    || (SCP->s_hdmi_4k2kvic &
						SINK_2160P_24HZ)) {
						b4kLen++;
						*(poBLK + *poCnt) = 3;
						*poCnt = *poCnt + 1;
						bVsdbLen += 1;
					}
					if (SCP->s_hdmi_4k2kvic &
					    SINK_2161P_24HZ) {
						b4kLen++;
						*(poBLK + *poCnt) = 4;
						*poCnt = *poCnt + 1;
						bVsdbLen += 1;
					}
				}
			}
			if ((u13DPresent == 0x80) &&
			    (u13DMulitipresent ==
			     0x20 || u13DMulitipresent == 0x40)) {
				// compose the 3d
				// structure
				E3DFPRes =
				    SCP->s_cea_FP_3D &
				    (SINK_1080P25 | SINK_1080I50 |
				     SINK_1080I60 | SINK_1080P24 |
				     SINK_1080P30 | SINK_720P24 |
				     SINK_720P50 | SINK_720P60);
				u4RxEDID3DSBSRes =
				    SCP->s_cea_SBS_3D &
				    (SINK_1080P25 | SINK_1080I50 |
				     SINK_1080I60 | SINK_1080P24 |
				     SINK_1080P30 | SINK_1080P50 |
				     SINK_1080P60 | SINK_720P50 |
				     SINK_720P60);
				E3DTBRes =
				    SCP->s_cea_TOB_3D &
				    (SINK_1080P25 | SINK_1080I50 |
				     SINK_1080I60 | SINK_1080P24 |
				     SINK_1080P30 | SINK_1080P50 |
				     SINK_1080P60 | SINK_720P50 |
				     SINK_720P60);

				if ((u1BitCnt(E3DFPRes &
					u4RxEDID3DSBSRes) >
				     u1BitCnt(E3DFPRes &
					E3DTBRes)) &&
				    (u1BitCnt(E3DFPRes &
					u4RxEDID3DSBSRes) >
					u1BitCnt(u4RxEDID3DSBSRes &
					E3DTBRes))) {
					if (u1BitCnt(E3DFPRes &
						u4RxEDID3DSBSRes) >
						((u1BitCnt(E3DFPRes &
						u4RxEDID3DSBSRes &
						E3DTBRes) * 3) / 2)) {
						u2RxEDIDStructAll = 0x0101;
						EStructAllRes =
						    E3DFPRes &
						    u4RxEDID3DSBSRes;
					} else {
						u2RxEDIDStructAll = 0x0141;
						EStructAllRes =
						    E3DFPRes &
						    u4RxEDID3DSBSRes &
						    E3DTBRes;
					}
				} else if (u1BitCnt(E3DFPRes &
					E3DTBRes) >
					u1BitCnt(u4RxEDID3DSBSRes &
					E3DTBRes)) {
					if (u1BitCnt(E3DFPRes &
						E3DTBRes) >
					    ((u1BitCnt(E3DFPRes &
						u4RxEDID3DSBSRes &
						E3DTBRes) * 3) / 2)) {
						u2RxEDIDStructAll = 0x0041;
						EStructAllRes =
						    E3DFPRes &
						    E3DTBRes;
					} else {
						u2RxEDIDStructAll = 0x0141;
						EStructAllRes =
						    E3DFPRes &
						    u4RxEDID3DSBSRes &
						    E3DTBRes;
					}
				} else {
					if (u1BitCnt(u4RxEDID3DSBSRes &
						E3DTBRes) >
					    ((u1BitCnt(E3DFPRes &
						u4RxEDID3DSBSRes &
						E3DTBRes) * 3) / 2)) {
						u2RxEDIDStructAll = 0x0140;
						EStructAllRes =
						    u4RxEDID3DSBSRes &
						    E3DTBRes;
					} else {
						u2RxEDIDStructAll = 0x0141;
						EStructAllRes =
						    E3DFPRes &
						    u4RxEDID3DSBSRes &
						    E3DTBRes;
					}
				}
				// 3D_Structure_ALL_16_8
				*(poBLK + *poCnt) =
					(u2RxEDIDStructAll >> 8) & 0xFF;
				// 3D_Structure_ALL_8_1
				*(poBLK + *poCnt + 1) =
					u2RxEDIDStructAll & 0xFF;
				*poCnt = *poCnt + 2;
				bVsdbLen += 2;
				u13DLen += 2;

				EDIDMASK = 0;
				for (i = 0; i < 16; i++) {
					if ((myhdmi->EFirst16VIC[i] &
					     EStructAllRes) != 0)
						EDIDMASK = EDIDMASK | (1 << i);
				}
				// 3D_MASK_16_8
				*(poBLK + *poCnt) = (EDIDMASK >> 8) & 0xFF;
				// 3D_MASK_ALL_8_1
				*(poBLK + *poCnt + 1) = EDIDMASK & 0xFF;
				*poCnt = *poCnt + 2;
				bVsdbLen += 2;
				u13DLen += 2;

			for (i = 0; i < 16; i++) {
				if ((myhdmi->EFirst16VIC[i] &
					(E3DFPRes ^ EStructAllRes)) != 0) {
					*(poBLK + *poCnt) =	((i << 4) &
						0xF0) | (0x00 & 0x0F);
					*poCnt = *poCnt + 1;
					bVsdbLen += 1;
					u13DLen += 1;
				}
				if ((myhdmi->EFirst16VIC[i] &
				     (E3DTBRes ^ EStructAllRes)) != 0) {
					*(poBLK + *poCnt) =	((i << 4) &
						0xF0) | (0x06 & 0x0F);
					*poCnt = *poCnt + 1;
					bVsdbLen += 1;
					u13DLen += 1;
				}
				if ((myhdmi->EFirst16VIC[i] &
				     (u4RxEDID3DSBSRes ^ EStructAllRes)) != 0) {
					*(poBLK + *poCnt) = ((i << 4) &
						0xF0) | (0x08 & 0x0F);
					*(poBLK + *poCnt + 1) = 0x10;
					*poCnt = *poCnt + 2;
					bVsdbLen += 2;
					u13DLen += 2;
				}
			}
			}
		}
		*(poBLK + head_start) = (0x03 << 5) | (bVsdbLen);
		if (u13DLenOff != 0) {
			*(poBLK + u13DLenOff) = (b4kLen << 5) | (u13DLen);
			// Length
			DBGLOG("[RX]b4kLen=%d,u13DLen=%d,%s=0x%x\n",
			   b4kLen, u13DLen,
			   "poBLK+u13DLenOff",
			   *(poBLK + u13DLenOff));
		}

ComposeVSDB_9:

		if (EDIDDBG) {
			for (i = 0; i < (*(poBLK + head_start) & 0x1f) + 1; i++)
				ELOG("[RX]rx vsdb[%d]=%x\n",
					   i, *(poBLK + head_start + i));
		}

ComposeVSDB_10:

		// re-assign the next data block address
		prData += (bNo + 1);	// '1' means the tag byte

		if (bCEALen >= (bNo + 1))
			bCEALen -= (bNo + 1);
		else
			bCEALen = 0;
	}
}

void ComposeMiniVSDB(struct MTK_HDMI *myhdmi,
	u8 u1AndOperation)
{
	u8 i;

	for (i = 0; i < E_VEND_BLK_MINI_LEN; i++)
		myhdmi->CP_VSDBDataBLK[i] = VendDataBLK_Mini[i];

	for (i = 0; i < 5; i++)
		myhdmi->PHY_ADR_ST[i] = 0;
}

void ComposeHFVSDB(struct MTK_HDMI *myhdmi,
	u8 u1AndOperation)
{
	DBGLOG("[RX]%s\n", __func__);

	if (u1AndOperation) {
		myhdmi->CP_HFVSDBDataBLK[8] = 0;
		if (SCP->s_vrr_en == 1)
			myhdmi->CP_HFVSDBDataBLK[0] = (3 << 5) | 10;
		else if (SCP->s_allm_en == 1)
			myhdmi->CP_HFVSDBDataBLK[0] = (3 << 5) | 8;
		else
			myhdmi->CP_HFVSDBDataBLK[0] = (3 << 5) | 7;
		myhdmi->CP_HFVSDBDataBLK[1] = 0xd8;
		myhdmi->CP_HFVSDBDataBLK[2] = 0x5d;
		myhdmi->CP_HFVSDBDataBLK[3] = 0xc4;
		myhdmi->CP_HFVSDBDataBLK[4] = 1;
		if (SCP->s_hfvsdb_blk[5] < 121)
			myhdmi->CP_HFVSDBDataBLK[5] =
			    SCP->s_hfvsdb_blk[5];
		else
			myhdmi->CP_HFVSDBDataBLK[5] = 120;	// 600m
		myhdmi->CP_HFVSDBDataBLK[6] = 0x80;
		myhdmi->CP_HFVSDBDataBLK[7] =
		    SCP->s_dc420_color_bit & 0x03;
		if (SCP->s_allm_en == 1)
			myhdmi->CP_HFVSDBDataBLK[8] |= (1 << 1);
		if (SCP->s_vrr_en == 1) {
			myhdmi->CP_HFVSDBDataBLK[9] = SCP->vrr_min;
			myhdmi->CP_HFVSDBDataBLK[10] = SCP->vrr_max;
			if (SCP->vrr_min < 24)
				myhdmi->CP_HFVSDBDataBLK[9] = 24;
			if (SCP->vrr_max > 60)
				myhdmi->CP_HFVSDBDataBLK[10] = 60;
		}
	} else {
		myhdmi->CP_HFVSDBDataBLK[0] = (3 << 5) | 7;
		myhdmi->CP_HFVSDBDataBLK[1] = 0xd8;
		myhdmi->CP_HFVSDBDataBLK[2] = 0x5d;
		myhdmi->CP_HFVSDBDataBLK[3] = 0xc4;
		myhdmi->CP_HFVSDBDataBLK[4] = 1;
		myhdmi->CP_HFVSDBDataBLK[5] = 120;	// 600m
		myhdmi->CP_HFVSDBDataBLK[6] = 0x80;
		myhdmi->CP_HFVSDBDataBLK[7] = 0x03;
	}

	DBGLOG("[RX]hf-vsdb:%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
	   myhdmi->CP_HFVSDBDataBLK[0],
	   myhdmi->CP_HFVSDBDataBLK[1],
	   myhdmi->CP_HFVSDBDataBLK[2],
	   myhdmi->CP_HFVSDBDataBLK[3],
	   myhdmi->CP_HFVSDBDataBLK[4],
	   myhdmi->CP_HFVSDBDataBLK[5],
	   myhdmi->CP_HFVSDBDataBLK[6],
	   myhdmi->CP_HFVSDBDataBLK[7],
	   myhdmi->CP_HFVSDBDataBLK[8]);
}

void ComposeColoriMetry(struct MTK_HDMI *myhdmi,
	u8 u1AndOperation)
{
	u8 sink_xvYcc = 0, sink_metadata = 0;

	DBGLOG("[RX]%s\n", __func__);

	if (SCP->s_colorimetry & SINK_XV_YCC601)
		sink_xvYcc |= 0x01;

	if (SCP->s_colorimetry & SINK_XV_YCC709)
		sink_xvYcc |= 0x02;

	if (SCP->s_colorimetry & SINK_COLOR_SPACE_BT2020_CYCC)
		sink_xvYcc |= 0x20;

	if (SCP->s_colorimetry & SINK_COLOR_SPACE_BT2020_YCC)
		sink_xvYcc |= 0x40;

	if (SCP->s_colorimetry & SINK_COLOR_SPACE_BT2020_RGB)
		sink_metadata |= 0x80;

	if (SCP->s_colorimetry & SINK_METADATA0)
		sink_metadata |= 0x01;

	if (SCP->s_colorimetry & SINK_METADATA1)
		sink_metadata |= 0x02;

	if (SCP->s_colorimetry & SINK_METADATA2)
		sink_metadata |= 0x04;

	if (SCP->s_colorimetry & SINK_METADATA3)
		sink_metadata |= 0x08;

	if (u1AndOperation) {
		myhdmi->CP_ColorimetryDataBLK[0] =
			Colorimetry_Data_Block[0];
		myhdmi->CP_ColorimetryDataBLK[1] =
			Colorimetry_Data_Block[1];
		myhdmi->CP_ColorimetryDataBLK[2] =
			Colorimetry_Data_Block[2] & sink_xvYcc;
		myhdmi->CP_ColorimetryDataBLK[3] =
		    Colorimetry_Data_Block[3] & sink_metadata;
	} else {
		myhdmi->CP_ColorimetryDataBLK[0] =
			Colorimetry_Data_Block[0];
		myhdmi->CP_ColorimetryDataBLK[1] =
			Colorimetry_Data_Block[1];
		myhdmi->CP_ColorimetryDataBLK[2] =
			Colorimetry_Data_Block[2];
		myhdmi->CP_ColorimetryDataBLK[3] =
			Colorimetry_Data_Block[3];
	}

	DBGLOG("[RX]cdb:%x,%x,%x,%x\n",
	   myhdmi->CP_ColorimetryDataBLK[0],
	   myhdmi->CP_ColorimetryDataBLK[1],
	   myhdmi->CP_ColorimetryDataBLK[2],
	   myhdmi->CP_ColorimetryDataBLK[3]);
}

void ComposeVCDB(struct MTK_HDMI *myhdmi,
	u8 u1AndOperation)
{
	DBGLOG("[RX]%s\n", __func__);

	//if (u1AndOperation) {
		myhdmi->CP_VcdbDataBLK[0] = Vcdb_Data_Block[0];
		myhdmi->CP_VcdbDataBLK[1] = Vcdb_Data_Block[1];
		myhdmi->CP_VcdbDataBLK[2] =  SCP->s_vcdb_data;
	//} else {
	//	myhdmi->CP_VcdbDataBLK[0] = Vcdb_Data_Block[0];
	//	myhdmi->CP_VcdbDataBLK[1] = Vcdb_Data_Block[1];
	//	myhdmi->CP_VcdbDataBLK[2] =  SCP->s_vcdb_data;
	//}

	DBGLOG("[RX]vcdb:%x,%x,%x\n",
		myhdmi->CP_VcdbDataBLK[0],
		myhdmi->CP_VcdbDataBLK[1],
		myhdmi->CP_VcdbDataBLK[2]);
}

bool Compose420CMDB(struct MTK_HDMI *myhdmi)
{
#if HDMIRX_IS_RPT_MODE
	u8 i;

	DBGLOG("[RX]%s\n", __func__);

	for (i = 0; i < E_420_CMDB_LEN; i++)
		myhdmi->CP_420cmdbDataBLK[i] = 0;

	if ((myhdmi->EDID420BitMap[0] == 0) &&
	    (myhdmi->EDID420BitMap[1] == 0) &&
	    (myhdmi->EDID420BitMap[2] == 0))
		return FALSE;

	myhdmi->CP_420cmdbDataBLK[0] = (0x07 << 5) | 5;
	myhdmi->CP_420cmdbDataBLK[1] = 0x0f;
	myhdmi->CP_420cmdbDataBLK[2] = myhdmi->EDID420BitMap[0];
	myhdmi->CP_420cmdbDataBLK[3] = myhdmi->EDID420BitMap[1];
	myhdmi->CP_420cmdbDataBLK[4] = myhdmi->EDID420BitMap[2];

	DBGLOG("[RX]420-cmdb:%x,%x,%x,%x,%x\n",
		myhdmi->CP_420cmdbDataBLK[0],
		myhdmi->CP_420cmdbDataBLK[1],
		myhdmi->CP_420cmdbDataBLK[2],
		myhdmi->CP_420cmdbDataBLK[3],
		myhdmi->CP_420cmdbDataBLK[4]);
#else
	DBGLOG("[RX]%s, receiver mode, not need\n", __func__);
#endif
	return TRUE;
}

bool Compose420VDB(struct MTK_HDMI *myhdmi,
	u8 u1AndOperation)
{
	u32 sink_4k_vid, u4_4k_420;
	u8 inx;

	DBGLOG("[RX]%s\n", __func__);

	for (inx = 0; inx < E_420_VDB_LEN; inx++)
		myhdmi->CP_420VicDataBLK[inx] = 0;

	sink_4k_vid = SCP->s_hdmi_4k2kvic &
	    (SINK_2160P_50HZ | SINK_2160P_60HZ |
	    SINK_2161P_50HZ | SINK_2161P_60HZ);
	u4_4k_420 = SCP->s_4k2kvic_420_vdb &
	    (SINK_2160P_50HZ | SINK_2160P_60HZ |
	    SINK_2161P_50HZ | SINK_2161P_60HZ);
	u4_4k_420 = u4_4k_420 & (~sink_4k_vid);

	inx = 0;

	if (u1AndOperation) {
		if (u4_4k_420 & SINK_2160P_50HZ) {
			myhdmi->CP_420VicDataBLK[inx + 2] = 96;
			inx++;
		}
		if (u4_4k_420 & SINK_2160P_60HZ) {
			myhdmi->CP_420VicDataBLK[inx + 2] = 97;
			inx++;
		}
		if (u4_4k_420 & SINK_2161P_50HZ) {
			myhdmi->CP_420VicDataBLK[inx + 2] = 101;
			inx++;
		}
		if (u4_4k_420 & SINK_2161P_60HZ) {
			myhdmi->CP_420VicDataBLK[inx + 2] = 102;
			inx++;
		}
	}

	if (inx == 0)
		return FALSE;

	myhdmi->CP_420VicDataBLK[0] = (0x07 << 5) | (inx + 1);
	myhdmi->CP_420VicDataBLK[1] = 0x0e;

	DBGLOG("[RX]420-vdb:%x,%x,%x,%x,%x,%x\n",
	   myhdmi->CP_420VicDataBLK[0],
	   myhdmi->CP_420VicDataBLK[1],
	   myhdmi->CP_420VicDataBLK[2],
	   myhdmi->CP_420VicDataBLK[3],
	   myhdmi->CP_420VicDataBLK[4],
	   myhdmi->CP_420VicDataBLK[5]);

	return TRUE;
}

bool ComposeHDRSMDB(struct MTK_HDMI *myhdmi,
	u8 u1AndOperation)
{
	u8 i, temp;

	DBGLOG("[RX]%s\n", __func__);

	for (i = 0; i < E_HDR_SMDB_LEN; i++)
		myhdmi->CP_HDRSMDataBLK[i] = 0;

	if (u1AndOperation) {
		if (SCP->s_hdr_block[0]) {
			temp = SCP->s_hdr_block[0] & 0x1F;
			myhdmi->CP_HDRSMDataBLK[0] = (0x07 << 5) | temp;
			myhdmi->CP_HDRSMDataBLK[1] = 0x06;
			myhdmi->CP_HDRSMDataBLK[2] =
			    SCP->s_hdr_block[2];
			myhdmi->CP_HDRSMDataBLK[3] =
			    SCP->s_hdr_block[3];
			if (temp >= 4)
				myhdmi->CP_HDRSMDataBLK[4] =
				    SCP->s_hdr_block[4];
			if (temp >= 5)
				myhdmi->CP_HDRSMDataBLK[5] =
				    SCP->s_hdr_block[5];
			if (temp >= 6)
				myhdmi->CP_HDRSMDataBLK[6] =
				    SCP->s_hdr_block[6];

			DBGLOG("[RX]hdr-smdb:%x,%x,%x,%x,%x,%x,%x\n",
			   myhdmi->CP_HDRSMDataBLK[0],
			   myhdmi->CP_HDRSMDataBLK[1],
			   myhdmi->CP_HDRSMDataBLK[2],
			   myhdmi->CP_HDRSMDataBLK[3],
			   myhdmi->CP_HDRSMDataBLK[4],
			   myhdmi->CP_HDRSMDataBLK[5],
			   myhdmi->CP_HDRSMDataBLK[6]);

			return TRUE;
		}
	}

	return FALSE;
}

bool ComposeDoviVisionDB(struct MTK_HDMI *myhdmi,
	u8 u1AndOperation)
{
	u8 i, temp;

	for (i = 0; i < E_DOVIVISION_DB_LEN; i++)
		myhdmi->CP_DoviVision_Data_Block[i] = 0;

	if (u1AndOperation) {
		if (SCP->s_dbvision_blk[0]) {
			temp = SCP->s_dbvision_blk[0] & 0x1F;
			DBGLOG("[RX]dolby Gx%x;\n",
				SCP->s_dbvision_vsvdb_Gx);
			DBGLOG("[RX]dolby Gy%x;\n",
				SCP->s_dbvision_vsvdb_Gy);
			DBGLOG("[RX]dolby Bx%x;\n",
				SCP->s_dbvision_vsvdb_Bx);
			DBGLOG("[RX]dolby By%x;\n",
				SCP->s_dbvision_vsvdb_By);
			DBGLOG("[RX]dolby DB13 %x;\n",
				SCP->s_dbvision_blk[13]);
			DBGLOG("[RX]dolby DB14 %x;\n",
				SCP->s_dbvision_blk[14]);
			DBGLOG("[RX]DoviVision DB:");
			for (i = 0; i <= temp; i++) {
				myhdmi->CP_DoviVision_Data_Block[i] =
					SCP->s_dbvision_blk[i];
				DBGLOG("[%d]:%x,%x\n", i,
					myhdmi->CP_DoviVision_Data_Block[i],
					SCP->s_dbvision_blk[i]);
			}
			return TRUE;
		}
	}

	return FALSE;
}

bool ComposeEdidBlock1(struct MTK_HDMI *myhdmi,
	u8 *prBlk,
	u8 *poBLK)
{
	u8 rEnd, i, DataBlkLen = 0;
	u8 rCnt, oCnt;
	u16 PixClk;
	u8 *prCEAStar, CEALen = 0;
	bool audio_flag, vendor_flag;
	bool speaker_flag;

	audio_flag = FALSE;
	vendor_flag = FALSE;
	speaker_flag = FALSE;

	rEnd = *(prBlk + 2);	// Original EDID CEA description.
	DBGLOG("[RX](prBlk+4) = 0x%p\n ", (prBlk + 4));

	prCEAStar = (prBlk + 4);
	if (rEnd >= 4)
		CEALen = rEnd - 4;

	DataBlkLen = 0;
	oCnt = 0;
	for (i = 0; i < 4; i++, oCnt++)
		*(poBLK + oCnt) = *(prBlk + i);
	// Audio output is HDMI+Speaket or Speaker
	if (myhdmi->AudUseAnd == FALSE)
		*(poBLK + 1) = 3;
	// DVI
	if (SCP->s_supp_hdmi_mode == FALSE) {
		// clear YcbCr444 and Ycbcr422
		*(poBLK + 3) = (*(poBLK + 3)) & (0x8f);
		// Audio output is HDMI+Speaket or Speaker
		if (myhdmi->AudUseAnd == FALSE)
			*(poBLK + 3) = (*(poBLK + 3)) | (1 << 6);
	}
	// The following is for Data Blcok process
	// Process video data Block
	DBGLOG("[RX]myhdmi->fgVidUseAnd =%d\n",
		myhdmi->fgVidUseAnd);
	DBGLOG("[RX]myhdmi->AudUseAnd =%d\n",
		myhdmi->AudUseAnd);

	DBGLOG("[RX]Block1 of video data block addr=0x%x\n", oCnt);

	if (myhdmi->fgVidUseAnd)
		ComposeVideoBlock(myhdmi, 1, myhdmi->AudUseAnd);
	else
		ComposeVideoBlock(myhdmi, 0, myhdmi->AudUseAnd);

	if ((myhdmi->CP_VidDataBLK[0] & 0x1f) != 0) {	/* SVD exist */
		// Add Video block length
		DataBlkLen = DataBlkLen + (myhdmi->CP_VidDataBLK[0] & 0x1f) + 1;
		for (i = 0; i < ((myhdmi->CP_VidDataBLK[0] & 0x1f) + 1);
		i++, oCnt++)
			*(poBLK + oCnt) = myhdmi->CP_VidDataBLK[i];
	}
	// Process Audio data Block
	DBGLOG("[RX]Blk1 of aud data blk addr=0x%x\n", oCnt);

	if (myhdmi->AudUseAnd) {
		vCEAAudioDataAndOperation(myhdmi,
			prCEAStar, CEALen, poBLK, &oCnt);
	} else {
		ComposeAudioBlock(myhdmi, 0);
		/* SAD exist */
		if ((myhdmi->CP_AudDataBLK[0] & 0x1f) != 0) {
			// Add Audio block length
			DataBlkLen = DataBlkLen +
				(myhdmi->CP_AudDataBLK[0] & 0x1f) + 1;
			for (i = 0; i < ((myhdmi->CP_AudDataBLK[0] & 0x1f) + 1);
				i++, oCnt++)
				*(poBLK + oCnt) = myhdmi->CP_AudDataBLK[i];
			audio_flag = TRUE;
		}
	}

	// Process Speaker Allocation
	DBGLOG("[RX]Blk1 of SPK Allocation data blk addr=0x%x\n", oCnt);

	if (myhdmi->AudUseAnd) {
		vCEASpkAndOperation(myhdmi, prCEAStar, CEALen, poBLK, &oCnt);
	} else {
		ComposeSpeakerAllocate(myhdmi, 0);
		if ((myhdmi->CP_SPKDataBLK[0] & 0x1f) != 0) {
			// Add Speaker Aloocate block length
			DataBlkLen = DataBlkLen +
				(myhdmi->CP_SPKDataBLK[0] & 0x1f) + 1;
			for (i = 0; i < ((myhdmi->CP_SPKDataBLK[0] & 0x1f) + 1);
				i++, oCnt++)
				*(poBLK + oCnt) = myhdmi->CP_SPKDataBLK[i];
			speaker_flag = TRUE;
		}
	}

	// Process VSDB
	/* HDMI mode, not DVI */
	if (SCP->s_supp_hdmi_mode == TRUE) {
		DBGLOG("[RX]Blk1 of vend-spec=0x%x\n", oCnt);
		if (myhdmi->fgVidUseAnd)
			ComposeVSDB(myhdmi, 1, myhdmi->AudUseAnd,
				prCEAStar, CEALen, poBLK, &oCnt);
		else
			ComposeVSDB(myhdmi, 0, myhdmi->AudUseAnd,
				prCEAStar, CEALen, poBLK, &oCnt);
	} else {
		/* HDMI Bypass */
		if (myhdmi->AudUseAnd) {
			myhdmi->PHYAdr.Org = 0;
			myhdmi->Edid.PHYPoint = 0;
			for (i = 0; i < 5; i++)
				myhdmi->PHY_ADR_ST[i] = 0;
		} else {
			DBGLOG("[RX]Blk1 of vend-spec(mini)=0x%x\n",
					oCnt);
			ComposeMiniVSDB(myhdmi, 0);
			myhdmi->PHYAdr.Org =
			    (myhdmi->CP_VSDBDataBLK[4] << 8) |
			    myhdmi->CP_VSDBDataBLK[5];
			myhdmi->Edid.PHYPoint = oCnt + 4;
			myhdmi->PHY_ADR_ST[0] = myhdmi->Edid.PHYPoint;
			// Add VSDB length
			DataBlkLen = DataBlkLen +
				(myhdmi->CP_VSDBDataBLK[0] & 0x1f) + 1;
			for (i = 0; i < ((myhdmi->CP_VSDBDataBLK[0] &
				0x1f) + 1); i++, oCnt++)
				*(poBLK + oCnt) = myhdmi->CP_VSDBDataBLK[i];
		}
	}

	// process hf-vsdb
	if (SCP->s_max_tmds_char_rate > 0x44) {
		if (SCP->s_supp_hdmi_mode == TRUE) {
			DBGLOG("[RX]Block1 of hf-vsdb addr=0x%x\n", oCnt);
			ComposeHFVSDB(myhdmi, myhdmi->fgVidUseAnd);
			for (i = 0; i < ((myhdmi->CP_HFVSDBDataBLK[0] &
				0x1f) + 1); i++, oCnt++)
				*(poBLK + oCnt) = myhdmi->CP_HFVSDBDataBLK[i];
		}
	}

	DBGLOG("[RX]myhdmi->PHYAdr.Org = 0x%x\n",
		myhdmi->PHYAdr.Org);
	DBGLOG("[RX]myhdmi->Edid.PHYPoint = %d\n",
		myhdmi->Edid.PHYPoint);

	vendor_flag = TRUE;
	// Process   Colorimetry
	if ((SCP->s_colorimetry & SINK_XV_YCC601) ||
	    (SCP->s_colorimetry &
	     SINK_XV_YCC709) ||
	    (SCP->s_colorimetry &
	     SINK_COLOR_SPACE_BT2020_CYCC) ||
	    (SCP->s_colorimetry & SINK_COLOR_SPACE_BT2020_YCC)) {
		DBGLOG("[RX]Blk1 of colorimetry=0x%x\n", oCnt);

		if (myhdmi->fgVidUseAnd)
			ComposeColoriMetry(myhdmi, 1);
		else
			ComposeColoriMetry(myhdmi, 0);
		// Add Speaker Aloocate block length
		DataBlkLen = DataBlkLen +
			(myhdmi->CP_ColorimetryDataBLK[0] & 0x1f) + 1;
		for (i = 0; i < ((myhdmi->CP_ColorimetryDataBLK[0] &
			0x1f) + 1); i++, oCnt++)
			*(poBLK + oCnt) = myhdmi->CP_ColorimetryDataBLK[i];
	}
	//Process VCDB(video Capability)
	if (SCP->s_vcdb_data) {
		ComposeVCDB(myhdmi, myhdmi->fgVidUseAnd);
		DataBlkLen = DataBlkLen + (myhdmi->CP_VcdbDataBLK[0] & 0x1f) + 1;
		for (i = 0; i < ((myhdmi->CP_VcdbDataBLK[0] & 0x1f) + 1); i++, oCnt++)
			*(poBLK + oCnt) = myhdmi->CP_VcdbDataBLK[i];
	}
	// process 420 cmdb
	if (Compose420CMDB(myhdmi) == TRUE) {
		DBGLOG("[RX]Block1 of 420-cmdb addr=0x%x\n", oCnt);
		for (i = 0; i < ((myhdmi->CP_420cmdbDataBLK[0] &
		0x1f) + 1); i++, oCnt++)
			*(poBLK + oCnt) = myhdmi->CP_420cmdbDataBLK[i];
	}
	// process 420 vdb
	if (Compose420VDB(myhdmi, myhdmi->fgVidUseAnd) == TRUE) {
		DBGLOG("[RX]Block1 of 420-vdb addr=0x%x\n", oCnt);
		for (i = 0; i < ((myhdmi->CP_420VicDataBLK[0] &
			0x1f) + 1); i++, oCnt++)
			*(poBLK + oCnt) = myhdmi->CP_420VicDataBLK[i];
	}
	// process hdr smdb
	if (ComposeHDRSMDB(myhdmi, myhdmi->fgVidUseAnd) == TRUE) {
		DBGLOG("[RX]Block1 of hdr-smdb addr=0x%x\n", oCnt);
		for (i = 0; i < ((myhdmi->CP_HDRSMDataBLK[0] &
			0x1f) + 1); i++, oCnt++)
			*(poBLK + oCnt) = myhdmi->CP_HDRSMDataBLK[i];
	}
	//process dolbyvision datablock
	if (ComposeDoviVisionDB(myhdmi, myhdmi->fgVidUseAnd) == TRUE) {
		DBGLOG("[RX]Block1 of DoviVision addr=0x%x\n", oCnt);
		for (i = 0;
		  i < ((myhdmi->CP_DoviVision_Data_Block[0] & 0x1f) + 1);
		  i++, oCnt++)
			*(poBLK + oCnt) = myhdmi->CP_DoviVision_Data_Block[i];
	}

	*(poBLK + 2) = oCnt;	// CEA description.

	DBGLOG("[RX]Compose DTD=0x%x, oCnt=0x%x\n",
		rEnd, oCnt);

	for (rCnt = rEnd; (oCnt + 18) < 128; rCnt += 18) {
		if ((rCnt + 18) >= 128) {
			DBGLOG("[RX]rCnt+18)>=128\n");
			break;
		}
		PixClk = (*(prBlk + rCnt + 1) << 8) | (*(prBlk + rCnt));
		DBGLOG("[RX]PixClk =%d\n", PixClk);
		if ((PixClk <= F_PIX_MAX) && PixClk) {
			if (fgDTDSupp(myhdmi, prBlk + rCnt) == TRUE) {
				DBGLOG("[RX]Supported DTD\n");
				DBGLOG("[RX]Blk1 of DTD=0x%x\n", oCnt);
				for (i = 0; i < 18; i++, oCnt++)
					*(poBLK + oCnt) = *(prBlk + rCnt + i);
			} else {
				DBGLOG("[RX]UnSupported DTD\n");
			}
		}
	}

	for (; oCnt < 128; oCnt++)
		*(poBLK + oCnt) = 0;

	return TRUE;
}

void EdidProcessing(struct MTK_HDMI *myhdmi)
{
	u8 PhyInx;
	bool b4BlockEdid;
	u8 Extension;
	u16 P1Addr = 0, P2Addr = 0, P3Addr = 0, l;
	u16 j;
	int i;
	bool GotHDMIFlag;

	DBGLOG("[RX]%s\n", __func__);

	myhdmi->Edid.bBlock0Err = FALSE;
	myhdmi->Edid.bBlock1Err = FALSE;
	myhdmi->Edid.bDownDvi = FALSE;

	/*** Block0 ***/
	if (txapi_get_edid(myhdmi, myhdmi->rEBuf, 0, 128) == TRUE) {
		if (Edid_CheckSum_Check(myhdmi, myhdmi->rEBuf) == FALSE) {
			DBGLOG("[RX]Edid CheckSum Err\r\n");
			Default_Edid_BL0_BL1_Write(myhdmi);
			return;
		}
		if (Edid_BL0Header_Check(myhdmi, myhdmi->rEBuf) == FALSE) {
			DBGLOG("[RX]Edid Header Err\r\n");
			Default_Edid_BL0_BL1_Write(myhdmi);
			return;
		}
		if (Edid_BL0Version_Check(myhdmi, myhdmi->rEBuf) == FALSE) {
			DBGLOG("[RX]Edid Version Err\r\n");
			Default_Edid_BL0_BL1_Write(myhdmi);
			return;
		}
		Extension = myhdmi->rEBuf[E_BL0_ADR_EXT_NMB];
		b4BlockEdid = FALSE;
		DBGLOG("[RX]Ext Block = %d\n", Extension);

		switch (Extension) {
		case 0:
		default:
			ComposeEdidBlock0(myhdmi, myhdmi->rEBuf, myhdmi->oEBuf);
			#if (HDMI_INPUT_COUNT == 1)
			vWriteEDIDBlk0(myhdmi, EEPROM0, myhdmi->oEBuf);
			#else
			vWriteEDIDBlk0(myhdmi, EEPROM0, myhdmi->oEBuf);
			vWriteEDIDBlk0(myhdmi, EEPROM1, myhdmi->oEBuf);
			vWriteEDIDBlk0(myhdmi, EEPROM2, myhdmi->oEBuf);
			#endif
			vEDIDCreateBlock1(myhdmi);
			return;

		case 1:
			break;

		case 2:
		case 3:
			DBGLOG("[RX]Edid 4Block Detect\r\n");
			b4BlockEdid = TRUE;
			break;
		}

	} else {
		DBGLOG("[RX]Edid0 Read Error ===\r\n");
		Default_Edid_BL0_BL1_Write(myhdmi);
		return;
	}

	ELOG("[RX]TX Read Blk0 EDID\n");
	vShowEDIDBLKData(EDIDDBG, &myhdmi->rEBuf[0]);

	ComposeEdidBlock0(myhdmi, myhdmi->rEBuf, myhdmi->oEBuf);

	DBGLOG("[RX]After ComposeEdidBlk0 Page0 EDID\n");

#if (HDMI_INPUT_COUNT == 1)
	vWriteEDIDBlk0(myhdmi, EEPROM0, myhdmi->oEBuf);
#else
	vWriteEDIDBlk0(myhdmi, EEPROM0, myhdmi->oEBuf);
	vWriteEDIDBlk0(myhdmi, EEPROM1, myhdmi->oEBuf);
	vWriteEDIDBlk0(myhdmi, EEPROM2, myhdmi->oEBuf);
#endif

	/*** Block1 ***/
	GotHDMIFlag = FALSE;
	for (i = 1; i < 4 && i <= Extension; i++) {
		if (GotHDMIFlag == FALSE) {
			/* block 2 */
			if (txapi_get_edid(myhdmi,
				myhdmi->rEBuf, i, 128) == TRUE) {
				if (ParseHDMIEDID(myhdmi,
					myhdmi->rEBuf) == TRUE) {
					GotHDMIFlag = TRUE;
					ELOG("[RX]TX Blk %2d EDID\n", i);
					vShowEDIDBLKData(EDIDDBG,
						&myhdmi->rEBuf[0]);
				} else {
					GotHDMIFlag = FALSE;
				}
			} else {
				DBGLOG("[RX]Edid1 Read Err\n");
				Default_Edid_BL1_Write(myhdmi);
				return;
			}
		}
	}

	if (GotHDMIFlag == FALSE) {
		RX_DEF_LOG("[rx]get hdmi cea flag fail, Extension=%d\n", Extension);
		Default_Edid_BL1_Write(myhdmi);
		return;
	}

	for (myhdmi->Edid.Number = 0;
		myhdmi->Edid.Number <= EEPROM2;
		myhdmi->Edid.Number++) {
		switch (myhdmi->Edid.Number) {
		case EEPROM0:
			ComposeEdidBlock1(myhdmi, myhdmi->rEBuf, myhdmi->oEBuf);
			if (myhdmi->Edid.PHYPoint != 0) {
			/* Mean that is not DVI */
				for (PhyInx = 0; PhyInx < 16; PhyInx += 4) {
					if (((myhdmi->PHYAdr.Org >>
						PhyInx) & 0x0f) != 0)
						break;
				}
				if (PhyInx != 0) {
					if (PhyInx >= 4)
						PhyInx -= 4;
					P1Addr = (myhdmi->PHYAdr.Org |
						(1 << PhyInx));
					P2Addr = (myhdmi->PHYAdr.Org |
						(2 << PhyInx));
					P3Addr = (myhdmi->PHYAdr.Org |
						(3 << PhyInx));
				} else {
					P1Addr = 0xffff;
					P2Addr = 0xffff;
					P3Addr = 0xffff;
				}
				/* for CLI debug mode */
				if (myhdmi->fgUseMdyPA == TRUE) {
					P1Addr = myhdmi->u2MdyPA;
					P2Addr = myhdmi->u2MdyPA;
					P3Addr = myhdmi->u2MdyPA;
				}
				myhdmi->_u2Port1Addr = P1Addr;
				myhdmi->_u2Port2Addr = P2Addr;
				myhdmi->_u2Port3Addr = P3Addr;
				if (EDIDDBG) {
					ELOG("[RX]P1Addr =0x%x\n", P1Addr);
					ELOG("[RX]P2Addr =0x%x\n", P2Addr);
					ELOG("[RX]P3Addr =0x%x\n", P3Addr);
				}
			}
			for (l = 0; l < 5; l++) {
				if (myhdmi->PHY_ADR_ST[l] != 0) {
					DBGLOG("[RX]1.PHY_ADR_ST[%d]=%x\n",
						l, myhdmi->PHY_ADR_ST[l]);
					j = myhdmi->PHY_ADR_ST[l];
					if (j < sizeof(myhdmi->oEBuf))
						myhdmi->oEBuf[j] =
							(P1Addr >> 8) & 0xff;
					j = j + 1;
					if (j < sizeof(myhdmi->oEBuf))
						myhdmi->oEBuf[j] =
							P1Addr & 0xff;
				}
			}
			myhdmi->oEBuf[E_ADR_CHK_SUM] =
			    Edid_Chk_Sum(myhdmi, myhdmi->oEBuf);
			myhdmi->_u1EDID0CHKSUM = myhdmi->oEBuf[E_ADR_CHK_SUM];
			myhdmi->_u2EDID0PA = P1Addr;
			myhdmi->_u1EDID0PAOFF = myhdmi->Edid.PHYPoint;

			vWriteEDIDBlk1(myhdmi, EEPROM0, myhdmi->oEBuf);

			DBGLOG("[RX]P1Addr = 0x%x\r\n", P1Addr);
			break;

#if (HDMI_INPUT_COUNT != 1)
		case EEPROM1:
			ComposeEdidBlock1(myhdmi, myhdmi->rEBuf, myhdmi->oEBuf);
			if (myhdmi->Edid.PHYPoint != 0) {
				/* Mean that is not DVI */
				for (PhyInx = 0; PhyInx < 16; PhyInx += 4) {
					if (((myhdmi->PHYAdr.Org >> PhyInx) &
						0x0f) != 0)
						break;
				}
				if (PhyInx != 0) {
					if (PhyInx >= 4)
						PhyInx -= 4;
					P1Addr = (myhdmi->PHYAdr.Org |
						(1 << PhyInx));
					P2Addr = (myhdmi->PHYAdr.Org |
						(2 << PhyInx));
					P3Addr = (myhdmi->PHYAdr.Org |
						(3 << PhyInx));
				} else {
					P1Addr = 0xffff;
					P2Addr = 0xffff;
					P3Addr = 0xffff;
				}
				/* for CLI debug mode */
				if (myhdmi->fgUseMdyPA == TRUE) {
					P1Addr = myhdmi->u2MdyPA;
					P2Addr = myhdmi->u2MdyPA;
					P3Addr = myhdmi->u2MdyPA;
				}
				myhdmi->_u2Port1Addr = P1Addr;
				myhdmi->_u2Port2Addr = P2Addr;
				myhdmi->_u2Port3Addr = P3Addr;
				if (EDIDDBG) {
					ELOG("[RX]P1Addr =0x%x\n", P1Addr);
					ELOG("[RX]P2Addr =0x%x\n", P2Addr);
					ELOG("[RX]P3Addr =0x%x\n", P3Addr);
				}
			}
			for (l = 0; l < 5; l++) {
				if (myhdmi->PHY_ADR_ST[l] != 0) {
					DBGLOG("[RX]1.PHY_ADR_ST[%d]=0x%x\n",
					   l, myhdmi->PHY_ADR_ST[l]);
					j = myhdmi->PHY_ADR_ST[l];
					if (j < sizeof(myhdmi->oEBuf))
						myhdmi->oEBuf[j] =
							(P2Addr >> 8) & 0xff;
					j = j + 1;
					if (j < sizeof(myhdmi->oEBuf))
						myhdmi->oEBuf[j] =
							P2Addr & 0xff;
				}
			}
			myhdmi->oEBuf[E_ADR_CHK_SUM] =
			    Edid_Chk_Sum(myhdmi, myhdmi->oEBuf);
			myhdmi->_u1EDID1CHKSUM = myhdmi->oEBuf[E_ADR_CHK_SUM];
			myhdmi->_u2EDID1PA = P2Addr;
			myhdmi->_u1EDID1PAOFF = myhdmi->Edid.PHYPoint;

			vWriteEDIDBlk1(myhdmi, EEPROM1, myhdmi->oEBuf);

			DBGLOG("[RX]P2Addr = 0x%x\r\n", P2Addr);
			break;

		case EEPROM2:
			ComposeEdidBlock1(myhdmi, myhdmi->rEBuf, myhdmi->oEBuf);
			if (myhdmi->Edid.PHYPoint != 0) {
				/* Mean that is not DVI */
				for (PhyInx = 0; PhyInx < 16; PhyInx += 4) {
					if (((myhdmi->PHYAdr.Org >> PhyInx) &
						0x0f) != 0)
						break;
				}
				if (PhyInx != 0) {
					if (PhyInx >= 4)
						PhyInx -= 4;
					P1Addr = (myhdmi->PHYAdr.Org |
						(1 << PhyInx));
					P2Addr = (myhdmi->PHYAdr.Org |
						(2 << PhyInx));
					P3Addr = (myhdmi->PHYAdr.Org |
						(3 << PhyInx));
				} else {
					P1Addr = 0xffff;
					P2Addr = 0xffff;
					P3Addr = 0xffff;
				}
				if (myhdmi->fgUseMdyPA == TRUE) {
					/* for CLI debug mode */
					P1Addr = myhdmi->u2MdyPA;
					P2Addr = myhdmi->u2MdyPA;
					P3Addr = myhdmi->u2MdyPA;
				}
				myhdmi->_u2Port1Addr = P1Addr;
				myhdmi->_u2Port2Addr = P2Addr;
				myhdmi->_u2Port3Addr = P3Addr;
				DBGLOG("[RX]P1Addr=0x%x\n", P1Addr);
				DBGLOG("[RX]P2Addr=0x%x\n", P2Addr);
				DBGLOG("[RX]P3Addr=0x%x\n", P3Addr);
			}

			for (l = 0; l < 5; l++) {
				if (myhdmi->PHY_ADR_ST[l] != 0) {
					DBGLOG("[RX]1.PHY_ADR_ST[%d]=%x\n",
						   l, myhdmi->PHY_ADR_ST[l]);
					j = myhdmi->PHY_ADR_ST[l];
					if (j < sizeof(myhdmi->oEBuf))
						myhdmi->oEBuf[j] =
							(P3Addr >> 8) & 0xff;
					j = j + 1;
					if (j < sizeof(myhdmi->oEBuf))
						myhdmi->oEBuf[j] =
							P3Addr & 0xff;
				}
			}
			myhdmi->oEBuf[E_ADR_CHK_SUM] =
			    Edid_Chk_Sum(myhdmi, myhdmi->oEBuf);
			// to do write to sram
			myhdmi->_u1EDID2CHKSUM = myhdmi->oEBuf[E_ADR_CHK_SUM];
			myhdmi->_u2EDID2PA = P3Addr;
			myhdmi->_u1EDID2PAOFF = myhdmi->Edid.PHYPoint;

			vWriteEDIDBlk1(myhdmi, EEPROM2, myhdmi->oEBuf);

			DBGLOG("[RX]P3Addr = 0x%x\r\n", P3Addr);
			break;
#endif
		default:
			break;
		}

		DBGLOG("[RX]After ComposeEdidBlock1 u1EdidNum%d Page1 EDID\n",
			   myhdmi->Edid.Number);
	}
}

void Default_Edid_BL0_BL1_Write(struct MTK_HDMI *myhdmi)
{
	Default_Edid_BL0_Write(myhdmi);
	Default_Edid_BL1_Write(myhdmi);
}

void vSetEdidUpdateMode(struct MTK_HDMI *myhdmi,
	bool fgVideoAndOp,
	bool fgAudioAndOP)
{
	DBGLOG("[RX] fgVideoAndOp = %d,fgAudioAndOP = %d\n",
		fgVideoAndOp, fgAudioAndOP);
	myhdmi->fgVidUseAnd = fgVideoAndOp;
	myhdmi->AudUseAnd = fgAudioAndOP;
}

void vSetEdidPcm2chOnly(struct MTK_HDMI *myhdmi,
	u8 u12chPcmOnly)
{
	if (u12chPcmOnly)
		myhdmi->_fg2chPcmOnly = TRUE;
	else
		myhdmi->_fg2chPcmOnly = FALSE;
}

bool fgOnlySupportPcm2ch(struct MTK_HDMI *myhdmi)
{
	return myhdmi->_fg2chPcmOnly;
}

void vShowEdidPhyAddress(struct MTK_HDMI *myhdmi)
{
	if (EDIDDBG) {
		ELOG("[RX]EDID1 PA = %d. %d. %d. %d\n",
			(myhdmi->_u2Port1Addr >> 12) & 0x0f,
			(myhdmi->_u2Port1Addr >> 8) & 0x0f,
			(myhdmi->_u2Port1Addr >> 4) & 0x0f,
			myhdmi->_u2Port1Addr & 0x0f);
		ELOG("[RX]EDID2 PA = %d. %d. %d. %d\n",
			(myhdmi->_u2Port2Addr >> 12) & 0x0f,
			(myhdmi->_u2Port2Addr >> 8) & 0x0f,
			(myhdmi->_u2Port2Addr >> 4) & 0x0f,
			myhdmi->_u2Port2Addr & 0x0f);
	}
}

void vEDIDCreateBlock1(struct MTK_HDMI *myhdmi)
{
	u8 Addr, i;
	u8 u1EdidNum, u1PAHigh, u1PALow;
	u8 PhyInx;
	u16 P1Addr, P2Addr, P3Addr;

	DBGLOG("[RX]%s\n", __func__);

	if (myhdmi->AudUseAnd == FALSE) {
		for (i = 0; i < 128; i++) {
			myhdmi->rEBuf[i] = 0;
			myhdmi->oEBuf[i] = 0;
		}

		/* Block 1 */
		DBGLOG("[RX]Default_Edid_BL1_Write\n");

		for (Addr = 0; Addr < 4; Addr++)
			myhdmi->oEBuf[Addr] = Def_Edid_Blk1_Header[Addr];
		// Audio output is HDMI+Speaket or Speaker
		if (myhdmi->AudUseAnd == FALSE)
			myhdmi->oEBuf[1] = 3;
		if (SCP->s_supp_hdmi_mode == FALSE) {
			// DVI
			// clear YcbCr444 and Ycbcr422
			myhdmi->oEBuf[3] = myhdmi->oEBuf[3] & (0x8f);
			// Audio output is HDMI+Speaket or
			if (myhdmi->AudUseAnd == FALSE)
				// Speaker
				// Set basic audio
				myhdmi->oEBuf[3] = myhdmi->oEBuf[3] | (1 << 6);
		}
		if (myhdmi->fgVidUseAnd)
			ComposeVideoBlock(myhdmi, 1, myhdmi->AudUseAnd);
		else
			ComposeVideoBlock(myhdmi, 0, myhdmi->AudUseAnd);
		if ((myhdmi->CP_VidDataBLK[0] & 0x1f) != 0) {
			// SVD exist
			for (i = 0; i < ((myhdmi->CP_VidDataBLK[0] &
				0x1f) + 1); i++, Addr++)
				myhdmi->oEBuf[Addr] = myhdmi->CP_VidDataBLK[i];
		}
		if (fgOnlySupportPcm2ch(myhdmi) == TRUE) {
			for (i = 0; (i < E_AUD2CHPCM_ONLY_LEN) && (Addr < 128);
				i++, Addr++)
				myhdmi->oEBuf[Addr] = AudDataBlk_2CH_ONLY[i];
		} else {
			for (i = 0; (i < E_AUD_BLK_LEN) && (Addr < 128);
				i++, Addr++)
				myhdmi->oEBuf[Addr] = Audio_Data_Block[i];
		}
		if (fgOnlySupportPcm2ch(myhdmi) == TRUE) {
			for (i = 0; (i < E_SPK_BLK_LEN) && (Addr < 128);
				i++, Addr++)
				myhdmi->oEBuf[Addr] = SpkDataBlk_2CH_ONLY[i];
		} else {
			for (i = 0; (i < E_SPK_BLK_LEN) && (Addr < 128);
				i++, Addr++)
				myhdmi->oEBuf[Addr] = Speaker_Data_Block[i];
		}
		PhyInx = Addr + 4;
		for (i = 0; i < E_VEND_BLK_MINI_LEN; i++)
			VendDataBLK_Full[i] = VendDataBLK_Mini[i];
		for (i = 0; i < E_VEND_BLK_MINI_LEN; i++, Addr++) {
			if (Addr < 128)
				modify_pa(&myhdmi->oEBuf[0],
					myhdmi->fgUseMdyPA,
					myhdmi->u2MdyPA,
					Addr,
					i);
		}
		myhdmi->oEBuf[2] = Addr;	// Update data offset
		for (; Addr < 128; Addr++)
			myhdmi->oEBuf[Addr] = 0;
		P1Addr = 0x1100;
		P2Addr = 0x1200;
		P3Addr = 0x1400;

		for (u1EdidNum = EEPROM0; u1EdidNum <= EEPROM2; u1EdidNum++) {
			switch (u1EdidNum) {
			case EEPROM0:
				u1PAHigh = (u8)(P1Addr >> 8);
				u1PALow = (u8)(P1Addr & 0xFF);
				if ((PhyInx + 1) < sizeof(myhdmi->oEBuf)) {
					myhdmi->oEBuf[PhyInx] = u1PAHigh;
					myhdmi->oEBuf[PhyInx + 1] = u1PALow;
				}
				myhdmi->_u2Port1Addr = P1Addr;
				if (txapi_is_plugin(myhdmi) == TRUE) {
					// Del eDeep Color Info
					if ((PhyInx + 2) <
						sizeof(myhdmi->oEBuf))
						myhdmi->oEBuf[PhyInx + 2] &=
							0x80;
				}

				myhdmi->oEBuf[E_ADR_CHK_SUM] =
					Edid_Chk_Sum(myhdmi, &myhdmi->oEBuf[0]);
				myhdmi->_u1EDID0CHKSUM =
					myhdmi->oBLK[E_ADR_CHK_SUM];
				myhdmi->_u2EDID0PA = P1Addr;
				myhdmi->_u1EDID0PAOFF = PhyInx;
				DBGLOG("%s=%d;%s=%2x;%s=%2x;%s=%2x\n",
				   "[RX]Block1 u1EdidNum",
				   u1EdidNum,
				   "_u1EDID0CHKSUM",
				   myhdmi->_u1EDID0CHKSUM,
				   "_u2EDID0PA",
				   myhdmi->_u2EDID0PA,
				   "_u1EDID0PAOFF",
				   myhdmi->_u1EDID0PAOFF);
				vWriteEDIDBlk1(myhdmi, EEPROM0, myhdmi->oEBuf);
				break;

#if (HDMI_INPUT_COUNT != 1)
			case EEPROM1:
				u1PAHigh = (u8)(P2Addr >> 8);
				u1PALow = (u8)(P2Addr & 0xFF);
				if ((PhyInx + 1) < sizeof(myhdmi->oEBuf)) {
					myhdmi->oEBuf[PhyInx] = u1PAHigh;
					myhdmi->oEBuf[PhyInx + 1] = u1PALow;
				}
				myhdmi->_u2Port2Addr = P2Addr;

				if (txapi_is_plugin(myhdmi) == TRUE) {
					if ((PhyInx + 2) <
						sizeof(myhdmi->oEBuf))
						myhdmi->oEBuf[PhyInx + 2] &=
							0x80;
					// Del eDeep Color Info
				}

				myhdmi->oEBuf[E_ADR_CHK_SUM] =
					Edid_Chk_Sum(myhdmi, &myhdmi->oEBuf[0]);
				myhdmi->_u1EDID1CHKSUM =
					myhdmi->oEBuf[E_ADR_CHK_SUM];
				myhdmi->_u2EDID1PA = P2Addr;
				myhdmi->_u1EDID1PAOFF = PhyInx;
				DBGLOG("%s=%d;%s=%2x;%s=%2x;%s=%2x\n",
				   "[RX]Block1 u1EdidNum",
				   u1EdidNum,
				   "_u1EDID1CHKSUM",
				   myhdmi->_u1EDID1CHKSUM,
				   "_u2EDID1PA",
				   myhdmi->_u2EDID1PA,
				   "_u1EDID1PAOFF",
				   myhdmi->_u1EDID1PAOFF);
				vWriteEDIDBlk1(myhdmi, EEPROM1, myhdmi->oEBuf);
				break;

			case EEPROM2:
				u1PAHigh = (u8)(P3Addr >> 8);
				u1PALow = (u8)(P3Addr & 0xFF);
				if ((PhyInx + 1) < sizeof(myhdmi->oEBuf)) {
					myhdmi->oEBuf[PhyInx] = u1PAHigh;
					myhdmi->oEBuf[PhyInx + 1] = u1PALow;
				}
				myhdmi->_u2Port3Addr = P3Addr;

				if (txapi_is_plugin(myhdmi) == TRUE) {
					if ((PhyInx + 2) <
						sizeof(myhdmi->oEBuf))
						myhdmi->oEBuf[PhyInx + 2] &=
							0x80;
					// Del eDeep Color Info
				}

				myhdmi->oEBuf[E_ADR_CHK_SUM] =
					Edid_Chk_Sum(myhdmi, &myhdmi->oEBuf[0]);
				myhdmi->_u1EDID2CHKSUM =
					myhdmi->oEBuf[E_ADR_CHK_SUM];
				myhdmi->_u2EDID2PA = P3Addr;
				myhdmi->_u1EDID2PAOFF = PhyInx;
				DBGLOG("%s=%d;%s=%2x;%s=%2x;%s=%2x\n",
					"[RX]Block1 u1EdidNum",
					u1EdidNum,
					"_u1EDID2CHKSUM",
					myhdmi->_u1EDID2CHKSUM,
					"_u2EDID2PA",
					myhdmi->_u2EDID2PA,
					"_u1EDID2PAOFF",
					myhdmi->_u1EDID2PAOFF);
				vWriteEDIDBlk1(myhdmi, EEPROM2, myhdmi->oEBuf);
				break;
#endif
			default:
				break;
			}
		}
		myhdmi->Edid.bBlock1Err = TRUE;
		myhdmi->Edid.bDownDvi = TRUE;
		myhdmi->PHYAdr.Org = 0x0000;
		myhdmi->PHYAdr.Dvd = 0x1000;
		myhdmi->PHYAdr.Sat = 0x2000;
		myhdmi->PHYAdr.Tv = 0x3000;
		myhdmi->PHYAdr.Else = 0x4000;
		myhdmi->Edid.PHYLevel = 1;
	} else {
		for (i = 0; i < 128; i++)
			myhdmi->oEBuf[i] = 0;
		myhdmi->_u1EDID0CHKSUM = 0;
		myhdmi->_u1EDID1CHKSUM = 0;
		myhdmi->_u2EDID0PA = 0;
		myhdmi->_u2EDID1PA = 0;
		myhdmi->_u1EDID0PAOFF = 0;
		myhdmi->_u1EDID1PAOFF = 0;
		#if (HDMI_INPUT_COUNT == 1)
		vWriteEDIDBlk1(myhdmi, EEPROM0, myhdmi->oEBuf);
		#else
		vWriteEDIDBlk1(myhdmi, EEPROM0, myhdmi->oEBuf);
		vWriteEDIDBlk1(myhdmi, EEPROM1, myhdmi->oEBuf);
		vWriteEDIDBlk1(myhdmi, EEPROM2, myhdmi->oEBuf);
		#endif
	}
}

void vWriteEDIDBlk0(struct MTK_HDMI *myhdmi,
	u8 u1EDIDNo,
	u8 *poBLK)
{
	u8 u1Cnt;

	if (u1EDIDNo == EEPROM0)
		for (u1Cnt = 0; u1Cnt < 128; u1Cnt++)
			myhdmi->u1HDMIIN1EDID[u1Cnt] = *(poBLK + u1Cnt);

	if (u1EDIDNo == EEPROM1)
		for (u1Cnt = 0; u1Cnt < 128; u1Cnt++)
			myhdmi->u1HDMIIN2EDID[u1Cnt] = *(poBLK + u1Cnt);

	vShowEDIDBLKData(EDIDDBG, poBLK);

	hdmi2com_write_edid_to_sram(myhdmi, 0, poBLK);
}

void vWriteEDIDBlk1(struct MTK_HDMI *myhdmi,
	u8 u1EDIDNo,
	u8 *poBLK)
{
	u8 u1Cnt;

	if (u1EDIDNo == EEPROM0)
		for (u1Cnt = 0; u1Cnt < 128; u1Cnt++)
			myhdmi->u1HDMIIN1EDID[128 + u1Cnt] = *(poBLK + u1Cnt);

	if (u1EDIDNo == EEPROM1)
		for (u1Cnt = 0; u1Cnt < 128; u1Cnt++)
			myhdmi->u1HDMIIN2EDID[128 + u1Cnt] = *(poBLK + u1Cnt);

	vShowEDIDBLKData(EDIDDBG, poBLK);

	hdmi2com_write_edid_to_sram(myhdmi, 1, poBLK);
}
