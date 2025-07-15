// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Copyright (c) 2025 Richtek Technology Corp.
 *
 * Author: Yuren Chen <ren_chen@richtek.com>
 */

#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/tcpci.h>
#include <linux/usb/tcpm.h>

/* Vendor Register Define */
#define MT6375_REG_PHYCTRL1		0x80
#define MT6375_REG_PHYCTRL2		0x81
#define MT6375_REG_PHYCTRL3		0x82
#define MT6375_REG_PHYCTRL7		0x86
#define MT6375_REG_VCONCTRL3		0x8C
#define MT6375_REG_SYSCTRL1		0x8F
#define MT6375_REG_PHYCTRL9		0xAC
#define MT6375_REG_SYSCTRL3		0xB0
#define MT6375_REG_TCPCCTRL1		0xB1
#define MT6375_REG_TCPCCTRL2		0xB2
#define MT6375_REG_TCPCCTRL3		0xB3
#define MT6375_REG_TCPCCTRL4		0xB4
#define MT6375_REG_SYSCTRL4		0xB8
#define MT6375_REG_LPWRCTRL5		0xBD
#define MT6375_REG_WATCHDOGCTRL		0xBE
#define MT6375_REG_I2CTORSTCTRL		0xBF
#define MT6375_REG_SHIELDCTRL1		0xCA
#define MT6375_REG_TYPECOTPCTRL		0xCD
#define MT6375_REG_FODCTRL		0xCF
#define MT6375_REG_CHG_TOP1		0x120
#define MT6375_REG_SYSCTRL5		0xF210

#define MT6375_VID	0x29CF
#define MT6375_PID	0x6375

struct mt6375_tcpc_info {
	struct tcpci_data tdata;
	struct tcpci *tcpci;
	struct device *dev;
	struct regulator *vbus;
	bool src_en;
	int irq;
};

static inline int mt6375_write8(struct regmap *regmap,
				unsigned int reg, u8 data)
{
	return regmap_raw_write(regmap, reg, &data, sizeof(data));
}

static inline int mt6375_read8(struct regmap *regmap,
			       unsigned int reg, u8 *data)
{
	return regmap_raw_read(regmap, reg, data, sizeof(*data));
}

static inline int mt6375_write16(struct regmap *regmap,
				 unsigned int reg, u16 data)
{
	data = cpu_to_le16(data);
	return regmap_raw_write(regmap, reg, &data, sizeof(data));
}

static inline int mt6375_read16(struct regmap *regmap,
				unsigned int reg, u16 *data)
{
	int ret;

	ret = regmap_raw_read(regmap, reg, data, sizeof(*data));
	if (ret < 0)
		return ret;
	*data = le16_to_cpu(*data);
	return 0;
}

static const struct reg_sequence mt6375_tcpc_init_settings[] = {
	/* Reset registers */
	{ MT6375_REG_SYSCTRL3, 0x01, 2000 },
	/* Config I2C timeout reset enable, and timeout to 200ms */
	{ MT6375_REG_I2CTORSTCTRL, 0x8F, 0 },
	/* tTCPCFilter = 500us */
	{ MT6375_REG_TCPCCTRL1, 0x14, 0 },
	/* DRP Toggle Cycle : 51.2ms (51.2 + 6.4*val ms) */
	{ MT6375_REG_TCPCCTRL2, 0x00, 0 },
	/* DRP Duty Ctrl : dcSRC: 308/1024 ((val+1)/1024) */
	{ MT6375_REG_TCPCCTRL3, 0x33, 0 },
	{ MT6375_REG_TCPCCTRL4, 0x01, 0 },
	/*
	 * Transition toggle count = 7
	 * OSC_FREQ_CFG = 0x01
	 * RXFilter out 100ns glitch = 0x00
	 */
	{ MT6375_REG_PHYCTRL1, 0x74, 0 },
	/* PHY_CDR threshold = 0x3A */
	{ MT6375_REG_PHYCTRL2, 0x3A, 0 },
	/* Transition window time = 43.29us */
	{ MT6375_REG_PHYCTRL3, 0x82, 0 },
	/* BMC decoder idle time = 17.982us */
	{ MT6375_REG_PHYCTRL7, 0x36, 0 },
	/* Retry period = 26.208us */
	{ MT6375_REG_PHYCTRL9, 0x3C, 0 },
	/* Enable PD Vconn current limit mode, ocp sel 100mA, and analog OVP */
	{ MT6375_REG_VCONCTRL3, 0x11, 0 },
	/* VBUS_VALID debounce time: 375us */
	{ MT6375_REG_LPWRCTRL5, 0x2F, 0 },
	/* Enable CC open 40ms when PMIC SYSUV, disable RPDET_AUTO & CTD */
	{ MT6375_REG_SHIELDCTRL1, 0x31, 0 },
	/* Disable OTP */
	{ MT6375_REG_TYPECOTPCTRL, 0x00, 0 },
	/* Disable FOD */
	{ MT6375_REG_FODCTRL, 0x10, 0 },
	/*
	 * Enable Alert.CCStatus assertion
	 * when CCStatus.Looking4Connection changes
	 */
	{ TCPC_TCPC_CTRL, 0x40, 0 },
	/* Enable auto dischg timer for IQ about 12mA consumption */
	{ MT6375_REG_WATCHDOGCTRL, 0xEB, 0 },
	/* Disable bleed dischg for IQ about 2mA consumption */
	{ TCPC_POWER_CTRL, 0x60, 0 },
	/* Auto LPM */
	{ MT6375_REG_SYSCTRL4, 0x0A, 0 },
	{ MT6375_REG_SYSCTRL5, 0x35, 0 },
	/* SHIPPING off, AUTOIDLE enable, TIMEOUT = 6.4ms */
	{ MT6375_REG_SYSCTRL1, 0xB8, 1000 },
};

