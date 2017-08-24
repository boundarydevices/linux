/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#ifndef _SII_TTASKS_
#define _SII_TTASKS_
enum SiI_RX_TTasks {

	SiI_RX_TT_Video,
	SiI_RX_TT_Audio,
	SiI_RX_TT_InfoFrmPkt,
	SiI_RX_TT_Sys,
	SiI_RX_TT_Last
};

BYTE siiTimerTasksHandler(BYTE bSlotTime);
void siiInitACPCheckTimeOut(void);
#endif


