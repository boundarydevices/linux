/*
 * drivers/amlogic/media/dtv_demod/include/addr_dtmb_che.h
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

#ifndef __ADDR_DTMB_CHE_H__
#define __ADDR_DTMB_CHE_H__

/*#include "addr_dtmb_top.h"*/

#define  DTMB_CHE_ADDR(x) (DTMB_DEMOD_BASE + (x << 2))

#define  DTMB_CHE_IBDFE_CONF0               DTMB_CHE_ADDR(0x8b)
#define  DTMB_CHE_TE_HREB_SNR               DTMB_CHE_ADDR(0x8d)
#define  DTMB_CHE_MC_SC_TIMING_POWTHR       DTMB_CHE_ADDR(0x8e)
#define  DTMB_CHE_MC_SC_PROTECT_GD          DTMB_CHE_ADDR(0x8f)
#define  DTMB_CHE_TIMING_LIMIT              DTMB_CHE_ADDR(0x90)
#define  DTMB_CHE_TPS_CONFIG                DTMB_CHE_ADDR(0x91)
#define  DTMB_CHE_FD_TD_STEPSIZE            DTMB_CHE_ADDR(0x92)
#define  DTMB_CHE_QSTEP_SET                 DTMB_CHE_ADDR(0x93)
#define  DTMB_CHE_SEG_CONFIG                DTMB_CHE_ADDR(0x94)
#define  DTMB_CHE_FD_TD_LEAKSIZE_CONFIG1    DTMB_CHE_ADDR(0x95)
#define  DTMB_CHE_FD_TD_LEAKSIZE_CONFIG2    DTMB_CHE_ADDR(0x96)
#define  DTMB_CHE_FD_TD_COEFF               DTMB_CHE_ADDR(0x97)
#define  DTMB_CHE_M_CCI_THR_CONFIG1         DTMB_CHE_ADDR(0x98)
#define  DTMB_CHE_M_CCI_THR_CONFIG2         DTMB_CHE_ADDR(0x99)
#define  DTMB_CHE_M_CCI_THR_CONFIG3         DTMB_CHE_ADDR(0x9a)
#define  DTMB_CHE_CCIDET_CONFIG             DTMB_CHE_ADDR(0x9b)
#define  DTMB_CHE_IBDFE_CONFIG1             DTMB_CHE_ADDR(0x9d)
#define  DTMB_CHE_IBDFE_CONFIG2             DTMB_CHE_ADDR(0x9e)
#define  DTMB_CHE_IBDFE_CONFIG3             DTMB_CHE_ADDR(0x9f)
#define  DTMB_CHE_TD_COEFF                  DTMB_CHE_ADDR(0xa0)
#define  DTMB_CHE_FD_TD_STEPSIZE_ADJ        DTMB_CHE_ADDR(0xa1)
#define  DTMB_CHE_FD_COEFF_FRZ              DTMB_CHE_ADDR(0xa2)
#define  DTMB_CHE_FD_COEFF                  DTMB_CHE_ADDR(0xa3)
#define  DTMB_CHE_FD_LEAKSIZE               DTMB_CHE_ADDR(0xa4)
#define  DTMB_CHE_IBDFE_CONFIG4             DTMB_CHE_ADDR(0xa5)
#define  DTMB_CHE_IBDFE_CONFIG5             DTMB_CHE_ADDR(0xa6)
#define  DTMB_CHE_IBDFE_CONFIG6             DTMB_CHE_ADDR(0xa7)
#define  DTMB_CHE_IBDFE_CONFIG7             DTMB_CHE_ADDR(0xa8)
#define  DTMB_CHE_DCM_SC_MC_GD_LEN          DTMB_CHE_ADDR(0xa9)
#define  DTMB_CHE_EQMC_PICK_THR             DTMB_CHE_ADDR(0xaa)
#define  DTMB_CHE_EQMC_THRESHOLD            DTMB_CHE_ADDR(0xab)
#define  DTMB_CHE_EQSC_PICK_THR             DTMB_CHE_ADDR(0xad)
#define  DTMB_CHE_EQSC_THRESHOLD            DTMB_CHE_ADDR(0xae)
#define  DTMB_CHE_PROTECT_GD_TPS            DTMB_CHE_ADDR(0xaf)
#define  DTMB_CHE_FD_TD_STEPSIZE_THR1       DTMB_CHE_ADDR(0xb0)
#define  DTMB_CHE_TDFD_SWITCH_SYM1          DTMB_CHE_ADDR(0xb1)
#define  DTMB_CHE_TDFD_SWITCH_SYM2          DTMB_CHE_ADDR(0xb2)
#define  DTMB_CHE_EQ_CONFIG                 DTMB_CHE_ADDR(0xb3)
#define  DTMB_CHE_EQSC_SNR_IMP_THR1         DTMB_CHE_ADDR(0xb4)
#define  DTMB_CHE_EQSC_SNR_IMP_THR2         DTMB_CHE_ADDR(0xb5)
#define  DTMB_CHE_EQMC_SNR_IMP_THR1         DTMB_CHE_ADDR(0xb6)
#define  DTMB_CHE_EQMC_SNR_IMP_THR2         DTMB_CHE_ADDR(0xb7)
#define  DTMB_CHE_EQSC_SNR_DROP_THR         DTMB_CHE_ADDR(0xb8)
#define  DTMB_CHE_EQMC_SNR_DROP_THR         DTMB_CHE_ADDR(0xb9)
#define  DTMB_CHE_M_CCI_THR                 DTMB_CHE_ADDR(0xba)
#define  DTMB_CHE_TPS_MC                    DTMB_CHE_ADDR(0xbb)
#define  DTMB_CHE_TPS_SC                    DTMB_CHE_ADDR(0xbc)
#define  DTMB_CHE_CHE_SET_FSM               DTMB_CHE_ADDR(0xbd)
#define  DTMB_CHE_ZERO_NUM_THR              DTMB_CHE_ADDR(0xbe)
#define  DTMB_CHE_TIMING_READY              DTMB_CHE_ADDR(0xbf)

#endif
