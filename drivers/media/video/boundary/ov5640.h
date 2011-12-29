#ifndef __OV5640_H_INCLUDED__
#define __OV5640_H_INCLUDED__
/*
 * Copyright 2010 Boundary Devices. All Rights Reserved.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

struct ov5640_reg_value {
	unsigned short addr;
	unsigned char value;
};

#define MAX_OV_NAME	16
struct ov5640_setting {
	char 				name[MAX_OV_NAME];
	double				fps ;			//
	unsigned 			mclk ;			// clock rate
	unsigned			v4l_fourcc ;		//
	unsigned			ipuin_fourcc ;		// camera sensor input
	unsigned			ipuout_fourcc ;		// CSI output
	unsigned			xres ;			//
	unsigned			yres ;			//
	unsigned			stride ;		//
	unsigned			sizeimage ;		//
	unsigned			hoffs ;
	unsigned			voffs ;
	unsigned			hmarg ;
	unsigned			vmarg ;
	unsigned			data_width ;
	unsigned 			clk_mode ;
	unsigned 			ext_vsync ;
	unsigned 			Vsync_pol ;
	unsigned 			Hsync_pol ;
	unsigned 			pixclk_pol ;
	unsigned 			data_pol ;
	unsigned 			pack_tight ;
	unsigned 			force_eof ;
	unsigned 			data_en_pol ;
	unsigned 			reg_count ;
	unsigned 			crc ;
#if 4 == __SIZEOF_POINTER__
	unsigned			pad ;
#endif
	struct ov5640_reg_value const 	*registers ;
};

#if 4 == __SIZEOF_POINTER__
#define OV5640_SETTING_SIZE	(sizeof(struct ov5640_setting)-sizeof(unsigned)-sizeof(void *))
#else
#define OV5640_SETTING_SIZE	(sizeof(struct ov5640_setting)-sizeof(void *))
#endif

#define MAX_REGS 1024
#define MIN_REGNUM 0x3000
#define MAX_REGNUM 0x6FFF

#endif
