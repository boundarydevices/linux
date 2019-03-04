/*
 * drivers/amlogic/clk/clk_measure.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/clk-provider.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/clk_measure.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/spinlock.h>
#include <linux/of_device.h>

#undef pr_fmt
#define pr_fmt(fmt) "clkmsr: " fmt

void __iomem *msr_clk_reg0;
void __iomem *msr_clk_reg2;
void __iomem *msr_clk_reg3;
void __iomem *msr_ring_reg0;

static DEFINE_SPINLOCK(clk_measure_lock);

#define CLKMSR_DEVICE_NAME	"clkmsr"
unsigned int clk_msr_index = 0xff;

struct meson_clkmsr_data {
	const char * const *clk_table;
	unsigned int table_size;
	unsigned int (*clk_msr_function)(unsigned int clk_mux);

/* sentinel maybe new diference between deferent SoCs */
};

static struct meson_clkmsr_data *clk_data;

static unsigned int m8b_clk_util_clk_msr(unsigned int clk_mux)
{
	unsigned int msr;
	unsigned int regval = 0;
	unsigned int val;

	writel_relaxed(0, msr_clk_reg0);
	/* Set the measurement gate to 64uS */
	val = readl_relaxed(msr_clk_reg0);
	val = (val & (~0xFFFF)) | (64-1);
	writel_relaxed(val, msr_clk_reg0);

	/* Disable continuous measurement */
	/* disable interrupts */
	val = readl_relaxed(msr_clk_reg0);
	val = val & (~((1<<18)|(1<<17)));
	writel_relaxed(val, msr_clk_reg0);
	val = readl_relaxed(msr_clk_reg0);
	val = (val & (~(0x1f<<20))) | (clk_mux<<20)|(1<<19)|(1<<16);
	writel_relaxed(val, msr_clk_reg0);

	/* Wait for the measurement to be done */
	do {
		regval = readl_relaxed(msr_clk_reg0);
	} while (regval & (1 << 31));
	/* disable measuring */
	val = readl_relaxed(msr_clk_reg0);
	val = val & (~(1<<16));
	writel_relaxed(val, msr_clk_reg0);

	msr = (readl_relaxed(msr_clk_reg2)+31)&0x000FFFFF;
	/* Return value in MHz*measured_val */
	return (msr>>6)*1000000;
}

static unsigned int gxbb_clk_util_clk_msr(unsigned int clk_mux)
{
	unsigned int  msr;
	unsigned int regval = 0;
	unsigned int val;

	writel_relaxed(0, msr_clk_reg0);
    /* Set the measurement gate to 50uS */
	val = readl_relaxed(msr_clk_reg0);
	val = (val & (~0xFFFF)) | (64-1);
	writel_relaxed(val, msr_clk_reg0);
    /* Disable continuous measurement */
    /* disable interrupts */
	val = readl_relaxed(msr_clk_reg0);
	val = val & (~((1<<18)|(1<<17)));
	writel_relaxed(val, msr_clk_reg0);
	val = readl_relaxed(msr_clk_reg0);
	val = (val & (~(0x7f<<20))) | (clk_mux<<20)|(1<<19)|(1<<16);
	writel_relaxed(val, msr_clk_reg0);
    /* Wait for the measurement to be done */
	do {
		regval = readl_relaxed(msr_clk_reg0);
	} while (regval & (1 << 31));
    /* disable measuring */
	val = readl_relaxed(msr_clk_reg0);
	val = val & (~(1<<16));
	msr = (readl_relaxed(msr_clk_reg2)+31)&0x000FFFFF;
    /* Return value in MHz*measured_val */
	return (msr>>6)*1000000;

}

static unsigned int meson_clk_util_ring_msr(unsigned int clk_mux)
{
	unsigned int  msr;
	unsigned int regval = 0;
	unsigned int val;

	writel_relaxed(0, msr_clk_reg0);
    /* Set the measurement gate to 50uS */
	val = readl_relaxed(msr_clk_reg0);
	val = (val & (~0xFFFF)) | (10000-1);
	writel_relaxed(val, msr_clk_reg0);
    /* Disable continuous measurement */
    /* disable interrupts */
	val = readl_relaxed(msr_clk_reg0);
	val = val & (~((1<<18)|(1<<17)));
	writel_relaxed(val, msr_clk_reg0);
	val = readl_relaxed(msr_clk_reg0);
	val = (val & (~(0x7f<<20))) | (clk_mux<<20)|(1<<19)|(1<<16);
	writel_relaxed(val, msr_clk_reg0);
    /* Wait for the measurement to be done */
	do {
		regval = readl_relaxed(msr_clk_reg0);
	} while (regval & (1 << 31));
    /* disable measuring */
	val = readl_relaxed(msr_clk_reg0);
	val = val & (~(1<<16));
	msr = (readl_relaxed(msr_clk_reg2)+31)&0x000FFFFF;
    /* Return value in MHz*measured_val */
	return (msr / 10);

}

int m8b_clk_measure(struct seq_file *s, void *what, unsigned int index)
{
	static const char * const clk_table[] = {
		[63] = "CTS_MIPI_CSI_CFG_CLK(63)",
		[62] = "VID2_PLL_CLK(62)        ",
		[61] = "GPIO_CLK(61)            ",
		[60] = "USB_32K_ALT(60)         ",
		[59] = "CTS_HCODEC_CLK(59)      ",
		[58] = "Reserved(58)            ",
		[57] = "Reserved(57)            ",
		[56] = "Reserved(56)            ",
		[55] = "Reserved(55)            ",
		[54] = "Reserved(54)            ",
		[53] = "Reserved(53)            ",
		[52] = "Reserved(52)            ",
		[51] = "Reserved(51)            ",
		[50] = "Reserved(50)            ",
		[49] = "CTS_PWM_E_CLK(49)       ",
		[48] = "CTS_PWM_F_CLK(48)       ",
		[47] = "DDR_DPLL_PT_CLK(47)     ",
		[46] = "CTS_PCM2_SCLK(46)       ",
		[45] = "CTS_PWM_A_CLK(45)       ",
		[44] = "CTS_PWM_B_CLK(44)       ",
		[43] = "CTS_PWM_C_CLK(43)       ",
		[42] = "CTS_PWM_D_CLK(42)       ",
		[41] = "CTS_ETH_RX_TX(41)       ",
		[40] = "CTS_PCM_MCLK(40)        ",
		[39] = "CTS_PCM_SCLK(39)        ",
		[38] = "CTS_VDIN_MEAS_CLK(38)   ",
		[37] = "Reserved(37)            ",
		[36] = "CTS_HDMI_TX_PIXEL_CLK(36)",
		[35] = "CTS_MALI_CLK (35)       ",
		[34] = "CTS_SDHC_SDCLK(34)      ",
		[33] = "CTS_SDHC_RXCLK(33)      ",
		[32] = "CTS_VDAC_CLK(32)        ",
		[31] = "CTS_AUDAC_CLKPI(31)     ",
		[30] = "MPLL_CLK_TEST_OUT(30)   ",
		[29] = "Reserved(29)            ",
		[28] = "CTS_SAR_ADC_CLK(28)     ",
		[27] = "Reserved(27)            ",
		[26] = "SC_CLK_INT(26)          ",
		[25] = "Reserved(25)            ",
		[24] = "LVDS_FIFO_CLK(24)       ",
		[23] = "HDMI_CH0_TMDSCLK(23)    ",
		[22] = "CLK_RMII_FROM_PAD (22)  ",
		[21] = "I2S_CLK_IN_SRC0(21)     ",
		[20] = "RTC_OSC_CLK_OUT(20)     ",
		[19] = "CTS_HDMI_SYS_CLK(19)    ",
		[18] = "A9_CLK_DIV16(18)        ",
		[17] = "Reserved(17)            ",
		[16] = "CTS_FEC_CLK_2(16)       ",
		[15] = "CTS_FEC_CLK_1 (15)      ",
		[14] = "CTS_FEC_CLK_0 (14)      ",
		[13] = "CTS_AMCLK(13)           ",
		[12] = "Reserved(12)            ",
		[11] = "CTS_ETH_RMII(11)        ",
		[10] = "Reserved(10)            ",
		[9] = "CTS_ENCL_CLK(9)          ",
		[8] = "CTS_ENCP_CLK(8)          ",
		[7] = "CLK81(7)                 ",
		[6] = "VID_PLL_CLK(6)           ",
		[5] = "Reserved(5)              ",
		[4] = "Reserved(4)              ",
		[3] = "A9_RING_OSC_CLK(3)       ",
		[2] = "AM_RING_OSC_CLK_OUT_EE2(2)",
		[1] = "AM_RING_OSC_CLK_OUT_EE1(1)",
		[0] = "AM_RING_OSC_CLK_OUT_EE0(0)",
	};
	int  i;
	int len = sizeof(clk_table)/sizeof(char *);

	if (index  == 0xff) {
		for (i = 0; i < len; i++)
			seq_printf(s, "[%2d][%10d]%s\n",
				   i, m8b_clk_util_clk_msr(i),
					clk_table[i]);
		return 0;
	}
	seq_printf(s, "[%10d]%s\n", m8b_clk_util_clk_msr(index),
		   clk_table[index]);
	clk_msr_index = 0xff;
	return 0;
}

