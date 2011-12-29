/*
 * IRTOUCH serial touchscreen driver
 *
 * Copyright (c) 2011 Smart Zhu
 */

#include <linux/usb.h>
#include <linux/usb/input.h>
#include <linux/hid.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>
#include <linux/delay.h>

#define DRIVER_DESC "IRTOUCH serial touchscreen driver"
#define TOUCH_DEVICE_NAME	"irtouchser"
#define IRTOUCH_MAJOR_NUM	555
#define IRTOUCH_MINOR_NUM 	0

#define SET_COMMAND	0x01
#define GET_COMMAND	0x02

#define	GET_CALIB_X	0x2D
#define	SET_CALIB_X	0x2E
#define	GET_CALIB_Y	0x2B
#define	SET_CALIB_Y	0x2C
#define	GET_DEVICE_CONFIG	0x30
#define	SET_DEVICE_CONFIG	0x31

#define SET_DEVICE_COMMAND_MODE	0x71
#define SET_DEVICE_WORK_MODE	0x93

#define CTLCODE_COORDINATE 0xc0
#define CTLCODE_CALIB_PARA_X 0xc1
#define CTLCODE_CALIB_PARA_Y 0xc2
#define CTLCODE_CALIB_START 0xc3

#define IRTOUCH_MAX_LENGTH  64

#ifndef SERIO_IRTOUCH
# define SERIO_IRTOUCH	0x41
#endif

