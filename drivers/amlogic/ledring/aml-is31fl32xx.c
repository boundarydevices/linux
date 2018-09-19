/*
 * Driver for ISSI IS31FL32xx family of I2C LED controllers
 *
 * Copyright 2015 Allworx Corp.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Datasheets:
 *   http://www.issi.com/US/product-analog-fxled-driver.shtml
 *   http://www.si-en.com/product.asp?parentid=890
 */

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/device.h>
#include <linux/uaccess.h>

/* Used to indicate a device has no such register */
#define IS31FL32XX_REG_NONE 0xFF

/* Software Shutdown bit in Shutdown Register */
#define IS31FL32XX_SHUTDOWN_SSD_ENABLE  0
#define IS31FL32XX_SHUTDOWN_SSD_DISABLE BIT(0)

/* IS31FL3216 has a number of unique registers */
#define IS31FL3216_CONFIG_REG 0x00
#define IS31FL3216_LIGHTING_EFFECT_REG 0x03
#define IS31FL3216_CHANNEL_CONFIG_REG 0x04

/* Software Shutdown bit in 3216 Config Register */
#define IS31FL3216_CONFIG_SSD_ENABLE  BIT(7)
#define IS31FL3216_CONFIG_SSD_DISABLE 0

#define DEFAULT_SPEED     100
#define DEVICE_NAME       "ledrgb"
#define CHAR_DEV_NAME     "aml_ledrgb"
#define CMD_LEDRING_ARG   0x100001

static struct _ledring {
	struct is31fl32xx_priv *g_client;
	struct timer_list mtimer;
	struct work_struct ledring_work;
	int ledring_speed;
	int m_index;
	int timeout_flag;
	int timeout;
	int action_times;
	struct kobject *running_kobj;
} *led_priv_data;

enum {
	MIC1_DIR = 1,
	MIC2_DIR,
	MIC3_DIR,
	MIC4_DIR,
	MIC5_DIR,
	MIC6_DIR
};

static u8 led_map[12] = {0, 2, 1, 3, 5, 4, 6, 8, 7, 9, 11, 10};

static struct _style {
	int num; 		/*the number of led*/
	int times;	 	/*1: stop after action once, 0: cycle action*/
	int speed; 		/*speed. N(ms)*/
	int timeout;		/*timeout N(s)*/
	int style[12][12];	/*style data*/
} styleData = {
	12, 0, DEFAULT_SPEED, 0,
	{
		/*led1    led2    led3    led4    led5    led6    led7    led8    led9    led10  led11   led12*/
	        {0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0xFFFFFF}, /*state1*/
	        {0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0xFFFFFF, 0x000080}, /*state2*/
	        {0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0xFFFFFF, 0x000080, 0x000080}, /*state3*/
	        {0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0xFFFFFF, 0x000080, 0x000080, 0x000080}, /*state4*/
	        {0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0xFFFFFF, 0x000080, 0x000080, 0x000080, 0x000080}, /*state5*/
	        {0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0xFFFFFF, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080}, /*state6*/
	        {0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0xFFFFFF, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080}, /*state7*/
	        {0x000080, 0x000080, 0x000080, 0x000080, 0xFFFFFF, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080}, /*state8*/
	        {0x000080, 0x000080, 0x000080, 0xFFFFFF, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080}, /*state9*/
	        {0x000080, 0x000080, 0xFFFFFF, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080}, /*state10*/
	        {0x000080, 0xFFFFFF, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080}, /*state11*/
	        {0xFFFFFF, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080, 0x000080}, /*state12*/
	},
};

struct is31fl32xx_priv;
struct is31fl32xx_led_data {
	struct led_classdev cdev;
	u8 channel; /* 1-based, max priv->cdef->channels */
	struct is31fl32xx_priv *priv;
};

struct is31fl32xx_led_priv_data {
	int is31fl32xx_led;
	int is31fl32xx_val;
};

struct is31fl32xx_priv {
	const struct is31fl32xx_chipdef *cdef;
	struct i2c_client *client;
	unsigned int num_leds;
	struct class *cls;
	int major;
	struct is31fl32xx_led_data leds[0];
};

