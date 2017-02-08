/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hdcp22/ESMHostLibDriverErrors.h
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

#ifndef _ESMDRIVERERROR_H_
#define _ESMDRIVERERROR_H_

/**
 * \defgroup HostLibErrors General Library Errors
 *
 * The following are error code definitions produced
 * by the API library.
 *
 * \addtogroup HostLibErrors
 * @{
 *
 */

#define ESM_HL_DRIVER_SUCCESS                0
#define ESM_HL_DRIVER_FAILED               (-1)
#define ESM_HL_DRIVER_NO_MEMORY            (-2)
#define ESM_HL_DRIVER_NO_ACCESS            (-3)
#define ESM_HL_DRIVER_INVALID_PARAM        (-4)
#define ESM_HL_DRIVER_TOO_MANY_ESM_DEVICES (-5)
#define ESM_HL_DRIVER_USER_DEFINED_ERROR   (-6)
/* anything beyond this error code is user defined */

/* End of APIErrors group */
/**
 * @}
 */
/**
 * @}
 */

#endif
