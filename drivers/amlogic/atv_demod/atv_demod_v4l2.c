/*
 * amlogic atv demod driver
 *
 * Author: nengwen.chen <nengwen.chen@amlogic.com>
 *
 *
 * Copyright (C) 2018 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/semaphore.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/freezer.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>

#include "atvdemod_func.h"

#include "atv_demod_debug.h"
#include "atv_demod_v4l2.h"
#include "atv_demod_driver.h"
#include "atv_demod_ops.h"


#define DEVICE_NAME "v4l2_frontend"

static DEFINE_MUTEX(v4l2_fe_mutex);
/* static int v4l2_shutdown_timeout;*/


static int v4l2_frontend_get_event(struct v4l2_frontend *v4l2_fe,
		struct v4l2_frontend_event *event, int flags)
{
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;
	struct v4l2_fe_events *events = &fepriv->events;

	pr_dbg("%s.\n", __func__);

	if (events->overflow) {
		events->overflow = 0;
		return -EOVERFLOW;
	}

	if (events->eventw == events->eventr) {
		int ret;

		if (flags & O_NONBLOCK)
			return -EWOULDBLOCK;

		up(&fepriv->sem);

		ret = wait_event_interruptible(events->wait_queue,
				events->eventw != events->eventr);

		if (down_interruptible(&fepriv->sem))
			return -ERESTARTSYS;

		if (ret < 0) {
			pr_err("ret = %d.\n", ret);
			return ret;
		}
	}

	mutex_lock(&events->mtx);
	*event = events->events[events->eventr];
	events->eventr = (events->eventr + 1) % MAX_EVENT;
	mutex_unlock(&events->mtx);

	return 0;
}

static void v4l2_frontend_add_event(struct v4l2_frontend *v4l2_fe,
		enum v4l2_status status)
{
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;
	struct v4l2_fe_events *events = &fepriv->events;
	struct v4l2_frontend_event *e = NULL;
	int wp;

	pr_dbg("%s.\n", __func__);

	mutex_lock(&events->mtx);

	wp = (events->eventw + 1) % MAX_EVENT;
	if (wp == events->eventr) {
		events->overflow = 1;
		events->eventr = (events->eventr + 1) % MAX_EVENT;
	}

	e = &events->events[events->eventw];
	e->status = status;

	memcpy(&e->parameters, &v4l2_fe->params,
			sizeof(struct v4l2_analog_parameters));

	events->eventw = wp;

	mutex_unlock(&events->mtx);

	wake_up_interruptible(&events->wait_queue);
}

static void v4l2_frontend_wakeup(struct v4l2_frontend *v4l2_fe)
{
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	fepriv->wakeup = 1;
	wake_up_interruptible(&fepriv->wait_queue);
}

static void v4l2_frontend_clear_events(struct v4l2_frontend *v4l2_fe)
{
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;
	struct v4l2_fe_events *events = &fepriv->events;

	mutex_lock(&events->mtx);
	events->eventr = events->eventw;
	mutex_unlock(&events->mtx);
}

static int v4l2_frontend_is_exiting(struct v4l2_frontend *v4l2_fe)
{
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	if (fepriv->exit != V4L2_FE_NO_EXIT)
		return 1;
#if 0
	if (time_after_eq(jiffies, fepriv->release_jiffies +
			v4l2_shutdown_timeout * HZ))
		return 1;
#endif
	return 0;
}

static int v4l2_frontend_should_wakeup(struct v4l2_frontend *v4l2_fe)
{
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	if (fepriv->wakeup) {
		fepriv->wakeup = 0;
		return 1;
	}

	return v4l2_frontend_is_exiting(v4l2_fe);
}

