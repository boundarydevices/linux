// SPDX-License-Identifier: GPL-2.0
//
// tac5x1x.c
//
// Copyright (C) 2022 - 2025 Texas Instruments Incorporated
//
// Author: Kevin Lu <kevin-lu@ti.com>
// Author: Kokila Karuppusamy <kokila.karuppusamy@ti.com>
// Author: Niranjan H Y <niranjan.hy@ti.com>
//

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/regulator/consumer.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/regmap.h>
#include <linux/device.h>
#include <sound/tlv.h>

#include "tac5x1x.h"

struct tac5x1x_setup_gpio {
	s32 gpio_func[3];
	s32 gpio_drive[3];
	s32 gpi1_func;
	s32 gpa_gpio;
};

struct tac5x1x_input_diag_config {
	s32 in_ch_en;
	s32 out_ch_en;
	s32 incl_se_inm;
	s32 incl_ac_coup;
};

struct tac5x1x_irqinfo {
	struct delayed_work irq_work;
	s32 irq_gpio;
	s32 irq;
	bool irq_enable;
};

struct tac5x1x_dev {
	u32 addr;
	bool is_loading;
};

struct interrupt_info {
	const char * const *names;
	size_t size;
};

struct tac5x1x_priv {
	struct snd_soc_component *component;
	struct regmap *regmap;
	struct device *dev;
	enum tac5x1x_type codec_type;
	bool pll_setup;
	s32 irq;
	s32 vref_vg;
	s32 adc_en;
	s32 dac_en;
	s32 micbias_en;
	s32 micbias_vg;
	s32 agc_en;
	s32 uad_en;
	s32 vad_en;
	s32 uag_en;
	s32 dac_mute;
	s32 dac_dig_vol[4];
	s32 dac_chns;
	u32 sysclk;
	s32 ndev;
	s32 micbias_threshold[2];
	s32 gpa_threshold[2];
	u8 dev_name[20];
	struct snd_soc_jack *jack;
	struct regulator *supply_avdd;
	struct regulator *supply_iovdd;
	struct tac5x1x_setup_gpio *gpio_setup;
	struct tac5x1x_irqinfo irqinfo;
	/* mutex to prevent simulataneous device read */
	struct mutex dev_lock;
	struct tac5x1x_dev tac5x1x_dev[2];
	struct tac5x1x_input_diag_config input_diag_config;
	void (*irq_work_func)(struct tac5x1x_priv *tac5x1x);
};

static const char * const int_ltch0[] = {
	"Clock Error",
	"PLL Lock",
	"Boost Over Temperature",
	"Boost Over Current",
	"Boost Mode",
	"Reserved",
	"Reserved",
	"Reserved",
};

static const char * const chx_ltch[] = {
	"Input Channel1 fault",
	"Input Channel2 fault",
	"Output Channel1 fault",
	"Output Channel2 fault",
	"Short to VBAT_IN",
	"Reserved",
	"Reserved",
	"Reserved",
};

static const char * const in_ch1_ltch[] = {
	"IN_CH1 open Input",
	"IN_CH1 Input shorted",
	"IN_CH1 INP shorted to GND",
	"IN_CH1 INM shorted to GND",
	"IN_CH1 INP shorted to MICBIAS",
	"IN_CH1 INM shorted to MICBIAS",
	"IN_CH1 INP shorted to VBAT_IN",
	"IN_CH1 INM shorted to VBAT_IN",
};

static const char * const in_ch2_ltch[] = {
	"IN_CH2 open Input",
	"IN_CH2 Input shorted",
	"IN_CH2 INP shorted to GND",
	"IN_CH2 INM shorted to GND",
	"IN_CH2 INP shorted to MICBIAS",
	"IN_CH2 INM shorted to MICBIAS",
	"IN_CH2 INP shorted to VBAT_IN",
	"IN_CH2 INM shorted to VBAT_IN",
};

static const char * const out_ch1_ltch[] = {
	"OUT_CH1 OUT1P Short circuit Fault",
	"OUT_CH1 OUT1M Short circuit Fault",
	"OUT_CH1 DRVRP Virtual Ground Fault",
	"OUT_CH1 DRVRM Virtual ground Fault",
	"OUT_CH1 ADC CH1 Mask",
	"OUT_CH1 ADC CH2 MASK",
	"Reserved",
	"Reserved",
};

static const char * const out_ch2_ltch[] = {
	"OUT_CH2 OUT2P Short circuit Fault",
	"OUT_CH2 OUT2M Short circuit Fault",
	"OUT_CH2 DRVRP Virtual Ground Fault",
	"OUT_CH2 DRVRM Virtual ground Fault",
	"Reserved",
	"Reserved",
	"AREG SC Fault Mask",
	"AREG SC Fault",
};

static const char * const int_ltch1[] = {
	"CH1 INP Over Voltage",
	"CH1 INM Over Voltage",
	"CH2 INP over Voltage",
	"CH2 INM Over Voltage",
	"Headset Insert Detection",
	"Headset Remove Detection",
	"Headset Hook",
	"MIPS Overload",
};

static const char * const int_ltch2[] = {
	"GPA Up threashold Fault",
	"GPA low threashold Fault",
	"VAD Power up detect",
	"VAD power down detect",
	"Micbias short circuit",
	"Micbias high current fault",
	"Micbias low current fault",
	"Micbias Over voltage fault",
};

static u32 int_reg_array[] = {
	TAC5X1X_REG_INT_LTCH0,
	TAC5X1X_REG_CHX_LTCH,
	TAC5X1X_REG_IN_CH1_LTCH,
	TAC5X1X_REG_IN_CH2_LTCH,
	TAC5X1X_REG_OUT_CH1_LTCH,
	TAC5X1X_REG_OUT_CH2_LTCH,
	TAC5X1X_REG_INT_LTCH1,
	TAC5X1X_REG_INT_LTCH2,
};

static const char * const int_reg_arr_name[] = {
	"TAC5X1X_REG_INT_LTCH0",
	"TAC5X1X_REG_CHX_LTCH",
	"TAC5X1X_REG_IN_CH1_LTCH",
	"TAC5X1X_REG_IN_CH2_LTCH",
	"TAC5X1X_REG_OUT_CH1_LTCH",
	"TAC5X1X_REG_OUT_CH2_LTCH",
	"TAC5X1X_REG_INT_LTCH1",
	"TAC5X1X_REG_INT_LTCH2",
};

static const struct interrupt_info interrupts[] = {
	{ int_ltch0, ARRAY_SIZE(int_ltch0) },
	{ chx_ltch, ARRAY_SIZE(chx_ltch) },
	{ in_ch1_ltch, ARRAY_SIZE(in_ch1_ltch) },
	{ in_ch2_ltch, ARRAY_SIZE(in_ch2_ltch) },
	{ out_ch1_ltch, ARRAY_SIZE(out_ch1_ltch) },
	{ out_ch2_ltch, ARRAY_SIZE(out_ch2_ltch) },
	{ int_ltch1, ARRAY_SIZE(int_ltch1) },
	{ int_ltch2, ARRAY_SIZE(int_ltch2) },
};

static s32 tac5x1x_regmap_write(struct tac5x1x_priv *tac5x1x,
				u32 reg, u32 value)
{
	s32 ret;
	s32 retry_count = 5;

	while (retry_count--) {
		ret = regmap_write(tac5x1x->regmap, reg,
				   value);
		if (ret >= 0)
			break;
		usleep_range(5000, 5050);
	}
	if (retry_count == -1)
		return 3;
	else
		return ret;
}

static s32 tac5x1x_regmap_read(struct tac5x1x_priv *tac5x1x,
			       u32 reg, u32 *value)
{
	s32 ret;
	s32 retry_count = 5;

	ret = regmap_reinit_cache(tac5x1x->regmap, &tac5x1x_regmap);
	if (ret) {
		dev_err(tac5x1x->dev, "Failed to reinit reg cache\n");
		return ret;
	}

	while (retry_count--) {
		ret = regmap_read(tac5x1x->regmap, reg,
				  value);
		if (ret >= 0)
			break;
		usleep_range(5000, 5050);
	}
	if (retry_count == -1)
		return 3;
	else
		return ret;
}

static s32 tac5x1x_dev_read(struct tac5x1x_priv *tac5x1x,
			    u32 dev_no, u32 reg,
			    u32 *ref_value)
{
	s32 ret;

	guard(mutex)(&tac5x1x->dev_lock);
	if (dev_no < tac5x1x->ndev) {
		ret = tac5x1x_regmap_write(tac5x1x,
					   TAC_PAGE_SELECT, 0);
		if (ret < 0) {
			dev_err(tac5x1x->dev, "%s, E=%d\n",
				__func__, ret);
			return ret;
		}

		ret = tac5x1x_regmap_read(tac5x1x, reg, ref_value);
		if (ret < 0)
			dev_err(tac5x1x->dev, "read, ERROR, E=%d\n",
				ret);
		else
			dev_dbg(tac5x1x->dev,
				"read PAGE:REG 0x%02x:0x%02x,0x%02x\n",
					TAC_PAGE_ID(reg),
					TAC_PAGE_REG(reg), *ref_value);
	} else {
		dev_err(tac5x1x->dev, "%s, ERROR: no such device(%d)\n",
			__func__, dev_no);
	}

	return 0;
}

static void tac5x1x_enable_irq(struct tac5x1x_priv *tac5x1x, bool is_enable)
{
	if (is_enable == tac5x1x->irqinfo.irq_enable &&
	    !gpio_is_valid(tac5x1x->irqinfo.irq_gpio))
		return;
	if (is_enable)
		enable_irq(tac5x1x->irqinfo.irq);
	else
		disable_irq_nosync(tac5x1x->irqinfo.irq);
	tac5x1x->irqinfo.irq_enable = is_enable;
}

static void tac5x1x_irq_work_func(struct tac5x1x_priv *tac5x1x)
{
	u32 reg_val, array_size, i, index = 0, bit = 0;
	s32 rc;

	tac5x1x_enable_irq(tac5x1x, false);
	array_size =  ARRAY_SIZE(int_reg_array);
	for (i = 0; i < array_size; i++) {
		rc = tac5x1x_dev_read(tac5x1x, index,
				      int_reg_array[i], &reg_val);
		if (!rc) {
			for (s32 j = 0; j < interrupts[i].size; j++) {
				bit = interrupts[i].size - 1 - j;
				if (reg_val & (1 << bit))
					dev_info(tac5x1x->dev,
						 "Reg: %s || %s\n",
						int_reg_arr_name[i],
						interrupts[i].names[j]);
			}
		} else {
			dev_err(tac5x1x->dev,
				"%s DEV_NO%d Read Reg 0x%04x error(rc=%d)\n",
					tac5x1x->dev_name, index,
					int_reg_array[i], rc);
		}
	}
	tac5x1x_enable_irq(tac5x1x, true);
}

static const struct regmap_range_cfg tac5x1x_ranges[] = {
	{
		.range_min = 0,
		.range_max = 12 * 128,
		.selector_reg = TAC_PAGE_SELECT,
		.selector_mask = GENMASK(7, 0),
		.selector_shift = 0,
		.window_start = 0,
		.window_len = 128,
	},
};

const struct regmap_config tac5x1x_regmap = {
	.max_register = 12 * 128,
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_MAPLE,
	.ranges = tac5x1x_ranges,
	.num_ranges = ARRAY_SIZE(tac5x1x_ranges),
};
EXPORT_SYMBOL(tac5x1x_regmap);

static void irq_work_routine(struct work_struct *work)
{
	struct tac5x1x_priv *tac5x1x =
		container_of(work, struct tac5x1x_priv, irqinfo.irq_work.work);

	if (tac5x1x->irq_work_func) {
		dev_dbg(tac5x1x->dev, "Calling irq_work_func\n");
		tac5x1x->irq_work_func(tac5x1x);
	} else {
		dev_dbg(tac5x1x->dev,
			"%s, irq_work_func is NULL\n", __func__);
	}
}

static irqreturn_t tac5x1x_irq_handler(s32 irq, void *dev_id)
{
	struct tac5x1x_priv *tac5x1x = (struct tac5x1x_priv *)dev_id;

	/* get IRQ status after 100 ms */
	schedule_delayed_work(&tac5x1x->irqinfo.irq_work,
			      msecs_to_jiffies(100));
	return IRQ_HANDLED;
}

