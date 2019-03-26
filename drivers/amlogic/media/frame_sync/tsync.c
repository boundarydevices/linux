/*
 * drivers/amlogic/media/frame_sync/tsync.c
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
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#ifdef ARC_700
/* #include <asm/arch/am_regs.h> */
#else
/* #include <mach/am_regs.h> */
#endif
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/frame_sync/tsync_pcr.h>
#include <linux/amlogic/media/registers/register.h>
#include <linux/amlogic/media/codec_mm/configs.h>

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
/* TODO: for stream buffer register bit define only */
/* #endif */

#if !defined(CONFIG_PREEMPT)
#define CONFIG_AM_TIMESYNC_LOG
#endif

#ifdef CONFIG_AM_TIMESYNC_LOG
#define AMLOG
#define LOG_LEVEL_ERROR     0
#define LOG_LEVEL_ATTENTION 1
#define LOG_LEVEL_INFO      2
#define LOG_LEVEL_VAR       amlog_level_tsync
#define LOG_MASK_VAR        amlog_mask_tsync
#endif
#include <linux/amlogic/media/utils/amlog.h>
MODULE_AMLOG(AMLOG_DEFAULT_LEVEL, 0, LOG_DEFAULT_LEVEL_DESC,
			 LOG_DEFAULT_MASK_DESC);

/* #define DEBUG */
#define AVEVENT_FLAG_PARAM  0x01

/* #define TSYNC_SLOW_SYNC */

#define PCR_CHECK_INTERVAL  (HZ * 5)
#define PCR_DETECT_MARGIN_SHIFT_AUDIO_HI     7
#define PCR_DETECT_MARGIN_SHIFT_AUDIO_LO     7
#define PCR_DETECT_MARGIN_SHIFT_VIDEO_HI     4
#define PCR_DETECT_MARGIN_SHIFT_VIDEO_LO     4
#define PCR_MAINTAIN_MARGIN_SHIFT_AUDIO      4
#define PCR_MAINTAIN_MARGIN_SHIFT_VIDEO      1
#define PCR_RECOVER_PCR_ADJ 15

#define TSYNC_INIT_STATE (0X01)
unsigned int tsync_flag;

enum {
	PCR_SYNC_UNSET,
	PCR_SYNC_HI,
	PCR_SYNC_LO,
};

enum {
	PCR_TRIGGER_AUDIO,
	PCR_TRIGGER_VIDEO
};

enum tsync_stat_e {
	TSYNC_STAT_PCRSCR_SETUP_NONE,
	TSYNC_STAT_PCRSCR_SETUP_VIDEO,
	TSYNC_STAT_PCRSCR_SETUP_AUDIO
};

enum {
	/* Fixed: use the Nominal M/N values */
	TOGGLE_MODE_FIXED = 0,
	/* Toggle between the Nominal M/N values and the Low M/N values */
	TOGGLE_MODE_NORMAL_LOW,
	/* Toggle between the Nominal M/N values and the High M/N values */
	TOGGLE_MODE_NORMAL_HIGH,
	/* Toggle between the Low M/N values and the High M/N Values */
	TOGGLE_MODE_LOW_HIGH,
};

static const struct {
	const char *token;
	const u32 token_size;
	const enum avevent_e event;
	const u32 flag;
} avevent_token[] = {
	{
		"VIDEO_START", 11, VIDEO_START, AVEVENT_FLAG_PARAM
	}, {
		"VIDEO_STOP", 10, VIDEO_STOP, 0
	}, {
		"VIDEO_PAUSE", 11, VIDEO_PAUSE, AVEVENT_FLAG_PARAM
	}, {
		"VIDEO_TSTAMP_DISCONTINUITY", 26, VIDEO_TSTAMP_DISCONTINUITY,
		AVEVENT_FLAG_PARAM
	}, {
		"AUDIO_START", 11, AUDIO_START, AVEVENT_FLAG_PARAM
	}, {
		"AUDIO_RESUME", 12, AUDIO_RESUME, 0
	}, {
		"AUDIO_STOP", 10, AUDIO_STOP, 0
	}, {
		"AUDIO_PAUSE", 11, AUDIO_PAUSE, 0
	}, {
		"AUDIO_TSTAMP_DISCONTINUITY", 26, AUDIO_TSTAMP_DISCONTINUITY,
		AVEVENT_FLAG_PARAM
	}, {
		"AUDIO_PRE_START", 15, AUDIO_PRE_START, 0
	}, {
	    "AUDIO_WAIT", 10, AUDIO_WAIT, 0
	}
};

static const char * const tsync_mode_str[] = {
	"vmaster", "amaster", "pcrmaster"
};

static DEFINE_SPINLOCK(lock);
static enum tsync_mode_e tsync_mode = TSYNC_MODE_AMASTER;
static enum tsync_stat_e tsync_stat = TSYNC_STAT_PCRSCR_SETUP_NONE;
static int tsync_enable;	/* 1; */
static int apts_discontinue;
static int vpts_discontinue;
static int pts_discontinue;
static u32 apts_discontinue_diff;
static u32 vpts_discontinue_diff;
static int tsync_abreak;
static bool tsync_pcr_recover_enable;
static int pcr_sync_stat = PCR_SYNC_UNSET;
static int pcr_recover_trigger;
static struct timer_list tsync_pcr_recover_timer;
static int tsync_trickmode;
static int vpause_flag;
static int apause_flag;
static bool dobly_avsync_test;
static int slowsync_enable;
static int apts_lookup_offset;
pfun_tsdemux_pcrscr_valid tsdemux_pcrscr_valid_cb;

pfun_tsdemux_pcrscr_get tsdemux_pcrscr_get_cb;

pfun_tsdemux_first_pcrscr_get tsdemux_first_pcrscr_get_cb;

pfun_tsdemux_pcraudio_valid tsdemux_pcraudio_valid_cb;

pfun_tsdemux_pcrvideo_valid tsdemux_pcrvideo_valid_cb;

pfun_get_buf_by_type get_buf_by_type_cb;

pfun_stbuf_level stbuf_level_cb;

pfun_stbuf_space stbuf_space_cb;

pfun_stbuf_size stbuf_size_cb;


/*
 *used to set player start sync mode, 0-none; 1-smoothsync; 2-droppcm;
 *default drop pcm
 */
static int startsync_mode = 2;

/*
 *		  threshold_min              threshold_max
 *			 |                          |
 *AMASTER<-------------->  |<-----DYNAMIC VMASTER---->|   VMASTER
 *static mode  | a dynamic  |            dynamic mode  |static mode
 *
 *static mode(S), Amster and Vmaster..
 *dynamic mode (D)  : discontinue....>min,< max,can switch to static mode,
 *		if diff is <min,and become to Sd mode if timerout.
 *sdynamic mode (A): dynamic mode become to static mode,because timer out,
 *		Don't do switch  befome timeout.
 *
 *tsync_av_mode switch...
 *(AMASTER S)<-->(D VMASTER)<--> (S VMASTER)
 *(D VMASTER)--time out->(A AMASTER)--time out->((AMASTER S))
 */
unsigned int tsync_av_threshold_min = AV_DISCONTINUE_THREDHOLD_MIN;
unsigned int tsync_av_threshold_max = AV_DISCONTINUE_THREDHOLD_MAX;
#define TSYNC_STATE_S  ('S')
#define TSYNC_STATE_A ('A')
#define TSYNC_STATE_D  ('D')
static unsigned int tsync_av_mode = TSYNC_STATE_S;	/* S=1,A=2,D=3; */
static u64
tsync_av_latest_switch_time_ms;	/* the time on latset switch */
static unsigned int tsync_av_dynamic_duration_ms;/* hold for dynamic mode; */
static u64 tsync_av_dynamic_timeout_ms;/* hold for dynamic mode; */
static struct timer_list tsync_state_switch_timer;
#define jiffies_ms div64_u64(get_jiffies_64() * 1000, HZ)
#define jiffies_ms32 (jiffies * 1000/HZ)

static unsigned int tsync_syncthresh = 1;
static int tsync_dec_reset_flag;
static int tsync_dec_reset_video_start;
static int tsync_automute_on;
static int tsync_video_started;

static int debug_pts_checkin;
static int debug_pts_checkout;
static int debug_vpts;
static int debug_apts;

#define M_HIGH_DIFF    2
#define M_LOW_DIFF     2
#define PLL_FACTOR   10000

#define LOW_TOGGLE_TIME           99
#define NORMAL_TOGGLE_TIME        499
#define HIGH_TOGGLE_TIME          99

#define PTS_CACHED_LO_NORMAL_TIME (90000)
#define PTS_CACHED_NORMAL_LO_TIME (45000)
#define PTS_CACHED_HI_NORMAL_TIME (135000)
#define PTS_CACHED_NORMAL_HI_TIME (180000)

