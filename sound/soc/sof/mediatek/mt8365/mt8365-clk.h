/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 *
 * Author: Aary Patil <aary.patil@mediatek.com>
 *
 * Header file for the mt8365 DSP clock definition
 */

#ifndef __MT8365_CLK_H
#define __MT8365_CLK_H

struct snd_sof_dev;

/*DSP clock*/
enum adsp_clk_id {
	CLK_TOP_DSP_SEL,
	CLK_TOP_CLK26M_D52,
	CLK_TOP_SYS_26M_D2,
	CLK_TOP_DSPPLL,
	CLK_TOP_DSPPLL_D2,
	CLK_TOP_DSPPLL_D4,
	CLK_TOP_DSPPLL_D8,
	CLK_TOP_DSP_26M,
	CLK_TOP_DSP_32K,
	ADSP_CLK_MAX
};

int mt8365_adsp_init_clock(struct snd_sof_dev *sdev);
int adsp_clock_on(struct snd_sof_dev *sdev);
int adsp_clock_off(struct snd_sof_dev *sdev);
#endif
