#ifndef AUD_DEMOD_REG_H

#define AUD_DEMOD_REG_H

#define ADEC_CTRL                 0x010
#define FREQ0_CTRL                0x011
#define FREQ1_CTRL                0x012
#define ID_FREQ_STEREO_CTRL       0x013
#define ID_FREQ_DUAL_CTRL         0x014
#define ID_FREQ_CARRIER_CTRL      0x015
#define DEMOD_GAIN_CTRL           0x016
#define BTSC_BYPASS_CTRL          0x017
#define EXPANDER_SPEC_ADJ         0x018
#define EXPANDER_GAIN_ADJ         0x019
#define LMR_GAIN_ADJ              0x01a
#define SAP_GAIN_ADJ              0x01b
#define LPR_GAIN_ADJ              0x01c
#define BTSC_STEREO_THD           0x01d
#define DELAY_COMP_CRTL           0x01e
#define EXPANDER_B_CTRL           0x01f
#define EXPANDER_B2C_ADJ          0x020
#define TEST_SEL_CTRL             0x021
#define IIR_SPEED_CTRL            0x022
#define STEREO_DET_THD            0x023
#define DUAL_DET_THD              0x024
#define SAP_DET_THD               0x025
#define MODE_DET_CNT_THD          0x026
#define ADEC_RESET                0x027

#define DDC_FIR_COEF0_0           0x030
#define DDC_FIR_COEF0_1           0x031
#define DDC_FIR_COEF0_2           0x032
#define DDC_FIR_COEF0_3           0x033
#define DDC_FIR_COEF0_4           0x034
#define DDC_FIR_COEF0_5           0x035
#define DDC_FIR_COEF0_6           0x036
#define DDC_FIR_COEF0_7           0x037
#define DDC_FIR_COEF0_8           0x038
#define DDC_FIR_COEF0_9           0x039
#define DDC_FIR_COEF0_10          0x03a
#define DDC_FIR_COEF0_11          0x03b
#define DDC_FIR_COEF0_12          0x03c
#define DDC_FIR_COEF0_13          0x03d
#define DDC_FIR_COEF0_14          0x03e
#define DDC_FIR_COEF0_15          0x03f
#define DDC_FIR_COEF0_16          0x040
#define DDC_FIR_COEF1_0           0x041
#define DDC_FIR_COEF1_1           0x042
#define DDC_FIR_COEF1_2           0x043
#define DDC_FIR_COEF1_3           0x044
#define DDC_FIR_COEF1_4           0x045
#define DDC_FIR_COEF1_5           0x046
#define DDC_FIR_COEF1_6           0x047
#define DDC_FIR_COEF1_7           0x048
#define DDC_FIR_COEF1_8           0x049
#define DDC_FIR_COEF1_9           0x04a
#define DDC_FIR_COEF1_10          0x04b
#define DDC_FIR_COEF1_11          0x04c
#define DDC_FIR_COEF1_12          0x04d
#define DDC_FIR_COEF1_13          0x04e
#define DDC_FIR_COEF1_14          0x04f
#define DDC_FIR_COEF1_15          0x050
#define DDC_FIR_COEF1_16          0x051
#define FILTER_00_0               0x052
#define FILTER_00_1               0x053
#define FILTER_00_2               0x054
#define FILTER_01_0               0x055
#define FILTER_01_1               0x056
#define FILTER_01_2               0x057
#define FILTER_02_0               0x058
#define FILTER_02_1               0x059
#define FILTER_02_2               0x05a
#define FILTER_03_0               0x05b
#define FILTER_03_1               0x05c
#define FILTER_03_2               0x05d
#define FILTER_04_0               0x05e
#define FILTER_04_1               0x05f
#define FILTER_04_2               0x060
#define FILTER_05_0               0x061
#define FILTER_05_1               0x062
#define FILTER_05_2               0x063
#define FILTER_06_0               0x064
#define FILTER_06_1               0x065
#define FILTER_06_2               0x066
#define FILTER_07_0               0x067
#define FILTER_07_1               0x068
#define FILTER_07_2               0x069
#define FILTER_08_0               0x06a
#define FILTER_08_1               0x06b
#define FILTER_08_2               0x06c
#define FILTER_09_0               0x06d
#define FILTER_09_1               0x06e
#define FILTER_09_2               0x06f
#define FILTER_10_0               0x070
#define FILTER_10_1               0x071
#define FILTER_10_2               0x072
#define FILTER_11_0               0x073
#define FILTER_11_1               0x074
#define FILTER_11_2               0x075
#define FILTER_12_0               0x076
#define FILTER_12_1               0x077
#define FILTER_12_2               0x078
#define FILTER_13_0               0x079
#define FILTER_13_1               0x07a
#define FILTER_13_2               0x07b
#define FILTER_14_0               0x07c
#define FILTER_14_1               0x07d
#define FILTER_14_2               0x07e
#define FILTER_15_0               0x07f
#define FILTER_15_1               0x080
#define FILTER_15_2               0x081

