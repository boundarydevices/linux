// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Qiqi Wang <qiqi.wang@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <dt-bindings/clock/mediatek,mt8189-clk.h>

#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-gate.h"
#include "clk-pll.h"

/* bringup config */
#define MT_CCF_PLL_DISABLE	0
#define MT_CCF_MUX_DISABLE	0

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1

/* TOPCK MUX SEL REG */
#define CLK_CFG_UPDATE				0x0004
#define CLK_CFG_UPDATE1				0x0008
#define CLK_CFG_UPDATE2				0x000c
#define VLP_CLK_CFG_UPDATE			0x0004
#define CLK_CFG_0				0x0010
#define CLK_CFG_0_SET				0x0014
#define CLK_CFG_0_CLR				0x0018
#define CLK_CFG_1				0x0020
#define CLK_CFG_1_SET				0x0024
#define CLK_CFG_1_CLR				0x0028
#define CLK_CFG_2				0x0030
#define CLK_CFG_2_SET				0x0034
#define CLK_CFG_2_CLR				0x0038
#define CLK_CFG_3				0x0040
#define CLK_CFG_3_SET				0x0044
#define CLK_CFG_3_CLR				0x0048
#define CLK_CFG_4				0x0050
#define CLK_CFG_4_SET				0x0054
#define CLK_CFG_4_CLR				0x0058
#define CLK_CFG_5				0x0060
#define CLK_CFG_5_SET				0x0064
#define CLK_CFG_5_CLR				0x0068
#define CLK_CFG_6				0x0070
#define CLK_CFG_6_SET				0x0074
#define CLK_CFG_6_CLR				0x0078
#define CLK_CFG_7				0x0080
#define CLK_CFG_7_SET				0x0084
#define CLK_CFG_7_CLR				0x0088
#define CLK_CFG_8				0x0090
#define CLK_CFG_8_SET				0x0094
#define CLK_CFG_8_CLR				0x0098
#define CLK_CFG_9				0x00A0
#define CLK_CFG_9_SET				0x00A4
#define CLK_CFG_9_CLR				0x00A8
#define CLK_CFG_10				0x00B0
#define CLK_CFG_10_SET				0x00B4
#define CLK_CFG_10_CLR				0x00B8
#define CLK_CFG_11				0x00C0
#define CLK_CFG_11_SET				0x00C4
#define CLK_CFG_11_CLR				0x00C8
#define CLK_CFG_12				0x00D0
#define CLK_CFG_12_SET				0x00D4
#define CLK_CFG_12_CLR				0x00D8
#define CLK_CFG_13				0x00E0
#define CLK_CFG_13_SET				0x00E4
#define CLK_CFG_13_CLR				0x00E8
#define CLK_CFG_14				0x00F0
#define CLK_CFG_14_SET				0x00F4
#define CLK_CFG_14_CLR				0x00F8
#define CLK_CFG_15				0x0100
#define CLK_CFG_15_SET				0x0104
#define CLK_CFG_15_CLR				0x0108
#define CLK_CFG_16				0x0110
#define CLK_CFG_16_SET				0x0114
#define CLK_CFG_16_CLR				0x0118
#define CLK_CFG_17				0x0180
#define CLK_CFG_17_SET				0x0184
#define CLK_CFG_17_CLR				0x0188
#define CLK_CFG_18				0x0190
#define CLK_CFG_18_SET				0x0194
#define CLK_CFG_18_CLR				0x0198
#define CLK_CFG_19				0x0240
#define CLK_CFG_19_SET				0x0244
#define CLK_CFG_19_CLR				0x0248
#define CLK_AUDDIV_0				0x0320
#define CLK_MISC_CFG_3				0x0510
#define CLK_MISC_CFG_3_SET				0x0514
#define CLK_MISC_CFG_3_CLR				0x0518
#define VLP_CLK_CFG_0				0x0008
#define VLP_CLK_CFG_0_SET				0x000C
#define VLP_CLK_CFG_0_CLR				0x0010
#define VLP_CLK_CFG_1				0x0014
#define VLP_CLK_CFG_1_SET				0x0018
#define VLP_CLK_CFG_1_CLR				0x001C
#define VLP_CLK_CFG_2				0x0020
#define VLP_CLK_CFG_2_SET				0x0024
#define VLP_CLK_CFG_2_CLR				0x0028
#define VLP_CLK_CFG_3				0x002C
#define VLP_CLK_CFG_3_SET				0x0030
#define VLP_CLK_CFG_3_CLR				0x0034
#define VLP_CLK_CFG_4				0x0038
#define VLP_CLK_CFG_4_SET				0x003C
#define VLP_CLK_CFG_4_CLR				0x0040
#define VLP_CLK_CFG_5				0x0044
#define VLP_CLK_CFG_5_SET				0x0048
#define VLP_CLK_CFG_5_CLR				0x004C

/* TOPCK MUX SHIFT */
#define TOP_MUX_AXI_SHIFT			0
#define TOP_MUX_AXI_PERI_SHIFT			1
#define TOP_MUX_AXI_UFS_SHIFT			2
#define TOP_MUX_BUS_AXIMEM_SHIFT		3
#define TOP_MUX_DISP0_SHIFT			4
#define TOP_MUX_MMINFRA_SHIFT			5
#define TOP_MUX_UART_SHIFT			6
#define TOP_MUX_SPI0_SHIFT			7
#define TOP_MUX_SPI1_SHIFT			8
#define TOP_MUX_SPI2_SHIFT			9
#define TOP_MUX_SPI3_SHIFT			10
#define TOP_MUX_SPI4_SHIFT			11
#define TOP_MUX_SPI5_SHIFT			12
#define TOP_MUX_MSDC_MACRO_0P_SHIFT		13
#define TOP_MUX_MSDC50_0_HCLK_SHIFT		14
#define TOP_MUX_MSDC50_0_SHIFT			15
#define TOP_MUX_AES_MSDCFDE_SHIFT		16
#define TOP_MUX_MSDC_MACRO_1P_SHIFT		17
#define TOP_MUX_MSDC30_1_SHIFT			18
#define TOP_MUX_MSDC30_1_HCLK_SHIFT		19
#define TOP_MUX_MSDC_MACRO_2P_SHIFT		20
#define TOP_MUX_MSDC30_2_SHIFT			21
#define TOP_MUX_MSDC30_2_HCLK_SHIFT		22
#define TOP_MUX_AUD_INTBUS_SHIFT		23
#define TOP_MUX_ATB_SHIFT			24
#define TOP_MUX_DISP_PWM_SHIFT			25
#define TOP_MUX_USB_TOP_P0_SHIFT		26
#define TOP_MUX_SSUSB_XHCI_P0_SHIFT		27
#define TOP_MUX_USB_TOP_P1_SHIFT		28
#define TOP_MUX_SSUSB_XHCI_P1_SHIFT		29
#define TOP_MUX_USB_TOP_P2_SHIFT		30
#define TOP_MUX_SSUSB_XHCI_P2_SHIFT		0
#define TOP_MUX_USB_TOP_P3_SHIFT		1
#define TOP_MUX_SSUSB_XHCI_P3_SHIFT		2
#define TOP_MUX_USB_TOP_P4_SHIFT		3
#define TOP_MUX_SSUSB_XHCI_P4_SHIFT		4
#define TOP_MUX_I2C_SHIFT			5
#define TOP_MUX_SENINF_SHIFT			6
#define TOP_MUX_SENINF1_SHIFT			7
#define TOP_MUX_AUD_ENGEN1_SHIFT		8
#define TOP_MUX_AUD_ENGEN2_SHIFT		9
#define TOP_MUX_AES_UFSFDE_SHIFT		10
#define TOP_MUX_UFS_SHIFT			11
#define TOP_MUX_UFS_MBIST_SHIFT			12
#define TOP_MUX_AUD_1_SHIFT			13
#define TOP_MUX_AUD_2_SHIFT			14
#define TOP_MUX_VENC_SHIFT			15
#define TOP_MUX_VDEC_SHIFT			16
#define TOP_MUX_PWM_SHIFT			17
#define TOP_MUX_AUDIO_H_SHIFT			18
#define TOP_MUX_MCUPM_SHIFT			19
#define TOP_MUX_MEM_SUB_SHIFT			20
#define TOP_MUX_MEM_SUB_PERI_SHIFT		21
#define TOP_MUX_MEM_SUB_UFS_SHIFT		22
#define TOP_MUX_EMI_N_SHIFT			23
#define TOP_MUX_DSI_OCC_SHIFT			24
#define TOP_MUX_AP2CONN_HOST_SHIFT		25
#define TOP_MUX_IMG1_SHIFT			26
#define TOP_MUX_IPE_SHIFT			27
#define TOP_MUX_CAM_SHIFT			28
#define TOP_MUX_CAMTM_SHIFT			29
#define TOP_MUX_DSP_SHIFT			30
#define TOP_MUX_SR_PKA_SHIFT			0
#define TOP_MUX_DXCC_SHIFT			1
#define TOP_MUX_MFG_REF_SHIFT			2
#define TOP_MUX_MDP0_SHIFT			3
#define TOP_MUX_DP_SHIFT			4
#define TOP_MUX_EDP_SHIFT			5
#define TOP_MUX_EDP_FAVT_SHIFT			6
#define TOP_MUX_SNPS_ETH_250M_SHIFT		7
#define TOP_MUX_SNPS_ETH_62P4M_PTP_SHIFT	8
#define TOP_MUX_SNPS_ETH_50M_RMII_SHIFT		9
#define TOP_MUX_SFLASH_SHIFT			10
#define TOP_MUX_GCPU_SHIFT			11
#define TOP_MUX_PCIE_MAC_TL_SHIFT		12
#define TOP_MUX_VDSTX_CLKDIG_CTS_SHIFT		13
#define TOP_MUX_PLL_DPIX_SHIFT			14
#define TOP_MUX_ECC_SHIFT			15
#define TOP_MUX_SCP_SHIFT			0
#define TOP_MUX_PWRAP_ULPOSC_SHIFT		1
#define TOP_MUX_SPMI_P_MST_SHIFT		2
#define TOP_MUX_DVFSRC_SHIFT			3
#define TOP_MUX_PWM_VLP_SHIFT			4
#define TOP_MUX_AXI_VLP_SHIFT			5
#define TOP_MUX_SYSTIMER_26M_SHIFT		6
#define TOP_MUX_SSPM_SHIFT			7
#define TOP_MUX_SSPM_F26M_SHIFT			8
#define TOP_MUX_SRCK_SHIFT			9
#define TOP_MUX_SCP_SPI_SHIFT			10
#define TOP_MUX_SCP_IIC_SHIFT			11
#define TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT		12
#define TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT		13
#define TOP_MUX_SSPM_ULPOSC_SHIFT		14
#define TOP_MUX_APXGPT_26M_SHIFT		15
#define TOP_MUX_VADSP_SHIFT			16
#define TOP_MUX_VADSP_VOWPLL_SHIFT		17
#define TOP_MUX_VADSP_UARTHUB_BCLK_SHIFT	18
#define TOP_MUX_CAMTG0_SHIFT			19
#define TOP_MUX_CAMTG1_SHIFT			20
#define TOP_MUX_CAMTG2_SHIFT			21
#define TOP_MUX_AUD_ADC_SHIFT			22
#define TOP_MUX_KP_IRQ_GEN_SHIFT		23

/* TOPCK CKSTA REG */


/* TOPCK DIVIDER REG */
#define CLK_AUDDIV_2				0x0328
#define CLK_AUDDIV_3				0x0334
#define CLK_AUDDIV_5				0x033C

