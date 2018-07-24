#ifndef _CS42528_H
#define _CS42528_H

struct cs42528_platform_data {
	int reset_pin;
	int mute_pin;
};

/* CS42888 register map */
#define CS42528_CHIPID		0x01	/* Chip ID */
#define CS42528_PWRCTL		0x02	/* Power Control */
#define CS42528_FUNCMOD		0x03	/* Functional Mode */
#define CS42528_INTF		0x04	/* Interface Formats */
#define CS42528_MISCCTL		0x05	/* Misc Control */
#define CS42528_CLKCTL		0x06	/* Clock Control */
#define CS42528_CLKRATIO	0x07	/* OMCK/PLL_CLK Ratio */
#define CS42528_RVCRSTA		0x08	/* RVCR Status */

#define CS42528_BPPCB0		0x09	/* Burst Preamble PC Byte 0 */
#define CS42528_BPPCB1		0x0A	/* Burst Preamble PC Byte 1 */
#define CS42528_BPPDB0		0x0B	/* Burst Preamble PD Byte 0 */
#define CS42528_BPPDB1		0x0C	/* Burst Preamble PD Byte 1 */

#define CS42528_TXCTL		0x0D	/* Volume Transition Control */
#define CS42528_DACMUTE		0x0E	/* Channel Mute */

#define CS42528_VOLCTLA1	0x0F	/* Volume Control A1 */
#define CS42528_VOLCTLB1	0x10	/* Volume Control B1 */
#define CS42528_VOLCTLA2	0x11	/* Volume Control A2 */
#define CS42528_VOLCTLB2	0x12	/* Volume Control B2 */
#define CS42528_VOLCTLA3	0x13	/* Volume Control A3 */
#define CS42528_VOLCTLB3	0x14	/* Volume Control B3 */
#define CS42528_VOLCTLA4	0x15	/* Volume Control A4 */
#define CS42528_VOLCTLB4	0x16	/* Volume Control B4 */

#define CS42528_CHINV		0x17	/* Channel invert */

#define CS42528_MIXCTLP1	0x18	/* Mixing Ctrl Pair 1 */
#define CS42528_MIXCTLP2	0x19	/* Mixing Ctrl Pair 2 */
#define CS42528_MIXCTLP3	0x1A	/* Mixing Ctrl Pair 3 */
#define CS42528_MIXCTLP4	0x1B	/* Mixing Ctrl Pair 4 */

#define CS42528_ADCLGAIN	0x1C	/* ADC Left Ch. Gain */
#define CS42528_ADCRGAIN	0x1D	/* ADC Right Ch. Gain */

#define CS42528_RCVRCTL1	0x1E	/* RVCR Mode Ctrl 1 */
#define CS42528_RCVRCTL2	0x1F	/* RVCR Mode Ctrl 2 */
#define CS42528_MUTECTRL	0x28	/* MuteC Pin Control */

#define CS42528_FIRSTREG	CS42528_CHIPID
#define CS42528_LASTREG		CS42528_MUTECTRL
#define CS42528_NUMREGS		(CS42528_LASTREG - CS42528_FIRSTREG + 1)
#define CS42528_I2C_INCR	0x80

/* Chip I.D. and Revision Register (Address 01h) */
#define CS42528_CHIPID_CHIP_ID_MASK		0xF0
#define CS42528_CHIPID_REV_ID_MASK		0x0F

/* Power Control (Address 02h) */
#define CS42528_PWRCTL_PDN_RCVR1_MASK	(1 << 7)
#define CS42528_PWRCTL_PDN_RCVR1		(1 << 7)
#define CS42528_PWRCTL_PDN_RCVR0_MASK	(1 << 6)
#define CS42528_PWRCTL_PDN_RCVR0		(1 << 6)
#define CS42528_PWRCTL_PDN_ADC_MASK		(1 << 5)
#define CS42528_PWRCTL_PDN_ADC			(1 << 5)
#define CS42528_PWRCTL_PDN_DAC4_MASK	(1 << 4)
#define CS42528_PWRCTL_PDN_DAC4			(1 << 4)
#define CS42528_PWRCTL_PDN_DAC3_MASK	(1 << 3)
#define CS42528_PWRCTL_PDN_DAC3			(1 << 3)
#define CS42528_PWRCTL_PDN_DAC2_MASK	(1 << 2)
#define CS42528_PWRCTL_PDN_DAC2			(1 << 2)
#define CS42528_PWRCTL_PDN_DAC1_MASK	(1 << 1)
#define CS42528_PWRCTL_PDN_DAC1			(1 << 1)
#define CS42528_PWRCTL_PDN_MASK			(1 << 0)
#define CS42528_PWRCTL_PDN				(1 << 0)

/* Interface Formats (Address 04h) */
#define CS42528_INTF_DAC_DIF_MASK		(3 << 6)
#define CS42528_INTF_DAC_DIF_LEFTJ		(0 << 6)
#define CS42528_INTF_DAC_DIF_I2S		(1 << 6)
#define CS42528_INTF_DAC_DIF_RIGHTJ		(2 << 6)

/* Channel Mute */
#define CS42528_DACMUTE_ALL	0xFF

#endif /* _CS42528_H */