int register_tsync_callbackfunc(enum tysnc_func_type_e ntype, void *pfunc)
{
	if (ntype >= TSYNC_FUNC_TYPE_MAX) {
		pr_info("register_tync_func ntype is err.\n");
		return -1;
	}
	switch (ntype) {
	case TSYNC_PCRSCR_VALID:
		tsdemux_pcrscr_valid_cb =
				(pfun_tsdemux_pcrscr_valid)pfunc;
		break;
	case TSYNC_PCRSCR_GET:
		tsdemux_pcrscr_get_cb =
				(pfun_tsdemux_pcrscr_get)pfunc;
		break;
	case TSYNC_FIRST_PCRSCR_GET:
		tsdemux_first_pcrscr_get_cb =
				(pfun_tsdemux_first_pcrscr_get)pfunc;
		break;
	case TSYNC_PCRAUDIO_VALID:
		tsdemux_pcraudio_valid_cb  =
				(pfun_tsdemux_pcraudio_valid)pfunc;
		break;
	case TSYNC_PCRVIDEO_VALID:
		tsdemux_pcrvideo_valid_cb =
				(pfun_tsdemux_pcrvideo_valid)pfunc;
		break;
	case TSYNC_BUF_BY_BYTE:
		get_buf_by_type_cb =
				(pfun_get_buf_by_type)pfunc;
		break;
	case TSYNC_STBUF_LEVEL:
		stbuf_level_cb =
				(pfun_stbuf_level)pfunc;
		break;
	case TSYNC_STBUF_SPACE:
		stbuf_space_cb =
				(pfun_stbuf_space)pfunc;
		break;
	case TSYNC_STBUF_SIZE:
		stbuf_size_cb =
				(pfun_stbuf_size)pfunc;
		break;
	default:
		break;
	}
	return 0;
}
EXPORT_SYMBOL(register_tsync_callbackfunc);



static void tsync_pcr_recover_with_audio(void)
{
#if 0				/* MESON_CPU_TYPE < MESON_CPU_TYPE_MESON6 */

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M6) {
		u32 ab_level = READ_AIU_REG(AIU_MEM_AIFIFO_LEVEL);
		u32 ab_size = READ_AIU_REG(AIU_MEM_AIFIFO_END_PTR)
			- READ_AIU_REG(AIU_MEM_AIFIFO_START_PTR) + 8;
		u32 vb_level = READ_VREG(VLD_MEM_VIFIFO_LEVEL);
		u32 vb_size = READ_VREG(VLD_MEM_VIFIFO_END_PTR)
			- READ_VREG(VLD_MEM_VIFIFO_START_PTR) + 8;

		bool lower_than_low = false;
		bool higher_than_high = false;
		bool lower_than_high = false;
		bool higher_than_low = false;

		if ((READ_AIU_REG(AIU_MEM_I2S_CONTROL) &
			 (MEM_CTRL_EMPTY_EN | MEM_CTRL_EMPTY_EN)) == 0)
			return;
		/*
		 *pr_info("ab_size:%d ab_level:%d vb_size:%d vb_level:%d\n",
		 *ab_size, ab_level, vb_size, vb_level);
		 */

		/*
		 *pr_info("vpts diff %d apts diff %d vlevel %d alevel %d\n",
		 *  pts_cached_time(PTS_TYPE_VIDEO),
		 * pts_cached_time(PTS_TYPE_AUDIO),
		 *vb_level, ab_level);
		 */

#ifndef CALC_CACHED_TIME
		lower_than_low = (ab_level <
			(ab_size >> PCR_DETECT_MARGIN_SHIFT_AUDIO_LO))
			 || (vb_level <
				 (vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_LO));
		higher_than_high = (((ab_level + (ab_size >>
				PCR_DETECT_MARGIN_SHIFT_AUDIO_HI)) >
			   ab_size) || ((vb_level + (vb_size >>
				   PCR_DETECT_MARGIN_SHIFT_VIDEO_HI)) >
				   vb_size));
		higher_than_low = ((!(pcr_recover_trigger &
					(1 << PCR_TRIGGER_AUDIO)))
					|| (ab_level > (ab_size >>
					PCR_MAINTAIN_MARGIN_SHIFT_AUDIO)))
					&& ((!(pcr_recover_trigger &
					(1 << PCR_TRIGGER_VIDEO)))
					||  ((vb_level + (vb_size >>
					PCR_MAINTAIN_MARGIN_SHIFT_VIDEO)) >
						vb_size));
		lower_than_high = ((!(pcr_recover_trigger &
					(1 << PCR_TRIGGER_AUDIO)))
					|| ((ab_level + (ab_size >>
					PCR_MAINTAIN_MARGIN_SHIFT_AUDIO))
					< ab_size)) &&
					((!(pcr_recover_trigger &
					(1 << PCR_TRIGGER_VIDEO)))
					|| (vb_level < (vb_size >>
					 PCR_MAINTAIN_MARGIN_SHIFT_VIDEO)));


#else
		lower_than_low = (pts_cached_time(PTS_TYPE_VIDEO) <
				PTS_CACHED_NORMAL_LO_TIME)
				&& (pts_cached_time(PTS_TYPE_AUDIO) <
				PTS_CACHED_NORMAL_LO_TIME);
		higher_than_high = ((pts_cached_time(PTS_TYPE_VIDEO) >=
				PTS_CACHED_NORMAL_HI_TIME)
				&& (pts_cached_time(PTS_TYPE_AUDIO) >=
				PTS_CACHED_NORMAL_HI_TIME));
		higher_than_low =  ((pts_cached_time(PTS_TYPE_VIDEO) >=
				PTS_CACHED_LO_NORMAL_TIME)
				|| (pts_cached_time(PTS_TYPE_AUDIO) >=
				PTS_CACHED_LO_NORMAL_TIME));
		lower_than_high = ((pts_cached_time(PTS_TYPE_VIDEO) <
				PTS_CACHED_HI_NORMAL_TIME)
				|| (pts_cached_time(PTS_TYPE_AUDIO) <
				PTS_CACHED_HI_NORMAL_TIME));


#endif
		if ((unlikely(pcr_sync_stat != PCR_SYNC_LO))
				&& lower_than_low) {
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_CNTL0,
					READ_HHI_REG(HHI_AUD_PLL_MOD_CNTL0) &
					(~
					 ((1 << 31) |
					 (TOGGLE_MODE_LOW_HIGH << 28))));
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_CNTL0,
					READ_HHI_REG(HHI_AUD_PLL_MOD_CNTL0) |
					(TOGGLE_MODE_NORMAL_LOW << 28));
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_CNTL0,
					READ_HHI_REG(HHI_AUD_PLL_MOD_CNTL0) |
					(1 << 31));

#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
			{
				u32 inc, M_nom, N_nom;

				M_nom = READ_HHI_REG(HHI_AUD_PLL_CNTL) & 0x1ff;
				N_nom =
					(READ_HHI_REG(HHI_AUD_PLL_CNTL) >> 9) &
					0x1f;

				inc =
					(M_nom * (NORMAL_TOGGLE_TIME + 1) +
					 (M_nom - M_LOW_DIFF) *
					 (LOW_TOGGLE_TIME +
					  1)) * PLL_FACTOR /
					((NORMAL_TOGGLE_TIME + LOW_TOGGLE_TIME +
					  2) * M_nom);
				set_timestamp_inc_factor(inc);
				pr_info("pll low inc: %d factor: %d\n", inc,
						PLL_FACTOR);
			}
#endif

			pcr_sync_stat = PCR_SYNC_LO;
			pr_info("pcr_sync_stat = PCR_SYNC_LO\n");
			if (ab_level <
				(ab_size >> PCR_DETECT_MARGIN_SHIFT_AUDIO_LO)) {
				pcr_recover_trigger |= (1 << PCR_TRIGGER_AUDIO);
				int a_shift = PCR_DETECT_MARGIN_SHIFT_AUDIO_LO;

				pr_info("audio: 0x%x < 0x%x, vb_level 0x%x\n",
						ab_level,
						(ab_size >> a_shift),
						vb_level);
			}
			if (vb_level <
				(vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_LO)) {
				pcr_recover_trigger |= (1 << PCR_TRIGGER_VIDEO);
				int v_shift = PCR_DETECT_MARGIN_SHIFT_VIDEO_LO;

				pr_info("video: 0x%x < 0x%x, ab_level 0x%x\n",
						vb_level,
						(vb_size >> v_shift),
						ab_level);
			}
		} else if ((unlikely(pcr_sync_stat != PCR_SYNC_HI))
				&& higher_than_high) {
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_CNTL0,
					READ_HHI_REG(HHI_AUD_PLL_MOD_CNTL0) &
					(~
					 ((1 << 31) |
					  (TOGGLE_MODE_LOW_HIGH << 28))));
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_CNTL0,
					READ_HHI_REG(HHI_AUD_PLL_MOD_CNTL0) |
					(TOGGLE_MODE_NORMAL_HIGH << 28));
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_CNTL0,
					READ_HHI_REG(HHI_AUD_PLL_MOD_CNTL0) |
					(1 << 31));
#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
			{
				u32 inc, M_nom, N_nom;

				M_nom = READ_HHI_REG(HHI_AUD_PLL_CNTL) & 0x1ff;
				N_nom =
					(READ_HHI_REG(HHI_AUD_PLL_CNTL) >> 9) &
					0x1f;

				inc =
					(M_nom * (NORMAL_TOGGLE_TIME + 1) +
					 (M_nom + M_HIGH_DIFF) *
					 (HIGH_TOGGLE_TIME +
					  1)) * PLL_FACTOR /
					((NORMAL_TOGGLE_TIME +
					  HIGH_TOGGLE_TIME +
					  2) * M_nom);
				set_timestamp_inc_factor(inc);
				pr_info("pll high inc: %d factor: %d\n", inc,
						PLL_FACTOR);
			}
