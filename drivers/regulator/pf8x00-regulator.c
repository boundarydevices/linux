// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017 NXP
 * Copyright (C) 2019 Boundary Devices
 * Copyright (C) 2020 Amarula Solutions(India)
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

/* registers */
#define PF8X00_DEVICEID			0x00
#define PF8X00_REVID			0x01
#define PF8X00_EMREV			0x02
#define PF8X00_PROGID			0x03
#define PF8X00_IMS_INT			0x04
#define PF8X00_IMS_THERM		0x07
#define PF8X00_SW_MODE_INT		0x0a
#define PF8X00_SW_MODE_MASK		0x0b
#define PF8X00_IMS_SW_ILIM		0x12
#define PF8X00_IMS_LDO_ILIM		0x15
#define PF8X00_IMS_SW_UV		0x18
#define PF8X00_IMS_SW_OV		0x1b
#define PF8X00_IMS_LDO_UV		0x1e
#define PF8X00_IMS_LDO_OV		0x21
#define PF8X00_IMS_PWRON		0x24
#define PF8X00_SYS_INT			0x27
#define PF8X00_HARD_FAULT		0x29
#define PF8X00_FSOB_FLAGS		0x2a
#define PF8X00_FSOB_SELECT		0x2b
#define PF8X00_ABIST_OV1		0x2c
#define PF8X00_ABIST_OV2		0x2d
#define PF8X00_ABIST_UV1		0x2e
#define PF8X00_ABIST_UV2		0x2f
#define PF8X00_TEST_FLAGS		0x30
#define PF8X00_ABIST_RUN		0x31
#define PF8X00_RANDOM_GEN		0x33
#define PF8X00_RANDOM_CHK		0x34
#define PF8X00_VMONEN1			0x35
#define PF8X00_VMONEN2			0x36
#define PF8X00_CTRL1			0x37
#define PF8X00_CTRL2			0x38
#define PF8X00_CTRL3			0x39
#define PF8X00_PWRUP_CTRL		0x3a
#define PF8X00_RESETBMCU		0x3c
#define PF8X00_PGOOD			0x3d
#define PF8X00_PWRDN_DLY1		0x3e
#define PF8X00_PWRDN_DLY2		0x3f
#define PF8X00_FREQ_CTRL		0x40
#define PF8X00_COINCELL_CTRL		0x41
#define PF8X00_PWRON			0x42
#define PF8X00_WD_CONFIG		0x43
#define PF8X00_WD_CLEAR			0x44
#define PF8X00_WD_EXPIRE		0x45
#define PF8X00_WD_COUNTER		0x46
#define PF8X00_FAULT_COUNTER		0x47
#define PF8X00_FSAFE_COUNTER		0x48
#define PF8X00_FAULT_TIMER		0x49
#define PF8X00_AMUX			0x4a
#define PF8X00_SW1_CONFIG1		0x4d
#define PF8X00_LDO1_CONFIG1		0x85
#define PF8X00_VSNVS_CONFIG1		0x9d
#define PF8X00_PAGE_SELECT		0x9f
#define PF8X00_OTP_CTRL3		0xa4
#define PF8X00_NUMREGS			0xe4

/* regulators */
enum pf8x00_regulators {
	PF8X00_LDO1,
	PF8X00_LDO2,
	PF8X00_LDO3,
	PF8X00_LDO4,
	PF8X00_BUCK1,
	PF8X00_BUCK2,
	PF8X00_BUCK3,
	PF8X00_BUCK4,
	PF8X00_BUCK5,
	PF8X00_BUCK6,
	PF8X00_BUCK7,
	PF8X00_VSNVS,

	PF8X00_MAX_REGULATORS,
};

enum pf8x00_buck_states {
	SW_CONFIG1,
	SW_CONFIG2,
	SW_PWRUP,
	SW_MODE1,
	SW_RUN_VOLT,
	SW_STBY_VOLT,
};
#define PF8X00_SW_BASE(i)		(8 * (i - PF8X00_BUCK1) + PF8X00_SW1_CONFIG1)