/* APMIXED PLL REG */
#define AP_PLL_CON3				0x00C
#define APLL1_TUNER_CON0			0x040
#define APLL2_TUNER_CON0			0x044
#define ARMPLL_LL_CON0				0x204
#define ARMPLL_LL_CON1				0x208
#define ARMPLL_LL_CON2				0x20C
#define ARMPLL_LL_CON3				0x210
#define ARMPLL_BL_CON0				0x214
#define ARMPLL_BL_CON1				0x218
#define ARMPLL_BL_CON2				0x21C
#define ARMPLL_BL_CON3				0x220
#define CCIPLL_CON0				0x224
#define CCIPLL_CON1				0x228
#define CCIPLL_CON2				0x22C
#define CCIPLL_CON3				0x230
#define MAINPLL_CON0				0x304
#define MAINPLL_CON1				0x308
#define MAINPLL_CON2				0x30C
#define MAINPLL_CON3				0x310
#define UNIVPLL_CON0				0x314
#define UNIVPLL_CON1				0x318
#define UNIVPLL_CON2				0x31C
#define UNIVPLL_CON3				0x320
#define MMPLL_CON0				0x324
#define MMPLL_CON1				0x328
#define MMPLL_CON2				0x32C
#define MMPLL_CON3				0x330
#define MFGPLL_CON0				0x504
#define MFGPLL_CON1				0x508
#define MFGPLL_CON2				0x50C
#define MFGPLL_CON3				0x510
#define APLL1_CON0				0x404
#define APLL1_CON1				0x408
#define APLL1_CON2				0x40C
#define APLL1_CON3				0x410
#define APLL1_CON4				0x414
#define APLL2_CON0				0x418
#define APLL2_CON1				0x41C
#define APLL2_CON2				0x420
#define APLL2_CON3				0x424
#define APLL2_CON4				0x428
#define EMIPLL_CON0				0x334
#define EMIPLL_CON1				0x338
#define EMIPLL_CON2				0x33C
#define EMIPLL_CON3				0x340
#define APUPLL2_CON0				0x614
#define APUPLL2_CON1				0x618
#define APUPLL2_CON2				0x61C
#define APUPLL2_CON3				0x620
#define APUPLL_CON0				0x604
#define APUPLL_CON1				0x608
#define APUPLL_CON2				0x60C
#define APUPLL_CON3				0x610
#define TVDPLL1_CON0				0x42C
#define TVDPLL1_CON1				0x430
#define TVDPLL1_CON2				0x434
#define TVDPLL1_CON3				0x438
#define TVDPLL2_CON0				0x43C
#define TVDPLL2_CON1				0x440
#define TVDPLL2_CON2				0x444
#define TVDPLL2_CON3				0x448
#define ETHPLL_CON0				0x514
#define ETHPLL_CON1				0x518
#define ETHPLL_CON2				0x51C
#define ETHPLL_CON3				0x520
#define MSDCPLL_CON0				0x524
#define MSDCPLL_CON1				0x528
#define MSDCPLL_CON2				0x52C
#define MSDCPLL_CON3				0x530
#define UFSPLL_CON0				0x534
#define UFSPLL_CON1				0x538
#define UFSPLL_CON2				0x53C
#define UFSPLL_CON3				0x540

/* HW Voter REG */


static DEFINE_SPINLOCK(mt8189_clk_lock);

