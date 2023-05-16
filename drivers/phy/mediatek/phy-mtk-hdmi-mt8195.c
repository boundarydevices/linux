// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Copyright (c) 2021 BayLibre, SAS
 */
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/nvmem-consumer.h>

#include <drm/drm_print.h>

#include "phy-mtk-hdmi.h"
#include "phy-mtk-hdmi-mt8195.h"

static void mtk_hdmi_ana_fifo_en(struct mtk_hdmi_phy *hdmi_phy)
{
	/* make data fifo writable for hdmi2.0 */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_ANA_CTL, REG_ANA_HDMI20_FIFO_EN,
			  REG_ANA_HDMI20_FIFO_EN);
}

static void
mtk_mt8195_phy_tmds_high_bit_clk_ratio(struct mtk_hdmi_phy *hdmi_phy,
				       bool enable)
{
	dev_info(hdmi_phy->dev, "tmds_high_bit_clk_ratio enabled:%d\n", enable);

	mtk_hdmi_ana_fifo_en(hdmi_phy);

	/* HDMI 2.0 specification, 3.4Gbps <= TMDS Bit Rate <= 6G,
	 * clock bit ratio 1:40, under 3.4Gbps, clock bit ratio 1:10
	 */
	if (enable)
		mtk_hdmi_phy_mask(hdmi_phy, HDMI20_CLK_CFG,
				  0x2 << REG_TXC_DIV_SHIFT, REG_TXC_DIV);
	else
		mtk_hdmi_phy_mask(hdmi_phy, HDMI20_CLK_CFG, 0, REG_TXC_DIV);
}

static void mtk_hdmi_pll_select_source(struct clk_hw *hw)
{
	struct mtk_hdmi_phy *hdmi_phy = to_mtk_hdmi_phy(hw);

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_CTL_3, 0x0, REG_HDMITX_REF_XTAL_SEL);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_CTL_3, 0x0, REG_HDMITX_REF_RESPLL_SEL);

	/* DA_HDMITX21_REF_CK for TXPLL input source */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_10, 0x0,
			  RG_HDMITXPLL_REF_CK_SEL);
}

static int mtk_hdmi_pll_performance_setting(struct clk_hw *hw)
{
	struct mtk_hdmi_phy *hdmi_phy = to_mtk_hdmi_phy(hw);

	/* BP2 */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_0,
			  0x1 << RG_HDMITXPLL_BP2_SHIFT, RG_HDMITXPLL_BP2);

	/* BC */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_2,
			  0x3 << RG_HDMITXPLL_BC_SHIFT, RG_HDMITXPLL_BC);

	/* IC */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_2,
			  0x1 << RG_HDMITXPLL_IC_SHIFT, RG_HDMITXPLL_IC);

	/* BR */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_2,
			  0x2 << RG_HDMITXPLL_BR_SHIFT, RG_HDMITXPLL_BR);

	/* IR */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_2,
			  0x2 << RG_HDMITXPLL_IR_SHIFT, RG_HDMITXPLL_IR);

	/* BP */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_2,
			  0xf << RG_HDMITXPLL_BP_SHIFT, RG_HDMITXPLL_BP);

	/* IBAND_FIX_EN, RESERVE[14] */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_0,
			  0x0 << RG_HDMITXPLL_IBAND_FIX_EN_SHIFT,
			  RG_HDMITXPLL_IBAND_FIX_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_1,
			  0x0 << RG_HDMITXPLL_RESERVE_BIT14_SHIFT,
			  RG_HDMITXPLL_RESERVE_BIT14);

	/* HIKVCO */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_2,
			  0x0 << RG_HDMITXPLL_HIKVCO_SHIFT,
			  RG_HDMITXPLL_HIKVCO);

	/* HREN */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_0,
			  0x1 << RG_HDMITXPLL_HREN_SHIFT, RG_HDMITXPLL_HREN);

	/* LVR_SEL */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_0,
			  0x1 << RG_HDMITXPLL_LVR_SEL_SHIFT,
			  RG_HDMITXPLL_LVR_SEL);

	/* RG_HDMITXPLL_RESERVE[12:11] */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_1,
			  0x3 << RG_HDMITXPLL_RESERVE_BIT12_11_SHIFT,
			  RG_HDMITXPLL_RESERVE_BIT12_11);

	/* TCL_EN */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_0,
			  0x1 << RG_HDMITXPLL_TCL_EN_SHIFT,
			  RG_HDMITXPLL_TCL_EN);

	/* we should always read calibration impedance 
	 * from efuse, unless for debugging purposes. 
	 * This calibraion value is not board-dependent
	 * so no SW adjustment required.
	 */
	if (hdmi_phy->conf->efuse_sw_mode) {
		dev_info(hdmi_phy->dev, "efuse_sw_mode ENABLED!!!");
		mtk_hdmi_phy_mask(hdmi_phy, HDMI_CTL_1,
				0x1f << RG_INTR_IMP_RG_MODE_SHIFT,
				RG_INTR_IMP_RG_MODE);
	}

	return 0;
}

