// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include "mdp_reg_rdma.h"
#include "mdp_tile_core.h"
#include "mdp_tile_comp.h"
#include "mtk-mdp3-alg.h"
#include "mtk-mdp3-core.h"

struct rdma_data {
	u32 tile_width;
	u8 rb_swap;	/* version for rb channel swap behavior */
};

static const struct rdma_data mt8189_rdma_data = {
	.tile_width = 640,
	.rb_swap = 2,
};

struct rdma_frame_data {
	u8 enable_ufo;
	u8 hw_fmt;
	u8 swap;
	u8 blk;
	u8 lb_2b_mode;
	u8 field;
	u8 blk_10bit;
	u8 blk_tile;
	u8 color_tran;
	u8 matrix_sel;
	u32 bits_per_pixel_y;
	u32 bits_per_pixel_uv;
	u32 hor_shift_uv;
	u32 ver_shift_uv;
	u32 vdo_blk_shift_w;
	u32 vdo_blk_height;
	u32 vdo_blk_shift_h;
	u32 datasize;		/* qos data size in bytes */
	u16 crop_off_l;		/* crop offset left */
	u16 crop_off_t;		/* crop offset top */
	u32 gmcif_con;
	u32 crc_inst_offset;
	bool ultra_off;
};

static inline struct rdma_frame_data *rdma_frm_data(struct mdp_alg_path_tp *path, u32 n)
{
	return path->nodes[n].data;
}

static inline const struct rdma_data *get_private_data(struct mdp_alg_task *task)
{
	switch (task->mdp->mdp_data->mdp_alg_plat) {
	case MDP_ALG_MT8189:
		return &mt8189_rdma_data;
	default:
		return 0;
	}
}

static void rdma_color_fmt(struct device *dev,
			   struct mdp_alg_frame_config *cfg,
			   struct rdma_frame_data *rdma_frm)
{
	u32 src_fmt = cfg->param->inputs[0].buffer.format.colorformat;
	u32 profile_in = cfg->param->inputs[0].buffer.format.ycbcr_prof;

	rdma_frm->color_tran = 0;
	rdma_frm->matrix_sel = 15;

	rdma_frm->hw_fmt = MDP_COLOR_GET_UNIQUE_ID(src_fmt);
	rdma_frm->swap = MDP_COLOR_IS_SWAPPED(src_fmt);
	rdma_frm->blk = MDP_COLOR_IS_BLOCK_MODE(src_fmt);
	rdma_frm->lb_2b_mode = rdma_frm->blk ? 0 : 1;
	rdma_frm->field = MDP_COLOR_IS_INTERLACED(src_fmt);
	rdma_frm->blk_10bit = MDP_COLOR_IS_10BIT_PACKED(src_fmt);
	rdma_frm->blk_tile = MDP_COLOR_IS_10BIT_TILE(src_fmt);

	switch (src_fmt) {
	case MDP_COLOR_GREY:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MDP_COLOR_RGB565:
	case MDP_COLOR_BGR565:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		rdma_frm->color_tran = 1;
		break;
	case MDP_COLOR_RGB888:
	case MDP_COLOR_BGR888:
		rdma_frm->bits_per_pixel_y = 24;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		rdma_frm->color_tran = 1;
		break;
	case MDP_COLOR_RGBA8888:
	case MDP_COLOR_BGRA8888:
	case MDP_COLOR_ARGB8888:
	case MDP_COLOR_ABGR8888:
	case MDP_COLOR_RGBA1010102:
	case MDP_COLOR_BGRA1010102:
	case MDP_COLOR_RGBA8888_AFBC:
	case MDP_COLOR_RGBA1010102_AFBC:
		rdma_frm->bits_per_pixel_y = 32;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		rdma_frm->color_tran = 1;
		break;
	case MDP_COLOR_UYVY:
	case MDP_COLOR_VYUY:
	case MDP_COLOR_YUYV:
	case MDP_COLOR_YVYU:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MDP_COLOR_I420:
	case MDP_COLOR_YV12:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 8;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MDP_COLOR_I422:
	case MDP_COLOR_YV16:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 8;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MDP_COLOR_I444:
	case MDP_COLOR_YV24:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 8;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MDP_COLOR_NV12:
	case MDP_COLOR_NV21:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MDP_COLOR_YUV420_AFBC:
	case MDP_COLOR_NV12_HYFBC:
		rdma_frm->bits_per_pixel_y = 12;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MDP_COLOR_420_BLK_UFO:
	case MDP_COLOR_420_BLK:
		rdma_frm->vdo_blk_shift_w = 4;
		rdma_frm->vdo_blk_height = 32;
		rdma_frm->vdo_blk_shift_h = 5;
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MDP_COLOR_NV16:
	case MDP_COLOR_NV61:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MDP_COLOR_NV24:
	case MDP_COLOR_NV42:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MDP_COLOR_NV12_10L:
	case MDP_COLOR_NV21_10L:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 32;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MDP_COLOR_YUV420_10P_AFBC:
	case MDP_COLOR_P010_HYFBC:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	default:
		dev_err(dev, "[rdma] not support format %x", src_fmt);
		break;
	}