enum pf8x00_ldo_states {
	LDO_CONFIG1,
	LDO_CONFIG2,
	LDO_PWRUP,
	LDO_RUN_VOLT,
	LDO_STBY_VOLT,
};
#define PF8X00_LDO_BASE(i)		(6 * (i - PF8X00_LDO1) + PF8X00_LDO1_CONFIG1)

enum swxilim_bits {
	SWXILIM_2100_MA,
	SWXILIM_2600_MA,
	SWXILIM_3000_MA,
	SWXILIM_4500_MA,
};
#define PF8X00_SWXSLEW_SHIFT		5
#define PF8X00_SWXSLEW_MASK		BIT(5)
#define PF8X00_SWXILIM_SHIFT		3
#define PF8X00_SWXILIM_MASK		GENMASK(4, 3)
#define PF8X00_SWXPHASE_MASK		GENMASK(2, 0)

enum pf8x00_devid {
	PF8100			= 0x0,
	PF8121A			= BIT(1),
	PF8200			= BIT(3),
};
#define PF8X00_FAM			BIT(6)
#define PF8X00_DEVICE_FAM_MASK		GENMASK(7, 4)
#define PF8X00_DEVICE_ID_MASK		GENMASK(3, 0)

struct pf8x00_regulator_data {
	struct regulator_desc desc;
	unsigned int suspend_enable_reg;
	unsigned int suspend_enable_mask;
	unsigned int suspend_voltage_reg;
	unsigned int suspend_voltage_cache;
	unsigned char hw_en;
	unsigned char vselect_en;
	unsigned char quad_phase;
	unsigned char dual_phase;
	unsigned char fast_slew;
	unsigned int clk_freq;
};

static const struct regmap_config pf8x00_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = PF8X00_NUMREGS - 1,
	.cache_type = REGCACHE_RBTREE,
};

/* VLDOx output: 1.5V to 5.0V */
static const int pf8x00_ldo_voltages[] = {
	1500000, 1600000, 1800000, 1850000, 2150000, 2500000, 2800000, 3000000,
	3100000, 3150000, 3200000, 3300000, 3350000, 4000000, 4900000, 5000000,
};

/* Output: 2.1A to 4.5A */
static const unsigned int pf8x00_sw_current_table[] = {
	2100000, 2600000, 3000000, 4500000,
};

/* Output: 0.4V to 1.8V */
#define PF8XOO_SW1_6_VOLTAGE_NUM 0xB2
static const struct linear_range pf8x00_sw1_to_6_voltages[] = {
	REGULATOR_LINEAR_RANGE(400000, 0x00, 0xB0, 6250),
	REGULATOR_LINEAR_RANGE(1800000, 0xB1, 0xB1, 0),
};

/* Output: 1.0V to 4.1V */
static const int pf8x00_sw7_voltages[] = {
	1000000, 1100000, 1200000, 1250000, 1300000, 1350000, 1500000, 1600000,
	1800000, 1850000, 2000000, 2100000, 2150000, 2250000, 2300000, 2400000,
	2500000, 2800000, 3150000, 3200000, 3250000, 3300000, 3350000, 3400000,
	3500000, 3800000, 4000000, 4100000, 4100000, 4100000, 4100000, 4100000,
};

/* Output: 1.8V, 3.0V, or 3.3V */
static const int pf8x00_vsnvs_voltages[] = {
	0, 1800000, 3000000, 3300000,
};

static short ilim_table[] = {
	2100, 2600, 3000, /* (4500 is selected for anything over 3000) */
};

static int swxilim_select(int ilim)
{
	u8 ilim_sel;

	if (ilim <= 0)
		return -1;
	while (ilim_sel < ARRAY_SIZE(ilim_table)) {
		if (ilim <= ilim_table[ilim_sel])
			break;
		ilim_sel++;
	}
	return ilim_sel;
}

