/*
 * drivers/input/touchscreen/sitronix_i2c_touch.c
 *
 * Touchscreen driver for Sitronix (I2C bus)
 *
 * Copyright (C) 2011 Sitronix Technology Co., Ltd.
 *	Rudy Huang <rudy_huang@sitronix.com.tw>
 */
/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/delay.h>

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>

#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif


#include "sitronix_i2c_touch.h"
#ifdef SITRONIX_FW_UPGRADE_FEATURE
#include <linux/cdev.h>
#include <asm/uaccess.h>
#ifdef SITRONIX_PERMISSION_THREAD
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/syscalls.h>
#endif // SITRONIX_PERMISSION_THREAD
#endif // SITRONIX_FW_UPGRADE_FEATURE
#include <linux/i2c.h>
#include <linux/input.h>
#ifdef SITRONIX_SUPPORT_MT_SLOT
#include <linux/input/mt.h>
#endif // SITRONIX_SUPPORT_MT_SLOT
#include <linux/interrupt.h>
#include <linux/slab.h> // to be compatible with linux kernel 3.2.15
#include <linux/gpio.h>
//#include <mach/gpio.h>
#ifdef SITRONIX_MONITOR_THREAD
#include <linux/kthread.h>
#endif // SITRONIX_MONITOR_THREAD
//#include <linux/sp_capture.h>


#include <linux/kthread.h>
#include <linux/path.h>
#include <linux/namei.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>

#ifdef SITRONIX_MULTI_SLAVE_ADDR
#if defined(CONFIG_MACH_OMAP4_PANDA)
#include <plat/gpio.h>
#endif //defined(CONFIG_MACH_OMAP4_PANDA)
#endif // SITRONIX_MULTI_SLAVE_ADDR

#define DRIVER_AUTHOR           "Sitronix, Inc."
#define DRIVER_NAME             "sitronix"
#define DRIVER_DESC             "Sitronix I2C touch"
#define DRIVER_DATE             "20180809"
#define DRIVER_MAJOR            2
#define DRIVER_MINOR	10
#define DRIVER_PATCHLEVEL       2
#define NAME_SIZE  20

MODULE_AUTHOR("Petitk Kao<petitk_kao@sitronix.com.tw>");
MODULE_DESCRIPTION("Sitronix I2C multitouch panels");
MODULE_LICENSE("GPL");
struct ctp_config_info{
	int input_type;
	char	*name;
	int  int_number;
};


struct ctp_config_info config_info = {
	.input_type = 1,
	.name = NULL,
	.int_number = 0,
};

#define CTP_IRQ_NUMBER          (config_info.int_number)
#define CTP_IRQ_MODE			(IRQF_TRIGGER_FALLING)
#define CTP_NAME		("sitronix")
//#define SCREEN_MAX_X	(screen_max_x)
#define SCREEN_MAX_Y	(screen_max_y)
#define PRESS_MAX		(255)

#if 0
static int screen_max_x = 0;
static int screen_max_y = 0;
static int revert_x_flag = 0;
static int revert_y_flag = 0;
static int exchange_x_y_flag = 0;
#endif
#ifdef SITRONIX_MULTI_SLAVE_ADDR
static __u32 twi_id = 0;
#endif
//static char irq_pin_name[8];


char sitronix_sensor_key_status = 0;
struct sitronix_sensor_key_t sitronix_sensor_key_array[] = {
	{KEY_MENU}, // bit 2
	{KEY_HOMEPAGE}, // bit 1
	{KEY_BACK}, // bit 0
};
#ifdef SITRONIX_AA_KEY
char sitronix_aa_key_status = 0;

#ifdef SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
#define SITRONIX_TOUCH_RESOLUTION_X 480 /* max of X value in display area */
#define SITRONIX_TOUCH_RESOLUTION_Y 854 /* max of Y value in display area */
#define SITRONIX_TOUCH_GAP_Y	10  /* Gap between bottom of display and top of touch key */
#define SITRONIX_TOUCH_MAX_Y 915  /* resolution of y axis of touch ic */
struct sitronix_AA_key sitronix_aa_key_array[] = {
	{15, 105, SITRONIX_TOUCH_RESOLUTION_Y + SITRONIX_TOUCH_GAP_Y, SITRONIX_TOUCH_MAX_Y, KEY_MENU}, /* MENU */
	{135, 225, SITRONIX_TOUCH_RESOLUTION_Y + SITRONIX_TOUCH_GAP_Y, SITRONIX_TOUCH_MAX_Y, KEY_HOME},
	{255, 345, SITRONIX_TOUCH_RESOLUTION_Y + SITRONIX_TOUCH_GAP_Y, SITRONIX_TOUCH_MAX_Y, KEY_BACK}, /* KEY_EXIT */
	{375, 465, SITRONIX_TOUCH_RESOLUTION_Y + SITRONIX_TOUCH_GAP_Y, SITRONIX_TOUCH_MAX_Y, KEY_SEARCH},
};
#else
#define SCALE_KEY_HIGH_Y 15
struct sitronix_AA_key sitronix_aa_key_array[] = {
	{0, 0, 0, 0, KEY_MENU}, /* MENU */
	{0, 0, 0, 0, KEY_HOME},
	{0, 0, 0, 0, KEY_BACK}, /* KEY_EXIT */
	{0, 0, 0, 0, KEY_SEARCH},
};
#endif // SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
#endif // SITRONIX_AA_KEY
struct rst_pin_ctrl{
	struct i2c_client *client;
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*rst_pin_ts_active;
	struct pinctrl_state	*rst_pin_ts_suspend;
	struct pinctrl_state	*rst_pin_ts_release;

};
struct sitronix_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct input_dev *keyevent_input;
	int use_irq;
#ifndef SITRONIX_INT_POLLING_MODE
	struct work_struct  work;
#else
	struct delayed_work work;
#endif // SITRONIX_INT_POLLING_MODE
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif // CONFIG_HAS_EARLYSUSPEND
	uint8_t fw_revision[4];
	int resolution_x;
	int resolution_y;
	uint8_t max_touches;
	uint8_t touch_protocol_type;
	uint8_t pixel_length;
	uint8_t chip_id;
#ifdef SITRONIX_MONITOR_THREAD
	uint8_t enable_monitor_thread;
	uint8_t RawCRC_enabled;
	int (*sitronix_mt_fp)(void *);
#endif // SITRONIX_MONITOR_THREAD
	uint8_t Num_X;
	uint8_t Num_Y;
	uint8_t sensing_mode;
	int suspend_state;
	spinlock_t irq_lock;
	u8 irq_is_disable;
	int irq_gpio;
	u32 irq_gpio_flags;
	int reset_gpio;
	u32 reset_gpio_flags;
	//int irq;
#if (CTP_AP_SP_SYNC_WAY != CTP_AP_SP_SYNC_NONE)
	// gavin TOUCH_DRIVE_T sitronix_driver_ops;
#endif

#if(CTP_AP_SP_SYNC_WAY & CTP_AP_SP_SYNC_GPIO)
	struct work_struct sitronix_io_sync_work;
#endif
};

#if (CTP_AP_SP_SYNC_WAY != CTP_AP_SP_SYNC_NONE)
static struct sitronix_ts_data *gSitronixPtr = NULL;
#endif

#if(CTP_AP_SP_SYNC_WAY & CTP_AP_SP_SYNC_GPIO)
//static struct gpio_config sitronix_sync_io;
static struct timer_list ctp_sync_timer;
static int ctp_sync_io_last_status = 1;
static int ctp_sync_pulse_count = 0;
static spinlock_t ctp_sync_lock;
static struct workqueue_struct *sitronix_io_sync_workqueue = NULL;
#endif

static int i2cErrorCount = 0;

#ifdef SITRONIX_MONITOR_THREAD
static struct task_struct * SitronixMonitorThread = NULL;
static int gMonitorThreadSleepInterval = 300; // 0.3 sec
static atomic_t iMonitorThreadPostpone = ATOMIC_INIT(0);

static uint8_t PreCheckData[4] ;
static int StatusCheckCount = 0;
static int sitronix_ts_delay_monitor_thread_start = DELAY_MONITOR_THREAD_START_PROBE;
#endif // SITRONIX_MONITOR_THREAD

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sitronix_ts_early_suspend(struct early_suspend *h);
static void sitronix_ts_late_resume(struct early_suspend *h);
#endif // CONFIG_HAS_EARLYSUSPEND
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data);

static struct sitronix_ts_data sitronix_ts_gpts = {0};
//static struct rst_pin_ctrl *rst_pin;
static atomic_t sitronix_ts_irq_on = ATOMIC_INIT(0);
static atomic_t sitronix_ts_in_int = ATOMIC_INIT(0);
#ifdef SITRONIX_SYSFS
static bool sitronix_ts_sysfs_created = false;
static bool sitronix_ts_sysfs_using = false;
#endif // SITRONIX_SYSFS

static void sitronix_ts_reset_ic(void);
//static int sitronix_ts_disable_int(struct sitronix_ts_data *ts, uint8_t value);

#ifdef ST_UPGRADE_FIRMWARE
extern int st_upgrade_fw(void);
#endif //ST_UPGRADE_FIRMWARE

#ifdef ST_TEST_RAW
extern int st_drv_test_raw();
#endif //ST_TEST_RAW

#ifdef SITRONIX_FW_UPGRADE_FEATURE
#ifdef SITRONIX_PERMISSION_THREAD
SYSCALL_DEFINE3(fchmodat, int, dfd, const char __user *, filename, mode_t, mode);
static struct task_struct * SitronixPermissionThread = NULL;
static int sitronix_ts_delay_permission_thread_start = 1000;

static int sitronix_ts_permission_thread(void *data)
{
	int ret = 0;
	int retry = 0;
	mm_segment_t fs = get_fs();
	set_fs(KERNEL_DS);

	DbgMsg("%s start\n", __FUNCTION__);
	do{
		DbgMsg("delay %d ms\n", sitronix_ts_delay_permission_thread_start);
		msleep(sitronix_ts_delay_permission_thread_start);
		ret = sys_fchmodat(AT_FDCWD, "/dev/"SITRONIX_I2C_TOUCH_DEV_NAME , 0666);
		if(ret < 0)
			printk("fail to execute sys_fchmodat, ret = %d\n", ret);
		if(retry++ > 10)
			break;
	}while(ret == -ENOENT);
	set_fs(fs);
	DbgMsg("%s exit\n", __FUNCTION__);
	return 0;
}
#endif // SITRONIX_PERMISSION_THREAD

int      sitronix_release(struct inode *, struct file *);
int      sitronix_open(struct inode *, struct file *);
ssize_t  sitronix_write(struct file *file, const char *buf, size_t count, loff_t *ppos);
ssize_t  sitronix_read(struct file *file, char *buf, size_t count, loff_t *ppos);
long	 sitronix_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static struct cdev sitronix_cdev;
static struct class *sitronix_class;
static int sitronix_major = 0;

int  sitronix_open(struct inode *inode, struct file *filp)
{
	return 0;
}
EXPORT_SYMBOL(sitronix_open);

int  sitronix_release(struct inode *inode, struct file *filp)
{
	return 0;
}
EXPORT_SYMBOL(sitronix_release);

ssize_t  sitronix_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int ret;
	char *tmp;

	if(!(sitronix_ts_gpts.client))
		return -EIO;

	if (count > 8192)
		count = 8192;

	tmp = (char *)kmalloc(count,GFP_KERNEL);
	if (tmp==NULL)
		return -ENOMEM;
	if (copy_from_user(tmp,buf,count)) {
		kfree(tmp);
		return -EFAULT;
	}
	UpgradeMsg("writing %zu bytes.\n", count);

	ret = i2c_master_send(sitronix_ts_gpts.client, tmp, count);
	kfree(tmp);
	return ret;
}
EXPORT_SYMBOL(sitronix_write);

ssize_t  sitronix_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	char *tmp;
	int ret;

	if(!(sitronix_ts_gpts.client))
		return -EIO;

	if (count > 8192)
		count = 8192;

	tmp = (char *)kmalloc(count,GFP_KERNEL);
	if (tmp==NULL)
		return -ENOMEM;

	UpgradeMsg("reading %zu bytes.\n", count);

	ret = i2c_master_recv(sitronix_ts_gpts.client, tmp, count);
	if (ret >= 0)
		ret = copy_to_user(buf,tmp,count)?-EFAULT:ret;
	kfree(tmp);
	return ret;
}
EXPORT_SYMBOL(sitronix_read);

static int sitronix_ts_resume(struct i2c_client *client);
static int sitronix_ts_suspend(struct i2c_client *client);
void sitronix_ts_reprobe(void);
long	 sitronix_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	int retval = 0;
	uint8_t temp[4];

	if (!(sitronix_ts_gpts.client))
		return -EIO;

	if (_IOC_TYPE(cmd) != SMT_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SMT_IOC_MAXNR) return -ENOTTY;
	if (_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE)) {
		err = !access_ok((void __user *)arg,\
				 _IOC_SIZE(cmd));
		if (err) return -EFAULT;
	}

	switch(cmd) {
		case IOCTL_SMT_GET_DRIVER_REVISION:
			UpgradeMsg("IOCTL_SMT_GET_DRIVER_REVISION\n");
			temp[0] = SITRONIX_TOUCH_DRIVER_VERSION;
			if(copy_to_user((uint8_t __user *)arg, &temp[0], 1)){
				UpgradeMsg("fail to get driver version\n");
				retval = -EFAULT;
			}
			break;
		case IOCTL_SMT_GET_FW_REVISION:
			UpgradeMsg("IOCTL_SMT_GET_FW_REVISION\n");
			if(copy_to_user((uint8_t __user *)arg, &(sitronix_ts_gpts.fw_revision[0]), 4))
					retval = -EFAULT;
			break;
		case IOCTL_SMT_ENABLE_IRQ:
			UpgradeMsg("IOCTL_SMT_ENABLE_IRQ\n");
			//sitronix_ts_disable_int(&sitronix_ts_gpts, 0);
			if(!atomic_read(&sitronix_ts_in_int)){
				if(!atomic_read(&sitronix_ts_irq_on)){
					atomic_set(&sitronix_ts_irq_on, 1);
					enable_irq(sitronix_ts_gpts.client->irq);
#ifdef SITRONIX_MONITOR_THREAD
					if(sitronix_ts_gpts.enable_monitor_thread == 1){
						if(!SitronixMonitorThread){
							atomic_set(&iMonitorThreadPostpone,1);
							SitronixMonitorThread = kthread_run(sitronix_ts_gpts.sitronix_mt_fp,"Sitronix","Monitorthread");
							if(IS_ERR(SitronixMonitorThread))
								SitronixMonitorThread = NULL;
						}
					}
#endif // SITRONIX_MONITOR_THREAD
				}
			}
			break;
		case IOCTL_SMT_DISABLE_IRQ:
			UpgradeMsg("IOCTL_SMT_DISABLE_IRQ\n");
			//sitronix_ts_disable_int(&sitronix_ts_gpts, 1);
#ifndef SITRONIX_INT_POLLING_MODE
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
//gavin			flush_work_sync(&sitronix_ts_gpts.work);
#endif // LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
#else
			cancel_delayed_work_sync(&sitronix_ts_gpts.work);
#endif
			if(atomic_read(&sitronix_ts_irq_on)){
				atomic_set(&sitronix_ts_irq_on, 0);
				disable_irq_nosync(sitronix_ts_gpts.client->irq);
#ifdef SITRONIX_MONITOR_THREAD
				if(sitronix_ts_gpts.enable_monitor_thread == 1){
					if(SitronixMonitorThread){
						kthread_stop(SitronixMonitorThread);
						SitronixMonitorThread = NULL;
					}
				}
#endif // SITRONIX_MONITOR_THREAD
			}
			break;
		case IOCTL_SMT_RESUME:
			UpgradeMsg("IOCTL_SMT_RESUME\n");
			sitronix_ts_resume(sitronix_ts_gpts.client);
			break;
		case IOCTL_SMT_SUSPEND:
			UpgradeMsg("IOCTL_SMT_SUSPEND\n");
			sitronix_ts_suspend(sitronix_ts_gpts.client);
			break;
		case IOCTL_SMT_HW_RESET:
			UpgradeMsg("IOCTL_SMT_HW_RESET\n");
			sitronix_ts_reset_ic();
			break;
		case IOCTL_SMT_REPROBE:
			UpgradeMsg("IOCTL_SMT_REPROBE\n");
			sitronix_ts_reprobe();
			break;
#ifdef ST_TEST_RAW
		case IOCTL_SMT_RAW_TEST:
			UpgradeMsg("IOCTL_SMT_RAW_TEST\n");
			retval = - st_drv_test_raw();
			break;
#endif
		default:
			retval = -ENOTTY;
	}

	return retval;
}
EXPORT_SYMBOL(sitronix_ioctl);
#endif // SITRONIX_FW_UPGRADE_FEATURE

