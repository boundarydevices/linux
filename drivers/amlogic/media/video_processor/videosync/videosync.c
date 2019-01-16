/*
 * drivers/amlogic/media/video_processor/videosync/videosync.c
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

#define DEBUG

#include "videosync.h"
#include <linux/amlogic/major.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>


#define VIDEOSYNC_DEVICE_NAME   "videosync"
#define RECEIVER_NAME "videosync"
#define PROVIDER_NAME "videosync"

static bool videosync_inited;/*false*/
static bool show_nosync;/*false*/
static bool smooth_sync_enable;
static int enable_video_discontinue_report = 1;
static u32 system_time_scale_base = 1;
static s32 system_time_inc_adj;/*?*/
static u32 vsync_pts_inc;/*?*/
static u32 omx_version = 2;
static u32 vp_debug_flag;
static bool no_render;/* default: false */
static bool async_mode;/* default: false */
static u32 video_early_threshold = 900; /* default: 900=>10ms */


/* video freerun mode */
#define FREERUN_NONE    0	/* no freerun mode */
#define FREERUN_NODUR   1	/* freerun without duration */
#define FREERUN_DUR     2	/* freerun with duration */
#define M_PTS_SMOOTH_MAX 45000
#define M_PTS_SMOOTH_MIN 2250
#define M_PTS_SMOOTH_ADJUST 900
#define DURATION_GCD 750

static int omx_pts_interval_upper = 11000;
static int omx_pts_interval_lower = -5500;
#define DUR2PTS(x) ((x) - ((x) >> 4))

#define PRINT_ERROR			0X0
#define PRINT_QUEUE_STATUS	0X0001
#define PRINT_TIMESTAMP		0X0002
#define PRINT_OTHER			0X0004

static struct videosync_dev *vp_dev;

static int vp_print(char *name, int debug_flag, const char *fmt, ...)
{
	if ((vp_debug_flag & debug_flag)
		|| (debug_flag == PRINT_ERROR)) {
		unsigned char buf[512];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "[%s]", name);
		vsnprintf(buf + len, 512-len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static u32 ts_pcrscr_get(struct videosync_s *dev_s)
{
	u32 sys_time = 0;
	/*unsigned long flags;*/
	sys_time = dev_s->system_time;
	return sys_time;
}

static void ts_pcrscr_set(struct videosync_s *dev_s, u32 pts)
{
	dev_s->system_time = pts;
	vp_print(dev_s->vf_receiver_name, PRINT_TIMESTAMP,
			"ts_pcrscr_set sys_time %d\n", dev_s->system_time);
}

static void ts_pcrscr_enable(struct videosync_s *dev_s, u32 enable)
{
	dev_s->system_time_up = enable;
}
static u32 ts_pcrscr_enable_state(struct videosync_s *dev_s)
{
	return dev_s->system_time_up;
}

void videosync_pcrscr_update(s32 inc, u32 base)
{
	int i = 0;
	u32 r;
	/*unsigned long flags;*/
	struct videosync_s *dev_s;
	u32 current_omx_pts;
	int diff;

	if (!videosync_inited)
		return;
	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &vp_dev->video_prov[i];
		if (dev_s && dev_s->active_state == VIDEOSYNC_ACTIVE) {
			if (system_time_scale_base != base) {
				dev_s->system_time_scale_remainder =
					dev_s->system_time_scale_remainder *
						base / system_time_scale_base;
				system_time_scale_base = base;
			}
			if (dev_s->system_time_up) {
				dev_s->system_time +=
					div_u64_rem(90000ULL * inc, base, &r)
					+ system_time_inc_adj;
				dev_s->system_time_scale_remainder += r;
				if (dev_s->system_time_scale_remainder
					>= system_time_scale_base) {
					dev_s->system_time++;
					dev_s->system_time_scale_remainder
						-= system_time_scale_base;
				}
				vp_print(dev_s->vf_receiver_name, PRINT_OTHER,
				"update sys_time %d, system_time_scale_base %d, inc %d\n",
				dev_s->system_time,
				system_time_scale_base,
				inc);
			}

			/*check if need to correct pcr by omx_pts*/
			current_omx_pts = dev_s->omx_pts;
			diff = dev_s->system_time - current_omx_pts;

			if ((diff - omx_pts_interval_upper) > 0
				|| (diff - omx_pts_interval_lower) < 0) {
				vp_print(dev_s->vf_receiver_name,
					PRINT_TIMESTAMP,
					"sys_time=%d, omx_pts=%d, diff=%d\n",
					dev_s->system_time,
					current_omx_pts,
					diff);
				ts_pcrscr_set(dev_s,
					current_omx_pts + DURATION_GCD);
			}
		}
	}
}


/* -----------------------------------------------------------------
 *           videosync operations
 * -----------------------------------------------------------------
 */
static struct vframe_s *videosync_vf_peek(void *op_arg)
{
	struct vframe_s *vf = NULL;
	struct videosync_s *dev_s = (struct videosync_s *)op_arg;

	vf = vfq_peek(&dev_s->ready_q);
	return vf;
}

static struct vframe_s *videosync_vf_get(void *op_arg)
{
	struct vframe_s *vf = NULL;
	struct videosync_s *dev_s = (struct videosync_s *)op_arg;

	vf = vfq_pop(&dev_s->ready_q);
	if (vf) {
		dev_s->cur_dispbuf = vf;
		dev_s->first_frame_toggled = 1;
	}
	return vf;
}

static void videosync_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct videosync_s *dev_s = (struct videosync_s *)op_arg;

	if (vf != NULL) {
		vf_put(vf, dev_s->vf_receiver_name);
		dev_s->put_frame_count++;
	} else {
		vp_print(dev_s->vf_receiver_name, 0,
			"videosync_vf_put: NULL!\n");
	}
}

