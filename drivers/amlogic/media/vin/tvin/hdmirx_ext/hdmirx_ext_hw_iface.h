/*
 * drivers/amlogic/media/vin/tvin/hdmirx_ext/hdmirx_ext_hw_iface.h
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

#ifndef __HDMIRX_EXT_HW_INTERFACE_H__
#define __HDMIRX_EXT_HW_INTERFACE_H__

#include "hdmirx_ext_drv.h"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* signal stauts */
extern int __hw_get_cable_status(void);
extern int __hw_get_signal_status(void);
extern int __hw_get_input_port(void);
extern void __hw_set_input_port(int port);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* signal timming */
extern int __hw_get_video_timming(struct video_timming_s *ptimming);
extern int __hw_get_video_mode(void);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* hdmi/dvi mode */
extern int __hw_is_hdmi_mode(void);
extern int __hw_get_video_mode(void);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* audio mode
 * audio sampling frequency:
 * 0x0 for 44.1 KHz
 * 0x1 for Not indicated
 * 0x2 for 48 KHz
 * 0x3 for 32 KHz
 * 0x4 for 22.05 KHz
 * 0x6 for 24 kHz
 * 0x8 for 88.2 kHz
 * 0x9 for 768 kHz (192*4)
 * 0xa for 96 kHz
 * 0xc for 176.4 kHz
 * 0xe for 192 kHz
 */
extern int __hw_get_audio_sample_rate(void);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug interface */
extern int __hw_debug(char *buf);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* chip id and driver version */
extern char *__hw_get_chip_id(void);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* hardware init related */
extern int __hw_init(void);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* hardware enable related */
extern int __hw_enable(void);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* hardware disable related */
extern void __hw_disable(void);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* hdmirx_ext utility */
extern void __hw_dump_video_timming(void);

#endif

