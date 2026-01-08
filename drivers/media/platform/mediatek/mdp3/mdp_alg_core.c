// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */
#include <linux/of.h>
#include <linux/of_address.h>
#include "mtk-mdp3-alg.h"
#include "mtk-mdp3-cmdq.h"
#include "mtk-mdp3-core.h"
#include "mtk-mdp3-m2m.h"

const struct mdp_alg_comp_ops *mdp_alg_ops[MDP_COMP_TYPE_COUNT] = {
	[MDP_COMP_TYPE_RDMA] =	&alg_ops_rdma,
	[MDP_COMP_TYPE_RSZ] =	&alg_ops_rsz,
	[MDP_COMP_TYPE_WROT] =	&alg_ops_wrot,
};

struct mdp_alg_cmdq {
	struct work_struct release_work;
	struct cmdq_cb_data *data;
	struct mdp_alg_task *task;
	struct cmdq_pkt pkt;
	struct mdp_comp *comps;
	void *ctx;
	enum mdp_cmdq_user user;
	u8 comp_num;
	u8 pipe_num;
	u8 pipe_idx;
};

static struct mdp_alg_cmdq *prepare_cmdq(struct mdp_alg_task *task, u32 pipe)
{
	struct mdp_alg_path_tp *path = &task->cfg.path[pipe];
	struct cmdq_client *clt = task->mdp->cmdq_clt[pipe];
	struct mdp_alg_cmdq *cmd;
	struct mdp_comp *comps;
	struct mdp_m2m_ctx *ctx = (struct mdp_m2m_ctx *)task->ctx;
	u32 node_cnt = path->node_cnt;
	int ret, i;

	ret = mbox_flush(clt->chan, 0);
	if (ret)
		goto err_prepare_cmdq;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd)
		goto err_prepare_cmdq;

	ret = mdp_cmdq_pkt_create(clt, &cmd->pkt, SZ_16K);
	if (ret)
		goto err_prepare_pkt;
	task->pkts[pipe] = &cmd->pkt;

	comps = kcalloc(node_cnt, sizeof(*comps), GFP_KERNEL);
	if (!comps)
		goto err_prepare_comp;

	for (i = 0; i < path->node_cnt; i++)
		memcpy(&comps[i], path->nodes[i].comp, sizeof(struct mdp_comp));

	cmd->task = task;
	cmd->comps = comps;
	cmd->ctx = ctx;
	cmd->user = MDP_CMDQ_USER_ALG;
	cmd->comp_num = node_cnt;
	cmd->pipe_num = task->cfg.dual ? 2 : 1;
	cmd->pipe_idx = pipe;

	return cmd;

err_prepare_comp:
	mdp_cmdq_pkt_destroy(&cmd->pkt);
err_prepare_pkt:
	kfree(cmd);
err_prepare_cmdq:
	cmd = NULL;
	return cmd;
}

static int prepare_comp(struct mdp_alg_task *task, u32 pipe)
{
	struct mdp_alg_path_tp *path = &task->cfg.path[pipe];
	int ret, i;

	for (i = 0; i < path->node_cnt; i++) {
		struct mdp_comp *comp = path->nodes[i].comp;

		ret = call_alg_op(comp, comp_prepare, task, path, i);
		if (ret) {
			dev_err(&task->mdp->pdev->dev, "comp_prepare fail %d",
				comp->public_id);
			break;
		}
	}

	return ret;
}

static int config_comp(struct mdp_alg_task *task, u32 pipe)
{
	struct mdp_alg_path_tp *path = &task->cfg.path[pipe];
	struct device *dev = &task->mdp->pdev->dev;
	struct mdp_comp *comp;
	struct cmdq_pkt *pkt = task->pkts[pipe];
	struct device *mmsys = task->mdp->mm_subsys[path->mmsys_idx].mmsys;
	struct platform_device *pdev = to_platform_device(mmsys);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	int ret, i;

	if (!res)
		return -ENODEV;

	/* Disable MDPSYS shadow */
	cmdq_pkt_write_value_addr(pkt, res->start + 0xf00, BIT(0), BIT(0));

	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;

		ret = call_alg_op(comp, comp_init, task, path, i);
		if (ret) {
			dev_err(dev, "comp_init fail %d", comp->public_id);
			break;
		}

		ret = call_alg_op(comp, config_frame, task, path, i);
		if (ret) {
			dev_err(dev, "comp_frame fail %d", comp->public_id);
			break;
		}
	}

	return ret;
}

