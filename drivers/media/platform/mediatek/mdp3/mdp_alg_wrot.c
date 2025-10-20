// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include "mdp_reg_wrot.h"
#include "mdp_tile_core.h"
#include "mdp_tile_comp.h"
#include "mtk-mdp3-alg.h"
#include "mtk-mdp3-core.h"

#define WROT_MIN_BUF_LINE_NUM 16
#define WROT_AFBC_ALIGN(p) ((((p) + 31) >> 5) << 5)

struct wrot_data {
	u32 fifo;
	u32 tile_width;
	u32 sram_size;
	u8 rb_swap;	/* version for rb channel swap behavior */
};

/* meta data for each different frame config */
struct wrot_frame_data {
	u8 out_idx;
	u32 out_w;
	u32 out_h;
	u32 y_stride;
	u32 uv_stride;
	u64 iova[IMG_MAX_PLANES];
	u32 plane_offset[IMG_MAX_PLANES];

	/* calculate in prepare and use as tile input */
	enum alg_orientation rotate;
	bool flip;
	bool en_x_crop;
	bool en_y_crop;
	struct img_crop out_crop;
	u8 pending_x;
	u8 pending_y;

	/* following data calculate in init and use in tile command */
	u8 mat_en;
	u8 mat_sel;
	u32 dither_con;
	u32 bbp_y;
	u32 bbp_uv;
	u32 hor_sh_uv;
	u32 ver_sh_uv;
	u32 filt_v;

	/* calculate in frame, use in each tile calc */
	u32 fifo_max_sz;
	u32 max_line_cnt;
};

struct wrot_ofst_addr {
	u64 y;
	u64 c;
	u64 v;
};

/* different wrot setting between each tile */
struct wrot_setting {
	u32 tar_xsize;
	/* result settings */
	u32 main_blk_width;
	u32 main_buf_line_num;
};

struct check_buf_param {
	u32 y_buf_size;
	u32 uv_buf_size;
	u32 y_buf_check;
	u32 uv_buf_check;
	u32 y_buf_width;
	u32 y_buf_usage;
	u32 uv_blk_width;
	u32 uv_blk_line;
	u32 uv_buf_width;
	u32 uv_buf_usage;
};

static const struct wrot_data mt8189_wrot_data = {
	.fifo = 512,
	.tile_width = 512,
	.sram_size = 512 * 1024,
};

/* filt_h, filt_v, uv_xsel, uv_ysel */
static const u32 uv_table[2][4][2][4] = {
	{	/* YUV422 */
		{	/* 0 */
			{ 1 /* [1 2 1] */, 0 /* drop  */, 0, 2 },
			{ 2 /* [1 2 1] */, 0 /* drop  */, 1, 2 }, /* flip */
		}, {	/* 90 */
			{ 0 /* drop    */, 4 /* [1 1] */, 2, 1 },
			{ 0 /* drop    */, 3 /* [1 1] */, 2, 0 }, /* flip */
		}, {	/* 180 */
			{ 2 /* [1 2 1] */, 0 /* drop  */, 1, 2 },
			{ 1 /* [1 2 1] */, 0 /* drop  */, 0, 2 }, /* flip */
		}, {	/* 270 */
			{ 0 /* drop    */, 3 /* [1 1] */, 2, 0 },
			{ 0 /* drop    */, 4 /* [1 1] */, 2, 1 }, /* flip */
		},
	}, {	/* YUV420 */
		{	/* 0 */
			{ 1 /* [1 2 1] */, 3 /* [1 1] */, 0, 0 },
			{ 2 /* [1 2 1] */, 3 /* [1 1] */, 1, 0 }, /* flip */
		}, {	/* 90 */
			{ 1 /* [1 2 1] */, 4 /* [1 1] */, 0, 1 },
			{ 1 /* [1 2 1] */, 3 /* [1 1] */, 0, 0 }, /* flip */
		}, {	/* 180 */
			{ 2 /* [1 2 1] */, 4 /* [1 1] */, 1, 1 },
			{ 1 /* [1 2 1] */, 4 /* [1 1] */, 0, 1 }, /* flip */
		}, {	/* 270 */
			{ 2 /* [1 2 1] */, 3 /* [1 1] */, 1, 0 },
			{ 2 /* [1 2 1] */, 4 /* [1 1] */, 1, 1 }, /* flip */
		},
	}
};

/* ceil_m and floor_m helper function */
static u32 ceil_m(u64 n, u64 d)
{
	u32 reminder = do_div((n), (d));

	return n + (reminder != 0);
}

static u32 floor_m(u64 n, u64 d)
{
	do_div(n, d);
	return n;
}

static bool is_change_wx(u16 r, bool f)
{
	return ((r == ROT_0 && f) ||
		(r == ROT_180 && !f) ||
		r == ROT_270);
}

static bool is_change_hy(u16 r, bool f)
{
	return ((r == ROT_90 && !f) ||
		r == ROT_180 ||
		(r == ROT_270 && f));
}

static void wrot_config_left(struct img_image_buffer *dst,
			     const struct img_crop *out_crop,
			     struct wrot_frame_data *wrot_frm)
{
	wrot_frm->en_x_crop = true;
	wrot_frm->out_crop.left = 0;
	wrot_frm->out_crop.width = out_crop->width >> 1;

	if (MDP_COLOR_IS_AFBC_ARGB(dst->format.colorformat))
		wrot_frm->out_crop.width = round_up(wrot_frm->out_crop.width, 32);
	else if (MDP_COLOR_IS_10BIT_PACKED(dst->format.colorformat))
		wrot_frm->out_crop.width = round_up(wrot_frm->out_crop.width, 4);
	else if (wrot_frm->out_crop.width & 1)
		wrot_frm->out_crop.width++; /* round_up(2) */