int gxl_clk_measure(struct seq_file *s, void *what, unsigned int index)
{
	static const char * const clk_table[] = {
		[82] = "Cts_ge2d_clk       ",
		[81] = "Cts_vapbclk        ",
		[80] = "Rng_ring_osc_clk[3]",
		[79] = "Rng_ring_osc_clk[2]",
		[78] = "Rng_ring_osc_clk[1]",
		[77] = "Rng_ring_osc_clk[0]",
		[76] = "cts_aoclk_int      ",
		[75] = "cts_aoclkx2_int    ",
		[74] = "0                  ",
		[73] = "cts_pwm_C_clk      ",
		[72] = "cts_pwm_D_clk      ",
		[71] = "cts_pwm_E_clk      ",
		[70] = "cts_pwm_F_clk      ",
		[69] = "0                  ",
		[68] = "0                  ",
		[67] = "0                  ",
		[66] = "cts_vid_lock_clk   ",
		[65] = "0                  ",
		[64] = "0                  ",
		[63] = "0                  ",
		[62] = "cts_hevc_clk       ",
		[61] = "gpio_clk_msr       ",
		[60] = "alt_32k_clk        ",
		[59] = "cts_hcodec_clk     ",
		[58] = "0                  ",
		[57] = "0					",
		[56] = "0					",
		[55] = "vid_pll_div_clk_out	",
		[54] = "0					",
		[53] = "Sd_emmc_clk_A		",
		[52] = "Sd_emmc_clk_B		",
		[51] = "Cts_nand_core_clk	",
		[50] = "Mp3_clk_out			",
		[49] = "mp2_clk_out			",
		[48] = "mp1_clk_out			",
		[47] = "ddr_dpll_pt_clk		",
		[46] = "cts_vpu_clk			",
		[45] = "cts_pwm_A_clk		",
		[44] = "cts_pwm_B_clk		",
		[43] = "fclk_div5			",
		[42] = "mp0_clk_out			",
		[41] = "eth_rx_clk_or_clk_rmii",
		[40] = "cts_pcm_mclk			",
		[39] = "cts_pcm_sclk			",
		[38] = "cts_vdin_meas_clk			",
		[37] = "cts_clk_i958			",
		[36] = "cts_hdmi_tx_pixel_clk ",
		[35] = "cts_mali_clk			",
		[34] = "0					",
		[33] = "0					",
		[32] = "cts_vdec_clk			",
		[31] = "MPLL_CLK_TEST_OUT	",
		[30] = "0					",
		[29] = "0					",
		[28] = "cts_sar_adc_clk					   ",
		[27] = "0					   ",
		[26] = "sc_clk_int			   ",
		[25] = "0					   ",
		[24] = "0					   ",
		[23] = "HDMI_CLK_TODIG		   ",
		[22] = "eth_phy_ref_clk		   ",
		[21] = "i2s_clk_in_src0		   ",
		[20] = "rtc_osc_clk_out		   ",
		[19] = "cts_hdmitx_sys_clk	   ",
		[18] = "sys_cpu_clk_div16		   ",
		[17] = "sys_pll_div16					   ",
		[16] = "cts_FEC_CLK_2		   ",
		[15] = "cts_FEC_CLK_1		   ",
		[14] = "cts_FEC_CLK_0		   ",
		[13] = "cts_amclk			   ",
		[12] = "Cts_pdm_clk			   ",
		[11] = "rgmii_tx_clk_to_phy	   ",
		[10] = "cts_vdac_clk			   ",
		[9] = "cts_encl_clk			  ",
		[8] = "cts_encp_clk			  ",
		[7] = "clk81					  ",
		[6] = "cts_enci_clk			  ",
		[5] = "0						  ",
		[4] = "gp0_pll_clk			  ",
		[3] = "A53_ring_osc_clk		  ",
		[2] = "am_ring_osc_clk_out_ee[2]",
		[1] = "am_ring_osc_clk_out_ee[1]",
		[0] = "am_ring_osc_clk_out_ee[0]",
	};
	int  i;
	int len = sizeof(clk_table)/sizeof(char *);

	if (index  == 0xff) {
		for (i = 0; i < len; i++)
			seq_printf(s, "[%2d][%10d]%s\n",
				   i, gxbb_clk_util_clk_msr(i),
					clk_table[i]);
		return 0;
	}
	seq_printf(s, "[%10d]%s\n", gxbb_clk_util_clk_msr(index),
		   clk_table[index]);
	clk_msr_index = 0xff;
	return 0;
}
int gxm_clk_measure(struct seq_file *s, void *what, unsigned int index)
{
	static const char * const clk_table[] = {
		[82] = "Cts_ge2d_clk       ",
		[81] = "Cts_vapbclk        ",
		[80] = "Rng_ring_osc_clk[3]",
		[79] = "Rng_ring_osc_clk[2]",
		[78] = "Rng_ring_osc_clk[1]",
		[77] = "Rng_ring_osc_clk[0]",
		[76] = "cts_aoclk_int      ",
		[75] = "cts_aoclkx2_int    ",
		[74] = "0                  ",
		[73] = "cts_pwm_C_clk      ",
		[72] = "cts_pwm_D_clk      ",
		[71] = "cts_pwm_E_clk      ",
		[70] = "cts_pwm_F_clk      ",
		[69] = "0                  ",
		[68] = "0                  ",
		[67] = "0                  ",
		[66] = "cts_vid_lock_clk   ",
		[65] = "0                  ",
		[64] = "0                  ",
		[63] = "0                  ",
		[62] = "cts_hevc_clk       ",
		[61] = "gpio_clk_msr       ",
		[60] = "alt_32k_clk        ",
		[59] = "cts_hcodec_clk     ",
		[58] = "cts_wave420l_bclk                  ",
		[57] = "cts_wave420l_cclk	",
		[56] = "cts_cci_clk			",
		[55] = "vid_pll_div_clk_out	",
		[54] = "0					",
		[53] = "Sd_emmc_clk_A		",
		[52] = "Sd_emmc_clk_B		",
		[51] = "Cts_nand_core_clk	",
		[50] = "Mp3_clk_out			",
		[49] = "mp2_clk_out			",
		[48] = "mp1_clk_out			",
		[47] = "ddr_dpll_pt_clk		",
		[46] = "cts_vpu_clk			",
		[45] = "cts_pwm_A_clk		",
		[44] = "cts_pwm_B_clk		",
		[43] = "fclk_div5			",
		[42] = "mp0_clk_out			",
		[41] = "eth_rx_clk_or_clk_rmii",
		[40] = "cts_pcm_mclk			",
		[39] = "cts_pcm_sclk			",
		[38] = "cts_vdin_meas_clk			",
		[37] = "cts_clk_i958			",
		[36] = "cts_hdmi_tx_pixel_clk ",
		[35] = "cts_mali_clk			",
		[34] = "0					",
		[33] = "0					",
		[32] = "cts_vdec_clk			",
		[31] = "MPLL_CLK_TEST_OUT	",
		[30] = "0					",
		[29] = "0					",
		[28] = "cts_sar_adc_clk					   ",
		[27] = "0					   ",
		[26] = "sc_clk_int			   ",
		[25] = "0					   ",
		[24] = "sys_cpu1_clk_div16			",
		[23] = "HDMI_CLK_TODIG		   ",
		[22] = "eth_phy_ref_clk		   ",
		[21] = "i2s_clk_in_src0		   ",
		[20] = "rtc_osc_clk_out		   ",
		[19] = "cts_hdmitx_sys_clk	   ",
		[18] = "sys_cpu_clk_div16		   ",
		[17] = "sys_pll_div16					   ",
		[16] = "cts_FEC_CLK_2		   ",
		[15] = "cts_FEC_CLK_1		   ",
		[14] = "cts_FEC_CLK_0		   ",
		[13] = "cts_amclk			   ",
		[12] = "Cts_pdm_clk			   ",
		[11] = "mac_eth_tx_clk	   ",
		[10] = "cts_vdac_clk			   ",
		[9] = "cts_encl_clk			  ",
		[8] = "cts_encp_clk			  ",
		[7] = "clk81					  ",
		[6] = "cts_enci_clk			  ",
		[5] = "0						  ",
		[4] = "gp0_pll_clk			  ",
		[3] = "A53_ring_osc_clk		  ",
		[2] = "am_ring_osc_clk_out_ee[2]",
		[1] = "am_ring_osc_clk_out_ee[1]",
		[0] = "am_ring_osc_clk_out_ee[0]",
	};
	int  i;
	int len = sizeof(clk_table)/sizeof(char *);

	if (index  == 0xff) {
		for (i = 0; i < len; i++)
			seq_printf(s, "[%2d][%10d]%s\n",
				   i, gxbb_clk_util_clk_msr(i),
					clk_table[i]);
		return 0;
	}
	seq_printf(s, "[%10d]%s\n", gxbb_clk_util_clk_msr(index),
		   clk_table[index]);
	clk_msr_index = 0xff;
	return 0;
}

