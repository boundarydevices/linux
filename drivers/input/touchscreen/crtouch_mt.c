/*
 * Copyright (C) 2011-2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
/*
 *   Copyright 2011-2012 Freescale Semiconductor, Inc. All Rights Reserved.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <linux/syscalls.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>

#include "crtouch_mt.h"

void report_single_touch(struct crtouch_data *crtouch)
{
	input_report_abs(crtouch->input_dev, ABS_MT_POSITION_X, crtouch->x1);
	input_report_abs(crtouch->input_dev, ABS_MT_POSITION_Y, crtouch->y1);
	input_report_abs(crtouch->input_dev, ABS_MT_TOUCH_MAJOR, 1);
	input_mt_sync(crtouch->input_dev);
	input_event(crtouch->input_dev, EV_ABS, ABS_X, crtouch->x1);
	input_event(crtouch->input_dev, EV_ABS, ABS_Y, crtouch->y1);
	input_event(crtouch->input_dev, EV_KEY, BTN_TOUCH, 1);
	input_report_abs(crtouch->input_dev, ABS_PRESSURE, 1);
	input_sync(crtouch->input_dev);

	crtouch->status_pressed = CRTOUCH_TOUCHED;
}

void report_multi_touch(struct crtouch_data *crtouch)
{

	input_report_key(crtouch->input_dev, ABS_MT_TRACKING_ID, 0);
	input_report_abs(crtouch->input_dev, ABS_MT_TOUCH_MAJOR, 1);
	input_report_abs(crtouch->input_dev, ABS_MT_POSITION_X, crtouch->x1);
	input_report_abs(crtouch->input_dev, ABS_MT_POSITION_Y, crtouch->y1);
	input_mt_sync(crtouch->input_dev);

	input_report_key(crtouch->input_dev, ABS_MT_TRACKING_ID, 1);
	input_report_abs(crtouch->input_dev, ABS_MT_TOUCH_MAJOR, 1);
	input_report_abs(crtouch->input_dev, ABS_MT_POSITION_X, crtouch->x2);
	input_report_abs(crtouch->input_dev, ABS_MT_POSITION_Y, crtouch->y2);
	input_mt_sync(crtouch->input_dev);

	input_sync(crtouch->input_dev);

	crtouch->status_pressed = CRTOUCH_TOUCHED;
}

void free_touch(struct crtouch_data *crtouch)
{
	input_event(crtouch->input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 0);
	input_mt_sync(crtouch->input_dev);
	input_event(crtouch->input_dev, EV_KEY, BTN_TOUCH, 0);
	input_report_abs(crtouch->input_dev, ABS_PRESSURE, 0);
	input_sync(crtouch->input_dev);

	crtouch->status_pressed = CRTOUCH_RELEASED;
}

void free_two_touch(struct crtouch_data *crtouch){

	input_event(crtouch->input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 0);
	input_mt_sync(crtouch->input_dev);
	input_sync(crtouch->input_dev);

}

int read_resolution(struct crtouch_data *crtouch)
{
	char resolution[4];
	int ret;

	ret = i2c_smbus_read_i2c_block_data(crtouch->client,
		HORIZONTAL_RESOLUTION_MBS, 4, resolution);

	if (ret < 0)
		return ret;

	crtouch->xmax = (resolution[0] << 8) | resolution[1];
	crtouch->ymax = (resolution[2] << 8) | resolution[3];
	pr_info("calibration values XMAX:%i YMAX:%i", crtouch->xmax, crtouch->ymax);
	if (!crtouch->xmax || !crtouch->ymax) {
		crtouch->xmax = 0x1000;
		crtouch->ymax = 0x1000;
		pr_info("changing to XMAX:%i YMAX:%i", crtouch->xmax, crtouch->ymax);
	}
	return 0;
}

const int sin_data[] = 	{
	0,
	1143,  2287,  3429,  4571,  5711,  6850,  7986,  9120,  10252, 11380,
	12504, 13625, 14742, 15854, 16961, 18064, 19160, 20251, 21336, 22414,
	23486, 24550, 25606, 26655, 27696, 28729, 29752, 30767, 31772, 32768,
	33753, 34728, 35693, 36647, 37589, 38521, 39440, 40347, 41243, 42125,
	42995, 43852, 44695, 45525, 46340, 47142, 47929, 48702, 49460, 50203,
	50931, 51643, 52339, 53019, 53683, 54331, 54963, 55577, 56175, 56755,
	57319, 57864, 58393, 58903, 59395, 59870, 60326, 60763, 61183, 61583,
	61965, 62328, 62672, 62997, 63302, 63589, 63856, 64103, 64331, 64540,
	64729, 64898, 65047, 65176, 65286, 65376, 65446, 65496, 65526, 65536,
};

static void report_MT(struct work_struct *work)
{

	struct crtouch_data *crtouch = container_of(work, struct crtouch_data, work);
	struct i2c_client *client = crtouch->client;

	s32 status_register_1 = 0;
	s32 status_register_2 = 0;
	s32 dynamic_status = 0;
	s32 fifo_capacitive = 0;
	s32 rotate_angle_help = 0;
	int result;
	int command = 0;
	int degrees = 0;
	int zoom_value_moved = 0;
	int value_to_zoom = 0;
	const int xmax = crtouch->xmax;
	const int ymax = crtouch->ymax;
	char xy[LEN_XY];

	status_register_1 = i2c_smbus_read_byte_data(client, STATUS_REGISTER_1);

	/*check zoom resistive*/
	if ((status_register_1 & MASK_EVENTS_ZOOM_R) == MASK_EVENTS_ZOOM_R && (status_register_1 & TWO_TOUCH)) {

		status_register_2 = i2c_smbus_read_byte_data(client, STATUS_REGISTER_2);

		crtouch->zoom_size = i2c_smbus_read_byte_data(client, ZOOM_SIZE);

		if ((status_register_2 & MASK_ZOOM_DIRECTION) == MASK_ZOOM_DIRECTION) {
			command = ZOOM_OUT;
		} else {
			command = ZOOM_IN;
		}

		if(command == crtouch->zoom_state){
			crtouch->zoom_size = crtouch->zoom_size - crtouch->last_zoom;
			crtouch->last_zoom = crtouch->zoom_size;
		}
		else{
			crtouch->last_zoom = 0;
			crtouch->zoom_state = command;
		}	

		value_to_zoom = ((crtouch->zoom_size * xmax) / MAX_ZOOM_CRTOUCH);


	/*check rotate resistive*/
	} else if ((status_register_1 & MASK_EVENTS_ROTATE_R) == MASK_EVENTS_ROTATE_R  && (status_register_1 & TWO_TOUCH)) {

		status_register_2 = i2c_smbus_read_byte_data(client, STATUS_REGISTER_2);
		crtouch->rotate_angle = i2c_smbus_read_byte_data(client, ROTATE_ANGLE);
		
		//printk(KERN_ALERT "Rotate Angle Complete: %d", crtouch->rotate_angle);

		rotate_angle_help = crtouch->rotate_angle;

		if ((status_register_2 & MASK_ROTATE_DIRECTION) == MASK_ROTATE_DIRECTION) {

			command = ROTATE_COUNTER_CLK;

			if (crtouch->rotate_state == ROTATE_CLK)
				crtouch->last_angle = 0;

			crtouch->rotate_state = ROTATE_COUNTER_CLK;
			crtouch->rotate_angle -= crtouch->last_angle;
			crtouch->last_angle += crtouch->rotate_angle;

		} else {

			command = ROTATE_CLK;

			if (crtouch->rotate_state == ROTATE_COUNTER_CLK)
				crtouch->last_angle = 0;

			crtouch->rotate_state = ROTATE_CLK;
			crtouch->rotate_angle -= crtouch->last_angle;
			crtouch->last_angle += crtouch->rotate_angle;
		}

		//printk(KERN_ALERT "Rotate Angle New: %d", crtouch->rotate_angle);
	}

	/*check slide resistive*/
	if ((status_register_1 & MASK_EVENTS_SLIDE_R) == MASK_EVENTS_SLIDE_R) {

		status_register_2 = i2c_smbus_read_byte_data(client, STATUS_REGISTER_2);

		if ((status_register_2 & MASK_SLIDE_DOWN) == MASK_SLIDE_DOWN) {
			input_report_key(crtouch->input_dev, KEY_H, 1);
			input_report_key(crtouch->input_dev, KEY_H, 0);
			input_sync(crtouch->input_dev);
		} else if ((status_register_2 & MASK_SLIDE_UP) == MASK_SLIDE_UP) {
			input_report_key(crtouch->input_dev, KEY_G, 1);
			input_report_key(crtouch->input_dev, KEY_G, 0);
			input_sync(crtouch->input_dev);
		} else if ((status_register_2 & MASK_SLIDE_LEFT) == MASK_SLIDE_LEFT) {
			input_report_key(crtouch->input_dev, KEY_F, 1);
			input_report_key(crtouch->input_dev, KEY_F, 0);
			input_sync(crtouch->input_dev);
		} else if ((status_register_2 & MASK_SLIDE_RIGHT) == MASK_SLIDE_RIGHT) {
			input_report_key(crtouch->input_dev, KEY_E, 1);
			input_report_key(crtouch->input_dev, KEY_E, 0);
			input_sync(crtouch->input_dev);
		}

	}

	/*check capacitive events*/
	if ((status_register_1 & MASK_EVENTS_CAPACITIVE) == MASK_EVENTS_CAPACITIVE) {

		/*capacitive keypad*/
		if ((crtouch->data_configuration & MASK_KEYPAD_CONF) == MASK_KEYPAD_CONF) {

			while ((fifo_capacitive = i2c_smbus_read_byte_data(client, CAPACITIVE_ELECTRODES_FIFO)) < 0xFF) {

				if ((fifo_capacitive & KEY_NUMBER) == MASK_DYNAMIC_DISPLACEMENT_BTN3) {

					if ((fifo_capacitive & EV_TYPE) == 0) {
						input_report_key(crtouch->input_dev, KEY_D, 1);
						input_sync(crtouch->input_dev);
					} else{
						input_report_key(crtouch->input_dev, KEY_D, 0);
						input_sync(crtouch->input_dev);
					}

				} else if ((fifo_capacitive & KEY_NUMBER) == MASK_DYNAMIC_DISPLACEMENT_BTN2) {

					if ((fifo_capacitive & EV_TYPE) == 0) {
						input_report_key(crtouch->input_dev, KEY_C, 1);
						input_sync(crtouch->input_dev);
					} else{
						input_report_key(crtouch->input_dev, KEY_C, 0);
						input_sync(crtouch->input_dev);
					}

				} else if ((fifo_capacitive & KEY_NUMBER) == MASK_DYNAMIC_DISPLACEMENT_BTN1) {

					if ((fifo_capacitive & EV_TYPE) == 0) {
						input_report_key(crtouch->input_dev, KEY_B, 1);
						input_sync(crtouch->input_dev);
					} else{
						input_report_key(crtouch->input_dev, KEY_B, 0);
						input_sync(crtouch->input_dev);
					}

				} else if ((fifo_capacitive & KEY_NUMBER) == MASK_DYNAMIC_DISPLACEMENT_BTN0) {

					if ((fifo_capacitive & EV_TYPE) == 0) {
						input_report_key(crtouch->input_dev, KEY_A, 1);
						input_sync(crtouch->input_dev);
					} else {
						input_report_key(crtouch->input_dev, KEY_A, 0);
						input_sync(crtouch->input_dev);
					}
				}

			}


		}
		/*capacitive slide*/
		else if ((crtouch->data_configuration & MASK_SLIDE_CONF) == MASK_SLIDE_CONF) {
			dynamic_status = i2c_smbus_read_byte_data(client, DYNAMIC_STATUS);

			if (dynamic_status & MASK_DYNAMIC_FLAG) {

				if (dynamic_status & MASK_DYNAMIC_DIRECTION) {
					input_report_key(crtouch->input_dev, KEY_I, 1);
					input_report_key(crtouch->input_dev, KEY_I, 0);
					input_sync(crtouch->input_dev);
				} else {
					input_report_key(crtouch->input_dev, KEY_J, 1);
					input_report_key(crtouch->input_dev, KEY_J, 0);
					input_sync(crtouch->input_dev);
				}
			}
		}
		/*capacitive rotate*/
		else if ((crtouch->data_configuration & MASK_ROTARY_CONF) == MASK_ROTARY_CONF) {

			dynamic_status = i2c_smbus_read_byte_data(client, DYNAMIC_STATUS);

			if (dynamic_status & MASK_DYNAMIC_FLAG) {

				if (dynamic_status & MASK_DYNAMIC_DIRECTION) {
					input_report_key(crtouch->input_dev, KEY_K, 1);
					input_report_key(crtouch->input_dev, KEY_K, 0);
					input_sync(crtouch->input_dev);
				} else {
					input_report_key(crtouch->input_dev, KEY_L, 1);
					input_report_key(crtouch->input_dev, KEY_L, 0);
					input_sync(crtouch->input_dev);
				}
			}
		}

	}

	/*check xy*/
	if ((status_register_1 & MASK_RESISTIVE_SAMPLE) == MASK_RESISTIVE_SAMPLE && !(status_register_1 & TWO_TOUCH)) {

		/*clean zoom/rotate data when release 2touch*/
		crtouch->last_angle = 0;
		crtouch->rotate_state = 0;
		crtouch->last_zoom = 0;
		crtouch->zoom_state = 0;

		if (!(status_register_1 & MASK_PRESSED)) {

			if (crtouch->status_pressed)
				free_touch(crtouch);
		} else {
			result = i2c_smbus_read_i2c_block_data(client, X_COORDINATE_MSB, LEN_XY, xy);

			if (result < 0)
				/*Do nothing*/
				printk(KERN_ALERT "Single Touch Error Reading");

			else{
				crtouch->x1 = xy[1];
				crtouch->x1 |= xy[0] << 8;
				crtouch->y1 = xy[3];
				crtouch->y1 |= xy[2] << 8;

				/*if differents points*/
				if (crtouch->x1_old != crtouch->x1 || crtouch->y1_old != crtouch->y1) {

					crtouch->x1_old = crtouch->x1;
					crtouch->y1_old = crtouch->y1;
					report_single_touch(crtouch);
				}

			}

		}

	}

	/*simulate gestures 2 touch*/
	if (command) {

		if (command == ZOOM_IN || command == ZOOM_OUT) {

			free_two_touch(crtouch);

			switch (command) {

			case ZOOM_IN:

				msleep(1);
				/*simulate initial points*/
				crtouch->x1 = (xmax/2) - (xmax/20);
				crtouch->x2 = (xmax/2) + (xmax/20);
				crtouch->y1 = ymax/2;
				crtouch->y2 = ymax/2;
				report_multi_touch(crtouch);

				/* to zoom the crtouch size reported
				msleep(1);
				crtouch->x1 -= crtouch->zoom_size/4;
				crtouch->x2 += crtouch->zoom_size/4;
				report_multi_touch(crtouch);
				msleep(1);
				crtouch->x1 -= crtouch->zoom_size/4;
				crtouch->x2 += crtouch->zoom_size/4;
				report_multi_touch(crtouch);
				*/

				zoom_value_moved = (xmax/100);

				if (zoom_value_moved > 0) {
					msleep(1);
					crtouch->x1 -= zoom_value_moved;
					crtouch->x2 += zoom_value_moved;
					report_multi_touch(crtouch);
					msleep(1);
					crtouch->x1 -= zoom_value_moved;
					crtouch->x2 += zoom_value_moved;
					report_multi_touch(crtouch);

				} else {
					msleep(1);
					crtouch->x1 -= 5;
					crtouch->x2 += 5;
					report_multi_touch(crtouch);
					msleep(1);
					crtouch->x1 -= 5;
					crtouch->x2 += 5;
					report_multi_touch(crtouch);
				}
				break;

			case ZOOM_OUT:

				msleep(1);
				crtouch->x1 = ((xmax * 43) / 100);
				crtouch->x2 = ((xmax * 57) / 100);
				crtouch->y1 = ymax/2;
				crtouch->y2 = ymax/2;
				report_multi_touch(crtouch);


				/* to zoom the crtouch size reported

				needs to updated the initial position, so delete the code that is up
				msleep(1);
				crtouch->x1 = ((xmax * 20) / 100);
				crtouch->x2 = ((xmax * 80) / 100);
				crtouch->y1 = ymax/2;
				crtouch->y2 = ymax/2;
				report_multi_touch(crtouch);


				msleep(1);
				crtouch->x1 += crtouch->zoom_size/4;
				crtouch->x2 -= crtouch->zoom_size/4;
				report_multi_touch(crtouch);
				msleep(1);
				crtouch->x1 += crtouch->zoom_size/4;
				crtouch->x2 -= crtouch->zoom_size/4;
				report_multi_touch(crtouch);*/
				

				zoom_value_moved = (xmax / 100);

				if (zoom_value_moved > 0) {
					msleep(1);
					crtouch->x1 += zoom_value_moved;
					crtouch->x2 -= zoom_value_moved;
					report_multi_touch(crtouch);
					msleep(1);
					crtouch->x1 += zoom_value_moved;
					crtouch->x2 -= zoom_value_moved;
					report_multi_touch(crtouch);

				} else {
					msleep(1);
					crtouch->x1 += 5;
					crtouch->x2 -= 5;
					report_multi_touch(crtouch);
					msleep(1);
					crtouch->x1 += 5;
					crtouch->x2 -= 5;
					report_multi_touch(crtouch);
				}
				break;
			}

		}

		else if (command == ROTATE_CLK || command == ROTATE_COUNTER_CLK) {
			int angle;
			int scale = (xmax > ymax) ? xmax : ymax;
			int invert = 0;
			int xcos, ysin;

			free_two_touch(crtouch);

			/*get radians from crtouch and change them to degrees*/
			degrees = ((crtouch->rotate_angle * 360) / (64 * 3));
			//printk(KERN_ALERT "Degrees calculated: %d", degrees);

			if (degrees < 0) {
				/*fix if negative values appear, because of time synchronization*/
				degrees = rotate_angle_help;
				crtouch->last_angle = rotate_angle_help;
			}

			/*simulate initial points*/
			crtouch->x1 = xmax/2;
			crtouch->y1 = ymax/2;
			crtouch->x2 = (xmax/2) + (xmax);
			crtouch->y2 = (ymax/2);
			report_multi_touch(crtouch);

			/*printk(KERN_ALERT "X1: %d", crtouch->x1);
			printk(KERN_ALERT "X2: %d", crtouch->x2);
			printk(KERN_ALERT "Y1: %d", crtouch->y1);
			printk(KERN_ALERT "Y2: %d", crtouch->y2);*/
			angle = degrees % 360;

			if (angle >= 180) {
				angle -= 180;
				invert = 3;
			}
			if (angle > 90) {
				angle = 180 - angle;
				invert ^= 1;
			}
			xcos = (sin_data[90 - angle] * scale) >> 16;
			ysin = (sin_data[angle] * scale) >> 16;

			if (command == ROTATE_COUNTER_CLK)
				invert ^= 3;

			if (invert & 1)
				xcos = -xcos;
			if (invert & 2)
				ysin = -ysin;

			crtouch->x2 = (xmax/2) + xcos;
			crtouch->y2 = (ymax/2) + ysin;
			report_multi_touch(crtouch);

			/*printk(KERN_ALERT "X1: %d", crtouch->x1);
			printk(KERN_ALERT "X2: %d", crtouch->x2);
			printk(KERN_ALERT "Y1: %d", crtouch->y1);
			printk(KERN_ALERT "Y2: %d", crtouch->y2);*/

		}

		/*clean command*/
		command = 0;

	}

}

