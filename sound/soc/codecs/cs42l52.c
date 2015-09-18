/*
 * cs42l52.c -- CS42L52 ALSA SoC audio driver
 *
 * Copyright 2012 CirrusLogic, Inc.
 *
 * Author: Georgi Vlaev <joe@nucleusys.com>
 * Author: Brian Austin <brian.austin@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/cs42l52.h>
#include "cs42l52.h"


struct sp_config {
	u8 spc, format, spfs;
	u32 srate;
};

struct cs42l52_private {
	struct regmap *regmap;
	struct snd_soc_codec *codec;
	struct device *dev;
	struct sp_config config;
	struct cs42l52_platform_data pdata;
	u32 sysclk;
	u8 mclksel;
	u32 mclk;
	u8 flags;
	struct input_dev *beep;
	struct work_struct beep_work;
	int beep_rate;
};

static const struct reg_default cs42l52_reg_defaults[] = {
	{ CS42L52_PWRCTL1, 0x9F },	/* r02 PWRCTL 1 */
	{ CS42L52_PWRCTL2, 0x07 },	/* r03 PWRCTL 2 */
	{ CS42L52_PWRCTL3, 0xFF },	/* r04 PWRCTL 3 */
	{ CS42L52_CLK_CTL, 0xA0 },	/* r05 Clocking Ctl */
	{ CS42L52_IFACE_CTL1, 0x00 },	/* r06 Interface Ctl 1 */
	{ CS42L52_IFACE_CTL2, 0x00 },	/* r07 Interface Ctl 2 */
	{ CS42L52_ADC_PGA_A, 0x80 },	/* r08 Input A Select */
	{ CS42L52_ADC_PGA_B, 0x80 },	/* r09 Input B Select */
	{ CS42L52_ANALOG_HPF_CTL, 0xA5 },	/* r0A Analog HPF Ctl */
	{ CS42L52_ADC_HPF_FREQ, 0x00 },	/* r0B ADC HPF Corner Freq */
	{ CS42L52_ADC_MISC_CTL, 0x00 },	/* r0C Misc. ADC Ctl */
	{ CS42L52_PB_CTL1, 0x60 },	/* r0D Playback Ctl 1 */
	{ CS42L52_MISC_CTL, 0x02 },	/* r0E Misc. Ctl */
	{ CS42L52_PB_CTL2, 0x00 },	/* r0F Playback Ctl 2 */
	{ CS42L52_MICA_CTL, 0x00 },	/* r10 MICA Amp Ctl */
	{ CS42L52_MICB_CTL, 0x00 },	/* r11 MICB Amp Ctl */
	{ CS42L52_PGAA_CTL, 0x00 },	/* r12 PGAA Vol, Misc. */
	{ CS42L52_PGAB_CTL, 0x00 },	/* r13 PGAB Vol, Misc. */
	{ CS42L52_PASSTHRUA_VOL, 0x00 },	/* r14 Bypass A Vol */
	{ CS42L52_PASSTHRUB_VOL, 0x00 },	/* r15 Bypass B Vol */
	{ CS42L52_ADCA_VOL, 0x00 },	/* r16 ADCA Volume */
	{ CS42L52_ADCB_VOL, 0x00 },	/* r17 ADCB Volume */
	{ CS42L52_ADCA_MIXER_VOL, 0x80 },	/* r18 ADCA Mixer Volume */
	{ CS42L52_ADCB_MIXER_VOL, 0x80 },	/* r19 ADCB Mixer Volume */
	{ CS42L52_PCMA_MIXER_VOL, 0x00 },	/* r1A PCMA Mixer Volume */
	{ CS42L52_PCMB_MIXER_VOL, 0x00 },	/* r1B PCMB Mixer Volume */
	{ CS42L52_BEEP_FREQ_ONTIME, 0x00 },	/* r1C Beep Freq on Time */
	{ CS42L52_BEEP_VOL_OFFTIME, 0x00 },	/* r1D Beep Volume off Time */
	{ CS42L52_BEEP_TONE_CTL, 0x00 },	/* r1E Beep Tone Cfg. */
	{ CS42L52_TONE_CTL, 0x88 },	/* r1F Tone Ctl */
	{ CS42L52_MSTA_VOL, 0x00 },	/* r20 Master A Volume */
	{ CS42L52_MSTB_VOL, 0x00 },	/* r21 Master B Volume */
	{ CS42L52_HPA_VOL, 0x00 },	/* r22 Headphone A Volume */
	{ CS42L52_HPB_VOL, 0x00 },	/* r23 Headphone B Volume */
	{ CS42L52_SPKA_VOL, 0x00 },	/* r24 Speaker A Volume */
	{ CS42L52_SPKB_VOL, 0x00 },	/* r25 Speaker B Volume */
	{ CS42L52_ADC_PCM_MIXER, 0x00 },	/* r26 Channel Mixer and Swap */
	{ CS42L52_LIMITER_CTL1, 0x00 },	/* r27 Limit Ctl 1 Thresholds */
	{ CS42L52_LIMITER_CTL2, 0x7F },	/* r28 Limit Ctl 2 Release Rate */
	{ CS42L52_LIMITER_AT_RATE, 0xC0 },	/* r29 Limiter Attack Rate */
	{ CS42L52_ALC_CTL, 0x00 },	/* r2A ALC Ctl 1 Attack Rate */
	{ CS42L52_ALC_RATE, 0x3F },	/* r2B ALC Release Rate */
	{ CS42L52_ALC_THRESHOLD, 0x00 },	/* r2C ALC Thresholds */
	{ CS42L52_NOISE_GATE_CTL, 0x00 },	/* r2D Noise Gate Ctl */
	{ CS42L52_CLK_STATUS, 0x00 },	/* r2E Overflow and Clock Status */
	{ CS42L52_BATT_COMPEN, 0x00 },	/* r2F battery Compensation */
	{ CS42L52_BATT_LEVEL, 0x00 },	/* r30 VP Battery Level */
	{ CS42L52_SPK_STATUS, 0x00 },	/* r31 Speaker Status */
	{ CS42L52_TEM_CTL, 0x3B },	/* r32 Temp Ctl */
	{ CS42L52_THE_FOLDBACK, 0x00 },	/* r33 Foldback */
	{ CS42L52_CHARGE_PUMP, 0x5F },	/* r34 Charge Pump Frequency */
};

static bool cs42l52_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS42L52_CHIP:
	case CS42L52_PWRCTL1:
	case CS42L52_PWRCTL2:
	case CS42L52_PWRCTL3:
	case CS42L52_CLK_CTL:
	case CS42L52_IFACE_CTL1:
	case CS42L52_IFACE_CTL2:
	case CS42L52_ADC_PGA_A:
	case CS42L52_ADC_PGA_B:
	case CS42L52_ANALOG_HPF_CTL:
	case CS42L52_ADC_HPF_FREQ:
	case CS42L52_ADC_MISC_CTL:
	case CS42L52_PB_CTL1:
	case CS42L52_MISC_CTL:
	case CS42L52_PB_CTL2:
	case CS42L52_MICA_CTL:
	case CS42L52_MICB_CTL:
	case CS42L52_PGAA_CTL:
	case CS42L52_PGAB_CTL:
	case CS42L52_PASSTHRUA_VOL:
	case CS42L52_PASSTHRUB_VOL:
	case CS42L52_ADCA_VOL:
	case CS42L52_ADCB_VOL:
	case CS42L52_ADCA_MIXER_VOL:
	case CS42L52_ADCB_MIXER_VOL:
	case CS42L52_PCMA_MIXER_VOL:
	case CS42L52_PCMB_MIXER_VOL:
	case CS42L52_BEEP_FREQ_ONTIME:
	case CS42L52_BEEP_VOL_OFFTIME:
	case CS42L52_BEEP_TONE_CTL:
	case CS42L52_TONE_CTL:
	case CS42L52_MSTA_VOL:
	case CS42L52_MSTB_VOL:
	case CS42L52_HPA_VOL:
	case CS42L52_HPB_VOL:
	case CS42L52_SPKA_VOL:
	case CS42L52_SPKB_VOL:
	case CS42L52_ADC_PCM_MIXER:
	case CS42L52_LIMITER_CTL1:
	case CS42L52_LIMITER_CTL2:
	case CS42L52_LIMITER_AT_RATE:
	case CS42L52_ALC_CTL:
	case CS42L52_ALC_RATE:
	case CS42L52_ALC_THRESHOLD:
	case CS42L52_NOISE_GATE_CTL:
	case CS42L52_CLK_STATUS:
	case CS42L52_BATT_COMPEN:
	case CS42L52_BATT_LEVEL:
	case CS42L52_SPK_STATUS:
	case CS42L52_TEM_CTL:
	case CS42L52_THE_FOLDBACK:
	case CS42L52_CHARGE_PUMP:
		return true;
	default:
		return false;
	}
}

static bool cs42l52_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS42L52_IFACE_CTL2:
	case CS42L52_CLK_STATUS:
	case CS42L52_BATT_LEVEL:
	case CS42L52_SPK_STATUS:
	case CS42L52_CHARGE_PUMP:
		return 1;
	default:
		return 0;
	}
}

static int cs42l52_snd_soc_get_bits(struct snd_soc_codec *codec,
		unsigned short reg, unsigned int mask, unsigned int shift)
{
	return (snd_soc_read(codec, reg) & mask) >> shift;
}

static int cs42l52_snd_soc_update_bits(struct snd_soc_codec *codec,
		unsigned short reg, unsigned int mask, unsigned int shift,
		unsigned int value)
{
	return snd_soc_update_bits(codec, reg, mask, value << shift);
}

static int cs42l52_snd_soc_test_bits(struct snd_soc_codec *codec,
		unsigned short reg, unsigned int mask, unsigned int shift,
		unsigned int value)
{
	return snd_soc_test_bits(codec, reg, mask, value << shift);
}

static int cs42l52_get_master_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		val = cs42l52_snd_soc_get_bits(codec, reg[i],
						CS42L52_MSTX_VOL_MASK, CS42L52_MSTX_VOL_SHIFT);

		/* out of range */
		if (val > CS42L52_MSTX_VOL_PLUS_12DB
			&& val < CS42L52_MSTX_VOL_MINUS_102DB)
			ucontrol->value.integer.value[i] = 0;
		else
			ucontrol->value.integer.value[i] = ((char)val + 
					CS42L52_MSTX_VOL_MAX - CS42L52_MSTX_VOL_PLUS_12DB) & 0xff;
	}

	return 0;
}

static int cs42l52_set_master_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	int change = 0;
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		/* out of range */
		if (ucontrol->value.integer.value[i] > CS42L52_MSTX_VOL_MAX)
			val = CS42L52_MSTX_VOL_PLUS_12DB;
		else
			val = (ucontrol->value.integer.value[i] & 0xff) -
				(CS42L52_MSTX_VOL_MAX - CS42L52_MSTX_VOL_PLUS_12DB);

		if (cs42l52_snd_soc_test_bits(codec, reg[i], CS42L52_MSTX_VOL_MASK,
						CS42L52_MSTX_VOL_SHIFT, val))
		{
			change = 1;
			cs42l52_snd_soc_update_bits(codec, reg[i],
						CS42L52_MSTX_VOL_MASK, CS42L52_MSTX_VOL_SHIFT,
						val);
		}
	}

	return change;
}

static int cs42l52_get_hp_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		val = cs42l52_snd_soc_get_bits(codec, reg[i], CS42L52_HPX_VOL_MASK,
						CS42L52_HPX_VOL_SHIFT);

		/* register value equals 0.0db */
		if (val == CS42L52_HPX_VOL_0DB)
			ucontrol->value.integer.value[i] = CS42L52_HPX_VOL_MAX;

		/* register value equals from -96.0db to -0.5db */
		else if (val >= CS42L52_SPKX_VOL_MINUS_96DB)
			ucontrol->value.integer.value[i] = val -
				CS42L52_SPKX_VOL_MINUS_96DB;

		/* register value equals from -96.0db to -96.0db */
		else
			ucontrol->value.integer.value[i] = 0;
	}

	return 0;
}

static int cs42l52_set_hp_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	int change = 0;
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		/* mixer value equals 0.0db */
		if (ucontrol->value.integer.value[i] >= CS42L52_HPX_VOL_MAX)
			val = CS42L52_HPX_VOL_0DB;

		/* mixer value equals from -96.0db to -0.5db */
		else
			val = ucontrol->value.integer.value[i] +
					CS42L52_HPX_VOL_MINUS_96DB;

		if (cs42l52_snd_soc_test_bits(codec, reg[i], CS42L52_HPX_VOL_MASK,
						CS42L52_HPX_VOL_SHIFT, val))
		{
			change = 1;
			cs42l52_snd_soc_update_bits(codec, reg[i], CS42L52_HPX_VOL_MASK,
						CS42L52_HPX_VOL_SHIFT, val);
		}
	}

	return change;
}

