/*
 * sound/soc/codecs/amlogic/aml_pmu4_codec.h
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

#ifndef AML_PMU4_H_
#define AML_PMU4_H_

#define PMU4_AUDIO_BASE    0x40
/*Info*/
#define PMU4_SOFT_RESET                0x00
#define PMU4_BLOCK_ENABLE              0x02
#define PMU4_AUDIO_CONFIG              0x04
#define PMU4_PGA_IN_CONFIG             0x06
#define PMU4_ADC_VOL_CTR               0x08
#define PMU4_DAC_SOFT_MUTE             0x0A
#define PMU4_DAC_VOL_CTR               0x0C
#define PMU4_LINE_OUT_CONFIG           0x0E

/*Block Enable , Reg 0x02h*/
#define PMU4_BIAS_CURRENT_EN    0xD
#define PMU4_PGAL_IN_EN         0xB
#define PMU4_PGAR_IN_EN         0xA
#define PMU4_PGAL_IN_ZC_EN      0x9
#define PMU4_PGAR_IN_ZC_EN      0x8
#define PMU4_ADCL_EN            0x7
#define PMU4_ADCR_EN            0x6
#define PMU4_DACL_EN            0x5
#define PMU4_DACR_EN            0x4
#define PMU4_LOLP_EN            0x3
#define PMU4_LOLN_EN            0x2
#define PMU4_LORP_EN            0x1
#define PMU4_LORN_EN            0x0

/*Audio Config,Reg 0x04h*/
#define PMU4_MCLK_FREQ          0xF
#define PMU4_I2S_MODE           0xE
#define PMU4_ADC_HPF_MODE       0xC
#define PMU4_ADC_DEM_EN         0xA
#define PMU4_ADC_CLK_TO_GPIO_EN 0x9
#define PMU4_DAC_CLK_TO_GPIO_EN 0x8
#define PMU4_DACL_DATA_SOURCE   0x7
#define PMU4_DACR_DATA_SOURCE   0x6
#define PMU4_DACL_INV           0x5
#define PMU4_DACR_INV           0x4
#define PMU4_ADCDATL_SOURCE     0x3
#define PMU4_ADCDATR_SOURCE     0x2
#define PMU4_ADCL_INV           0x1
#define PMU4_ADCR_INV           0x0

/*PGA_IN Config,  Reg 0x06h*/
#define PMU4_PGAL_IN_SEL        0xD
#define PMU4_PGAL_IN_GAIN       0x8
#define PMU4_PGAR_IN_SEL        0x5
#define PMU4_PGAR_IN_GAIN       0x0

/*ADC_Volume_Control , Reg 0x08h*/
#define PMU4_ADCL_VOL_CTR            0x8
#define PMU4_ADCR_VOL_CTR            0x0

/*DAC Soft Mute, Reg 0xA*/
#define PMU4_DAC_SOFT_MUTE_BIT  0xF
#define PMU4_DAC_UNMUTE_MODE    0xE
#define PMU4_DAC_MUTE_MODE      0xD
#define PMU4_DAC_VC_RAMP_MODE   0xC
#define PMU4_DAC_RAMP_RATE      0xA
#define PMU4_DAC_MONO           0x8
#define PMU4_MUTE_DAC_PD_EN     0x7

/*DAC_Volume_Control, Reg 0xC*/
#define PMU4_DACL_VOL_CTR            0x8
#define PMU4_DACR_VOL_CTR            0x0

/*Line-Out Config, Reg 0xE*/
#define PMU4_LOLP_SEL_DACL      0xE
#define PMU4_LOLP_SEL_AIL       0xD
#define PMU4_LOLP_SEL_SHIFT     0xC
#define PMU4_LOLN_SEL_DACL_INV  0xA
#define PMU4_LOLN_SEL_DACL      0x9
#define PMU4_LOLN_SEL_SHIFT     0x8
#define PMU4_LORP_SEL_DACR      0x6
#define PMU4_LORP_SEL_AIR       0x5
#define PMU4_LORP_SEL_SHIFT     0x4
#define PMU4_LORN_SEL_DACR_INV  0x2
#define PMU4_LORN_SEL_DACR      0x1
#define PMU4_LORN_SEL_SHIFT     0x0

#endif
