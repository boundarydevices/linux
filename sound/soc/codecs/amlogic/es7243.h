/*
 * ALSA SoC ES7243 codec driver
 *
 * Author:      David Yang, <yangxiaohua@everest-semi.com>
 * Copyright:   (C) 2017 Everest Semiconductor Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES7243_H
#define _ES7243_H

/* Codec TLV320AIC23 */
#define ES7243_MODECFG_REG00		0x00
#define ES7243_SDPFMT_REG01		0x01
#define ES7243_LRCDIV_REG02		0x02
#define ES7243_BCKDIV_REG03		0x03
#define ES7243_CLKDIV_REG04		0x04
#define ES7243_MUTECTL_REG05		0x05
#define ES7243_STATECTL_REG06		0x06
#define ES7243_ANACTL0_REG07		0x07
#define ES7243_ANACTL1_REG08		0x08
#define ES7243_ANACTL2_REG09		0x09
#define ES7243_ANACHARG_REG0A		0x0A
#define ES7243_INISTATE_REG0B		0x0B
#define ES7243_BIAS_REG0C		0x0C
#define ES7243_STMOSR_REG0D		0x0D
#define ES7243_CHIPID_REG0E		0x0E

#endif /* _ES7243_H_ */
