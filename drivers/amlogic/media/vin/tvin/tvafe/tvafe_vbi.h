/*
 * drivers/amlogic/media/vin/tvin/tvafe/tvafe_vbi.h
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

#ifndef TVIN_VBI_H_
#define TVIN_VBI_H_

/* Standard Linux Headers */
#include <linux/cdev.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>

/* *************************************************** */
/* *** macro definitions ***************************** */
/* *************************************************** */
/* defines for vbi spec */
/* vbi type id */
#define VBI_ID_CC                  0
#define VBI_ID_TT                  3
#define VBI_ID_WSS625              4
#define VBI_ID_WSSJ                5
#define VBI_ID_VPS                 6
#define MAX_PACKET_TYPE            VBI_ID_VPS

/* vbi package data bytes */
#define VBI_PCNT_USCC              2
#define VBI_PCNT_WSS625            2
#define VBI_PCNT_WSSJ              3
#define VBI_PCNT_VPS               8

/* teletext package data bytes */
#define VBI_PCNT_TT_625A           37
#define VBI_PCNT_TT_625B           42
#define VBI_PCNT_TT_625C           33
#define VBI_PCNT_TT_625D           34
#define VBI_PCNT_TT_525B           34
#define VBI_PCNT_TT_525C           33
#define VBI_PCNT_TT_525D           34

/* Teletext system type */
#define VBI_SYS_TT_625A            0
#define VBI_SYS_TT_625B            1
#define VBI_SYS_TT_625C            2
#define VBI_SYS_TT_625D            3
#define VBI_SYS_TT_525B            5
#define VBI_SYS_TT_525C            6
#define VBI_SYS_TT_525D            7


/* vbi data type setting */
#define VBI_DATA_TYPE_NULL         0x00
#define VBI_DATA_TYPE_USCC         0x11
#define VBI_DATA_TYPE_EUROCC       0x22
#define VBI_DATA_TYPE_VPS          0x33
#define VBI_DATA_TYPE_TT_625A      0x55
#define VBI_DATA_TYPE_TT_625B      0x66
#define VBI_DATA_TYPE_TT_625C      0x77
#define VBI_DATA_TYPE_TT_625D      0x88
#define VBI_DATA_TYPE_TT_525B      0x99
#define VBI_DATA_TYPE_TT_525C      0xaa
#define VBI_DATA_TYPE_TT_525D      0xbb
#define VBI_DATA_TYPE_WSS625       0xcc
#define VBI_DATA_TYPE_WSSJ         0xdd


/* vbi start code,TT start code is programmable by software,*/
/* but our ic use the programmable value as reverse!!*/
/* so wo should use the reverse value */
#define VBI_START_CODE_USCC         0x01
#define VBI_START_CODE_EUROCC       0x01
#define VBI_START_CODE_VPS          0x8a99
#define VBI_START_CODE_TT_625A      0xe7
#define VBI_START_CODE_TT_625A_REVERSE      0xe7
#define VBI_START_CODE_TT_625B      0xe4
#define VBI_START_CODE_TT_625B_REVERSE      0x27
#define VBI_START_CODE_TT_625C      0xe7
#define VBI_START_CODE_TT_625C_REVERSE      0xe7
#define VBI_START_CODE_TT_625D      0xe5
#define VBI_START_CODE_TT_625D_REVERSE      0xa7
#define VBI_START_CODE_TT_525B      0xe4
#define VBI_START_CODE_TT_525B_REVERSE      0x27
#define VBI_START_CODE_TT_525C      0xe7
#define VBI_START_CODE_TT_525C_REVERSE      0xe7
#define VBI_START_CODE_TT_525D      0xe5
#define VBI_START_CODE_TT_525D_REVERSE      0xa7
#define VBI_START_CODE_WSS625       0x1e3c1f
#define VBI_START_CODE_WSSJ         0x02

/*DTO calculation method:*/
/*For WSSJ:*/
/*DTO_Value = output_sampling_rate/(3.579545)*(2^11)*/
/*For WSS625*/
/*DTO_Value = output_sampling_rate*3/10 * (2^11)*/
/*For CLOSE CAPTION*/
/*DTO_Value = output_sampling_rate/(4*vbi_data_rate) * (2^11)*/
/*Otherwise*/
/*DTO_Value = input_sampling_rate/(2*vbi_data_rate) * (2^11)*/
/*!!!Note: DTO_Value should be round!!!*/
/*output_sampling_rate = 13.5M;*/
/*input_sampling_rate= 24M;*/
/*vbi_data_rate reference to vbi document.*/
#define VBI_DTO_USCC		0x35a0
#define VBI_DTO_EURCC		0x3600
#define VBI_DTO_TT625A		0x0f7a
#define VBI_DTO_TT625B		0x0dd6
#define VBI_DTO_TT625C		0x10be
#define VBI_DTO_TT625D		0x1103
#define VBI_DTO_TT525B		0x10c3
#define VBI_DTO_TT525C		0x10c3
#define VBI_DTO_TT525D		0x10c3
#define VBI_DTO_WSS625		0x2066
#define VBI_DTO_WSSJ		0x1e2c
#define VBI_DTO_VPS		0x1333


#define VBI_LINE_MIN               6
#define VBI_LINE_MAX               25

enum vbi_package_type_e {
	VBI_PACKAGE_CC1 = 1,
	VBI_PACKAGE_CC2 = 2,
	VBI_PACKAGE_CC3 = 4,
	VBI_PACKAGE_CC4 = 8,
	VBI_PACKAGE_TT1 = 16,
	VBI_PACKAGE_TT2 = 32,
	VBI_PACKAGE_TT3 = 64,
	VBI_PACKAGE_TT4 = 128,
	VBI_PACKAGE_XDS = 256
};

