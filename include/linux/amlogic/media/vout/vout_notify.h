/*
 * include/linux/amlogic/media/vout/vout_notify.h
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

#ifndef _VOUT_NOTIFY_H_
#define _VOUT_NOTIFY_H_

/* Linux Headers */
#include <linux/notifier.h>
#include <linux/list.h>
#include <linux/pm.h>

/* Local Headers */
#include "vinfo.h"

struct vframe_match_s {
	int fps;
	int frame_rate; /* *100 */
	unsigned int duration_num;
	unsigned int duration_den;
};

struct vout_op_s {
	struct vinfo_s *(*get_vinfo)(void);
	int (*set_vmode)(enum vmode_e);
	enum vmode_e (*validate_vmode)(char *);
	int (*vmode_is_supported)(enum vmode_e);
	int (*disable)(enum vmode_e);
	int (*set_state)(int);
	int (*clr_state)(int);
	int (*get_state)(void);
	int (*set_vframe_rate_hint)(int);
	int (*set_vframe_rate_end_hint)(void);
	int (*set_vframe_rate_policy)(int);
	int (*get_vframe_rate_policy)(void);
	void (*set_bist)(unsigned int);
	int (*vout_suspend)(void);
	int (*vout_resume)(void);
	int (*vout_shutdown)(void);
};

struct vout_server_s {
	struct list_head list;
	char *name;
	struct vout_op_s op;
};

struct vout_module_s {
	struct list_head vout_server_list;
	struct vout_server_s *curr_vout_server;
};

extern int vout_register_client(struct notifier_block *p);
extern int vout_unregister_client(struct notifier_block *p);
extern int vout_notifier_call_chain(unsigned int long, void *p);
extern int vout_register_server(struct vout_server_s *p);
extern int vout_unregister_server(struct vout_server_s *p);

extern struct vinfo_s *get_current_vinfo(void);
extern enum vmode_e get_current_vmode(void);
extern int set_vframe_rate_hint(int duration);
extern int set_vframe_rate_end_hint(void);
extern int set_vframe_rate_policy(int pol);
extern int get_vframe_rate_policy(void);
extern void set_vout_bist(unsigned int bist);

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
extern int vout2_register_client(struct notifier_block *p);
extern int vout2_unregister_client(struct notifier_block *p);
extern int vout2_notifier_call_chain(unsigned int long, void *p);
extern int vout2_register_server(struct vout_server_s *p);
extern int vout2_unregister_server(struct vout_server_s *p);

extern struct vinfo_s *get_current_vinfo2(void);
extern enum vmode_e get_current_vmode2(void);
extern int set_vframe2_rate_hint(int duration);
extern int set_vframe2_rate_end_hint(void);
extern int set_vframe2_rate_policy(int pol);
extern int get_vframe2_rate_policy(void);
extern void set_vout2_bist(unsigned int bist);

#endif

extern int vout_get_vsource_fps(int duration);

/* vdac ctrl,adc/dac ref signal,cvbs out signal
 * module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; dac:0x8
 */
extern void vdac_enable(bool on, unsigned int module_sel);


#define VOUT_EVENT_MODE_CHANGE_PRE     0x00010000
#define VOUT_EVENT_MODE_CHANGE         0x00020000
#define VOUT_EVENT_OSD_BLANK           0x00030000
#define VOUT_EVENT_OSD_DISP_AXIS       0x00040000
#define VOUT_EVENT_OSD_PREBLEND_ENABLE 0x00050000

/* ********** vout_ioctl ********** */
#define VOUT_IOC_TYPE            'C'
#define VOUT_IOC_NR_GET_VINFO    0x0
#define VOUT_IOC_NR_SET_VINFO    0x1

#define VOUT_IOC_CMD_GET_VINFO   \
		_IOR(VOUT_IOC_TYPE, VOUT_IOC_NR_GET_VINFO, struct vinfo_base_s)
#define VOUT_IOC_CMD_SET_VINFO   \
		_IOW(VOUT_IOC_TYPE, VOUT_IOC_NR_SET_VINFO, struct vinfo_base_s)
/* ******************************** */

extern char *get_vout_mode_internal(void);
extern char *get_vout_mode_uboot(void);

extern int set_vout_mode(char *name);
extern void set_vout_init(enum vmode_e mode);
extern void update_vout_viu(void);
extern int set_vout_vmode(enum vmode_e mode);
extern enum vmode_e validate_vmode(char *name);
extern int set_current_vmode(enum vmode_e mode);

#endif /* _VOUT_NOTIFY_H_ */
