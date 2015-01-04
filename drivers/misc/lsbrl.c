/*
 * LevelStar Braille Display Driver
 *
 * Copyright 2010-2011 LevelStar LLC.
 *
 * Based on SC11 cells from KGS
 *
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>

/*
 * Amount of time to wait for each pin change for power supply throttling
 */
#define DELAY_PER_PIN_CHANGE 1

/* Utility functions */

static unsigned char countBits[256] = {
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

static inline unsigned char pinsChanged(unsigned char c1, unsigned char c2)
{
	return countBits [c1^c2];
}

#define GP_DATA_BUF_EN	0
#define GP_CLK		1
#define GP_HV_EN	2
#define GP_MISO		3
#define GP_MOSI		4
#define GP_STROBE	5
#define GP_CNT		6

const char *gpio_names[] = {
[GP_DATA_BUF_EN] = "buf_en-gpios",
[GP_CLK] = "clock-gpios",
[GP_HV_EN] = "hv_en-gpios",
[GP_MISO] = "miso-gpios",
[GP_MOSI] = "mosi-gpios",
[GP_STROBE] = "strobe-gpios",
};

struct lsbrl {
	struct miscdevice miscdev;
	unsigned char len;
	unsigned char *contents;
	int	gpios[GP_CNT];
	enum of_gpio_flags gpio_flags[GP_CNT];
};

void set_gpio_state(struct lsbrl *l, int index, int active)
{
	if (gpio_is_valid(l->gpios[index])) {
		int state = (l->gpio_flags[index] & OF_GPIO_ACTIVE_LOW) ? 1 : 0;

		state ^= active;
		gpio_set_value(l->gpios[index], state);
//		pr_info("%s: active=%d, %s(%d)=0x%x\n", __func__, active, gpio_names[index], l->gpios[index], state);
	}
}

int get_gpio_state(struct lsbrl *l, int index)
{
	int val = 0;
	if (gpio_is_valid(l->gpios[index])) {
		int state = (l->gpio_flags[index] & OF_GPIO_ACTIVE_LOW) ? 1 : 0;

		val = gpio_get_value(l->gpios[index]);
		if (val >= 0)
			val ^= state;
	} else {
		pr_info("%s: invalid gpio %d\n", __func__, l->gpios[index]);
	}
	return val;
}

static int get_gpios(struct lsbrl *l, struct device *dev)
{
	int i;
	int ret;
	enum of_gpio_flags flags;

	for (i = 0; i < GP_CNT; i++) {
		l->gpios[i] = of_get_named_gpio_flags(dev->of_node, gpio_names[i], 0, &flags);
		if (!gpio_is_valid(l->gpios[i])) {
			pr_info("%s: gpio %s not available\n", __func__, gpio_names[i]);
		} else {
			int gflags = (flags & OF_GPIO_ACTIVE_LOW) ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;

			if (i == GP_MISO)
				gflags = GPIOF_IN;

			l->gpio_flags[i] = flags;
			pr_info("%s: %s flags(%d -> %d)\n", __func__, gpio_names[i], flags, gflags);
			ret = devm_gpio_request_one(dev, l->gpios[i], gflags, gpio_names[i]);
			if (ret < 0) {
				pr_info("%s: request of %s failed(%d)\n", __func__, gpio_names[i], ret);
				return ret;
			}
		}
	}
	return 0;
}

#define DELAY_US 5

void toggle_clock(struct lsbrl *l)
{
	udelay(DELAY_US);
	set_gpio_state(l, GP_CLK, 1);
	udelay(DELAY_US);
	set_gpio_state(l, GP_CLK, 0);
}

void do_strobe(struct lsbrl *l)
{
	udelay(DELAY_US);
	set_gpio_state(l, GP_STROBE, 1);
	udelay(DELAY_US);
	set_gpio_state(l, GP_STROBE, 0);
}

static inline unsigned char lsbrl_determine_length(struct lsbrl *l)
{
	unsigned int i;
	int val = 1;

	set_gpio_state(l, GP_STROBE, 0);
	set_gpio_state(l, GP_MOSI, 0);
	for (i = 0; i < 2048; i++)
		toggle_clock(l);

	for (i = 1; i < 2048; i++) {
		set_gpio_state(l, GP_MOSI, val);
		val ^= 1;
		toggle_clock(l);
		if (get_gpio_state(l, GP_MISO))
			break;
	}
	set_gpio_state(l, GP_MOSI, val);
	toggle_clock(l);
	do_strobe(l);

	if ((i >= 2048) || (i < 8)) {
		pr_warn("warn: using 18 for display len, miso=%d, loops=%i\n",
				get_gpio_state(l, GP_MISO), i);
		return 18;
	}
	pr_info("Braille display of length %d characters, bits(%d)\n", i/8, i);
	return i/8;
}

const unsigned char order[8] = {3, 4, 5, 7, 0, 1, 2, 6};

static inline int lsbrl_clock_bits(struct lsbrl *l, unsigned bits, int prev)
{
	unsigned char i;
	int val;

	/* Flip bits for KGS hardware */
	bits ^= 0XD2;

	for (i = 0; i < 8; i++) {
		val = (bits >> order[i]) & 1;
		if (val != prev) {
			set_gpio_state(l, GP_MOSI, val);
			prev = val;
		}
		toggle_clock(l);
	}
	return prev;
}

static unsigned int lsbrl_update(struct lsbrl *l, const unsigned char *new)
{
	unsigned char i = l->len;
	unsigned char *cur = l->contents;
	unsigned int bits_flipped = 0;
	int last = -1;
	int bits, prev_bits;
	int repeat = 2;

	set_gpio_state(l, GP_STROBE, 0);
	while (i--) {
		prev_bits = cur[i];
		cur[i] = bits = new[i];
		if (!repeat)
			bits_flipped += pinsChanged(prev_bits, bits);
		last = lsbrl_clock_bits(l, bits, last);
		if (repeat) {
			i++;
			repeat--;
		}
	}
	do_strobe(l);
	mdelay(bits_flipped * DELAY_PER_PIN_CHANGE);
	return bits_flipped;
}

static ssize_t lsbrl_write(struct file *filp, const char __user *data, size_t len, loff_t *pos)
{
	char kbuf[256];
	struct lsbrl *l = container_of(filp->private_data, struct lsbrl, miscdev);

	if (!l)
		return -EINVAL;
	if (len != l->len)
		return -EINVAL;
	if (copy_from_user(kbuf, data, len))
		return -EINVAL;
	lsbrl_update(l, kbuf);
	return len;
}

/* This empty function allows private_data initialization */
static int lsbrl_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations lsbrl_fops = {
	.owner = THIS_MODULE,
	.write = lsbrl_write,
	.open = lsbrl_open,
};

static int lsbrl_probe(struct platform_device *pdev) {
	struct lsbrl *l;
	int err;

	l = kzalloc(sizeof(struct lsbrl), GFP_KERNEL);
	if (!l)
		return -ENOMEM;
	l->miscdev.name = "lsbrl";
	l->miscdev.minor = MISC_DYNAMIC_MINOR;
	l->miscdev.fops = &lsbrl_fops;

	err = get_gpios(l, &pdev->dev);
	if (err)
		goto fail1;
	set_gpio_state(l, GP_DATA_BUF_EN, 1);

	l->len = lsbrl_determine_length(l);
	l->contents = kzalloc(l->len, GFP_KERNEL);
	if (!l->contents) {
		err = -ENOMEM;
		goto fail1;
	}


	platform_set_drvdata(pdev, l);
	err = misc_register(&l->miscdev);
	if (err)
		goto fail2;
	/* Enable the 200V supply */
	set_gpio_state(l, GP_HV_EN, 1);
	return 0;
fail2:
	kfree(l->contents);
fail1:
	/* Disable the 200V supply */
	set_gpio_state(l, GP_HV_EN, 0);
	set_gpio_state(l, GP_DATA_BUF_EN, 0);
	kfree(l);
	return err;
}

static int lsbrl_remove(struct platform_device *pdev)
{
	struct lsbrl *l = (struct lsbrl *)platform_get_drvdata(pdev);

	if (l) {
		misc_deregister(&l->miscdev);
		kfree(l->contents);
		kfree(l);
		set_gpio_state(l, GP_HV_EN, 0);
		set_gpio_state(l, GP_DATA_BUF_EN, 0);
	}
	return 0;
}

static int lsbrl_suspend(struct device *dev)
{
	struct lsbrl *l = dev_get_drvdata(dev);

	set_gpio_state(l, GP_HV_EN, 0);
	return 0;
}

static int lsbrl_resume(struct device *dev)
{
	struct lsbrl *l = dev_get_drvdata(dev);

	set_gpio_state(l, GP_HV_EN, 1);
	return 0;
}
static SIMPLE_DEV_PM_OPS(lsbrl_pm_ops, lsbrl_suspend, lsbrl_resume);

static struct of_device_id lsbrl_of_match[] = {
	{ .compatible = "ls-braille", },
	{ },
};
MODULE_DEVICE_TABLE(of, lsbrl_of_match);


static struct platform_driver lsbrl_driver  = {
	.probe = lsbrl_probe,
	.remove = lsbrl_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "ls-braille",
		.pm	= &lsbrl_pm_ops,
		.of_match_table = of_match_ptr(lsbrl_of_match),
	},
};

module_platform_driver(lsbrl_driver);
MODULE_LICENSE("GPL");
