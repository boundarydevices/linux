/*
 * Simple Magstripe Reader and Decoder
 *
 * Driver for a Neuron MCR-370T-1R-xxxx card reader
 * ------------------------------------------------
 * This reader accepts a single-track card with a
 * 21-character BCD string encoded in the stripe.
 *
 * When read, the driver will decode and return the
 * entire track of the card. Partial swipes, bad
 * stripe data, or swipes which take too long will
 * cause the driver to return an empty line. The
 * driver returns 'F/f' or 'R/r' when the device's
 * switches transition: F for front, R for rear,
 * upper-case for open, lower-case for closed.
 *
 * See this blog post for an old description:
 *     http://boundarydevices.com/magnetic-stripe-driver-for-linux-gpio/
 *
 * Author: Ryan Stewart (ryan@boundarydevices.com)
 *
 * Copyright (C) 2011-2015, Boundary Devices
 */
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#define INSERT	1
#define REMOVE	0

#define DATASIZE 32
#define EVENTSIZE 32
#define MAXREAD 85
#define DATAMASK (DATASIZE - 1)
#define DATABITMASK (DATASIZE * 8 - 1)
#define EVENTMASK (EVENTSIZE - 1)
#define BITS_PER_CHAR 5

static int mag_major;
static dev_t devnum;
static struct class *magdecode_class;

#define DRIVER_NAME "magdecode"

struct event {
	u8 type;
	u8 ptr; /* for data events, holds end of data string */
};

/*
 * Device structure, to keep track of everything device-specific here
 */
struct mag_dev {
	struct cdev cdev;
	struct device *chrdev;
	struct platform_device *pdev;
	wait_queue_head_t queue;
	spinlock_t lock; /* only one IRQ at a time */
	struct timer_list timer;
	int open_count;

	/*
	 * gpio pin numbers and flags
	 */
	int front_pin;
	int front_pin_close_high;
	int rear_pin;
	int rear_pin_close_high;
	int clock_pin;
	int clock_pin_active_high;
	int data_pin;
	int data_pin_active_high;
	char edge;
	char last_front;
	char last_rear;
	int timeout;

	/*
	 * Event buffer - Records the order of switch open/close events and
	 * data string completion events, so that we can distinguish between
	 * insertion and removal.
	 */
	struct event events[EVENTSIZE];
	u8 e_add;	/* Buffer index at which data is to be added */
	u8 e_take;	/* Buffer index from which data is to be read */

	/*
	 * Data buffer - Records bits in the order that they are read from the
	 * stripe; bits packed into unsigned chars
	 */
	u8 data[DATASIZE];
	u8 d_add;	/* Bit index at which data is to be added */
	u8 d_take;	/* Bit index from which data is to be read */
	u8 d_addprime;	/* Bit index of data strings */

	/*
	 * the data buffer, and helps us to keep from pushing meaningless data
	 * events into the event buffer
	 */
	u8 dd; /* Swipe direction (INSERT or REMOVE) */

	/*
	 * Used for sysfs entries
	 */
	u8 take_start;
	u8 take_end;
	u8 take_dd;
};

static void flush_data(struct mag_dev *dev)
{
	if (dev->d_add != dev->d_addprime) {
		int bitcount = (dev->d_add - dev->d_take) & DATABITMASK;
		if (bitcount >= 2*BITS_PER_CHAR) {
			dev->events[dev->e_add].type = 'd';
			dev->events[dev->e_add].ptr = dev->d_add;
			dev->e_add = (dev->e_add + 1) & EVENTMASK;
			wake_up(&dev->queue);
		}
		dev->d_addprime = dev->d_add;
	}
}

static void mag_timer(unsigned long arg)
{
	unsigned long flags;
	struct mag_dev *dev = (struct mag_dev *)arg;

	spin_lock_irqsave(&dev->lock, flags);

	flush_data(dev);

	spin_unlock_irqrestore(&dev->lock, flags);
}

static void check_pin(struct mag_dev *dev,
		      int pin, int active_high)
{
	u8 level, event;
	level = (0 != gpio_get_value(pin)) ^ active_high;

	/* Determine which switch it was, and whether it was opened or closed */
	if (pin == dev->front_pin)
		dev->last_front = event = 'F' | (level << 5);
	else
		dev->last_rear = event = 'R' | (level << 5);

	flush_data(dev);

	/* Add to event queue */
	dev->events[dev->e_add].type = event;

	/* advance e_add */
	dev->e_add = (dev->e_add + 1) & EVENTMASK;
	wake_up(&dev->queue);
}

