#ifndef __LINUX_MFD_DA9052_DEV_H
#define __LINUX_MFD_DA9052_DEV_H
/*
 * da9052 device interface specification
 *
 * Copyright(c) 2011 Boundary Devices
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#define BASE_MAGIC 0xDA

typedef unsigned da90_reg_and_value_t ;

#define GETREGNUM(rnv) (rnv&0xFF)
#define GETVAL(rnv) ((rnv>>8)&0xFF)

#define MAKERNV(regnum,val) ((regnum&0xFF)|((val&0xFF)<<8))

#define DA905X_GETREG _IOW(BASE_MAGIC, 0x01, da90_reg_and_value_t)
#define DA905X_SETREG _IOR(BASE_MAGIC, 0x01, da90_reg_and_value_t)

#endif
