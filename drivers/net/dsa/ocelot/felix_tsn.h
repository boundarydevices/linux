/* SPDX-License-Identifier: (GPL-2.0 OR MIT)
 *
 * Felix Switch TSN driver
 *
 * Copyright 2020 NXP
 */

#ifndef _MSCC_FELIX_SWITCH_TSN_H_
#define _MSCC_FELIX_SWITCH_TSN_H_

#include <soc/mscc/ocelot.h>
#include <net/dsa.h>

void felix_preempt_irq_clean(struct ocelot *ocelot);
void felix_cbs_reset(struct ocelot *ocelot, int port, int speed);
int felix_tsn_enable(struct dsa_switch *ds);
#endif
