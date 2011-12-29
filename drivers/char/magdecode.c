/***************************************************
* Simple Magstripe Reader and Decoder              *
* Author: Ryan Stewart (ryan@boundarydevices.com)  *
*                                                  *
* Driver for a Neuron MCR-370T-1R-xxxx card reader *
* ------------------------------------------------ *
* This reader accepts a single-track card with a   *
* 21-character BCD string encoded in the stripe.   *
*                                                  *
* When read, the driver will decode and return the *
* entire track of the card. Partial swipes, bad    *
* stripe data, or swipes which take too long will  *
* cause the driver to return an empty line. The    *
* driver returns 'F/f' or 'R/r' when the device's  *
* switches transition: F for front, R for rear,    *
* upper-case for open, lower-case for closed.      *
***************************************************/
/***********
* includes *
***********/

#include <asm/errno.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio-trig.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/magstripe.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <mach/gpio.h>

/**************
* definitions *
**************/

#define INSERT	1
#define REMOVE	0

#define DATASIZE 32
#define EVENTSIZE 32
#define MAXREAD 85	// most characters to return from a data string
#define DATAMASK (DATASIZE - 1)
#define DATABITMASK (DATASIZE * 8 - 1)
#define EVENTMASK (EVENTSIZE -1)

// Device structure, to keep track of everything device-specific here
struct mag_dev {
	struct list_head list_head;	// see global dev_list
	struct cdev cdev;
	struct platform_device *pdev;
	wait_queue_head_t queue;
	spinlock_t lock;
	struct timer_list timer;
	int open_count;

	// Contains references to config data, including gpio pin numbers
	// See arch/arm/mach_mx5/mx51_babbage.c
	struct mag_platform_data *pindef;

	// Event buffer - Records the order of switch open/close events and
	// data string completion events, so that we can distinguish between
	// insertion and removal.
	struct event {
		u8 type;
		u8 ptr; // for data events, holds end of data string
	} events[EVENTSIZE];
	u8 e_add;	// Buffer index at which data is to be added
	u8 e_take;	// Buffer index from which data is to be read

	// Data buffer - Records bits in the order that they are read from the
	// stripe; bits packed into unsigned chars
	u8 data[DATASIZE];
	u8 d_add; 	// Bit index at which data is to be added
	u8 d_take; 	// Bit index from which data is to be read
	u8 d_addprime; 	// remembers the starting index of data strings within
	// the data buffer, and helps us to keep from pushing meaningless data
	// events into the event buffer

	u8 dd; // Swipe direction (INSERT or REMOVE)
};


/***************
* declarations *
***************/

static int mag_major = 0;
static int mag_minor = 0;
module_param(mag_major, int, S_IRUGO);
module_param(mag_minor, int, S_IRUGO);

static dev_t devnum;

static char const my_name[] = {
	"magstripe"
};

// Track all devices associated with this driver
static struct list_head dev_list;

// Lookup table for translating from normal hex to BCD + parity bit
// Low 32 entries for reverse swipe, high 32 for forward swipe
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

/********************
* decoder functions *
********************/
// Pull an unaligned 5-bit BCD (+ parity bit) character out of the data buffer.
static char mag_charat(char *data, int index, char dd)
{
	// The idea is to treat data[] as a continuous ring of bits and
	// treat index as a true bit index into that ring, reading a 5-bit
	// character that starts at index rather than being forced to pull the
	// data out in 8-bit chunks.

	// We grab the byte at the bit index and the one above it
	unsigned char thischar, nextchar, result = 0x00;
	thischar = data[(index / 8) & DATAMASK];
	nextchar = data[(index / 8 + 1) & DATAMASK];

	// Shift current byte down so that bit at index is the new LSB
	thischar >>= (index & 0x7);
	// Shift next byte up so that its LSB is above the new character's MSB
	nextchar <<= 8 - (index & 0x7);

	// Combine and cut off high 3 bits. This is our 5-bit character.
	result = (thischar | nextchar) & 0x1F;

	// Translate using the LUT, and return
	return lut_bcdp[result | (dd << 5)];
}