static int is31fl32xx_write(struct is31fl32xx_priv *priv, u8 reg, u8 val);


/**
 * struct is31fl32xx_chipdef - chip-specific attributes
 * @channels            : Number of LED channels
 * @shutdown_reg        : address of Shutdown register (optional)
 * @pwm_update_reg      : address of PWM Update register
 * @global_control_reg  : address of Global Control register (optional)
 * @reset_reg           : address of Reset register (optional)
 * @pwm_register_base   : address of first PWM register
 * @pwm_registers_reversed: : true if PWM registers count down instead of up
 * @led_control_register_base : address of first LED control register (optional)
 * @enable_bits_per_led_control_register: number of LEDs enable bits in each
 * @reset_func:         : pointer to reset function
 *
 * For all optional register addresses, the sentinel value %IS31FL32XX_REG_NONE
 * indicates that this chip has no such register.
 *
 * If non-NULL, @reset_func will be called during probing to set all
 * necessary registers to a known initialization state. This is needed
 * for chips that do not have a @reset_reg.
 *
 * @enable_bits_per_led_control_register must be >=1 if
 * @led_control_register_base != %IS31FL32XX_REG_NONE.
 */
struct is31fl32xx_chipdef {
	u8	channels;
	u8	shutdown_reg;
	u8	pwm_update_reg;
	u8	global_control_reg;
	u8	reset_reg;
	u8	pwm_register_base;
	bool	pwm_registers_reversed;
	u8	led_control_register_base;
	u8	enable_bits_per_led_control_register;
	u8	pwm_frequency_set_reg;
	int (*reset_func)(struct is31fl32xx_priv *priv);
	int (*sw_shutdown_func)(struct is31fl32xx_priv *priv, bool enable);
};

static const struct is31fl32xx_chipdef is31fl3236_cdef = {
	.channels				= 36,
	.shutdown_reg				= 0x00,
	.pwm_update_reg				= 0x25,
	.global_control_reg			= 0x4a,
	.reset_reg				= 0x4f,
	.pwm_register_base			= 0x01,
	.pwm_frequency_set_reg                  = 0x4b,
	.led_control_register_base		= 0x26,
	.enable_bits_per_led_control_register	= 1,
};

static const struct is31fl32xx_chipdef is31fl3235_cdef = {
	.channels				= 28,
	.shutdown_reg				= 0x00,
	.pwm_update_reg				= 0x25,
	.global_control_reg			= 0x4a,
	.reset_reg				= 0x4f,
	.pwm_register_base			= 0x05,
	.led_control_register_base		= 0x2a,
	.enable_bits_per_led_control_register	= 1,
};

static const struct is31fl32xx_chipdef is31fl3218_cdef = {
	.channels				= 18,
	.shutdown_reg				= 0x00,
	.pwm_update_reg				= 0x16,
	.global_control_reg			= IS31FL32XX_REG_NONE,
	.reset_reg				= 0x17,
	.pwm_register_base			= 0x01,
	.led_control_register_base		= 0x13,
	.enable_bits_per_led_control_register	= 6,
};

static int is31fl3216_reset(struct is31fl32xx_priv *priv);
static int is31fl3216_software_shutdown(struct is31fl32xx_priv *priv,
					bool enable);
static const struct is31fl32xx_chipdef is31fl3216_cdef = {
	.channels				= 16,
	.shutdown_reg				= IS31FL32XX_REG_NONE,
	.pwm_update_reg				= 0xB0,
	.global_control_reg			= IS31FL32XX_REG_NONE,
	.reset_reg				= IS31FL32XX_REG_NONE,
	.pwm_register_base			= 0x10,
	.pwm_registers_reversed			= true,
	.led_control_register_base		= 0x01,
	.enable_bits_per_led_control_register	= 8,
	.reset_func				= is31fl3216_reset,
	.sw_shutdown_func			= is31fl3216_software_shutdown,
};

