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


#define AFC_BEST_LOCK    50
#define ATV_AFC_500KHZ   500000
#define ATV_AFC_1_0MHZ   1000000
#define ATV_AFC_2_0MHZ   2000000

static int tuner_status_cnt = 8; /* 4-->16 test on sky mxl661 */
module_param(tuner_status_cnt, int, 0644);
MODULE_DESCRIPTION("after write a freq, max cnt value of read tuner status\n");

static int slow_mode;
module_param(slow_mode, int, 0644);
MODULE_DESCRIPTION("search the channel by slow_mode,by add +1MHz\n");

typedef int (*hook_func_t) (void);
hook_func_t aml_fe_hook_atv_status;
hook_func_t aml_fe_hook_hv_lock;
hook_func_t aml_fe_hook_get_fmt;

void aml_fe_hook_cvd(hook_func_t atv_mode, hook_func_t cvd_hv_lock,
	hook_func_t get_fmt)
{
	aml_fe_hook_atv_status = atv_mode;
	aml_fe_hook_hv_lock = cvd_hv_lock;
	aml_fe_hook_get_fmt = get_fmt;
	pr_dbg("[aml_fe]%s\n", __func__);
}
EXPORT_SYMBOL(aml_fe_hook_cvd);

static v4l2_std_id demod_fmt_2_v4l2_std(int fmt)
{
	v4l2_std_id std = 0;

	switch (fmt) {
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK:
		std = V4L2_STD_PAL_DK;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I:
		std = V4L2_STD_PAL_I;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG:
		std = V4L2_STD_PAL_BG;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M:
		std = V4L2_STD_PAL_M;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_DK:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_I:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_BG:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_M:
		std = V4L2_STD_NTSC_M;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L:
		std = V4L2_STD_SECAM_L;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK2:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK3:
		std = V4L2_STD_SECAM_DK;
		break;
	default:
		pr_err("%s: unsupport fmt: 0x%0x.\n", __func__, fmt);
	}

	return std;
}

static v4l2_std_id trans_tvin_fmt_to_v4l2_std(int fmt)
{
	v4l2_std_id std = 0;

	switch (fmt) {
	case TVIN_SIG_FMT_CVBS_NTSC_M:
		std = V4L2_STD_NTSC;
		break;
	case TVIN_SIG_FMT_CVBS_NTSC_443:
		std = V4L2_STD_NTSC_443;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_I:
		std = V4L2_STD_PAL_I;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_M:
		std = V4L2_STD_PAL_M;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_60:
		std = V4L2_STD_PAL_60;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_CN:
		std = V4L2_STD_PAL_Nc;
		break;
	case TVIN_SIG_FMT_CVBS_SECAM:
		std = V4L2_STD_SECAM;
		break;
	default:
		pr_err("%s err fmt: 0x%x\n", __func__, fmt);
		break;
	}
	return std;
}

