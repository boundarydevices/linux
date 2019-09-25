// SPDX-License-Identifier: GPL-2.0+
//
// Copyright (C) 2011-2013 Freescale Semiconductor, Inc. All Rights Reserved.
// Copyright 2017 NXP
// Copyright 2019 Boundary Devices

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/of_regulator.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/regmap.h>

#define PF8X00_DEVICEID		0x00
#define PF8X00_REVID		0x01
#define PF8X00_EMREV		0x02
#define PF8X00_PROGID		0x03

#define PF8X00_IMS_INT		0x04
#define PF8X00_IMS_THERM	0x07
#define PF8X00_SW_MODE_INT	0x0a
#define PF8X00_SW_MODE_MASK	0x0b

#define IMS_INT		 	0
#define IMS_STATUS	 	0
#define IMS_MASK		1
#define IMS_SENSE		2

#define PF8X00_IMS_SW_ILIM	0x12
#define PF8X00_IMS_LDO_ILIM	0x15
#define PF8X00_IMS_SW_UV	0x18
#define PF8X00_IMS_SW_OV	0x1b
#define PF8X00_IMS_LDO_UV	0x1e
#define PF8X00_IMS_LDO_OV	0x21
#define PF8X00_IMS_PWRON	0x24
#define PF8X00_SYS_INT		0x27

#define PF8X00_HARD_FAULT	0x29
#define PF8X00_FSOB_FLAGS	0x2a
#define PF8X00_FSOB_SELECT	0x2b
#define PF8X00_ABIST_OV1	0x2c
#define PF8X00_ABIST_OV2	0x2d
#define PF8X00_ABIST_UV1	0x2e
#define PF8X00_ABIST_UV2	0x2f
#define PF8X00_TEST_FLAGS	0x30
#define PF8X00_ABIST_RUN	0x31

#define PF8X00_RANDOM_GEN	0x33
#define PF8X00_RANDOM_CHK	0x34
#define PF8X00_VMONEN1		0x35
#define PF8X00_VMONEN2		0x36
#define PF8X00_CTRL1		0x37
#define PF8X00_CTRL2		0x38
#define PF8X00_CTRL3		0x39
#define PF8X00_PWRUP_CTRL	0x3a

#define PF8X00_RESETBMCU	0x3c
#define PF8X00_PGOOD		0x3d
#define PF8X00_PWRDN_DLY1	0x3e
#define PF8X00_PWRDN_DLY2	0x3f
#define PF8X00_FREQ_CTRL	0x40
#define PF8X00_COINCELL_CTRL	0x41
#define PF8X00_PWRON		0x42
#define PF8X00_WD_CONFIG	0x43
#define PF8X00_WD_CLEAR		0x44
#define PF8X00_WD_EXPIRE	0x45
#define PF8X00_WD_COUNTER	0x46
#define PF8X00_FAULT_COUNTER	0x47
#define PF8X00_FSAFE_COUNTER	0x48
#define PF8X00_FAULT_TIMER	0x49
#define PF8X00_AMUX		0x4a

#define SW_CONFIG1	0
#define SW_CONFIG2	1
#define SW_PWRUP	2
#define SW_MODE1	3
#define SW_RUN_VOLT	4
#define SW_STBY_VOLT	5

/* i is in REG_SW1..REG_SW7 */
#define PF8X00_SW(i)		(8 * (i - REG_SW1) + 0x4d)

#define LDO_CONFIG1	0
#define LDO_CONFIG2	1
#define LDO_PWRUP	2
#define LDO_RUN_VOLT	3
#define LDO_STBY_VOLT	4

/* i is in REG_LDO1..REG_LDO4 */
#define PF8X00_LDO(i)		(6 * (i - REG_LDO1) + 0x85)

#define PF8X00_VSNVS_CONFIG1	0x9d

#define PF8X00_PAGE_SELECT	0x9f
#define PF8X00_OTP_FSOB_SELECT	0xa0
#define PF8X00_OTP_I2C		0xa1
#define PF8X00_OTP_CTRL1	0xa2
#define PF8X00_OTP_CTRL2	0xa3
#define PF8X00_OTP_CTRL3	0xa4