static int v4l2_frontend_thread(void *data)
{
	struct v4l2_frontend *v4l2_fe = (struct v4l2_frontend *) data;
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;
	enum v4l2_status s = V4L2_TIMEDOUT;
	unsigned long timeout = 0;

	pr_info("%s: thread start.\n", __func__);

	fepriv->delay = 3 * HZ;
	fepriv->status = 0;
	fepriv->wakeup = 0;

	set_freezable();
	while (1) {
		up(&fepriv->sem); /* is locked when we enter the thread... */
restart:
		timeout = wait_event_interruptible_timeout(fepriv->wait_queue,
				v4l2_frontend_should_wakeup(v4l2_fe)
				|| kthread_should_stop()
				|| freezing(current), fepriv->delay);

		if (kthread_should_stop() ||
				v4l2_frontend_is_exiting(v4l2_fe)) {
			/* got signal or quitting */
			fepriv->exit = V4L2_FE_NORMAL_EXIT;
			break;
		}

		if (try_to_freeze())
			goto restart;

		if (down_interruptible(&fepriv->sem))
			break;

		/* pr_dbg("%s: state = %d.\n", __func__, fepriv->state); */
		if (fepriv->state & V4L2FE_STATE_RETUNE) {
			pr_dbg("%s: Retune requested, V4L2FE_STATE_RETUNE.\n",
					__func__);
			fepriv->state = V4L2FE_STATE_TUNED;
		}
		/* Case where we are going to search for a carrier
		 * User asked us to retune again
		 * for some reason, possibly
		 * requesting a search with a new set of parameters
		 */
		if ((fepriv->algo_status & V4L2_SEARCH_AGAIN)
			&& !(fepriv->state & V4L2FE_STATE_IDLE)) {
			if (v4l2_fe->ops.search) {
				fepriv->algo_status =
						v4l2_fe->ops.search(v4l2_fe);
			/* We did do a search as was requested,
			 * the flags are now unset as well and has
			 * the flags wrt to search.
			 */
			} else {
				fepriv->algo_status &= ~V4L2_SEARCH_AGAIN;
			}
		}
		/* Track the carrier if the search was successful */
		if (fepriv->algo_status == V4L2_SEARCH_SUCCESS) {
			s = FE_HAS_LOCK;
		} else {
			/*dev->algo_status |= AML_ATVDEMOD_ALGO_SEARCH_AGAIN;*/
			if (fepriv->algo_status != V4L2_SEARCH_INVALID) {
				fepriv->delay = HZ / 2;
				s = V4L2_TIMEDOUT;
			}
		}

		if (s != fepriv->status) {
			/* update event list */
			v4l2_frontend_add_event(v4l2_fe, s);
			fepriv->status = s;
			if (!(s & FE_HAS_LOCK)) {
				fepriv->delay = HZ / 10;
				fepriv->algo_status |= V4L2_SEARCH_AGAIN;
			} else {
				fepriv->delay = 60 * HZ;
			}
		}
	}

	fepriv->thread = NULL;
	if (kthread_should_stop())
		fepriv->exit = V4L2_FE_DEVICE_REMOVED;
	else
		fepriv->exit = V4L2_FE_NO_EXIT;

	/* memory barrier */
	mb();

	v4l2_frontend_wakeup(v4l2_fe);

	pr_dbg("%s: thread exit state = %d.\n", __func__, fepriv->state);

	return 0;
}

static void v4l2_frontend_stop(struct v4l2_frontend *v4l2_fe)
{
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	pr_dbg("%s.\n", __func__);

	fepriv->exit = V4L2_FE_NORMAL_EXIT;

	/* memory barrier */
	mb();

	if (!fepriv->thread)
		return;

	kthread_stop(fepriv->thread);

	sema_init(&fepriv->sem, 1);
	fepriv->state = V4L2FE_STATE_IDLE;

	/* paranoia check in case a signal arrived */
	if (fepriv->thread)
		pr_info("%s: warning: thread %p won't exit\n",
				__func__, fepriv->thread);
}

static int v4l2_frontend_start(struct v4l2_frontend *v4l2_fe)
{
	int ret = 0;
	struct task_struct *thread = NULL;
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	pr_dbg("%s.\n", __func__);

	if (fepriv->thread) {
		if (fepriv->exit == V4L2_FE_NO_EXIT)
			return 0;

		v4l2_frontend_stop(v4l2_fe);
	}

	if (signal_pending(current))
		return -EINTR;

	if (down_interruptible(&fepriv->sem))
		return -EINTR;

	fepriv->state = V4L2FE_STATE_IDLE;
	fepriv->exit = V4L2_FE_NO_EXIT;
	fepriv->thread = NULL;

	/* memory barrier */
	mb();

	thread = kthread_run(v4l2_frontend_thread, v4l2_fe,
			"v4l2_frontend_thread");
	if (IS_ERR(thread)) {
		ret = PTR_ERR(thread);
		pr_err("%s: failed to start kthread (%d)\n", __func__, ret);
		up(&fepriv->sem);
		return ret;
	}

	fepriv->thread = thread;

	return 0;
}