static void sitronix_ts_reset_ic(void)
{
		printk("%s\n", __FUNCTION__);
#if 0
	struct sitronix_i2c_touch_platform_data *pdata;
	uint8_t rc;
	printk("%s,line=%d\n", __FUNCTION__,__LINE__);
	rc = gpio_request(pdata->reset_gpio, "reset_gpio");

	gpio_direction_output(pdata->reset_gpio, 0);
	msleep(3);
	gpio_direction_output(pdata->reset_gpio, 1);
	msleep(150);
#endif
#if 0
	printk("%s\n", __FUNCTION__);
	__gpio_set_value(config_info.wakeup_gpio.gpio, 0);
	msleep(3);
	__gpio_set_value(config_info.wakeup_gpio.gpio, 1);
	msleep(SITRONIX_TS_CHANGE_MODE_DELAY);
#endif
}

static int sitronix_i2c_read_bytes(struct i2c_client *client, u8 addr, u8 *rxbuf, int len)
{
	int ret = 0;
	u8 txbuf = addr;
	//printk("%s,line=%d\n", __FUNCTION__,__LINE__);
#if defined(SITRONIX_I2C_COMBINED_MESSAGE)
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &txbuf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = rxbuf,
		},
	};
#endif // defined(SITRONIX_I2C_COMBINED_MESSAGE)

	if(rxbuf == NULL)
		return -1;
#if defined(SITRONIX_I2C_COMBINED_MESSAGE)
	ret = i2c_transfer(client->adapter, &msg[0], 2);
#elif defined(SITRONIX_I2C_SINGLE_MESSAGE)
	ret = i2c_master_send(client, &txbuf, 1);
	if (ret < 0){
		printk("write 0x%x error (%d)\n", addr, ret);
		return ret;
	}
	ret = i2c_master_recv(client, rxbuf, len);
#endif // defined(SITRONIX_I2C_COMBINED_MESSAGE)
	if (ret < 0){
		DbgMsg("read 0x%x error (%d)\n", addr, ret);
		return ret;
	}
	return 0;
}

static int sitronix_i2c_write_bytes(struct i2c_client *client, u8 *txbuf, int len)
{
	int ret = 0;
	//printk("%s,line=%d\n", __FUNCTION__,__LINE__);
#if defined(SITRONIX_I2C_COMBINED_MESSAGE)
	struct i2c_msg msg[1] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = len,
			.buf = txbuf,
		},
	};
#endif // defined(SITRONIX_I2C_COMBINED_MESSAGE)

	if(txbuf == NULL)
		return -1;
#if defined(SITRONIX_I2C_COMBINED_MESSAGE)
	ret = i2c_transfer(client->adapter, &msg[0], 1);
#elif defined(SITRONIX_I2C_SINGLE_MESSAGE)
	ret = i2c_master_send(client, txbuf, len);
#endif // defined(SITRONIX_I2C_COMBINED_MESSAGE)
	if (ret < 0){
		printk("write 0x%x error (%d)\n", *txbuf, ret);
		return ret;
	}
	return 0;
}

