/*
 * drivers/amlogic/media/dtv_demod/include/addr_dtmb_top.h
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

#ifndef __ADDR_DTMB_TOP_H__
#define __ADDR_DTMB_TOP_H__

#include "addr_dtmb_top_bit.h"
#include "addr_dtmb_sync.h"
#include "addr_dtmb_sync_bit.h"
#include "addr_dtmb_che.h"
#include "addr_dtmb_che_bit.h"
#include "addr_dtmb_front.h"
#include "addr_dtmb_front_bit.h"

/* #define DTMB_DEMOD_BASE DEMOD_REG_ADDR(0x0) */
#define DTMB_DEMOD_BASE		DEMOD_REG_ADDR_OFFSET(0x0)

#define  DTMB_TOP_ADDR(x) (DTMB_DEMOD_BASE + (x << 2))

#define  DTMB_TOP_CTRL_SW_RST               DTMB_TOP_ADDR(0x1)
#define  DTMB_TOP_TESTBUS                   DTMB_TOP_ADDR(0x2)
#define  DTMB_TOP_TB                        DTMB_TOP_ADDR(0x3)
#define  DTMB_TOP_TB_V                      DTMB_TOP_ADDR(0x4)
#define  DTMB_TOP_TB_ADDR_BEGIN             DTMB_TOP_ADDR(0x5)
#define  DTMB_TOP_TB_ADDR_END               DTMB_TOP_ADDR(0x6)
#define  DTMB_TOP_CTRL_ENABLE               DTMB_TOP_ADDR(0x7)
#define  DTMB_TOP_CTRL_LOOP                 DTMB_TOP_ADDR(0x8)
#define  DTMB_TOP_CTRL_FSM                  DTMB_TOP_ADDR(0x9)
#define  DTMB_TOP_CTRL_AGC                  DTMB_TOP_ADDR(0xa)
#define  DTMB_TOP_CTRL_TS_SFO_CFO           DTMB_TOP_ADDR(0xb)
#define  DTMB_TOP_CTRL_FEC                  DTMB_TOP_ADDR(0xc)
#define  DTMB_TOP_CTRL_INTLV_TIME           DTMB_TOP_ADDR(0xd)
#define  DTMB_TOP_CTRL_DAGC_CCI             DTMB_TOP_ADDR(0xe)
#define  DTMB_TOP_CTRL_TPS                  DTMB_TOP_ADDR(0xf)
#define  DTMB_TOP_TPS_BIT                   DTMB_TOP_ADDR(0x10)
#define  DTMB_TOP_CCI_FLG                   DTMB_TOP_ADDR(0xc7)
#define  DTMB_TOP_TESTBUS_OUT               DTMB_TOP_ADDR(0xc8)
#define  DTMB_TOP_TBUS_DC_ADDR              DTMB_TOP_ADDR(0xc9)
#define  DTMB_TOP_FRONT_IQIB_CHECK          DTMB_TOP_ADDR(0xca)
#define  DTMB_TOP_SYNC_TS                   DTMB_TOP_ADDR(0xcb)
#define  DTMB_TOP_SYNC_PNPHASE              DTMB_TOP_ADDR(0xcd)
#define  DTMB_TOP_CTRL_DDC_ICFO             DTMB_TOP_ADDR(0xd2)
#define  DTMB_TOP_CTRL_DDC_FCFO             DTMB_TOP_ADDR(0xd3)
#define  DTMB_TOP_CTRL_FSM_STATE0           DTMB_TOP_ADDR(0xd4)
#define  DTMB_TOP_CTRL_FSM_STATE1           DTMB_TOP_ADDR(0xd5)
#define  DTMB_TOP_CTRL_FSM_STATE2           DTMB_TOP_ADDR(0xd6)
#define  DTMB_TOP_CTRL_FSM_STATE3           DTMB_TOP_ADDR(0xd7)
#define  DTMB_TOP_CTRL_TS2                  DTMB_TOP_ADDR(0xd8)
#define  DTMB_TOP_FRONT_AGC                 DTMB_TOP_ADDR(0xd9)
#define  DTMB_TOP_FRONT_DAGC                DTMB_TOP_ADDR(0xda)
#define  DTMB_TOP_FEC_TIME_STS              DTMB_TOP_ADDR(0xdb)
#define  DTMB_TOP_FEC_LDPC_STS              DTMB_TOP_ADDR(0xdc)
#define  DTMB_TOP_FEC_LDPC_IT_AVG           DTMB_TOP_ADDR(0xdd)
#define  DTMB_TOP_FEC_LDPC_UNC_ACC          DTMB_TOP_ADDR(0xde)
#define  DTMB_TOP_FEC_BCH_ACC               DTMB_TOP_ADDR(0xdf)
#define  DTMB_TOP_CTRL_ICFO_ALL             DTMB_TOP_ADDR(0xe0)
#define  DTMB_TOP_CTRL_FCFO_ALL             DTMB_TOP_ADDR(0xe1)
#define  DTMB_TOP_CTRL_SFO_ALL              DTMB_TOP_ADDR(0xe2)
#define  DTMB_TOP_FEC_LOCK_SNR              DTMB_TOP_ADDR(0xe3)
#define  DTMB_TOP_CHE_SEG_FACTOR            DTMB_TOP_ADDR(0xe4)
#define  DTMB_TOP_CTRL_CHE_WORKCNT          DTMB_TOP_ADDR(0xe5)
#define  DTMB_TOP_CHE_OBS_STATE1            DTMB_TOP_ADDR(0xe6)
#define  DTMB_TOP_CHE_OBS_STATE2            DTMB_TOP_ADDR(0xe7)
#define  DTMB_TOP_CHE_OBS_STATE3            DTMB_TOP_ADDR(0xe8)
#define  DTMB_TOP_CHE_OBS_STATE4            DTMB_TOP_ADDR(0xe9)
#define  DTMB_TOP_CHE_OBS_STATE5            DTMB_TOP_ADDR(0xea)
#define  DTMB_TOP_SYNC_CCI_NF1              DTMB_TOP_ADDR(0xee)
#define  DTMB_TOP_SYNC_CCI_NF2              DTMB_TOP_ADDR(0xef)
#define  DTMB_TOP_SYNC_CCI_NF2_POSITION     DTMB_TOP_ADDR(0xf0)
#define  DTMB_TOP_CTRL_SYS_OFDM_CNT         DTMB_TOP_ADDR(0xf1)
#define  DTMB_TOP_CTRL_TPS_Q_FINAL          DTMB_TOP_ADDR(0xf2)
#define  DTMB_TOP_FRONT_DC                  DTMB_TOP_ADDR(0xf3)
#define  DTMB_TOP_CHE_DEBUG                 DTMB_TOP_ADDR(0xf6)
#define  DTMB_TOP_CTRL_TOTPS_READY_CNT      DTMB_TOP_ADDR(0xff)

#endif
