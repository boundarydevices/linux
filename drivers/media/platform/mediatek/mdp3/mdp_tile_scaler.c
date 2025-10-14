// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include "mdp_tile_scaler.h"

void backward_6_taps(s32 out_start,
		     s32 out_end,
		     s32 coeff,
		     s32 precision,
		     s32 crop,
		     s32 crop_frac,
		     s32 in_max,
		     s32 in_align,
		     s32 *in_start,
		     s32 *in_end)
{
	s64 start, end;

	if (crop_frac < 0)
		crop_frac = -0xfffff;
	crop_frac = ((s64)crop_frac * precision) >> RSZ_TILE_SUBPIXEL_BITS;

	start = (s64)out_start * coeff + (s64)crop * precision + crop_frac;
	if (start <= (s64)3 * precision) {
		*in_start = 0;
	} else {
		start = start / precision - 3;
		if (!(start & 0x1) || in_align == 1)
			*in_start = (s32)start;
		else /* must be even */
			*in_start = (s32)start - 1;
	}

	end = (s64)out_end * coeff + (s64)crop * precision + crop_frac +
		(3 + 2) * precision;
	if (end > (s64)in_max * precision) {
		*in_end = in_max;
	} else {
		end = end / precision;
		if (end & 0x1 || in_align == 1)
			*in_end = (s32)end;
		else /* must be odd */
			*in_end = (s32)end + 1;
	}
}

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
		    s32 *chroma_frac)
{
	s64 offset;

	if (crop_frac < 0)
		crop_frac = -0xfffff;
	crop_frac = ((s64)crop_frac * precision) >> RSZ_TILE_SUBPIXEL_BITS;

	if (in_end == in_max && back_out_end_flag) {
		*out_end = out_max;
	} else {
		s64 end = (s64)(in_end - 2 - 2) * precision -
			(s64)crop * precision - crop_frac;
		s32 tmp = (s32)(end / coeff);

		if ((s64)tmp * coeff == end) /* ceiling in forward */
			tmp = tmp - 1;

		if (tmp & 0x1 || out_align == 1)
			*out_end = tmp;
		else /* must be odd */
			*out_end = tmp - 1;
	}

	*out_start = back_out_start;

	/* cal offset & frac by fixed out_start */
	offset = (s64)*out_start * coeff + (s64)crop * precision + crop_frac -
		(s64)in_start * precision;

	*luma = (s32)(offset / precision);
	*luma_frac = (s32)(offset - *luma * precision);

	if (*luma_frac < 0) {
		*luma -= 1;
		*luma_frac += precision;
	}

	*chroma = *luma;
	*chroma_frac = *luma_frac;

	if (*out_start > *out_end) {
		ASSERT(0);
		return; /* T_TP6_FOR_INVALID_OUT_XYS_XYE_ERROR; */
	}

	if (*out_end > out_max)
		*out_end = out_max;
}

void backward_src_acc(s32 out_start,
		      s32 out_end,
		      s32 coeff,
		      s32 precision,
		      s32 crop,
		      s32 crop_frac,
		      s32 in_max,
		      s32 in_align,
		      s32 *in_start,
		      s32 *in_end)
{
	s64 start, end, tmp;

	if (crop_frac < 0)
		crop_frac = -0xfffff;
	crop_frac = ((s64)crop_frac * coeff) >> RSZ_TILE_SUBPIXEL_BITS;

	if (coeff > precision) {
		ASSERT(0);
		return; /* T_RESIZER_SRC_ACC_SCALING_UP_ERROR; */
	}

	start = (s64)out_start * precision + (s64)crop * coeff + crop_frac;
	start = start * 2 + coeff - precision;
	if (start <= 0) {
		*in_start = 0;
	} else {
		tmp = start / (2 * coeff);
		if (!(tmp & 0x1) || in_align == 1)
			*in_start = (s32)tmp;
		else /* must be even */
			*in_start = (s32)tmp - 1;
	}

	end = (s64)out_end * precision + (s64)crop * coeff + crop_frac;
	end = end * 2 + coeff + precision;
	if (end >= (s64)in_max * 2 * coeff) {
		*in_end = in_max;
	} else {
		tmp = end / (2 * coeff);
		if (tmp * 2 * coeff == end) /* ceiling in backward */
			tmp = tmp - 1;

		if (tmp & 0x1 || in_align == 1)
			*in_end = (s32)tmp;
		else /* must be odd */
			*in_end = (s32)tmp + 1;
	}
}

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
		     s32 *chroma_frac)
{
	s64 offset;

	if (crop_frac < 0)
		crop_frac = -0xfffff;
	crop_frac = ((s64)crop_frac * coeff) >> RSZ_TILE_SUBPIXEL_BITS;

	if (coeff > precision) {
		ASSERT(0);
		return; /* T_RESIZER_SRC_ACC_SCALING_UP_ERROR; */
	}

	if (in_end >= in_max && back_out_end_flag) {
		*out_end = out_max;
	} else {
		s64 end = (s64)in_end * coeff + ((u32)coeff >> 1) -
			(s64)crop * coeff - crop_frac;
		s32 tmp = (s32)((end * 2 - precision) / (2 * precision));

		if (tmp & 0x1 || out_align == 1)
			*out_end = tmp;
		else /* must be odd */
			*out_end = tmp - 1;
	}

	*out_start = back_out_start;

	/* cal offset & frac by fixed out_start */
	offset = (s64)*out_start * precision + (s64)crop * coeff + crop_frac -
		(s64)in_start * coeff;

	*luma = (s32)(offset / precision);
	*luma_frac = (s32)(offset - *luma * precision);
	*chroma = *luma;
	*chroma_frac = *luma_frac;

	if (*out_start > *out_end) {
		ASSERT(0);
		return; /* T_SRC_ACC_FOR_INVALID_OUT_XYS_XYE_ERROR; */
	}

	if (*out_end > out_max)
		*out_end = out_max;
}

