/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _HDMI_RX2_EDID_H_
#define _HDMI_RX2_EDID_H_

void EdidProcessing(struct MTK_HDMI *myhdmi);
void Default_Edid_BL0_BL1_Write(struct MTK_HDMI *myhdmi);
void vSetEdidUpdateMode(struct MTK_HDMI *myhdmi,
	bool fgVideoAndOp,
	bool fgAudioAndOP);
void vCEAAudioDataAndOperation(struct MTK_HDMI *myhdmi,
	u8 *prData,
	u8 bCEALen, u8 *poBlock,
	u8 *poCount);
void vSetEdidPcm2chOnly(struct MTK_HDMI *myhdmi,
	u8 u12chPcmOnly);
void vShowEdidPhyAddress(struct MTK_HDMI *myhdmi);
void vEDIDCreateBlock1(struct MTK_HDMI *myhdmi);
void vWriteEDIDBlk0(struct MTK_HDMI *myhdmi,
	u8 u1EDIDNo,
	u8 *poBlock);
void vWriteEDIDBlk1(struct MTK_HDMI *myhdmi,
	u8 u1EDIDNo,
	u8 *poBlock);
void vShowEDIDCEABlock(struct MTK_HDMI *myhdmi,
	u8 *pbuf);
void ComposeEdidBlock0(struct MTK_HDMI *myhdmi,
	u8 *pr_rBlk,
	u8 *pr_oBlk);
void Default_Edid_BL0_BL1_Write(struct MTK_HDMI *myhdmi);
bool fgOnlySupportPcm2ch(struct MTK_HDMI *myhdmi);

#endif
