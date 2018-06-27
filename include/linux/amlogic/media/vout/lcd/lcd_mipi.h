/*
 * include/linux/amlogic/media/vout/lcd/lcd_mipi.h
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

#ifndef _INC_LCD_MIPI_H
#define _INC_LCD_MIPI_H

/* **********************************
 * mipi-dsi read/write api
 */

/* mipi command(payload) */
/* format:  data_type, num, data.... */
/* special: data_type=0xff,
 *	    num<0xff means delay ms, num=0xff means ending.
 */

/* *************************************************************
 * Function: dsi_write_cmd
 * Supported Data Type: DT_GEN_SHORT_WR_0, DT_GEN_SHORT_WR_1, DT_GEN_SHORT_WR_2,
			DT_DCS_SHORT_WR_0, DT_DCS_SHORT_WR_1,
			DT_GEN_LONG_WR, DT_DCS_LONG_WR,
			DT_SET_MAX_RET_PKT_SIZE
			DT_GEN_RD_0, DT_GEN_RD_1, DT_GEN_RD_2,
			DT_DCS_RD_0
 * Return:              command number
 */
extern int dsi_write_cmd(unsigned char *payload);

/* *************************************************************
 * Function: dsi_read_single
 * Supported Data Type: DT_GEN_RD_0, DT_GEN_RD_1, DT_GEN_RD_2,
			DT_DCS_RD_0
 * Return:              data count
			0 for not support
 */
extern int dsi_read_single(unsigned char *payload, unsigned char *rd_data,
		unsigned int rd_byte_len);

extern int dsi_set_operation_mode(unsigned char op_mode);

#endif