static int config_path_clk_ctrl(struct mdp_alg_task *task, u32 pipe, bool clear)
{
	struct mdp_alg_path_tp *path = &task->cfg.path[pipe];
	struct device *dev = &task->mdp->pdev->dev;
	struct mtk_mutex *mutex;
	int ret, i;

	mutex = task->mdp->mm_subsys[path->mmsys_idx].mdp_mutex[path->mutex_idx];

	if (clear)
		mtk_mutex_unprepare(mutex);

	for (i = 0; i < path->node_cnt; i++) {
		struct mdp_comp *comp = path->nodes[i].comp;

		if (clear)
			mdp_comp_clock_off(dev, comp);
		else
			mdp_comp_clock_on(dev, comp);
	}

	if (!clear)
		mtk_mutex_prepare(mutex);

	return 0;
}

static int config_mutex_mod_ctrl(struct mdp_alg_task *task, u32 pipe, bool clear)
{
	struct mdp_alg_path_tp *path = &task->cfg.path[pipe];
	struct mtk_mutex *mutex;
	const u32 *mutex_idx = task->mdp->mdp_data->mdp_mutex_table_idx;
	int ret, i;

	mutex = task->mdp->mm_subsys[path->mmsys_idx].mdp_mutex[path->mutex_idx];
	for (i = 0; i < path->node_cnt; i++) {
		struct mdp_comp *comp = path->nodes[i].comp;
		u32 id = comp->public_id;

		ret = mtk_mutex_write_mod(mutex, mutex_idx[id], clear);
		if (ret)
			return ret;
	}

	return 0;
}

static int config_mmsys_mux_ctrl(struct mdp_alg_task *task, u32 pipe, bool clear)
{
	struct mdp_alg_path_tp *path = &task->cfg.path[pipe];
	struct cmdq_pkt *pkt = task->pkts[pipe];
	struct device *mmsys = task->mdp->mm_subsys[path->mmsys_idx].mmsys;
	struct platform_device *pdev = to_platform_device(mmsys);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	int i, out, idx;
	int sof;

	if (!res)
		return -ENODEV;

	sof = path->mutex_idx << 8;

	for (i = 0; i < path->node_cnt; i++) {
		for (out = 0; out < MDP_ALG_MAX_OUTPUTS; out++) {
			struct mdp_alg_path_mux *m = path->nodes[i].mux[out];

			for (idx = 0; idx < MDP_ALG_MAX_MUX; idx++) {
				u16 ofst = m[idx].ofst;
				u16 value = clear ? 0 : m[idx].val;
				u32 mask = U32_MAX;

				if (ofst == 0)
					continue;
				if (m[idx].type == MDP_MUX_MOUT && !clear) {
					value = 1 << value;
					mask = value;
				}
				cmdq_pkt_write_value_addr(pkt, res->start + ofst,
							  value + sof, mask);
			}
		}
	}

	return 0;
}