	if (is_change_wx(wrot_frm->rotate, wrot_frm->flip))
		wrot_frm->out_crop.width = out_crop->width - wrot_frm->out_crop.width;
}

static void wrot_config_right(struct img_image_buffer *dst,
			      const struct img_crop *out_crop,
			      struct wrot_frame_data *wrot_frm)
{
	wrot_frm->en_x_crop = true;
	wrot_frm->out_crop.left = out_crop->width >> 1;

	if (MDP_COLOR_IS_AFBC_ARGB(dst->format.colorformat))
		wrot_frm->out_crop.left = round_up(wrot_frm->out_crop.left, 32);
	else if (MDP_COLOR_IS_10BIT_PACKED(dst->format.colorformat))
		wrot_frm->out_crop.left = round_up(wrot_frm->out_crop.left, 4);
	else if (wrot_frm->out_crop.left & 1)
		wrot_frm->out_crop.left++; /* round_up(2) */

	if (is_change_wx(wrot_frm->rotate, wrot_frm->flip))
		wrot_frm->out_crop.left = out_crop->width - wrot_frm->out_crop.left;
	wrot_frm->out_crop.width = out_crop->width - wrot_frm->out_crop.left;
}

static inline struct wrot_frame_data *wrot_frm_data(struct mdp_alg_path_tp *path, u32 n)
{
	return path->nodes[n].data;
}

static inline const struct wrot_data *get_private_data(struct mdp_alg_task *task)
{
	switch (task->mdp->mdp_data->mdp_alg_plat) {
	case MDP_ALG_MT8189:
		return &mt8189_wrot_data;
	default:
		return 0;
	}
}

static int wrot_prepare(struct mdp_alg_task *task,
			struct mdp_alg_path_tp *path, u32 n)
{
	struct wrot_frame_data *wrot_frm;
	struct mdp_alg_path_node *node = &path->nodes[n];
	struct mdp_alg_frame_config *cfg = &task->cfg;
	struct img_ipi_frameparam *p = task->cfg.param;
	struct img_image_buffer *dst_buf = &p->outputs[node->out_idx].buffer;
	struct img_crop out_crop;
	int i;

	wrot_frm = kzalloc(sizeof(*wrot_frm), GFP_KERNEL);
	if (!wrot_frm)
		return -ENOMEM;
	node->data = wrot_frm;

	/* cache out index for easy use */
	wrot_frm->out_idx = node->out_idx;
	wrot_frm->rotate = cfg->out_rotate[wrot_frm->out_idx];
	wrot_frm->flip = cfg->out_flip[wrot_frm->out_idx];

	/* select output port struct */
	wrot_frm->y_stride = dst_buf->format.plane_fmt[0].stride;
	wrot_frm->uv_stride = dst_buf->format.plane_fmt[1].stride;
	if (wrot_frm->rotate == ROT_0 || wrot_frm->rotate == ROT_180) {
		wrot_frm->out_w = dst_buf->format.width;
		wrot_frm->out_h = dst_buf->format.height;
	} else {
		wrot_frm->out_w = dst_buf->format.height;
		wrot_frm->out_h = dst_buf->format.width;
	}

	/* make sure uv stride data */
	if (MDP_COLOR_GET_PLANE_COUNT(dst_buf->format.colorformat) > 1 &&
	    !wrot_frm->uv_stride)
		wrot_frm->uv_stride =
			mdp_color_get_min_uv_stride(dst_buf->format.colorformat,
						    dst_buf->format.width);

	out_crop.left = 0;
	out_crop.top = 0;
	out_crop.width = wrot_frm->out_w;
	out_crop.height = wrot_frm->out_h;

	if (cfg->dual) {
		if (path->path_id == 0)
			wrot_config_left(dst_buf, &out_crop, wrot_frm);
		else
			wrot_config_right(dst_buf, &out_crop, wrot_frm);
	} else {
		/* assign full frame */
		wrot_frm->en_x_crop = true;
		memcpy(&wrot_frm->out_crop, &out_crop, sizeof(struct img_crop));
	}

	for (i = 0; i < IMG_MAX_PLANES; i++)
		wrot_frm->iova[i] = dst_buf->iova[i];

	return 0;
}

