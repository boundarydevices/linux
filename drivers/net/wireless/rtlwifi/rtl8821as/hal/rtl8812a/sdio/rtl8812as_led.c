/******************************************************************************
 *
 * Copyright(c) 2013 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#define _RTL8812AS_LED_C_

#include "drv_types.h"
#include "rtl8812a_hal.h"

//================================================================================
// LED object.
//================================================================================


//================================================================================
//	Prototype of protected function.
//================================================================================

//================================================================================
// LED_819xUsb routines.
//================================================================================

/*
 *	Description:
 *		Turn on LED according to LedPin specified.
 */
static void SwLedOn8812AS(PADAPTER padapter, PLED_SDIO pLed)
{
	if ((padapter->bSurpriseRemoved == _TRUE) || (padapter->bDriverStopped == _TRUE))
	{
		return;
	}

	pLed->bLedOn = _TRUE;
}

/*
 *	Description:
 *		Turn off LED according to LedPin specified.
 */
static void SwLedOff8821AS(PADAPTER padapter, PLED_SDIO pLed)
{
	if (padapter->bSurpriseRemoved == _TRUE)
	{
		goto exit;
	}

exit:
	pLed->bLedOn = _FALSE;

}

//================================================================================
// Default LED behavior.
//================================================================================

/*
 *	Description:
 *		Initialize all LED_871x objects.
 */
void InitSwLeds8821AS(PADAPTER padapter)
{
}

/*
 *	Description:
 *		DeInitialize all LED_819xUsb objects.
 */
void DeInitSwLeds8821AS(PADAPTER padapter)
{
}

