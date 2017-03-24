/*
 * drivers/amlogic/media/video_processor/ppmgr/ppmgr_pri.h
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

#ifndef _PPMGR_PRI_INCLUDE__
#define _PPMGR_PRI_INCLUDE__

/* for ppmgr device op. */
extern int init_ppmgr_device(void);
extern int uninit_ppmgr_device(void);

/* for ppmgr device class op. */
extern struct class *init_ppmgr_cls(void);

/* for thread of ppmgr. */
extern int start_ppmgr_task(void);
extern void stop_ppmgr_task(void);

/* for ppmgr private member. */
extern void set_ppmgr_buf_info(unsigned int start, unsigned int size);
extern void get_ppmgr_buf_info(unsigned int *start, unsigned int *size);

/*  ppmgr buffer op. */
extern int ppmgr_buffer_init(int vout_mode);
extern int ppmgr_buffer_uninit(void);
extern void vf_ppmgr_reset(int type);
extern int ppmgr_register(void);

/* for thread of tb detect. */
extern int start_tb_task(void);
extern void stop_tb_task(void);
extern void get_tb_detect_status(void);

#endif /* _PPMGR_PRI_INCLUDE__ */