static void v4l2_fe_try_analog_format(struct v4l2_frontend *v4l2_fe,
		bool auto_search_std, v4l2_std_id *video_fmt, int *audio_fmt)
{
	struct dvb_frontend *fe = &v4l2_fe->v4l2_ad->fe;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	struct analog_parameters params;
	int i = 0;
	int try_vfmt_cnt = 300;
	int varify_cnt = 0;
	v4l2_std_id std_bk = 0;
	int audio = 0;

	if (auto_search_std == true) {
		for (i = 0; i < try_vfmt_cnt; i++) {
			if (aml_fe_hook_get_fmt == NULL) {
				pr_err("%s: aml_fe_hook_get_fmt == NULL.\n",
						__func__);
				break;
			}
			std_bk = aml_fe_hook_get_fmt();
			if (std_bk) {
				varify_cnt++;
				pr_dbg("get varify_cnt:%d, cnt:%d, std_bk:0x%x\n",
						varify_cnt, i,
						(unsigned int) std_bk);
				if ((v4l2_fe->v4l2_ad->tuner_id == AM_TUNER_R840
					&& varify_cnt > 0)
					|| varify_cnt > 3)
					break;
			}

			if (i == (try_vfmt_cnt / 3) ||
				(i == (try_vfmt_cnt / 3) * 2)) {
				/* Before enter search,
				 * need set the std,
				 * then, try others std.
				 */
				if (p->std & V4L2_STD_PAL)
					p->std = V4L2_STD_NTSC_M;
				else if (p->std & V4L2_STD_NTSC)
					p->std = V4L2_STD_SECAM_L;
				else if (p->std & V4L2_STD_SECAM)
					p->std = V4L2_STD_PAL_DK;

				p->frequency += 1;
				params.frequency = p->frequency;
				params.mode = p->flag;
				params.audmode = p->audmode;
				params.std = p->std;

				fe->ops.analog_ops.set_params(fe, &params);
			}
			usleep_range(30 * 1000, 30 * 1000 + 100);
		}

		pr_dbg("get std_bk cnt:%d, std_bk: 0x%x\n",
				i, (unsigned int) std_bk);

		if (std_bk == 0) {
			pr_err("[%s] failed to get v fmt !!\n", __func__);
			pr_err("[%s] vfmt assume PAL !!\n", __func__);
			std_bk = TVIN_SIG_FMT_CVBS_PAL_I;
			p->std = V4L2_STD_PAL_I;
			p->frequency += 1;
			params.frequency = p->frequency;
			params.mode = p->flag;
			params.audmode = p->audmode;
			params.std = p->std;

			fe->ops.analog_ops.set_params(fe, &params);

			usleep_range(20 * 1000, 20 * 1000 + 100);
		}

		std_bk = trans_tvin_fmt_to_v4l2_std(std_bk);
	} else {
		/* Only search std by user setting,
		 * so no need tvafe identify signal.
		 */
		std_bk = p->std;
	}

	*video_fmt = std_bk;

	if (std_bk == V4L2_STD_NTSC) {
#if 0 /* For TV Signal Generator(TG39) test, NTSC need support other audio.*/
		amlatvdemod_set_std(AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_DK);
		audio = aml_audiomode_autodet(fe);
		audio = demod_fmt_2_v4l2_std(audio);
#if 0
		if (audio == V4L2_STD_PAL_M)
			audio = V4L2_STD_NTSC_M;
		else
			std_bk = V4L2_COLOR_STD_PAL;
#endif
#else /* Now, force to NTSC_M, Ours demod only support M for NTSC.*/
		audio = V4L2_STD_NTSC_M;
#endif
	} else if (std_bk == V4L2_STD_SECAM) {
		audio = V4L2_STD_SECAM_L;
	} else {
		/*V4L2_COLOR_STD_PAL*/
		amlatvdemod_set_std(AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK);
		audio = aml_audiomode_autodet(fe);
		audio = demod_fmt_2_v4l2_std(audio);
#if 0 /* Why do this to me? We need support PAL_M.*/
		if (audio == V4L2_STD_PAL_M) {
			audio = demod_fmt_2_v4l2_std(broad_std_except_pal_m);
			pr_err("select audmode 0x%x\n", audio);
		}
#endif
	}

	*audio_fmt = audio;
}

