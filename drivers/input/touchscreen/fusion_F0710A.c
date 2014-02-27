/*
 *  "fusion_F0710A"  touchscreen driver
 *	
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/gpio.h>

#include "fusion_F0710A.h"

#define DRV_NAME		"fusion_F0710A"


static struct fusion_F0710A_data fusion_F0710A;

static unsigned short normal_i2c[] = { fusion_F0710A_I2C_SLAVE_ADDR, I2C_CLIENT_END };

//I2C_CLIENT_INSMOD;

static int fusion_F0710A_write_u8(u8 addr, u8 data) 
{
	return i2c_smbus_write_byte_data(fusion_F0710A.client, addr, data);
}

static int fusion_F0710A_read_u8(u8 addr)
{
	return i2c_smbus_read_byte_data(fusion_F0710A.client, addr);
}

static int fusion_F0710A_read_block(u8 addr, u8 len, u8 *data)
{
#if 0
	/* When i2c_smbus_read_i2c_block_data() takes a block length parameter, we can do
	 * this. lm-sensors lists hints this has been fixed, but I can't tell whether it
	 * was or will be merged upstream. */

	return i2c_smbus_read_i2c_block_data(&fusion_F0710A.client, addr, data);
#else
	u8 msgbuf0[1] = { addr };
	u16 slave = fusion_F0710A.client->addr;
	u16 flags = fusion_F0710A.client->flags;
	struct i2c_msg msg[2] = { { slave, flags, 1, msgbuf0 },
				  { slave, flags | I2C_M_RD, len, data }
	};

	return i2c_transfer(fusion_F0710A.client->adapter, msg, ARRAY_SIZE(msg));
#endif
}


