/*
 * amlogic atv demod driver
 *
 * Author: nengwen.chen <nengwen.chen@amlogic.com>
 *
 *
 * Copyright (C) 2018 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ATV_DEMOD_DRIVER_H__
#define __ATV_DEMOD_DRIVER_H__

extern int atvdemod_debug_en;

#include <media/v4l2-device.h>
#include "drivers/media/dvb-core/dvb_frontend.h"
#include "atv_demod_v4l2.h"

enum aml_tuner_type_t {
	AM_TUNER_SI2176 = 1,
	AM_TUNER_SI2196 = 2,
	AM_TUNER_FQ1216 = 3,
	AM_TUNER_HTM = 4,
	AM_TUNER_CTC703 = 5,
	AM_TUNER_SI2177 = 6,
	AM_TUNER_R840 = 7,
	AM_TUNER_SI2157 = 8,
	AM_TUNER_SI2151 = 9,
	AM_TUNER_MXL661 = 10,
	AM_TUNER_MXL608 = 11
};

struct aml_atvdemod_parameters {

	struct analog_parameters param;

	unsigned int soundsys;/* A2,BTSC/EIAJ/NICAM */
	unsigned int lock_range;
	unsigned int leap_step;

	unsigned int afc_range;
	unsigned int tuner_id;
	unsigned int if_freq;
	unsigned int if_inv;
	unsigned int reserved;
};

struct aml_atvdemod_device {
	char *name;
	struct class cls;
	struct device *dev;

	unsigned int tuner_id;
	unsigned int i2c_addr;
	unsigned int i2c_adapter_id;
	struct i2c_adapter *i2c_adp;

	unsigned int if_freq;
	unsigned int if_inv;
	u64 std;

	int fre_offset;
	struct pinctrl *pin;
	const char *pin_name;

	struct v4l2_adapter v4l2_ad;
	struct v4l2_frontend v4l2_fe;

	void __iomem *demod_reg_base;
	void __iomem *audio_reg_base;
	void __iomem *hiu_reg_base;
	void __iomem *periphs_reg_base;

	unsigned int reg_23cf; /* IIR filter */
	int btsc_sap_mode; /*0: off 1:monitor 2:auto */

#define ATVDEMOD_STATE_IDEL  0
#define ATVDEMOD_STATE_WORK  1
#define ATVDEMOD_STATE_SLEEP 2
	int atvdemod_state;

	int (*demod_reg_write)(unsigned int reg, unsigned int val);
	int (*demod_reg_read)(unsigned int reg, unsigned int *val);

	int (*audio_reg_write)(unsigned int reg, unsigned int val);
	int (*audio_reg_read)(unsigned int reg, unsigned int *val);

	int (*hiu_reg_write)(unsigned int reg, unsigned int val);
	int (*hiu_reg_read)(unsigned int reg, unsigned int *val);

	int (*periphs_reg_write)(unsigned int reg, unsigned int val);
	int (*periphs_reg_read)(unsigned int reg, unsigned int *val);
};

extern struct aml_atvdemod_device *amlatvdemod_devp;

#endif /* __ATV_DEMOD_DRIVER_H__ */
