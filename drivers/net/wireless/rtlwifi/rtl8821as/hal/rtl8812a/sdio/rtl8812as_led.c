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
static void SwLedOn_8821AS(PADAPTER padapter, PLED_SDIO pLed)
{
	u8 LedCfg;

	if (RTW_CANNOT_RUN(padapter))
		goto exit;

	LedCfg = rtw_read8(padapter, REG_LEDCFG2);
	switch (pLed->LedPin) {	
	case LED_PIN_LED2:
		rtw_write8(padapter, REG_LEDCFG2, (LedCfg & 0xc0) | BIT5 | BIT4 | BIT3);
		break;

	default:
		break;
	}

exit:
	pLed->bLedOn = _TRUE;
}

/*
 *	Description:
 *		Turn off LED according to LedPin specified.
 */
static void SwLedOff_8821AS(PADAPTER padapter, PLED_SDIO pLed)
{
	u8 LedCfg;

	if (RTW_CANNOT_RUN(padapter))
		goto exit;

	LedCfg = rtw_read8(padapter, REG_LEDCFG2);
	switch (pLed->LedPin) { 
	case LED_PIN_LED2:
		rtw_write8(padapter, REG_LEDCFG2, (LedCfg & 0xc0) | BIT5 | BIT4);
		break;

	default:
		break;
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
void rtl8821as_InitSwLeds(PADAPTER padapter)
{
	struct led_priv *pledpriv = &(padapter->ledpriv);

	pledpriv->LedControlHandler = LedControlSDIO;
	pledpriv->SwLedOn = SwLedOn_8821AS;
	pledpriv->SwLedOff = SwLedOff_8821AS;

	InitLed(padapter, &(pledpriv->SwLed0), LED_PIN_LED2);
	InitLed(padapter, &(pledpriv->SwLed1), LED_PIN_LED1);
}

/*
 *	Description:
 *		DeInitialize all LED_819xUsb objects.
 */
void rtl8821as_DeInitSwLeds(PADAPTER padapter)
{
	struct led_priv	*ledpriv = &padapter->ledpriv;

	DeInitLed(&ledpriv->SwLed0);
	DeInitLed(&ledpriv->SwLed1);
}