static ssize_t run_state_read(
		struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	pr_info("close ledring: echo 0 > runflag\n");
	pr_info("open ledring:  echo 1 > runflag\n");
	return 0;
}
static ssize_t run_state_write(
		struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int ret,i;
	int runflag;
	ret = kstrtouint(buf, 10, &runflag);
	if (ret == 0) {
		if (runflag == 0) {
			del_timer(&led_priv_data->mtimer);
			for (i=0; i< styleData.num*3; i++) {
				ret = is31fl32xx_write(led_priv_data->g_client, i+1, 0);
				if (ret) {
					pr_err("try close led: %d fail\n", i+1);
					return ret;
				}
				ret = is31fl32xx_write(led_priv_data->g_client, 37, 0);
				if (ret)
					pr_err("try close led: %d fail\n", i+1);
			}
			led_priv_data->m_index = 0;
			pr_info("stop running ledring.\n");
		} else if (runflag == 1) {
			pr_info("start running ledring.\n");
			schedule_work(&led_priv_data->ledring_work);
			mod_timer(&led_priv_data->mtimer, jiffies+led_priv_data->ledring_speed*HZ/1000);
		}
	} else {
		pr_info("set run state fail!\n");
	}
	return count;
}

static struct kobj_attribute
running_attribute =
	__ATTR(runflag, 0664,
	run_state_read,
	run_state_write);

static struct attribute
*attrs[] = {
	&running_attribute.attr,
	NULL,
};

static struct attribute_group
attr_group = {
	.attrs = attrs,
};

static int is31fl32xx_write(struct is31fl32xx_priv *priv, u8 reg, u8 val)
{
	int ret;

	//pr_info("writing register %x,%x\n", reg, val);

	ret =  i2c_smbus_write_byte_data(priv->client, reg, val);
	if (ret) {
		dev_err(&priv->client->dev,
			"register write to 0x%02X failed (error %d)",
			reg, ret);
	}
	return ret;
}

/*
 * Custom reset function for IS31FL3216 because it does not have a RESET
 * register the way that the other IS31FL32xx chips do. We don't bother
 * writing the GPIO and animation registers, because the registers we
 * do write ensure those will have no effect.
 */
static int is31fl3216_reset(struct is31fl32xx_priv *priv)
{
	unsigned int i;
	int ret;

	ret = is31fl32xx_write(priv, IS31FL3216_CONFIG_REG,
			       IS31FL3216_CONFIG_SSD_ENABLE);
	if (ret)
		return ret;
	for (i = 0; i < priv->cdef->channels; i++) {
		ret = is31fl32xx_write(priv, priv->cdef->pwm_register_base+i,
				       0x00);
		if (ret)
			return ret;
	}
	ret = is31fl32xx_write(priv, priv->cdef->pwm_update_reg, 0);
	if (ret)
		return ret;
	ret = is31fl32xx_write(priv, IS31FL3216_LIGHTING_EFFECT_REG, 0x00);
	if (ret)
		return ret;
	ret = is31fl32xx_write(priv, IS31FL3216_CHANNEL_CONFIG_REG, 0x00);
	if (ret)
		return ret;

	return 0;
}

/*
 * Custom Software-Shutdown function for IS31FL3216 because it does not have
 * a SHUTDOWN register the way that the other IS31FL32xx chips do.
 * We don't bother doing a read/modify/write on the CONFIG register because
 * we only ever use a value of '0' for the other fields in that register.
 */
static int is31fl3216_software_shutdown(struct is31fl32xx_priv *priv,
					bool enable)
{
	u8 value = enable ? IS31FL3216_CONFIG_SSD_ENABLE :
			    IS31FL3216_CONFIG_SSD_DISABLE;

	return is31fl32xx_write(priv, IS31FL3216_CONFIG_REG, value);
}

/*
 * NOTE: A mutex is not needed in this function because:
 * - All referenced data is read-only after probe()
 * - The I2C core has a mutex on to protect the bus
 * - There are no read/modify/write operations
 * - Intervening operations between the write of the PWM register
 *   and the Update register are harmless.
 *
 * Example:
 *	PWM_REG_1 write 16
 *	UPDATE_REG write 0
 *	PWM_REG_2 write 128
 *	UPDATE_REG write 0
 *   vs:
 *	PWM_REG_1 write 16
 *	PWM_REG_2 write 128
 *	UPDATE_REG write 0
 *	UPDATE_REG write 0
 * are equivalent. Poking the Update register merely applies all PWM
 * register writes up to that point.
 */
