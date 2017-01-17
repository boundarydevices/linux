/*
 * include/linux/amlogic/media/registers/regs/amrisc_regs.h
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

#ifndef AMRISC_REGS_HEADER_
#define AMRISC_REGS_HEADER_

#define MSP 0x0300
#define MPSR 0x0301
#define MINT_VEC_BASE 0x0302
#define MCPU_INTR_GRP 0x0303
#define MCPU_INTR_MSK 0x0304
#define MCPU_INTR_REQ 0x0305
#define MPC_P 0x0306
#define MPC_D 0x0307
#define MPC_E 0x0308
#define MPC_W 0x0309
#define CPSR 0x0321
#define CINT_VEC_BASE 0x0322
#define CPC_P 0x0326
#define CPC_D 0x0327
#define CPC_E 0x0328
#define CPC_W 0x0329
#define IMEM_DMA_CTRL 0x0340
#define IMEM_DMA_ADR 0x0341
#define IMEM_DMA_COUNT 0x0342
#define WRRSP_IMEM 0x0343
#define LMEM_DMA_CTRL 0x0350
#define LMEM_DMA_ADR 0x0351
#define LMEM_DMA_COUNT 0x0352
#define WRRSP_LMEM 0x0353
#define MAC_CTRL1 0x0360
#define ACC0REG1 0x0361
#define ACC1REG1 0x0362
#define MAC_CTRL2 0x0370
#define ACC0REG2 0x0371
#define ACC1REG2 0x0372
#define CPU_TRACE 0x0380

#endif