static int cs42l52_get_spk_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		val = cs42l52_snd_soc_get_bits(codec, reg[i], CS42L52_SPKX_VOL_MASK,
						CS42L52_SPKX_VOL_SHIFT);

		/* register value equals 0.0db */
		if (val == CS42L52_SPKX_VOL_0DB)
			ucontrol->value.integer.value[i] = CS42L52_SPKX_VOL_MAX;

		/* register value equals from -96.0db to -0.5db */
		else if (val >= CS42L52_SPKX_VOL_MINUS_96DB)
			ucontrol->value.integer.value[i] = val -
				CS42L52_SPKX_VOL_MINUS_96DB;

		/* register value equals from -96.0db to -96.0db */
		else
			ucontrol->value.integer.value[i] = 0;
	}

	return 0;
}

static int cs42l52_set_spk_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	int change = 0;
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		/* mixer value equals 0.0db */
		if (ucontrol->value.integer.value[i] >= CS42L52_SPKX_VOL_MAX)
			val = CS42L52_SPKX_VOL_0DB;

		/* mixer value equals from -96.0db to -0.5db */
		else
			val = ucontrol->value.integer.value[i] +
					CS42L52_SPKX_VOL_MINUS_96DB;

		if (cs42l52_snd_soc_test_bits(codec, reg[i], CS42L52_SPKX_VOL_MASK,
						CS42L52_SPKX_VOL_SHIFT, val))
		{
			change = 1;
			cs42l52_snd_soc_update_bits(codec, reg[i], CS42L52_SPKX_VOL_MASK,
						CS42L52_SPKX_VOL_SHIFT, val);
		}
	}

	return change;
}

static int cs42l52_get_beep_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	u8 val;

	val = cs42l52_snd_soc_get_bits(codec, reg,
					CS42L52_BEEP_VOL_OFFTIME_VOL_MASK,
					CS42L52_BEEP_VOL_OFFTIME_VOL_SHIFT);

	/* register value equals from -56.0db to -8.0db */
	if (val >= CS42L52_BEEP_VOL_OFFTIME_VOL_MINUS_56DB)
		ucontrol->value.integer.value[0] = val -
			CS42L52_BEEP_VOL_OFFTIME_VOL_MINUS_56DB;

	/* register value equals from -6.0db to +6.0db */
	else
		ucontrol->value.integer.value[0] = CS42L52_BEEP_VOL_OFFTIME_VOL_MAX -
			CS42L52_BEEP_VOL_OFFTIME_VOL_MINUS_56DB + 1 + val;

	return 0;
}

static int cs42l52_set_beep_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	int change = 0;
	u8 val;

	/* out of range */
	if (ucontrol->value.integer.value[0] > CS42L52_BEEP_VOL_OFFTIME_VOL_MAX)
		val = CS42L52_BEEP_VOL_OFFTIME_VOL_PLUS_6DB;

	/* mixer value equals from -6.0db to +6.0db */
	else if (ucontrol->value.integer.value[0] >
				(CS42L52_BEEP_VOL_OFFTIME_VOL_MAX -
				 CS42L52_BEEP_VOL_OFFTIME_VOL_MINUS_56DB))
		val = ucontrol->value.integer.value[0] -
				(CS42L52_BEEP_VOL_OFFTIME_VOL_MAX -
				CS42L52_BEEP_VOL_OFFTIME_VOL_MINUS_56DB + 1);

	/* mixer value equals from -56.0db to -8.0db */
	else
		val = ucontrol->value.integer.value[0] +
				CS42L52_BEEP_VOL_OFFTIME_VOL_MINUS_56DB;

	if (cs42l52_snd_soc_test_bits(codec, reg,
					CS42L52_BEEP_VOL_OFFTIME_VOL_MASK,
					CS42L52_BEEP_VOL_OFFTIME_VOL_SHIFT, val))
	{
		change = 1;
		cs42l52_snd_soc_update_bits(codec, reg,
					CS42L52_BEEP_VOL_OFFTIME_VOL_MASK,
					CS42L52_BEEP_VOL_OFFTIME_VOL_SHIFT, val);
	}

	return change;
}

static int cs42l52_get_pga_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		val = cs42l52_snd_soc_get_bits(codec, reg[i],
						CS42L52_PGAX_CTL_VOL_MASK, CS42L52_PGAX_CTL_VOL_SHIFT);

		/* register value equals from 0.0db to +12.0db */
		if (val <= CS42L52_PGAX_CTL_VOL_PLUS_12DB)
			ucontrol->value.integer.value[i] = val +
				(CS42L52_PGAX_CTL_VOL_MAX - CS42L52_PGAX_CTL_VOL_PLUS_12DB);

		/* register value equals from -6.0db to -0.5db */
		else if (val >= CS42L52_PGAX_CTL_VOL_MINUS_6DB)
			ucontrol->value.integer.value[i] = val -
				CS42L52_PGAX_CTL_VOL_MINUS_6DB;

		/* register value equals from +12.0db to +12.0db */
		else if (val <= CS42L52_PGAX_CTL_VOL_PLUS_12DB_UPPER)
			ucontrol->value.integer.value[i] = CS42L52_PGAX_CTL_VOL_MAX;

		/* register value equals from -6.0db to -6.0db */
		else /* if (val >= CS42L52_PGAX_CTL_VOL_MINUS_6DB_LOWER) */
			ucontrol->value.integer.value[i] = 0;
	}

	return 0;
}

static int cs42l52_set_pga_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	int change = 0;
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		/* mixer value equals from 0.0db to +12.0db */
		if (ucontrol->value.integer.value[i] >=
			(CS42L52_PGAX_CTL_VOL_MAX - CS42L52_PGAX_CTL_VOL_PLUS_12DB))
			val = ucontrol->value.integer.value[i] -
				(CS42L52_PGAX_CTL_VOL_MAX - CS42L52_PGAX_CTL_VOL_PLUS_12DB);

		/* mixer value equals from -6.0db to -0.5db */
		else
			val = ucontrol->value.integer.value[i] +
					CS42L52_PGAX_CTL_VOL_MINUS_6DB;

		if (cs42l52_snd_soc_test_bits(codec, reg[i],
						CS42L52_PGAX_CTL_VOL_MASK,
						CS42L52_PGAX_CTL_VOL_SHIFT, val))
		{
			change = 1;
			cs42l52_snd_soc_update_bits(codec, reg[i],
						CS42L52_PGAX_CTL_VOL_MASK,
						CS42L52_PGAX_CTL_VOL_SHIFT, val);
		}
	}

	return change;
}

static int cs42l52_get_passthru_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		val = cs42l52_snd_soc_get_bits(codec, reg[i],
						CS42L52_PASSTHRUX_VOL_MASK,
						CS42L52_PASSTHRUX_VOL_SHIFT);

		ucontrol->value.integer.value[i] = ((char)val +
						CS42L52_PASSTHRUX_VOL_MAX -
						CS42L52_PASSTHRUX_VOL_PLUS_12DB) & 0xff;
	}

	return 0;
}

static int cs42l52_set_passthru_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	int change = 0;
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		/* out of range */
		if (ucontrol->value.integer.value[i] > CS42L52_PASSTHRUX_VOL_MAX)
			val = CS42L52_PASSTHRUX_VOL_PLUS_12DB;
		else
			val = (ucontrol->value.integer.value[i] & 0xff) -
				(CS42L52_PASSTHRUX_VOL_MAX - CS42L52_PASSTHRUX_VOL_PLUS_12DB);

		if (cs42l52_snd_soc_test_bits(codec, reg[i],
						CS42L52_PASSTHRUX_VOL_MASK,
						CS42L52_PASSTHRUX_VOL_SHIFT, val))
		{
			change = 1;
			cs42l52_snd_soc_update_bits(codec, reg[i],
						CS42L52_PASSTHRUX_VOL_MASK,
						CS42L52_PASSTHRUX_VOL_SHIFT, val);
		}
	}

	return change;
}

static int cs42l52_get_adc_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		val = cs42l52_snd_soc_get_bits(codec, reg[i],
						CS42L52_ADCX_VOL_MASK, CS42L52_ADCX_VOL_SHIFT);

		ucontrol->value.integer.value[i] = ((char)val +
						CS42L52_ADCX_VOL_MAX -
						CS42L52_ADCX_VOL_PLUS_24DB) & 0xff;
	}

	return 0;
}

static int cs42l52_set_adc_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	int change = 0;
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		/* out of range */
		if (ucontrol->value.integer.value[i] > CS42L52_ADCX_VOL_MAX)
			val = CS42L52_ADCX_VOL_PLUS_24DB;
		else
			val = (ucontrol->value.integer.value[i] & 0xff) -
				(CS42L52_ADCX_VOL_MAX - CS42L52_ADCX_VOL_PLUS_24DB);

		if (cs42l52_snd_soc_test_bits(codec, reg[i],
						CS42L52_ADCX_VOL_MASK,
						CS42L52_ADCX_VOL_SHIFT, val))
		{
			change = 1;
			cs42l52_snd_soc_update_bits(codec, reg[i],
						CS42L52_ADCX_VOL_MASK,
						CS42L52_ADCX_VOL_SHIFT, val);
		}
	}

	return change;
}

static int cs42l52_get_adc_mixer_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		val = cs42l52_snd_soc_get_bits(codec, reg[i],
						CS42L52_ADCX_MIXER_VOL_MASK, CS42L52_ADCX_MIXER_VOL_SHIFT);

		/* register value equals from 0.0db to +12.0db */
		if (val <= CS42L52_ADCX_MIXER_VOL_PLUS_12DB)
			ucontrol->value.integer.value[i] = val +
				(CS42L52_ADCX_MIXER_VOL_MAX - CS42L52_ADCX_MIXER_VOL_PLUS_12DB);

		/* register value equals from -51.5db to -0.5db */
		else /* if (val >= CS42L52_ADCX_MIXER_VOL_MINUS_51_5DB) */
			ucontrol->value.integer.value[i] = val -
				CS42L52_ADCX_MIXER_VOL_MINUS_51_5DB;
	}

	return 0;
}

static int cs42l52_set_adc_mixer_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	int change = 0;
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		/* mixer value equals from 0.0db to +12.0db */
		if (ucontrol->value.integer.value[i] >=
			(CS42L52_ADCX_MIXER_VOL_MAX - CS42L52_ADCX_MIXER_VOL_PLUS_12DB))
			val = ucontrol->value.integer.value[i] -
				(CS42L52_ADCX_MIXER_VOL_MAX - CS42L52_ADCX_MIXER_VOL_PLUS_12DB);

		/* mixer value equals from -51.5db to -0.5db */
		else
			val = ucontrol->value.integer.value[i] +
					CS42L52_ADCX_MIXER_VOL_MINUS_51_5DB;

		if (cs42l52_snd_soc_test_bits(codec, reg[i],
						CS42L52_ADCX_MIXER_VOL_MASK,
						CS42L52_ADCX_MIXER_VOL_SHIFT, val))
		{
			change = 1;
			cs42l52_snd_soc_update_bits(codec, reg[i],
						CS42L52_ADCX_MIXER_VOL_MASK,
						CS42L52_ADCX_MIXER_VOL_SHIFT, val);
		}
	}

	return change;
}

static int cs42l52_get_pcm_mixer_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		val = cs42l52_snd_soc_get_bits(codec, reg[i],
						CS42L52_PCMX_MIXER_VOL_MASK, CS42L52_PCMX_MIXER_VOL_SHIFT);

		/* register value equals from 0.0db to +12.0db */
		if (val <= CS42L52_PCMX_MIXER_VOL_PLUS_12DB)
			ucontrol->value.integer.value[i] = val +
				(CS42L52_PCMX_MIXER_VOL_MAX - CS42L52_PCMX_MIXER_VOL_PLUS_12DB);

		/* register value equals from -51.5db to -0.5db */
		else /* if (val >= CS42L52_PCMX_MIXER_VOL_MINUS_51_5DB) */
			ucontrol->value.integer.value[i] = val -
				CS42L52_PCMX_MIXER_VOL_MINUS_51_5DB;
	}

	return 0;
}

