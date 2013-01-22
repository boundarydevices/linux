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
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/iram_alloc.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/cpuidle.h>
#include <linux/gpio.h>
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/phy.h>
#include <linux/pinctrl/machine.h>
#include <linux/regmap.h>
#include <linux/micrel_phy.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/imx6q-iomuxc-gpr.h>
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

#define PMU_REG_CORE	0x140
#define IMX6Q_ANALOG_DIGPROG     0x260
#define WDOG_WMCR	0x8

static int mx6_cpu_type;
static int mx6_cpu_revision;
static int sd30_en;

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

static phys_addr_t ggpu_phys;
static u32 ggpu_size = SZ_128M;

static int spdif_en;
enum MX6_CPU_TYPE {
	MXC_CPU_MX6Q = 1,
	MXC_CPU_MX6DL =	2,
	MXC_CPU_MX6SL =	3,
};

static int mx6_cpu_type;
static int mx6_cpu_revision;
static void __iomem *wdog_base1;
static void __iomem *wdog_base2;

static int __init early_enable_sd30(char *p)
{
	sd30_en = 1;
	return 0;
}
early_param("sd30", early_enable_sd30);

static void remove_one_pin_from_node(const char *path,
			   const char *phandle_name,
			   const char *name,
			   u32 pin)
{
	struct device_node *np, *pinctrl;
	struct property *pbase;
	struct property *poldbase;
	u32 *psize;
	int i = 0, j = 0;

	np = of_find_node_by_path(path);
	pinctrl = of_parse_phandle(np, phandle_name, 0);
	poldbase = of_find_property(pinctrl, name, NULL);
	if (poldbase) {
		pbase = kzalloc(sizeof(*pbase)
				+ poldbase->length - 8, GFP_KERNEL);
		if (pbase == NULL)
			return;
		psize = (u32 *)(pbase + 1);
		pbase->length = poldbase->length - 8;
		pbase->name = kstrdup(poldbase->name, GFP_KERNEL);
		if (!pbase->name) {
			kfree(pbase);
			return;
		}

		pbase->value = psize;
		for (i = 0, j = 0; i < pbase->length; i += 4, j += 4) {
			if (cpu_to_be32(pin)
					== *(u32 *)(poldbase->value + j)) {
				i -= 4;
				j += 4;
				continue;
			}
			*(u32 *)(pbase->value + i) =
				*(u32 *)(poldbase->value + j);
		}

		prom_update_property(pinctrl, pbase, poldbase);
	}
}

static void __init imx6q_rm_flexcan1_en_pin(void)
{
	remove_one_pin_from_node("/soc/aips-bus@02000000/iomuxc@020e0000",
			"pinctrl-0", "fsl,pins", 1051);
}

static void __init imx6q_rm_usdhc3_vselect_pin(void)
{
	remove_one_pin_from_node("/soc/aips-bus@02100000/usdhc@02198000",
			"pinctrl-0", "fsl,pins", 1048);
	remove_one_pin_from_node("/soc/aips-bus@02100000/usdhc@02198000",
			"pinctrl-1", "fsl,pins", 1048);
	remove_one_pin_from_node("/soc/aips-bus@02100000/usdhc@02198000",
			"pinctrl-2", "fsl,pins", 1048);
}

static void __init imx6q_add_no18v_to_sdhc3(void)
{
	struct device_node *np;
	struct property *pbase;
	struct property *poldbase;

	np = of_find_node_by_path("/soc/aips-bus@02100000/usdhc@02198000");
	if (np) {
		poldbase = of_find_property(np, "no-1-8-v", NULL);
		if (poldbase)
			return;

		pbase = kzalloc(sizeof(*pbase), GFP_KERNEL);
		if (pbase == NULL)
			return;
		pbase->name = kstrdup("no-1-8-v", GFP_KERNEL);
		if (!pbase->name) {
			kfree(pbase);
			return;
		}
		prom_add_property(np, pbase);
	}
}

