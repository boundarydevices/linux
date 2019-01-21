/*
 * include/linux/amlogic/media/sound/misc.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __MISC_H__
#define __MISC_H__

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/control.h>

#ifdef CONFIG_AMLOGIC_ATV_DEMOD
extern const struct soc_enum atv_audio_status_enum;

int aml_get_atv_audio_stable(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

#endif

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
extern int tvin_get_av_status(void);
extern const struct soc_enum av_audio_status_enum;

extern int aml_get_av_audio_stable(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
extern int update_spdifin_audio_type(int audio_type);

extern const struct soc_enum hdmi_in_status_enum[];

extern int get_hdmi_sample_rate_index(void);

extern int aml_get_hdmiin_audio_stable(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

extern int aml_get_hdmiin_audio_samplerate(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

extern int aml_get_hdmiin_audio_channels(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

extern int aml_get_hdmiin_audio_format(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

extern int aml_set_atmos_audio_edid(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

extern int aml_get_atmos_audio_edid(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

extern int aml_get_hdmiin_audio_packet(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

#endif

#endif