static int v4l2_set_frontend(struct v4l2_frontend *v4l2_fe,
		struct v4l2_analog_parameters *params)
{
	u32 freq_min = 0;
	u32 freq_max = 0;
	struct analog_parameters p;
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;
	struct dvb_frontend *fe = &v4l2_fe->fe;

	pr_dbg("%s.\n", __func__);

	freq_min = fe->ops.tuner_ops.info.frequency_min;
	freq_max = fe->ops.tuner_ops.info.frequency_max;

	if (freq_min == 0 || freq_max == 0)
		pr_info("%s: demod or tuner frequency limits undefined.\n",
				__func__);

	/* range check: frequency */
	if ((freq_min && params->frequency < freq_min) ||
			(freq_max && params->frequency > freq_max)) {
		pr_err("%s: frequency %u out of range (%u..%u).\n",
				__func__, params->frequency,
				freq_min, freq_max);
		return -EINVAL;
	}

	/*
	 * Initialize output parameters to match the values given by
	 * the user. FE_SET_FRONTEND triggers an initial frontend event
	 * with status = 0, which copies output parameters to userspace.
	 */
	//dtv_property_legacy_params_sync_ex(fe, &fepriv->parameters_out);
	memcpy(&v4l2_fe->params, params, sizeof(struct v4l2_analog_parameters));

	fepriv->state = V4L2FE_STATE_RETUNE;

	/* Request the search algorithm to search */
	fepriv->algo_status |= V4L2_SEARCH_AGAIN;
	if (params->flag & ANALOG_FLAG_ENABLE_AFC) {
		/*dvb_frontend_add_event(fe, 0); */
		v4l2_frontend_clear_events(v4l2_fe);
		v4l2_frontend_wakeup(v4l2_fe);
	} else if (fe->ops.analog_ops.set_params) {
		/* TODO:*/
		p.frequency = params->frequency;
		p.mode = params->afc_range;
		p.std = params->std;
		p.audmode = params->audmode;
		fe->ops.analog_ops.set_params(fe, &p);
	}

	fepriv->status = 0;

	return 0;
}

static int v4l2_get_frontend(struct v4l2_frontend *v4l2_fe,
		struct v4l2_analog_parameters *p)
{
	pr_dbg("%s.\n", __func__);

	memcpy(p, &v4l2_fe->params, sizeof(struct v4l2_analog_parameters));

	return 0;
}

static int v4l2_frontend_set_mode(struct v4l2_frontend *v4l2_fe,
		int params)
{
	int ret = 0;
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;
	struct analog_demod_ops *analog_ops = NULL;
	int priv_cfg = 0;

	pr_dbg("%s: params = %d.\n", __func__, params);

	fepriv->state = V4L2FE_STATE_IDLE;

	analog_ops = &v4l2_fe->fe.ops.analog_ops;

	if (params)
		priv_cfg = AML_ATVDEMOD_INIT;
	else
		priv_cfg = AML_ATVDEMOD_UNINIT;

	if (analog_ops && analog_ops->set_config)
		ret = analog_ops->set_config(&v4l2_fe->fe, &priv_cfg);

	return ret;
}

static int v4l2_frontend_read_status(struct v4l2_frontend *v4l2_fe,
		enum v4l2_status *status)
{
	int ret = 0;
	struct analog_demod_ops *analog_ops = NULL;
	struct dvb_tuner_ops *tuner_ops = NULL;

	analog_ops = &v4l2_fe->fe.ops.analog_ops;
	tuner_ops = &v4l2_fe->fe.ops.tuner_ops;

	if (!status)
		return -1;
#if 0
	if (analog_ops->tuner_status)
		analog_ops->tuner_status(&v4l2_fe->fe, status);
	else if (tuner_ops->get_status)
		tuner_ops->get_status(&v4l2_fe->fe, status);
#endif

	return ret;
}

static void v4l2_frontend_vdev_release(struct video_device *dev)
{
	pr_dbg("%s.\n", __func__);
}

static ssize_t v4l2_frontend_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	pr_dbg("%s.\n", __func__);
	return 0;
}

static ssize_t v4l2_frontend_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	pr_dbg("%s.\n", __func__);
	return 0;
}

