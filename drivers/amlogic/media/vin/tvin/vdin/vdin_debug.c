/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_debug.c
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

/* Standard Linux Headers */
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
/* Local Headers */
#include "../tvin_format_table.h"
#include "vdin_drv.h"
#include "vdin_ctl.h"
#include "vdin_regs.h"
/*2018-07-18 add debugfs*/
#include <linux/seq_file.h>
#include <linux/debugfs.h>

static void vdin_get_vdin_yuv_rgb_mat0(unsigned int offset,
		unsigned int *rgb_yuv0,
		unsigned int *rgb_yuv1,
		unsigned int *rgb_yuv2)
{
	*rgb_yuv0 = 0;  *rgb_yuv1 = 0;  *rgb_yuv2 = 0;

	*rgb_yuv0 = ((rd_bits(offset, VDIN_MATRIX_PROBE_COLOR,
		COMPONENT2_PROBE_COLOR_BIT, COMPONENT2_PROBE_COLOR_WID)
			<< 8) >> 10);
	*rgb_yuv1 = ((rd_bits(offset, VDIN_MATRIX_PROBE_COLOR,
		COMPONENT1_PROBE_COLOR_BIT, COMPONENT1_PROBE_COLOR_WID)
			<< 8) >> 10);
	*rgb_yuv2 = ((rd_bits(offset, VDIN_MATRIX_PROBE_COLOR,
		COMPONENT0_PROBE_COLOR_BIT, COMPONENT0_PROBE_COLOR_WID)
			<< 8) >> 10);
}

static void vdin_set_prob_matrix0_xy(unsigned int offset,
		unsigned int x, unsigned int y, struct vdin_dev_s *devp)
{

	if (devp->fmt_info_p->scan_mode == TVIN_SCAN_MODE_INTERLACED)
		y = y/2;

	wr_bits(offset, VDIN_MATRIX_PROBE_POS, y,
		PROBE_POX_Y_BIT, PROBE_POX_Y_WID);
	wr_bits(offset, VDIN_MATRIX_PROBE_POS, x,
		PROBE_POS_X_BIT, PROBE_POS_X_WID);
}

static void vdin_set_before_after_mat0(unsigned int offset,
		unsigned int x, struct vdin_dev_s *devp)
{
	if ((x != 0) && (x != 1))
		return;

	wr_bits(offset, VDIN_MATRIX_CTRL, x,
			VDIN_PROBE_POST_BIT, VDIN_PROBE_POST_WID);
}

static void vdin_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static ssize_t sig_det_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int callmaster_status = 0;
	return sprintf(buf, "%d\n", callmaster_status);
}

static ssize_t sig_det_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	enum tvin_port_e port = TVIN_PORT_NULL;
	struct tvin_frontend_s *frontend = NULL;
	int callmaster_status = 0;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	long val;

	if (!buf)
		return len;
	/* port = simple_strtol(buf, NULL, 10); */
	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;
	else
		port = val;

	frontend = tvin_get_frontend(port, 0);
	if (frontend && frontend->dec_ops &&
		frontend->dec_ops->callmaster_det) {
		/*call the frontend det function*/
		callmaster_status =	frontend->dec_ops->callmaster_det(port,
						frontend);
		/* pr_info("%d\n",callmaster_status); */
	}
	pr_info("[vdin.%d]:%s callmaster_status=%d,port=[%s]\n",
			devp->index, __func__,
			callmaster_status, tvin_port_str(port));
	return len;
}
static DEVICE_ATTR(sig_det, 0664, sig_det_show, sig_det_store);

static ssize_t vdin_attr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	ssize_t len = 0;
	len += sprintf(buf+len,
		"\n0 HDMI0\t1 HDMI1\t2 HDMI2\t3 Component0\t4 Component1");
	len += sprintf(buf+len, "\n5 CVBS0\t6 CVBS1\t7 Vga0\t8 CVBS2\n");
	len += sprintf(buf+len,
		"echo tvstart/v4l2start port fmt_id/resolution > ");
	len += sprintf(buf+len,
		"/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf+len,
		"echo v4l2start bt656/viuin/video/isp h_actve v_active");
	len += sprintf(buf+len,
		"viuin/viu_wb0_vd1/viu_wb0_vd1/viu_wb0_post_blend/viu_wb0_osd1/viu_wb0_osd2");
	len += sprintf(buf+len,
		"viuin2/viu2_wb0_vd1/viu2_wb0_vd1/viu2_wb0_post_blend/viu2_wb0_osd1/viu2_wb0_osd2");
	len += sprintf(buf+len,
		"frame_rate cfmt dfmt scan_fmt > /sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf+len,
		"cfmt/dfmt:\t0 : RGB44\t1 YUV422\t2 YUV444\t7 NV12\t8 NV21\n");
	len += sprintf(buf+len,
		"scan_fmt:\t1 : PROGRESSIVE\t2 INTERLACE\n");
	len += sprintf(buf+len,
		"abnormal cnt %u\n", devp->abnormal_cnt);
	len += sprintf(buf+len,
		"echo fps >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo conversion w h dest_cfmt >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"\ndest_cfmt: 0 TVIN_RGB444\t1 TVIN_YUV422\t2 TVIN_YUV444");
	len += sprintf(buf+len,
		"\n3 TVIN_YUYV422\t4 TVIN_YVYU422\t5 TVIN_UYVY422");
	len += sprintf(buf+len,
		"\n6 TVIN_VYUY422\t7 TVIN_NV12\t8 TVIN_NV21\t9 TVIN_BGGR");
	len += sprintf(buf+len,
		"\n10 TVIN_RGGB\t11 TVIN_GBRG\t12 TVIN_GRBG");
	len += sprintf(buf+len,
		"echo state >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo histgram hnum vnum >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo force_recycle >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo read_pic parm1 parm2 >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo read_bin parm1 parm2 parm3 >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo dump_reg >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo capture external_storage/xxx.bin >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo freeze >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo unfreeze >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo rgb_xy x y  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo rgb_info  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo mpeg2vdin h_active v_active >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo yuv_rgb_info >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo mat0_xy x y >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo mat0_set x >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo hdr  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo snowon  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo snowoff  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo vf_reg  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo vf_unreg  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo pause_dec  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo resume_dec  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo color_depth val  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo color_depth_support val  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo color_depth_mode val  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo auto_cutwindow_en 0(1)  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo auto_ratio_en 0(1)  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo dolby_config  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo metadata  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo dolby_config  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo clean_dv  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo dv_debug  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo channel_order_config c0 c1 c2 >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo open_port portname >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo close_port >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf+len,
		"echo vshrk_en 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf+len,
		"echo prehsc_en 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf+len,
		"echo cma_mem_mode 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf+len,
		"echo dolby_input 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf+len,
		"echo hist_bar_enable 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf+len,
		"echo black_bar_enable 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf+len,
		"echo use_frame_rate 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf+len,
		"echo rdma_enable 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf+len,
		"echo irq_cnt >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf+len,
		"echo rdma_irq_cnt >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf+len,
		"echo skip_vf_num 0/1/2 /sys/class/vdin/vdinx/attr.\n");
	return len;
}
static void vdin_dump_one_buf_mem(char *path, struct vdin_dev_s *devp,
	unsigned int buf_num)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	unsigned int i;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR|O_CREAT, 0666);

	if (IS_ERR(filp)) {
		pr_info("create %s error.\n", path);
		return;
	}
	if ((devp->cma_config_flag & 0x1) &&
		(devp->cma_mem_alloc == 0)) {
		pr_info("%s:no cma alloc mem!!!\n", __func__);
		return;
	}
	if (buf_num < devp->canvas_max_num) {
		if (devp->cma_config_flag == 0x1)
			buf = codec_mm_phys_to_virt(devp->mem_start +
				devp->canvas_max_size*buf_num);
		else if (devp->cma_config_flag == 0x101)
			buf = codec_mm_phys_to_virt(devp->vfmem_start[buf_num]);
		else if (devp->cma_config_flag == 0x100)
			buf = phys_to_virt(devp->vfmem_start[buf_num]);
		else
			buf = phys_to_virt(devp->mem_start +
				devp->canvas_max_size*buf_num);
		/*only write active data*/
		for (i = 0; i < devp->canvas_h; i++) {
			vfs_write(filp, buf, devp->canvas_active_w, &pos);
			buf += devp->canvas_w;
		}
		/*vfs_write(filp, buf, devp->canvas_max_size, &pos);*/
		pr_info("write buffer %2d of %2u  to %s.\n",
				buf_num, devp->canvas_max_num, path);
	}
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}

static void vdin_dump_mem(char *path, struct vdin_dev_s *devp)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	loff_t i = 0, j = 0;
	unsigned int mem_size = 0;
	void *buf = NULL;
	void *vfbuf[VDIN_CANVAS_MAX_CNT];
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR|O_CREAT, 0666);

	mem_size = devp->canvas_active_w * devp->canvas_h;
	for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++)
		vfbuf[i] = NULL;
	if (IS_ERR(filp)) {
		pr_info("create %s error.\n", path);
		return;
	}
	if ((devp->cma_config_flag & 0x1) &&
		(devp->cma_mem_alloc == 0)) {
		pr_info("%s:no cma alloc mem!!!\n", __func__);
		return;
	}
	for (i = 0; i < devp->canvas_max_num; i++) {
		pos = mem_size * i;
		if (devp->cma_config_flag == 0x1)
			buf = codec_mm_phys_to_virt(devp->mem_start +
				devp->canvas_max_size*i);
		else if (devp->cma_config_flag == 0x101)
			vfbuf[i] = codec_mm_phys_to_virt(
				devp->vfmem_start[i]);
		else if (devp->cma_config_flag == 0x100)
			vfbuf[i] = phys_to_virt(devp->vfmem_start[i]);
		else
			buf = phys_to_virt(devp->mem_start +
				devp->canvas_max_size*i);
		/*only write active data*/
		for (j = 0; j < devp->canvas_h; j++) {
			if (devp->cma_config_flag & 0x100) {
				vfs_write(filp, vfbuf[i],
					devp->canvas_active_w, &pos);
				vfbuf[i] += devp->canvas_w;
			} else {
				vfs_write(filp, buf,
					devp->canvas_active_w, &pos);
				buf += devp->canvas_w;
			}
		}
		pr_info("write buffer %lld of %2u  to %s.\n",
				i, devp->canvas_max_num, path);
	}
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}

static void dump_other_mem(char *path,
		unsigned int start, unsigned int offset)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR|O_CREAT, 0666);

	if (IS_ERR(filp)) {
		pr_info("create %s error.\n", path);
		return;
	}
	buf = phys_to_virt(start);
	vfs_write(filp, buf, offset, &pos);
	pr_info("write from 0x%x to 0x%x to %s.\n",
			start, start+offset, path);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}
static void vdin_dv_debug(struct vdin_dev_s *devp)
{
	struct vframe_s *dv_vf;
	struct vf_entry *vfe;
	struct provider_aux_req_s req;

	vfe = devp->curr_wr_vfe;
	if (!vfe) {
		pr_info("current wr vfe is null\n");
		return;
	}
	dv_vf = &vfe->vf;
	req.vf = dv_vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;
	vf_notify_provider_by_name("vdin0",
		VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
		(void *)&req);
}

