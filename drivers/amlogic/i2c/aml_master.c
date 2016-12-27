/*
 * drivers/amlogic/i2c/aml_master.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#ifndef CONFIG_I2C
#error kkk
#endif
#include <linux/amlogic/i2c-amlogic.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/i2c-algo-bit.h>
#include <linux/hardirq.h>
#include <linux/export.h>
#include "aml_master.h"
#include <linux/module.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>

#ifdef CONFIG_OF
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of.h>
#else
#include <mach/gpio.h>
#include <mach/gpio_data.h>
#endif
int aml_i2c_device_num;
static struct aml_i2c_platform *aml_i2c_properties_list;
static int i2c_speed[] = {AML_I2C_SPEED_50K, AML_I2C_SPEED_100K,
	AML_I2C_SPEED_200K, AML_I2C_SPEED_300K, AML_I2C_SPEED_400K};

#define aml_i2c_dbg(i2c, fmt, args...)  { if (i2c->i2c_debug) \
	  pr_info("[i2c@%d] " fmt, i2c->master_no, ## args); }
#define aml_i2c_dump(i2c) \
	do {                    \
		if (i2c->i2c_debug) {     \
			pr_info("[i2c@%08lx] 0x%08x,0x%08x,0x%08x,0x%08x,", \
			(unsigned long)i2c->master_regs, \
			i2c->master_regs->i2c_ctrl, \
			i2c->master_regs->i2c_slave_addr, \
			i2c->master_regs->i2c_token_list_0, \
			i2c->master_regs->i2c_token_list_1); \
			pr_info("0x%08x,0x%08x,0x%08x,0x%08x\n", \
			i2c->master_regs->i2c_token_wdata_0, \
			i2c->master_regs->i2c_token_wdata_1, \
			i2c->master_regs->i2c_token_rdata_0, \
			i2c->master_regs->i2c_token_rdata_1); \
	} \
	} while (0)

static void aml_i2c_set_clk(struct aml_i2c *i2c, unsigned int speed)
{
	unsigned int i2c_clock_set;
	unsigned int sys_clk_rate;
	/* struct clk *sys_clk; */
	struct aml_i2c_reg_ctrl *ctrl;

	/* sys_clk = clk_get_sys("clk81", NULL); */
	clk_prepare_enable(i2c->clk);
	sys_clk_rate = clk_get_rate(i2c->clk);
	/* sys_clk_rate = get_mpeg_clk(); */

	i2c_clock_set = sys_clk_rate / speed;
	if (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR)
		> MESON_CPU_MAJOR_ID_GXBB) {
		i2c_clock_set >>= 1;
		ctrl = (struct aml_i2c_reg_ctrl *)&(i2c->master_regs->i2c_ctrl);
		if (i2c_clock_set > 0xfff)
			i2c_clock_set = 0xfff;
		ctrl->clk_delay = i2c_clock_set & 0x3ff;
		ctrl->unused.b.clk_delay_ext = i2c_clock_set >> 10;
		i2c->master_regs->i2c_slave_addr &= ~(0xfff<<16);
		i2c->master_regs->i2c_slave_addr |= (i2c_clock_set>>1)<<16;
		i2c->master_regs->i2c_slave_addr |= 1<<28;
		i2c->master_regs->i2c_slave_addr &= ~(0x3f<<8);
		/* no filter on scl&sda */
	} else{
		i2c_clock_set >>= 2;

	ctrl = (struct aml_i2c_reg_ctrl *)&(i2c->master_regs->i2c_ctrl);
	ctrl->clk_delay = i2c_clock_set & AML_I2C_CTRL_CLK_DELAY_MASK;
}
}

static void aml_i2c_set_platform_data(struct aml_i2c *i2c,
		struct aml_i2c_platform *plat)
{
	i2c->master_i2c_speed = plat->master_i2c_speed;
	i2c->wait_count = plat->wait_count;
	i2c->wait_ack_interval = plat->wait_ack_interval;
	i2c->wait_read_interval = plat->wait_read_interval;
	i2c->wait_xfer_interval = plat->wait_xfer_interval;
	i2c->mode = plat->use_pio & 3;
	i2c->irq = plat->use_pio >> 2;
	i2c->master_no = plat->master_no;
#ifdef ENABLE_GPIO_TRIGGER
	i2c->trig_gpio = -1;
#endif

	if (IS_ERR(plat->master_state_name))
		pr_info("error: no master_state_name\n");
	else
		i2c->master_state_name = plat->master_state_name;
}

static void aml_i2c_pinmux_master(struct aml_i2c *i2c)
{
#ifdef CONFIG_OF
#if 0
	i2c->p = devm_pinctrl_get_select(i2c->dev, i2c->master_state_name);
	if (IS_ERR(i2c->p)) {
		pr_info("set i2c pinmux error\n");
		i2c->p = NULL;
	}
#endif
#else
	pinmux_set(&i2c->master_pinmux);
#endif
}

/*set to gpio for -EIO & -ETIMEOUT?*/
static void aml_i2c_clr_pinmux(struct aml_i2c *i2c)
{
#ifdef CONFIG_OF
#if 0
	if (i2c->p)
		devm_pinctrl_put(i2c->p);
#endif
#else
	pinmux_clr(&i2c->master_pinmux);
#endif
}