static s32 tac5x1x_register_interrupt(struct tac5x1x_priv *tac5x1x)
{
	struct device_node *np = tac5x1x->dev->of_node;
	s32 ret;

	mutex_init(&tac5x1x->dev_lock);
	tac5x1x->irqinfo.irq = of_irq_get(np, 0);
	INIT_DELAYED_WORK(&tac5x1x->irqinfo.irq_work, irq_work_routine);

	dev_dbg(tac5x1x->dev, "irq = %d\n", tac5x1x->irqinfo.irq);
	ret = request_threaded_irq(tac5x1x->irqinfo.irq, tac5x1x_irq_handler,
				   NULL, IRQF_TRIGGER_FALLING, "TAC-IRQ", tac5x1x);
	if (ret) {
		dev_err(tac5x1x->dev, "request irq failed, ret %d\n", ret);
		return ret;
	}
	tac5x1x->irq_work_func = tac5x1x_irq_work_func;
	return 0;
};

static s32 tac5x1x_set_GPO1_gpio(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	s32 gpio_check, val;

	val = snd_soc_component_read(component, TAC5X1X_GPO1);
	gpio_check = ((val & TAC5X1X_GPIOX_CFG_MASK) >> 0);
	if (gpio_check != TAC5X1X_GPIO_GPO) {
		dev_err(component->dev,
			"%s: GPO1 is not configure as a GPO output\n",
			__func__);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0])
		val = 0;
	else
		val = TAC5X1X_GPO1_VAL;

	ucontrol->value.integer.value[0] = (val >> 0);
	snd_soc_component_update_bits(component, TAC5X1X_GPIOVAL,
				      TAC5X1X_GPO1_VAL, val);

	return 1;
};

static s32 tac5x1x_get_GPIO1_gpio(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	s32 val;

	val = snd_soc_component_read(component, TAC5X1X_GPIOVAL);
	ucontrol->value.integer.value[0] = ((val & TAC5X1X_GPIO1_MON) >> 0);

	return 0;
};

static s32 tac5x1x_set_GPIO1_gpio(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	s32 gpio_check, val;

	val = snd_soc_component_read(component, TAC5X1X_GPIO1);
	gpio_check = ((val & TAC5X1X_GPIOX_CFG_MASK) >> 0);
	if (gpio_check == TAC5X1X_GPIO_DISABLE) {
		dev_err(component->dev,
			"%s: GPIO1 is not configure as a GPO output\n",
			__func__);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0])
		val = 0;
	else
		val = TAC5X1X_GPIO1_VAL;

	ucontrol->value.integer.value[0] = (val >> 0);
	snd_soc_component_update_bits(component, TAC5X1X_GPIOVAL,
				      TAC5X1X_GPIO1_VAL, val);

	return 1;
};

static s32 tac5x1x_get_GPIO2_gpio(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	s32 val;

	val = snd_soc_component_read(component, TAC5X1X_GPIOVAL);
	ucontrol->value.integer.value[0] = !!(val & TAC5X1X_GPIO2_MON);

	return 0;
};

static s32 tac5x1x_set_GPIO2_gpio(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	s32 gpio_check, val;

	val = snd_soc_component_read(component, TAC5X1X_GPIO2);
	gpio_check = ((val & TAC5X1X_GPIOX_CFG_MASK) >> 0);
	if (gpio_check == TAC5X1X_GPIO_DISABLE) {
		dev_err(component->dev,
			"%s: GPIO2 is not configure as a GPO output\n",
			__func__);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0])
		val = 0;
	else
		val = TAC5X1X_GPIO2_VAL;

	ucontrol->value.integer.value[0] = (val >> 0);
	snd_soc_component_update_bits(component, TAC5X1X_GPIOVAL,
				      TAC5X1X_GPIO2_VAL, val);

	return 1;
};

static s32 tac5x1x_get_GPI1_gpio(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	s32 val;

	val = snd_soc_component_read(component, TAC5X1X_GPIOVAL);
	ucontrol->value.integer.value[0] = !!(val & TAC5X1X_GPI1_MON);

	return 0;
};

static const struct snd_kcontrol_new tac5x1x_GPO1[] = {
	SOC_SINGLE_BOOL_EXT("GPO1 GPO", 0, NULL, tac5x1x_set_GPO1_gpio),
};

static const struct snd_kcontrol_new tac5x1x_GPIO1_I[] = {
	SOC_SINGLE_BOOL_EXT("GPIO1 GPI", 0, tac5x1x_get_GPIO1_gpio, NULL),
};

static const struct snd_kcontrol_new tac5x1x_GPIO1_O[] = {
	SOC_SINGLE_BOOL_EXT("GPIO1 GPO", 0, NULL, tac5x1x_set_GPIO1_gpio),
};

static const struct snd_kcontrol_new tac5x1x_GPIO2_I[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "GPIO2 GPI",
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.get = tac5x1x_get_GPIO2_gpio,
		.info = snd_soc_info_bool_ext,
	}
};

static const struct snd_kcontrol_new tac5x1x_GPIO2_O[] = {
	SOC_SINGLE_BOOL_EXT("GPIO2 GPO", 0, NULL, tac5x1x_set_GPIO2_gpio),
};

static const struct snd_kcontrol_new tac5x1x_GPI1[] = {
	SOC_SINGLE_BOOL_EXT("GPI1 GPI", 0, tac5x1x_get_GPI1_gpio, NULL),
};

/* Record */
/* ADC Analog/PDM Selection */
static const char *const tac5x1x_input_source_text[] = {"Analog", "PDM"};

static SOC_ENUM_SINGLE_DECL(tac5x1x_in1_source_enum, TAC5X1X_INTF4, 7,
		tac5x1x_input_source_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_in2_source_enum, TAC5X1X_INTF4, 6,
		tac5x1x_input_source_text);

static const struct snd_kcontrol_new tac5x1x_dapm_in1_source_control[] = {
	SOC_DAPM_ENUM("CH1 Source MUX", tac5x1x_in1_source_enum),
};

static const struct snd_kcontrol_new tac5x1x_dapm_in2_source_control[] = {
	SOC_DAPM_ENUM("CH2 Source MUX", tac5x1x_in2_source_enum),
};

static const char *const tad5x1x_input_source_text[] = {"Disable", "PDM"};
static SOC_ENUM_SINGLE_DECL(tad5x1x_in1_source_enum, TAC5X1X_INTF4, 7,
		tad5x1x_input_source_text);
static SOC_ENUM_SINGLE_DECL(tad5x1x_in2_source_enum, TAC5X1X_INTF4, 6,
		tad5x1x_input_source_text);

static const struct snd_kcontrol_new tad5x1x_dapm_in1_source_control[] = {
	SOC_DAPM_ENUM("CH1 Source MUX", tad5x1x_in1_source_enum),
};

static const struct snd_kcontrol_new tad5x1x_dapm_in2_source_control[] = {
	SOC_DAPM_ENUM("CH2 Source MUX", tad5x1x_in2_source_enum),
};

/* ADC Analog source Selection */
static const char *const tac5x1x_input_analog_sel_text[] = {
	"Differential",
	"Single-ended",
	"Single-ended mux INxP",
	"Single-ended mux INxM",
};

static const char *const tac5x1x_input_analog2_sel_text[] = {
	"Differential",
	"Single-ended",
};

static SOC_ENUM_SINGLE_DECL(tac5x1x_adc1_config_enum, TAC5X1X_ADCCH1C0, 6,
		tac5x1x_input_analog_sel_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_adc2_config_enum, TAC5X1X_ADCCH2C0, 6,
		tac5x1x_input_analog2_sel_text);

static const struct snd_kcontrol_new tac5x1x_dapm_adc1_config_control[] = {
	SOC_DAPM_ENUM("ADC1 Analog MUX", tac5x1x_adc1_config_enum),
};

static const struct snd_kcontrol_new tac5x1x_dapm_adc2_config_control[] = {
	SOC_DAPM_ENUM("ADC2 Analog MUX", tac5x1x_adc2_config_enum),
};

/*
 * ADC full-scale selection
 * 2/10-VRMS is for TAX52xx/TAX51xx devices
 * 4/5-VRMS is for TAX54xx/TAX53xx devices
 */
static const char *const tac5x1x_adc_fscale_text[] = {"2/10-VRMS",
	"4/5-VRMS"};

static SOC_ENUM_SINGLE_DECL(tac5x1x_adc1_fscale_enum, TAC5X1X_ADCCH1C0, 1,
		tac5x1x_adc_fscale_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_adc2_fscale_enum, TAC5X1X_ADCCH2C0, 1,
		tac5x1x_adc_fscale_text);

static const struct snd_kcontrol_new tac5x1x_dapm_adc1_fscale_control[] = {
	SOC_DAPM_ENUM("ADC1 FSCALE MUX", tac5x1x_adc1_fscale_enum),
};

static const struct snd_kcontrol_new tac5x1x_dapm_adc2_fscale_control[] = {
	SOC_DAPM_ENUM("ADC2 FSCALE MUX", tac5x1x_adc2_fscale_enum),
};

/* Impedance Selection */
static const char *const resistor_text[] = {
	"5 kOhm",
	"10 kOhm",
	"40 kOhm",
};

static SOC_ENUM_SINGLE_DECL(adc1_resistor_enum, TAC5X1X_ADCCH1C0, 4,
		resistor_text);
static SOC_ENUM_SINGLE_DECL(adc2_resistor_enum, TAC5X1X_ADCCH2C0, 4,
		resistor_text);

/* PDM Data input pin Selection */
static const char *const pdm_pin_text[] = {
	"Disable",
	"GPIO1",
	"GPIO2",
	"GPI1",
};

static SOC_ENUM_SINGLE_DECL(pdm_12_pin_enum, TAC5X1X_INTF4, 2, pdm_pin_text);
static SOC_ENUM_SINGLE_DECL(pdm_34_pin_enum, TAC5X1X_INTF4, 0, pdm_pin_text);
static const struct snd_kcontrol_new pdm_12_pin_controls[] = {
	SOC_DAPM_ENUM("PDM chn12 Datain Select", pdm_12_pin_enum),
};

static const struct snd_kcontrol_new pdm_34_pin_controls[] = {
	SOC_DAPM_ENUM("PDM chn34 Datain Select", pdm_34_pin_enum),
};

static const char *const pdmclk_text[] = {
	"2.8224 MHz or 3.072 MHz", "1.4112 MHz or 1.536 MHz",
	"705.6 kHz or 768 kHz", "5.6448 MHz or 6.144 MHz"};

static SOC_ENUM_SINGLE_DECL(pdmclk_select_enum, TAC5X1X_CNTCLK0, 6,
		pdmclk_text);

/* Digital Volume control. From -80 to 47 dB in 0.5 dB steps */
static DECLARE_TLV_DB_SCALE(record_dig_vol_tlv, -8000, 50, 0);

/* Gain Calibration control. From -0.8db to 0.7db dB in 0.1 dB steps */
static DECLARE_TLV_DB_MINMAX(record_gain_cali_tlv, -80, 70);

/* Analog Level control. From -12 to 24 dB in 6 dB steps */
static DECLARE_TLV_DB_SCALE(playback_analog_level_tlv, -1200, 600, 0);

/* Digital Volume control. From -100 to 27 dB in 0.5 dB steps */
static DECLARE_TLV_DB_SCALE(dac_dig_vol_tlv, -10000, 50, 0); // mute ?

/* Gain Calibration control. From -0.8db to 0.7db dB in 0.1 dB steps */
static DECLARE_TLV_DB_MINMAX(playback_gain_cali_tlv, -80, 70);

/* Output Source Selection */
static const char *const tac5x1x_output_source_text[] = {
	"Disabled",
	"DAC Input",
	"Analog Bypass",
	"DAC + Analog Bypass Mix",
	"DAC -> OUTxP, INxP -> OUTxM",
	"INxM -> OUTxP, DAC -> OUTxM",
};

static SOC_ENUM_SINGLE_DECL(tac5x1x_out1_source_enum, TAC5X1X_OUT1CFG0, 5,
		tac5x1x_output_source_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_out2_source_enum, TAC5X1X_OUT2CFG0, 5,
		tac5x1x_output_source_text);

static const struct snd_kcontrol_new tac5x1x_dapm_out1_source_control[] = {
	SOC_DAPM_ENUM("OUT1X MUX", tac5x1x_out1_source_enum),
};

static const struct snd_kcontrol_new tac5x1x_dapm_out2_source_control[] = {
	SOC_DAPM_ENUM("OUT2X MUX", tac5x1x_out2_source_enum),
};

/* Output Config Selection */
static const char *const tac5x1x_output_config_text[] = {
	"Differential",
	"Stereo Single-ended",
	"Mono Single-ended at OUTxP only",
	"Mono Single-ended at OUTxM only",
	"Pseudo differential with OUTxM as VCOM",
	"Pseudo differential with OUTxM as external sensing",
	"Pseudo differential with OUTxP as VCOM",
};

static const char *const tac5x1x_output2_config_text[] = {
	"Differential",
	"Stereo Single-ended",
	"Mono Single-ended at OUTxP only",
	"Mono Single-ended at OUTxM only",
	"Pseudo differential with OUTxM as VCOM",
	"Reserved",
	"Pseudo differential with OUTxP as VCOM",
};

