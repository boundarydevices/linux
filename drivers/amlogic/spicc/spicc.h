/*
 * drivers/amlogic/spicc/spicc.h
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

#ifndef __SPICC_H__
#define __SPICC_H__

#define SPICC_FIFO_SIZE 16
#define SPICC_DEFAULT_BIT_WIDTH 8
#define SPICC_DEFAULT_SPEED_HZ 3000000

#define SPICC_REG_RXDATA (0<<2)
#define SPICC_REG_TXDATA (1<<2)
#define SPICC_REG_CON    (2<<2)
#define SPICC_REG_INT    (3<<2)
#define SPICC_REG_DMA    (4<<2)
#define SPICC_REG_STA    (5<<2)
#define SPICC_REG_PERIOD (6<<2)
#define SPICC_REG_TEST   (7<<2)
#define SPICC_REG_DRADDR (8<<2)
#define SPICC_REG_DWADDR (9<<2)
#define SPICC_REG_LD_CNTL0 (10<<2)
#define SPICC_REG_LD_CNTL1 (11<<2)
#define SPICC_REG_LD_RADDR (12<<2)
#define SPICC_REG_LD_WADDR (13<<2)
#define SPICC_REG_ENHANCE_CNTL (14<<2)
#define SPICC_REG_ENHANCE_CNTL1 (15<<2)

#define bits_desc(reg_offset, bits_offset, bits_len) \
	(((bits_len)<<24)|((bits_offset)<<16)|(reg_offset))
#define of_mem_offset(bd) ((bd)&0xffff)
#define of_bits_offset(bd) (((bd)>>16)&0xff)
#define of_bits_len(bd) (((bd)>>24)&0xff)

#define CON_ENABLE bits_desc(SPICC_REG_CON, 0, 1)
#define CON_MODE bits_desc(SPICC_REG_CON, 1, 1)
#define CON_XCH bits_desc(SPICC_REG_CON, 2, 1)
#define CON_SMC bits_desc(SPICC_REG_CON, 3, 1)
#define CON_CLK_POL bits_desc(SPICC_REG_CON, 4, 1)
#define CON_CLK_PHA bits_desc(SPICC_REG_CON, 5, 1)
#define CON_SS_CTL bits_desc(SPICC_REG_CON, 6, 1)
#define CON_SS_POL bits_desc(SPICC_REG_CON, 7, 1)
#define CON_DRCTL bits_desc(SPICC_REG_CON, 8, 2)
#define CON_CHIP_SELECT bits_desc(SPICC_REG_CON, 12, 2)
#define CON_DATA_RATE_DIV bits_desc(SPICC_REG_CON, 16, 3)
#define CON_BITS_PER_WORD bits_desc(SPICC_REG_CON, 19, 6)
#define CON_BURST_LEN bits_desc(SPICC_REG_CON, 25, 7)
#define BURST_LEN_MAX 120
#define DMA_RX_FIFO_TH_MAX 15

#define INT_TX_EMPTY_EN bits_desc(SPICC_REG_INT, 0, 1)
#define INT_TX_HALF_EN bits_desc(SPICC_REG_INT, 1, 1)
#define INT_TX_FULL_EN bits_desc(SPICC_REG_INT, 2, 1)
#define INT_RX_READY_EN bits_desc(SPICC_REG_INT, 3, 1)
#define INT_RX_HALF_EN bits_desc(SPICC_REG_INT, 4, 1)
#define INT_RX_FULL_EN bits_desc(SPICC_REG_INT, 5, 1)
#define INT_RX_OF_EN bits_desc(SPICC_REG_INT, 6, 1)
#define INT_XFER_COM_EN bits_desc(SPICC_REG_INT, 7, 1)

#define DMA_EN bits_desc(SPICC_REG_DMA, 0, 1)
#define DMA_TX_FIFO_TH bits_desc(SPICC_REG_DMA, 1, 5)
#define DMA_RX_FIFO_TH bits_desc(SPICC_REG_DMA, 6, 5)
#define DMA_NUM_RD_BURST bits_desc(SPICC_REG_DMA, 11, 4)
#define DMA_NUM_WR_BURST bits_desc(SPICC_REG_DMA, 15, 4)
#define DMA_URGENT bits_desc(SPICC_REG_DMA, 19, 1)
#define DMA_THREAD_ID bits_desc(SPICC_REG_DMA, 20, 6)
#define DMA_BURST_NUM bits_desc(SPICC_REG_DMA, 26, 6)

#define STA_TX_EMPTY bits_desc(SPICC_REG_STA, 0, 1)
#define STA_TX_HALF bits_desc(SPICC_REG_STA, 1, 1)
#define STA_TX_FULL bits_desc(SPICC_REG_STA, 2, 1)
#define STA_RX_READY bits_desc(SPICC_REG_STA, 3, 1)
#define STA_RX_HALF bits_desc(SPICC_REG_STA, 4, 1)
#define STA_RX_FULL bits_desc(SPICC_REG_STA, 5, 1)
#define STA_RX_OF bits_desc(SPICC_REG_STA, 6, 1)
#define STA_XFER_COM bits_desc(SPICC_REG_STA, 7, 1)

#define WAIT_CYCLES bits_desc(SPICC_REG_PERIOD, 0, 14)

#define TX_COUNT bits_desc(SPICC_REG_TEST, 0, 5)
#define RX_COUNT bits_desc(SPICC_REG_TEST, 5, 5)
#define LOOPBACK_EN bits_desc(SPICC_REG_TEST, 14, 1)
#define SWAP_EN bits_desc(SPICC_REG_TEST, 15, 1)
#define DELAY_CONTROL bits_desc(SPICC_REG_TEST, 16, 6)
#define FIFO_RESET bits_desc(SPICC_REG_TEST, 22, 2)
#define CLK_FREE_EN bits_desc(SPICC_REG_TEST, 24, 1)

#define CS_DELAY bits_desc(SPICC_REG_ENHANCE_CNTL, 0, 16)
#define ENHANCE_CLK_DIV bits_desc(SPICC_REG_ENHANCE_CNTL, 16, 8)
#define ENHANCE_CLK_DIV_SELECT bits_desc(SPICC_REG_ENHANCE_CNTL, 24, 1)
#define MOSI_OEN bits_desc(SPICC_REG_ENHANCE_CNTL, 25, 1)
#define CLK_OEN bits_desc(SPICC_REG_ENHANCE_CNTL, 26, 1)
#define CS_OEN bits_desc(SPICC_REG_ENHANCE_CNTL, 27, 1)
#define CS_DELAY_EN bits_desc(SPICC_REG_ENHANCE_CNTL, 28, 1)
#define MAIN_CLK_AO bits_desc(SPICC_REG_ENHANCE_CNTL, 29, 1)

#define MISO_I_CAPTURE_EN     bits_desc(SPICC_REG_ENHANCE_CNTL1, 0, 1)
#define MISO_I_CAPTURE_DELAY  bits_desc(SPICC_REG_ENHANCE_CNTL1, 1, 9)
#define MOSI_I_CAPTURE_EN     bits_desc(SPICC_REG_ENHANCE_CNTL1, 14, 1)
#define FCLK_EN               bits_desc(SPICC_REG_ENHANCE_CNTL1, 15, 1)
#define MOSI_I_DLYCTL_EN      bits_desc(SPICC_REG_ENHANCE_CNTL1, 16, 1)
#define MOSI_I_DLYCTL         bits_desc(SPICC_REG_ENHANCE_CNTL1, 17, 3)
#define MISO_I_DLYCTL_EN      bits_desc(SPICC_REG_ENHANCE_CNTL1, 20, 1)
#define MISO_I_DLYCTL         bits_desc(SPICC_REG_ENHANCE_CNTL1, 21, 3)
#define MOSI_O_DLYCTL_EN      bits_desc(SPICC_REG_ENHANCE_CNTL1, 24, 1)
#define MOSI_O_DLYCTL         bits_desc(SPICC_REG_ENHANCE_CNTL1, 25, 3)
#define MOSI_OEN_DLYCTL_EN    bits_desc(SPICC_REG_ENHANCE_CNTL1, 28, 1)
#define MOSI_OEN_DLYCTL       bits_desc(SPICC_REG_ENHANCE_CNTL1, 29, 3)

struct spicc_platform_data {
	int device_id;
	struct spicc_regs __iomem *regs;
	struct pinctrl *pinctrl;
	struct clk *clk;
	int num_chipselect;
	int *cs_gpios;
};

#endif