static int mtk_hdmi_pll_set_hw(struct clk_hw *hw, unsigned char prediv,
			       unsigned char fbkdiv_high,
			       unsigned long fbkdiv_low,
			       unsigned char fbkdiv_hs3, unsigned char posdiv1,
			       unsigned char posdiv2, unsigned char txprediv,
			       unsigned char txposdiv,
			       unsigned char digital_div)
{
	unsigned char txposdiv_value = 0;
	unsigned char div3_ctrl_value = 0;
	unsigned char posdiv_vallue = 0;
	unsigned char div_ctrl_value = 0;
	unsigned char reserve_3_2_value = 0;
	unsigned char prediv_value = 0;
	unsigned char reserve13_value = 0;
	struct mtk_hdmi_phy *hdmi_phy = to_mtk_hdmi_phy(hw);

	mtk_hdmi_pll_select_source(hw);

	mtk_hdmi_pll_performance_setting(hw);

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_10,
			  0x2 << RG_HDMITX21_BIAS_PE_BG_VREF_SEL_SHIFT,
			  RG_HDMITX21_BIAS_PE_BG_VREF_SEL);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_10,
			  0x0 << RG_HDMITX21_VREF_SEL_SHIFT,
			  RG_HDMITX21_VREF_SEL);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_9,
			  0x2 << RG_HDMITX21_SLDO_VREF_SEL_SHIFT,
			  RG_HDMITX21_SLDO_VREF_SEL);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_10,
			  0x0 << RG_HDMITX21_BIAS_PE_VREF_SELB_SHIFT,
			  RG_HDMITX21_BIAS_PE_VREF_SELB);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_3,
			  0x1 << RG_HDMITX21_SLDOLPF_EN_SHIFT,
			  RG_HDMITX21_SLDOLPF_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x11 << RG_HDMITX21_INTR_CAL_SHIFT,
			  RG_HDMITX21_INTR_CAL);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_2,
			  0x1 << RG_HDMITXPLL_PWD_SHIFT, RG_HDMITXPLL_PWD);

	/* TXPOSDIV */
	if (txposdiv == 1)
		txposdiv_value = 0x0;
	else if (txposdiv == 2)
		txposdiv_value = 0x1;
	else if (txposdiv == 4)
		txposdiv_value = 0x2;
	else if (txposdiv == 8)
		txposdiv_value = 0x3;
	else
		return -EINVAL;

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  txposdiv_value << RG_HDMITX21_TX_POSDIV_SHIFT,
			  RG_HDMITX21_TX_POSDIV);

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x1 << RG_HDMITX21_TX_POSDIV_EN_SHIFT,
			  RG_HDMITX21_TX_POSDIV_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x0 << RG_HDMITX21_FRL_EN_SHIFT, RG_HDMITX21_FRL_EN);

	/* TXPREDIV */
	if (txprediv == 2) {
		div3_ctrl_value = 0x0;
		posdiv_vallue = 0x0;
	} else if (txprediv == 4) {
		div3_ctrl_value = 0x0;
		posdiv_vallue = 0x1;
	} else if (txprediv == 6) {
		div3_ctrl_value = 0x1;
		posdiv_vallue = 0x0;
	} else if (txprediv == 12) {
		div3_ctrl_value = 0x1;
		posdiv_vallue = 0x1;
	} else {
		return -EINVAL;
	}

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_4,
			  div3_ctrl_value
				  << RG_HDMITXPLL_POSDIV_DIV3_CTRL_SHIFT,
			  RG_HDMITXPLL_POSDIV_DIV3_CTRL);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_4,
			  posdiv_vallue << RG_HDMITXPLL_POSDIV_SHIFT,
			  RG_HDMITXPLL_POSDIV);

	/* POSDIV1 */
	if (posdiv1 == 5)
		div_ctrl_value = 0x0;
	else if (posdiv1 == 10)
		div_ctrl_value = 0x1;
	else if (posdiv1 == (125 / 10))
		div_ctrl_value = 0x2;
	else if (posdiv1 == 15)
		div_ctrl_value = 0x3;
	else
		return -EINVAL;

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_4,
			  div_ctrl_value << RG_HDMITXPLL_DIV_CTRL_SHIFT,
			  RG_HDMITXPLL_DIV_CTRL);

	/* DE add new setting */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_1,
			  0x0 << RG_HDMITXPLL_RESERVE_BIT14_SHIFT,
			  RG_HDMITXPLL_RESERVE_BIT14);

	/* POSDIV2 */
	if (posdiv2 == 1)
		reserve_3_2_value = 0x0;
	else if (posdiv2 == 2)
		reserve_3_2_value = 0x1;
	else if (posdiv2 == 4)
		reserve_3_2_value = 0x2;
	else if (posdiv2 == 6)
		reserve_3_2_value = 0x3;
	else
		return -EINVAL;

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_1,
			  reserve_3_2_value
				  << RG_HDMITXPLL_RESERVE_BIT3_2_SHIFT,
			  RG_HDMITXPLL_RESERVE_BIT3_2);

	/* DE add new setting */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_1,
			  0x2 << RG_HDMITXPLL_RESERVE_BIT1_0_SHIFT,
			  RG_HDMITXPLL_RESERVE_BIT1_0);

	/* PREDIV */
	if (prediv == 1)
		prediv_value = 0x0;
	else if (prediv == 2)
		prediv_value = 0x1;
	else if (prediv == 4)
		prediv_value = 0x2;
	else
		return -EINVAL;

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_4,
			  prediv_value << RG_HDMITXPLL_PREDIV_SHIFT,
			  RG_HDMITXPLL_PREDIV);

	/* FBKDIV_HS3 */
	if (fbkdiv_hs3 == 1)
		reserve13_value = 0x0;
	else if (fbkdiv_hs3 == 2)
		reserve13_value = 0x1;
	else
		return -EINVAL;

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_1,
			  reserve13_value << RG_HDMITXPLL_RESERVE_BIT13_SHIFT,
			  RG_HDMITXPLL_RESERVE_BIT13);

	/* FBDIV */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_4,
			  fbkdiv_high << RG_HDMITXPLL_FBKDIV_high_SHIFT,
			  RG_HDMITXPLL_FBKDIV_HIGH);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_3,
			  fbkdiv_low << RG_HDMITXPLL_FBKDIV_low_SHIFT,
			  RG_HDMITXPLL_FBKDIV_low);

	/* Digital DIVIDER */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_CTL_3,
			  0x0 << REG_PIXEL_CLOCK_SEL_SHIFT,
			  REG_PIXEL_CLOCK_SEL);

	if (digital_div == 1) {
		mtk_hdmi_phy_mask(hdmi_phy, HDMI_CTL_3,
				  0x0 << REG_HDMITX_PIXEL_CLOCK_SHIFT,
				  REG_HDMITX_PIXEL_CLOCK);
	} else {
		mtk_hdmi_phy_mask(hdmi_phy, HDMI_CTL_3,
				  0x1 << REG_HDMITX_PIXEL_CLOCK_SHIFT,
				  REG_HDMITX_PIXEL_CLOCK);
		mtk_hdmi_phy_mask(hdmi_phy, HDMI_CTL_3,
				  (digital_div - 1) << REG_HDMITXPLL_DIV_SHIFT,
				  REG_HDMITXPLL_DIV);
	}

	return 0;
}