#define DBX_FILTER_01_0           0x085
#define DBX_FILTER_01_1           0x086
#define DBX_FILTER_01_2           0x087
#define DBX_FILTER_02_0           0x088
#define DBX_FILTER_02_1           0x089
#define DBX_FILTER_02_2           0x08a
#define DBX_FILTER_03_0           0x08b
#define DBX_FILTER_03_1           0x08c
#define DBX_FILTER_03_2           0x08d
#define DBX_FILTER_04_0           0x08e
#define DBX_FILTER_04_1           0x08f
#define DBX_FILTER_04_2           0x090
#define DBX_FILTER_05_0           0x091
#define DBX_FILTER_05_1           0x092
#define DBX_FILTER_05_2           0x093
#define DBX_FILTER_06_0           0x094
#define DBX_FILTER_06_1           0x095
#define DBX_FILTER_06_2           0x096
#define DBX_FILTER_07_0           0x097
#define DBX_FILTER_07_1           0x098
#define DBX_FILTER_07_2           0x099
#define DBX_FILTER_08_0           0x09a
#define DBX_FILTER_08_1           0x09b
#define DBX_FILTER_08_2           0x09c

#define BB_COEF0_A_0              0x0a0
#define BB_COEF0_A_1              0x0a1
#define BB_COEF0_A_2              0x0a2
#define BB_COEF0_A_3              0x0a3
#define BB_COEF1_A_0              0x0a4
#define BB_COEF1_A_1              0x0a5
#define BB_COEF1_A_2              0x0a6
#define BB_COEF1_A_3              0x0a7
#define BB_COEF2_A_0              0x0a8
#define BB_COEF2_A_1              0x0a9
#define BB_COEF2_A_2              0x0aa
#define BB_COEF2_A_3              0x0ab
#define BB_COEF0_B_0              0x0ac
#define BB_COEF0_B_1              0x0ad
#define BB_COEF0_B_2              0x0ae
#define BB_COEF0_B_3              0x0af
#define BB_COEF1_B_0              0x0b0
#define BB_COEF1_B_1              0x0b1
#define BB_COEF1_B_2              0x0b2
#define BB_COEF1_B_3              0x0b3
#define BB_COEF2_B_0              0x0b4
#define BB_COEF2_B_1              0x0b5
#define BB_COEF2_B_2              0x0b6
#define BB_COEF2_B_3              0x0b7
#define MUTE_THD0                 0x0b8
#define MUTE_THD1                 0x0b9
#define MUTE_THD2                 0x0ba
#define NOISE_THD0                0x0bb
#define NOISE_THD1                0x0bc
#define NOISE_THD2                0x0bd
#define BB_SPEED                  0x0be
#define SRC_CTRL0                 0x0c0
#define SRC_CTRL1                 0x0c1
#define SRC_CTRL2                 0x0c2
#define SRC_CTRL3                 0x0c3
#define SRC_CTRL4                 0x0c4
#define SRC_CTRL5                 0x0c5
#define SRC_CTRL6                 0x0c6
#define SRC_CTRL7                 0x0c7
#define SRC_CTRL8                 0x0c8
#define SRC_CTRL9                 0x0c9
#define THD_P0                    0x0d0
#define THD_M0                    0x0d1
#define THD_OV0                   0x0d2
#define THD_CNT0                  0x0d3
#define CNT_MAX0                  0x0d4
#define THD_P1                    0x0d5
#define THD_M1                    0x0d6
#define THD_OV1                   0x0d7
#define THD_CNT1                  0x0d8
#define CNT_MAX1                  0x0d9

#define OV_CNT_REPORT             0x0da
#define OV_FLAG_REPORT            0x0db

