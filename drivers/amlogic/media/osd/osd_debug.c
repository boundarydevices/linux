/*
 * drivers/amlogic/media/osd/osd_debug.c
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

/* Linux Headers */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>

/* Amlogic Headers */
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_GE2D
#include <linux/amlogic/media/ge2d/ge2d.h>
#endif

/* Local Headers */
#include "osd_canvas.h"
#include "osd_log.h"
#include "osd_reg.h"
#include "osd_io.h"
#include "osd_hw.h"

#define OSD_TEST_DURATION 200

#ifdef CONFIG_AMLOGIC_MEDIA_GE2D
static struct config_para_s ge2d_config;
static struct config_para_ex_s ge2d_config_ex;
static struct ge2d_context_s *ge2d_context;
#endif

char osd_debug_help[] = "Usage:\n"
"  echo [i | info] > debug ; Show osd pan/display/scale information\n"
"  echo [t | test] > debug ; Start osd auto test\n"
"  echo [r | read] reg > debug ; Read VCBUS register\n"
"  echo [w | write] reg val > debug ; Write VCBUS register\n"
"  echo [d | dump] {start end} > debug ; Dump VCBUS register\n\n";

static void osd_debug_dump_value(void)
{
	u32 index = 0;
	struct hw_para_s *hwpara = NULL;
	struct pandata_s *pdata = NULL;

	osd_get_hw_para(&hwpara);
	if (hwpara == NULL)
		return;

	osd_log_info("--- OSD ---\n");
	osd_log_info("bot_type: %d\n", hwpara->bot_type);
	osd_log_info("field_out_en: %d\n", hwpara->field_out_en);

	if (hwpara->osd_meson_dev.osd_ver == OSD_HIGH_ONE) {
		struct hw_osd_blending_s *blend_para = NULL;

		osd_get_blending_para(&blend_para);
		if (blend_para != NULL) {
			osd_log_info("OSD LAYER: %d\n", blend_para->layer_cnt);
			osd_log_info("|index\t|order\t|src axis\t|dst axis\n");
			for (index = 0; index < HW_OSD_COUNT; index++) {
				osd_log_info("%2d\t%2d\t(%4d,%4d,%4d,%4d)\t(%4d,%4d,%4d,%4d)\n",
					index,
					hwpara->order[index],
					hwpara->src_data[index].x,
					hwpara->src_data[index].y,
					hwpara->src_data[index].w,
					hwpara->src_data[index].h,
					hwpara->dst_data[index].x,
					hwpara->dst_data[index].y,
					hwpara->dst_data[index].w,
					hwpara->dst_data[index].h);
			}
		}
	}
	for (index = 0; index < HW_OSD_COUNT; index++) {
		osd_log_info("\n--- OSD%d ---\n", index);
		osd_log_info("order: %d\n", hwpara->order[index]);
		osd_log_info("scan_mode: %d\n", hwpara->scan_mode[index]);
		osd_log_info("enable: %d\n", hwpara->enable[index]);
		osd_log_info("2x-scale enable.h:%d .v: %d\n",
				hwpara->scale[index].h_enable,
				hwpara->scale[index].v_enable);
		osd_log_info("free-scale-mode: %d\n",
				hwpara->free_scale_mode[index]);
		osd_log_info("free-scale enable.h:%d .v: %d\n",
				hwpara->free_scale[index].h_enable,
				hwpara->free_scale[index].v_enable);
		pdata = &hwpara->pandata[index];
		osd_log_info("pan data:\n");
		osd_log_info("\tx_start: 0x%08x, x_end: 0x%08x\n",
				pdata->x_start, pdata->x_end);
		osd_log_info("\ty_start: 0x%08x, y_end: 0x%08x\n",
				pdata->y_start, pdata->y_end);

		pdata = &hwpara->dispdata[index];
		osd_log_info("disp data:\n");
		osd_log_info("\tx_start: 0x%08x, x_end: 0x%08x\n",
				pdata->x_start, pdata->x_end);
		osd_log_info("\ty_start: 0x%08x, y_end: 0x%08x\n",
				pdata->y_start, pdata->y_end);

		pdata = &hwpara->scaledata[index];
		osd_log_info("2x-scale data:\n");
		osd_log_info("\tx_start: 0x%08x, x_end: 0x%08x\n",
				pdata->x_start, pdata->x_end);
		osd_log_info("\ty_start: 0x%08x, y_end: 0x%08x\n",
				pdata->y_start, pdata->y_end);

		pdata = &hwpara->free_src_data[index];
		osd_log_info("free-scale src data:\n");
		osd_log_info("\tx_start: 0x%08x, x_end: 0x%08x\n",
				pdata->x_start, pdata->x_end);
		osd_log_info("\ty_start: 0x%08x, y_end: 0x%08x\n",
				pdata->y_start, pdata->y_end);

		pdata = &hwpara->free_dst_data[index];
		osd_log_info("free-scale dst data:\n");
		osd_log_info("\tx_start: 0x%08x, x_end: 0x%08x\n",
				pdata->x_start, pdata->x_end);
		osd_log_info("\ty_start: 0x%08x, y_end: 0x%08x\n",
				pdata->y_start, pdata->y_end);
	}

}