static int config_tile_run(struct mdp_alg_task *task, u32 pipe)
{
	struct mdp_alg_path_tp *path = &task->cfg.path[pipe];
	struct mdp_comp *comp;
	struct mtk_mutex *m;
	int ret, tile, i;
	int tile_cnt = task->cfg.tile[pipe]->tile_cnt;

	m = task->mdp->mm_subsys[path->mmsys_idx].mdp_mutex[path->mutex_idx];

	for (tile = 0; tile < tile_cnt; tile++) {
		ret = config_mutex_mod_ctrl(task, pipe, false);
		if (ret)
			return ret;

		mtk_mutex_write_sof(m, MUTEX_SOF_IDX_SINGLE_MODE);

		ret = config_mmsys_mux_ctrl(task, pipe, false);
		if (ret)
			return ret;

		for (i = 0; i < path->node_cnt; i++) {
			comp = path->nodes[i].comp;
			ret = call_alg_op(comp, config_tile, task, path, i, tile);
			if (ret)
				return ret;
		}

		mtk_mutex_enable_by_cmdq(m, (void *)task->pkts[pipe]);

		for (i = 0; i < path->node_cnt; i++) {
			comp = path->nodes[i].comp;
			ret = call_alg_op(comp, cmdq_wait, task, path, i);
			if (ret)
				return ret;
		}

		ret = config_mmsys_mux_ctrl(task, pipe, true);
		if (ret)
			return ret;
	}

	return ret;
}

static void cmdq_release_work(struct mdp_alg_cmdq *cmd)
{
	struct mdp_alg_task *task;
	struct mdp_alg_path_tp *path;
	struct mdp_dev *mdp;
	struct device *dev;
	int vb_state = VB2_BUF_STATE_DONE;
	int i, c;

	if (!cmd)
		return;

	task = cmd->task;
	path = &task->cfg.path[cmd->pipe_idx];
	mdp = task->mdp;
	dev = &task->mdp->pdev->dev;

	if (cmd->data->sta < 0)
		vb_state = VB2_BUF_STATE_ERROR;

	config_mutex_mod_ctrl(task, cmd->pipe_idx, true);
	config_path_clk_ctrl(task, cmd->pipe_idx, true);

	kfree(cmd->comps);
	cmd->comps = NULL;

	mdp_cmdq_pkt_destroy(&cmd->pkt);
	for (i = 0; i < path->node_cnt; i++) {
		kfree(path->nodes[i].data);
		path->nodes[i].data = NULL;
	}

	kfree(task->cfg.tile[path->path_id]);
	task->cfg.tile[path->path_id] = NULL;

	c = atomic_read(&mdp->job_count[MDP_CMDQ_USER_ALG]);
	if (!((c - 1) % cmd->pipe_num)) {
		mdp_m2m_process_done(cmd->ctx, vb_state);
		kfree(task);
		task = NULL;
	}
	atomic_dec(&mdp->job_count[MDP_CMDQ_USER_ALG]);

	kfree(cmd);
	cmd = NULL;
}

static void cmdq_release(struct work_struct *work)
{
	struct mdp_alg_cmdq *cmd;

	cmd = container_of(work, struct mdp_alg_cmdq, release_work);
	cmdq_release_work(cmd);
}

static void cmdq_callback(struct mbox_client *cl, void *mssg)
{
	struct mdp_alg_cmdq *cmd;
	struct cmdq_cb_data *data;
	struct mdp_dev *mdp;

	if (!mssg) {
		pr_info("%s:no callback data\n", __func__);
		return;
	}

	data = (struct cmdq_cb_data *)mssg;
	cmd = container_of(data->pkt, struct mdp_alg_cmdq, pkt);
	cmd->data = data;
	mdp = cmd->task->mdp;

	INIT_WORK(&cmd->release_work, cmdq_release);
	if (!queue_work(mdp->job_wq, &cmd->release_work))
		goto err_release_work;

	return;

err_release_work:
	dev_err(&mdp->pdev->dev, "queue_work fail!\n");
	cmdq_release_work(cmd);
}

static int cmdq_send(struct mdp_alg_cmdq *cmd)
{
	struct cmdq_client *clt = cmd->task->mdp->cmdq_clt[cmd->pipe_idx];
	struct cmdq_pkt *pkt = &cmd->pkt;
	int ret;

	cmdq_pkt_finalize(pkt);
	clt->client.rx_callback = cmdq_callback;
	dma_sync_single_for_device(clt->chan->mbox->dev,
				   pkt->pa_base, pkt->cmd_buf_size,
				   DMA_TO_DEVICE);
	ret = mbox_send_message(clt->chan, pkt);
	if (ret < 0) {
		cmdq_release_work(cmd);
		return ret;
	}
	mbox_client_txdone(clt->chan, 0);

	return 0;
}

