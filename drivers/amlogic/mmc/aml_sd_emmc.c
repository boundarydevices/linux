/*
 * drivers/amlogic/mmc/aml_sd_emmc.c
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
#include <linux/amlogic/aml_sd_emmc_v3.h>
struct mmc_host *sdio_host;

static unsigned int log2i(unsigned int val)
{
	unsigned int ret = -1;

	while (val != 0) {
		val >>= 1;
		ret++;
	}
	return ret;
}

#ifdef AML_CALIBRATION
u32 checksum_cali(u32 *buffer, u32 length)
{
	u32 sum = 0, *i = buffer;

	buffer += length;
	for (; i < buffer; sum += *(i++))
		;
	return sum;
}

static int is_larger(u8 value, u8 base, u8 wrap)
{
	int ret = 0;

	if ((value > base) || ((value < base) && (base == wrap)))
		ret = 1;
	return ret;
}

static void find_base(struct amlsd_platform *pdata, u8 *is_base_index,
		u8 fir_base[2][2], u8 *first_base_temp_num, u32 base_index_val,
			u8 *calout_cmp_num)
{
	u8 first_base_temp, line_x, dly_tmp, cal_time, max_index;

	line_x = pdata->c_ctrl.line_x;
	dly_tmp = pdata->c_ctrl.dly_tmp;
	cal_time = pdata->c_ctrl.cal_time;
	max_index = pdata->c_ctrl.max_index;

	if (pdata->calout[dly_tmp][cal_time] != 0xFF) {
		/* calculate base index! */
		if (*is_base_index == 1) {
			first_base_temp = pdata->calout[dly_tmp][cal_time];
			*first_base_temp_num = *first_base_temp_num + 1;
			if (*first_base_temp_num == 1) {
				fir_base[0][0] = first_base_temp;
				fir_base[0][1] = fir_base[0][1] + 1;
			} else {
					if (first_base_temp == fir_base[0][0])
						fir_base[0][1]++;
					else {
						fir_base[1][0] =
							first_base_temp;
						fir_base[1][1]++;
					}
			}
			/* get a higher index, add the counter! */
		} else if (is_larger(pdata->calout[dly_tmp][cal_time],
					base_index_val, max_index))
			*calout_cmp_num = *calout_cmp_num + 1;
	} else {
		/* todo, if we do not capture a valid value,
		 * HIGHLIGHT(cal_time = 0) may cause error!!!
		 */
			pr_err("!!!Do not capture a valid index");
			pr_err("@ line %d on capture %d\n",
						line_x, cal_time);
	}
}

static int aml_sd_emmc_cali_transfer(struct mmc_host *mmc,
			u8 opcode, u8 *blk_test, u32 blksz)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_command stop = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;

	cmd.opcode = opcode;
	cmd.arg = CALI_PATTERN_OFFSET;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	stop.opcode = MMC_STOP_TRANSMISSION;
	stop.arg = 0;
	stop.flags = MMC_RSP_R1B | MMC_CMD_AC;

	data.blksz = blksz;
	if (opcode == 18)
		data.blocks = CALI_BLK_CNT;
	else
		data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	memset(blk_test, 0, blksz * data.blocks);
	sg_init_one(&sg, blk_test, blksz * data.blocks);

	mrq.cmd = &cmd;
	mrq.stop = &stop;
	mrq.data = &data;
	host->mrq = &mrq;
	mmc_wait_for_req(mmc, &mrq);
	return data.error | cmd.error;
}

static int aml_cali_auto(struct mmc_host *mmc, struct cali_data *c_data)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;
	u8 i, max_cali_i = 0;
	u32 max_cali_count = 0;
	u32 cali_tmp[4] = {0};
	u32 line_delay = 0;
	u32 base_index_val = 0;
	u32 adjust;
	u8 is_base_index, max_index, line_x, dly_tmp;
	u8 bus_width = 8;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	u32 blksz = 512;
	int ret = 0;

	if (mmc->ios.bus_width == 0)
		bus_width = 1;
	else if (mmc->ios.bus_width == 2)
		bus_width = 4;
	else
		bus_width = 8;

	max_index = pdata->c_ctrl.max_index;
	/* for each line */
	for (line_x = 0; line_x < bus_width; line_x++) {
		base_index_val = 0;
		is_base_index = 1;
		memset(pdata->calout, 0xFF, 20 * 20);
		pdata->c_ctrl.line_x = line_x;
		/* for each delay index! */
		for (dly_tmp = 0; dly_tmp < MAX_DELAY_CNT; dly_tmp++) {
			max_cali_count = 0;
			max_cali_i = 0;
			line_delay = 0;
			line_delay = dly_tmp << (4 * line_x);
			writel(line_delay, host->base + SD_EMMC_DELAY);
			pdata->caling = 1;
			aml_sd_emmc_cali_transfer(mmc,
					MMC_READ_MULTIPLE_BLOCK,
					host->blk_test, blksz);
			for (i = 0; i < 4; i++) {
				cali_tmp[i]	= readl(host->base
						+ SD_EMMC_CALOUT + i*4);
				if (max_cali_count < (cali_tmp[i] & 0xffffff)) {
					max_cali_count
						= (cali_tmp[i] & 0xffffff);
					max_cali_i = i;
				}
			}
			pdata->calout[dly_tmp][line_x]
				= (cali_tmp[max_cali_i] >> 24) & 0x3f;
#ifdef CHOICE_DEBUG
			for (i = 0; i < 4; i++)
				pr_info("cali_index[%d] =0x%x, cali_count[%d] = %d\n",
						i, cali_tmp[i] >> 24, i,
						cali_tmp[i] & 0xffffff);
#endif
			pdata->caling = 0;
			adjust = readl(host->base + SD_EMMC_ADJUST);
			gadjust->cali_enable = 0;
			gadjust->cali_sel = 0;
			writel(adjust, host->base + SD_EMMC_ADJUST);
			if (is_base_index == 1) {
				is_base_index = 0;
				c_data->base_index[line_x] =
					pdata->calout[dly_tmp][line_x];
				if (c_data->base_index[line_x]
						< c_data->base_index_min)
					c_data->base_index_min
						= c_data->base_index[line_x];
				if (c_data->base_index[line_x]
						> c_data->base_index_max)
					c_data->base_index_max
						= c_data->base_index[line_x];
			}
			if (is_larger(pdata->calout[dly_tmp][line_x],
						c_data->base_index[line_x],
						max_index))
				break;
		}  /* endof dly_tmp loop... */
		/* get a valid index on current line! */
		if (dly_tmp == MAX_DELAY_CNT)
			ret = -1;
		else
			c_data->ln_delay[line_x] = dly_tmp;
		if (ret)
			break;
#ifdef CHOICE_DEBUG
		for (i = 0; i < 16; i++)
			pr_info("%02x, ", pdata->calout[i][line_x]);
		pr_info("\n");
#endif
	}
	return ret;
}

static int aml_cali_index(struct mmc_host *mmc, struct cali_data *c_data)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;
	u32 line_delay = 0;
	u8 calout_cmp_num = 0;
	u32 base_index_val = 0;
	u32 adjust;
	u8 bus_width = 8;
	u8 line_x, dly_tmp, cal_time;
	u8 is_base_index;
	u8 fir_base[2][2] = { {0} };
	u8 first_base_temp_num = 0;
	u8 cal_per_line_num = 8;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	u32 blksz = 512;
	int ret = 0;

	if (mmc->ios.bus_width == 0)
		bus_width = 1;
	else if (mmc->ios.bus_width == 2)
		bus_width = 4;
	else
		bus_width = 8;

	for (line_x = 0; line_x < bus_width; line_x++) {
		base_index_val = 0;
		is_base_index = 1;
		memset(pdata->calout, 0xFF, 20 * 20);
		first_base_temp_num = 0;
		fir_base[0][0] = 0;
		fir_base[0][1] = 0;
		fir_base[1][0] = 0;
		fir_base[1][1] = 0;
		pdata->c_ctrl.line_x = line_x;
		/* for each delay index! */
		for (dly_tmp = 0; dly_tmp < MAX_DELAY_CNT; dly_tmp++) {
			line_delay = 0;
			line_delay = dly_tmp << (4 * line_x);
			writel(line_delay, host->base + SD_EMMC_DELAY);
			calout_cmp_num = 0;
			pdata->c_ctrl.dly_tmp = dly_tmp;
			/* cal_time */
			for (cal_time = 0; cal_time < cal_per_line_num;
					cal_time++) {
				/* send read cmd. */
				pdata->caling = 1;
				aml_sd_emmc_cali_transfer(mmc,
					MMC_READ_MULTIPLE_BLOCK,
					host->blk_test, blksz);
				pdata->calout[dly_tmp][cal_time]
					= readl(host->base + SD_EMMC_CALOUT)
					& 0x3f;
				pdata->caling = 0;
				adjust = readl(host->base + SD_EMMC_ADJUST);
				gadjust->cali_enable = 0;
				gadjust->cali_sel = 0;
				writel(adjust, host->base + SD_EMMC_ADJUST);
				/*get a valid index*/
				pdata->c_ctrl.cal_time = cal_time;
				find_base(pdata, &is_base_index,
				fir_base, &first_base_temp_num,
				base_index_val, &calout_cmp_num);
			}	/* endof cal_time loop... */
			/* get base index value */
			/* if ((base_index_val > 0) && (is_base_index == 1)) {*/
			if (is_base_index == 1) {
				is_base_index = 0;
				if (fir_base[1][1] > fir_base[0][1])
					base_index_val = fir_base[1][0];
				else
					base_index_val = fir_base[0][0];
				/*base_index_val = valid_base_index; */
				if (base_index_val < c_data->base_index_min)
					c_data->base_index_min = base_index_val;
				if (base_index_val > c_data->base_index_max)
					c_data->base_index_max = base_index_val;
				c_data->base_index[line_x] = base_index_val;
				/* pr_err("get base index %d
				 * value @ line (%d)\n",
				 * base_index_val, line_x);
				 */
			} else if (calout_cmp_num == cal_per_line_num) {
				break;
			}
		}  /* endof dly_tmp loop... */
		/* get a valid index on current line! */
		if ((dly_tmp == MAX_DELAY_CNT)
				&& (calout_cmp_num != cal_per_line_num))
			ret = -1;
		else
			c_data->ln_delay[line_x] = dly_tmp;
		if (ret)
			break;
#ifdef CHOICE_DEBUG
		for (i = 0; i < 16; i++) {
			pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
					pdata->calout[i][0],
					pdata->calout[i][1],
					pdata->calout[i][2],
					pdata->calout[i][3],
					pdata->calout[i][4],
					pdata->calout[i][5],
					pdata->calout[i][6],
					pdata->calout[i][7]);
		}
#endif
	}
	return ret;
}

static int aml_cali_find(struct mmc_host *mmc, struct cali_data *c_data)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;
	u32 line_delay;
	struct sd_emmc_delay *line_dly = (struct sd_emmc_delay *)&line_delay;
	u32 max_cal_result = 0;
	u32 min_cal_result = 10000;
	u32 cal_result[8];
	u8 delay_step, max_index, bus_width = 8, line_x = 8;

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB)
		delay_step = 125;
	else
		delay_step = 200;
	if (mmc->ios.bus_width == 0)
		bus_width = 1;
	else if (mmc->ios.bus_width == 2)
		bus_width = 4;
	else
		bus_width = 8;

	max_index = pdata->c_ctrl.max_index;
	/* if base index wrap, fix */
	for (line_x = 0; line_x < bus_width; line_x++) {
		/* 1000 means index is 1ns */
		/* make sure no neg-value  for ln_delay*/
		if (c_data->ln_delay[line_x]*delay_step > 1000)
			c_data->ln_delay[line_x] = 1000 / delay_step;

		if (c_data->base_index[line_x] == max_index) {
			cal_result[line_x] = ((max_index+1))*1000 -
				c_data->ln_delay[line_x]*delay_step;
		} else if ((c_data->base_index_max == max_index) &&
			(c_data->base_index[line_x] != (max_index - 1)) &&
			(c_data->base_index[line_x] != (max_index - 2))) {
			cal_result[line_x] = ((c_data->base_index[line_x]+1)%
			(max_index+1) + (max_index+1))*1000 -
				c_data->ln_delay[line_x]*delay_step;
		} else {
			cal_result[line_x] = (c_data->base_index[line_x]+1)%
				(max_index+1) * 1000 -
					c_data->ln_delay[line_x]*delay_step;
		}
		max_cal_result = (max_cal_result < cal_result[line_x])
				? cal_result[line_x] : max_cal_result;
		min_cal_result = (min_cal_result > cal_result[line_x])
				? cal_result[line_x] : min_cal_result;
		pr_info("%s: delay[%d]=%5d padding=%2d, bidx=%d\n",
				mmc_hostname(mmc), line_x, cal_result[line_x],
				c_data->ln_delay[line_x],
				c_data->base_index[line_x]);
	}
	pr_info("%s: calibration result : max(%d), min(%d)\n",
		mmc_hostname(mmc), max_cal_result, min_cal_result);
	/* retry cali here! */
	if ((max_cal_result - min_cal_result) >= 2000)
		return -1;

	/* swap base_index_max */
	if ((c_data->base_index_max == max_index)
			&& (c_data->base_index_min == 0))
		c_data->base_index_max = 0;
	if (max_cal_result < (c_data->base_index_max * 1000))
		max_cal_result = (c_data->base_index_max * 1000);
	/* calculate each line delay we should use! */
	line_delay = readl(host->base + SD_EMMC_DELAY);
	line_dly->dat0 = (((max_cal_result - cal_result[0]) / delay_step)
			> 15) ? 15 :
			((max_cal_result - cal_result[0]) / delay_step);
	line_dly->dat1 = (((max_cal_result - cal_result[1]) / delay_step)
			> 15) ? 15 :
			((max_cal_result - cal_result[1]) / delay_step);
	line_dly->dat2 = (((max_cal_result - cal_result[2]) / delay_step)
			> 15) ? 15 :
			((max_cal_result - cal_result[2]) / delay_step);
	line_dly->dat3 = (((max_cal_result - cal_result[3]) / delay_step)
			> 15) ? 15 :
			((max_cal_result - cal_result[3]) / delay_step);
	line_dly->dat4 = (((max_cal_result - cal_result[4]) / delay_step)
			> 15) ? 15 :
			((max_cal_result - cal_result[4]) / delay_step);
	line_dly->dat5 = (((max_cal_result - cal_result[5]) / delay_step)
			> 15) ? 15 :
			((max_cal_result - cal_result[5]) / delay_step);
	line_dly->dat6 = (((max_cal_result - cal_result[6]) / delay_step)
			> 15) ? 15 :
			((max_cal_result - cal_result[6]) / delay_step);
	line_dly->dat7 = (((max_cal_result - cal_result[7]) / delay_step)
			> 15) ? 15 :
			((max_cal_result - cal_result[7]) / delay_step);

	pr_info("%s: line_delay =0x%x, max_cal_result =%d\n",
		mmc_hostname(mmc), line_delay, max_cal_result);
	/* set delay count into reg*/
	writel(line_delay, host->base + SD_EMMC_DELAY);
	return 0;
}