	if (profile_in == MDP_YCBCR_PROFILE_BT2020 ||
	    profile_in == MDP_YCBCR_PROFILE_FULL_BT709 ||
	    profile_in == MDP_YCBCR_PROFILE_FULL_BT2020)
		profile_in = MDP_YCBCR_PROFILE_BT709;

	if (rdma_frm->color_tran) {
		if (profile_in == MDP_YCBCR_PROFILE_BT601)
			rdma_frm->matrix_sel = 2;
		else if (profile_in == MDP_YCBCR_PROFILE_BT709)
			rdma_frm->matrix_sel = 3;
		else if (profile_in == MDP_YCBCR_PROFILE_FULL_BT601)
			rdma_frm->matrix_sel = 0;
		else
			dev_err(dev, "[rdma] unknown color conversion %x",
				profile_in);
	}
}

static void calc_hyfbc(struct img_image_buffer *src_buf,
		       u64 *y_header_addr, u64 *y_data_addr,
		       u64 *c_header_addr, u64 *c_data_addr)
{
	u64 buf_addr = src_buf->iova[0];
	u32 width = round_up(src_buf->format.width, 64);
	u32 height = round_up(src_buf->format.height, 64);
	u32 y_data_sz = width * height;
	u32 c_data_sz;
	u32 y_header_sz;
	u32 c_header_sz;
	u32 total_sz;

	if (MDP_COLOR_IS_10BIT_PACKED(src_buf->format.colorformat))
		y_data_sz = y_data_sz * 6 >> 2;

	c_data_sz = y_data_sz >> 1;
	y_header_sz = (width * height + 63) >> 6;
	c_header_sz = ((width * height >> 1) + 63) >> 6;

	*y_data_addr = round_up(buf_addr + y_header_sz, 4096);
	*y_header_addr = *y_data_addr - y_header_sz;	/* should be 64 aligned */
	*c_data_addr = round_up(*y_data_addr + y_data_sz + c_header_sz, 4096);
	*c_header_addr = round_down(*c_data_addr - c_header_sz, 64);

	total_sz = (u32)(*c_data_addr + c_data_sz - buf_addr);
}

static int rdma_prepare(struct mdp_alg_task *task,
			struct mdp_alg_path_tp *path, u32 n)
{
	struct rdma_frame_data *rdma_frm;

	rdma_frm = kzalloc(sizeof(*rdma_frm), GFP_KERNEL);
	if (!rdma_frm)
		return -ENOMEM;
	path->nodes[n].data = (void *)rdma_frm;

	return 0;
}

