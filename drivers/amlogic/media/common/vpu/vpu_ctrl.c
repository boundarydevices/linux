/*
 * drivers/amlogic/media/common/vpu/vpu_ctrl.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "vpu_reg.h"
#include "vpu.h"
#include "vpu_module.h"

static spinlock_t vpu_mem_lock;
static spinlock_t vpu_clk_gate_lock;

/* *********************************************** */
/* VPU_MEM_PD control */
/* *********************************************** */
/*
 *  Function: switch_vpu_mem_pd_vmod
 *      switch vpu memory power down by specified vmod
 *
 *  Parameters:
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *      flag - int, on/off switch flag, must be one of the following constants:
 *                 VPU_MEM_POWER_ON
 *                 VPU_MEM_POWER_DOWN
 *
 *  Example:
 *      switch_vpu_mem_pd_vmod(VPU_VENCP, VPU_MEM_POWER_ON);
 *      switch_vpu_mem_pd_vmod(VPU_VIU_OSD1, VPU_MEM_POWER_DOWN);
 *
 */
void switch_vpu_mem_pd_vmod(unsigned int vmod, int flag)
{
	unsigned long flags = 0;
	unsigned int _reg0, _reg1, _reg2;
	unsigned int val;
	int ret = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return;

	spin_lock_irqsave(&vpu_mem_lock, flags);

	val = (flag == VPU_MEM_POWER_ON) ? 0 : 3;
	_reg0 = HHI_VPU_MEM_PD_REG0;
	_reg1 = HHI_VPU_MEM_PD_REG1;
	_reg2 = HHI_VPU_MEM_PD_REG2;

	switch (vmod) {
	case VPU_VIU_OSD1:
		vpu_hiu_setb(_reg0, val, 0, 2);
		break;
	case VPU_VIU_OSD2:
		vpu_hiu_setb(_reg0, val, 2, 2);
		break;
	case VPU_VIU_VD1:
		vpu_hiu_setb(_reg0, val, 4, 2);
		break;
	case VPU_VIU_VD2:
		vpu_hiu_setb(_reg0, val, 6, 2);
		break;
	case VPU_VIU_CHROMA:
		vpu_hiu_setb(_reg0, val, 8, 2);
		break;
	case VPU_VIU_OFIFO:
		vpu_hiu_setb(_reg0, val, 10, 2);
		break;
	case VPU_VIU_SCALE:
		vpu_hiu_setb(_reg0, val, 12, 2);
		break;
	case VPU_VIU_OSD_SCALE:
		vpu_hiu_setb(_reg0, val, 14, 2);
		break;
	case VPU_VIU_VDIN0:
		vpu_hiu_setb(_reg0, val, 16, 2);
		break;
	case VPU_VIU_VDIN1:
		vpu_hiu_setb(_reg0, val, 18, 2);
		break;
	case VPU_PIC_ROT1:
	case VPU_VIU_SRSCL:
		vpu_hiu_setb(_reg0, val, 20, 2);
		break;
	case VPU_PIC_ROT2:
	case VPU_VIU_OSDSR:
	case VPU_AFBC_DEC1:
		vpu_hiu_setb(_reg0, val, 22, 2);
		break;
	case VPU_PIC_ROT3:
		vpu_hiu_setb(_reg0, val, 24, 2);
		break;
	case VPU_DI_PRE:
		vpu_hiu_setb(_reg0, val, 26, 2);
		break;
	case VPU_DI_POST:
		vpu_hiu_setb(_reg0, val, 28, 2);
		break;
	case VPU_SHARP:
		vpu_hiu_setb(_reg0, val, 30, 2);
		break;
	case VPU_VIU2_OSD1:
		vpu_hiu_setb(_reg1, val, 0, 2);
		break;
	case VPU_VIU2_OSD2:
		vpu_hiu_setb(_reg1, val, 2, 2);
		break;
	case VPU_VIU2_VD1:
		vpu_hiu_setb(_reg1, val, 4, 2);
		break;
	case VPU_VIU2_CHROMA:
		vpu_hiu_setb(_reg1, val, 6, 2);
		break;
	case VPU_VIU2_OFIFO:
		vpu_hiu_setb(_reg1, val, 8, 2);
		break;
	case VPU_VIU2_SCALE:
		vpu_hiu_setb(_reg1, val, 10, 2);
		break;
	case VPU_VIU2_OSD_SCALE:
		vpu_hiu_setb(_reg1, val, 12, 2);
		break;
	case VPU_VDIN_AM_ASYNC:
	case VPU_VPU_ARB:
		vpu_hiu_setb(_reg1, val, 14, 2);
		break;
	case VPU_VDISP_AM_ASYNC:
	case VPU_AFBC_DEC:
	case VPU_OSD1_AFBCD:
	case VPU_AFBC_DEC0:
		vpu_hiu_setb(_reg1, val, 16, 2);
		break;
	case VPU_VENCP:
		vpu_hiu_setb(_reg1, val, 20, 2);
		break;
	case VPU_VENCL:
		vpu_hiu_setb(_reg1, val, 22, 2);
		break;
	case VPU_VENCI:
		vpu_hiu_setb(_reg1, val, 24, 2);
		break;
	case VPU_ISP:
		vpu_hiu_setb(_reg1, val, 26, 2);
		break;
	case VPU_CVD2:
	case VPU_LDIM_STTS:
		vpu_hiu_setb(_reg1, val, 28, 2);
		break;
	case VPU_ATV_DMD:
	case VPU_XVYCC_LUT:
		vpu_hiu_setb(_reg1, val, 30, 2);
		break;
	case VPU_VIU1_WM:
		if ((vpu_chip_type == VPU_CHIP_GXL) ||
			(vpu_chip_type == VPU_CHIP_GXM) ||
			(vpu_chip_type == VPU_CHIP_TXL)) {
			vpu_hiu_setb(_reg2, val, 0, 2);
		}
		break;
	default:
		VPUPR("switch_vpu_mem_pd: unsupport vpu mod\n");
		break;
	}

	spin_unlock_irqrestore(&vpu_mem_lock, flags);

	if (vpu_debug_print_flag) {
		VPUPR("switch_vpu_mem_pd: %s %s\n",
			vpu_mod_table[vmod],
			((flag == VPU_MEM_POWER_ON) ? "ON" : "OFF"));
		dump_stack();
	}
}