static const struct mtk_fixed_factor vlp_ck_divs[] = {
	FACTOR(CLK_VLP_CK_SCP, "vlp_scp_ck",
			"vlp_scp_sel", 1, 1),
	FACTOR(CLK_VLP_CK_PWRAP_ULPOSC, "vlp_pwrap_ulposc_ck",
			"vlp_pwrap_ulposc_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SPMI_P_MST, "vlp_spmi_p_ck",
			"vlp_spmi_p_sel", 1, 1),
	FACTOR(CLK_VLP_CK_DVFSRC, "vlp_dvfsrc_ck",
			"vlp_dvfsrc_sel", 1, 1),
	FACTOR(CLK_VLP_CK_PWM_VLP, "vlp_pwm_vlp_ck",
			"vlp_pwm_vlp_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SSPM, "vlp_sspm_ck",
			"vlp_sspm_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SSPM_F26M, "vlp_sspm_f26m_ck",
			"vlp_sspm_f26m_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SRCK, "vlp_srck_ck",
			"vlp_srck_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SCP_IIC, "vlp_scp_iic_ck",
			"vlp_scp_iic_sel", 1, 1),
	FACTOR(CLK_VLP_CK_VADSP, "vlp_vadsp_ck",
			"vlp_vadsp_sel", 1, 1),
	FACTOR(CLK_VLP_CK_VADSP_VOWPLL, "vlp_vadsp_vowpll_ck",
			"vlp_vadsp_vowpll_sel", 1, 1),
	FACTOR(CLK_VLP_CK_VADSP_VLP_26M, "vlp_vadsp_vlp_26m_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_VLP_CK_SEJ_26M, "vlp_sej_26m_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_VLP_CK_SPMI_P_MST_32K, "vlp_spmi_p_32k_ck",
			"rtc32k_i", 1, 1),
	FACTOR(CLK_VLP_CK_VLP_F32K_COM, "vlp_vlp_f32k_com_ck",
			"rtc32k_i", 1, 1),
	FACTOR(CLK_VLP_CK_VLP_F26M_COM, "vlp_vlp_f26m_com_ck",
			"f26m_ck", 1, 1),
	FACTOR(CLK_VLP_CK_SSPM_F32K, "vlp_sspm_f32k_ck",
			"rtc32k_i", 1, 1),
	FACTOR(CLK_VLP_CK_SSPM_ULPOSC, "vlp_sspm_ulposc_ck",
			"vlp_sspm_ulposc_sel", 1, 1),
	FACTOR(CLK_VLP_CK_DPMSRCK, "vlp_dpmsrck_ck",
			"f26m_ck", 1, 1),
	FACTOR(CLK_VLP_CK_DPMSRULP, "vlp_dpmsrulp_ck",
			"osc_d10", 1, 1),
	FACTOR(CLK_VLP_CK_DPMSRRTC, "vlp_dpmsrrtc_ck",
			"rtc32k_i", 1, 1),
};

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_CCIPLL, "ccipll_ck",
			"ccipll", 1, 1),
	FACTOR(CLK_TOP_MAINPLL, "mainpll_ck",
			"mainpll", 1, 1),
	FACTOR(CLK_TOP_MAINPLL_D3, "mainpll_d3",
			"mainpll", 1, 3),
	FACTOR(CLK_TOP_MAINPLL_D4, "mainpll_d4",
			"mainpll", 1, 4),
	FACTOR(CLK_TOP_MAINPLL_D4_D2, "mainpll_d4_d2",
			"mainpll", 1, 8),
	FACTOR(CLK_TOP_MAINPLL_D4_D4, "mainpll_d4_d4",
			"mainpll", 1, 16),
	FACTOR(CLK_TOP_MAINPLL_D4_D8, "mainpll_d4_d8",
			"mainpll", 43, 1375),
	FACTOR(CLK_TOP_MAINPLL_D5, "mainpll_d5",
			"mainpll", 1, 5),
	FACTOR(CLK_TOP_MAINPLL_D5_D2, "mainpll_d5_d2",
			"mainpll", 1, 10),
	FACTOR(CLK_TOP_MAINPLL_D5_D4, "mainpll_d5_d4",
			"mainpll", 1, 20),
	FACTOR(CLK_TOP_MAINPLL_D5_D8, "mainpll_d5_d8",
			"mainpll", 1, 40),
	FACTOR(CLK_TOP_MAINPLL_D6, "mainpll_d6",
			"mainpll", 1, 6),
	FACTOR(CLK_TOP_MAINPLL_D6_D2, "mainpll_d6_d2",
			"mainpll", 1, 12),
	FACTOR(CLK_TOP_MAINPLL_D6_D4, "mainpll_d6_d4",
			"mainpll", 1, 24),
	FACTOR(CLK_TOP_MAINPLL_D6_D8, "mainpll_d6_d8",
			"mainpll", 1, 48),
	FACTOR(CLK_TOP_MAINPLL_D7, "mainpll_d7",
			"mainpll", 1, 7),
	FACTOR(CLK_TOP_MAINPLL_D7_D2, "mainpll_d7_d2",
			"mainpll", 1, 14),
	FACTOR(CLK_TOP_MAINPLL_D7_D4, "mainpll_d7_d4",
			"mainpll", 1, 28),
	FACTOR(CLK_TOP_MAINPLL_D7_D8, "mainpll_d7_d8",
			"mainpll", 1, 56),
	FACTOR(CLK_TOP_MAINPLL_D9, "mainpll_d9",
			"mainpll", 1, 9),
	FACTOR(CLK_TOP_UNIVPLL, "univpll_ck",
			"univpll", 1, 1),
	FACTOR(CLK_TOP_UNIVPLL_D2, "univpll_d2",
			"univpll", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3",
			"univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL_D4, "univpll_d4",
			"univpll", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D4_D2, "univpll_d4_d2",
			"univpll", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_D4_D4, "univpll_d4_d4",
			"univpll", 1, 16),
	FACTOR(CLK_TOP_UNIVPLL_D4_D8, "univpll_d4_d8",
			"univpll", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5",
			"univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL_D5_D2, "univpll_d5_d2",
			"univpll", 1, 10),
	FACTOR(CLK_TOP_UNIVPLL_D5_D4, "univpll_d5_d4",
			"univpll", 1, 20),
	FACTOR(CLK_TOP_UNIVPLL_D6, "univpll_d6",
			"univpll", 1, 6),
	FACTOR(CLK_TOP_UNIVPLL_D6_D2, "univpll_d6_d2",
			"univpll", 1, 12),
	FACTOR(CLK_TOP_UNIVPLL_D6_D4, "univpll_d6_d4",
			"univpll", 1, 24),
	FACTOR(CLK_TOP_UNIVPLL_D6_D8, "univpll_d6_d8",
			"univpll", 1, 48),
	FACTOR(CLK_TOP_UNIVPLL_D6_D16, "univpll_d6_d16",
			"univpll", 1, 96),
	FACTOR(CLK_TOP_UNIVPLL_D7, "univpll_d7",
			"univpll", 1, 7),
	FACTOR(CLK_TOP_UNIVPLL_D7_D2, "univpll_d7_d2",
			"univpll", 1, 14),
	FACTOR(CLK_TOP_UNIVPLL_D7_D3, "univpll_d7_d3",
			"univpll", 1, 21),
	FACTOR(CLK_TOP_LVDSTX_DG_CTS, "lvdstx_dg_cts_ck",
			"univpll", 1, 21),
	FACTOR(CLK_TOP_UNIVPLL_192M, "univpll_192m_ck",
			"univpll", 1, 13),
	FACTOR(CLK_TOP_UNIVPLL_192M_D2, "univpll_192m_d2",
			"univpll", 1, 26),
	FACTOR(CLK_TOP_UNIVPLL_192M_D4, "univpll_192m_d4",
			"univpll", 1, 52),
	FACTOR(CLK_TOP_UNIVPLL_192M_D8, "univpll_192m_d8",
			"univpll", 1, 104),
	FACTOR(CLK_TOP_UNIVPLL_192M_D10, "univpll_192m_d10",
			"univpll", 1, 130),
	FACTOR(CLK_TOP_UNIVPLL_192M_D16, "univpll_192m_d16",
			"univpll", 1, 208),
	FACTOR(CLK_TOP_UNIVPLL_192M_D32, "univpll_192m_d32",
			"univpll", 1, 416),
	FACTOR(CLK_TOP_APLL1, "apll1_ck",
			"apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D2, "apll1_d2",
			"apll1", 1, 2),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4",
			"apll1", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "apll1_d8",
			"apll1", 1, 8),
	FACTOR(CLK_TOP_APLL1_D3, "apll1_d3",
			"apll1", 1, 3),
	FACTOR(CLK_TOP_APLL2, "apll2_ck",
			"apll2", 1, 1),
	FACTOR(CLK_TOP_APLL2_D2, "apll2_d2",
			"apll2", 1, 2),
	FACTOR(CLK_TOP_APLL2_D4, "apll2_d4",
			"apll2", 1, 4),
	FACTOR(CLK_TOP_APLL2_D8, "apll2_d8",
			"apll2", 1, 8),
	FACTOR(CLK_TOP_APLL2_D3, "apll2_d3",
			"apll2", 1, 3),
	FACTOR(CLK_TOP_MFGPLL, "mfgpll_ck",
			"mfgpll", 1, 1),
	FACTOR(CLK_TOP_MMPLL, "mmpll_ck",
			"mmpll", 1, 1),
	FACTOR(CLK_TOP_MMPLL_D4, "mmpll_d4",
			"mmpll", 1, 4),
	FACTOR(CLK_TOP_MMPLL_D4_D2, "mmpll_d4_d2",
			"mmpll", 1, 8),
	FACTOR(CLK_TOP_MMPLL_D4_D4, "mmpll_d4_d4",
			"mmpll", 1, 16),
	FACTOR(CLK_TOP_VPLL_DPIX, "vpll_dpix_ck",
			"mmpll", 1, 16),
	FACTOR(CLK_TOP_MMPLL_D5, "mmpll_d5",
			"mmpll", 1, 5),
	FACTOR(CLK_TOP_MMPLL_D5_D2, "mmpll_d5_d2",
			"mmpll", 1, 10),
	FACTOR(CLK_TOP_MMPLL_D5_D4, "mmpll_d5_d4",
			"mmpll", 1, 20),
	FACTOR(CLK_TOP_MMPLL_D6, "mmpll_d6",
			"mmpll", 1, 6),
	FACTOR(CLK_TOP_MMPLL_D6_D2, "mmpll_d6_d2",
			"mmpll", 1, 12),
	FACTOR(CLK_TOP_MMPLL_D7, "mmpll_d7",
			"mmpll", 1, 7),
	FACTOR(CLK_TOP_MMPLL_D9, "mmpll_d9",
			"mmpll", 1, 9),
	FACTOR(CLK_TOP_TVDPLL1, "tvdpll1_ck",
			"tvdpll1", 1, 1),
	FACTOR(CLK_TOP_TVDPLL1_D2, "tvdpll1_d2",
			"tvdpll1", 1, 2),
	FACTOR(CLK_TOP_TVDPLL1_D4, "tvdpll1_d4",
			"tvdpll1", 1, 4),
	FACTOR(CLK_TOP_TVDPLL1_D8, "tvdpll1_d8",
			"tvdpll1", 1, 8),
	FACTOR(CLK_TOP_TVDPLL1_D16, "tvdpll1_d16",
			"tvdpll1", 92, 1473),
	FACTOR(CLK_TOP_TVDPLL2, "tvdpll2_ck",
			"tvdpll2", 1, 1),
	FACTOR(CLK_TOP_TVDPLL2_D2, "tvdpll2_d2",
			"tvdpll2", 1, 2),
	FACTOR(CLK_TOP_TVDPLL2_D4, "tvdpll2_d4",
			"tvdpll2", 1, 4),
	FACTOR(CLK_TOP_TVDPLL2_D8, "tvdpll2_d8",
			"tvdpll2", 1, 8),
	FACTOR(CLK_TOP_TVDPLL2_D16, "tvdpll2_d16",
			"tvdpll2", 92, 1473),
	FACTOR(CLK_TOP_ETHPLL, "ethpll_ck",
			"ethpll", 1, 1),
	FACTOR(CLK_TOP_ETHPLL_D2, "ethpll_d2",
			"ethpll", 1, 2),
	FACTOR(CLK_TOP_ETHPLL_D8, "ethpll_d8",
			"ethpll", 1, 8),
	FACTOR(CLK_TOP_ETHPLL_D10, "ethpll_d10",
			"ethpll", 1, 10),
	FACTOR(CLK_TOP_EMIPLL, "emipll_ck",
			"emipll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL, "msdcpll_ck",
			"msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2",
			"msdcpll", 1, 2),
	FACTOR(CLK_TOP_UFSPLL, "ufspll_ck",
			"ufspll", 1, 1),
	FACTOR(CLK_TOP_UFSPLL_D2, "ufspll_d2",
			"ufspll", 1, 2),
	FACTOR(CLK_TOP_APUPLL, "apupll_ck",
			"apupll", 1, 1),
	FACTOR(CLK_TOP_CLKRTC, "clkrtc",
			"clk32k", 1, 1),
	FACTOR(CLK_TOP_RTC32K_CK_I, "rtc32k_i",
			"clk32k", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX9, "tck_26m_mx9_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_VOWPLL, "vowpll_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_F26M_CK_D2, "f26m_d2",
			"clk26m", 1, 2),
	FACTOR(CLK_TOP_OSC, "osc_ck",
			"ulposc", 1, 1),
	FACTOR(CLK_TOP_OSC_D2, "osc_d2",
			"ulposc", 1, 2),
	FACTOR(CLK_TOP_OSC_D4, "osc_d4",
			"ulposc", 1, 4),
	FACTOR(CLK_TOP_OSC_D8, "osc_d8",
			"ulposc", 1, 8),
	FACTOR(CLK_TOP_OSC_D16, "osc_d16",
			"ulposc", 61, 973),
	FACTOR(CLK_TOP_OSC_D3, "osc_d3",
			"ulposc", 1, 3),
	FACTOR(CLK_TOP_OSC_D7, "osc_d7",
			"ulposc", 1, 7),
	FACTOR(CLK_TOP_OSC_D10, "osc_d10",
			"ulposc", 1, 10),
	FACTOR(CLK_TOP_OSC_D20, "osc_d20",
			"ulposc", 1, 20),
	FACTOR(CLK_TOP_F26M, "f26m_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_RTC, "rtc_ck",
			"clk32k", 1, 1),
	FACTOR(CLK_TOP_AXI, "axi_ck",
			"axi_sel", 1, 1),
	FACTOR(CLK_TOP_AXI_PERI, "axi_peri_ck",
			"axi_peri_sel", 1, 1),
	FACTOR(CLK_TOP_AXI_UFS, "axi_u_ck",
			"axi_u_sel", 1, 1),
	FACTOR(CLK_TOP_BUS_AXIMEM, "bus_aximem_ck",
			"bus_aximem_sel", 1, 1),
	FACTOR(CLK_TOP_DISP0, "disp0_ck",
			"disp0_sel", 1, 1),
	FACTOR(CLK_TOP_MMINFRA, "mminfra_ck",
			"mminfra_sel", 1, 1),
	FACTOR(CLK_TOP_UART, "uart_ck",
			"uart_sel", 1, 1),
	FACTOR(CLK_TOP_SPI0, "spi0_ck",
			"spi0_sel", 1, 1),
	FACTOR(CLK_TOP_SPI1, "spi1_ck",
			"spi1_sel", 1, 1),
	FACTOR(CLK_TOP_SPI2, "spi2_ck",
			"spi2_sel", 1, 1),
	FACTOR(CLK_TOP_SPI3, "spi3_ck",
			"spi3_sel", 1, 1),
	FACTOR(CLK_TOP_SPI4, "spi4_ck",
			"spi4_sel", 1, 1),
	FACTOR(CLK_TOP_SPI5, "spi5_ck",
			"spi5_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC50_0_HCLK, "msdc5hclk_ck",
			"msdc5hclk_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC50_0, "msdc50_0_ck",
			"msdc50_0_sel", 1, 1),
	FACTOR(CLK_TOP_AES_MSDCFDE, "aes_msdcfde_ck",
			"aes_msdcfde_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_1, "msdc30_1_ck",
			"msdc30_1_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_1_HCLK, "msdc30_1_h_ck",
			"msdc30_1_h_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_2, "msdc30_2_ck",
			"msdc30_2_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_2_HCLK, "msdc30_2_h_ck",
			"msdc30_2_h_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_INTBUS, "aud_intbus_ck",
			"aud_intbus_sel", 1, 1),
	FACTOR(CLK_TOP_ATB, "atb_ck",
			"atb_sel", 1, 1),
	FACTOR(CLK_TOP_DISP_PWM, "disp_pwm_ck",
			"disp_pwm_sel", 1, 1),
	FACTOR(CLK_TOP_USB_TOP_P0, "usb_p0_ck",
			"usb_p0_sel", 1, 1),
	FACTOR(CLK_TOP_USB_XHCI_P0, "ssusb_xhci_p0_ck",
			"ssusb_xhci_p0_sel", 1, 1),
	FACTOR(CLK_TOP_USB_TOP_P1, "usb_p1_ck",
			"usb_p1_sel", 1, 1),
	FACTOR(CLK_TOP_USB_XHCI_P1, "ssusb_xhci_p1_ck",
			"ssusb_xhci_p1_sel", 1, 1),
	FACTOR(CLK_TOP_USB_TOP_P2, "usb_p2_ck",
			"usb_p2_sel", 1, 1),
	FACTOR(CLK_TOP_USB_XHCI_P2, "ssusb_xhci_p2_ck",
			"ssusb_xhci_p2_sel", 1, 1),
	FACTOR(CLK_TOP_USB_TOP_P3, "usb_p3_ck",
			"usb_p3_sel", 1, 1),
	FACTOR(CLK_TOP_USB_XHCI_P3, "ssusb_xhci_p3_ck",
			"ssusb_xhci_p3_sel", 1, 1),
	FACTOR(CLK_TOP_USB_TOP_P4, "usb_p4_ck",
			"usb_p4_sel", 1, 1),
	FACTOR(CLK_TOP_USB_XHCI_P4, "ssusb_xhci_p4_ck",
			"ssusb_xhci_p4_sel", 1, 1),
	FACTOR(CLK_TOP_I2C, "i2c_ck",
			"i2c_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN1, "aud_engen1_ck",
			"aud_engen1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN2, "aud_engen2_ck",
			"aud_engen2_sel", 1, 1),
	FACTOR(CLK_TOP_AES_UFSFDE, "aes_ufsfde_ck",
			"aes_ufsfde_sel", 1, 1),
	FACTOR(CLK_TOP_UFS, "ufs_ck",
			"ufs_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_1, "aud_1_ck",
			"aud_1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_2, "aud_2_ck",
			"aud_2_sel", 1, 1),
	FACTOR(CLK_TOP_VENC, "venc_ck",
			"venc_sel", 1, 1),
	FACTOR(CLK_TOP_VDEC, "vdec_ck",
			"vdec_sel", 1, 1),
	FACTOR(CLK_TOP_PWM, "pwm_ck",
			"pwm_sel", 1, 1),
	FACTOR(CLK_TOP_AUDIO_H, "audio_h_ck",
			"audio_h_sel", 1, 1),
	FACTOR(CLK_TOP_MEM_SUB, "mem_sub_ck",
			"mem_sub_sel", 1, 1),
	FACTOR(CLK_TOP_MEM_SUB_PERI, "mem_sub_peri_ck",
			"mem_sub_peri_sel", 1, 1),
	FACTOR(CLK_TOP_MEM_SUB_UFS, "mem_sub_u_ck",
			"mem_sub_u_sel", 1, 1),
	FACTOR(CLK_TOP_DSI_OCC, "dsi_occ_ck",
			"dsi_occ_sel", 1, 1),
	FACTOR(CLK_TOP_IMG1, "img1_ck",
			"img1_sel", 1, 1),
	FACTOR(CLK_TOP_IPE, "ipe_ck",
			"ipe_sel", 1, 1),
	FACTOR(CLK_TOP_CAM, "cam_ck",
			"cam_sel", 1, 1),
	FACTOR(CLK_TOP_DXCC, "dxcc_ck",
			"dxcc_sel", 1, 1),
	FACTOR(CLK_TOP_MDP0, "mdp0_ck",
			"mdp0_sel", 1, 1),
	FACTOR(CLK_TOP_DP, "dp_ck",
			"dp_sel", 1, 1),
	FACTOR(CLK_TOP_EDP, "edp_ck",
			"edp_sel", 1, 1),
	FACTOR(CLK_TOP_EDP_FAVT, "edp_favt_ck",
			"edp_favt_sel", 1, 1),
	FACTOR(CLK_TOP_SFLASH, "sflash_ck",
			"sflash_sel", 1, 1),
	FACTOR(CLK_TOP_MAC_TL, "pcie_mac_tl_ck",
			"pcie_mac_tl_sel", 1, 1),
	FACTOR(CLK_TOP_VDSTX_DG_CTS, "vdstx_dg_cts_ck",
			"vdstx_dg_cts_sel", 1, 1),
	FACTOR(CLK_TOP_PLL_DPIX, "pll_dpix_ck",
			"pll_dpix_sel", 1, 1),
	FACTOR(CLK_TOP_ECC, "ecc_ck",
			"ecc_sel", 1, 1),
	FACTOR(CLK_TOP_USB_FRMCNT_CK_P0, "ssusb_frmcnt_p0",
			"univpll_192m_d4", 1, 1),
	FACTOR(CLK_TOP_USB_FRMCNT_CK_P1, "ssusb_frmcnt_p1",
			"univpll_192m_d4", 1, 1),
	FACTOR(CLK_TOP_USB_FRMCNT_CK_P2, "ssusb_frmcnt_p2",
			"univpll_192m_d4", 1, 1),
	FACTOR(CLK_TOP_USB_FRMCNT_CK_P3, "ssusb_frmcnt_p3",
			"univpll_192m_d4", 1, 1),
	FACTOR(CLK_TOP_USB_FRMCNT_CK_P4, "ssusb_frmcnt_p4",
			"univpll_192m_d4", 1, 1),
};

