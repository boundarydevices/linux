/*
 * drivers/amlogic/input/remote/remote_decoder_xmp.c
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

#include <linux/bitrev.h>
#include <linux/module.h>
#include "remote_meson.h"

#define XMP_UNIT		  136000 /* ns */
#define XMP_LEADER		  210000 /* ns */
#define XMP_NIBBLE_PREFIX	  760000 /* ns */
#define	XMP_HALFFRAME_SPACE	13800000 /* ns */

/* should be 80ms but not all dureation supliers can go that high */
#define	XMP_TRAILER_SPACE	20000000

enum xmp_state {
	STATE_INACTIVE,
	STATE_LEADER_PULSE,
	STATE_NIBBLE_SPACE,
};

struct nibble_win {
	u8 value;
	int min;
	int max;
};

struct xmp_dec {
	int state;
	unsigned int count;
	u32 durations[16];
};

static const struct nibble_win nibble_windows[] = {
	{0, 690000, 826000},   /*758000*/
	{1, 827000, 963000},   /*895000*/
	{2, 964000, 1100000},   /*1032000*/
	{3, 1110000, 1237000},   /*1169000*/
	{4, 1238000, 1374000},   /*1306000*/
	{5, 1375000, 1511000},   /*1443000*/
	{6, 1512000, 1648000},   /*1580000*/
	{7, 1649000, 1785000},   /*1717000*/
	{8, 1786000, 1922000},   /*1854000*/
	{9, 1923000, 2059000},   /*1991000*/
	{0xa, 2060000, 2196000}, /*2128000*/
	{0xb, 2197000, 2333000}, /*2265000*/
	{0xc, 2334000, 2470000}, /*2402000*/
	{0xd, 2471000, 2607000}, /*2539000*/
	{0xe, 2608000, 2744000}, /*2676000*/
	{0x0f, 2745000, 2881000}, /*2813000*/
	{0xff, 11800000, 16800000} /*13800000 ,half frame space*/
};

int decode_xmp(struct remote_dev *dev,
	struct xmp_dec *data, struct remote_raw_event *ev)
{
	int  i;
	u8 addr, subaddr, subaddr2, toggle, oem, obc1, obc2, sum1, sum2;
	u32 *n;
	u32 scancode;
	int custom_code;
	int nb = 0;
	char buf[512];

	if (data->count != 16) {
		sprintf(buf, "rx TRAILER c=%d, d=%d\n",
			data->count, ev->duration);
		debug_log_printk(dev, buf);
		data->state = STATE_INACTIVE;
		return -EINVAL;
	}

	n = data->durations;
	for (i = 0; i < 16; i++) {
		for (nb = 0; nb < 16; nb++) {
			if (n[i] >= nibble_windows[nb].min &&
				n[i] <= nibble_windows[nb].max) {
				n[i] = nibble_windows[nb].value;
			}
		}
	}
	sum1 = (15 + n[0] + n[1] + n[2] + n[3] +
		n[4] + n[5] + n[6] + n[7]) % 16;
	sum2 = (15 + n[8] + n[9] + n[10] + n[11] +
		n[12] + n[13] + n[14] + n[15]) % 16;

	if (sum1 != 15 || sum2 != 15) {
		debug_log_printk(dev, "checksum err\n");
		data->state = STATE_INACTIVE;
		return -EINVAL;
	}

	subaddr	= n[0] << 4 | n[2];
	subaddr2 = n[8] << 4 | n[11];
	oem = n[4] << 4 | n[5];
	addr = n[6] << 4 | n[7];
	toggle = n[10];
	obc1 = n[12] << 4 | n[13];
	obc2 = n[14] << 4 | n[15];

	if (subaddr != subaddr2) {
		sprintf(buf, "s1!=s2\n");
		debug_log_printk(dev, buf);
		data->state = STATE_INACTIVE;
		return -EINVAL;
	}
	scancode = obc1;
	custom_code =  oem << 8 | addr;
	sprintf(buf, "custom_code=%d\n", custom_code);
	debug_log_printk(dev, buf);
	sprintf(buf, "scancode=0x%x,t=%d\n", scancode, toggle);
	debug_log_printk(dev, buf);
	dev->set_custom_code(dev, custom_code);
	if (toggle == 0)
		remote_keydown(dev, scancode, REMOTE_NORMAL);
	else
		remote_keydown(dev, scancode, REMOTE_REPEAT);