static int pf8x00_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config)
{
	struct pf8x00_regulator_data *rdesc = config->driver_data;
	unsigned char hw_en = 0;
	unsigned char vselect_en = 0;
	unsigned char quad_phase = 0;
	unsigned char dual_phase = 0;
	unsigned fast_slew;
	int ilim_sel;
	int phase;
	unsigned mask = 0;
	int val = 0;
	unsigned char reg = PF8X00_SW_BASE(desc->id) + SW_CONFIG2;
	int ret;

	ret = of_property_read_u32(np, "nxp,ilim-ma", &val);
	if (ret) {
		dev_dbg(config->dev, "unspecified ilim for %s, use value stored in OTP\n",
			desc->name);
	} else {
		dev_warn(config->dev, "nxp,ilim-ma is deprecated, please use regulator-max-microamp\n");
		ilim_sel = swxilim_select(val);
		if (ilim_sel >= 0) {
			mask |= PF8X00_SWXILIM_MASK;
			val |= ilim_sel << PF8X00_SWXILIM_SHIFT;
		}
	}

	ret = of_property_read_u32(np, "nxp,phase-shift", &val);
	if (ret) {
		dev_dbg(config->dev,
			"unspecified phase-shift for %s, using OTP configuration\n",
			desc->name);
	} else if (val < 0 || val > 315 || val % 45 != 0) {
		dev_warn(config->dev,
				"invalid phase_shift %d for %s, using OTP configuration\n",
				val, desc->name);
	} else {
		phase = val / 45;
		if (phase >= 1)
			phase -= 1;
		else
			phase = 7;
		mask |= PF8X00_SWXPHASE_MASK;
		val |= phase;
	}
	fast_slew = ~0;
	ret = of_property_read_u32(np, "fast-slew", &fast_slew);
	if (!ret) {
		if (fast_slew <= 1) {
			mask |= PF8X00_SWXSLEW_MASK;
			val |= fast_slew << PF8X00_SWXSLEW_SHIFT;
		}
	}

	if ((desc->id >= PF8X00_BUCK1) && (desc->id <= PF8X00_BUCK7)) {
		if (mask)
			regmap_update_bits(config->regmap, reg, mask, val);

		if (fast_slew > 1) {
			ret = regmap_read(config->regmap, reg, &fast_slew);
			fast_slew >>= PF8X00_SWXSLEW_SHIFT;
			fast_slew &= 1;
			if (ret < 0)
				fast_slew = 0;
		}
		rdesc->fast_slew = fast_slew;
		dev_dbg(config->dev, "%s:id=%d ilim=%d phase=%d fast_slew=%d\n",
			__func__, desc->id, ilim_sel, phase, fast_slew);
	} else if (mask) {
		dev_warn(config->dev,
			"specifying fast-slew/nxp,phase-shift for %s is ignored\n",
			desc->name);
	}

	if (of_get_property(np, "hw-en", NULL))
		hw_en = 1;
	if (of_get_property(np, "quad-phase", NULL))
		quad_phase = 1;
	if (of_get_property(np, "dual-phase", NULL))
		dual_phase = 1;
	if (of_get_property(np, "vselect-en", NULL))
		vselect_en = 1;

	if ((desc->id != PF8X00_BUCK1) && quad_phase) {
		dev_err(config->dev, "ignoring, only sw1 can use quad-phase\n");
		quad_phase = 0;
	}
	if ((desc->id != PF8X00_BUCK1) && (desc->id != PF8X00_BUCK4)
			 && (desc->id != PF8X00_BUCK5) && dual_phase) {
		dev_err(config->dev, "ignoring, only sw1/sw4/sw5 can use dual-phase\n");
		dual_phase = 0;
	}
	if ((desc->id != PF8X00_LDO2) && vselect_en) {
		/* LDO2 has gpio vselect */
		dev_err(config->dev, "ignoring, only ldo2 can use vselect-en\n");
		vselect_en = 0;
	}
	if ((desc->id != PF8X00_LDO2) && hw_en) {
		/* LDO2 has gpio vselect */
		dev_err(config->dev, "ignoring, only ldo2 can use hw-en\n");
		hw_en = 0;
	}
	rdesc->hw_en = hw_en;
	rdesc->vselect_en = vselect_en;
	rdesc->quad_phase = quad_phase;
	rdesc->dual_phase = dual_phase;
	dev_dbg(config->dev,
		"%s:id=%d hw_en=%d vselect_en=%d quad_phase=%d fast_slew=%d\n"
		" dual_phase=%d\n", __func__, desc->id, hw_en, vselect_en,
		quad_phase, dual_phase, fast_slew);
	return 0;
}