static int v4l2_fe_afc_closer(struct v4l2_frontend *v4l2_fe, int minafcfreq,
		int maxafcfreq, int isAutoSearch)
{
	struct dvb_frontend *fe = &v4l2_fe->v4l2_ad->fe;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	struct analog_parameters params;
	int afc = 100;
	__u32 set_freq;
	int count = 25;
	int lock_cnt = 0;
	static int freq_success;
	static int temp_freq, temp_afc;
	struct timespec time_now;
	static struct timespec success_time;

	pr_dbg("[%s] freq_success: %d, freq: %d, minfreq: %d, maxfreq: %d\n",
		__func__, freq_success, p->frequency, minafcfreq, maxafcfreq);

	/* avoid more search the same program, except < 45.00Mhz */
	if (abs(p->frequency - freq_success) < 3000000
			&& p->frequency > 45000000) {
		ktime_get_ts(&time_now);
		//if (debug_fe & 0x2)
			pr_err("%s: tv_sec now:%ld,tv_sec success:%ld\n",
				__func__, time_now.tv_sec, success_time.tv_sec);
		/* beyond 10s search same frequency is ok */
		if ((time_now.tv_sec - success_time.tv_sec) < 10)
			return -1;
	}

	/*do the auto afc make sure the afc<50k or the range from api */
	if ((fe->ops.analog_ops.get_afc || fe->ops.tuner_ops.get_afc) &&
		fe->ops.tuner_ops.set_analog_params) {

		set_freq = p->frequency;
		while (abs(afc) > AFC_BEST_LOCK) {
			if (v4l2_fe->v4l2_ad->tuner_id == AM_TUNER_SI2151 ||
				v4l2_fe->v4l2_ad->tuner_id == AM_TUNER_R840)
				usleep_range(20 * 1000, 20 * 1000 + 100);
			else if (v4l2_fe->v4l2_ad->tuner_id == AM_TUNER_MXL661)
				usleep_range(30 * 1000, 30 * 1000 + 100);

			if (fe->ops.analog_ops.get_afc &&
			((v4l2_fe->v4l2_ad->tuner_id == AM_TUNER_R840) ||
			(v4l2_fe->v4l2_ad->tuner_id == AM_TUNER_SI2151) ||
			(v4l2_fe->v4l2_ad->tuner_id == AM_TUNER_MXL661)))
				fe->ops.analog_ops.get_afc(fe, &afc);
			else if (fe->ops.tuner_ops.get_afc)
				fe->ops.tuner_ops.get_afc(fe, &afc);

			//if (debug_fe & 0x2)
				pr_err("[%s] get afc %d khz, freq %u.\n",
					__func__, afc, p->frequency);

			if (afc == 0xffff) {
				/*last lock, but this unlock,so try get afc*/
				if (lock_cnt > 0) {
					p->frequency = temp_freq +
							temp_afc * 1000;
					pr_err("%s, force lock, f:%d\n",
							__func__, p->frequency);
					break;
				}

				afc = 500;
			} else {
				lock_cnt++;
				temp_freq = p->frequency;
				if (afc > 50)
					temp_afc = 500;
				else if (afc < -50)
					temp_afc = -500;
				else
					temp_afc = afc;
			}

			if (((abs(afc) > (500 - AFC_BEST_LOCK))
				&& (abs(afc) < (500 + AFC_BEST_LOCK))
				&& (abs(afc) != 500))
				|| ((abs(afc) == 500) && (lock_cnt > 0))) {
				p->frequency += afc * 1000;
				break;
			}

			if (afc >= (500 + AFC_BEST_LOCK))
				afc = 500;

			p->frequency += afc * 1000;

			if (unlikely(p->frequency > maxafcfreq)) {
				pr_err("[%s]:[%d] is exceed maxafcfreq[%d]\n",
					__func__, p->frequency, maxafcfreq);
				p->frequency = set_freq;
				return -1;
			}
#if 0 /*if enable ,it would miss program*/
			if (unlikely(c->frequency < minafcfreq)) {
				pr_dbg("[%s]:[%d] is exceed minafcfreq[%d]\n",
				       __func__, c->frequency, minafcfreq);
				c->frequency = set_freq;
				return -1;
			}
#endif
			if (likely(!(count--))) {
				pr_err("[%s]:exceed the afc count\n", __func__);
				p->frequency = set_freq;
				return -1;
			}

			/* delete it will miss program
			 * when c->frequency equal program frequency
			 */
			p->frequency++;
			if (fe->ops.tuner_ops.set_analog_params) {
				params.frequency = p->frequency;
				params.mode = p->flag;
				params.audmode = p->audmode;
				params.std = p->std;
				fe->ops.tuner_ops.set_analog_params(fe,
						&params);
			}
		}

		freq_success = p->frequency;
		ktime_get_ts(&success_time);
		//if (debug_fe & 0x2)
			pr_err("[%s] get afc %d khz done, freq %u.\n",
				__func__, afc, p->frequency);
	}

	return 0;
}

