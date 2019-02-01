/*
 * include/linux/amlogic/aml_sd_emmc_internal.h
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

#ifndef __AML_SD_EMMC_INTERNAL_H__
#define __AML_SD_EMMC_INTERNAL_H__

extern int aml_emmc_clktree_init(struct amlsd_host *host);

extern int meson_mmc_clk_init_v3(struct amlsd_host *host);

extern void meson_mmc_set_ios_v3(struct mmc_host *mmc, struct mmc_ios *ios);

extern void aml_sd_emmc_set_buswidth(struct amlsd_platform *pdata,
		u32 busw_ios);

extern int meson_mmc_request_done(struct mmc_host *mmc,
		struct mmc_request *mrq);

extern int meson_mmc_read_resp(struct mmc_host *mmc,
		struct mmc_command *cmd);

extern void aml_sd_emmc_send_stop(struct amlsd_host *host);

extern int aml_sd_emmc_post_dma(struct amlsd_host *host,
		struct mmc_request *mrq);

extern u32 aml_sd_emmc_tuning_transfer(struct mmc_host *mmc,
	u32 opcode, const u8 *blk_pattern, u8 *blk_test, u32 blksz);

void aml_mmc_clk_switch_off(struct amlsd_platform *pdata);

void aml_mmc_clk_switch(struct amlsd_host *host,
	int clk_div, int clk_src_sel);

#endif