static int fusion_F0710A_register_input(void)
{
	int ret;
	struct input_dev *dev;

	dev = fusion_F0710A.input = input_allocate_device();
	if (dev == NULL)
		return -ENOMEM;

	dev->name = "fusion_F0710A";

	set_bit(EV_KEY, dev->evbit);
	set_bit(EV_ABS, dev->evbit);

	input_set_abs_params(dev, ABS_MT_POSITION_X, 0, fusion_F0710A.info.xres-1, 0, 0);
	input_set_abs_params(dev, ABS_MT_POSITION_Y, 0, fusion_F0710A.info.yres-1, 0, 0);
	input_set_abs_params(dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(dev, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);

	ret = input_register_device(dev);
	if (ret < 0)
		goto bail1;

	return 0;

bail1:
	input_free_device(dev);
	return ret;
}

#define WC_RETRY_COUNT 		3
static int fusion_F0710A_write_complete(void)
{
	int ret, i;

	for(i=0; i<WC_RETRY_COUNT; i++)
	{
		ret = fusion_F0710A_write_u8(fusion_F0710A_SCAN_COMPLETE, 0);
		if(ret == 0)
			break;
		else
			dev_err(&fusion_F0710A.client->dev, "Write complete failed(%d): %d\n", i, ret);
	}

	return ret;
}

#define DATA_START	fusion_F0710A_DATA_INFO
#define	DATA_END	fusion_F0710A_SEC_TIDTS
#define DATA_LEN	(DATA_END - DATA_START + 1)
#define DATA_OFF(x)	((x) - DATA_START)

static int fusion_F0710A_read_sensor(void)
{
	int ret;
	u8 data[DATA_LEN];

#define DATA(x) (data[DATA_OFF(x)])
	/* To ensure data coherency, read the sensor with a single transaction. */
	ret = fusion_F0710A_read_block(DATA_START, DATA_LEN, data);
	if (ret < 0) {
		dev_err(&fusion_F0710A.client->dev,
			"Read block failed: %d\n", ret);
		/* Clear fusion_F0710A interrupt */
		fusion_F0710A_write_complete();
		
		return ret;
	}

	fusion_F0710A.f_num = DATA(fusion_F0710A_DATA_INFO)&0x03;
	
	fusion_F0710A.y1 = DATA(fusion_F0710A_POS_X1_HI) << 8;
	fusion_F0710A.y1 |= DATA(fusion_F0710A_POS_X1_LO);
	fusion_F0710A.x1 = DATA(fusion_F0710A_POS_Y1_HI) << 8;
	fusion_F0710A.x1 |= DATA(fusion_F0710A_POS_Y1_LO);
	fusion_F0710A.z1 = DATA(fusion_F0710A_FIR_PRESS);
	fusion_F0710A.tip1 = DATA(fusion_F0710A_FIR_TIDTS)&0x0f;
	fusion_F0710A.tid1 = (DATA(fusion_F0710A_FIR_TIDTS)&0xf0)>>4;
	
	
	fusion_F0710A.y2 = DATA(fusion_F0710A_POS_X2_HI) << 8;
	fusion_F0710A.y2 |= DATA(fusion_F0710A_POS_X2_LO);
	fusion_F0710A.x2 = DATA(fusion_F0710A_POS_Y2_HI) << 8;
	fusion_F0710A.x2 |= DATA(fusion_F0710A_POS_Y2_LO);
	fusion_F0710A.z2 = DATA(fusion_F0710A_SEC_PRESS);
	fusion_F0710A.tip2 = DATA(fusion_F0710A_SEC_TIDTS)&0x0f;
	fusion_F0710A.tid2 =(DATA(fusion_F0710A_SEC_TIDTS)&0xf0)>>4;

#undef DATA
	/* Clear fusion_F0710A interrupt */
	return fusion_F0710A_write_complete();
}

#define val_cut_max(x, max, reverse)	\
do					\
{					\
	if(x > max)			\
		x = max;		\
	if(reverse)			\
		x = (max) - (x);	\
}					\
while(0)

static void fusion_F0710A_wq(struct work_struct *work)
{
	struct input_dev *dev = fusion_F0710A.input;
	int save_points = 0;
	int x1 = 0, y1 = 0, z1 = 0, x2 = 0, y2 = 0, z2 = 0;

	if (fusion_F0710A_read_sensor() < 0)
		return;

	printk(KERN_DEBUG "tip1, tid1, x1, y1, z1 (%x,%x,%d,%d,%d); tip2, tid2, x2, y2, z2 (%x,%x,%d,%d,%d)\n",
		fusion_F0710A.tip1, fusion_F0710A.tid1, fusion_F0710A.x1, fusion_F0710A.y1, fusion_F0710A.z1,
		fusion_F0710A.tip2, fusion_F0710A.tid2, fusion_F0710A.x2, fusion_F0710A.y2, fusion_F0710A.z2);

	val_cut_max(fusion_F0710A.x1, fusion_F0710A.info.xres-1, fusion_F0710A.info.xy_reverse);
	val_cut_max(fusion_F0710A.y1, fusion_F0710A.info.yres-1, fusion_F0710A.info.xy_reverse);
	val_cut_max(fusion_F0710A.x2, fusion_F0710A.info.xres-1, fusion_F0710A.info.xy_reverse);
	val_cut_max(fusion_F0710A.y2, fusion_F0710A.info.yres-1, fusion_F0710A.info.xy_reverse);

	if(fusion_F0710A.tip1 == 1)
	{
		if(fusion_F0710A.tid1 == 1)
		{
			/* first point */
			x1 = fusion_F0710A.x1;
			y1 = fusion_F0710A.y1;
			z1 = fusion_F0710A.z1;
			save_points |= fusion_F0710A_SAVE_PT1;
		}
		else if(fusion_F0710A.tid1 == 2)
		{
			/* second point ABS_DISTANCE second point pressure, BTN_2 second point touch */
			x2 = fusion_F0710A.x1;
			y2 = fusion_F0710A.y1;
			z2 = fusion_F0710A.z1;
			save_points |= fusion_F0710A_SAVE_PT2;
		}
	}

	if(fusion_F0710A.tip2 == 1)
	{
		if(fusion_F0710A.tid2 == 2)
		{
			/* second point ABS_DISTANCE second point pressure, BTN_2 second point touch */
			x2 = fusion_F0710A.x2;
			y2 = fusion_F0710A.y2;
			z2 = fusion_F0710A.z2;
			save_points |= fusion_F0710A_SAVE_PT2;
		}
		else if(fusion_F0710A.tid2 == 1)/* maybe this will never happen */
		{
			/* first point */
			x1 = fusion_F0710A.x2;
			y1 = fusion_F0710A.y2;
			z1 = fusion_F0710A.z2;
			save_points |= fusion_F0710A_SAVE_PT1;
		}
	}

	input_report_abs(dev, ABS_MT_TOUCH_MAJOR, z1);
	input_report_abs(dev, ABS_MT_WIDTH_MAJOR, 1);
	input_report_abs(dev, ABS_MT_POSITION_X, x1);
	input_report_abs(dev, ABS_MT_POSITION_Y, y1);
	input_mt_sync(dev);
	input_report_abs(dev, ABS_MT_TOUCH_MAJOR, z2);
	input_report_abs(dev, ABS_MT_WIDTH_MAJOR, 2);
	input_report_abs(dev, ABS_MT_POSITION_X, x2);
	input_report_abs(dev, ABS_MT_POSITION_Y, y2);
	input_mt_sync(dev);

	input_sync(dev);

	enable_irq(fusion_F0710A.client->irq);

}
static DECLARE_WORK(fusion_F0710A_work, fusion_F0710A_wq);

static irqreturn_t fusion_F0710A_interrupt(int irq, void *dev_id)
{
	disable_irq_nosync(fusion_F0710A.client->irq);

	queue_work(fusion_F0710A.workq, &fusion_F0710A_work);
	return IRQ_HANDLED;
}

const static u8* g_ver_product[4] = {
	"10Z8", "70Z7", "43Z6", ""
};

static int fusion_F0710A_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	int ret;
	u8 ver_product, ver_id;
	u32 version;

	if(!i2c->irq)
	{
		dev_err(&i2c->dev, "fusion_F0710A irq < 0 \n");
		ret = -ENOMEM;
		goto bail1;
	}

	/* Attach the I2C client */
	fusion_F0710A.client =  i2c;
	i2c_set_clientdata(i2c, &fusion_F0710A);

	printk(KERN_INFO "fusion_F0710A :Touchscreen registered with bus id (%d) with slave address 0x%x\n",
			i2c_adapter_id(fusion_F0710A.client->adapter),	fusion_F0710A.client->addr);

	/* Read out a lot of registers */
	ret = fusion_F0710A_read_u8(fusion_F0710A_VIESION_INFO_LO);
	if (ret < 0) {
		dev_err(&i2c->dev, "query failed: %d\n", ret);
		goto bail1;
	}
	ver_product = (((u8)ret) & 0xc0) >> 6;
	version = (10 + ((((u32)ret)&0x30) >> 4)) * 100000;
	version += (((u32)ret)&0xf) * 1000;
	/* Read out a lot of registers */
	ret = fusion_F0710A_read_u8(fusion_F0710A_VIESION_INFO);
		if (ret < 0) {
		dev_err(&i2c->dev, "query failed: %d\n", ret);
		goto bail1;
	}
	ver_id = ((u8)(ret) & 0x6) >> 1;
	version += ((((u32)ret) & 0xf8) >> 3) * 10;
	version += (((u32)ret) & 0x1) + 1; /* 0 is build 1, 1 is build 2 */
	printk(KERN_INFO "fusion_F0710A version product %s(%d)\n", g_ver_product[ver_product] ,ver_product);
	printk(KERN_INFO "fusion_F0710A version id %s(%d)\n", ver_id ? "1.4" : "1.0", ver_id);
	printk(KERN_INFO "fusion_F0710A version series (%d)\n", version);

	switch(ver_product)
	{
	case fusion_F0710A_VIESION_07: /* 7 inch */
		fusion_F0710A.info.xres = fusion_F0710A07_XMAX;
		fusion_F0710A.info.yres = fusion_F0710A07_YMAX;
		fusion_F0710A.info.xy_reverse = fusion_F0710A07_REV;
		break;
	case fusion_F0710A_VIESION_43: /* 4.3 inch */
		fusion_F0710A.info.xres = fusion_F0710A43_XMAX;
		fusion_F0710A.info.yres = fusion_F0710A43_YMAX;
		fusion_F0710A.info.xy_reverse = fusion_F0710A43_REV;
		break;
	default: /* fusion_F0710A_VIESION_10 10 inch */
		fusion_F0710A.info.xres = fusion_F0710A10_XMAX;
		fusion_F0710A.info.yres = fusion_F0710A10_YMAX;
		fusion_F0710A.info.xy_reverse = fusion_F0710A10_REV;
		break;
	}

	/* Register the input device. */
	ret = fusion_F0710A_register_input();
	if (ret < 0) {
		dev_err(&i2c->dev, "can't register input: %d\n", ret);
		goto bail1;
	}

	/* Create a worker thread */
	fusion_F0710A.workq = create_singlethread_workqueue(DRV_NAME);
	if (fusion_F0710A.workq == NULL) {
		dev_err(&i2c->dev, "can't create work queue\n");
		ret = -ENOMEM;
		goto bail2;
	}


	/* Register for the interrupt and enable it. Our handler will
	*  start getting invoked after this call. */
	ret = request_irq(i2c->irq, fusion_F0710A_interrupt, IRQF_TRIGGER_RISING,
	i2c->name, &fusion_F0710A);
	if (ret < 0) {
		dev_err(&i2c->dev, "can't get irq %d: %d\n", i2c->irq, ret);
		goto bail3;
	}
	/* clear the irq first */
	ret = fusion_F0710A_write_u8(fusion_F0710A_SCAN_COMPLETE, 0);
	if (ret < 0) {
		dev_err(&i2c->dev, "Clear irq failed: %d\n", ret);
		goto bail4;
	}

	return 0;

	bail4:
	free_irq(i2c->irq, &fusion_F0710A);

	bail3:
	destroy_workqueue(fusion_F0710A.workq);
	fusion_F0710A.workq = NULL;


	bail2:
	input_unregister_device(fusion_F0710A.input);
	bail1:

	return ret;
}

