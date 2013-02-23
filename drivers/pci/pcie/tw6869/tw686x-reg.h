/*
 * Driver for Techwell TW6864/68 based DVR cards
 *
 * (c) 2009-10 liran <jlee@techwellinc.com.cn> [Techwell China]
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef TW686XREG_H_INCLUDED
#define TW686XREG_H_INCLUDED

//#define BIT(x)  (1<<(x))

//////////////////////////////////////////////////
//NO NOT CHANGE
// Register definitions
// IMPORTANT: These defines are DWORD-based.
#define TW6864_R_DMA_INT_STATUS				0x000	//0x00  //RO
#define		TW6864_R_DMA_INT_STATUS_ERR(ch)	(BIT(ch)<<24)
#define		TW6864_R_DMA_INT_STATUS_ERRALL	0xFF000000
#define		TW6864_R_DMA_INT_STATUS_TOUT	BIT(17)
#define		TW6864_R_DMA_INT_STATUS_DMA(ch)	BIT(ch)
#define		TW6864_R_DMA_INT_STATUS_DMAALL	0x1FFFF
#define TW6864_R_DMA_PB_STATUS				0x004	//0x01  //RO
#define TW6864_R_DMA_CMD					0x008	//0x02
#define		TW6864_R_DMA_CMD_TRIG			BIT(31)
#define		TW6864_R_DMA_CMD_RESET(ch)		BIT(ch)
#define		TW6864_R_DMA_CMD_RESETALL		0x01FFFF
#define TW6864_R_DMA_INT_ERROR				0x00C	//0x03
#define		TW6864_R_DMA_VINT_FIFO(ch)		(BIT(ch)<<24)
#define		TW6864_R_DMA_VINT_PTR(ch)		(BIT(ch)<<16)
#define		TW6864_R_DMA_XINT_ERROR(ch)		BIT(ch)
#define TW6864_R_VIDEO_CHID					0x010	//0x04
#define TW6864_R_VIDEO_PARSER_STATUS		0x014	//0x05
#define TW6864_R_SYS_SOFT_RST				0x018	//0x06

#define TW6864_R_DMA_PAGE_TABLE0			0x020	//0x08  //RW
#define TW6864_R_DMA_PAGE_TABLE1			0x024	//0x09
#define TW6864_R_DMA_CHANNEL_ENABLE			0x028	//0x0a
#define TW6864_R_DMA_CONFIG					0x02C	//0x0b
#define TW6864_R_DMA_INT_REF				0x030	//0x0c
#define TW6864_R_DMA_CHANNEL_TIMEOUT		0x034	//0x0d

#define TW6864_R_DMA_CH0_CONFIG				0x040	//0x10  //DMA_CH0_CONFIG ~ DMA_CH7_CONFIG are continusly
#define TW6864_R_DMA_CH1_CONFIG				0x044	//0x11
#define TW6864_R_DMA_CH2_CONFIG				0x048	//0x12
#define TW6864_R_DMA_CH3_CONFIG				0x04C	//0x13
#define TW6864_R_DMA_CH4_CONFIG				0x050	//0x14
#define TW6864_R_DMA_CH5_CONFIG				0x054	//0x15
#define TW6864_R_DMA_CH6_CONFIG				0x058	//0x16
#define TW6864_R_DMA_CH7_CONFIG				0x05C	//0x17
#define TW6864_R_DMA_CH8_CONFIG_P			0x060	//0x18   // DMA_CH8_CONFIG_P ~ DMA_CH10_CONFIG_B are continusly
#define TW6864_R_DMA_CH8_CONFIG_B			0x064	//0x19
#define TW6864_R_DMA_CH9_CONFIG_P			0x068	//0x1A
#define TW6864_R_DMA_CH9_CONFIG_B			0x06C	//0x1B
#define TW6864_R_DMA_CHA_CONFIG_P			0x070	//0x1C
#define TW6864_R_DMA_CHA_CONFIG_B			0x074	//0x1D
#define TW6864_R_DMA_CHB_CONFIG_P			0x078	//0x1E
#define TW6864_R_DMA_CHB_CONFIG_B			0x07C	//0x1F
#define TW6864_R_DMA_CHC_CONFIG_P			0x080	//0x20
#define TW6864_R_DMA_CHC_CONFIG_B			0x084	//0x21
#define TW6864_R_DMA_CHD_CONFIG_P			0x088	//0x22
#define TW6864_R_DMA_CHD_CONFIG_B			0x08C	//0x23
#define TW6864_R_DMA_CHE_CONFIG_P			0x090	//0x24
#define TW6864_R_DMA_CHE_CONFIG_B			0x094	//0x25
#define TW6864_R_DMA_CHF_CONFIG_P			0x098	//0x26
#define TW6864_R_DMA_CHF_CONFIG_B			0x09C	//0x27
#define TW6864_R_DMA_CH10_CONFIG_P			0x0A0	//0x28
#define TW6864_R_DMA_CH10_CONFIG_B			0x0A4	//0x29
#define TW6864_R_CH0to7_CONFIG_REG(idx)		(TW6864_R_DMA_CH0_CONFIG+((idx)*4))
#define TW6864_R_CH8to15_CONFIG_REG_P(idx)	(TW6864_R_DMA_CH8_CONFIG_P+((idx)*8))
#define TW6864_R_CH8to15_CONFIG_REG_B(idx)	(TW6864_R_DMA_CH8_CONFIG_B+((idx)*8))

#define TW6864_R_VIDEO_CTRL1				0x0A8	//0x2A
#define		TW6864_R_VIDEO_CTRL1_STANDARD(ch)	(BIT(ch)<<13)
#define		TW6864_R_VIDEO_CTRL1_STANDARDALL	(0xFF<<13)
#define TW6864_R_VIDEO_CTRL2				0x0AC	//0x2B
#define		TW6864_R_VIDEO_CTRL2_GENRST(ch)		(BIT(ch)<<16)
#define		TW6864_R_VIDEO_CTRL2_GENRSTALL		0xFF0000
#define		TW6864_R_VIDEO_CTRL2_PATTERNS(ch)	(BIT(ch)<<8)
#define		TW6864_R_VIDEO_CTRL2_PATTERNSALL	0xFF00
#define		TW6864_R_VIDEO_CTRL2_GEN(ch)		BIT(ch)
#define		TW6864_R_VIDEO_CTRL2_GENALL			0xFF
#define TW6864_R_AUDIO_CTRL1				0x0B0	//0x2C
#define TW6864_R_AUDIO_CTRL2				0x0B4	//0x2D
#define TW6864_R_PHASE_REF_CONFIG			0x0B8	//0x2E
#define TW6864_R_GPIO_REG					0x0BC	//0x2F

#define TW6864_R_INTL_HBAR0_CTRL			0x0C0	//0x30
#define TW6864_R_INTL_HBAR1_CTRL			0x0C4	//0x31
#define TW6864_R_INTL_HBAR2_CTRL			0x0C8	//0x32
#define TW6864_R_INTL_HBAR3_CTRL			0x0CC	//0x33
#define TW6864_R_INTL_HBAR4_CTRL			0x0D0	//0x34
#define TW6864_R_INTL_HBAR5_CTRL			0x0D4	//0x35
#define TW6864_R_INTL_HBAR6_CTRL			0x0D8	//0x36
#define TW6864_R_INTL_HBAR7_CTRL			0x0DC	//0x37

#define TW6864_R_AUDIO_CTRL3				0x0E0	//0x38
#define TW6864_R_DROP_FIELD_REG0			0x0E4	//0x39
#define TW6864_R_DROP_FIELD_REG1			0x0E8	//0x3A
#define TW6864_R_DROP_FIELD_REG2			0x0EC	//0x3B
#define TW6864_R_DROP_FIELD_REG3			0x0F0	//0x3C
#define TW6864_R_DROP_FIELD_REG4			0x0F4	//0x3D
#define TW6864_R_DROP_FIELD_REG5			0x0F8	//0x3E
#define TW6864_R_DROP_FIELD_REG6			0x0FC	//0x3F
#define TW6864_R_DROP_FIELD_REG7			0x100	//0x40
#define TW6864_R_VIDEO_SIZE_REGA			0x104	//0x41		//Rev.A only
#define TW6864_R_SHSCALER_REG0				0x108	//0x42
#define TW6864_R_SHSCALER_REG1				0x10C	//0x43
#define TW6864_R_SHSCALER_REG2				0x110	//0x44
#define TW6864_R_SHSCALER_REG3				0x114	//0x45
#define TW6864_R_SHSCALER_REG4				0x118	//0x46
#define TW6864_R_SHSCALER_REG5				0x11C	//0x47
#define TW6864_R_SHSCALER_REG6				0x120	//0x48
#define TW6864_R_SHSCALER_REG7				0x124	//0x49
#define TW6864_R_VIDEO_SIZE_REG0			0x128	//0x4A		//Rev.B or later
#define TW6864_R_VIDEO_SIZE_REG1			0x12C	//0x4B
#define TW6864_R_VIDEO_SIZE_REG2			0x130	//0x4C
#define TW6864_R_VIDEO_SIZE_REG3			0x134	//0x4D
#define TW6864_R_VIDEO_SIZE_REG4			0x138	//0x4E
#define TW6864_R_VIDEO_SIZE_REG5			0x13C	//0x4F
#define TW6864_R_VIDEO_SIZE_REG6			0x140	//0x50
#define TW6864_R_VIDEO_SIZE_REG7			0x144	//0x51
#define TW6864_R_VIDEO_SIZE_REG0_F2			0x148	//0x52  //Rev.B or later
#define TW6864_R_VIDEO_SIZE_REG1_F2			0x14C	//0x53
#define TW6864_R_VIDEO_SIZE_REG2_F2			0x150	//0x54
#define TW6864_R_VIDEO_SIZE_REG3_F2			0x154	//0x55
#define TW6864_R_VIDEO_SIZE_REG4_F2			0x158	//0x56
#define TW6864_R_VIDEO_SIZE_REG5_F2			0x15C	//0x57
#define TW6864_R_VIDEO_SIZE_REG6_F2			0x160	//0x58
#define TW6864_R_VIDEO_SIZE_REG7_F2			0x164	//0x59
#define TW6864_R_VC_CTRL_REG0				0x1C0	//0x70
#define TW6864_R_VC_CTRL_REG1				0x1C4	//0x71
#define TW6864_R_VC_CTRL_REG2				0x1C8	//0x72
#define TW6864_R_VC_CTRL_REG3				0x1D0	//0x73
#define TW6864_R_VC_CTRL_REG4				0x1D4	//0x74
#define TW6864_R_VC_CTRL_REG5				0x1D8	//0x75
#define TW6864_R_VC_CTRL_REG6				0x1DC	//0x76
#define TW6864_R_VC_CTRL_REG7				0x1E0	//0x77
#define TW6864_R_BDMA_ADDR_P_0				0x200	//0x80   //0x80 ~ 0xBF,Rev.B or later
#define TW6864_R_BDMA_WHP_0					0x204	//0x81
#define TW6864_R_BDMA_ADDR_B_0				0x208	//0x82
#define TW6864_R_BDMA_ADDR_P_F2_0			0x210	//0x84
#define TW6864_R_BDMA_WHP_F2_0				0x214	//0x85
#define TW6864_R_BDMA_ADDR_B_F2_0			0x218	//0x86
#define TW6864_R_CSR_REG					0x3F4	//0xFD
#define TW6864_R_EP_REG_ADDR				0x3F8	//0xFE
#define TW6864_R_EP_REG_DATA				0x3FC	//0xFF
#define TW6864_R_DROP_FIELD_REG(idx)		(TW6864_R_DROP_FIELD_REG0+((idx)*4))
#define TW6864_R_SHSCALER_REG(idx)			(TW6864_R_SHSCALER_REG0+((idx)*4))
#define TW6864_R_VIDEO_SIZE_REG(idx)		(TW6864_R_VIDEO_SIZE_REG0+((idx)*4))
#define TW6864_R_VIDEO_SIZE_REG_F2(idx)		(TW6864_R_VIDEO_SIZE_REG0_F2+((idx)*4))
#define TW6864_R_VC_CTRL_REG(idx)			(TW6864_R_VC_CTRL_REG0+((idx)*4))
#define TW6864_R_BDMA(reg, idx)				((reg)+(idx)*0x20)

//////////////////////////////////////////////////
// external 2864 registers definitions
#define TW2864_R_I2C_ADDR0					0x50
#define TW2864_R_I2C_ADDR1					0x52
#define TW2864_R_VSTATUS_0					0x00
#define TW2864_R_VSTATUS_1					0x10
#define TW2864_R_VSTATUS_2					0x20
#define TW2864_R_VSTATUS_3					0x30
#define TW2864_R_VDELAY_0					0x08
#define TW2864_R_VDELAY_1					0x18
#define TW2864_R_VDELAY_2					0x28
#define TW2864_R_VDELAY_3					0x38
#define TW2864_R_HDELAY_0					0x0A
#define TW2864_R_HDELAY_1					0x1A
#define TW2864_R_HDELAY_2					0x2A
#define TW2864_R_HDELAY_3					0x3A
#define TW2864_R_STANDARD_0					0x0E
#define TW2864_R_STANDARD_1					0x1E
#define TW2864_R_STANDARD_2					0x2E
#define TW2864_R_STANDARD_3					0x3E
#define TW2864_R_CLK_SEL_2865				0x61 //specfic to TW2865
#define TW2864_R_VERTICAL_SHARPNESS			0x85
#define TW2864_R_VERTICAL_CTRL				0x8F
#define TW2864_R_LOOP_CTRL					0x95
#define TW2864_R_MISC_CTRLII1				0x96
#define TW2864_R_MISC_CTRLII2				0x97
#define TW2864_R_MPP_MODE0					0xC8
#define TW2864_R_MPP_MODE1					0xC9
#define TW2864_R_NOVID						0x9E
#define TW2864_R_CLKODEL					0x9F
#define TW2864_R_HFLT1						0xA8
#define TW2864_R_HFLT2						0xA9
#define TW2864_R_VIDEO_OUT_CTRL				0xCA
#define TW2864_R_SMD						0xCF
#define TW2864_R_MASTER_CTRL				0xDB
#define TW2864_R_AUDIO_GAIN0				0xD0
#define TW2864_R_AUDIO_GAIN1				0xD1
#define TW2864_R_NUM_ADUIOR					0xD2
#define TW2864_R_SEQ_ADUIOR					0xD7
#define TW2864_R_MIX_OUT_SEL				0xE0
#define TW2864_R_VSCALE_LO_0				0xE4
#define TW2864_R_VHSCALE_HI_0				0xE5
#define TW2864_R_HSCALE_LO_0				0xE6
#define TW2864_R_VSCALE_LO_1				0xE7
#define TW2864_R_VHSCALE_HI_1				0xE8
#define TW2864_R_HSCALE_LO_1				0xE9
#define TW2864_R_VSCALE_LO_2				0xEA
#define TW2864_R_VHSCALE_HI_2				0xEB
#define TW2864_R_HSCALE_LO_2				0xEC
#define TW2864_R_VSCALE_LO_3				0xED
#define TW2864_R_VHSCALE_HI_3				0xEE
#define TW2864_R_HSCALE_LO_3				0xEF
#define TW2864_R_VIDEO_MISC					0xF9
#define TW2864_R_CLKOCTRL					0xFA
#define TW2864_R_CLKOPOL					0xFB
#define TW2864_R_AVDET_ENA					0xFC
#define TW2864_R_AVDET_STATE				0xFD
#define TW2864_R_DEV_REV					0xFE
#define TW2864_R_DEV_REV_LO					0xFF
#define TW2864_R_CHANNEL(reg, ch)			((reg)+(ch)*0x10)

//0x01 0x11 0x21 0x31 BRIGHT BRIGHTNESS
#define TW2864_R_BRIGHT_0					0x01
#define TW2864_R_BRIGHT_1					0x11
#define TW2864_R_BRIGHT_2					0x21
#define TW2864_R_BRIGHT_3					0x31

//0x02 0x12 0x22 0x32 CONTRAST CONTRAST
#define TW2864_R_CONTRAST_0					0x02
#define TW2864_R_CONTRAST_1					0x12
#define TW2864_R_CONTRAST_2					0x22
#define TW2864_R_CONTRAST_3					0x32

//0x03 0x13 0x23 0x33 SHARPNESS SCURVE VSF CTI SHARPNESS
#define TW2864_R_SHARPNESS_0				0x03
#define TW2864_R_SHARPNESS_1				0x13
#define TW2864_R_SHARPNESS_2				0x23
#define TW2864_R_SHARPNESS_3				0x33

//0x04 0x14 0x24 0x34 SAT_U SAT_U
#define TW2864_R_SAT_U_0					0x04
#define TW2864_R_SAT_U_1					0x14
#define TW2864_R_SAT_U_2					0x24
#define TW2864_R_SAT_U_3					0x34

//0x05 0x15 0x25 0x35 SAT_V SAT_V
#define TW2864_R_SAT_V_0					0x05
#define TW2864_R_SAT_V_1					0x15
#define TW2864_R_SAT_V_2					0x25
#define TW2864_R_SAT_V_3					0x35

//0x06 0x16 0x26 0x36 HUE HUE
#define TW2864_R_HUE_0						0x06
#define TW2864_R_HUE_1						0x16
#define TW2864_R_HUE_2						0x26
#define TW2864_R_HUE_3						0x36

//////////////////////////////////////////////////
// internal 2864 registers definitions
#define TW6864_R_VSTATUS_0					0x400	//0x100
#define TW6864_R_VSTATUS_1					0x440	//0x110
#define TW6864_R_VSTATUS_2					0x480	//0x120
#define TW6864_R_VSTATUS_3					0x4C0	//0x130
#define TW6864_R_BRIGHT_0					0x404	//0x101
#define TW6864_R_BRIGHT_1					0x444	//0x111
#define TW6864_R_BRIGHT_2					0x484	//0x121
#define TW6864_R_BRIGHT_3					0x4C4	//0x131
#define TW6864_R_CONTRAST_0					0x408	//0x102
#define TW6864_R_CONTRAST_1					0x448	//0x112
#define TW6864_R_CONTRAST_2					0x488	//0x122
#define TW6864_R_CONTRAST_3					0x4C8	//0x132
#define TW6864_R_SHARPNESS_0				0x40C	//0x103
#define TW6864_R_SHARPNESS_1				0x44C	//0x113
#define TW6864_R_SHARPNESS_2				0x48C	//0x123
#define TW6864_R_SHARPNESS_3				0x4CC	//0x133
#define TW6864_R_SAT_U_0					0x410	//0x104
#define TW6864_R_SAT_U_1					0x450	//0x114
#define TW6864_R_SAT_U_2					0x490	//0x124
#define TW6864_R_SAT_U_3					0x4D0	//0x134
#define TW6864_R_SAT_V_0					0x414	//0x105
#define TW6864_R_SAT_V_1					0x454	//0x115
#define TW6864_R_SAT_V_2					0x494	//0x125
#define TW6864_R_SAT_V_3					0x4D4	//0x135
#define TW6864_R_HUE_0						0x418	//0x106
#define TW6864_R_HUE_1						0x458	//0x116
#define TW6864_R_HUE_2						0x498	//0x126
#define TW6864_R_HUE_3						0x4D8	//0x136
#define TW6864_R_CROPPING_0					0x41C	//0x107
#define TW6864_R_CROPPING_1					0x45C	//0x117
#define TW6864_R_CROPPING_2					0x49C	//0x127
#define TW6864_R_CROPPING_3					0x4DC	//0x137
#define TW6864_R_VDELAY_0					0x420	//0x108
#define TW6864_R_VDELAY_1					0x460	//0x118
#define TW6864_R_VDELAY_2					0x4A0	//0x128
#define TW6864_R_VDELAY_3					0x4E0	//0x138
#define TW6864_R_VACTIVE_0					0x424	//0x109
#define TW6864_R_VACTIVE_1					0x464	//0x119
#define TW6864_R_VACTIVE_2					0x4A4	//0x129
#define TW6864_R_VACTIVE_3					0x4E4	//0x139
#define TW6864_R_HDELAY_0					0x428	//0x10A
#define TW6864_R_HDELAY_1					0x468	//0x11A
#define TW6864_R_HDELAY_2					0x4A8	//0x12A
#define TW6864_R_HDELAY_3					0x4E8	//0x13A
#define TW6864_R_HACTIVE_0					0x42C	//0x10B
#define TW6864_R_HACTIVE_1					0x46C	//0x11B
#define TW6864_R_HACTIVE_2					0x4AC	//0x12B
#define TW6864_R_HACTIVE_3					0x4EC	//0x13B
#define TW6864_R_MACROVISION_0				0x430	//0x10C
#define TW6864_R_MACROVISION_1				0x470	//0x11C
#define TW6864_R_MACROVISION_2				0x4B0	//0x12C
#define TW6864_R_MACROVISION_3				0x4F0	//0x13C
#define TW6864_R_CHIPSTATUSII_0				0x434	//0x10D
#define TW6864_R_CHIPSTATUSII_1				0x474	//0x11D
#define TW6864_R_CHIPSTATUSII_2				0x4B4	//0x12D
#define TW6864_R_CHIPSTATUSII_3				0x4F4	//0x13D
#define TW6864_R_STANDARD_0					0x438	//0x10E
#define TW6864_R_STANDARD_1					0x478	//0x11E
#define TW6864_R_STANDARD_2					0x4B8	//0x12E
#define TW6864_R_STANDARD_3					0x4F8	//0x13E
#define TW6864_R_STDDETECT_0				0x43C	//0x10F
#define TW6864_R_STDDETECT_1				0x47C	//0x11F
#define TW6864_R_STDDETECT_2				0x4BC	//0x12F
#define TW6864_R_STDDETECT_3				0x4FC	//0x13F
#define TW6864_R_VSCALE_LO_0				0x510	//0x144
#define TW6864_R_VSCALE_LO_1				0x550	//0x154
#define TW6864_R_VSCALE_LO_2				0x590	//0x164
#define TW6864_R_VSCALE_LO_3				0x5D0	//0x174
#define TW6864_R_VHSCALE_HI_0				0x514	//0x145
#define TW6864_R_VHSCALE_HI_1				0x554	//0x155
#define TW6864_R_VHSCALE_HI_2				0x594	//0x165
#define TW6864_R_VHSCALE_HI_3				0x5D4	//0x175
#define TW6864_R_HSCALE_LO_0				0x518	//0x146
#define TW6864_R_HSCALE_LO_1				0x558	//0x156
#define TW6864_R_HSCALE_LO_2				0x598	//0x166
#define TW6864_R_HSCALE_LO_3				0x5D8	//0x176
#define TW6864_R_CROPPING_F2_0				0x51C	//0x147
#define TW6864_R_CROPPING_F2_1				0x55C	//0x157
#define TW6864_R_CROPPING_F2_2				0x59C	//0x167
#define TW6864_R_CROPPING_F2_3				0x5DC	//0x177
#define TW6864_R_VDELAY_F2_0				0x520	//0x148
#define TW6864_R_VDELAY_F2_1				0x560	//0x158
#define TW6864_R_VDELAY_F2_2				0x5A0	//0x168
#define TW6864_R_VDELAY_F2_3				0x5E0	//0x178
#define TW6864_R_VACTIVE_F2_0				0x524	//0x149
#define TW6864_R_VACTIVE_F2_1				0x564	//0x159
#define TW6864_R_VACTIVE_F2_2				0x5A4	//0x169
#define TW6864_R_VACTIVE_F2_3				0x5E4	//0x179
#define TW6864_R_HDELAY_F2_0				0x528	//0x14A
#define TW6864_R_HDELAY_F2_1				0x568	//0x15A
#define TW6864_R_HDELAY_F2_2				0x5A8	//0x16A
#define TW6864_R_HDELAY_F2_3				0x5E8	//0x17A
#define TW6864_R_HACTIVE_F2_0				0x52C	//0x14B
#define TW6864_R_HACTIVE_F2_1				0x56C	//0x15B
#define TW6864_R_HACTIVE_F2_2				0x5AC	//0x16B
#define TW6864_R_HACTIVE_F2_3				0x5EC	//0x17B
#define TW6864_R_VSCALE_LO_F2_0				0x530	//0x14C
#define TW6864_R_VSCALE_LO_F2_1				0x570	//0x15C
#define TW6864_R_VSCALE_LO_F2_2				0x5B0	//0x16C
#define TW6864_R_VSCALE_LO_F2_3				0x5F0	//0x17C
#define TW6864_R_VHSCALE_HI_F2_0			0x534	//0x14D
#define TW6864_R_VHSCALE_HI_F2_1			0x574	//0x15D
#define TW6864_R_VHSCALE_HI_F2_2			0x5B4	//0x16D
#define TW6864_R_VHSCALE_HI_F2_3			0x5F4	//0x17D
#define TW6864_R_HSCALE_LO_F2_0				0x538	//0x14E
#define TW6864_R_HSCALE_LO_F2_1				0x578	//0x15E
#define TW6864_R_HSCALE_LO_F2_2				0x5B8	//0x16E
#define TW6864_R_HSCALE_LO_F2_3				0x5F8	//0x17E
#define TW6864_R_F2_CNT_0					0x53C	//0x14F
#define TW6864_R_F2_CNT_1					0x57C	//0x15F
#define TW6864_R_F2_CNT_2					0x5BC	//0x16F
#define TW6864_R_F2_CNT_3					0x5FC	//0x17F
#define TW6864_R_AVSRST						0x600	//0x180
#define TW6864_R_COLORKILL_HY				0x610	//0x184
#define TW6864_R_VERTICAL_CTRL				0x63C	//0x18F
#define TW6864_R_LOOP_CTRL					0x654	//0x195
#define TW6864_R_MISC_CTRLII1				0x658	//0x196
#define TW6864_R_MISC_CTRLII2				0x65C	//0x197
#define TW6864_R_HFLT1						0x6A0	//0x1A8
#define TW6864_R_HFLT2						0x6A4	//0x1A9
#define TW6864_R_AUDIO_GAIN_0				0x740	//0x1D0
#define TW6864_R_AUDIO_GAIN_1				0x744	//0x1D1
#define TW6864_R_AUDIO_GAIN_2				0x748	//0x1D2
#define TW6864_R_AUDIO_GAIN_3				0x74C	//0x1D3

#define TW6864_R_VSTATUS(ch)				(TW6864_R_VSTATUS_0 + (ch)*0x40)
#define TW6864_R_BRIGHT(ch)					(TW6864_R_BRIGHT_0 + (ch)*0x40)
#define TW6864_R_CONTRAST(ch)				(TW6864_R_CONTRAST_0 + (ch)*0x40)
#define TW6864_R_SHARPNESS(ch)				(TW6864_R_SHARPNESS_0 + (ch)*0x40)
#define TW6864_R_SAT_U(ch)					(TW6864_R_SAT_U_0 + (ch)*0x40)
#define TW6864_R_SAT_V(ch)					(TW6864_R_SAT_V_0 + (ch)*0x40)
#define TW6864_R_HUE(ch)					(TW6864_R_HUE_0 + (ch)*0x40)
#define TW6864_R_CROPPING(ch)				(TW6864_R_CROPPING_0 + (ch)*0x40)
#define TW6864_R_VDELAY(ch)					(TW6864_R_VDELAY_0 + (ch)*0x40)
#define TW6864_R_VACTIVE(ch)				(TW6864_R_VACTIVE_0 + (ch)*0x40)
#define TW6864_R_HDELAY(ch)					(TW6864_R_HDELAY_0 + (ch)*0x40)
#define TW6864_R_HACTIVE(ch)				(TW6864_R_HACTIVE_0 + (ch)*0x40)
#define TW6864_R_CROPPING_F2(ch)			(TW6864_R_CROPPING_F2_0 + (ch)*0x40)
#define TW6864_R_VDELAY_F2(ch)				(TW6864_R_VDELAY_F2_0 + (ch)*0x40)
#define TW6864_R_VACTIVE_F2(ch)				(TW6864_R_VACTIVE_F2_0 + (ch)*0x40)
#define TW6864_R_HDELAY_F2(ch)				(TW6864_R_HDELAY_F2_0 + (ch)*0x40)
#define TW6864_R_HACTIVE_F2(ch)				(TW6864_R_HACTIVE_F2_0 + (ch)*0x40)
#define TW6864_R_MACROVISION(ch)			(TW6864_R_MACROVISION_0 + (ch)*0x40)
#define TW6864_R_CHIPSTATUSII(ch)			(TW6864_R_CHIPSTATUSII_0 + (ch)*0x40)
#define TW6864_R_STANDARD(ch)				(TW6864_R_STANDARD_0 + (ch)*0x40)
#define TW6864_R_STDDETECT(ch)				(TW6864_R_STDDETECT_0 + (ch)*0x40)
#define TW6864_R_CHANNEL(reg, ch)			((reg)+(ch)*0x40)

//////////////////////////////////////////////////
//NO NOT CHANGE
//These 3 definitions are matched with hardware
#define TW6864_MAX_NUM_SG_DMA				8
#define	TW6864_MAX_NUM_DATA_DMA				9
#define TW6864_MAX_NUM_DMA					(TW6864_MAX_NUM_SG_DMA+TW6864_MAX_NUM_DATA_DMA)

#define TW6864_VIDEO_GEN					0xFF  //1=External,0=internal
#define	TW6864_VIDEO_GEN_PATTERNS			0x00  //0=ColorBar, 1=SEQ DATA , 8-bit
#define TW6864_VIDEO_FORMAT_UYVY			0
#define TW6864_VIDEO_FORMAT_YUV420			1
#define TW6864_VIDEO_FORMAT_Y411			2  //Use VIDEO_FORMAT_IYU1 instead, LEAD does not accept this, even they are the same
#define TW6864_VIDEO_FORMAT_Y41P			3
#define TW6864_VIDEO_FORMAT_RGB555			4
#define TW6864_VIDEO_FORMAT_RGB565			5
#define TW6864_VIDEO_FORMAT_IYU1			0xA //same as VIDEO_FORMAT_Y411, m_nVideoFormat will be masked by 0x7
#define TW6864_VIDEO_FORMAT_YUYV			6  //YUYV=YUY2, only available for Rev.B or later
#define TW6864_VIDEO_FORMAT_UYVY_FRAME		0x8  //VideoInfoHeader2

#define CaptureFLV                          0

#define TW6864_AUDIO_DMA_LENGTH				4096  //in bytes, should not > PAGE_SIZE(4096)
#define TW6864_AUDIO_GEN					1     //1=External,0=internal
#define	TW6864_AUDIO_GEN_PATTERN			0     //0=WAVE,1=SEQ DATA , 1-bit
#define TW6864_AUDIO_GEN_MIX_SEL			0     //(0~7)
#define TW6864_AUDIO_SAMPLE_RATE			8	  //in KHz
#define TW6864_AUDIO_SAMPLE_BIT				(CaptureFLV*8+8)  //8bit or 16bit
#define TW6864_AUDIO_SAMP_RATE_INT(rate)    (125000/(rate))
#define TW6864_AUDIO_SAMP_RATE_EXT(rate)  	((((unsigned long long)125000)<<16)/(rate))

//for TW6869
//#define FPGA

#define TW6869_R_MD_CONF0					0x180	//0x60  //TW6869 or later, 0x60 ~ 0x67
#define TW6869_R_MD_INIT0					0x1A0	//0x68  //TW6869 or later, 0x68 ~ 0x6F
#define TW6869_R_MD_MAP0					0x1C0	//0x70  //TW6869 or later, 0x70 ~ 0x77

#define TW6869_R_DMA_PAGE_TABLE0			TW6864_R_DMA_PAGE_TABLE0	//0x020	//0x08  //RW
#define TW6869_R_DMA_PAGE_TABLE1			TW6864_R_DMA_PAGE_TABLE1	//0x024	//0x09
#define TW6869_R_DMA_PAGE_TABLE2			0x340	//0xD0
#define TW6869_R_DMA_PAGE_TABLE3			0x344	//0xD1
#define TW6869_R_DMA_PAGE_TABLE4    		0x348	//0xD2
#define TW6869_R_DMA_PAGE_TABLE5    		0x34C	//0xD3
#define TW6869_R_DMA_PAGE_TABLE6    		0x350	//0xD4
#define TW6869_R_DMA_PAGE_TABLE7    		0x354	//0xD5
#define TW6869_R_DMA_PAGE_TABLE8    		0x358	//0xD6
#define TW6869_R_DMA_PAGE_TABLE9    		0x35C	//0xD7
#define TW6869_R_DMA_PAGE_TABLEA    		0x360	//0xD8
#define TW6869_R_DMA_PAGE_TABLEB    		0x364	//0xD9
#define TW6869_R_DMA_PAGE_TABLEC    		0x368	//0xDA
#define TW6869_R_DMA_PAGE_TABLED    		0x36C	//0xDB
#define TW6869_R_DMA_PAGE_TABLEE    		0x370	//0xDC
#define TW6869_R_DMA_PAGE_TABLEF    		0x374	//0xDD
#define TW6869_R_GPIO_REG_1					0x380	//0xE0
#define TW6869_R_GPIO_REG_2					0x384	//0xE1
#define TW6869_R_PIN_CFG_CTRL				0x3EC	//0xFB
#define TW6869_R_VSTATUS_4					0x800	//0x200
#define TW6869_R_VSTATUS_5					0x840	//0x210
#define TW6869_R_VSTATUS_6					0x880	//0x220
#define TW6869_R_VSTATUS_7					0x8C0	//0x230
#define TW6869_R_BRIGHT_4					0x804	//0x201
#define TW6869_R_BRIGHT_5					0x844	//0x211
#define TW6869_R_BRIGHT_6					0x884	//0x221
#define TW6869_R_BRIGHT_7					0x8C4	//0x231
#define TW6869_R_CONTRAST_4					0x808	//0x202
#define TW6869_R_CONTRAST_5					0x848	//0x212
#define TW6869_R_CONTRAST_6					0x888	//0x222
#define TW6869_R_CONTRAST_7					0x8C8	//0x232
#define TW6869_R_SHARPNESS_4				0x80C	//0x203
#define TW6869_R_SHARPNESS_5				0x84C	//0x213
#define TW6869_R_SHARPNESS_6				0x88C	//0x223
#define TW6869_R_SHARPNESS_7				0x8CC	//0x233
#define TW6869_R_SAT_U_4					0x810	//0x204
#define TW6869_R_SAT_U_5					0x850	//0x214
#define TW6869_R_SAT_U_6					0x890	//0x224
#define TW6869_R_SAT_U_7					0x8D0	//0x234
#define TW6869_R_SAT_V_4					0x814	//0x205
#define TW6869_R_SAT_V_5					0x854	//0x215
#define TW6869_R_SAT_V_6					0x894	//0x225
#define TW6869_R_SAT_V_7					0x8D4	//0x235
#define TW6869_R_HUE_4						0x818	//0x206
#define TW6869_R_HUE_5						0x858	//0x216
#define TW6869_R_HUE_6						0x898	//0x226
#define TW6869_R_HUE_7						0x8D8	//0x236
#define TW6869_R_CROPPING_4					0x81C	//0x207
#define TW6869_R_CROPPING_5					0x85C	//0x217
#define TW6869_R_CROPPING_6					0x89C	//0x227
#define TW6869_R_CROPPING_7					0x8DC	//0x237
#define TW6869_R_VDELAY_4					0x820	//0x208
#define TW6869_R_VDELAY_5					0x860	//0x218
#define TW6869_R_VDELAY_6					0x8A0	//0x228
#define TW6869_R_VDELAY_7					0x8E0	//0x238
#define TW6869_R_VACTIVE_4					0x824	//0x209
#define TW6869_R_VACTIVE_5					0x864	//0x219
#define TW6869_R_VACTIVE_6					0x8A4	//0x229
#define TW6869_R_VACTIVE_7					0x8E4	//0x239
#define TW6869_R_HDELAY_4					0x828	//0x20A
#define TW6869_R_HDELAY_5					0x868	//0x21A
#define TW6869_R_HDELAY_6					0x8A8	//0x22A
#define TW6869_R_HDELAY_7					0x8E8	//0x23A
#define TW6869_R_HACTIVE_4					0x82C	//0x20B
#define TW6869_R_HACTIVE_5					0x86C	//0x21B
#define TW6869_R_HACTIVE_6					0x8AC	//0x22B
#define TW6869_R_HACTIVE_7					0x8EC	//0x23B
#define TW6869_R_MACROVISION_4				0x830	//0x20C
#define TW6869_R_MACROVISION_5				0x870	//0x21C
#define TW6869_R_MACROVISION_6				0x8B0	//0x22C
#define TW6869_R_MACROVISION_7				0x8F0	//0x23C
#define TW6869_R_CHIPSTATUSII_4				0x834	//0x20D
#define TW6869_R_CHIPSTATUSII_5				0x874	//0x21D
#define TW6869_R_CHIPSTATUSII_6				0x8B4	//0x22D
#define TW6869_R_CHIPSTATUSII_7				0x8F4	//0x23D
#define TW6869_R_STANDARD_4					0x838	//0x20E
#define TW6869_R_STANDARD_5					0x878	//0x21E
#define TW6869_R_STANDARD_6					0x8B8	//0x22E
#define TW6869_R_STANDARD_7					0x8F8	//0x23E
#define TW6869_R_STDDETECT_4				0x83C	//0x20F
#define TW6869_R_STDDETECT_5				0x87C	//0x21F
#define TW6869_R_STDDETECT_6				0x8BC	//0x22F
#define TW6869_R_STDDETECT_7				0x8FC	//0x23F
#define TW6869_R_VSCALE_LO_4				0x910	//0x244
#define TW6869_R_VSCALE_LO_5				0x950	//0x254
#define TW6869_R_VSCALE_LO_6				0x990	//0x264
#define TW6869_R_VSCALE_LO_7				0x9D0	//0x274
#define TW6869_R_VHSCALE_HI_4				0x914	//0x245
#define TW6869_R_VHSCALE_HI_5				0x954	//0x255
#define TW6869_R_VHSCALE_HI_6				0x994	//0x265
#define TW6869_R_VHSCALE_HI_7				0x9D4	//0x275
#define TW6869_R_HSCALE_LO_4				0x918	//0x246
#define TW6869_R_HSCALE_LO_5				0x958	//0x256
#define TW6869_R_HSCALE_LO_6				0x998	//0x266
#define TW6869_R_HSCALE_LO_7				0x9D8	//0x276
#define TW6869_R_CROPPING_F2_4				0x91C	//0x247
#define TW6869_R_CROPPING_F2_5				0x95C	//0x257
#define TW6869_R_CROPPING_F2_6				0x99C	//0x267
#define TW6869_R_CROPPING_F2_7				0x9DC	//0x277
#define TW6869_R_VDELAY_F2_4				0x920	//0x248
#define TW6869_R_VDELAY_F2_5				0x960	//0x258
#define TW6869_R_VDELAY_F2_6				0x9A0	//0x268
#define TW6869_R_VDELAY_F2_7				0x9E0	//0x278
#define TW6869_R_VACTIVE_F2_4				0x924	//0x249
#define TW6869_R_VACTIVE_F2_5				0x964	//0x259
#define TW6869_R_VACTIVE_F2_6				0x9A4	//0x269
#define TW6869_R_VACTIVE_F2_7				0x9E4	//0x279
#define TW6869_R_HDELAY_F2_4				0x928	//0x24A
#define TW6869_R_HDELAY_F2_5				0x968	//0x25A
#define TW6869_R_HDELAY_F2_6				0x9A8	//0x26A
#define TW6869_R_HDELAY_F2_7				0x9E8	//0x27A
#define TW6869_R_HACTIVE_F2_4				0x92C	//0x24B
#define TW6869_R_HACTIVE_F2_5				0x96C	//0x25B
#define TW6869_R_HACTIVE_F2_6				0x9AC	//0x26B
#define TW6869_R_HACTIVE_F2_7				0x9EC	//0x27B
#define TW6869_R_VSCALE_LO_F2_4				0x930	//0x24C
#define TW6869_R_VSCALE_LO_F2_5				0x970	//0x25C
#define TW6869_R_VSCALE_LO_F2_6				0x9B0	//0x26C
#define TW6869_R_VSCALE_LO_F2_7				0x9F0	//0x27C
#define TW6869_R_VHSCALE_HI_F2_4			0x934	//0x24D
#define TW6869_R_VHSCALE_HI_F2_5			0x974	//0x25D
#define TW6869_R_VHSCALE_HI_F2_6			0x9B4	//0x26D
#define TW6869_R_VHSCALE_HI_F2_7			0x9F4	//0x27D
#define TW6869_R_HSCALE_LO_F2_4				0x938	//0x24E
#define TW6869_R_HSCALE_LO_F2_5				0x978	//0x25E
#define TW6869_R_HSCALE_LO_F2_6				0x9B8	//0x26E
#define TW6869_R_HSCALE_LO_F2_7				0x9F8	//0x27E
#define TW6869_R_F2_CNT_4					0x93C	//0x24F
#define TW6869_R_F2_CNT_5					0x97C	//0x25F
#define TW6869_R_F2_CNT_6					0x9BC	//0x26F
#define TW6869_R_F2_CNT_7					0x9FC	//0x27F
#define TW6869_R_VERTICAL_CTRL				0xA3C	//0x28F
#define TW6869_R_LOOP_CTRL					0xA54	//0x295
#define TW6869_R_HFLT1						0xAA0	//0x2A8
#define TW6869_R_HFLT2						0xAA4	//0x2A9
#define TW6869_R_AUDIO_GAIN_4				0xB40	//0x2D0
#define TW6869_R_AUDIO_GAIN_5				0xB44	//0x2D1
#define TW6869_R_AUDIO_GAIN_6				0xB48	//0x2D2
#define TW6869_R_AUDIO_GAIN_7				0xB4C	//0x2D3

#endif // TW686XREG_H_INCLUDED
