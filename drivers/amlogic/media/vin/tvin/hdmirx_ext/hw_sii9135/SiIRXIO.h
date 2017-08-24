/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#ifndef _SII_RXIO_
#define _SII_RXIO_

#include "SiIHLVIIC.h"


/* HDMI Receiver */
#define siiIIC_RX_RWBitsInByteP0(ADDR, DATA, OP)  \
		siiModifyBits(RX_SLV0, ADDR, DATA, OP)
#define siiIIC_RX_RWBitsInByteP1(ADDR, DATA, OP)  \
		siiModifyBits(RX_SLV1, ADDR, DATA, OP)

#define siiWriteByteHDMIRXP0(ADDR, DATA) hlWriteByte_8BA(RX_SLV0, ADDR, DATA)
#define siiReadByteHDMIRXP0(ADDR) hlReadByte_8BA(RX_SLV0, ADDR)
#define siiReadWordHDMIRXP0(ADDR) hlReadWord_8BA(RX_SLV0, ADDR)
#define siiWriteWordHDMIRXP0(ADDR, DATA) hlWriteWord_8BA(RX_SLV0, ADDR, DATA)

#define siiWriteByteHDMIRXP1(ADDR, DATA) hlWriteByte_8BA(RX_SLV1, ADDR, DATA)
#define siiReadByteHDMIRXP1(ADDR) hlReadByte_8BA(RX_SLV1, ADDR)
#define siiReadWordHDMIRXP1(ADDR) hlReadWord_8BA(RX_SLV1, ADDR)
#define siiWriteWordHDMIRXP1(ADDR, DATA) hlWriteWord_8BA(RX_SLV1, ADDR, DATA)

#define siiWaitForAckHDMIRX(TO) hlWaitForAck(RX_SLV0, TO)


/* Analog Front End */
#define siiIIC_RX_RWBitsInByteU0(ADDR, DATA, OP)  \
		siiModifyBits(RX_AFE0, ADDR, DATA, OP)
#define siiIIC_RX_RWBitsInByteU1(ADDR, DATA, OP)  \
		siiModifyBits(RX_AFE1, ADDR, DATA, OP)

#define siiWriteByteAFEU0(ADDR, DATA) hlWriteByte_8BA(RX_AFE0, ADDR, DATA)
#define siiReadByteAFEU0(ADDR) hlReadByte_8BA(RX_AFE0, ADDR)
#define siiReadWordAFEU0(ADDR) hlReadWord_8BA(RX_AFE0, ADDR)
#define siiWriteWordAFEU0(ADDR, DATA) hlWriteWord_8BA(RX_AFE0, ADDR, DATA)

#define siiWriteByteAFEU1(ADDR, DATA) hlWriteByte_8BA(RX_AFE1, ADDR, DATA)
#define siiReadByteAFEU1(ADDR) hlReadByte_8BA(RX_AFE1, ADDR)
#define siiReadWordAFEU1(ADDR) hlReadWord_8BA(RX_AFE1, ADDR)
#define siiWriteWordAFEU1(ADDR, DATA) hlWriteWord_8BA(RX_AFE1, ADDR, DATA)

#define siiWaitForAckAFE(TO) hlWaitForAck(RX_AFE0, TO)


void siiModifyBits(BYTE bSlaveAddr, BYTE bRegAddr, BYTE bModVal, BOOL qSet);
void siiReadBlockHDMIRXP0(BYTE bAddr, BYTE bNBytes, BYTE *pbData);
void siiReadBlockHDMIRXP1(BYTE bAddr, BYTE bNBytes, BYTE *pbData);
void siiWriteBlockHDMIRXP0(BYTE bAddr, BYTE bNBytes, BYTE *pbData);
void siiWriteBlockHDMIRXP1(BYTE bAddr, BYTE bNBytes, BYTE *pbData);


#define siiIIC_RX_RMW_ByteP0(ADDR, MASK, VAL)  \
		siiReadModWriteByte(RX_SLV0, ADDR, MASK, VAL)
#define siiIIC_RX_RWW_ByteP1(ADDR, MASK, VAL)  \
		siiReadModWriteByte(RX_SLV1, ADDR, MASK, VAL)

void siiReadModWriteByte(BYTE bSlaveAddr, BYTE bRegAddr,
		BYTE bBitMask, BYTE bNewValue);


#endif