static void osd_debug_dump_register_all(void)
{
	u32 reg = 0;
	u32 index = 0;
	u32 count = osd_hw.osd_meson_dev.osd_count;
	struct hw_osd_reg_s *osd_reg = NULL;

	reg = VPU_VIU_VENC_MUX_CTRL;
	osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
	reg = VPP_MISC;
	osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
	reg = VPP_OFIFO_SIZE;
	osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
	reg = VPP_HOLD_LINES;
	osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
	if (osd_hw.osd_meson_dev.osd_ver == OSD_HIGH_ONE) {
		reg = OSD_PATH_MISC_CTRL;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_CTRL;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_DIN0_SCOPE_H;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_DIN0_SCOPE_V;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_DIN1_SCOPE_H;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_DIN1_SCOPE_V;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_DIN2_SCOPE_H;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_DIN2_SCOPE_V;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_DIN3_SCOPE_H;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_DIN3_SCOPE_V;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_DUMMY_DATA0;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_DUMMY_ALPHA;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_BLEND0_SIZE;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_BLEND1_SIZE;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));

		reg = VPP_OSD1_IN_SIZE;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VPP_OSD1_BLD_H_SCOPE;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VPP_OSD1_BLD_V_SCOPE;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VPP_OSD2_BLD_H_SCOPE;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VPP_OSD2_BLD_V_SCOPE;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VD1_BLEND_SRC_CTRL;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VD2_BLEND_SRC_CTRL;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = OSD1_BLEND_SRC_CTRL;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = OSD2_BLEND_SRC_CTRL;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD_BLEND_CTRL1;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VPP_POSTBLEND_H_SIZE;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VPP_OUT_H_V_SIZE;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		if (!osd_hw.powered[count - 1])
			count--;
	}
	if (osd_hw.osd_meson_dev.osd_ver == OSD_NORMAL) {
		reg = VPP_OSD_SC_CTRL0;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VPP_OSD_SCI_WH_M1;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VPP_OSD_SCO_H_START_END;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VPP_OSD_SCO_V_START_END;
		osd_log_info("reg[0x%x]: 0x%08x\n\n", reg, osd_reg_read(reg));
	}
	#if 0
	if (osd_hw.osd_meson_dev.osd_ver == OSD_SIMPLE) {
		reg = OSD_DB_FLT_CTRL;
		osd_log_info("reg[0x%x]: 0x%08x\n\n", reg, osd_reg_read(reg));
	}
	#endif
	for (index = 0; index < count; index++) {
		osd_reg = &hw_osd_reg_array[index];
		reg = osd_reg->osd_fifo_ctrl_stat;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = osd_reg->osd_ctrl_stat;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = osd_reg->osd_ctrl_stat2;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = osd_reg->osd_blk0_cfg_w0;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = osd_reg->osd_blk0_cfg_w1;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = osd_reg->osd_blk0_cfg_w2;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = osd_reg->osd_blk0_cfg_w3;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = osd_reg->osd_blk0_cfg_w4;
		osd_log_info("reg[0x%x]: 0x%08x\n\n", reg, osd_reg_read(reg));

		if (osd_hw.osd_meson_dev.osd_ver == OSD_HIGH_ONE) {
			reg = osd_reg->osd_blk1_cfg_w4;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_blk2_cfg_w4;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_prot_ctrl;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_mali_unpack_ctrl;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_dimm_ctrl;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));

			reg = osd_reg->osd_vsc_phase_step;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_vsc_init_phase;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_vsc_ctrl0;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_hsc_phase_step;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_hsc_init_phase;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_hsc_ctrl0;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_sc_dummy_data;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_sc_ctrl0;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_sci_wh_m1;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_sco_h_start_end;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->osd_sco_v_start_end;
			osd_log_info("reg[0x%x]: 0x%08x\n\n",
				reg, osd_reg_read(reg));
		}
		if ((osd_hw.osd_meson_dev.afbc_type == MALI_AFBC) &&
			(osd_hw.osd_afbcd[index].enable)) {
			reg = osd_reg->afbc_header_buf_addr_low_s;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->afbc_header_buf_addr_high_s;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->afbc_format_specifier_s;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->afbc_buffer_width_s;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->afbc_buffer_hight_s;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->afbc_boundings_box_x_start_s;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->afbc_boundings_box_x_end_s;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->afbc_boundings_box_y_start_s;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->afbc_boundings_box_y_end_s;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->afbc_output_buf_addr_low_s;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->afbc_output_buf_addr_high_s;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->afbc_output_buf_stride_s;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = osd_reg->afbc_prefetch_cfg_s;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
		}
	}

	if (osd_hw.osd_meson_dev.cpu_id >= __MESON_CPU_MAJOR_ID_G12B) {
		if (osd_hw.osd_meson_dev.has_viu2 &&
			osd_hw.powered[OSD4]) {
			reg = VPP2_MISC;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = VPU_VIU_VENC_MUX_CTRL;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = VIU2_RMIF_CTRL1;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = VIU2_RMIF_SCOPE_X;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = VIU2_RMIF_SCOPE_Y;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = VIU2_ROT_BLK_SIZE;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = VIU2_ROT_LBUF_SIZE;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = VIU2_ROT_FMT_CTRL;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = VIU2_ROT_OUT_VCROP;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
			reg = VPP2_OFIFO_SIZE;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
		}
	}
	if (osd_hw.osd_meson_dev.afbc_type == MESON_AFBC) {
		reg = VIU_MISC_CTRL1;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
		for (reg = OSD1_AFBCD_ENABLE;
			reg <= OSD1_AFBCD_PIXEL_VSCOPE; reg++)
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
	} else if (osd_hw.osd_meson_dev.afbc_type == MALI_AFBC) {
		reg = OSD_PATH_MISC_CTRL;
		osd_log_info("reg[0x%x]: 0x%08x\n\n",
			reg, osd_reg_read(reg));
	}

	if (osd_hw.osd_meson_dev.osd_ver == OSD_SIMPLE) {
		reg = hw_osd_reg_array[OSD1].osd_blk1_cfg_w4;
		osd_log_info("reg[0x%x]: 0x%08x\n",
			reg, osd_reg_read(reg));

		reg = hw_osd_reg_array[OSD1].osd_blk2_cfg_w4;
		osd_log_info("reg[0x%x]: 0x%08x\n",
			reg, osd_reg_read(reg));
	}
}