static int videosync_event_cb(int type, void *data, void *private_data)
{
	return 0;
}

static int videosync_buffer_states(struct videosync_buffer_states *states,
	void *op_arg)
{
	struct videosync_s *dev_s = (struct videosync_s *)op_arg;

	states->buf_ready_num = vfq_level(&dev_s->ready_q);
	states->buf_queued_num = vfq_level(&dev_s->queued_q);
	states->total_num = dev_s->get_frame_count;
	return 0;
}

static int videosync_buffer_states_vf(struct vframe_states *states,
	void *op_arg)
{
	struct videosync_s *dev_s = (struct videosync_s *)op_arg;

	states->vf_pool_size = VIDEOSYNC_S_POOL_SIZE;
	states->buf_recycle_num = 0;
	states->buf_free_num = VIDEOSYNC_S_POOL_SIZE
		- vfq_level(&dev_s->ready_q)
		- vfq_level(&dev_s->queued_q);
	states->buf_avail_num = vfq_level(&dev_s->ready_q)
		+ vfq_level(&dev_s->queued_q);

	return 0;
}

static const struct vframe_operations_s v4lvideo_vf_provider = {
	.peek = videosync_vf_peek,
	.get = videosync_vf_get,
	.put = videosync_vf_put,
	.event_cb = videosync_event_cb,
	.vf_states = videosync_buffer_states_vf,
};

static void videosync_register(struct videosync_s *dev_s)
{
	vf_provider_init(&dev_s->video_vf_prov,
				dev_s->vf_provider_name,
				&v4lvideo_vf_provider, dev_s);
	vf_reg_provider(&dev_s->video_vf_prov);
	vf_notify_receiver(dev_s->vf_provider_name,
				VFRAME_EVENT_PROVIDER_START,
				NULL);
}

static void videosync_unregister(struct videosync_s *dev_s)
{
	vf_unreg_provider(&dev_s->video_vf_prov);
}

static ssize_t dump_queue_state_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int ret = 0;
	int i = 0;
	struct videosync_buffer_states states;
	struct videosync_s *dev_s = NULL;

	if (vp_dev == NULL)
		return -1;

	if (vp_dev->active_dev_s_num == 0)
		ret += sprintf(buf, "no active videosync\n");
	else {
		for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
			dev_s = &vp_dev->video_prov[i];
			if (dev_s->active_state == VIDEOSYNC_INACTIVE)
				continue;

			if (videosync_buffer_states(&states, dev_s) == 0) {
				ret += sprintf(buf + ret,
					    "\n#------ %s state ------#\n",
						dev_s->vf_receiver_name);
				ret += sprintf(buf + ret,
						"queued_q_size=%d\n",
						states.buf_queued_num);
				ret += sprintf(buf + ret,
						"ready_q_size=%d\n",
						states.buf_ready_num);
				ret += sprintf(buf + ret,
						"total frame count=%d\n",
						states.total_num);
			}
		}
	}
	return ret;
}

static ssize_t dump_pts_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int ret = 0;
	int i = 0;
	struct videosync_s *dev_s = NULL;

	if (vp_dev == NULL)
		return -1;

	if (vp_dev->active_dev_s_num == 0)
		ret += sprintf(buf, "no active videosync\n");
	else {
		for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
			dev_s = &vp_dev->video_prov[i];
			if (dev_s->active_state == VIDEOSYNC_INACTIVE)
				continue;

			ret += sprintf(buf + ret,
					   "%s:  ", dev_s->vf_receiver_name);
			ret += sprintf(buf + ret,
					   "omx_pts=%d, ", dev_s->omx_pts);
			ret += sprintf(buf + ret,
					"system_time=%d\n", dev_s->system_time);
		}
	}
	return ret;
}