#endif
			pcr_sync_stat = PCR_SYNC_HI;
			pr_info("pcr_sync_stat = PCR_SYNC_HI\n");
			if ((ab_level +
				 (ab_size >>
				  PCR_DETECT_MARGIN_SHIFT_AUDIO_HI)) >
				ab_size) {
				pcr_recover_trigger |= (1 << PCR_TRIGGER_AUDIO);
				pr_info
				("audio: 0x%x+0x%x > 0x%x, vb_level 0x%x\n",
				 ab_level,
				 (ab_size >>
				  PCR_DETECT_MARGIN_SHIFT_AUDIO_HI),
				 ab_size, vb_level);
			}
			if ((vb_level +
				 (vb_size >>
				  PCR_DETECT_MARGIN_SHIFT_VIDEO_HI)) >
				vb_size) {
				pcr_recover_trigger |= (1 << PCR_TRIGGER_VIDEO);
				pr_info
				("video: 0x%x+0x%x > 0x%x, ab_level 0x%x\n",
				 vb_level,
				 (vb_size >>
				  PCR_DETECT_MARGIN_SHIFT_VIDEO_HI),
				 vb_size, ab_level);
			}
		} else if ((pcr_sync_stat == PCR_SYNC_LO)
					&& higher_than_low || lower_than_high) {
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_CNTL0,
					READ_HHI_REG(HHI_AUD_PLL_MOD_CNTL0) &
					(~
					 ((1 << 31) |
					  (TOGGLE_MODE_LOW_HIGH << 28))));
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_CNTL0,
					READ_HHI_REG(HHI_AUD_PLL_MOD_CNTL0) |
						   (TOGGLE_MODE_FIXED << 28));
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_CNTL0,
					READ_HHI_REG(HHI_AUD_PLL_MOD_CNTL0) |
					(1 << 31));

#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
			{
				set_timestamp_inc_factor(PLL_FACTOR);
				pr_info("pll normal inc:%d\n", PLL_FACTOR);
			}
#endif

			pcr_sync_stat = PCR_SYNC_UNSET;
			pcr_recover_trigger = 0;
		}
	}
#endif
}

static void tsync_pcr_recover_with_video(void)
{
	u32 vb_level = READ_VREG(VLD_MEM_VIFIFO_LEVEL);
	u32 vb_size = READ_VREG(VLD_MEM_VIFIFO_END_PTR)
				  - READ_VREG(VLD_MEM_VIFIFO_START_PTR) + 8;

	if (vb_level < (vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_LO)) {
		timestamp_pcrscr_set_adj(-PCR_RECOVER_PCR_ADJ);
		pr_info(" timestamp_pcrscr_set_adj(-%d);\n",
				PCR_RECOVER_PCR_ADJ);
	} else if ((vb_level + (vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_HI)) >
			   vb_size) {
		timestamp_pcrscr_set_adj(PCR_RECOVER_PCR_ADJ);
		pr_info(" timestamp_pcrscr_set_adj(%d);\n",
				PCR_RECOVER_PCR_ADJ);
	} else if ((vb_level > (vb_size >> PCR_MAINTAIN_MARGIN_SHIFT_VIDEO))
			|| (vb_level <
				(vb_size -
				 (vb_size >> PCR_MAINTAIN_MARGIN_SHIFT_VIDEO))))
		timestamp_pcrscr_set_adj(0);
}

static bool tsync_pcr_recover_use_video(void)
{
	/*
	 *This is just a hacking to use audio output enable
	 *as the flag to check if this is a video only playback.
	 *Such processing can not handle an audio output with a
	 *mixer so audio playback has no direct relationship with
	 *applications. TODO.
	 */
	return ((READ_AIU_REG(AIU_MEM_I2S_CONTROL) &
			 (MEM_CTRL_EMPTY_EN | MEM_CTRL_EMPTY_EN)) == 0) ?
		true : false;
}

static void tsync_pcr_recover_timer_real(void)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (tsync_pcr_recover_enable) {
		if (tsync_pcr_recover_use_video())
			tsync_pcr_recover_with_video();
		else {
			timestamp_pcrscr_set_adj(0);
			tsync_pcr_recover_with_audio();
		}
	}

	spin_unlock_irqrestore(&lock, flags);
}

static void tsync_pcr_recover_timer_func(unsigned long arg)
{
	tsync_pcr_recover_timer_real();
	tsync_pcr_recover_timer.expires = jiffies + PCR_CHECK_INTERVAL;
	add_timer(&tsync_pcr_recover_timer);
}

/*
 *mode:
 *mode='V': diff_pts=|vpts-pcrscr|,jump_pts=vpts jump, video discontinue
 *mode='A': diff_pts=|apts-pcrscr|,jump_pts=apts jump, audio discontinue
 *mode='T': diff_pts=|vpts-apts|,timeout mode switch,
 */

static int tsync_mode_switch(int mode, unsigned long diff_pts, int jump_pts)
{
	int debugcnt = 0;
	int old_tsync_mode = tsync_mode;
	int old_tsync_av_mode = tsync_av_mode;
	char VA[] = "VA--";
	unsigned long oldtimeout = tsync_av_dynamic_timeout_ms;

	if (tsync_mode == TSYNC_MODE_PCRMASTER) {
		pr_info
		("[tsync_mode_switch]tsync_mode is pcr master, do nothing\n");
		return 0;
	}

	pr_info
	("%c-discontinue,pcr=%d,vpts=%d,apts=%d,diff_pts=%lu,jump_Pts=%d\n",
	 mode, timestamp_pcrscr_get(), timestamp_vpts_get(),
	 timestamp_apts_get(), diff_pts, jump_pts);
	if (!tsync_enable) {
		if (tsync_mode != TSYNC_MODE_VMASTER)
			tsync_mode = TSYNC_MODE_VMASTER;
		tsync_av_mode = TSYNC_STATE_S;
		tsync_av_dynamic_duration_ms = 0;
		pr_info("tsync_enable [%d]\n", tsync_enable);
		return 0;
	}
	if (mode == 'T') {	/*D/A--> ... */
		if (tsync_av_mode == TSYNC_STATE_D) {
			debugcnt |= 1 << 1;
			tsync_av_mode = TSYNC_STATE_A;
			tsync_mode = TSYNC_MODE_AMASTER;
			tsync_av_latest_switch_time_ms = jiffies_ms;
			tsync_av_dynamic_duration_ms =
				200 + diff_pts * 1000 / TIME_UNIT90K;
			if (tsync_av_dynamic_duration_ms > 5 * 1000)
				tsync_av_dynamic_duration_ms = 5 * 1000;
		} else if (tsync_av_mode == TSYNC_STATE_A) {
			debugcnt |= 1 << 2;
			tsync_av_mode = TSYNC_STATE_S;
			tsync_mode = TSYNC_MODE_AMASTER;
			tsync_av_latest_switch_time_ms = jiffies_ms;
			tsync_av_dynamic_duration_ms = 0;
		} else {
			/* / */
		}
		if (tsync_mode != old_tsync_mode
			|| tsync_av_mode != old_tsync_av_mode)
			pr_info("mode changes:tsync_mode:%c->%c,state:%c->%c,",
				 VA[old_tsync_mode], VA[tsync_mode],
				 old_tsync_av_mode, tsync_av_mode);
			pr_info("debugcnt=0x%x,diff_pts=%lu\n",
				debugcnt,
				diff_pts);
		return 0;
	}

	if (diff_pts < tsync_av_threshold_min) {
		/*->min*/
		debugcnt |= 1 << 1;
		tsync_av_mode = TSYNC_STATE_S;
		tsync_mode = TSYNC_MODE_AMASTER;
		tsync_av_latest_switch_time_ms = jiffies_ms;
		tsync_av_dynamic_duration_ms = 0;
	} else if (diff_pts <= tsync_av_threshold_max) {	/*min<-->max */
		if (tsync_av_mode == TSYNC_STATE_S) {
			debugcnt |= 1 << 2;
			tsync_av_mode = TSYNC_STATE_D;
			tsync_mode = TSYNC_MODE_VMASTER;
			tsync_av_latest_switch_time_ms = jiffies_ms;
			tsync_av_dynamic_duration_ms = 20 * 1000;
		} else if (tsync_av_mode ==
				   TSYNC_STATE_D) {
			/*new discontinue,continue wait.... */
			debugcnt |= 1 << 7;
			tsync_av_mode = TSYNC_STATE_D;
			tsync_mode = TSYNC_MODE_VMASTER;
			tsync_av_latest_switch_time_ms = jiffies_ms;
			tsync_av_dynamic_duration_ms = 20 * 1000;
		}
	} else if (diff_pts >= tsync_av_threshold_max) {	/*max<-- */
		if (tsync_av_mode == TSYNC_STATE_D) {
			debugcnt |= 1 << 3;
			tsync_av_mode = TSYNC_STATE_S;
			tsync_mode = TSYNC_MODE_VMASTER;
			tsync_av_latest_switch_time_ms = jiffies_ms;
			tsync_av_dynamic_duration_ms = 0;
		} else if (tsync_mode != TSYNC_MODE_VMASTER) {
			debugcnt |= 1 << 4;
			tsync_av_mode = TSYNC_STATE_S;
			tsync_mode = TSYNC_MODE_VMASTER;
			tsync_av_latest_switch_time_ms = jiffies_ms;
			tsync_av_dynamic_duration_ms = 0;
		}
	} else
		debugcnt |= 1 << 16;

	if (oldtimeout != tsync_av_latest_switch_time_ms +
		tsync_av_dynamic_duration_ms) {
		/*duration changed,update new timeout. */
		tsync_av_dynamic_timeout_ms =
			tsync_av_latest_switch_time_ms +
			tsync_av_dynamic_duration_ms;
	}
	pr_info("discontinue-tsync_mode:%c->%c,state:%c->%c,jiffies=%x",
			VA[old_tsync_mode], VA[tsync_mode],
			old_tsync_av_mode, tsync_av_mode,
			(u32)jiffies_ms32);
	pr_info("debugcnt=0x%x,diff_pts=%lu,tsync_mode=%d\n",
			debugcnt, diff_pts, tsync_mode);
	return 0;
}