static void osd_debug_dump_register_region(u32 start, u32 end)
{
	u32 reg = 0;

	for (reg = start; reg <= end; reg += 4)
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
}
static void osd_debug_dump_register(int argc, char **argv)
{
	int reg_start, reg_end;
	int ret;

	if (!(osd_hw.osd_meson_dev.osd_ver == OSD_SIMPLE) &&
		(osd_hw.hw_rdma_en))
		read_rdma_table();
	if ((argc == 3) && argv[1] && argv[2]) {
		ret = kstrtoint(argv[1], 16, &reg_start);
		ret = kstrtoint(argv[2], 16, &reg_end);
		osd_debug_dump_register_region(reg_start, reg_end);
	} else {
		osd_debug_dump_register_all();
	}
}

static void osd_debug_read_register(int argc, char **argv)
{
	int reg;
	int ret;

	if ((argc == 2) && argv[1]) {
		ret = kstrtoint(argv[1], 16, &reg);
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
	} else {
		osd_log_err("read: arg error\n");
	}
}

static void osd_debug_write_register(int argc, char **argv)
{
	int reg, val;
	int ret;

	if ((argc == 3) && argv[1] && argv[2]) {
		ret = kstrtoint(argv[1], 16, &reg);
		ret = kstrtoint(argv[2], 16, &val);
		osd_reg_write(reg, val);
		osd_log_info("reg[0x%x]: 0x%08x =0x%08x\n",
				reg, val, osd_reg_read(reg));
	} else {
		osd_log_err("write: arg error\n");
	}
}

