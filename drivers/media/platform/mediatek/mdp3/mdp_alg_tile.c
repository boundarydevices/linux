// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include "mdp_tile_core.h"
#include "mtk-mdp3-alg.h"
#include "mtk-mdp3-comp.h"
#include "mtk-mdp3-core.h"

static int tile_message_to_errno(enum mdp_tile_msg result)
{
	switch (result) {
	case T_OK:
		return 0;
	case T_OVER_MAX_BRANCH_NO_ERROR:
	case T_TILE_FUNC_CANNOT_FIND_LAST_FUNC_ERROR:
	case T_SCHEDULING_BACKWARD_ERROR:
	case T_SCHEDULING_FORWARD_ERROR:
	case M_NULL_DATA:
	case M_INVALID_STATE:
		return -EINVAL;
	case T_IN_CONST_X_ERROR:
	case T_IN_CONST_Y_ERROR:
	case T_OUT_CONST_X_ERROR:
	case T_OUT_CONST_Y_ERROR:
	case T_INIT_INCOR_X_INPUT_SIZE_POS_ERROR:
	case T_INIT_INCOR_Y_INPUT_SIZE_POS_ERROR:
	case T_INIT_INCOR_X_OUTPUT_SIZE_POS_ERROR:
	case T_INIT_INCOR_Y_OUTPUT_SIZE_POS_ERROR:
	case T_DISABLE_FUNC_X_SIZE_CHECK_ERROR:
	case T_DISABLE_FUNC_Y_SIZE_CHECK_ERROR:
		return -EDOM;
	case T_TILE_LOSS_OVER_TILE_HEIGHT_ERROR:
	case T_TILE_LOSS_OVER_TILE_WIDTH_ERROR:
	case T_CHECK_IN_CONFIG_ALIGN_XS_POS_ERROR:
	case T_CHECK_IN_CONFIG_ALIGN_XE_POS_ERROR:
	case T_CHECK_IN_CONFIG_ALIGN_YS_POS_ERROR:
	case T_CHECK_IN_CONFIG_ALIGN_YE_POS_ERROR:
	case T_XSIZE_NOT_DIV_BY_IN_CONST_X_ERROR:
	case T_YSIZE_NOT_DIV_BY_IN_CONST_Y_ERROR:
	case T_XSIZE_NOT_DIV_BY_OUT_CONST_X_ERROR:
	case T_YSIZE_NOT_DIV_BY_OUT_CONST_Y_ERROR:
	case T_TILE_FORWARD_OUT_OVER_TILE_WIDTH_ERROR:
	case T_TILE_FORWARD_OUT_OVER_TILE_HEIGHT_ERROR:
	case T_TILE_BACKWARD_IN_OVER_TILE_WIDTH_ERROR:
	case T_TILE_BACKWARD_IN_OVER_TILE_HEIGHT_ERROR:
	case T_FWD_CHECK_TOP_EDGE_ERROR:
	case T_FWD_CHECK_BOTTOM_EDGE_ERROR:
	case T_FWD_CHECK_LEFT_EDGE_ERROR:
	case T_FWD_CHECK_RIGHT_EDGE_ERROR:
	case T_BWD_CHECK_TOP_EDGE_ERROR:
	case T_BWD_CHECK_BOTTOM_EDGE_ERROR:
	case T_BWD_CHECK_LEFT_EDGE_ERROR:
	case T_BWD_CHECK_RIGHT_EDGE_ERROR:
	case T_CHECK_OUT_CONFIG_ALIGN_XS_POS_ERROR:
	case T_CHECK_OUT_CONFIG_ALIGN_XE_POS_ERROR:
	case T_CHECK_OUT_CONFIG_ALIGN_YS_POS_ERROR:
	case T_CHECK_OUT_CONFIG_ALIGN_YE_POS_ERROR:
	case M_BACKWARD_START_LESS_THAN_FORWARD:
	case M_RESIZER_SCALING_ERROR:
		return -ERANGE;
	default:
		return -EINVAL;
	}
}

static const struct mdp_alg_path_node *get_tile_node(const struct mdp_alg_path_tp *p,
						     u32 eng_idx)
{
	return &p->nodes[p->tile_engines[eng_idx]];
}

static void set_tile_in_region(struct tile_region *region,
			       struct tile_func_block *func)
{
	region->xs = func->in_pos_xs;
	region->xe = func->in_pos_xe;
	region->ys = func->in_pos_ys;
	region->ye = func->in_pos_ye;
}

static void set_tile_out_region(struct tile_region *region,
				struct tile_func_block *func)
{
	region->xs = func->out_pos_xs;
	region->xe = func->out_pos_xe;
	region->ys = func->out_pos_ys;
	region->ye = func->out_pos_ye;
}