#ifdef CONFIG_PM
static int fusion_F0710A_suspend(struct i2c_client *i2c, pm_message_t mesg)
{
	disable_irq(i2c->irq);
	flush_workqueue(fusion_F0710A.workq);

	return 0;
}

static int fusion_F0710A_resume(struct i2c_client *i2c)
{
	enable_irq(i2c->irq);

	return 0;
}
#endif

static int fusion_F0710A_remove(struct i2c_client *i2c)
{
	destroy_workqueue(fusion_F0710A.workq);
	free_irq(i2c->irq, &fusion_F0710A);
	input_unregister_device(fusion_F0710A.input);
	i2c_set_clientdata(i2c, NULL);

	printk(KERN_INFO "fusion_F0710A driver removed\n");
	
	return 0;
}

static struct i2c_device_id fusion_F0710A_id[] = {
	{"fusion_F0710A", 0},
	{},
};

static struct i2c_driver fusion_F0710A_i2c_drv = {
	.driver = {
		.name		= DRV_NAME,
	},
	.probe          = fusion_F0710A_probe,
	.remove         = fusion_F0710A_remove,
#ifdef CONFIG_PM
	.suspend		= fusion_F0710A_suspend,
	.resume			= fusion_F0710A_resume,
#endif
	.id_table       = fusion_F0710A_id,
	.address_list   = normal_i2c,
};


static int __init fusion_F0710A_init( void )
{
	int ret;

	memset(&fusion_F0710A, 0, sizeof(fusion_F0710A));

	/* Probe for fusion_F0710A on I2C. */
	ret = i2c_add_driver(&fusion_F0710A_i2c_drv);
	if (ret < 0) {
		printk(KERN_ERR  "fusion_F0710A_init can't add i2c driver: %d\n", ret);
	}

	return ret;
}

static void __exit fusion_F0710A_exit( void )
{
	i2c_del_driver(&fusion_F0710A_i2c_drv);
}
module_init(fusion_F0710A_init);
module_exit(fusion_F0710A_exit);

MODULE_DESCRIPTION("fusion_F0710A Touchscreen Driver");
MODULE_LICENSE("GPL");

