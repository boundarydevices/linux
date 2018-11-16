/*
 * drivers/amlogic/media/vin/tvin/tvafe/tvafe.h
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

#ifndef _TVAFE_H
#define _TVAFE_H

/* Standard Linux Headers */
#include <linux/cdev.h>
#include <linux/timer.h>
#include <linux/mutex.h>

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "../tvin_global.h"
#include "../tvin_frontend.h"
#include "tvafe_general.h"   /* For Kernel used only */
#include "tvafe_cvd.h"       /* For Kernel used only */

/* ************************************************* */
/* *** macro definitions ********************************************* */
/* *********************************************************** */
#define TVAFE_VER "Ref.2019/03/18"

/* used to set the flag of tvafe_dev_s */
#define TVAFE_FLAG_DEV_OPENED 0x00000010
#define TVAFE_FLAG_DEV_STARTED 0x00000020
#define TVAFE_FLAG_DEV_SNOW_FLAG 0x00000040
#define TVAFE_POWERDOWN_IN_IDLE 0x00000080

/*used to flag port opend for avdetect config*/
#define TVAFE_PORT_AV1 0x1
#define TVAFE_PORT_AV2 0x2

/************************************************************ */
/* *** enum definitions ********************************************* */
/************************************************************ */

/************************************************************* */
/* *** structure definitions ********************************************* */
/************************************************************* */

/* tvafe module structure */
struct tvafe_info_s {
	struct tvin_parm_s parm;
	struct tvafe_cvd2_s cvd2;
	/*WSS INFO for av/atv*/
	enum tvin_aspect_ratio_e aspect_ratio;
	unsigned int aspect_ratio_cnt;
};

/* tvafe device structure */
struct tvafe_dev_s {
	int	index;
	dev_t devt;
	struct cdev cdev;
	struct device *dev;

	struct mutex afe_mutex;
	struct timer_list timer;

	struct tvin_frontend_s frontend;
	unsigned int flags;
	/* bit4: TVAFE_FLAG_DEV_OPENED */
	/* bit5: TVAFE_FLAG_DEV_STARTED */
	struct tvafe_pin_mux_s *pinmux;
	/* pin mux setting from board config */
	/* cvd2 memory */
	struct tvafe_cvd2_mem_s mem;

	struct tvafe_info_s tvafe;
	unsigned int cma_config_en;
	/*cma_config_flag:1:share with codec_mm;0:cma alone*/
	unsigned int cma_config_flag;
#ifdef CONFIG_CMA
	struct platform_device *this_pdev;
	struct page *venc_pages;
	unsigned int cma_mem_size;/* BYTE */
	unsigned int cma_mem_alloc;
#endif
	unsigned int frame_skip_enable;
	unsigned int sizeof_tvafe_dev_s;
};

bool tvafe_get_snow_cfg(void);
void tvafe_set_snow_cfg(bool cfg);

typedef int (*hook_func_t)(void);
extern void aml_fe_hook_cvd(hook_func_t atv_mode,
		hook_func_t cvd_hv_lock, hook_func_t get_fmt);
extern int tvafe_reg_read(unsigned int reg, unsigned int *val);
extern int tvafe_reg_write(unsigned int reg, unsigned int val);
extern int tvafe_vbi_reg_read(unsigned int reg, unsigned int *val);
extern int tvafe_vbi_reg_write(unsigned int reg, unsigned int val);
extern int tvafe_hiu_reg_read(unsigned int reg, unsigned int *val);
extern int tvafe_hiu_reg_write(unsigned int reg, unsigned int val);
extern int tvafe_device_create_file(struct device *dev);
extern void tvafe_remove_device_files(struct device *dev);

extern bool disableapi;
extern bool force_stable;

#endif  /* _TVAFE_H */