static void aml_i2c_clear_token_list(struct aml_i2c *i2c)
{
	i2c->master_regs->i2c_token_list_0 = 0;
	i2c->master_regs->i2c_token_list_1 = 0;
	memset(i2c->token_tag, TOKEN_END, AML_I2C_MAX_TOKENS);
}

static void aml_i2c_set_token_list(struct aml_i2c *i2c)
{
	int i;
	unsigned int token_reg = 0;

	for (i = 0; i < AML_I2C_MAX_TOKENS; i++)
		token_reg |= i2c->token_tag[i]<<(i*4);

	i2c->master_regs->i2c_token_list_0 = token_reg;
}

/*poll status*/
static int aml_i2c_wait_ack(struct aml_i2c *i2c)
{
	int i, ret;
	struct aml_i2c_reg_ctrl *ctrl;

	for (i = 0; i < i2c->wait_count; i++) {
		ctrl = (struct aml_i2c_reg_ctrl *)&(i2c->master_regs->i2c_ctrl);
		if (ctrl->status == IDLE) {
			i2c->cur_token = ctrl->cur_token;
			ret = ctrl->error ? (-EIO) : 0;
			return ret;
		}
		if (i2c->mode == I2C_DELAY_MODE)
			cond_resched();
		udelay(i2c->wait_ack_interval);
	}

	return -ETIMEDOUT;
}

static void aml_i2c_get_read_data(struct aml_i2c *i2c, unsigned char *buf,
		size_t len)
{
	int i;
	unsigned long rdata0 = i2c->master_regs->i2c_token_rdata_0;
	unsigned long rdata1 = i2c->master_regs->i2c_token_rdata_1;

	for (i = 0; i < min_t(size_t, len, AML_I2C_MAX_TOKENS>>1); i++)
		*buf++ = (rdata0 >> (i*8)) & 0xff;

	for (; i < min_t(size_t, len, AML_I2C_MAX_TOKENS); i++)
		*buf++ = (rdata1 >> ((i - (AML_I2C_MAX_TOKENS>>1))*8)) & 0xff;
}

static void aml_i2c_fill_data(struct aml_i2c *i2c, unsigned char *buf,
							size_t len)
{
	int i;
	unsigned int wdata0 = 0;
	unsigned int wdata1 = 0;

	for (i = 0; i < min_t(size_t, len, AML_I2C_MAX_TOKENS>>1); i++)
		wdata0 |= (*buf++) << (i*8);

	for (; i < min_t(size_t, len, AML_I2C_MAX_TOKENS); i++)
		wdata1 |= (*buf++) << ((i - (AML_I2C_MAX_TOKENS>>1))*8);

	i2c->master_regs->i2c_token_wdata_0 = wdata0;
	i2c->master_regs->i2c_token_wdata_1 = wdata1;
}

static void aml_i2c_xfer_prepare(struct aml_i2c *i2c, unsigned int speed)
{
	aml_i2c_pinmux_master(i2c);
	aml_i2c_set_clk(i2c, speed);
}

static void aml_i2c_start_token_xfer(struct aml_i2c *i2c)
{
	aml_i2c_dump(i2c);
	i2c->master_regs->i2c_ctrl &= ~1;	/*clear*/
	i2c->master_regs->i2c_ctrl |= 1;	/*set*/
}

/*
 *our controller should send write data with slave addr in a token list,
 *so we can't do normal address, just set addr into addr reg
 */
static long aml_i2c_do_address(struct aml_i2c *i2c, unsigned int addr)
{
	i2c->cur_slave_addr = addr&0x7f;
	if (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR)
		> MESON_CPU_MAJOR_ID_GXBB) {
		i2c->master_regs->i2c_slave_addr &= ~0xff;
		i2c->master_regs->i2c_slave_addr |= i2c->cur_slave_addr<<1;
	} else
		i2c->master_regs->i2c_slave_addr = i2c->cur_slave_addr<<1;
	return 0;
}

static void aml_i2c_stop(struct aml_i2c *i2c)
{
	struct aml_i2c_reg_ctrl *ctrl;

	ctrl = (struct aml_i2c_reg_ctrl *)&(i2c->master_regs->i2c_ctrl);

	/* Controller has send the stop condition automatically when NACK error.
	 * We must not send again at here, otherwize, the CLK line will be
	 *pulled down.
	 */
	if (!ctrl->error) {
		aml_i2c_clear_token_list(i2c);
		i2c->token_tag[0] = TOKEN_STOP;
		aml_i2c_set_token_list(i2c);
		aml_i2c_start_token_xfer(i2c);
		if (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR)
			> MESON_CPU_MAJOR_ID_GXBB)
			aml_i2c_wait_ack(i2c);
		else
			udelay(i2c->wait_xfer_interval);
	}
	aml_i2c_clear_token_list(i2c);
}

