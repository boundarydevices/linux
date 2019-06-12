/*
 * include/linux/amlogic/aml_sd_emmc_v3.h
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

#ifndef __AML_SD_EMMC_V3_H__

#define __AML_SD_EMMC_V3_H__

int meson_mmc_clk_init_v3(struct amlsd_host *host);

void meson_mmc_set_ios_v3(struct mmc_host *mmc, struct mmc_ios *ios);

int aml_mmc_execute_tuning_v3(struct mmc_host *mmc, u32 opcode);

irqreturn_t meson_mmc_irq_thread_v3(int irq, void *dev_id);

int aml_post_hs400_timming(struct mmc_host *mmc);

extern ssize_t emmc_eyetest_show(struct device *dev,
		struct device_attribute *attr, char *buf);


extern ssize_t emmc_clktest_show(struct device *dev,
		struct device_attribute *attr, char *buf);


extern ssize_t emmc_scan_cmd_win(struct device *dev,
		struct device_attribute *attr, char *buf);

DEVICE_ATTR(emmc_eyetest, 0444, emmc_eyetest_show, NULL);
DEVICE_ATTR(emmc_clktest, 0444, emmc_clktest_show, NULL);
DEVICE_ATTR(emmc_cmd_window, 0444, emmc_scan_cmd_win, NULL);
#endif
