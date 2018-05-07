/*
 * drivers/amlogic/media/common/vpu/vpu.h
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

#ifndef __VPU_PARA_H__
#define __VPU_PARA_H__
#include <linux/clk.h>
#include <linux/clk-provider.h>

/*#define VPU_DEBUG_PRINT*/

#define VPUPR(fmt, args...)     pr_info("vpu: "fmt"", ## args)
#define VPUERR(fmt, args...)    pr_err("vpu: error: "fmt"", ## args)

enum vpu_chip_e {
	VPU_CHIP_GXBB = 0,
	VPU_CHIP_GXTVBB,
	VPU_CHIP_GXL,
	VPU_CHIP_GXM,
	VPU_CHIP_TXLX,
	VPU_CHIP_AXG,
	VPU_CHIP_G12A,
	VPU_CHIP_G12B,
	VPU_CHIP_MAX,
};

struct fclk_div_s {
	unsigned int fclk_id;
	unsigned int fclk_mux;
	unsigned int fclk_div;
};

struct vpu_clk_s {
	unsigned int freq;
	unsigned int mux;
	unsigned int div;
};

struct vpu_ctrl_s {
	unsigned int vmod;
	unsigned int reg;
	unsigned int bit;
	unsigned int len;
};

struct vpu_data_s {
	enum vpu_chip_e chip_type;
	const char *chip_name;
	unsigned char clk_level_dft;
	unsigned char clk_level_max;
	struct fclk_div_s *fclk_div_table;

	unsigned char gp_pll_valid;
	unsigned char mem_pd_reg1_valid;
	unsigned char mem_pd_reg2_valid;

	unsigned int mem_pd_table_cnt;
	unsigned int clk_gate_table_cnt;
	struct vpu_ctrl_s *mem_pd_table;
	struct vpu_ctrl_s *clk_gate_table;

	void (*power_on)(void);
	void (*power_off)(void);
};

struct vpu_conf_s {
	struct vpu_data_s *data;
	unsigned int clk_level;
	unsigned int mem_pd0;
	unsigned int mem_pd1;
	unsigned int mem_pd2;

	/* clktree */
	struct clk *gp_pll;
	struct clk *vpu_clk0;
	struct clk *vpu_clk1;
	struct clk *vpu_clk;

	unsigned int *clk_vmod;
};

/* ************************************************ */
extern struct vpu_conf_s vpu_conf;

extern int vpu_debug_print_flag;

extern int vpu_chip_valid_check(void);
extern void vpu_ctrl_probe(void);

extern void vpu_mem_pd_init_off(void);
extern void vpu_clk_gate_init_off(void);
extern void vpu_module_init_config(void);
extern void vpu_power_on_gx(void);
extern void vpu_power_off_gx(void);
extern void vpu_power_on_txlx(void);
extern void vpu_power_off_txlx(void);

#endif