static enum v4l2_search v4l2_frontend_search(struct v4l2_frontend *v4l2_fe)
{
	struct analog_parameters params;
	struct dvb_frontend *fe = &v4l2_fe->v4l2_ad->fe;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	enum v4l2_status tuner_state = V4L2_TIMEDOUT;
	enum v4l2_status ade_state = V4L2_TIMEDOUT;
	bool pll_lock = false;
	/*struct atv_status_s atv_status;*/
	__u32 set_freq = 0;
	__u32 minafcfreq = 0, maxafcfreq = 0;
	__u32 afc_step = 0;
	int tuner_status_cnt_local = tuner_status_cnt;
	v4l2_std_id std_bk = 0;
	int audio = 0;
	int double_check_cnt = 1;
	bool auto_search_std = false;
	int search_count = 0;
	int ret = -1;

#ifdef DEBUG_TIME_CUS
	unsigned int time_start, time_end, time_delta;

	time_start = jiffies_to_msecs(jiffies);
#endif

	if (unlikely(!fe || !p ||
			!fe->ops.tuner_ops.get_status ||
			!fe->ops.analog_ops.has_signal)) {
		pr_err("[%s] error: NULL function or pointer.\n", __func__);
		return V4L2_SEARCH_INVALID;
	}

	if (p->afc_range == 0) {
		pr_err("[%s]:afc_range == 0, skip the search\n", __func__);
		return V4L2_SEARCH_INVALID;
	}

	pr_err("[%s] afc_range: [%d], tuner type: [%d], freq: [%d].\n",
			__func__, p->afc_range,
			v4l2_fe->v4l2_ad->tuner_id, p->frequency);

	/* backup the freq by api */
	set_freq = p->frequency;

	/* Before enter search, need set the std first.
	 * If set p->analog.std == 0, will search all std (PAL/NTSC/SECAM),
	 * and need tvafe identify signal type.
	 */
	if (p->std == 0) {
		p->std = V4L2_STD_PAL_I;
		auto_search_std = true;
		pr_dbg("%s, user analog.std is 0, so set it to PAL | I.\n",
				__func__);
	}

	/*afc tune disable*/
	//TODO: afc_timer_disable();
	//analog_search_flag = 1;

	/*set the afc_range and start freq*/
	minafcfreq = p->frequency - p->afc_range;
	maxafcfreq = p->frequency + p->afc_range;

	/*from the current freq start, and set the afc_step*/
	/*if step is 2Mhz,r840 will miss program*/
	if (slow_mode || (v4l2_fe->v4l2_ad->tuner_id == AM_TUNER_R840)
			|| (p->afc_range == ATV_AFC_1_0MHZ)) {
		pr_dbg("[%s] slow mode to search the channel\n", __func__);
		afc_step = ATV_AFC_1_0MHZ;
	} else if (!slow_mode) {
		afc_step = ATV_AFC_2_0MHZ;
	} else {
		pr_dbg("[%s] slow mode to search the channel\n", __func__);
		afc_step = ATV_AFC_1_0MHZ;
	}

	/**enter auto search mode**/
	pr_dbg("%s Auto search user std: 0x%08x\n",
			__func__, (unsigned int) p->std);

#ifdef DEBUG_TIME_CUS
	time_end = jiffies_to_msecs(jiffies);
	time_delta = time_end - time_start;
	pr_dbg("[%s]: time_delta_001:%d ms,afc_step:%d\n",
			__func__, time_delta, afc_step);
#endif