static int aml_sd_emmc_execute_calibration(struct mmc_host *mmc,
			u32 *adj_win_start, u32 type)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;
	u32 adjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	u32 vclk, cali_retry = 0;
#ifdef SD_EMMC_CLK_CTRL
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&(vclk);
	u8 clk_div_tmp;
#else
	unsigned long clk_tmp;
#endif
	struct cali_data c_data;
	int ret = 0;
#ifdef CHOICE_DEBUG
	u8 i;
#endif

#ifdef SD_EMMC_CLK_CTRL
	vclk = readl(host->base + SD_EMMC_CLOCK);
	clk_div_tmp = clkc->div;
	if (type)
		clkc->div = 5;
	writel(vclk, host->base + SD_EMMC_CLOCK);
#else
	clk_tmp = clk_get_rate(host->cfg_div_clk);
	if (type)
		clk_set_rate(host->cfg_div_clk, 200000000);
	vclk = readl(host->base + SD_EMMC_CLOCK);
#endif
	pdata->clkc = vclk;
	pdata->c_ctrl.max_index = (vclk & 0x3f) - 1;

_cali_retry:
	memset(&c_data, 0, sizeof(struct cali_data));
	c_data.base_index_min = pdata->c_ctrl.max_index + 1;
	c_data.base_index_max = 0;
	pr_info("%s: trying cali %d-th time(s)\n",
				mmc_hostname(mmc), cali_retry);
	host->is_tunning = 1;
	/* for each line */
	if (type)
		ret = aml_cali_auto(mmc, &c_data);
	else
		ret = aml_cali_index(mmc, &c_data);
	if (ret) {
		/* Do not get a valid line delay index value! */
		if (cali_retry < MAX_CALI_RETRY) {
			pr_err("Do't get valid ln_delay @ line %d, try\n",
					pdata->c_ctrl.line_x);
			cali_retry++;
			goto _cali_retry;
		} else {
			pr_info("%s: calibration failed, use default\n",
					mmc_hostname(host->mmc));
			return -1;
		}
	}
	host->is_tunning = 0;

	ret = aml_cali_find(mmc, &c_data);
	/* retry cali here! */
	if (ret) {
		if (cali_retry < MAX_CALI_RETRY) {
			cali_retry++;
			goto _cali_retry;
		} else {
			pr_info("%s: calibration failed, use default\n",
				mmc_hostname(host->mmc));
			return -1;
		}
	}
	pr_info("calibration @%d times ok\n", cali_retry);

	/* restore original clk setting */
#ifdef SD_EMMC_CLK_CTRL
	vclk = readl(host->base + SD_EMMC_CLOCK);
	clkc->div = clk_div_tmp;
	writel(vclk, host->base + SD_EMMC_CLOCK);
#else
	clk_set_rate(host->cfg_div_clk, clk_tmp);
	vclk = readl(host->base + SD_EMMC_CLOCK);
#endif
	pdata->clkc = vclk;
	if (!type) {
		/* set default cmd delay*/
		adjust = readl(host->base + SD_EMMC_ADJUST);
		if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB)
			gadjust->cmd_delay = 7;
		writel(adjust, host->base + SD_EMMC_ADJUST);
	}

	pr_info("%s: base_index_max %d, base_index_min %d\n",
			mmc_hostname(mmc), c_data.base_index_max,
			c_data.base_index_min);

	/* get adjust point! */
	*adj_win_start = c_data.base_index_max + 2;

	return 0;
}
#endif

u32 aml_sd_emmc_tuning_transfer(struct mmc_host *mmc,
	u32 opcode, const u8 *blk_pattern, u8 *blk_test, u32 blksz)
{
	struct amlsd_host *host = mmc_priv(mmc);
	u32 vctrl = readl(host->base + SD_EMMC_CFG);
	struct sd_emmc_config *ctrl = (struct sd_emmc_config *)&vctrl;
	u32 tuning_err = 0;
	u32 n, nmatch;
	/* try ntries */
	for (n = 0, nmatch = 0; n < TUNING_NUM_PER_POINT; n++) {
		tuning_err = aml_sd_emmc_cali_transfer(mmc,
						opcode, blk_test, blksz);
		if (!tuning_err) {
			if (ctrl->ddr == 1)
				nmatch++;
			else if (!memcmp(blk_pattern, blk_test, blksz))
				nmatch++;
			else {
				sd_emmc_dbg(AMLSD_DBG_TUNING,
				"nmatch=%d\n", nmatch);
				break;
			}
		} else {
			sd_emmc_dbg(AMLSD_DBG_TUNING,
				"Tuning transfer error:");
			sd_emmc_dbg(AMLSD_DBG_TUNING,
		       "nmatch=%d\n", nmatch);
			break;
		}
	}
	return nmatch;
}

static int aml_tuning_adj(struct mmc_host *mmc, u32 opcode,
		struct aml_tuning_data *tuning_data,
		int *best_start, int *best_size)
{
	struct amlsd_host *host = mmc_priv(mmc);
	u32 adjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	u32 vclk;
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&(vclk);
	u32 vctrl;
	struct sd_emmc_config *ctrl = (struct sd_emmc_config *)&vctrl;
	u32 clk_rate = 1000000000, clock, clk_div, nmatch = 0;
	int adj_delay = 0;
	const u8 *blk_pattern = tuning_data->blk_pattern;
	unsigned int blksz = tuning_data->blksz;
	int wrap_win_start = -1, wrap_win_size = 0;
	int best_win_start = -1, best_win_size = 0;
	int curr_win_start = -1, curr_win_size = 0;

	vclk = readl(host->base + SD_EMMC_CLOCK);
	vctrl = readl(host->base + SD_EMMC_CFG);
	clk_div = clkc->div;
	clock = clk_rate / clk_div;/*200MHz, bus_clk */
	mmc->actual_clock = ctrl->ddr ?
		(clock / 2) : clock;/*100MHz in ddr */
	if (ctrl->ddr == 1) {
		blksz = 512;
		opcode = 17;
	}

	pr_info("%s: clk %d %s tuning start\n",
		mmc_hostname(mmc), (ctrl->ddr ? (clock / 2) : clock),
			(ctrl->ddr ? "DDR mode" : "SDR mode"));
	for (adj_delay = 0; adj_delay < clk_div; adj_delay++) {
		adjust = readl(host->base + SD_EMMC_ADJUST);
		gadjust->adj_delay = adj_delay;
		gadjust->adj_enable = 1;
		gadjust->cali_enable = 0;
		gadjust->cali_rise = 0;
		writel(adjust, host->base + SD_EMMC_ADJUST);
		nmatch = aml_sd_emmc_tuning_transfer(mmc, opcode,
				blk_pattern, host->blk_test, blksz);
		/*get a ok adjust point!*/
		if (nmatch == TUNING_NUM_PER_POINT) {
			if (adj_delay == 0)
				wrap_win_start = adj_delay;
			if (wrap_win_start >= 0)
				wrap_win_size++;
			if (curr_win_start < 0)
				curr_win_start = adj_delay;
			curr_win_size++;
			pr_info("%s: rx_tuning_result[%d] = %d\n",
				mmc_hostname(host->mmc), adj_delay, nmatch);
		} else {
			if (curr_win_start >= 0) {
				if (best_win_start < 0) {
					best_win_start = curr_win_start;
					best_win_size = curr_win_size;
				} else {
					if (best_win_size < curr_win_size) {
						best_win_start = curr_win_start;
						best_win_size = curr_win_size;
					}
				}
				wrap_win_start = -1;
				curr_win_start = -1;
				curr_win_size = 0;
			}
		}
	}
	/* last point is ok! */
	if (curr_win_start >= 0) {
		if (best_win_start < 0) {
			best_win_start = curr_win_start;
			best_win_size = curr_win_size;
		} else if (wrap_win_size > 0) {
			/* Wrap around case */
			if (curr_win_size + wrap_win_size > best_win_size) {
				best_win_start = curr_win_start;
				best_win_size = curr_win_size + wrap_win_size;
			}
		} else if (best_win_size < curr_win_size) {
			best_win_start = curr_win_start;
			best_win_size = curr_win_size;
		}

		curr_win_start = -1;
		curr_win_size = 0;
	}
	*best_start = best_win_start;
	*best_size = best_win_size;
	return 0;
}

/* TODO....., based on new tuning function */
int aml_sd_emmc_execute_tuning_(struct mmc_host *mmc, u32 opcode,
					struct aml_tuning_data *tuning_data,
					u32 adj_win_start)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;
	u32 vclk;
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&(vclk);
	u32 adjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	u32 clk_rate = 1000000000;
	unsigned long flags;
	int ret = 0;
	struct aml_emmc_adjust *emmc_adj = &host->emmc_adj;
	u8 tuning_num = 0;
	u32 clock, clk_div;
	u32 adj_delay_find;
	int best_win_start = -1, best_win_size = 0;

	writel(0, host->base + SD_EMMC_ADJUST);

tunning:
	spin_lock_irqsave(&host->mrq_lock, flags);
	pdata->need_retuning = false;
	spin_unlock_irqrestore(&host->mrq_lock, flags);
	vclk = readl(host->base + SD_EMMC_CLOCK);
	clk_div = clkc->div;
	clock = clk_rate / clk_div;/*200MHz, bus_clk */

	host->is_tunning = 1;
	ret = aml_tuning_adj(mmc, opcode,
			tuning_data, &best_win_start, &best_win_size);
	if (ret)
		return -ENOMEM;
	if (best_win_size <= 0) {
		if ((tuning_num++ > MAX_TUNING_RETRY)
			|| (clkc->div >= 10)) {
			pr_info("%s: final result of tuning failed\n",
				 mmc_hostname(host->mmc));
			return -1;
		}
		clkc->div += 1;
		writel(vclk, host->base + SD_EMMC_CLOCK);
		mmc->actual_clock = clk_rate / clkc->div;
		pdata->clkc = vclk;
		pr_info("%s: tuning failed, reduce freq and retuning\n",
			mmc_hostname(host->mmc));
		goto tunning;
	} else {
		pr_info("%s: best_win_start =%d, best_win_size =%d\n",
			mmc_hostname(host->mmc), best_win_start, best_win_size);
	}

	if ((best_win_size != clk_div)
		|| (aml_card_type_sdio(pdata)
			&& (get_cpu_type() == MESON_CPU_MAJOR_ID_GXM))) {
		adj_delay_find = best_win_start + (best_win_size - 1) / 2
						+ (best_win_size - 1) % 2;
		adj_delay_find = adj_delay_find % clk_div;
	} else
		adj_delay_find = 0;

	/* fixme, for retry debug. */
	if (aml_card_type_mmc(pdata)
		&& (clk_div <= 5) && (adj_win_start != 100)
		&& (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB)) {
		pr_info("%s: adj_win_start %d\n",
			mmc_hostname(host->mmc), adj_win_start);
		adj_delay_find = adj_win_start % clk_div;
	}
	adjust = readl(host->base + SD_EMMC_ADJUST);
	gadjust->adj_delay = adj_delay_find;
	gadjust->adj_enable = 1;
	gadjust->cali_enable = 0;
	gadjust->cali_rise = 0;
	writel(adjust, host->base + SD_EMMC_ADJUST);
	host->is_tunning = 0;

	/* fixme, yyh for retry flow. */
	emmc_adj->adj_win_start = best_win_start;
	emmc_adj->adj_win_len = best_win_size;
	emmc_adj->adj_point = adj_delay_find;
	emmc_adj->clk_div = clk_div;
	pr_info("%s: clock=0x%x, adjust=0x%x\n",
			mmc_hostname(host->mmc),
			readl(host->base + SD_EMMC_CLOCK),
			readl(host->base + SD_EMMC_ADJUST));
	return ret;
}

