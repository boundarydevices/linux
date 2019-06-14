/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hdmi_tx_scdc.c
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

#include <linux/delay.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_ddc.h>
#include "hw/common.h"

void scdc_config(struct hdmitx_dev *hdev)
{
	/* TMDS 1/40 & Scramble */
	scdc_wr_sink(TMDS_CFG, hdev->para->tmds_clk_div40 ? 0x3 : 0);
}

/* update CED, 10.4.1.8 */
static int scdc_ced_cnt(struct hdmitx_dev *hdev)
{
	struct ced_cnt *ced = &hdev->ced_cnt;
	u8 raw[7];
	u8 chksum;
	int i;

	memset(raw, 0, sizeof(raw));
	memset(ced, 0, sizeof(struct ced_cnt));

	chksum = 0;
	for (i = 0; i < 7; i++) {
		scdc_rd_sink(ERR_DET_0_L + i, &raw[i]);
		chksum += raw[i];
	}

	ced->ch0_cnt = raw[0] + ((raw[1] & 0x7f) << 8);
	ced->ch0_valid = (raw[1] >> 7) & 0x1;
	ced->ch1_cnt = raw[2] + ((raw[3] & 0x7f) << 8);
	ced->ch1_valid = (raw[3] >> 7) & 0x1;
	ced->ch2_cnt = raw[4] + ((raw[5] & 0x7f) << 8);
	ced->ch2_valid = (raw[5] >> 7) & 0x1;

	/* Do checksum */
	if (chksum != 0)
		pr_info("ced check sum error\n");
	if (ced->ch0_cnt)
		pr_info("ced: ch0_cnt = %d %s\n", ced->ch0_cnt,
			ced->ch0_valid ? "" : "unvalid");
	if (ced->ch1_cnt)
		pr_info("ced: ch1_cnt = %d %s\n", ced->ch1_cnt,
			ced->ch1_valid ? "" : "unvalid");
	if (ced->ch2_cnt)
		pr_info("ced: ch2_cnt = %d %s\n", ced->ch2_cnt,
			ced->ch2_valid ? "" : "unvalid");

	return chksum != 0;
}

/* update scdc status flags, 10.4.1.7 */
/* ignore STATUS_FLAGS_1, all bits are RSVD */
int scdc_status_flags(struct hdmitx_dev *hdev)
{
	u8 st = 0;
	u8 locked_st = 0;

	scdc_rd_sink(UPDATE_0, &st);
	if (st & STATUS_UPDATE) {
		scdc_rd_sink(STATUS_FLAGS_0, &locked_st);
		hdev->chlocked_st.clock_detected = locked_st & (1 << 0);
		hdev->chlocked_st.ch0_locked = !!(locked_st & (1 << 1));
		hdev->chlocked_st.ch1_locked = !!(locked_st & (2 << 1));
		hdev->chlocked_st.ch2_locked = !!(locked_st & (3 << 1));
	}
	if (st & CED_UPDATE)
		scdc_ced_cnt(hdev);
	if (st & (STATUS_UPDATE | CED_UPDATE))
		scdc_wr_sink(UPDATE_0, st & (STATUS_UPDATE | CED_UPDATE));
	if (!hdev->chlocked_st.clock_detected)
		pr_info("ced: clock undetected\n");
	if (!hdev->chlocked_st.ch0_locked)
		pr_info("ced: ch0 unlocked\n");
	if (!hdev->chlocked_st.ch1_locked)
		pr_info("ced: ch1 unlocked\n");
	if (!hdev->chlocked_st.ch2_locked)
		pr_info("ced: ch2 unlocked\n");

	return st & (STATUS_UPDATE | CED_UPDATE);
}
