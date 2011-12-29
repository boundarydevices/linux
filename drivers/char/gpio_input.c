/*
 * MX51 2XX GPIO driver
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
 * Copyright (C) 2008, Boundary Devices
 *
 */
#include <linux/module.h>
#include <linux/init.h>

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/console.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/gpio-trig.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/mach/irq.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>

#define GPIO_MAJOR 0 //0 means dynmaic assignment
static int gpio_major = GPIO_MAJOR ;

static char const driverName[] = {
	"gpio_input"
};

static char gpio_input_spec[256] = {CONFIG_GPIO_INPUT_DEFAULT};
module_param_string(spec, gpio_input_spec, sizeof(gpio_input_spec), 0644);
MODULE_PARM_DESC(spec, "Specification for gpio input params");

static struct class *gpio_class = 0;
static struct list_head pin_list ;
static int pin_count=0 ;

#define MAXNAMELEN 32
struct gpio_input_pin_t {
	struct list_head 	list_head;	// see global dev_list
	unsigned 		gpio ;
	char	 		name[MAXNAMELEN];
	struct cdev 		cdev;
};

struct gpio_data {
	unsigned		trigger_pin ;
        wait_queue_head_t 	wait_queue ; // Used for blocking read, poll
	unsigned char		take ;
	unsigned char		add ;
	unsigned char		data[256];
};

static int total_ints = 0 ;

static ssize_t gpio_write(struct file * file, const char __user * buf,
		        size_t count, loff_t *ppos)
{
	printk( KERN_ERR "%s\n", __func__ );
	return count ;
}

/* Status readback conforming to ieee1284 */
static ssize_t gpio_read(struct file * file, char __user * buf,
		       size_t count, loff_t *ppos)
{
	struct gpio_data *data = (struct gpio_data *)file->private_data ;
	if( data ){
		ssize_t rval = 0 ;
		do {
			while( (0 < count) && (data->add != data->take) ){
				unsigned char left = (data->add-data->take);
				if( left > count ){
					left = count ;
				}
				if( copy_to_user(buf,data->data+data->take,1) )
					return -EFAULT ;
				data->take++ ;
				count-- ;
				buf++ ;
				rval++ ;
			}
			if( (0 == rval) && (0==(file->f_flags & O_NONBLOCK)) ){
                                if( wait_event_interruptible(data->wait_queue, data->add != data->take) ){
					break;
				}
			} // wait for first
			else
				break;
		} while( 1 );

		return (0 == rval) ? -EINTR : rval ;
	}
	else
		return -EFAULT ;
}

static unsigned int gpio_poll(struct file *filp, poll_table *wait)
{
	struct gpio_data *data = (struct gpio_data *)filp->private_data ;
	if( data ){
		poll_wait(filp, &data->wait_queue, wait);
		return (data->add != data->take) ? POLLIN | POLLRDNORM : 0 ;
	}
	else
		return -EINVAL ;
}

static void sample_data (struct gpio_data *data) {
	data->data[data->add++] = gpio_get_value(data->trigger_pin);
	wake_up(&data->wait_queue);
}

static irqreturn_t int_handler( int irq, void *param)
{
        ++total_ints ;
	if( param && 1 ){
		struct file *file = (struct file *)param ;
                struct gpio_data *data = (struct gpio_data *)file->private_data ;
                sample_data (data);
	}
	return IRQ_HANDLED ;
}

static int gpio_open(struct inode * inode, struct file * file)
{
        unsigned int gpio ;
	struct gpio_data *data = kmalloc(sizeof(struct gpio_data),GFP_KERNEL);
        file->private_data = data ;
	memset(file->private_data,0,sizeof(*data));
	init_waitqueue_head(&data->wait_queue);
        gpio = MINOR (file->f_dentry->d_inode->i_rdev);
	if (0 != gpio) {
		set_irq_type(gpio_to_irq(gpio), IRQ_TYPE_EDGE_BOTH);
		printk (KERN_ERR "%s: requesting %u, irq %d\n", __func__, gpio, gpio_to_irq(gpio));
		if( 0 == request_irq(gpio_to_irq(gpio), int_handler, IRQF_DISABLED, driverName, file) ){
			data->trigger_pin = gpio ;
			sample_data (data);
		}
		else {
			printk( KERN_ERR "Error grabbing interrupt for GPIO %u\n", gpio );
		}
	}
	return 0 ;
}

static int gpio_release(struct inode * inode, struct file * file)
{
        if(file->private_data){
		struct gpio_data *data = (struct gpio_data *)file->private_data ;
		printk (KERN_ERR "%s: releasing %u, irq %d\n", __func__, data->trigger_pin, gpio_to_irq(data->trigger_pin));
		free_irq(gpio_to_irq(data->trigger_pin), file);
		kfree(file->private_data);
                file->private_data = 0 ;
	}
	return 0 ;
}

