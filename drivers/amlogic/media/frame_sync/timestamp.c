/*
 * drivers/amlogic/media/frame_sync/timestamp.c
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
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/registers/register.h>


u32 acc_apts_inc;
u32 acc_apts_dec;
u32 acc_pcrscr_inc;
u32 acc_pcrscr_dec;

static s32 system_time_inc_adj;
static u32 system_time;
static u32 system_time_up;
static u32 audio_pts_up;
static u32 audio_pts_started;
static u32 first_vpts;
static u32 first_checkin_vpts = 0xffffffff;
static u32 first_apts;

static u32 system_time_scale_base = 1;
static u32 system_time_scale_remainder;

#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
#define PLL_FACTOR 10000
static u32 timestamp_inc_factor = PLL_FACTOR;
void set_timestamp_inc_factor(u32 factor)
{
	timestamp_inc_factor = factor;
}
#endif

u32 timestamp_vpts_get(void)
{
	return (u32) READ_PARSER_REG(VIDEO_PTS);
}
EXPORT_SYMBOL(timestamp_vpts_get);

void timestamp_vpts_set(u32 pts)
{
	WRITE_PARSER_REG(VIDEO_PTS, pts);
}
EXPORT_SYMBOL(timestamp_vpts_set);

void timestamp_vpts_inc(s32 val)
{
	WRITE_PARSER_REG(VIDEO_PTS, READ_PARSER_REG(VIDEO_PTS) + val);
}
EXPORT_SYMBOL(timestamp_vpts_inc);

u32 timestamp_apts_get(void)
{
	return (u32) READ_PARSER_REG(AUDIO_PTS);
}
EXPORT_SYMBOL(timestamp_apts_get);

void timestamp_apts_set(u32 pts)
{
	WRITE_PARSER_REG(AUDIO_PTS, pts);
}
EXPORT_SYMBOL(timestamp_apts_set);

void timestamp_apts_inc(s32 inc)
{
	if (audio_pts_up) {
#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
		inc = inc * timestamp_inc_factor / PLL_FACTOR;
#endif
		WRITE_PARSER_REG(AUDIO_PTS, READ_PARSER_REG(AUDIO_PTS) + inc);
	}
}
EXPORT_SYMBOL(timestamp_apts_inc);

void timestamp_apts_enable(u32 enable)
{
	audio_pts_up = enable;
	pr_info("timestamp_apts_enable enable:%x,\n", enable);
}
EXPORT_SYMBOL(timestamp_apts_enable);

void timestamp_apts_start(u32 enable)
{
	audio_pts_started = enable;
	pr_info("audio pts started::::::: %d\n", enable);
}
EXPORT_SYMBOL(timestamp_apts_start);

u32 timestamp_apts_started(void)
{
	return audio_pts_started;
}
EXPORT_SYMBOL(timestamp_apts_started);

u32 timestamp_pcrscr_get(void)
{
	return system_time;
}
EXPORT_SYMBOL(timestamp_pcrscr_get);

void timestamp_pcrscr_set(u32 pts)
{
	system_time = pts;
}
EXPORT_SYMBOL(timestamp_pcrscr_set);

void timestamp_firstvpts_set(u32 pts)
{
	first_vpts = pts;
	pr_info("video first pts = %x\n", first_vpts);
}
EXPORT_SYMBOL(timestamp_firstvpts_set);

u32 timestamp_firstvpts_get(void)
{
	return first_vpts;
}
EXPORT_SYMBOL(timestamp_firstvpts_get);

void timestamp_checkin_firstvpts_set(u32 pts)
{
	first_checkin_vpts = pts;
	pr_info("video first checkin pts = %x\n", first_checkin_vpts);
}
EXPORT_SYMBOL(timestamp_checkin_firstvpts_set);

u32 timestamp_checkin_firstvpts_get(void)
{
	return first_checkin_vpts;
}
EXPORT_SYMBOL(timestamp_checkin_firstvpts_get);

void timestamp_firstapts_set(u32 pts)
{
	first_apts = pts;
	pr_info("audio first pts = %x\n", first_apts);
}
EXPORT_SYMBOL(timestamp_firstapts_set);

u32 timestamp_firstapts_get(void)
{
	return first_apts;
}
EXPORT_SYMBOL(timestamp_firstapts_get);

void timestamp_pcrscr_inc(s32 inc)
{
	if (system_time_up) {
#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
		inc = inc * timestamp_inc_factor / PLL_FACTOR;
#endif
		system_time += inc + system_time_inc_adj;
	}
}
EXPORT_SYMBOL(timestamp_pcrscr_inc);

void timestamp_pcrscr_inc_scale(s32 inc, u32 base)
{
	if (system_time_scale_base != base) {
		system_time_scale_remainder =
			system_time_scale_remainder *
			base / system_time_scale_base;
		system_time_scale_base = base;
	}

	if (system_time_up) {
		u32 r;

		system_time +=
			div_u64_rem(90000ULL * inc, base, &r) +
			system_time_inc_adj;
		system_time_scale_remainder += r;
		if (system_time_scale_remainder >= system_time_scale_base) {
			system_time++;
			system_time_scale_remainder -= system_time_scale_base;
		}
	}
}
EXPORT_SYMBOL(timestamp_pcrscr_inc_scale);

void timestamp_pcrscr_set_adj(s32 inc)
{
	system_time_inc_adj = inc;
}
EXPORT_SYMBOL(timestamp_pcrscr_set_adj);

void timestamp_pcrscr_enable(u32 enable)
{
	system_time_up = enable;
}
EXPORT_SYMBOL(timestamp_pcrscr_enable);

u32 timestamp_pcrscr_enable_state(void)
{
	return system_time_up;
}
EXPORT_SYMBOL(timestamp_pcrscr_enable_state);

MODULE_DESCRIPTION("AMLOGIC time sync management driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