static int is31fl32xx_brightness_set(struct led_classdev *led_cdev,
				     enum led_brightness brightness)
{
	const struct is31fl32xx_led_data *led_data =
		container_of(led_cdev, struct is31fl32xx_led_data, cdev);
	const struct is31fl32xx_chipdef *cdef = led_data->priv->cdef;
	u8 pwm_register_offset;
	int ret;

	//pr_info("%s: %x\n", __func__, brightness);

	/* NOTE: led_data->channel is 1-based */
	if (cdef->pwm_registers_reversed)
		pwm_register_offset = cdef->channels - led_data->channel;
	else
		pwm_register_offset = led_data->channel - 1;

	ret = is31fl32xx_write(led_data->priv,
			       cdef->pwm_register_base + pwm_register_offset,
			       brightness);
	if (ret)
		return ret;

	return is31fl32xx_write(led_data->priv, cdef->pwm_update_reg, 0);
}

static int is31fl32xx_reset_regs(struct is31fl32xx_priv *priv)
{
	const struct is31fl32xx_chipdef *cdef = priv->cdef;
	int ret;

	if (cdef->reset_reg != IS31FL32XX_REG_NONE) {
		ret = is31fl32xx_write(priv, cdef->reset_reg, 0);
		if (ret)
			return ret;
	}

	if (cdef->reset_func)
		return cdef->reset_func(priv);

	return 0;
}

static int is31fl32xx_software_shutdown(struct is31fl32xx_priv *priv,
					bool enable)
{
	const struct is31fl32xx_chipdef *cdef = priv->cdef;
	int ret;

	if (cdef->shutdown_reg != IS31FL32XX_REG_NONE) {
		u8 value = enable ? IS31FL32XX_SHUTDOWN_SSD_ENABLE :
				    IS31FL32XX_SHUTDOWN_SSD_DISABLE;
		ret = is31fl32xx_write(priv, cdef->shutdown_reg, value);
		if (ret)
			return ret;
	}

	if (cdef->sw_shutdown_func)
		return cdef->sw_shutdown_func(priv, enable);

	return 0;
}

static int is31fl32xx_init_regs(struct is31fl32xx_priv *priv)
{
	const struct is31fl32xx_chipdef *cdef = priv->cdef;
	int ret;

	ret = is31fl32xx_reset_regs(priv);
	if (ret)
		return ret;

	/*
	 * Set enable bit for all channels.
	 * We will control state with PWM registers alone.
	 */
	if (cdef->led_control_register_base != IS31FL32XX_REG_NONE) {
		u8 value =
		    GENMASK(cdef->enable_bits_per_led_control_register-1, 0);
		u8 num_regs = cdef->channels /
				cdef->enable_bits_per_led_control_register;
		int i;

		for (i = 0; i < num_regs; i++) {
			ret = is31fl32xx_write(priv,
				cdef->led_control_register_base+i,
				value);
			if (ret)
				return ret;
		}
	}

	ret = is31fl32xx_software_shutdown(priv, false);
	if (ret)
		return ret;

	if (cdef->global_control_reg != IS31FL32XX_REG_NONE) {
		ret = is31fl32xx_write(priv, cdef->global_control_reg, 0x00);
		if (ret)
			return ret;
	}

	if (cdef->pwm_frequency_set_reg) {
		ret = is31fl32xx_write(priv, cdef->pwm_frequency_set_reg, 0x01);
		if (ret)
			return ret;
	}

	return 0;
}

static inline size_t sizeof_is31fl32xx_priv(int num_leds)
{
	return sizeof(struct is31fl32xx_priv) +
		      (sizeof(struct is31fl32xx_led_data) * num_leds);
}

static int is31fl32xx_parse_child_dt(const struct device *dev,
				     const struct device_node *child,
				     struct is31fl32xx_led_data *led_data)
{
	struct led_classdev *cdev = &led_data->cdev;
	int ret = 0;
	u32 reg;

