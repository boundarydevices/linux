/*
 * include/dt-bindings/clock/amlogic,g12a-audio-clk.h
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

#ifndef __G12A_AUDIO_CLK_H
#define __G12A_AUDIO_CLK_H

/*
 * CLKID audio index values
 */

#define CLKID_AUDIO_DDR_ARB					0
#define CLKID_AUDIO_PDM						1
#define CLKID_AUDIO_TDMINA					2
#define CLKID_AUDIO_TDMINB					3
#define CLKID_AUDIO_TDMINC					4
#define CLKID_AUDIO_TDMINLB					5
#define CLKID_AUDIO_TDMOUTA					6
#define CLKID_AUDIO_TDMOUTB					7
#define CLKID_AUDIO_TDMOUTC					8
#define CLKID_AUDIO_FRDDRA					9
#define CLKID_AUDIO_FRDDRB					10
#define CLKID_AUDIO_FRDDRC					11
#define CLKID_AUDIO_TODDRA					12
#define CLKID_AUDIO_TODDRB					13
#define CLKID_AUDIO_TODDRC					14
#define CLKID_AUDIO_LOOPBACK				15
#define CLKID_AUDIO_SPDIFIN					16
#define CLKID_AUDIO_SPDIFOUT				17
#define CLKID_AUDIO_RESAMPLE				18
#define CLKID_AUDIO_POWER_DETECT			19
#define CLKID_AUDIO_TORAM					20
#define CLKID_AUDIO_SPDIFOUTB				21
#define CLKID_AUDIO_EQDRC					22

#define MCLK_BASE							23
#define CLKID_AUDIO_MCLK_A		(MCLK_BASE + 0)
#define CLKID_AUDIO_MCLK_B		(MCLK_BASE + 1)
#define CLKID_AUDIO_MCLK_C		(MCLK_BASE + 2)
#define CLKID_AUDIO_MCLK_D		(MCLK_BASE + 3)
#define CLKID_AUDIO_MCLK_E		(MCLK_BASE + 4)
#define CLKID_AUDIO_MCLK_F		(MCLK_BASE + 5)

#define CLKID_AUDIO_SPDIFIN_CTRL		(MCLK_BASE + 6)
#define CLKID_AUDIO_SPDIFOUT_CTRL		(MCLK_BASE + 7)
#define CLKID_AUDIO_PDMIN0		(MCLK_BASE + 8)
#define CLKID_AUDIO_PDMIN1		(MCLK_BASE + 9)
#define CLKID_AUDIO_SPDIFOUTB_CTRL		(MCLK_BASE + 10)
#define CLKID_AUDIO_LOCKER_OUT		(MCLK_BASE + 11)
#define CLKID_AUDIO_LOCKER_IN		(MCLK_BASE + 12)
#define CLKID_AUDIO_RESAMPLE_CTRL		(MCLK_BASE + 13)

#define NUM_AUDIO_CLKS			(MCLK_BASE + 14)
#endif /* __G12A_AUDIO_CLK_H */