/*
 *  Function: get_vpu_mem_pd_vmod
 *      switch vpu memory power down by specified vmod
 *
 *  Parameters:
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *
 *  Returns:
 *      int, 0 for power on, 1 for power down, -1 for error
 *
 *  Example:
 *      ret = get_vpu_mem_pd_vmod(VPU_VENCP);
 *      ret = get_vpu_mem_pd_vmod(VPU_VIU_OSD1);
 *
 */
#define VPU_MEM_PD_ERR        0xffff
int get_vpu_mem_pd_vmod(unsigned int vmod)
{
	unsigned int _reg0, _reg1, _reg2;
	unsigned int val;
	int ret = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;

	_reg0 = HHI_VPU_MEM_PD_REG0;
	_reg1 = HHI_VPU_MEM_PD_REG1;
	_reg2 = HHI_VPU_MEM_PD_REG2;

	switch (vmod) {
	case VPU_VIU_OSD1:
		val = vpu_hiu_getb(_reg0, 0, 2);
		break;
	case VPU_VIU_OSD2:
		val = vpu_hiu_getb(_reg0, 2, 2);
		break;
	case VPU_VIU_VD1:
		val = vpu_hiu_getb(_reg0, 4, 2);
		break;
	case VPU_VIU_VD2:
		val = vpu_hiu_getb(_reg0, 6, 2);
		break;
	case VPU_VIU_CHROMA:
		val = vpu_hiu_getb(_reg0, 8, 2);
		break;
	case VPU_VIU_OFIFO:
		val = vpu_hiu_getb(_reg0, 10, 2);
		break;
	case VPU_VIU_SCALE:
		val = vpu_hiu_getb(_reg0, 12, 2);
		break;
	case VPU_VIU_OSD_SCALE:
		val = vpu_hiu_getb(_reg0, 14, 2);
		break;
	case VPU_VIU_VDIN0:
		val = vpu_hiu_getb(_reg0, 16, 2);
		break;
	case VPU_VIU_VDIN1:
		val = vpu_hiu_getb(_reg0, 18, 2);
		break;
	case VPU_PIC_ROT1:
	case VPU_VIU_SRSCL:
		val = vpu_hiu_getb(_reg0, 20, 2);
		break;
	case VPU_PIC_ROT2:
	case VPU_VIU_OSDSR:
	case VPU_AFBC_DEC1:
		val = vpu_hiu_getb(_reg0, 22, 2);
		break;
	case VPU_PIC_ROT3:
		val = vpu_hiu_getb(_reg0, 24, 2);
		break;
	case VPU_DI_PRE:
		val = vpu_hiu_getb(_reg0, 26, 2);
		break;
	case VPU_DI_POST:
		val = vpu_hiu_getb(_reg0, 28, 2);
		break;
	case VPU_SHARP:
		val = vpu_hiu_getb(_reg0, 30, 2);
		break;
	case VPU_VIU2_OSD1:
		val = vpu_hiu_getb(_reg1, 0, 2);
		break;
	case VPU_VIU2_OSD2:
		val = vpu_hiu_getb(_reg1, 2, 2);
		break;
	case VPU_VIU2_VD1:
		val = vpu_hiu_getb(_reg1, 4, 2);
		break;
	case VPU_VIU2_CHROMA:
		val = vpu_hiu_getb(_reg1, 6, 2);
		break;
	case VPU_VIU2_OFIFO:
		val = vpu_hiu_getb(_reg1, 8, 2);
		break;
	case VPU_VIU2_SCALE:
		val = vpu_hiu_getb(_reg1, 10, 2);
		break;
	case VPU_VIU2_OSD_SCALE:
		val = vpu_hiu_getb(_reg1, 12, 2);
		break;
	case VPU_VDIN_AM_ASYNC:
	case VPU_VPU_ARB:
		val = vpu_hiu_getb(_reg1, 14, 2);
		break;
	case VPU_VDISP_AM_ASYNC:
	case VPU_AFBC_DEC:
	case VPU_OSD1_AFBCD:
	case VPU_AFBC_DEC0:
		val = vpu_hiu_getb(_reg1, 16, 2);
		break;
	case VPU_VENCP:
		val = vpu_hiu_getb(_reg1, 20, 2);
		break;
	case VPU_VENCL:
		val = vpu_hiu_getb(_reg1, 22, 2);
		break;
	case VPU_VENCI:
		val = vpu_hiu_getb(_reg1, 24, 2);
		break;
	case VPU_ISP:
		val = vpu_hiu_getb(_reg1, 26, 2);
		break;
	case VPU_CVD2:
	case VPU_LDIM_STTS:
		val = vpu_hiu_getb(_reg1, 28, 2);
		break;
	case VPU_ATV_DMD:
	case VPU_XVYCC_LUT:
		val = vpu_hiu_getb(_reg1, 30, 2);
		break;
	case VPU_VIU1_WM:
		if ((vpu_chip_type == VPU_CHIP_GXL) ||
			(vpu_chip_type == VPU_CHIP_GXM) ||
			(vpu_chip_type == VPU_CHIP_TXL)) {
			val = vpu_hiu_getb(_reg2, 0, 2);
		} else {
			val = VPU_MEM_PD_ERR;
		}
		break;
	default:
		val = VPU_MEM_PD_ERR;
		break;
	}

	if (val == 0)
		return VPU_MEM_POWER_ON;
	else if ((val == 0x3) || (val == 0xf))
		return VPU_MEM_POWER_DOWN;
	else
		return -1;
}

