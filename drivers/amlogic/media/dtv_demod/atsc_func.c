/*
 * drivers/amlogic/media/dtv_demod/atsc_func.c
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

#include "demod_func.h"
#include <linux/dvb/aml_demod.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/mutex.h>

/* static void __iomem * demod_meson_reg_map[4]; */
#if 0
#define pr_dbg(fmt, args ...) \
	do { \
		if (debug_atsc) \
			pr_info("FE: " fmt, ## args); \
	} while (0)
#define pr_error(fmt, args ...) pr_info("FE: " fmt, ## args)
#endif

MODULE_PARM_DESC(debug_atsc, "\n\t\t Enable frontend atsc debug information");
static int debug_atsc = 1;
module_param(debug_atsc, int, 0644);

MODULE_PARM_DESC(atsc_thread_enable, "\n\t\t Enable frontend debug information");
static int atsc_thread_enable = 1;
module_param(atsc_thread_enable, int, 0644);

MODULE_PARM_DESC(ar_enable, "\n\t\t Enable ar");
static int ar_enable;
module_param(ar_enable, int, 0644);

MODULE_PARM_DESC(cci_enable, "\n\t\t Enable ar");
static int cci_enable = 1;
module_param(cci_enable, int, 0644);

MODULE_PARM_DESC(cfo_count, "\n\t\t cfo_count");
static int cfo_count;
module_param(cfo_count, int, 0644);

MODULE_PARM_DESC(field_test_version, "\n\t\t field_test_version");
static int field_test_version;
module_param(field_test_version, int, 0644);

MODULE_PARM_DESC(cfo_times, "\n\t\t cfo_times");
static int cfo_times = 30;
module_param(cfo_times, int, 0644);


static int dagc_switch;
static int ar_flag;
static int awgn_flag;

/* 8vsb */
static struct atsc_cfg list_8vsb[22] = {
	{0x0733, 0x00, 0},
	{0x0734, 0xff, 0},
	{0x0716, 0x02, 0},	/* F06[7] invert spectrum  0x02 0x06 */
	{0x05e7, 0x00, 0},
	{0x05e8, 0x00, 0},
	{0x0f06, 0x80, 0},
	{0x0f09, 0x04, 0},
	{0x070c, 0x18, 0},
	{0x070d, 0x9d, 0},
	{0x070e, 0x89, 0},
	{0x070f, 0x6a, 0},
	{0x0710, 0x75, 0},
	{0x0711, 0x6f, 0},
	{0x072a, 0x02, 0},
	{0x072c, 0x02, 0},
	{0x090d, 0x03, 0},
	{0x090e, 0x02, 0},
	{0x090f, 0x00, 0},
	{0x0900, 0x01, 0},
	{0x0900, 0x00, 0},
	{0x0f00, 0x01, 0},
	{0x0000, 0x00, 1}
};

/* 64qam */
static struct atsc_cfg list_qam64[111] = {
	{0x0900, 0x01, 0},
	{0x0f04, 0x08, 0},
	{0x0f06, 0x80, 0},
	{0x0f07, 0x00, 0},
	{0x0f00, 0xe0, 0},
	{0x0f00, 0xec, 0},
	{0x0001, 0x05, 0},
	{0x0002, 0x61, 0},	/* /0x61 invert spectrum */
	{0x0003, 0x3e, 0},
	{0x0004, 0xed, 0},	/* 0x9d */
	{0x0005, 0x10, 0},
	{0x0006, 0xc0, 0},
	{0x0007, 0x5c, 0},
	{0x0008, 0x0f, 0},
	{0x0009, 0x4f, 0},
	{0x000a, 0xfc, 0},
	{0x000b, 0x0c, 0},
	{0x000c, 0x6c, 0},
	{0x000d, 0x3a, 0},
	{0x000e, 0x10, 0},
	{0x000f, 0x02, 0},
	{0x0011, 0x00, 0},
	{0x0012, 0xf5, 0},
	{0x0013, 0x74, 0},
	{0x0014, 0xb9, 0},
	{0x0015, 0x1f, 0},
	{0x0016, 0x80, 0},
	{0x0017, 0x1f, 0},
	{0x0018, 0x0f, 0},
	{0x001e, 0x00, 0},
	{0x001f, 0x00, 0},
	{0x0023, 0x03, 0},
	{0x0025, 0x20, 0},
	{0x0026, 0xff, 0},
	{0x0027, 0xff, 0},
	{0x0028, 0xf8, 0},
	{0x0200, 0x20, 0},
	{0x0201, 0x62, 0},
	{0x0202, 0x23, 0},
	{0x0204, 0x19, 0},
	{0x0205, 0x74, 0},
	{0x0206, 0xab, 0},
	{0x0207, 0xff, 0},
	{0x0208, 0xc0, 0},
	{0x0209, 0xff, 0},
	{0x0211, 0xc0, 0},
	{0x0212, 0xb0, 0},
	{0x0213, 0x05, 0},
	{0x0215, 0x08, 0},
	{0x0222, 0xe0, 0},
	{0x0223, 0xf0, 0},
	{0x0226, 0x40, 0},
	{0x0229, 0x23, 0},
	{0x022a, 0x02, 0},
	{0x022c, 0x01, 0},
	{0x022e, 0x01, 0},
	{0x022f, 0x25, 0},
	{0x0230, 0x40, 0},
	{0x0231, 0x01, 0},
	{0x0734, 0xff, 0},
	{0x073a, 0xff, 0},
	{0x073b, 0x04, 0},
	{0x073c, 0x08, 0},
	{0x073d, 0x08, 0},
	{0x073e, 0x01, 0},
	{0x073f, 0xf8, 0},
	{0x0740, 0xf1, 0},
	{0x0741, 0xf3, 0},
	{0x0742, 0xff, 0},
	{0x0743, 0x0f, 0},
	{0x0744, 0x1a, 0},
	{0x0745, 0x16, 0},
	{0x0746, 0x00, 0},
	{0x0747, 0xe3, 0},
	{0x0748, 0xce, 0},
	{0x0749, 0xd4, 0},
	{0x074a, 0x00, 0},
	{0x074b, 0x4b, 0},
	{0x074c, 0x00, 0},
	{0x074d, 0xa2, 0},
	{0x074e, 0x00, 0},
	{0x074f, 0xe6, 0},
	{0x0750, 0x00, 0},
	{0x0751, 0x00, 0},
	{0x0752, 0x01, 0},
	{0x0753, 0x03, 0},
	{0x0400, 0x00, 0},
	{0x0408, 0x04, 0},
	{0x040e, 0xe0, 0},
	{0x0500, 0x02, 0},
	{0x05e7, 0x00, 0},
	{0x05e8, 0x00, 0},
	{0x0f09, 0x18, 0},
	{0x070c, 0x20, 0},
	{0x070d, 0x41, 0},	/* 0x49 */
	{0x070e, 0x04, 0},	/* 0x37 */
	{0x070f, 0x00, 0},
	{0x0710, 0x00, 0},
	{0x0711, 0x00, 0},
	{0x0716, 0xf0, 0},
	{0x090f, 0x00, 0},
	{0x0900, 0x01, 1},
	{0x0900, 0x00, 0},
	{0x0001, 0xf5, 0},
	{0x0001, 0xf5, 1},
	{0x0001, 0xf5, 1},
	{0x0001, 0xf5, 1},
	{0x0001, 0xf5, 1},
	{0x0001, 0x05, 0},
	{0x0001, 0x05, 1},
	{0x0000, 0x00, 1}
};

/* 256qam */
static struct atsc_cfg list_qam256[113] = {
	{0x0900, 0x01, 0},
	{0x0f04, 0x08, 0},
	{0x0f06, 0x80, 0},
	{0x0f00, 0xe0, 0},
	{0x0f00, 0xec, 0},
	{0x0001, 0x05, 0},
	{0x0002, 0x01, 0},	/* 0x09 */
	{0x0003, 0x2c, 0},
	{0x0004, 0x91, 0},
	{0x0005, 0x10, 0},
	{0x0006, 0xc0, 0},
	{0x0007, 0x5c, 0},
	{0x0008, 0x0f, 0},
	{0x0009, 0x4f, 0},
	{0x000a, 0xfc, 0},
	{0x000b, 0x0c, 0},
	{0x000c, 0x6c, 0},
	{0x000d, 0x3a, 0},
	{0x000e, 0x10, 0},
	{0x000f, 0x02, 0},
	{0x0011, 0x80, 0},
	{0x0012, 0xf5, 0},	/* a5 */
	{0x0013, 0x74, 0},
	{0x0014, 0xb9, 0},
	{0x0015, 0x1f, 0},
	{0x0016, 0x80, 0},
	{0x0017, 0x1f, 0},
	{0x0018, 0x0f, 0},
	{0x001e, 0x00, 0},
	{0x001f, 0x00, 0},
	{0x0023, 0x03, 0},
	{0x0025, 0x20, 0},
	{0x0026, 0xff, 0},
	{0x0027, 0xff, 0},
	{0x0028, 0xf8, 0},
	{0x0200, 0x20, 0},
	{0x0201, 0x62, 0},
	{0x0202, 0x23, 0},
	{0x0204, 0x19, 0},
	{0x0205, 0x76, 0},
	{0x0206, 0xd2, 0},
	{0x0207, 0xff, 0},
	{0x0208, 0xc0, 0},
	{0x0209, 0xff, 0},
	{0x0211, 0xc0, 0},
	{0x0212, 0xb0, 0},
	{0x0213, 0x05, 0},
	{0x0215, 0x08, 0},
	{0x0222, 0xf0, 0},
	{0x0223, 0xff, 0},
	{0x0226, 0x40, 0},
	{0x0229, 0x23, 0},
	{0x022a, 0x02, 0},
	{0x022c, 0x01, 0},
	{0x022e, 0x01, 0},
	{0x022f, 0x05, 0},
	{0x0230, 0x40, 0},
	{0x0231, 0x01, 0},
	{0x0400, 0x02, 0},
	{0x0401, 0x30, 0},
	{0x0402, 0x13, 0},
	{0x0406, 0x06, 0},
	{0x0408, 0x04, 0},
	{0x040e, 0xe0, 0},
	{0x0411, 0x02, 0},
	{0x073a, 0x02, 0},
	{0x073b, 0x09, 0},
	{0x073c, 0x0c, 0},
	{0x073d, 0x08, 0},
	{0x073e, 0xfd, 0},
	{0x073f, 0xf2, 0},
	{0x0740, 0xed, 0},
	{0x0741, 0xf4, 0},
	{0x0742, 0x03, 0},
	{0x0743, 0x15, 0},
	{0x0744, 0x1d, 0},
	{0x0745, 0x15, 0},
	{0x0746, 0xfc, 0},
	{0x0747, 0xde, 0},
	{0x0748, 0xcc, 0},
	{0x0749, 0xd6, 0},
	{0x074a, 0x04, 0},
	{0x074b, 0x4f, 0},
	{0x074c, 0x00, 0},
	{0x074d, 0xa2, 0},
	{0x074e, 0x00, 0},
	{0x074f, 0xe3, 0},
	{0x0750, 0x00, 0},
	{0x0751, 0xfc, 0},
	{0x0752, 0x00, 0},
	{0x0753, 0x03, 0},
	{0x0500, 0x02, 0},
	{0x05e7, 0x00, 0},
	{0x05e8, 0x00, 0},
	{0x0f09, 0x18, 0},
	{0x070c, 0x20, 0},
	{0x070d, 0x49, 0},
	{0x070e, 0x37, 0},
	{0x070f, 0x00, 0},
	{0x0710, 0x00, 0},
	{0x0711, 0x00, 0},
	{0x0716, 0xf0, 0},
	{0x090f, 0x00, 0},
	{0x0900, 0x01, 1},
	{0x0900, 0x00, 0},
	{0x0001, 0xf5, 0},
	{0x0001, 0xf5, 1},
	{0x0001, 0xf5, 1},
	{0x0001, 0xf5, 1},
	{0x0001, 0xf5, 1},
	{0x0001, 0x05, 0},
	{0x0001, 0x05, 1},
	{0x0000, 0x00, 1}
};

unsigned int SNR_table[56] = {
		0, 7, 9, 11, 14,
		17, 22, 27, 34, 43, 54,
		68, 86, 108, 136, 171,
		215, 271, 341, 429, 540,
		566, 592, 620, 649, 680,
		712, 746, 781, 818, 856,
		896, 939, 983, 1029, 1078,
		1182, 1237, 1237, 1296, 1357,
		1708, 2150, 2707, 3408, 4291,
		5402, 6800, 8561, 10778, 13568,
		16312, 17081, 18081, 19081, 65536
};
unsigned int SNR_dB_table[56] = {
			360, 350, 340, 330, 320,
			310, 300, 290, 280, 270, 260,
			250, 240, 230, 220, 210,
			200, 190, 180, 170, 160,
			158, 156, 154, 152, 150,
			148, 146, 144, 142, 140,
			138, 136, 134, 132, 130,
			128, 126, 124, 122, 120,
			110, 100, 90, 80, 70,
			60, 50, 40, 30, 20,
			12, 10, 4, 2, 0
};

void atsc_set_version(int version)
{
	if (version == 1)
		field_test_version = 1;
	else if (version == 2)
		field_test_version = 0;
	else
		field_test_version = 1;
}

/*int atsc_qam_set(fe_modulation_t mode)*/
int atsc_qam_set(enum fe_modulation mode)

{
	int i, j;

	if (mode == VSB_8) {	/* 5-8vsb, 2-64qam, 4-256qam */
		for (i = 0; list_8vsb[i].adr != 0; i++) {
			if (list_8vsb[i].rw)
				atsc_read_reg(list_8vsb[i].adr);
			/* msleep(20); */
			else
				atsc_write_reg(list_8vsb[i].adr,
					       list_8vsb[i].dat);
			/* msleep(20); */
		}
		j = 15589;
		PR_ATSC("8-vsb mode\n");
	} else if (mode == QAM_64) {
		for (i = 0; list_qam64[i].adr != 0; i++) {
			if (list_qam64[i].rw) {
				atsc_read_reg(list_qam64[i].adr);
				msleep(20);
			} else {
				atsc_write_reg(list_qam64[i].adr,
					       list_qam64[i].dat);
				msleep(20);
			}
		}
		j = 16588;	/* 33177; */
		PR_ATSC("64qam mode\n");
	} else if (mode == QAM_256) {
		for (i = 0; list_qam256[i].adr != 0; i++) {
			if (list_qam256[i].rw) {
				atsc_read_reg(list_qam256[i].adr);
				msleep(20);
			} else {
				atsc_write_reg(list_qam256[i].adr,
					       list_qam256[i].dat);
				msleep(20);
			}
		}
		j = 15649;	/* 31298; */
		PR_ATSC("256qam mode\n");
	} else {
		for (i = 0; list_qam256[i].adr != 0; i++) {
			if (list_qam256[i].rw) {
				atsc_read_reg(list_qam256[i].adr);
				msleep(20);
			} else {
				atsc_write_reg(list_qam256[i].adr,
					       list_qam256[i].dat);
				msleep(20);
			}
		}
		j = 15649;	/* 31298; */
		PR_ATSC("256qam mode\n");
	}
	return j;
}

int read_snr_atsc_tune(void)
{
	unsigned int SNR;
	unsigned int SNR_dB;
	int tmp[3];

	tmp[0] = atsc_read_reg(0x0511);
	tmp[1] = atsc_read_reg(0x0512);
	SNR = (tmp[0] << 8) + tmp[1];
	SNR_dB = SNR_dB_table[atsc_find(SNR, SNR_table, 56)];
	return SNR_dB;
}

void set_cr_ck_rate(void)
{
	atsc_write_reg(0x0f07, 0x21);/*0x50*/
	/*25M ADC B2*/
	/*atsc_write_reg(0x070c, 0x19);*/
	/*atsc_write_reg(0x070d, 0x30);*/
	/*atsc_write_reg(0x070e, 0xbe);*/
	/*atsc_write_reg(0x070f, 0x52);*/
	/*atsc_write_reg(0x0710, 0xab);*/
	/*atsc_write_reg(0x0711, 0xfe);*/
	/*24M Crystal*/
	atsc_write_reg(0x70c, 0x1a);
	atsc_write_reg(0x70d, 0xaa);
	atsc_write_reg(0x70e, 0xaa);
	atsc_write_reg(0x70f, 0x3a);
	atsc_write_reg(0x710, 0xe5);
	atsc_write_reg(0x711, 0xc9);
	atsc_write_reg(0x0f06, 0x00);
	atsc_write_reg(0x0580, 0x01);
	atsc_write_reg(0x0520, 0x09);
	atsc_write_reg(0x0f28, 0x0f);
	atsc_write_reg(0x0f2d, 0x80);
	atsc_write_reg(0x0f2b, 0x04);
	atsc_write_reg(0x07f0, 0x31);
	atsc_write_reg(0x07f1, 0x31);
	atsc_write_reg(0x07f2, 0x31);
	atsc_write_reg(0x07f3, 0x31);
	atsc_write_reg(0x07f4, 0x31);
	atsc_write_reg(0x07f5, 0x31);
	atsc_write_reg(0x07f6, 0x31);
	atsc_write_reg(0x072c, 0x04);
	/* 50 ck confirm cnt*/
	atsc_write_reg(0x054f, 0x20);
	/* 76 timeout cnt*/
	atsc_write_reg(0x0550, 0x02);
	/* 77 timeout cnt */
	atsc_write_reg(0x0572, 0x18);
	/* 77 timeout cnt */
	atsc_write_reg(0x0556, 0x04);
	/* DFE sz for 75    */
	atsc_write_reg(0x0557, 0x04);
	/*  DFE sz for 76    */
	atsc_write_reg(0x055a, 0x04);
	/* DFE sz for 79   MUST  */
	atsc_write_reg(0x0f33, 0x33);
	/*  FA stepsize 0.5 MUST */
	atsc_write_reg(0x0f32, 0x33);
	/* FA stepsize 0.5 MUST  */
	atsc_write_reg(0x0f31, 0x33);
	/* FA stepsize 0.5 MUST */
	atsc_write_reg(0x0912, 0xf0);
	/* FA stepsize 0.5 MUST  */
	atsc_write_reg(0x0555, 0x02);
	/* FA stepsize 0.5 MUST  */
	atsc_write_reg(0x053c, 0x0e);
	/* FA stepsize 0.5 MUST */
	atsc_write_reg(0x053d, 0x0e);
	/* FA stepsize 0.5 MUST  */
	atsc_write_reg(0x053b, 0x0d);
	atsc_write_reg(0x057b, 0x01);
	atsc_write_reg(0x057a, 0x0d);
	atsc_write_reg(0x0f6f, 0x81);
	atsc_write_reg(0x0735, 0x83);
	atsc_write_reg(0x0f6a, 0x00);
	atsc_write_reg(0x0724, 0x00);
	/* PN 2cond correlation threshold ,*/
	/* case138 fail if it is default 16  */
	atsc_write_reg(0x0552, 0x10);
	/* FSM79 TO cnt  */
	atsc_write_reg(0x0551, 0x30);
	/* FSM78 TO cnt  */
	atsc_write_reg(0x050e, 0x20);
	/* FSM77 TO thres 10dB  */
	atsc_write_reg(0x050f, 0x1a);
	/* FSM78 TO thres 11dB  */
	atsc_write_reg(0x0510, 0x1a);
	/* FSM78 TO thres 11dB  */
	atsc_write_reg(0x05f2, 0x82);
	/* decrease lock time 0x88->0x82  */
	atsc_write_reg(0x05f3, 0x82);
	/* decrease lock time 0x88->0x82  */
	atsc_write_reg(0x736,  0x60);
	atsc_write_reg(0x737,  0x00);
	atsc_write_reg(0x738,  0x00);
	atsc_write_reg(0x739,  0x00);
	atsc_write_reg(0x735,  0x83);
	atsc_write_reg(0x918,  0x0);

	/*filed test bug in sfo*/
	atsc_write_reg(0x53b,  0x0b);
	atsc_write_reg(0x545,  0x0);
	atsc_write_reg(0x546,  0x80);

if (!field_test_version) {
	/*improve phase nosie 20170820*/
	atsc_write_reg(0x912,  0x10);
	atsc_write_reg(0xf55,  0x00);
	atsc_write_reg(0xf56,  0x00);
	atsc_write_reg(0xf57,  0x00);
	/*improve impluse nosie 20170820*/
	atsc_write_reg(0x546,  0xa0);
	/*improve awgn*/
	atsc_write_reg(0x52d,  0x04);
	atsc_write_reg(0x562,  0x0b);
	atsc_write_reg(0x53b,  0x0d);
	atsc_write_reg(0x735,  0x00);

	/*r2,2 case*/
	atsc_write_reg(0x505, 0x19);
	atsc_write_reg(0x506, 0x15);
	atsc_write_reg(0xf6f, 0xc0);
	atsc_write_reg(0xf6e, 0x09);
	atsc_write_reg(0x562, 0x08);

	if (awgn_flag == TASK4_TASK5)
		atsc_set_performance_register(TASK4_TASK5, 1);
	else if (awgn_flag == AWGN)
		atsc_set_performance_register(AWGN, 1);
	else if (awgn_flag == TASK4_TASK5)
		atsc_set_performance_register(TASK8_R22, 1);
	atsc_write_reg(0x716, 0x02);
} else {
	PR_ATSC("!!!!!! field test !!!!!!!!!");
	atsc_write_reg(0x716, 0x02);
	atsc_write_reg(0x552, 0x20);
	atsc_write_reg(0x551, 0x28);
	atsc_write_reg(0x550, 0x08);
	atsc_write_reg(0x54f, 0x08);
	atsc_write_reg(0x54e, 0x08);
	atsc_write_reg(0x54d, 0x08);
	atsc_write_reg(0x54c, 0x08);
	atsc_write_reg(0x53b, 0x0e);
}
	ar_flag = 0;
}


void atsc_initial(struct aml_demod_sta *demod_sta)
{
	int fc, fs, cr, ck, j;
	enum fe_modulation mode;

	mode = demod_sta->ch_mode;

	j = atsc_qam_set(mode);	/* set mode */

	fs = demod_sta->adc_freq;	/* KHZ 25200 */
	fc = demod_sta->ch_if;	/* KHZ 6350 */

	cr = (fc * (1 << 17) / fs) * (1 << 6);
	ck = fs * j / 10 - (1 << 25);
	/* ck_rate = (f_samp / f_vsb /2 -1)*(1<<25);*/
	/*double f_vsb = 10.76238;// double f_64q = 5.056941;*/
	/* // double f_256q = 5.360537; */

	atsc_write_reg(0x070e, cr & 0xff);
	atsc_write_reg(0x070d, (cr >> 8) & 0xff);
	atsc_write_reg(0x070c, (cr >> 16) & 0xff);

	if (demod_sta->ch_mode == VSB_8) {
		atsc_write_reg(0x0711, ck & 0xff);
		atsc_write_reg(0x0710, (ck >> 8) & 0xff);
		atsc_write_reg(0x070f, (ck >> 16) & 0xff);
	}
	PR_ATSC("0x70e is %x, 0x70d is %x, 0x70c is %x\n",
		cr & 0xff, (cr >> 8) & 0xff, (cr >> 16) & 0xff);
	PR_ATSC("fs is %d(SR),fc is %d(IF),cr is %x,ck is %x\n",
				fs, fc, cr, ck);
}

int atsc_set_ch(struct aml_demod_sta *demod_sta,
		/*struct aml_demod_i2c *demod_i2c,*/
		struct aml_demod_atsc *demod_atsc)
{
	int ret = 0;
	u8 demod_mode;
	u8 bw, sr, ifreq, agc_mode;
	u32 ch_freq;

	bw = demod_atsc->bw;
	sr = demod_atsc->sr;
	ifreq = demod_atsc->ifreq;
	agc_mode = demod_atsc->agc_mode;
	ch_freq = demod_atsc->ch_freq;
	demod_mode = demod_atsc->dat0;
	demod_sta->ch_mode = demod_atsc->mode;	/* TODO */
	demod_sta->agc_mode = agc_mode;
	demod_sta->ch_freq = ch_freq;
	demod_sta->dvb_mode = demod_mode;
	demod_sta->ch_bw = (8 - bw) * 1000;
	/*atsc_initial(demod_sta);*/
	set_cr_ck_rate();
	atsc_reset();
	/*set_cr_ck_rate();*/
	dagc_switch = Dagc_Open;
	PR_ATSC("ATSC mode\n");
	return ret;
}

int read_atsc_fsm(void)
{
	return atsc_read_reg(0x980);
}

int atsc_read_ser(void)
{
	return atsc_read_reg(0x0687) +
		(atsc_read_reg(0x0686) << 8) +
		(atsc_read_reg(0x0685) << 16) +
		(atsc_read_reg(0x0684) << 24);
}


int atsc_find(unsigned int data, unsigned int *ptable, int len)
{
	int end;
	int index;
	int start;
	int cnt = 0;

	start = 0;
	end = len;
	while ((len > 1) && (cnt < 10)) {
		cnt++;
		index = (len / 2);
		if (data > ptable[start + index]) {
			start = start + index;
			len = len - index;
		} else if (data < ptable[start + index]) {
			len = index;
		} else if (data == ptable[start + index]) {
			start = start + index;
			break;
		}
	}
	return start;
}

void atsc_reset(void)
{
	atsc_write_reg(0x0900, 0x01);
	msleep(20);
	atsc_write_reg(0x0900, 0x00);
}


int atsc_read_snr(void)
{
	int SNR;
	int SNR_dB;

	SNR = (atsc_read_reg(0x0511) << 8) +
			atsc_read_reg(0x0512);
	SNR_dB = SNR_dB_table[atsc_find(SNR, SNR_table, 56)];
	return SNR_dB/10;

}

int atsc_read_snr_10(void)
{
	int SNR;
	int SNR_dB;

	SNR = (atsc_read_reg(0x0511) << 8) +
			atsc_read_reg(0x0512);
	SNR_dB = SNR_dB_table[atsc_find(SNR, SNR_table, 56)];
	return SNR_dB;
}


int check_snr_ser(int ser_threshholds)
{
	int ser_be, ser_af;
	int SNR;
	int SNR_dB;

	SNR = (atsc_read_reg(0x0511) << 8) +
		atsc_read_reg(0x0512);
	SNR_dB = SNR_dB_table[atsc_find(SNR, SNR_table, 56)];
	ser_be = atsc_read_ser();
	msleep(100);
	ser_af = atsc_read_ser();
	PR_ATSC("check_snr_ser [%d][%2d]\n",
		(ser_af - ser_be), SNR_dB/10);
	if (((ser_af - ser_be) > ser_threshholds) ||
		(SNR_dB/10 < 14))
		return -1;
	else
		return 0;
}

int peak[2048];
int cci_run(void)
{
	int tmp0[4], tmp1[4], tmp2[4], tmp3[4];
	int result[4], bin[4], power[4];
	int max_p[2], max_b[2], ck0;
	int cci_cnt = 0;
	int avg_len   = 4;
	int threshold;
	int time[10];

	threshold = avg_len*200;
	time[0] = jiffies_to_msecs(jiffies);
	for (ck0 = 0; ck0 < 2048; ck0++)
		peak[ck0] = 0;
	for (ck0 = 0; ck0 < 2; ck0++) {
		max_p[ck0] = 0;
		max_b[ck0] = 0;
	}
	time[1] = jiffies_to_msecs(jiffies);
	PR_ATSC("[atsc_time][cci_run1,%d ms]\n",
		(time[1] - time[0]));
	for (ck0 = 0; ck0 < avg_len; ck0++) {
		/* step1: set 0x918[0] = 1;*/
		atsc_write_reg(0x918, 0x3);
		tmp0[1] = atsc_read_reg(0x981);
		tmp0[2] = atsc_read_reg(0x982);
		tmp0[3] = atsc_read_reg(0x983);
		tmp1[1] = atsc_read_reg(0x984);
		tmp1[2] = atsc_read_reg(0x985);
		tmp1[3] = atsc_read_reg(0x986);
		tmp2[1] = atsc_read_reg(0x987);
		tmp2[2] = atsc_read_reg(0x988);
		tmp2[3] = atsc_read_reg(0x989);
		tmp3[1] = atsc_read_reg(0x98a);
		tmp3[2] = atsc_read_reg(0x98b);
		tmp3[3] = atsc_read_reg(0x98c);
		result[0] = (tmp0[3]<<16) +
			(tmp0[2]<<8) + tmp0[1];
		power[0] = result[0]>>11;
		bin[0] = result[0]&0x7ff;
		result[1] = (tmp1[3]<<16) +
			(tmp1[2]<<8) + tmp1[1];
		power[1] = result[1]>>11;
		bin[1] = result[1]&0x7ff;
		result[2] = (tmp2[3]<<16)
			+ (tmp2[2]<<8) + tmp2[1];
		power[2] = result[2]>>11;
		bin[2] = result[2]&0x7ff;
		result[3] = (tmp3[3]<<16) +
			(tmp3[2]<<8) + tmp3[1];
		power[3] = result[3]>>11;
		bin[3] = result[3]&0x7ff;

		peak[bin[0]] = peak[bin[0]] + power[0];
		peak[bin[1]] = peak[bin[1]] + power[1];
		peak[bin[2]] = peak[bin[2]] + power[2];
		peak[bin[3]] = peak[bin[3]] + power[3];
		PR_ATSC("[%x]%x,[%x]%x,[%x]%x,[%x]%x\n",
			bin[0], peak[bin[0]], bin[1],
			peak[bin[1]], bin[2], peak[bin[2]],
			bin[3], peak[bin[3]]);
	}
	time[2] = jiffies_to_msecs(jiffies);
	PR_ATSC("[atsc_time][cci_run2,%d ms]\n",
		(time[2] - time[1]));
	cci_cnt = 0;
	for (ck0 = 0; ck0 < 2048; ck0++) {
		if (peak[ck0] > threshold) {
			if (peak[ck0] > max_p[0]) {
				max_p[1] = max_p[0];/*Shift Max*/
				max_b[1] = max_b[0];
				max_p[0] = peak[ck0];
				max_b[0] = ck0;
			} else if (peak[ck0] > max_p[1]) {
				max_p[1] = peak[ck0];
				max_b[1] = ck0;
				}
			cci_cnt = cci_cnt + 1;
		}
	}
	time[3] = jiffies_to_msecs(jiffies);
	PR_ATSC("[atsc_time][cci_run3,%d ms]\n",
		(time[3] - time[2]));
	atsc_write_reg(0x736, ((max_b[0]>>8)&0x7) + 0x40);
	atsc_write_reg(0x737, max_b[0]&0xff);
	atsc_write_reg(0x738, (max_b[1]>>8)&0x7);
	atsc_write_reg(0x739, max_b[1]&0xff);
	atsc_write_reg(0x735, 0x80);
	for (ck0 = 0; ck0 < 2; ck0++) {
		PR_ATSC(" %d - %d CCI: pos %x  power %x .\n",
			cci_cnt, ck0, max_b[ck0], max_p[ck0]);
	}
	time[4] = jiffies_to_msecs(jiffies);
	PR_ATSC("[atsc_time][cci_run4,%d ms]\n", (time[4] - time[3]));
	PR_ATSC("[atsc_time]--------printf cost %d us\n",
		jiffies_to_msecs(jiffies) - time[4]);
	return 0;
}

int cfo_run(void)
{
	int crRate0, crRate1, crRate2, crRate;
	int Fcent, Fs;
	int cfo_sta, cr_peak_sta;
	int i, j;
	int sys_state;
	int table_count;
	int max_count;
	int freq_table[] = {0, -50, 50, -100, 100, -150, 150};
	int scan_range;
	int Offset;
	int detec_cfo_times = 3;


	if (cfo_count == 1)
		max_count = 3;
	else if (cfo_count == 2)
		max_count = 5;
	else
		max_count = 7;
	/*for field test do 3 times*/
	if (field_test_version) {
		max_count = 1;
		detec_cfo_times = 20;
	}
	Fcent = Si2176_5M_If*1000;/*if*/
	Fs = Adc_Clk_24M;/*crystal*/
	cfo_sta = 0;
	cr_peak_sta = 0;
	table_count = 0;
	scan_range = 10;
	Offset = freq_table[table_count];
	PR_ATSC("Fcent[%d], Fs[%d]\n", Fcent, Fs);
	for (i = -(scan_range-1); i <= scan_range+1; i++) {
		atsc_reset();
		cfo_sta = UnLock;
		cr_peak_sta = UnLock;
		crRate = (1<<10)*(Fcent+Offset)/Fs;
		crRate *= (1<<13);
		crRate0 = crRate&0xff;
		crRate1 = (crRate>>8)&0xff;
		crRate2 = (crRate>>16)&0xff;
		/*set ddc init*/
		atsc_write_reg(0x70e, crRate0);
		atsc_write_reg(0x70d, crRate1);
		atsc_write_reg(0x70c, crRate2);
		PR_ATSC("[autoscan]crRate is %x, Offset is %dkhz\n ",
			crRate, Offset);
		/*detec cfo signal*/
		for (j = 0; j < detec_cfo_times; j++) {
			sys_state = read_atsc_fsm();
			/*sys_state = (sys_state >> 4) & 0x0f;*/
			if (sys_state >= CR_Lock) {
				cfo_sta = Lock;
				PR_ATSC("fsm[%x][autoscan]cfo lock\n",
					read_atsc_fsm());
				break;
			}
			msleep(20);
		}
		PR_ATSC("fsm[%x]cfo_sta is %d\n",
			read_atsc_fsm(), cfo_sta);
		/*detec cr peak signal*/
		if (cfo_sta == Lock) {
			for (j = 0; j < cfo_times; j++) {
				sys_state = read_atsc_fsm();
				PR_ATSC("fsm[%x]in CR LOCK\n", read_atsc_fsm());
				/*sys_state = (sys_state >> 4) & 0x0f;*/
				if (sys_state >= CR_Peak_Lock) {
					cr_peak_sta = Lock;
					PR_ATSC(
					"fsm[%x][autoscan]cr peak lock\n",
						read_atsc_fsm());
					break;
				} else if (sys_state <= 20) {
					PR_ATSC("no signal,break\n");
					break;
				}
				msleep(20);
			}
		} else {
			if (table_count >= (max_count-1))
				break;
			table_count++;
			Offset = freq_table[table_count];
			continue;
		}
		/*reset*/
		if (cr_peak_sta == Lock) {
			/*atsc_reset();*/
			PR_ATSC("fsm[%x][autoscan]atsc_reset\n",
			read_atsc_fsm());
			return Cfo_Ok;

		} else {
			if (table_count >= (max_count-1)) {
				PR_ATSC("cfo not lock,will try again\n");
				return Cfo_Fail;

			}
			table_count++;
			Offset = freq_table[table_count];

		}
	}
	return 0;
}

int AR_run(void)
{
	int snr_buffer;

	msleep(100);
	snr_buffer = read_snr_atsc_tune();
	snr_buffer = snr_buffer / 10;
	PR_ATSC("[AR]AR_run, snr_buffer is %d,ar_flag is %d\n",
		snr_buffer, ar_flag);
	if ((snr_buffer > 15) && ar_flag) {
		atsc_write_reg(0xf2b, 0x4);
		ar_flag = 0;
		PR_ATSC("[AR]close AR\n");
	} else if ((snr_buffer < 12) && !ar_flag) {
		atsc_write_reg(0xf28, 0x8c);
		atsc_write_reg(0xf28, 0xc);
		atsc_write_reg(0xf2b, 0x14);
		ar_flag = 1;
		PR_ATSC("[AR]open AR\n");
	}
	return 0;
}

int atsc_check_fsm_status_oneshot(void)
{
	unsigned int SNR;
	unsigned int atsc_snr = 0;
	unsigned int SNR_dB;
	int tmp[3];
	int cr;
	int ck;
	int SM;
	int ber;
	int ser;
	int cnt;

	cnt = 0;
	ber = 0;
	ser = 0;
	tmp[0] = atsc_read_reg(0x0511);
	tmp[1] = atsc_read_reg(0x0512);
	SNR = (tmp[0] << 8) + tmp[1];
	SNR_dB = SNR_dB_table[atsc_find(SNR, SNR_table, 56)];
	tmp[2] = atsc_read_reg(0x0782);
	tmp[1] = atsc_read_reg(0x0781);
	tmp[0] = atsc_read_reg(0x0780);
	cr = tmp[0] + (tmp[1] << 8) + (tmp[2] << 16);
	tmp[0] = atsc_read_reg(0x0786);
	tmp[1] = atsc_read_reg(0x0787);
	tmp[2] = atsc_read_reg(0x0788);
	ck = (tmp[0] << 16) + (tmp[1] << 8) + tmp[2];
	SM = atsc_read_reg(0x0980);
/* ber per */
	/*Enable SER/BER*/
	atsc_write_reg(0x0601, 0x8);
	ber = atsc_read_reg(0x0683) + (atsc_read_reg(0x0682) << 8)+
		(atsc_read_reg(0x0681) << 16) + (atsc_read_reg(0x0680) << 24);
	ser = atsc_read_reg(0x0687) + (atsc_read_reg(0x0686) << 8)+
		(atsc_read_reg(0x0685) << 16) + (atsc_read_reg(0x0684) << 24);
	PR_ATSC
	    ("SNR %4x SNRdB %2d.%2d FSM %x cr %6x ck %6x",
	     SNR, (SNR_dB / 10)
	     , (SNR_dB - (SNR_dB / 10) * 10)
	     , SM, cr, ck);
	PR_ATSC
		(" ber is %8x, ser is %8x\n",
		ber, ser);

	atsc_snr = (SNR_dB / 10);
	return atsc_snr;
}

int snr_avg_100_times(void)
{
	int i;
	int snr_table[100], snr_all = 0;
	int cnt = 0;

	for (i = 0; i < 100; i++) {
		snr_table[i] = atsc_read_snr_10();
		snr_all += snr_table[i];
		cnt++;
		if (cnt > 10) {
			cnt = 0;
			msleep(20);
		};
	}
	snr_all /= 100;
	PR_ATSC("snr_all is %d\n", snr_all);
	return snr_all/10;

}

void atsc_set_r22_register(int flag)
{
	if ((flag == 1) && (awgn_flag == 0)) {
		atsc_write_reg(0x5bc, 0x01);
		awgn_flag = 1;
		PR_ATSC("open awgn setting\n");
		msleep(50);
	} else {
		awgn_flag = 0;
		/*atsc_write_reg(0x5bc, 0x08);*/
		PR_ATSC("close awgn setting\n");
	}
}

void atsc_set_performance_register(int flag, int init)
{
	if (flag == TASK4_TASK5) {
		atsc_write_reg(0x912, 0x00);
		atsc_write_reg(0x716, 0x00);
		atsc_write_reg(0x5bc, 0x01);
		awgn_flag = TASK4_TASK5;
	} else if (flag == AWGN) {
		atsc_write_reg(0x912, 0x00);
		atsc_write_reg(0x5bc, 0x01);
		atsc_write_reg(0x716, 0x02);
		awgn_flag = AWGN;
	} else {
		atsc_write_reg(0x912, 0x50);
		atsc_write_reg(0x716, 0x02);
		if (init == 0) {
			atsc_write_reg(0x5bc, 0x07);
			atsc_write_reg(0xf6e, 0xaf);
		} else {
			atsc_write_reg(0x5bc, 0x8);
			atsc_write_reg(0xf6e, 0x09);
		}
		awgn_flag = TASK8_R22;
	}
}

void atsc_thread(void)
{
	int time_table[10];
	int i;
	int time[10];
	int ret;
	int ser_thresholds;
	static int register_set_flag;
	static int fsm_status;
	int snr_now;
	char *info1 = "[atsc_time]fsm";

	fsm_status = Idle;
	ser_thresholds = 200;
	time[4] = jiffies_to_msecs(jiffies);
	fsm_status = read_atsc_fsm();

	/*change */
	if (!atsc_thread_enable)
		return;

	if (!field_test_version)
		PR_ATSC("awgn_flag is %d\n", awgn_flag);
	if (fsm_status < Atsc_Lock) {
		/*step1:open dagc*/

		fsm_status = read_atsc_fsm();
		PR_ATSC("fsm[%x]not lock,need to run cci\n", fsm_status);
		time[0] = jiffies_to_msecs(jiffies);
		/*step2:run cci*/
		set_cr_ck_rate();
		atsc_reset();
		register_set_flag = 0;
		/*step:check AR*/
		if (ar_enable)
			AR_run();
//		if (cci_enable && !field_test_version)
//			ret = cci_run();
		atsc_reset();
		time[1] = jiffies_to_msecs(jiffies);
		time_table[0] = (time[1]-time[0]);
		fsm_status = read_atsc_fsm();
		PR_ATSC("%s[%x]cci finish,need to run cfo,cost %d ms\n",
			info1, fsm_status, time_table[0]);
		if (fsm_status >= Atsc_Lock) {
			return;
		} else if (fsm_status < CR_Lock) {
			/*step3:run cfo*/
			ret = cfo_run();
		} else {
			ret = Cfo_Ok;
			msleep(100);
		}
		time[2] = jiffies_to_msecs(jiffies);
		time_table[1] = (time[2] - time[1]);
		PR_ATSC("fsm[%x][atsc_time]cfo done,cost %d ms,\n",
			read_atsc_fsm(), time_table[1]);
		if (ret == Cfo_Fail)
			return;
		if (cci_enable)
			ret = cci_run();
		for (i = 0; i < 80; i++) {
			fsm_status = read_atsc_fsm();
			if (fsm_status >= Atsc_Lock) {
				time[3] = jiffies_to_msecs(jiffies);
				PR_ATSC("----------------------\n");
				time_table[2] = (time[3] - time[2]);
				time_table[3] = (time[3] - time[0]);
				time_table[4] = (time[3] - time[5]);
				PR_ATSC("%s[%x]cfo->fec lock cost %d ms\n",
					info1, fsm_status, time_table[2]);
				PR_ATSC("%s[%x]lock,one cost %d ms,\n",
					info1, fsm_status, time_table[3]);
				break;
			} else if (fsm_status <= Idle) {
				PR_ATSC("atsc idle,retune, and reset\n");
				set_cr_ck_rate();
				atsc_reset();
				awgn_flag = TASK8_R22;
				break;
			}
			msleep(20);
		}
	} else {
		time[4] = jiffies_to_msecs(jiffies);
		atsc_check_fsm_status_oneshot();
		if (ar_enable)
			AR_run();
		fsm_status = read_atsc_fsm();
		PR_ATSC("lock\n");
		msleep(100);
		if (!field_test_version) {
			if (register_set_flag == 0) {
				snr_now = snr_avg_100_times();
				PR_ATSC("snr_now is %d\n", snr_now);
				if ((snr_now <= 16) && snr_now >= 10) {
					atsc_set_performance_register
					(AWGN, 0);
				} else if (snr_now < 10) {
					atsc_set_performance_register
					(TASK4_TASK5, 0);
				} else {
					if (snr_now > 23) {
						atsc_set_performance_register
						(TASK4_TASK5, 0);
						PR_ATSC("snr(23)\n");
					} else if ((snr_now > 16)
							&& (snr_now <= 23)) {
						atsc_set_performance_register
						(TASK8_R22, 0);
						PR_ATSC("snr(16,23)\n");
					}

				}
				register_set_flag = 1;
				msleep(200);
				/*%lx to %x*/
				PR_ATSC("912 is %x,5bc is %x\n",
					atsc_read_reg(0x912),
					atsc_read_reg(0x5bc));
			}
		}

		time[5] = jiffies_to_msecs(jiffies);
		PR_ATSC("fsm[%x][atsc_time]check lock %d ms,\n",
					fsm_status, time[5]-time[4]);
	}

}

#if 0
static dtmb_cfg_t list_dtmb_v1[99] = {
	{0x00000000, 0x01, 0},
	{0x00001000, 0x02, 0},
	{0x00000000, 0x03, 0},
	{0x00000000, 0x04, 0},
	{0x00000000, 0x05, 0},
	{0x00000000, 0x06, 0},
	{0x007fffff, 0x07, 0},
	{0x0000000f, 0x08, 0},
	{0x00003000, 0x09, 0},
	{0x00000001, 0x0a, 0},
	{0x0c403006, 0x0b, 0},
	{0x44444400, 0x0c, 0},
	{0x1412c320, 0x0d, 0},
	{0x00000152, 0x10, 0},
	{0x47080137, 0x11, 0},
	{0x02200a16, 0x12, 0},
	{0x42190190, 0x13, 0},
	{0x7f807f80, 0x14, 0},
	{0x0000199a, 0x15, 0},
	{0x000a1466, 0x18, 0},
	{0x00274217, 0x1a, 0},
	{0x00131036, 0x1b, 1},
	{0x00000396, 0x1c, 0},
	{0x0037f3cc, 0x1d, 0},
	{0x00000029, 0x1e, 0},
	{0x0004f031, 0x1f, 0},
	{0x00f3cbd4, 0x20, 0},
	{0x0000007e, 0x21, 0},
	{0x23270b6a, 0x22, 0},
	{0x5f700c1b, 0x23, 0},
	{0x00133c2b, 0x24, 0},
	{0x2d3e0f12, 0x25, 0},
	{0x06363038, 0x26, 0},
	{0x060e0a3e, 0x27, 0},
	{0x0015161f, 0x28, 0},
	{0x0809031b, 0x29, 0},
	{0x181c0307, 0x2a, 0},
	{0x051f1a1b, 0x2b, 0},
	{0x00451dce, 0x2c, 0},
	{0x242fde12, 0x2d, 0},
	{0x0034e8fa, 0x2e, 0},
	{0x00000007, 0x30, 0},
	{0x16000d0c, 0x31, 0},
	{0x0000011f, 0x32, 0},
	{0x01000200, 0x33, 0},
	{0x10bbf376, 0x34, 0},
	{0x00000044, 0x35, 0},
	{0x00000000, 0x36, 0},
	{0x00000000, 0x37, 0},
	{0x00000000, 0x38, 0},
	{0x00000000, 0x39, 0},
	{0x00000031, 0x3a, 0},
	{0x4d6b0a58, 0x3b, 0},
	{0x00000c04, 0x3c, 0},
	{0x0d3b0a50, 0x3d, 0},
	{0x03140480, 0x3e, 0},
	{0x05e60452, 0x3f, 0},
	{0x05780400, 0x40, 0},
	{0x0063c025, 0x41, 0},
	{0x05050202, 0x42, 0},
	{0x5e4a0a14, 0x43, 0},
	{0x00003b42, 0x44, 0},
	{0xa53080ff, 0x45, 0},
	{0x00000000, 0x46, 0},
	{0x00133202, 0x47, 0},
	{0x01f00000, 0x48, 0},
	{0x00000000, 0x49, 0},
	{0x00000000, 0x4a, 0},
	{0x00000000, 0x4b, 0},
	{0x00000000, 0x4c, 0},
	{0x20405dc8, 0x4d, 0},
	{0x00000000, 0x4e, 0},
	{0x1f0205df, 0x4f, 0},
	{0x00001120, 0x50, 0},
	{0x4f190803, 0x51, 0},
	{0x00000000, 0x52, 0},
	{0x00000040, 0x53, 0},
	{0x00100050, 0x54, 0},
	{0x00cd1000, 0x55, 0},
	{0x00010fab, 0x56, 0},
	{0x03f0fc3f, 0x58, 0},
	{0x02005014, 0x59, 0},
	{0x01405014, 0x5a, 0},
	{0x00014284, 0x5b, 0},
	{0x00000320, 0x5c, 0},
	{0x14130e05, 0x5d, 0},
	{0x4321c963, 0x5f, 0},
	{0x624668f8, 0x60, 0},
	{0xccc08888, 0x61, 0},
	{0x13212111, 0x62, 0},
	{0x21100000, 0x63, 0},
	{0x624668f8, 0x64, 0},
	{0xccc08888, 0x65, 0},
	{0x13212111, 0x66, 0},
	{0x21100000, 0x67, 0},
	{0x624668f8, 0x68, 0},
	{0xccc08888, 0x69, 0},
	{0x0, 0x0, 0}
};
#endif

int find_2(int data, int *table, int len)
{
	int end;
	int index;
	int start;
	int cnt = 0;

	start = 0;
	end = len;
	/* printf("data is %d\n",data); */
	while ((len > 1) && (cnt < 10)) {
		cnt++;
		index = (len / 2);
		if (data > table[start + index]) {
			start = start + index;
			len = len - index - 1;
		}
		if (data < table[start + index]) {
			len = index + 1;
		} else if (data == table[start + index]) {
			start = start + index;
			break;
		}
	}
	return start;
}

int read_atsc_all_reg(void)
{
	return 0;
#if 0
	int i, j, k;
	unsigned long data;

	j = 4;

	PR_ATSC("system agc is:");	/* system agc */
	for (i = 0xc00; i <= 0xc0c; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0xc80; i <= 0xc87; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	PR_ATSC("\n vsb control is:");	/*vsb control */
	j = 4;
	for (i = 0x900; i <= 0x905; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x908; i <= 0x912; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x917; i <= 0x91b; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x980; i <= 0x992; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	PR_ATSC("\n vsb demod is:");	/*vsb demod */
	j = 4;
	for (i = 0x700; i <= 0x711; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x716; i <= 0x720; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x722; i <= 0x724; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x726; i <= 0x72c; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x730; i <= 0x732; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x735; i <= 0x751; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x780; i <= 0x795; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x752; i <= 0x755; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	PR_ATSC("\n vsb equalizer is:");	/*vsb equalizer */
	j = 4;
	for (i = 0x501; i <= 0x5ff; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	PR_ATSC("\n vsb fec is:");	/*vsb fec */
	j = 4;
	for (i = 0x601; i <= 0x601; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x682; i <= 0x685; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	PR_ATSC("\n qam demod is:");	/*qam demod */
	j = 4;
	for (i = 0x1; i <= 0x1a; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x25; i <= 0x28; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x101; i <= 0x10b; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x206; i <= 0x207; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	PR_ATSC("\n qam equalize is:");	/*qam equalize */
	j = 4;
	for (i = 0x200; i <= 0x23d; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x260; i <= 0x275; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	PR_ATSC("\n qam fec is:");	/*qam fec */
	j = 4;
	for (i = 0x400; i <= 0x418; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	PR_ATSC("\n system mpeg formatter is:"); /*system mpeg formatter */
	j = 4;
	for (i = 0xf00; i <= 0xf09; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			PR_ATSC("\n[addr:0x%x]", i);
			j = 0;
		}
		PR_ATSC("%02x   ", data);
		j++;
	}
	PR_ATSC("\n\n");
	return 0;
#endif
}

int check_atsc_fsm_status(void)
{
	unsigned int SNR;
	unsigned int atsc_snr = 0;
	unsigned int SNR_dB;
	int tmp[3];
	int cr;
	int ck;
	int SM;
	int tni;
	int ber;
	int ser;
	int cnt;
	unsigned int cap_status, cap_usr_dc_addr;

	cnt = 0;
	ber = 0;
	ser = 0;
	tni = atsc_read_reg((0x08) >> 16);
	tmp[0] = atsc_read_reg(0x0511);
	tmp[1] = atsc_read_reg(0x0512);
	SNR = (tmp[0] << 8) + tmp[1];
	SNR_dB = SNR_dB_table[atsc_find(SNR, SNR_table, 56)];
	tmp[2] = atsc_read_reg(0x0782);
	tmp[1] = atsc_read_reg(0x0781);
	tmp[0] = atsc_read_reg(0x0780);
	cr = tmp[0] + (tmp[1] << 8) + (tmp[2] << 16);
	tmp[0] = atsc_read_reg(0x0786);
	tmp[1] = atsc_read_reg(0x0787);
	tmp[2] = atsc_read_reg(0x0788);
	ck = (tmp[0] << 16) + (tmp[1] << 8) + tmp[2];
	SM = atsc_read_reg(0x0980);
	/* ber per */
	atsc_write_reg(0x0601, 0x8);/* Enable SER/BER*/
	ber = atsc_read_reg(0x0683) + (atsc_read_reg(0x0682) << 8)
		+ (atsc_read_reg(0x0681) << 16) +
		(atsc_read_reg(0x0680) << 24);
	ser = atsc_read_reg(0x0687) + (atsc_read_reg(0x0686) << 8)
		+ (atsc_read_reg(0x0685) << 16) +
		(atsc_read_reg(0x0684) << 24);
	cap_status = app_apb_read_reg(0x196);
	cap_usr_dc_addr = app_apb_read_reg(0x1a1);

/* read_atsc_all_reg(); */

	PR_ATSC
		("INT %x SNR %4x SNRdB %2d.%2d FSM %x cr %6x ck %6x",
		 tni, SNR, (SNR_dB / 10)
		 , (SNR_dB - (SNR_dB / 10) * 10)
		 , SM, cr, ck);
	PR_ATSC
		(" ber is %8x, ser is %8x, cap %4d cap_addr %8x\n",
		ber, ser, cap_status>>16, cap_usr_dc_addr);

	atsc_snr = (SNR_dB / 10);
	return atsc_snr;
}

