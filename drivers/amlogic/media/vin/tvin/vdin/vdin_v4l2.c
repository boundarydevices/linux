/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_v4l2.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
/* #include <linux/interrupt.h> */
#include <linux/mm.h>

#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>

static struct vdin_v4l2_ops_s ops = {NULL};

int vdin_reg_v4l2(struct vdin_v4l2_ops_s *v4l2_ops)
{
	void *ret = 0;

	if (!v4l2_ops)
		return -1;
	ret = memcpy(&ops, v4l2_ops, sizeof(struct vdin_v4l2_ops_s));
	if (ret)
		return 0;
	return -1;
}
EXPORT_SYMBOL(vdin_reg_v4l2);

void vdin_unreg_v4l2(void)
{
	memset(&ops, 0, sizeof(struct vdin_v4l2_ops_s));
}
EXPORT_SYMBOL(vdin_unreg_v4l2);

int v4l2_vdin_ops_init(struct vdin_v4l2_ops_s *vdin_v4l2p)
{
	void *ret = 0;

	if (!vdin_v4l2p)
		return -1;
	ret = memcpy(vdin_v4l2p, &ops, sizeof(struct vdin_v4l2_ops_s));
	if (ret)
		return 0;
	return -1;
}
EXPORT_SYMBOL(v4l2_vdin_ops_init);

struct vdin_v4l2_ops_s *get_vdin_v4l2_ops()
{
	if ((ops.start_tvin_service != NULL) && (ops.stop_tvin_service != NULL))
		return &ops;
	else {
		/* pr_err("[vdin..]%s: vdin v4l2 operation
		 * haven't registered.",__func__);
		 */
		return NULL;
	}
}
EXPORT_SYMBOL(get_vdin_v4l2_ops);

/*Converts commands into strings */
const char *cam_cmd_to_str(enum cam_command_e cmd)
{
	switch (cmd) {
	case CAM_COMMAND_INIT:
		return "CAM_COMMAND_INIT";
	case CAM_COMMAND_GET_STATE:
		return "CAM_COMMAND_GET_STATE";
	case CAM_COMMAND_SCENES:
		return "CAM_COMMAND_SCENES";
	case CAM_COMMAND_EFFECT:
		return "CAM_COMMAND_EFFECT";
	case CAM_COMMAND_AWB:
		return "CAM_COMMAND_AWB";
	case CAM_COMMAND_MWB:
		return "CAM_COMMAND_MWB";
	case CAM_COMMAND_SET_WORK_MODE:
		return "CAM_COMMAND_SET_WORK_MODE";
	case CAM_COMMAND_AE_ON:
		return "CAM_COMMAND_AE_ON";
	case CAM_COMMAND_AE_OFF:
		return "CAM_COMMAND_AE_OFF";
	case CAM_COMMAND_SET_AE_LEVEL:
		return "CAM_COMMAND_SET_AE_LEVEL";
	case CAM_COMMAND_AF:
		return "CAM_COMMAND_AF";
	case CAM_COMMAND_FULLSCAN:
		return "CAM_COMMAND_FULLSCAN";
	case CAM_COMMAND_TOUCH_FOCUS:
		return "CAM_COMMAND_TOUCH_FOCUS";
	case CAM_COMMAND_CONTINUOUS_FOCUS_ON:
		return "CAM_COMMAND_CONTINUOUS_FOCUS_ON";
	case CAM_COMMAND_CONTINUOUS_FOCUS_OFF:
		return "CAM_COMMAND_CONTINUOUS_FOCUS_OFF";
	case CAM_COMMAND_BACKGROUND_FOCUS_ON:
		return "CAM_COMMAND_BACKGROUND_FOCUS_ON";
	case CAM_COMMAND_BACKGROUND_FOCUS_OFF:
		return "CAM_COMMAND_BACKGROUND_FOCUS_OFF";
	case CAM_COMMAND_SET_FLASH_MODE:
		return "CAM_COMMAND_SET_FLASH_MODE";
	case CAM_COMMAND_TORCH:
		return "CAM_COMMAND_TORCH";
	case CMD_ISP_BYPASS:
		return "CMD_ISP_BYPASS";
	default:
		break;
	}
	return NULL;
}
EXPORT_SYMBOL(cam_cmd_to_str);
