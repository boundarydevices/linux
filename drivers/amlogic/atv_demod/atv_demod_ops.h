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

#ifndef __ATV_DEMOD_OPS_H__
#define __ATV_DEMOD_OPS_H__

#define AML_ATVDEMOD_UNINIT         0x0
#define AML_ATVDEMOD_INIT           0x1
#define AML_ATVDEMOD_RESUME         0x2
#define AML_ATVDEMOD_SCAN_MODE      0x3
#define AML_ATVDEMOD_UNSCAN_MODE    0x4

#define AFC_BEST_LOCK    50
#define ATV_AFC_500KHZ   500000
#define ATV_AFC_1_0MHZ   1000000
#define ATV_AFC_2_0MHZ   2000000

#include "drivers/media/dvb-core/dvb_frontend.h"
#include "drivers/media/tuners/tuner-i2c.h"

#include "atv_demod_driver.h"

struct atv_demod_sound_system {
	unsigned int broadcast_std;
	unsigned int audio_std;
	unsigned int input_mode;
	unsigned int output_mode;
};

struct atv_demod_priv {
	struct tuner_i2c_props i2c_props;
	struct list_head hybrid_tuner_instance_list;

	bool standby;

	struct aml_atvdemod_parameters atvdemod_param;
	struct atv_demod_sound_system sound_sys;
	struct work_struct demod_wq;
};

extern int atv_demod_enter_mode(void);

struct dvb_frontend *aml_atvdemod_attach(struct dvb_frontend *fe,
		struct v4l2_frontend *v4l2_fe,
		struct i2c_adapter *i2c_adap, u8 i2c_addr, u32 tuner_id);


#endif /* __ATV_DEMOD_OPS_H__ */