	if (of_property_read_string(child, "label", &cdev->name))
		cdev->name = child->name;

	ret = of_property_read_u32(child, "reg_offset", &reg);
	if (ret || reg < 1 || reg > led_data->priv->cdef->channels) {
		dev_err(dev,
			"Child node %s does not have a valid reg property\n",
			child->full_name);
		return -EINVAL;
	}
	led_data->channel = reg;

	of_property_read_string(child, "linux,default-trigger",
				&cdev->default_trigger);

	cdev->brightness_set_blocking = is31fl32xx_brightness_set;

	return 0;
}

static struct is31fl32xx_led_data *is31fl32xx_find_led_data(
					struct is31fl32xx_priv *priv,
					u8 channel)
{
	size_t i;

	for (i = 0; i < priv->num_leds; i++) {
		if (priv->leds[i].channel == channel)
			return &priv->leds[i];
	}

	return NULL;
}

static int is31fl32xx_parse_dt(struct device *dev,
			       struct is31fl32xx_priv *priv)
{
	struct device_node *child;
	int ret = 0;

	for_each_child_of_node(dev->of_node, child) {
		struct is31fl32xx_led_data *led_data =
			&priv->leds[priv->num_leds];
		const struct is31fl32xx_led_data *other_led_data;

		led_data->priv = priv;

		ret = is31fl32xx_parse_child_dt(dev, child, led_data);
		if (ret)
			goto err;

		/* Detect if channel is already in use by another child */
		other_led_data = is31fl32xx_find_led_data(priv,
							  led_data->channel);
		if (other_led_data) {
			dev_err(dev,
				"%s and %s both attempting to use channel %d\n",
				led_data->cdev.name,
				other_led_data->cdev.name,
				led_data->channel);
			goto err;
		}

		ret = devm_led_classdev_register(dev, &led_data->cdev);
		if (ret) {
			dev_err(dev, "failed to register PWM led for %s: %d\n",
				led_data->cdev.name, ret);
			goto err;
		}

		priv->num_leds++;
	}

	return 0;

err:
	of_node_put(child);
	return ret;
}

static const struct of_device_id of_is31fl32xx_match[] = {
	{ .compatible = "issi,is31fl3236", .data = &is31fl3236_cdef, },
	{ .compatible = "issi,is31fl3235", .data = &is31fl3235_cdef, },
	{ .compatible = "issi,is31fl3218", .data = &is31fl3218_cdef, },
	{ .compatible = "si-en,sn3218",    .data = &is31fl3218_cdef, },
	{ .compatible = "issi,is31fl3216", .data = &is31fl3216_cdef, },
	{ .compatible = "si-en,sn3216",    .data = &is31fl3216_cdef, },
	{},
};

MODULE_DEVICE_TABLE(of, of_is31fl32xx_match);

static long is31fl32xx_ioctl(struct file *file,
			unsigned int cmd,
			unsigned long args)
{
	int ret,i;

	switch (cmd) {
		del_timer(&led_priv_data->mtimer);
		case CMD_LEDRING_ARG:
			for (i=0; i< styleData.num*3; i++) {
				ret = is31fl32xx_write(led_priv_data->g_client, i+1, 0);
				if (ret) {
					pr_err("try close led: %d fail\n", i+1);
					return ret;
				}
				ret = is31fl32xx_write(led_priv_data->g_client, 37, 0);
				if (ret)
					pr_err("try close led: %d fail\n", i+1);
			}

			ret = copy_from_user(&styleData, (int *)args, sizeof(styleData));
			if (styleData.speed < 0) {
				pr_info("set speed level fail,use default speed!!\n");
				led_priv_data->ledring_speed = DEFAULT_SPEED;
			} else if (led_priv_data->ledring_speed != styleData.speed)
				led_priv_data->ledring_speed = styleData.speed;
			if (styleData.timeout != 0) {
				led_priv_data->timeout_flag = 1;
				led_priv_data->timeout = styleData.timeout*1000/led_priv_data->ledring_speed;
			} else led_priv_data->timeout_flag = 0;

			if (styleData.times < 0) styleData.times = 0;
		        led_priv_data->action_times = styleData.times;
			led_priv_data->m_index = 0;

			schedule_work(&led_priv_data->ledring_work);
			mod_timer(&led_priv_data->mtimer, jiffies+led_priv_data->ledring_speed*HZ/1000);
		break;
		default:
		break;
	}
	return 0;
}