static int sitronix_get_fw_revision(struct sitronix_ts_data *ts)
{
	int ret = 0;
	uint8_t buffer[4];
	printk("%s,line=%d\n", __FUNCTION__,__LINE__);

	ret = sitronix_i2c_read_bytes(ts->client, FIRMWARE_REVISION_3, buffer, 4);
	if (ret < 0){
		printk("read fw revision error (%d)\n", ret);
		return ret;
	}else{
		memcpy(ts->fw_revision, buffer, 4);
		printk("fw revision (hex) = %x %x %x %x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
	}
	return 0;
}
static int sitronix_get_max_touches(struct sitronix_ts_data *ts)
{
	int ret = 0;
	uint8_t buffer[1];
	printk("%s,line=%d\n", __FUNCTION__,__LINE__);

	ret = sitronix_i2c_read_bytes(ts->client, MAX_NUM_TOUCHES, buffer, 1);
	if (ret < 0){
		printk("read max touches error (%d)\n", ret);
		return ret;
	}else{
		ts->max_touches = buffer[0];
		if (ts->max_touches > SITRONIX_MAX_SUPPORTED_POINT)
			ts->max_touches = SITRONIX_MAX_SUPPORTED_POINT;
		printk("max touches = %d \n",ts->max_touches);
	}
	return 0;
}

static int sitronix_get_protocol_type(struct sitronix_ts_data *ts)
{
	int ret = 0;
	uint8_t buffer[1];
	printk("%s,line=%d\n", __FUNCTION__,__LINE__);

	if(ts->chip_id <= 3){
		ret = sitronix_i2c_read_bytes(ts->client, I2C_PROTOCOL, buffer, 1);
		if (ret < 0){
			printk("read i2c protocol error (%d)\n", ret);
			return ret;
		}else{
			ts->touch_protocol_type = buffer[0] & I2C_PROTOCOL_BMSK;
			printk("i2c protocol = %d \n", ts->touch_protocol_type);
			ts->sensing_mode = (buffer[0] & (ONE_D_SENSING_CONTROL_BMSK << ONE_D_SENSING_CONTROL_SHFT)) >> ONE_D_SENSING_CONTROL_SHFT;
			printk("sensing mode = %d \n", ts->sensing_mode);
		}
	}else{
		ts->touch_protocol_type = SITRONIX_A_TYPE;
		printk("i2c protocol = %d \n", ts->touch_protocol_type);
		ret = sitronix_i2c_read_bytes(ts->client, 0xf0, buffer, 1);
		if (ret < 0){
			printk("read sensing mode error (%d)\n", ret);
			return ret;
		}else{
			ts->sensing_mode = (buffer[0] & ONE_D_SENSING_CONTROL_BMSK);
			printk("sensing mode = %d \n", ts->sensing_mode);
		}
	}
	return 0;
}

static int sitronix_get_resolution(struct sitronix_ts_data *ts)
{
	/*
	int ret = 0;
	uint8_t buffer[4];
	int newResX = 800;
	int newResY = 480;

	buffer[0] = XY_RESOLUTION_HIGH;
	buffer[1] = (newResX>>8)<<4 | (newResY>>8);
	buffer[2] = (newResX&0xFF);
	buffer[3] = (newResY&0xFF);

	ret = sitronix_i2c_write_bytes(ts->client, buffer, 4);
	if (ret < 0){
		printk("set resolution error (%d)\n", ret);
		return ret;
	}else{
		ts->resolution_x = newResX;
		ts->resolution_y = newResY;
		printk("resolution = %d x %d\n", ts->resolution_x, ts->resolution_y);
	}
	*/
	int ret = 0;
	uint8_t buffer[4];
		printk("%s,line=%d\n", __FUNCTION__,__LINE__);
	ret = sitronix_i2c_read_bytes(ts->client, XY_RESOLUTION_HIGH, buffer, 3);
	if (ret < 0){
		printk("read resolution error (%d)\n", ret);
		return ret;
	}else{
		ts->resolution_x = ((buffer[0] & (X_RES_H_BMSK << X_RES_H_SHFT)) << 4) | buffer[1];
		ts->resolution_y = ((buffer[0] & Y_RES_H_BMSK) << 8) | buffer[2];
		printk("resolution = %d x %d\n", ts->resolution_x, ts->resolution_y);
	}
	return 0;

}

static int sitronix_ts_get_CHIP_ID(struct sitronix_ts_data *ts)
{
	int ret = 0;
	uint8_t buffer[3];
	printk("%s,line=%d\n", __FUNCTION__,__LINE__);

	DbgMsg("%s\n", __FUNCTION__);

	ret = sitronix_i2c_read_bytes(ts->client, CHIP_ID, buffer, 3);
	if (ret < 0){
		printk("read Chip ID error (%d)\n", ret);
		return ret;
	}else{
		if(buffer[0] == 0){
			if(buffer[1] + buffer[2] > 32)
				ts->chip_id = 2;
			else
				ts->chip_id = 0;
		}else
			ts->chip_id = buffer[0];
		ts->Num_X = buffer[1];
		ts->Num_Y = buffer[2];
		printk("Chip ID = %d\n", ts->chip_id);
		printk("Num_X = %d\n", ts->Num_X);
		printk("Num_Y = %d\n", ts->Num_Y);
	}

	return 0;
}

static int sitronix_ts_set_powerdown_bit(struct sitronix_ts_data *ts, int value)
{
	int ret = 0;
	uint8_t buffer[2];
	printk("%s,line=%d\n", __FUNCTION__,__LINE__);

	DbgMsg("%s, value = %d\n", __FUNCTION__, value);
	ret = sitronix_i2c_read_bytes(ts->client, DEVICE_CONTROL_REG, buffer, 1);
	if (ret < 0){
		printk("read device control status error (%d)\n", ret);
		return ret;
	}else{
		DbgMsg("dev status = %d \n", buffer[0]);
	}

	if(value == 0)
		buffer[1] = buffer[0] & 0xfd;
	else
		buffer[1] = buffer[0] | 0x2;

	buffer[0] = DEVICE_CONTROL_REG;
	ret = sitronix_i2c_write_bytes(ts->client, buffer, 2);
	if (ret < 0){
		printk("write power down error (%d)\n", ret);
		return ret;
	}

	return 0;
}

#if 0
static int sitronix_ts_disable_int(struct sitronix_ts_data *ts, uint8_t value)
{
	int ret = 0;
	uint8_t buffer[2];
	sitronix_i2c_protocol_map *i2c_ptcl;
	printk("%s,line=%d\n", __FUNCTION__,__LINE__);

	DbgMsg("%s, value = %d\n", __FUNCTION__, value);
	i2c_ptcl = (ts->chip_id > 3)? &sitronix_i2c_ptcl_v2 : &sitronix_i2c_ptcl_v1;

	ret = sitronix_i2c_read_bytes(ts->client, i2c_ptcl->dis_coord_flag.offset, buffer, 1);
	if (ret < 0){
		printk("read disable coord. flag error (%d)\n", ret);
		return ret;
	}

	buffer[1] = buffer[0] & ~(i2c_ptcl->dis_coord_flag.bmsk << i2c_ptcl->dis_coord_flag.shft);
	buffer[1] |= value;

	buffer[0] = i2c_ptcl->dis_coord_flag.offset;
	ret = sitronix_i2c_write_bytes(ts->client, buffer, 2);
	if (ret < 0){
		printk("write disable coord. flag error (%d)\n", ret);
		return ret;
	}
	return 0;
}
#endif
static int sitronix_ts_get_touch_info(struct sitronix_ts_data *ts)
{
	int ret = 0;
		printk("%s,line=%d\n", __FUNCTION__,__LINE__);
	ret = sitronix_get_resolution(ts);
	if(ret < 0)
		return ret;
	ret = sitronix_ts_get_CHIP_ID(ts);
	if(ret < 0)
		return ret;
	ret = sitronix_get_fw_revision(ts);
	if(ret < 0)
		return ret;
	ret = sitronix_get_protocol_type(ts);
	if(ret < 0)
		return ret;
	ret = sitronix_get_max_touches(ts);
	if(ret < 0)
		return ret;

	if((ts->fw_revision[0] == 0) && (ts->fw_revision[1] == 0)){
		if(ts->touch_protocol_type == SITRONIX_RESERVED_TYPE_0){
			ts->touch_protocol_type = SITRONIX_B_TYPE;
			printk("i2c protocol (revised) = %d \n", ts->touch_protocol_type);
		}
	}
	if(ts->touch_protocol_type == SITRONIX_A_TYPE)
		ts->pixel_length = PIXEL_DATA_LENGTH_A;
	else if(ts->touch_protocol_type == SITRONIX_B_TYPE){
		ts->pixel_length = PIXEL_DATA_LENGTH_B;
		ts->max_touches = 2;
		printk("max touches (revised) = %d \n", ts->max_touches);
	}

#ifdef SITRONIX_MONITOR_THREAD
	ts->RawCRC_enabled = 0;
	if(ts->chip_id > 3){
		ts->enable_monitor_thread = 1;
		ts->RawCRC_enabled = 1;
	}else if(ts->chip_id == 3){
		ts->enable_monitor_thread = 1;
		//if(((ts->fw_revision[2] << 8) | ts->fw_revision[3]) >= (9 << 8 | 3))
		if(((ts->fw_revision[2] << 8) | ts->fw_revision[3]) >= (6 << 8 | 3))
			ts->RawCRC_enabled = 1;
	}else
		ts->enable_monitor_thread = 0;
#endif // SITRONIX_MONITOR_THREAD

	return 0;
}

static int sitronix_ts_get_device_status(struct i2c_client *client, uint8_t *err_code, uint8_t *dev_status)
{
	int ret = 0;
	uint8_t buffer[8];
	printk("%s,line=%d\n",__FUNCTION__,__LINE__);
	DbgMsg("%s\n", __FUNCTION__);
	ret = sitronix_i2c_read_bytes(client, STATUS_REG, buffer, 8);
	if (ret < 0){
		printk("read status reg error (%d)\n", ret);
		return ret;
	}else{
		printk("status reg = %d \n", buffer[0]);
	}

	*err_code = (buffer[0] & 0xf0) >> 4;
	*dev_status = buffer[0] & 0xf;

	return 0;
}

#ifdef SITRONIX_IDENTIFY_ID
static int sitronix_ts_Enhance_Function_control(struct sitronix_ts_data *ts, uint8_t *value)
{
	int ret = 0;
	uint8_t buffer[1];

	DbgMsg("%s\n", __FUNCTION__);
	ret = sitronix_i2c_read_bytes(ts->client, 0xF0, buffer, 1);
	if (ret < 0){
		printk("read Enhance Functions status error (%d)\n", ret);
		return ret;
	}else{
		DbgMsg("Enhance Functions status = %d \n", buffer[0]);
	}

	*value = buffer[0] & 0x4;

	return 0;
}

static int sitronix_ts_FW_Bank_Select(struct sitronix_ts_data *ts, uint8_t value)
{
	int ret = 0;
	uint8_t buffer[2];

	DbgMsg("%s\n", __FUNCTION__);
	ret = sitronix_i2c_read_bytes(ts->client, 0xF1, buffer, 1);
	if (ret < 0){
		printk("read FW Bank Select status error (%d)\n", ret);
		return ret;
	}else{
		DbgMsg("FW Bank Select status = %d \n", buffer[0]);
	}

	buffer[1] = ((buffer[0] & 0xfc) | value);
	buffer[0] = 0xF1;
	ret = sitronix_i2c_write_bytes(ts->client, buffer, 2);
	if (ret < 0){
		printk("send FW Bank Select command error (%d)\n", ret);
		return ret;
	}

	return 0;
}

static int sitronix_get_id_info(struct sitronix_ts_data *ts, uint8_t *id_info)
{
	int ret = 0;
	uint8_t buffer[4];

	ret = sitronix_i2c_read_bytes(ts->client, 0x0C, buffer, 4);
	if (ret < 0){
		printk("read id info error (%d)\n", ret);
		return ret;
	}else{
		memcpy(id_info, buffer, 4);
	}
	return 0;
}

static int sitronix_ts_identify(struct sitronix_ts_data *ts)
{
	int ret = 0;
	uint8_t id[4];
	uint8_t Enhance_Function = 0;

	ret = sitronix_ts_FW_Bank_Select(ts, 1);
	if(ret < 0)
		return ret;
	ret = sitronix_ts_Enhance_Function_control(ts, &Enhance_Function);
	if(ret < 0)
		return ret;
	if(Enhance_Function == 0x4){
		ret = sitronix_get_id_info(ts, &id[0]);
		if(ret < 0)
			return ret;
		printk("id (hex) = %x %x %x %x\n", id[0], id[1], id[2], id[3]);
		if((id[0] == 1)&&(id[1] == 2)&&(id[2] == 0xb)&&(id[3] == 1)){
			return 0;
		}else{
			printk("Error: It is not Sitronix IC\n");
			return -1;
		}
	}else{
		printk("Error: Can not get ID of Sitronix IC\n");
		return -1;
	}
}
#endif // SITRONIX_IDENTIFY_ID

#ifdef SITRONIX_MONITOR_THREAD
static int sitronix_set_raw_data_type(struct sitronix_ts_data *ts)
{
	int ret = 0;
	uint8_t buffer[2] = {0};

	ret = sitronix_i2c_read_bytes(ts->client, DEVICE_CONTROL_REG, buffer, 1);
	if (ret < 0){
		DbgMsg("read DEVICE_CONTROL_REG error (%d)\n", ret);
		return ret;
	}else{
		DbgMsg("read DEVICE_CONTROL_REG status = %d \n", buffer[0]);
	}
	if(ts->sensing_mode == SENSING_BOTH_NOT){
		buffer[1] = ((buffer[0] & 0xf3) | (0x01 << 2));
	}else{
		buffer[1] = (buffer[0] & 0xf3);
	}
	buffer[0] = DEVICE_CONTROL_REG;
	ret = sitronix_i2c_write_bytes(ts->client, buffer, 2);
	if (ret < 0){
		DbgMsg("write DEVICE_CONTROL_REG error (%d)\n", ret);
		return ret;
	}
	return 0;
}

static int sitronix_ts_monitor_thread(void *data)
{
	int ret = 0;
	uint8_t buffer[4] = { 0, 0, 0, 0 };
	int result = 0;
	int once = 1;
	uint8_t raw_data_ofs = 0;

	DbgMsg("%s:\n", __FUNCTION__);

	printk("delay %d ms\n", sitronix_ts_delay_monitor_thread_start);
	msleep(sitronix_ts_delay_monitor_thread_start);
	while(!kthread_should_stop()){
		DbgMsg("%s:\n", "Sitronix_ts_monitoring 2222");
		if(atomic_read(&iMonitorThreadPostpone)){
				atomic_set(&iMonitorThreadPostpone,0);
		}else{
			if(once == 1){
				ret = sitronix_set_raw_data_type(&sitronix_ts_gpts);
				if (ret < 0)
					goto exit_i2c_invalid;

				if((sitronix_ts_gpts.sensing_mode == SENSING_BOTH) || (sitronix_ts_gpts.sensing_mode == SENSING_X_ONLY)){
					raw_data_ofs = 0x40;
				}else if(sitronix_ts_gpts.sensing_mode == SENSING_Y_ONLY){
					raw_data_ofs = 0x40 + sitronix_ts_gpts.Num_X * 2;
				}else{
					raw_data_ofs = 0x40;
				}

				once = 0;
			}
			if(raw_data_ofs != 0x40){
				ret = sitronix_i2c_read_bytes(sitronix_ts_gpts.client, 0x40, buffer, 1);
				if (ret < 0){
					DbgMsg("read raw data error (%d)\n", ret);
					result = 0;
					goto exit_i2c_invalid;
				}
			}
			ret = sitronix_i2c_read_bytes(sitronix_ts_gpts.client, raw_data_ofs, buffer, 4);
			if (ret < 0){
				DbgMsg("read raw data error (%d)\n", ret);
				result = 0;
				goto exit_i2c_invalid;
			}else{
				DbgMsg("%dD data h%x-%x = 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", (sitronix_ts_gpts.sensing_mode == SENSING_BOTH_NOT ? 2:1), raw_data_ofs, raw_data_ofs + 3, buffer[0], buffer[1], buffer[2], buffer[3]);
				//printk("%dD data h%x-%x = 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", (sitronix_ts_gpts.sensing_mode == SENSING_BOTH_NOT ? 2:1), raw_data_ofs, raw_data_ofs + 3, buffer[0], buffer[1], buffer[2], buffer[3]);
				result = 1;
				if ((PreCheckData[0] == buffer[0]) && (PreCheckData[1] == buffer[1]) &&
				(PreCheckData[2] == buffer[2]) && (PreCheckData[3] == buffer[3]))
					StatusCheckCount ++;
				else
					StatusCheckCount =0;
				PreCheckData[0] = buffer[0];
				PreCheckData[1] = buffer[1];
				PreCheckData[2] = buffer[2];
				PreCheckData[3] = buffer[3];
				if (3 <= StatusCheckCount){
					DbgMsg("IC Status doesn't update! \n");
					result = -1;
					StatusCheckCount = 0;
				}
			}
			if (-1 == result){
				printk("Chip abnormal, reset it!\n");
				sitronix_ts_reset_ic();
				i2cErrorCount = 0;
				StatusCheckCount = 0;
				if(sitronix_ts_gpts.RawCRC_enabled == 0){
					ret = sitronix_set_raw_data_type(&sitronix_ts_gpts);
					if (ret < 0)
						goto exit_i2c_invalid;
				}
			}
exit_i2c_invalid:
			if(0 == result){
				i2cErrorCount ++;
				if ((2 <= i2cErrorCount)){
					printk("I2C abnormal, reset it!\n");
					sitronix_ts_reset_ic();
					if(sitronix_ts_gpts.RawCRC_enabled == 0)
						sitronix_set_raw_data_type(&sitronix_ts_gpts);
					i2cErrorCount = 0;
					StatusCheckCount = 0;
				}
			}else
				i2cErrorCount = 0;
		}
		msleep(gMonitorThreadSleepInterval);
	}
	DbgMsg("%s exit\n", __FUNCTION__);
	return 0;
}

static int sitronix_ts_monitor_thread_v2(void *data)
{
	int ret = 0;
	uint8_t buffer[1] = {0};
	int result = 0;

	DbgMsg("%s:\n", __FUNCTION__);

	printk("delay %d ms\n", sitronix_ts_delay_monitor_thread_start);
	msleep(sitronix_ts_delay_monitor_thread_start);
	while(!kthread_should_stop()){
		DbgMsg("%s:\n", "Sitronix_ts_monitoring");
		if(atomic_read(&iMonitorThreadPostpone)){
				atomic_set(&iMonitorThreadPostpone,0);
		}else{
			ret = sitronix_i2c_read_bytes(sitronix_ts_gpts.client, 0xA, buffer, 1);
			if (ret < 0){
				DbgMsg("read Raw CRC error (%d)\n", ret);
				result = 0;
				goto exit_i2c_invalid;
			}else{
				DbgMsg("Raw CRC = 0x%02x\n", buffer[0]);
				//printk("Raw CRC = 0x%02x\n", buffer[0]);
				result = 1;
				if (PreCheckData[0] == buffer[0])
					StatusCheckCount ++;
				else
					StatusCheckCount =0;
				PreCheckData[0] = buffer[0];
				if (3 <= StatusCheckCount){
					DbgMsg("IC Status doesn't update! \n");
					result = -1;
					StatusCheckCount = 0;
				}
			}
			if (-1 == result){
				printk("Chip abnormal, reset it!\n");
				sitronix_ts_reset_ic();
				i2cErrorCount = 0;
				StatusCheckCount = 0;
			}
exit_i2c_invalid:
			if(0 == result){
				i2cErrorCount ++;
				if ((2 <= i2cErrorCount)){
					printk("I2C abnormal, reset it!\n");
					sitronix_ts_reset_ic();
					i2cErrorCount = 0;
					StatusCheckCount = 0;
				}
			}else
				i2cErrorCount = 0;
		}
		msleep(gMonitorThreadSleepInterval);
	}
	DbgMsg("%s exit\n", __FUNCTION__);
	return 0;
}
#endif // SITRONIX_MONITOR_THREAD

static inline void sitronix_ts_pen_down(struct input_dev *input_dev, int id, u16 x, u16 y)
{
	//printk("%s,line=%d\n", __FUNCTION__,__LINE__);


	input_report_abs(input_dev,  ABS_MT_TRACKING_ID, id+1);
	input_report_abs(input_dev,  ABS_MT_POSITION_X, x);
	input_report_abs(input_dev,  ABS_MT_POSITION_Y, y);
	input_report_abs(input_dev,  ABS_X, x);
	input_report_abs(input_dev,  ABS_Y, y);
	input_report_abs(input_dev,  ABS_MT_TOUCH_MAJOR, 255);
	input_report_abs(input_dev,  ABS_MT_WIDTH_MAJOR, 255);
	input_report_abs(input_dev, ABS_MT_PRESSURE, 255);

	input_mt_sync(input_dev);

	DbgMsg("sitronix: [%d](%d, %d)+\n", id, x, y);
}

static inline void sitronix_ts_pen_up(struct input_dev *input_dev, int id)
{
	//printk("%s,line=%d\n", __FUNCTION__,__LINE__);


	input_report_abs(input_dev,  ABS_MT_TRACKING_ID, id);
	input_report_abs(input_dev,  ABS_MT_TOUCH_MAJOR, 0);
	input_report_abs(input_dev,  ABS_MT_WIDTH_MAJOR, 0);
	input_report_abs(input_dev, ABS_MT_PRESSURE, 0);

	DbgMsg("[%d]-\n", id);
}

static inline void sitronix_ts_handle_sensor_key(struct input_dev *input_dev, struct sitronix_sensor_key_t *key_array, char *pre_key_status, char cur_key_status, int key_count)
{

	int i = 0;
	for(i = 0; i < key_count; i++){
		if(cur_key_status & (1 << i)){
			DbgMsg("lpz sensor key[%d] down\n", i);
			printk("kkk down now key %d \n",cur_key_status);
			input_report_key(input_dev, key_array[i].code, 1);

			input_sync(input_dev);
		}else{
			if(*pre_key_status & (1 << i)){
				//printk("kkk up now key %d \n",cur_key_status);
				DbgMsg("lpz sensor key[%d] up\n", i);
				input_report_key(input_dev, key_array[i].code, 0);
				input_sync(input_dev);
			}
		}
	}
	*pre_key_status = cur_key_status;
}

#ifdef SITRONIX_AA_KEY
static inline void sitronix_ts_handle_aa_key(struct input_dev *input_dev, struct sitronix_AA_key *key_array, char *pre_key_status, char cur_key_status, int key_count)
{

	int i = 0;
	for(i = 0; i < key_count; i++){
		if(cur_key_status & (1 << i)){
			DbgMsg("aa key[%d] down\n", i);
			input_report_key(input_dev, key_array[i].code, 1);
			input_sync(input_dev);
		}else{
			if(*pre_key_status & (1 << i)){
				DbgMsg("aa key[%d] up\n", i);
				input_report_key(input_dev, key_array[i].code, 0);
				input_sync(input_dev);
			}
		}
	}
	*pre_key_status = cur_key_status;
}
#endif // SITRONIX_AA_KEY

#ifdef SITRONIX_GESTURE

static void sitronix_gesture_func(struct input_dev *input_dev,int id)
{
	if(id == G_PALM)
	{
		printk("Gesture for Palm to suspend \n");
		input_report_key(input_dev,KEY_POWER,1);// KEY_LEFT, 1);
		input_sync(input_dev);
		input_report_key(input_dev, KEY_POWER, 0);
		input_sync(input_dev);
	}
}



#endif

#ifdef SITRONIX_SMART_WAKE_UP
static int swk_flag = 0;
static void sitronix_swk_set_swk_enable(struct sitronix_ts_data *ts)
{

	int ret = 0;
	unsigned char buffer[2] = {0};
	ret = sitronix_i2c_read_bytes(ts->client, MISC_CONTROL, buffer, 1);
	if(ret == 0)
	{
		buffer[1] = buffer[0] | 0x80;
		buffer[0] = MISC_CONTROL;
		sitronix_i2c_write_bytes(ts->client, buffer, 2);

		msleep(500);
	}


}
static void sitronix_swk_func(struct input_dev *input_dev, int id)
{
	if(id == DOUBLE_CLICK || id == SINGLE_CLICK)
	{
		if(swk_flag == 1)
		{
			//do wake up here
			printk("Smark Wake Up by Double click! \n");
			input_report_key(input_dev, KEY_POWER, 1);
			input_sync(input_dev);
			input_report_key(input_dev, KEY_POWER, 0);
			input_sync(input_dev);
			swk_flag = 0;
		}
	}
	else if(id == TOP_TO_DOWN_SLIDE)
	{
		printk("Smark Wake Up by TOP_TO_DOWN_SLIDE \n");
		//do wake up here
	}
	else if(id == DOWN_TO_UP_SLIDE)
	{
		printk("Smark Wake Up by DOWN_TO_UP_SLIDE \n");
		//do wake up here
			}
	else if(id == LEFT_TO_RIGHT_SLIDE)
	{
		printk("Smark Wake Up by LEFT_TO_RIGHT_SLIDE \n");
		//do wake up here
	}
	else if(id == RIGHT_TO_LEFT_SLIDE)
	{
		printk("Smark Wake Up by RIGHT_TO_LEFT_SLIDE \n");
		//do wake up here
	}
}
#endif	//SITRONIX_SMART_WAKE_UP

static void sitronix_ts_work_func(struct work_struct *work)
{
	int i;
#ifdef SITRONIX_AA_KEY
	int j;
	char aa_key_status = 0;
#endif // SITRONIX_AA_KEY
	int ret;


#ifndef SITRONIX_INT_POLLING_MODE
	struct sitronix_ts_data *ts = container_of(work, struct sitronix_ts_data, work);
#else
	struct sitronix_ts_data *ts = container_of(to_delayed_work(work), struct sitronix_ts_data, work);
#endif // SITRONIX_INT_POLLING_MODE
	u16 x, y;
	uint8_t buffer[1+ SITRONIX_MAX_SUPPORTED_POINT * PIXEL_DATA_LENGTH_A] = {0};
	uint8_t PixelCount = 0;

	DbgMsg("%s\n",  __FUNCTION__);
	//printk("%s,line=%d\n,use_irq=%d,irq_num=%d,",__FUNCTION__,__LINE__,ts->use_irq,ts->client->irq);
	atomic_set(&sitronix_ts_in_int, 1);

#ifdef SITRONIX_GESTURE
	if(!ts->suspend_state)
	{
		ret = sitronix_i2c_read_bytes(ts->client, FINGERS, buffer, 1);
		printk("SITRONIX_GESTURE ret:%d ,value:0x%X\n",ret,buffer[0]);
		buffer[0] &= 0xF;
		if((ret == 0 && buffer[0] == G_PALM))
		{
			sitronix_gesture_func(ts->keyevent_input,buffer[0]);
	goto exit_invalid_data;
		}
	}
#endif

#ifdef	SITRONIX_SMART_WAKE_UP
	if(ts->suspend_state){
//2.9.15 petitk add
		ret = sitronix_i2c_read_bytes(ts->client, SMART_WAKE_UP_REG, buffer, 1);
		if(ret ==0 && buffer[0] !=SWK_NO)
		{
			sitronix_swk_func(ts->keyevent_input, buffer[0]);
			goto exit_invalid_data;
		}
	}
#endif	//SITRONIX_SMART_WAKE_UP

	ret = sitronix_i2c_read_bytes(ts->client, KEYS_REG, buffer, 1 + ts->max_touches * ts->pixel_length);
	if (ret < 0) {
		printk("read finger error (%d)\n", ret);
		i2cErrorCount++;
		goto exit_invalid_data;
	}

	for(i = 0; i < ts->max_touches; i++){
		if(buffer[1 + i * ts->pixel_length + XY_COORD_H] & 0x80){

			x = (u16)(buffer[1 + i * ts->pixel_length + XY_COORD_H] & 0x70) << 4 | buffer[1 + i * ts->pixel_length + X_COORD_L];
			y = (u16)(buffer[1 + i * ts->pixel_length + XY_COORD_H] & 0x07) << 8 | buffer[1 + i * ts->pixel_length + Y_COORD_L];
			//printk("%s:line=%d,x=%d,y=%d\n",__FUNCTION__,__LINE__,x,y);
#ifndef SITRONIX_AA_KEY
			PixelCount++;
			//if (SpToAp_CheckState() == SP_STATE_IN_GETPIN)
		//		SpToAp_InsertTpPixel(TP_PIXEL_TYPE_DOWN, i, x, y);
			//else
				sitronix_ts_pen_down(ts->input_dev, i, x, y);
#else
#ifdef SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
			if(y < SITRONIX_TOUCH_RESOLUTION_Y){
#else
			if(y < (ts->resolution_y - ts->resolution_y / SCALE_KEY_HIGH_Y)){
#endif // SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
				PixelCount++;
				sitronix_ts_pen_down(ts->input_dev, i, x, y);
				//DbgMsg("AREA_DISPLAY\n");
			}else{
				for(j = 0; j < (sizeof(sitronix_aa_key_array)/sizeof(struct sitronix_AA_key)); j++){
					if((x >= sitronix_aa_key_array[j].x_low) &&
					(x <= sitronix_aa_key_array[j].x_high) &&
					(y >= sitronix_aa_key_array[j].y_low) &&
					(y <= sitronix_aa_key_array[j].y_high)){
						aa_key_status |= (1 << j);
						//DbgMsg("AREA_KEY [%d]\n", j);
						break;
					}
				}
			}
#endif // SITRONIX_AA_KEY
		}else{
			//sitronix_ts_pen_up(ts->input_dev, i);
		}
	}
	if(PixelCount == 0)
		sitronix_ts_pen_up(ts->input_dev, 0);


	input_report_key(ts->input_dev, BTN_TOUCH, PixelCount > 0);
	input_sync(ts->input_dev);

	sitronix_ts_handle_sensor_key(ts->keyevent_input, sitronix_sensor_key_array, &sitronix_sensor_key_status, buffer[0], (sizeof(sitronix_sensor_key_array)/sizeof(struct sitronix_sensor_key_t)));
#ifdef SITRONIX_AA_KEY
	sitronix_ts_handle_aa_key(ts->keyevent_input, sitronix_aa_key_array, &sitronix_aa_key_status, aa_key_status, (sizeof(sitronix_aa_key_array)/sizeof(struct sitronix_AA_key)));
#endif // SITRONIX_AA_KEY

exit_invalid_data:
#ifdef SITRONIX_INT_POLLING_MODE
	if(PixelCount > 0){
#ifdef SITRONIX_MONITOR_THREAD
		if(ts->enable_monitor_thread == 1){
			atomic_set(&iMonitorThreadPostpone,1);
		}
#endif // SITRONIX_MONITOR_THREAD
		schedule_delayed_work(&ts->work, msecs_to_jiffies(INT_POLLING_MODE_INTERVAL));
	}else{
#ifdef CONFIG_HARDIRQS_SW_RESEND
		printk("Please not set HARDIRQS_SW_RESEND to prevent kernel from sending SW IRQ\n");
#endif // CONFIG_HARDIRQS_SW_RESEND
		if (ts->use_irq){
			atomic_set(&sitronix_ts_irq_on, 1);
			enable_irq(ts->client->irq);
		}
	}
#endif // SITRONIX_INT_POLLING_MODE
#if defined(SITRONIX_LEVEL_TRIGGERED)
	if (ts->use_irq){
		atomic_set(&sitronix_ts_irq_on, 1);
		enable_irq(ts->client->irq);
	}
#endif // defined(SITRONIX_LEVEL_TRIGGERED)
	if ((2 <= i2cErrorCount)){
		printk("I2C abnormal in work_func(), reset it!\n");
		sitronix_ts_reset_ic();
		i2cErrorCount = 0;
#ifdef SITRONIX_MONITOR_THREAD
		if(ts->enable_monitor_thread == 1){
			StatusCheckCount = 0;
			if(ts->RawCRC_enabled == 0)
				sitronix_set_raw_data_type(&sitronix_ts_gpts);
		}
#endif // SITRONIX_MONITOR_THREAD
	}
	atomic_set(&sitronix_ts_in_int, 0);

}

static irqreturn_t sitronix_ts_irq_handler(int irq, void *dev_id)
{
	struct sitronix_ts_data *ts = dev_id;
	//printk("%s,line=%d\n",__FUNCTION__,__LINE__);
	DbgMsg("%s\n", __FUNCTION__);
	atomic_set(&sitronix_ts_in_int, 1);
#if defined(SITRONIX_LEVEL_TRIGGERED) || defined(SITRONIX_INT_POLLING_MODE)
	atomic_set(&sitronix_ts_irq_on, 0);
	disable_irq_nosync(ts->client->irq);
#endif // defined(SITRONIX_LEVEL_TRIGGERED) || defined(SITRONIX_INT_POLLING_MODE)
#ifdef SITRONIX_MONITOR_THREAD
	if(ts->enable_monitor_thread == 1){
		atomic_set(&iMonitorThreadPostpone,1);
	}
#endif // SITRONIX_MONITOR_THREAD
#ifndef SITRONIX_INT_POLLING_MODE
	schedule_work(&ts->work);
#else
	schedule_delayed_work(&ts->work, msecs_to_jiffies(0));
#endif // SITRONIX_INT_POLLING_MODE
	return IRQ_HANDLED;
}

#ifdef SITRONIX_SYSFS
static ssize_t sitronix_ts_reprobe_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	printk("sitronix_ts_reprobe_store!!!!!\n");
	sitronix_ts_sysfs_using = true;
	sitronix_ts_reprobe();
	sitronix_ts_sysfs_using = false;
	return count;
}

static DEVICE_ATTR(reprobe, S_IWUSR | S_IWGRP, NULL, sitronix_ts_reprobe_store);

static struct attribute *sitronix_ts_attrs_v0[] = {
	&dev_attr_reprobe.attr,
	NULL,
};

static struct attribute_group sitronix_ts_attr_group_v0 = {
	.name = "sitronix_ts_attrs",
	.attrs = sitronix_ts_attrs_v0,
};

static int sitronix_ts_create_sysfs_entry(struct i2c_client *client)
{
	int err;

	err = sysfs_create_group(&(client->dev.kobj), &sitronix_ts_attr_group_v0);
	if (err) {
		dev_warn(&client->dev, "%s(%u): sysfs_create_group() failed!\n", __FUNCTION__, __LINE__);
	}
	return err;
}

static void sitronix_ts_destroy_sysfs_entry(struct i2c_client *client)
{
	sysfs_remove_group(&(client->dev.kobj), &sitronix_ts_attr_group_v0);

	return;
}
#endif // SITRONIX_SYSFS

/*******************************************************
Function:
	Disable IRQ Function.

Input:
	data:	i2c client private struct.

Output:
	None.
*******************************************************/
void sitronix_irq_disable(struct sitronix_ts_data *data)
{
        unsigned long irqflags;
	int ret = 0;

        DbgMsg("%s ---start!---\n", __func__);
        spin_lock_irqsave(&data->irq_lock, irqflags);
        if (!data->irq_is_disable) {
               data->irq_is_disable = 1;
               //ret = input_set_int_enable(&(config_info.input_type), 0);
			   if (ret < 0)
				  ErrorMsg("%s irq disable failed\n", __func__);
        }
        spin_unlock_irqrestore(&data->irq_lock, irqflags);
}

/*******************************************************
Function:
	Disable IRQ Function.

Input:
	data:	i2c client private struct.

Output:
	None.
*******************************************************/
void sitronix_irq_enable(struct sitronix_ts_data *data)
{
        unsigned long irqflags = 0;
	int ret = 0;

        DbgMsg("%s ---start!---\n", __func__);

        spin_lock_irqsave(&data->irq_lock, irqflags);
        if (data->irq_is_disable) {
                data->irq_is_disable = 0;
                //ret = input_set_int_enable(&(config_info.input_type), 1);
				if (ret < 0)
					ErrorMsg("%s irq enable failed\n", __func__);
        }
        spin_unlock_irqrestore(&data->irq_lock, irqflags);
}


#if (CTP_AP_SP_SYNC_WAY != CTP_AP_SP_SYNC_NONE)
int sitronix_touch_reinit(void)
{
	if(gSitronixPtr == NULL)
		return -1;
	 printk("%s\n",__func__);
	 sitronix_irq_enable(gSitronixPtr);
	 return 0;
}

int sitronix_touch_uninit(void)
{
	if(gSitronixPtr == NULL)
		return -1;
	 printk("%s\n",__func__);
	 sitronix_irq_disable(gSitronixPtr);
	 while(cancel_work_sync(&gSitronixPtr->work));
        //flush_workqueue(sitronix_wq);
	return 0;
}
#endif

#if  (CTP_AP_SP_SYNC_WAY & CTP_AP_SP_SYNC_GPIO)
#define CTP_SYNC_IO_OUT	1
#define CTP_SYNC_IO_IN		0
#if 0

static int ap_sp_sync_gpio(struct gpio_config gpio, int direction, int value)
{
	//int status = 0;
	int ret = 0;

	if(0 != gpio_request(gpio.gpio, "ctp_sync_io")) {
		ErrorMsg("ap_sp_sync_gpio gpio_request is failed\n");
		return -1;
	}

	if(CTP_SYNC_IO_OUT == direction)
	{
		if (0 != gpio_direction_output(gpio.gpio, value)) {
			ErrorMsg(KERN_ERR "ap_sp_sync_gpio gpio set err!");
			return -1;
		}
	}
	else if(CTP_SYNC_IO_IN == direction)
	{
		if (0 != gpio_direction_input(gpio.gpio)) {
			ErrorMsg(KERN_ERR "ap_sp_sync_gpio gpio set err!");
			return -1;
		}
		ret =  __gpio_get_value(gpio.gpio);
	}

	gpio_free(gpio.gpio);

	return ret;
}
#endif

static void sitronix_io_sync_work_func(struct work_struct *work)
{
	int pin_val = 0xff;
	unsigned long irqflags;
	printk("%s:line=%d,",__FUNCTION__,__LINE__);
	// pin_val = ap_sp_sync_gpio(sitronix_sync_io,CTP_SYNC_IO_IN,0);
	 //printk("P:%d\n",pin_val);
	 if(0 == pin_val)
	 {
		spin_lock_irqsave(&ctp_sync_lock,irqflags);
		ctp_sync_pulse_count++;
		if(1 == ctp_sync_io_last_status)
		{
			ctp_sync_io_last_status = 0;
		}
		spin_unlock_irqrestore(&ctp_sync_lock,irqflags);

	 }
	else if(1 == pin_val)
	{
		if(!ctp_sync_io_last_status)
		{
			printk("C:%d\n",ctp_sync_pulse_count);
			if((ctp_sync_pulse_count > 1) && (ctp_sync_pulse_count < 4)) //tp switch to sp siginal
			{
				printk("U\n");
				sitronix_touch_uninit();//\B4?\AF\CA\FD\BB\E1\D7\E8\C8\FB\D0\E8?\D4?\AA?\B8\F6\B9\A4\D7\F7\B6\D3\C1\D0?
			}
			else if((ctp_sync_pulse_count > 3) && (ctp_sync_pulse_count < 6)) //tp switch to ap siginal
			{
				printk("R\n");
				sitronix_touch_reinit();
			}
		}
		spin_lock_irqsave(&ctp_sync_lock,irqflags);
		ctp_sync_io_last_status = 1;
		ctp_sync_pulse_count = 0;
		spin_unlock_irqrestore(&ctp_sync_lock,irqflags);

	}

}

void ctp_sync_timer_func(unsigned long data)
{
	printk("%s:line=%d,",__FUNCTION__,__LINE__);

	queue_work(sitronix_io_sync_workqueue, &gSitronixPtr->sitronix_io_sync_work);

	mod_timer(&ctp_sync_timer,jiffies+HZ/50);
}
#endif

#define SITRONIX_COORDS_ARR_SIZE	4
#define MAX_BUTTONS		4
#define PROP_NAME_SIZE		24


static int sitronix_ts_get_dt_coords(struct device *dev, char *name,
				   struct sitronix_i2c_touch_platform_data *pdata)
{
	struct property *prop;
	struct device_node *np = dev->of_node;
	int rc;
	u32 coords[SITRONIX_COORDS_ARR_SIZE];
	printk("%s,%d\n",__FUNCTION__,__LINE__);

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	rc = of_property_read_u32_array(np, name, coords,
					SITRONIX_COORDS_ARR_SIZE);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read %s\n", name);
		return rc;
	}

	if (!strcmp(name, "sitronix,panel-coords")) {
		pdata->panel_minx = coords[0];
		pdata->panel_miny = coords[1];
		pdata->panel_maxx = coords[2];
		pdata->panel_maxy = coords[3];
		printk("%s:panel-coords,minx=%d,miny=%d,maxx=%d,maxy=%d\n",__FUNCTION__,\
				pdata->panel_minx,pdata->panel_miny,pdata->panel_maxx,pdata->panel_maxy);
	} else if (!strcmp(name, "sitronix,display-coords")) {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
		printk("%s:display-coords,minx=%d,miny=%d,maxx=%d,maxy=%d\n",__FUNCTION__,\
				pdata->panel_minx,pdata->panel_miny,pdata->panel_maxx,pdata->panel_maxy);
	} else {
		dev_err(dev, "unsupported property %s\n", name);
		return -EINVAL;
	}

	return 0;
}
#if 1

