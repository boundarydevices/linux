/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#ifndef _TRACE_
#define _TRACE_

#include "SiITypeDefs.h"

#define siiGetNumberOfWarnings() SiI_bWCode[0]

void siiWarningHandler(BYTE bWrnCode);
void siiErrorHandler(BYTE bErrorCode);
void siiResetErrorsAndWarnings(void);
BYTE siiGetErrorsWarnings(void);
BYTE siiGetErrorsData(BYTE *pbErrData);
BYTE siiGetWarningData(BYTE *pbWrnData);
#endif

