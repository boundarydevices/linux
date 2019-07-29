/*
 * drivers/amlogic/media/deinterlace/pulldown_drv.c
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

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/device.h>
#include "deinterlace_hw.h"
#include "deinterlace_dbg.h"

static unsigned int field_diff_rate;

static unsigned int flm22_sure_num = 100;
static unsigned int flm22_sure_smnum = 70;
static unsigned int flm22_ratio = 200;
/* 79 for iptv test pd22 ts */
module_param_named(flm22_ratio, flm22_ratio, uint, 0644);

static struct sFlmSftPar pd_param;
static struct FlmDectRes dectres;
static struct FlmModReg_t flmreg;

static void pulldown_mode_init(struct pulldown_detected_s *pd_config)
{
	unsigned int i = 0;

	pd_config->global_mode =
		PULL_DOWN_NORMAL;
	for (i = 0; i < MAX_VOF_WIN_NUM; i++) {
		pd_config->regs[i].win_vs = 0;
		pd_config->regs[i].win_ve = 0;
		pd_config->regs[i].blend_mode =
			PULL_DOWN_NORMAL;
	}
}

static void pulldown_wnd_config(struct pulldown_detected_s *pd_config,
	unsigned short wins[][3])
{
	unsigned int i = 0;

	for (i = 0; i < MAX_VOF_WIN_NUM; i++) {
		pd_config->regs[i].win_vs = wins[i][0];
		pd_config->regs[i].win_ve = wins[i][1];
		pd_config->regs[i].blend_mode = wins[i][2];
	}
}

void pulldown_vof_win_vshift(struct pulldown_detected_s *wins,
	unsigned short v_offset)
{
	unsigned int i = 0;

	for (i = 0; i < MAX_VOF_WIN_NUM; i++) {
		if (wins->regs[0].win_vs > v_offset)
			wins->regs[0].win_vs -= v_offset;
		else
			wins->regs[0].win_vs = 0;
		if (wins->regs[0].win_ve > v_offset)
			wins->regs[0].win_ve -= v_offset;
		else
			wins->regs[0].win_ve = 0;
	}
}
static int flag_di_weave = 1;
static unsigned int pldn_mod;

static unsigned int pldn_cmb0 = 1;
module_param_named(pldn_cmb0, pldn_cmb0, uint, 0644);

static unsigned int pldn_cmb1;
module_param_named(pldn_cmb1, pldn_cmb1, uint, 0644);


/* static unsigned int flmxx_sure_num[7]
 * = {50, 50, 50, 50, 50, 50, 50};
 */
static unsigned int flmxx_sure_num[7] = {20, 20, 20, 20, 20, 20, 20};
static unsigned int flmxx_snum_adr = 7;
module_param_array(flmxx_sure_num, uint, &flmxx_snum_adr, 0664);

static unsigned int flm22_glbpxlnum_rat = 4; /* 4/256 = 64 */

static unsigned int flm22_glbpxl_maxrow = 16; /* 16/256 = 16 */
module_param(flm22_glbpxl_maxrow, uint, 0644);
MODULE_PARM_DESC(flm22_glbpxl_maxrow, "flm22_glbpxl_maxrow/n");

static unsigned int flm22_glbpxl_minrow = 3; /* 4/256 = 64 */
module_param(flm22_glbpxl_minrow, uint, 0644);
MODULE_PARM_DESC(flm22_glbpxl_minrow, "flm22_glbpxl_minrow/n");

static unsigned int cmb_3point_rnum;
module_param(cmb_3point_rnum, uint, 0644);
MODULE_PARM_DESC(cmb_3point_rnum, "cmb_3point_rnum/n");

static unsigned int cmb_3point_rrat = 32;
module_param(cmb_3point_rrat, uint, 0644);
MODULE_PARM_DESC(cmb_3point_rrat, "cmb_3point_rrat/n");

unsigned int pulldown_detection(struct pulldown_detected_s *res,
	struct combing_status_s *cmb_sts, bool reverse, struct vframe_s *vf)
{
	unsigned int glb_frame_mot_num, glb_field_mot_num, i;
	unsigned int mot_row = 0, mot_max = 0, ntmp = 0;
	unsigned int flm22_surenum = flm22_sure_num;
	int difflag = 2;
	bool flm32 = false, flm22 = false, flmxx = false;