static int sitronix_parse_dt(struct device *dev,
			   struct sitronix_i2c_touch_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	struct property *prop;
	//u32 temp_val, num_buttons;
	//u32 button_map[MAX_BUTTONS];
	//char prop_name[PROP_NAME_SIZE];
	//int i, read_cfg_num;

	printk("%s,%d\n",__FUNCTION__,__LINE__);

	pdata->name = "sitronix";
	rc = sitronix_ts_get_dt_coords(dev, "sitronix,panel-coords", pdata);
	if (rc && (rc != -EINVAL))
		return rc;

	rc = sitronix_ts_get_dt_coords(dev, "sitronix,display-coords", pdata);
	if (rc)
		return rc;

	pdata->i2c_pull_up = of_property_read_bool(np, "sitronix,i2c-pull-up");

	pdata->no_force_update = of_property_read_bool(np,
						       "sitronix,no-force-update");

	pdata->enable_power_off = of_property_read_bool(np,
							"sitronix,enable-power-off");
	pdata->fw_vkey_support = of_property_read_bool(np,
						       "sitronix,fw-vkey-support");
	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "sitronix,reset-gpios",
						    0,
						    &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
		return pdata->reset_gpio;

	pdata->irq_gpio = of_get_named_gpio_flags(np, "sitronix,interrupt-gpios",
						  0, &pdata->irq_gpio_flags);
	printk("%s,pdata->irq_gpio=%d\n",__FUNCTION__,pdata->irq_gpio);
	if (pdata->irq_gpio < 0)
		return pdata->irq_gpio;

	rc = of_property_read_string(np, "sitronix,product-id",
				     &pdata->product_id);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Failed to parse product_id.");
		return -EINVAL;
	}

	rc = of_property_read_u32(np, "sitronix,num_max_touches",
				  &pdata->num_max_touches);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Failed to parse num_max_touches.");
		return -EINVAL;
	}

	rc = of_property_read_u32(np, "sitronix,soft_rst_dly",
				  &pdata->soft_rst_dly);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Failed to parse soft reset delay.");
		return -EINVAL;
	}

	rc = of_property_read_string(np, "sitronix,fw_name", &pdata->fw_name);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Failed to parse firmware name.\n");
		return -EINVAL;
	}

	prop = of_find_property(np, "sitronix,button-map", NULL);
	if (prop) {
		pdata->num_button = prop->length / sizeof(u32);
		if (pdata->num_button > MAX_BUTTONS)
			return -EINVAL;

		rc = of_property_read_u32_array(np,
						"sitronix,button-map", pdata->button_map, pdata->num_button);
		if (rc) {
			dev_err(dev, "Unable to read key codes\n");
			return rc;
		}
	}

	return 0;
}
#endif