static int aml_sd_emmc_rxclk_find(struct mmc_host *mmc,
		u8 *rx_tuning_result, u8 total_point,
		int *best_win_start, int *best_win_size)
{
	struct amlsd_host *host = mmc_priv(mmc);
	int wrap_win_start = -1, wrap_win_size = 0;
	int curr_win_start = -1, curr_win_size = 0;
	u8 n;
	int rxclk_find;

	for (n = 0; n < total_point; n++) {
		if (rx_tuning_result[n] == TUNING_NUM_PER_POINT) {
			if (n == 0)
				wrap_win_start = n;
			if (wrap_win_start >= 0)
				wrap_win_size++;
			if (curr_win_start < 0)
				curr_win_start = n;
			curr_win_size++;
			pr_info("%s: rx_tuning_result[%d] = %d\n",
				mmc_hostname(host->mmc), n,
				TUNING_NUM_PER_POINT);
		} else {
			if (curr_win_start >= 0) {
				if (*best_win_start < 0) {
					*best_win_start = curr_win_start;
					*best_win_size = curr_win_size;
				} else {
					if (*best_win_size < curr_win_size) {
						*best_win_start =
								curr_win_start;
						*best_win_size = curr_win_size;
					}
				}

				wrap_win_start = -1;
				curr_win_start = -1;
				curr_win_size = 0;
			}

		}
	}
	/* last point is ok! */
	if (curr_win_start >= 0) {
		if (*best_win_start < 0) {
			*best_win_start = curr_win_start;
			*best_win_size = curr_win_size;
		} else if (wrap_win_size > 0) {
			/* Wrap around case */
			if (curr_win_size + wrap_win_size > *best_win_size) {
				*best_win_start = curr_win_start;
				*best_win_size = curr_win_size + wrap_win_size;
			}
		} else if (*best_win_size < curr_win_size) {
			*best_win_start = curr_win_start;
			*best_win_size = curr_win_size;
		}

		curr_win_start = -1;
		curr_win_size = 0;
	}
	if (*best_win_size <= 0) {
		rxclk_find = -1;
	} else {
		pr_info("%s: best_win_start =%d, best_win_size =%d\n",
			mmc_hostname(host->mmc), *best_win_start,
			*best_win_size);
		rxclk_find = *best_win_start + (*best_win_size + 1) / 2;
		rxclk_find %= total_point;
	}

	return rxclk_find;
}

static int aml_sd_emmc_execute_tuning_rxclk(struct mmc_host *mmc, u32 opcode,
		struct aml_tuning_data *tuning_data)
{
	/* need finish later */
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;
	u32 vclk;
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&(vclk);
	u32 vctrl;
	struct sd_emmc_config *ctrl = (struct sd_emmc_config *)&vctrl;
	u32 clk_rate = 1000000000;
	const u8 *blk_pattern = tuning_data->blk_pattern;
	unsigned int blksz = tuning_data->blksz;
	unsigned long flags;
	u8 steps, nmatch;
	u8 rx_phase, rx_delay;
	struct aml_emmc_rxclk *emmc_rxclk = &host->emmc_rxclk;
	u32 clock;
	int rxclk_find;
	u8 rx_tuning_result[25] = {0};
	u8 total_point = 25;
	int best_win_start = -1, best_win_size = 0;

	spin_lock_irqsave(&host->mrq_lock, flags);
	pdata->need_retuning = false;
	spin_unlock_irqrestore(&host->mrq_lock, flags);
	vclk = readl(host->base + SD_EMMC_CLOCK);
	vctrl = readl(host->base + SD_EMMC_CFG);

	clock = clk_rate / clkc->div;/*200mhz, bus_clk */
	mmc->actual_clock = ctrl->ddr ?
		(clock / 2) : clock;/*100mhz in ddr */

	if (ctrl->ddr == 1) {
		blksz = 512;
		opcode = 17;
	}

	host->is_tunning = 1;
	pr_info("%s: clk %d %s tuning start\n",
			mmc_hostname(mmc), (ctrl->ddr ? (clock / 2) : clock),
			(ctrl->ddr ? "DDR mode" : "SDR mode"));
	for (rx_phase = 0; rx_phase < 4; rx_phase += 2) {
		if (rx_phase == 0)
			steps = 10;
		else
			steps = 15;
		for (rx_delay = 0; rx_delay < steps; rx_delay++) {
			vclk = readl(host->base + SD_EMMC_CLOCK);
			clkc->rx_delay = rx_delay;
			clkc->rx_phase = rx_phase;
			writel(vclk, host->base + SD_EMMC_CLOCK);
			pdata->clkc = vclk;
			nmatch = aml_sd_emmc_tuning_transfer(mmc, opcode,
				blk_pattern, host->blk_test, blksz);
			rx_tuning_result[rx_phase * 5 + rx_delay] = nmatch;
		}
	}
	host->is_tunning = 0;
	rxclk_find = aml_sd_emmc_rxclk_find(mmc,
					rx_tuning_result, total_point,
					&best_win_start, &best_win_size);

	if (rxclk_find < 0) {
		pr_info("%s: tuning failed\n", mmc_hostname(host->mmc));
		return -1;
	} else if (rxclk_find < 10) {
		rx_phase = 0;
		rx_delay = rxclk_find;
	} else {
		rx_phase = 2;
		rx_delay = rxclk_find - 10;
	}

	pr_info("%s: rx_phase = %d, rx_delay = %d,",
			mmc_hostname(host->mmc), rx_phase, rx_delay);

	vclk = readl(host->base + SD_EMMC_CLOCK);
	clkc->rx_phase = rx_phase;
	clkc->rx_delay = rx_delay;
	writel(vclk, host->base + SD_EMMC_CLOCK);
	pdata->clkc = vclk;

	emmc_rxclk->rxclk_win_start = best_win_start;
	emmc_rxclk->rxclk_win_len = best_win_size;
	emmc_rxclk->rxclk_rx_phase = rx_phase;
	emmc_rxclk->rxclk_rx_delay = rx_delay;
	emmc_rxclk->rxclk_point = rxclk_find;

	return 0;
}

static int aml_mmc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	int err = 0;
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;
	u32 adjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	u32 vclk = readl(host->base + SD_EMMC_CLOCK);
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&(vclk);
	struct aml_tuning_data tuning_data;
	u32 adj_win_start = 100;

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

#ifdef AML_CALIBRATION
	if ((aml_card_type_mmc(pdata))
			&& (mmc->ios.timing != MMC_TIMING_MMC_HS400)) {
		if (clkc->div <= 10) {
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)
				err = aml_sd_emmc_execute_calibration(mmc,
						&adj_win_start, 1);
			else if (clkc->div <= 7)
				err = aml_sd_emmc_execute_calibration(mmc,
						&adj_win_start, 0);
		}
		/* if calibration failed, gdelay use default value */
		if (err) {
			if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB)
				writel(0x85854055, host->base + SD_EMMC_DELAY);
			else
				writel(0x10101331, host->base + SD_EMMC_DELAY);
		}
	}
#endif
	/* execute tuning... */
	if ((clkc->div > 5)
		|| (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB)) {
		err = aml_sd_emmc_execute_tuning_(mmc, opcode,
				&tuning_data, adj_win_start);
		if (!err)
			host->tuning_mode = ADJ_TUNING_MODE;
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
		if (aml_card_type_sdio(pdata)) {
			err = aml_sd_emmc_execute_tuning_(mmc, opcode,
					&tuning_data, adj_win_start);
			if (!err)
				host->tuning_mode = ADJ_TUNING_MODE;
		} else {
			err = 0;
			adjust = readl(host->base + SD_EMMC_ADJUST);
			gadjust->cali_enable = 1;
			gadjust->adj_auto = 1;
			writel(adjust, host->base + SD_EMMC_ADJUST);
			host->tuning_mode = AUTO_TUNING_MODE;
		}
	} else {
		err = aml_sd_emmc_execute_tuning_rxclk(mmc, opcode,
				&tuning_data);
		if (!err)
			host->tuning_mode = RX_PHASE_DELAY_TUNING_MODE;
	}

	pr_info("%s: clock =0x%x, delay=0x%x, adjust=0x%x\n",
			mmc_hostname(mmc),
			readl(host->base + SD_EMMC_CLOCK),
			readl(host->base + SD_EMMC_DELAY),
			readl(host->base + SD_EMMC_ADJUST));

	return err;

}

static void aml_mmc_clk_switch_off(struct amlsd_host *host)
{
	u32 vcfg = 0;
	struct sd_emmc_config *conf = (struct sd_emmc_config *)&vcfg;

	if (host->is_gated) {
		sd_emmc_dbg(AMLSD_DBG_IOS, "direct return\n");
		return;
	}

	/*Turn off Clock, here close whole clk for controller*/
	vcfg = readl(host->base + SD_EMMC_CFG);
	conf->stop_clk = 1;
	writel(vcfg, host->base + SD_EMMC_CFG);

	host->is_gated = true;
	/* sd_emmc_err("clock off\n"); */
}

void aml_mmc_clk_switch_on(
	struct amlsd_host *host, int clk_div, int clk_src_sel)
{
	u32 vclkc = 0;
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&vclkc;
	u32 vcfg = 0;
	struct sd_emmc_config *conf = (struct sd_emmc_config *)&vcfg;
	struct amlsd_platform *pdata = host->pdata;

	WARN_ON(!clk_div);

	vclkc = readl(host->base + SD_EMMC_CLOCK);
	/*Set clock divide*/
	clkc->div = clk_div;
	clkc->src = clk_src_sel;
	writel(vclkc, host->base + SD_EMMC_CLOCK);
	pdata->clkc = vclkc;

	/*Turn on Clock*/
	vcfg = readl(host->base + SD_EMMC_CFG);
	conf->stop_clk = 0;
	writel(vcfg, host->base + SD_EMMC_CFG);

	host->is_gated = false;
}

static void aml_mmc_clk_switch(struct amlsd_host *host,
	int clk_div, int clk_src_sel)
{
	u32 vclkc = 0;
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&vclkc;

	vclkc = readl(host->base + SD_EMMC_CLOCK);
	if (!host->is_gated && (clkc->div == clk_div)
				&& (clkc->src == clk_src_sel)) {
		/* sd_emmc_err("direct return\n"); */
		return; /* if the same, return directly */
	}

	aml_mmc_clk_switch_off(host);
	/* mdelay(1); */

	WARN_ON(!clk_div);

	aml_mmc_clk_switch_on(host, clk_div, clk_src_sel);
}

void aml_sd_emmc_set_clkc(struct amlsd_host *host)
{
	u32 vclkc = 0;
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&vclkc;
	struct amlsd_platform *pdata = host->pdata;

	vclkc = readl(host->base + SD_EMMC_CLOCK);
	if (!host->is_gated && (pdata->clkc == vclkc))
		return;

	if (host->is_gated)
		aml_mmc_clk_switch(host, clkc->div, clkc->src);
	else
		writel(pdata->clkc, host->base + SD_EMMC_CLOCK);
}