static const char * const vlp_scp_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4",
	"univpll_d3",
	"mainpll_d3",
	"univpll_d6",
	"apll1_ck",
	"mainpll_d4",
	"mainpll_d6",
	"mainpll_d7",
	"osc_d10"
};

static const char * const vlp_pwrap_ulposc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d7",
	"osc_d8",
	"osc_d16",
	"mainpll_d7_d8"
};

static const char * const vlp_spmi_p_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"osc_d8",
	"osc_d10",
	"osc_d16",
	"osc_d7",
	"clkrtc",
	"mainpll_d7_d8",
	"mainpll_d6_d8",
	"mainpll_d5_d8"
};

static const char * const vlp_dvfsrc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const vlp_pwm_vlp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d4",
	"clkrtc",
	"osc_d10",
	"mainpll_d4_d8"
};

static const char * const vlp_axi_vlp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d2",
	"mainpll_d7_d4",
	"mainpll_d7_d2"
};

static const char * const vlp_systimer_26m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const vlp_sspm_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"mainpll_d5_d2",
	"osc_ck",
	"mainpll_d6"
};

static const char * const vlp_sspm_f26m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const vlp_srck_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const vlp_scp_spi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d4",
	"mainpll_d7_d2",
	"osc_d10"
};

static const char * const vlp_scp_iic_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d4",
	"mainpll_d7_d2",
	"osc_d10"
};

static const char * const vlp_scp_spi_hs_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d4",
	"mainpll_d7_d2",
	"osc_d10"
};

static const char * const vlp_scp_iic_hs_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d4",
	"mainpll_d7_d2",
	"osc_d10"
};

static const char * const vlp_sspm_ulposc_parents[] = {
	"osc_ck",
	"univpll_d5_d2",
	"osc_d10"
};

static const char * const vlp_apxgpt_26m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const vlp_vadsp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"osc_d10",
	"osc_d2",
	"osc_ck",
	"mainpll_d4_d2"
};

static const char * const vlp_vadsp_vowpll_parents[] = {
	"tck_26m_mx9_ck",
	"vowpll_ck"
};

static const char * const vlp_vadsp_uarthub_b_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"univpll_d6_d4",
	"univpll_d6_d2"
};

static const char * const vlp_camtg0_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"osc_d16",
	"osc_d20",
	"osc_d10",
	"univpll_d6_d16",
	"tvdpll1_d16",
	"f26m_d2",
	"univpll_192m_d10",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const vlp_camtg1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"osc_d16",
	"osc_d20",
	"osc_d10",
	"univpll_d6_d16",
	"tvdpll1_d16",
	"f26m_d2",
	"univpll_192m_d10",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const vlp_camtg2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"osc_d16",
	"osc_d20",
	"osc_d10",
	"univpll_d6_d16",
	"tvdpll1_d16",
	"f26m_d2",
	"univpll_192m_d10",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const vlp_aud_adc_parents[] = {
	"tck_26m_mx9_ck",
	"vowpll_ck",
	"aud_adc_ext_ck",
	"osc_d10"
};

static const char * const vlp_kp_irq_gen_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d2",
	"mainpll_d7_d4",
	"mainpll_d7_d2"
};

static const struct mtk_mux vlp_ck_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* VLP_CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_P_MST_SEL/* dts */, "vlp_spmi_p_sel",
		vlp_spmi_p_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_P_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWM_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_F26M_SEL/* dts */, "vlp_sspm_f26m_sel",
		vlp_sspm_f26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_F26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SPI_HIGH_SPD_SEL/* dts */, "vlp_scp_spi_hs_sel",
		vlp_scp_spi_hs_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_ULPOSC_SEL/* dts */, "vlp_sspm_ulposc_sel",
		vlp_sspm_ulposc_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_APXGPT_26M_SEL/* dts */, "vlp_apxgpt_26m_sel",
		vlp_apxgpt_26m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APXGPT_26M_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_VADSP_SEL/* dts */, "vlp_vadsp_sel",
		vlp_vadsp_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VADSP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_VADSP_VOWPLL_SEL/* dts */, "vlp_vadsp_vowpll_sel",
		vlp_vadsp_vowpll_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VADSP_VOWPLL_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_VADSP_UARTHUB_BCLK_SEL/* dts */, "vlp_vadsp_uarthub_b_sel",
		vlp_vadsp_uarthub_b_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VADSP_UARTHUB_BCLK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_CAMTG0_SEL/* dts */, "vlp_camtg0_sel",
		vlp_camtg0_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG0_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_CAMTG1_SEL/* dts */, "vlp_camtg1_sel",
		vlp_camtg1_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_CAMTG2_SEL/* dts */, "vlp_camtg2_sel",
		vlp_camtg2_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_AUD_ADC_SEL/* dts */, "vlp_aud_adc_sel",
		vlp_aud_adc_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_ADC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_KP_IRQ_GEN_SEL/* dts */, "vlp_kp_irq_gen_sel",
		vlp_kp_irq_gen_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_KP_IRQ_GEN_SHIFT/* upd shift */),
#else
	/* VLP_CLK_CFG_0 */
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_P_MST_SEL/* dts */, "vlp_spmi_p_sel",
		vlp_spmi_p_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPMI_P_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_PWM_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SSPM_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_F26M_SEL/* dts */, "vlp_sspm_f26m_sel",
		vlp_sspm_f26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SSPM_F26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SRCK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SCP_SPI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SCP_IIC_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SPI_HIGH_SPD_SEL/* dts */, "vlp_scp_spi_hs_sel",
		vlp_scp_spi_hs_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_ULPOSC_SEL/* dts */, "vlp_sspm_ulposc_sel",
		vlp_sspm_ulposc_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SSPM_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_APXGPT_26M_SEL/* dts */, "vlp_apxgpt_26m_sel",
		vlp_apxgpt_26m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_APXGPT_26M_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_VADSP_SEL/* dts */, "vlp_vadsp_sel",
		vlp_vadsp_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_VADSP_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_VADSP_VOWPLL_SEL/* dts */, "vlp_vadsp_vowpll_sel",
		vlp_vadsp_vowpll_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_VADSP_VOWPLL_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_VADSP_UARTHUB_BCLK_SEL/* dts */, "vlp_vadsp_uarthub_b_sel",
		vlp_vadsp_uarthub_b_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_VADSP_UARTHUB_BCLK_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_CAMTG0_SEL/* dts */, "vlp_camtg0_sel",
		vlp_camtg0_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG0_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_CAMTG1_SEL/* dts */, "vlp_camtg1_sel",
		vlp_camtg1_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_CAMTG2_SEL/* dts */, "vlp_camtg2_sel",
		vlp_camtg2_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_AUD_ADC_SEL/* dts */, "vlp_aud_adc_sel",
		vlp_aud_adc_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_ADC_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_KP_IRQ_GEN_SEL/* dts */, "vlp_kp_irq_gen_sel",
		vlp_kp_irq_gen_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_KP_IRQ_GEN_SHIFT/* upd shift */),
#endif
};

static const char * const axi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d2",
	"mainpll_d4_d2",
	"mainpll_d5_d2",
	"mainpll_d6_d2",
	"osc_d4"
};

static const char * const axi_peri_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d2",
	"osc_d4"
};

static const char * const axi_u_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d8",
	"mainpll_d7_d4",
	"osc_d8"
};

static const char * const bus_aximem_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6"
};

static const char * const disp0_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"univpll_d5_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"univpll_d6",
	"mmpll_d6",
	"tvdpll1_ck",
	"tvdpll2_ck",
	"univpll_d4",
	"mmpll_d4"
};

static const char * const mminfra_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d4_d2",
	"mmpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"univpll_d6",
	"mainpll_d5",
	"mmpll_d6",
	"univpll_d5",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d4",
	"emipll_ck"
};

static const char * const uart_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d8"
};

static const char * const spi0_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_192m_ck",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const spi1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_192m_ck",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const spi2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_192m_ck",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const spi3_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_192m_ck",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const spi4_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_192m_ck",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const spi5_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_192m_ck",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const msdc_macro_0p_parents[] = {
	"tck_26m_mx9_ck",
	"msdcpll_ck",
	"mmpll_d5_d4",
	"univpll_d4_d2"
};

static const char * const msdc5hclk_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d6_d2"
};

static const char * const msdc50_0_parents[] = {
	"tck_26m_mx9_ck",
	"msdcpll_ck",
	"msdcpll_d2",
	"mainpll_d6_d2",
	"mainpll_d4_d4",
	"mainpll_d6",
	"univpll_d4_d4"
};

static const char * const aes_msdcfde_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d4_d4",
	"msdcpll_ck"
};

static const char * const msdc_macro_1p_parents[] = {
	"tck_26m_mx9_ck",
	"msdcpll_ck",
	"mmpll_d5_d4",
	"univpll_d4_d2"
};

static const char * const msdc30_1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"mainpll_d7_d2",
	"msdcpll_d2"
};

static const char * const msdc30_1_h_parents[] = {
	"tck_26m_mx9_ck",
	"msdcpll_d2",
	"mainpll_d4_d4",
	"mainpll_d6_d4"
};

static const char * const msdc_macro_2p_parents[] = {
	"tck_26m_mx9_ck",
	"msdcpll_ck",
	"mmpll_d5_d4",
	"univpll_d4_d2"
};

static const char * const msdc30_2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"mainpll_d7_d2",
	"msdcpll_d2"
};

static const char * const msdc30_2_h_parents[] = {
	"tck_26m_mx9_ck",
	"msdcpll_d2",
	"mainpll_d4_d4",
	"mainpll_d6_d4"
};

static const char * const aud_intbus_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d4"
};

static const char * const atb_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d5_d2"
};

static const char * const disp_pwm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"osc_d2",
	"osc_d4",
	"osc_d16",
	"univpll_d5_d4",
	"mainpll_d4_d4"
};

static const char * const usb_p0_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const ssusb_xhci_p0_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const usb_p1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const ssusb_xhci_p1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const usb_p2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const ssusb_xhci_p2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const usb_p3_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const ssusb_xhci_p3_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const usb_p4_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const ssusb_xhci_p4_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const i2c_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d8",
	"univpll_d5_d4",
	"mainpll_d4_d4"
};

static const char * const seninf_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mmpll_d7",
	"univpll_d6",
	"univpll_d5"
};

static const char * const seninf1_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mmpll_d7",
	"univpll_d6",
	"univpll_d5"
};

static const char * const aud_engen1_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_d2",
	"apll1_d4",
	"apll1_d8"
};

static const char * const aud_engen2_parents[] = {
	"tck_26m_mx9_ck",
	"apll2_d2",
	"apll2_d4",
	"apll2_d8"
};

static const char * const aes_ufsfde_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d4_d4",
	"univpll_d4_d2",
	"univpll_d6"
};

static const char * const ufs_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d8",
	"mainpll_d4_d4",
	"mainpll_d5_d2",
	"mainpll_d6_d2",
	"univpll_d6_d2",
	"msdcpll_d2"
};

static const char * const ufs_mbist_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"ufspll_d2"
};

static const char * const aud_1_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_ck"
};

static const char * const aud_2_parents[] = {
	"tck_26m_mx9_ck",
	"apll2_ck"
};

static const char * const venc_parents[] = {
	"tck_26m_mx9_ck",
	"mmpll_d4_d2",
	"mainpll_d6",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"univpll_d6",
	"mmpll_d6",
	"mainpll_d5_d2",
	"mainpll_d6_d2",
	"mmpll_d9",
	"mmpll_d4",
	"mainpll_d4",
	"univpll_d4",
	"univpll_d5",
	"univpll_d5_d2",
	"mainpll_d5"
};