static int sitronix_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i;
	int ret = 0;
	uint16_t max_x = 0, max_y = 0;
	uint8_t err_code = 0;
	uint8_t dev_status = 0;
	uint8_t rc;
	struct sitronix_i2c_touch_platform_data *pdata;
	//struct sitronix_ts_data *sitronix_ts_gpts;

	printk("%s,%d\n",__FUNCTION__,__LINE__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	pdata = devm_kzalloc(&client->dev,
						 sizeof(struct sitronix_i2c_touch_platform_data),
						 GFP_KERNEL);
	if (!pdata)
	{
		dev_err(&client->dev,"GTP Failed to allocate memory for pdata\n");
		return -ENOMEM;
	}
	ret = sitronix_parse_dt(&client->dev, pdata);
	if (ret)
		return ret;


	sitronix_ts_gpts.irq_gpio = pdata->irq_gpio;
	sitronix_ts_gpts.reset_gpio = pdata->reset_gpio;
#if 0//for touchscreen reset
	sitronix_ts_reset_ic();
#else
	printk("%s,line=%d,pdata->reset_gpio =%d\n", __FUNCTION__,__LINE__,pdata->reset_gpio);
	rc = gpio_request(pdata->reset_gpio, "reset_gpio");
	//if (rc)
		//ret rc;
	gpio_direction_output(sitronix_ts_gpts.reset_gpio, 0);
	msleep(3);
	gpio_direction_output(sitronix_ts_gpts.reset_gpio, 1);
	msleep(150);
	#if 0//for reset pin test
	for(i = 0;i<10;i++)
	{
	    gpio_direction_output(sitronix_ts_gpts.reset_gpio, 0);
	    usleep(3);
		gpio_direction_output(sitronix_ts_gpts.reset_gpio, 1);
		usleep(3);
	}
	#endif
	//rc = gpio_request(pdata->reset_gpio, "reset_gpio");
	//if (rc)
		//ret rc;
	//gpio_direction_output(pdata->reset_gpio, 0);
	//msleep(3);
	//gpio_direction_output(pdata->reset_gpio, 1);
	//msleep(150);
#endif

	sitronix_ts_gpts.client = client;
	#ifdef ST_UPGRADE_FIRMWARE
	#ifdef ST_FIREWARE_FILE
		kthread_run(st_upgrade_fw, "Sitronix", "sitronix_update");
	#else
		st_upgrade_fw();
	#endif //ST_FIREWARE_FILE

	#endif //ST_UPGRADE_FIRMWARE


	if(((ret = sitronix_ts_get_device_status(client, &err_code, &dev_status)) < 0) || (dev_status == 0x6) || ((err_code == 0x8)&&(dev_status == 0x0))){
		if((dev_status == 0x6) || ((err_code == 0x8)&&(dev_status == 0x0))){
			sitronix_ts_gpts.client = client;
		}
		ret = -EPERM;
		goto err_device_info_error;
	}
	sitronix_ts_gpts.client = client;
	sitronix_ts_gpts.suspend_state = 0;
	i2c_set_clientdata(client, &sitronix_ts_gpts);
	if((ret = sitronix_ts_get_touch_info(&sitronix_ts_gpts)) < 0)
		goto err_device_info_error;


#if 0//for touchscreen irq
printk("%s:pdata->irq_gpio=%d\n",__FUNCTION__,pdata->irq_gpio);
client->irq=pdata->irq_gpio;
printk("gpio num:%d, irq num:%d\n",CTP_IRQ_NUMBER,client->irq);
#else

#if 0
ret = gpio_request(pdata->irq_gpio, "GTP_INT_IRQ");
if (ret < 0)
{
	GTP_ERROR("Failed to request GPIO:%d, ERRNO:%d",
			  (s32) pdata->irq_gpio, ret);
	ret = -ENODEV;
}
else
{
	printk("%s:pdata->irq_gpio=%d\n",__FUNCTION__,pdata->irq_gpio);
	ret = gpio_direction_input(pdata->irq_gpio);
	if (ret < 0)
		gpio_free(pdata->irq_gpio);
	client->irq = gpio_to_irq(pdata->irq_gpio);
}
printk("gpio num:%d, irq num:%d\n",CTP_IRQ_NUMBER,client->irq);

#else
ret = gpio_request(sitronix_ts_gpts.irq_gpio, "GTP_INT_IRQ");
if (ret < 0)
{
	GTP_ERROR("Failed to request GPIO:%d, ERRNO:%d",
			  (s32) sitronix_ts_gpts.irq_gpio, ret);
	ret = -ENODEV;
}
else
{
	printk("%s:sitronix_ts_gpts.irq_gpio =%d\n",__FUNCTION__,sitronix_ts_gpts.irq_gpio);
	ret = gpio_direction_input(sitronix_ts_gpts.irq_gpio);
	if (ret < 0)
		gpio_free(sitronix_ts_gpts.irq_gpio);
	sitronix_ts_gpts.client->irq = gpio_to_irq(sitronix_ts_gpts.irq_gpio);
}
printk("gpio num:%d, irq num:%d\n",CTP_IRQ_NUMBER,sitronix_ts_gpts.client->irq);
#endif

//client->irq = gpio_to_irq(CTP_IRQ_NUMBER);
//DbgMsg("gpio num:%d, irq num:%d\n",CTP_IRQ_NUMBER,client->irq);
#endif


#ifdef SITRONIX_IDENTIFY_ID
	if((ret = sitronix_ts_identify(&sitronix_ts_gpts)) < 0)
		goto err_device_info_error;
#endif // SITRONIX_IDENTIFY_ID

#ifndef SITRONIX_INT_POLLING_MODE
	INIT_WORK(&(sitronix_ts_gpts.work), sitronix_ts_work_func);
#else
	INIT_DELAYED_WORK(&(sitronix_ts_gpts.work), sitronix_ts_work_func);
#endif // SITRONIX_INT_POLLING_MODE

	#if(CTP_AP_SP_SYNC_WAY & CTP_AP_SP_SYNC_GPIO)
	INIT_WORK(&sitronix_ts_gpts.sitronix_io_sync_work, sitronix_io_sync_work_func);
	#endif

	#if(CTP_AP_SP_SYNC_WAY & CTP_AP_SP_SYNC_GPIO)
	sitronix_io_sync_workqueue= create_singlethread_workqueue("sitronix_io_sync_workqueue");
	if (!sitronix_io_sync_workqueue) {
			ErrorMsg("Creat sitronix_io_sync_workqueue workqueue failed.\n");
			goto err_device_info_error;
	}
	#endif

#ifdef SITRONIX_MONITOR_THREAD
	if(sitronix_ts_gpts.enable_monitor_thread == 1){
		//== Add thread to monitor chip
		atomic_set(&iMonitorThreadPostpone,1);
		sitronix_ts_gpts.sitronix_mt_fp = sitronix_ts_gpts.RawCRC_enabled? sitronix_ts_monitor_thread_v2 : sitronix_ts_monitor_thread;
		SitronixMonitorThread = kthread_run(sitronix_ts_gpts.sitronix_mt_fp,"Sitronix","Monitorthread");
		if(IS_ERR(SitronixMonitorThread))
			SitronixMonitorThread = NULL;
	}
#endif // SITRONIX_MONITOR_THREAD

	sitronix_ts_gpts.input_dev = input_allocate_device();
	if (sitronix_ts_gpts.input_dev == NULL){
		printk("Can not allocate memory for input device.");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	sitronix_ts_gpts.input_dev->name = SITRONIX_I2C_TOUCH_MT_INPUT_DEV_NAME;
	sitronix_ts_gpts.input_dev->dev.parent = &client->dev;
	sitronix_ts_gpts.input_dev->id.bustype = BUS_I2C;

	set_bit(EV_KEY, sitronix_ts_gpts.input_dev->evbit);
	set_bit(BTN_TOUCH, sitronix_ts_gpts.input_dev->keybit);
	set_bit(EV_ABS, sitronix_ts_gpts.input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, sitronix_ts_gpts.input_dev->propbit);

	sitronix_ts_gpts.keyevent_input = input_allocate_device();
	if (sitronix_ts_gpts.keyevent_input == NULL){
		printk("Can not allocate memory for key input device.");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}
	sitronix_ts_gpts.keyevent_input->name  = SITRONIX_I2C_TOUCH_KEY_INPUT_DEV_NAME;
	sitronix_ts_gpts.keyevent_input->dev.parent = &client->dev;
	set_bit(EV_KEY, sitronix_ts_gpts.keyevent_input->evbit);
	for(i = 0; i < pdata->num_button; i++){
		set_bit(sitronix_sensor_key_array[i].code, sitronix_ts_gpts.keyevent_input->keybit);
		input_set_capability(sitronix_ts_gpts.keyevent_input,	EV_KEY, pdata->button_map[i]);
	}

#ifdef SITRONIX_SMART_WAKE_UP
	set_bit(KEY_POWER, sitronix_ts_gpts.keyevent_input->keybit);
#endif	//SITRONIX_SMART_WAKE_UP

#ifndef SITRONIX_AA_KEY
	max_x = sitronix_ts_gpts.resolution_x;
	max_y = sitronix_ts_gpts.resolution_y;
#else
#ifdef SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
	for(i = 0; i < (sizeof(sitronix_aa_key_array)/sizeof(struct sitronix_AA_key)); i++){
		set_bit(sitronix_aa_key_array[i].code, sitronix_ts_gpts.keyevent_input->keybit);
	}
	max_x = SITRONIX_TOUCH_RESOLUTION_X;
	max_y = SITRONIX_TOUCH_RESOLUTION_Y;
#else
	for(i = 0; i < (sizeof(sitronix_aa_key_array)/sizeof(struct sitronix_AA_key)); i++){
		sitronix_aa_key_array[i].x_low = ((sitronix_ts_gpts.resolution_x / (sizeof(sitronix_aa_key_array)/sizeof(struct sitronix_AA_key)) ) * i ) + 15;
		sitronix_aa_key_array[i].x_high = ((sitronix_ts_gpts.resolution_x / (sizeof(sitronix_aa_key_array)/sizeof(struct sitronix_AA_key)) ) * (i + 1)) - 15;
		sitronix_aa_key_array[i].y_low = sitronix_ts_gpts.resolution_y - sitronix_ts_gpts.resolution_y / SCALE_KEY_HIGH_Y;
		sitronix_aa_key_array[i].y_high = sitronix_ts_gpts.resolution_y;
		DbgMsg("key[%d] %d, %d, %d, %d\n", i, sitronix_aa_key_array[i].x_low, sitronix_aa_key_array[i].x_high, sitronix_aa_key_array[i].y_low, sitronix_aa_key_array[i].y_high);
		set_bit(sitronix_aa_key_array[i].code, sitronix_ts_gpts.keyevent_input->keybit);
	}
	max_x = sitronix_ts_gpts.resolution_x;
	max_y = sitronix_ts_gpts.resolution_y - sitronix_ts_gpts.resolution_y / SCALE_KEY_HIGH_Y;
#endif // SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
#endif // SITRONIX_AA_KEY


	ret = input_register_device(sitronix_ts_gpts.keyevent_input);
	if(ret < 0){
		printk("Can not register key input device.\n");
		goto err_input_register_device_failed;
	}


#ifdef SITRONIX_SUPPORT_MT_SLOT
	input_mt_init_slots(sitronix_ts_gpts.input_dev, sitronix_ts_gpts.max_touches);
#else
	__set_bit(ABS_X, sitronix_ts_gpts.input_dev->absbit);
	__set_bit(ABS_Y, sitronix_ts_gpts.input_dev->absbit);
	__set_bit(ABS_MT_TOUCH_MAJOR, sitronix_ts_gpts.input_dev->absbit);
	__set_bit(ABS_MT_WIDTH_MAJOR, sitronix_ts_gpts.input_dev->absbit);
	__set_bit(ABS_MT_POSITION_X, sitronix_ts_gpts.input_dev->absbit);
	__set_bit(ABS_MT_POSITION_Y, sitronix_ts_gpts.input_dev->absbit);
	__set_bit(ABS_MT_TOOL_TYPE, sitronix_ts_gpts.input_dev->absbit);
	__set_bit(ABS_MT_BLOB_ID, sitronix_ts_gpts.input_dev->absbit);
	__set_bit(ABS_MT_TRACKING_ID, sitronix_ts_gpts.input_dev->absbit);

	input_set_abs_params(sitronix_ts_gpts.input_dev, ABS_MT_TOUCH_MAJOR, 0,  255, 0, 0);
	input_set_abs_params(sitronix_ts_gpts.input_dev, ABS_MT_WIDTH_MAJOR, 0,  255, 0, 0);
	input_set_abs_params(sitronix_ts_gpts.input_dev, ABS_MT_TRACKING_ID, 0, sitronix_ts_gpts.max_touches, 0, 0);
	input_set_abs_params(sitronix_ts_gpts.input_dev, ABS_PRESSURE, 0, 255, 0, 0);
#endif // SITRONIX_SUPPORT_MT_SLOT
printk("%s,line=%d\n", __FUNCTION__,__LINE__);
#ifndef SITRONIX_SWAP_XY
	input_set_abs_params(sitronix_ts_gpts.input_dev, ABS_MT_POSITION_X, 0, max_x, 0, 0);
	input_set_abs_params(sitronix_ts_gpts.input_dev, ABS_MT_POSITION_Y, 0, max_y, 0, 0);
#else
	input_set_abs_params(sitronix_ts_gpts.input_dev, ABS_MT_POSITION_X, 0, max_y, 0, 0);
	input_set_abs_params(sitronix_ts_gpts.input_dev, ABS_MT_POSITION_Y, 0, max_x, 0, 0);
#endif // SITRONIX_SWAP_XY

	ret = input_register_device(sitronix_ts_gpts.input_dev);
	if(ret < 0){
		printk("Can not register input device.\n");
		goto err_input_register_device_failed;
	}

if (sitronix_ts_gpts.client->irq){
		dev_info(&client->dev, "irq = %d\n", sitronix_ts_gpts.client->irq);
#ifdef SITRONIX_LEVEL_TRIGGERED
	//#define IRQF_TRIGGER_NONE	0x00000000
	//#define IRQF_TRIGGER_RISING	0x00000001
	//#define IRQF_TRIGGER_FALLING	0x00000002
	//#define IRQF_TRIGGER_HIGH	0x00000004
	//#define IRQF_TRIGGER_LOW	0x00000008

	//ret = request_irq(client->irq, sitronix_ts_irq_handler, IRQF_TRIGGER_LOW | IRQF_DISABLED, client->name, &sitronix_ts_gpts);
	//ret = request_irq(sitronix_ts_gpts.client->irq, sitronix_ts_irq_handler, IRQF_TRIGGER_RISING |IRQF_TRIGGER_FALLING , SITRONIX_I2C_TOUCH_DRV_NAME, &sitronix_ts_gpts);
	//ret = request_irq(sitronix_ts_gpts.client->irq, sitronix_ts_irq_handler, IRQF_TRIGGER_RISING |IRQF_DISABLED , client->name, &sitronix_ts_gpts);
	ret = request_irq(sitronix_ts_gpts.client->irq, sitronix_ts_irq_handler, IRQF_TRIGGER_FALLING, client->name, &sitronix_ts_gpts);
#else
	ret = request_irq(sitronix_ts_gpts.client->irq, sitronix_ts_irq_handler, IRQF_TRIGGER_FALLING,SITRONIX_I2C_TOUCH_DRV_NAME, &sitronix_ts_gpts);
	//ret = request_irq(client->irq, sitronix_ts_irq_handler, IRQF_TRIGGER_FALLING | IRQF_DISABLED, client->name, &sitronix_ts_gpts);

#endif // SITRONIX_LEVEL_TRIGGERED
		if (ret == 0){
			atomic_set(&sitronix_ts_irq_on, 1);
			sitronix_ts_gpts.use_irq = 1;
		}else{
			dev_err(&client->dev, "request_irq failed\n");
			goto err_request_irq_failed;
		}
	}


#ifdef SITRONIX_SYSFS
	if(!sitronix_ts_sysfs_created){
		ret = sitronix_ts_create_sysfs_entry(client);
		if(ret < 0)
			goto err_create_sysfs_failed;
		sitronix_ts_sysfs_created = true;
	}
#endif // SITRONIX_SYSFS
printk("%s,line=%d\n", __FUNCTION__,__LINE__);

#if defined(CONFIG_FB)
	sitronix_ts_gpts.fb_notif.notifier_call = fb_notifier_callback;

	ret = fb_register_client(&sitronix_ts_gpts.fb_notif);

	if (ret)
		dev_err(&client->dev, "Unable to register fb_notifier: %d\n", ret);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	sitronix_ts_gpts.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	sitronix_ts_gpts.early_suspend.suspend = sitronix_ts_early_suspend;
	sitronix_ts_gpts.early_suspend.resume = sitronix_ts_late_resume;
	register_early_suspend(&sitronix_ts_gpts.early_suspend);
#endif // CONFIG_HAS_EARLYSUSPEND


//#ifdef ST_UPGRADE_FIRMWARE
//#ifdef ST_FIREWARE_FILE
//	kthread_run(st_upgrade_fw, "Sitronix", "sitronix_update");
//#else
//	st_upgrade_fw();
//#endif //ST_FIREWARE_FILE

//#endif //ST_UPGRADE_FIRMWARE


#ifdef ST_TEST_RAW
	st_drv_test_raw();
#endif //ST_TEST_RAW

	//add touch pannel prepare switch to sp interface
	#if (CTP_AP_SP_SYNC_WAY != CTP_AP_SP_SYNC_NONE)
	//sitronix_ts_gpts.sitronix_driver_ops.touch_device_name = CTP_NAME;
	//sitronix_ts_gpts.sitronix_driver_ops.touch_pannel_reinit = sitronix_touch_reinit;
	//sitronix_ts_gpts.sitronix_driver_ops.touch_pannel_uninit = sitronix_touch_uninit;
	//gSitronixPtr =  &sitronix_ts_gpts;
	#endif

	#if(CTP_AP_SP_SYNC_WAY & CTP_AP_SP_SYNC_APP)
	// touch_panel_driver_add(&gSitronixPtr->sitronix_driver_ops);
	#endif
	printk("%s,line=%d\n", __FUNCTION__,__LINE__);

	#if(CTP_AP_SP_SYNC_WAY & CTP_AP_SP_SYNC_GPIO)
	//spin_lock_init(&ctp_sync_lock);
	//init_timer(&ctp_sync_timer);
	//ctp_sync_timer.expires = jiffies + HZ/50;
	//ctp_sync_timer.function = ctp_sync_timer_func;
	//add_timer(&ctp_sync_timer);   //crash
	#endif

	return 0;
err_create_sysfs_failed:
err_request_irq_failed:
#ifdef SITRONIX_SYSFS
	input_unregister_device(sitronix_ts_gpts.input_dev);
	input_unregister_device(sitronix_ts_gpts.keyevent_input);
#endif // SITRONIX_SYSFS
err_input_register_device_failed:
err_input_dev_alloc_failed:
	#if(CTP_AP_SP_SYNC_WAY & CTP_AP_SP_SYNC_GPIO)
	destroy_workqueue(sitronix_io_sync_workqueue);
	#endif

	if(sitronix_ts_gpts.input_dev)
		input_free_device(sitronix_ts_gpts.input_dev);
	if(sitronix_ts_gpts.keyevent_input)
		input_free_device(sitronix_ts_gpts.keyevent_input);
#ifdef SITRONIX_MONITOR_THREAD
	if(sitronix_ts_gpts.enable_monitor_thread == 1){
		if(SitronixMonitorThread){
		      kthread_stop(SitronixMonitorThread);
		      SitronixMonitorThread = NULL;
		}
	}
#endif // SITRONIX_MONITOR_THREAD
err_device_info_error:
err_check_functionality_failed:

	return ret;
}

static void sitronix_ts_remove(struct i2c_client *client)
{
	struct sitronix_ts_data *ts = i2c_get_clientdata(client);

#if defined(CONFIG_FB)
	if (fb_unregister_client(&ts->fb_notif))
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif

#ifdef SITRONIX_SYSFS
	if(!sitronix_ts_sysfs_using){
		sitronix_ts_destroy_sysfs_entry(client);
		sitronix_ts_sysfs_created = false;
	}
#endif // SITRONIX_SYSFS
#ifdef SITRONIX_MONITOR_THREAD
	if(ts->enable_monitor_thread == 1){
		if(SitronixMonitorThread){
		      kthread_stop(SitronixMonitorThread);
		      SitronixMonitorThread = NULL;
		}
	}
#endif // SITRONIX_MONITOR_THREAD
	i2c_set_clientdata(client, NULL);
	if (ts->use_irq)
		free_irq(client->irq, ts);
	if(ts->input_dev)
		input_unregister_device(ts->input_dev);
	if(ts->keyevent_input)
		input_unregister_device(ts->keyevent_input);
}

static int sitronix_ts_suspend(struct i2c_client *client)
{
	int ret;
	struct sitronix_ts_data *ts = i2c_get_clientdata(client);

	DbgMsg("%s\n", __FUNCTION__);
//2.9.15 petitk add
#ifdef SITRONIX_SMART_WAKE_UP
	sitronix_swk_set_swk_enable(ts);
	swk_flag = 1;
#endif //SITRONIX_SMART_WAKE_UP

#ifdef SITRONIX_MONITOR_THREAD
	if(ts->enable_monitor_thread == 1){
		if(SitronixMonitorThread){
			kthread_stop(SitronixMonitorThread);
			SitronixMonitorThread = NULL;
		}
		sitronix_ts_delay_monitor_thread_start = DELAY_MONITOR_THREAD_START_RESUME;
	}
#endif // SITRONIX_MONITOR_THREAD
	if(ts->use_irq){
		atomic_set(&sitronix_ts_irq_on, 0);
		disable_irq_nosync(ts->client->irq);
	}
	ts->suspend_state = 1;

	ret = sitronix_ts_set_powerdown_bit(ts, 1);
	DbgMsg("%s return\n", __FUNCTION__);

	return 0;
}

static int sitronix_ts_resume(struct i2c_client *client)
{
	int ret;
	struct sitronix_ts_data *ts = i2c_get_clientdata(client);

	DbgMsg("%s\n", __FUNCTION__);

	ret = sitronix_ts_set_powerdown_bit(ts, 0);

	ts->suspend_state = 0;
	if(ts->use_irq){
		atomic_set(&sitronix_ts_irq_on, 1);
		enable_irq(ts->client->irq);
	}
#ifdef SITRONIX_MONITOR_THREAD
	if(ts->enable_monitor_thread == 1){
		atomic_set(&iMonitorThreadPostpone,1);
		SitronixMonitorThread = kthread_run(sitronix_ts_gpts.sitronix_mt_fp,"Sitronix","Monitorthread");
		if(IS_ERR(SitronixMonitorThread))
			SitronixMonitorThread = NULL;
	}
#endif // SITRONIX_MONITOR_THREAD
	DbgMsg("%s return\n", __FUNCTION__);

	return 0;
}


#if defined(CONFIG_FB)

/*******************************************************************************
*  Name: fb_notifier_callback
*  Brief:
*  Input:
*  Output:
*  Return:
*******************************************************************************/
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct sitronix_ts_data *sitronix_data = container_of(self, struct sitronix_ts_data, fb_notif);
				      //container_of(self, struct fts_ts_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK &&
			sitronix_data && sitronix_data->client) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK)
			sitronix_ts_resume(sitronix_data->client);
		else if (*blank == FB_BLANK_POWERDOWN)
			sitronix_ts_suspend(sitronix_data->client);
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void sitronix_ts_early_suspend(struct early_suspend *h)
{
	struct sitronix_ts_data *ts;
	DbgMsg("%s\n", __FUNCTION__);
	ts = container_of(h, struct sitronix_ts_data, early_suspend);
	sitronix_ts_suspend(ts->client);
}

