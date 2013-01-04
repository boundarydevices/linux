/*
 * Copyright 2011-2013 Freescale Semiconductor, Inc.
 * Copyright 2011 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/can/platform/flexcan.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/cpuidle.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/phy.h>
#include <linux/regmap.h>
#include <linux/micrel_phy.h>
#include <linux/mfd/syscon.h>
#include <linux/ahci_platform.h>
#include <asm/cpuidle.h>
#include <asm/smp_twd.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/system_misc.h>
#include <mach/common.h>
#include <mach/cpuidle.h>
#include <mach/hardware.h>


enum {
	HOST_CAP = 0x00,
	HOST_CAP_SSS = (1 << 27), /* Staggered Spin-up */
	HOST_CTL = 0x04, /* global host control */
	HOST_RESET = (1 << 0),  /* reset controller; self-clear */
	HOST_PORTS_IMPL	= 0x0c, /* bitmap of implemented ports */
	HOST_TIMER1MS = 0xe0, /* Timer 1-ms */
	HOST_VERSIONR = 0xf8, /* host version register*/

	PORT_SATA_SR = 0x128, /* Port0 SATA Status */
	PORT_PHY_CTL = 0x178, /* Port0 PHY Control */
	PORT_PHY_CTL_PDDQ_LOC = 0x100000, /* PORT_PHY_CTL bits */
};

void imx6q_restart(char mode, const char *cmd)
{
	struct device_node *np;
	void __iomem *wdog_base;

	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-wdt");
	wdog_base = of_iomap(np, 0);
	if (!wdog_base)
		goto soft;

	imx_src_prepare_restart();

	/* enable wdog */
	writew_relaxed(1 << 2, wdog_base);
	/* write twice to ensure the request will not get ignored */
	writew_relaxed(1 << 2, wdog_base);

	/* wait for reset to assert ... */
	mdelay(500);

	pr_err("Watchdog reset failed to assert reset\n");

	/* delay to allow the serial port to show the message */
	mdelay(50);

soft:
	/* we'll take a jump through zero as a poor second */
	soft_restart(0);
}

/* For imx6q sabrelite board: set KSZ9021RN RGMII pad skew */
static int ksz9021rn_phy_fixup(struct phy_device *phydev)
{
	if (IS_BUILTIN(CONFIG_PHYLIB)) {
		/* min rx data delay */
		phy_write(phydev, 0x0b, 0x8105);
		phy_write(phydev, 0x0c, 0x0000);

		/* max rx/tx clock delay, min rx/tx control delay */
		phy_write(phydev, 0x0b, 0x8104);
		phy_write(phydev, 0x0c, 0xf0f0);
		phy_write(phydev, 0x0b, 0x104);
	}

	return 0;
}

static void __init imx6q_sabrelite_cko1_setup(void)
{
	struct clk *cko1_sel, *ahb, *cko1;
	unsigned long rate;

	cko1_sel = clk_get_sys(NULL, "cko1_sel");
	ahb = clk_get_sys(NULL, "ahb");
	cko1 = clk_get_sys(NULL, "cko1");
	if (IS_ERR(cko1_sel) || IS_ERR(ahb) || IS_ERR(cko1)) {
		pr_err("cko1 setup failed!\n");
		goto put_clk;
	}
	clk_set_parent(cko1_sel, ahb);
	rate = clk_round_rate(cko1, 16000000);
	clk_set_rate(cko1, rate);
put_clk:
	if (!IS_ERR(cko1_sel))
		clk_put(cko1_sel);
	if (!IS_ERR(ahb))
		clk_put(ahb);
	if (!IS_ERR(cko1))
		clk_put(cko1);
}

static struct flexcan_platform_data flexcan_pdata[2];
static int en_gpio[2];
static int stby_gpio[2];
#define imx6q_flexcan_switch(id)	\
static void imx6q_flexcan ## id ##_switch(int enable)	\
{							\
	if (gpio_cansleep(en_gpio[id]))			\
		gpio_set_value_cansleep(en_gpio[id], enable);	\
	else						\
		gpio_set_value(en_gpio[id], enable);	\
							\
	if (gpio_cansleep(stby_gpio[id]))		\
		gpio_set_value_cansleep(stby_gpio[id], enable);	\
	else						\
		gpio_set_value(stby_gpio[id], enable);	\
}
imx6q_flexcan_switch(0)
imx6q_flexcan_switch(1)

