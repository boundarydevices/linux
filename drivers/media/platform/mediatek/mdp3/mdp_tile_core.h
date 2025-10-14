/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MDP_TILE_CORE_H__
#define __MDP_TILE_CORE_H__

#include <linux/types.h>
#include "mtk-mdp3-alg.h"

#define MAX_TILE_PREV_NO 10
#define MAX_TILE_BRANCH_NO 6
#define MIN_TILE_FUNC_NO 2
/* smaller or equal to PREVIOUS_BLK_NO_OF_START - 1 */
#define MAX_TILE_FUNC_NO MDP_ALG_MAX_PATH_NODES
#define MAX_INPUT_TILE_FUNC_NO 32
#define MAX_FORWARD_FUNC_CAL_LOOP_NO (16 * MAX_TILE_FUNC_NO)
#define MAX_TILE_FUNC_NAME_SIZE 32
#define MAX_TILE_TOT_NO 1200

#define TILE_MOD(num, denom) (((denom) == 1) ? 0 : \
	(((denom) == 2) ? ((num) & 0x1) : \
	(((denom) == 4) ? ((num) & 0x3) : \
	(((denom) == 8) ? ((num) & 0x7) : ((num) % (denom))))))
#define TILE_INT_DIV(num, denom) (((denom) == 1) ? (num) : \
	(((denom) == 2) ? ((unsigned int)(num) >> 1) : \
	(((denom) == 4) ? ((unsigned int)(num) >> 2) : \
	(((denom) == 8) ? ((unsigned int)(num) >> 3) : ((num) / (denom))))))

#define TILE_ORDER_Y_FIRST (0x1)
#define TILE_ORDER_RIGHT_TO_LEFT (0x2)
#define TILE_ORDER_BOTTOM_TO_TOP (0x4)

#define PREVIOUS_BLK_NO_OF_START 0xFF

/* MAX TILE WIDTH & HEIGHT */
#define MAX_SIZE 65536

/* TILE HORIZONTAL BUFFER */
#define MAX_TILE_BACKUP_HORZ_NO 24

/* Tile edge */
#define TILE_EDGE_BOTTOM_MASK (0x8)
#define TILE_EDGE_TOP_MASK (0x4)
#define TILE_EDGE_RIGHT_MASK (0x2)
#define TILE_EDGE_LEFT_MASK (0x1)
#define TILE_EDGE_HORZ_MASK (TILE_EDGE_RIGHT_MASK + TILE_EDGE_LEFT_MASK)

#define LAST_MODULE_ID_OF_START (0x0fffffff)

/* tile type: 0x1 non-fixed func to configure, 0x2 rdma, 0x4 wdma, 0x8 crop_en */
#define TILE_TYPE_LOSS (0x1)
#define TILE_TYPE_RDMA (0x2)
#define TILE_TYPE_WDMA (0x4)
#define TILE_TYPE_CROP_EN (0x8)
#define TILE_TYPE_DONT_CARE_END (0x10) /* used by mdp & sub_out*/

#define HORZ_PARA_BACKUP(a, b) do { \
	(a)->tdr_edge = (b)->tdr_edge; \
	/* diff view cal, to reset */ \
	(a)->tdr_h_disable_flag = (b)->tdr_h_disable_flag; \
	(a)->h_end_flag = (b)->h_end_flag; \
	(a)->in_pos_xs = (b)->in_pos_xs; \
	(a)->in_pos_xe = (b)->in_pos_xe; \
	/* backward boundary for ufd smt */ \
	(a)->out_pos_xs = (b)->out_pos_xs; \
	(a)->out_pos_xe = (b)->out_pos_xe; \
	/* diff view cal, to reset */ \
	(a)->bias_x = (b)->bias_x; \
	(a)->offset_x = (b)->offset_x; \
	(a)->bias_x_c = (b)->bias_x_c; \
	(a)->offset_x_c = (b)->offset_x_c; \
	/* diff view tdr used with backup */ \
	(a)->backward_tdr_h_disable_flag = (b)->backward_tdr_h_disable_flag; \
	/* diff view tdr used with backup */ \
	(a)->backward_h_end_flag = (b)->backward_h_end_flag; \
} while (0)

