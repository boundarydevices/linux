/*
 * include/linux/amlogic/media/sound/audin_regs.h
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

#ifndef _AML_AUDIN_REGS_H
#define _AML_AUDIN_REGS_H

#define AUDIN_SPDIF_MODE		0x2800
#define AUDIN_SPDIF_FS_CLK_RLTN	0x2801
#define AUDIN_SPDIF_CHNL_STS_A	0x2802
#define AUDIN_SPDIF_CHNL_STS_B	0x2803
#define AUDIN_SPDIF_MISC		0x2804
#define AUDIN_SPDIF_NPCM_PCPD	0x2805
#define AUDIN_SPDIF_ENHANCE_CNTL 0x2806
#define AUDIN_SPDIF_CHNL_STS_AB0 0x2807
#define AUDIN_SPDIF_CHNL_STS_AB1 0x2808
#define AUDIN_SPDIF_CHNL_STS_AB2 0x2809
#define AUDIN_SPDIF_CHNL_STS_AB3 0x280a
#define AUDIN_SPDIF_CHNL_STS_AB4 0x280b
#define AUDIN_SPDIF_CHNL_STS_AB5 0x280c
#define AUDIN_SPDIF_END			0x280f
#define AUDIN_I2SIN_CTRL		0x2810
#define AUDIN_SOURCE_SEL		0x2811
#define AUDIN_DECODE_FORMAT		0x2812
#define AUDIN_DECODE_CONTROL_STATUS		0x2813
#define AUDIN_DECODE_CHANNEL_STATUS_A_0	0x2814
#define AUDIN_DECODE_CHANNEL_STATUS_A_1	0x2815
#define AUDIN_DECODE_CHANNEL_STATUS_A_2	0x2816
#define AUDIN_DECODE_CHANNEL_STATUS_A_3	0x2817
#define AUDIN_DECODE_CHANNEL_STATUS_A_4	0x2818
#define AUDIN_DECODE_CHANNEL_STATUS_A_5	0x2819
#define AUDIN_FIFO0_START	0x2820
#define AUDIN_FIFO0_END		0x2821
#define AUDIN_FIFO0_PTR		0x2822
#define AUDIN_FIFO0_INTR	0x2823
#define AUDIN_FIFO0_RDPTR	0x2824
#define AUDIN_FIFO0_CTRL	0x2825
#define AUDIN_FIFO0_CTRL1	0x2826
#define AUDIN_FIFO0_LVL0	0x2827
#define AUDIN_FIFO0_LVL1	0x2828
#define AUDIN_FIFO0_LVL2	0x2829
#define AUDIN_FIFO0_REQID	0x2830
#define AUDIN_FIFO0_WRAP	0x2831
#define AUDIN_FIFO1_START	0x2833
#define AUDIN_FIFO1_END		0x2834
#define AUDIN_FIFO1_PTR		0x2835
#define AUDIN_FIFO1_INTR	0x2836
#define AUDIN_FIFO1_RDPTR	0x2837
#define AUDIN_FIFO1_CTRL	0x2838
#define AUDIN_FIFO1_CTRL1	0x2839
#define AUDIN_FIFO1_LVL0	0x2840
#define AUDIN_FIFO1_LVL1	0x2841
#define AUDIN_FIFO1_LVL2	0x2842
#define AUDIN_FIFO1_REQID	0x2843
#define AUDIN_FIFO1_WRAP	0x2844
#define AUDIN_FIFO2_START	0x2845
#define AUDIN_FIFO2_END		0x2846
#define AUDIN_FIFO2_PTR		0x2847
#define AUDIN_FIFO2_INTR	0x2848
#define AUDIN_FIFO2_RDPTR	0x2849
#define AUDIN_FIFO2_CTRL	0x284a
#define AUDIN_FIFO2_CTRL1	0x284b
#define AUDIN_FIFO2_LVL0	0x284c
#define AUDIN_FIFO2_LVL1	0x284d
#define AUDIN_FIFO2_LVL2	0x284e
#define AUDIN_FIFO2_REQID	0x284f
#define AUDIN_FIFO2_WRAP	0x2850
#define AUDIN_INT_CTRL		0x2851
#define AUDIN_FIFO_INT		0x2852
#define PCMIN_CTRL0		0x2860
#define PCMIN_CTRL1		0x2861
#define PCMIN1_CTRL0	0x2862
#define PCMIN1_CTRL1	0x2863
#define PCMOUT_CTRL0	0x2870
#define PCMOUT_CTRL1	0x2871
#define PCMOUT_CTRL2	0x2872
#define PCMOUT_CTRL3	0x2873
#define PCMOUT1_CTRL0	0x2874
#define PCMOUT1_CTRL1	0x2875
#define PCMOUT1_CTRL2	0x2876
#define PCMOUT1_CTRL3	0x2877
#define AUDOUT_CTRL		0x2880
#define AUDOUT_CTRL1	0x2881
#define AUDOUT_BUF0_STA		0x2882
#define AUDOUT_BUF0_EDA		0x2883
#define AUDOUT_BUF0_WPTR	0x2884
#define AUDOUT_BUF1_STA		0x2885
#define AUDOUT_BUF1_EDA		0x2886
#define AUDOUT_BUF1_WPTR	0x2887
#define AUDOUT_FIFO_RPTR	0x2888
#define AUDOUT_INTR_PTR		0x2889
#define AUDOUT_FIFO_STS		0x288a
#define AUDOUT1_CTRL		0x2890
#define AUDOUT1_CTRL1		0x2891
#define AUDOUT1_BUF0_STA	0x2892
#define AUDOUT1_BUF0_EDA	0x2893
#define AUDOUT1_BUF0_WPTR	0x2894
#define AUDOUT1_BUF1_STA	0x2895
#define AUDOUT1_BUF1_EDA	0x2896
#define AUDOUT1_BUF1_WPTR	0x2897
#define AUDOUT1_FIFO_RPTR	0x2898
#define AUDOUT1_INTR_PTR	0x2899
#define AUDOUT1_FIFO_STS	0x289a
#define AUDIN_HDMI_MEAS_CTRL			0x28a0
#define AUDIN_HDMI_MEAS_CYCLES_M1		0x28a1
#define AUDIN_HDMI_MEAS_INTR_MASKN		0x28a2
#define AUDIN_HDMI_MEAS_INTR_STAT		0x28a3
#define AUDIN_HDMI_REF_CYCLES_STAT_0	0x28a4
#define AUDIN_HDMI_REF_CYCLES_STAT_1	0x28a5
#define AUDIN_HDMIRX_AFIFO_STAT			0x28a6
#define AUDIN_FIFO0_PIO_STS		0x28b0
#define AUDIN_FIFO0_PIO_RDL		0x28b1
#define AUDIN_FIFO0_PIO_RDH		0x28b2
#define AUDIN_FIFO1_PIO_STS		0x28b3
#define AUDIN_FIFO1_PIO_RDL		0x28b4
#define AUDIN_FIFO1_PIO_RDH		0x28b5
#define AUDIN_FIFO2_PIO_STS		0x28b6
#define AUDIN_FIFO2_PIO_RDL		0x28b7
#define AUDIN_FIFO2_PIO_RDH		0x28b8
#define AUDOUT_FIFO_PIO_STS		0x28b9
#define AUDOUT_FIFO_PIO_WRL		0x28ba
#define AUDOUT_FIFO_PIO_WRH		0x28bb
#define AUDOUT1_FIFO_PIO_STS	0x28bc
#define AUDOUT1_FIFO_PIO_WRL	0x28bd
#define AUDOUT1_FIFO_PIO_WRH	0x28be
#define AUD_RESAMPLE_CTRL0		0x28bf
#define AUD_RESAMPLE_CTRL1		0x28c0
#define AUD_RESAMPLE_STATUS		0x28c1
#define AUD_RESAMPLE_CTRL2		0x28c2
#define AUD_RESAMPLE_COEF0		0x28c3
#define AUD_RESAMPLE_COEF1		0x28c4
#define AUD_RESAMPLE_COEF2		0x28c5
#define AUD_RESAMPLE_COEF3		0x28c6
#define AUD_RESAMPLE_COEF4		0x28c7
#define AUDIN_ATV_DEMOD_CTRL		0x28d0
#define AUDIN_HDMIRX_PAO_CTRL		0x28d1
#define AUDIN_HDMIRX_PAO_PCPD		0x28d2

#define AUDIN_ADDR_END			0x28d2

/* I2S CLK and LRCLK direction. 0 : input 1 : output. */
#define I2SIN_DIR			0

