/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _HDMI_RX21_PHY_H_
#define _HDMI_RX21_PHY_H_

u32 Read16Msk(void __iomem *addr, u32 msk);

void vHDMI21PhyTermCtrl(struct MTK_HDMI *myhdmi, bool bOnOff);

void vHDMI21PHY14(struct MTK_HDMI *myhdmi);
void vHDMI21PHY20(struct MTK_HDMI *myhdmi);
void vHDMI21PhySetting(struct MTK_HDMI *myhdmi);
void vHDMI21PhyInit(struct MTK_HDMI *myhdmi);
void vHDMI21Phy2xCLK(struct MTK_HDMI *myhdmi, bool en);
void riu_read(struct MTK_HDMI *myhdmi);
void riu_read2(struct MTK_HDMI *myhdmi);
bool _KHal_HDMIRx_GetClockStableFlag(struct MTK_HDMI *myhdmi);
void vHDMI21PhyIRQEnable(struct MTK_HDMI *myhdmi, u32 mask, bool en);
u32 vHDMI21PhyIRQClear(struct MTK_HDMI *myhdmi);
void vHDMI21PhyPowerDown(struct MTK_HDMI *myhdmi);
void vHDMI21PhyFIFOMACRST(struct MTK_HDMI *myhdmi);
void _KHal_HDMIRx_NewPHYAutoEQTrigger(struct MTK_HDMI *myhdmi, u32 LanSel);

#endif
