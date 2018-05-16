/*
 * drivers/amlogic/media/enhancement/amvecm/amvecm_vlock_regmap.h
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

#ifndef __AMVECM_VLOCK_REGMAP_H
#define __AMVECM_VLOCK_REGMAP_H

#include <linux/amlogic/media/amvecm/cm.h>

static struct am_regs_s vlock_enc_setting = {
	20,
	{
	/* optimize */
	{REG_TYPE_VCBUS, 0x3000,     0xffffffff, 0xE3f50f10  },
	{REG_TYPE_VCBUS, 0x3001,     0xffffffff, 0x41E3c3c   },
	{REG_TYPE_VCBUS, 0x3002,     0xffffffff, 0x6000000   },
	{REG_TYPE_VCBUS, 0x3003,     0xffffffff, 0x20680680  },
	{REG_TYPE_VCBUS, 0x3004,     0xffffffff, 0x280280    },
	{REG_TYPE_VCBUS, 0x3005,     0xffffffff, 0x8020000   },
	{REG_TYPE_VCBUS, 0x3006,     0xffffffff, 0x0008000   },
	{REG_TYPE_VCBUS, 0x3007,     0xffffffff, 0x0000000   },
	{REG_TYPE_VCBUS, 0x3008,     0xffffffff, 0x0000000   },
	{REG_TYPE_VCBUS, 0x3009,     0xffffffff, 0x0008000   },
	{REG_TYPE_VCBUS, 0x300a,     0xffffffff, 0x8000000   },
	{REG_TYPE_VCBUS, 0x300b,     0xffffffff, 0x000a000   },
	{REG_TYPE_VCBUS, 0x300c,     0xffffffff, 0xa000000   },
	{REG_TYPE_VCBUS, 0x300d,     0xffffffff, 0x0004000   },
	{REG_TYPE_VCBUS, 0x3010,     0xffffffff, 0x20001000  },
	{REG_TYPE_VCBUS, 0x3016,     0xffffffff, 0x18000     },
	{REG_TYPE_VCBUS, 0x3017,     0xffffffff, 0x01080     },
	{REG_TYPE_VCBUS, 0x301d,     0xffffffff, 0x30501080  },
	{REG_TYPE_VCBUS, 0x301e,     0xffffffff, 0x7	    },
	{REG_TYPE_VCBUS, 0x301f,     0xffffffff, 0x6000000   },
	{0}
	}
};
static struct am_regs_s vlock_pll_setting = {
	20,
	{
	/* optimize */
	{REG_TYPE_VCBUS, 0x3000,     0xffffffff, 0x07f13f1a   },
	{REG_TYPE_VCBUS, 0x3001,     0xffffffff, 0x04053c32   },
	{REG_TYPE_VCBUS, 0x3002,     0xffffffff, 0x06000000   },
	{REG_TYPE_VCBUS, 0x3003,     0xffffffff, 0x20780780   },
	{REG_TYPE_VCBUS, 0x3004,     0xffffffff, 0x00000000   },
	{REG_TYPE_VCBUS, 0x3005,     0xffffffff, 0x00080000   },
	{REG_TYPE_VCBUS, 0x3006,     0xffffffff, 0x00070000   },
	{REG_TYPE_VCBUS, 0x3007,     0xffffffff, 0x00000000   },
	{REG_TYPE_VCBUS, 0x3008,     0xffffffff, 0x00000000   },
	{REG_TYPE_VCBUS, 0x3009,     0xffffffff, 0x00100000   },
	{REG_TYPE_VCBUS, 0x300a,     0xffffffff, 0x00008000   },
	{REG_TYPE_VCBUS, 0x300b,     0xffffffff, 0x00100000   },
	{REG_TYPE_VCBUS, 0x300c,     0xffffffff, 0x00000000   },
	{REG_TYPE_VCBUS, 0x300d,     0xffffffff, 0x00004000   },
	{REG_TYPE_VCBUS, 0x3010,     0xffffffff, 0x20001000   },
	{REG_TYPE_VCBUS, 0x3016,     0xffffffff, 0x0003de00   },
	{REG_TYPE_VCBUS, 0x3017,     0xffffffff, 0x00001080   },
	{REG_TYPE_VCBUS, 0x301d,     0xffffffff, 0x30501080   },
	{REG_TYPE_VCBUS, 0x301e,     0xffffffff, 0x00000007   },
	{REG_TYPE_VCBUS, 0x301f,     0xffffffff, 0x06000000   },
	{0}
	}
};

#endif