static ssize_t dump_get_put_framecount_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int ret = 0;
	int i = 0;
	struct videosync_s *dev_s = NULL;

	if (vp_dev == NULL)
		return -1;

	if (vp_dev->active_dev_s_num == 0)
		ret += sprintf(buf, "no active videosync\n");
	else {
		for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
			dev_s = &vp_dev->video_prov[i];
			if (dev_s->active_state == VIDEOSYNC_INACTIVE)
				continue;

			ret += sprintf(buf + ret,
					   "%s:  ", dev_s->vf_receiver_name);
			ret += sprintf(buf + ret,
						"get_frame_count=%d, put_frame_count=%d\n",
						dev_s->get_frame_count,
						dev_s->put_frame_count);
		}
	}
	return ret;
}

static ssize_t dump_rect_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int ret = 0;
	int i = 0;
	struct videosync_s *dev_s = NULL;

	if (vp_dev == NULL)
		return -1;

	if (vp_dev->active_dev_s_num == 0)
		ret += sprintf(buf, "no active videosync\n");
	else {
		for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
			dev_s = &vp_dev->video_prov[i];
			if (dev_s->active_state == VIDEOSYNC_INACTIVE)
				continue;

			ret += sprintf(buf + ret,
					   "%s:  ", dev_s->vf_receiver_name);
			ret += sprintf(buf + ret,
						"rect = [%d,%d,%d,%d], ",
						dev_s->rect.left,
						dev_s->rect.top,
						dev_s->rect.width,
						dev_s->rect.height);
			ret += sprintf(buf + ret,
						"zorder = %d\n", dev_s->zorder);
		}
	}
	return ret;
}

static struct class_attribute videosync_class_attrs[] = {
	__ATTR_RO(dump_queue_state),
	__ATTR_RO(dump_pts),
	__ATTR_RO(dump_get_put_framecount),
	__ATTR_RO(dump_rect),
	__ATTR_NULL
};
static struct class videosync_class = {
	.name = "videosync",
	.class_attrs = videosync_class_attrs,
};


int videosync_assign_map(char **receiver_name, int *inst)
{
	int i = 0;
	struct videosync_s *dev_s  = NULL;

	mutex_lock(&vp_dev->vp_mutex);
	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &vp_dev->video_prov[i];
		if (dev_s->inst == *inst) {
			*receiver_name = dev_s->vf_receiver_name;
			pr_info("videosync_assign_map %s %p\n",
				dev_s->vf_receiver_name, dev_s);
			mutex_unlock(&vp_dev->vp_mutex);
			return 0;
		}
	}
	mutex_unlock(&vp_dev->vp_mutex);
	return -ENODEV;
}

int videosync_alloc_map(int *inst)
{
	int i = 0;
	struct videosync_s *dev_s  = NULL;

	mutex_lock(&vp_dev->vp_mutex);
	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &vp_dev->video_prov[i];
		if (dev_s->inst >= 0  && (!dev_s->mapped)) {
			dev_s->mapped = true;
			*inst = dev_s->inst;
			pr_info("videosync_alloc_map %d OK\n", dev_s->inst);
			mutex_unlock(&vp_dev->vp_mutex);
			return 0;
		}
	}
	mutex_unlock(&vp_dev->vp_mutex);
	return -ENODEV;
}

void videosync_release_map(int inst)
{

	int i = 0;
	struct videosync_s *dev_s  = NULL;

	mutex_lock(&vp_dev->vp_mutex);
	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &vp_dev->video_prov[i];
		if (dev_s->inst == inst  && (dev_s->mapped)) {
			dev_s->mapped = false;
			pr_info("videosync_release_map %d OK\n", inst);
			break;
		}
	}
	mutex_unlock(&vp_dev->vp_mutex);
}
void videosync_release_map_force(struct videosync_priv_s *priv)
{
	int i = 0;
	struct videosync_s *dev_s  = NULL;

	mutex_lock(&vp_dev->vp_mutex);
	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &vp_dev->video_prov[i];
		if (dev_s->inst == priv->vp_id && (dev_s->mapped)) {
			dev_s->mapped = false;
			pr_info("videosync_release_map_force %d OK\n",
				priv->vp_id);
			break;
		}
	}
	mutex_unlock(&vp_dev->vp_mutex);
}

static int videosync_open(struct inode *inode, struct file *file)
{
	struct videosync_priv_s *priv = NULL;

	pr_info("videosync_open\n");
	priv = kzalloc(sizeof(struct videosync_priv_s), GFP_KERNEL);
	if (priv == NULL) {
		pr_err("kzalloc videosync_priv_s failed!");
	} else {
		priv->vp_id = -1;
		priv->dev_s = NULL;
		file->private_data = priv;
	}
	return 0;
}

