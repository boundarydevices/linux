/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include "SiICmplDefs.h"
#ifndef _TYPEDEFS_
#define _TYPEDEFS_


#define BYTE    unsigned char
#define U8BIT   unsigned char
#define WORD    unsigned short
#define U16BIT  unsigned short
#define U32BIT  unsigned int
#define DWORD   unsigned int

#ifdef _8051_   /* for 8051 family of microcontrollers */

#define ROM   code   /* 8051 type of ROM memory */
#define IRAM  idata  /* 8051 type of RAM memory */

#define BOOL   bit

#else

#define BOOL   unsigned char

#define ROM
#define IRAM

#endif

#define FALSE  0
#define TRUE  1

#define ABSENT   0
#define PRESENT  1

/*#define USE_InternEEPROM  // enable InternEEPROM */

#endif