	read_pulldown_info(&glb_frame_mot_num,
		&glb_field_mot_num);

	read_new_pulldown_info(&flmreg);
	dectres.rF22Flag = FlmVOFSftTop(&(dectres.rCmb32Spcl),
		dectres.rPstCYWnds[0],
		dectres.rPstCYWnds[1],
		dectres.rPstCYWnds[2],
		dectres.rPstCYWnds[3],
		dectres.rPstCYWnds[4],
		&(dectres.rFlmPstGCm),
		&(dectres.rFlmSltPre),
		&(dectres.rFlmPstMod),
		&(dectres.dif01flag),
		flmreg.rROFldDif01,
		flmreg.rROFrmDif02,
		flmreg.rROCmbInf,
		glb_frame_mot_num,
		glb_field_mot_num,
		&cmb_sts->cmb_row_num,
		&cmb_sts->frame_diff_avg,
		&pd_param,
		reverse,
		vf);

	difflag = dectres.dif01flag;
	if (dectres.rFlmPstMod == 1)
		difflag = dectres.rFlmSltPre;
	if (pd_param.height >= 289) /*full hd */
		cmb_sts->cmb_row_num = cmb_sts->cmb_row_num << 1;
	if (cmb_sts->cmb_row_num > pd_param.height)
		cmb_sts->cmb_row_num = pd_param.height;

	prt_flg = ((pr_pd >> 1) & 0x1);
	if (prt_flg) {
		sprintf(debug_str, "#Pst-Dbg:\n");
		sprintf(debug_str + strlen(debug_str),
			"Mod=%d, Pre=%d, GCmb=%d, Lvl2=%d\n",
			dectres.rFlmPstMod,
			dectres.rFlmSltPre,
			dectres.rFlmPstGCm,
			dectres.rF22Flag);

		sprintf(debug_str + strlen(debug_str),
			"N%03d: nd[%d~%d], [%d~%d], [%d~%d], [%d~%d]\n",
			cmb_sts->cmb_row_num,
			dectres.rPstCYWnds[0][0],
			dectres.rPstCYWnds[0][1],
			dectres.rPstCYWnds[1][0],
			dectres.rPstCYWnds[1][1],
			dectres.rPstCYWnds[2][0],
			dectres.rPstCYWnds[2][1],
			dectres.rPstCYWnds[3][0],
			dectres.rPstCYWnds[3][1]);

		pr_info("%s", debug_str);
	}
	if (pr_pd)
		pr_info("diff_flag=%d\n", difflag);

	pulldown_mode_init(res);
	if (difflag == 1 && flag_di_weave)
		res->global_mode = PULL_DOWN_NORMAL;
	else if (difflag == 0 && flag_di_weave == 1)
		res->global_mode = PULL_DOWN_NORMAL_2;
	else
		res->global_mode = PULL_DOWN_NORMAL;

	if (dectres.rFlmPstMod == 1)
		cmb_sts->like_pulldown22_flag = dectres.rF22Flag;
	else
		cmb_sts->like_pulldown22_flag = 0;

