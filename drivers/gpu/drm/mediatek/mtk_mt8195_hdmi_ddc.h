/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DDC_MT8195_H
#define _MTK_DDC_MT8195_H

#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/mutex.h>

struct mtk_hdmi_ddc {
	struct mutex mtx;
	struct i2c_adapter adap;
	struct clk *clk;
	void __iomem *regs;
	struct mtk_hdmi *hdmi;
	unsigned int ddc_count;
};

#define DDC2_CLOK 572		/* BIM=208M/(v*4) = 90Khz */
#define DDC2_CLOK_EDID 832	/* BIM=208M/(v*4) = 62.5Khz */

unsigned char fg_ddc_data_read(struct mtk_hdmi_ddc *ddc,
				   unsigned char b_dev,
				   unsigned char b_data_addr,
				   unsigned char b_data_count,
				   unsigned char *pr_data);
unsigned char fg_ddc_data_write(struct mtk_hdmi_ddc *ddc,
				    unsigned char b_dev,
				    unsigned char b_data_addr,
				    unsigned char b_data_count,
				    unsigned char *pr_data);

void hdmi_ddc_request(struct mtk_hdmi_ddc *ddc, unsigned char req);
struct mtk_hdmi *hdmi_ctx_from_mtk_hdmi_ddc(struct mtk_hdmi_ddc *ddc);

#endif /* _MTK_DDC_MT8195_H */
