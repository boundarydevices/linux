/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MDP3_ALG_H__
#define __MTK_MDP3_ALG_H__

#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mdp_tile_scaler.h"
#include "mtk-mdp3-comp.h"
#include "mtk-mdp3-regs.h"
#include "mtk-mdp3-type.h"

#define MDP_ALG_MAX_PATH_NODES	27
#define MDP_ALG_MAX_INPUTS	2
#define MDP_ALG_MAX_OUTPUTS	2
#define MDP_ALG_MAX_PIPE	2
#define MDP_ALG_MAX_MUX		6
#define MDP_ALG_MAX_TILE_NUM	8

#define has_alg_op(_comp, op) \
	(_comp->alg_ops && _comp->alg_ops->op)
#define call_alg_op(_comp, op, ...) \
	(has_alg_op(_comp, op) ? \
		_comp->alg_ops->op(__VA_ARGS__) : 0)

struct mdp_dev;
struct mdp_comp;
struct tile_func_block;

enum alg_topology_scenario {
	TP_PATH_NO_PQ_P0,
	TP_PATH_NO_PQ_P1,
	TP_PATH_MAX
};

enum alg_orientation {
	ROT_0 = 0,
	ROT_90,
	ROT_180,
	ROT_270
};

enum mdp_alg_platform {
	MDP_ALG_NOSUPPPORT = 0,
	MDP_ALG_MT8189,
};

enum mdp_alg_mux_type {
	MDP_MUX_UNUSED = 0,
	MDP_MUX_MOUT,
	MDP_MUX_SOUT,
	MDP_MUX_SELIN,
};

struct mdp_alg_path_mux {
	u16 ofst;
	u16 val;
	u16 type;
};

struct mdp_alg_path_node {
	int id;
	struct mdp_alg_path_node *prev[MDP_ALG_MAX_INPUTS];
	struct mdp_alg_path_node *next[MDP_ALG_MAX_OUTPUTS];
	struct mdp_comp *comp;
	struct mdp_alg_path_mux mux[MDP_ALG_MAX_OUTPUTS][MDP_ALG_MAX_MUX];
	u8 out_idx;
	u8 tile_eng_idx;
	/* The component private data. */
	void *data;
};

struct mdp_alg_path_tp {
	u8 path_id;
	/* Nodes of this path topology, each node may link to others as a tree. */
	struct mdp_alg_path_node nodes[MDP_ALG_MAX_PATH_NODES];
	u8 node_cnt;

	struct device *mmsys;
	s32 mmsys_idx;
	struct device *mutex;
	s32 mutex_idx;

	u8 tile_engines[MDP_ALG_MAX_PATH_NODES];
	u8 tile_engine_cnt;
	u64 engine_flags;
};

struct mdp_alg_tile_engine {
	u8 comp_id;
	struct tile_region in;
	struct tile_region out;
	struct tile_offset luma;
	struct tile_offset chroma;
};

struct mdp_alg_tile_config {
	u16 tile_no;

	/* Current horizontal tile number, from left to right */
	u16 h_tile_no;
	/* Current vertical tile number, from top to bottom */
	u16 v_tile_no;

	u8 engine_cnt;
	struct mdp_alg_tile_engine tile_engines[MDP_ALG_MAX_PATH_NODES];

	/* Assign by wrot, end of current tile line */
	bool eol;
};

struct mdp_alg_tile_frame {
	u16 tile_cnt;
	u16 h_tile_cnt;
	u16 v_tile_cnt;
	/* Source crop with tile overhead */
	struct img_rect src_crop;
	struct mdp_alg_tile_config *tiles;
};

struct mdp_alg_tile_cache {
	void *func_list[MDP_ALG_MAX_PATH_NODES];
	struct mdp_alg_tile_config *tiles;
	bool ready;
};

struct tile_data_rdma {
	enum mdp_color src_fmt;
	u32 blk_shift_w;
	u32 blk_shift_h;
	struct img_rect crop;
	u32 max_width;
	u8 align_x;
	u16 read_rotate;
};

struct tile_data_rsz {
	bool use_121filter;
	u32 coeff_step_x;
	u32 coeff_step_y;
	u32 precision_x;
	u32 precision_y;
	struct img_crop crop;
	bool hor_scale;
	enum scaler_algo hor_algo;
	bool ver_scale;
	enum scaler_algo ver_algo;
	s32 c42_out_frame_w;
	s32 c24_in_frame_w;
	s32 prz_out_tile_w;
	s32 prz_back_xs;
	s32 prz_back_xe;
	bool ver_first;
	bool ver_cubic_trunc;
	u32 max_width;
	bool crop_aal_tile_loss;
};