static void sitronix_ts_late_resume(struct early_suspend *h)
{
	struct sitronix_ts_data *ts;
	DbgMsg("%s\n", __FUNCTION__);
	ts = container_of(h, struct sitronix_ts_data, early_suspend);
	sitronix_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id sitronix_ts_id[] = {
	{ SITRONIX_I2C_TOUCH_DRV_NAME, 0 },
	{ }
};
#ifdef SITRONIX_MULTI_SLAVE_ADDR
static int sitronix_ts_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	uint8_t buffer[8];
	sitronix_ts_reset_ic();
	printk("%s: bus = %d\n", __FUNCTION__, client->adapter->nr);
	if((client->adapter->nr == twi_id) && (!sitronix_i2c_read_bytes(client, STATUS_REG, buffer, 8))){
		printk("detect successed\n");
		strlcpy(info->type, SITRONIX_I2C_TOUCH_DRV_NAME, strlen(SITRONIX_I2C_TOUCH_DRV_NAME)+1);
		return 0;
	}else{
		printk("detect failed\n");
		return -ENODEV;
	}
}

const unsigned short sitronix_i2c_addr[] = {0x20, 0x38, 0x55, 0x70, 0x60, I2C_CLIENT_END};
#endif // SITRONIX_MULTI_SLAVE_ADDR



static struct of_device_id sitronix_match_table[] = {
	{ .compatible = "sitronix,st1633i", },
	{ },
};

//@landicorp: lpz add
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
static int sitronix_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	sitronix_ts_suspend(client);
        return 0;

}
static int sitronix_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	sitronix_ts_resume(client);
        return 0;
}

static const struct dev_pm_ops sitronix_ts_dev_pm_ops = {
	.suspend = sitronix_pm_suspend,
	.resume = sitronix_pm_resume,
};
#endif
static struct i2c_driver sitronix_ts_driver = {
#ifdef SITRONIX_MULTI_SLAVE_ADDR
	.class		= I2C_CLASS_HWMON,
#endif // SITRONIX_MULTI_SLAVE_ADDR
	.probe		= sitronix_ts_probe,
	.remove		= sitronix_ts_remove,
	.id_table	= sitronix_ts_id,
	.driver = {
		.name	= SITRONIX_I2C_TOUCH_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = sitronix_match_table,
#if CONFIG_PM
		.suspend = NULL,
		.resume = NULL,//fts_resume,
#endif
	},
#ifdef SITRONIX_MULTI_SLAVE_ADDR
	//.address_list = sitronix_i2c_addr,
	.detect = sitronix_ts_detect,
#endif // SITRONIX_MULTI_SLAVE_ADDR
};

#ifdef SITRONIX_FW_UPGRADE_FEATURE
static struct file_operations nc_fops = {
	.owner =        THIS_MODULE,
	.write		= sitronix_write,
	.read		= sitronix_read,
	.open		= sitronix_open,
	.unlocked_ioctl = sitronix_ioctl,
	.release	= sitronix_release,
};
#endif // SITRONIX_FW_UPGRADE_FEATURE
void sitronix_ts_reprobe(void)
{
	int retval = 0;
	printk("sitronix call reprobe!\n");
	i2c_del_driver(&sitronix_ts_driver);
	retval = i2c_add_driver(&sitronix_ts_driver);
	if(retval < 0)
		printk("fail to reprobe driver!\n");
}

#ifdef CONFIG_ARCH_SUNXI
/**
 * ctp_print_info - sysconfig print function
 * return value:
 *
 */
void sitronix_print_info(struct ctp_config_info info)
{
	DbgMsg("info.ctp_used:%d\n",info.ctp_used);
	DbgMsg("info.twi_id:%d\n",info.twi_id);
	DbgMsg("info.screen_max_x:%d\n",info.screen_max_x);
	DbgMsg("info.screen_max_y:%d\n",info.screen_max_y);
	DbgMsg("info.revert_x_flag:%d\n",info.revert_x_flag);
	DbgMsg("info.revert_y_flag:%d\n",info.revert_y_flag);
	DbgMsg("info.exchange_x_y_flag:%d\n",info.exchange_x_y_flag);
	DbgMsg("info.irq_gpio_number:%d\n",info.irq_gpio.gpio);
	DbgMsg("info.wakeup_gpio_number:%d\n",info.wakeup_gpio.gpio);
}


static int ctp_get_system_config(void)
{
        sitronix_print_info(config_info);
        twi_id = config_info.twi_id;
        screen_max_x = config_info.screen_max_x;
        screen_max_y = config_info.screen_max_y;
        revert_x_flag = config_info.revert_x_flag;
        revert_y_flag = config_info.revert_y_flag;
        exchange_x_y_flag = config_info.exchange_x_y_flag;
        if((screen_max_x == 0) || (screen_max_y == 0)){
                ErrorMsg("%s:read config error!\n",__func__);
                return 0;
        }
        return 1;
}
#endif

static int __init sitronix_ts_init(void)
{

#ifdef CONFIG_ARCH_SUNXI
	s32 ret = -1;
#endif
	s32 ret_iic = -2;
	#if(CTP_AP_SP_SYNC_WAY & CTP_AP_SP_SYNC_GPIO)
	//script_item_u   val;
	//script_item_value_type_e  type;
	//int pin_val = 0;
	#endif
#ifdef SITRONIX_FW_UPGRADE_FEATURE
	int result;
	int err = 0;
	dev_t devno = MKDEV(sitronix_major, 0);
#endif // SITRONIX_FW_UPGRADE_FEATURE

	#if(CTP_AP_SP_SYNC_WAY & CTP_AP_SP_SYNC_GPIO)
	//type = script_get_item("ctp_para", "ctp_sync_io", &val);
	//if(SCIRPT_ITEM_VALUE_TYPE_PIO != type) {
	//	ErrorMsg("%s: ctp_sync_io script_get_item err. \n",__func__ );
	//}
	//else
	{
		printk("ctp_sync_io get ok\n");
		//sitronix_sync_io = val.gpio;
	}

	//ap_sp_sync_gpio(sitronix_sync_io, CTP_SYNC_IO_IN, pin_val);
	#endif
#ifdef CONFIG_ARCH_SUNXI
	DbgMsg("****************************************************************\n");
	if (0)//(input_fetch_sysconfig_para(&(config_info.input_type)))
	{
		ErrorMsg("%s: ctp_fetch_sysconfig_para err.\n", __func__);
		return 0;
	}
	else
	{
		//ret = input_init_platform_resource(&(config_info.input_type));
		if (0 != ret) {
			ErrorMsg("%s:ctp_ops.init_platform_resource err. \n", __func__);
		}
	}

	if(config_info.ctp_used == 0)
	{
		ErrorMsg("*** ctp_used set to 0 !\n");
		ErrorMsg("*** if use ctp,please put the sys_config.fex ctp_used set to 1. \n");
		return 0;
	}

	if(!ctp_get_system_config())
	{
		ErrorMsg("%s:read config fail!\n",__func__);
		return ret;
	}
	DbgMsg("%s\n",__func__);

	//input_set_power_enable(&(config_info.input_type), 1);

	//sunxi_gpio_to_name(CTP_IRQ_NUMBER,irq_pin_name);

	//sitronix_i2c_driver.detect = ctp_detect;
#endif

	printk("Sitronix touch driver %d.%d.%d\n", DRIVER_MAJOR, DRIVER_MINOR, DRIVER_PATCHLEVEL);
	printk("Release date: %s\n", DRIVER_DATE);
#ifdef SITRONIX_FW_UPGRADE_FEATURE
	result  = alloc_chrdev_region(&devno, 0, 1, SITRONIX_I2C_TOUCH_DEV_NAME);
	if(result < 0){
		printk("fail to allocate chrdev (%d) \n", result);
		return 0;
	}
	sitronix_major = MAJOR(devno);
	cdev_init(&sitronix_cdev, &nc_fops);
	sitronix_cdev.owner = THIS_MODULE;
	sitronix_cdev.ops = &nc_fops;
        err =  cdev_add(&sitronix_cdev, devno, 1);
	if(err){
		printk("fail to add cdev (%d) \n", err);
		return 0;
	}
	printk("%s,%d\n",__FUNCTION__,__LINE__);

	sitronix_class = class_create(THIS_MODULE, SITRONIX_I2C_TOUCH_DEV_NAME);
	if (IS_ERR(sitronix_class)) {
		result = PTR_ERR(sitronix_class);
		unregister_chrdev(sitronix_major, SITRONIX_I2C_TOUCH_DEV_NAME);
		printk("fail to create class (%d) \n", result);
		return result;
	}
	printk("%s,%d\n",__FUNCTION__,__LINE__);
	device_create(sitronix_class, NULL, MKDEV(sitronix_major, 0), NULL, SITRONIX_I2C_TOUCH_DEV_NAME);
#ifdef SITRONIX_PERMISSION_THREAD
	SitronixPermissionThread = kthread_run(sitronix_ts_permission_thread,"Sitronix","Permissionthread");
	if(IS_ERR(SitronixPermissionThread))
		SitronixPermissionThread = NULL;
#endif // SITRONIX_PERMISSION_THREAD
#endif // SITRONIX_FW_UPGRADE_FEATURE

	ret_iic= i2c_add_driver(&sitronix_ts_driver);
	printk("IIC ADD DRIVER %s,ret_iic=%d\n",__FUNCTION__,ret_iic);

	return  ret_iic;
	//return i2c_add_driver(&sitronix_ts_driver);
}

static void __exit sitronix_ts_exit(void)
{
#ifdef SITRONIX_FW_UPGRADE_FEATURE
	dev_t dev_id = MKDEV(sitronix_major, 0);
#endif // SITRONIX_FW_UPGRADE_FEATURE
	i2c_del_driver(&sitronix_ts_driver);
#ifdef SITRONIX_FW_UPGRADE_FEATURE
	cdev_del(&sitronix_cdev);

	device_destroy(sitronix_class, dev_id); //delete device node under /dev
	class_destroy(sitronix_class); //delete class created by us
	unregister_chrdev_region(dev_id, 1);
#ifdef SITRONIX_PERMISSION_THREAD
	if(SitronixPermissionThread)
		SitronixPermissionThread = NULL;
#endif // SITRONIX_PERMISSION_THREAD
#endif // SITRONIX_FW_UPGRADE_FEATURE
}

module_init(sitronix_ts_init);
module_exit(sitronix_ts_exit);

MODULE_DESCRIPTION("Sitronix Multi-Touch Driver");
MODULE_LICENSE("GPL");


#if defined(ST_TEST_RAW) || defined(ST_UPGRADE_FIRMWARE)

static int st_i2c_read_direct(st_u8 *rxbuf, int len)
{

	int ret = 0;
	ret = i2c_master_recv(sitronix_ts_gpts.client, rxbuf, len);

	if (ret < 0){
		stmsg("read direct error (%d)\n", ret);
		return ret;
	}
	return len;
}