MODULE_AUTHOR("Smart Zhu <smart.ju888@gmail.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

#pragma pack(1)

typedef struct	_device_config{
	unsigned char deviceID;
	unsigned char monitorID;
	unsigned char reserved[8];
	unsigned char calibrateStatus;
	unsigned char reserved1[47];
}device_config;

typedef struct _calib_param{
	long	A00;
	long	A01;
	long	A10;
	unsigned char reserve[28];
}calib_param;

typedef struct _touch_point{
	int		x;
	int		y;
}touch_point;

struct irtouch_point{
	unsigned char identifier;
	short x;
	short y;
	short z;
	short width;
	short height;
	unsigned char	status;
	unsigned char	reserver;
};

struct irtouch_pkg{
	unsigned char	startBit;
	unsigned char	commandType;
	unsigned char	command;
	unsigned char	pkgLength;
	unsigned char	deviceId;
	struct irtouch_point	points[3];
	unsigned char	actualCounter;
	unsigned char	crc;
};

#pragma pack()

struct irtouch{
	struct input_dev *dev;
	struct serio 	*serio;
	int id;
	int idx;
	unsigned char csum;
	unsigned char data[IRTOUCH_MAX_LENGTH];
	struct irtouch_pkg irpkg;
	char phys[32];
};

struct device_context{
	char *irtouchName;
	bool startCalib;
	bool isCalibX;
	bool isCalibY;
	bool receiveStatus;
	touch_point touchPoint;
	calib_param calibX;
	calib_param calibY;
	device_config devConfig;
};

/*
 * Definitions & global arrays.
 */
struct device_context *devContext;
struct irtouch *irtouch;

static void translatePoint(touch_point pt, int *x, int *y)
{
	*x = ((devContext->calibX.A01 * pt.y) / 10000) + ((devContext->calibX.A10 * pt.x) / 10000) + devContext->calibX.A00;
	*y = ((devContext->calibY.A01 * pt.y) / 10000) + ((devContext->calibY.A10 * pt.x) / 10000) + devContext->calibY.A00;
}

static int build_package(unsigned char command_type, unsigned char command, int device_id, int length, unsigned char *data, unsigned char *out_data)
{
	unsigned char tmp = 0x55;
	unsigned char package[64] = {0};
	int i;
	int j;
	int out_length = 0;

	package[0] = 0xaa;
	package[1] = command_type;
	package[2] = command;
	package[3] = (unsigned char)length;
	package[4] = (unsigned char)device_id;

	if(length > 0)
	{
		length--;
	}

	for(i = 0; i < length; i++)
	{
		package[5 + i] = data[i];
	}

	for(j = 0; j < 5 + length; j++)
	{
		tmp = package[j] + tmp;
	}

	package[5 + length] = (unsigned char)tmp;

	out_length = 6 + length;

	memcpy(out_data, package, out_length);

	return out_length;
}

static int send_command(struct serio *serio, unsigned char *inData, int length)
{
	int i = 0;
	int ret = -1;

	for(i=0; i<length; i++)
	{
		ret = serio_write(serio,inData[i]);
	}

	return ret;
}

static void irtouch_delay(void)
{
	int iCount = 0;

	while(1)
	{
		if(devContext->receiveStatus || iCount > 50)
		{
			devContext->receiveStatus = false;
			return;
		}

		mdelay(10);
		iCount++;
	}
}

static void get_device_param(struct serio *serio)
{
	unsigned char	inData[58];
	unsigned char	outData[64];
	int status = -1;
	int length = 0;
	printk(KERN_ALERT "+++++++++++++++++++%s++++++++++++++++++\n",__func__);
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));

	//set touchscreen to command mode
	inData[0] = 0x01;

	length = build_package(SET_COMMAND,SET_DEVICE_COMMAND_MODE,0,2,inData,outData);
	status = send_command(serio,outData,length);
	if(status == -1)
	{
		printk(KERN_ALERT "Set command mode failed.");
	}
	irtouch_delay();


	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));

	inData[0] = 0xF4;

	length = build_package(SET_COMMAND,SET_DEVICE_WORK_MODE,0,2,inData,outData);
	status = send_command(serio,outData,length);
	if(status == -1)
	{
		printk(KERN_ALERT "Set device to work status failed.");
	}
	irtouch_delay();

	//get device configuration
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));

	length = build_package(GET_COMMAND,GET_DEVICE_CONFIG,1,1,inData,outData);
	status = send_command(serio,outData,length);
	if(status == -1)
	{
		printk(KERN_ALERT "Get device configuration failed.");
	}
	irtouch_delay();
	memcpy(&devContext->devConfig,&irtouch->data[5],sizeof(device_config));

	//get calib x parameter
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));

	length = build_package(GET_COMMAND,GET_CALIB_X,0,1,inData,outData);
	status = send_command(serio,outData,length);
	if(status == -1)
	{
		printk(KERN_ALERT "Get calib X param failed.");
	}
	irtouch_delay();
	memcpy(&devContext->calibX,&irtouch->data[5],sizeof(calib_param));

	//get calib y parameter
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));

	length = build_package(GET_COMMAND,GET_CALIB_Y,0,1,inData,outData);
	status = send_command(serio,outData,length);
	if(status == -1)
	{
		printk(KERN_ALERT "Get calib Y param failed.");
	}
	irtouch_delay();
	memcpy(&devContext->calibY,&irtouch->data[5],sizeof(calib_param));

	//set touchscreen to work mode
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));

	length = build_package(SET_COMMAND,SET_DEVICE_COMMAND_MODE,0,2,inData,outData);
	status = send_command(serio,outData,length);
	if(status == -1)
	{
		printk(KERN_ALERT "Set to work mode failed.");
	}
	irtouch_delay();
}

