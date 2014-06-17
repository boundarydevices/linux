/*
 * Copyright (C) 2005-2014 Freescale Semiconductor, Inc. All Rights Reserved.
 */
/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/pwm.h>
#include <linux/fsl_devices.h>
#include <linux/ir.h>
#include "mxc_ir.h"

#ifdef IR_HRTIMER_USED
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#else
#include "epit.h"
#endif

#define IR_MAX_CARRIER_FREQ (11000000)
#define IR_MIN_CARRIER_FREQ (3000)

struct mx6_ir_device {
	struct pwm_device	*pwm;
#ifdef IR_HRTIMER_USED
	struct hrtimer		hr_timer;
#else
	struct epit_device	*epit;
#endif
	int			pattern_len;
	int			*pattern;
	int			transmit_idx;
	int			transmit_level;
	struct device		*dev;
	wait_queue_head_t	queue;
	bool				cond;
	unsigned int		carry_freq;
};

struct carrier_freq {
	int min;
	int max;
};

static const struct carrier_freq mx6_ir_carrier_freq[] = {
	{IR_MIN_CARRIER_FREQ, IR_MAX_CARRIER_FREQ}, };

int ir_config(void *dev, int carry_freq)
{
	unsigned int period_ns;
	struct mx6_ir_device *ir_dev = (struct mx6_ir_device *)dev;

	if ((!ir_dev) || (!carry_freq))
		return -EINVAL;

	if (!ir_dev->pwm)
		return -EINVAL;

	period_ns = 1000000000;
	do_div(period_ns, carry_freq);
	pwm_config(ir_dev->pwm, (period_ns >> 1), period_ns);
	pwm_out_enable(ir_dev->pwm, 0);
	return 0;
}
EXPORT_SYMBOL(ir_config);

int ir_get_carrier_range(int id, int *min, int *max)
{
	if ((!min) || (!max)) {
		return -1;
	} else {
		*min = mx6_ir_carrier_freq[id].min;
		*max = mx6_ir_carrier_freq[id].max;
		return 0;
	}
}
EXPORT_SYMBOL(ir_get_carrier_range);

int ir_get_num_carrier_freqs(void)
{
	return ARRAY_SIZE(mx6_ir_carrier_freq);
}
EXPORT_SYMBOL(ir_get_num_carrier_freqs);


#ifdef IR_HRTIMER_USED
static void *hrtimer_cb_para;