static void wrot_color_fmt(struct mdp_alg_task *task, struct mdp_alg_path_tp *path,
			   u32 n, struct wrot_frame_data *wrot_frm)
{
	struct img_ipi_frameparam *p = task->cfg.param;
	struct img_image_buffer *dst_buf = &p->outputs[path->nodes[n].out_idx].buffer;
	struct device *dev = &task->mdp->pdev->dev;
	u32 fmt = dst_buf->format.colorformat;
	u32 profile_in = p->inputs[0].buffer.format.ycbcr_prof;
	u32 profile_out = dst_buf->format.ycbcr_prof;

	wrot_frm->mat_en = 0;
	wrot_frm->mat_sel = 15;
	wrot_frm->bbp_y = MDP_COLOR_BITS_PER_PIXEL(fmt);

	switch (fmt) {
	case MDP_COLOR_GREY:
		/* Y only */
		wrot_frm->bbp_uv = 0;
		wrot_frm->hor_sh_uv = 0;
		wrot_frm->ver_sh_uv = 0;
		break;
	case MDP_COLOR_RGB565:
	case MDP_COLOR_BGR565:
	case MDP_COLOR_RGB888:
	case MDP_COLOR_BGR888:
	case MDP_COLOR_RGBA8888:
	case MDP_COLOR_BGRA8888:
	case MDP_COLOR_ARGB8888:
	case MDP_COLOR_ABGR8888:
	/* HW_SUPPORT_10BIT_PATH */
	case MDP_COLOR_RGBA1010102:
	case MDP_COLOR_BGRA1010102:
	/* DMA_SUPPORT_AFBC */
	case MDP_COLOR_RGBA8888_AFBC:
	case MDP_COLOR_RGBA1010102_AFBC:
		wrot_frm->bbp_uv = 0;
		wrot_frm->hor_sh_uv = 0;
		wrot_frm->ver_sh_uv = 0;
		wrot_frm->mat_en = 1;
		break;
	case MDP_COLOR_UYVY:
	case MDP_COLOR_VYUY:
	case MDP_COLOR_YUYV:
	case MDP_COLOR_YVYU:
		/* YUV422/444, 1 plane */
		wrot_frm->bbp_uv = 0;
		wrot_frm->hor_sh_uv = 0;
		wrot_frm->ver_sh_uv = 0;
		break;
	case MDP_COLOR_I420:
	case MDP_COLOR_YV12:
		/* YUV420, 3 plane */
		wrot_frm->bbp_uv = 8;
		wrot_frm->hor_sh_uv = 1;
		wrot_frm->ver_sh_uv = 1;
		break;
	case MDP_COLOR_I422:
	case MDP_COLOR_YV16:
		/* YUV422, 3 plane */
		wrot_frm->bbp_uv = 8;
		wrot_frm->hor_sh_uv = 1;
		wrot_frm->ver_sh_uv = 0;
		break;
	case MDP_COLOR_I444:
	case MDP_COLOR_YV24:
		/* YUV444, 3 plane */
		wrot_frm->bbp_uv = 8;
		wrot_frm->hor_sh_uv = 0;
		wrot_frm->ver_sh_uv = 0;
		break;
	case MDP_COLOR_NV12:
	case MDP_COLOR_NV21:
		/* YUV420, 2 plane */
		wrot_frm->bbp_uv = 16;
		wrot_frm->hor_sh_uv = 1;
		wrot_frm->ver_sh_uv = 1;
		break;
	case MDP_COLOR_NV16:
	case MDP_COLOR_NV61:
		/* YUV422, 2 plane */
		wrot_frm->bbp_uv = 16;
		wrot_frm->hor_sh_uv = 1;
		wrot_frm->ver_sh_uv = 0;
		break;
	/* HW_SUPPORT_10BIT_PATH */
	case MDP_COLOR_NV12_10L:
	case MDP_COLOR_NV21_10L:
		/* P010 YUV420, 2 plane 10bit */
		wrot_frm->bbp_uv = 32;
		wrot_frm->hor_sh_uv = 1;
		wrot_frm->ver_sh_uv = 1;
		break;
	default:
		dev_err(dev, "[wrot] not support format %x", fmt);
		break;
	}

	/*
	 * 4'b0000: RGB to JPEG
	 * 4'b0010: RGB to BT601
	 * 4'b0011: RGB to BT709
	 * 4'b0100: JPEG to RGB
	 * 4'b0110: BT601 to RGB
	 * 4'b0111: BT709 to RGB
	 * 4'b1000: JPEG to BT601
	 * 4'b1001: JPEG to BT709
	 * 4'b1010: BT601 to JPEG
	 * 4'b1011: BT709 to JPEG
	 * 4'b1100: BT709 to BT601
	 * 4'b1101: BT601 to BT709
	 */
	if (profile_in == MDP_YCBCR_PROFILE_BT2020 ||
	    profile_in == MDP_YCBCR_PROFILE_FULL_BT709 ||
	    profile_in == MDP_YCBCR_PROFILE_FULL_BT2020)
		profile_in = MDP_YCBCR_PROFILE_BT709;

	if (wrot_frm->mat_en == 1) {
		if (profile_in == MDP_YCBCR_PROFILE_BT601)
			wrot_frm->mat_sel = 6;
		else if (profile_in == MDP_YCBCR_PROFILE_BT709)
			wrot_frm->mat_sel = 7;
		else if (profile_in == MDP_YCBCR_PROFILE_JPEG)
			wrot_frm->mat_sel = 4;
		else
			dev_err(dev, "unknown profile conversion %x",
				profile_in);
	} else {
		if (profile_in == MDP_YCBCR_PROFILE_JPEG &&
		    profile_out == MDP_YCBCR_PROFILE_BT601) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 8;
		} else if (profile_in == MDP_YCBCR_PROFILE_JPEG &&
			   profile_out == MDP_YCBCR_PROFILE_BT709) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 9;
		} else if (profile_in == MDP_YCBCR_PROFILE_BT601 &&
			   profile_out == MDP_YCBCR_PROFILE_JPEG) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 10;
		} else if (profile_in == MDP_YCBCR_PROFILE_BT709 &&
			   profile_out == MDP_YCBCR_PROFILE_JPEG) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 11;
		} else if (profile_in == MDP_YCBCR_PROFILE_BT709 &&
			   profile_out == MDP_YCBCR_PROFILE_BT601) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 12;
		} else if (profile_in == MDP_YCBCR_PROFILE_BT601 &&
			   profile_out == MDP_YCBCR_PROFILE_BT709) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 13;
		}
	}

	if (MDP_COLOR_IS_10BIT_PACKED(p->inputs[0].buffer.format.colorformat) &&
	    !MDP_COLOR_IS_10BIT_PACKED(fmt)) {
		wrot_frm->mat_en = 1;
		wrot_frm->dither_con = (0x1 << 10) +
				       (0x0 << 8) +
				       (0x0 << 4) +
				       (0x1 << 2) +
				       (0x1 << 1) +
				       (0x1 << 0);
	}
}