int crtouch_open(struct inode *inode, struct file *filp)
{
	struct crtouch_data *crtouch = container_of(inode->i_cdev, struct crtouch_data, cdev);;

	filp->private_data = crtouch;
	return 0;
}

/*read crtouch registers*/
static ssize_t crtouch_read(struct file *filep,
			char *buf, size_t count, loff_t *fpos)
{
	struct crtouch_data *crtouch = filep->private_data;
	s32 data_to_read;

	if (buf != NULL) {

		switch (*buf) {

		case STATUS_ERROR:
		case STATUS_REGISTER_1:
		case STATUS_REGISTER_2:
		case X_COORDINATE_MSB:
		case X_COORDINATE_LSB:
		case Y_COORDINATE_MSB:
		case Y_COORDINATE_LSB:
		case PRESSURE_VALUE_MSB:
		case PRESSURE_VALUE_LSB:
		case FIFO_STATUS:
		case FIFO_X_COORDINATE_MSB:
		case FIFO_X_COORDINATE_LSB:
		case FIFO_Y_COORDINATE_MSB:
		case FIFO_Y_COORDINATE_LSB:
		case FIFO_PRESSURE_VALUE_COORDINATE_MSB:
		case FIFO_PRESSURE_VALUE_COORDINATE_LSB:
		case UART_BAUDRATE_MSB:
		case UART_BAUDRATE_MID:
		case UART_BAUDRATE_LSB:
		case DEVICE_IDENTIFIER_REGISTER:
		case SLIDE_DISPLACEMENT:
		case ROTATE_ANGLE:
		case ZOOM_SIZE:
		case ELECTRODE_STATUS:
		case FAULTS_NOTE1:
		case E0_BASELINE_MSB:
		case E0_BASELINE_LSB:
		case E1_BASELINE_MSB:
		case E1_BASELINE_LSB:
		case E2_BASELINE_MSB:
		case E2_BASELINE_LSB:
		case E3_BASELINE_MSB:
		case E3_BASELINE_LSB:
		case E0_INSTANT_DELTA:
		case E1_INSTANT_DELTA:
		case E2_INSTANT_DELTA:
		case E3_INSTANT_DELTA:
		case DYNAMIC_STATUS:
		case STATIC_STATUS:
		case CONFIGURATION:
		case TRIGGER_EVENTS:
		case FIFO_SETUP:
		case SAMPLING_X_Y:
		case X_SETTLING_TIME_MBS:
		case X_SETTLING_TIME_LBS:
		case Y_SETTLING_TIME_MBS:
		case Y_SETTLING_TIME_LBS:
		case Z_SETTLING_TIME_MBS:
		case Z_SETTLING_TIME_LBS:
		case HORIZONTAL_RESOLUTION_MBS:
		case HORIZONTAL_RESOLUTION_LBS:
		case VERTICAL_RESOLUTION_MBS:
		case VERTICAL_RESOLUTION_LBS:
		case SLIDE_STEPS:
		case SYSTEM_CONFIG_NOTE2:
		case DC_TRACKER_RATE:
		case RESPONSE_TIME:
		case STUCK_KEY_TIMEOUT:
		case E0_SENSITIVITY:
		case E1_SENSITIVITY:
		case E2_SENSITIVITY:
		case E3_SENSITIVITY:
		case ELECTRODE_ENABLERS:
		case LOW_POWER_SCAN_PERIOD:
		case LOW_POWER_ELECTRODE:
		case LOW_POWER_ELECTRODE_SENSITIVITY:
		case CONTROL_CONFIG:
		case EVENTS:
		case AUTO_REPEAT_RATE:
		case AUTO_REPEAT_START:
		case MAX_TOUCHES:

			data_to_read = i2c_smbus_read_byte_data(crtouch->client, *buf);

			if (data_to_read >= 0 && copy_to_user(buf, &data_to_read, 1))
				printk(KERN_DEBUG "error reading from userspace\n");
			break;

		default:
			printk(KERN_DEBUG "invalid address to read\n");
			return -ENXIO;

		}

	}

	return 1;
}