static void tsync_state_switch_timer_fun(unsigned long arg)
{
	if (!vpause_flag && !apause_flag) {
		if (tsync_av_mode == TSYNC_STATE_D
			|| tsync_av_mode == TSYNC_STATE_A) {
			if (tsync_av_dynamic_timeout_ms < jiffies_ms) {
				/* to be amaster? */
				tsync_mode_switch('T',
						abs(timestamp_vpts_get() -
							timestamp_apts_get()),
						0);
				if (tsync_mode == TSYNC_MODE_AMASTER
					&& abs(timestamp_apts_get() -
						   timestamp_pcrscr_get()) >
					(TIME_UNIT90K * 50 / 1000)) {
					timestamp_pcrscr_set(
						timestamp_apts_get());
				}
			}
		}
	} else {
		/* video&audio paused,changed the timeout time to latter. */
		tsync_av_dynamic_timeout_ms =
			jiffies_ms + tsync_av_dynamic_duration_ms;
	}
	tsync_state_switch_timer.expires = jiffies + msecs_to_jiffies(200);
	add_timer(&tsync_state_switch_timer);
}

void tsync_mode_reinit(void)
{
	tsync_av_mode = TSYNC_STATE_S;
	tsync_av_dynamic_duration_ms = 0;
}
EXPORT_SYMBOL(tsync_mode_reinit);
void tsync_avevent_locked(enum avevent_e event, u32 param)
{
	u32 t;

	if (tsync_mode == TSYNC_MODE_PCRMASTER) {
		amlog_level(LOG_LEVEL_INFO,
				"[tsync_avevent_locked]");
		amlog_level(LOG_LEVEL_INFO,
				"PCR MASTER to use tsync pcr cmd deal ");
		tsync_pcr_avevent_locked(event, param);
		return;
	}

	switch (event) {
	case VIDEO_START:
		tsync_video_started = 1;

	//set tsync mode to vmaster to avoid video block caused
	// by avpts-diff too much
	//threshold 120s is an arbitrary value

		if (tsync_enable && !get_vsync_pts_inc_mode())
			tsync_mode = TSYNC_MODE_AMASTER;
		else{
			tsync_mode = TSYNC_MODE_VMASTER;
			if (get_vsync_pts_inc_mode())
				tsync_stat = TSYNC_STAT_PCRSCR_SETUP_NONE;

		}

		if (tsync_dec_reset_flag)
			tsync_dec_reset_video_start = 1;

		if (slowsync_enable == 1) {	/* slow sync enable */
			timestamp_pcrscr_set(param);
			set_pts_realign();
			tsync_stat = TSYNC_STAT_PCRSCR_SETUP_VIDEO;
		} else {
			if (tsync_stat == TSYNC_STAT_PCRSCR_SETUP_NONE) {
				if (tsync_syncthresh
					&& (tsync_mode == TSYNC_MODE_AMASTER)) {
					timestamp_pcrscr_set(param -
							VIDEO_HOLD_THRESHOLD);
				} else
				timestamp_pcrscr_set(param);
				set_pts_realign();
				tsync_stat = TSYNC_STAT_PCRSCR_SETUP_VIDEO;
			}
		}
		amlog_level(LOG_LEVEL_INFO,
				"vpts to scr, apts = 0x%x, vpts = 0x%x\n",
				timestamp_apts_get(), timestamp_vpts_get());

		if (tsync_stat == TSYNC_STAT_PCRSCR_SETUP_AUDIO) {
			t = timestamp_pcrscr_get();
			if (abs(param - t) > tsync_av_threshold_max) {
				/* if this happens, then play */
				tsync_stat = TSYNC_STAT_PCRSCR_SETUP_VIDEO;
				tsync_mode = TSYNC_MODE_VMASTER;
				tsync_enable = 0;
				timestamp_pcrscr_set(param);
				set_pts_realign();
			}
		}
		if (/*tsync_mode == TSYNC_MODE_VMASTER && */ !vpause_flag)
			timestamp_pcrscr_enable(1);

		if (!timestamp_firstvpts_get() && param)
			timestamp_firstvpts_set(param);
		break;

	case VIDEO_STOP:
		tsync_stat = TSYNC_STAT_PCRSCR_SETUP_NONE;
		timestamp_vpts_set(0);
		timestamp_pcrscr_enable(0);
		timestamp_firstvpts_set(0);
		tsync_video_started = 0;
		break;

	/*
	 *Note:
	 *Video and audio PTS discontinue happens typically with a loopback
	 *playback, with same bit stream play in loop and PTS wrap back from
	 *start point.
	 *When VIDEO_TSTAMP_DISCONTINUITY happens early, PCRSCR is set
	 *immedately to make video still keep running in VMATSER mode. This
	 *mode is restored to AMASTER mode when AUDIO_TSTAMP_DISCONTINUITY
	 *reports, or apts is close to scr later.
	 *When AUDIO_TSTAMP_DISCONTINUITY happens early, VMASTER mode is
	 *set to make video still keep running w/o setting PCRSCR. This mode
	 *is restored to AMASTER mode when VIDEO_TSTAMP_DISCONTINUITY
	 *reports, and scr is restored along with new video time stamp also.
	 */
	case VIDEO_TSTAMP_DISCONTINUITY: {
		unsigned int oldpts = timestamp_vpts_get();
		int oldmod = tsync_mode;
		if (tsync_mode == TSYNC_MODE_VMASTER)
			t = timestamp_apts_get();
		else
			t = timestamp_pcrscr_get();
		if (tsdemux_pcrscr_valid_cb && tsdemux_pcrscr_valid_cb() == 1) {
			if (abs(param - oldpts) > tsync_av_threshold_min) {
				vpts_discontinue = 1;
				if (apts_discontinue == 1) {
					apts_discontinue = 0;
					vpts_discontinue = 0;
					pr_info("set apts->pcrsrc,pcrsrc %x to %x,diff %d\n",
						timestamp_pcrscr_get(),
						timestamp_apts_get(),
						timestamp_apts_get()
						- timestamp_pcrscr_get());
					timestamp_pcrscr_set(param);
				} else {
					pr_info("set para->pcrsrc,pcrsrc %x to %x,diff %d\n",
						timestamp_pcrscr_get(), param,
						param-timestamp_pcrscr_get());
					timestamp_pcrscr_set(
						timestamp_vpts_get());
				}
			}
			timestamp_vpts_set(param);
			break;
		}
		/*
		 *amlog_level(LOG_LEVEL_ATTENTION,
		 *"VIDEO_TSTAMP_DISCONTINUITY, 0x%x, 0x%x\n", t, param);
		 */
		if ((abs(param - oldpts) > tsync_av_threshold_min)
			&& (!get_vsync_pts_inc_mode())) {
			vpts_discontinue = 1;
			vpts_discontinue_diff = abs(param - t);
			tsync_mode_switch('V', abs(param - t),
							  param - oldpts);
		}

		timestamp_vpts_set(param);
		if (tsync_mode == TSYNC_MODE_VMASTER) {
			timestamp_pcrscr_set(param);
			set_pts_realign();
		} else if (tsync_mode != oldmod
				 && tsync_mode == TSYNC_MODE_AMASTER) {
			timestamp_pcrscr_set(timestamp_apts_get());
			set_pts_realign();
		}
	}
	break;

	case AUDIO_TSTAMP_DISCONTINUITY: {
		unsigned int oldpts = timestamp_apts_get();
		int oldmod = tsync_mode;

		amlog_level(LOG_LEVEL_ATTENTION,
				"audio discontinue, reset apts, 0x%x\n",
				param);
		if (tsdemux_pcrscr_valid_cb && tsdemux_pcrscr_valid_cb() == 1) {
			if (abs(param - oldpts) > tsync_av_threshold_min) {
				apts_discontinue = 1;
				if (vpts_discontinue == 1) {
					pr_info("set para->pcrsrc,pcrsrc from %x to %x,diff %d\n",
						timestamp_pcrscr_get(), param,
						param-timestamp_pcrscr_get());
					apts_discontinue = 0;
					vpts_discontinue = 0;
					timestamp_pcrscr_set(param);
				} else {
					pr_info("set vpts->pcrsrc,pcrsrc from %x to %x,diff %d\n",
						timestamp_pcrscr_get(),
						timestamp_vpts_get(),
						timestamp_vpts_get()
						- timestamp_pcrscr_get());
					timestamp_pcrscr_set(
						timestamp_vpts_get());
				}
			}
			timestamp_apts_set(param);
			break;
		}
		timestamp_apts_set(param);
		if (!tsync_enable) {
			timestamp_apts_set(param);
			break;
		}
		if (tsync_mode == TSYNC_MODE_AMASTER)
			t = timestamp_vpts_get();
		else
			t = timestamp_pcrscr_get();
		amlog_level(LOG_LEVEL_ATTENTION,
				"AUDIO_TSTAMP_DISCONTINUITY, 0x%x, 0x%x\n",
				t, param);
		if ((abs(param - oldpts) > tsync_av_threshold_min)
			&& (!get_vsync_pts_inc_mode())) {
			apts_discontinue = 1;
			apts_discontinue_diff = abs(param - t);
			tsync_mode_switch('A', abs(param - t),
							  param - oldpts);
		}
		timestamp_apts_set(param);
		if (tsync_mode == TSYNC_MODE_AMASTER) {
			timestamp_pcrscr_set(param);
			set_pts_realign();
		} else if (tsync_mode != oldmod
				 && tsync_mode == TSYNC_MODE_VMASTER) {
			timestamp_pcrscr_set(timestamp_vpts_get());
			set_pts_realign();
		}
	}
	break;

	case AUDIO_PRE_START:
		timestamp_apts_start(0);
		break;

	case AUDIO_WAIT:
		timestamp_pcrscr_set(timestamp_vpts_get());
		break;
	case AUDIO_START:
		/* reset discontinue var */
		tsync_set_sync_adiscont(0);
		tsync_set_sync_adiscont_diff(0);
		tsync_set_sync_vdiscont(0);
		tsync_set_sync_vdiscont_diff(0);

		timestamp_apts_set(param);

		amlog_level(LOG_LEVEL_INFO, "audio start, reset apts = 0x%x\n",
					param);

		timestamp_apts_enable(1);

		timestamp_apts_start(1);

		if (!tsync_enable)
			break;

		t = timestamp_pcrscr_get();

		amlog_level(LOG_LEVEL_INFO,
					"[%s]param %d, t %d, tsync_abreak %d\n",
					__func__, param, t, tsync_abreak);
		/* 100ms, then wait to match */
		if (tsync_abreak &&
			(abs(param - t) > TIME_UNIT90K / 10))
			break;

		tsync_abreak = 0;
		/* after reset, video should be played first */
		if (tsync_dec_reset_flag) {
			int vpts = timestamp_vpts_get();

			if ((param < vpts) || (!tsync_dec_reset_video_start))
				timestamp_pcrscr_set(param);
			else
				timestamp_pcrscr_set(vpts);
			set_pts_realign();
			tsync_dec_reset_flag = 0;
			tsync_dec_reset_video_start = 0;
		} else if (tsync_mode == TSYNC_MODE_AMASTER) {
			timestamp_pcrscr_set(param);
			set_pts_realign();
		}

		tsync_stat = TSYNC_STAT_PCRSCR_SETUP_AUDIO;

		amlog_level(LOG_LEVEL_INFO, "apts reset scr = 0x%x\n", param);

		timestamp_pcrscr_enable(1);
		apause_flag = 0;
		break;

	case AUDIO_RESUME:
		timestamp_apts_enable(1);
		apause_flag = 0;
		if (!tsync_enable)
			break;
		timestamp_pcrscr_enable(1);

		break;

	case AUDIO_STOP:
		timestamp_apts_enable(0);
		timestamp_apts_set(-1);
		tsync_abreak = 0;
		if (tsync_trickmode)
			tsync_stat = TSYNC_STAT_PCRSCR_SETUP_VIDEO;
		else
			tsync_stat = TSYNC_STAT_PCRSCR_SETUP_NONE;
		apause_flag = 0;
		timestamp_apts_start(0);

		break;

	case AUDIO_PAUSE:
		apause_flag = 1;
		timestamp_apts_enable(0);

		if (!tsync_enable)
			break;

		timestamp_pcrscr_enable(0);
		break;

	case VIDEO_PAUSE:
		if (param == 1)
			vpause_flag = 1;
		else
			vpause_flag = 0;
		if (param == 1) {
			timestamp_pcrscr_enable(0);
			amlog_level(LOG_LEVEL_INFO, "video pause!\n");
		} else {
			if ((!apause_flag) || (!tsync_enable)) {
				timestamp_pcrscr_enable(1);
				amlog_level(LOG_LEVEL_INFO, "video resume\n");
			}
		}
		break;

	default:
		break;
	}
	switch (event) {
	case VIDEO_START:
	case AUDIO_START:
	case AUDIO_RESUME:
		/*amvdev_resume();*//*DEBUG_TMP*/
		break;
	case VIDEO_STOP:
	case AUDIO_STOP:
	case AUDIO_PAUSE:
		/*amvdev_pause();*//*DEBUG_TMP*/
		break;
	case VIDEO_PAUSE:
		/*
		 *if (vpause_flag)
		 *	amvdev_pause();
		 *else
		 *	amvdev_resume();
		 */
		//DEBUG_TMP
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(tsync_avevent_locked);

void tsync_avevent(enum avevent_e event, u32 param)
{
	ulong flags;
	ulong fiq_flag;

	amlog_level(LOG_LEVEL_INFO, "[%s]event:%d, param %d\n",
				__func__, event, param);
	spin_lock_irqsave(&lock, flags);

	raw_local_save_flags(fiq_flag);

	local_fiq_disable();

	tsync_avevent_locked(event, param);

	raw_local_irq_restore(fiq_flag);

	spin_unlock_irqrestore(&lock, flags);
}
EXPORT_SYMBOL(tsync_avevent);

void tsync_audio_break(int audio_break)
{
	tsync_abreak = audio_break;
}
EXPORT_SYMBOL(tsync_audio_break);

void tsync_trick_mode(int trick_mode)
{
	tsync_trickmode = trick_mode;
}
EXPORT_SYMBOL(tsync_trick_mode);

void tsync_set_avthresh(unsigned int av_thresh)
{
	/* tsync_av_thresh = av_thresh; */
}
EXPORT_SYMBOL(tsync_set_avthresh);

void tsync_set_syncthresh(unsigned int sync_thresh)
{
	tsync_syncthresh = sync_thresh;
}
EXPORT_SYMBOL(tsync_set_syncthresh);

void tsync_set_dec_reset(void)
{
	tsync_dec_reset_flag = 1;
}
EXPORT_SYMBOL(tsync_set_dec_reset);

void tsync_set_enable(int enable)
{
	tsync_enable = enable;
	tsync_av_mode = TSYNC_STATE_S;
}
EXPORT_SYMBOL(tsync_set_enable);

int tsync_get_sync_adiscont(void)
{
	return apts_discontinue;
}
EXPORT_SYMBOL(tsync_get_sync_adiscont);

int tsync_get_sync_vdiscont(void)
{
	return vpts_discontinue;
}
EXPORT_SYMBOL(tsync_get_sync_vdiscont);
u32 tsync_get_sync_adiscont_diff(void)
{
	return apts_discontinue_diff;
}
EXPORT_SYMBOL(tsync_get_sync_adiscont_diff);

u32 tsync_get_sync_vdiscont_diff(void)
{
	return vpts_discontinue_diff;
}
EXPORT_SYMBOL(tsync_get_sync_vdiscont_diff);
void tsync_set_sync_adiscont_diff(u32 discontinue_diff)
{
	apts_discontinue_diff = discontinue_diff;
}
EXPORT_SYMBOL(tsync_set_sync_adiscont_diff);
void tsync_set_sync_vdiscont_diff(u32 discontinue_diff)
{
	vpts_discontinue_diff = discontinue_diff;
}
EXPORT_SYMBOL(tsync_set_sync_vdiscont_diff);

void tsync_set_sync_adiscont(int syncdiscont)
{
	apts_discontinue = syncdiscont;
}
EXPORT_SYMBOL(tsync_set_sync_adiscont);

void tsync_set_sync_vdiscont(int syncdiscont)
{
	vpts_discontinue = syncdiscont;
}
EXPORT_SYMBOL(tsync_set_sync_vdiscont);

int tsync_set_video_runmode(void)
{
	if (tsdemux_pcrscr_valid_cb && tsdemux_pcrscr_valid_cb() == 1) {
		if (vpts_discontinue == 1 || apts_discontinue == 1)
			return 1;
	}
	return 0;
}
EXPORT_SYMBOL(tsync_set_video_runmode);
void tsync_set_automute_on(int automute_on)
{
	tsync_automute_on = automute_on;
}
EXPORT_SYMBOL(tsync_set_automute_on);

int tsync_set_apts(unsigned int pts)
{
	unsigned int t;
	/* ssize_t r; */
	unsigned int oldpts = timestamp_apts_get();
	int oldmod = tsync_mode;

	if (tsync_abreak)
		tsync_abreak = 0;
	if (!tsync_enable) {
		timestamp_apts_set(pts);
		return 0;
	}
	if (tsync_mode == TSYNC_MODE_AMASTER)
		t = timestamp_vpts_get();
	else
		t = timestamp_pcrscr_get();
	if (tsdemux_pcrscr_valid_cb && tsdemux_pcrscr_valid_cb() == 1) {
		timestamp_apts_set(pts);
		if ((int)(timestamp_apts_get() - timestamp_pcrscr_get())
			> 30 * TIME_UNIT90K / 1000
			|| (int)(timestamp_pcrscr_get() - timestamp_apts_get())
			> 80 * TIME_UNIT90K / 1000) {
			timestamp_pcrscr_set(pts);
			set_pts_realign();
		}
		return 0;
	}
	/* do not switch tsync mode until first video toggled. */
	if ((abs(oldpts - pts) > tsync_av_threshold_min) &&
		(timestamp_firstvpts_get() > 0)) {	/* is discontinue */
		apts_discontinue = 1;
		tsync_mode_switch('A', abs(pts - t),
				pts - oldpts);	/*if in VMASTER ,just wait */
	}
	timestamp_apts_set(pts);

	if (get_vsync_pts_inc_mode() && (tsync_mode != TSYNC_MODE_VMASTER))
		tsync_mode = TSYNC_MODE_VMASTER;

	if (tsync_mode == TSYNC_MODE_AMASTER)
		t = timestamp_pcrscr_get();

	if (tsync_mode == TSYNC_MODE_AMASTER) {
		/* special used for Dobly Certification AVSync test */
		if (dobly_avsync_test) {
			if (get_vsync_pts_inc_mode()
				&&
				(((int)(timestamp_apts_get() - t) >
				  60 * TIME_UNIT90K / 1000)
				 || (int)(t - timestamp_apts_get()) >
				 100 * TIME_UNIT90K / 1000)) {
				pr_info
				("[%d:avsync_test]reset apts:0x%x-->0x%x,",
				 __LINE__, oldpts, pts);
				pr_info(" pcr 0x%x, diff %d\n",
						t, pts - t);
				timestamp_pcrscr_set(pts);
			} else if ((!get_vsync_pts_inc_mode()) &&
					   ((int)(timestamp_apts_get() - t) >
						30 * TIME_UNIT90K / 1000
						|| (int)(t -
							timestamp_apts_get()) >
						80 * TIME_UNIT90K / 1000))
				timestamp_pcrscr_set(pts);
		} else {
			if (get_vsync_pts_inc_mode()
				&&
				(((int)(timestamp_apts_get() - t) >
				  100 * TIME_UNIT90K / 1000)
				 || (int)(t - timestamp_apts_get()) >
				 500 * TIME_UNIT90K / 1000)) {
				pr_info
				("[%d]reset apts:0x%x-->0x%x, ",
				 __LINE__, oldpts, pts);
				pr_info("pcr 0x%x, diff %d\n",
						t, pts - t);
				timestamp_pcrscr_set(pts);
				set_pts_realign();
			} else if ((!get_vsync_pts_inc_mode())
					   && (abs(timestamp_apts_get() - t) >
						   100 * TIME_UNIT90K / 1000)) {
				pr_info
				("[%d]reset apts:0x%x-->0x%x, ",
				 __LINE__, oldpts, pts);
				pr_info("pcr 0x%x, diff %d\n",
						t, pts - t);
				timestamp_pcrscr_set(pts);
				set_pts_realign();
			}
		}
	} else if ((oldmod != tsync_mode) && (tsync_mode == TSYNC_MODE_VMASTER))
		timestamp_pcrscr_set(timestamp_vpts_get());
	return 0;
}
EXPORT_SYMBOL(tsync_set_apts);

/*********************************************************/

static ssize_t show_pcr_recover(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s %s\n",
			((tsync_pcr_recover_enable) ? "on" : "off"),
			((pcr_sync_stat ==
			  PCR_SYNC_UNSET) ? ("UNSET") : ((pcr_sync_stat ==
					  PCR_SYNC_LO) ? "LO" :
							 "HI")));
}

void tsync_pcr_recover(void)
{
#if 0/* MESON_CPU_TYPE < MESON_CPU_TYPE_MESON6 */

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M6) {
		unsigned long M_nom, N_nom;

		if (tsync_pcr_recover_enable) {
			/* Set low toggle time (oscillator clock cycles) */
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_LOW_TCNT,
					LOW_TOGGLE_TIME);
			/* Set nominal toggle time (oscillator clock cycles) */
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_NOM_TCNT,
					NORMAL_TOGGLE_TIME);
			/* Set high toggle time (oscillator clock cycles) */
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_HIGH_TCNT,
					HIGH_TOGGLE_TIME);

			M_nom = READ_HHI_REG(HHI_AUD_PLL_CNTL) & 0x1ff;
			N_nom = (READ_HHI_REG(HHI_AUD_PLL_CNTL) >> 9) & 0x1f;

			WRITE_HHI_REG(HHI_AUD_PLL_MOD_CNTL0,
					(0 << 31) | (TOGGLE_MODE_FIXED << 28) |
					(N_nom << 23) |
					((M_nom + M_HIGH_DIFF) << 14) |
					(N_nom << 9) |
					((M_nom - M_LOW_DIFF) << 0));
			pcr_sync_stat = PCR_SYNC_UNSET;
			pcr_recover_trigger = 0;

			tsync_pcr_recover_timer_real();
		}
	}