static int meson_mmc_clk_set_rate(struct amlsd_host *host,
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

int aml_emmc_clktree_init(struct amlsd_host *host)
{
	int i, ret = 0;
	unsigned int f_min = UINT_MAX, mux_parent_count = 0;
	const char *mux_parent_names[MUX_CLK_NUM_PARENTS];
	struct clk_init_data init;
	char clk_name[32], name[16];
	const char *clk_div_parents[1];

	host->core_clk = devm_clk_get(host->dev, "core");
	if (IS_ERR(host->core_clk)) {
		ret = PTR_ERR(host->core_clk);
		return ret;
	}
	pr_info("core->rate: %lu\n", clk_get_rate(host->core_clk));
	pr_info("core->name: %s\n", __clk_get_name(host->core_clk));
	ret = clk_prepare_enable(host->core_clk);
	if (ret)
		return ret;

	/* get the mux parents */
	for (i = 0; i < MUX_CLK_NUM_PARENTS; i++) {
		snprintf(name, sizeof(name), "clkin%d", i);
		host->mux_parent[i] = devm_clk_get(host->dev, name);
		if (IS_ERR(host->mux_parent[i])) {
			ret = PTR_ERR(host->mux_parent[i]);
			if (PTR_ERR(host->mux_parent[i]) != -EPROBE_DEFER)
				dev_err(host->dev, "Missing clock %s\n", name);
			host->mux_parent[i] = NULL;
			return ret;
		}
		host->mux_parent_rate[i] = clk_get_rate(host->mux_parent[i]);
		mux_parent_names[i] = __clk_get_name(host->mux_parent[i]);
		pr_info("rate: %lu, name: %s\n",
			host->mux_parent_rate[i], mux_parent_names[i]);
		mux_parent_count++;
		if (host->mux_parent_rate[i] < f_min)
			f_min = host->mux_parent_rate[i];
		ret = clk_prepare_enable(host->mux_parent[i]);
	}

	/* cacluate f_min based on input clocks, and max divider value */
	if (f_min != UINT_MAX)
		f_min = DIV_ROUND_UP(CLK_SRC_XTAL_RATE, CLK_DIV_MAX);
	else
		f_min = 400000;  /* default min: 400 KHz */
	host->mmc->f_min = f_min;

	/* create the mux */
	snprintf(clk_name, sizeof(clk_name), "%s#mux", dev_name(host->dev));
	pr_info("clk_name: %s\n", clk_name);
	init.name = clk_name;
	init.ops = &clk_mux_ops;
	init.flags = 0;
	init.parent_names = mux_parent_names;
	init.num_parents = mux_parent_count;
	host->mux.reg = host->base + SD_EMMC_CLOCK;
	host->mux.shift = CLK_SRC_SHIFT;
	host->mux.mask = CLK_SRC_MASK;
	host->mux.flags = 0;
	host->mux.table = NULL;
	host->mux.hw.init = &init;
	host->mux_clk = devm_clk_register(host->dev, &host->mux.hw);
	if (WARN_ON(IS_ERR(host->mux_clk)))
		return PTR_ERR(host->mux_clk);

	/* create the divider */
	snprintf(clk_name, sizeof(clk_name), "%s#div", dev_name(host->dev));
	init.name = devm_kstrdup(host->dev, clk_name, GFP_KERNEL);
	init.ops = &clk_divider_ops;
	init.flags = CLK_SET_RATE_PARENT;
	clk_div_parents[0] = __clk_get_name(host->mux_clk);
	init.parent_names = clk_div_parents;
	init.num_parents = ARRAY_SIZE(clk_div_parents);
	host->cfg_div.reg = host->base + SD_EMMC_CLOCK;
	host->cfg_div.shift = CLK_DIV_SHIFT;
	host->cfg_div.width = CLK_DIV_WIDTH;
	host->cfg_div.hw.init = &init;
	host->cfg_div.flags = CLK_DIVIDER_ONE_BASED |
		CLK_DIVIDER_ROUND_CLOSEST | CLK_DIVIDER_ALLOW_ZERO;
	host->cfg_div_clk = devm_clk_register(host->dev, &host->cfg_div.hw);
	if (WARN_ON(PTR_ERR_OR_ZERO(host->cfg_div_clk)))
		return PTR_ERR(host->cfg_div_clk);

	ret = clk_prepare_enable(host->cfg_div_clk);
	pr_info("[%s] clock: 0x%x\n",
		__func__, readl(host->base + SD_EMMC_CLOCK_V3));
	return ret;
}

/*
 * The SD/eMMC IP block has an internal mux and divider used for
 * generating the MMC clock.  Use the clock framework to create and
 * manage these clocks.
 */
static int meson_mmc_clk_init(struct amlsd_host *host)
{
	int ret = 0;
	u32 vclkc = 0;
	struct sd_emmc_clock *pclkc = (struct sd_emmc_clock *)&vclkc;
	u32 vconf = 0;
	struct sd_emmc_config *pconf = (struct sd_emmc_config *)&vconf;
	struct amlsd_platform *pdata = host->pdata;

	writel(0, host->base + SD_EMMC_CLOCK);
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

static void aml_sd_emmc_tx_phase_set(struct amlsd_host *host)
{
	struct amlsd_platform *pdata = host->pdata;
	u32 vclkc = 0;
	struct sd_emmc_clock *pclkc = (struct sd_emmc_clock *)&vclkc;

	vclkc = readl(host->base + SD_EMMC_CLOCK);
	pclkc->tx_phase = pdata->tx_phase;
	if (pdata->tx_delay)
		pclkc->tx_delay = pdata->tx_delay;

	writel(vclkc, host->base + SD_EMMC_CLOCK);
}

static void aml_sd_emmc_set_timing(
		struct amlsd_host *host, u32 timing)
{
	struct amlsd_platform *pdata = host->pdata;
	u32 vctrl;
	struct sd_emmc_config *ctrl = (struct sd_emmc_config *)&vctrl;
	u32 vclkc;
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&vclkc;
	u32 adjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	u8 clk_div;
	u32 clk_rate = 1000000000;

	vctrl = readl(host->base + SD_EMMC_CFG);
	if ((timing == MMC_TIMING_MMC_HS400) ||
			(timing == MMC_TIMING_MMC_DDR52) ||
			(timing == MMC_TIMING_UHS_DDR50)) {
		if (timing == MMC_TIMING_MMC_HS400) {
			ctrl->chk_ds = 1;
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
				adjust = readl(host->base + SD_EMMC_ADJUST);
				gadjust->ds_enable = 1;
				writel(adjust, host->base + SD_EMMC_ADJUST);
				host->tuning_mode = AUTO_TUNING_MODE;
			}
		}
		vclkc = readl(host->base + SD_EMMC_CLOCK);
		ctrl->ddr = 1;
		clk_div = clkc->div;
		if (clk_div & 0x01)
			clk_div++;
		clkc->div = clk_div / 2;
		writel(vclkc, host->base + SD_EMMC_CLOCK);
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

/*setup bus width, 1bit, 4bits, 8bits*/
void aml_sd_emmc_set_buswidth(
		struct amlsd_host *host, u32 busw_ios)
{
	u32 vconf;
	struct sd_emmc_config *conf = (struct sd_emmc_config *)&vconf;
	u32 width = 0;

	switch (busw_ios) {
	case MMC_BUS_WIDTH_1:
			width = 0;
		break;
	case MMC_BUS_WIDTH_4:
			width = 1;
		break;
	case MMC_BUS_WIDTH_8:
			width = 2;
		break;
	default:
		sd_emmc_err("%s: error Data Bus\n",
				mmc_hostname(host->mmc));
		break;
	}

	if (width != host->bus_width) {
		vconf = readl(host->base + SD_EMMC_CFG);
		conf->bus_width = width;
		writel(vconf, host->base + SD_EMMC_CFG);
		host->bus_width = width;
		sd_emmc_dbg(AMLSD_DBG_IOS, "Bus Width Ios %d\n", busw_ios);
	}
}

/*call by mmc, power on, power off ...*/
static void aml_sd_emmc_set_power(struct amlsd_host *host, u32 power_mode)
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
		writel(0, host->base + SD_EMMC_DELAY);
		writel(0, host->base + SD_EMMC_ADJUST);
		break;
	default:
		if (pdata->pwr_pre)
			pdata->pwr_pre(pdata);
		if (pdata->pwr_off)
			pdata->pwr_off(pdata);
		break;
	}
}

static void meson_mmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;

	if (!pdata->is_in)
		return;

	/*Set Power*/
	aml_sd_emmc_set_power(host, ios->power_mode);

	/* Set Clock */
	meson_mmc_clk_set_rate(host, ios->clock);

	/* Set Bus Width */
	aml_sd_emmc_set_buswidth(host, ios->bus_width);

	/* Set Date Mode */
	aml_sd_emmc_set_timing(host, ios->timing);

	if (ios->chip_select == MMC_CS_HIGH)
		aml_cs_high(mmc);
	else if (ios->chip_select == MMC_CS_DONTCARE)
		aml_cs_dont_care(mmc);
}

#ifdef SD_EMMC_REQ_DMA_SGMAP
static char *aml_sd_emmc_kmap_atomic(
		struct scatterlist *sg, unsigned long *flags)
{
	local_irq_save(*flags);
	return kmap_atomic(sg_page(sg)) + sg->offset;
}

static void aml_sd_emmc_kunmap_atomic(
		void *buffer, unsigned long *flags)
{
	kunmap_atomic(buffer);
	local_irq_restore(*flags);
}

/*
 * aml_sg_copy_buffer - Copy data between
 * a linear buffer and an SG list  for amlogic,
 * We don't disable irq in this function
 */
static unsigned int aml_sd_emmc_pre_dma(struct amlsd_host *host,
	struct mmc_request *mrq, struct sd_emmc_desc_info *desc)
{
	struct mmc_data *data = NULL;
	struct scatterlist *sg;
	struct sd_emmc_desc_info *desc_cur = NULL;
	struct cmd_cfg *des_cmd_cur = NULL;
	dma_addr_t sg_addr = 0;
	char *buffer = NULL;
	unsigned int desc_cnt = 0, i = 0, data_len = 0;
	unsigned int data_size = 0, sg_blocks = 0;
	unsigned char direction = 0, data_rw = 0;
	unsigned char block_mode = 0, data_num = 0, bl_len = 0;
	unsigned long flags;

	data = mrq->cmd->data;
	if (data == NULL) {
		WARN_ON(1);
		goto err_exit;
	}

	if (data->flags & MMC_DATA_READ) {
		direction = DMA_FROM_DEVICE;
		data_rw = 0;
	} else{
		direction = DMA_TO_DEVICE;
		data_rw = 1;
	}

	host->sg_cnt = dma_map_sg(mmc_dev(host->mmc),
		data->sg, data->sg_len, direction);
	/*
	 * This only happens when someone fed
	 * us an invalid request.
	 */
	if (host->sg_cnt == 0) {
		WARN_ON(1);
		goto err_exit;
	}
#ifdef CHOICE_DEBUG
	pr_info("%s %d sg_cnt:%d, direction:%d\n",
	 __func__, __LINE__, host->sg_cnt, direction);
#endif

	data_size = (mrq->cmd->data->blksz * mrq->cmd->data->blocks);
	block_mode = ((mrq->cmd->data->blocks > 1)
		|| (mrq->cmd->data->blksz >= 512)) ? 1 : 0;

	data_num = 0;/*(data_size > 4) ? 0 : 1;*/
	bl_len = block_mode ? log2i(mrq->cmd->data->blksz) : 0;
	host->dma_sts = 0;
	if ((data_size & 0x3) && (host->sg_cnt > 1)) {
		host->dma_sts = (1<<0); /*  */
		pr_info("data:%d and sg_cnt:%d\n", data_size, host->sg_cnt);
	}
#ifdef CHOICE_DEBUG
	pr_info("%s %d sg_cnt:%d, block_mode:%d,\n",
			__func__, __LINE__, host->sg_cnt, block_mode);
	pr_info("data_num:%d bl_len:%d, blocks:%d, blksz:%d\n",
			data_num, bl_len, mrq->cmd->data->blocks,
			mrq->cmd->data->blksz);
#endif

	/* prepare desc for data operation */
	desc_cur = desc;

	for_each_sg(data->sg, sg, data->sg_len, i) {
		WARN_ON(sg->length & 0x3);

		des_cmd_cur = (struct cmd_cfg *)&(desc_cur->cmd_info);
		if (desc_cnt != 0) { /* for first desc, */
			des_cmd_cur->no_resp = 1;
			des_cmd_cur->no_cmd = 1;
		}
		des_cmd_cur->data_io = 1;

		des_cmd_cur->owner = 1;
		des_cmd_cur->timeout = 0xc;

		des_cmd_cur->data_wr = data_rw;
		des_cmd_cur->block_mode = block_mode;
		des_cmd_cur->data_num = data_num;

		data_len = block_mode ?
			(sg_dma_len(sg)>>bl_len) : sg_dma_len(sg);

		if ((data_len > 0x1ff) || (data_len == 0)) {
			pr_info("Error block_mode:%d, data_len:%d, bl_len:%d\n",
				block_mode, data_len, bl_len);
			pr_info("mrq->cmd->data->blocks:%d, mrq->cmd->data->blksz:%d\n",
				mrq->cmd->data->blocks, mrq->cmd->data->blksz);
			WARN_ON(1);
		}
		des_cmd_cur->length = data_len;

		sg_blocks += des_cmd_cur->length;
		sg_addr = sg_dma_address(sg);

		if (sg_addr & 0x7) { /* for no 64 bit addr alignment mode */
			WARN_ON(host->sg_cnt > 1);

			host->dma_sts |= (1<<1); /*  */

			host->dma_sts |= (1<<3); /*  */
			desc_cur->data_addr = host->bn_dma_buf;

			if (data->flags & MMC_DATA_WRITE) {
				buffer = aml_sd_emmc_kmap_atomic(sg, &flags);
				memcpy(host->bn_buf, buffer, data_size);
				aml_sd_emmc_kunmap_atomic(buffer, &flags);
			}
		} else{
			desc_cur->data_addr = sg_addr;
			/* desc_cur->data_addr &= ~(1<<0);   //DDR */
		}
#ifdef CHOICE_DEBUG
	aml_sd_emmc_desc_print_info(desc_cur);
	pr_info("%s %d desc_cur->data_addr : 0x%x\n",
		"des_cmd_cur->length:%d, sg->length:%d\n",
		"sg_dma_len(sg):%d, bl_len:%d\n",
		__func__, __LINE__, desc_cur->data_addr,
		des_cmd_cur->length, sg->length,
		sg_dma_len(sg), bl_len);
#endif
		desc_cur++;
		desc_cnt++;
		memset(desc_cur, 0, sizeof(struct sd_emmc_desc_info));
	}

	WARN_ON(desc_cnt != host->sg_cnt);

err_exit:
	return host->sg_cnt;
}

/**
 * aml_sg_copy_buffer - Copy data between
 *a linear buffer and an SG list  for amlogic,
 * We don't disable irq in this function
 **/
int aml_sd_emmc_post_dma(struct amlsd_host *host,
		struct mmc_request *mrq)
{
	struct mmc_data *data = NULL;
	struct scatterlist *sg;
	char *buffer = NULL;
	unsigned long flags;
	int i, ret = 0;


	data = mrq->cmd->data;
	if (data == NULL) {
		WARN_ON(1);
		ret = -1;
		goto err_exit;
	}