static int videosync_release(struct inode *inode, struct file *file)
{
	struct videosync_priv_s *priv = NULL;

	pr_info("videosync_release\n");
	if (file && file->private_data) {
		priv = file->private_data;

		/*check if map released.*/
		videosync_release_map_force(priv);

		kfree(priv);
		priv = NULL;
		file->private_data = NULL;
	}

	return 0;
}

static int set_omx_pts(u32 *p)
{
	struct videosync_s *dev_s = NULL;
	int ret = 0;
	u32 tmp_pts = p[0];
	/*u32 vision = p[1];*/
	u32 set_from_hwc = p[2];
	u32 frame_num = p[3];
	u32 not_reset = p[4];
	u32 dev_id = p[6];

	if (dev_id < VIDEOSYNC_S_COUNT)
		dev_s = &vp_dev->video_prov[dev_id];

	if (dev_s != NULL && dev_s->mapped) {
		mutex_lock(&dev_s->omx_mutex);
		vp_print(dev_s->vf_receiver_name, PRINT_TIMESTAMP,
			"set omx_pts %d, hwc %d, not_reset %d, frame_num\n",
			tmp_pts, set_from_hwc, not_reset, frame_num);
		if (not_reset == 0)
			dev_s->omx_pts = tmp_pts;

		mutex_unlock(&dev_s->omx_mutex);

		ts_pcrscr_enable(dev_s, 1);
	} else {
		ret = -EFAULT;
		pr_err("[%s]cannot find dev_id %d device\n",
			dev_s->vf_receiver_name, dev_id);
	}

	return ret;

}
static int set_omx_zorder(u32 *p)
{
	struct videosync_s *dev_s = NULL;
	int ret = 0;
	u32 dev_id = p[5];
	/*32 tmp_pts = p[6];*/

	if (dev_id < VIDEOSYNC_S_COUNT)
		dev_s = &vp_dev->video_prov[dev_id];

	if (dev_s != NULL && dev_s->mapped) {
		dev_s->zorder = p[0];
		dev_s->rect.left = p[1];
		dev_s->rect.top = p[2];
		dev_s->rect.width = p[3];
		dev_s->rect.height = p[4];

		vp_print(dev_s->vf_receiver_name, PRINT_TIMESTAMP,
			"set zorder %d: %d %d %d %d\n",
			p[0], p[1], p[2], p[3], p[4]);

	} else {
		ret = -EFAULT;
		pr_err("[%s]cannot find dev_id %d device\n",
			dev_s->vf_receiver_name, dev_id);
	}

	return ret;

}