static void mtimer_function(unsigned long data)
{
	schedule_work(&led_priv_data->ledring_work);
	mod_timer(&led_priv_data->mtimer, jiffies+led_priv_data->ledring_speed*HZ/1000);
}

static int leds_light(void)
{
	int i, j, k;
	int tmp, ret;

	for (i=0; i < styleData.num; i++) {
		k = (styleData.num-i)*3-2;
		for (j=0; j< 3; j++) {
			tmp = (styleData.style[led_priv_data->m_index][led_map[i]] >> (j*8)) & 0xFF;
			ret = is31fl32xx_write(led_priv_data->g_client, k+j, tmp);
			if (ret)
				pr_err("try lit led: %d fail\n", i+1);

			ret = is31fl32xx_write(led_priv_data->g_client, 37, 0);
			if (ret)
				pr_err("try update led data reg: %d fail\n", i+1);
		}
	}

	if(++led_priv_data->m_index > styleData.num-1) {
		if (styleData.times != 0) {
			if (--led_priv_data->action_times <= 0)
				del_timer(&led_priv_data->mtimer);
			led_priv_data->m_index = 0;
		} else
			led_priv_data->m_index = 0;
		led_priv_data->m_index = 0;
	}
	return 0;
}

static void ledring_work_func(
	struct work_struct *work)
{
	int ret, i;

	if (led_priv_data->timeout_flag == 0) {
		leds_light();
	} else {
		led_priv_data->timeout--;
		if (led_priv_data->timeout <= 0) {
			led_priv_data->timeout = 0;
			del_timer(&led_priv_data->mtimer);
			for (i=0; i< styleData.num*3; i++) {
				ret = is31fl32xx_write(led_priv_data->g_client, i+1, 0);
				if (ret)
					pr_err("try close led: %d fail\n", i+1);
				ret = is31fl32xx_write(led_priv_data->g_client, 37, 0);
				if (ret)
					pr_err("try close led: %d fail\n", i+1);
			}
		} else
			leds_light();
	}
}

static void setup_timer_task(void)
{
	setup_timer(&led_priv_data->mtimer, mtimer_function, 0);
	mod_timer(&led_priv_data->mtimer, jiffies+led_priv_data->ledring_speed*HZ/1000);
	INIT_WORK(&led_priv_data->ledring_work, ledring_work_func);
}

static const struct file_operations is31fl32xx_fops = {
	.owner = THIS_MODULE,
	.compat_ioctl = is31fl32xx_ioctl,
	.unlocked_ioctl = is31fl32xx_ioctl,
};

static int is31fl32xx_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	const struct is31fl32xx_chipdef *cdef;
	const struct of_device_id *of_dev_id;
	struct device *dev = &client->dev;
	struct is31fl32xx_priv *priv;
	int count, i;
	int try_times = 0;
	int ret = 0;

	pr_info("%s\n",__func__);
	of_dev_id = of_match_device(of_is31fl32xx_match, dev);
	if (!of_dev_id)
		return -EINVAL;

	cdef = of_dev_id->data;

	count = of_get_child_count(dev->of_node);
	if (!count)
		return -EINVAL;

	priv = devm_kzalloc(dev, sizeof_is31fl32xx_priv(count),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	led_priv_data = devm_kzalloc(dev, sizeof(struct _ledring),
			    GFP_KERNEL);
	if (!led_priv_data) {
		kfree(priv);
		return -ENOMEM;
	}

	led_priv_data->ledring_speed = DEFAULT_SPEED;

	priv->client = client;
	priv->cdef = cdef;
	i2c_set_clientdata(client, priv);
	for (i = 0; i < 3; i++) {
		ret = is31fl32xx_init_regs(priv);
		if (ret) {
			if (++try_times >= 3)
				goto err1;
		} else
			break;
	}
	ret = is31fl32xx_parse_dt(dev, priv);
	if (ret)
		goto err1;
	priv->major = register_chrdev(0, CHAR_DEV_NAME,
					&is31fl32xx_fops);
	priv->cls = class_create(THIS_MODULE, DEVICE_NAME);
	device_create(priv->cls, NULL, MKDEV(priv->major, 0), NULL, DEVICE_NAME);
	led_priv_data->g_client = priv;
	setup_timer_task();

	led_priv_data->running_kobj = kobject_create_and_add("ledrgb",
		kernel_kobj);
	if (!led_priv_data->running_kobj)
		goto err1;
	ret = sysfs_create_group(led_priv_data->running_kobj, &attr_group);
	if (ret)
		goto err2;

	return 0;
err1:
	return -ENOMEM;
err2:
	kobject_put(led_priv_data->running_kobj);
	return -ENOMEM;
}

