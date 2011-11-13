/*
 * Generic GPIO output driver
 *
 * This driver allows platform GPIO output pins to be presented to
 * userspace as character devices, such that writing a '0' to
 * the device drives the pin low, and writing a '1' to the port
 * drives the pin high.
 *
 * The 'gpio' module parameter defines the pins to use. It's a
 * string of the form:
 *
 *	gpo=111:pin111[:initval=0],112:pin112[initval=1]
 *
 * where
 *	111 and 112 are the pin numbers (GPIO numbers),
 *	pinname is the device name
 *	the optional :initval specifies the initial setting
 *
 * Copyright (C) 2011 Boundary Devices, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */
#include <linux/kobject.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/fcntl.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>

static int gpio_output_major = 0;
module_param(gpio_output_major, int, S_IRUGO);

static int pin_count=0 ;

static char gpio_output_spec[256] = {CONFIG_GPIO_OUTPUT_DEFAULT};
module_param_string(spec, gpio_output_spec, sizeof(gpio_output_spec), 0644);
MODULE_PARM_DESC(spec, "Specification for gpio output params");

static struct class *gpio_class = 0;

static struct list_head pin_list ;

#define MAXNAMELEN 32

struct gpio_output_pin_t {
	struct list_head 	list_head;	// see global dev_list
	unsigned 		gpio ;
	unsigned 		initlevel ;
	char	 		name[MAXNAMELEN];
	struct cdev 		cdev;
};

static char const name[] = {
	"gpio_output"
};

static int gpio_output_open(struct inode *inode, struct file *file)
{
	return 0 ;
}

static int gpio_output_release(struct inode *inode, struct file *file)
{
	return 0 ;
}

ssize_t gpio_output_read(struct file *file, char __user *data, size_t len, loff_t *off)
{
	ssize_t written = 0 ;
	if ((0 < len) && (0 == *off)) {
		unsigned gpio = MINOR (file->f_dentry->d_inode->i_rdev);
		char value = gpio_get_value(gpio)+'0';
		if (0 == put_user(value,data)) {
			written=sizeof(value);
			*off += written ;
			printk (KERN_ERR "%s: gp[%u] == %c\n", name,gpio,value);
		} else
			written = -EACCES ;
	}
	return written ;
}

ssize_t gpio_output_write(struct file *file, const char __user *data, size_t len, loff_t *off)
{
	unsigned gpio = MINOR (file->f_dentry->d_inode->i_rdev);
	unsigned char val ;
	if (0 == copy_from_user(&val,data,sizeof(val))) {
		val &= 1 ;
		printk (KERN_ERR "%s: set gp[%u] to %d (now %d)\n", name, gpio, val, gpio_get_value(gpio));
		gpio_set_value(gpio,val);
		printk (KERN_ERR "%s: gp[%u] now %d\n", name, gpio, gpio_get_value(gpio));
		return len ;
	}
	else
		return -EACCES ;
}

static struct file_operations gpio_output_fops = {
	.owner = THIS_MODULE,
	.open = gpio_output_open,
	.release = gpio_output_release,
	.read = gpio_output_read,
	.write = gpio_output_write,
};

static int read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct gpio_output_pin_t *pin, *next;
	int total = 0 ;
	// find node containing this pdev, kill associated cdev and clear node
	list_for_each_entry_safe(pin, next, &pin_list, list_head) {
		int len = snprintf(page,count,"%u.%s.%u\n",pin->gpio,pin->name,pin->initlevel);
		page += len ;
		count -= len ;
		total += len ;
	}
	return total ;
}

/*****************************
* driver framework functions *
*****************************/