// Find a valid data string in the data buffer, decode it, and place in buf
static int mag_decode(struct mag_dev *dev, char *buf)
{
	char *data = dev->data, dd = dev->dd;

	int scale, starti = -1, endi = -1, i = 0, sc = 0, bc, bi,
		bitcount = (dev->events[dev->e_take].ptr - dev->d_take) &
		DATABITMASK;

	if (dd == INSERT) {
		scale = 1;
		bi = dev->d_take;
	} else {
		scale = -1;
		bi = dev->d_take + bitcount - 5;
	}

	// Step through circular buffer looking for valid data
	for (bc = 0; bc < bitcount && i < MAXREAD; bi += scale,
		bc += (dd ? scale : -scale)) {

		char c = mag_charat(data, bi, dd);

		if (starti >= 0) {
			if (c == '?') {
				// end reached, so quit loop
				buf[i] = '\n';
				endi = bi;
				break;
			} else if (c == 'X') {
				// lookup table returns X on parity error
				// if that happens, forget everything and
				// start looking for sentinels again
				bi = starti;
				bc = sc;
				scale /= 5;
				starti = -1;
				i = 0;
				continue;
			} else {
				// else put this character into the buffer
				buf[i] = c;
				i++;
			}
		// If we haven't recorded start sentinel position yet,
		// check whether character at this bit is start sentinel
		} else if (c == ';') {
			// If so, start stepping 5 bits at a time
			scale *= 5;
			// Save bc and bi in case this is the wrong sentinel
			sc = bc;
			starti = bi;
		}
	}

	// Return an empty line on faulty output
	if (starti < 0 || endi < 0) {
		i = 0;
		buf[i] = '\n';
	}
	return i + 1;
}

static void check_pin(struct mag_dev *dev, int pin)
{
	u8 level, event;
	level = gpio_get_value(pin);

	// Determine which switch it was, and whether it was opened or closed
	if (pin == dev->pindef->front_pin) {
		event = 'F' | (level << 5);
	} else {
		event = 'R' | (level << 5);
	}

	if (dev->d_add != dev->d_addprime) {
	// switch events signal data completion, so flush data
		dev->events[dev->e_add].type = 'd';
		dev->events[dev->e_add].ptr = dev->d_add;
		dev->e_add = (dev->e_add + 1) & EVENTMASK;
		dev->d_addprime = dev->d_add;
	}

	// Add to event queue
	dev->events[dev->e_add].type = event;

	// advance e_add
	dev->e_add = (dev->e_add + 1) & EVENTMASK;
	wake_up(&dev->queue);
}

/**************************************
* interrupt handler / timer functions *
**************************************/
// Perform on switch flip
static irqreturn_t mag_switch_handler(int irq, void *dev_id)
{
	struct mag_dev *dev = dev_id;

	check_pin(dev,irq_to_gpio(irq));
	return IRQ_HANDLED;
}

// perform on clock pulse
static irqreturn_t mag_clock_handler(int irq, void *dev_id)
{
	u8 level;
	struct mag_dev *dev = dev_id;

	// get level on data pin
	level = gpio_get_value(dev->pindef->data_pin);

	// save data value to buffer
	if (!level)
		dev->data[dev->d_add / 8] |= 1 << (dev->d_add & 0x07);
	else
		dev->data[dev->d_add / 8] &= ~(1 << (dev->d_add & 0x07));

	// advance d_add
	dev->d_add = (dev->d_add + 1) & DATABITMASK;

	// (re)start timer
	mod_timer(&dev->timer, jiffies + (HZ / 10) * dev->pindef->timeout);

	return IRQ_HANDLED;
}