static void set_tile_luma(struct tile_offset *offset,
			  struct tile_func_block *func)
{
	offset->x = func->bias_x;
	offset->x_sub = func->offset_x;
	offset->y = func->bias_y;
	offset->y_sub = func->offset_y;
}

static void set_tile_chroma(struct tile_offset *offset,
			    struct tile_func_block *func)
{
	offset->x = func->bias_x_c;
	offset->x_sub = func->offset_x_c;
	offset->y = func->bias_y_c;
	offset->y_sub = func->offset_y_c;
}

static void set_tile_engine(const struct mdp_alg_path_node *engine,
			    struct mdp_alg_tile_engine *tile_eng,
			    struct tile_func_block *func)
{
	tile_eng->comp_id = engine->id;
	set_tile_in_region(&tile_eng->in, func);
	set_tile_out_region(&tile_eng->out, func);
	set_tile_luma(&tile_eng->luma, func);
	set_tile_chroma(&tile_eng->chroma, func);
}

static void set_tile_config(struct mdp_alg_task *task,
			    u32 pipe,
			    const struct mdp_alg_path_tp *path,
			    struct mdp_alg_tile_config *tile,
			    u32 tile_idx,
			    struct func_description *tile_func)
{
	int i;

	for (i = 0; i < path->tile_engine_cnt; i++) {
		const struct mdp_alg_path_node *e = get_tile_node(path, i);
		struct tile_func_block *func = tile_func->func_list[i];

		set_tile_engine(e, &tile->tile_engines[i], func);
	}

	tile->tile_no = tile_idx;
	tile->engine_cnt = path->tile_engine_cnt;
}

static int tile_prepare_comp(struct mdp_alg_task *task, u32 pipe,
			     struct func_description *tile_func,
			     union mdl_alg_tile_data *tile_datas)
{
	struct device *dev = &task->mdp->pdev->dev;
	struct mdp_alg_path_tp *path = &task->cfg.path[pipe];
	int i, ret;

	for (i = 0; i < path->tile_engine_cnt; i++) {
		struct mdp_comp *comp = get_tile_node(path, i)->comp;
		struct tile_func_block *func = tile_func->func_list[i];

		if (unlikely(comp->public_id != func->func_num)) {
			dev_err(dev, "[tile]mismatched tile_func(%d) and comp(%d) at [%d]",
				func->func_num, comp->public_id, i);
			return -EINVAL;
		}

		ret = call_alg_op(comp, tile_prepare, task,
				  path, path->tile_engines[i],
				  func, &tile_datas[i]);
		if (ret) {
			dev_err(dev, "[tile]comp(%d) prepare fail %d", comp->public_id, ret);
			return ret;
		}
	}
	return 0;
}

static void tile_destroy_working(struct tile_ctx *ctx)
{
	/* free working but keep output */
	kfree(ctx->tile_datas);
	kfree(ctx->tile_reg_map);
	kfree(ctx->tile_func);
}

static s32 tile_create_ctx(struct tile_ctx *ctx, u32 eng_cnt, size_t tile_max,
			   struct mdp_alg_tile_cache *tile_cache)
{
	int i;

	if (!tile_cache->ready) {
		for (i = 0; i < ARRAY_SIZE(tile_cache->func_list); i++) {
			if (tile_cache->func_list[i])
				continue;
			tile_cache->func_list[i] = kmalloc(sizeof(struct tile_func_block),
							   GFP_KERNEL);
			if (!tile_cache->func_list[i])
				return -ENOMEM;
		}

		if (!tile_cache->tiles) {
			tile_cache->tiles = vmalloc(MDP_ALG_MAX_TILE_NUM *
						    sizeof(*ctx->output->tiles));
			if (!tile_cache->tiles)
				return -ENOMEM;
		}

		tile_cache->ready = true;
	}

	ctx->output = kzalloc(sizeof(*ctx->output), GFP_KERNEL);
	if (!ctx->output)
		return -ENOMEM;
	ctx->tile_datas = kcalloc(eng_cnt, sizeof(*ctx->tile_datas), GFP_KERNEL);
	if (!ctx->tile_datas)
		return -ENOMEM;
	ctx->tile_reg_map = kzalloc(sizeof(*ctx->tile_reg_map), GFP_KERNEL);
	if (!ctx->tile_reg_map)
		return -ENOMEM;
	ctx->tile_func = kzalloc(sizeof(*ctx->tile_func), GFP_KERNEL);
	if (!ctx->tile_func)
		return -ENOMEM;
	for (i = 0; i < ARRAY_SIZE(tile_cache->func_list); i++)
		ctx->tile_func->func_list[i] = tile_cache->func_list[i];
	ctx->output->tiles = tile_cache->tiles;
	memset(ctx->output->tiles, 0, tile_max * sizeof(*ctx->output->tiles));

	return 0;
}