static int gpio_output_probe(struct platform_device *pdev)
{
	int result = 0;
	char const *next=gpio_output_spec;
	char const *end=next+strlen(gpio_output_spec);
	dev_t devnum ;
	if (gpio_output_major) {
		devnum = MKDEV(gpio_output_major, 0);
		result = register_chrdev_region(devnum, ARCH_NR_GPIOS, name);
	} else {
		devnum = 0;
		result = alloc_chrdev_region(&devnum, 0, ARCH_NR_GPIOS, name);
		gpio_output_major = MAJOR(devnum);
	}

	if (result < 0) {
		printk(KERN_ERR "%s: couldn't get major %d: err %d\n", name,
			gpio_output_major, result);
		return result ;
	}
	while (next && (next<end)) {
		char entry[sizeof(gpio_output_spec)];
		char const *comma=strchr(next,',');
		char *entryend ;
		char *colon ;
		unsigned len ;
		if (0 == comma)
			comma=end;
		len=comma-next ;
		memcpy(entry,next,len);
		entry[len]=0;
		entryend=entry+len ;
		colon=strchr(entry,':');
		if (colon && (colon<entryend) && (len<MAXNAMELEN)) {
			unsigned initlevel=0;
			char *pinname ;
			unsigned gpnum;
			*colon=0;
			gpnum=simple_strtoul(entry,0,0);
			pinname=colon+1 ;
			colon=strchr(pinname,':');
			if (colon && (colon <entryend)) {
				*colon=0;
				initlevel=simple_strtoul(colon+1,0,0);
			}
			if (gpio_is_valid(gpnum) && (1 >= initlevel)) {
				struct gpio_output_pin_t *pin = kzalloc(sizeof(*pin), GFP_KERNEL);
				if (pin) {
					int rv ;
					pin->gpio=gpnum ;
					strcpy(pin->name,pinname);
					pin->initlevel=initlevel ;
					if (0 == (rv=gpio_request(gpnum,pin->name))) {
						rv = gpio_direction_output(gpnum,initlevel);
						if (0 == rv) {
							INIT_LIST_HEAD(&pin->list_head);
							list_add_tail(&pin->list_head, &pin_list);
							++pin_count ;
						} else
							printk (KERN_ERR "%s: error %d setting pin %s as output\n",
								name,rv,pinname);
					}
					else {
						printk (KERN_ERR "%s: error %d allocating %s\n", name,rv, pinname);
						kfree(pin);
					}
				}
			}
			else
				printk (KERN_ERR "%s: invalid entry %s\n", __func__, entry);
		} else
			printk (KERN_ERR "%s: invalid entry (missing colon) %s: %p.%p\n", __func__, entry,colon,entryend);
		next=comma+1 ;
	}
	if (0 == result) {
		create_proc_read_entry(name,0444,0,read_proc,0);
		if (0 < pin_count) {
			struct gpio_output_pin_t *pin, *next;
			// find node containing this pdev, kill associated cdev and clear node
			list_for_each_entry_safe(pin, next, &pin_list, list_head) {
				dev_t devnum = MKDEV(gpio_output_major, pin->gpio);
				cdev_init(&pin->cdev, &gpio_output_fops);
				pin->cdev.owner = THIS_MODULE ;
				// Register cdev
				if (0 == (result = cdev_add(&pin->cdev, devnum, 1) )) {
					device_create(gpio_class, NULL, devnum, NULL, pin->name);
					printk (KERN_ERR "%s: registered %s, devnum %x\n", name, pin->name, devnum );
				} else {
					printk (KERN_ERR "%s: couldn't add device: err %d\n", pin->name, result);
					list_del(&pin->list_head);
					kobject_put(&pin->cdev.kobj);
					kfree(pin);
					--pin_count ;
				}
			}
		}
	}
	return result;
}

static int gpio_output_remove(struct platform_device *pdev)
{
	struct gpio_output_pin_t *pin, *next;

	printk (KERN_ERR "----------%s\n", __func__ );
	unregister_chrdev_region(MKDEV(gpio_output_major, 0), ARCH_NR_GPIOS);
	// find node containing this pdev, kill associated cdev and clear node
	list_for_each_entry_safe(pin, next, &pin_list, list_head) {
		printk (KERN_ERR "remove pin %u.%s.%u\n",pin->gpio,pin->name,pin->initlevel);
		device_destroy(gpio_class, MKDEV(gpio_output_major, pin->gpio));
		list_del(&pin->list_head);
		cdev_del(&pin->cdev);
                gpio_free(pin->gpio);
		kfree(pin);
	}
        remove_proc_entry(name,0);
	return 0 ;
}

static struct platform_driver gpio_output_driver = {
	.driver = {
		.name = name,
		},
	.probe = gpio_output_probe,
	.remove = gpio_output_remove,
};

int gpio_output_init(void)
{
	int result;

        INIT_LIST_HEAD(&pin_list);

	gpio_class = class_create(THIS_MODULE, name);
	if (IS_ERR(gpio_class)) {
		printk(KERN_ERR "Error creating %s class.\n", name);
		result = PTR_ERR(gpio_class);
	} else {
		// Register driver itself
		if ( 0 > (result = platform_driver_register(&gpio_output_driver))) {
			printk(KERN_ERR "%s: couldn't register driver, err %d\n",
				name, result);
		}
	}

	return result;
}

void gpio_output_exit(void)
{
	platform_driver_unregister(&gpio_output_driver);
	class_destroy(gpio_class);
}

module_init(gpio_output_init);
module_exit(gpio_output_exit);

/* module bookkeeping */
MODULE_AUTHOR("Eric Nelson (eric.nelson@boundarydevices.com)");
MODULE_LICENSE("GPL");
