/*
 * drivers/amlogic/cec/hdmi_ao_cec.h
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

#ifndef __AO_CEC_H__
#define __AO_CEC_H__

#include "ao_cec_reg.h"
#include "ee_cec_reg.h"

#define CEC_FRAME_DELAY		msecs_to_jiffies(400)
#define CEC_DEV_NAME		"cec"

#define CEC_EARLY_SUSPEND	(1 << 0)
#define CEC_DEEP_SUSPEND	(1 << 1)

#define HR_DELAY(n)		(ktime_set(0, n * 1000 * 1000))

#ifdef CONFIG_TVIN_HDMI
extern unsigned long hdmirx_rd_top(unsigned long addr);
extern void hdmirx_wr_top(unsigned long addr, unsigned long data);
extern uint32_t hdmirx_rd_dwc(uint16_t addr);
extern void hdmirx_wr_dwc(uint16_t addr, uint32_t data);
#else
static inline unsigned long hdmirx_rd_top(unsigned long addr)
{
	return 0;
}

static inline void hdmirx_wr_top(unsigned long addr, unsigned long data)
{
}

static inline uint32_t hdmirx_rd_dwc(uint16_t addr)
{
	return 0;
}
static inline void hdmirx_wr_dwc(uint16_t addr, uint32_t data)
{

}
#endif

#endif	/* __AO_CEC_H__ */