static void set_calib_param(struct serio *serio)
{
	unsigned char	inData[58];
	unsigned char	outData[64];
	int status = -1;
	int length = 0;
	printk(KERN_ALERT "+++++++++++++++++++%s++++++++++++++++++\n",__func__);
	//set touchscreen to command mode
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));

	inData[0] = 0x01;

	length = build_package(SET_COMMAND,SET_DEVICE_COMMAND_MODE,0,2,inData,outData);
	status = send_command(serio,outData,length);
	if(status == -1)
	{
		printk(KERN_ALERT "Set to command mode failed.");
	}
	irtouch_delay();

	//Set device configuration
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));

	devContext->devConfig.calibrateStatus = 1;
	memcpy(&inData,&devContext->devConfig,58);

	length = build_package(SET_COMMAND,SET_DEVICE_CONFIG,1,59,inData,outData);
	status = send_command(serio,outData,length);
	if(status == -1)
	{
		printk(KERN_ALERT "Set device configuration failed.");
	}
	irtouch_delay();

	//Set calib X param
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));
	memcpy(&inData,&devContext->calibX,40);

	length = build_package(SET_COMMAND,SET_CALIB_X,0,59,inData,outData);
	status = send_command(serio,outData,length);
	if(status == -1)
	{
		printk(KERN_ALERT "Set calib X param failed.");
	}
	irtouch_delay();

	//Set calib Y param
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));
	memcpy(&inData,&devContext->calibY,40);

	length = build_package(SET_COMMAND,SET_CALIB_Y,0,59,inData,outData);
	status = send_command(serio,outData,length);
	if(status == -1)
	{
		printk(KERN_ALERT "Set calib Y param failed.");
	}
	irtouch_delay();

	//set touchscreen to work mode
	memset(inData,0,sizeof(inData));
	memset(outData,0,sizeof(outData));

	length = build_package(SET_COMMAND,SET_DEVICE_COMMAND_MODE,0,2,inData,outData);
	status = send_command(serio,outData,length);
	if(status == -1)
	{
		printk(KERN_ALERT "Set to work mode failed.");
	}
	irtouch_delay();
}

static void irtouch_send_report(struct irtouch* irtouch)
{
	int x = 0;
	int y = 0;
	struct input_dev *dev = irtouch->dev;

	memcpy(&irtouch->irpkg,&irtouch->data[0],46);

	//send single-touch data
/*	devContext->touchPoint.x = irtouch->irpkg.points[0].x >> 3;
	devContext->touchPoint.y = irtouch->irpkg.points[0].y >> 3;

	if(devContext->devConfig.calibrateStatus && !devContext->startCalib)
	{
		translatePoint(devContext->touchPoint,&x,&y);
	}
	else
	{
		x = devContext->touchPoint.x;
		y = devContext->touchPoint.y;
	}

	//send single-touch data
	input_report_abs(dev, ABS_X, x);
	input_report_abs(dev, ABS_Y, y);
	input_report_key(dev, BTN_TOUCH, (irtouch->irpkg.points[0].status == 0x07) ? 1:0);
	input_sync(dev);
*/

	//send dual-touch data
	if(irtouch->irpkg.actualCounter > 1)
	{
		if(irtouch->irpkg.points[0].status == 0x07)
		{
			devContext->touchPoint.x = irtouch->irpkg.points[0].x;
			devContext->touchPoint.y = irtouch->irpkg.points[0].y;

			if(devContext->devConfig.calibrateStatus && !devContext->startCalib)
			{
				translatePoint(devContext->touchPoint,&x,&y);
			}
			else
			{
				x = devContext->touchPoint.x;
				y = devContext->touchPoint.y;
			}

			input_report_abs(dev,ABS_MT_TRACKING_ID,0);
			input_report_abs(dev,ABS_MT_POSITION_X, x);
			input_report_abs(dev,ABS_MT_POSITION_Y, y);
			input_report_abs(dev,ABS_MT_TOUCH_MAJOR, max(x,y));
			input_report_abs(dev,ABS_MT_TOUCH_MINOR, min(x,y));
		}
		else
		{
			input_report_abs(dev,ABS_MT_TOUCH_MAJOR, 0);
		}
		input_mt_sync(dev);

		if(irtouch->irpkg.points[1].status == 0x07)
		{
			devContext->touchPoint.x = irtouch->irpkg.points[1].x;
			devContext->touchPoint.y = irtouch->irpkg.points[1].y;

			if(devContext->devConfig.calibrateStatus && !devContext->startCalib)
			{
				translatePoint(devContext->touchPoint,&x,&y);
			}
			else
			{
				x = devContext->touchPoint.x;
				y = devContext->touchPoint.y;
			}

			input_report_abs(dev,ABS_MT_TRACKING_ID,1);
			input_report_abs(dev,ABS_MT_POSITION_X, x);
			input_report_abs(dev,ABS_MT_POSITION_Y, y);
			input_report_abs(dev,ABS_MT_TOUCH_MAJOR, max(x,y));
			input_report_abs(dev,ABS_MT_TOUCH_MINOR, min(x,y));
		}
		else
		{
			input_report_abs(dev,ABS_MT_TOUCH_MAJOR, 0);
		}
		input_mt_sync(dev);
	}
	else if(irtouch->irpkg.actualCounter == 1)
	{
		if(irtouch->irpkg.points[0].status == 0x07)
		{
			devContext->touchPoint.x = irtouch->irpkg.points[0].x;
			devContext->touchPoint.y = irtouch->irpkg.points[0].y;

			if(devContext->startCalib)
			{
				printk(KERN_ALERT "%s:     star calibretion    x(%d)    y(%d)    stauts(%x)\n",__func__,devContext->touchPoint.x,devContext->touchPoint.y,irtouch->irpkg.points[0].status);
			}

			if(devContext->devConfig.calibrateStatus && !devContext->startCalib)
			{
				translatePoint(devContext->touchPoint,&x,&y);
			}
			else
			{
				x = devContext->touchPoint.x;
				y = devContext->touchPoint.y;
			}

			input_report_abs(dev,ABS_MT_TRACKING_ID,0);
			input_report_abs(dev,ABS_MT_POSITION_X, x);
			input_report_abs(dev,ABS_MT_POSITION_Y, y);
			input_report_abs(dev,ABS_MT_TOUCH_MAJOR, max(x,y));
			input_report_abs(dev,ABS_MT_TOUCH_MINOR, min(x,y));
		}
		else
		{
			input_report_abs(dev,ABS_MT_TOUCH_MAJOR, 0);
		}

		input_mt_sync(dev);
	}
	input_sync(dev);

	//printk(KERN_ALERT "%s:     count(%d)    x(%d)    y(%d)    stauts(%x)\n",__func__,irtouch->irpkg.actualCounter,x,y,irtouch->irpkg.points[0].status);
}