	if ((data->flags & MMC_DATA_READ) && (host->dma_sts & (1<<1))) {
		dma_sync_sg_for_cpu(mmc_dev(host->mmc), data->sg,
			data->sg_len, DMA_FROM_DEVICE);

		for_each_sg(data->sg, sg, host->sg_cnt, i) {
			if (sg_dma_address(sg) & 0x7) {
				WARN_ON(!(host->dma_sts & (0x3<<2)));

				buffer = aml_sd_emmc_kmap_atomic(sg, &flags);
				memcpy(buffer, host->bn_buf,
				(mrq->data->blksz * mrq->data->blocks));
				aml_sd_emmc_kunmap_atomic(buffer, &flags);
			}
		}
	}

	dma_unmap_sg(mmc_dev(host->mmc), data->sg, data->sg_len,
		(data->flags & MMC_DATA_READ) ?
			DMA_FROM_DEVICE : DMA_TO_DEVICE);

err_exit:
	return ret;
}
#endif

static void aml_sd_emmc_check_sdio_irq(struct amlsd_host *host)
{
	u32 vstat = readl(host->base + SD_EMMC_STATUS);
	struct sd_emmc_status *ista = (struct sd_emmc_status *)&vstat;

	if (host->sdio_irqen) {
		if ((ista->irq_sdio || !(ista->dat_i & 0x02)) &&
			(host->mmc->sdio_irq_thread) &&
			(!atomic_read(&host->mmc->sdio_irq_thread_abort))) {
			/* pr_info("signalling irq 0x%x\n", vstat); */
			mmc_signal_sdio_irq(host->mmc);
		}
	}
}
int meson_mmc_request_done(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;
	unsigned long flags;

	WARN_ON(host->mrq != mrq);

	spin_lock_irqsave(&host->mrq_lock, flags);
	host->xfer_step = XFER_FINISHED;
	host->mrq = NULL;
	host->status = HOST_INVALID;
	spin_unlock_irqrestore(&host->mrq_lock, flags);
	if (pdata->xfer_post)
		pdata->xfer_post(mmc);

	aml_sd_emmc_check_sdio_irq(host);
	mmc_request_done(host->mmc, mrq);

	return 0;
}

static void __attribute__((unused))aml_sd_emmc_mrq_print_info(
	struct mmc_request *mrq, unsigned int desc_cnt)
{
	pr_info("*mmc_request desc_cnt:%d cmd:%d, arg:0x%x, flags:0x%x",
	desc_cnt, mrq->cmd->opcode, mrq->cmd->arg, mrq->cmd->flags);

	if (mrq->cmd->data)
		pr_info(", blksz:%d, blocks:0x%x",
			mrq->data->blksz, mrq->data->blocks);

	pr_info("\n");

}

static void __attribute__((unused))
	aml_sd_emmc_desc_print_info(struct sd_emmc_desc_info *desc_info)
{
	struct cmd_cfg *des_cmd_cur =
		(struct cmd_cfg *)&(desc_info->cmd_info);

	pr_info("#####desc_info check, desc_info:0x%p\n",
		desc_info);

	pr_info("\tlength:%d\n", des_cmd_cur->length);
	pr_info("\tblock_mode:%d\n", des_cmd_cur->block_mode);
	pr_info("\tr1b:%d\n", des_cmd_cur->r1b);
	pr_info("\tend_of_chain:%d\n", des_cmd_cur->end_of_chain);
	pr_info("\ttimeout:%d\n", des_cmd_cur->timeout);
	pr_info("\tno_resp:%d\n", des_cmd_cur->no_resp);
	pr_info("\tno_cmd:%d\n", des_cmd_cur->no_cmd);
	pr_info("\tdata_io:%d\n", des_cmd_cur->data_io);
	pr_info("\tdata_wr:%d\n", des_cmd_cur->data_wr);
	pr_info("\tresp_nocrc:%d\n", des_cmd_cur->resp_nocrc);
	pr_info("\tresp_128:%d\n", des_cmd_cur->resp_128);
	pr_info("\tresp_num:%d\n", des_cmd_cur->resp_num);
	pr_info("\tdata_num:%d\n", des_cmd_cur->data_num);
	pr_info("\tcmd_index:%d\n", des_cmd_cur->cmd_index);
	pr_info("\tcmd_arg:0x%x\n", desc_info->cmd_arg);
	pr_info("\tdata_addr:0x%x\n", desc_info->data_addr);
	pr_info("\tresp_addr:0x%x\n", desc_info->resp_addr);

}

static void meson_mmc_start_cmd(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;
	struct sd_emmc_desc_info *desc_cur;
	struct cmd_cfg *des_cmd_cur = NULL;
	u32 conf_flag = 0;
	u32 vconf;
	struct sd_emmc_config *pconf = (struct sd_emmc_config *)&vconf;
	u32 vstart;
	struct sd_emmc_start *desc_start = (struct sd_emmc_start *)&vstart;
#ifdef AML_CALIBRATION
	u32 adjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
#endif
	u32 desc_cnt = 0;
#ifdef SD_EMMC_REQ_DMA_SGMAP
	u32 sg_len = 0;
#else
	u32 cfg;
	u8 blk_len;
	unsigned int xfer_bytes = 0;
#endif

	/* Stop execution */
	vstart = readl(host->base + SD_EMMC_START);
	desc_start->busy = 0;
	writel(vstart, host->base + SD_EMMC_START);

	/* Setup descriptors */
	desc_cur = (struct sd_emmc_desc_info *)host->desc_buf;
	desc_cnt++;

	memset(desc_cur, 0, sizeof(struct sd_emmc_desc_info));

	sd_emmc_dbg(AMLSD_DBG_REQ, "%s %d cmd:%d, flags:0x%x, args:0x%x\n",
			__func__, __LINE__,	mrq->cmd->opcode,
			mrq->cmd->flags, mrq->cmd->arg);

	vconf = readl(host->base + SD_EMMC_CFG);
	/*sd/sdio switch volatile cmd11  need clock 6ms base on sd spec. */
	if (mrq->cmd->opcode == SD_SWITCH_VOLTAGE) {
		conf_flag |= 1 << 0;
		pconf->auto_clk = 0;
		host->sd_sdio_switch_volat_done = 0;
	}
	if ((pconf->auto_clk) && (pdata->auto_clk_close)) {
		conf_flag |= 1 << 1;
		pconf->auto_clk = 0;
	}

	/*check package size*/
	if (mrq->cmd->data) {
		if (pconf->bl_len != log2i(mrq->data->blksz)) {
			conf_flag |= 1 << 3;
			pconf->bl_len = log2i(mrq->data->blksz);
		}
	}
	if (conf_flag)
		writel(vconf, host->base + SD_EMMC_CFG);

	/*Add external CMD23 for multi-block operation*/
#ifdef SD_EMMC_MANUAL_CMD23
	if (((mrq->cmd->opcode == MMC_READ_MULTIPLE_BLOCK)
		|| (mrq->cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK))
		&& (mrq->cmd->data) && (mrq->sbc)) {
		des_cmd_cur = (struct cmd_cfg *)&(desc_cur->cmd_info);
		/*Command Index*/
		des_cmd_cur->cmd_index = MMC_SET_BLOCK_COUNT;
		des_cmd_cur->no_resp = 0;
		des_cmd_cur->r1b = 0;
		des_cmd_cur->data_io = 0;
		/* 10mS for only cmd timeout */
		des_cmd_cur->timeout = 0xc;
		des_cmd_cur->owner = 1;
		des_cmd_cur->end_of_chain = 0;

		desc_cur->cmd_arg = mrq->sbc->arg;
		/* response save into resp_addr itself */
		des_cmd_cur->resp_num = 1;
		desc_cur->resp_addr = 0;
		desc_cnt++;
		desc_cur++;
		memset(desc_cur, 0, sizeof(struct sd_emmc_desc_info));

	}
#endif

	des_cmd_cur = (struct cmd_cfg *)&(desc_cur->cmd_info);
	des_cmd_cur->cmd_index = mrq->cmd->opcode;
	des_cmd_cur->error = 0;
	des_cmd_cur->owner = 1;
	des_cmd_cur->end_of_chain = 0;

	/* Response */
	if (mrq->cmd->flags & MMC_RSP_PRESENT) {
		des_cmd_cur->no_resp = 0;
		if (mrq->cmd->flags & MMC_RSP_136) {
			/* response save into sram*/
			des_cmd_cur->resp_128 = 1;
			des_cmd_cur->resp_num = 0;
			desc_cur->resp_addr = 1;
		} else {
			/* response save into resp_addr itself */
			des_cmd_cur->resp_num = 1;
			desc_cur->resp_addr = 0;
		}

		if (mrq->cmd->flags & MMC_RSP_BUSY)
			des_cmd_cur->r1b = 1;

		if (!(mrq->cmd->flags & MMC_RSP_CRC))
			des_cmd_cur->resp_nocrc = 1;
	} else
		des_cmd_cur->no_resp = 1;

	desc_cur->cmd_arg = mrq->cmd->arg;
	/* data? */
	if (mrq->cmd->data) {
#ifdef SD_EMMC_REQ_DMA_SGMAP
		des_cmd_cur->timeout = 0xc;
		sg_len = aml_sd_emmc_pre_dma(host, mrq, desc_cur);
		WARN_ON(sg_len == 0);
		desc_cnt += (sg_len - 1);
		desc_cur += (sg_len - 1); /* last desc here */
#else
		desc_cur->cmd_info |= CMD_CFG_DATA_IO;
		if (mrq->cmd->data->blocks > 1) {
			desc_cur->cmd_info |= CMD_CFG_BLOCK_MODE;
			desc_cur->cmd_info |=
				(mrq->cmd->data->blocks & CMD_CFG_LENGTH_MASK)
				<< CMD_CFG_LENGTH_SHIFT;

			/* check if block-size matches, if not update */
			cfg = readl(host->base + SD_EMMC_CFG);
			blk_len = cfg & (CFG_BLK_LEN_MASK << CFG_BLK_LEN_SHIFT);
			blk_len >>= CFG_BLK_LEN_SHIFT;
			if (blk_len != ilog2(mrq->cmd->data->blksz)) {
				dev_warn(host->dev, "%s: update blk_len %d -> %d\n",
					__func__, blk_len,
					 ilog2(mrq->cmd->data->blksz));
				blk_len = ilog2(mrq->cmd->data->blksz);
				cfg &= ~(CFG_BLK_LEN_MASK << CFG_BLK_LEN_SHIFT);
				cfg |= blk_len << CFG_BLK_LEN_SHIFT;
				writel(cfg, host->base + SD_EMMC_CFG);
			}
		} else {
			desc_cur->cmd_info &= ~CMD_CFG_BLOCK_MODE;
			desc_cur->cmd_info |=
				(mrq->cmd->data->blksz & CMD_CFG_LENGTH_MASK)
				<< CMD_CFG_LENGTH_SHIFT;
		}

		mrq->cmd->data->bytes_xfered = 0;
		xfer_bytes = mrq->cmd->data->blksz * mrq->cmd->data->blocks;
		if (mrq->cmd->data->flags & MMC_DATA_WRITE) {
			desc_cur->cmd_info |= CMD_CFG_DATA_WR;
			WARN_ON(xfer_bytes > SD_EMMC_BOUNCE_REQ_SIZE);
			sg_copy_to_buffer(mrq->cmd->data->sg,
					mrq->cmd->data->sg_len,
					host->bn_buf, xfer_bytes);
			mrq->cmd->data->bytes_xfered = xfer_bytes;
			dma_wmb();
		} else {
			desc_cur->cmd_info &= ~CMD_CFG_DATA_WR;
		}

		if (xfer_bytes > 0) {
			desc_cur->cmd_info &= ~CMD_CFG_DATA_NUM;
			desc_cur->data_addr = host->bn_dma_buf & CMD_DATA_MASK;
		} else {
			/* write data to data_addr */
			desc_cur->cmd_info |= CMD_CFG_DATA_NUM;
			desc_cur->data_addr = 0;
		}

		cmd_cfg_timeout = 12;
#endif

#ifdef SD_EMMC_MANUAL_CMD23
		if (((mrq->cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK)
			|| (mrq->cmd->opcode == MMC_READ_MULTIPLE_BLOCK))
			&& (!host->cmd_is_stop) && (!mrq->sbc)) {

			/* pr_info("Send stop command here\n"); */

			/* for stop command,
			 * add another descriptor for stop command
			 */
			desc_cnt++;
			desc_cur++;
			memset(desc_cur, 0, sizeof(struct sd_emmc_desc_info));

			des_cmd_cur = (struct cmd_cfg *)&(desc_cur->cmd_info);

			/*Command Index*/
			des_cmd_cur->cmd_index = MMC_STOP_TRANSMISSION;
			des_cmd_cur->no_resp = 0;
			des_cmd_cur->r1b = 1;
			des_cmd_cur->data_io = 0;
			/* 10mS for only cmd timeout */
			des_cmd_cur->timeout = 0xc;
			des_cmd_cur->owner = 1;

			/* response save into resp_addr itself */
			des_cmd_cur->resp_num = 1;
			desc_cur->resp_addr = 0;
		}
#endif
	} else {
		des_cmd_cur->data_io = 0;
		/* Current 10uS based. 2^10 = 10mS for only cmd timeout */
		des_cmd_cur->timeout = 0xa;
	}

	if (mrq->cmd->opcode == MMC_SEND_STATUS)
		des_cmd_cur->timeout = 0xb;
	if (mrq->cmd->opcode == MMC_ERASE)
		des_cmd_cur->timeout = 0xf;

	host->cmd = mrq->cmd;

	/* Last descriptor */
	des_cmd_cur = (struct cmd_cfg *)&(desc_cur->cmd_info);
	des_cmd_cur->end_of_chain = 1;
	writel(SD_EMMC_IRQ_ALL, host->base + SD_EMMC_STATUS);

	vstart = readl(host->base + SD_EMMC_START);
	desc_start->init = 0;
	desc_start->busy = 1;
	desc_start->addr = host->desc_dma_addr >> 2;

#if 0  /* debug */
	desc_cur = (struct sd_emmc_desc_info *)host->desc_buf;
	des_cmd_cur = (struct cmd_cfg *)&(desc_cur->cmd_info);

	aml_sd_emmc_mrq_print_info(mrq, desc_cnt);

	while (desc_cnt) {
		aml_sd_emmc_desc_print_info(desc_cur);
		desc_cur++;
		des_cmd_cur = (struct cmd_cfg *)&(desc_cur->cmd_info);
		desc_cnt--;
	}

#endif
	dma_rmb();
	wmb(); /* ensure descriptor is written before kicked */
#ifdef AML_CALIBRATION
	if ((mrq->cmd->opcode == 18) && (pdata->caling == 1)) {
		adjust = readl(host->base + SD_EMMC_ADJUST);
		gadjust->cali_rise = 0;
		gadjust->cali_sel = pdata->c_ctrl.line_x;
		gadjust->cali_enable = 1;
		writel(adjust, host->base + SD_EMMC_ADJUST);
	}
#endif
	writel(vstart, host->base + SD_EMMC_START);
}