/*config vidn output channel order*/
static void vdin_channel_order_config(unsigned int offset,
	unsigned int vdin_data_bus_0, unsigned int vdin_data_bus_1,
	unsigned int vdin_data_bus_2)
{
	wr_bits(offset, VDIN_COM_CTRL0, vdin_data_bus_0,
			COMP0_OUT_SWT_BIT, COMP0_OUT_SWT_WID);
	wr_bits(offset, VDIN_COM_CTRL0, vdin_data_bus_1,
			COMP1_OUT_SWT_BIT, COMP1_OUT_SWT_WID);
	wr_bits(offset, VDIN_COM_CTRL0, vdin_data_bus_2,
			COMP2_OUT_SWT_BIT, COMP2_OUT_SWT_WID);
}
static void vdin_channel_order_status(unsigned int offset)
{
	unsigned int c0, c1, c2;

	c0 = rd_bits(offset, VDIN_COM_CTRL0,
			COMP0_OUT_SWT_BIT, COMP0_OUT_SWT_WID);
	c1 = rd_bits(offset, VDIN_COM_CTRL0,
			COMP1_OUT_SWT_BIT, COMP1_OUT_SWT_WID);
	c2 = rd_bits(offset, VDIN_COM_CTRL0,
			COMP2_OUT_SWT_BIT, COMP2_OUT_SWT_WID);
	pr_info("vdin(%x) current channel order is: %d %d %d\n",
		offset, c0, c1, c2);
}

static void vdin_dump_state(struct vdin_dev_s *devp)
{
	unsigned int i;
	struct vframe_s *vf = &devp->curr_wr_vfe->vf;
	struct tvin_parm_s *curparm = &devp->parm;
	struct vf_pool *vfp = devp->vfp;
	pr_info("h_active = %d, v_active = %d\n",
		devp->h_active, devp->v_active);
	pr_info("canvas_w = %d, canvas_h = %d\n",
		devp->canvas_w, devp->canvas_h);
	pr_info("canvas_alin_w = %d, canvas_active_w = %d\n",
		devp->canvas_alin_w, devp->canvas_active_w);
	if ((devp->cma_config_en != 1) || !(devp->cma_config_flag & 0x1))
		pr_info("mem_start = %ld, mem_size = %d\n",
			devp->mem_start, devp->mem_size);
	else
		for (i = 0; i < devp->canvas_max_num; i++)
			pr_info("buf[%d]mem_start = %ld, mem_size = %d\n",
			i, devp->vfmem_start[i], devp->vfmem_size);
	pr_info("signal format	= %s(0x%x)\n",
		tvin_sig_fmt_str(devp->parm.info.fmt),
		devp->parm.info.fmt);
	pr_info("trans_fmt	= %s(%d)\n",
		tvin_trans_fmt_str(devp->prop.trans_fmt),
		devp->prop.trans_fmt);
	pr_info("color_format	= %s(%d)\n",
		tvin_color_fmt_str(devp->prop.color_format),
		devp->prop.color_format);
	pr_info("format_convert = %s(%d)\n",
		vdin_fmt_convert_str(devp->format_convert),
		devp->format_convert);
	pr_info("aspect_ratio	= %s(%d)\ndecimation_ratio/dvi	= %u / %u\n",
		tvin_aspect_ratio_str(devp->prop.aspect_ratio),
		devp->prop.aspect_ratio,
		devp->prop.decimation_ratio, devp->prop.dvi_info);
	pr_info("[pre->cur]:hs(%d->%d),he(%d->%d),vs(%d->%d),ve(%d->%d)\n",
		devp->prop.pre_hs, devp->prop.hs,
		devp->prop.pre_he, devp->prop.he,
		devp->prop.pre_vs, devp->prop.vs,
		devp->prop.pre_ve, devp->prop.ve);
	pr_info("frontend_fps:%d\n", devp->prop.fps);
	pr_info("frontend_colordepth:%d\n", devp->prop.colordepth);
	pr_info("source_bitdepth:%d\n", devp->source_bitdepth);
	pr_info("color_depth_config:%d\n", devp->color_depth_config);
	pr_info("color_depth_mode:%d\n", devp->color_depth_mode);
	pr_info("color_depth_support:0x%x\n", devp->color_depth_support);
	pr_info("cma_flag:0x%x\n", devp->cma_config_flag);
	pr_info("auto_cutwindow_en:%d\n", devp->auto_cutwindow_en);
	pr_info("auto_ratio_en:%d\n", devp->auto_ratio_en);
	pr_info("cma_mem_alloc:%d\n", devp->cma_mem_alloc);
	pr_info("cma_mem_size:0x%x\n", devp->cma_mem_size);
	pr_info("cma_mem_mode:%d\n", devp->cma_mem_mode);
	pr_info("force_yuv444_malloc:%d\n", devp->force_yuv444_malloc);
	vdin_dump_vf_state(devp->vfp);
	if (vf) {
		pr_info("current vframe index(%u):\n", vf->index);
		pr_info("\t buf(w%u, h%u),type(0x%x),flag(0x%x), duration(%d),",
		vf->width, vf->height, vf->type, vf->flag, vf->duration);
		pr_info("\t ratio_control(0x%x).\n", vf->ratio_control);
		pr_info("\t trans fmt %u, left_start_x %u,",
			vf->trans_fmt, vf->left_eye.start_x);
		pr_info("\t right_start_x %u, width_x %u\n",
			vf->right_eye.start_x, vf->left_eye.width);
		pr_info("\t left_start_y %u, right_start_y %u, height_y %u\n",
			vf->left_eye.start_y, vf->right_eye.start_y,
			vf->left_eye.height);
	}
	if (vfp) {
		pr_info("skip_vf_num:%d\n", vfp->skip_vf_num);
		pr_info("**************disp_mode**************\n");
		for (i = 0; i < VFRAME_DISP_MAX_NUM; i++)
			pr_info("[%d]:%-5d", i, vfp->disp_mode[i]);
		pr_info("\n**************disp_index**************\n");
		for (i = 0; i < VFRAME_DISP_MAX_NUM; i++)
			pr_info("[%d]:%-5d", i, vfp->disp_index[i]);
	}
	pr_info("\n current parameters:\n");
	pr_info("\t frontend of vdin index :  %d, 3d flag : 0x%x\n",
		curparm->index,  curparm->flag);
	pr_info("\t reserved 0x%x, devp->flags:0x%x\n",
		curparm->reserved, devp->flags);
	pr_info("max buffer num %u, msr_clk_val:%d.\n",
		devp->canvas_max_num, devp->msr_clk_val);
	pr_info("canvas buffer size %u, rdma_enable: %d.\n",
		devp->canvas_max_size, devp->rdma_enable);
	pr_info("range(%d),csc_cfg:0x%x,urgent_en:%d\n",
		devp->prop.color_fmt_range,
		devp->csc_cfg, devp->urgent_en);
	pr_info("black_bar_enable: %d, hist_bar_enable: %d, use_frame_rate: %d\n ",
		devp->black_bar_enable,
		devp->hist_bar_enable, devp->use_frame_rate);
	pr_info("vdin_irq_flag: %d, vdin_rest_flag: %d, irq_cnt: %d, rdma_irq_cnt: %d\n",
		devp->vdin_irq_flag, devp->vdin_reset_flag,
		devp->irq_cnt, devp->rdma_irq_cnt);
	pr_info("rdma_enable :  %d\n", devp->rdma_enable);
	pr_info("dolby_input :  %d\n", devp->dv.dolby_input);
	if ((devp->cma_config_en != 1) || !(devp->cma_config_flag & 0x100))
		pr_info("dolby_mem_start = %ld, dolby_mem_size = %d\n",
			(devp->mem_start +
			devp->mem_size - devp->canvas_max_num*dolby_size_byte),
			dolby_size_byte);
	else
		for (i = 0; i < devp->canvas_max_num; i++)
			pr_info("dolby_mem_start[%d] = %ld, dolby_mem_size = %d\n",
				i, (devp->vfmem_start[i] + devp->vfmem_size -
				dolby_size_byte), dolby_size_byte);
	for (i = 0; i < devp->canvas_max_num; i++) {
		pr_info("dv_mem(%d):0x%x\n",
			devp->vfp->dv_buf_size[i],
			devp->vfp->dv_buf_mem[i]);
	}
	pr_info("dv_flag:%d;dv_config:%d,dolby_vision:%d\n",
		devp->dv.dv_flag, devp->dv.dv_config, devp->prop.dolby_vision);
	pr_info("size of struct vdin_dev_s: %d\n", devp->vdin_dev_ssize);
	pr_info("Vdin driver version :  %s\n", VDIN_VER);
}

/*2018-07-18 add debugfs*/
struct vdin_dev_s *vdin_get_dev(unsigned int index);

