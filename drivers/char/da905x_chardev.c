/*
 * DA905x PMIC char driver
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
#include <linux/platform_device.h>
#include <linux/mfd/da9052/da9052.h>
#include <linux/mfd/da9052/reg.h>
#include <linux/mfd/da9052/dev.h>
#include <linux/cdev.h>

static int da905x_chardev_major = 0;
static int da905x_chardev_minor = 0;
module_param(da905x_chardev_major, int, S_IRUGO);
module_param(da905x_chardev_minor, int, S_IRUGO);

struct da9052_chardev_priv {
	struct da9052 *da9052;
	struct platform_device *pdev;
	struct cdev cdev;
};

static dev_t devnum;
static struct class *da90_class = 0;

extern struct da9052 *get_da9052(void);

static char const name[] = {
	"da905x_chardev"
};

static char const shortname[] = {
	"da905x"
};

static int da905x_chardev_open(struct inode *inode, struct file *file)
{
	return 0 ;
}

static int da905x_chardev_release(struct inode *inode, struct file *file)
{
	return 0 ;
}

static int da905x_chardev_ioctl
	(struct inode *inode, struct file *f, unsigned int cmd, unsigned long arg)
{
	int retval = -EINVAL ;
        struct da9052 * const da9052 = get_da9052();

	if (da9052 && da9052->read && da9052->write) {
		switch (cmd) {
			case DA905X_GETREG: {
				da90_reg_and_value_t regnv ;
				if (0 == copy_from_user(&regnv,(void *)arg,sizeof(regnv))) {
					struct da9052_ssc_msg msg = {0};
					msg.addr = GETREGNUM(regnv);
					if (0 == (retval=da9052->read(da9052, &msg))) {
						regnv = MAKERNV(msg.addr,msg.data);
						if (0 == copy_to_user((void *)arg,&regnv,sizeof(regnv))) {
							retval = -EACCES ;
						}
					} else {
						printk (KERN_ERR "%s: read error %d\n", __func__, retval);
					}
				} else
					retval = -EACCES ;
				break;
			}
			case DA905X_SETREG: {
				da90_reg_and_value_t regnv ;
				if (0 == copy_from_user(&regnv,(void *)arg,sizeof(regnv))) {
					struct da9052_ssc_msg msg = {0};
					msg.addr = GETREGNUM(regnv);
					msg.data = GETVAL(regnv);;
					if (0 != (retval=da9052->write(da9052, &msg))) {
						printk (KERN_ERR "%s: write error %d\n", __func__, retval);
					}
				} else
					retval = -EACCES ;
				break;
			}
			default: {
				printk (KERN_ERR "%s: invalid cmd 0x%x\n", __func__, cmd );
			}
		}
	} else {
		printk (KERN_ERR "%s: no da9052 found\n", __func__ );
		retval = -EACCES ;
	}

	return 0 ;
}

static struct file_operations da905x_chardev_fops = {
	.owner = THIS_MODULE,
	.open = da905x_chardev_open,
	.release = da905x_chardev_release,
	.ioctl = da905x_chardev_ioctl,
};

/*****************************
* driver framework functions *
*****************************/

static int da905x_chardev_probe(struct platform_device *pdev)
{
	int result = 0;
	struct da9052_chardev_priv *priv;

	// Create device structure
	if ((priv = kzalloc(sizeof(struct da9052_chardev_priv), GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "%s: No memory left!", name);
		return -ENOMEM;
	}

	// Associate pdev and initialize cdev
	priv->pdev = pdev;
	cdev_init(&priv->cdev, &da905x_chardev_fops);
	priv->cdev.owner = THIS_MODULE ;
	// Register cdev
	if (0 == (result = cdev_add(&priv->cdev, MKDEV(da905x_chardev_major, 0), 1) )) {
		dev_t devnum = MKDEV(da905x_chardev_major, 0);
		device_create(da90_class, NULL, devnum, NULL, shortname);
		printk (KERN_ERR "%s: registered %s, devnum %x\n", __func__, name, devnum );
	} else {
		printk (KERN_ERR "%s: couldn't add device: err %d\n", name, result);
		kobject_put(&priv->cdev.kobj);
		kfree(priv);
	}

	return result;
}

static int da905x_chardev_remove(struct platform_device *pdev)
{
	device_destroy(da90_class, MKDEV(da905x_chardev_major, 0));
	return 0 ;
}

static struct platform_driver da905x_chardev_driver = {
	.driver = {
		.name = name,
		},
	.probe = da905x_chardev_probe,
	.remove = da905x_chardev_remove,
};

int da905x_chardev_init(void)
{
	int result;

	devnum = 0 ;
	da90_class = class_create(THIS_MODULE, name);
	if (IS_ERR(da90_class)) {
		printk(KERN_ERR "Error creating %s class.\n", name);
		result = PTR_ERR(da90_class);
	} else if (da905x_chardev_major) {
		devnum = MKDEV(da905x_chardev_major, da905x_chardev_minor);
		result = register_chrdev_region(devnum, 1, name);
	} else {
		result = alloc_chrdev_region(&devnum, da905x_chardev_minor, 1, name);
		da905x_chardev_major = MAJOR(devnum);
	}

	if (result < 0) {
		printk(KERN_ERR "%s: couldn't get major %d: err %d\n", name,
			da905x_chardev_major, result);
		goto fail_devnum;
	}

	// Register driver itself
	if ((result = platform_driver_register(&da905x_chardev_driver)) < 0) {
		printk(KERN_ERR "%s: couldn't register driver, err %d\n",
			name, result);
		goto fail_pdev;
	}

	return 0;

fail_pdev:
	unregister_chrdev_region(devnum, 1);
fail_devnum:
	return result;
}

void da905x_chardev_exit(void)
{
	platform_driver_unregister(&da905x_chardev_driver);
	unregister_chrdev_region(devnum, 1);
        class_destroy(da90_class);
}

module_init(da905x_chardev_init);
module_exit(da905x_chardev_exit);

/* module bookkeeping */
MODULE_AUTHOR("Eric Nelson (eric.nelson@boundarydevices.com)");
MODULE_LICENSE("GPL");