static unsigned int v4l2_frontend_poll(struct file *filp,
		struct poll_table_struct *pts)
{
	struct v4l2_frontend *v4l2_fe = video_get_drvdata(video_devdata(filp));
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	poll_wait(filp, &fepriv->events.wait_queue, pts);

	if (fepriv->events.eventw != fepriv->events.eventr) {
		pr_dbg("%s： POLLIN | POLLRDNORM | POLLPRI.\n", __func__);
		return (POLLIN | POLLRDNORM | POLLPRI);
	}

	return 0;
}

static void v4l2_property_dump(struct v4l2_frontend *v4l2_fe,
		bool is_set, struct v4l2_property *tvp)
{

	/*int i = 0;*/

	if (tvp->cmd <= 0 || tvp->cmd > DTV_MAX_COMMAND) {
		pr_warn("%s: %s tvp.cmd = 0x%08x undefined\n",
				__func__,
				is_set ? "SET" : "GET",
				tvp->cmd);
		return;
	}
#if 0
	pr_dbg("%s: %s tvp.cmd    = 0x%08x (%s)\n", __func__,
		is_set ? "SET" : "GET",
		tvp->cmd,
		v4l2_cmds[tvp->cmd].name);

	if (v4l2_cmds[tvp->cmd].buffer) {
		pr_dbg("%s: tvp.u.buffer.len = 0x%02x\n",
			__func__, tvp->u.buffer.len);

		for (i = 0; i < tvp->u.buffer.len; i++)
			pr_dbg("%s: tvp.u.buffer.data[0x%02x] = 0x%02x\n",
					__func__, i, tvp->u.buffer.data[i]);
	} else {
		pr_dbg("%s: tvp.u.data = 0x%08x\n", __func__,
				tvp->u.data);
	}
#endif
}

static int v4l2_property_process_set(struct v4l2_frontend *v4l2_fe,
		struct v4l2_property *tvp, struct file *file)
{
	int r = 0;

	v4l2_property_dump(v4l2_fe, true, tvp);

	switch (tvp->cmd) {
	case V4L2_TUNE:
		break;
	case V4L2_SOUND_SYS:
	case V4L2_SLOW_SEARCH_MODE:
		/* Allow the frontend to override outgoing properties */
		if (v4l2_fe->ops.set_property) {
			r = v4l2_fe->ops.set_property(v4l2_fe, tvp);
			if (r < 0)
				return r;
		}
		break;
	default:
		return -EINVAL;
	}

	return r;
}

static int v4l2_property_process_get(struct v4l2_frontend *v4l2_fe,
		struct v4l2_property *tvp, struct file *file)
{
	int r = 0;

	switch (tvp->cmd) {
	case V4L2_SOUND_SYS:
	case V4L2_SLOW_SEARCH_MODE:
		/* Allow the frontend to override outgoing properties */
		if (v4l2_fe->ops.get_property) {
			r = v4l2_fe->ops.get_property(v4l2_fe, tvp);
			if (r < 0)
				return r;
		}
		break;

	default:
		pr_dbg("%s: V4L2 property %d doesn't exist\n",
				__func__, tvp->cmd);
		return -EINVAL;
	}

	v4l2_property_dump(v4l2_fe, false, tvp);

	return 0;
}