int axg_clk_measure(struct seq_file *s, void *what, unsigned int index)
{
	static const char * const clk_table[] = {
		[109] = "audio_locker_in   ",
		[108] = "audio_locker_out  ",
		[107] = "pcie_refclk_p     ",
		[106] = "pcie_refclk_n     ",
		[105] = "audio_mclk_a      ",
		[104] = "audio_mclk_b      ",
		[103] = "audio_mclk_c      ",
		[102] = "audio_mclk_d      ",
		[101] = "audio_mclk_e      ",
		[100] = "audio_mclk_f      ",
		[99] = "audio_sclk_a       ",
		[98] = "audio_sclk_b       ",
		[97] = "audio_sclk_c       ",
		[96] = "audio_sclk_d       ",
		[95] = "audio_sclk_e       ",
		[94] = "audio_sclk_f       ",
		[93] = "audio_lrclk_a      ",
		[92] = "audio_lrclk_b      ",
		[91] = "audio_lrclk_c      ",
		[90] = "audio_lrclk_d      ",
		[89] = "audio_lrclk_e      ",
		[88] = "audio_lrclk_f      ",
		[87] = "audio_spdifint_clk ",
		[86] = "audio_spdifout_clk ",
		[85] = "audio_pdm_sysclk   ",
		[84] = "audio_resample_clk ",
		[83] = "0                  ",
		[82] = "Cts_ge2d_clk       ",
		[81] = "Cts_vapbclk        ",
		[80] = "Rng_ring_osc_clk[3]",
		[79] = "Rng_ring_osc_clk[2]",
		[78] = "Rng_ring_osc_clk[1]",
		[77] = "Rng_ring_osc_clk[0]",
		[76] = "tdmin_lb_sclk      ",
		[75] = "tdmin_lb_lrclk     ",
		[74] = "wifi_beacon        ",
		[73] = "cts_pwm_C_clk      ",
		[72] = "cts_pwm_D_clk      ",
		[71] = "audio_slv_sclk_a   ",
		[70] = "audio_slv_sclk_b   ",
		[69] = "audio_slv_sclk_c   ",
		[68] = "audio_slv_lrclk_a  ",
		[67] = "audio_slv_lrclk_b  ",
		[66] = "audio_slv_lrclk_c  ",
		[65] = "0                  ",
		[64] = "0                  ",
		[63] = "0                  ",
		[62] = "0                  ",
		[61] = "gpio_clk_msr       ",
		[60] = "0                  ",
		[59] = "0                  ",
		[58] = "0                  ",
		[57] = "0                  ",
		[56] = "0                  ",
		[55] = "0	                 ",
		[54] = "0                  ",
		[53] = "0	                 ",
		[52] = "sd_emmc_clk_B		   ",
		[51] = "sd_emmc_clk_C      ",
		[50] = "mp3_clk_out			   ",
		[49] = "mp2_clk_out			   ",
		[48] = "mp1_clk_out			   ",
		[47] = "ddr_dpll_pt_clk		 ",
		[46] = "cts_vpu_clk			   ",
		[45] = "cts_pwm_A_clk		   ",
		[44] = "cts_pwm_B_clk		   ",
		[43] = "fclk_div5			     ",
		[42] = "mp0_clk_out			   ",
		[41] = "mod_eth_rx_clk_rmii",
		[40] = "mod_eth_tx_clk     ",
		[39] = "0	                 ",
		[38] = "0	                 ",
		[37] = "0	                 ",
		[36] = "0	                 ",
		[35] = "0	                 ",
		[34] = "0	                 ",
		[33] = "0	                 ",
		[32] = "0	                 ",
		[31] = "MPLL_CLK_TEST_OUT	 ",
		[30] = "0	                 ",
		[29] = "0	                 ",
		[28] = "Cts_sar_adc_clk		 ",
		[27] = "0	                 ",
		[26] = "0	                 ",
		[25] = "0	                 ",
		[24] = "0	                 ",
		[23] = "mmc_clk            ",
		[22] = "0	                 ",
		[21] = "0	                 ",
		[20] = "rtc_osc_clk_out    ",
		[19] = "0	                 ",
		[18] = "sys_cpu_clk_div16  ",
		[17] = "sys_pll_div16      ",
		[16] = "0	                 ",
		[15] = "0	                 ",
		[14] = "0	                 ",
		[13] = "0	                 ",
		[12] = "0	                 ",
		[11] = "0	                 ",
		[10] = "0	                 ",
		[9] = "cts_encl_clk          ",
		[8] = "0	                 ",
		[7] = "clk81               ",
		[6] = "0	                 ",
		[5] = "gp1_pll_clk         ",
		[4] = "gp0_pll_clk         ",
		[3] = "A53_ring_osc_clk    ",
		[2] = "am_ring_osc_clk_out_ee[2]",
		[1] = "am_ring_osc_clk_out_ee[1]",
		[0] = "am_ring_osc_clk_out_ee[0]",
	};
	int  i;
	int len = sizeof(clk_table)/sizeof(char *);

	if (index  == 0xff) {
		for (i = 0; i < len; i++)
			seq_printf(s, "[%2d][%10d]%s\n",
				   i, gxbb_clk_util_clk_msr(i),
					clk_table[i]);
		return 0;
	}
	seq_printf(s, "[%10d]%s\n", gxbb_clk_util_clk_msr(index),
		   clk_table[index]);
	clk_msr_index = 0xff;
	return 0;
}