struct tile_data_tdshp {
	bool relay_mode;
	u32 max_width;
};

struct tile_data_wrot {
	enum mdp_color dest_fmt;
	u32 rotate;
	bool flip;
	bool alpha;
	bool racing;
	u8 racing_h;
	bool enable_x_crop;
	bool enable_y_crop;
	bool yuv_pending;
	struct img_rect crop;
	u32 max_width;
	u8 align_x;
	u8 align_y;
	bool first_x_pad;
	bool first_y_pad;
};

union mdl_alg_tile_data {
	struct tile_data_rdma rdma;
	struct tile_data_rsz rsz;
	struct tile_data_tdshp tdshp;
	struct tile_data_wrot wrot;
};

struct tile_ctx {
	/* Output */
	struct mdp_alg_tile_frame *output;
	/* Working */
	union mdl_alg_tile_data *tile_datas;
	struct tile_reg_map *tile_reg_map;
	struct func_description *tile_func;
};

struct mdp_alg_frame_config {
	struct img_ipi_frameparam *param;
	struct img_frame frame_in;
	struct img_frame frame_out[MDP_ALG_MAX_OUTPUTS];
	struct img_crop frame_in_crop[MDP_ALG_MAX_OUTPUTS];
	struct img_frame frame_tile_sz;
	u8 out_rotate[MDP_ALG_MAX_OUTPUTS];
	bool out_flip[MDP_ALG_MAX_OUTPUTS];

	/* Topology */
	struct mdp_alg_path_tp path[MDP_ALG_MAX_PIPE];
	bool dual:1;
	bool alpharot:1;

	/* Tile */
	struct mdp_alg_tile_frame *tile[MDP_ALG_MAX_PIPE];
	u32 hist_div[MDP_ALG_MAX_PATH_NODES];
};

struct mdp_alg_task {
	struct mdp_dev *mdp;
	void *ctx;
	struct mdp_alg_tile_cache *t_cache[MDP_ALG_MAX_PIPE];
	struct mdp_alg_frame_config cfg;
	struct cmdq_pkt *pkts[MDP_ALG_MAX_PIPE];
};

struct mdp_alg_comp_ops {
	int (*comp_prepare)(struct mdp_alg_task *task,
			    struct mdp_alg_path_tp *path,
			    u32 n);
	int (*comp_init)(struct mdp_alg_task *task,
			 struct mdp_alg_path_tp *path,
			 u32 n);
	int (*config_frame)(struct mdp_alg_task *task,
			    struct mdp_alg_path_tp *path,
			    u32 n);
	int (*config_tile)(struct mdp_alg_task *task,
			   struct mdp_alg_path_tp *path,
			   u32 n,
			   u32 t);
	int (*cmdq_wait)(struct mdp_alg_task *task,
			 struct mdp_alg_path_tp *path,
			 u32 n);
	int (*tile_prepare)(struct mdp_alg_task *task,
			    struct mdp_alg_path_tp *path,
			    u32 n,
			    struct tile_func_block *func,
			    union mdl_alg_tile_data *data);
};

extern const struct mdp_alg_comp_ops *mdp_alg_ops[MDP_COMP_TYPE_COUNT];
extern const struct mdp_alg_comp_ops alg_ops_rdma;
extern const struct mdp_alg_comp_ops alg_ops_rsz;
extern const struct mdp_alg_comp_ops alg_ops_wrot;

static inline struct mdp_alg_tile_engine *config_get_tile(struct mdp_alg_task *task,
							  struct mdp_alg_path_tp *path,
							  u32 n, u32 t)
{
	struct mdp_alg_tile_engine *engines =
		task->cfg.tile[path->path_id]->tiles[t].tile_engines;

	return &engines[path->nodes[n].tile_eng_idx];
}

int mdp_alg_tp_select_path(struct mdp_alg_task *task);
int mdp_alg_tile_calc(struct mdp_alg_task *task, u32 pipe);
int mdp_alg_submit(struct mdp_alg_task *task);
int mdp_alg_process(struct mdp_m2m_ctx *ctx, struct img_ipi_frameparam *param);

#endif  /* __MTK_MDP3_ALG_H__ */