static void meson_mmc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;
	unsigned long flags;

	WARN_ON(!mmc);
	WARN_ON(!mrq);

	if (aml_check_unsupport_cmd(mmc, mrq))
		return;

	sd_emmc_dbg(AMLSD_DBG_REQ, "%s: starting CMD%u arg %08x flags %08x\n",
			mmc_hostname(mmc), mrq->cmd->opcode,
			mrq->cmd->arg, mrq->cmd->flags);

	if ((pdata->is_in) && (mrq->cmd->opcode == 0))
		host->init_flag = 1;

	/*clear error flag if last command retried failed */
	if (host->error_flag & (1 << 30))
		host->error_flag = 0;

	/*clear pinmux & set pinmux*/
	if (pdata->xfer_pre)
		pdata->xfer_pre(mmc);

	spin_lock_irqsave(&host->mrq_lock, flags);
	host->mrq = mrq;
	host->opcode = mrq->cmd->opcode;

	meson_mmc_start_cmd(mmc, mrq);
	host->xfer_step = XFER_START;
	spin_unlock_irqrestore(&host->mrq_lock, flags);
}

int meson_mmc_read_resp(struct mmc_host *mmc, struct mmc_command *cmd)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct sd_emmc_desc_info *desc_info =
		(struct sd_emmc_desc_info *)host->desc_buf;
	struct cmd_cfg *des_cmd_cur = NULL;
	int i;

	for (i = 0; i < (SD_EMMC_MAX_DESC_MUN>>2); i++) {
		des_cmd_cur = (struct cmd_cfg *)&(desc_info->cmd_info);
		if (des_cmd_cur->cmd_index == cmd->opcode)
			break;
		desc_info++;
	}
	if (cmd->flags & MMC_RSP_136) {
		cmd->resp[0] = readl(host->base + SD_EMMC_CMD_RSP3);
		cmd->resp[1] = readl(host->base + SD_EMMC_CMD_RSP2);
		cmd->resp[2] = readl(host->base + SD_EMMC_CMD_RSP1);
		cmd->resp[3] = readl(host->base + SD_EMMC_CMD_RSP);
	} else if (cmd->flags & MMC_RSP_PRESENT) {
		cmd->resp[0] = desc_info->resp_addr;
	}

	return 0;
}

void aml_host_bus_fsm_show(struct amlsd_host *host, int fsm_val)
{
	switch (fsm_val) {
	case BUS_FSM_IDLE:
		sd_emmc_err("%s: err: idle, bus_fsm:0x%x\n",
				mmc_hostname(host->mmc), fsm_val);
		break;
	case BUS_FSM_SND_CMD:
		sd_emmc_err("%s: err: send cmd, bus_fsm:0x%x\n",
				mmc_hostname(host->mmc), fsm_val);
		break;
	case BUS_FSM_CMD_DONE:
		sd_emmc_err("%s: err: wait for cmd done, bus_fsm:0x%x\n",
				mmc_hostname(host->mmc), fsm_val);
		break;
	case BUS_FSM_RESP_START:
		sd_emmc_err("%s: err: resp start, bus_fsm:0x%x\n",
				mmc_hostname(host->mmc), fsm_val);
			break;
	case BUS_FSM_RESP_DONE:
		sd_emmc_err("%s: err: wait for resp done, bus_fsm:0x%x\n",
				mmc_hostname(host->mmc), fsm_val);
		break;
	case BUS_FSM_DATA_START:
		sd_emmc_err("%s: err: data start, bus_fsm:0x%x\n",
				mmc_hostname(host->mmc), fsm_val);
		break;
	case BUS_FSM_DATA_DONE:
		sd_emmc_err("%s: err: wait for data done, bus_fsm:0x%x\n",
				mmc_hostname(host->mmc), fsm_val);
		break;
	case BUS_FSM_DESC_WRITE_BACK:
		sd_emmc_err("%s: err: wait for desc write back, bus_fsm:0x%x\n",
				mmc_hostname(host->mmc), fsm_val);
		break;
	case BUS_FSM_IRQ_SERVICE:
		sd_emmc_err("%s: err: wait for irq service, bus_fsm:0x%x\n",
				mmc_hostname(host->mmc), fsm_val);
		break;
	default:
		sd_emmc_err("%s: err: unknown err, bus_fsm:0x%x\n",
				mmc_hostname(host->mmc), fsm_val);
		break;
	}
}

void mmc_cmd_LBA_show(struct mmc_host *mmc, struct mmc_request *mrq)
{
	int i;
	uint64_t offset, size;
	struct partitions *pp;

	if (!pt_fmt || !mrq->cmd->arg) /* no disk or no arg, nothing to do */
		return;

	for (i = 0; i < pt_fmt->part_num; i++) {
		pp = &(pt_fmt->partitions[i]);
		offset = pp->offset >> 9; /* unit:512 bytes */
		size = pp->size >> 9; /* unit:512 bytes */

		if ((mrq->cmd->arg >= offset)
				&& (mrq->cmd->arg < (offset + size))) {
			sd_emmc_err("%s: cmd %d, arg 0x%x, operation is in [%s] disk!\n",
				mmc_hostname(mmc),
				mrq->cmd->opcode, mrq->cmd->arg, pp->name);
			break;
		}
	}
	if (i == pt_fmt->part_num)
		sd_emmc_err("%s: cmd %d, arg 0x%x, operation is in [unknown] disk!\n",
			mmc_hostname(mmc),
			mrq->cmd->opcode, mrq->cmd->arg);
}

static irqreturn_t meson_mmc_irq(int irq, void *dev_id)
{
	struct amlsd_host *host = dev_id;
	struct mmc_request *mrq;
	unsigned long flags;
	struct amlsd_platform *pdata = host->pdata;
	struct mmc_host *mmc;
	u32 vstat = 0;
	u32 virqc = 0;
	u32 vstart = 0;
	u32 err = 0;

	struct sd_emmc_irq_en *irqc = (struct sd_emmc_irq_en *)&virqc;
	struct sd_emmc_status *ista = (struct sd_emmc_status *)&vstat;
	struct sd_emmc_start *desc_start = (struct sd_emmc_start *)&vstart;

	if (WARN_ON(!host))
		return IRQ_NONE;

	virqc = readl(host->base + SD_EMMC_IRQ_EN) & 0xffff;
	vstat = readl(host->base + SD_EMMC_STATUS) & 0xffffffff;
	host->ista = vstat;

	sd_emmc_dbg(AMLSD_DBG_REQ, "%s %d occurred, vstat:0x%x\n",
			__func__, __LINE__, vstat);

	if (irqc->irq_sdio && ista->irq_sdio) {
		if ((host->mmc->sdio_irq_thread)
			&& (!atomic_read(&host->mmc->sdio_irq_thread_abort))) {
			mmc_signal_sdio_irq(host->mmc);
			if (!(vstat & 0x3fff))
				return IRQ_HANDLED;
		}
	} else if (!(vstat & 0x3fff))
		return IRQ_HANDLED;

	spin_lock_irqsave(&host->mrq_lock, flags);
	mrq = host->mrq;
	mmc = host->mmc;
	vstart = readl(host->base + SD_EMMC_START);
	if ((desc_start->busy == 1)
		&& (aml_card_type_mmc(pdata) ||
			(aml_card_type_non_sdio(pdata)))) {
		desc_start->busy = 0;
		writel(vstart, host->base + SD_EMMC_START);
	}
	if (!mmc) {
		pr_info("sd_emmc_regs: irq_en = 0x%x at line %d\n",
			readl(host->base + SD_EMMC_IRQ_EN), __LINE__);
		pr_info("sd_emmc_regs: status = 0x%x at line %d\n",
			readl(host->base + SD_EMMC_STATUS), __LINE__);
		pr_info("sd_emmc_regs: cfg = 0x%x at line %d\n",
			readl(host->base + SD_EMMC_CFG), __LINE__);
		pr_info("sd_emmc_regs: clock = 0x%x at line %d\n",
			readl(host->base + SD_EMMC_CLOCK), __LINE__);
	}

#ifdef CHOICE_DEBUG
	pr_info("%s %d cmd:%d arg:0x%x ",
		__func__, __LINE__, mrq->cmd->opcode, mrq->cmd->arg);
	if (mrq->cmd->data)
		pr_info("blksz:%d, blocks:%d\n",
			mrq->data->blksz, mrq->data->blocks);
#endif

	if (!mrq && !irqc->irq_sdio) {
		if (!ista->irq_sdio) {
			sd_emmc_err("NULL mrq in aml_sd_emmc_irq step %d",
				host->xfer_step);
			sd_emmc_err("status:0x%x,irq_c:0x%0x\n",
					readl(host->base + SD_EMMC_STATUS),
					readl(host->base + SD_EMMC_IRQ_EN));
		}
		if (host->xfer_step == XFER_FINISHED ||
			host->xfer_step == XFER_TIMER_TIMEOUT){
			spin_unlock_irqrestore(&host->mrq_lock, flags);
			return IRQ_HANDLED;
		}
#ifdef CHOICE_DEBUG
/*	aml_sd_emmc_print_reg(host);*/
#endif
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		return IRQ_HANDLED;
	}
#ifdef CHOICE_DEBUG
	if ((host->xfer_step != XFER_AFTER_START)
		&& (!host->cmd_is_stop) && !irqc->irq_sdio) {
		sd_emmc_err("%s: host->xfer_step=%d\n",
			mmc_hostname(mmc), host->xfer_step);
		pr_info("%%sd_emmc_regs: irq_en = 0x%x at line %d\n",
			readl(host->base + SD_EMMC_IRQ_EN), __LINE__);
		pr_info("%%sd_emmc_regs: status = 0x%x at line %d\n",
			readl(host->base + SD_EMMC_STATUS), __LINE__);
		pr_info("%%sd_emmc_regs: cfg = 0x%x at line %d\n",
			readl(host->base + SD_EMMC_CFG), __LINE__);
		pr_info("%%sd_emmc_regs: clock = 0x%x at line %d\n",
			readl(host->base + SD_EMMC_CLOCK), __LINE__);
	}
#endif
	if (mrq) {
		if (host->cmd_is_stop)
			host->xfer_step = XFER_IRQ_TASKLET_BUSY;
		else
			host->xfer_step = XFER_IRQ_OCCUR;
	}

	/* ack all (enabled) interrupts */
	writel(0x7fff, host->base + SD_EMMC_STATUS);
	spin_unlock_irqrestore(&host->mrq_lock, flags);

	if (ista->end_of_chain || ista->desc_irq) {
		if (mrq->data)
			host->status = HOST_TASKLET_DATA;
		else
			host->status = HOST_TASKLET_CMD;
		mrq->cmd->error = 0;
	}

	if ((vstat & 0x1FFF) && (!host->cmd_is_stop))
		err = 1;

	if (ista->rxd_err || ista->txd_err) {
		host->status = HOST_DAT_CRC_ERR;
		mrq->cmd->error = -EILSEQ;
		if (host->is_tunning == 0) {
			sd_emmc_err("%s: warning... data crc, vstat:0x%x, virqc:%x",
					mmc_hostname(host->mmc),
					vstat, virqc);
			sd_emmc_err("@ cmd %d with %p; stop %d, status %d\n",
					mrq->cmd->opcode, mrq->data,
					host->cmd_is_stop,
					host->status);
		}
	} else if (ista->desc_err) {
		if (host->is_tunning == 0)
			sd_emmc_err("%s: warning... desc err,vstat:0x%x,virqc:%x\n",
					mmc_hostname(host->mmc),
					vstat, virqc);
		host->status = HOST_DAT_CRC_ERR;
		mrq->cmd->error = -EILSEQ;
	} else if (ista->resp_err) {
		if (host->is_tunning == 0)
			sd_emmc_err("%s: warning... response crc,vstat:0x%x,virqc:%x\n",
					mmc_hostname(host->mmc),
					vstat, virqc);
		host->status = HOST_RSP_CRC_ERR;
		mrq->cmd->error = -EILSEQ;
	} else if (ista->resp_timeout) {
		if (host->is_tunning == 0)
			sd_emmc_err("%s: resp_timeout,vstat:0x%x,virqc:%x\n",
					mmc_hostname(host->mmc),
					vstat, virqc);
		host->status = HOST_RSP_TIMEOUT_ERR;
		mrq->cmd->error = -ETIMEDOUT;
	} else if (ista->desc_timeout) {
		if (host->is_tunning == 0)
			sd_emmc_err("%s: desc_timeout,vstat:0x%x,virqc:%x\n",
					mmc_hostname(host->mmc),
					vstat, virqc);
		host->status = HOST_DAT_TIMEOUT_ERR;
		mrq->cmd->error = -ETIMEDOUT;
	}

	if (err) {
		if (host->is_tunning == 0) {
			aml_host_bus_fsm_show(host, ista->bus_fsm);
			if (aml_card_type_mmc(pdata))
				mmc_cmd_LBA_show(mmc, mrq);
		}
	}

	if (host->xfer_step == XFER_IRQ_UNKNOWN_IRQ)
		return IRQ_HANDLED;
	else
		return IRQ_WAKE_THREAD;
}

