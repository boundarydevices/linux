/*
 * Copyright (C) 2014-2015 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */
#ifndef __LINUX_IPU_V3_PRG_H_
#define __LINUX_IPU_V3_PRG_H_

#include <linux/ipu-v3.h>

#define PRG_SO_INTERLACE	1
#define PRG_SO_PROGRESSIVE	0
#define PRG_BLOCK_MODE		1
#define PRG_SCAN_MODE		0

struct ipu_prg_config {
	unsigned int id;
	unsigned int pre_num;
	ipu_channel_t ipu_ch;
	unsigned int stride;
	unsigned int height;
	unsigned int ipu_height;
	unsigned int crop_line;
	unsigned int so;
	unsigned int ilo;
	unsigned int block_mode;
	bool vflip;
	u32 baddr;
	u32 offset;
};

#ifdef CONFIG_MXC_IPU_V3_PRG
int ipu_prg_config(struct ipu_prg_config *config);
int ipu_prg_disable(unsigned int ipu_id, unsigned int pre_num);
int ipu_prg_wait_buf_ready(unsigned int ipu_id, unsigned int pre_num,
			   unsigned int hsk_line_num,
			   int pre_store_out_height);
#else
int ipu_prg_config(struct ipu_prg_config *config)
{
	return -ENODEV;
}

int ipu_prg_disable(unsigned int ipu_id, unsigned int pre_num)
{
	return -ENODEV;
}

int ipu_prg_wait_buf_ready(unsigned int ipu_id, unsigned int pre_num,
			   unsigned int hsk_line_num,
			   int pre_store_out_height)
{
	return -ENODEV;
}
#endif
#endif /* __LINUX_IPU_V3_PRG_H_ */