int txl_clk_measure(struct seq_file *s, void *what, unsigned int index)
{
	static const char * const clk_table[] = {
		[88] = "hdmirx_vid_clk",
		[87] = "lvds_fifo_clk",
		[86] = "hdmirx_phy_dtb[3]",
		[85] = "hdmirx_phy_dtb[2]",
		[84] = "hdmirx_phy_dtb[1]",
		[83] = "hdmirx_phy_dtb[0]",
		[82] = "Cts_ge2d_clk",
		[81] = "Cts_vapbclk",
		[80] = "Rng_ring_osc_clk[3]",
		[79] = "Rng_ring_osc_clk[2]",
		[78] = "Rng_ring_osc_clk[1]",
		[77] = "Rng_ring_osc_clk[0]",
		[76] = "cts_aoclk_int",
		[75] = "cts_aoclkx2_int",
		[74] = "cts_atv_dmd_vdac_clk",
		[73] = "cts_pwm_C_clk",
		[72] = "cts_pwm_D_clk",
		[71] = "cts_pwm_E_clk",
		[70] = "cts_pwm_F_clk",
		[69] = "Cts_hdcp22_skp",
		[68] = "Cts_hdcp22_esm",
		[67] = "tvfe_sample_clk",
		[66] = "cts_vid_lock_clk",
		[65] = "atv_dmd_sys_clk",
		[64] = "Cts_hdmirx_cfg_clk",
		[63] = "adc_dpll_intclk",
		[62] = "cts_hevc_clk",
		[61] = "gpio_clk_msr",
		[60] = "alt_32k_clk",
		[59] = "cts_hcodec_clk",
		[58] = "Hdmirx_aud_clk",
		[57] = "Cts_hdmirx_audmeas",
		[56] = "Cts_hdmirx_modet_clk",
		[55] = "vid_pll_div_clk_out",
		[54] = "Cts_hdmirx_arc_ref_clk",
		[53] = "Sd_emmc_clk_A",
		[52] = "Sd_emmc_clk_B",
		[51] = "Sd_emmc_clk_C",
		[50] = "Mp3_clk_out",
		[49] = "mp2_clk_out",
		[48] = "mp1_clk_out",
		[47] = "ddr_dpll_pt_clk",
		[46] = "cts_vpu_clk",
		[45] = "cts_pwm_A_clk",
		[44] = "cts_pwm_B_clk",
		[43] = "fclk_div5",
		[42] = "mp0_clk_out",
		[41] = "eth_rx_clk_or_clk_rmii",
		[40] = "cts_pcm_mclk",
		[39] = "cts_pcm_sclk",
		[38] = "Cts_vdin_meas_clk",
		[37] = "cts_clk_i958",
		[36] = "cts_hdmi_tx_pixel_clk",
		[35] = "cts_mali_clk",
		[34] = "adc_dpll_clk_b3",
		[33] = "adc_dpll_clk_b2",
		[32] = "cts_vdec_clk",
		[31] = "MPLL_CLK_TEST_OUT",
		[30] = "Hdmirx_audmeas_clk",
		[29] = "Hdmirx_pix_clk",
		[28] = "cts_sar_adc_clk",
		[27] = "Hdmirx_mpll_div_clk",
		[26] = "sc_clk_int",
		[25] = "Hdmirx_tmds_clk",
		[24] = "Hdmirx_aud_pll_clk",
		[23] = "mmc_clk",
		[22] = "eth_phy_ref_clk",
		[21] = "i2s_clk_in_src0",
		[20] = "rtc_osc_clk_out",
		[19] = "adc_dpll_clka2",
		[18] = "sys_cpu_clk_div16",
		[17] = "sys_pll_div16",
		[16] = "cts_FEC_CLK_2",
		[15] = "cts_FEC_CLK_1",
		[14] = "cts_FEC_CLK_0",
		[13] = "cts_amclk",
		[12] = "Cts_pdm_clk",
		[11] = "mac_eth_tx_clk",
		[10] = "cts_vdac_clk",
		[9] = "cts_encl_clk",
		[8] = "cts_encp_clk",
		[7] = "clk81",
		[6] = "cts_enci_clk",
		[5] = "gp1_pll_clk",
		[4] = "gp0_pll_clk",
		[3] = "A53_ring_osc_clk",
		[2] = "am_ring_osc_clk_out_ee[2]",
		[1] = "am_ring_osc_clk_out_ee[1]",
		[0] = "am_ring_osc_clk_out_ee[0]",
	};
	int  i;
	int len = sizeof(clk_table)/sizeof(char *);

	if (index  == 0xff) {
		for (i = 0; i < len; i++)
			seq_printf(s, "[%2d][%10d]%s\n",
				   i, gxbb_clk_util_clk_msr(i),
					clk_table[i]);
		return 0;
	}
	seq_printf(s, "[%10d]%s\n", gxbb_clk_util_clk_msr(index),
		   clk_table[index]);
	clk_msr_index = 0xff;

	return 0;
}


int txlx_clk_measure(struct seq_file *s, void *what, unsigned int index)
{
	static const char * const clk_table[] = {
		[109] = "cts_alocker_in_clk",
		[108] = "cts_alocker_out_clk",
		[107] = "am_ring_osc_clk_out_ee[11]",
		[106] = "am_ring_osc_clk_out_ee[10]",
		[105] = "am_ring_osc_clk_out_ee[9]",
		[104] = "am_ring_osc_clk_out_ee[8]",
		[103] = "am_ring_osc_clk_out_ee[7]",
		[102] = "am_ring_osc_clk_out_ee[6]",
		[101] = "am_ring_osc_clk_out_ee[5]",
		[100] = "am_ring_osc_clk_out_ee[4]",
		[99] = "am_ring_osc_clk_out_ee[3]",
		[98] = "cts_hdmirx_aud_pll_clk",
		[97] = "cts_vpu_clkb_tmp   ",
		[96] = "cts_vpu_clkb       ",
		[95] = "ethphy_test_clk_out",
		[94] = "atv_dmd_mono_clk_32",
		[93] = "cts_audin_lrclk",
		[92] = "cts_audin_sclk ",
		[91] = "cts_audin_mclk     ",
		[90] = "cts_hdmitx_sys_clk ",
		[89] = "HDMI_CLK_TODIG     ",
		[88] = "hdmirx_vid_clk     ",
		[87] = "lvds_fifo_clk      ",
		[86] = "hdmirx_phy_dtb[3]  ",
		[85] = "hdmirx_phy_dtb[2]  ",
		[84] = "hdmirx_phy_dtb[1]  ",
		[83] = "hdmirx_phy_dtb[0]  ",
		[82] = "Cts_ge2d_clk       ",
		[81] = "Cts_vapbclk        ",
		[80] = "Rng_ring_osc_clk[3]",
		[79] = "Rng_ring_osc_clk[2]",
		[78] = "Rng_ring_osc_clk[1]",
		[77] = "Rng_ring_osc_clk[0]",
		[76] = "cts_aoclk_int      ",
		[75] = "cts_aoclkx2_int    ",
		[74] = "cts_atv_dmd_vdac_clk",
		[73] = "cts_pwm_C_clk      ",
		[72] = "cts_pwm_D_clk      ",
		[71] = "cts_pwm_E_clk      ",
		[70] = "cts_pwm_F_clk      ",
		[69] = "Cts_hdcp22_skp     ",
		[68] = "Cts_hdcp22_esm     ",
		[67] = "tvfe_sample_clk    ",
		[66] = "cts_vid_lock_clk   ",
		[65] = "cts_atv_dmd_sys_clk",
		[64] = "Cts_hdmirx_cfg_clk ",
		[63] = "adc_dpll_intclk    ",
		[62] = "cts_hevc_clk       ",
		[61] = "gpio_clk_msr       ",
		[60] = "alt_32k_clk        ",
		[59] = "cts_hcodec_clk     ",
		[58] = "Hdmirx_aud_clk     ",
		[57] = "Cts_hdmirx_audmeas ",
		[56] = "Cts_hdmirx_modet_clk",
		[55] = "vid_pll_div_clk_out	",
		[54] = "Cts_hdmirx_arc_ref_clk",
		[53] = "sd_emmc_clk_A		",
		[52] = "sd_emmc_clk_B		",
		[51] = "sd_emmc_clk_C       ",
		[50] = "mp3_clk_out			",
		[49] = "mp2_clk_out			",
		[48] = "mp1_clk_out			",
		[47] = "ddr_dpll_pt_clk		",
		[46] = "cts_vpu_clk			",
		[45] = "cts_pwm_A_clk		",
		[44] = "cts_pwm_B_clk		",
		[43] = "fclk_div5			",
		[42] = "mp0_clk_out			",
		[41] = "eth_rx_clk_rmii     ",
		[40] = "cts_pcm_mclk        ",
		[39] = "cts_pcm_sclk        ",
		[38] = "Cts_vdin_meas_clk   ",
		[37] = "cts_clk_i958        ",
		[36] = "cts_hdmi_tx_pixel_clk",
		[35] = "cts_mali_clk		",
		[34] = "adc_dpll_clk_b3     ",
		[33] = "adc_dpll_clk_b2     ",
		[32] = "cts_vdec_clk        ",
		[31] = "MPLL_CLK_TEST_OUT	",
		[30] = "Hdmirx_audmeas_clk	",
		[29] = "Hdmirx_pix_clk		",
		[28] = "Cts_sar_adc_clk		",
		[27] = "Hdmirx_mpll_div_clk	",
		[26] = "sc_clk_int          ",
		[25] = "Hdmirx_tmds_clk	    ",
		[24] = "Hdmirx_aud_pll_clk ",
		[23] = "mmc_clk             ",
		[22] = "eth_phy_ref_clk     ",
		[21] = "i2s_clk_in_src0     ",
		[20] = "rtc_osc_clk_out     ",
		[19] = "adc_dpll_clka2      ",
		[18] = "sys_cpu_clk_div16   ",
		[17] = "sys_pll_div16       ",
		[16] = "cts_FEC_CLK_2       ",
		[15] = "cts_FEC_CLK_1       ",
		[14] = "cts_FEC_CLK_0       ",
		[13] = "cts_amclk           ",
		[12] = "Cts_demod_core_clk  ",
		[11] = "mac_eth_tx_clk      ",
		[10] = "cts_vdac_clk        ",
		[9] = "cts_encl_clk         ",
		[8] = "cts_encp_clk         ",
		[7] = "clk81                ",
		[6] = "cts_enci_clk         ",
		[5] = "gp1_pll_clk          ",
		[4] = "gp0_pll_clk          ",
		[3] = "A53_ring_osc_clk     ",
		[2] = "am_ring_osc_clk_out_ee[2]",
		[1] = "am_ring_osc_clk_out_ee[1]",
		[0] = "am_ring_osc_clk_out_ee[0]",
	};
	int  i;
	int len = sizeof(clk_table)/sizeof(char *);

	if (index  == 0xff) {
		for (i = 0; i < len; i++)
			seq_printf(s, "[%2d][%10d]%s\n",
				   i, gxbb_clk_util_clk_msr(i),
					clk_table[i]);
		return 0;
	}
	seq_printf(s, "[%10d]%s\n", gxbb_clk_util_clk_msr(index),
		   clk_table[index]);
	clk_msr_index = 0xff;
	return 0;
}