static int v4l2_frontend_ioctl_properties(struct file *filp,
			unsigned int cmd, void *parg)
{
	struct v4l2_frontend *v4l2_fe = video_get_drvdata(video_devdata(filp));
	int err = 0;

	struct v4l2_properties *tvps = parg;
	struct v4l2_property *tvp = NULL;
	int i = 0;

	pr_dbg("%s: cmd = 0x%x.\n", __func__, cmd);

	if (cmd == V4L2_SET_PROPERTY) {
		pr_dbg("%s: properties.num = %d\n", __func__, tvps->num);
		pr_dbg("%s: properties.props = %p\n", __func__, tvps->props);

		/* Put an arbitrary limit on the number of messages that can
		 * be sent at once
		 */
		if ((tvps->num == 0) || (tvps->num > V4L2_IOCTL_MAX_MSGS))
			return -EINVAL;

		tvp = memdup_user(tvps->props, tvps->num * sizeof(*tvp));
		if (IS_ERR(tvp))
			return PTR_ERR(tvp);

		for (i = 0; i < tvps->num; i++) {
			err = v4l2_property_process_set(v4l2_fe, tvp + i, filp);
			if (err < 0)
				goto out;
			(tvp + i)->result = err;
		}
	} else if (cmd == V4L2_GET_PROPERTY) {
		pr_dbg("%s: properties.num = %d\n", __func__, tvps->num);
		pr_dbg("%s: properties.props = %p\n", __func__, tvps->props);

		/* Put an arbitrary limit on the number of messages that can
		 * be sent at once
		 */
		if ((tvps->num == 0) || (tvps->num > V4L2_IOCTL_MAX_MSGS))
			return -EINVAL;

		tvp = memdup_user(tvps->props, tvps->num * sizeof(*tvp));
		if (IS_ERR(tvp))
			return PTR_ERR(tvp);

		/*
		 * Let's use our own copy of property cache, in order to
		 * avoid mangling with DTV zigzag logic, as drivers might
		 * return crap, if they don't check if the data is available
		 * before updating the properties cache.
		 */
#if 0
		if (fepriv->state != V4L2FE_STATE_IDLE) {
			err = v4l2_get_frontend(v4l2_fe, &getp, NULL);
			if (err < 0)
				goto out;
		}
#endif
		for (i = 0; i < tvps->num; i++) {
			err = v4l2_property_process_get(v4l2_fe, tvp + i, filp);
			if (err < 0)
				goto out;
			(tvp + i)->result = err;
		}

		if (copy_to_user((void __user *)tvps->props, tvp,
				 tvps->num * sizeof(struct v4l2_property))) {
			err = -EFAULT;
			goto out;
		}

	} else
		err = -EOPNOTSUPP;

out:
	kfree(tvp);
	return err;
}

static long v4l2_frontend_ioctl(struct file *filp, void *fh, bool valid_prio,
		unsigned int cmd, void *arg)
{
	int ret = 0;
	int need_lock = 1;
	struct v4l2_frontend *v4l2_fe = video_get_drvdata(video_devdata(filp));
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	pr_dbg("%s: cmd = 0x%x.\n", __func__, cmd);
	if (fepriv->exit != V4L2_FE_NO_EXIT)
		return -ENODEV;

	if (cmd == V4L2_READ_STATUS || cmd == V4L2_GET_FRONTEND)
		need_lock = 0;

	if (need_lock)
		if (down_interruptible(&fepriv->sem))
			return -ERESTARTSYS;

	if (fepriv->exit != DVB_FE_NO_EXIT) {
		up(&fepriv->sem);
		return -ENODEV;
	}

	switch (cmd) {
	case V4L2_SET_FRONTEND: /* 0x40285669 */
		ret = v4l2_set_frontend(v4l2_fe,
				(struct v4l2_analog_parameters *) arg);
		break;

	case V4L2_GET_FRONTEND:
		ret = v4l2_get_frontend(v4l2_fe,
				(struct v4l2_analog_parameters *) arg);
		break;

	case V4L2_GET_EVENT: /* 0x8030566b */
		ret = v4l2_frontend_get_event(v4l2_fe,
				(struct v4l2_frontend_event *) arg,
				filp->f_flags);
		break;

	case V4L2_SET_MODE: /* 0x4004566c */
		ret = v4l2_frontend_set_mode(v4l2_fe, *((int *) arg));
		break;

	case V4L2_READ_STATUS:
		ret = v4l2_frontend_read_status(v4l2_fe,
				(enum v4l2_status *) arg);
		break;

	case V4L2_SET_PROPERTY: /* 0xc010566e */
	case V4L2_GET_PROPERTY: /* 0xc010566f */
		ret = v4l2_frontend_ioctl_properties(filp, cmd, arg);
		break;

	default:
		pr_warn("%s: Unsupport cmd = 0x%x.\n", __func__, cmd);
		break;
	}

	if (need_lock)
		up(&fepriv->sem);

	return ret;
}

static int v4l2_frontend_open(struct file *filp)
{
	int ret = 0;
	struct v4l2_frontend *v4l2_fe = video_get_drvdata(video_devdata(filp));
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	/* Because tuner ko insmod after demod, so need check */
	if (!amlatvdemod_devp->tuner_attached
			|| !amlatvdemod_devp->analog_attached) {
		ret = aml_attach_demod_tuner(amlatvdemod_devp);
		if (ret < 0) {
			pr_err("%s: attach demod or tuner %d error.\n",
					__func__, v4l2_fe->tuner_id);
			return -EBUSY;
		}
	}

	ret = v4l2_frontend_start(v4l2_fe);

	fepriv->events.eventr = fepriv->events.eventw = 0;

	pr_info("%s: %s OK.\n", __func__, fepriv->v4l2dev->name);

	return ret;
}