#define PF8X00_OTP_FREQ_CTRL	0xa5
#define PF8X00_OTP_COINCELL	0xa6
#define PF8X00_OTP_PWRON	0xa7
#define PF8X00_OTP_WD_CONFIG	0xa8
#define PF8X00_OTP_WD_EXPIRE	0xa9
#define PF8X00_OTP_WD_COUNTER	0xaa
#define PF8X00_OTP_FAULT_COUNTER 0xab
#define PF8X00_OTP_FAULT_TIMER	0xac
#define PF8X00_OTP_PWRDN_DLY1	0xad
#define PF8X00_OTP_PWRDN_DLY2	0xae
#define PF8X00_OTP_PWRUP_CTRL	0xaf
#define PF8X00_OTP_RESETBMCU	0xb0
#define PF8X00_OTP_PGOOD	0xb1

#define OTP_SW_VOLT	0
#define OTP_SW_PWRUP	1
#define OTP_SW_CONFIG1	2
#define OTP_SW_CONFIG2	3

/* i is in REG_SW1..REG_SW7 */
#define PF8X00_OTP_SW(i)	(4 * (i - REG_SW1) + 0xb2)

#define OTP_LDO_VOLT	0
#define OTP_LDO_PWRUP	1
#define OTP_LDO_CONFIG	2

/* i is in REG_LDO1..REG_LDO4 */
#define PF8X00_OTP_LDO(i)	(3 * (i - REG_LDO1) + 0xce)

#define PF8X00_OTP_VSNVS_CONFIG	0xda
#define PF8X00_OTP_OV_BYPASS1	0xdb
#define PF8X00_OTP_OV_BYPASS2	0xdc
#define PF8X00_OTP_UV_BYPASS1	0xdd
#define PF8X00_OTP_UV_BYPASS2	0xde
#define PF8X00_OTP_ILIM_BYPASS1	0xdf
#define PF8X00_OTP_ILIM_BYPASS2	0xe0

#define PF8X00_OTP_DEBUG1	0xe3
#define PF8X_NUMREGS		0xe4

#define REG_LDO1		0
#define REG_LDO2		1
#define REG_LDO3		2
#define REG_LDO4		3
#define REG_SW1			4
#define REG_SW2			5
#define REG_SW3			6
#define REG_SW4			7
#define REG_SW5			8
#define REG_SW6			9
#define REG_SW7			10
#define REG_VSNVS		11
#define REG_NUM_REGULATORS	(4 + 7 + 1)

enum chips {
	PF8100 = 0x40,
	PF8121A = 0x42,
	PF8200 = 0x48,
};

struct id_name {
	enum chips id;
	const char *name;
};

struct pf8x_regulator {
	struct regulator_desc desc;
	unsigned char stby_reg;
	unsigned char stby_mask;
	int ilim;
	int phase;
	unsigned char hw_en;
	unsigned char vselect_en;
	unsigned char quad_phase;
	unsigned char dual_phase;
};

struct pf8x_chip {
	int	chip_id;
	int	prog_id;
	struct regmap *regmap;
	struct device *dev;
	struct pf8x_regulator regulator_descs[REG_NUM_REGULATORS];
	struct regulator_dev *regulators[REG_NUM_REGULATORS];
};

/* Output: 1.5V to 5.0V, LDO2 can use VSELECT */
static const int pf8x00_ldo_voltages[] = {
	1500000, 1600000, 1800000, 1850000, 2150000, 2500000, 2800000, 3000000,
	3100000, 3150000, 3200000, 3300000, 3350000, 4000000, 4900000, 5000000,
};

#define SWV(i)	(6250 * i + 400000)
#define SWV_LINE(i)	SWV(i*8+0), SWV(i*8+1), SWV(i*8+2), SWV(i*8+3), \
			SWV(i*8+4), SWV(i*8+5), SWV(i*8+6), SWV(i*8+7)

