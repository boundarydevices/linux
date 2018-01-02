#ifndef __DMIC_H__
#define __DMIC_H__

struct aml_dmic_priv {
	void __iomem *pdm_base;
	struct pinctrl *dmic_pins;
	struct clk *clk_pdm;
	struct clk *clk_mclk;
};

extern struct aml_dmic_priv *dmic_pub;

#endif