/*write crtouch register to configure*/
static ssize_t crtouch_write(struct file *filep, const char __user *buf, size_t size, loff_t *fpos)
{
	struct crtouch_data *crtouch = filep->private_data;
	const unsigned char *data_to_write = NULL;

	data_to_write = buf;

	if (data_to_write == NULL)
		return -ENOMEM;
	if ((data_to_write + 1) == NULL)
		return -EINVAL;

	/*update driver variable*/
	if (*data_to_write == CONFIGURATION)	{
			if ((*(data_to_write + 1) >= 0x00) && (*(data_to_write + 1) <= 0xFF))
				crtouch->data_configuration = *(data_to_write + 1);
	}

	switch (*data_to_write) {

	case CONFIGURATION:
	case X_SETTLING_TIME_MBS:
	case X_SETTLING_TIME_LBS:
	case Y_SETTLING_TIME_MBS:
	case Y_SETTLING_TIME_LBS:
	case Z_SETTLING_TIME_MBS:
	case Z_SETTLING_TIME_LBS:
	case HORIZONTAL_RESOLUTION_MBS:
	case HORIZONTAL_RESOLUTION_LBS:
	case VERTICAL_RESOLUTION_MBS:
	case VERTICAL_RESOLUTION_LBS:
	case SLIDE_STEPS:
	case SYSTEM_CONFIG_NOTE2:
	case DC_TRACKER_RATE:
	case STUCK_KEY_TIMEOUT:
	case LOW_POWER_ELECTRODE_SENSITIVITY:
	case EVENTS:
	case CONTROL_CONFIG:
	case AUTO_REPEAT_RATE:
	case AUTO_REPEAT_START:
	case TRIGGER_EVENTS:

		if ((*(data_to_write + 1) >= 0x00) && (*(data_to_write + 1) <= 0xFF))
			i2c_smbus_write_byte_data(crtouch->client,
					*data_to_write, *(data_to_write + 1));
		else
			printk(KERN_DEBUG "invalid range of data\n");

		break;


	case RESPONSE_TIME:

		if ((*(data_to_write + 1) >= 0x00) && (*(data_to_write + 1) <= 0x3F))
			i2c_smbus_write_byte_data(crtouch->client,
					*data_to_write, *(data_to_write+1));
		else
			printk(KERN_DEBUG "invalid range of data\n");
		break;

	case FIFO_SETUP:
		if ((*(data_to_write + 1) >= 0x00) && (*(data_to_write + 1) <= 0x1F))
			i2c_smbus_write_byte_data(crtouch->client,
					*data_to_write, *(data_to_write+1));
		else
			printk(KERN_DEBUG "invalid range of data\n");
		break;

	case SAMPLING_X_Y:
		if ((*(data_to_write + 1) >= 0x05) && (*(data_to_write + 1) <= 0x64))
			i2c_smbus_write_byte_data(crtouch->client,
					*data_to_write, *(data_to_write+1));
		else
			printk(KERN_DEBUG "invalid range of data\n");
		break;


	case E0_SENSITIVITY:
	case E1_SENSITIVITY:
	case E2_SENSITIVITY:
	case E3_SENSITIVITY:
		if ((*(data_to_write+1) >= 0x02) && (*(data_to_write+1) <= 0x7F))
			i2c_smbus_write_byte_data(crtouch->client,
					*data_to_write, *(data_to_write+1));
		else
			printk(KERN_DEBUG "invalid range of data\n");
		break;


	case ELECTRODE_ENABLERS:
	case LOW_POWER_SCAN_PERIOD:
	case LOW_POWER_ELECTRODE:
	case MAX_TOUCHES:
		if ((*(data_to_write+1) >= 0x00) && (*(data_to_write+1) <= 0x0F))
			i2c_smbus_write_byte_data(crtouch->client,
					*data_to_write, *(data_to_write+1));
		else
			printk(KERN_DEBUG "invalid range of data\n");
		break;


	default:
		printk(KERN_DEBUG "invalid address to write\n");
		return -ENXIO;

	}


	return 1;
}

