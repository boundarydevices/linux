/*
 * drivers/amlogic/irblaster/meson-irblaster.h
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

#ifndef __LINUX_AML_IR_BLASTER_H
#define __LINUX_AML_IR_BLASTER_H


/* Amlogic AO_IR_BLASTER_ADDR0 bits */
#define BLASTER_BUSY						BIT(26)
#define BLASTER_FIFO_FULL					BIT(25)
#define BLASTER_FIFO_EMPTY					BIT(24)
#define BLASTER_FIFO_LEVEL					(0xff << 16)
#define BLASTER_MODULATOR_TB_SYSTEM_CLOCK	(0x0 << 12)
#define BLASTER_MODULATOR_TB_XTAL3_TICK		(0x1 << 12)
#define BLASTER_MODULATOR_TB_1US_TICK		(0x2 << 12)
#define BLASTER_MODULATOR_TB_10US_TICK		(0x3 << 12)
#define BLASTER_SLOW_CLOCK_DIV				(0xff << 4)
#define BLASTER_SLOW_CLOCK_MODE				BIT(3)
#define BLASTER_INIT_HIGH					BIT(2)
#define BLASTER_INIT_LOW					BIT(1)
#define BLASTER_ENABLE						BIT(0)

/* Amlogic AO_IR_BLASTER_ADDR1 bits */
#define BLASTER_MODULATION_LOW_COUNT(c)		((c) << 16)
#define BLASTER_MODULATION_HIGH_COUNT(c)	((c) << 0)

/* Amlogic AO_IR_BLASTER_ADDR2 bits */
#define BLASTER_WRITE_FIFO					BIT(16)
#define BLASTER_MODULATION_ENABLE			BIT(12)
#define BLASTER_TIMEBASE_1US				(0x0 << 10)
#define BLASTER_TIMEBASE_10US				(0x1 << 10)
#define BLASTER_TIMEBASE_100US				(0x2 << 10)
#define BLASTER_TIMEBASE_MODULATION_CLOCK	(0x3 << 10)

/* Amlogic AO_IR_BLASTER_ADDR3 bits */
#define BLASTER_FIFO_THD_PENDING			BIT(16)
#define BLASTER_FIFO_IRQ_ENABLE				BIT(8)
#define BLASTER_FIFO_IRQ_THRESHOLD(c)		(((c) & 0xff) << 0)

#define DEFAULT_CARRIER_FREQ				(38000)
#define DEFAULT_DUTY_CYCLE					(50)
#define BLASTER_DEVICE_COUNT				(32)

#define MAX_PLUSE							(1024)
#define PS_SIZE								(10)
#define LIMIT_DUTY							(25)
#define MAX_DUTY							(75)
#define LIMIT_FREQ							(25000)
#define MAX_FREQ							(60000)
#define COUNT_DELAY_MASK					(0X3ff)
#define TIMEBASE_SHIFT						(10)
#define BLASTER_KFIFO_SIZE					(4)

enum  {	/* Modulation level*/
	fisrt_low  = 0,
	fisrt_high = 1
};

struct blaster_kfifo {
	int size;
	unsigned int buffer[MAX_PLUSE];
};

struct aml_irblaster_dev {
	struct device *dev;
	struct work_struct blaster_work;
	struct class *blaster_class;
	struct cdev blaster_cdev;
	unsigned int count;
	unsigned int irq;
	unsigned int buffer_size;
	unsigned int buffer[MAX_PLUSE];
	unsigned int carrier_freqs;
	unsigned int duty_cycle;
	struct completion blaster_completion;
	void __iomem	*reg_base;
	void __iomem	*reset_base;
};

#define AO_IR_BLASTER_ADDR0 (0x0)
#define AO_IR_BLASTER_ADDR1 (0x4)
#define AO_IR_BLASTER_ADDR2 (0x8)
#define AO_IR_BLASTER_ADDR3 (0xc)
#define AO_RTI_GEN_CTNL_REG0 (0x0)

#define CONSUMERIR_TRANSMIT     0x5500
#define GET_CARRIER         0x5501
#define SET_CARRIER         0x5502
#define SET_DUTYCYCLE         0x5503
#endif

