/*
 * include/dt-bindings/pwm/meson.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _DT_BINDINGS_PWM_MESON_H
#define _DT_BINDINGS_PWM_MESON_H


#define	PWM_A			0
#define	PWM_B			1
#define	PWM_C			2
#define	PWM_D			3
#define	PWM_AO_A		4
#define	PWM_AO_B		5
#define	PWM_AO_C		6
#define	PWM_AO_D		7

/*
 * Addtional 8 channels for gxtvbb , gxl ,gxm and txl
 */
#define	PWM_A2			8
#define	PWM_B2			9
#define	PWM_C2			10
#define	PWM_D2			11
#define	PWM_AO_A2		12
#define	PWM_AO_B2		13
#define	PWM_AO_C2		14
#define	PWM_AO_D2		15
/*add another four channels for txlx*/

#define	PWM_E			16
#define	PWM_F			17
#define	PWM_E2			18
#define	PWM_F2			19





#define CLKID_PLL_VID_NOT  /*for gxl gxm not support it*/



/* fclk_div4
 *--------------|\
 * fclk_div3	| \
 *--------------|  \    get clock source
 * vid_pll_clk	|  |---------------------
 *--------------|  |
 *	XTAL		| /
 *--------------|/
 * vid_pll_clk is not defined and described now,
 *waiting for  CLKID_VID_PLL is suportted in the future,
 * the macro is used for compiling passed.
 */
//#define CLKID_VID_PLL

/*
 * 4 clock sources to choose
 * keep the same order with pwm_aml_clk function in pwm driver
 */
#define	XTAL			0
#define	VID_PLL			1
#define	FCLK_DIV4		2
#define	FCLK_DIV3		3

#endif
