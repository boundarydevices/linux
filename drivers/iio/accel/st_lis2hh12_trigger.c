/*
 * STMicroelectronics lis2hh12 driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Armando Visconti <armando.visconti@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/iio/iio.h>
#include <linux/iio/trigger.h>
#include <linux/interrupt.h>
#include <linux/iio/events.h>

#include "st_lis2hh12.h"

static irqreturn_t lis2hh12_irq_management(int irq, void *private)
{
	struct lis2hh12_data *cdata = private;
	u8 status;
	struct timespec ts;

	get_monotonic_boottime(&ts);
	cdata->timestamp = timespec_to_ns(&ts);

	if (cdata->hwfifo_enabled) {
		cdata->tf->read(cdata, LIS2HH12_FIFO_STATUS_ADDR, 1, &status);

		if (status & LIS2HH12_FIFO_SRC_FTH_MASK)
			lis2hh12_read_fifo(cdata, true);
	} else {
		cdata->tf->read(cdata, LIS2HH12_STATUS_ADDR, 1, &status);

		if (status & LIS2HH12_DATA_XYZ_RDY)
			lis2hh12_read_xyz(cdata);
	}

	return IRQ_HANDLED;
}

int lis2hh12_allocate_triggers(struct lis2hh12_data *cdata,
			     const struct iio_trigger_ops *trigger_ops)
{
	int err, i, n;

	for (i = 0; i < LIS2HH12_SENSORS_NUMB; i++) {
		cdata->iio_trig[i] = iio_trigger_alloc("%s-trigger",
						cdata->iio_sensors_dev[i]->name);
		if (!cdata->iio_trig[i]) {
			dev_err(cdata->dev, "failed to allocate iio trigger.\n");
			err = -ENOMEM;

			goto deallocate_trigger;
		}
		iio_trigger_set_drvdata(cdata->iio_trig[i],
						cdata->iio_sensors_dev[i]);
		cdata->iio_trig[i]->ops = trigger_ops;
		cdata->iio_trig[i]->dev.parent = cdata->dev;
	}

	err = request_threaded_irq(cdata->irq, NULL, lis2hh12_irq_management,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT, cdata->name, cdata);
	if (err)
		goto deallocate_trigger;

	for (n = 0; n < LIS2HH12_SENSORS_NUMB; n++) {
		err = iio_trigger_register(cdata->iio_trig[n]);
		if (err < 0) {
			dev_err(cdata->dev, "failed to register iio trigger.\n");

			goto free_irq;
		}
		cdata->iio_sensors_dev[n]->trig = cdata->iio_trig[n];
	}

	return 0;

free_irq:
	free_irq(cdata->irq, cdata);
	for (n--; n >= 0; n--)
		iio_trigger_unregister(cdata->iio_trig[n]);
deallocate_trigger:
	for (i--; i >= 0; i--)
		iio_trigger_free(cdata->iio_trig[i]);

	return err;
}
EXPORT_SYMBOL(lis2hh12_allocate_triggers);

void lis2hh12_deallocate_triggers(struct lis2hh12_data *cdata)
{
	int i;

	free_irq(cdata->irq, cdata);

	for (i = 0; i < LIS2HH12_SENSORS_NUMB; i++)
		iio_trigger_unregister(cdata->iio_trig[i]);
}
EXPORT_SYMBOL(lis2hh12_deallocate_triggers);

MODULE_DESCRIPTION("STMicroelectronics lis2hh12 driver");
MODULE_AUTHOR("Armando Visconti <armando.visconti@st.com>");
MODULE_LICENSE("GPL v2");
