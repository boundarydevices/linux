// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include "mdp_reg_mux.h"
#include "mtk-mdp3-alg.h"
#include "mtk-mdp3-core.h"

struct tp_node {
	int to;
	struct mdp_alg_path_mux m[MDP_ALG_MAX_MUX];
};

struct tp_map {
	int from;
	struct tp_node n[MDP_ALG_MAX_OUTPUTS];
};

static const struct tp_map mt8189_path_map[TP_PATH_MAX][MDP_ALG_MAX_PATH_NODES] = {
	[TP_PATH_NO_PQ_P0] = {
		{MDP_COMP_RDMA0, {
			{MDP_COMP_RSZ2, {
				{MDP_MUX_MT8189_BYP0_MOUT_EN, 0x1, MDP_MUX_MOUT},
				{MDP_MUX_MT8189_RSZ2_SEL_IN, 0x1, MDP_MUX_SELIN},
			}},
		}},
		{MDP_COMP_RSZ2, {
			{MDP_COMP_WROT2, {
			}},
		}},
		{MDP_COMP_WROT2, {
		}},
	},
	[TP_PATH_NO_PQ_P1] = {
		{MDP_COMP_RDMA1, {
			{MDP_COMP_RSZ3, {
				{MDP_MUX_MT8189_BYP1_MOUT_EN, 0x1, MDP_MUX_MOUT},
				{MDP_MUX_MT8189_RSZ3_SEL_IN, 0x1, MDP_MUX_SELIN},
			}},
		}},
		{MDP_COMP_RSZ3, {
			{MDP_COMP_WROT3, {
			}},
		}},
		{MDP_COMP_WROT3, {
		}},
	},
};

static int tp_parse_path(struct mdp_dev *mdp, struct mdp_alg_path_tp *p,
			 const struct tp_map *map)
{
	struct device *dev = &mdp->pdev->dev;
	struct mdp_alg_path_node *prev[2] = {0};
	int cur_id[2] = {0};
	int i, tile_idx;

	for (i = 0; i < MDP_ALG_MAX_PATH_NODES; i++) {
		int from = map[i].from;
		const struct tp_node *next = map[i].n;
		s32 idx;

		if (!from)
			break;

		p->nodes[i].id = from;
		p->nodes[i].comp = mdp->comp[from];
		if (!p->nodes[i].comp) {
			dev_err(dev, "Uninit component id %d", from);
			return -EINVAL;
		}
		idx = mdp->mdp_data->comp_data[from].match.subsys_id;
		p->mmsys_idx = idx;
		p->mmsys = mdp->mm_subsys[idx].mmsys;
		p->mutex = mdp->mm_subsys[idx].mutex;
		memcpy(p->nodes[i].mux[0], map[i].n[0].m,
		       MDP_ALG_MAX_MUX * sizeof(struct mdp_alg_path_mux));
		memcpy(p->nodes[i].mux[1], map[i].n[1].m,
		       MDP_ALG_MAX_MUX * sizeof(struct mdp_alg_path_mux));

		/* check cursor for 2 out and link if id match */
		if (!cur_id[0] && next[0].to) {
			/* 1st component case */
			prev[0] = &p->nodes[i];
			cur_id[0] = next[0].to;
			p->nodes[i].out_idx = 0;
		} else if (cur_id[0] == from) {
			/* connect out 0 */
			prev[0]->next[0] = &p->nodes[i];
			p->nodes[i].prev[0] = prev[0];
			prev[0] = &p->nodes[i];
			cur_id[0] = next[0].to;
			p->nodes[i].out_idx = 0;

			/* also assign 1 in 2 out case, must branch from pipe 0 */
			if (!cur_id[1] && next[1].to) {
				prev[1] = &p->nodes[i];
				cur_id[1] = next[1].to;
			}
		} else if (cur_id[1] == from) {
			/* connect out 1 */
			if (!prev[1]->next[0])
				prev[1]->next[0] = &p->nodes[i];
			else
				prev[1]->next[1] = &p->nodes[i];
			p->nodes[i].prev[0] = prev[1];
			prev[1] = &p->nodes[i];
			cur_id[1] = next[0].to;
			p->nodes[i].out_idx = 1;

			/* at most 2 out in one path, cannot branch from pipe 1 */
			if (next[1].to) {
				dev_err(dev, "wrong path [%d]comp: %d", i, from);
				return -EINVAL;
			}
		} else {
			dev_err(dev, "connect fail [%d]comp:%d (%d->%d) and (%d->%d)",
				i, from, cur_id[0], next[0].to, cur_id[1], next[1].to);
			return -EINVAL;
		}
	}

	p->node_cnt = i;

	/* collect tile engines */
	tile_idx = 0;
	for (i = 0; i < p->node_cnt; i++) {
		if ((!p->nodes[i].prev[0] && !p->nodes[i].next[0])) {
			p->nodes[i].tile_eng_idx = ~0;
			continue;
		}
		p->nodes[i].tile_eng_idx = tile_idx;
		p->tile_engines[tile_idx++] = i;
	}
	p->tile_engine_cnt = tile_idx;

	return 0;
}

static int tp_select_path_mt8189(struct mdp_alg_task *task)
{
	struct mdp_alg_frame_config *cfg = &task->cfg;
	struct mdp_alg_path_tp *p = &cfg->path[TP_PATH_NO_PQ_P0];
	int ret;

	ret = tp_parse_path(task->mdp, p, mt8189_path_map[TP_PATH_NO_PQ_P0]);
	if (ret)
		return ret;
	p->path_id = TP_PATH_NO_PQ_P0;
	p->mmsys_idx = TP_PATH_NO_PQ_P0;
	p->mutex_idx = TP_PATH_NO_PQ_P0;

	if (cfg->param->type == MDP_STREAM_TYPE_DUAL_BITBLT) {
		p = &cfg->path[TP_PATH_NO_PQ_P1];
		ret = tp_parse_path(task->mdp, p, mt8189_path_map[TP_PATH_NO_PQ_P1]);
		if (ret)
			return ret;
		p->path_id = TP_PATH_NO_PQ_P1;
		p->mmsys_idx = TP_PATH_NO_PQ_P0;
		p->mutex_idx = TP_PATH_NO_PQ_P1;
		cfg->dual = true;
	}

	return ret;
}

int mdp_alg_tp_select_path(struct mdp_alg_task *task)
{
	switch (task->mdp->mdp_data->mdp_alg_plat) {
	case MDP_ALG_MT8189:
		return tp_select_path_mt8189(task);
	default:
		return -EINVAL;
	}
}