static void calc_plane_offset(u32 left, u32 top,
			      u32 y_stride, u32 uv_stride,
			      u32 bbp_y, u32 bbp_uv,
			      u32 hor_sh_uv, u32 ver_sh_uv,
			      u32 *offset)
{
	if (!left && !top)
		return;

	offset[0] += (left * bbp_y) + (y_stride * top);
	offset[1] += (left >> hor_sh_uv) * (bbp_uv) +
		     (top >> ver_sh_uv) * uv_stride;
	offset[2] += (left >> hor_sh_uv) * (bbp_uv) +
		     (top >> hor_sh_uv) * uv_stride;
}

static void calc_afbc_block(u32 bits_per_pixel, u32 y_stride, u32 height,
			    u64 *iova, u32 *block_x,
			    u64 *addr_c, u64 *addr_v, u64 *addr)
{
	u32 block_y;
	u64 header_sz;

	*block_x = ((y_stride << 3) / bits_per_pixel + 31) >> 5;
	block_y = (WROT_AFBC_ALIGN(height) + 7) >> 3;
	header_sz = ((((*block_x * block_y) << 4) + 1023) >> 10) << 10;

	*addr_c = iova[0];
	*addr_v = iova[2];
	*addr = *addr_c + header_sz;
}

static void wrot_calc_hw_buf_setting(const struct wrot_data *wrot,
				     u32 fmt,
				     struct wrot_frame_data *wrot_frm)
{
	if (MDP_COLOR_IS_YUV422(fmt)) {
		if (MDP_COLOR_GET_PLANE_COUNT(fmt) == 1) {
			wrot_frm->fifo_max_sz = wrot->tile_width * 32;
			wrot_frm->max_line_cnt = 32;
		} else {
			wrot_frm->fifo_max_sz = wrot->tile_width * 48;
			wrot_frm->max_line_cnt = 48;
		}
	} else if (MDP_COLOR_IS_YUV420(fmt)) {
		wrot_frm->fifo_max_sz = wrot->tile_width * 64;
		wrot_frm->max_line_cnt = 64;
	} else if (fmt == MDP_COLOR_GREY) {
		wrot_frm->fifo_max_sz = wrot->tile_width * 64;
		wrot_frm->max_line_cnt = 64;
	} else {
		wrot_frm->fifo_max_sz = wrot->tile_width * 32;
		wrot_frm->max_line_cnt = 32;
	}
}

static void wrot_config_addr(struct mdp_alg_task *task,
			     struct mdp_alg_path_tp *path, u32 n)
{
	struct wrot_frame_data *wrot_frm = wrot_frm_data(path, n);
	struct img_ipi_frameparam *p = task->cfg.param;
	struct img_image_buffer *dst_buf = &p->outputs[path->nodes[n].out_idx].buffer;
	struct cmdq_pkt *pkt = task->pkts[path->path_id];
	const phys_addr_t base_pa = path->nodes[n].comp->reg_base;
	u32 fmt = dst_buf->format.colorformat;
	u64 addr_c, addr_v, addr;

	if (MDP_COLOR_IS_AFBC(fmt)) {
		u32 block_x;
		u32 frame_size;

		calc_afbc_block(wrot_frm->bbp_y,
				wrot_frm->y_stride, dst_buf->format.height,
				wrot_frm->iova,
				&block_x, &addr_c, &addr_v, &addr);

		if (wrot_frm->rotate == ROT_0 || wrot_frm->rotate == ROT_180)
			frame_size = ((((wrot_frm->out_h + 31) >> 5) << 5) << 16) +
				     ((block_x << 5) << 0);
		else
			frame_size = ((((wrot_frm->out_w + 31) >> 5) << 5) << 16) +
				     ((block_x << 5) << 0);
		cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_FRAME_SIZE,
					  frame_size, U32_MAX);

		cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_AFBC_YUVTRANS,
					  MDP_COLOR_IS_RGB(fmt), 0x1);
	} else {
		addr = wrot_frm->iova[0];
		addr_c = wrot_frm->iova[1];
		addr_v = wrot_frm->iova[2];
	}

	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_BASE_ADDR,
				  addr, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_BASE_ADDR_C,
				  addr_c, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_BASE_ADDR_V,
				  addr_v, U32_MAX);
}

static s32 wrot_config_frame(struct mdp_alg_task *task,
			     struct mdp_alg_path_tp *path, u32 n)
{
	struct wrot_frame_data *wrot_frm = wrot_frm_data(path, n);
	const struct wrot_data *wrot = get_private_data(task);
	struct mdp_alg_frame_config *cfg = &task->cfg;
	struct img_ipi_frameparam *p = task->cfg.param;
	struct img_image_buffer *dst_buf = &p->outputs[path->nodes[n].out_idx].buffer;
	struct cmdq_pkt *pkt = task->pkts[path->path_id];
	const phys_addr_t base_pa = path->nodes[n].comp->reg_base;
	u32 dst_fmt = dst_buf->format.colorformat;
	u32 src_fmt = p->inputs[0].buffer.format.colorformat;
	const u16 rotate = wrot_frm->rotate;
	const u8 flip = wrot_frm->flip ? 1 : 0;
	const u32 h_subsample = MDP_COLOR_GET_H_SUBSAMPLE(dst_fmt);
	const u32 v_subsample = MDP_COLOR_GET_V_SUBSAMPLE(dst_fmt);
	const u8 plane = MDP_COLOR_GET_PLANE_COUNT(dst_fmt);
	const u32 preultra_en = 1;
	const u32 crop_en = 1;
	const u32 hw_fmt = MDP_COLOR_GET_UNIQUE_ID(dst_fmt);
	u32 out_swap = MDP_COLOR_IS_SWAPPED(dst_fmt);
	u32 uv_xsel, uv_ysel;
	u32 preultra, alpha;
	u32 scan_10bit = 0, bit_num = 0, pending_zero = 0, pvric = 0;

