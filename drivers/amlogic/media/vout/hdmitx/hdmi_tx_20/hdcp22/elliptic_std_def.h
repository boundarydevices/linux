/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hdcp22/elliptic_std_def.h
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

#ifndef __ELLIPTIC_STD_DEF_H__
#define __ELLIPTIC_STD_DEF_H__

#ifndef __KERNEL__
/**
 * \defgroup CALDef Common Definitions
 *
 * This section defines the common shared types
 *
 * \addtogroup CALDef
 * @{
 *
 */

/**
 * \defgroup SysStdTypes Standard Types
 * This section defines the common SHARED STANDARD types
 * \addtogroup SysStdTypes
 * @{
 *
 */

/**
 * \ingroup SysStdTypes Standard Types
 * include <stddef.h>\n
 * > - C89 compliant: if this is not available then add your definition here
 *
 */
 /* C89 compliant - if this is not available then add your definition here */
#include <stddef.h>
/**
 * \ingroup SysStdTypes Standard Types
 * include <stdint.h>\n
 * > - C99 compliant: if this is not available then add your definition here
 *
 */
 /* C99 compliant - if this is not available then add your definition here */
#include <stdint.h>
/**
 * \ingroup SysStdTypes Standard Types
 * include <stdarg.h>\n
 * > - C89 compliant: if this is not available then add your definition here
 *
 */
/* C89 compliant -  if this is not available then add your definition here */
#include <stdarg.h>

/**
 * @}
 */
/**
 * @}
 */

#else
#include <linux/types.h>
#include <linux/kernel.h>
#endif

#endif /* __ELLIPTIC_STD_DEF_H__ */