#define PILOT_BIAS_REPORT         0x0f0
#define STEREO_LEVEL_REPORT       0x0f1
#define SNR_REPORT                0x0f2
#define SNR_SAP_REPORT            0x0f3
#define POWER_REPORT              0x0f4
#define DC_REPORT                 0x0f5
#define CARRIER_MAG_REPORT        0x0f6
#define BTSC_AB_REPORT            0x0f7
#define AUDIO_MODE_REPORT         0x0f8

#define CTRL_SEL            0x0ff

#define	   FCLK    32e6
#define    ADDR_ADEC_CTRL             (ADEC_CTRL)

#define    ADDR_BTSC_BYPASS_CTRL     (BTSC_BYPASS_CTRL)
#define    ADDR_EXPANDER_SPECTRAL_ADJ (EXPANDER_SPEC_ADJ)
#define    ADDR_EXPANDER_GAIN_ADJ    (EXPANDER_GAIN_ADJ)
#define    ADDR_LMR_ADJ              (LMR_GAIN_ADJ)
#define    ADDR_SAP_ADJ              (SAP_GAIN_ADJ)
#define    ADDR_LPR_GAIN_ADJ         (LPR_GAIN_ADJ)
#define    ADDR_STEREO_THRESHOLD     (BTSC_STEREO_THD)
#define    ADDR_LPR_COMP_CTRL        (DELAY_COMP_CRTL)
#define    ADDR_EXPANDER_B_CTRL      (EXPANDER_B_CTRL)
#define    ADDR_EXPANDER_B2C_ADJ     (EXPANDER_B2C_ADJ)
#define    ADDR_TEST_SEL_CTRL        (TEST_SEL_CTRL)

#define    ADDR_SAP_FIR_COEF     (DDC_FIR_COEF1_0)

#define	 ADDR_SAP_DTO         (FREQ1_CTRL)

#define	 ADDR_DDC_FREQ0         (FREQ0_CTRL)
#define	 ADDR_DDC_FREQ1         (FREQ1_CTRL)

#define		ADDR_DEMOD_GAIN			(DEMOD_GAIN_CTRL)


#define	 ADDR_DDC_FIR0_COEF         (DDC_FIR_COEF0_0)
#define	 ADDR_DDC_FIR1_COEF         (DDC_FIR_COEF1_0)
#define	 ADDR_BB_FIR0_A_COEF         (BB_COEF0_A_0)
#define	 ADDR_BB_FIR1_A_COEF         (BB_COEF1_A_0)
#define	 ADDR_BB_FIR2_A_COEF         (BB_COEF2_A_0)

#define	 ADDR_BB_FIR0_B_COEF         (BB_COEF0_B_0)
#define	 ADDR_BB_FIR1_B_COEF         (BB_COEF0_B_1)
#define	 ADDR_BB_FIR2_B_COEF         (BB_COEF0_B_2)

#define    ADDR_BB_MUTE_THRESHOLD0   (MUTE_THD0)
#define    ADDR_BB_MUTE_THRESHOLD1   (MUTE_THD1)
#define    ADDR_BB_MUTE_THRESHOLD2   (MUTE_THD2)

#define    ADDR_BB_NOISE_THRESHOLD0   (NOISE_THD0)
#define    ADDR_BB_NOISE_THRESHOLD1   (NOISE_THD1)
#define    ADDR_BB_NOISE_THRESHOLD2   (NOISE_THD2)

#define	 ADDR_INDICATOR_STEREO_DTO  (ID_FREQ_STEREO_CTRL)
#define  ADDR_INDICATOR_DUAL_DTO	(ID_FREQ_DUAL_CTRL)

#define  ADDR_INDICATOR_CENTER_DTO  (ID_FREQ_CARRIER_CTRL)

#define  ADDR_IIR_LPR_15K_0	(FILTER_00_0)
#define  ADDR_IIR_LPR_15K_1	(FILTER_01_0)
#define  ADDR_IIR_LPR_15K_2	(FILTER_02_0)
#define  ADDR_IIR_LPR_15K_3	(FILTER_03_0)
#define  ADDR_IIR_LPR_15K_4	(FILTER_04_0)

#define  ADDR_IIR_LMR_15K_0	(FILTER_05_0)
#define  ADDR_IIR_LMR_15K_1	(FILTER_06_0)
#define  ADDR_IIR_LMR_15K_2	(FILTER_07_0)
#define  ADDR_IIR_LMR_15K_3	(FILTER_08_0)
#define  ADDR_IIR_LMR_15K_4	(FILTER_09_0)