static long videosync_ioctl(struct file *file,
			    unsigned int cmd,
			    ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	struct videosync_priv_s *priv = NULL;

	switch (cmd) {
	case VIDEOSYNC_IOC_ALLOC_ID:{
		u32 videosync_id = 0;

		ret = videosync_alloc_map(&videosync_id);
		if (ret != 0)
			break;
		put_user(videosync_id, (u32 __user *)argp);
		pr_info("alloc-id %d\n", videosync_id);

		if (file && file->private_data) {
			priv = file->private_data;
			priv->vp_id = videosync_id;
			priv->dev_s = &vp_dev->video_prov[videosync_id];
			pr_info("get dev_s %p\n", priv->dev_s);
		}
	}
	break;
	case VIDEOSYNC_IOC_FREE_ID:{
		u32 videosync_id;

		get_user(videosync_id, (u32 __user *)argp);
		pr_info("free-id %d\n", videosync_id);
		videosync_release_map(videosync_id);
	}
	break;

	case VIDEOSYNC_IOC_SET_FREERUN_MODE:
		if (arg > FREERUN_DUR)
			ret = -EFAULT;
		else {
			if (file && file->private_data) {
				priv = file->private_data;
				if (priv && priv->dev_s)
					priv->dev_s->freerun_mode = arg;
				else
					ret = -EFAULT;
			} else {
				ret = -EFAULT;
			}
		}
	break;

	case VIDEOSYNC_IOC_GET_FREERUN_MODE:
		if (file && file->private_data) {
			priv = file->private_data;
			if (priv && priv->dev_s)
				put_user(priv->dev_s->freerun_mode,
					(u32 __user *)argp);
			else
				ret = -EFAULT;

		} else
			ret = -EFAULT;

	break;
	case VIDEOSYNC_IOC_SET_OMX_VPTS:{
		u32 pts[7];

		if (copy_from_user(pts, argp, sizeof(pts)) == 0)
			ret = set_omx_pts(pts);

	}
	break;
	case VIDEOSYNC_IOC_SET_OMX_ZORDER:{
		u32 zorder[7];

		if (copy_from_user(zorder, argp, sizeof(zorder)) == 0)
			ret = set_omx_zorder(zorder);

	}
	break;
	case VIDEOSYNC_IOC_GET_OMX_VPTS:
		if (file && file->private_data) {
			priv = file->private_data;
			if (priv && priv->dev_s)
				put_user(priv->dev_s->omx_pts,
					(u32 __user *)argp);
			else
				ret = -EFAULT;
		} else {
			ret = -EFAULT;
		}
	break;
	case VIDEOSYNC_IOC_GET_OMX_VERSION:
		put_user(omx_version, (u32 __user *)argp);
		pr_info("get omx_version %d\n", omx_version);
	break;
	default:
		pr_info("ioctl invalid cmd 0x%x\n", cmd);
		return -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long videosync_compat_ioctl(struct file *file,
				unsigned int cmd,
				ulong arg)
{
	long ret = 0;

	ret = videosync_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

static void clear_queued_queue(struct videosync_s *dev_s)
{
	struct vframe_s *vf = NULL;

	vp_print(dev_s->vf_receiver_name, PRINT_QUEUE_STATUS,
		"clear_queued_queue: size %d!\n", vfq_level(&dev_s->queued_q));

	vf = vfq_pop(&dev_s->queued_q);
	while (vf) {
		vf_put(vf, dev_s->vf_receiver_name);
		vf = vfq_pop(&dev_s->queued_q);
	}

}

static void clear_ready_queue(struct videosync_s *dev_s)
{
	struct vframe_s *vf = NULL;

	vp_print(dev_s->vf_receiver_name, PRINT_QUEUE_STATUS,
		"clear_ready_queue: size %d!\n",
		vfq_level(&dev_s->ready_q));
	vf = vfq_pop(&dev_s->ready_q);
	while (vf) {
		vf_put(vf, dev_s->vf_receiver_name);
		vf = vfq_pop(&dev_s->ready_q);
	}
}

static inline bool omx_vpts_expire(struct vframe_s *cur_vf,
			       struct vframe_s *next_vf,
			       struct videosync_s *dev_s)

{
	u32 pts = next_vf->pts;
#ifdef VIDEO_PTS_CHASE
	u32 vid_pts, scr_pts;
#endif
	u32 systime;
	u32 adjust_pts, org_vpts;
	/*u32 dur_pts = 0;*/

	if (dev_s->freerun_mode == FREERUN_NODUR)
		return true;

	if (next_vf->duration == 0)
		return true;

	systime = ts_pcrscr_get(dev_s);

	if (no_render)
		dev_s->first_frame_toggled = 1; /*just for debug, not render*/

	vp_print(dev_s->vf_receiver_name, PRINT_TIMESTAMP,
		"sys_time=%d, vf->pts=%d, diff=%d\n",
		systime, pts, (int)(systime - pts));

	if (0) {
		pts =
		    timestamp_vpts_get() +
		    (cur_vf ? DUR2PTS(cur_vf->duration) : 0);
	}
	/* check video PTS discontinuity */
	else if (ts_pcrscr_enable_state(dev_s) > 0 &&
		 (enable_video_discontinue_report) &&
		 (dev_s->first_frame_toggled) &&
		 (abs(systime - pts) > tsync_vpts_discontinuity_margin()) &&
		 ((next_vf->flag & VFRAME_FLAG_NO_DISCONTINUE) == 0
		 || tsync_vpts_discontinuity_margin() <= 90000)) {

		vp_print(dev_s->vf_receiver_name, PRINT_TIMESTAMP,
			"discontinue, systime = %d, next_vf->pts = %d\n",
			systime, next_vf->pts);
		return true;
	}

	if (0/*smooth_sync_enable*/) {
		org_vpts = timestamp_vpts_get();
		if ((abs(org_vpts + vsync_pts_inc - systime) <
			M_PTS_SMOOTH_MAX)
		    && (abs(org_vpts + vsync_pts_inc - systime) >
			M_PTS_SMOOTH_MIN)) {

			if (!dev_s->video_frame_repeat_count) {
				dev_s->vpts_ref = org_vpts;
				dev_s->video_frame_repeat_count++;
			}

			if ((int)(org_vpts + vsync_pts_inc - systime) > 0) {
				adjust_pts =
				    dev_s->vpts_ref + (vsync_pts_inc -
						M_PTS_SMOOTH_ADJUST) *
				    dev_s->video_frame_repeat_count;
			} else {
				adjust_pts =
				    dev_s->vpts_ref + (vsync_pts_inc +
						M_PTS_SMOOTH_ADJUST) *
				    dev_s->video_frame_repeat_count;
			}

			return (int)(adjust_pts - pts) >= 0;
		}

		if (dev_s->video_frame_repeat_count) {
			dev_s->vpts_ref = 0;
			dev_s->video_frame_repeat_count = 0;
		}
	}

	return (systime + video_early_threshold) > pts;

}

void videosync_sync(struct videosync_s *dev_s)
{
	int ready_q_size = 0;
	struct vframe_s *vf;

	if (smooth_sync_enable) {
		if (dev_s->video_frame_repeat_count)
			dev_s->video_frame_repeat_count++;
	}

	vf = vfq_peek(&dev_s->queued_q);

	while (vf) {
		if (omx_vpts_expire(dev_s->cur_dispbuf, vf, dev_s)
			|| show_nosync) {

			vf = vfq_pop(&dev_s->queued_q);
			if (vf) {
				if (async_mode)	{
					if (vfq_level(&dev_s->ready_q) > 0)
						clear_ready_queue(dev_s);
				}

				vfq_push(&dev_s->ready_q, vf);
				ready_q_size = vfq_level(&dev_s->ready_q);
				vp_print(dev_s->vf_receiver_name,
					PRINT_QUEUE_STATUS,
					"add pts %d index 0x%x to ready_q, size %d\n",
					vf->pts, vf->index, ready_q_size);

				vf_notify_receiver(
					dev_s->vf_provider_name,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
				if (ready_q_size > VIDEOSYNC_S_POOL_SIZE - 1) {
					vp_print(dev_s->vf_receiver_name,
						PRINT_QUEUE_STATUS,
						"ready_q full!\n");
					break;
				}
			}

			vf = vfq_peek(&dev_s->queued_q);
			if (!vf)
				break;
		} else {
			break;
		}
	}

}


static void prepare_queued_queue(struct videosync_dev *dev)
{
	int i = 0;
	struct videosync_s *dev_s;
	struct vframe_s *vf;

	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &dev->video_prov[i];
		if (dev_s->active_state == VIDEOSYNC_INACTIVE)
			continue;

		if (vfq_full(&dev_s->queued_q)) {
			pr_info("[%s]queued_q full!\n",
				dev_s->vf_receiver_name);
			continue;
		}

		if (vf_peek(dev_s->vf_receiver_name)) {
			vf = vf_get(dev_s->vf_receiver_name);
			if (vf) {
				vfq_push(&dev_s->queued_q, vf);
				dev_s->get_frame_count++;
				vp_print(dev_s->vf_receiver_name,
					PRINT_QUEUE_STATUS,
					"add pts %d index 0x%x to queued_q, size %d\n",
					vf->pts,
					vf->index,
					vfq_level(&dev_s->queued_q));
			}
		} else {
			vp_print(dev_s->vf_receiver_name, PRINT_OTHER,
				"peek failed %d\n");
		}
	}

}
static void prepare_ready_queue(struct videosync_dev *dev)
{
	int i = 0;
	struct videosync_s *dev_s;

	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &dev->video_prov[i];
		if (dev_s->active_state == VIDEOSYNC_INACTIVE)
			continue;

		if (vfq_full(&dev_s->ready_q)) {
			pr_info("[%s]ready_q full!\n", dev_s->vf_receiver_name);
			continue;
		}

		videosync_sync(dev_s);

		if (no_render)
			clear_ready_queue(dev_s);/*just for debug, not render*/

	}

}

static void reinit_dev_s(struct videosync_s *dev_s)
{
	dev_s->get_frame_count = 0;
	dev_s->put_frame_count = 0;
	dev_s->system_time_up = 0;
	dev_s->system_time = 0;
	dev_s->first_frame_toggled = 0;
	dev_s->freerun_mode = 0;
	dev_s->system_time_scale_remainder = 0;
	dev_s->omx_pts = 0;
	dev_s->vpts_ref = 0;
	dev_s->video_frame_repeat_count = 0;
	dev_s->rect.top = 0;
	dev_s->rect.left = 0;
	dev_s->rect.width = 0;
	dev_s->rect.height = 0;
	dev_s->zorder = 0;
}
static int videosync_receiver_event_fun(int type, void *data,
	void *private_data)
{
	struct videosync_s *dev_s = (struct videosync_s *)private_data;
	unsigned long flags = 0;
	struct videosync_dev *dev;
	unsigned long time_left;

	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		pr_info("videosync: try to unreg %p, %s\n",
			dev_s, dev_s->vf_receiver_name);
		dev = (struct videosync_dev *)dev_s->dev;

		spin_lock_irqsave(&dev->dev_s_num_slock, flags);
		--dev->active_dev_s_num;
		spin_unlock_irqrestore(&dev->dev_s_num_slock, flags);

		videosync_unregister(dev_s);
		dev_s->receiver_register = false;

		if (dev_s->active_state == VIDEOSYNC_ACTIVE) {
			dev_s->active_state = VIDEOSYNC_INACTIVE_REQ;
			time_left = wait_for_completion_timeout(
				&dev_s->inactive_done,
				msecs_to_jiffies(100));
			if (time_left == 0)
				pr_err("videosync: unreg timeout\n");
		}
		clear_ready_queue(dev_s);
		clear_queued_queue(dev_s);

		/*tsync_avevent(VIDEO_STOP, 0);*/
		pr_info("videosync: unreg %p, %s\n",
			dev_s, dev_s->vf_receiver_name);
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		//omx_secret_mode = false;
		struct videosync_dev *dev = (struct videosync_dev *)dev_s->dev;

		reinit_dev_s(dev_s);
		//videosync_register(dev_s);
		dev_s->receiver_register = true;
		dev_s->active_state = VIDEOSYNC_ACTIVE;

		spin_lock_irqsave(&dev->dev_s_num_slock, flags);
		++dev->active_dev_s_num;
		spin_unlock_irqrestore(&dev->dev_s_num_slock, flags);

		vfq_init(&dev_s->queued_q, VIDEOSYNC_S_POOL_SIZE + 1,
				&dev_s->videosync_pool_queued[0]);
		vfq_init(&dev_s->ready_q, VIDEOSYNC_S_POOL_SIZE + 1,
				&dev_s->videosync_pool_ready[0]);

		init_completion(&dev_s->inactive_done);
		complete(&dev->thread_active);
		pr_info("videosync: reg %p, %s\n",
			dev_s, dev_s->vf_receiver_name);
	} else if (type == VFRAME_EVENT_PROVIDER_VFRAME_READY) {

	} else if (type == VFRAME_EVENT_PROVIDER_START) {
		videosync_register(dev_s);
		pr_info("videosync: start %p, %s\n",
			dev_s, dev_s->vf_receiver_name);
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		struct videosync_buffer_states states;

		videosync_buffer_states(&states, dev_s);
		if (states.buf_queued_num + states.buf_ready_num > 0)
			return RECEIVER_ACTIVE;

		vp_print(dev_s->vf_receiver_name, 0,
				"buf queue empty!!\n");
		return RECEIVER_ACTIVE;/*return RECEIVER_INACTIVE;*/
	}
	return 0;
}

static const struct file_operations videosync_fops = {

	.owner = THIS_MODULE,
	.open = videosync_open,
	.release = videosync_release,
	.unlocked_ioctl = videosync_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = videosync_compat_ioctl,
#endif
	.poll = NULL,
};

static const struct vframe_receiver_op_s videosync_vf_receiver = {
	.event_cb = videosync_receiver_event_fun,
};

static const struct videosync_operations_s videosync_vf_provider = {
	.peek = videosync_vf_peek,
	.get = videosync_vf_get,
	.put = videosync_vf_put,
	.event_cb = videosync_event_cb,
	.buffer_states = videosync_buffer_states,
};

static int __init videosync_create_instance(int inst)
{
	struct videosync_s *dev_s;

	dev_s = &vp_dev->video_prov[inst];
	dev_s->dev = vp_dev;
	dev_s->fd_num = 0;
	dev_s->ops = &videosync_vf_provider;
	dev_s->active_state = VIDEOSYNC_INACTIVE;
	pr_info("videosync_create_instance dev_s %p,dev_s->dev %p\n",
		dev_s, dev_s->dev);

	/* initialize locks */
	mutex_init(&dev_s->omx_mutex);
	spin_lock_init(&dev_s->timestamp_lock);

	dev_s->inst = inst;
	dev_s->index = inst;
	dev_s->mapped = true;
	snprintf(dev_s->vf_receiver_name, VIDEOSYNC_S_VF_RECEIVER_NAME_SIZE,
		RECEIVER_NAME ".%x", inst & 0xff);
	snprintf(dev_s->vf_provider_name, VIDEOSYNC_S_VF_RECEIVER_NAME_SIZE,
		PROVIDER_NAME ".%x", inst & 0xff);

	pr_info("videosync_create_instance reg %s\n",
		dev_s->vf_receiver_name);

	vf_receiver_init(&dev_s->vp_vf_receiver,
			dev_s->vf_receiver_name,
			&videosync_vf_receiver, dev_s);

	vf_reg_receiver(&dev_s->vp_vf_receiver);

	return 0;

}

static void __init videosync_destroy_instance(int inst)
{
	pr_info("videosync_destroy_instance %s\n",
		vp_dev->video_prov[inst].vf_receiver_name);
	vf_unreg_receiver(&vp_dev->video_prov[inst].vp_vf_receiver);
}


static void videosync_thread_tick(struct videosync_dev *dev)
{
	int i = 0;
	struct videosync_s *dev_s;
	unsigned long flags = 0;
	unsigned long time_left;

	if (!dev)
		return;

	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &dev->video_prov[i];
		if (dev_s->active_state == VIDEOSYNC_INACTIVE_REQ) {
			dev_s->active_state = VIDEOSYNC_INACTIVE;
			complete(&dev_s->inactive_done);
		}
	}

	vp_print(RECEIVER_NAME, PRINT_OTHER,
		"active num %d\n", dev->active_dev_s_num);

	spin_lock_irqsave(&dev->dev_s_num_slock, flags);
	if (dev->active_dev_s_num > 0) {
		spin_unlock_irqrestore(&dev->dev_s_num_slock, flags);
		prepare_queued_queue(dev);
		prepare_ready_queue(dev);
		usleep_range(7000, 8000);
	} else {
		spin_unlock_irqrestore(&dev->dev_s_num_slock, flags);
		vp_print(RECEIVER_NAME, PRINT_OTHER,
			"videosync: no active dev, thread go to sleep\n");
		time_left = wait_for_completion_timeout(&dev->thread_active,
			msecs_to_jiffies(500));
		if (time_left == 0)
			pr_err("videosync: thread_tick timeout\n");
	}
}