void imx6q_restart(char mode, const char *cmd)
{
	struct regmap *anatop;
	unsigned int value;
	int ret;

	if (!wdog_base1 || !wdog_base2)
		goto soft;

	imx_src_prepare_restart();


	anatop = syscon_regmap_lookup_by_compatible("fsl,imx6q-anatop");
	if (IS_ERR(anatop)) {
		pr_err("failed to find imx6q-anatop regmap!\n");
		return;
	}

	ret = regmap_read(anatop, PMU_REG_CORE, &value);
	if (ret) {
		pr_err("failed to read anatop 0x140 regmap!\n");
		return;
	}
	/* VDDARM bypassed? */
	if ((value & 0x1f) != 0x1f) {
		pr_info("Not in bypass-mode!\n");
		value = (1 << 2);
	} else {
		/*
		 * Sabresd board use WDOG2 to reset extern pmic in bypass mode,
		 * so do more WDOG2 reset here. Do not set SRS,since we will
		 * trigger external POR later
		 */
		pr_info("In bypass-mode!\n");
		value = 0x14;
		writew_relaxed(value, wdog_base2);
		writew_relaxed(value, wdog_base2);
	}

	/* enable wdog */
	writew_relaxed(value, wdog_base1);
	/* write twice to ensure the request will not get ignored */
	writew_relaxed(value, wdog_base1);

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

/* For imx6q arm2 & sabresd & sabreauto board:
 * set AR803x RGMII output 125MHz clock
 */
static int ar803x_phy_fixup(struct phy_device *phydev)
{
	unsigned short val;

	if (IS_BUILTIN(CONFIG_PHYLIB)) {
		/* disable phy AR8031 SmartEEE function. */
		phy_write(phydev, 0xd, 0x3);
		phy_write(phydev, 0xe, 0x805d);
		phy_write(phydev, 0xd, 0x4003);
		val = phy_read(phydev, 0xe);
		val &= ~(0x1 << 8);
		phy_write(phydev, 0xe, val);

		/* To enable AR8031 ouput a 125MHz clk from CLK_25M */
		phy_write(phydev, 0xd, 0x7);
		phy_write(phydev, 0xe, 0x8016);
		phy_write(phydev, 0xd, 0x4007);
		val = phy_read(phydev, 0xe);

		val &= 0xffe3;
		val |= 0x18;
		phy_write(phydev, 0xe, val);

		/* introduce tx clock delay */
		phy_write(phydev, 0x1d, 0x5);
		val = phy_read(phydev, 0x1e);
		val |= 0x0100;
		phy_write(phydev, 0x1e, val);

		/*check phy power*/
		val = phy_read(phydev, 0x0);
		if (val & BMCR_PDOWN)
			phy_write(phydev, 0x0, (val & ~BMCR_PDOWN));
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

static int __init imx6q_flexcan_fixup(void)
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

	return 0;
}

static void __init imx6q_ar803x_phy_fixup(void)
{
	/* The phy layer fixup for AR8031 and AR8033 */
	if (IS_BUILTIN(CONFIG_PHYLIB))
		phy_register_fixup_for_id(PHY_ANY_ID, ar803x_phy_fixup);
}

static void __init imx6q_arm2_init(void)
{
	if (!sd30_en) {
		imx6q_rm_usdhc3_vselect_pin();
		imx6q_add_no18v_to_sdhc3();
	} else {
		imx6q_rm_flexcan1_en_pin();
	}

	imx6q_ar803x_phy_fixup();
}

static void __init imx6q_sabrelite_init(void)
{
	if (IS_BUILTIN(CONFIG_PHYLIB))
		phy_register_fixup_for_uid(PHY_ID_KSZ9021, MICREL_PHY_ID_MASK,
				ksz9021rn_phy_fixup);
	imx6q_sabrelite_cko1_setup();
}

static void __init imx6q_sabresd_cko_setup(void)
{
	struct clk *cko2, *cko1_cko2_sel;

	cko1_cko2_sel = clk_get_sys(NULL, "cko1_cko2_sel");
	cko2 = clk_get_sys(NULL, "cko2");
	if (IS_ERR(cko1_cko2_sel) || IS_ERR(cko2)) {
		pr_err("cko setup failed!\n");
		goto put_clk_sd;
	}

	clk_set_parent(cko1_cko2_sel, cko2);
	clk_prepare_enable(cko2);

	return;
put_clk_sd:
	if (!IS_ERR(cko1_cko2_sel))
		clk_put(cko1_cko2_sel);
	if (!IS_ERR(cko2))
		clk_put(cko2);
}

static void __init imx6q_sabresd_init(void)
{
	imx6q_ar803x_phy_fixup();
	imx6q_sabresd_cko_setup();
}

static int __init early_enable_spdif(char *p)
{
	/* Basically we don't need SPDIF on sabrelite and sabresd */
	if ((of_machine_is_compatible("fsl,imx6q-sabrelite") ||
		of_machine_is_compatible("fsl,imx6q-sabresd")))
		return 0;

	spdif_en = 1;
	return 0;
}
early_param("spdif", early_enable_spdif);

static void __init imx6q_i2c3_sda_pindel(void)
{
	struct device_node *np, *pinctrl_i2c3;
	struct property *pbase;
	struct property *poldbase;
	u32 *psize;
	int i = 0, j = 0;

	/* Cancel GPIO_16 for I2C3 SDA config */
	np = of_find_node_by_path("/soc/aips-bus@02100000/i2c@021a8000");
	pinctrl_i2c3 = of_parse_phandle(np, "pinctrl-0", 0);
	poldbase = of_find_property(pinctrl_i2c3, "fsl,pins", NULL);
	if (poldbase) {
		pbase = kzalloc(sizeof(*pbase) + poldbase->length - 8,
					GFP_KERNEL);
		if (pbase == NULL)
			return;
		psize = (u32 *)(pbase + 1);
		/* Cancel  1037 0x4001b8b1 MX6Q_PAD_GPIO_16__I2C3_SDA */
		pbase->length = poldbase->length - 8;
		pbase->name = kstrdup(poldbase->name, GFP_KERNEL);
		if (!pbase->name) {
			kfree(pbase);
			return;
		}

		pbase->value = psize;
		for (i = 0, j = 0; i < pbase->length; i += 4, j += 4) {
			if (cpu_to_be32(1037) ==
				*(u32 *)(poldbase->value + j)) {
				i -= 4;
				j += 4;
				continue;
			}
			*(u32 *)(pbase->value + i) =
				*(u32 *)(poldbase->value + j);
		}

		prom_update_property(pinctrl_i2c3, pbase, poldbase);
	}
}

static void __init imx6q_1588_init(void)
{
	struct regmap *gpr;
	struct device_node *np, *pinctrl_enet;
	struct property *pbase;
	struct property *poldbase;
	u32 *psize;
	int i = 0;

	if (!IS_BUILTIN(CONFIG_FEC_PTP))
		return;

	gpr = syscon_regmap_lookup_by_compatible("fsl,imx6q-iomuxc-gpr");
	if (!IS_ERR(gpr))
		regmap_update_bits(gpr, 0x4, 1 << 21, 1 << 21);
	else
		pr_err("failed to find fsl,imx6q-iomux-gpr regmap\n");

	/* config GPIO_16 for IEEE1588 */
	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-fec");
	pinctrl_enet = of_parse_phandle(np, "pinctrl-0", 0);
	poldbase = of_find_property(pinctrl_enet, "fsl,pins", NULL);
	if (poldbase) {
		pbase = kzalloc(sizeof(*pbase) + poldbase->length + 8, GFP_KERNEL);
		if (pbase == NULL)
			return;
		psize = (u32 *)(pbase + 1);
		pbase->length = poldbase->length + 8;
		pbase->name = kstrdup(poldbase->name, GFP_KERNEL);
		if (!pbase->name) {
			kfree(pbase);
			return;
		}

		pbase->value = psize;
		for (i = 0; i < poldbase->length; i += 4)
			*(u32 *)(pbase->value + i) = *(u32 *)(poldbase->value + i);
		/* 1033 0x4001b0a8 MX6Q_PAD_GPIO_16__ENET_ANATOP_ETHERNET_REF_OUT */
		*(u32 *)(pbase->value + i) =  cpu_to_be32(1033);
		*(u32 *)(pbase->value + i + 4) =  cpu_to_be32(0x4001b0a8);

		prom_update_property(pinctrl_enet, pbase, poldbase);
	}

	/* arm2 and Sabrelite GPIO_16 is shared by 1588, i2c3 SDA, and spdif_en
	 * if enable PTP, cancel the pinctrl for i2c sda and disable spdif_en
	 */
	if (of_machine_is_compatible("fsl,imx6q-sabrelite") ||
		of_machine_is_compatible("fsl,imx6q-arm2")) {
		imx6q_i2c3_sda_pindel();
		spdif_en = 0;
	}
}

static void __init imx6q_gpu_init(void)
{
	struct device_node *np;
	struct platform_device *pdev = NULL;
	struct property *pbase;
	struct property *poldbase;
	u32 *psize;

	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-gpu");
	if (np)
		pdev = of_find_device_by_node(np);

	if (ggpu_phys) {
		pbase = kzalloc(sizeof(*pbase)+sizeof(u32), GFP_KERNEL);
		if (pbase == NULL)
			return;
		psize = (u32 *)(pbase + 1);
		pbase->length = sizeof(u32);
		pbase->name = kstrdup("contiguoussize", GFP_KERNEL);
		if (!pbase->name) {
			kfree(pbase);
			return;
		}

		pbase->value = psize;
		(*psize) = cpu_to_be32p(&ggpu_size);
		poldbase = of_find_property(np, "contiguoussize", NULL);
		if (poldbase)
			prom_update_property(np, pbase, poldbase);
		else
			prom_add_property(np, pbase);

		pbase = kzalloc(sizeof(*pbase)+sizeof(phys_addr_t), GFP_KERNEL);
		if (pbase == NULL)
			return;
		pbase->value = pbase + 1;
		pbase->length = sizeof(phys_addr_t);
		pbase->name = kstrdup("contiguousbase", GFP_KERNEL);
		if (!pbase->name) {
			kfree(pbase);
			return;
		}
		memcpy(pbase->value, &ggpu_phys, sizeof(phys_addr_t));
		poldbase = of_find_property(np, "contiguousbase", NULL);
		if (poldbase)
			prom_update_property(np, pbase, poldbase);
		else
			prom_add_property(np, pbase);
	}

	return;
}
static void __init imx6q_gpu_reserve(void)
{
	/* DTS is not ready here, only global variables can be used. */
	ggpu_phys = memblock_alloc_base(ggpu_size, SZ_4K, SZ_1G);
	memblock_remove(ggpu_phys, ggpu_size);
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

static int __init imx6_soc_init(void)
{
	struct regmap *gpr;
	unsigned int mask;
	int ret;
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-wdt");
	wdog_base1 = of_iomap(np, 0);
	WARN_ON(!wdog_base1);

	np = of_find_compatible_node(np, NULL, "fsl,imx6q-wdt");
	wdog_base2 = of_iomap(np, 0);
	WARN_ON(!wdog_base2);

	/*
	 * clear PDE bit of WDOGx to avoid system reset in 16 seconds
	 * if use WDOGx pin to reset external pmic. Do here repeated, whatever
	 * it will be done in bootloader.
	 */
	writew_relaxed(0, wdog_base1 + WDOG_WMCR);
	writew_relaxed(0, wdog_base2 + WDOG_WMCR);

	ret = iram_init(MX6Q_IRAM_BASE_ADDR, MX6Q_IRAM_SIZE);
	if (ret < 0) {
		pr_err("iram init fail:%d!\n", ret);
		return ret;
	}

	gpr = syscon_regmap_lookup_by_compatible("fsl,imx6q-iomuxc-gpr");
	if (IS_ERR(gpr)) {
		pr_err("failed to find fsl,imx6q-iomux-gpr regmap\n");
		return PTR_ERR(gpr);
	}

	/*
	 * enable AXI cache for VDOA/VPU/IPU
	 * set IPU AXI-id0 Qos=0xf(bypass) AXI-id1 Qos=0x7
	 * clear OCRAM_CTL bits to disable pipeline control
	 */
	ret = regmap_update_bits(gpr, IOMUXC_GPR3, IMX6Q_GPR3_OCRAM_CTL_MASK,
					~IMX6Q_GPR3_OCRAM_CTL_MASK);
	if (ret)
		goto out;

	mask = IMX6Q_GPR4_VDOA_WR_CACHE_SEL |
		IMX6Q_GPR4_VDOA_RD_CACHE_SEL |
		IMX6Q_GPR4_VDOA_WR_CACHE_VAL |
		IMX6Q_GPR4_VDOA_RD_CACHE_VAL |
		IMX6Q_GPR4_VPU_WR_CACHE_SEL |
		IMX6Q_GPR4_VPU_RD_CACHE_SEL |
		IMX6Q_GPR4_VPU_P_WR_CACHE_VAL |
		IMX6Q_GPR4_VPU_P_RD_CACHE_VAL |
		IMX6Q_GPR4_IPU_WR_CACHE_CTL |
		IMX6Q_GPR4_IPU_RD_CACHE_CTL;
	ret = regmap_update_bits(gpr, IOMUXC_GPR4, mask, mask);
	if (ret)
		goto out;

	ret = regmap_write(gpr, IOMUXC_GPR6, IMX6Q_GPR6_IPU1_QOS_VAL);
	if (ret)
		goto out;

	ret = regmap_write(gpr, IOMUXC_GPR7, IMX6Q_GPR7_IPU2_QOS_VAL);
	if (ret)
		goto out;

	return 0;
out:
	pr_err("error: regmap update/write iomux gpr ret:%d\n", ret);
	return ret;
}

static void __init imx6q_init_machine(void)
{
	if (of_machine_is_compatible("fsl,imx6q-sabrelite"))
		imx6q_sabrelite_init();
	else if (of_machine_is_compatible("fsl,imx6q-sabresd"))
		imx6q_sabresd_init();
	else if (of_machine_is_compatible("fsl,imx6q-arm2"))
		imx6q_arm2_init();

	of_platform_populate(NULL, of_default_bus_match_table,
			imx6q_auxdata_lookup, NULL);

	imx6_soc_init();
	imx6q_pm_init();
	imx6q_usb_init();
	imx6q_1588_init();
	if (IS_ENABLED(CONFIG_MXC_GPU_VIV))
		imx6q_gpu_init();
}

static void __init imx6q_init_late(void)
{
	imx6q_flexcan_fixup();
	/*
	 * WAIT mode is broken on TO 1.0 and 1.1, and there is no point to
	 * have cpuidle running on them.
	 */
	if (imx6q_revision() > IMX_CHIP_REVISION_1_1)
		imx6q_cpuidle_init();
}

static void __init imx6q_reserve(void)
{
	imx6q_gpu_reserve();
}

static void __init imx6q_map_io(void)
{
	init_consistent_dma_size(SZ_8M);
	imx_lluart_map_io();
	imx_scu_map_io();
	imx6q_clock_map_io();
	imx_pm_map_io();
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

static void  check_imx6q_cpu(void)
{
	struct device_node *np;
	void __iomem *base;
	static u32 rev;

	if (of_machine_is_compatible("fsl,imx6q")) {
		mx6_cpu_type = MXC_CPU_MX6Q;
		pr_info("Find i.MX6Q chip\n");
	} else if (of_machine_is_compatible("fsl,imx6dl")) {
		mx6_cpu_type = MXC_CPU_MX6DL;
		pr_info("Find i.MX6DL chip\n");
	} else if (of_machine_is_compatible("fsl,imx6sl")) {
		mx6_cpu_type = MXC_CPU_MX6SL;
		pr_info("Find i.MX6SL chip\n");
	} else {
		mx6_cpu_type = 0;
		pr_err("Can't find any i.MX6 chip !\n");
		return;
	}

	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-anatop");
	base = of_iomap(np, 0);
	WARN_ON(!base);
	rev =  readl_relaxed(base + IMX6Q_ANALOG_DIGPROG);
	iounmap(base);

	switch (rev & 0xff) {
	case 0:
		mx6_cpu_revision = IMX_CHIP_REVISION_1_0;
		pr_info("SOC revision TO1.0\n");
		break;
	case 1:
		mx6_cpu_revision = IMX_CHIP_REVISION_1_1;
		pr_info("SOC revision TO1.1\n");
		break;
	case 2:
		mx6_cpu_revision = IMX_CHIP_REVISION_1_2;
		pr_info("SOC revision TO1.2\n");
		break;
	default:
		mx6_cpu_revision = IMX_CHIP_REVISION_UNKNOWN;
		pr_err("SOC revision unrecognized!\n");
		break;
	}
}

int cpu_is_imx6dl(void)
{
	return (mx6_cpu_type == MXC_CPU_MX6DL) ? 1 : 0;
}

int cpu_is_imx6q(void)
{
	return (mx6_cpu_type == MXC_CPU_MX6Q) ? 1 : 0;
}

int imx6q_revision(void)
{
	return mx6_cpu_revision;
}
static void __init imx6q_timer_init(void)
{
	check_imx6q_cpu();
	mx6q_clocks_init();
	twd_local_timer_of_register();
}

static struct sys_timer imx6q_timer = {
	.init = imx6q_timer_init,
};

static const char *imx6q_dt_compat[] __initdata = {
	"fsl,imx6dl",
	"fsl,imx6q",
	NULL,
};

DT_MACHINE_START(IMX6Q, "Freescale i.MX6 Quad/DualLite (Device Tree)")
	.map_io		= imx6q_map_io,
	.reserve	= imx6q_reserve,
	.init_irq	= imx6q_init_irq,
	.handle_irq	= imx6q_handle_irq,
	.timer		= &imx6q_timer,
	.init_machine	= imx6q_init_machine,
	.init_late      = imx6q_init_late,
	.dt_compat	= imx6q_dt_compat,
	.restart	= imx6q_restart,
MACHINE_END