/*same as vdin_dump_state*/
static int seq_file_vdin_state_show(struct seq_file *seq, void *v)
{
	struct vdin_dev_s *devp;
	unsigned int i;
	struct vframe_s *vf;
	struct tvin_parm_s *curparm;
	struct vf_pool *vfp;

	devp = vdin_get_dev(0);
	vf = &devp->curr_wr_vfe->vf;
	curparm = &devp->parm;
	vfp = devp->vfp;

	seq_printf(seq, "h_active = %d, v_active = %d\n",
		devp->h_active, devp->v_active);
	seq_printf(seq, "canvas_w = %d, canvas_h = %d\n",
		devp->canvas_w, devp->canvas_h);
	seq_printf(seq, "canvas_alin_w = %d, canvas_active_w = %d\n",
		devp->canvas_alin_w, devp->canvas_active_w);
	if ((devp->cma_config_en != 1) || !(devp->cma_config_flag & 0x1))
		seq_printf(seq, "mem_start = %ld, mem_size = %d\n",
			devp->mem_start, devp->mem_size);
	else
		for (i = 0; i < devp->canvas_max_num; i++)
			seq_printf(seq, "buf[%d]mem_start = %ld, mem_size = %d\n",
			i, devp->vfmem_start[i], devp->vfmem_size);
	seq_printf(seq, "signal format	= %s(0x%x)\n",
		tvin_sig_fmt_str(devp->parm.info.fmt),
		devp->parm.info.fmt);
	seq_printf(seq, "trans_fmt	= %s(%d)\n",
		tvin_trans_fmt_str(devp->prop.trans_fmt),
		devp->prop.trans_fmt);
	seq_printf(seq, "color_format	= %s(%d)\n",
		tvin_color_fmt_str(devp->prop.color_format),
		devp->prop.color_format);
	seq_printf(seq, "format_convert = %s(%d)\n",
		vdin_fmt_convert_str(devp->format_convert),
		devp->format_convert);
	seq_printf(seq, "aspect_ratio	= %s(%d)\ndecimation_ratio/dvi	= %u / %u\n",
		tvin_aspect_ratio_str(devp->prop.aspect_ratio),
		devp->prop.aspect_ratio,
		devp->prop.decimation_ratio, devp->prop.dvi_info);
	seq_printf(seq, "[pre->cur]:hs(%d->%d),he(%d->%d),vs(%d->%d),ve(%d->%d)\n",
		devp->prop.pre_hs, devp->prop.hs,
		devp->prop.pre_he, devp->prop.he,
		devp->prop.pre_vs, devp->prop.vs,
		devp->prop.pre_ve, devp->prop.ve);
	seq_printf(seq, "frontend_fps:%d\n", devp->prop.fps);
	seq_printf(seq, "frontend_colordepth:%d\n", devp->prop.colordepth);
	seq_printf(seq, "source_bitdepth:%d\n", devp->source_bitdepth);
	seq_printf(seq, "color_depth_config:%d\n", devp->color_depth_config);
	seq_printf(seq, "color_depth_mode:%d\n", devp->color_depth_mode);
	seq_printf(seq, "color_depth_support:0x%x\n",
		devp->color_depth_support);
	seq_printf(seq, "cma_flag:0x%x\n", devp->cma_config_flag);
	seq_printf(seq, "auto_cutwindow_en:%d\n", devp->auto_cutwindow_en);
	seq_printf(seq, "auto_ratio_en:%d\n", devp->auto_ratio_en);
	seq_printf(seq, "cma_mem_alloc:%d\n", devp->cma_mem_alloc);
	seq_printf(seq, "cma_mem_size:0x%x\n", devp->cma_mem_size);
	seq_printf(seq, "cma_mem_mode:%d\n", devp->cma_mem_mode);
	seq_printf(seq, "force_yuv444_malloc:%d\n", devp->force_yuv444_malloc);
	vdin_dump_vf_state_seq(devp->vfp, seq);
	if (vf) {
		seq_printf(seq, "current vframe index(%u):\n", vf->index);
		seq_printf(seq, "\t buf(w%u, h%u),type(0x%x),flag(0x%x), duration(%d),\n",
		vf->width, vf->height, vf->type, vf->flag, vf->duration);
		seq_printf(seq, "\t ratio_control(0x%x).\n", vf->ratio_control);
		seq_printf(seq, "\t trans fmt %u, left_start_x %u,\n",
			vf->trans_fmt, vf->left_eye.start_x);
		seq_printf(seq, "\t right_start_x %u, width_x %u\n",
			vf->right_eye.start_x, vf->left_eye.width);
		seq_printf(seq, "\t left_start_y %u, right_start_y %u, height_y %u\n",
			vf->left_eye.start_y, vf->right_eye.start_y,
			vf->left_eye.height);
	}
	if (vfp) {
		seq_printf(seq, "skip_vf_num:%d\n", vfp->skip_vf_num);
		seq_puts(seq, "**************disp_mode**************\n");
		for (i = 0; i < VFRAME_DISP_MAX_NUM; i++)
			seq_printf(seq, "[%d]:%-5d\n", i, vfp->disp_mode[i]);
		seq_puts(seq, "\n**************disp_index**************\n");
		for (i = 0; i < VFRAME_DISP_MAX_NUM; i++)
			seq_printf(seq, "[%d]:%-5d\n", i, vfp->disp_index[i]);
	}
	seq_puts(seq, "\n current parameters:\n");
	seq_printf(seq, "\t frontend of vdin index :  %d, 3d flag : 0x%x\n",
		curparm->index,  curparm->flag);
	seq_printf(seq, "\t reserved 0x%x, devp->flags:0x%x\n",
		curparm->reserved, devp->flags);
	seq_printf(seq, "max buffer num %u, msr_clk_val:%d.\n",
		devp->canvas_max_num, devp->msr_clk_val);
	seq_printf(seq, "canvas buffer size %u, rdma_enable: %d.\n",
		devp->canvas_max_size, devp->rdma_enable);
	seq_printf(seq, "range(%d),csc_cfg:0x%x,urgent_en:%d\n",
		devp->prop.color_fmt_range,
		devp->csc_cfg, devp->urgent_en);
	seq_printf(seq, "black_bar_enable: %d, hist_bar_enable: %d, use_frame_rate: %d\n ",
		devp->black_bar_enable,
		devp->hist_bar_enable, devp->use_frame_rate);
	seq_printf(seq, "vdin_irq_flag: %d, vdin_rest_flag: %d, irq_cnt: %d, rdma_irq_cnt: %d\n",
		devp->vdin_irq_flag, devp->vdin_reset_flag,
		devp->irq_cnt, devp->rdma_irq_cnt);
	seq_printf(seq, "rdma_enable :  %d\n", devp->rdma_enable);
	seq_printf(seq, "dolby_input :  %d\n", devp->dv.dolby_input);
	if ((devp->cma_config_en != 1) || !(devp->cma_config_flag & 0x100))
		seq_printf(seq, "dolby_mem_start = %ld, dolby_mem_size = %d\n",
			(devp->mem_start +
			devp->mem_size - devp->canvas_max_num*dolby_size_byte),
			dolby_size_byte);
	else
		for (i = 0; i < devp->canvas_max_num; i++)
			seq_printf(seq, "dolby_mem_start[%d] = %ld, dolby_mem_size = %d\n",
				i, (devp->vfmem_start[i] + devp->vfmem_size -
				dolby_size_byte), dolby_size_byte);
	for (i = 0; i < devp->canvas_max_num; i++) {
		seq_printf(seq, "dv_mem(%d):0x%x\n",
			devp->vfp->dv_buf_size[i],
			devp->vfp->dv_buf_mem[i]);
	}
	seq_printf(seq, "dv_flag:%d;dv_config:%d,dolby_vision:%d\n",
		devp->dv.dv_flag, devp->dv.dv_config, devp->prop.dolby_vision);
	seq_printf(seq, "size of struct vdin_dev_s: %d\n",
		devp->vdin_dev_ssize);
	seq_printf(seq, "Vdin driver version :  %s\n", VDIN_VER);

	return 0;
}

static void vdin_dump_histgram(struct vdin_dev_s *devp)
{
	uint i;
	pr_info("%s:\n", __func__);
	for (i = 0; i < 64; i++) {
		pr_info("[%d]0x%-8x\t", i, devp->parm.histgram[i]);
		if ((i+1)%8 == 0)
			pr_info("\n");
	}
}

static void vdin_write_mem(
	struct vdin_dev_s *devp, char *type,
	char *path, char *md_path)
{
	unsigned int size = 0, vtype = 0;
	struct file *filp = NULL,  *md_flip = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs;
	void *dts = NULL;
	long val;
	int index;
	unsigned long addr;
	struct vf_pool *p = devp->vfp;
	/* vtype = simple_strtol(type, NULL, 10); */

	if (kstrtol(type, 10, &val) < 0)
		return;

	vtype = val;
	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);
		if (!devp->curr_wr_vfe) {
			pr_info("no buffer to write.\n");
			return;
		}
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pr_info("bin file path = %s\n", path);
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		pr_info("read %s error.\n", path);
		return;
	}
	devp->curr_wr_vfe->vf.type = VIDTYPE_VIU_SINGLE_PLANE |
			VIDTYPE_VIU_FIELD | VIDTYPE_VIU_422;
	if (vtype == 1) {
		devp->curr_wr_vfe->vf.type |= VIDTYPE_INTERLACE_TOP;
		pr_info("current vframe type is top.\n");
	} else if (vtype == 3) {
		devp->curr_wr_vfe->vf.type |= VIDTYPE_INTERLACE_BOTTOM;
		pr_info("current vframe type is bottom.\n");
	} else {
		devp->curr_wr_vfe->vf.type |= VIDTYPE_PROGRESSIVE;
	}
	if (vtype == 6) {
		devp->curr_wr_vfe->vf.bitdepth = BITDEPTH_Y10;
		devp->curr_wr_vfe->vf.bitdepth |= FULL_PACK_422_MODE;
		devp->curr_wr_vfe->vf.signal_type = 0x91000;
		devp->curr_wr_vfe->vf.source_type = VFRAME_SOURCE_TYPE_HDMI;
	}
	if ((vtype == 7) || (vtype == 8)) {
		devp->curr_wr_vfe->vf.type = 0x7000;
		devp->curr_wr_vfe->vf.bitdepth = 0;
		devp->curr_wr_vfe->vf.signal_type = 0x10100;
		devp->curr_wr_vfe->vf.source_type = VFRAME_SOURCE_TYPE_HDMI;
	}
	addr = canvas_get_addr(devp->curr_wr_vfe->vf.canvas0Addr);
	/* dts = ioremap(canvas_get_addr(devp->curr_wr_vfe->vf.canvas0Addr), */
	/* real_size); */
	dts = phys_to_virt(addr);
	size = vfs_read(filp, dts, devp->canvas_max_size, &pos);
	pr_info("warning: %s read %u < %u\n",
			__func__, size, devp->canvas_max_size);

	vfs_fsync(filp, 0);
	iounmap(dts);
	filp_close(filp, NULL);
	set_fs(old_fs);
	if (vtype == 8) {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		pr_info("md file path = %s\n", md_path);
		md_flip = filp_open(md_path, O_RDONLY, 0);
		if (IS_ERR(md_flip)) {
			pr_info("read %s error.\n", md_path);
			return;
		}
		index = devp->curr_wr_vfe->vf.index & 0xff;
		if (index != 0xff && index >= 0
			&& index < p->size) {
			u8 *c = devp->vfp->dv_buf_ori[index];

			pos = 0;
			size = vfs_read(md_flip,
				devp->vfp->dv_buf_ori[index],
				4096, &pos);
			p->dv_buf_size[index] = size;
			devp->vfp->dv_buf[index] = &c[0];
		}
		vfs_fsync(md_flip, 0);
		filp_close(md_flip, NULL);
		set_fs(old_fs);
	}

	provider_vf_put(devp->curr_wr_vfe, devp->vfp);
	devp->curr_wr_vfe = NULL;
	vf_notify_receiver(devp->name, VFRAME_EVENT_PROVIDER_VFRAME_READY,
			NULL);
}

static void vdin_write_cont_mem(struct vdin_dev_s *devp, char *type,
	char *path, char *num)
{
	unsigned int size = 0, i = 0;
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs;
	void *dts = NULL;
	long field_num;
	unsigned long addr;

	/* vtype = simple_strtol(type, NULL, 10); */

	if (kstrtol(num, 10, &field_num) < 0)
		return;

	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);
		if (!devp->curr_wr_vfe) {
			pr_info("no buffer to write.\n");
			return;
		}
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pr_info("bin file path = %s\n", path);
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		pr_info("read %s error.\n", path);
		return;
	}
	for (i = 0; i < field_num; i++) {
		if (!devp->curr_wr_vfe) {
			devp->curr_wr_vfe = provider_vf_get(devp->vfp);
			if (!devp->curr_wr_vfe) {
				pr_info("no buffer to write.\n");
				return;
			}
		}
		if (!strcmp("top", type)) {
			if (i%2 == 0) {
				devp->curr_wr_vfe->vf.type |=
					VIDTYPE_INTERLACE_TOP;
				pr_info("current vframe type is top.\n");
			} else {
				devp->curr_wr_vfe->vf.type |=
					VIDTYPE_INTERLACE_BOTTOM;
				pr_info("current vframe type is bottom.\n");
			}
		} else if (!strcmp("bot", type)) {
			if (i%2 == 1) {
				devp->curr_wr_vfe->vf.type |=
					VIDTYPE_INTERLACE_TOP;
				pr_info("current vframe type is top.\n");
			} else {
				devp->curr_wr_vfe->vf.type |=
					VIDTYPE_INTERLACE_BOTTOM;
				pr_info("current vframe type is bottom.\n");
			}
		} else {
			devp->curr_wr_vfe->vf.type |= VIDTYPE_PROGRESSIVE;
		}
		addr = canvas_get_addr(devp->curr_wr_vfe->vf.canvas0Addr);
		/* real_size); */
		dts = phys_to_virt(addr);
		size = vfs_read(filp, dts, devp->canvas_max_size, &pos);
		if (size < devp->canvas_max_size) {
			pr_info("%s read %u < %u error.\n",
				__func__, size, devp->canvas_max_size);
			return;
		}
		vfs_fsync(filp, 0);
		provider_vf_put(devp->curr_wr_vfe, devp->vfp);
		devp->curr_wr_vfe = NULL;
		vf_notify_receiver(devp->name,
			VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
		iounmap(dts);
	}
	filp_close(filp, NULL);
	set_fs(old_fs);

}