struct file_operations file_ops_crtouch = {
open:	crtouch_open,
write :	crtouch_write,
read :	crtouch_read,
};


static irqreturn_t crtouch_irq(int irq, void *dev_id)
{
	struct crtouch_data *crtouch = dev_id;
	queue_work(crtouch->workqueue, &crtouch->work);
	return IRQ_HANDLED;
}

static int crtouch_resume(struct device *dev)
{
	struct crtouch_data *crtouch = dev_get_drvdata(dev);
	int gpio = crtouch->wake_gpio;
	int active = crtouch->wake_active_low ? 0 : 1;

	if (!gpio_is_valid(gpio)) {
		gpio = crtouch->reset_gpio;
		active = crtouch->reset_active_low ? 0 : 1;
		if (!gpio_is_valid(gpio))
			return 0;
	}
	gpio_set_value(gpio, active);
	udelay(12);
	gpio_set_value(gpio, !active);
	return 0;
}

static int crtouch_suspend(struct device *dev)
{
	struct crtouch_data *crtouch = dev_get_drvdata(dev);
	s32 data_to_read;

	data_to_read = i2c_smbus_read_byte_data(crtouch->client, CONFIGURATION);
	data_to_read |= SHUTDOWN_CRTOUCH;
	i2c_smbus_write_byte_data(crtouch->client, CONFIGURATION , data_to_read);
	return 0;
}