#define HORZ_PARA_RESTORE(a, b) do { \
	(b)->tdr_edge = (((b)->tdr_edge) & (~TILE_EDGE_HORZ_MASK & 0xf)) | \
			(((a)->tdr_edge) & TILE_EDGE_HORZ_MASK); \
	/* diff view cal, to reset */ \
	(b)->tdr_h_disable_flag = (a)->tdr_h_disable_flag; \
	(b)->h_end_flag = (a)->h_end_flag; \
	(b)->in_pos_xs = (a)->in_pos_xs; \
	(b)->in_pos_xe = (a)->in_pos_xe; \
	/* backward boundary for ufd smt */ \
	(b)->out_pos_xs = (a)->out_pos_xs; \
	(b)->out_pos_xe = (a)->out_pos_xe; \
	/* diff view cal, to reset */ \
	(b)->bias_x = (a)->bias_x; \
	(b)->offset_x = (a)->offset_x; \
	(b)->bias_x_c = (a)->bias_x_c; \
	(b)->offset_x_c = (a)->offset_x_c; \
	/* diff view tdr used with backup */ \
	(b)->backward_tdr_h_disable_flag = (a)->backward_tdr_h_disable_flag; \
	/* diff view tdr used with backup */ \
	(b)->backward_h_end_flag = (a)->backward_h_end_flag; \
} while (0)

#define TILE_COPY_SRC_ORDER(a, b) do { \
	(a)->in_stream_order = (b)->src_stream_order; \
	(a)->in_cal_order = (b)->src_cal_order; \
	(a)->in_dump_order = (b)->src_dump_order; \
	(a)->out_stream_order = (b)->src_stream_order; \
	(a)->out_cal_order = (b)->src_cal_order; \
	(a)->out_dump_order = (b)->src_dump_order; \
} while (0)

#define TILE_COPY_PRE_ORDER(a, b) do { \
	(a)->in_stream_order = (b)->out_stream_order; \
	(a)->in_cal_order = (b)->out_cal_order; \
	(a)->in_dump_order = (b)->out_dump_order; \
	(a)->out_stream_order = (b)->out_stream_order; \
	(a)->out_cal_order = (b)->out_cal_order; \
	(a)->out_dump_order = (b)->out_dump_order; \
} while (0)

enum tile_run_mode_enum {
	TILE_RUN_MODE_SUB_OUT = 0x1,
	TILE_RUN_MODE_SUB_IN = TILE_RUN_MODE_SUB_OUT + 0x2,
	TILE_RUN_MODE_MAIN = TILE_RUN_MODE_SUB_IN + 0x4
};