static void osd_test_colorbar(void)
{
#define HHI_GCLK_OTHER    0x1054
	u32 gclk_other = 0;
	u32 encp_video_adv = 0;

	gclk_other = osd_cbus_read(HHI_GCLK_OTHER);
	encp_video_adv = osd_reg_read(ENCP_VIDEO_MODE_ADV);

	/* start test mode */
	osd_log_info("--- OSD TEST COLORBAR ---\n");
	osd_cbus_write(HHI_GCLK_OTHER, 0xFFFFFFFF);
	osd_reg_write(ENCP_VIDEO_MODE_ADV, 0);
	osd_reg_write(VENC_VIDEO_TST_EN, 1);
	/* TST_MODE COLORBAR */
	osd_log_info("- COLORBAR -\n");
	osd_reg_write(VENC_VIDEO_TST_MDSEL, 1);
	msleep(OSD_TEST_DURATION);

	/* TST_MODE THINLINE */
	osd_log_info("- THINLINE -\n");
	osd_reg_write(VENC_VIDEO_TST_MDSEL, 2);
	msleep(OSD_TEST_DURATION);
	/* TST_MODE DOTGRID */
	osd_log_info("- DOTGRID -\n");
	osd_reg_write(VENC_VIDEO_TST_MDSEL, 3);
	msleep(OSD_TEST_DURATION);

	/* stop test mode */
	osd_cbus_write(HHI_GCLK_OTHER, gclk_other);
	osd_reg_write(ENCP_VIDEO_MODE_ADV, encp_video_adv);
	osd_reg_write(VENC_VIDEO_TST_EN, 0);
	osd_reg_write(VENC_VIDEO_TST_MDSEL, 0);
}

static void osd_reset(void)
{
	osd_set_free_scale_enable_hw(0, 0);
	osd_enable_hw(0, 1);
}

static void osd_test_dummydata(void)
{
	u32 dummy_data = 0;

	dummy_data = osd_reg_read(VPP_DUMMY_DATA1);
	osd_reset();
	osd_log_info("--- OSD TEST DUMMYDATA ---\n");
	osd_reg_write(VPP_DUMMY_DATA1, 0xFF);
	msleep(OSD_TEST_DURATION);
	osd_reg_write(VPP_DUMMY_DATA1, 0);
	msleep(OSD_TEST_DURATION);
	osd_reg_write(VPP_DUMMY_DATA1, 0xFF00);
	msleep(OSD_TEST_DURATION);
	osd_reg_write(VPP_DUMMY_DATA1, dummy_data);
}

