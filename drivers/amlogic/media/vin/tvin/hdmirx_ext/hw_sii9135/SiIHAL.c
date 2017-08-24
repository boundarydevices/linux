/*------------------------------------------------------------------------------
 * Module Name:  SiIHAL
 * Module Description: MCU (CPU) Hardware dependent functions have been
 *                     placed here
 *
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */
#include "SiIHAL.h"
#include "SiIRXAPIDefs.h"
#include "SiIGlob.h"
#include "UGlob.h"
#include "../platform_iface.h"

WORD wScaleMS;


/*------------------------------------------------------------------------------
 * Function Name: InitGPIO_Pins
 * Function Description: Configure GPIOs
 *
 * Accepts: none
 * Returns: none
 * Globals: none
 *----------------------------------------------------------------------------
 */

void halInitGPIO_Pins(void)
{
	halPowerDownAudioDAC();
	/* YMA Clear is to activate. */
	halSetHPD1Pin();
	halSetHPD2Pin();

	/* halClearHPD1Pin(); */
	/* halClearHPD2Pin(); */
}
/*------------------------------------------------------------------------------
 * Function Name:  halDelayMS
 * Function Description: makes Delay in MS
 *
 * Accepts: BYTE, number of milliseconds to be delayed
 * Returns: none
 * Globals: wTicks
 *----------------------------------------------------------------------------
 */
void halDelayMS(BYTE MS)
{
#ifdef _8051_
	wScaleMS = (WORD)((DWORD)(MS * 1000) / SII_SYS_TICK_TIME);
	/* the expression can be optimized
	 * for simple TIME_TICK values
	 */
	RXEXTDBG("halDelayMS MS:%d, wScaleMS: %d\n", MS, wScaleMS);

	/* Value is global and decremented by hardware timer */
	while (wScaleMS)
		udelay(100);

#else
	__plat_msleep(MS);
#endif
	RXEXTDBG("halDelayMS %d\n", MS);
}

/*------------------------------------------------------------------------------
 * Function Name: siiGetTicksNumber
 * Function Description:  Get Ticks Number from system timer
 *
 * Accepts: none
 * Returns: wTickCounter
 * Globals: wTicks
 *----------------------------------------------------------------------------
 */
WORD siiGetTicksNumber(void)
{
#ifdef _8051_
	WORD wTicks;

	do {
		wTicks = wTickCounter;
	} while (wTicks != wTickCounter);
	/* reading of ticks should be atomic */

	return  wTicks;
#else
	return  wTickCounter;
#endif
}


/*------------------------------------------------------------------------------
 * Function Name: SysTickimerISR
 * Function Description: Timer 0 interrupts,  used to generate 8ms events used
 *                       for executing scheduled tasks
 *----------------------------------------------------------------------------
 */
#ifdef _8051_
void SysTickTimerISR(void) interrupt 1
{
	TF0 = 0;      /* Clear Interrupt Flag (8051 specific) */

	if (wScaleMS)
		wScaleMS--; /* Used to serve MS Delays */

	if (wTickCounter == wNewTaskTickCounter) {
		wNewTaskTickCounter += TASK_SLOT_IN_TICKS;

		if (!qReqTasksProcessing) {
			if (bNewTaskSlot != LAST_TASK)
				bNewTaskSlot++;
			else
				bNewTaskSlot = 0;
		}
		qReqTasksProcessing = TRUE;
	}
	/* Tick counter, used for time measuring */
	wTickCounter++;
}
#else
void SysTickTimerISR(void)
{
	if (wScaleMS)
		wScaleMS--; /* Used to serve MS Delays */

	if (wTickCounter == wNewTaskTickCounter) {
		wNewTaskTickCounter += TASK_SLOT_IN_TICKS;

		if (!qReqTasksProcessing) {
			if (bNewTaskSlot != LAST_TASK)
				bNewTaskSlot++;
			else
				bNewTaskSlot = 0;
		}
		qReqTasksProcessing = TRUE;
	}
	/* Tick counter, used for time measuring */
	wTickCounter++;
}
#endif