static irqreturn_t mag_switch_handler(int irq, void *dev_id,
				      int pin, int active_high)
{
	struct mag_dev *dev = dev_id;

	check_pin(dev, pin, active_high);
	return IRQ_HANDLED;
}

static irqreturn_t front_switch_handler(int irq, void *dev_id)
{
	struct mag_dev *dev = dev_id;
	return mag_switch_handler(irq, dev_id,
				  dev->front_pin,
				  dev->front_pin_close_high);
}

static irqreturn_t rear_switch_handler(int irq, void *dev_id)
{
	struct mag_dev *dev = dev_id;
	return mag_switch_handler(irq, dev_id, dev->rear_pin,
				  dev->rear_pin_close_high);
}

static irqreturn_t mag_clock_handler(int irq, void *dev_id)
{
	struct mag_dev *dev = dev_id;
	int level = (0 != gpio_get_value(dev->data_pin));

	/* save data value to buffer */
	if (level ^ dev->data_pin_active_high)
		dev->data[dev->d_add / 8] |= 1 << (dev->d_add & 0x07);
	else
		dev->data[dev->d_add / 8] &= ~(1 << (dev->d_add & 0x07));

	/* advance d_add */
	dev->d_add = (dev->d_add + 1) & DATABITMASK;

	/* (re)start timer */
	mod_timer(&dev->timer, jiffies + (HZ / 10) * dev->timeout);

	return IRQ_HANDLED;
}

static int mag_open(struct inode *inode, struct file *file)
{
	int result;
	struct mag_dev *dev;

	dev = container_of(inode->i_cdev, struct mag_dev, cdev);
	file->private_data = dev;

	/* Set up on first open */
	if (!dev->open_count) {
		/* init wait queue and spinlock */
		init_waitqueue_head(&dev->queue);
		spin_lock_init(&dev->lock);
		init_timer(&dev->timer);
		dev->timer.function = mag_timer;
		dev->timer.data = (unsigned long) dev;

		check_pin(dev, dev->rear_pin, dev->rear_pin_close_high);
		check_pin(dev, dev->front_pin, dev->front_pin_close_high);
		result = devm_request_irq(&dev->pdev->dev,
					  gpio_to_irq(dev->front_pin),
					  front_switch_handler,
					  IRQ_TYPE_EDGE_BOTH,
					  "magfront",
					  file->private_data);
		if (result)
			goto fail_rirq1;

		result = devm_request_irq(&dev->pdev->dev,
					  gpio_to_irq(dev->rear_pin),
					  rear_switch_handler,
					  IRQ_TYPE_EDGE_BOTH,
					  "magrear",
					  file->private_data);
		if (result)
			goto fail_rirq2;

		result = devm_request_irq(&dev->pdev->dev,
					  gpio_to_irq(dev->clock_pin),
					  mag_clock_handler,
					  dev->clock_pin_active_high
					  ? IRQ_TYPE_EDGE_RISING
					  : IRQ_TYPE_EDGE_FALLING,
					  "magclock",
					  file->private_data);
		if (result)
			goto fail_rirq3;
	}

	dev->open_count++;
	return 0;

fail_rirq3:
	devm_free_irq(&dev->pdev->dev,
		      gpio_to_irq(dev->rear_pin), file->private_data);
fail_rirq2:
	devm_free_irq(&dev->pdev->dev,
		      gpio_to_irq(dev->front_pin), file->private_data);
fail_rirq1:
	return result;
}

static int mag_release(struct inode *inode, struct file *file)
{
	struct mag_dev *dev = file->private_data;

	dev->open_count--;

	/* Clean up on last close */
	if (!dev->open_count) {
		/* IRQ release */
		devm_free_irq(&dev->pdev->dev,
			      gpio_to_irq(dev->front_pin), dev);
		devm_free_irq(&dev->pdev->dev,
			      gpio_to_irq(dev->rear_pin), dev);
		devm_free_irq(&dev->pdev->dev,
			      gpio_to_irq(dev->clock_pin), dev);
	}

	return 0;
}

static const char lut_bcdp[64] = {
	'X', '0', '8', 'X', '4', 'X', 'X', '<',
	'2', 'X', 'X', ':', 'X', '6', '>', 'X',
	'1', 'X', 'X', '9', 'X', '5', '=', 'X',
	'X', '3', ';', 'X', '7', 'X', 'X', '?',
	'X', '1', '2', 'X', '4', 'X', 'X', '7',
	'8', 'X', 'X', ';', 'X', '=', '>', 'X',
	'0', 'X', 'X', '3', 'X', '5', '6', 'X',
	'X', '9', ':', 'X', '<', 'X', 'X', '?',
};