static int st_i2c_read_bytes(st_u8 addr, st_u8 *rxbuf, int len)
{

	int ret = 0;
	st_u8 txbuf = addr;

	ret = i2c_master_send(sitronix_ts_gpts.client, &txbuf, 1);
	if (ret < 0){
		stmsg("write 0x%x error (%d)\n", addr, ret);
		return ret;
	}
	ret = i2c_master_recv(sitronix_ts_gpts.client, rxbuf, len);

	if (ret < 0){
		stmsg("read 0x%x error (%d)\n", addr, ret);
		return ret;
	}
	return len;
}

static int st_i2c_write_bytes(st_u8 *txbuf, int len)
{

	int ret = 0;
	if(txbuf == NULL)
		return -1;

	ret = i2c_master_send(sitronix_ts_gpts.client, txbuf, len);
	if (ret < 0){
		stmsg("write 0x%x error (%d)\n", *txbuf, ret);
		return ret;
	}
	return len;
}
#endif


#ifdef ST_UPGRADE_FIRMWARE

static int st_check_chipid(void)
{
	int ret = 0;
	st_u8 buffer[8];

	ret = st_i2c_read_bytes(0x1, buffer, 8);
	if (ret < 0){
		stmsg("read status reg error (%d)\n", ret);
		return ret;
	}else{
		stmsg("ChipID = %d\n", buffer[1]);
	}

#ifdef ST_IC_A8008
	if(buffer[1] != 0x6)
	{
		stmsg("This IC is not A8008 , cancel upgrade\n");
		return -1;
	}
#else
	if(buffer[1] != 0xA)
	{
		stmsg("This IC is not A8010 , cancel upgrade\n");
		return -1;
	}
#endif
	return 0;
}

static int st_get_device_status(void)
{
	int ret = 0;
	st_u8 buffer[8];

	ret = st_i2c_read_bytes(0x1, buffer, 8);
	if (ret < 0){
		stmsg("read status reg error (%d)\n", ret);
		return ret;
	}else{
		stmsg("status reg = %d\n", buffer[0]);
	}


	return buffer[0];
}

static int st_check_device_status(int ck1,int ck2,int delay)
{
	int maxTimes = 10;
	int isInStauts = 0;
	int status = -1;
	while(maxTimes-->0 && isInStauts==0)
	{
		status = st_get_device_status();
		stmsg("status : %d\n",status);
		if(status == ck1 || status == ck2)
			isInStauts=1;
		st_msleep(delay);
	}
	if(isInStauts==0)
		return -1;
	else
		return 0;
}

static int st_power_up(void)
{
	st_u8 reset[2];
	reset[0] = 2;
	reset[1] = 0;
	return st_i2c_write_bytes(reset,2);
}
int st_isp_off(void)
{
	unsigned char data[8];
	int rt = 0;

	memset(data, 0, sizeof(data));
	data[0] = ISP_CMD_RESET;

	rt += st_i2c_write_bytes(data,sizeof(data));

	if(rt < 0)
	{
		stmsg("ISP off error\n");
		return -1;
	}
	st_msleep(300);


	return st_check_device_status(0,4,10);
}
static int st_isp_on(void)
{
	unsigned char IspKey[] = {0,'S',0,'T',0,'X',0,'_',0,'F',0,'W',0,'U',0,'P'};
	unsigned char i;
	int icStatus = st_get_device_status();

	stmsg("ISP on\n");

	if(icStatus <0)
		return -1;
	if(icStatus == 0x6)
		return 0;
	else if(icStatus == 0x5)
		st_power_up();

	for(i=0;i<sizeof(IspKey); i+=2)
	{
		if(st_i2c_write_bytes(&IspKey[i],2) < 0)
		{
			stmsg("Entering ISP fail.\n");
			return -1;
		}
	}
	st_msleep(300);	//This delay is very important for ISP mode changing.
					//Do not remove this delay arbitrarily.
	return st_check_device_status(6,99,10);
}

static int st_irq_off(void)
{
	//if (sitronix_ts_gpts.use_irq){
	//	atomic_set(&sitronix_ts_irq_on, 0);
	//	disable_irq_nosync(sitronix_ts_gpts.client->irq);
	//}
	return 0;
}
static int st_irq_on(void)
{
	//if (sitronix_ts_gpts.use_irq)
	//{
	//	atomic_set(&sitronix_ts_irq_on, 1);
	//	enable_irq(sitronix_ts_gpts.client->irq);
	//}
	return 0;
}

unsigned short st_flash_get_checksum(unsigned char *Buf, unsigned short ValidDataSize)
{
	unsigned short Checksum;
	int i;

	Checksum = 0;
	for(i = 0; i < ValidDataSize; i++)
		Checksum += (unsigned short)Buf[i];

	return Checksum;
}

static int st_flash_unlock(void)
{
	unsigned char PacketData[ISP_PACKET_SIZE];

	int retryCount=0;
	int isSuccess=0;
	while(isSuccess==0 && retryCount++ < 2)
	{
		memset(PacketData,0,ISP_PACKET_SIZE);
		PacketData[0] = ISP_CMD_UNLOCK;
		if(st_i2c_write_bytes(PacketData,ISP_PACKET_SIZE) == ISP_PACKET_SIZE)
		{
			if(retryCount > 1)
				st_msleep(150);

			if(st_i2c_read_direct(PacketData,ISP_PACKET_SIZE) == ISP_PACKET_SIZE)
			{
				if(PacketData[0] == ISP_CMD_READY)
					isSuccess = 1;
			}
			else
				st_msleep(50);
		}

		if(isSuccess ==0)
		{
			stmsg("Read ISP_Unlock_Ready packet fail retry : %d\n",retryCount);
			//MSLEEP(30);
		}


	}

	if(isSuccess == 0)
	{
		stmsg("Read ISP_Unlock_Ready packet fail.\n");
		return -1;
	}

	return 0;
}

int st_flash_erase_page(unsigned short PageNumber)
{
	unsigned char PacketData[ISP_PACKET_SIZE];

	int retryCount=0;
	int isSuccess=0;
	while(isSuccess==0 && retryCount++ < 2)
	{
		memset(PacketData,0,ISP_PACKET_SIZE);
		PacketData[0] = ISP_CMD_ERASE;
		PacketData[2] = (unsigned char)PageNumber;
		if(st_i2c_write_bytes(PacketData,ISP_PACKET_SIZE) == ISP_PACKET_SIZE)
		{
			//if(retryCount > 1)
				st_msleep(150);

			if(st_i2c_read_direct(PacketData,ISP_PACKET_SIZE) == ISP_PACKET_SIZE)
			{
				if(PacketData[0] == ISP_CMD_READY)
					isSuccess = 1;
			}
			else
			{
				//time out
				st_msleep(50);
			}
		}

		if(isSuccess ==0)
		{
			stmsg("Read ISP_Erase_Ready packet fail with page %d retry : %d\n",PageNumber,retryCount);
			//MSLEEP(30);
		}


	}

	if(isSuccess == 0)
	{
		stmsg("Read ISP_Erase_Ready packet fail.\n");
		return -1;
	}

	return 0;
}

static int st_flash_read_page(unsigned char *Buf,unsigned short PageNumber)
{
	unsigned char PacketData[ISP_PACKET_SIZE];
	short ReadNumByte;
	short ReadLength;

	ReadNumByte = 0;
	memset(PacketData,0,ISP_PACKET_SIZE);
	PacketData[0] = ISP_CMD_READ_FLASH;
	PacketData[2] = (unsigned char)PageNumber;
	if(st_i2c_write_bytes(PacketData,ISP_PACKET_SIZE) != ISP_PACKET_SIZE)
	{
		stmsg("Send ISP_Read_Flash packet fail.\n");
		return -1;
	}

	while(ReadNumByte < ST_FLASH_PAGE_SIZE)
	{
		if((ReadLength = st_i2c_read_direct(Buf+ReadNumByte,ISP_PACKET_SIZE)) != ISP_PACKET_SIZE)
		{
			stmsg("ISP read page data fail.\n");
			return -1;
		}
		if(ReadLength == 0)
			break;
		ReadNumByte += ReadLength;
	}
	return ReadNumByte;
}

static int st_flash_write_page(unsigned char *Buf,unsigned short PageNumber)
{
	unsigned char PacketData[ISP_PACKET_SIZE];
	short WriteNumByte;
	short WriteLength;
	unsigned short Checksum;
	unsigned char RetryCount;

	RetryCount = 0;
	while(RetryCount++ < 1)
	{
		WriteNumByte = 0;
		memset(PacketData,0,ISP_PACKET_SIZE);
		Checksum = st_flash_get_checksum(Buf,ST_FLASH_PAGE_SIZE);

		PacketData[0] = ISP_CMD_WRITE_FLASH;
		PacketData[2] = (unsigned char)PageNumber;
		PacketData[4] = (unsigned char)(Checksum & 0xFF);
		PacketData[5] = (unsigned char)(Checksum >> 8);
		if(st_i2c_write_bytes(PacketData,ISP_PACKET_SIZE) != ISP_PACKET_SIZE )
		{
			stmsg("Send ISP_Write_Flash packet fail.\n");
			return -1;
		}
		PacketData[0] = ISP_CMD_SEND_DATA;
		while(WriteNumByte < ST_FLASH_PAGE_SIZE)
		{
			WriteLength = ST_FLASH_PAGE_SIZE - WriteNumByte;
			if(WriteLength > 7)
				WriteLength = 7;
			memcpy(&PacketData[1],&Buf[WriteNumByte],WriteLength);
			if(st_i2c_write_bytes(PacketData,ISP_PACKET_SIZE) != ISP_PACKET_SIZE)
			{
				stmsg("Send ISP_Write_Flash_Data packet error.\n");
				return -1;
			}
			WriteNumByte += WriteLength;
		}

		st_msleep(150);
		if(st_i2c_read_direct(PacketData,ISP_PACKET_SIZE) != ISP_PACKET_SIZE)
		{
			stmsg("ISP get \"Write Data Ready Packet\" fail.\n");
			return -1;
		}
		if(PacketData[0] != ISP_CMD_READY)
		{
			stmsg("Command ID of \"Write Data Ready Packet\" error.\n");
			return -1;
		}

		if((PacketData[2] & 0x10) != 0)
		{
			stmsg("Error occurs during write page data into flash. Error Code = 0x%X\n",PacketData[2]);
			return -1;
		}

		break;
	}
	return WriteNumByte;
}

int st_flash_write(unsigned char *Buf, int Offset, int NumByte)
{

	unsigned short StartPage;
	unsigned short PageOffset;
	int WriteNumByte;
	short WriteLength;
	unsigned char TempBuf[ST_FLASH_PAGE_SIZE];
	int retry = 0;
	int isSuccess = 0;

	stmsg("Write flash offset:0x%X , length:0x%X\n",Offset,NumByte);

	WriteNumByte = 0;
	if(NumByte == 0)
		return WriteNumByte;

	if((Offset + NumByte) > ST_FLASH_SIZE)
		NumByte = ST_FLASH_SIZE - Offset;

	StartPage = Offset / ST_FLASH_PAGE_SIZE;
	PageOffset = Offset % ST_FLASH_PAGE_SIZE;
	while(NumByte > 0)
	{
		if((PageOffset != 0) || (NumByte < ST_FLASH_PAGE_SIZE))
		{
			if(st_flash_read_page(TempBuf,StartPage) < 0)
				return -1;
		}

		WriteLength = ST_FLASH_PAGE_SIZE - PageOffset;
		if(NumByte < WriteLength)
			WriteLength = NumByte;
		memcpy(&TempBuf[PageOffset],Buf,WriteLength);

		retry = 0;
		isSuccess = 0;
		while(retry++ <2 && isSuccess ==0)
		{
			if(st_flash_unlock() >= 0 && st_flash_erase_page(StartPage) >= 0)
			{
				stmsg("write page:%d\n",StartPage);
				if(st_flash_unlock() >= 0 && st_flash_write_page(TempBuf,StartPage) >= 0)
					isSuccess =1;
			}
			isSuccess =1;

			if(isSuccess==0)
				stmsg("FIOCTL_IspPageWrite write page %d retry: %d\n",StartPage,retry);
		}
		if(isSuccess==0)
		{
			stmsg("FIOCTL_IspPageWrite write page %d error\n",StartPage);
			return -1;
		}
		else
			StartPage++;

		NumByte -= WriteLength;
		Buf += WriteLength;
		WriteNumByte += WriteLength;
		PageOffset = 0;
	}
	return WriteNumByte;
}



#ifdef ST_FIREWARE_FILE
static int st_check_fs_mounted(st_char *path_name)
{
    struct path root_path;
    struct path path;
    int err;
    err = kern_path("/", LOOKUP_FOLLOW, &root_path);

    if (err)
        return -1;

    err = kern_path(path_name, LOOKUP_FOLLOW, &path);

    if (err)
        return -1;

   return 0;

}
static int st_load_cfg_from_file(unsigned st_char *buf)
{
	int j;
	struct file  *cfg_fp;
	mm_segment_t fs;
	int fileSize = 0;

	cfg_fp = filp_open(ST_CFG_PATH, O_RDWR,0644);
	if(IS_ERR(cfg_fp)){
		stmsg("Test: filp_open error!!. %d\n",j);
	}else
	{
		fs = get_fs();
		set_fs(get_ds());

		fileSize = cfg_fp->f_op->read(cfg_fp,buf,ST_CFG_LEN,&cfg_fp->f_pos);
		set_fs(fs);
		filp_close(cfg_fp,NULL);
	}
	return fileSize;
}

static int st_load_fw_from_file(unsigned st_char *buf)
{
	int i,j;
	struct st_file  *fw_fp;
	mm_segment_t fs;
	int fileSize = 0;

	for(j=0;j<10;j++)
	{
		st_msleep(1000);
		stmsg("Wati for FS %d\n", j);

		if (st_check_fs_mounted(ST_FW_DIR) == 0)
		{
	stmsg("%s mounted ~!!!!\n",ST_FW_DIR);
	fileSize = 1;
	break;
		}
	}
	if(fileSize ==0)
	{
		stmsg("%s don't mounted ~!!!!\n",ST_FW_DIR);
		return -1;
	}
	for(j=0;j<10;j++)
	{
		st_msleep(1000);
		fileSize = 0;
		fw_fp = st_filp_open(ST_FW_PATH, O_RDWR,0666);
		if(IS_ERR(fw_fp)){
		stmsg("Test: filp_open error!!. %d\n",j);
			fileSize = 0;
	        }else
	        {
		fileSize = 0;
			fs = get_fs();
			set_fs(get_ds());
			//f->f_op->read(f,buf,ROM_SIZE,&f->f_pos);
			fileSize = fw_fp->f_op->read(fw_fp,buf,ST_FW_LEN,&fw_fp->f_pos);
			set_fs(fs);
			stmsg("fw file size:0x%X\n",fileSize);
			//for(i=0;i<0x10;i++)
			//	stmsg("Test: data is %X",buf[i]);
		st_filp_close(fw_fp,NULL);
			break;
		}
	}
	return fileSize;
}

unsigned char fw_buf[ST_FW_LEN];
unsigned char cfg_buf[ST_CFG_LEN];
#else
unsigned char fw_buf[] = SITRONIX_FW;
unsigned char cfg_buf[] = SITRONIX_CFG;
#endif //ST_FIREWARE_FILE


static int st_get_fw_info_offset(int fwSize,unsigned st_char *data)
{
	int off = 0;
	int devParamTabSize;
	int progSizeH;
	int progSizeL;

	off += 131+2+4;
	devParamTabSize = data[off];
	off += 1+devParamTabSize+4;	//4:TFW1

	if(off >= fwSize)
	{
		stmsg("FW structure error! check point 1\n");
		return -1;
	}
	progSizeH = data[off];
	progSizeL = data[off+1];

	off += 2 + ((progSizeH)*0x100) + progSizeL;
	if(off >= fwSize)
	{
		stmsg("FW structure error! check point 2");
		return -1;
	}

	if(data[off]   == 0x54 &&
	   data[off+1] == 0x46 &&
	   data[off+2] == 0x49 &&
	   data[off+3] == 0x32 )
	{
		stmsg("TOUCH_FW_INFO offset = 0x%X\n",off);
		return off+4;
	}
/*
	int i=0;
	for(i=fwSize-ST_FW_INFO_LEN-4;i>=4;i--)
	{
		if(	buf[i]   == 0x54 &&
			buf[i+1] == 0x46 &&
			buf[i+2] == 0x49 &&
			buf[i+3] == 0x32 )
		{
			stmsg("TOUCH_FW_INFO offset = 0x%X\n",i+4);
			return i+4;
		}
	}
*/
	stmsg("can't find TOUCH_FW_INFO offset\n");
	return -1;
}
static int st_compare_array(unsigned st_char *b1,unsigned st_char *b2,int len)
{
	int i=0;
	for(i=0;i<len;i++)
		if(b1[i] != b2[i])
			return -1;
	return 0;
}
unsigned char fw_check[ST_FLASH_PAGE_SIZE];
#ifdef ST_UPGRADE_BY_ID
unsigned char id_buf[] = SITRONIX_IDS;
unsigned char id_off[] = SITRONIX_ID_OFF;