/* I2S clk selection : 0 : from pad input. 1 : from AIU. */
#define I2SIN_CLK_SEL		1
#define I2SIN_LRCLK_SEL		2
#define I2SIN_POS_SYNC		3
#define I2SIN_LRCLK_SKEW	4
#define I2SIN_LRCLK_INVT	7

/* 9:8 : 0 16 bit. 1 : 18 bits 2 : 20 bits 3 : 24bits. */
#define I2SIN_SIZE			8
#define I2SIN_CHAN_EN		10
#define I2SIN_EN			15

#define AUDIN_FIFO_EN		0
/* write 1 to load address to AUDIN_FIFO0. */
#define AUDIN_FIFO_LOAD	2
enum data_source {
	SPDIF_IN,
	I2S_IN,
	PCM_IN,
	HDMI_IN,
	PAO_IN,
	ATV_ADEC
};
#define DMIC HDMI_IN
/* MBOX platform*/
       /* 0     spdifIN */
       /* 1     i2Sin */
       /* 2     PCMIN */
       /* 3     Dmic in */
/* TV platform*/
       /* 0     spdifIN */
       /* 1     i2Sin */
       /* 2     PCMIN */
       /* 3     HDMI in */
       /* 4     DEMODULATOR IN */
       /* 5     atv_dmd or adec resample*/
#define AUDIN_FIFO_DIN_SEL	3
/* 10:8   data endian control. */
#define AUDIN_FIFO_ENDIAN	8
/* 14:11   channel number. */
#define AUDIN_FIFO_CHAN	11
/* urgent request enable. */
#define AUDIN_FIFO_UG		15

#endif /* _AML_AUDIN_REGS_H */
