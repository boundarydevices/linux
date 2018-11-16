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


struct vlock_regs_s {
	unsigned int addr;
	unsigned int val;
};

#define VLOCK_DEFAULT_REG_SIZE 20
static struct vlock_regs_s vlock_enc_setting[VLOCK_DEFAULT_REG_SIZE] = {
	/* optimize */
	{0x3000,     0xE3f50f10  },
	{0x3001,     0x41E3c3c   },
	{0x3002,     0x6000000   },
	{0x3003,     0x20709709/*0x20680680  */},
	{0x3004,     0x00709709/*0x280280    */},
	{0x3005,     0x8020000   },
	{0x3006,     0x0008000   },
	{0x3007,     0x0000000   },
	{0x3008,     0x0000000   },
	{0x3009,     0x6000000/*0x0008000   */},
	{0x300a,     0x8000000   },
	{0x300b,     0x000a000   },
	{0x300c,     0xa000000   },
	{0x300d,     0x0004000   },
	{0x3010,     0x20001000  },
	{0x3016,     0x18000     },
	{0x3017,     0x01080     },
	{0x301d,     0x30501080  },
	{0x301e,     0x7	 },
	{0x301f,     0x6000000   },
};
static struct vlock_regs_s vlock_pll_setting[VLOCK_DEFAULT_REG_SIZE] = {
	/* optimize */
	{0x3000,     0x07f13f1a   },
	{0x3001,     0x04053c32   },
	{0x3002,     0x06000000   },
	{0x3003,     0x20780780   },
	{0x3004,     0x00680680   },
	{0x3005,     0x00080000   },
	{0x3006,     0x00070000   },
	{0x3007,     0x00000000   },
	{0x3008,     0x00000000   },
	{0x3009,     0x00100000   },
	{0x300a,     0x00600000   },
	{0x300b,     0x00100000   },
	{0x300c,     0x00600000   },
	{0x300d,     0x00004000   },
	{0x3010,     0x20001000   },
	{0x3016,     0x0003de00   },
	{0x3017,     0x00001010   },
	{0x301d,     0x30501080   },
	{0x301e,     0x00000007   },
	{0x301f,     0x06000000   }
};

#define VLOCK_PHASE_REG_SIZE 9
static struct vlock_regs_s vlock_pll_phase_setting[VLOCK_PHASE_REG_SIZE] = {
	{0x3004,     0x00620680},
	{0x3009,	 0x06000000},
	{0x300a,	 0x00600000},
	{0x300b,	 0x06000000},
	{0x300c,	 0x00600000},

	{0x3025,	 0x00002000},
	{0x3027,	 0x00022002},
	{0x3028,	 0x00001000},
	{0x302a,	 0x00022002},
};

#endif