static void __init imx6q_flexcan_fixup(void)
{
	struct device_node *np;
	char *s1, *s2;
	int i = 0;

	for_each_compatible_node(np, NULL, "fsl,imx6q-flexcan") {
		BUG_ON(i > 1);
		en_gpio[i] = of_get_named_gpio(np, "trx-en-gpio", 0);
		stby_gpio[i] = of_get_named_gpio(np, "trx-stby-gpio", 0);
		s1 = kasprintf(GFP_KERNEL, "flexcan%d-trx-en", i);
		s2 = kasprintf(GFP_KERNEL, "flexcan%d-trx-stby", i);
		if (!gpio_request_one(en_gpio[i], GPIOF_DIR_OUT, s1) &&
			!gpio_request_one(stby_gpio[i], GPIOF_DIR_OUT, s2)) {
			flexcan_pdata[i].transceiver_switch =
				i == 0 ? imx6q_flexcan0_switch : imx6q_flexcan1_switch ;
		}
		i++;
	}
}

static void __init imx6q_sabrelite_init(void)
{
	if (IS_BUILTIN(CONFIG_PHYLIB))
		phy_register_fixup_for_uid(PHY_ID_KSZ9021, MICREL_PHY_ID_MASK,
				ksz9021rn_phy_fixup);
	imx6q_sabrelite_cko1_setup();
}

static void __init imx6q_1588_init(void)
{
	struct regmap *gpr;

	gpr = syscon_regmap_lookup_by_compatible("fsl,imx6q-iomuxc-gpr");
	if (!IS_ERR(gpr))
		regmap_update_bits(gpr, 0x4, 1 << 21, 1 << 21);
	else
		pr_err("failed to find fsl,imx6q-iomux-gpr regmap\n");

}
static void __init imx6q_usb_init(void)
{
	struct regmap *anatop;

#define HW_ANADIG_USB1_CHRG_DETECT		0x000001b0
#define HW_ANADIG_USB2_CHRG_DETECT		0x00000210

#define BM_ANADIG_USB_CHRG_DETECT_EN_B		0x00100000
#define BM_ANADIG_USB_CHRG_DETECT_CHK_CHRG_B	0x00080000

	anatop = syscon_regmap_lookup_by_compatible("fsl,imx6q-anatop");
	if (!IS_ERR(anatop)) {
		/*
		 * The external charger detector needs to be disabled,
		 * or the signal at DP will be poor
		 */
		regmap_write(anatop, HW_ANADIG_USB1_CHRG_DETECT,
				BM_ANADIG_USB_CHRG_DETECT_EN_B
				| BM_ANADIG_USB_CHRG_DETECT_CHK_CHRG_B);
		regmap_write(anatop, HW_ANADIG_USB2_CHRG_DETECT,
				BM_ANADIG_USB_CHRG_DETECT_EN_B |
				BM_ANADIG_USB_CHRG_DETECT_CHK_CHRG_B);
	} else {
		pr_warn("failed to find fsl,imx6q-anatop regmap\n");
	}
}

#define MX6Q_SATA_BASE_ADDR	0x02200000
static unsigned int pwr_gpio;

