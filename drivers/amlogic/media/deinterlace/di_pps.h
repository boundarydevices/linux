/*
 * drivers/amlogic/media/deinterlace/di_pps.h
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

#ifndef DI_PPS_H
#define DI_PPS_H
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/video_sink/vpp.h>
#if 0
#define VPP_FLAG_WIDEMODE_MASK      0x0000000F
#define VPP_FLAG_INTERLACE_OUT      0x00000010
#define VPP_FLAG_INTERLACE_IN       0x00000020
#define VPP_FLAG_CBCR_SEPARATE      0x00000040
#define VPP_FLAG_ZOOM_SHORTSIDE     0x00000080
#define VPP_FLAG_AR_MASK            0x0003ff00
#define VPP_FLAG_AR_BITS            8
#define VPP_FLAG_PORTRAIT_MODE      0x00040000
#define VPP_FLAG_VSCALE_DISABLE     0x00080000

#define IDX_H           (2 << 8)
#define IDX_V_Y         (1 << 13)
#define IDX_V_CBCR      ((1 << 13) | (1 << 8))

#define ASPECT_4_3      ((3<<8)/4)
#define ASPECT_16_9     ((9<<8)/16)

#define SPEED_CHECK_DONE	0
#define SPEED_CHECK_HSKIP	1
#define SPEED_CHECK_VSKIP	2

enum f2v_vphase_type_e {
	F2V_IT2IT = 0,
	F2V_IB2IB,
	F2V_IT2IB,
	F2V_IB2IT,
	F2V_P2IT,
	F2V_P2IB,
	F2V_IT2P,
	F2V_IB2P,
	F2V_P2P,
	F2V_TYPE_MAX
}; /* frame to video conversion type */
#endif
struct pps_f2v_vphase_s {
	unsigned char rcv_num;
	unsigned char rpt_num;
	unsigned short phase;
};
struct ppsfilter_mode_s {
	u32 pps_hf_start_phase_step;
	u32 pps_hf_start_phase_slope;
	u32 pps_hf_end_phase_slope;
	const u32 *pps_vert_coeff;
	const u32 *pps_horz_coeff;
	u32 pps_sc_misc_;
	u32 pps_vsc_start_phase_step;
	u32 pps_hsc_start_phase_step;
	bool pps_pre_vsc_en;
	bool pps_pre_hsc_en;
	u32 pps_vert_filter;
	u32 pps_horz_filter;
	const u32 *pps_chroma_coeff;
	u32 pps_chroma_filter_en;
};

struct pps_frame_par_s {
	u32 pps_vsc_startp;
	u32 pps_vsc_endp;
	u32 pps_hsc_startp;
	u32 pps_hsc_linear_startp;
	u32 pps_hsc_linear_endp;
	u32 pps_hsc_endp;
	u32 VPP_hf_ini_phase_;
	struct f2v_vphase_s VPP_vf_ini_phase_[9];
	u32 pps_pic_in_height_;
	u32 pps_line_in_length_;
	struct ppsfilter_mode_s pps_filter;
	u32 pps_3d_mode;
	u32 trans_fmt;
	/* bit[1:0] 0: 1 pic,1:two pic one buf,2:tow pic two buf */
	/* bit[2]0:select pic0,1:select pic1 */
	/* bit[3]0:pic0 first,1:pic1 first */
	bool pps_3d_scale;
};

void di_pps_config(unsigned char path, int src_w, int src_h,
	int dst_w, int dst_h);
void dump_pps_reg(unsigned int base_addr);
#endif