int mdp_alg_submit(struct mdp_alg_task *task)
{
	struct mdp_dev *mdp = task->mdp;
	struct device *dev = &mdp->pdev->dev;
	int ret, i, pipe_used = 1;

	ret = mdp_alg_tp_select_path(task);
	if (ret)
		return ret;

	if (task->cfg.dual)
		pipe_used = 2;

	if (atomic_read(&mdp->suspended))
		return -EINVAL;
	atomic_set(&mdp->job_count[MDP_CMDQ_USER_ALG], pipe_used);

	for (i = 0; i < pipe_used; i++) {
		struct mdp_alg_cmdq *cmd;

		cmd = prepare_cmdq(task, i);
		if (!cmd)
			return ret;

		ret = prepare_comp(task, i);
		if (ret)
			return ret;

		ret = mdp_alg_tile_calc(task, i);
		if (ret)
			return ret;

		ret = config_path_clk_ctrl(task, i, false);
		if (ret)
			return ret;

		ret = config_comp(task, i);
		if (ret)
			return ret;

		ret = config_tile_run(task, i);
		if (ret)
			return ret;

		ret = cmdq_send(cmd);
		if (ret) {
			dev_err(dev, "pipe %d CMDQ send failed %d\n", i, ret);
			return ret;
		}
	}

	return 0;
}

int mdp_alg_process(struct mdp_m2m_ctx *ctx, struct img_ipi_frameparam *param)
{
	struct mdp_alg_task *task;
	int ret, i;

	if (atomic_read(&ctx->mdp_dev->job_count[MDP_CMDQ_USER_ALG])) {
		ret = wait_event_timeout(ctx->mdp_dev->callback_wq,
					 !atomic_read(&ctx->mdp_dev->job_count[MDP_CMDQ_USER_ALG]),
					 2 * HZ);
		if (ret == 0) {
			dev_err(&ctx->mdp_dev->pdev->dev,
				"%d M2M jobs not yet done\n",
				atomic_read(&ctx->mdp_dev->job_count[MDP_CMDQ_USER_ALG]));
			return -EBUSY;
		}
	}

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	task->cfg.frame_in.width = param->inputs[0].buffer.format.width;
	task->cfg.frame_in.height = param->inputs[0].buffer.format.height;
	for (i = 0; i < param->num_outputs; i++) {
		memcpy(&task->cfg.frame_in_crop[i], &param->outputs[i].crop,
		       sizeof(struct img_crop));

		switch (param->outputs[i].rotation) {
		case 90:
			task->cfg.out_rotate[i] = ROT_90;
			break;
		case 180:
			task->cfg.out_rotate[i] = ROT_180;
			break;
		case 270:
			task->cfg.out_rotate[i] = ROT_270;
			break;
		default:
			task->cfg.out_rotate[i] = ROT_0;
			break;
		}

		task->cfg.out_flip[i] = param->outputs[i].flags & IMG_CTRL_FLAG_HFLIP;
		if (task->cfg.out_rotate[i] == ROT_0 ||
		    task->cfg.out_rotate[i] == ROT_180) {
			task->cfg.frame_out[i].width =
				param->outputs[i].buffer.format.width;
			task->cfg.frame_out[i].height =
				param->outputs[i].buffer.format.height;
		} else {
			task->cfg.frame_out[i].width =
				param->outputs[i].buffer.format.height;
			task->cfg.frame_out[i].height =
				param->outputs[i].buffer.format.width;
		}

		task->cfg.frame_tile_sz.width =
			task->cfg.frame_in_crop[i].width;
		task->cfg.frame_tile_sz.height =
			task->cfg.frame_in_crop[i].height;
	}

	task->cfg.param = param;
	task->ctx = ctx;
	task->mdp = ctx->mdp_dev;
	for (i = 0; i < MDP_ALG_MAX_PIPE; i++)
		task->t_cache[i] = &ctx->tile_cache[i];

	return mdp_alg_submit(task);
}