static int v4l2_frontend_release(struct file *filp)
{
	pr_info("%s: OK.\n", __func__);

	return 0;
}

static const struct v4l2_file_operations v4l2_fe_fops = {
	.owner          = THIS_MODULE,
	.read           = v4l2_frontend_read,
	.write          = v4l2_frontend_write,
	.poll           = v4l2_frontend_poll,
	.unlocked_ioctl = video_ioctl2,
	.open           = v4l2_frontend_open,
	.release        = v4l2_frontend_release,
};

static int v4l2_frontend_vidioc_g_audio(struct file *file, void *fh,
		struct v4l2_audio *a)
{
	pr_dbg("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_s_audio(struct file *filp, void *fh,
		const struct v4l2_audio *a)
{
	pr_dbg("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_g_tuner(struct file *filp, void *fh,
				struct v4l2_tuner *a)
{
	pr_dbg("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_s_tuner(struct file *filp, void *fh,
		const struct v4l2_tuner *a)
{
	pr_dbg("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_g_frequency(struct file *filp, void *fh,
		struct v4l2_frequency *a)
{
	pr_dbg("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_s_frequency(struct file *filp, void *fh,
		const struct v4l2_frequency *a)
{
	pr_dbg("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_enum_freq_bands(struct file *filp, void *fh,
		struct v4l2_frequency_band *band)
{
	pr_dbg("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_g_modulator(struct file *filp, void *fh,
		struct v4l2_modulator *a)
{
	pr_dbg("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_s_modulator(struct file *filp, void *fh,
		const struct v4l2_modulator *a)
{
	pr_dbg("%s.\n", __func__);
	return 0;
}

int v4l2_frontend_vidioc_streamon(struct file *filp, void *fh,
		enum v4l2_buf_type i)
{
	pr_dbg("%s.\n", __func__);
	return 0;
}

int v4l2_frontend_vidioc_streamoff(struct file *filp, void *fh,
		enum v4l2_buf_type i)
{
	pr_dbg("%s.\n", __func__);
	return 0;
}

static const struct v4l2_ioctl_ops v4l2_fe_ioctl_ops = {
	.vidioc_g_audio         = v4l2_frontend_vidioc_g_audio,
	.vidioc_s_audio         = v4l2_frontend_vidioc_s_audio,
	.vidioc_g_tuner         = v4l2_frontend_vidioc_g_tuner,
	.vidioc_s_tuner         = v4l2_frontend_vidioc_s_tuner,
	.vidioc_g_frequency     = v4l2_frontend_vidioc_g_frequency,
	.vidioc_s_frequency     = v4l2_frontend_vidioc_s_frequency,
	.vidioc_enum_freq_bands = v4l2_frontend_vidioc_enum_freq_bands,
	.vidioc_g_modulator     = v4l2_frontend_vidioc_g_modulator,
	.vidioc_s_modulator     = v4l2_frontend_vidioc_s_modulator,
	.vidioc_streamon        = v4l2_frontend_vidioc_streamon,
	.vidioc_streamoff       = v4l2_frontend_vidioc_streamoff,
	.vidioc_default         = v4l2_frontend_ioctl,
};

static struct video_device aml_atvdemod_vdev = {
	.fops      = &v4l2_fe_fops,
	.ioctl_ops = &v4l2_fe_ioctl_ops,
	.name      = DEVICE_NAME,
	.release   = v4l2_frontend_vdev_release,
	.vfl_dir   = VFL_DIR_TX,
};

int v4l2_resister_frontend(struct v4l2_frontend *v4l2_fe)
{
	int ret = 0;
	struct v4l2_frontend_private *fepriv = NULL;
	struct v4l2_atvdemod_device *v4l2dev = NULL;

	if (!v4l2_fe) {
		pr_err("v4l2_fe NULL pointer.\n");
		return -1;
	}

	if (mutex_lock_interruptible(&v4l2_fe_mutex))
		return -ERESTARTSYS;

	v4l2_fe->frontend_priv = kzalloc(sizeof(struct v4l2_frontend_private),
			GFP_KERNEL);
	if (!v4l2_fe->frontend_priv) {
		mutex_unlock(&v4l2_fe_mutex);
		pr_err("kzalloc fail.\n");
		return -ENOMEM;
	}

	fepriv = v4l2_fe->frontend_priv;

	fepriv->v4l2dev = kzalloc(sizeof(struct v4l2_atvdemod_device),
			GFP_KERNEL);
	if (!fepriv->v4l2dev) {
		ret = -ENOMEM;
		goto malloc_fail;
	}

	v4l2dev = fepriv->v4l2dev;
	v4l2dev->name = DEVICE_NAME;
	v4l2dev->dev = v4l2_fe->dev;

	snprintf(v4l2dev->v4l2_dev.name, sizeof(v4l2dev->v4l2_dev.name),
			"%s", DEVICE_NAME);

	ret = v4l2_device_register(v4l2dev->dev, &v4l2dev->v4l2_dev);
	if (ret) {
		pr_err("register v4l2_device fail.\n");
		goto v4l2_fail;
	}

	v4l2dev->video_dev = &aml_atvdemod_vdev;
	v4l2dev->video_dev->v4l2_dev = &v4l2dev->v4l2_dev;

	video_set_drvdata(v4l2dev->video_dev, v4l2_fe);

	v4l2dev->video_dev->dev.init_name = DEVICE_NAME;
	ret = video_register_device(v4l2dev->video_dev,
			VFL_TYPE_GRABBER, 36);/* -1 --> 36 */
	if (ret) {
		pr_err("register video device fail.\n");
		goto vdev_fail;
	}

	sema_init(&fepriv->sem, 1);
	init_waitqueue_head(&fepriv->wait_queue);
	init_waitqueue_head(&fepriv->events.wait_queue);
	mutex_init(&fepriv->events.mtx);

	mutex_unlock(&v4l2_fe_mutex);

	pr_info("%s: OK.\n", __func__);

	return 0;

vdev_fail:
	v4l2_device_unregister(&v4l2dev->v4l2_dev);

malloc_fail:
	kfree(v4l2_fe->frontend_priv);
v4l2_fail:
	mutex_unlock(&v4l2_fe_mutex);
	pr_info("%s: Fail.\n", __func__);

	return ret;
}

int v4l2_unresister_frontend(struct v4l2_frontend *v4l2_fe)
{
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	mutex_lock(&v4l2_fe_mutex);

	video_unregister_device(fepriv->v4l2dev->video_dev);
	v4l2_device_unregister(&fepriv->v4l2dev->v4l2_dev);

	v4l2_frontend_stop(v4l2_fe);

	mutex_unlock(&v4l2_fe_mutex);

	pr_info("%s: OK.\n", __func__);

	return 0;
}

void v4l2_frontend_detach(struct v4l2_frontend *v4l2_fe)
{
	if (v4l2_fe->fe.ops.tuner_ops.release)
		v4l2_fe->fe.ops.tuner_ops.release(&v4l2_fe->fe);
	if (v4l2_fe->fe.ops.analog_ops.release)
		v4l2_fe->fe.ops.analog_ops.release(&v4l2_fe->fe);
	if (v4l2_fe->fe.ops.release)
		v4l2_fe->fe.ops.release(&v4l2_fe->fe);
}

int v4l2_frontend_suspend(struct v4l2_frontend *v4l2_fe)
{
	int ret = 0;
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct dvb_tuner_ops tuner_ops = fe->ops.tuner_ops;
	struct analog_demod_ops analog_ops = fe->ops.analog_ops;

	if (analog_ops.standby)
		analog_ops.standby(fe);

	if (tuner_ops.suspend)
		tuner_ops.suspend(fe);

	pr_info("%s: OK.\n", __func__);

	return ret;
}

int v4l2_frontend_resume(struct v4l2_frontend *v4l2_fe)
{
	int ret = 0;
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct dvb_tuner_ops tuner_ops = fe->ops.tuner_ops;
	struct analog_demod_ops analog_ops = fe->ops.analog_ops;
	int priv_cfg = AML_ATVDEMOD_RESUME;

	if (analog_ops.set_config)
		analog_ops.set_config(fe, &priv_cfg);

	if (tuner_ops.resume)
		tuner_ops.resume(fe);

	fepriv->state = V4L2FE_STATE_RETUNE;
	v4l2_frontend_wakeup(v4l2_fe);

	pr_info("%s: OK.\n", __func__);

	return ret;
}