static void dump_dolby_metadata(struct vdin_dev_s *devp)
{
	unsigned int i, j;

	pr_info("*****dolby_metadata(%d byte):*****\n", dolby_size_byte);
	for (i = 0; i < devp->canvas_max_num; i++) {
		pr_info("*****dolby_metadata(%d)[0x%x](%d byte):*****\n",
			i, devp->vfp->dv_buf_mem[i], dolby_size_byte);
		for (j = 0; j < dolby_size_byte; j++) {
			if ((j % 16) == 0)
				pr_info("\n");
			pr_info("0x%02x\t", *(devp->vfp->dv_buf_ori[i] + j));
		}
	}
}
static void dump_dolby_buf_clean(struct vdin_dev_s *devp)
{
	unsigned int i;

	for (i = 0; i < devp->canvas_max_num; i++)
		memset(devp->vfp->dv_buf_ori[i], 0, dolby_size_byte);
}
#ifdef CONFIG_AML_LOCAL_DIMMING

static void vdin_dump_histgram_ldim(struct vdin_dev_s *devp,
	unsigned int hnum, unsigned int vnum)
{
	uint i, j;
	unsigned int local_ldim_max[100] = {0};
	/*memcpy(&local_ldim_max[0], &vdin_ldim_max_global[0],
	 *100*sizeof(unsigned int));
	 */
	pr_info("%s:\n", __func__);
	for (i = 0; i < hnum; i++) {
		for (j = 0; j < vnum; j++) {
			pr_info("(%d,%d,%d)\t", local_ldim_max[j + i*10]&0x3ff,
			(local_ldim_max[j + i*10]>>10)&0x3ff,
			(local_ldim_max[j + i*10]>>20)&0x3ff);
		}
		pr_info("\n");
	}
}
#endif