static int rdma_config_frame(struct mdp_alg_task *task,
			     struct mdp_alg_path_tp *path, u32 n)
{
	const struct rdma_data *rdma = get_private_data(task);
	struct mdp_alg_frame_config *cfg = &task->cfg;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(path, n);
	struct mdp_alg_path_node *node = &path->nodes[n];
	struct img_ipi_frameparam *param = task->cfg.param;
	struct img_image_buffer *src_buf = &param->inputs[0].buffer;
	struct img_image_buffer *dst_buf = &param->outputs[node->out_idx].buffer;
	struct cmdq_pkt *pkt = task->pkts[path->path_id];
	const phys_addr_t base_pa = node->comp->reg_base;
	const u32 src_fmt = src_buf->format.colorformat;
	const u32 dst_fmt = dst_buf->format.colorformat;
	u8 simple_mode = 1;
	u8 filter_mode;
	u8 loose = 0;
	u8 bit_number = 0;
	u8 ufo_auo = 0;
	u8 ufo_jump = 0;
	u8 afbc = 0;
	u8 afbc_y2r = 0;
	u8 hyfbc = 0;
	u8 ufbdc = 0;
	u8 output_10bit = 0;
	u32 width_in_pxl = 0;
	u32 height_in_pxl = 0;
	u64 iova[3];
	u64 ufo_dec_length_y = 0;
	u64 ufo_dec_length_c = 0;
	u8 in_swap;

	/* clear event */
	cmdq_pkt_clear_event(pkt, node->comp->gce_event[MDP_GCE_EVENT_EOF]);