void irtouch_process_data(struct irtouch *irtouch, unsigned char data)
{
	irtouch->data[irtouch->idx] = data;

	switch(irtouch->idx){
		case 0:
			if(data != 0xaa)
			{
				irtouch->idx = 0;
				return;
			}
			break;
		case 1:
			if(data != 0x10)
			{
				irtouch->idx = 0;
				return;
			}
			break;
	}

	if(irtouch->data[0] == 0xaa)
	{
		if(irtouch->idx == (irtouch->data[3] + 4))
		{
			unsigned char CRC  = 0x55;
			int j = 0;

			for(j = 0; j < irtouch->idx; j++)
			{
				CRC = CRC + irtouch->data[j];
			}

			if(CRC == irtouch->data[irtouch->idx])
			{
				if(irtouch->data[2] == 0x65) //touch report;
				{
					irtouch_send_report(irtouch);
				}
				else //command report;
				{
					devContext->receiveStatus = true;
				}
			}

			irtouch->idx = 0;
			return;
		}
		else if(irtouch->data[2] == 0x65 && irtouch->idx > 45)
		{
			irtouch->idx = 0;
			return;
		}
	}
	else
	{
		irtouch->idx = 0;
		return;
	}

	irtouch->idx++;
}

static irqreturn_t irtouch_interrupt(struct serio *serio, unsigned char data, unsigned int flags)
{
	struct irtouch* irtouch = serio_get_drvdata(serio);

	switch(irtouch->id){
	case 0:
		irtouch_process_data(irtouch, data);
		break;
	}

	return IRQ_HANDLED;
}