/*
* 1.show the current frame rate
* echo fps >/sys/class/vdin/vdinx/attr
* 2.dump the data from vdin memory
* echo capture dir >/sys/class/vdin/vdinx/attr
* 3.start the vdin hardware
* echo tvstart/v4l2start port fmt_id/resolution(width height frame_rate) >dir
* 4.freeze the vdin buffer
* echo freeze/unfreeze >/sys/class/vdin/vdinx/attr
* 5.enable vdin0-nr path or vdin0-mem
* echo output2nr >/sys/class/vdin/vdin0/attr
* echo output2mem >/sys/class/vdin/vdin0/attr
* 6.modify for vdin fmt & color fmt conversion
* echo conversion w h cfmt >/sys/class/vdin/vdin0/attr
*/
static ssize_t vdin_attr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	unsigned int fps = 0;
	char ret = 0, *buf_orig, *parm[47] = {NULL};
	struct vdin_dev_s *devp;
	unsigned int time_start, time_end, time_delta;
	long val = 0;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	devp = dev_get_drvdata(dev);
	vdin_parse_param(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "fps", 3)) {
		if (devp->cycle)
			fps = (devp->msr_clk_val +
					(devp->cycle>>3))/devp->cycle;
		pr_info("%u f/s\n", fps);
		pr_info("hdmirx: %u f/s\n", devp->parm.info.fps);
	} else if (!strcmp(parm[0], "capture")) {
		if (parm[3] != NULL) {
			unsigned int start = 0, offset = 0;

			if (kstrtol(parm[2], 16, &val) == 0)
				start = val;
			if (kstrtol(parm[3], 16, &val) == 0)
				offset = val;
			dump_other_mem(parm[1], start, offset);
		} else if (parm[2] != NULL) {
			unsigned int buf_num = 0;

			if (kstrtol(parm[2], 10, &val) == 0)
				buf_num = val;
			vdin_dump_one_buf_mem(parm[1], devp, buf_num);
		} else if (parm[1] != NULL) {
			vdin_dump_mem(parm[1], devp);
		}
	} else if (!strcmp(parm[0], "tvstart")) {
		unsigned int port = 0, fmt = 0;

		if (!parm[2]) {
			kfree(buf_orig);
			return len;
		}
		if (kstrtol(parm[1], 16, &val) == 0)
			port = val;
		switch (port) {
		case 0:/* HDMI0 */
			port = TVIN_PORT_HDMI0;
			break;
		case 1:/* HDMI1 */
			port = TVIN_PORT_HDMI1;
			break;
		case 2:/* HDMI2 */
			port = TVIN_PORT_HDMI2;
			break;
		case 5:/* CVBS0 */
			port = TVIN_PORT_CVBS0;
			break;
		case 6:/* CVBS1 */
			port = TVIN_PORT_CVBS1;
			break;
		case 8:/* CVBS2 */
			port = TVIN_PORT_CVBS2;
			break;
		case 9:/* CVBS3 as atvdemod to cvd */
			port = TVIN_PORT_CVBS3;
			break;
		default:
			port = TVIN_PORT_CVBS0;
			break;
		}
		if (kstrtol(parm[2], 16, &val) == 0)
			fmt = val;

		/* devp->flags |= VDIN_FLAG_FS_OPENED; */
		/* request irq */
		snprintf(devp->irq_name, sizeof(devp->irq_name),
				"vdin%d-irq", devp->index);
		pr_info("vdin work in normal mode\n");
		ret = request_irq(devp->irq, vdin_isr, IRQF_SHARED,
				devp->irq_name, (void *)devp);

		/*disable irq until vdin is configured completely*/
		disable_irq_nosync(devp->irq);
		/*init queue*/
		init_waitqueue_head(&devp->queue);
		/* remove the hardware limit to vertical [0-max]*/
		/* WRITE_VCBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000fff); */
		/* pr_info("open device %s ok\n", dev_name(devp->dev)); */
		/* vdin_open_fe(port, 0, devp); */
		devp->parm.port = port;
		devp->parm.info.fmt = fmt;
		devp->fmt_info_p  = (struct tvin_format_s *)
				tvin_get_fmt_info(fmt);

		devp->unstable_flag = false;
		ret = vdin_open_fe(devp->parm.port, 0, devp);
		if (ret) {
			pr_err("TVIN_IOC_OPEN(%d) failed to open port 0x%x\n",
					devp->index, devp->parm.port);
		}
		devp->flags |= VDIN_FLAG_DEC_OPENED;
		pr_info("TVIN_IOC_OPEN() port %s opened ok\n\n",
			tvin_port_str(devp->parm.port));

		time_start = jiffies_to_msecs(jiffies);
start_chk:
		if (devp->flags & VDIN_FLAG_DEC_STARTED) {
			pr_info("TVIN_IOC_START_DEC() port 0x%x, started already\n",
					devp->parm.port);
		}
		time_end = jiffies_to_msecs(jiffies);
		time_delta = time_end - time_start;
		if (((devp->parm.info.status != TVIN_SIG_STATUS_STABLE) ||
				(devp->parm.info.fmt == TVIN_SIG_FMT_NULL)) &&
				(time_delta <= 5000))
			goto start_chk;

		pr_info("status: %s, fmt: %s\n",
			tvin_sig_status_str(devp->parm.info.status),
			tvin_sig_fmt_str(devp->parm.info.fmt));
		if (devp->parm.info.fmt != TVIN_SIG_FMT_NULL)
			fmt = devp->parm.info.fmt; /* = parm.info.fmt; */
		devp->fmt_info_p  =
				(struct tvin_format_s *)tvin_get_fmt_info(fmt);
		/* devp->fmt_info_p  =
		 * tvin_get_fmt_info(devp->parm.info.fmt);
		 */
		if (!devp->fmt_info_p) {
			pr_info("TVIN_IOC_START_DEC(%d) error, fmt is null\n",
					devp->index);
		}
		vdin_start_dec(devp);
		devp->flags |= VDIN_FLAG_DEC_STARTED;
		pr_info("TVIN_IOC_START_DEC port %s, decode started ok\n\n",
				tvin_port_str(devp->parm.port));
	} else if (!strcmp(parm[0], "tvstop")) {
		vdin_stop_dec(devp);
		vdin_close_fe(devp);
		/* devp->flags &= (~VDIN_FLAG_FS_OPENED); */
		devp->flags &= (~VDIN_FLAG_DEC_STARTED);
		/* free irq */
		free_irq(devp->irq, (void *)devp);
		/* reset the hardware limit to vertical [0-1079]  */
		/* WRITE_VCBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000437); */
	} else if (!strcmp(parm[0], "v4l2stop")) {
		stop_tvin_service(devp->index);
		devp->flags &= (~VDIN_FLAG_V4L2_DEBUG);
	} else if (!strcmp(parm[0], "v4l2start")) {
		struct vdin_parm_s param;
		if (!parm[4]) {
			pr_err("usage: echo v4l2start port width height");
			pr_err("fps cfmt > /sys/class/vdin/vdinx/attr.\n");
			pr_err("port mybe bt656 or viuin,");
			pr_err("fps the frame rate of input.\n");
			kfree(buf_orig);
			return len;
		}
		memset(&param, 0, sizeof(struct vdin_parm_s));
		/*parse the port*/
		if (!strcmp(parm[1], "bt656")) {
			param.port = TVIN_PORT_CAMERA;
			pr_info(" port is TVIN_PORT_CAMERA\n");
		} else if (!strcmp(parm[1], "viuin")) {
			param.port = TVIN_PORT_VIU1;
			pr_info(" port is TVIN_PORT_VIU\n");
		} else if (!strcmp(parm[1], "video")) {
			param.port = TVIN_PORT_VIU1_VIDEO;
			pr_info(" port is TVIN_PORT_VIU_VIDEO\n");
		} else if (!strcmp(parm[1], "viu_wb0_vd1")) {
			param.port = TVIN_PORT_VIU1_WB0_VD1;
			pr_info(" port is TVIN_PORT_VIU_WB0_VD1\n");
		} else if (!strcmp(parm[1], "viu_wb0_vd2")) {
			param.port = TVIN_PORT_VIU1_WB0_VD2;
			pr_info(" port is TVIN_PORT_VIU_WB0_VD2\n");
		} else if (!strcmp(parm[1], "viu_wb0_osd1")) {
			param.port = TVIN_PORT_VIU1_WB0_OSD1;
			pr_info(" port is TVIN_PORT_VIU_WB0_OSD1\n");
		} else if (!strcmp(parm[1], "viu_wb0_osd2")) {
			param.port = TVIN_PORT_VIU1_WB0_OSD2;
			pr_info(" port is TVIN_PORT_VIU_WB0_OSD2\n");
		} else if (!strcmp(parm[1], "viu_wb0_post_blend")) {
			param.port = TVIN_PORT_VIU1_WB0_POST_BLEND;
			pr_info(" port is TVIN_PORT_VIU_WB0_POST_BLEND\n");
		} else if (!strcmp(parm[1], "viu_wb1_vd1")) {
			param.port = TVIN_PORT_VIU1_WB1_VD1;
			pr_info(" port is TVIN_PORT_VIU_WB1_VD1\n");
		} else if (!strcmp(parm[1], "viu_wb1_vd2")) {
			param.port = TVIN_PORT_VIU1_WB1_VD2;
			pr_info(" port is TVIN_PORT_VIU_WB1_VD2\n");
		} else if (!strcmp(parm[1], "viu_wb1_osd1")) {
			param.port = TVIN_PORT_VIU1_WB1_OSD1;
			pr_info(" port is TVIN_PORT_VIU_WB1_OSD1\n");
		} else if (!strcmp(parm[1], "viu_wb0_osd2")) {
			param.port = TVIN_PORT_VIU1_WB1_OSD2;
			pr_info(" port is TVIN_PORT_VIU_WB1_OSD2\n");
		} else if (!strcmp(parm[1], "viu_wb1_post_blend")) {
			param.port = TVIN_PORT_VIU1_WB1_POST_BLEND;
			pr_info(" port is TVIN_PORT_VIU_WB1_POST_BLEND\n");
		} else if (!strcmp(parm[1], "viuin2")) {
			param.port = TVIN_PORT_VIU2;
			pr_info(" port is TVIN_PORT_VIU\n");
		} else if (!strcmp(parm[1], "video2")) {
			param.port = TVIN_PORT_VIU2_VIDEO;
			pr_info(" port is TVIN_PORT_VIU_VIDEO\n");
		} else if (!strcmp(parm[1], "viu2_wb0_vd1")) {
			param.port = TVIN_PORT_VIU2_WB0_VD1;
			pr_info(" port is TVIN_PORT_VIU_WB0_VD1\n");
		} else if (!strcmp(parm[1], "viu2_wb0_vd2")) {
			param.port = TVIN_PORT_VIU2_WB0_VD2;
			pr_info(" port is TVIN_PORT_VIU_WB0_VD2\n");
		} else if (!strcmp(parm[1], "viu2_wb0_osd1")) {
			param.port = TVIN_PORT_VIU2_WB0_OSD1;
			pr_info(" port is TVIN_PORT_VIU_WB0_OSD1\n");
		} else if (!strcmp(parm[1], "viu2_wb0_osd2")) {
			param.port = TVIN_PORT_VIU2_WB0_OSD2;
			pr_info(" port is TVIN_PORT_VIU_WB0_OSD2\n");
		} else if (!strcmp(parm[1], "viu2_wb0_post_blend")) {
			param.port = TVIN_PORT_VIU2_WB0_POST_BLEND;
			pr_info(" port is TVIN_PORT_VIU_WB0_POST_BLEND\n");
		} else if (!strcmp(parm[1], "viu2_wb1_vd1")) {
			param.port = TVIN_PORT_VIU2_WB1_VD1;
			pr_info(" port is TVIN_PORT_VIU_WB1_VD1\n");
		} else if (!strcmp(parm[1], "viu2_wb1_vd2")) {
			param.port = TVIN_PORT_VIU2_WB1_VD2;
			pr_info(" port is TVIN_PORT_VIU_WB1_VD2\n");
		} else if (!strcmp(parm[1], "viu2_wb1_osd1")) {
			param.port = TVIN_PORT_VIU2_WB1_OSD1;
			pr_info(" port is TVIN_PORT_VIU_WB1_OSD1\n");
		} else if (!strcmp(parm[1], "viu2_wb0_osd2")) {
			param.port = TVIN_PORT_VIU2_WB1_OSD2;
			pr_info(" port is TVIN_PORT_VIU_WB1_OSD2\n");
		} else if (!strcmp(parm[1], "viu2_wb1_post_blend")) {
			param.port = TVIN_PORT_VIU2_WB1_POST_BLEND;
			pr_info(" port is TVIN_PORT_VIU_WB1_POST_BLEND\n");
		} else if (!strcmp(parm[1], "isp")) {
			param.port = TVIN_PORT_ISP;
			pr_info(" port is TVIN_PORT_ISP\n");
		}
		/*parse the resolution*/
		if (kstrtol(parm[2], 10, &val) == 0)
			param.h_active = val;
		if (kstrtol(parm[3], 10, &val) == 0)
			param.v_active = val;
		if (kstrtol(parm[4], 10, &val) == 0)
			param.frame_rate = val;
		pr_info(" hactive:%d,vactive:%d, rate:%d\n",
				param.h_active,
				param.v_active,
				param.frame_rate);
		if (!parm[5])
			param.cfmt = TVIN_YUV422;
		else if (kstrtol(parm[5], 10, &val) == 0)
			param.cfmt = val;
		pr_info(" cfmt:%d\n", param.cfmt);
		if (!parm[6])
			param.dfmt = TVIN_YUV422;
		else if (kstrtol(parm[6], 10, &val) == 0)
			param.dfmt = val;
		pr_info(" dfmt:%d\n", param.dfmt);
		if (!parm[7])
			param.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
		else if (kstrtol(parm[7], 10, &val) == 0)
			param.scan_mode = val;
		pr_info(" scan_mode:%d\n", param.scan_mode);

		param.fmt = TVIN_SIG_FMT_MAX;
		devp->flags |= VDIN_FLAG_V4L2_DEBUG;
		/* param.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE; */
		/*start the vdin hardware*/
		start_tvin_service(devp->index, &param);
	} else if (!strcmp(parm[0], "disablesm"))
		del_timer_sync(&devp->timer);
	else if (!strcmp(parm[0], "freeze")) {
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
			pr_info("vdin not started,can't do freeze!!!\n");
		else if (devp->fmt_info_p->scan_mode ==
			TVIN_SCAN_MODE_PROGRESSIVE)
			vdin_vf_freeze(devp->vfp, 1);
		else
			vdin_vf_freeze(devp->vfp, 2);

	} else if (!strcmp(parm[0], "unfreeze")) {
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
			pr_info("vdin not started,can't do unfreeze!!!\n");
		else
			vdin_vf_unfreeze(devp->vfp);
	} else if (!strcmp(parm[0], "conversion")) {
		if (parm[1] && parm[2] && parm[3]) {
			if (kstrtoul(parm[1], 10, &val) == 0)
				devp->debug.scaler4w = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				devp->debug.scaler4h = val;
			if (kstrtoul(parm[3], 10, &val) == 0)
				devp->debug.dest_cfmt = val;

			devp->flags |= VDIN_FLAG_MANUAL_CONVERSION;
			pr_info("enable manual conversion w = %u h = %u ",
				devp->debug.scaler4w, devp->debug.scaler4h);
			pr_info("dest_cfmt = %s.\n",
				tvin_color_fmt_str(devp->debug.dest_cfmt));
		} else {
			devp->flags &= (~VDIN_FLAG_MANUAL_CONVERSION);
			pr_info("disable manual conversion w = %u h = %u ",
				devp->debug.scaler4w, devp->debug.scaler4h);
			pr_info("dest_cfmt = %s\n",
				tvin_color_fmt_str(devp->debug.dest_cfmt));
		}
	} else if (!strcmp(parm[0], "state")) {
		vdin_dump_state(devp);
	} else if (!strcmp(parm[0], "histgram")) {
		vdin_dump_histgram(devp);
#ifdef CONFIG_AML_LOCAL_DIMMING
	} else if (!strcmp(parm[0], "histgram_ldim")) {
		unsigned int hnum, vnum;
		if (parm[1] && parm[2]) {
			hnum = kstrtoul(parm[1], 10, (unsigned long *)&hnum);
			vnum = kstrtoul(parm[2], 10, (unsigned long *)&vnum);
		} else {
			hnum = 8;
			vnum = 2;
		}
		vdin_dump_histgram_ldim(devp, hnum, vnum);
#endif
	} else if (!strcmp(parm[0], "force_recycle")) {
		devp->flags |= VDIN_FLAG_FORCE_RECYCLE;
	} else if (!strcmp(parm[0], "read_pic")) {
		if (parm[1] && parm[2])
			vdin_write_mem(devp, parm[1], parm[2], parm[3]);
		else
			pr_err("miss parameters .\n");
	} else if (!strcmp(parm[0], "read_bin")) {
		if (parm[1] && parm[2] && parm[3])
			vdin_write_cont_mem(devp, parm[1], parm[2], parm[3]);
		else
			pr_err("miss parameters .\n");
	} else if (!strcmp(parm[0], "dump_reg")) {
		unsigned int reg;
		unsigned int offset = devp->addr_offset;
		pr_info("vdin%d addr offset:0x%x regs start----\n",
				devp->index, offset);
		for (reg = VDIN_SCALE_COEF_IDX;	reg <= 0x1273; reg++)
			pr_info("[0x%x]reg:0x%x-0x%x\n",
					(0xd0100000 + ((reg+offset)<<2)),
					(reg+offset), rd(offset, reg));
		pr_info("vdin%d regs   end----\n", devp->index);
	} else if (!strcmp(parm[0], "rgb_xy")) {
		unsigned int x = 0, y = 0;

		if (parm[1] && parm[2]) {
			if (kstrtoul(parm[1], 10, &val) == 0)
				x = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				y = val;
			vdin_set_prob_xy(devp->addr_offset, x, y, devp);
		} else
			pr_err("miss parameters .\n");
	} else if (!strcmp(parm[0], "rgb_info")) {
		unsigned int r, g, b;
		vdin_get_prob_rgb(devp->addr_offset, &r, &g, &b);
		pr_info("rgb_info-->r:%d,g:%d,b:%d\n", r, g, b);
	} else if (!strcmp(parm[0], "mpeg2vdin")) {
		if (parm[1] && parm[2]) {
			if (kstrtoul(parm[1], 10, &val) == 0)
				devp->h_active = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				devp->v_active = val;
			vdin_set_mpegin(devp);
			pr_info("mpeg2vdin:h_active:%d,v_active:%d\n",
				devp->h_active, devp->v_active);
		} else
			pr_err("miss parameters .\n");
	} else if (!strcmp(parm[0], "yuv_rgb_info")) {
		unsigned int rgb_yuv0, rgb_yuv1, rgb_yuv2;
		vdin_get_vdin_yuv_rgb_mat0(devp->addr_offset,
				&rgb_yuv0, &rgb_yuv1, &rgb_yuv2);
		pr_info("rgb_yuv0 :%d, rgb_yuv1 :%d , rgb_yuv2 :%d\n",
				rgb_yuv0, rgb_yuv1, rgb_yuv2);
	} else if (!strcmp(parm[0], "mat0_xy")) {
		unsigned int x = 0, y = 0;

		if (parm[1] && parm[2]) {
			if (kstrtoul(parm[1], 10, &val) == 0)
				x = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				y = val;
			pr_info("pos x  :%d, pos y  :%d\n", x, y);
			vdin_set_prob_matrix0_xy(devp->addr_offset, x, y, devp);
		} else
			pr_err("miss parameters .\n");
	} else if (!strcmp(parm[0], "mat0_set")) {
		unsigned int x = 0;

		if (!parm[1])
			pr_err("miss parameters .\n");
		if (kstrtoul(parm[1], 10, &val) == 0)
			x = val;
		pr_info("matrix set : %d\n", x);
		vdin_set_before_after_mat0(devp->addr_offset, x, devp);
	} else if (!strcmp(parm[0], "hdr")) {
		int i;
		struct vframe_master_display_colour_s *prop;
		prop = &devp->curr_wr_vfe->vf.prop.master_display_colour;
		pr_info("present_flag: %d\n", prop->present_flag);
		for (i = 0; i < 6; i++)
			pr_info("primaries %d: %#x\n", i,
				*(((u32 *)(prop->primaries)) + i));

		pr_info("white point x: %#x, y: %#x\n",
				*((u32 *)(prop->white_point)),
				*((u32 *)(prop->white_point) + 1));
		pr_info("lumi max: %#x, min: %#x\n",
				*((u32 *)(prop->luminance)),
				*(((u32 *)(prop->luminance)) + 1));
	} else if (!strcmp(parm[0], "snowon")) {
		unsigned int fmt;
		fmt = TVIN_SIG_FMT_CVBS_NTSC_M;
		devp->flags |= VDIN_FLAG_SNOW_FLAG;
		devp->flags |= VDIN_FLAG_SM_DISABLE;
		if (devp->flags & VDIN_FLAG_DEC_STARTED)
			pr_info("TVIN_IOC_START_DEC() TVIN_PORT_CVBS3, started already\n");
		else {
			devp->parm.info.fmt = fmt;
			devp->fmt_info_p  =
				(struct tvin_format_s *)tvin_get_fmt_info(fmt);
			vdin_start_dec(devp);
			devp->flags |= VDIN_FLAG_DEC_STARTED;
			pr_info("TVIN_IOC_START_DEC port TVIN_PORT_CVBS3, decode started ok\n\n");
		}
		devp->flags &= (~VDIN_FLAG_SM_DISABLE);
		/*tvafe_snow_config(1);*/
		/*tvafe_snow_config_clamp(1);*/
		pr_info("snowon config done!!\n");
	} else if (!strcmp(parm[0], "snowoff")) {
		devp->flags &= (~VDIN_FLAG_SNOW_FLAG);
		devp->flags |= VDIN_FLAG_SM_DISABLE;
		/*tvafe_snow_config(0);*/
		/*tvafe_snow_config_clamp(0);*/
		/* if fmt change, need restart dec vdin */
		if ((devp->parm.info.fmt != TVIN_SIG_FMT_CVBS_NTSC_M) &&
			(devp->parm.info.fmt != TVIN_SIG_FMT_NULL)) {
			if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
				pr_err("VDIN_FLAG_DEC_STARTED(%d) decode havn't started\n",
				devp->index);
			else {
				devp->flags |= VDIN_FLAG_DEC_STOP_ISR;
				vdin_stop_dec(devp);
				devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR;
				devp->flags &= (~VDIN_FLAG_DEC_STARTED);
				pr_info("VDIN_FLAG_DEC_STARTED(%d) port %s, decode stop ok\n",
					devp->parm.index,
					tvin_port_str(devp->parm.port));
			}
			if (devp->flags & VDIN_FLAG_DEC_STARTED)
				pr_info("VDIN_FLAG_DEC_STARTED() TVIN_PORT_CVBS3, started already\n");
			else {
				devp->fmt_info_p  =
				(struct tvin_format_s *)tvin_get_fmt_info(
				devp->parm.info.fmt);
				vdin_start_dec(devp);
				devp->flags |= VDIN_FLAG_DEC_STARTED;
				pr_info("VDIN_FLAG_DEC_STARTED port TVIN_PORT_CVBS3, decode started ok\n");
			}
		}
		devp->flags &= (~VDIN_FLAG_SM_DISABLE);
		pr_info("snowoff config done!!\n");
	} else if (!strcmp(parm[0], "vf_reg")) {
		if ((devp->flags & VDIN_FLAG_DEC_REGED)
			== VDIN_FLAG_DEC_REGED)
			pr_err("vf_reg(%d) decoder is registered already\n",
				devp->index);
		else {
			devp->flags |= VDIN_FLAG_DEC_REGED;
			vdin_vf_reg(devp);
			pr_info("vf_reg(%d) ok\n\n", devp->index);
		}
	} else if (!strcmp(parm[0], "vf_unreg")) {
		if ((devp->flags & VDIN_FLAG_DEC_REGED)
			!= VDIN_FLAG_DEC_REGED)
			pr_err("vf_unreg(%d) decoder isn't registered\n",
				devp->index);
		else {
			devp->flags &= (~VDIN_FLAG_DEC_REGED);
			vdin_vf_unreg(devp);
			pr_info("vf_unreg(%d) ok\n\n", devp->index);
		}
	} else if (!strcmp(parm[0], "pause_dec")) {
		vdin_pause_dec(devp);
		pr_info("pause_dec(%d) ok\n\n", devp->index);
	} else if (!strcmp(parm[0], "resume_dec")) {
		vdin_resume_dec(devp);
		pr_info("resume_dec(%d) ok\n\n", devp->index);
	} else if (!strcmp(parm[0], "color_depth")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->color_depth_config = val;
			pr_info("color_depth(%d):%d\n\n", devp->index,
				devp->color_depth_config);
		}
	} else if (!strcmp(parm[0], "color_depth_support")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 16, &val) == 0) {
			devp->color_depth_support = val;
			pr_info("color_depth_support(%d):%d\n\n", devp->index,
				devp->color_depth_support);
		}
	} else if (!strcmp(parm[0], "color_depth_mode")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->color_depth_mode = val;
			pr_info("color_depth_mode(%d):%d\n\n", devp->index,
				devp->color_depth_mode);
		}
	} else if (!strcmp(parm[0], "auto_cutwindow_en")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->auto_cutwindow_en = val;
			pr_info("auto_cutwindow_en(%d):%d\n\n", devp->index,
				devp->auto_cutwindow_en);
		}
	} else if (!strcmp(parm[0], "auto_ratio_en")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->auto_ratio_en = val;
			pr_info("auto_ratio_en(%d):%d\n\n", devp->index,
				devp->auto_ratio_en);
		}
	} else if (!strcmp(parm[0], "dolby_config")) {
		vdin_dolby_config(devp);
		pr_info("dolby_config done\n");
	} else if (!strcmp(parm[0], "metadata")) {
		dump_dolby_metadata(devp);
		pr_info("dolby_config done\n");
	} else if (!strcmp(parm[0], "clean_dv")) {
		dump_dolby_buf_clean(devp);
		pr_info("clean dolby vision mem done\n");
	} else if (!strcmp(parm[0], "dv_debug")) {
		vdin_dv_debug(devp);
	} else if (!strcmp(parm[0], "channel_order_config")) {
		unsigned int c0, c1, c2;

		if (!parm[3])
			pr_info("miss parameters\n");
		else {
			c0 = c1 = c2 = 0;
			if (kstrtoul(parm[1], 10, &val) == 0)
				c0 = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				c1 = val;
			if (kstrtoul(parm[3], 10, &val) == 0)
				c2 = val;
			vdin_channel_order_config(devp->addr_offset,
				c0, c1, c2);
		}
	} else if (!strcmp(parm[0], "channel_order_status"))
		vdin_channel_order_status(devp->addr_offset);
	else if (!strcmp(parm[0], "open_port")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 16, &val) == 0) {
			devp->parm.index = 0;
			devp->parm.port  = val;
			devp->unstable_flag = false;
			ret = vdin_open_fe(devp->parm.port,
				devp->parm.index, devp);
			if (ret) {
				pr_err("TVIN_IOC_OPEN(%d) failed to open port 0x%x\n",
					devp->parm.index, devp->parm.port);
			} else {
				devp->flags |= VDIN_FLAG_DEC_OPENED;
				pr_info("TVIN_IOC_OPEN(%d) port %s opened ok\n\n",
				devp->parm.index,
				tvin_port_str(devp->parm.port));
			}
		}
	} else if (!strcmp(parm[0], "close_port")) {
		enum tvin_port_e port = devp->parm.port;

		if (!(devp->flags & VDIN_FLAG_DEC_OPENED))
			pr_err("TVIN_IOC_CLOSE(%d) you have not opened port\n",
					devp->index);
		else {
			vdin_close_fe(devp);
			devp->flags &= (~VDIN_FLAG_DEC_OPENED);
			pr_info("TVIN_IOC_CLOSE(%d) port %s closed ok\n\n",
				devp->parm.index, tvin_port_str(port));
		}
	} else if (!strcmp(parm[0], "prehsc_en")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->prehsc_en = val;
			pr_info("prehsc_en(%d):%d\n\n", devp->index,
				devp->prehsc_en);
		}
	} else if (!strcmp(parm[0], "vshrk_en")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->vshrk_en = val;
			pr_info("vshrk_en(%d):%d\n\n", devp->index,
				devp->vshrk_en);
		}
	} else if (!strcmp(parm[0], "cma_mem_mode")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->cma_mem_mode = val;
			pr_info("cma_mem_mode(%d):%d\n\n", devp->index,
				devp->cma_mem_mode);
		}
	} else if (!strcmp(parm[0], "black_bar_enable")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->black_bar_enable = val;
			pr_info("black_bar_enable(%d):%d\n\n", devp->index,
				devp->black_bar_enable);
		}
	} else if (!strcmp(parm[0], "hist_bar_enable")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->hist_bar_enable = val;
			pr_info("hist_bar_enable(%d):%d\n\n", devp->index,
					devp->hist_bar_enable);
		}
	} else if (!strcmp(parm[0], "use_frame_rate")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->use_frame_rate = val;
			pr_info("use_frame_rate(%d):%d\n\n", devp->index,
					devp->use_frame_rate);
		}
	} else if (!strcmp(parm[0], "dolby_input")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->dv.dolby_input = val;
			pr_info("dolby_input(%d):%d\n\n", devp->index,
				devp->dv.dolby_input);
		}
	} else if (!strcmp(parm[0], "rdma_enable")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->rdma_enable = val;
			pr_info("rdma_enable (%d):%d\n", devp->index,
				devp->rdma_enable);
		}
	} else if (!strcmp(parm[0], "urgent_en")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->urgent_en = val;
			pr_info("urgent_en (%d):%d\n", devp->index,
				devp->urgent_en);
		}
	} else if (!strcmp(parm[0], "irq_flag")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->vdin_irq_flag = val;
			pr_info("vdin(%d) irq_flag: %d\n", devp->index,
						devp->vdin_irq_flag);
		}
	} else if (!strcmp(parm[0], "skip_vf_num")) {
		if (!parm[1])
			pr_err("miss parameters .\n");
		else if ((kstrtoul(parm[1], 10, &val) == 0) && (devp->vfp)) {
			devp->vfp->skip_vf_num = val;
			if (val == 0)
				memset(devp->vfp->disp_mode, 0,
					(sizeof(enum vframe_disp_mode_e) *
					VFRAME_DISP_MAX_NUM));
			pr_info("vframe_skip(%d):%d\n\n", devp->index,
				devp->vfp->skip_vf_num);
		}
	} else {
		pr_info("unknown command\n");
	}
	kfree(buf_orig);
	return len;
}
static DEVICE_ATTR(attr, 0664, vdin_attr_show, vdin_attr_store);
#ifdef VF_LOG_EN
static ssize_t vdin_vf_log_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct vf_log_s *log = &devp->vfp->log;

	len += sprintf(buf + len, "%d of %d\n", log->log_cur, VF_LOG_LEN);
	return len;
}

