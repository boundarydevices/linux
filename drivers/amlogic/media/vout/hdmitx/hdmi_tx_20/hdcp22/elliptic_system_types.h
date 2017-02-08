/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hdcp22/elliptic_system_types.h
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

#ifndef __ELLIPTIC_SYSTEM_TYPE_H__
#define __ELLIPTIC_SYSTEM_TYPE_H__

/**
 * \defgroup CALDef Common Definitions
 *
 * This section defines the common shared types
 *
 * \addtogroup CALDef
 * \{
 *
 */

/**
 * \defgroup SysTypes System Types
 *
 * This section defines the common SHARED SYSTEM types
 *
 * \addtogroup SysTypes
 * \{
 *
 */

/* System types definitions. */
#define ELP_STATUS                      int16_t
#define ELP_SYSTEM_INFO                 int16_t
#define ELP_SYSTEM_BIG_ENDIAN           0
#define ELP_SYSTEM_LITLLE_ENDIAN        1

/* PRNG definitions. */
#define PRINT_FUNC      printf_ptr
#define PRINT_FUNC_DECL int32_t (*PRINT_FUNC)(const void *str, ...)
#define PRNG_FUNC       prng
#define PRNG_FUNC2      prng2
#define PRNG_FUNC_DECL  uint8_t (*PRNG_FUNC)(void *, void *, uint8_t)
#define PRNG_FUNC2_DECL uint32_t (*PRNG_FUNC2)(void *, void *, uint32_t)

struct elp_std_prng_info {
	void *prng_inst;

	PRNG_FUNC_DECL;
};

struct elp_std_prng_info2 {
	void *prng_inst;

	PRNG_FUNC2_DECL;
};

#define ELP_STD_PRNG_INFO  elp_std_prng_info
#define ELP_STD_PRNG_INFO2 elp_std_prng_info2

/**
 * \}
 */

/**
 * \}
 */

#endif