static inline void ir_hrtimer_start(struct hrtimer *hr_timer, int width_ns,
				void *cb, void *para)
{
	ktime_t ktime;

	ktime = ktime_set(0, width_ns);
	hrtimer_init(hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer->function = cb;
	hrtimer_cb_para = para;
	hrtimer_start(hr_timer, ktime, HRTIMER_MODE_REL);
}

static inline void ir_hrtimer_stop(struct hrtimer *hr_timer)
{
	hrtimer_cancel(hr_timer);
}

static int ir_transmit_cb(struct hrtimer *timer)
{
	struct mx6_ir_device *ir_dev = (struct mx6_ir_device *)hrtimer_cb_para;
	ktime_t ktime;

	ir_dev->transmit_idx++;
	if (ir_dev->pattern_len == ir_dev->transmit_idx) {
		/* all bit transmit is done */
		/* stop pwm output */
		pwm_out_enable(ir_dev->pwm, 0);
		ir_dev->cond = true;
		wake_up(&ir_dev->queue);
		return HRTIMER_NORESTART;
	} else {
		/* start next bit transmit */;
		ir_dev->transmit_level = !ir_dev->transmit_level;
		ktime = ktime_set(0, ir_dev->pattern[ir_dev->transmit_idx] * 1000);
		hrtimer_forward(&ir_dev->hr_timer,
				ktime_get(),
				ktime);
		pwm_out_enable(ir_dev->pwm, ir_dev->transmit_level);
		return HRTIMER_RESTART;
	}
}

int ir_transmit(void *dev, int len, int *pattern, unsigned char start_level)
{
	struct mx6_ir_device *ir_dev = (struct mx6_ir_device *)dev;
	char level = start_level;

	if ((!ir_dev) || (!pattern))
		return -EINVAL;

	if (!ir_dev->pwm)
		return -EINVAL;

	ir_dev->transmit_level = start_level;
	ir_dev->transmit_idx = 0;
	ir_dev->pattern = pattern;
	ir_dev->pattern_len = len;
	ir_dev->cond = false;
	/*
	1. Enable/disable pwm output per level, start timer with width
	2. time cb will start next bit transmit,wait all bit transmit done
	3. disable pwm output
	*/
	pwm_enable(ir_dev->pwm);
	ir_dev->cond = false;
	ir_hrtimer_start(&ir_dev->hr_timer,
			(ir_dev->pattern[0] * 1000),
			(void *)ir_transmit_cb,
			(void *)(ir_dev));
	pwm_out_enable(ir_dev->pwm, ir_dev->transmit_level);
	wait_event(ir_dev->queue, ir_dev->cond);
	ir_hrtimer_stop(&ir_dev->hr_timer);
	pwm_disable(ir_dev->pwm);
	return 0;
}
#else
static int ir_transmit_cb(void *dev)
{
	struct mx6_ir_device *ir_dev = (struct mx6_ir_device *)dev;
	ir_dev->transmit_idx++;
	if (ir_dev->pattern_len == ir_dev->transmit_idx) {
		/* all bit transmit is done */
		/* stop pwm output */
		pwm_out_enable(ir_dev->pwm, 0);
		ir_dev->cond = true;
		wake_up(&ir_dev->queue);
	} else {
		/* start next bit transmit */;
		ir_dev->transmit_level = !ir_dev->transmit_level;
		pwm_out_enable(ir_dev->pwm, ir_dev->transmit_level);
		epit_start(ir_dev->epit,
			(ir_dev->pattern[ir_dev->transmit_idx] * 1000));

	}
	return 0;
}

int ir_transmit(void *dev, int len, int *pattern, unsigned char start_level)
{
	struct mx6_ir_device *ir_dev = (struct mx6_ir_device *)dev;

	if ((!ir_dev) || (!pattern))
		return -EINVAL;

	if ((!ir_dev->pwm) || (!ir_dev->epit))
		return -EINVAL;

	ir_dev->transmit_level = start_level;
	ir_dev->transmit_idx = 0;
	ir_dev->pattern = pattern;
	ir_dev->pattern_len = len;
	ir_dev->cond = false;
	/*
	1. Enable/disable pwm output per level, start timer with width
	2. time cb will start next bit transmit,wait all bit transmit done
	3. disable pwm output
	*/
	pwm_enable(ir_dev->pwm);
	epit_config(ir_dev->epit,
		EPIT_FREE_RUN_MODE,
		(void *)ir_transmit_cb,
		(void *)(ir_dev));
	epit_start(ir_dev->epit,
		(ir_dev->pattern[0] * 1000));
	pwm_out_enable(ir_dev->pwm, ir_dev->transmit_level);
	wait_event(ir_dev->queue, ir_dev->cond);
	epit_stop(ir_dev->epit);
	pwm_disable(ir_dev->pwm);
	return 0;
}
#endif
EXPORT_SYMBOL(ir_transmit);

#if defined(CONFIG_OF)
static const struct of_device_id mxc_ir_dt_ids[] = {
	{ .compatible = "fsl,mxc-ir"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mxc_ir_dt_ids);
#endif

static int mx6_ir_probe(struct platform_device *pdev)
{
	struct mx6_ir_device *pb;
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
#ifndef IR_HRTIMER_USED
	int epit_id = 0;
#endif

	pb = kzalloc(sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	pb->dev = &pdev->dev;

	pb->pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(pb->pwm)) {
		dev_err(&pdev->dev, "unable to request PWM.\n");
		ret = (PTR_ERR)(pb->pwm);
		goto err_pwm;
	}

#ifndef IR_HRTIMER_USED
	ret = of_property_read_u32(np, "epit", &epit_id);
	if (ret < 0) {
		dev_dbg(&pdev->dev, "can not get epit\n");
		goto err_pwm;
	}
	pb->epit = epit_request(epit_id, "IR Signal");
	if (IS_ERR(pb->epit)) {
		dev_err(&pdev->dev, "unable to request EPIT for IR Signal\n");
		ret = PTR_ERR(pb->epit);
		goto err_epit;
	} else {
		dev_dbg(&pdev->dev, "got EPIT for IR Signal\n");
	}
#endif
	/* Init queue */
	init_waitqueue_head(&pb->queue);
	ir_device_register(dev_name(&pdev->dev), &pdev->dev, pb);
	platform_set_drvdata(pdev, pb);

	return 0;

#ifndef IR_HRTIMER_USED
err_epit:
#endif
	devm_pwm_put(&pdev->dev, pb->pwm);
err_pwm:
	kfree(pb);
err_alloc:
	return ret;
}

static int mx6_ir_remove(struct platform_device *pdev)
{
	struct mx6_ir_device *pb = platform_get_drvdata(pdev);
	ir_device_unregister();
	pwm_disable(pb->pwm);
	devm_pwm_put(&pdev->dev, pb->pwm);
#ifndef IR_HRTIMER_USED
	epit_stop(pb->epit);
	epit_free(pb->epit);
#endif
	kfree(pb);
	return 0;
}

static struct platform_driver mx6_ir_driver = {
	.probe	= mx6_ir_probe,
	.remove	= mx6_ir_remove,
	.driver = {
		.name	= "mxc_ir",
		.owner	= THIS_MODULE,
		.of_match_table = mxc_ir_dt_ids,
	},
};

module_platform_driver(mx6_ir_driver)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("IR control interface on MX6 Platform");