	data->state = STATE_INACTIVE;
	return 0;
}

/**
 * ir_xmp_decode() - Decode one XMP pulse or space
 * @dev:	the struct rc_dev descriptor of the device
 * @duration:	the struct ir_raw_event descriptor of the pulse/space
 *
 * This function returns -EINVAL if the pulse violates the state machine
 */
static int ir_xmp_decode(struct remote_dev *dev, struct remote_raw_event ev,
				void *data_dec)
{
	struct xmp_dec *data = data_dec;
	char buf[512];

	if (ev.reset) {
		data->state = STATE_INACTIVE;
		return 0;
	}

	sprintf(buf, "dr:%d,s=%d, c=%d\n",
		ev.duration, data->state, data->count);
	debug_log_printk(dev, buf);

	switch (data->state) {

	case STATE_INACTIVE:
		if (eq_margin(ev.duration, XMP_LEADER, XMP_UNIT / 2)) {
			data->count = 0;
			data->state = STATE_NIBBLE_SPACE;
		}

		return 0;

	case STATE_LEADER_PULSE:
		debug_log_printk(dev, "STATE_LEADER_PULSE\n");

		if (eq_margin(ev.duration, XMP_LEADER, XMP_UNIT / 2))
			data->state = STATE_NIBBLE_SPACE;

		if (data->count == 16)
			return decode_xmp(dev, data, &ev);
		return 0;

	case STATE_NIBBLE_SPACE:
		if (geq_margin(ev.duration,
			XMP_TRAILER_SPACE, XMP_NIBBLE_PREFIX)) {
			return decode_xmp(dev, data, &ev);
		} else if (geq_margin(ev.duration, XMP_HALFFRAME_SPACE,
							XMP_NIBBLE_PREFIX)) {
			/* Expect 8 or 16 nibble pulses. 16
			 * in case of 'final' frame
			 */
			if (data->count == 16) {
				/*
				 * TODO: for now go back to half frame position
				 *	 so trailer can be found and key press
				 *	 can be handled.
				 */
				debug_log_printk(dev, "over pulses\n");
				data->count = 8;
			} else if (data->count != 8)
				debug_log_printk(dev, "half frame\n");
			data->state = STATE_LEADER_PULSE;
			return 0;

		} else if (geq_margin(ev.duration,
			XMP_NIBBLE_PREFIX, XMP_UNIT)) {
			/* store nibble raw data, decode after trailer */
			if (data->count == 16) {
				debug_log_printk(dev, "over pulses\n");
				data->state = STATE_INACTIVE;
				return -EINVAL;
			}
			data->durations[data->count] = ev.duration;
			data->count++;
			data->state = STATE_LEADER_PULSE;
			return 0;
		}

		break;
	}
	debug_log_printk(dev, "dec failed\n");

	data->state = STATE_INACTIVE;
	return -EINVAL;
}

static struct remote_raw_handler xmp_handler = {
	.protocols	= REMOTE_TYPE_RAW_XMP_1,
	.decode		= ir_xmp_decode,
};

static int __init ir_xmp_decode_init(void)
{
	xmp_handler.data = kzalloc(sizeof(struct xmp_dec), GFP_KERNEL);
	if (!xmp_handler.data) {
		pr_err("%s: ir_xmp_decode_init alloc xmp_dec failure\n",
					DRIVER_NAME);
		return -1;
	}
	remote_raw_handler_register(&xmp_handler);

	pr_info("%s: IR XMP protocol handler initialized\n", DRIVER_NAME);
	return 0;
}

static void __exit ir_xmp_decode_exit(void)
{
	remote_raw_handler_unregister(&xmp_handler);
	if (!xmp_handler.data)
		kfree(xmp_handler.data);
}

module_init(ir_xmp_decode_init);
module_exit(ir_xmp_decode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("XMP IR PROTOCOL DECODER");