	if ((pr_pd >> 1) & 0x1)
		pr_info("fld_dif_rat=%d\n",
			field_diff_rate);
	if ((dectres.rF22Flag >=
		(cmb_3point_rnum + field_diff_rate)) &&
		(cmb_sts->cmb_row_num >
		(pd_param.height * cmb_3point_rrat >> 8))) {
		if ((pr_pd >> 1) & 0x1)
			pr_info("coeff-3-point enabled\n");
	}
	if (dectres.rFlmPstMod != 0) {
		flm32 = (dectres.rFlmPstMod == 2 &&
			dectres.rFlmPstGCm == 0);

		ntmp = (glb_frame_mot_num + glb_field_mot_num) /
			(pd_param.width + 1);
		if (flm22_sure_num > ntmp + flm22_sure_smnum)
			flm22_surenum = flm22_sure_num - ntmp;
		else
			flm22_surenum = flm22_sure_smnum;

		if (dectres.rFlmPstMod == 1) {
			mot_row = glb_frame_mot_num *
				flm22_glbpxlnum_rat / (pd_param.width + 1);
			mot_max = (flm22_glbpxl_maxrow *
				pd_param.height + 128) >> 8;
			if ((pr_pd >> 1) & 0x1)
				pr_info("dejaggies level=%3d - (%02d - %02d)\n",
					dectres.rF22Flag,
					mot_max, mot_row);

			if (mot_row < mot_max) {
				if (dectres.rF22Flag >
					(mot_max - mot_row))
					dectres.rF22Flag -=
					(mot_max - mot_row);
				else
					dectres.rF22Flag = 0;

				if (mot_row <= flm22_glbpxl_minrow)
					dectres.rFlmPstMod = 0;
			}
		}

		flm22 = (dectres.rFlmPstMod == 1  &&
			dectres.rF22Flag >= flm22_surenum);
		if (dectres.rFlmPstMod >= 4)
			flmxx = (dectres.rF22Flag >=
				flmxx_sure_num[dectres.rFlmPstMod - 4]);
		else
			flmxx = 0;

		/* 2-2 force */
		if ((pldn_mod == 0) &&
			(flm32 || flm22 || flmxx)) {
			if (dectres.rFlmSltPre == 1)
				res->global_mode =
					PULL_DOWN_BLEND_0;
			else {
				res->global_mode =
					PULL_DOWN_BLEND_2;
			}
		} else if (pldn_mod == 1) {
			if (dectres.rFlmSltPre == 1)
				res->global_mode =
					PULL_DOWN_BLEND_0;
			else
				res->global_mode =
					PULL_DOWN_BLEND_2;
		} else {
			if (difflag == 1 && flag_di_weave) {
				res->global_mode
					= PULL_DOWN_NORMAL;
			} else if (difflag == 0 &&
				flag_di_weave) {
				res->global_mode
					= PULL_DOWN_NORMAL_2;
			} else {
				res->global_mode
					= PULL_DOWN_NORMAL;
			}
		}

		if (flm32 && (pldn_cmb0 == 1)) {
			pulldown_wnd_config(res,
				dectres.rPstCYWnds);
		} else if (dectres.rF22Flag > 1 &&
			dectres.rFlmPstMod == 1 &&
			pldn_cmb0 == 1) {
			if ((pr_pd >> 1) & 0x1)
				pr_info("dejaggies level= %3d\n",
					dectres.rF22Flag);
			} else if (dectres.rFlmPstGCm == 0 &&
				pldn_cmb0 > 1 && pldn_cmb0 <= 5) {
				pulldown_wnd_config(res,
					dectres.rPstCYWnds);
				/* 1-->only film-mode
				 * 2-->windows-->mtn
				 * 3-->windows-->detected
				 * 4-->windows-->di
				 */
				/* pldn_cmb0 == 2
				 * setting in pulldown wnd config
				 */
				if (pldn_cmb0 == 3) {
					for (i = 0; i < MAX_VOF_WIN_NUM; i++)
						res->regs[i].blend_mode = 3;
				} else if (pldn_cmb0 == 4) {
					for (i = 0; i < MAX_VOF_WIN_NUM; i++)
						res->regs[i].blend_mode = 2;
				} else if (pldn_cmb0 == 5) {
					res->regs[3].win_vs = 0;
					res->regs[3].win_ve = 60;
					res->regs[3].blend_mode = 0;
				}
			}
			/* else pldn_cmb0==0 (Nothing) */
			if ((dectres.rFlmPstGCm == 1) && (pldn_cmb1 > 0)
			    && (pldn_cmb1 <= 5)) {
				pulldown_wnd_config(res,
					dectres.rPstCYWnds);
				/*
				 * 1-->normal set in pulldown
				 * wnd config func
				 */
				if (pldn_cmb1 == 2) {
					for (i = 0; i < MAX_VOF_WIN_NUM; i++)
						res->regs[i].blend_mode = 3;
				} else if (pldn_cmb1 == 3) {
					for (i = 0; i < MAX_VOF_WIN_NUM; i++)
						res->regs[i].blend_mode = 2;
				} else if (pldn_cmb1 == 4) {
					res->regs[2].win_vs = 202;
					res->regs[2].win_ve = 222;
					res->regs[2].blend_mode = 0;
				}
			} else if ((pldn_cmb0 == 6) && (pldn_cmb1 == 6)) {
				res->regs[1].win_vs = 60;
				res->regs[1].win_ve = 180;
				res->regs[1].blend_mode = 0;
			}
		}
	return 0;
}

