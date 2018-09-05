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
	/* wie->lock on inputs, user must read 0 before sending a hexval */
	unsigned int lock;
	unsigned int length;
	unsigned long long int this_scan;
	/* Default different value at startup than wie->this_scan */
	unsigned long long int last_scan;
	/*
	 * Stores payload of bits, same as wie->this_scan without top and bottom
	 * bits of parity
	 */
	unsigned long long int this_payload;

	struct timespec64 last_time;
	/* 26-37 bit parity variables */
	bool even_parity;
	bool odd_parity;
	/* 48 bit parity variables */
	bool odd48_all_parity;
	bool even48_parity;
	bool odd48_parity;
	unsigned long long int t_mask;

/* Sending wie->states */
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
 * ======================================================================
 *  26-37 bit parity verification functions
 * ======================================================================
 * even parity - top bits
 * parity is first floor( (wie->length-2)/2 ) bits
 */
static bool verify_even_parity(struct wiegand_data *wie)
{
	int i;
	unsigned int parity = 0;
	unsigned long long int mask = 1ULL << (wie->length - 2 - 1);
	int parity_bits = (wie->length - 2) / 2;

	for (i = 0; i < parity_bits; i++) {
		if (wie->this_payload & mask)
			parity++;
		mask >>= 1;
	}
	return (parity & 1) == 1;
}

/*
 * odd parity - bottom bits
 * parity is the rest of the bits
 * This is unlike the documentation for the converter,
 * but works with the converter.
 */
static bool verify_odd_parity(struct wiegand_data *wie)
{
	int i;
	unsigned int parity = 0;
	unsigned long long int mask = 1ULL << (wie->length - 2 - 1);
	int parity_bits = (wie->length - 2) / 2;

	mask >>= parity_bits;
	for (i = 0; i < (wie->length - 2 - parity_bits); i++) {
		if (wie->this_payload & mask)
			parity++;
		mask >>= 1;
	}
	return (parity & 1) == 0;
}

static bool verify_parity(struct wiegand_data *wie)
{
	bool ret;

	ret = verify_even_parity(wie);
	if (ret != wie->even_parity)
		return false;
	ret = verify_odd_parity(wie);
	if (ret != wie->odd_parity)
		return false;
	return true;
}
/*
 * End 26-37 bit parity verification functions
 * ======================================================================
 * 48 bit parity verification functions
 * ======================================================================
 * O(47*X)
 */
static bool verify48_odd_all_parity(struct wiegand_data *wie)
{
	int i;
	unsigned int parity = 0;
	/* Mask goes from 0X0...0 down to 0...X */
	unsigned long long int mask = 1ULL << (wie->length - 2);
	int parity_bits = 47;

	for (i = 0; i < parity_bits; i++) {
		if (wie->this_scan & mask)
			parity++;
		mask >>= 1;
	}
	return (parity & 1) == 0;
}
/*
 * ..(15*XX.)O
 *          PP110110110110110110110110110110110110110110110P
 * Mask goes X000000000000000000000000000000000000000000000
 * Down to   00000000000000000000000000000000000000000000X0
 * parity_bits = 45 (48 - 3 parity bits)
 */
static bool verify48_odd_parity(struct wiegand_data *wie)
{
	int i;
	unsigned int parity = 0;
	/*
	 * mask is 110110110110110110110110110110110110110110110
	 *  = 30158033218998 in decimal
	 * payload XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
	 * result  ?????????????????????????????????????????????
	 * iter    X00000000000000000000000000000000000000000000
	 * shift = 45 times
	 */
	unsigned long long int mask = 1ULL << (wie->length - 3);
	int parity_bits = 45;

	for (i = 0; i < parity_bits / 3; i++) {
		if (wie->this_scan & mask)
			parity++;
		mask >>= 1;
		if (wie->this_scan & mask)
			parity++;
		mask >>= 2;
	}
	return (parity & 1) == 0;
}

/*
 * .E(15*.XX).
 * mask is         011011011011011011011011011011011011011011011
 *  = 15079016609499 in decimal
 * payload is      XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 * result is       ?????????????????????????????????????????????
 * iter is         X00000000000000000000000000000000000000000000
 * shift = 45 times
 */
static bool verify48_even_parity(struct wiegand_data *wie)
{
	int i;
	unsigned int parity = 0;
	unsigned long long int mask = 1ULL << (wie->length - 3);
	int parity_bits = 45;

	for (i = 0; i < parity_bits / 3; i++) {
		mask >>= 1;
		if (wie->this_scan & mask)
			parity++;
		mask >>= 1;
		if (wie->this_scan & mask)
			parity++;
		mask >>= 1;
	}
	return (parity & 1) == 1;
}

static bool verify48_parity(struct wiegand_data *wie)
{
	bool ret;

	ret = verify48_odd_all_parity(wie);
	if (ret != wie->odd48_all_parity) {
		pr_info("Wiegand: failed 48oddAllParity\n");
		return false;
	}
	ret = verify48_odd_parity(wie);
	if (ret != wie->odd48_parity) {
		pr_info("Wiegand: failed 48oddParity\n");
		return false;
	}
	ret = verify48_even_parity(wie);
	if (ret != wie->even48_parity) {
		pr_info("Wiegand: failed 48evenParity\n");
		return false;
	}
	return true;
}

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
		wie->lock = 0;
		return HRTIMER_NORESTART;
	}

	/* Should never get here, but just in case stop the timer. */
	return HRTIMER_NORESTART;
}