static int pf8x00_suspend_enable(struct regulator_dev *rdev)
{
	struct pf8x00_regulator_data *regl = rdev_get_drvdata(rdev);
	struct regmap *rmap = rdev_get_regmap(rdev);

	return regmap_update_bits(rmap, regl->suspend_enable_reg,
				  regl->suspend_enable_mask,
				  regl->suspend_enable_mask);
}

static int pf8x00_suspend_disable(struct regulator_dev *rdev)
{
	struct pf8x00_regulator_data *regl = rdev_get_drvdata(rdev);
	struct regmap *rmap = rdev_get_regmap(rdev);

	return regmap_update_bits(rmap, regl->suspend_enable_reg,
				  regl->suspend_enable_mask, 0);
}

static int pf8x00_set_suspend_voltage(struct regulator_dev *rdev, int uV)
{
	struct pf8x00_regulator_data *regl = rdev_get_drvdata(rdev);
	int ret;

	if (regl->suspend_voltage_cache == uV)
		return 0;

	ret = regulator_map_voltage_iterate(rdev, uV, uV);
	if (ret < 0) {
		dev_err(rdev_get_dev(rdev), "failed to map %i uV\n", uV);
		return ret;
	}

	dev_dbg(rdev_get_dev(rdev), "uV: %i, reg: 0x%x, msk: 0x%x, val: 0x%x\n",
		uV, regl->suspend_voltage_reg, regl->desc.vsel_mask, ret);
	ret = regmap_update_bits(rdev->regmap, regl->suspend_voltage_reg,
				 regl->desc.vsel_mask, ret);
	if (ret < 0) {
		dev_err(rdev_get_dev(rdev), "failed to set %i uV\n", uV);
		return ret;
	}

	regl->suspend_voltage_cache = uV;

	return 0;
}

struct dvs_ramp {
	unsigned short up_down_slow_fast[4];
};

/* Units uV/uS */
struct dvs_ramp ramp_table[] = {
/* 	up_slow,	down_slow,	up_fast,	down_fast */
[0]  = {{ 7813,		5208,		15625,		10417 }},
[1]  = {{ 8203,		5469,		16406,		10938 }},
[2]  = {{ 8594,		5729,		17188,		11458 }},
[3]  = {{ 8984,		5990,		17969,		11979 }},
[4]  = {{ 9375,		6250,		18750,		12500 }},
[9]  = {{ 6250,		4167,		12500,		 8333 }},
[10] = {{ 6641,		4427,		13281,		 8854 }},
[11] = {{ 7031,		4688,		14063,		 9375 }},
[12] = {{ 7422,		4948,		14844,		 9896 }},
};
static int pf8x00_regulator_set_voltage_time_sel(struct regulator_dev *rdev,
		unsigned int old_sel, unsigned int new_sel)
{
	struct pf8x00_regulator_data *rdesc = container_of(rdev->desc,
			struct pf8x00_regulator_data, desc);
	const unsigned int *volt_table = rdev->desc->volt_table;
	int old_v = volt_table[old_sel];
	int new_v = volt_table[new_sel];
	int change = (new_v - old_v);
	unsigned index;
	unsigned slew;

	index = (rdesc->fast_slew & 1) ? 2 : 0;
	if (change < 0)
		index++;
	slew = ramp_table[rdesc->clk_freq].up_down_slow_fast[index];
	return DIV_ROUND_UP(abs(change), slew);
}