static ssize_t aml_i2c_read(struct aml_i2c *i2c, unsigned char *buf,
							size_t len)
{
	int i;
	int ret;
	size_t rd_len;
	int tagnum = 0;

	if (!buf || !len)
		return -EINVAL;
	aml_i2c_clear_token_list(i2c);

	if (!(i2c->msg_flags & I2C_M_NOSTART)) {
		i2c->token_tag[tagnum++] = TOKEN_START;
		i2c->token_tag[tagnum++] = TOKEN_SLAVE_ADDR_READ;

		aml_i2c_set_token_list(i2c);
		aml_i2c_start_token_xfer(i2c);

		ret = aml_i2c_wait_ack(i2c);
		if (ret < 0)
			return ret;
		aml_i2c_clear_token_list(i2c);
	}

	while (len) {
		tagnum = 0;
		rd_len = min_t(size_t, len, AML_I2C_MAX_TOKENS);
		if (rd_len == 1)
			i2c->token_tag[tagnum++] = TOKEN_DATA_LAST;
		else{
			for (i = 0; i < rd_len-1; i++)
				i2c->token_tag[tagnum++] = TOKEN_DATA;
			if (len > rd_len)
				i2c->token_tag[tagnum++] = TOKEN_DATA;
			else
				i2c->token_tag[tagnum++] = TOKEN_DATA_LAST;
		}
		aml_i2c_set_token_list(i2c);
		aml_i2c_start_token_xfer(i2c);

		ret = aml_i2c_wait_ack(i2c);
		if (ret < 0)
			return ret;

		aml_i2c_get_read_data(i2c, buf, rd_len);
		len -= rd_len;
		buf += rd_len;

		aml_i2c_clear_token_list(i2c);
	}
	return 0;
}

static ssize_t aml_i2c_write(struct aml_i2c *i2c, unsigned char *buf,
							size_t len)
{
	int i;
	int ret;
	size_t wr_len;
	int tagnum = 0;

	if (!buf || !len)
		return -EINVAL;
	aml_i2c_clear_token_list(i2c);
	if (!(i2c->msg_flags & I2C_M_NOSTART)) {
		i2c->token_tag[tagnum++] = TOKEN_START;
		i2c->token_tag[tagnum++] = TOKEN_SLAVE_ADDR_WRITE;
	}
	while (len) {
		wr_len = min_t(size_t, len, AML_I2C_MAX_TOKENS-tagnum);
		for (i = 0; i < wr_len; i++)
			i2c->token_tag[tagnum++] = TOKEN_DATA;

		aml_i2c_set_token_list(i2c);

		aml_i2c_fill_data(i2c, buf, wr_len);

		aml_i2c_start_token_xfer(i2c);

		len -= wr_len;
		buf += wr_len;
		tagnum = 0;

		ret = aml_i2c_wait_ack(i2c);
		if (ret < 0)
			return ret;

		aml_i2c_clear_token_list(i2c);
	}
	return 0;
}

static struct aml_i2c_ops aml_i2c_m1_ops = {
	.xfer_prepare	= aml_i2c_xfer_prepare,
	.read		= aml_i2c_read,
	.write		= aml_i2c_write,
	.do_address	= aml_i2c_do_address,
	.stop		= aml_i2c_stop,
};

static ktime_t get_xfer_time(struct aml_i2c *i2c)
{
	int delay_us;
	ktime_t kt;

	delay_us = (1000000 * 9 * i2c->xfer_num) / (i2c->master_i2c_speed) + 10;
	kt = ktime_set(0, delay_us * 1000);
	return kt;
}

static int aml_i2c_handle_data(struct aml_i2c *i2c, int tag_num)
{
	if (!tag_num)
		aml_i2c_clear_token_list(i2c);
	i2c->xfer_num = 0;
	while ((i2c->remain_len > 0) && (tag_num < AML_I2C_MAX_TOKENS)) {
		i2c->token_tag[tag_num] = TOKEN_DATA;
		if ((i2c->msg_flags & I2C_M_RD) && (i2c->remain_len == 1))
			i2c->token_tag[tag_num] = TOKEN_DATA_LAST;
		tag_num++;
		i2c->xfer_num++;
		i2c->remain_len--;
	}
	if ((!i2c->msgs_num) && (!i2c->remain_len)) {
		/* the last msg and the last data */
		if (tag_num < AML_I2C_MAX_TOKENS) {
			/****************************
			 *the token list is not full, we can
			 *send the STOP together
			 ****************************/
			i2c->token_tag[tag_num] = TOKEN_STOP;
		} else {
			/* the token list is full, we have to sign a stop
			 *flag and wait next trasfer
			 */
			i2c->remain_len = -1;
		}
	} else if (i2c->remain_len < 0) {
		i2c->remain_len = 0;
		i2c->token_tag[tag_num] = TOKEN_STOP;
	}

	aml_i2c_set_token_list(i2c);
	if (!(i2c->msg_flags & I2C_M_RD) && i2c->xfer_num) {
		aml_i2c_fill_data(i2c, i2c->xfer_buf, i2c->xfer_num);
		i2c->xfer_buf += i2c->xfer_num;
	}
	aml_i2c_start_token_xfer(i2c);
	if (i2c->mode == I2C_TIMER_POLLING_MODE)
		hrtimer_set_expires(&i2c->aml_i2c_hrtimer, get_xfer_time(i2c));
	return i2c->xfer_num;
}