	/* Enable engine */
	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_EN, BIT(0), U32_MAX);

	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_GMCIF_CON,
				  BIT(0) |        /* COMMAND_DIV */
				  GENMASK(7, 4) | /* READ_REQUEST_TYPE */
				  GENMASK(9, 8) | /* WRITE_REQUEST_TYPE */
				  BIT(16),        /* PRE_ULTRA_EN */
				  U32_MAX);

	rdma_color_fmt(&task->mdp->pdev->dev, cfg, rdma_frm);

	if (MDP_COLOR_IS_10BIT_PACKED(src_fmt))
		rdma_frm->color_tran = 1;

	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_TRANSFORM_0,
				  (rdma_frm->matrix_sel << 23) +
				  (rdma_frm->color_tran << 16),
				  0x0F110000);

	if (MDP_COLOR_GET_V_SUBSAMPLE(src_fmt) &&
	    !MDP_COLOR_GET_V_SUBSAMPLE(dst_fmt) &&
	    !MDP_COLOR_IS_BLOCK_MODE(src_fmt))
		/* 420 to 422 interpolation solution */
		filter_mode = 2;
	else
		/* config.enRDMACrop ? 3 : 2 */
		/* RSZ uses YUV422, RDMA could use V filter unless cropping */
		filter_mode = 3;

	if (MDP_COLOR_IS_10BIT_LOOSE(src_fmt))
		loose = 1;
	if (MDP_COLOR_IS_10BIT_PACKED(src_fmt))
		bit_number = 1;

	in_swap = rdma_frm->swap;
	if (MDP_COLOR_IS_AFBC(dst_fmt) && MDP_COLOR_IS_10BIT_PACKED(dst_fmt)) {
		if (rdma->rb_swap == 1) {
			if (MDP_COLOR_IS_RGB(src_fmt) &&
			    !MDP_COLOR_IS_SWAPPED(dst_fmt))
				in_swap = in_swap ? 0 : 1;
		} else if (rdma->rb_swap == 2) {
			if (MDP_COLOR_IS_RGB(src_fmt) &&
			    !MDP_COLOR_IS_SWAPPED(dst_fmt))
				in_swap = in_swap ? 0 : 1;
		}
	}

	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_SRC_CON,
				  (rdma_frm->hw_fmt << 0) +
				  (filter_mode << 9) +
				  (loose << 11) +
				  (rdma_frm->field << 12) +
				  (in_swap << 14) +
				  (rdma_frm->blk << 15) +
				  (1 << 17) +	/* UNIFORM_CONFIG */
				  (bit_number << 18) +
				  (rdma_frm->blk_tile << 23) +
				  (0 << 24),	/* RING_BUF_READ */
				  U32_MAX);

	if (rdma_frm->blk_10bit)
		ufo_jump = MDP_COLOR_IS_10BIT_JUMP(src_fmt);
	else
		ufo_auo = MDP_COLOR_IS_AUO(src_fmt);

	if (MDP_COLOR_IS_HYFBC(src_fmt)) {
		hyfbc = 1;
		ufbdc = 1;
		width_in_pxl = round_up(src_buf->format.width, 32);
		height_in_pxl = round_up(src_buf->format.height, 16);
	} else if (MDP_COLOR_IS_AFBC(src_fmt)) {
		afbc = 1;
		if (MDP_COLOR_IS_RGB(src_fmt))
			afbc_y2r = 1;
		ufbdc = 1;
		if (MDP_COLOR_IS_YUV(src_fmt)) {
			width_in_pxl = round_up(src_buf->format.width, 16);
			height_in_pxl = round_up(src_buf->format.height, 16);
		} else {
			width_in_pxl = round_up(src_buf->format.width, 32);
			height_in_pxl = round_up(src_buf->format.height, 8);
		}
	} else if (rdma_frm->enable_ufo && rdma_frm->blk_10bit) {
		width_in_pxl = src_buf->format.plane_fmt[0].stride << 2;
		width_in_pxl = (width_in_pxl << 2) / 5;
	}

	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_MF_BKGD_SIZE_IN_PXL,
				  width_in_pxl, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_MF_BKGD_H_SIZE_IN_PXL,
				  height_in_pxl, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_AFBC_PAYLOAD_OST,
				  0, U32_MAX);

	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_COMP_CON,
				  (rdma_frm->enable_ufo << 31) +
				  (ufo_auo << 29) +
				  (ufo_jump << 28) +
				  (1 << 26) +	/* UFO_DATA_IN_NOT_REV */
				  (1 << 25) +	/* UFO_DATA_OUT_NOT_REV */
				  (0 << 24) +	/* ufo_dcp */
				  (0 << 23) +	/* ufo_dcp_10bit */
				  (afbc << 22) +
				  (afbc_y2r << 21) +
				  (0 << 20) +	/* pvric_en */
				  (1 << 19) +	/* SHORT_BURST */
				  (12 << 14) +	/* UFBDC_HG_DISABLE */
				  (hyfbc << 13) +
				  (ufbdc << 12) +
				  (1 << 11),	/* payload_ost */
				  U32_MAX);

	if (MDP_COLOR_IS_HYFBC(src_fmt))
		calc_hyfbc(src_buf, &ufo_dec_length_y, &iova[0],
			   &ufo_dec_length_c, &iova[1]);

	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_UFO_DEC_LENGTH_BASE_Y,
				  ufo_dec_length_y, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_UFO_DEC_LENGTH_BASE_C,
				  ufo_dec_length_c, U32_MAX);

	if (MDP_COLOR_IS_10BIT_PACKED(src_fmt) || MDP_COLOR_IS_10BIT_PACKED(dst_fmt))
		output_10bit = 1;
	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_CON,
				  (rdma_frm->lb_2b_mode << 12) +
				  (output_10bit << 5) +
				  (simple_mode << 4),
				  U32_MAX);

	/* Write frame base address */
	if (MDP_COLOR_IS_HYFBC(src_fmt)) {
		/* clear since not use */
		iova[2] = 0;
	} else {
		iova[0] = src_buf->iova[0];
		iova[1] = src_buf->iova[1];
		iova[2] = src_buf->iova[2];
	}

	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_SRC_BASE_0,
				  iova[0], U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_SRC_BASE_1,
				  iova[1], U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_SRC_BASE_2,
				  iova[2], U32_MAX);

	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_MF_BKGD_SIZE_IN_BYTE,
				  src_buf->format.plane_fmt[0].stride, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_SF_BKGD_SIZE_IN_BYTE,
				  src_buf->format.plane_fmt[1].stride, U32_MAX);

	return 0;
}

