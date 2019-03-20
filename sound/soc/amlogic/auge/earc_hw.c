/*
 * sound/soc/amlogic/auge/earc_hw.c
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/types.h>

#include "earc_hw.h"


void earcrx_cmdc_init(void)
{
	/* set irq mask */
	earcrx_top_write(EARCRX_CMDC_INT_MASK,
		(0 << 15) |  /* idle2_int */
		(0 << 14) |  /* idle1_int */
		(0 << 13) |  /* disc2_int */
		(0 << 12) |  /* disc1_int */
		(0 << 11) |  /* earc_int */
		(1 << 10) |  /* hb_status_int */
		(0 <<  9) |  /* losthb_int */
		(0 <<  8) |  /* timeout_int */
		(0 <<  7) |  /* status_ch_int */
		(0 <<  6) |  /* int_rec_invalid_id */
		(0 <<  5) |  /* int_rec_invalid_offset */
		(0 <<  4) |  /* int_rec_unexp */
		(0 <<  3) |  /* int_rec_ecc_err */
		(0 <<  2) |  /* int_rec_parity_err */
		(0 <<  1) |  /* int_recv_packet */
		(0 <<  0)	  /* int_rec_time_out */
		);
}

void earcrx_dmac_init(void)
{
	earcrx_dmac_write(EARCRX_DMAC_TOP_CTRL0, 1 << 31); /* reg_top_work_en */
	earcrx_dmac_write(EARCRX_DMAC_SYNC_CTRL0,
		(1 << 31)	|	 /* reg_work_en */
		(1 << 30)	|	 /* reg_rst_afifo_out_n */
		(1 << 29)	|	 /* reg_rst_afifo_in_n */
		(1 << 16)	|	 /* reg_ana_buf_data_sel_en */
		(3 << 12)	|	 /* reg_ana_buf_data_sel */
		(7 << 8)	|	 /* reg_ana_clr_cnt */
		(7 << 4)		 /* reg_ana_set_cnt */
		);
	earcrx_dmac_write(EARCRX_ERR_CORRECT_CTRL0,
		(1 << 29) | /* reg_rst_afifo_out_n */
		(1 << 28)	/* reg_rst_afifo_in_n */
		);
	earcrx_dmac_write(EARCRX_DMAC_UBIT_CTRL0,
		(1 << 31)  | /* reg_work_enable */
		(47 << 16) | /* reg_fifo_thd */
		(1 << 12)  | /* reg_user_lr */
		(29 << 0)	/* reg_data_bit */
		);
	earcrx_dmac_write(EARCRX_ANA_RST_CTRL0, 1 << 31);
	earcrx_dmac_write(EARCRX_ERR_CORRECT_CTRL0, 1 << 31); /* reg_work_en */
}

void earc_arc_init(void)
{
	earcrx_dmac_write(EARCRX_SPDIFIN_CTRL0,
		(1 << 31) | /* reg_work_en */
		(1 << 30) | /* reg_chnum_sel */
		(1 << 25) | /* reg_findpapb_en */
		(0xFFF<<12) /* reg_nonpcm2pcm_th */
		);
	earcrx_dmac_write(EARCRX_SPDIFIN_CTRL2,
		(1 << 14) | /* reg_earc_auto */
		(1 << 13)  /* reg_earcin_papb_lr */
		);
	earcrx_dmac_write(EARCRX_SPDIFIN_CTRL3,
		(0xEC37<<16) | /* reg_earc_pa_value */
		(0x5A5A<<0)    /* reg_earc_pb_value */
		);
}