static ssize_t vdin_vf_log_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct vdin_dev_s *devp = dev_get_drvdata(dev);

	if (!strncmp(buf, "start", 5)) {
		vf_log_init(devp->vfp);
	} else if (!strncmp(buf, "print", 5)) {
		vf_log_print(devp->vfp);
	} else {
		pr_info("unknown command :  %s\n"
			"Usage:\n"
			"a. show log message:\n"
			"echo print > / sys/class/vdin/vdin0/vf_log\n"
			"b. restart log message:\n"
			"echo start > / sys/class/vdin/vdin0/vf_log\n"
			"c. show log records\n"
			"cat > / sys/class/vdin/vdin0/vf_log\n", buf);
	}
	return count;
}

/*
	1. show log length.
	cat /sys/class/vdin/vdin0/vf_log
	cat /sys/class/vdin/vdin1/vf_log
	2. clear log buffer and start log.
	echo start > /sys/class/vdin/vdin0/vf_log
	echo start > /sys/class/vdin/vdin1/vf_log
	3. print log
	echo print > /sys/class/vdin/vdin0/vf_log
	echo print > /sys/class/vdin/vdin1/vf_log
*/
static DEVICE_ATTR(vf_log, 0664, vdin_vf_log_show, vdin_vf_log_store);
#endif /* VF_LOG_EN */

