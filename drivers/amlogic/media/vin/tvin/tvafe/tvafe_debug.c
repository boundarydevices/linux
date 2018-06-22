/*
 * drivers/amlogic/media/vin/tvin/tvafe/tvafe_debug.c
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
#include "../tvin_global.h"
#include "tvafe_regs.h"
#include "tvafe_debug.h"
#include "tvafe.h"

bool disableapi;
bool force_stable;

static void tvafe_state(struct tvafe_dev_s *devp)
{
	struct tvin_parm_s *parm = &devp->tvafe.parm;
	struct tvafe_cvd2_s *cvd2 = &devp->tvafe.cvd2;
	struct tvin_info_s *tvin_info = &parm->info;
	struct tvafe_cvd2_info_s *cvd2_info = &cvd2->info;
	struct tvafe_cvd2_lines_s *vlines = &cvd2_info->vlines;
	struct tvafe_cvd2_hw_data_s *hw = &cvd2->hw;
	/* top dev info */
	tvafe_pr_info("\n!!tvafe_dev_s info:\n");
	tvafe_pr_info("size of tvafe_dev_s:%d\n", devp->sizeof_tvafe_dev_s);
	tvafe_pr_info("tvafe_dev_s->index:%d\n", devp->index);
	tvafe_pr_info("tvafe_dev_s->flags:0x%x\n", devp->flags);
	tvafe_pr_info("tvafe_dev_s->cma_config_en:%d\n",
		devp->cma_config_en);
	tvafe_pr_info("tvafe_dev_s->cma_config_flag:0x%x\n",
		devp->cma_config_flag);
	tvafe_pr_info("tvafe_dev_s->frame_skip_enable:%d\n",
		devp->frame_skip_enable);
	#ifdef CONFIG_CMA
	tvafe_pr_info("devp->cma_mem_size:0x%x\n", devp->cma_mem_size);
	tvafe_pr_info("devp->cma_mem_alloc:%d\n", devp->cma_mem_alloc);
	#endif
	tvafe_pr_info("tvafe_info_s->aspect_ratio:%d\n",
		devp->tvafe.aspect_ratio);
	tvafe_pr_info("tvafe_info_s->aspect_ratio_last:%d\n",
		devp->tvafe.aspect_ratio_last);
	tvafe_pr_info("tvafe_info_s->aspect_ratio_cnt:%d\n",
		devp->tvafe.aspect_ratio_cnt);
	/* tvafe_dev_s->tvin_parm_s struct info */
	tvafe_pr_info("\n!!tvafe_dev_s->tvin_parm_s struct info:\n");
	tvafe_pr_info("tvin_parm_s->index:%d\n", parm->index);
	tvafe_pr_info("tvin_parm_s->port:0x%x\n", parm->port);
	tvafe_pr_info("tvin_parm_s->hist_pow:0x%x\n", parm->hist_pow);
	tvafe_pr_info("tvin_parm_s->luma_sum:0x%x\n", parm->luma_sum);
	tvafe_pr_info("tvin_parm_s->pixel_sum:0x%x\n", parm->pixel_sum);
	tvafe_pr_info("tvin_parm_s->flag:0x%x\n", parm->flag);
	tvafe_pr_info("tvin_parm_s->dest_width:0x%x\n", parm->dest_width);
	tvafe_pr_info("tvin_parm_s->dest_height:0x%x\n", parm->dest_height);
	tvafe_pr_info("tvin_parm_s->h_reverse:%d\n", parm->h_reverse);
	tvafe_pr_info("tvin_parm_s->v_reverse:%d\n", parm->v_reverse);
	/* tvafe_dev_s->tvafe_cvd2_s struct info */
	tvafe_pr_info("\n!!tvafe_dev_s->tvafe_cvd2_s struct info:\n");
	tvafe_pr_info("tvafe_cvd2_s->config_fmt:0x%x\n", cvd2->config_fmt);
	tvafe_pr_info("tvafe_cvd2_s->manual_fmt:0x%x\n", cvd2->manual_fmt);
	tvafe_pr_info("tvafe_cvd2_s->vd_port:0x%x\n", cvd2->vd_port);
	tvafe_pr_info("tvafe_cvd2_s->cvd2_init_en:%d\n", cvd2->cvd2_init_en);
	tvafe_pr_info("tvafe_cvd2_s->nonstd_detect_dis:%d\n",
		cvd2->nonstd_detect_dis);
	/* tvin_parm_s->tvin_info_s struct info */
	tvafe_pr_info("\n!!tvin_parm_s->tvin_info_s struct info:\n");
	tvafe_pr_info("tvin_info_s->trans_fmt:0x%x\n", tvin_info->trans_fmt);
	tvafe_pr_info("tvin_info_s->fmt:0x%x\n", tvin_info->fmt);
	tvafe_pr_info("tvin_info_s->status:0x%x\n", tvin_info->status);
	tvafe_pr_info("tvin_info_s->cfmt:%d\n", tvin_info->cfmt);
	tvafe_pr_info("tvin_info_s->fps:%d\n", tvin_info->fps);
	/* tvafe_cvd2_s->tvafe_cvd2_info_s struct info */
	tvafe_pr_info("\n!!tvafe_cvd2_s->tvafe_cvd2_info_s struct info:\n");
	tvafe_pr_info("tvafe_cvd2_info_s->state:0x%x\n", cvd2_info->state);
	tvafe_pr_info("tvafe_cvd2_info_s->state_cnt:%d\n",
		cvd2_info->state_cnt);
	tvafe_pr_info("tvafe_cvd2_info_s->non_std_enable:%d\n",
		cvd2_info->non_std_enable);
	tvafe_pr_info("tvafe_cvd2_info_s->non_std_config:%d\n",
		cvd2_info->non_std_config);
	tvafe_pr_info("tvafe_cvd2_info_s->non_std_worst:%d\n",
		cvd2_info->non_std_worst);
	tvafe_pr_info("tvafe_cvd2_info_s->adc_reload_en:%d\n",
		cvd2_info->adc_reload_en);
	tvafe_pr_info("tvafe_cvd2_info_s->hs_adj_en:%d\n",
		cvd2_info->hs_adj_en);
	tvafe_pr_info("tvafe_cvd2_info_s->hs_adj_dir:%d\n",
		cvd2_info->hs_adj_dir);
	tvafe_pr_info("tvafe_cvd2_info_s->hs_adj_level:%d\n",
		cvd2_info->hs_adj_level);
	tvafe_pr_info("tvafe_cvd2_info_s->vs_adj_level:%d\n",
		cvd2_info->vs_adj_level);
	tvafe_pr_info("tvafe_cvd2_info_s->vs_adj_en:%d\n",
		cvd2_info->vs_adj_en);