// Timer function: if timer expired, then flush data to event queue
static void mag_timer(unsigned long arg)
{
	unsigned long flags;
	struct mag_dev *dev = (struct mag_dev *) arg;

	spin_lock_irqsave(&dev->lock, flags);

	if (dev->d_add != dev->d_addprime) {
		dev->events[dev->e_add].type = 'd';
		dev->events[dev->e_add].ptr = dev->d_add;
		dev->e_add = (dev->e_add + 1) & EVENTMASK;
		dev->d_addprime = dev->d_add;
		wake_up(&dev->queue);
	}

	spin_unlock_irqrestore(&dev->lock, flags);
}

/*****************
* fops functions *
*****************/

static ssize_t mag_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	ssize_t result;
	struct mag_dev *dev = (struct mag_dev *) file->private_data;

	if (dev == NULL) return -EINVAL;

	if ((dev->e_add == dev->e_take) && !(file->f_flags & O_NONBLOCK))
		wait_event_interruptible(dev->queue,
			(dev->e_add != dev->e_take));
	if (dev->e_add != dev->e_take) {
		// read!
		u8 event = dev->events[dev->e_take].type;

		if (event == 'd') {
			char temp[MAXREAD];
			result = mag_decode(dev, temp);
			dev->d_take = dev->events[dev->e_take].ptr;
			if (result)
				if (copy_to_user(buf, temp, result))
					return -EFAULT;
		} else { // switch closure
			// determine direction of card swipe
			char temp[2] = { event, '\n' };

			// Lower case f = front switch closed
			if (event == 'f') dev->dd = INSERT;
			// Upper case R = rear switch open
			else if (event == 'R') dev->dd = REMOVE;

			result = 2;
			if (copy_to_user(buf, temp, result)) return -EFAULT;
		}
		dev->e_take = (dev->e_take + 1) & EVENTMASK;

	} else return -EINTR;
	return result;
}

static unsigned int mag_poll(struct file *file, struct poll_table_struct *table)
{
	struct mag_dev *dev = (struct mag_dev *) file->private_data;
	if (dev == NULL) return -EINVAL;

	poll_wait(file, &dev->queue, table);

	if (dev->e_add != dev->e_take) return POLLIN | POLLRDNORM;
	else return 0;
}

static int mag_open(struct inode *inode, struct file *file)
{
	int result;
	struct mag_dev *dev;

	dev = container_of(inode->i_cdev, struct mag_dev, cdev);
	file->private_data = dev;

	// Set up on first open
	if (!dev->open_count) {
		// init wait queue and spinlock
		init_waitqueue_head(&dev->queue);
		spin_lock_init(&dev->lock);
		init_timer(&dev->timer);
		dev->timer.function = mag_timer;
		dev->timer.data = (unsigned long) dev;

		// IRQ setup and request
		set_irq_type(gpio_to_irq(dev->pindef->front_pin),
			IRQ_TYPE_EDGE_BOTH);
		set_irq_type(gpio_to_irq(dev->pindef->rear_pin),
			IRQ_TYPE_EDGE_BOTH);
		set_irq_type(gpio_to_irq(dev->pindef->clock_pin),
			dev->pindef->edge ?
			IRQ_TYPE_EDGE_RISING : IRQ_TYPE_EDGE_FALLING);
		check_pin(dev,dev->pindef->rear_pin);
		check_pin(dev,dev->pindef->front_pin);
		if ((result = request_irq(gpio_to_irq(dev->pindef->front_pin),
			mag_switch_handler, 0, my_name, file->private_data)))
			goto fail_rirq1;
		if ((result = request_irq(gpio_to_irq(dev->pindef->rear_pin),
			mag_switch_handler, 0, my_name, file->private_data)))
			goto fail_rirq2;
		if ((result = request_irq(gpio_to_irq(dev->pindef->clock_pin),
			mag_clock_handler, 0, my_name, file->private_data)))
			goto fail_rirq3;
	}

	dev->open_count++;
	return 0;

fail_rirq3:
	free_irq(gpio_to_irq(dev->pindef->rear_pin), file->private_data);
fail_rirq2:
	free_irq(gpio_to_irq(dev->pindef->front_pin), file->private_data);
fail_rirq1:
	printk("%s: Couldn't get IRQs\n", my_name);
	return result;
}

