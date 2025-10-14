/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MDP_TILE_SCALER_H__
#define __MDP_TILE_SCALER_H__

#include <linux/printk.h>
#include <linux/types.h>
#include <linux/bug.h>

#define RSZ_TILE_SUBPIXEL_BITS 20

#ifndef ASSERT
#define ASSERT(expr) \
	do { \
		if (expr) \
			break; \
		pr_err("MML ASSERT FAILED %s, %d\n", __FILE__, __LINE__); \
		WARN_ON(1); \
	} while (0)
#endif

enum scaler_algo {
	/* Cannot modify these enum definition */
	SCALER_6_TAPS  = 0,
	SCALER_SRC_ACC = 1,	/* 1n tap */
	SCALER_CUB_ACC = 2,	/* 4n tap */
};

void backward_6_taps(s32 out_start,
		     s32 out_end,
		     s32 coeff,
		     s32 precision,
		     s32 crop,
		     s32 crop_frac,
		     s32 in_max,
		     s32 in_align,
		     s32 *in_start,
		     s32 *in_end);

void forward_6_taps(s32 in_start,
		    s32 in_end,
		    s32 in_max,
		    s32 coeff,
		    s32 precision,
		    s32 crop,
		    s32 crop_frac,
		    s32 out_max,
		    s32 out_align,
		    s32 back_out_start,
		    bool back_out_end_flag,
		    s32 *out_start,
		    s32 *out_end,
		    s32 *luma,
		    s32 *luma_frac,
		    s32 *chroma,
		    s32 *chroma_frac);

void backward_src_acc(s32 out_start,
		      s32 out_end,
		      s32 coeff,
		      s32 precision,
		      s32 crop,
		      s32 crop_frac,
		      s32 in_max,
		      s32 in_align,
		      s32 *in_start,
		      s32 *in_end);

void forward_src_acc(s32 in_start,
		     s32 in_end,
		     s32 in_max,
		     s32 coeff,
		     s32 precision,
		     s32 crop,
		     s32 crop_frac,
		     s32 out_max,
		     s32 out_align,
		     s32 back_out_start,
		     bool back_out_end_flag,
		     s32 *out_start,
		     s32 *out_end,
		     s32 *luma,
		     s32 *luma_frac,
		     s32 *chroma,
		     s32 *chroma_frac);

void backward_cub_acc(s32 out_start,
		      s32 out_end,
		      s32 coeff,
		      s32 precision,
		      s32 crop,
		      s32 crop_frac,
		      s32 in_max,
		      s32 in_align,
		      s32 *in_start,
		      s32 *in_end);

void forward_cub_acc(s32 in_start,
		     s32 in_end,
		     s32 in_max,
		     s32 coeff,
		     s32 precision,
		     s32 crop,
		     s32 crop_frac,
		     s32 out_max,
		     s32 out_align,
		     s32 back_out_start,
		     bool back_out_end_flag,
		     s32 *out_start,
		     s32 *out_end,
		     s32 *luma,
		     s32 *luma_frac,
		     s32 *chroma,
		     s32 *chroma_frac);

#endif  /* __MDP_TILE_SCALER_H__ */
