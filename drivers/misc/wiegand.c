/*Sends Wiegand signals out J46. GPIO7=Data0 and GPIO8=Data1 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
/* Required for the GPIO functions */
#include <linux/gpio.h>
/* Using kobjects for the sysfs bindings */
#include <linux/kobject.h>
/* Using kthreads for the flashing functionality */
#include <linux/kthread.h>
/* Using this header for the msleep() function */
#include <linux/delay.h>
/* gettimeofday */
#include <linux/time.h>
#include <linux/gpio.h>

#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/sched.h>

struct wiegand_data {
	struct gpio_desc *gpd_data0;	/* Nit6xlite = GPIO7 */
	struct gpio_desc *gpd_data1;	/* Nit6xlite = GPIO8 */

	/*Timer Variables */
	struct hrtimer htimer;
	ktime_t kt_period;

	/* Current time within 2050us period */
	unsigned int cur_time;
	struct mutex mlock;
	unsigned int length;
	unsigned long long int this_scan;

	struct timespec64 last_time;
	unsigned long long int t_mask;

/* Sending states */
#define IDLE 0
#define DRIVE 1
#define STOP 2
#define END 3
	int state;

	struct kobject *wiegand_kobj;
	unsigned int set0;
	unsigned int set1;
};

static struct wiegand_data *g_wie;

/*
 * Wiegand is active-low, but our gp's have an inverter,
 * making the gp active-high, writing 1 is actually 0v
 * after the inverter.
 * Hold desired bit low for 50us, then wait 2ms, then repeat
 * Trail with 50ms before new data.
 */
static enum hrtimer_restart function_timer(struct hrtimer *timer)
{
	struct wiegand_data *wie = container_of(timer, struct wiegand_data,
			htimer);
	ktime_t kt_now;
	ktime_t now, done;

	switch (wie->state) {
	case DRIVE:
		if (wie->this_scan & wie->t_mask) {
			gpiod_set_value(wie->gpd_data1, 1);
			wie->set1 = 1;
		} else {
			gpiod_set_value(wie->gpd_data0, 1);
			wie->set0 = 1;
		}
		wie->t_mask >>= 1;

		/*
		 * same as hrtimer_cb_get_time, but works even if
		 * CONFIG_HIGH_RES_TIMERS not set
		 */
		now = timer->base->get_time();
		done = ktime_add_ns(now, 50000);

		/*
		 * Busy wait to prevent OS from switching context,
		 * allows more precise timing
		 */
		while (1) {
			now = timer->base->get_time();
			if (ktime_compare(now, done) >= 0)
				break;
		}

		if (wie->set0) {
			gpiod_set_value(wie->gpd_data0, 0);
			wie->set0 = 0;
		} else if (wie->set1) {
			gpiod_set_value(wie->gpd_data1, 0);
			wie->set1 = 0;
		}
		if (wie->t_mask) {
			wie->state = DRIVE;
			/* 2ms time between bit drives */
			wie->kt_period = ktime_set(0, 2000000);
		} else {
			wie->state = END;
			/* 50ms time between word transmits */
			wie->kt_period = ktime_set(0, 50000000);
		}
		kt_now = hrtimer_cb_get_time(&wie->htimer);
		hrtimer_forward(&wie->htimer, kt_now, wie->kt_period);
		wie->state = wie->t_mask ? DRIVE : END;
		return HRTIMER_RESTART;

	case END:
		wie->state = IDLE;
		mutex_unlock(&wie->mlock);
		return HRTIMER_NORESTART;
	}

	/* Should never get here, but just in case stop the timer. */
	return HRTIMER_NORESTART;
}

/*Function called when /sys/kernel/wiegand/length file is read */
static ssize_t length_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	struct wiegand_data *wie = g_wie;

	return sprintf(buf, "%d\n", wie->length);
}

/*Function called when /sys/kernel/wiegand/length file is written to */
static ssize_t length_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	struct wiegand_data *wie = g_wie;
	/*
	 * Using a variable to validate the data sent
	 * Read in the inputsetting as an unsigned int
	 */
	unsigned int lengthsetting;
	int ret;

	ret = sscanf(buf, "%du", &lengthsetting);
	if (ret < 0)
		return ret;
	if ((lengthsetting >= 26) && (lengthsetting <= 64))
		wie->length = lengthsetting;
	return lengthsetting;
}

static ssize_t busy_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	struct wiegand_data *wie = g_wie;

	return sprintf(buf, "%d\n", mutex_is_locked(&wie->mlock) ? 1 : 0);
}

static ssize_t hexval_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	struct wiegand_data *wie = g_wie;

	return sprintf(buf, "%llx\n", wie->this_scan);
}

/*
 * Only store when either 5 seconds has passed or a new scan is
 * detected that is not a duplicate.
 */
