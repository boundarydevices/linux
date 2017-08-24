/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#ifndef SII_IIC_DEFS
#define SII_IIC_DEFS


#define IIC_WRITE 0x0
#define IIC_READ 0x01
#define ALIGN24C02 0x07   /* 8 bytes page */
#define ALIGN24C32 0x1f   /* 32 bytes page */
#define ALIGN24C264 0x3f  /* 64 bytes page */


#define I2CGetByte()     _I2CGetByte(0)
#define I2CGetLastByte() _I2CGetByte(1)
#define DDC_I2CGetByte()     DDC__I2CGetByte(0)
#define DDC_I2CGetLastByte() DDC__I2CGetByte(1)


#endif

