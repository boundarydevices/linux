/*
 * drivers/amlogic/mmc/aml_sd_emmc_v3.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/highmem.h>
#include <linux/amlogic/sd.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/mmc/emmc_partitions.h>
#include <linux/amlogic/amlsd.h>
#include <linux/amlogic/aml_sd_emmc_internal.h>

int meson_mmc_clk_init_v3(struct amlsd_host *host)
{
	int ret = 0;
	u32 vclkc = 0;
	struct sd_emmc_clock_v3 *pclkc = (struct sd_emmc_clock_v3 *)&vclkc;
	u32 vconf = 0;
	struct sd_emmc_config *pconf = (struct sd_emmc_config *)&vconf;
	struct amlsd_platform *pdata = host->pdata;

	ret = aml_emmc_clktree_init(host);
	if (ret)
		return ret;

	/* init SD_EMMC_CLOCK to sane defaults w/min clock rate */
	vclkc = 0;
	pclkc->div = 60;	 /* 400KHz */
	pclkc->src = 0;	  /* 0: Crystal 24MHz */
	pclkc->core_phase = 2;	  /* 2: 180 phase */
	pclkc->rx_phase = 0;
	pclkc->tx_phase = 0;
	pclkc->always_on = 1;	  /* Keep clock always on */
	writel(vclkc, host->base + SD_EMMC_CLOCK);
	pdata->clkc = vclkc;

	vconf = 0;
	/* 1bit mode */
	pconf->bus_width = 0;
	/* 512byte block length */
	pconf->bl_len = 9;
	/* 64 CLK cycle, here 2^8 = 256 clk cycles */
	pconf->resp_timeout = 8;
	/* 1024 CLK cycle, Max. 100mS. */
	pconf->rc_cc = 4;
	pconf->err_abort = 0;
	pconf->auto_clk = 1;
	writel(vconf, host->base + SD_EMMC_CFG);

	writel(0xffff, host->base + SD_EMMC_STATUS);
	writel(SD_EMMC_IRQ_ALL, host->base + SD_EMMC_IRQ_EN);

	return ret;
}

static int meson_mmc_clk_set_rate_v3(struct amlsd_host *host,
		unsigned long clk_ios)
{
	struct mmc_host *mmc = host->mmc;
	int ret = 0;
#ifdef SD_EMMC_CLK_CTRL
	u32 clk_rate, clk_div, clk_src_sel;
	struct amlsd_platform *pdata = host->pdata;
#else
	u32 vcfg = 0;
	struct sd_emmc_config *conf = (struct sd_emmc_config *)&vcfg;
#endif

#ifdef SD_EMMC_CLK_CTRL
	if (clk_ios == 0) {
		aml_mmc_clk_switch_off(host);
		return ret;
	}

	clk_src_sel = SD_EMMC_CLOCK_SRC_OSC;
	if (clk_ios < 20000000)
		clk_src_sel = SD_EMMC_CLOCK_SRC_OSC;
	else
		clk_src_sel = SD_EMMC_CLOCK_SRC_FCLK_DIV2;
#endif

	if (clk_ios) {
		if (WARN_ON(clk_ios > mmc->f_max))
			clk_ios = mmc->f_max;
		else if (WARN_ON(clk_ios < mmc->f_min))
			clk_ios = mmc->f_min;
	}

#ifdef SD_EMMC_CLK_CTRL
	WARN_ON(clk_src_sel > SD_EMMC_CLOCK_SRC_FCLK_DIV2);
	switch (clk_src_sel) {
	case SD_EMMC_CLOCK_SRC_OSC:
		clk_rate = 24000000;
		break;
	case SD_EMMC_CLOCK_SRC_FCLK_DIV2:
		clk_rate = 1000000000;
		break;
	default:
		pr_err("%s: clock source error: %d\n",
			mmc_hostname(host->mmc), clk_src_sel);
		ret = -1;
	}
	clk_div = (clk_rate / clk_ios) + (!!(clk_rate % clk_ios));

	aml_mmc_clk_switch(host, clk_div, clk_src_sel);
	pdata->clkc = readl(host->base + SD_EMMC_CLOCK);

	mmc->actual_clock = clk_rate / clk_div;
#else
	if (clk_ios == mmc->actual_clock)
		return 0;

	/* stop clock */
	vcfg = readl(host->base + SD_EMMC_CFG);
	if (!conf->stop_clk) {
		conf->stop_clk = 1;
		writel(vcfg, host->base + SD_EMMC_CFG);
	}

	dev_dbg(host->dev, "change clock rate %u -> %lu\n",
		mmc->actual_clock, clk_ios);
	ret = clk_set_rate(host->cfg_div_clk, clk_ios);
	if (clk_ios && clk_ios != clk_get_rate(host->cfg_div_clk))
		dev_warn(host->dev, "divider requested rate %lu != actual rate %lu: ret=%d\n",
			 clk_ios, clk_get_rate(host->cfg_div_clk), ret);
	else
		mmc->actual_clock = clk_ios;

	/* (re)start clock, if non-zero */
	if (clk_ios) {
		vcfg = readl(host->base + SD_EMMC_CFG);
		conf->stop_clk = 0;
		writel(vcfg, host->base + SD_EMMC_CFG);
	}
#endif

	return ret;
}