#define PCW_DECIMAL_WIDTH 24

static int mtk_hdmi_pll_calculate_params(struct clk_hw *hw, unsigned long rate,
					 unsigned long parent_rate)
{
	int ret;
	unsigned long long tmds_clk = 0;
	unsigned long long pixel_clk = 0;
	//pll input source frequency
	unsigned long long da_hdmitx21_ref_ck = 0;
	unsigned long long ns_hdmipll_ck = 0; //ICO output clk
	//source clk for Display digital
	unsigned long long ad_hdmipll_pixel_ck = 0;
	unsigned char digital_div = 0;
	unsigned long long pcw = 0; //FBDIV
	unsigned char txprediv = 0;
	unsigned char txposdiv = 0;
	unsigned char fbkdiv_high = 0;
	unsigned long fbkdiv_low = 0;
	unsigned char posdiv1 = 0;
	unsigned char posdiv2 = 0;
	unsigned char prediv = 1; //prediv is always 1
	unsigned char fbkdiv_hs3 = 1; //fbkdiv_hs3 is always 1
	int i = 0;
	unsigned char txpredivs[4] = { 2, 4, 6, 12 };

	struct mtk_hdmi_phy *hdmi_phy = to_mtk_hdmi_phy(hw);

	pixel_clk = rate;
	tmds_clk = pixel_clk;

	if ((tmds_clk < 25000000) || (tmds_clk > 594000000))
		return -EINVAL;

	if (tmds_clk >= 340000000)
		hdmi_phy->is_over_340M = true;
	else
		hdmi_phy->is_over_340M = false;

	da_hdmitx21_ref_ck = 26000000UL; //in HZ

	/*  TXPOSDIV stage treatment:
	 *	0M  <  TMDS clk  < 54M		  /8
	 *	54M <= TMDS clk  < 148.35M    /4
	 *	148.35M <=TMDS clk < 296.7M   /2
	 *	296.7 <=TMDS clk <= 594M	  /1
	 */
	if (tmds_clk < 54000000UL)
		txposdiv = 8;
	else if (tmds_clk >= 54000000UL && tmds_clk < 148350000UL)
		txposdiv = 4;
	else if (tmds_clk >= 148350000UL && tmds_clk < 296700000UL)
		txposdiv = 2;
	else if (tmds_clk >= 296700000UL && tmds_clk <= 594000000UL)
		txposdiv = 1;
	else
		return -EINVAL;

	/* calculate txprediv: can be 2, 4, 6, 12
	 * ICO clk = 5*TMDS_CLK*TXPOSDIV*TXPREDIV
	 * ICO clk constraint: 5G =< ICO clk <= 12G
	 */
	for (i = 0; i < ARRAY_SIZE(txpredivs); i++) {
		ns_hdmipll_ck = 5 * tmds_clk * txposdiv * txpredivs[i];
		if (ns_hdmipll_ck >= 5000000000UL &&
		    ns_hdmipll_ck <= 12000000000UL)
			break;
	}
	if (i == (ARRAY_SIZE(txpredivs) - 1) &&
	    (ns_hdmipll_ck < 5000000000UL || ns_hdmipll_ck > 12000000000UL)) {
		return -EINVAL;
	}
	if (i == ARRAY_SIZE(txpredivs))
		return -EINVAL;

	txprediv = txpredivs[i];

	/* PCW calculation: FBKDIV
	 * formula: pcw=(frequency_out*2^pcw_bit) / frequency_in / FBKDIV_HS3;
	 * RG_HDMITXPLL_FBKDIV[32:0]:
	 * [32,24] 9bit integer, [23,0]:24bit fraction
	 */
	pcw = ns_hdmipll_ck;
	pcw = pcw << PCW_DECIMAL_WIDTH;
	pcw = pcw / da_hdmitx21_ref_ck;
	pcw = pcw / fbkdiv_hs3;

	if ((pcw / BIT(32)) > 1) {
		return -EINVAL;
	} else if ((pcw / BIT(32)) == 1) {
		fbkdiv_high = 1;
		fbkdiv_low = pcw % BIT(32);
	} else {
		fbkdiv_high = 0;
		fbkdiv_low = pcw;
	}

	/* posdiv1:
	 * posdiv1 stage treatment according to color_depth:
	 * 24bit -> posdiv1 /10, 30bit -> posdiv1 /12.5,
	 * 36bit -> posdiv1 /15, 48bit -> posdiv1 /10
	 */
	posdiv1 = 10; // div 10
	posdiv2 = 1;
	ad_hdmipll_pixel_ck = (ns_hdmipll_ck / 10) / 1;

	/* Digital clk divider, max /32 */
	digital_div = ad_hdmipll_pixel_ck / pixel_clk;
	if (!(digital_div <= 32 && digital_div >= 1))
		return -EINVAL;

	ret = mtk_hdmi_pll_set_hw(hw, prediv, fbkdiv_high, fbkdiv_low,
				  fbkdiv_hs3, posdiv1, posdiv2, txprediv,
				  txposdiv, digital_div);
	if (ret)
		return -EINVAL;

	return 0;
}