struct mmc_command aml_sd_emmc_cmd = {
	.opcode = MMC_STOP_TRANSMISSION,
	.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC,
};
struct mmc_request aml_sd_emmc_stop = {
	.cmd = &aml_sd_emmc_cmd,
};

void aml_sd_emmc_send_stop(struct amlsd_host *host)
{
	unsigned long flags;

	/*Already in mrq_lock*/
	spin_lock_irqsave(&host->mrq_lock, flags);
	host->error_bak = host->mrq->cmd->error;
	host->mrq->cmd->error = 0;
	host->cmd_is_stop = 1;
	meson_mmc_start_cmd(host->mmc, &aml_sd_emmc_stop);
	spin_unlock_irqrestore(&host->mrq_lock, flags);
}

static irqreturn_t meson_mmc_irq_thread(int irq, void *dev_id)
{
	struct amlsd_host *host = dev_id;
	struct amlsd_platform *pdata = host->pdata;
	unsigned long flags;
	struct mmc_request *mrq;

	u32 status, rx_phase, xfer_bytes = 0;
	enum aml_mmc_waitfor xfer_step;
	struct aml_emmc_adjust *emmc_adj = &host->emmc_adj;
	struct aml_emmc_rxclk *emmc_rxclk = &host->emmc_rxclk;
	u32 adjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	u32 vclk;
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&(vclk);

	spin_lock_irqsave(&host->mrq_lock, flags);
	mrq = host->mrq;
	xfer_step = host->xfer_step;
	status = host->status;

	if ((xfer_step == XFER_FINISHED) || (xfer_step == XFER_TIMER_TIMEOUT)) {
		sd_emmc_err("Warning: %s xfer_step=%d, host->status=%d\n",
			mmc_hostname(host->mmc), xfer_step, status);
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		return IRQ_HANDLED;
	}

	WARN_ON((host->xfer_step != XFER_IRQ_OCCUR)
		 && (host->xfer_step != XFER_IRQ_TASKLET_BUSY));

	if (!mrq) {
		sd_emmc_err("%s: !mrq xfer_step %d\n",
			mmc_hostname(host->mmc), xfer_step);
		if (xfer_step == XFER_FINISHED ||
			xfer_step == XFER_TIMER_TIMEOUT){
			spin_unlock_irqrestore(&host->mrq_lock, flags);
			return IRQ_HANDLED;
		}
/*		aml_sd_emmc_print_err(host);*/
	}

	if (host->cmd_is_stop) {
		/* --new irq enter, */
		host->cmd_is_stop = 0;
		mrq->cmd->error = host->error_bak;
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		meson_mmc_request_done(host->mmc, host->mrq);

		return IRQ_HANDLED;
	}
	spin_unlock_irqrestore(&host->mrq_lock, flags);

	WARN_ON(!host->mrq->cmd);

	switch (status) {
	case HOST_TASKLET_DATA:
	case HOST_TASKLET_CMD:
		/* WARN_ON(aml_sd_emmc_desc_check(host)); */
		sd_emmc_dbg(AMLSD_DBG_REQ, "%s %d cmd:%d\n",
			__func__, __LINE__, mrq->cmd->opcode);
		host->error_flag = 0;
		if (mrq->cmd->data &&  mrq->cmd->opcode) {
			xfer_bytes = mrq->data->blksz*mrq->data->blocks;
			/* copy buffer from dma to data->sg in read cmd*/
#ifdef SD_EMMC_REQ_DMA_SGMAP
			WARN_ON(aml_sd_emmc_post_dma(host, mrq));
#else
			if (host->mrq->data->flags & MMC_DATA_READ) {
				WARN_ON(xfer_bytes > SD_EMMC_BOUNCE_REQ_SIZE);
				sg_copy_from_buffer(mrq->data->sg,
						mrq->data->sg_len,
						host->bn_buf, xfer_bytes);
			}
		}

#endif
			mrq->data->bytes_xfered = xfer_bytes;
			host->xfer_step = XFER_TASKLET_DATA;
		} else {
			host->xfer_step = XFER_TASKLET_CMD;
		}
		spin_lock_irqsave(&host->mrq_lock, flags);
		mrq->cmd->error = 0;
		spin_unlock_irqrestore(&host->mrq_lock, flags);

		/* check ready?? */
		/*Wait command busy*/
		meson_mmc_read_resp(host->mmc, mrq->cmd);
		meson_mmc_request_done(host->mmc, mrq);

		break;

	case HOST_RSP_TIMEOUT_ERR:
	case HOST_DAT_TIMEOUT_ERR:
	case HOST_RSP_CRC_ERR:
	case HOST_DAT_CRC_ERR:
		if (host->is_tunning == 0)
			pr_info("%s %d %s: cmd:%d\n", __func__, __LINE__,
				mmc_hostname(host->mmc), mrq->cmd->opcode);
		if (mrq->cmd->data) {
			dma_unmap_sg(mmc_dev(host->mmc), mrq->cmd->data->sg,
				mrq->cmd->data->sg_len,
				(mrq->cmd->data->flags & MMC_DATA_READ) ?
					DMA_FROM_DEVICE : DMA_TO_DEVICE);
		}
		meson_mmc_read_resp(host->mmc, mrq->cmd);

		/* set retry @ 1st error happens! */
		if ((host->error_flag == 0)
			&& (aml_card_type_mmc(pdata)
				|| aml_card_type_non_sdio(pdata))
			&& (host->is_tunning == 0)) {

			sd_emmc_err("%s() %d: set 1st retry!\n",
				__func__, __LINE__);
			host->error_flag |= (1<<0);
			spin_lock_irqsave(&host->mrq_lock, flags);
			mrq->cmd->retries = AML_ERROR_RETRY_COUNTER;
			spin_unlock_irqrestore(&host->mrq_lock, flags);
		}

		if (aml_card_type_non_sdio(pdata)
			&& (host->error_flag & (1<<0))
			&& mrq->cmd->retries
		/*	&& host->mmc->uhs_speed*/) {
			sd_emmc_err("retry cmd %d the %d-th time(s)\n",
					mrq->cmd->opcode, mrq->cmd->retries);
			vclk = readl(host->base + SD_EMMC_CLOCK);
			rx_phase = clkc->rx_phase;
			rx_phase++;
			rx_phase %= 4;
			pr_err("sd, retry, rx_phase %d -> %d\n",
					clkc->rx_phase, rx_phase);
			clkc->rx_phase = rx_phase;
			writel(vclk, host->base + SD_EMMC_CLOCK);
			pdata->clkc = vclk;
		}

		if (aml_card_type_mmc(pdata) &&
			(host->error_flag & (1<<0)) && mrq->cmd->retries) {
			sd_emmc_err("retry cmd %d the %d-th time(s)\n",
				mrq->cmd->opcode, mrq->cmd->retries);
			/* change configs on current host */
			switch (host->tuning_mode) {
			case AUTO_TUNING_MODE:
				if ((status == HOST_RSP_TIMEOUT_ERR)
					|| (status == HOST_RSP_CRC_ERR)) {
					adjust = readl(host->base
							+ SD_EMMC_ADJUST);
					if (gadjust->cmd_delay <= 13)
						gadjust->cmd_delay += 2;
					else if (gadjust->cmd_delay % 2)
						gadjust->cmd_delay = 0;
					else
						gadjust->cmd_delay = 1;
					writel(adjust, host->base
							+ SD_EMMC_ADJUST);
					sd_emmc_err("cmd_delay change to %d\n",
							gadjust->cmd_delay);
				}
				break;
			case ADJ_TUNING_MODE:
				adjust = readl(host->base + SD_EMMC_ADJUST);
				emmc_adj->adj_point++;
				emmc_adj->adj_point	%= emmc_adj->clk_div;
				pr_err("emmc, %d retry, adj %d -> %d\n",
						mrq->cmd->retries,
						gadjust->adj_delay,
						emmc_adj->adj_point);

				/*set new adjust!*/
				gadjust->adj_delay = emmc_adj->adj_point;
				gadjust->adj_enable = 1;
				writel(adjust, host->base + SD_EMMC_ADJUST);
				break;
			case RX_PHASE_DELAY_TUNING_MODE:
				vclk = readl(host->base + SD_EMMC_CLOCK);
				emmc_rxclk->rxclk_point++;
				emmc_rxclk->rxclk_point %= 25;
				if (emmc_rxclk->rxclk_point < 10) {
					emmc_rxclk->rxclk_rx_phase = 0;
					emmc_rxclk->rxclk_rx_delay
						= emmc_rxclk->rxclk_point;
				} else {
					emmc_rxclk->rxclk_rx_phase = 2;
					emmc_rxclk->rxclk_rx_delay
					= emmc_rxclk->rxclk_point - 10;
				}
				pr_err("emmc, %d retry, rx_phase %d -> %d, rx_delay %d -> %d\n",
						mrq->cmd->retries,
						clkc->rx_phase,
						emmc_rxclk->rxclk_rx_phase,
						clkc->rx_delay,
						emmc_rxclk->rxclk_rx_delay);
				clkc->rx_phase = emmc_rxclk->rxclk_rx_phase;
				clkc->rx_delay = emmc_rxclk->rxclk_rx_delay;
				writel(vclk, host->base + SD_EMMC_CLOCK);
				pdata->clkc = vclk;
				break;
			case NONE_TUNING:
			default:
				vclk = readl(host->base + SD_EMMC_CLOCK);
				rx_phase = clkc->rx_phase;
				rx_phase++;
				rx_phase %= 4;
				pr_err("emmc: retry, rx_phase %d -> %d\n",
						clkc->rx_phase, rx_phase);
				clkc->rx_phase = rx_phase;
				writel(vclk, host->base + SD_EMMC_CLOCK);
				pdata->clkc = vclk;
				break;
			}
		}
		/* last retry effort! */
		if ((aml_card_type_mmc(pdata) || aml_card_type_non_sdio(pdata))
			&& host->error_flag && (mrq->cmd->retries == 0)) {
			host->error_flag |= (1<<30);
			sd_emmc_err("Command retried failed line:%d, cmd:%d\n",
				__LINE__, mrq->cmd->opcode);
		}
		/* retry need send a stop 2 emmc... */
		/* do not send stop for sdio wifi case */
		if (host->mrq->stop
			&& (aml_card_type_mmc(pdata)
				|| aml_card_type_non_sdio(pdata))
			&& pdata->is_in
			&& (host->mrq->cmd->opcode != MMC_SEND_TUNING_BLOCK)
			&& (host->mrq->cmd->opcode !=
					MMC_SEND_TUNING_BLOCK_HS200))
			aml_sd_emmc_send_stop(host);
		else
			meson_mmc_request_done(host->mmc, host->mrq);
		break;

	default:
		sd_emmc_err("BUG %s: xfer_step=%d, host->status=%d\n",
			mmc_hostname(host->mmc),  xfer_step, status);
/*		aml_sd_emmc_print_err(host);*/
	}

	return IRQ_HANDLED;
}

static void aml_sd_emmc_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;
	unsigned long flags;
	/* u32 vstat = 0; */
	u32 vclkc = 0;
	struct sd_emmc_clock *pclock = (struct sd_emmc_clock *)&vclkc;
	u32 vconf = 0;
	struct sd_emmc_config *pconf = (struct sd_emmc_config *)&vconf;
	u32 virqc = 0;
	struct sd_emmc_irq_en *irqc = (struct sd_emmc_irq_en *)&virqc;

	host->sdio_irqen = enable;
	spin_lock_irqsave(&host->mrq_lock, flags);
	vclkc = readl(host->base + SD_EMMC_CLOCK);
	vconf = readl(host->base + SD_EMMC_CFG);
	virqc = readl(host->base + SD_EMMC_IRQ_EN);

	pclock->irq_sdio_sleep = 1;
	pclock->irq_sdio_sleep_ds = 0;
	pconf->irq_ds = 0;

	/* vstat = sd_emmc_regs->gstatus&SD_EMMC_IRQ_ALL; */
	if (enable)
		irqc->irq_sdio = 1;
	else
		irqc->irq_sdio = 0;

	writel(virqc, host->base + SD_EMMC_IRQ_EN);
	writel(vclkc, host->base + SD_EMMC_CLOCK);
	writel(vconf, host->base + SD_EMMC_CFG);
	pdata->clkc = vclkc;

	spin_unlock_irqrestore(&host->mrq_lock, flags);

	/* check if irq already occurred */
	aml_sd_emmc_check_sdio_irq(host);
}