#ifdef TVAFE_SET_CVBS_CDTO_EN
	tvafe_pr_info("tvafe_cvd2_info_s->hcnt64[0]:0x%x\n",
		cvd2_info->hcnt64[0]);
	tvafe_pr_info("tvafe_cvd2_info_s->hcnt64[1]:0x%x\n",
		cvd2_info->hcnt64[1]);
	tvafe_pr_info("tvafe_cvd2_info_s->hcnt64[2]:0x%x\n",
		cvd2_info->hcnt64[2]);
	tvafe_pr_info("tvafe_cvd2_info_s->hcnt64[3]:0x%x\n",
		cvd2_info->hcnt64[3]);
#endif
	/* tvafe_cvd2_info_s->tvafe_cvd2_lines_s struct info */
	tvafe_pr_info("\n!!tvafe_cvd2_info_s->tvafe_cvd2_lines_s struct info:\n");
	tvafe_pr_info("tvafe_cvd2_lines_s->check_cnt:0x%x\n",
		vlines->check_cnt);
	tvafe_pr_info("tvafe_cvd2_lines_s->de_offset:0x%x\n",
		vlines->de_offset);
	tvafe_pr_info("tvafe_cvd2_lines_s->val[0]:0x%x\n",
		vlines->val[0]);
	tvafe_pr_info("tvafe_cvd2_lines_s->val[1]:0x%x\n",
		vlines->val[1]);
	tvafe_pr_info("tvafe_cvd2_lines_s->val[2]:0x%x\n",
		vlines->val[2]);
	tvafe_pr_info("tvafe_cvd2_lines_s->val[3]:0x%x\n",
		vlines->val[3]);
	/* tvafe_cvd2_s->tvafe_cvd2_hw_data_s struct info */
	tvafe_pr_info("\n!!tvafe_cvd2_s->tvafe_cvd2_hw_data_s struct info:\n");
	tvafe_pr_info("tvafe_cvd2_hw_data_s->no_sig:%d\n", hw->no_sig);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->h_lock:%d\n", hw->h_lock);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->v_lock:%d\n", hw->v_lock);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->h_nonstd:%d\n", hw->h_nonstd);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->v_nonstd:%d\n", hw->v_nonstd);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->no_color_burst:%d\n",
		hw->no_color_burst);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->comb3d_off:%d\n",
		hw->comb3d_off);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->chroma_lock:%d\n",
		hw->chroma_lock);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->pal:%d\n", hw->pal);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->secam:%d\n", hw->secam);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->line625:%d\n", hw->line625);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->noisy:%d\n", hw->noisy);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->vcr:%d\n", hw->vcr);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->vcrtrick:%d\n", hw->vcrtrick);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->vcrff:%d\n", hw->vcrff);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->vcrrew:%d\n", hw->vcrrew);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->noisy:%d\n", hw->noisy);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->acc4xx_cnt:%d\n",
		hw->acc4xx_cnt);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->acc425_cnt:%d\n",
		hw->acc425_cnt);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->acc3xx_cnt:%d\n",
		hw->acc3xx_cnt);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->acc358_cnt:%d\n",
		hw->acc358_cnt);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->secam_detected:%d\n",
		hw->secam_detected);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->secam_phase:%d\n",
		hw->secam_phase);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->fsc_358:%d\n", hw->fsc_358);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->fsc_425:%d\n", hw->fsc_425);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->fsc_443:%d\n", hw->fsc_443);
}