static int mtk_hdmi_pll_drv_setting(struct clk_hw *hw)
{
	unsigned char data_channel_bias, clk_channel_bias;
	unsigned char impedance, impedance_en;
	unsigned char calibration;
	struct mtk_hdmi_phy *hdmi_phy = to_mtk_hdmi_phy(hw);
	unsigned long tmds_clk;
	unsigned long pixel_clk = hdmi_phy->pll_rate;

	/* TODO: TMDS clk frequency equals to pixel_clk
	 * only when using 8-bit RGB or YUV444.
	 * We need to adjust tmds_clk frequency
	 * according to color space and color depth.
	 */
	tmds_clk = pixel_clk;

	/* bias & impedance setting:
	 * 3G < data rate <= 6G: enable impedance 100ohm,
	 *      data channel bias 24mA, clock channel bias 20mA
	 * pixel clk >= HD,  74.175MHZ <= pixel clk <= 300MHZ:
	 *      enalbe impedance 100ohm
	 *      data channel 20mA, clock channel 16mA
	 * 27M =< pixel clk < 74.175: disable impedance
	 *      data channel & clock channel bias 10mA
	 */

	/* 3G < data rate <= 6G, 300M < tmds rate <= 594M */
	if (tmds_clk > 300000000UL && tmds_clk <= 594000000UL) {
		data_channel_bias = 0x3c; //24mA
		clk_channel_bias = 0x34; //20mA
		impedance_en = 0xf;
		impedance = 0x36; //100ohm
	} else if (pixel_clk >= 74175000UL && pixel_clk <= 300000000UL) {
		data_channel_bias = 0x34; //20mA
		clk_channel_bias = 0x2c; //16mA
		impedance_en = 0xf;
		impedance = 0x36; //100ohm
	} else if (pixel_clk >= 27000000UL && pixel_clk < 74175000UL) {
		data_channel_bias = 0x14; //10mA
		clk_channel_bias = 0x14; //10mA
		impedance_en = 0x0;
		impedance = 0x0;
	} else {

		return -EINVAL;
	}

	/* bias */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_1,
			  data_channel_bias << RG_HDMITX21_DRV_IBIAS_D0_SHIFT,
			  RG_HDMITX21_DRV_IBIAS_D0);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_1,
			  data_channel_bias << RG_HDMITX21_DRV_IBIAS_D1_SHIFT,
			  RG_HDMITX21_DRV_IBIAS_D1);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_1,
			  data_channel_bias << RG_HDMITX21_DRV_IBIAS_D2_SHIFT,
			  RG_HDMITX21_DRV_IBIAS_D2);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_0,
			  clk_channel_bias << RG_HDMITX21_DRV_IBIAS_CLK_SHIFT,
			  RG_HDMITX21_DRV_IBIAS_CLK);

	/* impedance */
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_0,
			  impedance_en << RG_HDMITX21_DRV_IMP_EN_SHIFT,
			  RG_HDMITX21_DRV_IMP_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_2,
			  impedance << RG_HDMITX21_DRV_IMP_D0_EN1_SHIFT,
			  RG_HDMITX21_DRV_IMP_D0_EN1);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_2,
			  impedance << RG_HDMITX21_DRV_IMP_D1_EN1_SHIFT,
			  RG_HDMITX21_DRV_IMP_D1_EN1);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_2,
			  impedance << RG_HDMITX21_DRV_IMP_D2_EN1_SHIFT,
			  RG_HDMITX21_DRV_IMP_D2_EN1);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_2,
			  impedance << RG_HDMITX21_DRV_IMP_CLK_EN1_SHIFT,
			  RG_HDMITX21_DRV_IMP_CLK_EN1);

	/* calibration */
	if (hdmi_phy->conf->efuse_sw_mode) {
		/* default SW calibration setting suggested */
		calibration = 0x11;
		mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
				calibration << RG_HDMITX21_INTR_CAL_SHIFT,
				RG_HDMITX21_INTR_CAL);
	}

	return 0;
}