static int rdma_config_tile(struct mdp_alg_task *task,
			    struct mdp_alg_path_tp *path,
			    u32 n, u32 t)
{
	struct rdma_frame_data *rdma_frm = rdma_frm_data(path, n);
	struct img_ipi_frameparam *param = task->cfg.param;
	struct img_image_buffer *src_buf = &param->inputs[0].buffer;
	u32 src_fmt = src_buf->format.colorformat;
	struct cmdq_pkt *pkt = task->pkts[path->path_id];
	const phys_addr_t base_pa = path->nodes[n].comp->reg_base;
	struct mdp_alg_tile_engine *tile = config_get_tile(task, path, n, t);
	u64 src_offset_0;
	u64 src_offset_1;
	u64 src_offset_2;
	u32 src_offset_0_p = 0;
	u32 mf_src_w;
	u32 mf_src_h;
	u32 mf_clip_w;
	u32 mf_clip_h;
	u32 mf_offset_w_1;
	u32 mf_offset_h_1;
	/* Following data retrieve from tile calc result */
	u64 in_xs = tile->in.xs;
	const u32 in_xe = tile->in.xe;
	u64 in_ys = tile->in.ys;
	const u32 in_ye = tile->in.ye;
	const u32 out_xs = tile->out.xs;
	const u32 out_xe = tile->out.xe;
	const u64 out_ys = tile->out.ys;
	const u32 out_ye = tile->out.ye;
	const u32 crop_ofst_x = tile->luma.x;
	const u32 crop_ofst_y = tile->luma.y;

	if (rdma_frm->blk) {
		/* Alignment X left in block boundary */
		in_xs = ((in_xs >> rdma_frm->vdo_blk_shift_w) <<
			rdma_frm->vdo_blk_shift_w);
		/* Alignment Y top in block boundary */
		in_ys = ((in_ys >> rdma_frm->vdo_blk_shift_h) <<
			rdma_frm->vdo_blk_shift_h);
	}

	if (MDP_COLOR_IS_AFBC(src_fmt) || MDP_COLOR_IS_HYFBC(src_fmt))
		src_offset_0_p = (in_xs & 0xFF) | ((in_ys & 0xFF) << 8);

	if (!rdma_frm->blk) {
		src_offset_0 = (in_xs * rdma_frm->bits_per_pixel_y >> 3) +
				in_ys * src_buf->format.plane_fmt[0].stride;
		src_offset_1 = ((in_xs >> rdma_frm->hor_shift_uv) *
				rdma_frm->bits_per_pixel_uv >> 3) +
				(in_ys >> rdma_frm->ver_shift_uv) *
				src_buf->format.plane_fmt[1].stride;
		src_offset_2 = ((in_xs >> rdma_frm->hor_shift_uv) *
				rdma_frm->bits_per_pixel_uv >> 3) +
				(in_ys >> rdma_frm->ver_shift_uv) *
				src_buf->format.plane_fmt[1].stride;
		mf_src_w = in_xe - in_xs + 1;
		mf_src_h = in_ye - in_ys + 1;

		mf_clip_w = out_xe - out_xs + 1;
		mf_clip_h = out_ye - out_ys + 1;

		mf_offset_w_1 = crop_ofst_x;
		mf_offset_h_1 = crop_ofst_y;
	} else {
		src_offset_0 = (in_xs *
			       (rdma_frm->vdo_blk_height << rdma_frm->field) *
			       rdma_frm->bits_per_pixel_y >> 3) +
			       (in_ys >> rdma_frm->vdo_blk_shift_h) *
			       src_buf->format.plane_fmt[0].stride;
		src_offset_1 = ((in_xs >> rdma_frm->hor_shift_uv) *
			       ((rdma_frm->vdo_blk_height >>
			       rdma_frm->ver_shift_uv) << rdma_frm->field) *
			       rdma_frm->bits_per_pixel_uv >> 3) +
			       (in_ys >> rdma_frm->vdo_blk_shift_h) *
			       src_buf->format.plane_fmt[1].stride;
		src_offset_2 = ((in_xs >> rdma_frm->hor_shift_uv) *
			       ((rdma_frm->vdo_blk_height >>
			       rdma_frm->ver_shift_uv) << rdma_frm->field) *
			       rdma_frm->bits_per_pixel_uv >> 3) +
			       (in_ys >> rdma_frm->vdo_blk_shift_h) *
			       src_buf->format.plane_fmt[1].stride;
		mf_src_w = in_xe - in_xs + 1;
		mf_src_h = (in_ye - in_ys + 1) << rdma_frm->field;

		mf_clip_w = out_xe - out_xs + 1;
		mf_clip_h = (out_ye - out_ys + 1) << rdma_frm->field;

		mf_offset_w_1 = out_xs + rdma_frm->crop_off_l - in_xs;
		mf_offset_h_1 = (out_ys + rdma_frm->crop_off_t - in_ys) << rdma_frm->field;
	}

	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_SRC_OFFSET_0,
				  src_offset_0, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_SRC_OFFSET_1,
				  src_offset_1, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_SRC_OFFSET_2,
				  src_offset_2, U32_MAX);

	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_SRC_OFFSET_0_P,
				  src_offset_0_p, U32_MAX);

	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_MF_SRC_SIZE,
				  (mf_src_h << 16) + mf_src_w, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_MF_CLIP_SIZE,
				  (mf_clip_h << 16) + mf_clip_w, U32_MAX);
	cmdq_pkt_write_value_addr(pkt, base_pa + MDP_RDMA_MF_OFFSET_1,
				  (mf_offset_h_1 << 16) + mf_offset_w_1, U32_MAX);

	return 0;
}