unsigned char pulldown_init(unsigned short width, unsigned short height)
{
	flm22_sure_num = (height * 100)/480;
	flm22_sure_smnum = (flm22_sure_num * flm22_ratio)/100;
	pd_param.width = width;
	pd_param.height = height;
	pd_param.field_count = 0;
	return FlmVOFSftInt(&pd_param);
}

struct pd_param_s {
	char *name;
	int *addr;
};

static struct pd_param_s pd_params[] = {
	{ "sFrmDifAvgRat",
	  &(pd_param.sFrmDifAvgRat)},
	{ "sFrmDifLgTDif",
	  &(pd_param.sFrmDifLgTDif) },
	{ "sF32StpWgt01",
	  &(pd_param.sF32StpWgt01) },
	{ "sF32StpWgt02",
	  &(pd_param.sF32StpWgt02) },
	{ "sF32DifLgRat",
	  &(pd_param.sF32DifLgRat) },
	{ "sFlm2MinAlpha",
	  &(pd_param.sFlm2MinAlpha) },
	{ "sFlm2MinBelta",
	  &(pd_param.sFlm2MinBelta) },
	{ "sFlm20ftAlpha",
	  &(pd_param.sFlm20ftAlpha) },
	{ "sFlm2LgDifThd",
	  &(pd_param.sFlm2LgDifThd) },
	{ "sFlm2LgFlgThd",
	  &(pd_param.sFlm2LgFlgThd) },
	{ "sF32Dif01A1",
	  &(pd_param.sF32Dif01A1)   },
	{ "sF32Dif01T1",
	  &(pd_param.sF32Dif01T1)   },
	{ "sF32Dif01A2",
	  &(pd_param.sF32Dif01A2)   },
	{ "sF32Dif01T2",
	  &(pd_param.sF32Dif01T2)   },
	{ "rCmbRwMinCt0",
	  &(pd_param.rCmbRwMinCt0)  },
	{ "rCmbRwMinCt1",
	  &(pd_param.rCmbRwMinCt1)  },
	{ "mPstDlyPre",
	  &(pd_param.mPstDlyPre)    },
	{ "mNxtDlySft",
	  &(pd_param.mNxtDlySft)    },
	{ "cmb22_nocmb_num",
	 &(pd_param.cmb22_nocmb_num)},
	{ "flm22_en",
	  &(pd_param.flm22_en)      },
	{ "flm32_en",
	     &(pd_param.flm32_en)   },
	{ "flm22_flag",
	  &(pd_param.flm22_flag)    },
	{ "flm22_avg_flag",
		&(pd_param.flm22_avg_flag)},
	{ "flm2224_flag",
	&(pd_param.flm2224_flag)    },
	{ "flm22_comlev",
	  &(pd_param.flm22_comlev)  },
	{ "flm22_comlev1",
	  &(pd_param.flm22_comlev1) },
	{ "flm22_comnum",
	  &(pd_param.flm22_comnum)  },
	{ "flm22_comth",
	  &(pd_param.flm22_comth)  },
	{ "flm22_dif01_avgth",
	  &(pd_param.flm22_dif01_avgth)  },
	{ "dif01rate",
	  &(pd_param.dif01rate)     },
	{ "flag_di01th",
	  &(pd_param.flag_di01th)   },
	{ "numthd",
	  &(pd_param.numthd)        },
	{ "flm32_dif02_gap_th",
	  &(pd_param.flm32_dif02_gap_th) },
	{ "flm32_luma_th",
	  &(pd_param.flm32_luma_th)   },
	{ "sF32Dif02M0",
	  &(pd_param.sF32Dif02M0)   },        /* mpeg-4096, cvbs-8192 */
	{ "sF32Dif02M1",
	  &(pd_param.sF32Dif02M1)   },        /* mpeg-4096, cvbs-8192 */
	{ "",		  NULL          }
};