#endif
}
EXPORT_SYMBOL(tsync_pcr_recover);

int tsync_get_mode(void)
{
	return tsync_mode;
}
EXPORT_SYMBOL(tsync_get_mode);

int tsync_get_debug_pts_checkin(void)
{
	return debug_pts_checkin;
}
EXPORT_SYMBOL(tsync_get_debug_pts_checkin);

int tsync_get_debug_pts_checkout(void)
{
	return debug_pts_checkout;
}
EXPORT_SYMBOL(tsync_get_debug_pts_checkout);

int tsync_get_debug_vpts(void)
{
	return debug_vpts;
}
EXPORT_SYMBOL(tsync_get_debug_vpts);

int tsync_get_debug_apts(void)
{
	return debug_apts;
}
EXPORT_SYMBOL(tsync_get_debug_apts);

int tsync_get_av_threshold_min(void)
{
	return tsync_av_threshold_min;
}
EXPORT_SYMBOL(tsync_get_av_threshold_min);

int tsync_get_av_threshold_max(void)
{
	return tsync_av_threshold_max;
}
EXPORT_SYMBOL(tsync_get_av_threshold_max);
int tsync_set_av_threshold_min(int min)
{

	return tsync_av_threshold_min = min;
}
EXPORT_SYMBOL(tsync_set_av_threshold_min);