static int mtk_hdmi_pll_prepare(struct clk_hw *hw)
{
	struct mtk_hdmi_phy *hdmi_phy = to_mtk_hdmi_phy(hw);

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x1 << RG_HDMITX21_TX_POSDIV_EN_SHIFT,
			  RG_HDMITX21_TX_POSDIV_EN);

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_0,
			  0xf << RG_HDMITX21_SER_EN_SHIFT, RG_HDMITX21_SER_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x1 << RG_HDMITX21_D0_DRV_OP_EN_SHIFT,
			  RG_HDMITX21_D0_DRV_OP_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x1 << RG_HDMITX21_D1_DRV_OP_EN_SHIFT,
			  RG_HDMITX21_D1_DRV_OP_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x1 << RG_HDMITX21_D2_DRV_OP_EN_SHIFT,
			  RG_HDMITX21_D2_DRV_OP_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x1 << RG_HDMITX21_CK_DRV_OP_EN_SHIFT,
			  RG_HDMITX21_CK_DRV_OP_EN);

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x0 << RG_HDMITX21_FRL_D0_EN_SHIFT,
			  RG_HDMITX21_FRL_D0_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x0 << RG_HDMITX21_FRL_D1_EN_SHIFT,
			  RG_HDMITX21_FRL_D1_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x0 << RG_HDMITX21_FRL_D2_EN_SHIFT,
			  RG_HDMITX21_FRL_D2_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x0 << RG_HDMITX21_FRL_CK_EN_SHIFT,
			  RG_HDMITX21_FRL_CK_EN);

	mtk_hdmi_pll_drv_setting(hw);

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_10,
			  0x0 << RG_HDMITX21_BG_PWD_SHIFT, RG_HDMITX21_BG_PWD);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x1 << RG_HDMITX21_BIAS_EN_SHIFT,
			  RG_HDMITX21_BIAS_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_3,
			  0x1 << RG_HDMITX21_CKLDO_EN_SHIFT,
			  RG_HDMITX21_CKLDO_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_3,
			  0xf << RG_HDMITX21_SLDO_EN_SHIFT,
			  RG_HDMITX21_SLDO_EN);

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_4,
			  0x1 << DA_HDMITXPLL_PWR_ON_SHIFT,
			  DA_HDMITXPLL_PWR_ON);
	usleep_range(5, 10);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_4,
			  0x0 << DA_HDMITXPLL_ISO_EN_SHIFT,
			  DA_HDMITXPLL_ISO_EN);
	usleep_range(5, 10);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_2,
			  0x0 << RG_HDMITXPLL_PWD_SHIFT, RG_HDMITXPLL_PWD);
	usleep_range(30, 50);
	return 0;
}