/*get readonly: 0 for rw, 1 for ro*/
static int aml_sd_emmc_get_ro(struct mmc_host *mmc)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;
	u32 ro = 0;

	if (pdata->ro)
		ro = pdata->ro(pdata);
	return ro;
}

/*
 * NOTE: we only need this until the GPIO/pinctrl driver can handle
 * interrupts.  For now, the MMC core will use this for polling.
 */
static int meson_mmc_get_cd(struct mmc_host *mmc)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;

	return pdata->is_in;
}

int aml_signal_voltage_switch(struct mmc_host *mmc, struct mmc_ios *ios)
{
	return aml_sd_voltage_switch(mmc, ios->signal_voltage);
}

/* Check if the card is pulling dat[0:3] low */
static int aml_sd_emmc_card_busy(struct mmc_host *mmc)
{
	struct amlsd_host *host = mmc_priv(mmc);
	struct amlsd_platform *pdata = host->pdata;
	unsigned int status = 0;
	/* only check data3_0 gpio level?? */
	u32 vstat;
	struct sd_emmc_status *ista = (struct sd_emmc_status *)&vstat;
	u32 vconf;
	struct sd_emmc_config *pconf = (struct sd_emmc_config *)&vconf;

	vstat = readl(host->base + SD_EMMC_STATUS);
	status = ista->dat_i & 0xf;

	/*must open auto_clk after sd/sdio switch volatile base on sd spec.*/
	if ((!aml_card_type_mmc(pdata))
			&& (host->sd_sdio_switch_volat_done)) {
		vconf = readl(host->base + SD_EMMC_CFG);
		pconf->auto_clk = 1;
		writel(vconf, host->base + SD_EMMC_CFG);
	}
	return !status;
}

/*this function tells wifi is using sd(sdiob) or sdio(sdioa)*/
const char *get_wifi_inf(void)
{
	if (sdio_host != NULL)
		return mmc_hostname(sdio_host);
	else
		return "sdio";

}
EXPORT_SYMBOL(get_wifi_inf);

static const struct mmc_host_ops meson_mmc_ops = {
	.request = meson_mmc_request,
	.set_ios = meson_mmc_set_ios,
	.enable_sdio_irq = aml_sd_emmc_enable_sdio_irq,
	.get_cd = meson_mmc_get_cd,
	.get_ro = aml_sd_emmc_get_ro,
	.start_signal_voltage_switch = aml_signal_voltage_switch,
	.card_busy = aml_sd_emmc_card_busy,
	.execute_tuning = aml_mmc_execute_tuning,
	.hw_reset = aml_emmc_hw_reset,
};

static const struct mmc_host_ops meson_mmc_ops_v3 = {
	.request = meson_mmc_request,
	.set_ios = meson_mmc_set_ios_v3,
	.enable_sdio_irq = aml_sd_emmc_enable_sdio_irq,
	.get_cd = meson_mmc_get_cd,
	.get_ro = aml_sd_emmc_get_ro,
	.start_signal_voltage_switch = aml_signal_voltage_switch,
	.card_busy = aml_sd_emmc_card_busy,
	.execute_tuning = aml_mmc_execute_tuning_v3,
	.hw_reset = aml_emmc_hw_reset,
	.post_hs400_timming = aml_post_hs400_timming,
};

static void aml_reg_print(struct amlsd_host *host)
{
	struct amlsd_platform *pdata = host->pdata;

	pr_info("%s reg val:\n", pdata->pinname);
	pr_info("SD_EMMC_CLOCK = 0x%x\n", readl(host->base + SD_EMMC_CLOCK));
	pr_info("SD_EMMC_CFG = 0x%x\n", readl(host->base + SD_EMMC_CFG));
	pr_info("SD_EMMC_STATUS = 0x%x\n", readl(host->base + SD_EMMC_STATUS));
	pr_info("SD_EMMC_IRQ_EN = 0x%x\n", readl(host->base + SD_EMMC_IRQ_EN));
};

static int meson_mmc_probe(struct platform_device *pdev)
{
	struct resource *res_mem;
	struct amlsd_host *host;
	struct amlsd_platform *pdata = NULL;
	struct mmc_host *mmc;
	int ret;

	aml_mmc_ver_msg_show();

	pdata = kzalloc(sizeof(struct amlsd_platform), GFP_KERNEL);
	if (!pdata)
		ret = -ENOMEM;

	mmc = mmc_alloc_host(sizeof(struct amlsd_host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;

	host = mmc_priv(mmc);
	host->mmc = mmc;
	host->pdev = pdev;
	host->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, host);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
		host->ctrl_ver = 3;
	host->pinmux_base = ioremap(0xc8834400, 0x200);
	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->base = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(host->base)) {
		ret = PTR_ERR(host->base);
		goto free_host;
	}

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq == 0) {
		dev_err(&pdev->dev, "failed to get interrupt resource.\n");
		ret = -EINVAL;
		goto free_host;
	}
	if (host->ctrl_ver >= 3)
		ret = devm_request_threaded_irq(&pdev->dev, host->irq,
			meson_mmc_irq, meson_mmc_irq_thread_v3,
			IRQF_SHARED, "meson-aml-mmc", host);
	else
		ret = devm_request_threaded_irq(&pdev->dev, host->irq,
			meson_mmc_irq, meson_mmc_irq_thread,
			IRQF_SHARED, "meson-aml-mmc", host);
	if (ret)
		goto free_host;

	host->blk_test = kzalloc((512 * CALI_BLK_CNT), GFP_KERNEL);
	if (host->blk_test == NULL) {
		ret = -ENOMEM;
		goto free_host;
	}
	/* data desc buffer */
	host->desc_buf =
		dma_alloc_coherent(host->dev,
				SD_EMMC_MAX_DESC_MUN
				* (sizeof(struct sd_emmc_desc_info)),
				&host->desc_dma_addr, GFP_KERNEL);
	if (host->desc_buf == NULL) {
		dev_err(host->dev, "Unable to map allocate DMA desc buffer.\n");
		ret = -ENOMEM;
		goto free_cali;
	}

	/* data bounce buffer */
	host->bn_buf =
		dma_alloc_coherent(host->dev, SD_EMMC_BOUNCE_REQ_SIZE,
				   &host->bn_dma_buf, GFP_KERNEL);
	if (host->bn_buf == NULL) {
		dev_err(host->dev, "Unable to map allocate DMA bounce buffer.\n");
		ret = -ENOMEM;
		goto free_cali;
	}

	host->pdata = pdata;
	spin_lock_init(&host->mrq_lock);
	mutex_init(&host->pinmux_lock);
	host->xfer_step = XFER_INIT;
	host->init_flag = 1;
	host->is_gated = false;

	if (host->ctrl_ver >= 3)
		ret = meson_mmc_clk_init_v3(host);
	else
		ret = meson_mmc_clk_init(host);
	if (ret)
		goto free_cali;

	ret = mmc_of_parse(mmc);
	if (ret) {
		dev_warn(&pdev->dev, "error parsing DT: %d\n", ret);
		goto free_cali;
	}

	if (amlsd_get_platform_data(pdev, pdata, mmc, 0))
		mmc_free_host(mmc);

	if (aml_card_type_mmc(pdata)
			&& (host->ctrl_ver < 3))
		/**set emmc tx_phase regs here base on dts**/
		aml_sd_emmc_tx_phase_set(host);

	dev_set_name(&mmc->class_dev, "%s", pdata->pinname);

	if (pdata->caps & MMC_PM_KEEP_POWER)
		mmc->pm_caps |= MMC_PM_KEEP_POWER;
	if (pdata->base != 0) {
		iounmap(host->pinmux_base);
		host->pinmux_base = ioremap(pdata->base, 0x200);
	}
	host->init_flag = 1;
	host->version = AML_MMC_VERSION;
	host->pinctrl = NULL;
	host->status = HOST_INVALID;
	host->is_tunning = 0;
	mmc->ios.clock = 400000;
	mmc->ios.bus_width = MMC_BUS_WIDTH_1;
	mmc->max_blk_count = 4095;
	mmc->max_blk_size = 4095;
	mmc->max_req_size = pdata->max_req_size;
	mmc->max_seg_size = mmc->max_req_size;
	mmc->max_segs = 1024;
	mmc->ocr_avail = pdata->ocr_avail;
	mmc->caps = pdata->caps;
	mmc->caps2 = pdata->caps2;
	mmc->f_min = pdata->f_min;
	mmc->f_max = pdata->f_max;
	mmc->max_current_180 = 300; /* 300 mA in 1.8V */
	mmc->max_current_330 = 300; /* 300 mA in 3.3V */

	if (mmc->caps & MMC_CAP_NONREMOVABLE)
		pdata->is_in = 1;

	if (aml_card_type_sdio(pdata)) { /* if sdio_wifi */
		/*	mmc->host_rescan_disable = true;*/
		/* do NOT run mmc_rescan for the first time */
		mmc->rescan_entered = 1;
	} else {
	/*	mmc->host_rescan_disable = false;*/
		mmc->rescan_entered = 0;
	}

	if (aml_card_type_mmc(pdata)) {
		/* Poll down BOOT_15 in case hardward not pull down */
		u32 boot_poll_en, boot_poll_down;

		boot_poll_down = readl(host->pinmux_base + BOOT_POLL_UP_DOWN);
		boot_poll_down &= (~(1 << 15));
		boot_poll_en = readl(host->pinmux_base + BOOT_POLL_UP_DOWN_EN);
		boot_poll_en |= (1 << 15);
		writel(boot_poll_down, host->pinmux_base + BOOT_POLL_UP_DOWN);
		writel(boot_poll_en, host->pinmux_base + BOOT_POLL_UP_DOWN_EN);
	}

	if (pdata->port_init)
		pdata->port_init(pdata);
	if (host->ctrl_ver >= 3)
		mmc->ops = &meson_mmc_ops_v3;
	else
		mmc->ops = &meson_mmc_ops;
	aml_reg_print(host);
	ret = mmc_add_host(mmc);
	if (ret) { /* error */
		sd_emmc_err("Failed to add mmc host.\n");
		goto free_cali;
	}
	if (aml_card_type_sdio(pdata)) /* if sdio_wifi */
		sdio_host = mmc;

	/*Register card detect irq : plug in & unplug*/
	if (aml_card_type_non_sdio(pdata)) {
		pdata->irq_init(pdata);
		mutex_init(&pdata->in_out_lock);
		ret = devm_request_threaded_irq(&pdev->dev, pdata->irq_cd,
				aml_sd_irq_cd, aml_irq_cd_thread,
				IRQF_TRIGGER_RISING
				| IRQF_TRIGGER_FALLING
				| IRQF_ONESHOT,
				"amlsd_cd", host);
		if (ret) {
			sd_emmc_err("Failed to request SD IN detect\n");
			goto free_cali;
		}
	}
	pr_info("%s() : success!\n", __func__);
	return 0;

free_cali:
	kfree(host->blk_test);
free_host:
	mmc_free_host(mmc);
	kfree(pdata);
	pr_err("%s() fail!\n", __func__);
	return ret;
}

static int meson_mmc_remove(struct platform_device *pdev)
{
	struct amlsd_host *host = dev_get_drvdata(&pdev->dev);
	struct amlsd_platform *pdata = host->pdata;

	if (WARN_ON(!host))
		return 0;

	if (host->bn_buf)
		dma_free_coherent(host->dev, SD_EMMC_BOUNCE_REQ_SIZE,
				host->bn_buf, host->bn_dma_buf);

	devm_free_irq(&pdev->dev, host->irq, host);
	iounmap(host->pinmux_base);

	if (host->cfg_div_clk)
		clk_disable_unprepare(host->cfg_div_clk);
	if (host->core_clk)
		clk_disable_unprepare(host->core_clk);

	kfree(host->blk_test);
	mmc_free_host(host->mmc);
	kfree(pdata);
	return 0;
}

static const struct of_device_id meson_mmc_of_match[] = {
	{
		.compatible = "amlogic, meson-aml-mmc",
	},
	{}
};
MODULE_DEVICE_TABLE(of, meson_mmc_of_match);

static struct platform_driver meson_mmc_driver = {
	.probe		= meson_mmc_probe,
	.remove		= meson_mmc_remove,
	.driver		= {
		.name = "meson-aml-mmc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(meson_mmc_of_match),
	},
};

static int __init meson_mmc_init(void)
{
	return platform_driver_register(&meson_mmc_driver);
}

static void __exit meson_mmc_cleanup(void)
{
	platform_driver_unregister(&meson_mmc_driver);
}

module_init(meson_mmc_init);
module_exit(meson_mmc_cleanup);

MODULE_DESCRIPTION("Amlogic S912/GXM SD/eMMC driver");
MODULE_AUTHOR("Kevin Hilman <khilman@baylibre.com>");
MODULE_LICENSE("GPL");