int tsync_set_av_threshold_max(int max)
{

	return tsync_av_threshold_max = max;
}
EXPORT_SYMBOL(tsync_set_av_threshold_max);

int tsync_get_vpause_flag(void)
{
	return vpause_flag;
}
EXPORT_SYMBOL(tsync_get_vpause_flag);

int tsync_set_vpause_flag(int mode)
{
	return vpause_flag = mode;
}
EXPORT_SYMBOL(tsync_set_vpause_flag);

int tsync_get_slowsync_enable(void)
{
	return slowsync_enable;
}
EXPORT_SYMBOL(tsync_get_slowsync_enable);

int tsync_set_slowsync_enable(int enable)
{
	return slowsync_enable = enable;
}
EXPORT_SYMBOL(tsync_set_slowsync_enable);

int tsync_get_startsync_mode(void)
{
	return startsync_mode;
}
EXPORT_SYMBOL(tsync_get_startsync_mode);

int tsync_set_startsync_mode(int mode)
{
	return startsync_mode = mode;
}
EXPORT_SYMBOL(tsync_set_startsync_mode);

static ssize_t store_pcr_recover(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
#if 0				/* MESON_CPU_TYPE < MESON_CPU_TYPE_MESON6 */

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M6) {
		unsigned int val;
		unsigned long M_nom, N_nom;
		ssize_t r;

		/*r = sscanf(buf, "%d", &val);*/
		r = kstrtoint(buf, 0, &val);

		if (r != 0)
			return -EINVAL;

		if (tsync_pcr_recover_enable == (val != 0))
			return size;

		tsync_pcr_recover_enable = (val != 0);

		if (tsync_pcr_recover_enable) {
			/* Set low toggle time (oscillator clock cycles) */
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_LOW_TCNT,
					LOW_TOGGLE_TIME);
			/* Set nominal toggle time (oscillator clock cycles) */
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_NOM_TCNT,
					NORMAL_TOGGLE_TIME);
			/* Set high toggle time (oscillator clock cycles) */
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_HIGH_TCNT,
						   HIGH_TOGGLE_TIME);

			M_nom = READ_HHI_REG(HHI_AUD_PLL_CNTL) & 0x1ff;
			N_nom = (READ_HHI_REG(HHI_AUD_PLL_CNTL) >> 9) & 0x1f;

			WRITE_HHI_REG(HHI_AUD_PLL_MOD_CNTL0,
					(0 << 31) | (TOGGLE_MODE_FIXED << 28) |
					(N_nom << 23) |
					((M_nom + M_HIGH_DIFF) << 14) |
					(N_nom << 9) |
					((M_nom - M_LOW_DIFF) << 0));
			pcr_sync_stat = PCR_SYNC_UNSET;
			pcr_recover_trigger = 0;

			tsync_pcr_recover_timer_real();

		} else {
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_CNTL0,
					READ_HHI_REG(HHI_AUD_PLL_MOD_CNTL0) &
					(~
					 ((1 << 31) |
					  (TOGGLE_MODE_LOW_HIGH << 28))));
			WRITE_HHI_REG(HHI_AUD_PLL_MOD_CNTL0,
					READ_HHI_REG(HHI_AUD_PLL_MOD_CNTL0) |
					(TOGGLE_MODE_FIXED << 28));
			pcr_sync_stat = PCR_SYNC_UNSET;
			pcr_recover_trigger = 0;
		}
	}
#endif

	return size;
}

static ssize_t show_vpts(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_vpts_get());
}

static ssize_t store_vpts(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int pts;
	ssize_t r;

	/*r = sscanf(buf, "0x%x", &pts);*/
	r = kstrtouint(buf, 0, &pts);

	if (r != 0)
		return -EINVAL;

	timestamp_vpts_set(pts);
	return size;
}

static ssize_t show_demux_pcr(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_tsdemux_pcr_get());
}

static ssize_t show_apts(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_apts_get());
}

static ssize_t store_apts(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int pts;
	ssize_t r;
	r = kstrtouint(buf, 0, &pts);

	if (r != 0)
		return -EINVAL;

	tsync_set_apts(pts);

	return size;
}

static ssize_t dobly_show_sync(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf,
		"avsync:cur value is %d\n 0:no set(default)\n 1:avsync test\n",
		dobly_avsync_test ? 1 : 0);
}