/*
 * Translate an unaligned 5-bit BCD character
 * (4 bits + parity)
 */
static char mag_charat(char *data, int index, char dd)
{
	/*
	 * The idea is to treat data[] as a continuous ring of bits and
	 * treat index as a true bit index into that ring, reading a 5-bit
	 * character that starts at index rather than being forced to pull the
	 * data out in 8-bit chunks.
	 */

	/* We grab the byte at the bit index and the one above it */
	unsigned char thischar, nextchar, result = 0x00;
	thischar = data[(index / 8) & DATAMASK];
	nextchar = data[(index / 8 + 1) & DATAMASK];

	/* first bit in LSB */
	thischar >>= (index & 0x7);
	/* above first byte's MSB */
	nextchar <<= 8 - (index & 0x7);

	/* Combine and cut off high 3 bits. This is our 5-bit character. */
	result = (thischar | nextchar) & 0x1F;

	/* Translate using the LUT, and return */
	return lut_bcdp[result | (dd << 5)];
}

static int mag_decode(struct mag_dev *dev, char *buf)
{
	char *data = dev->data;
	char dd = dev->dd;
	int bitcount, scale, bi, starti, endi, i, sc, bc;

	dev->take_end = dev->events[dev->e_take].ptr;
	dev->take_start = dev->d_take;
	dev->take_dd = dd;

	bitcount = (dev->take_end - dev->take_start) & DATABITMASK;
	if (dd == INSERT) {
		scale = 1;
		bi = dev->d_take;
	} else {
		scale = -1;
		bi = dev->d_take + bitcount - 5;
	}
	starti = -1;
	endi = -1;
	i = 0;
	sc = 0;

	/* Step through circular buffer looking for valid data */
	for (bc = 0; bc < bitcount && i < MAXREAD; bi += scale,
		bc += (dd ? scale : -scale)) {
		char c = mag_charat(data, bi, dd);
		if (starti >= 0) {
			if (c == '?') {
				/* end reached, so quit loop */
				buf[i] = '\n';
				endi = bi;
				break;
			} else if (c == 'X') {
				/*
				 * lookup table returns X on parity error
				 * if that happens, forget everything and
				 * start looking for sentinels again
				 */
				bi = starti;
				bc = sc;
				scale /= 5;
				starti = -1;
				i = 0;
				continue;
			} else {
				/* else put this character into the buffer */
				buf[i] = c;
				i++;
			}
		/*
		 * If we haven't recorded start sentinel position yet,
		 * check whether character at this bit is start sentinel
		 */
		} else if (c == ';') {
			/* If so, start stepping 5 bits at a time */
			scale *= 5;
			/* Save bc and bi in case this is the wrong sentinel */
			sc = bc;
			starti = bi;
		}
	}

	/* Return an empty line on faulty output */
	if (starti < 0 || endi < 0) {
		dev_dbg(dev->chrdev, "decode err(%d) dir(%d) at [%d..%d]\n",
			i, dev->take_dd, dev->take_start, dev->take_end);
		dev_dbg(dev->chrdev, "starti %d, endi %d\n", starti, endi);
		i = 0;
		buf[i] = '\n';
	} else {
		dev_dbg(dev->chrdev, "decode(%d) dir(%d) at [%d..%d]\n",
			i, dev->take_dd, dev->take_start, dev->take_end);
	}
	return i + 1;
}

static ssize_t mag_read
	(struct file *file, char __user *buf,
	 size_t count, loff_t *ppos)
{
	ssize_t result;
	struct mag_dev *dev = (struct mag_dev *)file->private_data;

	if (dev == NULL)
		return -EINVAL;

	if ((dev->e_add == dev->e_take) && !(file->f_flags & O_NONBLOCK))
		wait_event_interruptible(dev->queue,
					 (dev->e_add != dev->e_take));
	if (dev->e_add != dev->e_take) {
		/* read! */
		u8 event = dev->events[dev->e_take].type;

		if (event == 'd') {
			char temp[MAXREAD];
			result = mag_decode(dev, temp);
			dev->d_take = dev->events[dev->e_take].ptr;
			if (result) {
				if (copy_to_user(buf, temp, result))
					return -EFAULT;
			}
		} else { /* switch closure */
			/* determine direction of card swipe */
			char temp[2] = { event, '\n' };

			/* Lower case f = front switch closed */
			if (event == 'f')
				dev->dd = INSERT;
			/* Upper case R = rear switch open */
			else if (event == 'R')
				dev->dd = REMOVE;

			result = 2;
			if (copy_to_user(buf, temp, result))
				return -EFAULT;
		}
		dev->e_take = (dev->e_take + 1) & EVENTMASK;

		return result;
	} else {
		return -EINTR;
	}
}

