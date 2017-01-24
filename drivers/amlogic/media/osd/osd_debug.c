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
#include <linux/amlogic/cpu_version.h>
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
	osd_log_info("scan_mode: %d\n", hwpara->scan_mode);
	osd_log_info("order: %d\n", hwpara->order);
	osd_log_info("bot_type: %d\n", hwpara->bot_type);
	osd_log_info("field_out_en: %d\n", hwpara->field_out_en);

	for (index = 0; index < HW_OSD_COUNT; index++) {
		osd_log_info("\n--- OSD%d ---\n", index);
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
	u32 offset = 0;
	u32 index = 0;

	reg = VPU_VIU_VENC_MUX_CTRL;
	osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
	reg = VPP_MISC;
	osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
	reg = VPP_OFIFO_SIZE;
	osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
	reg = VPP_HOLD_LINES;
	osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
	reg = VPP_OSD_SC_CTRL0;
	osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
	reg = VPP_OSD_SCI_WH_M1;
	osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
	reg = VPP_OSD_SCO_H_START_END;
	osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
	reg = VPP_OSD_SCO_V_START_END;
	osd_log_info("reg[0x%x]: 0x%08x\n\n", reg, osd_reg_read(reg));

	for (index = 0; index < 2; index++) {
		if (index == 1)
			offset = REG_OFFSET;
		reg = offset + VIU_OSD1_FIFO_CTRL_STAT;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = offset + VIU_OSD1_CTRL_STAT;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = offset + VIU_OSD1_CTRL_STAT2;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = offset + VIU_OSD1_BLK0_CFG_W0;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = offset + VIU_OSD1_BLK0_CFG_W1;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = offset + VIU_OSD1_BLK0_CFG_W2;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = offset + VIU_OSD1_BLK0_CFG_W3;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD1_BLK0_CFG_W4;
		if (index == 1)
			reg = VIU_OSD2_BLK0_CFG_W4;
		osd_log_info("reg[0x%x]: 0x%08x\n\n", reg, osd_reg_read(reg));
	}

	if ((get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB) ||
		(get_cpu_type() == MESON_CPU_MAJOR_ID_GXM)) {
		reg = VIU_MISC_CTRL1;
			osd_log_info("reg[0x%x]: 0x%08x\n",
				reg, osd_reg_read(reg));
		for (reg = OSD1_AFBCD_ENABLE;
			reg <= OSD1_AFBCD_PIXEL_VSCOPE; reg++)
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

#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
	read_rdma_table();
#endif
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

	gclk_other = aml_read_cbus(HHI_GCLK_OTHER);
	encp_video_adv = osd_reg_read(ENCP_VIDEO_MODE_ADV);

	/* start test mode */
	osd_log_info("--- OSD TEST COLORBAR ---\n");
	aml_write_cbus(HHI_GCLK_OTHER, 0xFFFFFFFF);
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
	aml_write_cbus(HHI_GCLK_OTHER, gclk_other);
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
	struct canvas_s cs;
	struct config_para_s *cfg = &ge2d_config;
	struct config_para_ex_s *cfg_ex = &ge2d_config_ex;
	struct ge2d_context_s *context = ge2d_context;

	canvas_read(OSD1_CANVAS_INDEX, &cs);
	context = create_ge2d_work_queue();
	if (!context) {
		osd_log_err("create work queue error\n");
		return;
	}

	memset(cfg, 0, sizeof(struct config_para_s));
	cfg->src_dst_type = OSD0_OSD0;
	cfg->src_format = GE2D_FORMAT_S32_ARGB;
	cfg->src_planes[0].addr = cs.addr;
	cfg->src_planes[0].w = cs.width / 4;
	cfg->src_planes[0].h = cs.height;
	cfg->dst_planes[0].addr = cs.addr;
	cfg->dst_planes[0].w = cs.width / 4;
	cfg->dst_planes[0].h = cs.height;

	if (ge2d_context_config(context, cfg) < 0) {
		osd_log_err("ge2d config error.\n");
		return;
	}

	x = 0;
	y = 0;
	w = cs.width / 4;
	h = cs.height;
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
	cfg_ex->src_planes[0].addr = cs.addr;
	cfg_ex->src_planes[0].w = cs.width / 4;
	cfg_ex->src_planes[0].h = cs.height;
	cfg_ex->dst_planes[0].addr = cs.addr;
	cfg_ex->dst_planes[0].w = cs.width / 4;
	cfg_ex->dst_planes[0].h = cs.height;

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
	cfg_ex->src_para.width = cs.width / 4;
	cfg_ex->src_para.height = cs.height;

	cfg_ex->dst_para.canvas_index = OSD1_CANVAS_INDEX;
	cfg_ex->dst_para.mem_type = CANVAS_OSD0;
	cfg_ex->dst_para.format = GE2D_FORMAT_S32_ARGB;
	cfg_ex->dst_para.top = 0;
	cfg_ex->dst_para.left = 0;
	cfg_ex->dst_para.width = cs.width / 4;
	cfg_ex->dst_para.height = cs.height;
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
	default:
		osd_log_err("arg error\n");
		break;
	}

	kfree(buffer);
	return 0;
}