static int aml_i2c_handle_msg(struct aml_i2c *i2c, struct i2c_msg *msg)
{
	int tag_num = 0;

	aml_i2c_dbg(i2c, "msg%d: addr=0x%x, flag=0x%x, len=%d\n",
				i2c->msgs_num, msg->addr, msg->flags, msg->len);
	i2c->msg_flags = msg->flags;
	i2c->remain_len = msg->len;
	i2c->xfer_buf = msg->buf;
	i2c->msgs++;
	i2c->msgs_num--;
	i2c->ops->do_address(i2c, msg->addr);
	aml_i2c_clear_token_list(i2c);
	if (!(i2c->msg_flags & I2C_M_NOSTART))
		i2c->token_tag[tag_num++] = TOKEN_START;
	i2c->token_tag[tag_num++] =
	(i2c->msg_flags & I2C_M_RD) ? TOKEN_SLAVE_ADDR_READ
		: TOKEN_SLAVE_ADDR_WRITE;

	aml_i2c_handle_data(i2c, tag_num);
	/* pr_info("i2c msg: flag=%x, len=%d, addr=%x\n",
	 *p->flags, p->len, p->addr);
	 */
	return 0;
}

static int aml_i2c_int_xfer(struct aml_i2c *i2c, struct i2c_msg *msgs, int num)
{
	int ret;

	i2c->state = I2C_STATE_WORKING;
	i2c->msgs = msgs;
	i2c->msgs_num = num;
	aml_i2c_handle_msg(i2c, i2c->msgs);

	if (i2c->mode == I2C_TIMER_POLLING_MODE)
		hrtimer_start(&i2c->aml_i2c_hrtimer, get_xfer_time(i2c),
			HRTIMER_MODE_REL);

	wait_for_completion_interruptible_timeout(&i2c->aml_i2c_completion,
		msecs_to_jiffies(50));
	if (i2c->state == I2C_STATE_IDLE)
		ret = 0;
	else if (i2c->state == I2C_STATE_WORKING)
		ret = -ETIME;
	else
		ret = i2c->state;
	i2c->state = I2C_STATE_IDLE;
	return ret;
}

static irqreturn_t aml_i2c_complete_isr(int irq, void *dev_id)
{
	int ret;
	struct aml_i2c *i2c = (struct aml_i2c *)dev_id;

	if (i2c->state != I2C_STATE_WORKING) {
		i2c->state = I2C_STATE_IDLE;
		return IRQ_HANDLED;
	}

	ret = aml_i2c_wait_ack(i2c);
	if (ret < 0) {
		i2c->state = ret;
		complete(&i2c->aml_i2c_completion);
		return IRQ_HANDLED;
	}

	if ((i2c->msg_flags & I2C_M_RD) && (i2c->xfer_num)) {
		aml_i2c_get_read_data(i2c, i2c->xfer_buf, i2c->xfer_num);
		i2c->xfer_buf += i2c->xfer_num;
	}

	if (i2c->remain_len) {
		aml_i2c_handle_data(i2c, 0);
	} else if (i2c->msgs_num > 0) {
		aml_i2c_handle_msg(i2c, i2c->msgs);
	} else {
		i2c->state = I2C_STATE_IDLE;
		complete(&i2c->aml_i2c_completion);
	}

	return IRQ_HANDLED;
}

static enum hrtimer_restart aml_i2c_hrtimer_notify(struct hrtimer *hrtimer)
{
	int ret;
	struct aml_i2c *i2c = container_of(hrtimer,
		struct aml_i2c, aml_i2c_hrtimer);

	if (i2c->state != I2C_STATE_WORKING) {
		i2c->state = I2C_STATE_IDLE;
		return HRTIMER_NORESTART;
	}

	ret = aml_i2c_wait_ack(i2c);
	if (ret < 0) {
		i2c->state = ret;
		complete(&i2c->aml_i2c_completion);
		return HRTIMER_NORESTART;
	}

	if ((i2c->msg_flags & I2C_M_RD) && (i2c->xfer_num)) {
		aml_i2c_get_read_data(i2c, i2c->xfer_buf, i2c->xfer_num);
		i2c->xfer_buf += i2c->xfer_num;
	}

	if (i2c->remain_len) {
		aml_i2c_handle_data(i2c, 0);
		return HRTIMER_RESTART;
	} else if (i2c->msgs_num > 0) {
		aml_i2c_handle_msg(i2c, i2c->msgs);
		return HRTIMER_RESTART;
	}

	i2c->state = I2C_STATE_IDLE;
	complete(&i2c->aml_i2c_completion);
	return HRTIMER_NORESTART;
}