#ifdef ISR_LOG_EN
static ssize_t vdin_isr_log_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 len = 0;
	struct vdin_dev_s *vdevp;
	vdevp = dev_get_drvdata(dev);
		len += sprintf(buf + len, "%d of %d\n",
				vdevp->vfp->isr_log.log_cur, ISR_LOG_LEN);
	return len;
}
/*
*1. show isr log length.
*cat /sys/class/vdin/vdin0/vf_log
*2. clear isr log buffer and start log.
*echo start > /sys/class/vdin/vdinx/isr_log
*3. print isr log
*echo print > /sys/class/vdin/vdinx/isr_log
*/
static ssize_t vdin_isr_log_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct vdin_dev_s *vdevp;
	vdevp = dev_get_drvdata(dev);
	if (!strncmp(buf, "start", 5))
		isr_log_init(vdevp->vfp);
	else if (!strncmp(buf, "print", 5))
		isr_log_print(vdevp->vfp);
	return count;
}

static DEVICE_ATTR(isr_log,  0664, vdin_isr_log_show, vdin_isr_log_store);
#endif

static ssize_t vdin_crop_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct tvin_cutwin_s *crop = &devp->debug.cutwin;

	len += sprintf(buf+len,
		"hs_offset %u, he_offset %u, vs_offset %u, ve_offset %u\n",
		crop->hs, crop->he, crop->vs, crop->ve);
	return len;
}

static ssize_t vdin_crop_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	char *parm[4] = {NULL}, *buf_orig;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct tvin_cutwin_s *crop = &devp->debug.cutwin;
	long val;
	ssize_t ret_ext = count;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	vdin_parse_param(buf_orig, parm);

	if (!parm[3]) {
		ret_ext = -EINVAL;
		pr_info("miss param!!\n");
	} else {
		if (kstrtol(parm[0], 10, &val) == 0)
			crop->hs = val;
		if (kstrtol(parm[1], 10, &val) == 0)
			crop->he = val;
		if (kstrtol(parm[2], 10, &val) == 0)
			crop->vs = val;
		if (kstrtol(parm[3], 10, &val) == 0)
			crop->ve = val;
	}

	kfree(buf_orig);

	pr_info("hs_offset %u, he_offset %u, vs_offset %u, ve_offset %u.\n",
				crop->hs, crop->he, crop->vs, crop->ve);
	return ret_ext;
}

static DEVICE_ATTR(crop, 0664, vdin_crop_show, vdin_crop_store);

static ssize_t vdin_cm2_show(struct device *dev,
	     struct device_attribute *attr,
					     char *buf)
{
	int len = 0;
	struct vdin_dev_s *devp;
	unsigned int addr_port = VDIN_CHROMA_ADDR_PORT;
	unsigned int data_port = VDIN_CHROMA_DATA_PORT;

	devp = dev_get_drvdata(dev);
	if (devp->addr_offset != 0) {
		addr_port = VDIN_CHROMA_ADDR_PORT + devp->addr_offset;
		data_port = VDIN_CHROMA_DATA_PORT + devp->addr_offset;
	}

	len += sprintf(buf+len, "addr_port[0x%x] data_port[0x%x]\n",
		addr_port, data_port);
	len += sprintf(buf+len, "Usage:");
	len += sprintf(buf+len, "echo wm addr data0 data1 data2 data3 data4 >");
	len += sprintf(buf+len, "/sys/class/vdin/vdin0/cm2\n");
	len += sprintf(buf+len, "echo rm addr > / sys/class/vdin/vdin0/cm2\n");
	len += sprintf(buf+len, "echo wm addr data0 data1 data2 data3 data4 >");
	len += sprintf(buf+len, "/sys/class/vdin/vdin1/cm2\n");
	len += sprintf(buf+len, "echo rm addr > / sys/class/vdin/vdin1/cm2\n");
	return len;
}