static void mtk_hdmi_pll_unprepare(struct clk_hw *hw)
{
	struct mtk_hdmi_phy *hdmi_phy = to_mtk_hdmi_phy(hw);

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_10,
			  0x1 << RG_HDMITX21_BG_PWD_SHIFT, RG_HDMITX21_BG_PWD);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_6,
			  0x0 << RG_HDMITX21_BIAS_EN_SHIFT,
			  RG_HDMITX21_BIAS_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_3,
			  0x0 << RG_HDMITX21_CKLDO_EN_SHIFT,
			  RG_HDMITX21_CKLDO_EN);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_3,
			  0x0 << RG_HDMITX21_SLDO_EN_SHIFT,
			  RG_HDMITX21_SLDO_EN);

	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_2,
			  0x1 << RG_HDMITXPLL_PWD_SHIFT, RG_HDMITXPLL_PWD);
	usleep_range(10, 20);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_4,
			  0x1 << DA_HDMITXPLL_ISO_EN_SHIFT,
			  DA_HDMITXPLL_ISO_EN);
	usleep_range(10, 20);
	mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_PLL_CFG_4,
			  0x0 << DA_HDMITXPLL_PWR_ON_SHIFT,
			  DA_HDMITXPLL_PWR_ON);
}

static int mtk_hdmi_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				 unsigned long parent_rate)
{
	struct mtk_hdmi_phy *hdmi_phy = to_mtk_hdmi_phy(hw);
	int ret;

	dev_dbg(hdmi_phy->dev, "%s: %lu Hz, parent: %lu Hz\n", __func__, rate,
		parent_rate);

	ret = mtk_hdmi_pll_calculate_params(hw, rate, parent_rate);
	if (ret != 0)
		return -EINVAL;

	return 0;
}