int g12a_clk_measure(struct seq_file *s, void *what, unsigned int index)
{
	static const char * const clk_table[] = {
		[122] = "mod_audio_pdm_dclk_o       ",
		[121] = "audio_spdifin_mst_clk      ",
		[120] = "audio_spdifout_mst_clk     ",
		[119] = "audio_spdifout_b_mst_clk   ",
		[118] = "audio_pdm_sysclk           ",
		[117] = "audio_resample_clk         ",
		[116] = "audio_tdmin_a_sclk         ",
		[115] = "audio_tdmin_b_sclk         ",
		[114] = "audio_tdmin_c_sclk         ",
		[113] = "audio_tdmin_lb_sclk        ",
		[112] = "audio_tdmout_a_sclk        ",
		[111] = "audio_tdmout_b_sclk        ",
		[110] = "audio_tdmout_c_sclk        ",
		[109] = "c_alocker_out_clk          ",
		[108] = "c_alocker_in_clk           ",
		[107] = "au_dac_clk_g128x           ",
		[106] = "ephy_test_clk              ",
		[105] = "am_ring_osc_clk_out_ee[9]  ",
		[104] = "am_ring_osc_clk_out_ee[8]  ",
		[103] = "am_ring_osc_clk_out_ee[7]  ",
		[102] = "am_ring_osc_clk_out_ee[6]  ",
		[101] = "am_ring_osc_clk_out_ee[5]  ",
		[100] = "am_ring_osc_clk_out_ee[4]  ",
		[99] = "am_ring_osc_clk_out_ee[3]   ",
		[98] = "cts_ts_clk                 ",
		[97] = "cts_vpu_clkb_tmp           ",
		[96] = "cts_vpu_clkb               ",
		[95] = "eth_phy_plltxclk           ",
		[94] = "eth_phy_rxclk              ",
		[93] = "1'b0                       ",
		[92] = "1'b0                       ",
		[91] = "1'b0                       ",
		[90] = "cts_hdmitx_sys_clk         ",
		[89] = "HDMI_CLK_TODIG             ",
		[88] = "1'b0                       ",
		[87] = "1'b0                       ",
		[86] = "1'b0                       ",
		[85] = "1'b0                       ",
		[84] = "co_tx_clk                  ",
		[83] = "co_rx_clk                  ",
		[82] = "cts_ge2d_clk               ",
		[81] = "cts_vapbclk                ",
		[80] = "rng_ring_osc_clk[3]        ",
		[79] = "rng_ring_osc_clk[2]        ",
		[78] = "rng_ring_osc_clk[1]        ",
		[77] = "rng_ring_osc_clk[0]        ",
		[76] = "1'b0                       ",
		[75] = "cts_hevcf_clk              ",
		[74] = "1'b0                       ",
		[73] = "cts_pwm_C_clk              ",
		[72] = "cts_pwm_D_clk              ",
		[71] = "cts_pwm_E_clk              ",
		[70] = "cts_pwm_F_clk              ",
		[69] = "cts_hdcp22_skpclk          ",
		[68] = "cts_hdcp22_esmclk          ",
		[67] = "cts_dsi_phy_clk            ",
		[66] = "cts_vid_lock_clk           ",
		[65] = "cts_spicc_0_clk            ",
		[64] = "cts_spicc_1_clk            ",
		[63] = "cts_dsi_meas_clk           ",
		[62] = "cts_hevcb_clk              ",
		[61] = "gpio_clk_msr               ",
		[60] = "1'b0                       ",
		[59] = "cts_hcodec_clk             ",
		[58] = "cts_wave420l_bclk          ",
		[57] = "cts_wave420l_cclk          ",
		[56] = "cts_wave420l_aclk          ",
		[55] = "vid_pll_div_clk_out        ",
		[54] = "cts_vpu_clkc               ",
		[53] = "cts_sd_emmc_clk_A          ",
		[52] = "cts_sd_emmc_clk_B          ",
		[51] = "cts_sd_emmc_clk_C          ",
		[50] = "mp3_clk_out                ",
		[49] = "mp2_clk_out                ",
		[48] = "mp1_clk_out                ",
		[47] = "ddr_dpll_pt_clk            ",
		[46] = "cts_vpu_clk                ",
		[45] = "cts_pwm_A_clk              ",
		[44] = "cts_pwm_B_clk              ",
		[43] = "fclk_div5                  ",
		[42] = "mp0_clk_out                ",
		[41] = "mac_eth_rx_clk_rmii        ",
		[40] = "1'b0                       ",
		[39] = "cts_bt656_clk0             ",
		[38] = "cts_vdin_meas_clk          ",
		[37] = "cts_cdac_clk_c             ",
		[36] = "cts_hdmi_tx_pixel_clk      ",
		[35] = "cts_mali_clk               ",
		[34] = "eth_mppll_50m_ckout        ",
		[33] = "sys_cpu_ring_osc_clk[1]    ",
		[32] = "cts_vdec_clk               ",
		[31] = "mpll_clk_test_out          ",
		[30] = "pcie_clk_inn               ",
		[29] = "pcie_clk_inp               ",
		[28] = "cts_sar_adc_clk            ",
		[27] = "co_clkin_to_mac            ",
		[26] = "sc_clk_int                 ",
		[25] = "cts_eth_clk_rmii           ",
		[24] = "cts_eth_clk125Mhz          ",
		[23] = "mpll_clk_50m               ",
		[22] = "mac_eth_phy_ref_clk        ",
		[21] = "lcd_an_clk_ph3             ",
		[20] = "rtc_osc_clk_out            ",
		[19] = "lcd_an_clk_ph2             ",
		[18] = "sys_cpu_clk_div16          ",
		[17] = "sys_pll_div16              ",
		[16] = "cts_FEC_CLK_2              ",
		[15] = "cts_FEC_CLK_1              ",
		[14] = "cts_FEC_CLK_0              ",
		[13] = "mod_tcon_clko              ",
		[12] = "hifi_pll_clk               ",
		[11] = "mac_eth_tx_clk             ",
		[10] = "cts_vdac_clk               ",
		[9] = "cts_encl_clk             ",
		[8] = "cts_encp_clk             ",
		[7] = "clk81                    ",
		[6] = "cts_enci_clk             ",
		[5] = "1'b0                     ",
		[4] = "gp0_pll_clk              ",
		[3] = "sys_cpu_ring_osc_clk[0]  ",
		[2] = "am_ring_osc_clk_out_ee[2]",
		[1] = "am_ring_osc_clk_out_ee[1]",
		[0] = "am_ring_osc_clk_out_ee[0]",
	};
	int  i;
	int len = sizeof(clk_table)/sizeof(char *);

	if (index  == 0xff) {
		for (i = 0; i < len; i++)
			seq_printf(s, "[%2d][%10d]%s\n",
				   i, gxbb_clk_util_clk_msr(i),
					clk_table[i]);
		return 0;
	}
	seq_printf(s, "[%10d]%s\n", gxbb_clk_util_clk_msr(index),
		   clk_table[index]);
	clk_msr_index = 0xff;
	return 0;
}