static ssize_t dobly_store_sync(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int value;
	ssize_t r;

	/*r = sscanf(buf, "%u", &value);*/
	r = kstrtoint(buf, 0, &value);

	if (r != 0)
		return -EINVAL;

	if (value == 1)
		dobly_avsync_test = true;
	else
		dobly_avsync_test = false;

	return size;
}

static ssize_t show_pcrscr(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_pcrscr_get());
}

static ssize_t show_aptscheckin_flag(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", first_pts_checkin_complete(PTS_TYPE_AUDIO));
}

static ssize_t store_pcrscr(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int pts;
	ssize_t r;

	/*r = sscanf(buf, "0x%x", &pts);*/
	r = kstrtoint(buf, 0, &pts);

	if (r != 0)
		return -EINVAL;

	timestamp_pcrscr_set(pts);
	set_pts_realign();

	return size;
}

static ssize_t store_event(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(avevent_token); i++) {
		if (strncmp
			(avevent_token[i].token, buf,
			 avevent_token[i].token_size) == 0) {
			if (avevent_token[i].flag & AVEVENT_FLAG_PARAM) {
				char *param_str = strchr(buf, ':');
				unsigned long value;
				int ret;

				if (!param_str)
					return -EINVAL;
				ret = kstrtoul(param_str+1, 0,
				(unsigned long *)&value);
				if (ret < 0)
					return -EINVAL;
				tsync_avevent(avevent_token[i].event,
						value);
			} else
				tsync_avevent(avevent_token[i].event, 0);

			return size;
		}
	}

	return -EINVAL;
}

static ssize_t show_mode(struct class *class,
		struct class_attribute *attr, char *buf)
{
	if (tsync_mode <= TSYNC_MODE_PCRMASTER) {
		return sprintf(buf, "%d: %s\n", tsync_mode,
					   tsync_mode_str[tsync_mode]);
	}

	return sprintf(buf, "invalid mode");
}

static ssize_t store_mode(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if ((r != 0))
		return -EINVAL;

	if (mode == TSYNC_MODE_PCRMASTER)
		tsync_mode = TSYNC_MODE_PCRMASTER;
	else if (mode == TSYNC_MODE_VMASTER)
		tsync_mode = TSYNC_MODE_VMASTER;
	else
		tsync_mode = TSYNC_MODE_AMASTER;

	pr_info("[%s]tsync_mode=%d, buf=%s\n", __func__, tsync_mode, buf);
	return size;
}

static ssize_t show_enable(struct class *class,
		struct class_attribute *attr, char *buf)
{
	if (tsync_enable)
		return sprintf(buf, "1: enabled\n");

	return sprintf(buf, "0: disabled\n");
}

static ssize_t store_enable(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if ((r != 0))
		return -EINVAL;

	if (!tsync_automute_on)
		tsync_enable = mode ? 1 : 0;

	return size;
}

static ssize_t show_discontinue(struct class *class,
		struct class_attribute *attr, char *buf)
{
	pts_discontinue = vpts_discontinue || apts_discontinue;
	if (pts_discontinue) {
		sprintf(buf, "1: pts_discontinue, ");
		sprintf(buf, "%sapts_discontinue_diff=%d, ",
				buf, apts_discontinue_diff);
		return sprintf(buf, "%svpts_discontinue_diff=%d,\n",
				buf, vpts_discontinue_diff);
	}

	return sprintf(buf, "0: pts_continue\n");
}

static ssize_t store_discontinue(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int discontinue;
	ssize_t r;

	/*r = sscanf(buf, "%d", &discontinue);*/
	r = kstrtoint(buf, 0, &discontinue);

	if ((r != 0))
		return -EINVAL;

	pts_discontinue = discontinue ? 1 : 0;

	return size;
}

static ssize_t show_debug_pts_checkin(struct class *class,
		struct class_attribute *attr, char *buf)
{
	if (debug_pts_checkin)
		return sprintf(buf, "1: debug pts checkin on\n");

	return sprintf(buf, "0: debug pts checkin off\n");
}

static ssize_t store_debug_pts_checkin(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if ((r != 0))
		return -EINVAL;

	debug_pts_checkin = mode ? 1 : 0;

	return size;
}

static ssize_t show_debug_pts_checkout(struct class *class,
		struct class_attribute *attr, char *buf)
{
	if (debug_pts_checkout)
		return sprintf(buf, "1: debug pts checkout on\n");

	return sprintf(buf, "0: debug pts checkout off\n");
}

static ssize_t store_debug_pts_checkout(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if ((r != 0))
		return -EINVAL;

	debug_pts_checkout = mode ? 1 : 0;

	return size;
}

static ssize_t show_debug_vpts(struct class *class,
		struct class_attribute *attr, char *buf)
{
	if (debug_vpts)
		return sprintf(buf, "1: debug vpts on\n");

	return sprintf(buf, "0: debug vpts off\n");
}

static ssize_t store_debug_vpts(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if ((r != 0))
		return -EINVAL;

	debug_vpts = mode ? 1 : 0;

	return size;
}

static ssize_t show_debug_apts(struct class *class,
		struct class_attribute *attr, char *buf)
{
	if (debug_apts)
		return sprintf(buf, "1: debug apts on\n");

	return sprintf(buf, "0: debug apts off\n");
}

static ssize_t store_debug_apts(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if ((r != 0))
		return -EINVAL;

	debug_apts = mode ? 1 : 0;

	return size;
}

static ssize_t show_av_threshold_min(struct class *class,
		struct class_attribute *attr, char *buf)
{

	return sprintf(buf, "tsync_av_threshold_min=%d\n",
				   tsync_av_threshold_min);

}

static ssize_t store_av_threshold_min(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int min;
	ssize_t r;

	/*r = sscanf(buf, "%d", &min);*/
	r = kstrtoint(buf, 0, &min);

	if (r != 0)
		return -EINVAL;

	tsync_set_av_threshold_min(min);
	return size;
}

static ssize_t show_av_threshold_max(struct class *class,
		struct class_attribute *attr, char *buf)
{

	return sprintf(buf, "tsync_av_threshold_max=%d\n",
				   tsync_av_threshold_max);

}

static ssize_t store_av_threshold_max(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int max;
	ssize_t r;

	/*r = sscanf(buf, "%d", &max);*/
	r = kstrtoint(buf, 0, &max);

	if (r != 0)
		return -EINVAL;

	tsync_set_av_threshold_max(max);
	return size;
}

static ssize_t show_last_checkin_apts(struct class *class,
		struct class_attribute *attr, char *buf)
{
	unsigned int last_apts;

	last_apts = get_last_checkin_pts(PTS_TYPE_AUDIO);
	return sprintf(buf, "0x%x\n", last_apts);
}

static ssize_t show_firstvpts(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_firstvpts_get());
}

static ssize_t show_firstapts(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_firstapts_get());
}

static ssize_t store_firstapts(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int pts;
	ssize_t r;

	/*r = sscanf(buf, "0x%x", &pts);*/
	r = kstrtoint(buf, 0, &pts);

	if (r != 0)
		return -EINVAL;

	timestamp_firstapts_set(pts);

	return size;
}

static ssize_t show_checkin_firstvpts(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_checkin_firstvpts_get());
}

static ssize_t show_checkin_firstapts(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_checkin_firstapts_get());
}

static ssize_t show_vpause_flag(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", tsync_get_vpause_flag());
}

static ssize_t store_vpause_flag(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if (r != 0)
		return -EINVAL;
	tsync_set_vpause_flag(mode);
	return size;
}

static ssize_t show_slowsync_enable(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "slowsync enable:0x%x\n",
				   tsync_get_slowsync_enable());
}

static ssize_t store_slowsync_enable(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if (r != 0)
		return -EINVAL;

	tsync_set_slowsync_enable(mode);
	return size;
}

static ssize_t show_startsync_mode(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", tsync_get_startsync_mode());
}


static ssize_t show_apts_lookup(struct class *class,
	struct class_attribute *attrr, char *buf)
{
	u32 frame_size;
	unsigned int  pts = 0xffffffff;
	pts_lookup_offset(PTS_TYPE_AUDIO, apts_lookup_offset,
		&pts, &frame_size, 300);
	return sprintf(buf, "0x%x\n", pts);
}

static ssize_t store_apts_lookup(struct class *class,
	 struct class_attribute *attr, const char *buf, size_t size)
{
	unsigned int offset;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &offset);

	if (r != 0)
		return -EINVAL;
	apts_lookup_offset = offset;
	return size;
}


static ssize_t store_startsync_mode(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);
	if (r != 0)
		return -EINVAL;

	tsync_set_startsync_mode(mode);
	return size;
}