static const char * const vdec_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d2",
	"univpll_d5_d4",
	"mainpll_d5",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"univpll_d5_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"mmpll_d7",
	"mmpll_d6",
	"univpll_d6",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d5_d2"
};

static const char * const pwm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d8"
};

static const char * const audio_h_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d7_d2",
	"apll1_ck",
	"apll2_ck"
};

static const char * const mcupm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d5_d2",
	"mainpll_d6_d2"
};

static const char * const mem_sub_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"mainpll_d5",
	"univpll_d5",
	"mainpll_d4",
	"univpll_d4"
};

static const char * const mem_sub_peri_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d5",
	"univpll_d5",
	"mainpll_d4"
};

static const char * const mem_sub_u_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d5",
	"univpll_d5",
	"mainpll_d4"
};

static const char * const emi_n_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"mainpll_d9",
	"mainpll_d6",
	"mainpll_d5",
	"emipll_ck"
};

static const char * const dsi_occ_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_d5_d2",
	"univpll_d4_d2"
};

static const char * const ap2conn_host_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4"
};

static const char * const img1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4",
	"mmpll_d5",
	"mmpll_d6",
	"univpll_d6",
	"mmpll_d7",
	"mmpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"mmpll_d6_d2",
	"mmpll_d5_d2"
};

static const char * const ipe_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4",
	"mainpll_d4",
	"mmpll_d6",
	"univpll_d6",
	"mainpll_d6",
	"mmpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"mmpll_d6_d2",
	"mmpll_d5_d2"
};

static const char * const cam_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4",
	"mmpll_d4",
	"univpll_d4",
	"univpll_d5",
	"mmpll_d7",
	"mmpll_d6",
	"univpll_d6",
	"univpll_d4_d2",
	"mmpll_d9",
	"mainpll_d4_d2",
	"osc_d2"
};

static const char * const camtm_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"univpll_d6_d2",
	"univpll_d6_d4"
};

static const char * const dsp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d4",
	"osc_d3",
	"osc_d2",
	"univpll_d7_d2",
	"univpll_d6_d2",
	"mainpll_d6",
	"univpll_d5"
};

static const char * const sr_pka_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d4_d2",
	"mainpll_d7",
	"mainpll_d6",
	"mainpll_d5"
};

static const char * const dxcc_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d8",
	"mainpll_d4_d4",
	"mainpll_d4_d2"
};

static const char * const mfg_ref_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6_d2",
	"mainpll_d6",
	"mainpll_d5_d2"
};

static const char * const mdp0_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"univpll_d5_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"univpll_d6",
	"mmpll_d6",
	"tvdpll1_ck",
	"tvdpll2_ck",
	"univpll_d4",
	"mmpll_d4"
};

static const char * const dp_parents[] = {
	"tck_26m_mx9_ck",
	"tvdpll1_d16",
	"tvdpll1_d8",
	"tvdpll1_d4",
	"tvdpll1_d2"
};

static const char * const edp_parents[] = {
	"tck_26m_mx9_ck",
	"tvdpll2_d16",
	"tvdpll2_d8",
	"tvdpll2_d4",
	"tvdpll2_d2",
	"apll1_d4",
	"apll2_d4"
};

static const char * const edp_favt_parents[] = {
	"tck_26m_mx9_ck",
	"tvdpll2_d16",
	"tvdpll2_d8",
	"tvdpll2_d4",
	"tvdpll2_d2",
	"apll1_d4",
	"apll2_d4"
};

static const char * const snps_eth_250m_parents[] = {
	"tck_26m_mx9_ck",
	"ethpll_d2"
};

static const char * const snps_eth_62p4m_ptp_parents[] = {
	"tck_26m_mx9_ck",
	"ethpll_d8",
	"apll1_d3",
	"apll2_d3"
};

static const char * const snps_eth_50m_rmii_parents[] = {
	"tck_26m_mx9_ck",
	"ethpll_d10"
};

static const char * const sflash_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d8",
	"univpll_d6_d8",
	"mainpll_d7_d4",
	"mainpll_d6_d4",
	"univpll_d6_d4",
	"univpll_d7_d3",
	"univpll_d5_d4"
};

static const char * const gcpu_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d5_d2",
	"univpll_d5_d4",
	"univpll_d6"
};

static const char * const pcie_mac_tl_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"univpll_d5_d4"
};

static const char * const vdstx_dg_cts_parents[] = {
	"tck_26m_mx9_ck",
	"lvdstx_dg_cts_ck",
	"univpll_d7_d3"
};

static const char * const pll_dpix_parents[] = {
	"tck_26m_mx9_ck",
	"vpll_dpix_ck",
	"mmpll_d4_d4"
};

static const char * const ecc_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"univpll_d6",
	"mainpll_d4",
	"univpll_d4"
};

static const char * const apll_i2sin0_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin1_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin2_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin3_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin4_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin6_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout0_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout1_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout2_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout3_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout4_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout6_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_fmi2s_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_tdmout_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const mfg_sel_mfgpll_parents[] = {
	"mfg_ref_sel",
	"mfgpll_ck"
};

static const struct mtk_mux top_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_TOP_AXI_SEL/* dts */, "axi_sel",
		axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AXI_PERI_SEL/* dts */, "axi_peri_sel",
		axi_peri_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_PERI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AXI_U_SEL/* dts */, "axi_u_sel",
		axi_u_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_UFS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "bus_aximem_sel",
		bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_TOP_DISP0_SEL/* dts */, "disp0_sel",
		disp0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MMINFRA_SEL/* dts */, "mminfra_sel",
		mminfra_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_UART_SEL/* dts */, "uart_sel",
		uart_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UART_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI0_SEL/* dts */, "spi0_sel",
		spi0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI0_SHIFT/* upd shift */),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_TOP_SPI1_SEL/* dts */, "spi1_sel",
		spi1_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI2_SEL/* dts */, "spi2_sel",
		spi2_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI3_SEL/* dts */, "spi3_sel",
		spi3_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI3_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI4_SEL/* dts */, "spi4_sel",
		spi4_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI4_SHIFT/* upd shift */),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_TOP_SPI5_SEL/* dts */, "spi5_sel",
		spi5_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI5_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC_MACRO_0P_SEL/* dts */, "msdc_macro_0p_sel",
		msdc_macro_0p_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC_MACRO_0P_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL/* dts */, "msdc5hclk_sel",
		msdc5hclk_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC50_0_HCLK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL/* dts */, "msdc50_0_sel",
		msdc50_0_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC50_0_SHIFT/* upd shift */),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_TOP_AES_MSDCFDE_SEL/* dts */, "aes_msdcfde_sel",
		aes_msdcfde_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AES_MSDCFDE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC_MACRO_1P_SEL/* dts */, "msdc_macro_1p_sel",
		msdc_macro_1p_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL/* dts */, "msdc30_1_sel",
		msdc30_1_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC30_1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC30_1_HCLK_SEL/* dts */, "msdc30_1_h_sel",
		msdc30_1_h_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC30_1_HCLK_SHIFT/* upd shift */),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_TOP_MSDC_MACRO_2P_SEL/* dts */, "msdc_macro_2p_sel",
		msdc_macro_2p_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC_MACRO_2P_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC30_2_SEL/* dts */, "msdc30_2_sel",
		msdc30_2_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC30_2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC30_2_HCLK_SEL/* dts */, "msdc30_2_h_sel",
		msdc30_2_h_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC30_2_HCLK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL/* dts */, "aud_intbus_sel",
		aud_intbus_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD(CLK_TOP_ATB_SEL/* dts */, "atb_sel",
		atb_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL/* dts */, "disp_pwm_sel",
		disp_pwm_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP_PWM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USB_TOP_P0_SEL/* dts */, "usb_p0_sel",
		usb_p0_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_USB_TOP_P0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USB_XHCI_P0_SEL/* dts */, "ssusb_xhci_p0_sel",
		ssusb_xhci_p0_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSUSB_XHCI_P0_SHIFT/* upd shift */),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD(CLK_TOP_USB_TOP_P1_SEL/* dts */, "usb_p1_sel",
		usb_p1_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_USB_TOP_P1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USB_XHCI_P1_SEL/* dts */, "ssusb_xhci_p1_sel",
		ssusb_xhci_p1_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSUSB_XHCI_P1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USB_TOP_P2_SEL/* dts */, "usb_p2_sel",
		usb_p2_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_USB_TOP_P2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USB_XHCI_P2_SEL/* dts */, "ssusb_xhci_p2_sel",
		ssusb_xhci_p2_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_XHCI_P2_SHIFT/* upd shift */),
	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD(CLK_TOP_USB_TOP_P3_SEL/* dts */, "usb_p3_sel",
		usb_p3_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USB_TOP_P3_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USB_XHCI_P3_SEL/* dts */, "ssusb_xhci_p3_sel",
		ssusb_xhci_p3_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_XHCI_P3_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USB_TOP_P4_SEL/* dts */, "usb_p4_sel",
		usb_p4_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USB_TOP_P4_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USB_XHCI_P4_SEL/* dts */, "ssusb_xhci_p4_sel",
		ssusb_xhci_p4_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_XHCI_P4_SHIFT/* upd shift */),
	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD(CLK_TOP_I2C_SEL/* dts */, "i2c_sel",
		i2c_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_I2C_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF_SEL/* dts */, "seninf_sel",
		seninf_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SENINF_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF1_SEL/* dts */, "seninf1_sel",
		seninf1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SENINF1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "aud_engen1_sel",
		aud_engen1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "aud_engen2_sel",
		aud_engen2_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AES_UFSFDE_SEL/* dts */, "aes_ufsfde_sel",
		aes_ufsfde_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_U_SEL/* dts */, "ufs_sel",
		ufs_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_U_MBIST_SEL/* dts */, "ufs_mbist_sel",
		ufs_mbist_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_MBIST_SHIFT/* upd shift */),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD(CLK_TOP_AUD_1_SEL/* dts */, "aud_1_sel",
		aud_1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_2_SEL/* dts */, "aud_2_sel",
		aud_2_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_VENC_SEL/* dts */, "venc_sel",
		venc_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_VENC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_VDEC_SEL/* dts */, "vdec_sel",
		vdec_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_VDEC_SHIFT/* upd shift */),
	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD(CLK_TOP_PWM_SEL/* dts */, "pwm_sel",
		pwm_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PWM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUDIO_H_SEL/* dts */, "audio_h_sel",
		audio_h_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUDIO_H_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCUPM_SEL/* dts */, "mcupm_sel",
		mcupm_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_SEL/* dts */, "mem_sub_sel",
		mem_sub_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */),
	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_PERI_SEL/* dts */, "mem_sub_peri_sel",
		mem_sub_peri_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_PERI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_U_SEL/* dts */, "mem_sub_u_sel",
		mem_sub_u_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_UFS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EMI_N_SEL/* dts */, "emi_n_sel",
		emi_n_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DSI_OCC_SEL/* dts */, "dsi_occ_sel",
		dsi_occ_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DSI_OCC_SHIFT/* upd shift */),
	/* CLK_CFG_14 */
	MUX_CLR_SET_UPD(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "ap2conn_host_sel",
		ap2conn_host_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_IMG1_SEL/* dts */, "img1_sel",
		img1_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IMG1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_IPE_SEL/* dts */, "ipe_sel",
		ipe_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IPE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAM_SEL/* dts */, "cam_sel",
		cam_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_CAM_SHIFT/* upd shift */),
	/* CLK_CFG_15 */
	MUX_CLR_SET_UPD(CLK_TOP_CAMTM_SEL/* dts */, "camtm_sel",
		camtm_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_CAMTM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DSP_SEL/* dts */, "dsp_sel",
		dsp_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DSP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SR_PKA_SEL/* dts */, "sr_pka_sel",
		sr_pka_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SR_PKA_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DXCC_SEL/* dts */, "dxcc_sel",
		dxcc_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DXCC_SHIFT/* upd shift */),
	/* CLK_CFG_16 */
	MUX_CLR_SET_UPD(CLK_TOP_MFG_REF_SEL/* dts */, "mfg_ref_sel",
		mfg_ref_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MDP0_SEL/* dts */, "mdp0_sel",
		mdp0_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MDP0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DP_SEL/* dts */, "dp_sel",
		dp_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EDP_SEL/* dts */, "edp_sel",
		edp_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EDP_SHIFT/* upd shift */),
	/* CLK_CFG_17 */
	MUX_CLR_SET_UPD(CLK_TOP_EDP_FAVT_SEL/* dts */, "edp_favt_sel",
		edp_favt_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EDP_FAVT_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_ETH_250M_SEL/* dts */, "snps_eth_250m_sel",
		snps_eth_250m_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SNPS_ETH_250M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_ETH_62P4M_PTP_SEL/* dts */, "snps_eth_62p4m_ptp_sel",
		snps_eth_62p4m_ptp_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SNPS_ETH_62P4M_PTP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_ETH_50M_RMII_SEL/* dts */, "snps_eth_50m_rmii_sel",
		snps_eth_50m_rmii_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SNPS_ETH_50M_RMII_SHIFT/* upd shift */),
	/* CLK_CFG_18 */
	MUX_CLR_SET_UPD(CLK_TOP_SFLASH_SEL/* dts */, "sflash_sel",
		sflash_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SFLASH_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_GCPU_SEL/* dts */, "gcpu_sel",
		gcpu_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_GCPU_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MAC_TL_SEL/* dts */, "pcie_mac_tl_sel",
		pcie_mac_tl_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_PCIE_MAC_TL_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_VDSTX_DG_CTS_SEL/* dts */, "vdstx_dg_cts_sel",
		vdstx_dg_cts_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_VDSTX_CLKDIG_CTS_SHIFT/* upd shift */),
	/* CLK_CFG_19 */
	MUX_CLR_SET_UPD(CLK_TOP_PLL_DPIX_SEL/* dts */, "pll_dpix_sel",
		pll_dpix_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_PLL_DPIX_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_ECC_SEL/* dts */, "ecc_sel",
		ecc_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_ECC_SHIFT/* upd shift */),
	/* CLK_MISC_CFG_3 */
	MUX_CLR_SET(CLK_TOP_MFG_SEL_MFGPLL/* dts */, "mfg_sel_mfgpll",
		mfg_sel_mfgpll_parents/* parent */, CLK_MISC_CFG_3, CLK_MISC_CFG_3_SET,
		CLK_MISC_CFG_3_CLR/* set parent */, 16/* lsb */, 1/* width */),