static SOC_ENUM_SINGLE_DECL(tac5x1x_out1_config_enum, TAC5X1X_OUT1CFG0, 2,
			tac5x1x_output_config_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_out2_config_enum, TAC5X1X_OUT2CFG0, 2,
			tac5x1x_output2_config_text);

static const struct snd_kcontrol_new tac5x1x_dapm_out1_config_control[] = {
	SOC_DAPM_ENUM("OUT1X Config MUX", tac5x1x_out1_config_enum),
};

static const struct snd_kcontrol_new tac5x1x_dapm_out2_config_control[] = {
	SOC_DAPM_ENUM("OUT2X Config MUX", tac5x1x_out2_config_enum),
};

static const char *const tac5x1x_wideband_text[] = {
	"Audio BW 24-kHz",
	"Wide BW 96-kHz",
};

static SOC_ENUM_SINGLE_DECL(tac5x1x_adc1_wideband_enum, TAC5X1X_ADCCH1C0, 0,
		tac5x1x_wideband_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_adc2_wideband_enum, TAC5X1X_ADCCH2C0, 0,
		tac5x1x_wideband_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_dac1_wideband_enum, TAC5X1X_OUT1CFG1, 0,
		tac5x1x_wideband_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_dac2_wideband_enum, TAC5X1X_OUT2CFG1, 0,
		tac5x1x_wideband_text);

static const char *const tac5x1x_tolerance_text[] = {
	"AC Coupled with 100mVpp",
	"AC/DC Coupled with 1Vpp",
	"AC/DC Coupled with Rail-to-rail",
};

static SOC_ENUM_SINGLE_DECL(tac5x1x_adc1_tolerance_enum, TAC5X1X_ADCCH1C0, 2,
		tac5x1x_tolerance_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_adc2_tolerance_enum, TAC5X1X_ADCCH2C0, 2,
		tac5x1x_tolerance_text);

/* Output Drive Selection */
static const char *const tac5x1x_output_driver_text[] = {
	"Line-out",
	"Headphone",
	"4 ohm",
	"FD Receiver/Debug",
};

static SOC_ENUM_SINGLE_DECL(out1p_driver_enum, TAC5X1X_OUT1CFG1, 6,
		tac5x1x_output_driver_text);

static SOC_ENUM_SINGLE_DECL(out2p_driver_enum, TAC5X1X_OUT2CFG1, 6,
		tac5x1x_output_driver_text);

static const struct snd_kcontrol_new tac5x1x_dapm_out1_driver_control[] = {
	SOC_DAPM_ENUM("OUT1 driver MUX", out1p_driver_enum),
};

static const struct snd_kcontrol_new tac5x1x_dapm_out2_driver_control[] = {
	SOC_DAPM_ENUM("OUT2 driver MUX", out2p_driver_enum),
};

/* Decimation Filter Selection */
static const char *const decimation_filter_text[] = {
	"Linear Phase", "Low Latency", "Ultra-low Latency"};

static SOC_ENUM_SINGLE_DECL(decimation_filter_record_enum, TAC5X1X_DSP0, 6,
			decimation_filter_text);
static SOC_ENUM_SINGLE_DECL(decimation_filter_playback_enum, TAC5X1X_DSP1, 6,
			    decimation_filter_text);

static const struct snd_kcontrol_new tx_ch1_asi_switch =
	SOC_DAPM_SINGLE("Capture Switch", TAC5X1X_PASITXCH1, 5, 1, 0);
static const struct snd_kcontrol_new tx_ch2_asi_switch =
	SOC_DAPM_SINGLE("Capture Switch", TAC5X1X_PASITXCH2, 5, 1, 0);
static const struct snd_kcontrol_new tx_ch3_asi_switch =
	SOC_DAPM_SINGLE("Capture Switch", TAC5X1X_PASITXCH3, 5, 1, 0);
static const struct snd_kcontrol_new tx_ch4_asi_switch =
	SOC_DAPM_SINGLE("Capture Switch", TAC5X1X_PASITXCH4, 5, 1, 0);

static const struct snd_kcontrol_new rx_ch1_asi_switch =
	SOC_DAPM_SINGLE("Switch", TAC5X1X_PASIRXCH1, 5, 1, 0);
static const struct snd_kcontrol_new rx_ch2_asi_switch =
	SOC_DAPM_SINGLE("Switch", TAC5X1X_PASIRXCH2, 5, 1, 0);

static const char *const rx_ch3_asi_cfg_text[] = {
		"Disable",
		"DAC channel data",
};

static const char *const rx_ch5_asi_cfg_text[] = {
		"Disable",
		"DAC channel data",
		"ADC channel output loopback",
};

static const char *const rx_ch6_asi_cfg_text[] = {
		"Disable",
		"DAC channel data",
		"ADC channel output loopback",
		"Channel Input to ICLA device",
};

static const char *const tx_ch5_asi_cfg_text[] = {
		"Tristate",
		"Input Channel Loopback data",
		"Echo reference Channel data",
};

static const char *const tx_ch7_asi_cfg_text[] = {
		"Tristate",
		"Vbat_Wlby2,Temp_Wlby2",
		"echo_ref_ch1,echo_ref_ch2",
};

static const char *const tx_ch8_asi_cfg_text[] = {
		"Tristate",
		"ICLA data",
};

static const char *const tac5x1x_slot_select_text[] = {
	"Slot 0", "Slot 1", "Slot 2", "Slot 3",
	"Slot 4", "Slot 5", "Slot 6", "Slot 7",
	"Slot 8", "Slot 9", "Slot 10", "Slot 11",
	"Slot 12", "Slot 13", "Slot 14", "Slot 15",
	"Slot 16", "Slot 17", "Slot 18", "Slot 19",
	"Slot 20", "Slot 21", "Slot 22", "Slot 23",
	"Slot 24", "Slot 25", "Slot 26", "Slot 27",
	"Slot 28", "Slot 29", "Slot 30", "Slot 31",
};

static const char *const out2x_vcom_text[] = {
	"0.6 * Vref",
	"AVDD by 2",
};

static const char *const diag_cfg_text[] = {
	"0mv", "30mv", "60mv", "90mv",
	"120mv", "150mv", "180mv", "210mv",
	"240mv", "270mv", "300mv", "330mv",
	"360mv", "390mv", "420mv", "450mv",
};

static const char *const diag_cfg_gnd_text[] = {
	"0mv", "60mv", "120mv", "180mv",
	"240mv", "300mv", "360mv", "420mv",
	"480mv", "540mv", "600mv", "660mv",
	"720mv", "780mv", "840mv", "900mv",
};

static SOC_ENUM_SINGLE_DECL(out2x_vcom_enum, TAC5X1X_OUT2CFG0, 1,
		out2x_vcom_text);

static SOC_ENUM_SINGLE_DECL(tx_ch5_asi_cfg_enum, TAC5X1X_PASITXCH5, 5,
		tx_ch5_asi_cfg_text);
static SOC_ENUM_SINGLE_DECL(tx_ch6_asi_cfg_enum, TAC5X1X_PASITXCH6, 5,
		tx_ch5_asi_cfg_text);
static SOC_ENUM_SINGLE_DECL(tx_ch7_asi_cfg_enum, TAC5X1X_PASITXCH7, 5,
		tx_ch7_asi_cfg_text);
static SOC_ENUM_SINGLE_DECL(tx_ch8_asi_cfg_enum, TAC5X1X_PASITXCH8, 5,
		tx_ch8_asi_cfg_text);