static unsigned int mag_poll(struct file *file, struct poll_table_struct *table)
{
	struct mag_dev *dev = (struct mag_dev *)file->private_data;
	if (!dev)
		return -EINVAL;

	poll_wait(file, &dev->queue, table);

	if (dev->e_add != dev->e_take)
		return POLLIN | POLLRDNORM;
	else
		return 0;
}

static struct file_operations const mag_fops = {
	.owner = THIS_MODULE,
	.read = mag_read,
	.poll = mag_poll,
	.open = mag_open,
	.release = mag_release,
};

struct gpio_def {
	char const *name;
	int	*gpio_pin;
	int	*active_high;
};

static ssize_t show_front(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mag_dev *mag = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",
		       (0 != gpio_get_value(mag->front_pin))
			^ mag->front_pin_close_high);
}

static struct kobj_attribute front =
__ATTR(front, 0644, (void *)show_front, NULL);

static ssize_t show_rear(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mag_dev *mag = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",
		       (0 != gpio_get_value(mag->rear_pin))
			^ mag->rear_pin_close_high);
}

static struct kobj_attribute rear =
__ATTR(rear, 0644, (void *)show_rear, NULL);

static ssize_t show_last_front(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mag_dev *mag = dev_get_drvdata(dev);
	return sprintf(buf, "%c\n", mag->last_front);
}

static struct kobj_attribute last_front =
__ATTR(last_front, 0644, (void *)show_last_front, NULL);

static ssize_t show_last_rear(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mag_dev *mag = dev_get_drvdata(dev);
	return sprintf(buf, "%c\n", mag->last_rear);
}

static struct kobj_attribute last_rear =
__ATTR(last_rear, 0644, (void *)show_last_rear, NULL);

static ssize_t show_raw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mag_dev *mag = dev_get_drvdata(dev);
	int const max = PAGE_SIZE - 20;
	int i = 0;
	int count;
	u8 next = mag->take_start;
	buf += sprintf(buf, "%d:%d-%d:", mag->take_dd, mag->take_start, mag->take_end);
	count = (mag->take_end - next + DATASIZE) & DATABITMASK;
	if (count > max)
		count = max;

	for (i=0; i < count; i++) {
		char bit = (0 != (mag->data[next/8] & (1 << (next&7))));
		*buf++ = '0' + bit;
		next = (next + 1) & DATABITMASK;
	}
	*buf = '\n';
	return i+1;
}

static struct kobj_attribute raw =
__ATTR(raw, 0644, (void *)show_raw, NULL);

static struct attribute *mag_attrs[] = {
	&front.attr,
	&rear.attr,
	&last_front.attr,
	&last_rear.attr,
	&raw.attr,
	NULL,
};

static struct attribute_group mag_attr_grp = {
	.attrs = mag_attrs,
};

static int mag_of_probe(struct platform_device *pdev,
			struct device_node *np,
			struct mag_dev *dev)
{
	int i;
	struct gpio_def pins[] = {
		{ "front_pin",
		  &dev->front_pin,
		  &dev->front_pin_close_high }
	,	{ "rear_pin",
		  &dev->rear_pin,
		  &dev->rear_pin_close_high}
	,	{ "clock_pin",
		  &dev->clock_pin,
		  &dev->clock_pin_active_high}
	,	{ "data_pin",
		  &dev->data_pin,
		  &dev->data_pin_active_high}
	};
	for (i = 0; i < ARRAY_SIZE(pins); i++)
		*pins[i].gpio_pin = -1;

	for (i = 0; i < ARRAY_SIZE(pins); i++) {
		int rv;
		enum of_gpio_flags gpio_flags;
		int pin = of_get_named_gpio_flags(np, pins[i].name,
						  0, &gpio_flags);
		if (!gpio_is_valid(pin)) {
			dev_err(&pdev->dev, "Invalid %s\n", pins[i].name);
			break;
		}
		*pins[i].gpio_pin = pin;
		*pins[i].active_high = !(gpio_flags & OF_GPIO_ACTIVE_LOW);
		rv = devm_gpio_request(&pdev->dev, pin, pins[i].name);
		if (rv) {
			dev_err(&pdev->dev, "Error %d requesting pin %d:%s\n",
				rv, pin, pins[i].name);
			break;
		}
	}
	if (i < ARRAY_SIZE(pins)) {
		while (0 <= i) {
			devm_gpio_free(&pdev->dev, *pins[i].gpio_pin);
			*pins[i].gpio_pin = -1;
			i--;
		}
	}