static struct class_attribute tsync_class_attrs[] = {
	__ATTR(pts_video, 0664, show_vpts, store_vpts),
	__ATTR(pts_audio, 0664, show_apts, store_apts),
	__ATTR(dobly_av_sync, 0664, dobly_show_sync,
	dobly_store_sync),
	__ATTR(pts_pcrscr, 0664, show_pcrscr,
	store_pcrscr),
	__ATTR(apts_checkin_flag, 0664, show_aptscheckin_flag, NULL),
	__ATTR(event, 0664, NULL, store_event),
	__ATTR(mode, 0664, show_mode, store_mode),
	__ATTR(enable, 0664, show_enable, store_enable),
	__ATTR(pcr_recover, 0664, show_pcr_recover,
	store_pcr_recover),
	__ATTR(discontinue, 0644, show_discontinue,
	store_discontinue),
	__ATTR(debug_pts_checkin, 0644, show_debug_pts_checkin,
	store_debug_pts_checkin),
	__ATTR(debug_pts_checkout, 0644, show_debug_pts_checkout,
	store_debug_pts_checkout),
	__ATTR(debug_video_pts, 0644, show_debug_vpts,
	store_debug_vpts),
	__ATTR(debug_audio_pts, 0644, show_debug_apts,
	store_debug_apts),
	__ATTR(av_threshold_min, 0664,
	show_av_threshold_min, store_av_threshold_min),
	__ATTR(av_threshold_max, 0664,
	show_av_threshold_max, store_av_threshold_max),
	__ATTR(last_checkin_apts, 0644, show_last_checkin_apts,
	NULL),
	__ATTR(firstvpts, 0644, show_firstvpts, NULL),
	__ATTR(vpause_flag, 0644, show_vpause_flag,
	store_vpause_flag),
	__ATTR(slowsync_enable, 0644, show_slowsync_enable,
	store_slowsync_enable),
	__ATTR(startsync_mode, 0644, show_startsync_mode,
	store_startsync_mode),
	__ATTR(firstapts, 0664, show_firstapts,
	store_firstapts),
	__ATTR(checkin_firstvpts, 0644, show_checkin_firstvpts,
	NULL),
	__ATTR(apts_lookup, 0644, show_apts_lookup,
	store_apts_lookup),
	__ATTR(demux_pcr, 0644, show_demux_pcr,
	NULL),
	__ATTR(checkin_firstapts, 0644, show_checkin_firstapts,
	NULL),
	__ATTR_NULL
};

static struct class tsync_class = {
		.name = "tsync",
		.class_attrs = tsync_class_attrs,
	};


int tsync_store_fun(const char *trigger, int id, const char *buf, int size)
{
	int ret = size;

	switch (id) {
	case 0:	return store_vpts(NULL, NULL, buf, size);
	case 1:	return store_apts(NULL, NULL, buf, size);
	case 2:	return dobly_store_sync(NULL, NULL, buf, size);
	case 3:	return store_pcrscr(NULL, NULL, buf, size);
	case 4:	return store_event(NULL, NULL, buf, size);
	case 5:	return store_mode(NULL, NULL, buf, size);
	case 6:	return store_enable(NULL, NULL, buf, size);
	case 7:	return store_pcr_recover(NULL, NULL, buf, size);
	case 8:	return store_discontinue(NULL, NULL, buf, size);
	case 9:	return store_debug_pts_checkin(NULL, NULL, buf, size);
	case 10:return store_debug_pts_checkout(NULL, NULL, buf, size);
	case 11:return store_debug_vpts(NULL, NULL, buf, size);
	case 12:return store_debug_apts(NULL, NULL, buf, size);
	case 13:return store_av_threshold_min(NULL, NULL, buf, size);
	case 14:return store_av_threshold_max(NULL, NULL, buf, size);
	/*case 15:return -1;*/
	/*case 16:return -1;*/
	case 17:return store_vpause_flag(NULL, NULL, buf, size);
	case 18:return store_slowsync_enable(NULL, NULL, buf, size);
	case 19:return store_startsync_mode(NULL, NULL, buf, size);
	case 20:return store_firstapts(NULL, NULL, buf, size);
	case 21:return -1;
	default:
		ret = -1;
	}
	return size;
}
int tsync_show_fun(const char *trigger, int id, char *sbuf, int size)
{
	int ret = -1;
	void *buf, *getbuf = NULL;

	if (size < PAGE_SIZE) {
		getbuf = (void *)__get_free_page(GFP_KERNEL);
		if (!getbuf)
			return -ENOMEM;
		buf = getbuf;
	} else {
		buf = sbuf;
	}

	switch (id) {
	case 0:
		ret = show_vpts(NULL, NULL, buf);
		break;
	case 1:
		ret = show_apts(NULL, NULL, buf);
		break;
	case 2:
		ret =  dobly_show_sync(NULL, NULL, buf);
		break;
	case 3:
		ret = show_pcrscr(NULL, NULL, buf);
		break;
	case 4:
		ret = -1;
		break;
	case 5:
		ret = show_mode(NULL, NULL, buf);
		break;
	case 6:
		ret = show_enable(NULL, NULL, buf);
		break;
	case 7:
		ret = show_pcr_recover(NULL, NULL, buf);
		break;
	case 8:
		ret = show_discontinue(NULL, NULL, buf);
		break;
	case 9:
		ret = show_debug_pts_checkin(NULL, NULL, buf);
		break;
	case 10:
		ret = show_debug_pts_checkout(NULL, NULL, buf);
		break;
	case 11:
		ret = show_debug_vpts(NULL, NULL, buf);
		break;
	case 12:
		ret = show_debug_apts(NULL, NULL, buf);
		break;
	case 13:
		ret = show_av_threshold_min(NULL, NULL, buf);
		break;
	case 14:
		ret = show_av_threshold_max(NULL, NULL, buf);
		break;
	case 15:
		ret = show_last_checkin_apts(NULL, NULL, buf);
		break;
	case 16:
		ret = show_firstvpts(NULL, NULL, buf);
		break;
	case 17:
		ret = show_vpause_flag(NULL, NULL, buf);
		break;
	case 18:
		ret = show_slowsync_enable(NULL, NULL, buf);
		break;
	case 19:
		ret = show_startsync_mode(NULL, NULL, buf);
		break;
	case 20:
		ret = show_firstapts(NULL, NULL, buf);
		break;
	case 21:
		ret = show_checkin_firstvpts(NULL, NULL, buf);
		break;
	default:
		ret = -1;
	}
	if (ret > 0 && getbuf != NULL) {
		ret = min_t(int, ret, size);
		strncpy(sbuf, buf, ret);
	}
	if (getbuf != NULL)
		free_page((unsigned long)getbuf);
	return ret;
}


static struct mconfig tsync_configs[] = {
	MC_FUN_ID("pts_video", tsync_show_fun, tsync_store_fun, 0),
	MC_FUN_ID("pts_audio", tsync_show_fun, tsync_store_fun, 1),
	MC_FUN_ID("dobly_av_sync", tsync_show_fun, tsync_store_fun, 2),
	MC_FUN_ID("pts_pcrscr", tsync_show_fun, tsync_store_fun, 3),
	MC_FUN_ID("event", tsync_show_fun, tsync_store_fun, 4),
	MC_FUN_ID("mode", tsync_show_fun, tsync_store_fun, 5),
	MC_FUN_ID("enable", tsync_show_fun, tsync_store_fun, 6),
	MC_FUN_ID("pcr_recover", tsync_show_fun, tsync_store_fun, 7),
	MC_FUN_ID("discontinue", tsync_show_fun, tsync_store_fun, 8),
	MC_FUN_ID("debug_pts_checkin", tsync_show_fun, tsync_store_fun, 9),
	MC_FUN_ID("debug_pts_checkout", tsync_show_fun, tsync_store_fun, 10),
	MC_FUN_ID("debug_video_pts", tsync_show_fun, tsync_store_fun, 11),
	MC_FUN_ID("debug_audio_pts", tsync_show_fun, tsync_store_fun, 12),
	MC_FUN_ID("av_threshold_min", tsync_show_fun, tsync_store_fun, 13),
	MC_FUN_ID("av_threshold_max", tsync_show_fun, tsync_store_fun, 14),
	MC_FUN_ID("last_checkin_apts", tsync_show_fun, NULL, 15),
	MC_FUN_ID("firstvpts", tsync_show_fun, NULL, 16),
	MC_FUN_ID("vpause_flag", tsync_show_fun, tsync_store_fun, 17),
	MC_FUN_ID("slowsync_enable", tsync_show_fun, tsync_store_fun, 18),
	MC_FUN_ID("startsync_mode", tsync_show_fun, tsync_store_fun, 19),
	MC_FUN_ID("firstapts", tsync_show_fun, tsync_store_fun, 20),
	MC_FUN_ID("checkin_firstvpts", tsync_show_fun, NULL, 21),
};


static int __init tsync_module_init(void)
{
	int r;

	r = class_register(&tsync_class);

	if (r) {
		amlog_level(LOG_LEVEL_ERROR, "tsync class create fail.\n");
		return r;
	}

	/* init audio pts to -1, others to 0 */
	timestamp_apts_set(-1);
	timestamp_vpts_set(0);
	timestamp_pcrscr_set(0);

	init_timer(&tsync_pcr_recover_timer);

	tsync_pcr_recover_timer.function = tsync_pcr_recover_timer_func;
	tsync_pcr_recover_timer.expires = jiffies + PCR_CHECK_INTERVAL;
	pcr_sync_stat = PCR_SYNC_UNSET;
	pcr_recover_trigger = 0;

	add_timer(&tsync_pcr_recover_timer);

	init_timer(&tsync_state_switch_timer);
	tsync_state_switch_timer.function = tsync_state_switch_timer_fun;
	tsync_state_switch_timer.expires = jiffies + 1;

	add_timer(&tsync_state_switch_timer);
	REG_PATH_CONFIGS("media.tsync", tsync_configs);
	return 0;
}

static void __exit tsync_module_exit(void)
{
		del_timer_sync(&tsync_pcr_recover_timer);

	class_unregister(&tsync_class);
}

module_init(tsync_module_init);
module_exit(tsync_module_exit);
MODULE_DESCRIPTION("AMLOGIC time sync management driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