int g12_ring_measure(struct seq_file *s, void *what, unsigned int index)
{
	static const char * const clk_table[] = {
			[11] = "sys_cpu_ring_osc_clk[1] ",
			[10] = "sys_cpu_ring_osc_clk[0] ",
			[9] = "am_ring_osc_clk_out_ee[9] ",
			[8] = "am_ring_osc_clk_out_ee[8] ",
			[7] = "am_ring_osc_clk_out_ee[7] ",
			[6] = "am_ring_osc_clk_out_ee[6] ",
			[5] = "am_ring_osc_clk_out_ee[5] ",
			[4] = "am_ring_osc_clk_out_ee[4] ",
			[3] = "am_ring_osc_clk_out_ee[3] ",
			[2] = "am_ring_osc_clk_out_ee[2] ",
			[1] = "am_ring_osc_clk_out_ee[1] ",
			[0] = "am_ring_osc_clk_out_ee[0] ",
		};
	const int tb[] = {0, 1, 2, 99, 100, 101, 102, 103, 104, 105, 3, 33};
	unsigned long i;
	unsigned char ringinfo[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	/*RING_OSCILLATOR       0x7f: set slow ring*/
	if (msr_ring_reg0 != NULL) {
		writel_relaxed(0x555555, msr_ring_reg0);
		for (i = 0; i < 12; i++)
			seq_printf(s, "%s	:%10d	KHz\n",
			  clk_table[i], meson_clk_util_ring_msr(tb[i]));
	} else {
		seq_puts(s, "fail test osc ring info\n");
	}

	if (scpi_get_ring_value(ringinfo) != 0) {
		seq_puts(s, "fail get osc ring efuse info\n");
		return 0;
	}

	seq_puts(s, "osc ring efuse info:\n");

	for (i = 0; i < 8; i++)
		seq_printf(s, "0x%x ", ringinfo[i]);
	seq_puts(s, "\n");

	/*efuse to test value*/
	seq_puts(s, "ee[9], ee[1], ee[0], cpu[1], cpu[0], iddee, iddcpu\n");

	for (i = 1; i <= 5; i++)
		seq_printf(s, "%d KHz ", (ringinfo[i] * 20));

	for (i = 6; i <= 7; i++)
		seq_printf(s, "%d uA ", (ringinfo[i] * 200));

	seq_puts(s, "\n");

	return 0;
}

int g12b_clk_measure(struct seq_file *s, void *what, unsigned int index)
{
	static const char * const clk_table[] = {
		[126] = "mipi_csi_phy1_clk_out",
		[125] = "mipi_csi_phy0_clk_out",
		[124] = "cts_gdc_core_clk",
		[123] = "cts_gdc_axi_clk",
		[122] = "mod_audio_pdm_dclk_o       ",
		[121] = "audio_spdifin_mst_clk      ",
		[120] = "audio_spdifout_mst_clk     ",
		[119] = "audio_spdifout_b_mst_clk   ",
		[118] = "audio_pdm_sysclk           ",
		[117] = "audio_resample_clk         ",
		[116] = "audio_tdmin_a_sclk         ",
		[115] = "audio_tdmin_b_sclk         ",
		[114] = "audio_tdmin_c_sclk         ",
		[113] = "audio_tdmin_lb_sclk        ",
		[112] = "audio_tdmout_a_sclk        ",
		[111] = "audio_tdmout_b_sclk        ",
		[110] = "audio_tdmout_c_sclk        ",
		[109] = "c_alocker_out_clk          ",
		[108] = "c_alocker_in_clk           ",
		[107] = "au_dac_clk_g128x           ",
		[106] = "ephy_test_clk              ",
		[105] = "am_ring_osc_clk_out_ee[9]  ",
		[104] = "am_ring_osc_clk_out_ee[8]  ",
		[103] = "am_ring_osc_clk_out_ee[7]  ",
		[102] = "am_ring_osc_clk_out_ee[6]  ",
		[101] = "am_ring_osc_clk_out_ee[5]  ",
		[100] = "am_ring_osc_clk_out_ee[4]  ",
		[99] = "am_ring_osc_clk_out_ee[3]   ",
		[98] = "cts_ts_clk                 ",
		[97] = "cts_vpu_clkb_tmp           ",
		[96] = "cts_vpu_clkb               ",
		[95] = "eth_phy_plltxclk           ",
		[94] = "eth_phy_rxclk              ",
		[93] = "1'b0                       ",
		[92] = "sys_pllB_div16             ",
		[91] = "sys_cpuB_clk_div16         ",
		[90] = "cts_hdmitx_sys_clk         ",
		[89] = "HDMI_CLK_TODIG             ",
		[88] = "cts_mipi_isp_clk           ",
		[87] = "1'b0                       ",
		[86] = "cts_vipnanoq_core_clk      ",
		[85] = "cts_vipnanoq_axi_clk       ",
		[84] = "co_tx_clk                  ",
		[83] = "co_rx_clk                  ",
		[82] = "cts_ge2d_clk               ",
		[81] = "cts_vapbclk                ",
		[80] = "rng_ring_osc_clk[3]        ",
		[79] = "rng_ring_osc_clk[2]        ",
		[78] = "rng_ring_osc_clk[1]        ",
		[77] = "rng_ring_osc_clk[0]        ",
		[76] = "cts_cci_clk                ",
		[75] = "cts_hevcf_clk              ",
		[74] = "cts_mipi_csi_phy_clk       ",
		[73] = "cts_pwm_C_clk              ",
		[72] = "cts_pwm_D_clk              ",
		[71] = "cts_pwm_E_clk              ",
		[70] = "cts_pwm_F_clk              ",
		[69] = "cts_hdcp22_skpclk          ",
		[68] = "cts_hdcp22_esmclk          ",
		[67] = "cts_dsi_phy_clk            ",
		[66] = "cts_vid_lock_clk           ",
		[65] = "cts_spicc_0_clk            ",
		[64] = "cts_spicc_1_clk            ",
		[63] = "cts_dsi_meas_clk           ",
		[62] = "cts_hevcb_clk              ",
		[61] = "gpio_clk_msr               ",
		[60] = "1'b0                       ",
		[59] = "cts_hcodec_clk             ",
		[58] = "cts_wave420l_bclk          ",
		[57] = "cts_wave420l_cclk          ",
		[56] = "cts_wave420l_aclk          ",
		[55] = "vid_pll_div_clk_out        ",
		[54] = "cts_vpu_clkc               ",
		[53] = "cts_sd_emmc_clk_A          ",
		[52] = "cts_sd_emmc_clk_B          ",
		[51] = "cts_sd_emmc_clk_C          ",
		[50] = "mp3_clk_out                ",
		[49] = "mp2_clk_out                ",
		[48] = "mp1_clk_out                ",
		[47] = "ddr_dpll_pt_clk            ",
		[46] = "cts_vpu_clk                ",
		[45] = "cts_pwm_A_clk              ",
		[44] = "cts_pwm_B_clk              ",
		[43] = "fclk_div5                  ",
		[42] = "mp0_clk_out                ",
		[41] = "mac_eth_rx_clk_rmii        ",
		[40] = "1'b0                       ",
		[39] = "cts_bt656_clk0             ",
		[38] = "cts_vdin_meas_clk          ",
		[37] = "cts_cdac_clk_c             ",
		[36] = "cts_hdmi_tx_pixel_clk      ",
		[35] = "cts_mali_clk               ",
		[34] = "eth_mppll_50m_ckout        ",
		[33] = "sys_cpu_ring_osc_clk[1]    ",
		[32] = "cts_vdec_clk               ",
		[31] = "mpll_clk_test_out          ",
		[30] = "pcie_clk_inn               ",
		[29] = "pcie_clk_inp               ",
		[28] = "cts_sar_adc_clk            ",
		[27] = "co_clkin_to_mac            ",
		[26] = "sc_clk_int                 ",
		[25] = "cts_eth_clk_rmii           ",
		[24] = "cts_eth_clk125Mhz          ",
		[23] = "mpll_clk_50m               ",
		[22] = "mac_eth_phy_ref_clk        ",
		[21] = "lcd_an_clk_ph3             ",
		[20] = "rtc_osc_clk_out            ",
		[19] = "lcd_an_clk_ph2             ",
		[18] = "sys_cpu_clk_div16          ",
		[17] = "sys_pll_div16              ",
		[16] = "cts_FEC_CLK_2              ",
		[15] = "cts_FEC_CLK_1              ",
		[14] = "cts_FEC_CLK_0              ",
		[13] = "mod_tcon_clko              ",
		[12] = "hifi_pll_clk               ",
		[11] = "mac_eth_tx_clk             ",
		[10] = "cts_vdac_clk               ",
		[9] = "cts_encl_clk             ",
		[8] = "cts_encp_clk             ",
		[7] = "clk81                    ",
		[6] = "cts_enci_clk             ",
		[5] = "1'b0                     ",
		[4] = "gp0_pll_clk              ",
		[3] = "sys_cpu_ring_osc_clk[0]  ",
		[2] = "am_ring_osc_clk_out_ee[2]",
		[1] = "am_ring_osc_clk_out_ee[1]",
		[0] = "am_ring_osc_clk_out_ee[0]",
	};
	int  i;
	int len = sizeof(clk_table)/sizeof(char *);

	if (index  == 0xff) {
		for (i = 0; i < len; i++)
			seq_printf(s, "[%2d][%10d]%s\n",
				   i, gxbb_clk_util_clk_msr(i),
					clk_table[i]);
		return 0;
	}
	seq_printf(s, "[%10d]%s\n", gxbb_clk_util_clk_msr(index),
		   clk_table[index]);
	clk_msr_index = 0xff;
	return 0;
}

int meson_clk_measure(unsigned int clk_mux)
{
	int clk_val;
	unsigned long flags;

	spin_lock_irqsave(&clk_measure_lock, flags);
	if (clk_data) {
		clk_val = clk_data->clk_msr_function(clk_mux);
	} else {
		switch (get_cpu_type()) {
		case MESON_CPU_MAJOR_ID_M8B:
			clk_val = m8b_clk_util_clk_msr(clk_mux);
			break;
		case MESON_CPU_MAJOR_ID_GXL:
		case MESON_CPU_MAJOR_ID_GXM:
		case MESON_CPU_MAJOR_ID_TXL:
		case MESON_CPU_MAJOR_ID_TXLX:
		case MESON_CPU_MAJOR_ID_G12A:
		case MESON_CPU_MAJOR_ID_G12B:
		case MESON_CPU_MAJOR_ID_AXG:
			clk_val = gxbb_clk_util_clk_msr(clk_mux);
			break;
		default:
			pr_info("Unsupported chip clk measure\n");
			clk_val = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&clk_measure_lock, flags);

	return clk_val;

}
EXPORT_SYMBOL(meson_clk_measure);

static int dump_clk(struct seq_file *s, void *what)
{
	if (clk_data) {
		int i;
		const char * const *clk_table = clk_data->clk_table;
		int len = clk_data->table_size;

		for (i = 0; i < len; i++)
			seq_printf(s, "[%2d][%10d]%s\n", i,
			clk_data->clk_msr_function(i), clk_table[i]);
	} else {
		if (get_cpu_type() == MESON_CPU_MAJOR_ID_M8B)
			m8b_clk_measure(s, what, clk_msr_index);
		else if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXL)
			gxl_clk_measure(s, what, clk_msr_index);
		else if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXM)
			gxm_clk_measure(s, what, clk_msr_index);
		else if (get_cpu_type() == MESON_CPU_MAJOR_ID_AXG)
			axg_clk_measure(s, what, clk_msr_index);
		else if (get_cpu_type() == MESON_CPU_MAJOR_ID_TXL)
			txl_clk_measure(s, what, clk_msr_index);
		else if (get_cpu_type() == MESON_CPU_MAJOR_ID_TXLX)
			txlx_clk_measure(s, what, clk_msr_index);
		else if (get_cpu_type() == MESON_CPU_MAJOR_ID_G12A)
			g12a_clk_measure(s, what, clk_msr_index);
		else if (get_cpu_type() == MESON_CPU_MAJOR_ID_G12B)
			g12b_clk_measure(s, what, clk_msr_index);
	}

	return 0;
}