/*Function called when /sys/kernel/wiegand/wie->length file is read */
static ssize_t length_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	struct wiegand_data *wie = g_wie;

	return sprintf(buf, "%d\n", wie->length);
}

/*Function called when /sys/kernel/wiegand/wie->length file is written to */
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
	if ((lengthsetting >= 26) && (lengthsetting <= 48))
		wie->length = lengthsetting;
	return lengthsetting;
}

static ssize_t lock_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	struct wiegand_data *wie = g_wie;

	return sprintf(buf, "%d\n", wie->lock);
}

static ssize_t hexval_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	struct wiegand_data *wie = g_wie;

	return sprintf(buf, "%llx\n", wie->this_scan);
}

/*
 * Only store when unlocked and either 5 seconds has passed or a new scan is
 * detected that is not a duplicate.
 */
static ssize_t hexval_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	struct wiegand_data *wie = g_wie;
	bool ret;
	unsigned long long int mask;
	struct timespec64 this_time;

	ktime_get_real_ts64(&this_time);
	if (wie->lock == 0) { /* Only take hex input when a scan has cleared */
		int err = kstrtoull(buf, 16, &wie->this_scan);

		if (err)
			return err;
		if ((wie->this_scan != wie->last_scan) ||
				(this_time.tv_sec > wie->last_time.tv_sec+5)) {
			/*Debounce inputs for 5 seconds */
			pr_info("Wiegand: Got hexval input: %llx\n",
					wie->this_scan);
			/*
			 * Handles 26/37 bit standard wiegand which works on
			 * the little rs232 converter unit I have
			 */
			if ((wie->length >= 26) && (wie->length <= 37)) {
				wie->last_scan = wie->this_scan;
				/*
				 * even parity bit is top bit
				 * odd parity bit is bottom bit
				 */
				mask = 1ULL << (wie->length - 1);
				wie->even_parity = wie->this_scan & mask;
				mask = 1ULL;
				wie->odd_parity = wie->this_scan & mask;
				mask = (1ULL << (wie->length - 1)) - 2;
				/*
				 * top bit - 2 = payload without top and bottom
				 * bits of parity
				 */
				wie->this_payload = (wie->this_scan & mask)
						>> 1;
				pr_info("Wiegand: Payload: %llx\n",
						wie->this_payload);
				ret = verify_parity(wie);
				if (!ret) {
					pr_info("Wiegand: Parity mismatch. Scan again.\n");
					return count;
				}
				pr_info("Wiegand: Parity match!\n");
				wie->cur_time = 0;
				wie->lock = 1;
				/* Enable the timer */
				wie->state = DRIVE;
				/*
				 * reset mask. wie->this_scan includes
				 *  parity bits
				 */
				wie->t_mask = 1ULL << (wie->length - 1);
				/*
				 * seconds,nanoseconds. Could be any
				 * short time here.
				 */
				wie->kt_period = ktime_set(0, 50000);
				hrtimer_start(&wie->htimer, wie->kt_period,
						HRTIMER_MODE_REL);
			}
			/*
			 * 48-bit Corporate 1000 Canem Wiegand Format
			 * A = Company ID Code
			 * B = Card ID Number
			 * P = Parity Bit
			 * E = Even Parity On X's
			 * O = Odd Parity On X's
			 * PP(22*A)(23*B)P
			 * .E(15*.XX).
			 * ..(15*XX.)O
			 * O(47*X)
			 */
			else if (wie->length == 48) {
				/* Canem 48bit Corporate 1000 format */
				wie->last_scan = wie->this_scan;
				/* Top bit is odd parity of all bits */
				mask = 1ULL << (wie->length-1); /* Top bit */
				wie->odd48_all_parity = wie->this_scan & mask;
				/* Second to top bit */
				mask = 1ULL << (wie->length-2);
				wie->even48_parity = wie->this_scan & mask;
				mask = 1ULL;
				wie->odd48_parity = wie->this_scan & mask;
				/*
				 * mask is 1's everywhere except the 3 parity
				 * bits - 00111...10
				 */
				mask = (1ULL << (wie->length-2)) - 2;
				wie->this_payload = (wie->this_scan & mask)
						>> 1;
				pr_info("Wiegand: Payload: %llx\n",
						wie->this_payload);
				ret = verify48_parity(wie);
				if (!ret) {
					pr_info("Wiegand: Parity mismatch. Scan again.\n");
					return count;
				}
				pr_info("Wiegand: Parity match!\n");
				wie->cur_time = 0;
				wie->lock = 1;
				/* Enable the timer */
				wie->state = DRIVE;
				wie->t_mask = 1ULL << (wie->length-1);
				wie->kt_period = ktime_set(0, 50000);
				hrtimer_start(&wie->htimer, wie->kt_period,
						HRTIMER_MODE_REL);
			}
		}
	}
	return count;
}

static struct kobj_attribute length_attr = __ATTR(length, 0660,
		length_show, length_store);
/* No lock_store function, it is controlled internally */
static struct kobj_attribute lock_attr = __ATTR(lock, 0660, lock_show, NULL);
static struct kobj_attribute hexval_attr = __ATTR(hexval, 0660,
		hexval_show, hexval_store);

static struct attribute *wiegand_attrs[] = {
	&length_attr.attr,		/* WiegandNN where 37 >= NN >= 26 */
	&lock_attr.attr,
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
	wie->length = 26;
	wie->last_scan = 1;
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