static int cs42l52_set_pcm_mixer_volume(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg[2];
	int change = 0;
	u8 val, i;

	reg[0] = mc->reg;
	reg[1] = mc->rreg;

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {

		/* mixer value equals from 0.0db to +12.0db */
		if (ucontrol->value.integer.value[i] >=
			(CS42L52_PCMX_MIXER_VOL_MAX - CS42L52_PCMX_MIXER_VOL_PLUS_12DB))
			val = ucontrol->value.integer.value[i] -
				(CS42L52_PCMX_MIXER_VOL_MAX - CS42L52_PCMX_MIXER_VOL_PLUS_12DB);

		/* mixer value equals from -51.5db to -0.5db */
		else
			val = ucontrol->value.integer.value[i] +
					CS42L52_PCMX_MIXER_VOL_MINUS_51_5DB;

		if (cs42l52_snd_soc_test_bits(codec, reg[i],
						CS42L52_PCMX_MIXER_VOL_MASK,
						CS42L52_PCMX_MIXER_VOL_SHIFT, val))
		{
			change = 1;
			cs42l52_snd_soc_update_bits(codec, reg[i],
						CS42L52_PCMX_MIXER_VOL_MASK,
						CS42L52_PCMX_MIXER_VOL_SHIFT, val);
		}
	}

	return change;
}

/* 
 * TLV for master volume
 * -102.0db to +12.0db, step .5db
 * 0001 1000 - +12.0db
 * ...
 * 0000 0000 -  0.0db
 * 1111 1111 - -0.5db
 * 1111 1110 - -1.0db
 * ...
 * 0011 0100 - -102.0db
 * ...
 * 0001 1001 - -102.0db
 */
static DECLARE_TLV_DB_SCALE(master_tlv, -10200, 50, 0);
/* 
 * TLV for headphone volume
 * -96.0db to 0.0db, step .5db
 * 0000 0000 -  0.0db
 * 1111 1111 - -0.5db
 * 1111 1110 - -1.0db
 * ...
 * 0100 0000 - -96.0db
 * ...
 * 0000 0010 - -96.0db
 * 0000 0001 - muted
 */
static DECLARE_TLV_DB_SCALE(hp_tlv, -9600, 50, 1);

/* 
 * TLV for speaker volume
 * -96.0db to 0.0db, step .5db
 * 0000 0000 -  0.0db
 * 1111 1111 - -0.5db
 * 1111 1110 - -1.0db
 * ...
 * 0100 0000 - -96.0db
 * ...
 * 0000 0010 - -96.0db
 * 0000 0001 - muted
 */
static DECLARE_TLV_DB_SCALE(spk_tlv, -9600, 50, 1);

/* 
 * TLV for passthrough a/b volume
 * -60.0db to +12.0db, step .5db
 * 0111 1111 - +12.0db
 * ...
 * 0001 1000 - +12.0db
 * ...
 * 0000 0001 - +0.5db
 * 0000 0000 -  0.0db
 * 1111 1111 - -0.5db
 * ...
 * 1000 1000 - -60.0db
 * ...
 * 1000 0000 - -60.0db
 */
static DECLARE_TLV_DB_SCALE(passthru_tlv, -6000, 50, 0);

/* 
 * TLV for ADC a/b volume
 * -96.0db to +24.0db, step 1.0db
 * 0111 1111 - +24.0db
 * ...
 * 0001 1000 - +24.0db
 * ...
 * 0000 0001 - +1.0db
 * 0000 0000 -  0.0db
 * 1111 1111 - -1.0db
 * ...
 * 1010 0000 - -96.0db
 * ...
 * 1000 0000 - -96.0db
 */
static DECLARE_TLV_DB_SCALE(adc_tlv, -9600, 100, 0);

/* 
 * TLV for ADC/PCM MIXER a/b volume
 * -51.5db to +12.0db, step 0.5db
 * 001 1000 - +12.0db
 * ...
 * 000 0001 - +0.5db
 * 000 0000 -  0.0db
 * 111 1111 - -0.5db
 * ...
 * 001 1001 - -51.5db
 */
static DECLARE_TLV_DB_SCALE(mixer_tlv, -5150, 50, 0);

/* 
 * TLV for MIC GAIN a/b volume
 * +16.0db to +32.0db, step 1.0db
 * 1 1111 - +32.0db
 * ...
 * 1 0000 - +32.0db
 * 0 1111 - +31.0db
 * 0 1110 - +30.0db
 * ...
 * 0 0000 - +16.0db
 */
static DECLARE_TLV_DB_SCALE(mic_gain_tlv, 1600, 100, 0);

/* 
 * TLV for PGA a/b volume
 * -6.0db to +12.0db, step .5db
 * 01 1111 - +12.0db
 * ...
 * 01 1000 - +12.0db
 * ...
 * 00 0001 - +0.5db
 * 00 0000 -  0.0db
 * 11 1111 - -0.5db
 * ...
 * 10 1000 - -6.0db
 * ...
 * 10 0000 - -6.0db
 */
static DECLARE_TLV_DB_SCALE(pga_tlv, -600, 50, 0);

/* 
 * TLV for beep
 * -56.0db to +6.0db, step 2.0db
 * 00110 - +6.0db
 * ...
 * 00000 - -6.0db
 * 11111 - -8.0db
 * 11110 - -10.0db
 * ...
 * 00111 - -56.0db
 */
static DECLARE_TLV_DB_SCALE(beep_tlv, -5600, 200, 0);

/* 
 * TLV for tone of treble/bass gain
 * -10.5db to +12.0db, step 1.5db
 * 0000 - +12.0db
 * ...
 * 0111 - +1.5db
 * 1000 -  0.0db
 * 1001 - -1.5db
 * ...
 * 1111 - -10.5db
 */
static DECLARE_TLV_DB_SCALE(tone_gain_tlv, -1050, 150, 0);

static const unsigned int limiter_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 2, TLV_DB_SCALE_ITEM(-3000, 600, 0),
	3, 7, TLV_DB_SCALE_ITEM(-1200, 300, 0),
};

static const char * const adca_mux_text[] = {
	"Input1A", "Input2A", "Input3A", "Input4A", "PGA Input Left"};

static const char * const adcb_mux_text[] = {
	"Input1B", "Input2B", "Input3B", "Input4B", "PGA Input Right"};

static SOC_ENUM_SINGLE_DECL(adca_mux_enum,
				CS42L52_ADC_PGA_A,
				CS42L52_ADC_PGA_X_ADC_SEL_SHIFT,
				adca_mux_text);

static SOC_ENUM_SINGLE_DECL(adcb_mux_enum,
				CS42L52_ADC_PGA_B,
				CS42L52_ADC_PGA_X_ADC_SEL_SHIFT,
				adcb_mux_text);

static const struct snd_kcontrol_new adca_mux =
	SOC_DAPM_ENUM("Route", adca_mux_enum);

static const struct snd_kcontrol_new adcb_mux =
	SOC_DAPM_ENUM("Route", adcb_mux_enum);

static const char * const mic_bias_level_text[] = {
	"0.5 +VA", "0.6 +VA", "0.7 +VA",
	"0.8 +VA", "0.83 +VA", "0.91 +VA"
};

static SOC_ENUM_SINGLE_DECL(mic_bias_level_enum,
				CS42L52_IFACE_CTL2,
				CS42L52_IFACE_CTL2_BIAS_LVL_SHIFT,
				mic_bias_level_text);

static const char * const cs42l52_mic_text[] = { "MIC1", "MIC2" };

static SOC_ENUM_SINGLE_DECL(mica_enum,
				CS42L52_MICA_CTL,
				CS42L52_MICX_CTL_CFG_SHIFT,
				cs42l52_mic_text);

static SOC_ENUM_SINGLE_DECL(micb_enum,
				CS42L52_MICB_CTL,
				CS42L52_MICX_CTL_CFG_SHIFT,
				cs42l52_mic_text);

static const char * const digital_output_mux_text[] = {"ADC", "DSP"};

static SOC_ENUM_SINGLE_DECL(digital_output_mux_enum,
				CS42L52_ADC_MISC_CTL,
				CS42L52_ADC_MISC_CTL_DIG_MUX_SHIFT,
				digital_output_mux_text);

static const struct snd_kcontrol_new digital_output_mux =
	SOC_DAPM_ENUM("Route", digital_output_mux_enum);

static const char * const hp_gain_num_text[] = {
	"0.3959", "0.4571", "0.5111", "0.6047",
	"0.7099", "0.8399", "1.000", "1.1430"
};

static SOC_ENUM_SINGLE_DECL(hp_gain_enum,
				CS42L52_PB_CTL1,
				CS42L52_PB_CTL1_HP_GAIN_SHIFT,
				hp_gain_num_text);

static const char * const beep_pitch_text[] = {
	"C4", "C5", "D5", "E5", "F5", "G5", "A5", "B5",
	"C6", "D6", "E6", "F6", "G6", "A6", "B6", "C7"
};

static SOC_ENUM_SINGLE_DECL(beep_pitch_enum,
				CS42L52_BEEP_FREQ_ONTIME,
				CS42L52_BEEP_FREQ_ONTIME_FREQ_SHIFT,
				beep_pitch_text);

static const char * const beep_ontime_text[] = {
	"86 ms", "430 ms", "780 ms", "1.20 s", "1.50 s",
	"1.80 s", "2.20 s", "2.50 s", "2.80 s", "3.20 s",
	"3.50 s", "3.80 s", "4.20 s", "4.50 s", "4.80 s", "5.20 s"
};

static SOC_ENUM_SINGLE_DECL(beep_ontime_enum,
				CS42L52_BEEP_FREQ_ONTIME,
				CS42L52_BEEP_FREQ_ONTIME_ONTIME_SHIFT,
				beep_ontime_text);

static const char * const beep_offtime_text[] = {
	"1.23 s", "2.58 s", "3.90 s", "5.20 s",
	"6.60 s", "8.05 s", "9.35 s", "10.80 s"
};

static SOC_ENUM_SINGLE_DECL(beep_offtime_enum,
				CS42L52_BEEP_VOL_OFFTIME,
				CS42L52_BEEP_VOL_OFFTIME_OFFTIME_SHIFT,
				beep_offtime_text);

static const char * const beep_config_text[] = {
	"Off", "Single", "Multiple", "Continuous"
};

static SOC_ENUM_SINGLE_DECL(beep_config_enum,
				CS42L52_BEEP_TONE_CTL,
				CS42L52_BEEP_TONE_CTL_BEEP_CFG_SHIFT,
				beep_config_text);

static const char * const beep_bass_text[] = {
	"50 Hz", "100 Hz", "200 Hz", "250 Hz"
};

static SOC_ENUM_SINGLE_DECL(beep_bass_enum,
				CS42L52_BEEP_TONE_CTL,
				CS42L52_BEEP_TONE_CTL_BASS_CORNER_FREQ_SHIFT,
				beep_bass_text);

static const char * const beep_treble_text[] = {
	"5 kHz", "7 kHz", "10 kHz", " 15 kHz"
};

static SOC_ENUM_SINGLE_DECL(beep_treble_enum,
				CS42L52_BEEP_TONE_CTL,
				CS42L52_BEEP_TONE_CTL_TREBLE_CORNER_FREQ_SHIFT,
				beep_treble_text);

static const char * const ng_threshold_text[] = {
	"-34dB", "-37dB", "-40dB", "-43dB",
	"-46dB", "-52dB", "-58dB", "-64dB"
};

static SOC_ENUM_SINGLE_DECL(ng_threshold_enum,
				CS42L52_NOISE_GATE_CTL,
				CS42L52_NOISE_GATE_CTL_THRESHOLD_SHIFT,
				ng_threshold_text);

static const char * const cs42l52_ng_delay_text[] = {
	"50ms", "100ms", "150ms", "200ms"};

static SOC_ENUM_SINGLE_DECL(ng_delay_enum,
				CS42L52_NOISE_GATE_CTL,
				CS42L52_NOISE_GATE_CTL_DELAY_SHIFT,
				cs42l52_ng_delay_text);

static const char * const cs42l52_ng_type_text[] = {
	"Apply Specific", "Apply All"
};

static SOC_ENUM_SINGLE_DECL(ng_type_enum,
				CS42L52_NOISE_GATE_CTL,
				CS42L52_NOISE_GATE_CTL_ALL_SHIFT,
				cs42l52_ng_type_text);

static const char * const left_swap_text[] = {
	"Left", "LR 2", "Right"};

static const char * const right_swap_text[] = {
	"Right", "LR 2", "Left"};

static const unsigned int swap_values[] = { 0, 1, 3 };

static const struct soc_enum adca_swap_enum =
	SOC_VALUE_ENUM_SINGLE(CS42L52_ADC_PCM_MIXER,
				CS42L52_ADC_PCM_MIXER_ADCASWP_SHIFT,
				CS42L52_ADC_PCM_MIXER_ADCASWP_MASK,
				ARRAY_SIZE(left_swap_text),
				left_swap_text,
				swap_values);