static void tvafe_parse_param(char *buf_orig, char **parm)
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

/*default only one tvafe ,echo cvdfmt pali/palm/ntsc/secam >dir*/
static ssize_t tvafe_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t count)
{
	unsigned char fmt_index = 0;

	struct tvafe_dev_s *devp;
	unsigned long tmp = 0;
	char *buf_orig, *parm[47] = {NULL};
	long val;

	devp = dev_get_drvdata(dev);
	if (!buff)
		return count;
	buf_orig = kstrdup(buff, GFP_KERNEL);
	tvafe_parse_param(buf_orig, (char **)&parm);

	if (!strncmp(buff, "cvdfmt", strlen("cvdfmt"))) {
		fmt_index = strlen("cvdfmt") + 1;
		if (!strncmp(buff+fmt_index, "ntscm", strlen("ntscm")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_NTSC_M;
		else if (!strncmp(buff+fmt_index,
					"ntsc443", strlen("ntsc443")))
			devp->tvafe.cvd2.manual_fmt =
					TVIN_SIG_FMT_CVBS_NTSC_443;
		else if (!strncmp(buff+fmt_index, "pali", strlen("pali")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_PAL_I;
		else if (!strncmp(buff+fmt_index, "palm", strlen("plam")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_PAL_M;
		else if (!strncmp(buff+fmt_index, "pal60", strlen("pal60")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_PAL_60;
		else if (!strncmp(buff+fmt_index, "palcn", strlen("palcn")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_PAL_CN;
		else if (!strncmp(buff+fmt_index, "secam", strlen("secam")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_SECAM;
		else if (!strncmp(buff+fmt_index, "null", strlen("null")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_NULL;
		else
			tvafe_pr_info("%s:invaild command.", buff);
	} else if (!strncmp(buff, "disableapi", strlen("disableapi"))) {
		if (kstrtoul(buff+strlen("disableapi")+1, 10, &tmp) == 0)
			disableapi = tmp;

	} else if (!strncmp(buff, "force_stable", strlen("force_stable"))) {
		if (kstrtoul(buff+strlen("force_stable")+1, 10, &tmp) == 0)
			force_stable = tmp;
	} else if (!strncmp(buff, "tvafe_enable", strlen("tvafe_enable"))) {
		if (kstrtoul(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (val) {
			tvafe_enable_module(true);
			devp->flags &= (~TVAFE_POWERDOWN_IN_IDLE);
			tvafe_pr_info("%s:tvafe enable\n", __func__);
		} else {
			devp->flags |= TVAFE_POWERDOWN_IN_IDLE;
			tvafe_enable_module(false);
			tvafe_pr_info("%s:tvafe down\n", __func__);
		}
	} else if (!strncmp(buff, "afe_ver", strlen("afe_ver"))) {
		tvafe_pr_info("tvafe version :  %s\n", TVAFE_VER);
	} else if (!strncmp(buff, "snowon", strlen("snowon"))) {
		if (kstrtoul(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (val) {
			tvafe_snow_config(1);
			tvafe_snow_config_clamp(1);
			devp->flags |= TVAFE_FLAG_DEV_SNOW_FLAG;
			tvafe_snow_function_flag = true;
			tvafe_pr_info("%s:tvafe snowon\n", __func__);
		} else {
			tvafe_snow_config(0);
			tvafe_snow_config_clamp(0);
			devp->flags &= (~TVAFE_FLAG_DEV_SNOW_FLAG);
			tvafe_pr_info("%s:tvafe snowoff\n", __func__);
		}
	} else if (!strcmp(parm[0], "frame_skip_enable")) {
		if (kstrtoul(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		devp->frame_skip_enable = val;
		tvafe_pr_info("frame_skip_enable:%d\n",
			devp->frame_skip_enable);
	} else if (!strncmp(buff, "state", strlen("state"))) {
		tvafe_state(devp);
	} else if (!strncmp(buff, "nonstd_detect_dis",
		strlen("nonstd_detect_dis"))) {
		/*patch for Very low probability hanging issue on atv close*/
		/*only appeared in one project,this for reserved debug*/
		/*default setting to disable the nonstandard signal detect*/
		if (kstrtoul(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (val) {
			devp->tvafe.cvd2.nonstd_detect_dis = true;
			pr_info("[tvafe..]%s:disable nonstd detect\n",
				__func__);
		} else {
			devp->tvafe.cvd2.nonstd_detect_dis = false;
			pr_info("[tvafe..]%s:enable nonstd detect\n",
				__func__);
		}
	} else if (!strncmp(buff, "rf_ntsc50_en", strlen("rf_ntsc50_en"))) {
		if (kstrtoul(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (val) {
			tvafe_cvd2_rf_ntsc50_en(true);
			pr_info("[tvafe..]%s:tvafe_cvd2_rf_ntsc50_en\n",
				__func__);
		} else {
			tvafe_cvd2_rf_ntsc50_en(false);
			pr_info("[tvafe..]%s:tvafe_cvd2_rf_ntsc50_dis\n",
				__func__);
		}
	} else
		tvafe_pr_info("[%s]:invaild command.\n", __func__);
	kfree(buf_orig);
	return count;
}

static ssize_t tvafe_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf+len,
		"echo cvdfmt ntsc/ntsc443/pali/palm/palcn/pal60/secam/null > /sys/class/tvafe/tvafe0/debug;conifg manual fmt\n");
	len += sprintf(buf+len,
		"echo disableapi val(d) > /sys/class/tvafe/tvafe0/debug;enable/ignore api cmd\n");
	len += sprintf(buf+len,
		"echo force_stable val(d) > /sys/class/tvafe/tvafe0/debug;force stable or not force\n");
	len += sprintf(buf+len,
		"echo tvafe_enable val(d) > /sys/class/tvafe/tvafe0/debug;tvafe enable/disable\n");
	len += sprintf(buf+len,
		"echo afe_ver > /sys/class/tvafe/tvafe0/debug;show tvafe version\n");
	len += sprintf(buf+len,
		"echo snow val(d) > /sys/class/tvafe/tvafe0/debug;snow on/off\n");
	len += sprintf(buf+len,
		"echo frame_skip_enable val(d) > /sys/class/tvafe/tvafe0/debug;frame skip enable/disable\n");
	len += sprintf(buf+len,
		"echo state > /sys/class/tvafe/tvafe0/debug;show tvafe status\n");
	return len;
}

static DEVICE_ATTR(debug, 0644, tvafe_show, tvafe_store);

/**************************************************/
/*before to excute the func of tvafe_dumpmem_store ,*/
/*it must be setting the steps below:*/
/*echo wa 0x30b2 4c > /sys/class/register/reg*/
/*echo wa 0x3122 3000000 > /sys/class/register/reg*/
/*echo wa 0x3096 0 > /sys/class/register/reg*/
/*echo wa 0x3150 2 > /sys/class/register/reg*/

/*echo wa 0x3151 11b40000 > /sys/class/register/reg   //start mem adr*/
/*echo wa 0x3152 11bf61f0 > /sys/class/register/reg	//end mem adr*/
/*echo wa 0x3150 3 > /sys/class/register/reg*/
/*echo wa 0x3150 1 > /sys/class/register/reg*/

/*the steps above set  the cvd that  will  write the signal data to mem*/
/**************************************************/
static ssize_t tvafe_dumpmem_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t len)
{
	unsigned int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[6] = {NULL};
	struct tvafe_dev_s *devp;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	strcat(delim1, delim2);
	if (!buff)
		return len;
	buf_orig = kstrdup(buff, GFP_KERNEL);
	/* printk(KERN_INFO "input cmd : %s",buf_orig); */
	devp = dev_get_drvdata(dev);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (!strncmp(parm[0], "dumpmem", strlen("dumpmem"))) {
		if (parm[1] != NULL) {
			struct file *filp = NULL;
			loff_t pos = 0;
			void *buf = NULL;
/* unsigned int canvas_real_size = devp->canvas_h * devp->canvas_w; */
			mm_segment_t old_fs = get_fs();

			set_fs(KERNEL_DS);
			filp = filp_open(parm[1], O_RDWR|O_CREAT, 0666);
			if (IS_ERR(filp)) {
				tvafe_pr_err("create %s error.\n",
					parm[1]);
				kfree(buf_orig);
				return len;
			}
			if (devp->cma_config_flag == 1)
				buf = codec_mm_phys_to_virt(devp->mem.start);
			else
				buf = phys_to_virt(devp->mem.start);
			vfs_write(filp, buf, devp->mem.size, &pos);
			tvafe_pr_info("write buffer %2d of %s.\n",
				devp->mem.size, parm[1]);
			tvafe_pr_info("devp->mem.start   %x .\n",
				devp->mem.start);
			vfs_fsync(filp, 0);
			filp_close(filp, NULL);
			set_fs(old_fs);
		}
	}
	kfree(buf_orig);
	return len;

}

static DEVICE_ATTR(dumpmem, 0200, NULL, tvafe_dumpmem_store);

/*echo n >/sys/class/tvafe/tvafe0/cvd_reg8a set register of cvd2*/
static ssize_t tvafe_cvd_reg8a_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t count)
{
	unsigned int n;
	struct tvafe_dev_s *devp;

	devp = dev_get_drvdata(dev);
	if (kstrtouint(buff, 10, &n) != 0)
		return -1;
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		tvafe_pr_err("tvafe havn't opened, error!!!\n");
		return count;
	}
	tvafe_cvd2_set_reg8a(n);
	tvafe_pr_info(" set register of cvd 0x8a to %u.\n", n);
	return count;
}

static DEVICE_ATTR(cvd_reg8a, 0200, NULL, tvafe_cvd_reg8a_store);

static ssize_t tvafereg_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t count)
{
	struct tvafe_dev_s *devp;
	unsigned int argn = 0, addr = 0, value = 0, end = 0;
	char *p, *para, *buf_work, cmd = 0;
	char *argv[3];
	long tmp = 0;

	devp = dev_get_drvdata(dev);

	buf_work = kstrdup(buff, GFP_KERNEL);
	p = buf_work;

	for (argn = 0; argn < 3; argn++) {
		para = strsep(&p, " ");
		if (para == NULL)
			break;
		argv[argn] = para;
	}

	if (argn < 1 || argn > 3) {
		kfree(buf_work);
		return count;
	}

	cmd = argv[0][0];

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		tvafe_pr_err("tvafe havn't opened, error!!!\n");
		return count;
	}
		switch (cmd) {
		case 'r':
		case 'R':
			if (argn < 2) {
				tvafe_pr_err("syntax error.\n");
			} else{
				if (kstrtol(argv[1], 16, &tmp) == 0)
					addr = tmp;
				else
					break;
				value = R_APB_REG(addr<<2);
				tvafe_pr_info("APB[0x%04x]=0x%08x\n",
					addr, value);
			}
			break;
		case 'w':
		case 'W':
			if (argn < 3) {
				tvafe_pr_err("syntax error.\n");
			} else{
				if (kstrtol(argv[1], 16, &tmp) == 0)
					value = tmp;
				else
					break;
				if (kstrtol(argv[2], 16, &tmp) == 0)
					addr = tmp;
				else
					break;
				W_APB_REG(addr<<2, value);
				tvafe_pr_info("Write APB[0x%04x]=0x%08x\n",
					addr, R_APB_REG(addr<<2));
			}
			break;
		case 'd':
		/*case 'D': */
			if (argn < 3) {
				tvafe_pr_err("syntax error.\n");
			} else{
				if (kstrtol(argv[1], 16, &tmp) == 0)
					addr = tmp;
				else
					break;
				if (kstrtol(argv[2], 16, &tmp) == 0)
					end = tmp;
				else
					break;
				for (; addr <= end; addr++)
					tvafe_pr_info("APB[0x%04x]=0x%08x\n",
					addr, R_APB_REG(addr<<2));
			}
			break;
		case 'D':
			/* if (argn < 3) {*/
			/*	tvafe_pr_err("syntax error.\n");*/
			/* } else{*/
			tvafe_pr_info("dump TOP reg----\n");
			for (addr = TOP_BASE_ADD;
				addr <= (TOP_BASE_ADD+0xb2); addr++)
				tvafe_pr_info("[0x%x]APB[0x%04x]=0x%08x\n",
						(0XC8842000+(addr<<2)), addr,
						R_APB_REG(addr<<2));
			tvafe_pr_info("dump ACD reg----\n");
			for (addr = ACD_BASE_ADD;
				addr <= (ACD_BASE_ADD+0xA4); addr++)
				tvafe_pr_info("[0x%x]APB[0x%04x]=0x%08x\n",
						(0XC8842000+(addr<<2)), addr,
						R_APB_REG(addr<<2));
			tvafe_pr_info("dump CVD2 reg----\n");
			for (addr = CVD_BASE_ADD;
				addr <= (CVD_BASE_ADD+0xf9); addr++)
				tvafe_pr_info("[0x%x]APB[0x%04x]=0x%08x\n",
						(0XC8842000+(addr<<2)), addr,
						R_APB_REG(addr<<2));
			tvafe_pr_info("dump reg done----\n");
			/* } */
			break;
		default:
			tvafe_pr_err("not support.\n");
			break;
	}
	kfree(buf_work);
	return count;
}


static ssize_t tvafe_reg_show(struct device *dev,
		struct device_attribute *attr, char *buff)
{
	ssize_t len = 0;

	len += sprintf(buff+len, "Usage:\n");
	len += sprintf(buff+len,
		"\techo [read|write <date>] addr > reg;Access ATV DEMON/TVAFE/HDMIRX logic address.\n");
	len += sprintf(buff+len,
		"\techo dump <start> <end> > reg;Dump ATV DEMON/TVAFE/HDMIRX logic address.\n");
	len += sprintf(buff+len,
		"Address format:\n");
	len += sprintf(buff+len,
		"\taddr    : 0xXXXX, 16 bits register address\n");
	return len;
}

static DEVICE_ATTR(reg, 0644, tvafe_reg_show, tvafereg_store);

int tvafe_device_create_file(struct device *dev)
{
	int ret = 0;

	ret |= device_create_file(dev, &dev_attr_debug);
	ret |= device_create_file(dev, &dev_attr_cvd_reg8a);
	ret |= device_create_file(dev, &dev_attr_dumpmem);
	ret |= device_create_file(dev, &dev_attr_reg);

	return ret;
}

void tvafe_remove_device_files(struct device *dev)
{
	device_remove_file(dev, &dev_attr_debug);
	device_remove_file(dev, &dev_attr_dumpmem);
	device_remove_file(dev, &dev_attr_cvd_reg8a);
	device_remove_file(dev, &dev_attr_debug);
}