	cmdq_pkt_clear_event(pkt, path->nodes[n].comp->gce_event[MDP_GCE_EVENT_EOF]);

	wrot_color_fmt(task, path, n, wrot_frm);

	/* calculate for later config tile use */
	wrot_calc_hw_buf_setting(wrot, hw_fmt, wrot_frm);

	if (cfg->alpharot) {
		wrot_frm->mat_en = 0;

		if (wrot->rb_swap == 1) {
			if (!MDP_COLOR_IS_AFBC(src_fmt) &&
			    !MDP_COLOR_IS_10BIT_PACKED(src_fmt))
				out_swap ^= MDP_COLOR_IS_SWAPPED(src_fmt);
			else if (MDP_COLOR_IS_AFBC(src_fmt) &&
				 !MDP_COLOR_IS_10BIT_PACKED(src_fmt))
				out_swap = (MDP_COLOR_IS_SWAPPED(src_fmt) ==
					    MDP_COLOR_IS_SWAPPED(dst_fmt));
			else if (MDP_COLOR_IS_AFBC(src_fmt) &&
				 MDP_COLOR_IS_10BIT_PACKED(src_fmt))
				out_swap = out_swap ? 0 : 1;
		}
	}

	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_ROT_EN, BIT(0), BIT(0));

	if (h_subsample) {	/* YUV422/420 out */
		wrot_frm->filt_v = MDP_COLOR_GET_V_SUBSAMPLE(src_fmt) ||
				   MDP_COLOR_GET_GROUP(src_fmt) == 2 ?
				   0 : uv_table[v_subsample][rotate][flip][1];
		uv_xsel = uv_table[v_subsample][rotate][flip][2];
		uv_ysel = uv_table[v_subsample][rotate][flip][3];
	} else if (dst_fmt == MDP_COLOR_GREY) {
		uv_xsel = 0;
		uv_ysel = 0;
	} else {
		uv_xsel = 2;
		uv_ysel = 2;
	}

	if ((wrot_frm->out_crop.width & 0x1) && uv_xsel == 1)
		uv_xsel = 0;
	if ((wrot_frm->out_crop.height & 0x1) && uv_ysel == 1)
		uv_ysel = 0;

	if (out_swap == 1 && MDP_COLOR_GET_PLANE_COUNT(dst_fmt) == 3)
		swap(wrot_frm->iova[1], wrot_frm->iova[2]);

	wrot_config_addr(task, path, n);

	alpha = cfg->alpharot;
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_CTRL,
				  (uv_ysel << 30) +
				  (uv_xsel << 28) +
				  (flip << 24) +
				  (wrot_frm->rotate << 20) +
				  (alpha << 16) + /* alpha */
				  (preultra_en << 14) + /* pre-ultra */
				  (crop_en << 12) +
				  (out_swap <<  8) +
				  (hw_fmt <<  0),
				  0xf131512f);

	if (MDP_COLOR_IS_10BIT_LOOSE(dst_fmt)) {
		if (MDP_COLOR_GET_UNIQUE_ID(dst_fmt) == 12)
			scan_10bit = 1;
		else
			scan_10bit = 5;
		bit_num = 1;
	} else if (MDP_COLOR_IS_10BIT_PACKED(dst_fmt)) {
		if (MDP_COLOR_GET_UNIQUE_ID(dst_fmt) == 12)
			scan_10bit = 3;
		else
			scan_10bit = 1;
		pending_zero = BIT(26);
		bit_num = 1;
	}

	if (MDP_COLOR_IS_AFBC(dst_fmt)) {
		scan_10bit = 0;
		pending_zero = BIT(26);
	}
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_SCAN_10BIT,
				  scan_10bit, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_PENDING_ZERO,
				  pending_zero, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_CTRL_2,
				  bit_num, 0x00000007);

	if (MDP_COLOR_IS_AFBC(dst_fmt)) {
		pvric |= BIT(0);
		if (MDP_COLOR_IS_10BIT_PACKED(dst_fmt))
			pvric |= BIT(1);
	}
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_PVRIC, pvric, U32_MAX);

	if (plane == 3 || plane == 2 || hw_fmt == 7)	/* 3-plane, 2-plane, Y8 */
		preultra = (216 << 12) + (196 << 0);
	else if (hw_fmt == 0 || hw_fmt == 1)		/* RGB */
		preultra = (136 << 12) + (76 << 0);
	else if (hw_fmt == 2 || hw_fmt == 3)		/* ARGB */
		preultra = (96 << 12) + (16 << 0);
	else if (hw_fmt == 4 || hw_fmt == 5)		/* UYVY */
		preultra = (176 << 12) + (136 << 0);
	else
		preultra = 0;
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_DMA_PREULTRA, preultra, U32_MAX);

	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_STRIDE,
				  wrot_frm->y_stride, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_STRIDE_C,
				  wrot_frm->uv_stride, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_STRIDE_V,
				  wrot_frm->uv_stride, U32_MAX);

	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_MAT_CTRL,
				  (wrot_frm->mat_sel << 4) +
				  (wrot_frm->mat_en << 0), U32_MAX);

	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_DITHER,
				  0xff000000, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_RSV_1, BIT(31), BIT(31));
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_FIFO_TEST,
				  wrot->fifo, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_ROT_EN,
				  (0x1 << 23) + (0x1 << 20),
				  0x00900000);
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_DITHER_CON,
				  wrot_frm->dither_con, U32_MAX);

	return 0;
}