static int is31fl32xx_remove(struct i2c_client *client)
{
	struct is31fl32xx_priv *priv = i2c_get_clientdata(client);

	device_destroy(priv->cls, MKDEV(priv->major, 0));
	class_destroy(priv->cls);
	unregister_chrdev(priv->major, CHAR_DEV_NAME);

	flush_work(&led_priv_data->ledring_work);
	del_timer(&led_priv_data->mtimer);
	kobject_put(led_priv_data->running_kobj);

	return is31fl32xx_reset_regs(priv);
}

/*
 * i2c-core (and modalias) requires that id_table be properly filled,
 * even though it is not used for DeviceTree based instantiation.
 */
static const struct i2c_device_id is31fl32xx_id[] = {
	{ "is31fl3236" },
	{ "is31fl3235" },
	{ "is31fl3218" },
	{ "sn3218" },
	{ "is31fl3216" },
	{ "sn3216" },
	{},
};

MODULE_DEVICE_TABLE(i2c, is31fl32xx_id);

static int is31fl32xx_resume(struct device *dev)
{
	pr_info("enter is31fl32xx_resume.\n");
	schedule_work(&led_priv_data->ledring_work);
	mod_timer(&led_priv_data->mtimer,
		jiffies+led_priv_data->ledring_speed*HZ/1000);

	return 0;
}

static int is31fl32xx_suspend(struct device *dev)
{
	int i, ret;

	pr_info("enter is31fl32xx_suspend.\n");
	del_timer(&led_priv_data->mtimer);
	for (i = 0; i < styleData.num*3; i++) {
		ret = is31fl32xx_write(led_priv_data->g_client, i+1, 0);
		if (ret) {
			pr_err("try close led: %d fail\n", i+1);
				return ret;
		}
		ret = is31fl32xx_write(led_priv_data->g_client, 37, 0);
	if (ret)
		pr_err("try close led: %d fail\n", i+1);
	}

	return 0;
}

static const struct dev_pm_ops is31fl32xx_pm = {
	.suspend = is31fl32xx_suspend,
	.resume = is31fl32xx_resume,
};

static struct i2c_driver is31fl32xx_driver = {
	.driver = {
		.name	= "is31fl32xx,aml",
		.owner = THIS_MODULE,
		.of_match_table = of_is31fl32xx_match,
		.pm = &is31fl32xx_pm,
	},
	.probe		= is31fl32xx_probe,
	.remove		= is31fl32xx_remove,
	.id_table	= is31fl32xx_id,
};
static int __init is31fl32xx_init(void)
{
	return i2c_add_driver(&is31fl32xx_driver);
}
static void __exit is31fl32xx_exit(void)
{
	i2c_del_driver(&is31fl32xx_driver);
}

arch_initcall(is31fl32xx_init);
module_exit(is31fl32xx_exit);
MODULE_AUTHOR("David Rivshin <drivshin@allworx.com>");
MODULE_DESCRIPTION("ISSI IS31FL32xx LED driver");
MODULE_LICENSE("GPL v2");