static int mt6375_tcpc_init(struct tcpci *tcpci, struct tcpci_data *tdata)
{
	return regmap_multi_reg_write(tdata->regmap, mt6375_tcpc_init_settings,
				      ARRAY_SIZE(mt6375_tcpc_init_settings));
}

static int mt6375_tcpc_set_vbus(struct tcpci *tcpci, struct tcpci_data *tdata,
				bool source, bool sink)
{
	struct mt6375_tcpc_info *mti = container_of(tdata,
						    struct mt6375_tcpc_info,
						    tdata);
	struct regmap *regmap = mti->tdata.regmap;
	int ret;

	if (source == mti->src_en)
		return 0;

	if (mti->vbus)
		ret = (source ? regulator_enable : regulator_disable)(mti->vbus);
	else
		ret = regmap_update_bits(regmap, MT6375_REG_CHG_TOP1, BIT(2), source ? BIT(2) : 0);

	if (!ret)
		mti->src_en = source;
	return ret;
}

static irqreturn_t mt6375_tcpc_evt_handler(int irq, void *dev_id)
{
	struct mt6375_tcpc_info *mti = dev_id;

	while (tcpci_irq(mti->tcpci) != IRQ_NONE)
		;

	return IRQ_HANDLED;
}

static int mt6375_tcpc_check_ids(struct mt6375_tcpc_info *mti)
{
	struct regmap *regmap = mti->tdata.regmap;
	int ret;
	u16 id;

	ret = mt6375_read16(regmap, TCPC_VENDOR_ID, &id);
	if (ret < 0) {
		dev_err(mti->dev, "failed to read vid(%d)\n", ret);
		return ret;
	}
	if (id != MT6375_VID) {
		dev_err(mti->dev, "incorrect vid(0x%04X)\n", id);
		return -ENODEV;
	}

	ret = mt6375_read16(regmap, TCPC_PRODUCT_ID, &id);
	if (ret < 0) {
		dev_err(mti->dev, "failed to read pid(%d)\n", ret);
		return ret;
	}
	if (id != MT6375_PID) {
		dev_err(mti->dev, "incorrect pid(0x%04X)\n", id);
		return -ENODEV;
	}

	ret = mt6375_read16(regmap, TCPC_BCD_DEV, &id);
	if (ret < 0) {
		dev_err(mti->dev, "failed to read did(%d)\n", ret);
		return ret;
	}
	dev_info(mti->dev, "did = 0x%04X\n", id);

	return 0;
}

static int mt6375_tcpc_probe(struct platform_device *pdev)
{
	struct mt6375_tcpc_info *mti;
	struct device *dev = &pdev->dev;
	int ret;

	mti = devm_kzalloc(dev, sizeof(*mti), GFP_KERNEL);
	if (!mti)
		return -ENOMEM;

	mti->dev = dev;

	mti->tdata.regmap = dev_get_regmap(dev->parent, NULL);
	if (!mti->tdata.regmap)
		return dev_err_probe(dev, -ENODEV, "failed to get regmap\n");

	ret = mt6375_tcpc_check_ids(mti);
	if (ret < 0)
		return dev_err_probe(dev, ret, "failed to check IDs\n");

	mti->vbus = devm_regulator_get_optional(dev, "vbus");
	if (IS_ERR(mti->vbus)) {
		mti->vbus = NULL;
		dev_info(dev, "vbus regulator not supplied, using internal control\n");
	}

	mti->tdata.init = mt6375_tcpc_init;
	mti->tdata.set_vbus = mt6375_tcpc_set_vbus;
	mti->tcpci = tcpci_register_port(dev, &mti->tdata);
	if (IS_ERR(mti->tcpci))
		return dev_err_probe(dev, PTR_ERR(mti->tcpci),
				     "failed to register tcpci port\n");

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(dev, "failed to get irq\n");
		goto out_unregister_port;
	}
	mti->irq = ret;
	ret = devm_request_threaded_irq(dev, mti->irq, NULL,
					mt6375_tcpc_evt_handler, IRQF_ONESHOT,
					dev_name(dev), mti);
	if (ret < 0) {
		dev_err(dev, "failed to request irq %d\n", mti->irq);
		goto out_unregister_port;
	}

	platform_set_drvdata(pdev, mti);

	return 0;

out_unregister_port:
	tcpci_unregister_port(mti->tcpci);
	return ret;
}

static void mt6375_tcpc_remove(struct platform_device *pdev)
{
	struct mt6375_tcpc_info *mti = platform_get_drvdata(pdev);

	disable_irq(mti->irq);
	tcpci_unregister_port(mti->tcpci);
}

static const struct of_device_id mt6375_tcpc_of_match[] = {
	{ .compatible = "mediatek,mt6375-tcpc" },
	{}
};
MODULE_DEVICE_TABLE(of, mt6375_tcpc_of_match);

static struct platform_driver mt6375_tcpc_driver = {
	.driver = {
		.name = "mt6375-tcpc",
		.owner = THIS_MODULE,
		.of_match_table = mt6375_tcpc_of_match,
	},
	.probe = mt6375_tcpc_probe,
	.remove_new = mt6375_tcpc_remove,
};
module_platform_driver(mt6375_tcpc_driver);

MODULE_AUTHOR("Yuren Chen <ren_chen@richtek.com>");
MODULE_DESCRIPTION("MT6375 USB Type-C Port Controller Interface Driver");
MODULE_LICENSE("GPL");