/* Output: 0.4V to 1.8V */
static const int pf8x00_sw1_to_6_voltages[] = {
	SWV_LINE(0),
	SWV_LINE(1),
	SWV_LINE(2),
	SWV_LINE(3),
	SWV_LINE(4),
	SWV_LINE(5),
	SWV_LINE(6),
	SWV_LINE(7),
	SWV_LINE(8),
	SWV_LINE(9),
	SWV_LINE(10),
	SWV_LINE(11),
	SWV_LINE(12),
	SWV_LINE(13),
	SWV_LINE(14),
	SWV_LINE(15),
	SWV_LINE(16),
	SWV_LINE(17),
	SWV_LINE(18),
	SWV_LINE(19),
	SWV_LINE(20),
	SWV_LINE(21),
	1500000, 1800000,
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

static const struct i2c_device_id pf8x_device_id[] = {
	{.name = "pf8x00",},
	{ }
};
MODULE_DEVICE_TABLE(i2c, pf8x_device_id);

static const struct of_device_id pf8x_dt_ids[] = {
	{ .compatible = "nxp,pf8x00",},
	{ }
};
MODULE_DEVICE_TABLE(of, pf8x_dt_ids);

const struct id_name id_list[] = {
	{PF8100, "PF8100"},
	{PF8121A, "PF8121A"},
	{PF8200, "PF8200"},
	{0, "???"},
};

const struct id_name *get_id_name(enum chips id)
{
	const struct id_name *p = id_list;

	while (p->id) {
		if (p->id == id)
			break;
		p++;
	}
	return p;
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
	struct pf8x_chip *pf = rdev_get_drvdata(rdev);
	const unsigned int *volt_table = rdev->desc->volt_table;
	int old_v = volt_table[old_sel];
	int new_v = volt_table[new_sel];
	unsigned change = (new_v - old_v);
	unsigned clk_index = 0;
	unsigned index;
	unsigned fast = 0;
	unsigned slew;
	int ret;

	ret = regmap_read(pf->regmap, rdev->desc->enable_reg +
			SW_CONFIG2 - SW_MODE1, &fast);
	fast &= 0x20;
	if (ret < 0)
		fast = 0;
	ret = regmap_read(pf->regmap, PF8X00_FREQ_CTRL, &index);
	index &= 0xf;
	if (ret < 0)
		index = 0;
	if (((index & 7) > 4) || (index == 8))
		index = 0;

	index = fast ? 2 : 0;
	if (change < 0) {
		change = -change;
		index++;
	}
	slew = ramp_table[clk_index].up_down_slow_fast[index];
	return DIV_ROUND_UP(change, slew);
}

static short ilim_table[] = {
	2100, 2600, 3000, 	/* 4500, */
};

static int encode_ilim(struct pf8x_chip *pf, int ilim)
{
	int i = 0;

	if (ilim <= 0)
		return -1;
	while (i < ARRAY_SIZE(ilim_table)) {
		if (ilim <= ilim_table[i])
			break;
		i++;
	}
	return i;
}

static int encode_phase(struct pf8x_chip *pf, int phase)
{
	int ph;
	if (phase < 0)
		return -1;
	ph = phase / 45;
	if ((ph * 45) != phase) {
		dev_err(pf->dev, "ignoring, illegal phase %d\n", phase);
		return -1;
	}
	return (ph >= 1) ? ph - 1: 7;
}

static int pf8x00_of_parse_cb(struct device_node *np,
		const struct regulator_desc *desc,
		struct regulator_config *config)
{
	struct pf8x_chip *pf = config->driver_data;
	struct pf8x_regulator *rdesc = &pf->regulator_descs[desc->id];
	unsigned char hw_en = 0;
	unsigned char vselect_en = 0;
	unsigned char quad_phase = 0;
	unsigned char dual_phase = 0;
	int ilim;
	int phase;
	int ret;

	ret = of_property_read_u32(np, "ilim-ma",
			&ilim);
	if (ret)
		ilim = -1;
	ret = of_property_read_u32(np, "phase",
			&phase);
	if (ret)
		phase = -1;
	ilim = encode_ilim(pf, ilim);
	phase = encode_phase(pf, phase);

	if (of_get_property(np, "hw-en", NULL))
		hw_en = 1;
	if (of_get_property(np, "quad-phase", NULL))
		quad_phase = 1;
	if (of_get_property(np, "dual-phase", NULL))
		dual_phase = 1;
	if (of_get_property(np, "vselect-en", NULL))
		vselect_en = 1;

	if ((desc->id != REG_SW1) && quad_phase) {
		dev_err(pf->dev, "ignoring, only sw1 can use quad-phase\n");
		quad_phase = 0;
	}
	if ((desc->id != REG_SW1) && (desc->id != REG_SW4)
			 && (desc->id != REG_SW5) && dual_phase) {
		dev_err(pf->dev, "ignoring, only sw1/sw4/sw5 can use dual-phase\n");
		dual_phase = 0;
	}
	if ((desc->id != REG_LDO2) && vselect_en) {
		/* LDO2 has gpio vselect */
		dev_err(pf->dev, "ignoring, only ldo2 can use vselect-en\n");
		vselect_en = 0;
	}
	if ((desc->id != REG_LDO2) && hw_en) {
		/* LDO2 has gpio vselect */
		dev_err(pf->dev, "ignoring, only ldo2 can use hw-en\n");
		hw_en = 0;
	}
	if ((desc->id < REG_SW1) && (desc->id > REG_SW7)) {
		if ((unsigned)ilim <= 3) {
			dev_err(pf->dev, "ignoring, only sw1-7 can use ilim\n");
			ilim = -1;
		}
		if ((unsigned)phase <= 7) {
			dev_err(pf->dev, "ignoring, only sw1-7 can use phase\n");
			ilim = -1;
		}
	}
	rdesc->ilim = ilim;
	rdesc->phase = phase;
	rdesc->hw_en = hw_en;
	rdesc->vselect_en = vselect_en;
	rdesc->quad_phase = quad_phase;
	rdesc->dual_phase = dual_phase;
	pr_debug("%s:id=%d ilim=%d, phase=%d, hw_en=%d vselect_en=%d"
		" quad_phase=%d dual_phase=%d\n",
		__func__, desc->id, ilim, phase, hw_en, vselect_en,
		quad_phase, dual_phase);
	return 0;
}

static struct regulator_ops pf8x00_ldo_regulator_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_table,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
};

