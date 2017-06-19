/*
 * include/linux/amlogic/media/registers/regs/parser_regs.h
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

#ifndef PARSER_REGS_HEADER_
#define PARSER_REGS_HEADER_

/*
 * pay attention : the regs range has
 * changed to 0x38xx in txlx, it was
 * converted automatically based on
 * the platform at init.
 * #define PARSER_CONTROL 0x3860
 */
#define PARSER_CONTROL 0x2960
#define PARSER_FETCH_ADDR 0x2961
#define PARSER_FETCH_CMD 0x2962
#define PARSER_FETCH_STOP_ADDR 0x2963
#define PARSER_FETCH_LEVEL 0x2964
#define PARSER_CONFIG 0x2965
#define PFIFO_WR_PTR 0x2966
#define PFIFO_RD_PTR 0x2967
#define PFIFO_DATA 0x2968
#define PARSER_SEARCH_PATTERN 0x2969
#define PARSER_SEARCH_MASK 0x296a
#define PARSER_INT_ENABLE 0x296b
#define PARSER_INT_STATUS 0x296c
#define PARSER_SCR_CTL 0x296d
#define PARSER_SCR 0x296e
#define PARSER_PARAMETER 0x296f
#define PARSER_INSERT_DATA 0x2970
#define VAS_STREAM_ID 0x2971
#define VIDEO_DTS 0x2972
#define VIDEO_PTS 0x2973
#define VIDEO_PTS_DTS_WR_PTR 0x2974
#define AUDIO_PTS 0x2975
#define AUDIO_PTS_WR_PTR 0x2976
#define PARSER_ES_CONTROL 0x2977
#define PFIFO_MONITOR 0x2978
#define PARSER_VIDEO_START_PTR 0x2980
#define PARSER_VIDEO_END_PTR 0x2981
#define PARSER_VIDEO_WP 0x2982
#define PARSER_VIDEO_RP 0x2983
#define PARSER_VIDEO_HOLE 0x2984
#define PARSER_AUDIO_START_PTR 0x2985
#define PARSER_AUDIO_END_PTR 0x2986
#define PARSER_AUDIO_WP 0x2987
#define PARSER_AUDIO_RP 0x2988
#define PARSER_AUDIO_HOLE 0x2989
#define PARSER_SUB_START_PTR 0x298a
#define PARSER_SUB_END_PTR 0x298b
#define PARSER_SUB_WP 0x298c
#define PARSER_SUB_RP 0x298d
#define PARSER_SUB_HOLE 0x298e
#define PARSER_FETCH_INFO 0x298f
#define PARSER_STATUS 0x2990
#define PARSER_AV_WRAP_COUNT 0x2991
#define WRRSP_PARSER 0x2992
#define PARSER_VIDEO2_START_PTR 0x2993
#define PARSER_VIDEO2_END_PTR 0x2994
#define PARSER_VIDEO2_WP 0x2995
#define PARSER_VIDEO2_RP 0x2996
#define PARSER_VIDEO2_HOLE 0x2997
#define PARSER_AV2_WRAP_COUNT 0x2998

#endif