	while (minafcfreq <= p->frequency &&
			p->frequency <= maxafcfreq) {

		if (fe->ops.analog_ops.set_params) {
			params.frequency = p->frequency;
			params.mode = p->flag;
			params.audmode = p->audmode;
			params.std = p->std;
			fe->ops.analog_ops.set_params(fe, &params);
		}

		pr_dbg("[%s] [%d] is processing, [min=%d, max=%d].\n",
				__func__, p->frequency, minafcfreq, maxafcfreq);

		pll_lock = false;
		tuner_status_cnt_local = tuner_status_cnt;
		do {
			if (v4l2_fe->v4l2_ad->tuner_id == AM_TUNER_MXL661) {
				usleep_range(30 * 1000, 30 * 1000 + 100);
			} else if (v4l2_fe->v4l2_ad->tuner_id
					== AM_TUNER_R840) {
				usleep_range(20 * 1000, 20 * 1000 + 100);
				fe->ops.tuner_ops.get_status(fe,
						&tuner_state);
			} else {
				/*fee->tuner->drv->id == AM_TUNER_SI2151)*/
				usleep_range(10 * 1000, 10 * 1000 + 100);
			}

			fe->ops.analog_ops.has_signal(fe, (u16 *)&ade_state);
			tuner_status_cnt_local--;
			if (((ade_state == V4L2_HAS_LOCK ||
				tuner_state == V4L2_HAS_LOCK) &&
				(v4l2_fe->v4l2_ad->tuner_id
						!= AM_TUNER_R840)) ||
				((ade_state == V4L2_HAS_LOCK &&
				tuner_state == V4L2_HAS_LOCK) &&
				(v4l2_fe->v4l2_ad->tuner_id
						== AM_TUNER_R840))) {
				pll_lock = true;
				break;
			}

			if (tuner_status_cnt_local == 0)
				break;
		} while (1);

		if (pll_lock) {

			pr_dbg("[%s] freq: [%d] pll lock success\n",
					__func__, p->frequency);
#if 0 /* In get_pll_status has line_lock check.*/
			if (fee->tuner->drv->id == AM_TUNER_MXL661) {
				fe->ops.analog_ops.get_atv_status(fe,
						&atv_status);
				if (atv_status.atv_lock)
					usleep_range(30 * 1000,
						30 * 1000 + 100);
			}
#endif
			ret = v4l2_fe_afc_closer(v4l2_fe, minafcfreq,
					maxafcfreq + ATV_AFC_500KHZ, 1);
			if (ret == 0) {
				v4l2_fe_try_analog_format(v4l2_fe,
						auto_search_std,
						&std_bk, &audio);

				pr_dbg("[%s] freq:%d, std_bk:0x%x, audmode:0x%x, search OK.\n",
						__func__, p->frequency,
						(unsigned int) std_bk, audio);

				if (std_bk != 0) {
					p->audmode = audio;
					p->std = std_bk;
					/*avoid std unenable */
					p->frequency -= 1;
					std_bk = 0;
				}
#ifdef DEBUG_TIME_CUS
				time_end = jiffies_to_msecs(jiffies);
				time_delta = time_end - time_start;
				pr_dbg("[ATV_SEARCH_SUCCESS]%s: time_delta:%d ms\n",
						__func__, time_delta);
#endif
				/*sync param */
				//aml_fe_analog_sync_frontend(fe);
				/*afc tune enable*/
				//analog_search_flag = 0;
				//afc_timer_enable(fe);
				return V4L2_SEARCH_SUCCESS;
			}
		}

		/*avoid sound format is not match after search over */
		if (std_bk != 0) {
			p->std = std_bk;

			params.frequency = p->frequency;
			params.mode = p->flag;
			params.audmode = p->audmode;
			params.std = p->std;

			fe->ops.analog_ops.set_params(fe, &params);
			std_bk = 0;
		}

		pr_dbg("[%s] freq[analog.std:0x%08x] is[%d] unlock\n",
				__func__,
				(uint32_t) p->std, p->frequency);

		++search_count;
		if (p->frequency >= 44200000 &&
			p->frequency <= 44300000 &&
			double_check_cnt) {
			double_check_cnt--;
			p->frequency -= afc_step;
			pr_err("%s 44.25Mhz double check\n", __func__);
		} else
			p->frequency += afc_step * ((search_count % 2) ?
					-search_count : search_count);

#ifdef DEBUG_TIME_CUS
		time_end = jiffies_to_msecs(jiffies);
		time_delta = time_end - time_start;
		pr_dbg("[ATV_SEARCH_FAILED]%s: time_delta:%d ms\n",
				__func__, time_delta);
#endif
	}

#ifdef DEBUG_TIME_CUS
	time_end = jiffies_to_msecs(jiffies);
	time_delta = time_end - time_start;
	pr_dbg("[ATV_SEARCH_FAILED]%s: time_delta:%d ms\n",
			__func__, time_delta);
#endif