	return (i == ARRAY_SIZE(pins)) ? 0 : -EINVAL;
}

static int mag_probe(struct platform_device *pdev)
{
	int result = 0;
	struct mag_dev *dev;
	struct device_node *np = pdev->dev.of_node;

	if (!np)
		return -ENODEV;

	dev = devm_kzalloc(&pdev->dev, sizeof(struct mag_dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "%s: alloc failure", DRIVER_NAME);
		result = -ENOMEM;
		goto fail_entry;
	}

	dev->pdev = pdev;
	dev->timeout = 10;
	cdev_init(&dev->cdev, &mag_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &mag_fops;

	if (0 != mag_of_probe(pdev, np, dev)) {
		dev_err(&pdev->dev, "Invalid dt spec\n");
		result = -ENODEV;
		goto fail_cdevadd;
	}

	result = cdev_add(&dev->cdev, devnum, 1);
	if (result < 0) {
		dev_err(&pdev->dev, "%s: couldn't add device: err %d\n",
			DRIVER_NAME, result);
		goto fail_cdevadd;
	}

	dev->chrdev = device_create(magdecode_class, &platform_bus,
				    devnum, 0, "%s", DRIVER_NAME);
	platform_set_drvdata(pdev, dev);

	result = sysfs_create_group(&pdev->dev.kobj, &mag_attr_grp);
	if (result)
		dev_err(&pdev->dev, "failed to create sysfs entries");

	return 0;

fail_cdevadd:
	devm_kfree(&pdev->dev, dev);
fail_entry:
	return result;
}

static int mag_remove(struct platform_device *pdev)
{
	struct mag_dev *dev = platform_get_drvdata(pdev);
	if (dev) {
		int i;
		int pins[] = {
			dev->front_pin,
			dev->rear_pin,
			dev->clock_pin,
			dev->data_pin,
		};

		sysfs_remove_group(&pdev->dev.kobj, &mag_attr_grp);
		if (!IS_ERR(dev->chrdev)) {
			device_destroy(magdecode_class, devnum);
			dev->chrdev = 0;
		}
		for (i = 0; i < ARRAY_SIZE(pins); i++)
			if (gpio_is_valid(pins[i]))
				devm_gpio_free(&pdev->dev, pins[i]);
	}
	return 0;
}

static const struct of_device_id magdecode_ids[] = {
	{ .compatible = "boundary,magdecode", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, magdecode_ids);

static struct platform_driver mag_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = magdecode_ids,
		},
	.probe = mag_probe,
	.remove = mag_remove,
};

static char *magdecode_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = S_IRUGO | S_IWUSR;
	return kasprintf(GFP_KERNEL, "%s", DRIVER_NAME);
}

int __init magdecode_init(void)
{
	int result;

	/* Create a sysfs class. */
	magdecode_class = class_create(THIS_MODULE, "magdecode");
	if (IS_ERR(magdecode_class)) {
		result = PTR_ERR(magdecode_class);
		goto fail_class;
	}
	magdecode_class->devnode = magdecode_devnode;

	result = alloc_chrdev_region(&devnum, 0, 1, DRIVER_NAME);
	if (result < 0) {
		pr_err("%s: couldn't chrdevs: err %d\n",
		       DRIVER_NAME, result);
		goto fail_chrdev;
	}
	mag_major = MAJOR(devnum);

	result = platform_driver_register(&mag_driver);
	if (result < 0) {
		pr_err("%s: couldn't register driver, err %d\n",
		       DRIVER_NAME, result);
		goto fail_platform;
	}

	pr_info("driver %s registered\n", DRIVER_NAME);

	return 0;

fail_platform:
	unregister_chrdev_region(devnum, 1);

fail_chrdev:
	class_destroy(magdecode_class);
	magdecode_class = 0;

fail_class:
	return result;
}

void __exit magdecode_cleanup(void)
{
	platform_driver_unregister(&mag_driver);

	unregister_chrdev_region(devnum, 1);

	if (!IS_ERR(magdecode_class))
		class_destroy(magdecode_class);
	pr_info("%s unloaded\n", DRIVER_NAME);
}

module_init(magdecode_init);
module_exit(magdecode_cleanup);
MODULE_LICENSE("GPL");
