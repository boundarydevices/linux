// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include "mdp_tile_core.h"

/* Look-up table function */
static enum mdp_tile_msg tile_schedule_backward(struct func_description *func_param);
static enum mdp_tile_msg tile_bwd_comp(struct tile_reg_map *reg_map,
				       struct func_description *func_param);
static enum mdp_tile_msg tile_bwd_comp_min(struct tile_reg_map *reg_map,
					   struct func_description *func_param);
static enum mdp_tile_msg tile_bwd_comp_min_tile(struct tile_reg_map *reg_map,
						struct func_description *func_param);
static enum mdp_tile_msg tile_schedule_forward(struct func_description *func_param);
static enum mdp_tile_msg tile_fwd_comp(struct tile_reg_map *reg_map,
				       struct func_description *func_param);
static enum mdp_tile_msg tile_fwd_by_func(struct tile_func_block *blk,
					  struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_fwd_by_func_pre_x(struct tile_func_block *blk,
						struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_fwd_by_func_pre_x_inv(struct tile_func_block *blk,
						    struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_fwd_by_func_pre_y(struct tile_func_block *blk,
						struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_fwd_by_func_post_x(struct tile_func_block *blk,
						 struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_fwd_by_func_post_x_inv(struct tile_func_block *blk,
						     struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_fwd_by_func_post_y(struct tile_func_block *blk,
						 struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_fwd_comp_no_back(struct tile_reg_map *reg_map,
					       struct func_description *func_param);
static enum mdp_tile_msg tile_fwd_by_func_no_back(struct tile_func_block *blk,
						  struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_fwd_by_func_no_back_pre_x(struct tile_func_block *blk,
							struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_fwd_by_func_no_back_pre_y(struct tile_func_block *blk,
							struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_fwd_by_func_no_back_post_x(struct tile_func_block *blk,
							 struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_fwd_by_func_no_back_post_y(struct tile_func_block *blk,
							 struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_bwd_by_func_pre_x(struct tile_func_block *blk,
						struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_bwd_by_func_pre_y(struct tile_func_block *blk,
						struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_bwd_by_func(struct tile_func_block *blk,
					  struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_bwd_by_func_post_x(struct tile_func_block *blk,
						 struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_bwd_by_func_post_x_inv(struct tile_func_block *blk,
						     struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_bwd_by_func_post_y(struct tile_func_block *blk,
						 struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_bwd_output_config(struct tile_func_block *blk,
						struct tile_reg_map *reg_map,
						struct func_description *func_param);
static enum mdp_tile_msg tile_bwd_output_config_skip(struct tile_reg_map *reg_map,
						     struct func_description *func_param);
static enum mdp_tile_msg tile_bwd_output_config_x(struct tile_func_block *blk,
						  struct tile_reg_map *reg_map,
						  struct func_description *func_param);
static enum mdp_tile_msg tile_bwd_output_config_x_inv(struct tile_func_block *blk,
						      struct tile_reg_map *reg_map,
						      struct func_description *func_param);
static enum mdp_tile_msg tile_bwd_output_config_y(struct tile_func_block *blk,
						  struct tile_reg_map *reg_map,
						  struct func_description *func_param);
static enum mdp_tile_msg tile_bwd_output_config_min_tile(struct tile_func_block *blk,
							 struct tile_reg_map *reg_map,
							 struct func_description *func_param);
static enum mdp_tile_msg tile_bwd_output_config_x_min_tile(struct tile_func_block *blk,
							   struct tile_reg_map *reg_map,
							   struct func_description *func_param);
static enum mdp_tile_msg tile_bwd_output_config_x_inv_min_tile(struct tile_func_block *blk,
							       struct tile_reg_map *reg_map,
							       struct func_description *func_param);
static enum mdp_tile_msg tile_bwd_output_config_y_min_tile(struct tile_func_block *blk,
							   struct tile_reg_map *reg_map,
							   struct func_description *func_param);
static enum mdp_tile_msg tile_fwd_input_config(struct tile_func_block *blk,
					       struct tile_reg_map *reg_map,
					       struct func_description *func_param);
static enum mdp_tile_msg tile_bwd_input_check(struct tile_func_block *blk,
					      struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_fwd_output_check(struct tile_func_block *blk,
					       struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_fwd_recusive_check(struct tile_func_block *blk,
						 struct tile_reg_map *reg_map,
						 struct func_description *func_param,
						 bool *restart);
static enum mdp_tile_msg tile_check_input_config(struct tile_func_block *blk,
						 struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_check_output_config(struct tile_func_block *blk,
						  struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_check_output_config_x(struct tile_func_block *blk,
						    struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_check_output_config_x_inv(struct tile_func_block *blk,
							struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_check_output_config_y(struct tile_func_block *blk,
						    struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_update_last_x_y(struct tile_reg_map *reg_map,
					      struct func_description *func_param,
					      bool x_end_flag,
					      bool y_end_flag);
static enum mdp_tile_msg tile_compare_forward_back(struct tile_reg_map *reg_map,
						   struct func_description *func_param);
static enum mdp_tile_msg tile_init_by_prev(struct tile_func_block *blk,
					   struct func_description *func_param);

/* Diff view */
static enum mdp_tile_msg tile_check_min_tile(struct tile_reg_map *reg_map,
					     struct func_description *func_param);
static enum mdp_tile_msg tile_check_valid_output(struct tile_reg_map *reg_map,
						 struct func_description *func_param);
static enum mdp_tile_msg tile_init_tdr_ctrl_flag(struct tile_reg_map *reg_map,
						 struct func_description *func_param);
static enum mdp_tile_msg tile_bwd_min_tile_backup_input(struct tile_func_block *blk,
							struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_bwd_min_tile_init(struct tile_func_block *blk,
						struct tile_reg_map *reg_map);
static enum mdp_tile_msg tile_bwd_min_tile_restore(struct tile_func_block *blk,
						   struct tile_reg_map *reg_map);
/* Check end position with end flag after computation of all tiles are done */
static enum mdp_tile_msg tile_check_x_end_pos_with_flag(struct tile_reg_map *reg_map,
							struct func_description *func_param,
							bool *x_end, int curr_tile_no);
static enum mdp_tile_msg tile_check_y_end_pos_with_flag(struct tile_reg_map *reg_map,
							struct func_description *func_param,
							bool *y_end);

static inline int tile_cal_lcm(int a, int b)
{
	int m = a, n = b;

	if (a == 1)
		return b;
	if (b == 1)
		return a;
	if (a == b)
		return a;

	while (m != n) {
		if (m > n)
			m -= n;
		else
			n -= m;
	}

	if (m == 1)
		return a * b;

	return TILE_INT_DIV(a * b, m);
}

static inline void bwd_output_x_update_range(struct tile_func_block *n,
					     bool run_cal,
					     int *out_xs, int *out_xe,
					     int *min_xs, int *min_xe,
					     bool *last_full_range_flag)
{
	if (run_cal) {
		if (n->min_tile_in_pos_xs == n->in_pos_xs &&
		    n->min_tile_in_pos_xe == n->in_pos_xe &&
			n->in_pos_xe < n->max_in_pos_xe) {
			if (n->in_pos_xs < *min_xs) {
				*min_xs = n->in_pos_xs;
				*min_xe = n->in_pos_xe;
			} else if (n->in_pos_xs == *min_xs) {
				if (n->in_pos_xe > *min_xe)
					*min_xe = n->in_pos_xe;
			}
		} else {
			/* Select min pos */
			if (*out_xs > n->in_pos_xs)
				*out_xs = n->in_pos_xs;

			if (*last_full_range_flag) {
				/* Full range input */
				if (n->in_pos_xe >= n->max_in_pos_xe) {
					/* Select max xe pos */
					if (*out_xe < n->in_pos_xe)
						*out_xe = n->in_pos_xe;
				} else {
					/* Non-full range input */
					*out_xe = n->in_pos_xe;
					*last_full_range_flag = false;
				}
			} else {
				/* Non-full range input */
				if (n->in_pos_xe < n->max_in_pos_xe) {
					/* Select min xe pos */
					if (*out_xe > n->in_pos_xe)
						*out_xe = n->in_pos_xe;
				}
			}
		}
	} else {
		/* Select min pos */
		if (*out_xs > n->in_pos_xs)
			*out_xs = n->in_pos_xs;

		if (*last_full_range_flag) {
			/* Full range input */
			if (n->in_pos_xe >= n->max_in_pos_xe) {
				if (*out_xe < n->in_pos_xe)
					*out_xe = n->in_pos_xe;
			} else {
				/* Non-full range input */
				*out_xe = n->in_pos_xe;
				*last_full_range_flag = false;
			}
		} else if (n->in_pos_xe < n->max_in_pos_xe &&
			   *out_xe > n->in_pos_xe) {
			/* Non-full range input */
			*out_xe = n->in_pos_xe;
		}
	}
}

static inline void bwd_output_x_clamp(int *out_xs, int *out_xe,
				      int *min_xs, int *min_xe,
				      bool h_end_flag)
{
	if (*out_xe < *out_xs) {
		*out_xs = *min_xs;
		*out_xe = *min_xe;
	} else if (*min_xe >= *min_xs) {
		if (*out_xs <= *min_xs) {
			if (*min_xe <= *out_xe) {
				if (!h_end_flag)
					*out_xe = *min_xe;
			} else {
				if (h_end_flag) {
					*out_xe = *min_xe;
				} else {
					if (*min_xe + *out_xs < *out_xe + *min_xs)
						*out_xe = *min_xe;
					else if (2 * *out_xe + 1 > *min_xe + *min_xs)
						*out_xe = *min_xe;
				}
			}
		} else {
			if (*min_xe <= *out_xe) {
				if (!h_end_flag)
					if (*min_xe + *out_xs < *out_xe + *min_xs)
						*out_xe = *min_xe;
			} else {
				*out_xe = *min_xe;
			}
			*out_xs = *min_xs;
		}
	}
}

static inline enum mdp_tile_msg tile_init_func_run(struct tile_func_block *b,
						   struct tile_reg_map *m)
{
	return b->init_func ? b->init_func(b, m) : T_OK;
}

static inline enum mdp_tile_msg tile_for_func_run(struct tile_func_block *b,
						  struct tile_reg_map *m)
{
	return b->for_func ? b->for_func(b, m) : T_OK;
}

static inline enum mdp_tile_msg tile_back_func_run(struct tile_func_block *b,
						   struct tile_reg_map *m)
{
	return b->back_func ? b->back_func(b, m) : T_OK;
}

enum mdp_tile_msg tile_convert_func(struct tile_reg_map *reg_map,
				    struct func_description *func_param,
				    struct mdp_alg_path_tp *path)
{
	struct tile_func_block *f;
	struct tile_func_block *tgt;
	struct mdp_alg_path_node *node;
	int m_no, start_count;
	int i, j, k;

	/* Reset func_param */
	func_param->for_recursive_count = 0;

	m_no = path->tile_engine_cnt;
	for (i = 0; i < m_no; i++) {
		node = &path->nodes[path->tile_engines[i]];
		f = func_param->func_list[i];
		memset(f, 0x0, sizeof(*f));

		f->func_num = node->id;
		f->run_mode = TILE_RUN_MODE_MAIN;
		f->enable_flag = true;

		f->tot_prev_num = 1;
		if (node->prev[0])
			f->last_func_num[0] = node->prev[0]->id;
		else
			f->last_func_num[0] = LAST_MODULE_ID_OF_START;

		f->in_const_x = 1;
		f->in_const_y = 1;
		f->out_const_x = 1;
		f->out_const_y = 1;
	}
	func_param->used_func_no = m_no;

	start_count = 0;
	/* Check valid module no */
	if (m_no < MIN_TILE_FUNC_NO)
		return T_UNDER_MIN_TILE_FUNC_NO_ERROR;

	/* Connect modules with error check */
	for (i = 0; i < m_no; i++) {
		f = func_param->func_list[i];

		for (j = 0; j < f->tot_prev_num; j++) {
			int num = f->last_func_num[j];

			/* Skip start module */
			if (num == LAST_MODULE_ID_OF_START) {
				f->prev_blk_num[j] = PREVIOUS_BLK_NO_OF_START;
				start_count++;

				/* Valid input no */
				if (start_count > MAX_INPUT_TILE_FUNC_NO)
					return T_OVER_MAX_INPUT_TILE_FUNC_NO_ERROR;

				if (j)
					return T_TWO_START_PREV_ERROR;
			} else {
				bool found_flag = false;

				/* Check duplicated last func */
				for (k = j + 1; k < f->tot_prev_num; k++)
					if (num == f->last_func_num[k])
						return T_DUPLICATED_SUPPORT_FUNC_ERROR;

				for (k = 0; k < m_no; k++) {
					unsigned char *n;

					/* Skip self */
					if (i == k)
						continue;

					tgt = func_param->func_list[k];
					if (num != tgt->func_num)
						continue;

					/* Find last */
					found_flag = true;
					f->prev_blk_num[j] = k;

					/* Valid branch no */
					n = &tgt->tot_branch_num;
					if (tgt->tot_branch_num < MAX_TILE_BRANCH_NO) {
						tgt->next_blk_num[*n] = i;
						tgt->next_func_num[*n] = f->func_num;
						(*n)++;
					} else {
						/* Over max buffer size */
						(*n)++;
						return T_OVER_MAX_BRANCH_NO_ERROR;
					}
					break;
				}

				if (!found_flag)
					return T_TILE_FUNC_CANNOT_FIND_LAST_FUNC_ERROR;
			}
		}
	}

	return T_OK;
}

enum mdp_tile_msg tile_proc_main_single(struct tile_reg_map *reg_map,
					struct func_description *func_param,
					int tile_no, bool *stop_flag)
{
	enum mdp_tile_msg result;
	int max_loop_count = MAX_TILE_TOT_NO;
	int idx = reg_map->first_func_en_no;
	bool x_end_flag = func_param->func_list[idx]->h_end_flag;
	bool y_end_flag = func_param->func_list[idx]->v_end_flag;

	/* Update loop count */
	if (tile_no >= max_loop_count)
		return T_OVER_MAX_TILE_TOT_NO_ERROR;

	/* To run single tile */
	result = tile_bwd_output_config_skip(reg_map, func_param);
	if (result)
		goto err_return;
	result = tile_init_tdr_ctrl_flag(reg_map, func_param);
	if (result)
		goto err_return;
	result = tile_bwd_comp_min(reg_map, func_param);
	if (result)
		goto err_return;
	/* Min tile check backward */
	result = tile_check_min_tile(reg_map, func_param);
	if (result)
		goto err_return;
	result = tile_bwd_comp(reg_map, func_param);
	if (result)
		goto err_return;
	result = tile_fwd_comp(reg_map, func_param);
	if (result)
		goto err_return;
	result = tile_check_valid_output(reg_map, func_param);
	if (result)
		goto err_return;
	/* Min tile check forward */
	result = tile_check_min_tile(reg_map, func_param);
	if (result)
		goto err_return;

	/* Multi-input cal, run sub in found */
	if (reg_map->found_sub_in) {
		/* Config sub in mode */
		reg_map->run_mode = TILE_RUN_MODE_SUB_IN;
		result = tile_bwd_comp(reg_map, func_param);
		if (result)
			goto err_return;
		result = tile_fwd_comp(reg_map, func_param);
		if (result)
			goto err_return;
		result = tile_compare_forward_back(reg_map, func_param);
		if (result)
			goto err_return;

		/* Restore mode */
		reg_map->run_mode = TILE_RUN_MODE_MAIN;
	}

	/* Multi-input cal, run sub out found */
	if (reg_map->found_sub_out) {
		/* Config sub in mode */
		reg_map->run_mode = TILE_RUN_MODE_SUB_OUT;
		result = tile_fwd_comp_no_back(reg_map, func_param);
		if (result)
			goto err_return;
		/* Restore mode */
		reg_map->run_mode = TILE_RUN_MODE_MAIN;
	}

	/* Check tile end & update tile no */
	result = tile_check_x_end_pos_with_flag(reg_map, func_param,
						&x_end_flag, tile_no);
	if (result)
		goto err_return;

	result = tile_check_y_end_pos_with_flag(reg_map, func_param,
						&y_end_flag);
	if (result)
		goto err_return;

	/* To backup func property before curr_horizontal_tile_no increase */
	result = tile_update_last_x_y(reg_map, func_param, x_end_flag,
				      y_end_flag);
	if (result)
		goto err_return;
	reg_map->used_tile_no++;

	/* End loop found */
	if (y_end_flag && x_end_flag) {
		*stop_flag = true;
		/* Set default valid_tile_no */
		reg_map->valid_tile_no = reg_map->used_tile_no;
	} else {
		if (reg_map->first_frame)
			return T_FRAME_MODE_NOT_END_ERROR;

		if (tile_no + 1 >= max_loop_count)
			return T_OVER_MAX_TILE_TOT_NO_ERROR;
	}

err_return:
	return result;
}

static enum mdp_tile_msg tile_init_by_prev(struct tile_func_block *blk,
					   struct func_description *func_param)
{
	struct tile_func_block *f;
	bool found_prev = false;
	int i;

	for (i = 0; i < blk->tot_prev_num; i++) {
		f = func_param->func_list[blk->prev_blk_num[i]];

		/* Update only for main path */
		if (f->output_disable_flag)
			continue;

		if (found_prev) {
			if (blk->full_size_x_in != f->full_size_x_out ||
			    blk->full_size_y_in != f->full_size_y_out ||
			    blk->in_stream_order != f->out_stream_order ||
			    blk->in_cal_order != f->out_cal_order ||
			    blk->in_dump_order != f->out_dump_order)
				return T_DIFF_PREV_CONFIG_ERROR;
		} else {
			/* Skip call init function & force init size for disable func */
			blk->in_pos_xs = 0;
			blk->in_pos_ys = 0;
			blk->in_pos_xe = f->full_size_x_out - 1;
			blk->in_pos_ye = f->full_size_y_out - 1;
			blk->full_size_x_in = f->full_size_x_out;
			blk->full_size_y_in = f->full_size_y_out;
			/* Check cal order, must init for disabled function too */
			TILE_COPY_PRE_ORDER(blk, f);
			found_prev = true;
		}
	}

	return T_OK;
}

enum mdp_tile_msg tile_init_config(struct tile_reg_map *reg_map,
				   struct func_description *func_param)
{
	enum mdp_tile_msg result;
	struct tile_func_block *f, *nxt, *prv;
	int m_no = func_param->used_func_no;
	bool input_enable_flag = false;
	bool output_enable_flag = false;
	bool found_input_count = false;
	bool found_output_en = false;
	int out_const_x, out_const_y;
	int out_tile_width, out_tile_height;
	int out_max_width, out_max_height;
	int in_const_x, in_const_y;
	int in_tile_width, in_max_width;
	int in_tile_height, in_max_height;
	int id;
	int i, j;

	/* Update scheduling backward order */
	result = tile_schedule_backward(func_param);
	if (result)
		return result;

	/* Update scheduling forward order */
	result = tile_schedule_forward(func_param);
	if (result)
		return result;

	/* Check enable & output disable by backward */
	for (i = 0; i < m_no; i++) {
		id = func_param->scheduling_backward_order[i];
		f = func_param->func_list[id];

		if (f->output_disable_flag)
			continue;

		/* Trace output disable */
		if (!f->tot_branch_num) {
			/* End func */
			if (f->type & TILE_TYPE_WDMA) {
				if (!f->enable_flag) {
					f->output_disable_flag = true;
				} else {
					/* Init crop param */
					f->crop_bias_x = 0;
					f->crop_offset_x = 0;
					f->crop_bias_y = 0;
					f->crop_offset_y = 0;
				}
			} else if (f->prev_blk_num[0] != PREVIOUS_BLK_NO_OF_START) {
				f->output_disable_flag = true;
			}
		} else {
			/* Not end func */
			for (j = 0; j < f->tot_branch_num; j++) {
				nxt = func_param->func_list[f->next_blk_num[j]];

				/* Find out branch enabled output */
				if (!nxt->output_disable_flag) {
					f->output_disable_flag = false;
					break;
				}

				f->output_disable_flag = true;
			}

			if (f->output_disable_flag)
				continue;
			if (f->prev_blk_num[0] == PREVIOUS_BLK_NO_OF_START) {
				if (!input_enable_flag && f->enable_flag)
					input_enable_flag = true;
			} else if (!f->tot_prev_num) {
				return T_TILE_FUNC_CANNOT_FIND_LAST_FUNC_ERROR;
			}
		}
	}

	if (!input_enable_flag)
		return T_OUTPUT_DISABLE_INPUT_FUNC_CHECK_ERROR;

	/* Check RDMA disabled again to disable path by forward */
	for (i = 0; i < m_no; i++) {
		id = func_param->scheduling_forward_order[i];
		f = func_param->func_list[id];

		if (f->output_disable_flag)
			continue;

		/* Start func disabled */
		if (f->prev_blk_num[0] == PREVIOUS_BLK_NO_OF_START) {
			if (!f->enable_flag)
				f->output_disable_flag = true;
		} else {
			bool input_enable_count = false;

			for (j = 0; j < f->tot_prev_num; j++) {
				nxt = func_param->func_list[f->prev_blk_num[j]];
				/* Find input enabled */
				if (!nxt->output_disable_flag) {
					input_enable_count = true;
					break;
				}
			}
			if (!input_enable_count)
				f->output_disable_flag = true;
		}

		if (output_enable_flag)
			continue;

		/* End func */
		if (!f->tot_branch_num)
			if (!f->output_disable_flag && f->enable_flag)
				output_enable_flag = true;
	}

	if (!output_enable_flag)
		return T_OUTPUT_DISABLE_INPUT_FUNC_CHECK_ERROR;

	/* Init full size in & out at same time */
	for (i = 0; i < m_no; i++) {
		id = func_param->scheduling_forward_order[i];
		f = func_param->func_list[id];

		if (f->output_disable_flag && f->enable_flag) {
			if (f->prev_blk_num[0] == PREVIOUS_BLK_NO_OF_START) {
				if (!(f->type & TILE_TYPE_RDMA))
					return T_INCOR_START_FUNC_TYPE_ERROR;
			} else if (f->tot_branch_num == 0) {
				if (!(f->type & TILE_TYPE_WDMA))
					return T_INCOR_END_FUNC_TYPE_ERROR;
			}
			continue;
		}

		/* Cal input & output size */
		f->valid_v_no = 0;
		f->valid_h_no = 0;
		f->last_valid_tile_no = 0;
		f->last_valid_v_no = 0;
		f->tdr_h_disable_flag = false;
		f->tdr_v_disable_flag = false;

		/* Set size & init disable func */
		if (!f->enable_flag) {
			/* Clear tile size */
			f->in_tile_width = 0;
			f->in_max_width = 0;
			f->in_tile_height = 0;
			f->in_max_height = 0;
			f->out_tile_width = 0;
			f->out_max_width = 0;
			f->out_tile_height = 0;
			f->out_max_height = 0;
			f->in_const_x = 1;
			f->in_const_y = 1;
			f->out_const_x = 1;
			f->out_const_y = 1;
			/* Mask TILE_TYPE_CROP_EN type */
			f->type &= ~TILE_TYPE_CROP_EN;

			result = tile_init_by_prev(f, func_param);
			if (result)
				return result;
		} else {
			/* Set size for enabled func */
			if (f->prev_blk_num[0] == PREVIOUS_BLK_NO_OF_START) {/* Start func */
				/* Check call order, copy src in to first func, before init */
				TILE_COPY_SRC_ORDER(f, reg_map);

				/* Run init func ptr for start func */
				result = tile_init_func_run(f, reg_map);
				if (result)
					return result;

				/* Set input x y size & pos of start func */
				f->in_pos_xs = 0;
				f->in_pos_ys = 0;
				f->in_pos_xe = f->full_size_x_in - 1;
				f->in_pos_ye = f->full_size_y_in - 1;

				if (!(f->type & TILE_TYPE_RDMA))
					return T_INCOR_START_FUNC_TYPE_ERROR;

				/* Check call order, hw constraint */
				if (f->in_stream_order & TILE_ORDER_RIGHT_TO_LEFT)
					return T_INCOR_ORDER_CONFIG_ERROR;
			} else {/* Intermediate or end func */
				result = tile_init_by_prev(f, func_param);
				if (result)
					return result;

				/* Run init func ptr for intermediate or end func */
				result = tile_init_func_run(f, reg_map);
				if (result)
					return result;

				if (!f->tot_branch_num && !(f->type & TILE_TYPE_WDMA))
					return T_INCOR_END_FUNC_TYPE_ERROR;
			}

			/* Check call order, x flip check, xor, in & out */
			if ((f->in_stream_order & TILE_ORDER_BOTTOM_TO_TOP) !=
			    (f->out_stream_order & TILE_ORDER_BOTTOM_TO_TOP))
				return T_INCOR_ORDER_CONFIG_ERROR;

			/* Check call order, x flip check, xor, in & out */
			if ((f->in_stream_order & TILE_ORDER_RIGHT_TO_LEFT) ||
			    (f->out_stream_order & TILE_ORDER_RIGHT_TO_LEFT))
				return T_INCOR_ORDER_CONFIG_ERROR;

			/* Check call order, in & out check, commom */
			if ((f->in_cal_order & TILE_ORDER_Y_FIRST) ||
			    (f->out_cal_order & TILE_ORDER_Y_FIRST))
				return T_INCOR_ORDER_CONFIG_ERROR;

			/* Check call order, hw constraint */
			if ((f->in_cal_order & TILE_ORDER_BOTTOM_TO_TOP) ||
			    (f->out_cal_order & TILE_ORDER_BOTTOM_TO_TOP))
				return T_INCOR_ORDER_CONFIG_ERROR;

			/* Check call order, x flip check, xor, in & out */
			if ((f->in_cal_order & TILE_ORDER_RIGHT_TO_LEFT) !=
			    (f->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT))
				return T_INCOR_ORDER_CONFIG_ERROR;

			/* Check call order, x flip check, xor, in & out */
			if ((f->in_dump_order & TILE_ORDER_RIGHT_TO_LEFT) !=
			    (f->out_dump_order & TILE_ORDER_RIGHT_TO_LEFT))
				return T_INCOR_ORDER_CONFIG_ERROR;

			/* Check call order, x flip check, xor, in & out */
			if ((f->in_dump_order & TILE_ORDER_BOTTOM_TO_TOP) !=
			    (f->out_dump_order & TILE_ORDER_BOTTOM_TO_TOP))
				return T_INCOR_ORDER_CONFIG_ERROR;

			/* Check call order, x flip check, xor, in & out */
			if ((f->in_dump_order & TILE_ORDER_Y_FIRST) !=
			    (f->out_dump_order & TILE_ORDER_Y_FIRST))
				return T_INCOR_ORDER_CONFIG_ERROR;
		}

		/* Backup for next error check */
		f->last_input_xe_pos = f->in_pos_xe;
		f->last_input_ye_pos = f->in_pos_ye;
		f->last_input_xs_pos = f->in_pos_xs;
		f->last_input_ys_pos = f->in_pos_ys;
		result = tile_check_input_config(f, reg_map);
		if (result)
			return result;

		/* Set output size */
		if (!f->init_func || !f->enable_flag) {
			/* Copy for null init func */
			f->out_pos_xs = f->in_pos_xs;
			f->out_pos_ys = f->in_pos_ys;
			f->out_pos_xe = f->in_pos_xe;
			f->out_pos_ye = f->in_pos_ye;
			f->full_size_x_out = f->full_size_x_in;
			f->full_size_y_out = f->full_size_y_in;
		} else {
			/* Check output size initialized by init func ptr */
			if (f->full_size_x_out > 0 && f->full_size_y_out > 0) {
				/* Init with desired output size & skip forward */
				f->out_pos_xs = 0;
				f->out_pos_ys = 0;
				f->out_pos_xe = f->full_size_x_out - 1;
				f->out_pos_ye = f->full_size_y_out - 1;
			} else if (f->full_size_y_out > 0 && f->full_size_x_out <= 0) {
				/* Error with incorrect x output size */
				result = T_INIT_INCOR_X_OUTPUT_SIZE_POS_ERROR;
			} else if (f->full_size_x_out <= 0 && f->full_size_y_out > 0) {
				/* Error with incorrect y output size */
				result = T_INIT_INCOR_Y_OUTPUT_SIZE_POS_ERROR;
			} else {
				/* Copy for non-initialized output size by init func */
				f->out_pos_xs = f->in_pos_xs;
				f->out_pos_ys = f->in_pos_ys;
				f->out_pos_xe = f->in_pos_xe;
				f->out_pos_ye = f->in_pos_ye;
				f->full_size_x_out = f->full_size_x_in;
				f->full_size_y_out = f->full_size_y_in;
			}
		}
		if (result)
			return result;

		/* Backup for next error check */
		f->last_output_xe_pos = f->out_pos_xe;
		f->last_output_ye_pos = f->out_pos_ye;
		f->last_output_xs_pos = f->out_pos_xs;
		f->last_output_ys_pos = f->out_pos_ys;
		f->min_out_pos_xs = 0;
		f->max_out_pos_xe = f->full_size_x_out - 1;
		f->min_out_pos_ys = 0;
		f->max_out_pos_ye = f->full_size_y_out - 1;
		/* Check output x y size & pos */
		result = tile_check_output_config(f, reg_map);
		if (result)
			return result;

		/* Check size of disable func */
		if (!f->enable_flag) {
			if (f->full_size_x_in != f->full_size_x_out)
				return T_DISABLE_FUNC_X_SIZE_CHECK_ERROR;
			if (f->full_size_y_in != f->full_size_y_out)
				return T_DISABLE_FUNC_Y_SIZE_CHECK_ERROR;
		}

		/* Check alignment once */
		if (f->out_const_x <= 0)
			return T_OUT_CONST_X_ERROR;

		if (f->out_const_y <= 0)
			return T_OUT_CONST_Y_ERROR;

		if (f->in_const_x <= 0)
			return T_IN_CONST_X_ERROR;

		if (f->in_const_y <= 0)
			return T_IN_CONST_Y_ERROR;
	}

	/* Find out run mode with main, sub in, sub out */
	for (i = 0; i < m_no; i++) {
		bool found_main_input = false;

		id = func_param->scheduling_forward_order[i];
		f = func_param->func_list[id];

		if (f->output_disable_flag)
			continue;

		if (f->run_mode != TILE_RUN_MODE_MAIN) {
			if (f->run_mode == TILE_RUN_MODE_SUB_IN) {
				if (f->prev_blk_num[0] == PREVIOUS_BLK_NO_OF_START)
					reg_map->found_sub_in = true;
				else
					return T_INVALID_SUB_IN_CONFIG_ERROR;
			} else if (f->run_mode == TILE_RUN_MODE_SUB_OUT) {
				if (f->tot_branch_num)
					return T_INVALID_SUB_OUT_CONFIG_ERROR;

				if (reg_map->tdr_ctrl_en) {
					f->run_mode = TILE_RUN_MODE_MAIN;
				} else {
					f->type |= TILE_TYPE_DONT_CARE_END;
					reg_map->found_sub_out = true;
				}
			} else {
				return T_UNKNOWN_RUN_MODE_ERROR;
			}
			continue;
		}

		/* Check run_mode */
		if (f->prev_blk_num[0] == PREVIOUS_BLK_NO_OF_START) {
			if (found_input_count)
				return T_TWO_MAIN_START_ERROR;

			reg_map->first_func_en_no = id;
			found_input_count = true;
			continue;
		}

		for (j = 0; j < f->tot_prev_num; j++) {
			prv = func_param->func_list[f->prev_blk_num[j]];

			if (prv->output_disable_flag)
				continue;
			if (prv->run_mode != TILE_RUN_MODE_MAIN)
				continue;
			/* More main input error */
			if (found_main_input)
				return T_TWO_MAIN_PREV_ERROR;

			if (f->enable_flag && !found_output_en) {
				/* Only happen with enable */
				if (!f->tot_branch_num) {
					found_output_en = true;
					reg_map->last_func_en_no = id;
				}
			}
			found_main_input = true;
		}
		/* Found sub in to update */
		if (!found_main_input) {
			if (f->tot_branch_num == 0)
				return T_MIX_SUB_IN_OUT_PATH_ERROR;

			f->run_mode = TILE_RUN_MODE_SUB_IN;
		}
	}

	if (!found_output_en)
		return T_NO_MAIN_OUTPUT_ERROR;

	/* Search sub out path */
	if (reg_map->found_sub_out) {
		for (i = 0; i < m_no; i++) {
			id = func_param->scheduling_backward_order[i];
			f = func_param->func_list[id];

			/* Check run_mode */
			if (f->output_disable_flag)
				continue;
			if (!f->tot_branch_num)
				continue;

			for (j = 0; j < f->tot_branch_num; j++) {
				nxt = func_param->func_list[f->next_blk_num[j]];
				if (nxt->run_mode != TILE_RUN_MODE_SUB_OUT &&
				    !nxt->output_disable_flag)
					break;
			}

			/* Multi-in support */
			if (f->tot_branch_num == j)
				f->run_mode = TILE_RUN_MODE_SUB_OUT;
		}
	}

	/* Lcm in & out alignment by forward */
	for (i = 0; i < m_no; i++) {
		id = func_param->scheduling_forward_order[i];
		f = func_param->func_list[id];

		/* Skip output disable func in following check */
		if (f->output_disable_flag)
			continue;
		if (f->tot_branch_num < 0)
			continue;

		/* Start & end functions full size alignment has been checked before */
		out_const_x = f->out_const_x;
		out_const_y = f->out_const_y;
		out_tile_width = f->out_tile_width;
		out_tile_height = f->out_tile_height;
		out_max_width = f->out_max_width;
		out_max_height = f->out_max_height;

		for (j = 0; j < f->tot_branch_num; j++) {
			nxt = func_param->func_list[f->next_blk_num[j]];

			/* Skip output disable func in following check */
			if (nxt->output_disable_flag)
				continue;

			if (nxt->in_const_x > 1)
				out_const_x = tile_cal_lcm(out_const_x, nxt->in_const_x);
			if (nxt->in_const_y > 1)
				out_const_y = tile_cal_lcm(out_const_y, nxt->in_const_y);

			if (out_tile_width) {
				if (nxt->in_tile_width) {
					if (out_tile_width > nxt->in_tile_width)
						out_tile_width = nxt->in_tile_width;
				}
			} else if (nxt->in_tile_width) {
				out_tile_width = nxt->in_tile_width;
			}

			if (out_max_width) {
				if (nxt->in_max_width) {
					if (out_max_width > nxt->in_max_width)
						out_max_width = nxt->in_max_width;
				}
			} else if (nxt->in_max_width) {
				out_max_width = nxt->in_max_width;
			}

			if (out_tile_height) {
				if (nxt->in_tile_height) {
					if (out_tile_height > nxt->in_tile_height)
						out_tile_height = nxt->in_tile_height;
				}
			} else if (nxt->in_tile_height) {
				out_tile_height = nxt->in_tile_height;
			}

			if (out_max_height) {
				if (nxt->in_max_height) {
					if (out_max_height > nxt->in_max_height)
						out_max_height = nxt->in_max_height;
				}
			} else if (nxt->in_max_height) {
				out_max_height = nxt->in_max_height;
			}
		}

		f->out_const_x = out_const_x;
		f->out_const_y = out_const_y;
		f->out_tile_width = out_tile_width;
		f->out_max_width = out_max_width;
		f->out_tile_height = out_tile_height;
		f->out_max_height = out_max_height;

		for (j = 0; j < f->tot_branch_num; j++) {
			nxt = func_param->func_list[f->next_blk_num[j]];
			if (nxt->output_disable_flag)
				continue;

			nxt->in_const_x = out_const_x;
			nxt->in_const_y = out_const_y;
			nxt->in_tile_width = out_tile_width;
			nxt->in_max_width = out_max_width;
			nxt->in_tile_height = out_tile_height;
			nxt->in_max_height = out_max_height;
		}
		/* Check in/out alignment of full size */
		if (out_const_y > 1)
			if (TILE_MOD(f->full_size_y_out, out_const_y))
				return T_YSIZE_NOT_DIV_BY_OUT_CONST_Y_ERROR;

		if (out_const_x > 1)
			if (TILE_MOD(f->full_size_x_out, out_const_x))
				return T_XSIZE_NOT_DIV_BY_OUT_CONST_X_ERROR;
	}

	if (!reg_map->found_sub_in)
		goto err_skip_sub_in;

	/* Lcm in & out alignment by backward */
	for (i = 0; i < m_no; i++) {
		id = func_param->scheduling_backward_order[i];
		f = func_param->func_list[id];

		/* Skip output disable func in following check */
		if (f->output_disable_flag)
			continue;
		/* Only check sub in merge function */
		if (f->tot_prev_num <= 1)
			continue;

		in_const_x = f->in_const_x;
		in_const_y = f->in_const_y;
		in_tile_width = f->in_tile_width;
		in_max_width = f->in_max_width;
		in_tile_height = f->in_tile_height;
		in_max_height = f->in_max_height;

		for (j = 0; j < f->tot_prev_num; j++) {
			prv = func_param->func_list[f->prev_blk_num[j]];
			/* Skip output disable func in following check */
			if (prv->output_disable_flag)
				continue;

			if (prv->out_const_x > 1)
				in_const_x = tile_cal_lcm(in_const_x, prv->out_const_x);
			if (prv->out_const_y > 1)
				in_const_y = tile_cal_lcm(in_const_y, prv->out_const_y);

			if (in_tile_width) {
				if (prv->out_tile_width)
					if (in_tile_width > prv->out_tile_width)
						in_tile_width = prv->out_tile_width;
			} else if (prv->out_tile_width) {
				in_tile_width = prv->out_tile_width;
			}

			if (in_max_width) {
				if (prv->out_max_width)
					if (in_max_width > prv->out_max_width)
						in_max_width = prv->out_max_width;
			} else if (prv->out_max_width) {
				in_max_width = prv->out_max_width;
			}

			if (in_tile_height) {
				if (prv->out_tile_height) {
					if (in_tile_height > prv->out_tile_height)
						in_tile_height = prv->out_tile_height;
				}
			} else if (prv->out_tile_height) {
				in_tile_height = prv->out_tile_height;
			}

			if (in_max_height) {
				if (prv->out_max_height)
					if (in_max_height > prv->out_max_height)
						in_max_height = prv->out_max_height;
			} else if (prv->out_max_height) {
				in_max_height = prv->out_max_height;
			}
		}

		f->in_const_x = in_const_x;
		f->in_const_y = in_const_y;
		f->in_tile_width = in_tile_width;
		f->in_max_width = in_max_width;
		f->in_tile_height = in_tile_height;
		f->in_max_height = in_max_height;

		for (j = 0; j < f->tot_prev_num; j++) {
			prv = func_param->func_list[f->prev_blk_num[j]];
			/* Skip output disable func in following update */
			if (prv->output_disable_flag)
				continue;

			prv->out_const_x = in_const_x;
			prv->out_const_y = in_const_y;
			prv->out_tile_width = in_tile_width;
			prv->out_max_width = in_max_width;
			prv->out_tile_height = in_tile_height;
			prv->out_max_height = in_max_height;
		}
		/* Check in/out alignment of full size */
		if (in_const_y > 1)
			if (TILE_MOD(f->full_size_y_in, in_const_y))
				return T_YSIZE_NOT_DIV_BY_IN_CONST_Y_ERROR;

		if (in_const_x > 1)
			if (TILE_MOD(f->full_size_x_in, in_const_x))
				return T_XSIZE_NOT_DIV_BY_IN_CONST_X_ERROR;
	}

err_skip_sub_in:
	/* Clear valid flag */
	memset(func_param->valid_flag, 0x0, 4 * (m_no + 31) >> 5);

	for (i = 0; i < m_no; i++) {
		unsigned int *valid;

		id = func_param->scheduling_backward_order[i];
		f = func_param->func_list[id];
		valid = &func_param->valid_flag[id >> 5];

		/* Check broken path w/o main connected */
		if (f->run_mode != TILE_RUN_MODE_MAIN && !f->output_disable_flag) {
			if (f->tot_branch_num) {
				for (j = 0; j < f->tot_branch_num; j++) {
					int k = f->next_blk_num[j];
					int mask = 1 << (k & 0x1F);

					if (!(func_param->valid_flag[k >> 5] & mask))
						break;
				}

				if (f->tot_branch_num == j)
					*valid |= 1 << (id & 0x1F);
			} else {
				*valid |= 1 << (id & 0x1F);
			}
		}

		if (f->prev_blk_num[0] == PREVIOUS_BLK_NO_OF_START)
			if (*valid & (1 << (id & 0x1F)))
				return T_BROKEN_SUB_PATH_ERROR;
	}

	return result;
}

static enum mdp_tile_msg tile_bwd_comp(struct tile_reg_map *reg_map,
				       struct func_description *func_param)
{
	enum mdp_tile_msg result = T_OK;
	int i;

	/* Run normal tile */
	if (reg_map->backup_x_skip_y)
		return T_OK;

	/* Scheduling backward order */
	for (i = 0; i < func_param->used_func_no; i++) {
		unsigned char order = func_param->scheduling_backward_order[i];
		struct tile_func_block *f = func_param->func_list[order];

		if (f->output_disable_flag)
			continue;

		/* Skip diff run mode */
		if (reg_map->run_mode != f->run_mode)
			continue;

		/* Backward comp by func */
		result = tile_bwd_output_config(f, reg_map, func_param);
		if (result)
			goto err_return;
		result = tile_bwd_by_func(f, reg_map);
		if (result)
			goto err_return;
		/* Check input smaller than tile size */
		result = tile_bwd_input_check(f, reg_map);
		if (result)
			goto err_return;
	}

err_return:
	return result;
}

static enum mdp_tile_msg tile_bwd_comp_min(struct tile_reg_map *reg_map,
					   struct func_description *func_param)
{
	enum mdp_tile_msg result = T_OK;

	if (reg_map->backup_x_skip_y)
		return T_OK;

	/* init first_pass enable */
	if (reg_map->first_frame)
		return T_OK;

	/* run min tile cal */
	if (reg_map->tdr_ctrl_en) {
		reg_map->first_pass = 1;
		result =  tile_bwd_comp_min_tile(reg_map, func_param);
		reg_map->first_pass = 0;
	}

	return result;
}

static enum mdp_tile_msg tile_bwd_comp_min_tile(struct tile_reg_map *reg_map,
						struct func_description *func_param)
{
	enum mdp_tile_msg result = T_OK;
	int i;

	/* Scheduling backward order */
	for (i = 0; i < func_param->used_func_no; i++) {
		unsigned char order = func_param->scheduling_backward_order[i];
		struct tile_func_block *f = func_param->func_list[order];

		/* Skip output disable func */
		if (f->output_disable_flag)
			continue;
		if (reg_map->run_mode != f->run_mode)
			continue;

		result = tile_bwd_min_tile_init(f, reg_map);
		if (result)
			goto err_return;
		/* Backward comp by func */
		result = tile_bwd_output_config_min_tile(f, reg_map, func_param);
		if (result)
			goto err_return;
		/* Cal min tile backward */
		result = tile_bwd_by_func(f, reg_map);
		if (result)
			goto err_return;
		/* Check input smaller than tile size */
		result = tile_bwd_input_check(f, reg_map);
		if (result)
			goto err_return;
		result = tile_bwd_min_tile_backup_input(f, reg_map);
		if (result)
			goto err_return;
		result = tile_bwd_min_tile_restore(f, reg_map);
		if (result)
			goto err_return;
	}

err_return:
	return result;
}

static enum mdp_tile_msg tile_schedule_backward(struct func_description *func_param)
{
	int no = func_param->used_func_no;
	int i, j, k;

	/* Clear valid flag */
	memset(func_param->valid_flag, 0x0, 4 * ((no + 31) >> 5));

	/* Scheduling backward */
	for (i = 0; i < no; i++) {
		bool found_flag = false;
		unsigned char *order = &func_param->scheduling_backward_order[i];

		for (j = 0; j < no; j++) {
			unsigned int *valid = &func_param->valid_flag[j >> 5];
			struct tile_func_block *f = func_param->func_list[j];
			int idx, mask;

			if (*valid & (1 << (j & 0x1F)))
				continue;

			/* Non-branch to set valid if next valid */
			if (f->tot_branch_num == 1) {
				idx = f->next_blk_num[0];
				mask = (1 << (idx & 0x1F));
				idx >>= 5;
				if (func_param->valid_flag[idx] & mask) {
					*order = j;
					*valid |= 1 << (j & 0x1F);
					found_flag = true;
				}
			} else if (f->tot_branch_num == 0) {
			    /* Non-branch to set valid if next end */
				*order = j;
				*valid |= 1 << (j & 0x1F);
				found_flag = true;
			} else {
			    /* Non-branch to set valid if all branches valid */
				for (k = 0; k < f->tot_branch_num; k++) {
					idx = f->next_blk_num[k];
					mask = (1 << (idx & 0x1F));
					idx >>= 5;
					/* Stop when invalid found */
					if (!(func_param->valid_flag[idx] & mask))
						break;
				}

				/* Set valid if all valid */
				if (k == f->tot_branch_num) {
					*order = j;
					*valid |= 1 << (j & 0x1F);
					found_flag = true;
				}
			}

			if (found_flag)
				break;
		}

		if (!found_flag)
			return T_SCHEDULING_BACKWARD_ERROR;
	}

	return T_OK;
}

enum mdp_tile_msg tile_mode_init(struct tile_reg_map *reg_map,
				 struct func_description *func_param)
{
	int i;

	/* Reset necessary variables only */
	reg_map->skip_x_cal = false;
	reg_map->skip_y_cal = false;
	reg_map->backup_x_skip_y = false;
	reg_map->used_tile_no = 0;
	reg_map->horizontal_tile_no = 0;
	reg_map->curr_horizontal_tile_no = 0;
	reg_map->curr_vertical_tile_no = 0;
	reg_map->run_mode = TILE_RUN_MODE_MAIN;

	if (reg_map->first_frame)
		return T_OK;
	if (!reg_map->found_sub_out)
		return T_OK;

	/* Set all sub out to normal mode to prevent too small size in end tile of sub out */
	for (i = 0; i < func_param->used_func_no; i++) {
		struct tile_func_block *f = func_param->func_list[i];

		if (f->output_disable_flag)
			continue;

		if (f->run_mode == TILE_RUN_MODE_SUB_OUT) {
			f->type &= ~TILE_TYPE_DONT_CARE_END;
			f->run_mode = TILE_RUN_MODE_MAIN;
		}
	}
	reg_map->found_sub_out = false;

	return T_OK;
}

static enum mdp_tile_msg tile_check_valid_output(struct tile_reg_map *reg_map,
						 struct func_description *func_param)
{
	int i;
	bool found_output_count_x = false;
	bool found_output_count_y = false;
	int tile_reg_map_run_mode = reg_map->run_mode;
	int tile_reg_map_skip_x_cal = reg_map->skip_x_cal;
	int tile_reg_map_skip_y_cal = reg_map->skip_y_cal;

	if (reg_map->backup_x_skip_y)
		return T_OK;

	if (reg_map->first_frame)
		return T_OK;

	for (i = 0; i < func_param->used_func_no; i++) {
		/* Faster stop by backward order */
		unsigned char id = func_param->scheduling_backward_order[i];
		struct tile_func_block *f = func_param->func_list[id];
		bool crop_en = f->type & TILE_TYPE_CROP_EN;
		bool hit;

		if (f->output_disable_flag || !f->enable_flag ||
		    tile_reg_map_run_mode != f->run_mode)
			continue;

		if (tile_reg_map_skip_x_cal || found_output_count_x)
			goto err_skip_x_cal;

		if (f->tot_branch_num || f->tdr_h_disable_flag)
			goto err_skip_x_cal;

		if (!f->valid_h_no) {
			found_output_count_x = true;
			goto err_skip_x_cal;
		}

		if (f->type & TILE_TYPE_DONT_CARE_END) {
			found_output_count_x = true;
			goto err_skip_x_cal;
		}

		/* Not direct link */
		if (f->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT) {
			hit = (f->out_pos_xe + 1 == f->last_output_xs_pos);
			if (!crop_en || hit)
				if (f->out_pos_xs < f->last_output_xs_pos)
					found_output_count_x = true;
		} else {
			hit = (f->out_pos_xs == f->last_output_xe_pos + 1);
			if (!crop_en || hit)
				if (f->out_pos_xe > f->last_output_xe_pos)
					found_output_count_x = true;
		}

err_skip_x_cal:
		if (tile_reg_map_skip_y_cal ||
		    found_output_count_y)
			goto err_skip_y_cal;

		if (f->tot_branch_num ||
		    f->tdr_v_disable_flag)
			goto err_skip_y_cal;

		if (!f->valid_v_no) {
			found_output_count_y = true;
			goto err_skip_y_cal;
		}

		if (!(f->type & TILE_TYPE_DONT_CARE_END)) { /* Not direct link */
			hit = (f->out_pos_ys == f->last_output_ye_pos + 1);
			if (!crop_en || hit)
				if (f->out_pos_ye > f->last_output_ye_pos)
					found_output_count_y = true;
		} else {
			found_output_count_y = true;
		}

err_skip_y_cal:
		if ((found_output_count_x || tile_reg_map_skip_x_cal) &&
		    (found_output_count_y || tile_reg_map_skip_y_cal))
			return T_OK;
	}

	if (!tile_reg_map_skip_x_cal)
		if (!found_output_count_x)
			return T_DIFF_VIEW_OUTPUT_ERROR;

	if (!tile_reg_map_skip_y_cal)
		if (!found_output_count_y)
			return T_DIFF_VIEW_OUTPUT_ERROR;

	return T_OK;
}

static enum mdp_tile_msg tile_check_min_tile(struct tile_reg_map *reg_map,
					     struct func_description *func_param)
{
	int i;
	bool found_output_count_x = false;
	bool found_output_count_y = false;
	int tile_reg_map_run_mode = reg_map->run_mode;
	int tile_reg_map_skip_x_cal = reg_map->skip_x_cal;
	int tile_reg_map_skip_y_cal = reg_map->skip_y_cal;

	if (!reg_map->tdr_ctrl_en)
		return T_OK;

	if (reg_map->backup_x_skip_y)
		return T_OK;

	if (reg_map->first_frame)
		return T_OK;

	for (i = 0; i < func_param->used_func_no; i++) {
		/* Must check by forward order */
		unsigned char id = func_param->scheduling_forward_order[i];
		struct tile_func_block *f = func_param->func_list[id];

		if (f->output_disable_flag)
			continue;

		if (!f->enable_flag)
			continue;

		if (tile_reg_map_run_mode != f->run_mode)
			continue;

		if (!tile_reg_map_skip_x_cal) {
			if (!found_output_count_x) {
				/* Check valid input */
				if (f->prev_blk_num[0] == PREVIOUS_BLK_NO_OF_START)
					if (f->tdr_h_disable_flag)
						return T_DIFF_VIEW_INPUT_ERROR;

				/* Check valid output */
				if (!f->tot_branch_num && !f->tdr_h_disable_flag)
					found_output_count_x = true;
			}
		}

		if (!tile_reg_map_skip_y_cal) {
			if (!found_output_count_y) {
				/* Check valid input */
				if (f->prev_blk_num[0] == PREVIOUS_BLK_NO_OF_START) {
					if (f->tdr_v_disable_flag)
						return T_DIFF_VIEW_INPUT_ERROR;
				}
				/* Check valid output */
				if (!f->tot_branch_num && !f->tdr_v_disable_flag)
					found_output_count_y = true;
			}
		}

		if ((found_output_count_x || tile_reg_map_skip_x_cal) &&
		    (found_output_count_y || tile_reg_map_skip_y_cal))
			return T_OK;
	}

	if (!tile_reg_map_skip_x_cal)
		if (!found_output_count_x)
			return T_DIFF_VIEW_OUTPUT_ERROR;

	if (!tile_reg_map_skip_y_cal)
		if (!found_output_count_y)
			return T_DIFF_VIEW_OUTPUT_ERROR;

	return T_OK;
}

enum mdp_tile_msg tile_frame_mode_init(struct tile_reg_map *reg_map,
				       struct func_description *func_param)
{
	enum mdp_tile_msg result;
	int i;

	reg_map->first_frame = 1;

	result = tile_mode_init(reg_map, func_param);
	if (result)
		goto err_return;

	for (i = 0; i < func_param->used_func_no; i++) {
		struct tile_func_block *f = func_param->func_list[i];

		if (f->output_disable_flag)
			continue;

		f->min_in_pos_xs = MAX_SIZE;
		f->max_in_pos_xe = 0;
		f->min_in_pos_ys = MAX_SIZE;
		f->max_in_pos_ye = 0;
		f->in_tile_width_backup = f->in_tile_width;
		f->in_tile_height_backup = f->in_tile_height;
		f->out_tile_width_backup = f->out_tile_width;
		f->out_tile_height_backup = f->out_tile_height;
		f->in_tile_width = 0;
		f->in_tile_height = 0;
		f->out_tile_width = 0;
		f->out_tile_height = 0;
		f->min_tile_in_pos_xs = MAX_SIZE;
		f->min_tile_in_pos_xe = 0;
		f->min_tile_in_pos_ys = MAX_SIZE;
		f->min_tile_in_pos_ye =  0;
	}

err_return:
	return result;
}

enum mdp_tile_msg tile_frame_mode_close(struct tile_reg_map *reg_map,
					struct func_description *func_param)
{
	int i;

	for (i = 0; i < func_param->used_func_no; i++) {
		struct tile_func_block *f = func_param->func_list[i];

		if (f->output_disable_flag)
			continue;

		f->in_tile_width = f->in_tile_width_backup;
		f->in_tile_height = f->in_tile_height_backup;
		f->out_tile_width = f->out_tile_width_backup;
		f->out_tile_height = f->out_tile_height_backup;
		/*
		 * Update min & max pos only frame tdr is not skipped to
		 * prevent from error of min tile cal.
		 */
		if (f->run_mode == TILE_RUN_MODE_SUB_OUT) {
			f->min_in_pos_xs = f->in_pos_xs;
			f->max_in_pos_xe = f->in_pos_xe;
			f->min_in_pos_ys =  f->in_pos_ys;
			f->max_in_pos_ye = f->in_pos_ye;
		} else {
			f->min_in_pos_xs = f->backward_input_xs_pos;
			f->max_in_pos_xe = f->backward_input_xe_pos;
			f->min_in_pos_ys =  f->backward_input_ys_pos;
			f->max_in_pos_ye = f->backward_input_ye_pos;
		}
		f->min_out_pos_xs = f->out_pos_xs;
		f->max_out_pos_xe = f->out_pos_xe;
		f->min_out_pos_ys = f->out_pos_ys;
		f->max_out_pos_ye = f->out_pos_ye;
	}

	reg_map->first_frame = 0;

	return T_OK;
}

static enum mdp_tile_msg tile_check_input_config(struct tile_func_block *blk,
						 struct tile_reg_map *reg_map)
{
	if (reg_map->skip_x_cal)
		goto err_skip_x_cal;

	if (blk->tdr_h_disable_flag)
		goto err_skip_x_cal;

	/* Check input x size & pos */
	if (blk->full_size_x_in <= 0 ||
	    blk->in_pos_xs < 0 ||
	    blk->in_pos_xe >= blk->full_size_x_in ||
	    blk->in_pos_xs > blk->in_pos_xe)
		return T_INIT_INCOR_X_INPUT_SIZE_POS_ERROR;

	/* Skip start time check */
	if (blk->valid_h_no) {
		/* Check cal order, input */
		if (blk->in_cal_order & TILE_ORDER_RIGHT_TO_LEFT) {
			if (blk->in_pos_xe > blk->last_input_xe_pos)
				return T_TILE_LOSS_OVER_TILE_WIDTH_ERROR;
		} else {
			if (blk->in_pos_xs < blk->last_input_xs_pos)
				return T_TILE_LOSS_OVER_TILE_WIDTH_ERROR;
		}
	}

	/* Check mis-algin xe & ye compensated by over tile size in backward */
	if (blk->in_const_x > 1) {
		if (TILE_MOD(blk->in_pos_xe + 1, blk->in_const_x))
			return T_CHECK_IN_CONFIG_ALIGN_XE_POS_ERROR;

		if (TILE_MOD(blk->in_pos_xs, blk->in_const_x))
			return T_CHECK_IN_CONFIG_ALIGN_XS_POS_ERROR;
	}

err_skip_x_cal:
	if (reg_map->skip_y_cal)
		goto err_skip_y_cal;

	if (blk->tdr_v_disable_flag)
		goto err_skip_y_cal;

	/* Check input y size & pos */
	if (blk->full_size_y_in <= 0 ||
	    blk->in_pos_ys < 0 ||
	    blk->in_pos_ye >= blk->full_size_y_in ||
	    blk->in_pos_ys > blk->in_pos_ye)
		return T_INIT_INCOR_Y_INPUT_SIZE_POS_ERROR;

	/* Skip start time check */
	if (blk->valid_v_no)
		if (blk->in_pos_ys < blk->last_input_ys_pos)
			return T_TILE_LOSS_OVER_TILE_HEIGHT_ERROR;

	/* Check mis-algin xe & ye compensated by over tile size in backward */
	if (blk->in_const_y > 1) {
		if (TILE_MOD(blk->in_pos_ye + 1, blk->in_const_y))
			return T_CHECK_IN_CONFIG_ALIGN_YE_POS_ERROR;

		if (TILE_MOD(blk->in_pos_ys, blk->in_const_y))
			return T_CHECK_IN_CONFIG_ALIGN_YS_POS_ERROR;
	}

err_skip_y_cal:
	return T_OK;
}

static enum mdp_tile_msg tile_check_output_config_x(struct tile_func_block *blk,
						    struct tile_reg_map *reg_map)
{
	const int out_const_x = blk->out_const_x;

	if (reg_map->skip_x_cal)
		goto err_return;

	if (blk->tdr_h_disable_flag)
		goto err_return;

	/* Check output x size & pos */
	if (blk->min_out_pos_xs < 0 ||
	    blk->max_out_pos_xe < 0 ||
	    blk->min_out_pos_xs > blk->max_out_pos_xe ||
	    blk->out_pos_xs < blk->min_out_pos_xs ||
	    blk->out_pos_xe > blk->max_out_pos_xe ||
	    blk->out_pos_xs > blk->out_pos_xe)
		return T_INIT_INCOR_X_OUTPUT_SIZE_POS_ERROR;

	/* Check alignment */
	if (out_const_x > 1) {
		if (TILE_MOD(blk->out_pos_xs, out_const_x))
			return T_CHECK_OUT_CONFIG_ALIGN_XS_POS_ERROR;

		if (TILE_MOD(blk->out_pos_xe + 1, out_const_x))
			return T_CHECK_OUT_CONFIG_ALIGN_XE_POS_ERROR;
	}

	/* Non-end func */
	if (blk->tot_branch_num > 0 || (blk->type & TILE_TYPE_DONT_CARE_END))
		if (blk->valid_h_no) /* Skip start time check */
			if (blk->out_pos_xs < blk->last_output_xs_pos)
				return T_TILE_LOSS_OVER_TILE_WIDTH_ERROR;

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_check_output_config_x_inv(struct tile_func_block *blk,
							struct tile_reg_map *reg_map)
{
	const int out_const_x = blk->out_const_x;

	if (reg_map->skip_x_cal)
		goto err_return;

	if (blk->tdr_h_disable_flag)
		goto err_return;

	/* Check output x size & pos */
	if (blk->min_out_pos_xs < 0 ||
	    blk->max_out_pos_xe < 0 ||
	    blk->min_out_pos_xs > blk->max_out_pos_xe ||
	    blk->out_pos_xs < blk->min_out_pos_xs ||
	    blk->out_pos_xe > blk->max_out_pos_xe ||
	    blk->out_pos_xs > blk->out_pos_xe)
		return T_INIT_INCOR_X_OUTPUT_SIZE_POS_ERROR;

	/* Check alignment */
	if (out_const_x > 1) {
		if (TILE_MOD(blk->out_pos_xs, out_const_x))
			return T_CHECK_OUT_CONFIG_ALIGN_XS_POS_ERROR;

		if (TILE_MOD(blk->out_pos_xe + 1, out_const_x))
			return T_CHECK_OUT_CONFIG_ALIGN_XE_POS_ERROR;
	}

	/* Non-end func */
	if (blk->tot_branch_num > 0 || (blk->type & TILE_TYPE_DONT_CARE_END))
		if (blk->valid_h_no) /* Skip start time check */
			if (blk->out_pos_xe > blk->last_output_xe_pos)
				return T_TILE_LOSS_OVER_TILE_WIDTH_ERROR;

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_check_output_config_y(struct tile_func_block *blk,
						    struct tile_reg_map *reg_map)
{
	const int out_const_y = blk->out_const_y;

	if (reg_map->skip_y_cal)
		goto err_return;

	if (blk->tdr_v_disable_flag)
		goto err_return;

	/* Check output y size & pos */
	if (blk->min_out_pos_ys < 0 ||
	    blk->max_out_pos_ye < 0 ||
	    blk->min_out_pos_ys > blk->max_out_pos_ye ||
	    blk->out_pos_ys < blk->min_out_pos_ys ||
	    blk->out_pos_ye > blk->max_out_pos_ye ||
	    blk->out_pos_ys > blk->out_pos_ye) {
		return T_INIT_INCOR_Y_OUTPUT_SIZE_POS_ERROR;
	}

	/* Check alignment */
	if (out_const_y > 1) {
		if (TILE_MOD(blk->out_pos_ys, out_const_y))
			return T_CHECK_OUT_CONFIG_ALIGN_YS_POS_ERROR;

		if (TILE_MOD(blk->out_pos_ye + 1, out_const_y))
			return T_CHECK_OUT_CONFIG_ALIGN_YE_POS_ERROR;
	}

	/* Non-end func */
	if (blk->tot_branch_num > 0 || (blk->type & TILE_TYPE_DONT_CARE_END))
		if (blk->valid_v_no) /* Skip start time check */
			if (blk->out_pos_ys < blk->last_output_ys_pos)
				return T_TILE_LOSS_OVER_TILE_HEIGHT_ERROR;

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_check_output_config(struct tile_func_block *blk,
						  struct tile_reg_map *reg_map)
{
	enum mdp_tile_msg result = T_OK;

	if (!reg_map->skip_x_cal && !blk->tdr_h_disable_flag) {
		/* Check cal order, output */
		if (blk->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
			result = tile_check_output_config_x_inv(blk, reg_map);
		else
			result = tile_check_output_config_x(blk, reg_map);

		if (result)
			return result;
	}

	if (!reg_map->skip_y_cal && !blk->tdr_v_disable_flag)
		result = tile_check_output_config_y(blk, reg_map);

	return result;
}

static enum mdp_tile_msg tile_bwd_output_config_skip(struct tile_reg_map *reg_map,
						     struct func_description *func_param)
{
	int no = reg_map->curr_horizontal_tile_no;
	int i;

	/* Only check last & non-skipped */
	if (reg_map->curr_vertical_tile_no == 0) { /* First tile row */
		/* Last is end of right but not end tile */
		if (no == 0) { /* first tile */
			reg_map->skip_x_cal = false;
			reg_map->skip_y_cal = false;
		} else {
			reg_map->skip_x_cal = false;
			reg_map->skip_y_cal = true;
		}
		reg_map->backup_x_skip_y = false; /* x & y not skipped */
	} else { /* Middle row */
		/* Last is end of right but not end tile */
		if (no == 0) /* first tile column, y will need to cal */
			reg_map->skip_y_cal = false;
		else
			reg_map->skip_y_cal = true;

		if (no < MAX_TILE_BACKUP_HORZ_NO) { /* y will not need to cal */
			reg_map->skip_x_cal = true;

			for (i = 0; i < func_param->used_func_no; i++) {
				struct tile_func_block *f = func_param->func_list[i];
				struct tile_horz_backup *hor = &f->horz_para[no];

				if (!f->output_disable_flag)
					HORZ_PARA_RESTORE(hor, f);
			}
		} else {
			/* Not enough buffer to store tile horizontal parameters */
			reg_map->skip_x_cal = false;
		}

		if (reg_map->skip_x_cal && reg_map->skip_y_cal)
			reg_map->backup_x_skip_y = true;
		else
			reg_map->backup_x_skip_y = false;
	}

	return T_OK;
}

static enum mdp_tile_msg tile_bwd_output_config_x(struct tile_func_block *blk,
						  struct tile_reg_map *reg_map,
						  struct func_description *func_param)
{
	struct tile_func_block *n;
	const int out_const_x = blk->out_const_x;
	int val_e = TILE_MOD(blk->out_pos_xe + 1, out_const_x);
	int mode = reg_map->run_mode;
	int max_w, max_s, max_e;
	int bound;
	bool run_cal;
	int o_xs = MAX_SIZE;
	int o_xe = 0;
	int min_xs = MAX_SIZE;
	int min_xe = 0;
	bool max_h_edge_flag = true;
	bool h_end_flag = true;
	bool crop_h_end_flag = true;
	bool last_full_range_flag = true;
	int count = 0;
	int out_tile_width_loss = 0;
	int max_out_crop_xe = 0;
	int min_t_xe = 0;
	int i;

	if (reg_map->skip_x_cal)
		goto err_return;

	if (blk->tdr_h_disable_flag) {
		/* Backup direct flag */
		blk->direct_out_pos_xs = blk->out_pos_xs;
		blk->direct_out_pos_xe = blk->out_pos_xe;
		blk->direct_h_end_flag = blk->h_end_flag;
		goto err_return;
	}

	run_cal = !reg_map->first_pass &&
		  !reg_map->first_frame &&
		  reg_map->tdr_ctrl_en &&
		  blk->run_mode == TILE_RUN_MODE_MAIN;

	/* Tile movement */
	if (blk->tot_branch_num == 0) { /* End module only */
		/* Not direct link */
		if (!(blk->type & TILE_TYPE_DONT_CARE_END)) {
			/* Init output size of final module */
			if (blk->valid_h_no == 0) {
				/* First tile */
				bound = blk->min_out_pos_xs + blk->out_tile_width;
				blk->out_pos_xs = blk->min_out_pos_xs;
				blk->out_pos_xe = blk->out_tile_width ?
							bound - 1 : blk->max_out_pos_xe;
			} else {
				/* Move right to set output pos */
				if (blk->last_output_xe_pos < blk->max_out_pos_xe) {
					bound = blk->last_output_xe_pos + blk->out_tile_width;
					blk->out_pos_xs = blk->last_output_xe_pos + 1;
					blk->out_pos_xe = blk->out_tile_width ?
								bound : blk->max_out_pos_xe;
				} else {
					/* Keep min output size */
					blk->out_pos_xs = blk->max_out_pos_xe - out_const_x + 1;
					blk->out_pos_xe = blk->max_out_pos_xe;
				}
			}
			if (out_const_x > 1 && val_e)
				blk->out_pos_xe -= val_e;

			/* Check size equal to full size */
			if (blk->out_pos_xe >= blk->max_out_pos_xe) {
				blk->out_pos_xe = blk->max_out_pos_xe;
				blk->max_h_edge_flag = true;
				blk->h_end_flag = true;
				blk->crop_h_end_flag = true;
			} else {
				blk->h_end_flag = false;
				blk->crop_h_end_flag = false;
				bound = blk->out_pos_xs + blk->out_tile_width;
				if (blk->out_tile_width && (bound < blk->max_out_pos_xe + 1))
					blk->max_h_edge_flag = false;
				else
					blk->max_h_edge_flag = true;
			}
		} else { /* Direct link */
			blk->out_pos_xs = blk->direct_out_pos_xs;
			blk->out_pos_xe = blk->direct_out_pos_xe;
			blk->h_end_flag = blk->direct_h_end_flag;
			blk->crop_h_end_flag = blk->direct_h_end_flag;
		}
		blk->out_tile_width_max = blk->out_tile_width;
		blk->out_tile_width_max_str = blk->out_tile_width;
		blk->out_tile_width_max_end = blk->out_tile_width;
		blk->max_out_crop_xe = blk->max_out_pos_xe;
		blk->min_tile_crop_out_pos_xe = blk->min_tile_out_pos_xe;
	}  else if (blk->tot_branch_num == 1) { /* Check non-branch */
		/* Set curr out with next in */
		n = func_param->func_list[blk->next_blk_num[0]];

		blk->h_end_flag = n->h_end_flag;
		blk->crop_h_end_flag = n->crop_h_end_flag;
		/* tdr_h_disable_flag changed during back cal by sub-in */
		blk->tdr_h_disable_flag = n->tdr_h_disable_flag;
		/* Backward h_end_flag */
		if (!n->tdr_h_disable_flag) {
			blk->out_pos_xs = n->in_pos_xs;
			blk->out_pos_xe = n->in_pos_xe;
			blk->max_h_edge_flag = n->max_h_edge_flag;
			blk->out_tile_width_max = n->in_tile_width_max;
			blk->out_tile_width_max_str = n->in_tile_width_max_str;
			blk->out_tile_width_max_end = n->in_tile_width_max_end;
			/* Smart tile + ufd */
			blk->out_tile_width_loss = n->in_tile_width_loss;
			blk->max_out_crop_xe = n->max_in_crop_xe;
			blk->min_tile_crop_out_pos_xe = n->min_tile_crop_in_pos_xe;
			goto err_skip_branch;
		}

		count = 0;
		max_w = blk->out_tile_width;
		max_s = blk->out_tile_width;
		max_e = blk->out_tile_width;

		/* Min xs/ys & min xe/ye sorting for current support tile mode */
		for (i = 0; i < blk->tot_branch_num; i++) {
			n = func_param->func_list[blk->next_blk_num[i]];

			/* Skip output disabled branch */
			if (n->output_disable_flag)
				continue;
			if ((mode & n->run_mode) != mode)
				continue;

			h_end_flag &= n->h_end_flag;
			crop_h_end_flag &= n->crop_h_end_flag;
			/* tdr_h_disable_flag changed during back cal by sub-in */
			if (n->tdr_h_disable_flag)
				continue;

			max_h_edge_flag &= n->max_h_edge_flag;
			if (max_w && n->in_tile_width_max) {
				if (max_w > n->in_tile_width_max)
					max_w = n->in_tile_width_max;
			} else if (n->in_tile_width_max) {
				max_w = n->in_tile_width_max;
			}

			if (max_s && n->in_tile_width_max_str) {
				if (max_s > n->in_tile_width_max_str)
					max_s = n->in_tile_width_max_str;
			} else if (n->in_tile_width_max_str) {
				max_s = n->in_tile_width_max_str;
			}

			if (max_e && n->in_tile_width_max_end) {
				if (max_e > n->in_tile_width_max_end)
					max_e = n->in_tile_width_max_end;
			} else if (n->in_tile_width_max_end) {
				max_e = n->in_tile_width_max_end;
			}

			/* Smart tile + ufd */
			if (out_tile_width_loss <= n->in_tile_width_loss) {
				if (out_tile_width_loss == n->in_tile_width_loss) {
					if (max_out_crop_xe < n->max_in_crop_xe) {
						max_out_crop_xe = n->max_in_crop_xe;
						min_t_xe = n->min_tile_crop_in_pos_xe;
					}
				} else {
					out_tile_width_loss = n->in_tile_width_loss;
					max_out_crop_xe = n->max_in_crop_xe;
					min_t_xe = n->min_tile_crop_in_pos_xe;
				}
			}
			if (mode != TILE_RUN_MODE_MAIN) {
				/* Sub-in with multi-out */
				if (count) {
					if (o_xs != n->in_pos_xs ||
					    o_xe != n->in_pos_xe)
						return T_DIFF_NEXT_CONFIG_ERROR;
				} else {
					o_xs = n->in_pos_xs;
					o_xe = n->in_pos_xe;
				}
				count++;
				continue;
			}

			bwd_output_x_update_range(n, run_cal, &o_xs, &o_xe,
						  &min_xs, &min_xe,
						  &last_full_range_flag);
			count++;
		}

		/* Update h_end_flag */
		blk->h_end_flag = h_end_flag;
		blk->crop_h_end_flag = crop_h_end_flag;
		if (!count) {
			blk->tdr_h_disable_flag = true;
			goto err_skip_branch;
		}
		if (!run_cal)
			goto err_skip_out_pos;

		bwd_output_x_clamp(&o_xs, &o_xe, &min_xs, &min_xe, h_end_flag);
		if (o_xe < blk->min_tile_out_pos_xe)
			o_xe = blk->min_tile_out_pos_xe;

err_skip_out_pos:
		blk->out_pos_xs = o_xs;
		blk->out_pos_xe = o_xe;
		blk->max_h_edge_flag = max_h_edge_flag;
		/* Update tdr_h_disable_flag changed during back cal by sub-in */
		blk->tdr_h_disable_flag = false;
		blk->out_tile_width_max = max_w;
		blk->out_tile_width_max_str = max_s;
		blk->out_tile_width_max_end = max_e;
		/* Smart tile + ufd */
		blk->out_tile_width_loss = out_tile_width_loss;
		blk->max_out_crop_xe = max_out_crop_xe;
		blk->min_tile_crop_out_pos_xe = min_t_xe;
	}

err_skip_branch:
	if (blk->tdr_h_disable_flag)
		goto err_return;

	/* Right edge */
	max_e = blk->out_tile_width_max_end;
	if (blk->out_pos_xe + 1 >= blk->full_size_x_out) {
		if (max_e) {
			if (max_e + blk->out_pos_xs < blk->full_size_x_out) {
				blk->out_pos_xe = blk->full_size_x_out - 1 - out_const_x;
				blk->max_h_edge_flag = false; /* Update flag */
				blk->crop_h_end_flag = false; /* Update flag */
				blk->h_end_flag = false; /* Update flag */
			}
		}
	}

	/* Left edge */
	max_s = blk->out_tile_width_max_str;
	if (blk->out_pos_xs <= 0 && max_s) {
		if (max_s < blk->out_pos_xe + 1) {
			blk->in_pos_xe = max_s - 1;
			blk->max_h_edge_flag = false; /* Update flag */
			blk->crop_h_end_flag = false; /* Update flag */
			blk->h_end_flag = false; /* Update flag */
			if (out_const_x > 1 && val_e)
				blk->out_pos_xe -= val_e;
		}
	}

	/* Check over tile size with enable & skip buffer check false */
	max_w = blk->out_tile_width_max;
	if (blk->out_tile_width || max_w) {
		int out_tile_width = blk->out_tile_width;
		int xs = blk->out_pos_xs;

		if (out_tile_width) {
			if (max_w && out_tile_width > max_w)
				out_tile_width = max_w;
		} else {
			out_tile_width = max_w;
		}

		if (blk->out_pos_xe + 1 > xs + out_tile_width) {
			blk->max_h_edge_flag = false; /* Update flag */
			blk->crop_h_end_flag = false; /* Update flag */
		}

		/* Tile size constraint check */
		if (run_cal && blk->out_max_width) {
			if (blk->out_max_width < out_tile_width) {
				bound = xs + blk->out_max_width;
				if (bound < blk->min_tile_out_pos_xe + 1)
					out_tile_width = blk->min_tile_out_pos_xe - xs + 1;
				else
					out_tile_width = blk->out_max_width;
			}
		}

		if (blk->out_pos_xe + 1 > xs + out_tile_width) {
			blk->out_pos_xe = xs + out_tile_width - 1;
			blk->h_end_flag = false; /* Only update h_end_flag */
			if (out_const_x > 1 && val_e)
				blk->out_pos_xe -= val_e;
		}
	} else if (blk->out_max_width) {
		int xs = blk->out_pos_xs;

		if (run_cal) {
			if (xs + blk->out_max_width > blk->min_tile_out_pos_xe + 1) {
				if (blk->out_pos_xe + 1 > xs + blk->out_max_width) {
					blk->out_pos_xe = xs + blk->out_max_width - 1;
					blk->h_end_flag = false; /* Only update h_end_flag */
					if (out_const_x > 1 && val_e)
						blk->out_pos_xe -= val_e;
				}
			}
		}
	}

	return tile_check_output_config_x(blk, reg_map);

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_bwd_output_config_x_inv(struct tile_func_block *blk,
						      struct tile_reg_map *reg_map,
						      struct func_description *func_param)
{
	struct tile_func_block *n;
	int out_const_x = blk->out_const_x;
	int val_s = TILE_MOD(blk->out_pos_xs, out_const_x);
	int mode = reg_map->run_mode;
	int max_w = blk->out_tile_width;
	int max_s = blk->out_tile_width;
	int max_e = blk->out_tile_width;
	int bound;
	bool run_cal;
	int i;

	if (reg_map->skip_x_cal)
		goto err_return;

	if (blk->tdr_h_disable_flag) {
		/* Backup direct flag */
		blk->direct_out_pos_xs = blk->out_pos_xs;
		blk->direct_out_pos_xe = blk->out_pos_xe;
		blk->direct_h_end_flag = blk->h_end_flag;
		goto err_return;
	}

	run_cal = !reg_map->first_pass &&
		  !reg_map->first_frame &&
		  reg_map->tdr_ctrl_en &&
		  blk->run_mode == TILE_RUN_MODE_MAIN;

	/* Tile movement */
	if (blk->tot_branch_num == 0) { /* End module only */
		/* Not diect link */
		if (!(blk->type & TILE_TYPE_DONT_CARE_END)) {
			/* Init output size of final module */
			if (blk->valid_h_no == 0) {
				/* First tile */
				blk->out_pos_xs = blk->out_tile_width ?
					(blk->max_out_pos_xe - blk->out_tile_width + 1) :
					blk->min_out_pos_xs;
				blk->out_pos_xe = blk->max_out_pos_xe;
			} else {
				/* Move left to set output pos */
				if (blk->last_output_xs_pos > blk->min_out_pos_xs) {
					blk->out_pos_xs = blk->out_tile_width ?
						(blk->last_output_xs_pos - blk->out_tile_width) :
						blk->min_out_pos_xs;
					blk->out_pos_xe = blk->last_output_xs_pos - 1;
				} else {
					/* Keep min output size */
					blk->out_pos_xs = blk->min_out_pos_xs;
					blk->out_pos_xe = blk->min_out_pos_xs + out_const_x - 1;
				}
			}

			if (out_const_x > 1 && val_s)
				blk->out_pos_xs += out_const_x - val_s;

			/* Check over max size */
			if (blk->out_pos_xs <= blk->min_out_pos_xs) {
				blk->out_pos_xs = blk->min_out_pos_xs;
				blk->max_h_edge_flag = true;
				blk->h_end_flag = true;
				blk->crop_h_end_flag = true;
			} else {
				blk->h_end_flag = false;
				blk->crop_h_end_flag = false;
				bound = blk->min_out_pos_xs + blk->out_tile_width;
				if (blk->out_tile_width && (blk->out_pos_xe + 1 > bound))
					blk->max_h_edge_flag = false;
				else
					blk->max_h_edge_flag = true;
			}
		} else { /* Direct link */
			blk->out_pos_xs = blk->direct_out_pos_xs;
			blk->out_pos_xe = blk->direct_out_pos_xe;
			blk->h_end_flag = blk->direct_h_end_flag;
			blk->crop_h_end_flag = blk->direct_h_end_flag;
		}

		blk->out_tile_width_max = blk->out_tile_width;
		blk->out_tile_width_max_str = blk->out_tile_width;
		blk->out_tile_width_max_end = blk->out_tile_width;
		blk->min_out_crop_xs = blk->min_out_pos_xs;
		blk->min_tile_crop_out_pos_xs = blk->min_tile_out_pos_xs;
	} else if (blk->tot_branch_num == 1) { /* Check non-branch */
		/* Set curr out with next in */
		n = func_param->func_list[blk->next_blk_num[0]];

		blk->h_end_flag = n->h_end_flag;
		blk->crop_h_end_flag = n->crop_h_end_flag;
		/* Update tdr_h_disable_flag for change during back cal by sub-in */
		blk->tdr_h_disable_flag = n->tdr_h_disable_flag;
		/* Backward h_end_flag */
		if (!n->tdr_h_disable_flag) {
			blk->out_pos_xs = n->in_pos_xs;
			blk->out_pos_xe = n->in_pos_xe;
			blk->max_h_edge_flag = n->max_h_edge_flag;
			blk->out_tile_width_max = n->in_tile_width_max;
			blk->out_tile_width_max_str = n->in_tile_width_max_str;
			blk->out_tile_width_max_end = n->in_tile_width_max_end;
			/* Smart tile + ufd */
			blk->out_tile_width_loss = n->in_tile_width_loss;
			blk->min_out_crop_xs = n->min_in_pos_xs;
			blk->min_tile_crop_out_pos_xs = n->min_tile_crop_in_pos_xs;
		}
	} else { /* Branch */
		int count = 0;
		int out_pos_xs = MAX_SIZE;
		int out_pos_xe = 0;
		bool max_h_edge_flag = true;
		bool h_end_flag = true;
		bool crop_h_end_flag = true;
		bool last_full_range_flag = true;
		int t_width_loss = 0;
		int min_xs = MAX_SIZE;
		int min_t_pos_xs = MAX_SIZE;

		/* Max xs/ys & max xe/ye sorting for current support tile mode */
		for (i = 0; i < blk->tot_branch_num; i++) {
			n = func_param->func_list[blk->next_blk_num[i]];

			/* Skip output disabled branch */
			if (n->output_disable_flag)
				continue;
			if ((mode & n->run_mode) != mode)
				continue;

			h_end_flag &= n->h_end_flag;
			crop_h_end_flag &= n->crop_h_end_flag;
			/* Tdr_h_disable_flag changed during back cal by sub-in */

			if (n->tdr_h_disable_flag)
				continue;

			max_h_edge_flag &= n->max_h_edge_flag;
			if (max_w && n->in_tile_width_max) {
				if (max_w > n->in_tile_width_max)
					max_w = n->in_tile_width_max;
			} else if (n->in_tile_width_max) {
				max_w = n->in_tile_width_max;
			}

			if (max_s && n->in_tile_width_max_str) {
				if (max_s > n->in_tile_width_max_str)
					max_s = n->in_tile_width_max_str;
			} else if (n->in_tile_width_max_str) {
				max_s = n->in_tile_width_max_str;
			}

			if (max_e && n->in_tile_width_max_end) {
				if (max_e > n->in_tile_width_max_end)
					max_e = n->in_tile_width_max_end;
			} else if (n->in_tile_width_max_end) {
				max_e = n->in_tile_width_max_end;
			}

			/* Smart tile + ufd */
			if (t_width_loss <= n->in_tile_width_loss) {
				/* Check equal */
				if (t_width_loss == n->in_tile_width_loss) {
					if (min_xs > n->min_in_crop_xs) {
						min_xs = n->min_in_crop_xs;
						min_t_pos_xs = n->min_tile_crop_in_pos_xs;
					}
				} else {
					t_width_loss = n->in_tile_width_loss;
					min_xs = n->min_in_crop_xs;
					min_t_pos_xs = n->min_tile_crop_in_pos_xs;
				}
			}

			if (mode != TILE_RUN_MODE_MAIN) {
				/* Sub-in with multi-out */
				if (count) {
					if (out_pos_xs != n->in_pos_xs ||
					    out_pos_xe != n->in_pos_xe)
						return T_DIFF_NEXT_CONFIG_ERROR;
				} else {
					out_pos_xs = n->in_pos_xs;
					out_pos_xe = n->in_pos_xe;
				}
				count++;
				continue;
			}

			/* Select max xe pos */
			if (out_pos_xe < n->in_pos_xe)
				out_pos_xe = n->in_pos_xe;

			if (last_full_range_flag) { /* Last full range */
				/* Full range input */
				if (n->in_pos_xs <= n->min_in_pos_xs) {
					if (out_pos_xs > n->in_pos_xs)
						out_pos_xs = n->in_pos_xs;
				} else {
					/* Non-full range input */
					out_pos_xs = n->in_pos_xs;
					last_full_range_flag = false;
				}
			} else {
				/* Non-full range input */
				if (n->in_pos_xs > n->min_in_pos_xs)
					if (out_pos_xs < n->in_pos_xs)
						out_pos_xs = n->in_pos_xs;
			}
			count++;
		}

		/* Update h_end_flag */
		blk->h_end_flag = h_end_flag;
		blk->crop_h_end_flag = crop_h_end_flag;
		if (count) {
			if (run_cal)
				if (blk->min_tile_out_pos_xs < out_pos_xs)
					out_pos_xs = blk->min_tile_out_pos_xs;

			blk->out_pos_xs = out_pos_xs;
			blk->out_pos_xe = out_pos_xe;
			blk->max_h_edge_flag = max_h_edge_flag;
			/* Update tdr_h_disable_flag changed during back cal by sub-in */
			blk->tdr_h_disable_flag = false;
			blk->out_tile_width_max = max_w;
			blk->out_tile_width_max_str = max_s;
			blk->out_tile_width_max_end = max_e;
			/* Smart tile + ufd */
			blk->out_tile_width_loss = t_width_loss;
			blk->min_out_crop_xs = min_xs;
			blk->min_tile_crop_out_pos_xs = min_t_pos_xs;
		} else {
			blk->tdr_h_disable_flag = true;
		}
	}

	if (blk->tdr_h_disable_flag)
		goto err_return;

	/* Left edge */
	if (blk->out_pos_xs <= 0 && blk->out_tile_width_max_end) {
		if (blk->out_tile_width_max_end < blk->out_pos_xe + 1) {
			blk->out_pos_xs = out_const_x;
			/* Update flag */
			blk->max_h_edge_flag = false;
			blk->crop_h_end_flag = false;
			blk->h_end_flag = false;
		}
	}

	/* Right edge */
	if (blk->out_pos_xe + 1 >= blk->full_size_x_out) {
		if (blk->out_tile_width_max_str) {
			bound = blk->out_tile_width_max_str + blk->out_pos_xs;
			if (blk->full_size_x_out > bound) {
				blk->out_pos_xs = blk->full_size_x_out -
						  blk->out_tile_width_max_str;

				/* Update flag */
				blk->max_h_edge_flag = false;
				blk->crop_h_end_flag = false;
				blk->h_end_flag = false;
				if (out_const_x > 1 && val_s)
					blk->out_pos_xs += out_const_x - val_s;
			}
		}
	}

	/* Check over tile size with enable & skip buffer check false */
	if (blk->out_tile_width || blk->out_tile_width_max) {
		int out_w = blk->out_tile_width;

		if (out_w) {
			if (blk->out_tile_width_max)
				if (blk->out_tile_width > blk->out_tile_width_max)
					out_w = blk->out_tile_width_max;
		} else {
			out_w = blk->out_tile_width_max;
		}

		if (blk->out_pos_xe + 1 > blk->out_pos_xs + out_w) {
			/* Update flag */
			blk->max_h_edge_flag = false;
			blk->crop_h_end_flag = false;
		}

		/* Tile size constraint check */
		if (run_cal && blk->out_max_width) {
			if (blk->out_max_width < out_w) {
				bound = blk->min_tile_out_pos_xs + blk->out_max_width;
				if (blk->out_pos_xe + 1 > bound)
					out_w = blk->out_pos_xe -
						blk->min_tile_out_pos_xs + 1;
				else
					out_w = blk->out_max_width;
			}
		}

		if (blk->out_pos_xe + 1 > blk->out_pos_xs + out_w) {
			blk->out_pos_xs = blk->out_pos_xe - out_w + 1;
			/* Only update h_end_flag */
			blk->h_end_flag = false;
			if (out_const_x > 1 && val_s)
				blk->out_pos_xs += out_const_x - val_s;
		}
	} else if (blk->out_max_width) {
		if (run_cal) {
			int out_w = blk->out_max_width;

			if (blk->out_pos_xs + out_w > blk->min_tile_out_pos_xe + 1) {
				if (blk->out_pos_xe + 1 > blk->out_pos_xs + out_w) {
					blk->out_pos_xs = blk->out_pos_xe - out_w + 1;
					/* Only update h_end_flag */
					blk->h_end_flag = false;
					if (out_const_x > 1 && val_s)
						blk->out_pos_xs += out_const_x - val_s;
				}
			}
		}
	}

	return tile_check_output_config_x_inv(blk, reg_map);

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_bwd_output_config_y(struct tile_func_block *blk,
						  struct tile_reg_map *reg_map,
						  struct func_description *func_param)
{
	struct tile_func_block *n;
	int out_const_y = blk->out_const_y;
	int val_e = TILE_MOD(blk->out_pos_ye + 1, out_const_y);
	int mode = reg_map->run_mode;
	int max_h, max_s, max_e;
	int bound;
	bool run_cal;
	int i;

	if (reg_map->skip_y_cal)
		goto err_return;

	if (blk->tdr_v_disable_flag) {
		/* Backup direct flag */
		blk->direct_out_pos_ys = blk->out_pos_ys;
		blk->direct_out_pos_ye = blk->out_pos_ye;
		blk->direct_v_end_flag = blk->v_end_flag;
		goto err_return;
	}

	run_cal = !reg_map->first_pass &&
		  !reg_map->first_frame &&
		  reg_map->tdr_ctrl_en &&
		  blk->run_mode == TILE_RUN_MODE_MAIN;

	/* Tile movement */
	if (blk->tot_branch_num == 0) { /* End module only */
		/* Direct link */
		if (!(blk->type & TILE_TYPE_DONT_CARE_END)) {
			/* Init output size of final module */
			if (blk->valid_v_no == 0) {
				/* First tile row */
				bound = blk->min_out_pos_ys + blk->out_tile_height;
				blk->out_pos_ys = blk->min_out_pos_ys;
				blk->out_pos_ye = blk->out_tile_height ?
						  (bound - 1) : blk->max_out_pos_ye;
			} else { /* Middle row */
				if (blk->last_output_ye_pos < blk->max_out_pos_ye) {
					bound = blk->last_output_ye_pos + blk->out_tile_height;
					blk->out_pos_ys = blk->last_output_ye_pos + 1;
					blk->out_pos_ye = blk->out_tile_height ?
							  bound : blk->max_out_pos_ye;
				} else {
					blk->out_pos_ys = blk->last_output_ye_pos -
							  out_const_y + 1;
					blk->out_pos_ye = blk->last_output_ye_pos;
				}
			}

			if (out_const_y > 1 && val_e)
				blk->out_pos_ye -= val_e;

			/* Check size */
			if (blk->out_pos_ye >= blk->max_out_pos_ye) {
				blk->out_pos_ye = blk->max_out_pos_ye;
				blk->max_v_edge_flag = true;
				blk->v_end_flag = true;
				blk->crop_v_end_flag = true;
			} else {
				blk->v_end_flag = false;
				blk->crop_v_end_flag = false;
				bound = blk->out_pos_ys + blk->out_tile_height;
				if (blk->out_tile_height && (bound < blk->max_out_pos_ye + 1))
					blk->max_v_edge_flag = false;
				else
					blk->max_v_edge_flag = true;
			}
		} else { /* Direct link */
			blk->out_pos_ys = blk->direct_out_pos_ys;
			blk->out_pos_ye = blk->direct_out_pos_ye;
			blk->v_end_flag = blk->direct_v_end_flag;
			blk->crop_v_end_flag = blk->direct_v_end_flag;
		}

		blk->out_tile_height_max = blk->out_tile_height;
		blk->out_tile_height_max_str = blk->out_tile_height;
		blk->out_tile_height_max_end = blk->out_tile_height;
	} else if (blk->tot_branch_num == 1) { /* Check non-branch */
		/* Set curr out with next in */
		n = func_param->func_list[blk->next_blk_num[0]];

		blk->v_end_flag = n->v_end_flag;
		blk->crop_v_end_flag = n->crop_v_end_flag;
		/* Update tdr_v_disable_flag for change during back cal by sub-in */
		blk->tdr_v_disable_flag = n->tdr_v_disable_flag;

		/* Backward v_end_flag */
		if (!n->tdr_v_disable_flag) {
			blk->out_pos_ys = n->in_pos_ys;
			blk->out_pos_ye = n->in_pos_ye;
			blk->max_v_edge_flag = n->max_v_edge_flag;
			blk->out_tile_height_max = n->in_tile_height_max;
			blk->out_tile_height_max_str = n->in_tile_height_max_str;
			blk->out_tile_height_max_end = n->in_tile_height_max_end;
		}
	} else { /* Branch */
		/* Min xs/ys & min xe/ye sorting for current support tile mode */
		int count = 0;
		int out_pos_ys = MAX_SIZE;
		int out_pos_ye = 0;
		bool max_v_edge_flag = true;
		bool v_end_flag = true;
		bool crop_v_end_flag = true;
		bool last_full_range_flag = true;

		max_h = blk->out_tile_height;
		max_s = blk->out_tile_height;
		max_e = blk->out_tile_height;

		for (i = 0; i < blk->tot_branch_num; i++) {
			n = func_param->func_list[blk->next_blk_num[i]];

			/* Skip output disabled branch */
			if (n->output_disable_flag)
				continue;
			if ((mode & n->run_mode) != mode)
				continue;

			v_end_flag &= n->v_end_flag;
			crop_v_end_flag &= n->crop_v_end_flag;
			/* Tdr_v_disable_flag changed during back cal by sub-in */
			if (n->tdr_v_disable_flag)
				continue;

			max_v_edge_flag &= n->max_v_edge_flag;

			if (max_h && n->in_tile_height_max) {
				if (max_h > n->in_tile_height_max)
					max_h = n->in_tile_height_max;
			} else if (n->in_tile_height_max) {
				max_h = n->in_tile_height_max;
			}

			if (max_s && n->in_tile_height_max_str) {
				if (max_s > n->in_tile_height_max_str)
					max_s = n->in_tile_height_max_str;
			} else if (n->in_tile_height_max_str) {
				max_s = n->in_tile_height_max_str;
			}

			if (max_e && n->in_tile_height_max_end) {
				if (max_e > n->in_tile_height_max_end)
					max_e = n->in_tile_height_max_end;
			} else if (n->in_tile_height_max_end) {
				max_e = n->in_tile_height_max_end;
			}

			if (mode != TILE_RUN_MODE_MAIN) {
				/* Sub-in with multi-out */
				if (count) {
					if (out_pos_ys != n->in_pos_ys ||
					    out_pos_ye != n->in_pos_ye)
						return T_DIFF_NEXT_CONFIG_ERROR;
				} else {
					out_pos_ys = n->in_pos_ys;
					out_pos_ye = n->in_pos_ye;
				}
				count++;
				continue;
			}

			/* Select min pos */
			if (out_pos_ys > n->in_pos_ys)
				out_pos_ys = n->in_pos_ys;

			/* Last full range */
			if (last_full_range_flag) {
				/* Full range input */
				if (n->in_pos_ye >= n->max_in_pos_ye) {
					/* Select max ye pos */
					if (out_pos_ye < n->in_pos_ye)
						out_pos_ye = n->in_pos_ye;
				} else {
					/* Non-full range input */
					out_pos_ye = n->in_pos_ye;
					last_full_range_flag = false;
				}
			} else if (n->in_pos_ye < n->max_in_pos_ye) {
				/* Non-full range input */
				if (out_pos_ye > n->in_pos_ye)
					out_pos_ye = n->in_pos_ye;
			}
			count++;
		}

		/* Update v_end_flag */
		blk->v_end_flag = v_end_flag;
		blk->crop_v_end_flag = crop_v_end_flag;

		if (count) {
			if (run_cal)
				if (blk->min_tile_out_pos_ye > out_pos_ye)
					out_pos_ye = blk->min_tile_out_pos_ye;

			blk->out_pos_ys = out_pos_ys;
			blk->out_pos_ye = out_pos_ye;
			blk->max_v_edge_flag = max_v_edge_flag;
			/* Update tdr_v_disable_flag changed during back cal by sub-in */
			blk->tdr_v_disable_flag = false;
			blk->out_tile_height_max = max_h;
			blk->out_tile_height_max_str = max_s;
			blk->out_tile_height_max_end = max_e;
		} else {
			blk->tdr_v_disable_flag = true;
		}
	}

	if (blk->tdr_v_disable_flag)
		goto err_return;

	if ((blk->out_pos_ye + 1 >= blk->full_size_y_out) &&
	    blk->out_tile_height_max_end) {
		bound = blk->out_tile_height_max_end + blk->out_pos_ys;
		if (blk->full_size_y_out > bound) {
			blk->out_pos_ye = blk->full_size_y_out - 1 - out_const_y;
			blk->max_v_edge_flag = false;
			blk->crop_v_end_flag = false;
			blk->v_end_flag = false;
		}
	}

	/* Top edge */
	if (blk->out_pos_ys <= 0 &&
	    blk->out_tile_height_max_str) {
		if (blk->out_tile_height_max_str < blk->out_pos_ye + 1) {
			blk->out_pos_ye = blk->out_tile_height_max_str - 1;
			blk->max_v_edge_flag = false;
			blk->crop_v_end_flag = false;
			blk->v_end_flag = false;

			if (out_const_y > 1 && val_e)
				blk->out_pos_ye -= val_e;
		}
	}

	/* Check over tile size */
	if (blk->out_tile_height || blk->out_tile_height_max) {
		max_h = blk->out_tile_height;
		if (max_h && blk->out_tile_height_max) {
			if (blk->out_tile_height > blk->out_tile_height_max)
				max_h = blk->out_tile_height_max;
		} else {
			max_h = blk->out_tile_height_max;
		}

		if (blk->out_pos_ye + 1 > blk->out_pos_ys + max_h) {
			blk->max_v_edge_flag = false;
			blk->crop_v_end_flag = false;
		}

		/* Tile size constraint check */
		if (run_cal && blk->out_max_height) {
			if (blk->out_max_height < max_h) {
				bound = blk->out_pos_ys + blk->out_max_height;
				if (blk->min_tile_out_pos_ye + 1 > bound)
					max_h = blk->min_tile_out_pos_ye - blk->out_pos_ys + 1;
				else
					max_h = blk->out_max_height;
			}
		}
		if (blk->out_pos_ye + 1 > blk->out_pos_ys + max_h) {
			blk->out_pos_ye = blk->out_pos_ys + max_h - 1;
			/* Only update v_end_flag */
			blk->v_end_flag = false;
			if (out_const_y > 1 && val_e)
				blk->out_pos_ye -= val_e; /* Decreae ye */
		}
	} else if (blk->out_max_height) {
		if (run_cal) {
			max_h = blk->out_max_height;
			bound = blk->out_pos_ys + blk->out_max_height;

			if (blk->min_tile_out_pos_ye + 1 < bound) {
				if (blk->out_pos_ye + 1 > blk->out_pos_ys + max_h) {
					blk->out_pos_ye = blk->out_pos_ys + max_h - 1;
					/* Only update v_end_flag */
					blk->v_end_flag = false;
					if (out_const_y > 1 && val_e)
						blk->out_pos_ye -= val_e;
				}
			}
		}
	}

	return tile_check_output_config_y(blk, reg_map);

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_bwd_output_config(struct tile_func_block *blk,
						struct tile_reg_map *reg_map,
						struct func_description *func_param)
{
	enum mdp_tile_msg result = T_OK;

	/* Check cal order, output */
	if (blk->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
		result = tile_bwd_output_config_x_inv(blk, reg_map, func_param);
	else
		result = tile_bwd_output_config_x(blk, reg_map, func_param);

	if (result)
		return result;

	return tile_bwd_output_config_y(blk, reg_map, func_param);
}

static enum mdp_tile_msg tile_bwd_min_tile_backup_input(struct tile_func_block *blk,
							struct tile_reg_map *reg_map)
{
	if (!reg_map->skip_x_cal) {
		blk->min_cal_tdr_h_disable_flag = blk->tdr_h_disable_flag;
		blk->min_cal_h_end_flag = blk->h_end_flag;

		if (!blk->tdr_h_disable_flag) {
			blk->min_tile_in_pos_xs = blk->in_pos_xs;
			blk->min_tile_in_pos_xe = blk->in_pos_xe;
			blk->min_tile_out_pos_xs = blk->out_pos_xs;
			blk->min_tile_out_pos_xe = blk->out_pos_xe;
			blk->min_cal_max_h_edge_flag = blk->max_h_edge_flag;
		}
	}

	if (!reg_map->skip_y_cal) {
		blk->min_cal_tdr_v_disable_flag = blk->tdr_v_disable_flag;
		blk->min_cal_v_end_flag = blk->v_end_flag;

		if (!blk->tdr_v_disable_flag) {
			blk->min_tile_in_pos_ys = blk->in_pos_ys;
			blk->min_tile_in_pos_ye = blk->in_pos_ye;
			blk->min_tile_out_pos_ys = blk->out_pos_ys;
			blk->min_tile_out_pos_ye = blk->out_pos_ye;
			blk->min_cal_max_v_edge_flag = blk->max_v_edge_flag;
		}
	}

	return T_OK;
}

static enum mdp_tile_msg tile_bwd_min_tile_init(struct tile_func_block *blk,
						struct tile_reg_map *reg_map)
{
	/* To set no tile size constraint for correct min tile backward */
	blk->in_tile_width = 0;
	blk->out_tile_width = 0;
	blk->in_tile_height = 0;
	blk->out_tile_height = 0;

	return T_OK;
}

static enum mdp_tile_msg tile_bwd_min_tile_restore(struct tile_func_block *blk,
						   struct tile_reg_map *reg_map)
{
	/* To restore tile size constraint for normal tile backward */
	blk->in_tile_width = blk->in_tile_width_backup;
	blk->out_tile_width = blk->out_tile_width_backup;
	blk->in_tile_height = blk->in_tile_height_backup;
	blk->out_tile_height = blk->out_tile_height_backup;

	return T_OK;
}

static enum mdp_tile_msg tile_bwd_output_config_min_tile(struct tile_func_block *blk,
							 struct tile_reg_map *reg_map,
							 struct func_description *func_param)
{
	enum mdp_tile_msg result = T_OK;

	/* Check cal order, output */
	if (blk->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
		result = tile_bwd_output_config_x_inv_min_tile(blk, reg_map, func_param);
	else
		result = tile_bwd_output_config_x_min_tile(blk, reg_map, func_param);

	if (result == T_OK)
		result = tile_bwd_output_config_y_min_tile(blk, reg_map, func_param);

	return result;
}

static enum mdp_tile_msg tile_bwd_output_config_x_min_tile(struct tile_func_block *blk,
							   struct tile_reg_map *reg_map,
							   struct func_description *func_param)
{
	enum mdp_tile_msg result = T_OK;
	struct tile_func_block *next;
	int out_pos_xs = MAX_SIZE;/* Select min xs pos */
	int out_pos_xe = 0;/* Select max xe pos */
	bool max_h_edge_flag = true;
	bool h_end_flag = true;
	int count = 0;
	int mode = reg_map->run_mode;
	int i;

	if (reg_map->skip_x_cal)
		goto err_return;

	/* Tile movement */
	if (blk->tot_branch_num == 0) {/* End module only */
		/* No direct link */
		if (!(blk->type & TILE_TYPE_DONT_CARE_END)) {
			/* Init output size of final module */
			if (blk->valid_h_no == 0) {
				/* First tile */
				blk->out_pos_xs = blk->min_out_pos_xs;
				blk->out_pos_xe = blk->min_out_pos_xs +
						  blk->out_const_x - 1;
				blk->tdr_h_disable_flag = false;
			} else {
				/* Move right to set output pos */
				if (blk->last_output_xe_pos < blk->max_out_pos_xe) {
					blk->out_pos_xs = blk->last_output_xe_pos + 1;
					blk->out_pos_xe = blk->last_output_xe_pos +
							  blk->out_const_x;
					blk->tdr_h_disable_flag = false;
				} else {
					/* End of tile */
					blk->tdr_h_disable_flag = true;
				}
			}
			/* Check size equal to full size */
			if (!blk->tdr_h_disable_flag) {
				/* Not updated by back direct-link */
				if (blk->out_pos_xe >= blk->max_out_pos_xe) {
					blk->out_pos_xe = blk->max_out_pos_xe;
					blk->max_h_edge_flag = true;
					blk->h_end_flag = true;
				} else {
					int bound = blk->out_pos_xs + blk->out_tile_width;

					blk->h_end_flag = false;
					if (blk->out_tile_width &&
					    (blk->max_out_pos_xe + 1 > bound))
						blk->max_h_edge_flag = false;
					else
						blk->max_h_edge_flag = true;
				}
			} else {
				/* End of tile */
				blk->h_end_flag = true;
			}
		} else {/* Direct link */
			if (!blk->tdr_h_disable_flag) {
				/* Copy from min tile config */
				blk->out_pos_xs = blk->min_tile_out_pos_xs;
				blk->out_pos_xe = blk->min_tile_out_pos_xe;
			} else {
				/* Update for backward because of skipping normal back config */
				blk->h_end_flag = blk->direct_h_end_flag;
			}
		}
	} else if (blk->tot_branch_num == 1) {/* Check non-branch */
		/* Set curr out with next in */
		next = func_param->func_list[blk->next_blk_num[0]];
		/* Necessary info for backward */
		blk->tdr_h_disable_flag = next->tdr_h_disable_flag;
		blk->h_end_flag = next->h_end_flag;
		if (!next->tdr_h_disable_flag) {
			blk->out_pos_xs = next->in_pos_xs;
			blk->out_pos_xe = next->in_pos_xe;
			blk->max_h_edge_flag = next->max_h_edge_flag;
		}
	} else {/* Branch */
		/* Min xs/ys & min xe/ye sorting for current support tile mode */
		count = 0;

		for (i = 0; i < blk->tot_branch_num; i++) {
			next = func_param->func_list[blk->next_blk_num[i]];

			/* Skip output & tdr disabled branch */
			if (next->output_disable_flag)
				continue;
			if ((mode & next->run_mode) != mode)
				continue;

			/* Update h_end_flag for backward */
			h_end_flag &= next->h_end_flag;
			/* Skip tdr disabled */
			if (next->tdr_h_disable_flag)
				continue;

			max_h_edge_flag &= next->max_h_edge_flag;
			/* Min tile select min xs pos */
			if (out_pos_xs == next->in_pos_xs) {
				/* Min tile select max xe pos */
				if (out_pos_xe < next->in_pos_xe)
					out_pos_xe = next->in_pos_xe;
			} else if (out_pos_xs > next->in_pos_xs) {
				out_pos_xs = next->in_pos_xs;
				out_pos_xe = next->in_pos_xe;
			}
			count++;
		}

		/* Update h_end_flag */
		blk->h_end_flag = h_end_flag;

		/* No branches with tdr false */
		if (count) {
			blk->out_pos_xs = out_pos_xs;
			blk->out_pos_xe = out_pos_xe;
			blk->max_h_edge_flag = max_h_edge_flag;
			blk->tdr_h_disable_flag = false;
		} else {
			blk->tdr_h_disable_flag = true;
		}
	}
	if (!blk->tdr_h_disable_flag)
		result = tile_check_output_config_x(blk, reg_map);

	/* Direct link update */
	blk->min_tile_out_pos_xs = blk->out_pos_xs;
	blk->min_tile_out_pos_xe = blk->out_pos_xe;
	blk->crop_h_end_flag = blk->h_end_flag;
	blk->out_tile_width_max = 0;
	blk->out_tile_width_max_str = 0;
	blk->out_tile_width_max_end = 0;

err_return:
	return result;
}

static enum mdp_tile_msg tile_bwd_output_config_x_inv_min_tile(struct tile_func_block *blk,
							       struct tile_reg_map *reg_map,
							       struct func_description *func_param)
{
	enum mdp_tile_msg result = T_OK;
	struct tile_func_block *next;
	int out_pos_xs = MAX_SIZE; /* Select min xs pos */
	int out_pos_xe = 0; /* Select max xe pos */
	bool max_h_edge_flag = true;
	bool h_end_flag = true;
	int count;
	int mode = reg_map->run_mode;
	int i;

	if (reg_map->skip_x_cal)
		goto err_return;

	/* Tile movement */
	if (blk->tot_branch_num == 0) { /* End module only */
		/* No direct link */
		if (!(blk->type & TILE_TYPE_DONT_CARE_END)) {
			/* Init output size of final module */
			if (blk->valid_h_no == 0) { /* First tile */
				/* Top start */
				blk->out_pos_xs = blk->max_out_pos_xe -
						  blk->out_const_x + 1;
				blk->out_pos_xe = blk->max_out_pos_xe;
				blk->tdr_h_disable_flag = false;
			} else {
				/* Move left to set output pos */
				if (blk->last_output_xs_pos > blk->min_out_pos_xs) {
					blk->out_pos_xs = blk->last_output_xs_pos -
							  blk->out_const_x;
					blk->out_pos_xe = blk->last_output_xs_pos - 1;
					blk->tdr_h_disable_flag = false;
				} else {
					/* End of tile */
					blk->tdr_h_disable_flag = true;
				}
			}

			/* Check size equal to full size */
			if (!blk->tdr_h_disable_flag) {
				/* Not updated by back direct-link */
				if (blk->out_pos_xs <= blk->min_out_pos_xs) {
					blk->out_pos_xs = blk->min_out_pos_xs;
					blk->max_h_edge_flag = true;
					blk->h_end_flag = true;
				} else {
					int bnd = blk->min_out_pos_xs + blk->out_tile_width;

					blk->h_end_flag = false;
					if (blk->out_tile_width &&
					    (blk->out_pos_xe + 1 > bnd))
						blk->max_h_edge_flag = false;
					else
						blk->max_h_edge_flag = true;
				}
			} else {
				/* End of tile */
				blk->h_end_flag = true;
			}
		} else { /* Direct link */
			if (!blk->tdr_h_disable_flag) {
				/* Copy from min tile config */
				blk->out_pos_xs = blk->min_tile_out_pos_xs;
				blk->out_pos_xe = blk->min_tile_out_pos_xe;
			} else {
				/* Update for backward because of skipping normal back config */
				blk->h_end_flag = blk->direct_h_end_flag;
			}
		}
	} else if (blk->tot_branch_num == 1) { /* Check non-branch */
		/* Set curr out with next in */
		next = func_param->func_list[blk->next_blk_num[0]];

		/* Update for backward */
		blk->tdr_h_disable_flag = next->tdr_h_disable_flag;
		blk->h_end_flag = next->h_end_flag;
		/* Backward h_end_flag */
		if (!next->tdr_h_disable_flag) {
			blk->out_pos_xs = next->in_pos_xs;
			blk->out_pos_xe = next->in_pos_xe;
			blk->max_h_edge_flag = next->max_h_edge_flag;
		}
	} else { /* Branch */
		/* Min xs/ys & min xe/ye sorting for current support tile mode */
		count = 0;

		for (i = 0; i < blk->tot_branch_num; i++) {
			next = func_param->func_list[blk->next_blk_num[i]];

			/* Skip output & tdr disabled branch */
			if (next->output_disable_flag)
				continue;
			if ((mode & next->run_mode) == mode)
				continue;

			/* Update h_end_flag for backward */
			h_end_flag &= next->h_end_flag;
			/* Skip tdr disabled */
			if (next->tdr_h_disable_flag)
				continue;

			max_h_edge_flag &= next->max_h_edge_flag;
			/* Min tile select max xe pos */
			if (out_pos_xe == next->in_pos_xe) {
				/* Min tile select min xs pos */
				if (out_pos_xs > next->in_pos_xs)
					out_pos_xs = next->in_pos_xs;
			} else if (out_pos_xe < next->in_pos_xe) {
				out_pos_xe = next->in_pos_xe;
				out_pos_xs = next->in_pos_xs;
			}
			count++;
		}

		/* Update h_end_flag */
		blk->h_end_flag = h_end_flag;

		/* No branches with tdr false */
		if (count) {
			blk->out_pos_xs = out_pos_xs;
			blk->out_pos_xe = out_pos_xe;
			blk->max_h_edge_flag = max_h_edge_flag;
			blk->tdr_h_disable_flag = false;
		} else {
			blk->tdr_h_disable_flag = true;
		}
	}

	if (!blk->tdr_h_disable_flag)
		result = tile_check_output_config_x_inv(blk, reg_map);

	/* Direct link update */
	blk->min_tile_out_pos_xs = blk->out_pos_xs;
	blk->min_tile_out_pos_xe = blk->out_pos_xe;
	blk->crop_h_end_flag = blk->h_end_flag;
	blk->out_tile_width_max = 0;
	blk->out_tile_width_max_str = 0;
	blk->out_tile_width_max_end = 0;

err_return:
	return result;
}

static enum mdp_tile_msg tile_bwd_output_config_y_min_tile(struct tile_func_block *blk,
							   struct tile_reg_map *reg_map,
							   struct func_description *func_param)
{
	enum mdp_tile_msg result = T_OK;
	struct tile_func_block *next;
	int out_pos_ys = MAX_SIZE; /* Select min start pos */
	int out_pos_ye = 0; /* Select max end pos */
	bool max_v_edge_flag = true;
	bool v_end_flag = true;
	int count;
	int mode = reg_map->run_mode;
	int i;

	if (reg_map->skip_y_cal)
		goto err_return;

	/* Tile movement */
	if (blk->tot_branch_num == 0) { /* End module only */
		if (!(blk->type & TILE_TYPE_DONT_CARE_END)) {
			/* Init output size of final module */
			if (blk->valid_v_no == 0) {
				/* First tile row */
				blk->out_pos_ys = blk->min_out_pos_ys;
				blk->out_pos_ye = blk->min_out_pos_ys +
						  blk->out_const_y - 1;
				blk->tdr_v_disable_flag = false;
			} else { /* Middle row */
				/* Next left start to set output pos */
				if (blk->last_output_ye_pos < blk->max_out_pos_ye) {
					blk->out_pos_ys = blk->last_output_ye_pos + 1;
					blk->out_pos_ye = blk->last_output_ye_pos +
							  blk->out_const_y;
					blk->tdr_v_disable_flag = false;
				} else {
					/* Last tile is end */
					blk->tdr_v_disable_flag = true;
				}
			}

			/* Not updated by back direct-link */
			if (!blk->tdr_v_disable_flag) {
				if (blk->out_pos_ye >= blk->max_out_pos_ye) {
					blk->out_pos_ye = blk->max_out_pos_ye;
					blk->max_v_edge_flag = true;
					blk->v_end_flag = true;
				} else {
					int bnd = blk->out_pos_ys + blk->out_tile_height;

					blk->v_end_flag = false;
					if (blk->out_tile_height &&
					    (blk->max_out_pos_ye + 1 > bnd))
						blk->max_v_edge_flag = false;
					else
						blk->max_v_edge_flag = true;
				}
			} else {
				/* Update end flag for normal back */
				blk->v_end_flag = true;
			}
		} else {
			if (!blk->tdr_v_disable_flag) {
				/* Copy from min tile config */
				blk->out_pos_ys = blk->min_tile_out_pos_ys;
				blk->out_pos_ye = blk->min_tile_out_pos_ye;
			} else {
				/* Update for backward because of skipping normal back config */
				blk->v_end_flag = blk->direct_v_end_flag;
			}
		}
	} else if (blk->tot_branch_num == 1) { /* Check non-branch */
		/* Set curr out with next in */
		next = func_param->func_list[blk->next_blk_num[0]];

		/* Update for backward */
		blk->tdr_v_disable_flag = next->tdr_v_disable_flag;
		blk->v_end_flag = next->v_end_flag;
		if (!next->tdr_v_disable_flag) {
			blk->out_pos_ys = next->in_pos_ys;
			blk->out_pos_ye = next->in_pos_ye;
			blk->max_v_edge_flag = next->max_v_edge_flag;
		}
	} else { /* Branch */
		/* Min xs/ys & min xe/ye sorting for current support tile mode */
		count = 0;

		for (i = 0; i < blk->tot_branch_num; i++) {
			next = func_param->func_list[blk->next_blk_num[i]];

			/* Skip output disabled branch */
			if (next->output_disable_flag)
				continue;
			if ((mode & next->run_mode) != mode)
				continue;

			/* Update v_end_flag */
			v_end_flag &= next->v_end_flag;

			/* Skip when tdr disabled */
			if (next->tdr_v_disable_flag)
				continue;

			max_v_edge_flag &= next->max_v_edge_flag;
			/* Min tile select min ys pos */
			if (out_pos_ys == next->in_pos_ys) {
				/* Min tile select max ye pos */
				if (out_pos_ye < next->in_pos_ye)
					out_pos_ye = next->in_pos_ye;
			} else if (out_pos_ys > next->in_pos_ys) {
				out_pos_ys = next->in_pos_ys;
				out_pos_ye = next->in_pos_ye;
			}
			count++;
		}

		/* Update v_end_flag */
		blk->v_end_flag = v_end_flag;

		/* No branches with tdr false */
		if (count) {
			blk->out_pos_ys = out_pos_ys;
			blk->out_pos_ye = out_pos_ye;
			blk->max_v_edge_flag = max_v_edge_flag;
			blk->tdr_v_disable_flag = false;
		} else {
			blk->tdr_v_disable_flag = true;
		}
	}

	if (!blk->tdr_v_disable_flag)
		result = tile_check_output_config_y(blk, reg_map);

	/* Direct link update */
	blk->min_tile_out_pos_ys = blk->out_pos_ys;
	blk->min_tile_out_pos_ye = blk->out_pos_ye;
	blk->crop_v_end_flag = blk->v_end_flag;
	blk->out_tile_height_max = 0;
	blk->out_tile_height_max_str = 0;
	blk->out_tile_height_max_end = 0;

err_return:
	return result;
}

static enum mdp_tile_msg tile_bwd_input_check(struct tile_func_block *blk,
					      struct tile_reg_map *reg_map)
{
	if (reg_map->skip_x_cal)
		goto err_skip_x_cal;

	if (!blk->tdr_h_disable_flag) {
		/*
		 * Check resizer input xe & ye by tile size with
		 * enable & skip buffer check false
		 */
		if (blk->in_tile_width)
			if (blk->in_pos_xe + 1 > blk->in_pos_xs + blk->in_tile_width)
				return T_TILE_BACKWARD_IN_OVER_TILE_WIDTH_ERROR;

		/* Check tile edge flag by backward */
		if (!blk->in_pos_xs) {
			if ((blk->tdr_edge & TILE_EDGE_LEFT_MASK) != TILE_EDGE_LEFT_MASK)
				return T_BWD_CHECK_LEFT_EDGE_ERROR;
		} else {
			if (blk->tdr_edge & TILE_EDGE_LEFT_MASK)
				return T_BWD_CHECK_LEFT_EDGE_ERROR;
		}

		if (blk->in_pos_xe + 1 >= blk->full_size_x_in) {
			if (TILE_EDGE_RIGHT_MASK != (blk->tdr_edge & TILE_EDGE_RIGHT_MASK))
				return T_BWD_CHECK_RIGHT_EDGE_ERROR;
		} else {
			if (blk->tdr_edge & TILE_EDGE_RIGHT_MASK)
				return T_BWD_CHECK_RIGHT_EDGE_ERROR;
		}
	} else if (blk->run_mode == TILE_RUN_MODE_MAIN) {
		if (blk->prev_blk_num[0] == PREVIOUS_BLK_NO_OF_START)
			return T_DIFF_VIEW_TILE_WIDTH_ERROR;
	}

err_skip_x_cal:
	if (reg_map->skip_y_cal)
		goto err_skip_y_cal;

	if (!blk->tdr_v_disable_flag) {
		/*
		 * Check resizer input xe & ye by tile size with
		 * enable & skip buffer check false
		 */
		if (blk->in_tile_height)
			if (blk->in_pos_ye + 1 > blk->in_pos_ys + blk->in_tile_height)
				return T_TILE_BACKWARD_IN_OVER_TILE_HEIGHT_ERROR;

		/* Check tile edge flag by backward */
		if (!blk->in_pos_ys) {
			if ((blk->tdr_edge & TILE_EDGE_TOP_MASK) != TILE_EDGE_TOP_MASK)
				return T_BWD_CHECK_TOP_EDGE_ERROR;
		} else {
			if (blk->tdr_edge & TILE_EDGE_TOP_MASK)
				return T_BWD_CHECK_TOP_EDGE_ERROR;
		}

		if (blk->in_pos_ye + 1 >= blk->full_size_y_in) {
			if ((blk->tdr_edge & TILE_EDGE_BOTTOM_MASK) != TILE_EDGE_BOTTOM_MASK)
				return T_BWD_CHECK_BOTTOM_EDGE_ERROR;
		} else {
			if (blk->tdr_edge & TILE_EDGE_BOTTOM_MASK)
				return T_BWD_CHECK_BOTTOM_EDGE_ERROR;
		}
	} else if (blk->run_mode == TILE_RUN_MODE_MAIN) {
		if (blk->prev_blk_num[0] == PREVIOUS_BLK_NO_OF_START)
			return T_DIFF_VIEW_TILE_HEIGHT_ERROR;
	}

err_skip_y_cal:
	return tile_check_input_config(blk, reg_map);
}

static enum mdp_tile_msg tile_bwd_by_func_pre_x(struct tile_func_block *blk,
						struct tile_reg_map *reg_map)
{
	int in_const_x = blk->in_const_x;

	if (reg_map->skip_x_cal)
		goto err_return;

	if (blk->tdr_h_disable_flag)
		goto err_return;

	/* Update output right edge */
	if (blk->out_pos_xe + 1 >= blk->full_size_x_out) {
		blk->tdr_edge |= TILE_EDGE_RIGHT_MASK;
		blk->out_pos_xe = blk->full_size_x_out - 1;
	} else {
		blk->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
	}

	/* Update output left edge */
	if (blk->out_pos_xs <= 0) {
		blk->tdr_edge |= TILE_EDGE_LEFT_MASK;
		blk->out_pos_xs = 0;
	} else {
		blk->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
	}

	/* Set fixed left tile loss */
	if (blk->tdr_edge & TILE_EDGE_LEFT_MASK) {
		blk->in_pos_xs = blk->out_pos_xs;
	} else {
		if (blk->enable_flag)
			blk->in_pos_xs = blk->out_pos_xs - blk->l_tile_loss;
		else
			blk->in_pos_xs = blk->out_pos_xs;
	}

	/* Set fixed right tile loss */
	if (blk->tdr_edge & TILE_EDGE_RIGHT_MASK) {
		blk->in_pos_xe = blk->out_pos_xe;
	} else {
		if (blk->enable_flag)
			blk->in_pos_xe = blk->out_pos_xe + blk->r_tile_loss;
		else
			blk->in_pos_xe = blk->out_pos_xe;
	}

	/* Clip size */
	if (blk->in_pos_xs < 0)
		blk->in_pos_xs = 0;

	if (blk->in_pos_xe + 1 > blk->full_size_x_in)
		blk->in_pos_xe = blk->full_size_x_in - 1;

	/* Align input position */
	if (in_const_x > 1) {
		int val_s = TILE_MOD(blk->in_pos_xs, in_const_x);
		int val_e = TILE_MOD(blk->in_pos_xe + 1, in_const_x);

		if (val_s)
			blk->in_pos_xs -= val_s;

		if (val_e)
			blk->in_pos_xe += in_const_x - val_e;
	}

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_bwd_by_func_pre_y(struct tile_func_block *blk,
						struct tile_reg_map *reg_map)
{
	int in_const_y = blk->in_const_y;

	if (reg_map->skip_y_cal)
		goto err_return;

	if (blk->tdr_v_disable_flag)
		goto err_return;

	/* Update output bottom edge */
	if (blk->out_pos_ye + 1 >= blk->full_size_y_out) {
		blk->tdr_edge |= TILE_EDGE_BOTTOM_MASK;
		blk->out_pos_ye = blk->full_size_y_out - 1;
	} else {
		blk->tdr_edge &= ~TILE_EDGE_BOTTOM_MASK;
	}

	/* Update output top edge */
	if (blk->out_pos_ys <= 0) {
		blk->tdr_edge |= TILE_EDGE_TOP_MASK;
		blk->out_pos_ys = 0;
	} else {
		blk->tdr_edge &= ~TILE_EDGE_TOP_MASK;
	}

	/* Set fixed top tile loss */
	if (blk->tdr_edge & TILE_EDGE_TOP_MASK) {
		blk->in_pos_ys = blk->out_pos_ys;
	} else {
		if (!blk->enable_flag)
			blk->in_pos_ys = blk->out_pos_ys - blk->t_tile_loss;
		else
			blk->in_pos_ys = blk->out_pos_ys;
	}

	/* Set fixed bottom tile loss */
	if (blk->tdr_edge & TILE_EDGE_BOTTOM_MASK) {
		blk->in_pos_ye = blk->out_pos_ye;
	} else {
		if (!blk->enable_flag)
			blk->in_pos_ye = blk->out_pos_ye + blk->b_tile_loss;
		else
			blk->in_pos_ye = blk->out_pos_ye;
	}
	/* Clip size */
	if (blk->in_pos_ys < 0)
		blk->in_pos_ys = 0;

	if (blk->in_pos_ye + 1 > blk->full_size_y_in)
		blk->in_pos_ye = blk->full_size_y_in - 1;

	/* Align input position */
	if (in_const_y > 1) {
		int val_s = TILE_MOD(blk->in_pos_ys, in_const_y);
		int val_e = TILE_MOD(blk->in_pos_ye + 1, in_const_y);

		if (val_s)
			blk->in_pos_ys -= val_s;

		if (val_e)
			blk->in_pos_ye += in_const_y - val_e;
	}

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_bwd_by_func_post_x(struct tile_func_block *blk,
						 struct tile_reg_map *reg_map)
{
	int in_const_x = blk->in_const_x;
	int min_xsize = blk->in_min_width;
	int max_in_pos_xe;
	int val_s = TILE_MOD(blk->in_pos_xs, in_const_x);
	int val_e = TILE_MOD(blk->in_pos_xe + 1, in_const_x);

	if (reg_map->skip_x_cal)
		goto err_return;

	if (blk->tdr_h_disable_flag) {
		/* Backup flag */
		blk->backward_tdr_h_disable_flag = blk->tdr_h_disable_flag;
		blk->backward_h_end_flag = blk->h_end_flag;
		goto err_return;
	}

	max_in_pos_xe = reg_map->first_frame ?
			(blk->full_size_x_in - 1) : blk->max_in_pos_xe;

	if (blk->enable_flag) {
		/* Check min x size */
		if (blk->in_pos_xs + blk->in_min_width > blk->in_pos_xe + 1) {
			blk->in_pos_xe = blk->in_pos_xs + min_xsize - 1;

			if (blk->in_pos_xe >= max_in_pos_xe) {
				if (max_in_pos_xe + 1 >= blk->full_size_x_in) {
					if (blk->max_h_edge_flag) {
						blk->in_pos_xe = max_in_pos_xe;
						blk->in_pos_xs = max_in_pos_xe + 1 -
								 min_xsize;
					} else {
						blk->in_pos_xe = max_in_pos_xe - in_const_x;
						blk->in_pos_xs = max_in_pos_xe + 1 -
								 min_xsize - in_const_x;
					}
				} else {
					blk->in_pos_xe = max_in_pos_xe;
					blk->in_pos_xs = max_in_pos_xe + 1 - min_xsize;
				}

				/* Align xs position */
				if (in_const_x > 1 && val_s)
					blk->in_pos_xs -= val_s;
			}

			/* Align xe position */
			if (in_const_x > 1 && val_e)
				blk->in_pos_xe += in_const_x - val_e;
		}
		/* Check crop & update in_tile_width_max */
		if (!(blk->type & TILE_TYPE_CROP_EN) || (blk->type & TILE_TYPE_LOSS)) {
			/* In_tile_width_max */
			if (blk->in_tile_width && blk->out_tile_width_max) {
				if (blk->in_tile_width >
				    blk->out_tile_width_max + blk->l_tile_loss + blk->r_tile_loss)
					blk->in_tile_width_max = blk->out_tile_width_max +
								 blk->l_tile_loss +
								 blk->r_tile_loss;
				else
					blk->in_tile_width_max = blk->in_tile_width;
			} else if (blk->out_tile_width_max) {
				blk->in_tile_width_max = blk->out_tile_width_max +
							 blk->l_tile_loss + blk->r_tile_loss;
			} else {
				blk->in_tile_width_max = blk->in_tile_width;
			}

			/* In_tile_width_max_str */
			if (blk->in_tile_width && blk->out_tile_width_max_str) {
				if (blk->in_tile_width >
				    blk->out_tile_width_max_str + blk->r_tile_loss)
					blk->in_tile_width_max_str = blk->out_tile_width_max_str +
								     blk->r_tile_loss;
				else
					blk->in_tile_width_max_str = blk->in_tile_width;
			} else if (blk->out_tile_width_max_str) {
				blk->in_tile_width_max_str = blk->out_tile_width_max_str +
							     blk->r_tile_loss;
			} else {
				blk->in_tile_width_max_str = blk->in_tile_width;
			}

			/* Prevent from min edge error */
			if (blk->out_tile_width_max_str &&
			    blk->out_tile_width_max_str < blk->full_size_x_out &&
			    blk->in_tile_width_max_str >= blk->full_size_x_in)
				blk->in_tile_width_max_str = blk->full_size_x_in - in_const_x;

			/* In_tile_width_max_end */
			if (blk->in_tile_width && blk->out_tile_width_max_end) {
				if (blk->in_tile_width >
				    blk->out_tile_width_max_end + blk->l_tile_loss)
					blk->in_tile_width_max_end = blk->out_tile_width_max_end +
								     blk->l_tile_loss;
				else
					blk->in_tile_width_max_end = blk->in_tile_width;
			} else if (blk->out_tile_width_max_end) {
				blk->in_tile_width_max_end = blk->out_tile_width_max_end +
							     blk->l_tile_loss;
			} else {
				blk->in_tile_width_max_end = blk->in_tile_width;
			}

			/* Smart tile + ufd */
			blk->in_tile_width_loss = blk->out_tile_width_loss +
						  blk->l_tile_loss + blk->r_tile_loss;

			if (blk->max_out_crop_xe + blk->r_tile_loss < blk->max_in_pos_xe)
				blk->max_in_crop_xe = blk->max_out_crop_xe + blk->r_tile_loss;
			else
				blk->max_in_crop_xe = blk->max_in_pos_xe;

			if (blk->min_tile_crop_out_pos_xe + blk->r_tile_loss <
			    blk->max_in_pos_xe)
				blk->min_tile_crop_in_pos_xe = blk->min_tile_crop_out_pos_xe +
							       blk->r_tile_loss;
			else
				blk->min_tile_crop_in_pos_xe = blk->max_in_pos_xe;
		} else {
			blk->in_tile_width_max = blk->in_tile_width;
			blk->in_tile_width_max_str = blk->in_tile_width;
			blk->in_tile_width_max_end = blk->in_tile_width;
			/* Smart tile + ufd */
			blk->in_tile_width_loss = 0;
			blk->max_in_crop_xe = blk->max_in_pos_xe;
			blk->min_tile_crop_in_pos_xe = blk->min_tile_in_pos_xe;
		}
	} else {
		/* In_tile_width_max */
		if (blk->in_tile_width && blk->out_tile_width_max) {
			if (blk->in_tile_width > blk->out_tile_width_max)
				blk->in_tile_width_max = blk->out_tile_width_max;
			else
				blk->in_tile_width_max = blk->in_tile_width;
		} else if (blk->out_tile_width_max) {
			blk->in_tile_width_max = blk->out_tile_width_max;
		} else {
			blk->in_tile_width_max = blk->in_tile_width;
		}

		/* In_tile_width_max_str */
		if (blk->in_tile_width && blk->out_tile_width_max_str) {
			if (blk->in_tile_width > blk->out_tile_width_max_str)
				blk->in_tile_width_max_str = blk->out_tile_width_max_str;
			else
				blk->in_tile_width_max_str = blk->in_tile_width;
		} else if (blk->out_tile_width_max_str) {
			blk->in_tile_width_max_str = blk->out_tile_width_max_str;
		} else {
			blk->in_tile_width_max_str = blk->in_tile_width;
		}

		/* In_tile_width_max_end */
		if (blk->in_tile_width && blk->out_tile_width_max_end) {
			if (blk->in_tile_width > blk->out_tile_width_max_end)
				blk->in_tile_width_max_end = blk->out_tile_width_max_end;
			else
				blk->in_tile_width_max_end = blk->in_tile_width;
		} else if (blk->out_tile_width_max_end) {
			blk->in_tile_width_max_end = blk->out_tile_width_max_end;
		} else {
			blk->in_tile_width_max_end = blk->in_tile_width;
		}

		/* Smart tile + ufd */
		blk->in_tile_width_loss = blk->out_tile_width_loss;
		blk->max_in_crop_xe = blk->max_out_crop_xe;
		blk->min_tile_crop_in_pos_xe = blk->min_tile_crop_out_pos_xe;
	}
	/* Check align mis-match */
	if (!(blk->type & TILE_TYPE_CROP_EN) ||
	    (blk->type & TILE_TYPE_LOSS) ||
	    !blk->enable_flag) {
		/* Right edge */
		if (blk->in_pos_xe + 1 >= blk->full_size_x_in &&
		    blk->in_tile_width_max_end)
			if (blk->in_tile_width_max_end + blk->in_pos_xs <
				blk->full_size_x_in)
				blk->in_pos_xe = blk->full_size_x_in - 1 - in_const_x;

		/* Intermeidate */
		if (blk->in_pos_xe + 1 < blk->full_size_x_in) {
			if (blk->in_tile_width_max) {
				if (blk->in_tile_width_max + blk->in_pos_xs <
				    blk->in_pos_xe + 1) {
					blk->in_pos_xe = blk->in_pos_xs +
							 blk->in_tile_width_max - 1;

					if (in_const_x > 1 && val_e)
						blk->in_pos_xe -= val_e;
				}
			}
		}

		/* Left edge */
		if (blk->in_pos_xs <= 0 &&
		    blk->in_tile_width_max_str) {
			if (blk->in_tile_width_max_str < blk->in_pos_xe + 1) {
				blk->in_pos_xe = blk->in_tile_width_max_str - 1;
				if (in_const_x > 1 && val_e)
					blk->in_pos_xe -= val_e;
			}
		}
	}

	/* Clip input size */
	if (blk->in_pos_xe + 1 > blk->full_size_x_in)
		blk->in_pos_xe = blk->full_size_x_in - 1;

	if (blk->in_pos_xs <= 0)
		blk->in_pos_xs = 0;

	/* Output don't touch left edge */
	if ((blk->tdr_edge & TILE_EDGE_LEFT_MASK) != TILE_EDGE_LEFT_MASK) {
		/* Input touch left edge */
		if (blk->in_pos_xs <= 0)
			blk->tdr_edge |= TILE_EDGE_LEFT_MASK;
	} else {
		/* Input don't touch left edge */
		if (blk->in_pos_xs > 0)
			if (blk->type & TILE_TYPE_CROP_EN)
				blk->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
	}

	/* Config max_h_edge_flag */
	if (blk->in_tile_width) {
		/* Bound in_pos_xe with in_tile_width and make it align */
		if (blk->in_pos_xe + 1 > blk->in_pos_xs + blk->in_tile_width) {
			blk->in_pos_xe = blk->in_pos_xs + blk->in_tile_width - 1;

			if (in_const_x > 1 && val_e)
				blk->in_pos_xe -= val_e;
		}

		/* Module with crop */
		if ((blk->type & TILE_TYPE_CROP_EN) && blk->enable_flag) {
			if (!(blk->type & TILE_TYPE_LOSS))
				blk->crop_h_end_flag &= blk->h_end_flag;

			if (blk->in_pos_xs + blk->in_tile_width < blk->full_size_x_in)
				blk->max_h_edge_flag = false;
			else if (!(blk->type & TILE_TYPE_LOSS))
				blk->max_h_edge_flag = true;
		} else {
			if (blk->max_h_edge_flag)
				if (blk->in_pos_xs + blk->in_tile_width < blk->full_size_x_in)
					blk->max_h_edge_flag = false;
		}
	} else {
		/* Module with crop */
		if ((blk->type & TILE_TYPE_CROP_EN) && blk->enable_flag) {
			if (!(blk->type & TILE_TYPE_LOSS)) {
				blk->crop_h_end_flag &= blk->h_end_flag;
				blk->max_h_edge_flag = true;
			}
		}
	}

	/* Update tile edge flag due to dz crop & tile size */
	if (blk->in_pos_xe + 1 < blk->full_size_x_in)
		blk->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;

	/* Output don't touch right edge */
	if ((blk->tdr_edge & TILE_EDGE_RIGHT_MASK) != TILE_EDGE_RIGHT_MASK) {
		/* Input touch right edge */
		if (blk->in_pos_xe + 1 >= blk->full_size_x_in) {
			/* Crop module */
			if ((blk->type & TILE_TYPE_CROP_EN) &&
			    !(blk->type & TILE_TYPE_LOSS) &&
			    blk->enable_flag) {
				blk->tdr_edge |= TILE_EDGE_RIGHT_MASK;
			} else {
				/* Reduce input size if not able to touch edge */
				if (blk->max_h_edge_flag)
					blk->tdr_edge |= TILE_EDGE_RIGHT_MASK;
				else
					/* Reduce input size if not able to touch edge */
					blk->in_pos_xe -= in_const_x;
			}
		}
	}

	/* Sync end flag with input end position */
	if (blk->h_end_flag) {
		if (blk->in_pos_xe < blk->max_in_pos_xe) {
			if (blk->valid_h_no) {
				/* Diff view to check max last xe */
				if (blk->max_last_input_xe_pos < blk->max_in_pos_xe)
					blk->h_end_flag = false;
			} else {
				blk->h_end_flag = false;
			}
		}
	} else {
		/* Hit right edge with h_end_flag = false */
		if (blk->in_pos_xe + 1 >= blk->full_size_x_in) {
			if (blk->crop_h_end_flag && blk->max_h_edge_flag)
				blk->h_end_flag = true;
		} else {
			if (blk->crop_h_end_flag) {
				if (blk->valid_h_no) {
					/* Diff view to check max last xe */
					if (blk->max_last_input_xe_pos >= blk->max_in_pos_xe)
						blk->h_end_flag = true;
				} else {
					if (blk->in_pos_xe >= blk->max_in_pos_xe)
						blk->h_end_flag = true;
				}
			}
		}
	}

	/* Backup backward */
	blk->backward_input_xs_pos = blk->in_pos_xs;
	blk->backward_input_xe_pos = blk->in_pos_xe;
	blk->backward_output_xs_pos = blk->out_pos_xs;
	blk->backward_output_xe_pos = blk->out_pos_xe;

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_bwd_by_func_post_x_inv(struct tile_func_block *blk,
						     struct tile_reg_map *reg_map)
{
	int in_const_x = blk->in_const_x;
	int min_xsize = blk->in_min_width;
	int min_in_pos_xs;
	int val_s = TILE_MOD(blk->in_pos_xs, in_const_x);
	int val_e = TILE_MOD(blk->in_pos_xe + 1, in_const_x);
	int bound;

	if (reg_map->skip_x_cal)
		goto err_return;

	if (blk->tdr_h_disable_flag) {
		/* Backup flag */
		blk->backward_tdr_h_disable_flag = blk->tdr_h_disable_flag;
		blk->backward_h_end_flag = blk->h_end_flag;
		goto err_return;
	}

	min_in_pos_xs = reg_map->first_frame ? 0 : blk->min_in_pos_xs;

	if (blk->enable_flag) {
		/* Check min x size */
		if (blk->in_pos_xs + blk->in_min_width > blk->in_pos_xe + 1) {
			blk->in_pos_xs = blk->in_pos_xe + 1 -  min_xsize;

			if (blk->in_pos_xs <= min_in_pos_xs) {
				if (min_in_pos_xs == 0) {
					if (blk->max_h_edge_flag) {
						blk->in_pos_xs = 0;
						blk->in_pos_xe = min_xsize - 1;
					} else {
						blk->in_pos_xs = in_const_x;
						blk->in_pos_xe = min_xsize - 1 + in_const_x;
					}
				} else {
					blk->in_pos_xs = min_in_pos_xs;
					blk->in_pos_xe = min_in_pos_xs + min_xsize - 1;
				}

				/* Align xe position */
				if (in_const_x > 1 && val_e)
					blk->in_pos_xe += in_const_x - val_e;
			}

			/* Align xs position */
			if (in_const_x > 1 && val_s)
				blk->in_pos_xs -= val_s;
		}

		/* Check crop & update in_tile_width_max */
		if (!(blk->type & TILE_TYPE_CROP_EN) || (blk->type & TILE_TYPE_LOSS)) {
			/* In_tile_width_max */
			bound = blk->out_tile_width_max;
			bound += blk->l_tile_loss + blk->r_tile_loss;

			if (blk->in_tile_width && blk->out_tile_width_max) {
				if (blk->in_tile_width > bound)
					blk->in_tile_width_max = bound;
				else
					blk->in_tile_width_max = blk->in_tile_width;
			} else if (blk->out_tile_width_max) {
				blk->in_tile_width_max = bound;
			} else {
				blk->in_tile_width_max = blk->in_tile_width;
			}

			/* In_tile_width_max_str */
			if (blk->in_tile_width && blk->out_tile_width_max_str) {
				if (blk->in_tile_width >
				    blk->out_tile_width_max_str + blk->l_tile_loss)
					blk->in_tile_width_max_str = blk->out_tile_width_max_str +
								     blk->l_tile_loss;
				else
					blk->in_tile_width_max_str = blk->in_tile_width;
			} else if (blk->out_tile_width_max_str) {
				blk->in_tile_width_max_str = blk->out_tile_width_max_str +
							     blk->l_tile_loss;
			} else {
				blk->in_tile_width_max_str = blk->in_tile_width;
			}

			/* Prevent from min edge error */
			if (blk->out_tile_width_max_str &&
			    blk->out_tile_width_max_str < blk->full_size_x_out &&
			    blk->in_tile_width_max_str >= blk->full_size_x_in)
				blk->in_tile_width_max_str = blk->full_size_x_in - in_const_x;

			/* In_tile_width_max_end */
			bound = blk->out_tile_width_max_end + blk->r_tile_loss;
			if (blk->in_tile_width && blk->out_tile_width_max_end) {
				if (blk->in_tile_width > bound)
					blk->in_tile_width_max_end = bound;
				else
					blk->in_tile_width_max_end = blk->in_tile_width;
			} else if (blk->out_tile_width_max_end) {
				blk->in_tile_width_max_end = bound;
			} else {
				blk->in_tile_width_max_end = blk->in_tile_width;
			}

			/* Smart tile + ufd */
			blk->in_tile_width_loss = blk->out_tile_width_loss +
						  blk->l_tile_loss + blk->r_tile_loss;

			if (blk->min_in_pos_xs + blk->l_tile_loss < blk->min_out_crop_xs)
				blk->min_in_crop_xs = blk->min_out_crop_xs - blk->l_tile_loss;
			else
				blk->min_in_crop_xs = blk->min_in_pos_xs;

			bound = blk->min_tile_crop_out_pos_xs - blk->l_tile_loss;
			if (blk->min_in_pos_xs + blk->l_tile_loss < blk->min_tile_crop_out_pos_xs)
				blk->min_tile_crop_in_pos_xs = bound;
			else
				blk->min_tile_crop_in_pos_xs = blk->min_in_pos_xs;
		} else {
			blk->in_tile_width_max = blk->in_tile_width;
			blk->in_tile_width_max_str = blk->in_tile_width;
			blk->in_tile_width_max_end = blk->in_tile_width;
			/* Smart tile + ufd */
			blk->in_tile_width_loss = 0;
			blk->min_in_crop_xs = blk->min_in_pos_xs;
			blk->min_tile_crop_in_pos_xs = blk->min_tile_in_pos_xs;
		}
	} else {
		/* In_tile_width_max */
		if (blk->in_tile_width && blk->out_tile_width_max) {
			if (blk->in_tile_width > blk->out_tile_width_max)
				blk->in_tile_width_max = blk->out_tile_width_max;
			else
				blk->in_tile_width_max = blk->in_tile_width;
		} else if (blk->out_tile_width_max) {
			blk->in_tile_width_max = blk->out_tile_width_max;
		} else {
			blk->in_tile_width_max = blk->in_tile_width;
		}

		/* In_tile_width_max_str */
		if (blk->in_tile_width && blk->out_tile_width_max_str) {
			if (blk->in_tile_width > blk->out_tile_width_max_str)
				blk->in_tile_width_max_str = blk->out_tile_width_max_str;
			else
				blk->in_tile_width_max_str = blk->in_tile_width;
		} else if (blk->out_tile_width_max_str) {
			blk->in_tile_width_max_str = blk->out_tile_width_max_str;
		} else {
			blk->in_tile_width_max_str = blk->in_tile_width;
		}

		/* In_tile_width_max_end */
		if (blk->in_tile_width && blk->out_tile_width_max_end) {
			if (blk->in_tile_width > blk->out_tile_width_max_end)
				blk->in_tile_width_max_end = blk->out_tile_width_max_end;
			else
				blk->in_tile_width_max_end = blk->in_tile_width;
		} else if (blk->out_tile_width_max_end) {
			blk->in_tile_width_max_end = blk->out_tile_width_max_end;
		} else {
			blk->in_tile_width_max_end = blk->in_tile_width;
		}

		/* Smart tile + ufd */
		blk->in_tile_width_loss = blk->out_tile_width_loss;
		blk->min_in_crop_xs = blk->min_out_crop_xs;
		blk->min_tile_crop_in_pos_xs = blk->min_tile_crop_out_pos_xs;
	}

	/* Check align mis-match */
	if (!(blk->type & TILE_TYPE_CROP_EN) ||
	    (blk->type & TILE_TYPE_LOSS) ||
	    !blk->enable_flag) {
		/* Left edge */
		if (blk->in_pos_xs <= 0 && blk->in_tile_width_max_end)
			if (blk->in_tile_width_max_end < blk->in_pos_xe + 1)
				blk->in_pos_xs = in_const_x;

		/* Intermeidate */
		if (blk->in_pos_xs > 0 && blk->in_tile_width_max) {
			if (blk->in_tile_width_max + blk->in_pos_xs < blk->in_pos_xe + 1) {
				blk->in_pos_xs = blk->in_pos_xe - blk->in_tile_width_max + 1;

				if (in_const_x > 1 && val_s)
					blk->in_pos_xs += in_const_x - val_s;
			}
		}

		/* Right edge */
		if (blk->in_pos_xe + 1 >= blk->full_size_x_in && blk->in_tile_width_max_str) {
			if (blk->in_tile_width_max_str + blk->in_pos_xs <  blk->full_size_x_in) {
				blk->in_pos_xs = blk->full_size_x_in - blk->in_tile_width_max_str;

				if (in_const_x > 1 && val_s)
					blk->in_pos_xs += in_const_x - val_s;
			}
		}
	}

	/* clip input size */
	if (blk->in_pos_xs < 0)
		blk->in_pos_xs = 0;

	if (blk->in_pos_xe + 1 >= blk->full_size_x_in)
		blk->in_pos_xe = blk->full_size_x_in - 1;

	/* Config max_h_edge_flag */
	if (blk->in_tile_width) {
		/* Bound in_pos_xe with in_tile_width and make it align */
		if (blk->in_pos_xe + 1 > blk->in_pos_xs + blk->in_tile_width) {
			blk->in_pos_xs = blk->in_pos_xe - blk->in_tile_width + 1;

			if (in_const_x > 1 && val_s)
				blk->in_pos_xs += in_const_x - val_s;
		}

		if ((blk->type & TILE_TYPE_CROP_EN) && blk->enable_flag) {
			if (!(blk->type & TILE_TYPE_LOSS))
				blk->crop_h_end_flag &= blk->h_end_flag;

			/* note boundary, xs = 0 */
			if (blk->in_pos_xe >= blk->in_tile_width)
				blk->max_h_edge_flag = false;
			else if (!(blk->type & TILE_TYPE_LOSS))
				blk->max_h_edge_flag = true;
		} else {
			/* note boundary, xs = 0 */
			if (blk->max_h_edge_flag &&
			    blk->in_pos_xe >= blk->in_tile_width)
				blk->max_h_edge_flag = false;
		}
	} else if ((blk->type & TILE_TYPE_CROP_EN) && blk->enable_flag) {
		/* Crop module */
		if (!(blk->type & TILE_TYPE_LOSS)) {
			blk->crop_h_end_flag &= blk->h_end_flag;
			blk->max_h_edge_flag = true;
		}
	}

	/* Update tile edge flag due to dz crop & tile size */
	if (blk->in_pos_xs > 0)
		blk->tdr_edge &= ~TILE_EDGE_LEFT_MASK;

	/* Output don't touch left edge */
	if ((blk->tdr_edge & TILE_EDGE_LEFT_MASK) != TILE_EDGE_LEFT_MASK) {
		/* Input touch left edge */
		if (blk->in_pos_xs <= 0) {
			/* Crop module */
			if ((blk->type & TILE_TYPE_CROP_EN) &&
			    !(blk->type & TILE_TYPE_LOSS) &&
			    blk->enable_flag) {
				blk->tdr_edge |= TILE_EDGE_LEFT_MASK;
			} else {
				/* Reduce input size if not able to touch edge */
				if (blk->max_h_edge_flag)
					blk->tdr_edge |= TILE_EDGE_LEFT_MASK;
				else
					blk->in_pos_xs = in_const_x;
			}
		}
	}

	/* Output don't touch right edge */
	if ((blk->tdr_edge & TILE_EDGE_RIGHT_MASK) != TILE_EDGE_RIGHT_MASK) {
		/* Input touch right edge */
		if (blk->in_pos_xe + 1 >= blk->full_size_x_in)
			blk->tdr_edge |= TILE_EDGE_RIGHT_MASK;
	} else {
		/* Input don't touch right edge */
		if (blk->in_pos_xe + 1 < blk->full_size_x_in)
			if (blk->type & TILE_TYPE_CROP_EN)
				blk->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
	}

	/* Sync end flag with input start position */
	if (blk->h_end_flag) {
		if (blk->in_pos_xs > blk->min_in_pos_xs) {
			/* Diff view to check min last xs */
			if (blk->valid_h_no) {
				if (blk->min_last_input_xs_pos > blk->min_in_pos_xs)
					blk->h_end_flag = false;
			} else {
				blk->h_end_flag = false;
			}
		}
	} else {
		/* Hit left edge with h_end_flag = false */
		if (blk->in_pos_xs <= 0) {
			if (blk->crop_h_end_flag && blk->max_h_edge_flag)
				blk->h_end_flag = true;
		} else if (blk->crop_h_end_flag) {
			if (blk->valid_h_no) {
				/* Diff view to check max last xe */
				if (blk->min_last_input_xs_pos <= blk->min_in_pos_xs)
					blk->h_end_flag = true;
			} else {
				if (blk->in_pos_xs <= blk->min_in_pos_xs)
					blk->h_end_flag = true;
			}
		}
	}

	/* Backup backward */
	blk->backward_input_xs_pos = blk->in_pos_xs;
	blk->backward_input_xe_pos = blk->in_pos_xe;
	blk->backward_output_xs_pos = blk->out_pos_xs;
	blk->backward_output_xe_pos = blk->out_pos_xe;

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_bwd_by_func_post_y(struct tile_func_block *blk,
						 struct tile_reg_map *reg_map)
{
	int in_const_y = blk->in_const_y;
	int min_ysize = blk->in_min_height;
	int max_in_pos_ye;
	int val_s = TILE_MOD(blk->in_pos_ys, in_const_y);
	int val_e = TILE_MOD(blk->in_pos_ye + 1, in_const_y);
	int bound;

	if (reg_map->skip_y_cal)
		goto err_return;

	if (blk->tdr_v_disable_flag) {
		/* Backup flag */
		blk->backward_tdr_v_disable_flag = blk->tdr_v_disable_flag;
		blk->backward_v_end_flag = blk->v_end_flag;
		goto err_return;
	}

	max_in_pos_ye = reg_map->first_frame ?
			(blk->full_size_y_in - 1) : blk->max_in_pos_ye;

	if (blk->enable_flag) {
		/* Check min x size */
		if (blk->in_pos_ys + blk->in_min_height > blk->in_pos_ye + 1) {
			blk->in_pos_ye = blk->in_pos_ys + min_ysize - 1;

			if (blk->in_pos_ye >= max_in_pos_ye) {
				if (max_in_pos_ye + 1 >= blk->full_size_y_in) {
					if (blk->max_v_edge_flag) {
						blk->in_pos_ye = max_in_pos_ye;
						blk->in_pos_ys = max_in_pos_ye + 1 - min_ysize;
					} else {
						blk->in_pos_ye = max_in_pos_ye - in_const_y;
						blk->in_pos_ys = max_in_pos_ye + 1 -
								 min_ysize - in_const_y;
					}
				} else {
					blk->in_pos_ye = max_in_pos_ye;
					blk->in_pos_ys = max_in_pos_ye + 1 - min_ysize;
				}

				/* align ys position */
				if (in_const_y > 1 && val_s)
					blk->in_pos_ys -= val_s;
			}

			/* Align input position */
			if (in_const_y > 1 && val_e)
				blk->in_pos_ye += in_const_y - val_e;
		}

		/* Check crop & update in_tile_height_max */
		if (!(blk->type & TILE_TYPE_CROP_EN) || (blk->type & TILE_TYPE_LOSS)) {
			bound = blk->out_tile_height_max + blk->t_tile_loss + blk->b_tile_loss;
			if (blk->in_tile_height && blk->out_tile_height_max) {
				if (blk->in_tile_height > bound)
					blk->in_tile_height_max = bound;
				else
					blk->in_tile_height_max = blk->in_tile_height;
			} else if (blk->out_tile_height_max) {
				blk->in_tile_height_max = bound;
			} else {
				blk->in_tile_height_max = blk->in_tile_height;
			}

			bound = blk->out_tile_height_max_str + blk->b_tile_loss;
			if (blk->in_tile_height && blk->out_tile_height_max_str) {
				if (blk->in_tile_height > bound)
					blk->in_tile_height_max_str = bound;
				else
					blk->in_tile_height_max_str = blk->in_tile_height;
			} else if (blk->out_tile_height_max_str) {
				blk->in_tile_height_max_str = bound;
			} else {
				blk->in_tile_height_max_str = blk->in_tile_height;
			}

			/* Prevent from min edge error */
			if (blk->out_tile_height_max_str &&
			    blk->out_tile_height_max_str < blk->full_size_y_out &&
			    blk->in_tile_height_max_str >= blk->full_size_y_in)
				blk->in_tile_height_max_str = blk->full_size_y_in -
								in_const_y;

			/* In_tile_height_max_end */
			bound = blk->out_tile_height_max_end + blk->t_tile_loss;
			if (blk->in_tile_height && blk->out_tile_height_max_end) {
				if (blk->in_tile_height > bound)
					blk->in_tile_height_max_end = bound;
				else
					blk->in_tile_height_max_end = blk->in_tile_height;
			} else if (blk->out_tile_height_max_end) {
				blk->in_tile_height_max_end = bound;
			} else {
				blk->in_tile_height_max_end = blk->in_tile_height;
			}
		} else {
			blk->in_tile_height_max = blk->in_tile_height;
			blk->in_tile_height_max_str = blk->in_tile_height;
			blk->in_tile_height_max_end = blk->in_tile_height;
		}
	} else {
		if (blk->in_tile_height && blk->out_tile_height_max) {
			if (blk->in_tile_height > blk->out_tile_height_max)
				blk->in_tile_height_max = blk->out_tile_height_max;
			else
				blk->in_tile_height_max = blk->in_tile_height;
		} else if (blk->out_tile_height_max) {
			blk->in_tile_height_max = blk->out_tile_height_max;
		} else {
			blk->in_tile_height_max = blk->in_tile_height;
		}

		if (blk->in_tile_height && blk->out_tile_height_max_str) {
			if (blk->in_tile_height > blk->out_tile_height_max_str)
				blk->in_tile_height_max_str = blk->out_tile_height_max_str;
			else
				blk->in_tile_height_max_str = blk->in_tile_height;
		} else if (blk->out_tile_height_max_str) {
			blk->in_tile_height_max_str = blk->out_tile_height_max_str;
		} else {
			blk->in_tile_height_max_str = blk->in_tile_height;
		}

		if (blk->in_tile_height && blk->out_tile_height_max_end) {
			if (blk->in_tile_height > blk->out_tile_height_max_end)
				blk->in_tile_height_max_end = blk->out_tile_height_max_end;
			else
				blk->in_tile_height_max_end = blk->in_tile_height;
		} else if (blk->out_tile_height_max_end) {
			blk->in_tile_height_max_end = blk->out_tile_height_max_end;
		} else {
			blk->in_tile_height_max_end = blk->in_tile_height;
		}
	}

	/* Check align mis-match */
	if (!(blk->type & TILE_TYPE_CROP_EN) ||
	    (blk->type & TILE_TYPE_LOSS) ||
	    !blk->enable_flag) {
		/* Bottom edge */
		if (blk->in_pos_ye + 1 >= blk->full_size_y_in && blk->in_tile_height_max_end)
			if (blk->in_tile_height_max_end + blk->in_pos_ys < blk->full_size_y_in)
				blk->in_pos_ye = blk->full_size_y_in - 1 - in_const_y;

		/* Intermeidate */
		if (blk->in_pos_ye + 1 < blk->full_size_y_in && blk->in_tile_height_max) {
			if (blk->in_tile_height_max + blk->in_pos_ys < blk->in_pos_ye + 1) {
				blk->in_pos_ye = blk->in_pos_ys + blk->in_tile_height_max - 1;

				if (in_const_y > 1 && val_e)
					blk->in_pos_ye -= val_e;
			}
		}

		/* Bottom edge */
		if (blk->in_pos_ys <= 0 && blk->in_tile_height_max_str) {
			if (blk->in_tile_height_max_str < blk->in_pos_ye + 1) {
				blk->in_pos_ye = blk->in_tile_height_max_str - 1;

				if (in_const_y > 1 && val_e)
					blk->in_pos_ye -= val_e;
			}
		}
	}

	/* Clip input size */
	if (blk->in_pos_ye + 1 >= blk->full_size_y_in)
		blk->in_pos_ye = blk->full_size_y_in - 1;

	if (blk->in_pos_ys <= 0)
		blk->in_pos_ys = 0;

	/* Config max_v_edge_flag */
	if (blk->in_tile_height) {
		/* Check input end position with input tile size and make it align */
		if (blk->in_pos_ye + 1 > blk->in_pos_ys + blk->in_tile_height) {
			blk->in_pos_ye = blk->in_pos_ys + blk->in_tile_height - 1;

			if (in_const_y > 1 && val_e)
				blk->in_pos_ye -= val_e;
		}

		if ((blk->type & TILE_TYPE_CROP_EN) && blk->enable_flag) {
			if (!(blk->type & TILE_TYPE_LOSS))
				blk->crop_v_end_flag &= blk->v_end_flag;

			if (blk->in_pos_ys + blk->in_tile_height < blk->full_size_y_in)
				blk->max_v_edge_flag = false;
			else if (!(blk->type & TILE_TYPE_LOSS))
				blk->max_v_edge_flag = true;
		} else if (blk->max_v_edge_flag) {
			if (blk->in_pos_ys + blk->in_tile_height < blk->full_size_y_in)
				blk->max_v_edge_flag = false;
		}
	} else {
		if ((blk->type & TILE_TYPE_CROP_EN) && blk->enable_flag) {
			if (!(blk->type & TILE_TYPE_LOSS)) {
				blk->crop_v_end_flag &= blk->v_end_flag;
				blk->max_v_edge_flag = true;
			}
		}
	}

	/* Update tile edge flag due to dz crop & tile size */
	if (blk->in_pos_ye + 1 < blk->full_size_y_in)
		blk->tdr_edge &= ~TILE_EDGE_BOTTOM_MASK;

	/* Output don't touch bottom edge */
	if (TILE_EDGE_BOTTOM_MASK != (blk->tdr_edge & TILE_EDGE_BOTTOM_MASK)) {
		/* Input touch bottom edge */
		if (blk->in_pos_ye + 1 >= blk->full_size_y_in) {
			if ((blk->type & TILE_TYPE_CROP_EN) &&
			    !(blk->type & TILE_TYPE_LOSS) &&
			    blk->enable_flag) {
				blk->tdr_edge |= TILE_EDGE_BOTTOM_MASK;
			} else {
				/* Non-crop module */
				if (blk->max_v_edge_flag)
					blk->tdr_edge |= TILE_EDGE_BOTTOM_MASK;
				else /* Reduce input size if not able to touch edge */
					blk->in_pos_ye -= in_const_y;
			}
		}
	}

	/* Output don't touch top edge */
	if ((blk->tdr_edge & TILE_EDGE_TOP_MASK) != TILE_EDGE_TOP_MASK) {
		/* Input touch top edge */
		if (blk->in_pos_ys <= 0)
			blk->tdr_edge |= TILE_EDGE_TOP_MASK;
	} else {
		/* Input don't touch top edge */
		if (blk->in_pos_ys > 0 &&
		    (blk->type & TILE_TYPE_CROP_EN))
			blk->tdr_edge &= ~TILE_EDGE_TOP_MASK;
	}

	/* Sync end flag with end position */
	if (blk->v_end_flag) {
		if (blk->in_pos_ye < blk->max_in_pos_ye) {
			/* Diff view to check max last ye */
			if (blk->valid_v_no) {
				if (blk->max_last_input_ye_pos < blk->max_in_pos_ye)
					blk->v_end_flag = false;
			} else {
				blk->v_end_flag = false;
			}
		}
	} else {
		/* Hit bottom edge with v_end_flag = false */
		if (blk->in_pos_ye + 1 >= blk->full_size_y_in) {
			if (blk->crop_v_end_flag && blk->max_v_edge_flag)
				blk->v_end_flag = true;
		} else if (blk->crop_v_end_flag) {
			if (blk->valid_v_no) {
				/* Diff view to check max last xe */
				if (blk->max_last_input_ye_pos >= blk->max_in_pos_ye)
					blk->v_end_flag = true;
			} else {
				if (blk->in_pos_ye >= blk->max_in_pos_ye)
					blk->v_end_flag = true;
			}
		}
	}

	/* Backup backward */
	blk->backward_input_ys_pos = blk->in_pos_ys;
	blk->backward_input_ye_pos = blk->in_pos_ye;
	blk->backward_output_ys_pos = blk->out_pos_ys;
	blk->backward_output_ye_pos = blk->out_pos_ye;

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_bwd_by_func(struct tile_func_block *blk,
					  struct tile_reg_map *reg_map)
{
	enum mdp_tile_msg result = T_OK;

	/* No output_disable & run mode check */
	result = tile_bwd_by_func_pre_x(blk, reg_map);
	if (result)
		goto err_return;

	result = tile_bwd_by_func_pre_y(blk, reg_map);
	if (result)
		goto err_return;

	/* Back comp by module, force tdr enable end function */
	if (blk->enable_flag) {
		if ((!reg_map->skip_x_cal && !blk->tdr_h_disable_flag) ||
		    (!reg_map->skip_y_cal && !blk->tdr_v_disable_flag) ||
		    blk->tot_branch_num == 0) {
			/* Back func should handle alignment by itself */
			result = tile_back_func_run(blk, reg_map);
			if (result)
				goto err_return;
		}
	}

	/* Check cal order, input */
	if (blk->in_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
		result = tile_bwd_by_func_post_x_inv(blk, reg_map);
	else
		result = tile_bwd_by_func_post_x(blk, reg_map);
	if (result)
		goto err_return;

	result = tile_bwd_by_func_post_y(blk, reg_map);

err_return:
	return result;
}

static enum mdp_tile_msg tile_fwd_by_func_pre_x(struct tile_func_block *blk,
						struct tile_reg_map *reg_map)
{
	if (reg_map->skip_x_cal)
		goto err_return;

	if (blk->tdr_h_disable_flag)
		goto err_return;

	if (!blk->enable_flag) {
		blk->out_pos_xs = blk->in_pos_xs;
		blk->out_pos_xe = blk->in_pos_xe;
		goto err_return;
	}

	/* Update input left edge */
	if (blk->in_pos_xs <= 0)
		blk->tdr_edge |= TILE_EDGE_LEFT_MASK;

	/* Update input right edge */
	if (blk->in_pos_xe + 1 < blk->full_size_x_in)
		blk->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
	else
		blk->tdr_edge |= TILE_EDGE_RIGHT_MASK;

	/* Sub-in cannot change main path */
	if (blk->run_mode == TILE_RUN_MODE_SUB_IN && (blk->type & TILE_TYPE_CROP_EN)) {
		/* Skip input position check due to hw size limitation */
		if (blk->backward_output_xs_pos > 0)
			blk->out_pos_xs = blk->in_pos_xs + blk->l_tile_loss;

		if (blk->backward_output_xe_pos + 1 < blk->full_size_x_in)
			blk->out_pos_xe = blk->in_pos_xe - blk->r_tile_loss;
	} else {
		/* Set fixed left tile loss */
		if (blk->tdr_edge & TILE_EDGE_LEFT_MASK)
			blk->out_pos_xs = blk->in_pos_xs;
		else
			blk->out_pos_xs = blk->in_pos_xs + blk->l_tile_loss;

		/* Set fixed right tile loss */
		if (blk->tdr_edge & TILE_EDGE_RIGHT_MASK)
			blk->out_pos_xe = blk->in_pos_xe;
		else
			blk->out_pos_xe = blk->in_pos_xe - blk->r_tile_loss;
	}

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_fwd_by_func_pre_x_inv(struct tile_func_block *blk,
						    struct tile_reg_map *reg_map)
{
	if (reg_map->skip_x_cal)
		goto err_return;

	if (blk->tdr_h_disable_flag)
		goto err_return;

	if (!blk->enable_flag) {
		blk->out_pos_xs = blk->in_pos_xs;
		blk->out_pos_xe = blk->in_pos_xe;
		goto err_return;
	}

	/* Update input right edge */
	if (blk->in_pos_xe + 1 >= blk->full_size_x_in)
		blk->tdr_edge |= TILE_EDGE_RIGHT_MASK;

	/* Update input left edge */
	if (blk->in_pos_xs > 0)
		blk->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
	else
		blk->tdr_edge |= TILE_EDGE_LEFT_MASK;

	/* Sub-in cannot change main path */
	if (blk->run_mode == TILE_RUN_MODE_SUB_IN && (blk->type & TILE_TYPE_CROP_EN)) {
		/* Skip input position check due to hw size limitation */
		if (blk->backward_output_xs_pos > 0)
			blk->out_pos_xs = blk->in_pos_xs + blk->l_tile_loss;

		if (blk->backward_output_xe_pos + 1 < blk->full_size_x_in)
			blk->out_pos_xe = blk->in_pos_xe - blk->r_tile_loss;
	} else {
		/* Set fixed right tile loss */
		if (blk->tdr_edge & TILE_EDGE_RIGHT_MASK)
			blk->out_pos_xe = blk->in_pos_xe;
		else
			blk->out_pos_xe = blk->in_pos_xe - blk->r_tile_loss;

		/* Set fixed left tile loss */
		if (blk->tdr_edge & TILE_EDGE_LEFT_MASK)
			blk->out_pos_xs = blk->in_pos_xs;
		else
			blk->out_pos_xs = blk->in_pos_xs + blk->l_tile_loss;
	}

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_fwd_by_func_pre_y(struct tile_func_block *blk,
						struct tile_reg_map *reg_map)
{
	if (reg_map->skip_y_cal)
		goto err_return;

	if (blk->tdr_v_disable_flag)
		goto err_return;

	if (!blk->enable_flag) {
		blk->out_pos_ys = blk->in_pos_ys;
		blk->out_pos_ye = blk->in_pos_ye;
		goto err_return;
	}

	/* Update input top edge */
	if (blk->in_pos_ys <= 0)
		blk->tdr_edge |= TILE_EDGE_TOP_MASK;

	/* Update input bottom edge */
	if (blk->in_pos_ye + 1 < blk->full_size_y_in)
		blk->tdr_edge &= ~TILE_EDGE_BOTTOM_MASK;
	else
		blk->tdr_edge |= TILE_EDGE_BOTTOM_MASK;

	/* Sub-in cannot change main path */
	if (blk->run_mode == TILE_RUN_MODE_SUB_IN && (blk->type & TILE_TYPE_CROP_EN)) {
		/* Skip input position check due to hw size limitation */
		if (blk->backward_output_ys_pos > 0)
			blk->out_pos_ys = blk->in_pos_ys + blk->t_tile_loss;

		if (blk->backward_output_ye_pos + 1 < blk->full_size_y_in)
			blk->out_pos_ye = blk->in_pos_ye - blk->b_tile_loss;
	} else {
		/* Set fixed top tile loss */
		if (blk->tdr_edge & TILE_EDGE_TOP_MASK)
			blk->out_pos_ys = blk->in_pos_ys;
		else
			blk->out_pos_ys = blk->in_pos_ys + blk->t_tile_loss;

		/* Set fixed bottom tile loss */
		if (blk->tdr_edge & TILE_EDGE_BOTTOM_MASK)
			blk->out_pos_ye = blk->in_pos_ye;
		else
			blk->out_pos_ye = blk->in_pos_ye - blk->b_tile_loss;
	}

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_fwd_by_func_post_x(struct tile_func_block *blk,
						 struct tile_reg_map *reg_map)
{
	if (reg_map->skip_x_cal)
		goto err_return;

	if (blk->tdr_h_disable_flag)
		goto err_return;

	/* Crop module */
	if (blk->type & TILE_TYPE_CROP_EN) {
		/* Input touch left edge */
		if (blk->tdr_edge & TILE_EDGE_LEFT_MASK) {
			/* Output don't touch left edge */
			if (blk->out_pos_xs > 0)
				blk->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
		} else { /* Input don't touch left edge */
			/* Output touch left edge */
			if (blk->out_pos_xs <= 0)
				blk->tdr_edge |= TILE_EDGE_LEFT_MASK;
		}

		/* Input touch right edge */
		if (blk->tdr_edge & TILE_EDGE_RIGHT_MASK) {
			/* Output don't touch right edge */
			if (blk->out_pos_xe + 1 < blk->full_size_x_out)
				blk->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
		} else { /* Input don't touch right edge */
			/* Output touch right edge */
			if (blk->out_pos_xe + 1 >= blk->full_size_x_out)
				blk->tdr_edge |= TILE_EDGE_RIGHT_MASK;
		}
	}

	/* For all modules - set left edge if output touch left edge */
	if (blk->out_pos_xs <= 0)
		blk->tdr_edge |= TILE_EDGE_LEFT_MASK;

	if (reg_map->first_pass || reg_map->first_frame)
		goto err_return;
	if (!reg_map->tdr_ctrl_en)
		goto err_return;
	if (blk->run_mode != TILE_RUN_MODE_MAIN)
		goto err_return;

	if (blk->in_max_width)
		if (blk->in_log_width + blk->in_pos_xs < blk->in_pos_xe + 1)
			blk->in_log_width = blk->in_pos_xe - blk->in_pos_xs + 1;

	if (blk->out_max_width)
		if (blk->out_log_width + blk->out_pos_xs < blk->out_pos_xe + 1)
			blk->out_log_width = blk->out_pos_xe - blk->out_pos_xs + 1;

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_fwd_by_func_post_x_inv(struct tile_func_block *blk,
						     struct tile_reg_map *reg_map)
{
	if (reg_map->skip_x_cal)
		goto err_return;

	if (blk->tdr_h_disable_flag)
		goto err_return;

	/* Cop module */
	if (blk->type & TILE_TYPE_CROP_EN) {
		/* Input touch right edge */
		if (blk->tdr_edge & TILE_EDGE_RIGHT_MASK) {
			/* Output don't touch right edge*/
			if (blk->out_pos_xe + 1 < blk->full_size_x_out)
				blk->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
		} else { /* Input don't touch right edge */
			/* Output touch right edge*/
			if (blk->out_pos_xe + 1 >= blk->full_size_x_out)
				blk->tdr_edge |= TILE_EDGE_RIGHT_MASK;
		}
		/* Input touch left edge */
		if (blk->tdr_edge & TILE_EDGE_LEFT_MASK) {
			/* Output don't touch left edge */
			if (blk->out_pos_xs > 0)
				blk->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
		} else { /* Input don't touch left edge */
			/* Output touch left edge */
			if (blk->out_pos_xs <= 0)
				blk->tdr_edge |= TILE_EDGE_LEFT_MASK;
		}
	}

	/* For all modules - set right edge if output touch right edge */
	if (blk->out_pos_xe + 1 >= blk->full_size_x_out)
		blk->tdr_edge |= TILE_EDGE_RIGHT_MASK;

	if (reg_map->first_pass || reg_map->first_frame)
		goto err_return;
	if (!reg_map->tdr_ctrl_en)
		goto err_return;
	if (blk->run_mode != TILE_RUN_MODE_MAIN)
		goto err_return;

	if (blk->in_max_width)
		if (blk->in_log_width + blk->in_pos_xs < blk->in_pos_xe + 1)
			blk->in_log_width = blk->in_pos_xe - blk->in_pos_xs + 1;

	if (blk->out_max_width)
		if (blk->out_log_width + blk->out_pos_xs < blk->out_pos_xe + 1)
			blk->out_log_width = blk->out_pos_xe - blk->out_pos_xs + 1;

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_fwd_by_func_post_y(struct tile_func_block *blk,
						 struct tile_reg_map *reg_map)
{
	if (reg_map->skip_y_cal)
		goto err_return;

	if (blk->tdr_v_disable_flag)
		goto err_return;

	/* Crop module */
	if (blk->type & TILE_TYPE_CROP_EN) {
		/* Input touch top edge */
		if (blk->tdr_edge & TILE_EDGE_TOP_MASK) {
			/* Output don't touch top edge */
			if (blk->out_pos_ys > 0)
				blk->tdr_edge &= ~TILE_EDGE_TOP_MASK;
		} else { /* Input don't touch top edge */
			/* Output touch top edge */
			if (blk->out_pos_ys <= 0)
				blk->tdr_edge |= TILE_EDGE_TOP_MASK;
		}

		/* Input touch bottom edge */
		if (blk->tdr_edge & TILE_EDGE_BOTTOM_MASK) {
			/* Output don't touch bottom edge */
			if (blk->out_pos_ye + 1 < blk->full_size_y_out)
				blk->tdr_edge &= ~TILE_EDGE_BOTTOM_MASK;
		} else { /* Input don't touch bottom edge */
			/* Output touch bottom edge */
			if (blk->out_pos_ye + 1 >= blk->full_size_y_out)
				blk->tdr_edge |= TILE_EDGE_BOTTOM_MASK;
		}
	}

	/* For all modules - set top edge if output touch top edge */
	if (blk->out_pos_ys <= 0)
		blk->tdr_edge |= TILE_EDGE_TOP_MASK;

	if (reg_map->first_pass || reg_map->first_frame)
		goto err_return;
	if (!reg_map->tdr_ctrl_en)
		goto err_return;
	if (blk->run_mode != TILE_RUN_MODE_MAIN)
		goto err_return;

	if (blk->in_max_height)
		if (blk->in_log_height + blk->in_pos_ys < blk->in_pos_ye + 1)
			blk->in_log_height = blk->in_pos_ye - blk->in_pos_ys + 1;

	if (blk->out_max_height)
		if (blk->out_log_height + blk->out_pos_ys < blk->out_pos_ye + 1)
			blk->out_log_height = blk->out_pos_ye - blk->out_pos_ys + 1;

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_fwd_by_func(struct tile_func_block *blk,
					  struct tile_reg_map *reg_map)
{
	enum mdp_tile_msg result = T_OK;

	/* check cal order, input */
	if (blk->in_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
		result = tile_fwd_by_func_pre_x_inv(blk, reg_map);
	else
		result = tile_fwd_by_func_pre_x(blk, reg_map);
	if (result)
		goto err_return;

	result = tile_fwd_by_func_pre_y(blk, reg_map);
	if (result)
		goto err_return;

	/* forward comp */
	if (blk->enable_flag) {
		if ((!reg_map->skip_x_cal && !blk->tdr_h_disable_flag) ||
		    (!reg_map->skip_y_cal && !blk->tdr_v_disable_flag)) {
			result = tile_for_func_run(blk, reg_map);
			if (result)
				goto err_return;
		}
	}

	/* check cal order, output */
	if (blk->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
		result = tile_fwd_by_func_post_x_inv(blk, reg_map);
	else
		result = tile_fwd_by_func_post_x(blk, reg_map);
	if (result)
		goto err_return;

	result = tile_fwd_by_func_post_y(blk, reg_map);

err_return:
	return result;
}

static enum mdp_tile_msg tile_fwd_by_func_no_back_pre_x(struct tile_func_block *blk,
							struct tile_reg_map *reg_map)
{
	if (reg_map->skip_x_cal)
		goto err_return;

	if (blk->tdr_h_disable_flag)
		goto err_return;

	/* update input left edge */
	if (blk->in_pos_xs <= 0)
		blk->tdr_edge |= TILE_EDGE_LEFT_MASK;
	else
		blk->tdr_edge &= ~TILE_EDGE_LEFT_MASK;

	/* set fixed left tile loss */
	if (blk->tdr_edge & TILE_EDGE_LEFT_MASK) {
		blk->out_pos_xs = blk->in_pos_xs;
	} else {
		if (blk->enable_flag)
			blk->out_pos_xs = blk->in_pos_xs + blk->l_tile_loss;
		else
			blk->out_pos_xs = blk->in_pos_xs;
	}

	/* update input right edge */
	if (blk->in_pos_xe + 1 < blk->full_size_x_in)
		blk->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
	else
		blk->tdr_edge |= TILE_EDGE_RIGHT_MASK;

	/* set fixed right tile loss */
	if (blk->tdr_edge & TILE_EDGE_RIGHT_MASK) {
		blk->out_pos_xe = blk->in_pos_xe;
	} else {
		if (blk->enable_flag)
			blk->out_pos_xe = blk->in_pos_xe - blk->r_tile_loss;
		else
			blk->out_pos_xe = blk->in_pos_xe;
	}

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_fwd_by_func_no_back_pre_y(struct tile_func_block *blk,
							struct tile_reg_map *reg_map)
{
	if (reg_map->skip_y_cal)
		goto err_return;

	if (blk->tdr_v_disable_flag)
		goto err_return;

	/* update input top edge */
	if (blk->in_pos_ys <= 0)
		blk->tdr_edge |= TILE_EDGE_TOP_MASK;
	else
		blk->tdr_edge &= ~TILE_EDGE_TOP_MASK;

	/* set fixed top tile loss */
	if (blk->tdr_edge & TILE_EDGE_TOP_MASK) {
		blk->out_pos_ys = blk->in_pos_ys;
	} else {
		if (blk->enable_flag)
			blk->out_pos_ys = blk->in_pos_ys + blk->t_tile_loss;
		else
			blk->out_pos_ys = blk->in_pos_ys;
	}

	/* update input bottom edge */
	if (blk->in_pos_ye + 1 < blk->full_size_y_in)
		blk->tdr_edge &= ~TILE_EDGE_BOTTOM_MASK;
	else
		blk->tdr_edge |= TILE_EDGE_BOTTOM_MASK;

	/* set fixed bottom tile loss */
	if (blk->tdr_edge & TILE_EDGE_BOTTOM_MASK) {
		blk->out_pos_ye = blk->in_pos_ye;
	} else {
		if (blk->enable_flag)
			blk->out_pos_ye = blk->in_pos_ye - blk->b_tile_loss;
		else
			blk->out_pos_ye = blk->in_pos_ye;
	}

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_fwd_by_func_no_back_post_x(struct tile_func_block *blk,
							 struct tile_reg_map *reg_map)
{
	if (reg_map->skip_x_cal)
		goto err_return;

	if (blk->tdr_h_disable_flag)
		goto err_return;

	/* update output right edge */
	if (blk->out_pos_xe + 1 >= blk->full_size_x_out)
		blk->tdr_edge |= TILE_EDGE_RIGHT_MASK;
	else
		blk->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;

	/* update output left edge */
	if (blk->out_pos_xs > 0)
		blk->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
	else
		blk->tdr_edge |= TILE_EDGE_LEFT_MASK;

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_fwd_by_func_no_back_post_y(struct tile_func_block *blk,
							 struct tile_reg_map *reg_map)
{
	if (reg_map->skip_y_cal)
		goto err_return;

	if (blk->tdr_v_disable_flag)
		goto err_return;

	/* update output bottom edge */
	if (blk->out_pos_ye + 1 >= blk->full_size_y_out)
		blk->tdr_edge |= TILE_EDGE_BOTTOM_MASK;
	else
		blk->tdr_edge &= ~TILE_EDGE_BOTTOM_MASK;

	/* update output top edge */
	if (blk->out_pos_ys > 0)
		blk->tdr_edge &= ~TILE_EDGE_TOP_MASK;
	else
		blk->tdr_edge |= TILE_EDGE_TOP_MASK;

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_fwd_by_func_no_back(struct tile_func_block *blk,
						  struct tile_reg_map *reg_map)
{
	enum mdp_tile_msg result = T_OK;

	result = tile_fwd_by_func_no_back_pre_x(blk, reg_map);
	if (result)
		goto err_return;
	result = tile_fwd_by_func_no_back_pre_y(blk, reg_map);
	if (result)
		goto err_return;

	/* forward comp */
	if (blk->enable_flag) {
		if ((!reg_map->skip_x_cal && !blk->tdr_h_disable_flag) ||
		    (!reg_map->skip_y_cal && !blk->tdr_v_disable_flag)) {
			result = tile_for_func_run(blk, reg_map);
			if (result)
				goto err_return;
		}
	}

	result = tile_fwd_by_func_no_back_post_x(blk, reg_map);
	if (result)
		goto err_return;
	result = tile_fwd_by_func_no_back_post_y(blk, reg_map);

err_return:
	return result;
}

static enum mdp_tile_msg tile_schedule_forward(struct func_description *func_param)
{
	int module_no = func_param->used_func_no;
	int i, j, k;

	/* clear valid flag */
	memset(func_param->valid_flag, 0, 4 * ((module_no + 31) >> 5));

	/* scheduling forward */
	for (i = 0; i < module_no; i++) {
		bool found = false;
		unsigned char *id = (unsigned char *)&func_param->scheduling_forward_order[i];

		for (j = 0; j < module_no; j++) {
			struct tile_func_block *f = func_param->func_list[j];
			unsigned int *valid = &func_param->valid_flag[j >> 5];
			int mask = 1 << (j & 0x1F);

			if (*valid & mask)
				continue;

			found = true;
			for (k = 0; k < f->tot_prev_num; k++) {
				unsigned char prv = f->prev_blk_num[k];
				int m_prv = 1 << (prv & 0x1F);

				/* start to set valid*/
				if (f->prev_blk_num[k] != PREVIOUS_BLK_NO_OF_START) {
					if (!(func_param->valid_flag[prv >> 5] & m_prv)) {
						/* non-start to set valid if previous valid */
						found = false;
						break;
					}
				}
			}

			if (found) {
				*id = j;
				*valid |= mask;
				break;
			}
		}
		if (!found)
			return T_SCHEDULING_FORWARD_ERROR;
	}

	return T_OK;
}

static enum mdp_tile_msg tile_fwd_recusive_check(struct tile_func_block *blk,
						 struct tile_reg_map *reg_map,
						 struct func_description *func_param,
						 bool *restart)
{
	int run_mode = reg_map->run_mode;
	int oob_x, oob_y;
	int mod;
	int i;

	if (reg_map->skip_x_cal)
		goto err_skip_x_cal;

	if (blk->tdr_h_disable_flag)
		goto err_skip_x_cal;

	oob_x = blk->out_pos_xe + 1 > blk->out_pos_xs + blk->out_tile_width;
	if (blk->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT) {
		mod = TILE_MOD(blk->out_pos_xs, blk->out_const_x);

		if ((blk->out_tile_width && oob_x) || (blk->out_const_x > 1 && mod)) {
			*restart = true;

			if (!reg_map->recursive_forward_en)
				return T_RECURSIVE_FOUND_ERROR;

			/* xe mis-alignment to reduce source xe */
			for (i = 0; i < func_param->used_func_no; i++) {
				struct tile_func_block *s = func_param->func_list[i];

				if (s->output_disable_flag)
					continue;
				if (run_mode != s->run_mode)
					continue;
				if (s->prev_blk_num[0] != PREVIOUS_BLK_NO_OF_START)
					continue;

				s->in_pos_xs += s->in_const_x;

				if (s->in_pos_xs > s->in_pos_xe ||
				    s->in_pos_xs > s->min_tile_in_pos_xs)
					return T_INCOR_XE_INPUT_POS_REDUCED_BY_TILE_SIZE_ERROR;
			}
		}
	} else {
		mod = ((blk->out_pos_xe + 1) % blk->out_const_x);

		if ((blk->out_tile_width && oob_x) || (blk->out_const_x > 1 && mod)) {
			*restart = true;

			if (!reg_map->recursive_forward_en)
				return T_RECURSIVE_FOUND_ERROR;

			/* xe mis-alignment to reduce source xe */
			for (i = 0; i < func_param->used_func_no; i++) {
				struct tile_func_block *s = func_param->func_list[i];

				if (s->output_disable_flag)
					continue;
				if (run_mode != s->run_mode)
					continue;
				if (s->prev_blk_num[0] != PREVIOUS_BLK_NO_OF_START)
					continue;

				s->in_pos_xe -= s->in_const_x;

				if (s->in_pos_xs > s->in_pos_xe ||
				    s->in_pos_xe < s->min_tile_in_pos_xe)
					return T_INCOR_XE_INPUT_POS_REDUCED_BY_TILE_SIZE_ERROR;
			}
		}
	}

err_skip_x_cal:
	if (reg_map->skip_y_cal)
		goto err_skip_y_cal;

	if (blk->tdr_v_disable_flag)
		goto err_skip_y_cal;

	oob_y = blk->out_pos_ye + 1 > blk->out_pos_ys + blk->out_tile_height;
	mod = TILE_MOD(blk->out_pos_ye + 1, blk->out_const_y);

	if ((blk->out_tile_height && oob_y) || (blk->out_const_y > 1 && mod)) {
		*restart = true;

		if (!reg_map->recursive_forward_en)
			return T_RECURSIVE_FOUND_ERROR;

		/* ye mis-alignment to reduce source ye */
		for (i = 0; i < func_param->used_func_no; i++) {
			struct tile_func_block *s = func_param->func_list[i];

			if (s->output_disable_flag)
				continue;
			if (run_mode != s->run_mode)
				continue;
			if (s->prev_blk_num[0] != PREVIOUS_BLK_NO_OF_START)
				continue;

			s->in_pos_ye -= s->in_const_y;

			if (s->in_pos_ys > s->in_pos_ye ||
			    s->in_pos_ye < s->min_tile_in_pos_ye)
				return T_INCOR_YE_INPUT_POS_REDUCED_BY_TILE_SIZE_ERROR;
		}
	}

err_skip_y_cal:
	return T_OK;
}

static enum mdp_tile_msg tile_fwd_comp_no_back(struct tile_reg_map *reg_map,
					       struct func_description *func_param)
{
	enum mdp_tile_msg result = T_OK;
	int run_mode = reg_map->run_mode;
	int i;

	/* Check early terminated */
	if (reg_map->backup_x_skip_y)
		return T_OK;

	/* Loop count can be reset for mis-alignment handle */
	for (i = 0; i < func_param->used_func_no; i++) {
		unsigned char id = func_param->scheduling_forward_order[i];
		struct tile_func_block *f = func_param->func_list[id];

		/* Skip output disable func */
		if (f->output_disable_flag)
			continue;
		if (run_mode != f->run_mode)
			continue;

		/* Config forward in pos of func */
		result = tile_fwd_input_config(f, reg_map, func_param);
		if (result)
			goto err_return;
		/* Forward comp by func */
		result = tile_fwd_by_func_no_back(f, reg_map);
		if (result)
			goto err_return;
		/* Check output smaller than tile size & out size */
		result = tile_fwd_output_check(f, reg_map);
		if (result)
			goto err_return;
	}

err_return:
	return result;
}

static enum mdp_tile_msg tile_fwd_comp(struct tile_reg_map *reg_map,
				       struct func_description *func_param)
{
	enum mdp_tile_msg result = T_OK;
	int i, loop_count = 0;
	int run_mode = reg_map->run_mode;

	/* check early terminated */
	if (reg_map->backup_x_skip_y)
		goto err_return;

	/* loop count can be reset for mis-alignment handle */
	for (i = 0; i < func_param->used_func_no; loop_count++) {
		unsigned char id = func_param->scheduling_forward_order[i];
		struct tile_func_block *f = func_param->func_list[id];
		bool restart = false;

		/* skip output disable func */
		if (!f->output_disable_flag &&
		    run_mode == f->run_mode) {
			/* check forward loop count */
			if (loop_count >= MAX_FORWARD_FUNC_CAL_LOOP_NO)
				return T_FWD_FUNC_CAL_LOOP_COUNT_OVER_MAX_ERROR;

			/* config forward in pos of func */
			result = tile_fwd_input_config(f, reg_map, func_param);
			if (result)
				goto err_return;
			/* forward comp by func */
			result = tile_fwd_by_func(f, reg_map);
			if (result)
				goto err_return;
			/* check over tile size then forward or xe/ye mis-alignment happen */
			result = tile_fwd_recusive_check(f, reg_map,
							 func_param, &restart);
			if (result)
				goto err_return;

			if (restart) {
				/* reduce source xe to meet alignment */
				i = 0;
				func_param->for_recursive_count++;
				continue;
			}

			/* check output smaller than tile size & out size */
			result = tile_fwd_output_check(f, reg_map);
			if (result)
				goto err_return;
		}

		i++;
	}

err_return:
	return result;
}

static enum mdp_tile_msg tile_fwd_input_config(struct tile_func_block *blk,
					       struct tile_reg_map *reg_map,
					       struct func_description *func_param)
{
	struct tile_func_block *prv;
	int run_mode = reg_map->run_mode;
	int i;

	if (reg_map->skip_x_cal)
		goto err_skip_x_cal;

	if (blk->tdr_h_disable_flag)
		goto err_skip_x_cal;

	/* skip start module */
	if (blk->prev_blk_num[0] != PREVIOUS_BLK_NO_OF_START) {
		bool found_prev = false;

		for (i = 0; i < blk->tot_prev_num; i++) {
			prv = func_param->func_list[blk->prev_blk_num[i]];
			if (prv->output_disable_flag)
				continue;
			if (run_mode != (run_mode & prv->run_mode))
				continue;

			if (found_prev) {
				if (blk->in_pos_xs != prv->out_pos_xs ||
				    blk->in_pos_xe != prv->out_pos_xe ||
				    blk->h_end_flag != prv->h_end_flag ||
				    blk->tdr_h_disable_flag != prv->tdr_h_disable_flag)
					return T_DIFF_PREV_FORWARD_ERROR;
			} else {
				blk->tdr_h_disable_flag = prv->tdr_h_disable_flag;
				blk->h_end_flag = prv->h_end_flag;
				if (!prv->tdr_h_disable_flag) {
					blk->in_pos_xs = prv->out_pos_xs;
					blk->in_pos_xe = prv->out_pos_xe;
				}
				found_prev = true;
			}
		}
		if (!found_prev)
			return T_DIFF_VIEW_BRANCH_MERGE_ERROR;
	}

	if (blk->tdr_h_disable_flag)
		goto err_skip_x_cal;

	/* check min tile */
	if (blk->in_pos_xs > blk->min_tile_in_pos_xs ||
	    blk->in_pos_xe < blk->min_tile_in_pos_xe) {
		blk->tdr_h_disable_flag = true;
	} else {
		if (blk->enable_flag &&
		    blk->in_pos_xs + blk->in_min_width > blk->in_pos_xe + 1)
			return T_UNDER_MIN_XSIZE_ERROR;
	}

err_skip_x_cal:
	if (reg_map->skip_y_cal)
		goto err_skip_y_cal;

	if (blk->tdr_v_disable_flag)
		goto err_skip_y_cal;

	/* Skip start module */
	if (blk->prev_blk_num[0] != PREVIOUS_BLK_NO_OF_START) {
		bool found_prev = false;

		for (i = 0; i < blk->tot_prev_num; i++) {
			prv = func_param->func_list[blk->prev_blk_num[i]];
			if (prv->output_disable_flag)
				continue;
			if (run_mode != (run_mode & prv->run_mode))
				continue;

			if (found_prev) {
				if (blk->in_pos_ys != prv->out_pos_ys ||
				    blk->in_pos_ye != prv->out_pos_ye ||
				    blk->v_end_flag != prv->v_end_flag ||
				    blk->tdr_v_disable_flag != prv->tdr_v_disable_flag)
					return T_DIFF_PREV_FORWARD_ERROR;
			} else {
				blk->tdr_v_disable_flag = prv->tdr_v_disable_flag;
				blk->v_end_flag = prv->v_end_flag;
				if (!prv->tdr_v_disable_flag) {
					blk->in_pos_ys = prv->out_pos_ys;
					blk->in_pos_ye = prv->out_pos_ye;
				}
				found_prev = true;
			}
		}

		if (!found_prev)
			return T_DIFF_VIEW_BRANCH_MERGE_ERROR;
	}

	if (blk->tdr_v_disable_flag)
		goto err_skip_y_cal;

	/* check min tile */
	if (blk->in_pos_ys > blk->min_tile_in_pos_ys ||
	    blk->in_pos_ye < blk->min_tile_in_pos_ye) {
		blk->tdr_v_disable_flag = true;
	} else {
		if (blk->enable_flag &&
		    blk->in_pos_ys + blk->in_min_height > blk->in_pos_ye + 1)
			return T_UNDER_MIN_YSIZE_ERROR;
	}

err_skip_y_cal:
	return tile_check_input_config(blk, reg_map);
}

static enum mdp_tile_msg tile_init_tdr_ctrl_flag(struct tile_reg_map *reg_map,
						 struct func_description *func_param)
{
	int i;

	if (reg_map->backup_x_skip_y)
		return T_OK;

	if (reg_map->first_frame)
		return T_OK;

	if (!reg_map->tdr_ctrl_en)
		return T_OK;

	for (i = 0; i < func_param->used_func_no; i++) {
		struct tile_func_block *f = func_param->func_list[i];

		if (f->output_disable_flag)
			continue;

		/* init end func */
		if (!f->tot_branch_num) {
			/* init no direct link */
			if (!(f->type & TILE_TYPE_DONT_CARE_END)) {
				if (!reg_map->skip_x_cal)
					f->tdr_h_disable_flag = false;

				if (!reg_map->skip_y_cal)
					f->tdr_v_disable_flag = false;
			}
		} else {
			/* init sub-in */
			if (f->run_mode == TILE_RUN_MODE_SUB_IN) {
				if (!reg_map->skip_x_cal)
					f->tdr_h_disable_flag = false;

				if (!reg_map->skip_y_cal)
					f->tdr_v_disable_flag = false;
			}
		}
	}

	return T_OK;
}

static enum mdp_tile_msg tile_update_last_x_y(struct tile_reg_map *reg_map,
					      struct func_description *func_param,
					      bool x_end_flag, bool y_end_flag)
{
	int v_no = reg_map->curr_vertical_tile_no;
	int h_no = reg_map->curr_horizontal_tile_no;
	int first_frame = reg_map->first_frame;
	int i;

	for (i = 0; i < func_param->used_func_no; i++) {
		struct tile_func_block *f = func_param->func_list[i];

		if (f->output_disable_flag)
			continue;

		if (!f->tdr_v_disable_flag && !f->tdr_h_disable_flag)
			if (!first_frame) /* skip update when frame mode */
				f->last_valid_tile_no = reg_map->used_tile_no;

		if (!y_end_flag && !v_no)
			/* Assign param. pointer if there is enough buffer */
			if (h_no < MAX_TILE_BACKUP_HORZ_NO)
				HORZ_PARA_BACKUP(&f->horz_para[h_no], f);

		/* Update last x/y if not end */
		if (!x_end_flag) {
			if (!f->tdr_h_disable_flag) {
				f->last_input_xs_pos = f->in_pos_xs;
				f->last_input_xe_pos = f->in_pos_xe;

				if (f->valid_h_no) {
					/* Diff view to update min last xs & max last xe */
					if (f->in_pos_xs < f->min_last_input_xs_pos)
						f->min_last_input_xs_pos = f->in_pos_xs;
					if (f->max_last_input_xe_pos < f->in_pos_xe)
						f->max_last_input_xe_pos = f->in_pos_xe;
				} else {
					f->min_last_input_xs_pos = f->in_pos_xs;
					f->max_last_input_xe_pos = f->in_pos_xe;
				}

				f->last_output_xs_pos = f->out_pos_xs;
				f->last_output_xe_pos = f->out_pos_xe;

				/* Skip update when frame mode */
				if (!first_frame)
					f->valid_h_no++;
			}
		} else if (!y_end_flag) {
			f->valid_h_no = 0;
			if (!f->tdr_v_disable_flag) {
				f->last_input_ys_pos = f->in_pos_ys;
				f->last_input_ye_pos = f->in_pos_ye;

				if (f->valid_v_no) {
					if (f->max_last_input_ye_pos < f->in_pos_ye)
						f->max_last_input_ye_pos = f->in_pos_ye;
				} else {
					f->max_last_input_ye_pos = f->in_pos_ye;
				}

				f->last_output_ys_pos = f->out_pos_ys;
				f->last_output_ye_pos = f->out_pos_ye;

				/* Skip update when frame mode */
				if (!first_frame) {
					f->valid_v_no++;
					f->last_valid_v_no = reg_map->curr_vertical_tile_no;
				}
			}
		}
	}

	/* Must clear flag for direct-link test */
	reg_map->backup_x_skip_y = false;
	reg_map->skip_x_cal = false;

	if (x_end_flag) {
		reg_map->curr_horizontal_tile_no = 0;
		reg_map->curr_vertical_tile_no++;
		reg_map->skip_y_cal = false;
	} else {
		reg_map->curr_horizontal_tile_no++;
		reg_map->skip_y_cal = true;
	}

	return T_OK;
}

static enum mdp_tile_msg tile_fwd_output_check(struct tile_func_block *blk,
					       struct tile_reg_map *reg_map)
{
	if (reg_map->skip_x_cal)
		goto err_skip_x_cal;

	if (blk->tdr_h_disable_flag)
		goto err_skip_x_cal;

	/* Check resizer output xe & ye by tile size with enable */
	if (blk->out_tile_width)
		if (blk->out_pos_xe + 1 > blk->out_pos_xs + blk->out_tile_width)
			return T_TILE_FORWARD_OUT_OVER_TILE_WIDTH_ERROR;

	if (!blk->enable_flag)
		goto err_skip_x_cal;

	if (blk->out_pos_xs <= 0) {
		if ((blk->tdr_edge & TILE_EDGE_LEFT_MASK) != TILE_EDGE_LEFT_MASK)
			return T_FWD_CHECK_LEFT_EDGE_ERROR;
	} else {
		if (blk->tdr_edge & TILE_EDGE_LEFT_MASK)
			return T_FWD_CHECK_LEFT_EDGE_ERROR;
	}

	if (blk->out_pos_xe + 1 >= blk->full_size_x_out) {
		if ((blk->tdr_edge & TILE_EDGE_RIGHT_MASK) != TILE_EDGE_RIGHT_MASK)
			return T_FWD_CHECK_RIGHT_EDGE_ERROR;
	} else {
		if (blk->tdr_edge & TILE_EDGE_RIGHT_MASK)
			return T_FWD_CHECK_RIGHT_EDGE_ERROR;
	}

err_skip_x_cal:
	if (reg_map->skip_y_cal)
		goto err_skip_y_cal;

	if (blk->tdr_v_disable_flag)
		goto err_skip_y_cal;

	if (blk->out_tile_height)
		if (blk->out_pos_ye + 1 > blk->out_pos_ys + blk->out_tile_height)
			return T_TILE_FORWARD_OUT_OVER_TILE_HEIGHT_ERROR;

	if (!blk->enable_flag)
		goto err_skip_y_cal;

	if (blk->out_pos_ys == 0) {
		if ((blk->tdr_edge & TILE_EDGE_TOP_MASK) != TILE_EDGE_TOP_MASK)
			return T_FWD_CHECK_TOP_EDGE_ERROR;
	} else {
		if (blk->tdr_edge & TILE_EDGE_TOP_MASK)
			return T_FWD_CHECK_TOP_EDGE_ERROR;
	}

	if (blk->out_pos_ye + 1 >= blk->full_size_y_out) {
		if ((blk->tdr_edge & TILE_EDGE_BOTTOM_MASK) != TILE_EDGE_BOTTOM_MASK)
			return T_FWD_CHECK_BOTTOM_EDGE_ERROR;
	} else {
		if (blk->tdr_edge & TILE_EDGE_BOTTOM_MASK)
			return T_FWD_CHECK_BOTTOM_EDGE_ERROR;
	}

err_skip_y_cal:
	return tile_check_output_config(blk, reg_map);
}

static enum mdp_tile_msg tile_compare_forward_back(struct tile_reg_map *reg_map,
						   struct func_description *func_param)
{
	int i;
	int run_mode = reg_map->run_mode;
	int skip_x_cal = reg_map->skip_x_cal;
	int skip_y_cal = reg_map->skip_y_cal;

	if (reg_map->backup_x_skip_y)
		return T_OK;

	for (i = 0; i < func_param->used_func_no; i++) {
		/* check by forward order */
		unsigned char module_order = func_param->scheduling_forward_order[i];
		struct tile_func_block *f = func_param->func_list[module_order];

		if (f->output_disable_flag)
			continue;

		if (run_mode != f->run_mode)
			continue;

		if (!skip_x_cal && !f->tdr_h_disable_flag)
			if (f->in_pos_xs != f->backward_input_xs_pos ||
			    f->in_pos_xe != f->backward_input_xe_pos ||
			    f->out_pos_xs != f->backward_output_xs_pos ||
			    f->out_pos_xe != f->backward_output_xe_pos ||
			    f->h_end_flag != f->backward_h_end_flag)
				return T_FOR_BACK_COMP_X_ERROR;

		if (!skip_y_cal && !f->tdr_v_disable_flag)
			if (f->in_pos_ys != f->backward_input_ys_pos ||
			    f->in_pos_ye != f->backward_input_ye_pos ||
			    f->out_pos_ys != f->backward_output_ys_pos ||
			    f->out_pos_ye != f->backward_output_ye_pos ||
			    f->v_end_flag != f->backward_v_end_flag)
				return T_FOR_BACK_COMP_Y_ERROR;
	}

	return T_OK;
}

static enum mdp_tile_msg tile_check_x_end_pos_with_flag(struct tile_reg_map *reg_map,
							struct func_description *func_param,
							bool *x_end,
							int curr_tile_no)
{
	int hor_no = reg_map->curr_horizontal_tile_no;
	bool oob_x;
	int xe;
	int i;

	*x_end = func_param->func_list[reg_map->first_func_en_no]->h_end_flag;
	if (reg_map->skip_x_cal)
		goto err_return;

	if (hor_no && !(*x_end))
		goto err_return;

	for (i = 0; i < func_param->used_func_no; i++) {
		struct tile_func_block *f = func_param->func_list[i];

		if (f->output_disable_flag)
			continue;

		/* check first frame */
		if (reg_map->first_frame) {
			/* not direct link */
			if (!(f->type & TILE_TYPE_DONT_CARE_END) &&
			    !f->tot_branch_num &&
			    f->run_mode == TILE_RUN_MODE_MAIN) {
				if (f->min_out_pos_xs < f->out_pos_xs ||
				    f->out_pos_xe < f->max_out_pos_xe)
					return T_TILE_X_DIR_NOT_END_TOGETHER_ERROR;
			}
		} else {
			/* check only run mode & end functions */
			if (f->run_mode != TILE_RUN_MODE_MAIN)
				continue;

			if (f->in_cal_order & TILE_ORDER_RIGHT_TO_LEFT) {
				oob_x = f->in_pos_xe < f->max_in_pos_xe;
				if (!f->valid_h_no && oob_x)
					return T_TILE_X_DIR_NOT_END_TOGETHER_ERROR;
				if (!(*x_end))
					continue;
				oob_x = f->min_in_pos_xs < f->in_pos_xs;
				if (!(oob_x && f->valid_h_no))
					continue;

				/* diff view to check min last xs */
				if (f->min_in_pos_xs < f->min_last_input_xs_pos)
					return T_TILE_X_DIR_NOT_END_TOGETHER_ERROR;
			} else {
				oob_x = f->min_in_pos_xs < f->in_pos_xs;
				if (!hor_no && oob_x)
					return T_TILE_X_DIR_NOT_END_TOGETHER_ERROR;

				xe = f->max_in_pos_xe;
				if (*x_end && f->in_pos_xe >= xe)
					continue;

				/* diff view to check max last xe */
				if (f->valid_h_no && f->max_last_input_xe_pos < xe)
					return T_TILE_X_DIR_NOT_END_TOGETHER_ERROR;
			}
		}
	}

	/* set title h no once */
	if (*x_end && !reg_map->horizontal_tile_no)
		reg_map->horizontal_tile_no = curr_tile_no + 1;

err_return:
	return T_OK;
}

static enum mdp_tile_msg tile_check_y_end_pos_with_flag(struct tile_reg_map *reg_map,
							struct func_description *func_param,
							bool *y_end)
{
	int cur_v_tile_no = reg_map->curr_vertical_tile_no;
	int i;

	*y_end = func_param->func_list[reg_map->first_func_en_no]->v_end_flag;
	if (reg_map->skip_y_cal)
		goto err_return;

	if (cur_v_tile_no && !(*y_end))
		goto err_return;

	for (i = 0; i < func_param->used_func_no; i++) {
		struct tile_func_block *f = func_param->func_list[i];
		bool oob_y;

		if (f->output_disable_flag)
			continue;

		/* check first frame */
		if (reg_map->first_frame) {
			/* not direct link */
			if (!(f->type & TILE_TYPE_DONT_CARE_END) &&
			    !f->tot_branch_num &&
			    f->run_mode == TILE_RUN_MODE_MAIN) {
				if (f->out_pos_ye < f->max_out_pos_ye ||
				    f->out_pos_ys > f->min_out_pos_ys)
					return T_TILE_Y_DIR_NOT_END_TOGETHER_ERROR;
			}
		} else {
			/* check only run mode & end functions */
			if (f->run_mode != TILE_RUN_MODE_MAIN)
				continue;

			/* check output y size is end */
			oob_y = f->in_pos_ys > f->min_in_pos_ys;
			if (!f->valid_v_no && oob_y)
				return T_TILE_Y_DIR_NOT_END_TOGETHER_ERROR;

			if (*y_end && f->in_pos_ye < f->max_in_pos_ye) {
				/* diff view to check min last ye */
				oob_y = f->max_last_input_ye_pos < f->max_in_pos_ye;
				if (f->valid_v_no && oob_y)
					return T_TILE_Y_DIR_NOT_END_TOGETHER_ERROR;
			}
		}
	}

err_return:
	return T_OK;
}