static void aml_sd_emmc_set_timing_v3(struct amlsd_host *host,
				u32 timing)
{
	struct amlsd_platform *pdata = host->pdata;
	u32 vctrl;
	struct sd_emmc_config *ctrl = (struct sd_emmc_config *)&vctrl;
	u32 vclkc;
	struct sd_emmc_clock_v3 *clkc = (struct sd_emmc_clock_v3 *)&vclkc;
	u32 adjust;
	struct sd_emmc_adjust_v3 *gadjust = (struct sd_emmc_adjust_v3 *)&adjust;
	u8 clk_div = 0;
	u32 clk_rate = 1000000000;

	vctrl = readl(host->base + SD_EMMC_CFG);
	if ((timing == MMC_TIMING_MMC_HS400) ||
			 (timing == MMC_TIMING_MMC_DDR52) ||
				 (timing == MMC_TIMING_UHS_DDR50)) {
		if (timing == MMC_TIMING_MMC_HS400) {
			/*ctrl->chk_ds = 1;*/
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXLX) {
				adjust = readl(host->base + SD_EMMC_ADJUST_V3);
				gadjust->ds_enable = 1;
				writel(adjust, host->base + SD_EMMC_ADJUST_V3);
				/*host->tuning_mode = AUTO_TUNING_MODE;*/
			}
		}
		vclkc = readl(host->base + SD_EMMC_CLOCK_V3);
		ctrl->ddr = 1;
		clk_div = clkc->div;
		if (clk_div & 0x01)
			clk_div++;
		clkc->div = clk_div / 2;
		writel(vclkc, host->base + SD_EMMC_CLOCK_V3);
		pdata->clkc = vclkc;
		host->mmc->actual_clock = clk_rate / clk_div;
		pr_info("%s: try set sd/emmc to DDR mode\n",
			mmc_hostname(host->mmc));
	} else
		ctrl->ddr = 0;

	writel(vctrl, host->base + SD_EMMC_CFG);
	sd_emmc_dbg(AMLSD_DBG_IOS, "sd emmc is %s\n",
			ctrl->ddr?"DDR mode":"SDR mode");
}

/*call by mmc, power on, power off ...*/
static void aml_sd_emmc_set_power_v3(struct amlsd_host *host,
					u32 power_mode)
{
	struct amlsd_platform *pdata = host->pdata;

	switch (power_mode) {
	case MMC_POWER_ON:
		if (pdata->pwr_pre)
			pdata->pwr_pre(pdata);
		if (pdata->pwr_on)
			pdata->pwr_on(pdata);
		break;
	case MMC_POWER_UP:
		break;
	case MMC_POWER_OFF:
		writel(0, host->base + SD_EMMC_DELAY1_V3);
		writel(0, host->base + SD_EMMC_DELAY2_V3);
		writel(0, host->base + SD_EMMC_ADJUST_V3);
		break;
	default:
		if (pdata->pwr_pre)
			pdata->pwr_pre(pdata);
		if (pdata->pwr_off)
			pdata->pwr_off(pdata);
		break;
	}
}