static ssize_t hexval_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	struct wiegand_data *wie = g_wie;
	int err;
	int length;
	unsigned long long int scan;

	err = kstrtoull(buf, 16, &scan);
	if (err)
		return err;

	if (scan == wie->this_scan) {
		struct timespec64 this_time;

		ktime_get_real_ts64(&this_time);
		/*Debounce inputs for 5 seconds */
		if (this_time.tv_sec <= wie->last_time.tv_sec+5)
			return -EINVAL;
	}
	pr_info("Wiegand: Got hexval input: %llx\n", scan);

	/*
	 * Handles 26/37 bit standard wiegand which works on
	 * the little rs232 converter unit I have
	 */
	length = wie->length;
	mutex_lock(&wie->mlock);
	wie->this_scan = scan;
	wie->cur_time = 0;
	/* Enable the timer */
	wie->state = DRIVE;
	wie->t_mask = 1ULL << (length - 1);
	wie->kt_period = ktime_set(0, 50000);
	hrtimer_start(&wie->htimer, wie->kt_period,
			HRTIMER_MODE_REL);
	return count;
}

static struct kobj_attribute length_attr = __ATTR(length, 0660,
		length_show, length_store);
static struct kobj_attribute busy_attr = __ATTR(busy, 0660, busy_show, NULL);
static struct kobj_attribute hexval_attr = __ATTR(hexval, 0660,
		hexval_show, hexval_store);

static struct attribute *wiegand_attrs[] = {
	&length_attr.attr,		/* WiegandNN where 37 >= NN >= 26 */
	&busy_attr.attr,
	&hexval_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = wiegand_attrs,
};

static int wiegand_probe(struct platform_device *pdev)
{
	struct wiegand_data *wie;
	struct device *dev = &pdev->dev;
	int ret = 0;
	struct gpio_desc *gp;
	u32 length;

	pr_info("Wiegand: Initializing the Wiegand LKM\n");

	wie = devm_kzalloc(dev, sizeof(struct wiegand_data), GFP_KERNEL);
	if (!wie)
		return -ENOMEM;
	mutex_init(&wie->mlock);
	wie->length = 26;
	hrtimer_init(&wie->htimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	wie->htimer.function = &function_timer;

	/* Request GPIOS */
	gp = devm_gpiod_get_index(dev, "data0", 0, GPIOD_OUT_LOW);
	if (IS_ERR(gp)) {
		dev_alert(dev,
			"Wiegand: Error requesting Data0. %ld.\n", PTR_ERR(gp));
		return PTR_ERR(gp);

	}
	wie->gpd_data0 = gp;
	gp = devm_gpiod_get_index(dev, "data1", 0, GPIOD_OUT_LOW);
	if (IS_ERR(gp)) {
		dev_alert(dev,
			"Wiegand: Error requesting Data1. %ld.\n", PTR_ERR(gp));
		return PTR_ERR(gp);

	}
	wie->gpd_data1 = gp;

	/*
	 * export to /sys/class/gpio/gpio# for useful debugging
	 * the second argument prevents the direction from being changed
	 * Causes /sys/class/gpio/gpio7 to appear
	 */
	gpiod_export(wie->gpd_data0, false);
	/* Causes /sys/class/gpio/gpio8 to appear */
	gpiod_export(wie->gpd_data1, false);

	ret = of_property_read_u32(dev->of_node, "length", &length);
	if (!ret)
		wie->length = length;

	ktime_get_real_ts64(&wie->last_time);
	platform_set_drvdata(pdev, wie);
	g_wie = wie;

	/* /sys/kernel/wiegand/ setup there */
	wie->wiegand_kobj = kobject_create_and_add("wiegand", kernel_kobj);
	if (!wie->wiegand_kobj) {
		dev_alert(dev, "Wiegand: failed to create kobject\n");
		return -ENOMEM;
	}
	/* add the attributes to /sys/kernel/wiegand/ -- for example,
	 * /sys/kernel/wiegand/mode
	 */
	ret = sysfs_create_group(wie->wiegand_kobj, &attr_group);
	if (ret) {
		dev_alert(dev, "Wiegand: failed to create sysfs group\n");
		/* clean up -- remove the kobject sysfs entry */
		kobject_put(wie->wiegand_kobj);
	}
	return ret;
}

static void wiegand_shutdown(struct platform_device *pdev)
{
	struct wiegand_data *wie = platform_get_drvdata(pdev);

	if (!wie)
		return;
	/* Cancel timer */
	hrtimer_cancel(&wie->htimer);
	/* clean up -- remove the kobject sysfs entry */
	kobject_put(wie->wiegand_kobj);
	gpiod_unexport(wie->gpd_data0);
	gpiod_unexport(wie->gpd_data1);
	pr_info("Wiegand: Shutting down.\n");
}

static const struct of_device_id of_wiegand_match[] = {
	{ .compatible = "wiegand,scanout", },
	{},
};

MODULE_DEVICE_TABLE(of, of_wiegand_match);

static struct platform_driver wiegand_driver = {
	.probe		= wiegand_probe,
	.shutdown	= wiegand_shutdown,
	.driver		= {
		.name	= "wiegand",
		.of_match_table = of_wiegand_match,
	},
};

module_platform_driver(wiegand_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ian Coolidge");
MODULE_AUTHOR("Troy Kisky");
MODULE_DESCRIPTION("Wiegand input/output reader/generator");
MODULE_VERSION("0.1");