/* AHCI module Initialization. */
static int imx_sata_init(struct device *dev, void __iomem *addr)
{
	u32 tmpdata;
	int ret = 0, iterations = 200;
	struct clk *clk, *sata_clk, *sata_ref_clk;
	struct device_node *np;
	struct regmap *gpr;

	/* Enable the pwr, clks and so on */
	np = of_find_node_by_name(NULL, "imx-ahci");
	pwr_gpio = of_get_named_gpio(np, "pwr-gpios", 0);
	if (gpio_is_valid(pwr_gpio)) {
		ret = gpio_request_one(pwr_gpio, GPIOF_OUT_INIT_HIGH,
				"AHCI PWR EN");
		if (ret)
			dev_err(dev, "failed to PWR EN!\n");
	} else {
		dev_info(dev, "no PWR EN pin!\n");
	}

	sata_clk = devm_clk_get(dev, "sata");
	if (IS_ERR(sata_clk)) {
		dev_err(dev, "can't get sata clock.\n");
		return PTR_ERR(sata_clk);
	}
	ret = clk_prepare_enable(sata_clk);
	if (ret < 0) {
		dev_err(dev, "can't prepare-enable sata clock\n");
		clk_put(sata_clk);
		return ret;
	}

	sata_ref_clk = devm_clk_get(dev, "sata_ref");
	if (IS_ERR(sata_ref_clk)) {
		dev_err(dev, "can't get sata_ref clock.\n");
		ret = PTR_ERR(sata_ref_clk);
		goto release_sata_clk;
	}
	ret = clk_prepare_enable(sata_ref_clk);
	if (ret < 0) {
		dev_err(dev, "can't prepare-enable sata_ref clock\n");
		clk_put(sata_ref_clk);
		ret = PTR_ERR(sata_ref_clk);
		goto release_sata_clk;
	}

	/* Set PHY Paremeters, two steps to configure the GPR13,
	 * one write for rest of parameters, mask of first write is 0x07FFFFFD,
	 * and the other one write for setting the mpll_clk_off_b
	 *.rx_eq_val_0(iomuxc_gpr13[26:24]),
	 *.los_lvl(iomuxc_gpr13[23:19]),
	 *.rx_dpll_mode_0(iomuxc_gpr13[18:16]),
	 *.mpll_ss_en(iomuxc_gpr13[14]),
	 *.tx_atten_0(iomuxc_gpr13[13:11]),
	 *.tx_boost_0(iomuxc_gpr13[10:7]),
	 *.tx_lvl(iomuxc_gpr13[6:2]),
	 *.mpll_clk_off(iomuxc_gpr13[1]),
	 *.tx_edgerate_0(iomuxc_gpr13[0]),
	 */
	gpr = syscon_regmap_lookup_by_compatible("fsl,imx6q-iomuxc-gpr");
	if (!IS_ERR(gpr)) {
		regmap_update_bits(gpr, 0x34, 0x07FFFFFD, 0x0593A044);
		regmap_update_bits(gpr, 0x34, 0x2, 0x2);
	} else {
		dev_err(dev, "failed to find fsl,imx6q-iomux-gpr regmap\n");
	}

	/*
	 * Make sure that SATA PHY is enabled
	 * The PDDQ mode is disabled.
	 */
	tmpdata = readl(addr + PORT_PHY_CTL);
	writel(tmpdata & (~PORT_PHY_CTL_PDDQ_LOC), addr + PORT_PHY_CTL);

	/*  Reset HBA */
	writel(HOST_RESET, addr + HOST_CTL);

	tmpdata = 0;
	while (readl(addr + HOST_VERSIONR) == 0) {
		tmpdata++;
		if (tmpdata > 100000) {
			dev_err(dev, "Can't recover from RESET HBA!\n");
			break;
		}
	}

	/* Get the AHB clock rate, and configure the TIMER1MS reg later */
	clk = clk_get_sys(NULL, "ahb");
	if (IS_ERR(clk)) {
		dev_err(dev, "no ahb clock.\n");
		ret = PTR_ERR(clk);
		goto error;
	}
	tmpdata = clk_get_rate(clk) / 1000;
	clk_put(clk);

	writel(tmpdata, addr + HOST_TIMER1MS);

	tmpdata = readl(addr + HOST_CAP);
	if (!(tmpdata & HOST_CAP_SSS)) {
		tmpdata |= HOST_CAP_SSS;
		writel(tmpdata, addr + HOST_CAP);
	}

	if (!(readl(addr + HOST_PORTS_IMPL) & 0x1))
		writel((readl(addr + HOST_PORTS_IMPL) | 0x1),
			addr + HOST_PORTS_IMPL);

	/* Release resources when there is no device on the port */
	do {
		if ((readl(addr + PORT_SATA_SR) & 0xF) == 0)
			usleep_range(2000, 3000);
		else
			break;

		if (iterations == 0) {
			/*  Enter into PDDQ mode, save power */
			pr_info("No sata disk, enter into PDDQ mode.\n");
			tmpdata = readl(addr + PORT_PHY_CTL);
			writel(tmpdata | PORT_PHY_CTL_PDDQ_LOC,
					addr + PORT_PHY_CTL);
			ret = -ENODEV;
			goto error;
		}
	} while (iterations-- > 0);

	return 0;

error:
	regmap_update_bits(gpr, 0x34, 0x2, 0x2);
	clk_disable_unprepare(sata_ref_clk);
release_sata_clk:
	clk_disable_unprepare(sata_clk);

	return ret;
}

