/*------------------------------------------------------------------------------
 * Module Name: SiIHLVIIC
 *
 * Module Description:  high level i2c routines
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */
#include "SiIHLVIIC.h"
/* #include "SiISW_IIC.h" */
#include "SiIIICDefs.h"
#include "SiIHAL.h"
#include "../platform_iface.h"


/*------------------------------------------------------------------------ */
BOOL hlWaitForAck(BYTE bSlvAddr, WORD wTimeOut)
{
#if 0
	BOOL bResult = FALSE;
	int ret = 0;
	char value = 0x00;

	ret = __plat_i2c_write_block((bSlvAddr >> 1), 0x00, &value, 1);
	if (ret == 0)
		bResult = TRUE;

	return bResult;
#else
	return TRUE;
#endif
}


/*-------------------------------------------------------------------------- */
BYTE hlBlockRead_8BAS(struct I2CShortCommandType_s *IIC, BYTE *Data)
{
	int ret = 0;

	ret = __plat_i2c_read_block((IIC->SlaveAddr >> 1),
		IIC->RegAddrL, Data, IIC->NBytes);

	ret = (ret >= 0 ? IIC_OK : IIC_ERROR);
	return ret;
}

/*--------------------------------------------------------------------------- */

BYTE hlBlockWrite_8BAS(struct I2CShortCommandType_s *IIC, BYTE *Data)
{
	int ret = 0;

	ret = __plat_i2c_write_block((IIC->SlaveAddr >> 1),
		IIC->RegAddrL, Data, IIC->NBytes);

	ret = (ret >= 0 ? IIC_OK : IIC_ERROR);
	return ret;

}
/*------------------------------------------------------------------- */
BYTE hlReadByte_8BA(BYTE SlaveAddr, BYTE RegAddr)
{
	int ret = 0;
	unsigned char data = 0;

	ret = __plat_i2c_read_block((SlaveAddr >> 1), RegAddr, &data, 1);
	if (ret >= 0)
		return data;

	return 0;
}

/*-------------------------------------------------------------------- */

WORD hlReadWord_8BA(BYTE SlaveAddr, BYTE RegAddr)
{
	int ret = 0;
	unsigned char data[2] = {0, 0};
	WORD tmp;

	ret = __plat_i2c_read_block((SlaveAddr >> 1),
		RegAddr, (char *)&data, 2);
	if (ret >= 0) {
		tmp = ((data[1] << 8) | data[0]);
		return tmp;
	}

	return 0;
}


/*------------------------------------------------------------------- */
void hlWriteByte_8BA(BYTE SlaveAddr, BYTE RegAddr, BYTE Data)
{
	int ret = 0;

	ret = __plat_i2c_write_block((SlaveAddr >> 1), RegAddr, &Data, 1);
}
/*------------------------------------------------------------------- */

void hlWriteWord_8BA(BYTE SlaveAddr, BYTE RegAddr, WORD Data)
{
	int ret = 0;
	unsigned char buf[2] = {0, 0};

	buf[0] = Data & 0xff;
	buf[1] = Data >> 8;

	ret = __plat_i2c_write_block((SlaveAddr >> 1), RegAddr, buf, 2);
}