/* error enum */
enum mdp_tile_msg {
	T_OK = 0,
	T_UNKNOWN,
	T_OVER_MAX_MASK_WORD_NO_ERROR,
	T_OVER_MAX_TILE_WORD_NO_ERROR,
	T_OVER_MAX_TILE_TOT_NO_ERROR,
	T_UNDER_MIN_TILE_FUNC_NO_ERROR,
	T_DUPLICATED_SUPPORT_FUNC_ERROR,
	T_OVER_MAX_BRANCH_NO_ERROR,
	T_OVER_MAX_INPUT_TILE_FUNC_NO_ERROR,
	T_TILE_FUNC_CANNOT_FIND_LAST_FUNC_ERROR,
	T_SCHEDULING_BACKWARD_ERROR,
	T_SCHEDULING_FORWARD_ERROR,
	T_IN_CONST_X_ERROR,
	T_IN_CONST_Y_ERROR,
	T_OUT_CONST_X_ERROR,
	T_OUT_CONST_Y_ERROR,
	T_INIT_INCOR_X_INPUT_SIZE_POS_ERROR,
	T_INIT_INCOR_Y_INPUT_SIZE_POS_ERROR,
	T_INIT_INCOR_X_OUTPUT_SIZE_POS_ERROR,
	T_INIT_INCOR_Y_OUTPUT_SIZE_POS_ERROR,
	T_TILE_X_DIR_NOT_END_TOGETHER_ERROR,
	T_TILE_Y_DIR_NOT_END_TOGETHER_ERROR,
	T_INCOR_XE_INPUT_POS_REDUCED_BY_TILE_SIZE_ERROR,
	T_INCOR_YE_INPUT_POS_REDUCED_BY_TILE_SIZE_ERROR,
	T_FWD_FUNC_CAL_LOOP_COUNT_OVER_MAX_ERROR,
	T_TILE_LOSS_OVER_TILE_HEIGHT_ERROR,
	T_TILE_LOSS_OVER_TILE_WIDTH_ERROR,
	T_TP6_FOR_INVALID_OUT_XYS_XYE_ERROR,
	T_TP4_FOR_INVALID_OUT_XYS_XYE_ERROR,
	T_SRC_ACC_FOR_INVALID_OUT_XYS_XYE_ERROR,
	T_CUB_ACC_FOR_INVALID_OUT_XYS_XYE_ERROR,
	T_RECURSIVE_FOUND_ERROR,
	T_CHECK_IN_CONFIG_ALIGN_XS_POS_ERROR,
	T_CHECK_IN_CONFIG_ALIGN_XE_POS_ERROR,
	T_CHECK_IN_CONFIG_ALIGN_YS_POS_ERROR,
	T_CHECK_IN_CONFIG_ALIGN_YE_POS_ERROR,
	T_XSIZE_NOT_DIV_BY_IN_CONST_X_ERROR,
	T_YSIZE_NOT_DIV_BY_IN_CONST_Y_ERROR,
	T_XSIZE_NOT_DIV_BY_OUT_CONST_X_ERROR,
	T_YSIZE_NOT_DIV_BY_OUT_CONST_Y_ERROR,
	T_TILE_FORWARD_OUT_OVER_TILE_WIDTH_ERROR,
	T_TILE_FORWARD_OUT_OVER_TILE_HEIGHT_ERROR,
	T_TILE_BACKWARD_IN_OVER_TILE_WIDTH_ERROR,
	T_TILE_BACKWARD_IN_OVER_TILE_HEIGHT_ERROR,
	T_FWD_CHECK_TOP_EDGE_ERROR,
	T_FWD_CHECK_BOTTOM_EDGE_ERROR,
	T_FWD_CHECK_LEFT_EDGE_ERROR,
	T_FWD_CHECK_RIGHT_EDGE_ERROR,
	T_BWD_CHECK_TOP_EDGE_ERROR,
	T_BWD_CHECK_BOTTOM_EDGE_ERROR,
	T_BWD_CHECK_LEFT_EDGE_ERROR,
	T_BWD_CHECK_RIGHT_EDGE_ERROR,
	T_CHECK_OUT_CONFIG_ALIGN_XS_POS_ERROR,
	T_CHECK_OUT_CONFIG_ALIGN_XE_POS_ERROR,
	T_CHECK_OUT_CONFIG_ALIGN_YS_POS_ERROR,
	T_CHECK_OUT_CONFIG_ALIGN_YE_POS_ERROR,
	T_DISABLE_FUNC_X_SIZE_CHECK_ERROR,
	T_DISABLE_FUNC_Y_SIZE_CHECK_ERROR,
	T_OUTPUT_DISABLE_INPUT_FUNC_CHECK_ERROR,
	T_RESIZER_SRC_ACC_SCALING_UP_ERROR,
	T_RESIZER_CUBIC_ACC_SCALING_UP_ERROR,
	T_INCOR_END_FUNC_TYPE_ERROR,
	T_INCOR_START_FUNC_TYPE_ERROR,
	/* tdr sort check */
	T_INCOR_ORDER_CONFIG_ERROR,
	/* multi-input flow error check */
	T_TWO_MAIN_PREV_ERROR,
	T_TWO_START_PREV_ERROR,
	T_NO_MAIN_OUTPUT_ERROR,
	T_DIFF_PREV_CONFIG_ERROR,
	T_DIFF_NEXT_CONFIG_ERROR,
	T_TWO_MAIN_START_ERROR,
	T_DIFF_PREV_FORWARD_ERROR,
	T_FOR_BACK_COMP_X_ERROR,
	T_FOR_BACK_COMP_Y_ERROR,
	T_BROKEN_SUB_PATH_ERROR,
	T_MIX_SUB_IN_OUT_PATH_ERROR,
	T_INVALID_SUB_IN_CONFIG_ERROR,
	T_INVALID_SUB_OUT_CONFIG_ERROR,
	T_UNKNOWN_RUN_MODE_ERROR,
	/* diff view check */
	T_DIFF_VIEW_BRANCH_MERGE_ERROR,
	T_DIFF_VIEW_INPUT_ERROR,
	T_DIFF_VIEW_OUTPUT_ERROR,
	T_FRAME_MODE_NOT_END_ERROR,
	T_DIFF_VIEW_TILE_WIDTH_ERROR,
	T_DIFF_VIEW_TILE_HEIGHT_ERROR,
	/* min size constraints */
	T_UNDER_MIN_XSIZE_ERROR,
	T_UNDER_MIN_YSIZE_ERROR,
	/* MDP ERROR MESSAGE DATA */
	M_BACKWARD_START_LESS_THAN_FORWARD,
	/* PRZ check */
	M_RESIZER_SCALING_ERROR,
	/* General status */
	M_NULL_DATA,
	M_INVALID_STATE,
	M_UNKNOWN_ERROR,
	/* final count, can not be changed */
	T_TILE_MAX_NO
};