static void imx_sata_exit(struct device *dev)
{
	struct clk *sata_clk, *sata_ref_clk;
	struct regmap *gpr;

	gpr = syscon_regmap_lookup_by_compatible("fsl,imx6q-iomuxc-gpr");
	if (!IS_ERR(gpr))
		regmap_update_bits(gpr, 0x34, 0x2, 0x2);
	else
		dev_err(dev, "failed to find fsl,imx6q-iomux-gpr regmap\n");

	sata_clk = devm_clk_get(dev, "sata");
	if (IS_ERR(sata_clk))
		dev_err(dev, "can't get sata clock.\n");
	else
		clk_disable_unprepare(sata_clk);
	sata_ref_clk = devm_clk_get(dev, "sata_ref");
	if (IS_ERR(sata_ref_clk))
		dev_err(dev, "can't get sata_ref clock.\n");
	else
		clk_disable_unprepare(sata_ref_clk);

	/* Disable SATA power,in-activate pwr_gpio */
	if (gpio_is_valid(pwr_gpio)) {
		gpio_set_value(pwr_gpio, 0);
		gpio_free(pwr_gpio);
	}
}
static struct ahci_platform_data imx_sata_pdata = {
	.init = imx_sata_init,
	.exit = imx_sata_exit,
};

/* Add auxdata to pass platform data */
static const struct of_dev_auxdata imx6q_auxdata_lookup[] __initconst = {
	OF_DEV_AUXDATA("snps,imx-ahci", MX6Q_SATA_BASE_ADDR, "imx-ahci",
			&imx_sata_pdata),
	OF_DEV_AUXDATA("fsl,imx6q-flexcan", 0x02090000, NULL, &flexcan_pdata[0]),
	OF_DEV_AUXDATA("fsl,imx6q-flexcan", 0x02094000, NULL, &flexcan_pdata[1]),
	{ /* sentinel */ }
};

static void __init imx6q_init_machine(void)
{
	if (of_machine_is_compatible("fsl,imx6q-sabrelite"))
		imx6q_sabrelite_init();

	of_platform_populate(NULL, of_default_bus_match_table,
			imx6q_auxdata_lookup, NULL);

	imx6q_pm_init();
	imx6q_usb_init();
	imx6q_1588_init();
	imx6q_flexcan_fixup();
}

static struct cpuidle_driver imx6q_cpuidle_driver = {
	.name			= "imx6q_cpuidle",
	.owner			= THIS_MODULE,
	.en_core_tk_irqen	= 1,
	.states[0]		= ARM_CPUIDLE_WFI_STATE,
	.state_count		= 1,
};

static void __init imx6q_init_late(void)
{
	imx_cpuidle_init(&imx6q_cpuidle_driver);
}

static void __init imx6q_map_io(void)
{
	imx_lluart_map_io();
	imx_scu_map_io();
	imx6q_clock_map_io();
}

static int __init imx6q_gpio_add_irq_domain(struct device_node *np,
				struct device_node *interrupt_parent)
{
	static int gpio_irq_base = MXC_GPIO_IRQ_START + ARCH_NR_GPIOS;

	gpio_irq_base -= 32;
	irq_domain_add_legacy(np, 32, gpio_irq_base, 0, &irq_domain_simple_ops,
			      NULL);

	return 0;
}

static const struct of_device_id imx6q_irq_match[] __initconst = {
	{ .compatible = "arm,cortex-a9-gic", .data = gic_of_init, },
	{ .compatible = "fsl,imx6q-gpio", .data = imx6q_gpio_add_irq_domain, },
	{ /* sentinel */ }
};

static void __init imx6q_init_irq(void)
{
	l2x0_of_init(0, ~0UL);
	imx_src_init();
	imx_gpc_init();
	of_irq_init(imx6q_irq_match);
}

static void __init imx6q_timer_init(void)
{
	mx6q_clocks_init();
	twd_local_timer_of_register();
}

static struct sys_timer imx6q_timer = {
	.init = imx6q_timer_init,
};

static const char *imx6q_dt_compat[] __initdata = {
	"fsl,imx6q",
	NULL,
};

DT_MACHINE_START(IMX6Q, "Freescale i.MX6 Quad (Device Tree)")
	.map_io		= imx6q_map_io,
	.init_irq	= imx6q_init_irq,
	.handle_irq	= imx6q_handle_irq,
	.timer		= &imx6q_timer,
	.init_machine	= imx6q_init_machine,
	.init_late      = imx6q_init_late,
	.dt_compat	= imx6q_dt_compat,
	.restart	= imx6q_restart,
MACHINE_END