static long mtk_hdmi_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long *parent_rate)
{
	struct mtk_hdmi_phy *hdmi_phy = to_mtk_hdmi_phy(hw);

	hdmi_phy->pll_rate = rate;
	return rate;
}

static unsigned long mtk_hdmi_pll_recalc_rate(struct clk_hw *hw,
					      unsigned long parent_rate)
{
	struct mtk_hdmi_phy *hdmi_phy = to_mtk_hdmi_phy(hw);

	return hdmi_phy->pll_rate;
}

static const struct clk_ops mtk_hdmi_pll_ops = {
	.prepare = mtk_hdmi_pll_prepare,
	.unprepare = mtk_hdmi_pll_unprepare,
	.set_rate = mtk_hdmi_pll_set_rate,
	.round_rate = mtk_hdmi_pll_round_rate,
	.recalc_rate = mtk_hdmi_pll_recalc_rate,
};

static void vtx_signal_en(struct mtk_hdmi_phy *hdmi_phy, bool on)
{
	if (on) {
		DRM_DEV_DEBUG_DRIVER(hdmi_phy->dev, "HDMI Tx TMDS ON\n");
		mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_0, RG_HDMITX21_DRV_EN,
				  RG_HDMITX21_DRV_EN);
	} else {
		DRM_DEV_DEBUG_DRIVER(hdmi_phy->dev, "HDMI Tx TMDS OFF\n");
		mtk_hdmi_phy_mask(hdmi_phy, HDMI_1_CFG_0,
				  0x0 << RG_HDMITX21_DRV_EN_SHIFT,
				  RG_HDMITX21_DRV_EN);
	}
}

static void mtk_hdmi_phy_enable_tmds(struct mtk_hdmi_phy *hdmi_phy)
{
	vtx_signal_en(hdmi_phy, true);
	usleep_range(100, 150);
}

static void mtk_hdmi_phy_disable_tmds(struct mtk_hdmi_phy *hdmi_phy)
{
	vtx_signal_en(hdmi_phy, false);
}

static int mtk_hdmi_phy_configure(struct phy *phy, union phy_configure_opts *opts)
{
	struct phy_configure_opts_dp *dp_opts = &opts->dp;
	struct mtk_hdmi_phy *hdmi_phy = phy_get_drvdata(phy);
	int ret = 0;
	bool enable = hdmi_phy->is_over_340M;

	ret = clk_set_rate(hdmi_phy->pll, dp_opts->link_rate);

	if (ret)
		goto out;

	mtk_mt8195_phy_tmds_high_bit_clk_ratio(hdmi_phy, enable);

out:
	return ret;
}

static void mtk_hdmi_power_control(struct mtk_hdmi_phy *hdmi_phy, bool enable)
{
	dev_dbg(hdmi_phy->dev, "%s: enable = %d\n", __func__, enable);

	if (enable)
		mtk_hdmi_phy_mask(hdmi_phy, HDMI_CTL_1,
			0x1 << RG_HDMITX_PWR5V_O_SHIFT, RG_HDMITX_PWR5V_O);
	else
		mtk_hdmi_phy_mask(hdmi_phy, HDMI_CTL_1,
			0x0 << RG_HDMITX_PWR5V_O_SHIFT, RG_HDMITX_PWR5V_O);
}

struct mtk_hdmi_phy_conf mtk_hdmi_phy_8195_conf = {
	/* TODO: CLK_SET_RATE_GATE causes hdmi_txpll clock
	 * set_rate() fails. We need to adjust client
	 * calling sequences to properly address this issue.
	 */
	.flags = CLK_SET_RATE_PARENT,
	.hdmi_phy_clk_ops = &mtk_hdmi_pll_ops,
	.hdmi_phy_enable_tmds = mtk_hdmi_phy_enable_tmds,
	.hdmi_phy_disable_tmds = mtk_hdmi_phy_disable_tmds,
	.hdmi_phy_configure = mtk_hdmi_phy_configure,
	.efuse_sw_mode = false,
	.power_control = mtk_hdmi_power_control,
};

MODULE_AUTHOR("Can Zeng <can.zeng@mediatek.com>");
MODULE_DESCRIPTION("MediaTek MT8195 HDMI PHY Driver");
MODULE_LICENSE("GPL v2");
