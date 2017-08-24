/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

void siiGetRXDeviceInfo(BYTE *pbChipVer);
BOOL siiIsHDMI_Mode(void);
BOOL siiInitializeRX(BYTE *pbInit);
void siiRX_PowerDown(BYTE bPowerDown);
void siiRX_GlobalPower(BYTE bPowerState);
void siiSetMasterClock(BYTE bDividerIndex);

BOOL siiRX_CheckCableHPD(void);
void siiClearBCHCounter(void);
void siiRX_DisableTMDSCores(void);
void sii_SetVideoOutputPowerDown(BYTE bVideoOutputPowerDown);
void siiSetAutoSWReset(BOOL qOn);
void siiRXHardwareReset(void);
void siiSetAFEClockDelay(void);
BOOL siiCheckSupportDeepColorMode(void);

void siiSetHBRFs(BOOL qON);
void siiSetNormalTerminationValueCh1(BOOL qOn);
void siiSetNormalTerminationValueCh2(BOOL qOn);