static const struct snd_kcontrol_new adca_swap_mux =
	SOC_DAPM_ENUM("Route", adca_swap_enum);

static const struct soc_enum pcma_swap_enum =
	SOC_VALUE_ENUM_SINGLE(CS42L52_ADC_PCM_MIXER,
				CS42L52_ADC_PCM_MIXER_PCMASWP_SHIFT,
				CS42L52_ADC_PCM_MIXER_PCMASWP_MASK,
				ARRAY_SIZE(left_swap_text),
				left_swap_text,
				swap_values);

static const struct snd_kcontrol_new pcma_swap_mux =
	SOC_DAPM_ENUM("Route", pcma_swap_enum);

static const struct soc_enum adcb_swap_enum =
	SOC_VALUE_ENUM_SINGLE(CS42L52_ADC_PCM_MIXER,
				CS42L52_ADC_PCM_MIXER_ADCBSWP_SHIFT,
				CS42L52_ADC_PCM_MIXER_ADCBSWP_MASK,
				ARRAY_SIZE(right_swap_text),
				right_swap_text,
				swap_values);

static const struct snd_kcontrol_new adcb_swap_mux =
	SOC_DAPM_ENUM("Route", adcb_swap_enum);

static const struct soc_enum pcmb_swap_enum =
	SOC_VALUE_ENUM_SINGLE(CS42L52_ADC_PCM_MIXER,
				CS42L52_ADC_PCM_MIXER_PCMBSWP_SHIFT,
				CS42L52_ADC_PCM_MIXER_PCMBSWP_MASK,
				ARRAY_SIZE(right_swap_text),
				right_swap_text,
				swap_values);

static const struct snd_kcontrol_new pcmb_swap_mux =
	SOC_DAPM_ENUM("Route", pcmb_swap_enum);


static const struct snd_kcontrol_new passthrul_ctl =
	SOC_DAPM_SINGLE("Switch",
					CS42L52_MISC_CTL,
					CS42L52_MISC_CTL_PASSTHRUA_SHIFT,
					CS42L52_MISC_CTL_PASSTHRUX_MAX,
					CS42L52_MISC_CTL_PASSTHRUX_INVERT);

static const struct snd_kcontrol_new passthrur_ctl =
	SOC_DAPM_SINGLE("Switch",
					CS42L52_MISC_CTL,
					CS42L52_MISC_CTL_PASSTHRUB_SHIFT,
					CS42L52_MISC_CTL_PASSTHRUX_MAX,
					CS42L52_MISC_CTL_PASSTHRUX_INVERT);


static const struct snd_kcontrol_new spkl_ctl =
	SOC_DAPM_SINGLE("Switch",
					CS42L52_PWRCTL3,
					CS42L52_PWRCTL3_PDN_SPKA_SHIFT,
					CS42L52_PWRCTL3_PDN_SPKX_MAX,
					CS42L52_PWRCTL3_PDN_SPKX_INVERT);

static const struct snd_kcontrol_new spkr_ctl =
	SOC_DAPM_SINGLE("Switch",
					CS42L52_PWRCTL3,
					CS42L52_PWRCTL3_PDN_SPKB_SHIFT,
					CS42L52_PWRCTL3_PDN_SPKX_MAX,
					CS42L52_PWRCTL3_PDN_SPKX_INVERT);

static const struct snd_kcontrol_new hpl_ctl =
	SOC_DAPM_SINGLE("Switch",
					CS42L52_PWRCTL3,
					CS42L52_PWRCTL3_PDN_HPA_SHIFT,
					CS42L52_PWRCTL3_PDN_HPX_MAX,
					CS42L52_PWRCTL3_PDN_HPX_INVERT);

static const struct snd_kcontrol_new hpr_ctl =
	SOC_DAPM_SINGLE("Switch",
					CS42L52_PWRCTL3,
					CS42L52_PWRCTL3_PDN_HPB_SHIFT,
					CS42L52_PWRCTL3_PDN_HPX_MAX,
					CS42L52_PWRCTL3_PDN_HPX_INVERT);


