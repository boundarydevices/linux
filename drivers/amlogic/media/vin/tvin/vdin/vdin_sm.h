/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_sm.h
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

#ifndef __TVIN_STATE_MACHINE_H
#define __TVIN_STATE_MACHINE_H

#include "vdin_drv.h"

enum tvin_sm_status_e {
/* processing status from init to the finding of the 1st confirmed status */
	TVIN_SM_STATUS_NULL = 0,
	/* no signal - physically no signal */
	TVIN_SM_STATUS_NOSIG,
	/* unstable - physically bad signal */
	TVIN_SM_STATUS_UNSTABLE,
	 /* not supported - physically good signal & not supported */
	TVIN_SM_STATUS_NOTSUP,

	TVIN_SM_STATUS_PRESTABLE,
	 /* stable - physically good signal & supported */
	TVIN_SM_STATUS_STABLE,
};
struct tvin_sm_s {
	enum tvin_sig_status_e sig_status;
	enum tvin_sm_status_e state;
	unsigned int state_cnt; /* STATE_NOSIG, STATE_UNSTABLE */
	unsigned int exit_nosig_cnt; /* STATE_NOSIG */
	unsigned int back_nosig_cnt; /* STATE_UNSTABLE */
	unsigned int back_stable_cnt; /* STATE_UNSTABLE */
	unsigned int exit_prestable_cnt; /* STATE_PRESTABLE */
	/* thresholds of state switchted */
	int back_nosig_max_cnt;
	int atv_unstable_in_cnt;
	int atv_unstable_out_cnt;
	int atv_stable_out_cnt;
	int hdmi_unstable_out_cnt;
};

extern bool manual_flag;

void tvin_smr(struct vdin_dev_s *pdev);
void tvin_smr_init(int index);
void reset_tvin_smr(unsigned int index);

enum tvin_sm_status_e tvin_get_sm_status(int index);

#endif