static int gpio_ioctl(struct inode *inode, struct file *file,
		    unsigned int cmd, unsigned long arg)
{
	switch (cmd){
		case GPIO_TRIG_CFG:{
			struct gpio_data *data = (struct gpio_data *)file->private_data ;
			unsigned new_pin ;
			if( 0 == copy_from_user(&new_pin, (void *)arg, sizeof(new_pin)) ){
				if( data->trigger_pin ){
					printk (KERN_ERR "%s: releasing %u, irq %d\n", __func__, data->trigger_pin, gpio_to_irq(data->trigger_pin));
					free_irq(gpio_to_irq(data->trigger_pin), file);
					data->trigger_pin = 0 ;
				} // trigger previously set

				set_irq_type(gpio_to_irq(new_pin), IRQ_TYPE_EDGE_BOTH);
				printk (KERN_ERR "%s: requesting %u, irq %d\n", __func__, new_pin, gpio_to_irq(new_pin));
			        if( 0 == request_irq(gpio_to_irq(new_pin), int_handler, IRQF_DISABLED, driverName, file) ){
					data->trigger_pin = new_pin ;
					return 0 ;
				}
				else {
					printk( KERN_ERR "Error grabbing interrupt for GPIO %u\n", new_pin );
				}
			}

			break;
		}
		default:
			printk( KERN_ERR "%s: Unknown cmd 0x%x\n", __func__, cmd );
	}
	return -EFAULT ;
}

static const struct file_operations gpio_fops = {
	.owner		= THIS_MODULE,
	.write		= gpio_write,
	.ioctl		= gpio_ioctl,
	.open		= gpio_open,
	.release	= gpio_release,
	.read		= gpio_read,
	.poll		= gpio_poll,
};

static void create_devs(void)
{
	char const *next=gpio_input_spec;
	char const *end=next+strlen(gpio_input_spec);
	while (next && (next<end)) {
		char entry[sizeof(gpio_input_spec)];
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
			char *pinname ;
			unsigned gpnum;
			*colon=0;
			gpnum=simple_strtoul(entry,0,0);
			pinname=colon+1 ;
			colon=strchr(pinname,':');
			if (gpio_is_valid(gpnum)) {
				struct gpio_input_pin_t *pin = kzalloc(sizeof(*pin), GFP_KERNEL);
				if (pin) {
					int rv ;
					pin->gpio=gpnum ;
					strcpy(pin->name,pinname);
					if (0 == (rv=gpio_request(gpnum,pin->name))) {
						rv = gpio_direction_input(gpnum);
						if (0 == rv) {
							INIT_LIST_HEAD(&pin->list_head);
							list_add_tail(&pin->list_head, &pin_list);
							++pin_count ;
						} else
							printk (KERN_ERR "%s: error %d setting pin %s as input\n",
								driverName,rv,pinname);
					}
					else {
						printk (KERN_ERR "%s: error %d allocating %s\n", driverName,rv, pinname);
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

	if (0 < pin_count) {
		struct gpio_input_pin_t *pin, *next;
		// find node containing this pdev, kill associated cdev and clear node
		list_for_each_entry_safe(pin, next, &pin_list, list_head) {
			int result ;
			dev_t devnum = MKDEV(gpio_major, pin->gpio);
			cdev_init(&pin->cdev, &gpio_fops);
			pin->cdev.owner = THIS_MODULE ;
			// Register cdev
			if (0 == (result = cdev_add(&pin->cdev, devnum, 1) )) {
				device_create(gpio_class, NULL, devnum, NULL, pin->name);
				printk (KERN_ERR "%s: registered %s, devnum %x\n", driverName, pin->name, devnum );
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

static int __init gpio_init_module (void)
{
	int result ;

        INIT_LIST_HEAD(&pin_list);

	printk (KERN_ERR "%s: spec %s\n", __FILE__, gpio_input_spec);
	result = register_chrdev(gpio_major,driverName,&gpio_fops);
	if (result<0) {
		printk (KERN_WARNING __FILE__": Couldn't register device %d.\n", gpio_major);
		return result;
	}
	if (gpio_major==0)
		gpio_major = result; //dynamic assignment

	gpio_class = class_create(THIS_MODULE, driverName);
	if (IS_ERR(gpio_class)) {
                unregister_chrdev(gpio_major,driverName);
		return -EIO ;
	} else {
		printk (KERN_INFO "GPIO input driver. Boundary Devices\n");
		create_devs();
		return 0 ;
	}
}

static void gpio_cleanup_module (void)
{
	class_destroy(gpio_class);
        remove_proc_entry("triggers", 0 );
	unregister_chrdev(gpio_major,driverName);
}

module_init(gpio_init_module);
module_exit(gpio_cleanup_module);

MODULE_ALIAS_CHARDEV_MAJOR(LP_MAJOR);
MODULE_LICENSE("GPL");