static const struct regulator_ops pf8x00_ldo_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_table,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_suspend_enable = pf8x00_suspend_enable,
	.set_suspend_disable = pf8x00_suspend_disable,
	.set_suspend_voltage = pf8x00_set_suspend_voltage,
};


static const struct regulator_ops pf8x00_buck1_6_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.get_current_limit = regulator_get_current_limit_regmap,
	.set_current_limit = regulator_set_current_limit_regmap,
	.set_suspend_enable = pf8x00_suspend_enable,
	.set_suspend_disable = pf8x00_suspend_disable,
	.set_suspend_voltage = pf8x00_set_suspend_voltage,
};

static const struct regulator_ops pf8x00_buck7_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_ascend,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.get_current_limit = regulator_get_current_limit_regmap,
	.set_current_limit = regulator_set_current_limit_regmap,
	.set_suspend_enable = pf8x00_suspend_enable,
	.set_suspend_disable = pf8x00_suspend_disable,
};

static const struct regulator_ops pf8x00_vsnvs_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_ascend,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = pf8x00_regulator_set_voltage_time_sel,
};

#define PF8X00LDO(_id, _name, base, voltages)			\
	[PF8X00_LDO ## _id] = {					\
		.desc = {					\
			.name = _name,				\
			.of_match = _name,			\
			.regulators_node = "regulators",	\
			.n_voltages = ARRAY_SIZE(voltages),	\
			.ops = &pf8x00_ldo_ops,			\
			.type = REGULATOR_VOLTAGE,		\
			.id = PF8X00_LDO ## _id,		\
			.of_parse_cb = pf8x00_of_parse_cb,	\
			.owner = THIS_MODULE,			\
			.volt_table = voltages,			\
			.vsel_reg = (base) + LDO_RUN_VOLT,	\
			.vsel_mask = 0xff,			\
			.enable_reg = (base) + LDO_CONFIG2,	\
			.enable_val = 0x2,			\
			.disable_val = 0x0,			\
			.enable_mask = 2,			\
		},						\
		.suspend_enable_reg = (base) + LDO_CONFIG2,	\
		.suspend_enable_mask = 1,			\
		.suspend_voltage_reg = (base) + LDO_STBY_VOLT,	\
	}

#define PF8X00BUCK(_id, _name, base, voltages)			\
	[PF8X00_BUCK ## _id] = {				\
		.desc = {					\
			.name = _name,				\
			.of_match = _name,			\
			.regulators_node = "regulators",	\
			.of_parse_cb = pf8x00_of_parse_cb,	\
			.n_voltages = PF8XOO_SW1_6_VOLTAGE_NUM,	\
			.ops = &pf8x00_buck1_6_ops,		\
			.type = REGULATOR_VOLTAGE,		\
			.id = PF8X00_BUCK ## _id,		\
			.owner = THIS_MODULE,			\
			.ramp_delay = 19000,			\
			.linear_ranges = voltages,		\
			.n_linear_ranges = \
				ARRAY_SIZE(voltages),		\
			.vsel_reg = (base) + SW_RUN_VOLT,	\
			.vsel_mask = 0xff,			\
			.curr_table = pf8x00_sw_current_table, \
			.n_current_limits = \
				ARRAY_SIZE(pf8x00_sw_current_table), \
			.csel_reg = (base) + SW_CONFIG2,	\
			.csel_mask = PF8X00_SWXILIM_MASK,	\
			.enable_reg = (base) + SW_MODE1,	\
			.enable_val = 0x3,			\
			.disable_val = 0x0,			\
			.enable_mask = 0x3,			\
			.enable_time = 500,			\
		},						\
		.suspend_enable_reg = (base) + SW_MODE1,	\
		.suspend_enable_mask = 0xc,			\
		.suspend_voltage_reg = (base) + SW_STBY_VOLT,	\
	}