static struct regulator_ops pf8x00_sw_regulator_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_table,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = pf8x00_regulator_set_voltage_time_sel,
};

static struct regulator_ops pf8x00_vsnvs_regulator_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_ascend,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
};

#define STRUCT_LDO_REG(_id, _name, base, voltages)		\
	[_id] = {						\
		.desc = {					\
			.name = #_name,				\
			.of_match = of_match_ptr(#_name),	\
			.regulators_node = of_match_ptr("regulators"), \
			.of_parse_cb = pf8x00_of_parse_cb,	\
			.n_voltages = ARRAY_SIZE(voltages),	\
			.ops = &pf8x00_ldo_regulator_ops,	\
			.type = REGULATOR_VOLTAGE,		\
			.id = _id,				\
			.owner = THIS_MODULE,			\
			.volt_table = voltages,			\
			.vsel_reg = (base) + LDO_RUN_VOLT,	\
			.vsel_mask = 0xff,			\
			.enable_reg = (base) + LDO_CONFIG2,	\
			.enable_val = 0x2,			\
			.disable_val = 0x0,			\
			.enable_mask = 2,			\
		},						\
		.stby_reg = (base) + LDO_STBY_VOLT,		\
		.stby_mask = 0x20,				\
	}

#define STRUCT_SW_REG(_id, _name, base, voltages)		\
	[_id] = {						\
		.desc = {					\
			.name = #_name,				\
			.of_match = of_match_ptr(#_name),	\
			.regulators_node = of_match_ptr("regulators"), \
			.of_parse_cb = pf8x00_of_parse_cb,	\
			.n_voltages = ARRAY_SIZE(voltages),	\
			.ops = &pf8x00_sw_regulator_ops,	\
			.type = REGULATOR_VOLTAGE,		\
			.id = _id,				\
			.owner = THIS_MODULE,			\
			.volt_table = voltages,			\
			.vsel_reg = (base) + SW_RUN_VOLT,	\
			.vsel_mask = 0xff,			\
			.enable_reg = (base) + SW_MODE1,	\
			.enable_val = 0x3,			\
			.disable_val = 0x0,			\
			.enable_mask = 0x3,			\
			.enable_time = 500,			\
		},						\
		.stby_reg = (base) + SW_STBY_VOLT,		\
		.stby_mask = 0xff,				\
	}


#define STRUCT_VSNVS_REG(_id, _name, base, voltages)		\
	[_id] = {						\
		.desc = {					\
			.name = #_name,				\
			.of_match = of_match_ptr(#_name),	\
			.regulators_node = of_match_ptr("regulators"), \
			.of_parse_cb = pf8x00_of_parse_cb,	\
			.n_voltages = ARRAY_SIZE(voltages),	\
			.ops = &pf8x00_vsnvs_regulator_ops,	\
			.type = REGULATOR_VOLTAGE,		\
			.id = _id,				\
			.owner = THIS_MODULE,			\
			.volt_table = voltages,			\
			.vsel_reg = (base),			\
			.vsel_mask = 0x3,			\
		},						\
	}

static const struct pf8x_regulator pf8x00_regulators[] = {
	STRUCT_LDO_REG(REG_LDO1, ldo1, PF8X00_LDO(REG_LDO1), pf8x00_ldo_voltages),
	STRUCT_LDO_REG(REG_LDO2, ldo2, PF8X00_LDO(REG_LDO2), pf8x00_ldo_voltages),
	STRUCT_LDO_REG(REG_LDO3, ldo3, PF8X00_LDO(REG_LDO3), pf8x00_ldo_voltages),
	STRUCT_LDO_REG(REG_LDO4, ldo4, PF8X00_LDO(REG_LDO4), pf8x00_ldo_voltages),
	STRUCT_SW_REG(REG_SW1, sw1, PF8X00_SW(REG_SW1), pf8x00_sw1_to_6_voltages),
	STRUCT_SW_REG(REG_SW2, sw2, PF8X00_SW(REG_SW2), pf8x00_sw1_to_6_voltages),
	STRUCT_SW_REG(REG_SW3, sw3, PF8X00_SW(REG_SW3), pf8x00_sw1_to_6_voltages),
	STRUCT_SW_REG(REG_SW4, sw4, PF8X00_SW(REG_SW4), pf8x00_sw1_to_6_voltages),
	STRUCT_SW_REG(REG_SW5, sw5, PF8X00_SW(REG_SW5), pf8x00_sw1_to_6_voltages),
	STRUCT_SW_REG(REG_SW6, sw6, PF8X00_SW(REG_SW6), pf8x00_sw1_to_6_voltages),
	STRUCT_SW_REG(REG_SW7, sw7, PF8X00_SW(REG_SW7), pf8x00_sw7_voltages),
	STRUCT_VSNVS_REG(REG_VSNVS, vsnvs, PF8X00_VSNVS_CONFIG1, pf8x00_vsnvs_voltages),
};

#ifdef CONFIG_OF
static struct of_regulator_match pf8x00_matches[] = {
	{ .name = "ldo1",	},
	{ .name = "ldo2",	},
	{ .name = "ldo3",	},
	{ .name = "ldo4",	},
	{ .name = "sw1",	},
	{ .name = "sw2",	},
	{ .name = "sw3",	},
	{ .name = "sw4",	},
	{ .name = "sw5",	},
	{ .name = "sw6",	},
	{ .name = "sw7",	},
	{ .name = "vsnvs",	},
};

static int pf8x_parse_regulators_dt(struct pf8x_chip *pf)
{
	struct device *dev = pf->dev;
	struct device_node *np, *parent;
	int ret;

	np = of_node_get(dev->of_node);
	if (!np)
		return -EINVAL;

	parent = of_get_child_by_name(np, "regulators");
	if (!parent) {
		dev_err(dev, "regulators node not found\n");
		return -EINVAL;
	}

	ret = of_regulator_match(dev, parent, pf8x00_matches,
				 ARRAY_SIZE(pf8x00_matches));

	of_node_put(parent);
	if (ret < 0) {
		dev_err(dev, "Error parsing regulator init data: %d\n",
			ret);
		return ret;
	}

	return 0;
}

static inline struct regulator_init_data *match_init_data(int index)
{
	return pf8x00_matches[index].init_data;
}

static inline struct device_node *match_of_node(int index)
{
	return pf8x00_matches[index].of_node;
}
#else
static int pf8x_parse_regulators_dt(struct pf8x_chip *pf)
{
	return 0;
}

static inline struct regulator_init_data *match_init_data(int index)
{
	return NULL;
}

static inline struct device_node *match_of_node(int index)
{
	return NULL;
}
#endif

static int pf8x_identify(struct pf8x_chip *pf)
{
	const struct id_name *p;
	unsigned int value, id1, id2;
	int ret;

	ret = regmap_read(pf->regmap, PF8X00_DEVICEID, &value);
	if (ret)
		return ret;

	pf->chip_id = value;
	p = get_id_name(value);
	if (p->id != value) {
		dev_warn(pf->dev, "Illegal ID: %x\n", value);
		return -ENODEV;
	}

	ret = regmap_read(pf->regmap, PF8X00_REVID, &value);
	if (ret)
		value = 0;
	ret = regmap_read(pf->regmap, PF8X00_EMREV, &id1);
	if (ret)
		id1 = 0;
	ret = regmap_read(pf->regmap, PF8X00_PROGID, &id2);
	if (ret)
		id2 = 0;
	pf->prog_id = (id1 << 8) | id2;

	dev_info(pf->dev, "%s: Full layer: %x, Metal layer: %x, prog_id=0x%04x\n",
		 p->name, (value & 0xf0) >> 4, value & 0x0f, pf->prog_id);

	return 0;
}

static const struct regmap_config pf8x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = PF8X_NUMREGS - 1,
	.cache_type = REGCACHE_RBTREE,
};

struct otp_reg_lookup {
	unsigned short prog_id;
	unsigned char reg;
	unsigned char value;
};

static const struct otp_reg_lookup otp_map[] = {
	{ 0x401c, PF8X00_OTP_CTRL3, 0 },
	{ 0x4008, PF8X00_OTP_CTRL3, 0x04 },
	{ 0, 0, 0 },
};

static int get_otp_reg(struct pf8x_chip *pf, unsigned char reg)
{
	const struct otp_reg_lookup *p = otp_map;

	while (p->reg) {
		if ((pf->prog_id == p->prog_id) && (reg == p->reg))
			return p->value;
		p++;
	}

	dev_err(pf->dev, "reg(0x%02x) not found for 0x%04x\n",
		 reg, pf->prog_id);
	return -EINVAL;
}

static int pf8x00_regulator_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct pf8x_chip *pf;
#if 0
	struct pf8x_regulator_platform_data *pdata =
	    dev_get_platdata(&client->dev);
#endif
	struct regulator_config config = { };
	int i, ret;
	u32 num_regulators;
	unsigned hw_en;
	unsigned vselect_en;
	unsigned char quad_phase;
	unsigned char dual_phase;
	unsigned val;
	int ctrl3;
	const char *format = NULL;

	pf = devm_kzalloc(&client->dev, sizeof(*pf),
			GFP_KERNEL);
	if (!pf)
		return -ENOMEM;

	i2c_set_clientdata(client, pf);
	pf->dev = &client->dev;

	pf->regmap = devm_regmap_init_i2c(client, &pf8x_regmap_config);
	if (IS_ERR(pf->regmap)) {
		ret = PTR_ERR(pf->regmap);
		dev_err(&client->dev,
			"regmap allocation failed with err %d\n", ret);
		return ret;
	}

	ret = pf8x_identify(pf);
	if (ret)
		return ret;

	dev_info(&client->dev, "%s found.\n",
		get_id_name(pf->chip_id)->name);

	memcpy(pf->regulator_descs, pf8x00_regulators,
		sizeof(pf->regulator_descs));

	ret = pf8x_parse_regulators_dt(pf);
	if (ret)
		return ret;

	num_regulators = ARRAY_SIZE(pf->regulator_descs);
	for (i = 0; i < num_regulators; i++) {
		struct regulator_init_data *init_data;
		struct regulator_desc *desc;

		desc = &pf->regulator_descs[i].desc;
#if 0
		if (pdata)
			init_data = pdata->init_data[i];
		else
#endif
			init_data = match_init_data(i);

		config.dev = &client->dev;
		config.init_data = init_data;
		config.driver_data = pf;
		config.of_node = match_of_node(i);
		config.ena_gpiod = NULL;

		pf->regulators[i] =
			devm_regulator_register(&client->dev, desc, &config);
		if (IS_ERR(pf->regulators[i])) {
			dev_err(&client->dev, "register regulator%s failed\n",
				desc->name);
			return PTR_ERR(pf->regulators[i]);
		}
		if ((i >= REG_SW1) && (i <= REG_SW7)) {
			unsigned phase = pf->regulator_descs[i].phase;
			unsigned ilim = pf->regulator_descs[i].ilim;
			unsigned mask = 0;
			unsigned val = 0;
			unsigned reg = PF8X00_SW(i) + SW_CONFIG2;

			if (phase <= 7) {
				mask |= 7;
				val |= phase;
			}
			if (ilim <= 3) {
				mask |= 3 << 3;
				val |= ilim << 3;
			}
			if (mask) {
				pr_debug("%s:reg=0x%x, mask=0x%x, val=0x%x\n",
					__func__, reg, mask, val);
				ret = regmap_update_bits(pf->regmap, reg, mask,
						val);
			}
		}
	}
	hw_en = pf->regulator_descs[REG_LDO2].hw_en;
	vselect_en = pf->regulator_descs[REG_LDO2].vselect_en;
	val = vselect_en ? 8 : 0;
	if (hw_en)
		val |= 0x10;
	ret = regmap_update_bits(pf->regmap,
			PF8X00_LDO(REG_LDO2) + LDO_CONFIG2,
				 0x18, val);

	ctrl3 = get_otp_reg(pf, PF8X00_OTP_CTRL3);
	if (ctrl3 >= 0) {
		quad_phase = pf->regulator_descs[REG_SW1].quad_phase;
		dual_phase = pf->regulator_descs[REG_SW1].dual_phase;
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
			dual_phase = pf->regulator_descs[REG_SW4].dual_phase;
			if (dual_phase) {
				if ((ctrl3 & 0x0c) != 4)
					format = "sw4 dual_phase not set in otp_ctrl3 %x\n";
			} else if (ctrl3 & 0x0c) {
				format = "sw4 single_phase not set in otp_ctrl3 %x\n";
			}
		}
		dual_phase = pf->regulator_descs[REG_SW5].dual_phase;
		if (dual_phase) {
			if ((ctrl3 & 0x30) != 0x10)
				format = "sw5 dual_phase not set in otp_ctrl3 %x\n";
		} else if (ctrl3 & 0x30) {
			format = "sw5 single_phase not set in otp_ctrl3 %x\n";
		}
		if (format) {
			dev_err(pf->dev, format, ctrl3);
			dev_err(pf->dev, "!!!try updating u-boot, boot.scr, or pmic\n");
		}
	}
	return 0;
}

static int pf8x_suspend(struct device *dev)
{
#if 0
	struct pf8x_chip *pf = i2c_get_clientdata(to_i2c_client(dev));
#endif
	return 0;
}

static int pf8x_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops pf8x_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pf8x_suspend, pf8x_resume)
};

static struct i2c_driver pf8x_driver = {
	.id_table = pf8x_device_id,
	.driver = {
		.name = "pf8x00-regulator",
		.of_match_table = pf8x_dt_ids,
		.pm = &pf8x_pm_ops,
	},
	.probe = pf8x00_regulator_probe,
};
module_i2c_driver(pf8x_driver);

MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("Regulator Driver for NXP's PF8100/PF8200 PMIC");
MODULE_LICENSE("GPL v2");
