/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MDP3_CAP_H__
#define __MTK_MDP3_CAP_H__

#include <linux/soc/mediatek/mtk-hdmirx-intf.h>
#include <linux/soc/mediatek/mtk-mutex.h>
#include <media/v4l2-ctrls.h>
#include "mtk-mdp3-core.h"
#include "mtk-mdp3-vpu.h"
#include "mtk-mdp3-regs.h"

#define MIN_BUF_NEED 8

enum {
	MDP_CAP_SRC = 0,
	MDP_CAP_DST = 1,
	MDP_CAP_MAX,
};

enum cap_status {
	CAP_STATUS_STOP,
	CAP_STATUS_START,
};

struct mdp_cap_ctx {
	u32				id;
	struct mdp_dev			*mdp_dev;
	struct v4l2_fh			fh;
	struct v4l2_ctrl_handler	ctrl_handler;
	u32				frame_count[MDP_CAP_MAX];

	struct vb2_queue		cap_q;
	struct list_head		vb_queue; /* vb queue */
	struct list_head		ec_queue; /* execute queue */

	/* Protects queue */
	spinlock_t			slock;

	struct mdp_frameparam		curr_param;
	/* synchronization protect for mdp cap */
	struct mutex			ctx_lock;
	bool				pp_enable;
	enum cap_status			cap_status;

	u8				num_comps;
	struct mdp_comp			*comps[MDP_VPU_UID_MAX];
	struct mtk_mutex		*mutex[MDP_VPU_UID_MAX];
};

struct v4l2_cap_buffer {
	struct vb2_v4l2_buffer	vb;
	struct list_head	list;
};

struct mdp_cap_driver {
	struct hdmirx_capture_driver driver;
};

int mdp_capture_device_register(struct mdp_dev *mdp);
void mdp_cap_job_finish(struct mdp_cap_ctx *ctx);
int mdp_cap_init(struct mdp_dev *mdp);
void mdp_cap_deinit(void);

#endif  /* __MTK_MDP3_CAP_H__ */