#else
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_TOP_AXI_SEL/* dts */, "axi_sel",
		axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AXI_PERI_SEL/* dts */, "axi_peri_sel",
		axi_peri_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_PERI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AXI_U_SEL/* dts */, "axi_u_sel",
		axi_u_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_UFS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "bus_aximem_sel",
		bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */),
	/* CLK_CFG_1 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP0_SEL/* dts */, "disp0_sel",
		disp0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP0_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MMINFRA_SEL/* dts */, "mminfra_sel",
		mminfra_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UART_SEL/* dts */, "uart_sel",
		uart_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_UART_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI0_SEL/* dts */, "spi0_sel",
		spi0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI0_SHIFT/* upd shift */),
	/* CLK_CFG_2 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI1_SEL/* dts */, "spi1_sel",
		spi1_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI2_SEL/* dts */, "spi2_sel",
		spi2_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI3_SEL/* dts */, "spi3_sel",
		spi3_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI3_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI4_SEL/* dts */, "spi4_sel",
		spi4_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI4_SHIFT/* upd shift */),
	/* CLK_CFG_3 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI5_SEL/* dts */, "spi5_sel",
		spi5_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI5_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC_MACRO_0P_SEL/* dts */, "msdc_macro_0p_sel",
		msdc_macro_0p_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC_MACRO_0P_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL/* dts */, "msdc5hclk_sel",
		msdc5hclk_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC50_0_HCLK_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL/* dts */, "msdc50_0_sel",
		msdc50_0_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC50_0_SHIFT/* upd shift */),
	/* CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_MSDCFDE_SEL/* dts */, "aes_msdcfde_sel",
		aes_msdcfde_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AES_MSDCFDE_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC_MACRO_1P_SEL/* dts */, "msdc_macro_1p_sel",
		msdc_macro_1p_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL/* dts */, "msdc30_1_sel",
		msdc30_1_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_HCLK_SEL/* dts */, "msdc30_1_h_sel",
		msdc30_1_h_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_1_HCLK_SHIFT/* upd shift */),
	/* CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC_MACRO_2P_SEL/* dts */, "msdc_macro_2p_sel",
		msdc_macro_2p_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC_MACRO_2P_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_2_SEL/* dts */, "msdc30_2_sel",
		msdc30_2_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_2_HCLK_SEL/* dts */, "msdc30_2_h_sel",
		msdc30_2_h_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_2_HCLK_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL/* dts */, "aud_intbus_sel",
		aud_intbus_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD(CLK_TOP_ATB_SEL/* dts */, "atb_sel",
		atb_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_ATB_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL/* dts */, "disp_pwm_sel",
		disp_pwm_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP_PWM_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_P0_SEL/* dts */, "usb_p0_sel",
		usb_p0_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_USB_TOP_P0_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_XHCI_P0_SEL/* dts */, "ssusb_xhci_p0_sel",
		ssusb_xhci_p0_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_P0_SHIFT/* upd shift */),
	/* CLK_CFG_7 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_P1_SEL/* dts */, "usb_p1_sel",
		usb_p1_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_USB_TOP_P1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_XHCI_P1_SEL/* dts */, "ssusb_xhci_p1_sel",
		ssusb_xhci_p1_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_P1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_P2_SEL/* dts */, "usb_p2_sel",
		usb_p2_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_USB_TOP_P2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_XHCI_P2_SEL/* dts */, "ssusb_xhci_p2_sel",
		ssusb_xhci_p2_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_P2_SHIFT/* upd shift */),
	/* CLK_CFG_8 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_P3_SEL/* dts */, "usb_p3_sel",
		usb_p3_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USB_TOP_P3_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_XHCI_P3_SEL/* dts */, "ssusb_xhci_p3_sel",
		ssusb_xhci_p3_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_P3_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_P4_SEL/* dts */, "usb_p4_sel",
		usb_p4_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USB_TOP_P4_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_XHCI_P4_SEL/* dts */, "ssusb_xhci_p4_sel",
		ssusb_xhci_p4_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_P4_SHIFT/* upd shift */),
	/* CLK_CFG_9 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_I2C_SEL/* dts */, "i2c_sel",
		i2c_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF_SEL/* dts */, "seninf_sel",
		seninf_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF1_SEL/* dts */, "seninf1_sel",
		seninf1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "aud_engen1_sel",
		aud_engen1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */),
	/* CLK_CFG_10 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "aud_engen2_sel",
		aud_engen2_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_UFSFDE_SEL/* dts */, "aes_ufsfde_sel",
		aes_ufsfde_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_U_SEL/* dts */, "ufs_sel",
		ufs_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_U_MBIST_SEL/* dts */, "ufs_mbist_sel",
		ufs_mbist_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_MBIST_SHIFT/* upd shift */),
	/* CLK_CFG_11 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_1_SEL/* dts */, "aud_1_sel",
		aud_1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_2_SEL/* dts */, "aud_2_sel",
		aud_2_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_VENC_SEL/* dts */, "venc_sel",
		venc_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VENC_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_VDEC_SEL/* dts */, "vdec_sel",
		vdec_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VDEC_SHIFT/* upd shift */),
	/* CLK_CFG_12 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWM_SEL/* dts */, "pwm_sel",
		pwm_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PWM_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_H_SEL/* dts */, "audio_h_sel",
		audio_h_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUDIO_H_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCUPM_SEL/* dts */, "mcupm_sel",
		mcupm_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MCUPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_SEL/* dts */, "mem_sub_sel",
		mem_sub_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MEM_SUB_SHIFT/* upd shift */),
	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_PERI_SEL/* dts */, "mem_sub_peri_sel",
		mem_sub_peri_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MEM_SUB_PERI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_U_SEL/* dts */, "mem_sub_u_sel",
		mem_sub_u_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MEM_SUB_UFS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EMI_N_SEL/* dts */, "emi_n_sel",
		emi_n_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_EMI_N_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSI_OCC_SEL/* dts */, "dsi_occ_sel",
		dsi_occ_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DSI_OCC_SHIFT/* upd shift */),
	/* CLK_CFG_14 */
	MUX_CLR_SET_UPD(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "ap2conn_host_sel",
		ap2conn_host_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_IMG1_SEL/* dts */, "img1_sel",
		img1_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IMG1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_IPE_SEL/* dts */, "ipe_sel",
		ipe_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IPE_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAM_SEL/* dts */, "cam_sel",
		cam_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_CAM_SHIFT/* upd shift */),
	/* CLK_CFG_15 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTM_SEL/* dts */, "camtm_sel",
		camtm_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_CAMTM_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSP_SEL/* dts */, "dsp_sel",
		dsp_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DSP_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SR_PKA_SEL/* dts */, "sr_pka_sel",
		sr_pka_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SR_PKA_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DXCC_SEL/* dts */, "dxcc_sel",
		dxcc_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_DXCC_SHIFT/* upd shift */),
	/* CLK_CFG_16 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFG_REF_SEL/* dts */, "mfg_ref_sel",
		mfg_ref_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MFG_REF_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MDP0_SEL/* dts */, "mdp0_sel",
		mdp0_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MDP0_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DP_SEL/* dts */, "dp_sel",
		dp_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_DP_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_EDP_SEL/* dts */, "edp_sel",
		edp_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_EDP_SHIFT/* upd shift */),
	/* CLK_CFG_17 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_EDP_FAVT_SEL/* dts */, "edp_favt_sel",
		edp_favt_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_EDP_FAVT_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ETH_250M_SEL/* dts */, "snps_eth_250m_sel",
		snps_eth_250m_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SNPS_ETH_250M_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ETH_62P4M_PTP_SEL/* dts */, "snps_eth_62p4m_ptp_sel",
		snps_eth_62p4m_ptp_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SNPS_ETH_62P4M_PTP_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ETH_50M_RMII_SEL/* dts */, "snps_eth_50m_rmii_sel",
		snps_eth_50m_rmii_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SNPS_ETH_50M_RMII_SHIFT/* upd shift */),
	/* CLK_CFG_18 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SFLASH_SEL/* dts */, "sflash_sel",
		sflash_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SFLASH_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_GCPU_SEL/* dts */, "gcpu_sel",
		gcpu_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_GCPU_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MAC_TL_SEL/* dts */, "pcie_mac_tl_sel",
		pcie_mac_tl_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_PCIE_MAC_TL_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_VDSTX_DG_CTS_SEL/* dts */, "vdstx_dg_cts_sel",
		vdstx_dg_cts_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_VDSTX_CLKDIG_CTS_SHIFT/* upd shift */),
	/* CLK_CFG_19 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PLL_DPIX_SEL/* dts */, "pll_dpix_sel",
		pll_dpix_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_PLL_DPIX_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ECC_SEL/* dts */, "ecc_sel",
		ecc_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_ECC_SHIFT/* upd shift */),
	/* CLK_MISC_CFG_3 */
	MUX_CLR_SET(CLK_TOP_MFG_SEL_MFGPLL/* dts */, "mfg_sel_mfgpll",
		mfg_sel_mfgpll_parents/* parent */, CLK_MISC_CFG_3, CLK_MISC_CFG_3_SET,
		CLK_MISC_CFG_3_CLR/* set parent */, 16/* lsb */, 1/* width */),
#endif
};

