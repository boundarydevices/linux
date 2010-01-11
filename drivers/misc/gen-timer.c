/*
 * gen-timer:
 *
 * This driver gives a simple poll()'able interface to the system
 * timer.
 *
 * Because it uses the normal kernel timer mechanism, it is not very
 * accurate, but it is simple and solves the problem of how to schedule
 * timeouts in a poll() friendly way.
 *
 * Copyright (c) Boundary Devices, 2003
 *
 * http://kernelbook.sourceforge.net/kernel-locking.html/racing-timers.html
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/poll.h>

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/spinlock.h>

#include <linux/gen-timer.h>

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include <asm/system.h>
#include <asm/mach/map.h>
long long timeValToMs(struct timeval const *tv)
{
	return ((long long)tv->tv_sec * 1000) +
	    ((long long)tv->tv_usec / 1000);
} long long tickMs(void)
{
	struct timeval now;
	do_gettimeofday(&now);
	return timeValToMs(&now);
}

#define VERSION "$Revision: 1.5 $"		/* Driver revision number */
    typedef struct {
	struct timer_list tl;
	long long expiration;
	 int fired;
	 wait_queue_head_t waitq;
} timerInfo_t;

#define TIMERINFO( __filp ) ((timerInfo_t *)( (__filp)->private_data ))
static void expired(unsigned long parameters)
{
	timerInfo_t * ti = (timerInfo_t *) parameters;
	wake_up_interruptible(&ti->waitq);
} static int timer_open(struct inode *inode, struct file *filp)
{
	timerInfo_t * ti;
	if (filp->f_mode & FMODE_WRITE)
		return -EINVAL;
	ti = (timerInfo_t *) kmalloc(sizeof(timerInfo_t), GFP_KERNEL);
	init_timer(&ti->tl);
	ti->tl.data = (unsigned long)ti;
	ti->tl.function = expired;
	ti->expiration = tickMs();
	ti->fired = 1;
	init_waitqueue_head(&ti->waitq);
	filp->private_data = ti;
	return 0;
}
static int timer_release(struct inode *inode, struct file *filp)
{
	timerInfo_t * ti = TIMERINFO(filp);
	del_timer(&ti->tl);
	if (filp->private_data)
		kfree(filp->private_data);
	return 0;
}


// This function is called when a user space program attempts to write /dev/timer
static ssize_t timer_write(struct file *filp, const char *buffer, size_t count,
			   loff_t * ppos)
{
	return -EINVAL;
}
static unsigned int timer_poll(struct file *filp, poll_table * wait)
{
	timerInfo_t * ti = TIMERINFO(filp);
	int returnval;
	if (!ti->fired){
		long long when = ti->expiration;
		long long now = tickMs();
		if ((when - now) > 0){
			int waitJifs = ((int)(when - now)) * HZ / 1000;
			unsigned long const exp = jiffies + waitJifs + 1;
			if (timer_pending(&ti->tl))
				mod_timer(&ti->tl, exp);

			else {
				ti->tl.expires = exp;
				add_timer(&ti->tl);
			}
			poll_wait(filp, &ti->waitq, wait);
			returnval = 0;
		} else {
			ti->fired = 1;
			returnval = POLLIN | POLLRDNORM;
		}
	} else
		returnval = 0;	// idle... no timer set
	return returnval;
}


// This function is called when a user space program attempts to read /dev/timer
static ssize_t timer_read(struct file *filp, char *buffer, size_t count,
			  loff_t * ppos)
{
	int rval;
	timerInfo_t * ti = TIMERINFO(filp);
	long long now = tickMs();
	if (!ti->fired){
		long long when = ti->expiration;
		if (((when - now) > 0) && ((filp->f_flags & O_NONBLOCK) == 0))
			 {
			int waitJifs = ((int)(when - now)) * HZ / 1000;
			ti->tl.expires = jiffies + waitJifs + 1;
			add_timer(&ti->tl);
			interruptible_sleep_on(&ti->waitq);
			now = tickMs();
			}	// if we need to wait
		ti->fired = 1;
	} // have timeout
	if (sizeof(now) < count)
		count = sizeof(now);
	rval = copy_to_user(buffer, &now, count);
	if (rval)
		count = (ssize_t) - EINVAL;
	return count;
}


// This routine allows the driver to implement device-
// specific ioctl's.  If the ioctl number passed in cmd is
// not recognized by the driver, it should return ENOIOCTLCMD.
static int timer_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	int return_val = -ENOTTY;
	timerInfo_t * ti = TIMERINFO(filp);
	switch (cmd){
		case SET_TSTAMP:{
			long long exp;
			if (0 ==
			     copy_from_user(&exp, (void *)arg, sizeof(exp)))
				 {
				ti->expiration = exp;
				ti->fired = 0;
				return_val = 0;
				}

			else
				return_val = -EFAULT;
			break;
		}
		case CLEAR_TSTAMP:{
			ti->fired = 1;
			return_val = 0;
			break;
		}
		case GET_TSTAMP:{
			long long exp = ti->expiration;
			return_val =
			    (copy_to_user((void *)arg, &exp, sizeof(exp))) ?
			    -EFAULT : 0;
			break;
		}
	}
	return return_val;
}


// This structure is the file operations structure, which specifies what
// callbacks functions the kernel should call when a user mode process
// attempts to perform these operations on the device.
static struct file_operations timer_fops = {
	owner: 		THIS_MODULE,
	open: 		timer_open,
	read: 		timer_read,
	poll: 		timer_poll,
	write: 		timer_write,
	ioctl: 		timer_ioctl,
	release:	timer_release
#ifdef ASYNC_INTERFACE
	fasync:		timer_fasync,
#endif	/*  */
};


static struct miscdevice timer_misc = {
	MISC_DYNAMIC_MINOR, "gen-timer", &timer_fops
};
static int timer_registered;

//
// This function is called to initialise the driver, either from misc.c at
// bootup if the driver is compiled into the kernel, or from init_module
// below at module insert time. It attempts to register the device node.
//
static int __init timer_init(void)
{
	int error = misc_register(&timer_misc);
	if (error)
		printk(KERN_WARNING "mice: could not register gen-timer device, "
			"error: %d\n", error);
	else {
		timer_registered = 1;
		printk(KERN_ERR "generic timer driver version " VERSION " from Boundary Devices, 2007\n" );
	}

	return error ;
}
static void __exit timer_exit(void)
{
   if( timer_registered ){
      misc_deregister(&timer_misc);
   }
}

MODULE_AUTHOR("Boundary Devices");
MODULE_LICENSE("GPL");

module_init(timer_init);
module_exit(timer_exit);
