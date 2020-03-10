/*
 * STMicroelectronics lps33hw driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Matteo Dameno <matteo.dameno@st.com>
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

#include "st_lps33hw.h"

static irqreturn_t lps33hw_irq_handler(int irq, void *private)
{
	struct lps33hw_data *cdata = private;

	cdata->prev_timestamp = cdata->sensor_timestamp;
	cdata->sensor_timestamp = lps33hw_get_time_ns();
	/* Delta timestamp between start/watermark events.*/
	cdata->delta_ts = cdata->sensor_timestamp - cdata->prev_timestamp;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t lps33hw_irq_thread(int irq, void *private)
{
	struct lps33hw_data *cdata = private;
	u8 fifo_src;

	cdata->tf->read(cdata, LPS33HW_FIFO_SRC_ADDR, 1, &fifo_src);

	mutex_lock(&cdata->fifo_lock);
	if (fifo_src & LPS33HW_FIFO_SRC_FTH_MASK)
		lps33hw_flush_fifo(cdata, fifo_src);
	mutex_unlock(&cdata->fifo_lock);

	return IRQ_HANDLED;
}

int lps33hw_allocate_triggers(struct lps33hw_data *cdata,
			     const struct iio_trigger_ops *trigger_ops)
{
	int err, i, n;

	for (i = 0; i < LPS33HW_SENSORS_NUMB; i++) {
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

	err = request_threaded_irq(cdata->irq, lps33hw_irq_handler, lps33hw_irq_thread,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT, cdata->name, cdata);
	if (err)
		goto deallocate_trigger;

	for (n = 0; n < LPS33HW_SENSORS_NUMB; n++) {
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
EXPORT_SYMBOL(lps33hw_allocate_triggers);

void lps33hw_deallocate_triggers(struct lps33hw_data *cdata)
{
	int i;

	free_irq(cdata->irq, cdata);

	for (i = 0; i < LPS33HW_SENSORS_NUMB; i++)
		iio_trigger_unregister(cdata->iio_trig[i]);
}
EXPORT_SYMBOL(lps33hw_deallocate_triggers);

MODULE_DESCRIPTION("STMicroelectronics lps33hw driver");
MODULE_AUTHOR("Armando Visconti <armando.visconti@st.com>");
MODULE_LICENSE("GPL v2");