static const struct mtk_composite top_composites[] = {
	/* CLK_AUDDIV_0 */
	MUX(CLK_TOP_APLL_I2SIN0_MCK_SEL/* dts */, "apll_i2sin0_m_sel",
		apll_i2sin0_m_parents/* parent */, 0x0320/* ofs */,
		16/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN1_MCK_SEL/* dts */, "apll_i2sin1_m_sel",
		apll_i2sin1_m_parents/* parent */, 0x0320/* ofs */,
		17/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN2_MCK_SEL/* dts */, "apll_i2sin2_m_sel",
		apll_i2sin2_m_parents/* parent */, 0x0320/* ofs */,
		18/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN3_MCK_SEL/* dts */, "apll_i2sin3_m_sel",
		apll_i2sin3_m_parents/* parent */, 0x0320/* ofs */,
		19/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN4_MCK_SEL/* dts */, "apll_i2sin4_m_sel",
		apll_i2sin4_m_parents/* parent */, 0x0320/* ofs */,
		20/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN6_MCK_SEL/* dts */, "apll_i2sin6_m_sel",
		apll_i2sin6_m_parents/* parent */, 0x0320/* ofs */,
		21/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT0_MCK_SEL/* dts */, "apll_i2sout0_m_sel",
		apll_i2sout0_m_parents/* parent */, 0x0320/* ofs */,
		22/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT1_MCK_SEL/* dts */, "apll_i2sout1_m_sel",
		apll_i2sout1_m_parents/* parent */, 0x0320/* ofs */,
		23/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT2_MCK_SEL/* dts */, "apll_i2sout2_m_sel",
		apll_i2sout2_m_parents/* parent */, 0x0320/* ofs */,
		24/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT3_MCK_SEL/* dts */, "apll_i2sout3_m_sel",
		apll_i2sout3_m_parents/* parent */, 0x0320/* ofs */,
		25/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT4_MCK_SEL/* dts */, "apll_i2sout4_m_sel",
		apll_i2sout4_m_parents/* parent */, 0x0320/* ofs */,
		26/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT6_MCK_SEL/* dts */, "apll_i2sout6_m_sel",
		apll_i2sout6_m_parents/* parent */, 0x0320/* ofs */,
		27/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_FMI2S_MCK_SEL/* dts */, "apll_fmi2s_m_sel",
		apll_fmi2s_m_parents/* parent */, 0x0320/* ofs */,
		28/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_TDMOUT_MCK_SEL/* dts */, "apll_tdmout_m_sel",
		apll_tdmout_m_parents/* parent */, 0x0320/* ofs */,
		29/* lsb */, 1/* width */),
	/* CLK_AUDDIV_2 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN0/* dts */, "apll12_div_i2sin0"/* ccf */,
		"apll_i2sin0_m_sel"/* parent */, 0x0320/* pdn ofs */,
		0/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN1/* dts */, "apll12_div_i2sin1"/* ccf */,
		"apll_i2sin1_m_sel"/* parent */, 0x0320/* pdn ofs */,
		1/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		8/* lsb */),
	/* CLK_AUDDIV_3 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT0/* dts */, "apll12_div_i2sout0"/* ccf */,
		"apll_i2sout0_m_sel"/* parent */, 0x0320/* pdn ofs */,
		6/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT1/* dts */, "apll12_div_i2sout1"/* ccf */,
		"apll_i2sout1_m_sel"/* parent */, 0x0320/* pdn ofs */,
		7/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_5 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_FMI2S/* dts */, "apll12_div_fmi2s"/* ccf */,
		"apll_fmi2s_m_sel"/* parent */, 0x0320/* pdn ofs */,
		12/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_TDMOUT_M/* dts */, "apll12_div_tdmout_m"/* ccf */,
		"apll_tdmout_m_sel"/* parent */, 0x0320/* pdn ofs */,
		13/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		8/* lsb */),
};

static const struct mtk_gate_regs top_cg_regs = {
	.set_ofs = 0x514,
	.clr_ofs = 0x518,
	.sta_ofs = 0x510,
};

#define GATE_TOP_FLAGS(_id, _name, _parent, _shift, _flag) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top_cg_regs,			\
		.shift = _shift,			\
		.flags = _flag,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_TOP(_id, _name, _parent, _shift)		\
	GATE_TOP_FLAGS(_id, _name, _parent, _shift, 0)

#define GATE_TOP_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate top_clks[] = {
	GATE_TOP_FLAGS(CLK_TOP_FMCNT_P0_EN, "fmcnt_p0_en",
			"univpll_192m_d4"/* parent */, 0, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_FMCNT_P1_EN, "fmcnt_p1_en",
			"univpll_192m_d4"/* parent */, 1, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_FMCNT_P2_EN, "fmcnt_p2_en",
			"univpll_192m_d4"/* parent */, 2, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_FMCNT_P3_EN, "fmcnt_p3_en",
			"univpll_192m_d4"/* parent */, 3, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_FMCNT_P4_EN, "fmcnt_p4_en",
			"univpll_192m_d4"/* parent */, 4, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_USB_F26M_CK_EN, "ssusb_f26m",
			"f26m_ck"/* parent */, 5, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_SSPXTP_F26M_CK_EN, "sspxtp_f26m",
			"f26m_ck"/* parent */, 6, CLK_IS_CRITICAL),
	GATE_TOP(CLK_TOP_USB2_PHY_RF_P0_EN, "usb2_phy_rf_p0_en",
			"f26m_ck"/* parent */, 7),
	GATE_TOP(CLK_TOP_USB2_PHY_RF_P1_EN, "usb2_phy_rf_p1_en",
			"f26m_ck"/* parent */, 10),
	GATE_TOP(CLK_TOP_USB2_PHY_RF_P2_EN, "usb2_phy_rf_p2_en",
			"f26m_ck"/* parent */, 11),
	GATE_TOP(CLK_TOP_USB2_PHY_RF_P3_EN, "usb2_phy_rf_p3_en",
			"f26m_ck"/* parent */, 12),
	GATE_TOP(CLK_TOP_USB2_PHY_RF_P4_EN, "usb2_phy_rf_p4_en",
			"f26m_ck"/* parent */, 13),
	GATE_TOP_FLAGS(CLK_TOP_USB2_26M_CK_P0_EN, "usb2_26m_p0_en",
			"f26m_ck"/* parent */, 14, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_USB2_26M_CK_P1_EN, "usb2_26m_p1_en",
			"f26m_ck"/* parent */, 15, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_USB2_26M_CK_P2_EN, "usb2_26m_p2_en",
			"f26m_ck"/* parent */, 18, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_USB2_26M_CK_P3_EN, "usb2_26m_p3_en",
			"f26m_ck"/* parent */, 19, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_USB2_26M_CK_P4_EN, "usb2_26m_p4_en",
			"f26m_ck"/* parent */, 20, CLK_IS_CRITICAL),
	GATE_TOP(CLK_TOP_F26M_CK_EN, "pcie_f26m",
			"f26m_ck"/* parent */, 21),
	GATE_TOP_FLAGS(CLK_TOP_AP2CON_EN, "ap2con",
			"f26m_ck"/* parent */, 24, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_EINT_N_EN, "eint_n",
			"f26m_ck"/* parent */, 25, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_TOPCKGEN_FMIPI_CSI_UP26M_CK_EN, "TOPCKGEN_fmipi_csi_up26m",
			"osc_d10"/* parent */, 26, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_EINT_E_EN, "eint_e",
			"f26m_ck"/* parent */, 28, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_EINT_W_EN, "eint_w",
			"f26m_ck"/* parent */, 30, CLK_IS_CRITICAL),
	GATE_TOP_FLAGS(CLK_TOP_EINT_S_EN, "eint_s",
			"f26m_ck"/* parent */, 31, CLK_IS_CRITICAL),
};

static const struct mtk_gate_regs vlpcfg_ao_reg_cg_regs = {
	.set_ofs = 0x800,
	.clr_ofs = 0x800,
	.sta_ofs = 0x800,
};

#define GATE_VLPCFG_AO_REG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vlpcfg_ao_reg_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_VLPCFG_AO_REG_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate vlpcfg_ao_reg_clks[] = {
	GATE_VLPCFG_AO_REG(EN, "en",
			"f26m_ck"/* parent */, 8),
};

static const struct mtk_gate_regs vlp_ck_cg_regs = {
	.set_ofs = 0x1F4,
	.clr_ofs = 0x1F8,
	.sta_ofs = 0x1F0,
};

#define GATE_VLP_CK_FLAGS(_id, _name, _parent, _shift, _flag) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vlp_ck_cg_regs,			\
		.shift = _shift,			\
		.flags = _flag,				\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VLP_CK_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_VLP_CK(_id, _name, _parent, _shift)	\
	GATE_VLP_CK_FLAGS(_id, _name, _parent, _shift, 0)

static const struct mtk_gate vlp_ck_clks[] = {
	GATE_VLP_CK(CLK_VLP_CK_VADSYS_VLP_26M_EN, "vlp_vadsys_vlp_26m",
			"f26m_ck"/* parent */, 1),
	GATE_VLP_CK_FLAGS(CLK_VLP_CK_FMIPI_CSI_UP26M_CK_EN, "VLP_fmipi_csi_up26m",
			"osc_d10"/* parent */, 11, CLK_IS_CRITICAL),
};

static const struct mtk_gate_regs vlpcfg_reg_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};

#define GATE_VLPCFG_REG_FLAGS(_id, _name, _parent, _shift, _flags) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vlpcfg_reg_cg_regs,			\
		.shift = _shift,			\
		.flags = _flags,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_VLPCFG_REG(_id, _name, _parent, _shift)		\
	GATE_VLPCFG_REG_FLAGS(_id, _name, _parent, _shift, 0)

static const struct mtk_gate vlpcfg_reg_clks[] = {
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_SCP, "vlpcfg_scp_ck",
			"vlp_scp_ck"/* parent */, 28, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_RG_R_APXGPT_26M, "vlpcfg_r_apxgpt_26m_ck",
			"vlp_sej_26m_ck"/* parent */, 24, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_DPMSRCK_TEST, "vlpcfg_dpmsrck_test_ck",
			"vlp_dpmsrck_ck"/* parent */, 23, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_RG_DPMSRRTC_TEST, "vlpcfg_dpmsrrtc_test_ck",
			"vlp_dpmsrrtc_ck"/* parent */, 22, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_DPMSRULP_TEST, "vlpcfg_dpmsrulp_test_ck",
			"vlp_dpmsrulp_ck"/* parent */, 21, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_SPMI_P_MST, "vlpcfg_spmi_p_ck",
			"vlp_spmi_p_ck"/* parent */, 20, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_SPMI_P_MST_32K, "vlpcfg_spmi_p_32k_ck",
			"vlp_spmi_p_32k_ck"/* parent */, 18, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_PMIF_SPMI_P_SYS, "vlpcfg_pmif_spmi_p_sys_ck",
			"vlp_pwrap_ulposc_ck"/* parent */, 13, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_PMIF_SPMI_P_TMR, "vlpcfg_pmif_spmi_p_tmr_ck",
			"vlp_pwrap_ulposc_ck"/* parent */, 12, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG(CLK_VLPCFG_REG_PMIF_SPMI_M_SYS, "vlpcfg_pmif_spmi_m_sys_ck",
			"vlp_pwrap_ulposc_ck"/* parent */, 11),
	GATE_VLPCFG_REG(CLK_VLPCFG_REG_PMIF_SPMI_M_TMR, "vlpcfg_pmif_spmi_m_tmr_ck",
			"vlp_pwrap_ulposc_ck"/* parent */, 10),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_DVFSRC, "vlpcfg_dvfsrc_ck",
			"vlp_dvfsrc_ck"/* parent */, 9, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_PWM_VLP, "vlpcfg_pwm_vlp_ck",
			"vlp_pwm_vlp_ck"/* parent */, 8, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_SRCK, "vlpcfg_srck_ck",
			"vlp_srck_ck"/* parent */, 7, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_SSPM_F26M, "vlpcfg_sspm_f26m_ck",
			"vlp_sspm_f26m_ck"/* parent */, 4, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_SSPM_F32K, "vlpcfg_sspm_f32k_ck",
			"vlp_sspm_f32k_ck"/* parent */, 3, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_SSPM_ULPOSC, "vlpcfg_sspm_ulposc_ck",
			"vlp_sspm_ulposc_ck"/* parent */, 2, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_VLP_32K_COM, "vlpcfg_vlp_32k_com_ck",
			"vlp_vlp_f32k_com_ck"/* parent */, 1, CLK_IS_CRITICAL),
	GATE_VLPCFG_REG_FLAGS(CLK_VLPCFG_REG_VLP_26M_COM, "vlpcfg_vlp_26m_com_ck",
			"vlp_vlp_f26m_com_ck"/* parent */, 0, CLK_IS_CRITICAL),
};

static const struct mtk_gate_regs dbgao_cg_regs = {
	.set_ofs = 0x70,
	.clr_ofs = 0x70,
	.sta_ofs = 0x70,
};

