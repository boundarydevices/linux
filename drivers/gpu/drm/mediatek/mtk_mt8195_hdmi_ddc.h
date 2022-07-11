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
};

#endif /* _MTK_DDC_MT8195_H */