#define VBI_PACKAGE_FILTER_MAX     3

#define VBI_IOC_MAGIC 'X'

#define VBI_IOC_CC_EN              _IO(VBI_IOC_MAGIC, 0x01)
#define VBI_IOC_CC_DISABLE         _IO(VBI_IOC_MAGIC, 0x02)
#define VBI_IOC_SET_TYPE           _IOW(VBI_IOC_MAGIC, 0x03, int)
#define VBI_IOC_S_BUF_SIZE         _IOW(VBI_IOC_MAGIC, 0x04, int)
#define VBI_IOC_START              _IO(VBI_IOC_MAGIC, 0x05)
#define VBI_IOC_STOP               _IO(VBI_IOC_MAGIC, 0x06)


/*#define VBI_MEM_SIZE               0x80000*/
/* 0x8000   // 32768 hw address with 8bit not 64bit */
/* #define VBI_SLICED_MAX            64*/
/* 32768 hw address with 8bit not 64bit */
/* before m6tvd vbi_write_burst_byte = 8;*/
/*  after g9tv vbi_write_burst_byte = 16*/
#define VBI_WRITE_BURST_BYTE        16

/* debug defines */
#define VBI_CC_SUPPORT
#define VBI_TT_SUPPORT
#define VBI_WSS_SUPPORT
#define VBI_VPS_SUPPORT

#define VBI_DATA_TYPE_LEN          16
#define VBI_DATA_TYPE_MASK         0xf0000

#define VBI_PAC_TYPE_LEN           0
#define VBI_PAC_TYPE_MASK          0x0ffff

#define VBI_PAC_CC_FIELD_LEN       4
#define VBI_PAC_CC_FIELD_MASK      0xf0
#define VBI_PAC_CC_FIELD1          0
#define VBI_PAC_CC_FIELD2          1

/* vbi_slicer type */
enum vbi_slicer_e {
	VBI_TYPE_NULL = 0,
	VBI_TYPE_USCC = 0x00001,
	VBI_TYPE_EUROCC = 0x00020,
	VBI_TYPE_VPS = 0x00040,
	/* Germany, Austria and Switzerland. */
	VBI_TYPE_TT_625A = 0x00080,
	VBI_TYPE_TT_625B  = 0x00100,
	VBI_TYPE_TT_625C = 0x00200,
	VBI_TYPE_TT_625D  = 0x00400,
	VBI_TYPE_TT_525B  = 0x00800,
	VBI_TYPE_TT_525C = 0x01000,
	VBI_TYPE_TT_525D  = 0x02000,
	VBI_TYPE_WSS625 = 0x04000,
	VBI_TYPE_WSSJ = 0x08000
};

#define VBI_DEFAULT_BUFFER_SIZE 8192 /*set default buffer size--8KByte*/
#define VBI_DEFAULT_BUFFER_PACKAGE_NUM 100 /* default buffer size */


/**** enum definitions *************************/

enum field_id_e {
	FIELD_1 = 0,
	FIELD_2 = 1,
};

enum vbi_state_e {
	VBI_STATE_FREE      = 0,
	VBI_STATE_ALLOCATED = 1,
	VBI_STATE_SET       = 2,
	VBI_STATE_GO        = 3,
	VBI_STATE_DONE      = 4,
	VBI_STATE_TIMEDOUT  = 5
};


/**** structure definitions ******/

struct vbi_data_s {
	unsigned int vbi_type:8;
	unsigned int field_id:8;
	unsigned int tt_sys:8;/*tt*/
	unsigned int nbytes:16;
	unsigned int line_num:16;
	unsigned char b[42];         /* 42 for TT-625B */
};

struct vbi_ringbuffer_s {
	char *data;
	unsigned int size;
	unsigned int pread;
	unsigned int pwrite;
	int error;
	/*number of struct vbi_data_s*/
	unsigned int data_num;
	/* write to user space mode;*/
	/* 0: write struct vbi_data_s;1:write vbi_data_s->b */
	unsigned int data_wmode;
	wait_queue_head_t queue;
	spinlock_t lock;
};

struct vbi_slicer_s {
	enum vbi_slicer_e type;
	enum vbi_state_e state;

	struct vbi_ringbuffer_s buffer;
	struct mutex mutex;

	unsigned int reserve;
};

/* vbi device structure */
struct vbi_dev_s {
	dev_t devt;
	struct cdev cdev;
	struct device *dev;

	struct tasklet_struct tsklt_slicer;

	char irq_name[12];
	unsigned int vs_irq;
	unsigned int irq_free_status;
	spinlock_t vbi_isr_lock;

    /* vbi memory */
	unsigned int mem_start;
	unsigned int mem_size;

	unsigned char *pac_addr;
	unsigned char *pac_addr_start;
	unsigned char *pac_addr_end;
	unsigned int current_pac_wptr;
	unsigned int vs_delay;
	/* skip start frame vs for the vbi data is not ready so quickly */
	unsigned int vbi_data_type;
	unsigned int vbi_start_code;
	unsigned int vbi_dto_cc;
	unsigned int vbi_dto_tt;
	unsigned int vbi_dto_wss;
	unsigned int vbi_dto_vps;
	struct vbi_slicer_s *slicer;
	bool vbi_start;
	struct mutex mutex;
	spinlock_t lock;
	struct timer_list timer;
	bool tasklet_enable;
};

/*1: tvafe clk enable;*/
/*0: tvafe clk disable*/
/*read write cvd acd reg will crash when clk disabled*/
extern bool tvafe_clk_status;
extern unsigned int vbi_mem_start;

#endif /* TVIN_VBI_H_ */
