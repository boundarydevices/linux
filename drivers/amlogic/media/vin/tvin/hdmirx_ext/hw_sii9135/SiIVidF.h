/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#include "SiITypeDefs.h"
#include "SiIRXAPIDefs.h"

enum SiI_PixPrepl {

	SiI_PixRepl1 = 0x00,
	SiI_PixRepl2 = 0x01,
	SiI_PixRepl4 = 0x03

};

BYTE siiGetVideoFormatData(BYTE *pbVidFormatData);
BYTE siiSetVideoFormatData(BYTE *pbVidFormatData);
BYTE siiPrepVideoPathSelect(BYTE bVideoPathSelect, BYTE bInputColorSpace,
		BYTE *pbVidFormatData);
BYTE siiPrepSyncSelect(BYTE bSyncSelect, BYTE *pbVidFormatData);
BYTE siiPrepSyncCtrl(BYTE bSyncCtrl, BYTE *pbVidFormatData);
BYTE siiPrepVideoCtrl(BYTE bVideoOutCtrl, BYTE *pbVidFormatData);

void siiSetVidResDependentVidPath(BYTE bPixRepl, BYTE bVideoPath);
void siiSetVideoPathColorSpaceDependent(BYTE bVideoPath, BYTE bInputColorSpace);
/* void siiSetStaticVideoPath(BYTE bdata1, BYTE bdata2); */
void siiMuteVideo(BYTE On);