#define PF8X00BUCK7(_name, base, voltages)			\
	[PF8X00_BUCK7] = {				\
		.desc = {					\
			.name = _name,				\
			.of_match = _name,			\
			.regulators_node = "regulators",	\
			.of_parse_cb = pf8x00_of_parse_cb,	\
			.n_voltages = ARRAY_SIZE(voltages),	\
			.ops = &pf8x00_buck7_ops,		\
			.type = REGULATOR_VOLTAGE,		\
			.id = PF8X00_BUCK7,		\
			.owner = THIS_MODULE,			\
			.ramp_delay = 19000,			\
			.volt_table = voltages,			\
			.vsel_reg = (base) + SW_RUN_VOLT,	\
			.vsel_mask = 0xff,			\
			.curr_table = pf8x00_sw_current_table, \
			.n_current_limits = \
				ARRAY_SIZE(pf8x00_sw_current_table), \
			.csel_reg = (base) + SW_CONFIG2,	\
			.csel_mask = PF8X00_SWXILIM_MASK,	\
			.enable_reg = (base) + SW_MODE1,	\
			.enable_val = 0x3,			\
			.disable_val = 0x0,			\
			.enable_mask = 0x3,			\
			.enable_time = 500,			\
		},						\
	}


#define PF8X00VSNVS(_name, base, voltages)			\
	[PF8X00_VSNVS] = {					\
		.desc = {					\
			.name = _name,				\
			.of_match = _name,			\
			.regulators_node = "regulators",	\
			.n_voltages = ARRAY_SIZE(voltages),	\
			.ops = &pf8x00_vsnvs_ops,		\
			.type = REGULATOR_VOLTAGE,		\
			.id = PF8X00_VSNVS,			\
			.of_parse_cb = pf8x00_of_parse_cb,	\
			.owner = THIS_MODULE,			\
			.volt_table = voltages,			\
			.vsel_reg = (base),			\
			.vsel_mask = 0x3,			\
		},						\
	}

static struct pf8x00_regulator_data pf8x00_regs_data[PF8X00_MAX_REGULATORS] = {
	PF8X00LDO(1, "ldo1", PF8X00_LDO_BASE(PF8X00_LDO1), pf8x00_ldo_voltages),
	PF8X00LDO(2, "ldo2", PF8X00_LDO_BASE(PF8X00_LDO2), pf8x00_ldo_voltages),
	PF8X00LDO(3, "ldo3", PF8X00_LDO_BASE(PF8X00_LDO3), pf8x00_ldo_voltages),
	PF8X00LDO(4, "ldo4", PF8X00_LDO_BASE(PF8X00_LDO4), pf8x00_ldo_voltages),
	PF8X00BUCK(1, "buck1", PF8X00_SW_BASE(PF8X00_BUCK1), pf8x00_sw1_to_6_voltages),
	PF8X00BUCK(2, "buck2", PF8X00_SW_BASE(PF8X00_BUCK2), pf8x00_sw1_to_6_voltages),
	PF8X00BUCK(3, "buck3", PF8X00_SW_BASE(PF8X00_BUCK3), pf8x00_sw1_to_6_voltages),
	PF8X00BUCK(4, "buck4", PF8X00_SW_BASE(PF8X00_BUCK4), pf8x00_sw1_to_6_voltages),
	PF8X00BUCK(5, "buck5", PF8X00_SW_BASE(PF8X00_BUCK5), pf8x00_sw1_to_6_voltages),
	PF8X00BUCK(6, "buck6", PF8X00_SW_BASE(PF8X00_BUCK6), pf8x00_sw1_to_6_voltages),
	PF8X00BUCK7("buck7", PF8X00_SW_BASE(PF8X00_BUCK7), pf8x00_sw7_voltages),
	PF8X00VSNVS("vsnvs", PF8X00_VSNVS_CONFIG1, pf8x00_vsnvs_voltages),
};