static long ts_ioctl(struct file * filp, unsigned int ctl_code, unsigned long ctl_param)
{
	unsigned char buf = 0x00;
	int ret = -1;
	printk(KERN_ALERT "+++++++++++++++++++%s++++++++++++++++++\n",__func__);
	switch (ctl_code){

		case CTLCODE_CALIB_START:

			ret = copy_from_user(&buf, (unsigned char *)ctl_param, sizeof(unsigned char));
			if(buf == 0x01)
			{
				devContext->startCalib = true;
			}
			break;

		case CTLCODE_COORDINATE:
			ret = copy_to_user((touch_point *)ctl_param, &devContext->touchPoint, sizeof(touch_point));
			printk(KERN_ALERT "%s:   CTLCODE_COORDINATE   x:%d     y:%d\n",__func__,devContext->touchPoint.x,devContext->touchPoint.y);
			break;

		case CTLCODE_CALIB_PARA_X:
			devContext->isCalibX = true;
			ret = copy_from_user(&devContext->calibX, (calib_param *)ctl_param, sizeof(calib_param));
			break;

		case CTLCODE_CALIB_PARA_Y:
			devContext->isCalibY = true;
			ret = copy_from_user(&devContext->calibY, (calib_param *)ctl_param, sizeof(calib_param));
			break;
	}

	if(devContext->isCalibX && devContext->isCalibY)
	{
		devContext->startCalib = false;

		set_calib_param(irtouch->serio);
	}

	return ret;
}

static struct class *irser_class;
dev_t dev = 0;
struct cdev cdev;

static struct file_operations ts_fops = {
	owner:	THIS_MODULE,
	unlocked_ioctl:ts_ioctl,
};

/*
 * irtouch_disconnect() is the opposite of irtouch_connect()
 */
static void irtouch_disconnect(struct serio *serio)
{
	struct irtouch* irtouch = serio_get_drvdata(serio);

	cdev_del(&cdev);
	unregister_chrdev_region(dev,1);

	device_destroy(irser_class,MKDEV(IRTOUCH_MAJOR_NUM,0));
	class_destroy(irser_class);

	input_unregister_device(irtouch->dev);
	serio_close(serio);
	serio_set_drvdata(serio, NULL);
	kfree(irtouch);
	kfree(devContext);
}

static void char_reg_setup_cdev(void)
{
	int error, devno = MKDEV(IRTOUCH_MAJOR_NUM,IRTOUCH_MINOR_NUM);

	cdev_init(&cdev,&ts_fops);
	cdev.owner = THIS_MODULE;
	cdev.ops = &ts_fops;
	error = cdev_add(&cdev,devno,1);

	if(error)
	{
		printk(KERN_ALERT "Adding char_reg_setup_cdev error=%d",error);
	}
}

/*
 * irtouch_connect() is the routine that is called when someone adds a
 * new serio device that supports Gunze protocol and registers it as
 * an input device.
 */
static int irtouch_connect(struct serio *serio, struct serio_driver *drv)
{
	int err;
	struct input_dev *input_dev;
	printk(KERN_ALERT "+++++++++++++++++++%s++++++++++++++++++\n",__func__);
	if (!(irtouch = kmalloc(sizeof(struct irtouch), GFP_KERNEL)))
	return -ENOMEM;

	if (!(devContext = kmalloc(sizeof(struct device_context), GFP_KERNEL)))
	return -ENOMEM;

	memset(irtouch, 0, sizeof(struct irtouch));
	memset(devContext,0,sizeof(struct device_context));

	devContext->irtouchName = "IRTOUCH Serial TouchScreen";

	//create device node
	dev = MKDEV(IRTOUCH_MAJOR_NUM,IRTOUCH_MINOR_NUM);

	err = register_chrdev_region(dev,1,TOUCH_DEVICE_NAME);
	if(err < 0)
	{
		printk(KERN_ALERT "register chrdev error.\n");
		return err;
	}

	char_reg_setup_cdev();

	irser_class = class_create(THIS_MODULE, TOUCH_DEVICE_NAME);
	if(IS_ERR(irser_class))
	{
		printk(KERN_ALERT "class create failed.\n");
		return PTR_ERR(irser_class);
	}

	device_create(irser_class,NULL,MKDEV(IRTOUCH_MAJOR_NUM,0),NULL,TOUCH_DEVICE_NAME);


	input_dev = input_allocate_device();

	if (!irtouch || !input_dev){
		err = -ENOMEM;
		goto fail1;
	}

	irtouch->id = serio->id.id;
	irtouch->serio = serio;
	irtouch->dev = input_dev;

	irtouch->dev->evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);
	set_bit(BTN_TOUCH, irtouch->dev->keybit);
	irtouch->dev->absbit[0] = BIT(ABS_X) | BIT(ABS_Y);
	set_bit(ABS_PRESSURE, irtouch->dev->absbit);

	//single-touch