static int mag_release(struct inode *inode, struct file *file)
{
	struct mag_dev *dev = file->private_data;

	dev->open_count--;

	// Clean up on last close
	if (!dev->open_count) {
		// IRQ release
		free_irq(gpio_to_irq(dev->pindef->front_pin), dev);
		free_irq(gpio_to_irq(dev->pindef->rear_pin), dev);
		free_irq(gpio_to_irq(dev->pindef->clock_pin), dev);
	}

	return 0;
}

static struct file_operations mag_fops = {
	.owner = THIS_MODULE,
	.read = mag_read,
	.poll = mag_poll,
	.open = mag_open,
	.release = mag_release,
};

/*****************************
* driver framework functions *
*****************************/

static int mag_probe(struct platform_device *pdev)
{
	int result = 0;
	struct mag_dev *dev;

	// Create device structure
	if ((dev = kzalloc(sizeof(struct mag_dev), GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "%s: No memory left!", my_name);
		result = -ENOMEM;
		goto fail_entry;
	}

	// Associate pdev and initialize cdev
	dev->pdev = pdev;
	cdev_init(&dev->cdev, &mag_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &mag_fops;

	dev->pindef = (struct mag_platform_data *) pdev->dev.platform_data;

	// Register cdev
	if ((result = cdev_add(&dev->cdev, devnum, 1) ) < 0) {
		printk(KERN_ERR "%s: couldn't add device: err %d\n", my_name,
			result);
		goto fail_cdevadd;
	}

	// Add to the list
	INIT_LIST_HEAD(&dev->list_head);
	list_add(&dev->list_head, &dev_list);
	return 0;

fail_cdevadd:
	kfree(dev);
fail_entry:
	return result;
}

static int mag_remove(struct platform_device *pdev)
{
	static struct mag_dev *dev, *next;

	// find node containing this pdev, kill associated cdev and clear node
	list_for_each_entry_safe(dev, next, &dev_list, list_head) {
		if (pdev == dev->pdev) {
			cdev_del(&dev->cdev);
			list_del(&dev->list_head);
			kfree(dev);
			break;
		}
	}

	return 0;
}

static struct platform_driver mag_driver = {
	.driver = {
		.name = my_name,
		},
	.probe = mag_probe,
	.remove = mag_remove,
	.suspend = NULL,
	.resume = NULL,
};

/*****************************
* init and cleanup functions *
*****************************/

int mag_init(void)
{
	int result;

	// Setup dev #
	if (mag_major) {
		devnum = MKDEV(mag_major, mag_minor);
		result = register_chrdev_region(devnum, 1, my_name);
	} else {
		result = alloc_chrdev_region(&devnum, mag_minor, 1, my_name);
		mag_major = MAJOR(devnum);
	}

	if (result < 0) {
		printk(KERN_ERR "%s: couldn't get major %d: err %d\n", my_name,
			mag_major, result);
		goto fail_devnum;
	}

	// Create list head
	INIT_LIST_HEAD(&dev_list);

	// Register driver itself
	if ((result = platform_driver_register(&mag_driver)) < 0) {
		printk(KERN_ERR "%s: couldn't register driver, err %d\n",
			my_name, result);
		goto fail_pdev;
	}

	return 0;

fail_pdev:
	unregister_chrdev_region(devnum, 1);
fail_devnum:
	return result;
}

void mag_cleanup(void)
{
	struct list_head *ptr, *safe;

	platform_driver_unregister(&mag_driver);
	list_for_each_safe(ptr, safe, &dev_list) {
		list_del(ptr);
		kfree(container_of(ptr, struct mag_dev, list_head));
	}
	unregister_chrdev_region(devnum, 1);
}

module_init(mag_init);
module_exit(mag_cleanup);

/* module bookkeeping */
MODULE_AUTHOR("Ryan Stewart (ryan@boundarydevices.com)");
MODULE_LICENSE("GPL");