void meson_mmc_set_ios_v3(struct mmc_host *mmc,
				struct mmc_ios *ios)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;

	if (!pdata->is_in)
		return;
	/*Set Power*/
	aml_sd_emmc_set_power_v3(host, ios->power_mode);

	/*Set Clock*/
	meson_mmc_clk_set_rate_v3(host, ios->clock);

	/*Set Bus Width*/
	aml_sd_emmc_set_buswidth(host, ios->bus_width);

	/* Set Date Mode */
	aml_sd_emmc_set_timing_v3(host, ios->timing);

	if (ios->chip_select == MMC_CS_HIGH)
		aml_cs_high(mmc);
	else if (ios->chip_select == MMC_CS_DONTCARE)
		aml_cs_dont_care(mmc);
}

int aml_mmc_execute_tuning_v3(struct mmc_host *mmc, u32 opcode)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct aml_tuning_data tuning_data;
	int err;
	u32 adj_win_start = 100;
	u32 intf3;

	if (opcode == MMC_SEND_TUNING_BLOCK_HS200) {
		if (mmc->ios.bus_width == MMC_BUS_WIDTH_8) {
			tuning_data.blk_pattern = tuning_blk_pattern_8bit;
			tuning_data.blksz = sizeof(tuning_blk_pattern_8bit);
		} else if (mmc->ios.bus_width == MMC_BUS_WIDTH_4) {
			tuning_data.blk_pattern = tuning_blk_pattern_4bit;
			tuning_data.blksz = sizeof(tuning_blk_pattern_4bit);
		} else {
			return -EINVAL;
		}
	} else if (opcode == MMC_SEND_TUNING_BLOCK) {
		tuning_data.blk_pattern = tuning_blk_pattern_4bit;
		tuning_data.blksz = sizeof(tuning_blk_pattern_4bit);
	} else {
		sd_emmc_err("Undefined command(%d) for tuning\n", opcode);
		return -EINVAL;
	}

	intf3 = readl(host->base + SD_EMMC_INTF3);
	intf3 |= (1 << 22);
	writel(intf3, host->base + SD_EMMC_INTF3);
	err = aml_sd_emmc_execute_tuning_(mmc, opcode,
			&tuning_data, adj_win_start);

	pr_info("%s: gclock=0x%x, gdelay1=0x%x, gdelay2=0x%x,intf3=0x%x\n",
		mmc_hostname(mmc), readl(host->base + SD_EMMC_CLOCK_V3),
		readl(host->base + SD_EMMC_DELAY1_V3),
		readl(host->base + SD_EMMC_DELAY2_V3),
		readl(host->base + SD_EMMC_INTF3));
	return err;
}

/*static int aml_sd_emmc_cali_v3(struct mmc_host *mmc,
 *	u8 opcode, u8 *blk_test, u32 blksz, u32 blocks)
 *{
 *	struct amlsd_host *host = mmc_priv(mmc);
 *	struct mmc_request mrq = {NULL};
 *	struct mmc_command cmd = {0};
 *	struct mmc_command stop = {0};
 *	struct mmc_data data = {0};
 *	struct scatterlist sg;
 *
 *	cmd.opcode = opcode;
 *	cmd.arg = ((SZ_1M * (36 + 3)) / 512);
 *	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
 *
 *	stop.opcode = MMC_STOP_TRANSMISSION;
 *	stop.arg = 0;
 *	stop.flags = MMC_RSP_R1B | MMC_CMD_AC;
 *
 *	data.blksz = blksz;
 *	data.blocks = blocks;
 *	data.flags = MMC_DATA_READ;
 *	data.sg = &sg;
 *	data.sg_len = 1;
 *
 *	memset(blk_test, 0, blksz * data.blocks);
 *	sg_init_one(&sg, blk_test, blksz * data.blocks);
 *
 *	mrq.cmd = &cmd;
 *	mrq.stop = &stop;
 *	mrq.data = &data;
 *	host->mrq = &mrq;
 *	mmc_wait_for_req(mmc, &mrq);
 *	return data.error | cmd.error;
 *}
 */