static int rdma_wait(struct mdp_alg_task *task,
		     struct mdp_alg_path_tp *path, u32 n)
{
	struct mdp_comp *comp = path->nodes[n].comp;

	return cmdq_pkt_wfe(task->pkts[path->path_id],
			    comp->gce_event[MDP_GCE_EVENT_EOF], true);
}

static int rdma_tile_prepare(struct mdp_alg_task *task,
			     struct mdp_alg_path_tp *path,
			     u32 n,
			     struct tile_func_block *func,
			     union mdl_alg_tile_data *data)
{
	struct mdp_alg_frame_config *cfg = &task->cfg;
	struct img_ipi_frameparam *param = task->cfg.param;
	struct img_image_buffer *src_buf = &param->inputs[0].buffer;
	u32 src_fmt = src_buf->format.colorformat;
	const struct rdma_data *rdma = get_private_data(task);

	data->rdma.src_fmt = src_fmt;
	data->rdma.blk_shift_w = MDP_COLOR_IS_BLOCK_MODE(src_fmt) ? 4 : 0;
	data->rdma.blk_shift_h = MDP_COLOR_IS_BLOCK_MODE(src_fmt) ? 5 : 0;
	data->rdma.max_width = rdma->tile_width;

	/* RDMA support crop capability */
	func->type = TILE_TYPE_RDMA | TILE_TYPE_CROP_EN;
	func->init_func = tile_rdma_init;
	func->for_func = tile_rdma_for;
	func->back_func = tile_rdma_back;
	func->data = data;
	func->enable_flag = true;

	func->full_size_x_in = src_buf->format.width;
	func->full_size_y_in = src_buf->format.height;
	func->full_size_x_out = cfg->frame_tile_sz.width;
	func->full_size_y_out = cfg->frame_tile_sz.height;

	if (param->num_outputs == 1) {
		struct rdma_frame_data *rdma_frm = rdma_frm_data(path, n);

		data->rdma.crop.left = (u32)param->outputs[0].crop.left;
		data->rdma.crop.top = (u32)param->outputs[0].crop.top;
		data->rdma.crop.width = param->outputs[0].crop.width;
		data->rdma.crop.height = param->outputs[0].crop.height;
		rdma_frm->crop_off_l = data->rdma.crop.left;
		rdma_frm->crop_off_t = data->rdma.crop.top;
	} else {
		data->rdma.crop.left = 0;
		data->rdma.crop.top = 0;
		data->rdma.crop.width = src_buf->format.width;
		data->rdma.crop.height = src_buf->format.height;
	}

	return 0;
}

const struct mdp_alg_comp_ops alg_ops_rdma = {
	.comp_prepare = rdma_prepare,
	.config_frame = rdma_config_frame,
	.config_tile = rdma_config_tile,
	.cmdq_wait = rdma_wait,
	.tile_prepare = rdma_tile_prepare,
};