void backward_cub_acc(s32 out_start,
		      s32 out_end,
		      s32 coeff,
		      s32 precision,
		      s32 crop,
		      s32 crop_frac,
		      s32 in_max,
		      s32 in_align,
		      s32 *in_start,
		      s32 *in_end)
{
	s64 start, end, tmp;

	if (crop_frac < 0)
		crop_frac = -0xfffff;
	crop_frac = ((s64)crop_frac * coeff) >> RSZ_TILE_SUBPIXEL_BITS;

	if (coeff > precision) {
		ASSERT(0);
		return; /* T_RESIZER_CUBIC_ACC_SCALING_UP_ERROR; */
	}

	start = (s64)out_start * precision + (s64)crop * coeff + crop_frac;
	start = start - 2 * precision;
	if (start <= 0) {
		*in_start = 0;
	} else {
		tmp = start / coeff;
		if (!(tmp & 0x1) || in_align == 1)
			*in_start = (s32)tmp;
		else /* must be even */
			*in_start = (s32)tmp - 1;
	}

	end = (s64)out_end * precision + (s64)crop * coeff + crop_frac;
	end = end + coeff + 2 * precision;
	if (end >= (s64)in_max * coeff) {
		*in_end = in_max;
	} else {
		tmp = end / coeff;
		if (tmp * coeff == end) /* ceiling in backward */
			tmp = tmp - 1;

		if (tmp & 0x1 || in_align == 1)
			*in_end = (s32)tmp;
		else /* must be odd */
			*in_end = (s32)tmp + 1;
	}
}

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
		     s32 *chroma_frac)
{
	s64 offset;

	if (crop_frac < 0)
		crop_frac = -0xfffff;
	crop_frac = ((s64)crop_frac * coeff) >> RSZ_TILE_SUBPIXEL_BITS;

	if (coeff > precision) {
		ASSERT(0);
		return; /* T_RESIZER_CUBIC_ACC_SCALING_UP_ERROR; */
	}

	if (in_end >= in_max && back_out_end_flag) {
		*out_end = out_max;
	} else {
		s64 end = (s64)in_end * coeff -
			(s64)crop * coeff - crop_frac;
		s32 tmp = (s32)(end / precision - 2);

		if (tmp & 0x1 || out_align == 1)
			*out_end = tmp;
		else /* must be odd */
			*out_end = tmp - 1;
	}

	*out_start = back_out_start;

	/* cal offset & frac by fixed out_start */
	offset = (s64)*out_start * precision + (s64)crop * coeff + crop_frac -
		(s64)in_start * coeff;

	*luma = (s32)(offset / precision);
	*luma_frac = (s32)(offset - *luma * precision);
	*chroma = *luma;
	*chroma_frac = *luma_frac;

	if (*out_start > *out_end) {
		ASSERT(0);
		return; /* T_CUB_ACC_FOR_INVALID_OUT_XYS_XYE_ERROR; */
	}

	if (*out_end > out_max)
		*out_end = out_max;
}

