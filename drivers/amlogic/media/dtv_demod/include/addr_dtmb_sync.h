/*
 * drivers/amlogic/media/dtv_demod/include/addr_dtmb_sync.h
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

#ifndef __ADDR_DTMB_SYNC_H__
#define __ADDR_DTMB_SYNC_H__

/*#include "addr_dtmb_top.h"*/
#define  DTMB_SYNC_ADDR(x) (DTMB_DEMOD_BASE + (x << 2))

#define  DTMB_SYNC_TS_CFO_PN_VALUE           DTMB_SYNC_ADDR(0x57)
#define  DTMB_SYNC_TS_CFO_ERR_LIMIT          DTMB_SYNC_ADDR(0x58)
#define  DTMB_SYNC_TS_CFO_PN_MODIFY          DTMB_SYNC_ADDR(0x59)
#define  DTMB_SYNC_TS_GAIN                   DTMB_SYNC_ADDR(0x5a)
#define  DTMB_SYNC_FE_CONFIG                 DTMB_SYNC_ADDR(0x5b)
#define  DTMB_SYNC_PNPHASE_OFFSET            DTMB_SYNC_ADDR(0x5c)
#define  DTMB_SYNC_PNPHASE_CONFIG            DTMB_SYNC_ADDR(0x5d)
#define  DTMB_SYNC_SFO_SFO_PN0_MODIFY        DTMB_SYNC_ADDR(0x5e)
#define  DTMB_SYNC_SFO_SFO_PN1_MODIFY        DTMB_SYNC_ADDR(0x5f)
#define  DTMB_SYNC_SFO_SFO_PN2_MODIFY        DTMB_SYNC_ADDR(0x60)
#define  DTMB_SYNC_SFO_CONFIG                DTMB_SYNC_ADDR(0x61)
#define  DTMB_SYNC_FEC_CFG                   DTMB_SYNC_ADDR(0x67)
#define  DTMB_SYNC_FEC_DEBUG_CFG             DTMB_SYNC_ADDR(0x68)
#define  DTMB_SYNC_DATA_DDR_ADR              DTMB_SYNC_ADDR(0x69)
#define  DTMB_SYNC_DEBUG_DDR_ADR             DTMB_SYNC_ADDR(0x6a)
#define  DTMB_SYNC_FEC_SIM_CFG1              DTMB_SYNC_ADDR(0x6b)
#define  DTMB_SYNC_FEC_SIM_CFG2              DTMB_SYNC_ADDR(0x6c)
#define  DTMB_SYNC_TRACK_CFO_MAX             DTMB_SYNC_ADDR(0x6d)
#define  DTMB_SYNC_CCI_DAGC_CONFIG1          DTMB_SYNC_ADDR(0x6e)
#define  DTMB_SYNC_CCI_DAGC_CONFIG2          DTMB_SYNC_ADDR(0x6f)
#define  DTMB_SYNC_CCI_RP                    DTMB_SYNC_ADDR(0x70)
#define  DTMB_SYNC_CCI_DET_THRES             DTMB_SYNC_ADDR(0x71)
#define  DTMB_SYNC_CCI_NOTCH1_CONFIG1        DTMB_SYNC_ADDR(0x72)
#define  DTMB_SYNC_CCI_NOTCH1_CONFIG2        DTMB_SYNC_ADDR(0x73)
#define  DTMB_SYNC_CCI_NOTCH2_CONFIG1        DTMB_SYNC_ADDR(0x74)
#define  DTMB_SYNC_CCI_NOTCH2_CONFIG2        DTMB_SYNC_ADDR(0x75)

#endif