static int setup_wake_gpio(struct i2c_client *client, struct crtouch_data *crtouch)
{
	int err = 0;
	int gpio;
	enum of_gpio_flags flags;
	struct device_node *np = client->dev.of_node;

	crtouch->wake_gpio = -1;
	gpio = of_get_named_gpio_flags(np, "wake-gpios", 0, &flags);
	pr_info("%s:%d\n", __func__, gpio);
	if (!gpio_is_valid(gpio))
		return 0;

	crtouch->wake_active_low = flags & OF_GPIO_ACTIVE_LOW;
	err = devm_gpio_request_one(&client->dev, gpio, crtouch->wake_active_low ?
			GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW,
			"crtouch_wake_gpio");
	if (err)
		dev_err(&client->dev, "can't request wake gpio %d", gpio);
	crtouch->wake_gpio = gpio;
	return err;
}

static int setup_reset_gpio(struct i2c_client *client, struct crtouch_data *crtouch)
{
	int err = 0;
	int gpio;
	enum of_gpio_flags flags;
	struct device_node *np = client->dev.of_node;

	crtouch->reset_gpio = -1;
	gpio = of_get_named_gpio_flags(np, "reset-gpios", 0, &flags);
	pr_info("%s:%d\n", __func__, gpio);
	if (!gpio_is_valid(gpio))
		return 0;

	crtouch->reset_active_low = flags & OF_GPIO_ACTIVE_LOW;
	err = devm_gpio_request_one(&client->dev, gpio, crtouch->reset_active_low ?
			GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW,
			"crtouch_reset_gpio");
	if (err)
		dev_err(&client->dev, "can't request reset gpio %d", gpio);
	crtouch->reset_gpio = gpio;
	return err;
}

