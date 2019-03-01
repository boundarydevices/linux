/*
 * drivers/amlogic/media/dtv_demod/include/depend.h
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

#ifndef __DEPEND_H__
#define __DEPEND_H__

#include <linux/device.h>	/**/


extern void vdac_enable(bool on, unsigned int module_sel);

/*dma_alloc_from_contiguous*/
struct page *aml_dma_alloc_contiguous(struct device *dev, int count,
						unsigned int order);
/*dma_release_from_contiguous*/
bool aml_dma_release_contiguous(struct device *dev, struct page *pages,
						int count);


/*void ary_test(void);*/
enum aml_fe_n_mode_t {		/*same as aml_fe_mode_t in aml_fe.h*/
	AM_FE_UNKNOWN_N,
	AM_FE_QPSK_N,
	AM_FE_QAM_N,
	AM_FE_OFDM_N,
	AM_FE_ATSC_N,
	/*AM_FE_ANALOG = 16,*/
	AM_FE_DTMB_N,
	AM_FE_ISDBT_N,
	AM_FE_NUM,
};
/*----------------------------------*/

struct aml_exp_func {

	int (*leave_mode)(enum aml_fe_n_mode_t mode);
};

#endif	/*__DEPEND_H__*/
