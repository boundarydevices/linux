/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MDP_TILE_COMP_H__
#define __MDP_TILE_COMP_H__

/* prototype init */
enum mdp_tile_msg tile_rdma_init(struct tile_func_block *func,
				 struct tile_reg_map *reg_map);
enum mdp_tile_msg tile_prz_init(struct tile_func_block *func,
				struct tile_reg_map *reg_map);
enum mdp_tile_msg tile_tdshp_init(struct tile_func_block *func,
				  struct tile_reg_map *reg_map);
enum mdp_tile_msg tile_wrot_init(struct tile_func_block *func,
				 struct tile_reg_map *reg_map);
/* prototype for */
enum mdp_tile_msg tile_rdma_for(struct tile_func_block *func,
				struct tile_reg_map *reg_map);
enum mdp_tile_msg tile_crop_for(struct tile_func_block *func,
				struct tile_reg_map *reg_map);
enum mdp_tile_msg tile_aal_for(struct tile_func_block *func,
			       struct tile_reg_map *reg_map);
enum mdp_tile_msg tile_prz_for(struct tile_func_block *func,
			       struct tile_reg_map *reg_map);
enum mdp_tile_msg tile_wrot_for(struct tile_func_block *func,
				struct tile_reg_map *reg_map);
/* prototype back */
enum mdp_tile_msg tile_rdma_back(struct tile_func_block *func,
				 struct tile_reg_map *reg_map);
enum mdp_tile_msg tile_prz_back(struct tile_func_block *func,
				struct tile_reg_map *reg_map);
enum mdp_tile_msg tile_wrot_back(struct tile_func_block *func,
				 struct tile_reg_map *reg_map);
#endif /* __MDP_TILE_COMP_H__ */