static SOC_ENUM_SINGLE_DECL(tx_ch1_slot_select_enum, TAC5X1X_PASITXCH1, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(tx_ch2_slot_select_enum, TAC5X1X_PASITXCH2, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(tx_ch3_slot_select_enum, TAC5X1X_PASITXCH3, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(tx_ch4_slot_select_enum, TAC5X1X_PASITXCH4, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(tx_ch5_slot_select_enum, TAC5X1X_PASITXCH5, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(tx_ch6_slot_select_enum, TAC5X1X_PASITXCH6, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(tx_ch7_slot_select_enum, TAC5X1X_PASITXCH7, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(tx_ch8_slot_select_enum, TAC5X1X_PASITXCH8, 0,
		tac5x1x_slot_select_text);

static SOC_ENUM_SINGLE_DECL(rx_ch3_asi_cfg_enum, TAC5X1X_PASIRXCH3, 5,
		rx_ch3_asi_cfg_text);
static SOC_ENUM_SINGLE_DECL(rx_ch4_asi_cfg_enum, TAC5X1X_PASIRXCH4, 5,
		rx_ch3_asi_cfg_text);
static SOC_ENUM_SINGLE_DECL(rx_ch5_asi_cfg_enum, TAC5X1X_PASIRXCH5, 5,
		rx_ch5_asi_cfg_text);
static SOC_ENUM_SINGLE_DECL(rx_ch6_asi_cfg_enum, TAC5X1X_PASIRXCH6, 5,
		rx_ch6_asi_cfg_text);
static SOC_ENUM_SINGLE_DECL(rx_ch7_asi_cfg_enum, TAC5X1X_PASIRXCH7, 5,
		rx_ch6_asi_cfg_text);
static SOC_ENUM_SINGLE_DECL(rx_ch8_asi_cfg_enum, TAC5X1X_PASIRXCH8, 5,
		rx_ch6_asi_cfg_text);

static SOC_ENUM_SINGLE_DECL(rx_ch1_slot_select_enum, TAC5X1X_PASIRXCH1, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(rx_ch2_slot_select_enum, TAC5X1X_PASIRXCH2, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(rx_ch3_slot_select_enum, TAC5X1X_PASIRXCH3, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(rx_ch4_slot_select_enum, TAC5X1X_PASIRXCH4, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(rx_ch5_slot_select_enum, TAC5X1X_PASIRXCH5, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(rx_ch6_slot_select_enum, TAC5X1X_PASIRXCH6, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(rx_ch7_slot_select_enum, TAC5X1X_PASIRXCH7, 0,
		tac5x1x_slot_select_text);
static SOC_ENUM_SINGLE_DECL(rx_ch8_slot_select_enum, TAC5X1X_PASIRXCH8, 0,
		tac5x1x_slot_select_text);

static SOC_ENUM_SINGLE_DECL(diag_cfg1_sht_term_enum, TAC5X1X_DIAG_CFG1, 4,
		diag_cfg_text);
static SOC_ENUM_SINGLE_DECL(diag_cfg1_vbat_in_enum, TAC5X1X_DIAG_CFG1, 0,
		diag_cfg_text);
static SOC_ENUM_SINGLE_DECL(diag_cfg2_sht_gnd_enum, TAC5X1X_DIAG_CFG2, 4,
		diag_cfg_gnd_text);
static SOC_ENUM_SINGLE_DECL(diag_cfg2_micbias, TAC5X1X_DIAG_CFG2, 0,
		diag_cfg_text);

static const struct snd_kcontrol_new taa5x1x_controls[] = {
	SOC_ENUM("Record Decimation Filter",
		 decimation_filter_record_enum),
	SOC_ENUM("ADC1 Audio BW", tac5x1x_adc1_wideband_enum),

	SOC_SINGLE_TLV("ADC1 Digital Capture Volume", TAC5X1X_ADCCH1C2,
		       0, 0xff, 0, record_dig_vol_tlv),

	SOC_SINGLE_TLV("ADC1 Fine Capture Volume", TAC5X1X_ADCCH1C3,
		       0, 0xff, 0, record_gain_cali_tlv),

	SOC_SINGLE_RANGE("ADC1 Phase Capture Volume", TAC5X1X_ADCCH1C4,
			 2, 0, 63, 0),

	SOC_ENUM("ASI_TX_CH5_CFG", tx_ch5_asi_cfg_enum),
	SOC_ENUM("ASI_TX_CH6_CFG", tx_ch6_asi_cfg_enum),
	SOC_ENUM("ASI_TX_CH7_CFG", tx_ch7_asi_cfg_enum),
	SOC_ENUM("ASI_TX_CH8_CFG", tx_ch8_asi_cfg_enum),
	SOC_ENUM("ASI_TX_CH1_SLOT", tx_ch1_slot_select_enum),
	SOC_ENUM("ASI_TX_CH2_SLOT", tx_ch2_slot_select_enum),
	SOC_ENUM("ASI_TX_CH3_SLOT", tx_ch3_slot_select_enum),
	SOC_ENUM("ASI_TX_CH4_SLOT", tx_ch4_slot_select_enum),
	SOC_ENUM("ASI_TX_CH5_SLOT", tx_ch5_slot_select_enum),
	SOC_ENUM("ASI_TX_CH6_SLOT", tx_ch6_slot_select_enum),
	SOC_ENUM("ASI_TX_CH7_SLOT", tx_ch7_slot_select_enum),
	SOC_ENUM("ASI_TX_CH8_SLOT", tx_ch8_slot_select_enum),
	SOC_SINGLE("IN_CH1_EN Capture Switch", TAC5X1X_CH_EN, 7, 1, 0),
	SOC_SINGLE("IN_CH2_EN Capture Switch", TAC5X1X_CH_EN, 6, 1, 0),
	SOC_SINGLE("IN_CH3_EN Capture Switch", TAC5X1X_CH_EN, 5, 1, 0),
	SOC_SINGLE("IN_CH4_EN Capture Switch", TAC5X1X_CH_EN, 4, 1, 0),
};

static const struct snd_kcontrol_new tad5x1x_controls[] = {
	SOC_ENUM("Playback Decimation Filter",
		 decimation_filter_playback_enum),
	SOC_ENUM("DAC1 Audio BW", tac5x1x_dac1_wideband_enum),
	SOC_SINGLE_TLV("OUT1P Analog Level Playback Volume", TAC5X1X_OUT1CFG1,
		       3, 6, 1, playback_analog_level_tlv),
	SOC_SINGLE_TLV("OUT1M Analog Level Playback Volume", TAC5X1X_OUT1CFG2,
		       3, 6, 1, playback_analog_level_tlv),
	SOC_SINGLE_TLV("DAC1 CHA Digital Playback Volume", TAC5X1X_DACCH1A0,
		       0, 0xff, 0, dac_dig_vol_tlv),
	SOC_SINGLE_TLV("DAC1 CHB Digital Playback Volume", TAC5X1X_DACCH1B0,
		       0, 0xff, 0, dac_dig_vol_tlv),
	SOC_SINGLE_TLV("DAC1 CHA Gain Calibration Playback Volume",
		       TAC5X1X_DACCH1A1, 4, 0xf, 0,
			playback_gain_cali_tlv),
	SOC_SINGLE_TLV("DAC1 CHB Gain Calibration Playback Volume",
		       TAC5X1X_DACCH1B1, 4, 0xf, 0,
			playback_gain_cali_tlv),

	SOC_ENUM("ASI_RX_CH3_EN Playback", rx_ch3_asi_cfg_enum),
	SOC_ENUM("ASI_RX_CH4_EN Playback", rx_ch4_asi_cfg_enum),
	SOC_ENUM("ASI_RX_CH5_EN Playback", rx_ch5_asi_cfg_enum),
	SOC_ENUM("ASI_RX_CH6_EN Playback", rx_ch6_asi_cfg_enum),
	SOC_ENUM("ASI_RX_CH7_EN Playback", rx_ch7_asi_cfg_enum),
	SOC_ENUM("ASI_RX_CH8_EN Playback", rx_ch8_asi_cfg_enum),
	SOC_ENUM("ASI_RX_CH1_SLOT Playback", rx_ch1_slot_select_enum),
	SOC_ENUM("ASI_RX_CH2_SLOT Playback", rx_ch2_slot_select_enum),
	SOC_ENUM("ASI_RX_CH3_SLOT Playback", rx_ch3_slot_select_enum),
	SOC_ENUM("ASI_RX_CH4_SLOT Playback", rx_ch4_slot_select_enum),
	SOC_ENUM("ASI_RX_CH5_SLOT Playback", rx_ch5_slot_select_enum),
	SOC_ENUM("ASI_RX_CH6_SLOT Playback", rx_ch6_slot_select_enum),
	SOC_ENUM("ASI_RX_CH7_SLOT Playback", rx_ch7_slot_select_enum),
	SOC_ENUM("ASI_RX_CH8_SLOT Playback", rx_ch8_slot_select_enum),

	SOC_SINGLE("OUT_CH1_EN Playback Switch", TAC5X1X_CH_EN, 3, 1, 0),
	SOC_SINGLE("OUT_CH2_EN Playback Switch", TAC5X1X_CH_EN, 2, 1, 0),
	SOC_SINGLE("OUT_CH3_EN Playback Switch", TAC5X1X_CH_EN, 1, 1, 0),
	SOC_SINGLE("OUT_CH4_EN Playback Switch", TAC5X1X_CH_EN, 0, 1, 0),
};

static const struct snd_kcontrol_new tac5x11_controls[] = {
	SOC_ENUM("ADC1 Common-mode Tolerance", tac5x1x_adc1_tolerance_enum),
	SOC_ENUM("ADC1 Impedance Select", adc1_resistor_enum),
};

static const struct snd_kcontrol_new tad5x12_controls[] = {
	SOC_SINGLE_TLV("OUT2P Analog Level Playback Volume", TAC5X1X_OUT2CFG1,
		       3, 6, 1, playback_analog_level_tlv),
	SOC_SINGLE_TLV("OUT2M Analog Level Playback Volume", TAC5X1X_OUT2CFG2,
		       3, 6, 1, playback_analog_level_tlv),
	SOC_SINGLE_TLV("DAC2 CHA Digital Playback Volume", TAC5X1X_DACCH2A0,
		       0, 0xff, 0, dac_dig_vol_tlv),
	SOC_SINGLE_TLV("DAC2 CHB Digital Playback Volume", TAC5X1X_DACCH2B0,
		       0, 0xff, 0, dac_dig_vol_tlv),
	SOC_SINGLE_TLV("DAC2 CHA Gain Calibration Playback Volume",
		       TAC5X1X_DACCH2A1, 4, 0xf, 0,
			playback_gain_cali_tlv),
	SOC_SINGLE_TLV("DAC2 CHB Gain Calibration Playback Volume",
		       TAC5X1X_DACCH2B1, 4, 0xf, 0,
			playback_gain_cali_tlv),
	SOC_ENUM("DAC2 Audio BW", tac5x1x_dac2_wideband_enum),
	SOC_ENUM("OUT2X_VCOM", out2x_vcom_enum),
};

static const struct snd_kcontrol_new taa5x12_controls[] = {
	SOC_ENUM("ADC2 Audio BW", tac5x1x_adc2_wideband_enum),

	SOC_SINGLE_TLV("ADC2 Digital Capture Volume", TAC5X1X_ADCCH2C2,
		       0, 0xff, 0, record_dig_vol_tlv),

	SOC_SINGLE_TLV("ADC2 Fine Capture Volume", TAC5X1X_ADCCH2C3,
		       0, 0xff, 0, record_gain_cali_tlv),

	SOC_SINGLE_RANGE("ADC2 Phase Capture Volume", TAC5X1X_ADCCH2C4,
			 2, 0, 63, 0),
};

static const struct snd_kcontrol_new tac5x12_impedance_controls[] = {
	SOC_ENUM("ADC1 Impedance Select", adc1_resistor_enum),
	SOC_ENUM("ADC2 Impedance Select", adc2_resistor_enum),
};

static const struct snd_kcontrol_new tac5x12_tolerance_controls[] = {
	SOC_ENUM("ADC1 Common-mode Tolerance", tac5x1x_adc1_tolerance_enum),
	SOC_ENUM("ADC2 Common-mode Tolerance", tac5x1x_adc2_tolerance_enum),
};

static const struct snd_kcontrol_new tac5x1x_pdm_controls[] = {
	SOC_ENUM("PDM Clk Divider", pdmclk_select_enum),

	SOC_SINGLE_TLV("PDM1 Digital Capture Volume", TAC5X1X_ADCCH1C2,
		       0, 0xff, 0, record_dig_vol_tlv),
	SOC_SINGLE_TLV("PDM2 Digital Capture Volume", TAC5X1X_ADCCH2C2,
		       0, 0xff, 0, record_dig_vol_tlv),

	SOC_SINGLE_TLV("PDM1 Fine Capture Volume", TAC5X1X_ADCCH1C3,
		       0, 0xff, 0, record_gain_cali_tlv),
	SOC_SINGLE_TLV("PDM2 Fine Capture Volume", TAC5X1X_ADCCH2C3,
		       0, 0xff, 0, record_gain_cali_tlv),

	SOC_SINGLE_RANGE("PDM1 Phase Capture Volume", TAC5X1X_ADCCH1C4,
			 2, 0, 63, 0),
	SOC_SINGLE_RANGE("PDM2 Phase Capture Volume", TAC5X1X_ADCCH2C4,
			 2, 0, 63, 0),

	SOC_SINGLE_TLV("PDM3 Digital Capture Volume", TAC5X1X_ADCCH3C2,
		       0, 0xff, 0, record_dig_vol_tlv),
	SOC_SINGLE_TLV("PDM4 Digital Capture Volume", TAC5X1X_ADCCH4C2,
		       0, 0xff, 0, record_dig_vol_tlv),

	SOC_SINGLE_TLV("PDM3 Fine Capture Volume", TAC5X1X_ADCCH3C3,
		       0, 0xff, 0, record_gain_cali_tlv),
	SOC_SINGLE_TLV("PDM4 Fine Capture Volume", TAC5X1X_ADCCH4C3,
		       0, 0xff, 0, record_gain_cali_tlv),

	SOC_SINGLE_RANGE("PDM3 Phase Capture Volume", TAC5X1X_ADCCH3C4,
			 2, 0, 63, 0),
	SOC_SINGLE_RANGE("PDM4 Phase Capture Volume", TAC5X1X_ADCCH4C4,
			 2, 0, 63, 0),
};

static const struct snd_kcontrol_new taa5x1x_ip_controls[] = {
	SOC_ENUM("DIAG_SHT_TERM", diag_cfg1_sht_term_enum),
	SOC_ENUM("DIAG_SHT_VBAT_IN", diag_cfg1_vbat_in_enum),
	SOC_ENUM("DIAG_SHT_GND", diag_cfg2_sht_gnd_enum),
	SOC_ENUM("DIAG_SHT_MICBIAS", diag_cfg2_micbias),
};

static const struct snd_soc_dapm_widget taa5x1x_dapm_widgets[] = {
	/* ADC1 */
	SND_SOC_DAPM_INPUT("AIN1"),

	SND_SOC_DAPM_MUX("ADC1 Full-Scale", SND_SOC_NOPM, 0, 0,
			 tac5x1x_dapm_adc1_fscale_control),

	SND_SOC_DAPM_MUX("ADC1 Config", SND_SOC_NOPM, 0, 0,
			 tac5x1x_dapm_adc1_config_control),

	SND_SOC_DAPM_ADC("CH1_ADC_EN", "CH1 Capture", TAC5X1X_CH_EN, 7, 0),

	SND_SOC_DAPM_SWITCH("ASI_TX_CH1_EN", SND_SOC_NOPM, 0, 0,
			    &tx_ch1_asi_switch),

	SND_SOC_DAPM_MICBIAS("Mic Bias", TAC5X1X_PWR_CFG, 5, 0),
	SND_SOC_DAPM_SWITCH("ASI_TX_CH2_EN", SND_SOC_NOPM, 0, 0,
			    &tx_ch2_asi_switch),
};

static const struct snd_kcontrol_new tad5x1x_pdm_controls[] = {
	SOC_ENUM("ASI_TX_CH1_SLOT", tx_ch1_slot_select_enum),
	SOC_ENUM("ASI_TX_CH2_SLOT", tx_ch2_slot_select_enum),
	SOC_ENUM("ASI_TX_CH3_SLOT", tx_ch3_slot_select_enum),
	SOC_ENUM("ASI_TX_CH4_SLOT", tx_ch4_slot_select_enum),
};

static const struct snd_soc_dapm_widget tad5xx_dapm_widgets[] = {
	/* pdm capture */
	SND_SOC_DAPM_SWITCH("ASI_TX_CH1_EN", SND_SOC_NOPM, 0, 0,
			    &tx_ch1_asi_switch),
	SND_SOC_DAPM_SWITCH("ASI_TX_CH2_EN", SND_SOC_NOPM, 0, 0,
			    &tx_ch2_asi_switch),
};

static const struct snd_soc_dapm_widget tad5x1x_dapm_widgets[] = {
	/* DAC1 */
	SND_SOC_DAPM_AIF_IN("ASI IN1", "ASI Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUTPUT("OUT1"),

	SND_SOC_DAPM_MUX("OUT1x Source", SND_SOC_NOPM, 0, 0,
			 tac5x1x_dapm_out1_source_control),

	SND_SOC_DAPM_MUX("OUT1x Config", SND_SOC_NOPM, 0, 0,
			 tac5x1x_dapm_out1_config_control),

	SND_SOC_DAPM_MUX("OUT1x Driver", SND_SOC_NOPM, 0, 0,
			 tac5x1x_dapm_out1_driver_control),

	SND_SOC_DAPM_DAC("Left DAC Enable", "Left Playback", TAC5X1X_CH_EN, 3,
			 0),
	SND_SOC_DAPM_PGA("Left DAC Power", TAC5X1X_PWR_CFG, 6, 0, NULL, 0),

	SND_SOC_DAPM_SWITCH("ASI_RX_CH1_EN", SND_SOC_NOPM, 0, 0,
			    &rx_ch1_asi_switch),
};

static const struct snd_soc_dapm_widget taa5x12_dapm_widgets[] = {
	/* ADC2 */
	SND_SOC_DAPM_INPUT("AIN2"),

	SND_SOC_DAPM_MUX("ADC2 Full-Scale", SND_SOC_NOPM, 0, 0,
			 tac5x1x_dapm_adc2_fscale_control),

	SND_SOC_DAPM_MUX("ADC2 Config", SND_SOC_NOPM, 0, 0,
			 tac5x1x_dapm_adc2_config_control),

	SND_SOC_DAPM_ADC("CH2_ADC_EN", "CH2 Capture", TAC5X1X_CH_EN, 6, 0),

};

static const struct snd_soc_dapm_widget tad5x12_dapm_widgets[] = {
	/* DAC2 */
	SND_SOC_DAPM_OUTPUT("OUT2"),

	SND_SOC_DAPM_AIF_IN("ASI IN2", "ASI Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MUX("OUT2x Source", SND_SOC_NOPM, 0, 0,
			 tac5x1x_dapm_out2_source_control),

	SND_SOC_DAPM_MUX("OUT2x Config", SND_SOC_NOPM, 0, 0,
			 tac5x1x_dapm_out2_config_control),

	SND_SOC_DAPM_MUX("OUT2x Driver", SND_SOC_NOPM, 0, 0,
			 tac5x1x_dapm_out2_driver_control),

	SND_SOC_DAPM_DAC("Right DAC Enable",
			 "Right Playback", TAC5X1X_CH_EN, 2, 0),
	SND_SOC_DAPM_PGA("Right DAC Power", TAC5X1X_PWR_CFG, 6, 0, NULL, 0),

	SND_SOC_DAPM_SWITCH("ASI_RX_CH2_EN", SND_SOC_NOPM, 0, 0,
			    &rx_ch2_asi_switch),
};

static const struct snd_soc_dapm_widget tac5x1x_dapm_pdm_widgets[] = {
	/* PDM */
	SND_SOC_DAPM_INPUT("DIN1"),
	SND_SOC_DAPM_INPUT("DIN2"),
	SND_SOC_DAPM_INPUT("DIN3"),
	SND_SOC_DAPM_INPUT("DIN4"),

	SND_SOC_DAPM_MUX("PDM ch1 & ch2 Datain Select",
			 SND_SOC_NOPM, 0, 0, pdm_12_pin_controls),
	SND_SOC_DAPM_MUX("PDM ch3 & ch4 Datain Select",
			 SND_SOC_NOPM, 0, 0, pdm_34_pin_controls),

	SND_SOC_DAPM_ADC("CH1_PDM_EN", "PDM CH1 Capture", TAC5X1X_CH_EN, 7, 0),
	SND_SOC_DAPM_ADC("CH2_PDM_EN", "PDM CH2 Capture", TAC5X1X_CH_EN, 6, 0),
	SND_SOC_DAPM_ADC("CH3_PDM_EN", "PDM CH3 Capture", TAC5X1X_CH_EN, 5, 0),
	SND_SOC_DAPM_ADC("CH4_PDM_EN", "PDM CH4 Capture", TAC5X1X_CH_EN, 4, 0),

	SND_SOC_DAPM_SWITCH("ASI_TX_CH3_EN", SND_SOC_NOPM, 0, 0,
			    &tx_ch3_asi_switch),
	SND_SOC_DAPM_SWITCH("ASI_TX_CH4_EN", SND_SOC_NOPM, 0, 0,
			    &tx_ch4_asi_switch),
};

static const struct snd_soc_dapm_widget tac5x1x_common_dapm_widgets[] = {
	SND_SOC_DAPM_MUX("IN1 Source Mux", SND_SOC_NOPM, 0, 0,
			 tac5x1x_dapm_in1_source_control),
	SND_SOC_DAPM_MUX("IN2 Source Mux", SND_SOC_NOPM, 0, 0,
			 tac5x1x_dapm_in2_source_control),
	SND_SOC_DAPM_PGA("ADC Power", TAC5X1X_PWR_CFG, 7, 0, NULL, 0),
	SND_SOC_DAPM_AIF_OUT("AIF OUT", "ASI Capture", 0, SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_dapm_widget tad5x1x_common_dapm_widgets[] = {
	SND_SOC_DAPM_MUX("IN1 Source Mux", SND_SOC_NOPM, 0, 0,
			 tad5x1x_dapm_in1_source_control),
	SND_SOC_DAPM_MUX("IN2 Source Mux", SND_SOC_NOPM, 0, 0,
			 tad5x1x_dapm_in2_source_control),
	SND_SOC_DAPM_PGA("ADC Power", TAC5X1X_PWR_CFG, 7, 0, NULL, 0),
	SND_SOC_DAPM_AIF_OUT("AIF OUT", "ASI Capture", 0, SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_dapm_route taa5x1x_dapm_routes[] = {
	/* ADC channel1 */
	{"IN1 Source Mux", "Analog", "AIN1"},
	{"IN2 Source Mux", "Analog",
		"IN1 Source Mux"},

	{"CH1_ADC_EN", NULL, "IN2 Source Mux"},
	{"ASI_TX_CH1_EN", "Capture Switch", "CH1_ADC_EN"},

	{"ADC1 Config", "Differential", "ASI_TX_CH1_EN"},
	{"ADC1 Config", "Single-ended", "ASI_TX_CH1_EN"},
	{"ADC1 Config", "Single-ended mux INxP",
		"ASI_TX_CH1_EN"},
	{"ADC1 Config", "Single-ended mux INxM",
		"ASI_TX_CH1_EN"},

	{"ADC1 Full-Scale", "2/10-VRMS",
		"ADC1 Config"},
	{"ADC1 Full-Scale", "4/5-VRMS",
		"ADC1 Config"},
	{"Mic Bias", NULL, "ADC1 Full-Scale"},

};

static const struct snd_soc_dapm_route tad5x1x_dapm_routes[] = {
	/* Left Output */
	{"ASI_RX_CH1_EN", "Switch", "ASI IN1"},

	{"OUT1x Source", "DAC + Analog Bypass Mix", "ASI_RX_CH1_EN"},
	{"OUT1x Source", "DAC -> OUTxP, INxP -> OUTxM", "ASI_RX_CH1_EN"},
	{"OUT1x Source", "INxM -> OUTxP, DAC -> OUTxM", "ASI_RX_CH1_EN"},

	{"OUT1x Config", "Differential", "OUT1x Source"},
	// {"OUT1x Config", "Stereo Single-ended", "OUT1x Source"},
	{"OUT1x Config", "Mono Single-ended at OUTxP only", "OUT1x Source"},
	{"OUT1x Config", "Mono Single-ended at OUTxM only", "OUT1x Source"},
	{"OUT1x Config", "Pseudo differential with OUTxM as VCOM",
		"OUT1x Source"},
	{"OUT1x Config", "Pseudo differential with OUTxM as external sensing",
		"OUT1x Source"},
	{"OUT1x Config", "Pseudo differential with OUTxP as VCOM",
		"OUT1x Source"},

	{"OUT1x Driver", "Line-out", "OUT1x Config"},
	{"OUT1x Driver", "Headphone", "OUT1x Config"},

	{"Left DAC Enable", NULL, "OUT1x Driver"},
	{"Left DAC Power", NULL, "Left DAC Enable"},

	{"OUT1", NULL, "Left DAC Power"},
};

static const struct snd_soc_dapm_route taa5x12_dapm_routes[] = {
	/* ADC channel2 */
	{"CH2_ADC_EN", NULL, "AIN2"},

	{"ASI_TX_CH2_EN", "Capture Switch", "CH2_ADC_EN"},

	{"ADC2 Config", "Differential", "ASI_TX_CH2_EN"},
	{"ADC2 Config", "Single-ended", "ASI_TX_CH2_EN"},
	{"ADC2 Full-Scale", "2/10-VRMS",
		"ADC2 Config"},
	{"ADC2 Full-Scale", "4/5-VRMS",
		"ADC2 Config"},

	{"Mic Bias", NULL, "ADC2 Full-Scale"},
};

static const struct snd_soc_dapm_route tad5x12_dapm_routes[] = {
	/* Right Output */
	{"ASI_RX_CH2_EN", "Switch", "ASI IN2"},

	{"OUT2x Source", "DAC + Analog Bypass Mix", "ASI_RX_CH1_EN"},
	{"OUT2x Source", "DAC -> OUTxP, INxP -> OUTxM", "ASI_RX_CH1_EN"},
	{"OUT2x Source", "INxM -> OUTxP, DAC -> OUTxM", "ASI_RX_CH1_EN"},

	{"OUT2x Config", "Differential", "OUT2x Source"},
	// {"OUT2x Config", "Stereo Single-ended", "OUT2x Source"},
	{"OUT2x Config", "Mono Single-ended at OUTxP only", "OUT2x Source"},
	{"OUT2x Config", "Mono Single-ended at OUTxM only", "OUT2x Source"},
	{"OUT2x Config", "Pseudo differential with OUTxM as VCOM",
		"OUT2x Source"},
	{"OUT2x Config", "Pseudo differential with OUTxP as VCOM",
		"OUT2x Source"},

	{"OUT2x Driver", "Line-out", "OUT2x Config"},
	{"OUT2x Driver", "Headphone", "OUT2x Config"},

	{"Right DAC Enable", NULL, "OUT2x Driver"},

	{"Right DAC Power", NULL, "Right DAC Enable"},

	{"OUT2", NULL, "Right DAC Power"},
};

static const struct snd_soc_dapm_route tac5x1x_dapm_pdm_routes[] = {
	/* PDM channel1 & Channel2 */
	{"IN1 Source Mux", "PDM", "DIN1"},
	{"IN2 Source Mux", "PDM", "DIN2"},

	{"ASI_TX_CH1_EN", "Capture Switch",
		"IN1 Source Mux"},
	{"ASI_TX_CH2_EN", "Capture Switch",
		"IN2 Source Mux"},

	{"CH1_PDM_EN", NULL, "ASI_TX_CH1_EN"},
	{"CH2_PDM_EN", NULL, "ASI_TX_CH2_EN"},

	{"PDM ch1 & ch2 Datain Select", "GPIO1", "CH1_PDM_EN"},
	{"PDM ch1 & ch2 Datain Select", "GPIO2", "CH1_PDM_EN"},
	{"PDM ch1 & ch2 Datain Select", "GPI1", "CH1_PDM_EN"},

	{"ADC Power", NULL, "PDM ch1 & ch2 Datain Select"},

	/* PDM channel3 & Channel4 */
	{"ASI_TX_CH3_EN", "Capture Switch", "DIN3"},
	{"ASI_TX_CH4_EN", "Capture Switch", "DIN4"},

	{"CH3_PDM_EN", NULL, "ASI_TX_CH3_EN"},
	{"CH4_PDM_EN", NULL, "ASI_TX_CH4_EN"},

	{"PDM ch3 & ch4 Datain Select", "GPIO1", "CH3_PDM_EN"},
	{"PDM ch3 & ch4 Datain Select", "GPIO2", "CH3_PDM_EN"},
	{"PDM ch3 & ch4 Datain Select", "GPI1", "CH3_PDM_EN"},

	{"ADC Power", NULL, "PDM ch3 & ch4 Datain Select"},
	{"AIF OUT", NULL, "ADC Power"},
};

static const struct snd_soc_dapm_route tac5x1x_common_dapm_routes[] = {
	{"ADC Power", NULL, "Mic Bias"},
	{"AIF OUT", NULL, "ADC Power"},
};

static const struct reg_default tac5x1x_reg_defaults[] = {
	{TAC5X1X_PGSEL, 0x00},
	{TAC5X1X_INT, 0x10},
	{TAC5X1X_ADCCH1C0, 0x54},
	{TAC5X1X_ADCCH2C0, 0x54},
	{TAC5X1X_OUT1CFG0, 0x28},
	{TAC5X1X_OUT2CFG0, 0x28},
	{TAC5X1X_CH_EN, 0x00},
	{TAC5X1X_PASITXCH1, 0x20},
	{TAC5X1X_PASITXCH2, 0x30},
	{TAC5X1X_PASIRXCH1, 0x20},
	{TAC5X1X_PASIRXCH2, 0x30},
	{},
};

static s32 tac5x1x_pwr_ctrl(struct snd_soc_component *component,
			    bool power_state)
{
	struct tac5x1x_priv *tac5x1x =
		snd_soc_component_get_drvdata(component);
	s32 active_ctrl, ret;
	s32 pwr_ctrl = 0;

	if (power_state) {
		active_ctrl = TAC5X1X_VREF_SLEEP_ACTIVE_MASK;
		snd_soc_component_update_bits(component, TAC5X1X_VREFCFG,
					      TAC5X1X_VREFCFG_MICBIAS_VAL_MASK,
					      tac5x1x->micbias_vg << 2);
		snd_soc_component_update_bits(component, TAC5X1X_VREFCFG,
					      TAC5X1X_VREFCFG_VREF_FSCALE_MASK,
					      tac5x1x->vref_vg);

		if (tac5x1x->uad_en)
			pwr_ctrl |= TAC5X1X_PWR_CFG_UAD_EN;
		if (tac5x1x->vad_en)
			pwr_ctrl |= TAC5X1X_PWR_CFG_VAD_EN;
		if (tac5x1x->uag_en)
			pwr_ctrl |= TAC5X1X_PWR_CFG_UAG_EN;
	} else {
		active_ctrl = 0x0;
	}

	ret = snd_soc_component_update_bits(component, TAC5X1X_VREF,
					    TAC5X1X_VREF_SLEEP_EXIT_VREF_EN |
					    TAC5X1X_VREF_SLEEP_ACTIVE_MASK, active_ctrl);
	if (ret < 0) {
		dev_err(tac5x1x->dev,
			"%s, device active or sleep failed!, ret %d/n",
			__func__, ret);
		return ret;
	}

	ret = snd_soc_component_update_bits(component, TAC5X1X_PWR_CFG,
					    TAC5X1X_PWR_CFG_UAD_EN | TAC5X1X_PWR_CFG_UAG_EN |
					    TAC5X1X_PWR_CFG_VAD_EN, pwr_ctrl);
	if (ret < 0)
		dev_err(tac5x1x->dev,
			"%s, Power control set failed!, ret %d/n",
			__func__, ret);
	return ret;
}

static s32 tac5x1x_set_dai_fmt(struct snd_soc_dai *codec_dai, u32 fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	s32 iface_reg_1 = 0;
	s32 iface_reg_2 = 0;
	s32 iface_reg_3 = 0;

	dev_err(component->dev, "[tac5x1x] %s(), fmt=%d\n", __func__, fmt);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBP_CFP:
		iface_reg_1 |= TAC5X1X_PASI_MODE_MASK;
		break;
	case SND_SOC_DAIFMT_CBC_CFC:
		break;
	default:
		dev_err(component->dev,
			"%s: invalid DAI master/slave interface\n",
			__func__);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface_reg_2 |= TAC5X1X_PASI_FMT_I2S;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface_reg_2 |= TAC5X1X_PASI_FMT_TDM;
		iface_reg_3 |= BIT(0); /* add offset 1 */
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface_reg_2 |= TAC5X1X_PASI_FMT_TDM;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface_reg_2 |= TAC5X1X_PASI_FMT_LJ;
		break;
	default:
		dev_err(component->dev,
			"%s: invalid DAI interface format\n", __func__);
		return -EINVAL;
	}

	snd_soc_component_update_bits(component, TAC5X1X_CNTCLK2,
				      TAC5X1X_PASI_MODE_MASK, iface_reg_1);
	snd_soc_component_update_bits(component, TAC5X1X_PASI0,
				      TAC5X1X_PASI_FMT_MASK, iface_reg_2);
	snd_soc_component_update_bits(component, TAC5X1X_PASITX1,
				      TAC5X1X_PASITX_OFFSET_MASK, iface_reg_3);
	snd_soc_component_update_bits(component, TAC5X1X_PASIRX0,
				      TAC5X1X_PASIRX_OFFSET_MASK, iface_reg_3);
	return 0;
}

static s32 tac5x1x_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct tac5x1x_priv *tac5x1x =
		snd_soc_component_get_drvdata(component);
	s32 sample_rate, word_length = 0;

	switch (params_rate(params)) {
	case 24000:
		sample_rate = 25;
		break;
	case 32000:
		sample_rate = 23;
		break;
	case 44100:
	case 48000:
		sample_rate = 20;
		break;
	case 64000:
		sample_rate = 18;
		break;
	case 96000:
		sample_rate = 15;
		break;
	case 192000:
		sample_rate = 10;
		break;
	default:
		/* Auto detect sample rate */
		sample_rate = 0;
		break;
	}

	switch (params_physical_width(params)) {
	case 16:
		word_length |= TAC5X1X_WORD_LEN_16BITS;
		break;
	case 20:
		word_length |= TAC5X1X_WORD_LEN_20BITS;
		break;
	case 24:
		word_length |= TAC5X1X_WORD_LEN_24BITS;
		break;
	case 32:
		word_length |= TAC5X1X_WORD_LEN_32BITS;
		break;
	default:
		dev_err(tac5x1x->dev, "%s, set word length failed\n",
			__func__);
		return -EINVAL;
	}

	snd_soc_component_update_bits(component, TAC5X1X_CLK0, TAC5X1X_PASI_SAMP_RATE_MASK,
				      sample_rate << 2);
	snd_soc_component_update_bits(component, TAC5X1X_PASI0,
				      TAC5X1X_PASI_DATALEN_MASK, word_length);

	tac5x1x_pwr_ctrl(component, true);
	return 0;
}

static s32 tac5x1x_set_bias_level(struct snd_soc_component *component,
				  enum snd_soc_bias_level level)
{
	s32 ret;
	struct tac5x1x_priv *tac5x1x = snd_soc_component_get_drvdata(component);

	switch (level) {
	case SND_SOC_BIAS_ON:
		ret = tac5x1x_pwr_ctrl(component, true);
		if (ret < 0)
			dev_err(tac5x1x->dev, "%s, power up failed!/n", __func__);
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		break;
	case SND_SOC_BIAS_OFF:
		ret = tac5x1x_pwr_ctrl(component, false);
		if (ret < 0)
			dev_err(tac5x1x->dev, "%s, power down failed!/n", __func__);
		break;
	}

	return ret;
}

static const struct snd_soc_dai_ops tac5x1x_ops = {
	.hw_params = tac5x1x_hw_params,
	.set_fmt = tac5x1x_set_dai_fmt,
	.no_capture_mute = 1,
};

static struct snd_soc_dai_driver tac5x1x_dai = {
	.name = "tac5x1x-hifi",
	.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 4,
			.rates = TAC5X1X_RATES,
			.formats = TAC5X1X_FORMATS,},
	.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = TAC5X1X_RATES,
			.formats = TAC5X1X_FORMATS,
			},
	.ops = &tac5x1x_ops,
	.symmetric_rate = 1,
};

static struct snd_soc_dai_driver taa5x1x_dai = {
	.name = "taa5x1x-hifi",
	.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = TAC5X1X_RATES,
			.formats = TAC5X1X_FORMATS,
			},
	.ops = &tac5x1x_ops,
	.symmetric_rate = 1,
};

static struct snd_soc_dai_driver tad5x1x_dai = {
	.name = "tad5x1x-hifi",
	.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 4,
			.rates = TAC5X1X_RATES,
			.formats = TAC5X1X_FORMATS,
			},
	.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = TAC5X1X_RATES,
			.formats = TAC5X1X_FORMATS,
			},
	.ops = &tac5x1x_ops,
	.symmetric_rate = 1,
};

static void tac5x1x_setup_gpios(struct snd_soc_component *component)
{
	struct tac5x1x_priv *tac5x1x =
		snd_soc_component_get_drvdata(component);
	s32 *gpio_drive = tac5x1x->gpio_setup->gpio_drive;
	s32 *gpio_func = tac5x1x->gpio_setup->gpio_func;

	/* setup GPIO functions */
	/* GPIO1 */
	if (gpio_func[0] <= TAC5X1X_GPIO_DAISY_OUT) {
		snd_soc_component_update_bits(component, TAC5X1X_GPIO1, TAC5X1X_GPIOX_CFG_MASK,
					      gpio_func[0] << 4);
		snd_soc_component_update_bits(component, TAC5X1X_GPIO1, TAC5X1X_GPIOX_DRV_MASK,
					      gpio_drive[0]);

		if (gpio_func[0] == TAC5X1X_GPIO_GPI)
			snd_soc_add_component_controls(component, tac5x1x_GPIO1_I,
						       ARRAY_SIZE(tac5x1x_GPIO1_I));
		else if (gpio_func[0] == TAC5X1X_GPIO_GPO)
			snd_soc_add_component_controls(component, tac5x1x_GPIO1_O,
						       ARRAY_SIZE(tac5x1x_GPIO1_O));
	}
	/* GPIO2 */
	if (gpio_func[1] <= TAC5X1X_GPIO_DAISY_OUT) {
		snd_soc_component_update_bits(component, TAC5X1X_GPIO2, TAC5X1X_GPIOX_CFG_MASK,
					      gpio_func[1] << 4);
		snd_soc_component_update_bits(component, TAC5X1X_GPIO2, TAC5X1X_GPIOX_DRV_MASK,
					      gpio_drive[1]);

		if (gpio_func[1] == TAC5X1X_GPIO_GPI)
			snd_soc_add_component_controls(component, tac5x1x_GPIO2_I,
						       ARRAY_SIZE(tac5x1x_GPIO2_I));
		else if (gpio_func[1] == TAC5X1X_GPIO_GPO)
			snd_soc_add_component_controls(component, tac5x1x_GPIO2_O,
						       ARRAY_SIZE(tac5x1x_GPIO2_O));
	}
	/* GPO1 */
	if (gpio_func[2] <= TAC5X1X_GPIO_DAISY_OUT) {
		snd_soc_component_update_bits(component, TAC5X1X_GPO1, TAC5X1X_GPIOX_CFG_MASK,
					      gpio_func[2] << 4);
		snd_soc_component_update_bits(component, TAC5X1X_GPO1, TAC5X1X_GPIOX_DRV_MASK,
					      gpio_drive[2]);

		if (gpio_func[2] == TAC5X1X_GPIO_GPO)
			snd_soc_add_component_controls(component, tac5x1x_GPO1,
						       ARRAY_SIZE(tac5x1x_GPO1));
	}
	/* GPI1 */
	if (tac5x1x->gpio_setup->gpi1_func) {
		snd_soc_component_update_bits(component, TAC5X1X_GPI1, TAC5X1X_GPI1_CFG_MASK,
					      TAC5X1X_GPI1_CFG_MASK);
		snd_soc_add_component_controls(component, tac5x1x_GPI1, ARRAY_SIZE(tac5x1x_GPI1));
	}
	/*GPA GPIO*/
	if (tac5x1x->gpio_setup->gpa_gpio)
		snd_soc_component_update_bits(component, TAC5X1X_INTF5, TAC5X1X_GPA_CFG_MASK,
					      TAC5X1X_GPA_CFG_MASK);
}

static s32 tac5x1x_reset(struct snd_soc_component *component)
{
	s32 ret, index;

	ret = snd_soc_component_write(component, TAC5X1X_RESET, 1);
	if (ret < 0)
		return ret;
	/* Wait >= 10 ms after entering sleep mode. */
	usleep_range(10000, 100000);

	for (index = 0; index < ARRAY_SIZE(tac5x1x_reg_defaults); index++) {
		ret = snd_soc_component_write(component, tac5x1x_reg_defaults[index].reg,
					      tac5x1x_reg_defaults[index].def);
		if (ret < 0)
			return ret;
	}
	return ret;
}

static s32 tac5x1x_add_controls(struct snd_soc_component *component)
{
	struct tac5x1x_priv *tac5x1x =
		snd_soc_component_get_drvdata(component);
	s32 *gpio_func = tac5x1x->gpio_setup->gpio_func;
	s32 ret;

	switch (tac5x1x->codec_type) {
	case TAA5212:
		ret = snd_soc_add_component_controls(component, tac5x12_impedance_controls,
						     ARRAY_SIZE(tac5x12_impedance_controls));
		if (ret)
			return ret;
		fallthrough;
	case TAA5412:
		ret = snd_soc_add_component_controls(component, tac5x12_tolerance_controls,
						     ARRAY_SIZE(tac5x12_tolerance_controls));
		if (ret)
			return ret;
		ret = snd_soc_add_component_controls(component, taa5x1x_ip_controls,
						     ARRAY_SIZE(taa5x1x_ip_controls));
		if (ret)
			return ret;
		break;
	/* For Mono */
	case TAC5111:
	case TAC5211:
		ret = snd_soc_add_component_controls(component, tac5x11_controls,
						     ARRAY_SIZE(tac5x11_controls));
		if (ret)
			return ret;
	fallthrough;
	case TAC5311:
	case TAC5411:
		ret = snd_soc_add_component_controls(component, tad5x1x_controls,
						     ARRAY_SIZE(tad5x1x_controls));
		if (ret)
			return ret;
		break;
	/* For Stereo */
	case TAC5112:
	case TAC5212:
		ret = snd_soc_add_component_controls(component, tac5x12_impedance_controls,
						     ARRAY_SIZE(tac5x12_impedance_controls));
		if (ret)
			return ret;
		fallthrough;
	case TAC5312:
	case TAC5412:
		ret = snd_soc_add_component_controls(component, tac5x12_tolerance_controls,
						     ARRAY_SIZE(tac5x12_tolerance_controls));
		if (ret)
			return ret;
		ret = snd_soc_add_component_controls(component, tad5x1x_controls,
						     ARRAY_SIZE(tad5x1x_controls));
		if (ret)
			return ret;

		ret = snd_soc_add_component_controls(component, taa5x12_controls,
						     ARRAY_SIZE(taa5x12_controls));
		if (ret)
			return ret;

		ret = snd_soc_add_component_controls(component, tad5x12_controls,
						     ARRAY_SIZE(tad5x12_controls));
		if (ret)
			return ret;
		break;
	case TAD5212:
	case TAD5112:
		ret = snd_soc_add_component_controls(component, tad5x12_controls,
						     ARRAY_SIZE(tad5x12_controls));
		if (ret)
			return ret;

		ret = snd_soc_add_component_controls(component, tad5x1x_pdm_controls,
						     ARRAY_SIZE(tad5x1x_pdm_controls));
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	/* If enabled PDM GPIO*/
	if (memchr(gpio_func, TAC5X1X_GPIO_PDMCLK,
		   sizeof(*gpio_func) / sizeof(gpio_func[0]))) {
		ret = snd_soc_add_component_controls(component, tac5x1x_pdm_controls,
						     ARRAY_SIZE(tac5x1x_pdm_controls));
		if (ret)
			return ret;
	}

	return 0;
}

static s32 tac5x1x_add_ip_diag_controls(struct snd_soc_component *component)
{
	struct tac5x1x_priv *tac5x1x =
		snd_soc_component_get_drvdata(component);
	s32 ret;

	switch (tac5x1x->codec_type) {
	case TAA5212:
		break;
	case TAA5412:
	case TAC5311:
	case TAC5312:
	case TAC5411:
	case TAC5412:
		if (tac5x1x->input_diag_config.in_ch_en) {
			ret = snd_soc_add_component_controls(component, taa5x1x_ip_controls,
							     ARRAY_SIZE(taa5x1x_ip_controls));
			if (ret)
				return ret;
			snd_soc_component_update_bits(component, TAC5X1X_DIAG_CFG0,
						      TAC5X1X_IN_CH_DIAG_EN_MASK,
						      TAC5X1X_IN_CH_DIAG_EN_MASK);
		}
		if (tac5x1x->input_diag_config.out_ch_en) {
			snd_soc_component_update_bits(component, TAC5X1X_DIAG_CFG0,
						      TAC5X1X_OUT1P_DIAG_EN_MASK,
						      TAC5X1X_OUT1P_DIAG_EN_MASK);
		}
		if (tac5x1x->input_diag_config.incl_se_inm) {
			snd_soc_component_update_bits(component, TAC5X1X_DIAG_CFG0,
						      TAC5X1X_INCL_SE_INM_MASK,
						      TAC5X1X_INCL_SE_INM_MASK);
		}
		if (tac5x1x->input_diag_config.incl_ac_coup) {
			snd_soc_component_update_bits(component, TAC5X1X_DIAG_CFG0,
						      TAC5X1X_INCL_AC_COUP_MASK,
						      TAC5X1X_INCL_AC_COUP_MASK);
		}
		snd_soc_component_update_bits(component, TAC5X1X_DIAG_CFG7, 0xff,
					      tac5x1x->micbias_threshold[0]);
		snd_soc_component_update_bits(component, TAC5X1X_DIAG_CFG6, 0xff,
					      tac5x1x->micbias_threshold[1]);
		ret = tac5x1x_register_interrupt(tac5x1x);
		if (!ret)
			dev_info(tac5x1x->dev, "Interrupt registration Done");
		fallthrough;
	case TAC5111:
	case TAC5112:
	case TAC5211:
	case TAC5212:
	case TAD5112:
	case TAD5212:
		snd_soc_component_update_bits(component, TAC5X1X_DIAG_CFG9, 0xff,
					      tac5x1x->gpa_threshold[0]);
		snd_soc_component_update_bits(component, TAC5X1X_DIAG_CFG8, 0xff,
					      tac5x1x->gpa_threshold[1]);
		break;
	default:
		break;
	};

	return ret;
};

static s32 tac5x1x_add_widgets(struct snd_soc_component *component)
{
	struct tac5x1x_priv *tac5x1x = snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(component);
	s32 *gpio_func = tac5x1x->gpio_setup->gpio_func;
	s32 ret;

	switch (tac5x1x->codec_type) {
	case TAC5111:
	case TAC5211:
	case TAC5311:
	case TAC5411:
		ret = snd_soc_dapm_new_controls(dapm, tad5x1x_dapm_widgets,
						ARRAY_SIZE(tad5x1x_dapm_widgets));
		if (ret)
			return ret;
		ret = snd_soc_dapm_add_routes(dapm, tad5x1x_dapm_routes,
					      ARRAY_SIZE(tad5x1x_dapm_routes));
		if (ret)
			return ret;
		break;
	case TAC5112:
	case TAC5212:
	case TAC5312:
	case TAC5412:
		ret = snd_soc_dapm_new_controls(dapm, tad5x1x_dapm_widgets,
						ARRAY_SIZE(tad5x1x_dapm_widgets));
		if (ret)
			return ret;
		ret = snd_soc_dapm_add_routes(dapm, tad5x1x_dapm_routes,
					      ARRAY_SIZE(tad5x1x_dapm_routes));
		if (ret)
			return ret;
		ret = snd_soc_dapm_new_controls(dapm, tad5x12_dapm_widgets,
						ARRAY_SIZE(tad5x12_dapm_widgets));
		if (ret)
			return ret;
		ret = snd_soc_dapm_add_routes(dapm, tad5x12_dapm_routes,
					      ARRAY_SIZE(tad5x12_dapm_routes));
		if (ret)
			return ret;
		fallthrough;
	case TAA5212:
	case TAA5412:
		ret = snd_soc_dapm_new_controls(dapm, taa5x12_dapm_widgets,
						ARRAY_SIZE(taa5x12_dapm_widgets));
		if (ret)
			return ret;
		ret = snd_soc_dapm_add_routes(dapm, taa5x12_dapm_routes,
					      ARRAY_SIZE(taa5x12_dapm_routes));
		if (ret)
			return ret;
		break;
	case TAD5212:
	case TAD5112:
		ret = snd_soc_dapm_new_controls(dapm, tad5xx_dapm_widgets,
						ARRAY_SIZE(tad5xx_dapm_widgets));
		if (ret)
			return ret;

		ret = snd_soc_dapm_new_controls(dapm, tad5x12_dapm_widgets,
						ARRAY_SIZE(tad5x12_dapm_widgets));
		if (ret)
			return ret;
		ret = snd_soc_dapm_add_routes(dapm, tad5x12_dapm_routes,
					      ARRAY_SIZE(tad5x12_dapm_routes));
		if (ret)
			return ret;

		break;
	default:
		break;
	}

	if (!(tac5x1x->codec_type == TAD5212 || tac5x1x->codec_type == TAD5112)) {
		ret = snd_soc_dapm_new_controls(dapm, tac5x1x_common_dapm_widgets,
						ARRAY_SIZE(tac5x1x_common_dapm_widgets));
		if (ret)
			return ret;

		ret = snd_soc_dapm_add_routes(dapm, tac5x1x_common_dapm_routes,
					      ARRAY_SIZE(tac5x1x_common_dapm_routes));
		if (ret)
			return ret;
	} else {
		ret = snd_soc_dapm_new_controls(dapm, tad5x1x_common_dapm_widgets,
						ARRAY_SIZE(tad5x1x_common_dapm_widgets));
		if (ret)
			return ret;
	}
	/* If enabled PDM GPIO*/
	if (memchr(gpio_func, TAC5X1X_GPIO_PDMCLK,
		   sizeof(*gpio_func) / sizeof(gpio_func[0]))) {
		ret = snd_soc_dapm_new_controls(dapm, tac5x1x_dapm_pdm_widgets,
						ARRAY_SIZE(tac5x1x_dapm_pdm_widgets));
		if (ret)
			return ret;
		ret = snd_soc_dapm_add_routes(dapm, tac5x1x_dapm_pdm_routes,
					      ARRAY_SIZE(tac5x1x_dapm_pdm_routes));
		if (ret)
			return ret;
	}
	return 0;
}

static s32 tac5x1x_component_probe(struct snd_soc_component *component)
{
	s32 ret;
	struct tac5x1x_priv *tac5x1x = snd_soc_component_get_drvdata(component);

	ret = tac5x1x_add_controls(component);
	if (ret < 0) {
		dev_err(tac5x1x->dev, "%s, add control failed\n", __func__);
		return ret;
	}

	ret = tac5x1x_add_widgets(component);
	if (ret < 0) {
		dev_err(tac5x1x->dev, "%s, device widget addition failed\n", __func__);
		return ret;
	}

	ret = tac5x1x_reset(component);
	if (ret < 0) {
		dev_err(tac5x1x->dev, "%s, device reset failed\n", __func__);
		return ret;
	}

	if (tac5x1x->gpio_setup)
		tac5x1x_setup_gpios(component);

	ret = tac5x1x_add_ip_diag_controls(component);
	if (ret < 0) {
		dev_err(tac5x1x->dev, "%s, add Ip diag control failed\n", __func__);
		return ret;
	}
	return ret;
}

#ifdef CONFIG_PM
static s32 tac5x1x_soc_suspend(struct snd_soc_component *component)
{
	struct tac5x1x_priv *tac5x1x =
		snd_soc_component_get_drvdata(component);

	regcache_cache_only(tac5x1x->regmap, true);
	regcache_mark_dirty(tac5x1x->regmap);

	regulator_disable(tac5x1x->supply_avdd);
	regulator_disable(tac5x1x->supply_iovdd);

	return 0;
}

static s32 tac5x1x_soc_resume(struct snd_soc_component *component)
{
	struct tac5x1x_priv *tac5x1x =
		snd_soc_component_get_drvdata(component);
	s32 ret;

	regcache_cache_only(tac5x1x->regmap, false);
	snd_soc_component_cache_sync(component);

	ret = regulator_enable(tac5x1x->supply_avdd);
	if (ret) {
		regulator_disable(tac5x1x->supply_avdd);
		return ret;
	}

	ret = regulator_enable(tac5x1x->supply_iovdd);
	if (ret) {
		regulator_disable(tac5x1x->supply_iovdd);
		return ret;
	}
	return 0;
}
#else
#define tac5x1x_soc_suspend	NULL
#define tac5x1x_soc_resume	NULL
#endif /* CONFIG_PM */

static const struct snd_soc_component_driver soc_component_dev_tac5x1x = {
	.probe			= tac5x1x_component_probe,
	.set_bias_level		= tac5x1x_set_bias_level,
	.suspend		= tac5x1x_soc_suspend,
	.resume			= tac5x1x_soc_resume,
	.controls		= taa5x1x_controls,
	.num_controls		= ARRAY_SIZE(taa5x1x_controls),
	.dapm_widgets		= taa5x1x_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(taa5x1x_dapm_widgets),
	.dapm_routes		= taa5x1x_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(taa5x1x_dapm_routes),
	.suspend_bias_off	= 1,
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
};

static const struct snd_soc_component_driver soc_component_dev_taa5x1x = {
	.probe			= tac5x1x_component_probe,
	.set_bias_level		= tac5x1x_set_bias_level,
	.suspend		= tac5x1x_soc_suspend,
	.resume			= tac5x1x_soc_resume,
	.controls		= taa5x1x_controls,
	.num_controls		= ARRAY_SIZE(taa5x1x_controls),
	.dapm_widgets		= taa5x1x_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(taa5x1x_dapm_widgets),
	.dapm_routes		= taa5x1x_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(taa5x1x_dapm_routes),
	.suspend_bias_off	= 1,
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
};

static const struct snd_soc_component_driver soc_component_dev_tad5x1x = {
	.probe			= tac5x1x_component_probe,
	.set_bias_level		= tac5x1x_set_bias_level,
	.suspend		= tac5x1x_soc_suspend,
	.resume			= tac5x1x_soc_resume,
	.controls		= tad5x1x_controls,
	.num_controls		= ARRAY_SIZE(tad5x1x_controls),
	.dapm_widgets		= tad5x1x_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(tad5x1x_dapm_widgets),
	.dapm_routes		= tad5x1x_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(tad5x1x_dapm_routes),
	.suspend_bias_off	= 1,
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
};

static s32 tac5x1x_parse_dt(struct tac5x1x_priv *tac5x1x,
			    struct device_node *np)
{
	struct tac5x1x_input_diag_config input_config;
	struct tac5x1x_setup_gpio *tac5x1x_setup;
	s32 micbias_value = TAC5X1X_MICBIAS_VREF;
	s32 vref_value = TAC5X1X_VERF_2_5V;
	s32 ret;

	tac5x1x_setup = devm_kzalloc(tac5x1x->dev, sizeof(*tac5x1x_setup),
				     GFP_KERNEL);
	if (!tac5x1x_setup)
		return -ENOMEM;

	ret = fwnode_property_read_u32(tac5x1x->dev->fwnode, "ti,vref", &vref_value);
	if (ret) {
		dev_err(tac5x1x->dev, "Fail to get verf E:%d\n", ret);
		goto out;
	}
	ret = fwnode_property_read_u32(tac5x1x->dev->fwnode, "ti,micbias-vg", &micbias_value);
	if (ret) {
		dev_err(tac5x1x->dev, "Fail to get micbias-vg E:%d\n", ret);
		goto out;
	}

	if (micbias_value == TAC5X1X_MICBIAS_AVDD) {
		tac5x1x->micbias_vg = micbias_value;
		tac5x1x->vref_vg = TAC5X1X_VERF_2_75V;
		tac5x1x->micbias_en = true;
	} else {
		switch (vref_value) {
		case TAC5X1X_VERF_2_75V:
		case TAC5X1X_VERF_2_5V:
			switch (micbias_value) {
			case TAC5X1X_MICBIAS_VREF:
			case TAC5X1X_MICBIAS_0_5VREF:
				tac5x1x->micbias_vg = micbias_value;
				break;
			default:
				dev_err(tac5x1x->dev, "Bad tac5x1x-micbias-vg value %d\n",
					micbias_value);
				tac5x1x->micbias_vg = TAC5X1X_MICBIAS_AVDD;
				break;
			}
			tac5x1x->vref_vg = vref_value;
			tac5x1x->micbias_en = true;
			break;
		case TAC5X1X_VERF_1_375V:
			if (micbias_value == TAC5X1X_MICBIAS_VREF) {
				tac5x1x->micbias_vg = micbias_value;
			} else {
				dev_err(tac5x1x->dev, "Bad tac5x1x-micbias-vg value %d\n",
					micbias_value);
				tac5x1x->micbias_vg = TAC5X1X_MICBIAS_AVDD;
			}
			tac5x1x->micbias_en = true;
			tac5x1x->vref_vg = vref_value;
			break;
		default:
			dev_err(tac5x1x->dev, "Bad tac5x1x-vref-vg value %d\n", vref_value);
			tac5x1x->vref_vg = TAC5X1X_VERF_2_5V;
			tac5x1x->micbias_vg = TAC5X1X_MICBIAS_AVDD;
			tac5x1x->micbias_en = true;
			break;
		}
	}

	if (fwnode_property_read_u32(tac5x1x->dev->fwnode, "ti,gpi1-func",
				     &tac5x1x_setup->gpi1_func))
		dev_err(tac5x1x->dev, "%s, Fail to get gpi1-func value\n", __func__);

	if (fwnode_property_read_u32(tac5x1x->dev->fwnode, "ti,gpa-gpio",
				     &tac5x1x_setup->gpa_gpio))
		dev_err(tac5x1x->dev, "%s, Fail to get gpa-gpio value\n", __func__);

	if (fwnode_property_read_u32_array(tac5x1x->dev->fwnode, "ti,gpios-func",
					   tac5x1x_setup->gpio_func, 3))
		dev_err(tac5x1x->dev, "%s, Fail to get gpios-func value\n", __func__);
	if (fwnode_property_read_u32_array(tac5x1x->dev->fwnode, "ti,gpios-drive",
					   tac5x1x_setup->gpio_drive, 3))
		dev_err(tac5x1x->dev, "%s, Fail to get gpios-drive value\n", __func__);

	tac5x1x->gpa_threshold[0] = TAC5X1X_GPA_LOW_THRESHOLD;
	tac5x1x->gpa_threshold[1] = TAC5X1X_GPA_HIGH_THRESHOLD;
	if (fwnode_property_read_u32_array(tac5x1x->dev->fwnode, "ti,gpa-threshold",
					   tac5x1x->gpa_threshold, 2))
		dev_err(tac5x1x->dev, "%s, Fail to get ti,gpa-threshold value\n", __func__);

	tac5x1x->gpio_setup = tac5x1x_setup;

	switch (tac5x1x->codec_type) {
	case TAA5212:
	case TAC5111:
	case TAC5112:
	case TAC5211:
	case TAC5212:
	case TAD5112:
	case TAD5212:
		break;
	case TAA5412:
	case TAC5411:
	case TAC5412:
	case TAC5311:
	case TAC5312:
		tac5x1x->input_diag_config.in_ch_en = 0;
		if (fwnode_property_read_u32(tac5x1x->dev->fwnode, "ti,in-ch-en",
					     &input_config.in_ch_en))
			dev_err(tac5x1x->dev, "%s, Fail to get ti,in-ch-en value\n", __func__);
		tac5x1x->input_diag_config.out_ch_en = 0;
		if (fwnode_property_read_u32(tac5x1x->dev->fwnode,
					     "ti,out-ch-en", &input_config.in_ch_en))
			dev_err(tac5x1x->dev, "%s, Fail to get ti,out-ch-en value\n", __func__);
		tac5x1x->input_diag_config.incl_se_inm = 0;
		if (fwnode_property_read_u32(tac5x1x->dev->fwnode, "ti,incl-se-inm",
					     &input_config.incl_se_inm))
			dev_err(tac5x1x->dev, "%s, Fail to get ti,incl-se-inm value\n", __func__);
		tac5x1x->input_diag_config.incl_ac_coup = 0;
		if (fwnode_property_read_u32(tac5x1x->dev->fwnode,
					     "ti,incl-ac-coup", &input_config.incl_ac_coup))
			dev_err(tac5x1x->dev, "%s, Fail to get ti,incl-ac-coup value\n", __func__);
		tac5x1x->input_diag_config = input_config;

		tac5x1x->micbias_threshold[0] = TAC5X1X_MICBIAS_LOW_THRESHOLD;
		tac5x1x->micbias_threshold[1] =
					TAC5X1X_MICBIAS_HIGH_THRESHOLD;
		if (fwnode_property_read_u32_array(tac5x1x->dev->fwnode, "ti,micbias-threshold",
						   tac5x1x->micbias_threshold, 2))
			dev_err(tac5x1x->dev, "%s, Fail to get ti,micbias-threshold value\n",
				__func__);
		break;
	}
out:
	return ret;
}

static void tac5x1x_disable_regulators(struct tac5x1x_priv *tac5x1x)
{
	if (!IS_ERR(tac5x1x->supply_iovdd))
		regulator_disable(tac5x1x->supply_iovdd);

	if (!IS_ERR(tac5x1x->supply_avdd))
		regulator_disable(tac5x1x->supply_avdd);
}

static s32 tac5x1x_setup_regulators(struct device *dev,
				    struct tac5x1x_priv *tac5x1x)
{
	s32 ret;

	tac5x1x->supply_iovdd = devm_regulator_get(dev, "iovdd");
	tac5x1x->supply_avdd = devm_regulator_get(dev, "avdd");

	/* Check if the regulator requirements are fulfilled */
	if (IS_ERR(tac5x1x->supply_iovdd)) {
		dev_err(dev, "Missing supply 'iovdd'\n");
		return PTR_ERR(tac5x1x->supply_iovdd);
	}

	if (IS_ERR(tac5x1x->supply_avdd)) {
		dev_err(dev, "Missing supply 'avdd'\n");
		return PTR_ERR(tac5x1x->supply_avdd);
	}

	ret = regulator_enable(tac5x1x->supply_iovdd);
	if (ret) {
		dev_err(dev, "Failed to enable regulator iovdd\n");
		regulator_disable(tac5x1x->supply_iovdd);
		return ret;
	}

	ret = regulator_enable(tac5x1x->supply_avdd);
	if (ret) {
		dev_err(dev, "Failed to enable regulator avdd\n");
		regulator_disable(tac5x1x->supply_avdd);
		return ret;
	}

	return 0;
}

s32 tac5x1x_probe(struct device *dev, struct regmap *regmap,
		  enum tac5x1x_type type)
{
	struct device_node *np = dev->of_node;
	struct tac5x1x_priv *tac5x1x;
	s32 ret;

	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	tac5x1x = devm_kzalloc(dev, sizeof(struct tac5x1x_priv),
			       GFP_KERNEL);
	if (!tac5x1x)
		return -ENOMEM;

	tac5x1x->dev = dev;
	tac5x1x->codec_type = type;
	tac5x1x->regmap = regmap;
	tac5x1x->ndev = 1;

	dev_set_drvdata(dev, tac5x1x);
	if (np) {
		ret = tac5x1x_parse_dt(tac5x1x, np);
		if (ret) {
			dev_err(dev, "Failed to parse DT node\n");
			return ret;
		}
	} else {
		dev_err(dev, "%s: Fail to get device node\n", __func__);
	}

	ret = tac5x1x_setup_regulators(dev, tac5x1x);
	if (ret) {
		dev_err(dev, "Failed to setup regulators\n");
		return ret;
	}

	switch (tac5x1x->codec_type) {
	case TAA5212:
	case TAA5412:
		ret = devm_snd_soc_register_component(dev,
						      &soc_component_dev_taa5x1x, &taa5x1x_dai, 1);
		if (ret) {
			dev_err(dev, "Failed to register component\n");
			goto err_disable_regulators;
		}
		break;
	case TAC5111:
	case TAC5112:
	case TAC5211:
	case TAC5212:
	case TAC5311:
	case TAC5312:
	case TAC5411:
	case TAC5412:
		ret = devm_snd_soc_register_component(dev,
						      &soc_component_dev_tac5x1x, &tac5x1x_dai, 1);
		if (ret) {
			dev_err(dev, "Failed to register component\n");
			goto err_disable_regulators;
		}
		break;
	case TAD5112:
	case TAD5212:
		ret = devm_snd_soc_register_component(dev,
						      &soc_component_dev_tad5x1x, &tad5x1x_dai, 1);
		if (ret) {
			dev_err(dev, "Failed to register component\n");
			goto err_disable_regulators;
		}
		break;
	}
	return 0;

err_disable_regulators:
	tac5x1x_disable_regulators(tac5x1x);

	return ret;
}
EXPORT_SYMBOL(tac5x1x_probe);

s32 tac5x1x_remove(struct device *dev)
{
	struct tac5x1x_priv *tac5x1x = dev_get_drvdata(dev);

	tac5x1x_disable_regulators(tac5x1x);
	if (tac5x1x->irq_work_func) {
		dev_info(tac5x1x->dev, "Cancelled IRQ\n");
		free_irq(tac5x1x->irqinfo.irq, tac5x1x);
		mutex_destroy(&tac5x1x->dev_lock);
	}
	return 0;
}
EXPORT_SYMBOL(tac5x1x_remove);

const struct of_device_id tac5x1x_of_match[] = {
	{ .compatible = "ti,taa5212", .data = (void *)TAA5212 },
	{ .compatible = "ti,taa5412", .data = (void *)TAA5412 },
	{ .compatible = "ti,tac5111", .data = (void *)TAC5111 },
	{ .compatible = "ti,tac5112", .data = (void *)TAC5112 },
	{ .compatible = "ti,tac5211", .data = (void *)TAC5211 },
	{ .compatible = "ti,tac5212", .data = (void *)TAC5212 },
	{ .compatible = "ti,tac5311", .data = (void *)TAC5311 },
	{ .compatible = "ti,tac5312", .data = (void *)TAC5312 },
	{ .compatible = "ti,tac5411", .data = (void *)TAC5411 },
	{ .compatible = "ti,tac5412", .data = (void *)TAC5412 },
	{ .compatible = "ti,tad5112", .data = (void *)TAD5112 },
	{ .compatible = "ti,tad5212", .data = (void *)TAD5212 },
	{}
};
EXPORT_SYMBOL_GPL(tac5x1x_of_match);
MODULE_DEVICE_TABLE(of, tac5x1x_of_match);

MODULE_DESCRIPTION("ASoC tac5x1x codec driver");
MODULE_AUTHOR("Texas Instruments");
MODULE_LICENSE("GPL");