static const struct snd_kcontrol_new cs42l52_snd_controls[] = {

	SOC_DOUBLE_R_EXT_TLV("Master Volume",
				CS42L52_MSTA_VOL,
				CS42L52_MSTB_VOL,
				CS42L52_MSTX_VOL_SHIFT,
				CS42L52_MSTX_VOL_MAX,
				CS42L52_MSTX_VOL_INVERT,
				cs42l52_get_master_volume, cs42l52_set_master_volume,
				master_tlv),

	SOC_DOUBLE_R_EXT_TLV("Headphone Volume",
				CS42L52_HPA_VOL,
				CS42L52_HPB_VOL,
				CS42L52_HPX_VOL_SHIFT,
				CS42L52_HPX_VOL_MAX,
				CS42L52_HPX_VOL_INVERT,
				cs42l52_get_hp_volume, cs42l52_set_hp_volume,
				hp_tlv),

	SOC_ENUM("Headphone Analog Gain",
				hp_gain_enum),

	SOC_DOUBLE_R_EXT_TLV("Speaker Volume",
				CS42L52_SPKA_VOL,
				CS42L52_SPKB_VOL,
				CS42L52_SPKX_VOL_SHIFT,
				CS42L52_SPKX_VOL_MAX,
				CS42L52_SPKX_VOL_INVERT,
				cs42l52_get_spk_volume, cs42l52_set_spk_volume,
				spk_tlv),

	SOC_DOUBLE_R_EXT_TLV("Bypass Volume",
				CS42L52_PASSTHRUA_VOL,
				CS42L52_PASSTHRUB_VOL,
				CS42L52_PASSTHRUX_VOL_SHIFT,
				CS42L52_PASSTHRUX_VOL_MAX,
				CS42L52_PASSTHRUX_VOL_INVERT,
				cs42l52_get_passthru_volume, cs42l52_set_passthru_volume,
				passthru_tlv),

	SOC_DOUBLE("Bypass Mute",
				CS42L52_MISC_CTL,
				CS42L52_MISC_CTL_PASSMUTEA_SHIFT,
				CS42L52_MISC_CTL_PASSMUTEB_SHIFT,
				CS42L52_MISC_CTL_PASSMUTEX_MAX,
				CS42L52_MISC_CTL_PASSMUTEX_INVERT),

	SOC_DOUBLE_R_TLV("MIC Gain Volume",
				CS42L52_MICA_CTL,
				CS42L52_MICB_CTL,
				CS42L52_MICX_CTL_VOL_SHIFT,
				CS42L52_MICX_CTL_VOL_MAX,
				CS42L52_MICX_CTL_VOL_INVERT,
				mic_gain_tlv),

	SOC_ENUM("MIC Bias Level",
				mic_bias_level_enum),

	SOC_DOUBLE_R_EXT_TLV("ADC Volume",
				CS42L52_ADCA_VOL,
				CS42L52_ADCB_VOL,
				CS42L52_ADCX_VOL_SHIFT,
				CS42L52_ADCX_VOL_MAX,
				CS42L52_ADCX_VOL_INVERT,
				cs42l52_get_adc_volume, cs42l52_set_adc_volume,
				adc_tlv),

	SOC_DOUBLE_R_EXT_TLV("ADC Mixer Volume",
				CS42L52_ADCA_MIXER_VOL,
				CS42L52_ADCB_MIXER_VOL,
				CS42L52_ADCX_MIXER_VOL_SHIFT,
				CS42L52_ADCX_MIXER_VOL_MAX,
				CS42L52_ADCX_MIXER_VOL_INVERT,
				cs42l52_get_adc_mixer_volume, cs42l52_set_adc_mixer_volume,
				mixer_tlv),

	SOC_DOUBLE("ADC Switch",
				CS42L52_ADC_MISC_CTL,
				CS42L52_ADC_MISC_CTL_ADCA_MUTE_SHIFT,
				CS42L52_ADC_MISC_CTL_ADCB_MUTE_SHIFT,
				CS42L52_ADC_MISC_CTL_ADCX_MUTE_MAX,
				CS42L52_ADC_MISC_CTL_ADCX_MUTE_INVERT),

	SOC_DOUBLE_R("ADC Mixer Switch",
				CS42L52_ADCA_MIXER_VOL,
				CS42L52_ADCB_MIXER_VOL,
				CS42L52_ADCX_MIXER_MUTE_SHIFT,
				CS42L52_ADCX_MIXER_MUTE_MAX,
				CS42L52_ADCX_MIXER_MUTE_INVERT),

	SOC_DOUBLE_R_EXT_TLV("PGA Volume",
				CS42L52_PGAA_CTL,
				CS42L52_PGAB_CTL,
				CS42L52_PGAX_CTL_VOL_SHIFT,
				CS42L52_PGAX_CTL_VOL_MAX,
				CS42L52_PGAX_CTL_VOL_INVERT,
				cs42l52_get_pga_volume, cs42l52_set_pga_volume,
				pga_tlv),

	SOC_DOUBLE_R_EXT_TLV("PCM Mixer Volume",
				CS42L52_PCMA_MIXER_VOL,
				CS42L52_PCMB_MIXER_VOL,
				CS42L52_PCMX_MIXER_VOL_SHIFT,
				CS42L52_PCMX_MIXER_VOL_MAX,
				CS42L52_PCMX_MIXER_VOL_INVERT,
				cs42l52_get_pcm_mixer_volume, cs42l52_set_pcm_mixer_volume,
				mixer_tlv),

	SOC_DOUBLE_R("PCM Mixer Switch",
				CS42L52_PCMA_MIXER_VOL,
				CS42L52_PCMB_MIXER_VOL,
				CS42L52_PCMX_MIXER_MUTE_SHIFT,
				CS42L52_PCMX_MIXER_MUTE_MAX,
				CS42L52_PCMX_MIXER_MUTE_INVERT),

	/* Beep */
	SOC_ENUM("Beep Config",
				beep_config_enum),

	SOC_ENUM("Beep Pitch",
				beep_pitch_enum),

	SOC_ENUM("Beep on Time",
				beep_ontime_enum),

	SOC_ENUM("Beep off Time",
				beep_offtime_enum),

	SOC_SINGLE_EXT_TLV("Beep Volume",
				CS42L52_BEEP_VOL_OFFTIME,
				CS42L52_BEEP_VOL_OFFTIME_VOL_SHIFT,
				CS42L52_BEEP_VOL_OFFTIME_VOL_MAX,
				CS42L52_BEEP_VOL_OFFTIME_VOL_INVERT,
				cs42l52_get_beep_volume, cs42l52_set_beep_volume,
				beep_tlv),

	SOC_SINGLE("Beep Mixer Switch",
				CS42L52_BEEP_TONE_CTL,
				CS42L52_BEEP_TONE_CTL_BEEP_MIX_SHIFT,
				CS42L52_BEEP_TONE_CTL_BEEP_MIX_MAX,
				CS42L52_BEEP_TONE_CTL_BEEP_MIX_INVERT),

	SOC_ENUM("Beep Treble Corner Freq",
				beep_treble_enum),

	SOC_ENUM("Beep Bass Corner Freq",
				beep_bass_enum),

	/* Tone */
	SOC_SINGLE("Tone Control Switch",
				CS42L52_BEEP_TONE_CTL,
				CS42L52_BEEP_TONE_CTL_TONE_CTL_SHIFT,
				CS42L52_BEEP_TONE_CTL_TONE_CTL_MAX,
				CS42L52_BEEP_TONE_CTL_TONE_CTL_INVERT),

	SOC_SINGLE_TLV("Treble Gain Volume",
				CS42L52_TONE_CTL,
				CS42L52_TONE_CTL_TREBLE_GAIN_SHIFT,
				CS42L52_TONE_CTL_TREBLE_GAIN_MAX,
				CS42L52_TONE_CTL_TREBLE_GAIN_INVERT,
				tone_gain_tlv),

	SOC_SINGLE_TLV("Bass Gain Volume",
				CS42L52_TONE_CTL,
				CS42L52_TONE_CTL_BASS_GAIN_SHIFT,
				CS42L52_TONE_CTL_BASS_GAIN_MAX,
				CS42L52_TONE_CTL_BASS_GAIN_INVERT,
				tone_gain_tlv),

	/* Limiter */
	SOC_SINGLE_TLV("Limiter Max Threshold Volume",
				CS42L52_LIMITER_CTL1,
				CS42L52_LIMITER_CTL1_MAXIMUM_THRESHOLD_SHIFT,
				CS42L52_LIMITER_CTL1_MAXIMUM_THRESHOLD_MAX,
				CS42L52_LIMITER_CTL1_MAXIMUM_THRESHOLD_INVERT,
				limiter_tlv),

	SOC_SINGLE_TLV("Limiter Cushion Threshold Volume",
				CS42L52_LIMITER_CTL1,
				CS42L52_LIMITER_CTL1_CUSHION_THRESHOLD_SHIFT,
				CS42L52_LIMITER_CTL1_CUSHION_THRESHOLD_MAX,
				CS42L52_LIMITER_CTL1_CUSHION_THRESHOLD_INVERT,
				limiter_tlv),

	SOC_SINGLE_TLV("Limiter Release Rate Volume",
				CS42L52_LIMITER_CTL2,
				CS42L52_LIMITER_CTL2_RELEASE_RATE_SHIFT,
				CS42L52_LIMITER_CTL2_RELEASE_RATE_MAX,
				CS42L52_LIMITER_CTL2_RELEASE_RATE_INVERT,
				limiter_tlv),

	SOC_SINGLE_TLV("Limiter Attack Rate Volume",
				CS42L52_LIMITER_AT_RATE,
				CS42L52_LIMITER_AT_RATE_SHIFT,
				CS42L52_LIMITER_AT_RATE_MAX,
				CS42L52_LIMITER_AT_RATE_INVERT,
				limiter_tlv),

	SOC_SINGLE("Limiter Soft Ramp Switch",
				CS42L52_LIMITER_CTL1,
				CS42L52_LIMITER_CTL1_SOFT_RAMP_SHIFT,
				CS42L52_LIMITER_CTL1_SOFT_RAMP_MAX,
				CS42L52_LIMITER_CTL1_SOFT_RAMP_INVERT),

	SOC_SINGLE("Limiter Zero Cross Switch",
				CS42L52_LIMITER_CTL1,
				CS42L52_LIMITER_CTL1_ZERO_CROSS_SHIFT,
				CS42L52_LIMITER_CTL1_ZERO_CROSS_MAX,
				CS42L52_LIMITER_CTL1_ZERO_CROSS_INVERT),

	SOC_SINGLE("Limiter Switch",
				CS42L52_LIMITER_CTL2,
				CS42L52_LIMITER_CTL2_LIMITER_ENABLE_SHIFT,
				CS42L52_LIMITER_CTL2_LIMITER_ENABLE_MAX,
				CS42L52_LIMITER_CTL2_LIMITER_ENABLE_INVERT),

	/* ALC */
	SOC_SINGLE_TLV("ALC Attack Rate Volume",
				CS42L52_ALC_CTL,
				CS42L52_ALC_CTL_AT_RATE_SHIFT,
				CS42L52_ALC_CTL_AT_RATE_MAX,
				CS42L52_ALC_CTL_AT_RATE_INVERT,
				limiter_tlv),

	SOC_SINGLE_TLV("ALC Release Rate Volume",
				CS42L52_ALC_RATE,
				CS42L52_ALC_RATE_SHIFT,
				CS42L52_ALC_RATE_MAX,
				CS42L52_ALC_RATE_INVERT,
				limiter_tlv),

	SOC_SINGLE_TLV("ALC Max Threshold Volume",
				CS42L52_ALC_THRESHOLD,
				CS42L52_ALC_MAX_RATE_SHIFT,
				CS42L52_ALC_MAX_RATE_MAX,
				CS42L52_ALC_MAX_RATE_INVERT,
				limiter_tlv),

	SOC_SINGLE_TLV("ALC Min Threshold Volume",
				CS42L52_ALC_THRESHOLD,
				CS42L52_ALC_MIN_RATE_SHIFT,
				CS42L52_ALC_MIN_RATE_MAX,
				CS42L52_ALC_MIN_RATE_INVERT,
				limiter_tlv),

	SOC_DOUBLE_R("ALC Soft Ramp Capture Switch", 
				CS42L52_PGAA_CTL,
				CS42L52_PGAB_CTL,
				CS42L52_PGAX_CTL_ACL_SOFT_RAMP_SHIFT,
				CS42L52_PGAX_CTL_ACL_SOFT_RAMP_MAX,
				CS42L52_PGAX_CTL_ACL_SOFT_RAMP_INVERT),

	SOC_DOUBLE_R("ALC Zero Cross Capture Switch", 
				CS42L52_PGAA_CTL,
				CS42L52_PGAB_CTL,
				CS42L52_PGAX_CTL_ACL_ZERO_CROSS_SHIFT,
				CS42L52_PGAX_CTL_ACL_ZERO_CROSS_MAX,
				CS42L52_PGAX_CTL_ACL_ZERO_CROSS_INVERT),

	SOC_DOUBLE("ALC Capture Switch",
				CS42L52_ALC_CTL,
				CS42L52_ALC_CTL_ALCA_ENABLE_SHIFT,
				CS42L52_ALC_CTL_ALCB_ENABLE_SHIFT,
				CS42L52_ALC_CTL_ALCX_ENABLE_MAX,
				CS42L52_ALC_CTL_ALCX_ENABLE_INVERT),

	/* Noise gate */
	SOC_ENUM("NG Type Switch",
				ng_type_enum),

	SOC_SINGLE("NG Enable Switch",
				CS42L52_NOISE_GATE_CTL,
				CS42L52_NOISE_GATE_CTL_ENABLE_SHIFT,
				CS42L52_NOISE_GATE_CTL_ENABLE_MAX,
				CS42L52_NOISE_GATE_CTL_ENABLE_INVERT),

	SOC_SINGLE("NG Boost Switch",
				CS42L52_NOISE_GATE_CTL,
				CS42L52_NOISE_GATE_CTL_BOOST_SHIFT,
				CS42L52_NOISE_GATE_CTL_BOOST_MAX,
				CS42L52_NOISE_GATE_CTL_BOOST_INVERT),

	SOC_ENUM("NG Threshold",
				ng_threshold_enum),

	SOC_ENUM("NG Delay",
				ng_delay_enum),

	SOC_DOUBLE("Analog HPF Switch",
				CS42L52_ANALOG_HPF_CTL,
				CS42L52_ANALOG_HPF_CTL_HPFA_SHIFT,
				CS42L52_ANALOG_HPF_CTL_HPFB_SHIFT,
				CS42L52_ANALOG_HPF_CTL_HPFX_MAX,
				CS42L52_ANALOG_HPF_CTL_HPFX_INVERT),

	SOC_DOUBLE("Analog Soft Ramp Switch",
				CS42L52_ANALOG_HPF_CTL,
				CS42L52_ANALOG_HPF_CTL_ANLGSFTA_SHIFT,
				CS42L52_ANALOG_HPF_CTL_ANLGSFTB_SHIFT,
				CS42L52_ANALOG_HPF_CTL_ANLGSFTX_MAX,
				CS42L52_ANALOG_HPF_CTL_ANLGSFTX_INVERT),

	SOC_DOUBLE("Analog Zero Cross Switch",
				CS42L52_ANALOG_HPF_CTL,
				CS42L52_ANALOG_HPF_CTL_ANLGZCA_SHIFT,
				CS42L52_ANALOG_HPF_CTL_ANLGZCB_SHIFT,
				CS42L52_ANALOG_HPF_CTL_ANLGZCX_MAX,
				CS42L52_ANALOG_HPF_CTL_ANLGZCX_INVERT),

	SOC_DOUBLE("Analog HPF Freeze Switch",
				CS42L52_ANALOG_HPF_CTL,
				CS42L52_ANALOG_HPF_CTL_HPFRZA_SHIFT,
				CS42L52_ANALOG_HPF_CTL_HPFRZB_SHIFT,
				CS42L52_ANALOG_HPF_CTL_HPFRZX_MAX,
				CS42L52_ANALOG_HPF_CTL_HPFRZX_INVERT),

	SOC_SINGLE("Digital Soft Ramp Switch",
				CS42L52_MISC_CTL,
				CS42L52_MISC_CTL_DIGSFT_SHIFT,
				CS42L52_MISC_CTL_DIGSFT_MAX,
				CS42L52_MISC_CTL_DIGSFT_INVERT),

	SOC_SINGLE("Digital Zero Cross Switch",
				CS42L52_MISC_CTL,
				CS42L52_MISC_CTL_DIGZC_SHIFT,
				CS42L52_MISC_CTL_DIGZC_MAX,
				CS42L52_MISC_CTL_DIGZC_INVERT),

	SOC_SINGLE("Deemphasis Switch",
				CS42L52_MISC_CTL,
				CS42L52_MISC_CTL_DEEMPH_SHIFT,
				CS42L52_MISC_CTL_DEEMPH_MAX,
				CS42L52_MISC_CTL_DEEMPH_INVERT),

	SOC_SINGLE("Batt Compensation Switch",
				CS42L52_BATT_COMPEN,
				CS42L52_BATT_COMPEN_ENABLE_SWITCH,
				CS42L52_BATT_COMPEN_ENABLE_MAX,
				CS42L52_BATT_COMPEN_ENABLE_INVERT),

	SOC_SINGLE("Batt VP Monitor Switch",
				CS42L52_BATT_COMPEN,
				CS42L52_BATT_COMPEN_VPMONITOR_SWITCH,
				CS42L52_BATT_COMPEN_VPMONITOR_MAX,
				CS42L52_BATT_COMPEN_VPMONITOR_INVERT),

	SOC_SINGLE("Batt VP ref",
				CS42L52_BATT_COMPEN,
				CS42L52_BATT_COMPEN_VPREF_SWITCH,
				CS42L52_BATT_COMPEN_VPREF_MAX,
				CS42L52_BATT_COMPEN_VPREF_INVERT),

	SOC_SINGLE("PGA AIN1L Switch",
				CS42L52_ADC_PGA_A,
				CS42L52_ADC_PGA_X_PGA_SEL_AIN1_SHIFT,
				CS42L52_ADC_PGA_X_PGA_SEL_MAX,
				CS42L52_ADC_PGA_X_PGA_SEL_INVERT),

	SOC_SINGLE("PGA AIN1R Switch",
				CS42L52_ADC_PGA_B,
				CS42L52_ADC_PGA_X_PGA_SEL_AIN1_SHIFT,
				CS42L52_ADC_PGA_X_PGA_SEL_MAX,
				CS42L52_ADC_PGA_X_PGA_SEL_INVERT),

	SOC_SINGLE("PGA AIN2L Switch",
				CS42L52_ADC_PGA_A,
				CS42L52_ADC_PGA_X_PGA_SEL_AIN2_SHIFT,
				CS42L52_ADC_PGA_X_PGA_SEL_MAX,
				CS42L52_ADC_PGA_X_PGA_SEL_INVERT),

	SOC_SINGLE("PGA AIN2R Switch",
				CS42L52_ADC_PGA_B,
				CS42L52_ADC_PGA_X_PGA_SEL_AIN2_SHIFT,
				CS42L52_ADC_PGA_X_PGA_SEL_MAX,
				CS42L52_ADC_PGA_X_PGA_SEL_INVERT),

	SOC_SINGLE("PGA AIN3L Switch",
				CS42L52_ADC_PGA_A,
				CS42L52_ADC_PGA_X_PGA_SEL_AIN3_SHIFT,
				CS42L52_ADC_PGA_X_PGA_SEL_MAX,
				CS42L52_ADC_PGA_X_PGA_SEL_INVERT),

	SOC_SINGLE("PGA AIN3R Switch",
				CS42L52_ADC_PGA_B,
				CS42L52_ADC_PGA_X_PGA_SEL_AIN3_SHIFT,
				CS42L52_ADC_PGA_X_PGA_SEL_MAX,
				CS42L52_ADC_PGA_X_PGA_SEL_INVERT),

	SOC_SINGLE("PGA AIN4L Switch",
				CS42L52_ADC_PGA_A,
				CS42L52_ADC_PGA_X_PGA_SEL_AIN4_SHIFT,
				CS42L52_ADC_PGA_X_PGA_SEL_MAX,
				CS42L52_ADC_PGA_X_PGA_SEL_INVERT),

	SOC_SINGLE("PGA AIN4R Switch",
				CS42L52_ADC_PGA_B,
				CS42L52_ADC_PGA_X_PGA_SEL_AIN4_SHIFT,
				CS42L52_ADC_PGA_X_PGA_SEL_MAX,
				CS42L52_ADC_PGA_X_PGA_SEL_INVERT),

	SOC_SINGLE("PGA MICA Switch",
				CS42L52_ADC_PGA_A,
				CS42L52_ADC_PGA_X_PGA_SEL_MIC_SHIFT,
				CS42L52_ADC_PGA_X_PGA_SEL_MAX,
				CS42L52_ADC_PGA_X_PGA_SEL_INVERT),

	SOC_SINGLE("PGA MICB Switch",
				CS42L52_ADC_PGA_B,
				CS42L52_ADC_PGA_X_PGA_SEL_MIC_SHIFT,
				CS42L52_ADC_PGA_X_PGA_SEL_MAX,
				CS42L52_ADC_PGA_X_PGA_SEL_INVERT),

	SOC_SINGLE("PGA Left Switch",
				CS42L52_PWRCTL1,
				CS42L52_PWRCTL1_PDN_PGAA_SHIFT,
				CS42L52_PWRCTL1_PDN_PGAX_MAX,
				CS42L52_PWRCTL1_PDN_PGAX_INVERT),

	SOC_SINGLE("PGA Right Switch",
				CS42L52_PWRCTL1,
				CS42L52_PWRCTL1_PDN_PGAB_SHIFT,
				CS42L52_PWRCTL1_PDN_PGAX_MAX,
				CS42L52_PWRCTL1_PDN_PGAX_INVERT),

	SOC_SINGLE("ADC Left Switch",
				CS42L52_PWRCTL1,
				CS42L52_PWRCTL1_PDN_ADCA_SHIFT,
				CS42L52_PWRCTL1_PDN_ADCX_MAX,
				CS42L52_PWRCTL1_PDN_ADCX_INVERT),

	SOC_SINGLE("ADC Right Switch",
				CS42L52_PWRCTL1,
				CS42L52_PWRCTL1_PDN_ADCB_SHIFT,
				CS42L52_PWRCTL1_PDN_ADCX_MAX,
				CS42L52_PWRCTL1_PDN_ADCX_INVERT),

	SOC_DOUBLE("ADC Override Switch",
				CS42L52_PWRCTL2,
				CS42L52_PWRCTL2_PDN_OVRDA_SHIFT,
				CS42L52_PWRCTL2_PDN_OVRDB_SHIFT,
				CS42L52_PWRCTL2_PDN_OVRDX_MAX,
				CS42L52_PWRCTL2_PDN_OVRDX_INVERT),

	SOC_DOUBLE("HP Mute",
				CS42L52_PB_CTL2,
				CS42L52_PB_CTL2_HPA_MUTE_SHIFT,
				CS42L52_PB_CTL2_HPB_MUTE_SHIFT,
				CS42L52_PB_CTL2_HPX_MUTE_MAX,
				CS42L52_PB_CTL2_HPX_MUTE_INVERT),

	SOC_DOUBLE("SPK Mute",
				CS42L52_PB_CTL2,
				CS42L52_PB_CTL2_SPKA_MUTE_SHIFT,
				CS42L52_PB_CTL2_SPKB_MUTE_SHIFT,
				CS42L52_PB_CTL2_SPKX_MUTE_MAX,
				CS42L52_PB_CTL2_SPKX_MUTE_INVERT),

	SOC_SINGLE("SPK Swap",
				CS42L52_PB_CTL2,
				CS42L52_PB_CTL2_SPK_SWAP_SHIFT,
				CS42L52_PB_CTL2_SPK_SWAP_MAX,
				CS42L52_PB_CTL2_SPK_SWAP_INVERT),

	SOC_SINGLE("SPK Mono",
				CS42L52_PB_CTL2,
				CS42L52_PB_CTL2_SPK_MONO_SHIFT,
				CS42L52_PB_CTL2_SPK_MONO_MAX,
				CS42L52_PB_CTL2_SPK_MONO_INVERT),

	SOC_SINGLE("SPK Mute 50",
				CS42L52_PB_CTL2,
				CS42L52_PB_CTL2_SPK_MUTE50_SHIFT,
				CS42L52_PB_CTL2_SPK_MUTE50_MAX,
				CS42L52_PB_CTL2_SPK_MUTE50_INVERT),

	SOC_SINGLE("SPK Volume Settings B=A",
				CS42L52_PB_CTL2,
				CS42L52_PB_CTL2_SPK_VOL_B_EQ_A_SHIFT,
				CS42L52_PB_CTL2_SPK_VOL_B_EQ_A_MAX,
				CS42L52_PB_CTL2_SPK_VOL_B_EQ_A_INVERT),

};