static void wrot_tile_calc_comp(const struct wrot_frame_data *wrot_frm,
				const struct mdp_alg_tile_engine *tile,
				struct wrot_ofst_addr *ofst)
{
	const u64 out_xs = tile->out.xs;
	const u64 out_ys = 0;
	const u64 out_w = wrot_frm->out_w;
	const u64 out_h = wrot_frm->out_h;

	if (wrot_frm->rotate == ROT_0 && !wrot_frm->flip)
		ofst->y = (out_ys / 8) * (wrot_frm->y_stride / 128) * 1024 +
			  out_xs * 32;
	else if (wrot_frm->rotate == ROT_0 && wrot_frm->flip)
		ofst->y = ((out_ys / 8) * (wrot_frm->y_stride / 128) +
			  (out_w / 32) - (out_xs / 32) - 1) * 1024;
	else if (wrot_frm->rotate == ROT_90 && !wrot_frm->flip)
		ofst->y = ((out_xs / 8) * (wrot_frm->y_stride / 128) +
			  (out_h / 32) - (out_ys / 32) - 1) * 1024;
	else if (wrot_frm->rotate == ROT_90 && wrot_frm->flip)
		ofst->y = (out_xs / 8) * (wrot_frm->y_stride / 128) * 1024 +
			  out_ys * 32;
	else if (wrot_frm->rotate == ROT_180 && !wrot_frm->flip)
		ofst->y = (((out_h / 8) - (out_ys / 8) - 1) *
			  (wrot_frm->y_stride / 128) +
			  (out_w / 32) - (out_xs / 32) - 1) * 1024;
	else if (wrot_frm->rotate == ROT_180 && wrot_frm->flip)
		ofst->y = (((out_h / 8) - (out_ys / 8) - 1) *
			  (wrot_frm->y_stride / 128)) * 1024 + out_xs * 32;
	else if (wrot_frm->rotate == ROT_270 && !wrot_frm->flip)
		ofst->y = (((out_w / 8) - (out_xs / 8) - 1) *
			  (wrot_frm->y_stride / 128)) * 1024 + out_ys * 32;
	else if (wrot_frm->rotate == ROT_270 && wrot_frm->flip)
		ofst->y = (((out_w / 8) - (out_xs / 8) - 1) *
			  (wrot_frm->y_stride / 128) +
			  (out_h / 32) - (out_ys / 32) - 1) * 1024;

	/* Target U offset. RGBA: 64, YV12 8-bit: 24, 10-bit: 32. */
	ofst->c = ofst->y / 64;
}

static void wrot_tile_calc(const struct wrot_frame_data *wrot_frm,
			   const struct mdp_alg_tile_engine *tile,
			   struct wrot_ofst_addr *ofst)
{
	/* Following data retrieve from tile calc result */
	u64 out_xs = wrot_frm->pending_x ? round_up(tile->out.xs, 2) : tile->out.xs;
	u64 out_ys = wrot_frm->pending_y ? round_up(tile->out.ys, 2) : tile->out.ys;
	u32 out_w = wrot_frm->out_w;
	u32 out_h = wrot_frm->out_h;