#define  ADDR_IIR_LPR_DEEMPHASIS	(FILTER_10_0)
#define  ADDR_IIR_PILOT_0	(FILTER_11_0)
#define  ADDR_IIR_PILOT_1	(FILTER_12_0)
#define  ADDR_IIR_PILOT_2	(FILTER_13_0)
#define  ADDR_IIR_LPR_COMP	(FILTER_14_0)
#define  ADDR_IIR_LMR_DEEMPHASIS	(FILTER_15_0)

#define  ADDR_IIR_FIXED_DEEM_0	(DBX_FILTER_01_0)
#define  ADDR_IIR_FIXED_DEEM_1	(DBX_FILTER_02_0)
#define  ADDR_IIR_FIXED_DEEM_2	(DBX_FILTER_03_0)

#define  ADDR_IIR_Q_FILTER_0	(DBX_FILTER_04_0)
#define  ADDR_IIR_Q_FILTER_1	(DBX_FILTER_05_0)
#define  ADDR_IIR_Q_FILTER_2	(DBX_FILTER_06_0)
#define  ADDR_IIR_P_FILTER_0	(DBX_FILTER_07_0)
#define  ADDR_IIR_P_FILTER_1	(DBX_FILTER_08_0)

#define  ADDR_IIR_SPEED_CTRL    (IIR_SPEED_CTRL)
#define  ADDR_SEL_CTRL	(CTRL_SEL)


#define	AUDIO_STANDARD_BTSC				0x00
#define	AUDIO_STANDARD_EIAJ				0x01
#define	AUDIO_STANDARD_A2_K				0x02
#define	AUDIO_STANDARD_A2_BG				0x03
#define	AUDIO_STANDARD_A2_DK1				0x04
#define	AUDIO_STANDARD_A2_DK2			0x05
#define	AUDIO_STANDARD_A2_DK3			0x06
#define	AUDIO_STANDARD_NICAM_I			0x07
#define AUDIO_STANDARD_NICAM_BG			0x08
#define	AUDIO_STANDARD_NICAM_L			0x09
#define	AUDIO_STANDARD_NICAM_DK			0x0A
#define AUDIO_STANDARD_FM_USA			0x0B
#define	AUDIO_STANDARD_FM_EU			0x0C
#define AUDIO_STANDARD_CHINA			0x0E
#define	AUDIO_STANDARD_INDIAN		0x0F
#define	AUDIO_STANDARD_BTSC_SA		0x10
#define AUDIO_STANDARD_MONO_ONLY		0x11

#define AUDIO_OUTMODE_MONO 0
#define AUDIO_OUTMODE_STEREO 1
#define AUDIO_OUTMODE_SAP_DUAL 2


#define AUDIO_OUTMODE_BTSC_MONO 0
#define AUDIO_OUTMODE_BTSC_STEREO 1
#define AUDIO_OUTMODE_BTSC_SAP 2

#define AUDIO_OUTMODE_EIAJ_MONO 0
#define AUDIO_OUTMODE_EIAJ_STEREO 1
#define AUDIO_OUTMODE_EIAJ_MAIN 2
#define AUDIO_OUTMODE_EIAJ_SUB 3
#define AUDIO_OUTMODE_EIAJ_MAIN_SUB 4

#define AUDIO_OUTMODE_A2_MONO 0
#define AUDIO_OUTMODE_A2_STEREO 1
#define AUDIO_OUTMODE_A2_DUAL_A 2
#define AUDIO_OUTMODE_A2_DUAL_B 3
#define AUDIO_OUTMODE_A2_DUAL_AB 4

#define AUDIO_OUTMODE_NICAM_MONO 0
#define AUDIO_OUTMODE_NICAM_MONO1 1
#define AUDIO_OUTMODE_NICAM_STEREO 2
#define AUDIO_OUTMODE_NICAM_DUAL_A 3
#define AUDIO_OUTMODE_NICAM_DUAL_B 4
#define AUDIO_OUTMODE_NICAM_DUAL_AB 5

#define AUDIO_DEEM_75US 0
#define AUDIO_DEEM_50US 1
#define AUDIO_DEEM_J17  2


#endif
