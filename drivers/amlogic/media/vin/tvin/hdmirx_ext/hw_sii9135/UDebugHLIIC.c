/*------------------------------------------------------------------------------
 * Module Name: UDebugHLIIC
 *
 * Module Description: contains IIC functions which used outside of API
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */
#include "UDebugHLIIC.h"
/*#include "SiISW_IIC.h"*/
#include "SiIIICDefs.h"
#include "SiIHAL.h"

/*---------------------------------------------------------------------------*/
#ifdef _BIGEEPROM_
#ifdef _BLOCK_16BA
BYTE BlockWrite_16BA(I2CCommandType *I2CCommand)
{
	BYTE i, bState;

	if (!(I2CCommand->Flags & FLG_CONTD)) {
		bState = GetI2CState();
		if (bState)
			return IIC_CAPTURED;
		bState = I2CSendAddr(I2CCommand->SlaveAddr, IIC_WRITE);
		if (bState) {
			I2CSendStop();
			return IIC_NOACK;
		}
		I2CSendByte(I2CCommand->RegAddrH);
		I2CSendByte(I2CCommand->RegAddrL);
	}
	for (i = 0; i < I2CCommand->NBytes; i++) {
		I2CSendByte((BYTE)(I2CCommand->Data[i]));
		I2CCommand->RegAddrL++;
		if (!I2CCommand->RegAddrL) {
			I2CCommand->RegAddrH++;
			if ((I2CCommand->RegAddrL & ALIGN24C264) == 0) {
				I2CSendStop();
				I2CSendAddr(I2CCommand->SlaveAddr, IIC_WRITE);
				I2CSendByte(I2CCommand->RegAddrH);
				I2CSendByte(I2CCommand->RegAddrL);
			}
		}
	}
	if (!(I2CCommand->Flags & FLG_NOSTOP))
		I2CSendStop();

	return IIC_OK;

}
//------------------------------------------------------------------------------
BYTE BlockRead_16BA(I2CCommandType *I2CCommand)
{
	BYTE i, bState;

	if (!(I2CCommand->Flags & FLG_CONTD)) {
		bState = GetI2CState();
		if (bState)
			return IIC_CAPTURED;
		bState = I2CSendAddr(I2CCommand->SlaveAddr, IIC_WRITE);
		if (bState) {
			I2CSendStop();
			return IIC_NOACK;
		}
		I2CSendByte(I2CCommand->RegAddrH);
		I2CSendByte(I2CCommand->RegAddrL);
		I2CSendStop();
	}
	I2CSendAddr(I2CCommand->SlaveAddr, IIC_READ);
	for (i = 0; i < I2CCommand->NBytes-1; i++)
		I2CCommand->Data[i]  = I2CGetByte();
	if (I2CCommand->Flags & FLG_NOSTOP) {
		I2CCommand->Data[i] = I2CGetByte();
	} else {
		I2CCommand->Data[i] = I2CGetLastByte();
		I2CSendStop();
	}
	return IIC_OK;

}

//-----------------------------------------------------------------------------


BYTE BlockRead_16BAS(struct I2CShortCommandType_s *IIC, BYTE *Data)
{
	BYTE i, bState;

	if (!(IIC->Flags & FLG_CONTD)) {
		bState = GetI2CState();
		if (bState)
			return IIC_CAPTURED;
		bState = I2CSendAddr(IIC->SlaveAddr, IIC_WRITE);
		if (bState) {
			I2CSendStop();
			return IIC_NOACK;
		}
		bState = I2CSendByte(IIC->RegAddrH);
		if (bState) {
			I2CSendStop();
			return IIC_NOACK;
		}
		I2CSendByte(IIC->RegAddrL);
		bState = I2CSendAddr(IIC->SlaveAddr, IIC_READ);
		if (bState) {
			I2CSendStop();
			return IIC_NOACK;
		}
	}
	for (i = 0; i < IIC->NBytes - 1; i++)
		Data[i] = I2CGetByte();
	if (IIC->Flags & FLG_NOSTOP) {
		Data[i] = I2CGetByte();
	} else {
		Data[i] = I2CGetLastByte();
		I2CSendStop();
	}
	return IIC_OK;

}

//-----------------------------------------------------------------------------