static const struct snd_kcontrol_new cs42l52_mica_controls[] = {
	SOC_ENUM("MICA Select", mica_enum),
};

static const struct snd_kcontrol_new cs42l52_micb_controls[] = {
	SOC_ENUM("MICB Select", micb_enum),
};

static int cs42l52_add_mic_controls(struct snd_soc_codec *codec)
{
	struct cs42l52_private *cs42l52 = snd_soc_codec_get_drvdata(codec);
	struct cs42l52_platform_data *pdata = &cs42l52->pdata;

	if (!pdata->mica_diff_cfg)
		snd_soc_add_codec_controls(codec, cs42l52_mica_controls,
					ARRAY_SIZE(cs42l52_mica_controls));

	if (!pdata->micb_diff_cfg)
		snd_soc_add_codec_controls(codec, cs42l52_micb_controls,
					ARRAY_SIZE(cs42l52_micb_controls));

	return 0;
}

static const struct snd_soc_dapm_widget cs42l52_dapm_widgets[] = {

	SND_SOC_DAPM_SIGGEN("Beep"),

	SND_SOC_DAPM_INPUT("AIN1L"),
	SND_SOC_DAPM_INPUT("AIN1R"),
	SND_SOC_DAPM_INPUT("AIN2L"),
	SND_SOC_DAPM_INPUT("AIN2R"),
	SND_SOC_DAPM_INPUT("AIN3L"),
	SND_SOC_DAPM_INPUT("AIN3R"),
	SND_SOC_DAPM_INPUT("AIN4L"),
	SND_SOC_DAPM_INPUT("AIN4R"),
	SND_SOC_DAPM_INPUT("MICA"),
	SND_SOC_DAPM_INPUT("MICB"),

	SND_SOC_DAPM_AIF_OUT("AIFOUTL", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIFOUTR", NULL, 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("ADC Left", NULL,
				CS42L52_PWRCTL1,
				CS42L52_PWRCTL1_PDN_ADCA_SHIFT,
				CS42L52_PWRCTL1_PDN_ADCX_INVERT),

	SND_SOC_DAPM_ADC("ADC Right", NULL,
				CS42L52_PWRCTL1,
				CS42L52_PWRCTL1_PDN_ADCB_SHIFT,
				CS42L52_PWRCTL1_PDN_ADCX_INVERT),

	SND_SOC_DAPM_PGA("PGA Left",
				CS42L52_PWRCTL1,
				CS42L52_PWRCTL1_PDN_PGAA_SHIFT,
				CS42L52_PWRCTL1_PDN_PGAX_INVERT,
				NULL, 0),

	SND_SOC_DAPM_PGA("PGA Right",
				CS42L52_PWRCTL1,
				CS42L52_PWRCTL1_PDN_PGAB_SHIFT,
				CS42L52_PWRCTL1_PDN_PGAX_INVERT,
				NULL, 0),

	SND_SOC_DAPM_MUX("ADC Left Mux", SND_SOC_NOPM, 0, 0, &adca_mux),
	SND_SOC_DAPM_MUX("ADC Right Mux", SND_SOC_NOPM, 0, 0, &adcb_mux),

	SND_SOC_DAPM_MUX("ADC Left Swap Mux", SND_SOC_NOPM, 0, 0, &adca_swap_mux),
	SND_SOC_DAPM_MUX("ADC Right Swap Mux", SND_SOC_NOPM, 0, 0, &adcb_swap_mux),

	SND_SOC_DAPM_MUX("Digital Output Mux", SND_SOC_NOPM,
				0, 0, &digital_output_mux),

	SND_SOC_DAPM_PGA("PGA MICA",
				CS42L52_PWRCTL2,
				CS42L52_PWRCTL2_PDN_MICA_SHIFT,
				CS42L52_PWRCTL2_PDN_MICX_INVERT,
				NULL, 0),

	SND_SOC_DAPM_PGA("PGA MICB",
				CS42L52_PWRCTL2,
				CS42L52_PWRCTL2_PDN_MICB_SHIFT,
				CS42L52_PWRCTL2_PDN_MICX_INVERT,
				NULL, 0),

	SND_SOC_DAPM_MICBIAS("Mic Bias",
				CS42L52_PWRCTL2,
				CS42L52_PWRCTL2_PDN_BIAS_SHIFT,
				CS42L52_PWRCTL2_PDN_BIAS_INVERT),

	SND_SOC_DAPM_SUPPLY("Charge Pump",
				CS42L52_PWRCTL1,
				CS42L52_PWRCTL1_PDN_CHARGE_PUMP_SHIFT,
				CS42L52_PWRCTL1_PDN_CHARGE_PUMP_INVERT,
				NULL, 0),

	SND_SOC_DAPM_AIF_IN("AIFINL", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIFINR", NULL, 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_DAC("DAC Left", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC Right", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_SWITCH("Bypass Left",
				CS42L52_MISC_CTL,
				CS42L52_MISC_CTL_PASSTHRUA_SHIFT,
				CS42L52_MISC_CTL_PASSTHRUX_INVERT,
				&passthrul_ctl),

	SND_SOC_DAPM_SWITCH("Bypass Right",
				CS42L52_MISC_CTL,
				CS42L52_MISC_CTL_PASSTHRUB_SHIFT,
				CS42L52_MISC_CTL_PASSTHRUX_INVERT,
				&passthrur_ctl),

	SND_SOC_DAPM_MUX("PCM Left Swap Mux", SND_SOC_NOPM, 0, 0, &pcma_swap_mux),
	SND_SOC_DAPM_MUX("PCM Right Swap Mux", SND_SOC_NOPM, 0, 0, &pcmb_swap_mux),

	SND_SOC_DAPM_SWITCH("HP Left Amp",
					CS42L52_PWRCTL3,
					CS42L52_PWRCTL3_PDN_HPA_SHIFT,
					CS42L52_PWRCTL3_PDN_HPX_INVERT,
					&hpl_ctl),

	SND_SOC_DAPM_SWITCH("HP Right Amp",
					CS42L52_PWRCTL3,
					CS42L52_PWRCTL3_PDN_HPB_SHIFT,
					CS42L52_PWRCTL3_PDN_HPX_INVERT,
					&hpr_ctl),

	SND_SOC_DAPM_SWITCH("SPK Left Amp",
					CS42L52_PWRCTL3,
					CS42L52_PWRCTL3_PDN_SPKA_SHIFT,
					CS42L52_PWRCTL3_PDN_SPKX_INVERT,
					&spkl_ctl),

	SND_SOC_DAPM_SWITCH("SPK Right Amp",
					CS42L52_PWRCTL3,
					CS42L52_PWRCTL3_PDN_SPKB_SHIFT,
					CS42L52_PWRCTL3_PDN_SPKX_INVERT,
					&spkr_ctl),

	SND_SOC_DAPM_OUTPUT("HPOUTA"),
	SND_SOC_DAPM_OUTPUT("HPOUTB"),
	SND_SOC_DAPM_OUTPUT("SPKOUTA"),
	SND_SOC_DAPM_OUTPUT("SPKOUTB"),

};

static const struct snd_soc_dapm_route cs42l52_audio_map[] = {

	{"Capture", "DSP", "Digital Output Mux"},
	{"Capture", "ADC", "Digital Output Mux"},

	{"Digital Output Mux", NULL, "ADC Left"},
	{"Digital Output Mux", NULL, "ADC Right"},

	{"ADC Left", NULL, "ADC Left Swap Mux"},
	{"ADC Right", NULL, "ADC Right Swap Mux"},

	{"ADC Left Swap Mux", NULL, "ADC Left"},
	{"ADC Right Swap Mux", NULL, "ADC Right"},

	{"DAC Left", "Left", "ADC Left Swap Mux"},
	{"DAC Left", "LR 2", "ADC Left Swap Mux"},
	{"DAC Left", "Right", "ADC Left Swap Mux"},

	{"DAC Right", "Left", "ADC Right Swap Mux"},
	{"DAC Right", "LR 2", "ADC Right Swap Mux"},
	{"DAC Right", "Right", "ADC Right Swap Mux"},

	{"ADC Left", NULL, "Charge Pump"},
	{"ADC Right", NULL, "Charge Pump"},

	{"Charge Pump", NULL, "ADC Left Mux"},
	{"Charge Pump", NULL, "ADC Right Mux"},

	{"ADC Left Mux", NULL, "Mic Bias"},
	{"ADC Right Mux", NULL, "Mic Bias"},

	{"ADC Left Mux", "Input1A", "AIN1L"},
	{"ADC Left Mux", "Input2A", "AIN2L"},
	{"ADC Left Mux", "Input3A", "AIN3L"},
	{"ADC Left Mux", "Input4A", "AIN4L"},
	{"ADC Left Mux", "PGA Input Left", "PGA Left"},
	{"ADC Right Mux", "Input1B", "AIN1R"},
	{"ADC Right Mux", "Input2B", "AIN2R"},
	{"ADC Right Mux", "Input3B", "AIN3R"},
	{"ADC Right Mux", "Input4B", "AIN4R"},
	{"ADC Right Mux", "PGA Input Right", "PGA Right"},

	{"PGA Left", "Switch", "AIN1L"},
	{"PGA Left", "Switch", "AIN2L"},
	{"PGA Left", "Switch", "AIN3L"},
	{"PGA Left", "Switch", "AIN4L"},
	{"PGA Right", "Switch", "AIN1R"},
	{"PGA Right", "Switch", "AIN2R"},
	{"PGA Right", "Switch", "AIN3R"},
	{"PGA Right", "Switch", "AIN4R"},

	{"PGA Left", "Switch", "PGA MICA"},
	{"PGA MICA", NULL, "MICA"},

	{"PGA Right", "Switch", "PGA MICB"},
	{"PGA MICB", NULL, "MICB"},

	{"HPOUTA", NULL, "HP Left Amp"},
	{"HPOUTB", NULL, "HP Right Amp"},
	{"HP Left Amp", NULL, "Bypass Left"},
	{"HP Right Amp", NULL, "Bypass Right"},
	{"Bypass Left", "Switch", "PGA Left"},
	{"Bypass Right", "Switch", "PGA Right"},
	{"HP Left Amp", "Switch", "DAC Left"},
	{"HP Right Amp", "Switch", "DAC Right"},

	{"SPKOUTA", NULL, "SPK Left Amp"},
	{"SPKOUTB", NULL, "SPK Right Amp"},

	{"SPK Left Amp", NULL, "Beep"},
	{"SPK Right Amp", NULL, "Beep"},
	{"SPK Left Amp", "Switch", "Playback"},
	{"SPK Right Amp", "Switch", "Playback"},

	{"DAC Left", NULL, "Beep"},
	{"DAC Right", NULL, "Beep"},
	{"DAC Left", NULL, "Playback"},
	{"DAC Right", NULL, "Playback"},

	{"Digital Output Mux", "DSP", "Playback"},
	{"Digital Output Mux", "DSP", "Playback"},

	{"DAC Left", NULL, "PCM Left Swap Mux"},
	{"DAC Right", NULL, "PCM Right Swap Mux"},

	{"PCM Right Swap Mux", "Left", "Playback"},
	{"PCM Right Swap Mux", "LR 2", "Playback"},
	{"PCM Right Swap Mux", "Right", "Playback"},

	{"PCM Left Swap Mux", "Left", "Playback"},
	{"PCM Left Swap Mux", "LR 2", "Playback"},
	{"PCM Left Swap Mux", "Right", "Playback"},

	{"AIFINL", NULL, "Playback"},
	{"AIFINR", NULL, "Playback"},
};

struct cs42l52_clk_para {
	u32 mclk;
	u32 rate;
	u8 speed;
	u8 group;
	u8 videoclk;
	u8 ratio;
	u8 mclkdiv2;
};

static const struct cs42l52_clk_para clk_map_table[] = {
	/*8k*/
	{12000000, 8000, CLK_QS_MODE, CLK_32K, CLK_NO_27M, CLK_R_125, 0},
	{12288000, 8000, CLK_QS_MODE, CLK_32K, CLK_NO_27M, CLK_R_128, 0},
	{18432000, 8000, CLK_QS_MODE, CLK_32K, CLK_NO_27M, CLK_R_128, 0},
	{24000000, 8000, CLK_QS_MODE, CLK_32K, CLK_NO_27M, CLK_R_125, 1},
	{27000000, 8000, CLK_QS_MODE, CLK_32K, CLK_27M_MCLK, CLK_R_125, 0},

	/*11.025k*/
	{11289600, 11025, CLK_QS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{16934400, 11025, CLK_QS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{27000000, 11025, CLK_QS_MODE, CLK_NO_32K, CLK_27M_MCLK, CLK_R_136, 0},

	/*12k*/
	{27000000, 12000, CLK_QS_MODE, CLK_NO_32K, CLK_27M_MCLK, CLK_R_125, 0},

	/*16k*/
	{12000000, 16000, CLK_HS_MODE, CLK_32K, CLK_NO_27M, CLK_R_125, 0},
	{12288000, 16000, CLK_HS_MODE, CLK_32K, CLK_NO_27M, CLK_R_128, 0},
	{18432000, 16000, CLK_HS_MODE, CLK_32K, CLK_NO_27M, CLK_R_128, 0},
	{24000000, 16000, CLK_HS_MODE, CLK_32K, CLK_NO_27M, CLK_R_125, 1},
	{27000000, 16000, CLK_HS_MODE, CLK_32K, CLK_27M_MCLK, CLK_R_125, 0},

	/*22.05k*/
	{11289600, 22050, CLK_HS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{16934400, 22050, CLK_HS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{27000000, 22050, CLK_HS_MODE, CLK_NO_32K, CLK_27M_MCLK, CLK_R_136, 0},

	/*24k*/
	{27000000, 22050, CLK_HS_MODE, CLK_NO_32K, CLK_27M_MCLK, CLK_R_125, 0},

	/* 32k */
	{12000000, 32000, CLK_SS_MODE, CLK_32K, CLK_NO_27M, CLK_R_125, 0},
	{12288000, 32000, CLK_SS_MODE, CLK_32K, CLK_NO_27M, CLK_R_128, 0},
	{18432000, 32000, CLK_SS_MODE, CLK_32K, CLK_NO_27M, CLK_R_128, 0},
	{24000000, 32000, CLK_SS_MODE, CLK_32K, CLK_NO_27M, CLK_R_125, 1},
	{27000000, 32000, CLK_SS_MODE, CLK_32K, CLK_27M_MCLK, CLK_R_125, 0},

	/* 44.1k */
	{11289600, 44100, CLK_SS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{16934400, 44100, CLK_SS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{27000000, 44100, CLK_SS_MODE, CLK_NO_32K, CLK_27M_MCLK, CLK_R_136, 0},

	/* 48k */
	{12000000, 48000, CLK_SS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_125, 0},
	{12288000, 48000, CLK_SS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{18432000, 48000, CLK_SS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{24000000, 48000, CLK_SS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_125, 1},
	{27000000, 48000, CLK_SS_MODE, CLK_NO_32K, CLK_27M_MCLK, CLK_R_125, 0},

	/* 88.2k */
	{11289600, 88200, CLK_DS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{16934400, 88200, CLK_DS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},

	/* 96k */
	{12288000, 96000, CLK_DS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{18432000, 96000, CLK_DS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_128, 0},
	{12000000, 96000, CLK_DS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_125, 0},
	{24000000, 96000, CLK_DS_MODE, CLK_NO_32K, CLK_NO_27M, CLK_R_125, 1},
};

static int cs42l52_get_clk(int mclk, int rate)
{
	int i, ret = -EINVAL;
	u_int mclk1, mclk2 = 0;

	for (i = 0; i < ARRAY_SIZE(clk_map_table); i++) {
		if (clk_map_table[i].rate == rate) {
			mclk1 = clk_map_table[i].mclk;
			if (abs(mclk - mclk1) < abs(mclk - mclk2)) {
				mclk2 = mclk1;
				ret = i;
			}
		}
	}
	return ret;
}

static int cs42l52_set_sysclk(struct snd_soc_dai *codec_dai,
			int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs42l52_private *cs42l52 = snd_soc_codec_get_drvdata(codec);

	if ((freq >= CS42L52_MIN_CLK) && (freq <= CS42L52_MAX_CLK)) {
		cs42l52->sysclk = freq;
	} else {
		dev_err(codec->dev, "Invalid freq parameter\n");
		return -EINVAL;
	}
	return 0;
}

static int cs42l52_set_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs42l52_private *cs42l52 = snd_soc_codec_get_drvdata(codec);
	u8 iface = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface = CS42L52_IFACE_CTL1_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		iface = CS42L52_IFACE_CTL1_SLAVE;
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= CS42L52_IFACE_CTL1_ADC_FMT_I2S |
				CS42L52_IFACE_CTL1_DAC_FMT_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface |= CS42L52_IFACE_CTL1_DAC_FMT_RIGHT_J;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= CS42L52_IFACE_CTL1_ADC_FMT_LEFT_J |
				CS42L52_IFACE_CTL1_DAC_FMT_LEFT_J;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= CS42L52_IFACE_CTL1_DSP_MODE_EN;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= CS42L52_IFACE_CTL1_INV_SCLK;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= CS42L52_IFACE_CTL1_INV_SCLK;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		return -EINVAL;
	}
	cs42l52->config.format = iface;
	snd_soc_write(codec, CS42L52_IFACE_CTL1, cs42l52->config.format);

	return 0;
}

static int cs42l52_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;

	if (mute)
		snd_soc_update_bits(codec, CS42L52_PB_CTL1,
					CS42L52_PB_CTL1_MUTE_MASK,
					CS42L52_PB_CTL1_MUTE);
	else
		snd_soc_update_bits(codec, CS42L52_PB_CTL1,
					CS42L52_PB_CTL1_MUTE_MASK,
					CS42L52_PB_CTL1_UNMUTE);

	return 0;
}

static int cs42l52_pcm_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params,
					struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cs42l52_private *cs42l52 = snd_soc_codec_get_drvdata(codec);
	u32 clk = 0;
	int index;

	index = cs42l52_get_clk(cs42l52->sysclk, params_rate(params));
	if (index >= 0) {
		cs42l52->sysclk = clk_map_table[index].mclk;

		clk |= (clk_map_table[index].speed << CLK_SPEED_SHIFT) |
		(clk_map_table[index].group << CLK_32K_SR_SHIFT) |
		(clk_map_table[index].videoclk << CLK_27M_MCLK_SHIFT) |
		(clk_map_table[index].ratio << CLK_RATIO_SHIFT) |
		clk_map_table[index].mclkdiv2;

		snd_soc_write(codec, CS42L52_CLK_CTL, clk);
	} else {
		dev_err(codec->dev, "can't get correct mclk\n");
		return -EINVAL;
	}

	return 0;
}

static int cs42l52_set_bias_level(struct snd_soc_codec *codec,
					enum snd_soc_bias_level level)
{
	struct cs42l52_private *cs42l52 = snd_soc_codec_get_drvdata(codec);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		snd_soc_update_bits(codec, CS42L52_PWRCTL1,
					CS42L52_PWRCTL1_PDN_CODEC, 0);
		break;
	case SND_SOC_BIAS_STANDBY:
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {
			regcache_cache_only(cs42l52->regmap, false);
			regcache_sync(cs42l52->regmap);
		}
		snd_soc_write(codec, CS42L52_PWRCTL1, CS42L52_PWRCTL1_PDN_ALL);
		break;
	case SND_SOC_BIAS_OFF:
		snd_soc_write(codec, CS42L52_PWRCTL1, CS42L52_PWRCTL1_PDN_ALL);
		regcache_cache_only(cs42l52->regmap, true);
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}

#define CS42L52_RATES (SNDRV_PCM_RATE_8000_96000)

#define CS42L52_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | \
			SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_U18_3LE | \
			SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_U20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_U24_LE)

static struct snd_soc_dai_ops cs42l52_ops = {
	.hw_params	= cs42l52_pcm_hw_params,
	.digital_mute	= cs42l52_digital_mute,
	.set_fmt	= cs42l52_set_fmt,
	.set_sysclk	= cs42l52_set_sysclk,
};

static struct snd_soc_dai_driver cs42l52_dai = {
		.name = "cs42l52",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CS42L52_RATES,
			.formats = CS42L52_FORMATS,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CS42L52_RATES,
			.formats = CS42L52_FORMATS,
		},
		.ops = &cs42l52_ops,
};

#ifdef CONFIG_PM
static int cs42l52_suspend(struct snd_soc_codec *codec)
{
	cs42l52_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int cs42l52_resume(struct snd_soc_codec *codec)
{
	cs42l52_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}
#else
#define cs42l52_suspend NULL
#define cs42l52_resume NULL
#endif

static int beep_rates[] = {
	261, 522, 585, 667, 706, 774, 889, 1000,
	1043, 1200, 1333, 1412, 1600, 1714, 2000, 2182
};

static void cs42l52_beep_work(struct work_struct *work)
{
	struct cs42l52_private *cs42l52 =
		container_of(work, struct cs42l52_private, beep_work);
	struct snd_soc_codec *codec = cs42l52->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int i;
	int val = 0;
	int best = 0;

	if (cs42l52->beep_rate) {
		for (i = 0; i < ARRAY_SIZE(beep_rates); i++) {
			if (abs(cs42l52->beep_rate - beep_rates[i]) <
				abs(cs42l52->beep_rate - beep_rates[best]))
				best = i;
		}

		dev_dbg(codec->dev, "Set beep rate %dHz for requested %dHz\n",
			beep_rates[best], cs42l52->beep_rate);

		val = best;

		snd_soc_dapm_enable_pin(dapm, "Beep");
	} else {
		dev_dbg(codec->dev, "Disabling beep\n");
		snd_soc_dapm_disable_pin(dapm, "Beep");
	}

	cs42l52_snd_soc_update_bits(codec,
				CS42L52_BEEP_FREQ_ONTIME,
				CS42L52_BEEP_FREQ_ONTIME_FREQ_MASK,
				CS42L52_BEEP_FREQ_ONTIME_FREQ_SHIFT,
				val);

	snd_soc_dapm_sync(dapm);
}

/* For usability define a way of injecting beep events for the device -
 * many systems will not have a keyboard.
 */
static int cs42l52_beep_event(struct input_dev *dev, unsigned int type,
					unsigned int code, int hz)
{
	struct snd_soc_codec *codec = input_get_drvdata(dev);
	struct cs42l52_private *cs42l52 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "Beep event %x %x\n", code, hz);

	switch (code) {
	case SND_BELL:
		if (hz)
			hz = 261;
	case SND_TONE:
		break;
	default:
		return -1;
	}

	/* Kick the beep from a workqueue */
	cs42l52->beep_rate = hz;
	schedule_work(&cs42l52->beep_work);
	return 0;
}

static ssize_t cs42l52_beep_set(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct cs42l52_private *cs42l52 = dev_get_drvdata(dev);
	long int time;
	int ret;

	ret = kstrtol(buf, 10, &time);
	if (ret != 0)
		return ret;

	input_event(cs42l52->beep, EV_SND, SND_TONE, time);

	return count;
}

static DEVICE_ATTR(beep, 0200, NULL, cs42l52_beep_set);

static void cs42l52_init_beep(struct snd_soc_codec *codec)
{
	struct cs42l52_private *cs42l52 = snd_soc_codec_get_drvdata(codec);
	int ret;

	cs42l52->beep = devm_input_allocate_device(codec->dev);
	if (!cs42l52->beep) {
		dev_err(codec->dev, "Failed to allocate beep device\n");
		return;
	}

	INIT_WORK(&cs42l52->beep_work, cs42l52_beep_work);
	cs42l52->beep_rate = 0;

	cs42l52->beep->name = "CS42L52 Beep Generator";
	cs42l52->beep->phys = dev_name(codec->dev);
	cs42l52->beep->id.bustype = BUS_I2C;

	cs42l52->beep->evbit[0] = BIT_MASK(EV_SND);
	cs42l52->beep->sndbit[0] = BIT_MASK(SND_BELL) | BIT_MASK(SND_TONE);
	cs42l52->beep->event = cs42l52_beep_event;
	cs42l52->beep->dev.parent = codec->dev;
	input_set_drvdata(cs42l52->beep, codec);

	ret = input_register_device(cs42l52->beep);
	if (ret != 0) {
		cs42l52->beep = NULL;
		dev_err(codec->dev, "Failed to register beep device\n");
	}

	ret = device_create_file(codec->dev, &dev_attr_beep);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to create keyclick file: %d\n",
			ret);
	}
}

static void cs42l52_free_beep(struct snd_soc_codec *codec)
{
	struct cs42l52_private *cs42l52 = snd_soc_codec_get_drvdata(codec);

	device_remove_file(codec->dev, &dev_attr_beep);
	cancel_work_sync(&cs42l52->beep_work);
	cs42l52->beep = NULL;

	snd_soc_update_bits(codec, CS42L52_BEEP_TONE_CTL,
				CS42L52_BEEP_EN_MASK, 0);
}

static int cs42l52_probe(struct snd_soc_codec *codec)
{
	struct cs42l52_private *cs42l52 = snd_soc_codec_get_drvdata(codec);
	int ret;

	codec->control_data = cs42l52->regmap;
	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_REGMAP);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}
	regcache_cache_only(cs42l52->regmap, true);

	cs42l52_add_mic_controls(codec);

	cs42l52_init_beep(codec);

	cs42l52_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	cs42l52->sysclk = CS42L52_DEFAULT_CLK;
	cs42l52->config.format = CS42L52_DEFAULT_FORMAT;

	/* if Single Ended, Get Mic_Select */
	cs42l52_snd_soc_update_bits(codec,
				CS42L52_MICA_CTL,
				CS42L52_MICX_CTL_CFG_MASK,
				CS42L52_MICX_CTL_CFG_SHIFT,
				cs42l52->pdata.mica_diff_cfg);

	cs42l52_snd_soc_update_bits(codec,
				CS42L52_MICB_CTL,
				CS42L52_MICX_CTL_CFG_MASK,
				CS42L52_MICX_CTL_CFG_SHIFT,
				cs42l52->pdata.micb_diff_cfg);

	cs42l52_snd_soc_update_bits(codec,
				CS42L52_PB_CTL2,
				CS42L52_PB_CTL2_SPK_MONO_MASK,
				CS42L52_PB_CTL2_SPK_MONO_SHIFT,
				cs42l52->pdata.spk_mono);

	/* Set Platform Charge Pump Freq */
	if (cs42l52->pdata.chgfreq)
		cs42l52_snd_soc_update_bits(codec,
					CS42L52_CHARGE_PUMP,
					CS42L52_CHARGE_PUMP_MASK,
					CS42L52_CHARGE_PUMP_SHIFT,
					cs42l52->pdata.chgfreq);

	/* Set Platform Bias Level */
	if (cs42l52->pdata.micbias_lvl)
		cs42l52_snd_soc_update_bits(codec,
					CS42L52_IFACE_CTL2,
					CS42L52_IFACE_CTL2_BIAS_LVL_MASK,
					CS42L52_IFACE_CTL2_BIAS_LVL_SHIFT,
					cs42l52->pdata.micbias_lvl);

	if (gpio_is_valid(cs42l52->pdata.reset_gpio)) {
		ret = devm_gpio_request_one(cs42l52->dev,
							cs42l52->pdata.reset_gpio,
							GPIOF_OUT_INIT_HIGH,
							"CS42L52 /RST");
		if (ret < 0) {
			dev_err(cs42l52->dev, "Failed to request /RST %d: %d\n",
				cs42l52->pdata.reset_gpio, ret);
			return ret;
		}
		gpio_set_value_cansleep(cs42l52->pdata.reset_gpio, 0);
		gpio_set_value_cansleep(cs42l52->pdata.reset_gpio, 1);
	}

	return ret;
}

static int cs42l52_remove(struct snd_soc_codec *codec)
{
	cs42l52_free_beep(codec);
	cs42l52_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_cs42l52 = {
	.probe = cs42l52_probe,
	.remove = cs42l52_remove,
	.suspend = cs42l52_suspend,
	.resume = cs42l52_resume,
	.set_bias_level = cs42l52_set_bias_level,

	.dapm_widgets = cs42l52_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cs42l52_dapm_widgets),
	.dapm_routes = cs42l52_audio_map,
	.num_dapm_routes = ARRAY_SIZE(cs42l52_audio_map),

	.controls = cs42l52_snd_controls,
	.num_controls = ARRAY_SIZE(cs42l52_snd_controls),
};

/* Current and threshold powerup sequence Pg37 */
static const struct reg_default cs42l52_threshold_patch[] = {

	{ 0x00, 0x99 },
	{ 0x3E, 0xBA },
	{ 0x47, 0x80 },
	{ 0x32, 0xBB },
	{ 0x32, 0x3B },
	{ 0x00, 0x00 },

};

static struct regmap_config cs42l52_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = CS42L52_MAX_REGISTER,
	.reg_defaults = cs42l52_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(cs42l52_reg_defaults),
	.readable_reg = cs42l52_readable_register,
	.volatile_reg = cs42l52_volatile_register,
	.cache_type = REGCACHE_RBTREE,
};

