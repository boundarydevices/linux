/*
 * drivers/amlogic/audiodsp/dsp_monitor.c
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

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/cache.h>
#include <asm/cacheflush.h>

#include "dsp_microcode.h"
#include "audiodsp_module.h"
#include "dsp_control.h"
/* #include <asm/dsp/dsp_register.h> */

static void audiodsp_monitor(unsigned long arg)
{
	struct audiodsp_priv *priv = (struct audiodsp_priv *)arg;
	static unsigned long old_dsp_jiffies;
	static unsigned long jiffies_error;
	unsigned long dsp_jiffies;
	unsigned long dsp_status;

	dsp_jiffies = DSP_RD(DSP_JIFFIES);
	dsp_status = DSP_RD(DSP_STATUS);

	if (old_dsp_jiffies == dsp_jiffies)
		jiffies_error++;
	else {
		jiffies_error = 0;
		old_dsp_jiffies = dsp_jiffies;
	}
	if (jiffies_error > 5) {
		DSP_PRNT("Found audio dsp have some problem not running\n");
		DSP_PRNT("audio jiffies=%ld\n", old_dsp_jiffies);
		DSP_PRNT("audio status=%lx\n", dsp_status);
	}
	if (!dsp_check_status(priv))
		priv->dsp_abnormal_count++;
	else
		priv->dsp_abnormal_count = 0;

	if (priv->dsp_abnormal_count > 5 || jiffies_error > 5) {
		priv->decode_fatal_err |= 0x2;
		priv->dsp_abnormal_count = 0;
		jiffies_error = 0;
	}
	priv->dsp_mointer.expires = jiffies + HZ;
	add_timer(&priv->dsp_mointer);
}

void start_audiodsp_monitor(struct audiodsp_priv *priv)
{
	priv->dsp_mointer.expires = jiffies;
	add_timer(&priv->dsp_mointer);
}
void stop_audiodsp_monitor(struct audiodsp_priv *priv)
{
	del_timer_sync(&priv->dsp_mointer);
}

void init_audiodsp_monitor(struct audiodsp_priv *priv)
{
	setup_timer(&priv->dsp_mointer, audiodsp_monitor, (unsigned long)priv);
}

void release_audiodsp_monitor(struct audiodsp_priv *priv)
{
	stop_audiodsp_monitor(priv);
}