BYTE BlockWrite_16BAS(struct I2CShortCommandType_s *IIC, BYTE *Data)
{
	BYTE i, bState;

	if (!(IIC->Flags & FLG_CONTD)) {
		bState = GetI2CState();
		if (bState)
			return IIC_CAPTURED;
		bState = I2CSendAddr(IIC->SlaveAddr, IIC_WRITE);
		if (bState) {
			I2CSendStop();
			return IIC_NOACK;
		}
		bState = I2CSendByte(IIC->RegAddrH);
		if (bState) {
			I2CSendStop();
			return IIC_NOACK;
		}
		I2CSendByte(IIC->RegAddrL);
	}
	for (i = 0; i < IIC->NBytes; i++)
		I2CSendByte(Data[i]);
	if (!(IIC->Flags & FLG_NOSTOP))
		I2CSendStop();
	return IIC_OK;

}
#endif
#ifdef _DEAD_CODE_
/*-----------------------------------------------------------------------*/
void WriteWord_16BA(BYTE SlaveAddr, WORD RegAddr, WORD Data)
{
	I2CSendAddr(SlaveAddr, IIC_WRITE);
	I2CSendByte((BYTE)(RegAddr >> 8));
	I2CSendByte((BYTE)(RegAddr & 0xFF));
	I2CSendByte((BYTE)(Data & 0xFF));
	I2CSendByte((BYTE)(Data >> 8));
	I2CSendStop();

}

/*-----------------------------------------------------------------------*/
void WriteByte_16BA(BYTE SlaveAddr, WORD RegAddr, BYTE Data)
{
	I2CSendAddr(SlaveAddr, IIC_WRITE);
	I2CSendByte((BYTE)(RegAddr >> 8));
	I2CSendByte((BYTE)(RegAddr & 0xFF));
	I2CSendByte(Data);
	I2CSendStop();

}

/*-------------------------------------------------------------------*/
WORD ReadWord_16BA(BYTE SlaveAddr, WORD RegAddr)
{
	WORD Data;

	I2CSendAddr(SlaveAddr, IIC_WRITE);
	I2CSendByte((BYTE)RegAddr >> 8);
	I2CSendByte((BYTE)(RegAddr & 0xff));
	I2CSendStop();
	I2CSendAddr(SlaveAddr, IIC_READ);
	Data = I2CGetByte();
	Data |= (I2CGetLastByte() << 8);
	I2CSendStop();
	return Data;

}

/*-------------------------------------------------------------------*/
BYTE ReadByte_16BA(BYTE SlaveAddr, WORD RegAddr)
{
	BYTE Data;

	I2CSendAddr(SlaveAddr, IIC_WRITE);
	I2CSendByte((BYTE)RegAddr >> 8);
	I2CSendByte((BYTE)(RegAddr & 0xff));
	I2CSendStop();
	I2CSendAddr(SlaveAddr, IIC_READ);
	Data = I2CGetLastByte();
	I2CSendStop();
	return Data;

}
#endif

#endif /*  _end BIGEEPROM_ */
/*---------------------------------------------------------------------------*/
#ifdef _BLOCK_8BA
BYTE BlockRead_8BA(I2CCommandType *I2CCommand)
{
	BYTE i, bState;

	if (!(I2CCommand->Flags & FLG_CONTD)) {
		bState = GetI2CState();
		if (bState)
			return IIC_CAPTURED;
		bState = I2CSendAddr(I2CCommand->SlaveAddr, IIC_WRITE);
		if (bState) {
			I2CSendStop();
			return IIC_NOACK;
		}
		bState = I2CSendByte(I2CCommand->RegAddrL);
		if (bState) {
			I2CSendStop();
			return IIC_NOACK;
		}
		bState = I2CSendAddr(I2CCommand->SlaveAddr, IIC_READ);
		if (bState) {
			I2CSendStop();
			return IIC_NOACK;
		}
	}
	for (i = 0; i < I2CCommand->NBytes-1; i++)
		I2CCommand->Data[i] = I2CGetByte();
	if (I2CCommand->Flags & FLG_NOSTOP) {
		I2CCommand->Data[i] = I2CGetByte();
	} else {
		I2CCommand->Data[i] = I2CGetLastByte();
		I2CSendStop();
	}
	return IIC_OK;

}

//-----------------------------------------------------------------------------
BYTE BlockWrite_8BA(I2CCommandType *I2CCommand)
{
	BYTE i, bState;

	if (!(I2CCommand->Flags & FLG_CONTD)) {
		bState = GetI2CState();
		if (bState)
			return IIC_CAPTURED;
		bState = I2CSendAddr(I2CCommand->SlaveAddr, IIC_WRITE);
		if (bState) {
			I2CSendStop();
			return IIC_NOACK;
		}
		bState = I2CSendByte(I2CCommand->RegAddrL);
		if (bState) {
			I2CSendStop();
			return IIC_NOACK;
		}
	}
	for (i = 0; i < I2CCommand->NBytes; i++)
		I2CSendByte(I2CCommand->Data[i]);
	if (!(I2CCommand->Flags & FLG_NOSTOP))
		I2CSendStop();
	return IIC_OK;

}

//---------------------------------------------------------------------------
BYTE DoRecoverySCLs(void)
{
	MakeSCLPulses(9);
	return GetI2CStatus();

}
#endif