static ssize_t pd_parm_store(struct device *dev,
			      struct device_attribute *attr, const char *buff,
			      size_t count)
{
	int i = 0;
	int value = 0;
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;

	buf_orig = kstrdup(buff, GFP_KERNEL);
	parse_cmd_params(buf_orig, (char **)(&parm));
	for (i = 0; pd_params[i].addr; i++) {
		if (!strcmp(parm[0], pd_params[i].name)) {
			rc = kstrtoint(parm[1], 10, &value);
			*(pd_params[i].addr) = value;
			pr_dbg("%s=%d.\n", pd_params[i].name, value);
		}
	}

	return count;
}

static ssize_t pd_parm_show(struct device *dev,
			     struct device_attribute *attr, char *buff)
{
	ssize_t len = 0;
	int i = 0;

	for (i = 0; pd_params[i].addr; i++) {
		len += sprintf(buff + len, "%s=%d.\n",
			pd_params[i].name, *pd_params[i].addr);
	}

	len += sprintf(buff + len, "\npulldown detection result:\n");
	len += sprintf(buff + len, "rPstCYWnd0 s=%u.\n",
		dectres.rPstCYWnds[0][0]);
	len += sprintf(buff + len, "rPstCYWnd0 e=%u.\n",
		dectres.rPstCYWnds[0][1]);
	len += sprintf(buff + len, "rPstCYWnd0 b=%u.\n",
		dectres.rPstCYWnds[0][2]);

	len += sprintf(buff + len, "rPstCYWnd1 s=%u.\n",
		dectres.rPstCYWnds[1][0]);
	len += sprintf(buff + len, "rPstCYWnd1 e=%u.\n",
		dectres.rPstCYWnds[1][1]);
	len += sprintf(buff + len, "rPstCYWnd1 b=%u.\n",
		dectres.rPstCYWnds[1][2]);

	len += sprintf(buff + len, "rPstCYWnd2 s=%u.\n",
		dectres.rPstCYWnds[2][0]);
	len += sprintf(buff + len, "rPstCYWnd2 e=%u.\n",
		dectres.rPstCYWnds[2][1]);
	len += sprintf(buff + len, "rPstCYWnd2 b=%u.\n",
		dectres.rPstCYWnds[2][2]);

	len += sprintf(buff + len, "rPstCYWnd3 s=%u.\n",
		dectres.rPstCYWnds[3][0]);
	len += sprintf(buff + len, "rPstCYWnd3 e=%u.\n",
		dectres.rPstCYWnds[3][1]);
	len += sprintf(buff + len, "rPstCYWnd3 b=%u.\n",
		dectres.rPstCYWnds[3][2]);

	len += sprintf(buff + len, "rPstCYWnd4 s=%u.\n",
		dectres.rPstCYWnds[4][0]);
	len += sprintf(buff + len, "rPstCYWnd4 e=%u.\n",
		dectres.rPstCYWnds[4][1]);
	len += sprintf(buff + len, "rPstCYWnd4 b=%u.\n",
		dectres.rPstCYWnds[4][2]);

	len += sprintf(buff + len, "rFlmPstGCm=%u.\n",
		dectres.rFlmPstGCm);
	len += sprintf(buff + len, "rFlmSltPre=%u.\n",
		dectres.rFlmSltPre);
	len += sprintf(buff + len, "rFlmPstMod=%d.\n",
		dectres.rFlmPstMod);
	len += sprintf(buff + len, "rFlmPstMod=%d.\n",
		dectres.dif01flag);
	len += sprintf(buff + len, "rF22Flag=%d.\n",
		dectres.rF22Flag);
	return len;
}

static DEVICE_ATTR(pd_param, 0664, pd_parm_show, pd_parm_store);

void pd_device_files_add(struct device *dev)
{
	device_create_file(dev, &dev_attr_pd_param);

}

void pd_device_files_del(struct device *dev)
{
	device_create_file(dev, &dev_attr_pd_param);
}
#ifdef DEBUG_SUPPORT
module_param_named(flm22_sure_num, flm22_sure_num, uint, 0644);
module_param_named(flm22_glbpxlnum_rat, flm22_glbpxlnum_rat, uint, 0644);
module_param_named(flag_di_weave, flag_di_weave, int, 0644);
#endif