static void videosync_sleep(struct videosync_dev *dev)
{
	videosync_thread_tick(dev);
	try_to_freeze();
}

static int videosync_thread(void *data)
{
	struct videosync_dev *dev = (struct videosync_dev *)data;

	pr_info("videosync_thread started\n");

	set_freezable();

	while (!kthread_should_stop())
		videosync_sleep(dev);

	pr_info("videosync_thread exit\n");
	return 0;
}


static int __init videosync_init(void)
{
	int ret = -1, i;
	struct device *devp;

	ret = class_register(&videosync_class);
	if (ret < 0)
		return ret;
	ret = register_chrdev(VIDEOSYNC_MAJOR, "videosync", &videosync_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for videosync device\n");
		goto error1;
	}

	devp = device_create(&videosync_class,
				NULL,
				MKDEV(VIDEOSYNC_MAJOR, 0),
				NULL,
				VIDEOSYNC_DEVICE_NAME);
	if (IS_ERR(devp)) {
		pr_err("failed to create videosync device node\n");
		ret = PTR_ERR(devp);
		return ret;
	}

	vp_dev = kzalloc(sizeof(*vp_dev), GFP_KERNEL);
	if (!vp_dev)
		return -ENOMEM;

	vp_dev->video_prov = kzalloc(
		VIDEOSYNC_S_COUNT * sizeof(struct videosync_s),
		GFP_KERNEL);
	if (!vp_dev->video_prov)
		return -ENOMEM;

	mutex_init(&vp_dev->vp_mutex);
	spin_lock_init(&vp_dev->dev_s_num_slock);

	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		ret = videosync_create_instance(i);
		if (ret) {
			pr_err("videosync: error %d while create instance\n",
				ret);
			goto error1;
		}
	}

	init_completion(&vp_dev->thread_active);
	vp_dev->kthread = kthread_run(videosync_thread, vp_dev, "videosync");
	videosync_inited = true;
	return ret;

