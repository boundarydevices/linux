/*
 * drivers/amlogic/usb/dwc_otg/310/common_port/dwc_modpow.h
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

#ifndef _DWC_MODPOW_H
#define _DWC_MODPOW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dwc_os.h"

/** @file
 *
 * This file defines the module exponentiation function which is only used
 * internally by the DWC UWB modules for calculation of PKs during numeric
 * association.  The routine is taken from the PUTTY, an open source terminal
 * emulator.  The PUTTY License is preserved in the dwc_modpow.c file.
 *
 */

typedef uint32_t BignumInt;
typedef uint64_t BignumDblInt;
typedef BignumInt *Bignum;

/* Compute modular exponentiaion */
extern Bignum dwc_modpow(void *mem_ctx, Bignum base_in, Bignum exp, Bignum mod);

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_BIGNUM_H */