static int pf8x00_identify(struct device* dev, struct regmap *regmap)
{
	unsigned int value, id1, id2, prog_id;
	u8 dev_fam, dev_id;
	const char *name = NULL;
	int ret;

	ret = regmap_read(regmap, PF8X00_DEVICEID, &value);
	if (ret) {
		dev_err(dev, "failed to read chip family\n");
		return ret;
	}

	dev_fam = value & PF8X00_DEVICE_FAM_MASK;
	switch (dev_fam) {
	case PF8X00_FAM:
		break;
	default:
		dev_err(dev, "Chip 0x%x is not from PF8X00 family\n", dev_fam);
		return ret;
	}

	dev_id = value & PF8X00_DEVICE_ID_MASK;
	switch (dev_id) {
	case PF8100:
		name = "PF8100";
		break;
	case PF8121A:
		name = "PF8121A";
		break;
	case PF8200:
		name = "PF8200";
		break;
	default:
		dev_err(dev, "Unknown pf8x00 device id 0x%x\n", dev_id);
		return -ENODEV;
	}

	dev_info(dev, "%s PMIC found.\n", name);

	ret = regmap_read(regmap, PF8X00_REVID, &value);
	if (ret)
		value = 0;
	ret = regmap_read(regmap, PF8X00_EMREV, &id1);
	if (ret)
		id1 = 0;
	ret = regmap_read(regmap, PF8X00_PROGID, &id2);
	if (ret)
		id2 = 0;
	prog_id = (id1 << 8) | id2;

	dev_info(dev, "%s: Full layer: %x, Metal layer: %x, prog_id=0x%04x\n",
		name, (value & 0xf0) >> 4, value & 0x0f, prog_id);

	return prog_id;
}

struct otp_reg_lookup {
	unsigned short prog_id;
	unsigned char reg;
	unsigned char value;
};

static const struct otp_reg_lookup otp_map[] = {
	{ 0x401c, PF8X00_OTP_CTRL3, 0 },
	{ 0x4008, PF8X00_OTP_CTRL3, 0x04 },
	{ 0x301d, PF8X00_OTP_CTRL3, 0x04 },	/* test only */
	{ 0, 0, 0 },
};

static int get_otp_reg(struct device *dev, unsigned prog_id, unsigned char reg)
{
	const struct otp_reg_lookup *p = otp_map;

	while (p->reg) {
		if ((prog_id == p->prog_id) && (reg == p->reg))
			return p->value;
		p++;
	}

	dev_err(dev, "reg(0x%02x) not found for 0x%04x\n",
		reg, prog_id);
	return -EINVAL;
}