error1:
	kfree(vp_dev);
	vp_dev = NULL;
	unregister_chrdev(VIDEOSYNC_MAJOR, "videosync");
	class_unregister(&videosync_class);
	return ret;
}

static void __exit videosync_exit(void)
{
	int i, ret;

	complete(&vp_dev->thread_active);

	if (vp_dev->kthread) {
		ret = kthread_stop(vp_dev->kthread);
		if (ret < 0)
			pr_info("%s, kthread_stop return %d.\n", __func__, ret);
		vp_dev->kthread = NULL;
	}
	videosync_inited = false;

	for (i = 0; i < VIDEOSYNC_S_COUNT; i++)
		videosync_destroy_instance(i);

	kfree(vp_dev->video_prov);
	vp_dev->video_prov = NULL;

	kfree(vp_dev);
	vp_dev = NULL;

	device_destroy(&videosync_class, MKDEV(VIDEOSYNC_MAJOR, 0));
	unregister_chrdev(VIDEOSYNC_MAJOR, VIDEOSYNC_DEVICE_NAME);
	class_unregister(&videosync_class);
}

module_init(videosync_init);
module_exit(videosync_exit);

MODULE_PARM_DESC(smooth_sync_enable, "\n smooth_sync_enable\n");
module_param(smooth_sync_enable, bool, 0664);

MODULE_PARM_DESC(vp_debug_flag, "\n vp_debug_flag\n");
module_param(vp_debug_flag, uint, 0664);

MODULE_PARM_DESC(show_nosync, "\n show_nosync\n");
module_param(show_nosync, bool, 0664);

MODULE_PARM_DESC(no_render, "\n no_render\n");
module_param(no_render, bool, 0664);

MODULE_PARM_DESC(async_mode, "\n async_mode\n");
module_param(async_mode, bool, 0664);

MODULE_PARM_DESC(video_early_threshold, "\n video_early_threshold\n");
module_param(video_early_threshold, uint, 0664);