static void osd_test_rect(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_GE2D
	u32 x = 0;
	u32 y = 0;
	u32 w = 0;
	u32 h = 0;
	u32 color = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
	struct canvas_s cs;
#endif
	u32 cs_addr, cs_width, cs_height;
	struct config_para_s *cfg = &ge2d_config;
	struct config_para_ex_s *cfg_ex = &ge2d_config_ex;
	struct ge2d_context_s *context = ge2d_context;

#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
	if (!osd_hw.osd_meson_dev.osd_ver == OSD_SIMPLE) {
		canvas_read(OSD1_CANVAS_INDEX, &cs);
		cs_addr = cs.addr;
		cs_width = cs.width;
		cs_height = cs.height;
	} else
		osd_get_info(0, &cs_addr,
			&cs_width, &cs_height);
#else
	osd_get_info(0, &cs_addr,
		&cs_width, &cs_height);
#endif
	context = create_ge2d_work_queue();
	if (!context) {
		osd_log_err("create work queue error\n");
		return;
	}

	memset(cfg, 0, sizeof(struct config_para_s));
	cfg->src_dst_type = OSD0_OSD0;
	cfg->src_format = GE2D_FORMAT_S32_ARGB;
	cfg->src_planes[0].addr = cs_addr;
	cfg->src_planes[0].w = cs_width / 4;
	cfg->src_planes[0].h = cs_height;
	cfg->dst_planes[0].addr = cs_addr;
	cfg->dst_planes[0].w = cs_width / 4;
	cfg->dst_planes[0].h = cs_height;

	if (ge2d_context_config(context, cfg) < 0) {
		osd_log_err("ge2d config error.\n");
		return;
	}

	x = 0;
	y = 0;
	w = cs_width / 4;
	h = cs_height;
	color = 0x0;
	osd_log_info("- BLACK -");
	osd_log_info("- (%d, %d)-(%d, %d) -\n", x, y, w, h);
	fillrect(context, x, y, h, w, color);
	msleep(OSD_TEST_DURATION);

	x = 100;
	y = 0;
	w = 100;
	h = 100;
	color = 0xFF0000FF;
	osd_log_info("- RED -\n");
	osd_log_info("- (%d, %d)-(%d, %d) -\n", x, y, w, h);
	fillrect(context, x, y, h, w, color);
	msleep(OSD_TEST_DURATION);

	x += 100;
	color = 0x00FF00FF;
	osd_log_info("- GREEN -\n");
	osd_log_info("- (%d, %d)-(%d, %d) -\n", x, y, w, h);
	fillrect(context, x, y, h, w, color);
	msleep(OSD_TEST_DURATION);

	x += 100;
	color = 0x0000FFFF;
	osd_log_info("- BlUE -\n");
	osd_log_info("- (%d, %d)-(%d, %d) -\n", x, y, w, h);
	fillrect(context, x, y, h, w, color);
	msleep(OSD_TEST_DURATION);

	memset(cfg_ex, 0, sizeof(struct config_para_ex_s));
	cfg_ex->src_planes[0].addr = cs_addr;
	cfg_ex->src_planes[0].w = cs_width / 4;
	cfg_ex->src_planes[0].h = cs_height;
	cfg_ex->dst_planes[0].addr = cs_addr;
	cfg_ex->dst_planes[0].w = cs_width / 4;
	cfg_ex->dst_planes[0].h = cs_height;

	cfg_ex->src_para.canvas_index = OSD1_CANVAS_INDEX;
	cfg_ex->src_para.mem_type = CANVAS_OSD0;
	cfg_ex->src_para.format = GE2D_FORMAT_S32_ARGB;
	cfg_ex->src_para.fill_color_en = 0;
	cfg_ex->src_para.fill_mode = 0;
	cfg_ex->src_para.x_rev = 0;
	cfg_ex->src_para.y_rev = 0;
	cfg_ex->src_para.color = 0xffffffff;
	cfg_ex->src_para.top = 0;
	cfg_ex->src_para.left = 0;
	cfg_ex->src_para.width = cs_width / 4;
	cfg_ex->src_para.height = cs_height;

	cfg_ex->dst_para.canvas_index = OSD1_CANVAS_INDEX;
	cfg_ex->dst_para.mem_type = CANVAS_OSD0;
	cfg_ex->dst_para.format = GE2D_FORMAT_S32_ARGB;
	cfg_ex->dst_para.top = 0;
	cfg_ex->dst_para.left = 0;
	cfg_ex->dst_para.width = cs_width / 4;
	cfg_ex->dst_para.height = cs_height;
	cfg_ex->dst_para.fill_color_en = 0;
	cfg_ex->dst_para.fill_mode = 0;
	cfg_ex->dst_para.color = 0;
	cfg_ex->dst_para.x_rev = 0;
	cfg_ex->dst_para.y_rev = 0;
	cfg_ex->dst_xy_swap = 0;

	if (ge2d_context_config_ex(context, cfg_ex) < 0) {
		osd_log_err("ge2d config error.\n");
		return;
	}

	stretchblt(context, 100, 0, 400, 100, 100, 200, 700, 200);

	destroy_ge2d_work_queue(ge2d_context);
#endif
}

static void osd_debug_auto_test(void)
{
	if (!osd_hw.osd_meson_dev.osd_ver == OSD_SIMPLE)
		osd_test_colorbar();

	osd_test_dummydata();

	osd_test_rect();
}

char *osd_get_debug_hw(void)
{
	return osd_debug_help;
}

int osd_set_debug_hw(const char *buf)
{
	int argc;
	char *buffer, *p, *para;
	char *argv[4];
	char cmd;

	buffer = kstrdup(buf, GFP_KERNEL);
	p = buffer;
	for (argc = 0; argc < 4; argc++) {
		para = strsep(&p, " ");
		if (para == NULL)
			break;
		argv[argc] = para;
	}

	if (argc < 1 || argc > 4) {
		kfree(buffer);
		return -EINVAL;
	}

	cmd = argv[0][0];
	switch (cmd) {
	case 'i':
		osd_debug_dump_value();
		break;
	case 'd':
		osd_debug_dump_register(argc, argv);
		break;
	case 'r':
		osd_debug_read_register(argc, argv);
		break;
	case 'w':
		osd_debug_write_register(argc, argv);
		break;
	case 't':
		osd_debug_auto_test();
		break;
	case 's':
		output_save_info();
		break;
	default:
		osd_log_err("arg error\n");
		break;
	}

	kfree(buffer);
	return 0;
}