static int dump_ring(struct seq_file *s, void *what)
{
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_G12A)
		g12_ring_measure(s, what, clk_msr_index);
	return 0;
}


static ssize_t clkmsr_write(struct file *file, const char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf)-1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	/* ret = sscanf(buf, "%i", &clk_msr_index); */
	ret = kstrtouint(buf, 0, &clk_msr_index);
	switch (ret) {
	case 1:
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static ssize_t ringmsr_write(struct file *file, const char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	return count;
}

static int clkmsr_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_clk, inode->i_private);
}

static int ringmsr_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_ring, inode->i_private);
}


static const struct file_operations clkmsr_file_ops = {
	.open		= clkmsr_open,
	.read		= seq_read,
	.write		= clkmsr_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations ringmsr_file_ops = {
	.open		= ringmsr_open,
	.read		= seq_read,
	.write		= ringmsr_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static int aml_clkmsr_probe(struct platform_device *pdev)
{
	static struct dentry *debugfs_root;
	struct device_node *np;
	u32 ringctrl;

	np = pdev->dev.of_node;
	debugfs_root = debugfs_create_dir("aml_clkmsr", NULL);
	if (IS_ERR(debugfs_root) || !debugfs_root) {
		pr_warn("failed to create debugfs directory\n");
		debugfs_root = NULL;
		return -1;
	}
	debugfs_create_file("clkmsr", S_IFREG | 0444,
			    debugfs_root, NULL, &clkmsr_file_ops);

	debugfs_create_file("ringmsr", S_IFREG | 0444,
			    debugfs_root, NULL, &ringmsr_file_ops);

	msr_clk_reg0 = of_iomap(np, 0);
	msr_clk_reg2 = of_iomap(np, 1);
	pr_info("msr_clk_reg0=%p,msr_clk_reg2=%p\n",
		msr_clk_reg0, msr_clk_reg2);

	if (of_property_read_u32(pdev->dev.of_node,
				"ringctrl", &ringctrl)) {
		dev_err(&pdev->dev,
			"failed to get msr ring reg0\n");
		msr_ring_reg0 = NULL;
	} else {
		msr_ring_reg0 = ioremap(ringctrl, 1);
		pr_info("msr_ring_reg0=%p\n", msr_ring_reg0);
	}

	clk_data = (struct meson_clkmsr_data *)
	of_device_get_match_data(&pdev->dev);

	return 0;
}

static const char * const tl1_table[] = {
	[144] = "ts_pll_clk",
	[143] = "mainclk",
	[142] = "demode_ts_clk",
	[141] = "ts_ddr_clk",
	[140] = "audio_toacodec_bclk",
	[139] = "aud_adc_clk_g128x",
	[138] = "dsu_pll_clk_cpu",
	[137] = "atv_dmd_i2c_sclk",
	[136] = "sys_pll_clk",
	[135] = "tvfe_sample_clk",
	[134] = "adc_extclk_in",
	[133] = "atv_dmd_mono_clk_32",
	[132] = "audio_toacode_mclk",
	[131] = "ts_sar_clk",
	[130] = "au_dac2_clk_gf128x",
	[129] = "lvds_fifo_clk",
	[128] = "cts_tcon_pll_clk",
	[127] = "hdmirx_vid_clk",
	[126] = "sar_ring_osc_clk",
	[125] = "cts_hdmi_axi_clk",
	[124] =	"cts_demod_core_clk",
	[123] =	"mod_audio_pdm_dclk_o",
	[122] =	"audio_spdifin_mst_clk",
	[121] = "audio_spdifout_mst_clk",
	[120] = "audio_spdifout_b_mst_clk",
	[119] = "audio_pdm_sysclk",
	[118] = "audio_resamplea_clk",
	[117] = "audio_resampleb_clk",
	[116] = "audio_tdmin_a_sclk",
	[115] = "audio_tdmin_b_sclk",
	[114] = "audio_tdmin_c_sclk",
	[113] = "audio_tdmin_lb_sclk",
	[112] = "audio_tdmout_a_sclk",
	[111] = "audio_tdmout_b_sclk",
	[110] = "audio_tdmout_c_sclk",
	[109] = "o_vad_clk",
	[108] = "acodec_i2sout_bclk",
	[107] = "au_dac_clk_g128x",
	[106] = "ephy_test_clk",
	[105] = "am_ring_osc_clk_out_ee[9]",
	[104] = "am_ring_osc_clk_out_ee[8]",
	[103] = "am_ring_osc_clk_out_ee[7]",
	[102] = "am_ring_osc_clk_out_ee[6]",
	[101] = "am_ring_osc_clk_out_ee[5]",
	[100] = "am_ring_osc_clk_out_ee[4]",
	[99] = "am_ring_osc_clk_out_ee[3]",
	[98] = "cts_ts_clk",
	[97] = "cts_vpu_clkb_tmp",
	[96] = "cts_vpu_clkb",
	[95] = "eth_phy_plltxclk",
	[94] = "eth_phy_exclk",
	[93] = "sys_cpu_ring_osc_clk[3]",
	[92] = "sys_cpu_ring_osc_clk[2]",
	[91] = "hdmirx_audmeas_clk",
	[90] = "am_ring_osc_clk_out_ee[11]",
	[89] = "am_ring_osc_clk_out_ee[10]",
	[88] = "cts_hdmirx_meter_clk",
	[87] = "1'b0",
	[86] = "cts_hdmirx_modet_clk",
	[85] = "cts_hdmirx_acr_ref_clk",
	[84] = "co_tx_cl",
	[83] = "co_rx_clk",
	[82] = "cts_ge2d_clk",
	[81] = "cts_vapbclk",
	[80] = "rng_ring_osc_clk[3]",
	[79] = "rng_ring_osc_clk[2]",
	[78] = "rng_ring_osc_clk[1]",
	[77] = "rng_ring_osc_clk[0]",
	[76] = "hdmix_aud_clk",
	[75] = "cts_hevcf_clk",
	[74] = "hdmirx_aud_pll_clk",
	[73] = "cts_pwm_C_clk",
	[72] = "cts_pwm_D_clk",
	[71] = "cts_pwm_E_clk",
	[70] = "cts_pwm_F_clk",
	[69] = "cts_hdcp22_skpclk",
	[68] = "cts_hdcp22_esmclk",
	[67] = "hdmirx_apll_clk_audio",
	[66] = "cts_vid_lock_clk",
	[65] = "cts_spicc_0_clk",
	[64] = "cts_spicc_1_clk",
	[63] = "hdmirx_tmds_clk",
	[62] = "cts_hevcb_clk",
	[61] = "gpio_clk_msr",
	[60] = "cts_hdmirx_aud_pll_clk",
	[59] = "cts_hcodec_clk",
	[58] = "cts_vafe_datack",
	[57] = "cts_atv_dmd_vdac_clk",
	[56] = "cts_atv_dmd_sys_clk",
	[55] = "vid_pll_div_clk_out",
	[54] = "cts_vpu_clkc",
	[53] = "ddr_2xclk",
	[52] = "cts_sd_emmc_clk_B",
	[51] = "cts_sd_emmc_clk_C",
	[50] = "mp3_clk_out",
	[49] = "mp2_clk_out",
	[48] = "mp1_clk_out",
	[47] = "ddr_dpll_pt_clk",
	[46] = "cts_vpu_clk",
	[45] = "cts_pwm_A_clk",
	[44] = "cts_pwm_B_clk",
	[43] = "fclk_div5",
	[42] = "mp0_clk_out",
	[41] = "mac_eth_rx_clk_rmii",
	[40] = "cts_hdmirx_cfg_clk",
	[39] = "cts_bt656_clk0",
	[38] = "cts_vdin_meas_clk",
	[37] = "cts_cdac_clk_c",
	[36] = "cts_hdmi_tx_pixel_clk",
	[35] = "cts_mali_clk",
	[34] = "eth_mppll_50m_ckout",
	[33] = "sys_cpu_ring_osc_clk[1]",
	[32] = "cts_vdec_clk",
	[31] = "mpll_clk_test_out",
	[30] = "hdmirx_cable_clk",
	[29] = "hdmirx_apll_clk_out_div",
	[28] = "cts_sar_adc_clk",
	[27] = "co_clkin_to_mac",
	[26] = "sc_clk_int",
	[25] = "cts_eth_clk_rmii",
	[24] = "cts_eth_clk125Mhz",
	[23] = "mpll_clk_50m",
	[22] = "mac_eth_phy_ref_clk",
	[21] = "lcd_an_clk_ph3",
	[20] = "rtc_osc_clk_out",
	[19] = "lcd_an_clk_ph2",
	[18] = "sys_cpu_clk_div16",
	[17] = "sys_pll_div16",
	[16] = "cts_FEC_CLK_2",
	[15] = "cts_FEC_CLK_1",
	[14] = "cts_FEC_CLK_0",
	[13] = "mod_tcon_clko",
	[12] = "hifi_pll_clk",
	[11] = "mac_eth_tx_clk",
	[10] = "cts_vdac_clk",
	[9] = "cts_encl_clk",
	[8] = "cts_encp_clk",
	[7] = "clk81",
	[6] = "cts_enci_clk",
	[5] = "gp1_pll_clk",
	[4] = "gp0_pll_clk",
	[3] = "sys_cpu_ring_osc_clk[0]",
	[2] = "am_ring_osc_clk_out_ee[2]",
	[1] = "am_ring_osc_clk_out_ee[1]",
	[0] = "am_ring_osc_clk_out_ee[0]",
};

static const struct meson_clkmsr_data tl1_data = {
	.clk_table = tl1_table,
	.table_size = ARRAY_SIZE(tl1_table),
	.clk_msr_function = gxbb_clk_util_clk_msr,
};

static const struct of_device_id meson_clkmsr_dt_match[] = {
	{	.compatible = "amlogic, gxl_measure",},
	{	.compatible = "amlogic, m8b_measure",},
	{	.compatible = "amlogic,tl1-measure", .data = &tl1_data },

	{},
};

static struct platform_driver aml_clkmsr_driver = {
	.probe = aml_clkmsr_probe,
	.driver = {
		.name = CLKMSR_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = meson_clkmsr_dt_match,
	},
};

static int __init aml_clkmsr_init(void)
{
	int ret = -1;

	ret = platform_driver_register(&aml_clkmsr_driver);

	if (ret != 0) {
		pr_err("clkmsr:failed to register driver, error %d\n", ret);
		return -ENODEV;
	}
	pr_info("clkmsr: driver init\n");

	return ret;
}

static void __exit aml_clkmsr_exit(void)
{
	platform_driver_unregister(&aml_clkmsr_driver);
}

arch_initcall(aml_clkmsr_init);
module_exit(aml_clkmsr_exit);

MODULE_DESCRIPTION("Amlogic clkmsr module");
MODULE_LICENSE("GPL");