static int setup_irq_gpio(struct i2c_client *client, struct crtouch_data *crtouch)
{
	int err = 0;
	int gpio;
	struct device_node *np = client->dev.of_node;

	crtouch->irq_gpio = -1;
	gpio = of_get_named_gpio(np, "interrupts-extended", 0);
	pr_info("%s:%d\n", __func__, gpio);
	if (!gpio_is_valid(gpio))
		return 0;

	err = devm_gpio_request_one(&client->dev, gpio, GPIOF_DIR_IN,
			"crtouch_irq");
	if (err) {
		dev_err(&client->dev, "can't request irq gpio %d", gpio);
		return err;
	}
	crtouch->irq_gpio = gpio;
	return err;
}

static int crtouch_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct crtouch_data *crtouch;
	int result;
	struct input_dev *input_dev;
	s32 mask_trigger = 0;


	pr_info("%s\n", __func__);

	crtouch = kzalloc(sizeof(struct crtouch_data), GFP_KERNEL);

	if (!crtouch)
		return -ENOMEM;

	input_dev = input_allocate_device();
	if (!input_dev) {
		result = -ENOMEM;
		goto err_free_mem;
	}

	crtouch->irq = client->irq;
	crtouch->input_dev = input_dev;
	crtouch->client = client;
	crtouch->workqueue = create_singlethread_workqueue("crtouch");
	INIT_WORK(&crtouch->work, report_MT);

	setup_reset_gpio(client, crtouch);
	setup_wake_gpio(client, crtouch);
	setup_irq_gpio(client, crtouch);

	if (crtouch->workqueue == NULL) {
		printk(KERN_DEBUG "couldn't create workqueue\n");
		result = -ENOMEM;
		goto err_wqueue;
	}

	result = read_resolution(crtouch);
	if (result < 0) {
		pr_err("couldn't read size of screen %d\n", result);
		goto err_free_wq;
	}

	crtouch->data_configuration = i2c_smbus_read_byte_data(client, CONFIGURATION);
	crtouch->data_configuration &= CLEAN_SLIDE_EVENTS;
	crtouch->data_configuration |= SET_MULTITOUCH;
	i2c_smbus_write_byte_data(client, CONFIGURATION, crtouch->data_configuration);

	mask_trigger = i2c_smbus_read_byte_data(client, TRIGGER_EVENTS);
	mask_trigger |= SET_TRIGGER_RESISTIVE;
	i2c_smbus_write_byte_data(client, TRIGGER_EVENTS, mask_trigger);

	i2c_smbus_write_byte_data(client, TRIGGER_EVENTS, mask_trigger);

	crtouch->input_dev->name = "CRTOUCH Input Device";
	crtouch->input_dev->id.bustype = BUS_I2C;

	__set_bit(EV_ABS, crtouch->input_dev->evbit);
	__set_bit(EV_KEY, crtouch->input_dev->evbit);
	__set_bit(BTN_TOUCH, crtouch->input_dev->keybit);
	__set_bit(ABS_X, crtouch->input_dev->absbit);
	__set_bit(ABS_Y, crtouch->input_dev->absbit);
	__set_bit(ABS_PRESSURE, crtouch->input_dev->absbit);
	__set_bit(EV_SYN, crtouch->input_dev->evbit);

	/*register keys that will be reported by crtouch*/
	__set_bit(KEY_A, crtouch->input_dev->keybit);
	__set_bit(KEY_B, crtouch->input_dev->keybit);
	__set_bit(KEY_C, crtouch->input_dev->keybit);
	__set_bit(KEY_D, crtouch->input_dev->keybit);
	__set_bit(KEY_E, crtouch->input_dev->keybit);
	__set_bit(KEY_F, crtouch->input_dev->keybit);
	__set_bit(KEY_G, crtouch->input_dev->keybit);
	__set_bit(KEY_H, crtouch->input_dev->keybit);
	__set_bit(KEY_I, crtouch->input_dev->keybit);
	__set_bit(KEY_J, crtouch->input_dev->keybit);
	__set_bit(KEY_K, crtouch->input_dev->keybit);
	__set_bit(KEY_L, crtouch->input_dev->keybit);

	input_set_abs_params(crtouch->input_dev, ABS_X, XMIN, crtouch->xmax, 0, 0);
	input_set_abs_params(crtouch->input_dev, ABS_Y, YMIN, crtouch->ymax, 0, 0);
	input_set_abs_params(crtouch->input_dev, ABS_MT_POSITION_X,
				XMIN, crtouch->xmax, 0, 0);
	input_set_abs_params(crtouch->input_dev, ABS_MT_POSITION_Y,
				YMIN, crtouch->ymax, 0, 0);
	input_set_abs_params(crtouch->input_dev, ABS_MT_TOUCH_MAJOR, 0, 1, 0, 0);
	input_set_abs_params(crtouch->input_dev, ABS_MT_TRACKING_ID, 0, 1, 0, 0);

	input_set_abs_params(crtouch->input_dev, ABS_PRESSURE, 0, 1, 0, 0);
	printk(KERN_DEBUG "CR-TOUCH max values X: %d Y: %d\n", crtouch->xmax, crtouch->ymax);

	result = input_register_device(crtouch->input_dev);
	if (result)
		goto err_free_wq;

	if (alloc_chrdev_region(&crtouch->dev_number, 0, 2, DEV_NAME) < 0) {
		printk(KERN_DEBUG "couldn't allocate cr-touch device with dev");
		goto err_unr_dev;
	}

	cdev_init(&crtouch->cdev , &file_ops_crtouch);

	if (cdev_add(&crtouch->cdev, crtouch->dev_number, 1)) {
		printk(KERN_DEBUG "couldn't register cr-touch device with dev\n");
		goto err_unr_chrdev;
	}

	crtouch->crtouch_class = class_create(THIS_MODULE, DEV_NAME);

	if (crtouch->crtouch_class == NULL) {
		printk(KERN_DEBUG "unable to create a class");
		goto err_unr_cdev;
	}

	if (device_create(crtouch->crtouch_class, NULL, crtouch->dev_number,
				NULL, DEV_NAME) == NULL) {
		printk(KERN_DEBUG "unable to create a device");
		goto err_unr_class;
	}

	/* request irq trigger falling */
	result = request_irq(crtouch->irq, crtouch_irq,
			IRQF_TRIGGER_FALLING, IRQ_NAME, crtouch);

	if (result < 0) {
		printk(KERN_DEBUG "unable to request IRQ\n");
		goto err_unr_createdev;
	}

	/*clean interrupt pin*/
	i2c_smbus_read_byte_data(client, STATUS_REGISTER_1);
	i2c_set_clientdata(client, crtouch);
	return 0;