/* *********************************************** */
/* VPU_CLK_GATE control */
/* *********************************************** */
/*
 *  Function: switch_vpu_clk_gate_vmod
 *      switch vpu clk gate by specified vmod
 *
 *  Parameters:
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *      flag - int, on/off switch flag, must be one of the following constants:
 *                 VPU_CLK_GATE_ON
 *                 VPU_CLK_GATE_OFF
 *
 *  Example:
 *      switch_vpu_clk_gate_vmod(VPU_VENCP, VPU_CLK_GATE_ON);
 *      switch_vpu_clk_gate_vmod(VPU_VPP, VPU_CLK_GATE_OFF);
 *
 */
void switch_vpu_clk_gate_vmod(unsigned int vmod, int flag)
{
	unsigned long flags = 0;
	unsigned int val;
	int ret = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return;

	spin_lock_irqsave(&vpu_clk_gate_lock, flags);

	val = (flag == VPU_CLK_GATE_ON) ? 1 : 0;

	switch (vmod) {
	case VPU_VPU_TOP:
		vpu_vcbus_setb(VPU_CLK_GATE, val, 1, 1); /* vpu_system_clk */
		break;
	case VPU_VPU_CLKB:
		if ((vpu_chip_type == VPU_CHIP_GXTVBB) ||
			(vpu_chip_type == VPU_CHIP_GXL) ||
			(vpu_chip_type == VPU_CHIP_GXM) ||
			(vpu_chip_type == VPU_CHIP_TXL)) {
			vpu_vcbus_setb(VPU_CLK_GATE, val, 8, 1); /* clkb_gen */
			vpu_vcbus_setb(VPU_CLK_GATE, val, 9, 1); /* clkb_gen */
			/* clkb_gen_en */
			vpu_vcbus_setb(VPU_CLK_GATE, val, 17, 1);
		}
		if ((vpu_chip_type == VPU_CHIP_GXBB) ||
			(vpu_chip_type == VPU_CHIP_GXTVBB) ||
			(vpu_chip_type == VPU_CHIP_GXL) ||
			(vpu_chip_type == VPU_CHIP_GXM) ||
			(vpu_chip_type == VPU_CHIP_TXL)) {
			/* clkb_gate */
			vpu_vcbus_setb(VPU_CLK_GATE, val, 16, 1);
		}
		break;
	case VPU_RDMA:
		if ((vpu_chip_type == VPU_CHIP_GXBB) ||
			(vpu_chip_type == VPU_CHIP_GXTVBB) ||
			(vpu_chip_type == VPU_CHIP_GXL) ||
			(vpu_chip_type == VPU_CHIP_GXM) ||
			(vpu_chip_type == VPU_CHIP_TXL)) {
			vpu_vcbus_setb(VPU_CLK_GATE, val, 15, 1); /* rdma_clk */
		}
		break;
	case VPU_VLOCK:
		vpu_vcbus_setb(VPU_CLK_GATE, val, 14, 1);
		break;
	case VPU_MISC:
		vpu_vcbus_setb(VPU_CLK_GATE, val, 6, 1); /* hs,vs,interrupt */
		break;
	case VPU_VENC_DAC:
		/* clk for dac(r/w reg) */
		/*vpu_vcbus_setb(VPU_CLK_GATE, val, 12, 1);*/
		vpu_hiu_setb(HHI_GCLK_OTHER, val, 10, 1); /* dac top clk */
		break;
	case VPU_VENCP:
		if (flag == VPU_CLK_GATE_ON) {
			vpu_vcbus_set_mask(VPU_CLK_GATE,
				((1 << 3) || (1 << 0)));
			vpu_hiu_set_mask(HHI_GCLK_OTHER,
				((1 << 9) || (0x3 << 3)));
		} else {
			vpu_vcbus_clr_mask(VPU_CLK_GATE,
				((1 << 3) || (1 << 0)));
			vpu_hiu_clr_mask(HHI_GCLK_OTHER,
				((1 << 9) || (0x3 << 3)));
		}
		break;
	case VPU_VENCL:
		if (flag == VPU_CLK_GATE_ON) {
			vpu_vcbus_set_mask(VPU_CLK_GATE, (0x3 << 4));
			vpu_hiu_set_mask(HHI_GCLK_OTHER, (0x7 << 23));
		} else {
			vpu_vcbus_clr_mask(VPU_CLK_GATE, (0x3 << 4));
			vpu_hiu_clr_mask(HHI_GCLK_OTHER, (0x7 << 23));
		}
		break;
	case VPU_VENCI:
		if (flag == VPU_CLK_GATE_ON) {
			vpu_vcbus_set_mask(VPU_CLK_GATE, (0x3 << 10));
			vpu_hiu_set_mask(HHI_GCLK_OTHER,
				((1 << 8) | (0x3 << 1)));
		} else {
			vpu_vcbus_clr_mask(VPU_CLK_GATE, (0x3 << 10));
			vpu_hiu_clr_mask(HHI_GCLK_OTHER,
				((1 << 8) | (0x3 << 1)));
		}
		break;
	case VPU_VIU_VDIN0:
		if (flag == VPU_CLK_GATE_ON) {
			vpu_vcbus_set_mask(VDIN0_COM_GCLK_CTRL, 0x3f3ffff2);
			vpu_vcbus_set_mask(VDIN0_COM_GCLK_CTRL2, 0xf);
		} else {
			vpu_vcbus_clr_mask(VDIN0_COM_GCLK_CTRL, 0x3f3ffff0);
			vpu_vcbus_clr_mask(VDIN0_COM_GCLK_CTRL2, 0xf);
		}
		break;
	case VPU_VIU_VDIN1:
		if (flag == VPU_CLK_GATE_ON) {
			vpu_vcbus_set_mask(VDIN1_COM_GCLK_CTRL, 0x3f3ffff2);
			vpu_vcbus_set_mask(VDIN1_COM_GCLK_CTRL2, 0xf);
		} else {
			vpu_vcbus_clr_mask(VDIN1_COM_GCLK_CTRL, 0x3f3ffff0);
			vpu_vcbus_clr_mask(VDIN1_COM_GCLK_CTRL2, 0xf);
		}
		break;
	case VPU_DI:
		if (flag == VPU_CLK_GATE_ON) {
			vpu_vcbus_set_mask(DI_CLKG_CTRL, 0x1d1e0003);
			if ((vpu_chip_type == VPU_CHIP_GXTVBB) ||
				(vpu_chip_type == VPU_CHIP_GXL) ||
				(vpu_chip_type == VPU_CHIP_GXM) ||
				(vpu_chip_type == VPU_CHIP_TXL)) {
				vpu_vcbus_set_mask(DI_CLKG_CTRL, 0x60200000);
			}
		} else {
			vpu_vcbus_clr_mask(DI_CLKG_CTRL, 0x1d1e0003);
			if ((vpu_chip_type == VPU_CHIP_GXTVBB) ||
				(vpu_chip_type == VPU_CHIP_GXL) ||
				(vpu_chip_type == VPU_CHIP_GXM) ||
				(vpu_chip_type == VPU_CHIP_TXL)) {
				vpu_vcbus_clr_mask(DI_CLKG_CTRL, 0x60200000);
			}
		}
		break;
	case VPU_VPP:
		if (flag == VPU_CLK_GATE_ON) {
			vpu_vcbus_set_mask(VPP_GCLK_CTRL0, 0xffff3fcc);
			if ((vpu_chip_type == VPU_CHIP_GXTVBB) ||
				(vpu_chip_type == VPU_CHIP_GXL) ||
				(vpu_chip_type == VPU_CHIP_GXM) ||
				(vpu_chip_type == VPU_CHIP_TXL)) {
				vpu_vcbus_set_mask(VPP_GCLK_CTRL0, 0xc030);
			}
			vpu_vcbus_set_mask(VPP_GCLK_CTRL1, 0xfff);
			vpu_vcbus_set_mask(VPP_SC_GCLK_CTRL, 0x03fc0ffc);
			vpu_vcbus_set_mask(VPP_XVYCC_GCLK_CTRL, 0x3ffff);
		} else {
			vpu_vcbus_clr_mask(VPP_GCLK_CTRL0, 0xffff3fcc);
			if ((vpu_chip_type == VPU_CHIP_GXTVBB) ||
				(vpu_chip_type == VPU_CHIP_GXL) ||
				(vpu_chip_type == VPU_CHIP_GXM) ||
				(vpu_chip_type == VPU_CHIP_TXL)) {
				vpu_vcbus_clr_mask(VPP_GCLK_CTRL0, 0xc030);
			}
			vpu_vcbus_clr_mask(VPP_GCLK_CTRL1, 0xfff);
			vpu_vcbus_clr_mask(VPP_SC_GCLK_CTRL, 0x03fc0ffc);
			vpu_vcbus_clr_mask(VPP_XVYCC_GCLK_CTRL, 0x3ffff);
		}
		break;
	default:
		VPUPR("switch_vpu_clk_gate: unsupport vpu mod\n");
		break;
	}

	spin_unlock_irqrestore(&vpu_clk_gate_lock, flags);

	if (vpu_debug_print_flag) {
		VPUPR("switch_vpu_clk_gate: %s %s\n",
			vpu_mod_table[vmod],
			((flag == VPU_CLK_GATE_ON) ? "ON" : "OFF"));
		dump_stack();
	}
}

/* *********************************************** */

void vpu_ctrl_probe(void)
{
	spin_lock_init(&vpu_mem_lock);
	spin_lock_init(&vpu_clk_gate_lock);
}