	if (wrot_frm->rotate == ROT_0 && !wrot_frm->flip) {
		ofst->y = out_ys * wrot_frm->y_stride +
			  (out_xs * wrot_frm->bbp_y >> 3);
		ofst->c = (out_ys >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);
		ofst->v = (out_ys >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);
	} else if (wrot_frm->rotate == ROT_0 && wrot_frm->flip) {
		ofst->y = out_ys * wrot_frm->y_stride +
			  ((out_w - out_xs) *
			  wrot_frm->bbp_y >> 3) - 1;
		ofst->c = (out_ys >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  (((out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;
		ofst->v = (out_ys >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  (((out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;
	} else if (wrot_frm->rotate == ROT_90 && !wrot_frm->flip) {
		ofst->y = out_xs * wrot_frm->y_stride +
			  ((out_h - out_ys) *
			  wrot_frm->bbp_y >> 3) - 1;
		ofst->c = (out_xs >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  (((out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;
		ofst->v = (out_xs >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  (((out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;
	} else if (wrot_frm->rotate == ROT_90 && wrot_frm->flip) {
		ofst->y = out_xs * wrot_frm->y_stride +
			  (out_ys * wrot_frm->bbp_y >> 3);
		ofst->c = (out_xs >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);
		ofst->v = (out_xs >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);
	} else if (wrot_frm->rotate == ROT_180 && !wrot_frm->flip) {
		ofst->y = (out_h - out_ys - 1) *
			  wrot_frm->y_stride +
			  ((out_w - out_xs) *
			  wrot_frm->bbp_y >> 3) - 1;
		ofst->c = ((out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  (((out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;
		ofst->v = ((out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  (((out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;
	} else if (wrot_frm->rotate == ROT_180 && wrot_frm->flip) {
		ofst->y = (out_h - out_ys - 1) *
			  wrot_frm->y_stride +
			  (out_xs * wrot_frm->bbp_y >> 3);
		ofst->c = ((out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);
		ofst->v = ((out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);
	} else if (wrot_frm->rotate == ROT_270 && !wrot_frm->flip) {
		ofst->y = (out_w - out_xs - 1) *
			  wrot_frm->y_stride +
			  (out_ys * wrot_frm->bbp_y >> 3);
		ofst->c = ((out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);
		ofst->v = ((out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);
	} else if (wrot_frm->rotate == ROT_270 && wrot_frm->flip) {
		ofst->y = (out_w - out_xs - 1) *
			  wrot_frm->y_stride +
			  ((out_h - out_ys) *
			  wrot_frm->bbp_y >> 3) - 1;
		ofst->c = ((out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  (((out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;
		ofst->v = ((out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  (((out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;
	}
}

static void wrot_check_buf(const struct img_image_buffer *dst_buf,
			   struct wrot_setting *setting,
			   const struct wrot_frame_data *wrot_frm,
			   struct check_buf_param *buf)
{
	u32 fmt = dst_buf->format.colorformat;

	/* y_buf_width is just larger than main_blk_width */
	buf->y_buf_width =
		ceil_m(setting->main_blk_width, setting->main_buf_line_num) *
		setting->main_buf_line_num;
	buf->y_buf_usage = buf->y_buf_width * setting->main_buf_line_num;
	if (buf->y_buf_usage > buf->y_buf_size) {
		setting->main_buf_line_num = setting->main_buf_line_num - 4;
		buf->y_buf_check = 0;
		buf->uv_buf_check = 0;
		return;
	}

	buf->y_buf_check = 1;

	if (!MDP_COLOR_GET_H_SUBSAMPLE(fmt)) {
		buf->uv_blk_width = setting->main_blk_width;
		buf->uv_blk_line = setting->main_buf_line_num;
	} else {
		if (!MDP_COLOR_GET_V_SUBSAMPLE(fmt)) {
			/* YUV422 */
			if (wrot_frm->rotate == ROT_0 || wrot_frm->rotate == ROT_180) {
				buf->uv_blk_width = setting->main_blk_width >> 1;
				buf->uv_blk_line = setting->main_buf_line_num;
			} else {
				buf->uv_blk_width = setting->main_blk_width;
				buf->uv_blk_line = setting->main_buf_line_num >> 1;
			}
		} else {
			/* YUV420 */
			buf->uv_blk_width = setting->main_blk_width >> 1;
			buf->uv_blk_line = setting->main_buf_line_num >> 1;
		}
	}

	buf->uv_buf_width =
		ceil_m(buf->uv_blk_width, buf->uv_blk_line) * buf->uv_blk_line;
	buf->uv_buf_usage = buf->uv_buf_width * buf->uv_blk_line;
	if (buf->uv_buf_usage > buf->uv_buf_size) {
		setting->main_buf_line_num = setting->main_buf_line_num - 4;
		buf->y_buf_check = 0;
		buf->uv_buf_check = 0;
	} else {
		buf->uv_buf_check = 1;
	}
}

static void wrot_calc_setting(u32 tile_width,
			      struct img_image_buffer *dst_buf,
			      const struct wrot_frame_data *wrot_frm,
			      struct wrot_setting *setting)
{
	u32 hw_fmt = MDP_COLOR_GET_UNIQUE_ID(dst_buf->format.colorformat);
	u32 coeff1, coeff2, temp;
	struct check_buf_param buf = {0};

	if (hw_fmt == 9 || hw_fmt == 13) {
		buf.y_buf_size = tile_width * 48;
		buf.uv_buf_size = tile_width / 2 * 48;
	} else if (hw_fmt == 8 || hw_fmt == 12) {
		buf.y_buf_size = tile_width * 64;
		buf.uv_buf_size = tile_width / 2 * 32;
	} else {
		buf.y_buf_size = tile_width * 32;
		buf.uv_buf_size = tile_width * 32;
	}

	setting->main_buf_line_num = 0;
	/* Allocate FIFO buffer */
	setting->main_blk_width = setting->tar_xsize;

	coeff1 = floor_m(wrot_frm->fifo_max_sz, setting->tar_xsize * 2) * 2;
	coeff2 = ceil_m(setting->tar_xsize, coeff1);
	temp = ceil_m(setting->tar_xsize, coeff2 * 4) * 4;

	if (temp > setting->tar_xsize)
		setting->main_buf_line_num = ceil_m(setting->tar_xsize, 4) * 4;
	else
		setting->main_buf_line_num = temp;
	if (setting->main_buf_line_num > wrot_frm->max_line_cnt)
		setting->main_buf_line_num = wrot_frm->max_line_cnt;

	/* check for internal buffer size */
	while (!buf.y_buf_check || !buf.uv_buf_check)
		wrot_check_buf(dst_buf, setting, wrot_frm, &buf);
}

static int wrot_config_tile(struct mdp_alg_task *task,
			    struct mdp_alg_path_tp *path,
			    u32 n, u32 t)
{
	struct wrot_frame_data *wrot_frm = wrot_frm_data(path, n);
	struct img_ipi_frameparam *p = task->cfg.param;
	struct img_image_buffer *dst_buf = &p->outputs[path->nodes[n].out_idx].buffer;
	struct cmdq_pkt *pkt = task->pkts[path->path_id];
	const phys_addr_t base_pa = path->nodes[n].comp->reg_base;
	struct mdp_alg_tile_engine *tile = config_get_tile(task, path, n, t);
	const struct wrot_data *wrot = get_private_data(task);
	struct wrot_ofst_addr ofst = {0};
	struct wrot_setting setting = {0};
	u32 wrot_in_xsize;
	u32 wrot_in_ysize;
	u32 wrot_tar_xsize;
	u32 wrot_tar_ysize;
	u32 buf_line_num;

	/* Fill the tile settings */
	if (MDP_COLOR_IS_AFBC(dst_buf->format.colorformat))
		wrot_tile_calc_comp(wrot_frm, tile, &ofst);
	else
		wrot_tile_calc(wrot_frm, tile, &ofst);

	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_OFST_ADDR,
				  ofst.y & GENMASK_ULL(31, 0), U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_OFST_ADDR_HIGH,
				  ofst.y >> 32, U32_MAX);

	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_OFST_ADDR_C,
				  ofst.c & GENMASK_ULL(31, 0), U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_OFST_ADDR_HIGH_C,
				  ofst.c >> 32, U32_MAX);

	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_OFST_ADDR_V,
				  ofst.v & GENMASK_ULL(31, 0), U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_OFST_ADDR_HIGH_V,
				  ofst.v >> 32, U32_MAX);

	/* Write source size and target size */
	wrot_in_xsize = tile->in.xe - tile->in.xs + 1;
	wrot_in_ysize = tile->in.ye - tile->in.ys + 1;
	wrot_tar_xsize = tile->out.xe - tile->out.xs + 1;
	wrot_tar_ysize = tile->out.ye - tile->out.ys + 1;

	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_IN_SIZE,
				  (wrot_in_ysize << 16) + (wrot_in_xsize <<  0),
				  U32_MAX);

	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_TAR_SIZE,
				  (wrot_tar_ysize << 16) + (wrot_tar_xsize <<  0),
				  U32_MAX);

	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_CROP_OFST,
				  (tile->luma.y << 16) + (tile->luma.x <<  0),
				  U32_MAX);

	if (wrot_frm->pending_x || wrot_frm->pending_y) {
		u32 pending_zero = ((wrot_frm->pending_x & wrot_tar_xsize) << 2) +
				   ((wrot_frm->pending_y & wrot_tar_ysize) << 9);

		if (wrot_frm->pending_x && !is_change_wx(wrot_frm->rotate, wrot_frm->flip))
			pending_zero |= BIT(0);
		if (wrot_frm->pending_y && !is_change_hy(wrot_frm->rotate, wrot_frm->flip))
			pending_zero |= BIT(1);

		cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_PENDING_ZERO,
					  pending_zero, U32_MAX);
	}

	/* Set max internal buffer for tile, and check for size */
	setting.tar_xsize = wrot_tar_xsize;
	wrot_calc_setting(wrot->tile_width, dst_buf, wrot_frm, &setting);
	buf_line_num = setting.main_buf_line_num;

	cmdq_pkt_write_value_addr(pkt, base_pa + VIDO_MAIN_BUF_SIZE,
				  (setting.main_blk_width << 16) |
				  (buf_line_num << 8) |
				  (wrot_frm->filt_v << 4),
				  U32_MAX);

	return 0;
}

static int wrot_wait(struct mdp_alg_task *task,
		     struct mdp_alg_path_tp *path, u32 n)
{
	struct mdp_comp *comp = path->nodes[n].comp;

	return cmdq_pkt_wfe(task->pkts[path->path_id],
			    comp->gce_event[MDP_GCE_EVENT_EOF], true);
}

static int wrot_tile_prepare(struct mdp_alg_task *task,
			     struct mdp_alg_path_tp *path,
			     u32 n,
			     struct tile_func_block *func,
			     union mdl_alg_tile_data *data)
{
	const struct wrot_frame_data *wrot_frm = wrot_frm_data(path, n);
	struct img_ipi_frameparam *p = task->cfg.param;
	struct img_image_buffer *dst_buf = &p->outputs[path->nodes[n].out_idx].buffer;
	const struct wrot_data *wrot = get_private_data(task);

	data->wrot.dest_fmt = dst_buf->format.colorformat;
	data->wrot.rotate = wrot_frm->rotate;
	data->wrot.flip = wrot_frm->flip;

	data->wrot.enable_x_crop = wrot_frm->en_x_crop;
	data->wrot.enable_y_crop = wrot_frm->en_y_crop;
	data->wrot.crop.left = wrot_frm->out_crop.left;
	data->wrot.crop.top = wrot_frm->out_crop.top;
	data->wrot.crop.width = wrot_frm->out_crop.width;
	data->wrot.crop.height = wrot_frm->out_crop.height;

	func->full_size_x_in = wrot_frm->out_w;
	func->full_size_y_in = wrot_frm->out_h;
	func->full_size_x_out = wrot_frm->out_w;
	func->full_size_y_out = wrot_frm->out_h;

	data->wrot.max_width = wrot->tile_width;

	func->type = TILE_TYPE_WDMA | TILE_TYPE_CROP_EN;
	func->init_func = tile_wrot_init;
	func->for_func = tile_wrot_for;
	func->back_func = tile_wrot_back;
	func->data = data;
	func->enable_flag = true;

	return 0;
}

const struct mdp_alg_comp_ops alg_ops_wrot = {
	.comp_prepare = wrot_prepare,
	.config_frame = wrot_config_frame,
	.config_tile = wrot_config_tile,
	.cmdq_wait = wrot_wait,
	.tile_prepare = wrot_tile_prepare,
};
