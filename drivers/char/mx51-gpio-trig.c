/*
 * MX51 2XX triggered GPIO driver
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

#include <mach/hardware.h>
#include <mach/gpio.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/mach/irq.h>
#include <linux/proc_fs.h>

#define GPIO_MAJOR 0 //0 means dynmaic assignment
// #define GPIO_MAJOR 252 //0 means dynmaic assignment
static int gpio_major = GPIO_MAJOR ;
static char const driverName[] = {
	"gpio-trig"
};

#include <linux/gpio-trig.h>

struct gpio_data {
	struct trigger_t 	trig ;
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

static int gpio_open(struct inode * inode, struct file * file)
{
	struct gpio_data *data = kmalloc(sizeof(struct gpio_data),GFP_KERNEL);
	file->private_data = data ;
	memset(file->private_data,0,sizeof(*data));
	init_waitqueue_head(&data->wait_queue);
	return 0 ;
}

static int gpio_release(struct inode * inode, struct file * file)
{
	if(file->private_data){
		struct gpio_data *data = (struct gpio_data *)file->private_data ;
		struct trigger_t *pt = &data->trig ;
		printk (KERN_ERR "%s: releasing %u, irq %d\n", __func__, pt->trigger_pin, gpio_to_irq(pt->trigger_pin));
		free_irq(gpio_to_irq(pt->trigger_pin), file);
		kfree(file->private_data);
		file->private_data = 0 ;
	}
	return 0 ;
}

static void sample_data (struct gpio_data *data) {
	struct trigger_t *pt = &data->trig ;
	if( pt ){
		int i ;
		unsigned char rval = 0 ;
		for( i = 0 ; i < pt->num_pins ; i++ ){
			unsigned gpio = pt->pins[i];
			unsigned next = (0 != gpio_get_value(gpio));
			rval <<= 1 ;
			rval |= next ;
		}
		data->data[data->add++] = rval ;
		wake_up(&data->wait_queue);
	}
}

static irqreturn_t int_handler( int irq, void *param)
{
        ++total_ints ;
	if( param && 1 ){
		struct file *file = (struct file *)param ;
                struct gpio_data *data = (struct gpio_data *)file->private_data ;
		sample_data(data);
	}
	return IRQ_HANDLED ;
}

static int gpio_ioctl(struct inode *inode, struct file *file,
		    unsigned int cmd, unsigned long arg)
{
	switch (cmd){
		case GPIO_TRIG_CFG:{
			struct gpio_data *data = (struct gpio_data *)file->private_data ;
			struct trigger_t *pt = &data->trig ;
			struct trigger_t trig ;
			if( 0 == copy_from_user(&trig, (void *)arg, sizeof(trig)) ){
				pt = (struct trigger_t *)file->private_data ;
				if( pt->trigger_pin ){
					printk (KERN_ERR "%s: releasing %u, irq %d\n", __func__, pt->trigger_pin, gpio_to_irq(pt->trigger_pin));
					free_irq(gpio_to_irq(pt->trigger_pin), file);
					pt->trigger_pin = 0 ;
				} // trigger previously set

				set_irq_type(gpio_to_irq(trig.trigger_pin), trig.rising_edge ? IRQ_TYPE_EDGE_RISING : IRQ_TYPE_EDGE_FALLING);
				printk (KERN_ERR "%s: requesting %u, irq %d\n", __func__, trig.trigger_pin, gpio_to_irq(trig.trigger_pin));
				if( 0 == request_irq(gpio_to_irq(trig.trigger_pin), int_handler, IRQF_DISABLED, driverName, file) ){
					memcpy(pt,&trig,sizeof(trig));
					sample_data(data);
					return 0 ;
				}
				else {
					pt->trigger_pin = 0 ;
					printk( KERN_ERR "Error grabbing interrupt for GPIO %u\n", trig.trigger_pin );
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

#ifndef MODULE
static int __init gpio_setup (char *str)
{
	return 1;
}
#endif


static int read_proc( char *page, char **start, off_t off,
                      int count, int *eof, void *data)
{
        int int_count = total_ints ;
	total_ints = 0 ;
	return snprintf( page, count, "%u interrupts\n", int_count );
}

static int __init gpio_init_module (void)
{
	int result ;
        struct proc_dir_entry *pde ;

	result = register_chrdev(gpio_major,driverName,&gpio_fops);
	if (result<0) {
		printk (KERN_WARNING __FILE__": Couldn't register device %d.\n", gpio_major);
		return result;
	}
	if (gpio_major==0)
		gpio_major = result; //dynamic assignment

	pde = create_proc_entry("triggers", 0, 0);
	if( pde ) {
		pde->read_proc  = read_proc ;
	}

	printk (KERN_INFO "Triggered GPIO driver. Boundary Devices\n");
	return 0 ;
}

static void gpio_cleanup_module (void)
{
        remove_proc_entry("triggers", 0 );
	unregister_chrdev(gpio_major,driverName);
}

__setup("pxagpio=", gpio_setup);
module_init(gpio_init_module);
module_exit(gpio_cleanup_module);

MODULE_ALIAS_CHARDEV_MAJOR(LP_MAJOR);
MODULE_LICENSE("GPL");