/*General i2c master transfer*/
static int aml_i2c_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg *msgs,
							int num)
{
	struct aml_i2c *i2c = i2c_get_adapdata(i2c_adap);
	struct i2c_msg *p = NULL;
	unsigned int i;
	unsigned int ret = 0, speed = 0;

	if (!msgs || !num)
		return -1;
	WARN_ON(!i2c);
	/*should not use spin_lock, cond_resched in wait ack*/
	mutex_lock(i2c->lock);

	if (i2c->state != I2C_STATE_IDLE) {
		mutex_unlock(i2c->lock);
		return -EPERM;
	}

	speed = (msgs->flags>>1) & 0x7;
	if (speed > 0 && speed < 6) {
		ret = i2c->master_i2c_speed;
		i2c->master_i2c_speed = i2c_speed[speed-1];
		speed = ret;
	} else {
		speed = 0;
	}

	i2c->ops->xfer_prepare(i2c, i2c->master_i2c_speed);
	/*make sure change speed before start*/
	mb();
	if (i2c->mode != I2C_DELAY_MODE) {
		ret = aml_i2c_int_xfer(i2c, &msgs[0], num);
	} else {
		for (i = 0; !ret && i < num; i++) {
			p = &msgs[i];
			i2c->msg_flags = p->flags;
			aml_i2c_dbg(i2c, "msg%d: addr=0x%x, flag=0x%x, len=%d\n",
						i, p->addr, p->flags, p->len);
			ret = i2c->ops->do_address(i2c, p->addr);
			if (ret || !p->len)
				continue;
			if (p->flags & I2C_M_RD)
				ret = i2c->ops->read(i2c, p->buf, p->len);
			else
				ret = i2c->ops->write(i2c, p->buf, p->len);
		}
		i2c->ops->stop(i2c);
	}

	if (speed)
		i2c->master_i2c_speed = speed;
	aml_i2c_clr_pinmux(i2c);

	if (ret) {
#ifdef ENABLE_GPIO_TRIGGER
		if (i2c->trig_gpio >= 0) {
			i = gpiod_get_value(i2c->i2c_gdesc);
			gpiod_direction_output(i2c->i2c_gdesc, !i);
		}
#endif
		dev_err(&i2c_adap->dev,
			"[aml_i2c_xfer] error ret = %d (%s)",
			ret, ret == -EIO ? "-EIO" : "-ETIMEOUT");
		dev_err(&i2c_adap->dev,
			"token %d, master_no(%d) %dK addr 0x%x\n",
			i2c->cur_token, i2c->master_no,
			i2c->master_i2c_speed/1000,
			i2c->cur_slave_addr);
		aml_i2c_dump(i2c);
	}

	mutex_unlock(i2c->lock);
	/* Return the number of messages processed, or the error code*/
	ret = ret ? (-EAGAIN) : num;
	return ret;
}