static int cs42l52_i2c_probe(struct i2c_client *i2c_client,
					const struct i2c_device_id *id)
{
	struct cs42l52_private *cs42l52;
	struct cs42l52_platform_data *pdata = dev_get_platdata(&i2c_client->dev);
	int ret;
	unsigned int devid = 0;
	unsigned int reg;
	u32 val32;

	cs42l52 = devm_kzalloc(&i2c_client->dev, sizeof(struct cs42l52_private),
						GFP_KERNEL);
	if (cs42l52 == NULL)
		return -ENOMEM;
	cs42l52->dev = &i2c_client->dev;

	cs42l52->regmap = devm_regmap_init_i2c(i2c_client, &cs42l52_regmap);
	if (IS_ERR(cs42l52->regmap)) {
		ret = PTR_ERR(cs42l52->regmap);
		dev_err(&i2c_client->dev, "regmap_init() failed: %d\n", ret);
		return ret;
	}
	if (pdata) {
		cs42l52->pdata = *pdata;
	} else {
		pdata = devm_kzalloc(&i2c_client->dev,
							sizeof(struct cs42l52_platform_data),
							GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c_client->dev, "could not allocate pdata\n");
			return -ENOMEM;
		}
		if (i2c_client->dev.of_node) {
			if (of_property_read_bool(i2c_client->dev.of_node,
				"cirrus,mica-differential-cfg"))
				pdata->mica_diff_cfg = true;

			if (of_property_read_bool(i2c_client->dev.of_node,
				"cirrus,micb-differential-cfg"))
				pdata->micb_diff_cfg = true;

			if (of_property_read_u32(i2c_client->dev.of_node,
				"cirrus,micbias-lvl", &val32) >= 0)
				pdata->micbias_lvl = val32;

			if (of_property_read_u32(i2c_client->dev.of_node,
				"cirrus,chgfreq-divisor", &val32) >= 0)
				pdata->chgfreq = val32;

			if (of_property_read_bool(i2c_client->dev.of_node,
				"cirrus,spk-mono"))
				pdata->spk_mono = true;

			pdata->reset_gpio =
				of_get_named_gpio(i2c_client->dev.of_node,
						"reset-gpio", 0);
		}
		cs42l52->pdata = *pdata;
	}

	i2c_set_clientdata(i2c_client, cs42l52);

	ret = regmap_register_patch(cs42l52->regmap, cs42l52_threshold_patch,
					ARRAY_SIZE(cs42l52_threshold_patch));
	if (ret != 0)
		dev_warn(cs42l52->dev, "Failed to apply regmap patch: %d\n",
				ret);

	ret = regmap_read(cs42l52->regmap, CS42L52_CHIP, &reg);
	devid = reg & CS42L52_CHIP_ID_MASK;
	if (devid != CS42L52_CHIP_ID) {
		ret = -ENODEV;
		dev_err(&i2c_client->dev,
			"CS42L52 Device ID (%X). Expected %X\n",
			devid, CS42L52_CHIP_ID);
		return ret;
	}

	dev_info(&i2c_client->dev, "Cirrus Logic CS42L52, Revision: %02X\n",
			reg & CS42L52_CHIP_REV_MASK);

	regcache_cache_only(cs42l52->regmap, true);

	ret = snd_soc_register_codec(&i2c_client->dev,
			&soc_codec_dev_cs42l52, &cs42l52_dai, 1);
	if (ret < 0)
		return ret;
	return 0;
}

static int cs42l52_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct of_device_id cs42l52_of_match[] = {
	{ .compatible = "cirrus,cs42l52", },
	{},
};
MODULE_DEVICE_TABLE(of, cs42l52_of_match);


static const struct i2c_device_id cs42l52_id[] = {
	{ "cs42l52", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cs42l52_id);

static struct i2c_driver cs42l52_i2c_driver = {
	.driver = {
		.name = "cs42l52",
		.owner = THIS_MODULE,
		.of_match_table = cs42l52_of_match,
	},
	.id_table = cs42l52_id,
	.probe = cs42l52_i2c_probe,
	.remove = cs42l52_i2c_remove,
};

module_i2c_driver(cs42l52_i2c_driver);

MODULE_DESCRIPTION("ASoC CS42L52 driver");
MODULE_AUTHOR("Georgi Vlaev, Nucleus Systems Ltd, <joe@nucleusys.com>");
MODULE_AUTHOR("Brian Austin, Cirrus Logic Inc, <brian.austin@cirrus.com>");
MODULE_LICENSE("GPL");