static int tile_calc_frame(struct mdp_alg_task *task, u32 pipe, struct tile_ctx *ctx)
{
	struct mdp_alg_path_tp *path = &task->cfg.path[pipe];
	struct tile_reg_map *tile_reg_map = ctx->tile_reg_map;
	struct func_description *tile_func = ctx->tile_func;
	enum mdp_tile_msg result;
	bool stop = false;
	int ret;

	/* frame calculate */
	result = tile_convert_func(tile_reg_map, tile_func, path);
	if (result != T_OK)
		goto err_tile;

	/* comp prepare initTileCalc to get each engine's in/out size */
	ret = tile_prepare_comp(task, pipe, tile_func, ctx->tile_datas);
	if (ret)
		goto err_exit;

	result = tile_init_config(tile_reg_map, tile_func);
	if (result != T_OK)
		goto err_tile;

	result = tile_frame_mode_init(tile_reg_map, tile_func);
	if (result != T_OK)
		goto err_tile;

	result = tile_proc_main_single(tile_reg_map, tile_func, 0, &stop);
	if (result != T_OK)
		goto err_tile;

	result = tile_frame_mode_close(tile_reg_map, tile_func);
	if (result != T_OK)
		goto err_tile;

	return 0;

err_tile:
	dev_err(&task->mdp->pdev->dev, "fail message %d", result);
	ret = tile_message_to_errno(result);
err_exit:
	return ret;
}

static int tile_calc_loop(struct mdp_alg_task *task, u32 pipe, struct tile_ctx *ctx)
{
	struct mdp_alg_path_tp *path = &task->cfg.path[pipe];
	struct tile_reg_map *tile_reg_map = ctx->tile_reg_map;
	struct func_description *tile_func = ctx->tile_func;
	struct mdp_alg_tile_config *tiles = ctx->output->tiles;
	u32 tile_cnt = 0;
	enum mdp_tile_msg result;
	bool stop;
	int ret;

	/* tile calculate */
	result = tile_mode_init(tile_reg_map, tile_func);
	if (result != T_OK)
		goto err_tile;

	/* initialize stop for tile calculate */
	stop = false;
	while (!stop) {
		tiles[tile_cnt].h_tile_no = tile_reg_map->curr_horizontal_tile_no;
		tiles[tile_cnt].v_tile_no = tile_reg_map->curr_vertical_tile_no;

		result = tile_proc_main_single(tile_reg_map, tile_func,
					       tile_cnt, &stop);
		if (result != T_OK)
			goto err_tile;

		/* tile result from param */
		set_tile_config(task, pipe, path, &tiles[tile_cnt], tile_cnt, tile_func);
		tile_cnt++;
	}

	ctx->output->tile_cnt = tile_cnt;
	ctx->output->h_tile_cnt = tiles[tile_cnt - 1].h_tile_no + 1;
	ctx->output->v_tile_cnt = tiles[tile_cnt - 1].v_tile_no + 1;
	return 0;

err_tile:
	dev_err(&task->mdp->pdev->dev, "fail message %d", result);
	ret = tile_message_to_errno(result);
	return ret;
}

int mdp_alg_tile_calc(struct mdp_alg_task *task, u32 pipe)
{
	struct device *dev = &task->mdp->pdev->dev;
	struct mdp_alg_tile_cache *t_cache = task->t_cache[pipe];
	struct mdp_alg_frame_config *f_cfg = &task->cfg;
	struct mdp_alg_path_tp *path = &f_cfg->path[pipe];
	struct tile_ctx ctx;
	struct mdp_alg_tile_config *tile;
	u32 eng_cnt = path->tile_engine_cnt;
	u32 tile_cnt;
	int ret, i;

	ret = tile_create_ctx(&ctx, eng_cnt, MDP_ALG_MAX_TILE_NUM, t_cache);
	if (ret) {
		dev_err(dev, "no memory to create tile context");
		goto free_output;
	}

	ret = tile_calc_frame(task, pipe, &ctx);
	if (ret)
		goto free_output;

	ret = tile_calc_loop(task, pipe, &ctx);
	if (ret)
		goto free_output;

	tile = ctx.output->tiles;
	tile_cnt = ctx.output->tile_cnt;
	for (i = 0; i < path->tile_engine_cnt; i++) {
		if (!f_cfg->hist_div[i]) {
			if (pipe == 0) {
				f_cfg->hist_div[i] =
					tile[tile_cnt - 1].tile_engines[i].in.xe + 1;
			} else {
				f_cfg->hist_div[i] =
					tile[0].tile_engines[i].in.xs;
			}
		}
	}

	/* put tile output to task */
	f_cfg->tile[pipe] = ctx.output;
	goto free_working;

free_output:
	kfree(ctx.output);
free_working:
	tile_destroy_working(&ctx);
	return ret;
}