static u32 aml_i2c_func(struct i2c_adapter *i2c_adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm aml_i2c_algorithm = {
	.master_xfer = aml_i2c_xfer,
	.functionality = aml_i2c_func,
};

/***************i2c class****************/

static int aml_i2c_set_mode(struct aml_i2c *i2c, int mode, int irq)
{
	int err = 0;

	if (mode > I2C_TIMER_POLLING_MODE) {
		pr_info("error mode\n");
		err = -EINVAL;
	} else if (mode == i2c->mode) {
		pr_info("already in mode %d!\n", mode);
		err = -EINVAL;
	} else if (mode == I2C_INTERRUPT_MODE) {
		err = request_irq(irq, aml_i2c_complete_isr,
			IRQF_SHARED, "aml_i2c", i2c);
		pr_info("request irq#%d %s!\n", irq,
			err ? "failed":"success");
	} else if (mode == I2C_TIMER_POLLING_MODE) {
		hrtimer_init(&i2c->aml_i2c_hrtimer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		i2c->aml_i2c_hrtimer.function = aml_i2c_hrtimer_notify;
	}

	if (!err) {
		if (i2c->mode == I2C_INTERRUPT_MODE)
			free_irq(i2c->irq, i2c);
		else if (i2c->mode == I2C_TIMER_POLLING_MODE)
			hrtimer_cancel(&i2c->aml_i2c_hrtimer);
		pr_info("change mode from %d to %d\n",
			i2c->mode, mode);
		i2c->irq = irq;
		i2c->mode = mode;
	}
	return err;
}

static int aml_i2c_test_slave(struct aml_i2c *i2c, int argn, char *argv[])
{
	struct i2c_msg msgs[2], *msgp =  &msgs[0];
	int msg_num = 0;
	unsigned int addr = 0, wnum = 0, rnum = 0;
	char wbuf[8] = {0}, rbuf[64] = {0};
	int i;
	bool nostart;
	ssize_t	status;
	long value;

	if (argn < 3)
		goto err_arg;
	status = kstrtol(argv[0], 16, &value);
	if (status == 0)
		addr = (unsigned int)value;
	status = kstrtol(argv[1], 0, &value);
	if (status == 0)
		wnum = (unsigned int)value;
	status = kstrtol(argv[2], 0, &value);
	if (status == 0)
		rnum = (unsigned int)value;
	nostart = (rnum & 0x80) ? I2C_M_NOSTART : 0;
	rnum &= 0x7f;
	pr_info("addr=0x%x, wnum=%d, rnum=%d\n", addr, wnum, rnum);
	if (addr > 0x7f  || argn != (wnum+3) || !(wnum | rnum)
		|| wnum > ARRAY_SIZE(wbuf) || rnum > ARRAY_SIZE(rbuf)) {
		goto err_arg;
	}
	for (i = 0; i < wnum; i++) {
		status = kstrtol(argv[i+3], 16, &value);
		if (status == 0)
			wbuf[i] = (unsigned int)value;
	}
	if (wnum) {
		msgp->flags = !I2C_M_RD;
		msgp->addr  = addr;
		msgp->len   = wnum;
		msgp->buf   = wbuf;
		msg_num++;
		msgp++;
	}
	if (rnum) {
		msgp->flags = I2C_M_RD | nostart;
		msgp->addr  = addr;
		msgp->len   = rnum;
		msgp->buf   = rbuf;
		msg_num++;
		msgp++;
	}

	if (i2c_transfer(&i2c->adap, &msgs[0], msg_num) == msg_num) {
		for (i = 0; i < rnum; i++)
			pr_info("0x%x, ", rbuf[i]);
		pr_info("test ok!\n");
	} else {
		pr_info("test failed!\n");
		return -1;
	}
	return 0;

err_arg:
	pr_info("error argurment\n");
	return -1;
}

#ifdef ENABLE_GPIO_TRIGGER
static int aml_i2c_set_trig_gpio(struct aml_i2c *i2c, char *gpio_name)
{
	mutex_lock(i2c->lock);
	if (i2c->i2c_gdesc) {
		pr_info("free old trig_gpio(%d)!\n", i2c->trig_gpio);
		gpiod_put(i2c->i2c_gdesc);
		i2c->trig_gpio = -1;
	}
	i2c->i2c_gdesc = gpiod_get(i2c->dev, gpio_name, GPIOD_OUT_LOW);
	if (IS_ERR(i2c->i2c_gdesc)) {
		pr_info("error: %s cannot map to gpio!\n",
			gpio_name);
	} else {
		pr_info("success: select %s as trig_gpio!\n",
			gpio_name);
		gpiod_direction_output(i2c->i2c_gdesc, 1);
	}
	mutex_unlock(i2c->lock);
	return 0;
}
#endif

static ssize_t show_aml_i2c(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret;
	struct aml_i2c *i2c = container_of(class, struct aml_i2c, cls);

	if (!strcmp(attr->attr.name, "speed"))
		ret = sprintf(buf, "speed=%d\n", i2c->master_i2c_speed);

	else if (!strcmp(attr->attr.name, "mode"))
		ret = sprintf(buf, "mode=%d, irq=%d\n", i2c->mode, i2c->irq);

	else if (!strcmp(attr->attr.name, "debug"))
		ret = sprintf(buf, "debug=%d\n", i2c->i2c_debug);

	else if (!strcmp(attr->attr.name, "slave"))
		ret = sprintf(buf, "slave=%d\n", i2c->cur_slave_addr);

#ifdef ENABLE_GPIO_TRIGGER
	else if (!strcmp(attr->attr.name, "trig_gpio"))
		ret = sprintf(buf, "trig_gpio=%d\n", i2c->trig_gpio);
#endif

	else {
		pr_info("error attribute access\n");
		ret = 0;
	}

	return ret;
}

#define MAX_ARG_NUM 16
static ssize_t store_aml_i2c(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	struct aml_i2c *i2c = container_of(class, struct aml_i2c, cls);
	char *kbuf, *p, *argv[MAX_ARG_NUM];
	int argn;
	unsigned int val = 0, val2 = 0;
	ssize_t	status;
	long value;

	kbuf = kstrdup(buf, GFP_KERNEL);
	p = kbuf;
	for (argn = 0; argn < MAX_ARG_NUM; argn++) {
		argv[argn] = strsep(&p, " ");
		if (argv[argn] == NULL)
			break;
		/* pr_info("argv[%d]=%s\n", argn, argv[argn]); */
	}
	status = kstrtol(argv[0], 0, &value);
	if (status == 0)
		val = (unsigned int)value;
	val = argn ? val : 0;
	status = kstrtol(argv[1], 0, &value);
	if (status == 0)
		val2 = (unsigned int)value;
	val2 = (argn > 1) ? val2 : 0;

	if (!strcmp(attr->attr.name, "speed")) {
		mutex_lock(i2c->lock);
		i2c->master_i2c_speed = val;
		mutex_unlock(i2c->lock);
		pr_info("set speed: %d\n", i2c->master_i2c_speed);
	}

	else if (!strcmp(attr->attr.name, "mode")) {
		mutex_lock(i2c->lock);
		aml_i2c_set_mode(i2c, val, val2);
		mutex_unlock(i2c->lock);
	}

	else if (!strcmp(attr->attr.name, "debug")) {
		mutex_lock(i2c->lock);
		i2c->i2c_debug = !!val;
		mutex_unlock(i2c->lock);
		pr_info("%s debug\n", i2c->i2c_debug ?
			"enable" : "disable");
	}

	else if (!strcmp(attr->attr.name, "slave")) {
		aml_i2c_test_slave(i2c, argn, argv);
	}
#ifdef ENABLE_GPIO_TRIGGER
	else if (!strcmp(attr->attr.name, "trig_gpio"))
		aml_i2c_set_trig_gpio(i2c, argv[0]);
#endif

	kfree(kbuf);
	return count;
}

static struct class_attribute i2c_class_attrs[] = {
	__ATTR(speed, 0644, show_aml_i2c, store_aml_i2c),
	__ATTR(mode, 0644, show_aml_i2c, store_aml_i2c),
	__ATTR(debug, 0644, show_aml_i2c, store_aml_i2c),
	__ATTR(slave, 0644, show_aml_i2c, store_aml_i2c),
#ifdef ENABLE_GPIO_TRIGGER
	__ATTR(trig_gpio, 0644, show_aml_i2c, store_aml_i2c),
#endif
	__ATTR_NULL
};

#ifdef CONFIG_OF
static const struct of_device_id meson_i2c_dt_match[];
#endif
static inline struct aml_i2c_platform   *aml_get_driver_data(
			struct platform_device *pdev)
{
#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_node(meson_i2c_dt_match, pdev->dev.of_node);
		return (struct aml_i2c_platform *)match->data;
	}
#endif
	return (struct aml_i2c_platform *)
			platform_get_device_id(pdev)->driver_data;
}



static int aml_i2c_probe(struct platform_device *pdev)
{
	int ret;
	struct aml_i2c_platform *plat;
	struct resource *res;
	int device_id =  -1;
	struct reset_control *rst;

	resource_size_t *res_start;
	struct aml_i2c *i2c = kzalloc(sizeof(struct aml_i2c), GFP_KERNEL);

	struct aml_i2c_platform *aml_i2c_property =
		kzalloc(sizeof(struct aml_i2c_platform), GFP_KERNEL);
	/* printk(KERN_DEBUG "%s : %s\n", __FILE__, __FUNCTION__); */

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "no platform data\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "device_id", &device_id);
	if (ret) {
		dev_err(&pdev->dev, "don't find to match device_id\n");
		return -1;
	}

	pdev->id = device_id;

	if (!aml_i2c_property)
		dev_err(&pdev->dev, "can't alloc mem for i2c_property\n");
	else{
		ret = of_property_read_u32(pdev->dev.of_node, "use_pio",
			&(aml_i2c_property->use_pio));
		if (ret) {
			dev_info(&pdev->dev, "not find match use_pio, use default\n");
			aml_i2c_property->use_pio = 0;
		}

		ret = of_property_read_u32(pdev->dev.of_node,
			"master_i2c_speed",
			&(aml_i2c_property->master_i2c_speed));
		if (ret) {
			dev_info(&pdev->dev, "not find match master_i2c_speed, use default\n");
			if (aml_i2c_device_num == 0)
				aml_i2c_property->master_i2c_speed = 100000;
			else
				aml_i2c_property->master_i2c_speed = 300000;
		}

		aml_i2c_property->wait_count		= 50000;
		aml_i2c_property->wait_ack_interval	= 5;
		aml_i2c_property->wait_read_interval = 5;
		aml_i2c_property->wait_xfer_interval = 5;
		aml_i2c_property->master_no          = device_id;
		aml_i2c_property->master_state_name  = NULL;
	}

	if (aml_i2c_device_num == 0) {
		aml_i2c_properties_list = aml_i2c_property;
		INIT_LIST_HEAD(&aml_i2c_properties_list->list);
	} else{
		list_add_tail(&aml_i2c_property->list,
			&aml_i2c_properties_list->list);
	}
	aml_i2c_device_num++;

	plat = (struct aml_i2c_platform *)aml_i2c_property;

	ret = of_property_read_string(pdev->dev.of_node, "pinctrl-names",
		&plat->master_state_name);
	dev_dbg(&pdev->dev, "plat->state_name:%s\n", plat->master_state_name);

	i2c->ops = &aml_i2c_m1_ops;
	i2c->dev =  &pdev->dev;

  /* find the clock and enable it */
	rst = devm_reset_control_get(&pdev->dev, NULL);
	if (!IS_ERR(rst))
		reset_control_deassert(rst);
	i2c->clk = devm_clk_get(&pdev->dev, "clk_i2c");
	if (IS_ERR(i2c->clk)) {
		dev_err(&pdev->dev, "cannot get clock\n");
		return -ENOENT;
	}

	dev_dbg(&pdev->dev, "clock source %p\n", i2c->clk);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (IS_ERR(res))
		return PTR_ERR(res);

	res_start = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(res_start))
		return PTR_ERR(res_start);

/* res_start = of_iomap(pdev->dev.of_node,0); */
	i2c->master_regs = (struct aml_i2c_reg_master __iomem *)(res_start);

	WARN_ON(!i2c->master_regs);
	WARN_ON(!plat);
	aml_i2c_set_platform_data(i2c, plat);
	dev_dbg(&pdev->dev, "master_no = %d, master_regs=%p\n",
		i2c->master_no, i2c->master_regs);

	i2c->p = devm_pinctrl_get_select(i2c->dev, i2c->master_state_name);
	if (IS_ERR(i2c->p)) {
		dev_err(&pdev->dev, "set i2c pinmux error\n");
		i2c->p = NULL;
	}

	i2c->lock = kzalloc(sizeof(struct mutex), GFP_KERNEL);
	mutex_init(i2c->lock);

	/*setup adapter*/
	i2c->adap.nr = pdev->id ==  -1 ? 0 : pdev->id;
	i2c->adap.class = I2C_CLASS_HWMON;
	i2c->adap.algo = &aml_i2c_algorithm;
	i2c->adap.retries = 2;
	i2c->adap.timeout = 5;

	i2c->adap.dev.of_node = pdev->dev.of_node;

	sprintf(i2c->adap.name, ADAPTER_NAME"%d", i2c->adap.nr);
	i2c_set_adapdata(&i2c->adap, i2c);
	ret = i2c_add_numbered_adapter(&i2c->adap);
	if (ret < 0) {
		dev_err(&pdev->dev, "Adapter %s registration failed\n",
	      i2c->adap.name);
		kzfree(i2c);
		return -1;
	}
	dev_info(&pdev->dev, "add adapter %s(%p)\n",
		i2c->adap.name, &i2c->adap);

	i2c->state = I2C_STATE_IDLE;
	init_completion(&i2c->aml_i2c_completion);
	if (i2c->mode == I2C_TIMER_POLLING_MODE) {
		hrtimer_init(&i2c->aml_i2c_hrtimer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		i2c->aml_i2c_hrtimer.function = aml_i2c_hrtimer_notify;
		dev_info(&pdev->dev, "master %d work in timer polling mode\n",
			device_id);
	} else if (i2c->mode == I2C_INTERRUPT_MODE) {
		ret = request_irq(i2c->irq, aml_i2c_complete_isr,
				IRQF_SHARED, "aml_i2c", i2c);
		if (ret < 0) {
			dev_err(&pdev->dev, "master %d request irq(%d) failed!\n",
				device_id, i2c->irq);
			i2c->mode = I2C_DELAY_MODE;
		} else {
			dev_info(&pdev->dev, "master %d work in interrupt mode(irq=%d)\n",
				device_id, i2c->irq);
		}
	}
    /*setup class*/
	i2c->cls.name = kzalloc(NAME_LEN, GFP_KERNEL);
	if (i2c->adap.nr)
		sprintf((char *)i2c->cls.name, "i2c%d", i2c->adap.nr);
	else
		sprintf((char *)i2c->cls.name, "i2c");
	i2c->cls.class_attrs = i2c_class_attrs;
	ret = class_register(&i2c->cls);
	if (ret)
		dev_err(&pdev->dev, " class register i2c_class fail!\n");

	return 0;
}

static int aml_i2c_remove(struct platform_device *pdev)
{
	struct aml_i2c *i2c = platform_get_drvdata(pdev);

	if (i2c->mode == I2C_INTERRUPT_MODE)
		free_irq(i2c->irq, i2c);
	if (i2c->mode == I2C_TIMER_POLLING_MODE)
		hrtimer_cancel(&i2c->aml_i2c_hrtimer);
	mutex_destroy(i2c->lock);
	i2c_del_adapter(&i2c->adap);
	kzfree(i2c);
	i2c = NULL;
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id meson_i2c_dt_match[] = {
	{	.compatible = "amlogic, meson-i2c",
	},
	{},
};
#else
#define meson_i2c_dt_match NULL
#endif

static	int aml_i2c_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct i2c_adapter *adapter;
	struct aml_i2c *i2c;

	pr_info("%s\n", __func__);
	adapter = i2c_get_adapter(pdev->id ==  -1 ? 0 : pdev->id);
	WARN_ON(!adapter);
	i2c = i2c_get_adapdata(adapter);
	WARN_ON(!i2c);
	if (i2c->mode == I2C_INTERRUPT_MODE) {
		mutex_lock(i2c->lock);
		i2c->state = I2C_STATE_SUSPEND;
		disable_irq(i2c->irq);
		mutex_unlock(i2c->lock);
		pr_info("%s: disable #%d irq\n", __func__, i2c->irq);
	}

	return 0;
}

static	int aml_i2c_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct i2c_adapter *adapter;
	struct aml_i2c *i2c;

	pr_info("%s\n", __func__);
	adapter = i2c_get_adapter(pdev->id ==  -1 ? 0 : pdev->id);
	WARN_ON(!adapter);
	i2c = i2c_get_adapdata(adapter);
	WARN_ON(!i2c);
	if (i2c->mode == I2C_INTERRUPT_MODE) {
		mutex_lock(i2c->lock);
		i2c->state = I2C_STATE_IDLE;
		enable_irq(i2c->irq);
		mutex_unlock(i2c->lock);
		pr_info("%s: enable #%d irq\n", __func__, i2c->irq);
	}
	return 0;
}

static const struct dev_pm_ops aml_i2c_pm_ops = {
	.suspend_noirq = aml_i2c_suspend,
	.resume_noirq  = aml_i2c_resume,
};

static struct platform_driver aml_i2c_driver = {
	.probe = aml_i2c_probe,
	.remove = aml_i2c_remove,
	.driver = {
	.name = "aml-i2c",
	.owner = THIS_MODULE,
	.of_match_table = meson_i2c_dt_match,
	.pm = &aml_i2c_pm_ops,
	},
/* .suspend = aml_i2c_suspend, */
/* .resume = aml_i2c_resume, */
};

static int __init aml_i2c_init(void)
{
	int ret;

	pr_info("aml_i2c version: 20140813\n");
	ret = platform_driver_register(&aml_i2c_driver);
	return ret;
}

static void __exit aml_i2c_exit(void)
{
	platform_driver_unregister(&aml_i2c_driver);
}

arch_initcall(aml_i2c_init);
module_exit(aml_i2c_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("I2C driver for amlogic");
MODULE_LICENSE("GPL");

