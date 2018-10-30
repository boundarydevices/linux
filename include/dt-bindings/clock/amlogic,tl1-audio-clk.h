/*
 * include/dt-bindings/clock/amlogic,tl1-audio-clk.h
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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

#ifndef __TL1_AUDIO_CLK_H__
#define __TL1_AUDIO_CLK_H__

/*
 * CLKID audio index values
 */

#define CLKID_AUDIO_GATE_DDR_ARB                0
#define CLKID_AUDIO_GATE_PDM                    1
#define CLKID_AUDIO_GATE_TDMINA                 2
#define CLKID_AUDIO_GATE_TDMINB                 3
#define CLKID_AUDIO_GATE_TDMINC                 4
#define CLKID_AUDIO_GATE_TDMINLB                5
#define CLKID_AUDIO_GATE_TDMOUTA                6
#define CLKID_AUDIO_GATE_TDMOUTB                7
#define CLKID_AUDIO_GATE_TDMOUTC                8
#define CLKID_AUDIO_GATE_FRDDRA                 9
#define CLKID_AUDIO_GATE_FRDDRB                 10
#define CLKID_AUDIO_GATE_FRDDRC                 11
#define CLKID_AUDIO_GATE_TODDRA                 12
#define CLKID_AUDIO_GATE_TODDRB                 13
#define CLKID_AUDIO_GATE_TODDRC                 14
#define CLKID_AUDIO_GATE_LOOPBACKA              15
#define CLKID_AUDIO_GATE_SPDIFIN                16
#define CLKID_AUDIO_GATE_SPDIFOUT_A             17
#define CLKID_AUDIO_GATE_RESAMPLEA              18
#define CLKID_AUDIO_GATE_RESERVED0              19
#define CLKID_AUDIO_GATE_RESERVED1              20
#define CLKID_AUDIO_GATE_SPDIFOUT_B             21
#define CLKID_AUDIO_GATE_EQDRC                  22
#define CLKID_AUDIO_GATE_RESAMPLEB              23
#define CLKID_AUDIO_GATE_TOVAD                  24
#define CLKID_AUDIO_GATE_AUDIOLOCKER            25
#define CLKID_AUDIO_GATE_SPDIFIN_LB             26
#define CLKID_AUDIO_GATE_FRATV                  27
#define CLKID_AUDIO_GATE_FRHDMIRX               28
#define CLKID_AUDIO_GATE_FRDDRD                 29
#define CLKID_AUDIO_GATE_TODDRD                 30
#define CLKID_AUDIO_GATE_LOOPBACKB              31

#define MCLK_BASE                               32
#define CLKID_AUDIO_MCLK_A                      (MCLK_BASE + 0)
#define CLKID_AUDIO_MCLK_B                      (MCLK_BASE + 1)
#define CLKID_AUDIO_MCLK_C                      (MCLK_BASE + 2)
#define CLKID_AUDIO_MCLK_D                      (MCLK_BASE + 3)
#define CLKID_AUDIO_MCLK_E                      (MCLK_BASE + 4)
#define CLKID_AUDIO_MCLK_F                      (MCLK_BASE + 5)

#define CLKID_AUDIO_SPDIFIN                     (MCLK_BASE + 6)
#define CLKID_AUDIO_SPDIFOUT_A                  (MCLK_BASE + 7)
#define CLKID_AUDIO_RESAMPLE_A                  (MCLK_BASE + 8)
#define CLKID_AUDIO_LOCKER_OUT                  (MCLK_BASE + 9)
#define CLKID_AUDIO_LOCKER_IN                   (MCLK_BASE + 10)
#define CLKID_AUDIO_PDMIN0                      (MCLK_BASE + 11)
#define CLKID_AUDIO_PDMIN1                      (MCLK_BASE + 12)
#define CLKID_AUDIO_SPDIFOUT_B                  (MCLK_BASE + 13)
#define CLKID_AUDIO_RESAMPLE_B                  (MCLK_BASE + 14)
#define CLKID_AUDIO_SPDIFIN_LB                  (MCLK_BASE + 15)
#define CLKID_AUDIO_EQDRC                       (MCLK_BASE + 16)
#define CLKID_AUDIO_VAD                         (MCLK_BASE + 17)

#define NUM_AUDIO_CLKS                          (MCLK_BASE + 18)
#endif /* __TL1_AUDIO_CLK_H__ */