static ssize_t vdin_cm2_store(struct device *dev,
	      struct device_attribute *attr,
		   const char *buffer, size_t count)
{
	struct vdin_dev_s *devp;
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[7];
	u32 addr = 0;
	int data[5] = {0};
	unsigned int addr_port = VDIN_CHROMA_ADDR_PORT;
	unsigned int data_port = VDIN_CHROMA_DATA_PORT;
	long val;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	strcat(delim1, delim2);

	devp = dev_get_drvdata(dev);
	if (devp->addr_offset != 0) {
		addr_port = VDIN_CHROMA_ADDR_PORT + devp->addr_offset;
		data_port = VDIN_CHROMA_DATA_PORT + devp->addr_offset;
	}
	buf_orig = kstrdup(buffer, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (n == 0) {
		pr_info("parm not initialized.\n");
		kfree(buf_orig);
		return count;
	}
	if ((parm[0][0] == 'w') && parm[0][1] == 'm') {
		if (n != 7) {
			pr_info("read : invalid parameter\n");
			pr_info("please : cat / sys/class/vdin/vdin0/cm2\n");
			kfree(buf_orig);
			return count;
		}
		/* addr = simple_strtol(parm[1], NULL, 16); */
		if (kstrtol(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		addr = val;
		addr = addr - addr%8;
		if (kstrtol(parm[2], 16, &val) == 0)
			data[0] = val;
		if (kstrtol(parm[3], 16, &val) == 0)
			data[1] = val;
		if (kstrtol(parm[4], 16, &val) == 0)
			data[2] = val;
		if (kstrtol(parm[5], 16, &val) == 0)
			data[3] = val;
		if (kstrtol(parm[6], 16, &val) == 0)
			data[4] = val;
		aml_write_vcbus(addr_port, addr);
		aml_write_vcbus(data_port, data[0]);
		aml_write_vcbus(addr_port, addr + 1);
		aml_write_vcbus(data_port, data[1]);
		aml_write_vcbus(addr_port, addr + 2);
		aml_write_vcbus(data_port, data[2]);
		aml_write_vcbus(addr_port, addr + 3);
		aml_write_vcbus(data_port, data[3]);
		aml_write_vcbus(addr_port, addr + 4);
		aml_write_vcbus(data_port, data[4]);

		pr_info("wm[0x%x]  0x0\n", addr);
	} else if ((parm[0][0] == 'r') && parm[0][1] == 'm') {
		if (n != 2) {
			pr_info("read : invalid parameter\n");
			pr_info("please : cat / sys/class/vdin/vdin0/cm2\n");
			kfree(buf_orig);
			return count;
		}
		if (kstrtol(parm[1], 16, &val) == 0)
			addr = val;
		addr = addr - addr%8;
		aml_write_vcbus(addr_port, addr);
		data[0] = aml_read_vcbus(data_port);
		data[0] = aml_read_vcbus(data_port);
		data[0] = aml_read_vcbus(data_port);
		aml_write_vcbus(addr_port, addr+1);
		data[1] = aml_read_vcbus(data_port);
		data[1] = aml_read_vcbus(data_port);
		data[1] = aml_read_vcbus(data_port);
		aml_write_vcbus(addr_port, addr+2);
		data[2] = aml_read_vcbus(data_port);
		data[2] = aml_read_vcbus(data_port);
		data[2] = aml_read_vcbus(data_port);
		aml_write_vcbus(addr_port, addr+3);
		data[3] = aml_read_vcbus(data_port);
		data[3] = aml_read_vcbus(data_port);
		data[3] = aml_read_vcbus(data_port);
		aml_write_vcbus(addr_port, addr+4);
		data[4] = aml_read_vcbus(data_port);
		data[4] = aml_read_vcbus(data_port);
		data[4] = aml_read_vcbus(data_port);

		pr_info("rm:[0x%x] : data[0x%x][0x%x][0x%x][0x%x][0x%x]\n",
			addr, data[0], data[1], data[2], data[3], data[4]);
	} else if (!strcmp(parm[0], "enable")) {
		wr_bits(devp->addr_offset, VDIN_CM_BRI_CON_CTRL, 1,
				CM_TOP_EN_BIT, CM_TOP_EN_WID);
	} else if (!strcmp(parm[0], "disable")) {
		wr_bits(devp->addr_offset, VDIN_CM_BRI_CON_CTRL, 0,
				CM_TOP_EN_BIT, CM_TOP_EN_WID);
	} else {
		pr_info("invalid command\n");
		pr_info("please : cat / sys/class/vdin/vdin0/bit");
	}
	kfree(buf_orig);
	return count;

}

static DEVICE_ATTR(cm2, 0644, vdin_cm2_show, vdin_cm2_store);

int vdin_create_device_files(struct device *dev)
{
	int ret = 0;
	ret = device_create_file(dev, &dev_attr_sig_det);
	/* create sysfs attribute files */
	#ifdef VF_LOG_EN
	ret = device_create_file(dev, &dev_attr_vf_log);
	#endif
	#ifdef ISR_LOG_EN
	ret = device_create_file(dev, &dev_attr_isr_log);
	#endif
	ret = device_create_file(dev, &dev_attr_attr);
	ret = device_create_file(dev, &dev_attr_cm2);
#if 0
	ret = device_create_file(dev, &dev_attr_debug_for_isp);
#endif
	ret = device_create_file(dev, &dev_attr_crop);
	return ret;
}
void vdin_remove_device_files(struct device *dev)
{
	#ifdef VF_LOG_EN
	device_remove_file(dev, &dev_attr_vf_log);
	#endif
	#ifdef ISR_LOG_EN
	device_remove_file(dev, &dev_attr_isr_log);
	#endif
	device_remove_file(dev, &dev_attr_attr);
	device_remove_file(dev, &dev_attr_cm2);
#if 0
	device_remove_file(dev, &dev_attr_debug_for_isp);
#endif
	device_remove_file(dev, &dev_attr_crop);
	device_remove_file(dev, &dev_attr_sig_det);
}

#ifdef DEBUG_SUPPORT
static int memp = MEMP_DCDR_WITHOUT_3D;
static char *memp_str(int profile)
{
	switch (profile) {
	case MEMP_VDIN_WITHOUT_3D:
		return "vdin without 3d";
	case MEMP_VDIN_WITH_3D:
		return "vdin with 3d";
	case MEMP_DCDR_WITHOUT_3D:
		return "decoder without 3d";
	case MEMP_DCDR_WITH_3D:
		return "decoder with 3d";
	case MEMP_ATV_WITHOUT_3D:
		return "atv without 3d";
	case MEMP_ATV_WITH_3D:
		return "atv with 3d";
	default:
		return "unknown";
	}
}
/*
* cat /sys/class/vdin/memp
*/
static ssize_t memp_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int len = 0;
	len += sprintf(buf+len, "%d %s\n", memp, memp_str(memp));
	return len;
}
/*
* echo 0|1|2|3|4|5 > /sys/class/vdin/memp
*/
static void memp_set(int type)
{
	switch (type) {
	case MEMP_VDIN_WITHOUT_3D:
	case MEMP_VDIN_WITH_3D:
		aml_write_vcbus(VPU_VDIN_ASYNC_HOLD_CTRL, 0x80408040);
		aml_write_vcbus(VPU_VDISP_ASYNC_HOLD_CTRL, 0x80408040);
		aml_write_vcbus(VPU_VPUARB2_ASYNC_HOLD_CTRL, 0x80408040);
		aml_write_vcbus(VPU_VD1_MMC_CTRL,
			aml_read_vcbus(VPU_VD1_MMC_CTRL) | (1<<12));
		 /* arb0 */
		aml_write_vcbus(VPU_VD2_MMC_CTRL,
			aml_read_vcbus(VPU_VD2_MMC_CTRL) | (1<<12));
		  /* arb0 */
		aml_write_vcbus(VPU_DI_IF1_MMC_CTRL,
			aml_read_vcbus(VPU_DI_IF1_MMC_CTRL) | (1<<12));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_MEM_MMC_CTRL,
			aml_read_vcbus(VPU_DI_MEM_MMC_CTRL) & (~(1<<12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_INP_MMC_CTRL,
			aml_read_vcbus(VPU_DI_INP_MMC_CTRL) & (~(1<<12)));
		 /* arb0 */
		aml_write_vcbus(VPU_DI_MTNRD_MMC_CTRL,
			aml_read_vcbus(VPU_DI_MTNRD_MMC_CTRL) | (1<<12));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_CHAN2_MMC_CTRL,
			aml_read_vcbus(VPU_DI_CHAN2_MMC_CTRL) & (~(1<<12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_MTNWR_MMC_CTRL,
			aml_read_vcbus(VPU_DI_MTNWR_MMC_CTRL) & (~(1<<12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_NRWR_MMC_CTRL,
			aml_read_vcbus(VPU_DI_NRWR_MMC_CTRL) & (~(1<<12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_DIWR_MMC_CTRL,
			aml_read_vcbus(VPU_DI_DIWR_MMC_CTRL) & (~(1<<12)));
		memp = type;
		break;
	case MEMP_DCDR_WITHOUT_3D:
	case MEMP_DCDR_WITH_3D:
		aml_write_vcbus(VPU_VDIN_ASYNC_HOLD_CTRL, 0x80408040);
		 /* arb0 */
		aml_write_vcbus(VPU_VD1_MMC_CTRL,
			aml_read_vcbus(VPU_VD1_MMC_CTRL) | (1<<12));
		/* arb0 */
		aml_write_vcbus(VPU_VD2_MMC_CTRL,
			aml_read_vcbus(VPU_VD2_MMC_CTRL) | (1<<12));
		/* arb0 */
		aml_write_vcbus(VPU_DI_IF1_MMC_CTRL,
			aml_read_vcbus(VPU_DI_IF1_MMC_CTRL) | (1<<12));
		  /* arb1 */
		aml_write_vcbus(VPU_DI_MEM_MMC_CTRL,
			aml_read_vcbus(VPU_DI_MEM_MMC_CTRL) & (~(1<<12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_INP_MMC_CTRL,
			aml_read_vcbus(VPU_DI_INP_MMC_CTRL) & (~(1<<12)));
		/* arb0 */
		aml_write_vcbus(VPU_DI_MTNRD_MMC_CTRL,
			aml_read_vcbus(VPU_DI_MTNRD_MMC_CTRL) | (1<<12));
		/* arb1 */
		aml_write_vcbus(VPU_DI_CHAN2_MMC_CTRL,
			aml_read_vcbus(VPU_DI_CHAN2_MMC_CTRL) & (~(1<<12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_MTNWR_MMC_CTRL,
			aml_read_vcbus(VPU_DI_MTNWR_MMC_CTRL) & (~(1<<12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_NRWR_MMC_CTRL,
			aml_read_vcbus(VPU_DI_NRWR_MMC_CTRL) & (~(1<<12)));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_DIWR_MMC_CTRL,
			aml_read_vcbus(VPU_DI_DIWR_MMC_CTRL) & (~(1<<12)));
		memp = type;
		break;
	case MEMP_ATV_WITHOUT_3D:
	case MEMP_ATV_WITH_3D:
		/* arb0 */
		aml_write_vcbus(VPU_VD1_MMC_CTRL,
			aml_read_vcbus(VPU_VD1_MMC_CTRL) | (1<<12));
		/* arb0 */
		aml_write_vcbus(VPU_VD2_MMC_CTRL,
			aml_read_vcbus(VPU_VD2_MMC_CTRL) | (1<<12));
		 /* arb0 */
		aml_write_vcbus(VPU_DI_IF1_MMC_CTRL,
			aml_read_vcbus(VPU_DI_IF1_MMC_CTRL) | (1<<12));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_MEM_MMC_CTRL,
			aml_read_vcbus(VPU_DI_MEM_MMC_CTRL) & (~(1<<12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_INP_MMC_CTRL,
			aml_read_vcbus(VPU_DI_INP_MMC_CTRL) & (~(1<<12)));
		/* arb0 */
		aml_write_vcbus(VPU_DI_MTNRD_MMC_CTRL,
			aml_read_vcbus(VPU_DI_MTNRD_MMC_CTRL) | (1<<12));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_CHAN2_MMC_CTRL,
			aml_read_vcbus(VPU_DI_CHAN2_MMC_CTRL) & (~(1<<12)));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_MTNWR_MMC_CTRL,
			aml_read_vcbus(VPU_DI_MTNWR_MMC_CTRL) & (~(1<<12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_NRWR_MMC_CTRL,
			aml_read_vcbus(VPU_DI_NRWR_MMC_CTRL) & (~(1<<12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_DIWR_MMC_CTRL,
			aml_read_vcbus(VPU_DI_DIWR_MMC_CTRL) & (~(1<<12)));
		/* arb2 */
		aml_write_vcbus(VPU_TVD3D_MMC_CTRL,
			aml_read_vcbus(VPU_TVD3D_MMC_CTRL) | (1<<14));
		/* urgent */
		aml_write_vcbus(VPU_TVD3D_MMC_CTRL,
			aml_read_vcbus(VPU_TVD3D_MMC_CTRL) | (1<<15));
		/* arb2 */
		aml_write_vcbus(VPU_TVDVBI_MMC_CTRL,
			aml_read_vcbus(VPU_TVDVBI_MMC_CTRL) | (1<<14));
		 /* urgent */
		aml_write_vcbus(VPU_TVD3D_MMC_CTRL,
			aml_read_vcbus(VPU_TVD3D_MMC_CTRL) | (1<<15));
		memp = type;
		break;
	default:
		/* @todo */
		break;
	}
}

static ssize_t memp_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	/* int type = simple_strtol(buf, NULL, 10); */
	long type;

	if (kstrtol(buf, 10, &type) == 0)
		memp_set(type);
	else
		return -EINVAL;

	return count;
}

static CLASS_ATTR(memp, 0644, memp_show, memp_store);

int vdin_create_class_files(struct class *vdin_clsp)
{
	int ret = 0;
	ret = class_create_file(vdin_clsp, &class_attr_memp);
	return ret;
}
void vdin_remove_class_files(struct class *vdin_clsp)
{
	class_remove_file(vdin_clsp, &class_attr_memp);
}


#endif

/*2018-07-18 add debugfs*/
#define DEFINE_SHOW_VDIN(__name) \
static int __name ## _open(struct inode *inode, struct file *file)	\
{ \
	return single_open(file, __name ## _show, inode->i_private);	\
} \
									\
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.read = seq_read,		\
	.llseek = seq_lseek,		\
	.release = single_release,	\
}

DEFINE_SHOW_VDIN(seq_file_vdin_state);

struct vdin_debugfs_files_t {
	const char *name;
	const umode_t mode;
	const struct file_operations *fops;
};

static struct vdin_debugfs_files_t vdin_debugfs_files[] = {
	{"state", S_IFREG | 0644, &seq_file_vdin_state_fops},

};


void vdin_debugfs_init(struct vdin_dev_s *vdevp)
{
	int i;
	struct dentry *ent;
	unsigned int nub;

	nub = vdevp->index;

	if (nub > 0) {
		pr_info("%s only support debug vdin0 %d\n", __func__, nub);
		return;
	}

	if (vdevp->dbg_root)
		return;

	vdevp->dbg_root = debugfs_create_dir("vdin0", NULL);

	if (!vdevp->dbg_root) {
		pr_err("can't create debugfs dir di\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(vdin_debugfs_files); i++) {
		ent = debugfs_create_file(vdin_debugfs_files[i].name,
			vdin_debugfs_files[i].mode,
			vdevp->dbg_root, NULL,
			vdin_debugfs_files[i].fops);
		if (!ent)
			pr_err("debugfs create failed\n");
	}

}
void vdin_debugfs_exit(struct vdin_dev_s *vdevp)
{
	unsigned int nub;

	nub = vdevp->index;
	if (nub > 0) {
		pr_info("%s only support debug vdin0 %d\n", __func__, nub);
		return;
	}

	debugfs_remove(vdevp->dbg_root);
}
/*------------------------------------------*/

