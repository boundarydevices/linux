/*
 * drivers/amlogic/media/vout/vout_serve/vout_func.h
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

#ifndef _VOUT_FUNC_H_
#define _VOUT_FUNC_H_
#include <linux/cdev.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#define VOUTPR(fmt, args...)     pr_info("vout: "fmt"", ## args)
#define VOUTERR(fmt, args...)    pr_err("vout: error: "fmt"", ## args)

/* [3: 2] cntl_viu2_sel_venc:
 *         0=ENCL, 1=ENCI, 2=ENCP, 3=ENCT.
 * [1: 0] cntl_viu1_sel_venc:
 *         0=ENCL, 1=ENCI, 2=ENCP, 3=ENCT.
 */
#define VPU_VIU_VENC_MUX_CTRL                      0x271a
/* [2] Enci_afifo_clk: 0: cts_vpu_clk_tm 1: cts_vpu_clkc_tm
 * [1] Encl_afifo_clk: 0: cts_vpu_clk_tm 1: cts_vpu_clkc_tm
 * [0] Encp_afifo_clk: 0: cts_vpu_clk_tm 1: cts_vpu_clkc_tm
 */
#define VPU_VENCX_CLK_CTRL                         0x2785
#define VPP_POSTBLEND_H_SIZE                       0x1d21
#define VPP2_POSTBLEND_H_SIZE                      0x1921

struct vout_cdev_s {
	dev_t         devno;
	struct cdev   cdev;
	struct device *dev;
};

#ifdef CONFIG_AMLOGIC_HDMITX
extern int get_hpd_state(void);
#endif
extern int vout_get_hpd_state(void);
extern void vout_trim_string(char *str);

extern struct vinfo_s *get_invalid_vinfo(int index);
extern struct vout_module_s *vout_func_get_vout_module(void);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
extern struct vout_module_s *vout_func_get_vout2_module(void);
#endif

extern void vout_func_set_state(int index, enum vmode_e mode);
extern void vout_func_update_viu(int index);
extern int vout_func_set_vmode(int index, enum vmode_e mode);
extern int vout_func_set_current_vmode(int index, enum vmode_e mode);
extern enum vmode_e vout_func_validate_vmode(int index, char *name);
extern int vout_func_set_vframe_rate_hint(int index, int duration);
extern int vout_func_set_vframe_rate_end_hint(int index);
extern int vout_func_set_vframe_rate_policy(int index, int policy);
extern int vout_func_get_vframe_rate_policy(int index);
extern int vout_func_vout_suspend(int index);
extern int vout_func_vout_resume(int index);
extern int vout_func_vout_shutdown(int index);
extern int vout_func_vout_register_server(int index,
		struct vout_server_s *mem_server);
extern int vout_func_vout_unregister_server(int index,
		struct vout_server_s *mem_server);


extern int set_current_vmode(enum vmode_e);
extern enum vmode_e validate_vmode(char *p);

extern int vout_suspend(void);
extern int vout_resume(void);
extern int vout_shutdown(void);

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
extern int set_current_vmode2(enum vmode_e);
extern enum vmode_e validate_vmode2(char *p);

extern int vout2_suspend(void);
extern int vout2_resume(void);
extern int vout2_shutdown(void);
#endif

#endif