	pr_dbg("[%s] [%d] over of range [min=%d, max=%d], search failed.\n",
			__func__, p->frequency, minafcfreq, maxafcfreq);
	p->frequency = set_freq;
	/*afc tune enable*/
	//analog_search_flag = 0;
	//afc_timer_enable(fe);
	return DVBFE_ALGO_SEARCH_FAILED;
}


static int v4l2_frontend_get_event(struct v4l2_frontend *v4l2_fe,
		struct v4l2_frontend_event *event, int flags)
{
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;
	struct v4l2_fe_events *events = &fepriv->events;

	pr_err("%s.\n", __func__);

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

	pr_err("%s.\n", __func__);

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

	pr_err("%s: thread start.\n", __func__);

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
			pr_err("%s: Retune requested, V4L2FE_STATE_RETUNE.\n",
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
			if (v4l2_fe->search) {
				fepriv->algo_status = v4l2_fe->search(v4l2_fe);
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

	pr_err("%s: thread exit state = %d.\n", __func__, fepriv->state);

	return 0;
}

static void v4l2_frontend_stop(struct v4l2_frontend *v4l2_fe)
{
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	pr_err("%s.\n", __func__);

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
		pr_err("%s: warning: thread %p won't exit\n",
				__func__, fepriv->thread);
}

static int v4l2_frontend_start(struct v4l2_frontend *v4l2_fe)
{
	int ret = 0;
	struct task_struct *thread = NULL;
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	pr_err("%s.\n", __func__);

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
	struct dvb_frontend *fe = &v4l2_fe->v4l2_ad->fe;

	pr_err("%s.\n", __func__);

	freq_min = fe->ops.tuner_ops.info.frequency_min;
	freq_max = fe->ops.tuner_ops.info.frequency_max;

	if (freq_min == 0 || freq_max == 0)
		pr_err("%s: demod or tuner frequency limits undefined.\n",
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
	if (params->flag) {
		/*dvb_frontend_add_event(fe, 0); */
		v4l2_frontend_clear_events(v4l2_fe);
		v4l2_frontend_wakeup(v4l2_fe);
	} else if (fe->ops.analog_ops.set_params) {
		/* TODO:*/
		p.frequency = params->frequency;
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

	return 0;
}

static int v4l2_frontend_set_mode(struct v4l2_frontend *v4l2_fe,
		unsigned long params)
{
	int ret = 0;
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;
	struct analog_demod_ops *analog_ops = NULL;
	int priv_cfg = 0;

	pr_err("%s: params = %ld.\n", __func__, params);

	fepriv->state = V4L2FE_STATE_IDLE;

	analog_ops = &v4l2_fe->v4l2_ad->fe.ops.analog_ops;

	if (params)
		priv_cfg = AML_ATVDEMOD_INIT;
	else
		priv_cfg = AML_ATVDEMOD_UNINIT;

	if (analog_ops && analog_ops->set_config)
		ret = analog_ops->set_config(&v4l2_fe->v4l2_ad->fe, &priv_cfg);

	return ret;
}

static void v4l2_frontend_vdev_release(struct video_device *dev)
{
	pr_err("%s.\n", __func__);
}

static ssize_t v4l2_frontend_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	pr_err("%s.\n", __func__);
	return 0;
}

static ssize_t v4l2_frontend_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	pr_err("%s.\n", __func__);
	return 0;
}

static unsigned int v4l2_frontend_poll(struct file *filp,
		struct poll_table_struct *pts)
{
	struct v4l2_frontend *v4l2_fe = video_get_drvdata(video_devdata(filp));
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	poll_wait(filp, &fepriv->events.wait_queue, pts);

	if (fepriv->events.eventw != fepriv->events.eventr)
		return (POLLIN | POLLRDNORM | POLLPRI);

	pr_err("%s.\n", __func__);
	return 0;
}

static long v4l2_frontend_ioctl(struct file *filp, void *fh, bool valid_prio,
		unsigned int cmd, void *arg)
{
	int ret = 0;
	struct v4l2_frontend *v4l2_fe = video_get_drvdata(video_devdata(filp));
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	pr_err("%s: cmd = 0x%x.\n", __func__, cmd);
	if (fepriv->exit != V4L2_FE_NO_EXIT)
		return -ENODEV;

	if (down_interruptible(&fepriv->sem))
		return -ERESTARTSYS;

	switch (cmd) {
	case V4L2_SET_FRONTEND:
		ret = v4l2_set_frontend(v4l2_fe,
				(struct v4l2_analog_parameters *) arg);
		break;

	case V4L2_GET_FRONTEND:
		ret = v4l2_get_frontend(v4l2_fe,
				(struct v4l2_analog_parameters *) arg);
		break;

	case V4L2_GET_EVENT:
		ret = v4l2_frontend_get_event(v4l2_fe,
				(struct v4l2_frontend_event *) arg,
				filp->f_flags);
		break;

	case V4L2_SET_MODE:
		ret = v4l2_frontend_set_mode(v4l2_fe, (unsigned long) arg);
		break;

	default:
		break;
	}

	up(&fepriv->sem);

	return ret;
}

static int v4l2_frontend_open(struct file *filp)
{
	int ret = 0;
	void *p = NULL;
	struct v4l2_frontend *v4l2_fe = video_get_drvdata(video_devdata(filp));
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;
	struct dvb_tuner_ops *tuner_ops = &v4l2_fe->v4l2_ad->fe.ops.tuner_ops;

	if (tuner_ops->init == NULL) {
		switch (v4l2_fe->v4l2_ad->tuner_id) {
		case AM_TUNER_R840:
			break;
		case AM_TUNER_SI2151:
			p = v4l2_attach(si2151_attach, &v4l2_fe->v4l2_ad->fe,
					v4l2_fe->v4l2_ad->i2c.adapter,
					v4l2_fe->v4l2_ad->i2c.addr);
			break;
		case AM_TUNER_MXL661:
			p = v4l2_attach(mxl661_attach, &v4l2_fe->v4l2_ad->fe,
					v4l2_fe->v4l2_ad->i2c.adapter,
					v4l2_fe->v4l2_ad->i2c.addr);
			break;
		}

		if (p == NULL)
			pr_err("%s: v4l2_attach error.\n", __func__);
	}

	ret = v4l2_frontend_start(v4l2_fe);

	fepriv->events.eventr = fepriv->events.eventw = 0;

	pr_err("%s.\n", fepriv->v4l2dev->name);
	pr_err("%s.\n", __func__);

	return ret;
}

static int v4l2_frontend_release(struct file *filp)
{
	struct v4l2_frontend *v4l2_fe = video_get_drvdata(video_devdata(filp));
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;

	pr_err("%s.\n", fepriv->v4l2dev->name);
	pr_err("%s.\n", __func__);

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
	pr_err("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_s_audio(struct file *filp, void *fh,
		const struct v4l2_audio *a)
{
	pr_err("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_g_tuner(struct file *filp, void *fh,
				struct v4l2_tuner *a)
{
	pr_err("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_s_tuner(struct file *filp, void *fh,
		const struct v4l2_tuner *a)
{
	pr_err("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_g_frequency(struct file *filp, void *fh,
		struct v4l2_frequency *a)
{
	pr_err("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_s_frequency(struct file *filp, void *fh,
		const struct v4l2_frequency *a)
{
	pr_err("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_enum_freq_bands(struct file *filp, void *fh,
		struct v4l2_frequency_band *band)
{
	pr_err("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_g_modulator(struct file *filp, void *fh,
		struct v4l2_modulator *a)
{
	pr_err("%s.\n", __func__);
	return 0;
}

static int v4l2_frontend_vidioc_s_modulator(struct file *filp, void *fh,
		const struct v4l2_modulator *a)
{
	pr_err("%s.\n", __func__);
	return 0;
}

int v4l2_frontend_vidioc_streamon(struct file *filp, void *fh,
		enum v4l2_buf_type i)
{
	pr_err("%s.\n", __func__);
	return 0;
}

int v4l2_frontend_vidioc_streamoff(struct file *filp, void *fh,
		enum v4l2_buf_type i)
{
	pr_err("%s.\n", __func__);
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

int v4l2_resister_frontend(struct v4l2_adapter *v4l2_ad,
		struct v4l2_frontend *v4l2_fe)
{
	int ret = 0;
	struct v4l2_frontend_private *fepriv = NULL;
	struct v4l2_atvdemod_device *v4l2dev = NULL;

	if (v4l2_ad == NULL || v4l2_fe == NULL) {
		pr_err("NULL pointer.\n");
		return -1;
	}

	if (mutex_lock_interruptible(&v4l2_fe_mutex))
		return -ERESTARTSYS;

	v4l2_fe->frontend_priv = kzalloc(sizeof(struct v4l2_frontend_private),
			GFP_KERNEL);
	if (v4l2_fe->frontend_priv == NULL) {
		mutex_unlock(&v4l2_fe_mutex);
		pr_err("kzalloc fail.\n");
		return -ENOMEM;
	}

	v4l2_fe->search = v4l2_frontend_search;
	v4l2_fe->v4l2_ad = v4l2_ad;
	fepriv = v4l2_fe->frontend_priv;

	fepriv->v4l2dev = kzalloc(sizeof(struct v4l2_atvdemod_device),
			GFP_KERNEL);
	if (fepriv->v4l2dev == NULL) {
		ret = -ENOMEM;
		goto malloc_fail;
	}

	v4l2dev = fepriv->v4l2dev;
	v4l2dev->name = DEVICE_NAME;
	v4l2dev->dev = v4l2_fe->v4l2_ad->dev;

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
			VFL_TYPE_GRABBER, -1);
	if (ret) {
		pr_err("register video device fail.\n");
		goto vdev_fail;
	}

	if (!aml_atvdemod_attach(&v4l2_ad->fe, v4l2_ad->i2c.adapter,
			v4l2_ad->i2c.addr, v4l2_ad->tuner_id)) {
		pr_err("atvdemod attach fail.\n");
		goto attach_fail;
	}

	sema_init(&fepriv->sem, 1);
	init_waitqueue_head(&fepriv->wait_queue);
	init_waitqueue_head(&fepriv->events.wait_queue);
	mutex_init(&fepriv->events.mtx);

	mutex_unlock(&v4l2_fe_mutex);

	pr_err("resister aml atv demod device.\n");

	return 0;

attach_fail:

vdev_fail:
	v4l2_device_unregister(&v4l2dev->v4l2_dev);

malloc_fail:
	kfree(v4l2_fe->frontend_priv);
v4l2_fail:
	mutex_unlock(&v4l2_fe_mutex);

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

	pr_err("unregister aml atv demod device.\n");

	return 0;
}

int v4l2_frontend_suspend(struct v4l2_frontend *v4l2_fe)
{
	int ret = 0;
	struct dvb_frontend *fe = &v4l2_fe->v4l2_ad->fe;
	struct dvb_tuner_ops tuner_ops = fe->ops.tuner_ops;
	struct analog_demod_ops analog_ops = fe->ops.analog_ops;

	pr_info("%s.\n", __func__);

	if (analog_ops.standby)
		analog_ops.standby(fe);

	if (tuner_ops.suspend)
		tuner_ops.suspend(fe);

	return ret;
}

int v4l2_frontend_resume(struct v4l2_frontend *v4l2_fe)
{
	int ret = 0;
	struct v4l2_frontend_private *fepriv = v4l2_fe->frontend_priv;
	struct dvb_frontend *fe = &v4l2_fe->v4l2_ad->fe;
	struct dvb_tuner_ops tuner_ops = fe->ops.tuner_ops;
	struct analog_demod_ops analog_ops = fe->ops.analog_ops;
	int priv_cfg = AML_ATVDEMOD_RESUME;

	pr_info("%s.\n", __func__);

	if (analog_ops.set_config)
		analog_ops.set_config(fe, &priv_cfg);

	if (tuner_ops.resume)
		tuner_ops.resume(fe);

	fepriv->state = V4L2FE_STATE_RETUNE;
	v4l2_frontend_wakeup(v4l2_fe);

	return ret;
}