static int pf8x00_i2c_probe(struct i2c_client *client)
{
	struct regulator_config config = { NULL, };
	struct device *dev = &client->dev;
	struct regmap *regmap;
	unsigned hw_en;
	unsigned vselect_en;
	unsigned char quad_phase;
	unsigned char dual_phase;
	unsigned val;
	int ctrl3;
	const char *format = NULL;
	unsigned clk_freq = 0;
	int prog_id;
	int id;
	int ret;

	regmap = devm_regmap_init_i2c(client, &pf8x00_regmap_config);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(dev, "regmap allocation failed with err %d\n", ret);
		return ret;
	}

	prog_id = pf8x00_identify(dev, regmap);
	if (prog_id < 0)
		return prog_id;

	ret = regmap_read(regmap, PF8X00_FREQ_CTRL, &clk_freq);
	clk_freq &= 0xf;
	if (ret < 0)
		clk_freq = 0;
	if (((clk_freq & 7) > 4) || (clk_freq == 8))
		clk_freq = 0;

	for (id = 0; id < ARRAY_SIZE(pf8x00_regs_data); id++) {
		struct pf8x00_regulator_data *data = &pf8x00_regs_data[id];
		struct regulator_dev *rdev;

		data->clk_freq = clk_freq;
		config.dev = dev;
		config.driver_data = data;
		config.regmap = regmap;

		rdev = devm_regulator_register(dev, &data->desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(dev,
				"failed to register %s regulator\n", data->desc.name);
			return PTR_ERR(rdev);
		}
	}

	hw_en = pf8x00_regs_data[PF8X00_LDO2].hw_en;
	vselect_en = pf8x00_regs_data[PF8X00_LDO2].vselect_en;
	val = vselect_en ? 8 : 0;
	if (hw_en)
		val |= 0x10;
	ret = regmap_update_bits(regmap,
			PF8X00_LDO_BASE(PF8X00_LDO2) + LDO_CONFIG2,
				 0x18, val);

	ctrl3 = get_otp_reg(dev, prog_id, PF8X00_OTP_CTRL3);
	if (ctrl3 >= 0) {
		quad_phase = pf8x00_regs_data[PF8X00_BUCK1].quad_phase;
		dual_phase = pf8x00_regs_data[PF8X00_BUCK1].dual_phase;
		if (quad_phase) {
			if ((ctrl3 & 3) != 2)
				format = "sw1 quad_phase not set in otp_ctrl3 %x\n";
		} else if (dual_phase) {
			if ((ctrl3 & 3) != 1)
				format = "sw1 dual_phase not set in otp_ctrl3 %x\n";
		} else if (ctrl3 & 3) {
			format = "sw1 single_phase not set in otp_ctrl3 %x\n";
		}
		if (!quad_phase) {
			dual_phase = pf8x00_regs_data[PF8X00_BUCK4].dual_phase;
			if (dual_phase) {
				if ((ctrl3 & 0x0c) != 4)
					format = "sw4 dual_phase not set in otp_ctrl3 %x\n";
			} else if (ctrl3 & 0x0c) {
				format = "sw4 single_phase not set in otp_ctrl3 %x\n";
			}
		}
		dual_phase = pf8x00_regs_data[PF8X00_BUCK5].dual_phase;
		if (dual_phase) {
			if ((ctrl3 & 0x30) != 0x10)
				format = "sw5 dual_phase not set in otp_ctrl3 %x\n";
		} else if (ctrl3 & 0x30) {
			format = "sw5 single_phase not set in otp_ctrl3 %x\n";
		}
		if (format) {
			dev_err(dev, format, ctrl3);
			dev_err(dev, "!!!try updating u-boot, boot.scr, or pmic\n");
		}
	}
	return 0;
}

static const struct of_device_id pf8x00_dt_ids[] = {
	{ .compatible = "nxp,pf8100",},
	{ .compatible = "nxp,pf8121a",},
	{ .compatible = "nxp,pf8200",},
	{ }
};
MODULE_DEVICE_TABLE(of, pf8x00_dt_ids);

static const struct i2c_device_id pf8x00_i2c_id[] = {
	{ "pf8100", 0 },
	{ "pf8121a", 0 },
	{ "pf8200", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, pf8x00_i2c_id);

static struct i2c_driver pf8x00_regulator_driver = {
	.id_table = pf8x00_i2c_id,
	.driver = {
		.name = "pf8x00",
		.of_match_table = pf8x00_dt_ids,
	},
	.probe_new = pf8x00_i2c_probe,
};
module_i2c_driver(pf8x00_regulator_driver);

MODULE_AUTHOR("Jagan Teki <jagan@amarulasolutions.com>");
MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("Regulator Driver for NXP's PF8100/PF8121A/PF8200 PMIC");
MODULE_LICENSE("GPL v2");