struct tile_horz_backup {
	unsigned char tdr_edge;
	/* diff view cal, to reset */
	bool tdr_h_disable_flag;
	bool h_end_flag;
	int in_pos_xs;
	int in_pos_xe;
	/* backward boundary for ufd smt */
	int out_pos_xs;
	int out_pos_xe;
	/* diff view cal, to reset */
	int bias_x;
	int offset_x;
	int bias_x_c;
	int offset_x_c;
	/* diff view tdr used with backup */
	bool backward_tdr_h_disable_flag;
	/* diff view tdr used with backup */
	bool backward_h_end_flag;
};

/* tile reg & variable */
struct tile_reg_map {
	int skip_x_cal;
	int skip_y_cal;
	int backup_x_skip_y;
	int tdr_ctrl_en;
	int run_mode;
	/* frame mode flag */
	int first_frame; /* first frame to run frame mode */
	int curr_vertical_tile_no;
	int horizontal_tile_no;
	int curr_horizontal_tile_no;
	int used_tile_no;
	int valid_tile_no;
	/* tile cal & dump order flag */
	unsigned int src_stream_order; /* keep isp src_stream_order */
	unsigned int src_cal_order; /* copy RDMA in_cal_order */
	unsigned int src_dump_order; /* copy RDMA in_dump_order */
	/* sub mode */
	int found_sub_in;
	int found_sub_out;
	/* frame mode flag */
	int first_pass; /* first pass to run min edge & min tile cal */
	int first_func_en_no;
	int last_func_en_no;
	/* debug mode with invalid offset to enable recursive forward */
	int recursive_forward_en;
};