unsigned char fw_buf0[] = SITRONIX_FW;
unsigned char cfg_buf0[] = SITRONIX_CFG;
unsigned char fw_buf1[] = SITRONIX_FW1;
unsigned char cfg_buf1[] = SITRONIX_CFG1;
unsigned char fw_buf2[] = SITRONIX_FW2;
unsigned char cfg_buf2[] = SITRONIX_CFG2;

static void st_replace_fw_by_id(int id)
{
	if(id==0)
	{
		stmsg("Found id by SITRONIX_FW and SITRONIX_CFG\n");
		memcpy(fw_buf,fw_buf0,sizeof(fw_buf0));
		memcpy(cfg_buf,cfg_buf0,sizeof(cfg_buf0));
	}
	else if(id==1)
	{
		stmsg("Found id by SITRONIX_FW1 and SITRONIX_CFG1\n");
		memcpy(fw_buf,fw_buf1,sizeof(fw_buf1));
		memcpy(cfg_buf,cfg_buf1,sizeof(cfg_buf1));
	}
	else if(id==2)
	{
		stmsg("Found id by SITRONIX_FW2 and SITRONIX_CFG2\n");
		memcpy(fw_buf,fw_buf2,sizeof(fw_buf2));
		memcpy(cfg_buf,cfg_buf2,sizeof(cfg_buf2));
	}
}

static int st_select_fw_by_id(void)
{
	int ret=0;
	unsigned char buf[8];
	unsigned char id[4];
	int i=0;
	int idlen = sizeof(id_buf) / 4;
	int isFindID = 0;
	int status = st_get_device_status();
	if(status < 0)
	{
		return -1;
	}
	else if(status != 0x6)
	{
		st_i2c_read_bytes(0xC, buf, 4);
		st_i2c_read_bytes(0xF1, buf+4, 1);
		buf[6] = buf[4];
		buf[5] =  (buf[4] &0xFC) | 1;
		buf[4] = 0xF1;
		st_i2c_write_bytes(buf+4, 2);
		st_msleep(1);
		st_i2c_read_bytes(0xC, id, 4);
		buf[5] = buf[6];
		st_i2c_write_bytes(buf+4, 2);
		st_msleep(1);

		if(	id[0] == buf[0]
		   &&	id[1] == buf[1]
		   &&	id[2] == buf[2]
		   &&	id[3] == buf[3])
		 {
			stmsg("read customer id fail \n");
			return -1;
		 }
		 else
			stmsg("customer ids: %x %x %x %x \n",id[0],id[1],id[2],id[3]);


		 for(i=0;i<idlen;i++)
		 {
			if(	id[0] == id_buf[i*4]
			&&	id[1] == id_buf[i*4+1]
			&&	id[2] == id_buf[i*4+2]
			&&	id[3] == id_buf[i*4+3])
			{
				isFindID =1;
				st_replace_fw_by_id(i);
			}
		 }
		 if(0== isFindID)
			return -1;
	}
	else
	{
		stmsg("IC's status : boot code \n");
		// if bootcode
		if(0==st_isp_off())
		{
			//could ispoff
			stmsg("IC's could go normat status \n");
			return st_select_fw_by_id();
		}
		else
		{
			stmsg("IC really bootcode mode \n");
			ret = st_flash_read_page(fw_check,ST_CHECK_FW_OFFSET / ST_FLASH_PAGE_SIZE);
			if(ret < 0 )
			{
				stmsg("read flash fail! (%x)\n",ret);
			}

			for(i=0;i<idlen;i++)
			{
#ifdef ST_IC_A8008

				if(	fw_check[0x300+id_off[i]] == id_buf[i*4]
				&&	fw_check[0x301+id_off[i]] == id_buf[i*4+1]
				&&	fw_check[0x302+id_off[i]] == id_buf[i*4+2]
				&&	fw_check[0x303+id_off[i]] == id_buf[i*4+3])
#else
				if(	fw_check[0+id_off[i]] == id_buf[i*4]
				&&	fw_check[1+id_off[i]] == id_buf[i*4+1]
				&&	fw_check[2+id_off[i]] == id_buf[i*4+2]
				&&	fw_check[3+id_off[i]] == id_buf[i*4+3])
#endif
				{
					stmsg("Find ID in CFG ,customer ids: %x %x %x %x \n",id_buf[i*4],id_buf[i*4+1],id_buf[i*4+2],id_buf[i*4+3]);
					isFindID =1;
					st_replace_fw_by_id(i);
				}
			}

			if(0== isFindID)
			{
				stmsg("Find ID in CFG fail , try to find in page 0\n");

				ret = st_flash_read_page(fw_check,0);
				if(ret < 0 )
				{
					stmsg("read flash fail! (%x)\n",ret);
				}

				for(i=0;i<idlen;i++)
				{

					if(	fw_check[0+id_off[i]] == id_buf[i*4]
					&&	fw_check[1+id_off[i]] == id_buf[i*4+1]
					&&	fw_check[2+id_off[i]] == id_buf[i*4+2]
					&&	fw_check[3+id_off[i]] == id_buf[i*4+3])
					{
						stmsg("Find ID in page 0 ,customer ids: %x %x %x %x \n",id_buf[i*4],id_buf[i*4+1],id_buf[i*4+2],id_buf[i*4+3]);
						isFindID =1;
						st_replace_fw_by_id(i);
					}
				}
				if(0== isFindID)
					return -1;

			}
		}


	}
	return 0;
}
#endif //ST_UPGRADE_BY_ID


int st_upgrade_fw(void)
{
	int rt=0;
	int fwSize =0;
	int cfgSize =0;
	int fwInfoOff = 0;
	int fwInfoLen = ST_FW_INFO_LEN;
	int powerfulWrite = 0;

#ifdef ST_FIREWARE_FILE
	fwSize = st_load_fw_from_file(fw_buf);

	cfgSize = st_load_cfg_from_file(cfg_buf);
#else

#ifdef ST_UPGRADE_BY_ID

	if(0 != st_select_fw_by_id())
	{
		stmsg("find id fail , cancel upgrade\n");
		return -1;
	}
#endif

	fwSize = sizeof(fw_buf);
	cfgSize = sizeof(cfg_buf);
	stmsg("fwSize 0x%X,cfgsize 0x%X\n",fwSize,cfgSize);
#endif //ST_FIREWARE_FILE

	cfgSize = min(cfgSize,ST_CFG_LEN);
	if(fwSize != 0)
	{
		fwInfoOff = st_get_fw_info_offset(fwSize,fw_buf);
		if(fwInfoOff <0)
			fwSize = 0;
		fwInfoLen = fw_buf[fwInfoOff]+1+4;
#ifdef ST_IC_A8008
		if(fwInfoLen != ST_CFG_OFFSET - fwInfoOff)
		{
			stmsg("check FwInfo Len error (%x), cancel upgrade\n",fwInfoLen);
			return -1;
		}
#else
		if(fwInfoOff == -1)
		{
			stmsg("check fwInfoOff Len error (%x), cancel upgrade\n",fwInfoOff);
			return -1;
		}
#endif
		stmsg("fwInfoOff 0x%X\n",fwInfoOff);
	}

	if(fwSize ==0 && cfgSize ==0)
	{
		stmsg("can't find FW or CFG , cancel upgrade\n");
		return -1;
	}

	if(st_get_device_status() == 0x6)
		powerfulWrite = 1;

	st_irq_off();

	rt = st_isp_on();
	if(rt !=0)
	{
		stmsg("ISP on fail\n");
		goto ST_IRQ_ON;
	}

	if(st_check_chipid() < 0)
	{
		stmsg("Check ChipId fail\n");
		rt = -1;
		goto ST_ISP_OFF;
	}

	if(powerfulWrite ==0 &&(fwSize !=0 || cfgSize!=0))
	{
		//check fw and cfg
		int checkOff = (fwInfoOff / ST_FLASH_PAGE_SIZE) *ST_FLASH_PAGE_SIZE;
		if(st_flash_read_page(fw_check,fwInfoOff / ST_FLASH_PAGE_SIZE)< 0 )
		{
			stmsg("read flash fail , cancel upgrade\n");
			rt = -1;
			goto ST_ISP_OFF;
		}

		if(fwSize !=0)
		{
			if(0 == st_compare_array(fw_check+(fwInfoOff-checkOff),fw_buf+fwInfoOff,fwInfoLen) )
			{
				stmsg("fw compare :same\n");
				fwSize = 0;
			}
			else
			{
				stmsg("fw compare :different\n");
			}

		}

		if(cfgSize !=0)
		{

#ifdef ST_IC_A8008
			if(0 == st_compare_array(fw_check+(ST_CFG_OFFSET-checkOff),cfg_buf,cfgSize))
#else
			st_flash_read_page(fw_check,ST_CFG_OFFSET / ST_FLASH_PAGE_SIZE);
			if(0 == st_compare_array(fw_check,cfg_buf,cfgSize))
#endif
			{
				stmsg("cfg compare :same\n");
				cfgSize = 0;
			}
			else
			{
				stmsg("cfg compare : different\n");
			}

		}

	}



#ifdef ST_UPGRADE_BY_ID
	if(cfgSize != 0 || fwSize !=0)
	{
		//backup CFG in page 0
		memcpy(fw_check,fw_buf,ST_FLASH_PAGE_SIZE);
		memcpy(fw_check,cfg_buf,cfgSize);
		st_flash_write(fw_check,0,ST_FLASH_PAGE_SIZE);
	}
#endif	//ST_UPGRADE_BY_ID


	if(cfgSize !=0)
		st_flash_write(cfg_buf,ST_CFG_OFFSET,cfgSize);

	if(fwSize !=0)
	{
		//write page 1 ~ N
		st_flash_write(fw_buf+ST_FLASH_PAGE_SIZE,ST_FLASH_PAGE_SIZE,fwSize-ST_FLASH_PAGE_SIZE);
		//write page 0
		st_flash_write(fw_buf,0,ST_FLASH_PAGE_SIZE);
	}

#ifdef ST_UPGRADE_BY_ID
	//restore FW in page 0
	if(cfgSize != 0 && fwSize ==0)
		st_flash_write(fw_buf,0,ST_FLASH_PAGE_SIZE);
#endif	//ST_UPGRADE_BY_ID


ST_ISP_OFF:
	rt = st_isp_off();
ST_IRQ_ON:
	st_irq_on();
	return rt;
}
#endif //ST_UPGRADE_FIRMWARE

#ifdef ST_TEST_RAW
st_int st_drv_Get_2D_Length(st_int tMode[])
{
	if(tMode[0] ==0)
		return tMode[1];
	else
		return tMode[2];
}
st_int st_drv_Get_2D_Count(st_int tMode[])
{
	if(tMode[0] == 0)
		return tMode[2];
	else
		return tMode[1];
}

st_int st_drv_Get_2D_RAW(st_int tMode[],st_int rawJ[],st_int gsMode)
{
	st_int count =st_drv_Get_2D_Count(tMode);
	st_int length = st_drv_Get_2D_Length(tMode);
	st_int maxTimes = 40;
	st_int dataCount=0;
	st_int times = maxTimes;

	st_u8 raw[0x40];
	memset(raw,0,0x40);
	st_int i=0;
	st_int j=0;
	st_int index;
	st_int keyAddCount = (tMode[3] >0)? 1:0;
	st_int rawI;
	st_int errorCount = 0;
	st_u8 isFillData[MAX_SENSOR_COUNT];
	memset(isFillData,0,count+keyAddCount);

	//stmsg("isFill 0 : %d",isFillData[0]);
	while(dataCount != (count+keyAddCount) && times-- >0)
	{
		st_i2c_read_bytes(0x40,raw,0x40);
		//stmsg("%X %X %X data:%d key:%d",raw[0],raw[1],raw[2],dataCount,tMode[3]);
		if(raw[0] == 6)
		{
			int index = raw[2];
			//stmsg("isFill %d : %d %d , %d",index,isFillData[index],dataCount,count+keyAddCount);
			if(isFillData[index] ==0)
			{
				//stmsg("index %d",index);
				isFillData[index] = 1;
				dataCount++;
				//index = index*length;
				for(i=0;i<length;i++)
				{
					rawI = raw[4+2*i]*0x100 + raw[5+2*i];
					stmsg("Sensor RAW %d,%d = %d",index,i,rawI);
					if(rawI > MAX_RAW_LIMIT || rawI  < MIN_RAW_LIMIT)
					{
						stmsg("Error: Sensor RAW %d,%d = %d out of limity (%d,%d)",index,i,rawI,MIN_RAW_LIMIT,MAX_RAW_LIMIT);
						errorCount++;
					}
					//rawI[index+i] = raw[4+2*i]*0x100 + raw[5+2*i];
				}
			}
		}
		else if(raw[0]==7)
		{
			//key
			//stmsg("key");
			if(isFillData[count] ==0)
			{
				isFillData[count] = 1 ;
				dataCount++;
				for(i=0;i<tMode[3];i++)
				{
					rawI = raw[4+2*i]*0x100 + raw[5+2*i];
					//stmsg("Key RAW %d = %d",i,rawI);
					if(rawI > MAX_RAW_LIMIT || rawI  < MIN_RAW_LIMIT)
					{
						stmsg("Error: Key RAW %d = %d out of limity ",i,rawI,MIN_RAW_LIMIT,MAX_RAW_LIMIT);
						errorCount++;
					}
					//rawI[count*length+i] = raw[4+2*i]*0x100 + raw[5+2*i];

				}
			}
		}
	}

	if(times <=0)
	{
		stmsg("Get 2D RAW fail!");
		return -1;
	}
	return errorCount;
}
int st_drv_test_raw()
{
	stmsg("start of st_drv_test_raw");
	st_int result = 0;
	st_u8 buf[8];
	st_int sensorCount =0;
	st_int raw_J[MAX_SENSOR_COUNT];

	/////////////////////////////
	//check status
	memset(buf,0,8);
	if( st_i2c_read_bytes(1,buf,8) <0)
	{
		stmsg("get status fail");
		return -1;
	}

	stmsg("status :0x%X",buf[0]);
	if(buf[0] != 0 && buf[0] != 4)
	{
		stmsg("can't test in this status");
		result = -1;
		goto st_drv_notest;
	}
	//////////////////////////////
	//go develop page
	memset(buf,0,8);
	st_i2c_read_bytes(0xFF, buf,8);
	if(buf[1] != 0x53 || buf[2] != 0x54 || buf[3] != 0x50 || buf[4] != 0x41)
	{
		stmsg("ic check fail , not sitronix protocol type");
		goto st_drv_notest;
	}

	buf[0] = buf[6];
	buf[1] = buf[7];
	st_i2c_write_bytes(buf,2);

	st_msleep(1);
	st_i2c_read_bytes(0xFF,buf,2);
	stmsg("page 0x%X",buf[0]);
	if(buf[0] != 0xEF)
	{
		stmsg("change page fail");
		goto st_drv_notest;
	}
	///////////////////////////////
	//get tmode
	st_int tMode[4];
	tMode[0] = TX_ASSIGNMENT;	//tx is ?
	st_i2c_read_bytes(0xF5,buf,3);
	tMode[1] = buf[0];	//x
	tMode[2] = buf[1];	//y
	tMode[3] = buf[2]&0xf;	// key

	sensorCount = tMode[1]+tMode[2]+tMode[3];

	stmsg("sensor count:%d %d %d",tMode[1],tMode[2],tMode[3]);
	//////////////////////////////
	//get raw and judge
	result = st_drv_Get_2D_RAW(tMode,raw_J,0);
	if(result !=0)
	{
		stmsg("Error: Test fail with %d sensor",result);
		result = -1;
	}
	else
	{
	stmsg("Test successed!");
	}
	//////////////////////////////
	//reset
	buf[0] = 2;
	buf[1] = 1;
	st_i2c_write_bytes(buf,2);

	st_msleep(100);
st_drv_notest:
	return result;
}
#endif //ST_TEST_RAW