err_unr_createdev:
	device_destroy(crtouch->crtouch_class, crtouch->dev_number);
err_unr_class:
	class_destroy(crtouch->crtouch_class);
err_unr_cdev:
	cdev_del(&crtouch->cdev);
err_unr_chrdev:
	unregister_chrdev_region(crtouch->dev_number, 1);
err_unr_dev:
	input_unregister_device(crtouch->input_dev);
err_free_wq:
	destroy_workqueue(crtouch->workqueue);
err_wqueue:
	input_free_device(crtouch->input_dev);
err_free_mem:
	kfree(crtouch);
	return result;
}

static int crtouch_remove(struct i2c_client *client)
{
	struct crtouch_data *crtouch = i2c_get_clientdata(client);

	cancel_work_sync(&crtouch->work);

	device_destroy(crtouch->crtouch_class, crtouch->dev_number);
	class_destroy(crtouch->crtouch_class);
	cdev_del(&crtouch->cdev);
	unregister_chrdev_region(crtouch->dev_number, 1);
	input_unregister_device(crtouch->input_dev);
	destroy_workqueue(crtouch->workqueue);
	input_free_device(crtouch->input_dev);
	if (gpio_is_valid(crtouch->reset_gpio))
		gpio_set_value(crtouch->reset_gpio, crtouch->reset_active_low ? 0 : 1);
	kfree(crtouch);
	return 0;
}

static const struct i2c_device_id crtouch_idtable[] = {
	{"crtouch", 0},
	{}
};

static const struct dev_pm_ops crtouch_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = crtouch_suspend,
	.resume = crtouch_resume,
#endif
};

static struct i2c_driver crtouch_fops = {

	.driver = {
		   .owner = THIS_MODULE,
		   .name = "crtouch",
		   .pm	 = &crtouch_pm_ops,
	},
	.id_table	= crtouch_idtable,
	.probe		= crtouch_probe,
	.remove 	= crtouch_remove,

};

MODULE_DEVICE_TABLE(i2c, crtouch_idtable);

static int __init crtouch_init(void)
{
	return i2c_add_driver(&crtouch_fops);
}

static void __exit crtouch_exit(void)
{
	i2c_del_driver(&crtouch_fops);
}


module_init(crtouch_init);
module_exit(crtouch_exit);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("CR-TOUCH multitouch driver");
MODULE_LICENSE("GPL v2");