/* self reference type */
struct tile_func_block {
	int func_num;
	char func_name[MAX_TILE_FUNC_NAME_SIZE];
	enum tile_run_mode_enum run_mode;
	bool enable_flag;
	/* output_disable = false by tile_init_config() */
	bool output_disable_flag;
	unsigned char tdr_edge;
	unsigned char tot_branch_num;
	unsigned char next_blk_num[MAX_TILE_BRANCH_NO];
	unsigned char tot_prev_num;
	unsigned char prev_blk_num[MAX_TILE_PREV_NO];
	/* diff view cal, to reset */
	bool tdr_h_disable_flag;
	bool h_end_flag;
	bool crop_h_end_flag;
	int in_pos_xs;
	int in_pos_xe;
	int full_size_x_in;
	int in_tile_width;
	/* backward boundary */
	int in_tile_width_max;
	int in_tile_width_max_str;
	int in_tile_width_max_end;
	/* backward boundary for ufd smt */
	int in_tile_width_loss;
	int in_max_width;
	int in_min_width;
	int in_log_width;
	int out_pos_xs;
	int out_pos_xe;
	int full_size_x_out;
	int out_tile_width;
	/* backward boundary */
	int out_tile_width_max;
	int out_tile_width_max_str;
	int out_tile_width_max_end;
	/* backward boundary for ufd smt */
	int out_tile_width_loss;
	int out_max_width;
	int out_log_width;
	bool max_h_edge_flag;
	unsigned char in_const_x;
	unsigned char out_const_x;
	/* diff view cal, to reset */
	bool tdr_v_disable_flag;
	bool v_end_flag;
	bool crop_v_end_flag;
	int in_pos_ys;
	int in_pos_ye;
	int full_size_y_in;
	int in_tile_height;
	/* backward boundary */
	int in_tile_height_max;
	int in_tile_height_max_str;
	int in_tile_height_max_end;
	int in_max_height;
	int in_min_height;
	int in_log_height;
	int out_pos_ys;
	int out_pos_ye;
	int full_size_y_out;
	int out_tile_height;
	/* backward boundary */
	int out_tile_height_max;
	int out_tile_height_max_str;
	int out_tile_height_max_end;
	int out_max_height;
	int out_log_height;
	bool max_v_edge_flag;
	unsigned char in_const_y;
	unsigned char out_const_y;
	int min_in_pos_xs;
	int max_in_pos_xe;
	int min_out_pos_xs;
	int max_out_pos_xe;
	/* backward boundary for ufd smt */
	int min_in_crop_xs;
	int max_in_crop_xe;
	int min_out_crop_xs;
	int max_out_crop_xe;
	int min_in_pos_ys;
	int max_in_pos_ye;
	int min_out_pos_ys;
	int max_out_pos_ye;
	/* diff view cal, to reset */
	int valid_h_no;
	int valid_v_no;
	int last_valid_tile_no;
	int last_valid_v_no;
	int bias_x;
	int offset_x;
	int bias_x_c;
	int offset_x_c;
	int bias_y;
	int offset_y;
	int bias_y_c;
	int offset_y_c;
	unsigned char l_tile_loss;
	unsigned char r_tile_loss;
	unsigned char t_tile_loss;
	unsigned char b_tile_loss;
	int crop_bias_x;
	int crop_offset_x;
	int crop_bias_y;
	int crop_offset_y;
	int backward_input_xs_pos;
	int backward_input_xe_pos;
	int backward_output_xs_pos;
	int backward_output_xe_pos;
	int backward_input_ys_pos;
	int backward_input_ye_pos;
	int backward_output_ys_pos;
	int backward_output_ye_pos;
	int last_input_xs_pos;
	int last_input_xe_pos;
	int last_output_xs_pos;
	int last_output_xe_pos;
	int last_input_ys_pos;
	int last_input_ye_pos;
	int last_output_ys_pos;
	int last_output_ye_pos;
	unsigned char type;
	unsigned int in_stream_order;
	unsigned int out_stream_order;
	unsigned int in_cal_order;
	unsigned int out_cal_order;
	unsigned int in_dump_order;
	unsigned int out_dump_order;
	/* diff view cal */
	int min_tile_in_pos_xs;
	int min_tile_in_pos_xe;
	int min_tile_out_pos_xs;
	int min_tile_out_pos_xe;
	int min_tile_crop_in_pos_xs;
	int min_tile_crop_in_pos_xe;
	int min_tile_crop_out_pos_xs;
	int min_tile_crop_out_pos_xe;
	int min_tile_in_pos_ys;
	int min_tile_in_pos_ye;
	int min_tile_out_pos_ys;
	int min_tile_out_pos_ye;
	bool min_cal_tdr_h_disable_flag;
	/* diff view log */
	bool min_cal_h_end_flag;
	bool min_cal_max_h_edge_flag;
	bool min_cal_tdr_v_disable_flag;
	bool min_cal_v_end_flag;
	bool min_cal_max_v_edge_flag;
	/* DL interface */
	bool direct_h_end_flag;
	bool direct_v_end_flag;
	int direct_out_pos_xs;
	int direct_out_pos_xe;
	int direct_out_pos_ys;
	int direct_out_pos_ye;
	/* diff view tdr used with backup */
	bool backward_tdr_h_disable_flag;
	bool backward_tdr_v_disable_flag;
	/* diff view tdr used with backup */
	bool backward_h_end_flag;
	bool backward_v_end_flag;
	int in_tile_width_backup;
	int out_tile_width_backup;
	int in_tile_height_backup;
	int out_tile_height_backup;
	int min_last_input_xs_pos;
	int max_last_input_xe_pos;
	int max_last_input_ye_pos;
	int last_func_num[MAX_TILE_PREV_NO];
	int next_func_num[MAX_TILE_BRANCH_NO];
	struct tile_horz_backup horz_para[MAX_TILE_BACKUP_HORZ_NO];
	enum mdp_tile_msg (*init_func)(struct tile_func_block *func,
				       struct tile_reg_map *reg_map);
	enum mdp_tile_msg (*for_func)(struct tile_func_block *func,
				      struct tile_reg_map *reg_map);
	enum mdp_tile_msg (*back_func)(struct tile_func_block *func,
				       struct tile_reg_map *reg_map);
	union mdl_alg_tile_data *data;
};

/* tile function structure */
struct func_description {
	unsigned char used_func_no;
	unsigned int valid_flag[(MAX_TILE_FUNC_NO + 31) / 32];
	unsigned int for_recursive_count;
	unsigned char scheduling_forward_order[MAX_TILE_FUNC_NO];
	unsigned char scheduling_backward_order[MAX_TILE_FUNC_NO];
	struct tile_func_block *func_list[MAX_TILE_FUNC_NO];
};

/* tile interface */
enum mdp_tile_msg tile_convert_func(struct tile_reg_map *reg_map,
				    struct func_description *func_param,
				    struct mdp_alg_path_tp *path);
enum mdp_tile_msg tile_init_config(struct tile_reg_map *reg_map,
				   struct func_description *func_param);
enum mdp_tile_msg tile_frame_mode_init(struct tile_reg_map *reg_map,
				       struct func_description *func_param);
enum mdp_tile_msg tile_frame_mode_close(struct tile_reg_map *reg_map,
					struct func_description *func_param);
enum mdp_tile_msg tile_mode_init(struct tile_reg_map *reg_map,
				 struct func_description *func_param);
enum mdp_tile_msg tile_proc_main_single(struct tile_reg_map *reg_map,
					struct func_description *func_param,
					int tile_no, bool *stop_flag);

#endif /* __MDP_TILE_CORE_H__ */