#define GATE_DBGAO(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dbgao_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_DBGAO_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate dbgao_clks[] = {
	GATE_DBGAO(CLK_DBGAO_ATB_EN, "dbgao_atb_en",
			"atb_ck"/* parent */, 0),
};

static const struct mtk_gate_regs dem0_cg_regs = {
	.set_ofs = 0x2C,
	.clr_ofs = 0x2C,
	.sta_ofs = 0x2C,
};

static const struct mtk_gate_regs dem1_cg_regs = {
	.set_ofs = 0x30,
	.clr_ofs = 0x30,
	.sta_ofs = 0x30,
};

static const struct mtk_gate_regs dem2_cg_regs = {
	.set_ofs = 0x70,
	.clr_ofs = 0x70,
	.sta_ofs = 0x70,
};

#define GATE_DEM0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dem0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_DEM0_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_DEM1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dem1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_DEM1_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

#define GATE_DEM2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dem2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_DEM2_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate dem_clks[] = {
	/* DEM0 */
	GATE_DEM0(CLK_DEM_BUSCLK_EN, "dem_busclk_en",
			"axi_ck"/* parent */, 0),
	/* DEM1 */
	GATE_DEM1(CLK_DEM_SYSCLK_EN, "dem_sysclk_en",
			"axi_ck"/* parent */, 0),
	/* DEM2 */
	GATE_DEM2(CLK_DEM_ATB_EN, "dem_atb_en",
			"atb_ck"/* parent */, 0),
};

static const struct mtk_gate_regs dvfsrc_top_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

#define GATE_DVFSRC_TOP_FLAGS(_id, _name, _parent, _shift, _flags) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dvfsrc_top_cg_regs,			\
		.shift = _shift,			\
		.flags = _flags,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_DVFSRC_TOP_V(_id, _name, _parent) {    \
		.id = _id,              \
		.name = _name,              \
		.parent_name = _parent,         \
	}

static const struct mtk_gate dvfsrc_top_clks[] = {
	GATE_DVFSRC_TOP_FLAGS(CLK_DVFSRC_TOP_DVFSRC_EN, "dvfsrc_dvfsrc_en",
			"f26m_ck"/* parent */, 0, CLK_IS_CRITICAL),
};

enum subsys_id {
	APMIXEDSYS = 0,
	PLL_SYS_NUM,
};

static const struct mtk_pll_data *plls_data[PLL_SYS_NUM];
static void __iomem *plls_base[PLL_SYS_NUM];

#define MT8189_PLL_FMAX		(3800UL * MHZ)
#define MT8189_PLL_FMIN		(1500UL * MHZ)
#define MT8189_INTEGER_BITS	8

#if MT_CCF_PLL_DISABLE
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

#define PLL_SETCLR(_id, _name, _reg, _pll_setclr, _en_setclr_bit,		\
			_rstb_setclr_bit, _flags, _pd_reg,		\
			_pd_shift, _tuner_reg, _tuner_en_reg,		\
			_tuner_en_bit, _pcw_reg, _pcw_shift,		\
			_pcwbits) {					\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pll_setclr = &(_pll_setclr),				\
		.flags = (_flags | PLL_CFLAGS),				\
		.fmax = MT8189_PLL_FMAX,				\
		.fmin = MT8189_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,			\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT8189_INTEGER_BITS,			\
	}

static struct mtk_pll_setclr_data setclr_data = {
	.en_ofs = 0x0070,
	.en_set_ofs = 0x0074,
	.en_clr_ofs = 0x0078,
	.rstb_ofs = 0x0080,
	.rstb_set_ofs = 0x0084,
	.rstb_clr_ofs = 0x0088,
};

static const struct mtk_pll_data apmixed_plls[] = {
	PLL_SETCLR(CLK_APMIXED_ARMPLL_LL, "armpll-ll", ARMPLL_LL_CON0, setclr_data/*base*/,
		18, 0, PLL_AO,
		ARMPLL_LL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_LL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_ARMPLL_BL, "armpll-bl", ARMPLL_BL_CON0, setclr_data/*base*/,
		17, 0, PLL_AO,
		ARMPLL_BL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_BL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_CCIPLL, "ccipll", CCIPLL_CON0, setclr_data/*base*/,
		16, 0, PLL_AO,
		CCIPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		CCIPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_MAINPLL, "mainpll", MAINPLL_CON0, setclr_data/*base*/,
		15, 2, HAVE_RST_BAR | PLL_AO,
		MAINPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MAINPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_UNIVPLL, "univpll", UNIVPLL_CON0, setclr_data/*base*/,
		14, 1, HAVE_RST_BAR,
		UNIVPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		UNIVPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_MMPLL, "mmpll", MMPLL_CON0, setclr_data/*base*/,
		13, 0, HAVE_RST_BAR,
		MMPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MMPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_MFGPLL, "mfgpll", MFGPLL_CON0, setclr_data/*base*/,
		7, 0, 0,
		MFGPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_APLL1, "apll1", APLL1_CON0, setclr_data/*base*/,
		11, 0, 0,
		APLL1_CON1, 24/*pd*/,
		APLL1_TUNER_CON0, AP_PLL_CON3, 0/*tuner*/,
		APLL1_CON2, 0, 32/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_APLL2, "apll2", APLL2_CON0, setclr_data/*base*/,
		10, 0, 0,
		APLL2_CON1, 24/*pd*/,
		APLL2_TUNER_CON0, AP_PLL_CON3, 1/*tuner*/,
		APLL2_CON2, 0, 32/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_EMIPLL, "emipll", EMIPLL_CON0, setclr_data/*base*/,
		12, 0, PLL_AO,
		EMIPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		EMIPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_APUPLL2, "apupll2", APUPLL2_CON0, setclr_data/*base*/,
		2, 0, 0,
		APUPLL2_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		APUPLL2_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_APUPLL, "apupll", APUPLL_CON0, setclr_data/*base*/,
		3, 0, 0,
		APUPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		APUPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_TVDPLL1, "tvdpll1", TVDPLL1_CON0, setclr_data/*base*/,
		9, 0, 0,
		TVDPLL1_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		TVDPLL1_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_TVDPLL2, "tvdpll2", TVDPLL2_CON0, setclr_data/*base*/,
		8, 0, 0,
		TVDPLL2_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		TVDPLL2_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_ETHPLL, "ethpll", ETHPLL_CON0, setclr_data/*base*/,
		6, 0, 0,
		ETHPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ETHPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_MSDCPLL, "msdcpll", MSDCPLL_CON0, setclr_data/*base*/,
		5, 0, 0,
		MSDCPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MSDCPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_UFSPLL, "ufspll", UFSPLL_CON0, setclr_data/*base*/,
		4, 0, 0,
		UFSPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		UFSPLL_CON1, 0, 22/*pcw*/),
};

static int clk_mt8189_pll_registration(enum subsys_id id,
		const struct mtk_pll_data *plls,
		struct platform_device *pdev,
		int num_plls)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (id >= PLL_SYS_NUM) {
		pr_notice("%s init invalid id(%d)\n", __func__, id);
		return 0;
	}

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_info("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(num_plls);

	mtk_clk_register_plls(node, plls, num_plls,
			clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);

	plls_data[id] = plls;
	plls_base[id] = base;

	return r;
}

static int clk_mt8189_apmixed_probe(struct platform_device *pdev)
{
	return clk_mt8189_pll_registration(APMIXEDSYS, apmixed_plls,
			pdev, ARRAY_SIZE(apmixed_plls));
}

static int clk_mt8189_dbgao_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_DBGAO_NR_CLK);

	mtk_clk_register_gates(&pdev->dev, node, dbgao_clks,
			ARRAY_SIZE(dbgao_clks), clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static int clk_mt8189_dem_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_DEM_NR_CLK);

	mtk_clk_register_gates(&pdev->dev, node, dem_clks,
			ARRAY_SIZE(dem_clks), clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static int clk_mt8189_dvfsrc_top_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_DVFSRC_TOP_NR_CLK);

	mtk_clk_register_gates(&pdev->dev, node, dvfsrc_top_clks,
			ARRAY_SIZE(dvfsrc_top_clks), clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static int clk_mt8189_top_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_info("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);

	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs),
			clk_data);

	mtk_clk_register_muxes(&pdev->dev, top_muxes, ARRAY_SIZE(top_muxes),
			node, &mt8189_clk_lock, clk_data);

	mtk_clk_register_gates(&pdev->dev, node, top_clks, ARRAY_SIZE(top_clks), clk_data);

	mtk_clk_register_composites(&pdev->dev, top_composites, ARRAY_SIZE(top_composites),
			base, &mt8189_clk_lock, clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static int clk_mt8189_vlpcfg_ao_reg_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_VLPCFG_AO_REG_NR_CLK);

	mtk_clk_register_gates(&pdev->dev, node, vlpcfg_ao_reg_clks,
			ARRAY_SIZE(vlpcfg_ao_reg_clks), clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static int clk_mt8189_vlp_ck_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_info("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_VLP_CK_NR_CLK);

	mtk_clk_register_factors(vlp_ck_divs, ARRAY_SIZE(vlp_ck_divs),
			clk_data);

	mtk_clk_register_muxes(&pdev->dev, vlp_ck_muxes, ARRAY_SIZE(vlp_ck_muxes),
			node, &mt8189_clk_lock, clk_data);

	mtk_clk_register_gates(&pdev->dev, node, vlp_ck_clks,
			ARRAY_SIZE(vlp_ck_clks), clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static int clk_mt8189_vlpcfg_reg_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_VLPCFG_REG_NR_CLK);

	mtk_clk_register_gates(&pdev->dev, node, vlpcfg_reg_clks,
			ARRAY_SIZE(vlpcfg_reg_clks), clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

/* for suspend LDVT only */
static void pll_force_off_internal(const struct mtk_pll_data *plls,
		void __iomem *base)
{
	void __iomem *rst_reg, *en_reg, *pwr_reg;

	for (; plls->name; plls++) {
		/* do not pwrdn the AO PLLs */
		if ((plls->flags & PLL_AO) == PLL_AO)
			continue;

		if ((plls->flags & HAVE_RST_BAR) == HAVE_RST_BAR) {
			rst_reg = base + plls->en_reg;
			writel(readl(rst_reg) & ~plls->rst_bar_mask,
				rst_reg);
		}

		en_reg = base + plls->en_reg;

		pwr_reg = base + plls->pwr_reg;

		writel(readl(en_reg) & ~plls->en_mask,
				en_reg);
		writel(readl(pwr_reg) | (0x2),
				pwr_reg);
		writel(readl(pwr_reg) & ~(0x1),
				pwr_reg);
	}
}

void mt8189_pll_force_off(void)
{
	int i;

	for (i = 0; i < PLL_SYS_NUM; i++)
		pll_force_off_internal(plls_data[i], plls_base[i]);
}
EXPORT_SYMBOL_GPL(mt8189_pll_force_off);

static const struct of_device_id of_match_clk_mt8189[] = {
	{
		.compatible = "mediatek,mt8189-apmixedsys",
		.data = clk_mt8189_apmixed_probe,
	}, {
		.compatible = "mediatek,mt8189-dbg_ao",
		.data = clk_mt8189_dbgao_probe,
	}, {
		.compatible = "mediatek,mt8189-dem",
		.data = clk_mt8189_dem_probe,
	}, {
		.compatible = "mediatek,mt8189-dvfsrc_top",
		.data = clk_mt8189_dvfsrc_top_probe,
	}, {
		.compatible = "mediatek,mt8189-topckgen",
		.data = clk_mt8189_top_probe,
	}, {
		.compatible = "mediatek,mt8189-vlp_ao_ckgen",
		.data = clk_mt8189_vlpcfg_ao_reg_probe,
	}, {
		.compatible = "mediatek,mt8189-vlpcfg_reg_bus",
		.data = clk_mt8189_vlpcfg_reg_probe,
	}, {
		.compatible = "mediatek,mt8189-vlp_ckgen",
		.data = clk_mt8189_vlp_ck_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt8189_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pd);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_info(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt8189_drv = {
	.probe = clk_mt8189_probe,
	.driver = {
		.name = "clk-mt8189",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt8189,
	},
};

module_platform_driver(clk_mt8189_drv);
MODULE_LICENSE("GPL");