/*	input_set_abs_params(irtouch->dev, ABS_X, 0, 4095, 0, 0);
	input_set_abs_params(irtouch->dev, ABS_Y, 0, 4095, 0, 0);
	input_set_abs_params(irtouch->dev, ABS_PRESSURE, 0, 255, 0, 0);
*/
	//dual-touch
	input_set_abs_params(irtouch->dev, ABS_MT_POSITION_X, 0, 32767, 0, 0);
	input_set_abs_params(irtouch->dev, ABS_MT_POSITION_Y, 0, 32767, 0, 0);
	input_set_abs_params(irtouch->dev, ABS_MT_TOUCH_MAJOR, 0, 32767, 0, 0);
	input_set_abs_params(irtouch->dev, ABS_MT_TOUCH_MINOR, 0, 32767, 0, 0);
	input_set_abs_params(irtouch->dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0);

	sprintf(irtouch->phys, "%s/input0", serio->phys);

	irtouch->dev->name = devContext->irtouchName;
	irtouch->dev->phys = irtouch->phys;
	irtouch->dev->id.bustype = BUS_RS232;
	irtouch->dev->id.vendor = 0x6615;//SERIO_UNKNOWN;
	irtouch->dev->id.product = irtouch->id;
	irtouch->dev->id.version = 0x0100;

	serio_set_drvdata(serio, irtouch);

	err = serio_open(serio, drv);
	if (err) {
		serio_set_drvdata(serio, NULL);
		kfree(irtouch);
		kfree(devContext);
		return err;
	}

	get_device_param(serio);

	err = input_register_device(irtouch->dev);
	if(err)
	{
		goto fail1;
	}

	printk(KERN_INFO "input: %s on %s\n", devContext->irtouchName, serio->phys);

	return 0;

fail1:	input_free_device(input_dev);
	kfree(irtouch);
	kfree(devContext);
	return err;
}

/*
 * The serio driver structure.
 */

static struct serio_device_id irtouch_serio_ids[] = {
	{
		.type   = SERIO_RS232,
		.proto  = SERIO_ANY,
		.id = SERIO_ANY,
		.extra  = SERIO_ANY,
	},
	{ 0 }
};

MODULE_DEVICE_TABLE(serio, irtouch_serio_ids);

static struct serio_driver irtouch_drv = {
	.driver     = {
		.name   = "irtouchser",
	},
	.description    = DRIVER_DESC,
	.id_table   = irtouch_serio_ids,
	.interrupt  = irtouch_interrupt,
	.connect    = irtouch_connect,
	.disconnect = irtouch_disconnect,
};

/*
 * The functions for inserting/removing us as a module.
 */
static int __init irtouch_init(void)
{
	int ret = 0;
	printk(KERN_ALERT "+++++++++++++++++++%s++++++++++++++++++\n",__func__);
	ret = serio_register_driver(&irtouch_drv);

	return 0;
}

static void __exit irtouch_exit(void)
{
	serio_unregister_driver(&irtouch_drv);
}

module_init(irtouch_init);
module_exit(irtouch_exit);