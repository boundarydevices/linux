/*
 *  "fusion_F0710A" touchscreen driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

/* I2C slave address */
#define fusion_F0710A_I2C_SLAVE_ADDR		0x10

/* I2C registers */
#define fusion_F0710A_DATA_INFO			0x00

/* First Point*/
#define fusion_F0710A_POS_X1_HI			0x01 	/* 16-bit register, MSB */
#define fusion_F0710A_POS_X1_LO			0x02 	/* 16-bit register, LSB */
#define fusion_F0710A_POS_Y1_HI			0x03 	/* 16-bit register, MSB */
#define fusion_F0710A_POS_Y1_LO			0x04 	/* 16-bit register, LSB */
#define fusion_F0710A_FIR_PRESS			0X05
#define fusion_F0710A_FIR_TIDTS			0X06

/* Second Point */
#define fusion_F0710A_POS_X2_HI			0x07 	/* 16-bit register, MSB */
#define fusion_F0710A_POS_X2_LO			0x08 	/* 16-bit register, LSB */
#define fusion_F0710A_POS_Y2_HI			0x09 	/* 16-bit register, MSB */
#define fusion_F0710A_POS_Y2_LO			0x0A 	/* 16-bit register, LSB */
#define fusion_F0710A_SEC_PRESS			0x0B
#define fusion_F0710A_SEC_TIDTS			0x0C

#define fusion_F0710A_VIESION_INFO_LO		0X0E
#define fusion_F0710A_VIESION_INFO			0X0F

#define fusion_F0710A_RESET				0x10
#define fusion_F0710A_SCAN_COMPLETE		0x11


#define fusion_F0710A_VIESION_10			0
#define fusion_F0710A_VIESION_07			1
#define fusion_F0710A_VIESION_43			2

/* fusion_F0710A 10 inch panel */
#define fusion_F0710A10_XMAX				2275
#define fusion_F0710A10_YMAX				1275
#define fusion_F0710A10_REV				1

/* fusion_F0710A 7 inch panel */
#define fusion_F0710A07_XMAX				1500
#define fusion_F0710A07_YMAX				900
#define fusion_F0710A07_REV				0

/* fusion_F0710A 4.3 inch panel */
#define fusion_F0710A43_XMAX				900
#define fusion_F0710A43_YMAX				500
#define fusion_F0710A43_REV				0

#define	fusion_F0710A_SAVE_PT1				0x1
#define	fusion_F0710A_SAVE_PT2				0x2



/* fusion_F0710A touch screen information */
struct fusion_F0710A_info {
	int xres; /* x resolution */
	int yres; /* y resolution */
	int xy_reverse; /* if need reverse in the x,y value x=xres-1-x, y=yres-1-y*/    
};

struct fusion_F0710A_data {
	struct fusion_F0710A_info		info;
	struct i2c_client		*client;
	struct workqueue_struct	*workq;
	struct input_dev		*input;
	u16						x1;
	u16						y1;
	u8						z1;
	u8						tip1;
	u8						tid1;
	u16						x2;
	u16						y2;
	u8						z2;
	u8						tip2;
	u8						tid2;
	u8						f_num;
	u8						save_points;
};

